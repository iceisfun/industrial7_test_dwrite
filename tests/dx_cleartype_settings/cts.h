/* dx_cleartype_settings: system text-rendering settings (SystemParametersInfo font smoothing family, the per-display
 * "Avalon.Graphics" registry keys) and their effect on what DirectWrite hands out.
 *
 * Observed behaviour that these tests rely on:
 *  - CreateRenderingParams (device name NULL) and CreateMonitorRenderingParams(NULL) both describe the
 *    default display; CreateMonitorRenderingParams(hmon) uses the device name of that monitor.  Nothing is cached: every call reads the registry/SPI state again and builds a
 *    fresh object, so a shared factory sees changes immediately and an existing object never changes.
 *  - Built-in defaults: gamma 1.8, enhanced contrast 0.5, grayscale enhanced contrast 1.0, ClearType level 1.0, pixel geometry
 *    RGB (1), rendering mode DEFAULT (0), grid fit DEFAULT (0).
 *  - Registry (only when HKCU\Software\Microsoft\Avalon.Graphics exists): subkey named like the display device without the
 *    "\\.\" prefix (for example DISPLAY1), HKCU then HKLM.  GammaLevel (1000..2200, x0.001) HKCU or HKLM; PixelStructure (0..2)
 *    HKCU or HKLM; ClearTypeLevel (0..100, x0.01), EnhancedContrastLevel (0..400, x0.01), GrayscaleEnhancedContrastLevel (0..400,
 *    x0.01) HKCU only.  Out-of-range values are ignored.  TextContrastLevel is not read.
 *  - Without a valid PixelStructure the pixel geometry comes from SPI_GETFONTSMOOTHINGORIENTATION (0x2012): 0 (BGR) => 2, else 1.
 *  - Every DWriteRenderingParams constructor snapshots DetermineCompatibleSmoothingMode (SPI_GETFONTSMOOTHING 0x4A == 0 => aliased;
 *    else SPI_GETFONTSMOOTHINGTYPE 0x200A == 1 => grayscale; else ClearType) and the compatible gamma (SPI 0x200C) at creation.
 *    GetRecommendedRenderingMode with a DEFAULT-mode params object and DWRITE_MEASURING_MODE_GDI_CLASSIC uses the
 *    snapshot of the object; with a NULL params object it asks the system at call time.
 */
#ifndef CTS_H
#define CTS_H
#include "dwx.h"

#ifndef SPI_GETFONTSMOOTHING
#define SPI_GETFONTSMOOTHING 0x004A
#endif
#ifndef SPI_SETFONTSMOOTHING
#define SPI_SETFONTSMOOTHING 0x004B
#endif
#ifndef SPI_GETFONTSMOOTHINGTYPE
#define SPI_GETFONTSMOOTHINGTYPE 0x200A
#define SPI_SETFONTSMOOTHINGTYPE 0x200B
#endif
#ifndef SPI_GETFONTSMOOTHINGCONTRAST
#define SPI_GETFONTSMOOTHINGCONTRAST 0x200C
#define SPI_SETFONTSMOOTHINGCONTRAST 0x200D
#endif
#ifndef SPI_GETFONTSMOOTHINGORIENTATION
#define SPI_GETFONTSMOOTHINGORIENTATION 0x2012
#define SPI_SETFONTSMOOTHINGORIENTATION 0x2013
#endif
#ifndef SPI_GETCLEARTYPE
#define SPI_GETCLEARTYPE 0x1048
#define SPI_SETCLEARTYPE 0x1049
#endif
#define CTS_FE_STANDARD   1
#define CTS_FE_CLEARTYPE  2
#define CTS_ORIENT_BGR    0
#define CTS_ORIENT_RGB    1

/* ---- SPI state ---------------------------------------------------------------------------------------------------- */
typedef struct { BOOL smoothing; UINT type, contrast, orient; BOOL cleartype; int ok_s, ok_t, ok_c, ok_o, ok_ct; } cts_state;
static cts_state g_cts_saved; static int g_cts_have, g_cts_dirty;

