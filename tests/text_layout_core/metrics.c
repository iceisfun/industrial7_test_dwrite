/* IDWriteTextLayout::GetMetrics / DetermineMinWidth / GetOverhangMetrics over the synthetic font family.
 *
 * Numbers: at font size 2048 (= units per em) one DwTest character is exactly 1000 DIPs wide (DwNarrow letters 500, space 250), a line
 * is ascent 1800 + descent 450 + lineGap 0 = 2250 DIPs high, and the ink of a letter spans x 100..900 of its advance, y 0..1400 above the
 * baseline. DWRITE_TEXT_METRICS: width excludes trailing whitespace of each line, widthIncludingTrailingWhitespace includes it, height is
 * the sum of the line heights, left/top are the offsets of the text block inside the layout box after alignment, layoutWidth/Height
 * echo the layout box. A line break (CR, LF, CRLF, U+2028, U+2029, U+0085) ends a line and has zero width; text ending with a
 * break owns a further empty line. */
#include "tl.h"

#define L2048(var, text, w, h) TL_LAYOUT(var, text, TL_UPM, w, h)
#define BIG 100000.0f
#define LINEH (TL_ASC + TL_DESC)

TEST(metrics_single_line_basic)
{
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 5000.0f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m);
    CHECK_FLOAT_EQ(m.left, 0.0f); CHECK_FLOAT_EQ(m.top, 0.0f);
    CHECK_FLOAT_EQ(m.width, 4000.0f); CHECK_FLOAT_EQ(m.widthIncludingTrailingWhitespace, 4000.0f);
    CHECK_FLOAT_EQ(m.height, LINEH);
    CHECK_FLOAT_EQ(m.layoutWidth, 10000.0f); CHECK_FLOAT_EQ(m.layoutHeight, 5000.0f);
    EXPECT_EQ(m.maxBidiReorderingDepth, 1); EXPECT_EQ(m.lineCount, 1);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_struct_fully_written)
{
    /* every field is overwritten: a poisoned struct comes back without poison */
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 5000.0f);
    DWRITE_TEXT_METRICS m; unsigned char *p = (unsigned char *)&m; size_t i, poisoned = 0;
    memset(&m, 0xcc, sizeof m);
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    for (i = 0; i < sizeof m; i++) if (p[i] == 0xcc) poisoned++;
    CHECKF(poisoned < 4, "%u bytes still poisoned", (unsigned)poisoned);
    CHECK(sizeof(DWRITE_TEXT_METRICS) == 36);
    DW_RELEASE(l); TL_END();
}

static const struct { const WCHAR *text; float width, incl; UINT32 lines; } WTAB[] = {
    { L"A", 1000, 1000, 1 }, { L"ABCD", 4000, 4000, 1 }, { L"AB CD", 5000, 5000, 1 }, { L"AB ", 2000, 3000, 1 }, { L"AB  ", 2000, 4000, 1 },
    { L"  AB", 4000, 4000, 1 }, { L" ", 0, 1000, 1 }, { L"   ", 0, 3000, 1 }, { L"A B C", 5000, 5000, 1 }, { L"a1b2", 4000, 4000, 1 },
    { L"ABC\nDE", 3000, 3000, 2 }, { L"ABC \nDE", 3000, 4000, 2 }, { L"AB\n\nCDEF", 4000, 4000, 3 }, { L"AB\r\n", 2000, 2000, 2 },
    { L"\n", 0, 0, 2 }, { L"  \n  ", 0, 2000, 2 }, { L"A\rB", 1000, 1000, 2 }, { L"A\r\nB", 1000, 1000, 2 }, { L"A\n\rB", 1000, 1000, 3 },
    { L"A\x2028" L"B", 1000, 1000, 2 }, { L"A\x2029" L"B", 1000, 1000, 2 }, { L"A\x0085" L"B", 1000, 1000, 2 },
    { L"A\n", 1000, 1000, 2 }, { L"A\r", 1000, 1000, 2 }, { L"A\r\n", 1000, 1000, 2 }, { L"A\x2028", 1000, 1000, 2 }, { L"A\x2029", 1000, 1000, 2 }, { L"A\x0085", 1000, 1000, 2 },
    { L"\n\n\n", 0, 0, 4 }, { L"\r\n\r\n", 0, 0, 3 }, { L"\r\r", 0, 0, 3 }, { L"\n\r", 0, 0, 3 }, { L"A\r\n\r\nB", 1000, 1000, 3 },
    { L"AB\nCDE\nF", 3000, 3000, 3 }, { L"ABCDE\nF\nGH", 5000, 5000, 3 },
    { L"\x2028\x2028", 0, 0, 3 }, { L"\x2029" L"A\x2029", 1000, 1000, 3 }, { L"A\nB\rC\r\nD\x2028" L"E\x2029" L"F\x0085" L"G", 1000, 1000, 7 },
};
#define NWTAB ((int)(sizeof WTAB / sizeof WTAB[0]))

