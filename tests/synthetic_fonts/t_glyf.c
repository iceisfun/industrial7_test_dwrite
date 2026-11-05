/* glyf / loca parsing, observed through IDWriteFontFace::GetGlyphRunOutline and GetDesignGlyphMetrics.
 * The test fonts replace the generated glyf and loca tables with hand-encoded ones (compact flags, repeat flags, short/long coordinates,
 * instructions, every composite flag combination) while everything else stays the default synthetic font. Outlines are read at
 * emSize == unitsPerEm (2048) so one design unit is one device unit; bounds are compared with a small tolerance. */
#include "sf.h"

typedef struct { short x, y; int on; } P;
typedef struct { sf_buf g[TTF_DEF_NGLYPHS]; } gset;
static void gs_init(gset *s) { memset(s, 0, sizeof *s); }
static void gs_free(gset *s) { int i; for (i = 0; i < TTF_DEF_NGLYPHS; i++) sf_buf_free(&s->g[i]); }
static void pad4(sf_buf *o) { while (o->n & 3) sf_b8(o, 0); }

/* simple glyph encoder */
typedef struct { int compact; const uint8_t *instr; int ninstr; int overlap; int bbox_override; short bb[4]; } senc;
static void enc_simple(sf_buf *o, const P *pt, int n, const int *ends, int nc, const senc *e) {
    int i, minx = 32767, miny = 32767, maxx = -32768, maxy = -32768; uint8_t *fl = (uint8_t *)calloc(n ? n : 1, 1);
    for (i = 0; i < n; i++) { if (pt[i].x < minx) minx = pt[i].x; if (pt[i].x > maxx) maxx = pt[i].x; if (pt[i].y < miny) miny = pt[i].y; if (pt[i].y > maxy) maxy = pt[i].y; }
    if (!n) minx = miny = maxx = maxy = 0;
    if (e && e->bbox_override) { minx = e->bb[0]; miny = e->bb[1]; maxx = e->bb[2]; maxy = e->bb[3]; }
    sf_b16(o, nc); sf_b16(o, minx); sf_b16(o, miny); sf_b16(o, maxx); sf_b16(o, maxy);
    for (i = 0; i < nc; i++) sf_b16(o, ends[i]);
    sf_b16(o, e ? e->ninstr : 0);
    for (i = 0; e && i < e->ninstr; i++) sf_b8(o, e->instr[i]);
    {
        int px = 0, py = 0, compact = e && e->compact;
        for (i = 0; i < n; i++) {
            int dx = pt[i].x - px, dy = pt[i].y - py; unsigned f = pt[i].on ? 1 : 0;
            if (compact) {
                if (dx == 0) f |= 0x10; else if (dx > -256 && dx < 256) { f |= 0x02; if (dx > 0) f |= 0x10; }
                if (dy == 0) f |= 0x20; else if (dy > -256 && dy < 256) { f |= 0x04; if (dy > 0) f |= 0x20; }
            }
            if (i == 0 && e && e->overlap) f |= 0x40;
            fl[i] = (uint8_t)f; px = pt[i].x; py = pt[i].y;
        }
        for (i = 0; i < n; ) {
            int run = 1; while (compact && i + run < n && fl[i + run] == fl[i] && run < 256) run++;
            if (run > 1) { sf_b8(o, fl[i] | 0x08); sf_b8(o, run - 1); } else sf_b8(o, fl[i]);
            i += run;
        }
        px = 0; for (i = 0; i < n; i++) { int dx = pt[i].x - px; unsigned f = fl[i]; px = pt[i].x;
            if (compact) { if (f & 0x02) sf_b8(o, dx < 0 ? -dx : dx); else if (!(f & 0x10)) sf_b16(o, (unsigned)dx & 0xFFFF); } else sf_b16(o, (unsigned)dx & 0xFFFF); }
        py = 0; for (i = 0; i < n; i++) { int dy = pt[i].y - py; unsigned f = fl[i]; py = pt[i].y;
            if (compact) { if (f & 0x04) sf_b8(o, dy < 0 ? -dy : dy); else if (!(f & 0x20)) sf_b16(o, (unsigned)dy & 0xFFFF); } else sf_b16(o, (unsigned)dy & 0xFFFF); }
    }
    free(fl); pad4(o);
}
static const P BOX[4] = { { 100, 0, 1 }, { 100, 1400, 1 }, { 900, 1400, 1 }, { 900, 0, 1 } };
static void box(sf_buf *o, int compact) { int e[1] = { 3 }; senc se; memset(&se, 0, sizeof se); se.compact = compact; enc_simple(o, BOX, 4, e, 1, &se); }

/* composite encoder: flags are given verbatim (MORE_COMPONENTS is added automatically) */
typedef struct { unsigned flags; unsigned gid; int a1, a2; unsigned short s[4]; } ce;
static void enc_comp(sf_buf *o, const ce *c, int n, short x0, short y0, short x1, short y1, const uint8_t *instr, int ninstr) {
    int i; sf_b16(o, 0xFFFF); sf_b16(o, x0); sf_b16(o, y0); sf_b16(o, x1); sf_b16(o, y1);
    for (i = 0; i < n; i++) {
        unsigned fl = c[i].flags | (i + 1 < n ? 0x20 : 0); if (i + 1 == n && ninstr) fl |= 0x100;
        sf_b16(o, fl); sf_b16(o, c[i].gid);
        if (fl & 1) { sf_b16(o, (unsigned)c[i].a1 & 0xFFFF); sf_b16(o, (unsigned)c[i].a2 & 0xFFFF); } else { sf_b8(o, c[i].a1 & 0xFF); sf_b8(o, c[i].a2 & 0xFF); }
        if (fl & 0x08) sf_b16(o, c[i].s[0]); else if (fl & 0x40) { sf_b16(o, c[i].s[0]); sf_b16(o, c[i].s[1]); } else if (fl & 0x80) { int k; for (k = 0; k < 4; k++) sf_b16(o, c[i].s[k]); }
    }
    if (ninstr) { sf_b16(o, ninstr); for (i = 0; i < ninstr; i++) sf_b8(o, instr[i]); }
    pad4(o);
}
#define F_WORDS 0x0001
#define F_XY    0x0002
#define F_ROUND 0x0004
#define F_SCALE 0x0008
#define F_XYSCALE 0x0040
#define F_2X2   0x0080
#define F_UMM   0x0200
#define F_SCOFF 0x0800
#define F_UNSCOFF 0x1000
#define F2D14(x) ((unsigned short)(short)((x) * 16384.0))

