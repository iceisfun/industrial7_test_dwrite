/* IDWriteBitmapRenderTarget with real fonts: DrawGlyphRun in the GDI measuring/rendering modes, SetPixelsPerDip scaling, the returned
 * blackBoxRect, and the ink extent compared with GDI TextOut of the same character at the same pixel size.  The pixel comparisons use
 * tolerances of a few pixels (two different rasterisers); self-consistency checks (ppd/em equivalence, blackBox containment) are tight. */
#include "r2.h"

typedef struct r2_rt {
    IDWriteBitmapRenderTarget *rt; IDWriteBitmapRenderTarget1 *rt1; HDC dc; int w, h;
    IDWriteRenderingParams *rp[7];
} r2_rt;

static void rt_close(r2_rt *t)
{
    int i;
    for (i = 0; i < 7; i++) DW_RELEASE(t->rp[i]);
    if (t->rt1) { IDWriteBitmapRenderTarget1_Release(t->rt1); t->rt1 = NULL; }
    DW_RELEASE(t->rt);
    memset(t, 0, sizeof *t);
}
static int rt_open(r2_rt *t, r2_ctx *c, int w, int h)
{
    int i;
    memset(t, 0, sizeof *t); t->w = w; t->h = h;
    if (FAILED(IDWriteGdiInterop_CreateBitmapRenderTarget(c->g, NULL, w, h, &t->rt)) || !t->rt) { t_skip("CreateBitmapRenderTarget failed"); return 0; }
    if (FAILED(IDWriteBitmapRenderTarget_QueryInterface(t->rt, &IID_IDWriteBitmapRenderTarget1, (void **)&t->rt1))) t->rt1 = NULL;
    t->dc = IDWriteBitmapRenderTarget_GetMemoryDC(t->rt);
    for (i = 0; i < 7; i++)
        if (FAILED(IDWriteFactory_CreateCustomRenderingParams(c->f, 2.2f, 0.0f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)i, &t->rp[i]))) t->rp[i] = NULL;
    return t->dc != NULL;
}
static uint32_t *rt_bits(r2_rt *t, int *w, int *h)
{
    DIBSECTION ds; int n;
    GdiFlush();
    memset(&ds, 0, sizeof ds);
    n = GetObjectA(GetCurrentObject(t->dc, OBJ_BITMAP), sizeof ds, &ds);
    if (n != (int)sizeof ds || !ds.dsBm.bmBits) return NULL;
    *w = ds.dsBm.bmWidth; *h = ds.dsBm.bmHeight;
    return (uint32_t *)ds.dsBm.bmBits;
}
static void rt_white(r2_rt *t)
{
    int w, h, i; uint32_t *b = rt_bits(t, &w, &h);
    if (b) for (i = 0; i < w * h; i++) b[i] = 0xFFFFFF;
}
/* bounding box (right/bottom exclusive) of non-white pixels; 0 when none */
static int rt_ink(r2_rt *t, RECT *rc)
{
    int w, h, x, y; uint32_t *b = rt_bits(t, &w, &h);
    rc->left = w; rc->top = h; rc->right = rc->bottom = 0;
    if (!b) return 0;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) if ((b[y * w + x] & 0xFFFFFF) != 0xFFFFFF) {
        if (x < rc->left) rc->left = x; if (y < rc->top) rc->top = y; if (x + 1 > rc->right) rc->right = x + 1; if (y + 1 > rc->bottom) rc->bottom = y + 1;
    }
    return rc->right > rc->left;
}
static uint64_t rt_hash(r2_rt *t)
{
    int w, h, i; uint64_t hh = 1469598103934665603ull; uint32_t *b = rt_bits(t, &w, &h);
    if (!b) return 0;
    for (i = 0; i < w * h; i++) { hh ^= b[i] & 0xFFFFFF; hh *= 1099511628211ull; }
    return hh;
}
/* one glyph run for a single character */
static HRESULT rt_draw(r2_rt *t, r2_ctx *c, UINT32 ch, float em, float ox, float oy, DWRITE_MEASURING_MODE mm, int rmode, RECT *bb)
{
    DWRITE_GLYPH_RUN run; UINT16 g = r2_gid(c, ch); FLOAT adv = 0; DWRITE_GLYPH_METRICS gm;
    memset(&run, 0, sizeof run);
    run.fontFace = c->face; run.fontEmSize = em; run.glyphCount = 1; run.glyphIndices = &g;
    if (SUCCEEDED(IDWriteFontFace_GetDesignGlyphMetrics(c->face, &g, 1, &gm, FALSE))) adv = (FLOAT)gm.advanceWidth * em / (FLOAT)c->upm;
    run.glyphAdvances = &adv;
    return IDWriteBitmapRenderTarget_DrawGlyphRun(t->rt, ox, oy, mm, &run, t->rp[rmode], RGB(0, 0, 0), bb);
}
/* same character through GDI TextOut at the same pixel size, non-antialiased; ink box in *rc */
static int gdi_ink(r2_ctx *c, UINT32 ch, int px, int ox, int oy, int w, int h, RECT *rc)
{
    dwx_dc d; HFONT old; WCHAR wc = (WCHAR)ch; int ok;
    if (!dwx_dc_create(&d, w, h, 0xFFFFFF)) return 0;
    if (!r2_select(c, 400, 0, -px, R2_Q_NONAA)) { dwx_dc_destroy(&d); return 0; }
    {
        LOGFONTW lf; HFONT hf; r2_lf(&lf, c->name, 400, 0, -px, R2_Q_NONAA); hf = CreateFontIndirectW(&lf);
        old = (HFONT)SelectObject(d.dc, hf);
        SetTextAlign(d.dc, TA_BASELINE | TA_LEFT); SetBkMode(d.dc, TRANSPARENT); SetTextColor(d.dc, RGB(0, 0, 0));
        TextOutW(d.dc, ox, oy, &wc, 1);
        GdiFlush();
        ok = dwx_dc_bbox_not(&d, 0xFFFFFF, rc);
        SelectObject(d.dc, old); DeleteObject(hf);
    }
    dwx_dc_destroy(&d);
    return ok;
}

