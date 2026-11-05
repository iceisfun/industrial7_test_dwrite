/* GetRecommendedRenderingMode of IDWriteFontFace1 / IDWriteFontFace2 / IDWriteFontFace3.
 *   FontFace1: no argument validation, *mode zeroed first, RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED(7) reported as NATURAL_SYMMETRIC(5).
 *   FontFace2 and FontFace3: outputs (mode, gridfit) zeroed first; threshold > 1 or measuringMode > 2 -> E_INVALIDARG.
 *   Both outputs are written unconditionally: the mode and gridfit pointers must not be NULL.
 *   ResolveRenderingMode: ppem >= outline threshold -> OUTLINE; GDI_NATURAL measuring -> GDI_NATURAL; GDI_CLASSIC measuring -> ALIASED or
 *   GDI_CLASSIC; NATURAL measuring -> a natural mode (or ALIASED from gasp) for small sizes. The ALIASED threshold is 350 ppem. */
#include "ffx.h"

#define EINVAL ((HRESULT)0x80070057)
static const DWRITE_MATRIX *NOXF = NULL;

static HRESULT rm1(ffx *x, float em, float dx, float dy, const DWRITE_MATRIX *t, BOOL sw, int th, int mm, DWRITE_RENDERING_MODE *m) {
    *m = (DWRITE_RENDERING_MODE)0xCCCC;
    return ffx_c_rm1(x->f1, em, dx, dy, t, sw, (DWRITE_OUTLINE_THRESHOLD)th, (DWRITE_MEASURING_MODE)mm, m);
}
static HRESULT rm2(ffx *x, float em, float dx, float dy, const DWRITE_MATRIX *t, BOOL sw, int th, int mm, IDWriteRenderingParams *p, DWRITE_RENDERING_MODE *m, DWRITE_GRID_FIT_MODE *g) {
    *m = (DWRITE_RENDERING_MODE)0xCCCC; *g = (DWRITE_GRID_FIT_MODE)0xCCCC;
    return ffx_c_rm2(x->f2, em, dx, dy, t, sw, (DWRITE_OUTLINE_THRESHOLD)th, (DWRITE_MEASURING_MODE)mm, p, m, g);
}
static HRESULT rm3(ffx *x, float em, float dx, float dy, const DWRITE_MATRIX *t, BOOL sw, int th, int mm, IDWriteRenderingParams *p, DWRITE_RENDERING_MODE1 *m, DWRITE_GRID_FIT_MODE *g) {
    *m = (DWRITE_RENDERING_MODE1)0xCCCC; *g = (DWRITE_GRID_FIT_MODE)0xCCCC;
    return ffx_c_rm3(x->f3, em, dx, dy, t, sw, (DWRITE_OUTLINE_THRESHOLD)th, (DWRITE_MEASURING_MODE)mm, p, m, g);
}

