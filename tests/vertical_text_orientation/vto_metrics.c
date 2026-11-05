/* Metrics of vertical layouts (reading TOP_TO_BOTTOM / BOTTOM_TO_TOP with flow RIGHT_TO_LEFT / LEFT_TO_RIGHT).
 *
 * dwrite lays text out in an abstract horizontal frame whose wrapping length is maxHeight (GetLayoutWidth in a vertical layout) and whose
 * paragraph-alignment extent is maxWidth (GetLayoutHeight), then swaps left/top, width/height and widthIncludingTrailingWhitespace/
 * heightIncludingTrailingWhitespace, and mirrors left against maxWidth. layoutWidth / layoutHeight are NOT swapped (they stay maxWidth / maxHeight).
 * AlignmentHelperVertical (paragraph alignment x flow direction) and AlignmentHelperHorizontal (text alignment x reading direction bit 0) give the
 * block position; the tables below were derived from those two helpers.
 */
#include "vto.h"

#define A16 7.8125f           /* horizontal advance of a letter at size 16 */
#define V16 11.71875f         /* vertical advance of a letter / Han glyph of DwVert at size 16 */

static IDWriteTextLayout2 *l2of(IDWriteTextLayout *l) { IDWriteTextLayout2 *p = NULL; IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&p); return p; }

TEST(layout_extents_are_not_swapped)
{
    VT_BEGIN(); float ws[] = { 10, 100, 333.5f, 2000 }, hs[] = { 7, 80, 400.25f, 1500 }; int i;
    for (i = 0; i < 4; i++) {
        VT_VL(l, VT_FAM, L"ABCD", 16.0f, ws[i], hs[i]); DWRITE_TEXT_METRICS m = vt_metrics(l);
        CHECK_FLOAT_EQ(m.layoutWidth, ws[i]); CHECK_FLOAT_EQ(m.layoutHeight, hs[i]);
        DW_RELEASE(l);
    }
    VT_END();
}
TEST(latin_inline_extent_is_height)
{
    /* Latin is a rotated (non-sideways) run: horizontal advances run along the vertical inline axis. */
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 200, 200); DWRITE_TEXT_METRICS m = vt_metrics(l);
    CHECK_NEARF(m.height, 4 * A16); EXPECT_EQ(m.lineCount, 1);
    CHECK(m.width > 0.0f);
    DW_RELEASE(l); VT_END();
}
TEST(width_is_sum_of_line_heights)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 200, 30); DWRITE_TEXT_METRICS m = vt_metrics(l); vt_lines ls; vt_get_lines(l, &ls);
    CHECK(ls.n >= 2); EXPECT_EQ(m.lineCount, ls.n);
    CHECK_NEARF(m.width, vt_sum_heights(&ls));
    CHECK_NEARF(m.widthIncludingTrailingWhitespace, m.width);
    DW_RELEASE(l); VT_END();
}
TEST(multi_line_inline_extent_is_longest_line)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\nCDEF\nG", 16.0f, 200, 200); DWRITE_TEXT_METRICS m = vt_metrics(l);
    EXPECT_EQ(m.lineCount, 3); CHECK_NEARF(m.height, 4 * A16);
    DW_RELEASE(l); VT_END();
}
TEST(empty_text_swaps_like_horizontal)
{
    VT_BEGIN(); VT_VL(v, VT_FAM, L"", 16.0f, 100, 100); VT_HL(h, VT_FAM, L"", 16.0f, 100, 100);
    DWRITE_TEXT_METRICS mv = vt_metrics(v), mh = vt_metrics(h);
    CHECK_NEARF(mv.height, 0.0f); CHECK_NEARF(mh.width, 0.0f);
    CHECK_NEARF(mv.width, mh.height);                      /* the empty paragraph still owns one line */
    EXPECT_EQ(mv.lineCount, mh.lineCount);
    DW_RELEASE(v); DW_RELEASE(h); VT_END();
}
TEST(han_uses_vertical_advances_of_the_font)
{
    /* Upright (isSideways) runs take advanceHeight from vmtx, not advanceWidth from hmtx. */
    VT_BEGIN(); VT_VL(l, VT_FAM, HAN4, 16.0f, 200, 400); DWRITE_TEXT_METRICS m = vt_metrics(l);
    CHECK_NEARF(m.height, 4 * V16);
    DW_RELEASE(l); VT_END();
}
TEST(han_horizontal_control_uses_horizontal_advances)
{
    VT_BEGIN(); VT_HL(l, VT_FAM, HAN4, 16.0f, 400, 200); DWRITE_TEXT_METRICS m = vt_metrics(l);
    CHECK_NEARF(m.width, 4 * A16);
    DW_RELEASE(l); VT_END();
}
TEST(han_without_vertical_tables_is_noted)
{
    VT_BEGIN(); VT_VL(l, VT_PLAIN, HAN4, 16.0f, 200, 400); DWRITE_TEXT_METRICS m = vt_metrics(l);
    t_note("DwPlain (no vhea/vmtx) upright Han: height %g for 4 glyphs (horizontal advance would be %g, em would be 64)", m.height, 4 * A16);
    CHECK(m.height > 0.0f);
    DW_RELEASE(l); VT_END();
}
TEST(mixed_latin_han_extent_adds_both_axes)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00\x4e01" L"CD", 16.0f, 200, 400); DWRITE_TEXT_METRICS m = vt_metrics(l);
    CHECK_NEARF(m.height, 4 * A16 + 2 * V16);
    DW_RELEASE(l); VT_END();
}
TEST(space_advance_follows_neighbour_orientation)
{
    /* "A B": the space belongs to the Latin run (rotated, horizontal advance 1000 units). */
    VT_BEGIN(); VT_VL(l, VT_FAM, L"A B", 16.0f, 200, 400); DWRITE_TEXT_METRICS m = vt_metrics(l);
    CHECK_NEARF(m.height, 3 * A16);
    DW_RELEASE(l); VT_END();
}

