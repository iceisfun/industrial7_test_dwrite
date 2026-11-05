/* IDWriteBitmapRenderTarget::DrawGlyphRun on the synthetic box font: argument validation, exact pixel expectations.
 *
 * Synthetic font geometry (ttfgen.h, UPM 2048): letters are boxes (100,0)-(900,1400) with advance 1000.  At em E pixels the box
 * covers x in [ox + 100E/2048, ox + 900E/2048) and y in [oy - 1400E/2048, oy).  Interior pixels (a margin away from every edge)
 * must be exactly the text colour, pixels a margin away outside must be untouched.
 *
 * From the implementation of DrawGlyphRun (when the target has pixels):
 *  - blackBoxRect (if given) is zeroed first; NaN origin, NULL glyph run, NULL renderingParams, NULL fontFace, NULL glyphIndices with
 *    glyphCount != 0, negative fontEmSize => E_INVALIDARG and nothing is drawn;
 *  - only the low 24 bits of the COLORREF are used;
 *  - rendering mode OUTLINE goes through a GDI path/region fill (strictly two-colour output), the other modes through the glyph run
 *    analysis rasteriser writing straight into the DIB bits.
 */
#include "brt.h"

#define W 0xFFFFFF
#define K 0x000000

/* Compare the bitmap to the ideal box: interior (margin m inside) must be fg (tolerance tol), the area farther than m outside must be bg.
 * Returns mismatches in *in_bad / *out_bad. Box edges are floor/ceil'd outward for the outside test and inward for the inside test. */
static void brt_box_check(brt_ctx *c, float em, float ox, float oy, uint32_t fg, uint32_t bg, int m, int tol, int *in_bad, int *out_bad) {
    int w = 0, h = 0, x, y; uint32_t *b = brt_bits(c, &w, &h);
    int ix0 = (int)ceilf(ox + BRT_BX0(em)) + m, ix1 = (int)floorf(ox + BRT_BX1(em)) - m;
    int iy0 = (int)ceilf(oy - BRT_BY1(em)) + m, iy1 = (int)floorf(oy) - m;
    int ox0 = (int)floorf(ox + BRT_BX0(em)) - m, ox1 = (int)ceilf(ox + BRT_BX1(em)) + m;
    int oy0 = (int)floorf(oy - BRT_BY1(em)) - m, oy1 = (int)ceilf(oy) + m;
    *in_bad = *out_bad = 0;
    if (!b) { *in_bad = *out_bad = -1; return; }
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        uint32_t v = b[y * w + x] & 0xFFFFFF;
        if (x >= ix0 && x < ix1 && y >= iy0 && y < iy1) { if (!brt_near(v, fg, tol)) (*in_bad)++; }
        else if (x < ox0 || x >= ox1 || y < oy0 || y >= oy1) { if (v != bg) (*out_bad)++; }
    }
}
static int brt_box_interior_empty(float em, int m) { return (BRT_BX1(em) - BRT_BX0(em)) < 2 * m + 2 || BRT_BY1(em) < 2 * m + 2; }

/* ---- argument validation -------------------------------------------------------------------------------------- */
static RECT brt_sentinel(void) { RECT r; r.left = 0x1111; r.top = 0x2222; r.right = 0x3333; r.bottom = 0x4444; return r; }
static int brt_rect_zero(const RECT *r) { return !r->left && !r->top && !r->right && !r->bottom; }

TEST(brt_dv_null_glyph_run) {
    if (t_is_wine()) { t_skip("NULL/invalid pointers crash the non-Windows implementation"); return; }
    BRT_FIXTURE(c, 40, 40); RECT bb = brt_sentinel(); uint64_t h0;
    if (!c.rt) return;
    h0 = brt_hash(&c);
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(c.rt, 5, 30, DWRITE_MEASURING_MODE_NATURAL, NULL, c.rp_mode[1], RGB(255, 255, 255), &bb), E_INVALIDARG);
    CHECK(brt_rect_zero(&bb));
    EXPECT_EQ(brt_hash(&c), h0);
    brt_close(&c);
}

TEST(brt_dv_null_rendering_params) {
    if (t_is_wine()) { t_skip("NULL/invalid pointers crash the non-Windows implementation"); return; }
    BRT_FIXTURE(c, 40, 40); RECT bb = brt_sentinel(); brt_run r; uint64_t h0;
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 20.0f); h0 = brt_hash(&c);
    CHECK_HR(brt_draw_run(&c, &r, 5, 30, DWRITE_MEASURING_MODE_NATURAL, NULL, 0xFFFFFF, &bb), E_INVALIDARG);
    CHECK(brt_rect_zero(&bb));
    EXPECT_EQ(brt_hash(&c), h0);
    brt_close(&c);
}

TEST(brt_dv_nan_origin) {
    BRT_FIXTURE(c, 40, 40); brt_run r; uint64_t h0; RECT bb = brt_sentinel();
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 20.0f); h0 = brt_hash(&c);
    CHECK_HR(brt_draw_run(&c, &r, NAN, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], 0xFFFFFF, &bb), E_INVALIDARG);
    CHECK(brt_rect_zero(&bb));
    bb = brt_sentinel();
    CHECK_HR(brt_draw_run(&c, &r, 5, NAN, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], 0xFFFFFF, &bb), E_INVALIDARG);
    CHECK(brt_rect_zero(&bb));
    CHECK_HR(brt_draw_run(&c, &r, NAN, NAN, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], 0xFFFFFF, NULL), E_INVALIDARG);
    EXPECT_EQ(brt_hash(&c), h0);
    brt_close(&c);
}

TEST(brt_dv_null_font_face) {
    if (t_is_wine()) { t_skip("NULL/invalid pointers crash the non-Windows implementation"); return; }
    BRT_FIXTURE(c, 40, 40); brt_run r; RECT bb = brt_sentinel();
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 20.0f); r.run.fontFace = NULL;
    CHECK_HR(brt_draw_run(&c, &r, 5, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], 0xFFFFFF, &bb), E_INVALIDARG);
    CHECK(brt_rect_zero(&bb));
    EXPECT_EQ(brt_count_not(&c, 0), 0);
    brt_close(&c);
}

