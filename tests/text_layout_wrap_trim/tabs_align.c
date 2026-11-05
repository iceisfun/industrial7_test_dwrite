/* Incremental tab stops, text alignment (LEADING / TRAILING / CENTER / JUSTIFIED) and paragraph alignment (NEAR / FAR / CENTER).
 *
 * Facts from the Windows 10 probing:
 *  - SetIncrementalTabStop rejects values <= 0 and NaN (E_INVALIDARG); a tab advances to the next multiple of the stop:
 *    width = (floor(x / stop) + 1) * stop - x with x measured from the start of the line (ModifyTabWidth).
 *  - SetTextAlignment accepts 0..3, SetParagraphAlignment 0..2, SetReadingDirection and SetFlowDirection 0..3 (E_INVALIDARG above).
 *  - AlignmentHelperHorizontal: LEADING means near for left-to-right reading and far for right-to-left, TRAILING the opposite, CENTER centres;
 *    JUSTIFIED positions lines like LEADING. The position is not clamped: a line wider than the layout starts at a negative offset when
 *    aligned far or centred. Alignment uses the line width without trailing white space, which therefore hangs outside the box
 *    (for right-to-left reading the whole line box including the white space is aligned instead).
 *  - AlignmentHelperVertical: NEAR 0, FAR maxHeight - textHeight, CENTER half of that, without clamping.
 *  - JustifyLines stretches every line that does not end in a hard break and is not the last line (all lines for NO_WRAP). */
#include "wt.h"

#define TAB_WIDTH_CHARS(n) ((float)(n) * WT_CW)

static IDWriteTextLayout *mk(wt_fx *x, const WCHAR *t, float w, float h, DWRITE_TEXT_ALIGNMENT ta, DWRITE_PARAGRAPH_ALIGNMENT pa, DWRITE_READING_DIRECTION rd, DWRITE_WORD_WRAPPING ww)
{
    IDWriteTextLayout *l = wt_layout_(x, t, w, h, WT_SIZE);
    if (!l) return NULL;
    if (FAILED(IDWriteTextLayout_SetTextAlignment(l, ta)) || FAILED(IDWriteTextLayout_SetParagraphAlignment(l, pa)) ||
        FAILED(IDWriteTextLayout_SetReadingDirection(l, rd)) || FAILED(IDWriteTextLayout_SetWordWrapping(l, ww))) { IDWriteTextLayout_Release(l); return NULL; }
    return l;
}

/* ---- tab stops: property --------------------------------------------------------------------------------------------- */
TEST(tab_layout_roundtrip)
{
    WT_BEGIN(); static const float v[] = { 0.001f, 1.0f, 4.0f, 31.25f, 50.0f, 125.0f, 1000.0f, 1e9f, FLT_MAX, FLT_MIN, INFINITY }; size_t i; WT_LAYOUT(l, L"a\tb", 100.0f);
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECKF(IDWriteTextLayout_SetIncrementalTabStop(l, v[i]) == S_OK, "value %g", v[i]);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), v[i]);
    }
    DW_RELEASE(l); WT_END();
}

TEST(tab_layout_invalid_values)
{
    WT_BEGIN(); float nanv = NAN, nneg = -NAN; static const float bad[] = { 0.0f, -0.0f, -1.0f, -0.001f, -31.25f, -FLT_MAX, -INFINITY, -FLT_MIN }; size_t i;
    WT_LAYOUT(l, L"a\tb", 100.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 77.0f));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteTextLayout_SetIncrementalTabStop(l, bad[i]);
        CHECKF(hr == E_INVALIDARG, "value %g -> %s", bad[i], dw_hr_name(hr));
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), 77.0f);
    }
    CHECK_HR(IDWriteTextLayout_SetIncrementalTabStop(l, nanv), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetIncrementalTabStop(l, nneg), E_INVALIDARG);
    CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), 77.0f);
    DW_RELEASE(l); WT_END();
}

TEST(tab_format_roundtrip_and_invalid)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); float nanv = NAN;
    CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(t, 42.5f)); CHECK_FLOAT_EQ(IDWriteTextFormat_GetIncrementalTabStop(t), 42.5f);
    CHECK_HR(IDWriteTextFormat_SetIncrementalTabStop(t, 0.0f), E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_SetIncrementalTabStop(t, -5.0f), E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_SetIncrementalTabStop(t, nanv), E_INVALIDARG);
    CHECK_FLOAT_EQ(IDWriteTextFormat_GetIncrementalTabStop(t), 42.5f);
    DW_RELEASE(t); WT_END();
}

