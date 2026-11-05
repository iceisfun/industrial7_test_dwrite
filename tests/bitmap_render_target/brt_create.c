/* Creation, memory DC / DIB section properties, GetSize, QueryInterface, lifetime of IDWriteBitmapRenderTarget.
 *
 * Facts from the implementation (DWriteBitmapRenderTarget ctor / MemoryBitmap / MemoryDC):
 *  - the target always owns a memory DC made with CreateCompatibleDC(NULL); the hdc argument is only used as the reference DC for
 *    CreateDIBSection and is never modified or retained;
 *  - the DIB is 32bpp BI_RGB top-down, zero initialised; the DC is switched to GM_ADVANCED;
 *  - width or height 0 is accepted: no DIB is created, the DC keeps its default 1x1 monochrome bitmap and DrawGlyphRun is a no-op;
 *  - the object answers QueryInterface for IUnknown, IDWriteBitmapRenderTarget and IDWriteBitmapRenderTarget1 only.
 */
#include "brt.h"
static const IID brt_zero_iid = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };

/* ---- creation ------------------------------------------------------------------------------------------------ */
TEST(brt_create_basic) {
    brt_ctx c; brt_open(&c, NULL, 32, 16, 0);
    CHECK(c.rt != NULL);
    EXPECT_REFS(c.rt, 1);
    CHECK(c.dc != NULL);
    brt_close(&c);
}

TEST(brt_create_returns_s_ok_and_object) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = (IDWriteBitmapRenderTarget *)(UINT_PTR)0x1;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 4, 4, &rt));
    CHECK(rt != NULL && rt != (IDWriteBitmapRenderTarget *)(UINT_PTR)0x1);
    DW_RELEASE(rt); DW_RELEASE(gi);
}

TEST(brt_create_two_distinct_objects) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *a = NULL, *b = NULL;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 4, 4, &a));
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 4, 4, &b));
    CHECK(a && b && a != b);
    if (a && b) {
        HDC da = IDWriteBitmapRenderTarget_GetMemoryDC(a), db = IDWriteBitmapRenderTarget_GetMemoryDC(b);
        CHECK(da != db);
        EXPECT_REFS(a, 1); EXPECT_REFS(b, 1);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(gi);
}

TEST(brt_create_screen_dc) {
    HDC sdc = GetDC(NULL); brt_ctx c;
    brt_open(&c, sdc, 20, 10, 0);
    CHECK(c.rt != NULL);
    CHECK(c.dc != sdc);
    brt_close(&c);
    ReleaseDC(NULL, sdc);
}

TEST(brt_create_memory_dc) {
    HDC sdc = GetDC(NULL), mdc = CreateCompatibleDC(sdc); brt_ctx c;
    brt_open(&c, mdc, 20, 10, 0);
    CHECK(c.dc != mdc);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(c.rt, 8, 8));
    brt_close(&c);
    CHECK(GetObjectType(mdc) == OBJ_MEMDC);        /* the caller's DC is untouched */
    DeleteDC(mdc); ReleaseDC(NULL, sdc);
}

TEST(brt_create_memory_dc_with_bitmap_selected) {
    HDC sdc = GetDC(NULL), mdc = CreateCompatibleDC(sdc); HBITMAP bm = CreateCompatibleBitmap(sdc, 7, 5), old; brt_ctx c; BITMAP b;
    old = (HBITMAP)SelectObject(mdc, bm);
    brt_open(&c, mdc, 20, 10, 0);
    /* the caller's DC keeps its own bitmap */
    CHECK(GetCurrentObject(mdc, OBJ_BITMAP) == (HGDIOBJ)bm);
    CHECK(GetCurrentObject(c.dc, OBJ_BITMAP) != (HGDIOBJ)bm);
    memset(&b, 0, sizeof b); GetObjectA(GetCurrentObject(c.dc, OBJ_BITMAP), sizeof b, &b);
    EXPECT_EQ(b.bmWidth, 20); EXPECT_EQ(b.bmHeight, 10);
    brt_close(&c);
    CHECK(GetCurrentObject(mdc, OBJ_BITMAP) == (HGDIOBJ)bm);
    SelectObject(mdc, old); DeleteObject(bm); DeleteDC(mdc); ReleaseDC(NULL, sdc);
}

TEST(brt_create_hdc_does_not_affect_bitmap_format) {
    /* a 1bpp / 16bpp reference DC bitmap does not change the 32bpp DIB the target creates */
    HDC sdc = GetDC(NULL), mdc = CreateCompatibleDC(sdc); HBITMAP mono = CreateBitmap(4, 4, 1, 1, NULL), old; brt_ctx c; DIBSECTION ds;
    old = (HBITMAP)SelectObject(mdc, mono);
    brt_open(&c, mdc, 9, 3, 0);
    memset(&ds, 0, sizeof ds);
    EXPECT_EQ(GetObjectA(GetCurrentObject(c.dc, OBJ_BITMAP), sizeof ds, &ds), sizeof ds);
    EXPECT_EQ(ds.dsBm.bmBitsPixel, 32); EXPECT_EQ(ds.dsBm.bmWidth, 9); EXPECT_EQ(ds.dsBm.bmHeight, 3);
    brt_close(&c);
    SelectObject(mdc, old); DeleteObject(mono); DeleteDC(mdc); ReleaseDC(NULL, sdc);
}