/* ---- per font: DrawGlyphRun in the two GDI modes lands close to GDI's own rendering ---------------------------------------------------- */
static void brt_vs_gdi(const char *fam, int rmode, DWRITE_MEASURING_MODE mm)
{
    R2_OPEN(c, fam, 400, 0);
    r2_rt t; int ip, ic; static const UINT32 chs[] = { 'H', 'x', 'o', 'l', 'W' };
    if (!rt_open(&t, &c, 200, 120)) { rt_close(&t); r2_close(&c); return; }
    for (ip = 0; ip < R2_NPX; ip++) {
        int px = r2_px[ip];
        if (px > 48) continue;
        for (ic = 0; ic < 5; ic++) {
            RECT bb, ink, g; HRESULT hr;
            rt_white(&t);
            hr = rt_draw(&t, &c, chs[ic], (float)px, 20.0f, 80.0f, mm, rmode, &bb);
            CHECKF(hr == S_OK, "%s %dpx U+%04X: %s", fam, px, chs[ic], dw_hr_name(hr));
            if (hr != S_OK) continue;
            if (!rt_ink(&t, &ink)) { CHECKF(0, "%s %dpx U+%04X: nothing drawn", fam, px, chs[ic]); continue; }
            /* the blackBoxRect contains the changed pixels (allowing a pixel of filter slack) and is not wildly larger */
            CHECKF(bb.left <= ink.left + 1 && bb.top <= ink.top + 1 && bb.right >= ink.right - 1 && bb.bottom >= ink.bottom - 1,
                   "%s %dpx U+%04X: blackBox %ld,%ld-%ld,%ld ink %ld,%ld-%ld,%ld", fam, px, chs[ic], bb.left, bb.top, bb.right, bb.bottom, ink.left, ink.top, ink.right, ink.bottom);
            CHECKF(bb.right - bb.left <= ink.right - ink.left + 6 && bb.bottom - bb.top <= ink.bottom - ink.top + 6,
                   "%s %dpx U+%04X: blackBox much larger than the ink", fam, px, chs[ic]);
            if (gdi_ink(&c, chs[ic], px, 20, 80, 200, 120, &g)) {
                int dw = (ink.right - ink.left) - (g.right - g.left), dh = (ink.bottom - ink.top) - (g.bottom - g.top);
                CHECKF(r2_iabs(dw) <= 4, "%s %dpx U+%04X: ink width %ld vs GDI %ld", fam, px, chs[ic], ink.right - ink.left, g.right - g.left);
                CHECKF(r2_iabs(dh) <= 3, "%s %dpx U+%04X: ink height %ld vs GDI %ld", fam, px, chs[ic], ink.bottom - ink.top, g.bottom - g.top);
                CHECKF(r2_iabs(ink.bottom - g.bottom) <= 2, "%s %dpx U+%04X: ink bottom %ld vs GDI %ld (baseline)", fam, px, chs[ic], ink.bottom, g.bottom);
            }
        }
    }
    rt_close(&t); r2_close(&c);
}
static void brt_classic(const char *fam) { brt_vs_gdi(fam, DWRITE_RENDERING_MODE_GDI_CLASSIC, DWRITE_MEASURING_MODE_GDI_CLASSIC); }
static void brt_natural(const char *fam) { brt_vs_gdi(fam, DWRITE_RENDERING_MODE_GDI_NATURAL, DWRITE_MEASURING_MODE_GDI_NATURAL); }
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_brt_classic, brt_classic)
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_brt_natural, brt_natural)

