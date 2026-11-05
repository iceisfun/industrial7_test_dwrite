/* Additional coverage: the interop reference lifetime, repeated create/size cycles, QI on rt1, and DC properties. */
#include "brt.h"

TEST(brtx_two_targets_distinct_dcs) {
    brt_ctx a, b; brt_open(&a, NULL, 8, 8, 0); brt_open(&b, NULL, 8, 8, 0);
    if (a.rt && b.rt) {
        CHECK(a.rt != b.rt);
        CHECK(a.dc != NULL && b.dc != NULL);
        CHECK(a.dc != b.dc);              /* each target owns its CreateCompatibleDC DC */
    }
    brt_close(&b); brt_close(&a);
}

TEST(brtx_interop_released_before_target) {
    brt_ctx c; IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL; HDC dc;
    brt_open(&c, NULL, 6, 6, 0);
    if (!c.rt) { brt_close(&c); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(c.f, &gi));
    if (gi) {
        CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 5, 5, &rt));
        IDWriteGdiInterop_Release(gi);        /* the target must stay usable after the interop is released */
        if (rt) {
            dc = IDWriteBitmapRenderTarget_GetMemoryDC(rt);
            CHECK(dc != NULL);
            CHECK(GetObjectType(dc) == OBJ_MEMDC);
            IDWriteBitmapRenderTarget_Release(rt);
        }
    }
    brt_close(&c);
}

TEST(brtx_size_roundtrip_many) {
    static const UINT32 sz[][2] = {{1,1},{2,3},{17,5},{64,64},{3,200},{100,1}};
    unsigned i;
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        brt_ctx c; SIZE s; brt_open(&c, NULL, (int)sz[i][0], (int)sz[i][1], 0);
        if (!c.rt) { brt_close(&c); continue; }
        CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(c.rt, &s));
        CHECK(s.cx == (LONG)sz[i][0]); CHECK(s.cy == (LONG)sz[i][1]);
        brt_close(&c);
    }
}

TEST(brtx_rt1_aa_mode_roundtrip) {
    brt_ctx c; brt_open(&c, NULL, 4, 4, 0);
    if (c.rt1) {
        CHECK(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1) == DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
        CHECK_HR_OK(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE));
        CHECK(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1) == DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        CHECK_HR_OK(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(c.rt1, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE));
        CHECK(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(c.rt1) == DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    }
    brt_close(&c);
}

TEST(brtx_pixels_per_dip_roundtrip_values) {
    static const float v[] = {0.5f, 1.0f, 1.25f, 2.0f, 4.0f};
    brt_ctx c; unsigned i; brt_open(&c, NULL, 4, 4, 0);
    if (c.rt) {
        for (i = 0; i < sizeof v / sizeof v[0]; i++) {
            CHECK_HR_OK(IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, v[i]));
            CHECK(IDWriteBitmapRenderTarget_GetPixelsPerDip(c.rt) == v[i]);
        }
        CHECK(IDWriteBitmapRenderTarget_SetPixelsPerDip(c.rt, 0.0f) == E_INVALIDARG);
        CHECK(IDWriteBitmapRenderTarget_GetPixelsPerDip(c.rt) == 4.0f);   /* failed set leaves the value */
    }
    brt_close(&c);
}

TEST(brtx_transform_roundtrip_matrix) {
    brt_ctx c; DWRITE_MATRIX m = {1.5f, 0.25f, -0.5f, 2.0f, 3.0f, -4.0f}, g;
    brt_open(&c, NULL, 4, 4, 0);
    if (c.rt) {
        CHECK_HR_OK(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, &m));
        memset(&g, 0, sizeof g);
        CHECK_HR_OK(IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g));
        CHECK(memcmp(&g, &m, sizeof g) == 0);
        CHECK_HR_OK(IDWriteBitmapRenderTarget_SetCurrentTransform(c.rt, NULL));   /* NULL resets to identity */
        CHECK_HR_OK(IDWriteBitmapRenderTarget_GetCurrentTransform(c.rt, &g));
        CHECK(g.m11 == 1.0f && g.m12 == 0.0f && g.m21 == 0.0f && g.m22 == 1.0f && g.dx == 0.0f && g.dy == 0.0f);
    }
    brt_close(&c);
}

TEST(brtx_memory_dc_has_32bpp_dib) {
    brt_ctx c; HGDIOBJ bmp; DIBSECTION ds; brt_open(&c, NULL, 9, 7, 0);
    if (c.rt) {
        bmp = GetCurrentObject(c.dc, OBJ_BITMAP);
        memset(&ds, 0, sizeof ds);
        CHECK(GetObject(bmp, sizeof ds, &ds) == (int)sizeof ds);
        CHECK(ds.dsBm.bmWidth == 9); CHECK(ds.dsBm.bmBitsPixel == 32);
        CHECK(ds.dsBm.bmHeight == 7 || ds.dsBm.bmHeight == -7);
        CHECK(ds.dsBm.bmBits != NULL);
    }
    brt_close(&c);
}
