/* Outline thresholds, measuring-mode validation and rendering-params object identity, driven from the
 * DWriteFontFace::GetRecommendedRenderingMode / DWriteRenderingParams implementations. */
#include "cts.h"

typedef struct { dwx_font h; IDWriteFontFace3 *f3; } fx3;
static int fx3_open(fx3 *x) {
    memset(x, 0, sizeof *x);
    if (!dw_shared() || FAILED(dwx_font_open_default(&x->h, dw_shared())) || !x->h.face) { dwx_font_close(&x->h); return 0; }
    if (FAILED(IDWriteFontFace_QueryInterface(x->h.face, &IID_IDWriteFontFace3, (void **)&x->f3)) || !x->f3) { dwx_font_close(&x->h); return 0; }
    return 1;
}
static void fx3_close(fx3 *x) { if (x->f3) IDWriteFontFace3_Release(x->f3); x->f3 = NULL; dwx_font_close(&x->h); }
#define FX3_OR_SKIP(x) do { if (!fx3_open(&(x))) { t_skip("synthetic font / IDWriteFontFace3 unavailable"); return; } } while (0)

static int rec3(fx3 *x, float em, float dpi, DWRITE_OUTLINE_THRESHOLD th, DWRITE_MEASURING_MODE mm, IDWriteRenderingParams *p, int *gf) {
    DWRITE_RENDERING_MODE1 m = (DWRITE_RENDERING_MODE1)-1; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)-1;
    HRESULT hr = IDWriteFontFace3_GetRecommendedRenderingMode(x->f3, em, dpi, dpi, NULL, FALSE, th, mm, p, &m, &g);
    CHECKF(SUCCEEDED(hr), "GetRecommendedRenderingMode -> %s", dw_hr_name(hr));
    if (gf) *gf = (int)g;
    return (int)m;
}

/* The ANTIALIASED threshold switches to OUTLINE once em * dpi/96 reaches 100 (compared with >=). */
TEST(ext_outline_antialiased_threshold_boundary) {
    static const DWRITE_MEASURING_MODE mms[] = { DWRITE_MEASURING_MODE_NATURAL, DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_MEASURING_MODE_GDI_NATURAL };
    fx3 x; int i;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    for (i = 0; i < 3; i++) {
        CHECKF(rec3(&x, 99.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, mms[i], NULL, NULL) != CTS_MODE_OUTLINE, "99 em, measuring %d", (int)mms[i]);
        CHECKF(rec3(&x, 100.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, mms[i], NULL, NULL) == CTS_MODE_OUTLINE, "100 em, measuring %d", (int)mms[i]);
        CHECKF(rec3(&x, 101.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, mms[i], NULL, NULL) == CTS_MODE_OUTLINE, "101 em, measuring %d", (int)mms[i]);
    }
    fx3_close(&x); cts_end();
}
/* The ALIASED threshold moves the switch to 350 ppem. */
TEST(ext_outline_aliased_threshold_boundary) {
    fx3 x; int i;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    for (i = 0; i < 3; i++) {
        DWRITE_MEASURING_MODE mm = (DWRITE_MEASURING_MODE)i;
        CHECKF(rec3(&x, 349.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ALIASED, mm, NULL, NULL) != CTS_MODE_OUTLINE, "349 em, measuring %d", i);
        CHECKF(rec3(&x, 350.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ALIASED, mm, NULL, NULL) == CTS_MODE_OUTLINE, "350 em, measuring %d", i);
        CHECKF(rec3(&x, 120.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ALIASED, mm, NULL, NULL) != CTS_MODE_OUTLINE, "120 em with ALIASED threshold, measuring %d", i);
    }
    fx3_close(&x); cts_end();
}
/* ppem = em * dpi / 96: the same em size crosses the threshold at a higher resolution. */
TEST(ext_outline_scales_with_dpi) {
    fx3 x;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    CHECK(rec3(&x, 50.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, NULL) != CTS_MODE_OUTLINE);
    CHECK(rec3(&x, 50.0f, 192.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, NULL) == CTS_MODE_OUTLINE);
    CHECK(rec3(&x, 80.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, NULL) != CTS_MODE_OUTLINE);
    CHECK(rec3(&x, 80.0f, 144.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, NULL) == CTS_MODE_OUTLINE);
    fx3_close(&x); cts_end();
}
/* The outline switch precedes the smoothing lookup, so it is independent of the system smoothing state. */
TEST(ext_outline_wins_over_every_smoothing_state) {
    fx3 x; int s;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    for (s = 0; s < 3; s++) {
        if (s == 0) { CTS_SET_OR_SKIP(cts_set_smoothing(FALSE)); }
        else { CTS_SET_OR_SKIP(cts_set_smoothing(TRUE)); CTS_SET_OR_SKIP(cts_set_type(s == 1 ? CTS_FE_CLEARTYPE : CTS_FE_STANDARD)); }
        CHECKF(rec3(&x, 100.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, NULL) == CTS_MODE_OUTLINE, "state %d", s);
    }
    fx3_close(&x); cts_end();
}
/* A params object built with DEFAULT mode does not stop the outline switch either. */
TEST(ext_outline_with_system_params_object) {
    fx3 x; IDWriteRenderingParams *p = NULL;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(dw_shared(), &p));
    CHECK(rec3(&x, 150.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, NULL) == CTS_MODE_OUTLINE);
    IDWriteRenderingParams_Release(p); fx3_close(&x); cts_end();
}

