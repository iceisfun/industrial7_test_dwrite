/* IDWriteTextAnalyzer::AnalyzeLineBreakpoints: SetLineBreakpoints(textPosition, textLength, DWRITE_LINE_BREAKPOINT[textLength]).
 *
 * One sink call covers the whole requested range.  For consecutive characters i-1, i the "after" condition of i-1 equals the "before"
 * condition of i (both describe the same boundary):  CAN_BREAK = break allowed, MAY_NOT_BREAK = break prohibited, MUST_BREAK = mandatory.
 * The first character's "before" is MAY_NOT_BREAK when no text precedes the range (the source returns nothing from
 * GetTextBeforePosition) and the last character's "after" is CAN_BREAK when no text follows (MUST_BREAK after a hard break).
 * When text precedes / follows the range, the boundary belongs to the neighbouring range and is reported as NEUTRAL (MUST_BREAK
 * after a hard break).  Surrogate pairs are never split: the high surrogate's "after" and the low surrogate's "before" are
 * MAY_NOT_BREAK.  Pattern strings below describe the n+1 boundaries of an n-unit text: 'x' prohibited, '/' allowed, '!' mandatory. */
#include "ta.h"

static int cond_of(char c) { if (c == '_') return -1; return c == 'x' ? BC_MAYNOT : c == '/' ? BC_CAN : c == '!' ? BC_MUST : -1; }
static const char *cname(int c) { static const char *n[] = { "NEUTRAL", "CAN_BREAK", "MAY_NOT_BREAK", "MUST_BREAK" }; return n[c & 3]; }

static void lb_check(IDWriteTextAnalyzer *an, const WCHAR *t, const char *pat, UINT32 chunk)
{
    UINT32 n = LEN(t), j;
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run_n(an, TA_BREAK, t, n, chunk, &s, &k));
    CHECK(k->n_break == 1);
    CHECK(ta_runs_exact(k, 'b', 0, n));
    CHECK(dwx_ana_covered(k, 'b', n));
    CHECKF(strlen(pat) == n + 1, "pattern length %u for %u units", (unsigned)strlen(pat), n);
    if (strlen(pat) == n + 1) {
        for (j = 0; j <= n; j++) {
            int want = cond_of(pat[j]), got;
            if (want < 0) continue;
            got = j == 0 ? BRK_BEFORE(k->brk[0]) : BRK_AFTER(k->brk[j - 1]);
            CHECKF(got == want, "boundary %u (%s side): got %s want %s", j, j == 0 ? "start" : "after previous", cname(got), cname(want));
            if (j > 0 && j < n) CHECKF(BRK_BEFORE(k->brk[j]) == want, "boundary %u (before side of char %u): got %s want %s", j, j, cname(BRK_BEFORE(k->brk[j])), cname(want));
        }
    }
    for (j = 0; j < n; j++) CHECKF(k->brk[j].padding == 0, "padding bits set at %u", j);
    ta_free(s, k);
}
static void lb_expect(const WCHAR *t, const char *pat)
{
    TA_BEGIN();
    lb_check(an, t, pat, 0);
    lb_check(an, t, pat, 1);
    TA_END();
}
#define LB_TEST(nm, text, pat) TEST(lb_##nm) { lb_expect(text, pat); }

