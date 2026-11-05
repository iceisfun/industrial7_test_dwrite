#include "ta.h"

int ta_open(ta_ctx *c) {
    memset(c, 0, sizeof *c);
    c->f = dw_shared();
    if (!c->f) return 0;
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(c->f, &c->a)) || !c->a) return 0;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(c->a, &IID_IDWriteTextAnalyzer1, (void **)&c->a1))) c->a1 = NULL;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(c->a, &IID_IDWriteTextAnalyzer2, (void **)&c->a2))) c->a2 = NULL;
    return c->a1 && c->a2;
}
void ta_close(ta_ctx *c) {
    DW_RELEASE(c->a2); DW_RELEASE(c->a1); DW_RELEASE(c->a);
}

DWRITE_SCRIPT_ANALYSIS ta_sa_of(ta_ctx *c, const WCHAR *text, UINT32 len) {
    DWRITE_SCRIPT_ANALYSIS sa; memset(&sa, 0, sizeof sa); sa.script = 0xFFFF;
    dwx_ana_src *src = dwx_ana_src_new(text, len); dwx_ana_sink *sk = dwx_ana_sink_new();
    if (SUCCEEDED(IDWriteTextAnalyzer_AnalyzeScript(c->a, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sk)) && sk->script_set[0])
        sa = sk->script[0];
    dwx_release(src); dwx_release(sk);
    return sa;
}
UINT16 ta_script_of(ta_ctx *c, const WCHAR *text, UINT32 len) { return ta_sa_of(c, text, len).script; }

HRESULT ta_open_spec(dwx_font *h, IDWriteFactory *f, const ttf_spec *s) {
    size_t n = 0; uint8_t *b = ttf_build(s, &n);
    if (!b) return E_FAIL;
    HRESULT hr = dwx_font_open(h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(b);
    return hr;
}

#include "ta_shape.h"
int ta_shape(ta_shaped *s, ta_ctx *c, IDWriteFontFace *face, const WCHAR *text, UINT32 n, int rtl, float em) {
    memset(s, 0, sizeof *s);
    if (n == 0 || n > TA_SHAPE_MAX) return 0;
    s->sa = ta_sa_of(c, text, n); s->ntext = n;
    if (s->sa.script == 0xFFFF) return 0;
    if (FAILED(IDWriteTextAnalyzer_GetGlyphs(c->a, text, n, face, FALSE, rtl, &s->sa, L"en-us", NULL, NULL, NULL, 0, TA_SHAPE_MAX * 2, s->cmap, s->tprops,
                                             s->glyphs, s->gprops, &s->nglyphs))) return 0;
    if (FAILED(IDWriteTextAnalyzer_GetGlyphPlacements(c->a, text, s->cmap, s->tprops, n, s->glyphs, s->gprops, s->nglyphs, face, em, FALSE, rtl, &s->sa,
                                                      L"en-us", NULL, NULL, 0, s->adv, s->off))) return 0;
    return 1;
}
