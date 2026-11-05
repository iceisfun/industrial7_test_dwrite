/* IDWriteGlyphRunAnalysis::CreateAlphaTexture (aliased 1x1 textures).
 *
 * Facts (Win10 19041 dwrite.dll, DWriteGlyphRunAnalysis::CreateAlphaTexture -> GlyphRunAnalysis::CreateAlphaTexture*):
 *   textureBounds == NULL -> E_INVALIDARG. An empty rectangle (right <= left or bottom <= top) is an argument error -> E_INVALIDARG,
 *   as is a textureType other than 0/1 (the rectangle check comes first).
 *   Grayscale/aliased texture: needs bufferSize >= width*height else the OS exception ERROR_INSUFFICIENT_BUFFER -> 0x8007007A
 *   (checked before the buffer is touched, so a NULL buffer with size 0 fails the same way); bytes beyond width*height are
 *   zero-filled; the texture is row-major, tightly packed (stride = width), top row first. Pixels outside the glyph bitmaps
 *   are 0; the requested rectangle may be larger or smaller than GetAlphaTextureBounds and need not overlap it.
 *   ClearType texture: 3 bytes per pixel (R,G,B subpixel order), size 3*width*height; only valid on a ClearType analysis.
 *   Aliased rasterisation writes exactly 0 or 255.
 * Fixture geometry as in gra_bounds.c (em 512: box x 25..225, y -350..0, 200x350 = 70000 pixels).
 */
#include "gra.h"

#define PX(t, x, y) ((t).p[(size_t)(y) * (t).w + (x)])

static HRESULT ct(gra_fx *fx, IDWriteGlyphRunAnalysis **a, float ox, float oy) {
    return gra_make(fx, a, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, ox, oy);
}

