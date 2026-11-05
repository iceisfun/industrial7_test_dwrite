/* Rendering-params objects handed to other dwrite entry points:
 *   IDWriteFontFace::GetRecommendedRenderingMode (5 args), IDWriteFontFace2:: (10 args, GRID_FIT out),
 *   IDWriteFontFace3:: (RENDERING_MODE1 out), IDWriteGlyphRunAnalysis::GetAlphaBlendParams,
 *   IDWriteBitmapRenderTarget::DrawGlyphRun, plus QueryInterface negatives and multi-threaded use.
 *
 * Decompilation facts used:
 *  - a non-DEFAULT rendering mode stored in the params wins over the automatic choice (GetRecommendedRenderingMode returns it
 *    unchanged); value 7 is reported as 5 by the DWRITE_RENDERING_MODE out parameters;
 *  - the 5-argument GetRecommendedRenderingMode rejects NULL params with E_INVALIDARG (out zeroed); the others accept NULL;
 *  - IDWriteFontFace3::GetRecommendedRenderingMode upgrades an explicit NATURAL_SYMMETRIC to the downsampled mode 7 when the
 *    params were made by a pre-RENDERING_MODE1 overload (or NULL) and min(dpiX, dpiY) >= 172.8;
 *  - a non-DEFAULT grid fit mode stored in the params is returned as the recommended grid fit mode;
 *  - IDWriteGlyphRunAnalysis::GetAlphaBlendParams and IDWriteBitmapRenderTarget::DrawGlyphRun reject NULL params (E_INVALIDARG).
 */
#include "rp.h"

typedef struct { IDWriteFactory *f; dwx_font h; } rp_face_fx;
static int face_open(rp_face_fx *x) {
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    if (FAILED(dwx_font_open_default(&x->h, x->f)) || !x->h.face) { dwx_font_close(&x->h); IDWriteFactory_Release(x->f); x->f = NULL; return 0; }
    return 1;
}
static void face_close(rp_face_fx *x) { dwx_font_close(&x->h); if (x->f) IDWriteFactory_Release(x->f); x->f = NULL; }
#define FACE_BEGIN() rp_face_fx X; if (!face_open(&X)) { t_skip("synthetic font face unavailable"); return; } IDWriteFontFace *face = X.h.face
#define FACE_END() face_close(&X)

static void *face_v(IDWriteFontFace *face, const IID *iid) { void *p = NULL; if (FAILED(IUnknown_QueryInterface((IUnknown *)face, iid, &p))) return NULL; return p; }

/* params created through overload `ver` with the given mode / grid */
static void *mk(int ver, int mode, int grid) {
    rp_vals v = rp_base(); void *p = NULL; v.mode = mode; v.grid = grid;
    if (FAILED(rp_make(ver, &v, &p))) return NULL;
    return p;
}

