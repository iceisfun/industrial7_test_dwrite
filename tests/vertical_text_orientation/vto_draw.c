/* IDWriteTextLayout::Draw for vertical layouts: renderer protocol (IDWriteTextRenderer1), isSideways / angle of the glyph runs,
 * baseline origins, decorations and inline objects.
 *
 * Draw asks the renderer for IDWriteTextRenderer1; a vertical layout without it throws DWRITE_E_TEXTRENDERERINCOMPATIBLE before any
 * other renderer callback. With IDWriteTextRenderer1 every Draw* callback goes to the *1 entry point (also for horizontal layouts) and carries the
 * glyph orientation angle that the layout stored for the span (SetGlyphOrientation).
 * Per span the layout keeps (angle, isSideways) from IDWriteTextAnalysisSink1::SetGlyphOrientation: the DEFAULT orientation reports
 * (90 degrees, isSideways FALSE) for rotated scripts (Latin: horizontal metrics, run rotated with the line) and (0 degrees, isSideways TRUE) for upright
 * scripts (Han: vertical metrics); a span with isSideways TRUE sits on the central baseline of the line, the others on the roman baseline
 * (RecentMetricsCache::GetBaselineDelta). Baseline origins: world.x = maxWidth - abstract.y + originX, world.y = abstract.x + originY.
 */
#include "vto.h"

#define A16 7.8125f
#define V16 11.71875f
#define RUN(r, i) vt_run((r), (i))

static dwx_renderer *draw_ok(IDWriteTextLayout *l, float ox, float oy)
{
    dwx_renderer *r = vt_r1();
    CHECK_HR_OK(vt_draw(l, r, ox, oy));
    return r;
}

/* Win10 reports a zero-glyph run for each hard line break; collect only the runs that carry glyphs. */
static int ink_runs(dwx_renderer *r, const dwx_draw **o, int max)
{
    int i, n = 0; for (i = 0; i < vt_runs(r) && n < max; i++) if (RUN(r, i)->nglyphs > 0) o[n++] = RUN(r, i);
    return n;
}

/* ---- renderer protocol ---------------------------------------------------------------------------------------- */
TEST(plain_renderer_is_incompatible_with_vertical_layout)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); dwx_renderer *r = vt_r0();
    CHECK_HR(vt_draw(l, r, 0, 0), VT_E_INCOMPATIBLE);
    EXPECT_EQ(r->ncalls, 0); EXPECT_EQ(r->n_snapping, 0);               /* rejected before the pixel snapping callbacks */
    EXPECT_REFS(l, 1);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(plain_renderer_is_incompatible_for_every_vertical_pair)
{
    VT_BEGIN(); int rd, fd;
    for (rd = 2; rd < 4; rd++) for (fd = 2; fd < 4; fd++) {
        IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, L"AB", 16.0f, 100, 100, rd, fd); dwx_renderer *r = vt_r0();
        if (!l) { dwx_release(r); continue; }
        CHECKF(vt_draw(l, r, 0, 0) == VT_E_INCOMPATIBLE, "reading %d flow %d", rd, fd);
        EXPECT_EQ(r->ncalls, 0);
        dwx_release(r); DW_RELEASE(l);
    }
    VT_END();
}
TEST(plain_renderer_still_draws_horizontal_layout)
{
    VT_BEGIN(); VT_HL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); dwx_renderer *r = vt_r0();
    CHECK_HR_OK(vt_draw(l, r, 0, 0)); EXPECT_EQ(vt_runs(r), 1); EXPECT_EQ(r->r1_calls, 0);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(incompatible_draw_leaves_layout_usable)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); dwx_renderer *r0 = vt_r0(), *r1 = vt_r1();
    CHECK_HR(vt_draw(l, r0, 0, 0), VT_E_INCOMPATIBLE);
    CHECK_HR_OK(vt_draw(l, r1, 0, 0)); EXPECT_EQ(vt_runs(r1), 1);
    dwx_release(r0); dwx_release(r1); DW_RELEASE(l); VT_END();
}
TEST(renderer1_receives_star1_callbacks)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); dwx_renderer *r = draw_ok(l, 0, 0);
    EXPECT_EQ(vt_runs(r), 1); EXPECT_EQ(r->r1_calls, 1);
    CHECK(RUN(r, 0) && RUN(r, 0)->has_orientation);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(renderer1_also_used_for_horizontal_layouts_with_angle_zero)
{
    VT_BEGIN(); VT_HL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); dwx_renderer *r = draw_ok(l, 0, 0);
    EXPECT_EQ(vt_runs(r), 1); EXPECT_EQ(r->r1_calls, 1);
    if (RUN(r, 0)) { CHECK(RUN(r, 0)->has_orientation); EXPECT_EQ(RUN(r, 0)->angle, DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES); CHECK(!RUN(r, 0)->sideways); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(snapping_callbacks_made_once_for_vertical_draw)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); dwx_renderer *r = dwx_renderer_new();
    r->support_r1 = 1;                                                    /* snapping enabled: transform and ppd are queried */
    CHECK_HR_OK(vt_draw(l, r, 0, 0));
    EXPECT_EQ(r->n_snapping, 1); EXPECT_EQ(r->n_transform, 1); EXPECT_EQ(r->n_ppd, 1);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(empty_vertical_layout_draws_nothing)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"", 16.0f, 100, 100); dwx_renderer *r = draw_ok(l, 0, 0);
    EXPECT_EQ(vt_runs(r), 0);
    dwx_release(r); DW_RELEASE(l); VT_END();
}