TEST(gra_tex_box_all_255) {
    GRA_FIX(fx);
    int i; static const int ems[] = { 512, 1024 };
    for (i = 0; i < 2; i++) {
        gra_run(&fx, "A", (float)ems[i]);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make_al(&fx, &a, 0, 0));
        gra_tex t;
        int ok = gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t);
        CHECK(ok);
        if (ok) {
            CHECKF(t.w == GRA_X1(ems[i]) - GRA_X0(ems[i]) && t.h == GRA_HT(ems[i]), "size %dx%d", t.w, t.h);
            CHECK(t.ht == S_OK);
            CHECKF(gra_count_eq(&t, 255) == t.n, "em %d: %zu of %zu pixels are 255", ems[i], gra_count_eq(&t, 255), t.n);
            gra_tex_free(&t);
        }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_tex_values_only_0_or_255) {
    GRA_FIX(fx);
    UINT16 gids[] = { (UINT16)ttf_default_gid('A'), TTF_GID_TRIANGLE, TTF_GID_ROUND, TTF_GID_RING, TTF_GID_COMPOSITE };
    int i;
    for (i = 0; i < 5; i++) {
        gra_run_gids(&fx, &gids[i], 1, 300.0f);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make_al(&fx, &a, 0, 0));
        gra_tex t;
        if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
            CHECKF(gra_count_not01(&t) == 0, "glyph %u: %zu pixels are neither 0 nor 255", gids[i], gra_count_not01(&t));
            CHECK(gra_count_eq(&t, 0xCD) == 0);
            gra_tex_free(&t);
        } else CHECKF(0, "glyph %u: no texture (hb %s ht %s)", gids[i], dw_hr_name(t.hb), dw_hr_name(t.ht));
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_tex_buffer_size_protocol) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    size_t n = (size_t)(rc.right - rc.left) * (rc.bottom - rc.top);
    BYTE *buf = (BYTE *)malloc(n + 4096);
    static const long delta[] = { -(1L << 30), -70000, -1000, -2, -1, 0, 1, 2, 100, 4096 };
    size_t i;
    for (i = 0; i < sizeof delta / sizeof delta[0]; i++) {
        long sz = (long)n + delta[i];
        if (sz < 0) sz = 0;
        if (sz > (long)(n + 4096)) sz = (long)(n + 4096);
        memset(buf, 0xCD, n + 4096);
        HRESULT hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, buf, (UINT32)sz);
        if ((size_t)sz < n) {
            CHECKF(hr == GRA_E_NOBUF, "size %ld (need %zu) -> %s", sz, n, dw_hr_name(hr));
        } else {
            CHECKF(hr == S_OK, "size %ld (need %zu) -> %s", sz, n, dw_hr_name(hr));
            if (hr == S_OK) {
                size_t k, bad = 0;
                for (k = 0; k < n; k++) bad += buf[k] != 255;
                CHECK(bad == 0);
                /* the part of the buffer beyond width*height is cleared; nothing past bufferSize is touched */
                for (k = n; k < (size_t)sz; k++) if (buf[k] != 0) { CHECKF(0, "byte %zu of oversized buffer = 0x%02x, expected 0", k, buf[k]); break; }
                for (k = (size_t)sz; k < n + 4096; k++) if (buf[k] != 0xCD) { CHECKF(0, "byte %zu beyond bufferSize modified", k); break; }
            }
        }
    }
    free(buf);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_zero_size_null_buffer) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 64.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, NULL, 0), GRA_E_NOBUF);
    BYTE one = 0x5A;
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, &one, 0), GRA_E_NOBUF);
    CHECK(one == 0x5A);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, &one, 1), GRA_E_NOBUF);
    CHECK(one == 0x5A);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_null_bounds) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 64.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    BYTE buf[64]; memset(buf, 0xCD, sizeof buf);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, NULL, buf, sizeof buf), GRA_E_INVALIDARG);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, NULL, buf, sizeof buf), GRA_E_INVALIDARG);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, NULL, NULL, 0), GRA_E_INVALIDARG);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, (DWRITE_TEXTURE_TYPE)7, NULL, buf, sizeof buf), GRA_E_INVALIDARG);
    size_t i, bad = 0; for (i = 0; i < sizeof buf; i++) bad += buf[i] != 0xCD;
    CHECK(bad == 0);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_empty_bounds_rejected) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 64.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    static const RECT bad[] = { {0,0,0,0}, {5,5,5,5}, {0,0,10,0}, {0,0,0,10}, {10,0,0,10}, {0,10,10,0}, {10,10,0,0}, {3,-3,3,9}, {-5,-5,-5,-5}, {2,2,1,9}, {2,2,9,1} };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BYTE buf[4096]; memset(buf, 0xCD, sizeof buf);
        HRESULT hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &bad[i], buf, sizeof buf);
        CHECKF(hr == GRA_E_INVALIDARG, "rect (%ld,%ld,%ld,%ld) -> %s", bad[i].left, bad[i].top, bad[i].right, bad[i].bottom, dw_hr_name(hr));
        size_t k, mod = 0; for (k = 0; k < sizeof buf; k++) mod += buf[k] != 0xCD;
        CHECKF(mod == 0, "rect %zu: buffer modified (%zu bytes)", i, mod);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_invalid_texture_type) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 64.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    BYTE *buf = (BYTE *)malloc(4 * 64 * 64);
    static const int bad[] = { 2, 3, 255, 0x7fffffff, -1 };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        memset(buf, 0xCD, 4 * 64 * 64);
        HRESULT hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, (DWRITE_TEXTURE_TYPE)bad[i], &rc, buf, 4 * 64 * 64);
        CHECKF(hr == GRA_E_INVALIDARG, "texture type %d -> %s", bad[i], dw_hr_name(hr));
        size_t k, mod = 0; for (k = 0; k < 4 * 64 * 64; k++) mod += buf[k] != 0xCD;
        CHECKF(mod == 0, "texture type %d: buffer modified", bad[i]);
    }
    free(buf);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_cleartype_type_on_aliased_analysis_fails) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 64.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    size_t n = (size_t)(rc.right - rc.left) * (rc.bottom - rc.top) * 3;
    BYTE *buf = (BYTE *)malloc(n); memset(buf, 0xCD, n);
    /* only a ClearType analysis can produce 3x1 textures */
    HRESULT hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc, buf, (UINT32)n);
    t_note("CLEARTYPE_3x1 texture from an ALIASED analysis -> %s", dw_hr_name(hr));
    CHECK_HR_FAILED(hr);
    free(buf);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_rect_not_modified) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 128.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    RECT copy = rc;
    gra_tex t; int ok = gra_tex_rect(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, &t);
    CHECK(ok);
    CHECK_RECT_EQ(rc, copy);
    gra_tex_free(&t);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_without_prior_bounds_call) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    RECT rc = { GRA_X0(512), -GRA_HT(512), GRA_X1(512), 0 };
    gra_tex t; int ok = gra_tex_rect(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, &t);
    CHECKF(ok, "CreateAlphaTexture without a prior GetAlphaTextureBounds: %s", dw_hr_name(t.ht));
    if (ok) { CHECK(gra_count_eq(&t, 255) == t.n); gra_tex_free(&t); }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_idempotent) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 200.0f);
    UINT16 g[2] = { (UINT16)ttf_default_gid('A'), TTF_GID_ROUND };
    gra_run_gids(&fx, g, 2, 200.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex t0, t1;
    int ok0 = gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t0);
    CHECK(ok0);
    int i;
    for (i = 0; i < 5 && ok0; i++) {
        CHECK(gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t1));
        CHECK(t0.n == t1.n && !memcmp(t0.p, t1.p, t0.n));
        gra_tex_free(&t1);
    }
    if (ok0) gra_tex_free(&t0);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

