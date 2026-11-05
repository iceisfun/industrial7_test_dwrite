/* IDWriteTextLayout::GetLineMetrics / GetClusterMetrics buffer protocol, IDWriteTextLayout3::GetLineMetrics (DWRITE_LINE_METRICS1)
 * and IDWriteTextLayout2::GetMetrics (DWRITE_TEXT_METRICS1).
 *
 * Both getters zero *actualCount first, then reject (array == NULL && max != 0) with E_INVALIDARG; otherwise they lay the text out,
 * report the real count and return E_NOT_SUFFICIENT_BUFFER (0x8007007A) when max < count. The line getter copies nothing unless
 * the whole array fits; the cluster getter fills the first `max` entries and still reports the full count.
 * A zero-length string gives one (empty) line and zero clusters. */
#include "tlm.h"

#define SENT 0xcc

static void fill_lines(DWRITE_LINE_METRICS *a, int n) { memset(a, SENT, sizeof(*a) * n); }
static int lines_untouched(const DWRITE_LINE_METRICS *a, int from, int n)
{
    const unsigned char *p = (const unsigned char *)(a + from); size_t i;
    for (i = 0; i < sizeof(*a) * (size_t)(n - from); i++) if (p[i] != SENT) return 0;
    return 1;
}
static int clusters_untouched(const DWRITE_CLUSTER_METRICS *a, int from, int n)
{
    const unsigned char *p = (const unsigned char *)(a + from); size_t i;
    for (i = 0; i < sizeof(*a) * (size_t)(n - from); i++) if (p[i] != SENT) return 0;
    return 1;
}