static cts_state cts_read_state(void) {
    cts_state s; memset(&s, 0, sizeof s);
    s.ok_s = SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &s.smoothing, 0) != 0;
    s.ok_t = SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &s.type, 0) != 0;
    s.ok_c = SystemParametersInfoW(SPI_GETFONTSMOOTHINGCONTRAST, 0, &s.contrast, 0) != 0;
    s.ok_o = SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &s.orient, 0) != 0;
    s.ok_ct = SystemParametersInfoW(SPI_GETCLEARTYPE, 0, &s.cleartype, 0) != 0;
    return s;
}
static BOOL cts_set_smoothing(BOOL on) { return SystemParametersInfoW(SPI_SETFONTSMOOTHING, on ? TRUE : FALSE, NULL, SPIF_SENDCHANGE); }
static BOOL cts_set_type(UINT v)       { return SystemParametersInfoW(SPI_SETFONTSMOOTHINGTYPE, 0, (PVOID)(ULONG_PTR)v, SPIF_SENDCHANGE); }
static BOOL cts_set_contrast(UINT v)   { return SystemParametersInfoW(SPI_SETFONTSMOOTHINGCONTRAST, 0, (PVOID)(ULONG_PTR)v, SPIF_SENDCHANGE); }
static BOOL cts_set_orient(UINT v)     { return SystemParametersInfoW(SPI_SETFONTSMOOTHINGORIENTATION, 0, (PVOID)(ULONG_PTR)v, SPIF_SENDCHANGE); }
static BOOL cts_set_cleartype(BOOL on) { return SystemParametersInfoW(SPI_SETCLEARTYPE, 0, (PVOID)(ULONG_PTR)(on ? TRUE : FALSE), SPIF_SENDCHANGE); }
static BOOL cts_get_smoothing(void) { BOOL v = 0; SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &v, 0); return v; }
static UINT cts_get_type(void)      { UINT v = 0xdead; SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &v, 0); return v; }
static UINT cts_get_contrast(void)  { UINT v = 0xdead; SystemParametersInfoW(SPI_GETFONTSMOOTHINGCONTRAST, 0, &v, 0); return v; }
static UINT cts_get_orient(void)    { UINT v = 0xdead; SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &v, 0); return v; }
static BOOL cts_get_cleartype(void) { BOOL v = 0xdead; SystemParametersInfoW(SPI_GETCLEARTYPE, 0, &v, 0); return v; }

/* Put every setting back to the value saved by the first cts_begin of the process. */
static void cts_restore(void) {
    cts_state c;
    if (!g_cts_have) return;
    c = cts_read_state();
    if (g_cts_saved.ok_s && c.smoothing != g_cts_saved.smoothing) cts_set_smoothing(g_cts_saved.smoothing);
    if (g_cts_saved.ok_t && c.type != g_cts_saved.type) cts_set_type(g_cts_saved.type);
    if (g_cts_saved.ok_c && c.contrast != g_cts_saved.contrast) cts_set_contrast(g_cts_saved.contrast);
    if (g_cts_saved.ok_o && c.orient != g_cts_saved.orient) cts_set_orient(g_cts_saved.orient);
    if (g_cts_saved.ok_ct) { c = cts_read_state(); if (c.cleartype != g_cts_saved.cleartype) cts_set_cleartype(g_cts_saved.cleartype); }
    g_cts_dirty = 0;
}
/* Safety net for an abnormal end of a test (exception) or the process (destructor below). */
static void __attribute__((destructor)) cts_atexit(void) { if (g_cts_dirty) cts_restore(); }

/* Start of every test that changes a setting.  Returns 0 (and skips the test) when the settings cannot be queried or set. */
static int cts_begin(void) {
    if (g_cts_dirty) cts_restore();
    if (!g_cts_have) {
        g_cts_saved = cts_read_state();
        g_cts_have = 1;
    }
    if (!g_cts_saved.ok_s || !g_cts_saved.ok_t || !g_cts_saved.ok_o || !g_cts_saved.ok_c) {
        t_skip("SystemParametersInfo font smoothing queries unavailable (s=%d t=%d c=%d o=%d)", g_cts_saved.ok_s, g_cts_saved.ok_t, g_cts_saved.ok_c, g_cts_saved.ok_o);
        return 0;
    }
    g_cts_dirty = 1;       /* from here a restore is due */
    return 1;
}
static void cts_end(void) { cts_restore(); }
#define CTS_BEGIN() do { if (!cts_begin()) return; } while (0)
/* Setter that must work: skips (after restoring) when the session refuses the change. */
#define CTS_SET_OR_SKIP(expr) do { if (!(expr)) { DWORD e_ = GetLastError(); cts_end(); t_skip("%s refused (error %lu)", #expr, e_); return; } } while (0)

