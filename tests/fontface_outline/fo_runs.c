/* Glyph runs: advances, glyph offsets, right-to-left, sideways.
 * Windows ComputeGlyphOrigins (called with a NULL matrix, natural measuring):
 *   LTR, advances given:   origin.x = running sum of advances, y = 0
 *   LTR, advances NULL:    the font's advance of each glyph (design -> DIPs) is used
 *   RTL:   origin.x = pen - fontAdvance(glyph); then pen -= (advances ? advances[i] : fontAdvance)
 *   offsets: LTR x += advanceOffset, RTL x -= advanceOffset, y -= ascenderOffset (positive ascenderOffset moves up)
 *   sideways: advance = advanceHeight (or advances[i]); origin.x = pen + verticalOriginY, origin.y = advanceWidth / 2;
 *             outline rotated: (x, y-up) -> (ox - y*k, oy - x*k). */
#include "fo.h"

static const int N_ = 0;
static dwx_geom *run(IDWriteFontFace *face, float em, const UINT16 *g, const float *adv, const DWRITE_GLYPH_OFFSET *off, UINT32 n, BOOL sw, BOOL rtl) {
    dwx_geom *s = dwx_geom_new();
    CHECK_HR(fo_call(face, em, g, adv, off, n, sw, rtl, s), S_OK);
    return s;
}
/* x of the first figure's begin point of glyph #i, where every glyph is a letter box (first point (100,0)). */
static int box_origin_x(const dwx_geom *s, int figure, float *ox, float *oy) {
    float x, y; if (!fo_begin_pt(s, figure, &x, &y)) return 0;
    *ox = x - 100.0f; *oy = y; return 1;
}

