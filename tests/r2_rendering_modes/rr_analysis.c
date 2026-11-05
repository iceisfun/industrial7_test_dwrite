/* IDWriteGlyphRunAnalysis matrix: creation overloads x rendering mode x measuring mode x orientation x transform, bounds and textures.
 *
 * Facts (the three CreateGlyphRunAnalysis overloads, glyph run analysis construction, GetAlphaTextureBounds,
 * CreateAlphaTexture, CreateAlphaTextureGrayscale):
 *   - the rendering mode / antialias mode pair selects the texture kind: GetAlphaTextureBounds(type) returns the bounds when `type` is the
 *     kind of this analysis and S_OK with an all-zero RECT otherwise; type >= 2 -> E_INVALIDARG with a zeroed RECT.
 *   - CreateAlphaTexture: bounds == NULL -> E_INVALIDARG; empty rectangle -> E_INVALIDARG (checked before the texture type);
 *     bufferSize smaller than the texture -> 0x8007007A, also for bufferSize 0; the 1x1 texture zero-fills bytes beyond width*height.
 *   - a uniform scale matrix (m12 = m21 = 0, m11 == m22) is folded into pixelsPerDip and the baseline origin before rasterizing.
 *   Fixture: the default synthetic font, glyph 'A' = box (100,0)-(900,1400) of 2048 units.  At em = 512 the box is x 25..225, y -350..0 (200x350 px).
 */
#include "rr.h"

#define RT(n) TEST(r2_rendering_modes_##n)
#define TT_AL 0
#define TT_CT 1

static const DWRITE_MATRIX XF[] = {
    { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 2, 0, 0 }, { 3, 0, 0, 2, 0, 0 }, { -1, 0, 0, 1, 0, 0 }, { 1, 0, 0, -1, 0, 0 }, { -1, 0, 0, -1, 0, 0 },
    { 0, 1, -1, 0, 0, 0 }, { 0, -1, 1, 0, 0, 0 }, { 0, 2, -2, 0, 0, 0 }, { 1, 0, 0, 1, 40, -30 }, { 2, 0, 0, 2, 10, 20 }, { 0, 1, -1, 0, 100, 5 },
    { 1, 0, 0.5f, 1, 0, 0 }, { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 },
};
#define NXF ((int)(sizeof XF / sizeof XF[0]))
static const char *const XFN[] = { "ident", "scale2", "scale3x2", "flipx", "flipy", "rot180", "rot90", "rot270", "rot90x2", "translate", "scale2+t", "rot90+t", "shear", "rot45" };
static int xf_axis_aligned(int i) { return i <= 11; }

/* exact bounds of the em=512 box under matrix m (axis-aligned matrices only) */
static void box_bounds(const DWRITE_MATRIX *m, float ox, float oy, RECT *r) {
    static const float px[2] = { 25, 225 }, py[2] = { -350, 0 };
    int i, j; float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (i = 0; i < 2; i++) for (j = 0; j < 2; j++) {
        float x = px[i] + ox, y = py[j] + oy;
        float tx = x * m->m11 + y * m->m21 + m->dx, ty = x * m->m12 + y * m->m22 + m->dy;
        if (tx < x0) x0 = tx; if (tx > x1) x1 = tx; if (ty < y0) y0 = ty; if (ty > y1) y1 = ty;
    }
    r->left = (LONG)floorf(x0 + 0.5f); r->top = (LONG)floorf(y0 + 0.5f); r->right = (LONG)floorf(x1 + 0.5f); r->bottom = (LONG)floorf(y1 + 0.5f);
}
/* texture kind of a mode: probes both and requires exactly one to be non-empty; returns the kind (0/1) or -1 */
static int kind_of(IDWriteGlyphRunAnalysis *a, RECT *rc_out) {
    RECT r0, r1; HRESULT h0 = rr_bounds(a, TT_AL, &r0), h1 = rr_bounds(a, TT_CT, &r1);
    int e0 = rr_rect_empty(&r0), e1 = rr_rect_empty(&r1);
    CHECK_HR_OK(h0); CHECK_HR_OK(h1);
    if (e0 && e1) return -1;
    t_check(e0 != e1, __FILE__, __LINE__, "exactly one texture kind", "al (%ld,%ld,%ld,%ld) ct (%ld,%ld,%ld,%ld)", r0.left, r0.top, r0.right, r0.bottom, r1.left, r1.top, r1.right, r1.bottom);
    if (!e0 && !e1) return -1;
    if (rc_out) *rc_out = e0 ? r1 : r0;
    return e0 ? TT_CT : TT_AL;
}
static void zero_rect_check(const RECT *r, const char *what) {
    t_check(!r->left && !r->top && !r->right && !r->bottom, __FILE__, __LINE__, "zero rect", "%s: (%ld,%ld,%ld,%ld)", what, r->left, r->top, r->right, r->bottom);
}

/* ---- creation: every valid combination ------------------------------------------------------------------------------ */
static void create_matrix_for_mode(int mode) {
    RR_RUN(r);
    int mm, sw, xi, ppd;
    rr_run_set(&r, "AB", 24.0f);
    for (mm = 0; mm < 3; mm++) for (sw = 0; sw < 2; sw++) for (xi = -1; xi < NXF; xi++) for (ppd = 1; ppd <= 2; ppd++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; HRESULT hr;
        r.run.isSideways = sw;
        hr = IDWriteFactory_CreateGlyphRunAnalysis(r.x.f, &r.run, (float)ppd, xi < 0 ? NULL : &XF[xi], (DWRITE_RENDERING_MODE)mode, (DWRITE_MEASURING_MODE)mm, 0, 0, &a);
        t_check(hr == S_OK && a && a != (void *)(ULONG_PTR)0x1000, __FILE__, __LINE__, "create", "mode %s mm %d sw %d xf %s ppd %d: %s", rr_mode_names[mode], mm, sw, xi < 0 ? "none" : XFN[xi], ppd, dw_hr_name(hr));
        if (hr == S_OK && a) { EXPECT_REFS(a, 1); IDWriteGlyphRunAnalysis_Release(a); }
    }
    rr_run_close(&r);
}
RT(create_matrix_aliased) { create_matrix_for_mode(1); }
RT(create_matrix_gdi_classic) { create_matrix_for_mode(2); }
RT(create_matrix_gdi_natural) { create_matrix_for_mode(3); }
RT(create_matrix_natural) { create_matrix_for_mode(4); }
RT(create_matrix_natural_symmetric) { create_matrix_for_mode(5); }