/* ---- UAX #14 samples --------------------------------------------------------------------------------------------------------------- */
LB_TEST(letters,            L"ab",              "xx/")
LB_TEST(single_letter,      L"a",               "x/")
LB_TEST(word_space_word,    L"ab cd",           "xxx/x/")
LB_TEST(double_space,       L"a  b",            "xxx//")
LB_TEST(trailing_space,     L"ab ",             "xxx/")
LB_TEST(leading_space,      L" ab",             "x/x/")
LB_TEST(only_spaces,        L"   ",             "xxx/")
LB_TEST(three_words,        L"a b c",           "xx/x//")
LB_TEST(hyphen,             L"a-b",             "xx//")
LB_TEST(hyphen_after_digit, L"10-20",           "xxxxx/")
LB_TEST(nbsp,               L"a\x00a0" L"b",    "xxx/")
LB_TEST(nbsp_with_space,    L"a\x00a0 b",       "xxx//")
LB_TEST(narrow_nbsp,        L"a\x202f" L"b",    "xxx/")
LB_TEST(word_joiner,        L"a\x2060" L"b",    "xxx/")
LB_TEST(zwsp,               L"a\x200b" L"b",    "xx//")
LB_TEST(zwsp_then_space,    L"a\x200b b",       "xxx//")
LB_TEST(cjk_ideographs,     L"\x4e2d\x6587\x5b57", "x///")
LB_TEST(latin_then_cjk,     L"a\x4e2d",         "x//")
LB_TEST(cjk_then_latin,     L"\x4e2d" L"a",     "x//")
LB_TEST(hiragana,           L"\x3042\x3044\x3046", "x///")
LB_TEST(katakana,           L"\x30a2\x30a4",    "x//")
LB_TEST(cjk_close_punct,    L"\x4e2d\x3002",    "xx/")
LB_TEST(cjk_open_bracket,   L"\x4e2d\x300c\x6587", "x/x/")
LB_TEST(cjk_close_then_ideograph, L"\x300d\x6587", "x//")
LB_TEST(quotes_tight,       L"\"ab\"",          "xxxx/")
LB_TEST(quote_after_space,  L"a \"b\"",         "xx/xx/")
LB_TEST(open_paren,         L"(a)",             "xxx/")
LB_TEST(paren_in_sentence,  L"a (b) c",         "xx/xxx//")
LB_TEST(open_paren_space,   L"( a)",            "xxxx/")
LB_TEST(digits_comma,       L"12,345",          "xxxxxx/")
LB_TEST(digits_decimal,     L"3.14",            "xxxx/")
LB_TEST(currency_prefix,    L"$100",            "xxxx/")
LB_TEST(percent_suffix,     L"100%",            "xxxx/")
LB_TEST(letter_digit,       L"a1",              "xx/")
LB_TEST(digit_letter,       L"1a",              "xx/")
LB_TEST(number_space_number, L"1 2",            "xx//")
LB_TEST(lf,                 L"a\nb",            "xx!/")
LB_TEST(crlf,               L"a\r\nb",          "xxx!/")
LB_TEST(cr_alone,           L"a\rb",            "xx!/")
LB_TEST(trailing_lf,        L"a\n",             "xx!")
LB_TEST(trailing_crlf,      L"a\r\n",           "xxx!")
LB_TEST(lf_only,            L"\n",              "x!")
LB_TEST(two_lf,             L"\n\n",            "x!!")
LB_TEST(lf_cr,              L"\n\r",            "x!_")
LB_TEST(line_separator,     L"a\x2028" L"b",    "xx!/")
LB_TEST(paragraph_separator, L"a\x2029" L"b",   "xx!/")
LB_TEST(next_line,          L"a\x0085" L"b",    "xx!/")
LB_TEST(vertical_tab,       L"a\x000b" L"b",    "xx!/")
LB_TEST(form_feed,          L"a\x000c" L"b",    "xx!/")
LB_TEST(space_before_lf,    L"a \nb",           "xxx!/")
LB_TEST(soft_hyphen,        L"a\x00ad" L"b",    "xx//")
LB_TEST(tab,                L"a\tb",            "xx//")
LB_TEST(combining_mark,     L"e\x0301",         "xx/")
LB_TEST(combining_mark_then_letter, L"e\x0301" L"b", "xxx/")
LB_TEST(surrogate_pair_only, L"\xd840\xdc00",   "xx/")
LB_TEST(surrogate_between_letters_cjk, L"\xd840\xdc00\xd840\xdc01", "xx/x/")
LB_TEST(ideograph_after_ext_b, L"\x4e2d\xd840\xdc00", "x/x/")
LB_TEST(exclamation_end,    L"a!",              "xx/")
LB_TEST(question_space,     L"a? b",            "xxx//")
LB_TEST(comma_space,        L"a, b",            "xxx//")
LB_TEST(period_space,       L"a. b",            "xxx//")

