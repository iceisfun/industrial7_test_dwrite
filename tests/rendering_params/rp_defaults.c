/* CreateRenderingParams / CreateMonitorRenderingParams: system-default parameters.
 *
 * Both entry points funnel into the same code: the object is built from built-in defaults (gamma 1.8, enhanced contrast 0.5,
 * grayscale enhanced contrast 1.0, ClearType level 1.0, pixel geometry RGB, rendering mode DEFAULT, grid fit DEFAULT) that
 * are overridden per display by HKCU/HKLM "Software\Microsoft\Avalon.Graphics\<device>" values, which are range-limited:
 * GammaLevel 1000..2200 (x0.001), ClearTypeLevel 0..100 (x0.01), EnhancedContrastLevel and GrayscaleEnhancedContrastLevel 0..400
 * (x0.01), PixelStructure 0..2.  Pixel geometry otherwise follows SPI_GETFONTSMOOTHINGORIENTATION (RGB or BGR).
 * The tests hard-check the ranges and the DEFAULT enums, and only t_note the numbers that a user's ClearType tuning can change.
 */
#include "rp.h"

#define NEED_PARAMS(var) void *var = NULL; if (!rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &var)) { t_skip("CreateRenderingParams failed"); return; }

static int read_default(DWRITE_FACTORY_TYPE t, rp_out *o) {
    void *p = NULL; int ok = 0;
    if (rp_get_default(t, &p)) ok = rp_read(p, o);
    DW_RELEASE(p);
    return ok;
}

