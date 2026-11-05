/* Text analysis source / sink callbacks driven by IDWriteTextAnalyzer (IDWriteTextAnalysisSource: GetTextBeforePosition,
 * GetParagraphReadingDirection, GetNumberSubstitution; IDWriteTextAnalysisSink: SetScriptAnalysis, SetLineBreakpoints,
 * SetBidiLevel, SetNumberSubstitution; the *1 variants GetVerticalGlyphOrientation / SetGlyphOrientation).
 *
 * Values are the ones measured on Win10 22H2 (standard and elevated user alike).  Each group ends with a control that must
 * differ from the pinned behaviour, so the pins cannot pass vacuously.
 *   which source callbacks each Analyze* entry point makes (and with which position):
 *     AnalyzeScript:             GetTextAtPosition                                        -> SetScriptAnalysis
 *     AnalyzeBidi:               GetParagraphReadingDirection, GetTextAtPosition, GetLocaleName -> SetBidiLevel
 *     AnalyzeNumberSubstitution: GetParagraphReadingDirection, GetTextAtPosition, GetNumberSubstitution -> SetNumberSubstitution
 *     AnalyzeLineBreakpoints:    (GetTextBeforePosition only for a start > 0), GetTextAtPosition -> one SetLineBreakpoints call
 *     AnalyzeVerticalGlyphOrientation: GetParagraphReadingDirection, GetTextAtPosition, GetVerticalGlyphOrientation -> SetGlyphOrientation */
#include "dwx.h"
#include <wchar.h>

enum { K_SCRIPT, K_BIDI, K_NUMSUB, K_BREAK, K_ORIENT };
enum { FN_AT = 1, FN_BEFORE, FN_DIR, FN_LOCALE, FN_SUBST, FN_ORIENT };

typedef struct fx { IDWriteFactory *f; IDWriteTextAnalyzer *an; IDWriteTextAnalyzer1 *a1; } fx;
static int fx_open(fx *x)
{
    memset(x, 0, sizeof *x);
    x->f = dw_shared();
    if (!x->f || FAILED(IDWriteFactory_CreateTextAnalyzer(x->f, &x->an)) || !x->an) { t_skip("CreateTextAnalyzer unavailable"); return 0; }
    IDWriteTextAnalyzer_QueryInterface(x->an, &IID_IDWriteTextAnalyzer1, (void **)&x->a1);
    return 1;
}
static void fx_close(fx *x) { DW_RELEASE(x->an); DW_RELEASE(x->a1); }
#define FX_BEGIN() fx X; if (!fx_open(&X)) return

static HRESULT run(fx *x, int kind, dwx_ana_src *s, UINT32 pos, UINT32 len, dwx_ana_sink *k)
{
    IDWriteTextAnalysisSource *src = (IDWriteTextAnalysisSource *)s; IDWriteTextAnalysisSink *snk = (IDWriteTextAnalysisSink *)k;
    switch (kind) {
    case K_SCRIPT: return IDWriteTextAnalyzer_AnalyzeScript(x->an, src, pos, len, snk);
    case K_BIDI:   return IDWriteTextAnalyzer_AnalyzeBidi(x->an, src, pos, len, snk);
    case K_NUMSUB: return IDWriteTextAnalyzer_AnalyzeNumberSubstitution(x->an, src, pos, len, snk);
    case K_BREAK:  return IDWriteTextAnalyzer_AnalyzeLineBreakpoints(x->an, src, pos, len, snk);
    default:       return x->a1 ? IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(x->a1, (IDWriteTextAnalysisSource1 *)s, pos, len, (IDWriteTextAnalysisSink1 *)k) : E_NOINTERFACE;
    }
}
/* the call log of the source as a compact string, e.g. "3@0 1@0 4@0" */
static const char *logstr(const dwx_ana_src *s)
{
    static char buf[256]; int i, o = 0;
    buf[0] = 0;
    for (i = 0; i < s->log_n && o < 230; i++) o += snprintf(buf + o, sizeof buf - o, "%s%d@%u", i ? " " : "", s->log[i].fn, s->log[i].pos);
    return buf;
}

