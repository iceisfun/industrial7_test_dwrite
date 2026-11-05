/* Shared fixtures for the IDWriteBitmapRenderTarget / IDWriteBitmapRenderTarget1 tests.
 *
 * The render target keeps its own memory DC with a 32bpp top-down DIB section selected; the tests read the pixels back
 * through GetCurrentObject(dc, OBJ_BITMAP) + GetObject(DIBSECTION).dsBm.bmBits (after GdiFlush) and, in a dedicated test,
 * through GetDIBits.  The synthetic default font (ttfgen.h) has solid box glyphs, so coverage is exactly predictable.
 */
#ifndef BRT_H
#define BRT_H
#include "dw.h"
#include "dwx.h"

#define BRT_UPM 2048.0f
/* default synthetic box glyph, in pixels for a given em (DIPs * pixelsPerDip = em for ppd 1): */
#define BRT_BX0(em) (100.0f * (em) / BRT_UPM)
#define BRT_BX1(em) (900.0f * (em) / BRT_UPM)
#define BRT_BY1(em) (1400.0f * (em) / BRT_UPM)          /* height above the baseline */
#define BRT_ADV(em) (1000.0f * (em) / BRT_UPM)

typedef struct brt_ctx {
    IDWriteFactory *f; IDWriteGdiInterop *gi;
    IDWriteBitmapRenderTarget *rt; IDWriteBitmapRenderTarget1 *rt1;
    HDC dc; int w, h;
    dwx_font font; int have_font;
    IDWriteRenderingParams *rp_default;           /* factory CreateRenderingParams */
    IDWriteRenderingParams *rp_mode[7];           /* custom params, one per DWRITE_RENDERING_MODE (0..6), ClearType RGB level 1 */
} brt_ctx;

static void brt_close(brt_ctx *c) {
    int i;
    for (i = 0; i < 7; i++) DW_RELEASE(c->rp_mode[i]);
    DW_RELEASE(c->rp_default);
    c->rt1 = NULL; DW_RELEASE(c->rt);
    if (c->have_font) dwx_font_close(&c->font);
    DW_RELEASE(c->gi);
    memset(c, 0, sizeof *c);
}

/* Open a render target of the given size (hdc may be NULL). with_font opens the default synthetic font.
 * On failure everything is released and the test is skipped. */
static void brt_open(brt_ctx *c, HDC hdc, int w, int h, int with_font) {
    HRESULT hr; int i;
    memset(c, 0, sizeof *c);
    c->f = dw_shared(); c->w = w; c->h = h;
    if (!c->f) { t_skip("no factory"); return; }
    hr = IDWriteFactory_GetGdiInterop(c->f, &c->gi);
    if (FAILED(hr) || !c->gi) { brt_close(c); t_skip("GetGdiInterop failed %s", dw_hr_name(hr)); return; }
    hr = IDWriteGdiInterop_CreateBitmapRenderTarget(c->gi, hdc, (UINT32)w, (UINT32)h, &c->rt);
    if (FAILED(hr) || !c->rt) { brt_close(c); t_skip("CreateBitmapRenderTarget(%d,%d) failed %s", w, h, dw_hr_name(hr)); return; }
    /* rt1 is a borrowed pointer: same object as rt, the QI reference is dropped at once so refcount stays 1 */
    if (FAILED(IDWriteBitmapRenderTarget_QueryInterface(c->rt, &IID_IDWriteBitmapRenderTarget1, (void **)&c->rt1))) c->rt1 = NULL;
    else IDWriteBitmapRenderTarget1_Release(c->rt1);
    c->dc = IDWriteBitmapRenderTarget_GetMemoryDC(c->rt);
    if (with_font) {
        if (FAILED(dwx_font_open_default(&c->font, c->f)) || !c->font.face) { dwx_font_close(&c->font); brt_close(c); t_skip("synthetic font failed"); return; }
        c->have_font = 1;
        if (FAILED(IDWriteFactory_CreateRenderingParams(c->f, &c->rp_default))) c->rp_default = NULL;
        for (i = 0; i < 7; i++)
            if (FAILED(IDWriteFactory_CreateCustomRenderingParams(c->f, 2.2f, 0.0f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)i, &c->rp_mode[i]))) c->rp_mode[i] = NULL;
    }
}

