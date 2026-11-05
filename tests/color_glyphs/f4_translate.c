/* IDWriteFactory4::TranslateColorGlyphRun(origin, run, desc, desiredGlyphImageFormats, mode, transform, palette, &IDWriteColorGlyphRunEnumerator1)
 * (dwrite.dll!DWriteFactory::TranslateColorGlyphRun 0x1cf40, the same worker the IDWriteFactory2 entry reaches with formats = 7):
 *   *layers = NULL first; run == NULL or any bit of desiredGlyphImageFormats outside 0xFF -> E_INVALIDARG; otherwise
 *   used = desired & face formats (COLR dropped for an unknown palette index); no bit of 0xFC left, or no colour glyph -> DWRITE_E_NOCOLOR. */
#include "cg.h"

static UINT32 face_fmts(cgfont *x) { return x->f4 ? (UINT32)x->f4->lpVtbl->GetGlyphImageFormats(x->f4) : (CGF_TT | CGF_COLR); }
static const UINT32 k_sentinel = 0x12345678;

TEST(f4_factory_available) {
    CG_NEED_F4(f); IDWriteFactory *base = dw_shared(); IDWriteFactory4 *q = NULL;
    CHECK_HR_OK(IDWriteFactory_QueryInterface(base, &IID_IDWriteFactory4, (void **)&q));
    if (q) { CHECK(dw_same_identity((IUnknown *)q, (IUnknown *)f)); IDWriteFactory4_Release(q); }
}
TEST(f4_color_font_face_formats) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR);
    UINT32 ff = face_fmts(&x); EXPECT_EQ(ff, CGF_TT | CGF_COLR);
    cg_close(&x);
    if (!cg_open_plain(&x)) { cg_close(&x); return; }
    ff = face_fmts(&x); EXPECT_EQ(ff, CGF_TT);
    cg_close(&x);
}
TEST(f4_formats_none_is_nocolor) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_B }; static cgrun r; D2D1_POINT_2F o = { 0, 0 };
    IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)k_sentinel;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0);
    CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_NONE, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR);
    CHECK_OUT_NULL(e);
    cg_close(&x);
}
TEST(f4_outline_formats_alone_are_nocolor) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_B }; static cgrun r; D2D1_POINT_2F o = { 0, 0 }; int i;
    static const UINT32 fm[] = { CGF_TT, CGF_CFF, CGF_TT | CGF_CFF };
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0);
    for (i = 0; i < 3; i++) {
        IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)k_sentinel;
        CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)fm[i], DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR);
        CHECK_OUT_NULL(e);
    }
    cg_close(&x);
}
TEST(f4_colr_alone_succeeds) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F, GID_B }; static cgrun r;
    cg_mkrun(&r, x.face, 20.0f, g, 3, 0, 0, FALSE, 0);
    CHECK_HR(cg_check_f4(f, "COLR", &r.run, 0, 0, CGF_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, face_fmts(&x)), S_OK);
    cg_close(&x);
}
TEST(f4_colr_alone_run_formats) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F }; static cgrun r; D2D1_POINT_2F o = { 0, 0 };
    IDWriteColorGlyphRunEnumerator1 *e = NULL; BOOL has; int k = 0;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has) {
        const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c));
        /* base run: (used & 3) == 0 -> NONE; colour layers: COLR only */
        if (c) EXPECT_EQ(c->glyphImageFormat, k == 0 ? CGF_NONE : CGF_COLR);
        k++;
    }
    EXPECT_EQ(k, 3);
    DW_RELEASE(e); cg_close(&x);
}
TEST(f4_truetype_plus_colr_run_formats) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F }; static cgrun r; D2D1_POINT_2F o = { 0, 0 };
    IDWriteColorGlyphRunEnumerator1 *e = NULL; BOOL has; int k = 0;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(CGF_TT | CGF_COLR), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has) {
        const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c));
        if (c) EXPECT_EQ(c->glyphImageFormat, k == 0 ? CGF_TT : (CGF_TT | CGF_COLR));
        k++;
    }
    EXPECT_EQ(k, 3);
    DW_RELEASE(e); cg_close(&x);
}
TEST(f4_cff_plus_colr_uses_only_supported_bits) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_B, GID_F }; static cgrun r;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0);
    /* the face has no CFF outlines: CFF is masked out, the run formats are COLR / NONE */
    CHECK_HR(cg_check_f4(f, "CFF|COLR", &r.run, 0, 0, CGF_CFF | CGF_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, face_fmts(&x)), S_OK);
    cg_close(&x);
}
TEST(f4_formats_all_256_on_plain_font) {
    CG_NEED_F4(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 28 }; static cgrun r; D2D1_POINT_2F o = { 1, 2 }; int fm;
    cg_mkrun(&r, x.face, 20.0f, g, 3, 0, 0, FALSE, 0);
    for (fm = 0; fm < 256; fm++) {
        IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)k_sentinel;
        HRESULT hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)fm, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        CHECKF(hr == CG_E_NOCOLOR && e == NULL, "formats 0x%02x: %s %p", fm, dw_hr_name(hr), (void *)e);
    }
    cg_close(&x);
}
TEST(f4_formats_all_256_on_color_font) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x); int fm, k;
    static const UINT16 runs[][4] = { { GID_A }, { GID_F }, { GID_A, GID_F }, { GID_B, GID_C, GID_F, GID_a } }; static const int len[] = { 1, 1, 2, 4 };
    for (k = 0; k < 4; k++) for (fm = 0; fm < 256; fm++) {
        static cgrun r; char tag[48]; snprintf(tag, sizeof tag, "run %d formats 0x%02x", k, fm);
        cg_mkrun(&r, x.face, 24.0f, runs[k], len[k], 2, 0, FALSE, 0);
        cg_check_f4(f, tag, &r.run, 3, 4, (UINT32)fm, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, ff);
    }
    cg_close(&x);
}
TEST(f4_formats_all_256_on_svg_font) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR | CGFX_SVG); UINT32 ff = face_fmts(&x); int fm, k;
    if (!(ff & CGF_SVG)) { t_note("face formats 0x%x: the synthetic SVG table is not reported", ff); t_skip("SVG table not recognised"); cg_close(&x); return; }
    CHECK(ff & CGF_COLR); CHECK(ff & CGF_TT);
    static const UINT16 runs[][6] = { { GID_B }, { GID_F }, { GID_F, GID_F, GID_b }, { GID_A, GID_F, GID_B, GID_b, GID_c, GID_E }, { GID_F, GID_A, GID_F, GID_A } }; static const int len[] = { 1, 1, 3, 6, 4 };
    for (k = 0; k < 5; k++) for (fm = 0; fm < 256; fm += (fm < 32 ? 1 : 5)) {
        static cgrun r; char tag[48]; snprintf(tag, sizeof tag, "svg run %d formats 0x%02x", k, fm);
        cg_mkrun(&r, x.face, 24.0f, runs[k], len[k], 2, 0, FALSE, 0);
        cg_check_f4(f, tag, &r.run, 3, 4, (UINT32)fm, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, ff);
    }
    cg_close(&x);
}
TEST(f4_unknown_format_bits_are_invalid_arg) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; static cgrun r; D2D1_POINT_2F o = { 0, 0 }; int b;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    for (b = 8; b < 32; b++) {
        IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)k_sentinel; UINT32 v = 1u << b;
        HRESULT hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)v, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        CHECKF(hr == CG_E_INVALIDARG && e == NULL, "bit %d alone: %s", b, dw_hr_name(hr));
        e = (void *)(ULONG_PTR)k_sentinel;
        hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(v | CGF_COLR | CGF_TT), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        CHECKF(hr == CG_E_INVALIDARG && e == NULL, "bit %d with COLR|TT: %s", b, dw_hr_name(hr));
    }
    cg_close(&x);
}
TEST(f4_all_ones_format_is_invalid) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; static cgrun r; D2D1_POINT_2F o = { 0, 0 }; IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)k_sentinel;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0xFFFFFFFFu, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_INVALIDARG);
    CHECK_OUT_NULL(e);
    e = (void *)(ULONG_PTR)k_sentinel;
    CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x1FF, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_INVALIDARG);
    CHECK_OUT_NULL(e);
    cg_close(&x);
}
TEST(f4_invalid_format_checked_on_plain_font_too) {
    CG_NEED_F4(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g = 2; static cgrun r; D2D1_POINT_2F o = { 0, 0 }; IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)k_sentinel;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x100, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_INVALIDARG);
    CHECK_OUT_NULL(e);
    cg_close(&x);
}
TEST(f4_null_run_is_invalid_arg) {
    CG_NEED_F4(f); D2D1_POINT_2F o = { 0, 0 }; int i; static const UINT32 fm[] = { 0, CGF_COLR, CGF_TT | CGF_COLR, 0xFF };
    if (t_is_wine()) { t_skip("Wine dereferences the NULL run"); return; }
    for (i = 0; i < 4; i++) {
        IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)k_sentinel;
        HRESULT hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, NULL, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)fm[i], DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        CHECKF(hr == CG_E_INVALIDARG && e == NULL, "formats 0x%x: %s", fm[i], dw_hr_name(hr));
    }
}
TEST(f4_palette_index_sweep) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x);
    static const UINT32 pal[] = { 0, 1, 2, 3, 100, 0xFFFF, 0x10000, 0xFFFFFFFFu }; static const UINT32 fm[] = { CGF_COLR, CGF_TT | CGF_COLR, 0xFF }; int i, j;
    UINT16 g[] = { GID_C, GID_F, GID_D }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 1, 0, FALSE, 0);
    for (i = 0; i < 8; i++) for (j = 0; j < 3; j++) {
        char tag[48]; snprintf(tag, sizeof tag, "pal 0x%x fm 0x%x", pal[i], fm[j]);
        HRESULT hr = cg_check_f4(f, tag, &r.run, 0, 0, fm[j], DWRITE_MEASURING_MODE_NATURAL, NULL, pal[i], ff);
        CHECK_HR(hr, pal[i] < 2 ? S_OK : CG_E_NOCOLOR);
    }
    cg_close(&x);
}
TEST(f4_origin_is_applied_per_axis) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x); int i;
    static const float v[][2] = { { 0, 0 }, { 10, 0 }, { 0, 10 }, { -33.5f, 77.25f }, { 4096, -4096 }, { 0.125f, 0.0625f } };
    UINT16 g[] = { GID_F, GID_A, GID_F, GID_B }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 4, 2, 0, FALSE, 0);
    for (i = 0; i < 6; i++) { char tag[32]; snprintf(tag, sizeof tag, "origin %d", i); cg_check_f4(f, tag, &r.run, v[i][0], v[i][1], 0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, ff); }
    cg_close(&x);
}
TEST(f4_transform_sweep) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x); int i;
    static const DWRITE_MATRIX m[] = { { 1, 0, 0, 1, 0, 0 }, { 3, 0, 0, 3, 0, 0 }, { 0, 1, -1, 0, 0, 0 }, { 0.25f, 0, 0, 0.25f, 9, 9 }, { 1, 1, 1, 1, 0, 0 }, { 100, 0, 0, 100, 0, 0 } };
    UINT16 g[] = { GID_A, GID_F, GID_a }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 1, 0, FALSE, 0);
    for (i = 0; i < 6; i++) { char tag[32]; snprintf(tag, sizeof tag, "matrix %d", i); cg_check_f4(f, tag, &r.run, 2, 3, 0x7, DWRITE_MEASURING_MODE_NATURAL, &m[i], 0, ff); }
    cg_close(&x);
}
TEST(f4_measuring_modes) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x); int mm;
    UINT16 g[] = { GID_A, GID_F, GID_B }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, FALSE, 0);
    for (mm = 0; mm < 3; mm++) { char tag[32]; snprintf(tag, sizeof tag, "mm %d", mm); cg_check_f4(f, tag, &r.run, 2, 3, 0x7, (DWRITE_MEASURING_MODE)mm, NULL, 0, ff); }
    cg_close(&x);
}
TEST(f4_pattern_table_formats_and_palettes) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x);
    static const struct { UINT16 g[5]; int n; } p[] = {
        { { GID_A }, 1 }, { { GID_A, GID_A }, 2 }, { { GID_A, GID_F }, 2 }, { { GID_F, GID_A, GID_F }, 3 }, { { GID_A, GID_B, GID_C }, 3 }, { { GID_D, GID_E }, 2 },
        { { GID_a, GID_0, GID_0 + 2, GID_E, GID_F }, 5 }, { { 0, GID_C, 1 }, 3 }, { { GID_E }, 1 },
    };
    static const UINT32 fm[] = { 0x7, 0x4, 0x5, 0xFF, 0xFC, 0x3 }; int i, j, pal;
    for (pal = 0; pal < 2; pal++) for (j = 0; j < 6; j++) for (i = 0; i < (int)(sizeof p / sizeof p[0]); i++) {
        static cgrun r; char tag[64]; snprintf(tag, sizeof tag, "pattern %d fm 0x%x pal %d", i, fm[j], pal);
        cg_mkrun(&r, x.face, 32.0f, p[i].g, p[i].n, i & 1 ? 2 : 0, 0, FALSE, 0);
        cg_check_f4(f, tag, &r.run, 11, 22, fm[j], DWRITE_MEASURING_MODE_NATURAL, NULL, pal, ff);
    }
    cg_close(&x);
}
TEST(f4_right_to_left_and_levels) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x); static const UINT32 lv[] = { 0, 1, 2, 3, 125, 126 }; int i;
    UINT16 g[] = { GID_A, GID_F, GID_B, GID_C }; static cgrun r;
    for (i = 0; i < 6; i++) { char tag[32]; snprintf(tag, sizeof tag, "bidi %u", lv[i]); cg_mkrun(&r, x.face, 20.0f, g, 4, 2, 0, FALSE, lv[i]); cg_check_f4(f, tag, &r.run, 300, 5, 0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, ff); }
    cg_close(&x);
}
TEST(f4_sideways_with_provided_advances) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x); UINT16 g[] = { GID_A, GID_F, GID_B }; static cgrun r;
    cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, TRUE, 0); cg_check_f4(f, "sideways", &r.run, 30, 40, 0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, ff);
    cg_close(&x);
}
TEST(f4_matches_factory2_for_default_formats) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_F, GID_a, GID_C, GID_F }; static cgrun r; D2D1_POINT_2F o = { 17, 19 };
    IDWriteColorGlyphRunEnumerator *e2 = NULL; IDWriteColorGlyphRunEnumerator1 *e4 = NULL; BOOL h2, h4; int k = 0;
    cg_mkrun(&r, x.face, 20.0f, g, 4, 2, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(cg_f2(), o.x, o.y, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e2));
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(CGF_TT | CGF_CFF | CGF_COLR), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e4));
    while (e2 && e4) {
        const DWRITE_COLOR_GLYPH_RUN1 *a = NULL, *b = NULL;
        IDWriteColorGlyphRunEnumerator_MoveNext(e2, &h2); IDWriteColorGlyphRunEnumerator1_MoveNext(e4, &h4);
        CHECK(h2 == h4); if (!h2 || !h4) break;
        IDWriteColorGlyphRunEnumerator1_GetCurrentRun((IDWriteColorGlyphRunEnumerator1 *)e2, &a); IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e4, &b);
        if (a && b) {
            CHECK(a->glyphRun.glyphCount == b->glyphRun.glyphCount && a->glyphRun.glyphIndices[0] == b->glyphRun.glyphIndices[0] && a->baselineOriginX == b->baselineOriginX && a->baselineOriginY == b->baselineOriginY);
            CHECK(a->paletteIndex == b->paletteIndex && a->glyphImageFormat == b->glyphImageFormat && !memcmp(&a->runColor, &b->runColor, sizeof a->runColor));
        }
        k++;
    }
    EXPECT_EQ(k, 1 + 4 + 3);
    DW_RELEASE(e2); DW_RELEASE(e4); cg_close(&x);
}
TEST(f4_enumerator_also_answers_v0_interface) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_B; static cgrun r; D2D1_POINT_2F o = { 0, 0 }; IDWriteColorGlyphRunEnumerator1 *e = NULL; IDWriteColorGlyphRunEnumerator *e0 = NULL;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator, (void **)&e0)); CHECK((void *)e0 == (void *)e);
        DW_RELEASE(e0); DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(f4_description_is_ignored) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x); UINT16 g[] = { GID_A, GID_F }; WCHAR txt[] = L"AF"; UINT16 cm[] = { 0, 1 };
    DWRITE_GLYPH_RUN_DESCRIPTION d; static cgrun r; IDWriteColorGlyphRunEnumerator1 *e = NULL; D2D1_POINT_2F o = { 1, 2 }; BOOL has; int k = 0;
    memset(&d, 0, sizeof d); d.localeName = L"en-us"; d.string = txt; d.stringLength = 2; d.clusterMap = cm;
    cg_mkrun(&r, x.face, 20.0f, g, 2, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, &d, (DWRITE_GLYPH_IMAGE_FORMATS)7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has) {
        const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c);
        if (c && c->glyphRunDescription) t_note("run %d description pointer %p", k, (void *)c->glyphRunDescription);
        k++;
    }
    EXPECT_EQ(k, 3);
    DW_RELEASE(e); cg_close(&x);
}
TEST(f4_run_length_sweep) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT32 ff = face_fmts(&x); int n;
    for (n = 1; n <= 64; n++) {
        UINT16 g[64]; static cgrun r; char tag[32]; int i; for (i = 0; i < n; i++) g[i] = (UINT16)(i % 4 == 2 ? GID_F : GID_0 + i % 10);
        cg_mkrun(&r, x.face, 20.0f, g, n, 1, 0, FALSE, 0); snprintf(tag, sizeof tag, "n=%d", n);
        cg_check_f4(f, tag, &r.run, 0, 0, 0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, ff);
    }
    cg_close(&x);
}
TEST(f4_isolated_factory) {
    IDWriteFactory4 *f = (IDWriteFactory4 *)dw_factory_v(DWRITE_FACTORY_TYPE_ISOLATED, 4); if (!f) { t_skip("no isolated IDWriteFactory4"); return; }
    CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_C, GID_F }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 2, 2, 0, FALSE, 0);
    cg_check_f4(f, "isolated", &r.run, 7, 9, 0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, face_fmts(&x));
    cg_close(&x);
}
TEST(f4_out_pointer_zeroed_on_every_failure) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_F; static cgrun r; D2D1_POINT_2F o = { 0, 0 }; IDWriteColorGlyphRunEnumerator1 *e;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    e = (void *)(ULONG_PTR)k_sentinel; IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e); CHECK_OUT_NULL(e);
    e = (void *)(ULONG_PTR)k_sentinel; IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x400, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e); CHECK_OUT_NULL(e);
    g = GID_A; r.gl[0] = g;
    e = (void *)(ULONG_PTR)k_sentinel; IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)7, DWRITE_MEASURING_MODE_NATURAL, NULL, 9, &e); CHECK_OUT_NULL(e);
    e = (void *)(ULONG_PTR)k_sentinel; IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_NONE, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e); CHECK_OUT_NULL(e);
    cg_close(&x);
}
TEST(f4_failed_calls_do_not_leak_face_references) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; static cgrun r; D2D1_POINT_2F o = { 0, 0 }; IDWriteColorGlyphRunEnumerator1 *e = NULL; int i;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    ULONG before = dw_refcount((IUnknown *)x.face);
    for (i = 0; i < 40; i++) {
        IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_NONE, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 7, &e);
        IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x1000, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
    }
    EXPECT_REFS(x.face, before);
    if (SUCCEEDED(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e))) { DW_RELEASE(e); }
    EXPECT_REFS(x.face, before);
    cg_close(&x);
}
/* ---- SVG precedence and grouping ---- */
TEST(f4_svg_group_of_consecutive_glyphs) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR | CGFX_SVG); UINT32 ff = face_fmts(&x);
    if (!(ff & CGF_SVG)) { t_skip("SVG table not recognised (face formats 0x%x)", ff); cg_close(&x); return; }
    UINT16 g[] = { GID_F, GID_F, GID_F }; static cgrun r; D2D1_POINT_2F o = { 50, 60 }; IDWriteColorGlyphRunEnumerator1 *e = NULL; BOOL has; int k = 0;
    cg_mkrun(&r, x.face, 2048.0f, g, 3, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(CGF_TT | CGF_SVG), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has) {
        const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c);
        if (c && k == 0) { EXPECT_EQ(c->glyphRun.glyphCount, 3); EXPECT_EQ(c->glyphImageFormat, CGF_NONE); }
        if (c && k == 1) {
            EXPECT_EQ(c->glyphRun.glyphCount, 3); EXPECT_EQ(c->glyphImageFormat, CGF_SVG); EXPECT_EQ(c->paletteIndex, 0xFFFF);
            CHECK(c->baselineOriginX == 50.0f && c->baselineOriginY == 60.0f);
            if (c->glyphRun.glyphCount == 3) { CHECK(c->glyphRun.glyphIndices[0] == GID_F && c->glyphRun.glyphIndices[2] == GID_F); CHECK_NEAR(c->glyphRun.glyphAdvances[0], 1000.0f, 1e-2); CHECK_NEAR(c->glyphRun.glyphAdvances[2], 1000.0f, 1e-2); }
        }
        k++;
    }
    EXPECT_EQ(k, 2);
    DW_RELEASE(e); cg_close(&x);
}
TEST(f4_svg_splits_around_other_glyphs) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR | CGFX_SVG); UINT32 ff = face_fmts(&x);
    if (!(ff & CGF_SVG)) { t_skip("SVG table not recognised (face formats 0x%x)", ff); cg_close(&x); return; }
    static const UINT16 g[][5] = { { GID_F, GID_A, GID_F }, { GID_F, GID_H, GID_F }, { GID_F, 0, GID_b, GID_b }, { GID_B, GID_F, GID_F }, { GID_c, GID_c, GID_c, GID_F, GID_F } };
    static const int n[] = { 3, 3, 4, 3, 5 }; int i, fm;
    for (fm = 0; fm < 3; fm++) for (i = 0; i < 5; i++) {
        static cgrun r; char tag[48]; static const UINT32 f_[] = { CGF_SVG, CGF_SVG | CGF_COLR, 0xFF }; snprintf(tag, sizeof tag, "split %d fm 0x%x", i, f_[fm]);
        cg_mkrun(&r, x.face, 20.0f, g[i], n[i], 2, 0, FALSE, 0); cg_check_f4(f, tag, &r.run, 5, 6, f_[fm], DWRITE_MEASURING_MODE_NATURAL, NULL, 0, ff);
    }
    cg_close(&x);
}
TEST(f4_svg_wins_over_colr_for_the_same_glyph) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR | CGFX_SVG); UINT32 ff = face_fmts(&x);
    if (!(ff & CGF_SVG)) { t_skip("SVG table not recognised"); cg_close(&x); return; }
    UINT16 g = GID_B;    /* has both COLR layers and an SVG document */
    static cgrun r; D2D1_POINT_2F o = { 0, 0 }; IDWriteColorGlyphRunEnumerator1 *e = NULL; BOOL has; int k = 0;
    cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(CGF_SVG | CGF_COLR), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has) { const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c); if (c && k == 1) EXPECT_EQ(c->glyphImageFormat, CGF_SVG); k++; }
    EXPECT_EQ(k, 2);
    DW_RELEASE(e);
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)CGF_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e)); k = 0;
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has) { const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c); if (c && k == 1) EXPECT_EQ(c->glyphImageFormat, CGF_COLR); k++; }
    EXPECT_EQ(k, 2);
    DW_RELEASE(e); cg_close(&x);
}
TEST(f4_svg_only_glyph_in_font_without_palette_match) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR | CGFX_SVG); UINT32 ff = face_fmts(&x);
    if (!(ff & CGF_SVG)) { t_skip("SVG table not recognised"); cg_close(&x); return; }
    /* unknown palette index drops COLR but SVG documents do not depend on palettes */
    UINT16 g[] = { GID_A, GID_F, GID_b }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, FALSE, 0);
    CHECK_HR(cg_check_f4(f, "svg|colr bad palette", &r.run, 0, 0, CGF_SVG | CGF_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 9, ff), S_OK);
    CHECK_HR(cg_check_f4(f, "colr bad palette", &r.run, 0, 0, CGF_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 9, ff), CG_E_NOCOLOR);
    cg_close(&x);
}
TEST(f4_rtl_svg_origin) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR | CGFX_SVG); UINT32 ff = face_fmts(&x);
    if (!(ff & CGF_SVG)) { t_skip("SVG table not recognised"); cg_close(&x); return; }
    UINT16 g[] = { GID_A, GID_F, GID_F }; static cgrun r; cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, FALSE, 1);
    cg_check_f4(f, "rtl svg", &r.run, 200, 0, CGF_SVG | CGF_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, ff);
    cg_close(&x);
}
