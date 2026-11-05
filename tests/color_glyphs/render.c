/* Colour layer runs as input of CreateGlyphRunAnalysis, and composition of the layers with their palette colours.
 * Every layer run handed out by the enumerator is a complete, valid DWRITE_GLYPH_RUN (face, size, 1 glyph, advances) positioned at its own
 * baseline origin, so rasterising it with IDWriteGlyphRunAnalysis gives exactly the pixels of that glyph drawn on its own. */
#include "cg.h"

typedef struct layer { DWRITE_GLYPH_RUN run; UINT16 gl[64]; float adv[64]; DWRITE_GLYPH_OFFSET off[64]; float x, y; float c[4]; UINT16 pal; } layer;
static int get_layers(IDWriteColorGlyphRunEnumerator *e, layer *L, int max) {
    int k = 0; BOOL has;
    while (k < max && SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) {
        const DWRITE_COLOR_GLYPH_RUN *c = NULL; UINT32 i; if (FAILED(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c)) || !c) break;
        L[k].run = c->glyphRun; for (i = 0; i < c->glyphRun.glyphCount && i < 64; i++) { L[k].gl[i] = c->glyphRun.glyphIndices[i]; L[k].adv[i] = c->glyphRun.glyphAdvances ? c->glyphRun.glyphAdvances[i] : 0; if (c->glyphRun.glyphOffsets) L[k].off[i] = c->glyphRun.glyphOffsets[i]; }
        L[k].run.glyphIndices = L[k].gl; L[k].run.glyphAdvances = L[k].adv; L[k].run.glyphOffsets = c->glyphRun.glyphOffsets ? L[k].off : NULL;
        L[k].x = c->baselineOriginX; L[k].y = c->baselineOriginY; L[k].c[0] = c->runColor.r; L[k].c[1] = c->runColor.g; L[k].c[2] = c->runColor.b; L[k].c[3] = c->runColor.a; L[k].pal = c->paletteIndex;
        k++;
    }
    return k;
}
static HRESULT analysis(IDWriteFactory *f, const DWRITE_GLYPH_RUN *r, float x, float y, DWRITE_RENDERING_MODE rm, DWRITE_MEASURING_MODE mm, IDWriteGlyphRunAnalysis **a) {
    return IDWriteFactory_CreateGlyphRunAnalysis(f, r, 1.0f, NULL, rm, mm, x, y, a);
}
static int bounds_of(IDWriteFactory *f, const DWRITE_GLYPH_RUN *r, float x, float y, RECT *rc) {
    IDWriteGlyphRunAnalysis *a = NULL; HRESULT hr = analysis(f, r, x, y, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &a);
    if (FAILED(hr) || !a) return 0;
    hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_ALIASED_1x1, rc); IDWriteGlyphRunAnalysis_Release(a); return SUCCEEDED(hr);
}
static int render_cov(IDWriteFactory *f, const DWRITE_GLYPH_RUN *r, float x, float y, BYTE *cov, int W, int H) {
    IDWriteGlyphRunAnalysis *a = NULL; RECT rc; BYTE *tex; int w, h, i, j;
    if (FAILED(analysis(f, r, x, y, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, &a)) || !a) return 0;
    if (FAILED(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc))) { IDWriteGlyphRunAnalysis_Release(a); return 0; }
    w = rc.right - rc.left; h = rc.bottom - rc.top;
    if (w > 0 && h > 0) {
        tex = (BYTE *)calloc(1, (size_t)w * h);
        if (SUCCEEDED(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, &rc, tex, (UINT32)(w * h)))) {
            for (j = 0; j < h; j++) for (i = 0; i < w; i++) { int px = rc.left + i, py = rc.top + j; if (px >= 0 && px < W && py >= 0 && py < H) cov[py * W + px] = tex[j * w + i]; }
        }
        free(tex);
    }
    IDWriteGlyphRunAnalysis_Release(a); return 1;
}
#define RW 80
#define RH 100