/* ---- windows different from the glyph bounds ----------------------------------------------------------------------------- */
TEST(gra_tex_larger_window_zero_padded) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 128.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex tight; CHECK(gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &tight));
    static const int pads[][4] = { {1,1,1,1}, {5,0,0,0}, {0,5,0,0}, {0,0,5,0}, {0,0,0,5}, {3,7,11,13}, {32,32,32,32} };
    size_t i;
    for (i = 0; i < sizeof pads / sizeof pads[0] && tight.p; i++) {
        RECT w = { tight.rc.left - pads[i][0], tight.rc.top - pads[i][1], tight.rc.right + pads[i][2], tight.rc.bottom + pads[i][3] };
        gra_tex t;
        CHECK(gra_tex_rect(a, DWRITE_TEXTURE_ALIASED_1x1, &w, &t));
        int x, y, bad = 0;
        for (y = 0; y < t.h && t.p; y++) for (x = 0; x < t.w; x++) {
            int tx = x - pads[i][0], ty = y - pads[i][1];
            BYTE want = (tx >= 0 && ty >= 0 && tx < tight.w && ty < tight.h) ? PX(tight, tx, ty) : 0;
            bad += PX(t, x, y) != want;
        }
        CHECKF(bad == 0, "window pad (%d,%d,%d,%d): %d wrong pixels", pads[i][0], pads[i][1], pads[i][2], pads[i][3], bad);
        gra_tex_free(&t);
    }
    gra_tex_free(&tight);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_cropped_window_matches_subrect) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_RING;
    gra_run_gids(&fx, &g, 1, 256.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex full; CHECK(gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &full));
    const int crops[][4] = { {1,0,0,0}, {0,1,0,0}, {0,0,1,0}, {0,0,0,1}, {10,10,10,10}, {20,5,3,40}, {full.w / 2, 0, 0, 0}, {0, full.h / 2, 0, 0} };
    size_t i;
    for (i = 0; i < sizeof crops / sizeof crops[0] && full.p; i++) {
        RECT w = { full.rc.left + crops[i][0], full.rc.top + crops[i][1], full.rc.right - crops[i][2], full.rc.bottom - crops[i][3] };
        gra_tex t;
        CHECK(gra_tex_rect(a, DWRITE_TEXTURE_ALIASED_1x1, &w, &t));
        int x, y, bad = 0;
        for (y = 0; y < t.h && t.p; y++) for (x = 0; x < t.w; x++) bad += PX(t, x, y) != PX(full, x + crops[i][0], y + crops[i][1]);
        CHECKF(bad == 0, "crop %zu: %d wrong pixels", i, bad);
        gra_tex_free(&t);
    }
    gra_tex_free(&full);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_shifted_window_overlap) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_RING;
    gra_run_gids(&fx, &g, 1, 256.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex full; CHECK(gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &full));
    const int sh[][2] = { {10, 0}, {-10, 0}, {0, 10}, {0, -10}, {25, 40}, {-25, -40}, {full.w - 1, 0}, {0, -(full.h - 1)} };
    size_t i;
    for (i = 0; i < sizeof sh / sizeof sh[0] && full.p; i++) {
        RECT w = { full.rc.left + sh[i][0], full.rc.top + sh[i][1], full.rc.right + sh[i][0], full.rc.bottom + sh[i][1] };
        gra_tex t;
        CHECK(gra_tex_rect(a, DWRITE_TEXTURE_ALIASED_1x1, &w, &t));
        int x, y, bad = 0;
        for (y = 0; y < t.h && t.p; y++) for (x = 0; x < t.w; x++) {
            int fx_ = x + sh[i][0], fy_ = y + sh[i][1];
            BYTE want = (fx_ >= 0 && fy_ >= 0 && fx_ < full.w && fy_ < full.h) ? PX(full, fx_, fy_) : 0;
            bad += PX(t, x, y) != want;
        }
        CHECKF(bad == 0, "shift (%d,%d): %d wrong pixels", sh[i][0], sh[i][1], bad);
        gra_tex_free(&t);
    }
    gra_tex_free(&full);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_disjoint_window) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 128.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    RECT wins[] = { { rc.right + 10, rc.top, rc.right + 30, rc.bottom }, { rc.left - 30, rc.top, rc.left - 10, rc.bottom }, { rc.left, rc.bottom + 5, rc.right, rc.bottom + 25 },
                    { rc.left, rc.top - 25, rc.right, rc.top - 5 }, { 100000, 100000, 100010, 100010 }, { -100010, -100010, -100000, -100000 } };
    size_t i;
    for (i = 0; i < sizeof wins / sizeof wins[0]; i++) {
        gra_tex t;
        int ok = gra_tex_rect(a, DWRITE_TEXTURE_ALIASED_1x1, &wins[i], &t);
        t_note("disjoint window %zu -> %s", i, dw_hr_name(t.ht));
        if (ok) CHECKF(gra_count_nz(&t) == 0 && gra_count_eq(&t, 0xCD) == 0, "disjoint window %zu not fully cleared", i);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_single_pixel_windows) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    struct { int x, y; BYTE want; } c[] = {
        { GRA_X0(512), -GRA_HT(512), 255 }, { GRA_X1(512) - 1, -1, 255 }, { GRA_X0(512), -1, 255 }, { GRA_X1(512) - 1, -GRA_HT(512), 255 },
        { 100, -100, 255 }, { GRA_X0(512) - 1, -100, 0 }, { GRA_X1(512), -100, 0 }, { 100, -GRA_HT(512) - 1, 0 }, { 100, 0, 0 }, { 0, 0, 0 },
    };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        RECT w = { c[i].x, c[i].y, c[i].x + 1, c[i].y + 1 };
        BYTE b = 0xCD;
        HRESULT hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &w, &b, 1);
        CHECKF(hr == S_OK && b == c[i].want, "pixel (%d,%d): hr %s value %u want %u", c[i].x, c[i].y, dw_hr_name(hr), b, c[i].want);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

