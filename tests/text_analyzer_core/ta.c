#include "ta.h"

HRESULT ta_do(IDWriteTextAnalyzer *an, int kind, dwx_ana_src *s, UINT32 pos, UINT32 len, dwx_ana_sink *k)
{
    IDWriteTextAnalysisSource *src = (IDWriteTextAnalysisSource *)s;
    IDWriteTextAnalysisSink *snk = (IDWriteTextAnalysisSink *)k;
    switch (kind) {
    case TA_SCRIPT: return IDWriteTextAnalyzer_AnalyzeScript(an, src, pos, len, snk);
    case TA_BIDI:   return IDWriteTextAnalyzer_AnalyzeBidi(an, src, pos, len, snk);
    case TA_NUMSUB: return IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, src, pos, len, snk);
    default:        return IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, src, pos, len, snk);
    }
}

HRESULT ta_run_n(IDWriteTextAnalyzer *an, int kind, const WCHAR *text, UINT32 n, UINT32 chunk, dwx_ana_src **ps, dwx_ana_sink **pk)
{
    *ps = dwx_ana_src_new(text, n); (*ps)->chunk = chunk;
    *pk = dwx_ana_sink_new();
    return ta_do(an, kind, *ps, 0, n, *pk);
}
HRESULT ta_run(IDWriteTextAnalyzer *an, int kind, const WCHAR *text, dwx_ana_src **ps, dwx_ana_sink **pk)
{
    return ta_run_n(an, kind, text, (UINT32)wcslen(text), 0, ps, pk);
}
void ta_free(dwx_ana_src *s, dwx_ana_sink *k) { if (s) dwx_release(s); if (k) dwx_release(k); }

UINT32 ta_iso(IDWriteTextAnalyzer *an, DWRITE_SCRIPT_ANALYSIS sa)
{
    IDWriteTextAnalyzer1 *a1 = NULL; DWRITE_SCRIPT_PROPERTIES p; UINT32 r = 0;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer1, (void **)&a1)) || !a1) return 0;
    memset(&p, 0, sizeof p);
    if (SUCCEEDED(IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, &p))) r = p.isoScriptNumber;
    IDWriteTextAnalyzer1_Release(a1);
    return r;
}

int ta_runs_exact(const dwx_ana_sink *k, char fam, UINT32 pos, UINT32 len)
{
    const void *runs; int n, i; UINT32 at = pos;
    typedef struct { UINT32 pos, len; } run_t;
    const run_t *r;
    if (fam == 's') { runs = k->script_runs; n = k->nrun_script; }
    else if (fam == 'b') { runs = k->break_runs; n = k->nrun_break; }
    else { runs = k->bidi_runs; n = k->nrun_bidi; }
    if (n > 256) return 0;
    r = (const run_t *)runs;
    for (i = 0; i < n; i++) { if (r[i].pos != at || r[i].len == 0) return 0; at += r[i].len; }
    return at == pos + len;
}

/* ---- synthetic fonts ---------------------------------------------------------------------------------------------------------- */
static const ttf_sub4 g_lig[] = { { { 33, 36 }, 2, 65 } };
static const ttf_feature g_gsub[] = { { "liga", TTF_LK_LIGATURE, 1, g_lig } };
static const ttf_kernpair g_kp[] = { { 2, 23, -100 }, { 23, 2, -100 }, { 21, 42, -150 }, { 2, 26, -80 } };

int ta_font_open(ta_font *t, IDWriteFactory *f, int shaping)
{
    ttf_spec s; size_t n = 0; uint8_t *b; HRESULT hr;
    memset(t, 0, sizeof *t);
    ttf_default_spec(&s);
    ttf_set_names(&s, "DwShape", "Regular", "DwShape", "DwShape-Regular");
    if (shaping) { s.gsub = g_gsub; s.ngsub = 1; s.gpos_kern = g_kp; s.ngpos_kern = 4; }
    b = ttf_build(&s, &n);
    if (!b) return 0;
    hr = dwx_font_open(&t->h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(b);
    if (hr != S_OK || !t->h.face) return 0;
    t->face = t->h.face; t->ok = 1;
    return 1;
}
void ta_font_close(ta_font *t) { dwx_font_close(&t->h); t->face = NULL; }

DWRITE_SCRIPT_ANALYSIS ta_script_of(IDWriteTextAnalyzer *an, const WCHAR *text, UINT32 len)
{
    DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT };
    dwx_ana_src *s = dwx_ana_src_new(text, len); dwx_ana_sink *k = dwx_ana_sink_new();
    if (SUCCEEDED(ta_do(an, TA_SCRIPT, s, 0, len, k)) && k->script_set[0]) sa = k->script[0];
    ta_free(s, k);
    return sa;
}

