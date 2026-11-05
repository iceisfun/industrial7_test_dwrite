/* CreateCustomRenderingParams boundary matrix over IDWriteFactory/1/2/3 and the getters of IDWriteRenderingParams..3.
 *
 * Facts (every overload writes *params = NULL first):
 *   E_INVALIDARG unless 0 < gamma <= 256, enhancedContrast >= 0, grayscaleEnhancedContrast >= 0, clearTypeLevel >= 0, pixelGeometry <= 2,
 *   renderingMode <= 6 (<= 7 for the DWRITE_RENDERING_MODE1 overload), gridFitMode <= 2.  There is no upper limit on the contrast values or
 *   on clearTypeLevel.  The legacy getter GetRenderingMode reports the downsampled mode (7) as NATURAL_SYMMETRIC (5); IDWriteRenderingParams
 *   objects created before IDWriteRenderingParams1 report a grayscale enhanced contrast of 1.0 only through the v0 overload.
 */
#include "rr.h"

#define RT(n) TEST(r2_rendering_modes_##n)

typedef struct { float gamma, ec, gec, ct; int pg, mode, grid; } pv;
static pv base(void) { pv v = { 2.2f, 0.5f, 0.25f, 0.75f, 1, 0, 0 }; return v; }

static HRESULT make(int ver, const pv *v, void **out) {
    void *f = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver);
    HRESULT hr;
    *out = (void *)(ULONG_PTR)0x1000;
    if (!f) { *out = NULL; return RR_UNSUP; }
    switch (ver) {
    case 0: hr = IDWriteFactory_CreateCustomRenderingParams((IDWriteFactory *)f, v->gamma, v->ec, v->ct, (DWRITE_PIXEL_GEOMETRY)v->pg, (DWRITE_RENDERING_MODE)v->mode, (IDWriteRenderingParams **)out); break;
    case 1: hr = IDWriteFactory1_CreateCustomRenderingParams((IDWriteFactory1 *)f, v->gamma, v->ec, v->gec, v->ct, (DWRITE_PIXEL_GEOMETRY)v->pg, (DWRITE_RENDERING_MODE)v->mode, (IDWriteRenderingParams1 **)out); break;
    case 2: hr = IDWriteFactory2_CreateCustomRenderingParams((IDWriteFactory2 *)f, v->gamma, v->ec, v->gec, v->ct, (DWRITE_PIXEL_GEOMETRY)v->pg, (DWRITE_RENDERING_MODE)v->mode, (DWRITE_GRID_FIT_MODE)v->grid, (IDWriteRenderingParams2 **)out); break;
    default: hr = IDWriteFactory3_CreateCustomRenderingParams((IDWriteFactory3 *)f, v->gamma, v->ec, v->gec, v->ct, (DWRITE_PIXEL_GEOMETRY)v->pg, (DWRITE_RENDERING_MODE1)v->mode, (DWRITE_GRID_FIT_MODE)v->grid, (IDWriteRenderingParams3 **)out); break;
    }
    return hr;
}
/* ver 0..3; expect 1 = accepted, 0 = E_INVALIDARG with *out == NULL */
static void expect(int ver, const pv *v, int ok, const char *what) {
    void *p; HRESULT hr = make(ver, v, &p);
    if (hr == RR_UNSUP) return;
    if (ok) t_check(hr == S_OK && p && p != (void *)(ULONG_PTR)0x1000, __FILE__, __LINE__, "accepted", "v%d %s: %s", ver, what, dw_hr_name(hr));
    else t_check(hr == RR_INVALIDARG && p == NULL, __FILE__, __LINE__, "rejected", "v%d %s: %s out %p", ver, what, dw_hr_name(hr), p);
    if (SUCCEEDED(hr) && p && p != (void *)(ULONG_PTR)0x1000) IUnknown_Release((IUnknown *)p);
}
/* same expectation on every overload that has the field */
static void all(const pv *v, int ok, const char *what, int first_ver) { int ver; for (ver = first_ver; ver <= 3; ver++) expect(ver, v, ok, what); }