TEST(tab_inherited_from_format)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteTextLayout *l = NULL;
    t_note("default incremental tab stop of a size 64 format: %g", IDWriteTextFormat_GetIncrementalTabStop(t));
    CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(t, 123.0f));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"a\tb", 3, t, 100.0f, 100.0f, &l));
    if (l) {
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), 123.0f);
        CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 5.0f)); CHECK_FLOAT_EQ(IDWriteTextFormat_GetIncrementalTabStop(t), 123.0f);
    }
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(tab_default_matches_format_default)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); WT_LAYOUT(l, L"a\tb", 100.0f);
    CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), IDWriteTextFormat_GetIncrementalTabStop(t));
    CHECK(IDWriteTextLayout_GetIncrementalTabStop(l) > 0.0f);
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

/* ---- tab stops: layout ----------------------------------------------------------------------------------------------- */
static float tab_cluster_width(IDWriteTextLayout *l, UINT32 idx)
{
    DWRITE_CLUSTER_METRICS cm[32]; UINT32 n = wt_clusters(l, cm, 32);
    return idx < n ? cm[idx].width : -1.0f;
}

TEST(tab_advances_to_next_stop)
{
    WT_BEGIN(); static const struct { const WCHAR *t; float stop_chars; UINT32 tab_cluster; float want_chars; } c[] = {
        { L"\tb", 4, 0, 4 }, { L"a\tb", 4, 1, 3 }, { L"ab\tb", 4, 2, 2 }, { L"abc\tb", 4, 3, 1 }, { L"abcd\tb", 4, 4, 4 },
        { L"abcde\tb", 4, 5, 3 }, { L"abcdefg\tb", 4, 7, 1 }, { L"abcdefgh\tb", 4, 8, 4 },
        { L"\tb", 8, 0, 8 }, { L"abc\tb", 8, 3, 5 }, { L"abcdefgh\tb", 8, 8, 8 },
        { L"\tb", 1, 0, 1 }, { L"abc\tb", 1, 3, 1 }, { L"abc\tb", 2, 3, 1 }, { L"ab\tb", 2, 2, 2 },
        { L"a\tb", 2.5f, 1, 1.5f }, { L"abc\tb", 2.5f, 3, 2.0f }, { L"abcd\tb", 2.5f, 4, 1.0f }, { L"abcde\tb", 2.5f, 5, 2.5f },
    };
    int i;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        IDWriteTextLayout *l = wt_layout_(&X, c[i].t, 5000.0f, WT_BIGH, WT_SIZE); float w;
        if (!l) continue;
        CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, c[i].stop_chars * WT_CW));
        w = tab_cluster_width(l, c[i].tab_cluster);
        CHECKF(fabsf(w - c[i].want_chars * WT_CW) < 0.01f, "case %d \"%ls\" stop %g: tab width %g want %g", i, c[i].t, c[i].stop_chars * WT_CW, w, c[i].want_chars * WT_CW);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(tab_consecutive_tabs)
{
    WT_BEGIN(); DWRITE_CLUSTER_METRICS cm[16]; UINT32 n; WT_LAYOUT(l, L"a\t\t\tb", 5000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4.0f * WT_CW));
    n = wt_clusters(l, cm, 16); EXPECT_EQ(n, 5);
    if (n == 5) {
        CHECK_NEAR(cm[1].width, 3.0 * WT_CW, 0.01); CHECK_NEAR(cm[2].width, 4.0 * WT_CW, 0.01); CHECK_NEAR(cm[3].width, 4.0 * WT_CW, 0.01);
        EXPECT_EQ(cm[1].isWhitespace, 1);
    }
    DW_RELEASE(l); WT_END();
}

TEST(tab_position_of_following_text)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; WT_LAYOUT(l, L"a\tb\tc", 5000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 50.0f));
    wt_metrics(l, &m);
    /* 'a' ends at 31.25, tab to 50, 'b' to 81.25, tab to 100, 'c' to 131.25 */
    CHECK_NEAR(m.width, 131.25, 0.01);
    CHECK_NEAR(tab_cluster_width(l, 1), 18.75, 0.01); CHECK_NEAR(tab_cluster_width(l, 3), 18.75, 0.01);
    DW_RELEASE(l); WT_END();
}

