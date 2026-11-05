/* IDWriteFactory::CreateTextAnalyzer and the identity/lifetime rules of the returned object, plus the argument validation shared by
 * the four Analyze* methods.
 *
 * Each Analyze* entry point rejects (before any callback) a NULL source, a NULL sink or textPosition + textLength overflowing 32 bits
 * with E_INVALIDARG; a zero length is valid at any position (including 0xFFFFFFFF) and returns S_OK. */
#include "ta.h"

static const IID IID_Unknown_probe = { 0x7b6a1d15, 0x2b4c, 0x4a9d, { 0x9a, 0x01, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 } };

TEST(create_basic)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *a = (IDWriteTextAnalyzer *)(ULONG_PTR)1;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a));
    CHECK(a != NULL && a != (IDWriteTextAnalyzer *)(ULONG_PTR)1);
    if (a && a != (IDWriteTextAnalyzer *)(ULONG_PTR)1) { EXPECT_REFS(a, 1); IDWriteTextAnalyzer_Release(a); }
}

TEST(create_distinct_objects)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *a = NULL, *b = NULL;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a));
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &b));
    CHECK(a && b);
    if (a && b) {
        /* IDWriteFactory::CreateTextAnalyzer allocates a new object on every call */
        CHECK(a != b);
        if (a != b) { EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); }
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(create_isolated_factory)
{
    IDWriteFactory *f = dw_isolated(); IDWriteTextAnalyzer *a = NULL;
    if (!f) { t_skip("no isolated factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a));
    CHECK(a != NULL);
    DW_RELEASE(a);
}

TEST(create_refcount_protocol)
{
    TA_BEGIN();
    EXPECT_REFS(an, 1);
    { ULONG r;
    r = IDWriteTextAnalyzer_AddRef(an); EXPECT_EQ(r, 2);
    r = IDWriteTextAnalyzer_AddRef(an); EXPECT_EQ(r, 3);
    r = IDWriteTextAnalyzer_Release(an); EXPECT_EQ(r, 2);
    r = IDWriteTextAnalyzer_Release(an); EXPECT_EQ(r, 1); }
    TA_END();
}

TEST(qi_self_identity)
{
    TA_BEGIN();
    IDWriteTextAnalyzer *p = NULL; IUnknown *u = NULL;
    CHECK_HR_OK(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer, (void **)&p));
    CHECK(p == an);
    EXPECT_REFS(an, 2);
    DW_RELEASE(p);
    CHECK_HR_OK(IDWriteTextAnalyzer_QueryInterface(an, &IID_IUnknown, (void **)&u));
    CHECK(u != NULL);
    EXPECT_REFS(an, 2);
    DW_RELEASE(u);
    EXPECT_REFS(an, 1);
    TA_END();
}

TEST(qi_unknown_iid_fails)
{
    TA_BEGIN();
    void *p = (void *)(ULONG_PTR)0x1234;
    CHECK_HR(IDWriteTextAnalyzer_QueryInterface(an, &IID_Unknown_probe, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    EXPECT_REFS(an, 1);
    p = (void *)(ULONG_PTR)0x1234;
    CHECK_HR(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteFactory, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    p = (void *)(ULONG_PTR)0x1234;
    CHECK_HR(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextFormat, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    TA_END();
}

TEST(qi_analyzer1_analyzer2)
{
    TA_BEGIN();
    /* Windows 8 / 8.1 and later: IDWriteTextAnalyzer1 and IDWriteTextAnalyzer2 are implemented by the same object */
    IDWriteTextAnalyzer1 *a1 = NULL; IDWriteTextAnalyzer2 *a2 = NULL;
    HRESULT h1 = IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer1, (void **)&a1);
    HRESULT h2 = IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer2, (void **)&a2);
    CHECK_HR_OK(h1); CHECK_HR_OK(h2);
    if (a1) { CHECK(dw_same_identity((IUnknown *)an, (IUnknown *)a1)); EXPECT_REFS(an, 3 - (a2 ? 0 : 1)); }
    if (a1 && a2) {
        IDWriteTextAnalyzer1 *back = NULL;
        CHECK_HR_OK(IDWriteTextAnalyzer2_QueryInterface(a2, &IID_IDWriteTextAnalyzer1, (void **)&back));
        CHECK(back == a1);
        DW_RELEASE(back);
        IDWriteTextAnalyzer *base = NULL;
        CHECK_HR_OK(IDWriteTextAnalyzer2_QueryInterface(a2, &IID_IDWriteTextAnalyzer, (void **)&base));
        CHECK(base == an);
        DW_RELEASE(base);
    }
    DW_RELEASE(a1); DW_RELEASE(a2);
    EXPECT_REFS(an, 1);
    TA_END();
}

TEST(qi_analyzer3_probe)
{
    TA_BEGIN();
    /* IDWriteTextAnalyzer3 does not exist; unrelated newer-factory IIDs must not be answered */
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteFactory3, &p);
    CHECK_HR(hr, E_NOINTERFACE); CHECK_OUT_NULL(p);
    TA_END();
}

TEST(lifetime_after_factory_release)
{
    /* the analyzer is usable after the factory that made it has been released */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteTextAnalyzer *a = NULL;
    static const WCHAR t[] = L"abc";
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a));
    DW_RELEASE(f);
    if (!a) return;
    {
        dwx_ana_src *s; dwx_ana_sink *k;
        CHECK_HR_OK(ta_run(a, TA_SCRIPT, t, &s, &k));
        CHECK(ta_runs_exact(k, 's', 0, 3));
        ta_free(s, k);
    }
    DW_RELEASE(a);
}