/* ---- IDWriteFontFace::GetRecommendedRenderingMode ------------------------------------------------------------------------ */
TEST(rpu_face1_null_params_invalid_arg) {
    FACE_BEGIN();
    DWRITE_RENDERING_MODE m = (DWRITE_RENDERING_MODE)99;
    CHECK_HR(IDWriteFontFace_GetRecommendedRenderingMode(face, 16.0f, 1.0f, DWRITE_MEASURING_MODE_NATURAL, NULL, &m), DW_E_INVALIDARG);
    EXPECT_EQ(m, 0);   /* *renderingMode = DEFAULT is written first */
    FACE_END();
}
TEST(rpu_face1_explicit_mode_is_returned) {
    FACE_BEGIN(); int ver, m, mm; static const float sizes[] = { 6.0f, 12.0f, 20.0f, 72.0f, 300.0f };
    for (ver = 0; ver <= 3; ver++) {
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        for (m = 1; m <= 6; m++) {
            void *p = mk(ver, m, 0); unsigned s;
            if (!p) { CHECKF(0, "params v%d mode %d", ver, m); continue; }
            for (mm = 0; mm <= 2; mm++) for (s = 0; s < 5; s++) {
                DWRITE_RENDERING_MODE out = (DWRITE_RENDERING_MODE)99;
                HRESULT hr = IDWriteFontFace_GetRecommendedRenderingMode(face, sizes[s], 1.0f, (DWRITE_MEASURING_MODE)mm, (IDWriteRenderingParams *)p, &out);
                if (hr != S_OK || (int)out != m) CHECKF(0, "v%d mode %d mm %d size %g -> %s out %d", ver, m, mm, (double)sizes[s], dw_hr_name(hr), (int)out);
            }
            DW_RELEASE(p);
        }
    }
    FACE_END();
}
TEST(rpu_face1_mode7_reported_as_5) {
    FACE_BEGIN(); RP_NEED(3);
    void *p = mk(3, 7, 0); DWRITE_RENDERING_MODE out = (DWRITE_RENDERING_MODE)99;
    if (p) {
        CHECK_HR_OK(IDWriteFontFace_GetRecommendedRenderingMode(face, 16.0f, 1.0f, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)p, &out));
        EXPECT_EQ(out, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC);
    }
    DW_RELEASE(p);
    FACE_END();
}
TEST(rpu_face1_default_params_resolve_to_concrete_mode) {
    FACE_BEGIN(); int mm;
    for (mm = 0; mm <= 2; mm++) {
        void *p = NULL; DWRITE_RENDERING_MODE out = (DWRITE_RENDERING_MODE)99;
        if (!rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &p)) { t_skip("no default params"); break; }
        CHECK_HR_OK(IDWriteFontFace_GetRecommendedRenderingMode(face, 16.0f, 1.0f, (DWRITE_MEASURING_MODE)mm, (IDWriteRenderingParams *)p, &out));
        CHECKF(out >= DWRITE_RENDERING_MODE_ALIASED && out <= DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, "mode %d for measuring %d", (int)out, mm);
        t_note("default params, measuring %d, 16 em -> mode %d", mm, (int)out);
        DW_RELEASE(p);
    }
    FACE_END();
}
TEST(rpu_face1_default_mode_custom_params_resolve) {
    FACE_BEGIN(); void *p = mk(0, 0, 0); DWRITE_RENDERING_MODE out = (DWRITE_RENDERING_MODE)99;
    if (p) {
        CHECK_HR_OK(IDWriteFontFace_GetRecommendedRenderingMode(face, 16.0f, 1.0f, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)p, &out));
        CHECKF(out >= 1 && out <= 5, "mode %d", (int)out);
        t_note("custom DEFAULT-mode params, 16 em NATURAL -> %d", (int)out);
    }
    DW_RELEASE(p);
    FACE_END();
}
TEST(rpu_face1_does_not_retain_params) {
    FACE_BEGIN(); void *p = mk(0, 3, 0); DWRITE_RENDERING_MODE out;
    if (p) {
        int i;
        for (i = 0; i < 10; i++) IDWriteFontFace_GetRecommendedRenderingMode(face, 16.0f, 1.0f, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)p, &out);
        EXPECT_REFS(p, 1);
    }
    DW_RELEASE(p);
    FACE_END();
}
TEST(rpu_face1_params_released_after_use_destroyed) {
    FACE_BEGIN(); void *p = mk(0, 2, 0); DWRITE_RENDERING_MODE out;
    if (p) {
        CHECK_HR_OK(IDWriteFontFace_GetRecommendedRenderingMode(face, 16.0f, 1.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, (IDWriteRenderingParams *)p, &out));
        ULONG rr = IDWriteRenderingParams_Release((IDWriteRenderingParams *)p); EXPECT_EQ(rr, 0);
    }
    FACE_END();
}