/* ---- wrapping axis ---------------------------------------------------------------------------------------------- */
TEST(wrap_length_is_max_height)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 1000, 30); DWRITE_TEXT_METRICS m = vt_metrics(l);
    CHECK(m.lineCount >= 2); CHECK(m.height <= 30.0f + 0.01f);
    DW_RELEASE(l); VT_END();
}
TEST(wrap_ignores_max_width)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 3, 1000); DWRITE_TEXT_METRICS m = vt_metrics(l);
    EXPECT_EQ(m.lineCount, 1); CHECK_NEARF(m.height, 11 * A16);
    DW_RELEASE(l); VT_END();
}
TEST(horizontal_control_wraps_on_max_width_not_height)
{
    VT_BEGIN(); VT_HL(a, VT_FAM, L"AB CD EF GH", 16.0f, 30, 3); VT_HL(b, VT_FAM, L"AB CD EF GH", 16.0f, 1000, 3);
    CHECK(vt_metrics(a).lineCount >= 2); EXPECT_EQ(vt_metrics(b).lineCount, 1);
    DW_RELEASE(a); DW_RELEASE(b); VT_END();
}
TEST(wrapping_matches_horizontal_with_swapped_limits)
{
    VT_BEGIN(); VT_VL(v, VT_FAM, L"AB CD EF GH IJ", 16.0f, 1000, 40); VT_HL(h, VT_FAM, L"AB CD EF GH IJ", 16.0f, 40, 1000);
    vt_lines lv, lh; UINT32 i;
    vt_get_lines(v, &lv); vt_get_lines(h, &lh);
    EXPECT_EQ(lv.n, lh.n);
    for (i = 0; i < lv.n && i < lh.n; i++) { EXPECT_EQ(lv.lm[i].length, lh.lm[i].length); EXPECT_EQ(lv.lm[i].trailingWhitespaceLength, lh.lm[i].trailingWhitespaceLength); EXPECT_EQ(lv.lm[i].newlineLength, lh.lm[i].newlineLength); }
    DW_RELEASE(v); DW_RELEASE(h); VT_END();
}
TEST(no_wrap_keeps_one_line_and_overflows_max_height)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 100, 30); DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    m = vt_metrics(l);
    EXPECT_EQ(m.lineCount, 1); CHECK_NEARF(m.height, 11 * A16); CHECK(m.height > 30.0f);
    DW_RELEASE(l); VT_END();
}
TEST(set_max_height_rewraps)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 100, 1000); UINT32 n0, n1;
    n0 = vt_metrics(l).lineCount; EXPECT_EQ(n0, 1);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 30)); n1 = vt_metrics(l).lineCount; CHECK(n1 >= 2);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 1000)); EXPECT_EQ(vt_metrics(l).lineCount, 1);
    DW_RELEASE(l); VT_END();
}
TEST(set_max_width_does_not_rewrap_vertical)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 100, 30); UINT32 n0 = vt_metrics(l).lineCount;
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 1)); EXPECT_EQ(vt_metrics(l).lineCount, n0);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 5000)); EXPECT_EQ(vt_metrics(l).lineCount, n0);
    DW_RELEASE(l); VT_END();
}
TEST(clusters_follow_orientation_advance)
{
    VT_BEGIN(); VT_VL(a, VT_FAM, L"AB", 16.0f, 100, 100); VT_VL(b, VT_FAM, L"\x4e00\x4e01", 16.0f, 100, 100);
    DWRITE_CLUSTER_METRICS c[4]; UINT32 n = 0;
    CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(a, c, 4, &n)); EXPECT_EQ(n, 2);
    if (n == 2) { CHECK_NEARF(c[0].width, A16); CHECK_NEARF(c[1].width, A16); EXPECT_EQ(c[0].length, 1); }
    n = 0; CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(b, c, 4, &n)); EXPECT_EQ(n, 2);
    if (n == 2) { CHECK_NEARF(c[0].width, V16); CHECK_NEARF(c[1].width, V16); }
    DW_RELEASE(a); DW_RELEASE(b); VT_END();
}
TEST(trailing_whitespace_goes_to_height_including)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB  ", 16.0f, 200, 400); IDWriteTextLayout2 *l2 = l2of(l); DWRITE_TEXT_METRICS1 m;
    if (!l2) { DW_RELEASE(l); VT_END(); t_skip("IDWriteTextLayout2"); return; }
    memset(&m, 0, sizeof m); CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l2, &m));
    CHECK_NEARF(m.height, 2 * A16); CHECK_NEARF(m.heightIncludingTrailingWhitespace, 4 * A16);
    CHECK_NEARF(m.widthIncludingTrailingWhitespace, m.width);
    DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}
