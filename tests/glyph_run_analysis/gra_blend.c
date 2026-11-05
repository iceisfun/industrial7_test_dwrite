/* IDWriteGlyphRunAnalysis::GetAlphaBlendParams, texture-kind matrix over (rendering mode x antialias mode x grid fit),
 * font simulations and system-font relations.
 *
 * Facts (Win10 19041 dwrite.dll):
 *   GetAlphaBlendParams(params, &gamma, &contrast, &level): params == NULL -> E_INVALIDARG with the outputs untouched; otherwise S_OK and
 *   all three outputs written. For the plain NATURAL / NATURAL_SYMMETRIC ClearType analysis the outputs derive from the supplied
 *   parameters: gamma = params' gamma, contrast = params' enhanced contrast (plus a 0.5 boost for some faces), level = params'
 *   ClearType level. A grayscale analysis reports ClearType level 0. GDI-compatible modes substitute GDI-compatible numbers.
 *   The texture kind (ALIASED_1x1 / CLEARTYPE_3x1) of an analysis follows rendering mode and antialias mode: grayscale antialiasing and
 *   ALIASED rendering give ALIASED_1x1; the legacy CreateGlyphRunAnalysis (no antialias argument) is ClearType.
 */
#include "gra.h"

static const float SENT = 123456.0f;

static HRESULT crp(IDWriteFactory *f, float gamma, float contrast, float level, IDWriteRenderingParams **p) {
    *p = NULL;
    return IDWriteFactory_CreateCustomRenderingParams(f, gamma, contrast, level, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, p);
}