/* ---- which source callbacks each entry point makes -------------------------------------------------------------------------- */
static const struct { int kind; const char *name; const char *full, *sub; long script, brk, bidi, nsub, orient; } MATRIX[] = {
    /* full range [0,7) of "abc def" and sub-range [2,5) */
    { K_SCRIPT, "script", "1@0",         "1@2",              1, 0, 0, 0, 0 },
    { K_BIDI,   "bidi",   "3@0 1@0 4@0", "3@0 1@2 4@2",      0, 0, 1, 0, 0 },
    { K_NUMSUB, "numsub", "3@0 1@0 5@0", "3@0 1@2 5@2",      0, 0, 0, 0, 0 },
    { K_BREAK,  "break",  "1@0 1@7",     "2@2 1@2 1@5",      0, 1, 0, 0, 0 },
    { K_ORIENT, "orient", "3@0 1@0 6@0", "3@0 1@2 6@2",      0, 0, 0, 0, 1 },
};
TEST(eum_source_callback_sequence_per_entry_point)
{
    FX_BEGIN(); int i;
    for (i = 0; i < 5; i++) {
        static const WCHAR t[] = L"abc def";
        dwx_ana_src *s = dwx_ana_src_new(t, 7); dwx_ana_sink *k = dwx_ana_sink_new();
        s->support_v1 = k->support_v1 = 1;
        if (MATRIX[i].kind == K_ORIENT && !X.a1) { dwx_release(s); dwx_release(k); continue; }
        CHECK_HR_OK(run(&X, MATRIX[i].kind, s, 0, 7, k));
        CHECKF(!strcmp(logstr(s), MATRIX[i].full), "%s full: log \"%s\" want \"%s\"", MATRIX[i].name, logstr(s), MATRIX[i].full);
        EXPECT_EQ(k->n_script, MATRIX[i].script); EXPECT_EQ(k->n_break, MATRIX[i].brk); EXPECT_EQ(k->n_bidi, MATRIX[i].bidi);
        EXPECT_EQ(k->n_subst, MATRIX[i].nsub); EXPECT_EQ(k->n_orient, MATRIX[i].orient);
        dwx_release(s); dwx_release(k);
        s = dwx_ana_src_new(t, 7); k = dwx_ana_sink_new(); s->support_v1 = k->support_v1 = 1;
        CHECK_HR_OK(run(&X, MATRIX[i].kind, s, 2, 3, k));
        CHECKF(!strcmp(logstr(s), MATRIX[i].sub), "%s sub: log \"%s\" want \"%s\"", MATRIX[i].name, logstr(s), MATRIX[i].sub);
        dwx_release(s); dwx_release(k);
    }
    fx_close(&X);
}
TEST(eum_source_callback_sequence_control)
{
    /* control: the log really differs between entry points and between sub-range and full range */
    FX_BEGIN();
    static const WCHAR t[] = L"abc def";
    dwx_ana_src *s1 = dwx_ana_src_new(t, 7), *s2 = dwx_ana_src_new(t, 7); dwx_ana_sink *k1 = dwx_ana_sink_new(), *k2 = dwx_ana_sink_new();
    char a[256];
    CHECK_HR_OK(run(&X, K_BIDI, s1, 0, 7, k1)); CHECK_HR_OK(run(&X, K_SCRIPT, s2, 0, 7, k2));
    snprintf(a, sizeof a, "%s", logstr(s1));
    CHECK(strcmp(a, logstr(s2)) != 0);
    CHECK(s1->n_dir == 1 && s2->n_dir == 0);          /* only the bidi pass asks for the paragraph direction */
    dwx_release(s1); dwx_release(s2); dwx_release(k1); dwx_release(k2);
    fx_close(&X);
}

