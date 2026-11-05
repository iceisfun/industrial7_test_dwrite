/* AnalyzeNumberSubstitution through a scripted source (NumberSubstitutionContext::Analyze): the source is asked for the substitution
 * object of each run (GetNumberSubstitution(position)), a NULL object or method NONE never produces a sink callback, and for the
 * other methods the sink receives SetNumberSubstitution only for runs of ASCII digits (with their embedded separators), clipped at the
 * source run boundaries, with the very object the source handed out.  A run length of 0 means "the rest of the range". */
#include "ms.h"
#define MT(n) TEST(r2_bidi_linebreak_matrix_##n)

static IDWriteNumberSubstitution *mk(IDWriteFactory *f, DWRITE_NUMBER_SUBSTITUTION_METHOD m, const WCHAR *loc)
{
    IDWriteNumberSubstitution *ns = NULL;
    if (FAILED(IDWriteFactory_CreateNumberSubstitution(f, m, loc, FALSE, &ns))) return NULL;
    return ns;
}
static void src_positions(ms_src *s, UINT32 *out, int *n)
{
    int i; *n = 0;
    for (i = 0; i < s->nlog; i++) if (s->log[i].fn == MF_SUBST && *n < 64) out[(*n)++] = s->log[i].pos;
}
static void run_positions(const WCHAR *t, IDWriteNumberSubstitution *(*make)(IDWriteFactory *), UINT32 pos, UINT32 len, UINT32 run, const UINT32 *want, int nwant)
{
    MS_BEGIN(); ms_src *s = ms_src_new(t, LEN(t)); ms_sink *k = ms_sink_new(0); UINT32 got[64]; int n, i;
    s->subst = make ? make(c_.f) : NULL; s->subst_run = run;
    CHECK_HR_OK(ms_do(an, MA_NUMSUB, s, pos, len, k));
    src_positions(s, got, &n);
    EXPECT_EQ(n, nwant);
    for (i = 0; i < n && i < nwant; i++) EXPECT_EQ(got[i], want[i]);
    if (s->subst) IDWriteNumberSubstitution_Release(s->subst);
    s->subst = NULL;
    ms_src_free(s); ms_sink_free(k); MS_END();
}
static IDWriteNumberSubstitution *mk_none(IDWriteFactory *f) { return mk(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-us"); }
static IDWriteNumberSubstitution *mk_national_ar(IDWriteFactory *f) { return mk(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA"); }
static IDWriteNumberSubstitution *mk_trad_hi(IDWriteFactory *f) { return mk(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL, L"hi-IN"); }
static IDWriteNumberSubstitution *mk_ctx_ar(IDWriteFactory *f) { return mk(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL, L"ar-SA"); }
static IDWriteNumberSubstitution *mk_culture_ar(IDWriteFactory *f) { return mk(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE, L"ar-SA"); }

#define POSTEST(nm, text, make, pos, len, run, ...) MT(numsub_positions_##nm) { static const UINT32 w_[] = { __VA_ARGS__ }; run_positions(text, make, pos, len, run, w_, (int)(sizeof w_ / sizeof w_[0])); }
POSTEST(null_whole,        L"abcdef", NULL,           0, 6, 0, 0)
POSTEST(null_run2,         L"abcdef", NULL,           0, 6, 2, 0, 2, 4)
POSTEST(null_run1,         L"abcdef", NULL,           0, 6, 1, 0, 1, 2, 3, 4, 5)
POSTEST(null_run4,         L"abcdef", NULL,           0, 6, 4, 0, 4)
POSTEST(null_run_exact,    L"abcdef", NULL,           0, 6, 6, 0)
POSTEST(null_run_longer,   L"abcdef", NULL,           0, 6, 100, 0)
POSTEST(null_sub_run2,     L"abcdef", NULL,           1, 4, 2, 1, 3)
POSTEST(null_sub_run3,     L"abcdefgh", NULL,         2, 5, 3, 2, 5)
POSTEST(none_whole,        L"ab 123", mk_none,        0, 6, 0, 0)
POSTEST(none_run2,         L"ab 123", mk_none,        0, 6, 2, 0, 2, 4)
POSTEST(none_run3,         L"ab 123", mk_none,        0, 6, 3, 0, 3)
POSTEST(none_sub,          L"ab 123 cd", mk_none,     3, 3, 0, 3)
POSTEST(national_whole,    L"ab 123", mk_national_ar, 0, 6, 0, 0)
POSTEST(national_run2,     L"ab 123", mk_national_ar, 0, 6, 2, 0, 2, 4)
POSTEST(national_run1,     L"ab 123", mk_national_ar, 0, 6, 1, 0, 1, 2, 3, 4, 5)
POSTEST(traditional_run2,  L"ab 123", mk_trad_hi,     0, 6, 2, 0, 2, 4)
POSTEST(contextual_run3,   L"12 34 56", mk_ctx_ar,    0, 8, 3, 0, 3, 6)
POSTEST(culture_run4,      L"12 34 56", mk_culture_ar, 0, 8, 4, 0, 4)
POSTEST(sub_range_starts_midrun, L"abcdef", mk_none, 3, 3, 0, 3)

/* zero-length range: neither source nor sink is called */
MT(numsub_zero_length_no_calls) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"abc", 3); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_NUMSUB, s, 1, 0, k));
    EXPECT_EQ(s->nlog, 0); EXPECT_EQ(k->nev, 0);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(numsub_null_object_no_sink_calls) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"abc 123 def 456", 15); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_NUMSUB, s, 0, 15, k));
    EXPECT_EQ(k->n[MK_SUBST], 0); EXPECT_EQ(s->n[MF_AT] >= 1, 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(numsub_none_method_no_sink_calls) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"abc 123 def 456", 15); ms_sink *k = ms_sink_new(0);
    s->subst = mk_none(c_.f);
    if (!s->subst) { t_skip("CreateNumberSubstitution failed"); ms_src_free(s); ms_sink_free(k); MS_END(); return; }
    CHECK_HR_OK(ms_do(an, MA_NUMSUB, s, 0, 15, k));
    EXPECT_EQ(k->n[MK_SUBST], 0);
    IDWriteNumberSubstitution_Release(s->subst); s->subst = NULL;
    ms_src_free(s); ms_sink_free(k); MS_END();
}
/* callbacks (if any) carry the object the source handed out, never cover a letter, and lie inside the digit runs */
static void numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD m, const WCHAR *loc, const WCHAR *t, UINT32 run, int pat)
{
    MS_BEGIN(); ms_src *s = ms_src_new(t, LEN(t)); ms_sink *k = ms_sink_new(0); int i; UINT32 j;
    s->subst = mk(c_.f, m, loc); s->subst_run = run;
    if (!s->subst) { t_skip("CreateNumberSubstitution failed"); ms_src_free(s); ms_sink_free(k); MS_END(); return; }
    if (pat) ms_src_cuts(s, ms_patterns[pat], 6);
    CHECK_HR_OK(ms_do(an, MA_NUMSUB, s, 0, LEN(t), k));
    for (i = 0; i < k->nev; i++) {
        const ms_ev *e = &k->ev[i];
        CHECK(e->fam == MK_SUBST); CHECK(e->ns == s->subst); CHECK(e->len > 0);
        for (j = e->pos; j < e->pos + e->len && j < LEN(t); j++) CHECKF(t[j] != 'a' && t[j] != 'b' && t[j] != 'c', "callback covers letter at %u", j);
        if (i > 0) CHECK(e->pos >= k->ev[i - 1].pos + k->ev[i - 1].len);
    }
    t_note("method %d locale %ls run %u pieces %d: %d SetNumberSubstitution call(s)", (int)m, loc, run, pat, k->n[MK_SUBST]);
    IDWriteNumberSubstitution_Release(s->subst); s->subst = NULL;
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(numsub_national_arabic_digits_runs)   { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", L"abc 123 abc 45,6.7 abc", 0, 0); }
MT(numsub_national_arabic_digits_run3)   { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", L"abc 123 abc 45,6.7 abc", 3, 0); }
MT(numsub_national_arabic_digits_run1)   { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", L"abc 123 abc 45,6.7 abc", 1, 0); }
MT(numsub_national_arabic_digits_pieces) { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", L"abc 123 abc 45,6.7 abc", 0, 1); }
MT(numsub_traditional_hindi)             { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL, L"hi-IN", L"abc 123 abc 45,6.7 abc", 0, 0); }
MT(numsub_traditional_thai)              { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL, L"th-TH", L"abc 123 abc 45,6.7 abc", 0, 0); }
MT(numsub_contextual_arabic)             { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL, L"ar-SA", L"abc 123 abc 45,6.7 abc", 0, 0); }
MT(numsub_from_culture_arabic)           { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE, L"ar-SA", L"abc 123 abc 45,6.7 abc", 0, 0); }
MT(numsub_from_culture_english)          { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE, L"en-US", L"abc 123 abc 45,6.7 abc", 0, 0); }
MT(numsub_national_english)              { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"en-US", L"abc 123 abc 45,6.7 abc", 0, 0); }
MT(numsub_national_persian)              { numsub_method(DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"fa-IR", L"abc 123 abc 45,6.7 abc", 0, 0); }

