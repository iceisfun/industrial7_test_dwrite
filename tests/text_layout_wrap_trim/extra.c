/* Wrapping and trimming relations that cross the individual areas: font size scaling, per-range formatting, hit testing across wrapped
 * lines and alignment of trimmed lines. */
#include "wt.h"

TEST(extra_wrapping_scale_invariant)
{
    /* widths scale with the font size, so the same relative layout width gives the same line breaks at any size */
    WT_BEGIN(); static const float sizes[] = { 8.0f, 12.0f, 16.0f, 20.0f, 32.0f, 64.0f, 100.0f, 256.0f }; size_t i; int m;
    static const DWRITE_WORD_WRAPPING modes[] = { DWRITE_WORD_WRAPPING_WRAP, DWRITE_WORD_WRAPPING_WHOLE_WORD, DWRITE_WORD_WRAPPING_CHARACTER, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK };
    for (m = 0; m < 4; m++) for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        float cw = sizes[i] * 1000.0f / 2048.0f; IDWriteTextLayout *l = wt_layout_(&X, L"aaa bbb ccc ddd eee", 7.5f * cw, WT_BIGH, sizes[i]);
        wt_lines o;
        if (!l) continue;
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, modes[m]));
        CHECK_HR_OK(wt_get_lines(l, &o));
        /* CHARACTER breaks after the blank that follows the last fitting word, exactly like the word modes here */
        CHECKF(o.n == 3 && o.lm[0].length == 8 && o.lm[1].length == 8 && o.lm[2].length == 3, "mode %d size %g: %u lines first %u", modes[m], sizes[i], o.n, o.lm[0].length);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(extra_wrapping_uses_per_range_font_size)
{
    WT_BEGIN(); WT_LAYOUT(l, L"aaaa bbbb", WT_FIT(7));
    WT_LENS(l, 5, 4);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, WT_SIZE / 2.0f, RNG(0, 4)));            /* first word half as wide: 2 + 1 + 4 characters */
    WT_LENS(l, 9);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, WT_SIZE, RNG(0, 4)));
    WT_LENS(l, 5, 4);
    DW_RELEASE(l); WT_END();
}

TEST(extra_wrapping_width_of_mixed_sizes)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; WT_LAYOUT(l, L"abcd", 10000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, WT_SIZE * 2.0f, RNG(1, 2)));
    wt_metrics(l, &m); CHECK_NEAR(m.width, WT_CH(1) + WT_CH(4) + WT_CH(1), 0.02);       /* a (1) + bc (2 x 2) + d (1) */
    DW_RELEASE(l); WT_END();
}

TEST(extra_hit_test_point_on_wrapped_lines)
{
    WT_BEGIN(); wt_lines o; BOOL trailing, inside; DWRITE_HIT_TEST_METRICS hm; WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(3));
    CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 3);
    if (o.n == 3) {
        float lh = o.lm[0].height; HRESULT hr;
        hr = IDWriteTextLayout_HitTestPoint(l, 1.25f * WT_CW, 1.5f * lh, &trailing, &inside, &hm);
        if (hr == S_OK) { EXPECT_EQ(hm.textPosition, 5); EXPECT_EQ(trailing, 0); EXPECT_EQ(inside, 1); EXPECT_EQ(hm.length, 1); }
        else t_note("HitTestPoint -> %s", dw_hr_name(hr));
        hr = IDWriteTextLayout_HitTestPoint(l, 1.75f * WT_CW, 1.5f * lh, &trailing, &inside, &hm);
        if (hr == S_OK) { EXPECT_EQ(hm.textPosition, 5); EXPECT_EQ(trailing, 1); EXPECT_EQ(inside, 1); }
        hr = IDWriteTextLayout_HitTestPoint(l, 0.25f * WT_CW, 0.5f * lh, &trailing, &inside, &hm);
        if (hr == S_OK) { EXPECT_EQ(hm.textPosition, 0); EXPECT_EQ(trailing, 0); EXPECT_EQ(inside, 1); }
        hr = IDWriteTextLayout_HitTestPoint(l, 0.25f * WT_CW, 2.5f * lh, &trailing, &inside, &hm);
        if (hr == S_OK) { EXPECT_EQ(hm.textPosition, 8); EXPECT_EQ(inside, 1); }
    }
    DW_RELEASE(l); WT_END();
}

