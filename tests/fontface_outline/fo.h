/* Shared helpers for the fontface_outline area: IDWriteFontFace::GetGlyphRunOutline.
 *
 * The expectation model (fo_exp) re-derives, from the *synthetic font description* alone, the exact stream of
 * ID2D1SimplifiedGeometrySink calls the OS must produce.  Facts it encodes (Win10 DWriteFontFace::GetGlyphRunOutline ->
 * ComputeGlyphRunOutline -> StreamGlyphOutlines, TrueTypeRasterizer::GetOutline):
 *   - SetFillMode(WINDING) once per call (when glyphCount > 0), then per glyph / per contour:
 *     BeginFigure(pt, FILLED) , AddLines(1 point) / AddBeziers(1 segment) per segment , EndFigure(CLOSED).
 *   - Contours with fewer than 2 points produce no figure.  No SetSegmentFlags, no Close() is ever called.
 *   - TrueType quadratic segments are converted to cubic: c1 = p0 + 2/3 (q - p0), c2 = p1 + 2/3 (q - p1).
 *   - A contour starting off-curve starts at its last point (when that is on-curve) or at the midpoint of its first and
 *     last point (both off-curve); consecutive off-curve points get an implied on-curve midpoint; a trailing off-curve
 *     point curves back to the start; no closing AddLines is emitted (EndFigure(CLOSED) closes).
 *   - design (x, y-up) -> (ox + x*k, oy - y*k), k = emSize / unitsPerEm; sideways: (ox - y*k, oy - x*k).
 */
#ifndef FO_H
#define FO_H
#include "dwx.h"

#define FO_NEED_FACE() dwx_font h; memset(&h, 0, sizeof h); \
    if (!dw_shared()) { t_skip("DWriteCreateFactory unavailable"); return; } \
    DW_REQUIRE_OK(dwx_font_open_default(&h, dw_shared())); if (!h.face) { dwx_font_close(&h); t_skip("no face"); return; }

/* ---------------------------------------------------------------- expectation model */
#define FO_MAXEV 1500
typedef struct { int kind; int n; float p[6]; } fo_ev;       /* kind: DWX_G_FILL/BEGIN/LINES/BEZIERS/END */
typedef struct { fo_ev ev[FO_MAXEV]; int n; } fo_exp;

typedef struct { double x, y; int on; } fo_pt;
typedef struct { double k, ox, oy, shear; int sideways; } fo_xf;

