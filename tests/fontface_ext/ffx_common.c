#include "ffx.h"

int ffx_open_blob(ffx *x, const void *blob, size_t n, UINT32 index, DWRITE_FONT_SIMULATIONS sim) {
    IDWriteFactory *f = dw_shared();
    HRESULT hr;
    memset(x, 0, sizeof *x);
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    hr = dwx_font_open(&x->h, f, blob, n, index, sim);
    x->owns_h = 1;
    if (FAILED(hr) || !x->h.face) { t_skip("cannot open synthetic face: %s", dw_hr_name(hr)); return 0; }
    x->f0 = x->h.face; IDWriteFontFace_AddRef(x->f0);
    if (FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace1, (void **)&x->f1)) ||
        FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace2, (void **)&x->f2)) ||
        FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace3, (void **)&x->f3)) ||
        FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace4, (void **)&x->f4)) ||
        FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace5, (void **)&x->f5))) {
        t_skip("face lacks IDWriteFontFace1..5");
        return 0;
    }
    return 1;
}
int ffx_open_spec(ffx *x, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim) {
    size_t n = 0; uint8_t *b = ttf_build(s, &n); int ok;
    if (!b) { memset(x, 0, sizeof *x); t_skip("ttf_build failed"); return 0; }
    ok = ffx_open_blob(x, b, n, 0, sim);
    free(b);
    return ok;
}
int ffx_open_default(ffx *x) { ttf_spec s; ttf_default_spec(&s); return ffx_open_spec(x, &s, DWRITE_FONT_SIMULATIONS_NONE); }

int ffx_open_sys(ffx *x, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STRETCH st, DWRITE_FONT_STYLE sty) {
    IDWriteFactory *f = dw_shared();
    memset(x, 0, sizeof *x);
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    x->f0 = dw_sysface(f, family, w, st, sty);
    if (!x->f0) { t_skip("system font family not installed"); return 0; }
    if (FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace1, (void **)&x->f1)) ||
        FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace2, (void **)&x->f2)) ||
        FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace3, (void **)&x->f3)) ||
        FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace4, (void **)&x->f4)) ||
        FAILED(IDWriteFontFace_QueryInterface(x->f0, &IID_IDWriteFontFace5, (void **)&x->f5))) {
        t_skip("face lacks IDWriteFontFace1..5");
        return 0;
    }
    return 1;
}
void ffx_close(ffx *x) {
    DW_RELEASE(x->f5); DW_RELEASE(x->f4); DW_RELEASE(x->f3); DW_RELEASE(x->f2); DW_RELEASE(x->f1); DW_RELEASE(x->f0);
    if (x->owns_h) dwx_font_close(&x->h);
    memset(x, 0, sizeof *x);
}
void ffx_metrics1(ffx *x, DWRITE_FONT_METRICS1 *m) { memset(m, 0xCC, sizeof *m); IDWriteFontFace1_GetMetrics(x->f1, m); }

/* ---- fixtures ------------------------------------------------------------------------------------------------ */
#define A_ G('A')
const ttf_kernpair ffx_kern_pairs[] = {
    { 2,  23, -120 },    /* A V */
    { 2,  26,  -80 },    /* A Y */
    { 21, 42, -150 },    /* T o */
    { 13, 21,   30 },    /* L T (positive) */
    { 23, 2,  -100 },    /* V A */
    { 0,  0,    -5 },    /* notdef notdef */
};
const int ffx_n_kern_pairs = (int)(sizeof ffx_kern_pairs / sizeof ffx_kern_pairs[0]);
int ffx_kern_value(unsigned l, unsigned r) {
    int i; for (i = 0; i < ffx_n_kern_pairs; i++) if (ffx_kern_pairs[i].l == l && ffx_kern_pairs[i].r == r) return ffx_kern_pairs[i].v;
    return 0;
}
void ffx_spec_kern(ttf_spec *s) { ttf_default_spec(s); s->kern = ffx_kern_pairs; s->nkern = ffx_n_kern_pairs; }
void ffx_spec_gpos_kern(ttf_spec *s) { ttf_default_spec(s); s->gpos_kern = ffx_kern_pairs; s->ngpos_kern = ffx_n_kern_pairs; }

const ttf_sub1 ffx_vert_subs[] = { { 2, 64 }, { 3, 65 }, { 4, 68 } };   /* A->triangle, B->round, C->ring */
const int ffx_n_vert_subs = 3;
static const ttf_feature g_vert_feat[] = { { "vert", TTF_LK_SINGLE, 3, ffx_vert_subs } };
void ffx_spec_vert(ttf_spec *s) { ttf_default_spec(s); s->gsub = g_vert_feat; s->ngsub = 1; s->vertical = 1; s->v_ascent = 1024; s->v_descent = -1024; }

/* COLR: glyph 2 ('A') = layers {64 pal 0, 65 pal 1}; glyph 3 ('B') = layer {68 pal 2}. CPAL: one palette, 4 entries. */
static const ttf_colr_base g_cbase[] = { { 2, 0, 2 }, { 3, 2, 1 } };
static const ttf_colr_layer g_clay[] = { { 64, 0 }, { 65, 1 }, { 68, 2 } };
const uint32_t ffx_palette[] = { 0xFFFF0000u, 0x8000FF00u, 0x400000FFu, 0xFF123456u };
const int ffx_n_palette = 4;
void ffx_spec_color(ttf_spec *s) {
    ttf_default_spec(s);
    s->colr_base = g_cbase; s->ncolr_base = 2; s->colr_layers = g_clay; s->ncolr_layers = 3;
    s->palette = ffx_palette; s->npalette = ffx_n_palette;
}

/* Sparse cmap: A-C, E, a, 0x3041-0x3043 (hiragana), 0x4E00, 0x1F600-0x1F601. Glyph numbers cycle through 2..60. */
static const ttf_map g_rng_map[] = {
    { 0x41, 2 }, { 0x42, 3 }, { 0x43, 4 }, { 0x45, 5 }, { 0x61, 6 },
    { 0x3041, 7 }, { 0x3042, 8 }, { 0x3043, 9 }, { 0x4E00, 10 },
    { 0x1F600, 11 }, { 0x1F601, 12 },
};
static const ttf_cmap g_rng_cmap[] = {
    { 12, 3, 10, (int)(sizeof g_rng_map / sizeof g_rng_map[0]), g_rng_map, 0, NULL },
    { 4,  3, 1,  9, g_rng_map, 0, NULL },       /* BMP part only: the first nine entries */
};
void ffx_spec_ranges(ttf_spec *s) { ttf_default_spec(s); s->cmaps = g_rng_cmap; s->ncmaps = 2; }