TEST(brt_dv_null_glyph_indices_with_count) {
    if (t_is_wine()) { t_skip("NULL/invalid pointers crash the non-Windows implementation"); return; }
    BRT_FIXTURE(c, 40, 40); brt_run r; RECT bb = brt_sentinel();
    if (!c.rt) return;
    brt_make_run(&c, &r, "AB", 20.0f); r.run.glyphIndices = NULL;
    CHECK_HR(brt_draw_run(&c, &r, 5, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], 0xFFFFFF, &bb), E_INVALIDARG);
    CHECK(brt_rect_zero(&bb));
    EXPECT_EQ(brt_count_not(&c, 0), 0);
    brt_close(&c);
}

TEST(brt_dv_negative_em_size) {
    static const float v[] = { -1.0f, -0.001f, -100.0f, -1e30f, -HUGE_VALF };
    BRT_FIXTURE(c, 40, 40); brt_run r; int i;
    if (!c.rt) return;
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        RECT bb = brt_sentinel(); HRESULT hr;
        brt_make_run(&c, &r, "A", 20.0f); r.run.fontEmSize = v[i];
        hr = brt_draw_run(&c, &r, 5, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], 0xFFFFFF, &bb);
        t_check(hr == E_INVALIDARG && brt_rect_zero(&bb), __FILE__, __LINE__, "negative em", "em %g -> %s", v[i], dw_hr_name(hr));
    }
    EXPECT_EQ(brt_count_not(&c, 0), 0);
    brt_close(&c);
}

TEST(brt_dv_invalid_leaves_all_pixels) {
    if (t_is_wine()) { t_skip("NULL/invalid pointers crash the non-Windows implementation"); return; }
    BRT_FIXTURE(c, 60, 60); brt_run r; uint64_t h0;
    if (!c.rt) return;
    brt_fill(&c, 0x00123456); h0 = brt_hash(&c);
    brt_make_run(&c, &r, "A", 30.0f);
    IDWriteBitmapRenderTarget_DrawGlyphRun(c.rt, 5, 50, DWRITE_MEASURING_MODE_NATURAL, NULL, c.rp_mode[1], K, NULL);
    brt_draw_run(&c, &r, 5, 50, DWRITE_MEASURING_MODE_NATURAL, NULL, K, NULL);
    brt_draw_run(&c, &r, NAN, 50, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL);
    r.run.fontEmSize = -3;
    brt_draw_run(&c, &r, 5, 50, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL);
    EXPECT_EQ(brt_hash(&c), h0);
    brt_close(&c);
}

TEST(brt_dv_null_blackbox_ok) {
    BRT_FIXTURE(c, 60, 60);
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 30.0f, 5.0f, 50.0f, K, NULL));
    CHECK(brt_count_not(&c, W) > 100);
    brt_close(&c);
}

TEST(brt_dv_blackbox_overwritten_on_success) {
    BRT_FIXTURE(c, 80, 80); RECT bb = brt_sentinel();
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 40.0f, 10.0f, 60.0f, K, &bb));
    CHECK(!(bb.left == 0x1111 && bb.right == 0x3333));
    CHECK(bb.right > bb.left && bb.bottom > bb.top);
    brt_close(&c);
}

TEST(brt_dv_rendering_params_default_object) {
    BRT_FIXTURE(c, 80, 80); RECT bb;
    if (!c.rt) return;
    if (!c.rp_default) { brt_close(&c); t_skip("no default params"); return; }
    brt_fill(&c, W);
    brt_run r; brt_make_run(&c, &r, "A", 48.0f);
    CHECK_HR_OK(brt_draw_run(&c, &r, 10, 60, DWRITE_MEASURING_MODE_NATURAL, c.rp_default, K, &bb));
    CHECK(brt_near(brt_px(&c, 10 + 14, 60 - 20), K, 3));
    EXPECT_EQ(brt_px(&c, 2, 2), W);
    brt_close(&c);
}

TEST(brt_dv_zero_glyph_count_with_indices) {
    BRT_FIXTURE(c, 40, 40); brt_run r; RECT bb = brt_sentinel(); uint64_t h0;
    if (!c.rt) return;
    brt_fill(&c, W); h0 = brt_hash(&c);
    brt_make_run(&c, &r, "A", 20.0f); r.run.glyphCount = 0;
    CHECK_HR_OK(brt_draw_run(&c, &r, 5, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, &bb));
    EXPECT_EQ(brt_hash(&c), h0);
    t_note("zero glyph blackBox = {%ld,%ld,%ld,%ld}", bb.left, bb.top, bb.right, bb.bottom);
    brt_close(&c);
}

TEST(brt_dv_zero_glyph_count_null_indices) {
    BRT_FIXTURE(c, 40, 40); brt_run r; RECT bb = brt_sentinel(); HRESULT hr; uint64_t h0;
    if (!c.rt) return;
    brt_fill(&c, W); h0 = brt_hash(&c);
    brt_make_run(&c, &r, "A", 20.0f); r.run.glyphCount = 0; r.run.glyphIndices = NULL; r.run.glyphAdvances = NULL;
    hr = brt_draw_run(&c, &r, 5, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, &bb);
    t_note("zero glyphs, NULL arrays -> %s", dw_hr_name(hr));
    CHECK(hr == S_OK || hr == E_INVALIDARG);
    if (hr == S_OK) EXPECT_EQ(brt_hash(&c), h0);
    brt_close(&c);
}

