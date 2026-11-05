#include "cg.h"
#include <math.h>

/* ---------------------------------------------------------------------------------------------------- fixtures */
const uint32_t cg_pal[CG_NPAL][CG_NENT] = {
    { 0xFFFF0000u, 0x8000FF00u, 0x400000FFu, 0xFF123456u, 0x00FFFFFFu, 0xFF00FFFFu },
    { 0xFF0000FFu, 0xFFFFFF00u, 0xFF808080u, 0xFF010203u, 0xFF445566u, 0x20ABCDEFu },
};
static const ttf_colr_base g_base[] = {
    { 2, 0, 2 }, { 3, 2, 1 }, { 4, 3, 3 }, { 5, 6, 2 }, { 6, 8, 1 }, { 28, 9, 4 },
    { 54, 13, 1 }, { 55, 14, 1 }, { 56, 15, 1 }, { 57, 16, 1 }, { 58, 17, 1 }, { 59, 18, 1 }, { 60, 19, 1 }, { 61, 20, 1 }, { 62, 21, 1 }, { 63, 22, 1 },
};
static const ttf_colr_layer g_lay[] = {
    { 64, 0 }, { 65, 1 },                       /* A */
    { 68, 2 },                                  /* B */
    { 64, 0xFFFF }, { 65, 9 }, { 68, 3 },       /* C: foreground, out of range index, regular */
    { 64, 4 }, { 65, 0 },                       /* D: first layer transparent in palette 0 */
    { 64, 4 },                                  /* E: only a transparent layer in palette 0 */
    { 66, 5 }, { 64, 0 }, { 68, 1 }, { 65, 2 }, /* a: four layers */
    { 64, 0 }, { 64, 5 }, { 64, 4 }, { 64, 3 }, { 64, 2 }, { 64, 1 }, { 64, 0 }, { 64, 5 }, { 64, 4 }, { 64, 3 },   /* digits */
};
#define NBASE ((int)(sizeof g_base / sizeof g_base[0]))
#define NLAY ((int)(sizeof g_lay / sizeof g_lay[0]))
int cg_layers_of(unsigned gid, const ttf_colr_layer **layers) {
    int i; for (i = 0; i < NBASE; i++) if (g_base[i].gid == gid) { if (layers) *layers = &g_lay[g_base[i].first]; return g_base[i].n; }
    return 0;
}
const char *const cg_svg_docs[3] = {
    "<svg xmlns=\"http://www.w3.org/2000/svg\"><circle id=\"glyph3\" r=\"40\"/></svg>",
    "<svg xmlns=\"http://www.w3.org/2000/svg\"><rect id=\"glyph7\" width=\"80\" height=\"80\"/></svg>",
    "<svg xmlns=\"http://www.w3.org/2000/svg\"><path id=\"glyph29\" d=\"M0 0L10 0L10 10z\"/></svg>",
};
int cg_svg_doc_of(unsigned gid) { return gid == 3 ? 0 : gid == 7 ? 1 : (gid == 29 || gid == 30) ? 2 : -1; }
const uint8_t cg_png_blob[] = {
    0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 0x0D, 'I', 'H', 'D', 'R', 0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0, 0x1F, 0x15, 0xC4, 0x89,
    0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xAE, 0x42, 0x60, 0x82 };
const unsigned cg_png_len = (unsigned)sizeof cg_png_blob;