TEST(metrics1_base_fields_equal_metrics)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF", 16.0f, 120, 30); IDWriteTextLayout2 *l2 = l2of(l); DWRITE_TEXT_METRICS m; DWRITE_TEXT_METRICS1 m1;
    if (!l2) { DW_RELEASE(l); VT_END(); t_skip("IDWriteTextLayout2"); return; }
    m = vt_metrics(l); memset(&m1, 0, sizeof m1); CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l2, &m1));
    CHECK_FLOAT_EQ(m1.left, m.left); CHECK_FLOAT_EQ(m1.top, m.top); CHECK_FLOAT_EQ(m1.width, m.width); CHECK_FLOAT_EQ(m1.height, m.height);
    CHECK_FLOAT_EQ(m1.layoutWidth, m.layoutWidth); CHECK_FLOAT_EQ(m1.layoutHeight, m.layoutHeight); EXPECT_EQ(m1.lineCount, m.lineCount);
    CHECK_FLOAT_EQ(m1.widthIncludingTrailingWhitespace, m.widthIncludingTrailingWhitespace);
    DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}

/* ---- block position: paragraph alignment x flow direction (left) ------------------------------------------------- */
typedef struct { int fd, pa; int mode; } leftcase;
TEST(left_for_flow_right_to_left_by_paragraph_alignment)
{
    /* flow RTL: lines are stacked from the right edge. NEAR: left = W - S; FAR: left = 0; CENTER: (W - S) / 2. */
    VT_BEGIN(); float W = 200; int pa;
    for (pa = 0; pa < 3; pa++) {
        VT_VL(l, VT_FAM, L"ABCD", 16.0f, W, 100); DWRITE_TEXT_METRICS m; float want;
        CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, (DWRITE_PARAGRAPH_ALIGNMENT)pa)); m = vt_metrics(l);
        want = pa == DWRITE_PARAGRAPH_ALIGNMENT_NEAR ? W - m.width : pa == DWRITE_PARAGRAPH_ALIGNMENT_FAR ? 0.0f : (W - m.width) / 2;
        CHECKF(fabs(m.left - want) < 0.001, "paragraph alignment %d: left %g want %g (width %g)", pa, m.left, want, m.width);
        DW_RELEASE(l);
    }
    VT_END();
}
TEST(left_for_flow_left_to_right_by_paragraph_alignment)
{
    /* flow LTR: lines are stacked from the left edge. NEAR: left = 0; FAR: left = W - S; CENTER: (W - S) / 2. */
    VT_BEGIN(); float W = 200; int pa;
    for (pa = 0; pa < 3; pa++) {
        IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, L"ABCD", 16.0f, W, 100, RD_TTB, FD_LTR); DWRITE_TEXT_METRICS m; float want;
        if (!l) { VT_END(); t_skip("layout"); return; }
        CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, (DWRITE_PARAGRAPH_ALIGNMENT)pa)); m = vt_metrics(l);
        want = pa == DWRITE_PARAGRAPH_ALIGNMENT_NEAR ? 0.0f : pa == DWRITE_PARAGRAPH_ALIGNMENT_FAR ? W - m.width : (W - m.width) / 2;
        CHECKF(fabs(m.left - want) < 0.001, "paragraph alignment %d: left %g want %g (width %g)", pa, m.left, want, m.width);
        DW_RELEASE(l);
    }
    VT_END();
}
TEST(left_with_several_lines_uses_total_flow_extent)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 300, 30); DWRITE_TEXT_METRICS m = vt_metrics(l); vt_lines ls;
    vt_get_lines(l, &ls); CHECK(ls.n >= 2);
    CHECK_NEARF(m.left, 300 - vt_sum_heights(&ls)); CHECK_NEARF(m.width, vt_sum_heights(&ls));
    DW_RELEASE(l); VT_END();
}
TEST(left_not_affected_by_text_alignment)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 200, 100); float l0; int ta;
    l0 = vt_metrics(l).left;
    for (ta = 0; ta < 4; ta++) { CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)ta)); CHECK_NEARF(vt_metrics(l).left, l0); }
    DW_RELEASE(l); VT_END();
}
TEST(horizontal_flow_directions_top_by_paragraph_alignment)
{
    /* Horizontal reading with flow TOP_TO_BOTTOM or BOTTOM_TO_TOP: BOTTOM_TO_TOP stacks lines from the bottom edge. */
    VT_BEGIN(); float H = 200; int fd, pa;
    for (fd = 0; fd < 2; fd++) for (pa = 0; pa < 3; pa++) {
        IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, L"ABCD", 16.0f, 100, H, RD_LTR, fd); DWRITE_TEXT_METRICS m; float want;
        if (!l) { VT_END(); t_skip("layout"); return; }
        CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, (DWRITE_PARAGRAPH_ALIGNMENT)pa)); m = vt_metrics(l);
        if (fd == 0) want = pa == 0 ? 0.0f : pa == 1 ? H - m.height : (H - m.height) / 2;
        else         want = pa == 0 ? H - m.height : pa == 1 ? 0.0f : (H - m.height) / 2;
        CHECKF(fabs(m.top - want) < 0.001, "flow %s paragraph %d: top %g want %g", (fd ? "BTT" : "TTB"), pa, m.top, want);
        DW_RELEASE(l);
    }
    VT_END();
}

