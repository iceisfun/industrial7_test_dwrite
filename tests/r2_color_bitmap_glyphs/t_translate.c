/* IDWriteFactory4::TranslateColorGlyphRun over bitmap (sbix) and COLR glyphs, walked through IDWriteColorGlyphRunEnumerator1.
 *
 * dwrite.dll behaviour reproduced by the oracle below (DWriteColorGlyphRunEnumerator::Create !0x1caf0, FindColorGlyphs !0x1d228, InitializeColorGlyphPositions
 * !0x1d050, AdvanceToBaseRun !0x1dd3c, AdvanceToColorRun !0x1db78, MoveNext !0x1daf0):
 *   used = requested & face formats, COLR dropped when the palette index is >= the palette count; nothing with a bit in 0xFC left -> DWRITE_E_NOCOLOR.
 *   Bitmap/SVG glyphs: first entry (increasing ppem) of the glyph whose format is in `used` and whose ppem >= round(maxScale(matrix) * fontEmSize); COLR glyphs:
 *   glyphs with layers when COLR is in `used`.  A glyph with a bitmap entry is a bitmap glyph even if it also has COLR layers.
 *   First run = the whole input run, every colour glyph replaced by glyph 1, format (used & 3) or NONE when every glyph is a colour glyph, palette 0xFFFF, zero colour.
 *   Then the colour glyphs in run order: a COLR glyph yields one single-glyph run per layer whose palette entry is not fully transparent (format (used&3)|COLR, measuring mode as
 *   requested); a bitmap glyph yields one run that also swallows the NEXT colour glyphs (plain glyphs in between are invisible) while their bitmap entry has the same
 *   format; its glyph advances are the position differences (last = that glyph's own advance), its measuring mode field is 0 and its palette index 0xFFFF. */
#include "crb.h"

#define F_ALLC (F_TT | F_COLR | F_PNG | F_JPEG | F_TIFF | F_SVG)
#define TY_PNG CRB_TAG('p','n','g',' ')
#define TY_JPG CRB_TAG('j','p','g',' ')
#define TY_TIF CRB_TAG('t','i','f','f')
typedef struct { unsigned gid, ppem, fmt; } bent;
static const bent bm[] = { { 2, 16, F_PNG }, { 3, 16, F_JPEG }, { 2, 32, F_TIFF }, { 3, 32, F_PNG }, { 4, 32, F_PNG }, { 5, 32, F_JPEG }, { 6, 32, F_JPEG }, { 7, 32, F_PNG }, { 8, 64, F_PNG } };
#define NBM ((int)(sizeof bm / sizeof bm[0]))
static const crb_cbase cbase[] = { { 2, 0, 1 }, { 10, 1, 2 }, { 11, 3, 1 }, { 12, 4, 1 }, { 13, 5, 1 }, { 14, 6, 3 } };
static const crb_clayer clay[] = { { 64, 0 }, { 64, 0 }, { 65, 1 }, { 68, 2 }, { 64, 0xFFFF }, { 65, 9 }, { 64, 1 }, { 65, 0 }, { 68, 2 } };
#define NCB ((int)(sizeof cbase / sizeof cbase[0]))
static const uint32_t pal[2][3] = { { 0xFFFF0000u, 0x8000FF00u, 0x400000FFu }, { 0xFF0000FFu, 0x00FFFFFFu, 0xFF445566u } };