/* ---- GetParagraphReadingDirection ------------------------------------------------------------------------------------------- */
TEST(eum_paragraph_direction_asked_once_and_decides_levels)
{
    FX_BEGIN();
    static const WCHAR t[] = { 'a', 'b', 0x05d0 };
    dwx_ana_src *s = dwx_ana_src_new(t, 3); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_BIDI, s, 0, 3, k));
    EXPECT_EQ(s->n_dir, 1);
    EXPECT_EQ(k->bidi_explicit[0], 0); EXPECT_EQ(k->bidi_explicit[2], 0);
    EXPECT_EQ(k->bidi_resolved[0], 0); EXPECT_EQ(k->bidi_resolved[2], 1);
    dwx_release(s); dwx_release(k);
    /* the same text with an RTL paragraph: explicit level 1 everywhere, Latin resolves to 2 */
    s = dwx_ana_src_new(t, 3); k = dwx_ana_sink_new(); s->dir = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
    CHECK_HR_OK(run(&X, K_BIDI, s, 0, 3, k));
    EXPECT_EQ(s->n_dir, 1);
    EXPECT_EQ(k->bidi_explicit[0], 1); EXPECT_EQ(k->bidi_explicit[1], 1); EXPECT_EQ(k->bidi_explicit[2], 1);
    EXPECT_EQ(k->bidi_resolved[0], 2); EXPECT_EQ(k->bidi_resolved[1], 2); EXPECT_EQ(k->bidi_resolved[2], 1);
    dwx_release(s); dwx_release(k);
    fx_close(&X);
}
TEST(eum_paragraph_direction_not_asked_by_script_or_breaks)
{
    FX_BEGIN();
    static const WCHAR t[] = L"ab cd";
    int kinds[2] = { K_SCRIPT, K_BREAK }, i;
    for (i = 0; i < 2; i++) {
        dwx_ana_src *s = dwx_ana_src_new(t, 5); dwx_ana_sink *k = dwx_ana_sink_new();
        CHECK_HR_OK(run(&X, kinds[i], s, 0, 5, k));
        EXPECT_EQ(s->n_dir, 0);
        dwx_release(s); dwx_release(k);
    }
    /* control: bidi does ask */
    { dwx_ana_src *s = dwx_ana_src_new(t, 5); dwx_ana_sink *k = dwx_ana_sink_new();
      CHECK_HR_OK(run(&X, K_BIDI, s, 0, 5, k)); EXPECT_EQ(s->n_dir, 1); dwx_release(s); dwx_release(k); }
    fx_close(&X);
}

/* ---- GetTextBeforePosition -------------------------------------------------------------------------------------------------- */
TEST(eum_text_before_position_only_for_line_breaks_after_the_start)
{
    FX_BEGIN();
    static const WCHAR t[] = L"abc def";
    dwx_ana_src *s = dwx_ana_src_new(t, 7); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_BREAK, s, 3, 4, k));
    EXPECT_EQ(s->n_before, 1);
    CHECK(s->log_n >= 1 && s->log[0].fn == FN_BEFORE && s->log[0].pos == 3);       /* the very first call, at the range start */
    dwx_release(s); dwx_release(k);
    /* control: starting at 0 there is nothing before, the source is not asked */
    s = dwx_ana_src_new(t, 7); k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_BREAK, s, 0, 7, k));
    EXPECT_EQ(s->n_before, 0);
    dwx_release(s); dwx_release(k);
    /* never asked by the other passes, even from the middle of the text */
    { int kinds[3] = { K_SCRIPT, K_BIDI, K_NUMSUB }, i;
      for (i = 0; i < 3; i++) { s = dwx_ana_src_new(t, 7); k = dwx_ana_sink_new(); CHECK_HR_OK(run(&X, kinds[i], s, 3, 4, k)); EXPECT_EQ(s->n_before, 0); dwx_release(s); dwx_release(k); } }
    fx_close(&X);
}
TEST(eum_text_before_position_chunking_and_values)
{
    FX_BEGIN();
    static const WCHAR t[] = L"abc def";
    dwx_ana_src *s = dwx_ana_src_new(t, 7); dwx_ana_sink *k = dwx_ana_sink_new();
    s->chunk = 2;                      /* both GetTextAtPosition and GetTextBeforePosition hand out at most 2 units */
    CHECK_HR_OK(run(&X, K_BREAK, s, 3, 4, k));
    EXPECT_EQ(k->n_break, 1);          /* still a single sink call for the whole range */
    EXPECT_EQ(s->n_before, 1);
    EXPECT_EQ(s->n_at, 7);
    CHECKF(!strcmp(logstr(s), "2@3 1@3 1@3 1@4 1@5 1@6 1@6 1@7"), "log \"%s\"", logstr(s));
    dwx_release(s); dwx_release(k);
    fx_close(&X);
}
TEST(eum_line_break_values_pinned)
{
    FX_BEGIN();
    static const int bef[7] = { 2, 2, 2, 2, 1, 2, 2 }, aft[7] = { 2, 2, 2, 1, 2, 2, 1 };
    static const WCHAR t[] = L"abc def";
    int i;
    dwx_ana_src *s = dwx_ana_src_new(t, 7); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_BREAK, s, 0, 7, k));
    for (i = 0; i < 7; i++) {
        CHECKF(k->brk_set[i], "pos %d not reported", i);
        CHECKF(k->brk[i].breakConditionBefore == bef[i] && k->brk[i].breakConditionAfter == aft[i], "pos %d before/after %d/%d want %d/%d", i,
               k->brk[i].breakConditionBefore, k->brk[i].breakConditionAfter, bef[i], aft[i]);
    }
    /* control: break opportunity exists after the blank (CAN_BREAK = 1) but not inside the word */
    CHECK(k->brk[3].breakConditionAfter == DWRITE_BREAK_CONDITION_CAN_BREAK);
    CHECK(k->brk[1].breakConditionAfter != DWRITE_BREAK_CONDITION_CAN_BREAK);
    dwx_release(s); dwx_release(k);
    fx_close(&X);
}