/* ---- isSideways / angle per script --------------------------------------------------------------------------- */
TEST(latin_run_is_rotated_not_sideways)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 100); dwx_renderer *r = draw_ok(l, 0, 0); const dwx_draw *d = RUN(r, 0);
    CHECK(d != NULL);
    if (d) { t_note("Latin run: angle %d sideways %d", d->angle, d->sideways); EXPECT_EQ(d->nglyphs, 4); EXPECT_EQ(d->bidi, 0); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(han_run_is_sideways_upright)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, HAN4, 16.0f, 100, 100); dwx_renderer *r = draw_ok(l, 0, 0); const dwx_draw *d = RUN(r, 0);
    CHECK(d != NULL);
    if (d) { t_note("Han run: angle %d sideways %d", d->angle, d->sideways); EXPECT_EQ(d->nglyphs, 4); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(rotated_and_upright_runs_have_the_same_transform_index)
{
    /* angle + isSideways is the same for both kinds of run: the matrix GetGlyphOrientationTransform returns is the 90 degree rotation. */
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00\x4e01", 16.0f, 100, 100); dwx_renderer *r = draw_ok(l, 0, 0); int i;
    EXPECT_EQ(vt_runs(r), 2);
    for (i = 0; i < vt_runs(r); i++) { const dwx_draw *d = RUN(r, i); if (d) EXPECT_EQ(((int)d->angle + (d->sideways ? 1 : 0)) & 3, 1); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(mixed_latin_han_latin_runs)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00\x4e01" L"CD", 16.0f, 100, 200); dwx_renderer *r = draw_ok(l, 0, 0);
    EXPECT_EQ(vt_runs(r), 3);
    if (vt_runs(r) == 3) {
        EXPECT_EQ(RUN(r, 0)->text_pos, 0); EXPECT_EQ(RUN(r, 1)->text_pos, 2); EXPECT_EQ(RUN(r, 2)->text_pos, 4);
        EXPECT_EQ(RUN(r, 0)->nglyphs, 2); EXPECT_EQ(RUN(r, 1)->nglyphs, 2); EXPECT_EQ(RUN(r, 2)->nglyphs, 2);
        CHECK(RUN(r, 0)->sideways == RUN(r, 2)->sideways); CHECK(RUN(r, 0)->sideways != RUN(r, 1)->sideways || RUN(r, 0)->angle != RUN(r, 1)->angle);
    }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(run_description_covers_the_run_text)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00\x4e01" L"CD", 16.0f, 100, 200); dwx_renderer *r = draw_ok(l, 0, 0);
    if (vt_runs(r) == 3) {
        EXPECT_EQ(RUN(r, 0)->text_len, 2); EXPECT_EQ(RUN(r, 1)->text_len, 2);
        CHECK(RUN(r, 1)->has_desc && RUN(r, 1)->text && RUN(r, 1)->text[0] == 0x4e00 && RUN(r, 1)->text[1] == 0x4e01);
        CHECK(RUN(r, 2)->has_desc && RUN(r, 2)->text && RUN(r, 2)->text[0] == 'C');
    } else CHECK(vt_runs(r) == 3);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(glyph_indices_follow_the_cmap_in_vertical_runs)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"A\x4e00", 16.0f, 100, 200); dwx_renderer *r = draw_ok(l, 0, 0);
    EXPECT_EQ(vt_runs(r), 2);
    if (vt_runs(r) == 2 && RUN(r, 0)->nglyphs == 1 && RUN(r, 1)->nglyphs == 1) { EXPECT_EQ(RUN(r, 0)->indices[0], 2); EXPECT_EQ(RUN(r, 1)->indices[0], 2); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(run_advances_are_horizontal_for_rotated_and_vertical_for_upright)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00\x4e01", 16.0f, 100, 200); dwx_renderer *r = draw_ok(l, 0, 0);
    EXPECT_EQ(vt_runs(r), 2);
    if (vt_runs(r) == 2) {
        const dwx_draw *a = RUN(r, 0), *b = RUN(r, 1); UINT32 i;
        for (i = 0; i < a->nglyphs; i++) CHECK_NEARF(a->advances[i], A16);
        for (i = 0; i < b->nglyphs; i++) CHECK_NEARF(b->advances[i], V16);
        CHECK_NEARF(dwx_run_width(a), 2 * A16); CHECK_NEARF(dwx_run_width(b), 2 * V16);
    }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(plain_font_han_run_still_flagged_sideways)
{
    VT_BEGIN(); VT_VL(l, VT_PLAIN, HAN4, 16.0f, 100, 200); dwx_renderer *r = draw_ok(l, 0, 0); const dwx_draw *d = RUN(r, 0);
    CHECK(d != NULL);
    if (d) { CHECK(d->sideways); t_note("DwPlain Han advances: first %g (no vmtx)", d->nglyphs ? d->advances[0] : -1.0f); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(stacked_orientation_flags_noted)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00", 16.0f, 100, 200); IDWriteTextLayout2 *l2 = NULL; dwx_renderer *r; int i;
    DW_REQUIRE_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2));
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    r = draw_ok(l, 0, 0);
    for (i = 0; i < vt_runs(r); i++) { const dwx_draw *d = RUN(r, i); t_note("STACKED run %d: pos %u sideways %d angle %d advance0 %g", i, d->text_pos, d->sideways, d->angle, d->nglyphs ? d->advances[0] : -1.0f); CHECK(d->angle != 2); }
    dwx_release(r); DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}
TEST(stacked_then_default_restores_flags)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 200); IDWriteTextLayout2 *l2 = NULL; dwx_renderer *r;
    DW_REQUIRE_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2));
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    r = draw_ok(l, 0, 0); dwx_release(r);
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT));
    r = draw_ok(l, 0, 0);
    if (RUN(r, 0)) t_note("DEFAULT Latin: angle %d sideways %d", RUN(r, 0)->angle, RUN(r, 0)->sideways);
    dwx_release(r); DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}