TEST(lifetime_many_create_release)
{
    IDWriteFactory *f = dw_shared(); int i;
    if (!f) { t_skip("no factory"); return; }
    for (i = 0; i < 200; i++) {
        IDWriteTextAnalyzer *a = NULL;
        if (FAILED(IDWriteFactory_CreateTextAnalyzer(f, &a)) || !a) { CHECKF(0, "iteration %d", i); break; }
        IDWriteTextAnalyzer_Release(a);
    }
}

/* ---- argument validation for the four analysis entry points ---------------------------------------------------------------------- */
static const char *const kname[] = { "", "AnalyzeScript", "AnalyzeBidi", "AnalyzeNumberSubstitution", "AnalyzeLineBreakpoints" };

static void check_untouched(const char *what, const dwx_ana_src *s, const dwx_ana_sink *k)
{
    CHECKF(s == NULL || (s->n_at == 0 && s->n_before == 0 && s->n_dir == 0 && s->n_locale == 0 && s->n_subst == 0), "%s: source was called", what);
    CHECKF(k == NULL || (k->n_script == 0 && k->n_break == 0 && k->n_bidi == 0 && k->n_subst == 0), "%s: sink was called", what);
}

static void invalid_args_for(int kind)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abcdef";
    dwx_ana_src *s = dwx_ana_src_new(t, 6); dwx_ana_sink *k = dwx_ana_sink_new();
    IDWriteTextAnalysisSource *src = (IDWriteTextAnalysisSource *)s; IDWriteTextAnalysisSink *snk = (IDWriteTextAnalysisSink *)k;
    HRESULT hr;
#define CALL(sr, p, l, sn) (kind == TA_SCRIPT ? IDWriteTextAnalyzer_AnalyzeScript(an, sr, p, l, sn) : \
                            kind == TA_BIDI ? IDWriteTextAnalyzer_AnalyzeBidi(an, sr, p, l, sn) : \
                            kind == TA_NUMSUB ? IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, sr, p, l, sn) : \
                            IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, sr, p, l, sn))
    if (t_is_wine()) t_note("NULL source/sink cases skipped (the emulator dereferences them)");
    else {
        hr = CALL(NULL, 0, 6, snk);       CHECK_HR(hr, E_INVALIDARG); check_untouched("null source", s, k);
        hr = CALL(src, 0, 6, NULL);       CHECK_HR(hr, E_INVALIDARG); check_untouched("null sink", s, k);
        hr = CALL(NULL, 0, 6, NULL);      CHECK_HR(hr, E_INVALIDARG); check_untouched("both null", s, k);
        hr = CALL(NULL, 0xFFFFFFFFu, 1, NULL); CHECK_HR(hr, E_INVALIDARG);
    }
    if (t_is_wine() && kind == TA_BREAK) { ta_free(s, k); TA_END(); t_skip("emulator reads text before validating"); return; }
    hr = CALL(src, 0xFFFFFFFFu, 1, snk);  CHECK_HR(hr, E_INVALIDARG); check_untouched("pos max len 1", s, k);
    hr = CALL(src, 1, 0xFFFFFFFFu, snk);  CHECK_HR(hr, E_INVALIDARG); check_untouched("pos 1 len max", s, k);
    hr = CALL(src, 0x80000000u, 0x80000000u, snk); CHECK_HR(hr, E_INVALIDARG); check_untouched("2^31 + 2^31", s, k);
    hr = CALL(src, 0xFFFFFFF0u, 0x20, snk); CHECK_HR(hr, E_INVALIDARG); check_untouched("wrap", s, k);