static void fo_push(fo_exp *e, int kind, int n, double a, double b, double c, double d, double f, double g) {
    if (e->n >= FO_MAXEV) return;
    e->ev[e->n].kind = kind; e->ev[e->n].n = n;
    e->ev[e->n].p[0] = (float)a; e->ev[e->n].p[1] = (float)b; e->ev[e->n].p[2] = (float)c;
    e->ev[e->n].p[3] = (float)d; e->ev[e->n].p[4] = (float)f; e->ev[e->n].p[5] = (float)g; e->n++;
}
static void fo_map(const fo_xf *t, double x, double y, double *ox, double *oy) {
    if (!t->sideways) { double xs = x + t->shear * y; *ox = t->ox + xs * t->k; *oy = t->oy - y * t->k; }
    else { double ys = y - t->shear * x; *ox = t->ox - ys * t->k; *oy = t->oy - x * t->k; }
}
/* One contour -> figure events (appended to e). */
static void fo_contour(fo_exp *e, const fo_pt *p, int n, const fo_xf *t) {
    int i0, i1, i, pend = 0;
    double sx, sy, px = 0, py = 0, cx = 0, cy = 0, ox, oy;   /* start, pending off-curve (design), current output point */
    double curx, cury;
    if (n < 2) return;
    if (!p[0].on) {
        if (!p[n - 1].on) { sx = (p[0].x + p[n - 1].x) / 2; sy = (p[0].y + p[n - 1].y) / 2; i0 = 0; i1 = n - 1; }
        else { sx = p[n - 1].x; sy = p[n - 1].y; i0 = 0; i1 = n - 2; }
    } else { sx = p[0].x; sy = p[0].y; i0 = 1; i1 = n - 1; }
    fo_map(t, sx, sy, &curx, &cury);
    fo_push(e, DWX_G_BEGIN, 1, curx, cury, 0, 0, 0, 0);
    for (i = i0; i <= i1; i++) {
        if (!p[i].on) {
            if (pend) {
                double mx = (px + p[i].x) / 2, my = (py + p[i].y) / 2, qx, qy, ex, ey, c1x, c1y, c2x, c2y;
                fo_map(t, px, py, &qx, &qy); fo_map(t, mx, my, &ex, &ey);
                c1x = curx + 2.0 / 3 * (qx - curx); c1y = cury + 2.0 / 3 * (qy - cury);
                c2x = ex + 2.0 / 3 * (qx - ex);     c2y = ey + 2.0 / 3 * (qy - ey);
                fo_push(e, DWX_G_BEZIERS, 1, c1x, c1y, c2x, c2y, ex, ey); curx = ex; cury = ey;
            }
            px = p[i].x; py = p[i].y; pend = 1;
        } else if (pend) {
            double qx, qy, ex, ey, c1x, c1y, c2x, c2y;
            fo_map(t, px, py, &qx, &qy); fo_map(t, p[i].x, p[i].y, &ex, &ey);
            c1x = curx + 2.0 / 3 * (qx - curx); c1y = cury + 2.0 / 3 * (qy - cury);
            c2x = ex + 2.0 / 3 * (qx - ex);     c2y = ey + 2.0 / 3 * (qy - ey);
            fo_push(e, DWX_G_BEZIERS, 1, c1x, c1y, c2x, c2y, ex, ey); curx = ex; cury = ey; pend = 0;
        } else {
            fo_map(t, p[i].x, p[i].y, &ox, &oy);
            fo_push(e, DWX_G_LINES, 1, ox, oy, 0, 0, 0, 0); curx = ox; cury = oy;
        }
    }
    if (pend) {
        double qx, qy, ex, ey, c1x, c1y, c2x, c2y;
        fo_map(t, px, py, &qx, &qy); fo_map(t, sx, sy, &ex, &ey);
        c1x = curx + 2.0 / 3 * (qx - curx); c1y = cury + 2.0 / 3 * (qy - cury);
        c2x = ex + 2.0 / 3 * (qx - ex);     c2y = ey + 2.0 / 3 * (qy - ey);
        fo_push(e, DWX_G_BEZIERS, 1, c1x, c1y, c2x, c2y, ex, ey);
    }
    fo_push(e, DWX_G_END, 0, 0, 0, 0, 0, 0, 0);
}

/* Flatten glyph gid of the glyph table into design-space contours (composites resolved).
 * scaled_off: composite offsets are scaled by the component scale (SCALED_COMPONENT_OFFSET). */
typedef struct { fo_pt pts[256]; int n; int ends[32]; int nc; } fo_flat;
static void fo_flatten_rec(fo_flat *f, const ttf_glyph *tab, int ntab, int gid, double S, double tx, double ty, int depth) {
    const ttf_glyph *g;
    int i, c, start;
    if (gid < 0 || gid >= ntab || depth > 6) return;
    g = &tab[gid];
    if (g->ncomp > 0) {
        for (i = 0; i < g->ncomp; i++) {
            const ttf_comp *cp = &g->comps[i];
            double s = cp->scale ? (double)cp->scale / 16384.0 : 1.0;
            double dx = cp->dx, dy = cp->dy;
            if (cp->flags & TTF_COMP_SCALED_OFFSET) { dx *= s; dy *= s; }
            fo_flatten_rec(f, tab, ntab, cp->gid, S * s, tx + S * dx, ty + S * dy, depth + 1);
        }
        return;
    }
    start = 0;
    for (c = 0; c < g->ncont; c++) {
        int end = g->ends[c], base = f->n;
        for (i = start; i <= end && f->n < 256; i++) {
            f->pts[f->n].x = g->pts[i].x * S + tx; f->pts[f->n].y = g->pts[i].y * S + ty; f->pts[f->n].on = g->pts[i].on; f->n++;
        }
        if (f->nc < 32) f->ends[f->nc++] = f->n - 1;
        (void)base;
        start = end + 1;
    }
}
static void fo_glyph_events(fo_exp *e, const ttf_glyph *tab, int ntab, int gid, const fo_xf *t) {
    fo_flat *f = (fo_flat *)calloc(1, sizeof *f);
    int c, s = 0;
    fo_flatten_rec(f, tab, ntab, gid, 1.0, 0, 0, 0);
    for (c = 0; c < f->nc; c++) { fo_contour(e, f->pts + s, f->ends[c] - s + 1, t); s = f->ends[c] + 1; }
    free(f);
}