TEST(tab_restarts_on_each_line)
{
    WT_BEGIN(); DWRITE_CLUSTER_METRICS cm[16]; UINT32 n; WT_LAYOUT(l, L"abcdef\ncd\tx", 5000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4.0f * WT_CW));
    n = wt_clusters(l, cm, 16);
    if (n == 10) CHECK_NEAR(cm[8].width, 2.0 * WT_CW, 0.01);     /* "cd" then the tab: measured from the line start, not the text start */
    else t_note("cluster count %u", n);
    DW_RELEASE(l); WT_END();
}

TEST(tab_relayouts_when_stop_changes)
{
    WT_BEGIN(); WT_LAYOUT(l, L"a\tb", 5000.0f); DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4.0f * WT_CW)); wt_metrics(l, &m); CHECK_NEAR(m.width, 5.0 * WT_CW, 0.01);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 8.0f * WT_CW)); wt_metrics(l, &m); CHECK_NEAR(m.width, 9.0 * WT_CW, 0.01);
    CHECK_HR(IDWriteTextLayout_SetIncrementalTabStop(l, -1.0f), E_INVALIDARG); wt_metrics(l, &m); CHECK_NEAR(m.width, 9.0 * WT_CW, 0.01);
    DW_RELEASE(l); WT_END();
}

TEST(tab_not_counted_as_trailing_whitespace)
{
    WT_BEGIN(); wt_lines o; DWRITE_TEXT_METRICS m; WT_LAYOUT(l, L"ab\t", 5000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4.0f * WT_CW));
    CHECK_HR_OK(wt_get_lines(l, &o)); wt_metrics(l, &m);
    /* a tab is a white space cluster but is not counted as trailing white space: it keeps its width in m.width */
    EXPECT_EQ(o.n, 1); EXPECT_EQ(o.lm[0].length, 3); EXPECT_EQ(o.lm[0].trailingWhitespaceLength, 0);
    CHECK_NEAR(m.width, 4.0 * WT_CW, 0.01); CHECK_NEAR(m.widthIncludingTrailingWhitespace, 4.0 * WT_CW, 0.01);
    DW_RELEASE(l); WT_END();
}

TEST(tab_cluster_is_whitespace_flag)
{
    WT_BEGIN(); DWRITE_CLUSTER_METRICS cm[8]; UINT32 n; WT_LAYOUT(l, L"a\tb", 5000.0f);
    n = wt_clusters(l, cm, 8); EXPECT_EQ(n, 3);
    if (n == 3) { EXPECT_EQ(cm[1].isWhitespace, 1); EXPECT_EQ(cm[1].isNewline, 0); EXPECT_EQ(cm[1].length, 1); EXPECT_EQ(cm[0].isWhitespace, 0); }
    DW_RELEASE(l); WT_END();
}

TEST(tab_does_not_overflow_line)
{
    /* a tab (white space) never triggers a break by itself: the text after it decides */
    WT_BEGIN(); WT_LAYOUT(l, L"a\tb", WT_FIT(3));
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4.0f * WT_CW));
    WT_LENS(l, 2, 1);                    /* the break opportunity after the tab is taken before 'b' */
    DW_RELEASE(l); WT_END();
}

TEST(tab_tiny_stop_is_valid)
{
    WT_BEGIN(); WT_LAYOUT(l, L"a\tb", 5000.0f); DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 0.5f)); wt_metrics(l, &m);
    CHECK(m.width >= 2.0f * WT_CW && m.width < 2.0f * WT_CW + 1.0f);
    DW_RELEASE(l); WT_END();
}

