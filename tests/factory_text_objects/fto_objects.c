/* Small factory-created objects: rendering params (4 creators + monitor), typography, number substitution,
 * GDI interop, text analyzer, system font collection.
 *
 * Facts (Win10 19041 dwrite.dll):
 *   CreateCustomRenderingParams (all overloads): gamma must satisfy 0 < gamma <= 256 (IsInRange); enhanced
 *     contrast, grayscale contrast and cleartype level must be >= 0 (no upper bound); pixel geometry <= 2 (BGR); rendering
 *     mode <= 6 (OUTLINE) for the DWRITE_RENDERING_MODE overloads, <= 7 for the IDWriteFactory3 (RENDERING_MODE1) overload;
 *     grid fit mode <= 2. Violations -> E_INVALIDARG with *params zeroed. The object stores the arguments verbatim; the
 *     IDWriteFactory (v0) overload fixes the grayscale contrast to 1.0 and grid fit to DEFAULT; RENDERING_MODE1 value 7
 *     (NATURAL_SYMMETRIC_DOWNSAMPLED) is reported as NATURAL_SYMMETRIC (5) by the legacy GetRenderingMode().
 *   CreateNumberSubstitution: locale==NULL is allowed only for method NONE (2); method > 4 -> E_INVALIDARG.
 *   CreateTypography / CreateTextAnalyzer / GetGdiInterop never fail.
 */
#include "fto.h"

#define NIL_P(p) ((void *)(p) == FTO_SENTINEL)

/* ---- CreateRenderingParams / CreateMonitorRenderingParams ---------------------------------------------------- */
FTO_BOTH(fto_rp_default_params) {
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(f, &p));
    if (!p || NIL_P(p)) return;
    EXPECT_REFS(p, 1);
    float g = IDWriteRenderingParams_GetGamma(p);
    float ec = IDWriteRenderingParams_GetEnhancedContrast(p);
    float ct = IDWriteRenderingParams_GetClearTypeLevel(p);
    DWRITE_PIXEL_GEOMETRY pg = IDWriteRenderingParams_GetPixelGeometry(p);
    DWRITE_RENDERING_MODE rm = IDWriteRenderingParams_GetRenderingMode(p);
    CHECK(g > 0.0f && g <= 256.0f);
    CHECK(ec >= 0.0f);
    CHECK(ct >= 0.0f);
    CHECK((UINT32)pg <= 2);
    CHECK((UINT32)rm <= 6);
    t_note("%s: default params gamma=%g contrast=%g cleartype=%g geometry=%d mode=%d", tag, g, ec, ct, pg, rm);
    IDWriteRenderingParams_Release(p);
}

FTO_BOTH(fto_rp_default_params_qi) {
    IDWriteRenderingParams *p = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(f, &p));
    if (!p) return;
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteRenderingParams, 1), DW_QI_ROW(IDWriteRenderingParams1, 1), DW_QI_ROW(IDWriteRenderingParams2, 1),
                         DW_QI_ROW(IDWriteRenderingParams3, 1), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteTextFormat, 0) };
    DW_CHECK_QI(p, rows);
    IDWriteRenderingParams_Release(p);
}

FTO_BOTH(fto_rp_default_params_higher_interfaces_consistent) {
    IDWriteRenderingParams *p = NULL;
    IDWriteRenderingParams1 *p1 = NULL; IDWriteRenderingParams2 *p2 = NULL; IDWriteRenderingParams3 *p3 = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(f, &p));
    if (!p) return;
    IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams1, (void **)&p1);
    IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams2, (void **)&p2);
    IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams3, (void **)&p3);
    if (p1) { CHECK(IDWriteRenderingParams1_GetGamma(p1) == IDWriteRenderingParams_GetGamma(p)); CHECK(IDWriteRenderingParams1_GetGrayscaleEnhancedContrast(p1) >= 0.0f); }
    if (p2) CHECK((UINT32)IDWriteRenderingParams2_GetGridFitMode(p2) <= 2);
    if (p3) {
        DWRITE_RENDERING_MODE1 m1 = IDWriteRenderingParams3_GetRenderingMode1(p3);
        DWRITE_RENDERING_MODE m0 = IDWriteRenderingParams_GetRenderingMode(p);
        CHECK((UINT32)m1 <= 7);
        /* legacy mode is the MODE1 value, except DOWNSAMPLED (7) which maps to NATURAL_SYMMETRIC (5) */
        CHECK((UINT32)m0 == (m1 == DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED ? 5u : (UINT32)m1));
    }
    DW_RELEASE(p1); DW_RELEASE(p2); DW_RELEASE(p3); IDWriteRenderingParams_Release(p);
}

