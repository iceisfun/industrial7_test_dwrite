/* Resize, pixels-per-DIP, current transform and text antialias mode of the bitmap render target.
 *
 * Facts from the implementation:
 *  - Resize(w,h) with the current size is a no-op; otherwise a new DIB is created, the overlapping top-left region of the old pixels is
 *    copied (content is preserved, new area is zero), the new bitmap is selected into the same DC and the old one is deleted;
 *  - SetPixelsPerDip(p) rejects p <= 0 with E_INVALIDARG; the default is GetDeviceCaps(memdc, LOGPIXELSX) / 96;
 *  - SetCurrentTransform(NULL) resets to identity, a matrix with a NaN in any of the six members is E_INVALIDARG, nothing else is validated;
 *  - SetTextAntialiasMode accepts 0 (CLEARTYPE) and 1 (GRAYSCALE) only; the default is CLEARTYPE.
 */
#include "brt.h"

/* ---- Resize ---------------------------------------------------------------------------------------------------- */
static uint32_t brt_pat(int x, int y) { return (uint32_t)((((x * 7 + 1) & 0xFF) << 16) | (((y * 11 + 3) & 0xFF) << 8) | ((x + y * 3 + 5) & 0xFF)); }
static void brt_fill_pattern(brt_ctx *c) {
    int w = 0, h = 0, x, y; uint32_t *b = brt_bits(c, &w, &h);
    if (!b) return;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) b[y * w + x] = brt_pat(x, y);
    GdiFlush();
}

TEST(brt_resize_updates_getsize) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 10, 10, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 30, 12));
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
    EXPECT_EQ(s.cx, 30); EXPECT_EQ(s.cy, 12);
    brt_close(&c);
}

TEST(brt_resize_updates_dib) {
    brt_ctx c; BITMAP b; brt_open(&c, NULL, 10, 10, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 25, 7));
    memset(&b, 0, sizeof b); GetObjectA(GetCurrentObject(c.dc, OBJ_BITMAP), sizeof b, &b);
    EXPECT_EQ(b.bmWidth, 25); EXPECT_EQ(b.bmHeight, 7); EXPECT_EQ(b.bmBitsPixel, 32);
    brt_close(&c);
}

TEST(brt_resize_same_dc_handle) {
    brt_ctx c; brt_open(&c, NULL, 10, 10, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 11, 11));
    CHECK(IDWriteBitmapRenderTarget_GetMemoryDC(c.rt) == c.dc);
    brt_close(&c);
}

TEST(brt_resize_new_bitmap_handle_old_deleted) {
    brt_ctx c; HGDIOBJ before, after; DWORD n0, n1; brt_open(&c, NULL, 10, 10, 0);
    before = GetCurrentObject(c.dc, OBJ_BITMAP); n0 = brt_gdi_count();
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 20, 10));
    after = GetCurrentObject(c.dc, OBJ_BITMAP);
    CHECK(before != after);
    n1 = brt_gdi_settle(n0);
    t_check(n1 <= n0, __FILE__, __LINE__, "previous DIB deleted", "GDI objects %lu before Resize, %lu after", n0, n1);
    EXPECT_EQ(GetObjectType(after), OBJ_BITMAP);
    brt_close(&c);
}

TEST(brt_resize_same_size_is_noop) {
    brt_ctx c; HGDIOBJ before, after; uint32_t *b0, *b1; int w, h; brt_open(&c, NULL, 10, 10, 0);
    brt_fill_pattern(&c);
    before = GetCurrentObject(c.dc, OBJ_BITMAP); b0 = brt_bits(&c, &w, &h);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 10, 10));
    after = GetCurrentObject(c.dc, OBJ_BITMAP); b1 = brt_bits(&c, &w, &h);
    CHECK(before == after); CHECK(b0 == b1);
    EXPECT_EQ(brt_px(&c, 3, 4), brt_pat(3, 4));
    brt_close(&c);
}