RT(create_f1_rejects_bad_rendering_modes) {
    RR_RUN(r);
    static const int bad[] = { 0, 6, 7, 8, 100, 255, 0x7fffffff };
    int i;
    rr_run_set(&r, "A", 24.0f);
    for (i = 0; i < 7; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000;
        HRESULT hr = rr_an(&r, &a, 1.0f, NULL, bad[i], 0, 0, 0);
        t_check(hr == RR_INVALIDARG && a == NULL, __FILE__, __LINE__, "bad mode", "mode %d -> %s, out %p", bad[i], dw_hr_name(hr), (void *)a);
    }
    rr_run_close(&r);
}
RT(create_f1_rejects_bad_measuring_modes) {
    RR_RUN(r);
    static const int bad[] = { 3, 4, 100, 0x7fffffff };
    int i, mode;
    rr_run_set(&r, "A", 24.0f);
    for (mode = 1; mode <= 5; mode++) for (i = 0; i < 4; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000;
        HRESULT hr = rr_an(&r, &a, 1.0f, NULL, mode, bad[i], 0, 0);
        t_check(hr == RR_INVALIDARG && a == NULL, __FILE__, __LINE__, "bad measuring", "mode %d mm %d -> %s", mode, bad[i], dw_hr_name(hr));
    }
    rr_run_close(&r);
}
RT(create_f1_bad_enum_does_not_leak_references) {
    RR_RUN(r);
    int i; LONG before;
    rr_run_set(&r, "A", 24.0f);
    before = (LONG)dw_refcount((IUnknown *)r.x.f0);
    for (i = 0; i < 20; i++) { IDWriteGlyphRunAnalysis *a; rr_an(&r, &a, 1.0f, NULL, 0, 0, 0, 0); rr_an(&r, &a, 1.0f, NULL, 4, 9, 0, 0); }
    EXPECT_EQ(dw_refcount((IUnknown *)r.x.f0), (ULONG)before);
    rr_run_close(&r);
}
RT(create_f2_mode_gridfit_antialias_table) {
    RR_RUN(r);
    int mode, gf, aa;
    rr_run_set(&r, "A", 24.0f);
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2)) { t_skip("IDWriteFactory2 unsupported"); rr_run_close(&r); return; }
    for (mode = 0; mode <= 8; mode++) for (gf = 0; gf <= 3; gf++) for (aa = 0; aa <= 2; aa++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000;
        HRESULT hr = rr_an2(&r, &a, NULL, mode, 0, gf, aa, 0, 0);
        int valid = mode >= 1 && mode <= 5 && gf <= 2 && aa <= 1;
        t_check(valid ? hr == S_OK && a : hr == RR_INVALIDARG && a == NULL, __FILE__, __LINE__, "f2 table", "mode %d gridfit %d aa %d -> %s", mode, gf, aa, dw_hr_name(hr));
        if (hr == S_OK && a) IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(create_f3_mode1_gridfit_antialias_table) {
    RR_RUN(r);
    int mode, gf, aa;
    rr_run_set(&r, "A", 24.0f);
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3)) { t_skip("IDWriteFactory3 unsupported"); rr_run_close(&r); return; }
    for (mode = 0; mode <= 9; mode++) for (gf = 0; gf <= 3; gf++) for (aa = 0; aa <= 2; aa++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000;
        HRESULT hr = rr_an3(&r, &a, NULL, mode, 0, gf, aa, 0, 0);
        int valid = mode >= 1 && mode <= 7 && mode != 6 && gf <= 2 && aa <= 1;
        t_check(valid ? hr == S_OK && a : hr == RR_INVALIDARG && a == NULL, __FILE__, __LINE__, "f3 table", "mode %d gridfit %d aa %d -> %s", mode, gf, aa, dw_hr_name(hr));
        if (hr == S_OK && a) IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(create_f2_f3_bad_measuring_and_null_run) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a;
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2);
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    rr_run_set(&r, "A", 24.0f);
    if (f2) {
        a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(rr_an2(&r, &a, NULL, 4, 3, 0, 0, 0, 0), RR_INVALIDARG); CHECK_OUT_NULL(a);
        a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(IDWriteFactory2_CreateGlyphRunAnalysis(f2, NULL, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0, &a), RR_INVALIDARG); CHECK_OUT_NULL(a);
    }
    if (f3) {
        a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(rr_an3(&r, &a, NULL, 4, 3, 0, 0, 0, 0), RR_INVALIDARG); CHECK_OUT_NULL(a);
        a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, NULL, NULL, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0, &a), RR_INVALIDARG); CHECK_OUT_NULL(a);
    }
    if (!f2 && !f3) t_skip("no IDWriteFactory2/3");
    rr_run_close(&r);
}
RT(create_nan_and_infinite_inputs_rejected) {
    RR_RUN(r);
    float nan = nanf(""), inf = INFINITY;
    DWRITE_MATRIX bad = { 1, 0, 0, 1, 0, 0 };
    IDWriteGlyphRunAnalysis *a;
    rr_run_set(&r, "A", 24.0f);
    a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(rr_an(&r, &a, nan, NULL, 4, 0, 0, 0), RR_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(rr_an(&r, &a, 1, NULL, 4, 0, nan, 0), RR_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(rr_an(&r, &a, 1, NULL, 4, 0, 0, nan), RR_INVALIDARG); CHECK_OUT_NULL(a);
    bad.m12 = nan;
    a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(rr_an(&r, &a, 1, &bad, 4, 0, 0, 0), RR_INVALIDARG); CHECK_OUT_NULL(a);
    bad.m12 = 0; bad.dx = nan;
    a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(rr_an(&r, &a, 1, &bad, 4, 0, 0, 0), RR_INVALIDARG); CHECK_OUT_NULL(a);
    (void)inf;
    rr_run_close(&r);
}
RT(create_ppd_zero_and_negative_rejected) {
    RR_RUN(r);
    static const float bad[] = { 0.0f, -1.0f, -0.0001f, -100.0f };
    int i;
    rr_run_set(&r, "A", 24.0f);
    for (i = 0; i < 4; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000;
        t_check(rr_an(&r, &a, bad[i], NULL, 4, 0, 0, 0) == RR_INVALIDARG && a == NULL, __FILE__, __LINE__, "ppd", "ppd %g", bad[i]);
    }
    rr_run_close(&r);
}
RT(create_negative_em_rejected_zero_em_ok) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a;
    rr_run_set(&r, "A", -1.0f);
    a = (IDWriteGlyphRunAnalysis *)(ULONG_PTR)0x1000; CHECK_HR(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0), RR_INVALIDARG); CHECK_OUT_NULL(a);
    rr_run_set(&r, "A", 0.0f);
    a = NULL; { HRESULT hr = rr_an(&r, &a, 1, NULL, 1, 0, 0, 0); CHECK(hr == S_OK || hr == RR_INVALIDARG); if (hr == S_OK) { RECT rc; CHECK_HR_OK(rr_bounds(a, TT_AL, &rc)); CHECK(rr_rect_empty(&rc)); IDWriteGlyphRunAnalysis_Release(a); } }
    rr_run_close(&r);
}