/* Sideways / RTL / offsets origin model: mirrors ComputeGlyphOrigins.
 * fadv[i] : the glyph's own horizontal advance in design units (non-sideways); for sideways, vadv/vorg/hadv come from
 * IDWriteFontFace::GetDesignGlyphMetrics.  Output origins in DIPs. */
typedef struct { double x, y; } fo_org;
static void fo_origins(fo_org *o, int n, double k, const double *fadv /*design adv width*/, const double *vadv /*adv height*/,
                       const double *vorg /*vertical origin Y*/, const float *adv, const DWRITE_GLYPH_OFFSET *off, int sideways, int rtl) {
    double cur = 0; int i;
    for (i = 0; i < n; i++) {
        if (sideways) {
            double ah = vadv[i] * k, vo = vorg[i] * k, aw = fadv[i] * k, step = adv ? adv[i] : ah;
            if (rtl) { o[i].x = cur - ah + vo; cur -= step; } else { o[i].x = cur + vo; cur += step; }
            o[i].y = aw * 0.5;
        } else if (!rtl && adv) { o[i].x = cur; o[i].y = 0; cur += adv[i]; }
        else {
            double fa = fadv[i] * k, step = adv ? adv[i] : fa;
            if (rtl) { o[i].x = cur - fa; cur -= step; } else { o[i].x = cur; cur += step; }
            o[i].y = 0;
        }
    }
    if (off) for (i = 0; i < n; i++) {
        if (rtl) o[i].x -= off[i].advanceOffset; else o[i].x += off[i].advanceOffset;
        o[i].y -= off[i].ascenderOffset;
    }
}

/* ---------------------------------------------------------------- comparison */
/* Compare the recorded sink stream to the model.  Returns 1 when identical within tol.  One CHECK per call; the failure
 * detail names the first difference. */