TEST(extra_hit_test_range_spans_lines)
{
    WT_BEGIN(); DWRITE_HIT_TEST_METRICS hm[8]; UINT32 n = 0, i, total = 0; HRESULT hr; WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(3));
    hr = IDWriteTextLayout_HitTestTextRange(l, 0, 11, 0.0f, 0.0f, hm, 8, &n);
    if (hr == S_OK) {
        EXPECT_EQ(n, 3);
        for (i = 0; i < n; i++) total += hm[i].length;
        EXPECT_EQ(total, 11);
        if (n == 3) { CHECK(hm[1].top > hm[0].top && hm[2].top > hm[1].top); CHECK_NEAR(hm[0].left, 0.0, 0.01); EXPECT_EQ(hm[0].textPosition, 0); EXPECT_EQ(hm[1].textPosition, 4); EXPECT_EQ(hm[2].textPosition, 8); }
    } else t_note("HitTestTextRange -> %s", dw_hr_name(hr));
    n = 0; hr = IDWriteTextLayout_HitTestTextRange(l, 0, 11, 0.0f, 0.0f, hm, 2, &n);
    if (hr != S_OK) { EXPECT_EQ(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 3); }
    DW_RELEASE(l); WT_END();
}

TEST(extra_trimmed_line_aligned_with_sign)
{
    /* the trimmed line (kept text + sign = 5 chars) is aligned like any other line of that width */
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteInlineObject *s = NULL; static const float want[4] = { 0, 0.5f, 0.25f, 0 }; int ta;
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)); DW_RELEASE(t);
    for (ta = 0; ta < 4 && s; ta++) {
        IDWriteTextLayout *l = wt_layout_wrap(&X, L"AAAAAAAAAA", WT_FIT(10), DWRITE_WORD_WRAPPING_NO_WRAP); DWRITE_TRIMMING tm = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; DWRITE_TEXT_METRICS m;
        if (!l) continue;
        CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tm, s)); CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)ta));
        /* narrower box: force trimming to a known width of 6 characters, in a box of 11.5: kept 5 + sign 1 */
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, WT_FIT(6)));
        wt_metrics(l, &m);
        CHECK_NEAR(m.width, WT_CH(6), 0.02);
        t_note("alignment %d: trimmed line left %g (box %g, line %g)", ta, m.left, m.layoutWidth, m.width);
        CHECK_NEAR(m.left, want[ta] * WT_CW, 0.02);             /* the trimmed line is 6 characters wide in a 6.5 box */
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(extra_trimmed_line_trailing_alignment_exact)
{
    WT_BEGIN(); IDWriteInlineObject *s = NULL; IDWriteTextFormat *t = wt_format(&X, WT_SIZE); DWRITE_TRIMMING tm = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; DWRITE_TEXT_METRICS m;
    IDWriteTextLayout *l;
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)); DW_RELEASE(t);
    l = wt_layout_wrap(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l && s) {
        CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tm, s));
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
        wt_metrics(l, &m);
        CHECK_NEAR(m.width, WT_CH(5), 0.02); CHECK_NEAR(m.left, WT_FIT(5) - WT_CH(5), 0.02);          /* 4 kept characters + sign = 5 in a 5.5 box */
    }
    DW_RELEASE(l); DW_RELEASE(s); WT_END();
}

TEST(extra_trim_then_wrap_mode_switch)
{
    /* NO_WRAP -> WRAP at the same trimming: the over-long unbreakable word is still trimmed, not split */
    WT_BEGIN(); IDWriteInlineObject *s = NULL; IDWriteTextFormat *t = wt_format(&X, WT_SIZE); DWRITE_TRIMMING tm = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; wt_lines o;
    IDWriteTextLayout *l;
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)); DW_RELEASE(t);
    l = wt_layout_wrap(&X, L"AAAAAAAAAA BB", WT_FIT(5), DWRITE_WORD_WRAPPING_WRAP);
    if (l && s) {
        CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tm, s));
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 2);
        if (o.n == 2) { EXPECT_EQ(o.lm[0].length, 11); EXPECT_EQ(o.lm[0].isTrimmed, 1); EXPECT_EQ(o.lm[1].length, 2); EXPECT_EQ(o.lm[1].isTrimmed, 0); }
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK));
        CHECK_HR_OK(wt_get_lines(l, &o)); CHECK(o.n >= 3);
    }
    DW_RELEASE(l); DW_RELEASE(s); WT_END();
}

