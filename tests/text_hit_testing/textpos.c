/* IDWriteTextLayout::HitTestTextPosition on left-to-right text.
 *
 * Facts (DWriteTextLayout::HitTestTextPosition -> TextLayout::HitTestTextPosition / HitTestTextPositionInSpan):
 *  - never fails; x, y and the metrics are zeroed, then filled; positions past the end are clamped to the text length;
 *  - the metrics describe the whole CLUSTER that holds the position (textPosition = cluster start, length = cluster length, left/width =
 *    its extent, top/height = the line box), whichever code unit inside the cluster was asked for;
 *  - x is metrics.left, or metrics.left + metrics.width for a trailing hit of a left-to-right cluster; y is metrics.top;
 *  - isTrailingHit is ignored for positions at or beyond the end of the text; the end position yields a zero-length, zero-width
 *    rectangle at the right edge of the last left-to-right span with textPosition = text length.
 */
#include "th.h"

#define ASCII8 L"ABCDEFGH"

TEST(tp_each_position_leading)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    float lh = th_lh(l, 0); UINT32 i;
    for (i = 0; i < 8; i++) {
        th_tp q = th_pos(l, i, FALSE);
        CHK_NEAR(q.x, i * CW); CHK_NEAR(q.y, 0.0f);
        CHK_M(q.m, i, 1, i * CW, CW, 0, 1);
        CHK_NEAR(q.m.top, 0.0f); CHECK(q.m.height > 0.0f && q.m.height <= lh + EPS);
        CHECK(q.m.isTrimmed == 0);
    }
    TH_DONE(l);
}

TEST(tp_each_position_trailing)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 8; i++) {
        th_tp q = th_pos(l, i, TRUE);
        CHK_NEAR(q.x, (i + 1) * CW); CHK_NEAR(q.y, 0.0f);
        CHK_M(q.m, i, 1, i * CW, CW, 0, 1);              /* the rectangle is the same, only the caret side differs */
    }
    TH_DONE(l);
}

TEST(tp_trailing_of_one_is_leading_of_next)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i + 1 < 8; i++) {
        th_tp a = th_pos(l, i, TRUE), b = th_pos(l, i + 1, FALSE);
        CHK_NEAR(a.x, b.x); CHK_NEAR(a.y, b.y);
    }
    TH_DONE(l);
}

TEST(tp_end_position)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    float lh = th_lh(l, 0);
    th_tp q = th_pos(l, 8, FALSE);
    CHK_NEAR(q.x, 8 * CW); CHK_NEAR(q.y, 0.0f);
    CHK_M(q.m, 8, 0, 8 * CW, 0.0f, 0, 1);
    CHK_NEAR(q.m.top, 0.0f); CHECK(q.m.height > 0.0f && q.m.height <= lh + EPS);
    CHECK(q.m.isTrimmed == 0);
    TH_DONE(l);
}

TEST(tp_end_position_ignores_trailing_flag)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    th_tp a = th_pos(l, 8, FALSE), b = th_pos(l, 8, TRUE);
    CHECK(a.x == b.x && a.y == b.y);
    CHECK(!memcmp(&a.m, &b.m, sizeof a.m));
    TH_DONE(l);
}

TEST(tp_position_beyond_end_is_clamped)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    th_tp e = th_pos(l, 8, FALSE);
    UINT32 ps[] = { 9, 10, 100, 0x7fffffff, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    unsigned i;
    for (i = 0; i < sizeof ps / sizeof ps[0]; i++) {
        th_tp q = th_pos(l, ps[i], i & 1);
        CHECKF(q.x == e.x && q.y == e.y && !memcmp(&q.m, &e.m, sizeof e.m), "position %u not clamped to the end", ps[i]);
    }
    TH_DONE(l);
}

