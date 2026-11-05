/* IDWriteTextAnalyzer::AnalyzeBidi: SetBidiLevel(textPosition, textLength, explicitLevel, resolvedLevel).
 *
 * explicitLevel is the level after the explicit rules (paragraph level, embeddings, overrides); resolvedLevel the level after the
 * weak/neutral/implicit rules.  The paragraph level comes from the source's GetParagraphReadingDirection (called exactly once per
 * AnalyzeBidi call); the analyzer also asks the source for GetLocaleName (the language selects how European digits following Arabic
 * letters are resolved).  Reported runs are ascending, contiguous and cover the requested range. */
#include "ta.h"

#define RLM 0x200F
#define LRM 0x200E
#define LRE 0x202A
#define RLE 0x202B
#define PDF 0x202C
#define LRO 0x202D
#define RLO 0x202E
#define LRI 0x2066
#define RLI 0x2067
#define FSI 0x2068
#define PDI 0x2069

/* Expected levels are given as strings of digits, one per UTF-16 unit ('-' = do not check that position). */
static void levels(const WCHAR *t, DWRITE_READING_DIRECTION dir, const char *explicit_, const char *resolved, const char *loc)
{
    TA_BEGIN();
    UINT32 n = LEN(t), i;
    dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
    s->dir = dir;
    if (loc) lstrcpyW(s->locale, (const WCHAR *)loc);
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, n, k));
    CHECK(ta_runs_exact(k, 'l', 0, n));
    CHECK(dwx_ana_covered(k, 'l', n));
    CHECK(!k->overlap);
    for (i = 0; i < n && i < DWX_ANA_MAX; i++) {
        if (explicit_ && explicit_[i] != '-') CHECKF(k->bidi_explicit[i] == explicit_[i] - '0', "pos %u explicit level %u want %d", i, k->bidi_explicit[i], explicit_[i] - '0');
        if (resolved && resolved[i] != '-') CHECKF(k->bidi_resolved[i] == resolved[i] - '0', "pos %u resolved level %u want %d", i, k->bidi_resolved[i], resolved[i] - '0');
    }
    ta_free(s, k); TA_END();
}
#define LTR DWRITE_READING_DIRECTION_LEFT_TO_RIGHT
#define RTL DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
#define BIDI_TEST(nm, dir, text, ex, rs) TEST(bidi_##nm) { levels(text, dir, ex, rs, NULL); }

/* ---- paragraph level ------------------------------------------------------------------------------------------------------------- */
BIDI_TEST(ltr_latin,        LTR, L"abc",                    "000",   "000")
BIDI_TEST(ltr_latin_spaces, LTR, L"ab cd",                  "00000", "00000")
BIDI_TEST(ltr_digits,       LTR, L"a12b",                   "0000",  "0000")
BIDI_TEST(ltr_single,       LTR, L"a",                      "0",     "0")
BIDI_TEST(rtl_latin,        RTL, L"abc",                    "111",   "222")
BIDI_TEST(rtl_hebrew,       RTL, L"\x05d0\x05d1\x05d2",      "111",   "111")
BIDI_TEST(rtl_arabic,       RTL, L"\x0627\x0628\x0629",      "111",   "111")
BIDI_TEST(rtl_spaces_only,  RTL, L"   ",                    "111",   "111")
BIDI_TEST(ltr_spaces_only,  LTR, L"   ",                    "000",   "000")
BIDI_TEST(rtl_digits_only,  RTL, L"123",                    "111",   "222")
BIDI_TEST(ltr_hebrew,       LTR, L"\x05d0\x05d1\x05d2",      "000",   "111")
BIDI_TEST(ltr_arabic,       LTR, L"\x0627\x0628\x0629",      "000",   "111")

/* ---- mixed ----------------------------------------------------------------------------------------------------------------------- */
BIDI_TEST(ltr_l_r_l,        LTR, L"abc \x05d0\x05d1\x05d2 def", "00000000000", "00001110000")
BIDI_TEST(ltr_r_between_l,  LTR, L"a\x05d0" L"b",              "000",   "010")
BIDI_TEST(ltr_neutral_between_r, LTR, L"\x05d0 \x05d1",         "000",   "111")
BIDI_TEST(ltr_trailing_neutral_after_r, LTR, L"\x05d0.",        "00",    "10")
BIDI_TEST(ltr_leading_neutral_before_r, LTR, L".\x05d0",        "00",    "01")
BIDI_TEST(rtl_r_l_r,        RTL, L"\x05d0 abc \x05d1",          "1111111", "1122211")
BIDI_TEST(rtl_trailing_neutral_after_l, RTL, L"abc.",           "1111",  "2221")
BIDI_TEST(rtl_leading_neutral_before_l, RTL, L".abc",           "1111",  "1222")
BIDI_TEST(rtl_neutral_between_l, RTL, L"a b",                   "111",   "222")
BIDI_TEST(rtl_neutral_between_l_and_r, RTL, L"a \x05d0",        "111",   "211")
BIDI_TEST(ltr_arabic_then_latin, LTR, L"\x0627\x0628 ab",       "00000", "11000")
BIDI_TEST(ltr_two_hebrew_words, LTR, L"\x05d0\x05d1 \x05d2\x05d3", "00000", "11111")