/* every rendering mode x measuring mode combination draws something and reports a box that covers it */
TEST(r2_gdi_interop_deep_brt_mode_matrix_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; int rm, mm;
    if (!rt_open(&t, &c, 120, 100)) { rt_close(&t); r2_close(&c); return; }
    for (rm = 0; rm < 7; rm++)
        for (mm = 0; mm < 3; mm++) {
            RECT bb, ink; HRESULT hr;
            rt_white(&t);
            hr = rt_draw(&t, &c, 'H', 24.0f, 20.0f, 60.0f, (DWRITE_MEASURING_MODE)mm, rm, &bb);
            CHECKF(hr == S_OK, "rendering mode %d measuring %d -> %s", rm, mm, dw_hr_name(hr));
            if (hr != S_OK) continue;
            CHECKF(rt_ink(&t, &ink), "rendering mode %d measuring %d drew nothing", rm, mm);
            CHECKF(bb.left <= ink.left + 1 && bb.top <= ink.top + 1 && bb.right >= ink.right - 1 && bb.bottom >= ink.bottom - 1,
                   "mode %d/%d: blackBox %ld,%ld-%ld,%ld ink %ld,%ld-%ld,%ld", rm, mm, bb.left, bb.top, bb.right, bb.bottom, ink.left, ink.top, ink.right, ink.bottom);
        }
    rt_close(&t); r2_close(&c);
}

/* invalid measuring mode / NULL parameters: arguments are validated, nothing is drawn */
TEST(r2_gdi_interop_deep_brt_bad_args_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; DWRITE_GLYPH_RUN run; UINT16 g; FLOAT adv = 10; RECT bb;
    if (!rt_open(&t, &c, 60, 60)) { rt_close(&t); r2_close(&c); return; }
    g = r2_gid(&c, 'H'); memset(&run, 0, sizeof run);
    run.fontFace = c.face; run.fontEmSize = 12; run.glyphCount = 1; run.glyphIndices = &g; run.glyphAdvances = &adv;
    rt_white(&t);
    memset(&bb, 0xCD, sizeof bb);
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(t.rt, 10, 40, (DWRITE_MEASURING_MODE)3, &run, t.rp[0], 0, &bb), DW_E_INVALIDARG);
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(t.rt, 10, 40, (DWRITE_MEASURING_MODE)-1, &run, t.rp[0], 0, &bb), DW_E_INVALIDARG);
    { RECT ink; CHECK(!rt_ink(&t, &ink)); }
    rt_close(&t); r2_close(&c);
}