TEST(brt_create_dc_not_released_by_target) {
    HDC sdc = GetDC(NULL); brt_ctx c;
    brt_open(&c, sdc, 5, 5, 0);
    brt_close(&c);
    CHECK(GetDeviceCaps(sdc, BITSPIXEL) > 0);      /* still a usable DC */
    ReleaseDC(NULL, sdc);
}

/* ---- memory DC ----------------------------------------------------------------------------------------------- */
TEST(brt_memdc_valid_handle) {
    brt_ctx c; brt_open(&c, NULL, 10, 10, 0);
    CHECK(c.dc != NULL);
    EXPECT_EQ(GetObjectType(c.dc), OBJ_MEMDC);
    brt_close(&c);
}

TEST(brt_memdc_stable_between_calls) {
    brt_ctx c; brt_open(&c, NULL, 10, 10, 0);
    CHECK(IDWriteBitmapRenderTarget_GetMemoryDC(c.rt) == c.dc);
    CHECK(IDWriteBitmapRenderTarget_GetMemoryDC(c.rt) == c.dc);
    EXPECT_REFS(c.rt, 1);                          /* GetMemoryDC does not AddRef */
    brt_close(&c);
}

TEST(brt_memdc_same_from_rt1_pointer) {
    brt_ctx c; brt_open(&c, NULL, 10, 10, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    CHECK(IDWriteBitmapRenderTarget1_GetMemoryDC(c.rt1) == c.dc);
    brt_close(&c);
}

TEST(brt_memdc_graphics_mode_advanced) {
    brt_ctx c; brt_open(&c, NULL, 10, 10, 0);
    EXPECT_EQ(GetGraphicsMode(c.dc), GM_ADVANCED);     /* the constructor calls SetGraphicsMode(GM_ADVANCED) */
    brt_close(&c);
}

TEST(brt_memdc_world_transform_identity) {
    brt_ctx c; XFORM x; brt_open(&c, NULL, 10, 10, 0);
    memset(&x, 0, sizeof x);
    CHECK(GetWorldTransform(c.dc, &x));
    CHECK_FLOAT_EQ(x.eM11, 1.0f); CHECK_FLOAT_EQ(x.eM22, 1.0f); CHECK_FLOAT_EQ(x.eM12, 0.0f); CHECK_FLOAT_EQ(x.eM21, 0.0f);
    CHECK_FLOAT_EQ(x.eDx, 0.0f); CHECK_FLOAT_EQ(x.eDy, 0.0f);
    brt_close(&c);
}

TEST(brt_memdc_bitmap_is_selected_dib) {
    brt_ctx c; DIBSECTION ds; HGDIOBJ bmp;
    brt_open(&c, NULL, 37, 19, 0);
    bmp = GetCurrentObject(c.dc, OBJ_BITMAP);
    CHECK(bmp != NULL);
    EXPECT_EQ(GetObjectType(bmp), OBJ_BITMAP);
    memset(&ds, 0, sizeof ds);
    EXPECT_EQ(GetObjectA(bmp, sizeof ds, &ds), sizeof ds);           /* DIB sections report a DIBSECTION */
    EXPECT_EQ(ds.dsBm.bmWidth, 37); EXPECT_EQ(ds.dsBm.bmHeight, 19);
    EXPECT_EQ(ds.dsBm.bmBitsPixel, 32); EXPECT_EQ(ds.dsBm.bmPlanes, 1);
    EXPECT_EQ(ds.dsBm.bmWidthBytes, 37 * 4);
    CHECK(ds.dsBm.bmBits != NULL);
    EXPECT_EQ(ds.dsBmih.biSize, sizeof(BITMAPINFOHEADER));
    EXPECT_EQ(ds.dsBmih.biPlanes, 1); EXPECT_EQ(ds.dsBmih.biBitCount, 32);
    EXPECT_EQ(ds.dsBmih.biCompression, BI_RGB);
    EXPECT_EQ(ds.dsBmih.biWidth, 37);
    CHECK(ds.dsBmih.biHeight == -19 || ds.dsBmih.biHeight == 19);
    t_note("DIBSECTION biHeight=%ld (top-down creation)", (long)ds.dsBmih.biHeight);
    brt_close(&c);
}

TEST(brt_memdc_bitmap_top_down_orientation) {
    /* row 0 of the bits is the top row: writing the first dword is visible at (0,0) through GDI */
    brt_ctx c; int w, h; uint32_t *b;
    brt_open(&c, NULL, 4, 4, 0);
    b = brt_bits(&c, &w, &h);
    CHECK(b != NULL);
    if (b) {
        b[0] = 0x00112233; GdiFlush();
        EXPECT_EQ(GetPixel(c.dc, 0, 0), RGB(0x11, 0x22, 0x33));
        EXPECT_EQ(GetPixel(c.dc, 0, 3), RGB(0, 0, 0));
        b[3 * 4] = 0x00445566; GdiFlush();
        EXPECT_EQ(GetPixel(c.dc, 0, 3), RGB(0x44, 0x55, 0x66));
    }
    brt_close(&c);
}

TEST(brt_memdc_getobject_bitmap_size_table) {
    static const int sz[][2] = { {1,1},{2,1},{1,2},{3,3},{5,7},{16,16},{17,31},{64,1},{1,64},{100,50},{333,22},{640,480} };
    int i;
    for (i = 0; i < (int)(sizeof sz / sizeof sz[0]); i++) {
        brt_ctx c; BITMAP b; SIZE s;
        brt_open(&c, NULL, sz[i][0], sz[i][1], 0);
        memset(&b, 0, sizeof b);
        GetObjectA(GetCurrentObject(c.dc, OBJ_BITMAP), sizeof b, &b);
        t_check(b.bmWidth == sz[i][0] && b.bmHeight == sz[i][1], __FILE__, __LINE__, "bitmap size", "%dx%d got %ldx%ld", sz[i][0], sz[i][1], b.bmWidth, b.bmHeight);
        memset(&s, 0xCC, sizeof s);
        CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(c.rt, &s));
        t_check(s.cx == sz[i][0] && s.cy == sz[i][1], __FILE__, __LINE__, "GetSize", "%dx%d got %ldx%ld", sz[i][0], sz[i][1], s.cx, s.cy);
        brt_close(&c);
    }
}