/* ---- FontFace1 ------------------------------------------------------------------------------------------------------- */
TEST(rm1_grid_returns_valid_modes) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const float ems[] = { 4, 8, 12, 16, 24, 48, 96, 200, 349, 350, 351, 1000, 5000 };
    int i, th, mm;
    for (i = 0; i < (int)(sizeof ems / sizeof ems[0]); i++) for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) {
        DWRITE_RENDERING_MODE m; HRESULT hr = rm1(&x, ems[i], 96, 96, NOXF, FALSE, th, mm, &m);
        t_check(hr == S_OK && m >= 1 && m <= 6, __FILE__, __LINE__, "rm1 grid", "em %g th %d mm %d: %s mode %d", ems[i], th, mm, dw_hr_name(hr), (int)m);
    }
    ffx_close(&x);
}
TEST(rm1_gdi_natural_measuring) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const float ems[] = { 6, 9, 12, 16, 24, 48, 100, 300 };
    int i;
    for (i = 0; i < 8; i++) {
        DWRITE_RENDERING_MODE m; CHECK_HR_OK(rm1(&x, ems[i], 96, 96, NOXF, FALSE, 1, DWRITE_MEASURING_MODE_GDI_NATURAL, &m));
        t_check(m == DWRITE_RENDERING_MODE_GDI_NATURAL, __FILE__, __LINE__, "gdi natural", "em %g: mode %d", ems[i], (int)m);
    }
    ffx_close(&x);
}
TEST(rm1_gdi_classic_measuring_is_aliased_or_classic) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const float ems[] = { 6, 9, 12, 16, 24, 48, 100, 300 };
    int i, th;
    for (th = 0; th < 2; th++) for (i = 0; i < 8; i++) {
        DWRITE_RENDERING_MODE m; CHECK_HR_OK(rm1(&x, ems[i], 96, 96, NOXF, FALSE, th, DWRITE_MEASURING_MODE_GDI_CLASSIC, &m));
        t_check(m == DWRITE_RENDERING_MODE_ALIASED || m == DWRITE_RENDERING_MODE_GDI_CLASSIC || m == DWRITE_RENDERING_MODE_OUTLINE, __FILE__, __LINE__, "gdi classic", "em %g th %d: mode %d", ems[i], th, (int)m);
    }
    ffx_close(&x);
}
TEST(rm1_outline_for_huge_sizes_every_measuring_mode) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    int th, mm;
    for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) {
        DWRITE_RENDERING_MODE m; CHECK_HR_OK(rm1(&x, 1000.0f, 96, 96, NOXF, FALSE, th, mm, &m));
        t_check(m == DWRITE_RENDERING_MODE_OUTLINE, __FILE__, __LINE__, "outline", "th %d mm %d: mode %d", th, mm, (int)m);
    }
    ffx_close(&x);
}
TEST(rm1_aliased_threshold_is_350_ppem) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_RENDERING_MODE m;
    CHECK_HR_OK(rm1(&x, 349.0f, 96, 96, NOXF, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &m));
    CHECK(m != DWRITE_RENDERING_MODE_OUTLINE);
    CHECK_HR_OK(rm1(&x, 350.0f, 96, 96, NOXF, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &m));
    EXPECT_EQ(m, DWRITE_RENDERING_MODE_OUTLINE);
    CHECK_HR_OK(rm1(&x, 351.0f, 96, 96, NOXF, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &m));
    EXPECT_EQ(m, DWRITE_RENDERING_MODE_OUTLINE);
    ffx_close(&x);
}
TEST(rm1_antialiased_threshold_not_above_aliased) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    /* "a relatively low threshold" for anti-aliased outlines vs "a much higher" one for aliased: scan for the first outline size for each */
    int first[2] = { -1, -1 }, th, em;
    for (th = 0; th < 2; th++) for (em = 1; em <= 600; em++) {
        DWRITE_RENDERING_MODE m; rm1(&x, (float)em, 96, 96, NOXF, FALSE, th, DWRITE_MEASURING_MODE_NATURAL, &m);
        if (m == DWRITE_RENDERING_MODE_OUTLINE) { first[th] = em; break; }
    }
    t_note("first outline size: anti-aliased threshold %d ppem, aliased threshold %d ppem", first[0], first[1]);
    CHECK(first[0] > 0 && first[1] > 0);
    CHECK(first[0] <= first[1]);
    EXPECT_EQ(first[1], 350);
    ffx_close(&x);
}
TEST(rm1_small_sizes_are_never_outline) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    int th, mm, em;
    for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) for (em = 1; em <= 60; em += 3) {
        DWRITE_RENDERING_MODE m; CHECK_HR_OK(rm1(&x, (float)em, 96, 96, NOXF, FALSE, th, mm, &m));
        t_check(m != DWRITE_RENDERING_MODE_OUTLINE, __FILE__, __LINE__, "small", "em %d th %d mm %d -> outline", em, th, mm);
    }
    ffx_close(&x);
}
TEST(rm1_dpi_scales_ppem) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_RENDERING_MODE m;
    CHECK_HR_OK(rm1(&x, 100.0f, 96, 96, NOXF, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &m));
    CHECK(m != DWRITE_RENDERING_MODE_OUTLINE);
    CHECK_HR_OK(rm1(&x, 100.0f, 480, 480, NOXF, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &m));
    EXPECT_EQ(m, DWRITE_RENDERING_MODE_OUTLINE);        /* 100 * 480 / 96 = 500 ppem */
    ffx_close(&x);
}
TEST(rm1_scale_transform_scales_ppem) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_MATRIX t = { 2, 0, 0, 2, 0, 0 }; DWRITE_RENDERING_MODE m;
    CHECK_HR_OK(rm1(&x, 200.0f, 96, 96, &t, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &m));
    EXPECT_EQ(m, DWRITE_RENDERING_MODE_OUTLINE);        /* 200 * 2 = 400 ppem */
    DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 };
    CHECK_HR_OK(rm1(&x, 200.0f, 96, 96, &id, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &m));
    CHECK(m != DWRITE_RENDERING_MODE_OUTLINE);
    ffx_close(&x);
}
TEST(rm1_rotation_and_sideways_succeed) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_MATRIX rot = { 0, 1, -1, 0, 0, 0 }, sk = { 1, 0, 0.5f, 1, 3, 4 }; DWRITE_RENDERING_MODE m;
    CHECK_HR_OK(rm1(&x, 16.0f, 96, 96, &rot, FALSE, 0, 0, &m)); CHECK(m >= 1 && m <= 6);
    CHECK_HR_OK(rm1(&x, 16.0f, 96, 96, &sk, TRUE, 0, 0, &m)); CHECK(m >= 1 && m <= 6);
    CHECK_HR_OK(rm1(&x, 16.0f, 96, 144, NOXF, TRUE, 1, 1, &m)); CHECK(m >= 1 && m <= 6);
    CHECK_HR_OK(rm1(&x, 16.0f, 144, 96, NOXF, FALSE, 1, 2, &m)); CHECK(m >= 1 && m <= 6);
    ffx_close(&x);
}
TEST(rm1_never_reports_downsampled_mode) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    float dpis[] = { 96, 192, 480, 960 }; int d, em, bad = 0;
    for (d = 0; d < 4; d++) for (em = 1; em <= 400; em += 7) {
        DWRITE_RENDERING_MODE m; rm1(&x, (float)em, dpis[d], dpis[d], NOXF, FALSE, 1, 0, &m);
        if ((int)m == 7 || (int)m == 0) bad++;
    }
    EXPECT_EQ(bad, 0);       /* NATURAL_SYMMETRIC_DOWNSAMPLED is mapped to NATURAL_SYMMETRIC for the older interfaces */
    ffx_close(&x);
}
TEST(rm1_repeatable) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_RENDERING_MODE a, b; rm1(&x, 18, 96, 96, NOXF, FALSE, 0, 0, &a); rm1(&x, 18, 96, 96, NOXF, FALSE, 0, 0, &b);
    EXPECT_EQ(a, b);
    ffx_close(&x);
}
TEST(rm1_gasp_variants_note) {
    static const struct { unsigned short ppem, beh; const char *n; } g[] = { { 0xFFFF, 0 }, { 0xFFFF, 1 }, { 0xFFFF, 2 }, { 0xFFFF, 3 }, { 0xFFFF, 0xF }, { 0xFFFF, 0xA } };
    int i;
    for (i = 0; i < 6; i++) {
        ttf_gasp gp[1] = { { g[i].ppem, g[i].beh } };
        ttf_spec s; ttf_default_spec(&s); s.gasp = gp; s.ngasp = 1;
        ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
        DWRITE_RENDERING_MODE m8, m16, m40; rm1(&x, 8, 96, 96, NOXF, FALSE, 0, 0, &m8); rm1(&x, 16, 96, 96, NOXF, FALSE, 0, 0, &m16); rm1(&x, 40, 96, 96, NOXF, FALSE, 0, 0, &m40);
        t_note("gasp behavior 0x%X: natural modes at 8/16/40 ppem = %d/%d/%d", g[i].beh, (int)m8, (int)m16, (int)m40);
        CHECK(m8 >= 1 && m8 <= 6 && m16 >= 1 && m16 <= 6 && m40 >= 1 && m40 <= 6);
        ffx_close(&x);
    }
}
TEST(rm1_system_fonts_valid) {
    static const WCHAR *fam[] = { L"Arial", L"Segoe UI", L"Times New Roman", L"Consolas", L"Courier New", L"MS Gothic" };
    int i;
    for (i = 0; i < 6; i++) {
        ffx x; if (!ffx_open_sys(&x, fam[i], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); continue; }
        DWRITE_RENDERING_MODE a, b, c; HRESULT h1 = rm1(&x, 12, 96, 96, NOXF, FALSE, 0, 0, &a), h2 = rm1(&x, 12, 96, 96, NOXF, FALSE, 0, 1, &b), h3 = rm1(&x, 12, 96, 96, NOXF, FALSE, 0, 2, &c);
        CHECK(h1 == S_OK && h2 == S_OK && h3 == S_OK);
        t_note("%ls 12 ppem natural/classic/gdi-natural: %d/%d/%d", fam[i], (int)a, (int)b, (int)c);
        EXPECT_EQ(c, DWRITE_RENDERING_MODE_GDI_NATURAL);
        ffx_close(&x);
    }
}

