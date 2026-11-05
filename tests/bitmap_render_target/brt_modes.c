/* DrawGlyphRun: rendering modes x antialias modes x measuring modes, exact OUTLINE rasterisation, transforms, sideways and bidi
 * runs, pixels-per-DIP scaling, ClearType versus grayscale output, system fonts.
 *
 * Transform convention (pixels): a point p in DIPs (baseline origin + glyph coordinates, y down) lands at
 *   ppd * (m11*x + m21*y + dx, m12*x + m22*y + dy)
 * The baseline origin goes through the transform together with the glyph outline.  A sideways run first rotates every glyph 90 degrees
 * to the left about the baseline origin: (x,y) -> (y,-x).
 */
#include "brt.h"

#define W 0xFFFFFF
#define K 0x000000

static void brt_quad(const DWRITE_MATRIX *m, float ppd, float ox, float oy, float em, int sideways, float q[4][2]) {
    float lx[4] = { BRT_BX0(em), BRT_BX1(em), BRT_BX1(em), BRT_BX0(em) }, ly[4] = { 0, 0, -BRT_BY1(em), -BRT_BY1(em) };
    int i;
    for (i = 0; i < 4; i++) {
        float x = lx[i], y = ly[i], t;
        if (sideways) { t = x; x = y; y = -t; }
        x += ox; y += oy;
        q[i][0] = ppd * (m->m11 * x + m->m21 * y + m->dx);
        q[i][1] = ppd * (m->m12 * x + m->m22 * y + m->dy);
    }
}
/* pixels with centre at least `margin` inside the quad must be fg; pixels at least `margin` outside must be bg */
static void brt_quad_check(brt_ctx *c, const float q[4][2], float margin, uint32_t fg, uint32_t bg, int tol, int *in_bad, int *out_bad, int *in_n) {
    int w = 0, h = 0, x, y; uint32_t *b = brt_bits(c, &w, &h);
    *in_bad = *out_bad = *in_n = 0;
    if (!b) { *in_bad = *out_bad = -1; return; }
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        float d = brt_quad_depth(q, x + 0.5f, y + 0.5f); uint32_t v = b[y * w + x] & 0xFFFFFF;
        if (d >= margin) { (*in_n)++; if (!brt_near(v, fg, tol)) (*in_bad)++; }
        else if (d <= -margin) { if (v != bg) (*out_bad)++; }
    }
}

/* ---- rendering mode x antialias mode x measuring mode ------------------------------------------------------------ */
static void brt_matrix_for_aa(DWRITE_TEXT_ANTIALIAS_MODE aa) {
    BRT_FIXTURE(c, 120, 120); int rm, mm;
    static const DWRITE_MEASURING_MODE mms[3] = { DWRITE_MEASURING_MODE_NATURAL, DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_MEASURING_MODE_GDI_NATURAL };
    if (!c.rt) return;
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, aa));
    for (rm = 0; rm < 7; rm++) for (mm = 0; mm < 3; mm++) {
        brt_run r; RECT bb; HRESULT hr; int ib, ob; float ox = 14.0f, oy = 90.0f, em = 48.0f;
        brt_fill(&c, W);
        brt_make_run(&c, &r, "A", em);
        hr = brt_draw_run(&c, &r, ox, oy, mms[mm], c.rp_mode[rm], 0x204080, &bb);
        t_check(hr == S_OK, __FILE__, __LINE__, "DrawGlyphRun", "aa %d render %d measure %d -> %s", aa, rm, mm, dw_hr_name(hr));
        { int w = 0, h = 0, x, y; uint32_t *b = brt_bits(&c, &w, &h); ib = ob = 0;
          for (y = 0; b && y < h; y++) for (x = 0; x < w; x++) {
              uint32_t v = b[y * w + x] & 0xFFFFFF;
              int inside = x >= (int)ceilf(ox + BRT_BX0(em)) + 3 && x < (int)floorf(ox + BRT_BX1(em)) - 3 && y >= (int)ceilf(oy - BRT_BY1(em)) + 3 && y < (int)floorf(oy) - 3;
              int outside = x < (int)floorf(ox + BRT_BX0(em)) - 3 || x >= (int)ceilf(ox + BRT_BX1(em)) + 3 || y < (int)floorf(oy - BRT_BY1(em)) - 3 || y >= (int)ceilf(oy) + 3;
              if (inside && !brt_near(v, 0x204080, 3)) ib++;
              if (outside && v != W) ob++;
          } }
        t_check(ib == 0 && ob == 0, __FILE__, __LINE__, "box pixels", "aa %d render %d measure %d: interior bad %d, outside bad %d", aa, rm, mm, ib, ob);
    }
    brt_close(&c);
}
TEST(brtm_matrix_cleartype_aa) { brt_matrix_for_aa(DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_matrix_grayscale_aa) { brt_matrix_for_aa(DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE); }

TEST(brtm_default_params_both_aa_modes) {
    BRT_FIXTURE(c, 120, 120); int a;
    if (!c.rt) return;
    if (!c.rp_default || !c.rt1) { brt_close(&c); t_skip("needs default params and RT1"); return; }
    for (a = 0; a < 2; a++) {
        brt_run r; RECT bb; int x, y, bad = 0; float ox = 14.0f, oy = 90.0f, em = 48.0f;
        IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, (DWRITE_TEXT_ANTIALIAS_MODE)a);
        brt_fill(&c, W); brt_make_run(&c, &r, "A", em);
        CHECK_HR_OK(brt_draw_run(&c, &r, ox, oy, DWRITE_MEASURING_MODE_NATURAL, c.rp_default, 0x204080, &bb));
        for (y = (int)(oy - BRT_BY1(em)) + 4; y < (int)oy - 4; y++) for (x = (int)(ox + BRT_BX1(em) * 0.5f); x < (int)(ox + BRT_BX1(em)) - 4; x++)
            if (!brt_near(brt_px(&c, x, y), 0x204080, 3)) bad++;
        t_check(bad == 0, __FILE__, __LINE__, "interior", "aa %d: %d bad", a, bad);
    }
    brt_close(&c);
}