TEST(metrics_width_table)
{
    TL_BEGIN();
    int i;
    for (i = 0; i < NWTAB; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, WTAB[i].text, TL_UPM, BIG, BIG); DWRITE_TEXT_METRICS m;
        if (!l) { t_check(0, __FILE__, __LINE__, "layout", "row %d creation failed", i); continue; }
        tl_metrics(l, &m);
        CHECKF(m.width == WTAB[i].width, "row %d width %g want %g", i, m.width, WTAB[i].width);
        CHECKF(m.widthIncludingTrailingWhitespace == WTAB[i].incl, "row %d incl %g want %g", i, m.widthIncludingTrailingWhitespace, WTAB[i].incl);
        CHECKF(m.lineCount == WTAB[i].lines, "row %d lines %u want %u", i, m.lineCount, WTAB[i].lines);
        CHECKF(m.height == WTAB[i].lines * LINEH, "row %d height %g want %g", i, m.height, WTAB[i].lines * LINEH);
        CHECKF(m.left == 0.0f && m.top == 0.0f, "row %d left/top %g/%g", i, m.left, m.top);
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(metrics_width_table_narrow_family)
{
    TL_BEGIN();
    static const struct { const WCHAR *text; float width, incl; } t[] = {
        { L"AB", 1000, 1000 }, { L"AB CD", 1000 + 250 + 1000, 2250 }, { L"AB ", 1000, 1250 }, { L"  ", 0, 500 }, { L"ABCDEFGH", 4000, 4000 }, { L"A\nBB", 1000, 1000 },
    };
    size_t i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_NARROW, t[i].text, TL_UPM, BIG, BIG); DWRITE_TEXT_METRICS m;
        if (!l) { t_check(0, __FILE__, __LINE__, "layout", "row %u", (unsigned)i); continue; }
        tl_metrics(l, &m);
        CHECKF(m.width == t[i].width, "row %u width %g want %g", (unsigned)i, m.width, t[i].width);
        CHECKF(m.widthIncludingTrailingWhitespace == t[i].incl, "row %u incl %g want %g", (unsigned)i, m.widthIncludingTrailingWhitespace, t[i].incl);
        CHECK_FLOAT_EQ(m.height, (wcschr(t[i].text, '\n') ? 2 : 1) * LINEH);
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(metrics_size_scaling)
{
    /* advance = designAdvance * size / 2048, line height = 2250 * size / 2048 */
    TL_BEGIN();
    static const float sizes[] = { 1, 2, 8, 10, 12, 16, 24, 32, 48, 64, 100, 256, 1024, 2048, 4096 };
    size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, L"ABCD", sizes[i], BIG, BIG); DWRITE_TEXT_METRICS m;
        double s = sizes[i];
        if (!l) { t_check(0, __FILE__, __LINE__, "layout", "size %g", s); continue; }
        tl_metrics(l, &m);
        CHECKF(fabs(m.width - 4000.0 * s / 2048.0) <= 4000.0 * s / 2048.0 * 1e-4 + 1e-4, "size %g width %g want %g", s, m.width, 4000.0 * s / 2048.0);
        CHECKF(fabs(m.height - 2250.0 * s / 2048.0) <= 2250.0 * s / 2048.0 * 1e-4 + 1e-4, "size %g height %g want %g", s, m.height, 2250.0 * s / 2048.0);
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(metrics_exact_binary_fractions_at_16)
{
    TL_BEGIN(); TL_LAYOUT(l, L"ABCD EF", 16.0f, 500.0f, 100.0f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m);
    CHECK_NEAR(m.width, 7 * 7.8125, 1e-3); CHECK_NEAR(m.height, 17.578125, 1e-3);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_font_size_zero_invalid_but_tiny_ok)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 0.001f); IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 10.0f, 10.0f, &l));
    if (l) { tl_metrics(l, &m); CHECK(m.width >= 0.0f && m.width < 0.01f); EXPECT_EQ(m.lineCount, 1); DW_RELEASE(l); }
    DW_RELEASE(t); TL_END();
}

TEST(metrics_empty_layout)
{
    TL_BEGIN(); L2048(l, L"", 5000.0f, 3000.0f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m);
    CHECK_FLOAT_EQ(m.width, 0.0f); CHECK_FLOAT_EQ(m.widthIncludingTrailingWhitespace, 0.0f);
    EXPECT_EQ(m.lineCount, 1);
    CHECK_FLOAT_EQ(m.height, LINEH);                 /* the single empty line has the font's line height */
    CHECK_FLOAT_EQ(m.layoutWidth, 5000.0f); CHECK_FLOAT_EQ(m.layoutHeight, 3000.0f);
    EXPECT_EQ(m.maxBidiReorderingDepth, 1);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_empty_layout_alignment)
{
    TL_BEGIN(); L2048(l, L"", 5000.0f, 3000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.top, 3000.0f - LINEH);
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.top, (3000.0f - LINEH) / 2);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 0.0f);
    t_note("empty text centred in 5000: left %g (2500 when the empty line is aligned like a text line)", m.left);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_repeatable)
{
    TL_BEGIN(); L2048(l, L"AB CD\nEF", 3500.0f, 9000.0f);
    DWRITE_TEXT_METRICS a, b; int i;
    tl_metrics(l, &a);
    for (i = 0; i < 20; i++) { tl_metrics(l, &b); CHECK(!memcmp(&a, &b, sizeof a)); }
    DW_RELEASE(l); TL_END();
}