FTO_BOTH(fto_rp_monitor_null) {
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
    /* A NULL monitor selects the default (non-monitor) params, the same path CreateRenderingParams uses. */
    CHECK_HR_OK(IDWriteFactory_CreateMonitorRenderingParams(f, NULL, &p));
    if (p && !NIL_P(p)) {
        CHECK(IDWriteRenderingParams_GetGamma(p) > 0.0f);
        IDWriteRenderingParams *d = NULL;
        CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(f, &d));
        if (d) {
            CHECK_FLOAT_EQ(IDWriteRenderingParams_GetGamma(p), IDWriteRenderingParams_GetGamma(d));
            CHECK_FLOAT_EQ(IDWriteRenderingParams_GetEnhancedContrast(p), IDWriteRenderingParams_GetEnhancedContrast(d));
            CHECK_FLOAT_EQ(IDWriteRenderingParams_GetClearTypeLevel(p), IDWriteRenderingParams_GetClearTypeLevel(d));
            CHECK(IDWriteRenderingParams_GetPixelGeometry(p) == IDWriteRenderingParams_GetPixelGeometry(d));
            CHECK(IDWriteRenderingParams_GetRenderingMode(p) == IDWriteRenderingParams_GetRenderingMode(d));
            IDWriteRenderingParams_Release(d);
        }
        IDWriteRenderingParams_Release(p);
    }
}

FTO_BOTH(fto_rp_monitor_primary) {
    POINT pt = { 0, 0 };
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
    if (!mon) { t_skip("no monitor in this session"); return; }
    HRESULT hr = IDWriteFactory_CreateMonitorRenderingParams(f, mon, &p);
    CHECK_HR_OK(hr);
    if (hr == S_OK && p && !NIL_P(p)) {
        CHECK(IDWriteRenderingParams_GetGamma(p) > 0.0f && IDWriteRenderingParams_GetGamma(p) <= 256.0f);
        CHECK(IDWriteRenderingParams_GetEnhancedContrast(p) >= 0.0f);
        CHECK((UINT32)IDWriteRenderingParams_GetPixelGeometry(p) <= 2);
        t_note("%s: primary monitor params gamma=%g geometry=%d mode=%d", tag, IDWriteRenderingParams_GetGamma(p),
               IDWriteRenderingParams_GetPixelGeometry(p), IDWriteRenderingParams_GetRenderingMode(p));
        IDWriteRenderingParams_Release(p);
    }
}

FTO_BOTH(fto_rp_monitor_bogus_handle_note) {
    /* Not a monitor: behaviour is whatever GetMonitorInfo-based lookup does; must not crash and must keep the out slot consistent. */
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
    HRESULT hr = IDWriteFactory_CreateMonitorRenderingParams(f, (HMONITOR)(ULONG_PTR)0x7fff1234, &p);
    t_note("%s: invalid HMONITOR -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr)) { CHECK(p != NULL && !NIL_P(p)); if (p && !NIL_P(p)) IDWriteRenderingParams_Release(p); }
    else CHECK_OUT_NULL(p);
}

/* ---- CreateCustomRenderingParams (v0) -------------------------------------------------------------------------- */
FTO_BOTH(fto_rp_custom_roundtrip) {
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR_OK(IDWriteFactory_CreateCustomRenderingParams(f, 2.2f, 0.5f, 0.75f, DWRITE_PIXEL_GEOMETRY_BGR, DWRITE_RENDERING_MODE_GDI_NATURAL, &p));
    if (!p || NIL_P(p)) return;
    CHECK_FLOAT_EQ(IDWriteRenderingParams_GetGamma(p), 2.2f);
    CHECK_FLOAT_EQ(IDWriteRenderingParams_GetEnhancedContrast(p), 0.5f);
    CHECK_FLOAT_EQ(IDWriteRenderingParams_GetClearTypeLevel(p), 0.75f);
    CHECK(IDWriteRenderingParams_GetPixelGeometry(p) == DWRITE_PIXEL_GEOMETRY_BGR);
    CHECK(IDWriteRenderingParams_GetRenderingMode(p) == DWRITE_RENDERING_MODE_GDI_NATURAL);
    EXPECT_REFS(p, 1);
    IDWriteRenderingParams1 *p1 = NULL; IDWriteRenderingParams2 *p2 = NULL;
    if (SUCCEEDED(IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams1, (void **)&p1)) && p1) {
        CHECK_FLOAT_EQ(IDWriteRenderingParams1_GetGrayscaleEnhancedContrast(p1), 1.0f);   /* v0 creator fixes it to 1.0 */
        IDWriteRenderingParams1_Release(p1);
    }
    if (SUCCEEDED(IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams2, (void **)&p2)) && p2) {
        CHECK(IDWriteRenderingParams2_GetGridFitMode(p2) == DWRITE_GRID_FIT_MODE_DEFAULT);
        IDWriteRenderingParams2_Release(p2);
    }
    IDWriteRenderingParams_Release(p);
}

