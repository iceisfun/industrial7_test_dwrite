/* IDWriteTextLayout2::SetLastLineWrapping / IDWriteTextFormat1::SetLastLineWrapping: object independence, invalidation and interplay.
 *
 * Win10 dwrite.dll: IDWriteTextLayout::SetLastLineWrapping stores (value != 0) in its own byte and always returns S_OK, IDWriteTextFormat::SetLastLineWrapping
 * stores (value != 0) in the format's byte; the getters return that byte (exactly 0 or 1). A layout copies the format's setting when it
 * is created, afterwards the two objects are independent. With the setting off, the line that crosses maxHeight takes all the remaining text of its paragraph.
 */
#include "g1.h"

#define LL_TEXT L"aaa bbb ccc ddd"
static float line_height(g1_fx *x)
{
    IDWriteTextLayout *p = g1_layout(x, LL_TEXT, G1_BIG, G1_BIG, G1_SIZE); g1_lines o; float h = 0.0f;
    if (p && SUCCEEDED(g1_get_lines(p, &o)) && o.n >= 1) h = o.lm[0].height;
    DW_RELEASE(p);
    return h;
}
#define LL_BEGIN() G1_BEGIN(); float H = line_height(&X); if (H <= 0.0f) { G1_END(); t_skip("line height probe failed"); return; }
static IDWriteTextLayout2 *as2(IDWriteTextLayout *l) { return (IDWriteTextLayout2 *)g1_qi((IUnknown *)l, &IID_IDWriteTextLayout2); }