TEST(brt_memdc_initial_pixels_zero) {
    brt_ctx c; int w, h, i, nz = 0; uint32_t *b;
    brt_open(&c, NULL, 40, 30, 0);
    b = brt_bits(&c, &w, &h);
    CHECK(b != NULL);
    if (b) { for (i = 0; i < w * h; i++) if ((b[i] & 0xFFFFFF) != 0) nz++; }
    EXPECT_EQ(nz, 0);
    brt_close(&c);
}

TEST(brt_memdc_getdibits_matches_bits) {
    /* pixel readback through GetDIBits (the documented route) agrees with the DIB section memory */
    brt_ctx c; int w, h, x, y, bad = 0; uint32_t *b; BITMAPINFO bi; uint32_t *buf; HDC sdc;
    brt_open(&c, NULL, 6, 5, 0);
    b = brt_bits(&c, &w, &h);
    if (!b) { brt_close(&c); t_skip("no bits"); return; }
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) b[y * w + x] = (uint32_t)((x * 40) << 16 | (y * 50) << 8 | (x + y));
    GdiFlush();
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    buf = (uint32_t *)calloc((size_t)w * h, 4);
    sdc = GetDC(NULL);
    EXPECT_EQ(GetDIBits(sdc, (HBITMAP)GetCurrentObject(c.dc, OBJ_BITMAP), 0, h, buf, &bi, DIB_RGB_COLORS), h);
    ReleaseDC(NULL, sdc);
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) if ((buf[y * w + x] & 0xFFFFFF) != (b[y * w + x] & 0xFFFFFF)) bad++;
    EXPECT_EQ(bad, 0);
    free(buf);
    brt_close(&c);
}

TEST(brt_memdc_gdi_fillrect_visible_in_bits) {
    brt_ctx c; RECT r = { 2, 1, 5, 3 }; HBRUSH br = CreateSolidBrush(RGB(0xAA, 0x55, 0x11));
    brt_open(&c, NULL, 8, 6, 0);
    FillRect(c.dc, &r, br);
    EXPECT_EQ(brt_px(&c, 2, 1), 0xAA5511); EXPECT_EQ(brt_px(&c, 4, 2), 0xAA5511);
    EXPECT_EQ(brt_px(&c, 5, 2), 0); EXPECT_EQ(brt_px(&c, 1, 1), 0); EXPECT_EQ(brt_px(&c, 2, 3), 0);
    EXPECT_EQ(brt_count_not(&c, 0), 6);
    DeleteObject(br); brt_close(&c);
}

TEST(brt_memdc_setpixel_getpixel_roundtrip) {
    brt_ctx c; brt_open(&c, NULL, 8, 8, 0);
    SetPixelV(c.dc, 3, 4, RGB(1, 2, 3));
    EXPECT_EQ(GetPixel(c.dc, 3, 4), RGB(1, 2, 3));
    EXPECT_EQ(brt_px(&c, 3, 4), 0x010203);          /* COLORREF 0x00BBGGRR is stored as xRGB */
    brt_close(&c);
}