HRESULT ta_shape(IDWriteTextAnalyzer *an, IDWriteFontFace *face, const WCHAR *text, UINT32 len, BOOL rtl, BOOL sideways,
                 const WCHAR *locale, const DWRITE_TYPOGRAPHIC_FEATURES **feat, const UINT32 *flen, UINT32 nfeat,
                 UINT32 maxg, ta_shaped *o)
{
    memset(o, 0, sizeof *o);
    o->len = len; o->loc = locale; o->sa = ta_script_of(an, text, len);
    o->actual = 0xdeadbeef;
    if (maxg > TA_MAXG) maxg = TA_MAXG;
    o->nglyph = maxg;
    o->hr = IDWriteTextAnalyzer_GetGlyphs(an, text, len, face, sideways, rtl, &o->sa, locale, NULL, feat, flen, nfeat, maxg,
                                          o->cluster, o->tp, o->gid, o->gp, &o->actual);
    return o->hr;
}

HRESULT ta_place(IDWriteTextAnalyzer *an, IDWriteFontFace *face, const WCHAR *text, BOOL rtl, BOOL sideways, float em,
                 const DWRITE_TYPOGRAPHIC_FEATURES **feat, const UINT32 *flen, UINT32 nfeat, ta_shaped *s)
{
    s->hr_place = IDWriteTextAnalyzer_GetGlyphPlacements(an, text, s->cluster, s->tp, s->len, s->gid, s->gp, s->actual, face, em,
                                                         sideways, rtl, &s->sa, s->loc, feat, flen, nfeat, s->adv, s->off);
    return s->hr_place;
}

/* ---- scripted source ------------------------------------------------------------------------------------------------------------ */
static HRESULT WINAPI fs_QI(IDWriteTextAnalysisSource *This, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteTextAnalysisSource)) { *o = This; ((ta_fsrc *)This)->ref++; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI fs_AR(IDWriteTextAnalysisSource *This) { return (ULONG)++((ta_fsrc *)This)->ref; }
static ULONG WINAPI fs_RL(IDWriteTextAnalysisSource *This) { return (ULONG)--((ta_fsrc *)This)->ref; }
static HRESULT WINAPI fs_At(IDWriteTextAnalysisSource *This, UINT32 pos, const WCHAR **t, UINT32 *n)
{
    ta_fsrc *s = (ta_fsrc *)This; LONG k = s->n_at++;
    if (s->fail_at && k >= s->ok_at) { *t = NULL; *n = 0; return s->fail_at; }
    if (pos >= s->len) { *t = NULL; *n = 0; return S_OK; }
    *t = s->text + pos; *n = s->len - pos; return S_OK;
}
static HRESULT WINAPI fs_Before(IDWriteTextAnalysisSource *This, UINT32 pos, const WCHAR **t, UINT32 *n)
{
    ta_fsrc *s = (ta_fsrc *)This; LONG k = s->n_before++;
    if (s->fail_before && k >= s->ok_before) { *t = NULL; *n = 0; return s->fail_before; }
    if (pos == 0 || pos > s->len) { *t = NULL; *n = 0; return S_OK; }
    *t = s->text; *n = pos; return S_OK;
}
static DWRITE_READING_DIRECTION WINAPI fs_Dir(IDWriteTextAnalysisSource *This) { ta_fsrc *s = (ta_fsrc *)This; s->n_dir++; return s->dir; }
static HRESULT WINAPI fs_Loc(IDWriteTextAnalysisSource *This, UINT32 pos, UINT32 *n, const WCHAR **l)
{
    ta_fsrc *s = (ta_fsrc *)This; s->n_locale++; *l = L"en-us"; *n = pos < s->len ? s->len - pos : 0; return S_OK;
}
static HRESULT WINAPI fs_Sub(IDWriteTextAnalysisSource *This, UINT32 pos, UINT32 *n, IDWriteNumberSubstitution **ns)
{
    ta_fsrc *s = (ta_fsrc *)This; s->n_subst++;
    *n = pos < s->len ? s->len - pos : 0; *ns = NULL;
    if (s->fail_subst) return s->fail_subst;
    *ns = s->subst; if (*ns) IDWriteNumberSubstitution_AddRef(*ns);
    return S_OK;
}
static const IDWriteTextAnalysisSourceVtbl fs_vt = { fs_QI, fs_AR, fs_RL, fs_At, fs_Before, fs_Dir, fs_Loc, fs_Sub };
void ta_fsrc_init(ta_fsrc *s, const WCHAR *text, UINT32 len)
{
    memset(s, 0, sizeof *s); s->lpVtbl = &fs_vt; s->ref = 1; s->text = text; s->len = len; s->dir = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
}