static sf_blob font_from(const gset *s, int long_loca) {
    ttf_spec sp; sf_buf glyf = { 0, 0, 0 }, loca = { 0, 0, 0 }; ttf_rawtable raw[2]; int i; sf_blob b;
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) {
        if (long_loca) sf_b32(&loca, (uint32_t)glyf.n); else sf_b16(&loca, (unsigned)(glyf.n / 2));
        { size_t k; for (k = 0; k < s->g[i].n; k++) sf_b8(&glyf, s->g[i].p[k]); }
        while (glyf.n & 1) sf_b8(&glyf, 0);
    }
    if (long_loca) sf_b32(&loca, (uint32_t)glyf.n); else sf_b16(&loca, (unsigned)(glyf.n / 2));
    ttf_default_spec(&sp); sp.long_loca = long_loca;
    {   /* hmtx lsb := glyph header xMin for every custom glyph, so no implementation has a reason to shift outlines */
        static ttf_glyph gl[TTF_DEF_NGLYPHS]; int ng; const ttf_glyph *d = ttf_default_glyphs(&ng); memcpy(gl, d, sizeof gl);
        for (i = 0; i < TTF_DEF_NGLYPHS; i++) if (s->g[i].n >= 10) { gl[i].lsb_set = 1; gl[i].lsb = (short)sf_r16(s->g[i].p + 2); }
        sp.glyphs = gl;
    }
    memcpy(raw[0].tag, "glyf", 5); raw[0].data = glyf.p; raw[0].len = (unsigned)glyf.n;
    memcpy(raw[1].tag, "loca", 5); raw[1].data = loca.p; raw[1].len = (unsigned)loca.n;
    sp.raw = raw; sp.nraw = 2; b = sf_build(&sp);
    sf_buf_free(&glyf); sf_buf_free(&loca); return b;
}
typedef struct { HRESULT hr; int nfig, nlines, nbez, npts; float x0, y0, x1, y1; } outl;
static outl outline(IDWriteFontFace *f, UINT16 gid) {
    outl r; dwx_geom *g = dwx_geom_new(); memset(&r, 0, sizeof r);
    r.hr = IDWriteFontFace_GetGlyphRunOutline(f, 2048.0f, &gid, NULL, NULL, 1, FALSE, FALSE, (IDWriteGeometrySink *)g);
    r.nfig = g->nfigures_begun; r.nlines = g->nlines; r.nbez = g->nbeziers; r.npts = g->npts; dwx_geom_bounds(g, &r.x0, &r.y0, &r.x1, &r.y1);
    CHECK(g->order_violations == 0);
    dwx_release(g); return r;
}
/* x range / y magnitude range of the outline (the sink's y axis points down) */
static void expect_bounds(const char *l, const outl *o, float x0, float x1, float ymin, float ymax) {
    float lo = o->y0 < o->y1 ? o->y0 : o->y1, hi = o->y0 < o->y1 ? o->y1 : o->y0, a, b;
    a = -hi; b = -lo;                                                            /* the sink's y axis points down: font y = -sink y */
    CHECKF(fabsf(o->x0 - x0) < 0.6f && fabsf(o->x1 - x1) < 0.6f, "%s: x range [%g,%g] want [%g,%g]", l, o->x0, o->x1, x0, x1);
    CHECKF(fabsf(a - ymin) < 0.6f && fabsf(b - ymax) < 0.6f, "%s: y range [%g,%g] want [%g,%g] (sink y [%g,%g])", l, a, b, ymin, ymax, o->y0, o->y1);
}
typedef struct { sf_probe p; sf_blob b; gset gs; } gf;
static int gf_open(gf *c, int long_loca, const char *label) {
    c->b = font_from(&c->gs, long_loca); sf_survey_light = 0;
    if (!sf_open_face(&c->p, c->b.p, c->b.n)) { sf_close(&c->p); sf_free(&c->b); gs_free(&c->gs); return 0; }
    t_note("%s: face=%s", label, dw_hr_name(c->p.hr_face));
    return 1;
}
static void gf_close(gf *c) { sf_close(&c->p); sf_free(&c->b); gs_free(&c->gs); }
#define BEGIN(c) gf c; gs_init(&c.gs)
#define OPENF(c, ll, label) if (!gf_open(&c, ll, label)) return; if (!c.p.face) { CHECKF(0, "%s: no face (%s)", label, dw_hr_name(c.p.hr_face)); gf_close(&c); return; }