TEST(brt_dv_zero_glyph_all_rendering_modes) {
    BRT_FIXTURE(c, 40, 40); brt_run r; int m, a; uint64_t h0;
    if (!c.rt) return;
    brt_fill(&c, W); h0 = brt_hash(&c);
    brt_make_run(&c, &r, "A", 20.0f); r.run.glyphCount = 0;
    for (a = 0; a < 2 && c.rt1; a++) {
        IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, (DWRITE_TEXT_ANTIALIAS_MODE)a);
        for (m = 0; m < 7; m++) {
            RECT bb = brt_sentinel(); HRESULT hr = brt_draw_run(&c, &r, 5, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[m], K, &bb);
            t_check(hr == S_OK, __FILE__, __LINE__, "zero glyphs", "mode %d aa %d -> %s", m, a, dw_hr_name(hr));
            t_note("zero glyphs mode %d aa %d box {%ld,%ld,%ld,%ld}", m, a, bb.left, bb.top, bb.right, bb.bottom);
        }
    }
    EXPECT_EQ(brt_hash(&c), h0);
    brt_close(&c);
}

TEST(brt_dv_empty_glyphs_space_and_empty500) {
    BRT_FIXTURE(c, 60, 40); brt_run r; RECT bb; uint64_t h0; int g;
    if (!c.rt) return;
    brt_fill(&c, W); h0 = brt_hash(&c);
    for (g = 0; g < 2; g++) {
        brt_make_run(&c, &r, "A", 30.0f); r.idx[0] = g ? TTF_GID_EMPTY500 : TTF_GID_SPACE;
        bb = brt_sentinel();
        CHECK_HR_OK(brt_draw_run(&c, &r, 10, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, &bb));
        t_note("empty glyph %d blackBox {%ld,%ld,%ld,%ld}", g, bb.left, bb.top, bb.right, bb.bottom);
    }
    EXPECT_EQ(brt_hash(&c), h0);
    brt_close(&c);
}

/* ---- the box glyph ---------------------------------------------------------------------------------------------- */
TEST(brt_box_black_on_white) {
    BRT_FIXTURE(c, 100, 100); int ib, ob; RECT bb;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, K, &bb));
    brt_box_check(&c, 64.0f, 10.0f, 80.0f, K, W, 2, 0, &ib, &ob);
    EXPECT_EQ(ib, 0); EXPECT_EQ(ob, 0);
    brt_close(&c);
}

TEST(brt_box_white_on_black_initial_bitmap) {
    BRT_FIXTURE(c, 100, 100); int ib, ob;
    if (!c.rt) return;
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, W, NULL));      /* bitmap is zero (black) initially */
    brt_box_check(&c, 64.0f, 10.0f, 80.0f, W, K, 2, 0, &ib, &ob);
    EXPECT_EQ(ib, 0); EXPECT_EQ(ob, 0);
    brt_close(&c);
}

TEST(brt_box_colour_channel_order) {
    static const uint32_t col[] = { 0xFF0000, 0x00FF00, 0x0000FF, 0x123456, 0xFEDCBA, 0x010203, 0xFFFF00, 0x00FFFF, 0xFF00FF, 0x808080, 0x7F7F7F };
    BRT_FIXTURE(c, 100, 100); int i;
    if (!c.rt) return;
    for (i = 0; i < (int)(sizeof col / sizeof col[0]); i++) {
        int ib, ob;
        brt_fill(&c, W == col[i] ? K : W);
        CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, col[i], NULL));    /* COLORREF built with RGB() from the xRGB value */
        brt_box_check(&c, 64.0f, 10.0f, 80.0f, col[i], W, 2, 0, &ib, &ob);
        t_check(ib == 0, __FILE__, __LINE__, "interior colour", "colour %06x: %d bad", col[i], ib);
        EXPECT_EQ(brt_px(&c, 10 + 14, 80 - 20), col[i]);
    }
    brt_close(&c);
}

TEST(brt_box_colorref_raw_values) {
    /* COLORREF 0x00BBGGRR: red byte is the low byte */
    BRT_FIXTURE(c, 100, 100); brt_run r;
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 64.0f);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_DrawGlyphRun(c.rt, 10, 80, DWRITE_MEASURING_MODE_NATURAL, &r.run, c.rp_mode[1], 0x000000FF, NULL));
    EXPECT_EQ(brt_px(&c, 24, 60), 0xFF0000);
    brt_fill(&c, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_DrawGlyphRun(c.rt, 10, 80, DWRITE_MEASURING_MODE_NATURAL, &r.run, c.rp_mode[1], 0x00FF0000, NULL));
    EXPECT_EQ(brt_px(&c, 24, 60), 0x0000FF);
    brt_close(&c);
}

TEST(brt_box_colorref_high_byte_ignored) {
    BRT_FIXTURE(c, 100, 100); brt_run r; uint64_t h1, h2; int i;
    static const COLORREF hb[] = { 0x01000000, 0x80000000, 0xFF000000, 0xAB000000 };
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 64.0f);
    brt_fill(&c, W);
    IDWriteBitmapRenderTarget_DrawGlyphRun(c.rt, 10, 80, DWRITE_MEASURING_MODE_NATURAL, &r.run, c.rp_mode[1], RGB(0x20, 0x40, 0x60), NULL);
    h1 = brt_hash(&c);
    for (i = 0; i < 4; i++) {
        brt_fill(&c, W);
        CHECK_HR_OK(IDWriteBitmapRenderTarget_DrawGlyphRun(c.rt, 10, 80, DWRITE_MEASURING_MODE_NATURAL, &r.run, c.rp_mode[1], RGB(0x20, 0x40, 0x60) | hb[i], NULL));
        h2 = brt_hash(&c);
        t_check(h1 == h2, __FILE__, __LINE__, "high byte ignored", "high byte %08lx changed the result", (unsigned long)hb[i]);
    }
    brt_close(&c);
}

TEST(brt_box_on_coloured_background) {
    BRT_FIXTURE(c, 100, 100); int ib, ob;
    if (!c.rt) return;
    brt_fill(&c, 0x336699);
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, 0xFFCC00, NULL));
    brt_box_check(&c, 64.0f, 10.0f, 80.0f, 0xFFCC00, 0x336699, 2, 0, &ib, &ob);
    EXPECT_EQ(ib, 0); EXPECT_EQ(ob, 0);
    brt_close(&c);
}