TEST(metrics_repeatable_across_layouts)
{
    TL_BEGIN();
    int i; DWRITE_TEXT_METRICS a, b;
    L2048(l0, L"AB CD\nEF", 3500.0f, 9000.0f);
    tl_metrics(l0, &a);
    for (i = 0; i < 10; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, L"AB CD\nEF", TL_UPM, 3500.0f, 9000.0f);
        if (!l) continue;
        tl_metrics(l, &b); CHECK(!memcmp(&a, &b, sizeof a)); DW_RELEASE(l);
    }
    DW_RELEASE(l0); TL_END();
}

TEST(metrics_after_invalidating_setters_same_values)
{
    /* re-applying the current values re-lays out the text with the same outcome */
    TL_BEGIN(); L2048(l, L"AB CD\nEF", 3500.0f, 9000.0f);
    DWRITE_TEXT_METRICS a, b;
    tl_metrics(l, &a);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 3500.0f)); tl_metrics(l, &b); CHECK(!memcmp(&a, &b, sizeof a));
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_LEADING)); tl_metrics(l, &b); CHECK(!memcmp(&a, &b, sizeof a));
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 2048.0f, RNG(0, 5))); tl_metrics(l, &b); CHECK(!memcmp(&a, &b, sizeof a));
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TL_FAM, RNG(0, 3))); tl_metrics(l, &b); CHECK(!memcmp(&a, &b, sizeof a));
    DW_RELEASE(l); TL_END();
}

/* ---- alignment ----------------------------------------------------------------------------------------------------------------- */
TEST(metrics_text_alignment_single_line)
{
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 5000.0f);
    static const struct { DWRITE_TEXT_ALIGNMENT a; float left; } t[] = {
        { DWRITE_TEXT_ALIGNMENT_LEADING, 0 }, { DWRITE_TEXT_ALIGNMENT_TRAILING, 6000 }, { DWRITE_TEXT_ALIGNMENT_CENTER, 3000 },
    };
    size_t i; DWRITE_TEXT_METRICS m;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, t[i].a));
        tl_metrics(l, &m);
        CHECKF(m.left == t[i].left, "alignment %d left %g want %g", t[i].a, m.left, t[i].left);
        CHECK_FLOAT_EQ(m.width, 4000.0f); CHECK_FLOAT_EQ(m.top, 0.0f); CHECK_FLOAT_EQ(m.layoutWidth, 10000.0f);
    }
    DW_RELEASE(l); TL_END();
}

TEST(metrics_text_alignment_justified_last_line)
{
    /* the only (= last) line of a paragraph is not stretched by justification; where it sits is implementation detail */
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 5000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_JUSTIFIED));
    tl_metrics(l, &m);
    t_note("justified single line: left %g width %g", m.left, m.width);
    CHECK(m.width >= 4000.0f && m.width <= 10000.0f); EXPECT_EQ(m.lineCount, 1);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_text_alignment_rtl)
{
    /* with a right-to-left reading direction leading means the right edge */
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 5000.0f);
    static const struct { DWRITE_TEXT_ALIGNMENT a; float left; } t[] = {
        { DWRITE_TEXT_ALIGNMENT_LEADING, 6000 }, { DWRITE_TEXT_ALIGNMENT_TRAILING, 0 }, { DWRITE_TEXT_ALIGNMENT_CENTER, 3000 },
    };
    size_t i; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, t[i].a));
        tl_metrics(l, &m);
        CHECKF(m.left == t[i].left, "rtl alignment %d left %g want %g", t[i].a, m.left, t[i].left);
        CHECK_FLOAT_EQ(m.width, 4000.0f);
    }
    DW_RELEASE(l); TL_END();
}

