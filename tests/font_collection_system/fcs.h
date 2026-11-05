/* Shared scaffolding for the system font collection area. */
#ifndef FCS_H
#define FCS_H
#include "dwx.h"
#include <wchar.h>

#define FCS_NOFONT ((HRESULT)0x88985002)   /* DWRITE_E_NOFONT */

/* Skip the test when the shared factory is not available. */
#define NEED_F() IDWriteFactory *f = dw_shared(); if (!f) { t_skip("DWriteCreateFactory unavailable"); return; }
/* Shared factory plus the legacy system collection (released by END_SYS). */
#define NEED_SYS() \
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; \
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return; } \
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) { t_skip("no system font collection"); return; }
#define END_SYS() DW_RELEASE(c)
/* Versioned factory (3, 5, 6, 7) or skip. */
#define NEED_FV(var, type, ver) \
    type *var = (type *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver); \
    if (!var) { t_skip("IDWriteFactory%d not supported", ver); return; }

/* The mingw IDWriteFactory5_GetSystemFontCollection macro recurses into the IDWriteFactory3 one; call the slot directly. */
#define FCS_F5_GETSYS(f5, dl, pc, chk) ((*(f5)->lpVtbl->IDWriteFactory3_GetSystemFontCollection))((f5), (dl), (pc), (chk))

/* UTF-8 rendering of a wide string in one of a few rotating static buffers (diagnostics only). */
static const char *fcs_u8(const WCHAR *w) {
    static char ring[4][512]; static int n; char *b = ring[n++ & 3];
    if (!w) return "(null)";
    if (!dw_narrow(w, b, 512)) strcpy(b, "?");
    return b;
}
/* English (or first) family name of a family into buf; "" on failure. */
static HRESULT fcs_family_en(IDWriteFontFamily *fam, char *buf, int cap) {
    IDWriteLocalizedStrings *s = NULL; HRESULT hr;
    buf[0] = 0;
    hr = IDWriteFontFamily_GetFamilyNames(fam, &s);
    if (FAILED(hr) || !s) return FAILED(hr) ? hr : E_FAIL;
    if (IDWriteLocalizedStrings_GetCount(s) == 0) { IDWriteLocalizedStrings_Release(s); return E_FAIL; }
    hr = dw_ls_en(s, buf, cap);
    IDWriteLocalizedStrings_Release(s);
    return hr;
}
/* Same for the wide string. */
static HRESULT fcs_family_en_w(IDWriteFontFamily *fam, WCHAR *buf, int cap) {
    char u[512]; HRESULT hr = fcs_family_en(fam, u, sizeof u);
    buf[0] = 0;
    if (SUCCEEDED(hr)) dw_wide(u, buf, cap);
    return hr;
}
/* Case-insensitive ASCII compare of UTF-8 strings. */
static int fcs_ieq(const char *a, const char *b) { return !_stricmp(a, b); }
/* Family at index i of c, name in buf. */
static HRESULT fcs_name_at(IDWriteFontCollection *c, UINT32 i, char *buf, int cap) {
    IDWriteFontFamily *fam = NULL; HRESULT hr = IDWriteFontCollection_GetFontFamily(c, i, &fam);
    buf[0] = 0;
    if (FAILED(hr) || !fam) return FAILED(hr) ? hr : E_FAIL;
    hr = fcs_family_en(fam, buf, cap);
    IDWriteFontFamily_Release(fam);
    return hr;
}
/* Find a family; returns its index or UINT32_MAX when absent. Hard-checks the out-parameter relation. */
static UINT32 fcs_find(IDWriteFontCollection *c, const WCHAR *name, const char *file, int line) {
    UINT32 idx = 0x1234; BOOL ex = 0x55;
    HRESULT hr = IDWriteFontCollection_FindFamilyName(c, name, &idx, &ex);
    t_check(hr == S_OK, file, line, "FindFamilyName S_OK", "hr %s for \"%s\"", dw_hr_name(hr), fcs_u8(name));
    if (ex) t_check(ex == 1 && idx < IDWriteFontCollection_GetFontFamilyCount(c), file, line, "found => BOOL 1 and index < count", "ex %d idx %u", ex, idx);
    else t_check(idx == 0xFFFFFFFFu, file, line, "not found => index UINT_MAX", "idx %u", idx);
    return ex ? idx : 0xFFFFFFFFu;
}
#define FIND(c, name) fcs_find((c), (name), __FILE__, __LINE__)
/* Same object after QI to IUnknown. */
static int fcs_same(void *a, void *b) { return dw_same_identity((IUnknown *)a, (IUnknown *)b); }
/* Families enumerated cheaply: upper bound used by the heavier loops. */
#define FCS_MAX_FAMILIES 1000
#define FCS_MAX_FONTS 250

/* Family count of a collection without macros colliding on the versioned names. */
static UINT32 fcs_count(IDWriteFontCollection *c) { return IDWriteFontCollection_GetFontFamilyCount(c); }
/* Iterate families [0, min(count, lim)) of collection c; never `continue` inside, END_FAMILY releases. */
#define FOREACH_FAMILY(c, i, fam, lim) \
    for (UINT32 i = 0, n_##i = fcs_count(c); i < n_##i && i < (UINT32)(lim); i++) { \
        IDWriteFontFamily *fam = NULL; \
        HRESULT hrf_##i = IDWriteFontCollection_GetFontFamily((c), i, &fam); \
        t_check(hrf_##i == S_OK && fam != NULL, __FILE__, __LINE__, "GetFontFamily(i) S_OK", "index %u hr %s", i, dw_hr_name(hrf_##i)); \
        if (fam) {
#define END_FAMILY(fam) IDWriteFontFamily_Release(fam); } }
#endif