TEST(brtm_rt1_pointer_draws_same) {
    BRT_FIXTURE(c, 100, 100); brt_run r; uint64_t h1, h2;
    if (!c.rt) return;
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    brt_make_run(&c, &r, "A", 48.0f);
    brt_fill(&c, W);
    IDWriteBitmapRenderTarget_DrawGlyphRun(c.rt, 10, 80, DWRITE_MEASURING_MODE_NATURAL, &r.run, c.rp_mode[1], RGB(0, 0, 0), NULL); h1 = brt_hash(&c);
    brt_fill(&c, W);
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_DrawGlyphRun(c.rt1, 10, 80, DWRITE_MEASURING_MODE_NATURAL, &r.run, c.rp_mode[1], RGB(0, 0, 0), NULL)); h2 = brt_hash(&c);
    CHECK(h1 == h2);
    brt_close(&c);
}

TEST(brtm_aliased_vs_natural_antialiasing) {
    BRT_FIXTURE(c, 100, 100); int w, h, i, mixed_alias = 0, mixed_nat = 0; uint32_t *b; brt_run r;
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 31.7f);
    brt_fill(&c, W);
    brt_draw_run(&c, &r, 10.3f, 60.4f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_ALIASED], K, NULL);
    b = brt_bits(&c, &w, &h);
    for (i = 0; b && i < w * h; i++) { uint32_t v = b[i] & 0xFFFFFF; if (v != W && v != K) mixed_alias++; }
    brt_fill(&c, W);
    brt_draw_run(&c, &r, 10.3f, 60.4f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC], K, NULL);
    b = brt_bits(&c, &w, &h);
    for (i = 0; b && i < w * h; i++) { uint32_t v = b[i] & 0xFFFFFF; if (v != W && v != K) mixed_nat++; }
    EXPECT_EQ(mixed_alias, 0);
    CHECK(mixed_nat > 0);
    brt_close(&c);
}

TEST(brtm_cleartype_has_colour_fringes) {
    BRT_FIXTURE(c, 100, 100); int w, h, i, coloured = 0, partial = 0; uint32_t *b; brt_run r;
    if (!c.rt) return;
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    brt_make_run(&c, &r, "A", 31.7f);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 10.3f, 60.4f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC], K, NULL));
    b = brt_bits(&c, &w, &h);
    for (i = 0; b && i < w * h; i++) {
        uint32_t v = b[i] & 0xFFFFFF; int R = v >> 16, G = (v >> 8) & 0xFF, B = v & 0xFF;
        if (v != W && v != K) partial++;
        if (R != G || G != B) coloured++;
    }
    CHECK(partial > 0); CHECK(coloured > 0);
    t_note("ClearType edge pixels %d, coloured %d", partial, coloured);
    brt_close(&c);
}

TEST(brtm_grayscale_has_no_colour_fringes) {
    BRT_FIXTURE(c, 100, 100); int w, h, i, coloured = 0, partial = 0; uint32_t *b; brt_run r;
    if (!c.rt) return;
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    brt_make_run(&c, &r, "A", 31.7f);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 10.3f, 60.4f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC], K, NULL));
    b = brt_bits(&c, &w, &h);
    for (i = 0; b && i < w * h; i++) {
        uint32_t v = b[i] & 0xFFFFFF; int R = v >> 16, G = (v >> 8) & 0xFF, B = v & 0xFF;
        if (v != W && v != K) partial++;
        if (R != G || G != B) coloured++;
    }
    CHECK(partial > 0); EXPECT_EQ(coloured, 0);
    brt_close(&c);
}

TEST(brtm_aa_mode_changes_output) {
    BRT_FIXTURE(c, 100, 100); brt_run r; uint64_t h1, h2;
    if (!c.rt) return;
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    brt_make_run(&c, &r, "A", 31.7f);
    IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    brt_fill(&c, W); brt_draw_run(&c, &r, 10.3f, 60.4f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[4], K, NULL); h1 = brt_hash(&c);
    IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    brt_fill(&c, W); brt_draw_run(&c, &r, 10.3f, 60.4f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[4], K, NULL); h2 = brt_hash(&c);
    CHECK(h1 != h2);
    brt_close(&c);
}