/* ---- texture kind per mode ----------------------------------------------------------------------------------------- */
RT(texture_kind_f1_modes) {
    RR_RUN(r);
    int mode, mm;
    rr_run_set(&r, "A", 512.0f);
    for (mode = 1; mode <= 5; mode++) for (mm = 0; mm < 3; mm++) {
        IDWriteGlyphRunAnalysis *a; RECT rc; int k;
        if (FAILED(rr_an(&r, &a, 1, NULL, mode, mm, 0, 0))) { CHECK(0); continue; }
        k = kind_of(a, &rc);
        t_check(k >= 0, __FILE__, __LINE__, "has a texture kind", "mode %s mm %d", rr_mode_names[mode], mm);
        if (mode == 1) t_check(k == TT_AL, __FILE__, __LINE__, "ALIASED is a 1x1 texture", "mm %d kind %d", mm, k);
        if (mode >= 4) t_check(k == TT_CT, __FILE__, __LINE__, "NATURAL modes are ClearType textures", "mode %d mm %d kind %d", mode, mm, k);
        if (mode == 2 || mode == 3) t_note("%s measuring %d: texture kind %s", rr_mode_names[mode], mm, k == TT_CT ? "CLEARTYPE_3x1" : "ALIASED_1x1");
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(texture_kind_f2_antialias_modes) {
    RR_RUN(r);
    int mode, aa, gf;
    rr_run_set(&r, "A", 512.0f);
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2)) { t_skip("IDWriteFactory2 unsupported"); rr_run_close(&r); return; }
    for (mode = 1; mode <= 5; mode++) for (aa = 0; aa <= 1; aa++) for (gf = 0; gf <= 2; gf++) {
        IDWriteGlyphRunAnalysis *a; RECT rc; int k;
        if (FAILED(rr_an2(&r, &a, NULL, mode, 0, gf, aa, 0, 0))) { CHECK(0); continue; }
        k = kind_of(a, &rc);
        t_check(k >= 0, __FILE__, __LINE__, "has a texture kind", "mode %s aa %d gf %d", rr_mode_names[mode], aa, gf);
        if (aa == 1) t_check(k == TT_AL, __FILE__, __LINE__, "grayscale antialiasing uses the 1x1 texture", "mode %s gf %d kind %d", rr_mode_names[mode], gf, k);
        if (mode == 1) t_check(k == TT_AL, __FILE__, __LINE__, "ALIASED mode uses the 1x1 texture", "aa %d gf %d kind %d", aa, gf, k);
        if (aa == 0 && mode >= 4) t_check(k == TT_CT, __FILE__, __LINE__, "ClearType antialiasing of NATURAL modes", "mode %d gf %d kind %d", mode, gf, k);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(texture_kind_f3_downsampled_mode) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; RECT rc;
    rr_run_set(&r, "A", 512.0f);
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3)) { t_skip("IDWriteFactory3 unsupported"); rr_run_close(&r); return; }
    CHECK_HR_OK(rr_an3(&r, &a, NULL, 7, 0, 0, 0, 0, 0));
    if (a) { int k = kind_of(a, &rc); CHECK(k >= 0); t_note("NATURAL_SYMMETRIC_DOWNSAMPLED cleartype analysis texture kind %d bounds (%ld,%ld,%ld,%ld)", k, rc.left, rc.top, rc.right, rc.bottom); IDWriteGlyphRunAnalysis_Release(a); }
    CHECK_HR_OK(rr_an3(&r, &a, NULL, 7, 0, 0, 1, 0, 0));
    if (a) { int k = kind_of(a, &rc); EXPECT_EQ(k, TT_AL); IDWriteGlyphRunAnalysis_Release(a); }
    rr_run_close(&r);
}
RT(texture_other_kind_gives_zero_rect_ok) {
    RR_RUN(r);
    int mode;
    rr_run_set(&r, "A", 512.0f);
    for (mode = 1; mode <= 5; mode++) {
        IDWriteGlyphRunAnalysis *a; RECT rc, other; int k;
        if (FAILED(rr_an(&r, &a, 1, NULL, mode, 0, 0, 0))) { CHECK(0); continue; }
        k = kind_of(a, &rc);
        if (k >= 0) { CHECK_HR_OK(rr_bounds(a, k == TT_AL ? TT_CT : TT_AL, &other)); zero_rect_check(&other, "other kind"); }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(bounds_type_out_of_range) {
    RR_RUN(r);
    static const unsigned bad[] = { 2, 3, 100, 0x80000000u, 0xFFFFFFFFu };
    int mode, i;
    rr_run_set(&r, "A", 24.0f);
    for (mode = 1; mode <= 5; mode++) for (i = 0; i < 5; i++) {
        IDWriteGlyphRunAnalysis *a; RECT rc;
        if (FAILED(rr_an(&r, &a, 1, NULL, mode, 0, 0, 0))) { CHECK(0); continue; }
        memset(&rc, 0x55, sizeof rc);
        t_check(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, (DWRITE_TEXTURE_TYPE)bad[i], &rc) == RR_INVALIDARG, __FILE__, __LINE__, "bad type", "type %u", bad[i]);
        zero_rect_check(&rc, "bad type rect");
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(create_alpha_texture_argument_order) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; RECT rc = { 0, -10, 10, 0 }, empty = { 5, 5, 5, 9 }; BYTE buf[1024];
    rr_run_set(&r, "A", 512.0f);
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0));
    if (a) {
        CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, NULL, buf, sizeof buf), RR_INVALIDARG);
        CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &empty, buf, sizeof buf), RR_INVALIDARG);
        /* the empty rectangle is rejected before the texture type is looked at */
        CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, (DWRITE_TEXTURE_TYPE)5, &empty, buf, sizeof buf), RR_INVALIDARG);
        CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, (DWRITE_TEXTURE_TYPE)5, &rc, buf, sizeof buf), RR_INVALIDARG);
        CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, (DWRITE_TEXTURE_TYPE)0xFFFFFFFF, &rc, buf, sizeof buf), RR_INVALIDARG);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}