TEST(orientation_set_on_a_horizontal_layout_does_not_change_runs)
{
    VT_BEGIN(); VT_HL(l, VT_FAM, L"AB\x4e00", 16.0f, 200, 100); IDWriteTextLayout2 *l2 = NULL; dwx_renderer *a, *b;
    DW_REQUIRE_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2));
    a = draw_ok(l, 0, 0);
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    b = draw_ok(l, 0, 0);
    EXPECT_EQ(vt_runs(a), vt_runs(b));
    if (vt_runs(a) == vt_runs(b) && vt_runs(a) >= 1) { CHECK(RUN(a, 0)->sideways == RUN(b, 0)->sideways); EXPECT_EQ(RUN(a, 0)->angle, RUN(b, 0)->angle); CHECK_NEARF(RUN(a, 0)->x, RUN(b, 0)->x); }
    dwx_release(a); dwx_release(b); DW_RELEASE(l2); DW_RELEASE(l); VT_END();
}
TEST(vert_feature_font_run_indices_noted)
{
    /* A font with a GSUB 'vert' single substitution: upright runs may be shaped with the vertical variants. */
    VT_BEGIN(); VT_VL(l, VT_SUB, L"\x4e00\x4e01\x4e02", 16.0f, 100, 200); dwx_renderer *r = draw_ok(l, 0, 0); const dwx_draw *d = RUN(r, 0);
    CHECK(d != NULL);
    if (d && d->nglyphs == 3) {
        t_note("DwVertSub upright run glyphs: %u %u %u (cmap 2 3 4; vert maps 2->64, 3->65)", d->indices[0], d->indices[1], d->indices[2]);
        CHECK(d->indices[2] == 4);
        CHECK(d->indices[0] == 2 || d->indices[0] == 64); CHECK(d->indices[1] == 3 || d->indices[1] == 65);
    }
    dwx_release(r); DW_RELEASE(l); VT_END();
}