/* ---- simple glyphs ---------------------------------------------------------------------------------------------------- */
static void simple_box_case(int compact, int long_loca, const char *label) {
    BEGIN(c); outl o; box(&c.gs.g[2], compact);
    OPENF(c, long_loca, label);
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 1); expect_bounds(label, &o, 100, 900, 0, 1400);
    gf_close(&c);
}
TEST(gl_box_long_flags_short_loca) { simple_box_case(0, 0, "16-bit coordinates, short loca"); }
TEST(gl_box_long_flags_long_loca) { simple_box_case(0, 1, "16-bit coordinates, long loca"); }
TEST(gl_box_compact_flags_short_loca) { simple_box_case(1, 0, "compact flags, short loca"); }
TEST(gl_box_compact_flags_long_loca) { simple_box_case(1, 1, "compact flags, long loca"); }
TEST(gl_empty_glyph_has_no_figures) {
    BEGIN(c); outl o; OPENF(c, 0, "all glyphs empty");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 0); gf_close(&c);
}
TEST(gl_zero_contour_glyph_with_data) {
    BEGIN(c); outl o; sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0);
    OPENF(c, 0, "numberOfContours = 0 with a header");
    o = outline(c.p.face, 2); t_note("hr=%s figures=%d", dw_hr_name(o.hr), o.nfig); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 0); gf_close(&c);
}
TEST(gl_coordinate_ranges) {
    /* deltas that need 1 byte, 2 bytes, negative and positive, zero in x or y: all four flag combinations */
    static const P pts[] = { { -1000, -500, 1 }, { 3000, -500, 1 }, { 3000, 4000, 1 }, { 3100, 4000, 1 }, { 3100, 4255, 1 }, { 2845, 4255, 1 }, { 2845, 4256, 1 }, { 500, 4256, 1 }, { -1000, 4000, 1 } };
    int ends[] = { 8 }; senc se; int cmp;
    for (cmp = 0; cmp < 2; cmp++) {
        BEGIN(c); outl o; char l[48]; memset(&se, 0, sizeof se); se.compact = cmp; enc_simple(&c.gs.g[2], pts, 9, ends, 1, &se);
        snprintf(l, sizeof l, "wide coordinate range, compact=%d", cmp);
        OPENF(c, 1, l);
        o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); expect_bounds(l, &o, -1000, 3100, -500, 4256 /* abs after flip */);
        gf_close(&c);
    }
}
TEST(gl_extreme_coordinates) {
    static const P pts[] = { { -32768, -32768, 1 }, { 32767, -32768, 1 }, { 32767, 32767, 1 }, { -32768, 32767, 1 } };
    int ends[] = { 3 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1; enc_simple(&c.gs.g[2], pts, 4, ends, 1, &se);
    OPENF(c, 1, "int16 extremes");
    o = outline(c.p.face, 2); t_note("hr=%s x[%g,%g] y[%g,%g]", dw_hr_name(o.hr), o.x0, o.x1, o.y0, o.y1); CHECK_HR(o.hr, S_OK);
    PRED("int16 extremes preserved", fabsf(o.x0 + 32768.0f) < 1 && fabsf(o.x1 - 32767.0f) < 1);
    gf_close(&c);
}
TEST(gl_repeat_flags_long_runs) {
    P pts[300]; int ends[1] = { 299 }, i; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1;
    for (i = 0; i < 300; i++) { pts[i].x = (short)(100 + 3 * i); pts[i].y = (short)(10 + 2 * i); pts[i].on = 1; }
    enc_simple(&c.gs.g[2], pts, 300, ends, 1, &se);
    OPENF(c, 1, "300 points sharing one flag (repeat runs of 256 + 44)");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); expect_bounds("repeat flags", &o, 100, 100 + 3 * 299, 10, 10 + 2 * 299); gf_close(&c);
}
TEST(gl_repeat_flag_alternating_runs) {
    P pts[64]; int ends[1] = { 63 }, i; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1;
    for (i = 0; i < 64; i++) { pts[i].x = (short)(200 + (i / 8) * 40 + (i % 8) * 2); pts[i].y = (short)(100 + (i % 8) * 90 + (i / 8)); pts[i].on = 1; }
    enc_simple(&c.gs.g[2], pts, 64, ends, 1, &se);
    OPENF(c, 0, "runs of 7 and 1");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 1);
    gf_close(&c);
}
TEST(gl_instructions_are_skipped) {
    static uint8_t ins[40]; int e[1] = { 3 }, i; senc se; BEGIN(c); outl o;
    for (i = 0; i < 40; i++) ins[i] = (uint8_t)(0xB0 + (i & 7));            /* PUSHB[n] garbage */
    memset(&se, 0, sizeof se); se.compact = 1; se.instr = ins; se.ninstr = 40; enc_simple(&c.gs.g[2], BOX, 4, e, 1, &se);
    OPENF(c, 0, "40 instruction bytes");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); expect_bounds("instructions", &o, 100, 900, 0, 1400); gf_close(&c);
}
TEST(gl_instruction_length_overruns_glyph) {
    int e[1] = { 3 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.instr = (const uint8_t *)""; se.ninstr = 0; enc_simple(&c.gs.g[2], BOX, 4, e, 1, &se);
    { size_t at = 10 + 2; sf_w16(c.gs.g[2].p + at + 2, 0x7000); }          /* instructionLength field */
    OPENF(c, 0, "instruction length beyond the glyph");
    o = outline(c.p.face, 2); t_note("hr=%s", dw_hr_name(o.hr)); PRED("overrun glyph fails", FAILED(o.hr));
    gf_close(&c);
}
TEST(gl_overlap_flag_is_harmless) {
    int e[1] = { 3 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1; se.overlap = 1; enc_simple(&c.gs.g[2], BOX, 4, e, 1, &se);
    OPENF(c, 0, "OVERLAP_SIMPLE flag");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); expect_bounds("overlap", &o, 100, 900, 0, 1400); gf_close(&c);
}
TEST(gl_header_bbox_is_not_the_outline) {
    int e[1] = { 3 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.bbox_override = 1; se.bb[0] = -500; se.bb[1] = -500; se.bb[2] = 5000; se.bb[3] = 5000;
    enc_simple(&c.gs.g[2], BOX, 4, e, 1, &se);
    OPENF(c, 0, "header bbox lies");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); t_note("outline with a lying header bbox: x[%g,%g] y[%g,%g] (points say x[100,900] y[0,1400])", o.x0, o.x1, o.y0, o.y1);
    CHECK(fabsf((o.x1 - o.x0) - 800) < 0.6f);
    { DWRITE_GLYPH_METRICS m; UINT16 g = 2; if (SUCCEEDED(IDWriteFontFace_GetDesignGlyphMetrics(c.p.face, &g, 1, &m, FALSE))) t_note("metrics with a lying bbox: lsb=%d adv=%u rsb=%d", (int)m.leftSideBearing, (unsigned)m.advanceWidth, (int)m.rightSideBearing); }
    gf_close(&c);
}
TEST(gl_off_curve_points_become_beziers) {
    static const P pts[] = { { 500, 0, 1 }, { 1000, 0, 0 }, { 1000, 500, 1 }, { 1000, 1000, 0 }, { 500, 1000, 1 }, { 0, 1000, 0 }, { 0, 500, 1 }, { 0, 0, 0 } };
    int e[1] = { 7 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1; enc_simple(&c.gs.g[2], pts, 8, e, 1, &se);
    OPENF(c, 0, "alternating on/off curve circle-ish");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 1); CHECK(o.nbez >= 4); expect_bounds("curved", &o, 0, 1000, 0, 1000);
    gf_close(&c);
}
TEST(gl_consecutive_off_curve_points) {
    static const P pts[] = { { 0, 0, 1 }, { 400, 0, 0 }, { 800, 0, 0 }, { 1200, 0, 1 }, { 1200, 800, 1 } };
    int e[1] = { 4 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1; enc_simple(&c.gs.g[2], pts, 5, e, 1, &se);
    OPENF(c, 0, "two consecutive off-curve points (implied on-curve midpoint)");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nbez >= 2); gf_close(&c);
}
TEST(gl_all_off_curve_contour) {
    static const P pts[] = { { 0, 500, 0 }, { 500, 1000, 0 }, { 1000, 500, 0 }, { 500, 0, 0 } };
    int e[1] = { 3 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1; enc_simple(&c.gs.g[2], pts, 4, e, 1, &se);
    OPENF(c, 0, "contour with no on-curve point");
    o = outline(c.p.face, 2); t_note("hr=%s figures=%d beziers=%d", dw_hr_name(o.hr), o.nfig, o.nbez); PRED("all-off-curve contour is drawn", SUCCEEDED(o.hr) && o.nfig == 1); gf_close(&c);
}
TEST(gl_contour_starting_off_curve) {
    static const P pts[] = { { 500, 0, 0 }, { 1000, 0, 1 }, { 1000, 1000, 1 }, { 0, 1000, 1 }, { 0, 0, 1 } };
    int e[1] = { 4 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1; enc_simple(&c.gs.g[2], pts, 5, e, 1, &se);
    OPENF(c, 0, "first point off-curve");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 1); expect_bounds("off-curve start", &o, 0, 1000, 0, 1000); gf_close(&c);
}
TEST(gl_two_contours) {
    static const P pts[] = { { 100, 0, 1 }, { 100, 1400, 1 }, { 900, 1400, 1 }, { 900, 0, 1 }, { 300, 300, 1 }, { 700, 300, 1 }, { 700, 1100, 1 }, { 300, 1100, 1 } };
    int e[2] = { 3, 7 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1; enc_simple(&c.gs.g[2], pts, 8, e, 2, &se);
    OPENF(c, 0, "box with a hole");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 2); expect_bounds("two contours", &o, 100, 900, 0, 1400); gf_close(&c);
}
TEST(gl_many_contours) {
    P pts[40]; int e[10], i; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1;
    for (i = 0; i < 10; i++) { int b = 4 * i; pts[b].x = (short)(100 * i); pts[b].y = 0; pts[b + 1].x = (short)(100 * i); pts[b + 1].y = 50; pts[b + 2].x = (short)(100 * i + 50); pts[b + 2].y = 50; pts[b + 3].x = (short)(100 * i + 50); pts[b + 3].y = 0; e[i] = b + 3; }
    for (i = 0; i < 40; i++) pts[i].on = 1;
    enc_simple(&c.gs.g[2], pts, 40, e, 10, &se);
    OPENF(c, 0, "ten contours");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 10); gf_close(&c);
}
TEST(gl_single_point_contour) {
    static const P pts[] = { { 100, 100, 1 } }; int e[1] = { 0 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); enc_simple(&c.gs.g[2], pts, 1, e, 1, &se);
    OPENF(c, 0, "one point");
    o = outline(c.p.face, 2); t_note("hr=%s figures=%d lines=%d", dw_hr_name(o.hr), o.nfig, o.nlines); PRED("degenerate contour tolerated", SUCCEEDED(o.hr)); gf_close(&c);
}
TEST(gl_two_point_contour) {
    static const P pts[] = { { 100, 100, 1 }, { 500, 500, 1 } }; int e[1] = { 1 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); enc_simple(&c.gs.g[2], pts, 2, e, 1, &se);
    OPENF(c, 0, "two points");
    o = outline(c.p.face, 2); t_note("hr=%s figures=%d lines=%d", dw_hr_name(o.hr), o.nfig, o.nlines); PRED("degenerate contour tolerated", SUCCEEDED(o.hr)); gf_close(&c);
}
TEST(gl_endpoints_decreasing) {
    int e[2] = { 3, 1 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); enc_simple(&c.gs.g[2], BOX, 4, e, 2, &se);
    OPENF(c, 0, "endPtsOfContours not ascending");
    o = outline(c.p.face, 2); t_note("hr=%s figures=%d", dw_hr_name(o.hr), o.nfig); PRED("malformed contour table fails", FAILED(o.hr)); gf_close(&c);
}
TEST(gl_endpoint_beyond_points) {
    int e[1] = { 9 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); enc_simple(&c.gs.g[2], BOX, 4, e, 1, &se);
    OPENF(c, 0, "last endpoint index beyond the point data");
    o = outline(c.p.face, 2); t_note("hr=%s", dw_hr_name(o.hr)); PRED("overrunning glyph fails", FAILED(o.hr)); gf_close(&c);
}
TEST(gl_glyph_data_truncated_by_loca) {
    int e[1] = { 3 }; senc se; BEGIN(c); outl o; memset(&se, 0, sizeof se); se.compact = 1; enc_simple(&c.gs.g[2], BOX, 4, e, 1, &se);
    c.gs.g[2].n -= 8;           /* chop the tail (coordinates) before the loca is built */
    OPENF(c, 0, "glyph cut short");
    o = outline(c.p.face, 2); t_note("hr=%s", dw_hr_name(o.hr)); PRED("short glyph fails", FAILED(o.hr)); gf_close(&c);
}
TEST(gl_glyph_shorter_than_header) {
    BEGIN(c); outl o; sf_b16(&c.gs.g[2], 1); sf_b16(&c.gs.g[2], 0);
    OPENF(c, 0, "glyph of 4 bytes");
    o = outline(c.p.face, 2); t_note("hr=%s", dw_hr_name(o.hr)); PRED("fails", FAILED(o.hr)); gf_close(&c);
}
TEST(gl_negative_contour_counts) {
    int v[] = { -1, -2, -100, -32768 }; int i;
    for (i = 0; i < 4; i++) {
        BEGIN(c); outl o; char l[48]; sf_b16(&c.gs.g[2], (unsigned)v[i] & 0xFFFF); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0);
        sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 3); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0);       /* flags=0, component gid 3, args */
        snprintf(l, sizeof l, "numberOfContours=%d", v[i]);
        if (gf_open(&c, 0, l)) { if (c.p.face) { o = outline(c.p.face, 2); t_note("  outline: %s figures=%d", dw_hr_name(o.hr), o.nfig); } gf_close(&c); }
    }
}