/* ---------------------------------------------------------------------------------------------------- builders */
static void p16(uint8_t *p, unsigned o, unsigned v) { p[o] = (uint8_t)(v >> 8); p[o + 1] = (uint8_t)v; }
static void p32(uint8_t *p, unsigned o, unsigned v) { p16(p, o, v >> 16); p16(p, o + 2, v & 0xFFFF); }
uint8_t *cg_build_colr(const ttf_colr_base *b, int nb, const ttf_colr_layer *l, int nl, size_t *n) {
    size_t sz = 14 + 6 * (size_t)nb + 4 * (size_t)nl; uint8_t *p = (uint8_t *)calloc(1, sz); int i;
    p16(p, 0, 0); p16(p, 2, nb); p32(p, 4, 14); p32(p, 8, 14 + 6 * nb); p16(p, 12, nl);
    for (i = 0; i < nb; i++) { p16(p, 14 + 6 * i, b[i].gid); p16(p, 16 + 6 * i, b[i].first); p16(p, 18 + 6 * i, b[i].n); }
    for (i = 0; i < nl; i++) { p16(p, 14 + 6 * nb + 4 * i, l[i].gid); p16(p, 16 + 6 * nb + 4 * i, l[i].pal); }
    *n = sz; return p;
}
uint8_t *cg_build_cpal(int npal, int nent, const uint32_t *argb, size_t *n) {
    size_t hdr = 12 + 2 * (size_t)npal, sz = hdr + 4 * (size_t)npal * nent; uint8_t *p = (uint8_t *)calloc(1, sz); int i;
    p16(p, 0, 0); p16(p, 2, nent); p16(p, 4, npal); p16(p, 6, npal * nent); p32(p, 8, (unsigned)hdr);
    for (i = 0; i < npal; i++) p16(p, 12 + 2 * i, i * nent);
    for (i = 0; i < npal * nent; i++) { uint32_t c = argb[i]; uint8_t *r = p + hdr + 4 * i; r[0] = c & 0xFF; r[1] = (c >> 8) & 0xFF; r[2] = (c >> 16) & 0xFF; r[3] = c >> 24; }
    *n = sz; return p;
}
static uint8_t *build_svg_tbl(size_t *n) {
    size_t sz = 10 + 2 + 3 * 12, i; uint8_t *p; unsigned off = 2 + 3 * 12;
    static const unsigned st[3] = { 3, 7, 29 }, en[3] = { 3, 7, 30 };
    for (i = 0; i < 3; i++) sz += strlen(cg_svg_docs[i]);
    p = (uint8_t *)calloc(1, sz);
    p16(p, 0, 0); p32(p, 2, 10); p32(p, 6, 0); p16(p, 10, 3);
    for (i = 0; i < 3; i++) {
        unsigned l = (unsigned)strlen(cg_svg_docs[i]);
        p16(p, 12 + 12 * (unsigned)i, st[i]); p16(p, 14 + 12 * (unsigned)i, en[i]); p32(p, 16 + 12 * (unsigned)i, off); p32(p, 20 + 12 * (unsigned)i, l);
        memcpy(p + 10 + off, cg_svg_docs[i], l); off += l;
    }
    *n = sz; return p;
}
static void build_cb(uint8_t **cblc, size_t *nlc, uint8_t **cbdt, size_t *ndt) {
    unsigned rec = 9 + cg_png_len, s, g;
    uint8_t *lc = (uint8_t *)calloc(1, 8 + 96 + 2 * 28), *dt = (uint8_t *)calloc(1, 4 + 4 * rec);
    p16(lc, 0, 3); p16(lc, 2, 0); p32(lc, 4, 2);
    p16(dt, 0, 3); p16(dt, 2, 0);
    for (s = 0; s < 2; s++) {
        unsigned bs = 8 + 48 * s, arr = 104 + 28 * s, ppem = s ? 32 : 16, base = 4 + 2 * rec * s;
        p32(lc, bs, arr); p32(lc, bs + 4, 28); p32(lc, bs + 8, 1); p32(lc, bs + 12, 0);
        lc[bs + 16] = 12; lc[bs + 17] = (uint8_t)-4; lc[bs + 18] = (uint8_t)ppem; lc[bs + 19] = 1;       /* hori line metrics */
        lc[bs + 28] = 12; lc[bs + 29] = (uint8_t)-4; lc[bs + 30] = (uint8_t)ppem; lc[bs + 31] = 1;       /* vert line metrics */
        p16(lc, bs + 40, 8); p16(lc, bs + 42, 9); lc[bs + 44] = (uint8_t)ppem; lc[bs + 45] = (uint8_t)ppem; lc[bs + 46] = 32; lc[bs + 47] = 1;
        p16(lc, arr, 8); p16(lc, arr + 2, 9); p32(lc, arr + 4, 8);                               /* index subtable array */
        p16(lc, arr + 8, 1); p16(lc, arr + 10, 17); p32(lc, arr + 12, base);                     /* format 1, image format 17 */
        p32(lc, arr + 16, 0); p32(lc, arr + 20, rec); p32(lc, arr + 24, 2 * rec);
        for (g = 0; g < 2; g++) {
            uint8_t *r = dt + base + g * rec;
            r[0] = (uint8_t)ppem; r[1] = (uint8_t)ppem; r[2] = 0; r[3] = (uint8_t)(ppem - 4); r[4] = (uint8_t)ppem;     /* small glyph metrics */
            p32(r, 5, cg_png_len); memcpy(r + 9, cg_png_blob, cg_png_len);
        }
    }
    *cblc = lc; *nlc = 8 + 96 + 2 * 28; *cbdt = dt; *ndt = 4 + 4 * rec;
}
uint8_t *cg_build_fx(unsigned fx, size_t *n) {
    ttf_spec s; ttf_rawtable raw[6]; int nraw = 0; uint32_t flat[CG_NPAL * CG_NENT]; uint8_t *colr, *cpal, *svg = NULL, *lc = NULL, *dt = NULL, *out;
    size_t ncolr, ncpal, nsvg = 0, nlc = 0, ndt = 0;
    memcpy(flat, cg_pal, sizeof flat);
    ttf_default_spec(&s);
    colr = cg_build_colr(g_base, NBASE, g_lay, NLAY, &ncolr); cpal = cg_build_cpal(CG_NPAL, CG_NENT, flat, &ncpal);
    raw[nraw].data = colr; raw[nraw].len = (unsigned)ncolr; memcpy(raw[nraw].tag, "COLR", 5); nraw++;
    raw[nraw].data = cpal; raw[nraw].len = (unsigned)ncpal; memcpy(raw[nraw].tag, "CPAL", 5); nraw++;
    if (fx & CGFX_SVG) { svg = build_svg_tbl(&nsvg); raw[nraw].data = svg; raw[nraw].len = (unsigned)nsvg; memcpy(raw[nraw].tag, "SVG ", 5); nraw++; }
    if (fx & CGFX_PNG) {
        build_cb(&lc, &nlc, &dt, &ndt);
        raw[nraw].data = lc; raw[nraw].len = (unsigned)nlc; memcpy(raw[nraw].tag, "CBLC", 5); nraw++;
        raw[nraw].data = dt; raw[nraw].len = (unsigned)ndt; memcpy(raw[nraw].tag, "CBDT", 5); nraw++;
    }
    s.raw = raw; s.nraw = nraw;
    out = ttf_build(&s, n);
    free(colr); free(cpal); free(svg); free(lc); free(dt);
    return out;
}