TEST(brt_resize_grow_preserves_top_left) {
    brt_ctx c; int x, y, bad = 0, badnew = 0; brt_open(&c, NULL, 6, 5, 0);
    brt_fill_pattern(&c);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 11, 9));
    for (y = 0; y < 9; y++) for (x = 0; x < 11; x++) {
        uint32_t v = brt_px(&c, x, y);
        if (x < 6 && y < 5) { if (v != (brt_pat(x, y) & 0xFFFFFF)) bad++; }
        else if (v != 0) badnew++;                 /* newly exposed area is zero */
    }
    EXPECT_EQ(bad, 0); EXPECT_EQ(badnew, 0);
    brt_close(&c);
}

TEST(brt_resize_shrink_crops_bottom_right) {
    brt_ctx c; int x, y, bad = 0; brt_open(&c, NULL, 12, 10, 0);
    brt_fill_pattern(&c);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 5, 4));
    for (y = 0; y < 4; y++) for (x = 0; x < 5; x++) if (brt_px(&c, x, y) != (brt_pat(x, y) & 0xFFFFFF)) bad++;
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brt_resize_wider_only) {
    brt_ctx c; int x, y, bad = 0; brt_open(&c, NULL, 6, 6, 0);
    brt_fill_pattern(&c);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 10, 6));
    for (y = 0; y < 6; y++) for (x = 0; x < 10; x++) {
        uint32_t v = brt_px(&c, x, y), want = x < 6 ? (brt_pat(x, y) & 0xFFFFFF) : 0;
        if (v != want) bad++;
    }
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brt_resize_taller_only) {
    brt_ctx c; int x, y, bad = 0; brt_open(&c, NULL, 6, 6, 0);
    brt_fill_pattern(&c);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 6, 10));
    for (y = 0; y < 10; y++) for (x = 0; x < 6; x++) {
        uint32_t v = brt_px(&c, x, y), want = y < 6 ? (brt_pat(x, y) & 0xFFFFFF) : 0;
        if (v != want) bad++;
    }
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brt_resize_mixed_grow_shrink) {
    brt_ctx c; int x, y, bad = 0; brt_open(&c, NULL, 8, 8, 0);
    brt_fill_pattern(&c);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 12, 3));      /* wider, shorter */
    for (y = 0; y < 3; y++) for (x = 0; x < 12; x++) {
        uint32_t want = x < 8 ? (brt_pat(x, y) & 0xFFFFFF) : 0;
        if (brt_px(&c, x, y) != want) bad++;
    }
    EXPECT_EQ(bad, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 4, 9));       /* narrower, taller: rows >= 3 are zero */
    bad = 0;
    for (y = 0; y < 9; y++) for (x = 0; x < 4; x++) {
        uint32_t want = y < 3 ? (brt_pat(x, y) & 0xFFFFFF) : 0;
        if (brt_px(&c, x, y) != want) bad++;
    }
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brt_resize_content_not_cleared) {
    brt_ctx c; brt_open(&c, NULL, 8, 8, 0);
    brt_fill(&c, 0x00AABBCC);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 8, 9));
    EXPECT_EQ(brt_px(&c, 7, 7), 0x00AABBCC);
    EXPECT_EQ(brt_px(&c, 7, 8), 0);
    brt_close(&c);
}

TEST(brt_resize_gdi_content_preserved) {
    brt_ctx c; RECT r = { 1, 1, 3, 3 }; HBRUSH br = CreateSolidBrush(RGB(10, 20, 30)); brt_open(&c, NULL, 8, 8, 0);
    FillRect(c.dc, &r, br);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 16, 16));
    EXPECT_EQ(GetPixel(c.dc, 2, 2), RGB(10, 20, 30));
    EXPECT_EQ(brt_px(&c, 1, 1), 0x0A141E);
    FillRect(c.dc, &r, br);                         /* GDI keeps working on the new bitmap */
    r.left = 10; r.right = 12; r.top = 10; r.bottom = 12;
    FillRect(c.dc, &r, br);
    EXPECT_EQ(brt_px(&c, 11, 11), 0x0A141E);
    DeleteObject(br); brt_close(&c);
}