FTO_BOTH(fto_rp_custom_gamma_table) {
    static const struct { float g; int ok; } rows[] = {
        { 1.0f, 1 }, { 0.001f, 1 }, { FLT_MIN, 1 }, { 1.8f, 1 }, { 2.2f, 1 }, { 4.0f, 1 }, { 100.0f, 1 }, { 255.99f, 1 }, { 256.0f, 1 },
        { 0.0f, 0 }, { -0.0f, 0 }, { -1.0f, 0 }, { -FLT_MIN, 0 }, { 256.001f, 0 }, { 257.0f, 0 }, { 1e10f, 0 }, { FLT_MAX, 0 },
        { INFINITY, 0 }, { -INFINITY, 0 }, { NAN, 0 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateCustomRenderingParams(f, rows[i].g, 0.5f, 0.5f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p);
        if (rows[i].ok) {
            CHECKF(hr == S_OK, "gamma %g: %s", rows[i].g, dw_hr_name(hr));
            if (hr == S_OK && p) { CHECKF(IDWriteRenderingParams_GetGamma(p) == rows[i].g, "gamma %g round trip", rows[i].g); IDWriteRenderingParams_Release(p); }
        } else {
            CHECKF(hr == FTO_E_INVALIDARG, "gamma %g: %s", rows[i].g, dw_hr_name(hr));
            CHECK_OUT_NULL(p);
        }
    }
}

FTO_BOTH(fto_rp_custom_negative_levels) {
    IDWriteRenderingParams *p;
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, -0.001f, 0.5f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 0.5f, -0.001f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, -INFINITY, 0.5f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 0.5f, -1e30f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
}

FTO_BOTH(fto_rp_custom_no_upper_bound_on_levels) {
    /* Contrast and cleartype level have no upper limit (only >= 0 is enforced), zero is accepted. */
    static const float v[] = { 0.0f, -0.0f, 1.0f, 2.0f, 10.0f, 1000.0f, 1e30f };
    unsigned i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteRenderingParams *p = NULL;
        CHECKF(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, v[i], v[i], DWRITE_PIXEL_GEOMETRY_FLAT, DWRITE_RENDERING_MODE_NATURAL, &p) == S_OK, "level %g", v[i]);
        if (p) {
            CHECK(IDWriteRenderingParams_GetEnhancedContrast(p) == v[i]);
            CHECK(IDWriteRenderingParams_GetClearTypeLevel(p) == v[i]);
            IDWriteRenderingParams_Release(p);
        }
    }
}

FTO_BOTH(fto_rp_custom_nan_levels_note) {
    /* NaN contrast / cleartype compare false against 0, so the validation as observed lets them through; recorded only. */
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
    HRESULT hr = IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, NAN, 0.5f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p);
    t_note("%s: NaN enhanced contrast -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && p && !NIL_P(p)) IDWriteRenderingParams_Release(p); else CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    hr = IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 0.5f, NAN, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p);
    t_note("%s: NaN cleartype level -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && p && !NIL_P(p)) IDWriteRenderingParams_Release(p); else CHECK_OUT_NULL(p);
}

FTO_BOTH(fto_rp_custom_geometry_table) {
    unsigned g;
    for (g = 0; g < 8; g++) {
        IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 0.5f, 0.5f, (DWRITE_PIXEL_GEOMETRY)g, DWRITE_RENDERING_MODE_DEFAULT, &p);
        if (g <= 2) {
            CHECKF(hr == S_OK, "geometry %u: %s", g, dw_hr_name(hr));
            if (hr == S_OK && p) { CHECK((UINT32)IDWriteRenderingParams_GetPixelGeometry(p) == g); IDWriteRenderingParams_Release(p); }
        } else { CHECKF(hr == FTO_E_INVALIDARG, "geometry %u: %s", g, dw_hr_name(hr)); CHECK_OUT_NULL(p); }
    }
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 0.5f, 0.5f, (DWRITE_PIXEL_GEOMETRY)1000, DWRITE_RENDERING_MODE_DEFAULT, &p), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(p);
}

FTO_BOTH(fto_rp_custom_mode_table) {
    unsigned m;
    for (m = 0; m < 12; m++) {
        IDWriteRenderingParams *p = (IDWriteRenderingParams *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 0.5f, 0.5f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)m, &p);
        if (m <= 6) {
            CHECKF(hr == S_OK, "mode %u: %s", m, dw_hr_name(hr));
            if (hr == S_OK && p) { CHECKF((UINT32)IDWriteRenderingParams_GetRenderingMode(p) == m, "mode %u round trip", m); IDWriteRenderingParams_Release(p); }
        } else { CHECKF(hr == FTO_E_INVALIDARG, "mode %u: %s", m, dw_hr_name(hr)); CHECK_OUT_NULL(p); }
    }
}

FTO_BOTH(fto_rp_custom_each_invalid_arg_zeroes_out) {
    IDWriteRenderingParams *p;
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 0.0f, 1, 1, 0, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, -1, 1, 0, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 1, -1, 0, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 1, 1, 3, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 1.0f, 1, 1, 0, 7, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
}

FTO_BOTH(fto_rp_custom_distinct_objects) {
    IDWriteRenderingParams *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateCustomRenderingParams(f, 1.5f, 0.5f, 0.5f, 1, 4, &a));
    CHECK_HR_OK(IDWriteFactory_CreateCustomRenderingParams(f, 1.5f, 0.5f, 0.5f, 1, 4, &b));
    if (a && b) CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
}