#undef CALL
    EXPECT_REFS(an, 1);
    (void)kname;
    ta_free(s, k); TA_END();
}
TEST(args_invalid_script)  { invalid_args_for(TA_SCRIPT); }
TEST(args_invalid_bidi)    { invalid_args_for(TA_BIDI); }
TEST(args_invalid_numsub)  { invalid_args_for(TA_NUMSUB); }
TEST(args_invalid_break)   { invalid_args_for(TA_BREAK); }

static void zero_len_for(int kind)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abcdef";
    static const UINT32 pos[] = { 0, 3, 6, 7, 1000, 0xFFFFFFFFu };
    int i;
    for (i = 0; i < (int)(sizeof pos / sizeof pos[0]); i++) {
        dwx_ana_src *s = dwx_ana_src_new(t, 6); dwx_ana_sink *k = dwx_ana_sink_new();
        HRESULT hr = ta_do(an, kind, s, pos[i], 0, k);
        CHECKF(hr == S_OK, "kind %d pos %u: %s", kind, pos[i], dw_hr_name(hr));
        CHECKF(k->n_script == 0 && k->n_break == 0 && k->n_subst == 0, "kind %d pos %u: sink called for zero length", kind, pos[i]);
        if (kind == TA_BIDI && k->n_bidi) t_note("AnalyzeBidi with zero length produced %ld SetBidiLevel calls", k->n_bidi);
        else CHECK(k->n_bidi == 0);
        ta_free(s, k);
    }
    TA_END();
}
TEST(args_zero_length_script) { zero_len_for(TA_SCRIPT); }
TEST(args_zero_length_bidi)   { zero_len_for(TA_BIDI); }
TEST(args_zero_length_numsub) { zero_len_for(TA_NUMSUB); }
TEST(args_zero_length_break)  { zero_len_for(TA_BREAK); }

static void zero_len_nosource_calls(int kind)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abcdef";
    dwx_ana_src *s = dwx_ana_src_new(t, 6); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, kind, s, 2, 0, k));
    /* a zero-length request never asks the source for text */
    CHECK(s->n_at == 0);
    CHECK(s->n_before == 0);
    ta_free(s, k); TA_END();
}
TEST(args_zero_length_no_text_script) { zero_len_nosource_calls(TA_SCRIPT); }
TEST(args_zero_length_no_text_numsub) { zero_len_nosource_calls(TA_NUMSUB); }
TEST(args_zero_length_no_text_break)  { zero_len_nosource_calls(TA_BREAK); }

/* GetParagraphReadingDirection: AnalyzeBidi and AnalyzeNumberSubstitution ask once per call; script / line breaking never do. */
static void dir_calls(int kind, LONG want)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc def 123";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, kind, t, &s, &k));
    CHECKF(s->n_dir == want, "kind %d: GetParagraphReadingDirection called %ld times, want %ld", kind, s->n_dir, want);
    ta_free(s, k); TA_END();
}
TEST(source_direction_called_bidi)   { dir_calls(TA_BIDI, 1); }
TEST(source_direction_called_numsub) { dir_calls(TA_NUMSUB, 1); }
TEST(source_direction_not_called_script) { dir_calls(TA_SCRIPT, 0); }
TEST(source_direction_not_called_break)   { dir_calls(TA_BREAK, 0); }

