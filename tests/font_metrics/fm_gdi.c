/* IDWriteFontFace::GetGdiCompatibleMetrics / IDWriteFontFace1::GetGdiCompatibleMetrics.
 *
 * Algorithm (GetGdiCompatibleMetrics): ppem = emSize * pixelsPerDip * scale(transform); designUnitsPerPixel = upm / ppem;
 * every vertical/box/script metric v becomes round(round(v / dpp) * dpp) (round half up, stored back into 16 bits); designUnitsPerEm
 * and hasTypographicMetrics are copied; when ppem or dpp is ~0 (|x| < 2^-16) the unrounded metrics are returned. ascent/descent
 * come from a VDMX table when one covers the integral ppem (1 < ppem < 255), which the synthetic fonts do not have. */
#include "fm.h"
#include <math.h>

static int setup(fm_font *F, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim) {
    if (!fm_open(F, s, sim)) return 0;
    if (!F->f1) { t_skip("IDWriteFontFace1 unavailable"); return 0; }
    return 1;
}
static HRESULT gdi(fm_font *F, float em, float ppd, const DWRITE_MATRIX *m, DWRITE_FONT_METRICS1 *o) {
    memset(o, 0xCD, sizeof *o);
    return IDWriteFontFace1_GetGdiCompatibleMetrics(F->f1, em, ppd, m, o);
}
static void run_one(fm_font *F, float em, float ppd, const DWRITE_MATRIX *m, const char *lab, unsigned skip) {
    DWRITE_FONT_METRICS1 base, got, want; HRESULT hr;
    IDWriteFontFace1_GetMetrics(F->f1, &base);
    hr = gdi(F, em, ppd, m, &got);
    if (hr != S_OK) { t_check(0, __FILE__, __LINE__, "GetGdiCompatibleMetrics", "%s hr=%s", lab, dw_hr_name(hr)); return; }
    fm_gdi_expect(&base, em, ppd, m, &want);
    fm_cmp_(__FILE__, __LINE__, &got, &want, skip, lab);
}
#define SETUP(F, spec, sim) fm_font F; memset(&F, 0, sizeof F); if (!setup(&F, &(spec), (sim))) { fm_close(&F); return; }