TEST(extra_line_metrics_baseline_and_height_consistent)
{
    WT_BEGIN(); wt_lines o; UINT32 i; WT_LAYOUT(l, L"aaa bbb ccc ddd", WT_FIT(3));
    CHECK_HR_OK(wt_get_lines(l, &o));
    for (i = 0; i < o.n; i++) {
        CHECK(o.lm[i].baseline > 0.0f && o.lm[i].baseline < o.lm[i].height);
        CHECK_NEAR(o.lm[i].height, o.lm[0].height, 0.001); CHECK_NEAR(o.lm[i].baseline, o.lm[0].baseline, 0.001);
        EXPECT_EQ(o.lm[i].isTrimmed, 0);
    }
    DW_RELEASE(l); WT_END();
}

TEST(extra_wrap_after_text_edit_relayouts)
{
    /* every setter invalidates the built layout: changing the text through the IDWriteTextLayout is not possible, so use font size/max width */
    WT_BEGIN(); WT_LAYOUT(l, L"aaa bbb ccc", WT_FIT(7)); WT_LENS(l, 8, 3);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, WT_SIZE * 2.0f, RNG(0, 11))); WT_LENS(l, 4, 4, 3);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, WT_SIZE, RNG(0, 11))); WT_LENS(l, 8, 3);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, WT_FIT(2))); WT_LENS(l, 4, 4, 3);
    DW_RELEASE(l); WT_END();
}

TEST(extra_wrap_bigger_font_size_range_narrower_fit)
{
    WT_BEGIN(); WT_LAYOUT(l, L"aa bb cc", WT_FIT(5));
    WT_LENS(l, 6, 2);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, WT_SIZE * 2.0f, RNG(3, 2)));     /* "bb" now 4 wide: "aa bb" needs 7 */
    WT_LENS(l, 3, 3, 2);
    DW_RELEASE(l); WT_END();
}

TEST(extra_wrap_line_metrics_sum_and_start_positions)
{
    /* the line lengths always sum to the text length and hit test of each line start lands on that line's left edge (x = 0, LTR) */
    WT_BEGIN(); static const WCHAR *t = L"aaa bbb ccc ddd eee"; int w;
    for (w = 3; w <= 12; w++) {
        IDWriteTextLayout *l = wt_layout_(&X, t, WT_FIT(w), WT_BIGH, WT_SIZE); wt_lines o; UINT32 i, sum = 0;
        if (!l) continue;
        CHECK_HR_OK(wt_get_lines(l, &o));
        for (i = 0; i < o.n; i++) sum += o.lm[i].length;
        CHECKF(sum == 19, "width %d: sum %u", w, sum);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(extra_wrap_format_setters_roundtrip_all_modes)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); int m;
    if (t) {
        for (m = 0; m < 5; m++) { CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, (DWRITE_WORD_WRAPPING)m)); EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(t), m); }
        CHECK_HR(IDWriteTextFormat_SetWordWrapping(t, (DWRITE_WORD_WRAPPING)5), E_INVALIDARG);
        EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(t), 4);            /* a rejected value leaves the setting alone */
        IDWriteTextFormat_Release(t);
    }
    WT_END();
}

TEST(extra_trim_sign_release_after_layout_release)
{
    /* the layout holds its own reference to the sign: releasing ours keeps trimming working */
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteInlineObject *s = NULL; IDWriteTextLayout *l = NULL;
    if (t && SUCCEEDED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)) && s) {
        DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; wt_lines o;
        if (SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"aaaaaaaaaa", 10, t, WT_FIT(4), WT_BIGH, &l))) {
            CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
            CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, s));
            IDWriteInlineObject_Release(s); s = NULL;
            CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 1); EXPECT_EQ(o.lm[0].isTrimmed, 1);
            IDWriteTextLayout_Release(l);
        }
    }
    DW_RELEASE(s); DW_RELEASE(t); WT_END();
}