/* ---- empty runs --------------------------------------------------------------------------------------------------------- */
RT(empty_run_every_mode) {
    RR_RUN(r);
    int mode, tt;
    rr_run_set(&r, "A", 512.0f);
    r.run.glyphCount = 0;
    for (mode = 1; mode <= 5; mode++) {
        IDWriteGlyphRunAnalysis *a;
        CHECK_HR_OK(rr_an(&r, &a, 1, NULL, mode, 0, 100, 100));
        if (!a) continue;
        for (tt = 0; tt < 2; tt++) {
            RECT rc;
            CHECK_HR_OK(rr_bounds(a, tt, &rc));
            CHECK(rr_rect_empty(&rc));
            if (rc.left || rc.top || rc.right || rc.bottom) t_note("empty run, mode %s texture %d: bounds (%ld,%ld,%ld,%ld)", rr_mode_names[mode], tt, rc.left, rc.top, rc.right, rc.bottom);
            { BYTE b[16]; CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, (DWRITE_TEXTURE_TYPE)tt, &rc, b, sizeof b), RR_INVALIDARG); }
        }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(blank_glyphs_run_every_mode) {
    RR_RUN(r);
    int mode, tt;
    rr_run_set(&r, "  ", 512.0f);         /* two space glyphs, no outline */
    for (mode = 1; mode <= 5; mode++) {
        IDWriteGlyphRunAnalysis *a;
        CHECK_HR_OK(rr_an(&r, &a, 1, NULL, mode, 0, 0, 0));
        if (!a) continue;
        for (tt = 0; tt < 2; tt++) { RECT rc; CHECK_HR_OK(rr_bounds(a, tt, &rc)); CHECK(rr_rect_empty(&rc)); }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(blank_glyph_plus_box_equals_box_alone_when_blank_first_has_no_advance) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a, *b; RECT ra, rb;
    float adv[2] = { 0.0f, 0.0f };
    rr_run_set(&r, " A", 512.0f);
    memcpy(r.adv, adv, sizeof adv); r.run.glyphAdvances = r.adv;
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0));
    rr_run_set(&r, "A", 512.0f);
    CHECK_HR_OK(rr_an(&r, &b, 1, NULL, 1, 0, 0, 0));
    if (a && b) { CHECK_HR_OK(rr_bounds(a, TT_AL, &ra)); CHECK_HR_OK(rr_bounds(b, TT_AL, &rb)); CHECK_RECT_EQ(ra, rb); CHECK_RECT(ra, 25, -350, 225, 0); }
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    if (b) IDWriteGlyphRunAnalysis_Release(b);
    rr_run_close(&r);
}