/* Argument validation: measuring mode outside 0..2 and threshold outside 0..1 give E_INVALIDARG and both outputs read zero. */
TEST(ext_invalid_measuring_modes_table) {
    static const unsigned bad[] = { 3, 4, 7, 100, 0x7fffffffu, 0xffffffffu, 0x80000000u };
    fx3 x; unsigned i;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWRITE_RENDERING_MODE1 m = (DWRITE_RENDERING_MODE1)0x55; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)0x55;
        HRESULT hr = IDWriteFontFace3_GetRecommendedRenderingMode(x.f3, 12.0f, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)bad[i], NULL, &m, &g);
        CHECKF(hr == E_INVALIDARG, "measuring %#x -> %s", bad[i], dw_hr_name(hr));
        CHECKF((int)m == 0 && (int)g == 0, "measuring %#x: outputs %d/%d", bad[i], (int)m, (int)g);
    }
    fx3_close(&x); cts_end();
}
TEST(ext_invalid_thresholds_table) {
    static const unsigned bad[] = { 2, 3, 0x100, 0x7fffffffu, 0xffffffffu };
    fx3 x; unsigned i;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWRITE_RENDERING_MODE1 m = (DWRITE_RENDERING_MODE1)0x55; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)0x55;
        HRESULT hr = IDWriteFontFace3_GetRecommendedRenderingMode(x.f3, 12.0f, 96.0f, 96.0f, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)bad[i], DWRITE_MEASURING_MODE_NATURAL, NULL, &m, &g);
        CHECKF(hr == E_INVALIDARG, "threshold %#x -> %s", bad[i], dw_hr_name(hr));
        CHECKF((int)m == 0 && (int)g == 0, "threshold %#x: outputs %d/%d", bad[i], (int)m, (int)g);
    }
    fx3_close(&x); cts_end();
}
TEST(ext_valid_measuring_modes_succeed) {
    fx3 x; int i;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    for (i = 0; i < 3; i++) {
        int gf = -1; int m = rec3(&x, 12.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)i, NULL, &gf);
        CHECKF(m >= 1 && m <= 7, "measuring %d -> mode %d", i, m);
        CHECKF(gf >= 1 && gf <= 2, "measuring %d -> grid fit %d (resolved, never DEFAULT)", i, gf);
    }
    fx3_close(&x); cts_end();
}
/* The resolved grid-fit mode is the same for repeated calls (no hidden state). */
TEST(ext_recommended_mode_repeatable) {
    fx3 x; int a, b, ga = 0, gb = 0;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    a = rec3(&x, 14.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, &ga);
    b = rec3(&x, 14.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, &gb);
    CHECK(a == b); CHECK(ga == gb);
    fx3_close(&x); cts_end();
}
/* Explicit params mode other than DEFAULT is returned unchanged for the GDI modes in every smoothing state. */
TEST(ext_explicit_gdi_modes_ignore_smoothing_state_via_v3) {
    static const int modes[] = { DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_GDI_CLASSIC, DWRITE_RENDERING_MODE_GDI_NATURAL };
    fx3 x; int i, s;
    CTS_BEGIN(); FX3_OR_SKIP(x);
    for (i = 0; i < 3; i++) for (s = 0; s < 2; s++) {
        IDWriteRenderingParams *p = NULL; int m;
        if (s == 0) { CTS_SET_OR_SKIP(cts_set_smoothing(FALSE)); } else { CTS_SET_OR_SKIP(cts_set_smoothing(TRUE)); CTS_SET_OR_SKIP(cts_set_type(CTS_FE_CLEARTYPE)); }
        if (FAILED(IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 2.2f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)modes[i], &p)) || !p) continue;
        m = rec3(&x, 12.0f, 96.0f, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, p, NULL);
        CHECKF(m == modes[i], "explicit %d state %d -> %d", modes[i], s, m);
        IDWriteRenderingParams_Release(p);
    }
    fx3_close(&x); cts_end();
}

