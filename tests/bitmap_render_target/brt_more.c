/* More DrawGlyphRun coverage: angle and scale sweeps, extreme sizes, unusual advances, parameter sweeps, repeated use. */
#include "brt.h"

#define W 0xFFFFFF
#define K 0x000000

/* ---- sweeps ----------------------------------------------------------------------------------------------------- */
static void brt_sweep_rotation(int rmode, DWRITE_TEXT_ANTIALIAS_MODE aa) {
    BRT_FIXTURE(c, 240, 240); int deg;
    if (!c.rt) return;
    if (c.rt1) IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, aa);
    for (deg = 0; deg < 360; deg += 15) {
        float a = deg * 3.14159265f / 180.0f, cs = cosf(a), sn = sinf(a), em = 40.0f;
        DWRITE_MATRIX m; float q[4][2], cx, cy; int ib, ob, nin; brt_run r; HRESULT hr;
        /* rotate about the bitmap centre: translation chosen so the origin (ox,oy) lands at (120,120) */
        float ox = 0.0f, oy = 0.0f;
        m = brt_mat(cs, sn, -sn, cs, 0, 0);
        cx = 120.0f; cy = 120.0f;
        m.dx = cx - (m.m11 * ox + m.m21 * oy); m.dy = cy - (m.m12 * ox + m.m22 * oy);
        brt_make_run(&c, &r, "A", em);
        brt_fill(&c, W);
        IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
        hr = brt_draw_run(&c, &r, ox, oy, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[rmode], 0x00A000, NULL);
        t_check(hr == S_OK, __FILE__, __LINE__, "draw", "%d deg: %s", deg, dw_hr_name(hr));
        brt_quad(&m, 1.0f, ox, oy, em, 0, q);
        brt_quad_check(&c, q, 3.0f, 0x00A000, W, 3, &ib, &ob, &nin);
        t_check(ib == 0 && ob == 0 && nin > 0, __FILE__, __LINE__, "rotated box", "%d deg render %d: interior bad %d of %d, outside bad %d", deg, rmode, ib, nin, ob);
    }
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, NULL);
    brt_close(&c);
}
TEST(brtx_rotation_sweep_aliased) { brt_sweep_rotation(DWRITE_RENDERING_MODE_ALIASED, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtx_rotation_sweep_outline) { brt_sweep_rotation(DWRITE_RENDERING_MODE_OUTLINE, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }
TEST(brtx_rotation_sweep_natural_gray) { brt_sweep_rotation(DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE); }
TEST(brtx_rotation_sweep_natural_cleartype) { brt_sweep_rotation(DWRITE_RENDERING_MODE_NATURAL, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE); }

TEST(brtx_scale_sweep_aliased) {
    static const float sc[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f };
    BRT_FIXTURE(c, 260, 260); int i;
    if (!c.rt) return;
    for (i = 0; i < 8; i++) {
        float em = 36.0f, ox = 60.0f / sc[i], oy = 180.0f / sc[i]; DWRITE_MATRIX m = brt_mat(sc[i], 0, 0, sc[i], 0, 0); float q[4][2]; int ib, ob, nin; brt_run r;
        brt_make_run(&c, &r, "A", em);
        brt_fill(&c, W);
        IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
        CHECK_HR_OK(brt_draw_run(&c, &r, ox, oy, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
        brt_quad(&m, 1.0f, ox, oy, em, 0, q);
        brt_quad_check(&c, q, 2.5f, K, W, 0, &ib, &ob, &nin);
        t_check(ib == 0 && ob == 0, __FILE__, __LINE__, "scaled box", "scale %g: interior bad %d of %d, outside bad %d", sc[i], ib, nin, ob);
    }
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, NULL);
    brt_close(&c);
}

TEST(brtx_scale2_doubles_blackbox_extent) {
    BRT_FIXTURE(c, 300, 300); RECT a, b; DWRITE_MATRIX m = brt_mat(2, 0, 0, 2, 0, 0);
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 40.0f, 20.0f, 100.0f, K, &a));
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 40.0f, 10.0f, 50.0f, K, &b));               /* origin halved: same anchor */
    t_check(labs((b.right - b.left) - 2 * (a.right - a.left)) <= 4 && labs((b.bottom - b.top) - 2 * (a.bottom - a.top)) <= 4, __FILE__, __LINE__, "doubled extent",
            "unscaled {%ld,%ld,%ld,%ld} scaled {%ld,%ld,%ld,%ld}", a.left, a.top, a.right, a.bottom, b.left, b.top, b.right, b.bottom);
    brt_close(&c);
}