TEST(brt_resize_returns_ok_for_valid_table) {
    static const int sz[][2] = { {1,1},{2,3},{64,64},{3,200},{200,3},{17,17},{1,1},{300,300},{5,5} };
    brt_ctx c; int i; brt_open(&c, NULL, 10, 10, 0);
    for (i = 0; i < (int)(sizeof sz / sizeof sz[0]); i++) {
        SIZE s; HRESULT hr = IDWriteBitmapRenderTarget_Resize(c.rt, sz[i][0], sz[i][1]);
        t_check(hr == S_OK, __FILE__, __LINE__, "Resize", "%dx%d -> %s", sz[i][0], sz[i][1], dw_hr_name(hr));
        IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
        t_check(s.cx == sz[i][0] && s.cy == sz[i][1], __FILE__, __LINE__, "GetSize after Resize", "%dx%d got %ldx%ld", sz[i][0], sz[i][1], s.cx, s.cy);
        t_check(brt_px(&c, sz[i][0] - 1, sz[i][1] - 1) != 0xDEADBEEF, __FILE__, __LINE__, "last pixel readable", "%dx%d", sz[i][0], sz[i][1]);
    }
    brt_close(&c);
}

TEST(brt_resize_to_zero_restores_stock_bitmap) {
    brt_ctx c; BITMAP b; HGDIOBJ old; SIZE s; DWORD n0, n1; brt_open(&c, NULL, 10, 10, 0);
    old = GetCurrentObject(c.dc, OBJ_BITMAP); n0 = brt_gdi_count();
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 0, 0));
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
    EXPECT_EQ(s.cx, 0); EXPECT_EQ(s.cy, 0);
    memset(&b, 0, sizeof b); GetObjectA(GetCurrentObject(c.dc, OBJ_BITMAP), sizeof b, &b);
    EXPECT_EQ(b.bmWidth, 1); EXPECT_EQ(b.bmHeight, 1); EXPECT_EQ(b.bmBitsPixel, 1);
    n1 = brt_gdi_settle(n0 - 1);
    t_check(n1 < n0, __FILE__, __LINE__, "10x10 DIB deleted", "GDI objects %lu before Resize(0,0), %lu after", n0, n1);
    brt_close(&c);
}

TEST(brt_resize_from_zero_creates_zeroed_dib) {
    brt_ctx c; BITMAP b; IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL; HDC dc;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    DW_REQUIRE_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 0, 0, &rt));
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(rt, 6, 4));
    dc = IDWriteBitmapRenderTarget_GetMemoryDC(rt);
    memset(&b, 0, sizeof b); GetObjectA(GetCurrentObject(dc, OBJ_BITMAP), sizeof b, &b);
    EXPECT_EQ(b.bmWidth, 6); EXPECT_EQ(b.bmHeight, 4); EXPECT_EQ(b.bmBitsPixel, 32);
    EXPECT_EQ(GetPixel(dc, 5, 3), RGB(0, 0, 0));
    DW_RELEASE(rt); DW_RELEASE(gi);
    (void)c;
}

TEST(brt_resize_zero_dimension_one_axis) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 10, 10, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 10, 0));
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s); EXPECT_EQ(s.cx, 10); EXPECT_EQ(s.cy, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 0, 10));
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s); EXPECT_EQ(s.cx, 0); EXPECT_EQ(s.cy, 10);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 4, 4));
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s); EXPECT_EQ(s.cx, 4); EXPECT_EQ(s.cy, 4);
    EXPECT_EQ(brt_px(&c, 3, 3), 0);
    brt_close(&c);
}

TEST(brt_resize_through_rt1_pointer) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 10, 10, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_Resize(c.rt1, 3, 4));
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
    EXPECT_EQ(s.cx, 3); EXPECT_EQ(s.cy, 4);
    brt_close(&c);
}

TEST(brt_resize_keeps_properties) {
    brt_ctx c; DWRITE_MATRIX m = brt_mat(2, 0, 0, 3, 4, 5), g; brt_open(&c, NULL, 10, 10, 0);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 1.5f);
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
    if (c.rt1) IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 40, 40));
    CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(c.rt), 1.5f);
    IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g);
    CHECK(brt_mat_eq(&g, &m));
    if (c.rt1) EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1), DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    brt_close(&c);
}

TEST(brt_resize_keeps_dc_state) {
    brt_ctx c; brt_open(&c, NULL, 10, 10, 0);
    SetBkColor(c.dc, RGB(1, 2, 3));
    EXPECT_EQ(GetGraphicsMode(c.dc), GM_ADVANCED);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 30, 30));
    EXPECT_EQ(GetGraphicsMode(c.dc), GM_ADVANCED);
    EXPECT_EQ(GetBkColor(c.dc), RGB(1, 2, 3));
    brt_close(&c);
}

