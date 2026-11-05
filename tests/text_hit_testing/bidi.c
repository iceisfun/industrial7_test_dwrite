/* Hit testing of right-to-left and mixed-direction text.
 *
 * Facts (TextLayout::HitTestPointInSpan / HitTestTextPosition, bidi level in the span record):
 *  - a span of odd bidi level lays its clusters out from the span's right edge going left;
 *  - HitTestTextPosition: leading hit of a right-to-left cluster is at its RIGHT edge (metrics.left + width), trailing hit at metrics.left;
 *    the end-of-text position of a right-to-left last span is the span's left edge;
 *  - HitTestPoint: inside a right-to-left span the first cluster (from the right) whose left edge is <= x wins (a point on a boundary
 *    belongs to the cluster on its right) and isTrailingHit is (mid <= x) XOR rtl, i.e. the left half of a right-to-left cluster is its trailing half;
 *  - metrics.bidiLevel is the resolved level of the span (2 for Latin in a right-to-left paragraph).
 */
#include "th.h"

TEST(bidi_rtl_positions_leading)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 4; i++) {
        th_tp q = th_pos(l, i, FALSE);
        CHK_M(q.m, i, 1, (3 - i) * CW, CW, 1, 1);
        CHK_NEAR(q.x, (4 - i) * CW);                          /* leading edge of an RTL cluster is its right edge */
        CHK_NEAR(q.y, 0.0f);
    }
    TH_DONE(l);
}

TEST(bidi_rtl_positions_trailing)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 4; i++) {
        th_tp q = th_pos(l, i, TRUE);
        CHK_M(q.m, i, 1, (3 - i) * CW, CW, 1, 1);
        CHK_NEAR(q.x, (3 - i) * CW);
    }
    TH_DONE(l);
}

TEST(bidi_rtl_end_position)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    th_tp a = th_pos(l, 4, FALSE), b = th_pos(l, 4, TRUE);
    CHK_NEAR(a.x, 0.0f); CHK_NEAR(b.x, 0.0f);                 /* after the last logical character = left edge */
    CHECK(a.m.textPosition == 4 && a.m.length == 0);
    CHK_NEAR(a.m.width, 0.0f);
    TH_DONE(l);
}

TEST(bidi_rtl_point_cluster_selection)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    UINT32 s;
    for (s = 0; s < 4; s++) {                                 /* visual slot s from the left holds logical cluster 3-s */
        th_pt p = th_point(l, s * CW + 0.25f * CW, 3.0f);
        CHK_PT(p, 3 - s, 1, 1, 1);                            /* left half of an RTL cluster is its trailing half */
        p = th_point(l, s * CW + 0.75f * CW, 3.0f);
        CHK_PT(p, 3 - s, 1, 0, 1);
    }
    TH_DONE(l);
}

TEST(bidi_rtl_point_boundary_belongs_to_right_cluster)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    UINT32 s;
    for (s = 1; s < 4; s++) {
        th_pt p = th_point(l, s * CW, 3.0f);                  /* boundary between slot s-1 and s: left edge of slot s wins (<=) */
        CHK_PT(p, 3 - s, 1, -1, 1);
    }
    TH_DONE(l);
}

TEST(bidi_rtl_point_midpoint)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    th_pt p = th_point(l, 1.5f * CW, 3.0f);                   /* mid <= x -> true, XOR rtl -> false */
    CHK_PT(p, 2, 1, 0, 1);
    p = th_point(l, 1.5f * CW - 0.01f, 3.0f);
    CHK_PT(p, 2, 1, 1, 1);
    TH_DONE(l);
}

TEST(bidi_rtl_point_outside)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    th_pt p = th_point(l, -20.0f, 3.0f);   CHK_PT(p, 3, 1, 1, 0);       /* left of everything: last logical cluster, trailing */
    p = th_point(l, 500.0f, 3.0f);         CHK_PT(p, 0, 1, 0, 0);       /* right of everything: first logical cluster, leading */
    p = th_point(l, 2 * CW, -50.0f);       CHK_PT(p, 1, 1, -1, 0);
    p = th_point(l, 4 * CW, 3.0f);         CHK_PT(p, 0, 1, 0, 1);       /* right edge inclusive */
    TH_DONE(l);
}

