/* Additional line / cluster / text metrics relations. */
#include "tlm.h"

TEST(ex_line_lengths_tile_text)
{
    /* line lengths always add up to the text length for every wrapping mode */
    TM_BEGIN();
    static const DWRITE_WORD_WRAPPING modes[] = { DWRITE_WORD_WRAPPING_WRAP, DWRITE_WORD_WRAPPING_NO_WRAP, DWRITE_WORD_WRAPPING_WHOLE_WORD, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK };
    size_t k;
    for (k = 0; k < 4; k++) {
        TM_L(l, L"alpha beta gamma\ndelta  epsilon zeta", 6 * CW, 1000); tm_lines ln;
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, modes[k]));
        if (tm_get_lines(l, &ln)) CHECKF(tm_sum_line_len(&ln) == 36, "mode %zu sum %u", k, tm_sum_line_len(&ln));
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(ex_cluster_lengths_tile_text)
{
    TM_BEGIN(); TM_LF(l, FAM_UNI, L"ab\x0301 c\xD835\xDC00 d\r\ne\t\x00AD" L"f", 6 * CW, 1000);
    tm_clusters c;
    if (tm_get_clusters(l, &c)) EXPECT_EQ(tm_sum_cluster_len(&c), 15);
    DW_RELEASE(l); TM_END();
}
TEST(ex_metrics_height_equals_line_sum_after_resize)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB CCC DDD", 1000, 1000);
    UINT32 w;
    for (w = 1; w <= 12; w++) {
        tm_lines ln; DWRITE_TEXT_METRICS m;
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, w * CW));
        if (tm_get_lines(l, &ln)) { tm_metrics(l, &m); EXPECT_EQ(m.lineCount, ln.n); NEAR(m.height, tm_sum_line_height(&ln)); }
    }
    DW_RELEASE(l); TM_END();
}
TEST(ex_width_incl_ws_ge_width_sweep)
{
    TM_BEGIN(); TM_L(l, L"AA  BB   CC ", 1000, 1000);
    UINT32 w;
    for (w = 1; w <= 14; w++) {
        DWRITE_TEXT_METRICS m;
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, w * CW));
        tm_metrics(l, &m);
        CHECKF(m.widthIncludingTrailingWhitespace >= m.width, "w=%u %g < %g", w, m.widthIncludingTrailingWhitespace, m.width);
    }
    DW_RELEASE(l); TM_END();
}
TEST(ex_line_trailing_ws_le_length)
{
    TM_BEGIN(); TM_L(l, L"A  B   \nC \n  ", 3 * CW, 1000);
    tm_lines ln; UINT32 i;
    if (tm_get_lines(l, &ln)) for (i = 0; i < ln.n; i++)
        CHECKF(ln.v[i].trailingWhitespaceLength <= ln.v[i].length, "line %u", i);
    DW_RELEASE(l); TM_END();
}
TEST(ex_set_layout_height_no_effect_on_lines)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB", 4 * CW, 1000);
    tm_lines a, b;
    if (tm_get_lines(l, &a)) {
        CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 5000.0f));
        if (tm_get_lines(l, &b)) { EXPECT_EQ(a.n, b.n); CHECK(!memcmp(a.v, b.v, sizeof a.v[0] * a.n)); }
    }
    DW_RELEASE(l); TM_END();
}
TEST(ex_resize_roundtrip_same_cluster_metrics)
{
    TM_BEGIN(); TM_L(l1, L"AB CD", 1000, 1000); TM_L(l2, L"AB CD", 1000, 1000);
    tm_clusters a, b;
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l2, 2 * CW));
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l2, 1000));
    if (tm_get_clusters(l1, &a) && tm_get_clusters(l2, &b)) { EXPECT_EQ(a.n, b.n); CHECK(!memcmp(a.v, b.v, sizeof a.v[0] * a.n)); }
    DW_RELEASE(l1); DW_RELEASE(l2); TM_END();
}
TEST(ex_line_baseline_le_height_default)
{
    TM_BEGIN(); TM_L(l, L"A\nBB\n\nC", 1000, 1000);
    tm_lines ln; UINT32 i;
    if (tm_get_lines(l, &ln)) for (i = 0; i < ln.n; i++) {
        CHECKF(ln.v[i].baseline > 0 && ln.v[i].baseline <= ln.v[i].height, "line %u", i);
        NEAR(ln.v[i].height, LH); NEAR(ln.v[i].baseline, ASC);
    }
    DW_RELEASE(l); TM_END();
}