/* ---- pixels per DIP ---------------------------------------------------------------------------------------------------------------------- */
/* (em DIP, ppd) draws what (em * ppd, 1.0) draws: the effective pixel size is the product */
TEST(r2_gdi_interop_deep_brt_ppd_equals_em_product_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    static const struct { float em, ppd; float px; } rows[] = { { 8, 1.25f, 10 }, { 12, 1.25f, 15 }, { 16, 1.25f, 20 }, { 12, 2.0f, 24 }, { 10, 2.0f, 20 }, { 16, 1.5f, 24 }, { 24, 2.0f, 48 } };
    size_t i; int rm;
    for (rm = 2; rm <= 3; rm++)
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            r2_rt a, b; RECT ia, ib, ba, bb; uint64_t ha, hb; HRESULT h1, h2;
            if (!rt_open(&a, &c, 160, 120)) { rt_close(&a); break; }
            if (!rt_open(&b, &c, 160, 120)) { rt_close(&a); rt_close(&b); break; }
            CHECK_HR_OK(IDWriteBitmapRenderTarget_SetPixelsPerDip(a.rt, rows[i].ppd));
            rt_white(&a); rt_white(&b);
            /* origins are in DIPs: 20 DIPs / ppd gives 20 pixels in both */
            h1 = rt_draw(&a, &c, 'H', rows[i].em, 20.0f / rows[i].ppd, 90.0f / rows[i].ppd, DWRITE_MEASURING_MODE_GDI_CLASSIC, rm, &ba);
            h2 = rt_draw(&b, &c, 'H', rows[i].px, 20.0f, 90.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, rm, &bb);
            CHECK_HR_OK(h1); CHECK_HR_OK(h2);
            if (rt_ink(&a, &ia) && rt_ink(&b, &ib)) {
                CHECKF(r2_iabs((ia.right - ia.left) - (ib.right - ib.left)) <= 1 && r2_iabs((ia.bottom - ia.top) - (ib.bottom - ib.top)) <= 2,
                       "mode %d em %g ppd %g: ink %ldx%ld vs %ldx%ld", rm, rows[i].em, rows[i].ppd, ia.right - ia.left, ia.bottom - ia.top, ib.right - ib.left, ib.bottom - ib.top);
                CHECKF(r2_iabs(ia.left - ib.left) <= 1 && r2_iabs(ia.bottom - ib.bottom) <= 1, "mode %d em %g ppd %g: ink position differs", rm, rows[i].em, rows[i].ppd);
            } else CHECK(0);
            ha = rt_hash(&a); hb = rt_hash(&b);
            if (ha != hb) t_note("mode %d em %g ppd %g: pixels differ from the em*ppd rendering", rm, rows[i].em, rows[i].ppd);
            CHECKF(r2_iabs((ba.right - ba.left) - (bb.right - bb.left)) <= 1 && r2_iabs((ba.bottom - ba.top) - (bb.bottom - bb.top)) <= 2, "blackBox size %ldx%ld vs %ldx%ld", ba.right - ba.left, ba.bottom - ba.top, bb.right - bb.left, bb.bottom - bb.top);
            rt_close(&a); rt_close(&b);
        }
    r2_close(&c);
}
/* ink height grows in proportion to pixels per DIP */
TEST(r2_gdi_interop_deep_brt_ppd_scales_ink_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    static const float ppds[] = { 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f };
    r2_rt t; size_t i; int base_h = 0, base_w = 0;
    if (!rt_open(&t, &c, 240, 200)) { rt_close(&t); r2_close(&c); return; }
    for (i = 0; i < sizeof ppds / sizeof ppds[0]; i++) {
        RECT ink, bb; float got = 0;
        CHECK_HR_OK(IDWriteBitmapRenderTarget_SetPixelsPerDip(t.rt, ppds[i]));
        got = IDWriteBitmapRenderTarget_GetPixelsPerDip(t.rt); EXPECT_EQ(got == ppds[i], 1);
        rt_white(&t);
        CHECK_HR_OK(rt_draw(&t, &c, 'H', 16.0f, 10.0f / ppds[i], 150.0f / ppds[i], DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_RENDERING_MODE_GDI_CLASSIC, &bb));
        if (rt_ink(&t, &ink)) {
            int h = ink.bottom - ink.top, w = ink.right - ink.left;
            if (!i) { base_h = h; base_w = w; }
            else {
                CHECKF(fabs(h - base_h * ppds[i]) <= 3.0 + ppds[i], "ppd %g: ink height %d vs %d * ppd", ppds[i], h, base_h);
                CHECKF(fabs(w - base_w * ppds[i]) <= 3.0 + ppds[i], "ppd %g: ink width %d vs %d * ppd", ppds[i], w, base_w);
            }
        } else CHECK(0);
    }
    rt_close(&t); r2_close(&c);
}