/* ---- rendering params reader --------------------------------------------------------------------------------------- */
typedef struct {
    float gamma, ec, ct, gec; int pg, mode, grid, mode1;
    int have1, have2, have3;
} cts_pv;
static int cts_read(IDWriteRenderingParams *p, cts_pv *o) {
    IDWriteRenderingParams1 *p1 = NULL; IDWriteRenderingParams2 *p2 = NULL; IDWriteRenderingParams3 *p3 = NULL;
    memset(o, 0, sizeof *o);
    if (!p) return 0;
    o->gamma = IDWriteRenderingParams_GetGamma(p);
    o->ec = IDWriteRenderingParams_GetEnhancedContrast(p);
    o->ct = IDWriteRenderingParams_GetClearTypeLevel(p);
    o->pg = (int)IDWriteRenderingParams_GetPixelGeometry(p);
    o->mode = (int)IDWriteRenderingParams_GetRenderingMode(p);
    o->gec = 1.0f; o->grid = 0; o->mode1 = o->mode;
    if (SUCCEEDED(IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams1, (void **)&p1)) && p1) {
        o->have1 = 1; o->gec = IDWriteRenderingParams1_GetGrayscaleEnhancedContrast(p1); IDWriteRenderingParams1_Release(p1);
    }
    if (SUCCEEDED(IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams2, (void **)&p2)) && p2) {
        o->have2 = 1; o->grid = (int)IDWriteRenderingParams2_GetGridFitMode(p2); IDWriteRenderingParams2_Release(p2);
    }
    if (SUCCEEDED(IDWriteRenderingParams_QueryInterface(p, &IID_IDWriteRenderingParams3, (void **)&p3)) && p3) {
        o->have3 = 1; o->mode1 = (int)IDWriteRenderingParams3_GetRenderingMode1(p3); IDWriteRenderingParams3_Release(p3);
    }
    return 1;
}
static int cts_same(const cts_pv *a, const cts_pv *b) {
    return a->gamma == b->gamma && a->ec == b->ec && a->ct == b->ct && a->gec == b->gec && a->pg == b->pg && a->mode == b->mode &&
           a->grid == b->grid && a->mode1 == b->mode1;
}
static void cts_dump(const char *tag, const cts_pv *v) {
    t_note("%s: gamma %g ec %g gec %g ct %g pg %d mode %d mode1 %d grid %d", tag, (double)v->gamma, (double)v->ec, (double)v->gec, (double)v->ct, v->pg, v->mode, v->mode1, v->grid);
}
/* Create the system default (CreateRenderingParams) from factory f and read it.  Returns 0 when creation failed. */
static int cts_default_of(IDWriteFactory *f, cts_pv *o, IDWriteRenderingParams **keep) {
    IDWriteRenderingParams *p = NULL;
    HRESULT hr = IDWriteFactory_CreateRenderingParams(f, &p);
    int ok;
    if (FAILED(hr) || !p) return 0;
    ok = cts_read(p, o);
    if (keep) *keep = p; else IDWriteRenderingParams_Release(p);
    return ok;
}
static int cts_default(cts_pv *o) { IDWriteFactory *f = dw_shared(); return f ? cts_default_of(f, o, NULL) : 0; }

static HMONITOR cts_primary_monitor(void) { POINT pt = { 0, 0 }; return MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY); }
static int cts_monitor_params(HMONITOR m, cts_pv *o) {
    IDWriteFactory *f = dw_shared(); IDWriteRenderingParams *p = NULL; int ok;
    if (!f || !m) return 0;
    if (FAILED(IDWriteFactory_CreateMonitorRenderingParams(f, m, &p)) || !p) return 0;
    ok = cts_read(p, o); IDWriteRenderingParams_Release(p);
    return ok;
}