/* ---- GetLineMetrics ------------------------------------------------------------------------------------ */
TEST(lb_line_probe_null_zero)
{
    /* NULL array with max 0 is the sizing call: count filled, E_NOT_SUFFICIENT_BUFFER because count > 0 */
    TM_BEGIN(); TM_L(l, L"ABC", 1000, 1000);
    UINT32 n = 0x55;
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 1);
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_probe_multiline)
{
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 1000);
    UINT32 n = 0;
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 3);
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_null_nonzero_invalidarg)
{
    /* the count is zeroed before the argument check */
    TM_BEGIN(); TM_L(l, L"ABC", 1000, 1000);
    UINT32 n = 0x1234;
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 1, &n), DW_E_INVALIDARG);
    EXPECT_EQ(n, 0);
    n = 0x1234;
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 100, &n), DW_E_INVALIDARG);
    EXPECT_EQ(n, 0);
    n = 0x1234;
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0xffffffffu, &n), DW_E_INVALIDARG);
    EXPECT_EQ(n, 0);
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_actual_overwritten)
{
    TM_BEGIN(); TM_L(l, L"ABC", 1000, 1000);
    DWRITE_LINE_METRICS m[4]; UINT32 n = 0x7777;
    fill_lines(m, 4);
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, m, 4, &n));
    EXPECT_EQ(n, 1);
    n = 0;
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, m, 4, &n));
    EXPECT_EQ(n, 1);
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_exact_buffer)
{
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 1000);
    DWRITE_LINE_METRICS m[8]; UINT32 n = 0;
    fill_lines(m, 8);
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, m, 3, &n));
    EXPECT_EQ(n, 3);
    CHECK(lines_untouched(m, 3, 8));       /* only the first three entries are written */
    CHECK(m[0].length == 2 && m[1].length == 2 && m[2].length == 1);
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_too_small_writes_nothing)
{
    /* max < count: no entry is copied at all (all-or-nothing), the full count is reported */
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 1000);
    DWRITE_LINE_METRICS m[8]; UINT32 n = 0; UINT32 cap;
    for (cap = 1; cap <= 2; cap++) {
        fill_lines(m, 8); n = 0;
        CHECKF(IDWriteTextLayout_GetLineMetrics(l, m, cap, &n) == DW_E_NOT_SUFFICIENT_BUFFER, "cap %u", cap);
        EXPECT_EQ(n, 3);
        CHECKF(lines_untouched(m, 0, 8), "cap %u modified the array", cap);
    }
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_array_with_zero_max)
{
    TM_BEGIN(); TM_L(l, L"A\nB", 1000, 1000);
    DWRITE_LINE_METRICS m[4]; UINT32 n = 0;
    fill_lines(m, 4);
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, m, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 2);
    CHECK(lines_untouched(m, 0, 4));
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_oversized_buffer)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    DWRITE_LINE_METRICS m[16]; UINT32 n = 0;
    fill_lines(m, 16);
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, m, 16, &n));
    EXPECT_EQ(n, 1);
    CHECK(lines_untouched(m, 1, 16));
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_repeatable)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB CCC", 4 * CW, 1000);
    tm_lines a, b; UINT32 i;
    if (tm_get_lines(l, &a) && tm_get_lines(l, &b)) {
        EXPECT_EQ(a.n, b.n);
        for (i = 0; i < a.n && i < b.n; i++) CHECK(!memcmp(&a.v[i], &b.v[i], sizeof a.v[i]));
    }
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_empty_text_one_line)
{
    /* a zero-length layout still has one (empty) line */
    TM_BEGIN();
    IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"", 0, SZ, 100, 100);
    if (!l) { TM_END(); t_skip("CreateTextLayout failed"); return; }
    DWRITE_LINE_METRICS m[2]; UINT32 n = 0; fill_lines(m, 2);
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 1);
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, m, 2, &n));
    EXPECT_EQ(n, 1);
    EXPECT_EQ(m[0].length, 0); EXPECT_EQ(m[0].trailingWhitespaceLength, 0); EXPECT_EQ(m[0].newlineLength, 0);
    CHECK(!m[0].isTrimmed);
    CHECK(m[0].height >= 0.0f);
    DW_RELEASE(l); TM_END();
}
TEST(lb_line_empty_text_height)
{
    TM_BEGIN();
    IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"", 0, SZ, 100, 100);
    if (!l) { TM_END(); t_skip("CreateTextLayout failed"); return; }
    tm_lines ln; DWRITE_TEXT_METRICS tm;
    if (tm_get_lines(l, &ln)) {
        t_note("empty layout: line height %g baseline %g (non-empty line is %g / %g)", ln.v[0].height, ln.v[0].baseline, LH, ASC);
        tm_metrics(l, &tm);
        EXPECT_EQ(tm.lineCount, 1);
        NEAR(tm.height, ln.v[0].height);
        NEAR(tm.width, 0.0f);
    }
    DW_RELEASE(l); TM_END();
}