TEST(brt_resize_repeated_cycles) {
    brt_ctx c; int i, bad = 0; brt_open(&c, NULL, 4, 4, 0);
    brt_fill(&c, 0x00123456);
    for (i = 0; i < 60; i++) {
        int w = 4 + (i * 7) % 29, h = 4 + (i * 5) % 23; SIZE s;
        if (FAILED(IDWriteBitmapRenderTarget_Resize(c.rt, w, h))) bad++;
        IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
        if (s.cx != w || s.cy != h) bad++;
        if (brt_px(&c, 0, 0) != 0x00123456) bad++;                    /* top-left pixel survives every resize (min size >= 4) */
    }
    EXPECT_EQ(bad, 0);
    brt_close(&c);
}

TEST(brt_resize_large_and_back) {
    brt_ctx c; brt_open(&c, NULL, 8, 8, 0);
    brt_fill(&c, 0x00FEDCBA);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 3000, 3000));
    EXPECT_EQ(brt_px(&c, 7, 7), 0x00FEDCBA); EXPECT_EQ(brt_px(&c, 8, 8), 0); EXPECT_EQ(brt_px(&c, 2999, 2999), 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 2, 2));
    EXPECT_EQ(brt_px(&c, 1, 1), 0x00FEDCBA);
    brt_close(&c);
}

TEST(brt_resize_then_draw) {
    BRT_FIXTURE(c, 16, 16); RECT bb; int ox = 20, oy = 60;
    if (!c.rt) return;
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 100, 100));
    brt_fill(&c, 0x00FFFFFF);
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, (float)ox, (float)oy, 0x000000, &bb));
    EXPECT_EQ(brt_px(&c, ox + 20, oy - 20), 0x000000);
    EXPECT_EQ(brt_px(&c, 2, 2), 0xFFFFFF);
    brt_close(&c);
}

/* ---- pixels per DIP ------------------------------------------------------------------------------------------- */
TEST(brt_ppd_default_from_dc_dpi) {
    brt_ctx c; float p, want; brt_open(&c, NULL, 4, 4, 0);
    p = IDWriteBitmapRenderTarget_GetPixelsPerDip(c.rt);
    want = (float)GetDeviceCaps(c.dc, LOGPIXELSX) / 96.0f;           /* constructor: GetDeviceCaps(dc, LOGPIXELSX) * (1/96) */
    CHECK_NEAR(p, want, 1e-5);
    t_note("default pixelsPerDip %g (LOGPIXELSX %d)", p, GetDeviceCaps(c.dc, LOGPIXELSX));
    brt_close(&c);
}

TEST(brt_ppd_default_independent_of_reference_dc) {
    HDC sdc = GetDC(NULL), mdc = CreateCompatibleDC(sdc); brt_ctx a, b;
    brt_open(&a, NULL, 4, 4, 0); brt_open(&b, mdc, 4, 4, 0);
    CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(a.rt), IDWriteBitmapRenderTarget_GetPixelsPerDip(b.rt));
    brt_close(&a); brt_close(&b); DeleteDC(mdc); ReleaseDC(NULL, sdc);
}

TEST(brt_ppd_set_get_roundtrip_table) {
    static const float v[] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 8.0f, 0.001f, 100.0f, 1e6f, 1e-30f };
    brt_ctx c; int i; brt_open(&c, NULL, 4, 4, 0);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        HRESULT hr = IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, v[i]);
        float g = IDWriteBitmapRenderTarget_GetPixelsPerDip(c.rt);
        t_check(hr == S_OK && g == v[i], __FILE__, __LINE__, "ppd roundtrip", "set %g -> %s get %g", v[i], dw_hr_name(hr), g);
    }
    brt_close(&c);
}

TEST(brt_ppd_invalid_values_rejected) {
    static const float v[] = { 0.0f, -0.0f, -1.0f, -0.5f, -2.0f, -1e-30f, -1e30f, -HUGE_VALF };
    brt_ctx c; int i; brt_open(&c, NULL, 4, 4, 0);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 1.75f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        HRESULT hr = IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, v[i]);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "invalid ppd", "set %g -> %s", v[i], dw_hr_name(hr));
        CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(c.rt), 1.75f);       /* unchanged on failure */
    }
    brt_close(&c);
}