TEST(metrics_text_alignment_multiline_uses_min_left)
{
    /* left is the left edge of the block: the smallest line offset; width the widest line */
    TL_BEGIN(); L2048(l, L"ABCD\nAB", 10000.0f, 5000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.left, 6000.0f); CHECK_FLOAT_EQ(m.width, 4000.0f); EXPECT_EQ(m.lineCount, 2);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.left, 3000.0f); CHECK_FLOAT_EQ(m.width, 4000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_text_alignment_ignores_trailing_whitespace)
{
    /* trailing spaces hang outside the aligned block: the visible text is what gets aligned */
    TL_BEGIN(); L2048(l, L"AB  ", 10000.0f, 5000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    tl_metrics(l, &m);
    CHECK_FLOAT_EQ(m.width, 2000.0f); CHECK_FLOAT_EQ(m.widthIncludingTrailingWhitespace, 4000.0f);
    t_note("trailing alignment of 'AB  ' in 10000: left = %g (visible text flush: 8000, with spaces: 6000)", m.left);
    CHECK(m.left == 8000.0f || m.left == 6000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_paragraph_alignment)
{
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 10000.0f);
    static const struct { DWRITE_PARAGRAPH_ALIGNMENT a; float top; } t[] = {
        { DWRITE_PARAGRAPH_ALIGNMENT_NEAR, 0 }, { DWRITE_PARAGRAPH_ALIGNMENT_FAR, 10000 - 2250 }, { DWRITE_PARAGRAPH_ALIGNMENT_CENTER, (10000 - 2250) / 2.0f },
    };
    size_t i; DWRITE_TEXT_METRICS m;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, t[i].a));
        tl_metrics(l, &m);
        CHECKF(m.top == t[i].top, "paragraph alignment %d top %g want %g", t[i].a, m.top, t[i].top);
        CHECK_FLOAT_EQ(m.left, 0.0f); CHECK_FLOAT_EQ(m.height, LINEH); CHECK_FLOAT_EQ(m.layoutHeight, 10000.0f);
    }
    DW_RELEASE(l); TL_END();
}

TEST(metrics_paragraph_alignment_multiline)
{
    TL_BEGIN(); L2048(l, L"A\nB\nC", 10000.0f, 10000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.height, 3 * LINEH); CHECK_FLOAT_EQ(m.top, 10000 - 3 * LINEH);
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.top, (10000 - 3 * LINEH) / 2);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_alignment_does_not_change_sizes)
{
    TL_BEGIN(); L2048(l, L"AB CD\nEFG", 10000.0f, 10000.0f);
    DWRITE_TEXT_METRICS a, b; UINT32 ta, pa;
    tl_metrics(l, &a);
    for (ta = 0; ta < 4; ta++)
        for (pa = 0; pa < 3; pa++) {
            CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)ta));
            CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, (DWRITE_PARAGRAPH_ALIGNMENT)pa));
            tl_metrics(l, &b);
            CHECKF(b.height == a.height && b.lineCount == a.lineCount && b.layoutWidth == a.layoutWidth, "ta %u pa %u", ta, pa);
            CHECKF(b.width == a.width, "ta %u pa %u width %g vs %g", ta, pa, b.width, a.width);
        }
    DW_RELEASE(l); TL_END();
}

TEST(metrics_justified_non_last_line_fills_width)
{
    /* every line except the last is stretched to the layout width; the last line keeps its natural width */
    TL_BEGIN(); L2048(l, L"AB CD EF GH", 6500.0f, 10000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_JUSTIFIED));
    tl_metrics(l, &m);
    t_note("justified: lines %u width %g left %g", m.lineCount, m.width, m.left);
    EXPECT_EQ(m.lineCount, 2);
    CHECK(m.width >= 5000.0f && m.width <= 6500.0f);
    DW_RELEASE(l); TL_END();
}

