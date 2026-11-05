/* IDWriteTextLayout::HitTestPoint on plain left-to-right text.
 *
 * Facts (IDWriteTextLayout::HitTestPoint -> HitTestPoint / HitTestPointInSpan):
 *  - the method never fails: it zeroes the three outputs, lays the text out, fills them and returns S_OK;
 *  - the line is the first one whose bottom edge is strictly greater than y (last line when y is below everything);
 *  - inside a line the cluster is the first whose right edge is strictly greater than x, so a point exactly on a cluster boundary
 *    belongs to the cluster on its right; left of everything gives the first cluster, right of everything the last one;
 *  - isTrailingHit is (clusterLeft + clusterWidth/2 <= x) for left-to-right clusters, hence the exact midpoint is a trailing hit;
 *  - isInside is true when x lies in [leftmost, rightmost] and y in [lineTop, lineTop+lineHeight] (both ends inclusive).
 */
#include "th.h"

#define ASCII8 L"ABCDEFGH"

TEST(pt_status_and_bool_normalisation)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    th_pt p = th_point(l, 1.0f, 1.0f);                 /* wrapper requires S_OK and 0/1 BOOL values */
    CHK_PT(p, 0, 1, 0, 1);
    TH_DONE(l);
}

TEST(pt_leading_quarter_of_each_char)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 8; i++) {
        th_pt p = th_point(l, i * CW + CW * 0.25f, 3.0f);
        CHK_PT(p, i, 1, 0, 1);
        CHK_NEAR(p.m.left, i * CW); CHK_NEAR(p.m.width, CW);
    }
    TH_DONE(l);
}

TEST(pt_trailing_quarter_of_each_char)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 8; i++) {
        th_pt p = th_point(l, i * CW + CW * 0.75f, 3.0f);
        CHK_PT(p, i, 1, 1, 1);
    }
    TH_DONE(l);
}

TEST(pt_exact_midpoint_is_trailing)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 8; i++) {
        th_pt p = th_point(l, i * CW + CW * 0.5f, 3.0f);           /* left + width/2 <= x */
        CHK_PT(p, i, 1, 1, 1);
        p = th_point(l, i * CW + CW * 0.5f - 0.01f, 3.0f);
        CHK_PT(p, i, 1, 0, 1);
    }
    TH_DONE(l);
}

TEST(pt_cluster_boundary_belongs_to_next)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 1; i < 8; i++) {
        th_pt p = th_point(l, i * CW, 3.0f);                        /* right edge of i-1 is strict */
        CHK_PT(p, i, 1, 0, 1);
        p = th_point(l, i * CW - 0.01f, 3.0f);
        CHK_PT(p, i - 1, 1, 1, 1);
    }
    TH_DONE(l);
}

TEST(pt_left_edge_is_inside)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    th_pt p = th_point(l, 0.0f, 0.0f);
    CHK_PT(p, 0, 1, 0, 1);
    p = th_point(l, 0.0f, 3.0f);
    CHK_PT(p, 0, 1, 0, 1);
    TH_DONE(l);
}

TEST(pt_right_edge_is_inside_and_trailing)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    DWRITE_TEXT_METRICS tm; th_metrics(l, &tm);
    CHK_NEAR(tm.width, 8 * CW);
    th_pt p = th_point(l, tm.width, 3.0f);                          /* [leftmost, rightmost] inclusive */
    CHK_PT(p, 7, 1, 1, 1);
    p = th_point(l, tm.width + 0.01f, 3.0f);
    CHK_PT(p, 7, 1, 1, 0);
    TH_DONE(l);
}

TEST(pt_left_of_layout)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    float xs[] = { -0.01f, -1.0f, -50.0f, -1e6f, -1e30f, -FLT_MAX };
    unsigned i;
    for (i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        th_pt p = th_point(l, xs[i], 3.0f);
        CHK_PT(p, 0, 1, 0, 0);
        CHK_NEAR(p.m.left, 0.0f);
    }
    TH_DONE(l);
}

TEST(pt_right_of_layout)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    float xs[] = { 62.52f, 100.0f, 500.0f, 999.0f, 1e6f, 1e30f, FLT_MAX };
    unsigned i;
    for (i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        th_pt p = th_point(l, xs[i], 3.0f);
        CHK_PT(p, 7, 1, 1, 0);
        CHK_NEAR(p.m.left, 7 * CW);
    }
    TH_DONE(l);
}