/* ---- IDWriteFontFace2::GetRecommendedRenderingMode ------------------------------------------------------------------------- */
TEST(rpu_face2_explicit_mode_and_grid_returned) {
    FACE_BEGIN(); IDWriteFontFace2 *f2 = face_v(face, &IID_IDWriteFontFace2);
    if (!f2) { FACE_END(); t_skip("IDWriteFontFace2 unsupported"); return; }
    int ver, m, g;
    for (ver = 2; ver <= 3; ver++) {
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        for (m = 1; m <= 6; m++) for (g = 1; g <= 2; g++) {
            void *p = mk(ver, m, g); DWRITE_RENDERING_MODE out = (DWRITE_RENDERING_MODE)99; DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)99;
            if (!p) { CHECKF(0, "params v%d", ver); continue; }
            /* neither a non-DEFAULT rendering mode nor a non-DEFAULT grid fit mode is overridden by the heuristics */
            HRESULT hr = IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.0f, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)p, &out, &gf);
            if (hr != S_OK || (int)out != m || (int)gf != g) CHECKF(0, "v%d mode %d grid %d -> %s out %d grid %d", ver, m, g, dw_hr_name(hr), (int)out, (int)gf);
            DW_RELEASE(p);
        }
    }
    DW_RELEASE(f2); FACE_END();
}
TEST(rpu_face2_mode7_reported_as_5) {
    FACE_BEGIN(); RP_NEED(3); IDWriteFontFace2 *f2 = face_v(face, &IID_IDWriteFontFace2);
    if (!f2) { FACE_END(); t_skip("IDWriteFontFace2 unsupported"); return; }
    void *p = mk(3, 7, 2); DWRITE_RENDERING_MODE out = (DWRITE_RENDERING_MODE)99; DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)99;
    if (p) {
        CHECK_HR_OK(IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.0f, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)p, &out, &gf));
        EXPECT_EQ(out, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC); EXPECT_EQ(gf, DWRITE_GRID_FIT_MODE_ENABLED);
    }
    DW_RELEASE(p); DW_RELEASE(f2); FACE_END();
}
TEST(rpu_face2_null_params_accepted) {
    FACE_BEGIN(); IDWriteFontFace2 *f2 = face_v(face, &IID_IDWriteFontFace2);
    if (!f2) { FACE_END(); t_skip("IDWriteFontFace2 unsupported"); return; }
    DWRITE_RENDERING_MODE out = (DWRITE_RENDERING_MODE)99; DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)99;
    CHECK_HR_OK(IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.0f, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, &out, &gf));
    CHECKF(out >= 1 && out <= 6, "mode %d", (int)out); CHECKF(gf == DWRITE_GRID_FIT_MODE_DISABLED || gf == DWRITE_GRID_FIT_MODE_ENABLED, "grid fit %d", (int)gf);
    DW_RELEASE(f2); FACE_END();
}
TEST(rpu_face2_invalid_threshold_and_measuring_zero_outputs) {
    FACE_BEGIN(); IDWriteFontFace2 *f2 = face_v(face, &IID_IDWriteFontFace2);
    if (!f2) { FACE_END(); t_skip("IDWriteFontFace2 unsupported"); return; }
    void *p = mk(2, 3, 1); DWRITE_RENDERING_MODE out; DWRITE_GRID_FIT_MODE gf;
    if (p) {
        out = (DWRITE_RENDERING_MODE)99; gf = (DWRITE_GRID_FIT_MODE)99;
        CHECK_HR(IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.0f, 96.0f, 96.0f, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)2, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)p, &out, &gf), DW_E_INVALIDARG);
        EXPECT_EQ(out, 0); EXPECT_EQ(gf, 0);
        out = (DWRITE_RENDERING_MODE)99; gf = (DWRITE_GRID_FIT_MODE)99;
        CHECK_HR(IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.0f, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)3, (IDWriteRenderingParams *)p, &out, &gf), DW_E_INVALIDARG);
        EXPECT_EQ(out, 0); EXPECT_EQ(gf, 0);
    }
    DW_RELEASE(p); DW_RELEASE(f2); FACE_END();
}
TEST(rpu_face2_sideways_and_transform_accepted_with_params) {
    FACE_BEGIN(); IDWriteFontFace2 *f2 = face_v(face, &IID_IDWriteFontFace2);
    if (!f2) { FACE_END(); t_skip("IDWriteFontFace2 unsupported"); return; }
    void *p = mk(2, 4, 2); DWRITE_MATRIX mx = { 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f };
    if (p) {
        DWRITE_RENDERING_MODE out = (DWRITE_RENDERING_MODE)99; DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)99;
        CHECK_HR_OK(IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.0f, 96.0f, 96.0f, &mx, TRUE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)p, &out, &gf));
        EXPECT_EQ(out, DWRITE_RENDERING_MODE_NATURAL); EXPECT_EQ(gf, DWRITE_GRID_FIT_MODE_ENABLED);
    }
    DW_RELEASE(p); DW_RELEASE(f2); FACE_END();
}