TEST(gra_blend_null_params) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    float g = SENT, c = SENT, l = SENT;
    CHECK_HR(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(a, NULL, &g, &c, &l), GRA_E_INVALIDARG);
    CHECKF(g == SENT && c == SENT && l == SENT, "outputs were written on failure: %g %g %g", g, c, l);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_blend_default_params_natural) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    IDWriteRenderingParams *p = NULL;
    GRA_REQ(IDWriteFactory_CreateRenderingParams(fx.f, &p));
    float g = SENT, c = SENT, l = SENT;
    CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(a, p, &g, &c, &l));
    t_note("default params: gamma %g contrast %g level %g (params: %g %g %g)", g, c, l, IDWriteRenderingParams_GetGamma(p), IDWriteRenderingParams_GetEnhancedContrast(p), IDWriteRenderingParams_GetClearTypeLevel(p));
    CHECK(g > 0.0f && g <= 10.0f);
    CHECK(c >= 0.0f && c <= 10.0f);
    CHECK(l >= 0.0f && l <= 1.0f);
    CHECK(g == IDWriteRenderingParams_GetGamma(p));
    CHECK(l == IDWriteRenderingParams_GetClearTypeLevel(p));
    float ec = IDWriteRenderingParams_GetEnhancedContrast(p);
    CHECKF(c == ec || c == ec + 0.5f, "contrast %g vs params %g", c, ec);
    EXPECT_REFS(p, 1);
    IDWriteRenderingParams_Release(p);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_blend_custom_params_natural_table) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    static const float gam[] = { 1.0f, 1.8f, 2.2f, 3.0f, 0.5f }, con[] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f }, lev[] = { 0.0f, 0.25f, 0.5f, 1.0f, 0.75f };
    static const DWRITE_RENDERING_MODE modes[] = { DWRITE_RENDERING_MODE_NATURAL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC };
    size_t i, m;
    for (m = 0; m < 2; m++) for (i = 0; i < 5; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, modes[m], DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        IDWriteRenderingParams *p;
        GRA_REQ(crp(fx.f, gam[i], con[i], lev[i], &p));
        float g = SENT, c = SENT, l = SENT;
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(a, p, &g, &c, &l));
        CHECKF(g == gam[i], "%s: gamma %g want %g", gra_mode_name(modes[m]), g, gam[i]);
        CHECKF(c == con[i] || c == con[i] + 0.5f, "%s: contrast %g want %g (+0.5)", gra_mode_name(modes[m]), c, con[i]);
        CHECKF(l == lev[i], "%s: ClearType level %g want %g", gra_mode_name(modes[m]), l, lev[i]);
        IDWriteRenderingParams_Release(p);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_blend_params_object_unchanged_and_reusable) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    IDWriteRenderingParams *p;
    GRA_REQ(crp(fx.f, 1.7f, 0.3f, 0.6f, &p));
    int i;
    for (i = 0; i < 10; i++) {
        float g = SENT, c = SENT, l = SENT;
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(a, p, &g, &c, &l));
        CHECK(g == 1.7f && l == 0.6f);
    }
    CHECK(IDWriteRenderingParams_GetGamma(p) == 1.7f && IDWriteRenderingParams_GetEnhancedContrast(p) == 0.3f && IDWriteRenderingParams_GetClearTypeLevel(p) == 0.6f);
    EXPECT_REFS(p, 1);
    IDWriteRenderingParams_Release(p);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_blend_grayscale_level_zero) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make2(&fx, &a, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0));
    IDWriteRenderingParams *p;
    GRA_REQ(crp(fx.f, 2.0f, 0.4f, 0.8f, &p));
    float g = SENT, c = SENT, l = SENT;
    CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(a, p, &g, &c, &l));
    t_note("grayscale NATURAL: gamma %g contrast %g level %g", g, c, l);
    CHECKF(l == 0.0f, "grayscale analysis reports ClearType level %g", l);
    CHECKF(g == 2.0f, "gamma %g", g);
    IDWriteRenderingParams_Release(p);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_blend_grayscale_contrast_params3) {
    GRA_FIX(fx); GRA_NEED_F(fx, 3);
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    gra_run(&fx, "A", 24.0f);
    IDWriteRenderingParams3 *p = NULL;
    GRA_REQ(IDWriteFactory3_CreateCustomRenderingParams(f3, 2.0f, 0.3f, 0.9f, 0.5f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, &p));
    IDWriteGlyphRunAnalysis *g_a = NULL, *c_a = NULL;
    GRA_REQ(gra_make3(&fx, &g_a, NULL, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0));
    GRA_REQ(gra_make3(&fx, &c_a, NULL, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0));
    float g = SENT, c = SENT, l = SENT;
    CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(c_a, (IDWriteRenderingParams *)p, &g, &c, &l));
    t_note("ClearType analysis, params3 (gamma 2, contrast 0.3, grayscale contrast 0.9, level 0.5): %g %g %g", g, c, l);
    CHECK(g == 2.0f && l == 0.5f);
    CHECK(c == 0.3f || c == 0.8f);
    g = c = l = SENT;
    CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(g_a, (IDWriteRenderingParams *)p, &g, &c, &l));
    t_note("grayscale analysis, same params3: %g %g %g", g, c, l);
    CHECK(g == 2.0f && l == 0.0f);
    CHECKF(c == 0.9f || c == 1.4f || c == 0.3f || c == 0.8f, "contrast %g is neither the ClearType nor the grayscale enhanced contrast (+0.5)", c);
    IDWriteRenderingParams3_Release(p);
    IDWriteGlyphRunAnalysis_Release(g_a); IDWriteGlyphRunAnalysis_Release(c_a);
    gra_close(&fx);
}

TEST(gra_blend_all_modes_sane) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    IDWriteRenderingParams *p;
    GRA_REQ(crp(fx.f, 1.8f, 0.5f, 0.5f, &p));
    int mode, aa;
    for (mode = DWRITE_RENDERING_MODE_ALIASED; mode <= DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC; mode++) for (aa = 0; aa < 2; aa++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        HRESULT hr = aa ? gra_make2(&fx, &a, NULL, (DWRITE_RENDERING_MODE)mode, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0)
                         : gra_make(&fx, &a, 1.0f, NULL, (DWRITE_RENDERING_MODE)mode, DWRITE_MEASURING_MODE_NATURAL, 0, 0);
        if (hr == GRA_UNSUPPORTED) continue;
        CHECK_HR_OK(hr);
        if (!a) continue;
        float g = SENT, c = SENT, l = SENT;
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(a, p, &g, &c, &l));
        t_note("%s aa=%d: gamma %g contrast %g level %g", gra_mode_name(mode), aa, g, c, l);
        /* GDI-compatible modes substitute their own gamma, which may be any non-negative number */
        float gmin = (mode == DWRITE_RENDERING_MODE_GDI_CLASSIC || mode == DWRITE_RENDERING_MODE_GDI_NATURAL) ? 0.0f : 1e-6f;
        CHECKF(g >= gmin && g <= 10.0f && c >= 0.0f && c <= 10.0f && l >= 0.0f && l <= 1.0f, "%s aa=%d: values out of range", gra_mode_name(mode), aa);
        CHECKF(g != SENT && c != SENT && l != SENT, "%s aa=%d: an output was not written", gra_mode_name(mode), aa);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    IDWriteRenderingParams_Release(p);
    gra_close(&fx);
}