/* ---------------------------------------------------------------------------------------------------- open/close */
IDWriteFactory2 *cg_f2(void) { return (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2); }
IDWriteFactory4 *cg_f4(void) { return (IDWriteFactory4 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 4); }
static int finish_open(cgfont *x) {
    if (!x->face) { t_skip("face unavailable"); return 0; }
    IDWriteFontFace_QueryInterface(x->face, &IID_IDWriteFontFace2, (void **)&x->f2);
    IDWriteFontFace_QueryInterface(x->face, &IID_IDWriteFontFace4, (void **)&x->f4);
    if (!x->f2) { t_skip("IDWriteFontFace2 unsupported"); return 0; }
    return 1;
}
int cg_open_blob(cgfont *x, const void *blob, size_t n) {
    IDWriteFactory *f = dw_shared(); HRESULT hr;
    memset(x, 0, sizeof *x);
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    x->owns = 1;
    hr = dwx_font_open(&x->h, f, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (FAILED(hr) || !x->h.face) { t_skip("cannot open synthetic face: %s", dw_hr_name(hr)); return 0; }
    x->face = x->h.face;
    return finish_open(x);
}
int cg_open_plain(cgfont *x) { size_t n = 0; uint8_t *b = ttf_build_default(&n); int ok = b && cg_open_blob(x, b, n); if (!b) { memset(x, 0, sizeof *x); t_skip("ttf_build failed"); } free(b); return ok; }
int cg_open_fx(cgfont *x, unsigned fx) { size_t n = 0; uint8_t *b = cg_build_fx(fx, &n); int ok = b && cg_open_blob(x, b, n); if (!b) { memset(x, 0, sizeof *x); t_skip("ttf_build failed"); } free(b); return ok; }
int cg_open_sys(cgfont *x, const WCHAR *fam) {
    IDWriteFactory *f = dw_shared();
    memset(x, 0, sizeof *x);
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    x->face = dw_sysface(f, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!x->face) { t_skip("system font family not installed"); return 0; }
    return finish_open(x);
}
void cg_close(cgfont *x) {
    DW_RELEASE(x->f4); DW_RELEASE(x->f2);
    if (x->owns) dwx_font_close(&x->h); else DW_RELEASE(x->face);
    memset(x, 0, sizeof *x);
}

/* ---------------------------------------------------------------------------------------------------- runs */
int cg_design_adv(unsigned gid) { return gid == 0 ? 1200 : gid == 67 ? 500 : 1000; }
void cg_mkrun(cgrun *r, IDWriteFontFace *face, float em, const UINT16 *g, int n, int with_adv, int with_off, BOOL sideways, UINT32 bidi) {
    int i;
    memset(r, 0, sizeof *r);
    for (i = 0; i < n; i++) {
        r->gl[i] = g[i];
        r->adv[i] = with_adv == 2 ? (float)(7 + 3 * i + g[i] % 5) : (float)cg_design_adv(g[i]) * em / 2048.0f;
        r->off[i].advanceOffset = 0.25f * (float)(i + 1); r->off[i].ascenderOffset = -0.5f * (float)(i + 1);
    }
    r->run.fontFace = face; r->run.fontEmSize = em; r->run.glyphCount = (UINT32)n; r->run.glyphIndices = r->gl;
    r->run.glyphAdvances = with_adv ? r->adv : NULL; r->run.glyphOffsets = with_off ? r->off : NULL;
    r->run.isSideways = sideways; r->run.bidiLevel = bidi;
}

/* ---------------------------------------------------------------------------------------------------- oracle */
const cg_model cg_fixture_model = { g_base, NBASE, g_lay, NLAY, CG_NPAL, CG_NENT, &cg_pal[0][0], 1 };
const cg_model *cg_model_override;
static int m_layers(const cg_model *m, unsigned gid, const ttf_colr_layer **layers) {
    int lo = 0, hi = m->nbase - 1;
    while (lo <= hi) { int mid = (lo + hi) / 2; if (m->base[mid].gid == gid) { if (layers) *layers = &m->lay[m->base[mid].first]; return m->base[mid].n; } if (m->base[mid].gid < gid) lo = mid + 1; else hi = mid - 1; }
    return 0;
}
int cg_expect(const DWRITE_GLYPH_RUN *run, float ox, float oy, UINT32 req, UINT32 face_formats, UINT32 palette, cg_exp *out) {
    const cg_model *m = cg_model_override ? cg_model_override : &cg_fixture_model;
    UINT32 used = req & face_formats, n = run->glyphCount, i, ncolor = 0, nout = 0; int kind[64]; float adv[64], cum[65], nan = (float)NAN; int unknown_adv;
    if (palette >= (UINT32)m->npal) used &= ~(UINT32)CGF_COLR;
    if (!(used & 0xFC) || n == 0 || n > 64) return 0;
    for (i = 0; i < n; i++) {
        unsigned gid = run->glyphIndices[i];
        kind[i] = 0;
        if ((used & CGF_SVG) && m->svg && cg_svg_doc_of(gid) >= 0) kind[i] = 2;
        else if ((used & CGF_COLR) && m_layers(m, gid, NULL) > 0) kind[i] = 1;
        if (kind[i]) ncolor++;
    }
    if (!ncolor) return 0;
    unknown_adv = !run->glyphAdvances && run->isSideways;
    for (i = 0; i < n; i++) adv[i] = run->glyphAdvances ? run->glyphAdvances[i] : unknown_adv ? nan : (float)cg_design_adv(run->glyphIndices[i]) * run->fontEmSize / 2048.0f;
    cum[0] = 0; for (i = 0; i < n; i++) cum[i + 1] = cum[i] + adv[i];
    {   /* base run */
        cg_exp *e = &out[nout++]; memset(e, 0, sizeof *e);
        e->base = 1; e->n = n; e->x = ox; e->y = oy; e->pal = 0xFFFF; e->fmt = (ncolor == n) ? 0 : (used & 3);
        for (i = 0; i < n && i < 8; i++) { e->gl[i] = kind[i] ? 1 : run->glyphIndices[i]; e->adv[i] = adv[i]; e->advmask |= 1u << i; }
    }
    for (i = 0; i < n; i++) {
        if (kind[i] == 1) {
            const ttf_colr_layer *L; int nl = m_layers(m, run->glyphIndices[i], &L), j;
            for (j = 0; j < nl; j++) {
                unsigned pi = L[j].pal; cg_exp *e;
                if (pi < (unsigned)m->nent && (m->argb[palette * m->nent + pi] >> 24) == 0) continue;
                e = &out[nout++]; memset(e, 0, sizeof *e);
                e->n = 1; e->gl[0] = L[j].gid; e->first_glyph = (int)i; e->fmt = (used & 3) | CGF_COLR; e->y = oy;
                e->x = (run->bidiLevel & 1) ? ox - cum[i] : ox + cum[i];
                e->adv[0] = adv[0]; if (i == 0) e->advmask = 1;
                if (pi < (unsigned)m->nent) { uint32_t c = m->argb[palette * m->nent + pi]; e->c[0] = ((c >> 16) & 255) / 255.0f; e->c[1] = ((c >> 8) & 255) / 255.0f; e->c[2] = (c & 255) / 255.0f; e->c[3] = (c >> 24) / 255.0f; e->pal = (UINT16)pi; }
                else e->pal = 0xFFFF;
            }
        } else if (kind[i] == 2) {
            cg_exp *e = &out[nout++]; UINT32 j = i, m = 0; memset(e, 0, sizeof *e);
            e->svg = 1; e->first_glyph = (int)i; e->fmt = CGF_SVG; e->y = oy; e->pal = 0xFFFF; e->x = (run->bidiLevel & 1) ? ox - cum[i] : ox + cum[i];
            e->gl[m] = run->glyphIndices[j];
            while (j + 1 < n && kind[j + 1] == 2 && m + 1 < 8) {
                e->adv[m] = cum[j + 1] - cum[j]; e->advmask |= 1u << m; j++; m++; e->gl[m] = run->glyphIndices[j];
            }
            e->adv[m] = adv[j]; e->advmask |= 1u << m; e->n = m + 1;
            if (m == 0) { e->adv[0] = adv[0]; e->advmask = (i == 0); }
            kind[i] = 3; { UINT32 k; for (k = i + 1; k <= j; k++) kind[k] = 3; }
        }
    }
    return (int)nout;
}

/* ---------------------------------------------------------------------------------------------------- walker */
static int near_(float a, float b) { if (a != a || b != b) return 1; return fabsf(a - b) <= 1e-3f * (1.0f + fabsf(b)); }
int cg_walk(IDWriteColorGlyphRunEnumerator *e, const char *tag, const DWRITE_GLYPH_RUN *in, const cg_exp *exp, int nexp, DWRITE_MEASURING_MODE mm) {
    IDWriteColorGlyphRunEnumerator1 *e1 = NULL; int i = 0; HRESULT hr;
    IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1);
    for (;;) {
        BOOL has = (BOOL)0x55; const DWRITE_COLOR_GLYPH_RUN *r = (const DWRITE_COLOR_GLYPH_RUN *)(ULONG_PTR)1; const DWRITE_COLOR_GLYPH_RUN1 *r1 = NULL; UINT32 k;
        hr = IDWriteColorGlyphRunEnumerator_MoveNext(e, &has);
        CHECKF(hr == S_OK, "%s: MoveNext #%d -> %s", tag, i, dw_hr_name(hr));
        if (hr != S_OK || !has) { CHECKF(has == 0 || hr != S_OK, "%s: hasRun %d", tag, has); break; }
        CHECKF(has == 1, "%s: hasRun is %d, not TRUE", tag, has);
        hr = IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &r);
        CHECKF(hr == S_OK && r != NULL, "%s: GetCurrentRun #%d -> %s %p", tag, i, dw_hr_name(hr), (const void *)r);
        if (hr != S_OK || !r) break;
        if (e1) {
            hr = IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e1, &r1);
            CHECKF(hr == S_OK && (const void *)r1 == (const void *)r, "%s: v1 GetCurrentRun #%d -> %s %p vs %p", tag, i, dw_hr_name(hr), (const void *)r1, (const void *)r);
        }
        if (i >= nexp) { CHECKF(0, "%s: unexpected extra run #%d (%u glyphs)", tag, i, r->glyphRun.glyphCount); i++; if (i > 200) break; continue; }
        {
            const cg_exp *x = &exp[i]; const DWRITE_GLYPH_RUN *g = &r->glyphRun;
            CHECKF(g->fontFace == in->fontFace, "%s #%d: fontFace %p want %p", tag, i, (void *)g->fontFace, (void *)in->fontFace);
            CHECKF(g->fontEmSize == in->fontEmSize, "%s #%d: em %g want %g", tag, i, g->fontEmSize, in->fontEmSize);
            CHECKF(g->isSideways == in->isSideways, "%s #%d: sideways %d", tag, i, g->isSideways);
            CHECKF(g->bidiLevel == in->bidiLevel, "%s #%d: bidi %u", tag, i, g->bidiLevel);
            CHECKF(r->glyphRunDescription == NULL, "%s #%d: description %p", tag, i, (void *)r->glyphRunDescription);
            CHECKF(g->glyphCount == x->n, "%s #%d: glyphCount %u want %u", tag, i, g->glyphCount, x->n);
            CHECKF(g->glyphIndices != NULL && g->glyphIndices != in->glyphIndices, "%s #%d: indices %p (input %p)", tag, i, (const void *)g->glyphIndices, (const void *)in->glyphIndices);
            CHECKF(g->glyphAdvances != NULL, "%s #%d: advances NULL", tag, i);
            if (g->glyphIndices && g->glyphCount == x->n) for (k = 0; k < x->n && k < 8; k++) CHECKF(g->glyphIndices[k] == x->gl[k], "%s #%d: glyph[%u] %u want %u", tag, i, k, g->glyphIndices[k], x->gl[k]);
            if (g->glyphAdvances && g->glyphCount == x->n) for (k = 0; k < x->n && k < 8; k++) if (x->advmask & (1u << k)) CHECKF(near_(g->glyphAdvances[k], x->adv[k]), "%s #%d: advance[%u] %g want %g", tag, i, k, g->glyphAdvances[k], x->adv[k]);
            CHECKF(near_(r->baselineOriginX, x->x), "%s #%d: x %g want %g", tag, i, r->baselineOriginX, x->x);
            CHECKF(near_(r->baselineOriginY, x->y), "%s #%d: y %g want %g", tag, i, r->baselineOriginY, x->y);
            CHECKF(r->paletteIndex == x->pal, "%s #%d: paletteIndex 0x%x want 0x%x", tag, i, r->paletteIndex, x->pal);
            CHECKF(near_(r->runColor.r, x->c[0]) && near_(r->runColor.g, x->c[1]) && near_(r->runColor.b, x->c[2]) && near_(r->runColor.a, x->c[3]),
                   "%s #%d: color %g %g %g %g want %g %g %g %g", tag, i, r->runColor.r, r->runColor.g, r->runColor.b, r->runColor.a, x->c[0], x->c[1], x->c[2], x->c[3]);
            if (r1) {
                CHECKF((UINT32)r1->glyphImageFormat == x->fmt, "%s #%d: glyphImageFormat 0x%x want 0x%x", tag, i, r1->glyphImageFormat, x->fmt);
                CHECKF(r1->measuringMode == mm, "%s #%d: measuringMode %d want %d", tag, i, r1->measuringMode, mm);
            }
        }
        i++;
    }
    CHECKF(i == nexp, "%s: %d runs enumerated, want %d", tag, i, nexp);
    if (e1) IDWriteColorGlyphRunEnumerator1_Release(e1);
    return i;
}
HRESULT cg_check_f2(IDWriteFactory2 *f, const char *tag, const DWRITE_GLYPH_RUN *run, float ox, float oy, DWRITE_MEASURING_MODE mm, const DWRITE_MATRIX *m, UINT32 palette, UINT32 face_formats) {
    static cg_exp exp[CG_MAXEXP]; IDWriteColorGlyphRunEnumerator *e = (IDWriteColorGlyphRunEnumerator *)(ULONG_PTR)0x12345678;
    int n = cg_expect(run, ox, oy, CGF_TT | CGF_CFF | CGF_COLR, face_formats, palette, exp);
    HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f, ox, oy, run, NULL, mm, m, palette, &e);
    if (!n) {
        CHECKF(hr == CG_E_NOCOLOR, "%s: want DWRITE_E_NOCOLOR, got %s", tag, dw_hr_name(hr));
        CHECKF(e == NULL, "%s: out pointer not zeroed (%p)", tag, (void *)e);
        if (e && e != (void *)(ULONG_PTR)0x12345678) IDWriteColorGlyphRunEnumerator_Release(e);
        return hr;
    }
    CHECKF(hr == S_OK, "%s: want S_OK, got %s", tag, dw_hr_name(hr));
    if (hr == S_OK && e) { cg_walk(e, tag, run, exp, n, mm); IDWriteColorGlyphRunEnumerator_Release(e); }
    return hr;
}
HRESULT cg_check_f4(IDWriteFactory4 *f, const char *tag, const DWRITE_GLYPH_RUN *run, float ox, float oy, UINT32 formats, DWRITE_MEASURING_MODE mm, const DWRITE_MATRIX *m, UINT32 palette, UINT32 face_formats) {
    static cg_exp exp[CG_MAXEXP]; IDWriteColorGlyphRunEnumerator1 *e = (IDWriteColorGlyphRunEnumerator1 *)(ULONG_PTR)0x12345678; D2D1_POINT_2F o;
    int n = cg_expect(run, ox, oy, formats, face_formats, palette, exp); HRESULT hr;
    o.x = ox; o.y = oy;
    hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)formats, mm, m, palette, &e);
    if (!n) {
        CHECKF(hr == CG_E_NOCOLOR, "%s: want DWRITE_E_NOCOLOR, got %s", tag, dw_hr_name(hr));
        CHECKF(e == NULL, "%s: out pointer not zeroed (%p)", tag, (void *)e);
        if (e && e != (void *)(ULONG_PTR)0x12345678) IDWriteColorGlyphRunEnumerator1_Release(e);
        return hr;
    }
    CHECKF(hr == S_OK, "%s: want S_OK, got %s", tag, dw_hr_name(hr));
    if (hr == S_OK && e) { cg_walk((IDWriteColorGlyphRunEnumerator *)e, tag, run, exp, n, mm); IDWriteColorGlyphRunEnumerator1_Release(e); }
    return hr;
}