TEST(brt_box_gdi_prefilled_background) {
    /* background painted through GDI FillRect, as an application would */
    BRT_FIXTURE(c, 100, 100); int ib, ob; RECT all = { 0, 0, 100, 100 }; HBRUSH br = CreateSolidBrush(RGB(0xF0, 0xF0, 0xE0));
    if (!c.rt) return;
    FillRect(c.dc, &all, br);
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, 0x102030, NULL));
    brt_box_check(&c, 64.0f, 10.0f, 80.0f, 0x102030, 0xF0F0E0, 2, 0, &ib, &ob);
    EXPECT_EQ(ib, 0); EXPECT_EQ(ob, 0);
    DeleteObject(br); brt_close(&c);
}

TEST(brt_box_aliased_only_two_colours) {
    BRT_FIXTURE(c, 100, 100); int w, h, i, other = 0; uint32_t *b;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 10.5f, 80.25f, 0x204060, NULL));
    b = brt_bits(&c, &w, &h);
    for (i = 0; b && i < w * h; i++) { uint32_t v = b[i] & 0xFFFFFF; if (v != W && v != 0x204060) other++; }
    EXPECT_EQ(other, 0);
    brt_close(&c);
}

TEST(brt_box_aliased_area_close_to_geometry) {
    BRT_FIXTURE(c, 100, 100); float em = 64.0f, ww, hh; int n, lo, hi;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", em, 10.0f, 80.0f, K, NULL));
    n = brt_count_not(&c, W);
    ww = BRT_BX1(em) - BRT_BX0(em); hh = BRT_BY1(em);
    lo = (int)((ww - 2) * (hh - 2)); hi = (int)((ww + 2) * (hh + 2));
    t_check(n >= lo && n <= hi, __FILE__, __LINE__, "covered pixel count", "got %d want in [%d,%d] (ideal %g)", n, lo, hi, ww * hh);
    brt_close(&c);
}

TEST(brt_box_aliased_bbox_close_to_geometry) {
    BRT_FIXTURE(c, 100, 100); RECT r; float em = 64.0f, ox = 10.0f, oy = 80.0f;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", em, ox, oy, K, NULL));
    CHECK(brt_bbox_not(&c, W, &r));
    CHECK(labs(r.left - (long)floorf(ox + BRT_BX0(em))) <= 2);
    CHECK(labs(r.right - (long)ceilf(ox + BRT_BX1(em))) <= 2);
    CHECK(labs(r.top - (long)floorf(oy - BRT_BY1(em))) <= 2);
    CHECK(labs(r.bottom - (long)ceilf(oy)) <= 2);
    t_note("box pixels {%ld,%ld,%ld,%ld}; ideal x %g..%g y %g..%g", r.left, r.top, r.right, r.bottom, ox + BRT_BX0(em), ox + BRT_BX1(em), oy - BRT_BY1(em), oy);
    brt_close(&c);
}

TEST(brt_box_blackbox_contains_changed_pixels) {
    BRT_FIXTURE(c, 100, 100); RECT bb, r; int m;
    if (!c.rt) return;
    for (m = 1; m < 7; m++) {      /* explicit rendering modes 1..6 */
        brt_run run; brt_make_run(&c, &run, "A", 64.0f);
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw_run(&c, &run, 10, 80, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[m], K, &bb));
        if (brt_bbox_not(&c, W, &r))
            t_check(bb.left <= r.left && bb.top <= r.top && bb.right >= r.right && bb.bottom >= r.bottom, __FILE__, __LINE__, "blackBox contains drawn pixels",
                    "mode %d: box {%ld,%ld,%ld,%ld} pixels {%ld,%ld,%ld,%ld}", m, bb.left, bb.top, bb.right, bb.bottom, r.left, r.top, r.right, r.bottom);
        else t_check(0, __FILE__, __LINE__, "something drawn", "mode %d drew nothing", m);
    }
    brt_close(&c);
}

TEST(brt_box_blackbox_translates_with_origin) {
    BRT_FIXTURE(c, 200, 200); RECT a, b; static const int d[][2] = { {1,0},{0,1},{7,5},{30,2},{3,60} }; int i;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 48.0f, 10.0f, 70.0f, K, &a));
    for (i = 0; i < 5; i++) {
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw(&c, "A", 48.0f, 10.0f + d[i][0], 70.0f + d[i][1], K, &b));
        t_check(b.left - a.left == d[i][0] && b.right - a.right == d[i][0] && b.top - a.top == d[i][1] && b.bottom - a.bottom == d[i][1],
                __FILE__, __LINE__, "blackBox translation", "shift (%d,%d): {%ld,%ld,%ld,%ld} vs {%ld,%ld,%ld,%ld}", d[i][0], d[i][1], b.left, b.top, b.right, b.bottom, a.left, a.top, a.right, a.bottom);
    }
    brt_close(&c);
}

TEST(brt_box_blackbox_matches_pixels_roughly) {
    BRT_FIXTURE(c, 100, 100); RECT bb, r;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, K, &bb));
    CHECK(brt_bbox_not(&c, W, &r));
    CHECK(labs(bb.left - r.left) <= 3 && labs(bb.right - r.right) <= 3 && labs(bb.top - r.top) <= 3 && labs(bb.bottom - r.bottom) <= 3);
    t_note("blackBox {%ld,%ld,%ld,%ld} pixels {%ld,%ld,%ld,%ld}", bb.left, bb.top, bb.right, bb.bottom, r.left, r.top, r.right, r.bottom);
    brt_close(&c);
}

