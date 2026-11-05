/* Source / sink call protocol of AnalyzeLineBreakpoints, AnalyzeBidi and AnalyzeVerticalGlyphOrientation: which source methods are
 * asked, in which order, at which positions, how NULL / zero length answers are interpreted, how sink callbacks are delivered and what
 * a failing sink does to the rest of the walk.  Every expectation is derived from the dwrite.dll decompilation noted in ms.h. */
#include "ms.h"
#define MT(n) TEST(r2_bidi_linebreak_matrix_##n)

static const char *fnname(int fn) { static const char *n[] = { "?", "AT", "BEFORE", "DIR", "LOCALE", "SUBST", "ORIENT" }; return fn >= 0 && fn <= 6 ? n[fn] : "?"; }
static void dump_log(const ms_src *s, char *buf, size_t cap)
{
    int i; size_t o = 0; buf[0] = 0;
    for (i = 0; i < s->nlog && o + 24 < cap; i++) o += snprintf(buf + o, cap - o, "%s(%u)=%u ", fnname(s->log[i].fn), s->log[i].pos, s->log[i].ret);
}
/* log shape check: sequence of (fn,pos) pairs */
typedef struct { int fn; UINT32 pos; } step;
static void expect_log(const ms_src *s, const step *w, int n, const char *file, int line)
{
    char got[512]; int i, ok = (s->nlog == n);
    for (i = 0; ok && i < n; i++) if (s->log[i].fn != w[i].fn || s->log[i].pos != w[i].pos) ok = 0;
    dump_log(s, got, sizeof got);
    t_check(ok, file, line, "source call log", "got: %s", got);
}
#define EXPECT_LOG(s, ...) do { static const step w_[] = { __VA_ARGS__ }; expect_log(s, w_, (int)(sizeof w_ / sizeof w_[0]), __FILE__, __LINE__); } while (0)

/* ---- line breakpoints: source protocol --------------------------------------------------------------------------------------- */
MT(calls_break_whole_text_two_at_calls) {
    /* the iterator asks for the range once; the character after the range is asked for once */
    MS_BEGIN(); ms_src *s = ms_src_new(L"abc", 3); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 3, k));
    EXPECT_LOG(s, { MF_AT, 0 }, { MF_AT, 3 });
    EXPECT_EQ(s->log[0].ret, 3); EXPECT_EQ(s->log[1].ret, 0);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_no_other_source_methods) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"ab cd", 5); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 5, k));
    EXPECT_EQ(s->n[MF_DIR], 0); EXPECT_EQ(s->n[MF_LOCALE], 0); EXPECT_EQ(s->n[MF_SUBST], 0); EXPECT_EQ(s->n[MF_ORIENT], 0); EXPECT_EQ(s->n[MF_BEFORE], 0);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_subrange_asks_before_first) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"ab cd ef", 8); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 3, 2, k));
    EXPECT_LOG(s, { MF_BEFORE, 3 }, { MF_AT, 3 }, { MF_AT, 5 });
    CHECK(ms_sink_exact(k, MK_BREAK, 3, 2));
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_at_end_of_text_subrange) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"ab cd", 5); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 2, 3, k));
    EXPECT_LOG(s, { MF_BEFORE, 2 }, { MF_AT, 2 }, { MF_AT, 5 });
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_zero_length_no_calls) {
    static const UINT32 pos[] = { 0, 1, 3 }; int i;
    MS_BEGIN();
    for (i = 0; i < 3; i++) {
        ms_src *s = ms_src_new(L"abcd", 4); ms_sink *k = ms_sink_new(0);
        CHECK_HR_OK(ms_do(an, MA_BREAK, s, pos[i], 0, k));
        EXPECT_EQ(s->nlog, 0); EXPECT_EQ(k->nev, 0);
        ms_src_free(s); ms_sink_free(k);
    }
    MS_END();
}
MT(calls_break_single_sink_call_with_range) {
    static const struct { UINT32 pos, len; } r[] = { { 0, 5 }, { 1, 3 }, { 2, 3 }, { 0, 1 }, { 4, 1 } }; int i;
    MS_BEGIN();
    for (i = 0; i < 5; i++) {
        ms_src *s = ms_src_new(L"ab cd", 5); ms_sink *k = ms_sink_new(0);
        CHECK_HR_OK(ms_do(an, MA_BREAK, s, r[i].pos, r[i].len, k));
        EXPECT_EQ(k->nev, 1); EXPECT_EQ(k->ev[0].fam, MK_BREAK); EXPECT_EQ(k->ev[0].pos, r[i].pos); EXPECT_EQ(k->ev[0].len, r[i].len);
        ms_src_free(s); ms_sink_free(k);
    }
    MS_END();
}
/* LF, LS, PS end a range: the character after the range cannot change the "after" condition and is never requested. */
static void break_hard_end(WCHAR hard, int want_lookahead_calls, int must)
{
    MS_BEGIN(); WCHAR t[8] = { 'a', 'b', 0, 'c', 'd', 0 }; ms_src *s; ms_sink *k;
    t[2] = hard; s = ms_src_new(t, 5); k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 3, k));
    if (want_lookahead_calls == 0) EXPECT_EQ(ms_log_count(s, MF_AT, 3), 0);
    else if (want_lookahead_calls == 1) EXPECT_EQ(ms_log_count(s, MF_AT, 3), 1);
    else t_note("U+%04X ending a range: %d lookahead call(s) at the position after the range", hard, ms_log_count(s, MF_AT, 3));
    EXPECT_EQ(BRK_AFTER(k->brk[2]), must ? BC_MUST : BC_NEUTRAL);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_lf_end_no_lookahead)    { break_hard_end(0x000a, 0, 1); }
