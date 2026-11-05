#include "ms.h"

/* ---- source -------------------------------------------------------------------------------------------------------------------- */
static UINT32 next_cut(const ms_src *s, UINT32 pos)         /* smallest piece boundary strictly greater than pos */
{
    UINT32 b = 0; int i = 0;
    if (!s->ncuts) return s->len;
    while (b <= pos) { b += s->cuts[i % s->ncuts]; i++; }
    return b < s->len ? b : s->len;
}
static UINT32 prev_cut(const ms_src *s, UINT32 pos)         /* largest piece boundary strictly smaller than pos (0 when none) */
{
    UINT32 b = 0, last = 0; int i = 0;
    if (!s->ncuts) return 0;
    while (b < pos) { last = b; b += s->cuts[i % s->ncuts]; i++; }
    return last;
}
static void logc(ms_src *s, int fn, UINT32 pos, UINT32 ret, const WCHAR *p)
{
    s->n[fn]++;
    if (s->nlog < MS_EVMAX) { s->log[s->nlog].fn = fn; s->log[s->nlog].pos = pos; s->log[s->nlog].ret = ret; s->log[s->nlog].ptr = p; s->nlog++; }
}
static HRESULT WINAPI ss_QI(IDWriteTextAnalysisSource1 *This, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteTextAnalysisSource) || IsEqualIID(r, &IID_IDWriteTextAnalysisSource1)) { *o = This; ((ms_src *)This)->ref++; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ss_AR(IDWriteTextAnalysisSource1 *This) { return (ULONG)++((ms_src *)This)->ref; }
static ULONG WINAPI ss_RL(IDWriteTextAnalysisSource1 *This) { return (ULONG)--((ms_src *)This)->ref; }
static HRESULT WINAPI ss_At(IDWriteTextAnalysisSource1 *This, UINT32 pos, const WCHAR **t, UINT32 *n)
{
    ms_src *s = (ms_src *)This; UINT32 end;
    if (pos >= s->len || (s->null_after && pos > 0 && pos >= s->null_after)) { *t = NULL; *n = 0; logc(s, MF_AT, pos, 0, NULL); return S_OK; }
    end = next_cut(s, pos);
    *t = s->text + pos; *n = end - pos; logc(s, MF_AT, pos, *n, *t); return S_OK;
}
static HRESULT WINAPI ss_Before(IDWriteTextAnalysisSource1 *This, UINT32 pos, const WCHAR **t, UINT32 *n)
{
    ms_src *s = (ms_src *)This; UINT32 st;
    if (pos == 0 || pos > s->len || s->null_before) { *t = NULL; *n = 0; logc(s, MF_BEFORE, pos, 0, NULL); return S_OK; }
    st = prev_cut(s, pos);
    *t = s->text + st; *n = pos - st; logc(s, MF_BEFORE, pos, *n, *t); return S_OK;
}
static DWRITE_READING_DIRECTION WINAPI ss_Dir(IDWriteTextAnalysisSource1 *This) { ms_src *s = (ms_src *)This; logc(s, MF_DIR, 0, 0, NULL); return s->dir; }
static HRESULT WINAPI ss_Loc(IDWriteTextAnalysisSource1 *This, UINT32 pos, UINT32 *n, const WCHAR **l)
{
    ms_src *s = (ms_src *)This; UINT32 rest = pos < s->len ? s->len - pos : 0;
    *l = s->locale; *n = (s->locale_run && s->locale_run < rest) ? s->locale_run : (s->locale_run == 0xffffffffu ? 0 : rest);
    logc(s, MF_LOCALE, pos, *n, NULL); return S_OK;
}
static HRESULT WINAPI ss_Sub(IDWriteTextAnalysisSource1 *This, UINT32 pos, UINT32 *n, IDWriteNumberSubstitution **ns)
{
    ms_src *s = (ms_src *)This; UINT32 rest = pos < s->len ? s->len - pos : 0;
    *n = (s->subst_run && s->subst_run < rest) ? s->subst_run : rest;
    *ns = s->subst; if (*ns) IDWriteNumberSubstitution_AddRef(*ns);
    logc(s, MF_SUBST, pos, *n, NULL); return S_OK;
}
static HRESULT WINAPI ss_Orient(IDWriteTextAnalysisSource1 *This, UINT32 pos, UINT32 *n, DWRITE_VERTICAL_GLYPH_ORIENTATION *o, UINT8 *bidi)
{
    ms_src *s = (ms_src *)This; UINT32 b = 0; int i = 0;
    if (s->fail_orient) { *n = 0; *o = DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT; *bidi = 0; logc(s, MF_ORIENT, pos, 0, NULL); return s->fail_orient; }
    if (!s->nor) { *n = pos < s->len ? s->len - pos : 0; *o = DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT; *bidi = 0; logc(s, MF_ORIENT, pos, *n, NULL); return S_OK; }
    for (i = 0; i < s->nor - 1 && pos >= b + s->or_len[i]; i++) b += s->or_len[i];
    *n = (i == s->nor - 1 && pos >= b + s->or_len[i]) ? (pos < s->len ? s->len - pos : 0) : b + s->or_len[i] - pos;
    *o = s->or_o[i]; *bidi = s->or_bidi[i]; logc(s, MF_ORIENT, pos, *n, NULL); return S_OK;
}
static const IDWriteTextAnalysisSource1Vtbl ss_vt = { ss_QI, ss_AR, ss_RL, ss_At, ss_Before, ss_Dir, ss_Loc, ss_Sub, ss_Orient };
ms_src *ms_src_new(const WCHAR *text, UINT32 len)
{
    ms_src *s = (ms_src *)calloc(1, sizeof *s);
    s->lpVtbl = &ss_vt; s->ref = 1; s->text = text; s->len = len; s->dir = LTR; s->locale = L"en-us";
    return s;
}
void ms_src_cuts(ms_src *s, const UINT32 *cuts, int n) { int i; s->ncuts = 0; for (i = 0; i < n && i < 16; i++) if (cuts[i]) s->cuts[s->ncuts++] = cuts[i]; }
void ms_src_free(ms_src *s) { if (!s) return; CHECKF(s->ref == 1, "source reference count %ld after the call", (long)s->ref); free(s); }
int ms_log_count(const ms_src *s, int fn, UINT32 pos)
{
    int i, c = 0;
    for (i = 0; i < s->nlog; i++) if (s->log[i].fn == fn && (pos == 0xffffffffu || s->log[i].pos == pos)) c++;
    return c;
}

/* ---- sink ---------------------------------------------------------------------------------------------------------------------- */
static HRESULT WINAPI sk_QI(IDWriteTextAnalysisSink1 *This, REFIID r, void **o)
{
    ms_sink *s = (ms_sink *)This;
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteTextAnalysisSink) || (s->support_v1 && IsEqualIID(r, &IID_IDWriteTextAnalysisSink1))) { *o = This; s->ref++; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sk_AR(IDWriteTextAnalysisSink1 *This) { return (ULONG)++((ms_sink *)This)->ref; }
static ULONG WINAPI sk_RL(IDWriteTextAnalysisSink1 *This) { return (ULONG)--((ms_sink *)This)->ref; }
static ms_ev *sk_begin(ms_sink *s, int kind, UINT32 pos, UINT32 len, HRESULT *fail)
{
    UINT32 i; ms_ev *e = NULL;
    s->n[kind]++; *fail = S_OK;
    if (s->fail_kind == kind && s->n[kind] >= s->fail_after) { *fail = s->fail_hr; return NULL; }
    if (s->nev < MS_EVMAX) { e = &s->ev[s->nev]; memset(e, 0, sizeof *e); e->fam = kind; e->pos = pos; e->len = len; s->nev++; }
    for (i = pos; i < pos + len; i++) { if (i >= MS_MAX) { s->overflow = 1; break; } if (s->hits[kind][i] < 255) s->hits[kind][i]++; }
    return e;
}
static HRESULT WINAPI sk_Script(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, const DWRITE_SCRIPT_ANALYSIS *a)
{
    ms_sink *s = (ms_sink *)This; HRESULT f; ms_ev *e = sk_begin(s, MK_SCRIPT, pos, len, &f); UINT32 i;
    if (f) return f;
    CHECK(a != NULL); if (!a) return S_OK;
    if (e) e->sa = *a;
    for (i = pos; i < pos + len && i < MS_MAX; i++) s->script[i] = *a;
    return S_OK;
}
static HRESULT WINAPI sk_Break(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, const DWRITE_LINE_BREAKPOINT *b)
{
    ms_sink *s = (ms_sink *)This; HRESULT f; UINT32 i; sk_begin(s, MK_BREAK, pos, len, &f);
    if (f) return f;
    CHECK(b != NULL); if (!b) return S_OK;
    for (i = 0; i < len && pos + i < MS_MAX; i++) s->brk[pos + i] = b[i];
    return S_OK;
}
static HRESULT WINAPI sk_Bidi(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, UINT8 ex, UINT8 res)
{
    ms_sink *s = (ms_sink *)This; HRESULT f; ms_ev *e = sk_begin(s, MK_BIDI, pos, len, &f); UINT32 i;
    if (f) return f;
    if (e) { e->ex = ex; e->res = res; }
    for (i = pos; i < pos + len && i < MS_MAX; i++) { s->ex[i] = ex; s->res[i] = res; }
    return S_OK;
}
static HRESULT WINAPI sk_Subst(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, IDWriteNumberSubstitution *ns)
{
    ms_sink *s = (ms_sink *)This; HRESULT f; ms_ev *e = sk_begin(s, MK_SUBST, pos, len, &f);
    if (f) return f;
    if (e) e->ns = ns;
    return S_OK;
}
static HRESULT WINAPI sk_Orient(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, DWRITE_GLYPH_ORIENTATION_ANGLE a, UINT8 abl, BOOL sw, BOOL rtl)
{
    ms_sink *s = (ms_sink *)This; HRESULT f; ms_ev *e = sk_begin(s, MK_ORIENT, pos, len, &f); UINT32 i;
    if (f) return f;
    if (e) { e->ang = a; e->abl = abl; e->sw = sw; e->rtl = rtl; }
    for (i = pos; i < pos + len && i < MS_MAX; i++) { s->ang[i] = a; s->abl[i] = abl; s->sw[i] = sw; s->rtl[i] = rtl; }
    return S_OK;
}
static const IDWriteTextAnalysisSink1Vtbl sk_vt = { sk_QI, sk_AR, sk_RL, sk_Script, sk_Break, sk_Bidi, sk_Subst, sk_Orient };
ms_sink *ms_sink_new(int v1) { ms_sink *s = (ms_sink *)calloc(1, sizeof *s); s->lpVtbl = &sk_vt; s->ref = 1; s->support_v1 = v1; return s; }
void ms_sink_free(ms_sink *k) { if (!k) return; CHECKF(k->ref == 1, "sink reference count %ld after the call", (long)k->ref); CHECK(!k->overflow); free(k); }
int ms_sink_exact(const ms_sink *k, int kind, UINT32 pos, UINT32 len)
{
    UINT32 i;
    for (i = 0; i < MS_MAX; i++) { int want = (i >= pos && i < pos + len) ? 1 : 0; if (k->hits[kind][i] != want) return 0; }
    return 1;
}
int ms_runs_exact(const ms_sink *k, int kind, UINT32 pos, UINT32 len)
{
    int i; UINT32 at = pos;
    for (i = 0; i < k->nev; i++) {
        if (k->ev[i].fam != kind) continue;
        if (k->ev[i].pos != at || k->ev[i].len == 0) return 0;
        at += k->ev[i].len;
    }
    return at == pos + len && k->nev < MS_EVMAX;
}

/* ---- analyzer ------------------------------------------------------------------------------------------------------------------ */
int ms_open(ms_ctx *c, int need_v2)
{
    memset(c, 0, sizeof *c);
    c->f = dw_shared();
    if (!c->f) return 0;
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(c->f, &c->a)) || !c->a) return 0;
    if (need_v2) {
        ms_src *s; ms_sink *k; HRESULT hr;
        if (FAILED(IDWriteTextAnalyzer_QueryInterface(c->a, &IID_IDWriteTextAnalyzer2, (void **)&c->a2)) || !c->a2) return 0;
        /* an analyzer that reports E_NOTIMPL does not provide the orientation analysis at all: prerequisite missing */
        s = ms_src_new(L"a", 1); k = ms_sink_new(1);
        hr = ms_orient(c->a2, s, 0, 1, k);
        free(s); free(k);
        if (hr == E_NOTIMPL) return 0;
    }
    return 1;
}
void ms_close(ms_ctx *c) { if (c->a2) IDWriteTextAnalyzer2_Release(c->a2); c->a2 = NULL; if (c->a) IDWriteTextAnalyzer_Release(c->a); c->a = NULL; }
HRESULT ms_do(IDWriteTextAnalyzer *an, int kind, ms_src *s, UINT32 pos, UINT32 len, ms_sink *k)
{
    IDWriteTextAnalysisSource *src = (IDWriteTextAnalysisSource *)s; IDWriteTextAnalysisSink *snk = (IDWriteTextAnalysisSink *)k;
    switch (kind) {
    case MA_SCRIPT: return IDWriteTextAnalyzer_AnalyzeScript(an, src, pos, len, snk);
    case MA_BIDI:   return IDWriteTextAnalyzer_AnalyzeBidi(an, src, pos, len, snk);
    case MA_NUMSUB: return IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, src, pos, len, snk);
    default:        return IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, src, pos, len, snk);
    }
}
HRESULT ms_orient(IDWriteTextAnalyzer2 *a2, ms_src *s, UINT32 pos, UINT32 len, ms_sink *k)
{
    return IDWriteTextAnalyzer2_AnalyzeVerticalGlyphOrientation(a2, (IDWriteTextAnalysisSource1 *)s, pos, len, (IDWriteTextAnalysisSink1 *)k);
}

const UINT32 ms_patterns[][6] = {
    { 0 }, { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 7 }, { 1, 2, 3 }, { 2, 1, 3 }, { 3, 1 }, { 1, 1, 2 }, { 6, 1 }, { 2, 3, 5 },
};
const int ms_npatterns = (int)(sizeof ms_patterns / sizeof ms_patterns[0]);