TEST(brtm_pixel_geometry_variants) {
    /* RGB and BGR stripe order give mirrored fringes: different bitmaps; all three keep the interior solid */
    BRT_FIXTURE(c, 100, 100); static const DWRITE_PIXEL_GEOMETRY pg[3] = { DWRITE_PIXEL_GEOMETRY_FLAT, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_PIXEL_GEOMETRY_BGR };
    uint64_t h[3]; int i; brt_run r;
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 31.7f);
    for (i = 0; i < 3; i++) {
        IDWriteRenderingParams *p = NULL;
        CHECK_HR_OK(IDWriteFactory_CreateCustomRenderingParams(c.f, 2.2f, 0.0f, 1.0f, pg[i], DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, &p));
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw_run(&c, &r, 10.3f, 60.4f, DWRITE_MEASURING_MODE_NATURAL, p, K, NULL));
        h[i] = brt_hash(&c);
        EXPECT_EQ(brt_px(&c, 10 + 10, 60 - 10), K);
        DW_RELEASE(p);
    }
    CHECK(h[1] != h[2]);
    t_note("pixel geometry hashes flat/rgb/bgr %s %s", h[0] == h[1] ? "flat==rgb" : "flat!=rgb", h[0] == h[2] ? "flat==bgr" : "flat!=bgr");
    brt_close(&c);
}

TEST(brtm_gamma_contrast_params_keep_interior) {
    static const float gm[] = { 1.0f, 1.8f, 2.2f, 3.0f }, ct[] = { 0.0f, 0.5f, 1.0f };
    BRT_FIXTURE(c, 100, 100); int i, j; brt_run r;
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 48.0f);
    for (i = 0; i < 4; i++) for (j = 0; j < 3; j++) {
        IDWriteRenderingParams *p = NULL; HRESULT hr;
        hr = IDWriteFactory_CreateCustomRenderingParams(c.f, gm[i], ct[j], 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_NATURAL, &p);
        if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "params", "gamma %g contrast %g -> %s", gm[i], ct[j], dw_hr_name(hr)); continue; }
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw_run(&c, &r, 10.0f, 80.0f, DWRITE_MEASURING_MODE_NATURAL, p, K, NULL));
        t_check(brt_rect_mismatch(&c, 10 + 10, 80 - 25, 10 + 25, 80 - 6, K, 3) == 0, __FILE__, __LINE__, "interior solid", "gamma %g contrast %g", gm[i], ct[j]);
        DW_RELEASE(p);
    }
    brt_close(&c);
}

/* ---- OUTLINE mode: GDI path / region fill, strictly two-colour --------------------------------------------------------- */
TEST(brtm_outline_two_colours_only) {
    BRT_FIXTURE(c, 120, 120); int a, bbx;
    if (!c.rt) return;
    for (a = 0; a < 2; a++) for (bbx = 0; bbx < 2; bbx++) {
        int w = 0, h = 0, i, other = 0; uint32_t *b; RECT r; brt_run run;
        if (c.rt1) IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, (DWRITE_TEXT_ANTIALIAS_MODE)a); else if (a) continue;
        brt_fill(&c, W); brt_make_run(&c, &run, "A", 47.3f);
        CHECK_HR_OK(brt_draw_run(&c, &run, 10.4f, 90.6f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_OUTLINE], 0x2040C0, bbx ? &r : NULL));
        b = brt_bits(&c, &w, &h);
        for (i = 0; b && i < w * h; i++) { uint32_t v = b[i] & 0xFFFFFF; if (v != W && v != 0x2040C0) other++; }
        t_check(other == 0, __FILE__, __LINE__, "outline output is two-colour", "aa %d blackBox %d: %d other pixels", a, bbx, other);
        t_check(brt_count_not(&c, W) > 500, __FILE__, __LINE__, "drawn", "aa %d blackBox %d", a, bbx);
    }
    brt_close(&c);
}

TEST(brtm_outline_blackbox_is_pixel_bbox) {
    BRT_FIXTURE(c, 140, 140); int a;
    if (!c.rt) return;
    for (a = 0; a < 2; a++) {
        RECT bb, r; brt_run run;
        if (c.rt1) IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, (DWRITE_TEXT_ANTIALIAS_MODE)a); else if (a) continue;
        brt_fill(&c, W); brt_make_run(&c, &run, "AB", 40.0f);
        CHECK_HR_OK(brt_draw_run(&c, &run, 10.0f, 100.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_OUTLINE], K, &bb));
        CHECK(brt_bbox_not(&c, W, &r));
        t_check(bb.left == r.left && bb.top == r.top && bb.right == r.right && bb.bottom == r.bottom, __FILE__, __LINE__, "outline blackBox == painted bbox",
                "aa %d: box {%ld,%ld,%ld,%ld} pixels {%ld,%ld,%ld,%ld}", a, bb.left, bb.top, bb.right, bb.bottom, r.left, r.top, r.right, r.bottom);
    }
    brt_close(&c);
}

TEST(brtm_outline_exact_integer_geometry) {
    /* em 512: box x [25,225) y [-350,0) relative to the origin, all at whole pixels */
    BRT_FIXTURE(c, 300, 400); RECT r, bb; int n;
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 512.0f, 20.0f, 380.0f, K, NULL));      /* aliased params, large size: path depends on the recommended mode */
    brt_fill(&c, W);
    {
        brt_run run; brt_make_run(&c, &run, "A", 512.0f);
        CHECK_HR_OK(brt_draw_run(&c, &run, 20.0f, 380.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[DWRITE_RENDERING_MODE_OUTLINE], K, &bb));
    }
    CHECK(brt_bbox_not(&c, W, &r));
    n = brt_count_not(&c, W);
    t_note("outline em512 pixels {%ld,%ld,%ld,%ld} count %d (ideal {45,30,245,380} 70000) blackBox {%ld,%ld,%ld,%ld}", r.left, r.top, r.right, r.bottom, n, bb.left, bb.top, bb.right, bb.bottom);
    CHECK(labs(r.left - 45) <= 1 && labs(r.right - 245) <= 1 && labs(r.top - 30) <= 1 && labs(r.bottom - 380) <= 1);
    CHECK(n >= 69000 && n <= 71000);
    brt_close(&c);
}