RT(params_gamma_boundaries) {
    static const struct { float g; int ok; } c[] = { { 0.0f, 0 }, { -0.0f, 0 }, { -1.0f, 0 }, { 1e-30f, 1 }, { 1.4e-45f, 1 }, { 0.5f, 1 }, { 1.0f, 1 }, { 2.2f, 1 }, { 255.99f, 1 }, { 256.0f, 1 }, { 256.01f, 0 }, { 257.0f, 0 }, { 1e30f, 0 }, { -1e30f, 0 } };
    int i;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) { pv v = base(); v.gamma = c[i].g; all(&v, c[i].ok, "gamma", 0); }
}
RT(params_enhanced_contrast_boundaries) {
    static const struct { float e; int ok; } c[] = { { 0.0f, 1 }, { -0.0f, 1 }, { 1e-30f, 1 }, { 1.0f, 1 }, { 100.0f, 1 }, { 1e30f, 1 }, { -1e-30f, 0 }, { -0.001f, 0 }, { -1.0f, 0 }, { -1e30f, 0 } };
    int i;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) { pv v = base(); v.ec = c[i].e; all(&v, c[i].ok, "enhancedContrast", 0); }
}
RT(params_grayscale_enhanced_contrast_boundaries) {
    static const struct { float e; int ok; } c[] = { { 0.0f, 1 }, { 1.0f, 1 }, { 50.0f, 1 }, { 1e30f, 1 }, { -1e-30f, 0 }, { -1.0f, 0 }, { -1e30f, 0 } };
    int i;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) { pv v = base(); v.gec = c[i].e; all(&v, c[i].ok, "grayscaleEnhancedContrast", 1); }
}
RT(params_grayscale_contrast_is_ignored_by_v0) {
    pv v = base(); void *p; HRESULT hr;
    v.gec = -5.0f;              /* v0 has no such argument: the field is not part of the call */
    hr = make(0, &v, &p);
    if (hr == RR_UNSUP) return;
    CHECK_HR_OK(hr);
    if (hr == S_OK) IUnknown_Release((IUnknown *)p);
}
RT(params_cleartype_level_boundaries) {
    static const struct { float l; int ok; } c[] = { { 0.0f, 1 }, { -0.0f, 1 }, { 0.5f, 1 }, { 1.0f, 1 }, { 1.0001f, 1 }, { 2.5f, 1 }, { 1000.0f, 1 }, { 1e30f, 1 }, { -1e-30f, 0 }, { -0.5f, 0 }, { -1e30f, 0 } };
    int i;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) { pv v = base(); v.ct = c[i].l; all(&v, c[i].ok, "clearTypeLevel", 0); }
}
RT(params_pixel_geometry_range) {
    int g;
    for (g = -1; g <= 6; g++) { pv v = base(); v.pg = g; all(&v, g >= 0 && g <= 2, "pixelGeometry", 0); }
    { pv v = base(); v.pg = 0x7fffffff; all(&v, 0, "pixelGeometry", 0); }
}
RT(params_rendering_mode_range_legacy_overloads) {
    int m;
    for (m = 0; m <= 9; m++) { pv v = base(); v.mode = m; expect(0, &v, m <= 6, "renderingMode"); expect(1, &v, m <= 6, "renderingMode"); expect(2, &v, m <= 6, "renderingMode"); }
    { pv v = base(); v.mode = -1; expect(0, &v, 0, "renderingMode -1"); expect(2, &v, 0, "renderingMode -1"); }
}
RT(params_rendering_mode_range_mode1_overload) {
    int m;
    for (m = 0; m <= 9; m++) { pv v = base(); v.mode = m; expect(3, &v, m <= 7, "renderingMode1"); }
    { pv v = base(); v.mode = 0x7fffffff; expect(3, &v, 0, "renderingMode1 max"); }
}
RT(params_grid_fit_range) {
    int g;
    for (g = -1; g <= 5; g++) { pv v = base(); v.grid = g; expect(2, &v, g >= 0 && g <= 2, "gridFitMode"); expect(3, &v, g >= 0 && g <= 2, "gridFitMode"); }
}
RT(params_one_bad_field_among_good_ones) {
    pv v = base(); int ver;
    for (ver = 0; ver <= 3; ver++) {
        void *p; HRESULT hr;
        v = base(); v.gamma = 0.0f; v.ec = -1.0f; hr = make(ver, &v, &p); if (hr != RR_UNSUP) t_check(hr == RR_INVALIDARG && p == NULL, __FILE__, __LINE__, "two bad fields", "v%d: %s", ver, dw_hr_name(hr));
        v = base(); v.pg = 9; v.mode = 9; hr = make(ver, &v, &p); if (hr != RR_UNSUP) t_check(hr == RR_INVALIDARG && p == NULL, __FILE__, __LINE__, "two bad enums", "v%d: %s", ver, dw_hr_name(hr));
    }
}