/* ---- aliased bounds and textures --------------------------------------------------------------------------------------- */
RT(aliased_bounds_independent_of_measuring_mode) {
    RR_RUN(r);
    int mm, em;
    for (em = 512; em <= 1536; em += 512) {
        rr_run_set(&r, "A", (float)em);
        for (mm = 0; mm < 3; mm++) {
            IDWriteGlyphRunAnalysis *a; RECT rc;
            CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, mm, 0, 0));
            if (!a) continue;
            CHECK_HR_OK(rr_bounds(a, TT_AL, &rc));
            t_check(rc.left == RR_X0(em) && rc.top == -RR_HT(em) && rc.right == RR_X1(em) && rc.bottom == 0, __FILE__, __LINE__, "box", "em %d mm %d: (%ld,%ld,%ld,%ld)", em, mm, rc.left, rc.top, rc.right, rc.bottom);
            IDWriteGlyphRunAnalysis_Release(a);
        }
    }
    rr_run_close(&r);
}
RT(aliased_bounds_transform_table) {
    RR_RUN(r);
    int i;
    rr_run_set(&r, "A", 512.0f);
    for (i = 0; i < NXF; i++) {
        IDWriteGlyphRunAnalysis *a; RECT rc, want;
        if (!xf_axis_aligned(i)) continue;
        CHECK_HR_OK(rr_an(&r, &a, 1, &XF[i], 1, 0, 0, 0));
        if (!a) continue;
        CHECK_HR_OK(rr_bounds(a, TT_AL, &rc));
        box_bounds(&XF[i], 0, 0, &want);
        t_check(rc.left == want.left && rc.top == want.top && rc.right == want.right && rc.bottom == want.bottom, __FILE__, __LINE__, "transformed box", "%s: got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)",
                XFN[i], rc.left, rc.top, rc.right, rc.bottom, want.left, want.top, want.right, want.bottom);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(aliased_bounds_transform_applies_to_origin) {
    RR_RUN(r);
    int i;
    rr_run_set(&r, "A", 512.0f);
    for (i = 0; i < NXF; i++) {
        IDWriteGlyphRunAnalysis *a; RECT rc, want;
        if (!xf_axis_aligned(i)) continue;
        CHECK_HR_OK(rr_an(&r, &a, 1, &XF[i], 1, 0, 7, -3));
        if (!a) continue;
        CHECK_HR_OK(rr_bounds(a, TT_AL, &rc));
        box_bounds(&XF[i], 7, -3, &want);
        t_check(rc.left == want.left && rc.top == want.top && rc.right == want.right && rc.bottom == want.bottom, __FILE__, __LINE__, "origin through matrix", "%s: got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)",
                XFN[i], rc.left, rc.top, rc.right, rc.bottom, want.left, want.top, want.right, want.bottom);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(aliased_texture_pixel_counts_transform_table) {
    RR_RUN(r);
    int i;
    rr_run_set(&r, "A", 512.0f);
    for (i = 0; i < NXF; i++) {
        IDWriteGlyphRunAnalysis *a; rr_tex t;
        if (!xf_axis_aligned(i)) continue;
        CHECK_HR_OK(rr_an(&r, &a, 1, &XF[i], 1, 0, 0, 0));
        if (!a) continue;
        if (rr_tex_get(a, TT_AL, &t)) {
            /* the transformed box covers exactly its bounding rectangle: every pixel 255 */
            t_check(rr_count(&t, 255, 255) == t.n, __FILE__, __LINE__, "solid box", "%s: %lu of %lu pixels are 255", XFN[i], (unsigned long)rr_count(&t, 255, 255), (unsigned long)t.n);
            rr_tex_free(&t);
        } else CHECKF(0, "%s: no texture", XFN[i]);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(aliased_texture_zero_fill_beyond_pixels) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; RECT rc;
    rr_run_set(&r, "A", 512.0f);
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0));
    if (a && SUCCEEDED(rr_bounds(a, TT_AL, &rc))) {
        RECT win = { rc.left, rc.top, rc.left + 8, rc.top + 4 };         /* 32 pixels */
        size_t n = 32 + 40, i; BYTE *p = (BYTE *)malloc(n);
        memset(p, 0xCD, n);
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, p, (UINT32)n));
        for (i = 0; i < 32; i++) CHECK(p[i] == 255);
        for (i = 32; i < n; i++) if (p[i] != 0) { CHECKF(0, "byte %lu beyond the texture = 0x%02x", (unsigned long)i, p[i]); break; }
        free(p);
    }
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    rr_run_close(&r);
}
RT(aliased_texture_size_protocol_all_aliased_inputs) {
    RR_RUN(r);
    int variant;
    rr_run_set(&r, "A", 512.0f);
    for (variant = 0; variant < 3; variant++) {
        IDWriteGlyphRunAnalysis *a; RECT rc;
        if (variant == 0) CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0));
        else if (variant == 1) { if (FAILED(rr_an2(&r, &a, NULL, 4, 0, 0, 1, 0, 0))) { a = NULL; } }
        else { if (FAILED(rr_an3(&r, &a, NULL, 7, 0, 0, 1, 0, 0))) { a = NULL; } }
        if (!a) continue;
        if (SUCCEEDED(rr_bounds(a, TT_AL, &rc)) && !rr_rect_empty(&rc)) {
            RECT win = { rc.left, rc.top, rc.left + 10, rc.top + 7 };    /* 70 */
            BYTE buf[200];
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 70), S_OK);
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 69), RR_NOBUF);
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 0), RR_NOBUF);
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 71), S_OK);
            CHECK(buf[70] == 0);
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 200), S_OK);
            CHECK(buf[199] == 0 && buf[70] == 0);
        } else CHECKF(0, "variant %d: no aliased bounds", variant);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(aliased_texture_failure_leaves_buffer_untouched) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; RECT rc;
    rr_run_set(&r, "A", 512.0f);
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0));
    if (a && SUCCEEDED(rr_bounds(a, TT_AL, &rc))) {
        BYTE buf[64]; size_t i, touched = 0;
        memset(buf, 0xCD, sizeof buf);
        CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, buf, 63), RR_NOBUF);
        for (i = 0; i < sizeof buf; i++) touched += buf[i] != 0xCD;
        EXPECT_EQ(touched, 0);       /* the size check precedes any write */
    }
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    rr_run_close(&r);
}
RT(aliased_origin_rounding_table) {
    RR_RUN(r);
    /* baseline origin fractions: the aliased bitmap lands on the pixel grid; the shift is the fraction rounded to the nearest integer */
    static const struct { float ox, oy; int dx, dy; } c[] = {
        { 0.0f, 0.0f, 0, 0 }, { 0.25f, 0.25f, 0, 0 }, { 0.2f, -0.2f, 0, 0 }, { 0.75f, 0.75f, 1, 1 }, { -0.75f, -0.75f, -1, -1 }, { 0.9f, -0.9f, 1, -1 },
        { 1.0f, 1.0f, 1, 1 }, { 1.25f, -1.25f, 1, -1 }, { 1.75f, -1.75f, 2, -2 }, { 10.4f, 20.6f, 10, 21 }, { -10.4f, -20.6f, -10, -21 },
    };
    int i;
    rr_run_set(&r, "A", 512.0f);
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        IDWriteGlyphRunAnalysis *a; RECT rc;
        CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, c[i].ox, c[i].oy));
        if (!a) continue;
        CHECK_HR_OK(rr_bounds(a, TT_AL, &rc));
        t_check(rc.left == 25 + c[i].dx && rc.right == 225 + c[i].dx && rc.top == -350 + c[i].dy && rc.bottom == c[i].dy, __FILE__, __LINE__, "origin fraction",
                "origin (%g,%g): got (%ld,%ld,%ld,%ld) want shift (%d,%d)", c[i].ox, c[i].oy, rc.left, rc.top, rc.right, rc.bottom, c[i].dx, c[i].dy);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(aliased_origin_half_pixel_note) {
    RR_RUN(r);
    static const float h[] = { 0.5f, -0.5f, 1.5f, -1.5f, 2.5f };
    int i;
    rr_run_set(&r, "A", 512.0f);
    for (i = 0; i < 5; i++) {
        IDWriteGlyphRunAnalysis *a; RECT rc;
        CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, h[i], h[i]));
        if (!a) continue;
        CHECK_HR_OK(rr_bounds(a, TT_AL, &rc));
        t_note("origin (%g,%g) -> left shift %ld top shift %ld", h[i], h[i], rc.left - 25, rc.bottom);
        CHECK(rc.right - rc.left == 200 && rc.bottom - rc.top == 350);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(aliased_bounds_ppd_and_em_are_interchangeable) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a, *b; RECT ra, rb;
    rr_run_set(&r, "A", 256.0f);
    CHECK_HR_OK(rr_an(&r, &a, 2.0f, NULL, 1, 0, 0, 0));
    rr_run_set(&r, "A", 512.0f);
    CHECK_HR_OK(rr_an(&r, &b, 1.0f, NULL, 1, 0, 0, 0));
    if (a && b) { CHECK_HR_OK(rr_bounds(a, TT_AL, &ra)); CHECK_HR_OK(rr_bounds(b, TT_AL, &rb)); CHECK_RECT_EQ(ra, rb); }
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    if (b) IDWriteGlyphRunAnalysis_Release(b);
    rr_run_close(&r);
}
RT(aliased_uniform_matrix_equals_ppd) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a, *b; RECT ra, rb;
    static const DWRITE_MATRIX m2 = { 2, 0, 0, 2, 0, 0 };
    rr_run_set(&r, "A", 256.0f);
    CHECK_HR_OK(rr_an(&r, &a, 1.0f, &m2, 1, 0, 3, 5));
    CHECK_HR_OK(rr_an(&r, &b, 2.0f, NULL, 1, 0, 3, 5));
    if (a && b) { CHECK_HR_OK(rr_bounds(a, TT_AL, &ra)); CHECK_HR_OK(rr_bounds(b, TT_AL, &rb)); CHECK_RECT_EQ(ra, rb); CHECK_RECT(ra, 31, -340, 231, 10); }
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    if (b) IDWriteGlyphRunAnalysis_Release(b);
    rr_run_close(&r);
}
RT(aliased_advances_and_offsets_matrix) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; RECT rc;
    DWRITE_GLYPH_OFFSET off[2] = { { 0, 0 }, { 10, -20 } };
    rr_run_set(&r, "AA", 512.0f);
    r.adv[0] = 300; r.adv[1] = 300; r.run.glyphAdvances = r.adv; r.run.glyphOffsets = off;
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0));
    if (a) { CHECK_HR_OK(rr_bounds(a, TT_AL, &rc)); CHECK_RECT(rc, 25, -350, 535, 20); IDWriteGlyphRunAnalysis_Release(a); }   /* a negative ascenderOffset moves the second glyph down */
    rr_run_close(&r);
}
RT(aliased_sideways_equals_rotation_matrix) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a, *b; RECT ra, rb;
    static const DWRITE_MATRIX rot = { 0, 1, -1, 0, 0, 0 };
    rr_run_set(&r, "A", 512.0f);
    r.run.isSideways = TRUE;
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0));
    r.run.isSideways = FALSE;
    CHECK_HR_OK(rr_an(&r, &b, 1, &rot, 1, 0, 0, 0));
    if (a && b) { CHECK_HR_OK(rr_bounds(a, TT_AL, &ra)); CHECK_HR_OK(rr_bounds(b, TT_AL, &rb)); t_note("sideways bounds (%ld,%ld,%ld,%ld); rot90 matrix (%ld,%ld,%ld,%ld)", ra.left, ra.top, ra.right, ra.bottom, rb.left, rb.top, rb.right, rb.bottom);
        CHECK(ra.right - ra.left == rb.right - rb.left || ra.right - ra.left == rb.bottom - rb.top); }
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    if (b) IDWriteGlyphRunAnalysis_Release(b);
    rr_run_close(&r);
}

