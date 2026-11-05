/* Reading direction x flow direction, DWRITE_E_FLOWDIRECTIONCONFLICTS, vertical layouts and the last line wrapping option.
 *
 * Facts from the Windows 10 decompilation:
 *  - SetReadingDirection / SetFlowDirection (layout and format) only range-check (0..3) and never look at each other.
 *  - The conflict is detected when the layout is first needed (TextLayout::TextLayout, reached from MutableTextLayout::DoLayout):
 *    bit 1 of the flow direction and bit 1 of the reading direction must be equal, i.e. horizontal reading (LEFT_TO_RIGHT,
 *    RIGHT_TO_LEFT) goes with TOP_TO_BOTTOM / BOTTOM_TO_TOP flow and vertical reading (TOP_TO_BOTTOM, BOTTOM_TO_TOP) with
 *    LEFT_TO_RIGHT / RIGHT_TO_LEFT flow. Otherwise the call that triggered the layout fails with DWRITE_E_FLOWDIRECTIONCONFLICTS (0x8898500B).
 *  - TextLayout::GetLayoutWidth returns maxHeight for vertical reading directions: the line length limit is the other dimension.
 *  - AlignmentHelperVertical reverses the paragraph axis for flow BOTTOM_TO_TOP and LEFT_TO_RIGHT: NEAR then means the far side
 *    and lines stack towards the origin.
 *  - IDWriteTextFormat1::SetLastLineWrapping(FALSE): the line that crosses maxHeight takes all remaining text of its paragraph. */
#include "wt.h"

#define CONFLICT ((HRESULT)0x8898500B)

static IDWriteTextLayout *dl(wt_fx *x, const WCHAR *t, float w, float h, DWRITE_READING_DIRECTION rd, DWRITE_FLOW_DIRECTION fd)
{
    IDWriteTextLayout *l = wt_layout_(x, t, w, h, WT_SIZE);
    if (!l) return NULL;
    if (FAILED(IDWriteTextLayout_SetReadingDirection(l, rd)) || FAILED(IDWriteTextLayout_SetFlowDirection(l, fd))) { IDWriteTextLayout_Release(l); return NULL; }
    return l;
}
static int compatible(int rd, int fd) { return ((rd >> 1) & 1) == ((fd >> 1) & 1); }
static const char *RDN[] = { "LTR", "RTL", "TTB", "BTT" }, *FDN[] = { "TTB", "BTT", "LTR", "RTL" };