TEST(rpd_create_ok) {
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)RP_SENT;
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(f, &p));
    CHECK(p != NULL && p != RP_SENT);
    if (p != RP_SENT) DW_RELEASE(p);
}
TEST(rpd_create_ok_isolated) {
    IDWriteFactory *f = dw_isolated(); REQUIRE(f);
    IDWriteRenderingParams *p = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(f, &p));
    CHECK(p != NULL);
    DW_RELEASE(p);
}
TEST(rpd_create_fresh_object_refcount_one) {
    NEED_PARAMS(p);
    /* the factory hands out a new object holding one reference (ComObject starts at 0, the creating ComPtr adds one) */
    EXPECT_REFS(p, 1);
    DW_RELEASE(p);
}
TEST(rpd_create_returns_distinct_objects) {
    void *a = NULL, *b = NULL;
    if (!rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &a) || !rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &b)) { DW_RELEASE(a); DW_RELEASE(b); t_skip("create failed"); return; }
    /* a new allocation per call, not a cached singleton */
    CHECK(a != b);
    EXPECT_REFS(a, 1); EXPECT_REFS(b, 1);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(rpd_create_through_each_factory_version) {
    int v;
    for (v = 0; v <= 7; v++) {
        void *f = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v);
        IDWriteRenderingParams *p = NULL;
        if (!f) { t_note("IDWriteFactory%d unsupported", v); continue; }
        /* CreateRenderingParams sits in the IDWriteFactory part of every derived vtable */
        CHECK_HR_OK(IDWriteFactory_CreateRenderingParams((IDWriteFactory *)f, &p));
        CHECK(p != NULL);
        DW_RELEASE(p);
    }
}
TEST(rpd_default_gamma_range) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    /* registry GammaLevel is accepted only for 1000..2200; the built-in default is 1.8 */
    CHECKF(o.gamma >= 1.0f && o.gamma <= 2.2f, "gamma %g", (double)o.gamma);
    t_note("default gamma %g", (double)o.gamma);
    DW_RELEASE(p);
}
TEST(rpd_default_enhanced_contrast_range) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    CHECKF(o.ec >= 0.0f && o.ec <= 4.0f, "enhanced contrast %g", (double)o.ec);   /* registry value <= 400 x 0.01 */
    t_note("default enhanced contrast %g", (double)o.ec);
    DW_RELEASE(p);
}
TEST(rpd_default_grayscale_contrast_range) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    CHECKF(o.gec >= 0.0f && o.gec <= 4.0f, "grayscale enhanced contrast %g", (double)o.gec);
    t_note("default grayscale enhanced contrast %g", (double)o.gec);
    DW_RELEASE(p);
}
TEST(rpd_default_cleartype_level_range) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    CHECKF(o.ct >= 0.0f && o.ct <= 1.0f, "ClearType level %g", (double)o.ct);   /* registry value <= 100 x 0.01 */
    t_note("default ClearType level %g", (double)o.ct);
    DW_RELEASE(p);
}
TEST(rpd_default_pixel_geometry_valid) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    CHECKF(o.pg >= DWRITE_PIXEL_GEOMETRY_FLAT && o.pg <= DWRITE_PIXEL_GEOMETRY_BGR, "pixel geometry %d", o.pg);
    t_note("default pixel geometry %d (0 FLAT, 1 RGB, 2 BGR)", o.pg);
    DW_RELEASE(p);
}
TEST(rpd_default_rendering_mode_is_default) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    /* rendering mode is never taken from the registry: it is DEFAULT so that GetRecommendedRenderingMode decides */
    EXPECT_EQ(o.mode, DWRITE_RENDERING_MODE_DEFAULT);
    EXPECT_EQ(o.mode1, DWRITE_RENDERING_MODE1_DEFAULT);
    DW_RELEASE(p);
}
TEST(rpd_default_grid_fit_is_default) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    EXPECT_EQ(o.grid, DWRITE_GRID_FIT_MODE_DEFAULT);
    DW_RELEASE(p);
}
TEST(rpd_default_values_note) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    t_note("defaults: gamma=%g ec=%g gec=%g ct=%g pg=%d mode=%d mode1=%d grid=%d (untuned Win10: 1.8 0.5 1 1 RGB DEFAULT)", (double)o.gamma, (double)o.ec, (double)o.gec, (double)o.ct, o.pg, o.mode, o.mode1, o.grid);
    DW_RELEASE(p);
}
TEST(rpd_default_finite_values) {
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    CHECK(isfinite(o.gamma) && isfinite(o.ec) && isfinite(o.gec) && isfinite(o.ct));
    CHECK(o.gamma > 0.0f);
    DW_RELEASE(p);
}
TEST(rpd_default_stable_between_calls) {
    rp_out a, b;
    if (!read_default(DWRITE_FACTORY_TYPE_SHARED, &a) || !read_default(DWRITE_FACTORY_TYPE_SHARED, &b)) { t_skip("create failed"); return; }
    RP_CMP(a, b, "second CreateRenderingParams");
}
TEST(rpd_default_same_for_isolated_and_shared) {
    rp_out a, b;
    if (!read_default(DWRITE_FACTORY_TYPE_SHARED, &a) || !read_default(DWRITE_FACTORY_TYPE_ISOLATED, &b)) { t_skip("create failed"); return; }
    RP_CMP(a, b, "isolated factory");
}
TEST(rpd_default_same_through_every_factory_version) {
    rp_out base; int v;
    if (!read_default(DWRITE_FACTORY_TYPE_SHARED, &base)) { t_skip("create failed"); return; }
    for (v = 1; v <= 3; v++) {
        void *f = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v); void *p = NULL; rp_out o;
        if (!f) continue;
        if (SUCCEEDED(IDWriteFactory_CreateRenderingParams((IDWriteFactory *)f, (IDWriteRenderingParams **)&p)) && p && rp_read(p, &o)) RP_CMP(o, base, "factory version");
        DW_RELEASE(p);
    }
}
TEST(rpd_default_object_is_fully_featured) {
    NEED_PARAMS(p); rp_out o;
    /* even an object made by the oldest factory method answers the newest interface */
    CHECK(rp_read(p, &o));
    DW_RELEASE(p);
}
TEST(rpd_default_has_no_mode_flag_difference) {
    /* a default-created object has DEFAULT mode so rendering mode 7 never appears through the old getter */
    NEED_PARAMS(p); rp_out o; rp_read(p, &o);
    CHECK(o.mode >= 0 && o.mode <= 6);
    DW_RELEASE(p);
}

