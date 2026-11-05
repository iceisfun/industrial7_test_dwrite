/* IDWriteFactory2::TranslateColorGlyphRun on synthetic COLR/CPAL fonts (dwrite.dll!DWriteFactory::TranslateColorGlyphRun 0x25240 forwards to
 * the shared worker with formats = TRUETYPE|CFF|COLR; the worker (0x1cf40) zeroes *layers first, fails E_INVALIDARG for a NULL run,
 * and DWRITE_E_NOCOLOR (0x8898500C) when DWriteColorGlyphRunEnumerator::Create finds no colour glyph: no usable format left after the face's
 * formats and the palette index check, or no glyph of the run with colour data).
 * All expectations come from the oracle in cg.c (see cg.h for the rules it encodes). */
#include "cg.h"

#define FACE_FMT_COLR (CGF_TT | CGF_COLR)
static const WCHAR *const k_nothing = NULL;

static void run_pattern(cgfont *x, const char *tag, const UINT16 *g, int n, float em, int adv, int off, BOOL sideways, UINT32 bidi, DWRITE_MEASURING_MODE mm, UINT32 pal) {
    IDWriteFactory2 *f = cg_f2(); static cgrun r;
    cg_mkrun(&r, x->face, em, g, n, adv, off, sideways, bidi);
    cg_check_f2(f, tag, &r.run, 10.5f, -3.25f, mm, NULL, pal, FACE_FMT_COLR);
}

