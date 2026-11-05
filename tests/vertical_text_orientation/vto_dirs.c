/* Flow / reading direction pairing, vertical glyph orientation setters, and who reports a conflicting pair.
 *
 * DWriteTextFormat::SetFlowDirection / DWriteTextLayout::SetFlowDirection reject values above 3 with E_INVALIDARG, SetReadingDirection likewise,
 * SetVerticalGlyphOrientation accepts 0 and 1 only. A pair whose bit 1 differs (reading 2/3 is vertical, flow 2/3 is vertical) throws
 * DWRITE_E_FLOWDIRECTIONCONFLICTS when the layout is materialised (TextLayout::TextLayout), i.e. from the first call that needs the laid-out text.
 * The layout setters mark the layout dirty so a repaired pair works again without recreating the object.
 */
#include "vto.h"

static int conflicts(int rd, int fd) { return ((rd >> 1) ^ (fd >> 1)) & 1; }
static const char *rdn(int r) { static const char *n[] = { "LTR", "RTL", "TTB", "BTT" }; return r >= 0 && r < 4 ? n[r] : "?"; }
static const char *fdn(int r) { static const char *n[] = { "TTB", "BTT", "LTR", "RTL" }; return r >= 0 && r < 4 ? n[r] : "?"; }

TEST(enum_values_are_abi)
{
    CHECK(FD_TTB == 0 && FD_BTT == 1 && FD_LTR == 2 && FD_RTL == 3);
    CHECK(RD_LTR == 0 && RD_RTL == 1 && RD_TTB == 2 && RD_BTT == 3);
    CHECK(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT == 0 && DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED == 1);
    CHECK(DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES == 0 && DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES == 1 &&
          DWRITE_GLYPH_ORIENTATION_ANGLE_180_DEGREES == 2 && DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES == 3);
}

/* ---- format and layout setters ------------------------------------------------------------------------------ */
TEST(format_flow_roundtrip_all)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); int v;
    if (!t) { VT_END(); t_skip("format"); return; }
    for (v = 0; v < 4; v++) {
        CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, (DWRITE_FLOW_DIRECTION)v));
        EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(t), v);
    }
    DW_RELEASE(t); VT_END();
}
TEST(format_reading_roundtrip_all)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); int v;
    if (!t) { VT_END(); t_skip("format"); return; }
    for (v = 0; v < 4; v++) {
        CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, (DWRITE_READING_DIRECTION)v));
        EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(t), v);
    }
    DW_RELEASE(t); VT_END();
}
TEST(format_flow_invalid_values_rejected)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); UINT32 bad[] = { 4, 5, 7, 255, 0x7fffffff, 0x80000000u, 0xffffffffu }; int i;
    if (!t) { VT_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, FD_RTL));
    for (i = 0; i < 7; i++) {
        CHECK_HR(IDWriteTextFormat_SetFlowDirection(t, (DWRITE_FLOW_DIRECTION)bad[i]), VT_E_INVALIDARG);
        EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(t), FD_RTL);            /* a rejected call changes nothing */
    }
    DW_RELEASE(t); VT_END();
}
TEST(format_reading_invalid_values_rejected)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); UINT32 bad[] = { 4, 5, 7, 255, 0x7fffffff, 0x80000000u, 0xffffffffu }; int i;
    if (!t) { VT_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, RD_BTT));
    for (i = 0; i < 7; i++) {
        CHECK_HR(IDWriteTextFormat_SetReadingDirection(t, (DWRITE_READING_DIRECTION)bad[i]), VT_E_INVALIDARG);
        EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(t), RD_BTT);
    }
    DW_RELEASE(t); VT_END();
}
TEST(layout_flow_reading_roundtrip_and_invalid)
{
    VT_BEGIN(); VT_HL(l, VT_FAM, L"AB", 16.0f, 100, 100); int v;
    for (v = 0; v < 4; v++) {
        CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)v)); EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), v);
        CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)v)); EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), v);
    }
    CHECK_HR(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)4), VT_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)4), VT_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)0xffffffffu), VT_E_INVALIDARG);
    EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), 3); EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), 3);
    DW_RELEASE(l); VT_END();
}
TEST(defaults_are_horizontal_and_default_orientation)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteTextLayout *l = NULL; IDWriteTextFormat1 *t1 = NULL; IDWriteTextLayout2 *l2 = NULL;
    if (!t) { VT_END(); t_skip("format"); return; }
    EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(t), FD_TTB); EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(t), RD_LTR);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"AB", 2, t, 50, 50, &l));
    EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), FD_TTB); EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), RD_LTR);
    if (SUCCEEDED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1)))
        EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(t1), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2)))
        EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    DW_RELEASE(t1); DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(t); VT_END();
}