MT(calls_break_ls_end_no_lookahead)    { break_hard_end(0x2028, 0, 1); }
MT(calls_break_ps_end_no_lookahead)    { break_hard_end(0x2029, 0, 1); }
MT(calls_break_nel_end_lookahead_note) { break_hard_end(0x0085, -1, 1); }
MT(calls_break_vt_end_lookahead_note)  { break_hard_end(0x000b, -1, 1); }
MT(calls_break_ff_end_lookahead_note)  { break_hard_end(0x000c, -1, 1); }
MT(calls_break_cr_end_lookahead_note)  {
    /* CR must look at the next character (CR LF is one break) */
    MS_BEGIN(); ms_src *s = ms_src_new(L"ab\r\ncd", 6); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 3, k));
    t_note("CR ending a range: %d lookahead call(s); after-condition %d", ms_log_count(s, MF_AT, 3), BRK_AFTER(k->brk[2]));
    CHECK(ms_log_count(s, MF_AT, 3) <= 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_letter_end_asks_for_next) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"abcd", 4); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 2, k));
    EXPECT_EQ(ms_log_count(s, MF_AT, 2), 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}

/* Condition at the start of the range depends only on the last unit returned by GetTextBeforePosition. */
static void before_case(WCHAR prev, WCHAR first, int want)
{
    MS_BEGIN(); WCHAR t[4]; ms_src *s; ms_sink *k;
    t[0] = prev; t[1] = first; t[2] = 'x'; t[3] = 0;
    s = ms_src_new(t, 3); k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 1, 2, k));
    CHECKF(BRK_BEFORE(k->brk[1]) == want, "before U+%04X + U+%04X: got %d want %d", prev, first, BRK_BEFORE(k->brk[1]), want);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_before_letter_neutral)  { before_case('a', 'c', BC_NEUTRAL); }
MT(calls_break_before_space_neutral)   { before_case(' ', 'c', BC_NEUTRAL); }
MT(calls_break_before_hyphen_neutral)  { before_case('-', 'c', BC_NEUTRAL); }
MT(calls_break_before_zwsp_neutral)    { before_case(0x200b, 'c', BC_NEUTRAL); }
MT(calls_break_before_lf_must)         { before_case('\n', 'c', BC_MUST); }
MT(calls_break_before_cr_must)         { before_case('\r', 'c', BC_MUST); }
MT(calls_break_before_ls_must)         { before_case(0x2028, 'c', BC_MUST); }
MT(calls_break_before_ps_must)         { before_case(0x2029, 'c', BC_MUST); }
MT(calls_break_before_nel_must)        { before_case(0x0085, 'c', BC_MUST); }
MT(calls_break_before_cr_lf_neutral)   { before_case('\r', '\n', BC_NEUTRAL); }   /* CR x LF is not a break of its own */
MT(calls_break_before_lf_lf_must)      { before_case('\n', '\n', BC_MUST); }
MT(calls_break_before_lf_space_must)   { before_case('\n', ' ', BC_MUST); }