TEST(bidi_rtl_roundtrip_every_position)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 4; i++) {
        th_tp lead = th_pos(l, i, FALSE), trail = th_pos(l, i, TRUE);
        /* a point just inside the caret's cluster, on the caret side, hits that position with the matching flag */
        th_pt a = th_point(l, lead.x - 0.1f * CW, 3.0f);
        CHK_PT(a, i, 1, 0, 1);
        th_pt b = th_point(l, trail.x + 0.1f * CW, 3.0f);
        CHK_PT(b, i, 1, 1, 1);
    }
    TH_DONE(l);
}

TEST(bidi_rtl_paragraph_hebrew_right_aligned)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    DWRITE_TEXT_METRICS tm; th_metrics(l, &tm);
    CHK_NEAR(tm.left, 100.0f - 4 * CW);                       /* leading alignment is the right side */
    th_tp q = th_pos(l, 0, FALSE);
    CHK_NEAR(q.x, 100.0f); CHK_M(q.m, 0, 1, 100.0f - CW, CW, 1, 1);
    q = th_pos(l, 3, TRUE);    CHK_NEAR(q.x, 100.0f - 4 * CW);
    th_tp e = th_pos(l, 4, FALSE);   CHK_NEAR(e.x, 100.0f - 4 * CW);
    th_pt p = th_point(l, 99.0f, 3.0f);    CHK_PT(p, 0, 1, 0, 1);
    p = th_point(l, 5.0f, 3.0f);           CHK_PT(p, 3, 1, 1, 0);
    TH_DONE(l);
}

TEST(bidi_rtl_paragraph_latin_text_level_two)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    float left = 100.0f - 4 * CW; UINT32 i;
    for (i = 0; i < 4; i++) {
        th_tp q = th_pos(l, i, FALSE);
        CHK_M(q.m, i, 1, left + i * CW, CW, 2, 1);            /* Latin keeps its visual order inside the right-to-left line */
        CHK_NEAR(q.x, left + i * CW);
    }
    th_tp e = th_pos(l, 4, FALSE);
    CHK_NEAR(e.x, left + 4 * CW);
    th_pt p = th_point(l, left + 1.0f, 3.0f);   CHK_PT(p, 0, 1, 0, 1);
    TH_DONE(l);
}

TEST(bidi_rtl_paragraph_trailing_alignment_is_left)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    th_tp q = th_pos(l, 0, FALSE);
    CHK_NEAR(q.x, 0.0f);
    TH_DONE(l);
}

TEST(bidi_mixed_levels_and_positions)
{
    /* A0 B1 sp2 | alef3 bet4 gimel5 | sp6 C7 D8  ->  visual:  A B sp gimel bet alef sp C D */
    TH_BEGIN(); TH_LAYOUT(l, L"AB " HEB3 L" CD", 1000.0f, 200.0f);
    struct { UINT32 pos; float left; UINT32 lvl; } e[] = {
        { 0, 0, 0 }, { 1, 1, 0 }, { 2, 2, 0 }, { 3, 5, 1 }, { 4, 4, 1 }, { 5, 3, 1 }, { 6, 6, 0 }, { 7, 7, 0 }, { 8, 8, 0 } };
    unsigned i;
    for (i = 0; i < sizeof e / sizeof e[0]; i++) {
        th_tp q = th_pos(l, e[i].pos, FALSE);
        CHK_M(q.m, e[i].pos, 1, e[i].left * CW, CW, e[i].lvl, 1);
        CHK_NEAR(q.x, (e[i].lvl ? e[i].left + 1 : e[i].left) * CW);
        th_tp t = th_pos(l, e[i].pos, TRUE);
        CHK_NEAR(t.x, (e[i].lvl ? e[i].left : e[i].left + 1) * CW);
    }
    TH_DONE(l);
}

TEST(bidi_mixed_point_hits)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB " HEB3 L" CD", 1000.0f, 200.0f);
    UINT32 slot, logical[9] = { 0, 1, 2, 5, 4, 3, 6, 7, 8 };
    for (slot = 0; slot < 9; slot++) {
        int rtl = slot >= 3 && slot <= 5;
        th_pt p = th_point(l, slot * CW + 0.25f * CW, 3.0f);        /* left quarter: leading for LTR, trailing for RTL */
        CHK_PT(p, logical[slot], 1, rtl ? 1 : 0, 1);
        p = th_point(l, slot * CW + 0.75f * CW, 3.0f);
        CHK_PT(p, logical[slot], 1, rtl ? 0 : 1, 1);
    }
    TH_DONE(l);
}