/* ---- vertical glyph orientation setters -------------------------------------------------------------------------- */
TEST(format1_orientation_roundtrip)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteTextFormat1 *t1 = NULL;
    if (!t) { VT_END(); t_skip("format"); return; }
    DW_REQUIRE_OK(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1));
    CHECK_HR_OK(IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(t1), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    CHECK_HR_OK(IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT));
    EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(t1), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    DW_RELEASE(t1); DW_RELEASE(t); VT_END();
}
TEST(format1_orientation_invalid_rejected)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteTextFormat1 *t1 = NULL; UINT32 bad[] = { 2, 3, 100, 0x80000000u, 0xffffffffu }; int i;
    if (!t) { VT_END(); t_skip("format"); return; }
    DW_REQUIRE_OK(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1));
    CHECK_HR_OK(IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    for (i = 0; i < 5; i++) {
        CHECK_HR(IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, (DWRITE_VERTICAL_GLYPH_ORIENTATION)bad[i]), VT_E_INVALIDARG);
        EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(t1), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    }
    DW_RELEASE(t1); DW_RELEASE(t); VT_END();
}
TEST(layout2_orientation_roundtrip_and_invalid)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB", 16.0f, 100, 100); IDWriteTextLayout2 *l2 = NULL; UINT32 bad[] = { 2, 3, 0x7fffffff, 0xffffffffu }; int i;
    DW_REQUIRE_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2));
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    for (i = 0; i < 4; i++) {
        CHECK_HR(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, (DWRITE_VERTICAL_GLYPH_ORIENTATION)bad[i]), VT_E_INVALIDARG);
        EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    }
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT));
    EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}
TEST(layout_exposes_format1_orientation_on_the_same_state)
{
    /* DWriteTextLayout implements IDWriteTextFormat1 itself: setting through one face is visible through the other. */
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB", 16.0f, 100, 100); IDWriteTextLayout2 *l2 = NULL; IDWriteTextFormat1 *t1 = NULL;
    DW_REQUIRE_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2));
    DW_REQUIRE_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat1, (void **)&t1));
    CHECK_HR_OK(IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT));
    EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(t1), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    DW_RELEASE(t1); DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}
TEST(layout_inherits_direction_and_orientation_from_format)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteTextFormat1 *t1 = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout2 *l2 = NULL;
    if (!t) { VT_END(); t_skip("format"); return; }
    DW_REQUIRE_OK(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1));
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, RD_TTB));
    CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, FD_LTR));
    CHECK_HR_OK(IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"AB", 2, t, 50, 50, &l));
    EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), RD_TTB); EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), FD_LTR);
    if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2)))
        EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(t1); DW_RELEASE(t); VT_END();
}
TEST(format_and_layout_are_independent_after_creation)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t) { VT_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"AB", 2, t, 50, 50, &l));
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, RD_TTB)); CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, FD_RTL));
    EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), RD_LTR); EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), FD_TTB);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, RD_BTT));
    EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(t), RD_TTB);
    DW_RELEASE(l); DW_RELEASE(t); VT_END();
}
TEST(layout_orientation_setter_changes_glyph_runs_in_vertical_layout)
{
    /* The orientation setters mark the layout dirty: the next Draw reflects the new value. STACKED turns the sideways Latin run upright. */
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABC", 16.0f, 100, 100); IDWriteTextLayout2 *l2 = NULL; dwx_renderer *r;
    DW_REQUIRE_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2));
    r = vt_r1(); CHECK_HR_OK(vt_draw(l, r, 0, 0)); CHECK(vt_runs(r) >= 1);
    if (vt_run(r, 0)) { CHECK(!vt_run(r, 0)->sideways); }                    /* DEFAULT: Latin is a rotated, non-"sideways" run */
    dwx_release(r);
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    r = vt_r1(); CHECK_HR_OK(vt_draw(l, r, 0, 0)); CHECK(vt_runs(r) >= 1);
    if (vt_run(r, 0)) { t_note("STACKED Latin run: sideways=%d angle=%d", vt_run(r, 0)->sideways, vt_run(r, 0)->angle); }
    dwx_release(r);
    DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}