/* ---- content ----------------------------------------------------------------------------------------------------------------- */
TEST(gra_tex_ring_has_hole) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_RING;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
        /* outer 25..225 x -350..0 ; hole x 75..175, y -275..-75 */
        CHECK(t.w == 200 && t.h == 350);
        CHECKF(gra_count_eq(&t, 255) == 200u * 350u - 100u * 200u, "filled pixels %zu, want %u", gra_count_eq(&t, 255), 200 * 350 - 100 * 200);
        int x, y, bad = 0;
        for (y = 0; y < t.h; y++) for (x = 0; x < t.w; x++) {
            int px = t.rc.left + x, py = t.rc.top + y;
            int hole = px >= 75 && px < 175 && py >= -275 && py < -75;
            bad += PX(t, x, y) != (hole ? 0 : 255);
        }
        CHECKF(bad == 0, "%d pixels differ from the exact ring", bad);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_triangle_shape) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_TRIANGLE;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
        double area = 0.5 * 200 * 350;
        size_t n = gra_count_eq(&t, 255);
        CHECKF(n > area * 0.98 && n < area * 1.02, "filled %zu, geometric area %.0f", n, area);
        /* widths grow monotonically (within one pixel) from the apex row downwards; every row is centred on x = 125 */
        int y, prev = 0, bad = 0;
        for (y = 0; y < t.h; y++) {
            int x, first = -1, last = -1, cnt = 0;
            for (x = 0; x < t.w; x++) if (PX(t, x, y)) { if (first < 0) first = x; last = x; cnt++; }
            if (cnt) {
                if (cnt + 1 < prev) bad++;
                prev = cnt;
                int centre2 = 2 * t.rc.left + first + last + 1;   /* twice the centre */
                if (centre2 < 249 || centre2 > 251) bad++;
                if (cnt != last - first + 1) bad++;               /* no holes within a row */
            }
        }
        CHECKF(bad == 0, "%d irregular rows", bad);
        CHECK(t.w == 200);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_row_major_top_first) {
    /* composite glyph: box on the bottom, triangle on top: the top rows are narrow, the bottom rows full width */
    GRA_FIX(fx);
    UINT16 g = TTF_GID_COMPOSITE;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
        int x, top = 0, bottom = 0, mid_box = 0;
        for (x = 0; x < t.w; x++) { top += PX(t, x, 3) != 0; bottom += PX(t, x, t.h - 1) != 0; mid_box += PX(t, x, t.h - 175) != 0; }
        CHECKF(top < 20 && bottom == 200 && mid_box == 200, "row counts top %d bottom %d lower box %d (w=%d h=%d)", top, bottom, mid_box, t.w, t.h);
        CHECKF(t.h >= 695 && t.h <= 700, "height %d", t.h);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_two_glyphs_with_gap) {
    GRA_FIX(fx);
    gra_run(&fx, "AA", 512.0f);
    float adv[2] = { 400, 0 };
    gra_advances(&fx, adv);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
        CHECK(t.rc.left == 25 && t.rc.right == 625);
        int x, y, bad = 0;
        for (y = 0; y < t.h; y++) for (x = 0; x < t.w; x++) {
            int px = t.rc.left + x;
            int in = (px >= 25 && px < 225) || (px >= 425 && px < 625);
            bad += PX(t, x, y) != (in ? 255 : 0);
        }
        CHECKF(bad == 0, "%d pixels wrong (gap 225..425 must be empty)", bad);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_overlapping_glyphs_merge) {
    GRA_FIX(fx);
    gra_run(&fx, "AA", 512.0f);
    float adv[2] = { 100, 0 };
    gra_advances(&fx, adv);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
        CHECK(t.rc.left == 25 && t.rc.right == 325);
        CHECKF(gra_count_eq(&t, 255) == t.n, "overlapping boxes: union must be solid (%zu of %zu)", gra_count_eq(&t, 255), t.n);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_origin_translation_same_pixels) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_ROUND;
    gra_run_gids(&fx, &g, 1, 300.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    GRA_REQ(gra_make_al(&fx, &b, 17, -23));
    gra_tex ta, tb;
    int oa = gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &ta), ob = gra_tex_get(b, DWRITE_TEXTURE_ALIASED_1x1, &tb);
    CHECK(oa && ob);
    if (oa && ob) {
        CHECK(ta.w == tb.w && ta.h == tb.h && tb.rc.left == ta.rc.left + 17 && tb.rc.top == ta.rc.top - 23);
        CHECK(ta.n == tb.n && !memcmp(ta.p, tb.p, ta.n));
    }
    if (oa) gra_tex_free(&ta);
    if (ob) gra_tex_free(&tb);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

TEST(gra_tex_rounded_fractional_origin_same_pixels) {
    /* aliased origins snap to whole pixels: 0.1 and 0.4 give the same bitmap as 0 */
    GRA_FIX(fx);
    UINT16 g = TTF_GID_ROUND;
    gra_run_gids(&fx, &g, 1, 300.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    GRA_REQ(gra_make_al(&fx, &b, 0.1f, 0.4f));
    gra_tex ta, tb;
    int oa = gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &ta), ob = gra_tex_get(b, DWRITE_TEXTURE_ALIASED_1x1, &tb);
    CHECK(oa && ob);
    if (oa && ob) CHECK(ta.n == tb.n && !memcmp(ta.p, tb.p, ta.n) && !memcmp(&ta.rc, &tb.rc, sizeof ta.rc));
    if (oa) gra_tex_free(&ta);
    if (ob) gra_tex_free(&tb);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

TEST(gra_tex_all_box_letters_identical) {
    GRA_FIX(fx);
    const char *set = "AZaz09";
    gra_tex ref = {0}; int have = 0;
    size_t i;
    for (i = 0; set[i]; i++) {
        char s[2] = { set[i], 0 };
        gra_run(&fx, s, 200.0f);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make_al(&fx, &a, 0, 0));
        gra_tex t;
        if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
            if (!have) { ref = t; have = 1; }
            else { CHECK(ref.n == t.n && !memcmp(ref.p, t.p, ref.n)); gra_tex_free(&t); }
        } else CHECK(0);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    if (have) gra_tex_free(&ref);
    gra_close(&fx);
}