/* ---- numbers --------------------------------------------------------------------------------------------------------------------- */
BIDI_TEST(ltr_hebrew_digits,     LTR, L"\x05d0 123 \x05d1",     "0000000", "1122211")
BIDI_TEST(ltr_hebrew_digits_end, LTR, L"\x05d0 123",            "00000",   "11222")
BIDI_TEST(ltr_digits_then_hebrew, LTR, L"123 \x05d0",           "00000",   "00001")
BIDI_TEST(rtl_hebrew_digits,     RTL, L"\x05d0 123 \x05d1",     "1111111", "1122211")
BIDI_TEST(rtl_digits_in_latin,   RTL, L"abc 123",               "1111111", "2222222")
BIDI_TEST(ltr_arabic_digits,     LTR, L"\x0627 123 \x0628",     "0000000", "1122211")
BIDI_TEST(rtl_arabic_digits,     RTL, L"\x0627 123",            "11111",   "11222")
BIDI_TEST(ltr_number_separator,  LTR, L"\x05d0 1,234.5",        "000000000", "112222222")
BIDI_TEST(ltr_percent_after_number, LTR, L"\x05d0 50%",         "00000",   "11222")
BIDI_TEST(ltr_currency_before_number, LTR, L"\x05d0 $50",       "00000",   "11222")
BIDI_TEST(ltr_plus_minus,        LTR, L"\x05d0 -5",             "0000",    "1112")

/* ---- Arabic digit handling depends on the language of the run ------------------------------------------------------------------- */
TEST(bidi_arabic_digits_by_language_note)
{
    static const WCHAR t[] = L"\x0627 123";
    static const WCHAR *const loc[] = { L"en-us", L"ar-sa", L"ar-ma", L"ar-eg", L"fa-ir", L"he-il", L"", L"ar" };
    TA_BEGIN();
    int i;
    for (i = 0; i < (int)(sizeof loc / sizeof loc[0]); i++) {
        dwx_ana_src *s = dwx_ana_src_new(t, 5); dwx_ana_sink *k = dwx_ana_sink_new();
        lstrcpyW(s->locale, loc[i]);
        CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, 5, k));
        CHECK(ta_runs_exact(k, 'l', 0, 5));
        t_note("locale \"%ls\": resolved %u%u%u%u%u", loc[i], k->bidi_resolved[0], k->bidi_resolved[1], k->bidi_resolved[2], k->bidi_resolved[3], k->bidi_resolved[4]);
        /* the Arabic letter itself and the explicit levels do not depend on the language */
        CHECK(k->bidi_resolved[0] == 1);
        CHECK(k->bidi_explicit[0] == 0 && k->bidi_explicit[4] == 0);
        ta_free(s, k);
    }
    TA_END();
}
TEST(bidi_locale_requested_at_position)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x05d0\x05d1 def";
    dwx_ana_src *s = dwx_ana_src_new(t, LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, LEN(t), k));
    CHECK(s->n_locale >= 1);
    if (s->n_locale >= 1) { int i; for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 4) { CHECK(s->log[i].pos == 0); break; } }
    ta_free(s, k);
    s = dwx_ana_src_new(t, LEN(t)); k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 4, 3, k));
    { int i; for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 4) { CHECK(s->log[i].pos == 4); break; } }
    ta_free(s, k);
    TA_END();
}
TEST(bidi_locale_run_short_chunks)
{
    /* the locale run length returned by the source is respected: asking again at the end of each run */
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x05d0\x05d1 def";
    dwx_ana_src *s = dwx_ana_src_new(t, LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new(); int i; unsigned prev = 0;
    s->locale_run = 2;
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, LEN(t), k));
    CHECK(s->n_locale >= 4);
    for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 4) { CHECK(s->log[i].pos >= prev); prev = s->log[i].pos; }
    CHECK(ta_runs_exact(k, 'l', 0, LEN(t)));
    ta_free(s, k); TA_END();
}
TEST(bidi_locale_error_propagates)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc";
    dwx_ana_src *s = dwx_ana_src_new(t, 3); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    s->fail_locale = E_FAIL;
    hr = ta_do(an, TA_BIDI, s, 0, 3, k);
    CHECK_HR_PROP(hr, E_FAIL);
    CHECK(k->n_bidi == 0);
    ta_free(s, k); TA_END();
}