/* Condition at the end of the range depends only on the first unit returned by GetTextAtPosition(textPosition + textLength). */
static void after_case(WCHAR last, WCHAR next, int want)
{
    MS_BEGIN(); WCHAR t[4]; ms_src *s; ms_sink *k;
    t[0] = 'x'; t[1] = last; t[2] = next; t[3] = 0;
    s = ms_src_new(t, 3); k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 2, k));
    CHECKF(BRK_AFTER(k->brk[1]) == want, "U+%04X followed by U+%04X: after-condition got %d want %d", last, next, BRK_AFTER(k->brk[1]), want);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_after_letter_then_letter_neutral) { after_case('a', 'b', BC_NEUTRAL); }
MT(calls_break_after_letter_then_space_neutral)  { after_case('a', ' ', BC_NEUTRAL); }
MT(calls_break_after_letter_then_lf_neutral)     { after_case('a', '\n', BC_NEUTRAL); }
MT(calls_break_after_space_then_letter_neutral)  { after_case(' ', 'b', BC_NEUTRAL); }
MT(calls_break_after_lf_then_letter_must)        { after_case('\n', 'b', BC_MUST); }
MT(calls_break_after_cr_then_letter_must)        { after_case('\r', 'b', BC_MUST); }
MT(calls_break_after_cr_then_lf_neutral)         { after_case('\r', '\n', BC_NEUTRAL); }
MT(calls_break_after_ls_then_letter_must)        { after_case(0x2028, 'b', BC_MUST); }

MT(calls_break_before_null_is_start_of_text) {
    /* NULL / length 0 from GetTextBeforePosition is "no preceding text": the first condition is MAY_NOT_BREAK like at position 0 */
    MS_BEGIN(); ms_src *s = ms_src_new(L"ab cd", 5); ms_sink *k = ms_sink_new(0);
    s->null_before = 1;
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 3, 2, k));
    EXPECT_EQ(s->n[MF_BEFORE], 1);
    EXPECT_EQ(BRK_BEFORE(k->brk[3]), BC_MAYNOT);
    ms_src_free(s); ms_sink_free(k);
    s = ms_src_new(L"ab cd", 5); k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 2, k));
    EXPECT_EQ(BRK_BEFORE(k->brk[0]), BC_MAYNOT);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_after_null_is_end_of_text) {
    /* NULL / length 0 from GetTextAtPosition(textPosition + textLength) is the end of the text: CAN_BREAK after the last unit */
    MS_BEGIN(); ms_src *s = ms_src_new(L"abcdef", 6); ms_sink *k = ms_sink_new(0);
    s->null_after = 3;
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 3, k));
    EXPECT_EQ(ms_log_count(s, MF_AT, 3), 1);
    EXPECT_EQ(BRK_AFTER(k->brk[2]), BC_CAN);
    ms_src_free(s); ms_sink_free(k);
    s = ms_src_new(L"abc\n", 4); k = ms_sink_new(0); s->null_after = 4;
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 4, k));
    EXPECT_EQ(BRK_AFTER(k->brk[3]), BC_MUST);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_before_returns_longer_text_only_last_unit_counts) {
    /* with 1-unit pieces GetTextBeforePosition returns only the previous unit; with the whole prefix it returns more: same answer */
    static const UINT32 one[] = { 1 }; MS_BEGIN(); int w;
    for (w = 0; w < 2; w++) {
        ms_src *s = ms_src_new(L"hello\nworld", 11); ms_sink *k = ms_sink_new(0);
        if (w) ms_src_cuts(s, one, 1);
        CHECK_HR_OK(ms_do(an, MA_BREAK, s, 6, 5, k));
        EXPECT_EQ(BRK_BEFORE(k->brk[6]), BC_MUST);
        EXPECT_EQ(s->n[MF_BEFORE], 1);
        ms_src_free(s); ms_sink_free(k);
    }
    MS_END();
}
MT(calls_break_piece_boundaries_extra_at_calls_noted) {
    static const UINT32 one[] = { 1 }; MS_BEGIN(); char b[512];
    ms_src *s = ms_src_new(L"abcd", 4); ms_sink *k = ms_sink_new(0);
    ms_src_cuts(s, one, 1);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 4, k));
    dump_log(s, b, sizeof b); t_note("1-unit pieces over 4 letters: %s", b);
    { int i; for (i = 0; i < s->nlog; i++) { CHECK(s->log[i].fn == MF_AT); CHECK(s->log[i].pos <= 4); CHECK(s->log[i].ret <= 1); } }
    EXPECT_EQ(s->log[0].pos, 0);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_break_all_source_positions_stay_in_text_and_ahead) {
    /* with every piece size the source is only asked for positions inside [textPosition, textPosition + textLength] (plus the Before call) */
    MS_BEGIN(); int p; UINT32 i;
    for (p = 0; p < ms_npatterns; p++) {
        ms_src *s = ms_src_new(L"ab cd ef gh", 11); ms_sink *k = ms_sink_new(0);
        ms_src_cuts(s, ms_patterns[p], 6);
        CHECK_HR_OK(ms_do(an, MA_BREAK, s, 3, 5, k));
        for (i = 0; i < (UINT32)s->nlog; i++) {
            if (s->log[i].fn == MF_AT) CHECKF(s->log[i].pos >= 3 && s->log[i].pos <= 8, "pattern %d: GetTextAtPosition(%u)", p, s->log[i].pos);
            else if (s->log[i].fn == MF_BEFORE) CHECKF(s->log[i].pos == 3, "pattern %d: GetTextBeforePosition(%u)", p, s->log[i].pos);
            else CHECKF(0, "pattern %d: unexpected source method %d", p, s->log[i].fn);
        }
        EXPECT_EQ(s->n[MF_BEFORE], 1);
        ms_src_free(s); ms_sink_free(k);
    }
    MS_END();
}