TEST(f2_factory_exposes_interface) {
    CG_NEED_F2(f);
    IDWriteFactory *base = dw_shared(); IDWriteFactory2 *q = NULL;
    CHECK_HR_OK(IDWriteFactory_QueryInterface(base, &IID_IDWriteFactory2, (void **)&q));
    CHECK(q != NULL);
    if (q) { CHECK(dw_same_identity((IUnknown *)q, (IUnknown *)f)); IDWriteFactory2_Release(q); }
}
TEST(f2_plain_font_has_no_color) {
    CG_NEED_F2(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g = (UINT16)ttf_default_gid('A'); cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR);
    CHECK_OUT_NULL(e);
    cg_close(&x);
}
TEST(f2_plain_font_every_glyph_nocolor) {
    CG_NEED_F2(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    int i; for (i = 0; i < TTF_DEF_NGLYPHS; i++) {
        UINT16 g = (UINT16)i; cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1;
        cg_mkrun(&r, x.face, 16.0f, &g, 1, 1, 0, FALSE, 0);
        HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f, 1, 2, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        CHECKF(hr == CG_E_NOCOLOR && e == NULL, "glyph %d: %s %p", i, dw_hr_name(hr), (void *)e);
    }
    cg_close(&x);
}
TEST(f2_plain_font_palette_index_irrelevant) {
    CG_NEED_F2(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    static const UINT32 pal[] = { 0, 1, 5, 0xFFFF, 0xFFFFFFFFu }; int i;
    UINT16 g = 2; cgrun r; cg_mkrun(&r, x.face, 12.0f, &g, 1, 0, 0, FALSE, 0);
    for (i = 0; i < 5; i++) { IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, pal[i], &e); CHECKF(hr == CG_E_NOCOLOR && !e, "palette %u: %s", pal[i], dw_hr_name(hr)); }
    cg_close(&x);
}
TEST(f2_null_run_is_invalid_arg) {
    CG_NEED_F2(f); int mm, pal;
    if (t_is_wine()) { t_skip("Wine dereferences the NULL run"); return; }
    for (mm = 0; mm < 3; mm++) for (pal = 0; pal < 3; pal++) {
        IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, NULL, NULL, (DWRITE_MEASURING_MODE)mm, NULL, pal, &e);
        CHECKF(hr == CG_E_INVALIDARG && e == NULL, "mm %d pal %d: %s %p", mm, pal, dw_hr_name(hr), (void *)e);
    }
}
TEST(f2_null_run_with_transform_and_description) {
    CG_NEED_F2(f); DWRITE_MATRIX m = { 1, 0, 0, 1, 0, 0 }; DWRITE_GLYPH_RUN_DESCRIPTION d;
    if (t_is_wine()) { t_skip("Wine dereferences the NULL run"); return; } IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1;
    memset(&d, 0, sizeof d);
    CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 3, 4, NULL, &d, DWRITE_MEASURING_MODE_GDI_CLASSIC, &m, 0, &e), CG_E_INVALIDARG);
    CHECK_OUT_NULL(e);
}
TEST(f2_color_glyph_succeeds) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    UINT16 g = GID_A; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    CHECK(e != NULL);
    DW_RELEASE(e);
    cg_close(&x);
}
TEST(f2_color_font_every_glyph_palette0) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int i;
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) { char tag[40]; UINT16 g = (UINT16)i; snprintf(tag, sizeof tag, "glyph %d", i); run_pattern(&x, tag, &g, 1, 20.0f, 0, 0, FALSE, 0, DWRITE_MEASURING_MODE_NATURAL, 0); }
    cg_close(&x);
}
TEST(f2_color_font_every_glyph_palette1) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int i;
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) { char tag[40]; UINT16 g = (UINT16)i; snprintf(tag, sizeof tag, "glyph %d pal1", i); run_pattern(&x, tag, &g, 1, 20.0f, 1, 0, FALSE, 0, DWRITE_MEASURING_MODE_NATURAL, 1); }
    cg_close(&x);
}
TEST(f2_color_font_noncolor_glyph_nocolor) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    static const UINT16 g[] = { 0, 1, GID_F, GID_G, GID_b, 53, 64, 65, 66, 67, 68 }; int i;
    for (i = 0; i < (int)(sizeof g / sizeof g[0]); i++) { cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; cg_mkrun(&r, x.face, 20.0f, &g[i], 1, 0, 0, FALSE, 0);
        HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e); CHECKF(hr == CG_E_NOCOLOR && !e, "glyph %u: %s", g[i], dw_hr_name(hr)); }
    cg_close(&x);
}
TEST(f2_color_font_runs_of_only_noncolor_glyphs) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    UINT16 g[] = { GID_F, GID_G, GID_H, 1, 0, GID_b }; cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1;
    cg_mkrun(&r, x.face, 20.0f, g, 6, 1, 0, FALSE, 0);
    CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR);
    CHECK_OUT_NULL(e);
    cg_close(&x);
}
TEST(f2_glyph_count_zero) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0); r.run.glyphCount = 0;
    /* no glyph => no colour glyph found => DWRITE_E_NOCOLOR (FindColorGlyphs returns 0) */
    CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR);
    CHECK_OUT_NULL(e);
    r.run.glyphIndices = NULL; e = (void *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR);
    CHECK_OUT_NULL(e);
    cg_close(&x);
}
TEST(f2_out_pointer_zeroed_before_anything) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_F; cgrun r;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)0xdeadbeef;
    CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR);
    CHECK_OUT_NULL(e);
    e = (void *)(ULONG_PTR)0xdeadbeef;
    if (!t_is_wine()) { CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, NULL, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_INVALIDARG);
    CHECK_OUT_NULL(e); }
    cg_close(&x);
}
TEST(f2_palette_index_sweep) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    static const UINT32 pal[] = { 0, 1, 2, 3, 7, 0xFF, 0x100, 0xFFFE, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu }; int i;
    UINT16 g[] = { GID_A, GID_F, GID_B }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 1, 0, FALSE, 0);
    for (i = 0; i < (int)(sizeof pal / sizeof pal[0]); i++) {
        char tag[48]; snprintf(tag, sizeof tag, "palette 0x%x", pal[i]);
        HRESULT hr = cg_check_f2(f, tag, &r.run, 0, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, pal[i], FACE_FMT_COLR);
        /* an unknown palette index is NOCOLOR (the COLR bit is dropped, nothing else is requested): never E_INVALIDARG */
        if (pal[i] >= 2) CHECK_HR(hr, CG_E_NOCOLOR); else CHECK_HR(hr, S_OK);
    }
    cg_close(&x);
}
TEST(f2_palette_index_matches_face_palette_count) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    UINT32 np = IDWriteFontFace2_GetColorPaletteCount(x.f2); EXPECT_EQ(np, 2);
    UINT16 g = GID_B; cgrun r; cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    IDWriteColorGlyphRunEnumerator *e = NULL;
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, np - 1, &e)); DW_RELEASE(e);
    e = (void *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, np, &e), CG_E_NOCOLOR); CHECK_OUT_NULL(e);
    cg_close(&x);
}
TEST(f2_palette_selects_colors) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int p;
    UINT16 g = GID_B;   /* single layer {ring, palette entry 2} */
    for (p = 0; p < 2; p++) {
        cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; BOOL has = 0; const DWRITE_COLOR_GLYPH_RUN *c = NULL;
        cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
        CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, p, &e));
        if (!e) continue;
        CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)); CHECK(has);          /* base run */
        CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)); CHECK(has);
        CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c));
        if (c) {
            uint32_t v = cg_pal[p][2];
            CHECK_NEAR(c->runColor.r, ((v >> 16) & 255) / 255.0f, 1e-5); CHECK_NEAR(c->runColor.g, ((v >> 8) & 255) / 255.0f, 1e-5);
            CHECK_NEAR(c->runColor.b, (v & 255) / 255.0f, 1e-5); CHECK_NEAR(c->runColor.a, (v >> 24) / 255.0f, 1e-5);
            EXPECT_EQ(c->paletteIndex, 2); EXPECT_EQ(c->glyphRun.glyphIndices[0], GID_RING);
        }
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(f2_description_is_ignored) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    UINT16 g[] = { GID_A, GID_F }; WCHAR txt[] = L"AF"; UINT16 cm[] = { 0, 1 }; DWRITE_GLYPH_RUN_DESCRIPTION d; cgrun r; static cg_exp ex[CG_MAXEXP];
    IDWriteColorGlyphRunEnumerator *e = NULL;
    memset(&d, 0, sizeof d); d.localeName = L"en-us"; d.string = txt; d.stringLength = 2; d.clusterMap = cm; d.textPosition = 0;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0);
    int n = cg_expect(&r.run, 5, 6, 7, FACE_FMT_COLR, 0, ex);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 5, 6, &r.run, &d, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        /* the enumerator never reports the description pointer back */
        BOOL has; int k = 0;
        while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) {
            const DWRITE_COLOR_GLYPH_RUN *c = NULL; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c));
            if (c && c->glyphRunDescription) t_note("run %d reports a description pointer %p", k, (void *)c->glyphRunDescription);
            CHECK(c == NULL || c->glyphRunDescription == NULL || c->glyphRunDescription == &d);
            k++;
        }
        EXPECT_EQ(k, n);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(f2_measuring_modes_provided_advances) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int mm;
    UINT16 g[] = { GID_A, GID_F, GID_B, GID_C };
    for (mm = 0; mm < 3; mm++) { char tag[32]; snprintf(tag, sizeof tag, "mm %d", mm); run_pattern(&x, tag, g, 4, 18.0f, 2, 0, FALSE, 0, (DWRITE_MEASURING_MODE)mm, 0); }
    cg_close(&x);
}
TEST(f2_measuring_modes_computed_advances_natural) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    UINT16 g[] = { GID_A, GID_F, GID_B, GID_C, 0, GID_a };
    run_pattern(&x, "natural", g, 6, 2048.0f, 0, 0, FALSE, 0, DWRITE_MEASURING_MODE_NATURAL, 0);
    run_pattern(&x, "natural 24", g, 6, 24.0f, 0, 0, FALSE, 0, DWRITE_MEASURING_MODE_NATURAL, 1);
    cg_close(&x);
}
TEST(f2_measuring_modes_computed_advances_gdi) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int mm;
    UINT16 g[] = { GID_F, GID_A, GID_F }; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL;
    for (mm = 1; mm <= 2; mm++) {
        cg_mkrun(&r, x.face, 10.0f, g, 3, 0, 0, FALSE, 0);
        CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, (DWRITE_MEASURING_MODE)mm, NULL, 0, &e));
        if (!e) continue;
        BOOL has; const DWRITE_COLOR_GLYPH_RUN *c = NULL; int k = 0; float adv0 = -1;
        while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) {
            CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c));
            if (k == 0 && c) { adv0 = c->glyphRun.glyphAdvances[0]; EXPECT_EQ(c->glyphRun.glyphCount, 3); }
            k++;
        }
        /* GDI compatible advances are whole pixels at 10 ppem: 1000/2048*10 = 4.88 -> 5 */
        t_note("mm %d: base advance[0] = %g (natural would be %g)", mm, adv0, 1000 * 10.0f / 2048);
        CHECK(adv0 == floorf(adv0));
        EXPECT_EQ(k, 3);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(f2_transform_variants) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    static const DWRITE_MATRIX m[] = { { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 2, 0, 0 }, { 0, 1, -1, 0, 5, 6 }, { 1, 0, 0, 1, 100, -50 }, { 0.5f, 0, 0, 0.5f, 0, 0 }, { 1, 0.5f, 0, 1, 0, 0 }, { 0, 0, 0, 0, 0, 0 } };
    UINT16 g[] = { GID_A, GID_F, GID_B }; static cgrun r; int i; cg_mkrun(&r, x.face, 20.0f, g, 3, 1, 0, FALSE, 0);
    for (i = 0; i < (int)(sizeof m / sizeof m[0]); i++) { char tag[32]; snprintf(tag, sizeof tag, "matrix %d", i); cg_check_f2(f, tag, &r.run, 7, 8, DWRITE_MEASURING_MODE_NATURAL, &m[i], 0, FACE_FMT_COLR); }
    cg_close(&x);
}
TEST(f2_transform_with_gdi_modes) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int mm;
    DWRITE_MATRIX m = { 1.5f, 0, 0, 1.5f, 0, 0 }; UINT16 g[] = { GID_A, GID_F }; static cgrun r;
    for (mm = 0; mm < 3; mm++) { cg_mkrun(&r, x.face, 20.0f, g, 2, 1, 0, FALSE, 0); char tag[32]; snprintf(tag, sizeof tag, "mm %d scaled", mm); cg_check_f2(f, tag, &r.run, 1, 1, (DWRITE_MEASURING_MODE)mm, &m, 0, FACE_FMT_COLR); }
    cg_close(&x);
}
TEST(f2_baseline_origin_sweep) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    static const float v[][2] = { { 0, 0 }, { 100.5f, -20.25f }, { -1000, 1000 }, { 1e-3f, -1e-3f }, { 123456.0f, 654321.0f }, { -0.0f, 0.0f }, { 1e9f, -1e9f }, { 0.5f, 0.5f } }; int i;
    UINT16 g[] = { GID_F, GID_A, GID_C, GID_F, GID_a }; static cgrun r; cg_mkrun(&r, x.face, 32.0f, g, 5, 1, 0, FALSE, 0);
    for (i = 0; i < 8; i++) { char tag[32]; snprintf(tag, sizeof tag, "origin %d", i); cg_check_f2(cg_f2(), tag, &r.run, v[i][0], v[i][1], DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR); }
    cg_close(&x);
}
TEST(f2_font_size_sweep_computed_advances) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    static const float em[] = { 1, 2.5f, 8, 11.25f, 12, 16, 20, 48, 72, 100, 256, 1000, 2048, 4096 }; int i;
    UINT16 g[] = { GID_A, GID_F, GID_B, 0, GID_C }; static cgrun r;
    for (i = 0; i < (int)(sizeof em / sizeof em[0]); i++) { char tag[32]; cg_mkrun(&r, x.face, em[i], g, 5, 0, 0, FALSE, 0); snprintf(tag, sizeof tag, "em %g", em[i]); cg_check_f2(f, tag, &r.run, 2, 3, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR); }
    cg_close(&x);
}
TEST(f2_font_size_zero_and_negative) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); static const float em[] = { 0.0f, -12.0f }; int i;
    for (i = 0; i < 2; i++) {
        UINT16 g[] = { GID_A, GID_F }; cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; cg_mkrun(&r, x.face, em[i], g, 2, 1, 0, FALSE, 0);
        HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        t_note("em %g: %s", em[i], dw_hr_name(hr));
        CHECK(hr == S_OK ? e != NULL : e == NULL);
        if (e) { BOOL has; int k = 0; while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has && k < 20) k++; t_note("em %g: %d runs", em[i], k); DW_RELEASE(e); }
    }
    cg_close(&x);
}
TEST(f2_run_length_sweep) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int n;
    UINT16 g[64]; static cgrun r;
    for (n = 1; n <= 64; n++) {
        int i; char tag[32]; for (i = 0; i < n; i++) g[i] = (UINT16)((i % 3 == 1) ? GID_F : (GID_0 + i % 10));
        cg_mkrun(&r, x.face, 20.0f, g, n, 1, 0, FALSE, 0); snprintf(tag, sizeof tag, "n=%d", n);
        cg_check_f2(f, tag, &r.run, 0, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    }
    cg_close(&x);
}
TEST(f2_more_than_eight_color_glyphs) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int n;
    /* the enumerator keeps the first 8 colour glyph entries inline and looks up the rest in a second pass */
    for (n = 6; n <= 20; n++) {
        UINT16 g[64]; static cgrun r; char tag[32]; int i;
        for (i = 0; i < n; i++) g[i] = (UINT16)(i % 2 ? GID_A : GID_0 + (i / 2) % 10);
        cg_mkrun(&r, x.face, 16.0f, g, n, 2, 0, FALSE, 0); snprintf(tag, sizeof tag, "%d colour glyphs", n);
        cg_check_f2(f, tag, &r.run, 3, 4, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    }
    cg_close(&x);
}
TEST(f2_more_than_eight_color_glyphs_mixed_with_plain) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int n;
    for (n = 8; n <= 30; n += 2) {
        UINT16 g[64]; static cgrun r; char tag[40]; int i;
        for (i = 0; i < n; i++) g[i] = (UINT16)(i % 3 == 0 ? GID_F : i % 3 == 1 ? GID_B : GID_a);
        cg_mkrun(&r, x.face, 16.0f, g, n, 1, 0, TRUE ? FALSE : FALSE, 0); snprintf(tag, sizeof tag, "mixed n=%d", n);
        cg_check_f2(f, tag, &r.run, 0, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, 1, FACE_FMT_COLR);
    }
    cg_close(&x);
}
TEST(f2_glyph_pattern_table) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    static const struct { UINT16 g[6]; int n; } p[] = {
        { { GID_A }, 1 }, { { GID_A, GID_A }, 2 }, { { GID_A, GID_F }, 2 }, { { GID_F, GID_A }, 2 }, { { GID_F, GID_A, GID_F }, 3 },
        { { GID_A, GID_F, GID_A }, 3 }, { { GID_A, GID_B, GID_C }, 3 }, { { GID_C, GID_C, GID_C }, 3 }, { { GID_D, GID_E }, 2 },
        { { GID_E, GID_E, GID_E }, 3 }, { { GID_D, GID_F, GID_E, GID_F }, 4 }, { { GID_a, GID_A, GID_a }, 3 }, { { 0, GID_A, 0 }, 3 },
        { { 1, GID_B, 1, GID_C }, 4 }, { { GID_F, GID_F, GID_F, GID_F, GID_A }, 5 }, { { GID_a, GID_0, GID_0 + 1, GID_0 + 4, GID_E }, 5 },
        { { GID_TRI, GID_A, GID_ROUND }, 3 }, { { GID_RING, GID_B }, 2 }, { { GID_E }, 1 }, { { GID_D }, 1 }, { { GID_C }, 1 }, { { GID_a }, 1 },
    };
    int i, pal;
    for (pal = 0; pal < 2; pal++) for (i = 0; i < (int)(sizeof p / sizeof p[0]); i++) {
        char tag[48]; static cgrun r; snprintf(tag, sizeof tag, "pattern %d pal %d", i, pal);
        cg_mkrun(&r, x.face, 32.0f, p[i].g, p[i].n, 0, 0, FALSE, 0);
        cg_check_f2(f, tag, &r.run, 11, 22, DWRITE_MEASURING_MODE_NATURAL, NULL, pal, FACE_FMT_COLR);
        cg_mkrun(&r, x.face, 32.0f, p[i].g, p[i].n, 2, 0, FALSE, 0); snprintf(tag, sizeof tag, "pattern %d pal %d adv", i, pal);
        cg_check_f2(f, tag, &r.run, 11, 22, DWRITE_MEASURING_MODE_NATURAL, NULL, pal, FACE_FMT_COLR);
    }
    cg_close(&x);
}
TEST(f2_right_to_left_levels) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); static const UINT32 lv[] = { 0, 1, 2, 3, 4, 5, 124, 125, 126, 255 }; int i;
    UINT16 g[] = { GID_A, GID_F, GID_B, GID_C };
    for (i = 0; i < (int)(sizeof lv / sizeof lv[0]); i++) { char tag[32]; static cgrun r; snprintf(tag, sizeof tag, "bidi %u", lv[i]); cg_mkrun(&r, x.face, 20.0f, g, 4, 2, 0, FALSE, lv[i]); cg_check_f2(f, tag, &r.run, 200, 10, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR); }
    cg_close(&x);
}
TEST(f2_right_to_left_computed_advances) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { 0, GID_A, GID_F, GID_a, GID_B };
    static cgrun r; cg_mkrun(&r, x.face, 2048.0f, g, 5, 0, 0, FALSE, 1);
    cg_check_f2(f, "rtl computed", &r.run, 5000, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    cg_close(&x);
}
TEST(f2_rtl_layer_origin_is_left_of_run_origin) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_F, GID_F, GID_B }; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL;
    cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, FALSE, 1);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 100, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        BOOL has; const DWRITE_COLOR_GLYPH_RUN *c = NULL; IDWriteColorGlyphRunEnumerator_MoveNext(e, &has); IDWriteColorGlyphRunEnumerator_MoveNext(e, &has);
        CHECK(has); IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c);
        if (c) CHECK_NEAR(c->baselineOriginX, 100.0f - (r.adv[0] + r.adv[1]), 1e-3);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(f2_sideways_provided_advances) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F, GID_B };
    static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, TRUE, 0);
    cg_check_f2(f, "sideways", &r.run, 30, 40, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, TRUE, 1);
    cg_check_f2(f, "sideways rtl", &r.run, 30, 40, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    cg_close(&x);
}
TEST(f2_sideways_computed_advances_structure) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F, GID_B };
    static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 0, 0, TRUE, 0);
    /* positions depend on the vertical advances the font reports: the oracle leaves them unchecked (NaN), the structure is checked */
    cg_check_f2(f, "sideways computed", &r.run, 30, 40, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    cg_close(&x);
}
TEST(f2_glyph_offsets_pass_through) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_F, GID_A, GID_B, GID_C }; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL;
    cg_mkrun(&r, x.face, 20.0f, g, 4, 2, 1, FALSE, 0);
    cg_check_f2(f, "offsets", &r.run, 0, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        BOOL has; int k = 0;
        while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) {
            const DWRITE_COLOR_GLYPH_RUN *c = NULL; UINT32 j; IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c);
            if (c && c->glyphRun.glyphOffsets) {
                if (k == 0) { for (j = 0; j < 4; j++) CHECKF(c->glyphRun.glyphOffsets[j].advanceOffset == r.off[j].advanceOffset && c->glyphRun.glyphOffsets[j].ascenderOffset == r.off[j].ascenderOffset, "base offset %u", j); }
                else {
                    /* layer runs carry the offset of the colour glyph they belong to (glyph 1 for A, 2 for B, 3 for C) */
                    int src = k <= 2 ? 1 : k <= 3 ? 2 : 3;
                    CHECKF(c->glyphRun.glyphOffsets[0].advanceOffset == r.off[src].advanceOffset && c->glyphRun.glyphOffsets[0].ascenderOffset == r.off[src].ascenderOffset, "layer %d offset (src glyph %d): %g %g", k, src, c->glyphRun.glyphOffsets[0].advanceOffset, c->glyphRun.glyphOffsets[0].ascenderOffset);
                }
            } else CHECKF(0, "run %d has no offsets", k);
            k++;
        }
        EXPECT_EQ(k, 1 + 2 + 1 + 3);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(f2_offsets_do_not_move_layer_origins) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_B }; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 2, 1, FALSE, 0);
    /* the oracle (no offsets applied to x/y) is the expectation for a run with offsets as well */
    cg_check_f2(f, "with offsets", &r.run, 50, 60, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    cg_close(&x);
}
TEST(f2_enumerator_copies_the_run_data) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 gsrc[] = { GID_A, GID_F, GID_B };
    UINT16 *gi = (UINT16 *)malloc(sizeof gsrc); float *ad = (float *)malloc(3 * sizeof(float)); DWRITE_GLYPH_OFFSET *of = (DWRITE_GLYPH_OFFSET *)malloc(3 * sizeof *of);
    DWRITE_GLYPH_RUN run; IDWriteColorGlyphRunEnumerator *e = NULL; static cg_exp ex[CG_MAXEXP]; int n;
    memcpy(gi, gsrc, sizeof gsrc); ad[0] = 11; ad[1] = 22; ad[2] = 33; of[0].advanceOffset = 1; of[0].ascenderOffset = 2; of[1] = of[0]; of[2] = of[0];
    memset(&run, 0, sizeof run); run.fontFace = x.face; run.fontEmSize = 20; run.glyphCount = 3; run.glyphIndices = gi; run.glyphAdvances = ad; run.glyphOffsets = of;
    n = cg_expect(&run, 4, 5, 7, FACE_FMT_COLR, 0, ex);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 4, 5, &run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    /* scribble over and free the caller's arrays: the enumerator owns copies */
    memset(gi, 0xEE, sizeof gsrc); memset(ad, 0xEE, 3 * sizeof(float)); memset(of, 0xEE, 3 * sizeof *of);
    free(gi); free(ad); free(of);
    if (e) { run.glyphIndices = (const UINT16 *)gsrc; run.glyphAdvances = NULL; cg_walk(e, "after free", &run, ex, n, DWRITE_MEASURING_MODE_NATURAL); DW_RELEASE(e); }
    cg_close(&x);
}
TEST(f2_run_struct_may_be_discarded) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_C }; DWRITE_GLYPH_RUN *run = (DWRITE_GLYPH_RUN *)calloc(1, sizeof *run);
    IDWriteColorGlyphRunEnumerator *e = NULL; run->fontFace = x.face; run->fontEmSize = 14; run->glyphCount = 2; run->glyphIndices = g;
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    memset(run, 0xCD, sizeof *run); free(run);
    if (e) { BOOL has; int k = 0; while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) { const DWRITE_COLOR_GLYPH_RUN *c = NULL; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c)); CHECK(c && c->glyphRun.fontEmSize == 14.0f && c->glyphRun.fontFace == x.face); k++; } EXPECT_EQ(k, 1 + 2 + 3); DW_RELEASE(e); }
    cg_close(&x);
}
TEST(f2_enumerator_holds_the_face) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL, *e2 = NULL;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    ULONG before = dw_refcount((IUnknown *)x.face);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    ULONG held = dw_refcount((IUnknown *)x.face); t_note("face refs %lu -> %lu while one enumerator is alive", before, held);
    CHECK(held > before);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e2));
    CHECK(dw_refcount((IUnknown *)x.face) >= held);
    DW_RELEASE(e); DW_RELEASE(e2);
    EXPECT_REFS(x.face, before);
    cg_close(&x);
}
TEST(f2_failed_translate_leaves_face_refcount) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_F; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; int i;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    ULONG before = dw_refcount((IUnknown *)x.face);
    for (i = 0; i < 50; i++) IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
    EXPECT_REFS(x.face, before);
    cg_close(&x);
}
TEST(f2_repeated_translate_is_stable) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F, GID_a }; static cgrun r; static cg_exp ex[CG_MAXEXP]; int i, n;
    cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, FALSE, 0); n = cg_expect(&r.run, 1, 2, 7, FACE_FMT_COLR, 0, ex);
    ULONG before = dw_refcount((IUnknown *)x.face);
    for (i = 0; i < 300; i++) {
        IDWriteColorGlyphRunEnumerator *e = NULL;
        if (FAILED(IDWriteFactory2_TranslateColorGlyphRun(f, 1, 2, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e)) || !e) { CHECKF(0, "iteration %d failed", i); break; }
        if (i % 50 == 0) cg_walk(e, "loop", &r.run, ex, n, DWRITE_MEASURING_MODE_NATURAL);
        IDWriteColorGlyphRunEnumerator_Release(e);
    }
    EXPECT_REFS(x.face, before);
    cg_close(&x);
}
TEST(f2_two_enumerators_are_independent) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_B }; cgrun r; IDWriteColorGlyphRunEnumerator *a = NULL, *b = NULL; BOOL ha, hb;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &a));
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 1, &b));
    if (a && b) {
        const DWRITE_COLOR_GLYPH_RUN *ca = NULL, *cb = NULL; int i;
        CHECK(a != b);
        for (i = 0; i < 2; i++) { IDWriteColorGlyphRunEnumerator_MoveNext(a, &ha); }
        IDWriteColorGlyphRunEnumerator_MoveNext(b, &hb); CHECK(ha && hb);
        IDWriteColorGlyphRunEnumerator_GetCurrentRun(a, &ca); IDWriteColorGlyphRunEnumerator_GetCurrentRun(b, &cb);
        CHECK(ca != cb);   /* each enumerator owns its current-run record */
        if (ca && cb) { CHECK(ca->glyphRun.glyphIndices != cb->glyphRun.glyphIndices); CHECK(cb->glyphRun.glyphCount == 2 && ca->glyphRun.glyphCount == 1); }
    }
    DW_RELEASE(a); DW_RELEASE(b);
    cg_close(&x);
}
TEST(f2_enumerator_outlives_font_handles) {
    CG_NEED_F2(f); cgfont x; if (!cg_open_fx(&x, CGFX_COLR)) { cg_close(&x); return; }
    UINT16 g[] = { GID_A, GID_F }; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; static cg_exp ex[CG_MAXEXP]; int n;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 2, 0, FALSE, 0); n = cg_expect(&r.run, 1, 2, 7, FACE_FMT_COLR, 0, ex);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 1, 2, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    DWRITE_GLYPH_RUN keep = r.run; IDWriteFontFace_AddRef(x.face);        /* the walker compares fontFace pointers: keep the identity alive */
    cg_close(&x);                                                          /* loader unregistered, file and caller references gone */
    if (e) { cg_walk(e, "after close", &keep, ex, n, DWRITE_MEASURING_MODE_NATURAL); DW_RELEASE(e); }
    IDWriteFontFace_Release(keep.fontFace);
}
TEST(f2_isolated_factory_same_results) {
    IDWriteFactory2 *f = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_ISOLATED, 2); if (!f) { t_skip("no isolated IDWriteFactory2"); return; }
    cgfont x; if (!cg_open_fx(&x, CGFX_COLR)) { cg_close(&x); return; }
    UINT16 g[] = { GID_A, GID_F, GID_C }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, FALSE, 0);
    cg_check_f2(f, "isolated factory", &r.run, 7, 9, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    cg_close(&x);
}
TEST(f2_via_factory3_and_factory4_pointers) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFactory4 *f4 = cg_f4();
    if (!f3) { t_skip("no IDWriteFactory3"); return; }
    cgfont x; if (!cg_open_fx(&x, CGFX_COLR)) { cg_close(&x); return; }
    UINT16 g[] = { GID_B, GID_F }; static cgrun r; static cg_exp ex[CG_MAXEXP]; int n; IDWriteColorGlyphRunEnumerator *e = NULL;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0); n = cg_expect(&r.run, 1, 2, 7, FACE_FMT_COLR, 0, ex);
    CHECK_HR_OK(IDWriteFactory3_TranslateColorGlyphRun(f3, 1, 2, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) { cg_walk(e, "factory3 pointer", &r.run, ex, n, DWRITE_MEASURING_MODE_NATURAL); DW_RELEASE(e); }
    if (f4) {
        /* IDWriteFactory4 inherits the IDWriteFactory2 slot unchanged */
        CHECK_HR_OK(f4->lpVtbl->TranslateColorGlyphRun((IDWriteFactory2 *)f4, 1, 2, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
        if (e) { cg_walk(e, "factory4 pointer", &r.run, ex, n, DWRITE_MEASURING_MODE_NATURAL); DW_RELEASE(e); }
    }
    cg_close(&x);
}
TEST(f2_new_factory_instances) {
    IDWriteFactory *nf = dw_new_factory(DWRITE_FACTORY_TYPE_SHARED); IDWriteFactory2 *f = NULL;
    if (!nf) { t_skip("cannot create a factory"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(nf, &IID_IDWriteFactory2, (void **)&f))) { IDWriteFactory_Release(nf); t_skip("no IDWriteFactory2"); return; }
    dwx_font h; static cgrun r; UINT16 g[] = { GID_C, GID_F };
    size_t n = 0; uint8_t *b = cg_build_fx(CGFX_COLR, &n);
    if (SUCCEEDED(dwx_font_open(&h, nf, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE)) && h.face) {
        cg_mkrun(&r, h.face, 20.0f, g, 2, 2, 0, FALSE, 0);
        cg_check_f2(f, "private factory", &r.run, 0, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    } else t_skip("cannot open font in a private factory");
    dwx_font_close(&h); free(b);
    IDWriteFactory2_Release(f); IDWriteFactory_Release(nf);
}
TEST(f2_color_glyph_with_each_layer_glyph_differs_from_base) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); int gid;
    for (gid = 0; gid < TTF_DEF_NGLYPHS; gid++) {
        const ttf_colr_layer *L; int nl = cg_layers_of(gid, &L), j;
        if (!nl) continue;
        for (j = 0; j < nl; j++) CHECKF(L[j].gid != gid, "fixture sanity: glyph %d layer %d is the base glyph", gid, j);
    }
    cg_close(&x);
}
TEST(f2_system_fonts_without_color) {
    CG_NEED_F2(f); static const WCHAR *const fam[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Consolas" }; int i;
    for (i = 0; i < 6; i++) {
        cgfont x; if (!cg_open_sys(&x, fam[i])) { cg_close(&x); continue; }
        UINT32 cp[] = { 'A', 'g', '0', ' ' }; UINT16 gl[4]; cgrun r; int k;
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.face, cp, 4, gl));
        for (k = 0; k < 4; k++) {
            IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; HRESULT hr;
            cg_mkrun(&r, x.face, 20.0f, &gl[k], 1, 0, 0, FALSE, 0);
            hr = IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
            CHECKF(hr == CG_E_NOCOLOR && !e, "%ls U+%04X: %s", fam[i], cp[k], dw_hr_name(hr));
        }
        cg_close(&x);
    }
}