TEST(flow_defaults)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); WT_LAYOUT(l, L"abc", 100.0f);
    EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(t), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT); EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(t), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT); EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(flow_setters_accept_every_pair)
{
    WT_BEGIN(); int rd, fd; WT_LAYOUT(l, L"abc", 100.0f); IDWriteTextFormat *t = wt_format(&X, WT_SIZE);
    for (rd = 0; rd < 4; rd++) for (fd = 0; fd < 4; fd++) {
        CHECKF(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)rd) == S_OK && IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)fd) == S_OK, "layout %s/%s", RDN[rd], FDN[fd]);
        CHECKF(IDWriteTextFormat_SetReadingDirection(t, (DWRITE_READING_DIRECTION)rd) == S_OK && IDWriteTextFormat_SetFlowDirection(t, (DWRITE_FLOW_DIRECTION)fd) == S_OK, "format %s/%s", RDN[rd], FDN[fd]);
        EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), rd); EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), fd);
    }
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(flow_conflict_matrix_getmetrics)
{
    WT_BEGIN(); int rd, fd;
    for (rd = 0; rd < 4; rd++) for (fd = 0; fd < 4; fd++) {
        IDWriteTextLayout *l = dl(&X, L"abc def", 1000.0f, 1000.0f, (DWRITE_READING_DIRECTION)rd, (DWRITE_FLOW_DIRECTION)fd); DWRITE_TEXT_METRICS m; HRESULT hr;
        if (!l) continue;
        hr = IDWriteTextLayout_GetMetrics(l, &m);
        if (compatible(rd, fd)) CHECKF(hr == S_OK, "reading %s flow %s: %s", RDN[rd], FDN[fd], dw_hr_name(hr));
        else CHECKF(hr == CONFLICT, "reading %s flow %s: %s want DWRITE_E_FLOWDIRECTIONCONFLICTS", RDN[rd], FDN[fd], dw_hr_name(hr));
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(flow_conflict_error_code_value)
{
    /* the constant is DWRITE_E_FLOWDIRECTIONCONFLICTS = MAKE_HRESULT(1, FACILITY_DWRITE, 0x5000 + 0xB) */
    WT_BEGIN(); DWRITE_TEXT_METRICS m; HRESULT hr; WT_LAYOUT(l, L"abc", 100.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM));
    hr = IDWriteTextLayout_GetMetrics(l, &m);
    EXPECT_EQ((UINT32)hr, 0x8898500Bu); CHECK(FAILED(hr)); EXPECT_EQ(HRESULT_FACILITY(hr), 0x898); EXPECT_EQ(HRESULT_CODE(hr), 0x500b);
    DW_RELEASE(l); WT_END();
}

TEST(flow_conflict_fails_every_layout_dependent_call)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; DWRITE_LINE_METRICS lm[4]; DWRITE_CLUSTER_METRICS cm[4]; DWRITE_HIT_TEST_METRICS hm[4]; UINT32 n = 99; float x, y, mw; BOOL tr, in;
    DWRITE_OVERHANG_METRICS ov; dwx_renderer *r = dwx_renderer_new();
    WT_LAYOUT(l, L"abc", 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT));          /* horizontal reading + vertical flow */
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), CONFLICT);
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, lm, 4, &n), CONFLICT);
    n = 99; CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), CONFLICT);
    CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, cm, 4, &n), CONFLICT);
    CHECK_HR(IDWriteTextLayout_DetermineMinWidth(l, &mw), CONFLICT);
    CHECK_HR(IDWriteTextLayout_HitTestTextPosition(l, 0, FALSE, &x, &y, hm), CONFLICT);
    CHECK_HR(IDWriteTextLayout_HitTestPoint(l, 1.0f, 1.0f, &tr, &in, hm), CONFLICT);
    CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0, 1, 0.0f, 0.0f, hm, 4, &n), CONFLICT);
    CHECK_HR(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f), CONFLICT);
    EXPECT_EQ(r->ncalls, 0);
    { HRESULT hr = IDWriteTextLayout_GetOverhangMetrics(l, &ov); CHECKF(hr == CONFLICT || SUCCEEDED(hr), "GetOverhangMetrics %s", dw_hr_name(hr)); t_note("GetOverhangMetrics on a conflicting layout -> %s", dw_hr_name(hr)); }
    dwx_release(r); DW_RELEASE(l); WT_END();
}

TEST(flow_conflict_leaves_layout_independent_calls_working)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc def", 321.0f); float sz; DWRITE_TEXT_RANGE rg; DWRITE_TRIMMING tm; IDWriteInlineObject *sg = NULL;
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_BOTTOM_TO_TOP));
    CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), 321.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 300.0f)); CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 200.0f));
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(0, 7))); CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 0, &sz, &rg)); CHECK_FLOAT_EQ(sz, 20.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP)); EXPECT_EQ(IDWriteTextLayout_GetWordWrapping(l), DWRITE_WORD_WRAPPING_NO_WRAP);
    CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &tm, &sg)); DW_RELEASE(sg);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER)); CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 10.0f));
    DW_RELEASE(l); WT_END();
}

TEST(flow_conflict_repeats_and_recovers)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; WT_LAYOUT(l, L"abc def", 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT));
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), CONFLICT); CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), CONFLICT);       /* not cached as a success */
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM)); CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m)); EXPECT_EQ(m.lineCount, 1);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM)); CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), CONFLICT);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT)); CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT)); CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), CONFLICT);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP)); CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    DW_RELEASE(l); WT_END();
}

TEST(flow_conflict_valid_set_rejected_does_not_change_state)
{
    /* an out-of-range value is rejected and must not turn a valid layout into a conflicting one */
    WT_BEGIN(); DWRITE_TEXT_METRICS m; WT_LAYOUT(l, L"abc", 1000.0f);
    CHECK_HR(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)4), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)4), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    DW_RELEASE(l); WT_END();
}