/* ---- ClearType 3x1 against the aliased box ------------------------------------------------------------------------------- */
static void ct_relation(int mode, int em) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a, *c; RECT ra, rc;
    rr_run_set(&r, "A", (float)em);
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 1, 0, 0, 0));
    CHECK_HR_OK(rr_an(&r, &c, 1, NULL, mode, 0, 0, 0));
    if (a && c) {
        CHECK_HR_OK(rr_bounds(a, TT_AL, &ra)); CHECK_HR_OK(rr_bounds(c, TT_CT, &rc));
        if (mode == 5 && em >= 1024) {   /* symmetric ClearType yields an empty texture box from 1024 ppem up (Win10) */
            t_check(rc.left == rc.right && rc.top == rc.bottom, __FILE__, __LINE__, "symmetric ClearType box is empty at 1024+ ppem", "em %d: ct (%ld,%ld,%ld,%ld)", em, rc.left, rc.top, rc.right, rc.bottom);
        } else {
        t_check(rc.top == ra.top && rc.bottom == ra.bottom, __FILE__, __LINE__, "ClearType rows equal the aliased rows", "em %d: ct (%ld,%ld) al (%ld,%ld)", em, rc.top, rc.bottom, ra.top, ra.bottom);
        t_check(rc.left <= ra.left && rc.right >= ra.right, __FILE__, __LINE__, "ClearType is not narrower", "em %d: ct x %ld..%ld al x %ld..%ld", em, rc.left, rc.right, ra.left, ra.right);
        t_check(ra.left - rc.left <= 3 && rc.right - ra.right <= 3, __FILE__, __LINE__, "filter padding is at most 3 pixels", "em %d: pad %ld/%ld", em, ra.left - rc.left, rc.right - ra.right);
        t_check((ra.left - rc.left) == (rc.right - ra.right), __FILE__, __LINE__, "padding is symmetric", "em %d: %ld vs %ld", em, ra.left - rc.left, rc.right - ra.right);
        }
    }
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    if (c) IDWriteGlyphRunAnalysis_Release(c);
    rr_run_close(&r);
}
RT(cleartype_vs_aliased_natural_512) { ct_relation(4, 512); }
RT(cleartype_vs_aliased_natural_1024) { ct_relation(4, 1024); }
RT(cleartype_vs_aliased_natural_1536) { ct_relation(4, 1536); }
RT(cleartype_vs_aliased_symmetric_512) { ct_relation(5, 512); }
RT(cleartype_vs_aliased_symmetric_1024) { ct_relation(5, 1024); }