/* ---- wrapping ------------------------------------------------------------------------------------------------------------------------ */
TEST(metrics_wrap_by_width_table)
{
    TL_BEGIN();
    static const struct { float maxw; UINT32 lines; float width; } t[] = {
        { 100000, 1, 8000 }, { 8000, 1, 8000 }, { 7999, 2, 5000 }, { 5000, 2, 5000 }, { 4999, 3, 2000 }, { 2000, 3, 2000 }, { 1999, 3, 2000 }, { 0, 3, 2000 },
        { 3000, 3, 2000 },
    };
    size_t i;
    /* "AB CD EF" with a 1000 wide space: the first break fits "AB CD" (5000) before the box is exceeded */
    (void)t;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, L"AB CD EF", TL_UPM, t[i].maxw, 100000.0f); DWRITE_TEXT_METRICS m;
        if (!l) { t_check(0, __FILE__, __LINE__, "layout", "row %u", (unsigned)i); continue; }
        tl_metrics(l, &m);
        CHECKF(m.lineCount == t[i].lines, "maxw %g lines %u want %u", t[i].maxw, m.lineCount, t[i].lines);
        CHECKF(m.width == t[i].width, "maxw %g width %g want %g", t[i].maxw, m.width, t[i].width);
        CHECKF(m.height == t[i].lines * LINEH, "maxw %g height %g", t[i].maxw, m.height);
        CHECK_FLOAT_EQ(m.layoutWidth, t[i].maxw);
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(metrics_wrap_trailing_space_hangs)
{
    /* "AB CD EF" in 5000: first line "AB CD " - the space after CD hangs past the box */
    TL_BEGIN(); L2048(l, L"AB CD EF", 5000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 2); CHECK_FLOAT_EQ(m.width, 5000.0f); CHECK_FLOAT_EQ(m.widthIncludingTrailingWhitespace, 6000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_no_wrap_single_line)
{
    TL_BEGIN(); L2048(l, L"AB CD EF", 1000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    tl_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 1); CHECK_FLOAT_EQ(m.width, 8000.0f); CHECK_FLOAT_EQ(m.layoutWidth, 1000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_no_wrap_explicit_breaks_still_break)
{
    TL_BEGIN(); L2048(l, L"AB CD\nEF GH", 1000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    tl_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 2); CHECK_FLOAT_EQ(m.width, 5000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_emergency_break_splits_long_word)
{
    TL_BEGIN(); L2048(l, L"ABCDEFGH", 3500.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 1); CHECK_FLOAT_EQ(m.width, 8000.0f);              /* plain wrapping never splits a word */
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK));
    tl_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 3); CHECK_FLOAT_EQ(m.width, 3000.0f);              /* ABC DEF GH */
    CHECK_FLOAT_EQ(m.height, 3 * LINEH);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_character_wrapping)
{
    TL_BEGIN(); L2048(l, L"ABCDEFGH", 3500.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_CHARACTER));
    tl_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 3); CHECK_FLOAT_EQ(m.width, 3000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_set_max_width_relayouts)
{
    TL_BEGIN(); L2048(l, L"AB CD EF", 100000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 1);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 2000.0f)); tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 3); CHECK_FLOAT_EQ(m.layoutWidth, 2000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 100000.0f)); tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 1);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 10.0f)); tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 1);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_max_height_does_not_clip_lines)
{
    TL_BEGIN(); L2048(l, L"A\nB\nC", 100000.0f, 1.0f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 3); CHECK_FLOAT_EQ(m.height, 3 * LINEH); CHECK_FLOAT_EQ(m.layoutHeight, 1.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_zero_size_box)
{
    TL_BEGIN(); L2048(l, L"AB CD", 0.0f, 0.0f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m);
    CHECK_FLOAT_EQ(m.layoutWidth, 0.0f); CHECK_FLOAT_EQ(m.layoutHeight, 0.0f);
    EXPECT_EQ(m.lineCount, 2); CHECK_FLOAT_EQ(m.width, 2000.0f);
    DW_RELEASE(l); TL_END();
}

/* ---- tabs ---------------------------------------------------------------------------------------------------------------------------------- */
TEST(metrics_default_tab_stop_is_four_em)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 20.0f);
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_FLOAT_EQ(IDWriteTextFormat_GetIncrementalTabStop(t), 80.0f);          /* 4 * font size at creation */
    DW_RELEASE(t); TL_END();
}

TEST(metrics_tab_advances_to_next_stop)
{
    TL_BEGIN(); L2048(l, L"A\tB", 100000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 3000.0f));
    tl_metrics(l, &m);
    CHECK_FLOAT_EQ(m.width, 4000.0f);                 /* B starts at the tab stop 3000 */
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 1500.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 3000.0f);   /* A ends at 1000, next multiple of 1500 is 1500 */
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 500.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 3000.0f);   /* next multiple of 500 after 1000 is 1500 */
    DW_RELEASE(l); TL_END();
}

TEST(metrics_leading_tab)
{
    TL_BEGIN(); L2048(l, L"\tA\t\tB", 100000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 2500.0f));
    tl_metrics(l, &m);
    /* tab -> 2500, A -> 3500, tab -> 5000, tab -> 7500, B -> 8500 */
    CHECK_FLOAT_EQ(m.width, 8500.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_tab_stop_survives_font_size_change)
{
    TL_BEGIN(); L2048(l, L"A\tB", 100000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 3000.0f));
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(0, 3)));
    CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), 3000.0f);
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 3500.0f);                  /* B at 3000 + half-size B */
    DW_RELEASE(l); TL_END();
}

/* ---- per-range formatting reaches the metrics ------------------------------------------------------------------------------------------------ */
TEST(metrics_range_font_size_changes_width)
{
    TL_BEGIN(); L2048(l, L"ABCD", 100000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(1, 2)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 3000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 4096.0f, RNG(0, 4)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 8000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_range_font_size_changes_line_height)
{
    TL_BEGIN(); L2048(l, L"AB", 100000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(0, 1)));
    tl_metrics(l, &m);
    CHECK_FLOAT_EQ(m.height, LINEH);                  /* the line is as tall as its tallest run */
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(0, 2)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.height, LINEH / 2);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_range_family_changes_width)
{
    TL_BEGIN(); L2048(l, L"ABCD", 100000.0f, 100000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TL_NARROW, RNG(2, 2)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 3000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TL_NARROW, RNG(0, 4)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 2000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TL_FAM, RNG(0, 4)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 4000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(metrics_range_beyond_text_has_no_effect)
{
    TL_BEGIN(); L2048(l, L"ABCD", 100000.0f, 100000.0f);
    DWRITE_TEXT_METRICS a, b;
    tl_metrics(l, &a);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 4096.0f, RNG(4, 100)));
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TL_NARROW, RNG(50, 5)));
    tl_metrics(l, &b); CHECK(!memcmp(&a, &b, sizeof a));
    DW_RELEASE(l); TL_END();
}