TEST(brt_ppd_nan_is_noted) {
    brt_ctx c; HRESULT hr; brt_open(&c, NULL, 4, 4, 0);
    hr = IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, NAN);
    t_note("SetPixelsPerDip(NaN) -> %s", dw_hr_name(hr));
    IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 1.0f);
    CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(c.rt), 1.0f);
    brt_close(&c);
}

TEST(brt_ppd_per_target) {
    brt_ctx a, b; brt_open(&a, NULL, 4, 4, 0); brt_open(&b, NULL, 4, 4, 0);
    float db = IDWriteBitmapRenderTarget_GetPixelsPerDip(b.rt);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(a.rt, 3.0f);
    CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(a.rt), 3.0f);
    CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(b.rt), db);
    brt_close(&a); brt_close(&b);
}

TEST(brt_ppd_via_rt1_pointer) {
    brt_ctx c; brt_open(&c, NULL, 4, 4, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_SetPixelsPerDip(c.rt1, 2.0f));
    CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(c.rt), 2.0f);
    CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget1_GetPixelsPerDip(c.rt1), 2.0f);
    brt_close(&c);
}

TEST(brt_ppd_does_not_touch_dc_or_size) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 9, 7, 0);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 4.0f);
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
    EXPECT_EQ(s.cx, 9); EXPECT_EQ(s.cy, 7);
    EXPECT_EQ(GetObjectType(c.dc), OBJ_MEMDC);
    brt_close(&c);
}

TEST(brt_ppd_scales_box_glyph) {
    /* em 32 DIPs at 2 pixels per DIP covers the same pixels as em 64 at 1 pixel per DIP (aliased rendering) */
    BRT_FIXTURE(c, 120, 120); RECT r1, r2; int n1, n2;
    if (!c.rt) return;
    brt_fill(&c, 0xFFFFFF);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 1.0f);
    CHECK_HR_OK(brt_draw(&c, "A", 64.0f, 20.0f, 90.0f, 0x000000, NULL));
    n1 = brt_bbox_not(&c, 0xFFFFFF, &r1);
    brt_fill(&c, 0xFFFFFF);
    IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 2.0f);
    CHECK_HR_OK(brt_draw(&c, "A", 32.0f, 10.0f, 45.0f, 0x000000, NULL));          /* origin is in DIPs too */
    n2 = brt_bbox_not(&c, 0xFFFFFF, &r2);
    CHECK(n1 && n2);
    t_note("ppd1 box %ld,%ld-%ld,%ld ; ppd2 box %ld,%ld-%ld,%ld", r1.left, r1.top, r1.right, r1.bottom, r2.left, r2.top, r2.right, r2.bottom);
    CHECK(labs(r1.left - r2.left) <= 2 && labs(r1.right - r2.right) <= 2 && labs(r1.top - r2.top) <= 2 && labs(r1.bottom - r2.bottom) <= 2);
    brt_close(&c);
}

/* ---- current transform -------------------------------------------------------------------------------------- */
TEST(brt_xform_default_identity) {
    brt_ctx c; DWRITE_MATRIX m; brt_open(&c, NULL, 4, 4, 0);
    memset(&m, 0x7E, sizeof m);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &m));
    CHECK_FLOAT_EQ(m.m11, 1); CHECK_FLOAT_EQ(m.m12, 0); CHECK_FLOAT_EQ(m.m21, 0); CHECK_FLOAT_EQ(m.m22, 1); CHECK_FLOAT_EQ(m.dx, 0); CHECK_FLOAT_EQ(m.dy, 0);
    brt_close(&c);
}

