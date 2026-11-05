#include "rr.h"

const DWRITE_MATRIX rr_ident = { 1, 0, 0, 1, 0, 0 };

/* gasp behavior bits: 1 GRIDFIT, 2 DOGRAY, 4 SYMMETRIC_GRIDFIT, 8 SYMMETRIC_SMOOTHING */
const rr_gasp rr_gasps[] = {
    { { { 0xFFFF, 0x000F } }, 1, "all_flags" },
    { { { 0xFFFF, 0x0003 } }, 1, "no_symmetric_smoothing" },
    { { { 0xFFFF, 0x0000 } }, 1, "no_flags" },
    { { { 0xFFFF, 0x0008 } }, 1, "smoothing_only" },
    { { { 8, 0x0003 }, { 16, 0x000B }, { 0xFFFF, 0x0003 } }, 3, "smoothing_9_to_16" },
    { { { 12, 0x0008 }, { 0xFFFF, 0x0000 } }, 2, "smoothing_up_to_12" },
    { { { 20, 0x0003 }, { 0xFFFF, 0x000F } }, 2, "smoothing_above_20" },
    { { { 6, 0x0008 }, { 10, 0x0000 }, { 14, 0x0008 }, { 0xFFFF, 0x0000 } }, 4, "alternating" },
    { { { 50, 0x0008 } }, 1, "single_50_smoothing" },
    { { { 50, 0x0003 } }, 1, "single_50_plain" },
    { { { 10, 0x0008 }, { 10, 0x0000 }, { 0xFFFF, 0x0000 } }, 3, "duplicate_max_skipped" },
    { { { 20, 0x0008 }, { 10, 0x0000 }, { 0xFFFF, 0x0000 } }, 3, "descending_range_skipped" },
    { { { 1, 0x0008 }, { 2, 0x0000 }, { 3, 0x0008 }, { 4, 0x0000 }, { 0xFFFF, 0x0008 } }, 5, "fine_toggles" },
    { { { 9, 0x0004 }, { 30, 0x0008 }, { 0xFFFF, 0x0004 } }, 3, "grid_and_smoothing_split" },
    { { { 0xFFFF, 0x0004 } }, 1, "grid_only" },
    { { { 16, 0x0002 }, { 0xFFFF, 0x0001 } }, 2, "gray_then_grid" },
};
const int rr_ngasps = (int)(sizeof rr_gasps / sizeof rr_gasps[0]);

int rr_open(rr_font *x, const rr_gasp *g) {
    ttf_spec s; size_t n; uint8_t *b; HRESULT hr;
    memset(x, 0, sizeof *x);
    x->f = dw_shared();
    if (!x->f) return 0;
    ttf_default_spec(&s);
    if (g && g->n) s.gasp = g->r, s.ngasp = g->n;
    b = ttf_build(&s, &n);
    if (!b) return 0;
    hr = dwx_font_open(&x->h, x->f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(b);
    if (FAILED(hr) || !x->h.face) return 0;
    x->f0 = x->h.face;
    IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace1, (void **)&x->f1);
    IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace2, (void **)&x->f2);
    IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace3, (void **)&x->f3);
    if (FAILED(IDWriteFactory_CreateRenderingParams(x->f, &x->dp))) x->dp = NULL;
    x->ok = 1;
    return 1;
}
void rr_close(rr_font *x) {
    DW_RELEASE(x->f1); DW_RELEASE(x->f2); DW_RELEASE(x->f3); DW_RELEASE(x->dp);
    x->f0 = NULL;
    dwx_font_close(&x->h);
    x->ok = 0;
}

HRESULT rr_call0(rr_font *x, float em, float ppd, int mm, IDWriteRenderingParams *p, int *mode) {
    DWRITE_RENDERING_MODE m = (DWRITE_RENDERING_MODE)RR_SENT;
    HRESULT hr = IDWriteFontFace_GetRecommendedRenderingMode(x->f0, em, ppd, (DWRITE_MEASURING_MODE)mm, p, &m);
    *mode = (int)m;
    return hr;
}
HRESULT rr_call1(rr_font *x, const rr_in *in, int *mode) {
    DWRITE_RENDERING_MODE m = (DWRITE_RENDERING_MODE)RR_SENT;
    HRESULT hr = (x->f1->lpVtbl->IDWriteFontFace1_GetRecommendedRenderingMode)(x->f1, in->em, in->dx, in->dy, in->hasm ? &in->m : NULL, in->sw,
        (DWRITE_OUTLINE_THRESHOLD)in->th, (DWRITE_MEASURING_MODE)in->mm, &m);
    *mode = (int)m;
    return hr;
}
HRESULT rr_call2(rr_font *x, const rr_in *in, IDWriteRenderingParams *p, int *mode, int *grid) {
    DWRITE_RENDERING_MODE m = (DWRITE_RENDERING_MODE)RR_SENT; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)RR_SENT;
    HRESULT hr = (x->f2->lpVtbl->IDWriteFontFace2_GetRecommendedRenderingMode)(x->f2, in->em, in->dx, in->dy, in->hasm ? &in->m : NULL, in->sw,
        (DWRITE_OUTLINE_THRESHOLD)in->th, (DWRITE_MEASURING_MODE)in->mm, p, &m, &g);
    *mode = (int)m; *grid = (int)g;
    return hr;
}
HRESULT rr_call3(rr_font *x, const rr_in *in, IDWriteRenderingParams *p, int *mode, int *grid) {
    DWRITE_RENDERING_MODE1 m = (DWRITE_RENDERING_MODE1)RR_SENT; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)RR_SENT;
    HRESULT hr = (x->f3->lpVtbl->IDWriteFontFace3_GetRecommendedRenderingMode)(x->f3, in->em, in->dx, in->dy, in->hasm ? &in->m : NULL, in->sw,
        (DWRITE_OUTLINE_THRESHOLD)in->th, (DWRITE_MEASURING_MODE)in->mm, p, &m, &g);
    *mode = (int)m; *grid = (int)g;
    return hr;
}