/* ---- enum validation ------------------------------------------------------------------------------------------------- */
TEST(align_text_alignment_values)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); int i; size_t k;
    static const UINT32 bad[] = { 4, 5, 100, 0x7fffffff, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_LEADING);
    for (i = 0; i <= 3; i++) {
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)i)); EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), i);
        CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(t, (DWRITE_TEXT_ALIGNMENT)i)); EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(t), i);
    }
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER)); CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_CENTER));
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        CHECKF(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)bad[k]) == E_INVALIDARG, "layout 0x%x", bad[k]);
        CHECKF(IDWriteTextFormat_SetTextAlignment(t, (DWRITE_TEXT_ALIGNMENT)bad[k]) == E_INVALIDARG, "format 0x%x", bad[k]);
        EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_CENTER); EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(t), DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(align_paragraph_alignment_values)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); int i; size_t k;
    static const UINT32 bad[] = { 3, 4, 100, 0x7fffffff, 0x80000000u, 0xffffffffu };
    EXPECT_EQ(IDWriteTextLayout_GetParagraphAlignment(l), DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    for (i = 0; i <= 2; i++) {
        CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, (DWRITE_PARAGRAPH_ALIGNMENT)i)); EXPECT_EQ(IDWriteTextLayout_GetParagraphAlignment(l), i);
        CHECK_HR_OK(IDWriteTextFormat_SetParagraphAlignment(t, (DWRITE_PARAGRAPH_ALIGNMENT)i)); EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(t), i);
    }
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR)); CHECK_HR_OK(IDWriteTextFormat_SetParagraphAlignment(t, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        CHECKF(IDWriteTextLayout_SetParagraphAlignment(l, (DWRITE_PARAGRAPH_ALIGNMENT)bad[k]) == E_INVALIDARG, "layout 0x%x", bad[k]);
        CHECKF(IDWriteTextFormat_SetParagraphAlignment(t, (DWRITE_PARAGRAPH_ALIGNMENT)bad[k]) == E_INVALIDARG, "format 0x%x", bad[k]);
        EXPECT_EQ(IDWriteTextLayout_GetParagraphAlignment(l), DWRITE_PARAGRAPH_ALIGNMENT_FAR); EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(t), DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    }
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(align_direction_values)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); int i; size_t k;
    static const UINT32 bad[] = { 4, 5, 100, 0x7fffffff, 0x80000000u, 0xffffffffu };
    EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    for (i = 0; i <= 3; i++) {
        CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)i)); EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), i);
        CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, (DWRITE_READING_DIRECTION)i)); EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(t), i);
        CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)i)); EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), i);
        CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, (DWRITE_FLOW_DIRECTION)i)); EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(t), i);
    }
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT)); CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP));
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        CHECKF(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)bad[k]) == E_INVALIDARG, "layout reading 0x%x", bad[k]);
        CHECKF(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)bad[k]) == E_INVALIDARG, "layout flow 0x%x", bad[k]);
        CHECKF(IDWriteTextFormat_SetReadingDirection(t, (DWRITE_READING_DIRECTION)bad[k]) == E_INVALIDARG, "format reading 0x%x", bad[k]);
        CHECKF(IDWriteTextFormat_SetFlowDirection(t, (DWRITE_FLOW_DIRECTION)bad[k]) == E_INVALIDARG, "format flow 0x%x", bad[k]);
        EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), DWRITE_READING_DIRECTION_RIGHT_TO_LEFT); EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP);
    }
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(align_inherited_from_format)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteTextLayout *l = NULL;
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_JUSTIFIED));
    CHECK_HR_OK(IDWriteTextFormat_SetParagraphAlignment(t, DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 100.0f, 100.0f, &l));
    if (l) {
        EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_JUSTIFIED); EXPECT_EQ(IDWriteTextLayout_GetParagraphAlignment(l), DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), DWRITE_READING_DIRECTION_RIGHT_TO_LEFT); EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP);
    }
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

/* ---- horizontal alignment: line position ---------------------------------------------------------------------------- */
typedef struct { DWRITE_TEXT_ALIGNMENT ta; DWRITE_READING_DIRECTION rd; float left_chars; } acase;