TEST(tp_single_char)
{
    TH_BEGIN(); TH_LAYOUT(l, L"Q", 1000.0f, 200.0f);
    th_tp q = th_pos(l, 0, FALSE);   CHK_NEAR(q.x, 0.0f);  CHK_M(q.m, 0, 1, 0.0f, CW, 0, 1);
    q = th_pos(l, 0, TRUE);          CHK_NEAR(q.x, CW);    CHK_M(q.m, 0, 1, 0.0f, CW, 0, 1);
    q = th_pos(l, 1, TRUE);          CHK_NEAR(q.x, CW);    CHK_M(q.m, 1, 0, CW, 0.0f, 0, 1);
    TH_DONE(l);
}

TEST(tp_layout_not_modified_by_hit_tests)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD", 1000.0f, 200.0f);
    DWRITE_TEXT_METRICS a, b; th_metrics(l, &a);
    UINT32 i;
    for (i = 0; i <= 6; i++) { th_pos(l, i, i & 1); }
    th_point(l, 3.0f, 3.0f);
    th_range(l, 0, 5, 0, 0);
    th_metrics(l, &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    TH_DONE(l);
}

/* ---- multi-line --------------------------------------------------------------------------------------------- */
TEST(tp_multiline_positions)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD\nEF", 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    float h0 = th_lh(l, 0), h1 = th_lh(l, 1);
    th_tp q = th_pos(l, 3, FALSE);   CHK_NEAR(q.x, 0.0f);      CHK_NEAR(q.y, h0);        CHK_M(q.m, 3, 1, 0.0f, CW, 0, 1);
    q = th_pos(l, 4, TRUE);          CHK_NEAR(q.x, 2 * CW);    CHK_NEAR(q.y, h0);
    q = th_pos(l, 6, FALSE);         CHK_NEAR(q.x, 0.0f);      CHK_NEAR(q.y, h0 + h1);
    q = th_pos(l, 7, TRUE);          CHK_NEAR(q.x, 2 * CW);    CHK_NEAR(q.y, h0 + h1);
    CHECK(q.m.height > 0.0f && q.m.height <= th_lh(l, 2) + EPS);
    TH_DONE(l);
}

TEST(tp_newline_sits_at_end_of_its_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD", 1000.0f, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    th_tp q = th_pos(l, 2, FALSE);
    CHK_NEAR(q.x, 2 * CW); CHK_NEAR(q.y, 0.0f);
    CHECK(q.m.textPosition == 2 && q.m.length == 1);
    CHK_NEAR(q.m.left, 2 * CW);
    t_note("newline cluster width %g (leading) , trailing hit x %g", q.m.width, th_pos(l, 2, TRUE).x);
    TH_DONE(l);
}

TEST(tp_crlf_is_two_clusters_one_break)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\r\nCD", 1000.0f, 200.0f);
    th_tp a = th_pos(l, 2, FALSE), b = th_pos(l, 3, FALSE);
    t_note("crlf: pos2 (%u,%u) x %g, pos3 (%u,%u) x %g", a.m.textPosition, a.m.length, a.x, b.m.textPosition, b.m.length, b.x);
    CHECK(a.m.textPosition == 2 && a.m.length == 1);
    CHECK(b.m.textPosition == 3 && b.m.length == 1);   /* CR and LF are separate clusters at the same x */
    th_tp c = th_pos(l, 4, FALSE);
    CHK_NEAR(c.x, 0.0f); CHK_NEAR(c.y, th_lh(l, 0));
    CHK_M(c.m, 4, 1, 0.0f, CW, 0, 1);
    TH_DONE(l);
}

TEST(tp_trailing_newline_end_caret)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\n", 1000.0f, 200.0f);
    UINT32 n = th_lines(l);
    th_tp q = th_pos(l, 3, FALSE);
    t_note("\"AB\\n\": %u lines, end caret x %g y %g, m.top %g", n, q.x, q.y, q.m.top);
    CHECK(q.m.textPosition == 3 && q.m.length == 0);
    if (n == 2) { CHK_NEAR(q.y, th_lh(l, 0)); CHK_NEAR(q.x, 0.0f); }      /* the empty last line owns the end caret */
    TH_DONE(l);
}