/* ---- explicit embeddings and overrides ------------------------------------------------------------------------------------------- */
#define E_(a) a
BIDI_TEST(ltr_rle_latin,   LTR, L"a\x202b" L"b\x202c" L"c",   "0-1-0", "0-2-0")
BIDI_TEST(ltr_rle_hebrew,  LTR, L"a\x202b\x05d0\x202c" L"c",  "0-1-0", "0-1-0")
BIDI_TEST(ltr_lre_latin,   LTR, L"a\x202a" L"b\x202c" L"c",   "0-2-0", "0-2-0")
BIDI_TEST(rtl_lre_latin,   RTL, L"\x05d0\x202a" L"b\x202c\x05d1", "1-2-1", "1-2-1")
BIDI_TEST(rtl_rle_hebrew,  RTL, L"\x05d0\x202b\x05d1\x202c\x05d2", "1-3-1", "1-3-1")
BIDI_TEST(ltr_rlo_latin,   LTR, L"a\x202e" L"bc\x202c" L"d",  "0-11-0", "0-11-0")
BIDI_TEST(ltr_lro_hebrew,  LTR, L"a\x202d\x05d0\x05d1\x202c" L"d", "0-22-0", "0-22-0")
BIDI_TEST(rtl_rlo_latin,   RTL, L"\x202e" L"ab\x202c",        "-33-",  "-33-")
BIDI_TEST(ltr_nested_rle_lre, LTR, L"\x202b\x202a" L"a\x202c\x202c", "--2--", "--2--")
BIDI_TEST(ltr_nested_rle_rle, LTR, L"\x202b\x202b\x05d0\x202c\x202c", "--3--", "--3--")
BIDI_TEST(ltr_rle_digits,  LTR, L"\x202b" L"12\x202c",        "-11-",  "-22-")
BIDI_TEST(ltr_unmatched_pdf, LTR, L"a\x202c" L"b",            "0-0",   "0-0")
BIDI_TEST(ltr_unterminated_rle, LTR, L"a\x202b" L"bc",        "0-11",  "0-22")
BIDI_TEST(ltr_rle_neutrals, LTR, L"\x202b a \x202c",          "-111-", "-121-")

/* Level of the control characters themselves and isolates (Unicode 6.3) is recorded; the controlled text is checked. */
TEST(bidi_control_char_levels_note)
{
    TA_BEGIN();
    static const WCHAR t[] = L"a\x202b" L"b\x202c" L"c";
    dwx_ana_src *s = dwx_ana_src_new(t, 5); dwx_ana_sink *k = dwx_ana_sink_new(); int i;
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, 5, k));
    for (i = 0; i < 5; i++) t_note("RLE/PDF text pos %d: explicit %u resolved %u", i, k->bidi_explicit[i], k->bidi_resolved[i]);
    ta_free(s, k); TA_END();
}
TEST(bidi_isolates_note)
{
    static const struct { const char *n; WCHAR t[8]; } rows[] = {
        { "LRI x PDI", { 'a', LRI, 'b', PDI, 'c', 0 } }, { "RLI x PDI", { 'a', RLI, 'b', PDI, 'c', 0 } },
        { "RLI hebrew PDI", { 'a', RLI, 0x5d0, PDI, 'c', 0 } }, { "FSI hebrew PDI", { 'a', FSI, 0x5d0, PDI, 'c', 0 } },
        { "FSI latin PDI", { 0x5d0, FSI, 'a', PDI, 0x5d1, 0 } }, { "unmatched PDI", { 'a', PDI, 'b', 0 } },
        { "unterminated RLI", { 'a', RLI, 'b', 0 } },
    };
    TA_BEGIN();
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        UINT32 n = LEN(rows[i].t), j; char buf[64]; int o = 0;
        dwx_ana_src *s = dwx_ana_src_new(rows[i].t, n); dwx_ana_sink *k = dwx_ana_sink_new();
        CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, n, k));
        CHECK(ta_runs_exact(k, 'l', 0, n));
        for (j = 0; j < n && o < 50; j++) o += snprintf(buf + o, sizeof buf - o, "%u/%u ", k->bidi_explicit[j], k->bidi_resolved[j]);
        t_note("%s: explicit/resolved %s", rows[i].n, buf);
        ta_free(s, k);
    }
    TA_END();
}
TEST(bidi_marks_lrm_rlm)
{
    /* LRM/RLM are strong characters of the matching direction */
    levels(L"\x05d0\x200e" L"\x05d1", LTR, "000", "101", NULL);
    levels(L"a\x200f" L"b", RTL, "111", "212", NULL);
}
BIDI_TEST(ltr_rlm_trailing, LTR, L"abc\x200f",       "0000", "0001")
BIDI_TEST(rtl_lrm_trailing, RTL, L"\x05d0\x200e",    "11",   "12")

/* ---- paired brackets and neutrals ------------------------------------------------------------------------------------------------ */
BIDI_TEST(rtl_parens_around_latin, RTL, L"(abc)",      "11111", "12221")
BIDI_TEST(ltr_parens_around_hebrew, LTR, L"(\x05d0\x05d1)", "0000", "0110")
BIDI_TEST(ltr_hebrew_in_latin_parens, LTR, L"a(\x05d0)b", "00000", "00100")
BIDI_TEST(ltr_neutral_run_resolved_by_embedding, LTR, L"\x05d0 !? \x05d1", "000000", "111111")