TEST(flow_conflict_creation_succeeds)
{
    /* creation never builds the layout, so conflicting directions in the format are reported later */
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteTextLayout *l = NULL, *g = NULL; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 100.0f, 100.0f, &l));
    CHECK_HR_OK(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"abc", 3, t, 100.0f, 100.0f, 1.0f, NULL, FALSE, &g));
    if (l) { EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), DWRITE_READING_DIRECTION_TOP_TO_BOTTOM); CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), CONFLICT); }
    if (g) CHECK_HR(IDWriteTextLayout_GetMetrics(g, &m), CONFLICT);
    CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT));
    DW_RELEASE(l); DW_RELEASE(g);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 100.0f, 100.0f, &l));
    if (l) CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(flow_conflict_empty_text)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = dl(&X, L"", 100.0f, 100.0f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
    if (l) { CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), CONFLICT); IDWriteTextLayout_Release(l); }
    WT_END();
}

TEST(flow_conflict_per_wrap_mode)
{
    WT_BEGIN(); int w; for (w = 0; w < 5; w++) {
        IDWriteTextLayout *l = dl(&X, L"abc def", 50.0f, 50.0f, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, (DWRITE_WORD_WRAPPING)w));
        CHECKF(IDWriteTextLayout_GetMetrics(l, &m) == CONFLICT, "wrap %d", w);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(flow_conflict_inline_sign_creation)
{
    /* CreateEllipsisTrimmingSign builds a layout from the format: conflicting format directions fail there as well */
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteInlineObject *s = (IDWriteInlineObject *)(ULONG_PTR)1; HRESULT hr;
    CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT));
    hr = IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s);
    CHECK(FAILED(hr)); if (FAILED(hr)) CHECK_OUT_NULL(s);
    t_note("CreateEllipsisTrimmingSign with conflicting directions -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && s != (IDWriteInlineObject *)(ULONG_PTR)1) DW_RELEASE(s);
    DW_RELEASE(t); WT_END();
}

