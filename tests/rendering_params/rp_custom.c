/* CreateCustomRenderingParams, all four factory overloads:
 *   IDWriteFactory   (gamma, enhancedContrast, clearTypeLevel, pixelGeometry, renderingMode)                    -> Params
 *   IDWriteFactory1  (gamma, enhancedContrast, grayscaleEnhancedContrast, clearTypeLevel, pixelGeometry, mode)    -> Params1
 *   IDWriteFactory2  (... , mode, gridFitMode)                                                                    -> Params2
 *   IDWriteFactory3  (... , DWRITE_RENDERING_MODE1 mode, gridFitMode)                                             -> Params3
 * Every accepted value is stored verbatim (no clamping, no rounding) and reported by every getter at every interface level.
 * The overloads that have no grayscale contrast report 1.0, those without a grid fit mode report DEFAULT.
 */
#include "rp.h"

#define MAXMODE(ver) ((ver) == 3 ? 7 : 6)

/* ---- one block of tests per factory overload ------------------------------------------------------------------------ */
#define RP_VER_TESTS(N) \
TEST(rpc_v##N##_basic_roundtrip) { RP_NEED(N); rp_vals v = rp_base(); RP_ROUNDTRIP(N, &v); } \
TEST(rpc_v##N##_isolated_factory) { RP_NEED(N); rp_vals v = rp_base(); v.gamma = 1.5f; rp_roundtrip_(DWRITE_FACTORY_TYPE_ISOLATED, N, &v, __FILE__, __LINE__); } \
TEST(rpc_v##N##_every_pixel_geometry) { \
    RP_NEED(N); int pg; \
    for (pg = DWRITE_PIXEL_GEOMETRY_FLAT; pg <= DWRITE_PIXEL_GEOMETRY_BGR; pg++) { rp_vals v = rp_base(); v.pg = pg; RP_ROUNDTRIP(N, &v); } } \
TEST(rpc_v##N##_every_rendering_mode) { \
    RP_NEED(N); int m; \
    for (m = 0; m <= MAXMODE(N); m++) { rp_vals v = rp_base(); v.mode = m; RP_ROUNDTRIP(N, &v); } } \
TEST(rpc_v##N##_mode_by_pixel_geometry_matrix) { \
    RP_NEED(N); int m, pg; \
    for (pg = 0; pg <= 2; pg++) for (m = 0; m <= MAXMODE(N); m++) { rp_vals v = rp_base(); v.pg = pg; v.mode = m; v.gamma = 1.0f + 0.25f * (float)m; RP_ROUNDTRIP(N, &v); } } \
TEST(rpc_v##N##_gamma_table) { \
    RP_NEED(N); static const float g[] = { 1.0f, 1.8f, 2.2f, 2.5f, 3.0f, 0.5f, 0.001f, 10.0f, 100.0f, 255.99f, 256.0f, 1e-30f, FLT_MIN, 1.17549435e-38f, 0.3333333f }; \
    unsigned i; for (i = 0; i < sizeof g / sizeof g[0]; i++) { rp_vals v = rp_base(); v.gamma = g[i]; RP_ROUNDTRIP(N, &v); } } \
TEST(rpc_v##N##_enhanced_contrast_table) { \
    RP_NEED(N); static const float c[] = { 0.0f, 0.1f, 0.5f, 1.0f, 2.0f, 4.0f, 10.0f, 1000.0f, 1e20f, FLT_MAX, FLT_MIN, 0.7f }; \
    unsigned i; for (i = 0; i < sizeof c / sizeof c[0]; i++) { rp_vals v = rp_base(); v.ec = c[i]; RP_ROUNDTRIP(N, &v); } } \
TEST(rpc_v##N##_cleartype_level_table) { \
    RP_NEED(N); static const float c[] = { 0.0f, 0.01f, 0.25f, 0.5f, 0.75f, 0.999f, 1.0f, 1.0000001f, 1.5f, 2.0f, 100.0f, 1e9f }; \
    unsigned i; for (i = 0; i < sizeof c / sizeof c[0]; i++) { rp_vals v = rp_base(); v.ct = c[i]; RP_ROUNDTRIP(N, &v); } } \
TEST(rpc_v##N##_grayscale_contrast_table) { \
    RP_NEED(N); static const float c[] = { 0.0f, 0.1f, 0.5f, 1.0f, 2.0f, 4.0f, 17.5f, 1e6f, FLT_MAX }; \
    unsigned i; for (i = 0; i < sizeof c / sizeof c[0]; i++) { rp_vals v = rp_base(); v.gec = c[i]; RP_ROUNDTRIP(N, &v); } } \
TEST(rpc_v##N##_all_zero_contrast_and_level) { \
    RP_NEED(N); rp_vals v = rp_base(); v.ec = 0.0f; v.gec = 0.0f; v.ct = 0.0f; RP_ROUNDTRIP(N, &v); } \
TEST(rpc_v##N##_negative_zero_accepted) { \
    RP_NEED(N); rp_vals v = rp_base(); v.ec = rp_negzero(); v.ct = rp_negzero(); v.gec = rp_negzero(); \
    /* -0.0 < 0.0 is false: accepted, and kept bit for bit */ RP_ROUNDTRIP(N, &v); } \
TEST(rpc_v##N##_returns_new_object_each_call) { \
    RP_NEED(N); rp_vals v = rp_base(); void *a = NULL, *b = NULL; \
    CHECK_HR_OK(rp_make(N, &v, &a)); CHECK_HR_OK(rp_make(N, &v, &b)); \
    CHECK(a && b && a != b); if (a) EXPECT_REFS(a, 1); if (b) EXPECT_REFS(b, 1); DW_RELEASE(a); DW_RELEASE(b); } \
TEST(rpc_v##N##_objects_are_independent) { \
    RP_NEED(N); rp_vals v1 = rp_base(), v2 = rp_base(); void *a = NULL, *b = NULL; rp_out oa, ob, ea, eb; \
    v1.gamma = 1.0f; v1.ec = 0.0f; v1.pg = 0; v1.mode = 1; v2.gamma = 3.0f; v2.ec = 2.0f; v2.pg = 2; v2.mode = 3; v2.ct = 0.5f; \
    CHECK_HR_OK(rp_make(N, &v1, &a)); CHECK_HR_OK(rp_make(N, &v2, &b)); \
    if (a && b && rp_read(a, &oa) && rp_read(b, &ob)) { ea = rp_expect(N, &v1); eb = rp_expect(N, &v2); RP_CMP(oa, ea, "first"); RP_CMP(ob, eb, "second"); } \
    DW_RELEASE(a); DW_RELEASE(b); } \
TEST(rpc_v##N##_qi_chain_identity) { \
    RP_NEED(N); rp_vals v = rp_base(); void *p = NULL; DW_REQUIRE_OK(rp_make(N, &v, &p)); \
    void *q[4] = {0}, *u = NULL; const IID *ids[4] = { &IID_IDWriteRenderingParams, &IID_IDWriteRenderingParams1, &IID_IDWriteRenderingParams2, &IID_IDWriteRenderingParams3 }; int i; \
    /* all four interfaces (and IUnknown) are answered with the object's single vtable pointer */ \
    for (i = 0; i < 4; i++) { CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, ids[i], &q[i])); CHECK(q[i] == p); } \
    CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, &IID_IUnknown, &u)); CHECK(u == p); \
    EXPECT_REFS(p, 6); \
    for (i = 0; i < 4; i++) DW_RELEASE(q[i]); DW_RELEASE(u); EXPECT_REFS(p, 1); DW_RELEASE(p); } \