TEST(brt_memdc_clip_box_is_bitmap) {
    brt_ctx c; RECT r; int t;
    brt_open(&c, NULL, 23, 11, 0);
    memset(&r, 0, sizeof r);
    t = GetClipBox(c.dc, &r);
    CHECK(t == SIMPLEREGION || t == NULLREGION || t == COMPLEXREGION);
    EXPECT_EQ(r.left, 0); EXPECT_EQ(r.top, 0); EXPECT_EQ(r.right, 23); EXPECT_EQ(r.bottom, 11);
    brt_close(&c);
}

TEST(brt_memdc_bitmap_survives_repeated_queries) {
    brt_ctx c; HGDIOBJ a, b; brt_open(&c, NULL, 12, 12, 0);
    a = GetCurrentObject(c.dc, OBJ_BITMAP); b = GetCurrentObject(c.dc, OBJ_BITMAP);
    CHECK(a == b); CHECK(a != NULL);
    brt_close(&c);
}

/* ---- GetSize --------------------------------------------------------------------------------------------------- */
TEST(brt_getsize_basic) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 123, 45, 0);
    memset(&s, 0, sizeof s);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(c.rt, &s));
    EXPECT_EQ(s.cx, 123); EXPECT_EQ(s.cy, 45);
    brt_close(&c);
}

TEST(brt_getsize_via_rt1) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 13, 55, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    memset(&s, 0, sizeof s);
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_GetSize(c.rt1, &s));
    EXPECT_EQ(s.cx, 13); EXPECT_EQ(s.cy, 55);
    brt_close(&c);
}

TEST(brt_getsize_repeatable) {
    brt_ctx c; SIZE a, b; brt_open(&c, NULL, 30, 20, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(c.rt, &a));
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(c.rt, &b));
    EXPECT_EQ(a.cx, b.cx); EXPECT_EQ(a.cy, b.cy);
    brt_close(&c);
}

TEST(brt_getsize_width_height_not_swapped) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 9, 70, 0);
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
    CHECK(s.cx == 9 && s.cy == 70);
    brt_close(&c);
}

/* ---- size limits ----------------------------------------------------------------------------------------------- */
TEST(brt_size_1x1) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 1, 1, 1);
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
    EXPECT_EQ(s.cx, 1); EXPECT_EQ(s.cy, 1);
    EXPECT_EQ(brt_px(&c, 0, 0), 0);
    EXPECT_EQ(brt_px(&c, 1, 0), 0xDEADBEEF);        /* helper sentinel: outside */
    brt_close(&c);
}

TEST(brt_size_1x1_draw_ok) {
    brt_ctx c; RECT bb; brt_open(&c, NULL, 1, 1, 1);
    memset(&bb, 0x55, sizeof bb);
    CHECK_HR_OK(brt_draw(&c, "A", 8.0f, 0.0f, 1.0f, 0xFFFFFF, &bb));
    brt_close(&c);
}

TEST(brt_size_zero_by_zero_accepted) {
    /* MemoryBitmap creates no DIB for a zero dimension, creation itself does not validate */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL; SIZE s;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 0, 0, &rt));
    CHECK(rt != NULL);
    if (rt) {
        memset(&s, 0x77, sizeof s);
        CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(rt, &s));
        EXPECT_EQ(s.cx, 0); EXPECT_EQ(s.cy, 0);
        CHECK(IDWriteBitmapRenderTarget_GetMemoryDC(rt) != NULL);
    }
    DW_RELEASE(rt); DW_RELEASE(gi);
}

TEST(brt_size_zero_width_or_height) {
    static const int sz[][2] = { {0,5}, {5,0}, {0,1}, {1,0}, {0,1000}, {1000,0} };
    int i; IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    for (i = 0; i < (int)(sizeof sz / sizeof sz[0]); i++) {
        IDWriteBitmapRenderTarget *rt = NULL; SIZE s; HDC dc; BITMAP b;
        HRESULT hr = IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, sz[i][0], sz[i][1], &rt);
        t_check(hr == S_OK && rt, __FILE__, __LINE__, "create", "%dx%d -> %s", sz[i][0], sz[i][1], dw_hr_name(hr));
        if (!rt) continue;
        IDWriteBitmapRenderTarget_GetSize(rt, &s);
        t_check(s.cx == sz[i][0] && s.cy == sz[i][1], __FILE__, __LINE__, "size", "%dx%d got %ldx%ld", sz[i][0], sz[i][1], s.cx, s.cy);
        dc = IDWriteBitmapRenderTarget_GetMemoryDC(rt);
        CHECK(dc != NULL);
        /* the DC still holds its stock 1x1 monochrome bitmap */
        memset(&b, 0, sizeof b);
        GetObjectA(GetCurrentObject(dc, OBJ_BITMAP), sizeof b, &b);
        EXPECT_EQ(b.bmWidth, 1); EXPECT_EQ(b.bmHeight, 1); EXPECT_EQ(b.bmBitsPixel, 1);
        DW_RELEASE(rt);
    }
    DW_RELEASE(gi);
}