/* ---- SetScriptAnalysis / SetBidiLevel runs ---------------------------------------------------------------------------------- */
TEST(eum_sink_script_runs)
{
    FX_BEGIN();
    static const WCHAR mixed[] = { 'a', 'b', ' ', 0x05d0, 0x05d1, ' ', 'c', 'd' };         /* Latin, Hebrew, Latin: 3 script runs */
    static const WCHAR han[] = { 0x4e00, 0x4e8c, 'a', 'b' };                                /* Han, Latin: 2 runs */
    dwx_ana_src *s = dwx_ana_src_new(mixed, 8); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_SCRIPT, s, 0, 8, k));
    EXPECT_EQ(k->n_script, 3);
    EXPECT_EQ(k->nrun_script, 3);
    CHECK(dwx_ana_covered(k, 's', 8)); CHECK(!k->overlap);
    CHECK(k->script_runs[0].pos == 0 && k->script_runs[1].pos == 3 && k->script_runs[2].pos == 6);
    CHECK(k->script[0].script == k->script[1].script && k->script[3].script != k->script[0].script && k->script[6].script == k->script[0].script);
    dwx_release(s); dwx_release(k);
    s = dwx_ana_src_new(han, 4); k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_SCRIPT, s, 0, 4, k));
    EXPECT_EQ(k->n_script, 2);
    EXPECT_EQ(s->n_at, 1);
    dwx_release(s); dwx_release(k);
    /* sub-range: a single run, positions outside the range untouched */
    s = dwx_ana_src_new(mixed, 8); k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_SCRIPT, s, 3, 2, k));
    EXPECT_EQ(k->n_script, 1);
    CHECK(!k->script_set[0] && !k->script_set[2] && k->script_set[3] && k->script_set[4] && !k->script_set[5]);
    dwx_release(s); dwx_release(k);
    fx_close(&X);
}
TEST(eum_sink_bidi_runs)
{
    FX_BEGIN();
    static const WCHAR t[] = { 'a', 'b', ' ', 0x05d0, 0x05d1, ' ', 'c', 'd' };
    dwx_ana_src *s = dwx_ana_src_new(t, 8); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_BIDI, s, 0, 8, k));
    EXPECT_EQ(k->n_bidi, 3);                                      /* levels 0 / 1 / 0 */
    CHECK(dwx_ana_covered(k, 'l', 8));
    CHECK(k->bidi_resolved[0] == 0 && k->bidi_resolved[3] == 1 && k->bidi_resolved[4] == 1 && k->bidi_resolved[7] == 0);
    CHECK(k->bidi_runs[0].pos == 0 && k->bidi_runs[1].pos == 3 && k->bidi_runs[2].pos == 5);
    EXPECT_EQ(s->n_locale, 1);
    dwx_release(s); dwx_release(k);
    /* control: pure Latin is a single run */
    s = dwx_ana_src_new(L"abcdefgh", 8); k = dwx_ana_sink_new();
    CHECK_HR_OK(run(&X, K_BIDI, s, 0, 8, k));
    EXPECT_EQ(k->n_bidi, 1);
    dwx_release(s); dwx_release(k);
    fx_close(&X);
}

