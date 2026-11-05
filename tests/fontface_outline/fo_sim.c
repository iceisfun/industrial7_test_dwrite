/* Font simulations (bold / oblique) and system-font relations for IDWriteFontFace::GetGlyphRunOutline.
 *
 * Windows: the glyph-outline cache key (GlyphOutlineRasterizationParameters) carries only the rasterizer flags derived by
 * MakeRasterizerFlagsForOutlineRendering(simulations, isSideways): bold = flag 2, oblique = 4 (+8 sideways).  The outline is
 * produced at ppem == unitsPerEm and scaled by emSize/unitsPerEm afterwards, so every outline, simulated or not, is exactly
 * linear in emSize.  Oblique shears the design outline: x' = x + y * 0x5700/65536 (0.33984375); sideways oblique shears
 * the other axis (y' = y - x * 0.33984375) before the 90 degree rotation. */
#include "fo.h"

#define OBL 0.33984375

static dwx_geom *run1(IDWriteFontFace *face, float em, UINT16 gid, BOOL sw, BOOL rtl) {
    dwx_geom *s = dwx_geom_new();
    CHECK_HR(fo_call(face, em, &gid, NULL, NULL, 1, sw, rtl, s), S_OK);
    return s;
}
static void same_kinds(const dwx_geom *a, const dwx_geom *b, const char *what) {
    int i, ok = a->nev == b->nev;
    for (i = 0; ok && i < a->nev; i++) if (a->ev[i].kind != b->ev[i].kind || a->ev[i].count != b->ev[i].count) ok = 0;
    CHECKF(ok, "%s: event structure differs (%d vs %d events)", what, a->nev, b->nev);
    CHECK(a->npts == b->npts);
}

/* ---- simulations flags ---------------------------------------------------------------------- */
TEST(sim_face_reports_simulations) {
    static const DWRITE_FONT_SIMULATIONS sims[] = { DWRITE_FONT_SIMULATIONS_NONE, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE,
                                                    DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE };
    size_t i;
    for (i = 0; i < 4; i++) {
        FO_LAB(h, sims[i]);
        CHECKF(IDWriteFontFace_GetSimulations(h.face) == sims[i], "sim %u -> %u", sims[i], IDWriteFontFace_GetSimulations(h.face));
        dwx_geom *s = run1(h.face, 100.0f, 2, FALSE, FALSE);
        CHECK(s->nfigures_begun == 1);
        FO_INVARIANTS(s, 1);
        dwx_release(s); dwx_font_close(&h);
    }
}