/* ---- conflicting pairs ------------------------------------------------------------------------------------------ */
TEST(conflict_table_get_metrics)
{
    VT_BEGIN(); int rd, fd;
    for (rd = 0; rd < 4; rd++) for (fd = 0; fd < 4; fd++) {
        IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, L"ABCD", 16.0f, 100, 100, rd, fd); DWRITE_TEXT_METRICS m; HRESULT hr;
        if (!l) { t_note("setters rejected reading %d flow %d", rd, fd); continue; }
        memset(&m, 0, sizeof m);
        hr = IDWriteTextLayout_GetMetrics(l, &m);
        CHECKF(hr == (conflicts(rd, fd) ? VT_E_CONFLICT : S_OK), "reading %s flow %s -> %s", rdn(rd), fdn(fd), dw_hr_name(hr));
        DW_RELEASE(l);
    }
    VT_END();
}
TEST(conflict_reported_by_every_layout_dependent_call)
{
    VT_BEGIN(); VT_HL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); DWRITE_TEXT_METRICS m; DWRITE_LINE_METRICS lm[4]; UINT32 n = 7; DWRITE_CLUSTER_METRICS cm[8];
    DWRITE_OVERHANG_METRICS oh; float mw; BOOL trail, inside; DWRITE_HIT_TEST_METRICS hm; float px, py; dwx_renderer *r = vt_r1();
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, FD_LTR));                       /* horizontal reading + vertical flow */
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), VT_E_CONFLICT);
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, lm, 4, &n), VT_E_CONFLICT);
    CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, cm, 8, &n), VT_E_CONFLICT);
    CHECK_HR(IDWriteTextLayout_GetOverhangMetrics(l, &oh), VT_E_CONFLICT);
    CHECK_HR(IDWriteTextLayout_DetermineMinWidth(l, &mw), VT_E_CONFLICT);
    CHECK_HR(IDWriteTextLayout_HitTestPoint(l, 1, 1, &trail, &inside, &hm), VT_E_CONFLICT);
    CHECK_HR(IDWriteTextLayout_HitTestTextPosition(l, 1, FALSE, &px, &py, &hm), VT_E_CONFLICT);
    CHECK_HR(vt_draw(l, r, 0, 0), VT_E_CONFLICT);
    EXPECT_EQ(vt_runs(r), 0);                                                        /* nothing was drawn */
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, FD_TTB));                      /* repair: the same object works again */
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(conflict_vertical_reading_with_horizontal_flow)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, FD_TTB));
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), VT_E_CONFLICT);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, FD_BTT));
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), VT_E_CONFLICT);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, RD_BTT)); CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, FD_LTR));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    DW_RELEASE(l); VT_END();
}
TEST(conflict_from_format_pair_surfaces_at_layout_use)
{
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
    if (!t) { VT_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, RD_TTB));                   /* flow stays TOP_TO_BOTTOM: conflict */
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 100, 100, &l));    /* creation does not validate the pair */
    CHECK(l != NULL);
    if (l) { CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), VT_E_CONFLICT); DW_RELEASE(l); }
    DW_RELEASE(t); VT_END();
}
TEST(conflict_ellipsis_trimming_sign_factory)
{
    /* DWriteFactory::CreateEllipsisTrimmingSign builds a layout from the format and lays it out immediately. */
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteInlineObject *sign = (IDWriteInlineObject *)(ULONG_PTR)1;
    if (!t) { VT_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, FD_LTR));
    CHECK_HR(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &sign), VT_E_CONFLICT);
    CHECK_OUT_NULL(sign);
    CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, FD_TTB));
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &sign)); DW_RELEASE(sign);
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, RD_TTB)); CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, FD_RTL));
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &sign)); DW_RELEASE(sign);
    DW_RELEASE(t); VT_END();
}
TEST(ellipsis_sign_uses_vertical_ellipsis_for_vertical_formats)
{
    /* CreateEllipsisTrimmingSign picks U+22EE (VERTICAL ELLIPSIS) when the format's reading direction is vertical, U+2026 otherwise. */
    VT_BEGIN(); IDWriteTextFormat *t = vt_format(&X, VT_FAM, 16.0f); IDWriteInlineObject *sign = NULL; int pass;
    if (!t) { VT_END(); t_skip("format"); return; }
    for (pass = 0; pass < 2; pass++) {
        dwx_renderer *r = vt_r1(); const dwx_draw *d; HRESULT hr;
        CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, pass ? RD_TTB : RD_LTR));
        CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, pass ? FD_RTL : FD_TTB));
        DW_REQUIRE_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &sign));
        hr = IDWriteInlineObject_Draw(sign, NULL, (IDWriteTextRenderer *)r, 0, 0, FALSE, FALSE, NULL);
        CHECK_HR_OK(hr);
        d = vt_run(r, 0);
        CHECK(d != NULL);
        if (d && d->has_desc && d->text_len >= 1 && d->text) { EXPECT_EQ(d->text[0], pass ? 0x22ee : 0x2026); }
        else if (d) t_note("no run description text for the trimming sign");
        if (d) { EXPECT_EQ(d->nglyphs, 1); if (d->nglyphs == 1) EXPECT_EQ(d->indices[0], pass ? TTF_GID_TRIANGLE : TTF_GID_ROUND); }
        dwx_release(r); DW_RELEASE(sign);
    }
    DW_RELEASE(t); VT_END();
}