/* ---- extreme sizes -------------------------------------------------------------------------------------------------- */
TEST(brtx_tiny_em_sizes_succeed) {
    static const float em[] = { 0.01f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };
    BRT_FIXTURE(c, 40, 40); int i, m;
    if (!c.rt) return;
    for (i = 0; i < 8; i++) for (m = 0; m < 7; m++) {
        brt_run r; RECT bb; HRESULT hr; brt_make_run(&c, &r, "AB", em[i]);
        hr = brt_draw_run(&c, &r, 5, 30, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[m], K, &bb);
        t_check(hr == S_OK, __FILE__, __LINE__, "tiny em", "em %g render %d -> %s", em[i], m, dw_hr_name(hr));
    }
    brt_close(&c);
}

TEST(brtx_tiny_em_draws_something_at_4px) {
    BRT_FIXTURE(c, 40, 40);
    if (!c.rt) return;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "AAA", 8.0f, 5, 30, K, NULL));
    CHECK(brt_count_not(&c, W) > 10);
    brt_close(&c);
}

TEST(brtx_large_em_covers_bitmap) {
    BRT_FIXTURE(c, 64, 64); int m;
    if (!c.rt) return;
    for (m = 1; m < 7; m++) {
        HRESULT hr; brt_run r; brt_make_run(&c, &r, "A", 1000.0f);
        brt_fill(&c, W);
        hr = brt_draw_run(&c, &r, -100.0f, 600.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[m], K, NULL);       /* box x [-51,339) y [-83,600) */
        t_check(hr == S_OK, __FILE__, __LINE__, "large em", "render %d -> %s", m, dw_hr_name(hr));
        if (m == DWRITE_RENDERING_MODE_ALIASED || m == DWRITE_RENDERING_MODE_OUTLINE)
            t_check(brt_count_not(&c, K) == 0, __FILE__, __LINE__, "bitmap fully covered", "render %d: %d pixels not black", m, brt_count_not(&c, K));
        else
            t_note("large em render %d: %d of 4096 pixels not black", m, brt_count_not(&c, K));     /* other modes may fall back differently at huge sizes */
    }
    brt_close(&c);
}

TEST(brtx_extreme_ppd_values) {
    static const float ppd[] = { 0.01f, 0.1f, 10.0f, 50.0f };
    BRT_FIXTURE(c, 64, 64); int i;
    if (!c.rt) return;
    for (i = 0; i < 4; i++) {
        HRESULT hr; IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, ppd[i]);
        hr = brt_draw(&c, "A", 8.0f, 2.0f, 6.0f, K, NULL);
        t_check(hr == S_OK, __FILE__, __LINE__, "extreme ppd", "ppd %g -> %s", ppd[i], dw_hr_name(hr));
    }
    brt_close(&c);
}

/* ---- advances / offsets ----------------------------------------------------------------------------------------- */
TEST(brtx_zero_advances_overlap_at_origin) {
    BRT_FIXTURE(c, 120, 100); brt_run r; float em = 40.0f; int ib, ob;
    if (!c.rt) return;
    brt_make_run(&c, &r, "AAA", em);
    r.adv[0] = r.adv[1] = r.adv[2] = 0.0f;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 20, 70, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    {
        int bad = brt_rect_mismatch(&c, (int)ceilf(20 + BRT_BX0(em)) + 2, (int)ceilf(70 - BRT_BY1(em)) + 2, (int)floorf(20 + BRT_BX1(em)) - 2, 68, K, 0);
        EXPECT_EQ(bad, 0);
    }
    EXPECT_EQ(brt_px(&c, (int)(20 + BRT_BX1(em)) + 4, 55), W);
    (void)ib; (void)ob;
    brt_close(&c);
}

TEST(brtx_negative_advance_moves_left) {
    BRT_FIXTURE(c, 160, 100); brt_run r; float em = 30.0f, ox = 100.0f; int bad = 0;
    if (!c.rt) return;
    brt_make_run(&c, &r, "AB", em);
    r.adv[0] = -40.0f;                                    /* second glyph is drawn 40 pixels to the left of the origin */
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, ox, 70, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    bad += brt_rect_mismatch(&c, (int)ceilf(ox - 40 + BRT_BX0(em)) + 2, (int)ceilf(70 - BRT_BY1(em)) + 2, (int)floorf(ox - 40 + BRT_BX1(em)) - 2, 68, K, 0);
    bad += brt_rect_mismatch(&c, (int)ceilf(ox + BRT_BX0(em)) + 2, (int)ceilf(70 - BRT_BY1(em)) + 2, (int)floorf(ox + BRT_BX1(em)) - 2, 68, K, 0);
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brtx_null_advances_uses_design_advances) {
    BRT_FIXTURE(c, 200, 100); brt_run r; HRESULT hr; float em = 32.0f;
    if (!c.rt) return;
    brt_make_run(&c, &r, "AB", em);
    r.run.glyphAdvances = NULL;
    brt_fill(&c, W);
    hr = brt_draw_run(&c, &r, 10, 70, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL);
    t_note("DrawGlyphRun with NULL glyphAdvances -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr)) {
        /* first glyph at the origin; the second one follows at some advance (design advance 1000 units expected) */
        EXPECT_EQ(brt_rect_mismatch(&c, (int)ceilf(10 + BRT_BX0(em)) + 2, (int)ceilf(70 - BRT_BY1(em)) + 2, (int)floorf(10 + BRT_BX1(em)) - 2, 68, K, 0), 0);
        t_check(brt_rect_mismatch(&c, (int)ceilf(10 + BRT_ADV(em) + BRT_BX0(em)) + 2, (int)ceilf(70 - BRT_BY1(em)) + 2, (int)floorf(10 + BRT_ADV(em) + BRT_BX1(em)) - 2, 68, K, 0) == 0,
                __FILE__, __LINE__, "second glyph at design advance", "NULL advances");
    }
    brt_close(&c);
}

