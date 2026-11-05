#include "gra.h"

const DWRITE_MATRIX gra_ident = { 1, 0, 0, 1, 0, 0 };

int gra_open(gra_fx *fx) {
    memset(fx, 0, sizeof *fx);
    fx->f = dw_shared();
    if (!fx->f) return 0;
    if (FAILED(dwx_font_open_default(&fx->h, fx->f)) || !fx->h.face) return 0;
    fx->ok = 1;
    return 1;
}
void gra_close(gra_fx *fx) { dwx_font_close(&fx->h); fx->ok = 0; }

void gra_run(gra_fx *fx, const char *chars, float em) {
    int n = 0;
    for (; chars[n] && n < 16; n++) fx->gi[n] = (UINT16)ttf_default_gid((unsigned char)chars[n]);
    gra_run_gids(fx, fx->gi, n, em);
}
void gra_run_gids(gra_fx *fx, const UINT16 *gids, int n, float em) {
    if (gids != fx->gi) memcpy(fx->gi, gids, n * sizeof(UINT16));
    memset(&fx->run, 0, sizeof fx->run);
    fx->run.fontFace = fx->h.face; fx->run.fontEmSize = em; fx->run.glyphCount = n; fx->run.glyphIndices = fx->gi;
}
void gra_advances(gra_fx *fx, const float *adv) { memcpy(fx->adv, adv, fx->run.glyphCount * sizeof(float)); fx->run.glyphAdvances = fx->adv; }
void gra_offsets(gra_fx *fx, const DWRITE_GLYPH_OFFSET *off) { memcpy(fx->off, off, fx->run.glyphCount * sizeof *off); fx->run.glyphOffsets = fx->off; }

HRESULT gra_make(gra_fx *fx, IDWriteGlyphRunAnalysis **a, float ppd, const DWRITE_MATRIX *m, DWRITE_RENDERING_MODE mode,
                 DWRITE_MEASURING_MODE mm, float ox, float oy) {
    *a = NULL;
    return IDWriteFactory_CreateGlyphRunAnalysis(fx->f, &fx->run, ppd, m, mode, mm, ox, oy, a);
}
HRESULT gra_make_al(gra_fx *fx, IDWriteGlyphRunAnalysis **a, float ox, float oy) {
    return gra_make(fx, a, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, ox, oy);
}
HRESULT gra_make2(gra_fx *fx, IDWriteGlyphRunAnalysis **a, const DWRITE_MATRIX *m, DWRITE_RENDERING_MODE mode, DWRITE_MEASURING_MODE mm,
                  DWRITE_GRID_FIT_MODE gf, DWRITE_TEXT_ANTIALIAS_MODE aa, float ox, float oy) {
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2);
    *a = NULL;
    if (!f2) return GRA_UNSUPPORTED;
    return IDWriteFactory2_CreateGlyphRunAnalysis(f2, &fx->run, m, mode, mm, gf, aa, ox, oy, a);
}
HRESULT gra_make3(gra_fx *fx, IDWriteGlyphRunAnalysis **a, const DWRITE_MATRIX *m, DWRITE_RENDERING_MODE1 mode, DWRITE_MEASURING_MODE mm,
                  DWRITE_GRID_FIT_MODE gf, DWRITE_TEXT_ANTIALIAS_MODE aa, float ox, float oy) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    *a = NULL;
    if (!f3) return GRA_UNSUPPORTED;
    return IDWriteFactory3_CreateGlyphRunAnalysis(f3, &fx->run, m, mode, mm, gf, aa, ox, oy, a);
}

HRESULT gra_bounds(IDWriteGlyphRunAnalysis *a, DWRITE_TEXTURE_TYPE t, RECT *rc) {
    memset(rc, 0xAA, sizeof *rc);
    return IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, t, rc);
}
int gra_tex_rect(IDWriteGlyphRunAnalysis *a, DWRITE_TEXTURE_TYPE t, const RECT *rc, gra_tex *x) {
    memset(x, 0, sizeof *x);
    x->rc = *rc; x->w = rc->right - rc->left; x->h = rc->bottom - rc->top;
    if (x->w <= 0 || x->h <= 0 || x->w > 4096 || x->h > 4096) return 0;
    x->n = (size_t)x->w * x->h * (t == DWRITE_TEXTURE_CLEARTYPE_3x1 ? 3 : 1);
    x->p = (BYTE *)malloc(x->n);
    if (!x->p) return 0;
    memset(x->p, 0xCD, x->n);
    x->ht = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, t, rc, x->p, (UINT32)x->n);
    return SUCCEEDED(x->ht);
}
int gra_tex_get(IDWriteGlyphRunAnalysis *a, DWRITE_TEXTURE_TYPE t, gra_tex *x) {
    RECT rc;
    memset(x, 0, sizeof *x);
    x->hb = gra_bounds(a, t, &rc);
    x->rc = rc;
    if (FAILED(x->hb) || gra_rect_empty(&rc)) return 0;
    return gra_tex_rect(a, t, &rc, x) ? (x->hb = S_OK, 1) : 0;
}
void gra_tex_free(gra_tex *x) { free(x->p); x->p = NULL; }
size_t gra_count_eq(const gra_tex *x, BYTE v) { size_t i, c = 0; for (i = 0; i < x->n; i++) c += x->p[i] == v; return c; }
size_t gra_count_nz(const gra_tex *x) { size_t i, c = 0; for (i = 0; i < x->n; i++) c += x->p[i] != 0; return c; }
unsigned long long gra_sum(const gra_tex *x) { size_t i; unsigned long long s = 0; for (i = 0; i < x->n; i++) s += x->p[i]; return s; }
size_t gra_count_not01(const gra_tex *x) { size_t i, c = 0; for (i = 0; i < x->n; i++) c += x->p[i] != 0 && x->p[i] != 255; return c; }

const char *gra_mode_name(int m) {
    switch (m) {
    case 0: return "DEFAULT"; case 1: return "ALIASED"; case 2: return "GDI_CLASSIC"; case 3: return "GDI_NATURAL";
    case 4: return "NATURAL"; case 5: return "NATURAL_SYMMETRIC"; case 6: return "OUTLINE"; case 7: return "NATURAL_SYMMETRIC_DOWNSAMPLED";
    }
    return "?";
}