/* ---- LTR advances ------------------------------------------------------------------------ */
TEST(ltr_default_advances_explicit) {
    FO_NEED_FACE();
    UINT16 g[3] = { 2, 3, 4 }; dwx_geom *s = run(h.face, 2048.0f, g, NULL, NULL, 3, FALSE, FALSE); float x, y;
    CHECK(s->nfigures_begun == 3);
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == 100 && y == 0);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 1100 && y == 0);     /* + glyph advance 1000 design units */
    CHECK(fo_begin_pt(s, 3, &x, &y) && x == 2100 && y == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_default_advances_scale_with_em) {
    FO_NEED_FACE();
    UINT16 g[2] = { 2, 3 }; dwx_geom *s = run(h.face, 1024.0f, g, NULL, NULL, 2, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 50 + 500 && y == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_custom_advances_explicit) {
    FO_NEED_FACE();
    UINT16 g[3] = { 2, 3, 4 }; float adv[3] = { 300, 700, 5 }; dwx_geom *s = run(h.face, 2048.0f, g, adv, NULL, 3, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == 100);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 400);
    CHECK(fo_begin_pt(s, 3, &x, &y) && x == 1100);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_custom_advances_in_dips_not_scaled) {
    FO_NEED_FACE();
    /* advances are DIPs: independent of emSize */
    UINT16 g[2] = { 2, 3 }; float adv[2] = { 30, 0 }; float x, y; size_t i; static const float ems[] = { 10, 100, 2048 };
    for (i = 0; i < 3; i++) {
        dwx_geom *s = run(h.face, ems[i], g, adv, NULL, 2, FALSE, FALSE);
        CHECK(fo_begin_pt(s, 1, &x, &y)); float x1 = x;
        CHECK(fo_begin_pt(s, 2, &x, &y)); CHECK_NEAR(x - x1, 30.0, 1e-4);
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(ltr_advances_equal_font_advances_same_as_null) {
    FO_LAB(h, 0);
    UINT16 g[5] = { 0, 2, TTF_GID_EMPTY500, FO_G_NARROW, 3 }; float adv[5]; int i; const double em = 640.0, k = em / 2048;
    for (i = 0; i < 5; i++) adv[i] = (float)(fo_glyphs[g[i]].adv * k);
    dwx_geom *a = run(h.face, (float)em, g, NULL, NULL, 5, FALSE, FALSE), *b = run(h.face, (float)em, g, adv, NULL, 5, FALSE, FALSE);
    CHECK(a->npts == b->npts && a->npts > 0);
    for (i = 0; i < a->npts && i < b->npts; i++) { CHECK_NEAR(a->pts[i].x, b->pts[i].x, 1e-3); CHECK_NEAR(a->pts[i].y, b->pts[i].y, 1e-3); }
    dwx_release(a); dwx_release(b); dwx_font_close(&h);
}
TEST(ltr_zero_advances_overlap) {
    FO_NEED_FACE();
    UINT16 g[3] = { 2, 2, 2 }; float adv[3] = { 0, 0, 0 }; dwx_geom *s = run(h.face, 2048.0f, g, adv, NULL, 3, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == 100);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 100);
    CHECK(fo_begin_pt(s, 3, &x, &y) && x == 100);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_negative_advance_moves_left) {
    FO_NEED_FACE();
    UINT16 g[3] = { 2, 2, 2 }; float adv[3] = { -400, 150, 0 }; dwx_geom *s = run(h.face, 2048.0f, g, adv, NULL, 3, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 100 - 400);
    CHECK(fo_begin_pt(s, 3, &x, &y) && x == 100 - 400 + 150);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_fractional_advances_accumulate) {
    FO_NEED_FACE();
    UINT16 g[4] = { 2, 2, 2, 2 }; float adv[4] = { 10.25f, 0.5f, 3.125f, 99 }; dwx_geom *s = run(h.face, 2048.0f, g, adv, NULL, 4, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 110.25f);
    CHECK(fo_begin_pt(s, 3, &x, &y) && x == 110.75f);
    CHECK(fo_begin_pt(s, 4, &x, &y) && x == 113.875f);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_last_advance_is_irrelevant) {
    FO_NEED_FACE();
    UINT16 g[2] = { 2, 3 }; float a1[2] = { 200, 1 }, a2[2] = { 200, 999999 };
    dwx_geom *a = run(h.face, 100.0f, g, a1, NULL, 2, FALSE, FALSE), *b = run(h.face, 100.0f, g, a2, NULL, 2, FALSE, FALSE);
    CHECK(a->npts == b->npts && !memcmp(a->pts, b->pts, sizeof(D2D1_POINT_2F) * a->npts));
    dwx_release(a); dwx_release(b); dwx_font_close(&h);
}
TEST(ltr_notdef_advance_1200) {
    FO_NEED_FACE();
    UINT16 g[2] = { 0, 2 }; dwx_geom *s = run(h.face, 2048.0f, g, NULL, NULL, 2, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 1200 + 100);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_empty500_advance) {
    FO_NEED_FACE();
    UINT16 g[3] = { 2, TTF_GID_EMPTY500, 2 }; dwx_geom *s = run(h.face, 2048.0f, g, NULL, NULL, 3, FALSE, FALSE); float x, y;
    CHECK(s->nfigures_begun == 2);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 1000 + 500 + 100);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_large_advance_float_precision) {
    FO_NEED_FACE();
    UINT16 g[2] = { 2, 2 }; float adv[2] = { 100000.0f, 0 }; dwx_geom *s = run(h.face, 2048.0f, g, adv, NULL, 2, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 100100.0f);
    dwx_release(s); dwx_font_close(&h);
}
TEST(ltr_shape_independent_of_position) {
    FO_LAB(h, 0);
    /* a glyph far from the origin has exactly the same shape (differences of coordinates) as at the origin */
    UINT16 g[2] = { TTF_GID_ROUND, TTF_GID_ROUND }; float adv[2] = { 512, 0 }; dwx_geom *s = run(h.face, 256.0f, g, adv, NULL, 2, FALSE, FALSE);
    CHECK(s->npts == 26);
    int i; for (i = 0; i < 13 && s->npts == 26; i++) { CHECK_NEAR(s->pts[13 + i].x - s->pts[i].x, 512.0, 1e-3); CHECK_NEAR(s->pts[13 + i].y, s->pts[i].y, 1e-4); }
    dwx_release(s); dwx_font_close(&h);
}