TEST(brtx_offsets_do_not_move_pen) {
    BRT_FIXTURE(c, 200, 100); brt_run r; float em = 30.0f, ox = 10.0f; int bad = 0;
    if (!c.rt) return;
    brt_make_run(&c, &r, "AB", em);
    r.off[0].advanceOffset = 50.0f;                       /* first glyph shifted right by 50; second still at the pen position */
    r.run.glyphOffsets = r.off;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, ox, 70, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    bad += brt_rect_mismatch(&c, (int)ceilf(ox + 50 + BRT_BX0(em)) + 2, (int)ceilf(70 - BRT_BY1(em)) + 2, (int)floorf(ox + 50 + BRT_BX1(em)) - 2, 68, K, 0);
    bad += brt_rect_mismatch(&c, (int)ceilf(ox + BRT_ADV(em) + BRT_BX0(em)) + 2, (int)ceilf(70 - BRT_BY1(em)) + 2, (int)floorf(ox + BRT_ADV(em) + BRT_BX1(em)) - 2, 68, K, 0);
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brtx_ascender_offset_sign) {
    BRT_FIXTURE(c, 100, 120); brt_run r; float em = 30.0f; int up, down;
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", em);
    r.off[0].ascenderOffset = 20.0f; r.run.glyphOffsets = r.off;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    up = brt_rect_mismatch(&c, 14, (int)ceilf(80 - 20 - BRT_BY1(em)) + 2, 20, 58, K, 0);
    r.off[0].ascenderOffset = -20.0f;
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], K, NULL));
    down = brt_rect_mismatch(&c, 14, (int)ceilf(80 + 20 - BRT_BY1(em)) + 2, 20, 98, K, 0);
    EXPECT_EQ(up, 0); EXPECT_EQ(down, 0);
    brt_close(&c);
}

/* ---- rendering parameters sweep --------------------------------------------------------------------------------- */
TEST(brtx_cleartype_levels_all_succeed) {
    static const float lv[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    BRT_FIXTURE(c, 100, 100); int i, g; brt_run r;
    if (!c.rt) return;
    brt_make_run(&c, &r, "A", 40.0f);
    for (i = 0; i < 5; i++) for (g = 0; g < 3; g++) {
        IDWriteRenderingParams *p = NULL; HRESULT hr;
        hr = IDWriteFactory_CreateCustomRenderingParams(c.f, 2.2f, 0.5f, lv[i], (DWRITE_PIXEL_GEOMETRY)g, DWRITE_RENDERING_MODE_NATURAL, &p);
        if (FAILED(hr)) { t_check(0, __FILE__, __LINE__, "params", "level %g geometry %d -> %s", lv[i], g, dw_hr_name(hr)); continue; }
        brt_fill(&c, W);
        hr = brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, p, K, NULL);
        t_check(hr == S_OK, __FILE__, __LINE__, "draw", "level %g geometry %d -> %s", lv[i], g, dw_hr_name(hr));
        t_check(brt_rect_mismatch(&c, 10 + 6, 80 - 24, 10 + 17, 80 - 6, K, 6) == 0, __FILE__, __LINE__, "interior", "level %g geometry %d", lv[i], g);
        DW_RELEASE(p);
    }
    brt_close(&c);
}

TEST(brtx_monitor_params_work) {
    BRT_FIXTURE(c, 100, 100); IDWriteRenderingParams *p = NULL; HRESULT hr; brt_run r; HMONITOR mon;
    if (!c.rt) return;
    mon = MonitorFromWindow(GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    hr = IDWriteFactory_CreateMonitorRenderingParams(c.f, mon, &p);
    if (FAILED(hr) || !p) { brt_close(&c); t_skip("monitor params unavailable: %s", dw_hr_name(hr)); return; }
    brt_make_run(&c, &r, "A", 40.0f);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw_run(&c, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, p, K, NULL));
    CHECK(brt_rect_mismatch(&c, 10 + 6, 80 - 24, 10 + 17, 80 - 6, K, 6) == 0);
    DW_RELEASE(p); brt_close(&c);
}

