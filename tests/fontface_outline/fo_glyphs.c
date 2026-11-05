/* Exact outlines of known glyphs of the synthetic font through IDWriteFontFace::GetGlyphRunOutline.
 * Coordinates: design units (UPM 2048) * emSize/2048, y negated (y-down), glyph origin at (0,0). */
#include "fo.h"

typedef struct { int kind; int n; float p[6]; } ex_ev;
#define L_(x, y)            { DWX_G_LINES, 1, { x, y } }
#define B_(a, b, c, d, e, f) { DWX_G_BEZIERS, 1, { a, b, c, d, e, f } }
#define BEG_(x, y)          { DWX_G_BEGIN, 1, { x, y } }
#define END_               { DWX_G_END, 0, { 0 } }

/* Compare a stream against a literal event list (after the leading SetFillMode). */
static void ex_check_(const dwx_geom *g, const ex_ev *e, int n, double tol, const char *file, int line, const char *what) {
    fo_exp m; int i;
    m.n = 0; fo_push(&m, DWX_G_FILL, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0; i < n; i++) {
        fo_push(&m, e[i].kind, e[i].n, e[i].p[0], e[i].p[1], e[i].p[2], e[i].p[3], e[i].p[4], e[i].p[5]);
    }
    fo_compare_(g, &m, tol, file, line, what);
}
#define EX_CHECK(g, arr, tol, what) ex_check_((g), (arr), (int)(sizeof(arr) / sizeof((arr)[0])), (tol), __FILE__, __LINE__, (what))