/* ---- CreateCustomRenderingParams v1 (IDWriteFactory1) ----------------------------------------------------------- */
#define NEED_F(n, var) IDWriteFactory##n *var = (IDWriteFactory##n *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, n); \
    if (!var) { t_skip("IDWriteFactory" #n " unsupported"); return; }

TEST(fto_rp1_roundtrip) {
    NEED_F(1, f1)
    IDWriteRenderingParams1 *p = (IDWriteRenderingParams1 *)FTO_SENTINEL;
    CHECK_HR_OK(IDWriteFactory1_CreateCustomRenderingParams(f1, 1.8f, 0.25f, 0.6f, 0.9f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, &p));
    if (!p || NIL_P(p)) return;
    CHECK_FLOAT_EQ(IDWriteRenderingParams1_GetGamma(p), 1.8f);
    CHECK_FLOAT_EQ(IDWriteRenderingParams1_GetEnhancedContrast(p), 0.25f);
    CHECK_FLOAT_EQ(IDWriteRenderingParams1_GetGrayscaleEnhancedContrast(p), 0.6f);
    CHECK_FLOAT_EQ(IDWriteRenderingParams1_GetClearTypeLevel(p), 0.9f);
    CHECK(IDWriteRenderingParams1_GetPixelGeometry(p) == DWRITE_PIXEL_GEOMETRY_RGB);
    CHECK(IDWriteRenderingParams1_GetRenderingMode(p) == DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC);
    IDWriteRenderingParams1_Release(p);
}

TEST(fto_rp1_validation) {
    NEED_F(1, f1)
    static const struct { float g, ec, gc, ct; UINT32 geo, mode; int ok; } rows[] = {
        { 1, 0, 0, 0, 0, 0, 1 }, { 256, 1, 1, 1, 2, 6, 1 }, { 1, 100, 100, 100, 1, 3, 1 },
        { 0, 1, 1, 1, 0, 0, 0 }, { 257, 1, 1, 1, 0, 0, 0 }, { NAN, 1, 1, 1, 0, 0, 0 },
        { 1, -1, 1, 1, 0, 0, 0 }, { 1, 1, -1, 1, 0, 0, 0 }, { 1, 1, 1, -1, 0, 0, 0 },
        { 1, 1, 1, 1, 3, 0, 0 }, { 1, 1, 1, 1, 0, 7, 0 }, { 1, 1, 1, 1, 99, 99, 0 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteRenderingParams1 *p = (IDWriteRenderingParams1 *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory1_CreateCustomRenderingParams(f1, rows[i].g, rows[i].ec, rows[i].gc, rows[i].ct, (DWRITE_PIXEL_GEOMETRY)rows[i].geo, (DWRITE_RENDERING_MODE)rows[i].mode, &p);
        if (rows[i].ok) { CHECKF(hr == S_OK, "row %u: %s", i, dw_hr_name(hr)); if (hr == S_OK && p) IDWriteRenderingParams1_Release(p); }
        else { CHECKF(hr == FTO_E_INVALIDARG, "row %u: %s", i, dw_hr_name(hr)); CHECK_OUT_NULL(p); }
    }
}

TEST(fto_rp1_grid_fit_default) {
    NEED_F(1, f1)
    IDWriteRenderingParams1 *p = NULL; IDWriteRenderingParams2 *p2 = NULL;
    CHECK_HR_OK(IDWriteFactory1_CreateCustomRenderingParams(f1, 1.0f, 1, 1, 1, 0, 0, &p));
    if (!p) return;
    if (SUCCEEDED(IDWriteRenderingParams1_QueryInterface(p, &IID_IDWriteRenderingParams2, (void **)&p2)) && p2) {
        CHECK(IDWriteRenderingParams2_GetGridFitMode(p2) == DWRITE_GRID_FIT_MODE_DEFAULT);
        IDWriteRenderingParams2_Release(p2);
    }
    IDWriteRenderingParams1_Release(p);
}

/* ---- v2 (IDWriteFactory2) -------------------------------------------------------------------------------------- */
TEST(fto_rp2_roundtrip_and_validation) {
    NEED_F(2, f2)
    IDWriteRenderingParams2 *p = (IDWriteRenderingParams2 *)FTO_SENTINEL;
    CHECK_HR_OK(IDWriteFactory2_CreateCustomRenderingParams(f2, 2.0f, 0.1f, 0.2f, 0.3f, DWRITE_PIXEL_GEOMETRY_BGR, DWRITE_RENDERING_MODE_OUTLINE, DWRITE_GRID_FIT_MODE_ENABLED, &p));
    if (p && !NIL_P(p)) {
        CHECK_FLOAT_EQ(IDWriteRenderingParams2_GetGamma(p), 2.0f);
        CHECK_FLOAT_EQ(IDWriteRenderingParams2_GetEnhancedContrast(p), 0.1f);
        CHECK_FLOAT_EQ(IDWriteRenderingParams2_GetGrayscaleEnhancedContrast(p), 0.2f);
        CHECK_FLOAT_EQ(IDWriteRenderingParams2_GetClearTypeLevel(p), 0.3f);
        CHECK(IDWriteRenderingParams2_GetPixelGeometry(p) == DWRITE_PIXEL_GEOMETRY_BGR);
        CHECK(IDWriteRenderingParams2_GetRenderingMode(p) == DWRITE_RENDERING_MODE_OUTLINE);
        CHECK(IDWriteRenderingParams2_GetGridFitMode(p) == DWRITE_GRID_FIT_MODE_ENABLED);
        IDWriteRenderingParams2_Release(p);
    }
    unsigned gf;
    for (gf = 0; gf < 6; gf++) {
        p = (IDWriteRenderingParams2 *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory2_CreateCustomRenderingParams(f2, 1, 1, 1, 1, 0, 0, (DWRITE_GRID_FIT_MODE)gf, &p);
        if (gf <= 2) { CHECKF(hr == S_OK, "gridfit %u: %s", gf, dw_hr_name(hr)); if (hr == S_OK && p) { CHECK((UINT32)IDWriteRenderingParams2_GetGridFitMode(p) == gf); IDWriteRenderingParams2_Release(p); } }
        else { CHECKF(hr == FTO_E_INVALIDARG, "gridfit %u: %s", gf, dw_hr_name(hr)); CHECK_OUT_NULL(p); }
    }
    p = (IDWriteRenderingParams2 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory2_CreateCustomRenderingParams(f2, 1, 1, 1, 1, 0, 7, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams2 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory2_CreateCustomRenderingParams(f2, 0, 1, 1, 1, 0, 0, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams2 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory2_CreateCustomRenderingParams(f2, 1, 1, 1, 1, 3, 0, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
}

/* ---- v3 (IDWriteFactory3, RENDERING_MODE1) ---------------------------------------------------------------------- */
TEST(fto_rp3_mode1_table) {
    NEED_F(3, f3)
    unsigned m;
    for (m = 0; m < 12; m++) {
        IDWriteRenderingParams3 *p = (IDWriteRenderingParams3 *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory3_CreateCustomRenderingParams(f3, 1.0f, 0.5f, 0.5f, 0.5f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE1)m, DWRITE_GRID_FIT_MODE_DEFAULT, &p);
        if (m <= 7) {
            CHECKF(hr == S_OK, "mode1 %u: %s", m, dw_hr_name(hr));
            if (hr == S_OK && p) {
                CHECKF((UINT32)IDWriteRenderingParams3_GetRenderingMode1(p) == m, "mode1 %u round trip", m);
                /* legacy getter folds DOWNSAMPLED (7) into NATURAL_SYMMETRIC (5) */
                CHECKF((UINT32)IDWriteRenderingParams3_GetRenderingMode(p) == (m == 7 ? 5u : m), "legacy mode for %u", m);
                IDWriteRenderingParams3_Release(p);
            }
        } else { CHECKF(hr == FTO_E_INVALIDARG, "mode1 %u: %s", m, dw_hr_name(hr)); CHECK_OUT_NULL(p); }
    }
}

TEST(fto_rp3_roundtrip_and_validation) {
    NEED_F(3, f3)
    IDWriteRenderingParams3 *p = (IDWriteRenderingParams3 *)FTO_SENTINEL;
    CHECK_HR_OK(IDWriteFactory3_CreateCustomRenderingParams(f3, 2.4f, 0.3f, 0.4f, 0.5f, DWRITE_PIXEL_GEOMETRY_FLAT, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_GRID_FIT_MODE_DISABLED, &p));
    if (p && !NIL_P(p)) {
        CHECK_FLOAT_EQ(IDWriteRenderingParams3_GetGamma(p), 2.4f);
        CHECK_FLOAT_EQ(IDWriteRenderingParams3_GetEnhancedContrast(p), 0.3f);
        CHECK_FLOAT_EQ(IDWriteRenderingParams3_GetGrayscaleEnhancedContrast(p), 0.4f);
        CHECK_FLOAT_EQ(IDWriteRenderingParams3_GetClearTypeLevel(p), 0.5f);
        CHECK(IDWriteRenderingParams3_GetPixelGeometry(p) == DWRITE_PIXEL_GEOMETRY_FLAT);
        CHECK(IDWriteRenderingParams3_GetGridFitMode(p) == DWRITE_GRID_FIT_MODE_DISABLED);
        IDWriteRenderingParams3_Release(p);
    }
    p = (IDWriteRenderingParams3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory3_CreateCustomRenderingParams(f3, 300, 1, 1, 1, 0, 0, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory3_CreateCustomRenderingParams(f3, 1, 1, 1, 1, 0, 0, 3, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory3_CreateCustomRenderingParams(f3, 1, 1, -0.5f, 1, 0, 0, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
    p = (IDWriteRenderingParams3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory3_CreateCustomRenderingParams(f3, 1, 1, 1, 1, 5, 0, 0, &p), FTO_E_INVALIDARG); CHECK_OUT_NULL(p);
}

/* ---- Typography ------------------------------------------------------------------------------------------------- */
FTO_BOTH(fto_typo_create) {
    IDWriteTypography *t = (IDWriteTypography *)FTO_SENTINEL;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t || NIL_P(t)) return;
    EXPECT_REFS(t, 1);
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), 0);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteTypography, 1), DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteInlineObject, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0) };
    DW_CHECK_QI(t, rows);
    IDWriteTypography_Release(t);
}

FTO_BOTH(fto_typo_distinct_objects) {
    IDWriteTypography *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &a));
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &b));
    if (a && b) {
        CHECK(a != b);
        DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1 };
        CHECK_HR_OK(IDWriteTypography_AddFontFeature(a, ft));
        EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(a), 1);
        EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(b), 0);
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

FTO_BOTH(fto_typo_feature_roundtrip) {
    IDWriteTypography *t = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    static const DWRITE_FONT_FEATURE_TAG tags[] = { DWRITE_FONT_FEATURE_TAG_KERNING, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_5, DWRITE_FONT_FEATURE_TAG_SLASHED_ZERO };
    unsigned i;
    for (i = 0; i < 4; i++) {
        DWRITE_FONT_FEATURE ft = { tags[i], 10 + i };
        CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, ft));
        EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), i + 1);
    }
    for (i = 0; i < 4; i++) {
        DWRITE_FONT_FEATURE g = { (DWRITE_FONT_FEATURE_TAG)0, 0 };
        CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, i, &g));
        CHECK(g.nameTag == tags[i] && g.parameter == 10 + i);
    }
    DWRITE_FONT_FEATURE g = { (DWRITE_FONT_FEATURE_TAG)7, 7 };
    HRESULT hr = IDWriteTypography_GetFontFeature(t, 4, &g);
    CHECKF(FAILED(hr), "out-of-range feature index: %s", dw_hr_name(hr));
    t_note("GetFontFeature(count) -> %s", dw_hr_name(hr));
    IDWriteTypography_Release(t);
}