uint8_t *crb_translate_fixture(size_t *n) {
    static crb_sbrec A[3], B[8], C[1]; static crb_sbstrike st[3]; size_t l[9], nc, np, sn; uint8_t *p[9], *colr, *cpal, *sbx; ttf_rawtable raw[3]; unsigned first[2] = { 0, 3 }; int i;
    p[0] = crb_png(8, 8, 1, &l[0]); p[1] = crb_jpeg(8, 8, 2, 0, &l[1]); p[2] = crb_tiff(8, 8, 0, 3, 3, 0, 3, &l[2]);
    A[0] = (crb_sbrec){ 2, TY_PNG, 0, 0, p[0], l[0], 0 }; A[1] = (crb_sbrec){ 3, TY_JPG, 0, 0, p[1], l[1], 0 };
    B[0] = (crb_sbrec){ 2, TY_TIF, 0, 0, p[2], l[2], 0 }; B[1] = (crb_sbrec){ 3, TY_PNG, 0, 0, p[0], l[0], 0 }; B[2] = (crb_sbrec){ 4, TY_PNG, 0, 0, p[0], l[0], 0 };
    B[3] = (crb_sbrec){ 5, TY_JPG, 0, 0, p[1], l[1], 0 }; B[4] = (crb_sbrec){ 6, TY_JPG, 0, 0, p[1], l[1], 0 }; B[5] = (crb_sbrec){ 7, TY_PNG, 0, 0, p[0], l[0], 0 };
    C[0] = (crb_sbrec){ 8, TY_PNG, 0, 0, p[0], l[0], 0 };
    st[0] = (crb_sbstrike){ 16, 72, A, 2 }; st[1] = (crb_sbstrike){ 32, 72, B, 6 }; st[2] = (crb_sbstrike){ 64, 72, C, 1 };
    sbx = crb_sbix(st, 3, TTF_DEF_NGLYPHS, 1, &sn);
    colr = crb_colr(0, cbase, NCB, clay, 9, &nc); cpal = crb_cpal(0, 3, 2, first, 6, &pal[0][0], &np);
    memcpy(raw[0].tag, "sbix", 5); raw[0].data = sbx; raw[0].len = (unsigned)sn; memcpy(raw[1].tag, "COLR", 5); raw[1].data = colr; raw[1].len = (unsigned)nc; memcpy(raw[2].tag, "CPAL", 5); raw[2].data = cpal; raw[2].len = (unsigned)np;
    { uint8_t *out = crb_blob(raw, 3, 0, n); free(sbx); free(colr); free(cpal); for (i = 0; i < 3; i++) free(p[i]); return out; }
}