/* a number split by a source run boundary produces two callbacks (or none); never one callback across the boundary */
MT(numsub_number_cut_at_source_run_boundary) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"12345", 5); ms_sink *k = ms_sink_new(0); int i;
    s->subst = mk(c_.f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA"); s->subst_run = 2;
    if (!s->subst) { t_skip("CreateNumberSubstitution failed"); ms_src_free(s); ms_sink_free(k); MS_END(); return; }
    CHECK_HR_OK(ms_do(an, MA_NUMSUB, s, 0, 5, k));
    for (i = 0; i < k->nev; i++) CHECKF(!(k->ev[i].pos < 2 && k->ev[i].pos + k->ev[i].len > 2) && !(k->ev[i].pos < 4 && k->ev[i].pos + k->ev[i].len > 4), "callback [%u,%u) crosses a source run boundary", k->ev[i].pos, k->ev[i].pos + k->ev[i].len);
    t_note("12345 in runs of 2 (national, ar-SA): %d callback(s)", k->n[MK_SUBST]);
    IDWriteNumberSubstitution_Release(s->subst); s->subst = NULL;
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(numsub_object_refcount_balanced) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"abc 123 def", 11); ms_sink *k = ms_sink_new(0); int p;
    s->subst = mk(c_.f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA");
    if (!s->subst) { t_skip("CreateNumberSubstitution failed"); ms_src_free(s); ms_sink_free(k); MS_END(); return; }
    for (p = 0; p < 3; p++) {
        ms_sink *k2 = ms_sink_new(0);
        CHECK_HR_OK(ms_do(an, MA_NUMSUB, s, 0, 11, k2));
        EXPECT_REFS(s->subst, 1);                           /* every reference taken for the call was released */
        ms_sink_free(k2);
    }
    IDWriteNumberSubstitution_Release(s->subst); s->subst = NULL;
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(numsub_sink_fail_stops) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"1 a 2 a 3", 9); ms_sink *k = ms_sink_new(0); HRESULT hr;
    s->subst = mk(c_.f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA");
    if (!s->subst) { t_skip("CreateNumberSubstitution failed"); ms_src_free(s); ms_sink_free(k); MS_END(); return; }
    k->fail_kind = MK_SUBST; k->fail_after = 1; k->fail_hr = E_FAIL;
    hr = ms_do(an, MA_NUMSUB, s, 0, 9, k);
    if (k->n[MK_SUBST] == 0) { CHECK_HR_OK(hr); t_note("national ar-SA produced no callbacks for ASCII digits"); }
    else { CHECK_HR_FAILED(hr); EXPECT_EQ(k->n[MK_SUBST], 1); t_note("object references after the aborted walk: %lu", dw_refcount((IUnknown *)s->subst)); }
    IDWriteNumberSubstitution_Release(s->subst); s->subst = NULL;
    ms_src_free(s); ms_sink_free(k); MS_END();
}