/* ---- GetClusterMetrics ---------------------------------------------------------------------------------- */
TEST(cb_probe_null_zero)
{
    TM_BEGIN(); TM_L(l, L"ABCDE", 1000, 1000);
    UINT32 n = 0x55;
    CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 5);
    DW_RELEASE(l); TM_END();
}
TEST(cb_null_nonzero_invalidarg)
{
    TM_BEGIN(); TM_L(l, L"ABCDE", 1000, 1000);
    UINT32 n = 0x1234;
    CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, NULL, 1, &n), DW_E_INVALIDARG);
    EXPECT_EQ(n, 0);
    n = 0x1234;
    CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, NULL, 0xffffffffu, &n), DW_E_INVALIDARG);
    EXPECT_EQ(n, 0);
    DW_RELEASE(l); TM_END();
}
TEST(cb_exact_buffer)
{
    TM_BEGIN(); TM_L(l, L"ABCDE", 1000, 1000);
    DWRITE_CLUSTER_METRICS m[10]; UINT32 n = 0x99; memset(m, SENT, sizeof m);
    CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(l, m, 5, &n));
    EXPECT_EQ(n, 5);
    CHECK(clusters_untouched(m, 5, 10));
    DW_RELEASE(l); TM_END();
}
TEST(cb_too_small_fills_prefix)
{
    /* the cluster getter fills the first `max` entries even when the buffer is too small, and reports the full count */
    TM_BEGIN(); TM_L(l, L"ABCDE", 1000, 1000);
    DWRITE_CLUSTER_METRICS m[10]; UINT32 n = 0, cap;
    for (cap = 1; cap <= 4; cap++) {
        memset(m, SENT, sizeof m); n = 0;
        CHECKF(IDWriteTextLayout_GetClusterMetrics(l, m, cap, &n) == DW_E_NOT_SUFFICIENT_BUFFER, "cap %u", cap);
        EXPECT_EQ(n, 5);
        { UINT32 i; for (i = 0; i < cap; i++) { NEAR(m[i].width, CW); EXPECT_EQ(m[i].length, 1); } }
        CHECKF(clusters_untouched(m, cap, 10), "cap %u wrote past the buffer", cap);
    }
    DW_RELEASE(l); TM_END();
}
TEST(cb_array_with_zero_max)
{
    TM_BEGIN(); TM_L(l, L"ABCDE", 1000, 1000);
    DWRITE_CLUSTER_METRICS m[4]; UINT32 n = 0; memset(m, SENT, sizeof m);
    CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, m, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 5);
    CHECK(clusters_untouched(m, 0, 4));
    DW_RELEASE(l); TM_END();
}
TEST(cb_empty_text_zero_clusters)
{
    /* zero clusters: nothing can be insufficient, so even the NULL/0 probe succeeds */
    TM_BEGIN();
    IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"", 0, SZ, 100, 100);
    if (!l) { TM_END(); t_skip("CreateTextLayout failed"); return; }
    DWRITE_CLUSTER_METRICS m[2]; UINT32 n = 0x77; memset(m, SENT, sizeof m);
    CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &n));
    EXPECT_EQ(n, 0);
    n = 0x77;
    CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(l, m, 2, &n));
    EXPECT_EQ(n, 0);
    CHECK(clusters_untouched(m, 0, 2));
    n = 0x77;
    CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, NULL, 5, &n), DW_E_INVALIDARG);
    EXPECT_EQ(n, 0);
    DW_RELEASE(l); TM_END();
}
TEST(cb_zero_length_of_longer_buffer)
{
    /* length argument shorter than the buffer: only the first `length` characters are laid out */
    TM_BEGIN();
    IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"ABCDEFGH", 3, SZ, 100, 100);
    if (!l) { TM_END(); t_skip("CreateTextLayout failed"); return; }
    tm_clusters c; tm_lines ln;
    if (tm_get_clusters(l, &c)) EXPECT_EQ(c.n, 3);
    if (tm_get_lines(l, &ln)) EXPECT_EQ(ln.v[0].length, 3);
    DW_RELEASE(l); TM_END();
}
TEST(cb_repeatable)
{
    TM_BEGIN(); TM_L(l, L"AA BB CC", 3 * CW, 1000);
    tm_clusters a, b; UINT32 i;
    if (tm_get_clusters(l, &a) && tm_get_clusters(l, &b)) {
        EXPECT_EQ(a.n, b.n);
        for (i = 0; i < a.n && i < b.n; i++) CHECK(!memcmp(&a.v[i], &b.v[i], sizeof a.v[i]));
    }
    DW_RELEASE(l); TM_END();
}
TEST(cb_padding_bits_zero)
{
    /* the reserved padding bits of every cluster are zero */
    TM_BEGIN(); TM_LF(l, FAM_UNI, L"A B\r\nC\t\x00AD" L"D", 1000, 1000);
    tm_clusters c; UINT32 i;
    if (tm_get_clusters(l, &c)) for (i = 0; i < c.n; i++) CHECKF(c.v[i].padding == 0, "cluster %u padding %#x", i, c.v[i].padding);
    DW_RELEASE(l); TM_END();
}
TEST(cb_metrics_independent_of_line_buffer)
{
    /* the two getters are independent views: calling one does not change what the other reports */
    TM_BEGIN(); TM_L(l, L"AAA BBB", 4 * CW, 1000);
    tm_clusters c1, c2; tm_lines l1, l2;
    if (tm_get_clusters(l, &c1) && tm_get_lines(l, &l1) && tm_get_clusters(l, &c2) && tm_get_lines(l, &l2)) {
        EXPECT_EQ(c1.n, c2.n); EXPECT_EQ(l1.n, l2.n);
        CHECK(!memcmp(c1.v, c2.v, sizeof c1.v[0] * c1.n));
        CHECK(!memcmp(l1.v, l2.v, sizeof l1.v[0] * l1.n));
    }
    DW_RELEASE(l); TM_END();
}