/* ---- run fixture ---------------------------------------------------------------------------------------------- */
int rr_run_open(rr_run *r) { memset(r, 0, sizeof *r); r->ok = rr_open(&r->x, NULL); return r->ok; }
void rr_run_close(rr_run *r) { rr_close(&r->x); }
void rr_run_set(rr_run *r, const char *chars, float em) {
    int n = 0;
    for (; chars[n] && n < 16; n++) r->gi[n] = (UINT16)ttf_default_gid((unsigned char)chars[n]);
    memset(&r->run, 0, sizeof r->run);
    r->run.fontFace = r->x.f0; r->run.fontEmSize = em; r->run.glyphCount = n; r->run.glyphIndices = r->gi;
}
HRESULT rr_an(rr_run *r, IDWriteGlyphRunAnalysis **a, float ppd, const DWRITE_MATRIX *m, int mode, int mm, float ox, float oy) {
    *a = NULL;
    return IDWriteFactory_CreateGlyphRunAnalysis(r->x.f, &r->run, ppd, m, (DWRITE_RENDERING_MODE)mode, (DWRITE_MEASURING_MODE)mm, ox, oy, a);
}
HRESULT rr_an2(rr_run *r, IDWriteGlyphRunAnalysis **a, const DWRITE_MATRIX *m, int mode, int mm, int gf, int aa, float ox, float oy) {
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2);
    *a = NULL;
    if (!f2) return RR_UNSUP;
    return IDWriteFactory2_CreateGlyphRunAnalysis(f2, &r->run, m, (DWRITE_RENDERING_MODE)mode, (DWRITE_MEASURING_MODE)mm, (DWRITE_GRID_FIT_MODE)gf,
                                                  (DWRITE_TEXT_ANTIALIAS_MODE)aa, ox, oy, a);
}
HRESULT rr_an3(rr_run *r, IDWriteGlyphRunAnalysis **a, const DWRITE_MATRIX *m, int mode1, int mm, int gf, int aa, float ox, float oy) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    *a = NULL;
    if (!f3) return RR_UNSUP;
    return IDWriteFactory3_CreateGlyphRunAnalysis(f3, &r->run, m, (DWRITE_RENDERING_MODE1)mode1, (DWRITE_MEASURING_MODE)mm, (DWRITE_GRID_FIT_MODE)gf,
                                                  (DWRITE_TEXT_ANTIALIAS_MODE)aa, ox, oy, a);
}
HRESULT rr_bounds(IDWriteGlyphRunAnalysis *a, int type, RECT *rc) {
    memset(rc, 0xAA, sizeof *rc);
    return IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, (DWRITE_TEXTURE_TYPE)type, rc);
}
int rr_tex_rect(IDWriteGlyphRunAnalysis *a, int type, const RECT *rc, rr_tex *t) {
    HRESULT hr;
    memset(t, 0, sizeof *t);
    t->rc = *rc; t->w = rc->right - rc->left; t->h = rc->bottom - rc->top;
    if (t->w <= 0 || t->h <= 0 || t->w > 4096 || t->h > 4096) return 0;
    t->n = (size_t)t->w * t->h * (type == DWRITE_TEXTURE_CLEARTYPE_3x1 ? 3 : 1);
    t->p = (BYTE *)malloc(t->n);
    if (!t->p) return 0;
    memset(t->p, 0xCD, t->n);
    hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, (DWRITE_TEXTURE_TYPE)type, rc, t->p, (UINT32)t->n);
    return SUCCEEDED(hr);
}
int rr_tex_get(IDWriteGlyphRunAnalysis *a, int type, rr_tex *t) {
    RECT rc;
    memset(t, 0, sizeof *t);
    if (FAILED(rr_bounds(a, type, &rc)) || rr_rect_empty(&rc)) return 0;
    return rr_tex_rect(a, type, &rc, t);
}
void rr_tex_free(rr_tex *t) { free(t->p); t->p = NULL; }