FTO_BOTH(fto_typo_survives_factory_use) {
    IDWriteTypography *t = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (fmt && SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"fi", 2, fmt, 100, 100, &l)) && l) {
        DWRITE_TEXT_RANGE r = { 0, 2 };
        DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0 };
        CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, ft));
        CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, r));
        IDWriteTypography *back = NULL;
        CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 0, &back, &r));
        CHECK(back == t);
        DW_RELEASE(back);
    }
    DW_RELEASE(l); DW_RELEASE(fmt); IDWriteTypography_Release(t);
}

/* ---- CreateNumberSubstitution ------------------------------------------------------------------------------------ */
FTO_BOTH(fto_ns_none_accepts_anything) {
    static const WCHAR *loc[] = { NULL, L"", L"en-us", L"ar-sa", L"not a locale at all", L"xx-invalid", L"\x4e2d" };
    unsigned i;
    for (i = 0; i < sizeof loc / sizeof loc[0]; i++) {
        IDWriteNumberSubstitution *ns = (IDWriteNumberSubstitution *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, loc[i], FALSE, &ns);
        CHECKF(hr == S_OK, "NONE with locale #%u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && ns && !NIL_P(ns)) IDWriteNumberSubstitution_Release(ns);
        ns = (IDWriteNumberSubstitution *)FTO_SENTINEL;
        hr = IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, loc[i], TRUE, &ns);
        CHECKF(hr == S_OK, "NONE+ignoreUserOverride with locale #%u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && ns && !NIL_P(ns)) IDWriteNumberSubstitution_Release(ns);
    }
}

FTO_BOTH(fto_ns_null_locale_rejected_except_none) {
    static const DWRITE_NUMBER_SUBSTITUTION_METHOD m[] = { DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE, DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL,
        DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL };
    unsigned i;
    for (i = 0; i < 4; i++) {
        IDWriteNumberSubstitution *ns = (IDWriteNumberSubstitution *)FTO_SENTINEL;
        CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, m[i], NULL, FALSE, &ns), FTO_E_INVALIDARG);
        CHECKF(ns == NULL, "method %d: out %p", m[i], (void *)ns);
        ns = (IDWriteNumberSubstitution *)FTO_SENTINEL;
        CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, m[i], NULL, TRUE, &ns), FTO_E_INVALIDARG);
        CHECKF(ns == NULL, "method %d (ignore override): out %p", m[i], (void *)ns);
    }
}