TEST(bidi_mixed_point_outside_ends)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB " HEB3 L" CD", 1000.0f, 200.0f);
    th_pt p = th_point(l, -5.0f, 3.0f);     CHK_PT(p, 0, 1, 0, 0);
    p = th_point(l, 500.0f, 3.0f);          CHK_PT(p, 8, 1, 1, 0);
    TH_DONE(l);
}

TEST(bidi_mixed_end_position_ltr_last_span)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB " HEB3 L" CD", 1000.0f, 200.0f);
    th_tp q = th_pos(l, 9, FALSE);
    CHK_NEAR(q.x, 9 * CW); CHECK(q.m.textPosition == 9 && q.m.length == 0);
    TH_DONE(l);
}

TEST(bidi_mixed_end_position_rtl_last_span)
{
    /* the last span is right-to-left: the end caret sits at that span's left edge, which is visually left of the earlier text */
    TH_BEGIN(); TH_LAYOUT(l, L"AB " HEB3, 1000.0f, 200.0f);
    th_tp q = th_pos(l, 6, FALSE);
    CHECK(q.m.textPosition == 6 && q.m.length == 0);
    CHK_NEAR(q.x, 3 * CW);
    TH_DONE(l);
}

TEST(bidi_rtl_first_then_ltr)
{
    /* Hebrew first in an LTR paragraph: alef bet(0,1) sp(2) A B(3,4) -> visual: bet alef sp A B */
    TH_BEGIN(); TH_LAYOUT(l, HEB2 L" AB", 1000.0f, 200.0f);
    th_tp a = th_pos(l, 0, FALSE), b = th_pos(l, 1, FALSE), s = th_pos(l, 2, FALSE), c = th_pos(l, 3, FALSE);
    CHK_M(a.m, 0, 1, 1 * CW, CW, 1, 1);
    CHK_M(b.m, 1, 1, 0 * CW, CW, 1, 1);
    CHK_M(s.m, 2, 1, 2 * CW, CW, 0, 1);
    CHK_M(c.m, 3, 1, 3 * CW, CW, 0, 1);
    TH_DONE(l);
}

TEST(bidi_rtl_paragraph_mixed)
{
    /* RTL paragraph: alef bet sp A B -> visual from the right: alef, bet, sp, then "AB" reading left to right at the far left */
    TH_BEGIN(); TH_LAYOUT(l, HEB2 L" AB", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    float L0 = 100.0f - 5 * CW;
    th_tp a = th_pos(l, 0, FALSE), b = th_pos(l, 1, FALSE), s = th_pos(l, 2, FALSE), c = th_pos(l, 3, FALSE), d = th_pos(l, 4, FALSE);
    CHK_M(a.m, 0, 1, L0 + 4 * CW, CW, 1, 1);
    CHK_M(b.m, 1, 1, L0 + 3 * CW, CW, 1, 1);
    CHK_M(s.m, 2, 1, L0 + 2 * CW, CW, 1, 1);
    CHK_M(c.m, 3, 1, L0 + 0 * CW, CW, 2, 1);
    CHK_M(d.m, 4, 1, L0 + 1 * CW, CW, 2, 1);
    TH_DONE(l);
}

TEST(bidi_arabic_noted)
{
    /* Arabic joins through GSUB in real fonts; the synthetic font has no joining features, so only invariants are checked */
    TH_BEGIN(); TH_LAYOUT(l, L"\x0627\x0628\x0627", 1000.0f, 200.0f);
    th_tp q = th_pos(l, 0, FALSE);
    CHECK(q.m.bidiLevel == 1);
    CHECK(q.m.textPosition == 0 && q.m.length >= 1);
    th_tp r = th_pos(l, 0, TRUE);
    CHECK(r.x <= q.x);
    t_note("Arabic: cluster 0 length %u width %g, leading x %g trailing x %g", q.m.length, q.m.width, q.x, r.x);
    TH_DONE(l);
}

/* ---- ranges across directions ------------------------------------------------------------------------------------ */
TEST(bidi_range_full_mixed_gives_three_rects)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB " HEB3 L" CD", 1000.0f, 200.0f);
    th_rg r = th_range(l, 0, 9, 0, 0);
    CHECK(r.count == 3);
    if (r.count == 3) {
        CHK_M(r.m[0], 0, 3, 0.0f, 3 * CW, 0, 1);
        CHK_M(r.m[1], 3, 3, 3 * CW, 3 * CW, 1, 1);
        CHK_M(r.m[2], 6, 3, 6 * CW, 3 * CW, 0, 1);
    }
    TH_DONE(l);
}