/* ---- CreateMonitorRenderingParams ---------------------------------------------------------------------------------- */
static HMONITOR primary_monitor(void) {
    typeof(&MonitorFromWindow) fn = U(MonitorFromWindow);
    return fn ? fn(NULL, MONITOR_DEFAULTTOPRIMARY) : NULL;
}
TEST(rpd_monitor_null_is_default) {
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)RP_SENT;
    /* a NULL monitor takes the same path as CreateRenderingParams */
    CHECK_HR_OK(IDWriteFactory_CreateMonitorRenderingParams(f, NULL, &p));
    CHECK(p != NULL && p != RP_SENT);
    if (p != RP_SENT) DW_RELEASE(p);
}
TEST(rpd_monitor_null_equals_default_values) {
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    void *a = NULL, *b = NULL; rp_out oa, ob;
    CHECK_HR_OK(IDWriteFactory_CreateMonitorRenderingParams(f, NULL, (IDWriteRenderingParams **)&a));
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(f, (IDWriteRenderingParams **)&b));
    if (a && b && rp_read(a, &oa) && rp_read(b, &ob)) RP_CMP(oa, ob, "monitor(NULL) vs default");
    CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(rpd_monitor_primary_ok) {
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    HMONITOR m = primary_monitor();
    if (!m) { t_skip("no primary monitor"); return; }
    IDWriteRenderingParams *p = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateMonitorRenderingParams(f, m, &p));
    CHECK(p != NULL);
    if (p) EXPECT_REFS(p, 1);
    DW_RELEASE(p);
}
TEST(rpd_monitor_primary_values_in_range) {
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    HMONITOR m = primary_monitor();
    if (!m) { t_skip("no primary monitor"); return; }
    void *p = NULL; rp_out o;
    DW_REQUIRE_OK(IDWriteFactory_CreateMonitorRenderingParams(f, m, (IDWriteRenderingParams **)&p));
    CHECK(rp_read(p, &o));
    CHECKF(o.gamma >= 1.0f && o.gamma <= 2.2f, "gamma %g", (double)o.gamma);
    CHECKF(o.ec >= 0.0f && o.ec <= 4.0f, "ec %g", (double)o.ec);
    CHECKF(o.gec >= 0.0f && o.gec <= 4.0f, "gec %g", (double)o.gec);
    CHECKF(o.ct >= 0.0f && o.ct <= 1.0f, "ct %g", (double)o.ct);
    CHECK(o.pg >= 0 && o.pg <= 2);
    EXPECT_EQ(o.mode, 0); EXPECT_EQ(o.mode1, 0); EXPECT_EQ(o.grid, 0);
    DW_RELEASE(p);
}
TEST(rpd_monitor_primary_vs_default_note) {
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    HMONITOR m = primary_monitor();
    if (!m) { t_skip("no primary monitor"); return; }
    void *a = NULL, *b = NULL; rp_out oa, ob;
    DW_REQUIRE_OK(IDWriteFactory_CreateMonitorRenderingParams(f, m, (IDWriteRenderingParams **)&a));
    if (rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &b) && rp_read(a, &oa) && rp_read(b, &ob)) {
        /* the default is the first attached display device, which is normally the primary monitor */
        t_note("primary monitor vs default: gamma %g/%g ec %g/%g ct %g/%g pg %d/%d", (double)oa.gamma, (double)ob.gamma, (double)oa.ec, (double)ob.ec, (double)oa.ct, (double)ob.ct, oa.pg, ob.pg);
    }
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(rpd_monitor_primary_each_factory_version) {
    HMONITOR m = primary_monitor(); int v;
    if (!m) { t_skip("no primary monitor"); return; }
    for (v = 0; v <= 3; v++) {
        void *f = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v); IDWriteRenderingParams *p = NULL;
        if (!f) continue;
        CHECK_HR_OK(IDWriteFactory_CreateMonitorRenderingParams((IDWriteFactory *)f, m, &p));
        CHECK(p != NULL);
        DW_RELEASE(p);
    }
}
TEST(rpd_monitor_primary_isolated) {
    IDWriteFactory *f = dw_isolated(); REQUIRE(f);
    HMONITOR m = primary_monitor();
    if (!m) { t_skip("no primary monitor"); return; }
    IDWriteRenderingParams *p = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateMonitorRenderingParams(f, m, &p));
    CHECK(p != NULL);
    DW_RELEASE(p);
}
TEST(rpd_monitor_from_point_and_window_agree) {
    /* the monitor under the origin is the primary monitor; both HMONITORs produce valid params */
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    typeof(&MonitorFromPoint) fn = U(MonitorFromPoint);
    if (!fn) { t_skip("MonitorFromPoint missing"); return; }
    POINT pt = { 0, 0 };
    HMONITOR m = fn(pt, MONITOR_DEFAULTTOPRIMARY);
    if (!m) { t_skip("no monitor"); return; }
    IDWriteRenderingParams *p = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateMonitorRenderingParams(f, m, &p));
    CHECK(p != NULL);
    DW_RELEASE(p);
}
TEST(rpd_monitor_repeated_distinct_objects) {
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    HMONITOR m = primary_monitor();
    if (!m) { t_skip("no primary monitor"); return; }
    IDWriteRenderingParams *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateMonitorRenderingParams(f, m, &a));
    DW_REQUIRE_OK(IDWriteFactory_CreateMonitorRenderingParams(f, m, &b));
    CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(rpd_monitor_invalid_handle_not_called) {
    /* An HMONITOR that GetMonitorInfo rejects makes CreateFromMonitor throw a GdiException and the factory method has no
     * handler, so the behaviour is not a contract an application can rely on: deliberately not exercised. */
    t_note("invalid HMONITOR is not exercised (C++ exception escapes the factory method)");
}