static void ct_texture_shape(int mode) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; rr_tex t;
    rr_run_set(&r, "A", 512.0f);
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, mode, 0, 0, 0));
    if (a && rr_tex_get(a, TT_CT, &t)) {
        int stride = t.w * 3, y, x, nondec = 0, asym = 0, mid = stride / 2;
        t_check(t.n == (size_t)stride * t.h, __FILE__, __LINE__, "3 bytes per pixel", "n %lu", (unsigned long)t.n);
        y = t.h / 2;
        /* along a row through the middle: zero outside, a rising ramp on the left, full coverage inside, mirrored ramp on the right */
        for (x = 1; x < mid; x++) nondec += t.p[y * stride + x] < t.p[y * stride + x - 1];
        for (x = 0; x < stride / 2; x++) asym += t.p[y * stride + x] != t.p[y * stride + stride - 1 - x];
        CHECKF(nondec == 0, "%d decreasing steps on the left half", nondec);
        CHECKF(asym == 0, "%d asymmetric bytes", asym);
        CHECK(t.p[y * stride + mid] == 255);
        CHECK(t.p[y * stride] <= t.p[y * stride + 1]);
        CHECK(t.p[y * stride + 3 * 15] == 255);
        /* identical rows: the box has no vertical structure */
        for (x = 0; x < stride; x++) { CHECK(t.p[x] == t.p[y * stride + x]); CHECK(t.p[(t.h - 1) * stride + x] == t.p[x]); }
        rr_tex_free(&t);
    } else CHECKF(0, "no ClearType texture for mode %d", mode);
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    rr_run_close(&r);
}
RT(cleartype_texture_shape_natural) { ct_texture_shape(4); }
RT(cleartype_texture_shape_symmetric) { ct_texture_shape(5); }
RT(cleartype_texture_size_protocol) {
    RR_RUN(r);
    int mode;
    rr_run_set(&r, "A", 512.0f);
    for (mode = 4; mode <= 5; mode++) {
        IDWriteGlyphRunAnalysis *a; RECT rc;
        CHECK_HR_OK(rr_an(&r, &a, 1, NULL, mode, 0, 0, 0));
        if (a && SUCCEEDED(rr_bounds(a, TT_CT, &rc))) {
            RECT win = { rc.left, rc.top, rc.left + 4, rc.top + 3 };       /* 12 pixels = 36 bytes */
            BYTE buf[128];
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &win, buf, 36), S_OK);
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &win, buf, 35), RR_NOBUF);
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &win, buf, 0), RR_NOBUF);
            memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &win, buf, 128), S_OK);
            /* the 1x1 request needs only w*h = 12 bytes but this analysis has no 1x1 texture: a documented failure, never a crash */
            { HRESULT hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 128); t_note("1x1 texture request on a ClearType analysis: %s", dw_hr_name(hr)); }
        } else CHECKF(0, "mode %d: no ClearType bounds", mode);
        if (a) IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(cleartype_texture_deterministic_across_objects) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a, *b; rr_tex ta, tb;
    rr_run_set(&r, "AB", 512.0f);
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 4, 0, 0.3f, 0.6f));
    CHECK_HR_OK(rr_an(&r, &b, 1, NULL, 4, 0, 0.3f, 0.6f));
    if (a && b && rr_tex_get(a, TT_CT, &ta)) {
        if (rr_tex_get(b, TT_CT, &tb)) { CHECK(ta.n == tb.n && ta.rc.left == tb.rc.left && ta.rc.top == tb.rc.top); if (ta.n == tb.n) CHECK(memcmp(ta.p, tb.p, ta.n) == 0); rr_tex_free(&tb); } else CHECK(0);
        rr_tex_free(&ta);
    } else CHECK(0);
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    if (b) IDWriteGlyphRunAnalysis_Release(b);
    rr_run_close(&r);
}
RT(cleartype_gridfit_and_overloads_agree_on_bounds) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a, *b, *c; RECT ra, rb, rc;
    rr_run_set(&r, "A", 512.0f);
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3)) { t_skip("IDWriteFactory3 unsupported"); rr_run_close(&r); return; }
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 4, 0, 0, 0));
    CHECK_HR_OK(rr_an2(&r, &b, NULL, 4, 0, 0, 0, 0, 0));
    CHECK_HR_OK(rr_an3(&r, &c, NULL, 4, 0, 0, 0, 0, 0));
    if (a && b && c) { CHECK_HR_OK(rr_bounds(a, TT_CT, &ra)); CHECK_HR_OK(rr_bounds(b, TT_CT, &rb)); CHECK_HR_OK(rr_bounds(c, TT_CT, &rc)); CHECK_RECT_EQ(ra, rb); CHECK_RECT_EQ(ra, rc); }
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    if (b) IDWriteGlyphRunAnalysis_Release(b);
    if (c) IDWriteGlyphRunAnalysis_Release(c);
    rr_run_close(&r);
}