static dwx_geom *one(IDWriteFontFace *face, float em, UINT16 gid) {
    dwx_geom *s = dwx_geom_new();
    CHECK_HR(fo_call(face, em, &gid, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    return s;
}

/* ---- literal expectations, em == unitsPerEm (scale exactly 1.0, results are exact floats) ------------------ */
TEST(glyph_box_exact_2048) {
    FO_NEED_FACE();
    dwx_geom *s = one(h.face, 2048.0f, 2);       /* 'A': (100,0) (100,1400) (900,1400) (900,0) */
    static const ex_ev e[] = { BEG_(100, 0), L_(100, -1400), L_(900, -1400), L_(900, 0), END_ };
    EX_CHECK(s, e, 0.0, "A@2048");
    FO_INVARIANTS(s, 1);
    CHECK(s->nfigures_begun == 1 && s->nlines == 3 && s->nbeziers == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_box_exact_all_letters_and_digits) {
    FO_NEED_FACE();
    static const ex_ev e[] = { BEG_(100, 0), L_(100, -1400), L_(900, -1400), L_(900, 0), END_ };
    UINT16 gid;
    for (gid = 2; gid < 64; gid++) {      /* A-Z, a-z, 0-9 share one outline */
        dwx_geom *s = one(h.face, 2048.0f, gid);
        EX_CHECK(s, e, 0.0, "letter box");
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(glyph_box_exact_half_scale) {
    FO_NEED_FACE();
    dwx_geom *s = one(h.face, 1024.0f, 2);
    static const ex_ev e[] = { BEG_(50, 0), L_(50, -700), L_(450, -700), L_(450, 0), END_ };
    EX_CHECK(s, e, 0.0, "A@1024");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_box_exact_double_scale) {
    FO_NEED_FACE();
    dwx_geom *s = one(h.face, 4096.0f, 2);
    static const ex_ev e[] = { BEG_(200, 0), L_(200, -2800), L_(1800, -2800), L_(1800, 0), END_ };
    EX_CHECK(s, e, 0.0, "A@4096");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_box_em_100) {
    FO_NEED_FACE();
    dwx_geom *s = one(h.face, 100.0f, 2);
    double k = 100.0 / 2048.0;
    static const ex_ev e0[] = { BEG_(0, 0), L_(0, 0), L_(0, 0), L_(0, 0), END_ };
    ex_ev e[5]; memcpy(e, e0, sizeof e);
    e[0].p[0] = (float)(100 * k);
    e[1].p[0] = (float)(100 * k); e[1].p[1] = (float)(-1400 * k);
    e[2].p[0] = (float)(900 * k); e[2].p[1] = (float)(-1400 * k);
    e[3].p[0] = (float)(900 * k);
    EX_CHECK(s, e, 1e-4, "A@100");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_triangle_exact) {
    FO_NEED_FACE();
    dwx_geom *s = one(h.face, 2048.0f, TTF_GID_TRIANGLE);
    static const ex_ev e[] = { BEG_(100, 0), L_(500, -1400), L_(900, 0), END_ };
    EX_CHECK(s, e, 0.0, "triangle");
    CHECK(s->nlines == 2);       /* two AddLines; the closing edge comes from EndFigure(CLOSED) */
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_round_exact_2048) {
    FO_NEED_FACE();
    /* points: (500,0) on, (100,0) off, (100,700) on, (100,1400) off, (500,1400) on, (900,1400) off, (900,700) on, (900,0) off
     * each quadratic Q(p0,q,p1) becomes the cubic (p0 + 2/3 (q-p0), p1 + 2/3 (q-p1), p1); the trailing off-curve point
     * curves back to the start point. */
    dwx_geom *s = one(h.face, 2048.0f, TTF_GID_ROUND);
    const double t = 2.0 / 3.0;
    static const ex_ev e0[] = { BEG_(500, 0), B_(0,0,0,0,0,0), B_(0,0,0,0,0,0), B_(0,0,0,0,0,0), B_(0,0,0,0,0,0), END_ };
    ex_ev e[6]; memcpy(e, e0, sizeof e);
    /* 1: (500,0) -q(100,0)-> (100,700) */
    e[1].p[0] = (float)(500 + t * (100 - 500)); e[1].p[1] = 0; e[1].p[2] = 100; e[1].p[3] = (float)(-700 + t * (0 + 700)); e[1].p[4] = 100; e[1].p[5] = -700;
    /* 2: (100,700) -q(100,1400)-> (500,1400) */
    e[2].p[0] = 100; e[2].p[1] = (float)(-700 + t * (-1400 + 700)); e[2].p[2] = (float)(500 + t * (100 - 500)); e[2].p[3] = -1400; e[2].p[4] = 500; e[2].p[5] = -1400;
    /* 3: (500,1400) -q(900,1400)-> (900,700) */
    e[3].p[0] = (float)(500 + t * (900 - 500)); e[3].p[1] = -1400; e[3].p[2] = 900; e[3].p[3] = (float)(-700 + t * (-1400 + 700)); e[3].p[4] = 900; e[3].p[5] = -700;
    /* 4: (900,700) -q(900,0)-> start (500,0) */
    e[4].p[0] = 900; e[4].p[1] = (float)(-700 + t * (0 + 700)); e[4].p[2] = (float)(500 + t * (900 - 500)); e[4].p[3] = 0; e[4].p[4] = 500; e[4].p[5] = 0;
    EX_CHECK(s, e, 1e-3, "round");
    CHECK(s->nbeziers == 4 && s->nlines == 0 && s->nfigures_begun == 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_ring_two_figures_exact) {
    FO_NEED_FACE();
    dwx_geom *s = one(h.face, 2048.0f, TTF_GID_RING);
    static const ex_ev e[] = { BEG_(100, 0), L_(100, -1400), L_(900, -1400), L_(900, 0), END_,
                               BEG_(300, -300), L_(700, -300), L_(700, -1100), L_(300, -1100), END_ };
    EX_CHECK(s, e, 0.0, "ring");
    CHECK(s->nfigures_begun == 2 && s->nfigures_ended == 2);
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_composite_exact) {
    FO_NEED_FACE();
    /* composite: 'A' at (0,0) then triangle translated by (0,1400) */
    dwx_geom *s = one(h.face, 2048.0f, TTF_GID_COMPOSITE);
    static const ex_ev e[] = { BEG_(100, 0), L_(100, -1400), L_(900, -1400), L_(900, 0), END_,
                               BEG_(100, -1400), L_(500, -2800), L_(900, -1400), END_ };
    EX_CHECK(s, e, 0.0, "composite");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_space_and_empty_no_figures) {
    FO_NEED_FACE();
    UINT16 gids[2] = { 1, TTF_GID_EMPTY500 }; int i;
    for (i = 0; i < 2; i++) { dwx_geom *s = one(h.face, 2048.0f, gids[i]); CHECK(s->nev == 1); CHECK(s->npts == 0); dwx_release(s); }
    dwx_font_close(&h);
}

/* ---- lab glyphs, literal ---------------------------------------------------------------------------- */
TEST(glyph_offcurve_start_ends_on_last_point) {
    FO_LAB(h, 0);
    /* (500,0)off (900,700)on (500,1400)off (100,700)on: the figure starts at the last on-curve point */
    dwx_geom *s = one(h.face, 2048.0f, FO_G_OFFSTART);
    CHECK(s->nev >= 3);
    if (s->nev >= 3) { CHECK(s->ev[1].kind == DWX_G_BEGIN); CHECK_FLOAT_EQ(s->ev[1].pt.x, 100); CHECK_FLOAT_EQ(s->ev[1].pt.y, -700); }
    CHECK(s->nbeziers == 2 && s->nlines == 0);
    CHECK(s->npts == 1 + 6);
    if (s->npts == 7) {
        CHECK_FLOAT_EQ(s->pts[3].x, 900); CHECK_FLOAT_EQ(s->pts[3].y, -700);   /* end of the first cubic */
        CHECK_FLOAT_EQ(s->pts[6].x, 100); CHECK_FLOAT_EQ(s->pts[6].y, -700);   /* back to the start point */
    }
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_all_offcurve_midpoints) {
    FO_LAB(h, 0);
    /* four off-curve points (100,0) (100,1400) (900,1400) (900,0): implied on-curve points are the midpoints; the figure
     * starts at the midpoint of first and last point, and has one cubic per off-curve point */
    dwx_geom *s = one(h.face, 2048.0f, FO_G_ALLOFF4);
    CHECK(s->nbeziers == 4 && s->nlines == 0 && s->nfigures_begun == 1);
    CHECK(s->npts == 13);
    if (s->npts == 13) {
        CHECK_FLOAT_EQ(s->pts[0].x, 500); CHECK_FLOAT_EQ(s->pts[0].y, 0);
        CHECK_FLOAT_EQ(s->pts[3].x, 100); CHECK_FLOAT_EQ(s->pts[3].y, -700);
        CHECK_FLOAT_EQ(s->pts[6].x, 500); CHECK_FLOAT_EQ(s->pts[6].y, -1400);
        CHECK_FLOAT_EQ(s->pts[9].x, 900); CHECK_FLOAT_EQ(s->pts[9].y, -700);
        CHECK_FLOAT_EQ(s->pts[12].x, 500); CHECK_FLOAT_EQ(s->pts[12].y, 0);
        /* first cubic: from (500,0) with quad control (100,0) to (100,-700) */
        CHECK_NEAR(s->pts[1].x, 500 - 2.0 / 3 * 400, 1e-3); CHECK_NEAR(s->pts[1].y, 0, 1e-6);
        CHECK_NEAR(s->pts[2].x, 100, 1e-3);                 CHECK_NEAR(s->pts[2].y, -700 + 2.0 / 3 * 700, 1e-3);
    }
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_all_offcurve_odd_midpoints_half_units) {
    FO_LAB(h, 0);
    /* (101,3) (1001,7) (555,1399): start = mid(first,last) = (328,701); midpoints have exact half-unit coordinates */
    dwx_geom *s = one(h.face, 2048.0f, FO_G_ALLOFF3ODD);
    CHECK(s->nbeziers == 3 && s->nfigures_begun == 1);
    CHECK(s->npts == 10);
    if (s->npts == 10) {
        CHECK_FLOAT_EQ(s->pts[0].x, 328);   CHECK_FLOAT_EQ(s->pts[0].y, -701);
        CHECK_FLOAT_EQ(s->pts[3].x, 551);   CHECK_FLOAT_EQ(s->pts[3].y, -5);       /* mid((101,3),(1001,7)) */
        CHECK_FLOAT_EQ(s->pts[6].x, 778);   CHECK_FLOAT_EQ(s->pts[6].y, -703);     /* mid((1001,7),(555,1399)) */
        CHECK_FLOAT_EQ(s->pts[9].x, 328);   CHECK_FLOAT_EQ(s->pts[9].y, -701);
    }
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_consecutive_offcurve_in_middle) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_MIDOFF);   /* on(100,0) off(100,1400) off(900,1400) on(900,0) */
    CHECK(s->nbeziers == 2 && s->nlines == 0);
    CHECK(s->npts == 7);
    if (s->npts == 7) {
        CHECK_FLOAT_EQ(s->pts[0].x, 100); CHECK_FLOAT_EQ(s->pts[0].y, 0);
        CHECK_FLOAT_EQ(s->pts[3].x, 500); CHECK_FLOAT_EQ(s->pts[3].y, -1400);   /* implied on-curve point */
        CHECK_FLOAT_EQ(s->pts[6].x, 900); CHECK_FLOAT_EQ(s->pts[6].y, 0);
    }
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_trailing_offcurve_curves_to_start) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_LASTOFF);     /* on(100,0) on(500,1400) off(900,0) */
    static const ex_ev e[] = { BEG_(100, 0), L_(500, -1400),
                               B_(500 + 400 * 2.0f / 3, -1400 + 1400 * 2.0f / 3, 100 + 800 * 2.0f / 3, 0, 100, 0), END_ };
    EX_CHECK(s, e, 1e-3, "lastoff");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_two_point_contour_is_a_figure) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_TWOPT);
    static const ex_ev e[] = { BEG_(100, 0), L_(900, -700), END_ };
    EX_CHECK(s, e, 0.0, "two points");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_one_point_contour_skipped) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_ONEPT_BOX);    /* contour 0 has a single point, contour 1 is a box */
    static const ex_ev e[] = { BEG_(100, 0), L_(100, -1400), L_(900, -1400), L_(900, 0), END_ };
    EX_CHECK(s, e, 0.0, "onept+box");
    CHECK(s->nfigures_begun == 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_only_one_point_contours_empty) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_EMPTYCONT);
    CHECK(s->nev == 1 && s->ev[0].kind == DWX_G_FILL);
    CHECK(s->nfigures_begun == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_pentagon_lines_only) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_PENTA);
    static const ex_ev e[] = { BEG_(500, 0), L_(900, -500), L_(700, -1300), L_(300, -1300), L_(100, -500), END_ };
    EX_CHECK(s, e, 0.0, "pentagon");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_negative_coordinates) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_NEG);       /* (-300,-200) (-300,600) (400,600) (400,-200) */
    static const ex_ev e[] = { BEG_(-300, 200), L_(-300, -600), L_(400, -600), L_(400, 200), END_ };
    EX_CHECK(s, e, 0.0, "negative");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_big_box) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_BIG);
    static const ex_ev e[] = { BEG_(0, 0), L_(0, -2000), L_(2000, -2000), L_(2000, 0), END_ };
    EX_CHECK(s, e, 0.0, "big");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_multi_contour_mixed) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_MULTI);
    /* contour 0: box; contour 1: three off-curve points (start (400,650) = mid of first/last); contour 2: two points */
    CHECK(s->nfigures_begun == 3 && s->nfigures_ended == 3);
    CHECK(s->nlines == 3 + 0 + 1);
    CHECK(s->nbeziers == 3);
    int b1 = -1, i, nb = 0;
    for (i = 0; i < s->nev; i++) if (s->ev[i].kind == DWX_G_BEGIN && ++nb == 2) b1 = i;
    CHECK(b1 > 0);
    if (b1 > 0) { CHECK_FLOAT_EQ(s->ev[b1].pt.x, 400); CHECK_FLOAT_EQ(s->ev[b1].pt.y, -650); }
    nb = 0; for (i = 0; i < s->nev; i++) if (s->ev[i].kind == DWX_G_BEGIN && ++nb == 3) b1 = i;
    if (b1 > 0) { CHECK_FLOAT_EQ(s->ev[b1].pt.x, 200); CHECK_FLOAT_EQ(s->ev[b1].pt.y, -100); }
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_composite_scaled_components) {
    FO_LAB(h, 0);
    /* components scaled by 0.5 (F2Dot14 0x2000): box -> (50,0)-(450,700); triangle -> *0.5 then +(1000,500) */
    dwx_geom *s = one(h.face, 2048.0f, FO_G_COMP_SCALE);
    static const ex_ev e[] = { BEG_(50, 0), L_(50, -700), L_(450, -700), L_(450, 0), END_,
                               BEG_(1050, -500), L_(1250, -1200), L_(1450, -500), END_ };
    EX_CHECK(s, e, 0.05, "comp scale 0.5");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_composite_negative_scale) {
    FO_LAB(h, 0);
    /* scale -1.0 (0xC000 pattern of F2Dot14): point reflection, then translate by (1000,1400) */
    dwx_geom *s = one(h.face, 2048.0f, FO_G_COMP_NEG);
    static const ex_ev e[] = { BEG_(900, -1400), L_(500, 0), L_(100, -1400), END_ };
    EX_CHECK(s, e, 0.05, "comp scale -1");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_composite_nested) {
    FO_LAB(h, 0);
    dwx_geom *s = one(h.face, 2048.0f, FO_G_COMP_NESTED);   /* composite(box, triangle@1400) + box@(1000,0) */
    static const ex_ev e[] = { BEG_(100, 0), L_(100, -1400), L_(900, -1400), L_(900, 0), END_,
                               BEG_(100, -1400), L_(500, -2800), L_(900, -1400), END_,
                               BEG_(1100, 0), L_(1100, -1400), L_(1900, -1400), L_(1900, 0), END_ };
    EX_CHECK(s, e, 0.0, "nested composite");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_composite_unscaled_offset_default) {
    FO_LAB(h, 0);
    /* neither SCALED_ nor UNSCALED_COMPONENT_OFFSET set: the offset (200,400) is applied unscaled after the 0.5 scale */
    dwx_geom *s = one(h.face, 2048.0f, FO_G_COMP_OFF);
    static const ex_ev e[] = { BEG_(250, -400), L_(450, -1100), L_(650, -400), END_ };
    EX_CHECK(s, e, 0.05, "comp offset unscaled");
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_composite_scaled_offset_flag_note) {
    FO_LAB(h, 0);
    /* SCALED_COMPONENT_OFFSET (0x0800): offset (200,400) scaled by 0.5 -> (100,200); report what the rasterizer did */
    dwx_geom *s = one(h.face, 2048.0f, FO_G_COMP_SOFF);
    CHECK(s->nfigures_begun == 1 && s->nlines == 2);
    if (s->npts == 3) {
        t_note("SCALED_COMPONENT_OFFSET: first point (%g,%g); scaled-offset model expects (150,-200), unscaled (250,-400)", s->pts[0].x, s->pts[0].y);
        CHECK_NEAR(s->pts[1].x - s->pts[0].x, 200, 0.05);       /* triangle shape itself is unaffected */
        CHECK_NEAR(s->pts[1].y - s->pts[0].y, -700, 0.05);
    }
    dwx_release(s); dwx_font_close(&h);
}
TEST(glyph_lsb_differs_from_xmin_note) {
    FO_LAB(h, 0);
    /* hmtx lsb = 0 while xMin = -300: TrueType rasterizers shift the outline so that xMin == lsb (phantom point 1) */
    dwx_geom *s = one(h.face, 2048.0f, FO_G_LSBSHIFT);
    float x0, y0, x1, y1; fo_bounds(s, &x0, &y0, &x1, &y1);
    t_note("lsb!=xMin glyph x range [%g,%g] (unshifted -300..400, shifted 0..700)", x0, x1);
    CHECK_NEAR(x1 - x0, 700, 0.05);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- model-driven sweeps ---------------------------------------------------------------------------- */
static const float sweep_ems[] = { 2048.0f, 1024.0f, 4096.0f, 512.0f, 100.0f, 72.0f, 33.3f, 16.0f, 12.5f, 7.25f, 1.0f, 3000.0f };
static void glyph_model(UINT16 gid, double tol_scale) {
    FO_LAB(h, 0);
    size_t i; int j;
    for (i = 0; i < sizeof sweep_ems / sizeof sweep_ems[0]; i++) {
        double em = sweep_ems[i]; fo_exp e; dwx_geom *s = dwx_geom_new(); char tag[64];
        double tol = 1e-4 + em * 1e-6 + tol_scale * em / 2048.0;
        snprintf(tag, sizeof tag, "gid %u em %g", gid, em);
        CHECK_HR(fo_call(h.face, (float)em, &gid, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
        fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, em, &gid, NULL, NULL, 1, 0, 0, 0);
        FO_COMPARE(s, &e, tol, tag);
        FO_INVARIANTS(s, 1);
        dwx_release(s);
    }
    /* the same glyph twice in a run: second copy is displaced by exactly its own advance */
    for (j = 0; j < 3; j++) {
        UINT16 g2[2] = { gid, gid }; double em = sweep_ems[j]; fo_exp e; dwx_geom *s = dwx_geom_new();
        CHECK_HR(fo_call(h.face, (float)em, g2, NULL, NULL, 2, FALSE, FALSE, s), S_OK);
        fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, em, g2, NULL, NULL, 2, 0, 0, 0);
        FO_COMPARE(s, &e, 1e-4 + em * 1e-6 + tol_scale * em / 2048.0, "pair");
        dwx_release(s);
    }
    dwx_font_close(&h);
}
#define GT(nm, gid, tol) TEST(model_##nm) { glyph_model((gid), (tol)); }
GT(notdef, 0, 0)       GT(space, 1, 0)        GT(letter_A, 2, 0)    GT(letter_Z, 27, 0)   GT(letter_a, 28, 0)
GT(digit_0, 54, 0)     GT(triangle, 64, 0)    GT(round, 65, 0)      GT(composite, 66, 0)  GT(empty500, 67, 0)
GT(ring, 68, 0)        GT(offstart, FO_G_OFFSTART, 0)  GT(alloff4, FO_G_ALLOFF4, 0)  GT(alloff3odd, FO_G_ALLOFF3ODD, 0)
GT(midoff, FO_G_MIDOFF, 0)  GT(twopt, FO_G_TWOPT, 0)  GT(onept_box, FO_G_ONEPT_BOX, 0)  GT(penta, FO_G_PENTA, 0)
GT(neg, FO_G_NEG, 0)   GT(multi, FO_G_MULTI, 0)  GT(lastoff, FO_G_LASTOFF, 0)  GT(comp_scale, FO_G_COMP_SCALE, 0.02)
GT(comp_neg, FO_G_COMP_NEG, 0.02)  GT(comp_nested, FO_G_COMP_NESTED, 0)  GT(comp_off, FO_G_COMP_OFF, 0.02)
GT(big, FO_G_BIG, 0)   GT(narrow, FO_G_NARROW, 0)  GT(emptycont, FO_G_EMPTYCONT, 0)

TEST(model_all_glyphs_sweep) {
    FO_LAB(h, 0);
    static const float ems[] = { 2048.0f, 777.7f, 20.0f };
    UINT16 gid; size_t i;
    for (i = 0; i < 3; i++) for (gid = 0; gid < FO_G_COUNT; gid++) {
        if (gid == FO_G_LSBSHIFT || gid == FO_G_COMP_SOFF) continue;    /* rasterizer-dependent, see the note tests */
        double em = ems[i]; fo_exp e; dwx_geom *s = dwx_geom_new(); char tag[64];
        snprintf(tag, sizeof tag, "sweep gid %u em %g", gid, em);
        CHECK_HR(fo_call(h.face, (float)em, &gid, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
        fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, em, &gid, NULL, NULL, 1, 0, 0, 0);
        FO_COMPARE(s, &e, 1e-4 + em * 1e-6 + ((gid >= FO_G_COMP_SCALE && gid <= FO_G_COMP_OFF) ? 0.02 * em / 2048.0 : 0), tag);
        FO_INVARIANTS(s, 1);
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(model_whole_font_one_run) {
    FO_LAB(h, 0);
    UINT16 g[FO_G_COUNT]; int i, n = 0; fo_exp e; dwx_geom *s = dwx_geom_new();
    for (i = 0; i < FO_G_COUNT; i++) if (i != FO_G_LSBSHIFT && i != FO_G_COMP_SOFF) g[n++] = (UINT16)i;
    CHECK_HR(fo_call(h.face, 24.0f, g, NULL, NULL, n, FALSE, FALSE, s), S_OK);
    fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, 24.0, g, NULL, NULL, n, 0, 0, 0);
    FO_COMPARE(s, &e, 0.01, "whole font");
    FO_INVARIANTS(s, 1);
    CHECK(s->fill_calls == 1);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- scaling and orientation ------------------------------------------------------------------------ */
TEST(scale_linear_in_emsize) {
    FO_LAB(h, 0);
    UINT16 gids[] = { 2, 64, 65, 68, 66, FO_G_OFFSTART, FO_G_ALLOFF4, FO_G_MULTI, FO_G_NEG, FO_G_LASTOFF }; size_t gi; int i;
    for (gi = 0; gi < sizeof gids / sizeof gids[0]; gi++) {
        dwx_geom *a = one(h.face, 100.0f, gids[gi]), *b = one(h.face, 300.0f, gids[gi]), *c = one(h.face, 25.0f, gids[gi]);
        CHECK(a->npts == b->npts && a->npts == c->npts && a->nev == b->nev);
        for (i = 0; i < a->npts && i < b->npts && i < c->npts; i++) {
            CHECK_NEAR(b->pts[i].x, 3.0 * a->pts[i].x, 2e-3); CHECK_NEAR(b->pts[i].y, 3.0 * a->pts[i].y, 2e-3);
            CHECK_NEAR(c->pts[i].x, 0.25 * a->pts[i].x, 2e-3); CHECK_NEAR(c->pts[i].y, 0.25 * a->pts[i].y, 2e-3);
        }
        dwx_release(a); dwx_release(b); dwx_release(c);
    }
    dwx_font_close(&h);
}
TEST(scale_em_equals_upm_is_design_units) {
    FO_LAB(h, 0);
    UINT16 gids[] = { 2, 0, 64, 68, FO_G_PENTA }; size_t gi;
    for (gi = 0; gi < sizeof gids / sizeof gids[0]; gi++) {
        dwx_geom *s = one(h.face, 2048.0f, gids[gi]); int i;
        for (i = 0; i < s->npts; i++) {   /* every coordinate of a line-only glyph is an integer number of design units */
            CHECK(s->pts[i].x == floorf(s->pts[i].x)); CHECK(s->pts[i].y == floorf(s->pts[i].y));
        }
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(orientation_y_down_baseline_at_zero) {
    FO_NEED_FACE();
    /* glyph origin = baseline at y == 0; ink above the baseline has negative y */
    dwx_geom *s = one(h.face, 2048.0f, 2); float x0, y0, x1, y1;
    fo_bounds(s, &x0, &y0, &x1, &y1);
    CHECK_FLOAT_EQ(y1, 0); CHECK_FLOAT_EQ(y0, -1400); CHECK_FLOAT_EQ(x0, 100); CHECK_FLOAT_EQ(x1, 900);
    dwx_release(s); dwx_font_close(&h);
}
TEST(orientation_bbox_scales_with_design_box) {
    FO_NEED_FACE();
    static const float ems[] = { 8, 24, 96, 640 }; size_t i;
    for (i = 0; i < 4; i++) {
        dwx_geom *s = one(h.face, ems[i], 2); float x0, y0, x1, y1; double k = ems[i] / 2048.0;
        fo_bounds(s, &x0, &y0, &x1, &y1);
        CHECK_NEAR(x0, 100 * k, 1e-4 * ems[i] + 1e-4); CHECK_NEAR(x1, 900 * k, 1e-4 * ems[i] + 1e-4);
        CHECK_NEAR(y0, -1400 * k, 1e-4 * ems[i] + 1e-4); CHECK_NEAR(y1, 0, 1e-6);
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(orientation_winding_matches_font) {
    FO_NEED_FACE();
    /* the 'A' box is clockwise in the font (y-up); after the y flip it is counter-clockwise in y-down coordinates:
     * signed area (shoelace) of the control polygon is therefore negative-of-font-orientation, and the ring's hole has the
     * opposite sign from its outer contour. */
    dwx_geom *s = one(h.face, 2048.0f, TTF_GID_RING); int i, b[2], nb = 0; double area[2] = { 0, 0 };
    CHECK(s->nfigures_begun == 2);
    /* first figure points 0..3, second 4..7 */
    for (i = 0; i < 8 && s->npts >= 8; i++) {
        int f = i / 4, j = (i % 4), nx = f * 4 + ((j + 1) % 4);
        area[f] += (double)s->pts[f * 4 + j].x * s->pts[nx].y - (double)s->pts[nx].x * s->pts[f * 4 + j].y;
    }
    (void)b; (void)nb;
    CHECK(area[0] * area[1] < 0);        /* opposite orientation: outer cw, hole ccw (a hole under non-zero winding) */
    CHECK(fabs(area[0] / 2) == 800.0 * 1400.0);
    CHECK(fabs(area[1] / 2) == 400.0 * 800.0);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- the glyph table via GetGlyphIndices ---------------------------------------------------------- */
TEST(index_via_cmap_matches_direct_gid) {
    FO_NEED_FACE();
    UINT32 cps[4] = { 'A', 'z', '7', ' ' }; UINT16 gi[4] = { 0 }; int i;
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(h.face, cps, 4, gi), S_OK);
    for (i = 0; i < 4; i++) CHECK(gi[i] == ttf_default_gid(cps[i]));
    {
        dwx_geom *a = dwx_geom_new(), *b = dwx_geom_new(); UINT16 d[4] = { 2, 53, 61, 1 };   /* A, z, 7, space by gid */
        CHECK_HR(fo_call(h.face, 80.0f, gi, NULL, NULL, 4, FALSE, FALSE, a), S_OK);
        CHECK_HR(fo_call(h.face, 80.0f, d, NULL, NULL, 4, FALSE, FALSE, b), S_OK);
        CHECK(a->npts == b->npts && a->npts > 0 && !memcmp(a->pts, b->pts, sizeof(D2D1_POINT_2F) * a->npts));
        dwx_release(a); dwx_release(b);
    }
    dwx_font_close(&h);
}