typedef struct { int bitmap; unsigned fmt; const crb_clayer *lay; int nlay; int idx; float x, adv; int consumed; } cg_t;
typedef struct { unsigned n; UINT16 gl[16]; float adv[16]; float x, y; float col[4]; UINT16 pal; UINT32 fmt; int mm; } erun;
static const bent *lookup(unsigned gid, unsigned ppem, unsigned used) {
    int i; const bent *best = NULL;
    for (i = 0; i < NBM; i++) if (bm[i].gid == gid && (bm[i].fmt & used) && bm[i].ppem >= ppem && (!best || bm[i].ppem < best->ppem)) best = &bm[i];
    return best;
}
static double max_scale(const DWRITE_MATRIX *m) {
    double a, b, c, d, p, q, r, s, t;
    if (!m) return 1.0;
    if (m->m12 == 0 && m->m21 == 0) return fabs(m->m22) > fabs(m->m11) ? fabs(m->m22) : fabs(m->m11);
    a = m->m11; b = m->m12; c = m->m21; d = m->m22;
    p = a * a + b * b; q = c * c + d * d; r = d * b + c * a; s = p - q; t = sqrt(r * 4 * r + s * s);
    return sqrt((q + p + t) * 0.5);
}
/* Oracle: fills out[], returns the number of runs or -1 for DWRITE_E_NOCOLOR. */
static int oracle(const DWRITE_GLYPH_RUN *run, float ox, float oy, UINT32 req, UINT32 palette, DWRITE_MEASURING_MODE mm, const DWRITE_MATRIX *mat, erun *out) {
    UINT32 used = req & (F_TT | F_COLR | F_PNG | F_JPEG | F_TIFF);   /* & face formats */ static cg_t cgs[64]; int ncg = 0, nout = 0, i, j; unsigned ppem, n = run->glyphCount; float cum[65]; int rtl = run->bidiLevel & 1;
    if (palette >= 2) used &= ~(UINT32)F_COLR;
    if (!(used & 0xFC)) return -1;
    { double v = max_scale(mat) * run->fontEmSize; ppem = (unsigned)floor(v + 0.5); }
    cum[0] = 0; for (i = 0; i < (int)n; i++) cum[i + 1] = cum[i] + run->glyphAdvances[i];
    for (i = 0; i < (int)n; i++) {
        const bent *e = (used & 0xF8) ? lookup(run->glyphIndices[i], ppem, used) : NULL; const crb_cbase *cb = NULL; int k;
        if (used & F_COLR) for (k = 0; k < NCB; k++) if (cbase[k].gid == run->glyphIndices[i]) cb = &cbase[k];
        if (!e && !cb) continue;
        memset(&cgs[ncg], 0, sizeof cgs[ncg]); cgs[ncg].idx = i; cgs[ncg].x = cum[i]; cgs[ncg].adv = run->glyphAdvances[i];
        if (e) { cgs[ncg].bitmap = 1; cgs[ncg].fmt = e->fmt; } else { cgs[ncg].lay = &clay[cb->first]; cgs[ncg].nlay = (int)cb->n; }
        ncg++;
    }
    if (!ncg) return -1;
    memset(&out[0], 0, sizeof out[0]); out[0].n = n; out[0].x = ox; out[0].y = oy; out[0].pal = 0xFFFF; out[0].mm = mm; out[0].fmt = ncg == (int)n ? 0 : (used & 3);
    for (i = 0; i < (int)n; i++) { int iscg = 0; for (j = 0; j < ncg; j++) if (cgs[j].idx == i) iscg = 1; out[0].gl[i] = iscg ? 1 : run->glyphIndices[i]; out[0].adv[i] = run->glyphAdvances[i]; }
    nout = 1;
    for (i = 0; i < ncg; i++) {
        cg_t *c = &cgs[i];
        if (c->bitmap) {
            erun *e; if (c->consumed) continue;
            e = &out[nout++]; memset(e, 0, sizeof *e); e->n = 1; e->gl[0] = run->glyphIndices[c->idx]; e->fmt = c->fmt; e->pal = 0xFFFF; e->mm = 0; e->y = oy;
            e->x = rtl ? ox - c->x : ox + c->x; e->adv[0] = c->adv;
            { float prev = c->x; for (j = i + 1; j < ncg && cgs[j].bitmap && cgs[j].fmt == c->fmt && !cgs[j].consumed; j++) {
                  e->adv[e->n - 1] = cgs[j].x - prev; prev = cgs[j].x; e->gl[e->n] = run->glyphIndices[cgs[j].idx]; e->adv[e->n] = cgs[j].adv; e->n++; cgs[j].consumed = 1; } }
        } else {
            for (j = 0; j < c->nlay; j++) {
                unsigned pi = c->lay[j].pal; erun *e;
                if (pi < 3 && (pal[palette][pi] >> 24) == 0) continue;
                e = &out[nout++]; memset(e, 0, sizeof *e); e->n = 1; e->gl[0] = c->lay[j].gid; e->fmt = (used & 3) | F_COLR; e->mm = mm; e->y = oy; e->x = rtl ? ox - c->x : ox + c->x;
                if (pi < 3) { uint32_t v = pal[palette][pi]; e->col[0] = ((v >> 16) & 255) / 255.0f; e->col[1] = ((v >> 8) & 255) / 255.0f; e->col[2] = (v & 255) / 255.0f; e->col[3] = (v >> 24) / 255.0f; e->pal = (UINT16)pi; } else e->pal = 0xFFFF;
            }
        }
    }
    return nout;
}
static int near_(float a, float b) { return fabsf(a - b) <= 1e-3f * (1.0f + fabsf(b)); }
/* Runs one translation and compares the whole enumeration with the oracle. Returns the HRESULT of the call. */
static HRESULT check(IDWriteFactory4 *f, crb *x, const char *tag, const UINT16 *g, int n, float em, int bidi, float ox, float oy, UINT32 fmts, UINT32 palette, DWRITE_MEASURING_MODE mm, const DWRITE_MATRIX *mat) {
    static erun exp[128]; static float adv[64]; DWRITE_GLYPH_RUN run; IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)1; HRESULT hr; int ne, k = 0, i; BOOL has;
    D2D1_POINT_2F o = { ox, oy };
    for (i = 0; i < n; i++) adv[i] = 7.0f + 3.0f * (float)i + (float)(g[i] % 5);
    memset(&run, 0, sizeof run); run.fontFace = x->h.face; run.fontEmSize = em; run.glyphCount = (UINT32)n; run.glyphIndices = g; run.glyphAdvances = adv; run.bidiLevel = (UINT32)bidi;
    ne = oracle(&run, ox, oy, fmts, palette, mm, mat, exp);
    hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, &run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)fmts, mm, mat, palette, &e);
    if (ne < 0) { CHECKF(hr == E_NOCOLOR_ && e == NULL, "%s: %s (e=%p), want DWRITE_E_NOCOLOR", tag, dw_hr_name(hr), (void *)e); if (e && SUCCEEDED(hr)) IDWriteColorGlyphRunEnumerator1_Release(e); return hr; }
    if (hr != S_OK || !e) { CHECKF(0, "%s: %s", tag, dw_hr_name(hr)); return hr; }
    while (SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has && k < 100) {
        const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; const erun *w = &exp[k];
        if (FAILED(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c)) || !c) { CHECKF(0, "%s: GetCurrentRun failed at run %d", tag, k); break; }
        if (getenv("CRB_VERBOSE")) t_note("%s run %d: n=%u g0=%u fmt=0x%x pal=%u x=%g mm=%d", tag, k, c->glyphRun.glyphCount, c->glyphRun.glyphIndices[0], c->glyphImageFormat, c->paletteIndex, c->baselineOriginX, c->measuringMode);
        if (k >= ne) { CHECKF(0, "%s: more runs than expected (%d)", tag, ne); k++; continue; }
        CHECKF(c->glyphRun.glyphCount == w->n, "%s run %d: glyphCount %u want %u", tag, k, c->glyphRun.glyphCount, w->n);
        if (c->glyphRun.glyphCount == w->n) {
            for (i = 0; i < (int)w->n; i++) {
                CHECKF(c->glyphRun.glyphIndices[i] == w->gl[i], "%s run %d glyph %d: %u want %u", tag, k, i, c->glyphRun.glyphIndices[i], w->gl[i]);
                if (k == 0 || w->n >= 2) CHECKF(near_(c->glyphRun.glyphAdvances[i], w->adv[i]), "%s run %d advance %d: %g want %g", tag, k, i, c->glyphRun.glyphAdvances[i], w->adv[i]);
            }
        }
        CHECKF(near_(c->baselineOriginX, w->x) && near_(c->baselineOriginY, w->y), "%s run %d: origin (%g,%g) want (%g,%g)", tag, k, c->baselineOriginX, c->baselineOriginY, w->x, w->y);
        CHECKF((UINT32)c->glyphImageFormat == w->fmt, "%s run %d: format 0x%x want 0x%x", tag, k, c->glyphImageFormat, w->fmt);
        CHECKF(c->paletteIndex == w->pal, "%s run %d: palette %u want %u", tag, k, c->paletteIndex, w->pal);
        CHECKF(near_(c->runColor.r, w->col[0]) && near_(c->runColor.g, w->col[1]) && near_(c->runColor.b, w->col[2]) && near_(c->runColor.a, w->col[3]), "%s run %d: color (%g %g %g %g)", tag, k, c->runColor.r, c->runColor.g, c->runColor.b, c->runColor.a);
        CHECKF((int)c->measuringMode == w->mm, "%s run %d: measuring mode %d want %d", tag, k, c->measuringMode, w->mm);
        CHECKF(c->glyphRun.fontFace == x->h.face && c->glyphRun.fontEmSize == em && c->glyphRun.bidiLevel == (UINT32)bidi && !c->glyphRun.isSideways, "%s run %d: face/em/bidi copy", tag, k);
        k++;
    }
    CHECKF(k == ne, "%s: %d runs, want %d", tag, k, ne);
    { BOOL again = TRUE; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &again)); CHECKF(!again, "%s: MoveNext after the end returned a run", tag); }
    IDWriteColorGlyphRunEnumerator1_Release(e);
    return hr;
}
#define NEED_F4() IDWriteFactory4 *f4 = crb_f4(); if (!f4) { t_skip("IDWriteFactory4 unsupported"); return; } size_t fn_; uint8_t *fb_ = crb_translate_fixture(&fn_); CRB_NEED(x, fb_, fn_)