TEST(tp_empty_lines_between_breaks)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\n\nB", 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    float h0 = th_lh(l, 0), h1 = th_lh(l, 1);
    th_tp q = th_pos(l, 2, FALSE);                    /* the second '\n' is the whole (empty) middle line */
    CHK_NEAR(q.x, 0.0f); CHK_NEAR(q.y, h0);
    CHECK(q.m.textPosition == 2 && q.m.length == 1);
    q = th_pos(l, 3, FALSE);   CHK_NEAR(q.x, 0.0f); CHK_NEAR(q.y, h0 + h1);
    TH_DONE(l);
}

TEST(tp_wrapped_lines)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB", 5 * CW, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    float h = th_lh(l, 0);
    th_tp q = th_pos(l, 4, FALSE);   CHK_NEAR(q.x, 4 * CW); CHK_NEAR(q.y, 0.0f);       /* the space stays on line 0 */
    q = th_pos(l, 5, FALSE);         CHK_NEAR(q.x, 0.0f);   CHK_NEAR(q.y, h);
    q = th_pos(l, 8, TRUE);          CHK_NEAR(q.x, 4 * CW); CHK_NEAR(q.y, h);
    q = th_pos(l, 9, FALSE);         CHK_NEAR(q.x, 4 * CW); CHK_NEAR(q.y, h);          /* end caret on the last line */
    CHK_M(q.m, 9, 0, 4 * CW, 0.0f, 0, 1);
    TH_DONE(l);
}

TEST(tp_rewrap_after_max_width_change)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB", 1000.0f, 200.0f);
    float h = th_lh(l, 0);
    th_tp q = th_pos(l, 5, FALSE);   CHK_NEAR(q.x, 5 * CW); CHK_NEAR(q.y, 0.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 5 * CW));
    q = th_pos(l, 5, FALSE);         CHK_NEAR(q.x, 0.0f);   CHK_NEAR(q.y, h);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 1000.0f));
    q = th_pos(l, 5, FALSE);         CHK_NEAR(q.x, 5 * CW); CHK_NEAR(q.y, 0.0f);
    TH_DONE(l);
}

TEST(tp_no_wrap_keeps_one_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB", 5 * CW, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    th_tp q = th_pos(l, 8, FALSE);
    CHK_NEAR(q.x, 8 * CW); CHK_NEAR(q.y, 0.0f);
    TH_DONE(l);
}

/* ---- clusters ----------------------------------------------------------------------------------------------- */
TEST(tp_surrogate_pair)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\xd800\xdc00" L"B", 1000.0f, 200.0f);
    th_tp a = th_pos(l, 1, FALSE), b = th_pos(l, 2, FALSE);     /* the trail surrogate maps to the cluster start */
    CHK_M(a.m, 1, 2, CW, CW, 0, 1);
    CHK_M(b.m, 1, 2, CW, CW, 0, 1);
    CHK_NEAR(a.x, CW); CHK_NEAR(b.x, CW);                        /* leading hit: start of the cluster, even for the trail position */
    th_tp c = th_pos(l, 1, TRUE), d = th_pos(l, 2, TRUE);
    CHK_NEAR(c.x, 2 * CW); CHK_NEAR(d.x, 2 * CW);
    th_tp e = th_pos(l, 3, FALSE);   CHK_NEAR(e.x, 2 * CW);  CHK_M(e.m, 3, 1, 2 * CW, CW, 0, 1);
    th_tp g = th_pos(l, 4, FALSE);   CHK_NEAR(g.x, 3 * CW);  CHK_M(g.m, 4, 0, 3 * CW, 0.0f, 0, 1);
    TH_DONE(l);
}

TEST(tp_two_surrogate_pairs)
{
    TH_BEGIN(); TH_LAYOUT(l, L"\xd800\xdc00\xd800\xdc01", 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 4; i++) {
        th_tp q = th_pos(l, i, FALSE);
        CHK_M(q.m, i & ~1u, 2, (i >> 1) * CW, CW, 0, 1);
    }
    TH_DONE(l);
}

