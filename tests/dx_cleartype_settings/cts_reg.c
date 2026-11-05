/* Registry side of the text-rendering settings.
 *  - read-only inspection of HKCU\Control Panel\Desktop and HKCU/HKLM\Software\Microsoft\Avalon.Graphics (notes only);
 *  - scratch values: HKCU\Software\Microsoft\Avalon.Graphics\<device> values that the test itself creates (only when they do
 *    not exist yet) and removes again.  The key of the display device DirectWrite looks at is used:
 *      default params  -> first display device attached to the desktop
 *      monitor params  -> the device name of the HMONITOR (GetMonitorInfo szDevice)
 *    Value rules (decompilation): GammaLevel 1000..2200 x0.001; ClearTypeLevel 0..100, EnhancedContrastLevel 0..400,
 *    GrayscaleEnhancedContrastLevel 0..400 all x0.01; PixelStructure 0..2; out-of-range values are ignored and the previous
 *    default stays; TextContrastLevel is not consulted. */
#include "cts.h"

/* ---- inspection (notes) ------------------------------------------------------------------------------------------------- */
TEST(reg_desktop_font_smoothing_values_note) {
    static const char *names[] = { "FontSmoothing", "FontSmoothingType", "FontSmoothingGamma", "FontSmoothingOrientation", "FontSmoothingCleartype", "FontSmoothingContrast" };
    int i;
    for (i = 0; i < 6; i++) {
        char text[128]; int t = cts_reg_any(HKEY_CURRENT_USER, CTS_DESKTOP, names[i], text, sizeof text);
        if (t) t_note("HKCU\\%s\\%s = %s (type %d)", CTS_DESKTOP, names[i], text, t); else t_note("HKCU\\%s\\%s absent", CTS_DESKTOP, names[i]);
    }
}
TEST(reg_desktop_vs_spi_note) {
    char text[64]; int t = cts_reg_any(HKEY_CURRENT_USER, CTS_DESKTOP, "FontSmoothing", text, sizeof text);
    t_note("registry FontSmoothing %s%s -> SPI_GETFONTSMOOTHING %d", t ? "= " : "absent", text, (int)cts_get_smoothing());
    t = cts_reg_any(HKEY_CURRENT_USER, CTS_DESKTOP, "FontSmoothingType", text, sizeof text);
    t_note("registry FontSmoothingType %s%s -> SPI_GETFONTSMOOTHINGTYPE %u", t ? "= " : "absent", text, cts_get_type());
    t = cts_reg_any(HKEY_CURRENT_USER, CTS_DESKTOP, "FontSmoothingOrientation", text, sizeof text);
    t_note("registry FontSmoothingOrientation %s%s -> SPI_GETFONTSMOOTHINGORIENTATION %u", t ? "= " : "absent", text, cts_get_orient());
    t = cts_reg_any(HKEY_CURRENT_USER, CTS_DESKTOP, "FontSmoothingGamma", text, sizeof text);
    t_note("registry FontSmoothingGamma %s%s -> SPI_GETFONTSMOOTHINGCONTRAST %u", t ? "= " : "absent", text, cts_get_contrast());
}
TEST(reg_avalon_graphics_keys_note) {
    HKEY roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE }; static const char *rn[] = { "HKCU", "HKLM" };
    static const char *vals[] = { "ClearTypeLevel", "GammaLevel", "PixelStructure", "TextContrastLevel", "EnhancedContrastLevel", "GrayscaleEnhancedContrastLevel" };
    int r;
    for (r = 0; r < 2; r++) {
        HKEY base; DWORD i;
        if (RegOpenKeyExA(roots[r], CTS_AV_BASE, 0, KEY_READ, &base) != ERROR_SUCCESS) { t_note("%s\\%s absent", rn[r], CTS_AV_BASE); continue; }
        for (i = 0; i < 64; i++) {
            char sub[128], path[300]; DWORD cch = sizeof sub; int v;
            if (RegEnumKeyExA(base, i, sub, &cch, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
            snprintf(path, sizeof path, "%s\\%s", CTS_AV_BASE, sub);
            for (v = 0; v < 6; v++) {
                char text[64]; int t = cts_reg_any(roots[r], path, vals[v], text, sizeof text);
                if (t) t_note("%s\\%s : %s = %s (type %d)", rn[r], path, vals[v], text, t);
            }
        }
        RegCloseKey(base);
    }
}
TEST(reg_avalon_values_in_valid_range_note) {
    /* values outside the readable ranges are ignored by DirectWrite; report any the machine carries */
    static const struct { const char *n; DWORD lo, hi; } rng[] = { { "GammaLevel", 1000, 2200 }, { "ClearTypeLevel", 0, 100 }, { "EnhancedContrastLevel", 0, 400 }, { "GrayscaleEnhancedContrastLevel", 0, 400 }, { "PixelStructure", 0, 2 } };
    char dev[64]; int i;
    if (!cts_first_attached(dev, sizeof dev)) { t_skip("no attached display device"); return; }
    for (i = 0; i < 5; i++) {
        char path[300]; DWORD v;
        snprintf(path, sizeof path, "%s\\%s", CTS_AV_BASE, dev);
        if (cts_reg_dword(HKEY_CURRENT_USER, path, rng[i].n, &v) && (v < rng[i].lo || v > rng[i].hi)) t_note("%s=%lu outside %lu..%lu: ignored by DirectWrite", rng[i].n, v, rng[i].lo, rng[i].hi);
    }
}
TEST(reg_display_devices_and_keys_note) {
    DISPLAY_DEVICEA dd; DWORD i;
    for (i = 0; i < 16; i++) {
        char path[300]; HKEY k; int has;
        memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
        if (!EnumDisplayDevicesA(NULL, i, &dd, 0)) break;
        {
            const char *n = dd.DeviceName; while (n[0] == '\\' || (n[0] == '.' && n[1] == '\\')) n += (n[0] == '\\') ? 1 : 2;
            snprintf(path, sizeof path, "%s\\%s", CTS_AV_BASE, n);
            has = RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) == ERROR_SUCCESS; if (has) RegCloseKey(k);
            t_note("display device %lu: %s flags %#lx, HKCU key %s", i, dd.DeviceName, dd.StateFlags, has ? "present" : "absent");
        }
    }
}
TEST(reg_params_follow_existing_registry_values) {
    /* with a key for the device DirectWrite consults, each readable value shows up in the defaults */
    char dev[64], path[300]; cts_pv v; DWORD x;
    if (!cts_first_attached(dev, sizeof dev)) { t_skip("no attached display device"); return; }
    if (!cts_default(&v)) { t_skip("create failed"); return; }
    snprintf(path, sizeof path, "%s\\%s", CTS_AV_BASE, dev);
    if (cts_reg_dword(HKEY_CURRENT_USER, path, "ClearTypeLevel", &x) && x <= 100) CHECKF(v.ct == (float)x * 0.01f, "ClearTypeLevel %lu -> %g", x, (double)v.ct);
    if (cts_reg_dword(HKEY_CURRENT_USER, path, "EnhancedContrastLevel", &x) && x <= 400) CHECKF(v.ec == (float)x * 0.01f, "EnhancedContrastLevel %lu -> %g", x, (double)v.ec);
    if (cts_reg_dword(HKEY_CURRENT_USER, path, "GrayscaleEnhancedContrastLevel", &x) && x <= 400) CHECKF(v.gec == (float)x * 0.01f, "GrayscaleEnhancedContrastLevel %lu -> %g", x, (double)v.gec);
    if (cts_reg_dword(HKEY_CURRENT_USER, path, "GammaLevel", &x) && x >= 1000 && x <= 2200) CHECKF(v.gamma == (float)x * 0.001f, "GammaLevel %lu -> %g", x, (double)v.gamma);
    if (cts_reg_dword(HKEY_CURRENT_USER, path, "PixelStructure", &x) && x <= 2) CHECKF(v.pg == (int)x, "PixelStructure %lu -> %d", x, v.pg);
}