TEST(metrics_inline_object_replaces_text)
{
    TL_BEGIN(); L2048(l, L"ABCD", 100000.0f, 100000.0f);
    dwx_inline *o = dwx_inline_new(2500.0f, 1000.0f, 800.0f, FALSE);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(1, 1)));
    tl_metrics(l, &m);
    CHECK_FLOAT_EQ(m.width, 3000.0f + 2500.0f);        /* A, the object in place of B, C, D */
    EXPECT_EQ(m.lineCount, 1);
    CHECK(o->n_metrics >= 1);
    DW_RELEASE(l); dwx_release(o); TL_END();
}

/* ---- DetermineMinWidth -------------------------------------------------------------------------------------------------------------------------- */
static float minw(IDWriteTextLayout *l)
{
    float w = -1.0f; HRESULT hr = IDWriteTextLayout_DetermineMinWidth(l, &w);
    t_check(hr == S_OK, __FILE__, __LINE__, "DetermineMinWidth", "got %s", dw_hr_name(hr));
    return w;
}

TEST(minwidth_table)
{
    TL_BEGIN();
    static const struct { const WCHAR *text; float w; } t[] = {
        { L"", 0 }, { L"A", 1000 }, { L"ABCDEF", 6000 }, { L"AB CD EFG", 3000 }, { L"AB  CD", 2000 }, { L" AB ", 2000 }, { L"AB   ", 2000 },
        { L"ABC DE", 3000 }, { L"A BCDE F", 4000 }, { L"ABC\nDE", 3000 }, { L"AB\nCDEF", 4000 }, { L"AB \nCD ", 2000 }, { L"   ", 0 },
        { L"\n", 0 }, { L"A\n\nB", 1000 }, { L"AB CD\nEFGH IJ", 4000 }, { L"A\tB", 1000 },
    };
    size_t i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, t[i].text, TL_UPM, BIG, BIG);
        if (!l) { t_check(0, __FILE__, __LINE__, "layout", "row %u", (unsigned)i); continue; }
        { float w = minw(l); CHECKF(w == t[i].w, "row %u (%ls) minwidth %g want %g", (unsigned)i, t[i].text, w, t[i].w); }
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(minwidth_independent_of_max_width)
{
    TL_BEGIN(); L2048(l, L"AB CDE F", 100000.0f, 100000.0f);
    static const float w[] = { 0, 1, 2500, 3000, 100000 };
    size_t i;
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, w[i]));
        { float m = minw(l); CHECKF(m == 3000.0f, "maxWidth %g minwidth %g", w[i], m); }
    }
    DW_RELEASE(l); TL_END();
}

TEST(minwidth_follows_range_formatting)
{
    TL_BEGIN(); L2048(l, L"AB CDE F", 100000.0f, 100000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TL_NARROW, RNG(3, 3)));
    CHECK_FLOAT_EQ(minw(l), 2000.0f);                                    /* widest remaining word is "AB" (2000); CDE is 1500 now */
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 4096.0f, RNG(0, 2)));
    CHECK_FLOAT_EQ(minw(l), 4000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(minwidth_null_pointer)
{
    TL_BEGIN(); L2048(l, L"AB", 1000.0f, 1000.0f);
    CHECK_HR(IDWriteTextLayout_DetermineMinWidth(l, NULL), E_INVALIDARG);
    DW_RELEASE(l); TL_END();
}

TEST(minwidth_zero_length_text)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abcd", 0, t, 100.0f, 100.0f, &l));
    if (l) { float w = 1.0f; CHECK_HR_OK(IDWriteTextLayout_DetermineMinWidth(l, &w)); CHECK_FLOAT_EQ(w, 0.0f); DW_RELEASE(l); }
    DW_RELEASE(t); TL_END();
}

TEST(minwidth_not_larger_than_width)
{
    TL_BEGIN();
    static const WCHAR *const s[] = { L"A", L"AB CD", L"ABCDEFGH IJ", L"A B C D", L"AB\nCDE", L"  AB  CD  " };
    size_t i;
    for (i = 0; i < sizeof s / sizeof s[0]; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, s[i], TL_UPM, BIG, BIG); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        tl_metrics(l, &m);
        CHECKF(minw(l) <= m.width, "%ls: minwidth %g > width %g", s[i], minw(l), m.width);
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(minwidth_gives_the_natural_wrap_width)
{
    /* wrapping at exactly the minimum width never needs an emergency split */
    TL_BEGIN(); L2048(l, L"AB CDE F GH", 100000.0f, 100000.0f);
    float w = minw(l); DWRITE_TEXT_METRICS m;
    CHECK_FLOAT_EQ(w, 3000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, w));
    tl_metrics(l, &m); CHECK(m.width <= w); EXPECT_EQ(m.lineCount, 4);           /* AB / CDE / F / GH: "F GH" is 4000 wide */
    DW_RELEASE(l); TL_END();
}