TEST(gra_tex_round_glyph_symmetry) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_ROUND;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
        /* the generator draws a left/right symmetric shape: mirror columns agree except for the edge pixels */
        int x, y, diff = 0, total = 0;
        for (y = 0; y < t.h; y++) for (x = 0; x < t.w / 2; x++) { total++; diff += PX(t, x, y) != PX(t, t.w - 1 - x, y); }
        t_note("round glyph %dx%d, %d of %d mirror pixels differ", t.w, t.h, diff, total);
        CHECK(diff * 100 < total);
        size_t n = gra_count_eq(&t, 255);
        CHECK(n > t.n / 2);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_transformed_box_areas) {
    GRA_FIX(fx);
    static const struct { const char *n; DWRITE_MATRIX m; int w, h; } c[] = {
        { "rot90",  {  0, 1, -1, 0, 0, 0 }, 700, 400 }, { "rot180", { -1, 0, 0, -1, 0, 0 }, 400, 700 }, { "rot270", { 0, -1, 1, 0, 0, 0 }, 700, 400 },
        { "flipx",  { -1, 0, 0, 1, 0, 0 }, 400, 700 }, { "flipy", { 1, 0, 0, -1, 0, 0 }, 400, 700 }, { "scale x2", { 2, 0, 0, 1, 0, 0 }, 800, 700 },
        { "scale 2", { 2, 0, 0, 2, 0, 0 }, 400, 700 },
    };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        gra_run(&fx, "A", c[i].m.m11 == 2 && c[i].m.m22 == 2 ? 512.0f : 1024.0f);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, &c[i].m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        gra_tex t;
        int ok = gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t);
        CHECKF(ok, "%s: no texture", c[i].n);
        if (ok) {
            CHECKF(t.w == c[i].w && t.h == c[i].h, "%s: %dx%d want %dx%d", c[i].n, t.w, t.h, c[i].w, c[i].h);
            CHECKF(gra_count_eq(&t, 255) == t.n, "%s: not solid", c[i].n);
            gra_tex_free(&t);
        }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_tex_rotated_triangle_points_right) {
    /* rotating by 90 degrees clockwise turns the apex (top of the glyph) towards +x: the widest column is the left one */
    GRA_FIX(fx);
    UINT16 g = TTF_GID_TRIANGLE;
    gra_run_gids(&fx, &g, 1, 512.0f);
    DWRITE_MATRIX m = { 0, 1, -1, 0, 0, 0 };
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
        int y, left = 0, right = 0;
        for (y = 0; y < t.h; y++) { left += PX(t, 2, y) != 0; right += PX(t, t.w - 3, y) != 0; }
        CHECKF(left > right + 100, "left column pixels %d, right column pixels %d", left, right);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_sideways_equals_rotation_pixels) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_TRIANGLE;
    gra_run_gids(&fx, &g, 1, 512.0f);
    DWRITE_MATRIX m = { 0, 1, -1, 0, 0, 0 };
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    fx.run.isSideways = TRUE;
    GRA_REQ(gra_make_al(&fx, &b, 0, 0));
    gra_tex ta, tb;
    int oa = gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &ta), ob = gra_tex_get(b, DWRITE_TEXTURE_ALIASED_1x1, &tb);
    CHECK(oa && ob);
    /* sideways turns glyphs the opposite way to this matrix (image is the 180-degree rotation), and is positioned differently */
    if (oa && ob) {
        size_t k, bad = 0;
        CHECK(ta.n == tb.n && ta.w == tb.w && ta.h == tb.h);
        if (ta.n == tb.n) for (k = 0; k < ta.n; k++) bad += ta.p[k] != tb.p[ta.n - 1 - k];
        CHECKF(bad == 0, "%zu pixels differ from the 180-degree rotation", bad);
    }
    if (oa) gra_tex_free(&ta);
    if (ob) gra_tex_free(&tb);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