/* ---- scratch-value cases ------------------------------------------------------------------------------------------------- */
enum { F_CT, F_GAMMA, F_EC, F_GEC, F_PG };
static float field_of(const cts_pv *v, int f) { return f == F_CT ? v->ct : f == F_GAMMA ? v->gamma : f == F_EC ? v->ec : f == F_GEC ? v->gec : (float)v->pg; }

/* devkind 0: CreateRenderingParams (first attached device); 1: CreateMonitorRenderingParams(primary).  scale > 0: expected = value * scale;
 * scale == 0: pixel geometry value taken as is; ignored: the value is out of range and the baseline must remain. */
static void av_case(int devkind, const char *name, DWORD value, int field, float scale, int ignored) {
    char dev[64]; cts_pv base, now; HMONITOR m = NULL; float want;
    if (devkind == 0) { if (!cts_first_attached(dev, sizeof dev)) { t_skip("no attached display device"); return; } }
    else { m = cts_primary_monitor(); if (!m || !cts_monitor_device(m, dev, sizeof dev)) { t_skip("no primary monitor"); return; } }
    if (!(devkind == 0 ? cts_default(&base) : cts_monitor_params(m, &base))) { t_skip("create failed"); return; }
    if (!cts_av_open(dev)) { t_skip("HKCU\\%s not writable", CTS_AV_BASE); return; }
    if (!cts_av_set(name, value)) { cts_av_close(); t_skip("%s already exists for %s; not touching it", name, dev); return; }
    if (!(devkind == 0 ? cts_default(&now) : cts_monitor_params(m, &now))) { cts_av_close(); t_skip("create failed"); return; }
    want = ignored ? field_of(&base, field) : (scale == 0.0f ? (float)value : (float)value * scale);
    CHECKF(field_of(&now, field) == want, "%s=%lu on %s: got %g want %g", name, (unsigned long)value, dev, (double)field_of(&now, field), (double)want);
    /* the other readable fields do not move (PixelStructure aside, which has no sibling) */
    if (field != F_CT) CHECK(now.ct == base.ct);
    if (field != F_GAMMA) CHECK(now.gamma == base.gamma);
    if (field != F_EC) CHECK(now.ec == base.ec);
    if (field != F_GEC) CHECK(now.gec == base.gec);
    if (field != F_PG) CHECK(now.pg == base.pg);
    CHECK(now.mode == 0 && now.mode1 == 0 && now.grid == 0);
    cts_av_close();
    /* removing the value brings the baseline back: nothing is cached */
    (devkind == 0 ? cts_default(&now) : cts_monitor_params(m, &now));
    CHECK(cts_same(&now, &base));
}
#define AVC(tn, kind, name, val, fld, scale, ign) TEST(tn) { av_case(kind, name, val, fld, scale, ign); }