/* ---- sub-ranges and structure ---------------------------------------------------------------------------------------------------- */
TEST(bidi_subrange_positions_absolute)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x05d0\x05d1\x05d2 def";
    dwx_ana_src *s = dwx_ana_src_new(t, LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new(); UINT32 i;
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 4, 3, k));
    CHECK(ta_runs_exact(k, 'l', 4, 3));
    for (i = 0; i < 4; i++) CHECK(!k->bidi_set[i]);
    for (i = 4; i < 7; i++) CHECK(k->bidi_set[i] && k->bidi_resolved[i] == 1 && k->bidi_explicit[i] == 0);
    CHECK(!k->bidi_set[7]);
    ta_free(s, k); TA_END();
}
TEST(bidi_subrange_runs_cover_exact)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x05d0\x05d1\x05d2 def \x0627\x0628";
    UINT32 p, l, n = LEN(t);
    for (p = 0; p < n; p += 3) for (l = 1; p + l <= n; l += 4) {
        dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
        HRESULT hr = ta_do(an, TA_BIDI, s, p, l, k);
        CHECKF(hr == S_OK && ta_runs_exact(k, 'l', p, l) && !k->overlap, "pos %u len %u: %s", p, l, dw_hr_name(hr));
        ta_free(s, k);
    }
    TA_END();
}
TEST(bidi_uniform_text_is_one_run_note)
{
    TA_BEGIN();
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_BIDI, L"abcdef", &s, &k));
    t_note("uniform LTR text -> %ld SetBidiLevel call(s)", k->n_bidi);
    CHECK(ta_runs_exact(k, 'l', 0, 6));
    ta_free(s, k);
    CHECK_HR_OK(ta_run(an, TA_BIDI, L"\x05d0\x05d1\x05d2", &s, &k));
    t_note("uniform RTL text -> %ld SetBidiLevel call(s)", k->n_bidi);
    CHECK(ta_runs_exact(k, 'l', 0, 3));
    ta_free(s, k);
    TA_END();
}
TEST(bidi_runs_split_at_level_changes)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x05d0\x05d1\x05d2 def";
    dwx_ana_src *s; dwx_ana_sink *k; int i, changes = 0;
    CHECK_HR_OK(ta_run(an, TA_BIDI, t, &s, &k));
    for (i = 1; i < (int)LEN(t); i++) if (k->bidi_resolved[i] != k->bidi_resolved[i - 1]) changes++;
    CHECK(k->n_bidi >= changes + 1);
    /* no run spans two different level pairs */
    for (i = 0; i < k->nrun_bidi && i < 256; i++) {
        UINT32 p = k->bidi_runs[i].pos, l = k->bidi_runs[i].len, j;
        for (j = 1; j < l; j++) CHECK(k->bidi_resolved[p + j] == k->bidi_resolved[p] && k->bidi_explicit[p + j] == k->bidi_explicit[p]);
    }
    ta_free(s, k); TA_END();
}
TEST(bidi_surrogate_pair_same_level)
{
    TA_BEGIN();
    static const WCHAR t1[] = L"\x05d0\xd802\xdd00\x05d1";    /* U+10900 PHOENICIAN LETTER ALF (R) */
    static const WCHAR t2[] = L"a\xd83d\xde00" L"b";            /* U+1F600 */
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_BIDI, t1, &s, &k));
    CHECK(k->bidi_resolved[1] == k->bidi_resolved[2] && k->bidi_explicit[1] == k->bidi_explicit[2]);
    CHECK(k->bidi_resolved[1] == 1);
    ta_free(s, k);
    CHECK_HR_OK(ta_run(an, TA_BIDI, t2, &s, &k));
    CHECK(k->bidi_resolved[1] == k->bidi_resolved[2]);
    ta_free(s, k);
    TA_END();
}
TEST(bidi_unreported_outside_range)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc";
    dwx_ana_src *s = dwx_ana_src_new(t, 3); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 1, 1, k));
    CHECK(!k->bidi_set[0] && k->bidi_set[1] && !k->bidi_set[2]);
    ta_free(s, k); TA_END();
}
TEST(bidi_levels_bounded)
{
    /* explicit levels never exceed 125 (UAX #9 max_depth) and resolved <= 126 */
    TA_BEGIN();
    static WCHAR t[300]; int i, n = 0;
    dwx_ana_src *s; dwx_ana_sink *k;
    for (i = 0; i < 140; i++) t[n++] = RLE;
    t[n++] = 'a';
    s = dwx_ana_src_new(t, n); k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, n, k));
    CHECK(ta_runs_exact(k, 'l', 0, n));
    for (i = 0; i < n; i++) CHECKF(k->bidi_explicit[i] <= 125 && k->bidi_resolved[i] <= 126, "pos %d levels %u/%u", i, k->bidi_explicit[i], k->bidi_resolved[i]);
    ta_free(s, k); TA_END();
}
TEST(bidi_reading_direction_values)
{
    /* all four enumerators are accepted as the paragraph direction; vertical ones are recorded */
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x05d0\x05d1";
    int d;
    for (d = 0; d <= 3; d++) {
        dwx_ana_src *s = dwx_ana_src_new(t, 6); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
        s->dir = (DWRITE_READING_DIRECTION)d;
        hr = ta_do(an, TA_BIDI, s, 0, 6, k);
        CHECKF(hr == S_OK, "direction %d: %s", d, dw_hr_name(hr));
        if (hr == S_OK) { CHECK(ta_runs_exact(k, 'l', 0, 6)); t_note("direction %d: levels %u %u %u %u %u %u", d, k->bidi_resolved[0], k->bidi_resolved[1], k->bidi_resolved[2], k->bidi_resolved[3], k->bidi_resolved[4], k->bidi_resolved[5]); }
        ta_free(s, k);
    }
    TA_END();
}
TEST(bidi_reading_direction_invalid)
{
    /* an enumerator outside 0..3 makes the analysis fail before the sink is touched */
    TA_BEGIN();
    static const WCHAR t[] = L"abc";
    static const int bad[] = { 4, 5, 100, -1 };
    int i;
    for (i = 0; i < 4; i++) {
        dwx_ana_src *s = dwx_ana_src_new(t, 3); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
        s->dir = (DWRITE_READING_DIRECTION)bad[i];
        hr = ta_do(an, TA_BIDI, s, 0, 3, k);
        CHECKF(FAILED(hr), "direction %d: %s", bad[i], dw_hr_name(hr));
        if (FAILED(hr)) t_note("direction %d -> %s", bad[i], dw_hr_name(hr));
        CHECK(k->n_bidi == 0);
        ta_free(s, k);
    }
    TA_END();
}
TEST(bidi_direction_top_to_bottom_like_ltr)
{
    /* TOP_TO_BOTTOM selects the left-to-right paragraph level, BOTTOM_TO_TOP the right-to-left one */
    TA_BEGIN();
    static const WCHAR t[] = L"abc";
    int d;
    for (d = 2; d <= 3; d++) {
        dwx_ana_src *s = dwx_ana_src_new(t, 3); dwx_ana_sink *k = dwx_ana_sink_new();
        s->dir = (DWRITE_READING_DIRECTION)d;
        CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, 3, k));
        CHECKF(k->bidi_explicit[0] == (d == 2 ? 0 : 1), "direction %d explicit %u", d, k->bidi_explicit[0]);
        ta_free(s, k);
    }
    TA_END();
}
TEST(bidi_sink_error_propagates)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x05d0\x05d1 def";
    dwx_ana_src *s = dwx_ana_src_new(t, LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    k->fail_bidi = E_OUTOFMEMORY;
    hr = ta_do(an, TA_BIDI, s, 0, LEN(t), k);
    CHECK_HR_PROP(hr, E_OUTOFMEMORY);
    CHECK(k->n_bidi == 1);
    ta_free(s, k); TA_END();
}
TEST(bidi_source_error_propagates)
{
    TA_BEGIN();
    ta_fsrc fs; dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    static const WCHAR t[] = L"abc";
    if (t_is_wine()) { ta_free(NULL, k); TA_END(); t_skip("emulator faults on source errors"); return; }
    ta_fsrc_init(&fs, t, 3); fs.fail_at = E_FAIL;
    hr = IDWriteTextAnalyzer_AnalyzeBidi(an, (IDWriteTextAnalysisSource *)&fs, 0, 3, (IDWriteTextAnalysisSink *)k);
    CHECK_HR_PROP(hr, E_FAIL);
    CHECK(k->n_bidi == 0);
    ta_free(NULL, k); TA_END();
}
TEST(bidi_huge)
{
    TA_BEGIN();
    UINT32 n = 50000, i; WCHAR *t = (WCHAR *)malloc(n * sizeof(WCHAR));
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!t) { TA_END(); t_skip("alloc"); return; }
    for (i = 0; i < n; i++) t[i] = (WCHAR)(((i / 10) & 1) ? 0x5d0 : 'a');
    s = dwx_ana_src_new(t, n); k = dwx_ana_sink_new(); s->chunk = 1000;
    CHECK_HR_OK(ta_do(an, TA_BIDI, s, 0, n, k));
    CHECK(k->n_bidi >= 5000);
    CHECK(!k->overlap);
    ta_free(s, k); free(t); TA_END();
}