/* ---- bold ------------------------------------------------------------------------------------ */
TEST(bold_structure_unchanged) {
    FO_LAB(h0, 0); FO_LAB(h1, DWRITE_FONT_SIMULATIONS_BOLD);
    UINT16 gids[] = { 2, 64, 65, 68, 66, FO_G_ALLOFF4, FO_G_OFFSTART, FO_G_MULTI }; size_t i;
    for (i = 0; i < sizeof gids / sizeof gids[0]; i++) {
        dwx_geom *a = run1(h0.face, 200.0f, gids[i], FALSE, FALSE), *b = run1(h1.face, 200.0f, gids[i], FALSE, FALSE);
        same_kinds(a, b, "bold");
        FO_INVARIANTS(b, 1);
        dwx_release(a); dwx_release(b);
    }
    dwx_font_close(&h0); dwx_font_close(&h1);
}
TEST(bold_widens_box) {
    FO_LAB(h0, 0); FO_LAB(h1, DWRITE_FONT_SIMULATIONS_BOLD);
    dwx_geom *a = run1(h0.face, 2048.0f, 2, FALSE, FALSE), *b = run1(h1.face, 2048.0f, 2, FALSE, FALSE);
    float ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
    fo_bounds(a, &ax0, &ay0, &ax1, &ay1); fo_bounds(b, &bx0, &by0, &bx1, &by1);
    CHECKF(bx1 - bx0 > ax1 - ax0, "bold width %g <= regular %g", bx1 - bx0, ax1 - ax0);
    t_note("bold sim 'A' box @2048: regular x[%g,%g] y[%g,%g]  bold x[%g,%g] y[%g,%g]", ax0, ax1, ay0, ay1, bx0, bx1, by0, by1);
    dwx_release(a); dwx_release(b); dwx_font_close(&h0); dwx_font_close(&h1);
}
TEST(bold_widens_every_outline_bbox) {
    FO_LAB(h0, 0); FO_LAB(h1, DWRITE_FONT_SIMULATIONS_BOLD);
    UINT16 gids[] = { 2, 64, 65, 68, 66, FO_G_PENTA, FO_G_NEG, FO_G_ALLOFF4 }; size_t i;
    for (i = 0; i < sizeof gids / sizeof gids[0]; i++) {
        dwx_geom *a = run1(h0.face, 1000.0f, gids[i], FALSE, FALSE), *b = run1(h1.face, 1000.0f, gids[i], FALSE, FALSE);
        float ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
        fo_bounds(a, &ax0, &ay0, &ax1, &ay1); fo_bounds(b, &bx0, &by0, &bx1, &by1);
        CHECKF(bx1 - bx0 > ax1 - ax0, "gid %u: bold width %g regular %g", gids[i], bx1 - bx0, ax1 - ax0);
        dwx_release(a); dwx_release(b);
    }
    dwx_font_close(&h0); dwx_font_close(&h1);
}
TEST(bold_scales_linearly_with_em) {
    FO_LAB(h, DWRITE_FONT_SIMULATIONS_BOLD);
    /* outline cache is independent of emSize: bold outlines scale exactly like regular ones */
    UINT16 gids[] = { 2, 65, 68, FO_G_ALLOFF4 }; size_t i; int p;
    for (i = 0; i < 4; i++) {
        dwx_geom *a = run1(h.face, 2048.0f, gids[i], FALSE, FALSE), *b = run1(h.face, 128.0f, gids[i], FALSE, FALSE), *c = run1(h.face, 4096.0f, gids[i], FALSE, FALSE);
        CHECK(a->npts == b->npts && a->npts == c->npts && a->npts > 0);
        for (p = 0; p < a->npts && p < b->npts && p < c->npts; p++) {
            CHECK_NEAR(b->pts[p].x, a->pts[p].x / 16.0, 1e-3); CHECK_NEAR(b->pts[p].y, a->pts[p].y / 16.0, 1e-3);
            CHECK_NEAR(c->pts[p].x, a->pts[p].x * 2.0, 1e-3); CHECK_NEAR(c->pts[p].y, a->pts[p].y * 2.0, 1e-3);
        }
        dwx_release(a); dwx_release(b); dwx_release(c);
    }
    dwx_font_close(&h);
}
TEST(bold_advance_not_smaller) {
    FO_LAB(h0, 0); FO_LAB(h1, DWRITE_FONT_SIMULATIONS_BOLD);
    /* default advances come from the (simulated) face: bold never narrows the advance */
    UINT16 g[2] = { 2, 2 }; dwx_geom *a = dwx_geom_new(), *b = dwx_geom_new(); float ax, ay, bx, by;
    CHECK_HR(fo_call(h0.face, 2048.0f, g, NULL, NULL, 2, FALSE, FALSE, a), S_OK);
    CHECK_HR(fo_call(h1.face, 2048.0f, g, NULL, NULL, 2, FALSE, FALSE, b), S_OK);
    CHECK(fo_begin_pt(a, 2, &ax, &ay) && fo_begin_pt(b, 2, &bx, &by));
    t_note("2nd glyph begin x: regular %g, bold-simulated %g", ax, bx);
    DWRITE_GLYPH_METRICS m0, m1; UINT16 gi = 2; memset(&m0, 0, sizeof m0); memset(&m1, 0, sizeof m1);
    IDWriteFontFace_GetDesignGlyphMetrics(h0.face, &gi, 1, &m0, FALSE); IDWriteFontFace_GetDesignGlyphMetrics(h1.face, &gi, 1, &m1, FALSE);
    CHECK(m1.advanceWidth >= m0.advanceWidth);
    dwx_release(a); dwx_release(b); dwx_font_close(&h0); dwx_font_close(&h1);
}
TEST(bold_explicit_advances_still_used) {
    FO_LAB(h, DWRITE_FONT_SIMULATIONS_BOLD);
    UINT16 g[2] = { 2, 2 }; float adv[2] = { 321, 0 }; dwx_geom *s = dwx_geom_new(); float x1, y1, x2, y2;
    CHECK_HR(fo_call(h.face, 2048.0f, g, adv, NULL, 2, FALSE, FALSE, s), S_OK);
    CHECK(fo_begin_pt(s, 1, &x1, &y1) && fo_begin_pt(s, 2, &x2, &y2));
    CHECK_NEAR(x2 - x1, 321.0, 1e-3); CHECK_NEAR(y2, y1, 1e-4);
    dwx_release(s); dwx_font_close(&h);
}
TEST(bold_empty_glyph_still_empty) {
    FO_LAB(h, DWRITE_FONT_SIMULATIONS_BOLD);
    dwx_geom *s = run1(h.face, 100.0f, 1, FALSE, FALSE);
    CHECK(s->nev == 1); dwx_release(s); dwx_font_close(&h);
}