/* Pairs whose treatment depends on the Unicode version of the line-break tables are recorded, not enforced. */
static void lb_note(const WCHAR *t, const char *what)
{
    TA_BEGIN();
    UINT32 n = LEN(t), j; char buf[160]; int o = 0;
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s, &k));
    CHECK(ta_runs_exact(k, 'b', 0, n));
    for (j = 0; j + 1 < n && o < 140; j++) {
        int a = BRK_AFTER(k->brk[j]), b = BRK_BEFORE(k->brk[j + 1]);
        CHECK(a == b);
        o += snprintf(buf + o, sizeof buf - o, "%c", a == BC_CAN ? '/' : a == BC_MUST ? '!' : a == BC_MAYNOT ? 'x' : '?');
    }
    t_note("%s: interior boundaries %s", what, buf);
    ta_free(s, k); TA_END();
}
TEST(lb_note_slash)          { lb_note(L"a/b", "a/b"); }
TEST(lb_note_slash_digits)   { lb_note(L"1/2", "1/2"); }
TEST(lb_note_url)            { lb_note(L"http://a.b/c?d=e", "URL"); }
TEST(lb_note_close_paren_letter) { lb_note(L"(a)b", "(a)b"); }
TEST(lb_note_exclam_letter)  { lb_note(L"a!b", "a!b"); }
TEST(lb_note_emdash)         { lb_note(L"a\x2014" L"b", "a em-dash b"); }
TEST(lb_note_emoji)          { lb_note(L"a\xd83d\xde00" L"b", "a emoji b"); }
TEST(lb_note_emoji_pair)     { lb_note(L"\xd83d\xde00\xd83d\xde01", "emoji emoji"); }
TEST(lb_note_hangul)         { lb_note(L"\xd55c\xad6d\xc5b4", "hangul syllables"); }
TEST(lb_note_thai)           { lb_note(L"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35", "thai"); }
TEST(lb_note_space_cm)       { lb_note(L"a \x0301" L"b", "a SP CM b"); }
TEST(lb_note_ellipsis)       { lb_note(L"a\x2026" L"b", "a ellipsis b"); }
TEST(lb_note_hebrew_hyphen)  { lb_note(L"\x05d0-\x05d1", "hebrew hyphen"); }
TEST(lb_note_prefix_postfix) { lb_note(L"a$ 5%", "a$ 5%"); }
TEST(lb_note_small_kana)     { lb_note(L"\x3042\x3041\x3044", "a small-a i"); }
TEST(lb_note_prolonged)      { lb_note(L"\x30a2\x30fc\x30a4", "a prolonged i"); }
TEST(lb_note_fullwidth_space){ lb_note(L"a\x3000" L"b", "a ideographic-space b"); }
TEST(lb_note_en_space)       { lb_note(L"a\x2002" L"b", "a en-space b"); }

