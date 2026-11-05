/* Shared helpers for the r2_factory_platform_dll area. */
#ifndef RFP_H
#define RFP_H
#include "dwx.h"

#define FT_SHARED   DWRITE_FACTORY_TYPE_SHARED
#define FT_ISOLATED DWRITE_FACTORY_TYPE_ISOLATED
#define RFP_SENTINEL ((void *)(ULONG_PTR)0x1)

static inline HMODULE rfp_mod(void) { return t_module("dwrite.dll"); }
static inline HRESULT rfp_create(int type, REFIID riid, void **out) {
    dw_create_fn fn = dw_proc();
    if (!fn) { *out = NULL; return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND); }
    return fn((DWRITE_FACTORY_TYPE)type, riid, (IUnknown **)out);
}
#define NEED_EXPORT() do { if (!dw_proc()) { t_skip("DWriteCreateFactory not exported"); return; } } while (0)
/* Cached factory of a given generation (ver 0..7) or skip the test. */
#define NEED_FACTORY(var, type, ver, T) \
    T *var = (T *)dw_factory_v((type), (ver)); \
    if (!var) { t_skip("IDWriteFactory%d unsupported", (ver)); return; }

/* Fresh arial-like text format on f; NULL when creation fails. */
static inline IDWriteTextFormat *rfp_fmt(IDWriteFactory *f, float size, const WCHAR *loc) {
    IDWriteTextFormat *tf = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, loc, &tf))) return NULL;
    return tf;
}
/* Skip where the emulated implementation dereferences a NULL argument (Windows validates it). */
#define NO_WINE() do { if (t_is_wine()) { t_skip("emulated implementation faults on NULL argument"); return; } } while (0)
#endif