TEST(source_direction_once_even_if_chunked)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc def 123 abc";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run_n(an, TA_BIDI, t, LEN(t), 2, &s, &k));
    CHECK(s->n_dir == 1);
    ta_free(s, k);
    CHECK_HR_OK(ta_run_n(an, TA_NUMSUB, t, LEN(t), 1, &s, &k));
    CHECK(s->n_dir == 1);
    ta_free(s, k);
    TA_END();
}

TEST(source_not_retained)
{
    /* the analyzer holds no reference to the source or sink after the call returns */
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x03b1\x03b2 123";
    int kind;
    for (kind = TA_SCRIPT; kind <= TA_BREAK; kind++) {
        dwx_ana_src *s = dwx_ana_src_new(t, LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new();
        CHECK_HR_OK(ta_do(an, kind, s, 0, LEN(t), k));
        CHECKF(dwx_refs(s) == 1, "kind %d: source refs %ld", kind, (long)dwx_refs(s));
        CHECKF(dwx_refs(k) == 1, "kind %d: sink refs %ld", kind, (long)dwx_refs(k));
        ta_free(s, k);
        CHECK(dwx_destroyed(s) && dwx_destroyed(k));
    }
    TA_END();
}

TEST(source_sink_qi_not_required)
{
    /* the plain IDWriteTextAnalysisSource/Sink vtables are enough: no QueryInterface is issued on them for the legacy entry points */
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x05d0\x05d1 123";
    int kind;
    for (kind = TA_SCRIPT; kind <= TA_BREAK; kind++) {
        dwx_ana_src *s = dwx_ana_src_new(t, LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new();
        CHECK_HR_OK(ta_do(an, kind, s, 0, LEN(t), k));
        if (dwx_qi_calls(s) || dwx_qi_calls(k)) t_note("kind %d: QI calls src %ld sink %ld", kind, (long)dwx_qi_calls(s), (long)dwx_qi_calls(k));
        ta_free(s, k);
    }
    TA_END();
}

/* ---- IDWriteFactory::CreateNumberSubstitution (the objects the number substitution analysis consumes) ----------------------------- */
TEST(numsub_create_null_locale)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = (IDWriteNumberSubstitution *)(ULONG_PTR)1;
    if (!f) { t_skip("no factory"); return; }
    /* a locale name is required unless the method is NONE */
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, NULL, FALSE, &ns), E_INVALIDARG);
    CHECK_OUT_NULL(ns);
    ns = (IDWriteNumberSubstitution *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE, NULL, FALSE, &ns), E_INVALIDARG);
    CHECK_OUT_NULL(ns);
    ns = (IDWriteNumberSubstitution *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL, NULL, FALSE, &ns), E_INVALIDARG);
    CHECK_OUT_NULL(ns);
    ns = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, NULL, FALSE, &ns));
    CHECK(ns != NULL);
    if (ns) EXPECT_REFS(ns, 1);
    DW_RELEASE(ns);
}
TEST(numsub_create_invalid_method)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = (IDWriteNumberSubstitution *)(ULONG_PTR)1;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)5, L"en-us", FALSE, &ns), E_INVALIDARG);
    CHECK_OUT_NULL(ns);
    ns = (IDWriteNumberSubstitution *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)0x7fffffff, L"en-us", FALSE, &ns), E_INVALIDARG);
    CHECK_OUT_NULL(ns);
}
TEST(numsub_create_ok_methods)
{
    IDWriteFactory *f = dw_shared(); int m;
    if (!f) { t_skip("no factory"); return; }
    for (m = 0; m <= 4; m++) {
        IDWriteNumberSubstitution *ns = NULL;
        CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, L"ar-sa", TRUE, &ns));
        if (ns) { EXPECT_REFS(ns, 1); DW_RELEASE(ns); }
    }
}
TEST(numsub_create_qi)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; void *p = (void *)(ULONG_PTR)1;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-sa", FALSE, &ns));
    if (!ns) return;
    CHECK_HR(IDWriteNumberSubstitution_QueryInterface(ns, &IID_IDWriteTextAnalyzer, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    CHECK_HR_OK(IDWriteNumberSubstitution_QueryInterface(ns, &IID_IDWriteNumberSubstitution, &p));
    CHECK(p == ns);
    IDWriteNumberSubstitution_Release(ns);
    DW_RELEASE(ns);
}