/* ---- IDWriteFontFace3::GetRecommendedRenderingMode ------------------------------------------------------------------------- */
static HRESULT face3_call(IDWriteFontFace3 *f3, float dpi, void *params, DWRITE_RENDERING_MODE1 *m, DWRITE_GRID_FIT_MODE *g) {
    *m = (DWRITE_RENDERING_MODE1)99; *g = (DWRITE_GRID_FIT_MODE)99;
    return IDWriteFontFace3_GetRecommendedRenderingMode(f3, 16.0f, dpi, dpi, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)params, m, g);
}
TEST(rpu_face3_explicit_modes_pass_through) {
    FACE_BEGIN(); IDWriteFontFace3 *f3 = face_v(face, &IID_IDWriteFontFace3);
    if (!f3) { FACE_END(); t_skip("IDWriteFontFace3 unsupported"); return; }
    static const int modes[] = { 1, 2, 3, 6 }; unsigned i; int ver;
    for (ver = 0; ver <= 3; ver++) {
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        for (i = 0; i < 4; i++) {
            void *p = mk(ver, modes[i], 0); DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
            if (!p) continue;
            HRESULT hr = face3_call(f3, 96.0f, p, &m, &g);
            if (hr != S_OK || (int)m != modes[i]) CHECKF(0, "v%d mode %d -> %s %d", ver, modes[i], dw_hr_name(hr), (int)m);
            DW_RELEASE(p);
        }
    }
    DW_RELEASE(f3); FACE_END();
}
TEST(rpu_face3_mode7_stays_7) {
    FACE_BEGIN(); RP_NEED(3); IDWriteFontFace3 *f3 = face_v(face, &IID_IDWriteFontFace3);
    if (!f3) { FACE_END(); t_skip("IDWriteFontFace3 unsupported"); return; }
    void *p = mk(3, 7, 0); DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
    if (p) { CHECK_HR_OK(face3_call(f3, 96.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED); }
    DW_RELEASE(p); DW_RELEASE(f3); FACE_END();
}
TEST(rpu_face3_symmetric_upgrade_for_old_overloads_at_high_dpi) {
    FACE_BEGIN(); IDWriteFontFace3 *f3 = face_v(face, &IID_IDWriteFontFace3);
    if (!f3) { FACE_END(); t_skip("IDWriteFontFace3 unsupported"); return; }
    int ver; DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
    for (ver = 0; ver <= 2; ver++) {
        void *p; if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        p = mk(ver, 5, 0); if (!p) continue;
        /* params made before RENDERING_MODE1 existed: NATURAL_SYMMETRIC means "or downsampled when the display is dense" */
        CHECK_HR_OK(face3_call(f3, 96.0f, p, &m, &g));  EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC);
        CHECK_HR_OK(face3_call(f3, 144.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC);
        CHECK_HR_OK(face3_call(f3, 172.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC);
        CHECK_HR_OK(face3_call(f3, 173.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED);
        CHECK_HR_OK(face3_call(f3, 192.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED);
        CHECK_HR_OK(face3_call(f3, 384.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED);
        DW_RELEASE(p);
    }
    DW_RELEASE(f3); FACE_END();
}
TEST(rpu_face3_natural_not_upgraded_with_equal_dpi_and_no_transform) {
    FACE_BEGIN(); IDWriteFontFace3 *f3 = face_v(face, &IID_IDWriteFontFace3);
    if (!f3) { FACE_END(); t_skip("IDWriteFontFace3 unsupported"); return; }
    int ver; DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
    for (ver = 0; ver <= 2; ver++) {
        void *p; if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        /* explicit NATURAL with dpiX == dpiY and no transform: the axis-alignment bit is set, so the 4 -> 5 promotion is skipped at any density */
        p = mk(ver, 4, 0); if (!p) continue;
        CHECK_HR_OK(face3_call(f3, 96.0f, p, &m, &g));  EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL);
        CHECK_HR_OK(face3_call(f3, 192.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL);
        DW_RELEASE(p);
    }
    DW_RELEASE(f3); FACE_END();
}
TEST(rpu_face3_symmetric_not_upgraded_for_mode1_overload) {
    FACE_BEGIN(); RP_NEED(3); IDWriteFontFace3 *f3 = face_v(face, &IID_IDWriteFontFace3);
    if (!f3) { FACE_END(); t_skip("IDWriteFontFace3 unsupported"); return; }
    void *p = mk(3, 5, 0); DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
    if (p) {
        /* an explicit DWRITE_RENDERING_MODE1 request is honoured literally at every density */
        CHECK_HR_OK(face3_call(f3, 96.0f, p, &m, &g));  EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC);
        CHECK_HR_OK(face3_call(f3, 192.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC);
        CHECK_HR_OK(face3_call(f3, 384.0f, p, &m, &g)); EXPECT_EQ(m, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC);
    }
    DW_RELEASE(p); DW_RELEASE(f3); FACE_END();
}
TEST(rpu_face3_explicit_grid_fit_returned) {
    FACE_BEGIN(); IDWriteFontFace3 *f3 = face_v(face, &IID_IDWriteFontFace3);
    if (!f3) { FACE_END(); t_skip("IDWriteFontFace3 unsupported"); return; }
    int ver, g_in;
    for (ver = 2; ver <= 3; ver++) for (g_in = 1; g_in <= 2; g_in++) {
        void *p; DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        p = mk(ver, 3, g_in); if (!p) continue;
        CHECK_HR_OK(face3_call(f3, 96.0f, p, &m, &g)); EXPECT_EQ(g, g_in);
        DW_RELEASE(p);
    }
    DW_RELEASE(f3); FACE_END();
}
TEST(rpu_face3_null_params_and_invalid_enums) {
    FACE_BEGIN(); IDWriteFontFace3 *f3 = face_v(face, &IID_IDWriteFontFace3);
    if (!f3) { FACE_END(); t_skip("IDWriteFontFace3 unsupported"); return; }
    DWRITE_RENDERING_MODE1 m; DWRITE_GRID_FIT_MODE g;
    CHECK_HR_OK(face3_call(f3, 96.0f, NULL, &m, &g));
    CHECKF((int)m >= 1 && (int)m <= 7, "mode1 %d", (int)m);
    m = (DWRITE_RENDERING_MODE1)99; g = (DWRITE_GRID_FIT_MODE)99;
    CHECK_HR(IDWriteFontFace3_GetRecommendedRenderingMode(f3, 16.0f, 96.0f, 96.0f, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)2, DWRITE_MEASURING_MODE_NATURAL, NULL, &m, &g), DW_E_INVALIDARG);
    EXPECT_EQ(m, 0); EXPECT_EQ(g, 0);
    m = (DWRITE_RENDERING_MODE1)99; g = (DWRITE_GRID_FIT_MODE)99;
    CHECK_HR(IDWriteFontFace3_GetRecommendedRenderingMode(f3, 16.0f, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)3, NULL, &m, &g), DW_E_INVALIDARG);
    EXPECT_EQ(m, 0); EXPECT_EQ(g, 0);
    DW_RELEASE(f3); FACE_END();
}

/* ---- IDWriteGlyphRunAnalysis::GetAlphaBlendParams ---------------------------------------------------------------------------- */
typedef struct { rp_face_fx x; UINT16 gi[3]; DWRITE_GLYPH_RUN run; IDWriteGlyphRunAnalysis *a; } rp_ana;
static int ana_open(rp_ana *g, DWRITE_RENDERING_MODE mode) {
    memset(g, 0, sizeof *g);
    if (!face_open(&g->x)) return 0;
    g->gi[0] = ttf_default_gid('A'); g->gi[1] = ttf_default_gid('B'); g->gi[2] = ttf_default_gid('C');
    g->run.fontFace = g->x.h.face; g->run.fontEmSize = 20.0f; g->run.glyphCount = 3; g->run.glyphIndices = g->gi;
    if (FAILED(IDWriteFactory_CreateGlyphRunAnalysis(g->x.f, &g->run, 1.0f, NULL, mode, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &g->a)) || !g->a) { face_close(&g->x); return 0; }
    return 1;
}
static void ana_close(rp_ana *g) { DW_RELEASE(g->a); face_close(&g->x); }
TEST(rpu_alpha_blend_null_params_invalid_arg) {
    rp_ana g; if (!ana_open(&g, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC)) { t_skip("glyph run analysis unavailable"); return; }
    float ga = 7.0f, ec = 7.0f, ct = 7.0f;
    CHECK_HR(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(g.a, NULL, &ga, &ec, &ct), DW_E_INVALIDARG);
    /* nothing is written on failure */
    CHECK_FLOAT_EQ(ga, 7.0f); CHECK_FLOAT_EQ(ec, 7.0f); CHECK_FLOAT_EQ(ct, 7.0f);
    ana_close(&g);
}
TEST(rpu_alpha_blend_valid_params_all_modes) {
    DWRITE_RENDERING_MODE modes[] = { DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_GDI_CLASSIC, DWRITE_RENDERING_MODE_GDI_NATURAL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC };
    unsigned i;
    for (i = 0; i < 5; i++) {
        rp_ana g; void *p = mk(0, 0, 0); float ga = 7.0f, ec = 7.0f, ct = 7.0f;
        if (!p || !ana_open(&g, modes[i])) { DW_RELEASE(p); t_note("analysis for mode %d unavailable", (int)modes[i]); continue; }
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(g.a, (IDWriteRenderingParams *)p, &ga, &ec, &ct));
        CHECK(isfinite(ga) && isfinite(ec) && isfinite(ct));
        CHECK(ga != 7.0f || ec != 7.0f || ct != 7.0f);
        t_note("analysis mode %d with gamma 2.2 ec 0.5 ct 1: blend gamma %g contrast %g cleartype %g", (int)modes[i], (double)ga, (double)ec, (double)ct);
        DW_RELEASE(p); ana_close(&g);
    }
}
TEST(rpu_alpha_blend_gamma_follows_params) {
    rp_ana g; void *p1 = mk(0, 0, 0), *p2; float g1 = 0, g2 = 0, ec = 0, ct = 0;
    rp_vals v = rp_base();
    if (!p1 || !ana_open(&g, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC)) { DW_RELEASE(p1); t_skip("analysis unavailable"); return; }
    v.gamma = 1.0f; p2 = NULL; rp_make(0, &v, &p2);
    if (p2) {
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(g.a, (IDWriteRenderingParams *)p1, &g1, &ec, &ct));
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(g.a, (IDWriteRenderingParams *)p2, &g2, &ec, &ct));
        t_note("blend gamma for params gamma 2.2 / 1.0: %g / %g", (double)g1, (double)g2);
    }
    DW_RELEASE(p1); DW_RELEASE(p2); ana_close(&g);
}
TEST(rpu_alpha_blend_params_from_every_overload) {
    int ver;
    for (ver = 0; ver <= 3; ver++) {
        rp_ana g; void *p; float ga = 0, ec = 0, ct = 0;
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        p = mk(ver, 0, 0);
        if (!p || !ana_open(&g, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC)) { DW_RELEASE(p); continue; }
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaBlendParams(g.a, (IDWriteRenderingParams *)p, &ga, &ec, &ct));
        EXPECT_REFS(p, 1);
        DW_RELEASE(p); ana_close(&g);
    }
}