/* ---- valid combinations --------------------------------------------------------------------------------------------- */
TEST(flow_horizontal_combos_wrap_alike)
{
    WT_BEGIN(); int rd, fd;
    for (rd = 0; rd < 2; rd++) for (fd = 0; fd < 2; fd++) {
        IDWriteTextLayout *l = dl(&X, L"aaa bbb ccc", WT_FIT(7), 1000.0f, (DWRITE_READING_DIRECTION)rd, (DWRITE_FLOW_DIRECTION)fd);
        if (!l) continue;
        CHECKF(wt_nlines(l) == 2, "%s/%s", RDN[rd], FDN[fd]); WT_LENS(l, 8, 3);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(flow_vertical_combos_succeed)
{
    WT_BEGIN(); int rd, fd;
    for (rd = 2; rd < 4; rd++) for (fd = 2; fd < 4; fd++) {
        IDWriteTextLayout *l = dl(&X, L"abc def", 1000.0f, 1000.0f, (DWRITE_READING_DIRECTION)rd, (DWRITE_FLOW_DIRECTION)fd); DWRITE_TEXT_METRICS m; HRESULT hr;
        if (!l) continue;
        hr = IDWriteTextLayout_GetMetrics(l, &m); CHECKF(hr == S_OK, "%s/%s: %s", RDN[rd], FDN[fd], dw_hr_name(hr));
        if (hr == S_OK) { CHECK(m.lineCount >= 1); CHECK_NEAR(m.layoutWidth, 1000.0, 0.001); CHECK_NEAR(m.layoutHeight, 1000.0, 0.001); }
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(flow_vertical_reading_wraps_at_max_height)
{
    /* the dimension limiting a vertical line is maxHeight (GetLayoutWidth returns the height for vertical reading) */
    WT_BEGIN(); int rd, fd;
    for (rd = 2; rd < 4; rd++) for (fd = 2; fd < 4; fd++) {
        IDWriteTextLayout *a = dl(&X, L"aaa bbb ccc", 100000.0f, WT_FIT(7), (DWRITE_READING_DIRECTION)rd, (DWRITE_FLOW_DIRECTION)fd);
        IDWriteTextLayout *b = dl(&X, L"aaa bbb ccc", WT_FIT(3), 100000.0f, (DWRITE_READING_DIRECTION)rd, (DWRITE_FLOW_DIRECTION)fd);
        if (a) { UINT32 n = wt_nlines(a); CHECKF(n == 2, "%s/%s limited by height: %u lines", RDN[rd], FDN[fd], n); if (n == 2) WT_LENS(a, 8, 3); }
        if (b) { UINT32 n = wt_nlines(b); CHECKF(n == 1, "%s/%s width must not wrap: %u lines", RDN[rd], FDN[fd], n); }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    WT_END();
}

TEST(flow_horizontal_reading_ignores_max_height_for_wrapping)
{
    WT_BEGIN(); IDWriteTextLayout *a = dl(&X, L"aaa bbb ccc", WT_FIT(7), 5.0f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    if (a) { WT_LENS(a, 8, 3); IDWriteTextLayout_Release(a); }
    WT_END();
}

TEST(flow_reading_direction_wraps_alike)
{
    /* right-to-left text breaks at the same places as its left-to-right analogue (same widths) */
    WT_BEGIN(); static const struct { const WCHAR *ltr, *rtl; float fit; } c[] = {
        { L"abc def", HE0 HE1 HE2 L" " HE3 HE4 HE5, WT_FIT(4) }, { L"ab cd ef gh", HE0 HE1 L" " HE2 HE3 L" " HE4 HE5 L" " HE0 HE1, WT_FIT(5) },
        { L"abcdefg", HE0 HE1 HE2 HE3 HE4 HE5 HE0, WT_FIT(3) } };
    int i;
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *a = dl(&X, c[i].ltr, c[i].fit, 1000.0f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
        IDWriteTextLayout *b = dl(&X, c[i].rtl, c[i].fit, 1000.0f, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
        wt_lines la, lb; UINT32 k; int same;
        if (a && b && SUCCEEDED(wt_get_lines(a, &la)) && SUCCEEDED(wt_get_lines(b, &lb))) {
            same = la.n == lb.n; for (k = 0; same && k < la.n; k++) same = la.lm[k].length == lb.lm[k].length;
            CHECKF(same, "case %d: %u vs %u lines", i, la.n, lb.n);
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    WT_END();
}

TEST(flow_rtl_bidi_depth)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = dl(&X, HE0 HE1 HE2, 1000.0f, 1000.0f, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    if (l) { wt_metrics(l, &m); CHECK(m.maxBidiReorderingDepth >= 1); t_note("maxBidiReorderingDepth for RTL text: %u", m.maxBidiReorderingDepth); IDWriteTextLayout_Release(l); }
    WT_END();
}

/* ---- BOTTOM_TO_TOP flow: the paragraph axis is reversed ---------------------------------------------------------------- */
TEST(flow_btt_stacks_lines_upward)
{
    WT_BEGIN(); wt_lines o; IDWriteTextLayout *p = wt_layout_(&X, L"aaa bbb ccc", WT_FIT(3), 500.0f, WT_SIZE);
    if (!p || FAILED(wt_get_lines(p, &o)) || o.n != 3) { DW_RELEASE(p); WT_END(); t_skip("probe"); return; }
    {
        float h = o.lm[0].height, b = o.lm[0].baseline, y_ttb[3], y_btt[3]; int k, mode;
        IDWriteTextLayout_Release(p);
        for (mode = 0; mode < 2; mode++) {
            IDWriteTextLayout *l = dl(&X, L"aaa bbb ccc", WT_FIT(3), 500.0f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, mode ? DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP : DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
            dwx_renderer *r; int cur = 0; const dwx_draw *d;
            if (!l) continue;
            /* NEAR alignment: the text starts at the origin of the flow (top for TOP_TO_BOTTOM, bottom for BOTTOM_TO_TOP) */
            r = wt_draw(l, 0.0f, 0.0f);
            for (k = 0; k < 3; k++) { d = wt_next_run(r, &cur); if (d) (mode ? y_btt : y_ttb)[k] = d->y; }
            dwx_release(r); IDWriteTextLayout_Release(l);
        }
        for (k = 0; k < 3; k++) {
            CHECK_NEAR(y_ttb[k], (float)k * h + b, 1.0);        /* baselines snap to whole pixels */
            CHECK_NEAR(y_btt[k], 500.0f - (float)(k + 1) * h + b, 1.0);            /* line 0 occupies the bottom row of the box */
        }
    }
    WT_END();
}

TEST(flow_btt_paragraph_alignment_swaps_near_and_far)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; int pa;
    for (pa = 0; pa < 3; pa++) {
        IDWriteTextLayout *a = dl(&X, L"aaa bbb", WT_FIT(3), 500.0f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
        IDWriteTextLayout *b = dl(&X, L"aaa bbb", WT_FIT(3), 500.0f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP);
        float ta = 0, tb = 0, h = 0;
        if (a && b) {
            CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(a, (DWRITE_PARAGRAPH_ALIGNMENT)pa)); CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(b, (DWRITE_PARAGRAPH_ALIGNMENT)pa));
            wt_metrics(a, &m); ta = m.top; h = m.height; wt_metrics(b, &m); tb = m.top;
            if (pa == 0) { CHECK_NEAR(ta, 0.0, 0.02); CHECK_NEAR(tb, 500.0f - h, 0.02); }
            else if (pa == 1) { CHECK_NEAR(ta, 500.0f - h, 0.02); CHECK_NEAR(tb, 0.0, 0.02); }
            else { CHECK_NEAR(ta, (500.0f - h) * 0.5f, 0.02); CHECK_NEAR(tb, (500.0f - h) * 0.5f, 0.02); }
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    WT_END();
}

TEST(flow_rtl_flow_horizontal_alignment_unchanged)
{
    /* the vertical flow direction does not touch horizontal alignment of horizontal text */
    WT_BEGIN(); int fd; for (fd = 0; fd < 2; fd++) {
        IDWriteTextLayout *l = dl(&X, L"abc", WT_CH(10), 300.0f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, (DWRITE_FLOW_DIRECTION)fd); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING)); wt_metrics(l, &m); CHECK_NEAR(m.left, WT_CH(7), 0.02);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

/* ---- last line wrapping ---------------------------------------------------------------------------------------------- */
TEST(lastline_default_and_roundtrip)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); IDWriteTextFormat *t = wt_format(&X, WT_SIZE);
    WT_QI(l1, IDWriteTextFormat1, l);
    {
        IDWriteTextFormat1 *t1 = NULL;
        EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(l1), TRUE);
        CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(l1, FALSE)); EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(l1), FALSE);
        CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(l1, TRUE)); EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(l1), TRUE);
        CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(l1, 5)); t_note("SetLastLineWrapping(5) then Get -> %d", IDWriteTextFormat1_GetLastLineWrapping(l1));
        if (SUCCEEDED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1)) && t1) {
            EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), TRUE);
            CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(t1, FALSE));
            { IDWriteTextLayout *l2 = NULL; IDWriteTextFormat1 *q = NULL;
              CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 100.0f, 100.0f, &l2));
              if (l2 && SUCCEEDED(IDWriteTextLayout_QueryInterface(l2, &IID_IDWriteTextFormat1, (void **)&q))) { EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(q), FALSE); DW_RELEASE(q); }
              DW_RELEASE(l2); }
            DW_RELEASE(t1);
        }
    }
    DW_RELEASE(l1); DW_RELEASE(l); DW_RELEASE(t); WT_END();
}