TEST(brt_box_null_blackbox_same_pixels) {
    BRT_FIXTURE(c, 100, 100); uint64_t h1, h2; RECT bb;
    if (!c.rt) return;
    brt_fill(&c, W); brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, K, &bb); h1 = brt_hash(&c);
    brt_fill(&c, W); brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, K, NULL); h2 = brt_hash(&c);
    CHECK(h1 == h2);
    brt_close(&c);
}

TEST(brt_box_em_size_table) {
    static const float em[] = { 12, 16, 20, 24, 32, 40, 48, 64, 80, 100, 128, 150 };
    BRT_FIXTURE(c, 200, 200); int i;
    if (!c.rt) return;
    for (i = 0; i < (int)(sizeof em / sizeof em[0]); i++) {
        int ib, ob; float ox = 20.0f, oy = 180.0f;
        if (brt_box_interior_empty(em[i], 2)) continue;
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw(&c, "A", em[i], ox, oy, K, NULL));
        brt_box_check(&c, em[i], ox, oy, K, W, 2, 0, &ib, &ob);
        t_check(ib == 0 && ob == 0, __FILE__, __LINE__, "box at em", "em %g: interior bad %d outside bad %d", em[i], ib, ob);
    }
    brt_close(&c);
}

TEST(brt_box_integer_origin_translation_invariance) {
    /* an integer shift of the origin shifts the aliased bitmap exactly */
    BRT_FIXTURE(c, 160, 160); uint32_t *ref; int w, h, x, y, i, bad = 0; static const int d[][2] = { {1,0},{0,1},{13,9},{-5,4},{20,-7} };
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 50.0f, 60.0f, 90.0f, K, NULL));
    ref = (uint32_t *)malloc(160 * 160 * 4); brt_bits(&c, &w, &h); memcpy(ref, brt_bits(&c, &w, &h), (size_t)w * h * 4);
    for (i = 0; i < 5; i++) {
        brt_fill(&c, W);
        brt_draw(&c, "A", 50.0f, 60.0f + d[i][0], 90.0f + d[i][1], K, NULL);
        for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
            int sx = x - d[i][0], sy = y - d[i][1];
            if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
            if ((brt_px(&c, x, y)) != (ref[sy * w + sx] & 0xFFFFFF)) bad++;
        }
    }
    EXPECT_EQ(bad, 0);
    free(ref); brt_close(&c);
}

TEST(brt_box_fractional_origin_stays_close) {
    static const float fx[] = { 0.0f, 0.25f, 0.5f, 0.75f, 0.99f };
    BRT_FIXTURE(c, 120, 120); int i;
    if (!c.rt) return;
    for (i = 0; i < 5; i++) {
        RECT r; float ox = 20.0f + fx[i], oy = 90.0f + fx[4 - i];
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw(&c, "A", 64.0f, ox, oy, K, NULL));
        CHECK(brt_bbox_not(&c, W, &r));
        t_check(labs(r.left - (long)floorf(ox + BRT_BX0(64))) <= 2 && labs(r.right - (long)ceilf(ox + BRT_BX1(64))) <= 2 &&
                labs(r.top - (long)floorf(oy - BRT_BY1(64))) <= 2 && labs(r.bottom - (long)ceilf(oy)) <= 2, __FILE__, __LINE__, "fractional origin", "origin %g,%g -> {%ld,%ld,%ld,%ld}", ox, oy, r.left, r.top, r.right, r.bottom);
    }
    brt_close(&c);
}

/* ---- several glyphs -------------------------------------------------------------------------------------------- */
TEST(brt_run_two_glyphs_default_advances) {
    BRT_FIXTURE(c, 200, 100); float em = 48.0f, ox = 10.0f, oy = 70.0f; int g, bad = 0;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "AB", em, ox, oy, K, NULL));
    for (g = 0; g < 2; g++) {
        int ib, ob; (void)ob;
        bad += brt_rect_mismatch(&c, (int)ceilf(ox + g * BRT_ADV(em) + BRT_BX0(em)) + 2, (int)ceilf(oy - BRT_BY1(em)) + 2,
                                 (int)floorf(ox + g * BRT_ADV(em) + BRT_BX1(em)) - 2, (int)floorf(oy) - 2, K, 0);
    }
    EXPECT_EQ(bad, 0);
    /* gap between the boxes untouched */
    EXPECT_EQ(brt_px(&c, (int)(ox + BRT_ADV(em) + BRT_BX0(em) - 4), (int)(oy - 20)), W);
    brt_close(&c);
}

