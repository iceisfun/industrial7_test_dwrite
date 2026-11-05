/* IDWriteTextLayout::HitTestTextRange.
 *
 * Facts (DWriteTextLayout::HitTestTextRange -> TextLayout::HitTestTextRange + HitTestMetricsMerger):
 *  - *actualHitTestMetricsCount is zeroed first; startPosition + length overflowing 32 bits, or a NULL buffer with a non-zero
 *    maxHitTestMetricsCount, is E_INVALIDARG (count stays 0);
 *  - the range is clamped to the text: end = min(start+length, textLength), start = min(start, end);
 *  - adjacent clusters of one line with the same bidi level / text-ness / trim state merge into one rectangle, in logical order;
 *    a line break, a direction change or an inline object starts a new one; style changes alone do not;
 *  - when the count exceeds maxHitTestMetricsCount the result is E_NOT_SUFFICIENT_BUFFER (0x8007007A) with the needed count and the
 *    buffer is not written; NULL buffer with max 0 is the count-only query (also E_NOT_SUFFICIENT_BUFFER, as the count is at least 1);
 *  - originX / originY are added to left / top of every rectangle.
 */
#include "th.h"

#define ASCII6 L"ABCDEF"

TEST(rg_single_line_inner_range)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    float lh = th_lh(l, 0);
    th_rg r = th_range(l, 1, 3, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) { CHK_M(r.m[0], 1, 3, CW, 3 * CW, 0, 1); CHK_NEAR(r.m[0].top, 0.0f); CHK_NEAR(r.m[0].height, lh); CHECK(r.m[0].isTrimmed == 0); }
    TH_DONE(l);
}

TEST(rg_full_range)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    th_rg r = th_range(l, 0, 6, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 0, 6, 0.0f, 6 * CW, 0, 1);
    TH_DONE(l);
}

TEST(rg_every_sub_range_single_rect)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    UINT32 s, n;
    for (s = 0; s < 6; s++)
        for (n = 1; s + n <= 6; n++) {
            th_rg r = th_range(l, s, n, 0, 0);
            CHECKF(r.count == 1, "range (%u,%u) count %u", s, n, r.count);
            if (r.count == 1) CHK_M(r.m[0], s, n, s * CW, n * CW, 0, 1);
        }
    TH_DONE(l);
}

TEST(rg_single_char_each)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 6; i++) {
        th_rg r = th_range(l, i, 1, 0, 0);
        CHECK(r.count == 1);
        if (r.count == 1) CHK_M(r.m[0], i, 1, i * CW, CW, 0, 1);
    }
    TH_DONE(l);
}

TEST(rg_zero_length_inside)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 6; i++) {
        th_rg r = th_range(l, i, 0, 0, 0);
        CHECKF(r.count == 1, "zero-length at %u count %u", i, r.count);
        if (r.count == 1) { CHK_M(r.m[0], i, 0, i * CW, 0.0f, 0, 1); CHK_NEAR(r.m[0].top, 0.0f); }
    }
    TH_DONE(l);
}

TEST(rg_zero_length_at_end)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    th_rg r = th_range(l, 6, 0, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 6, 0, 6 * CW, 0.0f, 0, 1);
    TH_DONE(l);
}

TEST(rg_start_at_end_with_length)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    th_rg r = th_range(l, 6, 5, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 6, 0, 6 * CW, 0.0f, 0, 1);
    TH_DONE(l);
}

TEST(rg_start_beyond_end_gives_end_range)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    th_rg r = th_range(l, 7, 10, 0, 0);                         /* dummy zero-length range at the end of the text */
    CHECK(r.count == 1);
    if (r.count == 1) { CHK_M(r.m[0], 6, 0, 6 * CW, 0.0f, 0, 1); CHECK(r.m[0].isText == 1); }
    r = th_range(l, 100, 0, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 6, 0, 6 * CW, 0.0f, 0, 1);
    r = th_range(l, 0x7fffffffu, 1, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 6, 0, 6 * CW, 0.0f, 0, 1);
    TH_DONE(l);
}