/* ---- block position along the inline axis (top) -------------------------------------------------------------------- */
TEST(top_by_reading_direction_and_text_alignment)
{
    /* top = aligned position of the line in the abstract frame; reading direction bit 0 (BOTTOM_TO_TOP) reverses leading/trailing.
     * LEADING 0, TRAILING 1, CENTER 2 (JUSTIFIED is laid out like LEADING for a single, last line). */
    VT_BEGIN(); float H = 300; int rd, ta;
    for (rd = 2; rd < 4; rd++) for (ta = 0; ta < 4; ta++) {
        IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, L"ABCD", 16.0f, 100, H, rd, FD_RTL); DWRITE_TEXT_METRICS m; float want; int eff;
        if (!l) { VT_END(); t_skip("layout"); return; }
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)ta)); m = vt_metrics(l);
        eff = ta == 3 ? 0 : ta;                                  /* effective alignment: 0 leading, 1 trailing, 2 center */
        if (eff == 2) want = (H - m.height) / 2;
        else { int far_end = (eff == 1) ^ (rd == 3); want = far_end ? H - m.height : 0.0f; }
        CHECKF(fabs(m.top - want) < 0.001, "reading %s alignment %d: top %g want %g (height %g)", (rd == 3 ? "BTT" : "TTB"), ta, m.top, want, m.height);
        DW_RELEASE(l);
    }
    VT_END();
}
TEST(top_with_overflowing_line_is_negative_for_trailing)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCDEFGH", 16.0f, 100, 30); DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING)); m = vt_metrics(l);
    CHECK_NEARF(m.top, 30 - 8 * A16);
    DW_RELEASE(l); VT_END();
}
TEST(top_unchanged_by_flow_direction)
{
    VT_BEGIN(); IDWriteTextLayout *a = vt_layout_(&X, VT_FAM, L"ABCD", 16.0f, 100, 200, RD_TTB, FD_RTL), *b = vt_layout_(&X, VT_FAM, L"ABCD", 16.0f, 100, 200, RD_TTB, FD_LTR);
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); VT_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(a, DWRITE_TEXT_ALIGNMENT_CENTER)); CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(b, DWRITE_TEXT_ALIGNMENT_CENTER));
    CHECK_NEARF(vt_metrics(a).top, vt_metrics(b).top); CHECK_NEARF(vt_metrics(a).height, vt_metrics(b).height); CHECK_NEARF(vt_metrics(a).width, vt_metrics(b).width);
    DW_RELEASE(a); DW_RELEASE(b); VT_END();
}
TEST(setters_invalidate_the_vertical_layout)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 200, 100); DWRITE_TEXT_METRICS a, b;
    a = vt_metrics(l);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, FD_LTR)); CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
    b = vt_metrics(l);
    CHECK_NEARF(a.left, 200 - a.width); CHECK_NEARF(b.left, 0.0f);
    DW_RELEASE(l); VT_END();
}