TEST(gra_tex_gdi_modes_aliased_values_note) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 40.0f);
    int mode;
    for (mode = DWRITE_RENDERING_MODE_ALIASED; mode <= DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC; mode++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, (DWRITE_RENDERING_MODE)mode, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        RECT r0; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
        DWRITE_TEXTURE_TYPE tt = gra_rect_zero(&r0) ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;
        gra_tex t;
        if (gra_tex_get(a, tt, &t)) {
            t_note("%s: texture type %d, %dx%d, %zu partial bytes of %zu", gra_mode_name(mode), tt, t.w, t.h, gra_count_not01(&t), t.n);
            CHECK(gra_count_nz(&t) > 0);
            if (mode == DWRITE_RENDERING_MODE_ALIASED) CHECK(gra_count_not01(&t) == 0);
            gra_tex_free(&t);
        } else CHECKF(0, "%s: no texture (%s)", gra_mode_name(mode), dw_hr_name(t.ht));
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

/* ---- ClearType 3x1 --------------------------------------------------------------------------------------------------------------- */
TEST(gra_tex_ct_size_protocol) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 128.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(ct(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc));
    size_t n = (size_t)(rc.right - rc.left) * (rc.bottom - rc.top) * 3;
    BYTE *buf = (BYTE *)malloc(n + 100);
    static const long delta[] = { -(1L << 30), -2, -1, 0, 1, 50 };
    size_t i;
    for (i = 0; i < sizeof delta / sizeof delta[0]; i++) {
        long sz = (long)n + delta[i]; if (sz < 0) sz = 0; if (sz > (long)(n + 100)) sz = (long)(n + 100);
        memset(buf, 0xCD, n + 100);
        HRESULT hr = IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc, buf, (UINT32)sz);
        if ((size_t)sz < n) CHECKF(hr == GRA_E_NOBUF, "size %ld need %zu -> %s", sz, n, dw_hr_name(hr));
        else {
            CHECKF(hr == S_OK, "size %ld need %zu -> %s", sz, n, dw_hr_name(hr));
            size_t k; int tailbad = 0;
            for (k = n; k < (size_t)sz; k++) tailbad += buf[k] != 0;
            CHECKF(tailbad == 0, "size %ld: tail beyond 3*w*h not cleared", sz);
        }
    }
    /* a grayscale-sized buffer (w*h) is far too small for 3 bytes per pixel */
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc, buf, (UINT32)(n / 3)), GRA_E_NOBUF);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc, NULL, 0), GRA_E_NOBUF);
    free(buf);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_ct_arg_errors) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 64.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(ct(&fx, &a, 0, 0));
    RECT empty = { 3, 3, 3, 9 }; BYTE buf[256];
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &empty, buf, sizeof buf), GRA_E_INVALIDARG);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_CLEARTYPE_3x1, NULL, buf, sizeof buf), GRA_E_INVALIDARG);
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc));
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, (DWRITE_TEXTURE_TYPE)2, &rc, buf, sizeof buf), GRA_E_INVALIDARG);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_ct_interior_and_symmetry) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(ct(&fx, &a, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &t)) {
        int stride = t.w * 3;
        CHECK(t.n == (size_t)stride * t.h);
        /* the box is left/right symmetric around the middle of the padded window; the filter is symmetric */
        int x, y, asym = 0, rowdiff = 0;
        for (y = 0; y < t.h; y++) {
            for (x = 0; x < stride / 2; x++) asym += t.p[y * stride + x] != t.p[y * stride + stride - 1 - x];
            for (x = 0; x < stride; x++) rowdiff += t.p[y * stride + x] != t.p[x];
        }
        CHECKF(asym == 0, "%d asymmetric subpixel bytes", asym);
        CHECKF(rowdiff == 0, "%d bytes differ from row 0 (a box has identical rows)", rowdiff);
        /* deep inside the box every subpixel is fully covered; deep outside none */
        int mid = stride / 2;
        CHECKF(t.p[(t.h / 2) * stride + mid] >= 250, "centre byte %u", t.p[(t.h / 2) * stride + mid]);
        CHECKF(t.p[(t.h / 2) * stride + 3 * 10] >= 250, "byte 30 %u", t.p[(t.h / 2) * stride + 30]);
        t_note("first pixel bytes of row 0: %u %u %u | %u %u %u", t.p[0], t.p[1], t.p[2], t.p[3], t.p[4], t.p[5]);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_ct_filter_spreads_beyond_box) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(ct(&fx, &a, 0, 0));
    RECT tight = { GRA_X0(512), -GRA_HT(512), GRA_X1(512), 0 };
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &t)) {
        int pad = tight.left - t.rc.left;
        t_note("cleartype window pad %d pixel(s); leftmost pixel bytes %u %u %u", pad, t.p[0], t.p[1], t.p[2]);
        CHECK(pad >= 1);                                       /* the filter reaches outside the glyph box, so the window grows */
        /* the extreme outer subpixel carries the faintest, non-zero spill (or 0 when the filter is 3 taps wide) */
        int row = t.h / 2, stride = t.w * 3;
        int lastzero = -1, x;
        for (x = 0; x < stride; x++) if (t.p[row * stride + x] == 0) lastzero = x; else break;
        CHECKF(lastzero < 3 * pad, "%d leading zero subpixels, window pad is %d pixels", lastzero + 1, pad);
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_ct_coverage_sum_matches_area) {
    GRA_FIX(fx);
    static const int ems[] = { 512, 1024 };
    int i;
    for (i = 0; i < 2; i++) {
        gra_run(&fx, "A", (float)ems[i]);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(ct(&fx, &a, 0, 0));
        gra_tex t;
        if (gra_tex_get(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &t)) {
            double area = (double)(GRA_X1(ems[i]) - GRA_X0(ems[i])) * GRA_HT(ems[i]);
            double cov = gra_sum(&t) / (255.0 * 3.0);
            CHECKF(cov > area * 0.98 && cov < area * 1.02, "em %d: coverage %.1f vs area %.1f", ems[i], cov, area);
            gra_tex_free(&t);
        }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_tex_ct_larger_and_cropped_windows) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 128.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(ct(&fx, &a, 0, 0));
    gra_tex full;
    if (gra_tex_get(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &full)) {
        RECT big = { full.rc.left - 4, full.rc.top - 3, full.rc.right + 5, full.rc.bottom + 2 };
        gra_tex t;
        CHECK(gra_tex_rect(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &big, &t));
        int x, y, bad = 0;
        for (y = 0; y < t.h && t.p; y++) for (x = 0; x < t.w * 3; x++) {
            int fxx = x - 12, fyy = y - 3;
            BYTE want = (fxx >= 0 && fyy >= 0 && fxx < full.w * 3 && fyy < full.h) ? full.p[fyy * full.w * 3 + fxx] : 0;
            bad += t.p[y * t.w * 3 + x] != want;
        }
        CHECKF(bad == 0, "larger window: %d wrong bytes", bad);
        gra_tex_free(&t);
        RECT crop = { full.rc.left + 2, full.rc.top + 5, full.rc.right - 3, full.rc.bottom - 4 };
        CHECK(gra_tex_rect(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &crop, &t));
        bad = 0;
        for (y = 0; y < t.h && t.p; y++) for (x = 0; x < t.w * 3; x++) bad += t.p[y * t.w * 3 + x] != full.p[(y + 5) * full.w * 3 + x + 6];
        CHECKF(bad == 0, "cropped window: %d wrong bytes", bad);
        gra_tex_free(&t);
        gra_tex_free(&full);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_ct_idempotent_and_independent_objects) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 100.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(ct(&fx, &a, 0.3f, 0.2f));
    GRA_REQ(ct(&fx, &b, 0.3f, 0.2f));
    gra_tex t0, t1, t2;
    int o0 = gra_tex_get(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &t0), o1 = gra_tex_get(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &t1), o2 = gra_tex_get(b, DWRITE_TEXTURE_CLEARTYPE_3x1, &t2);
    CHECK(o0 && o1 && o2);
    if (o0 && o1 && o2) {
        CHECK(t0.n == t1.n && !memcmp(t0.p, t1.p, t0.n));
        CHECK(t0.n == t2.n && !memcmp(t0.p, t2.p, t0.n));
    }
    if (o0) gra_tex_free(&t0); if (o1) gra_tex_free(&t1); if (o2) gra_tex_free(&t2);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

TEST(gra_tex_ct_subpixel_origin_shifts_coverage) {
    /* moving the origin by 1/3 pixel keeps the total coverage (within filter rounding) and changes the bytes */
    GRA_FIX(fx);
    gra_run(&fx, "A", 128.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(ct(&fx, &a, 0, 0));
    GRA_REQ(ct(&fx, &b, 1.0f / 3.0f, 0));
    gra_tex ta, tb;
    int oa = gra_tex_get(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &ta), ob = gra_tex_get(b, DWRITE_TEXTURE_CLEARTYPE_3x1, &tb);
    CHECK(oa && ob);
    if (oa && ob) {
        double ca = gra_sum(&ta) / 765.0, cb = gra_sum(&tb) / 765.0;
        CHECKF(cb > ca * 0.97 && cb < ca * 1.03, "coverage %.1f vs %.1f", ca, cb);
        t_note("window %dx%d vs %dx%d, identical=%d", ta.w, ta.h, tb.w, tb.h, ta.n == tb.n && !memcmp(ta.p, tb.p, ta.n));
        CHECK(abs(ta.w - tb.w) <= 1 && ta.h == tb.h);
    }
    if (oa) gra_tex_free(&ta); if (ob) gra_tex_free(&tb);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

/* ---- grayscale anti-aliasing (IDWriteFactory2/3 overloads) ------------------------------------------------------------------- */
static HRESULT gray(gra_fx *fx, IDWriteGlyphRunAnalysis **a, DWRITE_RENDERING_MODE mode, float ox, float oy) {
    return gra_make2(fx, a, NULL, mode, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, ox, oy);
}

TEST(gra_tex_gray_uses_aliased_texture_type) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 100.0f);
    int mode;
    for (mode = DWRITE_RENDERING_MODE_ALIASED; mode <= DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC; mode++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gray(&fx, &a, (DWRITE_RENDERING_MODE)mode, 0, 0));
        RECT r0, r1;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1));
        CHECKF(!gra_rect_empty(&r0) && gra_rect_zero(&r1), "%s grayscale: type0 (%ld,%ld,%ld,%ld) type1 (%ld,%ld,%ld,%ld)", gra_mode_name(mode),
               r0.left, r0.top, r0.right, r0.bottom, r1.left, r1.top, r1.right, r1.bottom);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_tex_gray_coverage_sum_matches_area) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    static const int ems[] = { 400, 512, 1024 };
    int i;
    for (i = 0; i < 3; i++) {
        gra_run(&fx, "A", (float)ems[i]);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gray(&fx, &a, DWRITE_RENDERING_MODE_NATURAL, 0, 0));
        gra_tex t;
        if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
            double area = ems[i] * 800.0 / 2048 * (ems[i] * 1400.0 / 2048);
            double cov = gra_sum(&t) / 255.0;
            CHECKF(cov > area * 0.98 && cov < area * 1.02, "em %d: coverage %.1f vs exact area %.1f", ems[i], cov, area);
            gra_tex_free(&t);
        } else CHECK(0);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_tex_gray_small_sizes_partial_coverage) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 20.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gray(&fx, &a, DWRITE_RENDERING_MODE_NATURAL, 0, 0));
    gra_tex t;
    if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
        double area = 20 * 800.0 / 2048 * (20 * 1400.0 / 2048);
        double cov = gra_sum(&t) / 255.0;
        t_note("em 20 grayscale %dx%d coverage %.2f vs area %.2f, partial bytes %zu", t.w, t.h, cov, area, gra_count_not01(&t));
        CHECK(cov > area * 0.85 && cov < area * 1.15);
        CHECK(gra_count_not01(&t) > 0);                      /* a 7.8 x 13.7 px box has fractional edge pixels */
        gra_tex_free(&t);
    }
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_gray_origin_half_pixel_keeps_sum) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 100.0f);
    static const float o[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    double ref = 0;
    size_t i;
    for (i = 0; i < sizeof o / sizeof o[0]; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gray(&fx, &a, DWRITE_RENDERING_MODE_NATURAL, o[i], o[i]));
        gra_tex t;
        if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) {
            double cov = gra_sum(&t) / 255.0;
            if (i == 0) ref = cov;
            CHECKF(cov > ref * 0.97 && cov < ref * 1.03, "origin %g: coverage %.1f vs %.1f at origin 0", o[i], cov, ref);
            gra_tex_free(&t);
        } else CHECK(0);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_tex_gray_size_protocol) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 50.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gray(&fx, &a, DWRITE_RENDERING_MODE_NATURAL, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    size_t n = (size_t)(rc.right - rc.left) * (rc.bottom - rc.top);
    BYTE *buf = (BYTE *)malloc(n + 64);
    memset(buf, 0xCD, n + 64);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, buf, (UINT32)n - 1), GRA_E_NOBUF);
    CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, NULL, 0), GRA_E_NOBUF);
    CHECK_HR_OK(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, buf, (UINT32)n + 64));
    size_t k, bad = 0; for (k = n; k < n + 64; k++) bad += buf[k] != 0;
    CHECK(bad == 0);
    free(buf);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_tex_gray_vs_aliased_coverage_relation) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    if (t_is_wine()) { t_skip("an aliased texture after a grayscale one of the same run faults in the emulator"); gra_close(&fx); return; }
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(gray(&fx, &a, DWRITE_RENDERING_MODE_NATURAL, 0, 0));
    GRA_REQ(gra_make_al(&fx, &b, 0, 0));
    gra_tex ta, tb;
    int oa = gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &ta), ob = gra_tex_get(b, DWRITE_TEXTURE_ALIASED_1x1, &tb);
    CHECK(oa && ob);
    if (oa && ob) {
        double ca = gra_sum(&ta) / 255.0, cb = (double)gra_count_eq(&tb, 255);
        CHECKF(ca > cb * 0.97 && ca < cb * 1.03, "grayscale coverage %g vs aliased pixel count %g", ca, cb);
    }
    if (oa) gra_tex_free(&ta); if (ob) gra_tex_free(&tb);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