TEST(brtm_outline_ppd_and_transform_equivalent) {
    /* em 256 DIPs at 2 pixels per DIP, and em 256 with a scale-2 transform and origin halved: same pixels as em 512 at ppd 1 */
    BRT_FIXTURE(c, 300, 400); RECT r0, r1, r2; brt_run run; DWRITE_MATRIX m = brt_mat(2, 0, 0, 2, 0, 0);
    if (!c.rt) return;
    brt_make_run(&c, &run, "A", 512.0f);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &run, 20.0f, 380.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[6], K, NULL));
    CHECK(brt_bbox_not(&c, W, &r0));
    brt_make_run(&c, &run, "A", 256.0f);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 2.0f);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &run, 10.0f, 190.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[6], K, NULL));
    CHECK(brt_bbox_not(&c, W, &r1));
    IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 1.0f);
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &run, 10.0f, 190.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[6], K, NULL));
    CHECK(brt_bbox_not(&c, W, &r2));
    CHECK(labs(r0.left - r1.left) <= 1 && labs(r0.right - r1.right) <= 1 && labs(r0.top - r1.top) <= 1 && labs(r0.bottom - r1.bottom) <= 1);
    CHECK(labs(r0.left - r2.left) <= 1 && labs(r0.right - r2.right) <= 1 && labs(r0.top - r2.top) <= 1 && labs(r0.bottom - r2.bottom) <= 1);
    brt_close(&c);
}

TEST(brtm_outline_integer_translation_exact) {
    BRT_FIXTURE(c, 200, 200); int w, h, x, y, bad = 0; uint32_t *ref; brt_run run;
    if (!c.rt) return;
    brt_make_run(&c, &run, "A", 70.0f);
    brt_fill(&c, W);
    brt_draw_run(&c, &run, 30.0f, 120.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[6], K, NULL);
    brt_bits(&c, &w, &h); ref = (uint32_t *)malloc((size_t)w * h * 4); memcpy(ref, brt_bits(&c, &w, &h), (size_t)w * h * 4);
    brt_fill(&c, W);
    brt_draw_run(&c, &run, 41.0f, 133.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[6], K, NULL);
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        int sx = x - 11, sy = y - 13;
        if (sx >= 0 && sy >= 0 && sx < w && sy < h && brt_px(&c, x, y) != (ref[sy * w + sx] & 0xFFFFFF)) bad++;
    }
    EXPECT_EQ(bad, 0);
    free(ref); brt_close(&c);
}

/* ---- transforms ------------------------------------------------------------------------------------------------------ */
typedef struct { const char *name; DWRITE_MATRIX m; float ppd; float ox, oy; } brt_xf_row;

static void brt_xf_run(const brt_xf_row *t, int n, int rmode, int sideways, DWRITE_TEXT_ANTIALIAS_MODE aa) {
    BRT_FIXTURE(c, 220, 220); int i;
    if (!c.rt) return;
    if (c.rt1) IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, aa);
    for (i = 0; i < n; i++) {
        brt_run r; float q[4][2]; int ib, ob, nin; RECT bb, px; HRESULT hr; float em = 36.0f;
        brt_make_run(&c, &r, "A", em); r.run.isSideways = sideways;
        brt_fill(&c, W);
        IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, t[i].ppd);
        CHECK_HR_OK(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &t[i].m));
        hr = brt_draw_run(&c, &r, t[i].ox, t[i].oy, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[rmode], 0x0000FF, &bb);
        t_check(hr == S_OK, __FILE__, __LINE__, "draw", "%s: %s", t[i].name, dw_hr_name(hr));
        brt_quad(&t[i].m, t[i].ppd, t[i].ox, t[i].oy, em, sideways, q);
        brt_quad_check(&c, q, 3.0f, 0x0000FF, W, 3, &ib, &ob, &nin);
        t_check(ib == 0 && ob == 0, __FILE__, __LINE__, "transformed box", "%s (render %d sideways %d): interior bad %d of %d, outside bad %d", t[i].name, rmode, sideways, ib, nin, ob);
        t_check(nin > 0, __FILE__, __LINE__, "interior sample", "%s: only %d interior pixels", t[i].name, nin);
        if (brt_bbox_not(&c, W, &px))
            t_check(bb.left <= px.left && bb.top <= px.top && bb.right >= px.right && bb.bottom >= px.bottom, __FILE__, __LINE__, "blackBox contains pixels", "%s: box {%ld,%ld,%ld,%ld} pixels {%ld,%ld,%ld,%ld}",
                    t[i].name, bb.left, bb.top, bb.right, bb.bottom, px.left, px.top, px.right, px.bottom);
    }
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, NULL);
    brt_close(&c);
}
static const brt_xf_row brt_xf_table[] = {
    { "identity",        { 1, 0, 0, 1, 0, 0 },           1.0f, 40, 120 },
    { "scale2",          { 2, 0, 0, 2, 0, 0 },           1.0f, 20, 90 },
    { "scale x2 y1",     { 2, 0, 0, 1, 0, 0 },           1.0f, 20, 120 },
    { "scale x1 y0.5",   { 1, 0, 0, 0.5f, 0, 0 },        1.0f, 40, 240 },
    { "scale 0.5",       { 0.5f, 0, 0, 0.5f, 0, 0 },     1.0f, 80, 240 },
    { "translate",       { 1, 0, 0, 1, 30, -20 },        1.0f, 20, 150 },
    { "rotate 90cw",     { 0, 1, -1, 0, 0, 0 },          1.0f, 120, -20 },
    { "rotate 90ccw",    { 0, -1, 1, 0, 0, 0 },          1.0f, -150, 60 },
    { "rotate 180",      { -1, 0, 0, -1, 0, 0 },         1.0f, -110, -110 },
    { "flip x",          { -1, 0, 0, 1, 0, 0 },          1.0f, -100, 120 },
    { "flip y",          { 1, 0, 0, -1, 0, 0 },          1.0f, 60, -100 },
    { "rotate 30cw",     { 0.8660254f, 0.5f, -0.5f, 0.8660254f, 0, 0 }, 1.0f, 90, 90 },
    { "rotate 45cw",     { 0.7071068f, 0.7071068f, -0.7071068f, 0.7071068f, 0, 0 }, 1.0f, 70, 40 },
    { "skew x",          { 1, 0, 0.5f, 1, 0, 0 },        1.0f, 60, 120 },
    { "skew y",          { 1, 0.3f, 0, 1, 0, 0 },        1.0f, 30, 90 },
    { "ppd 2",           { 1, 0, 0, 1, 0, 0 },           2.0f, 20, 55 },
    { "ppd 1.5 rot90",   { 0, 1, -1, 0, 0, 0 },          1.5f, 80, -15 },
    { "ppd 0.5 scale 4", { 4, 0, 0, 4, 0, 0 },           0.5f, 20, 60 },
};
#define BRT_NXF ((int)(sizeof brt_xf_table / sizeof brt_xf_table[0]))