/* ---- grayscale antialiasing (IDWriteFactory2/3) ------------------------------------------------------------------------- */
RT(grayscale_texture_covers_box) {
    RR_RUN(r);
    int mode, gf;
    rr_run_set(&r, "A", 512.0f);
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2)) { t_skip("IDWriteFactory2 unsupported"); rr_run_close(&r); return; }
    for (mode = 4; mode <= 5; mode++) for (gf = 0; gf <= 2; gf++) {
        IDWriteGlyphRunAnalysis *a; rr_tex t;
        CHECK_HR_OK(rr_an2(&r, &a, NULL, mode, 0, gf, 1, 0, 0));
        if (a && rr_tex_get(a, TT_AL, &t)) {
            int cx = t.w / 2, cy = t.h / 2;
            CHECK(t.p[cy * t.w + cx] == 255);
            CHECK(rr_count(&t, 1, 254) < t.n / 4);                                  /* only an edge fringe is partial */
            CHECK(t.w >= 200 && t.w <= 206 && t.h >= 350 && t.h <= 356);
            rr_tex_free(&t);
        } else CHECKF(0, "mode %d gf %d: no grayscale texture", mode, gf);
        if (a) IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(grayscale_size_protocol) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; RECT rc;
    rr_run_set(&r, "A", 512.0f);
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2)) { t_skip("IDWriteFactory2 unsupported"); rr_run_close(&r); return; }
    CHECK_HR_OK(rr_an2(&r, &a, NULL, 4, 0, 0, 1, 0, 0));
    if (a && SUCCEEDED(rr_bounds(a, TT_AL, &rc))) {
        RECT win = { rc.left, rc.top, rc.left + 5, rc.top + 5 }; BYTE buf[64];
        memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 25), S_OK);
        memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 24), RR_NOBUF);
        memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 0), RR_NOBUF);
        memset(buf, 0xCD, sizeof buf); CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &win, buf, 64), S_OK); CHECK(buf[25] == 0 && buf[63] == 0);
    } else CHECK(0);
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    rr_run_close(&r);
}
RT(grayscale_sum_close_to_aliased_area) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; rr_tex t;
    rr_run_set(&r, "A", 512.0f);
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2)) { t_skip("IDWriteFactory2 unsupported"); rr_run_close(&r); return; }
    CHECK_HR_OK(rr_an2(&r, &a, NULL, 4, 0, 0, 1, 0, 0));
    if (a && rr_tex_get(a, TT_AL, &t)) {
        double want = 200.0 * 350.0 * 255.0, got = (double)rr_sum(&t);
        t_check(fabs(got - want) <= want * 0.01, __FILE__, __LINE__, "coverage sum", "got %.0f want %.0f", got, want);
        rr_tex_free(&t);
    } else CHECK(0);
    if (a) IDWriteGlyphRunAnalysis_Release(a);
    rr_run_close(&r);
}
RT(grayscale_small_em_is_partial_coverage) {
    RR_RUN(r);
    int em;
    if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2)) { t_skip("IDWriteFactory2 unsupported"); rr_run_close(&r); return; }
    for (em = 5; em <= 11; em += 3) {
        IDWriteGlyphRunAnalysis *a; rr_tex t;
        rr_run_set(&r, "A", (float)em);
        CHECK_HR_OK(rr_an2(&r, &a, NULL, 4, 0, 0, 1, 0, 0));
        if (a && rr_tex_get(a, TT_AL, &t)) {
            /* the box is 0.39 em wide and 0.68 em tall: coverage sum is area-proportional, bytes range over 0..255 */
            double area = (800.0 / 2048.0 * em) * (1400.0 / 2048.0 * em), got = (double)rr_sum(&t) / 255.0;
            t_check(fabs(got - area) <= area * 0.15 + 0.5, __FILE__, __LINE__, "small-size coverage", "em %d: sum/255 = %.2f area %.2f", em, got, area);
            rr_tex_free(&t);
        } else CHECKF(0, "em %d: no texture", em);
        if (a) IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}

/* ---- lifetime / identity ------------------------------------------------------------------------------------------------------- */
RT(analysis_qi_surface) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a; int n, i;
    const dw_qi_row *rows = dw_iid_table(&n);
    rr_run_set(&r, "A", 24.0f);
    CHECK_HR_OK(rr_an(&r, &a, 1, NULL, 4, 0, 0, 0));
    if (a) {
        for (i = 0; i < n; i++) {
            void *p = (void *)(ULONG_PTR)0x1000; HRESULT hr = IUnknown_QueryInterface((IUnknown *)a, rows[i].iid, &p);
            if (!memcmp(rows[i].iid, &IID_IDWriteGlyphRunAnalysis, sizeof(IID))) { CHECK_HR_OK(hr); if (SUCCEEDED(hr)) { EXPECT_EQ(p, a); IUnknown_Release((IUnknown *)p); } }
            else { t_check(hr == E_NOINTERFACE && p == NULL, __FILE__, __LINE__, "other interfaces unsupported", "%s -> %s %p", rows[i].name, dw_hr_name(hr), p); if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p); }
        }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    rr_run_close(&r);
}
RT(analysis_outlives_run_font_and_factory_objects) {
    IDWriteGlyphRunAnalysis *a = NULL; RECT rc, rc2; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    dwx_font h; HRESULT hr; DWRITE_GLYPH_RUN run; UINT16 g = (UINT16)ttf_default_gid('A');
    if (!f) { t_skip("no isolated factory"); return; }
    hr = dwx_font_open_default(&h, f);
    if (FAILED(hr) || !h.face) { dwx_font_close(&h); IDWriteFactory_Release(f); t_skip("font"); return; }
    memset(&run, 0, sizeof run); run.fontFace = h.face; run.fontEmSize = 512; run.glyphCount = 1; run.glyphIndices = &g;
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(f, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a));
    if (a) {
        CHECK_HR_OK(rr_bounds(a, TT_AL, &rc));
        dwx_font_close(&h);
        IDWriteFactory_Release(f);
        g = 0; run.fontEmSize = 1; memset(&run, 0xEE, sizeof run);
        CHECK_HR_OK(rr_bounds(a, TT_AL, &rc2));
        CHECK_RECT_EQ(rc, rc2);
        { rr_tex t; if (rr_tex_get(a, TT_AL, &t)) { CHECK(rr_count(&t, 255, 255) == t.n); rr_tex_free(&t); } else CHECK(0); }
        IDWriteGlyphRunAnalysis_Release(a);
    } else { dwx_font_close(&h); IDWriteFactory_Release(f); }
}
RT(many_analyses_alive_together) {
    RR_RUN(r);
    IDWriteGlyphRunAnalysis *a[24]; int i, mode;
    rr_run_set(&r, "A", 24.0f);
    for (i = 0; i < 24; i++) { mode = 1 + i % 5; a[i] = NULL; CHECK_HR_OK(rr_an(&r, &a[i], 1.0f + (float)(i % 3), NULL, mode, i % 3, (float)i * 0.25f, 0)); }
    for (i = 0; i < 24; i++) if (a[i]) { RECT rc; int k = kind_of(a[i], &rc); CHECK(k >= 0); IDWriteGlyphRunAnalysis_Release(a[i]); }
    rr_run_close(&r);
}