/* ---- oblique --------------------------------------------------------------------------------- */
static void oblique_model_check(UINT16 gid, double em, BOOL rtl) {
    FO_LAB(h, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    dwx_geom *s = run1(h.face, (float)em, gid, FALSE, rtl); fo_exp e; char tag[64];
    snprintf(tag, sizeof tag, "oblique gid %u em %g", gid, em);
    fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, em, &gid, NULL, NULL, 1, 0, rtl, OBL);
    FO_COMPARE(s, &e, 0.05 * em / 2048 + em * 1e-5 + 1e-4, tag);
    FO_INVARIANTS(s, 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(oblique_box_shear) { oblique_model_check(2, 2048.0, FALSE); }
TEST(oblique_box_shear_small_em) { oblique_model_check(2, 40.0, FALSE); }
TEST(oblique_triangle_shear) { oblique_model_check(64, 1024.0, FALSE); }
TEST(oblique_round_shear) { oblique_model_check(65, 512.0, FALSE); }
TEST(oblique_ring_shear) { oblique_model_check(68, 2048.0, FALSE); }
TEST(oblique_composite_shear) { oblique_model_check(66, 700.0, FALSE); }
TEST(oblique_offcurve_shear) { oblique_model_check(FO_G_ALLOFF4, 2048.0, FALSE); oblique_model_check(FO_G_OFFSTART, 100.0, FALSE); }
TEST(oblique_rtl_shear) { oblique_model_check(2, 2048.0, TRUE); }
TEST(oblique_explicit_box_corners) {
    FO_LAB(h, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    /* box (100,0)-(900,1400): bottom edge on the baseline is not moved, the top edge shifts right by 1400 * 0.33984375 = 475.78 */
    dwx_geom *s = run1(h.face, 2048.0f, 2, FALSE, FALSE);
    CHECK(s->npts == 4);
    if (s->npts == 4) {
        CHECK_NEAR(s->pts[0].x, 100, 0.05);                CHECK_NEAR(s->pts[0].y, 0, 1e-4);
        CHECK_NEAR(s->pts[1].x, 100 + 475.78125, 0.05);    CHECK_NEAR(s->pts[1].y, -1400, 1e-4);
        CHECK_NEAR(s->pts[2].x, 900 + 475.78125, 0.05);    CHECK_NEAR(s->pts[2].y, -1400, 1e-4);
        CHECK_NEAR(s->pts[3].x, 900, 0.05);                CHECK_NEAR(s->pts[3].y, 0, 1e-4);
    }
    dwx_release(s); dwx_font_close(&h);
}
TEST(oblique_y_coordinates_unchanged) {
    FO_LAB(h0, 0); FO_LAB(h1, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    UINT16 gids[] = { 2, 64, 65, 68, 66, FO_G_MULTI, FO_G_NEG, FO_G_ALLOFF3ODD }; size_t i; int p;
    for (i = 0; i < sizeof gids / sizeof gids[0]; i++) {
        dwx_geom *a = run1(h0.face, 640.0f, gids[i], FALSE, FALSE), *b = run1(h1.face, 640.0f, gids[i], FALSE, FALSE);
        same_kinds(a, b, "oblique");
        for (p = 0; p < a->npts && p < b->npts; p++) CHECK_NEAR(a->pts[p].y, b->pts[p].y, 0.02);
        dwx_release(a); dwx_release(b);
    }
    dwx_font_close(&h0); dwx_font_close(&h1);
}
TEST(oblique_advances_unchanged) {
    FO_LAB(h0, 0); FO_LAB(h1, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    UINT16 g[3] = { 2, 64, 0 }; dwx_geom *a = dwx_geom_new(), *b = dwx_geom_new(); int i;
    CHECK_HR(fo_call(h0.face, 2048.0f, g, NULL, NULL, 3, FALSE, FALSE, a), S_OK);
    CHECK_HR(fo_call(h1.face, 2048.0f, g, NULL, NULL, 3, FALSE, FALSE, b), S_OK);
    for (i = 2; i <= 3; i++) {   /* pen positions: begin x of the glyph minus the (sheared-less) start point x (bottom-left: y == 0 -> not sheared) */
        float ax, ay, bx, by; CHECK(fo_begin_pt(a, i, &ax, &ay) && fo_begin_pt(b, i, &bx, &by)); CHECK_NEAR(ax, bx, 0.05);
    }
    dwx_release(a); dwx_release(b); dwx_font_close(&h0); dwx_font_close(&h1);
}
TEST(oblique_scales_linearly_with_em) {
    FO_LAB(h, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    UINT16 gids[] = { 2, 65, 68 }; size_t i; int p;
    for (i = 0; i < 3; i++) {
        dwx_geom *a = run1(h.face, 2048.0f, gids[i], FALSE, FALSE), *b = run1(h.face, 64.0f, gids[i], FALSE, FALSE);
        CHECK(a->npts == b->npts && a->npts > 0);
        for (p = 0; p < a->npts && p < b->npts; p++) { CHECK_NEAR(b->pts[p].x, a->pts[p].x / 32.0, 1e-3); CHECK_NEAR(b->pts[p].y, a->pts[p].y / 32.0, 1e-3); }
        dwx_release(a); dwx_release(b);
    }
    dwx_font_close(&h);
}
TEST(oblique_sideways_note) {
    FO_LAB(h0, 0); FO_LAB(h1, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    /* sideways oblique is its own rasterizer variant (flag bit 8); the transform shears the design y axis: report the result */
    dwx_geom *a = run1(h0.face, 2048.0f, 2, TRUE, FALSE), *b = run1(h1.face, 2048.0f, 2, TRUE, FALSE);
    same_kinds(a, b, "sideways oblique");
    fo_exp e; char msg[200] = ""; UINT16 g = 2;
    fo_model_run(&e, fo_glyphs, FO_G_COUNT, h1.face, 2048.0, &g, NULL, NULL, 1, 1, 0, OBL);
    if (a->npts > 0 && b->npts > 0) t_note("sideways oblique first point: regular (%g,%g) oblique (%g,%g)", a->pts[0].x, a->pts[0].y, b->pts[0].x, b->pts[0].y);
    t_note("sideways oblique shear model (y' = y - 0.3398 x, then rotate): %s %s", fo_diff_(b, &e, 0.08, "sw-obl", msg, sizeof msg) ? "matches" : "differs:", msg);
    dwx_release(a); dwx_release(b); dwx_font_close(&h0); dwx_font_close(&h1);
}
TEST(oblique_bold_combined_structure) {
    FO_LAB(h0, 0); FO_LAB(h1, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE);
    UINT16 gids[] = { 2, 65, 68, FO_G_MULTI }; size_t i;
    for (i = 0; i < 4; i++) {
        dwx_geom *a = run1(h0.face, 300.0f, gids[i], FALSE, FALSE), *b = run1(h1.face, 300.0f, gids[i], FALSE, FALSE);
        same_kinds(a, b, "bold+oblique"); FO_INVARIANTS(b, 1);
        dwx_release(a); dwx_release(b);
    }
    /* top of the box is displaced to the right of its bottom (italic slant), in spite of emboldening */
    dwx_geom *s = run1(h1.face, 2048.0f, 2, FALSE, FALSE);
    if (s->npts == 4) CHECK(s->pts[1].x - s->pts[0].x > 300);
    dwx_release(s); dwx_font_close(&h0); dwx_font_close(&h1);
}

/* ---- bounding boxes vs metrics (synthetic font; exact) ---------------------------------------- */
TEST(bbox_matches_design_glyph_metrics) {
    FO_LAB(h, 0);
    UINT16 gids[] = { 0, 2, 64, 68, FO_G_PENTA, FO_G_NEG, FO_G_BIG, FO_G_NARROW }; size_t i;
    for (i = 0; i < sizeof gids / sizeof gids[0]; i++) {
        DWRITE_GLYPH_METRICS m; float x0, y0, x1, y1; double k = 0.25; UINT16 g = gids[i];
        dwx_geom *s = run1(h.face, 512.0f, g, FALSE, FALSE);
        memset(&m, 0, sizeof m);
        CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(h.face, &g, 1, &m, FALSE), S_OK);
        fo_bounds(s, &x0, &y0, &x1, &y1);
        /* straight-edge glyphs: the control-point box is the glyph box.  x0 = leftSideBearing, x1 = advance - rightSideBearing */
        CHECK_NEAR(x0, m.leftSideBearing * k, 0.01);
        CHECK_NEAR(x1, ((double)m.advanceWidth - m.rightSideBearing) * k, 0.01);
        CHECK_NEAR(-y0, ((double)m.verticalOriginY - m.topSideBearing) * k, 0.01);
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(bbox_within_font_metrics_box) {
    FO_NEED_FACE();
    DWRITE_FONT_METRICS fm; UINT16 g = 2; dwx_geom *s = run1(h.face, 2048.0f, g, FALSE, FALSE); float x0, y0, x1, y1;
    IDWriteFontFace_GetMetrics(h.face, &fm);
    fo_bounds(s, &x0, &y0, &x1, &y1);
    CHECK(-y0 <= fm.ascent && y1 >= -fm.descent);           /* synthetic hhea ascent 1700, descent 400 */
    CHECK(fm.designUnitsPerEm == 2048);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- system fonts ------------------------------------------------------------------------------ */
static IDWriteFontFace *sys(const WCHAR *fam, DWRITE_FONT_WEIGHT w) {
    IDWriteFactory *f = dw_shared(); if (!f) return NULL;
    return dw_sysface(f, fam, w, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
}
static UINT16 gidof(IDWriteFontFace *face, UINT32 cp) { UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g); return g; }
#define NEED_SYS(var, fam, w) IDWriteFontFace *var = sys(fam, w); if (!var) { t_skip("font %ls not installed", fam); return; }

static void figure_count_case(const WCHAR *fam, UINT32 cp, int figs, int exact_lines_only) {
    IDWriteFontFace *face = sys(fam, DWRITE_FONT_WEIGHT_REGULAR);
    if (!face) { t_skip("font %ls not installed", fam); return; }
    UINT16 g = gidof(face, cp); dwx_geom *s;
    CHECK(g != 0);
    s = run1(face, 2048.0f, g, FALSE, FALSE);
    if (figs >= 0) CHECKF(s->nfigures_begun == figs, "%ls U+%04X: %d figures, want %d", fam, cp, s->nfigures_begun, figs);
    else CHECKF(s->nfigures_begun >= -figs, "%ls U+%04X: %d figures, want >= %d", fam, cp, s->nfigures_begun, -figs);
    if (exact_lines_only) CHECK(s->nbeziers == 0 && s->nlines > 0);
    FO_INVARIANTS(s, 1);
    dwx_release(s); IDWriteFontFace_Release(face);
}
TEST(sys_arial_O_two_figures) { figure_count_case(L"Arial", 'O', 2, 0); }
TEST(sys_arial_A_two_figures_lines_only) { figure_count_case(L"Arial", 'A', 2, 1); }
TEST(sys_arial_period_one_figure_lines_only) { figure_count_case(L"Arial", '.', 1, 1); }
TEST(sys_arial_space_no_figures) { figure_count_case(L"Arial", ' ', 0, 0); }
TEST(sys_arial_B_three_figures) { figure_count_case(L"Arial", 'B', 3, 0); }
TEST(sys_arial_I_one_figure) { figure_count_case(L"Arial", 'I', 1, 1); }
TEST(sys_arial_H_one_figure_lines_only) { figure_count_case(L"Arial", 'H', 1, 1); }
TEST(sys_arial_zero_two_figures) { figure_count_case(L"Arial", '0', 2, 0); }
TEST(sys_times_O_counter) { figure_count_case(L"Times New Roman", 'O', -2, 0); }
TEST(sys_courier_O_counter) { figure_count_case(L"Courier New", 'O', -2, 0); }
TEST(sys_segoe_O_counter) { figure_count_case(L"Segoe UI", 'O', -2, 0); }
TEST(sys_tahoma_O_counter) { figure_count_case(L"Tahoma", 'O', -2, 0); }
TEST(sys_consolas_O_counter) { figure_count_case(L"Consolas", 'O', -2, 0); }
TEST(sys_times_space_empty) { figure_count_case(L"Times New Roman", ' ', 0, 0); }
TEST(sys_segoe_space_empty) { figure_count_case(L"Segoe UI", ' ', 0, 0); }

static int be16(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }
static UINT32 be32(const uint8_t *p) { return ((UINT32)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
/* glyph header bbox from glyf/loca/head: 1 on success (simple glyph with contours) */
static int glyf_bbox(IDWriteFontFace *face, UINT16 gid, int *x0, int *y0, int *x1, int *y1) {
    UINT32 hl, ll, gl; uint8_t *head = dw_face_table(face, DW_TAG('h','e','a','d'), &hl), *loca = dw_face_table(face, DW_TAG('l','o','c','a'), &ll), *glyf = dw_face_table(face, DW_TAG('g','l','y','f'), &gl);
    int ok = 0;
    if (head && loca && glyf && hl >= 54) {
        int fmt = be16(head + 50); UINT32 o0, o1;
        if (fmt == 0 && (UINT32)(gid + 2) * 2 <= ll) { o0 = (UINT32)(loca[gid * 2] << 8 | loca[gid * 2 + 1]) * 2; o1 = (UINT32)(loca[gid * 2 + 2] << 8 | loca[gid * 2 + 3]) * 2; }
        else if (fmt == 1 && (UINT32)(gid + 2) * 4 <= ll) { o0 = be32(loca + gid * 4); o1 = be32(loca + gid * 4 + 4); }
        else o0 = o1 = 0;
        if (o1 > o0 && o0 + 10 <= gl && be16(glyf + o0) > 0) { *x0 = be16(glyf + o0 + 2); *y0 = be16(glyf + o0 + 4); *x1 = be16(glyf + o0 + 6); *y1 = be16(glyf + o0 + 8); ok = 1; }
    }
    free(head); free(loca); free(glyf);
    return ok;
}
static void bbox_vs_glyf(const WCHAR *fam, const UINT32 *cps, int n) {
    IDWriteFontFace *face = sys(fam, DWRITE_FONT_WEIGHT_REGULAR); int i;
    if (!face) { t_skip("font %ls not installed", fam); return; }
    DWRITE_FONT_METRICS fm; IDWriteFontFace_GetMetrics(face, &fm);
    for (i = 0; i < n; i++) {
        UINT16 g = gidof(face, cps[i]); int bx0, by0, bx1, by1; float x0, y0, x1, y1; double k = 1000.0 / fm.designUnitsPerEm;
        if (!g || !glyf_bbox(face, g, &bx0, &by0, &bx1, &by1)) { t_note("%ls U+%04X: no simple glyf header", fam, cps[i]); continue; }
        dwx_geom *s = run1(face, 1000.0f, g, FALSE, FALSE);
        fo_bounds(s, &x0, &y0, &x1, &y1);
        /* the outline is unhinted at ppem == upm; its control-point box is the glyf header box (+-1 design unit) */
        int straight = (cps[i] == 'H' || cps[i] == 'I' || cps[i] == 'E' || cps[i] == 'L' || cps[i] == 'T');   /* curved glyphs may be hint-adjusted or have off-curve extrema: relation only */
        if (!straight) { int okb = fabs(x0 - bx0 * k) <= 3 * k && fabs(x1 - bx1 * k) <= 3 * k; if (!okb) t_note("%ls U+%04X: bbox differs from glyf header", fam, cps[i]); dwx_release(s); continue; }
        CHECKF(fabs(x0 - bx0 * k) <= 1.5 * k && fabs(x1 - bx1 * k) <= 1.5 * k && fabs(-y1 - by0 * k) <= 1.5 * k && fabs(-y0 - by1 * k) <= 1.5 * k,
               "%ls U+%04X: outline x[%g,%g] y[%g,%g] vs glyf [%d,%d]-[%d,%d] @upm %u", fam, cps[i], x0, x1, -y1, -y0, bx0, by0, bx1, by1, fm.designUnitsPerEm);
        dwx_release(s);
    }
    IDWriteFontFace_Release(face);
}
TEST(sys_arial_bbox_equals_glyf_header) { static const UINT32 c[] = { 'H', 'I', 'E', 'L', 'T', 'A', 'O', 'o', 'g', 'y', '.', ',', '0', '8', 'B' }; bbox_vs_glyf(L"Arial", c, 15); }
TEST(sys_times_bbox_equals_glyf_header) { static const UINT32 c[] = { 'H', 'I', 'E', 'O', 'g', 'a', '.', '0' }; bbox_vs_glyf(L"Times New Roman", c, 8); }
TEST(sys_courier_bbox_equals_glyf_header) { static const UINT32 c[] = { 'H', 'O', 'g', '.', 'm' }; bbox_vs_glyf(L"Courier New", c, 5); }
TEST(sys_segoe_bbox_equals_glyf_header) { static const UINT32 c[] = { 'H', 'O', 'g', '.', 'e' }; bbox_vs_glyf(L"Segoe UI", c, 5); }
TEST(sys_tahoma_bbox_equals_glyf_header) { static const UINT32 c[] = { 'H', 'O', 'g', '.', 'e' }; bbox_vs_glyf(L"Tahoma", c, 5); }
TEST(sys_consolas_bbox_equals_glyf_header) { static const UINT32 c[] = { 'H', 'O', 'g', '.', 'e' }; bbox_vs_glyf(L"Consolas", c, 5); }

TEST(sys_arial_linear_in_em) {
    NEED_SYS(face, L"Arial", DWRITE_FONT_WEIGHT_REGULAR);
    static const UINT32 cps[] = { 'O', 'g', 'e', '&', 'S' }; int i, p;
    for (i = 0; i < 5; i++) {
        UINT16 g = gidof(face, cps[i]); dwx_geom *a = run1(face, 12.0f, g, FALSE, FALSE), *b = run1(face, 1200.0f, g, FALSE, FALSE);
        CHECK(a->npts == b->npts && a->npts > 0);
        for (p = 0; p < a->npts && p < b->npts; p++) { CHECK_NEAR(b->pts[p].x, a->pts[p].x * 100.0, 0.02); CHECK_NEAR(b->pts[p].y, a->pts[p].y * 100.0, 0.02); }
        dwx_release(a); dwx_release(b);
    }
    IDWriteFontFace_Release(face);
}
TEST(sys_arial_bbox_inside_ascent_descent) {
    NEED_SYS(face, L"Arial", DWRITE_FONT_WEIGHT_REGULAR);
    DWRITE_FONT_METRICS fm; IDWriteFontFace_GetMetrics(face, &fm);
    static const UINT32 cps[] = { 'H', 'O', 'g', 'y', 'p', 'l', '(', 'j' }; int i; double k = 1000.0 / fm.designUnitsPerEm;
    for (i = 0; i < 8; i++) {
        UINT16 g = gidof(face, cps[i]); dwx_geom *s = run1(face, 1000.0f, g, FALSE, FALSE); float x0, y0, x1, y1;
        fo_bounds(s, &x0, &y0, &x1, &y1);
        CHECKF(-y0 <= fm.ascent * k + 1, "U+%04X top %g above ascent %g", cps[i], -y0, fm.ascent * k);
        CHECKF(y1 <= fm.descent * k + 1, "U+%04X bottom %g below descent %g", cps[i], y1, fm.descent * k);
        dwx_release(s);
    }
    t_note("Arial metrics: upm %u ascent %u descent %u capHeight %u xHeight %u", fm.designUnitsPerEm, fm.ascent, fm.descent, fm.capHeight, fm.xHeight);
    IDWriteFontFace_Release(face);
}
TEST(sys_arial_cap_height_relation) {
    NEED_SYS(face, L"Arial", DWRITE_FONT_WEIGHT_REGULAR);
    DWRITE_FONT_METRICS fm; IDWriteFontFace_GetMetrics(face, &fm);
    UINT16 g = gidof(face, 'H'); dwx_geom *s = run1(face, (float)fm.designUnitsPerEm, g, FALSE, FALSE); float x0, y0, x1, y1;
    fo_bounds(s, &x0, &y0, &x1, &y1);
    t_note("Arial 'H' at em=upm: top %g (capHeight %u), left %g right %g", -y0, fm.capHeight, x0, x1);
    CHECK_NEAR(-y0, fm.capHeight, 3.0);       /* flat-topped capital: OS/2 sCapHeight describes it */
    CHECK_NEAR(y1, 0, 1e-3);
    dwx_release(s); IDWriteFontFace_Release(face);
}
TEST(sys_arial_bold_wider_than_regular) {
    NEED_SYS(reg, L"Arial", DWRITE_FONT_WEIGHT_REGULAR);
    IDWriteFontFace *bold = sys(L"Arial", DWRITE_FONT_WEIGHT_BOLD);
    if (!bold) { IDWriteFontFace_Release(reg); t_skip("Arial Bold missing"); return; }
    UINT16 gr = gidof(reg, 'H'), gb = gidof(bold, 'H'); float a0, b0, a1, b1, c0, d0, c1, d1;
    dwx_geom *a = run1(reg, 1000.0f, gr, FALSE, FALSE), *b = run1(bold, 1000.0f, gb, FALSE, FALSE);
    fo_bounds(a, &a0, &b0, &a1, &b1); fo_bounds(b, &c0, &d0, &c1, &d1);
    t_note("Arial H width regular %g bold %g", a1 - a0, c1 - c0);
    CHECK(c1 - c0 > 0 && a1 - a0 > 0);
    CHECK(a->nfigures_begun == 1 && b->nfigures_begun == 1);
    dwx_release(a); dwx_release(b); IDWriteFontFace_Release(reg); IDWriteFontFace_Release(bold);
}
TEST(sys_arial_file_with_simulations) {
    IDWriteFactory *f = dw_shared(); char path[MAX_PATH]; static const char *const cand[] = { "arial.ttf", NULL };
    if (!f || !dw_find_system_font(cand, path, sizeof path)) { t_skip("arial.ttf not found"); return; }
    WCHAR wp[MAX_PATH]; IDWriteFontFile *file = NULL; IDWriteFontFace *face[4] = { 0 }; int i;
    static const DWRITE_FONT_SIMULATIONS sims[4] = { 0, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, 3 };
    dw_wide(path, wp, MAX_PATH);
    DW_REQUIRE_OK(IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file));
    for (i = 0; i < 4; i++) DW_REQUIRE_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, sims[i], &face[i]));
    UINT16 g = gidof(face[0], 'A'); dwx_geom *s[4]; float bb[4][4];
    for (i = 0; i < 4; i++) { s[i] = run1(face[i], 1000.0f, g, FALSE, FALSE); fo_bounds(s[i], &bb[i][0], &bb[i][1], &bb[i][2], &bb[i][3]); }
    for (i = 1; i < 4; i++) same_kinds(s[0], s[i], "arial simulated");
    CHECK(bb[1][2] - bb[1][0] > bb[0][2] - bb[0][0]);                       /* bold wider */
    CHECK_NEAR(bb[2][1], bb[0][1], 0.5); CHECK_NEAR(bb[2][3], bb[0][3], 0.5);   /* oblique keeps y */
    CHECK(bb[2][2] - bb[0][2] > 100);                                       /* top right corner sheared right */
    for (i = 0; i < 4; i++) { dwx_release(s[i]); IDWriteFontFace_Release(face[i]); }
    IDWriteFontFile_Release(file);
}
TEST(sys_msgothic_ttc_outline) {
    IDWriteFactory *f = dw_shared(); char path[MAX_PATH]; static const char *const cand[] = { "msgothic.ttc", NULL };
    if (!f || !dw_find_system_font(cand, path, sizeof path)) { t_skip("msgothic.ttc not found"); return; }
    WCHAR wp[MAX_PATH]; IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL; BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf = 0;
    dw_wide(path, wp, MAX_PATH);
    DW_REQUIRE_OK(IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file));
    DW_REQUIRE_OK(IDWriteFontFile_Analyze(file, &sup, &ft, &fct, &nf));
    DW_REQUIRE_OK(IDWriteFactory_CreateFontFace(f, fct, 1, &file, 0, 0, &face));
    UINT16 g = gidof(face, 'A'); dwx_geom *s = run1(face, 100.0f, g, FALSE, FALSE);
    CHECK(g != 0); CHECK(s->nfigures_begun >= 1); FO_INVARIANTS(s, 1);
    dwx_release(s); IDWriteFontFace_Release(face); IDWriteFontFile_Release(file);
}
TEST(sys_variable_bahnschrift_note) {
    NEED_SYS(face, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR);
    UINT16 g = gidof(face, 'A'); dwx_geom *s = run1(face, 100.0f, g, FALSE, FALSE);
    CHECK(g != 0); CHECK(s->nfigures_begun >= 1);
    FO_INVARIANTS(s, 1);
    t_note("Bahnschrift 'A': %d figures %d lines %d beziers", s->nfigures_begun, s->nlines, s->nbeziers);
    dwx_release(s); IDWriteFontFace_Release(face);
}
TEST(sys_symbol_fonts_nonempty) {
    static const WCHAR *const fams[] = { L"Marlett", L"Segoe UI Symbol", L"Segoe UI Emoji", L"Wingdings" }; size_t i; int tested = 0;
    for (i = 0; i < 4; i++) {
        IDWriteFontFace *face = sys(fams[i], DWRITE_FONT_WEIGHT_REGULAR); if (!face) continue;
        UINT32 cp = (i == 0) ? 0x30 : (i == 1) ? 0x2660 : (i == 2) ? 0x25A0 : 0x41; UINT16 g = gidof(face, cp);
        if (!g) { IDWriteFontFace_Release(face); continue; }
        dwx_geom *s = run1(face, 64.0f, g, FALSE, FALSE);
        FO_INVARIANTS(s, 1);
        t_note("%ls U+%04X: %d figures", fams[i], cp, s->nfigures_begun); tested++;
        dwx_release(s); IDWriteFontFace_Release(face);
    }
    if (!tested) t_skip("no symbol font available");
}
TEST(sys_gdi_font_face_same_outline) {
    /* a face created from GDI interop for the same font produces the same outline as the collection face */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; NEED_SYS(face, L"Arial", DWRITE_FONT_WEIGHT_REGULAR);
    if (!f || FAILED(IDWriteFactory_GetGdiInterop(f, &gi)) || !gi) { IDWriteFontFace_Release(face); t_skip("no GDI interop"); return; }
    LOGFONTW lf; IDWriteFont *font = NULL; IDWriteFontFace *face2 = NULL;
    memset(&lf, 0, sizeof lf); lf.lfHeight = -20; lf.lfWeight = FW_NORMAL; lstrcpyW(lf.lfFaceName, L"Arial");
    if (SUCCEEDED(IDWriteGdiInterop_CreateFontFromLOGFONT(gi, &lf, &font)) && font && SUCCEEDED(IDWriteFont_CreateFontFace(font, &face2)) && face2) {
        UINT16 g = gidof(face, 'g'), g2 = gidof(face2, 'g'); dwx_geom *a = run1(face, 77.0f, g, FALSE, FALSE), *b = run1(face2, 77.0f, g2, FALSE, FALSE);
        CHECK(g == g2); CHECK(a->npts == b->npts && a->npts > 0 && !memcmp(a->pts, b->pts, sizeof(D2D1_POINT_2F) * a->npts));
        dwx_release(a); dwx_release(b);
    } else t_skip("CreateFontFromLOGFONT failed");
    DW_RELEASE(face2); DW_RELEASE(font); IDWriteGdiInterop_Release(gi); IDWriteFontFace_Release(face);
}
