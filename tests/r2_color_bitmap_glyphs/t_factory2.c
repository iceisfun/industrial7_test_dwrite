/* IDWriteFactory2::TranslateColorGlyphRun is IDWriteFactory4's with the image formats fixed to TRUETYPE | CFF | COLR (forwards with formats 7),
 * so bitmap and SVG glyphs never take part, and its enumerator is the same object as the IDWriteColorGlyphRunEnumerator1 one (GetCurrentRun of the v0 interface,
 *, forwards to the v1 implementation which returns the address of one record inside the enumerator). */
#include "crb.h"

static int run_all(IDWriteColorGlyphRunEnumerator *e, UINT32 *fmts, UINT16 *first, int max) {
    int k = 0; BOOL has;
    while (k < max && SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) {
        const DWRITE_COLOR_GLYPH_RUN *c = NULL; IDWriteColorGlyphRunEnumerator1 *e1 = NULL; const DWRITE_COLOR_GLYPH_RUN1 *c1 = NULL;
        if (FAILED(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c)) || !c) break;
        if (SUCCEEDED(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1)) && e1) {
            IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e1, &c1); CHECKF((const void *)c1 == (const void *)c, "run %d: v0 record %p v1 record %p", k, (const void *)c, (const void *)c1);
            fmts[k] = c1 ? c1->glyphImageFormat : 0xDEAD; IDWriteColorGlyphRunEnumerator1_Release(e1);
        }
        first[k] = c->glyphRun.glyphIndices[0]; k++;
    }
    return k;
}
#define NEED_F2() IDWriteFactory2 *f2 = crb_f2(); if (!f2) { t_skip("IDWriteFactory2 unsupported"); return; } size_t fn_; uint8_t *fb_ = crb_translate_fixture(&fn_); CRB_NEED(x, fb_, fn_)

T(factory2_never_translates_bitmap_glyphs) {
    NEED_F2(); static const UINT16 g[] = { 3, 4, 5, 6, 7, 8 }; float adv[6] = { 1, 2, 3, 4, 5, 6 }; DWRITE_GLYPH_RUN r; int i;
    memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.glyphCount = 1; r.glyphAdvances = adv;
    for (i = 0; i < 6; i++) {
        IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; float em[3] = { 8, 20, 70 }; int k;
        r.glyphIndices = &g[i];
        for (k = 0; k < 3; k++) { e = (void *)(ULONG_PTR)1; r.fontEmSize = em[k]; CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f2, 0, 0, &r, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), E_NOCOLOR_); CHECK_OUT_NULL(e); }
    }
    crb_close(&x);
}
T(factory2_uses_colr_even_when_the_glyph_has_a_bitmap) {
    NEED_F2(); UINT16 g = 2; float adv = 12; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator *e = NULL; UINT32 fm[8]; UINT16 fg[8]; int k;
    memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 10; r.glyphCount = 1; r.glyphIndices = &g; r.glyphAdvances = &adv;
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 0, 0, &r, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    k = e ? run_all(e, fm, fg, 8) : 0;
    EXPECT_EQ(k, 2); if (k == 2) { EXPECT_EQ(fm[0], 0); EXPECT_EQ(fg[0], 1); EXPECT_EQ(fm[1], F_TT | F_COLR); EXPECT_EQ(fg[1], 64); }
    DW_RELEASE(e); crb_close(&x);
}
T(factory2_equals_factory4_with_formats_seven) {
    NEED_F2(); IDWriteFactory4 *f4 = crb_f4(); static const UINT16 g[] = { 3, 10, 2, 28, 14, 4 }; float adv[6] = { 5, 6, 7, 8, 9, 10 }; DWRITE_GLYPH_RUN r; int p;
    if (!f4) { crb_close(&x); t_skip("IDWriteFactory4 unsupported"); return; }
    memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 20; r.glyphCount = 6; r.glyphIndices = g; r.glyphAdvances = adv;
    for (p = 0; p < 4; p++) {
        IDWriteColorGlyphRunEnumerator *ea = NULL; IDWriteColorGlyphRunEnumerator1 *eb = NULL; UINT32 fa[16], fb[16]; UINT16 ga[16], gb[16]; int ka, kb; D2D1_POINT_2F o = { 3, 4 };
        HRESULT h1 = IDWriteFactory2_TranslateColorGlyphRun(f2, 3, 4, &r, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, (UINT32)p, &ea);
        HRESULT h2 = IDWriteFactory4_TranslateColorGlyphRun(f4, o, &r, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)7, DWRITE_MEASURING_MODE_NATURAL, NULL, (UINT32)p, &eb);
        CHECKF(h1 == h2, "palette %d: %s vs %s", p, dw_hr_name(h1), dw_hr_name(h2));
        if (h1 == S_OK && h2 == S_OK) {
            ka = run_all(ea, fa, ga, 16); kb = run_all((IDWriteColorGlyphRunEnumerator *)eb, fb, gb, 16);
            CHECKF(ka == kb, "palette %d: %d vs %d runs", p, ka, kb);
            if (ka == kb) { int i; for (i = 0; i < ka; i++) CHECKF(fa[i] == fb[i] && ga[i] == gb[i], "palette %d run %d differs", p, i); }
        }
        DW_RELEASE(ea); DW_RELEASE(eb);
    }
    crb_close(&x);
}
T(factory2_enumerator_is_the_enumerator1_object) {
    NEED_F2(); UINT16 g = 10; float adv = 12; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator *e = NULL; IDWriteColorGlyphRunEnumerator1 *e1 = NULL;
    memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 10; r.glyphCount = 1; r.glyphIndices = &g; r.glyphAdvances = &adv;
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 0, 0, &r, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        ULONG base = dw_refcount((IUnknown *)e);
        CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1));
        EXPECT_EQ((void *)e, (void *)e1); EXPECT_EQ(dw_refcount((IUnknown *)e), base + 1);
        DW_RELEASE(e1);
    }
    DW_RELEASE(e); crb_close(&x);
}
T(factory2_palette_index_checked_against_cpal) {
    NEED_F2(); UINT16 g = 10; float adv = 12; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator *e; UINT32 p;
    memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 10; r.glyphCount = 1; r.glyphIndices = &g; r.glyphAdvances = &adv;
    for (p = 0; p < 6; p++) {
        e = (void *)(ULONG_PTR)1;
        CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f2, 0, 0, &r, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, p, &e), p < 2 ? S_OK : E_NOCOLOR_);
        if (p >= 2) CHECK_OUT_NULL(e);
        DW_RELEASE(e);
    }
    crb_close(&x);
}