/* ---- pixel access ------------------------------------------------------------------------------------------- */
/* Pointer to the DIB bits of the bitmap currently selected into the target's DC (NULL when it is not a DIB section). */
static uint32_t *brt_bits(brt_ctx *c, int *w, int *h) {
    DIBSECTION ds; HGDIOBJ bmp; int n;
    GdiFlush();
    bmp = GetCurrentObject(c->dc, OBJ_BITMAP);
    memset(&ds, 0, sizeof ds);
    n = GetObjectA(bmp, sizeof ds, &ds);
    if (n != (int)sizeof ds || !ds.dsBm.bmBits) return NULL;
    if (w) *w = ds.dsBm.bmWidth;
    if (h) *h = ds.dsBm.bmHeight;
    return (uint32_t *)ds.dsBm.bmBits;
}
static uint32_t brt_px(brt_ctx *c, int x, int y) {
    int w = 0, h = 0; uint32_t *b = brt_bits(c, &w, &h);
    if (!b || x < 0 || y < 0 || x >= w || y >= h) return 0xDEADBEEF;
    return b[y * w + x] & 0x00FFFFFF;
}
static void brt_fill(brt_ctx *c, uint32_t xrgb) {
    int w = 0, h = 0, i; uint32_t *b = brt_bits(c, &w, &h);
    if (!b) return;
    for (i = 0; i < w * h; i++) b[i] = xrgb;
    GdiFlush();
}
static uint64_t brt_hash(brt_ctx *c) {
    int w = 0, h = 0, i; uint64_t hh = 1469598103934665603ull; uint32_t *b = brt_bits(c, &w, &h);
    if (!b) return 0;
    for (i = 0; i < w * h; i++) { hh ^= b[i] & 0xFFFFFF; hh *= 1099511628211ull; }
    return hh;
}
static int brt_count_not(brt_ctx *c, uint32_t xrgb) {
    int w = 0, h = 0, i, n = 0; uint32_t *b = brt_bits(c, &w, &h);
    if (!b) return -1;
    for (i = 0; i < w * h; i++) if ((b[i] & 0xFFFFFF) != xrgb) n++;
    return n;
}
static int brt_count_eq(brt_ctx *c, uint32_t xrgb) { int w = 0, h = 0; brt_bits(c, &w, &h); return w * h - brt_count_not(c, xrgb); }
/* Bounding box (right/bottom exclusive) of pixels differing from bg; returns 0 when none. */
static int brt_bbox_not(brt_ctx *c, uint32_t bg, RECT *rc) {
    int w = 0, h = 0, x, y; uint32_t *b = brt_bits(c, &w, &h);
    rc->left = w; rc->top = h; rc->right = rc->bottom = 0;
    if (!b) return 0;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) if ((b[y * w + x] & 0xFFFFFF) != bg) {
        if (x < rc->left) rc->left = x; if (y < rc->top) rc->top = y; if (x + 1 > rc->right) rc->right = x + 1; if (y + 1 > rc->bottom) rc->bottom = y + 1;
    }
    return rc->right > rc->left;
}
static int brt_near(uint32_t a, uint32_t b, int tol) {
    int s;
    for (s = 0; s < 24; s += 8) { int d = (int)((a >> s) & 0xFF) - (int)((b >> s) & 0xFF); if (d < 0) d = -d; if (d > tol) return 0; }
    return 1;
}
/* number of pixels in [x0,x1) x [y0,y1) that are NOT within tol of xrgb (rect clamped to the bitmap; -1 when unreadable) */
static int brt_rect_mismatch(brt_ctx *c, int x0, int y0, int x1, int y1, uint32_t xrgb, int tol) {
    int w = 0, h = 0, x, y, n = 0; uint32_t *b = brt_bits(c, &w, &h);
    if (!b) return -1;
    for (y = y0; y < y1; y++) for (x = x0; x < x1; x++) {
        if (x < 0 || y < 0 || x >= w || y >= h) continue;
        if (!brt_near(b[y * w + x] & 0xFFFFFF, xrgb, tol)) n++;
    }
    return n;
}
static int brt_isize(float v) { return (int)floorf(v); }