/* ---- re-layout after property changes --------------------------------------------------------------------- */
TEST(lb_relayout_after_maxwidth)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB CCC", 1000, 1000);
    UINT32 n = 0;
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 1);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 4 * CW));
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 3);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 1000));
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 1);
    DW_RELEASE(l); TM_END();
}
TEST(cb_relayout_after_fontsize)
{
    TM_BEGIN(); TM_L(l, L"ABC", 1000, 1000);
    tm_clusters c;
    if (tm_get_clusters(l, &c)) NEAR(c.v[0].width, CW);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(0, 3)));
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 3); NEAR(c.v[0].width, 2 * CW); NEAR(c.v[2].width, 2 * CW); }
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 8.0f, RNG(1, 1)));
    if (tm_get_clusters(l, &c)) { NEAR(c.v[0].width, 2 * CW); NEAR(c.v[1].width, 0.5f * CW); NEAR(c.v[2].width, 2 * CW); }
    DW_RELEASE(l); TM_END();
}
TEST(lb_failed_setter_keeps_metrics)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB CCC", 4 * CW, 1000);
    tm_lines a, b;
    if (tm_get_lines(l, &a)) {
        CHECK_HR(IDWriteTextLayout_SetMaxWidth(l, -1.0f), DW_E_INVALIDARG);
        if (tm_get_lines(l, &b)) { EXPECT_EQ(a.n, b.n); CHECK(!memcmp(a.v, b.v, sizeof a.v[0] * a.n)); }
    }
    DW_RELEASE(l); TM_END();
}
TEST(lb_same_text_two_layouts_same_metrics)
{
    TM_BEGIN(); TM_L(l1, L"AAA BBB CCC", 5 * CW, 1000); TM_L(l2, L"AAA BBB CCC", 5 * CW, 1000);
    tm_lines a, b;
    if (tm_get_lines(l1, &a) && tm_get_lines(l2, &b)) { EXPECT_EQ(a.n, b.n); CHECK(!memcmp(a.v, b.v, sizeof a.v[0] * a.n)); }
    DW_RELEASE(l1); DW_RELEASE(l2); TM_END();
}