TEST(rg_length_beyond_end_is_clamped)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    th_rg r = th_range(l, 0, 10, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 0, 6, 0.0f, 6 * CW, 0, 1);
    r = th_range(l, 4, 100, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 4, 2, 4 * CW, 2 * CW, 0, 1);
    r = th_range(l, 0, 0xffffffffu, 0, 0);                      /* 0 + 0xffffffff does not overflow */
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 0, 6, 0.0f, 6 * CW, 0, 1);
    r = th_range(l, 3, 0xfffffffcu, 0, 0);                      /* 3 + 0xfffffffc == 0xffffffff */
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 3, 3, 3 * CW, 3 * CW, 0, 1);
    TH_DONE(l);
}

TEST(rg_overflowing_range_is_invalid_arg)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    struct { UINT32 s, n; } c[] = { { 0xffffffffu, 2 }, { 1, 0xffffffffu }, { 0x80000000u, 0x80000000u }, { 0xfffffffeu, 3 }, { 0xffffffffu, 0xffffffffu }, { 7, 0xfffffffeu } };
    unsigned i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        DWRITE_HIT_TEST_METRICS m[2]; UINT32 n = 0xdeadbeef;
        memset(m, 0xcc, sizeof m);
        HRESULT hr = IDWriteTextLayout_HitTestTextRange(l, c[i].s, c[i].n, 0, 0, m, 2, &n);
        CHECKF(hr == DW_E_INVALIDARG, "(%u,%u) got %s", c[i].s, c[i].n, dw_hr_name(hr));
        CHECKF(n == 0, "(%u,%u) actual count %u not zeroed", c[i].s, c[i].n, n);
        CHECK(m[0].textPosition == 0xcccccccc);
    }
    TH_DONE(l);
}

TEST(rg_max_uint_start_zero_length_is_valid)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    th_rg r = th_range(l, 0xffffffffu, 0, 0, 0);                /* no overflow: clamped to the end */
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 6, 0, 6 * CW, 0.0f, 0, 1);
    TH_DONE(l);
}

TEST(rg_null_buffer_with_capacity_is_invalid_arg)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    UINT32 n = 0xdeadbeef;
    CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0, 3, 0, 0, NULL, 1, &n), DW_E_INVALIDARG);
    CHECK(n == 0);
    n = 0xdeadbeef;
    CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0, 3, 0, 0, NULL, 100, &n), DW_E_INVALIDARG);
    CHECK(n == 0);
    TH_DONE(l);
}

TEST(rg_count_only_query)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    UINT32 n = 0xdeadbeef;
    CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0, 6, 0, 0, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK(n == 1);
    DWRITE_HIT_TEST_METRICS m[1]; memset(m, 0xcc, sizeof m); n = 0xdeadbeef;
    CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0, 6, 0, 0, m, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK(n == 1);
    CHECK(m[0].textPosition == 0xcccccccc);                       /* nothing written */
    TH_DONE(l);
}

TEST(rg_exact_capacity_one)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    DWRITE_HIT_TEST_METRICS m[2]; UINT32 n = 0xdeadbeef; memset(m, 0xcc, sizeof m);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(l, 2, 2, 0, 0, m, 1, &n));
    CHECK(n == 1);
    CHK_M(m[0], 2, 2, 2 * CW, 2 * CW, 0, 1);
    CHECK(m[1].textPosition == 0xcccccccc);
    TH_DONE(l);
}