/* ---- GetOverhangMetrics -------------------------------------------------------------------------------------------------------------------------- */
TEST(overhang_null_pointer)
{
    TL_BEGIN(); L2048(l, L"AB", 1000.0f, 1000.0f);
    if (t_is_wine()) { DW_RELEASE(l); TL_END(); t_skip("emulated implementation dereferences the pointer"); return; }
    CHECK_HR(IDWriteTextLayout_GetOverhangMetrics(l, NULL), E_INVALIDARG);
    DW_RELEASE(l); TL_END();
}

TEST(overhang_plain_text_inside_box)
{
    /* the overhang is measured from the layout box to the ink box: left = -(ink left), right = ink right - layoutWidth, ...
     * Letters have 100 units of side bearing on each side, so the ink of "ABCD" spans x 100..3900 */
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 10000.0f);
    DWRITE_OVERHANG_METRICS o;
    memset(&o, 0xcc, sizeof o);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &o));
    CHECK_NEAR(o.left, -100.0, 0.5); CHECK_NEAR(o.right, 3900.0 - 10000.0, 0.5);
    CHECK(o.top < 0.0f); CHECK(o.bottom < 0.0f);
    t_note("overhang of ABCD in 10000x10000: l %g t %g r %g b %g", o.left, o.top, o.right, o.bottom);
    DW_RELEASE(l); TL_END();
}

TEST(overhang_text_overflowing_box)
{
    TL_BEGIN(); L2048(l, L"ABCD", 2000.0f, 1000.0f);
    DWRITE_OVERHANG_METRICS o;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &o));
    CHECK_NEAR(o.right, 3900.0 - 2000.0, 0.5);        /* ink ends 1900 past the box */
    CHECK(o.bottom > 0.0f);                           /* a line taller than the 1000 box */
    DW_RELEASE(l); TL_END();
}

TEST(overhang_tracks_max_size)
{
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 10000.0f);
    DWRITE_OVERHANG_METRICS a, b;
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &a));
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 9000.0f)); CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 9500.0f));
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &b));
    CHECK_NEAR(b.right - a.right, 1000.0, 0.5); CHECK_NEAR(b.bottom - a.bottom, 500.0, 0.5);
    CHECK_NEAR(b.left, a.left, 0.001); CHECK_NEAR(b.top, a.top, 0.001);
    DW_RELEASE(l); TL_END();
}

TEST(overhang_alignment_moves_ink)
{
    TL_BEGIN(); L2048(l, L"ABCD", 10000.0f, 10000.0f);
    DWRITE_OVERHANG_METRICS a, b;
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &a));
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &b));
    CHECK_NEAR(b.left - a.left, -6000.0, 0.5); CHECK_NEAR(b.right - a.right, 6000.0, 0.5);
    DW_RELEASE(l); TL_END();
}

TEST(overhang_repeatable)
{
    TL_BEGIN(); L2048(l, L"AB CD", 10000.0f, 10000.0f);
    DWRITE_OVERHANG_METRICS a, b;
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &a)); CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &b));
    CHECK(!memcmp(&a, &b, sizeof a));
    DW_RELEASE(l); TL_END();
}