TEST(brt_size_zero_draw_is_noop_success) {
    /* with no pixel buffer DrawGlyphRun returns S_OK immediately: no validation, blackBox left untouched */
    brt_ctx c; RECT bb, keep; brt_run r; HRESULT hr;
    brt_open(&c, NULL, 0, 0, 1);
    brt_make_run(&c, &r, "A", 20.0f);
    bb.left = 11; bb.top = 22; bb.right = 33; bb.bottom = 44; keep = bb;
    hr = brt_draw_run(&c, &r, 5.0f, 5.0f, DWRITE_MEASURING_MODE_NATURAL, c.rp_mode[1], 0xFFFFFF, &bb);
    EXPECT_EQ(hr, S_OK);
    CHECK(bb.left == keep.left && bb.top == keep.top && bb.right == keep.right && bb.bottom == keep.bottom);
    hr = IDWriteBitmapRenderTarget_DrawGlyphRun(c.rt, 0, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, NULL, 0, NULL);
    EXPECT_EQ(hr, S_OK);                             /* even invalid arguments succeed on an empty target */
    brt_close(&c);
}

TEST(brt_size_wide_and_tall) {
    static const int sz[][2] = { {4096, 1}, {1, 4096}, {2048, 8}, {8, 2048}, {1000, 1000} };
    int i;
    for (i = 0; i < (int)(sizeof sz / sizeof sz[0]); i++) {
        brt_ctx c; SIZE s; brt_open(&c, NULL, sz[i][0], sz[i][1], 0);
        IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
        t_check(s.cx == sz[i][0] && s.cy == sz[i][1], __FILE__, __LINE__, "size", "%dx%d got %ldx%ld", sz[i][0], sz[i][1], s.cx, s.cy);
        EXPECT_EQ(brt_px(&c, sz[i][0] - 1, sz[i][1] - 1), 0);
        brt_close(&c);
    }
}

TEST(brt_size_4096x4096) {
    brt_ctx c; SIZE s; brt_open(&c, NULL, 4096, 4096, 0);          /* 64 MB DIB */
    IDWriteBitmapRenderTarget_GetSize(c.rt, &s);
    EXPECT_EQ(s.cx, 4096); EXPECT_EQ(s.cy, 4096);
    EXPECT_EQ(brt_px(&c, 4095, 4095), 0); EXPECT_EQ(brt_px(&c, 0, 0), 0);
    brt_close(&c);
}

TEST(brt_size_non_multiple_of_four) {
    static const int w[] = { 1, 2, 3, 5, 6, 7, 9, 13, 31, 33 };
    int i;
    for (i = 0; i < (int)(sizeof w / sizeof w[0]); i++) {
        brt_ctx c; BITMAP b; brt_open(&c, NULL, w[i], 3, 0);
        memset(&b, 0, sizeof b); GetObjectA(GetCurrentObject(c.dc, OBJ_BITMAP), sizeof b, &b);
        t_check(b.bmWidthBytes == w[i] * 4 && b.bmWidth == w[i], __FILE__, __LINE__, "stride", "w=%d bytes=%ld", w[i], b.bmWidthBytes);
        brt_close(&c);
    }
}

/* ---- IUnknown ---------------------------------------------------------------------------------------------------- */
TEST(brt_unknown_addref_release) {
    brt_ctx c; brt_open(&c, NULL, 4, 4, 0);
    EXPECT_EQ(IDWriteBitmapRenderTarget_AddRef(c.rt), 2); EXPECT_EQ(IDWriteBitmapRenderTarget_AddRef(c.rt), 3);
    EXPECT_EQ(IDWriteBitmapRenderTarget_Release(c.rt), 2); EXPECT_EQ(IDWriteBitmapRenderTarget_Release(c.rt), 1);
    brt_close(&c);
}

TEST(brt_unknown_qi_supported_set) {
    brt_ctx c; int n, i; const dw_qi_row *tab; dw_qi_row rows[200];
    brt_open(&c, NULL, 4, 4, 0);
    tab = dw_iid_table(&n);
    if (n > 200) n = 200;
    for (i = 0; i < n; i++) {
        rows[i] = tab[i];
        rows[i].expect = (tab[i].iid == &IID_IDWriteBitmapRenderTarget || tab[i].iid == &IID_IDWriteBitmapRenderTarget1 ||
                          IsEqualIID(tab[i].iid, &IID_IDWriteBitmapRenderTarget) || IsEqualIID(tab[i].iid, &IID_IDWriteBitmapRenderTarget1)) ? 1 : 0;
        if (IsEqualIID(tab[i].iid, &DW_IID_IDWriteBitmapRenderTarget2) || IsEqualIID(tab[i].iid, &DW_IID_IDWriteBitmapRenderTarget3))
            rows[i].expect = 0;
    }
    dw_qi_check_((IUnknown *)c.rt, rows, n, __FILE__, __LINE__);
    brt_close(&c);
}