TEST(pt_infinite_coordinates)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    th_pt p = th_point(l, INFINITY, 3.0f);
    CHK_PT(p, 7, 1, 1, 0);
    p = th_point(l, -INFINITY, 3.0f);
    CHK_PT(p, 0, 1, 0, 0);
    p = th_point(l, 20.0f, INFINITY);
    CHK_PT(p, 2, 1, -1, 0);
    p = th_point(l, 20.0f, -INFINITY);
    CHK_PT(p, 2, 1, -1, 0);
    TH_DONE(l);
}

TEST(pt_above_layout)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    float ys[] = { -0.01f, -5.0f, -1e6f, -1e30f };
    unsigned i;
    for (i = 0; i < sizeof ys / sizeof ys[0]; i++) {
        th_pt p = th_point(l, 2 * CW + 1.0f, ys[i]);                /* x still selects the cluster */
        CHK_PT(p, 2, 1, 0, 0);
        CHK_NEAR(p.m.top, 0.0f);
    }
    TH_DONE(l);
}

TEST(pt_below_layout)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    float lh = th_lh(l, 0);
    float ys[] = { lh + 0.01f, lh + 5.0f, 199.0f, 1e6f, 1e30f };
    unsigned i;
    for (i = 0; i < sizeof ys / sizeof ys[0]; i++) {
        th_pt p = th_point(l, 3 * CW + 1.0f, ys[i]);
        CHK_PT(p, 3, 1, 0, 0);
        CHK_NEAR(p.m.top, 0.0f); CHECK(p.m.height > 0.0f && p.m.height <= lh + EPS);
    }
    TH_DONE(l);
}

TEST(pt_four_corners_outside)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    th_pt p;
    p = th_point(l, -1.0f, -1.0f);   CHK_PT(p, 0, 1, 0, 0);
    p = th_point(l, 500.0f, -1.0f);  CHK_PT(p, 7, 1, 1, 0);
    p = th_point(l, -1.0f, 500.0f);  CHK_PT(p, 0, 1, 0, 0);
    p = th_point(l, 500.0f, 500.0f); CHK_PT(p, 7, 1, 1, 0);
    TH_DONE(l);
}

TEST(pt_y_edges_inclusive)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    th_pt p = th_point(l, 10.0f, 0.0f);   CHK_PT(p, 1, 1, -1, 1);
    float bot = p.m.top + p.m.height;                                   /* the hit rectangle's own bottom edge */
    CHECK(p.m.height > 0.0f && bot <= th_lh(l, 0) + EPS);
    p = th_point(l, 10.0f, bot);          CHK_PT(p, 1, 1, -1, 1);       /* single line: y == bottom is still inside */
    p = th_point(l, 10.0f, bot + 0.01f);  CHK_PT(p, 1, 1, -1, 0);
    p = th_point(l, 10.0f, -0.01f);       CHK_PT(p, 1, 1, -1, 0);
    p = th_point(l, 10.0f, bot * 0.5f);   CHK_PT(p, 1, 1, -1, 1);
    TH_DONE(l);
}

TEST(pt_metrics_of_hit_cluster)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    float lh = th_lh(l, 0);
    th_pt p = th_point(l, 3 * CW + 2.0f, 2.0f);
    CHK_M(p.m, 3, 1, 3 * CW, CW, 0, 1);
    CHK_NEAR(p.m.top, 0.0f); CHECK(p.m.height > 0.0f && p.m.height <= lh + EPS);
    CHECK(p.m.isTrimmed == 0);
    TH_DONE(l);
}

TEST(pt_digits_and_spaces_are_clusters)
{
    TH_BEGIN(); TH_LAYOUT(l, L"1 2 3", 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 5; i++) {
        th_pt p = th_point(l, i * CW + 1.0f, 3.0f);
        CHK_PT(p, i, 1, 0, 1);
    }
    TH_DONE(l);
}

TEST(pt_single_char_layout)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A", 1000.0f, 200.0f);
    th_pt p = th_point(l, 1.0f, 1.0f);        CHK_PT(p, 0, 1, 0, 1);
    p = th_point(l, CW - 1.0f, 1.0f);         CHK_PT(p, 0, 1, 1, 1);
    p = th_point(l, -10.0f, 1.0f);            CHK_PT(p, 0, 1, 0, 0);
    p = th_point(l, 100.0f, 1.0f);            CHK_PT(p, 0, 1, 1, 0);
    TH_DONE(l);
}

