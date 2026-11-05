/* Argument validation of the four CreateCustomRenderingParams overloads.
 *
 * Every overload zeroes *params first and then fails with E_INVALIDARG (0x80070057) when any one of
 *     !(0 < gamma <= 256)   enhancedContrast < 0   grayscaleEnhancedContrast < 0   clearTypeLevel < 0
 *     pixelGeometry > 2     renderingMode > 6 (> 7 for DWRITE_RENDERING_MODE1)       gridFitMode > 2
 * holds.  There is NO upper limit for the contrast values or the ClearType level, and comparisons are plain IEEE compares,
 * so a NaN gamma is rejected (0 < NaN is false) while a NaN contrast compares "not less than 0" - what NaN does there is only
 * noted.  Enumerations are compared as unsigned values; negative inputs are only noted.
 */
#include "rp.h"

#define RP_FIELD_TABLE_TEST(N, tname, field, ...) \
TEST(rpv_v##N##_##tname) { \
    RP_NEED(N); static const float bad[] = { __VA_ARGS__ }; unsigned i; \
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) { rp_vals v = rp_base(); char w[48]; v.field = bad[i]; _snprintf(w, sizeof w, #field "=%g", (double)bad[i]); RP_REJECT(N, &v, w); } }

#define NEG_TINY (-FLT_MIN)
#define RP_NEG_TESTS(N) \
RP_FIELD_TABLE_TEST(N, gamma_not_positive, gamma, 0.0f, -1.0f, -0.5f, -FLT_MAX, NEG_TINY, -100.0f) \
RP_FIELD_TABLE_TEST(N, gamma_above_256, gamma, 256.00003f, 256.5f, 257.0f, 1000.0f, 65536.0f, 1e30f, FLT_MAX) \
RP_FIELD_TABLE_TEST(N, enhanced_contrast_negative, ec, -0.0001f, -0.5f, -1.0f, -100.0f, -FLT_MAX, NEG_TINY) \
RP_FIELD_TABLE_TEST(N, cleartype_level_negative, ct, -0.0001f, -0.5f, -1.0f, -100.0f, -FLT_MAX, NEG_TINY) \
TEST(rpv_v##N##_gamma_special_values) { \
    RP_NEED(N); rp_vals v = rp_base(); \
    v.gamma = rp_negzero(); RP_REJECT(N, &v, "gamma=-0.0");             /* 0 < -0.0 is false */ \
    v.gamma = rp_nan(); RP_REJECT(N, &v, "gamma=NaN");                  /* every NaN compare is false */ \
    v.gamma = rp_inf(); RP_REJECT(N, &v, "gamma=+inf"); \
    v.gamma = -rp_inf(); RP_REJECT(N, &v, "gamma=-inf"); } \
TEST(rpv_v##N##_negative_infinity_contrast_and_level) { \
    RP_NEED(N); rp_vals v = rp_base(); v.ec = -rp_inf(); RP_REJECT(N, &v, "ec=-inf"); v = rp_base(); v.ct = -rp_inf(); RP_REJECT(N, &v, "ct=-inf"); } \
TEST(rpv_v##N##_nan_contrast_and_level_noted) { \
    RP_NEED(N); rp_vals v = rp_base(); void *p = NULL; HRESULT hr; \
    v.ec = rp_nan(); hr = rp_make(N, &v, &p); t_note("v%d enhancedContrast=NaN -> %s", N, dw_hr_name(hr)); DW_RELEASE(p); \
    v = rp_base(); v.ct = rp_nan(); hr = rp_make(N, &v, &p); t_note("v%d clearTypeLevel=NaN -> %s", N, dw_hr_name(hr)); DW_RELEASE(p); \
    v = rp_base(); v.ec = rp_inf(); hr = rp_make(N, &v, &p); t_note("v%d enhancedContrast=+inf -> %s", N, dw_hr_name(hr)); DW_RELEASE(p); \
    v = rp_base(); v.ct = rp_inf(); hr = rp_make(N, &v, &p); t_note("v%d clearTypeLevel=+inf -> %s", N, dw_hr_name(hr)); DW_RELEASE(p); } \
TEST(rpv_v##N##_pixel_geometry_out_of_range) { \
    RP_NEED(N); static const int bad[] = { 3, 4, 5, 7, 8, 16, 100, 255, 256, 0x7fff, 0x7fffffff }; unsigned i; \
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) { rp_vals v = rp_base(); char w[48]; v.pg = bad[i]; _snprintf(w, sizeof w, "pixelGeometry=%d", bad[i]); RP_REJECT(N, &v, w); } } \
TEST(rpv_v##N##_pixel_geometry_high_bit_noted) { \
    RP_NEED(N); rp_vals v = rp_base(); void *p = NULL; v.pg = -1; HRESULT hr = rp_make(N, &v, &p); \
    t_note("v%d pixelGeometry=-1 -> %s", N, dw_hr_name(hr)); DW_RELEASE(p); \
    v.pg = (int)0x80000000u; hr = rp_make(N, &v, &p); t_note("v%d pixelGeometry=0x80000000 -> %s", N, dw_hr_name(hr)); DW_RELEASE(p); } \
TEST(rpv_v##N##_rendering_mode_out_of_range) { \
    RP_NEED(N); static const int bad[] = { 8, 9, 10, 16, 100, 255, 0x7fff, 0x7fffffff }; unsigned i; \
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) { rp_vals v = rp_base(); char w[48]; v.mode = bad[i]; _snprintf(w, sizeof w, "mode=%d", bad[i]); RP_REJECT(N, &v, w); } } \
TEST(rpv_v##N##_rendering_mode_high_bit_noted) { \
    RP_NEED(N); rp_vals v = rp_base(); void *p = NULL; v.mode = -1; HRESULT hr = rp_make(N, &v, &p); \
    t_note("v%d renderingMode=-1 -> %s", N, dw_hr_name(hr)); DW_RELEASE(p); } \