/* ---- origins ------------------------------------------------------------------------------------------------- */
TEST(first_run_starts_at_top_edge_for_ttb_reading)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 200); dwx_renderer *r = draw_ok(l, 0, 0); const dwx_draw *d = RUN(r, 0);
    CHECK(d != NULL);
    if (d) CHECK_NEARF(d->y, 0.0f);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(origin_offsets_translate_both_axes)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00\x4e01" L"CD", 16.0f, 100, 200); dwx_renderer *a = draw_ok(l, 0, 0), *b = draw_ok(l, 37.5f, -12.25f); int i;
    EXPECT_EQ(vt_runs(a), vt_runs(b));
    for (i = 0; i < vt_runs(a) && i < vt_runs(b); i++) { CHECK_NEARF(RUN(b, i)->x - RUN(a, i)->x, 37.5f); CHECK_NEARF(RUN(b, i)->y - RUN(a, i)->y, -12.25f); }
    dwx_release(a); dwx_release(b); DW_RELEASE(l); VT_END();
}
TEST(runs_advance_along_y)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00\x4e01" L"CD", 16.0f, 100, 200); dwx_renderer *r = draw_ok(l, 0, 5.0f);
    EXPECT_EQ(vt_runs(r), 3);
    if (vt_runs(r) == 3) {
        CHECK_NEARF(RUN(r, 0)->y, 5.0f); CHECK_NEARF(RUN(r, 1)->y, 5.0f + 2 * A16); CHECK_NEARF(RUN(r, 2)->y, 5.0f + 2 * A16 + 2 * V16);
    }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(baseline_x_lies_inside_the_line_box)
{
    VT_BEGIN(); float W = 200; VT_VL(l, VT_FAM, L"ABCD", 16.0f, W, 100); dwx_renderer *r = draw_ok(l, 10.0f, 0); const dwx_draw *d = RUN(r, 0); vt_lines ls;
    vt_get_lines(l, &ls);
    CHECK(d != NULL && ls.n == 1);
    if (d && ls.n == 1) { CHECK(d->x >= 10.0f + W - ls.lm[0].height - 0.001); CHECK(d->x <= 10.0f + W + 0.001); t_note("baseline x %g (right edge %g, line height %g, line baseline %g)", d->x - 10.0f, W, ls.lm[0].height, ls.lm[0].baseline); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(flow_direction_mirrors_the_line_across_the_box)
{
    /* RTL flow puts line 0 against the right edge (box [W-h, W]); LTR flow against the left edge (box [0, h]): the baselines differ by W - h. */
    VT_BEGIN(); float W = 200; VT_VL(a, VT_FAM, L"ABCD", 16.0f, W, 100);
    IDWriteTextLayout *b = vt_layout_(&X, VT_FAM, L"ABCD", 16.0f, W, 100, RD_TTB, FD_LTR); dwx_renderer *ra, *rb; vt_lines ls;
    if (!b) { DW_RELEASE(a); VT_END(); t_skip("layout"); return; }
    ra = draw_ok(a, 0, 0); rb = draw_ok(b, 0, 0); vt_get_lines(a, &ls);
    if (RUN(ra, 0) && RUN(rb, 0) && ls.n == 1) CHECK_NEARF(RUN(ra, 0)->x - RUN(rb, 0)->x, W - ls.lm[0].height);
    else CHECK(0);
    dwx_release(ra); dwx_release(rb); DW_RELEASE(a); DW_RELEASE(b); VT_END();
}
TEST(uniform_spacing_pitch_is_line_height_rtl_flow)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\nCD\nEF", 16.0f, 300, 100); dwx_renderer *r; const dwx_draw *g[3]; int n;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    r = draw_ok(l, 0, 0); n = ink_runs(r, g, 3); EXPECT_EQ(n, 3);
    if (n == 3) { CHECK_NEARF(g[1]->x - g[0]->x, -30.0f); CHECK_NEARF(g[2]->x - g[1]->x, -30.0f); CHECK_NEARF(g[1]->y, g[0]->y); CHECK_NEARF(g[2]->y, g[0]->y); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(uniform_spacing_pitch_is_line_height_ltr_flow)
{
    VT_BEGIN(); IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, L"AB\nCD\nEF", 16.0f, 300, 100, RD_TTB, FD_LTR); dwx_renderer *r; const dwx_draw *g[3]; int n;
    if (!l) { VT_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    r = draw_ok(l, 0, 0); n = ink_runs(r, g, 3); EXPECT_EQ(n, 3);
    if (n == 3) { CHECK_NEARF(g[1]->x - g[0]->x, 30.0f); CHECK_NEARF(g[2]->x - g[1]->x, 30.0f); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(uniform_spacing_first_baseline_distance_from_box_edge)
{
    /* UNIFORM(30, 24): each line is 30 wide with the baseline 24 from its near (ascent) side; the ascent side of a rotated line faces right. */
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB", 16.0f, 300, 100); dwx_renderer *r;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    r = draw_ok(l, 0, 0);
    if (RUN(r, 0)) { t_note("uniform vertical baseline x = %g (box [270,300])", RUN(r, 0)->x); CHECK(RUN(r, 0)->x >= 270.0f - 0.001 && RUN(r, 0)->x <= 300.0f + 0.001); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(second_line_restarts_at_top)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\nCD", 16.0f, 300, 100); dwx_renderer *r = draw_ok(l, 0, 0); const dwx_draw *g[2]; int n = ink_runs(r, g, 2);
    EXPECT_EQ(n, 2);
    if (n == 2) { CHECK_NEARF(g[0]->y, 0.0f); CHECK_NEARF(g[1]->y, 0.0f); CHECK(g[1]->x < g[0]->x); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(trailing_text_alignment_moves_run_down)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 200); dwx_renderer *r;
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    r = draw_ok(l, 0, 0);
    if (RUN(r, 0)) CHECK_NEARF(RUN(r, 0)->y, 200 - 4 * A16);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(centered_text_alignment_centres_run)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 200); dwx_renderer *r;
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    r = draw_ok(l, 0, 0);
    if (RUN(r, 0)) CHECK_NEARF(RUN(r, 0)->y, (200 - 4 * A16) / 2);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(bottom_to_top_reading_anchors_text_at_bottom)
{
    VT_BEGIN(); IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, L"ABCD", 16.0f, 100, 200, RD_BTT, FD_RTL); dwx_renderer *r;
    if (!l) { VT_END(); t_skip("layout"); return; }
    r = draw_ok(l, 0, 0);
    if (RUN(r, 0)) {
        t_note("BTT reading: run bidi level %u, origin y %g", RUN(r, 0)->bidi, RUN(r, 0)->y);
        if (!(RUN(r, 0)->bidi & 1)) CHECK_NEARF(RUN(r, 0)->y, 200 - 4 * A16);      /* an even level run is positioned by its leading (top) edge */
        else CHECK_NEARF(RUN(r, 0)->y, 200.0f);
    } else CHECK(0);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(snapping_rounds_vertical_origins_to_pixels)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00", 16.0f, 100.3f, 200); dwx_renderer *r = dwx_renderer_new(); int i;
    r->support_r1 = 1;
    CHECK_HR_OK(vt_draw(l, r, 0.3f, 0.2f));
    for (i = 0; i < vt_runs(r); i++) { const dwx_draw *d = RUN(r, i); CHECKF(fabs(d->x - floor(d->x + 0.5)) < 1e-4, "run %d origin (%g,%g)", i, d->x, d->y); t_note("snapped vertical run %d origin (%g,%g): y keeps the unrounded inline-axis position", i, d->x, d->y); }
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(client_context_and_effect_reach_the_vertical_callbacks)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 200); dwx_effect *e = dwx_effect_new(7); DWRITE_TEXT_RANGE rg = { 1, 2 }; dwx_renderer *r; int i, seen = 0;
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, rg));
    r = draw_ok(l, 0, 0);
    for (i = 0; i < vt_runs(r); i++) { CHECK(RUN(r, i)->ctx == (void *)(ULONG_PTR)0x1234abcd); if (RUN(r, i)->effect == (IUnknown *)e) { seen++; EXPECT_EQ(RUN(r, i)->text_pos, 1); EXPECT_EQ(RUN(r, i)->nglyphs, 2); } }
    EXPECT_EQ(seen, 1); EXPECT_EQ(vt_runs(r), 3);                          /* A | BC (effect) | D */
    dwx_release(r); dwx_release(e); DW_RELEASE(l); VT_END();
}

/* ---- decorations and inline objects ---------------------------------------------------------------------------- */
TEST(underline_in_vertical_layout)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 200); DWRITE_TEXT_RANGE rg = { 0, 4 }; dwx_renderer *r; const dwx_draw *u;
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, rg));
    r = draw_ok(l, 0, 0); EXPECT_EQ(r->count[DWX_DRAW_UNDERLINE], 1);
    u = dwx_renderer_call(r, 0); { int cur = 0; u = dwx_renderer_next(r, DWX_DRAW_UNDERLINE, &cur); }
    if (u) {
        CHECK(u->has_orientation);
        CHECK_NEARF(u->ul.width, 4 * A16);
        EXPECT_EQ(u->ul.readingDirection, RD_TTB); EXPECT_EQ(u->ul.flowDirection, FD_RTL);
        t_note("vertical underline: origin (%g,%g) angle %d thickness %g offset %g", u->x, u->y, u->angle, u->ul.thickness, u->ul.offset);
    } else CHECK(u != NULL);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(strikethrough_in_vertical_layout)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"ABCD", 16.0f, 100, 200); DWRITE_TEXT_RANGE rg = { 0, 4 }; dwx_renderer *r; const dwx_draw *s; int cur = 0;
    CHECK_HR_OK(IDWriteTextLayout_SetStrikethrough(l, TRUE, rg));
    r = draw_ok(l, 0, 0); EXPECT_EQ(r->count[DWX_DRAW_STRIKETHROUGH], 1);
    s = dwx_renderer_next(r, DWX_DRAW_STRIKETHROUGH, &cur);
    if (s) { CHECK(s->has_orientation); CHECK_NEARF(s->st.width, 4 * A16); EXPECT_EQ(s->st.readingDirection, RD_TTB); EXPECT_EQ(s->st.flowDirection, FD_RTL); } else CHECK(s != NULL);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(underline_width_spans_mixed_orientation_runs)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB\x4e00\x4e01", 16.0f, 100, 200); DWRITE_TEXT_RANGE rg = { 0, 4 }; dwx_renderer *r; int cur = 0; const dwx_draw *u; float sum = 0; int i;
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, rg));
    r = draw_ok(l, 0, 0);
    for (i = 0; i < vt_runs(r); i++) sum += dwx_run_width(RUN(r, i));
    CHECK_NEARF(sum, 2 * A16 + 2 * V16);
    while ((u = dwx_renderer_next(r, DWX_DRAW_UNDERLINE, &cur)) != NULL) t_note("underline segment width %g", u->ul.width);
    CHECK(r->count[DWX_DRAW_UNDERLINE] >= 1);
    dwx_release(r); DW_RELEASE(l); VT_END();
}
TEST(inline_object_in_vertical_layout)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"A#B", 16.0f, 100, 200); dwx_inline *io = dwx_inline_new(12.0f, 8.0f, 6.0f, FALSE); DWRITE_TEXT_RANGE rg = { 1, 1 }; dwx_renderer *r; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, rg));
    r = vt_r1(); r->recurse_inline = 0;
    CHECK_HR_OK(vt_draw(l, r, 0, 0)); m = vt_metrics(l);
    EXPECT_EQ(r->count[DWX_DRAW_INLINE], 1);
    { int cur = 0; const dwx_draw *d = dwx_renderer_next(r, DWX_DRAW_INLINE, &cur);
      if (d) { CHECK(d->has_orientation); CHECK(d->obj == (IDWriteInlineObject *)io); t_note("inline in vertical layout: origin (%g,%g) angle %d sideways %d rtl %d; layout height %g width %g", d->x, d->y, d->angle, d->obj_sideways, d->obj_rtl, m.height, m.width); } }
    CHECK(io->n_metrics >= 1);
    dwx_release(r); DW_RELEASE(l); dwx_release(io); VT_END();
}
TEST(inline_object_extent_adds_to_inline_axis_noted)
{
    VT_BEGIN(); VT_VL(l, VT_FAM, L"AB", 16.0f, 100, 200); VT_VL(k, VT_FAM, L"A#B", 16.0f, 100, 200); dwx_inline *io = dwx_inline_new(12.0f, 8.0f, 6.0f, FALSE); DWRITE_TEXT_RANGE rg = { 1, 1 };
    DWRITE_TEXT_METRICS m0 = vt_metrics(l), m1;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(k, (IDWriteInlineObject *)io, rg)); m1 = vt_metrics(k);
    t_note("inline 12x8 baseline 6 in vertical text: height %g -> %g (A,B = %g), width %g -> %g", m0.height, m1.height, 2 * A16, m0.width, m1.width);
    CHECK(m1.height >= 2 * A16);
    dwx_release(io); DW_RELEASE(l); DW_RELEASE(k); VT_END();
}