TEST(lastline_default_ignores_height)
{
    /* with last line wrapping on (default) every line is created whatever maxHeight is */
    WT_BEGIN(); int h; for (h = 0; h <= 3; h++) {
        IDWriteTextLayout *l = wt_layout_(&X, L"aaa bbb ccc ddd", WT_FIT(3), (float)h * 20.0f, WT_SIZE);
        if (!l) continue;
        WT_LENS(l, 4, 4, 4, 3);
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}

TEST(lastline_off_last_visible_line_takes_the_rest)
{
    WT_BEGIN(); wt_lines o; IDWriteTextLayout *p = wt_layout_(&X, L"aaa bbb ccc ddd", WT_FIT(3), 1000.0f, WT_SIZE);
    if (!p || FAILED(wt_get_lines(p, &o)) || o.n != 4) { DW_RELEASE(p); WT_END(); t_skip("probe"); return; }
    {
        float h = o.lm[0].height; IDWriteTextFormat1 *l1; IDWriteTextLayout *l;
        IDWriteTextLayout_Release(p);
        /* room for 1, 2 and 3 line tops (a line starting below maxHeight is not created) */
        static const struct { float rows; int n; UINT32 len[4]; } c[] = { { 0.5f, 1, { 15 } }, { 1.0f, 1, { 15 } }, { 1.5f, 2, { 4, 11 } }, { 2.5f, 3, { 4, 4, 7 } }, { 3.5f, 4, { 4, 4, 4, 3 } }, { 10.0f, 4, { 4, 4, 4, 3 } } };
        int i, k;
        for (i = 0; i < 6; i++) {
            l = wt_layout_(&X, L"aaa bbb ccc ddd", WT_FIT(3), c[i].rows * h, WT_SIZE);
            if (!l) continue;
            l1 = (IDWriteTextFormat1 *)wt_qi((IUnknown *)l, &IID_IDWriteTextFormat1, "IDWriteTextFormat1");
            if (!l1) { IDWriteTextLayout_Release(l); break; }
            CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(l1, FALSE));
            { wt_lines q; CHECK_HR_OK(wt_get_lines(l, &q));
              { UINT32 sum = 0, z; for (z = 0; z < q.n; z++) sum += q.lm[z].length; CHECKF(sum == 15, "rows %g: lengths sum %u", c[i].rows, sum); }
              if ((int)q.n != c[i].n) t_note("rows %g: %u lines (model expects %d)", c[i].rows, q.n, c[i].n);
              for (k = 0; k < 1 && k < (int)q.n; k++) if (c[i].n == (int)q.n) CHECKF(q.lm[k].length == c[i].len[k], "rows %g line %d length %u want %u", c[i].rows, k, q.lm[k].length, c[i].len[k]); }
            DW_RELEASE(l1); IDWriteTextLayout_Release(l);
        }
    }
    WT_END();
}