/* ---- IDWriteTextLayout3::GetLineMetrics (DWRITE_LINE_METRICS1) ---------------------------------------------- */
TEST(lb1_probe_and_invalidarg)
{
    TM_BEGIN(); TM_L(l, L"A\nB", 1000, 1000);
    TM_QI(l3, IDWriteTextLayout3, l);
    UINT32 n = 0x55; DWRITE_LINE_METRICS1 m[4];
    CHECK_HR(IDWriteTextLayout3_GetLineMetrics(l3, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 2);
    n = 0x55;
    CHECK_HR(IDWriteTextLayout3_GetLineMetrics(l3, NULL, 3, &n), DW_E_INVALIDARG);
    EXPECT_EQ(n, 0);
    memset(m, SENT, sizeof m); n = 0;
    CHECK_HR(IDWriteTextLayout3_GetLineMetrics(l3, m, 1, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 2);
    { const unsigned char *p = (const unsigned char *)m; size_t i; int clean = 1; for (i = 0; i < sizeof m; i++) if (p[i] != SENT) clean = 0; CHECK(clean); }
    CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, m, 4, &n));
    EXPECT_EQ(n, 2);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TM_END();
}
TEST(lb1_prefix_matches_lb0)
{
    /* the first six fields of DWRITE_LINE_METRICS1 are DWRITE_LINE_METRICS; default spacing leaves no leading */
    TM_BEGIN(); TM_L(l, L"AAA BBB\nCC", 4 * CW, 1000);
    TM_QI(l3, IDWriteTextLayout3, l);
    tm_lines a; DWRITE_LINE_METRICS1 b[8]; UINT32 n = 0, i;
    if (tm_get_lines(l, &a)) {
        CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, b, 8, &n));
        EXPECT_EQ(n, a.n);
        for (i = 0; i < n && i < a.n; i++) {
            EXPECT_EQ(b[i].length, a.v[i].length); EXPECT_EQ(b[i].trailingWhitespaceLength, a.v[i].trailingWhitespaceLength);
            EXPECT_EQ(b[i].newlineLength, a.v[i].newlineLength); EXPECT_EQ(b[i].isTrimmed, a.v[i].isTrimmed);
            CHECK_FLOAT_EQ(b[i].height, a.v[i].height); CHECK_FLOAT_EQ(b[i].baseline, a.v[i].baseline);
            CHECK_FLOAT_EQ(b[i].leadingBefore, 0.0f); CHECK_FLOAT_EQ(b[i].leadingAfter, 0.0f);
        }
    }
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TM_END();
}

/* ---- IDWriteTextLayout2::GetMetrics (DWRITE_TEXT_METRICS1) -------------------------------------------------- */
TEST(tm1_prefix_matches_tm0)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB CCC", 5 * CW, 100);
    TM_QI(l2, IDWriteTextLayout2, l);
    DWRITE_TEXT_METRICS a; DWRITE_TEXT_METRICS1 b;
    tm_metrics(l, &a);
    memset(&b, 0xcc, sizeof b);
    CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l2, &b));
    CHECK_FLOAT_EQ(b.left, a.left); CHECK_FLOAT_EQ(b.top, a.top); CHECK_FLOAT_EQ(b.width, a.width);
    CHECK_FLOAT_EQ(b.widthIncludingTrailingWhitespace, a.widthIncludingTrailingWhitespace);
    CHECK_FLOAT_EQ(b.height, a.height); CHECK_FLOAT_EQ(b.layoutWidth, a.layoutWidth); CHECK_FLOAT_EQ(b.layoutHeight, a.layoutHeight);
    EXPECT_EQ(b.maxBidiReorderingDepth, a.maxBidiReorderingDepth); EXPECT_EQ(b.lineCount, a.lineCount);
    CHECK(b.heightIncludingTrailingWhitespace >= b.height);
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TM_END();
}
TEST(tm1_trailing_height_with_trailing_newline)
{
    /* a final empty line (text ends with a newline) is part of heightIncludingTrailingWhitespace; height may exclude it */
    TM_BEGIN(); TM_L(l, L"A\n", 1000, 1000);
    TM_QI(l2, IDWriteTextLayout2, l);
    DWRITE_TEXT_METRICS1 b; DWRITE_TEXT_METRICS a;
    memset(&b, 0xcc, sizeof b);
    CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l2, &b));
    tm_metrics(l, &a);
    EXPECT_EQ(a.lineCount, 2);
    CHECK(b.heightIncludingTrailingWhitespace >= b.height);
    t_note("TEXT_METRICS height %g / heightIncludingTrailingWhitespace %g for \"A\\n\"", b.height, b.heightIncludingTrailingWhitespace);
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TM_END();
}