static int fo_compare_(const dwx_geom *g, const fo_exp *e, double tol, const char *file, int line, const char *what) {
    int i, pi = 0, ok = 1; char msg[200] = "";
    if (g->nev != e->n) { snprintf(msg, sizeof msg, "%s: event count %d want %d", what, g->nev, e->n); ok = 0; }
    for (i = 0; ok && i < e->n; i++) {
        const dwx_gev *a = &g->ev[i]; const fo_ev *b = &e->ev[i]; int j, np = 0;
        if (a->kind != b->kind) { snprintf(msg, sizeof msg, "%s: ev %d kind %d want %d", what, i, a->kind, b->kind); ok = 0; break; }
        if (b->kind == DWX_G_FILL) { if (a->arg != D2D1_FILL_MODE_WINDING) { snprintf(msg, sizeof msg, "%s: fill mode %d", what, a->arg); ok = 0; } continue; }
        if (b->kind == DWX_G_END) { if (a->arg != D2D1_FIGURE_END_CLOSED) { snprintf(msg, sizeof msg, "%s: ev %d end flag %d", what, i, a->arg); ok = 0; } continue; }
        if (b->kind == DWX_G_BEGIN) { np = 1; if (a->arg != D2D1_FIGURE_BEGIN_FILLED) { snprintf(msg, sizeof msg, "%s: ev %d begin flag %d", what, i, a->arg); ok = 0; break; } }
        else if (b->kind == DWX_G_LINES) np = b->n; else if (b->kind == DWX_G_BEZIERS) np = 3 * b->n;
        if (b->kind != DWX_G_BEGIN && a->count != b->n) { snprintf(msg, sizeof msg, "%s: ev %d count %d want %d", what, i, a->count, b->n); ok = 0; break; }
        for (j = 0; j < np; j++) {
            double gx, gy, wx, wy;
            if (pi >= g->npts) { snprintf(msg, sizeof msg, "%s: ran out of points at ev %d", what, i); ok = 0; break; }
            gx = g->pts[pi].x; gy = g->pts[pi].y; wx = b->p[2 * j]; wy = b->p[2 * j + 1]; pi++;
            if (!(fabs(gx - wx) <= tol && fabs(gy - wy) <= tol)) {
                snprintf(msg, sizeof msg, "%s: ev %d pt %d got (%g,%g) want (%g,%g)", what, i, j, gx, gy, wx, wy); ok = 0; break;
            }
        }
    }
    t_check(ok, file, line, "outline stream matches model", "%s", msg);
    return ok;
}
#define FO_COMPARE(g, e, tol, what) fo_compare_((g), (e), (tol), __FILE__, __LINE__, (what))

/* Structural invariants of any non-empty stream. */
static void fo_invariants_(const dwx_geom *g, int expect_events, const char *file, int line) {
    int i, bad_begin = 0, bad_end = 0, bad_cnt = 0, nfill = 0;
    for (i = 0; i < g->nev; i++) {
        const dwx_gev *a = &g->ev[i];
        if (a->kind == DWX_G_FILL) nfill++;
        if (a->kind == DWX_G_BEGIN && a->arg != D2D1_FIGURE_BEGIN_FILLED) bad_begin++;
        if (a->kind == DWX_G_END && a->arg != D2D1_FIGURE_END_CLOSED) bad_end++;
        if ((a->kind == DWX_G_LINES || a->kind == DWX_G_BEZIERS) && a->count != 1) bad_cnt++;
    }
    t_check(g->order_violations == 0, file, line, "no figure-order violations", "violations=%d", g->order_violations);
    t_check(g->close_calls == 0, file, line, "Close() never called by dwrite", "close_calls=%d", g->close_calls);
    t_check(g->nfigures_begun == g->nfigures_ended, file, line, "figures balanced", "begun=%d ended=%d", g->nfigures_begun, g->nfigures_ended);
    t_check(g->open_figure == 0, file, line, "no figure left open", "open=%d", g->open_figure);
    t_check(bad_begin == 0 && bad_end == 0 && bad_cnt == 0, file, line, "flags FILLED/CLOSED, one segment per call", "begin=%d end=%d cnt=%d", bad_begin, bad_end, bad_cnt);
    t_check(g->seg_flags == 0, file, line, "SetSegmentFlags never called", "flags=%d", g->seg_flags);
    if (expect_events) {
        t_check(nfill == 1 && g->nev > 0 && g->ev[0].kind == DWX_G_FILL, file, line, "SetFillMode first and exactly once", "fills=%d", nfill);
        t_check(g->fill_mode == D2D1_FILL_MODE_WINDING, file, line, "fill mode WINDING", "mode=%d", g->fill_mode);
    }
    t_check(g->calls_after_close == 0, file, line, "no calls after Close", "n=%d", g->calls_after_close);
}
#define FO_INVARIANTS(g, expect) fo_invariants_((g), (expect), __FILE__, __LINE__)