TEST(brt_xform_roundtrip_table) {
    static const DWRITE_MATRIX tab[] = {
        { 2, 0, 0, 2, 0, 0 }, { 0.5f, 0, 0, 0.5f, 0, 0 }, { 1, 0, 0, 1, 5, 7 }, { 0, 1, -1, 0, 0, 0 }, { 0, -1, 1, 0, 3, 3 },
        { -1, 0, 0, 1, 0, 0 }, { 1, 0, 0, -1, 0, 0 }, { 1, 0.5f, 0, 1, 0, 0 }, { 1, 0, 0.25f, 1, 0, 0 }, { 0.7071f, 0.7071f, -0.7071f, 0.7071f, 1, 2 },
        { 3, 4, 5, 6, 7, 8 }, { 1e-3f, 0, 0, 1e3f, -100, 100 }, { 0, 0, 0, 0, 0, 0 }, { 1, 0, 0, 0, 0, 0 }, { -3, -3, -3, -3, -3, -3 },
    };
    brt_ctx c; int i; brt_open(&c, NULL, 4, 4, 0);
    for (i = 0; i < (int)(sizeof tab / sizeof tab[0]); i++) {
        DWRITE_MATRIX g; HRESULT hr = IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &tab[i]);
        memset(&g, 0, sizeof g);
        IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g);
        t_check(hr == S_OK && brt_mat_eq(&g, &tab[i]), __FILE__, __LINE__, "transform roundtrip", "entry %d hr=%s got {%g %g %g %g %g %g}", i, dw_hr_name(hr), g.m11, g.m12, g.m21, g.m22, g.dx, g.dy);
    }
    brt_close(&c);
}

TEST(brt_xform_null_resets_to_identity) {
    brt_ctx c; DWRITE_MATRIX m = brt_mat(2, 0, 0, 2, 9, 9), g; brt_open(&c, NULL, 4, 4, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m));
    CHECK_HR_OK(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, NULL));
    IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g);
    CHECK_FLOAT_EQ(g.m11, 1); CHECK_FLOAT_EQ(g.m22, 1); CHECK_FLOAT_EQ(g.m12, 0); CHECK_FLOAT_EQ(g.m21, 0); CHECK_FLOAT_EQ(g.dx, 0); CHECK_FLOAT_EQ(g.dy, 0);
    brt_close(&c);
}

TEST(brt_xform_explicit_identity_reads_identity) {
    brt_ctx c; DWRITE_MATRIX m = brt_mat(1, 0, 0, 1, 0, 0), g, big = brt_mat(5, 0, 0, 5, 1, 1); brt_open(&c, NULL, 4, 4, 0);
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &big);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m));
    IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g);
    CHECK(brt_mat_eq(&g, &m));
    brt_close(&c);
}

TEST(brt_xform_nan_rejected_each_member) {
    brt_ctx c; int i; DWRITE_MATRIX base = brt_mat(2, 0.5f, 0.25f, 3, 4, 5), g; brt_open(&c, NULL, 4, 4, 0);
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &base);
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX m = base; float *p = &m.m11 + i; HRESULT hr;
        *p = NAN;
        hr = IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "NaN member", "member %d -> %s", i, dw_hr_name(hr));
        IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g);
        t_check(brt_mat_eq(&g, &base), __FILE__, __LINE__, "unchanged after NaN", "member %d", i);
    }
    brt_close(&c);
}

TEST(brt_xform_nan_identity_otherwise_still_rejected) {
    brt_ctx c; DWRITE_MATRIX m = brt_mat(1, 0, 0, 1, 0, 0); brt_open(&c, NULL, 4, 4, 0);
    m.dx = NAN;
    CHECK_HR(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m), E_INVALIDARG);
    brt_close(&c);
}

TEST(brt_xform_infinity_not_validated) {
    brt_ctx c; DWRITE_MATRIX m = brt_mat(INFINITY, 0, 0, 1, 0, 0), g; brt_open(&c, NULL, 4, 4, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m));
    IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g);
    CHECK(g.m11 == INFINITY);
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, NULL);
    brt_close(&c);
}

TEST(brt_xform_singular_accepted) {
    brt_ctx c; DWRITE_MATRIX m = brt_mat(1, 2, 2, 4, 0, 0), g; brt_open(&c, NULL, 4, 4, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m));
    IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g);
    CHECK(brt_mat_eq(&g, &m));
    brt_close(&c);
}