/* ====================================================================================================================================
 * AnalyzeNumberSubstitution
 * The sink is called only for runs of European digits (U+0030..0039, with embedded ',' '.' between digits and a trailing '%') for which
 * the substitution object returned by the source applies: NATIONAL and TRADITIONAL always, NONE and a NULL object never,
 * CONTEXTUAL depending on the preceding script.  The sink receives the very object the source returned. */
typedef struct { UINT32 pos, len; } rng;
static IDWriteNumberSubstitution *mk_sub(IDWriteFactory *f, DWRITE_NUMBER_SUBSTITUTION_METHOD m, const WCHAR *loc)
{
    IDWriteNumberSubstitution *ns = NULL;
    if (FAILED(IDWriteFactory_CreateNumberSubstitution(f, m, loc, FALSE, &ns))) return NULL;
    return ns;
}
/* Collects (pos,len) of every SetNumberSubstitution call by scanning set positions in the sink. */
static int sub_ranges(const dwx_ana_sink *k, UINT32 n, rng *out, int cap)
{
    UINT32 i; int c = 0;
    for (i = 0; i < n && i < DWX_ANA_MAX; i++) {
        if (!k->subst_set[i]) continue;
        if (i > 0 && k->subst_set[i - 1] && k->subst[i - 1] == k->subst[i]) { out[c - 1].len++; continue; }
        if (c < cap) { out[c].pos = i; out[c].len = 1; c++; }
    }
    return c;
}
static void ns_expect(DWRITE_NUMBER_SUBSTITUTION_METHOD m, const WCHAR *loc, const WCHAR *t, const rng *e, int ne)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, m, loc);
    UINT32 n = LEN(t); rng got[16]; int c, i;
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s = dwx_ana_src_new(t, n); k = dwx_ana_sink_new(); s->subst = ns;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, n, k));
    c = sub_ranges(k, n, got, 16);
    CHECKF(c == ne, "%d substituted ranges, want %d", c, ne);
    for (i = 0; i < c && i < ne; i++) {
        CHECKF(got[i].pos == e[i].pos && got[i].len == e[i].len, "range %d is (%u,%u), want (%u,%u)", i, got[i].pos, got[i].len, e[i].pos, e[i].len);
        CHECK(k->subst[got[i].pos] == ns);
    }
    CHECK(!k->overflow);
    CHECKF(dwx_refs(s) == 1, "source refs %ld", (long)dwx_refs(s));
    ta_free(s, k);
    EXPECT_REFS(ns, 1);
    DW_RELEASE(ns); TA_END();
}
#define NS_TEST(nm, m, loc, text, ...) TEST(numsub_##nm) { static const rng e[] = { __VA_ARGS__ }; ns_expect(m, loc, text, e, (int)(sizeof e / sizeof e[0])); }
#define NATIONAL DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL
#define TRADITIONAL DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL
#define NSNONE DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE

NS_TEST(national_digits_only,   NATIONAL, L"ar-sa", L"123",              { 0, 3 })
NS_TEST(national_in_text,       NATIONAL, L"ar-sa", L"abc 123 def",      { 4, 3 })
NS_TEST(national_two_numbers,   NATIONAL, L"ar-sa", L"1 22 333",         { 0, 1 }, { 2, 2 }, { 5, 3 })
NS_TEST(national_no_digits,     NATIONAL, L"ar-sa", L"abc def")
NS_TEST(national_decimal,       NATIONAL, L"ar-sa", L"a 1.5 b",          { 2, 3 })
NS_TEST(national_thousands,     NATIONAL, L"ar-sa", L"x1,234.56y",       { 1, 8 })
NS_TEST(national_trailing_dot,  NATIONAL, L"ar-sa", L"12. a",            { 0, 2 })
NS_TEST(national_trailing_comma, NATIONAL, L"ar-sa", L"12, a",           { 0, 2 })
NS_TEST(national_leading_dot,   NATIONAL, L"ar-sa", L".5",               { 1, 1 })
NS_TEST(national_percent,       NATIONAL, L"ar-sa", L"50%",              { 0, 3 })
NS_TEST(national_double_percent, NATIONAL, L"ar-sa", L"50%%",            { 0, 3 })
NS_TEST(national_minus_excluded, NATIONAL, L"ar-sa", L"-5",              { 1, 1 })
NS_TEST(national_colon_splits,  NATIONAL, L"ar-sa", L"12:34",            { 0, 2 }, { 3, 2 })
NS_TEST(national_double_dot,    NATIONAL, L"ar-sa", L"1..2",             { 0, 1 }, { 3, 1 })
NS_TEST(national_at_end,        NATIONAL, L"ar-sa", L"abc7",             { 3, 1 })
NS_TEST(national_at_start,      NATIONAL, L"ar-sa", L"7abc",             { 0, 1 })
NS_TEST(national_fullwidth_digits_not_matched, NATIONAL, L"ar-sa", L"\xff11\xff12")
NS_TEST(national_arabic_indic_not_matched, NATIONAL, L"ar-sa", L"\x0661\x0662")
/* An en-us locale (nominal digits) is treated as METHOD_NONE: nothing is substituted; other Latin-digit locales still substitute. */
NS_TEST(national_en_locale_is_nominal, NATIONAL, L"en-us", L"4 5")
NS_TEST(national_de_locale_substitutes, NATIONAL, L"de-de", L"4 5", { 0, 1 }, { 2, 1 })
NS_TEST(national_fa_locale,     NATIONAL, L"fa-ir", L"2024",             { 0, 4 })
NS_TEST(none_digits,            NSNONE, L"ar-sa", L"abc 123")
NS_TEST(none_en,                NSNONE, L"en-us", L"123")