TEST(gra_blend_params_from_other_factory) {
    GRA_FIX(fx);
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!iso) { t_skip("isolated factory unavailable"); gra_close(&fx); return; }
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    IDWriteRenderingParams *p;
    GRA_REQ(crp(iso, 1.3f, 0.2f, 0.4f, &p));
    float g = SENT, c = SENT, l = SENT;
    CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(a, p, &g, &c, &l));
    CHECK(g == 1.3f && l == 0.4f);
    IDWriteRenderingParams_Release(p);
    IDWriteGlyphRunAnalysis_Release(a);
    IDWriteFactory_Release(iso);
    gra_close(&fx);
}

TEST(gra_blend_independent_of_glyph_content) {
    GRA_FIX(fx);
    IDWriteRenderingParams *p;
    GRA_REQ(crp(fx.f, 1.9f, 0.45f, 0.35f, &p));
    const char *runs[] = { "A", " ", "ABC", "AAAAAAAA" };
    size_t i;
    float g0 = 0, c0 = 0, l0 = 0;
    for (i = 0; i < 4; i++) {
        gra_run(&fx, runs[i], 30.0f);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        float g = SENT, c = SENT, l = SENT;
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(a, p, &g, &c, &l));
        if (i == 0) { g0 = g; c0 = c; l0 = l; }
        CHECKF(g == g0 && c == c0 && l == l0, "run \"%s\": %g %g %g differs from %g %g %g", runs[i], g, c, l, g0, c0, l0);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    IDWriteRenderingParams_Release(p);
    gra_close(&fx);
}