TEST(tp_combining_mark)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ae\x0301" L"b", 1000.0f, 200.0f);
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    if (n != 3 || c[1].length != 2) { t_note("e+U+0301 clusters: %u", n); TH_DONE(l); return; }
    th_tp a = th_pos(l, 1, FALSE), b = th_pos(l, 2, FALSE);
    CHK_M(a.m, 1, 2, c[0].width, c[1].width, 0, 1);
    CHK_M(b.m, 1, 2, c[0].width, c[1].width, 0, 1);
    CHK_NEAR(a.x, c[0].width); CHK_NEAR(b.x, c[0].width);
    th_tp d = th_pos(l, 2, TRUE);    CHK_NEAR(d.x, c[0].width + c[1].width);
    th_tp e = th_pos(l, 3, FALSE);   CHK_NEAR(e.x, c[0].width + c[1].width);
    TH_DONE(l);
}

TEST(tp_ligature_cluster)
{
    TH_BEGIN(); TH_LAYOUT_F(l, TH_LIG, L"afib", 1000.0f, 200.0f);
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    if (n != 3 || c[1].length != 2) { t_note("fi not ligated, %u clusters", n); TH_DONE(l); return; }
    th_tp a = th_pos(l, 1, FALSE), b = th_pos(l, 2, FALSE);
    CHK_M(a.m, 1, 2, c[0].width, c[1].width, 0, 1);
    CHK_M(b.m, 1, 2, c[0].width, c[1].width, 0, 1);
    CHK_NEAR(a.x, b.x);                                           /* no caret inside a ligature cluster */
    th_tp d = th_pos(l, 1, TRUE);    CHK_NEAR(d.x, c[0].width + c[1].width);
    TH_DONE(l);
}

TEST(tp_ligature_font_without_ligature_text)
{
    TH_BEGIN(); TH_LAYOUT_F(l, TH_LIG, L"fxi", 1000.0f, 200.0f);       /* no f+i sequence: three clusters */
    UINT32 i;
    for (i = 0; i < 3; i++) { th_tp q = th_pos(l, i, FALSE); CHK_M(q.m, i, 1, i * CW, CW, 0, 1); }
    TH_DONE(l);
}

TEST(tp_tab_stops)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\tB", 1000.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 100.0f));
    th_tp q = th_pos(l, 1, FALSE);
    CHK_NEAR(q.x, CW);
    CHECK(q.m.textPosition == 1 && q.m.length == 1);
    CHK_NEAR(q.m.width, 100.0f - CW);                              /* the tab advances to the next multiple of the stop */
    th_tp b = th_pos(l, 2, FALSE);   CHK_NEAR(b.x, 100.0f);
    q = th_pos(l, 1, TRUE);          CHK_NEAR(q.x, 100.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 50.0f));
    b = th_pos(l, 2, FALSE);         CHK_NEAR(b.x, 50.0f);
    TH_DONE(l);
}

TEST(tp_two_tabs)
{
    TH_BEGIN(); TH_LAYOUT(l, L"\t\tX", 1000.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 40.0f));
    th_tp a = th_pos(l, 1, FALSE), b = th_pos(l, 2, FALSE);
    CHK_NEAR(a.x, 40.0f); CHK_NEAR(b.x, 80.0f);
    TH_DONE(l);
}

/* ---- alignment ----------------------------------------------------------------------------------------------- */
TEST(tp_text_alignment_center)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    float left = (100.0f - 4 * CW) / 2;
    th_tp q = th_pos(l, 0, FALSE);   CHK_NEAR(q.x, left);
    q = th_pos(l, 3, TRUE);          CHK_NEAR(q.x, left + 4 * CW);
    q = th_pos(l, 4, FALSE);         CHK_NEAR(q.x, left + 4 * CW);
    th_pt p = th_point(l, left + 1.0f, 3.0f);   CHK_PT(p, 0, 1, 0, 1);
    p = th_point(l, left - 1.0f, 3.0f);          CHK_PT(p, 0, 1, 0, 0);
    TH_DONE(l);
}