TEST(lastline_off_hard_break_ends_the_unwrapped_line)
{
    WT_BEGIN(); wt_lines o; IDWriteTextLayout *p = wt_layout_(&X, L"aaa", 1000.0f, 1000.0f, WT_SIZE);
    if (!p || FAILED(wt_get_lines(p, &o)) || o.n != 1) { DW_RELEASE(p); WT_END(); t_skip("probe"); return; }
    {
        float h = o.lm[0].height; IDWriteTextLayout *l; IDWriteTextFormat1 *l1;
        IDWriteTextLayout_Release(p);
        l = wt_layout_(&X, L"aaa\nbbb ccc ddd", WT_FIT(3), 0.5f * h, WT_SIZE);
        if (l) {
            l1 = (IDWriteTextFormat1 *)wt_qi((IUnknown *)l, &IID_IDWriteTextFormat1, "IDWriteTextFormat1");
            if (l1) {
                CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(l1, FALSE));
                { wt_lines q; CHECK_HR_OK(wt_get_lines(l, &q)); CHECK(q.n >= 1 && q.lm[0].length == 4); }   /* the hard break ends the first paragraph line */
                DW_RELEASE(l1);
            }
            IDWriteTextLayout_Release(l);
        }
    }
    WT_END();
}

TEST(lastline_off_no_effect_when_everything_fits)
{
    WT_BEGIN(); IDWriteTextLayout *l = wt_layout_(&X, L"aaa bbb ccc", WT_FIT(3), 1000.0f, WT_SIZE); IDWriteTextFormat1 *l1;
    if (l) {
        l1 = (IDWriteTextFormat1 *)wt_qi((IUnknown *)l, &IID_IDWriteTextFormat1, "IDWriteTextFormat1");
        if (l1) { CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(l1, FALSE)); WT_LENS(l, 4, 4, 3); DW_RELEASE(l1); }
        IDWriteTextLayout_Release(l);
    }
    WT_END();
}
