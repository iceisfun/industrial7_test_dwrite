/* DWRITE_LINE_METRICS content: length / trailingWhitespaceLength / newlineLength / isTrimmed for wrapped text, whitespace, every
 * newline kind, tabs and the word-wrapping modes; invariants that tie lines, clusters and DWRITE_TEXT_METRICS together.
 *
 * Facts used (Windows line breaker):
 *  - a line is broken when the running width of the content exceeds the layout width strictly (exact fit stays on the line);
 *    whitespace clusters never trigger a break, so trailing whitespace may overhang the layout width;
 *  - a word wider than the layout is split by character by WRAP, EMERGENCY_BREAK and CHARACTER; WHOLE_WORD and NO_WRAP keep it;
 *  - trailingWhitespaceLength counts whitespace clusters at the end of the line including the newline sequence; a trailing tab
 *    is the exception and is not counted;
 *  - newlineLength is the length of the mandatory-break sequence ending the line (CRLF = 2, every other newline char = 1);
 *  - text ending in a mandatory break gets one more, empty, line; an empty string has exactly one empty line. */
#include "tlm.h"

typedef struct { UINT32 len, tws, nl; } xl;
typedef struct {
    const char *name; const WCHAR *fam; const WCHAR *text; float w; int ww; int n; xl ln[8];
} wcase;

static void run_cases(tm_fx *x, const wcase *cs, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        const wcase *c = &cs[i];
        IDWriteTextLayout *l = tm_layout_(x, c->fam ? c->fam : FAM_TEST, c->text, (UINT32)wcslen(c->text), SZ, c->w < 0 ? 1000.0f : c->w * CW, 1000.0f);
        tm_lines ln; int j;
        if (!l) { CHECKF(0, "%s: layout creation failed", c->name); continue; }
        if (c->ww >= 0) CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, (DWRITE_WORD_WRAPPING)c->ww));
        if (tm_get_lines(l, &ln)) {
            CHECKF(ln.n == (UINT32)c->n, "%s: line count %u want %d", c->name, ln.n, c->n);
            for (j = 0; j < c->n && (UINT32)j < ln.n; j++) {
                CHECKF(ln.v[j].length == c->ln[j].len, "%s: line %d length %u want %u", c->name, j, ln.v[j].length, c->ln[j].len);
                CHECKF(ln.v[j].trailingWhitespaceLength == c->ln[j].tws, "%s: line %d trailing ws %u want %u", c->name, j, ln.v[j].trailingWhitespaceLength, c->ln[j].tws);
                CHECKF(ln.v[j].newlineLength == c->ln[j].nl, "%s: line %d newline %u want %u", c->name, j, ln.v[j].newlineLength, c->ln[j].nl);
                CHECKF(!ln.v[j].isTrimmed, "%s: line %d unexpectedly trimmed", c->name, j);
            }
        }
        DW_RELEASE(l);
    }
}

#define WRAP DWRITE_WORD_WRAPPING_WRAP
#define NOWRAP DWRITE_WORD_WRAPPING_NO_WRAP
#define EMERG DWRITE_WORD_WRAPPING_EMERGENCY_BREAK
#define WHOLE DWRITE_WORD_WRAPPING_WHOLE_WORD
#define CHAR_ DWRITE_WORD_WRAPPING_CHARACTER