TEST(brt_unknown_qi_all_three_same_pointer) {
    brt_ctx c; void *a = NULL, *b = NULL, *u = NULL; brt_open(&c, NULL, 4, 4, 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_QueryInterface(c.rt, &IID_IUnknown, &u));
    CHECK_HR_OK(IDWriteBitmapRenderTarget_QueryInterface(c.rt, &IID_IDWriteBitmapRenderTarget, &a));
    CHECK_HR_OK(IDWriteBitmapRenderTarget_QueryInterface(c.rt, &IID_IDWriteBitmapRenderTarget1, &b));
    CHECK(u == (void *)c.rt); CHECK(a == (void *)c.rt); CHECK(b == (void *)c.rt);
    EXPECT_REFS(c.rt, 4);
    IUnknown_Release((IUnknown *)u); IUnknown_Release((IUnknown *)a); IUnknown_Release((IUnknown *)b);
    EXPECT_REFS(c.rt, 1);
    brt_close(&c);
}

TEST(brt_unknown_qi_failure_zeroes_out) {
    brt_ctx c; void *p = (void *)(UINT_PTR)0x1234; brt_open(&c, NULL, 4, 4, 0);
    CHECK_HR(IDWriteBitmapRenderTarget_QueryInterface(c.rt, &IID_IDWriteFactory, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    p = (void *)(UINT_PTR)0x1234;
    CHECK_HR(IDWriteBitmapRenderTarget_QueryInterface(c.rt, &brt_zero_iid, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    EXPECT_REFS(c.rt, 1);
    brt_close(&c);
}

TEST(brt_unknown_qi_rt2_rt3_not_implemented) {
    brt_ctx c; void *p = (void *)(UINT_PTR)0x1; brt_open(&c, NULL, 4, 4, 0);
    CHECK_HR(IDWriteBitmapRenderTarget_QueryInterface(c.rt, &DW_IID_IDWriteBitmapRenderTarget2, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    p = (void *)(UINT_PTR)0x1;
    CHECK_HR(IDWriteBitmapRenderTarget_QueryInterface(c.rt, &DW_IID_IDWriteBitmapRenderTarget3, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    brt_close(&c);
}

TEST(brt_unknown_qi_from_rt1_back_to_rt) {
    brt_ctx c; IDWriteBitmapRenderTarget *r = NULL; brt_open(&c, NULL, 4, 4, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    CHECK_HR_OK(IDWriteBitmapRenderTarget1_QueryInterface(c.rt1, &IID_IDWriteBitmapRenderTarget, (void **)&r));
    CHECK(r == c.rt);
    EXPECT_REFS(c.rt, 2);
    DW_RELEASE(r);
    EXPECT_REFS(c.rt, 1);
    brt_close(&c);
}

TEST(brt_unknown_rt1_addref_shares_count) {
    brt_ctx c; brt_open(&c, NULL, 4, 4, 0);
    if (!c.rt1) { brt_close(&c); t_skip("no RT1"); return; }
    EXPECT_EQ(IDWriteBitmapRenderTarget1_AddRef(c.rt1), 2);          /* one object, one count */
    EXPECT_EQ(IDWriteBitmapRenderTarget_AddRef(c.rt), 3);
    EXPECT_EQ(IDWriteBitmapRenderTarget1_Release(c.rt1), 2);
    EXPECT_EQ(IDWriteBitmapRenderTarget_Release(c.rt), 1);
    brt_close(&c);
}

TEST(brt_unknown_vtable_slots_nonnull) {
    brt_ctx c; int i, n; void **vt; brt_open(&c, NULL, 4, 4, 0);
    vt = *(void ***)c.rt;
    n = 3 + 7;                                         /* IUnknown + IDWriteBitmapRenderTarget */
    for (i = 0; i < n; i++) t_check(vt[i] != NULL, __FILE__, __LINE__, "vtable slot", "slot %d is NULL", i);
    if (c.rt1) { vt = *(void ***)c.rt1; for (i = 0; i < n + 2; i++) t_check(vt[i] != NULL, __FILE__, __LINE__, "rt1 vtable slot", "slot %d is NULL", i); }
    brt_close(&c);
}

/* ---- lifetime -------------------------------------------------------------------------------------------------- */
TEST(brt_life_release_frees_dc_and_bitmap) {
    brt_ctx c; HDC dc; HGDIOBJ bmp;
    brt_open(&c, NULL, 10, 10, 0);
    dc = c.dc; bmp = GetCurrentObject(dc, OBJ_BITMAP);
    EXPECT_EQ(GetObjectType(dc), OBJ_MEMDC); EXPECT_EQ(GetObjectType(bmp), OBJ_BITMAP);
    brt_close(&c);
    EXPECT_EQ(GetObjectType(dc), 0);                  /* destructor: DeleteDC */
    EXPECT_EQ(GetObjectType(bmp), 0);                 /* and DeleteObject on the DIB */
}

TEST(brt_life_gdi_object_count_stable) {
    DWORD before, after; int i;
    { brt_ctx w; brt_open(&w, NULL, 8, 8, 0); brt_close(&w); }          /* warm up lazily created GDI state */
    before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (i = 0; i < 50; i++) { brt_ctx c; brt_open(&c, NULL, 16 + i, 16, 0); brt_close(&c); }
    after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    t_check(after == before, __FILE__, __LINE__, "GDI objects stable", "before %lu after %lu", before, after);
}

TEST(brt_life_gdi_count_with_resize_and_draw) {
    DWORD before, after; int i; brt_ctx c;
    brt_open(&c, NULL, 40, 40, 1);
    brt_draw(&c, "A", 20.0f, 4.0f, 30.0f, 0xFFFFFF, NULL);
    before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (i = 0; i < 40; i++) {
        IDWriteBitmapRenderTarget_Resize(c.rt, 30 + (i % 7) * 5, 25 + (i % 5) * 4);
        brt_draw(&c, "AB", 18.0f, 2.0f, 20.0f, 0xFFFFFF, NULL);
    }
    after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    t_check(after == before, __FILE__, __LINE__, "GDI objects stable", "before %lu after %lu", before, after);
    brt_close(&c);
}

TEST(brt_life_release_interop_first) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL; SIZE s;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    DW_REQUIRE_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 12, 9, &rt));
    DW_RELEASE(gi);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(rt, &s));
    EXPECT_EQ(s.cx, 12); EXPECT_EQ(s.cy, 9);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(rt, 20, 20));
    CHECK(IDWriteBitmapRenderTarget_GetMemoryDC(rt) != NULL);
    DW_RELEASE(rt);
}

TEST(brt_life_release_factory_first) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL; SIZE s;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    DW_REQUIRE_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 12, 9, &rt));
    IDWriteFactory_Release(f); DW_RELEASE(gi);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(rt, &s));
    EXPECT_EQ(s.cx, 12); EXPECT_EQ(s.cy, 9);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_SetPixelsPerDip(rt, 2.0f));
    CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(rt), 2.0f);
    DW_RELEASE(rt);
}