TEST(brt_xform_per_target) {
    brt_ctx a, b; DWRITE_MATRIX m = brt_mat(2, 0, 0, 2, 0, 0), g; brt_open(&a, NULL, 4, 4, 0); brt_open(&b, NULL, 4, 4, 0);
    IDWriteBitmapRenderTarget_SetCurrentTransform(a.rt, &m);
    IDWriteBitmapRenderTarget_GetCurrentTransform(b.rt, &g);
    CHECK_FLOAT_EQ(g.m11, 1);
    brt_close(&a); brt_close(&b);
}

TEST(brt_xform_not_the_dc_world_transform) {
    /* docs: unrelated to the world transform of the underlying DC */
    brt_ctx c; DWRITE_MATRIX m = brt_mat(3, 0, 0, 3, 5, 5); XFORM x; brt_open(&c, NULL, 4, 4, 0);
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
    GetWorldTransform(c.dc, &x);
    CHECK_FLOAT_EQ(x.eM11, 1.0f); CHECK_FLOAT_EQ(x.eDx, 0.0f);
    brt_close(&c);
}

TEST(brt_xform_via_rt1_pointer) {
    brt_ctx c; DWRITE_MATRIX m = brt_mat(1, 0, 0, 2, 0, 0), g; brt_open(&c, NULL, 4, 4, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_SetCurrentTransform(c.rt1, &m));
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g));
    CHECK(brt_mat_eq(&g, &m));
    brt_close(&c);
}

TEST(brt_xform_set_does_not_alias_caller_memory) {
    brt_ctx c; DWRITE_MATRIX m = brt_mat(2, 0, 0, 2, 0, 0), g; brt_open(&c, NULL, 4, 4, 0);
    IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m);
    m.m11 = 9; m.dx = 9;
    IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g);
    CHECK_FLOAT_EQ(g.m11, 2); CHECK_FLOAT_EQ(g.dx, 0);
    brt_close(&c);
}

/* ---- text antialias mode (IDWriteBitmapRenderTarget1) -------------------------------------------------------- */
TEST(brt_aa_enum_values) {
    EXPECT_EQ(DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0);
    EXPECT_EQ(DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 1);
}

TEST(brt_aa_default_cleartype) {
    brt_ctx c; brt_open(&c, NULL, 4, 4, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1), DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    brt_close(&c);
}

TEST(brt_aa_set_get_roundtrip) {
    brt_ctx c; brt_open(&c, NULL, 4, 4, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE));
    EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1), DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE));
    EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1), DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE));    /* idempotent */
    EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1), DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    brt_close(&c);
}

TEST(brt_aa_invalid_modes_rejected) {
    static const UINT32 bad[] = { 2, 3, 4, 7, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    brt_ctx c; int i; brt_open(&c, NULL, 4, 4, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        HRESULT hr = IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, (DWRITE_TEXT_ANTIALIAS_MODE)bad[i]);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "invalid AA mode", "mode %u -> %s", bad[i], dw_hr_name(hr));
        t_check(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1) == DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, __FILE__, __LINE__, "unchanged", "mode %u", bad[i]);
    }
    brt_close(&c);
}

TEST(brt_aa_second_qi_pointer_sees_state) {
    brt_ctx c; IDWriteBitmapRenderTarget1 *other = NULL; brt_open(&c, NULL, 4, 4, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    CHECK_HR_OK(IDWriteBitmapRenderTarget_QueryInterface(c.rt, &IID_IDWriteBitmapRenderTarget1, (void **)&other));
    IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    if (other) EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(other), DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    DW_RELEASE(other);
    brt_close(&c);
}

TEST(brt_aa_per_target_and_resize_independent) {
    brt_ctx a, b; brt_open(&a, NULL, 4, 4, 0); brt_open(&b, NULL, 4, 4, 0);
    if (!a.rt1 || !b.rt1) { brt_close(&a); brt_close(&b); t_skip("no RT1"); return; }
    IDWriteBitmapRenderTarget1_SetTextAntialiasMode(a.rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(b.rt1), DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    IDWriteBitmapRenderTarget_Resize(a.rt, 9, 9);
    EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(a.rt1), DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    brt_close(&a); brt_close(&b);
}

TEST(brt_aa_rt1_always_available_on_win10) {
    /* the object is one class implementing both interfaces */
    brt_ctx c; brt_open(&c, NULL, 4, 4, 0);
    CHECK(c.rt1 != NULL);
    brt_close(&c);
}