static void big_run(cgfont *x, const char *tag, int n, int adv, int bidi, UINT32 pal, int seed) {
    DWRITE_GLYPH_RUN run; UINT16 *g = (UINT16 *)malloc(sizeof(UINT16) * n); float *a = (float *)malloc(sizeof(float) * n); int i;
    for (i = 0; i < n; i++) { unsigned v = (unsigned)(i * 2654435761u + seed) >> 7; g[i] = (UINT16)(v % 69); a[i] = (float)(5 + (v % 13)); }
    memset(&run, 0, sizeof run); run.fontFace = x->face; run.fontEmSize = 16.0f; run.glyphCount = (UINT32)n; run.glyphIndices = g; run.glyphAdvances = adv ? a : NULL; run.bidiLevel = bidi;
    cg_check_f2(cg_f2(), tag, &run, 7, 9, DWRITE_MEASURING_MODE_NATURAL, NULL, pal, FACE_FMT_COLR);
    free(g); free(a);
}
TEST(f2_long_runs_pseudo_random_glyphs) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); static const int len[] = { 100, 255, 256, 257, 300, 512, 1000, 2048 }; int i, pal;
    for (pal = 0; pal < 2; pal++) for (i = 0; i < 8; i++) { char tag[40]; snprintf(tag, sizeof tag, "n=%d pal %d", len[i], pal); big_run(&x, tag, len[i], 1, 0, pal, i); big_run(&x, tag, len[i], 0, 1, pal, i + 9); }
    cg_close(&x);
}
TEST(f2_long_run_of_one_colour_glyph) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); DWRITE_GLYPH_RUN run; int n = 1500; UINT16 *g = (UINT16 *)malloc(sizeof(UINT16) * n); int i;
    for (i = 0; i < n; i++) g[i] = GID_A;
    memset(&run, 0, sizeof run); run.fontFace = x.face; run.fontEmSize = 2.0f; run.glyphCount = n; run.glyphIndices = g;
    cg_check_f2(f, "1500 x A", &run, 0, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, FACE_FMT_COLR);
    free(g); cg_close(&x);
}
TEST(f2_private_factory_refcount_is_balanced) {
    IDWriteFactory *nf = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory2 *f = NULL; if (!nf) { t_skip("cannot create a factory"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(nf, &IID_IDWriteFactory2, (void **)&f))) { IDWriteFactory_Release(nf); t_skip("no IDWriteFactory2"); return; }
    dwx_font h; size_t n = 0; uint8_t *b = cg_build_fx(CGFX_COLR, &n);
    if (SUCCEEDED(dwx_font_open(&h, nf, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE)) && h.face) {
        UINT16 g[] = { GID_A, GID_B }; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; ULONG before, held, after; int i;
        cg_mkrun(&r, h.face, 20.0f, g, 2, 0, 0, FALSE, 0);
        before = dw_refcount((IUnknown *)nf);
        for (i = 0; i < 20; i++) { e = NULL; CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e)); held = dw_refcount((IUnknown *)nf); DW_RELEASE(e); }
        after = dw_refcount((IUnknown *)nf);
        t_note("private factory refs: before %lu, with enumerator %lu, after %lu", before, held, after);
        EXPECT_EQ(after, before);
    } else t_skip("cannot open font in a private factory");
    dwx_font_close(&h); free(b); IDWriteFactory2_Release(f); IDWriteFactory_Release(nf);
}
TEST(f2_iterate_without_releasing_between_runs) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_a, GID_a, GID_a }; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; BOOL has; int k = 0;
    const DWRITE_COLOR_GLYPH_RUN *keep[20]; cg_mkrun(&r, x.face, 20.0f, g, 3, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has && k < 20) { IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &keep[k]); k++; }
    EXPECT_EQ(k, 1 + 4 * 3);
    DW_RELEASE(e); cg_close(&x);
}