TEST(rg_origin_offsets_added)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    th_rg a = th_range(l, 1, 2, 0, 0), b = th_range(l, 1, 2, 10.5f, 20.25f), c = th_range(l, 1, 2, -3.0f, 0.0f), d = th_range(l, 1, 2, 0.0f, 7.0f);
    CHECK(a.count == 1 && b.count == 1 && c.count == 1 && d.count == 1);
    if (a.count == 1 && b.count == 1 && c.count == 1 && d.count == 1) {
        CHK_NEAR(b.m[0].left, a.m[0].left + 10.5f); CHK_NEAR(b.m[0].top, a.m[0].top + 20.25f);
        CHK_NEAR(b.m[0].width, a.m[0].width); CHK_NEAR(b.m[0].height, a.m[0].height);
        CHK_NEAR(c.m[0].left, a.m[0].left - 3.0f); CHK_NEAR(c.m[0].top, a.m[0].top);
        CHK_NEAR(d.m[0].left, a.m[0].left);        CHK_NEAR(d.m[0].top, a.m[0].top + 7.0f);
    }
    TH_DONE(l);
}

TEST(rg_origin_applied_to_every_rect)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD\nEF", 1000.0f, 200.0f);
    th_rg a = th_range(l, 0, 8, 0, 0), b = th_range(l, 0, 8, 100.0f, 50.0f);
    CHECK(a.count == b.count);
    UINT32 i;
    for (i = 0; i < a.count && i < b.count; i++) { CHK_NEAR(b.m[i].left, a.m[i].left + 100.0f); CHK_NEAR(b.m[i].top, a.m[i].top + 50.0f); }
    TH_DONE(l);
}

TEST(rg_origin_applied_to_zero_length_range)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    th_rg a = th_range(l, 6, 0, 5.0f, 6.0f);
    CHECK(a.count == 1);
    if (a.count == 1) { CHK_NEAR(a.m[0].left, 6 * CW + 5.0f); CHK_NEAR(a.m[0].top, 6.0f); }
    TH_DONE(l);
}

/* ---- lines ------------------------------------------------------------------------------------------------------------- */
TEST(rg_range_over_newlines_one_rect_per_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD\nEF", 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    th_rg r = th_range(l, 0, 8, 0, 0);
    CHECK(r.count == 3);
    if (r.count == 3) {
        UINT32 k; float top = 0;
        for (k = 0; k < 3; k++) {
            UINT32 start = 3 * k, len = k < 2 ? 3 : 2;
            CHECK(r.m[k].textPosition == start && r.m[k].length == len);
            CHK_NEAR(r.m[k].left, 0.0f); CHK_NEAR(r.m[k].top, top); CHK_NEAR(r.m[k].height, th_lh(l, k));
            CHK_NEAR(r.m[k].width, th_cw_sum(l, start, len));
            top += th_lh(l, k);
        }
    }
    TH_DONE(l);
}

TEST(rg_range_starting_mid_line_ending_mid_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD\nEF", 1000.0f, 200.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    th_rg r = th_range(l, 1, 5, 0, 0);                              /* B \n C D \n */
    CHECK(r.count == 2);
    if (r.count == 2) {
        CHECK(r.m[0].textPosition == 1 && r.m[0].length == 2);
        CHK_NEAR(r.m[0].left, CW); CHK_NEAR(r.m[0].top, 0.0f);
        CHECK(r.m[1].textPosition == 3 && r.m[1].length == 3);
        CHK_NEAR(r.m[1].left, 0.0f); CHK_NEAR(r.m[1].top, th_lh(l, 0));
    }
    r = th_range(l, 4, 3, 0, 0);                                    /* D \n E */
    CHECK(r.count == 2);
    if (r.count == 2) { CHECK(r.m[0].textPosition == 4 && r.m[0].length == 2); CHECK(r.m[1].textPosition == 6 && r.m[1].length == 1); }
    TH_DONE(l);
}