/* ---- drawing -------------------------------------------------------------------------------------------------- */
#define BRT_RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))          /* pixel (xRGB) */
#define BRT_COLORREF(xrgb) RGB(((xrgb) >> 16) & 0xFF, ((xrgb) >> 8) & 0xFF, (xrgb) & 0xFF)

typedef struct brt_run {
    DWRITE_GLYPH_RUN run; UINT16 idx[64]; float adv[64]; DWRITE_GLYPH_OFFSET off[64];
} brt_run;
/* Run over ASCII text through the default synthetic font cmap, advances = 1000 units. */
static void brt_make_run(brt_ctx *c, brt_run *r, const char *text, float em) {
    int i, n = (int)strlen(text);
    memset(r, 0, sizeof *r);
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) { r->idx[i] = (UINT16)ttf_default_gid((unsigned char)text[i]); r->adv[i] = BRT_ADV(em); }
    r->run.fontFace = c->font.face; r->run.fontEmSize = em; r->run.glyphCount = (UINT32)n;
    r->run.glyphIndices = r->idx; r->run.glyphAdvances = r->adv; r->run.glyphOffsets = NULL;
}
static HRESULT brt_draw_run(brt_ctx *c, const brt_run *r, float ox, float oy, DWRITE_MEASURING_MODE mm,
                            IDWriteRenderingParams *rp, uint32_t xrgb, RECT *bb) {
    return IDWriteBitmapRenderTarget_DrawGlyphRun(c->rt, ox, oy, mm, &r->run, rp, BRT_COLORREF(xrgb), bb);
}
/* Draw ASCII text with the aliased custom parameters. */
static HRESULT brt_draw(brt_ctx *c, const char *text, float em, float ox, float oy, uint32_t xrgb, RECT *bb) {
    brt_run r; brt_make_run(c, &r, text, em);
    return brt_draw_run(c, &r, ox, oy, DWRITE_MEASURING_MODE_NATURAL, c->rp_mode[DWRITE_RENDERING_MODE_ALIASED], xrgb, bb);
}
/* One glyph by index. */
static HRESULT brt_draw_gid(brt_ctx *c, UINT16 gid, float em, float ox, float oy, uint32_t xrgb, RECT *bb, int rmode) {
    brt_run r; brt_make_run(c, &r, "A", em); r.idx[0] = gid;
    return brt_draw_run(c, &r, ox, oy, DWRITE_MEASURING_MODE_NATURAL, c->rp_mode[rmode], xrgb, bb);
}

/* Create a target + font and report; shorthand for tests that only need a fixture. */
#define BRT_FIXTURE(c, w, h) brt_ctx c; brt_open(&c, NULL, (w), (h), 1)

/* Matrix helper */
static DWRITE_MATRIX brt_mat(float m11, float m12, float m21, float m22, float dx, float dy) {
    DWRITE_MATRIX m; m.m11 = m11; m.m12 = m12; m.m21 = m21; m.m22 = m22; m.dx = dx; m.dy = dy; return m;
}
static int brt_mat_eq(const DWRITE_MATRIX *a, const DWRITE_MATRIX *b) {
    return a->m11 == b->m11 && a->m12 == b->m12 && a->m21 == b->m21 && a->m22 == b->m22 && a->dx == b->dx && a->dy == b->dy;
}
/* Signed depth of (px,py) inside the convex quad q[4]: min over edges of the distance to the edge line, positive inside.
 * depth >= m  : the point is at least m pixels inside;  depth <= -m : the point is at least m pixels outside. */
static float brt_quad_depth(const float q[4][2], float px, float py) {
    int i; float area = 0, best = 1e30f;
    for (i = 0; i < 4; i++) area += q[i][0] * q[(i + 1) & 3][1] - q[(i + 1) & 3][0] * q[i][1];
    for (i = 0; i < 4; i++) {
        float ax = q[i][0], ay = q[i][1], bx = q[(i + 1) & 3][0], by = q[(i + 1) & 3][1];
        float ex = bx - ax, ey = by - ay, len = sqrtf(ex * ex + ey * ey);
        float d = (ex * (py - ay) - ey * (px - ax)) / (len > 0 ? len : 1.0f);
        if (area < 0) d = -d;
        if (d < best) best = d;
    }
    return best;
}
#endif