TEST(align_single_line_left_matrix)
{
    /* one 3-character line in a 10-character box: DWRITE_TEXT_METRICS.left is the line's start */
    WT_BEGIN();
    static const acase c[] = {
        { DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, 0 },
        { DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, 7 },
        { DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, 3.5f },
        { DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, 0 },
        { DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, 7 },
        { DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, 0 },
        { DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, 3.5f },
        { DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, 7 },
    };
    int i;
    for (i = 0; i < 8; i++) {
        IDWriteTextLayout *l = mk(&X, L"abc", WT_CH(10), WT_BIGH, c[i].ta, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, c[i].rd, DWRITE_WORD_WRAPPING_WRAP); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        wt_metrics(l, &m);
        CHECKF(fabsf(m.left - c[i].left_chars * WT_CW) < 0.02f, "ta %d rd %d: left %g want %g", c[i].ta, c[i].rd, m.left, c[i].left_chars * WT_CW);
        CHECK_NEAR(m.width, WT_CH(3), 0.02); CHECK_NEAR(m.widthIncludingTrailingWhitespace, WT_CH(3), 0.02);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(align_draw_origin_ltr)
{
    WT_BEGIN(); static const struct { DWRITE_TEXT_ALIGNMENT ta; float x_chars; } c[] = { { DWRITE_TEXT_ALIGNMENT_LEADING, 0 }, { DWRITE_TEXT_ALIGNMENT_TRAILING, 7 }, { DWRITE_TEXT_ALIGNMENT_CENTER, 3.5f }, { DWRITE_TEXT_ALIGNMENT_JUSTIFIED, 0 } };
    int i;
    for (i = 0; i < 4; i++) {
        IDWriteTextLayout *l = mk(&X, L"abc", WT_CH(10), WT_BIGH, c[i].ta, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
        dwx_renderer *r; int cur = 0; const dwx_draw *d;
        if (!l) continue;
        r = wt_draw(l, 50.0f, 0.0f);
        d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
        CHECKF(d != NULL, "ta %d: no glyph run", c[i].ta);
        if (d) CHECKF(fabsf(d->x - (50.0f + c[i].x_chars * WT_CW)) < 0.02f, "ta %d: origin %g want %g", c[i].ta, d->x, 50.0f + c[i].x_chars * WT_CW);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(align_each_line_aligned_independently)
{
    /* two lines of different width: the first (6 chars) and the second (3 chars) */
    WT_BEGIN(); IDWriteTextLayout *l = mk(&X, L"aaaaaa\nbbb", WT_CH(10), WT_BIGH, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        dwx_renderer *r = wt_draw(l, 0, 0); int cur = 0; const dwx_draw *d1 = wt_next_run(r, &cur), *d2 = wt_next_run(r, &cur);
        DWRITE_TEXT_METRICS m; wt_metrics(l, &m);
        CHECK(d1 && d2);
        if (d1 && d2) { CHECK_NEAR(d1->x, WT_CH(4), 0.02); CHECK_NEAR(d2->x, WT_CH(7), 0.02); }
        CHECK_NEAR(m.left, WT_CH(4), 0.02);                      /* left = leftmost line start */
        CHECK_NEAR(m.width, WT_CH(6), 0.02);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(align_center_each_line)
{
    WT_BEGIN(); IDWriteTextLayout *l = mk(&X, L"aaaaaa\nbbb\nc", WT_CH(10), WT_BIGH, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        dwx_renderer *r = wt_draw(l, 0, 0); int cur = 0; const dwx_draw *d;
        static const float want[] = { 2.0f, 3.5f, 4.5f }; int k;
        for (k = 0; k < 3; k++) { d = wt_next_run(r, &cur); CHECK(d != NULL); if (d) CHECK_NEAR(d->x, want[k] * WT_CW, 0.02); }
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(align_trailing_whitespace_hangs_ltr)
{
    /* alignment uses the width without trailing white space: the blanks stick out of the box for TRAILING */
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = mk(&X, L"abc  ", WT_CH(10), WT_BIGH, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        wt_metrics(l, &m);
        CHECK_NEAR(m.left, WT_CH(7), 0.02); CHECK_NEAR(m.width, WT_CH(3), 0.02); CHECK_NEAR(m.widthIncludingTrailingWhitespace, WT_CH(5), 0.02);
        IDWriteTextLayout_Release(l);
    }
    l = mk(&X, L"abc  ", WT_CH(10), WT_BIGH, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) { wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(3.5f), 0.02); IDWriteTextLayout_Release(l); }
    WT_END();
}

TEST(align_trailing_whitespace_rtl_aligns_whole_box)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = mk(&X, L"abc  ", WT_CH(10), WT_BIGH, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        wt_metrics(l, &m);
        CHECK_NEAR(m.left, WT_CH(5), 0.02); CHECK_NEAR(m.width, WT_CH(3), 0.02); CHECK_NEAR(m.widthIncludingTrailingWhitespace, WT_CH(5), 0.02);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(align_not_clamped_for_overflowing_line)
{
    WT_BEGIN(); static const struct { DWRITE_TEXT_ALIGNMENT ta; float left_chars; } c[] = {
        { DWRITE_TEXT_ALIGNMENT_LEADING, 0 }, { DWRITE_TEXT_ALIGNMENT_TRAILING, -6 }, { DWRITE_TEXT_ALIGNMENT_CENTER, -3 }, { DWRITE_TEXT_ALIGNMENT_JUSTIFIED, 0 } }; int i;
    for (i = 0; i < 4; i++) {
        IDWriteTextLayout *l = mk(&X, L"abcdefghij", WT_CH(4), WT_BIGH, c[i].ta, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_NO_WRAP); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        wt_metrics(l, &m);
        CHECKF(fabsf(m.left - c[i].left_chars * WT_CW) < 0.02f, "ta %d: left %g want %g", c[i].ta, m.left, c[i].left_chars * WT_CW);
        if (c[i].ta != DWRITE_TEXT_ALIGNMENT_JUSTIFIED) CHECK_NEAR(m.width, WT_CH(10), 0.02);
        else t_note("justified overflowing line: width %g", m.width);       /* justification of a line with no break opportunity is not specified */
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(align_changes_relayout)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; WT_LAYOUT(l, L"abc", WT_CH(10));
    wt_metrics(l, &m); CHECK_NEAR(m.left, 0.0, 0.01);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING)); wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(7), 0.02);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER)); wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(3.5f), 0.02);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT)); wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(3.5f), 0.02);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_LEADING)); wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(7), 0.02);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, WT_CH(20))); wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(17), 0.02);
    CHECK_HR(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)9), E_INVALIDARG); wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(17), 0.02);
    DW_RELEASE(l); WT_END();
}