/* ---- multi-line ---------------------------------------------------------------------------------------- */
#define ML L"AB\nCD\nEF"
TEST(pt_multiline_line_selection_by_y)
{
    TH_BEGIN(); TH_LAYOUT(l, ML, 1000.0f, 200.0f);
    UINT32 k;
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    for (k = 0; k < 3; k++) {
        float lh = th_lh(l, k), top = 0; UINT32 j;
        for (j = 0; j < k; j++) top += th_lh(l, j);
        th_pt p = th_point(l, 1.0f, top + lh * 0.5f);
        CHK_PT(p, 3 * k, 1, 0, 1);
        CHK_NEAR(p.m.top, top);
        p = th_point(l, CW + 1.0f, top + lh * 0.5f);
        CHK_PT(p, 3 * k + 1, 1, 0, 1);
    }
    TH_DONE(l);
}

TEST(pt_multiline_boundary_belongs_to_next_line)
{
    TH_BEGIN(); TH_LAYOUT(l, ML, 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    float h0 = th_lh(l, 0), h1 = th_lh(l, 1);
    th_pt p = th_point(l, 1.0f, h0);                  /* strict: y < bottom of line 0 is false */
    CHK_PT(p, 3, 1, 0, 1);
    p = th_point(l, 1.0f, h0 - 0.01f);                CHK_PT(p, 0, 1, 0, 1);
    p = th_point(l, 1.0f, h0 + h1);                   CHK_PT(p, 6, 1, 0, 1);
    p = th_point(l, 1.0f, h0 + h1 - 0.01f);           CHK_PT(p, 3, 1, 0, 1);
    TH_DONE(l);
}

TEST(pt_multiline_above_and_below)
{
    TH_BEGIN(); TH_LAYOUT(l, ML, 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    th_pt p = th_point(l, 1.0f, -20.0f);              CHK_PT(p, 0, 1, 0, 0);
    p = th_point(l, 1.0f, 190.0f);                    CHK_PT(p, 6, 1, 0, 0);
    p = th_point(l, CW + 1.0f, 1e9f);                 CHK_PT(p, 7, 1, 0, 0);
    TH_DONE(l);
}

TEST(pt_multiline_right_of_line_excludes_newline)
{
    /* right of "AB" the hit lands on the leading edge of the line-break cluster (position 2), outside */
    TH_BEGIN(); TH_LAYOUT(l, ML, 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    float h = th_lh(l, 0);
    th_pt p = th_point(l, 300.0f, h * 0.5f);          CHK_PT(p, 2, 1, 0, 0);
    p = th_point(l, 300.0f, h * 1.5f);                CHK_PT(p, 5, 1, 0, 0);
    p = th_point(l, 300.0f, h * 2.5f);                CHK_PT(p, 7, 1, 1, 0);
    TH_DONE(l);
}

TEST(pt_multiline_left_of_each_line)
{
    TH_BEGIN(); TH_LAYOUT(l, ML, 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    UINT32 k;
    for (k = 0; k < 3; k++) {
        th_pt p = th_point(l, -30.0f, th_lh(l, 0) * (k + 0.5f));
        CHK_PT(p, 3 * k, 1, 0, 0);
    }
    TH_DONE(l);
}

TEST(pt_multiline_inside_flag_per_line)
{
    /* the second line is shorter ("C" only): points right of it but within the first line's extent are outside */
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD\nE\nFG", 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    float h = th_lh(l, 0);
    th_pt p = th_point(l, 3 * CW + 1.0f, h * 0.5f);   CHK_PT(p, 3, 1, 0, 1);
    p = th_point(l, 3 * CW + 1.0f, h * 1.5f);         CHK_PT(p, 6, 1, 0, 0);         /* line "E": only column 0 exists */
    p = th_point(l, CW * 0.25f, h * 1.5f);            CHK_PT(p, 5, 1, 0, 1);
    p = th_point(l, 1.25f * CW, h * 2.5f);            CHK_PT(p, 8, 1, 0, 1);
    TH_DONE(l);
}

TEST(pt_crlf_line_break)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\r\nCD", 1000.0f, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    float h = th_lh(l, 0);
    th_pt p = th_point(l, 1.0f, h * 1.5f);            CHK_PT(p, 4, 1, 0, 1);          /* "\r\n" is one break (two one-unit clusters): D follows at 4 */
    p = th_point(l, 300.0f, h * 0.5f);                CHK_PT(p, 2, 1, 0, 0);
    TH_DONE(l);
}

TEST(pt_wrapped_second_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB", 5 * CW, 200.0f);          /* wraps after the space */
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    float h = th_lh(l, 0);
    th_pt p = th_point(l, 1.0f, h * 1.5f);            CHK_PT(p, 5, 1, 0, 1);
    p = th_point(l, 3 * CW + 1.0f, h * 1.5f);         CHK_PT(p, 8, 1, 0, 1);
    p = th_point(l, 3 * CW + 1.0f, h * 0.5f);         CHK_PT(p, 3, 1, 0, 1);
    TH_DONE(l);
}

TEST(pt_wrapped_trailing_space_cell)
{
    /* The hit never lands on the line-break cluster, but trailing whitespace of a wrapped line is an ordinary cluster
     * (the limit is the line length minus its newline length): the space ending line 0 can be hit. */
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB", 5 * CW, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    th_pt p = th_point(l, 4 * CW + 1.0f, 3.0f);
    CHK_PT(p, 4, 1, 0, 1);
    p = th_point(l, 500.0f, 3.0f);
    CHK_PT(p, 4, 1, 1, 0);
    TH_DONE(l);
}

TEST(pt_nowrap_overflow_extent)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 3 * CW, 200.0f);
    IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP);
    th_pt p = th_point(l, 6 * CW + 1.0f, 3.0f);                    /* beyond maxWidth: still laid out, no trimming configured */
    CHK_PT(p, 6, 1, 0, 1);
    CHECK(p.m.isTrimmed == 0);
    TH_DONE(l);
}