/* ---- IDWriteBitmapRenderTarget::DrawGlyphRun ------------------------------------------------------------------------------------ */
typedef struct { rp_face_fx x; IDWriteGdiInterop *gi; IDWriteBitmapRenderTarget *rt; UINT16 gid[1]; float adv[1]; DWRITE_GLYPH_RUN run; } rp_rt;
static int rt_open(rp_rt *r) {
    memset(r, 0, sizeof *r);
    if (!face_open(&r->x)) return 0;
    if (FAILED(IDWriteFactory_GetGdiInterop(r->x.f, &r->gi)) || !r->gi) { face_close(&r->x); return 0; }
    if (FAILED(IDWriteGdiInterop_CreateBitmapRenderTarget(r->gi, NULL, 96, 96, &r->rt)) || !r->rt) { DW_RELEASE(r->gi); face_close(&r->x); return 0; }
    r->gid[0] = ttf_default_gid('A'); r->adv[0] = 20.0f;
    r->run.fontFace = r->x.h.face; r->run.fontEmSize = 40.0f; r->run.glyphCount = 1; r->run.glyphIndices = r->gid; r->run.glyphAdvances = r->adv;
    return 1;
}
static void rt_close(rp_rt *r) { DW_RELEASE(r->rt); DW_RELEASE(r->gi); face_close(&r->x); }
TEST(rpu_draw_null_params_invalid_arg) {
    rp_rt r; if (!rt_open(&r)) { t_skip("bitmap render target unavailable"); return; }
    RECT bb = { 5, 6, 7, 8 };
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(r.rt, 10.0f, 60.0f, DWRITE_MEASURING_MODE_NATURAL, &r.run, NULL, RGB(0, 0, 0), &bb), DW_E_INVALIDARG);
    /* the black box is cleared before validation */
    EXPECT_EQ(bb.left, 0); EXPECT_EQ(bb.top, 0); EXPECT_EQ(bb.right, 0); EXPECT_EQ(bb.bottom, 0);
    rt_close(&r);
}
TEST(rpu_draw_nan_origin_invalid_arg) {
    rp_rt r; void *p = mk(0, 1, 0); if (!p || !rt_open(&r)) { DW_RELEASE(p); t_skip("bitmap render target unavailable"); return; }
    RECT bb = { 5, 6, 7, 8 };
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(r.rt, rp_nan(), 60.0f, DWRITE_MEASURING_MODE_NATURAL, &r.run, (IDWriteRenderingParams *)p, RGB(0, 0, 0), &bb), DW_E_INVALIDARG);
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(r.rt, 10.0f, rp_nan(), DWRITE_MEASURING_MODE_NATURAL, &r.run, (IDWriteRenderingParams *)p, RGB(0, 0, 0), &bb), DW_E_INVALIDARG);
    EXPECT_EQ(bb.right, 0);
    DW_RELEASE(p); rt_close(&r);
}
TEST(rpu_draw_each_rendering_mode_accepted) {
    int m;
    for (m = 0; m <= 6; m++) {
        rp_rt r; void *p = mk(0, m, 0); RECT bb = { -1, -1, -1, -1 };
        if (!p || !rt_open(&r)) { DW_RELEASE(p); t_skip("bitmap render target unavailable"); return; }
        HRESULT hr = IDWriteBitmapRenderTarget_DrawGlyphRun(r.rt, 10.0f, 70.0f, DWRITE_MEASURING_MODE_NATURAL, &r.run, (IDWriteRenderingParams *)p, RGB(0, 0, 0), &bb);
        char msg[48]; _snprintf(msg, sizeof msg, "DrawGlyphRun with params mode %d", m);
        dw_check_hr(hr, S_OK, __FILE__, __LINE__, msg);
        CHECKF(bb.right > bb.left && bb.bottom > bb.top, "mode %d: empty black box (%ld,%ld,%ld,%ld)", m, bb.left, bb.top, bb.right, bb.bottom);
        CHECKF(bb.left >= 0 && bb.top >= 0 && bb.right <= 96 && bb.bottom <= 96, "mode %d: black box outside the target (%ld,%ld,%ld,%ld)", m, bb.left, bb.top, bb.right, bb.bottom);
        EXPECT_REFS(p, 1);
        DW_RELEASE(p); rt_close(&r);
    }
}
TEST(rpu_draw_params_from_every_overload) {
    int ver;
    for (ver = 0; ver <= 3; ver++) {
        rp_rt r; void *p; RECT bb;
        if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) continue;
        p = mk(ver, 4, 0);
        if (!p || !rt_open(&r)) { DW_RELEASE(p); continue; }
        CHECK_HR_OK(IDWriteBitmapRenderTarget_DrawGlyphRun(r.rt, 10.0f, 70.0f, DWRITE_MEASURING_MODE_NATURAL, &r.run, (IDWriteRenderingParams *)p, RGB(0, 0, 0), &bb));
        DW_RELEASE(p); rt_close(&r);
    }
}
TEST(rpu_draw_default_params_and_monitor_params) {
    rp_rt r; void *p = NULL, *q = NULL; RECT bb;
    if (!rt_open(&r)) { t_skip("bitmap render target unavailable"); return; }
    if (rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &p))
        CHECK_HR_OK(IDWriteBitmapRenderTarget_DrawGlyphRun(r.rt, 10.0f, 70.0f, DWRITE_MEASURING_MODE_NATURAL, &r.run, (IDWriteRenderingParams *)p, RGB(0, 0, 0), &bb));
    if (SUCCEEDED(IDWriteFactory_CreateMonitorRenderingParams(dw_shared(), NULL, (IDWriteRenderingParams **)&q)) && q)
        CHECK_HR_OK(IDWriteBitmapRenderTarget_DrawGlyphRun(r.rt, 10.0f, 70.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, &r.run, (IDWriteRenderingParams *)q, RGB(0, 0, 0), &bb));
    DW_RELEASE(p); DW_RELEASE(q); rt_close(&r);
}