/* ---- line spacing and line metrics ------------------------------------------------------------------------------- */
TEST(uniform_line_spacing_sets_flow_extent)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 300, 30); vt_lines ls; DWRITE_TEXT_METRICS m; UINT32 i;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    vt_get_lines(l, &ls); m = vt_metrics(l); CHECK(ls.n >= 2);
    for (i = 0; i < ls.n; i++) { CHECK_NEARF(ls.lm[i].height, 30.0f); CHECK_NEARF(ls.lm[i].baseline, 24.0f); }
    CHECK_NEARF(m.width, 30.0f * ls.n); CHECK_NEARF(m.left, 300 - 30.0f * ls.n);
    DW_RELEASE(l); VT_END();
}
TEST(line_metrics_buffer_protocol_vertical)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD EF GH", 16.0f, 300, 30); DWRITE_LINE_METRICS lm[8]; UINT32 n = 99, total;
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER); total = n; CHECK(total >= 2);
    n = 99; CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, lm, 1, &n), DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, total);
    n = 0; CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, lm, 8, &n)); EXPECT_EQ(n, total);
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 5, &n), VT_E_INVALIDARG);
    DW_RELEASE(l); VT_END();
}
TEST(line_lengths_sum_to_text_length_vertical)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CD\nEF GH", 16.0f, 300, 30); vt_lines ls; UINT32 i, sum = 0;
    vt_get_lines(l, &ls);
    for (i = 0; i < ls.n; i++) sum += ls.lm[i].length;
    EXPECT_EQ(sum, 11);
    DW_RELEASE(l); VT_END();
}
TEST(hard_line_break_lines)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\nCD\r\nEF", 16.0f, 300, 300); vt_lines ls;
    vt_get_lines(l, &ls); EXPECT_EQ(ls.n, 3);
    if (ls.n == 3) { EXPECT_EQ(ls.lm[0].newlineLength, 1); EXPECT_EQ(ls.lm[1].newlineLength, 2); EXPECT_EQ(ls.lm[2].newlineLength, 0); CHECK(ls.lm[0].isTrimmed == FALSE); }
    DW_RELEASE(l); VT_END();
}
TEST(trimming_character_along_vertical_axis)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCDEFGHIJKL", 16.0f, 100, 40); DWRITE_TRIMMING tr; DWRITE_TEXT_METRICS m; vt_lines ls;
    memset(&tr, 0, sizeof tr); tr.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, NULL));
    m = vt_metrics(l); vt_get_lines(l, &ls);
    EXPECT_EQ(ls.n, 1);
    if (ls.n == 1) CHECK(ls.lm[0].isTrimmed);
    CHECK(m.height <= 40.0f + 0.01f); CHECK(m.height > 0.0f);
    DW_RELEASE(l); VT_END();
}
TEST(trimming_with_ellipsis_sign_in_vertical_layout)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteTextLayout *l = NULL; IDWriteInlineObject *sign = NULL; DWRITE_TRIMMING tr; DWRITE_TEXT_METRICS m;
    if (!t) { VT_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, RD_TTB)); CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, FD_RTL));
    DW_REQUIRE_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &sign));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCDEFGHIJKL", 12, t, 100, 40, &l));
    memset(&tr, 0, sizeof tr); tr.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, sign));
    m = vt_metrics(l); CHECK(m.height <= 40.0f + 0.01f);
    { DWRITE_INLINE_OBJECT_METRICS im; memset(&im, 0, sizeof im); if (SUCCEEDED(IDWriteInlineObject_GetMetrics(sign, &im))) t_note("vertical ellipsis sign metrics: width %g height %g baseline %g", im.width, im.height, im.baseline); }
    DW_RELEASE(l); DW_RELEASE(sign); DW_RELEASE(t); VT_END();
}
TEST(orientation_stacked_extent_is_one_of_the_two_advances)
{
    /* STACKED replaces the per-class table: Latin letters become either rotated (horizontal advance) or upright (vertical advance) runs. */
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 200, 200); IDWriteTextLayout2 *l2 = l2of(l); DWRITE_TEXT_METRICS m;
    if (!l2) { DW_RELEASE(l); VT_END(); t_skip("IDWriteTextLayout2"); return; }
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED)); m = vt_metrics(l);
    t_note("STACKED Latin: height %g (rotated %g / upright %g)", m.height, 4 * A16, 4 * V16);
    CHECK(fabs(m.height - 4 * A16) < 0.001 || fabs(m.height - 4 * V16) < 0.001);
    DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}