/* ---- registry ------------------------------------------------------------------------------------------------------ */
static int cts_reg_dword(HKEY root, const char *sub, const char *name, DWORD *v) {
    HKEY k; DWORD type = 0, cb = sizeof *v; LONG r; DWORD tmp = 0;
    if (RegOpenKeyExA(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    r = RegQueryValueExA(k, name, NULL, &type, (BYTE *)&tmp, &cb);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS || type != REG_DWORD) return 0;
    *v = tmp; return 1;
}
/* 0 = absent, otherwise the registry type; text receives REG_SZ data or a REG_DWORD rendered as decimal. */
static int cts_reg_any(HKEY root, const char *sub, const char *name, char *text, size_t cap) {
    HKEY k; DWORD type = 0, cb; LONG r; BYTE buf[256];
    text[0] = 0;
    if (RegOpenKeyExA(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    cb = sizeof buf - 1;
    r = RegQueryValueExA(k, name, NULL, &type, buf, &cb);
    RegCloseKey(k);
    if (r != ERROR_SUCCESS) return 0;
    if (type == REG_DWORD && cb >= 4) snprintf(text, cap, "%lu", (unsigned long)*(DWORD *)buf);
    else if (type == REG_SZ) { buf[cb] = 0; snprintf(text, cap, "%s", (char *)buf); }
    else snprintf(text, cap, "<type %lu, %lu bytes>", (unsigned long)type, (unsigned long)cb);
    return (int)type;
}
#define CTS_AV_BASE "Software\\Microsoft\\Avalon.Graphics"
#define CTS_DESKTOP "Control Panel\\Desktop"

/* Device name of the first display device attached to the desktop (what CreateRenderingParams uses), without the "\\.\" prefix. */
static int cts_first_attached(char *out, size_t cap) {
    DISPLAY_DEVICEA dd; DWORD i;
    for (i = 0; i < 64; i++) {
        memset(&dd, 0, sizeof dd); dd.cb = sizeof dd;
        if (!EnumDisplayDevicesA(NULL, i, &dd, 0)) break;
        if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            const char *n = dd.DeviceName; while (n[0] == '\\' || (n[0] == '.' && n[1] == '\\')) n += (n[0] == '\\') ? 1 : 2;
            snprintf(out, cap, "%s", n); return 1;
        }
    }
    return 0;
}
static int cts_monitor_device(HMONITOR m, char *out, size_t cap) {
    MONITORINFOEXA mi; const char *n;
    memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    if (!m || !GetMonitorInfoA(m, (MONITORINFO *)&mi)) return 0;
    n = mi.szDevice; while (n[0] == '\\' || (n[0] == '.' && n[1] == '\\')) n += (n[0] == '\\') ? 1 : 2;
    snprintf(out, cap, "%s", n); return 1;
}
/* True when no Avalon.Graphics key exists in HKCU/HKLM at all (so the built-in defaults are what DirectWrite reports). */
static int cts_no_avalon(void) {
    HKEY k; int any = 0;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CTS_AV_BASE, 0, KEY_READ, &k) == ERROR_SUCCESS) { RegCloseKey(k); any = 1; }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, CTS_AV_BASE, 0, KEY_READ, &k) == ERROR_SUCCESS) { RegCloseKey(k); any = 1; }
    return !any;
}

/* HKCU\Software\Microsoft\Avalon.Graphics\<dev> scratch values: only values that did not exist are written, and everything the
 * test created is removed again (also by the next cts_av_open after a test aborted, and by the process exit destructor). */
typedef struct {
    int active, created_base, created_dev; char dev[64];
    char values[8][48]; int nvalues;
} cts_av;
static cts_av g_av;