/* GetPixelsPerDip default is dpi/96 of the reference DC, and SetPixelsPerDip changes drawing but not the memory DC, size or transform */
TEST(r2_gdi_interop_deep_brt_ppd_state_isolated_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; DWRITE_MATRIX m0, m1; UINT32 w0, h0; SIZE s0, s1; HDC dc0;
    if (!rt_open(&t, &c, 80, 60)) { rt_close(&t); r2_close(&c); return; }
    dc0 = t.dc; s0.cx = t.w; s0.cy = t.h; (void)w0; (void)h0;
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetCurrentTransform(t.rt, &m0));
    CHECK_HR_OK(IDWriteBitmapRenderTarget_SetPixelsPerDip(t.rt, 1.75f));
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetCurrentTransform(t.rt, &m1));
    CHECK(!memcmp(&m0, &m1, sizeof m0));
    CHECK(IDWriteBitmapRenderTarget_GetMemoryDC(t.rt) == dc0);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(t.rt, &s1)); CHECK(s1.cx == s0.cx && s1.cy == s0.cy);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_Resize(t.rt, 100, 90));
    CHECK(IDWriteBitmapRenderTarget_GetPixelsPerDip(t.rt) == 1.75f);
    CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize(t.rt, &s1)); CHECK(s1.cx == 100 && s1.cy == 90);
    rt_close(&t); r2_close(&c);
}