TEST(orientation_default_vs_stacked_han_unchanged_or_noted)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, HAN4, 16.0f, 200, 400); IDWriteTextLayout2 *l2 = l2of(l); DWRITE_TEXT_METRICS a, b;
    if (!l2) { DW_RELEASE(l); VT_END(); t_skip("IDWriteTextLayout2"); return; }
    a = vt_metrics(l);
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED)); b = vt_metrics(l);
    t_note("Han DEFAULT height %g STACKED height %g", a.height, b.height);
    CHECK(b.height > 0.0f);
    DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}
TEST(line_height_vertical_vs_horizontal_noted)
{
    VT_BEGIN(); VT_VL(v, VT_FAM, L"ABCD", 16.0f, 200, 200); VT_HL(h, VT_FAM, L"ABCD", 16.0f, 200, 200); vt_lines lv, lh;
    vt_get_lines(v, &lv); vt_get_lines(h, &lh);
    EXPECT_EQ(lv.n, 1); EXPECT_EQ(lh.n, 1);
    if (lv.n == 1 && lh.n == 1) t_note("line height vertical %g horizontal %g; baseline vertical %g horizontal %g", lv.lm[0].height, lh.lm[0].height, lv.lm[0].baseline, lh.lm[0].baseline);
    CHECK(lv.lm[0].height > 0.0f);
    DW_RELEASE(v); DW_RELEASE(h); VT_END();
}
TEST(overhang_metrics_vertical_noted)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); DWRITE_OVERHANG_METRICS oh;
    memset(&oh, 0, sizeof oh); CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &oh));
    t_note("vertical overhang L %g T %g R %g B %g", oh.left, oh.top, oh.right, oh.bottom);
    DW_RELEASE(l); VT_END();
}
TEST(determine_min_width_vertical_noted)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB CDEF", 16.0f, 100, 100); float mw = -1;
    CHECK_HR_OK(IDWriteTextLayout_DetermineMinWidth(l, &mw));
    t_note("vertical DetermineMinWidth %g (longest word %g)", mw, 4 * A16);
    CHECK(mw > 0.0f);
    DW_RELEASE(l); VT_END();
}