TEST(rn_layer_runs_are_valid_analysis_input) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F, GID_a, GID_C }; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; static layer L[32]; int k, i, m;
    cg_mkrun(&r, x.face, 40.0f, g, 4, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 10, 60, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (!e) { cg_close(&x); return; }
    k = get_layers(e, L, 32); EXPECT_EQ(k, 1 + 2 + 5 + 3);
    for (m = 0; m < 3; m++) for (i = 0; i < k; i++) {
        static const DWRITE_RENDERING_MODE rm[] = { DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_NATURAL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC };
        IDWriteGlyphRunAnalysis *a = NULL; HRESULT hr = analysis(dw_shared(), &L[i].run, L[i].x, L[i].y, rm[m], DWRITE_MEASURING_MODE_NATURAL, &a);
        CHECKF(hr == S_OK && a, "layer %d rendering mode %d: %s", i, rm[m], dw_hr_name(hr)); DW_RELEASE(a);
    }
    DW_RELEASE(e); cg_close(&x);
}
TEST(rn_base_run_of_colour_only_run_draws_nothing) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_B, GID_C }; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; static layer L[32]; int k; RECT rc;
    cg_mkrun(&r, x.face, 40.0f, g, 3, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 10, 60, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (!e) { cg_close(&x); return; }
    k = get_layers(e, L, 32); CHECK(k > 1);
    /* the base run only holds the (empty) space glyph 1 in place of every colour glyph */
    if (k > 1 && bounds_of(dw_shared(), &L[0].run, L[0].x, L[0].y, &rc)) CHECKF(rc.right - rc.left <= 0 || rc.bottom - rc.top <= 0, "base run bounds %ld,%ld,%ld,%ld", rc.left, rc.top, rc.right, rc.bottom);
    DW_RELEASE(e); cg_close(&x);
}
TEST(rn_layer_bounds_equal_the_plain_glyph_bounds) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_F, GID_A, GID_C }; static cgrun r, p; IDWriteColorGlyphRunEnumerator *e = NULL; static layer L[32]; int k, i;
    cg_mkrun(&r, x.face, 48.0f, g, 3, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 5.25f, 70.5f, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (!e) { cg_close(&x); return; }
    k = get_layers(e, L, 32);
    for (i = 1; i < k; i++) {
        RECT a, b; UINT16 one = L[i].gl[0]; cg_mkrun(&p, x.face, 48.0f, &one, 1, 1, 0, FALSE, 0);
        CHECKF(bounds_of(dw_shared(), &L[i].run, L[i].x, L[i].y, &a) && bounds_of(dw_shared(), &p.run, L[i].x, L[i].y, &b) && !memcmp(&a, &b, sizeof a), "layer %d bounds differ from the plain glyph %u", i, one);
    }
    DW_RELEASE(e); cg_close(&x);
}
TEST(rn_layer_bounds_contain_the_outline_box) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_B; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; static layer L[8]; RECT rc;
    cg_mkrun(&r, x.face, 100.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 10, 80, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (!e) { cg_close(&x); return; }
    int k = get_layers(e, L, 8); EXPECT_EQ(k, 2);
    /* ring glyph: box (100,0)-(900,1400) design units, 100/2048 scale: x 4.88..43.95, height 68.36 above the baseline */
    if (k >= 1 && bounds_of(dw_shared(), &L[k-1].run, L[k-1].x, L[k-1].y, &rc)) {
        CHECKF(rc.left >= 14 && rc.left <= 16 && rc.right >= 53 && rc.right <= 55, "x range %ld..%ld", rc.left, rc.right);
        CHECKF(rc.top >= 11 && rc.top <= 13 && rc.bottom >= 79 && rc.bottom <= 81, "y range %ld..%ld", rc.top, rc.bottom);
    }
    DW_RELEASE(e); cg_close(&x);
}
TEST(rn_ring_layer_has_a_hole) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_B; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; static layer L[8]; static BYTE cov[RW * RH];
    cg_mkrun(&r, x.face, 100.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 10, 80, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (!e) { cg_close(&x); return; }
    int k = get_layers(e, L, 8); EXPECT_EQ(k, 2);
    if (k >= 1 && render_cov(dw_shared(), &L[k-1].run, L[k-1].x, L[k-1].y, cov, RW, RH)) {
        /* outer box x 14.9..53.9 y 11.6..80, hole x 24.7..44.2 y 26.3..65.4 (even-odd: ccw hole) */
        CHECKF(cov[75 * RW + 18] == 255, "ring body pixel = %u", cov[75 * RW + 18]);
        CHECKF(cov[45 * RW + 34] == 0, "hole pixel = %u", cov[45 * RW + 34]);
        CHECKF(cov[15 * RW + 40] == 255, "ring top pixel = %u", cov[15 * RW + 40]);
        CHECKF(cov[5 * RW + 5] == 0 && cov[90 * RW + 70] == 0, "outside pixels covered");
    } else CHECKF(0, "layer not rendered");
    DW_RELEASE(e); cg_close(&x);
}
TEST(rn_composite_layer_colors) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); UINT16 g = GID_B; static cgrun r; static layer L[8]; static BYTE cov[RW * RH]; int p;
    for (p = 0; p < 2; p++) {
        IDWriteColorGlyphRunEnumerator *e = NULL; int k; cg_mkrun(&r, x.face, 100.0f, &g, 1, 0, 0, FALSE, 0); memset(cov, 0, sizeof cov);
        CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 10, 80, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, p, &e));
        if (!e) continue;
        k = get_layers(e, L, 8); EXPECT_EQ(k, 2);
        if (k >= 1 && render_cov(dw_shared(), &L[k-1].run, L[k-1].x, L[k-1].y, cov, RW, RH)) {
            /* over a white background: out = white * (1 - a*cov) + colour * a*cov, per channel */
            float cv = cov[75 * RW + 18] / 255.0f, a = L[k-1].c[3] * cv, want[3], white = 1.0f; int ch;
            for (ch = 0; ch < 3; ch++) want[ch] = white * (1 - a) + L[k-1].c[ch] * a;
            uint32_t v = cg_pal[p][2]; float ex[3] = { ((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f }, ea = (v >> 24) / 255.0f;
            for (ch = 0; ch < 3; ch++) { float w2 = white * (1 - ea) + ex[ch] * ea; CHECKF(fabsf(want[ch] - w2) < 1e-4f, "palette %d channel %d: composed %g want %g", p, ch, want[ch], w2); }
        }
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(rn_translation_invariance_of_layer_origins) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); static cgrun r1, r2; static layer A[8], B[8]; UINT16 g1[] = { GID_A }, g2[] = { GID_F, GID_A }; IDWriteColorGlyphRunEnumerator *e1 = NULL, *e2 = NULL;
    /* em 204.8: every glyph advance is exactly 100 */
    cg_mkrun(&r1, x.face, 204.8f, g1, 1, 0, 0, FALSE, 0); cg_mkrun(&r2, x.face, 204.8f, g2, 2, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 10, 200, &r1.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e1));
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 10, 200, &r2.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e2));
    if (e1 && e2) {
        int k1 = get_layers(e1, A, 8), k2 = get_layers(e2, B, 8), i;
        EXPECT_EQ(k1, 3); EXPECT_EQ(k2, 3);
        for (i = 1; i < k1 && i < k2; i++) {
            RECT a, b;
            CHECKF(B[i].x - A[i].x == 100.0f, "layer %d x %g vs %g", i, B[i].x, A[i].x);
            if (bounds_of(dw_shared(), &A[i].run, A[i].x, A[i].y, &a) && bounds_of(dw_shared(), &B[i].run, B[i].x, B[i].y, &b)) CHECKF(b.left - a.left == 100 && b.right - a.right == 100 && b.top == a.top && b.bottom == a.bottom, "layer %d bounds shift", i);
        }
    }
    DW_RELEASE(e1); DW_RELEASE(e2); cg_close(&x);
}
TEST(rn_rtl_layer_bounds_follow_origin) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); static cgrun r; static layer L[8]; UINT16 g[] = { GID_F, GID_A }; IDWriteColorGlyphRunEnumerator *e = NULL;
    cg_mkrun(&r, x.face, 204.8f, g, 2, 0, 0, FALSE, 1);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 500, 200, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        int k = get_layers(e, L, 8), i; EXPECT_EQ(k, 3);
        for (i = 1; i < k; i++) CHECKF(L[i].x == 400.0f && L[i].y == 200.0f, "layer %d origin %g,%g", i, L[i].x, L[i].y);
        RECT a, b; static cgrun p; UINT16 one = L[1].gl[0]; cg_mkrun(&p, x.face, 204.8f, &one, 1, 1, 0, FALSE, 0);
        if (k > 1) CHECK(bounds_of(dw_shared(), &L[1].run, L[1].x, L[1].y, &a) && bounds_of(dw_shared(), &p.run, 400.0f, 200.0f, &b) && !memcmp(&a, &b, sizeof a));
    }
    DW_RELEASE(e); cg_close(&x);
}
TEST(rn_factory2_analysis_with_layer_runs) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); static cgrun r; static layer L[8]; UINT16 g = GID_A; IDWriteColorGlyphRunEnumerator *e = NULL; int i, gf, aa;
    cg_mkrun(&r, x.face, 30.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 4, 40, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        int k = get_layers(e, L, 8);
        for (i = 0; i < k; i++) for (gf = 0; gf <= 2; gf++) for (aa = 0; aa <= 1; aa++) {
            IDWriteGlyphRunAnalysis *a = NULL;
            HRESULT hr = IDWriteFactory2_CreateGlyphRunAnalysis(f2, &L[i].run, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, (DWRITE_GRID_FIT_MODE)gf, (DWRITE_TEXT_ANTIALIAS_MODE)aa, L[i].x, L[i].y, &a);
            CHECKF(hr == S_OK && a, "layer %d gridfit %d aa %d: %s", i, gf, aa, dw_hr_name(hr)); DW_RELEASE(a);
        }
    }
    DW_RELEASE(e); cg_close(&x);
}
TEST(rn_layer_run_survives_enumerator_release) {
    CG_NEED_F2(f2); CG_OPEN(x, CGFX_COLR); static cgrun r; static layer L[8]; UINT16 g = GID_a; IDWriteColorGlyphRunEnumerator *e = NULL;
    cg_mkrun(&r, x.face, 30.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f2, 4, 40, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 1, &e));
    if (e) {
        int k = get_layers(e, L, 8), i; DW_RELEASE(e);                       /* the copies in L[] are independent of the enumerator */
        for (i = 1; i < k; i++) { RECT rc; CHECKF(bounds_of(dw_shared(), &L[i].run, L[i].x, L[i].y, &rc) && rc.right > rc.left, "layer %d", i); }
    }
    cg_close(&x);
}