/* ---- composite glyphs ----------------------------------------------------------------------------------------------------- */
static void comp_case(const char *label, const ce *c1, int n, float ex0, float ex1, float eymin, float eymax, int hard) {
    BEGIN(c); outl o;
    box(&c.gs.g[3], 1); enc_comp(&c.gs.g[2], c1, n, (short)ex0, (short)eymin, (short)ex1, (short)eymax, NULL, 0);
    OPENF(c, 0, label);
    o = outline(c.p.face, 2);
    if (hard) { CHECKF(SUCCEEDED(o.hr), "%s: %s", label, dw_hr_name(o.hr)); expect_bounds(label, &o, ex0, ex1, eymin, eymax); }
    else { t_note("%s: hr=%s x[%g,%g] y[%g,%g] figures=%d", label, dw_hr_name(o.hr), o.x0, o.x1, o.y0, o.y1, o.nfig); }
    gf_close(&c);
}
TEST(gc_single_component_no_offset) { ce c[] = { { F_WORDS | F_XY, 3, 0, 0, { 0 } } }; comp_case("plain composite", c, 1, 100, 900, 0, 1400, 1); }
TEST(gc_byte_arguments) { ce c[] = { { F_XY, 3, 20, 100, { 0 } } }; comp_case("byte offsets (20,100)", c, 1, 120, 920, 100, 1500, 1); }
TEST(gc_byte_arguments_negative) { ce c[] = { { F_XY, 3, -50, -30, { 0 } } }; comp_case("byte offsets (-50,-30)", c, 1, 50, 850, -30, 1370, 1); }
TEST(gc_word_arguments) { ce c[] = { { F_WORDS | F_XY, 3, 300, 2000, { 0 } } }; comp_case("word offsets (300,2000)", c, 1, 400, 1200, 2000, 3400, 1); }
TEST(gc_word_arguments_negative) { ce c[] = { { F_WORDS | F_XY, 3, -1000, -700, { 0 } } }; comp_case("word offsets (-1000,-700)", c, 1, -900, -100, -700, 700, 1); }
TEST(gc_byte_arguments_sign_extension) { ce c[] = { { F_XY, 3, 0xFF, 0x80, { 0 } } }; comp_case("byte offsets 0xFF,0x80 are signed (-1,-128)", c, 1, 99, 899, -128, 1272, 1); }
TEST(gc_round_xy_flag) { ce c[] = { { F_WORDS | F_XY | F_ROUND, 3, 11, 13, { 0 } } }; comp_case("ROUND_XY_TO_GRID", c, 1, 111, 911, 13, 1413, 0); }
TEST(gc_uniform_scale_half) { ce c[] = { { F_WORDS | F_XY | F_SCALE, 3, 0, 0, { F2D14(0.5) } } }; comp_case("scale 0.5", c, 1, 50, 450, 0, 700, 1); }
TEST(gc_uniform_scale_two) { ce c[] = { { F_WORDS | F_XY | F_SCALE, 3, 0, 0, { F2D14(1.99) } } }; comp_case("scale ~2", c, 1, 199, 1791, 0, 2786, 0); }
TEST(gc_uniform_scale_with_offset_default) { ce c[] = { { F_WORDS | F_XY | F_SCALE, 3, 200, 0, { F2D14(0.5) } } }; comp_case("scale 0.5, offset (200,0), no SCALED/UNSCALED flag", c, 1, 250, 650, 0, 700, 0); }
TEST(gc_uniform_scale_with_unscaled_offset) { ce c[] = { { F_WORDS | F_XY | F_SCALE | F_UNSCOFF, 3, 200, 0, { F2D14(0.5) } } }; comp_case("scale 0.5, offset 200 UNSCALED", c, 1, 250, 650, 0, 700, 1); }
TEST(gc_uniform_scale_with_scaled_offset) { ce c[] = { { F_WORDS | F_XY | F_SCALE | F_SCOFF, 3, 200, 0, { F2D14(0.5) } } }; comp_case("scale 0.5, offset 200 SCALED", c, 1, 150, 550, 0, 700, 1); }
TEST(gc_negative_scale_mirrors) { ce c[] = { { F_WORDS | F_XY | F_SCALE, 3, 0, 0, { F2D14(-1.0) } } }; comp_case("scale -1", c, 1, -900, -100, -1400, 0, 1); }
TEST(gc_xy_scale) { ce c[] = { { F_WORDS | F_XY | F_XYSCALE, 3, 0, 0, { F2D14(0.5), F2D14(0.25) } } }; comp_case("x scale 0.5, y scale 0.25", c, 1, 50, 450, 0, 350, 1); }
TEST(gc_xy_scale_flip_y) { ce c[] = { { F_WORDS | F_XY | F_XYSCALE, 3, 0, 0, { F2D14(1.0), F2D14(-1.0) } } }; comp_case("y scale -1", c, 1, 100, 900, -1400, 0, 1); }
TEST(gc_two_by_two_halve_y) { ce c[] = { { F_WORDS | F_XY | F_2X2, 3, 0, 0, { F2D14(1.0), 0, 0, F2D14(0.5) } } }; comp_case("2x2 diag(1,0.5)", c, 1, 100, 900, 0, 700, 1); }
TEST(gc_two_by_two_rotate_90) { ce c[] = { { F_WORDS | F_XY | F_2X2, 3, 0, 0, { 0, F2D14(1.0), 0xC000, 0 } } }; comp_case("2x2 rotate 90deg", c, 1, -1400, 0, 100, 900, 1); }
TEST(gc_two_by_two_shear) { ce c[] = { { F_WORDS | F_XY | F_2X2, 3, 0, 0, { F2D14(1.0), 0, F2D14(0.5), F2D14(1.0) } } }; comp_case("2x2 shear x += 0.5*y", c, 1, 100, 1600, 0, 1400, 1); }
TEST(gc_two_components) { ce c[] = { { F_WORDS | F_XY, 3, 0, 0, { 0 } }, { F_WORDS | F_XY, 3, 1000, 1500, { 0 } } }; comp_case("two boxes", c, 2, 100, 1900, 0, 2900, 1); }
TEST(gc_three_components_mixed_encodings) {
    ce c[] = { { F_XY, 3, 0, 0, { 0 } }, { F_WORDS | F_XY | F_SCALE, 3, 1000, 0, { F2D14(0.5) } }, { F_XY | F_XYSCALE, 3, 10, 20, { F2D14(0.25), F2D14(0.25) } } };
    comp_case("three components, byte/word/scale/xyscale", c, 3, 35, 1450, 0, 1400, 0);
}
TEST(gc_many_components) {
    ce c[12]; int i; for (i = 0; i < 12; i++) { c[i].flags = F_WORDS | F_XY; c[i].gid = 3; c[i].a1 = 1000 * i; c[i].a2 = 0; memset(c[i].s, 0, sizeof c[i].s); }
    comp_case("twelve components", c, 12, 100, 11900, 0, 1400, 1);
}
TEST(gc_use_my_metrics_flag) {
    BEGIN(c); ce cc[] = { { F_WORDS | F_XY | F_UMM, 3, 0, 0, { 0 } } }; DWRITE_GLYPH_METRICS m2, m3; UINT16 g2 = 2, g3 = 3;
    box(&c.gs.g[3], 1); enc_comp(&c.gs.g[2], cc, 1, 100, 0, 900, 1400, NULL, 0);
    OPENF(c, 0, "USE_MY_METRICS");
    if (SUCCEEDED(IDWriteFontFace_GetDesignGlyphMetrics(c.p.face, &g2, 1, &m2, FALSE)) && SUCCEEDED(IDWriteFontFace_GetDesignGlyphMetrics(c.p.face, &g3, 1, &m3, FALSE)))
        t_note("composite adv=%u lsb=%d, component adv=%u lsb=%d (hmtx: adv 1000 each)", (unsigned)m2.advanceWidth, (int)m2.leftSideBearing, (unsigned)m3.advanceWidth, (int)m3.leftSideBearing);
    gf_close(&c);
}
TEST(gc_composite_instructions) {
    static const uint8_t ins[] = { 0xB0, 0x01, 0x2F, 0x2E };
    BEGIN(c); ce cc[] = { { F_WORDS | F_XY, 3, 0, 0, { 0 } }, { F_WORDS | F_XY, 3, 1000, 0, { 0 } } }; outl o;
    box(&c.gs.g[3], 1); enc_comp(&c.gs.g[2], cc, 2, 100, 0, 1900, 1400, ins, 4);
    OPENF(c, 0, "composite with trailing instructions");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); expect_bounds("composite+instructions", &o, 100, 1900, 0, 1400); gf_close(&c);
}
TEST(gc_nested_composites) {
    BEGIN(c); ce a[] = { { F_WORDS | F_XY, 3, 0, 0, { 0 } }, { F_WORDS | F_XY, 3, 1000, 0, { 0 } } }, b[] = { { F_WORDS | F_XY, 4, 0, 0, { 0 } }, { F_WORDS | F_XY, 4, 0, 2000, { 0 } } }; outl o;
    box(&c.gs.g[3], 1); enc_comp(&c.gs.g[4], a, 2, 100, 0, 1900, 1400, NULL, 0); enc_comp(&c.gs.g[2], b, 2, 100, 0, 1900, 3400, NULL, 0);
    OPENF(c, 0, "composite of composites");
    o = outline(c.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 4); expect_bounds("nested", &o, 100, 1900, 0, 3400); gf_close(&c);
}
TEST(gc_nesting_depth_chain) {
    BEGIN(c); int d; outl o; ce one[1];
    box(&c.gs.g[3], 1);
    for (d = 0; d < 6; d++) { one[0].flags = F_WORDS | F_XY; one[0].gid = d == 0 ? 3 : 7 + d - 1; one[0].a1 = 10; one[0].a2 = 0; memset(one[0].s, 0, sizeof one[0].s); enc_comp(&c.gs.g[7 + d], one, 1, 0, 0, 0, 0, NULL, 0); }
    OPENF(c, 0, "chain of six single-component composites");
    o = outline(c.p.face, 12); t_note("depth-6 chain: %s x[%g,%g]", dw_hr_name(o.hr), o.x0, o.x1); PRED("deep chain resolves", SUCCEEDED(o.hr) && fabsf(o.x0 - (100 + 60)) < 1);
    gf_close(&c);
}
TEST(gc_component_gid_out_of_range) {
    ce c[] = { { F_WORDS | F_XY, 500, 0, 0, { 0 } } }; BEGIN(c2); outl o;
    enc_comp(&c2.gs.g[2], c, 1, 0, 0, 0, 0, NULL, 0);
    OPENF(c2, 0, "component gid 500 of 69");
    o = outline(c2.p.face, 2); t_note("hr=%s", dw_hr_name(o.hr)); PRED("bad component gid fails", FAILED(o.hr)); gf_close(&c2);
}
TEST(gc_component_is_empty_glyph) {
    ce c[] = { { F_WORDS | F_XY, 5, 0, 0, { 0 } }, { F_WORDS | F_XY, 3, 0, 0, { 0 } } }; BEGIN(c2); outl o;
    box(&c2.gs.g[3], 1); enc_comp(&c2.gs.g[2], c, 2, 100, 0, 900, 1400, NULL, 0);
    OPENF(c2, 0, "first component is an empty glyph");
    o = outline(c2.p.face, 2); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 1); gf_close(&c2);
}
TEST(gc_point_matching_arguments) {
    ce c[] = { { F_WORDS, 3, 0, 0, { 0 } }, { F_WORDS, 3, 2, 0, { 0 } } }; BEGIN(c2); outl o;     /* ARGS_ARE_XY_VALUES clear: (parent point, child point) */
    box(&c2.gs.g[3], 1); enc_comp(&c2.gs.g[2], c, 2, 100, 0, 1700, 2800, NULL, 0);
    OPENF(c2, 0, "point matching (parent pt 2 on child pt 0)");
    o = outline(c2.p.face, 2); t_note("point matching: %s x[%g,%g] y[%g,%g] figures %d", dw_hr_name(o.hr), o.x0, o.x1, o.y0, o.y1, o.nfig);
    PRED("point matching supported", SUCCEEDED(o.hr)); gf_close(&c2);
}
TEST(gc_point_matching_out_of_range) {
    ce c[] = { { F_WORDS, 3, 0, 0, { 0 } }, { F_WORDS, 3, 99, 0, { 0 } } }; BEGIN(c2); outl o;
    box(&c2.gs.g[3], 1); enc_comp(&c2.gs.g[2], c, 2, 100, 0, 900, 1400, NULL, 0);
    OPENF(c2, 0, "point matching with parent point 99");
    o = outline(c2.p.face, 2); t_note("hr=%s", dw_hr_name(o.hr)); gf_close(&c2);
}
TEST(gc_truncated_component_record) {
    ce c[] = { { F_WORDS | F_XY, 3, 0, 0, { 0 } } }; BEGIN(c2); outl o;
    box(&c2.gs.g[3], 1); enc_comp(&c2.gs.g[2], c, 1, 0, 0, 0, 0, NULL, 0); c2.gs.g[2].n = 14;       /* header + flags/gid only */
    OPENF(c2, 0, "composite cut in the middle of the arguments");
    o = outline(c2.p.face, 2); t_note("hr=%s", dw_hr_name(o.hr)); PRED("truncated composite fails", FAILED(o.hr)); gf_close(&c2);
}
TEST(gc_more_components_without_data) {
    BEGIN(c); outl o; sf_b16(&c.gs.g[2], 0xFFFF); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0);
    sf_b16(&c.gs.g[2], F_WORDS | F_XY | 0x20); sf_b16(&c.gs.g[2], 3); sf_b16(&c.gs.g[2], 0); sf_b16(&c.gs.g[2], 0);       /* MORE_COMPONENTS but nothing follows */
    box(&c.gs.g[3], 1);
    OPENF(c, 0, "MORE_COMPONENTS with no next record");
    o = outline(c.p.face, 2); t_note("hr=%s", dw_hr_name(o.hr)); PRED("fails", FAILED(o.hr)); gf_close(&c);
}

