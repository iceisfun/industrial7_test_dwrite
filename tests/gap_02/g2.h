/* Shared scaffolding for the gap_02 area (thin-coverage methods): synthetic fonts served through an ISOLATED factory.
 * Teardown order: release objects built from the loader, then close the fixture (unregisters, releases the factory). */
#ifndef G2_H
#define G2_H
#include "dwx.h"
#include <wchar.h>

#undef IDWriteFont3_CreateFontFace
#define G2_F3_CREATEFACE(p, o) ((p)->lpVtbl->IDWriteFont3_CreateFontFace((p), (o)))
#define G2_E_NOTSUFF ((HRESULT)0x8007007A)
#define G2_SENT ((void *)(ULONG_PTR)0x1)
#define G2_SOFT(c) do { if (!(c)) t_note("soft expectation not met at line %d: %s", __LINE__, #c); } while (0)

/* A fresh isolated factory plus the default synthetic font opened as a face. */
typedef struct g2_face {
    IDWriteFactory *f; dwx_font h; IDWriteFontFace *face; IDWriteFontFace3 *face3; IDWriteFactory4 *f4; int ok;
} g2_face;
static int g2_face_open(g2_face *x, const ttf_spec *spec)
{
    HRESULT hr; uint8_t *b; size_t n;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    b = spec ? ttf_build(spec, &n) : ttf_build_default(&n);
    if (!b) { DW_RELEASE(x->f); t_skip("font generation failed"); return 0; }
    hr = dwx_font_open(&x->h, x->f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(b);
    if (FAILED(hr) || !x->h.face) { dwx_font_close(&x->h); DW_RELEASE(x->f); t_skip("synthetic font did not open: %s", dw_hr_name(hr)); return 0; }
    x->face = x->h.face;
    if (FAILED(IDWriteFontFace_QueryInterface(x->face, &IID_IDWriteFontFace3, (void **)&x->face3))) x->face3 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory4, (void **)&x->f4))) x->f4 = NULL;
    x->ok = 1;
    return 1;
}
static void g2_face_close(g2_face *x)
{
    if (!x->ok) return;
    DW_RELEASE(x->face3); DW_RELEASE(x->f4);
    dwx_font_close(&x->h);
    DW_RELEASE(x->f);
    x->ok = 0;
}

/* A private factory with a custom collection built from synthetic fonts (font[i] is the IDWriteFont for blob i). */
#define G2_MAX 8
typedef struct g2_coll {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; int n; UINT32 keys[G2_MAX]; HRESULT hr;
} g2_coll;
static void g2_coll_close(g2_coll *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) dwx_coll_unregister(x->cl);
    if (x->fl) dwx_loader_unregister(x->fl);
    if (x->cl) { dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}
static int g2_coll_open(g2_coll *x, const ttf_spec *specs, int n)
{
    int i;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    x->fl = dwx_loader_new();
    for (i = 0; i < n; i++) {
        size_t sz; uint8_t *b = ttf_build(&specs[i], &sz);
        if (!b) { g2_coll_close(x); t_skip("font generation failed"); return 0; }
        x->keys[i] = dwx_loader_add(x->fl, b, sz); free(b);
    }
    x->n = n;
    x->hr = dwx_loader_register(x->fl, x->f);
    if (SUCCEEDED(x->hr)) {
        x->cl = dwx_coll_new(x->fl);
        for (i = 0; i < n; i++) dwx_coll_add_key(x->cl, x->keys[i]);
        x->hr = dwx_coll_register(x->cl, x->f);
        if (SUCCEEDED(x->hr)) x->hr = dwx_coll_create(x->cl, x->f, &x->coll);
    }
    if (FAILED(x->hr) || !x->coll) { HRESULT h = x->hr; g2_coll_close(x); t_skip("custom collection unavailable: %s", dw_hr_name(h)); return 0; }
    return 1;
}
static void g2_spec(ttf_spec *s, const char *family)
{
    char ps[96]; snprintf(ps, sizeof ps, "%s-Regular", family);
    ttf_default_spec(s); ttf_set_names(s, family, "Regular", family, ps);
}
/* First IDWriteFont of the named family in a collection (NULL when absent). */
static IDWriteFont *g2_font(IDWriteFontCollection *c, const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st)
{
    UINT32 idx = 0; BOOL ex = FALSE; IDWriteFontFamily *ff = NULL; IDWriteFont *font = NULL;
    IDWriteFontCollection_FindFamilyName(c, fam, &idx, &ex);
    if (!ex || FAILED(IDWriteFontCollection_GetFontFamily(c, idx, &ff)) || !ff) return NULL;
    if (FAILED(IDWriteFontFamily_GetFirstMatchingFont(ff, w, DWRITE_FONT_STRETCH_NORMAL, st, &font))) font = NULL;
    IDWriteFontFamily_Release(ff);
    return font;
}
/* IUnknown identity of any COM pointer (refcount balanced). */
static IUnknown *g2_ident(void *p)
{
    IUnknown *u = NULL;
    if (!p || FAILED(IUnknown_QueryInterface((IUnknown *)p, &IID_IUnknown, (void **)&u))) return NULL;
    IUnknown_Release(u);
    return u;
}
#endif