TEST(tp_text_alignment_trailing)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    float left = 100.0f - 4 * CW;
    th_tp q = th_pos(l, 0, FALSE);   CHK_NEAR(q.x, left);
    q = th_pos(l, 4, FALSE);         CHK_NEAR(q.x, 100.0f);
    th_pt p = th_point(l, 99.0f, 3.0f);          CHK_PT(p, 3, 1, 1, 1);
    p = th_point(l, 50.0f, 3.0f);                CHK_PT(p, 0, 1, 0, 0);
    TH_DONE(l);
}

TEST(tp_text_alignment_leading_default)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 200.0f);
    CHECK(IDWriteTextLayout_GetTextAlignment(l) == DWRITE_TEXT_ALIGNMENT_LEADING);
    th_tp q = th_pos(l, 0, FALSE);   CHK_NEAR(q.x, 0.0f);
    TH_DONE(l);
}

TEST(tp_alignment_per_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA\nBB", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    th_tp a = th_pos(l, 0, FALSE), b = th_pos(l, 5, FALSE);       /* each line is centred on its own width */
    CHK_NEAR(a.x, (100.0f - 4 * CW) / 2);
    CHK_NEAR(b.x, (100.0f - 2 * CW) / 2);
    TH_DONE(l);
}

TEST(tp_alignment_follows_set_max_width)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    th_tp q = th_pos(l, 0, FALSE);   CHK_NEAR(q.x, 100.0f - 4 * CW);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 200.0f));
    q = th_pos(l, 0, FALSE);         CHK_NEAR(q.x, 200.0f - 4 * CW);
    TH_DONE(l);
}

TEST(tp_paragraph_alignment_near_center_far)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 200.0f);
    float lh = th_lh(l, 0);
    th_tp q = th_pos(l, 0, FALSE);   CHK_NEAR(q.y, 0.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    q = th_pos(l, 0, FALSE);         CHK_NEAR(q.y, (200.0f - lh) / 2); CHK_NEAR(q.m.top, (200.0f - lh) / 2); CHECK(q.m.height > 0.0f && q.m.height <= lh + EPS);
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    q = th_pos(l, 0, FALSE);         CHK_NEAR(q.y, 200.0f - lh);
    th_pt p = th_point(l, 1.0f, 5.0f);
    CHK_PT(p, 0, 1, 0, 0);                                         /* above the bottom-aligned line: outside */
    p = th_point(l, 1.0f, 200.0f - lh * 0.5f);   CHK_PT(p, 0, 1, 0, 1);
    TH_DONE(l);
}

TEST(tp_paragraph_alignment_multiline)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\nB", 100.0f, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    float h0 = th_lh(l, 0), h1 = th_lh(l, 1), tot = h0 + h1;
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    th_tp q = th_pos(l, 2, FALSE);   CHK_NEAR(q.y, (200.0f - tot) / 2 + h0);
    q = th_pos(l, 0, FALSE);         CHK_NEAR(q.y, (200.0f - tot) / 2);
    TH_DONE(l);
}

TEST(tp_alignment_combined)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB", 100.0f, 100.0f);
    float lh = th_lh(l, 0);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    th_tp q = th_pos(l, 1, TRUE);
    CHK_NEAR(q.x, 100.0f); CHK_NEAR(q.y, 100.0f - lh);
    q = th_pos(l, 0, FALSE);   CHK_NEAR(q.x, 100.0f - 2 * CW);
    TH_DONE(l);
}

TEST(tp_justified_alignment_noted)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB CCCC DDDD", 8 * CW, 200.0f);
    HRESULT hr = IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_JUSTIFIED);
    if (FAILED(hr)) { t_note("JUSTIFIED rejected: %s", dw_hr_name(hr)); TH_DONE(l); return; }
    th_tp q = th_pos(l, 0, FALSE);
    CHK_NEAR(q.x, 0.0f);
    t_note("justified: line count %u, pos 3 x %g", th_lines(l), th_pos(l, 3, TRUE).x);
    TH_DONE(l);
}