FTO_BOTH(fto_ns_method_out_of_range) {
    static const int bad[] = { 5, 6, 100, 255, 256, 0x7fffffff };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteNumberSubstitution *ns = (IDWriteNumberSubstitution *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)bad[i], L"en-us", FALSE, &ns);
        if (bad[i] == 256) {   /* only the low byte of the method is validated (0 = FROM_CULTURE) */
            CHECKF(hr == S_OK, "method %d: %s", bad[i], dw_hr_name(hr));
            if (hr == S_OK && ns && !NIL_P(ns)) IDWriteNumberSubstitution_Release(ns);
            continue;
        }
        CHECKF(hr == FTO_E_INVALIDARG, "method %d: %s", bad[i], dw_hr_name(hr));
        CHECKF(ns == NULL, "method %d: out %p", bad[i], (void *)ns);
    }
}

FTO_BOTH(fto_ns_methods_with_locale_note) {
    static const WCHAR *loc[] = { L"en-us", L"ar-sa", L"fa-ir", L"hi-in", L"th-th", L"zh-cn", L"ja-jp", L"de-de" };
    unsigned m, i;
    for (m = 0; m <= 4; m++) {
        for (i = 0; i < sizeof loc / sizeof loc[0]; i++) {
            IDWriteNumberSubstitution *ns = (IDWriteNumberSubstitution *)FTO_SENTINEL;
            HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, loc[i], TRUE, &ns);
            t_note("%s: method %u locale %ls -> %s", tag, m, loc[i], dw_hr_name(hr));
            if (SUCCEEDED(hr)) { CHECK(ns && !NIL_P(ns)); if (ns && !NIL_P(ns)) IDWriteNumberSubstitution_Release(ns); }
            else CHECK_OUT_NULL(ns);
        }
    }
}