/* ---- glyph offsets ----------------------------------------------------------------------------- */
TEST(offsets_explicit) {
    FO_NEED_FACE();
    UINT16 g[2] = { 2, 2 }; DWRITE_GLYPH_OFFSET off[2] = { { 50.0f, 70.0f }, { 0, 0 } }; dwx_geom *s = run(h.face, 2048.0f, g, NULL, off, 2, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == 150 && y == -70);      /* ascenderOffset > 0 moves the glyph up (y-down: negative) */
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 1100 && y == 0);       /* the pen is not moved by an offset */
    dwx_release(s); dwx_font_close(&h);
}
TEST(offsets_negative) {
    FO_NEED_FACE();
    UINT16 g[1] = { 2 }; DWRITE_GLYPH_OFFSET off[1] = { { -30.0f, -12.5f } }; dwx_geom *s = run(h.face, 2048.0f, g, NULL, off, 1, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == 70 && y == 12.5f);
    dwx_release(s); dwx_font_close(&h);
}
TEST(offsets_affect_only_own_glyph) {
    FO_NEED_FACE();
    UINT16 g[3] = { 2, 3, 4 }; DWRITE_GLYPH_OFFSET off[3] = { { 0, 0 }, { 11, 22 }, { 0, 0 } };
    dwx_geom *a = run(h.face, 300.0f, g, NULL, NULL, 3, FALSE, FALSE), *b = run(h.face, 300.0f, g, NULL, off, 3, FALSE, FALSE);
    CHECK(a->npts == b->npts && a->npts == 12);
    int i; for (i = 0; i < 12 && a->npts == 12; i++) {
        double dx = (i >= 4 && i < 8) ? 11 : 0, dy = (i >= 4 && i < 8) ? -22 : 0;
        CHECK_NEAR(b->pts[i].x - a->pts[i].x, dx, 1e-4); CHECK_NEAR(b->pts[i].y - a->pts[i].y, dy, 1e-4);
    }
    dwx_release(a); dwx_release(b); dwx_font_close(&h);
}
TEST(offsets_not_scaled_by_em) {
    FO_NEED_FACE();
    UINT16 g[1] = { 2 }; DWRITE_GLYPH_OFFSET off[1] = { { 10.0f, 5.0f } }; size_t i; static const float ems[] = { 8, 100, 2048 };
    for (i = 0; i < 3; i++) {
        dwx_geom *s = run(h.face, ems[i], g, NULL, off, 1, FALSE, FALSE); float x, y;
        CHECK(fo_begin_pt(s, 1, &x, &y)); CHECK_NEAR(x, 100.0 * ems[i] / 2048 + 10, 1e-4); CHECK_NEAR(y, -5, 1e-6);
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(offsets_zero_array_equals_null) {
    FO_LAB(h, 0);
    UINT16 g[4] = { 2, FO_G_OFFSTART, 65, 68 }; DWRITE_GLYPH_OFFSET off[4]; memset(off, 0, sizeof off);
    dwx_geom *a = run(h.face, 90.0f, g, NULL, NULL, 4, FALSE, FALSE), *b = run(h.face, 90.0f, g, NULL, off, 4, FALSE, FALSE);
    CHECK(a->nev == b->nev && a->npts == b->npts && a->npts > 0);
    CHECK(!memcmp(a->pts, b->pts, sizeof(D2D1_POINT_2F) * a->npts));
    dwx_release(a); dwx_release(b); dwx_font_close(&h);
}
TEST(offsets_with_custom_advances) {
    FO_NEED_FACE();
    UINT16 g[2] = { 2, 2 }; float adv[2] = { 100, 0 }; DWRITE_GLYPH_OFFSET off[2] = { { 0, 0 }, { 7, 9 } };
    dwx_geom *s = run(h.face, 2048.0f, g, adv, off, 2, FALSE, FALSE); float x, y;
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 100 + 100 + 7 && y == -9);
    dwx_release(s); dwx_font_close(&h);
}
TEST(offsets_and_advances_independent) {
    FO_NEED_FACE();
    /* glyph 0 is shifted by +10 while the pen moves by 10: glyph 1 lands exactly where glyph 0 was drawn */
    UINT16 g[2] = { 2, 2 }; float adv[2] = { 10, 0 }; DWRITE_GLYPH_OFFSET off[2] = { { 10, 0 }, { 0, 0 } };
    dwx_geom *s = run(h.face, 2048.0f, g, adv, off, 2, FALSE, FALSE); float x, y, x1;
    CHECK(fo_begin_pt(s, 1, &x1, &y) && x1 == 110);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 110);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- right to left ------------------------------------------------------------------------- */
TEST(rtl_default_explicit) {
    FO_NEED_FACE();
    UINT16 g[3] = { 2, 3, 4 }; dwx_geom *s = run(h.face, 2048.0f, g, NULL, NULL, 3, FALSE, TRUE); float x, y;
    /* each glyph is drawn to the LEFT of the pen by its own advance: origins -1000, -2000, -3000 */
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == -900 && y == 0);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == -1900);
    CHECK(fo_begin_pt(s, 3, &x, &y) && x == -2900);
    dwx_release(s); dwx_font_close(&h);
}
TEST(rtl_custom_advances_explicit) {
    FO_NEED_FACE();
    /* position uses the glyph's own font advance, the pen then moves left by the supplied advance */
    UINT16 g[3] = { 2, 3, 4 }; float adv[3] = { 300, 700, 50 }; dwx_geom *s = run(h.face, 2048.0f, g, adv, NULL, 3, FALSE, TRUE); float x, y;
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == -1000 + 100);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == -300 - 1000 + 100);
    CHECK(fo_begin_pt(s, 3, &x, &y) && x == -1000 - 1000 + 100);
    dwx_release(s); dwx_font_close(&h);
}
TEST(rtl_offsets_advance_offset_negated) {
    FO_NEED_FACE();
    UINT16 g[1] = { 2 }; DWRITE_GLYPH_OFFSET off[1] = { { 25.0f, 40.0f } }; float x, y;
    dwx_geom *l = run(h.face, 2048.0f, g, NULL, off, 1, FALSE, FALSE), *r = run(h.face, 2048.0f, g, NULL, off, 1, FALSE, TRUE);
    CHECK(fo_begin_pt(l, 1, &x, &y) && x == 125 && y == -40);
    CHECK(fo_begin_pt(r, 1, &x, &y) && x == -1000 - 25 + 100 && y == -40);    /* x offset is subtracted, y offset is the same */
    dwx_release(l); dwx_release(r); dwx_font_close(&h);
}
TEST(rtl_notdef_uses_its_own_advance) {
    FO_NEED_FACE();
    UINT16 g[2] = { 0, 2 }; dwx_geom *s = run(h.face, 2048.0f, g, NULL, NULL, 2, FALSE, TRUE); float x, y;
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == -1200 + 100);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == -1200 - 1000 + 100);
    dwx_release(s); dwx_font_close(&h);
}
TEST(rtl_shapes_are_not_mirrored) {
    FO_LAB(h, 0);
    /* RTL changes where glyphs are placed, not how they are drawn: relative to its origin the outline is identical */
    UINT16 g = TTF_GID_ROUND; dwx_geom *l = run(h.face, 256.0f, &g, NULL, NULL, 1, FALSE, FALSE), *r = run(h.face, 256.0f, &g, NULL, NULL, 1, FALSE, TRUE);
    int i; CHECK(l->npts == r->npts && l->npts == 13);
    for (i = 0; i < l->npts && i < r->npts; i++) { CHECK_NEAR(r->pts[i].x - l->pts[i].x, -125.0, 1e-3); CHECK_NEAR(r->pts[i].y, l->pts[i].y, 1e-5); }
    dwx_release(l); dwx_release(r); dwx_font_close(&h);
}
TEST(rtl_pen_equals_negated_ltr_sum) {
    FO_LAB(h, 0);
    /* with advances == font advances, glyph i of an RTL run sits at -(sum of advances 0..i) */
    UINT16 g[4] = { 2, FO_G_NARROW, 0, 3 }; double em = 2048.0, sum = 0; int i; dwx_geom *s = run(h.face, 2048.0f, g, NULL, NULL, 4, FALSE, TRUE); float x, y;
    for (i = 0; i < 4; i++) { sum += fo_glyphs[g[i]].adv * em / 2048; CHECK(fo_begin_pt(s, i + 1, &x, &y)); CHECK_NEAR(x, -sum + (g[i] == FO_G_NARROW ? 50 : g[i] == 0 ? 100 : 100), 1e-3); }
    dwx_release(s); dwx_font_close(&h);
}
TEST(rtl_single_glyph) {
    FO_NEED_FACE();
    UINT16 g = 64; dwx_geom *s = run(h.face, 1024.0f, &g, NULL, NULL, 1, FALSE, TRUE); float x, y;
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == -500 + 50 && y == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(rtl_negative_advance_moves_pen_right) {
    FO_NEED_FACE();
    UINT16 g[2] = { 2, 2 }; float adv[2] = { -200, 0 }; dwx_geom *s = run(h.face, 2048.0f, g, adv, NULL, 2, FALSE, TRUE); float x, y;
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == -900);
    CHECK(fo_begin_pt(s, 2, &x, &y) && x == 200 - 1000 + 100);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- sideways ------------------------------------------------------------------------------ */
static void sideways_rotation_relation(IDWriteFontFace *face, UINT16 gid, float em) {
    /* (x,y) of the unrotated outline -> (ox + Y, oy - X) where (X,Y) is the horizontal outline at origin (0,0) */
    dwx_geom *a = run(face, em, &gid, NULL, NULL, 1, FALSE, FALSE), *b = run(face, em, &gid, NULL, NULL, 1, TRUE, FALSE);
    CHECK(a->npts == b->npts && a->nev == b->nev);
    if (a->npts == b->npts && a->npts > 0) {
        double ox = b->pts[0].x - a->pts[0].y, oy = b->pts[0].y + a->pts[0].x; int i;
        for (i = 0; i < a->npts; i++) {
            CHECK_NEAR(b->pts[i].x, ox + a->pts[i].y, 2e-3 + em * 1e-6);
            CHECK_NEAR(b->pts[i].y, oy - a->pts[i].x, 2e-3 + em * 1e-6);
        }
        /* origin equals metrics: x = verticalOriginY * k, y = advanceWidth / 2 * k */
        DWRITE_GLYPH_METRICS m; memset(&m, 0, sizeof m);
        CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(face, &gid, 1, &m, TRUE), S_OK);
        CHECK_NEAR(ox, m.verticalOriginY * (double)em / 2048, 2e-3 + em * 1e-6);
        CHECK_NEAR(oy, m.advanceWidth * 0.5 * (double)em / 2048, 2e-3 + em * 1e-6);
    }
    CHECK(a->nlines == b->nlines && a->nbeziers == b->nbeziers && a->nfigures_begun == b->nfigures_begun);
    dwx_release(a); dwx_release(b);
}
TEST(sideways_rotation_box) { FO_LAB(h, 0); sideways_rotation_relation(h.face, 2, 2048.0f); sideways_rotation_relation(h.face, 2, 100.0f); dwx_font_close(&h); }
TEST(sideways_rotation_triangle) { FO_LAB(h, 0); sideways_rotation_relation(h.face, TTF_GID_TRIANGLE, 500.0f); dwx_font_close(&h); }
TEST(sideways_rotation_round) { FO_LAB(h, 0); sideways_rotation_relation(h.face, TTF_GID_ROUND, 333.0f); dwx_font_close(&h); }
TEST(sideways_rotation_ring) { FO_LAB(h, 0); sideways_rotation_relation(h.face, TTF_GID_RING, 64.0f); dwx_font_close(&h); }
TEST(sideways_rotation_composite) { FO_LAB(h, 0); sideways_rotation_relation(h.face, TTF_GID_COMPOSITE, 1024.0f); dwx_font_close(&h); }
TEST(sideways_rotation_offcurve) { FO_LAB(h, 0); sideways_rotation_relation(h.face, FO_G_ALLOFF4, 2048.0f); sideways_rotation_relation(h.face, FO_G_OFFSTART, 99.0f); dwx_font_close(&h); }
TEST(sideways_rotation_vertical_font) {
    FO_LABV(h, 0);
    sideways_rotation_relation(h.face, 2, 2048.0f); sideways_rotation_relation(h.face, TTF_GID_ROUND, 400.0f); sideways_rotation_relation(h.face, FO_G_NARROW, 80.0f);
    dwx_font_close(&h);
}
TEST(sideways_pen_advances_by_advance_height) {
    FO_LABV(h, 0);
    UINT16 g[3] = { 2, 64, FO_G_NARROW }; DWRITE_GLYPH_METRICS m[3]; int i; float ox[3], oy[3];
    for (i = 0; i < 3; i++) { memset(&m[i], 0, sizeof m[i]); IDWriteFontFace_GetDesignGlyphMetrics(h.face, &g[i], 1, &m[i], TRUE); }
    dwx_geom *s = run(h.face, 2048.0f, g, NULL, NULL, 3, TRUE, FALSE);
    CHECK(s->nfigures_begun == 3);
    /* origin_i.x = sum_{j<i} advanceHeight_j + verticalOriginY_i; recover the origin from the first point: x' = ox - y*k, y' = oy - x*k */
    for (i = 0; i < 3; i++) {
        float bx, by; static const double fx[3] = { 100, 100, 50 }, fy[3] = { 0, 0, 0 };
        CHECK(fo_begin_pt(s, i + 1, &bx, &by)); ox[i] = bx + (float)fy[i]; oy[i] = by + (float)fx[i];
    }
    CHECK_NEAR(ox[1] - ox[0], (double)m[0].advanceHeight - (double)m[0].verticalOriginY + (double)m[1].verticalOriginY, 1e-2);
    CHECK_NEAR(ox[2] - ox[1], (double)m[1].advanceHeight - (double)m[1].verticalOriginY + (double)m[2].verticalOriginY, 1e-2);
    for (i = 0; i < 3; i++) CHECK_NEAR(oy[i], m[i].advanceWidth * 0.5, 1e-2);
    t_note("vertical lab font: advanceHeight %u %u %u (vmtx 2500 1800 700), verticalOriginY %d %d %d", m[0].advanceHeight, m[1].advanceHeight, m[2].advanceHeight, m[0].verticalOriginY, m[1].verticalOriginY, m[2].verticalOriginY);
    dwx_release(s); dwx_font_close(&h);
}
TEST(sideways_custom_advances) {
    FO_LAB(h, 0);
    UINT16 g[2] = { 2, 2 }; float adv[2] = { 123, 0 };
    dwx_geom *a = run(h.face, 1000.0f, g, adv, NULL, 2, TRUE, FALSE); float x1, y1, x2, y2;
    CHECK(fo_begin_pt(a, 1, &x1, &y1) && fo_begin_pt(a, 2, &x2, &y2));
    CHECK_NEAR(x2 - x1, 123.0, 1e-3); CHECK_NEAR(y2, y1, 1e-4);
    dwx_release(a); dwx_font_close(&h);
}
TEST(sideways_offsets) {
    FO_LAB(h, 0);
    UINT16 g = 2; DWRITE_GLYPH_OFFSET off = { 17.0f, 4.0f };
    dwx_geom *a = run(h.face, 1000.0f, &g, NULL, NULL, 1, TRUE, FALSE), *b = run(h.face, 1000.0f, &g, NULL, &off, 1, TRUE, FALSE);
    int i; CHECK(a->npts == b->npts && a->npts == 4);
    for (i = 0; i < a->npts && i < b->npts; i++) { CHECK_NEAR(b->pts[i].x - a->pts[i].x, 17.0, 1e-3); CHECK_NEAR(b->pts[i].y - a->pts[i].y, -4.0, 1e-3); }
    dwx_release(a); dwx_release(b); dwx_font_close(&h);
}
TEST(sideways_rtl) {
    FO_LAB(h, 0);
    UINT16 g[2] = { 2, 3 }; DWRITE_GLYPH_METRICS m; memset(&m, 0, sizeof m);
    IDWriteFontFace_GetDesignGlyphMetrics(h.face, g, 1, &m, TRUE);
    fo_exp e; dwx_geom *s = run(h.face, 800.0f, g, NULL, NULL, 2, TRUE, TRUE);
    fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, 800.0, g, NULL, NULL, 2, 1, 1, 0);
    FO_COMPARE(s, &e, 5e-3, "sideways rtl");
    /* the pen moves to negative x */
    float x1, y1, x2, y2; CHECK(fo_begin_pt(s, 1, &x1, &y1) && fo_begin_pt(s, 2, &x2, &y2)); CHECK(x2 < x1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(sideways_empty_glyphs_still_advance) {
    FO_LAB(h, 0);
    UINT16 g[3] = { 2, 1, 2 }; fo_exp e; dwx_geom *s = run(h.face, 640.0f, g, NULL, NULL, 3, TRUE, FALSE);
    fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, 640.0, g, NULL, NULL, 3, 1, 0, 0);
    FO_COMPARE(s, &e, 5e-3, "sideways with space");
    CHECK(s->nfigures_begun == 2);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- full combination matrix, model driven ----------------------------------------------------- */
static void combos(int vertical) {
    dwx_font h; static const float ems[] = { 2048.0f, 300.0f, 17.5f }; int ei;
    UINT16 seqA[4] = { 2, 64, 65, 68 }, seqB[5] = { 0, FO_G_OFFSTART, 1, FO_G_MULTI, FO_G_NARROW }, seqC[3] = { FO_G_COMP_NESTED, FO_G_PENTA, TTF_GID_COMPOSITE };
    const UINT16 *seqs[3] = { seqA, seqB, seqC }; const UINT32 nseq[3] = { 4, 5, 3 };
    if (vertical) { fo_lab_vertical = 1; }
    if (FAILED(fo_open_lab(&h, 0)) || !h.face) { fo_lab_vertical = 0; dwx_font_close(&h); t_skip("lab font unavailable"); return; }
    fo_lab_vertical = 0;
    for (ei = 0; ei < 3; ei++) {
        int sq, sw, rtl, ha, ho;
        for (sq = 0; sq < 3; sq++) for (sw = 0; sw < 2; sw++) for (rtl = 0; rtl < 2; rtl++) for (ha = 0; ha < 2; ha++) for (ho = 0; ho < 2; ho++) {
            float adv[5] = { 41.5f, 7.0f, 120.25f, 0.0f, 63.0f }; DWRITE_GLYPH_OFFSET off[5] = { { 3.5f, 2.0f }, { -4.0f, 1.0f }, { 0, -6.5f }, { 9, 9 }, { -1, -2 } };
            UINT32 n = nseq[sq]; fo_exp e; dwx_geom *s = dwx_geom_new(); char tag[96];
            snprintf(tag, sizeof tag, "seq%d em%g sw%d rtl%d adv%d off%d v%d", sq, ems[ei], sw, rtl, ha, ho, vertical);
            CHECK_HR(fo_call(h.face, ems[ei], seqs[sq], ha ? adv : NULL, ho ? off : NULL, n, sw, rtl, s), S_OK);
            fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, ems[ei], seqs[sq], ha ? adv : NULL, ho ? off : NULL, n, sw, rtl, 0);
            FO_COMPARE(s, &e, 5e-3 + ems[ei] * 2e-6, tag);
            FO_INVARIANTS(s, 1);
            dwx_release(s);
        }
    }
    dwx_font_close(&h);
}
TEST(combos_matrix) { combos(0); }
TEST(combos_matrix_vertical_font) { combos(1); }

TEST(origin_counts_independent_of_flags) {
    FO_LAB(h, 0);
    /* flags only move glyphs: figure/segment counts are identical in all four orientations */
    UINT16 g[4] = { 2, FO_G_ALLOFF3ODD, 68, FO_G_MULTI }; int sw, rtl; int nf = -1, nl = -1, nb = -1;
    for (sw = 0; sw < 2; sw++) for (rtl = 0; rtl < 2; rtl++) {
        dwx_geom *s = run(h.face, 100.0f, g, NULL, NULL, 4, sw, rtl);
        if (nf < 0) { nf = s->nfigures_begun; nl = s->nlines; nb = s->nbeziers; }
        CHECK(s->nfigures_begun == nf && s->nlines == nl && s->nbeziers == nb);
        dwx_release(s);
    }
    dwx_font_close(&h);
}