TEST(bidi_range_partial_crossing_direction)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB " HEB3 L" CD", 1000.0f, 200.0f);
    th_rg r = th_range(l, 1, 4, 0, 0);                         /* B, space, alef, bet */
    CHECK(r.count == 2);
    if (r.count == 2) {
        CHK_M(r.m[0], 1, 2, 1 * CW, 2 * CW, 0, 1);
        CHK_M(r.m[1], 3, 2, 4 * CW, 2 * CW, 1, 1);             /* alef at slot 5, bet at slot 4 */
    }
    TH_DONE(l);
}

TEST(bidi_range_inside_rtl_run)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB4, 1000.0f, 200.0f);
    th_rg r = th_range(l, 1, 2, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 1, 2, 1 * CW, 2 * CW, 1, 1);
    r = th_range(l, 0, 4, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) CHK_M(r.m[0], 0, 4, 0.0f, 4 * CW, 1, 1);
    r = th_range(l, 0, 1, 0, 0);
    if (r.count == 1) CHK_M(r.m[0], 0, 1, 3 * CW, CW, 1, 1);
    TH_DONE(l);
}

TEST(bidi_range_rtl_paragraph_mixed)
{
    TH_BEGIN(); TH_LAYOUT(l, HEB2 L" AB", 100.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    float L0 = 100.0f - 5 * CW;
    th_rg r = th_range(l, 0, 5, 0, 0);
    CHECK(r.count == 2);
    if (r.count == 2) {
        CHK_M(r.m[0], 0, 3, L0 + 2 * CW, 3 * CW, 1, 1);
        CHK_M(r.m[1], 3, 2, L0, 2 * CW, 2, 1);
    }
    TH_DONE(l);
}

TEST(bidi_range_buffer_protocol_three_rects)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB " HEB3 L" CD", 1000.0f, 200.0f);
    DWRITE_HIT_TEST_METRICS m[4]; UINT32 n; HRESULT hr;
    memset(m, 0xcc, sizeof m); n = 0xdeadbeef;
    hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, m, 2, &n);
    CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(n == 3);
    { unsigned char *b = (unsigned char *)m; unsigned k, dirty = 0; for (k = 0; k < sizeof m; k++) if (b[k] != 0xcc) dirty++; CHECK(dirty == 0); }
    memset(m, 0xcc, sizeof m); n = 0xdeadbeef;
    hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, m, 3, &n);
    CHECK_HR_OK(hr); CHECK(n == 3);
    CHECK(m[3].textPosition == 0xcccccccc);                       /* nothing written past the count */
    memset(m, 0xcc, sizeof m); n = 0xdeadbeef;
    hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, m, 4, &n);
    CHECK_HR_OK(hr); CHECK(n == 3);
    CHECK(m[3].textPosition == 0xcccccccc);
    memset(m, 0xcc, sizeof m); n = 0xdeadbeef;
    hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, m, 0, &n);
    CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(n == 3);
    n = 0xdeadbeef;
    hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, NULL, 0, &n);
    CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(n == 3);
    TH_DONE(l);
}

TEST(bidi_range_count_follows_direction_changes)
{
    /* alternating runs: each direction change adds one rectangle */
    TH_BEGIN(); TH_LAYOUT(l, L"A " HEB1 L" B " HEB1 L" C", 1000.0f, 200.0f);
    th_rg r = th_range(l, 0, 9, 0, 0);
    UINT32 total = 0, i;
    for (i = 0; i < r.count; i++) total += r.m[i].length;
    CHECK(total == 9);
    CHECK(r.count == 5);                 /* [A sp] [alef] [sp B sp] [alef] [sp C]: neutrals between R and L resolve to L */
    t_note("A alef B alef C: %u rectangles", r.count);
    for (i = 0; i < r.count; i++) CHECK(r.m[i].bidiLevel <= 1);
    TH_DONE(l);
}