FTO_BOTH(fto_ns_qi_refs_distinct) {
    IDWriteNumberSubstitution *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-us", FALSE, &a));
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-us", FALSE, &b));
    if (a && b) {
        CHECK(a != b);
        EXPECT_REFS(a, 1);
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteNumberSubstitution, 1), DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteTypography, 0), DW_QI_ROW(IDWriteFont, 0) };
        DW_CHECK_QI(a, rows);
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

FTO_BOTH(fto_ns_locale_string_not_retained_by_caller) {
    WCHAR loc[8] = L"en-us";
    IDWriteNumberSubstitution *ns = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, loc, FALSE, &ns));
    lstrcpyW(loc, L"zz");
    DW_RELEASE(ns);
}

/* ---- GetGdiInterop -------------------------------------------------------------------------------------------------- */
FTO_BOTH(fto_gdi_interop_basic) {
    IDWriteGdiInterop *a = (IDWriteGdiInterop *)FTO_SENTINEL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a));
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &b));
    if (a && b && !NIL_P(a)) {
        CHECK(a == b);          /* the factory caches one interop object */
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteGdiInterop, 1), DW_QI_ROW(IDWriteGdiInterop1, 1), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0),
                             DW_QI_ROW(IDWriteBitmapRenderTarget, 0), DW_QI_ROW(IDWriteTextFormat, 0) };
        DW_CHECK_QI(a, rows);
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

FTO_BOTH(fto_gdi_interop_refcount_balance) {
    IDWriteGdiInterop *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a));
    if (!a) return;
    ULONG r1 = dw_refcount((IUnknown *)a);
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &b));
    ULONG r2 = dw_refcount((IUnknown *)a);
    t_note("%s: interop refcount %lu -> %lu after a second GetGdiInterop (child object)", tag, r1, r2);
    CHECK(r2 >= r1);
    IDWriteGdiInterop_Release(b);
    CHECK(dw_refcount((IUnknown *)a) <= r2);
    IDWriteGdiInterop_Release(a);
}

TEST(fto_gdi_interop_outlives_isolated_factory) {
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteGdiInterop *g = NULL;
    if (!iso) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(iso, &g));
    IDWriteFactory_Release(iso);
    if (!g) return;
    IDWriteGdiInterop1 *g1 = NULL;
    if (SUCCEEDED(IDWriteGdiInterop_QueryInterface(g, &IID_IDWriteGdiInterop1, (void **)&g1)) && g1) IDWriteGdiInterop1_Release(g1);
    IDWriteBitmapRenderTarget *bt = NULL;
    HRESULT hr = IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 8, 8, &bt);
    t_note("CreateBitmapRenderTarget after factory release -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr)) { CHECK(bt != NULL); DW_RELEASE(bt); } else CHECK_OUT_NULL(bt);
    IDWriteGdiInterop_Release(g);
}