TEST(brtm_xform_aliased) { brt_xf_run(brt_xf_table, BRT_NXF, DWRITE_RENDERING_MODE_ALIASED, 0, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_xform_outline) { brt_xf_run(brt_xf_table, BRT_NXF, DWRITE_RENDERING_MODE_OUTLINE, 0, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_xform_outline_grayscale) { brt_xf_run(brt_xf_table, BRT_NXF, DWRITE_RENDERING_MODE_OUTLINE, 0, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE); }
TEST(brtm_xform_natural_cleartype) { brt_xf_run(brt_xf_table, BRT_NXF, DWRITE_RENDERING_MODE_NATURAL, 0, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_xform_natural_grayscale) { brt_xf_run(brt_xf_table, BRT_NXF, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, 0, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE); }
TEST(brtm_xform_default_mode) { brt_xf_run(brt_xf_table, BRT_NXF, DWRITE_RENDERING_MODE_DEFAULT, 0, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE); }

TEST(brtm_xform_reset_restores_upright) {
    BRT_FIXTURE(c, 120, 120); DWRITE_MATRIX m = brt_mat(0, 1, -1, 0, 0, 0); int ib, ob;
    if (!c.rt) return;
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
    brt_fill(&c, W); brt_draw(&c, "A", 40.0f, 50.0f, 40.0f, K, NULL);
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, NULL);
    brt_fill(&c, W); brt_draw(&c, "A", 40.0f, 20.0f, 90.0f, K, NULL);
    {
        int w = 0, h = 0, x, y; uint32_t *b = brt_bits(&c, &w, &h); ib = ob = 0;
        for (y = 0; b && y < h; y++) for (x = 0; x < w; x++) {
            uint32_t v = b[y * w + x] & 0xFFFFFF;
            int inside = x >= (int)ceilf(20 + BRT_BX0(40)) + 2 && x < (int)floorf(20 + BRT_BX1(40)) - 2 && y >= (int)ceilf(90 - BRT_BY1(40)) + 2 && y < 88;
            int outside = x < (int)floorf(20 + BRT_BX0(40)) - 2 || x >= (int)ceilf(20 + BRT_BX1(40)) + 2 || y < (int)floorf(90 - BRT_BY1(40)) - 2 || y >= 92;
            if (inside && v != K) ib++;
            if (outside && v != W) ob++;
        }
    }
    EXPECT_EQ(ib, 0); EXPECT_EQ(ob, 0);
    brt_close(&c);
}

TEST(brtm_xform_is_not_dc_world_transform) {
    /* DC world transform set by the application does not move the aliased raster output */
    BRT_FIXTURE(c, 120, 120); XFORM x = { 1, 0, 0, 1, 40, 40 }; int w, h; uint64_t h1, h2;
    if (!c.rt) return;
    brt_fill(&c, W); brt_draw(&c, "A", 40.0f, 20.0f, 60.0f, K, NULL); h1 = brt_hash(&c);
    SetWorldTransform(c.dc, &x);
    brt_fill(&c, W); brt_draw(&c, "A", 40.0f, 20.0f, 60.0f, K, NULL); h2 = brt_hash(&c);
    ModifyWorldTransform(c.dc, NULL, MWT_IDENTITY);
    brt_bits(&c, &w, &h);
    EXPECT_EQ(h1, h2);
    brt_close(&c);
}

/* ---- sideways and bidi -------------------------------------------------------------------------------------------------- */
static const brt_xf_row brt_sw_table[] = {
    { "sideways identity", { 1, 0, 0, 1, 0, 0 },    1.0f, 100, 120 },
    { "sideways ppd 2",    { 1, 0, 0, 1, 0, 0 },    2.0f, 50, 60 },
    { "sideways scale 2",  { 2, 0, 0, 2, 0, 0 },    1.0f, 60, 100 },
    { "sideways rot90cw",  { 0, 1, -1, 0, 80, 60 }, 1.0f, 0, 0 },         /* upright result: vertical writing */
};
TEST(brtm_sideways_aliased) { brt_xf_run(brt_sw_table, 4, DWRITE_RENDERING_MODE_ALIASED, 1, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_sideways_outline) { brt_xf_run(brt_sw_table, 4, DWRITE_RENDERING_MODE_OUTLINE, 1, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_sideways_natural) { brt_xf_run(brt_sw_table, 4, DWRITE_RENDERING_MODE_NATURAL, 1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE); }

TEST(brtm_sideways_rotated_back_equals_upright) {
    /* a sideways run turned 90 degrees to the right is the ordinary upright glyph (vertical writing recipe) */
    BRT_FIXTURE(c, 160, 160); brt_run r; DWRITE_MATRIX m = brt_mat(0, 1, -1, 0, 60, 90); int w, h; uint32_t *ref; int x, y, bad = 0;
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 40.0f);
    brt_fill(&c, W);
    brt_draw_run(&c, &r, 0, 0, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL);          /* plain: origin at (0,0) clipped */
    brt_fill(&c, W);
    brt_draw_run(&c, &r, 60, 90, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL);
    brt_bits(&c, &w, &h); ref = (uint32_t *)malloc((size_t)w * h * 4); memcpy(ref, brt_bits(&c, &w, &h), (size_t)w * h * 4);
    r.run.isSideways = TRUE;
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 0, 0, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        float d1, d2; (void)d1; (void)d2;
        if (!brt_near(brt_px(&c, x, y), ref[y * w + x] & 0xFFFFFF, 0)) bad++;
    }
    t_note("sideways+rotate vs upright: %d differing pixels (edge rounding)", bad);
    CHECK(bad < 200);              /* identical up to edge rounding */
    free(ref); brt_close(&c);
}

TEST(brtm_bidi_levels) {
    BRT_FIXTURE(c, 240, 100); int lvl; uint64_t h[4]; RECT box[4];
    if (!c.rt) return;
    for (lvl = 0; lvl < 4; lvl++) {
        brt_run r; RECT bb;
        brt_make_run(&c, &r, "AB", 32.0f); r.run.bidiLevel = lvl;
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw_run(&c, &r, 100, 70, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, &bb));
        h[lvl] = brt_hash(&c);
        t_check(brt_bbox_not(&c, W, &box[lvl]), __FILE__, __LINE__, "drawn", "level %d", lvl);
        t_note("bidi level %d: pixels {%ld,%ld,%ld,%ld}", lvl, box[lvl].left, box[lvl].top, box[lvl].right, box[lvl].bottom);
    }
    CHECK(h[0] == h[2]); CHECK(h[1] == h[3]);          /* only the parity of the level matters */
    brt_close(&c);
}

/* ---- ppd and size relations -------------------------------------------------------------------------------------------- */
TEST(brtm_ppd_table_scales_box) {
    static const float ppd[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };
    BRT_FIXTURE(c, 240, 240); int i;
    if (!c.rt) return;
    for (i = 0; i < 5; i++) {
        float em = 32.0f, ox = 10.0f, oy = 60.0f; DWRITE_MATRIX id = brt_mat(1, 0, 0, 1, 0, 0); float q[4][2]; int ib, ob, nin;
        brt_run r; brt_make_run(&c, &r, "A", em);
        IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, ppd[i]);
        brt_fill(&c, W);
        CHECK_HR_OK(brt_draw_run(&c, &r, ox, oy, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
        brt_quad(&id, ppd[i], ox, oy, em, 0, q);
        brt_quad_check(&c, q, 2.5f, K, W, 0, &ib, &ob, &nin);
        t_check(ib == 0 && ob == 0, __FILE__, __LINE__, "ppd box", "ppd %g: interior bad %d outside bad %d (n=%d)", ppd[i], ib, ob, nin);
    }
    brt_close(&c);
}

TEST(brtm_zero_em_size) {
    BRT_FIXTURE(c, 40, 40); brt_run r; HRESULT hr; RECT bb = {0};
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 0.0f); r.run.fontEmSize = 0.0f;
    brt_fill(&c, W);
    hr = brt_draw_run(&c, &r, 10, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, &bb);
    t_note("fontEmSize 0 -> %s", dw_hr_name(hr));
    CHECK(hr == S_OK || hr == E_INVALIDARG);
    brt_close(&c);
}

/* ---- cross-object use -------------------------------------------------------------------------------------------------- */
TEST(brtm_face_from_other_factory) {
    BRT_FIXTURE(c, 100, 100); IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_font fh; brt_run r; IDWriteRenderingParams *p = NULL; int ib, ob;
    if (!c.rt) return;
    if (!iso) { brt_close(&c); t_skip("no isolated factory"); return; }
    memset(&fh, 0, sizeof fh);
    if (FAILED(dwx_font_open_default(&fh, iso)) || FAILED(IDWriteFactory_CreateCustomRenderingParams(iso, 2.2f, 0, 1, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_ALIASED, &p))) {
        dwx_font_close(&fh); DW_RELEASE(p); IDWriteFactory_Release(iso); brt_close(&c); t_skip("setup"); return;
    }
    brt_make_run(&c, &r, "A", 64.0f); r.run.fontFace = fh.face;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, p, K, NULL));
    {
        int w = 0, h = 0; uint32_t *b = brt_bits(&c, &w, &h); (void)b; (void)h; (void)w;
    }
    EXPECT_EQ(brt_px(&c, 24, 60), K); EXPECT_EQ(brt_px(&c, 2, 2), W);
    DW_RELEASE(p); dwx_font_close(&fh); IDWriteFactory_Release(iso);
    brt_close(&c);
}