TEST(rg_rects_tile_the_range_in_order)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB CCCC\nDD", 6 * CW, 200.0f);
    th_rg r = th_range(l, 0, 17, 0, 0);
    UINT32 i, at = 0;
    CHECK(r.count >= 3);
    for (i = 0; i < r.count; i++) {
        CHECKF(r.m[i].textPosition == at, "rect %u starts at %u, expected %u", i, r.m[i].textPosition, at);
        at += r.m[i].length;
        if (i) CHECK(r.m[i].top > r.m[i - 1].top - EPS);
    }
    CHECK(at == 17);
    TH_DONE(l);
}

TEST(rg_wrapped_range_two_lines)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB", 5 * CW, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    th_rg r = th_range(l, 0, 9, 0, 0);
    CHECK(r.count == 2);
    if (r.count == 2) {
        CHECK(r.m[0].textPosition == 0 && r.m[0].length == 5);          /* the wrapping space belongs to line 0 */
        CHECK(r.m[1].textPosition == 5 && r.m[1].length == 4);
        CHK_NEAR(r.m[0].top, 0.0f); CHK_NEAR(r.m[1].top, th_lh(l, 0));
        CHK_NEAR(r.m[1].left, 0.0f); CHK_NEAR(r.m[1].width, 4 * CW);
    }
    r = th_range(l, 2, 5, 0, 0);
    CHECK(r.count == 2);
    if (r.count == 2) { CHECK(r.m[0].textPosition == 2 && r.m[0].length == 3); CHECK(r.m[1].textPosition == 5 && r.m[1].length == 2); }
    TH_DONE(l);
}

TEST(rg_range_inside_second_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB", 5 * CW, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    th_rg r = th_range(l, 6, 2, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) { CHK_M(r.m[0], 6, 2, CW, 2 * CW, 0, 1); CHK_NEAR(r.m[0].top, th_lh(l, 0)); }
    TH_DONE(l);
}

TEST(rg_zero_length_on_second_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD", 1000.0f, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    th_rg r = th_range(l, 4, 0, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) { CHK_M(r.m[0], 4, 0, CW, 0.0f, 0, 1); CHK_NEAR(r.m[0].top, th_lh(l, 0)); }
    TH_DONE(l);
}

TEST(rg_range_ends_exactly_at_line_start)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD", 1000.0f, 200.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    th_rg r = th_range(l, 0, 3, 0, 0);                              /* "AB\n": nothing of line 1 */
    CHECK(r.count == 1);
    if (r.count == 1) { CHECK(r.m[0].textPosition == 0 && r.m[0].length == 3); CHK_NEAR(r.m[0].top, 0.0f); }
    TH_DONE(l);
}

TEST(rg_alignment_moves_rects)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    float lh = th_lh(l, 0), left = (100.0f - 4 * CW) / 2;
    th_rg r = th_range(l, 1, 2, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) { CHK_M(r.m[0], 1, 2, left + CW, 2 * CW, 0, 1); CHK_NEAR(r.m[0].top, (200.0f - lh) / 2); }
    TH_DONE(l);
}

/* ---- styles, inline objects, trimming ------------------------------------------------------------------------------------ */
TEST(rg_font_size_change_merges_into_one_rect)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDEF", 1000.0f, 200.0f);
    DWRITE_TEXT_RANGE s = { 3, 3 };
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, s));
    th_rg r = th_range(l, 0, 6, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) {
        CHK_M(r.m[0], 0, 6, 0.0f, 3 * CW + 3 * 2 * CW, 0, 1);
        CHK_NEAR(r.m[0].height, th_lh(l, 0));                      /* the merged rectangle is the full line box */
    }
    TH_DONE(l);
}

TEST(rg_underline_and_weight_changes_merge)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDEF", 1000.0f, 200.0f);
    DWRITE_TEXT_RANGE s = { 2, 2 };
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, s));
    CHECK_HR_OK(IDWriteTextLayout_SetStrikethrough(l, TRUE, s));
    th_rg r = th_range(l, 0, 6, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 0, 6, 0.0f, 6 * CW, 0, 1);
    TH_DONE(l);
}

