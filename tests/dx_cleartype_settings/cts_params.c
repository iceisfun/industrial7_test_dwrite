/* IDWriteFactory::CreateRenderingParams / CreateMonitorRenderingParams as a function of the system font smoothing settings.
 * See cts.h for the decompilation facts: only SPI_GETFONTSMOOTHINGORIENTATION feeds the reported pixel geometry (unless a
 * PixelStructure registry value overrides it); smoothing flag, type and contrast never change the reported getters.  Objects are
 * created fresh on every call and never change afterwards. */
#include "cts.h"

#define NEED_DEFAULT(v) cts_pv v; if (!dw_shared() || !cts_default(&v)) { t_skip("CreateRenderingParams failed"); return; }
/* The pixel-geometry rules below only hold without a PixelStructure registry override. */
#define NEED_NO_PIXSTRUCT() do { if (cts_avalon_has_value("PixelStructure")) { cts_end(); t_skip("a PixelStructure registry value overrides the orientation"); return; } } while (0)

/* ---- defaults (no setting changed) ------------------------------------------------------------------------------- */
TEST(par_default_mode_is_default) {
    NEED_DEFAULT(v);
    EXPECT_EQ(v.mode, DWRITE_RENDERING_MODE_DEFAULT);
    EXPECT_EQ(v.mode1, 0);
    EXPECT_EQ(v.grid, DWRITE_GRID_FIT_MODE_DEFAULT);
}
TEST(par_default_values_in_registry_clamped_ranges) {
    NEED_DEFAULT(v);
    /* the registry readers accept GammaLevel 1000..2200 (x0.001), ClearTypeLevel 0..100 and contrast 0..400 (x0.01) */
    CHECKF(v.gamma >= 1.0f && v.gamma <= 2.2f + 1e-6f, "gamma %g", (double)v.gamma);
    CHECKF(v.ec >= 0.0f && v.ec <= 4.0f + 1e-6f, "enhanced contrast %g", (double)v.ec);
    CHECKF(v.gec >= 0.0f && v.gec <= 4.0f + 1e-6f, "grayscale enhanced contrast %g", (double)v.gec);
    CHECKF(v.ct >= 0.0f && v.ct <= 1.0f + 1e-6f, "cleartype level %g", (double)v.ct);
    CHECKF(v.pg == DWRITE_PIXEL_GEOMETRY_FLAT || v.pg == DWRITE_PIXEL_GEOMETRY_RGB || v.pg == DWRITE_PIXEL_GEOMETRY_BGR, "pixel geometry %d", v.pg);
    cts_dump("default", &v);
}
TEST(par_default_builtin_numbers_without_avalon_keys) {
    NEED_DEFAULT(v);
    if (!cts_no_avalon()) { t_skip("Avalon.Graphics keys exist; built-in defaults are overridden"); return; }
    CHECK(v.gamma == 1.8f);
    CHECK(v.ec == 0.5f);
    CHECK(v.gec == 1.0f);
    CHECK(v.ct == 1.0f);
}
TEST(par_default_have_all_param_interfaces_note) {
    NEED_DEFAULT(v);
    t_note("rendering params interfaces: 1=%d 2=%d 3=%d", v.have1, v.have2, v.have3);
    /* the system parameter object implements IDWriteRenderingParams1 (Win8 platform update) and 2 (Win8.1); 3 arrived later */
    CHECK(v.have1);
    CHECK(v.have2);
}
TEST(par_default_repeat_is_stable) {
    cts_pv a, b;
    if (!dw_shared() || !cts_default(&a) || !cts_default(&b)) { t_skip("create failed"); return; }
    CHECK(cts_same(&a, &b));
}