/* ---- texture kind matrix ------------------------------------------------------------------------------------------------------ */
TEST(gra_matrix_f2_texture_kind) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 64.0f);
    int mode, aa, mm, gf;
    for (mode = DWRITE_RENDERING_MODE_ALIASED; mode <= DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC; mode++) for (aa = 0; aa < 2; aa++) for (mm = 0; mm < 3; mm++) for (gf = 0; gf < 3; gf++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        HRESULT hr = gra_make2(&fx, &a, NULL, (DWRITE_RENDERING_MODE)mode, (DWRITE_MEASURING_MODE)mm, (DWRITE_GRID_FIT_MODE)gf, (DWRITE_TEXT_ANTIALIAS_MODE)aa, 0, 0);
        CHECKF(hr == S_OK, "%s aa %d mm %d gf %d -> %s", gra_mode_name(mode), aa, mm, gf, dw_hr_name(hr));
        if (hr != S_OK || !a) continue;
        RECT r0, r1;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1));
        int e0 = gra_rect_empty(&r0), e1 = gra_rect_empty(&r1);
        CHECKF(e0 != e1, "%s aa %d mm %d gf %d: type0 empty=%d type1 empty=%d (exactly one texture kind must carry the bounds)", gra_mode_name(mode), aa, mm, gf, e0, e1);
        CHECKF((e0 ? gra_rect_zero(&r0) : gra_rect_zero(&r1)), "%s aa %d: the unmatched texture type must report an all-zero rect", gra_mode_name(mode), aa);
        if (aa == 1 || mode == DWRITE_RENDERING_MODE_ALIASED) CHECKF(!e0, "%s aa %d: expected the ALIASED_1x1 kind", gra_mode_name(mode), aa);
        if (aa == 0 && (mode == DWRITE_RENDERING_MODE_NATURAL || mode == DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC)) CHECKF(!e1, "%s ClearType: expected CLEARTYPE_3x1", gra_mode_name(mode));
        if (aa == 0 && (mode == DWRITE_RENDERING_MODE_GDI_CLASSIC || mode == DWRITE_RENDERING_MODE_GDI_NATURAL)) t_note("%s ClearType mm %d gf %d: %s", gra_mode_name(mode), mm, gf, e0 ? "3x1" : "1x1");
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_matrix_f3_texture_kind) {
    GRA_FIX(fx); GRA_NEED_F(fx, 3);
    gra_run(&fx, "A", 64.0f);
    int mode, aa;
    for (mode = 1; mode <= 7; mode++) {
        if (mode == 6) continue;
        for (aa = 0; aa < 2; aa++) {
            IDWriteGlyphRunAnalysis *a = NULL;
            HRESULT hr = gra_make3(&fx, &a, NULL, (DWRITE_RENDERING_MODE1)mode, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, (DWRITE_TEXT_ANTIALIAS_MODE)aa, 0, 0);
            CHECKF(hr == S_OK, "%s aa %d -> %s", gra_mode_name(mode), aa, dw_hr_name(hr));
            if (hr != S_OK || !a) continue;
            RECT r0, r1;
            CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
            CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1));
            CHECKF(gra_rect_empty(&r0) != gra_rect_empty(&r1), "%s aa %d: exactly one texture kind expected", gra_mode_name(mode), aa);
            if (aa == 1 || mode == 1) CHECK(!gra_rect_empty(&r0));
            t_note("f3 %s aa %d -> %s", gra_mode_name(mode), aa, gra_rect_empty(&r0) ? "3x1" : "1x1");
            IDWriteGlyphRunAnalysis_Release(a);
        }
    }
    gra_close(&fx);
}

