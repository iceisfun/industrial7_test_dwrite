/* Constants, cross-product sweeps and robustness cases around TranslateColorGlyphRun. */
#include "cg.h"

TEST(x_constant_values) {
    EXPECT_EQ((UINT32)CG_E_NOCOLOR, 0x8898500Cu); EXPECT_EQ((UINT32)DWRITE_E_NOCOLOR, 0x8898500Cu); EXPECT_EQ((UINT32)CG_E_NOT_VALID_STATE, (UINT32)HRESULT_FROM_WIN32(ERROR_INVALID_STATE));
    EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_NONE, 0); EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE, 1); EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_CFF, 2); EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_COLR, 4);
    EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_SVG, 8); EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_PNG, 0x10); EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_JPEG, 0x20); EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_TIFF, 0x40);
    EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8, 0x80);
    EXPECT_EQ(DWRITE_MEASURING_MODE_NATURAL, 0); EXPECT_EQ(DWRITE_MEASURING_MODE_GDI_CLASSIC, 1); EXPECT_EQ(DWRITE_MEASURING_MODE_GDI_NATURAL, 2);
}
TEST(x_cross_product_factory2) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); static const float em[] = { 9, 16, 100 }; static const UINT32 bd[] = { 0, 1 }; int mm, a, o, e, b, pal, n = 0;
    UINT16 g[] = { GID_F, GID_A, GID_C, GID_F, GID_D }; static cgrun r;
    for (mm = 0; mm < 3; mm++) for (a = 1; a <= 2; a++) for (o = 0; o < 2; o++) for (e = 0; e < 3; e++) for (b = 0; b < 2; b++) for (pal = 0; pal < 2; pal++) {
        char tag[64]; snprintf(tag, sizeof tag, "mm%d adv%d off%d em%d bidi%d pal%d", mm, a, o, e, b, pal);
        cg_mkrun(&r, x.face, em[e], g, 5, a, o, FALSE, bd[b]); cg_check_f2(f, tag, &r.run, 12.5f, -4.0f, (DWRITE_MEASURING_MODE)mm, NULL, pal, CGF_TT | CGF_COLR); n++;
    }
    EXPECT_EQ(n, 3 * 2 * 2 * 3 * 2 * 2);
    cg_close(&x);
}
TEST(x_cross_product_factory4) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); static const UINT32 fm[] = { 4, 5, 7, 0xFC, 0xFF, 0x3, 0 }; int mm, a, f_, b, pal;
    UINT16 g[] = { GID_B, GID_F, GID_a, GID_E }; static cgrun r;
    for (mm = 0; mm < 3; mm++) for (a = 1; a <= 2; a++) for (f_ = 0; f_ < 7; f_++) for (b = 0; b < 2; b++) for (pal = 0; pal < 3; pal++) {
        char tag[64]; snprintf(tag, sizeof tag, "mm%d adv%d fm0x%x bidi%d pal%d", mm, a, fm[f_], b, pal);
        cg_mkrun(&r, x.face, 16.0f, g, 4, a, 0, FALSE, b); cg_check_f4(f, tag, &r.run, 3, 9, fm[f_], (DWRITE_MEASURING_MODE)mm, NULL, pal, CGF_TT | CGF_COLR);
    }
    cg_close(&x);
}
TEST(x_glyph_index_outside_the_font) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); static const UINT16 bad[] = { 69, 500, 5000, 0xFFFE, 0xFFFF }; int i;
    for (i = 0; i < 5; i++) {
        UINT16 g[] = { GID_A, bad[i] }; cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; int k = 0; BOOL has;
        cg_mkrun(&r, x.face, 20.0f, g, 2, 1, 0, FALSE, 0);
        HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        t_note("glyph %u next to a colour glyph: %s", bad[i], dw_hr_name(hr));
        CHECK(hr == S_OK ? e != NULL : e == NULL);
        if (hr == S_OK && e) { while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has && k < 20) k++; EXPECT_EQ(k, 3); DW_RELEASE(e); }
        g[0] = bad[i]; cg_mkrun(&r, x.face, 20.0f, g, 1, 1, 0, FALSE, 0); e = (void *)(ULONG_PTR)1;
        hr = IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        t_note("glyph %u alone: %s", bad[i], dw_hr_name(hr));
        if (SUCCEEDED(hr)) { DW_RELEASE(e); } else CHECK_OUT_NULL(e);
    }
    cg_close(&x);
}
TEST(x_rtl_offsets_flow_through) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_F, GID_A, GID_B }; static cgrun r;
    cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 1, FALSE, 1); cg_check_f2(f, "rtl + offsets", &r.run, 90, 10, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, CGF_TT | CGF_COLR);
    cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 1, TRUE, 1); cg_check_f2(f, "rtl + offsets + sideways", &r.run, 90, 10, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, CGF_TT | CGF_COLR);
    cg_close(&x);
}
TEST(x_translate_from_two_faces_of_one_blob_set) {
    CG_NEED_F2(f); cgfont a, b; if (!cg_open_fx(&a, CGFX_COLR)) { cg_close(&a); return; } if (!cg_open_fx(&b, CGFX_COLR | CGFX_SVG)) { cg_close(&a); cg_close(&b); return; }
    UINT16 g[] = { GID_A, GID_F }; static cgrun r1, r2; IDWriteColorGlyphRunEnumerator *e1 = NULL, *e2 = NULL; BOOL h1, h2; int k1 = 0, k2 = 0;
    cg_mkrun(&r1, a.face, 20.0f, g, 2, 1, 0, FALSE, 0); cg_mkrun(&r2, b.face, 20.0f, g, 2, 1, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r1.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e1));
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r2.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e2));
    while (e1 && IDWriteColorGlyphRunEnumerator_MoveNext(e1, &h1) == S_OK && h1) { const DWRITE_COLOR_GLYPH_RUN *c = NULL; IDWriteColorGlyphRunEnumerator_GetCurrentRun(e1, &c); CHECK(c && c->glyphRun.fontFace == a.face); k1++; }
    while (e2 && IDWriteColorGlyphRunEnumerator_MoveNext(e2, &h2) == S_OK && h2) { const DWRITE_COLOR_GLYPH_RUN *c = NULL; IDWriteColorGlyphRunEnumerator_GetCurrentRun(e2, &c); CHECK(c && c->glyphRun.fontFace == b.face); k2++; }
    EXPECT_EQ(k1, 3); EXPECT_EQ(k2, 3);
    DW_RELEASE(e1); DW_RELEASE(e2); cg_close(&a); cg_close(&b);
}
TEST(x_enumerator_vtable_slots) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; IDWriteColorGlyphRunEnumerator1 *e1 = NULL;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1)) && e1) {
        /* IDWriteColorGlyphRunEnumerator1 extends the base interface: the first slots are shared, the v1 GetCurrentRun is an extra slot */
        CHECK(e1->lpVtbl->QueryInterface == ((IDWriteColorGlyphRunEnumerator *)e1)->lpVtbl->QueryInterface);
        CHECK(e1->lpVtbl->MoveNext != NULL && e1->lpVtbl->GetCurrentRun != NULL && e1->lpVtbl->IDWriteColorGlyphRunEnumerator1_GetCurrentRun != NULL);
        CHECK(e1->lpVtbl->GetCurrentRun != (void *)e1->lpVtbl->IDWriteColorGlyphRunEnumerator1_GetCurrentRun);
        IDWriteColorGlyphRunEnumerator1_Release(e1);
    }
    DW_RELEASE(e); cg_close(&x);
}
TEST(x_factory4_vtable_slots) {
    CG_NEED_F4(f);
    CHECK(f->lpVtbl->IDWriteFactory4_TranslateColorGlyphRun != NULL);
    CHECK((void *)f->lpVtbl->IDWriteFactory4_TranslateColorGlyphRun != (void *)f->lpVtbl->TranslateColorGlyphRun);
}
TEST(x_stress_translate_and_walk) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_a, GID_C, GID_F, GID_0, GID_E }; static cgrun r; static cg_exp ex[CG_MAXEXP]; int i, n;
    cg_mkrun(&r, x.face, 20.0f, g, 5, 2, 1, FALSE, 0); n = cg_expect(&r.run, 1, 2, 7, CGF_TT | CGF_COLR, 1, ex);
    for (i = 0; i < 2000; i++) {
        IDWriteColorGlyphRunEnumerator *e = NULL; if (FAILED(IDWriteFactory2_TranslateColorGlyphRun(f, 1, 2, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 1, &e)) || !e) { CHECKF(0, "iteration %d", i); break; }
        if (i % 400 == 0) cg_walk(e, "stress", &r.run, ex, n, DWRITE_MEASURING_MODE_NATURAL);
        IDWriteColorGlyphRunEnumerator_Release(e);
    }
    cg_close(&x);
}
TEST(x_palette_changes_only_colors) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F, GID_a }; static cgrun r; static cg_exp e0[CG_MAXEXP], e1[CG_MAXEXP]; int n0, n1, i;
    cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, FALSE, 0);
    n0 = cg_expect(&r.run, 0, 0, 7, CGF_TT | CGF_COLR, 0, e0); n1 = cg_expect(&r.run, 0, 0, 7, CGF_TT | CGF_COLR, 1, e1);
    EXPECT_EQ(n0, n1);
    for (i = 0; i < n0 && i < n1; i++) CHECKF(e0[i].gl[0] == e1[i].gl[0] && e0[i].x == e1[i].x, "run %d glyph/origin depend on the palette", i);
    cg_close(&x);
}