/* ---- GetNumberSubstitution / SetNumberSubstitution -------------------------------------------------------------------------- */
TEST(eum_number_substitution_reaches_sink_for_digit_runs_only)
{
    FX_BEGIN();
    static const WCHAR t[] = L"a 123 b 45";
    IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *s; dwx_ana_sink *k; int i;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(X.f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-sa", TRUE, &ns));
    if (!ns) { fx_close(&X); return; }
    s = dwx_ana_src_new(t, 10); k = dwx_ana_sink_new(); s->subst = ns;
    CHECK_HR_OK(run(&X, K_NUMSUB, s, 0, 10, k));
    EXPECT_EQ(s->n_subst, 1);
    EXPECT_EQ(k->n_subst, 2);                                     /* "123" and "45", the text between is not reported */
    for (i = 0; i < 10; i++) {
        BOOL digit = (i >= 2 && i <= 4) || i >= 8;
        CHECKF(!!k->subst_set[i] == digit, "pos %d reported=%d", i, k->subst_set[i]);
        if (digit) CHECKF(k->subst[i] == ns, "pos %d wrong object %p", i, (void *)k->subst[i]);
    }
    EXPECT_REFS(ns, 1);                                           /* AddRef from GetNumberSubstitution was released again */
    dwx_release(s); dwx_release(k);
    EXPECT_REFS(ns, 1);
    DW_RELEASE(ns);
    fx_close(&X);
}
TEST(eum_number_substitution_source_run_length_splits_sink_calls)
{
    FX_BEGIN();
    static const WCHAR t[] = L"a 123 b 45";
    IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(X.f, DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL, L"ar-sa", TRUE, &ns));
    if (!ns) { fx_close(&X); return; }
    s = dwx_ana_src_new(t, 10); k = dwx_ana_sink_new(); s->subst = ns; s->subst_run = 3;       /* the source reports runs of 3 */
    CHECK_HR_OK(run(&X, K_NUMSUB, s, 0, 10, k));
    EXPECT_EQ(s->n_subst, 4);
    EXPECT_EQ(k->n_subst, 4);
    dwx_release(s); dwx_release(k);
    /* control: without the run limit the same text costs one source call */
    s = dwx_ana_src_new(t, 10); k = dwx_ana_sink_new(); s->subst = ns;
    CHECK_HR_OK(run(&X, K_NUMSUB, s, 0, 10, k));
    EXPECT_EQ(s->n_subst, 1);
    dwx_release(s); dwx_release(k);
    DW_RELEASE(ns);
    fx_close(&X);
}
TEST(eum_number_substitution_methods_that_do_not_reach_the_sink)
{
    /* FROM_CULTURE, CONTEXTUAL and NONE never produce a SetNumberSubstitution call; NATIONAL and TRADITIONAL do (control). */
    FX_BEGIN();
    static const WCHAR t[] = L"a 123 b 45";
    int m;
    for (m = 0; m < 5; m++) {
        IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *s; dwx_ana_sink *k;
        CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(X.f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, L"ar-sa", TRUE, &ns));
        if (!ns) continue;
        s = dwx_ana_src_new(t, 10); k = dwx_ana_sink_new(); s->subst = ns;
        CHECK_HR_OK(run(&X, K_NUMSUB, s, 0, 10, k));
        EXPECT_EQ(s->n_subst, 1);
        EXPECT_EQ(k->n_subst, (m == DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL || m == DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL) ? 2 : 0);
        dwx_release(s); dwx_release(k); DW_RELEASE(ns);
    }
    fx_close(&X);
}
TEST(eum_number_substitution_null_object)
{
    FX_BEGIN();
    static const WCHAR t[] = L"a 123 b 45";
    dwx_ana_src *s = dwx_ana_src_new(t, 10); dwx_ana_sink *k = dwx_ana_sink_new();       /* source answers S_OK with a NULL object */
    CHECK_HR_OK(run(&X, K_NUMSUB, s, 0, 10, k));
    EXPECT_EQ(s->n_subst, 1);
    EXPECT_EQ(k->n_subst, 0);
    CHECK(!k->subst_set[2]);
    dwx_release(s); dwx_release(k);
    fx_close(&X);
}