TEST(numsub_null_object_no_calls)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc 123";
    dwx_ana_src *s = dwx_ana_src_new(t, 7); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, 7, k));
    CHECK(k->n_subst == 0);
    CHECK(s->n_subst >= 1);
    ta_free(s, k); TA_END();
}
TEST(numsub_source_called_at_position)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc 123 456";
    dwx_ana_src *s = dwx_ana_src_new(t, 11); dwx_ana_sink *k = dwx_ana_sink_new(); int i;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 4, 7, k));
    for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 5) { CHECK(s->log[i].pos == 4); break; }
    ta_free(s, k); TA_END();
}
TEST(numsub_run_boundaries_from_source)
{
    /* the substitution run length returned by the source splits a digit run into separate sink calls */
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, NATIONAL, L"ar-sa");
    static const WCHAR t[] = L"123456";
    dwx_ana_src *s; dwx_ana_sink *k; rng got[16]; int c;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s = dwx_ana_src_new(t, 6); k = dwx_ana_sink_new(); s->subst = ns; s->subst_run = 2;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, 6, k));
    CHECK(k->n_subst == 3);
    c = sub_ranges(k, 6, got, 16);
    CHECK(c == 1);
    CHECK(s->n_subst >= 3);
    ta_free(s, k); DW_RELEASE(ns); TA_END();
}
TEST(numsub_two_objects)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *a = mk_sub(f, NATIONAL, L"ar-sa"), *b = mk_sub(f, TRADITIONAL, L"hi-in");
    static const WCHAR t[] = L"12 34";
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s = dwx_ana_src_new(t, 5); k = dwx_ana_sink_new(); s->subst = a; s->subst_run = 3;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, 5, k));
    CHECK(k->subst_set[0] && k->subst[0] == a);
    CHECK(k->subst_set[3] && k->subst[3] == a);
    ta_free(s, k); DW_RELEASE(a); DW_RELEASE(b); TA_END();
}
TEST(numsub_subrange)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, NATIONAL, L"ar-sa");
    static const WCHAR t[] = L"12 34 56";
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s = dwx_ana_src_new(t, 8); k = dwx_ana_sink_new(); s->subst = ns;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 3, 2, k));
    CHECK(!k->subst_set[0] && !k->subst_set[1] && k->subst_set[3] && k->subst_set[4] && !k->subst_set[6]);
    CHECK(k->n_subst == 1);
    ta_free(s, k); DW_RELEASE(ns); TA_END();
}
TEST(numsub_subrange_cuts_number)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, NATIONAL, L"ar-sa");
    static const WCHAR t[] = L"123456";
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s = dwx_ana_src_new(t, 6); k = dwx_ana_sink_new(); s->subst = ns;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 1, 3, k));
    CHECK(k->n_subst == 1 && !k->subst_set[0] && k->subst_set[1] && k->subst_set[3] && !k->subst_set[4]);
    ta_free(s, k); DW_RELEASE(ns); TA_END();
}
TEST(numsub_refcount_balanced)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, NATIONAL, L"ar-sa");
    static const WCHAR t[] = L"1 2 3 4 5";
    dwx_ana_src *s; dwx_ana_sink *k; int i;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s = dwx_ana_src_new(t, 9); k = dwx_ana_sink_new(); s->subst = ns; s->subst_run = 1;
    for (i = 0; i < 5; i++) {
        CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, 9, k));
        EXPECT_REFS(ns, 1);
    }
    ta_free(s, k); DW_RELEASE(ns); TA_END();
}
TEST(numsub_sink_error_propagates)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, NATIONAL, L"ar-sa");
    static const WCHAR t[] = L"1 2";
    dwx_ana_src *s; dwx_ana_sink *k; HRESULT hr;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s = dwx_ana_src_new(t, 3); k = dwx_ana_sink_new(); s->subst = ns; k->fail_subst = E_FAIL;
    hr = ta_do(an, TA_NUMSUB, s, 0, 3, k);
    CHECK_HR_PROP(hr, E_FAIL);
    CHECK(k->n_subst == 1);
    EXPECT_REFS(ns, 1);
    ta_free(s, k); DW_RELEASE(ns); TA_END();
}
TEST(numsub_source_error_propagates)
{
    TA_BEGIN();
    static const WCHAR t[] = L"1 2";
    dwx_ana_src *s = dwx_ana_src_new(t, 3); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    s->fail_subst = E_OUTOFMEMORY;
    hr = ta_do(an, TA_NUMSUB, s, 0, 3, k);
    CHECK_HR_PROP(hr, E_OUTOFMEMORY);
    CHECK(k->n_subst == 0);
    ta_free(s, k); TA_END();
}
TEST(numsub_traditional_and_hi_note)
{
    TA_BEGIN();
    static const struct { DWRITE_NUMBER_SUBSTITUTION_METHOD m; const WCHAR *loc; } rows[] = {
        { TRADITIONAL, L"ar-sa" }, { TRADITIONAL, L"en-us" }, { TRADITIONAL, L"hi-in" }, { TRADITIONAL, L"th-th" }, { TRADITIONAL, L"fa-ir" },
        { NATIONAL, L"hi-in" }, { NATIONAL, L"th-th" }, { NATIONAL, L"bn-bd" },
    };
    static const WCHAR t[] = L"x 123";
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        IDWriteNumberSubstitution *ns = mk_sub(f, rows[i].m, rows[i].loc); dwx_ana_src *s; dwx_ana_sink *k;
        if (!ns) { t_note("method %d %ls: creation failed", (int)rows[i].m, rows[i].loc); continue; }
        s = dwx_ana_src_new(t, 5); k = dwx_ana_sink_new(); s->subst = ns;
        CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, 5, k));
        t_note("method %d %ls: %ld sink call(s)", (int)rows[i].m, rows[i].loc, k->n_subst);
        ta_free(s, k); DW_RELEASE(ns);
    }
    TA_END();
}
TEST(numsub_contextual_note)
{
    TA_BEGIN();
    static const struct { const char *n; const WCHAR *loc; const WCHAR *t; } rows[] = {
        { "ar after Arabic", L"ar-sa", L"\x0627\x0628 123" }, { "ar after Latin", L"ar-sa", L"abc 123" }, { "ar digits first", L"ar-sa", L"123" },
        { "hi after Devanagari", L"hi-in", L"\x0915\x0916 123" }, { "en after Latin", L"en-us", L"abc 123" }, { "th after Thai", L"th-th", L"\x0e01 123" },
    };
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        IDWriteNumberSubstitution *ns = mk_sub(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL, rows[i].loc);
        UINT32 n = LEN(rows[i].t); dwx_ana_src *s; dwx_ana_sink *k; rng got[8]; int c;
        if (!ns) { t_note("%s: CreateNumberSubstitution failed", rows[i].n); continue; }
        s = dwx_ana_src_new(rows[i].t, n); k = dwx_ana_sink_new(); s->subst = ns;
        CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, n, k));
        c = sub_ranges(k, n, got, 8);
        t_note("contextual %s: %d range(s)%s", rows[i].n, c, c ? "" : " (not substituted)");
        ta_free(s, k); DW_RELEASE(ns);
    }
    TA_END();
}
TEST(numsub_from_culture_note)
{
    TA_BEGIN();
    static const WCHAR *const loc[] = { L"ar-sa", L"en-us", L"fa-ir", L"hi-in" };
    static const WCHAR t[] = L"x 123";
    int i;
    for (i = 0; i < 4; i++) {
        IDWriteNumberSubstitution *ns = mk_sub(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE, loc[i]);
        dwx_ana_src *s; dwx_ana_sink *k;
        if (!ns) { t_note("FROM_CULTURE %ls: creation failed", loc[i]); continue; }
        s = dwx_ana_src_new(t, 5); k = dwx_ana_sink_new(); s->subst = ns;
        CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, 5, k));
        t_note("FROM_CULTURE %ls: %ld sink call(s)", loc[i], k->n_subst);
        ta_free(s, k); DW_RELEASE(ns);
    }
    TA_END();
}
TEST(numsub_chunked_same)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, NATIONAL, L"ar-sa");
    static const WCHAR t[] = L"a 1,234.5 b 77% c 8";
    UINT32 n = LEN(t); int ch;
    dwx_ana_src *s0; dwx_ana_sink *k0;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s0 = dwx_ana_src_new(t, n); k0 = dwx_ana_sink_new(); s0->subst = ns;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s0, 0, n, k0));
    for (ch = 1; ch <= 6; ch++) {
        dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new(); UINT32 i;
        s->subst = ns; s->chunk = (UINT32)ch;
        CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, n, k));
        for (i = 0; i < n; i++) CHECKF(k->subst_set[i] == k0->subst_set[i], "chunk %d pos %u", ch, i);
        ta_free(s, k);
    }
    ta_free(s0, k0); DW_RELEASE(ns); TA_END();
}
TEST(numsub_surrogates_in_text)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, NATIONAL, L"ar-sa");
    static const WCHAR t[] = L"\xd83d\xde00 12 \xd83d\xde01";
    dwx_ana_src *s; dwx_ana_sink *k; rng got[4]; int c;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    s = dwx_ana_src_new(t, LEN(t)); k = dwx_ana_sink_new(); s->subst = ns;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, LEN(t), k));
    c = sub_ranges(k, LEN(t), got, 4);
    CHECK(c == 1 && got[0].pos == 3 && got[0].len == 2);
    ta_free(s, k); DW_RELEASE(ns); TA_END();
}
TEST(numsub_long_text)
{
    TA_BEGIN();
    IDWriteNumberSubstitution *ns = mk_sub(f, NATIONAL, L"ar-sa");
    static WCHAR t[3000]; int i, n = 3000;
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!ns) { TA_END(); t_skip("CreateNumberSubstitution failed"); return; }
    for (i = 0; i < n; i++) t[i] = (WCHAR)(i % 3 == 2 ? ' ' : '0' + i % 10);
    s = dwx_ana_src_new(t, n); k = dwx_ana_sink_new(); s->subst = ns;
    CHECK_HR_OK(ta_do(an, TA_NUMSUB, s, 0, n, k));
    CHECK(k->n_subst == 1000);
    ta_free(s, k); DW_RELEASE(ns); TA_END();
}