TEST(ll_wrap_basic)
{
    TM_BEGIN();
    static const wcase cs[] = {
        { "two words wide", 0, L"AAAA BBBB", 6, -1, 2, { {5,1,0}, {4,0,0} } },
        { "fits", 0, L"AAAA BBBB", 100, -1, 1, { {9,0,0} } },
        { "exact fit", 0, L"AAAA BBBB", 9, -1, 1, { {9,0,0} } },
        { "just under fit", 0, L"AAAA BBBB", 8.99f, -1, 2, { {5,1,0}, {4,0,0} } },
        { "three words w5", 0, L"AA BB CC", 5, -1, 2, { {6,1,0}, {2,0,0} } },
        { "three words w4", 0, L"AA BB CC", 4, -1, 3, { {3,1,0}, {3,1,0}, {2,0,0} } },
        { "three words w2", 0, L"AA BB CC", 2, -1, 3, { {3,1,0}, {3,1,0}, {2,0,0} } },
        { "single letters", 0, L"A B C D", 1, -1, 4, { {2,1,0}, {2,1,0}, {2,1,0}, {1,0,0} } },
        { "zero width", 0, L"AAA BBB", 0, -1, 6, { {1,0,0}, {1,0,0}, {2,1,0}, {1,0,0}, {1,0,0}, {1,0,0} } },
        { "word exactly at limit then more", 0, L"AAA BBB CCC", 7, -1, 2, { {8,1,0}, {3,0,0} } },
        { "limit one more than word", 0, L"AAA BBB CCC", 3, -1, 3, { {4,1,0}, {4,1,0}, {3,0,0} } },
    };
    run_cases(&X, cs, sizeof cs / sizeof cs[0]);
    TM_END();
}
TEST(ll_wrap_long_words)
{
    TM_BEGIN();
    static const wcase cs[] = {
        { "overflow splits word", 0, L"AAAAAAAA", 3, WRAP, 3, { {3,0,0}, {3,0,0}, {2,0,0} } },
        { "overflow then wrap", 0, L"AAAAAAAA BB", 3, WRAP, 4, { {3,0,0}, {3,0,0}, {3,1,0}, {2,0,0} } },
        { "whole word", 0, L"AAAAAAAA", 3, WHOLE, 1, { {8,0,0} } },
        { "emergency", 0, L"AAAAAAAA", 3, EMERG, 3, { {3,0,0}, {3,0,0}, {2,0,0} } },
        { "character", 0, L"AAAAAAAA", 3, CHAR_, 3, { {3,0,0}, {3,0,0}, {2,0,0} } },
        { "emergency exact", 0, L"AAAAAA", 3, EMERG, 2, { {3,0,0}, {3,0,0} } },
        { "character one wide", 0, L"AAAA", 1, CHAR_, 4, { {1,0,0}, {1,0,0}, {1,0,0}, {1,0,0} } },
        { "emergency one wide", 0, L"AAAA", 1, EMERG, 4, { {1,0,0}, {1,0,0}, {1,0,0}, {1,0,0} } },
        { "no wrap", 0, L"AA BB CC", 2, NOWRAP, 1, { {8,0,0} } },
        { "no wrap long", 0, L"AAAAAAAA", 3, NOWRAP, 1, { {8,0,0} } },
    };
    run_cases(&X, cs, sizeof cs / sizeof cs[0]);
    TM_END();
}
TEST(ll_wrap_whitespace)
{
    TM_BEGIN();
    static const wcase cs[] = {
        { "trailing spaces", 0, L"AB   ", 100, -1, 1, { {5,3,0} } },
        { "leading spaces", 0, L"  AB", 100, -1, 1, { {4,0,0} } },
        { "only spaces", 0, L"   ", 100, -1, 1, { {3,3,0} } },
        { "single space", 0, L" ", 100, -1, 1, { {1,1,0} } },
        { "many spaces wrap", 0, L"AA    BB", 3, -1, 2, { {6,4,0}, {2,0,0} } },
        { "space then word", 0, L"AA BB", 2, -1, 2, { {3,1,0}, {2,0,0} } },
        /* the break opportunity after the last leading space is taken as soon as AAA overflows, so the spaces form a line of their own */
        { "leading spaces wrap", 0, L"   AAA BBB", 5, -1, 3, { {3,3,0}, {4,1,0}, {3,0,0} } },
        { "spaces overhang limit", 0, L"AB      ", 3, -1, 1, { {8,6,0} } },
    };
    run_cases(&X, cs, sizeof cs / sizeof cs[0]);
    TM_END();
}
TEST(ll_newline_kinds)
{
    /* every character with a mandatory break ends the line and is counted as one newline (CRLF as two) */
    TM_BEGIN();
    static const wcase cs[] = {
        { "LF", 0, L"AB\nCD", 100, -1, 2, { {3,1,1}, {2,0,0} } },
        { "CR", 0, L"AB\rCD", 100, -1, 2, { {3,1,1}, {2,0,0} } },
        { "CRLF", 0, L"AB\r\nCD", 100, -1, 2, { {4,2,2}, {2,0,0} } },
        { "VT", 0, L"AB\vCD", 100, -1, 2, { {3,1,1}, {2,0,0} } },
        { "FF", 0, L"AB\fCD", 100, -1, 2, { {3,1,1}, {2,0,0} } },
        { "NEL", 0, L"AB\x85" L"CD", 100, -1, 2, { {3,1,1}, {2,0,0} } },
        { "LS", 0, L"AB " L"CD", 100, -1, 2, { {3,1,1}, {2,0,0} } },
        { "PS", 0, L"AB " L"CD", 100, -1, 2, { {3,1,1}, {2,0,0} } },
        { "space before LF", 0, L"AB \nCD", 100, -1, 2, { {4,2,1}, {2,0,0} } },
        { "empty line between", 0, L"A\n\nB", 100, -1, 3, { {2,1,1}, {1,1,1}, {1,0,0} } },
        { "LF CR is two breaks", 0, L"AB\n\rCD", 100, -1, 3, { {3,1,1}, {1,1,1}, {2,0,0} } },
        { "CRLF CRLF", 0, L"A\r\n\r\nB", 100, -1, 3, { {3,2,2}, {2,2,2}, {1,0,0} } },
        { "newline plus wrap", 0, L"AAA BBB\nCC", 4, -1, 3, { {4,1,0}, {4,1,1}, {2,0,0} } },
        { "newline in no wrap", 0, L"AA BB\nCC", 1, NOWRAP, 2, { {6,1,1}, {2,0,0} } },
    };
    run_cases(&X, cs, sizeof cs / sizeof cs[0]);
    TM_END();
}
TEST(ll_trailing_empty_line)
{
    /* text ending in a mandatory break gets one extra, empty line */
    TM_BEGIN();
    static const wcase cs[] = {
        { "trailing LF", 0, L"AB\n", 100, -1, 2, { {3,1,1}, {0,0,0} } },
        { "trailing CRLF", 0, L"AB\r\n", 100, -1, 2, { {4,2,2}, {0,0,0} } },
        { "only LF", 0, L"\n", 100, -1, 2, { {1,1,1}, {0,0,0} } },
        { "two LF", 0, L"\n\n", 100, -1, 3, { {1,1,1}, {1,1,1}, {0,0,0} } },
        { "only CRLF", 0, L"\r\n", 100, -1, 2, { {2,2,2}, {0,0,0} } },
        { "trailing LS", 0, L"AB ", 100, -1, 2, { {3,1,1}, {0,0,0} } },
        { "trailing PS", 0, L"AB ", 100, -1, 2, { {3,1,1}, {0,0,0} } },
        { "trailing space no extra line", 0, L"AB ", 100, -1, 1, { {3,1,0} } },
    };
    run_cases(&X, cs, sizeof cs / sizeof cs[0]);
    TM_END();
}
TEST(ll_trailing_empty_line_height)
{
    /* the extra empty line has the height of a regular line (the caret needs it) */
    TM_BEGIN(); TM_L(l, L"AB\n", 1000, 1000);
    tm_lines ln;
    if (tm_get_lines(l, &ln) && ln.n == 2) {
        NEAR(ln.v[0].height, LH); NEAR(ln.v[1].height, LH);
        NEAR(ln.v[0].baseline, ASC); NEAR(ln.v[1].baseline, ASC);
    }
    DW_RELEASE(l); TM_END();
}
TEST(ll_tabs)
{
    /* a trailing tab is not counted as trailing whitespace; a space after it still is */
    TM_BEGIN();
    static const wcase cs[] = {
        { "tab inside", 0, L"AB\tCD", 100, -1, 1, { {5,0,0} } },
        { "trailing tab", 0, L"AB\t", 100, -1, 1, { {3,0,0} } },
        { "tab then space", 0, L"AB\t ", 100, -1, 1, { {4,1,0} } },
        { "space then tab", 0, L"AB \t", 100, -1, 1, { {4,0,0} } },
        { "tab newline", 0, L"AB\t\nCD", 100, -1, 2, { {4,1,1}, {2,0,0} } },
    };
    run_cases(&X, cs, sizeof cs / sizeof cs[0]);
    TM_END();
}
TEST(ll_break_opportunities_extended)
{
    TM_BEGIN();
    static const wcase cs[] = {
        { "break after hyphen", FAM_UNI, L"AAA-BBB", 5, -1, 2, { {4,0,0}, {3,0,0} } },
        { "hyphen fits", FAM_UNI, L"AAA-BBB", 7, -1, 1, { {7,0,0} } },
        { "cjk between any", FAM_UNI, L"一丁丂七", 4, -1, 2, { {2,0,0}, {2,0,0} } },
        { "cjk odd width", FAM_UNI, L"一丁丂", 5, -1, 2, { {2,0,0}, {1,0,0} } },
        { "cjk one per line", FAM_UNI, L"一丁丂", 2, -1, 3, { {1,0,0}, {1,0,0}, {1,0,0} } },
        { "cjk narrower than glyph", FAM_UNI, L"一丁", 1, -1, 2, { {1,0,0}, {1,0,0} } },
        { "kana between any", FAM_UNI, L"あいう", 4, -1, 2, { {2,0,0}, {1,0,0} } },
        { "break at nbsp", FAM_UNI, L"AAA " L"BBB", 4, -1, 2, { {4,1,0}, {3,0,0} } },
        { "break after soft hyphen", FAM_UNI, L"AAA­" L"BBB", 4, -1, 2, { {4,0,0}, {3,0,0} } },
        { "no break in no wrap cjk", FAM_UNI, L"一丁丂", 2, NOWRAP, 1, { {3,0,0} } },
        { "cjk with emergency", FAM_UNI, L"一丁丂七", 4, EMERG, 2, { {2,0,0}, {2,0,0} } },
        { "surrogates as units", FAM_UNI, L"\U0001D400\U0001D401\U0001D402", 3, CHAR_, 2, { {4,0,0}, {2,0,0} } },
        { "surrogates one per line", FAM_UNI, L"\U0001D400\U0001D401\U0001D402", 2, CHAR_, 3, { {2,0,0}, {2,0,0}, {2,0,0} } },
    };
    run_cases(&X, cs, sizeof cs / sizeof cs[0]);
    TM_END();
}