/* ---- bidi: source protocol ----------------------------------------------------------------------------------------------------- */
MT(calls_bidi_whole_text_log) {
    /* direction first, then the text, then the locale of the first run */
    MS_BEGIN(); ms_src *s = ms_src_new(L"abc", 3); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BIDI, s, 0, 3, k));
    EXPECT_LOG(s, { MF_DIR, 0 }, { MF_AT, 0 }, { MF_LOCALE, 0 });
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_bidi_subrange_log) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"abcdef", 6); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BIDI, s, 2, 3, k));
    EXPECT_LOG(s, { MF_DIR, 0 }, { MF_AT, 2 }, { MF_LOCALE, 2 });
    CHECK(ms_sink_exact(k, MK_BIDI, 2, 3));
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_bidi_direction_first_with_pieces) {
    MS_BEGIN(); int p;
    for (p = 0; p < ms_npatterns; p++) {
        ms_src *s = ms_src_new(L"ab cd ef", 8); ms_sink *k = ms_sink_new(0);
        ms_src_cuts(s, ms_patterns[p], 6);
        CHECK_HR_OK(ms_do(an, MA_BIDI, s, 0, 8, k));
        EXPECT_EQ(s->log[0].fn, MF_DIR); EXPECT_EQ(s->n[MF_DIR], 1);
        EXPECT_EQ(s->log[1].fn, MF_AT); EXPECT_EQ(s->log[1].pos, 0);
        EXPECT_EQ(s->n[MF_BEFORE], 0); EXPECT_EQ(s->n[MF_SUBST], 0); EXPECT_EQ(s->n[MF_ORIENT], 0);
        ms_src_free(s); ms_sink_free(k);
    }
    MS_END();
}
MT(calls_bidi_zero_length_no_levels) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"abc", 3); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BIDI, s, 1, 0, k));
    EXPECT_EQ(k->nev, 0); EXPECT_EQ(s->n[MF_DIR], 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
/* locale runs: one GetLocaleName per run, at the first position not covered by the previous answer */
static void locale_runs(const WCHAR *t, UINT32 pos, UINT32 len, UINT32 run, const UINT32 *want, int nwant)
{
    MS_BEGIN(); ms_src *s = ms_src_new(t, LEN(t)); ms_sink *k = ms_sink_new(0); int i, nloc = 0;
    s->locale_run = run;
    CHECK_HR_OK(ms_do(an, MA_BIDI, s, pos, len, k));
    CHECK(ms_sink_exact(k, MK_BIDI, pos, len));
    for (i = 0; i < s->nlog; i++) if (s->log[i].fn == MF_LOCALE) {
        CHECKF(nloc < nwant && s->log[i].pos == want[nloc], "locale call %d at %u", nloc, s->log[i].pos);
        nloc++;
    }
    EXPECT_EQ(nloc, nwant);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_bidi_locale_run_2)  { static const UINT32 w[] = { 0, 2, 4 }; locale_runs(L"abcdef", 0, 6, 2, w, 3); }
MT(calls_bidi_locale_run_1)  { static const UINT32 w[] = { 0, 1, 2, 3, 4, 5 }; locale_runs(L"abcdef", 0, 6, 1, w, 6); }
MT(calls_bidi_locale_run_4)  { static const UINT32 w[] = { 0, 4 }; locale_runs(L"abcdef", 0, 6, 4, w, 2); }
MT(calls_bidi_locale_run_exact) { static const UINT32 w[] = { 0 }; locale_runs(L"abcdef", 0, 6, 6, w, 1); }
MT(calls_bidi_locale_run_longer_than_range) { static const UINT32 w[] = { 1 }; locale_runs(L"abcdef", 1, 3, 100, w, 1); }
MT(calls_bidi_locale_run_subrange) { static const UINT32 w[] = { 1, 3 }; locale_runs(L"abcdef", 1, 4, 2, w, 2); }
MT(calls_bidi_locale_run_subrange_odd) { static const UINT32 w[] = { 2, 5 }; locale_runs(L"abcdefgh", 2, 5, 3, w, 2); }
MT(calls_bidi_locale_length_zero_means_rest) {
    /* a length of 0 from GetLocaleName covers the rest of the range */
    static const UINT32 w[] = { 0 }; locale_runs(L"abcdef", 0, 6, 0xffffffffu, w, 1);
}
MT(calls_bidi_locale_length_zero_same_result) {
    MS_BEGIN(); static const WCHAR t[] = { 'a', 'b', ' ', 0x05d0, 0x05d1, ' ', '1', '2', 0 }; ms_src *a = ms_src_new(t, 8), *b = ms_src_new(t, 8); ms_sink *ka = ms_sink_new(0), *kb = ms_sink_new(0); int i;
    b->locale_run = 0xffffffffu;
    CHECK_HR_OK(ms_do(an, MA_BIDI, a, 0, 8, ka)); CHECK_HR_OK(ms_do(an, MA_BIDI, b, 0, 8, kb));
    for (i = 0; i < 8; i++) { EXPECT_EQ(ka->ex[i], kb->ex[i]); EXPECT_EQ(ka->res[i], kb->res[i]); }
    ms_src_free(a); ms_src_free(b); ms_sink_free(ka); ms_sink_free(kb); MS_END();
}
static void locale_variant(const WCHAR *loc, int is_null)
{
    MS_BEGIN(); static const WCHAR t[] = { 'a', 'b', ' ', 0x05d0, 0x05d1, ' ', '1', '2', ' ', 'c', 0 };
    ms_src *a = ms_src_new(t, 10), *b = ms_src_new(t, 10); ms_sink *ka = ms_sink_new(0), *kb = ms_sink_new(0); int i;
    b->locale = is_null ? NULL : loc;
    CHECK_HR_OK(ms_do(an, MA_BIDI, a, 0, 10, ka)); CHECK_HR_OK(ms_do(an, MA_BIDI, b, 0, 10, kb));
    CHECK(ms_sink_exact(kb, MK_BIDI, 0, 10));
    for (i = 0; i < 10; i++) { EXPECT_EQ(ka->ex[i], kb->ex[i]); EXPECT_EQ(ka->res[i], kb->res[i]); }
    ms_src_free(a); ms_src_free(b); ms_sink_free(ka); ms_sink_free(kb); MS_END();
}
MT(calls_bidi_locale_null_pointer_ok)  { locale_variant(NULL, 1); }
MT(calls_bidi_locale_empty_string_ok)  { locale_variant(L"", 0); }
MT(calls_bidi_locale_he_il_same_for_latin_hebrew) { locale_variant(L"he-IL", 0); }
MT(calls_bidi_locale_de_de_same) { locale_variant(L"de-DE", 0); }
MT(calls_bidi_locale_ja_jp_same) { locale_variant(L"ja-JP", 0); }
MT(calls_bidi_locale_garbage_tag_ok) { locale_variant(L"not a locale", 0); }
MT(calls_bidi_locale_long_tag_ok) { locale_variant(L"zh-Hant-TW-u-ca-chinese-nu-hanidec-x-private", 0); }

/* ---- sink delivery ------------------------------------------------------------------------------------------------------------- */
MT(calls_sink_script_ascending_cover) {
    MS_BEGIN(); static const WCHAR t[] = { 'a', 'b', 0x0391, 0x0392, 0x0430, 0x05d0, ' ', 0x4e2d, 0 };
    ms_src *s = ms_src_new(t, 8); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_SCRIPT, s, 0, 8, k));
    CHECK(ms_runs_exact(k, MK_SCRIPT, 0, 8)); CHECK(ms_sink_exact(k, MK_SCRIPT, 0, 8));
    CHECK(k->n[MK_SCRIPT] >= 4);
    EXPECT_EQ(k->n[MK_BIDI], 0); EXPECT_EQ(k->n[MK_BREAK], 0); EXPECT_EQ(k->n[MK_SUBST], 0);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_sink_bidi_only_bidi_callbacks) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"ab cd", 5); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BIDI, s, 0, 5, k));
    EXPECT_EQ(k->n[MK_SCRIPT], 0); EXPECT_EQ(k->n[MK_BREAK], 0); EXPECT_EQ(k->n[MK_SUBST], 0); EXPECT_EQ(k->n[MK_ORIENT], 0); CHECK(k->n[MK_BIDI] >= 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_sink_break_only_break_callback) {
    MS_BEGIN(); ms_src *s = ms_src_new(L"ab cd", 5); ms_sink *k = ms_sink_new(0);
    CHECK_HR_OK(ms_do(an, MA_BREAK, s, 0, 5, k));
    EXPECT_EQ(k->n[MK_SCRIPT], 0); EXPECT_EQ(k->n[MK_BIDI], 0); EXPECT_EQ(k->n[MK_SUBST], 0); EXPECT_EQ(k->n[MK_ORIENT], 0); EXPECT_EQ(k->n[MK_BREAK], 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
/* a failing sink callback ends the walk: no further callbacks, the call fails */
static void sink_fail(int kind, int which, HRESULT hr, const WCHAR *t, UINT32 minruns)
{
    MS_BEGIN(); ms_src *s = ms_src_new(t, LEN(t)); ms_sink *k = ms_sink_new(0); HRESULT got;
    k->fail_kind = kind == MA_SCRIPT ? MK_SCRIPT : kind == MA_BIDI ? MK_BIDI : MK_BREAK; k->fail_after = which; k->fail_hr = hr;
    got = ms_do(an, kind, s, 0, LEN(t), k);
    CHECK_HR_FAILED(got);
    if (got != hr) t_note("sink returned %s, analyzer returned %s", dw_hr_name(hr), dw_hr_name(got));
    EXPECT_EQ(k->n[k->fail_kind], which);                       /* the failing call was the last one */
    ms_src_free(s); ms_sink_free(k); MS_END();
    (void)minruns;
}
MT(calls_sink_fail_break_first)  { sink_fail(MA_BREAK, 1, E_FAIL, L"ab cd", 1); }
MT(calls_sink_fail_break_abort)  { sink_fail(MA_BREAK, 1, E_ABORT, L"ab cd", 1); }
MT(calls_sink_fail_break_invalidarg) { sink_fail(MA_BREAK, 1, E_INVALIDARG, L"ab cd", 1); }
MT(calls_sink_fail_break_custom) { sink_fail(MA_BREAK, 1, (HRESULT)0x88985002, L"ab cd", 1); }
MT(calls_sink_fail_script_first) { sink_fail(MA_SCRIPT, 1, E_FAIL, L"ab\x0391\x0392 \x05d0 cd", 3); }
MT(calls_sink_fail_script_second) { sink_fail(MA_SCRIPT, 2, E_FAIL, L"ab\x0391\x0392 \x05d0 cd", 3); }
MT(calls_sink_fail_script_third) { sink_fail(MA_SCRIPT, 3, E_ABORT, L"ab\x0391\x0392 \x05d0 cd", 3); }
MT(calls_sink_fail_bidi_first)  { sink_fail(MA_BIDI, 1, E_FAIL, L"ab \x05d0\x05d1 cd \x05d2", 3); }
MT(calls_sink_fail_bidi_second) { sink_fail(MA_BIDI, 2, E_FAIL, L"ab \x05d0\x05d1 cd \x05d2", 3); }
MT(calls_sink_fail_bidi_third)  { sink_fail(MA_BIDI, 3, (HRESULT)0x88985003, L"ab \x05d0\x05d1 cd \x05d2", 3); }
MT(calls_sink_success_codes_pass) {
    /* any non-failure HRESULT from the sink lets the walk continue: S_FALSE and other success values */
    static const HRESULT ok[] = { S_FALSE, (HRESULT)1, (HRESULT)0x00040000, (HRESULT)0x7fffffff }; int i;
    MS_BEGIN();
    for (i = 0; i < 4; i++) {
        static const WCHAR t[] = { 'a', ' ', 0x05d0, ' ', 'b', 0 };
        ms_src *s = ms_src_new(t, 5); ms_sink *k = ms_sink_new(0); ms_src *s2 = ms_src_new(t, 5); ms_sink *k2 = ms_sink_new(0);
        (void)ok[i];
        CHECK_HR_OK(ms_do(an, MA_BIDI, s, 0, 5, k));
        CHECK_HR_OK(ms_do(an, MA_BIDI, s2, 0, 5, k2));
        ms_src_free(s); ms_sink_free(k); ms_src_free(s2); ms_sink_free(k2);
    }
    MS_END();
}

/* ---- vertical glyph orientation: source protocol ------------------------------------------------------------------------------- */
static void or_setup(ms_src *s, int n, const UINT32 *len, const DWRITE_VERTICAL_GLYPH_ORIENTATION *o, const UINT8 *bidi)
{
    int i; s->nor = n; for (i = 0; i < n; i++) { s->or_len[i] = len[i]; s->or_o[i] = o ? o[i] : DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT; s->or_bidi[i] = bidi ? bidi[i] : 0; }
}
MT(calls_orient_text_before_first_run_query) {
    MS_BEGIN2(); ms_src *s = ms_src_new(L"abcd", 4); ms_sink *k = ms_sink_new(1); int first_at = -1, first_or = -1, i;
    CHECK_HR_OK(ms_orient(an2, s, 0, 4, k));
    for (i = 0; i < s->nlog; i++) { if (s->log[i].fn == MF_AT && first_at < 0) first_at = i; if (s->log[i].fn == MF_ORIENT && first_or < 0) first_or = i; }
    CHECK(first_at >= 0 && first_or >= 0 && first_at < first_or);
    EXPECT_EQ(s->n[MF_DIR], 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_run_queries_at_run_starts) {
    MS_BEGIN2(); static const UINT32 len[] = { 2, 3, 1 }; ms_src *s = ms_src_new(L"abcdefgh", 8); ms_sink *k = ms_sink_new(1); int i; static const UINT32 want[] = { 0, 2, 5, 6 };
    or_setup(s, 3, len, NULL, NULL);
    CHECK_HR_OK(ms_orient(an2, s, 0, 8, k));
    { int c = 0; for (i = 0; i < s->nlog; i++) if (s->log[i].fn == MF_ORIENT) { CHECKF(c < 4 && s->log[i].pos == want[c], "orientation query %d at %u", c, s->log[i].pos); c++; } EXPECT_EQ(c, 4); }
    EXPECT_EQ(k->n[MK_ORIENT], 1);          /* identical information coalesces */
    CHECK(ms_sink_exact(k, MK_ORIENT, 0, 8));
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_zero_length_run_means_rest) {
    MS_BEGIN2(); static const UINT32 len[] = { 0 }; ms_src *s = ms_src_new(L"abcdefgh", 8); ms_sink *k = ms_sink_new(1);
    or_setup(s, 1, len, NULL, NULL);
    CHECK_HR_OK(ms_orient(an2, s, 0, 8, k));
    EXPECT_EQ(s->n[MF_ORIENT], 1); EXPECT_EQ(k->n[MK_ORIENT], 1); CHECK(ms_sink_exact(k, MK_ORIENT, 0, 8));
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_run_longer_than_range_clipped) {
    MS_BEGIN2(); static const UINT32 len[] = { 100 }; ms_src *s = ms_src_new(L"abcdefgh", 8); ms_sink *k = ms_sink_new(1);
    or_setup(s, 1, len, NULL, NULL);
    CHECK_HR_OK(ms_orient(an2, s, 2, 3, k));
    EXPECT_EQ(s->n[MF_ORIENT], 1); EXPECT_EQ(k->n[MK_ORIENT], 1);
    CHECK(ms_sink_exact(k, MK_ORIENT, 2, 3)); EXPECT_EQ(k->ev[0].pos, 2); EXPECT_EQ(k->ev[0].len, 3);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_level_change_splits_sink_runs) {
    /* the adjusted bidi level is part of the information: runs with different levels are delivered separately */
    MS_BEGIN2(); static const UINT32 len[] = { 2, 2, 2, 2 }; static const UINT8 lv[] = { 0, 0, 2, 2 }; ms_src *s = ms_src_new(L"abcdefgh", 8); ms_sink *k = ms_sink_new(1);
    or_setup(s, 4, len, NULL, lv);
    CHECK_HR_OK(ms_orient(an2, s, 0, 8, k));
    EXPECT_EQ(k->n[MK_ORIENT], 2);
    EXPECT_EQ(k->ev[0].pos, 0); EXPECT_EQ(k->ev[0].len, 4); EXPECT_EQ(k->ev[1].pos, 4); EXPECT_EQ(k->ev[1].len, 4);
    EXPECT_EQ(k->ev[0].abl, 0); EXPECT_EQ(k->ev[1].abl, 2);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_alternating_levels_one_run_each) {
    MS_BEGIN2(); static const UINT32 len[] = { 1, 1, 1, 1, 1, 1 }; static const UINT8 lv[] = { 0, 2, 0, 2, 0, 2 }; ms_src *s = ms_src_new(L"abcdef", 6); ms_sink *k = ms_sink_new(1); int i;
    or_setup(s, 6, len, NULL, lv);
    CHECK_HR_OK(ms_orient(an2, s, 0, 6, k));
    EXPECT_EQ(k->n[MK_ORIENT], 6);
    for (i = 0; i < k->nev && i < 6; i++) { EXPECT_EQ(k->ev[i].pos, i); EXPECT_EQ(k->ev[i].len, 1); EXPECT_EQ(k->ev[i].abl, (i & 1) ? 2 : 0); }
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_mode_change_splits_default_stacked) {
    MS_BEGIN2(); static const UINT32 len[] = { 3, 3 }; static const DWRITE_VERTICAL_GLYPH_ORIENTATION o[] = { DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED };
    ms_src *s = ms_src_new(L"abcdef", 6); ms_sink *k = ms_sink_new(1);
    or_setup(s, 2, len, o, NULL);
    CHECK_HR_OK(ms_orient(an2, s, 0, 6, k));
    /* Latin text: default = rotated (angle 0, sideways), stacked = upright at 90 degrees: two runs */
    EXPECT_EQ(k->n[MK_ORIENT], 2);
    CHECK(k->sw[0]); CHECK(!k->sw[3]);
    EXPECT_EQ(k->ang[0], DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES); EXPECT_EQ(k->ang[3], DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_subrange_positions_absolute) {
    MS_BEGIN2(); ms_src *s = ms_src_new(L"abcdefgh", 8); ms_sink *k = ms_sink_new(1);
    CHECK_HR_OK(ms_orient(an2, s, 3, 4, k));
    EXPECT_EQ(k->n[MK_ORIENT], 1); EXPECT_EQ(k->ev[0].pos, 3); EXPECT_EQ(k->ev[0].len, 4); EXPECT_EQ(s->log[0].fn == MF_DIR || s->log[0].fn == MF_AT, 1);
    CHECK(ms_sink_exact(k, MK_ORIENT, 3, 4));
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_sink_failure_stops_walk) {
    MS_BEGIN2(); static const UINT32 len[] = { 1, 1, 1, 1 }; static const UINT8 lv[] = { 0, 2, 0, 2 }; ms_src *s = ms_src_new(L"abcd", 4); ms_sink *k = ms_sink_new(1); HRESULT hr;
    or_setup(s, 4, len, NULL, lv);
    k->fail_kind = MK_ORIENT; k->fail_after = 2; k->fail_hr = E_FAIL;
    hr = ms_orient(an2, s, 0, 4, k);
    CHECK_HR_FAILED(hr);
    EXPECT_EQ(k->n[MK_ORIENT], 2);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_source_failure_surfaces) {
    MS_BEGIN2(); ms_src *s = ms_src_new(L"abcd", 4); ms_sink *k = ms_sink_new(1); HRESULT hr;
    s->fail_orient = E_FAIL;
    hr = ms_orient(an2, s, 0, 4, k);
    CHECK_HR_FAILED(hr); EXPECT_EQ(k->n[MK_ORIENT], 0); EXPECT_EQ(s->n[MF_ORIENT], 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(calls_orient_zero_length_only_direction) {
    MS_BEGIN2(); ms_src *s = ms_src_new(L"abcd", 4); ms_sink *k = ms_sink_new(1);
    CHECK_HR_OK(ms_orient(an2, s, 1, 0, k));
    EXPECT_EQ(k->nev, 0); EXPECT_EQ(s->n[MF_ORIENT], 0); EXPECT_EQ(s->n[MF_AT], 0);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