TEST(fmg_invalid_em_size) {
    static const float v[] = { 0.0f, -0.0f, -1.0f, -0.001f, -1000.0f, -INFINITY };
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    unsigned i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        DWRITE_FONT_METRICS1 m; HRESULT hr = gdi(&F, v[i], 1.0f, NULL, &m);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "E_INVALIDARG", "em %g -> %s", v[i], dw_hr_name(hr));
        /* the output is cleared before validation */
        t_check(m.designUnitsPerEm == 0 && m.ascent == 0 && m.hasTypographicMetrics == 0 && m.subscriptSizeY == 0, __FILE__, __LINE__, "zeroed on failure", "em %g", v[i]);
    }
    fm_close(&F);
}
TEST(fmg_invalid_pixels_per_dip) {
    static const float v[] = { 0.0f, -0.0f, -1.0f, -0.5f, -INFINITY };
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    unsigned i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        DWRITE_FONT_METRICS1 m; HRESULT hr = gdi(&F, 12.0f, v[i], NULL, &m);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "E_INVALIDARG", "ppd %g -> %s", v[i], dw_hr_name(hr));
        t_check(m.designUnitsPerEm == 0 && m.ascent == 0, __FILE__, __LINE__, "zeroed on failure", "ppd %g", v[i]);
    }
    fm_close(&F);
}
TEST(fmg_nan_matrix_element_is_invalid) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    int e;
    for (e = 0; e < 6; e++) {
        DWRITE_MATRIX m = { 1, 0, 0, 1, 0, 0 }; float *p = (float *)&m; DWRITE_FONT_METRICS1 o;
        p[e] = NAN;
        HRESULT hr = gdi(&F, 12.0f, 1.0f, &m, &o);
        /* every one of the six elements (including dx/dy) is checked for NaN */
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "E_INVALIDARG", "NaN in element %d -> %s", e, dw_hr_name(hr));
        t_check(o.designUnitsPerEm == 0 && o.ascent == 0, __FILE__, __LINE__, "zeroed on failure", "element %d", e);
    }
    fm_close(&F);
}
TEST(fmg_v0_invalid_args) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS m; HRESULT hr;
    memset(&m, 0xCD, sizeof m); hr = IDWriteFontFace_GetGdiCompatibleMetrics(F.face, 0.0f, 1.0f, NULL, &m);
    CHECK_HR(hr, E_INVALIDARG); CHECKF(m.designUnitsPerEm == 0 && m.ascent == 0 && m.descent == 0 && m.strikethroughThickness == 0, "v0 metrics not cleared");
    memset(&m, 0xCD, sizeof m); hr = IDWriteFontFace_GetGdiCompatibleMetrics(F.face, 12.0f, 0.0f, NULL, &m);
    CHECK_HR(hr, E_INVALIDARG); CHECKF(m.designUnitsPerEm == 0 && m.ascent == 0, "v0 metrics not cleared");
    {
        DWRITE_MATRIX mx = { NAN, 0, 0, 1, 0, 0 };
        memset(&m, 0xCD, sizeof m); hr = IDWriteFontFace_GetGdiCompatibleMetrics(F.face, 12.0f, 1.0f, &mx, &m);
        CHECK_HR(hr, E_INVALIDARG); CHECKF(m.designUnitsPerEm == 0, "v0 metrics not cleared");
    }
    fm_close(&F);
}
TEST(fmg_valid_returns_s_ok_and_fills_everything) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    CHECK_HR(gdi(&F, 16.0f, 1.0f, NULL, &m), S_OK);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    CHECK(m.ascent != 0xCDCD && m.descent != 0xCDCD && (UINT16)m.lineGap != 0xCDCD);
    fm_close(&F);
}
TEST(fmg_size_ladder_ppd_1) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 96; px++) { char l[40]; snprintf(l, sizeof l, "em %d ppd 1", px); run_one(&F, (float)px, 1.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_size_ladder_ppd_1_25) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 96; px++) { char l[40]; snprintf(l, sizeof l, "em %d ppd 1.25", px); run_one(&F, (float)px, 1.25f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_size_ladder_ppd_1_5) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 96; px++) { char l[40]; snprintf(l, sizeof l, "em %d ppd 1.5", px); run_one(&F, (float)px, 1.5f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_size_ladder_ppd_2) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 96; px++) { char l[40]; snprintf(l, sizeof l, "em %d ppd 2", px); run_one(&F, (float)px, 2.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_fractional_sizes) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    static const float em[] = { 6.25f, 8.5f, 9.75f, 10.4f, 10.5f, 10.6f, 11.5f, 12.49f, 12.5f, 13.333f, 14.4f, 15.999f, 16.001f, 17.5f, 21.333f, 0.5f, 1.0f, 2.0f, 3.5f, 100.5f, 255.0f, 255.5f, 300.0f, 1000.0f, 2048.0f, 5000.0f, 100000.0f };
    unsigned i;
    for (i = 0; i < sizeof em / sizeof em[0]; i++) { char l[40]; snprintf(l, sizeof l, "em %g", em[i]); run_one(&F, em[i], 1.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_fractional_ppd) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    static const float ppd[] = { 0.5f, 0.75f, 1.0f, 1.1f, 1.25f, 1.33f, 1.5f, 1.75f, 2.0f, 2.25f, 2.5f, 3.0f, 4.0f };
    unsigned i; int px;
    for (i = 0; i < sizeof ppd / sizeof ppd[0]; i++)
        for (px = 7; px <= 40; px += 3) { char l[48]; snprintf(l, sizeof l, "em %d ppd %g", px, ppd[i]); run_one(&F, (float)px, ppd[i], NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_tiny_size_returns_unrounded) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_FONT_METRICS1 base, m;
    IDWriteFontFace1_GetMetrics(F.f1, &base);
    /* ppem below 2^-16 counts as zero: the metrics come back exactly as stored */
    CHECK_HR(gdi(&F, 1e-7f, 1.0f, NULL, &m), S_OK);
    FM_CMP(&m, &base, 0, "tiny em");
    CHECK_HR(gdi(&F, 1.0f, 1e-7f, NULL, &m), S_OK);
    FM_CMP(&m, &base, 0, "tiny ppd");
    fm_close(&F);
}
TEST(fmg_design_size_is_identity) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_FONT_METRICS1 base, m;
    IDWriteFontFace1_GetMetrics(F.f1, &base);
    /* ppem == upm gives dpp 1.0: every value is already on the grid */
    CHECK_HR(gdi(&F, 2048.0f, 1.0f, NULL, &m), S_OK); FM_CMP(&m, &base, 0, "em=upm");
    CHECK_HR(gdi(&F, 1024.0f, 2.0f, NULL, &m), S_OK); FM_CMP(&m, &base, 0, "em=upm/2 ppd 2");
    CHECK_HR(gdi(&F, 4096.0f, 1.0f, NULL, &m), S_OK); FM_CMP(&m, &base, 0, "em=2upm");
    fm_close(&F);
}
TEST(fmg_copied_fields) {
    ttf_spec s; ttf_default_spec(&s); s.fs_selection |= 0x80; SETUP(F, s, 0);
    int px;
    for (px = 6; px < 60; px += 4) {
        DWRITE_FONT_METRICS1 m; CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &m), S_OK);
        EXPECT_EQ(m.designUnitsPerEm, 2048); CHECK(m.hasTypographicMetrics);
    }
    fm_close(&F);
}
TEST(fmg_half_pixel_rounds_up) {
    /* upm 2048, ppem 16: one pixel = 128 design units. Values at exactly half a pixel round UP (toward +infinity). */
    static const struct { int v, want; } asc[] = { { 64, 128 }, { 192, 256 }, { 320, 384 }, { 63, 0 }, { 65, 128 }, { 128, 128 }, { 1, 0 }, { 191, 128 }, { 193, 256 } };
    unsigned i;
    for (i = 0; i < sizeof asc / sizeof asc[0]; i++) {
        ttf_spec s; ttf_default_spec(&s); s.win_asc = asc[i].v; SETUP(F, s, 0);
        DWRITE_FONT_METRICS1 m; CHECK_HR(gdi(&F, 16.0f, 1.0f, NULL, &m), S_OK);
        t_check(m.ascent == asc[i].want, __FILE__, __LINE__, "ascent", "win ascent %d got %d want %d", asc[i].v, m.ascent, asc[i].want);
        fm_close(&F);
    }
}
TEST(fmg_negative_half_rounds_toward_positive) {
    /* underline position is signed: -0.5 px -> 0, -1.5 px -> -1 px, -2.5 px -> -2 px, -0.51 -> -1 */
    static const struct { int v, want; } t[] = { { -64, 0 }, { -192, -128 }, { -320, -256 }, { -65, -128 }, { -63, 0 }, { -128, -128 }, { 64, 128 }, { 0, 0 } };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ttf_spec s; ttf_default_spec(&s); s.underline_pos = t[i].v; SETUP(F, s, 0);
        DWRITE_FONT_METRICS1 m; CHECK_HR(gdi(&F, 16.0f, 1.0f, NULL, &m), S_OK);
        t_check(m.underlinePosition == t[i].want, __FILE__, __LINE__, "underlinePosition", "post %d got %d want %d", t[i].v, m.underlinePosition, t[i].want);
        fm_close(&F);
    }
}
TEST(fmg_thickness_can_round_to_zero) {
    ttf_spec s; ttf_default_spec(&s); s.underline_thick = 40; s.strike_size = 40; SETUP(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    /* 40 / 128 = 0.31 px -> 0 : GDI-compatible metrics do not enforce a one pixel minimum */
    CHECK_HR(gdi(&F, 16.0f, 1.0f, NULL, &m), S_OK);
    EXPECT_EQ(m.underlineThickness, 0); EXPECT_EQ(m.strikethroughThickness, 0);
    fm_close(&F);
}
TEST(fmg_values_are_pixel_multiples) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    int px;
    for (px = 8; px <= 72; px += 2) {
        DWRITE_FONT_METRICS1 m; CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &m), S_OK);
        /* for an integral ppem every rounded value, divided by designUnitsPerPixel, is an integer up to the +-0.5 design unit rounding of the result */
        float dpp = 2048.0f / px; int i;
        for (i = FM_ASC; i <= FM_SUPSY; i++) {
            float v = (float)fm_field_get(&m, i) / dpp, r = floorf(v + 0.5f);
            t_check(fabsf(v - r) <= 0.5f / dpp + 1e-4f, __FILE__, __LINE__, fm_field_name(i), "em %d value %d = %g px", px, fm_field_get(&m, i), v);
        }
    }
    fm_close(&F);
}
TEST(fmg_ppd_and_em_are_interchangeable) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    static const struct { float e1, p1, e2, p2; } t[] = { { 12, 2, 24, 1 }, { 16, 1.5f, 24, 1 }, { 10, 2.5f, 25, 1 }, { 8, 4, 32, 1 }, { 11, 1.25f, 13.75f, 1 }, { 20, 0.5f, 10, 1 } };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        DWRITE_FONT_METRICS1 a, b; char l[48];
        CHECK_HR(gdi(&F, t[i].e1, t[i].p1, NULL, &a), S_OK); CHECK_HR(gdi(&F, t[i].e2, t[i].p2, NULL, &b), S_OK);
        snprintf(l, sizeof l, "%g*%g vs %g*%g", t[i].e1, t[i].p1, t[i].e2, t[i].p2);
        FM_CMP(&a, &b, 0, l);          /* only ppem = em * ppd matters */
    }
    fm_close(&F);
}
TEST(fmg_result_independent_of_previous_calls) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_FONT_METRICS1 a, b, base1, base2;
    IDWriteFontFace1_GetMetrics(F.f1, &base1);
    CHECK_HR(gdi(&F, 13.0f, 1.0f, NULL, &a), S_OK);
    CHECK_HR(gdi(&F, 40.0f, 1.0f, NULL, &b), S_OK);
    CHECK_HR(gdi(&F, 13.0f, 1.0f, NULL, &b), S_OK);
    FM_CMP(&a, &b, 0, "same inputs, same outputs");
    IDWriteFontFace1_GetMetrics(F.f1, &base2); CHECK(!memcmp(&base1, &base2, sizeof base1));
    fm_close(&F);
}