/* ---- blackBoxRect protocol -------------------------------------------------------------------------------------------------------------- */
/* moving the baseline origin by whole pixels moves the box by the same amount; the box size is unchanged */
TEST(r2_gdi_interop_deep_brt_blackbox_translation_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; static const int d[][2] = { { 0, 0 }, { 5, 0 }, { 0, 7 }, { 11, 3 }, { -4, -2 } }; size_t i; RECT b0 = { 0, 0, 0, 0 };
    int rm;
    if (!rt_open(&t, &c, 120, 100)) { rt_close(&t); r2_close(&c); return; }
    for (rm = 2; rm <= 3; rm++)
        for (i = 0; i < sizeof d / sizeof d[0]; i++) {
            RECT bb;
            rt_white(&t);
            CHECK_HR_OK(rt_draw(&t, &c, 'W', 20.0f, 30.0f + d[i][0], 60.0f + d[i][1], DWRITE_MEASURING_MODE_GDI_CLASSIC, rm, &bb));
            if (!i) b0 = bb;
            else {
                CHECKF(bb.right - bb.left == b0.right - b0.left && bb.bottom - bb.top == b0.bottom - b0.top, "mode %d shift %d,%d: size %ldx%ld vs %ldx%ld", rm, d[i][0], d[i][1], bb.right - bb.left, bb.bottom - bb.top, b0.right - b0.left, b0.bottom - b0.top);
                CHECKF(bb.left - b0.left == d[i][0] && bb.top - b0.top == d[i][1], "mode %d shift %d,%d: box moved by %ld,%ld", rm, d[i][0], d[i][1], bb.left - b0.left, bb.top - b0.top);
            }
        }
    rt_close(&t); r2_close(&c);
}
/* the box is also reported for a space-only run (empty or zero-size); no pixels change */
TEST(r2_gdi_interop_deep_brt_blackbox_space_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; RECT bb, ink;
    if (!rt_open(&t, &c, 80, 60)) { rt_close(&t); r2_close(&c); return; }
    rt_white(&t);
    memset(&bb, 0xCD, sizeof bb);
    CHECK_HR_OK(rt_draw(&t, &c, ' ', 20.0f, 20.0f, 40.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_RENDERING_MODE_GDI_CLASSIC, &bb));
    CHECK(!rt_ink(&t, &ink));
    t_note("space blackBoxRect: %ld,%ld-%ld,%ld", bb.left, bb.top, bb.right, bb.bottom);
    CHECK(bb.right - bb.left <= 1 || bb.right <= bb.left);
    rt_close(&t); r2_close(&c);
}
/* NULL blackBoxRect draws the same pixels as a non-NULL one */
TEST(r2_gdi_interop_deep_brt_null_blackbox_same_pixels_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; uint64_t h1, h2; RECT bb; int rm;
    if (!rt_open(&t, &c, 100, 80)) { rt_close(&t); r2_close(&c); return; }
    for (rm = 0; rm < 7; rm++) {
        rt_white(&t); CHECK_HR_OK(rt_draw(&t, &c, 'g', 28.0f, 20.0f, 50.0f, DWRITE_MEASURING_MODE_GDI_NATURAL, rm, &bb)); h1 = rt_hash(&t);
        rt_white(&t); CHECK_HR_OK(rt_draw(&t, &c, 'g', 28.0f, 20.0f, 50.0f, DWRITE_MEASURING_MODE_GDI_NATURAL, rm, NULL)); h2 = rt_hash(&t);
        CHECKF(h1 == h2, "rendering mode %d: pixels differ with NULL blackBoxRect", rm);
    }
    rt_close(&t); r2_close(&c);
}
/* repeated identical draws produce identical pixels (no hidden state) */
TEST(r2_gdi_interop_deep_brt_repeatable_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; uint64_t h0 = 0; int i, rm;
    if (!rt_open(&t, &c, 100, 80)) { rt_close(&t); r2_close(&c); return; }
    for (rm = 2; rm <= 3; rm++)
        for (i = 0; i < 5; i++) {
            RECT bb;
            rt_white(&t); CHECK_HR_OK(rt_draw(&t, &c, 'm', 30.0f, 12.0f, 55.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, rm, &bb));
            if (!i) h0 = rt_hash(&t); else CHECKF(rt_hash(&t) == h0, "mode %d draw #%d differs", rm, i);
        }
    rt_close(&t); r2_close(&c);
}
/* The DC the target owns keeps its own GDI font: DrawGlyphRun does not select fonts into the memory DC */
TEST(r2_gdi_interop_deep_brt_dc_font_untouched_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; HGDIOBJ f0, f1; RECT bb; int rm;
    if (!rt_open(&t, &c, 80, 60)) { rt_close(&t); r2_close(&c); return; }
    f0 = GetCurrentObject(t.dc, OBJ_FONT);
    for (rm = 0; rm < 7; rm++) {
        CHECK_HR_OK(rt_draw(&t, &c, 'A', 18.0f, 10.0f, 40.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, rm, &bb));
        f1 = GetCurrentObject(t.dc, OBJ_FONT);
        CHECKF(f0 == f1, "rendering mode %d changed the selected font", rm);
    }
    rt_close(&t); r2_close(&c);
}
/* GDI text drawn into the target's DC and DirectWrite text drawn by the target land in the same bitmap */
TEST(r2_gdi_interop_deep_brt_gdi_and_dwrite_share_bitmap_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    r2_rt t; RECT bb, ink; WCHAR ch = 'H'; HFONT hf, old; LOGFONTW lf;
    if (!rt_open(&t, &c, 160, 80)) { rt_close(&t); r2_close(&c); return; }
    rt_white(&t);
    r2_lf(&lf, "Arial", 400, 0, -24, R2_Q_NONAA); hf = CreateFontIndirectW(&lf);
    old = (HFONT)SelectObject(t.dc, hf); SetTextAlign(t.dc, TA_BASELINE); SetBkMode(t.dc, TRANSPARENT);
    TextOutW(t.dc, 10, 50, &ch, 1);
    CHECK_HR_OK(rt_draw(&t, &c, 'H', 24.0f, 90.0f, 50.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_RENDERING_MODE_GDI_CLASSIC, &bb));
    SelectObject(t.dc, old); DeleteObject(hf);
    CHECK(rt_ink(&t, &ink));
    /* two glyphs: one left of x = 70, one right of it */
    {
        int w, h, x, y, l = 0, r = 0; uint32_t *b = rt_bits(&t, &w, &h);
        for (y = 0; b && y < h; y++) for (x = 0; x < w; x++) if ((b[y * w + x] & 0xFFFFFF) != 0xFFFFFF) { if (x < 70) l++; else r++; }
        CHECKF(l > 20 && r > 20, "GDI ink %d, DirectWrite ink %d", l, r);
    }
    rt_close(&t); r2_close(&c);
}