/* ---- styles ----------------------------------------------------------------------------------------------------- */
TEST(tp_font_size_range)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 1000.0f, 200.0f);
    DWRITE_TEXT_RANGE r = { 1, 2 };
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, r));
    th_tp q = th_pos(l, 1, FALSE);   CHK_NEAR(q.x, CW);       CHK_NEAR(q.m.width, 2 * CW);
    q = th_pos(l, 2, FALSE);         CHK_NEAR(q.x, 3 * CW);   CHK_NEAR(q.m.width, 2 * CW);
    q = th_pos(l, 3, FALSE);         CHK_NEAR(q.x, 5 * CW);   CHK_NEAR(q.m.width, CW);
    q = th_pos(l, 4, FALSE);         CHK_NEAR(q.x, 6 * CW);
    TH_DONE(l);
}

TEST(tp_font_size_range_line_box_covers_largest)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 1000.0f, 200.0f);
    DWRITE_TEXT_RANGE r = { 1, 2 };
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, r));
    float lh = th_lh(l, 0);
    th_tp a = th_pos(l, 0, FALSE), b = th_pos(l, 1, FALSE);
    CHECK(a.m.top >= -EPS && a.m.top + a.m.height <= lh + EPS);
    CHECK(b.m.top >= -EPS && b.m.top + b.m.height <= lh + EPS);
    t_note("mixed sizes: line height %g, small-run height %g, large-run top %g height %g", lh, a.m.height, b.m.top, b.m.height);
    TH_DONE(l);
}

TEST(tp_style_change_between_clusters_keeps_geometry)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 1000.0f, 200.0f);
    DWRITE_TEXT_RANGE r = { 1, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, r));       /* a span boundary without a metric change */
    UINT32 i;
    for (i = 0; i < 4; i++) { th_tp q = th_pos(l, i, FALSE); CHK_NEAR(q.x, i * CW); CHK_M(q.m, i, 1, i * CW, CW, 0, 1); }
    th_pt p = th_point(l, CW + 1.0f, 3.0f);                        CHK_PT(p, 1, 1, 0, 1);
    TH_DONE(l);
}

TEST(tp_narrow_vs_wide_font_family)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 1000.0f, 200.0f);
    DWRITE_TEXT_RANGE r = { 2, 2 };
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TH_LIG, r)); /* same advances: geometry unchanged */
    th_tp q = th_pos(l, 3, TRUE);   CHK_NEAR(q.x, 4 * CW);
    TH_DONE(l);
}

/* ---- inline objects ------------------------------------------------------------------------------------------------- */
TEST(tp_inline_object_replaces_text_metrics)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABC", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(20.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_RANGE r = { 1, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, r));
    th_tp a = th_pos(l, 0, FALSE), b = th_pos(l, 1, FALSE), c = th_pos(l, 2, FALSE);
    CHK_M(a.m, 0, 1, 0.0f, CW, 0, 1);
    CHK_M(b.m, 1, 1, CW, 20.0f, 0, 0);                              /* isText FALSE, width from IDWriteInlineObject::GetMetrics */
    CHK_M(c.m, 2, 1, CW + 20.0f, CW, 0, 1);
    CHK_NEAR(b.x, CW);
    th_tp b2 = th_pos(l, 1, TRUE);  CHK_NEAR(b2.x, CW + 20.0f);
    CHECK(io->n_metrics >= 1);
    IDWriteTextLayout_Release(l); dwx_release(io); TH_END();
}

TEST(tp_inline_object_multi_char_range)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDE", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(30.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_RANGE r = { 1, 3 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, r));
    UINT32 p;
    for (p = 1; p <= 3; p++) {                                       /* every position inside the object hits the whole object */
        th_tp q = th_pos(l, p, FALSE);
        CHK_M(q.m, 1, 3, CW, 30.0f, 0, 0);
        CHK_NEAR(q.x, CW);
        q = th_pos(l, p, TRUE);
        CHK_NEAR(q.x, CW + 30.0f);
    }
    th_tp e = th_pos(l, 4, FALSE);   CHK_M(e.m, 4, 1, CW + 30.0f, CW, 0, 1);
    IDWriteTextLayout_Release(l); dwx_release(io); TH_END();
}