TEST(gap1_ll_format_default_true_and_setters_return_ok)
{
    G1_BEGIN();
    IDWriteTextFormat *t = g1_format(&X, G1_SIZE); IDWriteTextFormat1 *t1 = t ? (IDWriteTextFormat1 *)g1_qi((IUnknown *)t, &IID_IDWriteTextFormat1) : NULL;
    if (t1) {
        EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), TRUE);
        CHECK_HR(IDWriteTextFormat1_SetLastLineWrapping(t1, FALSE), S_OK); EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), FALSE);
        CHECK_HR(IDWriteTextFormat1_SetLastLineWrapping(t1, FALSE), S_OK); EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), FALSE);
        CHECK_HR(IDWriteTextFormat1_SetLastLineWrapping(t1, TRUE), S_OK); EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), TRUE);
    } else t_skip("IDWriteTextFormat1 unsupported");
    DW_RELEASE(t1); DW_RELEASE(t); G1_END();
}
TEST(gap1_ll_value_table_reads_back_zero_or_one)
{
    static const int vals[] = { 0, 1, 2, -1, 0x100, 0x10000, 0x01000000, INT_MIN, INT_MAX, 12345678 };
    G1_BEGIN();
    IDWriteTextFormat *t = g1_format(&X, G1_SIZE); IDWriteTextFormat1 *t1 = t ? (IDWriteTextFormat1 *)g1_qi((IUnknown *)t, &IID_IDWriteTextFormat1) : NULL;
    IDWriteTextLayout *l = g1_layout(&X, LL_TEXT, 100.0f, 100.0f, G1_SIZE); IDWriteTextLayout2 *l2 = l ? as2(l) : NULL; unsigned i;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        if (t1) { CHECKF(IDWriteTextFormat1_SetLastLineWrapping(t1, vals[i]) == S_OK, "format Set(%d)", vals[i]); EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), vals[i] != 0); }
        if (l2) { CHECKF(IDWriteTextLayout2_SetLastLineWrapping(l2, vals[i]) == S_OK, "layout Set(%d)", vals[i]); EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), vals[i] != 0); }
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(t1); DW_RELEASE(t); G1_END();
}
TEST(gap1_ll_layout_copies_the_format_setting_at_creation)
{
    G1_BEGIN();
    IDWriteTextFormat *t = g1_format(&X, G1_SIZE); IDWriteTextFormat1 *t1 = t ? (IDWriteTextFormat1 *)g1_qi((IUnknown *)t, &IID_IDWriteTextFormat1) : NULL;
    if (t1) {
        int v;
        for (v = 1; v >= 0; v--) {
            IDWriteTextLayout *l = NULL; IDWriteTextLayout2 *l2;
            CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(t1, v));
            CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, LL_TEXT, 15, t, 100.0f, 100.0f, &l));
            l2 = l ? as2(l) : NULL;
            if (l2) { EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), v); DW_RELEASE(l2); }
            DW_RELEASE(l);
        }
    }
    DW_RELEASE(t1); DW_RELEASE(t); G1_END();
}
TEST(gap1_ll_layout_and_source_format_are_independent_afterwards)
{
    G1_BEGIN();
    IDWriteTextFormat *t = g1_format(&X, G1_SIZE); IDWriteTextFormat1 *t1 = t ? (IDWriteTextFormat1 *)g1_qi((IUnknown *)t, &IID_IDWriteTextFormat1) : NULL;
    IDWriteTextLayout *l = NULL; IDWriteTextLayout2 *l2 = NULL;
    if (t1 && SUCCEEDED(IDWriteFactory_CreateTextLayout(X.f, LL_TEXT, 15, t, 100.0f, 100.0f, &l)) && l && (l2 = as2(l)) != NULL) {
        CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(t1, FALSE));
        EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), TRUE);        /* the format changed after the layout was made */
        CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, TRUE));
        CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(t1, TRUE));
        CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
        EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), TRUE);        /* and the layout does not write back */
        EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), FALSE);
    } else t_skip("fixture unavailable");
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(t1); DW_RELEASE(t); G1_END();
}
TEST(gap1_ll_one_layout_object_three_views)
{
    G1_BEGIN();
    IDWriteTextLayout *l = g1_layout(&X, LL_TEXT, 100.0f, 100.0f, G1_SIZE);
    IDWriteTextLayout2 *l2 = l ? as2(l) : NULL; IDWriteTextFormat1 *f1 = l ? (IDWriteTextFormat1 *)g1_qi((IUnknown *)l, &IID_IDWriteTextFormat1) : NULL;
    IDWriteTextFormat3 *f3 = l ? (IDWriteTextFormat3 *)g1_qi((IUnknown *)l, &IID_IDWriteTextFormat3) : NULL;
    if (l2 && f1) {
        CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
        EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(f1), FALSE);
        if (f3) EXPECT_EQ(IDWriteTextFormat3_GetLastLineWrapping(f3), FALSE);
        CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(f1, TRUE));
        EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), TRUE);
        if (f3) { CHECK_HR_OK(IDWriteTextFormat3_SetLastLineWrapping(f3, FALSE)); EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), FALSE); }
    } else t_skip("interfaces unavailable");
    DW_RELEASE(f3); DW_RELEASE(f1); DW_RELEASE(l2); DW_RELEASE(l); G1_END();
}
TEST(gap1_ll_toggling_relayouts_and_restores)
{
    LL_BEGIN();
    IDWriteTextLayout *l = g1_layout(&X, LL_TEXT, G1_FIT(3), 1.5f * H, G1_SIZE); IDWriteTextLayout2 *l2 = l ? as2(l) : NULL;
    if (l2) {
        G1_LENS(l, 4, 4, 4, 3);                               /* on: every line is created whatever the height is */
        CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
        t_note("off, room for 2 line tops: {%s}", g1_lens_str(l));
        { g1_lines o; UINT32 sum = 0, i; g1_get_lines(l, &o); for (i = 0; i < o.n; i++) sum += o.lm[i].length; EXPECT_EQ(sum, 15); }
        CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, TRUE));
        G1_LENS(l, 4, 4, 4, 3);                               /* back to the original result */
    } else t_skip("IDWriteTextLayout2 unsupported");
    DW_RELEASE(l2); DW_RELEASE(l); G1_END();
}
TEST(gap1_ll_off_has_no_effect_when_height_is_ample)
{
    LL_BEGIN();
    static const float rows[] = { 4.5f, 10.0f, 100.0f };
    unsigned i;
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = g1_layout(&X, LL_TEXT, G1_FIT(3), rows[i] * H, G1_SIZE); IDWriteTextLayout2 *l2 = l ? as2(l) : NULL;
        if (l2) { CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE)); G1_LENS(l, 4, 4, 4, 3); }
        DW_RELEASE(l2); DW_RELEASE(l);
    }
    G1_END();
}
TEST(gap1_ll_off_with_no_wrapping_is_one_line)
{
    LL_BEGIN();
    IDWriteTextLayout *l = g1_layout(&X, LL_TEXT, G1_FIT(3), 0.5f * H, G1_SIZE); IDWriteTextLayout2 *l2 = l ? as2(l) : NULL;
    if (l2) {
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
        G1_LENS(l, 15);
        CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
        G1_LENS(l, 15);
    }
    DW_RELEASE(l2); DW_RELEASE(l); G1_END();
}
TEST(gap1_ll_off_height_change_relayouts)
{
    LL_BEGIN();
    IDWriteTextLayout *l = g1_layout(&X, LL_TEXT, G1_FIT(3), 0.5f * H, G1_SIZE); IDWriteTextLayout2 *l2 = l ? as2(l) : NULL;
    if (l2) {
        g1_lines a, b;
        CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
        CHECK_HR_OK(g1_get_lines(l, &a));
        CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 100.0f * H));
        G1_LENS(l, 4, 4, 4, 3);
        CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 0.5f * H));
        CHECK_HR_OK(g1_get_lines(l, &b));
        EXPECT_EQ(a.n, b.n);
        { UINT32 i; for (i = 0; i < a.n && i < b.n; i++) EXPECT_EQ(a.lm[i].length, b.lm[i].length); }
    }
    DW_RELEASE(l2); DW_RELEASE(l); G1_END();
}
TEST(gap1_ll_off_two_paragraphs_note)
{
    LL_BEGIN();
    static const WCHAR *texts[] = { L"aaa bbb\nccc ddd eee", L"aaa bbb ccc\r\nddd eee fff", L"aa\n\nbb cc dd ee" };
    unsigned i;
    for (i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        IDWriteTextLayout *l = g1_layout(&X, texts[i], G1_FIT(3), 1.5f * H, G1_SIZE); IDWriteTextLayout2 *l2 = l ? as2(l) : NULL;
        if (l2) {
            g1_lines o; UINT32 k, sum = 0, len = (UINT32)wcslen(texts[i]);
            t_note("text %u on: {%s}", i, g1_lens_str(l));
            CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
            CHECK_HR_OK(g1_get_lines(l, &o));
            for (k = 0; k < o.n; k++) sum += o.lm[k].length;
            EXPECT_EQ(sum, len);                                   /* no text is lost */
            t_note("text %u off: {%s}", i, g1_lens_str(l));
        }
        DW_RELEASE(l2); DW_RELEASE(l);
    }
    G1_END();
}
TEST(gap1_ll_off_preserves_total_length_sweep)
{
    LL_BEGIN();
    static const float rows[] = { 0.0f, 0.25f, 0.99f, 1.0f, 1.01f, 1.5f, 2.0f, 2.5f, 3.0f, 3.99f, 4.0f };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextLayout *l = g1_layout(&X, LL_TEXT, G1_FIT(3), rows[i] * H, G1_SIZE); IDWriteTextLayout2 *l2 = l ? as2(l) : NULL;
        if (l2) {
            g1_lines o; UINT32 k, sum = 0;
            CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
            CHECK_HR_OK(g1_get_lines(l, &o));
            for (k = 0; k < o.n; k++) { sum += o.lm[k].length; CHECK(o.lm[k].length > 0); }
            CHECKF(sum == 15, "rows %g: line lengths sum to %u", rows[i], sum);
            CHECKF(o.n >= 1 && o.n <= 4, "rows %g: %u lines", rows[i], o.n);
        }
        DW_RELEASE(l2); DW_RELEASE(l);
    }
    G1_END();
}
TEST(gap1_ll_off_is_ignored_by_metrics_text_only_properties_note)
{
    LL_BEGIN();
    IDWriteTextLayout *l = g1_layout(&X, LL_TEXT, G1_FIT(3), 1.5f * H, G1_SIZE); IDWriteTextLayout2 *l2 = l ? as2(l) : NULL;
    if (l2) {
        DWRITE_TEXT_METRICS on, off;
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &on));
        CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &off));
        t_note("on: %u lines width %g height %g, off: %u lines width %g height %g", on.lineCount, on.width, on.height, off.lineCount, off.width, off.height);
        EXPECT_EQ(on.lineCount > 0, 1);
        CHECK(off.lineCount >= 1 && off.lineCount <= on.lineCount);
    }
    DW_RELEASE(l2); DW_RELEASE(l); G1_END();
}