/* ---------------------------------------------------------------- font lab */
/* Default font (gid 0..68) extended with extra outline shapes. */
enum {
    FO_G_OFFSTART = 69,   /* start off-curve, last on-curve */
    FO_G_ALLOFF4,         /* 70 all off-curve, 4 points */
    FO_G_ALLOFF3ODD,      /* 71 all off-curve, odd coordinates (half-unit midpoints) */
    FO_G_MIDOFF,          /* 72 on, off, off, on */
    FO_G_TWOPT,           /* 73 two-point contour */
    FO_G_ONEPT_BOX,       /* 74 single-point contour then a box */
    FO_G_PENTA,           /* 75 pentagon, lines only */
    FO_G_NEG,             /* 76 box in negative coordinates */
    FO_G_MULTI,           /* 77 three contours */
    FO_G_LASTOFF,         /* 78 on, on, off */
    FO_G_COMP_SCALE,      /* 79 composite, scale 0.5 on both components */
    FO_G_COMP_NEG,        /* 80 composite with scale -1 */
    FO_G_COMP_NESTED,     /* 81 composite of composite */
    FO_G_COMP_OFF,        /* 82 scaled component with unscaled offset */
    FO_G_COMP_SOFF,       /* 83 scaled component, SCALED_COMPONENT_OFFSET */
    FO_G_BIG,             /* 84 large box */
    FO_G_NARROW,          /* 85 small box, advance 300 */
    FO_G_LSBSHIFT,        /* 86 box whose hmtx lsb differs from xMin */
    FO_G_EMPTYCONT,       /* 87 only one-point contours (no visible figure) */
    FO_G_COUNT
};
static const ttf_pt fo_p_offstart[4]  = { {500,0,0}, {900,700,1}, {500,1400,0}, {100,700,1} };
static const ttf_pt fo_p_alloff4[4]   = { {100,0,0}, {100,1400,0}, {900,1400,0}, {900,0,0} };
static const ttf_pt fo_p_alloff3[3]   = { {101,3,0}, {1001,7,0}, {555,1399,0} };
static const ttf_pt fo_p_midoff[4]    = { {100,0,1}, {100,1400,0}, {900,1400,0}, {900,0,1} };
static const ttf_pt fo_p_twopt[2]     = { {100,0,1}, {900,700,1} };
static const ttf_pt fo_p_onept_box[5] = { {500,500,1}, {100,0,1}, {100,1400,1}, {900,1400,1}, {900,0,1} };
static const ttf_pt fo_p_penta[5]     = { {500,0,1}, {900,500,1}, {700,1300,1}, {300,1300,1}, {100,500,1} };
static const ttf_pt fo_p_neg[4]       = { {-300,-200,1}, {-300,600,1}, {400,600,1}, {400,-200,1} };
static const ttf_pt fo_p_multi[9]     = { {100,0,1}, {100,1400,1}, {900,1400,1}, {900,0,1}, {300,300,0}, {700,300,0}, {500,1000,0}, {200,100,1}, {800,900,1} };
static const ttf_pt fo_p_lastoff[3]   = { {100,0,1}, {500,1400,1}, {900,0,0} };
static const ttf_pt fo_p_big[4]       = { {0,0,1}, {0,2000,1}, {2000,2000,1}, {2000,0,1} };
static const ttf_pt fo_p_narrow[4]    = { {50,0,1}, {50,500,1}, {250,500,1}, {250,0,1} };
static const ttf_pt fo_p_emptycont[3] = { {100,100,1}, {500,500,1}, {900,900,1} };
static const unsigned short fo_e3[1] = { 2 }, fo_e4[1] = { 3 }, fo_e2[1] = { 1 }, fo_e5[1] = { 4 };
static const unsigned short fo_e_onept[2] = { 0, 4 }, fo_e_multi[3] = { 3, 6, 8 }, fo_e_emptycont[3] = { 0, 1, 2 };
static const ttf_comp fo_c_scale[2]  = { { 2, 0, 0, 0, 0x2000 }, { TTF_GID_TRIANGLE, 1000, 500, 0, 0x2000 } };
static const ttf_comp fo_c_neg[1]    = { { TTF_GID_TRIANGLE, 1000, 1400, 0, -0x4000 } };
static const ttf_comp fo_c_nested[2] = { { TTF_GID_COMPOSITE, 0, 0, 0, 0 }, { 2, 1000, 0, 0, 0 } };
static const ttf_comp fo_c_off[1]    = { { TTF_GID_TRIANGLE, 200, 400, 0, 0x2000 } };
static const ttf_comp fo_c_soff[1]   = { { TTF_GID_TRIANGLE, 200, 400, TTF_COMP_SCALED_OFFSET, 0x2000 } };