T(translate_bitmap_fixture_formats) {
    NEED_F4();
    EXPECT_EQ(crb_face_formats(&x), F_TT | F_COLR | F_PNG | F_JPEG | F_TIFF);
    crb_close(&x);
}
T(translate_single_bitmap_glyphs_by_em) {
    NEED_F4(); static const float em[] = { 4.0f, 9.0f, 15.0f, 16.0f, 17.0f, 31.0f, 32.0f, 33.0f, 63.0f, 64.0f, 65.0f, 100.0f }; int i; unsigned g;
    for (g = 2; g <= 9; g++) for (i = 0; i < (int)(sizeof em / sizeof em[0]); i++) {
        char tag[64]; UINT16 gl = (UINT16)g; snprintf(tag, sizeof tag, "glyph %u em %g", g, em[i]);
        check(f4, &x, tag, &gl, 1, em[i], 0, 5, 6, F_TT | F_PNG | F_JPEG | F_TIFF, 0, DWRITE_MEASURING_MODE_NATURAL, NULL);
    }
    crb_close(&x);
}
T(translate_format_masks_select_entries) {
    NEED_F4(); static const UINT32 fm[] = { F_PNG, F_JPEG, F_TIFF, F_PNG | F_JPEG, F_PNG | F_TIFF, F_JPEG | F_TIFF, F_PNG | F_JPEG | F_TIFF, F_TT | F_PNG, F_COLR, F_COLR | F_PNG, F_TT, F_CFF, F_SVG, F_PREM, F_TT | F_COLR | F_PNG | F_JPEG | F_TIFF | F_SVG | F_PREM };
    static const UINT16 g[] = { 2, 3, 4, 5, 6, 7, 8, 2, 10, 11, 0, 14 }; unsigned i, em;
    for (i = 0; i < sizeof fm / sizeof fm[0]; i++) for (em = 8; em <= 40; em += 8) {
        char tag[64]; snprintf(tag, sizeof tag, "mask 0x%x em %u", fm[i], em);
        check(f4, &x, tag, g, 12, (float)em, 0, 1, 2, fm[i], 0, DWRITE_MEASURING_MODE_NATURAL, NULL);
    }
    crb_close(&x);
}
T(translate_consecutive_same_format_glyphs_merge) {
    NEED_F4(); static const UINT16 s[][8] = { { 3, 4 }, { 4, 3, 7 }, { 3, 4, 5 }, { 5, 6 }, { 4, 5, 6, 3 }, { 3, 4, 7, 3, 4, 7, 3, 4 } };
    static const int n[] = { 2, 3, 3, 2, 4, 8 }; int i;
    for (i = 0; i < 6; i++) { char tag[32]; snprintf(tag, sizeof tag, "seq %d", i); check(f4, &x, tag, s[i], n[i], 20.0f, 0, 11, 13, F_TT | F_PNG | F_JPEG | F_TIFF, 0, DWRITE_MEASURING_MODE_NATURAL, NULL); }
    crb_close(&x);
}
T(translate_plain_glyphs_between_bitmap_glyphs_do_not_split_a_group) {
    NEED_F4(); static const UINT16 s[][6] = { { 3, 0, 4 }, { 3, 1, 1, 4 }, { 4, 28, 7, 28, 28, 3 }, { 3, 0, 5, 0, 6 }, { 3, 0, 4, 0, 5, 0, 6, 0 } };
    static const int n[] = { 3, 4, 6, 5, 8 }; int i;
    for (i = 0; i < 5; i++) { char tag[32]; snprintf(tag, sizeof tag, "gap %d", i); check(f4, &x, tag, s[i], n[i], 20.0f, 0, 3, 4, F_TT | F_PNG | F_JPEG | F_TIFF, 0, DWRITE_MEASURING_MODE_NATURAL, NULL); }
    /* concrete case: glyph 3 and 4 both PNG@32 with two plain glyphs between them -> one run [3,4] whose first advance spans the gap */
    {
        UINT16 g[] = { 3, 28, 28, 4 }; float adv[4] = { 10, 20, 30, 40 }; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator1 *e = NULL; D2D1_POINT_2F o = { 100, 200 }; BOOL has; int k = 0;
        memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 20; r.glyphCount = 4; r.glyphIndices = g; r.glyphAdvances = adv;
        CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f4, o, &r, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(F_TT | F_PNG), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
        while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has && k < 5) {
            const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c);
            if (k == 0) { EXPECT_EQ(c->glyphRun.glyphCount, 4); EXPECT_EQ(c->glyphImageFormat, F_TT); EXPECT_EQ(c->glyphRun.glyphIndices[0], 1); EXPECT_EQ(c->glyphRun.glyphIndices[1], 28); EXPECT_EQ(c->glyphRun.glyphIndices[3], 1); }
            if (k == 1) { EXPECT_EQ(c->glyphRun.glyphCount, 2); EXPECT_EQ(c->glyphRun.glyphIndices[0], 3); EXPECT_EQ(c->glyphRun.glyphIndices[1], 4); CHECK_FLOAT_EQ(c->glyphRun.glyphAdvances[0], 60.0f); CHECK_FLOAT_EQ(c->glyphRun.glyphAdvances[1], 40.0f); CHECK_FLOAT_EQ(c->baselineOriginX, 100.0f); EXPECT_EQ(c->glyphImageFormat, F_PNG); EXPECT_EQ(c->paletteIndex, 0xFFFF); }
            k++;
        }
        EXPECT_EQ(k, 2); DW_RELEASE(e);
    }
    crb_close(&x);
}
T(translate_different_bitmap_formats_do_not_merge) {
    NEED_F4(); static const UINT16 s[][5] = { { 4, 5 }, { 5, 4 }, { 4, 5, 4 }, { 5, 6, 4, 7 }, { 3, 2 }, { 2, 3 } }; static const int n[] = { 2, 2, 3, 4, 2, 2 }; int i;
    for (i = 0; i < 6; i++) { char tag[32]; snprintf(tag, sizeof tag, "mixed %d", i); check(f4, &x, tag, s[i], n[i], 20.0f, 0, 0, 0, F_TT | F_PNG | F_JPEG | F_TIFF, 0, DWRITE_MEASURING_MODE_NATURAL, NULL); }
    crb_close(&x);
}
T(translate_colr_glyph_between_bitmap_glyphs_breaks_the_group) {
    NEED_F4(); static const UINT16 s[][5] = { { 3, 10, 4 }, { 3, 11, 4, 4 }, { 10, 3, 4, 11 }, { 4, 12, 4 }, { 3, 13, 3, 14 } }; static const int n[] = { 3, 4, 4, 3, 4 }; int i, p;
    for (p = 0; p < 2; p++) for (i = 0; i < 5; i++) { char tag[40]; snprintf(tag, sizeof tag, "colr gap %d pal %d", i, p); check(f4, &x, tag, s[i], n[i], 20.0f, 0, 9, 8, F_TT | F_COLR | F_PNG, (UINT32)p, DWRITE_MEASURING_MODE_NATURAL, NULL); }
    crb_close(&x);
}
T(translate_bitmap_wins_over_colr_for_the_same_glyph) {
    NEED_F4(); UINT16 g = 2; float adv = 12; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator1 *e = NULL; BOOL has; D2D1_POINT_2F o = { 0, 0 }; int k = 0;
    /* glyph 2 has a COLR layer and a PNG@16 / TIFF@32 entry */
    memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 10; r.glyphCount = 1; r.glyphIndices = &g; r.glyphAdvances = &adv;
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f4, o, &r, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(F_PNG | F_COLR), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has && k < 5) { const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c); if (k == 1) EXPECT_EQ(c->glyphImageFormat, F_PNG); k++; }
    EXPECT_EQ(k, 2); DW_RELEASE(e);
    /* ppem 20 is above the PNG strike: the TIFF@32 entry is not in the mask, so COLR is used */
    r.fontEmSize = 20; k = 0;
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f4, o, &r, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(F_PNG | F_COLR), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has && k < 5) { const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c); if (k == 1) EXPECT_EQ(c->glyphImageFormat, F_TT | F_COLR); k++; }
    EXPECT_EQ(k, 2); DW_RELEASE(e);
    crb_close(&x);
}
T(translate_bitmap_runs_report_natural_measuring_mode) {
    NEED_F4(); static const DWRITE_MEASURING_MODE mm[] = { DWRITE_MEASURING_MODE_NATURAL, DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_MEASURING_MODE_GDI_NATURAL }; int i;
    static const UINT16 g[] = { 3, 10, 4, 28 };
    for (i = 0; i < 3; i++) { char tag[32]; snprintf(tag, sizeof tag, "mm %d", i); check(f4, &x, tag, g, 4, 20.0f, 0, 2, 3, F_TT | F_COLR | F_PNG, 0, mm[i], NULL); }
    crb_close(&x);
}
T(translate_rtl_runs) {
    NEED_F4(); static const UINT16 s[][6] = { { 3, 4 }, { 3, 28, 4 }, { 10, 3, 4, 28, 11 }, { 5, 5, 5 } }; static const int n[] = { 2, 3, 5, 3 }; int i, b;
    for (b = 1; b <= 3; b += 2) for (i = 0; i < 4; i++) { char tag[32]; snprintf(tag, sizeof tag, "rtl %d level %d", i, b); check(f4, &x, tag, s[i], n[i], 20.0f, b, 300, 7, F_TT | F_COLR | F_PNG | F_JPEG, 1, DWRITE_MEASURING_MODE_NATURAL, NULL); }
    crb_close(&x);
}
T(translate_transform_scales_the_requested_ppem) {
    NEED_F4(); UINT16 g[] = { 2, 3, 4, 5, 8 };
    static const DWRITE_MATRIX m[] = { { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 2, 0, 0 }, { 3, 0, 0, 1, 0, 0 }, { 1, 0, 0, 4, 5, 6 }, { 0.5f, 0, 0, 0.5f, 0, 0 }, { -2, 0, 0, 1, 0, 0 }, { 0, 3, -3, 0, 0, 0 },
        { 1, 1, -1, 1, 0, 0 }, { 2, 1, 0, 1, 0, 0 }, { 0, 0, 0, 0, 0, 0 }, { 1, 0.5f, 0, 1, 0, 0 }, { 0.8f, 0.6f, -0.6f, 0.8f, 0, 0 } };
    unsigned i; static const float em[] = { 8.0f, 10.0f, 18.0f, 30.0f };
    for (i = 0; i < sizeof m / sizeof m[0]; i++) { unsigned j; for (j = 0; j < 4; j++) { char tag[64]; snprintf(tag, sizeof tag, "matrix %u em %g", i, em[j]); check(f4, &x, tag, g, 5, em[j], 0, 0, 0, F_TT | F_PNG | F_JPEG | F_TIFF, 0, DWRITE_MEASURING_MODE_NATURAL, &m[i]); } }
    crb_close(&x);
}
T(translate_no_color_glyph_in_run_is_nocolor) {
    NEED_F4(); static const UINT16 g[] = { 0, 1, 28, 29, 9, 4, 5 }; UINT32 fm; char tag[48];
    /* glyphs 4 and 5 have PNG/JPEG entries from ppem 32 on; at em 33 nothing is left for them */
    for (fm = 0; fm < 0x100; fm += 1) { snprintf(tag, sizeof tag, "mask 0x%x", fm); check(f4, &x, tag, g, 7, 33.0f, 0, 0, 0, fm, 0, DWRITE_MEASURING_MODE_NATURAL, NULL); }
    crb_close(&x);
}
T(translate_palette_selection_for_colr_layers) {
    NEED_F4(); static const UINT16 g[] = { 10, 11, 12, 13, 14, 28 }; unsigned p;
    for (p = 0; p < 6; p++) { char tag[32]; snprintf(tag, sizeof tag, "palette %u", p); check(f4, &x, tag, g, 6, 20.0f, 0, 4, 5, F_TT | F_COLR, p, DWRITE_MEASURING_MODE_NATURAL, NULL); }
    { unsigned bad[] = { 2, 3, 100, 65535, 0xFFFF, 0x10000, 0xFFFFFFFFu }; for (p = 0; p < 7; p++) { char tag[32]; snprintf(tag, sizeof tag, "bad palette 0x%x", bad[p]); check(f4, &x, tag, g, 6, 20.0f, 0, 4, 5, F_TT | F_COLR, bad[p], DWRITE_MEASURING_MODE_NATURAL, NULL); } }
    crb_close(&x);
}
T(translate_bad_palette_still_allows_bitmaps) {
    NEED_F4(); static const UINT16 g[] = { 3, 10, 4 };
    /* COLR is dropped for an unknown palette but PNG entries do not depend on palettes */
    check(f4, &x, "bad palette with png", g, 3, 20.0f, 0, 1, 1, F_TT | F_COLR | F_PNG, 7, DWRITE_MEASURING_MODE_NATURAL, NULL);
    check(f4, &x, "bad palette colr only", g, 3, 20.0f, 0, 1, 1, F_TT | F_COLR, 7, DWRITE_MEASURING_MODE_NATURAL, NULL);
    crb_close(&x);
}
T(translate_glyph_offsets_follow_bitmap_glyphs) {
    NEED_F4(); UINT16 g[] = { 3, 28, 4 }; float adv[3] = { 5, 6, 7 }; DWRITE_GLYPH_OFFSET off[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } }; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator1 *e = NULL; BOOL has; D2D1_POINT_2F o = { 0, 0 }; int k = 0;
    memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 20; r.glyphCount = 3; r.glyphIndices = g; r.glyphAdvances = adv; r.glyphOffsets = off;
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f4, o, &r, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(F_TT | F_PNG), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has && k < 5) {
        const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c);
        CHECKF(c->glyphRun.glyphOffsets != NULL, "run %d: offsets dropped", k);
        if (c->glyphRun.glyphOffsets && k == 0) { CHECK(c->glyphRun.glyphOffsets[0].advanceOffset == 1 && c->glyphRun.glyphOffsets[2].ascenderOffset == 6); }
        if (c->glyphRun.glyphOffsets && k == 1) { CHECKF(c->glyphRun.glyphCount == 2 && c->glyphRun.glyphOffsets[0].advanceOffset == 1 && c->glyphRun.glyphOffsets[1].advanceOffset == 5, "merged run offsets (%g %g)", c->glyphRun.glyphOffsets[0].advanceOffset, c->glyphRun.glyphOffsets[1].advanceOffset); }
        k++;
    }
    EXPECT_EQ(k, 2); DW_RELEASE(e);
    crb_close(&x);
}
T(translate_long_runs_with_many_bitmap_glyphs) {
    NEED_F4(); static UINT16 g[64]; int i, len;
    for (len = 1; len <= 64; len += 7) {
        for (i = 0; i < len; i++) g[i] = (UINT16)((i * 5 + 3) % 12 + 2);
        { char tag[32]; snprintf(tag, sizeof tag, "len %d", len); check(f4, &x, tag, g, len, 20.0f, 0, 1, 1, F_TT | F_COLR | F_PNG | F_JPEG | F_TIFF, 0, DWRITE_MEASURING_MODE_NATURAL, NULL); }
    }
    crb_close(&x);
}
T(translate_enumerator_pointers_are_stable_per_run) {
    NEED_F4(); UINT16 g[] = { 3, 10 }; float adv[2] = { 5, 6 }; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator1 *e = NULL; BOOL has; D2D1_POINT_2F o = { 0, 0 };
    memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 20; r.glyphCount = 2; r.glyphIndices = g; r.glyphAdvances = adv;
    CHECK_HR_OK(IDWriteFactory4_TranslateColorGlyphRun(f4, o, &r, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(F_TT | F_COLR | F_PNG), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        const DWRITE_COLOR_GLYPH_RUN1 *a = NULL, *b = NULL; const DWRITE_COLOR_GLYPH_RUN1 *c0 = NULL;
        CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)); CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &a)); CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c0));
        CHECK(a == c0);
        CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)); CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &b));
        /* one record inside the enumerator is rewritten per run */
        CHECK(a == b);
    }
    DW_RELEASE(e); crb_close(&x);
}
