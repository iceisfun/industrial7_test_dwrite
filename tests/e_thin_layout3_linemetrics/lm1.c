/* IDWriteTextLayout3::GetLineMetrics (DWRITE_LINE_METRICS1) over the synthetic DwTest family (see text_layout_core/tl.h).
 * At font size 2048: every character 1000 DIPs wide, ascent 1800, descent 450, natural line height 2250. */
#include "../text_layout_core/tl.h"

#define LAYOUT(var, text, w) TL_LAYOUT(var, text, TL_UPM, (w), 100000.0f)
#define SZ_NOT_SUFF ((HRESULT)0x8007007A)

static IDWriteTextLayout3 *qi3(IDWriteTextLayout *l) { return (IDWriteTextLayout3 *)tl_qi((IUnknown *)l, &IID_IDWriteTextLayout3, "IDWriteTextLayout3"); }

static void fill(DWRITE_LINE_METRICS1 *m, UINT32 n) { memset(m, 0xCC, n * sizeof *m); }

TEST(lm1_default_two_lines)
{
    TL_BEGIN(); LAYOUT(l, L"ABC\nDE", 100000.0f); IDWriteTextLayout3 *l3 = qi3(l); if (!l3) { DW_RELEASE(l); TL_END(); return; }
    DWRITE_LINE_METRICS1 m[4]; UINT32 n = 99; fill(m, 4);
    CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, m, 4, &n));
    EXPECT_EQ(n, 2);
    EXPECT_EQ(m[0].length, 4); EXPECT_EQ(m[0].newlineLength, 1); EXPECT_EQ(m[0].trailingWhitespaceLength, 1);
    EXPECT_EQ(m[1].length, 2); EXPECT_EQ(m[1].newlineLength, 0); EXPECT_EQ(m[1].trailingWhitespaceLength, 0);
    CHECK_FBITS(m[0].height, 2250.0f); CHECK_FBITS(m[0].baseline, 1800.0f);
    CHECK_FBITS(m[1].height, 2250.0f); CHECK_FBITS(m[1].baseline, 1800.0f);
    CHECK_FBITS(m[0].leadingBefore, 0.0f); CHECK_FBITS(m[0].leadingAfter, 0.0f);
    CHECK_FBITS(m[1].leadingBefore, 0.0f); CHECK_FBITS(m[1].leadingAfter, 0.0f);
    EXPECT_EQ(m[0].isTrimmed, FALSE); EXPECT_EQ(m[1].isTrimmed, FALSE);
    /* untouched entries stay untouched */
    EXPECT_EQ(m[2].length, 0xCCCCCCCCu);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(lm1_count_query_and_small_buffer)
{
    TL_BEGIN(); LAYOUT(l, L"A\nB\nC", 100000.0f); IDWriteTextLayout3 *l3 = qi3(l); if (!l3) { DW_RELEASE(l); TL_END(); return; }
    DWRITE_LINE_METRICS1 m[1]; UINT32 n = 0;
    CHECK_HR(IDWriteTextLayout3_GetLineMetrics(l3, NULL, 0, &n), SZ_NOT_SUFF); EXPECT_EQ(n, 3);
    fill(m, 1); n = 0;
    CHECK_HR(IDWriteTextLayout3_GetLineMetrics(l3, m, 1, &n), SZ_NOT_SUFF); EXPECT_EQ(n, 3);
    {   /* negative control: a large enough buffer succeeds and reports the same count */
        DWRITE_LINE_METRICS1 big[3]; n = 0; fill(big, 3);
        CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, big, 3, &n)); EXPECT_EQ(n, 3);
        EXPECT_EQ(big[2].length, 1);
    }
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(lm1_proportional_leading_split)
{
    TL_BEGIN(); LAYOUT(l, L"AB\nCD", 100000.0f); IDWriteTextLayout3 *l3 = qi3(l); if (!l3) { DW_RELEASE(l); TL_END(); return; }
    DWRITE_LINE_SPACING ls = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 1.0f, 0.25f, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT };
    DWRITE_LINE_METRICS1 m[2]; UINT32 n = 0; fill(m, 2);
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &ls));
    CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, m, 2, &n)); EXPECT_EQ(n, 2);
    /* total 4500, ascent 1800, extra 2250 split 25% / 75% */
    CHECK_FBITS(m[0].height, 4500.0f); CHECK_FBITS(m[0].leadingBefore, 562.5f); CHECK_FBITS(m[0].leadingAfter, 1687.5f);
    CHECK_FBITS(m[0].baseline, 2362.5f);
    CHECK_FBITS(m[1].height, 4500.0f); CHECK_FBITS(m[1].leadingBefore, 562.5f); CHECK_FBITS(m[1].leadingAfter, 1687.5f);
    /* negative control: the fraction is what moves the split */
    ls.leadingBefore = 0.5f;
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &ls)); fill(m, 2);
    CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, m, 2, &n));
    CHECK_FBITS(m[0].leadingBefore, 1125.0f); CHECK_FBITS(m[0].leadingAfter, 1125.0f); CHECK_FBITS(m[0].baseline, 2925.0f);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(lm1_uniform_has_no_split_fields_in_v0_view)
{
    TL_BEGIN(); LAYOUT(l, L"AB", 100000.0f); IDWriteTextLayout3 *l3 = qi3(l); if (!l3) { DW_RELEASE(l); TL_END(); return; }
    DWRITE_LINE_SPACING ls = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 3000.0f, 2400.0f, 0.0f, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT };
    DWRITE_LINE_METRICS1 m1; DWRITE_LINE_METRICS m0; UINT32 n = 0;
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &ls));
    CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, &m1, 1, &n));
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, &m0, 1, &n));
    CHECK_FBITS(m1.height, 3000.0f); CHECK_FBITS(m1.baseline, 2400.0f);
    CHECK_FBITS(m0.height, m1.height); CHECK_FBITS(m0.baseline, m1.baseline);
    EXPECT_EQ(m0.length, m1.length); EXPECT_EQ(m0.newlineLength, m1.newlineLength);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(lm1_wrapped_lines_trailing_whitespace)
{
    TL_BEGIN(); LAYOUT(l, L"AB CD EF", 3000.0f); IDWriteTextLayout3 *l3 = qi3(l); if (!l3) { DW_RELEASE(l); TL_END(); return; }
    DWRITE_LINE_METRICS1 m[4]; UINT32 n = 0; fill(m, 4);
    CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, m, 4, &n));
    EXPECT_EQ(n, 3);
    EXPECT_EQ(m[0].length, 3); EXPECT_EQ(m[0].trailingWhitespaceLength, 1); EXPECT_EQ(m[0].newlineLength, 0);
    EXPECT_EQ(m[1].length, 3); EXPECT_EQ(m[1].trailingWhitespaceLength, 1);
    EXPECT_EQ(m[2].length, 2); EXPECT_EQ(m[2].trailingWhitespaceLength, 0);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}