TEST(rg_inline_object_splits_rects)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDEF", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(20.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_RANGE s = { 3, 3 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, s));
    th_rg r = th_range(l, 0, 6, 0, 0);
    CHECK(r.count == 2);
    if (r.count == 2) {
        CHK_M(r.m[0], 0, 3, 0.0f, 3 * CW, 0, 1);
        CHK_M(r.m[1], 3, 3, 3 * CW, 20.0f, 0, 0);                  /* one rectangle for the whole object, not text */
    }
    r = th_range(l, 4, 1, 0, 0);                                     /* inside the object: the whole object */
    CHECK(r.count == 1);
    if (r.count == 1) { t_note("range inside object: (%u,%u) isText %d", r.m[0].textPosition, r.m[0].length, (int)r.m[0].isText); CHECK(r.m[0].isText == 0); }
    IDWriteTextLayout_Release(l); dwx_release(io); TH_END();
}

TEST(rg_inline_object_in_middle_three_rects)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDE", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(15.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_RANGE s = { 2, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, s));
    th_rg r = th_range(l, 0, 5, 0, 0);
    CHECK(r.count == 3);
    if (r.count == 3) {
        CHK_M(r.m[0], 0, 2, 0.0f, 2 * CW, 0, 1);
        CHK_M(r.m[1], 2, 1, 2 * CW, 15.0f, 0, 0);
        CHK_M(r.m[2], 3, 2, 2 * CW + 15.0f, 2 * CW, 0, 1);
    }
    IDWriteTextLayout_Release(l); dwx_release(io); TH_END();
}

TEST(rg_inline_object_start_beyond_end)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDEF", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(20.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_RANGE s = { 3, 3 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, s));
    th_rg r = th_range(l, 7, 10, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) {
        CHECK(r.m[0].textPosition == 6 && r.m[0].length == 0);
        CHK_NEAR(r.m[0].left, 3 * CW + 20.0f);
        t_note("end range after trailing inline object: isText %d", (int)r.m[0].isText);
    }
    IDWriteTextLayout_Release(l); dwx_release(io); TH_END();
}

TEST(rg_trimmed_range_flags)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDEFGH", 4 * CW, 200.0f);
    IDWriteInlineObject *sign = NULL; IDWriteTextFormat *t = th_format(&X, TH_FAM, 16.0f);
    if (!t || FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &sign))) { DW_RELEASE(t); TH_DONE(l); t_skip("no trimming sign"); return; }
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, sign));
    th_rg r = th_range(l, 0, 8, 0, 0);
    UINT32 i, total = 0, anyTrim = 0, firstTrim = 0;
    for (i = 0; i < r.count; i++) { total += r.m[i].length; if (r.m[i].isTrimmed) anyTrim = 1; }
    (void)firstTrim;
    CHECK(total == 8);
    CHECK(anyTrim);
    CHECK(r.count >= 2);
    if (r.count) CHECK(r.m[0].isTrimmed == 0);
    for (i = 0; i < r.count; i++) t_note("trim rect %u: pos %u len %u left %g width %g trimmed %d text %d", i, r.m[i].textPosition, r.m[i].length, r.m[i].left, r.m[i].width, (int)r.m[i].isTrimmed, (int)r.m[i].isText);
    DW_RELEASE(sign); DW_RELEASE(t); TH_DONE(l);
}

/* ---- clusters ------------------------------------------------------------------------------------------------------------ */
TEST(rg_surrogate_pair_whole_cluster)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\xd800\xdc00" L"B", 1000.0f, 200.0f);
    th_rg r = th_range(l, 0, 4, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 0, 4, 0.0f, 3 * CW, 0, 1);
    r = th_range(l, 1, 2, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 1, 2, CW, CW, 0, 1);
    TH_DONE(l);
}