TEST(rpc_v##N##_qi_between_levels) { \
    RP_NEED(N); rp_vals v = rp_base(); void *p = NULL; DW_REQUIRE_OK(rp_make(N, &v, &p)); \
    void *a = NULL, *b = NULL, *c = NULL; \
    /* params3 -> params2 -> params1 -> params, and back up: each hop works from any level */ \
    CHECK_HR_OK(IDWriteRenderingParams3_QueryInterface((IDWriteRenderingParams3 *)p, &IID_IDWriteRenderingParams2, &a)); \
    CHECK_HR_OK(IDWriteRenderingParams2_QueryInterface((IDWriteRenderingParams2 *)a, &IID_IDWriteRenderingParams1, &b)); \
    CHECK_HR_OK(IDWriteRenderingParams1_QueryInterface((IDWriteRenderingParams1 *)b, &IID_IDWriteRenderingParams, &c)); \
    DW_RELEASE(a); DW_RELEASE(b); \
    CHECK_HR_OK(IDWriteRenderingParams_QueryInterface((IDWriteRenderingParams *)c, &IID_IDWriteRenderingParams3, &a)); \
    CHECK(a == p); DW_RELEASE(a); DW_RELEASE(c); DW_RELEASE(p); } \
TEST(rpc_v##N##_refcount_protocol) { \
    RP_NEED(N); rp_vals v = rp_base(); void *p = NULL; DW_REQUIRE_OK(rp_make(N, &v, &p)); \
    IDWriteRenderingParams *b = (IDWriteRenderingParams *)p; \
    ULONG r_; r_ = IDWriteRenderingParams_AddRef(b); EXPECT_EQ(r_, 2); r_ = IDWriteRenderingParams_AddRef(b); EXPECT_EQ(r_, 3); \
    r_ = IDWriteRenderingParams_Release(b); EXPECT_EQ(r_, 2); r_ = IDWriteRenderingParams_Release(b); EXPECT_EQ(r_, 1); \
    r_ = IDWriteRenderingParams_Release(b); EXPECT_EQ(r_, 0); } \