/* ---------------------------------------------------------------------------------------------------- COLR/CPAL v0 model of a real face */
static unsigned r16(const uint8_t *p) { return (unsigned)(p[0] << 8 | p[1]); }
static unsigned r32(const uint8_t *p) { return (unsigned)r16(p) << 16 | r16(p + 2); }
int cg_model_from_face(IDWriteFontFace *face, cg_model *m) {
    UINT32 cl = 0, pl = 0; uint8_t *colr = dw_face_table(face, DW_TAG('C','O','L','R'), &cl), *cpal = dw_face_table(face, DW_TAG('C','P','A','L'), &pl);
    int ok = 0, i; ttf_colr_base *b = NULL; ttf_colr_layer *l = NULL; uint32_t *argb = NULL;
    memset(m, 0, sizeof *m);
    if (!colr || !cpal || cl < 14 || pl < 12 || r16(colr) != 0 || r16(cpal) > 1) goto done;
    {
        unsigned nb = r16(colr + 2), bo = r32(colr + 4), lo = r32(colr + 8), nl = r16(colr + 12);
        unsigned nent = r16(cpal + 2), np = r16(cpal + 4), ro = r32(cpal + 8);
        if (bo + 6ull * nb > cl || lo + 4ull * nl > cl || 12ull + 2ull * np > pl) goto done;
        b = (ttf_colr_base *)calloc(nb + 1, sizeof *b); l = (ttf_colr_layer *)calloc(nl + 1, sizeof *l); argb = (uint32_t *)calloc((size_t)np * nent + 1, 4);
        for (i = 0; i < (int)nb; i++) { b[i].gid = (unsigned short)r16(colr + bo + 6 * i); b[i].first = (unsigned short)r16(colr + bo + 6 * i + 2); b[i].n = (unsigned short)r16(colr + bo + 6 * i + 4); }
        for (i = 0; i < (int)nl; i++) { l[i].gid = (unsigned short)r16(colr + lo + 4 * i); l[i].pal = (unsigned short)r16(colr + lo + 4 * i + 2); }
        for (i = 0; i < (int)np; i++) {
            unsigned first = r16(cpal + 12 + 2 * i), k;
            for (k = 0; k < nent; k++) {
                unsigned o = ro + 4 * (first + k);
                if (o + 4 > pl) goto done;
                argb[i * nent + k] = (uint32_t)cpal[o + 3] << 24 | (uint32_t)cpal[o + 2] << 16 | (uint32_t)cpal[o + 1] << 8 | cpal[o];
            }
        }
        for (i = 0; i < (int)nb; i++) if (b[i].first + (unsigned)b[i].n > nl) goto done;
        for (i = 1; i < (int)nb; i++) if (b[i - 1].gid >= b[i].gid) goto done;
        m->base = b; m->nbase = (int)nb; m->lay = l; m->nlay = (int)nl; m->npal = (int)np; m->nent = (int)nent; m->argb = argb; m->svg = 0;
        ok = 1;
    }
done:
    free(colr); free(cpal);
    if (!ok) { free(b); free(l); free(argb); memset(m, 0, sizeof *m); }
    return ok;
}
void cg_model_free(cg_model *m) { free((void *)m->base); free((void *)m->lay); free((void *)m->argb); memset(m, 0, sizeof *m); }
