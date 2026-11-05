#include "r2.h"

int r2_open(r2_ctx *c) {
    memset(c, 0, sizeof *c);
    c->f = dw_shared();
    if (!c->f) return 0;
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(c->f, &c->a)) || !c->a) return 0;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(c->a, &IID_IDWriteTextAnalyzer1, (void **)&c->a1))) c->a1 = NULL;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(c->a, &IID_IDWriteTextAnalyzer2, (void **)&c->a2))) c->a2 = NULL;
    return c->a1 && c->a2;
}
void r2_close(r2_ctx *c) { DW_RELEASE(c->a2); DW_RELEASE(c->a1); DW_RELEASE(c->a); }

DWRITE_SCRIPT_ANALYSIS r2_sa_of(r2_ctx *c, const WCHAR *text, UINT32 len) {
    DWRITE_SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa); sa.script = 0xFFFF;
    dwx_ana_src *src = dwx_ana_src_new(text, len); dwx_ana_sink *sk = dwx_ana_sink_new();
    if (SUCCEEDED(IDWriteTextAnalyzer_AnalyzeScript(c->a, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sk)) && sk->script_set[0])
        sa = sk->script[0];
    dwx_release(src); dwx_release(sk);
    dwx_free(src); dwx_free(sk);                   /* the doubles keep their (large) memory until freed */
    return sa;
}

static ttf_glyph g_r2_glyphs[1024];
HRESULT r2_font_open(r2_font *x, IDWriteFactory *f, int nglyphs, const otl_table *gsub, const otl_table *gpos) {
    ttf_spec s; ttf_rawtable raw[2]; int nraw = 0, i, nd = 0;
    const ttf_glyph *d = ttf_default_glyphs(&nd);
    uint8_t *blob; size_t n; HRESULT hr;
    memset(x, 0, sizeof *x);
    ttf_default_spec(&s);
    if (nglyphs > nd) {
        if (nglyphs > 1024) nglyphs = 1024;
        memcpy(g_r2_glyphs, d, nd * sizeof *d);
        for (i = nd; i < nglyphs; i++) { memset(&g_r2_glyphs[i], 0, sizeof g_r2_glyphs[i]); g_r2_glyphs[i].adv = TTF_DEF_ADV; }
        s.glyphs = g_r2_glyphs; s.nglyphs = nglyphs;
    }
    if (gsub) { x->gsub = otl_build(gsub, 0, &x->ngsub); raw[nraw].data = x->gsub; raw[nraw].len = (unsigned)x->ngsub; memcpy(raw[nraw].tag, "GSUB", 5); nraw++; }
    if (gpos) { x->gpos = otl_build(gpos, 1, &x->ngpos); raw[nraw].data = x->gpos; raw[nraw].len = (unsigned)x->ngpos; memcpy(raw[nraw].tag, "GPOS", 5); nraw++; }
    s.raw = raw; s.nraw = nraw;
    blob = ttf_build(&s, &n);
    if (!blob) return E_FAIL;
    hr = dwx_font_open(&x->h, f, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(blob);
    return hr;
}
void r2_font_close(r2_font *x) { dwx_font_close(&x->h); free(x->gsub); free(x->gpos); x->gsub = x->gpos = NULL; }

HRESULT r2_features(r2_ctx *c, IDWriteFontFace *face, DWRITE_SCRIPT_ANALYSIS sa, const WCHAR *loc, UINT32 cap, UINT32 *n, DWRITE_FONT_FEATURE_TAG *t) {
    return IDWriteTextAnalyzer2_GetTypographicFeatures(c->a2, face, sa, loc, cap, n, t);
}
int r2_covered(r2_ctx *c, IDWriteFontFace *face, DWRITE_SCRIPT_ANALYSIS sa, const WCHAR *loc, UINT32 tag, UINT16 gid) {
    UINT8 out = 0xEE;
    if (FAILED(IDWriteTextAnalyzer2_CheckTypographicFeature(c->a2, face, sa, loc, (DWRITE_FONT_FEATURE_TAG)tag, 1, &gid, &out))) return -1;
    return out;
}
UINT32 r2_ot_lang(const WCHAR *loc) {
    WCHAR b[16] = { 0 };
    if (!loc || !*loc) return R2_TAG('d', 'f', 'l', 't');
    /* LOCALE_SOPENTYPELANGUAGETAG (0x7a) is what the OS lookup of the language tag uses */
    if (GetLocaleInfoEx(loc, 0x7a, b, 16) != 5) return R2_TAG('d', 'f', 'l', 't');
    return R2_TAG(b[0], b[1], b[2], b[3]);
}