/* ---- survival / lifetime -------------------------------------------------------------------------------------------- */
TEST(rpd_survives_factory_release) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("factory unavailable"); return; }
    void *p = NULL; rp_out before, after;
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(f, (IDWriteRenderingParams **)&p));
    rp_read(p, &before);
    IDWriteFactory_Release(f);
    /* the object carries no reference to the factory that made it */
    CHECK(rp_read(p, &after));
    RP_CMP(after, before, "after factory release");
    EXPECT_REFS(p, 1);
    DW_RELEASE(p);
}
TEST(rpd_getters_repeatable) {
    NEED_PARAMS(p); rp_out first, o; int i;
    rp_read(p, &first);
    for (i = 0; i < 1000; i++) {
        IDWriteRenderingParams *b = (IDWriteRenderingParams *)p;
        if (!rp_fbits(IDWriteRenderingParams_GetGamma(b), first.gamma) || !rp_fbits(IDWriteRenderingParams_GetEnhancedContrast(b), first.ec) ||
            !rp_fbits(IDWriteRenderingParams_GetClearTypeLevel(b), first.ct) || IDWriteRenderingParams_GetPixelGeometry(b) != (DWRITE_PIXEL_GEOMETRY)first.pg ||
            IDWriteRenderingParams_GetRenderingMode(b) != (DWRITE_RENDERING_MODE)first.mode) { CHECKF(0, "getter changed at iteration %d", i); break; }
    }
    rp_read(p, &o);
    RP_CMP(o, first, "after 1000 reads");
    DW_RELEASE(p);
}
TEST(rpd_many_objects_alive) {
    enum { N = 500 };
    void **objs = calloc(N, sizeof(void *)); int i, distinct = 1;
    if (!objs) { t_skip("alloc"); return; }
    for (i = 0; i < N; i++) {
        if (!rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &objs[i])) { CHECKF(0, "create %d failed", i); break; }
    }
    for (i = 1; i < N && objs[i]; i++) if (objs[i] == objs[i - 1]) distinct = 0;
    CHECK(distinct);
    for (i = 0; i < N; i++) DW_RELEASE(objs[i]);
    free(objs);
}