/* ---- FontFace2 ------------------------------------------------------------------------------------------------------- */
TEST(rm2_invalid_enum_arguments_zero_outputs) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const struct { int th, mm; } bad[] = { { 2, 0 }, { 7, 0 }, { -1, 0 }, { 0, 3 }, { 0, 4 }, { 1, 100 }, { 2, 3 }, { 0x7FFFFFFF, 0 }, { 0, -1 } };
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        DWRITE_RENDERING_MODE m; DWRITE_GRID_FIT_MODE g;
        HRESULT hr = rm2(&x, 16, 96, 96, NOXF, FALSE, bad[i].th, bad[i].mm, NULL, &m, &g);
        t_check(hr == EINVAL, __FILE__, __LINE__, "rm2 enum", "th %d mm %d -> %s", bad[i].th, bad[i].mm, dw_hr_name(hr));
        t_check((int)m == 0 && (int)g == 0, __FILE__, __LINE__, "rm2 zero", "th %d mm %d: outputs %d/%d", bad[i].th, bad[i].mm, (int)m, (int)g);
    }
    ffx_close(&x);
}
TEST(rm2_valid_arguments_succeed_and_in_range) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    int th, mm; float ems[] = { 8, 16, 40, 400 }; int i;
    for (i = 0; i < 4; i++) for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) {
        DWRITE_RENDERING_MODE m; DWRITE_GRID_FIT_MODE g;
        HRESULT hr = rm2(&x, ems[i], 96, 96, NOXF, FALSE, th, mm, NULL, &m, &g);
        t_check(hr == S_OK && m >= 1 && m <= 6 && g >= 0 && g <= 2, __FILE__, __LINE__, "rm2", "em %g th %d mm %d: %s mode %d grid %d", ems[i], th, mm, dw_hr_name(hr), (int)m, (int)g);
    }
    ffx_close(&x);
}
TEST(rm2_agrees_with_rm1_without_params) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    int th, mm, em, bad = 0;
    for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) for (em = 6; em <= 420; em += 17) {
        DWRITE_RENDERING_MODE a, b; DWRITE_GRID_FIT_MODE g;
        rm1(&x, (float)em, 96, 96, NOXF, FALSE, th, mm, &a); rm2(&x, (float)em, 96, 96, NOXF, FALSE, th, mm, NULL, &b, &g);
        if (a != b) bad++;
    }
    EXPECT_EQ(bad, 0);
    ffx_close(&x);
}
TEST(rm2_outline_has_defined_grid_fit) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_RENDERING_MODE m; DWRITE_GRID_FIT_MODE g;
    CHECK_HR_OK(rm2(&x, 1000, 96, 96, NOXF, FALSE, 0, 0, NULL, &m, &g));
    EXPECT_EQ(m, DWRITE_RENDERING_MODE_OUTLINE);
    t_note("outline grid fit mode %d", (int)g);
    CHECK(g >= DWRITE_GRID_FIT_MODE_DISABLED && g <= DWRITE_GRID_FIT_MODE_ENABLED);
    ffx_close(&x);
}