TEST(pt_empty_layout)
{
    TH_BEGIN(); TH_LAYOUT(l, L"", 100.0f, 100.0f);
    th_pt p = th_point(l, 5.0f, 5.0f);
    CHECK(p.m.textPosition == 0 && p.m.length == 0);
    t_note("empty layout point hit: inside %d trailing %d width %g height %g isText %d", (int)p.in, (int)p.tr, p.m.width, p.m.height, (int)p.m.isText);
    p = th_point(l, -5.0f, -5.0f);
    CHECK(p.m.textPosition == 0 && p.m.length == 0);
    TH_DONE(l);
}

TEST(pt_surrogate_pair_cluster)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\xd800\xdc00" L"B", 1000.0f, 200.0f);
    th_pt p = th_point(l, CW + 1.0f, 3.0f);           CHK_PT(p, 1, 2, 0, 1);         /* pair = one cluster of length 2 */
    CHK_M(p.m, 1, 2, CW, CW, 0, 1);
    p = th_point(l, 2 * CW - 1.0f, 3.0f);             CHK_PT(p, 1, 2, 1, 1);
    p = th_point(l, 2 * CW + 1.0f, 3.0f);             CHK_PT(p, 3, 1, 0, 1);         /* B starts at position 3 */
    TH_DONE(l);
}

TEST(pt_combining_mark_cluster)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ae\x0301" L"b", 1000.0f, 200.0f);
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    if (n != 3) { t_note("a e+U+0301 b gave %u clusters (expected 3)", n); TH_DONE(l); return; }
    CHECK(c[1].length == 2);
    th_pt p = th_point(l, c[0].width + 1.0f, 3.0f);   CHK_PT(p, 1, 2, 0, 1);
    CHK_NEAR(p.m.width, c[1].width);
    p = th_point(l, c[0].width + c[1].width + 1.0f, 3.0f);   CHK_PT(p, 3, 1, 0, 1);
    TH_DONE(l);
}