TEST(brtm_face_refcount_balanced) {
    BRT_FIXTURE(c, 100, 100); brt_run r; ULONG a, b; int i;
    if (!c.rt) return;
    brt_make_run(&c, &r, "AB", 40.0f);
    a = dw_refcount((IUnknown *)c.font.face);
    for (i = 0; i < 20; i++) brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[i % 7], K, NULL);
    b = dw_refcount((IUnknown *)c.font.face);
    t_check(a == b, __FILE__, __LINE__, "face refcount", "before %lu after %lu", a, b);
    a = dw_refcount((IUnknown *)c.rp_mode[1]);
    for (i = 0; i < 20; i++) brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL);
    b = dw_refcount((IUnknown *)c.rp_mode[1]);
    t_check(a == b, __FILE__, __LINE__, "params refcount", "before %lu after %lu", a, b);
    brt_close(&c);
}

typedef struct { volatile LONG ok; int id; } brt_thr2;
static DWORD WINAPI brt_draw_thread(LPVOID arg) {
    brt_thr2 *t = (brt_thr2 *)arg; brt_ctx c; int i, good = 1;
    memset(&c, 0, sizeof c);
    c.f = dw_shared();
    if (!c.f || FAILED(IDWriteFactory_GetGdiInterop(c.f, &c.gi)) || FAILED(IDWriteGdiInterop_CreateBitmapRenderTarget(c.gi, NULL, 100, 100, &c.rt))) return 1;
    c.dc = IDWriteBitmapRenderTarget_GetMemoryDC(c.rt);
    if (FAILED(dwx_font_open_default(&c.font, c.f))) { dwx_font_close(&c.font); return 1; }
    c.have_font = 1;
    if (FAILED(IDWriteFactory_CreateCustomRenderingParams(c.f, 2.2f, 0, 1, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_ALIASED, &c.rp_mode[1]))) good = 0;
    for (i = 0; good && i < 20; i++) {
        brt_fill(&c, W);
        if (FAILED(brt_draw(&c, "A", 64.0f, 10.0f, 80.0f, K, NULL))) good = 0;
        if (brt_px(&c, 24, 60) != K || brt_px(&c, 2, 2) != W) good = 0;
    }
    brt_close(&c);
    t->ok = good;
    return 0;
}
TEST(brtm_threads_draw_own_targets) {
    HANDLE th[4]; brt_thr2 info[4]; int i, good = 0;
    for (i = 0; i < 4; i++) { info[i].ok = 0; info[i].id = i; th[i] = CreateThread(NULL, 0, brt_draw_thread, &info[i], 0, NULL); }
    for (i = 0; i < 4; i++) { if (th[i]) { WaitForSingleObject(th[i], 30000); CloseHandle(th[i]); } good += info[i].ok; }
    EXPECT_EQ(good, 4);
}