static ttf_glyph fo_glyphs[FO_G_COUNT];
static void fo_lab_spec(ttf_spec *s) {
    int n, i; const ttf_glyph *d = ttf_default_glyphs(&n);
    ttf_default_spec(s);
    memset(fo_glyphs, 0, sizeof fo_glyphs);
    for (i = 0; i < n && i < TTF_DEF_NGLYPHS; i++) fo_glyphs[i] = d[i];
#define FG(id, a, np, nc, pp, ee) do { fo_glyphs[id].adv = (a); fo_glyphs[id].npts = (np); fo_glyphs[id].ncont = (nc); fo_glyphs[id].pts = (pp); fo_glyphs[id].ends = (ee); } while (0)
    FG(FO_G_OFFSTART, 1000, 4, 1, fo_p_offstart, fo_e4);
    FG(FO_G_ALLOFF4, 1000, 4, 1, fo_p_alloff4, fo_e4);
    FG(FO_G_ALLOFF3ODD, 1100, 3, 1, fo_p_alloff3, fo_e3);
    FG(FO_G_MIDOFF, 1000, 4, 1, fo_p_midoff, fo_e4);
    FG(FO_G_TWOPT, 1000, 2, 1, fo_p_twopt, fo_e2);
    FG(FO_G_ONEPT_BOX, 1000, 5, 2, fo_p_onept_box, fo_e_onept);
    FG(FO_G_PENTA, 1000, 5, 1, fo_p_penta, fo_e5);
    FG(FO_G_NEG, 700, 4, 1, fo_p_neg, fo_e4);
    FG(FO_G_MULTI, 1000, 9, 3, fo_p_multi, fo_e_multi);
    FG(FO_G_LASTOFF, 1000, 3, 1, fo_p_lastoff, fo_e3);
    FG(FO_G_BIG, 2100, 4, 1, fo_p_big, fo_e4);
    FG(FO_G_NARROW, 300, 4, 1, fo_p_narrow, fo_e4);
    FG(FO_G_LSBSHIFT, 1000, 4, 1, fo_p_neg, fo_e4);   /* xMin = -300, hmtx lsb forced to 0 */
    FG(FO_G_EMPTYCONT, 1000, 3, 3, fo_p_emptycont, fo_e_emptycont);
#undef FG
    fo_glyphs[FO_G_LSBSHIFT].lsb_set = 1; fo_glyphs[FO_G_LSBSHIFT].lsb = 0;
    fo_glyphs[FO_G_COMP_SCALE].adv = 1000;  fo_glyphs[FO_G_COMP_SCALE].ncomp = 2; fo_glyphs[FO_G_COMP_SCALE].comps = fo_c_scale;
    fo_glyphs[FO_G_COMP_NEG].adv = 1000;    fo_glyphs[FO_G_COMP_NEG].ncomp = 1;   fo_glyphs[FO_G_COMP_NEG].comps = fo_c_neg;
    fo_glyphs[FO_G_COMP_NESTED].adv = 2000; fo_glyphs[FO_G_COMP_NESTED].ncomp = 2; fo_glyphs[FO_G_COMP_NESTED].comps = fo_c_nested;
    fo_glyphs[FO_G_COMP_OFF].adv = 1000;    fo_glyphs[FO_G_COMP_OFF].ncomp = 1;   fo_glyphs[FO_G_COMP_OFF].comps = fo_c_off;
    fo_glyphs[FO_G_COMP_SOFF].adv = 1000;   fo_glyphs[FO_G_COMP_SOFF].ncomp = 1;  fo_glyphs[FO_G_COMP_SOFF].comps = fo_c_soff;
    s->glyphs = fo_glyphs; s->nglyphs = FO_G_COUNT;
}
/* Open the lab font; simulations optional.  Caller always dwx_font_close()s.  Returns the HRESULT. */
static HRESULT fo_open_lab(dwx_font *h, DWRITE_FONT_SIMULATIONS sim) {
    ttf_spec s; size_t n; uint8_t *b; HRESULT hr;
    memset(h, 0, sizeof *h);
    if (!dw_shared()) return E_FAIL;
    fo_lab_spec(&s);
    b = ttf_build(&s, &n);
    if (!b) return E_FAIL;
    hr = dwx_font_open(h, dw_shared(), b, n, 0, sim);
    free(b);
    return hr;
}
#define FO_LAB(h, sim) dwx_font h; if (FAILED(fo_open_lab(&h, sim)) || !h.face) { dwx_font_close(&h); t_skip("lab font unavailable"); return; }