/* ---- transforms -------------------------------------------------------------------------------------------------------------------- */
typedef struct { const char *n; DWRITE_MATRIX m; } mrow;
static const mrow mats[] = {
    { "identity", { 1, 0, 0, 1, 0, 0 } },
    { "scale2", { 2, 0, 0, 2, 0, 0 } },
    { "scale0.5", { 0.5f, 0, 0, 0.5f, 0, 0 } },
    { "scale3", { 3, 0, 0, 3, 0, 0 } },
    { "scale-1", { -1, 0, 0, -1, 0, 0 } },
    { "flipY", { 1, 0, 0, -1, 0, 0 } },
    { "flipX", { -1, 0, 0, 1, 0, 0 } },
    { "rot90", { 0, 1, -1, 0, 0, 0 } },
    { "rot-90", { 0, -1, 1, 0, 0, 0 } },
    { "rot180", { -1, 0, 0, -1, 0, 0 } },
    { "rot90x2", { 0, 2, -2, 0, 0, 0 } },
    { "rot45", { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 } },
    { "rot30", { 0.8660254f, 0.5f, -0.5f, 0.8660254f, 0, 0 } },
    { "rot45x2", { 1.41421356f, 1.41421356f, -1.41421356f, 1.41421356f, 0, 0 } },
    { "nonuniform 2,1", { 2, 0, 0, 1, 0, 0 } },
    { "nonuniform 1,3", { 1, 0, 0, 3, 0, 0 } },
    { "nonuniform 4,0.25", { 4, 0, 0, 0.25f, 0, 0 } },
    { "nonuniform rot90 2,1", { 0, 2, -1, 0, 0, 0 } },
    { "shear x", { 1, 0, 0.5f, 1, 0, 0 } },
    { "shear y", { 1, 0.5f, 0, 1, 0, 0 } },
    { "skew both", { 1, 0.3f, 0.4f, 1, 0, 0 } },
    { "translate only", { 1, 0, 0, 1, 100, -50 } },
    { "scale2+translate", { 2, 0, 0, 2, 7, 9 } },
    { "zero", { 0, 0, 0, 0, 0, 0 } },
    { "zero scale, translate", { 0, 0, 0, 0, 5, 5 } },
    { "tiny scale", { 1e-7f, 0, 0, 1e-7f, 0, 0 } },
    { "m12 tiny", { 1, 1e-6f, 0, 1, 0, 0 } },
    { "diag 1,1.00001", { 1, 0, 0, 1.00001f, 0, 0 } },
    { "diag 1,1.001", { 1, 0, 0, 1.001f, 0, 0 } },
};
TEST(fmg_transform_table_em_13) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    unsigned i;
    for (i = 0; i < sizeof mats / sizeof mats[0]; i++) run_one(&F, 13.0f, 1.0f, &mats[i].m, mats[i].n, 0);
    fm_close(&F);
}
TEST(fmg_transform_table_em_20_ppd_1_5) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    unsigned i;
    for (i = 0; i < sizeof mats / sizeof mats[0]; i++) run_one(&F, 20.0f, 1.5f, &mats[i].m, mats[i].n, 0);
    fm_close(&F);
}
TEST(fmg_transform_table_em_9) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    unsigned i;
    for (i = 0; i < sizeof mats / sizeof mats[0]; i++) run_one(&F, 9.0f, 1.0f, &mats[i].m, mats[i].n, 0);
    fm_close(&F);
}
TEST(fmg_identity_matrix_equals_null) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 }; int px;
    for (px = 6; px <= 48; px += 3) {
        DWRITE_FONT_METRICS1 a, b; char l[32];
        CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &a), S_OK); CHECK_HR(gdi(&F, (float)px, 1.0f, &id, &b), S_OK);
        snprintf(l, sizeof l, "em %d", px); FM_CMP(&a, &b, 0, l);
    }
    fm_close(&F);
}
TEST(fmg_scale_matrix_multiplies_em) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_MATRIX m2 = { 2, 0, 0, 2, 0, 0 }, m3 = { 3, 0, 0, 3, 0, 0 }, mh = { 0.5f, 0, 0, 0.5f, 0, 0 }; int px;
    for (px = 6; px <= 40; px += 2) {
        DWRITE_FONT_METRICS1 a, b; char l[48];
        CHECK_HR(gdi(&F, (float)px, 1.0f, &m2, &a), S_OK); CHECK_HR(gdi(&F, (float)(px * 2), 1.0f, NULL, &b), S_OK);
        snprintf(l, sizeof l, "scale2 em %d", px); FM_CMP(&a, &b, 0, l);
        CHECK_HR(gdi(&F, (float)px, 1.0f, &m3, &a), S_OK); CHECK_HR(gdi(&F, (float)(px * 3), 1.0f, NULL, &b), S_OK);
        snprintf(l, sizeof l, "scale3 em %d", px); FM_CMP(&a, &b, 0, l);
        CHECK_HR(gdi(&F, (float)(px * 2), 1.0f, &mh, &a), S_OK); CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &b), S_OK);
        snprintf(l, sizeof l, "half em %d", px); FM_CMP(&a, &b, 0, l);
    }
    fm_close(&F);
}
TEST(fmg_rotation_does_not_change_metrics) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_MATRIX r90 = { 0, 1, -1, 0, 0, 0 }, r270 = { 0, -1, 1, 0, 0, 0 }, r180 = { -1, 0, 0, -1, 0, 0 }; int px;
    for (px = 6; px <= 40; px += 2) {
        DWRITE_FONT_METRICS1 a, b; char l[48];
        CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &a), S_OK);
        CHECK_HR(gdi(&F, (float)px, 1.0f, &r90, &b), S_OK); snprintf(l, sizeof l, "rot90 em %d", px); FM_CMP(&a, &b, 0, l);
        CHECK_HR(gdi(&F, (float)px, 1.0f, &r270, &b), S_OK); snprintf(l, sizeof l, "rot270 em %d", px); FM_CMP(&a, &b, 0, l);
        CHECK_HR(gdi(&F, (float)px, 1.0f, &r180, &b), S_OK); snprintf(l, sizeof l, "rot180 em %d", px); FM_CMP(&a, &b, 0, l);
    }
    fm_close(&F);
}
TEST(fmg_translation_is_ignored) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_MATRIX t = { 1, 0, 0, 1, 1234.5f, -77.25f }; int px;
    for (px = 6; px <= 40; px += 5) {
        DWRITE_FONT_METRICS1 a, b; char l[32];
        CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &a), S_OK); CHECK_HR(gdi(&F, (float)px, 1.0f, &t, &b), S_OK);
        snprintf(l, sizeof l, "em %d", px); FM_CMP(&a, &b, 0, l);
    }
    fm_close(&F);
}
TEST(fmg_zero_matrix_returns_stored_metrics) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_MATRIX z = { 0, 0, 0, 0, 0, 0 }; DWRITE_FONT_METRICS1 base, m;
    IDWriteFontFace1_GetMetrics(F.f1, &base);
    /* a degenerate transform yields ppem 0: S_OK with the unrounded metrics (no failure) */
    CHECK_HR(gdi(&F, 12.0f, 1.0f, &z, &m), S_OK);
    FM_CMP(&m, &base, 0, "zero matrix");
    fm_close(&F);
}
TEST(fmg_nonuniform_scale_uses_second_row) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    /* unequal axis scales: the scale taken is the length of the transformed (0,1) vector, i.e. hypot(m21, m22) */
    DWRITE_MATRIX a = { 2, 0, 0, 1, 0, 0 }, b = { 1, 0, 0, 3, 0, 0 }, c = { 5, 0, 0, 2, 0, 0 };
    DWRITE_FONT_METRICS1 x, y;
    CHECK_HR(gdi(&F, 12.0f, 1.0f, &a, &x), S_OK); CHECK_HR(gdi(&F, 12.0f, 1.0f, NULL, &y), S_OK); FM_CMP(&x, &y, 0, "2,1 acts as 1");
    CHECK_HR(gdi(&F, 12.0f, 1.0f, &b, &x), S_OK); CHECK_HR(gdi(&F, 36.0f, 1.0f, NULL, &y), S_OK); FM_CMP(&x, &y, 0, "1,3 acts as 3");
    CHECK_HR(gdi(&F, 12.0f, 1.0f, &c, &x), S_OK); CHECK_HR(gdi(&F, 24.0f, 1.0f, NULL, &y), S_OK); FM_CMP(&x, &y, 0, "5,2 acts as 2");
    fm_close(&F);
}
TEST(fmg_scale_factor_mirror_values) {
    /* sanity of the mirror itself against the documented cases */
    DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 }, r = { 0, 1, -1, 0, 0, 0 }, n = { 2, 0, 0, 1, 0, 0 };
    CHECK_FLOAT_EQ(fm_scale(NULL), 1.0f); CHECK_FLOAT_EQ(fm_scale(&id), 1.0f); CHECK_FLOAT_EQ(fm_scale(&r), 1.0f); CHECK_FLOAT_EQ(fm_scale(&n), 1.0f);
}