TEST(brt_run_custom_advances_honoured) {
    BRT_FIXTURE(c, 300, 100); brt_run r; float em = 32.0f, ox = 10.0f, oy = 70.0f; int g, bad = 0;
    static const float adv[4] = { 50.0f, 20.0f, 70.0f, 10.0f };
    float pen = ox;
    if (!c.rt) return;
    brt_make_run(&c, &r, "ABCD", em);
    memcpy(r.adv, adv, sizeof adv);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, ox, oy, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    for (g = 0; g < 4; g++) {
        bad += brt_rect_mismatch(&c, (int)ceilf(pen + BRT_BX0(em)) + 2, (int)ceilf(oy - BRT_BY1(em)) + 2, (int)floorf(pen + BRT_BX1(em)) - 2, (int)floorf(oy) - 2, K, 0);
        pen += adv[g];
    }
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brt_run_glyph_offsets) {
    /* advanceOffset moves right, ascenderOffset moves up (pre-transform), without moving the pen */
    BRT_FIXTURE(c, 200, 120); brt_run r; float em = 40.0f, ox = 20.0f, oy = 90.0f; int bad = 0;
    if (!c.rt) return;
    brt_make_run(&c, &r, "AB", em);
    r.off[0].advanceOffset = 15.0f; r.off[0].ascenderOffset = 10.0f;
    r.off[1].advanceOffset = -5.0f; r.off[1].ascenderOffset = -8.0f;
    r.run.glyphOffsets = r.off;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, ox, oy, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    bad += brt_rect_mismatch(&c, (int)ceilf(ox + 15 + BRT_BX0(em)) + 2, (int)ceilf(oy - 10 - BRT_BY1(em)) + 2, (int)floorf(ox + 15 + BRT_BX1(em)) - 2, (int)floorf(oy - 10) - 2, K, 0);
    bad += brt_rect_mismatch(&c, (int)ceilf(ox + BRT_ADV(em) - 5 + BRT_BX0(em)) + 2, (int)ceilf(oy + 8 - BRT_BY1(em)) + 2, (int)floorf(ox + BRT_ADV(em) - 5 + BRT_BX1(em)) - 2, (int)floorf(oy + 8) - 2, K, 0);
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brt_run_many_glyphs) {
    BRT_FIXTURE(c, 640, 60); int g, bad = 0; float em = 16.0f, ox = 4.0f, oy = 40.0f;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", em, ox, oy, K, NULL));
    for (g = 0; g < 62; g++)
        bad += brt_rect_mismatch(&c, (int)ceilf(ox + g * BRT_ADV(em) + BRT_BX0(em)) + 1, (int)ceilf(oy - BRT_BY1(em)) + 1, (int)floorf(ox + g * BRT_ADV(em) + BRT_BX1(em)) - 1, (int)floorf(oy) - 1, K, 0);
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brt_run_two_calls_equal_one_call) {
    BRT_FIXTURE(c, 200, 100); uint64_t h1, h2; float em = 40.0f;
    if (!c.rt) return;
    brt_fill(&c, W); brt_draw(&c, "AB", em, 10, 70, K, NULL); h1 = brt_hash(&c);
    brt_fill(&c, W);
    brt_draw(&c, "A", em, 10, 70, K, NULL);
    brt_draw(&c, "B", em, 10 + BRT_ADV(em), 70, K, NULL);
    h2 = brt_hash(&c);
    t_check(h1 == h2, __FILE__, __LINE__, "split run", "hash %llx vs %llx", (unsigned long long)h1, (unsigned long long)h2);
    brt_close(&c);
}

/* ---- special glyph shapes ---------------------------------------------------------------------------------------- */
TEST(brt_glyph_notdef_box) {
    BRT_FIXTURE(c, 160, 120); float em = 64.0f; int bad = 0;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_gid(&c, TTF_GID_NOTDEF, em, 10, 90, K, NULL, 1));
    /* .notdef box is (100,0)-(1100,1400) */
    bad += brt_rect_mismatch(&c, (int)(10 + 100 * em / 2048) + 2, (int)(90 - 1400 * em / 2048) + 2, (int)(10 + 1100 * em / 2048) - 2, 88, K, 0);
    EXPECT_EQ(bad, 0);
    EXPECT_EQ(brt_px(&c, (int)(10 + 1100 * em / 2048) + 3, 60), W);
    brt_close(&c);
}

TEST(brt_glyph_triangle_rows_widen) {
    /* (100,0) (500,1400) (900,0): row width grows linearly from the apex to the base */
    BRT_FIXTURE(c, 160, 160); float em = 128.0f, ox = 10.0f, oy = 140.0f; int y, prev = -1, mono = 1, base_w, top_w;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_gid(&c, TTF_GID_TRIANGLE, em, ox, oy, K, NULL, 1));
    for (y = (int)(oy - BRT_BY1(em)) + 1; y < (int)oy; y++) {
        int x, n = 0;
        for (x = 0; x < 160; x++) if (brt_px(&c, x, y) != W) n++;
        if (prev >= 0 && n + 1 < prev) mono = 0;
        prev = n;
    }
    CHECK(mono);
    { int x, n = 0; for (x = 0; x < 160; x++) if (brt_px(&c, x, (int)oy - 2) != W) n++; base_w = n; }
    { int x, n = 0; for (x = 0; x < 160; x++) if (brt_px(&c, x, (int)(oy - BRT_BY1(em)) + 6) != W) n++; top_w = n; }
    CHECK(labs(base_w - (long)(BRT_BX1(em) - BRT_BX0(em))) <= 3);
    CHECK(top_w <= 6);
    /* the apex column is covered, the lower-left corner region above the base is not */
    EXPECT_EQ(brt_px(&c, (int)(ox + 500 * em / 2048), (int)oy - 20), K);
    EXPECT_EQ(brt_px(&c, (int)(ox + 100 * em / 2048) + 3, (int)(oy - BRT_BY1(em) / 2)), W);
    brt_close(&c);
}

TEST(brt_glyph_ring_hole_not_painted) {
    /* outer box (100,0)-(900,1400), hole (300,300)-(700,1100) with opposite winding */
    BRT_FIXTURE(c, 160, 160); float em = 128.0f, ox = 10.0f, oy = 140.0f; int m;
    if (!c.rt) return;
    for (m = 1; m < 7; m++) {
        if (m == 2 || m == 3) continue;
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw_gid(&c, TTF_GID_RING, em, ox, oy, K, NULL, m));
        t_check(brt_near(brt_px(&c, (int)(ox + 500 * em / 2048), (int)(oy - 700 * em / 2048)), W, 3), __FILE__, __LINE__, "hole centre", "mode %d", m);
        t_check(brt_near(brt_px(&c, (int)(ox + 200 * em / 2048), (int)(oy - 700 * em / 2048)), K, 3), __FILE__, __LINE__, "left band", "mode %d", m);
        t_check(brt_near(brt_px(&c, (int)(ox + 500 * em / 2048), (int)(oy - 150 * em / 2048)), K, 3), __FILE__, __LINE__, "bottom band", "mode %d", m);
        t_check(brt_near(brt_px(&c, (int)(ox + 500 * em / 2048), (int)(oy - 1250 * em / 2048)), K, 3), __FILE__, __LINE__, "top band", "mode %d", m);
    }
    brt_close(&c);
}