/* Run one call and compare to the model: returns HRESULT. */
static HRESULT fo_call(IDWriteFontFace *face, float em, const UINT16 *g, const float *adv, const DWRITE_GLYPH_OFFSET *off,
                       UINT32 n, BOOL sw, BOOL rtl, dwx_geom *sink) {
    return IDWriteFontFace_GetGlyphRunOutline(face, em, g, adv, off, n, sw, rtl, (IDWriteGeometrySink *)sink);
}
/* Build the model for a whole run on a lab/default glyph table (upm 2048). */
static void fo_model_run(fo_exp *e, const ttf_glyph *tab, int ntab, IDWriteFontFace *face, double em, const UINT16 *g,
                         const float *adv, const DWRITE_GLYPH_OFFSET *off, UINT32 n, int sw, int rtl, double shear) {
    UINT32 i; fo_org *o = (fo_org *)calloc(n ? n : 1, sizeof *o);
    double *fa = (double *)calloc(n ? n : 1, sizeof(double)), *va = (double *)calloc(n ? n : 1, sizeof(double)), *vo = (double *)calloc(n ? n : 1, sizeof(double));
    double k = em / TTF_DEF_UPM;
    e->n = 0;
    if (n == 0) { free(o); free(fa); free(va); free(vo); return; }
    for (i = 0; i < n; i++) {
        fa[i] = g[i] < ntab ? tab[g[i]].adv : 0;
        if (sw) {
            DWRITE_GLYPH_METRICS m; UINT16 gi = g[i];
            memset(&m, 0, sizeof m);
            IDWriteFontFace_GetDesignGlyphMetrics(face, &gi, 1, &m, TRUE);
            fa[i] = m.advanceWidth; va[i] = m.advanceHeight; vo[i] = m.verticalOriginY;
        }
    }
    fo_origins(o, (int)n, k, fa, va, vo, adv, off, sw, rtl);
    fo_push(e, DWX_G_FILL, 0, 0, 0, 0, 0, 0, 0);
    for (i = 0; i < n; i++) {
        fo_xf t; t.k = k; t.ox = o[i].x; t.oy = o[i].y; t.shear = shear; t.sideways = sw;
        fo_glyph_events(e, tab, ntab, g[i], &t);
    }
    free(o); free(fa); free(va); free(vo);
}
#define FO_GLYPH_ADV_DU(tab, gid) ((double)(tab)[gid].adv)

/* bounding box of recorded control points */
static int fo_bounds(const dwx_geom *g, float *x0, float *y0, float *x1, float *y1) {
    if (!g->npts) { *x0 = *y0 = *x1 = *y1 = 0; return 0; }
    dwx_geom_bounds(g, x0, y0, x1, y1); return 1;
}
#endif