TEST(brtx_params_object_reuse_across_targets) {
    BRT_FIXTURE(a, 100, 100); brt_ctx b; brt_run r;
    if (!a.rt) return;
    brt_open(&b, NULL, 100, 100, 1);
    brt_make_run(&a, &r, "A", 40.0f);
    brt_fill(&a, W); brt_fill(&b, W);
    CHECK_HR_OK(brt_draw_run(&a, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, a.rp_mode[1], K, NULL));
    r.run.fontFace = b.font.face;
    CHECK_HR_OK(brt_draw_run(&b, &r, 10, 80, DWRITE_MEASURING_MODE_NATURAL, a.rp_mode[1], K, NULL));        /* params from a, face from b */
    EXPECT_EQ(brt_hash(&a), brt_hash(&b));
    brt_close(&b); brt_close(&a);
}

TEST(brtx_gdi_text_and_dwrite_text_coexist) {
    /* GDI drawing into the same DC before and after DrawGlyphRun both land in the bitmap */
    BRT_FIXTURE(c, 100, 100); RECT r1 = { 0, 0, 10, 10 }, r2 = { 90, 90, 100, 100 }; HBRUSH br = CreateSolidBrush(RGB(255, 0, 0));
    if (!c.rt) return;
    brt_fill(&c, W);
    FillRect(c.dc, &r1, br);
    CHECK_HR_OK(brt_draw(&c, "A", 40.0f, 30.0f, 80.0f, K, NULL));
    FillRect(c.dc, &r2, br);
    EXPECT_EQ(brt_px(&c, 5, 5), 0xFF0000); EXPECT_EQ(brt_px(&c, 95, 95), 0xFF0000);
    EXPECT_EQ(brt_px(&c, 30 + 10, 80 - 12), K);
    DeleteObject(br); brt_close(&c);
}

TEST(brtx_savedc_restoredc_around_draw) {
    BRT_FIXTURE(c, 100, 100); int id;
    if (!c.rt) return;
    id = SaveDC(c.dc);
    CHECK(id > 0);
    brt_fill(&c, W);
    CHECK_HR_OK(brt_draw(&c, "A", 40.0f, 30.0f, 80.0f, K, NULL));
    CHECK(RestoreDC(c.dc, id));
    EXPECT_EQ(GetObjectType(c.dc), OBJ_MEMDC);
    brt_close(&c);
}

TEST(brtx_target_size_table_draw) {
    static const int sz[][2] = { {1,1},{2,2},{3,5},{8,8},{9,17},{31,3},{64,64},{200,5},{5,200} };
    int i;
    for (i = 0; i < 9; i++) {
        brt_ctx c; HRESULT hr; brt_open(&c, NULL, sz[i][0], sz[i][1], 1);
        brt_fill(&c, W);
        hr = brt_draw(&c, "AB", 12.0f, 1.0f, (float)sz[i][1] - 1, K, NULL);
        t_check(hr == S_OK, __FILE__, __LINE__, "draw on small target", "%dx%d -> %s", sz[i][0], sz[i][1], dw_hr_name(hr));
        brt_close(&c);
    }
}

TEST(brtx_repeated_draws_stay_stable) {
    BRT_FIXTURE(c, 100, 100); uint64_t h0 = 0; int i, bad = 0;
    if (!c.rt) return;
    for (i = 0; i < 200; i++) {
        brt_fill(&c, W);
        if (FAILED(brt_draw(&c, "AB", 30.0f + (i % 3), 10, 80, K, NULL))) bad++;
        if (i == 0) h0 = brt_hash(&c);
        if (i % 3 == 0 && brt_hash(&c) != h0) bad++;
    }
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brtx_resize_between_draws_keeps_working) {
    BRT_FIXTURE(c, 50, 50); int i, bad = 0;
    if (!c.rt) return;
    for (i = 0; i < 30; i++) {
        int w = 40 + (i * 13) % 90, h = 40 + (i * 7) % 70;
        if (FAILED(IDWriteBitmapRenderTarget_Resize(c.rt, w, h))) bad++;
        brt_fill(&c, W);
        if (FAILED(brt_draw(&c, "A", 30.0f, 8, 35, K, NULL))) bad++;
        if (brt_px(&c, 8 + 6, 35 - 12) != K || brt_px(&c, 0, 0) != W) bad++;
    }
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}