/* ---- CreateTextAnalyzer ---------------------------------------------------------------------------------------------- */
FTO_BOTH(fto_analyzer_create) {
    IDWriteTextAnalyzer *a = (IDWriteTextAnalyzer *)FTO_SENTINEL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a));
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &b));
    if (a && b && !NIL_P(a)) {
        CHECK(a != b);
        EXPECT_REFS(a, 1);
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteTextAnalyzer, 1), DW_QI_ROW(IDWriteTextAnalyzer1, 1), DW_QI_ROW(IDWriteTextAnalyzer2, 1),
                             DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteTextLayout, 0), DW_QI_ROW(IDWriteFont, 0) };
        DW_CHECK_QI(a, rows);
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(fto_analyzer_works_after_factory_release) {
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteTextAnalyzer *an = NULL;
    static const WCHAR txt[] = { 'a', 'b', ' ', 'c', 'd', 0x05D0, 0x05D1 };
    if (!iso) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(iso, &an));
    IDWriteFactory_Release(iso);
    if (!an) return;
    dwx_ana_src *src = dwx_ana_src_new(txt, 7);
    dwx_ana_sink *sink = dwx_ana_sink_new();
    CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, 7, (IDWriteTextAnalysisSink *)sink));
    CHECK(dwx_ana_covered(sink, 's', 7));
    CHECK(sink->overlap == 0);
    CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)src, 0, 7, (IDWriteTextAnalysisSink *)sink));
    CHECK(dwx_ana_covered(sink, 'b', 7));
    CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeBidi(an, (IDWriteTextAnalysisSource *)src, 0, 7, (IDWriteTextAnalysisSink *)sink));
    CHECK(dwx_ana_covered(sink, 'l', 7));
    IDWriteTextAnalyzer_Release(an);
    dwx_release(src); dwx_release(sink);
}

/* ---- GetSystemFontCollection ------------------------------------------------------------------------------------------ */
FTO_BOTH(fto_sysfc_basic) {
    IDWriteFontCollection *a = (IDWriteFontCollection *)FTO_SENTINEL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &b, FALSE));
    if (a && b && !NIL_P(a)) {
        CHECK(a == b);                                     /* cached per factory */
        CHECK(IDWriteFontCollection_GetFontFamilyCount(a) > 0);
        UINT32 idx = 0; BOOL ex = TRUE;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(a, L"NoSuchFamily_zz9_xyz", &idx, &ex));
        CHECK(ex == FALSE);
        CHECK(idx == 0xFFFFFFFFu);
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontCollection, 1), DW_QI_ROW(IDWriteFontCollection1, 1), DW_QI_ROW(IDWriteFontCollection2, -1),
                             DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFont, 0) };
        DW_CHECK_QI(a, rows);
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

FTO_BOTH(fto_sysfc_check_for_updates_values) {
    static const BOOL v[] = { FALSE, TRUE, 2, -1 };
    unsigned i;
    UINT32 base = 0;
    for (i = 0; i < 4; i++) {
        IDWriteFontCollection *c = NULL;
        CHECKF(IDWriteFactory_GetSystemFontCollection(f, &c, v[i]) == S_OK, "checkForUpdates=%d", v[i]);
        if (c) {
            UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c);
            if (i == 0) base = n;
            else t_note("%s: checkForUpdates=%d family count %u (base %u)", tag, v[i], n, base);
            CHECK(n > 0);
            IDWriteFontCollection_Release(c);
        }
    }
}

FTO_BOTH(fto_sysfc_refcount_balance) {
    IDWriteFontCollection *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    if (!a) return;
    ULONG r1 = dw_refcount((IUnknown *)a);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &b, FALSE));
    ULONG r2 = dw_refcount((IUnknown *)a);
    t_note("%s: system collection refcount %lu -> %lu (child object)", tag, r1, r2);
    CHECK(r2 >= r1);
    IDWriteFontCollection_Release(b);
    CHECK(dw_refcount((IUnknown *)a) <= r2);
    IDWriteFontCollection_Release(a);
}

TEST(fto_sysfc_shared_vs_isolated_distinct) {
    IDWriteFactory *sh = dw_shared(), *iso = dw_isolated();
    IDWriteFontCollection *a = NULL, *b = NULL;
    if (!sh || !iso) { t_skip("no factories"); return; }
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(sh, &a, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(iso, &b, FALSE));
    if (a && b) {
        CHECK(a != b);
        CHECK(IDWriteFontCollection_GetFontFamilyCount(a) == IDWriteFontCollection_GetFontFamilyCount(b));
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(fto_sysfc_v3_include_downloadable) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontCollection1 *c = (IDWriteFontCollection1 *)FTO_SENTINEL;
    HRESULT hr = IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &c, FALSE);
    CHECK_HR_OK(hr);
    if (hr == S_OK && c && !NIL_P(c)) {
        CHECK(IDWriteFontCollection1_GetFontFamilyCount(c) > 0);
        IDWriteFontCollection1_Release(c);
    }
    c = (IDWriteFontCollection1 *)FTO_SENTINEL;
    hr = IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &c, FALSE);
    t_note("includeDownloadableFonts=TRUE -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && c && !NIL_P(c)) IDWriteFontCollection1_Release(c); else CHECK_OUT_NULL(c);
}

FTO_BOTH(fto_sysfc_eudc_note) {
    IDWriteFactory1 *f1 = (IDWriteFactory1 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 1);
    IDWriteFontCollection *c = (IDWriteFontCollection *)FTO_SENTINEL;
    if (!f1) { t_skip("IDWriteFactory1 unsupported"); return; }
    (void)f;
    HRESULT hr = IDWriteFactory1_GetEudcFontCollection(f1, &c, FALSE);
    t_note("GetEudcFontCollection -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr)) { CHECK(c && !NIL_P(c)); if (c && !NIL_P(c)) IDWriteFontCollection_Release(c); }
}