TEST(rpv_v##N##_failure_leaves_out_null_even_if_preset) { \
    RP_NEED(N); rp_vals v = rp_base(); v.gamma = -1.0f; void *p = RP_SENT; \
    /* the pointer is written NULL before validation: preset junk is never observed back */ \
    HRESULT hr = rp_make(N, &v, &p); CHECK_HR(hr, DW_E_INVALIDARG); CHECK_OUT_NULL(p); } \
TEST(rpv_v##N##_several_invalid_arguments_at_once) { \
    RP_NEED(N); rp_vals v = { -1.0f, -1.0f, -1.0f, -1.0f, 9, 99, 9 }; RP_REJECT(N, &v, "everything invalid"); } \
TEST(rpv_v##N##_valid_after_invalid_succeeds) { \
    RP_NEED(N); rp_vals bad = rp_base(); rp_vals good = rp_base(); bad.gamma = 0.0f; RP_REJECT(N, &bad, "gamma=0"); RP_ROUNDTRIP(N, &good); RP_REJECT(N, &bad, "gamma=0 again"); } \
TEST(rpv_v##N##_each_invalid_field_alone_with_extremes_elsewhere) { \
    RP_NEED(N); rp_vals v; \
    /* valid extremes in the other fields do not mask or cause the failure */ \
    v = rp_base(); v.ec = FLT_MAX; v.ct = 1e9f; v.gec = FLT_MAX; v.pg = 2; v.mode = MAXM(N); v.grid = 2; v.gamma = 0.0f; RP_REJECT(N, &v, "gamma=0 amid extremes"); \
    v = rp_base(); v.ec = FLT_MAX; v.ct = 1e9f; v.gec = FLT_MAX; v.pg = 2; v.mode = MAXM(N); v.grid = 2; v.pg = 3; RP_REJECT(N, &v, "pg=3 amid extremes"); \
    v = rp_base(); v.ec = FLT_MAX; v.ct = 1e9f; v.gec = FLT_MAX; v.pg = 2; v.mode = MAXM(N); v.grid = 2; v.mode = MAXM(N) + 1; RP_REJECT(N, &v, "mode+1 amid extremes"); \
    v = rp_base(); v.ec = FLT_MAX; v.ct = 1e9f; v.gec = FLT_MAX; v.pg = 2; v.mode = MAXM(N); v.grid = 2; v.ec = -1.0f; RP_REJECT(N, &v, "ec=-1 amid extremes"); \
    v = rp_base(); v.ec = FLT_MAX; v.ct = 1e9f; v.gec = FLT_MAX; v.pg = 2; v.mode = MAXM(N); v.grid = 2; v.ct = -1.0f; RP_REJECT(N, &v, "ct=-1 amid extremes"); } \
TEST(rpv_v##N##_upper_bounds_accepted) { \
    RP_NEED(N); rp_vals v = rp_base(); v.gamma = 256.0f; v.ec = FLT_MAX; v.gec = FLT_MAX; v.ct = 1e9f; v.pg = 2; v.mode = MAXM(N); v.grid = 2; RP_ROUNDTRIP(N, &v); } \
TEST(rpv_v##N##_lower_bounds_accepted) { \
    RP_NEED(N); rp_vals v = rp_base(); v.gamma = FLT_MIN; v.ec = 0.0f; v.gec = 0.0f; v.ct = 0.0f; v.pg = 0; v.mode = 0; v.grid = 0; RP_ROUNDTRIP(N, &v); } \
TEST(rpv_v##N##_cleartype_level_above_one_is_accepted) { \
    RP_NEED(N); rp_vals v = rp_base(); \
    /* documented as 0..1, but the implementation only rejects negative levels */ \
    v.ct = 1.5f; RP_ROUNDTRIP(N, &v); v.ct = 10.0f; RP_ROUNDTRIP(N, &v); }

#define MAXM(N) ((N) == 3 ? 7 : 6)

RP_NEG_TESTS(0)
RP_NEG_TESTS(1)
RP_NEG_TESTS(2)
RP_NEG_TESTS(3)