/* ---- whitespace and soft hyphen flags ---------------------------------------------------------------------------------------------- */
static int flags_of(IDWriteTextAnalyzer *an, WCHAR c, int *ws, int *sh)
{
    WCHAR t[2] = { c, 0 }; dwx_ana_src *s; dwx_ana_sink *k;
    HRESULT hr = ta_run(an, TA_BREAK, t, &s, &k);
    *ws = k->brk[0].isWhitespace; *sh = k->brk[0].isSoftHyphen;
    ta_free(s, k);
    return hr == S_OK;
}
TEST(flags_space_is_whitespace)
{
    TA_BEGIN();
    int ws, sh;
    CHECK(flags_of(an, ' ', &ws, &sh)); CHECK(ws == 1 && sh == 0);
    TA_END();
}
TEST(flags_letters_not_whitespace)
{
    TA_BEGIN();
    static const WCHAR c[] = { 'a', 'Z', '0', '9', '-', '.', ',', 0x4e2d, 0x5d0, 0x628, 0x3b1, '!', '(' };
    int i, ws, sh;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) { CHECK(flags_of(an, c[i], &ws, &sh)); CHECKF(ws == 0 && sh == 0, "U+%04X ws %d sh %d", c[i], ws, sh); }
    TA_END();
}
TEST(flags_soft_hyphen)
{
    TA_BEGIN();
    int ws, sh;
    CHECK(flags_of(an, 0x00ad, &ws, &sh)); CHECK(sh == 1);
    CHECK(ws == 0);
    TA_END();
}
TEST(flags_only_soft_hyphen_sets_it)
{
    TA_BEGIN();
    static const WCHAR c[] = { ' ', '-', 0x2010, 0x2011, 0x00a0, 0x200b, '\t', '\n', 'x' };
    int i, ws, sh;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) { CHECK(flags_of(an, c[i], &ws, &sh)); CHECKF(sh == 0, "U+%04X reports soft hyphen", c[i]); }
    TA_END();
}
TEST(flags_whitespace_note)
{
    TA_BEGIN();
    static const WCHAR c[] = { '\t', '\n', '\r', 0x0b, 0x0c, 0x0085, 0x00a0, 0x1680, 0x2000, 0x2002, 0x2003, 0x2009, 0x200a, 0x200b, 0x200c, 0x200d, 0x2028, 0x2029, 0x202f, 0x205f, 0x2060, 0x3000, 0xfeff };
    int i, ws, sh;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) { CHECK(flags_of(an, c[i], &ws, &sh)); t_note("U+%04X isWhitespace=%d", c[i], ws); }
    TA_END();
}
TEST(flags_surrogate_pair_same_flags)
{
    TA_BEGIN();
    static const WCHAR t[] = L"\xd840\xdc00";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s, &k));
    CHECK(k->brk[0].isWhitespace == k->brk[1].isWhitespace);
    CHECK(k->brk[0].isSoftHyphen == k->brk[1].isSoftHyphen);
    CHECK(BRK_AFTER(k->brk[0]) == BC_MAYNOT);
    CHECK(BRK_BEFORE(k->brk[1]) == BC_MAYNOT);
    ta_free(s, k); TA_END();
}
TEST(surrogate_pair_inside_text)
{
    TA_BEGIN();
    static const WCHAR t[] = L"ab\xd83d\xde00\xd83d\xde01 cd\xd840\xdc00";
    UINT32 n = LEN(t), i; dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s, &k));
    for (i = 0; i + 1 < n; i++) if ((t[i] & 0xfc00) == 0xd800 && (t[i + 1] & 0xfc00) == 0xdc00) {
        CHECKF(BRK_AFTER(k->brk[i]) == BC_MAYNOT && BRK_BEFORE(k->brk[i + 1]) == BC_MAYNOT, "pair at %u split", i);
    }
    ta_free(s, k); TA_END();
}

/* ---- boundary consistency over mixed text ------------------------------------------------------------------------------------------- */
static const WCHAR *const g_lbtexts[] = {
    L"The quick brown fox jumps over the lazy dog.", L"Hello, world! (see 1,234.56 or 99%) -- ok?", L"a-b/c\\d_e|f", L"\x4e2d\x6587 text \x65e5\x672c\x8a9e\x3002",
    L"line1\nline2\r\nline3\rend", L"\x05d0\x05d1 \x05d2 abc \x0627\x0628", L"tab\there\x00a0nbsp\x00ad" L"soft", L"\xd83d\xde00 \xd83d\xde01\xd83d\xde02 x",
};
TEST(boundary_consistency_all_texts)
{
    TA_BEGIN();
    int ti;
    for (ti = 0; ti < (int)(sizeof g_lbtexts / sizeof g_lbtexts[0]); ti++) {
        const WCHAR *t = g_lbtexts[ti]; UINT32 n = LEN(t), i; dwx_ana_src *s; dwx_ana_sink *k;
        CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s, &k));
        CHECK(k->n_break == 1);
        CHECKF(BRK_BEFORE(k->brk[0]) == BC_MAYNOT, "text %d: start boundary %s", ti, cname(BRK_BEFORE(k->brk[0])));
        for (i = 1; i < n; i++) CHECKF(BRK_AFTER(k->brk[i - 1]) == BRK_BEFORE(k->brk[i]), "text %d boundary %u: after %s before %s", ti, i, cname(BRK_AFTER(k->brk[i - 1])), cname(BRK_BEFORE(k->brk[i])));
        CHECKF(BRK_AFTER(k->brk[n - 1]) == BC_CAN || BRK_AFTER(k->brk[n - 1]) == BC_MUST, "text %d: end boundary %s", ti, cname(BRK_AFTER(k->brk[n - 1])));
        for (i = 0; i < n; i++) CHECKF(BRK_BEFORE(k->brk[i]) != BC_NEUTRAL && BRK_AFTER(k->brk[i]) != BC_NEUTRAL, "text %d unit %u neutral", ti, i);
        ta_free(s, k);
    }
    TA_END();
}
TEST(letters_never_break_inside_word)
{
    TA_BEGIN();
    static const WCHAR t[] = L"internationalization";
    UINT32 i, n = LEN(t); dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s, &k));
    for (i = 1; i < n; i++) CHECK(BRK_BEFORE(k->brk[i]) == BC_MAYNOT);
    ta_free(s, k); TA_END();
}