TEST(brt_life_draw_after_factory_released) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL;
    dwx_font fh; IDWriteRenderingParams *rp = NULL; brt_run r; HDC dc; RECT bb;
    if (!f) { t_skip("no factory"); return; }
    memset(&fh, 0, sizeof fh);
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    DW_REQUIRE_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 64, 64, &rt));
    if (FAILED(dwx_font_open_default(&fh, f))) { dwx_font_close(&fh); DW_RELEASE(rt); DW_RELEASE(gi); IDWriteFactory_Release(f); t_skip("font"); return; }
    IDWriteFactory_CreateCustomRenderingParams(f, 2.2f, 0.0f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_ALIASED, &rp);
    dc = IDWriteBitmapRenderTarget_GetMemoryDC(rt);
    memset(&r, 0, sizeof r);
    r.idx[0] = (UINT16)ttf_default_gid('A'); r.adv[0] = BRT_ADV(32.0f);
    r.run.fontFace = fh.face; r.run.fontEmSize = 32.0f; r.run.glyphCount = 1; r.run.glyphIndices = r.idx; r.run.glyphAdvances = r.adv;
    /* objects handed to DrawGlyphRun hold their own references: dropping the factory must not matter */
    IDWriteFactory_Release(f);
    if (rp) {
        CHECK_HR_OK(IDWriteBitmapRenderTarget_DrawGlyphRun(rt, 10.0f, 50.0f, DWRITE_MEASURING_MODE_NATURAL, &r.run, rp, RGB(255, 255, 255), &bb));
        CHECK(GetPixel(dc, 20, 30) == RGB(255, 255, 255));
    }
    DW_RELEASE(rp);
    dwx_font_close(&fh);
    DW_RELEASE(gi); DW_RELEASE(rt);
}

TEST(brt_life_user_dc_independent_of_target) {
    HDC sdc = GetDC(NULL), mdc = CreateCompatibleDC(sdc); HBITMAP bm = CreateCompatibleBitmap(sdc, 6, 6), old; brt_ctx c;
    old = (HBITMAP)SelectObject(mdc, bm);
    SetPixelV(mdc, 1, 1, RGB(9, 9, 9));
    brt_open(&c, mdc, 16, 16, 0);
    brt_fill(&c, 0x00FFFFFF);
    IDWriteBitmapRenderTarget_Resize(c.rt, 3, 3);
    brt_close(&c);
    EXPECT_EQ(GetPixel(mdc, 1, 1), RGB(9, 9, 9));
    SelectObject(mdc, old); DeleteObject(bm); DeleteDC(mdc); ReleaseDC(NULL, sdc);
}