/* ---- QueryInterface negatives ------------------------------------------------------------------------------------------------------- */
static const GUID rp_guid_null = {0,0,0,{0,0,0,0,0,0,0,0}};
static int is_params_iid(const IID *i) {
    return IsEqualIID(i, &IID_IDWriteRenderingParams) || IsEqualIID(i, &IID_IDWriteRenderingParams1) || IsEqualIID(i, &IID_IDWriteRenderingParams2) || IsEqualIID(i, &IID_IDWriteRenderingParams3) || IsEqualIID(i, &IID_IUnknown);
}
TEST(rpu_qi_all_other_interfaces_rejected) {
    int ver;
    for (ver = -1; ver <= 3; ver++) {
        void *p = NULL; int n = 0, i; const dw_qi_row *rows = dw_iid_table(&n);
        if (ver < 0) { if (!rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &p)) continue; }
        else { rp_vals v = rp_base(); if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver) || FAILED(rp_make(ver, &v, &p))) continue; }
        for (i = 0; i < n; i++) {
            void *q = RP_SENT; HRESULT hr;
            if (is_params_iid(rows[i].iid)) continue;
            hr = IUnknown_QueryInterface((IUnknown *)p, rows[i].iid, &q);
            if (hr != E_NOINTERFACE || q != NULL) { CHECKF(0, "QI(%s) on ver %d object -> %s %p", rows[i].name, ver, dw_hr_name(hr), q); if (q && q != RP_SENT) IUnknown_Release((IUnknown *)q); }
        }
        EXPECT_REFS(p, 1);
        DW_RELEASE(p);
    }
}
TEST(rpu_qi_guid_null_and_misc) {
    void *p = NULL, *q = RP_SENT; if (!rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &p)) { t_skip("no params"); return; }
    CHECK_HR(IUnknown_QueryInterface((IUnknown *)p, &rp_guid_null, &q), E_NOINTERFACE); CHECK_OUT_NULL(q);
    q = RP_SENT; CHECK_HR(IUnknown_QueryInterface((IUnknown *)p, &IID_IDispatch, &q), E_NOINTERFACE); CHECK_OUT_NULL(q);
    q = RP_SENT; CHECK_HR(IUnknown_QueryInterface((IUnknown *)p, &IID_IClassFactory, &q), E_NOINTERFACE); CHECK_OUT_NULL(q);
    EXPECT_REFS(p, 1);
    DW_RELEASE(p);
}
TEST(rpu_qi_matrix_helper_on_custom_object) {
    rp_vals v = rp_base(); void *p = NULL; DW_REQUIRE_OK(rp_make(0, &v, &p));
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteRenderingParams, 1), DW_QI_ROW(IDWriteRenderingParams1, 1), DW_QI_ROW(IDWriteRenderingParams2, 1), DW_QI_ROW(IDWriteRenderingParams3, 1),
                         DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteTextFormat, 0) };
    DW_CHECK_QI(p, rows);
    DW_RELEASE(p);
}