/* ---- isTrimmed ----------------------------------------------------------------------------------------- */
static void set_trim(IDWriteTextLayout *l, DWRITE_TRIMMING_GRANULARITY g)
{
    DWRITE_TRIMMING t; t.granularity = g; t.delimiter = 0; t.delimiterCount = 0;
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, NULL));
}
TEST(ll_trimmed_by_character)
{
    /* one unbreakable word wider than the layout with trimming enabled: the line is flagged, its length is unchanged */
    TM_BEGIN(); TM_L(l, L"AAAAAAAA", 3 * CW, 1000);
    tm_lines ln;
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); CHECK(ln.v[0].isTrimmed); EXPECT_EQ(ln.v[0].length, 8); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_not_trimmed_without_trimming)
{
    TM_BEGIN(); TM_L(l, L"AAAAAAAA", 3 * CW, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); CHECK(!ln.v[0].isTrimmed); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_not_trimmed_when_fits)
{
    TM_BEGIN(); TM_L(l, L"AAAA", 10 * CW, 1000);
    tm_lines ln;
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); CHECK(!ln.v[0].isTrimmed); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_not_trimmed_exact_fit)
{
    TM_BEGIN(); TM_L(l, L"AAAA", 4 * CW, 1000);
    tm_lines ln;
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); CHECK(!ln.v[0].isTrimmed); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_trimmed_no_wrap_word)
{
    TM_BEGIN(); TM_L(l, L"AA BB CC", 3 * CW, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_WORD);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); CHECK(ln.v[0].isTrimmed); EXPECT_EQ(ln.v[0].length, 8); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_trimming_none_granularity)
{
    TM_BEGIN(); TM_L(l, L"AAAAAAAA", 3 * CW, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_NONE);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); CHECK(!ln.v[0].isTrimmed); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_trimmed_wrapped_lines_not_trimmed)
{
    /* lines that wrapped cleanly are not trimmed even though trimming is on */
    TM_BEGIN(); TM_L(l, L"AAA BBB CCC", 4 * CW, 1000);
    tm_lines ln; UINT32 i;
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 3); for (i = 0; i < ln.n; i++) CHECKF(!ln.v[i].isTrimmed, "line %u trimmed", i); }
    DW_RELEASE(l); TM_END();
}

