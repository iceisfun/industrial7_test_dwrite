/* Shared helpers for the factory_create area. */
#ifndef FC_H
#define FC_H
#include "dw.h"

#define FT_SHARED   DWRITE_FACTORY_TYPE_SHARED
#define FT_ISOLATED DWRITE_FACTORY_TYPE_ISOLATED

/* DWriteCreateFactory through the resolved export; r is the result, p the out pointer (pre-set to a sentinel). */
static inline HRESULT fc_create(int type, REFIID riid, void **out) {
    dw_create_fn fn = dw_proc();
    if (!fn) { *out = NULL; return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND); }
    return fn((DWRITE_FACTORY_TYPE)type, riid, (IUnknown **)out);
}
#define FC_SENTINEL ((void *)(ULONG_PTR)0x1)

/* Skip the test when the export cannot be resolved. */
#define NEED_EXPORT() do { if (!dw_proc()) { t_skip("DWriteCreateFactory not exported"); return; } } while (0)

/* Table of the factory interface generations. */
typedef struct { int ver; const char *name; } fc_ver;

/* Smoke-use of a factory: succeeds when basic services respond. */
static inline int fc_factory_usable(IDWriteFactory *f) {
    IDWriteTextFormat *tf = NULL; HRESULT hr; int ok = 0;
    hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf);
    if (SUCCEEDED(hr) && tf) { ok = 1; IDWriteTextFormat_Release(tf); }
    return ok;
}
#endif