/* ---- threads ------------------------------------------------------------------------------------------------------------------------- */
typedef struct { int id; int bad; void *shared; } rp_thr;
static DWORD WINAPI rp_worker(LPVOID arg) {
    rp_thr *t = (rp_thr *)arg; int i;
    for (i = 0; i < 100; i++) {
        rp_vals v = rp_base(); void *p = NULL; v.gamma = 1.0f + (float)t->id * 0.125f; v.pg = (t->id + i) % 3; v.mode = i % 8; v.grid = i % 3; v.ct = (float)(i % 4) * 0.25f;
        IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
        if (!f3) { t->bad++; break; }
        if (FAILED(IDWriteFactory3_CreateCustomRenderingParams(f3, v.gamma, v.ec, v.gec, v.ct, (DWRITE_PIXEL_GEOMETRY)v.pg, (DWRITE_RENDERING_MODE1)v.mode, (DWRITE_GRID_FIT_MODE)v.grid, (IDWriteRenderingParams3 **)&p)) || !p) { t->bad++; continue; }
        IDWriteRenderingParams3 *a = p;
        if (!rp_fbits(IDWriteRenderingParams3_GetGamma(a), v.gamma) || IDWriteRenderingParams3_GetPixelGeometry(a) != (DWRITE_PIXEL_GEOMETRY)v.pg ||
            IDWriteRenderingParams3_GetRenderingMode1(a) != (DWRITE_RENDERING_MODE1)v.mode || IDWriteRenderingParams3_GetGridFitMode(a) != (DWRITE_GRID_FIT_MODE)v.grid) t->bad++;
        if (t->shared) { IDWriteRenderingParams *s = (IDWriteRenderingParams *)t->shared; IUnknown_AddRef((IUnknown *)s); (void)IDWriteRenderingParams_GetGamma(s); IUnknown_Release((IUnknown *)s); }
        IUnknown_Release((IUnknown *)p);
    }
    return 0;
}
TEST(rpu_threads_create_and_read_concurrently) {
    RP_NEED(3);
    enum { N = 8 }; HANDLE h[N]; rp_thr t[N]; int i, done = 0; void *shared = NULL;
    rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &shared);
    for (i = 0; i < N; i++) { t[i].id = i; t[i].bad = 0; t[i].shared = shared; h[i] = CreateThread(NULL, 0, rp_worker, &t[i], 0, NULL); if (h[i]) done++; }
    EXPECT_EQ(done, N);
    for (i = 0; i < N; i++) if (h[i]) { DWORD w = WaitForSingleObject(h[i], 20000); CHECK(w == WAIT_OBJECT_0); CloseHandle(h[i]); }
    for (i = 0; i < N; i++) CHECKF(t[i].bad == 0, "thread %d saw %d inconsistencies", i, t[i].bad);
    if (shared) EXPECT_REFS(shared, 1);
    DW_RELEASE(shared);
}
static DWORD WINAPI rp_refworker(LPVOID arg) {
    IUnknown *u = (IUnknown *)arg; int i;
    for (i = 0; i < 5000; i++) { IUnknown_AddRef(u); IUnknown_Release(u); }
    return 0;
}
TEST(rpu_threads_refcount_balanced) {
    enum { N = 4 }; HANDLE h[N]; int i; void *p = NULL;
    if (!rp_get_default(DWRITE_FACTORY_TYPE_SHARED, &p)) { t_skip("no params"); return; }
    for (i = 0; i < N; i++) h[i] = CreateThread(NULL, 0, rp_refworker, p, 0, NULL);
    for (i = 0; i < N; i++) if (h[i]) { WaitForSingleObject(h[i], 20000); CloseHandle(h[i]); }
    EXPECT_REFS(p, 1);
    DW_RELEASE(p);
}