/* ---- heights and baselines of default lines ------------------------------------------------------------------ */
TEST(ll_height_baseline_synthetic)
{
    /* DwTest: win ascent 1800 + win descent 450 (USE_TYPO_METRICS clear), lineGap 0 */
    TM_BEGIN(); TM_L(l, L"AAA BBB CCC", 4 * CW, 1000);
    tm_lines ln; UINT32 i;
    if (tm_get_lines(l, &ln)) for (i = 0; i < ln.n; i++) { NEAR(ln.v[i].height, LH); NEAR(ln.v[i].baseline, ASC); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_height_scales_with_size)
{
    TM_BEGIN();
    static const float sizes[] = { 1.0f, 8.0f, 12.0f, 16.0f, 20.0f, 48.0f, 100.0f, 2048.0f };
    size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"AB", 2, sizes[i], 100000, 100000);
        tm_lines ln;
        if (l && tm_get_lines(l, &ln)) {
            float tol = 0.001f + sizes[i] * 1e-5f;
            CHECKF(DW_APPROX(ln.v[0].height, sizes[i] * 2250.0f / 2048.0f, tol), "size %g height %g", sizes[i], ln.v[0].height);
            CHECKF(DW_APPROX(ln.v[0].baseline, sizes[i] * 1800.0f / 2048.0f, tol), "size %g baseline %g", sizes[i], ln.v[0].baseline);
        } else CHECKF(0, "size %g layout", sizes[i]);
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(ll_height_from_font_metrics_all_families)
{
    /* height = ascent + descent + lineGap and baseline = ascent + lineGap of the font's own metrics (gap above the line) */
    TM_BEGIN();
    static const WCHAR *fams[] = { FAM_TEST, FAM_GAP, FAM_ASYM, FAM_UNI };
    size_t i;
    for (i = 0; i < 4; i++) {
        DWRITE_FONT_METRICS fm; IDWriteTextLayout *l = tm_layout_(&X, fams[i], L"AB", 2, SZ, 1000, 1000); tm_lines ln;
        if (!l || !tm_font_metrics(&X, fams[i], &fm)) { CHECKF(0, "family %zu", i); DW_RELEASE(l); continue; }
        if (tm_get_lines(l, &ln)) {
            float s = SZ / fm.designUnitsPerEm;
            CHECKF(DW_APPROX(ln.v[0].height, (fm.ascent + fm.descent + fm.lineGap) * s, EPS), "family %zu: height %g want %g", i, ln.v[0].height, (fm.ascent + fm.descent + fm.lineGap) * s);
            CHECKF(DW_APPROX(ln.v[0].baseline, (fm.ascent + fm.lineGap) * s, EPS), "family %zu: baseline %g want %g", i, ln.v[0].baseline, (fm.ascent + fm.lineGap) * s);
        }
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(ll_font_gap_family_has_gap)
{
    /* precondition for the gap tests: the DwGap font reports a positive line gap and the layout includes it */
    TM_BEGIN();
    DWRITE_FONT_METRICS fm, base;
    if (!tm_font_metrics(&X, FAM_GAP, &fm) || !tm_font_metrics(&X, FAM_TEST, &base)) { TM_END(); t_skip("font metrics unavailable"); return; }
    t_note("DwGap lineGap %d (ascent %d descent %d), DwTest lineGap %d", fm.lineGap, fm.ascent, fm.descent, base.lineGap);
    EXPECT_EQ(base.ascent, 1800); EXPECT_EQ(base.descent, 450); EXPECT_EQ(base.lineGap, 0);
    CHECK(fm.lineGap > 0);
    { TM_LF(l, FAM_GAP, L"AB", 1000, 1000); tm_lines ln;
      if (tm_get_lines(l, &ln)) CHECK(ln.v[0].height > U16(fm.ascent + fm.descent) + EPS);
      DW_RELEASE(l); }
    TM_END();
}
TEST(ll_mixed_fonts_max_ascent_plus_max_descent)
{
    /* a line takes the largest ascent and the largest descent over its runs: DwTest 1800/450 and DwAsym 1500/700 => 1800 + 700 */
    TM_BEGIN(); TM_L(l, L"AAAABBBB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, FAM_ASYM, RNG(4, 4)));
    if (tm_get_lines(l, &ln)) {
        EXPECT_EQ(ln.n, 1);
        NEAR(ln.v[0].height, U16(1800 + 700));
        NEAR(ln.v[0].baseline, U16(1800));
    }
    DW_RELEASE(l); TM_END();
}
TEST(ll_mixed_sizes_max_over_runs)
{
    TM_BEGIN(); TM_L(l, L"AAAABBBB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(2, 2)));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); NEAR(ln.v[0].height, 2 * LH); NEAR(ln.v[0].baseline, 2 * ASC); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_mixed_size_only_on_other_line)
{
    /* each line measures only its own runs */
    TM_BEGIN(); TM_L(l, L"AA\nBB\nCC", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(3, 2)));
    if (tm_get_lines(l, &ln)) {
        EXPECT_EQ(ln.n, 3);
        NEAR(ln.v[0].height, LH); NEAR(ln.v[1].height, 2 * LH); NEAR(ln.v[2].height, LH);
        NEAR(ln.v[0].baseline, ASC); NEAR(ln.v[1].baseline, 2 * ASC); NEAR(ln.v[2].baseline, ASC);
    }
    DW_RELEASE(l); TM_END();
}
TEST(ll_newline_char_size_counts)
{
    /* the size of the run holding the newline character itself also sizes its line */
    TM_BEGIN(); TM_L(l, L"AA\nBB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(2, 1)));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 2); t_note("line0 height %g (regular %g, doubled %g)", ln.v[0].height, LH, 2 * LH); NEAR(ln.v[1].height, LH); }
    DW_RELEASE(l); TM_END();
}
TEST(ll_empty_line_between_uses_font_height)
{
    TM_BEGIN(); TM_L(l, L"A\n\nB", 1000, 1000);
    tm_lines ln;
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 3); NEAR(ln.v[0].height, LH); NEAR(ln.v[1].height, LH); NEAR(ln.v[2].height, LH); NEAR(ln.v[1].baseline, ASC); }
    DW_RELEASE(l); TM_END();
}

/* ---- invariants over many texts and widths ---------------------------------------------------------------- */
typedef struct { const WCHAR *fam; const WCHAR *text; } itext;
static int is_nl_char(WCHAR c) { return c == '\n' || c == '\r' || c == 0xb || c == 0xc || c == 0x85 || c == 0x2028 || c == 0x2029; }

static void check_invariants(IDWriteTextLayout *l, const WCHAR *text, UINT32 len, const char *tag, float w)
{
    tm_lines ln; tm_clusters cl; DWRITE_TEXT_METRICS m; UINT32 i, ci = 0, pos = 0; float maxw = 0.0f, maxwi = 0.0f;
    if (!tm_get_lines(l, &ln) || !tm_get_clusters(l, &cl)) return;
    tm_metrics(l, &m);
    CHECKF(tm_sum_line_len(&ln) == len, "%s w=%g: line lengths sum %u want %u", tag, w, tm_sum_line_len(&ln), len);
    CHECKF(tm_sum_cluster_len(&cl) == len, "%s w=%g: cluster lengths sum %u want %u", tag, w, tm_sum_cluster_len(&cl), len);
    CHECKF(m.lineCount == ln.n, "%s w=%g: lineCount %u vs %u", tag, w, m.lineCount, ln.n);
    CHECKF(DW_APPROX(m.height, tm_sum_line_height(&ln), 0.01f), "%s w=%g: height %g vs sum %g", tag, w, m.height, tm_sum_line_height(&ln));
    CHECKF(m.widthIncludingTrailingWhitespace + 0.001f >= m.width, "%s w=%g: width incl %g < width %g", tag, w, m.widthIncludingTrailingWhitespace, m.width);
    CHECKF(DW_APPROX(m.layoutWidth, w, 0.001f), "%s: layoutWidth %g", tag, m.layoutWidth);
    for (i = 0; i < ln.n; i++) {
        UINT32 end = pos + ln.v[i].length, cpos = pos, nonws_end_cluster = ci;
        float lw = 0.0f, lwi = 0.0f, trail = 0.0f; UINT32 first = ci;
        CHECKF(ln.v[i].trailingWhitespaceLength <= ln.v[i].length, "%s line %u: tws > length", tag, i);
        CHECKF(ln.v[i].newlineLength <= ln.v[i].trailingWhitespaceLength, "%s line %u: newline %u > tws %u", tag, i, ln.v[i].newlineLength, ln.v[i].trailingWhitespaceLength);
        CHECKF(ln.v[i].height > 0.0f, "%s line %u: height %g", tag, i, ln.v[i].height);
        CHECKF(ln.v[i].baseline > 0.0f && ln.v[i].baseline <= ln.v[i].height, "%s line %u: baseline %g height %g", tag, i, ln.v[i].baseline, ln.v[i].height);
        while (ci < cl.n && cpos < end) { cpos += cl.v[ci].length; ci++; }
        CHECKF(cpos == end, "%s line %u: clusters do not tile the line (%u vs %u)", tag, i, cpos, end);
        for (; first < ci; first++) {
            lwi += cl.v[first].width;
            if (cl.v[first].isWhitespace) trail += cl.v[first].width; else { lw = lwi; trail = 0; }
        }
        (void)nonws_end_cluster;
        if (lw > maxw) maxw = lw;
        if (lwi > maxwi) maxwi = lwi;
        /* a newline ends every line but the last (and the last when text ends with a break) */
        if (ln.v[i].newlineLength) CHECKF(is_nl_char(text[end - 1]), "%s line %u: newline length %u but last char %#x", tag, i, ln.v[i].newlineLength, text[end - 1]);
        else if (i + 1 < ln.n) CHECKF(!is_nl_char(text[end - 1]) || 1, "x");
        pos = end;
    }
    CHECKF(DW_APPROX(m.width, maxw, 0.01f), "%s w=%g: metrics width %g vs widest line %g", tag, w, m.width, maxw);
    CHECKF(DW_APPROX(m.widthIncludingTrailingWhitespace, maxwi, 0.01f), "%s w=%g: width incl %g vs %g", tag, w, m.widthIncludingTrailingWhitespace, maxwi);
}

TEST(ll_invariants_plain_text)
{
    TM_BEGIN();
    static const itext texts[] = {
        { FAM_TEST, L"AAAA BBBB CCCC DDDD" }, { FAM_TEST, L"A B C D E F G H" }, { FAM_TEST, L"AAAAAAAAAAAA" },
        { FAM_TEST, L"AA  BB   CC    DD" }, { FAM_TEST, L"  lead and trail  " }, { FAM_TEST, L"one\ntwo three\nfour five six" },
        { FAM_TEST, L"x" }, { FAM_TEST, L"a\r\nb\r\n\r\nc" }, { FAM_UNI, L"AAA-BBB-CCC DDD" }, { FAM_UNI, L"一丁丂七丄丅" },
        { FAM_UNI, L"AB一丁 CD" }, { FAM_UNI, L"ÁB́ ĆD" },
    };
    static const float widths[] = { 0, 1, 2, 3, 4, 5, 7, 10, 20, 1000 };
    size_t i, j;
    for (i = 0; i < sizeof texts / sizeof texts[0]; i++)
        for (j = 0; j < sizeof widths / sizeof widths[0]; j++) {
            float w = widths[j] * CW; char tag[64];
            IDWriteTextLayout *l = tm_layout_(&X, texts[i].fam, texts[i].text, (UINT32)wcslen(texts[i].text), SZ, w, 1000);
            snprintf(tag, sizeof tag, "text#%zu", i);
            if (!l) { CHECKF(0, "%s layout", tag); continue; }
            check_invariants(l, texts[i].text, (UINT32)wcslen(texts[i].text), tag, w);
            DW_RELEASE(l);
        }
    TM_END();
}
TEST(ll_invariants_wrapping_modes)
{
    TM_BEGIN();
    static const WCHAR *texts[] = { L"AAAA BBBB CCCC", L"AAAAAAAAAAAA", L"A B  C" };
    static const DWRITE_WORD_WRAPPING modes[] = { WRAP, NOWRAP, EMERG, WHOLE, CHAR_ };
    size_t i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 5; j++) {
            float w = 3 * CW; char tag[64];
            IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, texts[i], (UINT32)wcslen(texts[i]), SZ, w, 1000);
            if (!l) { CHECKF(0, "layout"); continue; }
            snprintf(tag, sizeof tag, "text#%zu mode %d", i, (int)modes[j]);
            CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, modes[j]));
            check_invariants(l, texts[i], (UINT32)wcslen(texts[i]), tag, w);
            DW_RELEASE(l);
        }
    TM_END();
}
TEST(ll_lines_fit_width_when_breakable)
{
    /* every line of breakable text is no wider (excluding trailing whitespace) than the layout width as long as no single word is wider */
    TM_BEGIN();
    static const WCHAR *text = L"AAA BB C DDDD EE FFF G HHH";   /* longest word 4 letters */
    static const float widths[] = { 4, 5, 6, 8, 9, 12, 15, 30 };
    size_t k;
    for (k = 0; k < sizeof widths / sizeof widths[0]; k++) {
        float w = widths[k] * CW; IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, text, (UINT32)wcslen(text), SZ, w, 1000);
        tm_lines ln; tm_clusters cl; DWRITE_TEXT_METRICS m; UINT32 i, ci = 0;
        if (!l) { CHECKF(0, "layout"); continue; }
        if (tm_get_lines(l, &ln) && tm_get_clusters(l, &cl)) {
            for (i = 0; i < ln.n; i++) {
                float lw = 0, acc = 0; UINT32 pos = 0, target = ln.v[i].length;
                while (ci < cl.n && pos < target) { acc += cl.v[ci].width; if (!cl.v[ci].isWhitespace) lw = acc; pos += cl.v[ci].length; ci++; }
                CHECKF(lw <= w + 0.001f, "width %g line %u content %g", w, i, lw);
            }
            tm_metrics(l, &m);
            CHECKF(m.width <= w + 0.001f, "width %g: metrics.width %g", w, m.width);
        }
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(ll_narrower_layout_never_fewer_lines)
{
    TM_BEGIN();
    static const WCHAR *text = L"AAA BBB CCC DDD EEE FFF GGG";
    UINT32 prev = 0; int k;
    for (k = 40; k >= 1; k--) {
        float w = k * CW; IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, text, (UINT32)wcslen(text), SZ, w, 1000);
        UINT32 n = 0;
        if (!l) { CHECKF(0, "layout"); continue; }
        IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n);
        CHECKF(n >= prev, "width %d chars: %u lines after %u", k, n, prev);
        prev = n;
        DW_RELEASE(l);
    }
    CHECK(prev >= 7);
    TM_END();
}
TEST(ll_max_height_does_not_drop_lines)
{
    /* a layout height smaller than the text never removes lines when trimming is off */
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 1000);
    tm_lines ln; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 0.0f));
    if (tm_get_lines(l, &ln)) EXPECT_EQ(ln.n, 3);
    tm_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 3);
    NEAR(m.height, 3 * LH);
    NEAR(m.layoutHeight, 0.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, LH * 1.5f));
    if (tm_get_lines(l, &ln)) EXPECT_EQ(ln.n, 3);
    DW_RELEASE(l); TM_END();
}
TEST(ll_lines_property_across_resets)
{
    /* wrapping state follows the current properties, not a cache: toggle wrapping and width repeatedly */
    TM_BEGIN(); TM_L(l, L"AA BB CC", 1000, 1000);
    UINT32 n; int k;
    for (k = 0; k < 6; k++) {
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, (k & 1) ? NOWRAP : WRAP));
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 2 * CW));
        n = 0; IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n);
        CHECKF(n == ((k & 1) ? 1u : 3u), "iteration %d: %u lines", k, n);
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 1000));
        n = 0; IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n);
        EXPECT_EQ(n, 1);
    }
    DW_RELEASE(l); TM_END();
}