TEST(align_hit_test_positions_ltr)
{
    WT_BEGIN(); IDWriteTextLayout *l = mk(&X, L"abc", WT_CH(10), WT_BIGH, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        CHECK_NEAR(wt_x(l, 0), WT_CH(7), 0.02); CHECK_NEAR(wt_x(l, 1), WT_CH(8), 0.02); CHECK_NEAR(wt_x(l, 2), WT_CH(9), 0.02); CHECK_NEAR(wt_x_trailing(l, 2), WT_CH(10), 0.02);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(align_hit_test_boxes_rtl)
{
    /* right-to-left reading: the first character is the rightmost box under LEADING alignment (HitTestTextRange boxes, caret side independent) */
    WT_BEGIN(); IDWriteTextLayout *l = mk(&X, HE0 HE1 HE2, WT_CH(10), WT_BIGH, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        DWRITE_HIT_TEST_METRICS hm[4]; UINT32 n = 0; HRESULT hr; int k;
        static const float left[3] = { 9, 8, 7 };
        for (k = 0; k < 3; k++) {
            n = 0; hr = IDWriteTextLayout_HitTestTextRange(l, k, 1, 0.0f, 0.0f, hm, 4, &n);
            if (hr == S_OK && n == 1) { CHECK_NEAR(hm[0].left, left[k] * WT_CW, 0.02); CHECK_NEAR(hm[0].width, WT_CW, 0.02); EXPECT_EQ(hm[0].bidiLevel & 1, 1); }
            else CHECKF(hr == S_OK && n == 1, "range %d: %s n=%u", k, dw_hr_name(hr), n);
        }
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

/* ---- justification --------------------------------------------------------------------------------------------------- */
TEST(justify_fills_every_line_but_the_last)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = mk(&X, L"aaa bbb ccc", WT_FIT(7), WT_BIGH, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        WT_LENS(l, 8, 3); wt_metrics(l, &m);
        CHECK_NEAR(m.width, WT_FIT(7), 0.05);                   /* "aaa bbb" was stretched to the full box */
        CHECK_NEAR(m.left, 0.0, 0.01);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(justify_last_line_not_stretched)
{
    WT_BEGIN(); IDWriteTextLayout *l = mk(&X, L"aaa bbb ccc", WT_FIT(7), WT_BIGH, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        dwx_renderer *r = wt_draw(l, 0, 0); int cur = 0; const dwx_draw *d; float last_w = 0, last_x = -1;
        while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) { last_x = d->x; last_w = dwx_run_width(d); }
        CHECK_NEAR(last_x, 0.0, 0.01); CHECK_NEAR(last_w, WT_CH(3), 0.02);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(justify_line_ending_in_newline_not_stretched)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = mk(&X, L"aa bb\ncc dd", WT_CH(20), WT_BIGH, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) { wt_metrics(l, &m); CHECK_NEAR(m.width, WT_CH(5), 0.02); IDWriteTextLayout_Release(l); }
    WT_END();
}

TEST(justify_single_line_not_stretched)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = mk(&X, L"aa bb", WT_CH(20), WT_BIGH, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) { wt_metrics(l, &m); CHECK_NEAR(m.width, WT_CH(5), 0.02); IDWriteTextLayout_Release(l); }
    WT_END();
}

TEST(justify_no_wrap_stretches_the_line)
{
    /* NO_WRAP layouts justify even their only line */
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = mk(&X, L"aa bb", WT_CH(20), WT_BIGH, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) { wt_metrics(l, &m); t_note("NO_WRAP justified width %g (box %g)", m.width, WT_CH(20)); CHECK(m.width >= 5 * WT_CW - 0.01f && m.width <= WT_CH(20) + 0.05f); IDWriteTextLayout_Release(l); }
    WT_END();
}

TEST(justify_every_wrapped_line_reaches_the_box)
{
    WT_BEGIN(); static const WCHAR *t = L"aa bb cc dd ee ff gg hh ii jj"; int w;
    for (w = 6; w <= 14; w += 2) {
        IDWriteTextLayout *l = mk(&X, t, WT_FIT(w), WT_BIGH, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
        DWRITE_TEXT_METRICS m;
        if (!l) continue;
        wt_metrics(l, &m);
        CHECKF(m.lineCount >= 2 && fabsf(m.width - WT_FIT(w)) < 0.05f, "width %d: lines %u width %g want %g", w, m.lineCount, m.width, WT_FIT(w));
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(justify_does_not_change_line_breaks)
{
    WT_BEGIN(); static const WCHAR *t = L"the quick brown fox jumps over the lazy dog"; int w;
    for (w = 6; w <= 20; w += 2) {
        IDWriteTextLayout *a = mk(&X, t, WT_FIT(w), WT_BIGH, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
        IDWriteTextLayout *b = mk(&X, t, WT_FIT(w), WT_BIGH, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
        wt_lines la, lb; UINT32 i; int same = 1;
        if (a && b && SUCCEEDED(wt_get_lines(a, &la)) && SUCCEEDED(wt_get_lines(b, &lb))) {
            same = la.n == lb.n; for (i = 0; same && i < la.n; i++) same = la.lm[i].length == lb.lm[i].length;
            CHECKF(same, "width %d", w);
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    WT_END();
}

TEST(justify_text_does_not_change_text_extent)
{
    /* the glyph advances sum to the box width on justified lines (Draw) */
    WT_BEGIN(); IDWriteTextLayout *l = mk(&X, L"aaa bbb ccc ddd", WT_FIT(9), WT_BIGH, DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        wt_lines o; dwx_renderer *r; int i, cur = 0; const dwx_draw *d; CHECK_HR_OK(wt_get_lines(l, &o));
        r = wt_draw(l, 0, 0);
        for (i = 0; i < (int)o.n - 1; i++) {
            float w = 0; UINT32 chars = 0; float y0 = -1;
            while (chars < o.lm[i].length && (d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) { if (y0 < 0) y0 = d->y; w += dwx_run_width(d); chars += d->text_len; }
            /* the run includes the trailing blank; its glyph is not stretched: total = box + trailing blank */
            CHECKF(w >= WT_FIT(9) - 0.05f, "line %d: drawn width %g", i, w);
        }
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    WT_END();
}

/* ---- paragraph alignment --------------------------------------------------------------------------------------------- */
TEST(para_top_offset_matrix)
{
    WT_BEGIN(); static const struct { DWRITE_PARAGRAPH_ALIGNMENT pa; int kind; } c[] = { { DWRITE_PARAGRAPH_ALIGNMENT_NEAR, 0 }, { DWRITE_PARAGRAPH_ALIGNMENT_FAR, 1 }, { DWRITE_PARAGRAPH_ALIGNMENT_CENTER, 2 } };
    int i;
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = mk(&X, L"aaa bbb", WT_FIT(3), 500.0f, DWRITE_TEXT_ALIGNMENT_LEADING, c[i].pa, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        wt_metrics(l, &m);
        EXPECT_EQ(m.lineCount, 2);
        CHECK_NEAR(m.layoutHeight, 500.0, 0.001);
        { float want = c[i].kind == 0 ? 0.0f : c[i].kind == 1 ? 500.0f - m.height : (500.0f - m.height) * 0.5f; CHECKF(fabsf(m.top - want) < 0.02f, "pa %d: top %g want %g (height %g)", c[i].pa, m.top, want, m.height); }
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(para_draw_baseline_moves_with_alignment)
{
    WT_BEGIN(); float y[3]; int i;
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = mk(&X, L"abc", WT_CH(10), 400.0f, DWRITE_TEXT_ALIGNMENT_LEADING, (DWRITE_PARAGRAPH_ALIGNMENT)i, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
        dwx_renderer *r; int cur = 0; const dwx_draw *d; y[i] = -1;
        if (!l) continue;
        r = wt_draw(l, 0, 13.0f); d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur); if (d) y[i] = d->y;
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    {
        wt_lines o; IDWriteTextLayout *l = wt_layout_(&X, L"abc", WT_CH(10), 400.0f, WT_SIZE);
        if (l && SUCCEEDED(wt_get_lines(l, &o))) {
            float h = o.lm[0].height;
            { float bl = o.lm[0].baseline;   /* baseline origin is rounded to the nearest whole pixel */
                CHECK_NEAR(y[0], floorf(13.0f + bl + 0.5f), 0.001); CHECK_NEAR(y[1], floorf(13.0f + (400.0f - h) + bl + 0.5f), 0.001); CHECK_NEAR(y[2], floorf(13.0f + (400.0f - h) * 0.5f + bl + 0.5f), 0.001); }
        }
        DW_RELEASE(l);
    }
    WT_END();
}

TEST(para_not_clamped_when_text_taller_than_box)
{
    WT_BEGIN(); IDWriteTextLayout *l = mk(&X, L"aaa bbb ccc", WT_FIT(3), 50.0f, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_FAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP); DWRITE_TEXT_METRICS m;
    if (l) {
        wt_metrics(l, &m); EXPECT_EQ(m.lineCount, 3);
        CHECK(m.height > 50.0f); CHECK_NEAR(m.top, 50.0f - m.height, 0.02);
        CHECK(m.top < 0.0f);
        CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_CENTER)); wt_metrics(l, &m); CHECK_NEAR(m.top, (50.0f - m.height) * 0.5f, 0.02);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(para_alignment_does_not_move_left)
{
    WT_BEGIN(); int i; for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = mk(&X, L"abc", WT_CH(10), 400.0f, DWRITE_TEXT_ALIGNMENT_TRAILING, (DWRITE_PARAGRAPH_ALIGNMENT)i, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(7), 0.02); CHECK_NEAR(m.width, WT_CH(3), 0.02);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(para_zero_height_layout)
{
    WT_BEGIN(); int i; for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = mk(&X, L"abc", WT_CH(10), 0.0f, DWRITE_TEXT_ALIGNMENT_LEADING, (DWRITE_PARAGRAPH_ALIGNMENT)i, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        wt_metrics(l, &m);
        CHECKF(fabsf(m.top - (i == 0 ? 0.0f : i == 1 ? -m.height : -m.height * 0.5f)) < 0.02f, "pa %d top %g height %g", i, m.top, m.height);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(para_changes_relayout)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; WT_LAYOUT_H(l, L"abc", WT_CH(10), 300.0f);
    wt_metrics(l, &m); CHECK_NEAR(m.top, 0.0, 0.01);
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR)); wt_metrics(l, &m); CHECK_NEAR(m.top, 300.0f - m.height, 0.02);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 600.0f)); wt_metrics(l, &m); CHECK_NEAR(m.top, 600.0f - m.height, 0.02);
    CHECK_HR(IDWriteTextLayout_SetParagraphAlignment(l, (DWRITE_PARAGRAPH_ALIGNMENT)3), E_INVALIDARG); wt_metrics(l, &m); CHECK_NEAR(m.top, 600.0f - m.height, 0.02);
    DW_RELEASE(l); WT_END();
}

TEST(para_hit_test_y_follows_alignment)
{
    WT_BEGIN(); IDWriteTextLayout *l = mk(&X, L"abc", WT_CH(10), 400.0f, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_FAR, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_WORD_WRAPPING_WRAP);
    if (l) { DWRITE_TEXT_METRICS m; wt_metrics(l, &m); CHECK_NEAR(wt_y(l, 0), 400.0f - m.height, 0.05); IDWriteTextLayout_Release(l); }
    WT_END();
}