TEST(rg_range_splitting_a_cluster_noted)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\xd800\xdc00" L"B", 1000.0f, 200.0f);
    UINT32 s, n;
    for (s = 1; s <= 2; s++) for (n = 0; n <= 1; n++) {
        th_rg r = th_range(l, s, n, 0, 0);
        CHECK(r.count == 1);
        if (r.count == 1) t_note("range (%u,%u) inside a surrogate pair -> pos %u len %u left %g width %g", s, n, r.m[0].textPosition, r.m[0].length, r.m[0].left, r.m[0].width);
    }
    TH_DONE(l);
}

TEST(rg_ligature_cluster_range)
{
    TH_BEGIN(); TH_LAYOUT_F(l, TH_LIG, L"afib", 1000.0f, 200.0f);
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    if (n != 3 || c[1].length != 2) { t_note("fi not ligated (%u clusters)", n); TH_DONE(l); return; }
    th_rg r = th_range(l, 1, 2, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 1, 2, c[0].width, c[1].width, 0, 1);
    r = th_range(l, 0, 4, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 0, 4, 0.0f, c[0].width + c[1].width + c[2].width, 0, 1);
    r = th_range(l, 1, 1, 0, 0);
    if (r.count == 1) t_note("half of a ligature (1,1): pos %u len %u width %g (cluster width %g)", r.m[0].textPosition, r.m[0].length, r.m[0].width, c[1].width);
    TH_DONE(l);
}

TEST(rg_tab_range_width)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\tB", 1000.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 100.0f));
    th_rg r = th_range(l, 0, 3, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 0, 3, 0.0f, 100.0f + CW, 0, 1);
    r = th_range(l, 1, 1, 0, 0);
    if (r.count == 1) CHK_M(r.m[0], 1, 1, CW, 100.0f - CW, 0, 1);
    TH_DONE(l);
}

TEST(rg_empty_layout)
{
    TH_BEGIN(); TH_LAYOUT(l, L"", 100.0f, 100.0f);
    th_rg r = th_range(l, 0, 0, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) { CHECK(r.m[0].textPosition == 0 && r.m[0].length == 0); CHK_NEAR(r.m[0].width, 0.0f); }
    r = th_range(l, 0, 5, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHECK(r.m[0].textPosition == 0 && r.m[0].length == 0);
    TH_DONE(l);
}

TEST(rg_matches_text_position_rectangles)
{
    /* a one-cluster range equals the rectangle HitTestTextPosition reports for that cluster */
    TH_BEGIN(); TH_LAYOUT(l, L"AB\nCD EF", 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 8; i++) {
        th_tp q = th_pos(l, i, FALSE); th_rg r = th_range(l, i, 1, 0, 0);
        CHECKF(r.count == 1, "pos %u count %u", i, r.count);
        if (r.count == 1) {
            CHECKF(r.m[0].textPosition == q.m.textPosition && r.m[0].length == q.m.length, "pos %u", i);
            CHK_NEAR(r.m[0].left, q.m.left); CHK_NEAR(r.m[0].width, q.m.width);
            CHECK(r.m[0].top <= q.m.top + EPS && r.m[0].top + r.m[0].height + EPS >= q.m.top + q.m.height);   /* line box contains the span box */
        }
    }
    TH_DONE(l);
}

TEST(rg_via_derived_layout_interface)
{
    TH_BEGIN(); TH_LAYOUT(l, ASCII6, 1000.0f, 200.0f);
    IDWriteTextLayout2 *l2 = NULL;
    if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2)) && l2) {
        DWRITE_HIT_TEST_METRICS m[2]; UINT32 n = 0;
        CHECK_HR_OK(IDWriteTextLayout2_HitTestTextRange(l2, 1, 2, 0, 0, m, 2, &n));
        CHECK(n == 1);
        CHK_M(m[0], 1, 2, CW, 2 * CW, 0, 1);
        IDWriteTextLayout2_Release(l2);
    }
    TH_DONE(l);
}