TEST(pt_ligature_cluster)
{
    TH_BEGIN(); TH_LAYOUT_F(l, TH_LIG, L"afib", 1000.0f, 200.0f);
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    if (n != 3 || c[1].length != 2) { t_note("fi was not ligated (clusters %u)", n); TH_DONE(l); return; }
    /* a ligature cluster is hit as a whole: both characters map to the same hit rectangle */
    th_pt p = th_point(l, c[0].width + 1.0f, 3.0f);   CHK_PT(p, 1, 2, 0, 1);
    CHK_NEAR(p.m.width, c[1].width);
    p = th_point(l, c[0].width + c[1].width - 1.0f, 3.0f);   CHK_PT(p, 1, 2, 1, 1);
    p = th_point(l, c[0].width + c[1].width + 1.0f, 3.0f);   CHK_PT(p, 3, 1, 0, 1);
    TH_DONE(l);
}

TEST(pt_font_size_change_widens_cluster)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 1000.0f, 200.0f);
    DWRITE_TEXT_RANGE r = { 2, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, r));        /* C is twice as wide */
    th_pt p = th_point(l, 2 * CW + 1.0f, 3.0f);       CHK_PT(p, 2, 1, 0, 1);
    CHK_NEAR(p.m.width, 2 * CW);
    p = th_point(l, 2 * CW + 2 * CW - 1.0f, 3.0f);    CHK_PT(p, 2, 1, 1, 1);
    p = th_point(l, 4 * CW + 1.0f, th_lh(l, 0) * 0.75f);             CHK_PT(p, 3, 1, 0, 1);
    TH_DONE(l);
}

TEST(pt_point_tracks_layout_property_changes)
{
    /* HitTestPoint runs the layout first: a changed max width is visible at once */
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB", 1000.0f, 200.0f);
    float h = th_lh(l, 0);
    th_pt p = th_point(l, 1.0f, h * 1.5f);            /* single line: below -> last cluster... y outside */
    CHK_PT(p, 0, 1, 0, 0);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 5 * CW));
    p = th_point(l, 1.0f, h * 1.5f);                  CHK_PT(p, 5, 1, 0, 1);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 1000.0f));
    p = th_point(l, 1.0f, h * 1.5f);                  CHK_PT(p, 0, 1, 0, 0);
    TH_DONE(l);
}

TEST(pt_via_derived_layout_interfaces)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    IDWriteTextLayout4 *l4 = NULL; IDWriteTextLayout1 *l1 = NULL;
    HRESULT hr1 = IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1);
    HRESULT hr4 = IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4);
    th_pt a = th_point(l, 20.0f, 3.0f);
    if (SUCCEEDED(hr1) && l1) {
        th_pt b; memset(&b, 0xcc, sizeof b);
        CHECK_HR_OK(IDWriteTextLayout1_HitTestPoint(l1, 20.0f, 3.0f, &b.tr, &b.in, &b.m));
        CHECK(!memcmp(&a.m, &b.m, sizeof a.m) && a.tr == b.tr && a.in == b.in);
        IDWriteTextLayout1_Release(l1);
    }
    if (SUCCEEDED(hr4) && l4) {
        th_pt b; memset(&b, 0xcc, sizeof b);
        CHECK_HR_OK(IDWriteTextLayout4_HitTestPoint(l4, 20.0f, 3.0f, &b.tr, &b.in, &b.m));
        CHECK(!memcmp(&a.m, &b.m, sizeof a.m) && a.tr == b.tr && a.in == b.in);
        IDWriteTextLayout4_Release(l4);
    }
    TH_DONE(l);
}

TEST(pt_layout_outlives_factory_reference)
{
    /* a layout keeps what it needs alive: hit testing works after the caller dropped its factory reference */
    IDWriteFactory *nf = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    if (!nf) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_CreateTextFormat(nf, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, TH_LOC, &t))) { DW_RELEASE(nf); t_skip("format"); return; }
    if (FAILED(IDWriteFactory_CreateTextLayout(nf, L"Hello", 5, t, 500.0f, 100.0f, &l))) { DW_RELEASE(t); DW_RELEASE(nf); t_skip("layout"); return; }
    DW_RELEASE(t); DW_RELEASE(nf);
    th_pt p = th_point(l, 1.0f, 3.0f);
    CHK_PT(p, 0, 1, 0, 1);
    th_tp q = th_pos(l, 3, FALSE);
    CHECK(q.m.textPosition == 3);
    IDWriteTextLayout_Release(l);
}

TEST(pt_repeated_calls_identical)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    th_pt a = th_point(l, 33.3f, 4.0f), b = th_point(l, 33.3f, 4.0f);
    CHECK(!memcmp(&a, &b, sizeof a));
    TH_DONE(l);
}