/* ---- other fonts / simulations ------------------------------------------------------------------------------------------------------- */
TEST(fmg_upm_1000_font) {
    ttf_spec s; fm_spec_upm(&s, 1000); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 60; px += 3) { char l[40]; snprintf(l, sizeof l, "upm1000 em %d", px); run_one(&F, (float)px, 1.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_upm_1024_font) {
    ttf_spec s; fm_spec_upm(&s, 1024); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 60; px += 3) { char l[40]; snprintf(l, sizeof l, "upm1024 em %d", px); run_one(&F, (float)px, 1.25f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_upm_4096_font) {
    ttf_spec s; fm_spec_upm(&s, 4096); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 60; px += 3) { char l[40]; snprintf(l, sizeof l, "upm4096 em %d", px); run_one(&F, (float)px, 1.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_upm_16_font) {
    ttf_spec s; fm_spec_upm(&s, 16); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 60; px += 3) { char l[40]; snprintf(l, sizeof l, "upm16 em %d", px); run_one(&F, (float)px, 1.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_typo_metrics_font) {
    ttf_spec s; ttf_default_spec(&s); s.fs_selection |= 0x80; SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 60; px += 3) { char l[40]; snprintf(l, sizeof l, "typo em %d", px); run_one(&F, (float)px, 1.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_oblique_face_rounds_the_sheared_box) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    int px;
    for (px = 6; px <= 60; px += 3) { char l[40]; snprintf(l, sizeof l, "oblique em %d", px); run_one(&F, (float)px, 1.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_bold_face_same_as_regular) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, DWRITE_FONT_SIMULATIONS_BOLD);
    fm_font G; memset(&G, 0, sizeof G);
    if (!setup(&G, &s, 0)) { fm_close(&G); fm_close(&F); return; }
    int px;
    for (px = 6; px <= 48; px += 6) {
        DWRITE_FONT_METRICS1 a, b; char l[32];
        CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &a), S_OK); CHECK_HR(gdi(&G, (float)px, 1.0f, NULL, &b), S_OK);
        snprintf(l, sizeof l, "bold em %d", px); FM_CMP(&a, &b, 0, l);
    }
    fm_close(&G); fm_close(&F);
}
TEST(fmg_no_os2_font) {
    ttf_spec s; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2; SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 60; px += 6) { char l[40]; snprintf(l, sizeof l, "noos2 em %d", px); run_one(&F, (float)px, 1.0f, NULL, l, 0); }
    fm_close(&F);
}
TEST(fmg_v0_equals_v1_prefix_over_sizes) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 96; px += 5) {
        DWRITE_FONT_METRICS v0; DWRITE_FONT_METRICS1 v1; char l[32];
        memset(&v0, 0xCD, sizeof v0);
        CHECK_HR(IDWriteFontFace_GetGdiCompatibleMetrics(F.face, (float)px, 1.0f, NULL, &v0), S_OK);
        CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &v1), S_OK);
        snprintf(l, sizeof l, "em %d", px);
        t_check(v0.designUnitsPerEm == v1.designUnitsPerEm && v0.ascent == v1.ascent && v0.descent == v1.descent && v0.lineGap == v1.lineGap && v0.capHeight == v1.capHeight &&
                v0.xHeight == v1.xHeight && v0.underlinePosition == v1.underlinePosition && v0.underlineThickness == v1.underlineThickness &&
                v0.strikethroughPosition == v1.strikethroughPosition && v0.strikethroughThickness == v1.strikethroughThickness, __FILE__, __LINE__, "v0 == v1 prefix", "%s", l);
    }
    fm_close(&F);
}
TEST(fmg_ascent_descent_relation) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    int px;
    for (px = 6; px <= 96; px++) {
        DWRITE_FONT_METRICS1 m, base; float dpp = 2048.0f / px;
        IDWriteFontFace1_GetMetrics(F.f1, &base);
        CHECK_HR(gdi(&F, (float)px, 1.0f, NULL, &m), S_OK);
        /* rounding moves a value by at most half a pixel (+1 unit for the second rounding to integer design units) */
        t_check(fabsf((float)m.ascent - base.ascent) <= dpp / 2 + 1, __FILE__, __LINE__, "ascent drift", "em %d: %d vs %d", px, m.ascent, base.ascent);
        t_check(fabsf((float)m.descent - base.descent) <= dpp / 2 + 1, __FILE__, __LINE__, "descent drift", "em %d: %d vs %d", px, m.descent, base.descent);
    }
    fm_close(&F);
}