/* ---- rendering params object identity --------------------------------------------------------------------------------------- */
TEST(ext_params_refcount_protocol) {
    IDWriteRenderingParams *p = NULL; ULONG r;
    if (!dw_shared()) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(dw_shared(), &p));
    r = IDWriteRenderingParams_AddRef(p);   EXPECT_EQ(r, 2u);
    r = IDWriteRenderingParams_AddRef(p);   EXPECT_EQ(r, 3u);
    r = IDWriteRenderingParams_Release(p);  EXPECT_EQ(r, 2u);
    r = IDWriteRenderingParams_Release(p);  EXPECT_EQ(r, 1u);
    r = IDWriteRenderingParams_Release(p);  EXPECT_EQ(r, 0u);
}
TEST(ext_params_query_interface_identity) {
    IDWriteRenderingParams *p = NULL; IUnknown *u1 = NULL, *u2 = NULL; IDWriteRenderingParams *same = NULL;
    if (!dw_shared()) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(dw_shared(), &p));
    CHECK_HR_OK(IDWriteRenderingParams_QueryInterface(p, &IID_IUnknown, (void **)&u1));
    CHECK_HR_OK(IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams, (void **)&same));
    if (u1) { CHECK_HR_OK(IUnknown_QueryInterface(u1, &IID_IUnknown, (void **)&u2)); CHECK(u1 == u2); }
    CHECK(same == p);
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    if (same) IDWriteRenderingParams_Release(same);
    IDWriteRenderingParams_Release(p);
}
TEST(ext_params_query_interface_unsupported) {
    IDWriteRenderingParams *p = NULL; void *o = (void *)(ULONG_PTR)1; HRESULT hr;
    if (!dw_shared()) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(dw_shared(), &p));
    hr = IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteFontFace, &o);
    CHECK_HR(hr, E_NOINTERFACE);
    CHECK(o == NULL);
    o = (void *)(ULONG_PTR)1;
    hr = IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteFactory, &o);
    CHECK_HR(hr, E_NOINTERFACE);
    CHECK(o == NULL);
    IDWriteRenderingParams_Release(p);
}
TEST(ext_params_derived_interfaces_share_values) {
    IDWriteRenderingParams *p = NULL; IDWriteRenderingParams1 *p1 = NULL;
    if (!dw_shared()) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(dw_shared(), &p));
    if (SUCCEEDED(IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams1, (void **)&p1)) && p1) {
        CHECK(IDWriteRenderingParams1_GetGamma(p1) == IDWriteRenderingParams_GetGamma(p));
        CHECK(IDWriteRenderingParams1_GetEnhancedContrast(p1) == IDWriteRenderingParams_GetEnhancedContrast(p));
        CHECK(IDWriteRenderingParams1_GetClearTypeLevel(p1) == IDWriteRenderingParams_GetClearTypeLevel(p));
        CHECK(IDWriteRenderingParams1_GetPixelGeometry(p1) == IDWriteRenderingParams_GetPixelGeometry(p));
        CHECK(IDWriteRenderingParams1_GetRenderingMode(p1) == IDWriteRenderingParams_GetRenderingMode(p));
        IDWriteRenderingParams1_Release(p1);
    } else t_skip("IDWriteRenderingParams1 unsupported");
    IDWriteRenderingParams_Release(p);
}
/* The object keeps its own references: it outlives the factory that made it. */
TEST(ext_params_outlive_isolated_factory) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteRenderingParams *p = NULL; cts_pv a, b;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(f, &p));
    cts_read(p, &a);
    IDWriteFactory_Release(f);
    cts_read(p, &b);
    CHECK(cts_same(&a, &b));
    IDWriteRenderingParams_Release(p);
}
/* A bogus (non-NULL) monitor handle is a handle, not a pointer: the call may fail but must leave the out pointer NULL then. */
TEST(ext_monitor_params_bogus_handle_note) {
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)(ULONG_PTR)1; HRESULT hr;
    if (!dw_shared()) { t_skip("no factory"); return; }
    hr = IDWriteFactory_CreateMonitorRenderingParams(dw_shared(), (HMONITOR)(ULONG_PTR)0x7ffe, &p);
    t_note("CreateMonitorRenderingParams(bogus HMONITOR) -> %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK(p == NULL);
    else if (p) IDWriteRenderingParams_Release(p);
}
/* Custom params: every field is stored verbatim, including mode values that the default constructor never produces. */
TEST(ext_custom_params_roundtrip_modes) {
    static const int modes[] = { 0, 1, 2, 3, 4, 5, 6 };
    int i;
    if (!dw_shared()) { t_skip("no factory"); return; }
    for (i = 0; i < 7; i++) {
        IDWriteRenderingParams *p = NULL; HRESULT hr = IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 1.0f, 0.0f, 0.0f, DWRITE_PIXEL_GEOMETRY_FLAT, (DWRITE_RENDERING_MODE)modes[i], &p);
        CHECKF(SUCCEEDED(hr), "mode %d -> %s", modes[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && p) { CHECKF((int)IDWriteRenderingParams_GetRenderingMode(p) == modes[i], "mode %d", modes[i]); IDWriteRenderingParams_Release(p); }
    }
}
TEST(ext_custom_params_invalid_arguments) {
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)(ULONG_PTR)1; HRESULT hr;
    if (!dw_shared()) { t_skip("no factory"); return; }
    hr = IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 1.0f, 0.0f, 0.0f, (DWRITE_PIXEL_GEOMETRY)3, DWRITE_RENDERING_MODE_DEFAULT, &p);
    t_note("pixel geometry 3 -> %s", dw_hr_name(hr));
    if (FAILED(hr)) { CHECK_HR(hr, E_INVALIDARG); CHECK(p == NULL); } else if (p) IDWriteRenderingParams_Release(p);
    p = (IDWriteRenderingParams *)(ULONG_PTR)1;
    hr = IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 1.0f, 0.0f, 0.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)7, &p);
    t_note("rendering mode 7 -> %s", dw_hr_name(hr));
    if (FAILED(hr)) { CHECK_HR(hr, E_INVALIDARG); CHECK(p == NULL); } else if (p) IDWriteRenderingParams_Release(p);
}