/* ---- ranges and context ------------------------------------------------------------------------------------------------------------ */
TEST(subrange_context_before)
{
    TA_BEGIN();
    static const WCHAR t[] = L"ab cd ef";
    dwx_ana_src *s = dwx_ana_src_new(t, 8); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BREAK, s, 3, 2, k));
    CHECK(k->n_break == 1);
    CHECK(k->break_runs[0].pos == 3 && k->break_runs[0].len == 2);
    CHECK(!k->brk_set[2] && k->brk_set[3] && k->brk_set[4] && !k->brk_set[5]);
    /* preceding text exists: the boundary in front of the range is left to the neighbouring range */
    CHECKF(BRK_BEFORE(k->brk[3]) == BC_NEUTRAL, "before the range: %s", cname(BRK_BEFORE(k->brk[3])));
    CHECK(s->n_before >= 1);
    CHECK(BRK_BEFORE(k->brk[4]) == BC_MAYNOT && BRK_AFTER(k->brk[3]) == BC_MAYNOT);
    ta_free(s, k); TA_END();
}
TEST(subrange_context_after)
{
    TA_BEGIN();
    static const WCHAR t[] = L"ab cd ef";
    dwx_ana_src *s = dwx_ana_src_new(t, 8); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BREAK, s, 0, 2, k));
    CHECKF(BRK_BEFORE(k->brk[0]) == BC_MAYNOT, "start: %s", cname(BRK_BEFORE(k->brk[0])));
    CHECKF(BRK_AFTER(k->brk[1]) == BC_NEUTRAL, "end with following text: %s", cname(BRK_AFTER(k->brk[1])));
    CHECK(s->n_before == 0);
    ta_free(s, k); TA_END();
}
TEST(subrange_no_before_call_at_zero)
{
    TA_BEGIN();
    static const WCHAR t[] = L"ab cd";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s, &k));
    CHECK(s->n_before == 0);
    CHECK(s->n_at >= 1);
    ta_free(s, k); TA_END();
}
TEST(subrange_after_hard_break_is_must)
{
    TA_BEGIN();
    static const WCHAR t[] = L"ab\ncd";
    dwx_ana_src *s = dwx_ana_src_new(t, 5); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BREAK, s, 3, 2, k));
    CHECKF(BRK_BEFORE(k->brk[3]) == BC_MUST, "after LF context: %s", cname(BRK_BEFORE(k->brk[3])));
    ta_free(s, k);
    s = dwx_ana_src_new(t, 5); k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BREAK, s, 0, 3, k));
    CHECKF(BRK_AFTER(k->brk[2]) == BC_MUST, "range ending with LF: %s", cname(BRK_AFTER(k->brk[2])));
    ta_free(s, k); TA_END();
}
TEST(subrange_each_position_one_call)
{
    TA_BEGIN();
    static const WCHAR t[] = L"The quick, brown fox.";
    UINT32 n = LEN(t), p, l;
    for (p = 0; p < n; p += 2) for (l = 1; p + l <= n; l += 5) {
        dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
        HRESULT hr = ta_do(an, TA_BREAK, s, p, l, k);
        CHECKF(hr == S_OK && k->n_break == 1 && k->break_runs[0].pos == p && k->break_runs[0].len == l, "pos %u len %u", p, l);
        ta_free(s, k);
    }
    TA_END();
}
TEST(subrange_interior_matches_whole)
{
    /* inside a sub-range the interior boundaries equal those of the whole-text analysis */
    TA_BEGIN();
    static const WCHAR t[] = L"The quick, brown-fox (jumps) over 12,345 dogs.";
    UINT32 n = LEN(t), i; dwx_ana_src *s0; dwx_ana_sink *k0;
    CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s0, &k0));
    {
        dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
        CHECK_HR_OK(ta_do(an, TA_BREAK, s, 5, 25, k));
        for (i = 6; i < 29; i++) CHECKF(!memcmp(&k->brk[i], &k0->brk[i], 1), "unit %u: %02x vs %02x", i, *(unsigned char *)&k->brk[i], *(unsigned char *)&k0->brk[i]);
        ta_free(s, k);
    }
    ta_free(s0, k0); TA_END();
}
TEST(chunk_sizes_same_result_long)
{
    TA_BEGIN();
    static const WCHAR t[] = L"Hello, world! (1,234.5) \x4e2d\x6587\x3002 a-b\nnext\x00ad" L"soft";
    UINT32 n = LEN(t), i; int ch; dwx_ana_src *s0; dwx_ana_sink *k0;
    CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s0, &k0));
    for (ch = 1; ch <= 12; ch++) {
        dwx_ana_src *s; dwx_ana_sink *k;
        CHECK_HR_OK(ta_run_n(an, TA_BREAK, t, n, (UINT32)ch, &s, &k));
        for (i = 0; i < n; i++) CHECKF(!memcmp(&k->brk[i], &k0->brk[i], 1), "chunk %d unit %u", ch, i);
        ta_free(s, k);
    }
    ta_free(s0, k0); TA_END();
}
TEST(single_chars_boundaries)
{
    TA_BEGIN();
    static const WCHAR c[] = { 'a', ' ', '-', '1', 0x4e2d, 0x5d0, '(', ')', '\n', 0x00ad };
    int i;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        WCHAR t[2] = { c[i], 0 }; dwx_ana_src *s; dwx_ana_sink *k;
        CHECK_HR_OK(ta_run(an, TA_BREAK, t, &s, &k));
        CHECKF(BRK_BEFORE(k->brk[0]) == BC_MAYNOT, "U+%04X start %s", c[i], cname(BRK_BEFORE(k->brk[0])));
        CHECKF(BRK_AFTER(k->brk[0]) == (c[i] == '\n' ? BC_MUST : BC_CAN), "U+%04X end %s", c[i], cname(BRK_AFTER(k->brk[0])));
        ta_free(s, k);
    }
    TA_END();
}
TEST(huge_line_break_text)
{
    TA_BEGIN();
    UINT32 n = 100000, i; WCHAR *t = (WCHAR *)malloc(n * sizeof(WCHAR));
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!t) { TA_END(); t_skip("alloc"); return; }
    for (i = 0; i < n; i++) t[i] = (WCHAR)(i % 6 == 5 ? ' ' : 'a' + i % 26);
    s = dwx_ana_src_new(t, n); k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_BREAK, s, 0, n, k));
    CHECK(k->n_break == 1);
    CHECK(k->break_runs[0].pos == 0 && k->break_runs[0].len == n);
    ta_free(s, k); free(t); TA_END();
}
TEST(sink_error_propagates_break)
{
    TA_BEGIN();
    static const WCHAR t[] = L"ab cd";
    dwx_ana_src *s = dwx_ana_src_new(t, 5); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    k->fail_break = E_ABORT;
    hr = ta_do(an, TA_BREAK, s, 0, 5, k);
    CHECK_HR(hr, E_ABORT);
    CHECK(k->n_break == 1);
    ta_free(s, k); TA_END();
}
TEST(source_error_propagates_break)
{
    TA_BEGIN();
    static const WCHAR t[] = L"ab cd";
    ta_fsrc fs; dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    if (t_is_wine()) { ta_free(NULL, k); TA_END(); t_skip("emulator faults on source errors"); return; }
    ta_fsrc_init(&fs, t, 5); fs.fail_at = E_FAIL;
    hr = IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)&fs, 0, 5, (IDWriteTextAnalysisSink *)k);
    CHECK_HR_PROP(hr, E_FAIL);
    CHECK(k->n_break == 0);
    ta_free(NULL, k); TA_END();
}
TEST(source_before_error_propagates_break)
{
    TA_BEGIN();
    static const WCHAR t[] = L"ab cd";
    ta_fsrc fs; dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    if (t_is_wine()) { ta_free(NULL, k); TA_END(); t_skip("emulator faults on source errors"); return; }
    ta_fsrc_init(&fs, t, 5); fs.fail_before = E_FAIL;
    hr = IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)&fs, 2, 3, (IDWriteTextAnalysisSink *)k);
    CHECK_HR_PROP(hr, E_FAIL);
    CHECK(k->n_break == 0);
    ta_free(NULL, k); TA_END();
}