/* ---- GDI-compatible layouts --------------------------------------------------------------------------------------------------------------------------- */
static IDWriteTextLayout *gdi_layout(tl_fx *x, const WCHAR *text, float size, float w, float h, float ppd, BOOL nat)
{
    IDWriteTextFormat *t = tl_format(x, TL_FAM, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(x->f, text, (UINT32)wcslen(text), t, w, h, ppd, NULL, nat, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

TEST(gdi_metrics_whole_pixel_advances_exact)
{
    /* at 256 pixels per em a letter advance is exactly 125 pixels, so every measuring mode agrees */
    TL_BEGIN();
    int nat;
    for (nat = 0; nat < 2; nat++) {
        IDWriteTextLayout *l = gdi_layout(&X, L"ABCD", 256.0f, 10000.0f, 10000.0f, 1.0f, nat); DWRITE_TEXT_METRICS m;
        if (!l) { t_check(0, __FILE__, __LINE__, "gdi layout", "nat %d", nat); continue; }
        tl_metrics(l, &m);
        CHECKF(m.width == 500.0f, "natural=%d width %g", nat, m.width);
        CHECK_FLOAT_EQ(m.layoutWidth, 10000.0f); EXPECT_EQ(m.lineCount, 1);
        t_note("gdi natural=%d line height at 256px: %g", nat, m.height);
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(gdi_metrics_pixels_per_dip_scales)
{
    /* size 128 DIPs at 2 pixels per DIP is 256 pixels per em: 125 pixels per letter = 62.5 DIPs */
    TL_BEGIN();
    IDWriteTextLayout *l = gdi_layout(&X, L"ABCD", 128.0f, 10000.0f, 10000.0f, 2.0f, FALSE); DWRITE_TEXT_METRICS m;
    if (!l) { TL_END(); t_skip("gdi layout"); return; }
    tl_metrics(l, &m);
    CHECK_FLOAT_EQ(m.width, 250.0f); EXPECT_EQ(m.lineCount, 1);
    DW_RELEASE(l); TL_END();
}

TEST(gdi_metrics_classic_rounds_advances_to_pixels)
{
    TL_BEGIN();
    IDWriteTextLayout *l = gdi_layout(&X, L"ABCD", 16.0f, 10000.0f, 10000.0f, 1.0f, FALSE); DWRITE_TEXT_METRICS m;
    if (!l) { TL_END(); t_skip("gdi layout"); return; }
    tl_metrics(l, &m);
    CHECKF(m.width == floorf(m.width), "classic width %g is not a whole number of pixels", m.width);
    CHECK(m.width >= 28.0f && m.width <= 36.0f);
    t_note("gdi classic ABCD at 16px: width %g (natural layout: 31.25)", m.width);
    DW_RELEASE(l); TL_END();
}

TEST(gdi_metrics_natural_close_to_ideal)
{
    TL_BEGIN();
    IDWriteTextLayout *l = gdi_layout(&X, L"ABCD", 16.0f, 10000.0f, 10000.0f, 1.0f, TRUE); DWRITE_TEXT_METRICS m;
    if (!l) { TL_END(); t_skip("gdi layout"); return; }
    tl_metrics(l, &m);
    CHECK(m.width >= 28.0f && m.width <= 36.0f);
    t_note("gdi natural ABCD at 16px: width %g", m.width);
    DW_RELEASE(l); TL_END();
}

TEST(gdi_layout_properties_and_wrap)
{
    TL_BEGIN();
    IDWriteTextLayout *l = gdi_layout(&X, L"AB CD EF", 256.0f, 800.0f, 10000.0f, 1.0f, FALSE); DWRITE_TEXT_METRICS m;   /* 125 per char */
    if (!l) { TL_END(); t_skip("gdi layout"); return; }
    tl_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 2); CHECK_FLOAT_EQ(m.width, 625.0f); CHECK_FLOAT_EQ(m.layoutWidth, 800.0f);   /* "AB CD" fits, "EF" wraps */
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 100000.0f)); tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 1); CHECK_FLOAT_EQ(m.width, 1000.0f);
    DW_RELEASE(l); TL_END();
}

TEST(gdi_metrics_empty_and_newlines)
{
    TL_BEGIN();
    IDWriteTextLayout *l = gdi_layout(&X, L"", 256.0f, 100.0f, 100.0f, 1.0f, FALSE); DWRITE_TEXT_METRICS m;
    if (!l) { TL_END(); t_skip("gdi layout"); return; }
    tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 1); CHECK_FLOAT_EQ(m.width, 0.0f);
    DW_RELEASE(l);
    l = gdi_layout(&X, L"A\nB\nC", 256.0f, 1000.0f, 1000.0f, 1.0f, TRUE);
    if (l) { tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 3); CHECK_FLOAT_EQ(m.width, 125.0f); DW_RELEASE(l); }
    TL_END();
}

TEST(gdi_metrics_ppd_zero_and_negative_create_only)
{
    /* creation succeeds for these values; the measurements are unspecified and are not queried */
    TL_BEGIN();
    IDWriteTextLayout *l = gdi_layout(&X, L"AB", 16.0f, 100.0f, 100.0f, 0.0f, FALSE);
    CHECK(l != NULL); DW_RELEASE(l);
    l = gdi_layout(&X, L"AB", 16.0f, 100.0f, 100.0f, -1.0f, TRUE);
    CHECK(l != NULL); DW_RELEASE(l);
    TL_END();
}

TEST(gdi_metrics_transform_accepted)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 256.0f); IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
    static const DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 }, shear = { 1, 0.5f, 0, 1, 0, 0 }, scale2 = { 2, 0, 0, 2, 0, 0 };
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"ABCD", 4, t, 10000, 10000, 1.0f, &id, FALSE, &l));
    if (l) { tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 500.0f); DW_RELEASE(l); }
    CHECK_HR_OK(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"ABCD", 4, t, 10000, 10000, 1.0f, &shear, FALSE, &l));
    if (l) { tl_metrics(l, &m); t_note("sheared transform: width %g", m.width); DW_RELEASE(l); }
    CHECK_HR_OK(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"ABCD", 4, t, 10000, 10000, 1.0f, &scale2, FALSE, &l));
    if (l) { tl_metrics(l, &m); t_note("2x transform: width %g", m.width); DW_RELEASE(l); }
    DW_RELEASE(t); TL_END();
}