/* ---- FontFace3 (rendering params carrying explicit modes) ---------------------------------------------------------------- */
static IDWriteRenderingParams3 *mkparams(DWRITE_RENDERING_MODE1 mode, DWRITE_GRID_FIT_MODE grid) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    IDWriteRenderingParams3 *p = NULL;
    if (!f3) return NULL;
    if (FAILED((f3->lpVtbl->IDWriteFactory3_CreateCustomRenderingParams)(f3, 1.8f, 0.5f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, mode, grid, &p))) return NULL;
    return p;
}
TEST(rm3_invalid_enum_arguments_zero_outputs) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const struct { int th, mm; } bad[] = { { 2, 0 }, { 5, 1 }, { 0, 3 }, { 1, 9 }, { 0x100, 0x100 } };
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
        HRESULT hr = rm3(&x, 16, 96, 96, NOXF, FALSE, bad[i].th, bad[i].mm, NULL, &m, &g);
        t_check(hr == EINVAL, __FILE__, __LINE__, "rm3 enum", "th %d mm %d -> %s", bad[i].th, bad[i].mm, dw_hr_name(hr));
        t_check((int)m == 0 && (int)g == 0, __FILE__, __LINE__, "rm3 zero", "th %d mm %d: outputs %d/%d", bad[i].th, bad[i].mm, (int)m, (int)g);
    }
    ffx_close(&x);
}
TEST(rm3_valid_arguments_in_range) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    int th, mm, i; float ems[] = { 8, 16, 40, 400, 2000 };
    for (i = 0; i < 5; i++) for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) {
        DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
        HRESULT hr = rm3(&x, ems[i], 96, 96, NOXF, FALSE, th, mm, NULL, &m, &g);
        t_check(hr == S_OK && (int)m >= 1 && (int)m <= 7 && (int)g >= 0 && (int)g <= 2, __FILE__, __LINE__, "rm3", "em %g th %d mm %d: %s mode %d grid %d", ems[i], th, mm, dw_hr_name(hr), (int)m, (int)g);
    }
    ffx_close(&x);
}
TEST(rm3_outline_and_gdi_natural_basics) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
    CHECK_HR_OK(rm3(&x, 1000, 96, 96, NOXF, FALSE, 0, 0, NULL, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_OUTLINE);
    CHECK_HR_OK(rm3(&x, 16, 96, 96, NOXF, FALSE, 0, DWRITE_MEASURING_MODE_GDI_NATURAL, NULL, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_GDI_NATURAL);
    ffx_close(&x);
}
TEST(rm3_relation_to_rm2_note) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    int mm, em, diff = 0, n = 0;
    for (mm = 0; mm < 3; mm++) for (em = 6; em <= 420; em += 17) {
        DWRITE_RENDERING_MODE a; DWRITE_RENDERING_MODE1 b; DWRITE_GRID_FIT_MODE g1, g2;
        rm2(&x, (float)em, 96, 96, NOXF, FALSE, 0, mm, NULL, &a, &g1); rm3(&x, (float)em, 96, 96, NOXF, FALSE, 0, mm, NULL, &b, &g2);
        int conv = (int)b == 7 ? 5 : (int)b; n++;
        if (conv != (int)a || g1 != g2) { diff++; t_note("em %d mm %d: face2 %d/%d face3 %d/%d", em, mm, (int)a, (int)g1, (int)b, (int)g2); }
    }
    t_note("%d of %d combinations differ between IDWriteFontFace2 and IDWriteFontFace3", diff, n);
    ffx_close(&x);
}
static void explicit_mode_case(DWRITE_RENDERING_MODE1 pm, DWRITE_GRID_FIT_MODE pg, int expect_mode, int expect_grid) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteRenderingParams3 *p = mkparams(pm, pg);
    if (!p) { t_skip("IDWriteFactory3::CreateCustomRenderingParams unavailable"); ffx_close(&x); return; }
    DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
    CHECK_HR_OK(rm3(&x, 16, 96, 96, NOXF, FALSE, 0, 0, (IDWriteRenderingParams *)p, &m, &g));
    if (expect_mode >= 0) t_check((int)m == expect_mode, __FILE__, __LINE__, "explicit mode", "params mode %d -> %d (want %d)", (int)pm, (int)m, expect_mode);
    if (expect_grid >= 0) t_check((int)g == expect_grid, __FILE__, __LINE__, "explicit grid", "params grid %d -> %d (want %d)", (int)pg, (int)g, expect_grid);
    IDWriteRenderingParams3_Release(p);
    ffx_close(&x);
}
TEST(rm3_explicit_aliased) { explicit_mode_case(DWRITE_RENDERING_MODE1_ALIASED, DWRITE_GRID_FIT_MODE_ENABLED, 1, 2); }
TEST(rm3_explicit_gdi_classic) { explicit_mode_case(DWRITE_RENDERING_MODE1_GDI_CLASSIC, DWRITE_GRID_FIT_MODE_ENABLED, 2, 2); }
TEST(rm3_explicit_gdi_natural) { explicit_mode_case(DWRITE_RENDERING_MODE1_GDI_NATURAL, DWRITE_GRID_FIT_MODE_DISABLED, 3, 1); }
TEST(rm3_explicit_natural) { explicit_mode_case(DWRITE_RENDERING_MODE1_NATURAL, DWRITE_GRID_FIT_MODE_DISABLED, 4, 1); }
TEST(rm3_explicit_outline) { explicit_mode_case(DWRITE_RENDERING_MODE1_OUTLINE, DWRITE_GRID_FIT_MODE_DISABLED, 6, 1); }
TEST(rm3_explicit_default_mode_resolves) { explicit_mode_case(DWRITE_RENDERING_MODE1_DEFAULT, DWRITE_GRID_FIT_MODE_DEFAULT, -1, -1); }
TEST(rm3_explicit_grid_only) { explicit_mode_case(DWRITE_RENDERING_MODE1_DEFAULT, DWRITE_GRID_FIT_MODE_ENABLED, -1, 2); }
TEST(rm2_explicit_params_mode) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteRenderingParams3 *p = mkparams(DWRITE_RENDERING_MODE1_GDI_CLASSIC, DWRITE_GRID_FIT_MODE_ENABLED);
    if (!p) { t_skip("no rendering params"); ffx_close(&x); return; }
    DWRITE_RENDERING_MODE m; DWRITE_GRID_FIT_MODE g;
    CHECK_HR_OK(rm2(&x, 16, 96, 96, NOXF, FALSE, 0, 0, (IDWriteRenderingParams *)p, &m, &g));
    EXPECT_EQ(m, DWRITE_RENDERING_MODE_GDI_CLASSIC); EXPECT_EQ(g, DWRITE_GRID_FIT_MODE_ENABLED);
    IDWriteRenderingParams3_Release(p);
    ffx_close(&x);
}