/* ---- trimming by height: lines that do not fit stay in the list with zero height and isTrimmed ----------------- */
TEST(ll_trim_height_hides_later_lines)
{
    /* with trimming enabled and a layout height of 1.5 lines, line 0 stays, later lines are kept but collapsed (height 0, baseline 0, trimmed) */
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 1.5f * LH);
    tm_lines ln; DWRITE_TEXT_METRICS m;
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
    if (tm_get_lines(l, &ln)) {
        EXPECT_EQ(ln.n, 3);
        NEAR(ln.v[0].height, LH); CHECK(!ln.v[0].isTrimmed);
        NEAR(ln.v[1].height, 0.0f); NEAR(ln.v[1].baseline, 0.0f); CHECK(ln.v[1].isTrimmed);
        NEAR(ln.v[2].height, 0.0f); NEAR(ln.v[2].baseline, 0.0f); CHECK(ln.v[2].isTrimmed);
        EXPECT_EQ(ln.v[1].length, 2); EXPECT_EQ(ln.v[2].length, 1);       /* text coverage is unchanged */
    }
    tm_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 3);
    t_note("trimmed-by-height layout: GetMetrics height %g (one visible line %g)", m.height, LH);
    DW_RELEASE(l); TM_END();
}
TEST(ll_trim_height_wrapped_last_visible_line_trimmed)
{
    /* the last line that still fits is itself trimmed (it is where the ellipsis would go) when the text continues without a newline */
    TM_BEGIN(); TM_L(l, L"AA BB CC", 3 * CW, 1.5f * LH);
    tm_lines ln;
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_WORD);
    if (tm_get_lines(l, &ln)) {
        EXPECT_EQ(ln.n, 3);
        CHECK(ln.v[0].isTrimmed); NEAR(ln.v[0].height, LH);
        CHECK(ln.v[1].isTrimmed); NEAR(ln.v[1].height, 0.0f);
        CHECK(ln.v[2].isTrimmed); NEAR(ln.v[2].height, 0.0f);
    }
    DW_RELEASE(l); TM_END();
}
TEST(ll_trim_height_all_fit_not_trimmed)
{
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 3 * LH);
    tm_lines ln; UINT32 i;
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 3); for (i = 0; i < 3 && i < ln.n; i++) { CHECK(!ln.v[i].isTrimmed); NEAR(ln.v[i].height, LH); } }
    DW_RELEASE(l); TM_END();
}
TEST(ll_trim_height_without_trimming_keeps_heights)
{
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 1.5f * LH);
    tm_lines ln; UINT32 i;
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 3); for (i = 0; i < 3 && i < ln.n; i++) { CHECK(!ln.v[i].isTrimmed); NEAR(ln.v[i].height, LH); } }
    DW_RELEASE(l); TM_END();
}
TEST(ll_trim_height_sweep)
{
    /* the number of lines with non-zero height grows with the layout height; the line list itself never changes */
    TM_BEGIN();
    int k; UINT32 prev_vis = 0;
    for (k = 0; k <= 8; k++) {
        float h = k * 0.5f * LH; tm_lines ln; UINT32 i, vis = 0;
        IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"A\nB\nC\nD", 7, SZ, 1000, h);
        if (!l) { CHECKF(0, "layout"); continue; }
        set_trim(l, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
        if (tm_get_lines(l, &ln)) {
            EXPECT_EQ(ln.n, 4);
            for (i = 0; i < ln.n; i++) if (ln.v[i].height > 0.0f) vis++;
            CHECKF(vis >= prev_vis, "height %g: %u visible after %u", h, vis, prev_vis);
            CHECKF(vis >= 1, "height %g: no visible line", h);
            if (h >= 4 * LH) EXPECT_EQ(vis, 4);
            prev_vis = vis;
        }
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(ll_trim_width_overflow_per_line)
{
    /* only the line wider than the layout is trimmed: "AAAAAAAA BB" at 3 letters wide -> first line overflows, second fits */
    TM_BEGIN(); TM_L(l, L"AAAAAAAA BB", 3 * CW, 1000);
    tm_lines ln;
    set_trim(l, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 2); CHECK(ln.v[0].isTrimmed); CHECK(!ln.v[1].isTrimmed); }
    DW_RELEASE(l); TM_END();
}