/* ---- orientation drives pixel geometry --------------------------------------------------------------------------------- */
TEST(par_orientation_bgr_reports_bgr_geometry) {
    cts_pv v;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    if (!cts_default(&v)) { cts_end(); t_skip("create failed"); return; }
    EXPECT_EQ(v.pg, DWRITE_PIXEL_GEOMETRY_BGR);
    cts_end();
}
TEST(par_orientation_rgb_reports_rgb_geometry) {
    cts_pv v;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    if (!cts_default(&v)) { cts_end(); t_skip("create failed"); return; }
    EXPECT_EQ(v.pg, DWRITE_PIXEL_GEOMETRY_RGB);
    cts_end();
}
TEST(par_orientation_flip_changes_geometry_both_ways) {
    cts_pv a, b, c;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    if (!cts_default(&a)) { cts_end(); t_skip("create failed"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    cts_default(&b);
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    cts_default(&c);
    CHECK(a.pg != b.pg);
    CHECK(a.pg == c.pg);
    cts_end();
}
TEST(par_orientation_other_getters_unchanged) {
    cts_pv a, b;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    if (!cts_default(&a)) { cts_end(); t_skip("create failed"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    cts_default(&b);
    CHECK(a.gamma == b.gamma && a.ec == b.ec && a.gec == b.gec && a.ct == b.ct && a.mode == b.mode && a.mode1 == b.mode1 && a.grid == b.grid);
    cts_end();
}
TEST(par_orientation_seen_by_shared_factory_immediately) {
    /* nothing is cached: the shared factory builds a new object that reads the system state at call time */
    IDWriteFactory *f = dw_shared(); cts_pv a, b;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    if (!f) { cts_end(); t_skip("no factory"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    if (!cts_default_of(f, &a, NULL)) { cts_end(); t_skip("create failed"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    cts_default_of(f, &b, NULL);
    CHECK(a.pg == DWRITE_PIXEL_GEOMETRY_RGB);
    CHECK(b.pg == DWRITE_PIXEL_GEOMETRY_BGR);
    cts_end();
}
TEST(par_orientation_seen_by_isolated_factory_created_before_change) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); cts_pv a, b;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    if (!f) { cts_end(); t_skip("no factory"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    cts_default_of(f, &a, NULL);
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    cts_default_of(f, &b, NULL);
    CHECK(a.pg == DWRITE_PIXEL_GEOMETRY_RGB);
    CHECK(b.pg == DWRITE_PIXEL_GEOMETRY_BGR);
    IDWriteFactory_Release(f);
    cts_end();
}
TEST(par_orientation_seen_by_factory_created_after_change) {
    IDWriteFactory *f; cts_pv a;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { cts_end(); t_skip("no factory"); return; }
    if (cts_default_of(f, &a, NULL)) CHECK(a.pg == DWRITE_PIXEL_GEOMETRY_BGR);
    IDWriteFactory_Release(f);
    cts_end();
}
TEST(par_orientation_isolated_equals_shared) {
    IDWriteFactory *s = dw_shared(), *i = dw_isolated(); cts_pv a, b; int o;
    CTS_BEGIN();
    if (!s || !i) { cts_end(); t_skip("no factory"); return; }
    for (o = 0; o < 2; o++) {
        CTS_SET_OR_SKIP(cts_set_orient(o));
        if (cts_default_of(s, &a, NULL) && cts_default_of(i, &b, NULL)) CHECKF(cts_same(&a, &b), "orientation %d: shared and isolated differ", o);
    }
    cts_end();
}
TEST(par_orientation_every_factory_version) {
    int v, o;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    for (o = 0; o < 2; o++) {
        CTS_SET_OR_SKIP(cts_set_orient(o));
        for (v = 0; v <= 7; v++) {
            void *f = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v); cts_pv a;
            if (!f) continue;
            if (cts_default_of((IDWriteFactory *)f, &a, NULL)) CHECKF(a.pg == (o ? DWRITE_PIXEL_GEOMETRY_RGB : DWRITE_PIXEL_GEOMETRY_BGR), "factory v%d orientation %d pg %d", v, o, a.pg);
        }
    }
    cts_end();
}
TEST(par_orientation_alternating_creates_no_caching) {
    int i;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    for (i = 0; i < 8; i++) {
        cts_pv a;
        CTS_SET_OR_SKIP(cts_set_orient(i & 1));
        if (cts_default(&a)) CHECKF(a.pg == ((i & 1) ? DWRITE_PIXEL_GEOMETRY_RGB : DWRITE_PIXEL_GEOMETRY_BGR), "step %d pg %d", i, a.pg);
    }
    cts_end();
}
TEST(par_orientation_existing_object_keeps_values) {
    IDWriteFactory *f = dw_shared(); IDWriteRenderingParams *p = NULL; cts_pv before, after;
    CTS_BEGIN();
    if (!f) { cts_end(); t_skip("no factory"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    if (!cts_default_of(f, &before, &p)) { cts_end(); t_skip("create failed"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    cts_read(p, &after);
    CHECK(cts_same(&before, &after));
    IDWriteRenderingParams_Release(p);
    cts_end();
}
TEST(par_orientation_object_survives_restore_and_factory_release) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteRenderingParams *p = NULL; cts_pv before, after;
    CTS_BEGIN();
    if (!f) { cts_end(); t_skip("no factory"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    if (!cts_default_of(f, &before, &p)) { IDWriteFactory_Release(f); cts_end(); t_skip("create failed"); return; }
    IDWriteFactory_Release(f);
    cts_end();                                    /* settings back to the original */
    cts_read(p, &after);
    CHECK(cts_same(&before, &after));
    IDWriteRenderingParams_Release(p);
}

/* ---- settings that must NOT change the reported parameters --------------------------------------------------------------- */
TEST(par_smoothing_off_leaves_getters_unchanged) {
    cts_pv a, b;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    if (!cts_default(&a)) { cts_end(); t_skip("create failed"); return; }
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    cts_default(&b);
    CHECK(cts_same(&a, &b));
    cts_end();
}
TEST(par_smoothing_on_leaves_getters_unchanged) {
    cts_pv a, b;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    if (!cts_default(&a)) { cts_end(); t_skip("create failed"); return; }
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    cts_default(&b);
    CHECK(cts_same(&a, &b));
    cts_end();
}
TEST(par_type_change_leaves_getters_unchanged) {
    cts_pv a, b;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_CLEARTYPE));
    if (!cts_default(&a)) { cts_end(); t_skip("create failed"); return; }
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_STANDARD));
    cts_default(&b);
    CHECK(cts_same(&a, &b));
    /* in particular the ClearType level is the registry/built-in value, not a function of the GDI smoothing type */
    CHECK(a.ct == b.ct);
    cts_end();
}
TEST(par_contrast_change_leaves_getters_unchanged) {
    cts_pv a, b; static const UINT v[] = { 1000, 1400, 2200 }; int i;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_contrast(1400));
    if (!cts_default(&a)) { cts_end(); t_skip("create failed"); return; }
    for (i = 0; i < 3; i++) {
        CTS_SET_OR_SKIP(cts_set_contrast(v[i]));
        cts_default(&b);
        /* the GDI contrast only feeds the private compatible-gamma snapshot; the public gamma/contrast getters do not move */
        CHECKF(cts_same(&a, &b), "contrast %u changed the public getters", v[i]);
    }
    cts_end();
}
TEST(par_all_non_orientation_settings_together) {
    cts_pv a, b; int i;
    CTS_BEGIN();
    if (!cts_default(&a)) { cts_end(); t_skip("create failed"); return; }
    for (i = 0; i < 6; i++) {
        CTS_SET_OR_SKIP(cts_set_smoothing(i & 1));
        CTS_SET_OR_SKIP(cts_set_type((i & 2) ? CTS_FE_STANDARD : CTS_FE_CLEARTYPE));
        CTS_SET_OR_SKIP(cts_set_contrast(1000 + 200 * (UINT)i));
        cts_default(&b);
        CHECKF(cts_same(&a, &b), "round %d", i);
    }
    cts_end();
}
TEST(par_rendering_mode_stays_default_for_every_setting) {
    int s, t;
    CTS_BEGIN();
    for (s = 0; s < 2; s++) for (t = 1; t <= 2; t++) {
        cts_pv a;
        CTS_SET_OR_SKIP(cts_set_smoothing(s));
        CTS_SET_OR_SKIP(cts_set_type((UINT)t));
        if (cts_default(&a)) { CHECKF(a.mode == 0 && a.mode1 == 0 && a.grid == 0, "smoothing %d type %d: mode %d/%d grid %d", s, t, a.mode, a.mode1, a.grid); }
    }
    cts_end();
}
TEST(par_gamma_default_independent_of_contrast_setting) {
    cts_pv a;
    CTS_BEGIN();
    if (!cts_no_avalon()) { cts_end(); t_skip("registry overrides gamma"); return; }
    CTS_SET_OR_SKIP(cts_set_contrast(2200));
    if (!cts_default(&a)) { cts_end(); t_skip("create failed"); return; }
    CHECK(a.gamma == 1.8f);
    CTS_SET_OR_SKIP(cts_set_contrast(1000));
    cts_default(&a);
    CHECK(a.gamma == 1.8f);
    cts_end();
}

/* ---- monitor flavour ------------------------------------------------------------------------------------------------------ */
TEST(par_monitor_primary_follows_orientation) {
    HMONITOR m = cts_primary_monitor(); cts_pv a, b;
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    if (!m) { cts_end(); t_skip("no primary monitor"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    if (!cts_monitor_params(m, &a)) { cts_end(); t_skip("CreateMonitorRenderingParams failed"); return; }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    cts_monitor_params(m, &b);
    CHECK(a.pg == DWRITE_PIXEL_GEOMETRY_RGB);
    CHECK(b.pg == DWRITE_PIXEL_GEOMETRY_BGR);
    cts_end();
}
TEST(par_monitor_null_equals_default_under_each_orientation) {
    IDWriteFactory *f = dw_shared(); int o;
    CTS_BEGIN();
    if (!f) { cts_end(); t_skip("no factory"); return; }
    for (o = 0; o < 2; o++) {
        IDWriteRenderingParams *p = NULL; cts_pv a, b;
        CTS_SET_OR_SKIP(cts_set_orient(o));
        /* a NULL monitor takes exactly the CreateRenderingParams path */
        if (FAILED(IDWriteFactory_CreateMonitorRenderingParams(f, NULL, &p)) || !p) continue;
        cts_read(p, &a); IDWriteRenderingParams_Release(p);
        if (cts_default(&b)) CHECKF(cts_same(&a, &b), "orientation %d", o);
    }
    cts_end();
}
TEST(par_monitor_primary_other_getters_unchanged_by_smoothing) {
    HMONITOR m = cts_primary_monitor(); cts_pv a, b;
    CTS_BEGIN();
    if (!m) { cts_end(); t_skip("no primary monitor"); return; }
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    if (!cts_monitor_params(m, &a)) { cts_end(); t_skip("create failed"); return; }
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    cts_monitor_params(m, &b);
    CHECK(cts_same(&a, &b));
    cts_end();
}
TEST(par_monitor_primary_vs_first_attached_device) {
    HMONITOR m = cts_primary_monitor(); char pdev[64], fdev[64]; cts_pv a, b;
    CTS_BEGIN();
    if (!m || !cts_monitor_device(m, pdev, sizeof pdev) || !cts_first_attached(fdev, sizeof fdev)) { cts_end(); t_skip("display devices unavailable"); return; }
    t_note("primary monitor device %s, first attached display device %s", pdev, fdev);
    if (!lstrcmpiA(pdev, fdev) && cts_monitor_params(m, &a) && cts_default(&b)) CHECK(cts_same(&a, &b));
    cts_end();
}
TEST(par_monitor_every_monitor_under_orientation_change) {
    struct { HMONITOR m[8]; int n; } mons; int o, i;
    BOOL CALLBACK cb(HMONITOR h, HDC dc, LPRECT rc, LPARAM l) { typeof(mons) *p = (void *)l; if (p->n < 8) p->m[p->n++] = h; return TRUE; }
    CTS_BEGIN(); NEED_NO_PIXSTRUCT();
    mons.n = 0;
    EnumDisplayMonitors(NULL, NULL, cb, (LPARAM)&mons);
    if (!mons.n) { cts_end(); t_skip("no monitors"); return; }
    t_note("%d monitor(s)", mons.n);
    for (o = 0; o < 2; o++) {
        CTS_SET_OR_SKIP(cts_set_orient(o));
        for (i = 0; i < mons.n; i++) {
            cts_pv a;
            if (cts_monitor_params(mons.m[i], &a)) CHECKF(a.pg == (o ? DWRITE_PIXEL_GEOMETRY_RGB : DWRITE_PIXEL_GEOMETRY_BGR), "monitor %d orientation %d pg %d", i, o, a.pg);
        }
    }
    cts_end();
}

/* ---- custom parameters are independent of the system ---------------------------------------------------------------------- */
TEST(par_custom_params_ignore_orientation) {
    IDWriteFactory *f = dw_shared(); static const int pgs[] = { 0, 1, 2 }; int i, o;
    CTS_BEGIN();
    if (!f) { cts_end(); t_skip("no factory"); return; }
    for (o = 0; o < 2; o++) {
        CTS_SET_OR_SKIP(cts_set_orient(o));
        for (i = 0; i < 3; i++) {
            IDWriteRenderingParams *p = NULL; cts_pv v;
            if (FAILED(IDWriteFactory_CreateCustomRenderingParams(f, 2.0f, 0.25f, 0.5f, (DWRITE_PIXEL_GEOMETRY)pgs[i], DWRITE_RENDERING_MODE_NATURAL, &p)) || !p) continue;
            cts_read(p, &v); IDWriteRenderingParams_Release(p);
            CHECKF(v.pg == pgs[i], "orientation %d requested pg %d got %d", o, pgs[i], v.pg);
            CHECK(v.gamma == 2.0f && v.ec == 0.25f && v.ct == 0.5f && v.mode == DWRITE_RENDERING_MODE_NATURAL);
        }
    }
    cts_end();
}
TEST(par_custom_params_ignore_smoothing_and_type) {
    IDWriteFactory *f = dw_shared(); int s, t;
    CTS_BEGIN();
    if (!f) { cts_end(); t_skip("no factory"); return; }
    for (s = 0; s < 2; s++) for (t = 1; t <= 2; t++) {
        IDWriteRenderingParams *p = NULL; cts_pv v;
        CTS_SET_OR_SKIP(cts_set_smoothing(s)); CTS_SET_OR_SKIP(cts_set_type((UINT)t));
        if (FAILED(IDWriteFactory_CreateCustomRenderingParams(f, 1.5f, 1.0f, 0.75f, DWRITE_PIXEL_GEOMETRY_BGR, DWRITE_RENDERING_MODE_DEFAULT, &p)) || !p) continue;
        cts_read(p, &v); IDWriteRenderingParams_Release(p);
        CHECKF(v.gamma == 1.5f && v.ec == 1.0f && v.ct == 0.75f && v.pg == DWRITE_PIXEL_GEOMETRY_BGR && v.mode == DWRITE_RENDERING_MODE_DEFAULT, "smoothing %d type %d", s, t);
    }
    cts_end();
}
TEST(par_isolated_factory_params_survive_setting_flood) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteRenderingParams *p = NULL; cts_pv a, b; int i;
    CTS_BEGIN();
    if (!f) { cts_end(); t_skip("no factory"); return; }
    if (!cts_default_of(f, &a, &p)) { IDWriteFactory_Release(f); cts_end(); t_skip("create failed"); return; }
    for (i = 0; i < 10; i++) {
        CTS_SET_OR_SKIP(cts_set_smoothing(i & 1)); CTS_SET_OR_SKIP(cts_set_orient((i >> 1) & 1)); CTS_SET_OR_SKIP(cts_set_contrast(1000 + 100 * (UINT)i));
    }
    cts_read(p, &b);
    CHECK(cts_same(&a, &b));
    IDWriteRenderingParams_Release(p); IDWriteFactory_Release(f);
    cts_end();
}