/* ---- getters after creation ---------------------------------------------------------------------------------------------- */
static void getters_for(int ver, const pv *v) {
    void *p; HRESULT hr = make(ver, v, &p);
    if (hr == RR_UNSUP) return;
    CHECK_HR_OK(hr);
    if (hr == S_OK && p) {
        IDWriteRenderingParams *q0 = NULL; IDWriteRenderingParams1 *q1 = NULL; IDWriteRenderingParams2 *q2 = NULL; IDWriteRenderingParams3 *q3 = NULL;
        CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams, (void **)&q0));
        CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams1, (void **)&q1));
        CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams2, (void **)&q2));
        CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams3, (void **)&q3));
        if (q0 && q1 && q2 && q3) {
            float gec = ver == 0 ? 1.0f : v->gec;
            CHECK_FLOAT_EQ(IDWriteRenderingParams_GetGamma(q0), v->gamma);
            CHECK_FLOAT_EQ(IDWriteRenderingParams_GetEnhancedContrast(q0), v->ec);
            CHECK_FLOAT_EQ(IDWriteRenderingParams_GetClearTypeLevel(q0), v->ct);
            EXPECT_EQ(IDWriteRenderingParams_GetPixelGeometry(q0), v->pg);
            EXPECT_EQ(IDWriteRenderingParams_GetRenderingMode(q0), v->mode == 7 ? 5 : v->mode);
            CHECK_FLOAT_EQ(IDWriteRenderingParams1_GetGrayscaleEnhancedContrast(q1), gec);
            EXPECT_EQ(IDWriteRenderingParams2_GetGridFitMode(q2), ver >= 2 ? v->grid : 0);
            EXPECT_EQ(IDWriteRenderingParams3_GetRenderingMode1(q3), v->mode);
            CHECK_FLOAT_EQ(IDWriteRenderingParams3_GetGamma(q3), v->gamma);
            CHECK_FLOAT_EQ(IDWriteRenderingParams3_GetGrayscaleEnhancedContrast(q3), gec);
        }
        DW_RELEASE(q0); DW_RELEASE(q1); DW_RELEASE(q2); DW_RELEASE(q3);
        IUnknown_Release((IUnknown *)p);
    }
}
RT(params_getters_v0) { pv v = base(); getters_for(0, &v); v.gamma = 256.0f; v.ct = 7.5f; v.ec = 40.0f; getters_for(0, &v); }
RT(params_getters_v1) { pv v = base(); getters_for(1, &v); v.gec = 0.0f; getters_for(1, &v); v.gec = 33.0f; getters_for(1, &v); }
RT(params_getters_v2) { pv v = base(); int g; for (g = 0; g <= 2; g++) { v.grid = g; getters_for(2, &v); } }
RT(params_getters_v3) { pv v = base(); int m, g; for (m = 0; m <= 7; m++) for (g = 0; g <= 2; g++) { v.mode = m; v.grid = g; getters_for(3, &v); } }
RT(params_getters_every_pixel_geometry_and_mode) {
    pv v = base(); int ver, pg, m;
    for (ver = 0; ver <= 3; ver++) for (pg = 0; pg <= 2; pg++) for (m = 0; m <= 6; m++) { v.pg = pg; v.mode = m; getters_for(ver, &v); }
}
RT(params_downsampled_mode_maps_to_symmetric_for_legacy_getter) {
    pv v = base(); void *p; v.mode = 7;
    if (make(3, &v, &p) == RR_UNSUP) { t_skip("IDWriteFactory3 unsupported"); return; }
    if (p) { CHECK_HR_OK(S_OK); IUnknown_Release((IUnknown *)p); }
    getters_for(3, &v);
}
RT(params_objects_are_independent_and_counted) {
    pv a = base(), b = base(); void *pa, *pb; ULONG r;
    a.gamma = 1.0f; b.gamma = 3.0f;
    if (make(0, &a, &pa) != S_OK || make(0, &b, &pb) != S_OK) { t_skip("create"); return; }
    CHECK_FLOAT_EQ(IDWriteRenderingParams_GetGamma((IDWriteRenderingParams *)pa), 1.0f);
    CHECK_FLOAT_EQ(IDWriteRenderingParams_GetGamma((IDWriteRenderingParams *)pb), 3.0f);
    CHECK(pa != pb);
    EXPECT_REFS(pa, 1); EXPECT_REFS(pb, 1);
    r = IUnknown_AddRef((IUnknown *)pa); EXPECT_EQ(r, 2); r = IUnknown_Release((IUnknown *)pa); EXPECT_EQ(r, 1);
    r = IUnknown_Release((IUnknown *)pa); EXPECT_EQ(r, 0);
    CHECK_FLOAT_EQ(IDWriteRenderingParams_GetGamma((IDWriteRenderingParams *)pb), 3.0f);
    IUnknown_Release((IUnknown *)pb);
}
RT(params_qi_rejects_other_interfaces) {
    pv v = base(); void *p; int n, i;
    const dw_qi_row *rows = dw_iid_table(&n);
    if (make(0, &v, &p) != S_OK) { t_skip("create"); return; }
    for (i = 0; i < n; i++) {
        void *q = (void *)(ULONG_PTR)0x1000; HRESULT hr = IUnknown_QueryInterface((IUnknown *)p, rows[i].iid, &q);
        int ours = !memcmp(rows[i].iid, &IID_IDWriteRenderingParams, sizeof(IID)) || !memcmp(rows[i].iid, &IID_IDWriteRenderingParams1, sizeof(IID)) ||
                   !memcmp(rows[i].iid, &IID_IDWriteRenderingParams2, sizeof(IID)) || !memcmp(rows[i].iid, &IID_IDWriteRenderingParams3, sizeof(IID));
        if (ours) { CHECK_HR_OK(hr); if (SUCCEEDED(hr) && q) IUnknown_Release((IUnknown *)q); }
        else { t_check(hr == E_NOINTERFACE && q == NULL, __FILE__, __LINE__, "foreign interface", "%s: %s %p", rows[i].name, dw_hr_name(hr), q); if (SUCCEEDED(hr) && q) IUnknown_Release((IUnknown *)q); }
    }
    IUnknown_Release((IUnknown *)p);
}
RT(params_default_params_are_valid_inputs_for_recommended_mode) {
    RR_FIX3(x, NULL);
    IDWriteRenderingParams *dp = NULL, *mp = NULL; int m, g; rr_in in = rr_mk(12, 96, 0, 0);
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(x.f, &dp));
    CHECK_HR_OK(IDWriteFactory_CreateMonitorRenderingParams(x.f, MonitorFromPoint((POINT){ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mp));
    if (dp) { CHECK_HR_OK(rr_call3(&x, &in, dp, &m, &g)); CHECK(m >= 1 && m <= 7); }
    if (mp) { CHECK_HR_OK(rr_call3(&x, &in, mp, &m, &g)); CHECK(m >= 1 && m <= 7); t_note("primary monitor params: mode %d grid %d", m, g); }
    DW_RELEASE(dp); DW_RELEASE(mp);
    rr_close(&x);
}