TEST(brt_life_two_targets_independent) {
    brt_ctx a, b; brt_open(&a, NULL, 10, 10, 0); brt_open(&b, NULL, 10, 10, 0);
    brt_fill(&a, 0x00FF00);
    EXPECT_EQ(brt_px(&a, 5, 5), 0x00FF00); EXPECT_EQ(brt_px(&b, 5, 5), 0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(a.rt, 4, 4));
    { SIZE s; IDWriteBitmapRenderTarget_GetSize(b.rt, &s); EXPECT_EQ(s.cx, 10); }
    brt_close(&a);
    EXPECT_EQ(brt_px(&b, 5, 5), 0);
    brt_close(&b);
}

TEST(brt_life_many_simultaneous_targets) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt[100]; int i, ok = 0;
    if (!f) { t_skip("no factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    for (i = 0; i < 100; i++) { rt[i] = NULL; if (SUCCEEDED(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 8 + i, 8, &rt[i])) && rt[i]) ok++; }
    EXPECT_EQ(ok, 100);
    for (i = 0; i < 100; i++) if (rt[i]) {
        SIZE s; IDWriteBitmapRenderTarget_GetSize(rt[i], &s);
        t_check(s.cx == 8 + i && s.cy == 8, __FILE__, __LINE__, "size", "target %d", i);
        IDWriteBitmapRenderTarget_Release(rt[i]);
    }
    DW_RELEASE(gi);
}

TEST(brt_life_interop_creates_from_every_factory_version) {
    int v;
    for (v = 0; v <= 7; v++) {
        IDWriteFactory *f = (IDWriteFactory *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL;
        if (!f) continue;
        if (SUCCEEDED(IDWriteFactory_GetGdiInterop(f, &gi)) && gi) {
            HRESULT hr = IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 5, 5, &rt);
            t_check(hr == S_OK && rt, __FILE__, __LINE__, "create", "factory v%d -> %s", v, dw_hr_name(hr));
            DW_RELEASE(rt); DW_RELEASE(gi);
        }
    }
}

TEST(brt_life_isolated_and_shared_factories) {
    DWRITE_FACTORY_TYPE t[2] = { DWRITE_FACTORY_TYPE_SHARED, DWRITE_FACTORY_TYPE_ISOLATED }; int i;
    for (i = 0; i < 2; i++) {
        IDWriteFactory *f = dw_new_factory(t[i]); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL; void *p = NULL;
        if (!f) continue;
        IDWriteFactory_GetGdiInterop(f, &gi);
        if (gi && SUCCEEDED(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 6, 6, &rt)) && rt) {
            CHECK_HR_OK(IDWriteBitmapRenderTarget_QueryInterface(rt, &IID_IDWriteBitmapRenderTarget1, &p));
            DW_RELEASE(p);
        }
        DW_RELEASE(rt); DW_RELEASE(gi); IDWriteFactory_Release(f);
    }
}

typedef struct { volatile LONG ok; int id; } brt_thr;
static DWORD WINAPI brt_thread_main(LPVOID arg) {
    brt_thr *t = (brt_thr *)arg; IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL; int i, good = 1;
    if (!f || FAILED(IDWriteFactory_GetGdiInterop(f, &gi))) return 1;
    for (i = 0; i < 30; i++) {
        SIZE s;
        if (FAILED(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 10 + t->id, 10 + i, &rt)) || !rt) { good = 0; break; }
        IDWriteBitmapRenderTarget_GetSize(rt, &s);
        if (s.cx != 10 + t->id || s.cy != 10 + i) good = 0;
        if (FAILED(IDWriteBitmapRenderTarget_Resize(rt, 20, 20))) good = 0;
        IDWriteBitmapRenderTarget_Release(rt); rt = NULL;
    }
    IDWriteGdiInterop_Release(gi);
    t->ok = good;
    return 0;
}
TEST(brt_life_threads_independent_targets) {
    if (t_is_wine()) { t_skip("concurrent target creation hangs in the non-Windows implementation"); return; }
    HANDLE th[8]; brt_thr info[8]; int i, good = 0;
    for (i = 0; i < 8; i++) { info[i].ok = 0; info[i].id = i; th[i] = CreateThread(NULL, 0, brt_thread_main, &info[i], 0, NULL); }
    for (i = 0; i < 8; i++) { if (th[i]) { WaitForSingleObject(th[i], 20000); CloseHandle(th[i]); } good += info[i].ok; }
    EXPECT_EQ(good, 8);
}