/* ---- system fonts ------------------------------------------------------------------------------------------------------ */
static const WCHAR *const brt_sysfonts[] = { L"Arial", L"Segoe UI", L"Tahoma", L"Times New Roman", L"Courier New", L"Consolas", L"Verdana", L"Calibri" };

static void brt_sys_I(brt_ctx *c, int fi, int rmode, DWRITE_MEASURING_MODE mm, float em, int *tested) {
    IDWriteFontFace *face; UINT32 cp = 'I'; UINT16 gid = 0; dwx_geom *g; UINT16 idx; float adv; float x0, y0, x1, y1; brt_run r; RECT bb, px; HRESULT hr;
    float ox = 20.0f, oy = 150.0f; int col, y, bad = 0;
    face = dw_sysface(c->f, brt_sysfonts[fi], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) return;
    if (FAILED(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gid)) || !gid) { IDWriteFontFace_Release(face); return; }
    idx = gid; adv = 0;
    g = dwx_geom_new();
    hr = IDWriteFontFace_GetGlyphRunOutline(face, em, &idx, &adv, NULL, 1, FALSE, FALSE, (IDWriteGeometrySink *)g);
    if (FAILED(hr) || !g->npts) { dwx_release(g); IDWriteFontFace_Release(face); return; }
    dwx_geom_bounds(g, &x0, &y0, &x1, &y1);
    dwx_release(g);
    (*tested)++;
    memset(&r, 0, sizeof r); r.run.fontFace = face; r.run.fontEmSize = em; r.run.glyphCount = 1; r.run.glyphIndices = &idx; r.run.glyphAdvances = &adv;
    {
        DWRITE_GLYPH_METRICS gm; IDWriteFontFace_GetDesignGlyphMetrics(face, &idx, 1, &gm, FALSE);
        DWRITE_FONT_METRICS fm; IDWriteFontFace_GetMetrics(face, &fm);
        adv = (float)gm.advanceWidth * em / fm.designUnitsPerEm;
    }
    brt_fill(c, W);
    hr = brt_draw_run(c, &r, ox, oy, mm, c->rp_mode[rmode], 0x802010, &bb);
    t_check(hr == S_OK, __FILE__, __LINE__, "draw I", "%ls render %d: %s", brt_sysfonts[fi], rmode, dw_hr_name(hr));
    /* outline bounds are relative to the baseline origin with y down */
    col = (int)floorf(ox + (x0 + x1) * 0.5f);
    for (y = (int)ceilf(oy + y0) + 3; y < (int)floorf(oy + y1) - 3; y++) if (!brt_near(brt_px(c, col, y), 0x802010, 3)) bad++;
    t_check(bad == 0, __FILE__, __LINE__, "stem column solid", "%ls em %g render %d: %d bad pixels in column %d (outline x %g..%g y %g..%g)", brt_sysfonts[fi], em, rmode, bad, col, x0, x1, y0, y1);
    if (brt_bbox_not(c, W, &px)) {
        t_check(labs(px.left - (long)floorf(ox + x0)) <= 3 && labs(px.right - (long)ceilf(ox + x1)) <= 3 && labs(px.top - (long)floorf(oy + y0)) <= 3 && labs(px.bottom - (long)ceilf(oy + y1)) <= 3,
                __FILE__, __LINE__, "pixels match outline bounds", "%ls em %g render %d: pixels {%ld,%ld,%ld,%ld} outline {%g,%g,%g,%g}", brt_sysfonts[fi], em, rmode, px.left, px.top, px.right, px.bottom, ox + x0, oy + y0, ox + x1, oy + y1);
    } else t_check(0, __FILE__, __LINE__, "drawn", "%ls em %g render %d: nothing drawn", brt_sysfonts[fi], em, rmode);
    IDWriteFontFace_Release(face);
}
static void brt_sys_run(int rmode, DWRITE_MEASURING_MODE mm, DWRITE_TEXT_ANTIALIAS_MODE aa) {
    BRT_FIXTURE(c, 160, 180); static const float ems[] = { 24.0f, 48.0f, 96.0f }; int fi, ei, tested = 0;
    if (!c.rt) return;
    if (c.rt1) IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, aa);
    for (fi = 0; fi < 8; fi++) for (ei = 0; ei < 3; ei++) brt_sys_I(&c, fi, rmode, mm, ems[ei], &tested);
    brt_close(&c);
    if (!tested) t_skip("no system font with an 'I' glyph available");
}
TEST(brtm_sys_I_aliased) { brt_sys_run(DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_sys_I_outline) { brt_sys_run(DWRITE_RENDERING_MODE_OUTLINE, DWRITE_MEASURING_MODE_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_sys_I_cleartype_natural) { brt_sys_run(DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_sys_I_grayscale_natural) { brt_sys_run(DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE); }
TEST(brtm_sys_I_gdi_classic) { brt_sys_run(DWRITE_RENDERING_MODE_GDI_CLASSIC, DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtm_sys_I_gdi_natural) { brt_sys_run(DWRITE_RENDERING_MODE_GDI_NATURAL, DWRITE_MEASURING_MODE_GDI_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE); }
TEST(brtm_sys_I_default_mode) { brt_sys_run(DWRITE_RENDERING_MODE_DEFAULT, DWRITE_MEASURING_MODE_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }

TEST(brtm_sys_arial_text_extent) {
    /* "Hello" in Arial: pixel extent roughly equals the sum of advances; baseline row is the bottom of 'H' */
    BRT_FIXTURE(c, 300, 100); IDWriteFontFace *face = dw_sysface(c.f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    UINT32 cps[5] = { 'H', 'e', 'l', 'l', 'o' }; UINT16 gi[5]; DWRITE_GLYPH_METRICS gm[5]; DWRITE_FONT_METRICS fm; float adv[5], sum = 0, em = 40.0f; int i; brt_run r; RECT px;
    if (!c.rt) { if (face) IDWriteFontFace_Release(face); return; }
    if (!face) { brt_close(&c); t_skip("Arial not installed"); return; }
    IDWriteFontFace_GetGlyphIndices(face, cps, 5, gi);
    IDWriteFontFace_GetDesignGlyphMetrics(face, gi, 5, gm, FALSE);
    IDWriteFontFace_GetMetrics(face, &fm);
    for (i = 0; i < 5; i++) { adv[i] = gm[i].advanceWidth * em / fm.designUnitsPerEm; sum += adv[i]; }
    memset(&r, 0, sizeof r); r.run.fontFace = face; r.run.fontEmSize = em; r.run.glyphCount = 5; r.run.glyphIndices = gi; r.run.glyphAdvances = adv;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 10, 60, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    CHECK(brt_bbox_not(&c, W, &px));
    CHECK(px.left >= 10 && px.left <= 10 + 8);
    CHECK(px.right <= 10 + (long)ceilf(sum) + 1 && px.right >= 10 + (long)(sum * 0.8f));
    CHECK(px.bottom >= 60 && px.bottom <= 60 + (long)(em * 0.3f));            /* 'o' / 'e' overshoot, nothing hangs below */
    t_note("Arial 'Hello' em 40: pixels {%ld,%ld,%ld,%ld} advance sum %g", px.left, px.top, px.right, px.bottom, sum);
    IDWriteFontFace_Release(face); brt_close(&c);
}