TEST(brt_glyph_composite_two_parts) {
    /* 'A' box plus the triangle stacked on top (dy = 1400): total height 2800 units */
    BRT_FIXTURE(c, 160, 220); float em = 64.0f, ox = 10.0f, oy = 200.0f;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_gid(&c, TTF_GID_COMPOSITE, em, ox, oy, K, NULL, 1));
    EXPECT_EQ(brt_px(&c, (int)(ox + 500 * em / 2048), (int)(oy - 700 * em / 2048)), K);       /* box part */
    EXPECT_EQ(brt_px(&c, (int)(ox + 500 * em / 2048), (int)(oy - 2000 * em / 2048)), K);      /* triangle part, above 1400 */
    EXPECT_EQ(brt_px(&c, (int)(ox + 150 * em / 2048), (int)(oy - 2000 * em / 2048)), W);      /* outside the triangle's slope */
    brt_close(&c);
}

TEST(brt_glyph_round_centre_filled) {
    BRT_FIXTURE(c, 160, 160); float em = 128.0f, ox = 10.0f, oy = 140.0f;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_gid(&c, TTF_GID_ROUND, em, ox, oy, K, NULL, 1));
    EXPECT_EQ(brt_px(&c, (int)(ox + 500 * em / 2048), (int)(oy - 700 * em / 2048)), K);
    CHECK(brt_count_not(&c, W) > 500);
    brt_close(&c);
}

/* ---- overdraw --------------------------------------------------------------------------------------------------- */
TEST(brt_overdraw_same_twice_idempotent) {
    BRT_FIXTURE(c, 100, 100); uint64_t h1;
    if (!c.rt) return;
    brt_fill(&c, W);
    brt_draw(&c, "A", 64.0f, 10, 80, K, NULL); h1 = brt_hash(&c);
    brt_draw(&c, "A", 64.0f, 10, 80, K, NULL);
    EXPECT_EQ(brt_hash(&c), h1);
    brt_close(&c);
}

TEST(brt_overdraw_second_colour_replaces_interior) {
    BRT_FIXTURE(c, 100, 100);
    if (!c.rt) return;
    brt_fill(&c, W);
    brt_draw(&c, "A", 64.0f, 10, 80, 0xFF0000, NULL);
    brt_draw(&c, "A", 64.0f, 10, 80, 0x0000FF, NULL);
    EXPECT_EQ(brt_px(&c, 24, 60), 0x0000FF);
    brt_draw(&c, "A", 64.0f, 10, 80, 0x00FF00, NULL);
    EXPECT_EQ(brt_px(&c, 24, 60), 0x00FF00);
    brt_close(&c);
}

TEST(brt_overdraw_overlapping_boxes) {
    BRT_FIXTURE(c, 120, 100);
    if (!c.rt) return;
    brt_fill(&c, W);
    brt_draw(&c, "A", 64.0f, 10, 80, 0xFF0000, NULL);
    brt_draw(&c, "A", 64.0f, 30, 80, 0x0000FF, NULL);          /* overlaps the first box by 5 pixels */
    EXPECT_EQ(brt_px(&c, 16, 60), 0xFF0000);                    /* only first */
    EXPECT_EQ(brt_px(&c, 38, 60), 0x0000FF);                    /* overlap region: later draw wins */
    EXPECT_EQ(brt_px(&c, 50, 60), 0x0000FF);
    brt_close(&c);
}

TEST(brt_overdraw_aa_blends_over_existing) {
    /* anti-aliased text composites over what is there: an edge pixel is neither the old nor the new colour */
    BRT_FIXTURE(c, 100, 100); int w, h, i, mixed = 0; uint32_t *b;
    if (!c.rt) return;
    brt_fill(&c, W);
    {
        brt_run r; brt_make_run(&c, &r, "A", 31.7f);
        CHECK_HR_OK(brt_draw_run(&c, &r, 10.3f, 60.4f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC], K, NULL));
    }
    b = brt_bits(&c, &w, &h);
    for (i = 0; b && i < w * h; i++) { uint32_t v = b[i] & 0xFFFFFF; if (v != W && v != K) mixed++; }
    CHECK(mixed > 0);
    brt_close(&c);
}

/* ---- clipping / off-bitmap ---------------------------------------------------------------------------------------- */
TEST(brt_clip_partial_equals_reference_window) {
    /* drawing partly outside the bitmap equals the same drawing on a larger bitmap, cropped */
    static const int off[][2] = { {-20, 0}, {0, -20}, {-30, -30}, {30, 0}, {0, 30}, {35, 35}, {-10, 25}, {25, -10} };
    int i;
    for (i = 0; i < 8; i++) {
        brt_ctx s, big; int x, y, bad = 0; float ox = 10.0f + off[i][0], oy = 40.0f + off[i][1];
        brt_open(&s, NULL, 60, 60, 1); brt_open(&big, NULL, 260, 260, 1);
        brt_fill(&s, W); brt_fill(&big, W);
        CHECK_HR_OK(brt_draw(&s, "A", 48.0f, ox, oy, K, NULL));
        CHECK_HR_OK(brt_draw(&big, "A", 48.0f, ox + 100, oy + 100, K, NULL));
        for (y = 0; y < 60; y++) for (x = 0; x < 60; x++) if (brt_px(&s, x, y) != brt_px(&big, x + 100, y + 100)) bad++;
        t_check(bad == 0, __FILE__, __LINE__, "clipped == cropped reference", "offset (%d,%d): %d pixels differ", off[i][0], off[i][1], bad);
        brt_close(&s); brt_close(&big);
    }
}