TEST(gra_matrix_gridfit_modes_integer_box) {
    /* The synthetic font has no hinting instructions and integer box edges at em 512: grid fitting cannot move them. */
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 512.0f);
    int gf;
    for (gf = 0; gf < 3; gf++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make2(&fx, &a, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, (DWRITE_GRID_FIT_MODE)gf, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        CHECKF(rc.left == GRA_X0(512) && rc.right == GRA_X1(512) && rc.top == -GRA_HT(512) && rc.bottom == 0, "gridfit %d: (%ld,%ld,%ld,%ld)", gf, rc.left, rc.top, rc.right, rc.bottom);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_matrix_measuring_modes_with_explicit_advances) {
    /* With explicit advances the measuring mode does not change glyph placement of an aliased run. */
    GRA_FIX(fx);
    gra_run(&fx, "AB", 512.0f);
    float adv[2] = { 300, 0 };
    gra_advances(&fx, adv);
    RECT first = {0};
    int mm;
    for (mm = 0; mm < 3; mm++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, (DWRITE_MEASURING_MODE)mm, 0, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        if (mm == 0) first = rc; else CHECK_RECT_EQ(rc, first);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    CHECK_RECT(first, GRA_X0(512), -GRA_HT(512), GRA_X1(512) + 300, 0);
    gra_close(&fx);
}

TEST(gra_matrix_size_class_bounds_monotonic_cleartype) {
    GRA_FIX(fx);
    int em;
    long prev = 0;
    for (em = 8; em <= 200; em += 12) {
        gra_run(&fx, "A", (float)em);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc));
        CHECKF(!gra_rect_empty(&rc), "em %d: empty ClearType bounds", em);
        long h = rc.bottom - rc.top;
        CHECKF(h >= (long)(em * 1400.0 / 2048) - 1 && h <= (long)(em * 1400.0 / 2048) + 2, "em %d: height %ld", em, h);
        CHECK(h >= prev);
        prev = h;
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

/* ---- simulations --------------------------------------------------------------------------------------------------------------- */
static int sim_bounds(IDWriteFactory *f, DWRITE_FONT_SIMULATIONS sim, float em, RECT *out) {
    size_t sz; uint8_t *blob = ttf_build_default(&sz);
    dwx_font h;
    int ok = 0;
    memset(&h, 0, sizeof h);
    if (blob && SUCCEEDED(dwx_font_open(&h, f, blob, sz, 0, sim)) && h.face) {
        UINT16 g = (UINT16)ttf_default_gid('A');
        DWRITE_GLYPH_RUN run; memset(&run, 0, sizeof run);
        run.fontFace = h.face; run.fontEmSize = em; run.glyphCount = 1; run.glyphIndices = &g;
        IDWriteGlyphRunAnalysis *a = NULL;
        if (SUCCEEDED(IDWriteFactory_CreateGlyphRunAnalysis(f, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a)) && a) {
            ok = SUCCEEDED(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, out));
            IDWriteGlyphRunAnalysis_Release(a);
        }
    }
    dwx_font_close(&h);
    free(blob);
    return ok;
}

TEST(gra_sim_bold_widens_bounds) {
    IDWriteFactory *f = dw_shared();
    RECT r0, r1;
    if (!sim_bounds(f, DWRITE_FONT_SIMULATIONS_NONE, 512.0f, &r0) || !sim_bounds(f, DWRITE_FONT_SIMULATIONS_BOLD, 512.0f, &r1)) { t_skip("font unavailable"); return; }
    t_note("regular (%ld,%ld,%ld,%ld) bold (%ld,%ld,%ld,%ld)", r0.left, r0.top, r0.right, r0.bottom, r1.left, r1.top, r1.right, r1.bottom);
    CHECK_RECT(r0, GRA_X0(512), -GRA_HT(512), GRA_X1(512), 0);
    CHECK(r1.right - r1.left > r0.right - r0.left);
    CHECK(r1.left <= r0.left && r1.right >= r0.right && r1.top <= r0.top && r1.bottom >= r0.bottom);
}

TEST(gra_sim_oblique_shears_bounds) {
    IDWriteFactory *f = dw_shared();
    RECT r0, r1;
    if (!sim_bounds(f, DWRITE_FONT_SIMULATIONS_NONE, 512.0f, &r0) || !sim_bounds(f, DWRITE_FONT_SIMULATIONS_OBLIQUE, 512.0f, &r1)) { t_skip("font unavailable"); return; }
    t_note("oblique (%ld,%ld,%ld,%ld)", r1.left, r1.top, r1.right, r1.bottom);
    CHECK(r1.right > r0.right);                          /* the top of the glyph leans to the right */
    CHECK(r1.top == r0.top && r1.bottom == r0.bottom);
    CHECK(r1.left >= r0.left - 1);
}

TEST(gra_sim_bold_oblique_combines) {
    IDWriteFactory *f = dw_shared();
    RECT r0, rb, ro, rbo;
    if (!sim_bounds(f, 0, 512.0f, &r0) || !sim_bounds(f, DWRITE_FONT_SIMULATIONS_BOLD, 512.0f, &rb) || !sim_bounds(f, DWRITE_FONT_SIMULATIONS_OBLIQUE, 512.0f, &ro) ||
        !sim_bounds(f, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE, 512.0f, &rbo)) { t_skip("font unavailable"); return; }
    CHECK(rbo.right >= rb.right && rbo.right >= ro.right);
    CHECK(rbo.right - rbo.left > ro.right - ro.left - 1);
}

/* ---- installed fonts: relations only -------------------------------------------------------------------------------------------- */
static int sys_bounds(IDWriteFactory *f, const WCHAR *family, const WCHAR *text, float em, DWRITE_RENDERING_MODE mode, float ppd, RECT *out, DWRITE_TEXTURE_TYPE *kind) {
    IDWriteFontFace *face = dw_sysface(f, family, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) return 0;
    UINT32 cp[16]; UINT16 gi[16]; UINT32 n = 0;
    while (text[n] && n < 16) { cp[n] = text[n]; n++; }
    IDWriteFontFace_GetGlyphIndices(face, cp, n, gi);
    DWRITE_GLYPH_RUN run; memset(&run, 0, sizeof run);
    run.fontFace = face; run.fontEmSize = em; run.glyphCount = n; run.glyphIndices = gi;
    IDWriteGlyphRunAnalysis *a = NULL;
    int ok = 0;
    if (SUCCEEDED(IDWriteFactory_CreateGlyphRunAnalysis(f, &run, ppd, NULL, mode, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a)) && a) {
        RECT r0, r1;
        gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0);
        gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1);
        if (!gra_rect_zero(&r0)) { *out = r0; *kind = DWRITE_TEXTURE_ALIASED_1x1; ok = 1; }
        else { *out = r1; *kind = DWRITE_TEXTURE_CLEARTYPE_3x1; ok = 1; }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    IDWriteFontFace_Release(face);
    return ok;
}

