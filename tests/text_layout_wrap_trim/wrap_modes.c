/* DWRITE_WORD_WRAPPING on IDWriteTextLayout / IDWriteTextFormat and the line breaking it drives.
 *
 * Facts from the Windows 10 decompilation (DWriteTextLayout::SetWordWrapping, TextLayout::BreakLines / FindLineBreak):
 *  - SetWordWrapping accepts the values 0..4 (WRAP, NO_WRAP, EMERGENCY_BREAK, WHOLE_WORD, CHARACTER) on the base interface and
 *    rejects everything else with E_INVALIDARG; no IDWriteTextFormat1 is involved.
 *  - WRAP and EMERGENCY_BREAK break between clusters when a single unbreakable segment is wider than the line; WHOLE_WORD never
 *    does; CHARACTER treats every cluster boundary as a break opportunity; NO_WRAP never starts a new line for width.
 *  - A line always holds at least one cluster; white space at the end of a line never makes the line overflow.
 *  - Text that is empty or ends with a newline gets a final empty line.
 * Fixture: every character is WT_CW wide (see wt.h); WT_FIT(k) holds k characters, WT_CH(k) holds exactly k. */
#include "wt.h"

static const struct { DWRITE_WORD_WRAPPING w; const char *name; } g_modes[] = {
    { DWRITE_WORD_WRAPPING_WRAP, "WRAP" }, { DWRITE_WORD_WRAPPING_NO_WRAP, "NO_WRAP" }, { DWRITE_WORD_WRAPPING_EMERGENCY_BREAK, "EMERGENCY_BREAK" },
    { DWRITE_WORD_WRAPPING_WHOLE_WORD, "WHOLE_WORD" }, { DWRITE_WORD_WRAPPING_CHARACTER, "CHARACTER" } };
#define NMODES 5

/* ---- setters, getters, inheritance ------------------------------------------------------------------------------ */
TEST(wrap_default_is_wrap)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, 16.0f); WT_LAYOUT(l, L"abc", 100.0f);
    EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(t), DWRITE_WORD_WRAPPING_WRAP);
    EXPECT_EQ(IDWriteTextLayout_GetWordWrapping(l), DWRITE_WORD_WRAPPING_WRAP);
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(wrap_layout_roundtrip_all_values)
{
    WT_BEGIN(); int i; WT_LAYOUT(l, L"abc", 100.0f);
    for (i = 0; i < NMODES; i++) {
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, g_modes[i].w));
        CHECKF(IDWriteTextLayout_GetWordWrapping(l) == g_modes[i].w, "mode %s", g_modes[i].name);
    }
    DW_RELEASE(l); WT_END();
}

TEST(wrap_format_roundtrip_all_values)
{
    WT_BEGIN(); int i; IDWriteTextFormat *t = wt_format(&X, 16.0f);
    for (i = 0; i < NMODES; i++) {
        CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, g_modes[i].w));
        CHECKF(IDWriteTextFormat_GetWordWrapping(t) == g_modes[i].w, "mode %s", g_modes[i].name);
    }
    DW_RELEASE(t); WT_END();
}

TEST(wrap_invalid_values_rejected_layout)
{
    WT_BEGIN(); static const UINT32 bad[] = { 5, 6, 7, 8, 15, 16, 255, 256, 0x10000, 0x7fffffff, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    size_t i; WT_LAYOUT(l, L"abc", 100.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_WHOLE_WORD));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteTextLayout_SetWordWrapping(l, (DWRITE_WORD_WRAPPING)bad[i]);
        CHECKF(hr == E_INVALIDARG, "value 0x%x -> %s", bad[i], dw_hr_name(hr));
        EXPECT_EQ(IDWriteTextLayout_GetWordWrapping(l), DWRITE_WORD_WRAPPING_WHOLE_WORD);   /* a failing call leaves the value */
    }
    DW_RELEASE(l); WT_END();
}

TEST(wrap_invalid_values_rejected_format)
{
    WT_BEGIN(); static const UINT32 bad[] = { 5, 9, 255, 0x7fffffff, 0x80000000u, 0xffffffffu };
    size_t i; IDWriteTextFormat *t = wt_format(&X, 16.0f);
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_CHARACTER));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteTextFormat_SetWordWrapping(t, (DWRITE_WORD_WRAPPING)bad[i]);
        CHECKF(hr == E_INVALIDARG, "value 0x%x -> %s", bad[i], dw_hr_name(hr));
        EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(t), DWRITE_WORD_WRAPPING_CHARACTER);
    }
    DW_RELEASE(t); WT_END();
}

TEST(wrap_extended_values_need_no_textformat1)
{
    /* values 2..4 are valid on the IDWriteTextFormat vtable itself; the layout also exposes IDWriteTextFormat1 over the same object */
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); IDWriteTextFormat1 *f1 = NULL; IDWriteTextFormat *t = wt_format(&X, 16.0f);
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK));
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_WHOLE_WORD));
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_CHARACTER));
    CHECK_HR_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat1, (void **)&f1));
    if (f1) { CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_CHARACTER)); EXPECT_EQ(IDWriteTextFormat1_GetWordWrapping(f1), DWRITE_WORD_WRAPPING_CHARACTER); DW_RELEASE(f1); }
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(wrap_inherited_from_format)
{
    WT_BEGIN(); int i;
    for (i = 0; i < NMODES; i++) {
        IDWriteTextFormat *t = wt_format(&X, 16.0f); IDWriteTextLayout *l = NULL;
        CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, g_modes[i].w));
        CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc def", 7, t, 100.0f, 100.0f, &l));
        if (l) CHECKF(IDWriteTextLayout_GetWordWrapping(l) == g_modes[i].w, "inherited %s", g_modes[i].name);
        /* the copy is independent in both directions */
        if (l) { CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP)); EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(t), g_modes[i].w); }
        DW_RELEASE(l); DW_RELEASE(t);
    }
    WT_END();
}