/* ---- vertical glyph orientation callbacks ----------------------------------------------------------------------------------- */
TEST(eum_orientation_callbacks)
{
    FX_BEGIN();
    static const WCHAR han_latin[] = { 0x4e00, 0x4e8c, 'a', 'b' };
    static const WCHAR latin[] = L"abcd";
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!X.a1) { fx_close(&X); t_skip("IDWriteTextAnalyzer1 unavailable"); return; }
    s = dwx_ana_src_new(han_latin, 4); k = dwx_ana_sink_new(); s->support_v1 = k->support_v1 = 1;
    CHECK_HR_OK(run(&X, K_ORIENT, s, 0, 4, k));
    EXPECT_EQ(s->n_orient, 1);                                    /* one source answer covers the text */
    EXPECT_EQ(k->n_orient, 2);                                    /* Han and Latin form two runs */
    CHECK(k->orient_set[0] && k->orient_set[3]);
    CHECK(k->orient_sideways[0] != k->orient_sideways[2]);
    dwx_release(s); dwx_release(k);
    s = dwx_ana_src_new(latin, 4); k = dwx_ana_sink_new(); s->support_v1 = k->support_v1 = 1;
    CHECK_HR_OK(run(&X, K_ORIENT, s, 0, 4, k));
    EXPECT_EQ(k->n_orient, 1);                                    /* control: a single script class is a single run */
    dwx_release(s); dwx_release(k);
    /* source reports 2-unit orientation runs: asked once per run, positions ascending */
    s = dwx_ana_src_new(latin, 4); k = dwx_ana_sink_new(); s->support_v1 = k->support_v1 = 1; s->orient_run = 2;
    CHECK_HR_OK(run(&X, K_ORIENT, s, 0, 4, k));
    EXPECT_EQ(s->n_orient, 2);
    CHECKF(!strcmp(logstr(s), "3@0 1@0 6@0 6@2"), "log \"%s\"", logstr(s));   /* the text is fetched once, orientation per run */
    dwx_release(s); dwx_release(k);
    fx_close(&X);
}