TEST(rpc_v##N##_survives_factory_release) { \
    RP_NEED(N); IDWriteFactory *nf = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); void *fv = NULL; void *p = NULL; \
    if (!nf) { t_skip("factory"); return; } \
    if (FAILED(IUnknown_QueryInterface((IUnknown *)nf, dw_factory_iid(N), &fv)) || !fv) { IDWriteFactory_Release(nf); t_skip("factory version"); return; } \
    rp_vals v = rp_base(); v.gamma = 2.0f; v.ec = 0.75f; v.ct = 0.5f; v.pg = 2; v.mode = 4; v.gec = 0.125f; v.grid = 2; rp_out want = rp_expect(N, &v), got; \
    switch (N) { \
    case 0: CHECK_HR_OK(IDWriteFactory_CreateCustomRenderingParams((IDWriteFactory *)fv, v.gamma, v.ec, v.ct, v.pg, v.mode, (IDWriteRenderingParams **)&p)); break; \
    case 1: CHECK_HR_OK(IDWriteFactory1_CreateCustomRenderingParams((IDWriteFactory1 *)fv, v.gamma, v.ec, v.gec, v.ct, v.pg, v.mode, (IDWriteRenderingParams1 **)&p)); break; \
    case 2: CHECK_HR_OK(IDWriteFactory2_CreateCustomRenderingParams((IDWriteFactory2 *)fv, v.gamma, v.ec, v.gec, v.ct, v.pg, v.mode, v.grid, (IDWriteRenderingParams2 **)&p)); break; \
    default: CHECK_HR_OK(IDWriteFactory3_CreateCustomRenderingParams((IDWriteFactory3 *)fv, v.gamma, v.ec, v.gec, v.ct, v.pg, v.mode, v.grid, (IDWriteRenderingParams3 **)&p)); } \
    IUnknown_Release((IUnknown *)fv); IDWriteFactory_Release(nf); \
    if (p && rp_read(p, &got)) RP_CMP(got, want, "after factory release"); \
    DW_RELEASE(p); }

RP_VER_TESTS(0)
RP_VER_TESTS(1)
RP_VER_TESTS(2)
RP_VER_TESTS(3)