TEST(wrap_gdi_compatible_layout_inherits)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, 16.0f); IDWriteTextLayout *l = NULL;
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_WHOLE_WORD));
    CHECK_HR_OK(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"abc def", 7, t, 100.0f, 100.0f, 1.0f, NULL, FALSE, &l));
    if (l) EXPECT_EQ(IDWriteTextLayout_GetWordWrapping(l), DWRITE_WORD_WRAPPING_WHOLE_WORD);
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(wrap_set_invalidates_layout)
{
    /* every setter clears the "layout valid" flag: metrics reflect the new mode on the next call */
    WT_BEGIN(); WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(3));
    EXPECT_EQ(wt_nlines(l), 3);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP)); EXPECT_EQ(wt_nlines(l), 1);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_WRAP)); EXPECT_EQ(wt_nlines(l), 3);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_WHOLE_WORD)); EXPECT_EQ(wt_nlines(l), 3);
    DW_RELEASE(l); WT_END();
}

TEST(wrap_failed_set_keeps_layout)
{
    WT_BEGIN(); WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(3));
    EXPECT_EQ(wt_nlines(l), 3);
    CHECK_HR(IDWriteTextLayout_SetWordWrapping(l, (DWRITE_WORD_WRAPPING)5), E_INVALIDARG);
    EXPECT_EQ(wt_nlines(l), 3);
    DW_RELEASE(l); WT_END();
}

/* ---- WRAP: basic tables ----------------------------------------------------------------------------------------- */
typedef struct { const WCHAR *text; float fit; int n; UINT32 len[12]; } wcase;

static void run_cases(wt_fx *X, const wcase *c, int n, DWRITE_WORD_WRAPPING mode, const char *name)
{
    int i;
    for (i = 0; i < n; i++) {
        IDWriteTextLayout *l = wt_layout_wrap(X, c[i].text, c[i].fit, mode);
        if (!l) { t_check(0, __FILE__, __LINE__, "layout", "case %d %s", i, name); continue; }
        {
            wt_lines o; int k, ok; HRESULT hr = wt_get_lines(l, &o);
            ok = SUCCEEDED(hr) && (int)o.n == c[i].n;
            for (k = 0; ok && k < c[i].n; k++) ok = o.lm[k].length == c[i].len[k];
            if (!ok) {
                char got[160] = "", tmp[16]; UINT32 q;
                for (q = 0; q < o.n && q < 20; q++) { snprintf(tmp, sizeof tmp, "%s%u", q ? "," : "", o.lm[q].length); strcat(got, tmp); }
                t_check(0, __FILE__, __LINE__, "case", "%s case %d text \"%ls\" fit %g: got %u lines {%s} want %d lines first %u", name, i, c[i].text, c[i].fit, o.n, got, c[i].n, c[i].len[0]);
            } else t_check(1, __FILE__, __LINE__, "case", NULL);
        }
        IDWriteTextLayout_Release(l);
    }
}