/* ---- callback failures stop the walk ---------------------------------------------------------------------------------------- */
TEST(eum_callback_failure_codes_propagate)
{
    FX_BEGIN();
    static const WCHAR t[] = { 'a', 'b', 0x05d0, 0x05d1 };
    static const WCHAR d[] = L"a 123 b 45";
    dwx_ana_src *s; dwx_ana_sink *k; IDWriteNumberSubstitution *ns = NULL;
    s = dwx_ana_src_new(t, 4); k = dwx_ana_sink_new(); k->fail_script = E_ABORT;
    CHECK_HR(run(&X, K_SCRIPT, s, 0, 4, k), E_ABORT); EXPECT_EQ(k->n_script, 1); dwx_release(s); dwx_release(k);
    s = dwx_ana_src_new(t, 4); k = dwx_ana_sink_new(); k->fail_bidi = E_ABORT;
    CHECK_HR(run(&X, K_BIDI, s, 0, 4, k), E_ABORT); EXPECT_EQ(k->n_bidi, 1); dwx_release(s); dwx_release(k);
    s = dwx_ana_src_new(L"ab cd", 5); k = dwx_ana_sink_new(); k->fail_break = E_ABORT;
    CHECK_HR(run(&X, K_BREAK, s, 0, 5, k), E_ABORT); EXPECT_EQ(k->n_break, 1); dwx_release(s); dwx_release(k);
    s = dwx_ana_src_new(d, 10); k = dwx_ana_sink_new(); s->fail_subst = E_ABORT;
    CHECK_HR(run(&X, K_NUMSUB, s, 0, 10, k), E_ABORT); EXPECT_EQ(k->n_subst, 0); dwx_release(s); dwx_release(k);
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(X.f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-sa", TRUE, &ns));
    if (ns) {
        s = dwx_ana_src_new(d, 10); k = dwx_ana_sink_new(); s->subst = ns; k->fail_subst = E_ABORT;
        CHECK_HR(run(&X, K_NUMSUB, s, 0, 10, k), E_ABORT);
        EXPECT_EQ(k->n_subst, 1);                                 /* the second digit run is never delivered */
        EXPECT_EQ(s->n_subst, 1);
        dwx_release(s); dwx_release(k);
        /* control: the same call without an injected failure succeeds and delivers both runs */
        s = dwx_ana_src_new(d, 10); k = dwx_ana_sink_new(); s->subst = ns;
        CHECK_HR_OK(run(&X, K_NUMSUB, s, 0, 10, k)); EXPECT_EQ(k->n_subst, 2);
        dwx_release(s); dwx_release(k);
        DW_RELEASE(ns);
    }
    fx_close(&X);
}

/* ---- the test doubles themselves honour the interface contract (so the observations above mean something) -------------- */
TEST(eum_double_contract_direct_calls)
{
    static const WCHAR t[] = L"abc def";
    dwx_ana_src *s = dwx_ana_src_new(t, 7); dwx_ana_sink *k = dwx_ana_sink_new();
    IDWriteTextAnalysisSource *src = (IDWriteTextAnalysisSource *)s; IDWriteTextAnalysisSink *snk = (IDWriteTextAnalysisSink *)k;
    const WCHAR *txt = NULL; UINT32 len = 0; DWRITE_SCRIPT_ANALYSIS sa = { 5, DWRITE_SCRIPT_SHAPES_DEFAULT };
    DWRITE_LINE_BREAKPOINT bp; IDWriteNumberSubstitution *ns = (IDWriteNumberSubstitution *)1;
    EXPECT_EQ(IDWriteTextAnalysisSource_GetParagraphReadingDirection(src), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    CHECK_HR_OK(IDWriteTextAnalysisSource_GetTextBeforePosition(src, 3, &txt, &len));
    EXPECT_EQ(len, 3); CHECK(txt == t);
    CHECK_HR_OK(IDWriteTextAnalysisSource_GetTextBeforePosition(src, 0, &txt, &len));
    EXPECT_EQ(len, 0); CHECK(txt == NULL);
    ns = (IDWriteNumberSubstitution *)1; len = 0;
    { UINT32 rl = 0; CHECK_HR_OK(IDWriteTextAnalysisSource_GetNumberSubstitution(src, 0, &rl, &ns)); EXPECT_EQ(rl, 7); CHECK(ns == NULL); }
    CHECK_HR_OK(IDWriteTextAnalysisSink_SetScriptAnalysis(snk, 1, 2, &sa));
    CHECK(k->script_set[1] && k->script_set[2] && !k->script_set[3] && k->script[1].script == 5);
    memset(&bp, 0, sizeof bp); bp.breakConditionAfter = DWRITE_BREAK_CONDITION_CAN_BREAK;
    CHECK_HR_OK(IDWriteTextAnalysisSink_SetLineBreakpoints(snk, 0, 1, &bp));
    EXPECT_EQ(k->brk[0].breakConditionAfter, DWRITE_BREAK_CONDITION_CAN_BREAK);
    CHECK_HR_OK(IDWriteTextAnalysisSink_SetBidiLevel(snk, 4, 2, 1, 3));
    CHECK(k->bidi_explicit[4] == 1 && k->bidi_resolved[5] == 3 && !k->bidi_set[6]);
    CHECK_HR_OK(IDWriteTextAnalysisSink_SetNumberSubstitution(snk, 2, 1, (IDWriteNumberSubstitution *)8));
    CHECK(k->subst_set[2] && k->subst[2] == (IDWriteNumberSubstitution *)8);
    /* control: a position never written stays unset */
    CHECK(!k->subst_set[3]);
    dwx_release(s); dwx_release(k);
}
TEST(eum_double_contract_v1_direct_calls)
{
    static const WCHAR t[] = L"abcd";
    dwx_ana_src *s = dwx_ana_src_new(t, 4); dwx_ana_sink *k = dwx_ana_sink_new();
    IDWriteTextAnalysisSource1 *src = (IDWriteTextAnalysisSource1 *)s; IDWriteTextAnalysisSink1 *snk = (IDWriteTextAnalysisSink1 *)k;
    UINT32 rl = 0; DWRITE_VERTICAL_GLYPH_ORIENTATION o = (DWRITE_VERTICAL_GLYPH_ORIENTATION)9; UINT8 bidi = 7;
    s->support_v1 = k->support_v1 = 1; s->orientation = DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED; s->orient_bidi = 2;
    CHECK_HR_OK(IDWriteTextAnalysisSource1_GetVerticalGlyphOrientation(src, 1, &rl, &o, &bidi));
    EXPECT_EQ(rl, 3); EXPECT_EQ(o, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED); EXPECT_EQ(bidi, 2);
    CHECK_HR_OK(IDWriteTextAnalysisSink1_SetGlyphOrientation(snk, 1, 2, DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES, 3, TRUE, TRUE));
    CHECK(k->orient_set[1] && k->orient_set[2] && !k->orient_set[0] && !k->orient_set[3]);
    EXPECT_EQ(k->angle[2], DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES); EXPECT_EQ(k->orient_bidi[1], 3);
    CHECK(k->orient_sideways[1] && k->orient_rtl[2]);
    dwx_release(s); dwx_release(k);
}