/* ---- loca ---------------------------------------------------------------------------------------------------------------------------- */
TEST(lo_short_loca_word_offsets) {
    /* glyphs at 4-byte spaced offsets: short loca stores offset/2 */
    BEGIN(c); outl o; int i; for (i = 2; i < 8; i++) box(&c.gs.g[i], 1);
    OPENF(c, 0, "six glyphs, short loca");
    for (i = 2; i < 8; i++) { o = outline(c.p.face, (UINT16)i); CHECKF(SUCCEEDED(o.hr) && o.nfig == 1, "gid %d: %s", i, dw_hr_name(o.hr)); }
    gf_close(&c);
}
TEST(lo_long_loca_unaligned_offsets) {
    /* long loca entries need not be multiples of 4: put the glyph at odd offset 3 by prefixing a 3 byte glyph (padded to 2 by font_from -> offsets even) */
    BEGIN(c); outl o; sf_b8(&c.gs.g[2], 0); sf_b8(&c.gs.g[2], 0); sf_b8(&c.gs.g[2], 0); sf_b8(&c.gs.g[2], 0); box(&c.gs.g[3], 1);
    OPENF(c, 1, "long loca");
    o = outline(c.p.face, 3); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 1); gf_close(&c);
}
static sf_blob loca_tweak(void (*fn)(uint8_t *loca, unsigned len, int is_long), int is_long) {
    gset gs; sf_blob b; unsigned l; uint8_t *lo; int i; gs_init(&gs);
    for (i = 2; i < 6; i++) box(&gs.g[i], 1);
    b = font_from(&gs, is_long); gs_free(&gs);
    lo = sf_table(&b, "loca", &l); fn(lo, l, is_long); return b;
}
static void lt_decreasing(uint8_t *lo, unsigned len, int is_long) { if (is_long) { uint32_t a = sf_r32(lo + 4 * 3); sf_w32(lo + 4 * 4, a - 4); } else { unsigned a = sf_r16(lo + 2 * 3); sf_w16(lo + 2 * 4, a - 2); } }
static void lt_beyond_glyf(uint8_t *lo, unsigned len, int is_long) { if (is_long) sf_w32(lo + 4 * 4, 0x00FFFFFF); else sf_w16(lo + 2 * 4, 0x7FFF); }
static void lt_all_zero(uint8_t *lo, unsigned len, int is_long) { memset(lo, 0, len); }
static void lt_all_equal_nonzero(uint8_t *lo, unsigned len, int is_long) { unsigned i; for (i = 0; i < len; i++) lo[i] = 0; if (is_long) { for (i = 0; i < len / 4; i++) sf_w32(lo + 4 * i, 8); } else { for (i = 0; i < len / 2; i++) sf_w16(lo + 2 * i, 4); } }
static void loca_case(const char *label, void (*fn)(uint8_t *, unsigned, int), int is_long) {
    sf_blob b = loca_tweak(fn, is_long); sf_probe p;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    t_note("%s: face=%s", label, dw_hr_name(p.hr_face));
    if (p.face) { int i; for (i = 2; i < 6; i++) { outl o = outline(p.face, (UINT16)i); t_note("  gid %d: %s figures=%d", i, dw_hr_name(o.hr), o.nfig); } }
    sf_close(&p); sf_free(&b);
}
TEST(lo_entry_decreasing_short) { loca_case("loca[4] < loca[3] (short)", lt_decreasing, 0); }
TEST(lo_entry_decreasing_long) { loca_case("loca[4] < loca[3] (long)", lt_decreasing, 1); }
TEST(lo_entry_beyond_glyf_short) { loca_case("loca[4] beyond glyf (short)", lt_beyond_glyf, 0); }
TEST(lo_entry_beyond_glyf_long) { loca_case("loca[4] beyond glyf (long)", lt_beyond_glyf, 1); }
TEST(lo_all_zero_short) { loca_case("loca all zero (short)", lt_all_zero, 0); }
TEST(lo_all_zero_long) { loca_case("loca all zero (long)", lt_all_zero, 1); }
TEST(lo_all_equal_nonzero) { loca_case("loca all equal (every glyph empty)", lt_all_equal_nonzero, 0); }
TEST(lo_missing_final_entry) {
    gset gs; sf_blob b; sf_probe p; unsigned l; uint8_t *lo; int i; gs_init(&gs); for (i = 2; i < 6; i++) box(&gs.g[i], 1);
    b = font_from(&gs, 0); gs_free(&gs); lo = sf_table(&b, "loca", &l);
    sf_replace_table(&b, "loca", lo, l - 2);
    if (sf_open_face(&p, b.p, b.n)) { t_note("loca without the numGlyphs+1 entry: face=%s", dw_hr_name(p.hr_face)); if (p.face) { outl o = outline(p.face, 68); t_note("  last glyph: %s", dw_hr_name(o.hr)); o = outline(p.face, 2); t_note("  gid 2: %s figures=%d", dw_hr_name(o.hr), o.nfig); } }
    sf_close(&p); sf_free(&b);
}
TEST(lo_truncated_to_half) {
    gset gs; sf_blob b; sf_probe p; unsigned l; uint8_t *lo; int i; gs_init(&gs); for (i = 2; i < 6; i++) box(&gs.g[i], 1);
    b = font_from(&gs, 0); gs_free(&gs); lo = sf_table(&b, "loca", &l);
    sf_replace_table(&b, "loca", lo, l / 2);
    if (sf_open_face(&p, b.p, b.n)) { t_note("loca cut to half: face=%s", dw_hr_name(p.hr_face)); if (p.face) { outl o = outline(p.face, 2); t_note("  gid 2: %s", dw_hr_name(o.hr)); o = outline(p.face, 60); t_note("  gid 60: %s", dw_hr_name(o.hr)); } }
    sf_close(&p); sf_free(&b);
}
TEST(lo_missing_glyf_and_loca) {
    ttf_spec s; sf_blob b; sf_probe p; ttf_default_spec(&s); s.omit |= TTF_OMIT_GLYF; b = sf_build(&s);
    CHECK(sf_find(&b, "glyf") < 0 && sf_find(&b, "loca") < 0);
    if (sf_open_face(&p, b.p, b.n)) {
        t_note("font without glyf/loca: face=%s", dw_hr_name(p.hr_face));
        if (p.face) { outl o = outline(p.face, 2); t_note("  outline of gid 2: %s figures=%d", dw_hr_name(o.hr), o.nfig); CHECK(sf_gid(p.face, 'A') == 2); }
    }
    sf_close(&p); sf_free(&b);
}
TEST(lo_glyf_without_loca) {
    sf_blob b = sf_default(); sf_probe p; sf_drop_record(&b, "loca");
    if (sf_open_face(&p, b.p, b.n)) { t_note("glyf without loca: face=%s", dw_hr_name(p.hr_face)); if (p.face) { outl o = outline(p.face, 2); t_note("  gid 2: %s", dw_hr_name(o.hr)); } }
    sf_close(&p); sf_free(&b);
}
TEST(lo_loca_without_glyf) {
    sf_blob b = sf_default(); sf_probe p; sf_drop_record(&b, "glyf");
    if (sf_open_face(&p, b.p, b.n)) { t_note("loca without glyf: face=%s", dw_hr_name(p.hr_face)); if (p.face) { outl o = outline(p.face, 2); t_note("  gid 2: %s", dw_hr_name(o.hr)); } }
    sf_close(&p); sf_free(&b);
}
TEST(lo_default_font_composite_and_ring) {
    sf_blob b = sf_default(); sf_probe p;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face) {
        outl o = outline(p.face, TTF_GID_COMPOSITE); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 2); expect_bounds("composite 66", &o, 100, 900, 0, 2800);
        o = outline(p.face, TTF_GID_RING); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 2); expect_bounds("ring 68", &o, 100, 900, 0, 1400);
        o = outline(p.face, TTF_GID_TRIANGLE); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 1); expect_bounds("triangle 64", &o, 100, 900, 0, 1400);
        o = outline(p.face, TTF_GID_ROUND); CHECK_HR(o.hr, S_OK); CHECK(o.nbez >= 1);
        o = outline(p.face, TTF_GID_EMPTY500); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 0);
        o = outline(p.face, TTF_GID_SPACE); CHECK_HR(o.hr, S_OK); CHECK(o.nfig == 0);
    }
    sf_close(&p); sf_free(&b);
}