/* ---- overload-specific facts ---------------------------------------------------------------------------------------- */
TEST(rpc_v0_grayscale_contrast_is_one) {
    RP_NEED(0); int i;
    /* the original overload has no grayscale parameter: the stored value is the constant 1.0 whatever the other inputs are */
    for (i = 0; i < 4; i++) {
        rp_vals v = rp_base(); void *p = NULL; v.ec = 0.25f * (float)i; v.gec = 9.0f; /* ignored by v0 */
        if (SUCCEEDED(rp_make(0, &v, &p)) && p) {
            IDWriteRenderingParams1 *p1 = NULL;
            if (SUCCEEDED(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams1, (void **)&p1)) && p1)
                CHECK_FLOAT_EQ(IDWriteRenderingParams1_GetGrayscaleEnhancedContrast(p1), 1.0f);
            DW_RELEASE(p1);
        }
        DW_RELEASE(p);
    }
}
TEST(rpc_v0_grid_fit_is_default) {
    RP_NEED(0); rp_vals v = rp_base(); void *p = NULL; rp_out o;
    DW_REQUIRE_OK(rp_make(0, &v, &p));
    if (rp_read(p, &o)) EXPECT_EQ(o.grid, DWRITE_GRID_FIT_MODE_DEFAULT);
    DW_RELEASE(p);
}
TEST(rpc_v1_grid_fit_is_default) {
    RP_NEED(1); rp_vals v = rp_base(); void *p = NULL; rp_out o;
    DW_REQUIRE_OK(rp_make(1, &v, &p));
    if (rp_read(p, &o)) EXPECT_EQ(o.grid, DWRITE_GRID_FIT_MODE_DEFAULT);
    DW_RELEASE(p);
}
TEST(rpc_v1_grayscale_contrast_roundtrip_distinct_from_enhanced) {
    RP_NEED(1); rp_vals v = rp_base(); void *p = NULL; rp_out o;
    v.ec = 0.5f; v.gec = 1.75f;
    DW_REQUIRE_OK(rp_make(1, &v, &p));
    if (rp_read(p, &o)) { CHECK_FLOAT_EQ(o.ec, 0.5f); CHECK_FLOAT_EQ(o.gec, 1.75f); }
    DW_RELEASE(p);
}
TEST(rpc_v1_argument_order) {
    RP_NEED(1);
    /* (gamma, enhancedContrast, grayscaleEnhancedContrast, clearTypeLevel): distinct values expose any swap */
    rp_vals v = { 1.1f, 2.2f, 3.3f, 0.4f, 2, 3, 0 }; RP_ROUNDTRIP(1, &v);
}
TEST(rpc_v0_argument_order) {
    RP_NEED(0);
    /* (gamma, enhancedContrast, clearTypeLevel, pixelGeometry, renderingMode) */
    rp_vals v = { 1.1f, 2.2f, 3.3f, 0.4f, 2, 3, 0 }; v.ct = 0.4f; RP_ROUNDTRIP(0, &v);
}
TEST(rpc_v2_grid_fit_modes) {
    RP_NEED(2); int g;
    for (g = DWRITE_GRID_FIT_MODE_DEFAULT; g <= DWRITE_GRID_FIT_MODE_ENABLED; g++) { rp_vals v = rp_base(); v.grid = g; RP_ROUNDTRIP(2, &v); }
}
TEST(rpc_v3_grid_fit_modes) {
    RP_NEED(3); int g;
    for (g = DWRITE_GRID_FIT_MODE_DEFAULT; g <= DWRITE_GRID_FIT_MODE_ENABLED; g++) { rp_vals v = rp_base(); v.grid = g; RP_ROUNDTRIP(3, &v); }
}
TEST(rpc_v2_grid_fit_by_mode_matrix) {
    RP_NEED(2); int g, m;
    for (g = 0; g <= 2; g++) for (m = 0; m <= 6; m++) { rp_vals v = rp_base(); v.grid = g; v.mode = m; v.pg = (g + m) % 3; RP_ROUNDTRIP(2, &v); }
}
TEST(rpc_v3_grid_fit_by_mode_matrix) {
    RP_NEED(3); int g, m;
    for (g = 0; g <= 2; g++) for (m = 0; m <= 7; m++) { rp_vals v = rp_base(); v.grid = g; v.mode = m; v.pg = (g + m) % 3; RP_ROUNDTRIP(3, &v); }
}
TEST(rpc_v3_downsampled_mode_reported_as_natural_symmetric_by_old_getter) {
    RP_NEED(3); rp_vals v = rp_base(); void *p = NULL;
    v.mode = DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED;
    DW_REQUIRE_OK(rp_make(3, &v, &p));
    IDWriteRenderingParams3 *p3 = rp_p3(p);
    if (p3) {
        /* GetRenderingMode(): value 7 does not exist in DWRITE_RENDERING_MODE and is mapped to NATURAL_SYMMETRIC (5) */
        EXPECT_EQ(IDWriteRenderingParams3_GetRenderingMode1(p3), DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED);
        EXPECT_EQ(IDWriteRenderingParams3_GetRenderingMode(p3), DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC);
        EXPECT_EQ(DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED, 7);
        EXPECT_EQ(DWRITE_RENDERING_MODE1_OUTLINE, 6);
    }
    DW_RELEASE(p3); DW_RELEASE(p);
}
TEST(rpc_v3_mode1_equals_old_mode_for_values_0_to_6) {
    RP_NEED(3); int m;
    for (m = 0; m <= 6; m++) {
        rp_vals v = rp_base(); void *p = NULL; v.mode = m;
        if (SUCCEEDED(rp_make(3, &v, &p)) && p) {
            IDWriteRenderingParams3 *p3 = rp_p3(p);
            if (p3) { EXPECT_EQ(IDWriteRenderingParams3_GetRenderingMode1(p3), m); EXPECT_EQ(IDWriteRenderingParams3_GetRenderingMode(p3), m); }
            DW_RELEASE(p3);
        }
        DW_RELEASE(p);
    }
}
TEST(rpc_v0_outline_mode_accepted) {
    RP_NEED(0); rp_vals v = rp_base(); v.mode = DWRITE_RENDERING_MODE_OUTLINE; RP_ROUNDTRIP(0, &v);
}
TEST(rpc_v2_outline_mode_accepted) {
    RP_NEED(2); rp_vals v = rp_base(); v.mode = DWRITE_RENDERING_MODE_OUTLINE; RP_ROUNDTRIP(2, &v);
}
TEST(rpc_enum_values) {
    /* header values the tests above depend on */
    EXPECT_EQ(DWRITE_PIXEL_GEOMETRY_FLAT, 0); EXPECT_EQ(DWRITE_PIXEL_GEOMETRY_RGB, 1); EXPECT_EQ(DWRITE_PIXEL_GEOMETRY_BGR, 2);
    EXPECT_EQ(DWRITE_RENDERING_MODE_DEFAULT, 0); EXPECT_EQ(DWRITE_RENDERING_MODE_ALIASED, 1); EXPECT_EQ(DWRITE_RENDERING_MODE_GDI_CLASSIC, 2);
    EXPECT_EQ(DWRITE_RENDERING_MODE_GDI_NATURAL, 3); EXPECT_EQ(DWRITE_RENDERING_MODE_NATURAL, 4); EXPECT_EQ(DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, 5);
    EXPECT_EQ(DWRITE_RENDERING_MODE_OUTLINE, 6);
    EXPECT_EQ(DWRITE_GRID_FIT_MODE_DEFAULT, 0); EXPECT_EQ(DWRITE_GRID_FIT_MODE_DISABLED, 1); EXPECT_EQ(DWRITE_GRID_FIT_MODE_ENABLED, 2);
}
TEST(rpc_same_values_all_overloads_agree) {
    int v;
    /* the shared subset of parameters yields identical objects whichever overload built them */
    rp_vals in = { 1.6f, 0.35f, 1.0f, 0.8f, 2, 4, 0 }; rp_out ref; int have = 0;
    for (v = 0; v <= 3; v++) {
        void *p = NULL; rp_out o;
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v)) continue;
        DW_REQUIRE_OK(rp_make(v, &in, &p));
        if (rp_read(p, &o)) { if (have) RP_CMP(o, ref, "overload agreement"); else { ref = o; have = 1; } }
        DW_RELEASE(p);
    }
}
TEST(rpc_precision_is_exact_32_bit) {
    RP_NEED(3);
    /* values are stored as binary32 with no rounding: neighbours of 2.2f stay distinct */
    float a = rp_next_up(2.2f), b = rp_next_down(2.2f);
    rp_vals va = rp_base(), vb = rp_base(); va.gamma = a; vb.gamma = b;
    RP_ROUNDTRIP(3, &va); RP_ROUNDTRIP(3, &vb);
    va = rp_base(); va.ec = rp_next_up(0.0f); RP_ROUNDTRIP(3, &va);                 /* smallest denormal */
    va = rp_base(); va.ct = rp_next_down(1.0f); RP_ROUNDTRIP(3, &va);
}
TEST(rpc_gamma_upper_boundary_inclusive) {
    int ver;
    /* IsInRange is (0 < gamma <= 256) */
    for (ver = 0; ver <= 3; ver++) {
        rp_vals v = rp_base(); if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        v.gamma = 256.0f; RP_ROUNDTRIP(ver, &v);
        v.gamma = rp_next_down(256.0f); RP_ROUNDTRIP(ver, &v);
    }
}
TEST(rpc_gamma_lower_boundary_exclusive_smallest_positive) {
    int ver;
    for (ver = 0; ver <= 3; ver++) {
        rp_vals v = rp_base(); if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        v.gamma = rp_next_up(0.0f); RP_ROUNDTRIP(ver, &v);        /* the smallest positive denormal is > 0 */
    }
}
TEST(rpc_many_objects_all_overloads) {
    int ver, i;
    for (ver = 0; ver <= 3; ver++) {
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        void *objs[64]; rp_vals vals[64];
        for (i = 0; i < 64; i++) {
            vals[i] = rp_base(); vals[i].gamma = 1.0f + (float)i * 0.0625f; vals[i].pg = i % 3; vals[i].mode = i % (MAXMODE(ver) + 1); vals[i].ct = (float)(i % 5) * 0.25f; vals[i].grid = i % 3;
            objs[i] = NULL; CHECK_HR_OK(rp_make(ver, &vals[i], &objs[i]));
        }
        for (i = 0; i < 64; i++) { rp_out o, e = rp_expect(ver, &vals[i]); if (objs[i] && rp_read(objs[i], &o)) RP_CMP(o, e, "many"); }
        for (i = 0; i < 64; i++) DW_RELEASE(objs[i]);
    }
}