TEST(gra_sys_arial_bounds_relations) {
    IDWriteFactory *f = dw_shared();
    RECT r; DWRITE_TEXTURE_TYPE k;
    if (!sys_bounds(f, L"Arial", L"H", 20.0f, DWRITE_RENDERING_MODE_ALIASED, 1.0f, &r, &k)) { t_skip("Arial not installed"); return; }
    t_note("Arial 'H' 20px aliased: (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(k == DWRITE_TEXTURE_ALIASED_1x1);
    long w = r.right - r.left, h = r.bottom - r.top;
    /* cap height 1467/2048 em -> 14.3 px; stem to stem about 0.57 em */
    CHECK(h >= 13 && h <= 16);
    CHECK(w >= 9 && w <= 13);
    CHECK(r.top < 0 && r.bottom <= 1 && r.left >= 0);
}

TEST(gra_sys_arial_cap_height_exact_at_2048) {
    IDWriteFactory *f = dw_shared();
    IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Arial not installed"); return; }
    IDWriteFontFace_Release(face);
    RECT r; DWRITE_TEXTURE_TYPE k;
    if (!sys_bounds(f, L"Arial", L"H", 2048.0f, DWRITE_RENDERING_MODE_ALIASED, 1.0f, &r, &k)) { t_skip("Arial run failed"); return; }
    /* At em = units-per-em the pixel box equals the glyph's design box: top is -yMax of 'H' (cap height 1466 in Arial). */
    t_note("Arial 'H' at 2048px: (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(r.bottom == 0 && r.top >= -1480 && r.top <= -1450);
    CHECK(r.left > 100 && r.left < 250 && r.right > 1250 && r.right < 1600);
}

TEST(gra_sys_arial_wide_vs_narrow) {
    IDWriteFactory *f = dw_shared();
    RECT rw, ri; DWRITE_TEXTURE_TYPE k;
    if (!sys_bounds(f, L"Arial", L"W", 40.0f, DWRITE_RENDERING_MODE_ALIASED, 1.0f, &rw, &k) || !sys_bounds(f, L"Arial", L"i", 40.0f, DWRITE_RENDERING_MODE_ALIASED, 1.0f, &ri, &k)) { t_skip("Arial not installed"); return; }
    CHECK(rw.right - rw.left > 2 * (ri.right - ri.left));
    CHECK(ri.bottom - ri.top >= 25 && rw.bottom - rw.top >= 25);
}

TEST(gra_sys_arial_space_empty) {
    IDWriteFactory *f = dw_shared();
    RECT r; DWRITE_TEXTURE_TYPE k;
    if (!sys_bounds(f, L"Arial", L" ", 30.0f, DWRITE_RENDERING_MODE_ALIASED, 1.0f, &r, &k)) { t_skip("Arial not installed"); return; }
    CHECK_RECT_ZERO(r);
}

TEST(gra_sys_arial_ppd_doubles_size) {
    IDWriteFactory *f = dw_shared();
    RECT r1, r2; DWRITE_TEXTURE_TYPE k;
    if (!sys_bounds(f, L"Arial", L"H", 100.0f, DWRITE_RENDERING_MODE_ALIASED, 1.0f, &r1, &k) || !sys_bounds(f, L"Arial", L"H", 100.0f, DWRITE_RENDERING_MODE_ALIASED, 2.0f, &r2, &k)) { t_skip("Arial not installed"); return; }
    long h1 = r1.bottom - r1.top, h2 = r2.bottom - r2.top, w1 = r1.right - r1.left, w2 = r2.right - r2.left;
    CHECKF(h2 >= 2 * h1 - 2 && h2 <= 2 * h1 + 2 && w2 >= 2 * w1 - 2 && w2 <= 2 * w1 + 2, "ppd 2: %ldx%ld vs ppd 1: %ldx%ld", w2, h2, w1, h1);
}

TEST(gra_sys_arial_cleartype_vs_aliased_note) {
    IDWriteFactory *f = dw_shared();
    RECT ra, rc; DWRITE_TEXTURE_TYPE ka, kc;
    if (!sys_bounds(f, L"Arial", L"Hamburg", 16.0f, DWRITE_RENDERING_MODE_ALIASED, 1.0f, &ra, &ka) || !sys_bounds(f, L"Arial", L"Hamburg", 16.0f, DWRITE_RENDERING_MODE_NATURAL, 1.0f, &rc, &kc)) { t_skip("Arial not installed"); return; }
    t_note("Arial 'Hamburg' 16px: aliased (%ld,%ld,%ld,%ld) natural/%s (%ld,%ld,%ld,%ld)", ra.left, ra.top, ra.right, ra.bottom, kc == DWRITE_TEXTURE_CLEARTYPE_3x1 ? "ClearType" : "gray", rc.left, rc.top, rc.right, rc.bottom);
    CHECK(kc == DWRITE_TEXTURE_CLEARTYPE_3x1);
    CHECK(rc.left <= ra.left + 1 && rc.right >= ra.right - 1);
    CHECK(rc.right - rc.left <= (ra.right - ra.left) + 6);
}

TEST(gra_sys_fonts_texture_roundtrip_note) {
    IDWriteFactory *f = dw_shared();
    static const WCHAR *fam[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Consolas", L"Verdana" };
    size_t i;
    int tried = 0;
    for (i = 0; i < sizeof fam / sizeof fam[0]; i++) {
        IDWriteFontFace *face = dw_sysface(f, fam[i], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!face) continue;
        UINT32 cp[3] = { 'T', 'g', '8' }; UINT16 gi[3];
        IDWriteFontFace_GetGlyphIndices(face, cp, 3, gi);
        DWRITE_GLYPH_RUN run; memset(&run, 0, sizeof run);
        run.fontFace = face; run.fontEmSize = 18.0f; run.glyphCount = 3; run.glyphIndices = gi;
        IDWriteGlyphRunAnalysis *a = NULL;
        int mode;
        for (mode = DWRITE_RENDERING_MODE_ALIASED; mode <= DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC; mode++) {
            if (FAILED(IDWriteFactory_CreateGlyphRunAnalysis(f, &run, 1.0f, NULL, (DWRITE_RENDERING_MODE)mode, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a)) || !a) { CHECK(0); continue; }
            RECT r0; gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0);
            DWRITE_TEXTURE_TYPE tt = gra_rect_zero(&r0) ? DWRITE_TEXTURE_CLEARTYPE_3x1 : DWRITE_TEXTURE_ALIASED_1x1;
            gra_tex t;
            int ok = gra_tex_get(a, tt, &t);
            CHECKF(ok, "%ls mode %s: texture failed (%s)", fam[i], gra_mode_name(mode), dw_hr_name(t.ht));
            if (ok) {
                t_note("%ls %s: %dx%d type %d, nonzero %zu", fam[i], gra_mode_name(mode), t.w, t.h, tt, gra_count_nz(&t));
                CHECK(gra_count_nz(&t) > 0);
                CHECK(gra_count_eq(&t, 0xCD) == 0);
                gra_tex_free(&t);
            }
            IDWriteGlyphRunAnalysis_Release(a); a = NULL;
        }
        IDWriteFontFace_Release(face);
        tried++;
    }
    if (!tried) t_skip("none of the sample system fonts is installed");
}