/* ---- many threads, separate objects ---------------------------------------------------------------------------------------------- */
typedef struct { HANDLE th; gra_fx *fx; int id; int ok; unsigned long long sum; } thr_t;
static DWORD WINAPI thr_main(LPVOID p) {
    thr_t *t = (thr_t *)p;
    DWRITE_GLYPH_RUN run = t->fx->run;
    IDWriteGlyphRunAnalysis *a = NULL;
    int i; t->ok = 1;
    for (i = 0; i < 20; i++) {
        if (FAILED(IDWriteFactory_CreateGlyphRunAnalysis(t->fx->f, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a)) || !a) { t->ok = 0; return 0; }
        gra_tex x;
        if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &x)) { unsigned long long s = gra_sum(&x); if (i == 0) t->sum = s; else if (s != t->sum) t->ok = 0; gra_tex_free(&x); }
        else t->ok = 0;
        IDWriteGlyphRunAnalysis_Release(a); a = NULL;
    }
    return 0;
}
TEST(gra_tex_threads_separate_objects) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_ROUND;
    gra_run_gids(&fx, &g, 1, 120.0f);
    thr_t th[4]; int i;
    for (i = 0; i < 4; i++) { th[i].fx = &fx; th[i].id = i; th[i].ok = 0; th[i].sum = 0; th[i].th = CreateThread(NULL, 0, thr_main, &th[i], 0, NULL); }
    for (i = 0; i < 4; i++) {
        if (th[i].th) { DWORD w = WaitForSingleObject(th[i].th, 20000); CHECKF(w == WAIT_OBJECT_0, "thread %d did not finish", i); CloseHandle(th[i].th); }
    }
    for (i = 0; i < 4; i++) CHECKF(th[i].ok, "thread %d saw inconsistent results", i);
    for (i = 1; i < 4; i++) CHECK(th[i].sum == th[0].sum);
    gra_close(&fx);
}