static void cts_av_close(void) {
    HKEY base, dev; int i;
    if (!g_av.active) return;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CTS_AV_BASE, 0, KEY_READ | KEY_WRITE, &base) == ERROR_SUCCESS) {
        if (RegOpenKeyExA(base, g_av.dev, 0, KEY_READ | KEY_WRITE, &dev) == ERROR_SUCCESS) {
            for (i = 0; i < g_av.nvalues; i++) RegDeleteValueA(dev, g_av.values[i]);
            RegCloseKey(dev);
        }
        if (g_av.created_dev) RegDeleteKeyA(base, g_av.dev);
        RegCloseKey(base);
    }
    if (g_av.created_base) RegDeleteKeyA(HKEY_CURRENT_USER, CTS_AV_BASE);
    memset(&g_av, 0, sizeof g_av);
}
static void __attribute__((destructor)) cts_av_atexit(void) { cts_av_close(); }
/* Prepare the scratch key for device `dev`; 0 when the registry is not writable. */
static int cts_av_open(const char *dev) {
    HKEY base = NULL, k = NULL; DWORD disp = 0;
    cts_av_close();
    if (RegCreateKeyExA(HKEY_CURRENT_USER, CTS_AV_BASE, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &base, &disp) != ERROR_SUCCESS) return 0;
    g_av.created_base = disp == REG_CREATED_NEW_KEY;
    g_av.active = 1;
    snprintf(g_av.dev, sizeof g_av.dev, "%s", dev);
    if (RegCreateKeyExA(base, dev, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &k, &disp) != ERROR_SUCCESS) { RegCloseKey(base); cts_av_close(); return 0; }
    g_av.created_dev = disp == REG_CREATED_NEW_KEY;
    RegCloseKey(k); RegCloseKey(base);
    return 1;
}
/* Writes a DWORD value that must not already exist; returns 1 when written. */
static int cts_av_set(const char *name, DWORD v) {
    HKEY base, k; DWORD type, cb = 0; int ok = 0;
    if (!g_av.active || g_av.nvalues >= 8) return 0;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CTS_AV_BASE, 0, KEY_READ | KEY_WRITE, &base) != ERROR_SUCCESS) return 0;
    if (RegOpenKeyExA(base, g_av.dev, 0, KEY_READ | KEY_WRITE, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExA(k, name, NULL, &type, NULL, &cb) == ERROR_FILE_NOT_FOUND) {
            if (RegSetValueExA(k, name, 0, REG_DWORD, (BYTE *)&v, sizeof v) == ERROR_SUCCESS) {
                snprintf(g_av.values[g_av.nvalues++], 48, "%s", name); ok = 1;
            }
        }
        RegCloseKey(k);
    }
    RegCloseKey(base);
    return ok;
}
static void cts_av_unset(const char *name) {
    HKEY base, k; int i;
    if (!g_av.active) return;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CTS_AV_BASE, 0, KEY_READ | KEY_WRITE, &base) == ERROR_SUCCESS) {
        if (RegOpenKeyExA(base, g_av.dev, 0, KEY_READ | KEY_WRITE, &k) == ERROR_SUCCESS) { RegDeleteValueA(k, name); RegCloseKey(k); }
        RegCloseKey(base);
    }
    for (i = 0; i < g_av.nvalues; i++) if (!strcmp(g_av.values[i], name)) { memmove(g_av.values[i], g_av.values[i + 1], (size_t)(g_av.nvalues - i - 1) * 48); g_av.nvalues--; break; }
}


/* True when any device subkey of Avalon.Graphics in HKCU or HKLM carries the named DWORD value. */
static int cts_avalon_has_value(const char *name) {
    HKEY roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE }; int r, found = 0;
    for (r = 0; r < 2; r++) {
        HKEY base; DWORD i;
        if (RegOpenKeyExA(roots[r], CTS_AV_BASE, 0, KEY_READ, &base) != ERROR_SUCCESS) continue;
        for (i = 0; i < 256; i++) {
            char sub[128]; DWORD cch = sizeof sub; DWORD v;
            if (RegEnumKeyExA(base, i, sub, &cch, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
            {
                char path[300]; snprintf(path, sizeof path, "%s\\%s", CTS_AV_BASE, sub);
                if (cts_reg_dword(roots[r], path, name, &v)) found = 1;
            }
        }
        RegCloseKey(base);
    }
    return found;
}

/* ---- font face for recommended-mode queries -------------------------------------------------------------------------- */
/* GetRecommendedRenderingMode through the newest face interface available. mode receives a DWRITE_RENDERING_MODE1 value. */
static HRESULT cts_recommend(IDWriteFontFace *face, float em, DWRITE_MEASURING_MODE mm, IDWriteRenderingParams *p, int *mode) {
    IDWriteFontFace3 *f3 = NULL; HRESULT hr;
    *mode = -1;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
        DWRITE_RENDERING_MODE1 m1 = (DWRITE_RENDERING_MODE1)-1; DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)-1;
        hr = IDWriteFontFace3_GetRecommendedRenderingMode(f3, em, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, mm, p, &m1, &gf);
        IDWriteFontFace3_Release(f3);
        *mode = (int)m1; return hr;
    } else {
        DWRITE_RENDERING_MODE m0 = (DWRITE_RENDERING_MODE)-1;
        hr = IDWriteFontFace_GetRecommendedRenderingMode(face, em, 1.0f, mm, p, &m0);
        *mode = (int)m0; return hr;
    }
}
#define CTS_MODE_ALIASED 1
#define CTS_MODE_GDI_CLASSIC 2
#define CTS_MODE_GDI_NATURAL 3
#define CTS_MODE_OUTLINE 6

/* Wide text helpers */
static void cts_w(const char *s, WCHAR *w, int cap) { dw_wide(s, w, cap); }
#endif