TEST(tp_inline_object_at_start_and_end)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB", 1000.0f, 200.0f);
    dwx_inline *i0 = dwx_inline_new(12.0f, 10.0f, 8.0f, FALSE), *i1 = dwx_inline_new(14.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_RANGE r0 = { 0, 1 }, r1 = { 1, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i0, r0));
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i1, r1));
    th_tp a = th_pos(l, 0, FALSE), b = th_pos(l, 1, FALSE), e = th_pos(l, 2, FALSE);
    CHK_M(a.m, 0, 1, 0.0f, 12.0f, 0, 0);
    CHK_M(b.m, 1, 1, 12.0f, 14.0f, 0, 0);
    CHECK(e.m.textPosition == 2 && e.m.length == 0);
    CHK_NEAR(e.x, 26.0f);
    t_note("end caret after an inline object: isText %d", (int)e.m.isText);
    IDWriteTextLayout_Release(l); dwx_release(i0); dwx_release(i1); TH_END();
}

TEST(tp_inline_object_inside_line_box)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(20.0f, 40.0f, 30.0f, FALSE);
    DWRITE_TEXT_RANGE r = { 1, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, r));
    float lh = th_lh(l, 0);
    th_tp q = th_pos(l, 1, FALSE);
    CHECK(q.m.top >= -EPS && q.m.top + q.m.height <= lh + EPS);
    t_note("inline object 20x40 base 30: line height %g, object rect top %g height %g", lh, q.m.top, q.m.height);
    IDWriteTextLayout_Release(l); dwx_release(io); TH_END();
}

/* ---- trimming ---------------------------------------------------------------------------------------------------------- */
TEST(tp_trimming_marks_positions)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 4 * CW, 200.0f);
    IDWriteInlineObject *sign = NULL;
    IDWriteTextFormat *t = th_format(&X, TH_FAM, 16.0f);
    if (!t || FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &sign))) { DW_RELEASE(t); TH_DONE(l); t_skip("no trimming sign"); return; }
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, sign));
    th_tp a = th_pos(l, 0, FALSE), z = th_pos(l, 7, FALSE);
    CHECK(a.m.isTrimmed == 0);
    CHECK(z.m.isTrimmed == 1);
    UINT32 i; char buf[64]; int o = 0;
    for (i = 0; i < 8; i++) o += snprintf(buf + o, sizeof buf - o, "%d", (int)th_pos(l, i, FALSE).m.isTrimmed);
    t_note("isTrimmed per position (width 4 chars, ellipsis sign): %s", buf);
    DW_RELEASE(sign); DW_RELEASE(t); TH_DONE(l);
}

TEST(tp_no_trimming_when_granularity_none)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 4 * CW, 200.0f);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0 };
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, NULL));
    UINT32 i;
    for (i = 0; i < 8; i++) { th_tp q = th_pos(l, i, FALSE); CHECK(q.m.isTrimmed == 0); CHK_NEAR(q.x, i * CW); }
    TH_DONE(l);
}

TEST(tp_via_derived_layout_interfaces)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII8, 1000.0f, 200.0f);
    IDWriteTextLayout3 *l3 = NULL;
    if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout3, (void **)&l3)) && l3) {
        th_tp a = th_pos(l, 3, TRUE), b; float x = 1e30f, y = 1e30f;
        memset(&b.m, 0xcc, sizeof b.m);
        CHECK_HR_OK(IDWriteTextLayout3_HitTestTextPosition(l3, 3, TRUE, &x, &y, &b.m));
        CHECK(x == a.x && y == a.y && !memcmp(&a.m, &b.m, sizeof a.m));
        IDWriteTextLayout3_Release(l3);
    }
    TH_DONE(l);
}