/* ---- grayscale contrast (overloads 1..3) ------------------------------------------------------------------------------- */
TEST(rpv_gec_negative_rejected) {
    int ver; static const float bad[] = { -0.0001f, -1.0f, -100.0f, -FLT_MAX, -FLT_MIN };
    for (ver = 1; ver <= 3; ver++) {
        unsigned i; if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) { rp_vals v = rp_base(); char w[48]; v.gec = bad[i]; _snprintf(w, sizeof w, "gec=%g", (double)bad[i]); RP_REJECT(ver, &v, w); }
        { rp_vals v = rp_base(); v.gec = -rp_inf(); RP_REJECT(ver, &v, "gec=-inf"); }
    }
}
TEST(rpv_gec_nan_noted) {
    int ver;
    for (ver = 1; ver <= 3; ver++) {
        rp_vals v = rp_base(); void *p = NULL; if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        v.gec = rp_nan(); HRESULT hr = rp_make(ver, &v, &p); t_note("v%d grayscaleEnhancedContrast=NaN -> %s", ver, dw_hr_name(hr)); DW_RELEASE(p);
    }
}
TEST(rpv_v0_ignores_grayscale_field) {
    RP_NEED(0);
    /* the original overload has no grayscale argument: a negative value in the unused field of the test struct changes nothing */
    rp_vals v = rp_base(); v.gec = -5.0f; RP_ROUNDTRIP(0, &v);
}

/* ---- grid fit mode (overloads 2, 3) ------------------------------------------------------------------------------------- */
TEST(rpv_grid_fit_out_of_range) {
    int ver; static const int bad[] = { 3, 4, 5, 8, 100, 0x7fffffff };
    for (ver = 2; ver <= 3; ver++) {
        unsigned i; if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) { rp_vals v = rp_base(); char w[48]; v.grid = bad[i]; _snprintf(w, sizeof w, "grid=%d", bad[i]); RP_REJECT(ver, &v, w); }
    }
}
TEST(rpv_grid_fit_negative_noted) {
    int ver;
    for (ver = 2; ver <= 3; ver++) {
        rp_vals v = rp_base(); void *p = NULL; if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        v.grid = -1; HRESULT hr = rp_make(ver, &v, &p); t_note("v%d gridFitMode=-1 -> %s", ver, dw_hr_name(hr)); DW_RELEASE(p);
    }
}

/* ---- rendering mode value 7 differs between the overloads ------------------------------------------------------------------ */
TEST(rpv_mode_7_only_valid_for_mode1_overload) {
    int ver;
    for (ver = 0; ver <= 3; ver++) {
        rp_vals v = rp_base(); if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        v.mode = 7;
        /* DWRITE_RENDERING_MODE ends at OUTLINE (6); NATURAL_SYMMETRIC_DOWNSAMPLED exists only in DWRITE_RENDERING_MODE1 */
        if (ver == 3) RP_ROUNDTRIP(ver, &v); else RP_REJECT(ver, &v, "mode=7");
    }
}
TEST(rpv_mode_8_rejected_everywhere) {
    int ver;
    for (ver = 0; ver <= 3; ver++) { rp_vals v = rp_base(); if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue; v.mode = 8; RP_REJECT(ver, &v, "mode=8"); }
}
TEST(rpv_mode_6_accepted_everywhere) {
    int ver;
    for (ver = 0; ver <= 3; ver++) { rp_vals v = rp_base(); if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue; v.mode = 6; RP_ROUNDTRIP(ver, &v); }
}

/* ---- isolated factories validate the same way -------------------------------------------------------------------------------- */
TEST(rpv_isolated_factory_validates_identically) {
    int ver;
    for (ver = 0; ver <= 3; ver++) {
        void *p = RP_SENT; rp_vals v = rp_base(); HRESULT hr;
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_ISOLATED, ver)) continue;
        v.gamma = 0.0f; hr = rp_make_t(DWRITE_FACTORY_TYPE_ISOLATED, ver, &v, &p);
        CHECK_HR(hr, DW_E_INVALIDARG); CHECK_OUT_NULL(p); DW_RELEASE(p);
        v = rp_base(); v.pg = 3; hr = rp_make_t(DWRITE_FACTORY_TYPE_ISOLATED, ver, &v, &p);
        CHECK_HR(hr, DW_E_INVALIDARG); CHECK_OUT_NULL(p); DW_RELEASE(p);
    }
}
TEST(rpv_failures_do_not_leak_objects) {
    int ver, i;
    /* 200 failing calls per overload leave nothing to release: *out stays NULL every time */
    for (ver = 0; ver <= 3; ver++) {
        rp_vals v = rp_base(); void *p = NULL; if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        v.ec = -1.0f;
        for (i = 0; i < 200; i++) { HRESULT hr = rp_make(ver, &v, &p); if (hr != DW_E_INVALIDARG || p) { CHECKF(0, "v%d iteration %d: %s %p", ver, i, dw_hr_name(hr), p); DW_RELEASE(p); break; } }
    }
}
TEST(rpv_hresult_value_is_E_INVALIDARG) {
    EXPECT_EQ(DW_E_INVALIDARG, E_INVALIDARG);
    EXPECT_EQ((unsigned)E_INVALIDARG, 0x80070057u);
}