/* ClearTypeLevel: 0..100 x0.01, HKCU only */
AVC(av_ct_0_default,        0, "ClearTypeLevel", 0,    F_CT, 0.01f, 0)
AVC(av_ct_1_default,        0, "ClearTypeLevel", 1,    F_CT, 0.01f, 0)
AVC(av_ct_25_default,       0, "ClearTypeLevel", 25,   F_CT, 0.01f, 0)
AVC(av_ct_50_default,       0, "ClearTypeLevel", 50,   F_CT, 0.01f, 0)
AVC(av_ct_99_default,       0, "ClearTypeLevel", 99,   F_CT, 0.01f, 0)
AVC(av_ct_100_default,      0, "ClearTypeLevel", 100,  F_CT, 0.01f, 0)
AVC(av_ct_101_ignored,      0, "ClearTypeLevel", 101,  F_CT, 0,     1)
AVC(av_ct_1000_ignored,     0, "ClearTypeLevel", 1000, F_CT, 0,     1)
AVC(av_ct_max_ignored,      0, "ClearTypeLevel", 0xFFFFFFFFu, F_CT, 0, 1)
AVC(av_ct_0_monitor,        1, "ClearTypeLevel", 0,    F_CT, 0.01f, 0)
AVC(av_ct_50_monitor,       1, "ClearTypeLevel", 50,   F_CT, 0.01f, 0)
AVC(av_ct_100_monitor,      1, "ClearTypeLevel", 100,  F_CT, 0.01f, 0)
AVC(av_ct_101_monitor_ign,  1, "ClearTypeLevel", 101,  F_CT, 0,     1)
/* GammaLevel: 1000..2200 x0.001 */
AVC(av_gamma_1000_default,  0, "GammaLevel", 1000, F_GAMMA, 0.001f, 0)
AVC(av_gamma_1400_default,  0, "GammaLevel", 1400, F_GAMMA, 0.001f, 0)
AVC(av_gamma_1800_default,  0, "GammaLevel", 1800, F_GAMMA, 0.001f, 0)
AVC(av_gamma_2000_default,  0, "GammaLevel", 2000, F_GAMMA, 0.001f, 0)
AVC(av_gamma_2200_default,  0, "GammaLevel", 2200, F_GAMMA, 0.001f, 0)
AVC(av_gamma_999_ignored,   0, "GammaLevel", 999,  F_GAMMA, 0,      1)
AVC(av_gamma_0_ignored,     0, "GammaLevel", 0,    F_GAMMA, 0,      1)
AVC(av_gamma_2201_ignored,  0, "GammaLevel", 2201, F_GAMMA, 0,      1)
AVC(av_gamma_max_ignored,   0, "GammaLevel", 0xFFFFFFFFu, F_GAMMA, 0, 1)
AVC(av_gamma_1000_monitor,  1, "GammaLevel", 1000, F_GAMMA, 0.001f, 0)
AVC(av_gamma_2200_monitor,  1, "GammaLevel", 2200, F_GAMMA, 0.001f, 0)
AVC(av_gamma_2201_monitor_ign, 1, "GammaLevel", 2201, F_GAMMA, 0,   1)
/* EnhancedContrastLevel: 0..400 x0.01 */
AVC(av_ec_0_default,        0, "EnhancedContrastLevel", 0,   F_EC, 0.01f, 0)
AVC(av_ec_50_default,       0, "EnhancedContrastLevel", 50,  F_EC, 0.01f, 0)
AVC(av_ec_100_default,      0, "EnhancedContrastLevel", 100, F_EC, 0.01f, 0)
AVC(av_ec_250_default,      0, "EnhancedContrastLevel", 250, F_EC, 0.01f, 0)
AVC(av_ec_400_default,      0, "EnhancedContrastLevel", 400, F_EC, 0.01f, 0)
AVC(av_ec_401_ignored,      0, "EnhancedContrastLevel", 401, F_EC, 0,     1)
AVC(av_ec_max_ignored,      0, "EnhancedContrastLevel", 0xFFFFFFFFu, F_EC, 0, 1)
AVC(av_ec_0_monitor,        1, "EnhancedContrastLevel", 0,   F_EC, 0.01f, 0)
AVC(av_ec_400_monitor,      1, "EnhancedContrastLevel", 400, F_EC, 0.01f, 0)
AVC(av_ec_401_monitor_ign,  1, "EnhancedContrastLevel", 401, F_EC, 0,     1)
/* GrayscaleEnhancedContrastLevel: 0..400 x0.01 (IDWriteRenderingParams1) */
AVC(av_gec_0_default,       0, "GrayscaleEnhancedContrastLevel", 0,   F_GEC, 0.01f, 0)
AVC(av_gec_100_default,     0, "GrayscaleEnhancedContrastLevel", 100, F_GEC, 0.01f, 0)
AVC(av_gec_250_default,     0, "GrayscaleEnhancedContrastLevel", 250, F_GEC, 0.01f, 0)
AVC(av_gec_400_default,     0, "GrayscaleEnhancedContrastLevel", 400, F_GEC, 0.01f, 0)
AVC(av_gec_401_ignored,     0, "GrayscaleEnhancedContrastLevel", 401, F_GEC, 0,     1)
AVC(av_gec_400_monitor,     1, "GrayscaleEnhancedContrastLevel", 400, F_GEC, 0.01f, 0)
/* PixelStructure: 0 flat, 1 RGB, 2 BGR; 3 and above ignored */
AVC(av_ps_0_default,        0, "PixelStructure", 0, F_PG, 0, 0)
AVC(av_ps_1_default,        0, "PixelStructure", 1, F_PG, 0, 0)
AVC(av_ps_2_default,        0, "PixelStructure", 2, F_PG, 0, 0)
AVC(av_ps_0_monitor,        1, "PixelStructure", 0, F_PG, 0, 0)
AVC(av_ps_2_monitor,        1, "PixelStructure", 2, F_PG, 0, 0)
AVC(av_ps_3_ignored,        0, "PixelStructure", 3, F_PG, 0, 1)
AVC(av_ps_max_ignored,      0, "PixelStructure", 0xFFFFFFFFu, F_PG, 0, 1)
/* TextContrastLevel is not consulted by the parameter factory: no field moves */
AVC(av_tcl_0_ec_unchanged,    0, "TextContrastLevel", 0,   F_EC, 0, 1)
AVC(av_tcl_100_ec_unchanged,  0, "TextContrastLevel", 100, F_EC, 0, 1)
AVC(av_tcl_400_ec_unchanged,  0, "TextContrastLevel", 400, F_EC, 0, 1)
AVC(av_tcl_100_gamma_unchanged, 0, "TextContrastLevel", 100, F_GAMMA, 0, 1)
AVC(av_tcl_monitor_ec_unchanged, 1, "TextContrastLevel", 100, F_EC, 0, 1)