TEST(brt_clip_fully_outside_draws_nothing) {
    static const float pos[][2] = { {-200, 30}, {300, 30}, {10, -200}, {10, 300}, {-500, -500}, {500, 500}, {-48, 30}, {60, 30} };
    BRT_FIXTURE(c, 60, 60); int i; uint64_t h0;
    if (!c.rt) return;
    brt_fill(&c, W); h0 = brt_hash(&c);
    for (i = 0; i < 8; i++) {
        RECT bb = brt_sentinel(); HRESULT hr = brt_draw(&c, "A", 20.0f, pos[i][0], pos[i][1], K, &bb);
        t_check(hr == S_OK, __FILE__, __LINE__, "off-bitmap draw", "(%g,%g) -> %s", pos[i][0], pos[i][1], dw_hr_name(hr));
        t_check(brt_hash(&c) == h0, __FILE__, __LINE__, "bitmap untouched", "(%g,%g)", pos[i][0], pos[i][1]);
        t_note("off-bitmap (%g,%g) blackBox {%ld,%ld,%ld,%ld}", pos[i][0], pos[i][1], bb.left, bb.top, bb.right, bb.bottom);
    }
    brt_close(&c);
}

TEST(brt_clip_far_coordinates_succeed) {
    static const float v[] = { -10000, 10000, -100000, 100000 };
    BRT_FIXTURE(c, 40, 40); int i, j; uint64_t h0;
    if (!c.rt) return;
    brt_fill(&c, W); h0 = brt_hash(&c);
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
        HRESULT hr = brt_draw(&c, "A", 20.0f, v[i], v[j], K, NULL);
        if (i < 2 && j < 2) t_check(hr == S_OK, __FILE__, __LINE__, "far origin", "(%g,%g) -> %s", v[i], v[j], dw_hr_name(hr));
        else t_note("far origin (%g,%g) -> %s", v[i], v[j], dw_hr_name(hr));
    }
    EXPECT_EQ(brt_hash(&c), h0);
    brt_close(&c);
}

TEST(brt_clip_glyph_larger_than_bitmap) {
    BRT_FIXTURE(c, 30, 30);
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 400.0f, -20.0f, 100.0f, K, NULL));
    EXPECT_EQ(brt_px(&c, 15, 15), K);               /* box (-20+19.5 .. -20+175.8) x (100-273 .. 100) covers the whole 30x30 */
    EXPECT_EQ(brt_px(&c, 0, 0), K); EXPECT_EQ(brt_px(&c, 29, 29), K);
    brt_close(&c);
}

TEST(brt_clip_one_pixel_bitmap_covered) {
    BRT_FIXTURE(c, 1, 1);
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 100.0f, -10.0f, 40.0f, K, NULL));
    EXPECT_EQ(brt_px(&c, 0, 0), K);
    brt_close(&c);
}

TEST(brt_clip_dc_clip_region_ignored_by_aliased_raster) {
    /* the glyph run analysis writes straight into the DIB: a clip region set on the DC does not limit it */
    BRT_FIXTURE(c, 100, 100); int ib, ob;
    if (!c.rt) return;
    brt_fill(&c, W);
    IntersectClipRect(c.dc, 0, 0, 20, 20);
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, K, NULL));
    SelectClipRgn(c.dc, NULL);
    brt_box_check(&c, 64.0f, 10.0f, 80.0f, K, W, 2, 0, &ib, &ob);
    EXPECT_EQ(ib, 0); EXPECT_EQ(ob, 0);
    brt_close(&c);
}

/* ---- DC state ------------------------------------------------------------------------------------------------------- */
TEST(brt_dcstate_restored_after_draw_all_modes) {
    BRT_FIXTURE(c, 100, 100); int m, a; XFORM x;
    if (!c.rt) return;
    for (a = 0; a < 2; a++) for (m = 0; m < 7; m++) {
        HGDIOBJ bmp = GetCurrentObject(c.dc, OBJ_BITMAP), brush = GetCurrentObject(c.dc, OBJ_BRUSH), pen = GetCurrentObject(c.dc, OBJ_PEN);
        COLORREF dcb = GetDCBrushColor(c.dc); brt_run r;
        if (c.rt1) IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, (DWRITE_TEXT_ANTIALIAS_MODE)a);
        brt_make_run(&c, &r, "A", 48.0f);
        CHECK_HR_OK(brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[m], 0x336699, (m & 1) ? NULL : &(RECT){0}));
        t_check(GetCurrentObject(c.dc, OBJ_BITMAP) == bmp, __FILE__, __LINE__, "bitmap", "mode %d aa %d", m, a);
        t_check(GetCurrentObject(c.dc, OBJ_BRUSH) == brush, __FILE__, __LINE__, "brush restored", "mode %d aa %d", m, a);
        t_check(GetCurrentObject(c.dc, OBJ_PEN) == pen, __FILE__, __LINE__, "pen", "mode %d aa %d", m, a);
        t_check(GetDCBrushColor(c.dc) == dcb, __FILE__, __LINE__, "DC brush colour restored", "mode %d aa %d: %lx vs %lx", m, a, (unsigned long)GetDCBrushColor(c.dc), (unsigned long)dcb);
        t_check(GetGraphicsMode(c.dc) == GM_ADVANCED, __FILE__, __LINE__, "graphics mode", "mode %d aa %d", m, a);
        memset(&x, 0, sizeof x); GetWorldTransform(c.dc, &x);
        t_check(x.eM11 == 1 && x.eM22 == 1 && x.eM12 == 0 && x.eM21 == 0 && x.eDx == 0 && x.eDy == 0, __FILE__, __LINE__, "world transform identity", "mode %d aa %d", m, a);
    }
    brt_close(&c);
}

TEST(brt_dcstate_gdi_object_count_stable_outline) {
    BRT_FIXTURE(c, 100, 100); DWORD before, after; int i;
    if (!c.rt) return;
    brt_draw(&c, "A", 48.0f, 10, 80, K, NULL);
    before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (i = 0; i < 40; i++) {
        brt_run r; RECT bb; brt_make_run(&c, &r, "AB", 30.0f);
        brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_OUTLINE], K, (i & 1) ? &bb : NULL);
        brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_NATURAL], K, &bb);
    }
    after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    t_check(after == before, __FILE__, __LINE__, "GDI objects", "before %lu after %lu", before, after);
    brt_close(&c);
}