TEST(wrap_words_basic_table)
{
    WT_BEGIN();
    static const wcase c[] = {
        { L"aaa bbb ccc", WT_FIT(11), 1, { 11 } },
        { L"aaa bbb ccc", WT_FIT(10), 2, { 8, 3 } },
        { L"aaa bbb ccc", WT_FIT(8), 2, { 8, 3 } },
        { L"aaa bbb ccc", WT_FIT(7), 2, { 8, 3 } },
        { L"aaa bbb ccc", WT_FIT(6), 3, { 4, 4, 3 } },
        { L"aaa bbb ccc", WT_FIT(4), 3, { 4, 4, 3 } },
        { L"aaa bbb ccc", WT_FIT(3), 3, { 4, 4, 3 } },
        { L"aa bb cc dd", WT_FIT(5), 2, { 6, 5 } },
        { L"aa bb cc dd", WT_FIT(2), 4, { 3, 3, 3, 2 } },
        { L"a b c d e f", WT_FIT(3), 3, { 4, 4, 3 } },
        { L"a b c d e f", WT_FIT(1), 6, { 2, 2, 2, 2, 2, 1 } },
        { L"ab", WT_FIT(5), 1, { 2 } },
        { L"a", WT_FIT(0), 1, { 1 } },
        { L"one two three four", WT_FIT(9), 3, { 8, 6, 4 } },
    };
    run_cases(&X, c, (int)(sizeof c / sizeof c[0]), DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    WT_END();
}

TEST(wrap_multiple_spaces)
{
    WT_BEGIN();
    static const wcase c[] = {
        { L"aa   bb", WT_FIT(3), 2, { 5, 2 } },          /* the run of spaces stays at the end of line one */
        { L"aa   bb", WT_FIT(2), 2, { 5, 2 } },
        { L"aa   bb", WT_FIT(7), 1, { 7 } },
        { L"aa   bb", WT_FIT(6), 2, { 5, 2 } },
        { L"aa        bb", WT_FIT(2), 2, { 10, 2 } },
        { L"  aa", WT_FIT(3), 2, { 2, 2 } },             /* leading blanks count towards the width */
        { L"  aa", WT_FIT(4), 1, { 4 } },
    };
    run_cases(&X, c, (int)(sizeof c / sizeof c[0]), DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    WT_END();
}

TEST(wrap_trailing_whitespace_never_overflows)
{
    WT_BEGIN(); int n;
    static const WCHAR *texts[] = { L"abc     ", L"abc " , L"a                 ", L"abc" NBSP NBSP NBSP, L"ab cd        " };
    for (n = 0; n < 5; n++) {
        DWRITE_TEXT_METRICS m; wt_lines o; IDWriteTextLayout *l = wt_layout_(&X, texts[n], WT_FIT(2), WT_BIGH, WT_SIZE);
        if (!l) continue;
        wt_metrics(l, &m);
        CHECK_HR_OK(wt_get_lines(l, &o));
        if (n == 0 || n == 1 || n == 2) {
            EXPECT_EQ(o.n, 1);
            EXPECT_EQ(o.lm[0].length, wcslen(texts[n]));
        }
        /* width excludes the trailing white space, the second width includes it */
        CHECKF(m.widthIncludingTrailingWhitespace >= m.width, "case %d: %g >= %g", n, m.widthIncludingTrailingWhitespace, m.width);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(wrap_trailing_whitespace_metrics)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; wt_lines o; WT_LAYOUT(l, L"abc     ", WT_FIT(3));
    wt_metrics(l, &m); CHECK_HR_OK(wt_get_lines(l, &o));
    EXPECT_EQ(o.n, 1); EXPECT_EQ(m.lineCount, 1);
    EXPECT_EQ(o.lm[0].length, 8); EXPECT_EQ(o.lm[0].trailingWhitespaceLength, 5); EXPECT_EQ(o.lm[0].newlineLength, 0);
    CHECK_NEAR(m.width, WT_CH(3), 0.01); CHECK_NEAR(m.widthIncludingTrailingWhitespace, WT_CH(8), 0.01);
    DW_RELEASE(l); WT_END();
}

TEST(wrap_trailing_whitespace_between_lines)
{
    /* the blank that ended line one is trailing white space of that line, not leading white space of the next */
    WT_BEGIN(); wt_lines o; DWRITE_TEXT_METRICS m; WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(7));
    CHECK_HR_OK(wt_get_lines(l, &o)); wt_metrics(l, &m);
    EXPECT_EQ(o.n, 2);
    EXPECT_EQ(o.lm[0].length, 8); EXPECT_EQ(o.lm[0].trailingWhitespaceLength, 1);
    EXPECT_EQ(o.lm[1].length, 3); EXPECT_EQ(o.lm[1].trailingWhitespaceLength, 0);
    CHECK_NEAR(m.width, WT_CH(7), 0.01); CHECK_NEAR(m.widthIncludingTrailingWhitespace, WT_CH(8), 0.01);
    CHECK_NEAR(wt_x(l, 8), 0.0, 0.01);
    DW_RELEASE(l); WT_END();
}

TEST(wrap_exact_fit_fits)
{
    /* a line may be exactly as wide as the layout: overflow needs the width to be strictly greater */
    WT_BEGIN();
    static const wcase c[] = {
        { L"abc def", WT_CH(3), 2, { 4, 3 } },
        { L"abc def", WT_CH(7), 1, { 7 } },
        { L"abc def", WT_CH(6), 2, { 4, 3 } },
        { L"abcd", WT_CH(4), 1, { 4 } },
        { L"abcd", WT_CH(3), 2, { 3, 1 } },
        { L"ab cd ef", WT_CH(5), 2, { 6, 2 } },
        { L"ab cd ef", WT_CH(8), 1, { 8 } },
        { L"ab cd ef", WT_CH(7), 2, { 6, 2 } },
    };
    run_cases(&X, c, (int)(sizeof c / sizeof c[0]), DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    WT_END();
}

TEST(wrap_newlines_are_hard_breaks)
{
    WT_BEGIN(); wt_lines o;
    static const struct { const WCHAR *t; int n; UINT32 len[6]; UINT32 nl[6]; } c[] = {
        { L"ab\ncd", 2, { 3, 2 }, { 1, 0 } },
        { L"ab\r\ncd", 2, { 4, 2 }, { 2, 0 } },
        { L"ab\rcd", 2, { 3, 2 }, { 1, 0 } },
        { L"ab\n\ncd", 3, { 3, 1, 2 }, { 1, 1, 0 } },
        { L"\nab", 2, { 1, 2 }, { 1, 0 } },
        { L"ab\n", 2, { 3, 0 }, { 1, 0 } },              /* text ending with a newline gets an empty final line */
        { L"ab\r\n", 2, { 4, 0 }, { 2, 0 } },
        { L"\n", 2, { 1, 0 }, { 1, 0 } },
        { L"a" L"\x2028" L"b", 2, { 2, 1 }, { 1, 0 } },
        { L"a" L"\x2029" L"b", 2, { 2, 1 }, { 1, 0 } },
    };
    int i, k;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        IDWriteTextLayout *l = wt_layout_(&X, c[i].t, 1000.0f, WT_BIGH, WT_SIZE);
        if (!l) continue;
        CHECK_HR_OK(wt_get_lines(l, &o));
        CHECKF((int)o.n == c[i].n, "case %d: %u lines want %d", i, o.n, c[i].n);
        for (k = 0; k < c[i].n && k < (int)o.n; k++) {
            CHECKF(o.lm[k].length == c[i].len[k], "case %d line %d length %u want %u", i, k, o.lm[k].length, c[i].len[k]);
            CHECKF(o.lm[k].newlineLength == c[i].nl[k], "case %d line %d newline %u want %u", i, k, o.lm[k].newlineLength, c[i].nl[k]);
        }
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(wrap_empty_text_has_one_line)
{
    WT_BEGIN(); wt_lines o; DWRITE_TEXT_METRICS m; int i;
    for (i = 0; i < NMODES; i++) {
        IDWriteTextLayout *l = wt_layout_wrap(&X, L"", WT_FIT(3), g_modes[i].w);
        if (!l) continue;
        CHECK_HR_OK(wt_get_lines(l, &o)); wt_metrics(l, &m);
        CHECKF(o.n == 1 && o.lm[0].length == 0, "%s: lines %u", g_modes[i].name, o.n);
        CHECKF(m.lineCount == 1, "%s", g_modes[i].name);
        CHECK_NEAR(m.width, 0.0, 0.001);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

/* ---- long words: emergency breaking ------------------------------------------------------------------------------ */
TEST(wrap_long_word_per_mode)
{
    WT_BEGIN();
    static const wcase brk[] = {
        { L"abcdefghij", WT_FIT(4), 3, { 4, 4, 2 } }, { L"abcdefghij", WT_FIT(3), 4, { 3, 3, 3, 1 } },
        { L"abcdefghij", WT_FIT(9), 2, { 9, 1 } }, { L"abcdefghij", WT_FIT(10), 1, { 10 } },
        { L"abcdefghij", WT_FIT(1), 10, { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
        { L"ab cdefghij", WT_FIT(4), 3, { 3, 4, 4 } },
    };
    static const wcase whole[] = {
        { L"abcdefghij", WT_FIT(4), 1, { 10 } }, { L"abcdefghij", WT_FIT(1), 1, { 10 } },
        { L"ab cdefghij", WT_FIT(4), 2, { 3, 8 } }, { L"ab cd efghij kl", WT_FIT(4), 4, { 3, 3, 7, 2 } },
    };
    run_cases(&X, brk, 5, DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    run_cases(&X, brk, 5, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK, "EMERGENCY_BREAK");
    run_cases(&X, brk, 5, DWRITE_WORD_WRAPPING_CHARACTER, "CHARACTER");
    run_cases(&X, whole, 4, DWRITE_WORD_WRAPPING_WHOLE_WORD, "WHOLE_WORD");
    WT_END();
}

TEST(wrap_long_word_after_short_word)
{
    /* the long word moves to its own line when the short word fits, then breaks inside itself only if it still does not fit */
    WT_BEGIN();
    static const wcase c[] = {
        { L"ab cdefghij", WT_FIT(4), 3, { 3, 4, 4 } },
        { L"ab cdefghij", WT_FIT(5), 3, { 3, 5, 3 } },
        { L"ab cdefg", WT_FIT(5), 2, { 3, 5 } },
    };
    run_cases(&X, c, 3, DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    run_cases(&X, c, 3, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK, "EMERGENCY_BREAK");
    WT_END();
}

TEST(wrap_wrap_equals_emergency_without_trimming)
{
    WT_BEGIN(); int n, w;
    static const WCHAR *texts[] = { L"abcdefghij klmnop q", L"a bb ccc dddd eeeee", L"xxxxxxxxxxxxxxxxxxxx", L"ab\ncdefghijkl\nmn" };
    for (n = 0; n < 4; n++) for (w = 0; w <= 12; w += 2) {
        IDWriteTextLayout *a = wt_layout_wrap(&X, texts[n], WT_FIT(w), DWRITE_WORD_WRAPPING_WRAP);
        IDWriteTextLayout *b = wt_layout_wrap(&X, texts[n], WT_FIT(w), DWRITE_WORD_WRAPPING_EMERGENCY_BREAK);
        wt_lines la, lb; UINT32 k;
        if (a && b && SUCCEEDED(wt_get_lines(a, &la)) && SUCCEEDED(wt_get_lines(b, &lb))) {
            int same = la.n == lb.n;
            for (k = 0; same && k < la.n; k++) same = la.lm[k].length == lb.lm[k].length;
            CHECKF(same, "text %d width %d: WRAP and EMERGENCY_BREAK differ", n, w);
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    WT_END();
}

TEST(wrap_width_smaller_than_one_character)
{
    WT_BEGIN(); int i;
    static const float widths[] = { 0.0f, 0.5f, 1.0f, 15.0f, 31.0f, 31.2f };
    for (i = 0; i < (int)(sizeof widths / sizeof widths[0]); i++) {
        IDWriteTextLayout *l = wt_layout_wrap(&X, L"abcd", widths[i], DWRITE_WORD_WRAPPING_WRAP);
        if (!l) continue;
        CHECKF(wt_nlines(l) == 4, "width %g: %u lines", widths[i], wt_nlines(l));      /* a line holds at least one cluster */
        WT_LENS(l, 1, 1, 1, 1);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(wrap_width_zero_per_mode)
{
    WT_BEGIN();
    static const wcase brk[] = { { L"ab", 0.0f, 2, { 1, 1 } }, { L"abcd", 0.0f, 4, { 1, 1, 1, 1 } }, { L"ab cd", 0.0f, 4, { 1, 2, 1, 1 } } };
    static const wcase whole[] = { { L"ab", 0.0f, 1, { 2 } }, { L"abcd", 0.0f, 1, { 4 } }, { L"ab cd", 0.0f, 2, { 3, 2 } } };
    run_cases(&X, brk, 3, DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    run_cases(&X, brk, 3, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK, "EMERGENCY_BREAK");
    run_cases(&X, brk, 3, DWRITE_WORD_WRAPPING_CHARACTER, "CHARACTER");
    run_cases(&X, whole, 3, DWRITE_WORD_WRAPPING_WHOLE_WORD, "WHOLE_WORD");
    WT_END();
}

/* ---- CHARACTER ---------------------------------------------------------------------------------------------------- */
TEST(wrap_character_breaks_anywhere)
{
    WT_BEGIN();
    static const wcase c[] = {
        { L"ab cd ef", WT_FIT(4), 2, { 4, 4 } },
        { L"abcdefgh", WT_FIT(3), 3, { 3, 3, 2 } },
        { L"abc def", WT_FIT(3), 2, { 4, 3 } },
        { L"abc def", WT_FIT(5), 2, { 5, 2 } },
        { L"aaa-bbb", WT_FIT(5), 2, { 5, 2 } },
        { L"aaa" NBSP L"bbb", WT_FIT(5), 2, { 5, 2 } },
        { L"ab cd", WT_FIT(1), 4, { 1, 2, 1, 1 } },
    };
    run_cases(&X, c, (int)(sizeof c / sizeof c[0]), DWRITE_WORD_WRAPPING_CHARACTER, "CHARACTER");
    WT_END();
}

TEST(wrap_character_vs_wrap_for_words)
{
    /* the same text fills lines completely under CHARACTER, but leaves ragged ends under WHOLE_WORD */
    WT_BEGIN(); IDWriteTextLayout *a = wt_layout_wrap(&X, L"aaa bbb ccc ddd", WT_FIT(6), DWRITE_WORD_WRAPPING_CHARACTER);
    IDWriteTextLayout *b = wt_layout_wrap(&X, L"aaa bbb ccc ddd", WT_FIT(6), DWRITE_WORD_WRAPPING_WHOLE_WORD);
    if (a && b) {
        WT_LENS(a, 6, 6, 3);        /* "aaa bb" "b ccc " "ddd": lines are filled to the last cluster that fits */
        WT_LENS(b, 4, 4, 4, 3);     /* whole words only: "aaa " "bbb " "ccc " "ddd" */
    }
    DW_RELEASE(a); DW_RELEASE(b); WT_END();
}

/* ---- NO_WRAP -------------------------------------------------------------------------------------------------------- */
TEST(nowrap_single_line)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m;
    WT_LAYOUT_W(l, L"aaa bbb ccc ddd eee fff", WT_FIT(3), DWRITE_WORD_WRAPPING_NO_WRAP);
    WT_LENS(l, 23); wt_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 1);
    CHECK_NEAR(m.width, WT_CH(23), 0.01);                    /* the line is as wide as its text: it overflows the layout box */
    CHECK_NEAR(m.layoutWidth, WT_FIT(3), 0.01);
    CHECK(m.width > m.layoutWidth);
    DW_RELEASE(l); WT_END();
}

TEST(nowrap_hard_breaks_still_break)
{
    WT_BEGIN();
    WT_LAYOUT_W(l, L"aaa bbb\nccc ddd\r\neee", WT_FIT(3), DWRITE_WORD_WRAPPING_NO_WRAP);
    WT_LENS(l, 8, 9, 3);
    DW_RELEASE(l); WT_END();
}

TEST(nowrap_zero_width)
{
    WT_BEGIN(); WT_LAYOUT_W(l, L"abc def", 0.0f, DWRITE_WORD_WRAPPING_NO_WRAP); WT_LENS(l, 7); DW_RELEASE(l); WT_END();
}

TEST(nowrap_vs_wrap_line_counts)
{
    WT_BEGIN(); int i;
    for (i = 1; i <= 10; i++) {
        IDWriteTextLayout *a = wt_layout_wrap(&X, L"aa bb cc dd ee ff", WT_FIT(i), DWRITE_WORD_WRAPPING_NO_WRAP);
        IDWriteTextLayout *b = wt_layout_wrap(&X, L"aa bb cc dd ee ff", WT_FIT(i), DWRITE_WORD_WRAPPING_WRAP);
        if (a && b) { CHECKF(wt_nlines(a) == 1, "NO_WRAP width %d", i); CHECKF(wt_nlines(b) >= 1, "WRAP width %d", i); }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    WT_END();
}

TEST(nowrap_no_trailing_ws_in_width)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; WT_LAYOUT_W(l, L"abcd    ", WT_FIT(2), DWRITE_WORD_WRAPPING_NO_WRAP);
    wt_metrics(l, &m); CHECK_NEAR(m.width, WT_CH(4), 0.01); CHECK_NEAR(m.widthIncludingTrailingWhitespace, WT_CH(8), 0.01);
    DW_RELEASE(l); WT_END();
}

/* ---- break opportunities -------------------------------------------------------------------------------------------- */
TEST(wrap_break_after_hyphen)
{
    WT_BEGIN();
    static const wcase c[] = {
        { L"aaa-bbb", WT_FIT(5), 2, { 4, 3 } },
        { L"aaa-bbb ccc", WT_FIT(7), 2, { 8, 3 } },
        { L"aaa-bbb-ccc", WT_FIT(8), 2, { 8, 3 } },
        { L"aaa-bbb-ccc", WT_FIT(4), 3, { 4, 4, 3 } },
    };
    run_cases(&X, c, 4, DWRITE_WORD_WRAPPING_WHOLE_WORD, "WHOLE_WORD");
    run_cases(&X, c, 4, DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    WT_END();
}

TEST(wrap_nbsp_prevents_break)
{
    WT_BEGIN();
    static const wcase whole[] = {
        { L"aaa" NBSP L"bbb ccc", WT_FIT(5), 2, { 8, 3 } },
        { L"aaa" NBSP L"bbb ccc", WT_FIT(7), 2, { 8, 3 } },
        { L"aaa" NBSP L"bbb ccc", WT_FIT(11), 1, { 11 } },
        { L"aa" NBSP L"bb" NBSP L"cc dd", WT_FIT(6), 2, { 9, 2 } },
    };
    static const wcase emerg[] = {
        { L"aaa" NBSP L"bbb ccc", WT_FIT(5), 3, { 5, 3, 3 } },       /* too wide even alone: split inside the NBSP word */
        { L"aaa" NBSP L"bbb ccc", WT_FIT(7), 2, { 8, 3 } },
    };
    run_cases(&X, whole, 4, DWRITE_WORD_WRAPPING_WHOLE_WORD, "WHOLE_WORD");
    run_cases(&X, emerg, 2, DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    WT_END();
}

TEST(wrap_zwsp_allows_break)
{
    WT_BEGIN();
    static const wcase c[] = {
        { L"aaaa" ZWSP L"bbbbb", WT_FIT(6), 2, { 5, 5 } },
        { L"aaaa" ZWSP L"bbbbb", WT_FIT(9), 1, { 10 } },
        { L"aa" ZWSP L"bb" ZWSP L"cc", WT_FIT(2), 3, { 3, 3, 2 } },
        { L"aaaabbbbb", WT_FIT(6), 2, { 6, 3 } },
    };
    run_cases(&X, c, 3, DWRITE_WORD_WRAPPING_WHOLE_WORD, "WHOLE_WORD");
    run_cases(&X, c + 3, 1, DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    run_cases(&X, c + 3, 1, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK, "EMERGENCY_BREAK");
    WT_END();
}

TEST(wrap_cjk_breaks_between_ideographs)
{
    WT_BEGIN();
    static const wcase c[] = {
        { CJ0 CJ1 CJ2 CJ3 CJ4 CJ5, WT_FIT(4), 2, { 4, 2 } },
        { CJ0 CJ1 CJ2 CJ3 CJ4 CJ5, WT_FIT(1), 6, { 1, 1, 1, 1, 1, 1 } },
        { CJ0 CJ1 CJ2 CJ3 CJ4 CJ5, WT_FIT(3), 2, { 3, 3 } },
        { CJ0 CJ1 CJ2 CJ3 CJ4 CJ5, WT_FIT(6), 1, { 6 } },
        { HIRA_A HIRA_I KATA_A CJ0, WT_FIT(2), 2, { 2, 2 } },
    };
    int m;
    for (m = 0; m < NMODES; m++) if (g_modes[m].w != DWRITE_WORD_WRAPPING_NO_WRAP) run_cases(&X, c, 5, g_modes[m].w, g_modes[m].name);
    WT_END();
}

TEST(wrap_cjk_kinsoku_closing_not_line_start)
{
    /* closing punctuation (CL: U+3002 U+3001 U+300D) may not start a line: the break moves before the preceding ideograph */
    WT_BEGIN();
    static const wcase c[] = {
        { CJ0 CJ1 CJ2 KFULL CJ3, WT_FIT(3), 2, { 2, 3 } },
        { CJ0 CJ1 CJ2 KCOMMA CJ3, WT_FIT(3), 2, { 2, 3 } },
        { CJ0 CJ1 CJ2 KCLOSE CJ3, WT_FIT(3), 2, { 2, 3 } },
        { CJ0 CJ1 CJ2 KFULL CJ3, WT_FIT(4), 2, { 4, 1 } },
    };
    run_cases(&X, c, 4, DWRITE_WORD_WRAPPING_WHOLE_WORD, "WHOLE_WORD");
    WT_END();
}

TEST(wrap_cjk_kinsoku_opening_not_line_end)
{
    /* opening punctuation (OP: U+300C) may not end a line */
    WT_BEGIN();
    static const wcase c[] = {
        { CJ0 CJ1 KOPEN CJ2 CJ3, WT_FIT(3), 2, { 2, 3 } },
        { CJ0 CJ1 KOPEN CJ2 CJ3, WT_FIT(2), 3, { 2, 2, 1 } },       /* the opening bracket stays with CJ2: break before it */
    };
    static const wcase emerg[] = { { CJ0 CJ1 KOPEN CJ2 CJ3, WT_FIT(1), 5, { 1, 1, 1, 1, 1 } } };   /* emergency breaking ignores opportunities */
    run_cases(&X, c, 2, DWRITE_WORD_WRAPPING_WHOLE_WORD, "WHOLE_WORD");
    run_cases(&X, emerg, 1, DWRITE_WORD_WRAPPING_WRAP, "WRAP");
    WT_END();
}

TEST(wrap_latin_open_punctuation_stays_with_word)
{
    /* (OP SP* x): an opening bracket never ends a line, so "(abc)" moves as a unit */
    WT_BEGIN(); IDWriteTextLayout *l = wt_layout_wrap(&X, L"aa (bb)", WT_FIT(4), DWRITE_WORD_WRAPPING_WHOLE_WORD);
    if (l) { wt_lines o; CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 2); if (o.n == 2) { EXPECT_EQ(o.lm[0].length, 3); EXPECT_EQ(o.lm[1].length, 4); } }
    DW_RELEASE(l); WT_END();
}

/* ---- invariants against the line break analyzer ---------------------------------------------------------------------- */
TEST(wrap_invariants_whole_word_sweep)
{
    WT_BEGIN(); int n, w;
    static const WCHAR *texts[] = {
        L"the quick brown fox jumps over the lazy dog", L"a bb ccc dddd eeeee ffffff", L"aaa-bbb ccc-ddd eee", L"x, y; z. w! q? r: s",
        L"one  two   three    four", L"(alpha) [beta] {gamma}", L"abc" NBSP L"def ghi" NBSP L"jkl mno", CJ0 CJ1 CJ2 L" abc " CJ3 CJ4 L" def",
        L"12 34 5678 9 0", L"a/b c\\d e|f g", L"aa" ZWSP L"bb cc" ZWSP L"dd", L"wide  words and spaces   "
    };
    for (n = 0; n < (int)(sizeof texts / sizeof texts[0]); n++) for (w = 6; w <= 30; w += 3) {
        IDWriteTextLayout *l = wt_layout_wrap(&X, texts[n], WT_FIT(w), DWRITE_WORD_WRAPPING_WHOLE_WORD);
        if (l) { WT_CHECK_GREEDY(l, texts[n], WT_FIT(w)); IDWriteTextLayout_Release(l); }
    }
    WT_END();
}

TEST(wrap_invariants_wrap_sweep)
{
    /* with no word wider than the layout the default mode is plain greedy line filling over the analyzer's opportunities */
    WT_BEGIN(); int n, w;
    static const WCHAR *texts[] = {
        L"the quick brown fox jumps over the lazy dog", L"aaa-bbb ccc-ddd eee", L"one  two   three    four", L"a bb ccc dddd eeeee",
        L"abc" NBSP L"def ghi" NBSP L"jkl mno", CJ0 CJ1 CJ2 L" abc " CJ3 CJ4 L" def", L"aa" ZWSP L"bb cc" ZWSP L"dd ee"
    };
    for (n = 0; n < (int)(sizeof texts / sizeof texts[0]); n++) for (w = 8; w <= 24; w += 2) {
        IDWriteTextLayout *l = wt_layout_wrap(&X, texts[n], WT_FIT(w), DWRITE_WORD_WRAPPING_WRAP);
        if (l) { WT_CHECK_GREEDY(l, texts[n], WT_FIT(w)); IDWriteTextLayout_Release(l); }
    }
    WT_END();
}

TEST(wrap_line_count_monotone_in_width)
{
    WT_BEGIN(); int n, m;
    static const WCHAR *texts[] = { L"the quick brown fox jumps over the lazy dog", L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", L"ab\ncd ef gh ij\nkl", CJ0 CJ1 CJ2 CJ3 CJ4 CJ5 L"\x4e06" L"\x4e07" };
    for (n = 0; n < 4; n++) for (m = 0; m < NMODES; m++) {
        int w; UINT32 prev = 1000000;
        if (g_modes[m].w == DWRITE_WORD_WRAPPING_NO_WRAP) continue;
        for (w = 0; w <= 48; w += 2) {
            IDWriteTextLayout *l = wt_layout_wrap(&X, texts[n], (float)w * WT_CW, g_modes[m].w); UINT32 c;
            if (!l) continue;
            c = wt_nlines(l);
            CHECKF(c <= prev, "text %d %s width %d: %u lines after %u", n, g_modes[m].name, w, c, prev);
            prev = c; IDWriteTextLayout_Release(l);
        }
    }
    WT_END();
}

TEST(wrap_line_lengths_always_sum_to_text)
{
    WT_BEGIN(); int n, m, w;
    static const WCHAR *texts[] = { L"the quick brown fox", L"aaaaaaaaaaaaaaaaaaaa", L"ab\ncd ef\r\ngh\n", L"", L"   ", L"a b", L"x" ZWSP L"y z" };
    for (n = 0; n < 7; n++) for (m = 0; m < NMODES; m++) for (w = 0; w <= 24; w += 3) {
        IDWriteTextLayout *l = wt_layout_wrap(&X, texts[n], (float)w * WT_CW, g_modes[m].w); wt_lines o; UINT32 i, sum = 0;
        if (!l) continue;
        if (SUCCEEDED(wt_get_lines(l, &o))) {
            for (i = 0; i < o.n; i++) sum += o.lm[i].length;
            CHECKF(sum == wcslen(texts[n]), "text %d %s width %d: sum %u", n, g_modes[m].name, w, sum);
            CHECKF(o.n >= 1, "at least one line");
        }
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

/* ---- metrics relations -------------------------------------------------------------------------------------------- */
TEST(wrap_metrics_height_is_sum_of_lines)
{
    WT_BEGIN(); int m;
    for (m = 0; m < NMODES; m++) {
        IDWriteTextLayout *l = wt_layout_wrap(&X, L"aaa bbb ccc ddd eee", WT_FIT(7), g_modes[m].w); wt_lines o; DWRITE_TEXT_METRICS tm; UINT32 i; float sum = 0;
        if (!l) continue;
        CHECK_HR_OK(wt_get_lines(l, &o)); wt_metrics(l, &tm);
        for (i = 0; i < o.n; i++) sum += o.lm[i].height;
        CHECKF(tm.lineCount == o.n, "%s", g_modes[m].name);
        CHECK_NEAR(tm.height, sum, 0.01);
        CHECK_NEAR(tm.layoutWidth, WT_FIT(7), 0.001); CHECK_NEAR(tm.layoutHeight, WT_BIGH, 0.001);
        if (o.n) t_note("line height at size 64: %g (win ascent+descent predicts %g)", o.lm[0].height, WT_LH);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(wrap_metrics_width_is_widest_line)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; WT_LAYOUT(l, L"aa bbbb cc", WT_FIT(7));
    wt_metrics(l, &m); WT_LENS(l, 8, 2);
    CHECK_NEAR(m.width, WT_CH(7), 0.01);                         /* "aa bbbb" without the blank that follows it */
    CHECK_NEAR(m.widthIncludingTrailingWhitespace, WT_CH(8), 0.01);
    EXPECT_EQ(m.lineCount, 2);
    DW_RELEASE(l); WT_END();
}

TEST(wrap_set_max_width_rewraps)
{
    WT_BEGIN(); WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(11));
    WT_LENS(l, 11);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, WT_FIT(7))); WT_LENS(l, 8, 3);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, WT_FIT(3))); WT_LENS(l, 4, 4, 3);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, WT_FIT(20))); WT_LENS(l, 11);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_WHOLE_WORD)); CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 0.0f)); WT_LENS(l, 4, 4, 3);
    DW_RELEASE(l); WT_END();
}

TEST(wrap_hit_test_line_starts)
{
    WT_BEGIN(); wt_lines o; UINT32 i; WT_LAYOUT(l, L"aaa bbb ccc ddd", WT_FIT(7));
    CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 2);
    for (i = 0; i < o.n; i++) {
        CHECK_NEAR(wt_x(l, o.start[i]), 0.0, 0.01);
        CHECK_NEAR(wt_y(l, o.start[i]), (float)i * o.lm[0].height, 0.1);
    }
    CHECK_NEAR(wt_x(l, 4), WT_CH(4), 0.01);
    CHECK_NEAR(wt_x(l, 9), WT_CH(1), 0.01);
    DW_RELEASE(l); WT_END();
}

TEST(wrap_draw_runs_per_line)
{
    /* every line is drawn from its own baseline origin; lines are WT_LH apart */
    WT_BEGIN(); WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(3)); dwx_renderer *r = wt_draw(l, 10.0f, 20.0f);
    int cur = 0; const dwx_draw *d; float prev_y = -1; int runs = 0; wt_lines lo; float lh = 0;
    if (SUCCEEDED(wt_get_lines(l, &lo)) && lo.n) lh = lo.lm[0].height;
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
        runs++;
        CHECK_NEAR(d->x, 10.0, 0.01);
        if (prev_y >= 0) CHECK_NEAR(d->y - prev_y, lh, 0.1);
        prev_y = d->y;
    }
    CHECKF(runs >= 3, "glyph runs %d", runs);
    dwx_release(r); DW_RELEASE(l); WT_END();
}

TEST(wrap_cluster_can_wrap_after)
{
    WT_BEGIN(); DWRITE_CLUSTER_METRICS cm[16]; UINT32 n; WT_LAYOUT(l, L"ab cd", 1000.0f);
    n = wt_clusters(l, cm, 16); EXPECT_EQ(n, 5);
    if (n == 5) {
        EXPECT_EQ(cm[0].canWrapLineAfter, 0); EXPECT_EQ(cm[1].canWrapLineAfter, 0);
        EXPECT_EQ(cm[2].canWrapLineAfter, 1); EXPECT_EQ(cm[2].isWhitespace, 1);
        EXPECT_EQ(cm[3].canWrapLineAfter, 0); EXPECT_EQ(cm[0].isWhitespace, 0); EXPECT_EQ(cm[0].isNewline, 0);
        CHECK_NEAR(cm[0].width, WT_CW, 0.001); CHECK_NEAR(cm[2].width, WT_CW, 0.001);
        EXPECT_EQ(cm[0].length, 1);
    }
    DW_RELEASE(l); WT_END();
}

TEST(wrap_cluster_newline_flags)
{
    /* the CR LF pair is newline text: clusters flagged isNewline cover exactly the two characters, others none */
    WT_BEGIN(); DWRITE_CLUSTER_METRICS cm[16]; UINT32 n, i, nl = 0, total = 0; WT_LAYOUT(l, L"a\r\nb", 1000.0f);
    n = wt_clusters(l, cm, 16);
    CHECK(n >= 3 && n <= 4);
    for (i = 0; i < n; i++) { total += cm[i].length; if (cm[i].isNewline) nl += cm[i].length; }
    EXPECT_EQ(total, 4); EXPECT_EQ(nl, 2);
    if (n >= 3) { EXPECT_EQ(cm[0].isNewline, 0); EXPECT_EQ(cm[n - 1].isNewline, 0); }
    if (n == 3) t_note("CR LF forms one cluster");
    DW_RELEASE(l); WT_END();
}

TEST(wrap_cluster_metrics_buffer_protocol)
{
    WT_BEGIN(); DWRITE_CLUSTER_METRICS cm[8]; UINT32 n = 0; HRESULT hr; WT_LAYOUT(l, L"ab cd", 1000.0f);
    hr = IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &n); EXPECT_EQ(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 5);
    n = 0; hr = IDWriteTextLayout_GetClusterMetrics(l, cm, 4, &n); EXPECT_EQ(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 5);
    n = 0; hr = IDWriteTextLayout_GetClusterMetrics(l, cm, 5, &n); EXPECT_EQ(hr, S_OK); EXPECT_EQ(n, 5);
    n = 0; hr = IDWriteTextLayout_GetClusterMetrics(l, cm, 8, &n); EXPECT_EQ(hr, S_OK); EXPECT_EQ(n, 5);
    DW_RELEASE(l); WT_END();
}

TEST(wrap_line_metrics_buffer_protocol)
{
    WT_BEGIN(); DWRITE_LINE_METRICS lm[8]; UINT32 n = 0; HRESULT hr; WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(3));
    hr = IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n); EXPECT_EQ(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 3);
    n = 0; hr = IDWriteTextLayout_GetLineMetrics(l, lm, 2, &n); EXPECT_EQ(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 3);
    n = 0; hr = IDWriteTextLayout_GetLineMetrics(l, lm, 3, &n); EXPECT_EQ(hr, S_OK); EXPECT_EQ(n, 3);
    n = 0; hr = IDWriteTextLayout_GetLineMetrics(l, lm, 8, &n); EXPECT_EQ(hr, S_OK); EXPECT_EQ(n, 3);
    DW_RELEASE(l); WT_END();
}

/* ---- DetermineMinWidth ---------------------------------------------------------------------------------------------- */
TEST(minwidth_longest_unbreakable_segment)
{
    WT_BEGIN(); int i; float mw;
    static const struct { const WCHAR *t; int chars; } c[] = {
        { L"aaa bbbbb cc", 5 }, { L"a", 1 }, { L"abcdefgh", 8 }, { L"aaa-bbbbb", 5 }, { L"aa" NBSP L"bb cc", 5 }, { L"ab cd    ", 2 },
        { CJ0 CJ1 CJ2 CJ3, 1 }, { L"aaaa" ZWSP L"bbbbbbb", 7 }, { L"x   y", 1 }, { L"abc def ghij", 4 },
    };
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        IDWriteTextLayout *l = wt_layout_(&X, c[i].t, 1000.0f, WT_BIGH, WT_SIZE);
        if (!l) continue;
        mw = -1; CHECK_HR_OK(IDWriteTextLayout_DetermineMinWidth(l, &mw));
        CHECKF(fabsf(mw - WT_CH(c[i].chars)) < 0.01f, "case %d \"%ls\": %g want %g", i, c[i].t, mw, WT_CH(c[i].chars));
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(minwidth_independent_of_max_width)
{
    WT_BEGIN(); float a = -1, b = -1; WT_LAYOUT(l, L"aaa bbbbb cc", 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_DetermineMinWidth(l, &a));
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 1.0f)); CHECK_HR_OK(IDWriteTextLayout_DetermineMinWidth(l, &b));
    CHECK_FLOAT_EQ(a, b);
    DW_RELEASE(l); WT_END();
}

TEST(minwidth_layout_at_min_width_has_no_emergency_break)
{
    WT_BEGIN(); float mw = 0; const WCHAR *t = L"aaa bbbbb cc dddd";
    WT_LAYOUT(l, t, 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_DetermineMinWidth(l, &mw));
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, mw));
    WT_LENS(l, 4, 6, 3, 4);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, mw - 1.0f));        /* one DIP narrower: the five letter word must break */
    CHECKF(wt_nlines(l) > 4, "lines %u", wt_nlines(l));
    DW_RELEASE(l); WT_END();
}