/* ---- behaviours of the registry layer ---------------------------------------------------------------------------------------- */
TEST(av_pixel_structure_overrides_orientation) {
    char dev[64]; cts_pv v; int o;
    CTS_BEGIN();
    if (!cts_first_attached(dev, sizeof dev)) { cts_end(); t_skip("no attached display device"); return; }
    if (!cts_av_open(dev)) { cts_end(); t_skip("registry not writable"); return; }
    if (!cts_av_set("PixelStructure", 0)) { cts_av_close(); cts_end(); t_skip("PixelStructure exists"); return; }
    for (o = 0; o < 2; o++) {
        CTS_SET_OR_SKIP(cts_set_orient(o));
        if (cts_default(&v)) CHECKF(v.pg == DWRITE_PIXEL_GEOMETRY_FLAT, "orientation %d: pg %d (a valid PixelStructure replaces the SPI orientation)", o, v.pg);
    }
    cts_av_close(); cts_end();
}
TEST(av_invalid_pixel_structure_falls_back_to_orientation) {
    char dev[64]; cts_pv v; int o;
    CTS_BEGIN();
    if (cts_avalon_has_value("PixelStructure")) { cts_end(); t_skip("PixelStructure present on this machine"); return; }
    if (!cts_first_attached(dev, sizeof dev)) { cts_end(); t_skip("no attached display device"); return; }
    if (!cts_av_open(dev)) { cts_end(); t_skip("registry not writable"); return; }
    if (!cts_av_set("PixelStructure", 3)) { cts_av_close(); cts_end(); t_skip("cannot write"); return; }
    for (o = 0; o < 2; o++) {
        CTS_SET_OR_SKIP(cts_set_orient(o));
        if (cts_default(&v)) CHECKF(v.pg == (o ? DWRITE_PIXEL_GEOMETRY_RGB : DWRITE_PIXEL_GEOMETRY_BGR), "orientation %d: pg %d", o, v.pg);
    }
    cts_av_close(); cts_end();
}
TEST(av_values_read_fresh_each_time) {
    char dev[64]; cts_pv a, b, c;
    if (!cts_first_attached(dev, sizeof dev) || !cts_av_open(dev)) { t_skip("registry not usable"); return; }
    if (!cts_av_set("ClearTypeLevel", 20)) { cts_av_close(); t_skip("ClearTypeLevel exists"); return; }
    cts_default(&a);
    cts_av_unset("ClearTypeLevel");
    if (!cts_av_set("ClearTypeLevel", 80)) { cts_av_close(); t_skip("cannot rewrite"); return; }
    cts_default(&b);
    cts_av_close();
    cts_default(&c);
    CHECK(a.ct == 20 * 0.01f);
    CHECK(b.ct == 80 * 0.01f);
    CHECK(c.ct != a.ct || c.ct != b.ct);
}
TEST(av_existing_object_not_updated_by_registry_change) {
    char dev[64]; IDWriteRenderingParams *p = NULL; cts_pv before, after;
    if (!dw_shared() || !cts_first_attached(dev, sizeof dev) || !cts_av_open(dev)) { t_skip("registry not usable"); return; }
    if (!cts_av_set("ClearTypeLevel", 30)) { cts_av_close(); t_skip("ClearTypeLevel exists"); return; }
    if (!cts_default_of(dw_shared(), &before, &p)) { cts_av_close(); t_skip("create failed"); return; }
    cts_av_unset("ClearTypeLevel"); cts_av_set("ClearTypeLevel", 70);
    cts_read(p, &after);
    CHECK(cts_same(&before, &after));
    IDWriteRenderingParams_Release(p);
    cts_av_close();
}
TEST(av_key_for_unrelated_device_has_no_effect) {
    char dev[64]; cts_pv a, b;
    if (!cts_first_attached(dev, sizeof dev)) { t_skip("no attached display device"); return; }
    if (!cts_default(&a)) { t_skip("create failed"); return; }
    if (!cts_av_open("DISPLAY99")) { t_skip("registry not writable"); return; }
    if (!lstrcmpiA(dev, "DISPLAY99")) { cts_av_close(); t_skip("unexpected device name"); return; }
    cts_av_set("ClearTypeLevel", 5); cts_av_set("GammaLevel", 1100); cts_av_set("EnhancedContrastLevel", 300); cts_av_set("PixelStructure", 0);
    cts_default(&b);
    CHECK(cts_same(&a, &b));
    cts_av_close();
}
TEST(av_all_values_together_default) {
    char dev[64]; cts_pv v;
    if (!cts_first_attached(dev, sizeof dev) || !cts_av_open(dev)) { t_skip("registry not usable"); return; }
    if (!cts_av_set("ClearTypeLevel", 40) || !cts_av_set("GammaLevel", 1500) || !cts_av_set("EnhancedContrastLevel", 150) ||
        !cts_av_set("GrayscaleEnhancedContrastLevel", 220) || !cts_av_set("PixelStructure", 2)) { cts_av_close(); t_skip("some values already exist"); return; }
    if (cts_default(&v)) {
        CHECK(v.ct == 40 * 0.01f); CHECK(v.gamma == 1500 * 0.001f); CHECK(v.ec == 150 * 0.01f); CHECK(v.gec == 220 * 0.01f); CHECK(v.pg == 2);
        CHECK(v.mode == 0 && v.mode1 == 0 && v.grid == 0);
    }
    cts_av_close();
}
TEST(av_values_apply_to_every_factory_version) {
    char dev[64]; int v;
    if (!cts_first_attached(dev, sizeof dev) || !cts_av_open(dev)) { t_skip("registry not usable"); return; }
    if (!cts_av_set("ClearTypeLevel", 60)) { cts_av_close(); t_skip("exists"); return; }
    for (v = 0; v <= 7; v++) {
        void *f = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v); cts_pv a;
        if (f && cts_default_of((IDWriteFactory *)f, &a, NULL)) CHECKF(a.ct == 60 * 0.01f, "factory v%d ct %g", v, (double)a.ct);
    }
    cts_av_close();
}
TEST(av_values_apply_to_isolated_factory_too) {
    char dev[64]; cts_pv a;
    if (!dw_isolated() || !cts_first_attached(dev, sizeof dev) || !cts_av_open(dev)) { t_skip("registry not usable"); return; }
    if (!cts_av_set("EnhancedContrastLevel", 175)) { cts_av_close(); t_skip("exists"); return; }
    if (cts_default_of(dw_isolated(), &a, NULL)) CHECK(a.ec == 175 * 0.01f);
    cts_av_close();
}
TEST(av_non_dword_value_type_note) {
    /* DirectWrite reads numbers through a typed registry helper; report what a REG_SZ value does */
    char dev[64]; cts_pv a, b; HKEY base, k;
    if (!cts_first_attached(dev, sizeof dev) || !cts_av_open(dev)) { t_skip("registry not usable"); return; }
    if (!cts_default(&a)) { cts_av_close(); t_skip("create failed"); return; }
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CTS_AV_BASE, 0, KEY_READ | KEY_WRITE, &base) == ERROR_SUCCESS) {
        if (RegOpenKeyExA(base, dev, 0, KEY_READ | KEY_WRITE, &k) == ERROR_SUCCESS) {
            DWORD t, cb = 0;
            if (RegQueryValueExA(k, "ClearTypeLevel", NULL, &t, NULL, &cb) == ERROR_FILE_NOT_FOUND) {
                RegSetValueExA(k, "ClearTypeLevel", 0, REG_SZ, (const BYTE *)"50", 3);
                cts_default(&b);
                t_note("REG_SZ ClearTypeLevel=\"50\" -> ct %g (default %g)", (double)b.ct, (double)a.ct);
                RegDeleteValueA(k, "ClearTypeLevel");
            }
            RegCloseKey(k);
        }
        RegCloseKey(base);
    }
    cts_av_close();
}
TEST(av_scratch_cleanup_leaves_no_trace) {
    char dev[64]; HKEY k; int existed_base, existed_dev;
    if (!cts_first_attached(dev, sizeof dev)) { t_skip("no attached display device"); return; }
    {
        char path[300]; snprintf(path, sizeof path, "%s\\%s", CTS_AV_BASE, dev);
        existed_base = RegOpenKeyExA(HKEY_CURRENT_USER, CTS_AV_BASE, 0, KEY_READ, &k) == ERROR_SUCCESS; if (existed_base) RegCloseKey(k);
        existed_dev = RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) == ERROR_SUCCESS; if (existed_dev) RegCloseKey(k);
        if (!cts_av_open(dev)) { t_skip("registry not writable"); return; }
        cts_av_set("ClearTypeLevel", 10);
        cts_av_close();
        {
            DWORD v; int base_now = RegOpenKeyExA(HKEY_CURRENT_USER, CTS_AV_BASE, 0, KEY_READ, &k) == ERROR_SUCCESS; if (base_now) RegCloseKey(k);
            int dev_now = RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_READ, &k) == ERROR_SUCCESS; if (dev_now) RegCloseKey(k);
            CHECK(base_now == existed_base);
            CHECK(dev_now == existed_dev);
            if (!existed_dev) CHECK(!cts_reg_dword(HKEY_CURRENT_USER, path, "ClearTypeLevel", &v));
        }
    }
}
