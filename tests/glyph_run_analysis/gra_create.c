/* IDWriteGlyphRunAnalysis creation: argument validation of the three factory overloads, out-parameter state, object identity,
 * lifetime and independence from the memory the caller passed in.
 *
 * Facts (Win10 19041 dwrite.dll):
 *   IDWriteFactory::CreateGlyphRunAnalysis (IDWriteFactory1 style, with pixelsPerDip): *out = NULL first; E_INVALIDARG (0x80070057) for
 *     run == NULL, run->fontFace == NULL, run->glyphIndices == NULL (also when glyphCount == 0), fontEmSize < 0, pixelsPerDip <= 0, NaN in
 *     pixelsPerDip / baseline origin / any of the six transform members, renderingMode > 6, measuringMode > 2. The modes DEFAULT (0) and
 *     OUTLINE (6) pass that first filter but the analysis constructor rejects them (renderingMode argument exception) -> E_INVALIDARG.
 *   IDWriteFactory2 overload (no pixelsPerDip): same plus gridFitMode > 2 and antialiasMode > 1 invalid.
 *   IDWriteFactory3 overload (DWRITE_RENDERING_MODE1): renderingMode > 7 invalid; NATURAL_SYMMETRIC_DOWNSAMPLED (7) is valid.
 *   The glyph bitmaps are produced at construction: the object does not read the caller's run again.
 */
#include "gra.h"

#define F_ALL_MODES 5
static const DWRITE_RENDERING_MODE good_modes[F_ALL_MODES] = { DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_GDI_CLASSIC,
    DWRITE_RENDERING_MODE_GDI_NATURAL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC };

TEST(gra_create_valid_all_modes_and_measuring) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    int m, mm;
    for (m = 0; m < F_ALL_MODES; m++) for (mm = 0; mm < 3; mm++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, good_modes[m], (DWRITE_MEASURING_MODE)mm, 0, 0, &a);
        CHECKF(hr == S_OK, "mode %s measuring %d -> %s", gra_mode_name(good_modes[m]), mm, dw_hr_name(hr));
        if (hr == S_OK && !GRA_NIL(a) && a) { EXPECT_REFS(a, 1); IDWriteGlyphRunAnalysis_Release(a); }
    }
    gra_close(&fx);
}

TEST(gra_create_mode_default_invalid) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    int mm;
    for (mm = 0; mm < 3; mm++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_DEFAULT, (DWRITE_MEASURING_MODE)mm, 0, 0, &a);
        CHECK_HR(hr, GRA_E_INVALIDARG);
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_mode_outline_invalid) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    int mm;
    for (mm = 0; mm < 3; mm++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_OUTLINE, (DWRITE_MEASURING_MODE)mm, 0, 0, &a);
        CHECK_HR(hr, GRA_E_INVALIDARG);
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_mode_out_of_range) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    static const int bad[] = { 7, 8, 9, 100, 0x7fffffff, -1, (int)0x80000000 };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, (DWRITE_RENDERING_MODE)bad[i], DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
        CHECKF(hr == GRA_E_INVALIDARG, "mode %d -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_measuring_out_of_range) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    static const int bad[] = { 3, 4, 100, 0x7fffffff, -1, (int)0x80000000 };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, (DWRITE_MEASURING_MODE)bad[i], 0, 0, &a);
        CHECKF(hr == GRA_E_INVALIDARG, "measuring %d -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_null_run) {
    GRA_FIX(fx);
    if (t_is_wine()) { t_skip("emulator dereferences the unvalidated run pointer"); gra_close(&fx); return; }
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(fx.f, NULL, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a), GRA_E_INVALIDARG);
    CHECK_OUT_NULL(a);
    gra_close(&fx);
}

TEST(gra_create_null_fontface) {
    GRA_FIX(fx);
    if (t_is_wine()) { t_skip("emulator dereferences the unvalidated run pointer"); gra_close(&fx); return; }
    gra_run(&fx, "A", 24.0f);
    fx.run.fontFace = NULL;
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a), GRA_E_INVALIDARG);
    CHECK_OUT_NULL(a);
    gra_close(&fx);
}

TEST(gra_create_null_indices) {
    GRA_FIX(fx);
    if (t_is_wine()) { t_skip("emulator dereferences the unvalidated run pointer"); gra_close(&fx); return; }
    gra_run(&fx, "ABC", 24.0f);
    fx.run.glyphIndices = NULL;
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a), GRA_E_INVALIDARG);
    CHECK_OUT_NULL(a);
    /* NULL glyphIndices is rejected even when the run is empty: the check precedes the glyph count. */
    fx.run.glyphCount = 0;
    a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a), GRA_E_INVALIDARG);
    CHECK_OUT_NULL(a);
    gra_close(&fx);
}

TEST(gra_create_negative_em) {
    GRA_FIX(fx);
    static const float bad[] = { -1.0f, -0.001f, -1000.0f, -FLT_MAX, -INFINITY };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        gra_run(&fx, "A", bad[i]);
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
        CHECKF(hr == GRA_E_INVALIDARG, "em %g -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_zero_em) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 0.0f);
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    /* fontEmSize == 0 passes the factory's "< 0" test; whether the constructor accepts it is a note. */
    HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
    t_note("em 0 -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && a) {
        RECT rc;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        t_note("em 0 bounds (%ld,%ld,%ld,%ld)", rc.left, rc.top, rc.right, rc.bottom);
        IDWriteGlyphRunAnalysis_Release(a);
    } else CHECK_OUT_NULL(a);
    gra_close(&fx);
}

TEST(gra_create_ppd_invalid) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    static const float bad[] = { 0.0f, -0.0f, -1.0f, -1e-30f, -FLT_MAX, -INFINITY };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, bad[i], NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
        CHECKF(hr == GRA_E_INVALIDARG, "pixelsPerDip %g -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

/* The legacy overload tests "pixelsPerDip <= 0"; a NaN compares false there, so its outcome is not asserted. */
TEST(gra_create_ppd_nan_noted) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, NAN, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
    t_note("pixelsPerDip NaN -> %s", dw_hr_name(hr));
    CHECK(hr == S_OK || hr == GRA_E_INVALIDARG);
    if (SUCCEEDED(hr) && a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    else CHECK_OUT_NULL(a);
    gra_close(&fx);
}

TEST(gra_create_ppd_valid_values) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    static const float good[] = { 0.25f, 0.5f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f };
    size_t i;
    for (i = 0; i < sizeof good / sizeof good[0]; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, good[i], NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
        CHECKF(hr == S_OK, "pixelsPerDip %g -> %s", good[i], dw_hr_name(hr));
        if (hr == S_OK && a) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_nan_origin) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    float ox[] = { NAN, 0, NAN }, oy[] = { 0, NAN, NAN };
    int i;
    for (i = 0; i < 3; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, ox[i], oy[i], &a);
        CHECKF(hr == GRA_E_INVALIDARG, "origin case %d -> %s", i, dw_hr_name(hr));
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_nan_transform_members) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    int k;
    for (k = 0; k < 6; k++) {
        DWRITE_MATRIX m = gra_ident;
        FLOAT *p = &m.m11;
        p[k] = NAN;
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
        CHECKF(hr == GRA_E_INVALIDARG, "NaN in matrix member %d -> %s", k, dw_hr_name(hr));
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_null_and_identity_transform_equal) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    CHECK_HR_OK(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 3, 4));
    CHECK_HR_OK(gra_make(&fx, &b, 1.0f, &gra_ident, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 3, 4));
    if (a && b) {
        RECT ra, rb;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &ra));
        CHECK_HR_OK(gra_bounds(b, DWRITE_TEXTURE_ALIASED_1x1, &rb));
        CHECK_RECT_EQ(ra, rb);
        CHECKF(!gra_rect_empty(&ra), "bounds empty");
    }
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

TEST(gra_create_empty_run_ok) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    fx.run.glyphCount = 0;
    int m;
    for (m = 0; m < F_ALL_MODES; m++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, good_modes[m], DWRITE_MEASURING_MODE_NATURAL, 5.0f, 6.0f, &a);
        CHECKF(hr == S_OK, "empty run mode %s -> %s", gra_mode_name(good_modes[m]), dw_hr_name(hr));
        if (hr == S_OK && a && !GRA_NIL(a)) {
            RECT rc;
            CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
            CHECK_RECT_ZERO(rc);
            CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc));
            CHECK_RECT_ZERO(rc);
            IDWriteGlyphRunAnalysis_Release(a);
        }
    }
    gra_close(&fx);
}

TEST(gra_create_advances_offsets_optional) {
    GRA_FIX(fx);
    gra_run(&fx, "ABC", 24.0f);
    CHECK(fx.run.glyphAdvances == NULL && fx.run.glyphOffsets == NULL);
    IDWriteGlyphRunAnalysis *a = NULL;
    CHECK_HR_OK(gra_make_al(&fx, &a, 0, 0));
    DW_RELEASE(a);
    float adv[3] = { 10, 11, 12 };
    gra_advances(&fx, adv);
    CHECK_HR_OK(gra_make_al(&fx, &a, 0, 0));
    DW_RELEASE(a);
    DWRITE_GLYPH_OFFSET off[3] = { {1, 2}, {0, 0}, {-3, 4} };
    gra_offsets(&fx, off);
    CHECK_HR_OK(gra_make_al(&fx, &a, 0, 0));
    DW_RELEASE(a);
    fx.run.glyphAdvances = NULL;
    CHECK_HR_OK(gra_make_al(&fx, &a, 0, 0));
    DW_RELEASE(a);
    gra_close(&fx);
}

TEST(gra_create_qi_and_identity) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    EXPECT_REFS(a, 1);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteGlyphRunAnalysis, 1), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFactory, 0),
        DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteTextLayout, 0), DW_QI_ROW(IDWriteRenderingParams, 0),
        DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteBitmapRenderTarget, 0), DW_QI_ROW(IDWriteGdiInterop, 0) };
    DW_CHECK_QI(a, rows);
    IUnknown *u = NULL;
    CHECK_HR_OK(IDWriteGlyphRunAnalysis_QueryInterface(a, &IID_IUnknown, (void **)&u));
    EXPECT_REFS(a, 2);
    if (u) { CHECK((void *)u == (void *)a); IUnknown_Release(u); }
    EXPECT_REFS(a, 1);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_create_qi_failure_zeroes_out) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    void *p = GRA_SENTINEL;
    CHECK_HR(IDWriteGlyphRunAnalysis_QueryInterface(a, &IID_IDWriteFont, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    p = GRA_SENTINEL;
    CHECK_HR(IDWriteGlyphRunAnalysis_QueryInterface(a, &IID_IDWriteTextRenderer, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    EXPECT_REFS(a, 1);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_create_refcount_balance) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    int i;
    for (i = 0; i < 10; i++) IDWriteGlyphRunAnalysis_AddRef(a);
    EXPECT_REFS(a, 11);
    for (i = 0; i < 10; i++) IDWriteGlyphRunAnalysis_Release(a);
    EXPECT_REFS(a, 1);
    ULONG r = IDWriteGlyphRunAnalysis_AddRef(a);
    EXPECT_EQ(r, 2);
    r = IDWriteGlyphRunAnalysis_Release(a);
    EXPECT_EQ(r, 1);
    r = IDWriteGlyphRunAnalysis_Release(a);
    EXPECT_EQ(r, 0);
    gra_close(&fx);
}

TEST(gra_create_font_stream_balance_note) {
    /* The face keeps its own table fragments, so outstanding fragments are legitimate; only record them and verify that the
     * analysis itself can be released afterwards without disturbing the face. */
    GRA_FIX(fx);
    gra_run(&fx, "ABC", 32.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    RECT rc;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    LONG before = dwx_loader_total_outstanding(fx.h.ld);
    IDWriteGlyphRunAnalysis_Release(a);
    LONG after = dwx_loader_total_outstanding(fx.h.ld);
    t_note("outstanding stream fragments: %ld with analysis, %ld after release", (long)before, (long)after);
    CHECK(after <= before);
    CHECK(IDWriteFontFace_GetGlyphCount(fx.h.face) == TTF_DEF_NGLYPHS);
    gra_close(&fx);
}

TEST(gra_create_two_objects_independent) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(gra_make_al(&fx, &a, 0, 0));
    GRA_REQ(gra_make_al(&fx, &b, 10, 20));
    CHECK(a != b);
    EXPECT_REFS(a, 1);
    EXPECT_REFS(b, 1);
    RECT ra, rb;
    IDWriteGlyphRunAnalysis_Release(a);
    CHECK_HR_OK(gra_bounds(b, DWRITE_TEXTURE_ALIASED_1x1, &rb));
    CHECK_RECT(rb, GRA_X0(512) + 10, -GRA_HT(512) + 20, GRA_X1(512) + 10, 20);
    IDWriteGlyphRunAnalysis_Release(b);
    gra_close(&fx);
}

TEST(gra_create_survives_run_buffer_modification) {
    GRA_FIX(fx);
    float adv[3] = { 250, 250, 250 };
    DWRITE_GLYPH_OFFSET off[3] = { {0, 0}, {5, 0}, {0, 7} };
    int m;
    for (m = 0; m < F_ALL_MODES; m++) {
        gra_run(&fx, "ABC", 512.0f);
        gra_advances(&fx, adv); gra_offsets(&fx, off);
        IDWriteGlyphRunAnalysis *a = NULL;
        HRESULT hr = gra_make(&fx, &a, 1.0f, NULL, good_modes[m], DWRITE_MEASURING_MODE_NATURAL, 0, 0);
        if (FAILED(hr) || !a) { CHECK_HR_OK(hr); continue; }
        gra_tex t0, t1; RECT r0, r1;
        DWRITE_TEXTURE_TYPE tt = DWRITE_TEXTURE_ALIASED_1x1;
        CHECK_HR_OK(gra_bounds(a, tt, &r0));
        if (gra_rect_empty(&r0)) { tt = DWRITE_TEXTURE_CLEARTYPE_3x1; CHECK_HR_OK(gra_bounds(a, tt, &r0)); }
        int got0 = gra_tex_get(a, tt, &t0);
        /* trash every buffer the run pointed to */
        memset(fx.gi, 0, sizeof fx.gi); memset(fx.adv, 0xFF, sizeof fx.adv); memset(fx.off, 0x7F, sizeof fx.off);
        CHECK_HR_OK(gra_bounds(a, tt, &r1));
        CHECK_RECT_EQ(r0, r1);
        int got1 = gra_tex_get(a, tt, &t1);
        CHECKF(got0 == got1, "mode %s: texture availability changed", gra_mode_name(good_modes[m]));
        if (got0 && got1) CHECKF(t0.n == t1.n && !memcmp(t0.p, t1.p, t0.n), "mode %s: texture content changed after the run buffer was trashed", gra_mode_name(good_modes[m]));
        if (got0) gra_tex_free(&t0);
        if (got1) gra_tex_free(&t1);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_survives_run_struct_destruction) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    DWRITE_GLYPH_RUN *heap = (DWRITE_GLYPH_RUN *)malloc(sizeof *heap);
    UINT16 *gids = (UINT16 *)malloc(sizeof(UINT16));
    *heap = fx.run; gids[0] = fx.gi[0]; heap->glyphIndices = gids;
    IDWriteGlyphRunAnalysis *a = NULL;
    HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(fx.f, heap, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
    memset(gids, 0xEE, sizeof(UINT16)); free(gids);
    memset(heap, 0xEE, sizeof *heap); free(heap);
    CHECK_HR_OK(hr);
    if (a) {
        RECT rc;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        CHECK_RECT(rc, GRA_X0(512), -GRA_HT(512), GRA_X1(512), 0);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_survives_matrix_modification) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    DWRITE_MATRIX m = { 0, 1, -1, 0, 0, 0 };
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    RECT r0, r1;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
    m.m11 = 9; m.m12 = 9; m.m21 = 9; m.m22 = 9; m.dx = 99; m.dy = 99;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r1));
    CHECK_RECT_EQ(r0, r1);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_create_after_factory_release) {
    /* A private factory is released while the analysis lives: the analysis keeps working (glyph bitmaps exist since construction). */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("isolated factory unavailable"); return; }
    dwx_font h;
    if (FAILED(dwx_font_open_default(&h, f)) || !h.face) { t_skip("font unavailable"); dwx_font_close(&h); IDWriteFactory_Release(f); return; }
    UINT16 g = (UINT16)ttf_default_gid('A');
    DWRITE_GLYPH_RUN run; memset(&run, 0, sizeof run);
    run.fontFace = h.face; run.fontEmSize = 512.0f; run.glyphCount = 1; run.glyphIndices = &g;
    IDWriteGlyphRunAnalysis *a = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(f, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a));
    dwx_font_close(&h);
    ULONG r = IDWriteFactory_Release(f);
    t_note("factory refcount after release: %lu", r);
    if (a) {
        RECT rc;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        CHECK_RECT(rc, GRA_X0(512), -GRA_HT(512), GRA_X1(512), 0);
        gra_tex t;
        if (gra_tex_get(a, DWRITE_TEXTURE_ALIASED_1x1, &t)) { CHECK(gra_count_eq(&t, 255) == t.n); gra_tex_free(&t); }
        IDWriteGlyphRunAnalysis_Release(a);
    }
}

TEST(gra_create_isolated_equals_shared) {
    GRA_FIX(fx);
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!iso) { t_skip("isolated factory unavailable"); gra_close(&fx); return; }
    dwx_font h2;
    if (FAILED(dwx_font_open_default(&h2, iso)) || !h2.face) { t_skip("font unavailable"); dwx_font_close(&h2); IDWriteFactory_Release(iso); gra_close(&fx); return; }
    gra_run(&fx, "AB", 512.0f);
    DWRITE_GLYPH_RUN r2 = fx.run; r2.fontFace = h2.face;
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(fx.f, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 2, 3, &a));
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(iso, &r2, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 2, 3, &b));
    if (a && b) {
        RECT ra, rb;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &ra));
        CHECK_HR_OK(gra_bounds(b, DWRITE_TEXTURE_ALIASED_1x1, &rb));
        CHECK_RECT_EQ(ra, rb);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    dwx_font_close(&h2);
    IDWriteFactory_Release(iso);
    gra_close(&fx);
}

TEST(gra_create_face_from_other_factory_ok) {
    /* A face created by factory A can be rasterised through factory B's CreateGlyphRunAnalysis. */
    GRA_FIX(fx);
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!iso) { t_skip("isolated factory unavailable"); gra_close(&fx); return; }
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(iso, &fx.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
    t_note("face of the shared factory through an isolated factory -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr)) { RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc)); CHECK_RECT(rc, GRA_X0(512), -GRA_HT(512), GRA_X1(512), 0); IDWriteGlyphRunAnalysis_Release(a); }
    else CHECK_OUT_NULL(a);
    IDWriteFactory_Release(iso);
    gra_close(&fx);
}

TEST(gra_create_glyph_index_out_of_range_no_crash) {
    GRA_FIX(fx);
    UINT16 g[3] = { (UINT16)ttf_default_gid('A'), 0xFFFF, 5000 };
    gra_run_gids(&fx, g, 3, 64.0f);
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    HRESULT hr = gra_make_al(&fx, &a, 0, 0);
    t_note("glyph ids beyond maxp.numGlyphs -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && a) {
        RECT rc;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        t_note("bounds (%ld,%ld,%ld,%ld)", rc.left, rc.top, rc.right, rc.bottom);
        IDWriteGlyphRunAnalysis_Release(a);
    } else CHECK_OUT_NULL(a);
    gra_close(&fx);
}

TEST(gra_create_sideways_bool_any_nonzero) {
    GRA_FIX(fx);
    int vals[] = { 1, 2, -1, 0x100 };
    size_t i;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        gra_run(&fx, "AB", 512.0f);
        fx.run.isSideways = TRUE;
        IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
        GRA_REQ(gra_make_al(&fx, &a, 0, 0));
        fx.run.isSideways = vals[i];
        CHECK_HR_OK(gra_make_al(&fx, &b, 0, 0));
        if (a && b) {
            RECT ra, rb;
            CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &ra));
            CHECK_HR_OK(gra_bounds(b, DWRITE_TEXTURE_ALIASED_1x1, &rb));
            CHECKF(ra.left == rb.left && ra.top == rb.top && ra.right == rb.right && ra.bottom == rb.bottom, "isSideways=%d differs from TRUE", vals[i]);
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    gra_close(&fx);
}

/* ---- IDWriteFactory2 overload -------------------------------------------------------------------------------------- */
TEST(gra_create_f2_valid_matrix) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 24.0f);
    int m, gf, aa;
    for (m = 0; m < F_ALL_MODES; m++) for (gf = 0; gf < 3; gf++) for (aa = 0; aa < 2; aa++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = gra_make2(&fx, &a, NULL, good_modes[m], DWRITE_MEASURING_MODE_NATURAL, (DWRITE_GRID_FIT_MODE)gf, (DWRITE_TEXT_ANTIALIAS_MODE)aa, 0, 0);
        CHECKF(hr == S_OK, "f2 mode %s gridfit %d aa %d -> %s", gra_mode_name(good_modes[m]), gf, aa, dw_hr_name(hr));
        if (hr == S_OK && a) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_f2_invalid_args) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 24.0f);
    struct { const char *what; int mode, mm, gf, aa; } c[] = {
        { "mode DEFAULT", 0, 0, 0, 0 }, { "mode OUTLINE", 6, 0, 0, 0 }, { "mode 7", 7, 0, 0, 0 }, { "mode -1", -1, 0, 0, 0 },
        { "measuring 3", 4, 3, 0, 0 }, { "measuring -1", 4, -1, 0, 0 }, { "gridfit 3", 4, 0, 3, 0 }, { "gridfit -1", 4, 0, -1, 0 },
        { "aa 2", 4, 0, 0, 2 }, { "aa -1", 4, 0, 0, -1 }, { "aa 100", 4, 0, 0, 100 },
    };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = gra_make2(&fx, &a, NULL, (DWRITE_RENDERING_MODE)c[i].mode, (DWRITE_MEASURING_MODE)c[i].mm, (DWRITE_GRID_FIT_MODE)c[i].gf, (DWRITE_TEXT_ANTIALIAS_MODE)c[i].aa, 0, 0);
        CHECKF(hr == GRA_E_INVALIDARG, "f2 %s -> %s", c[i].what, dw_hr_name(hr));
        CHECK_OUT_NULL(a);
        if (a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_f2_null_args) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2);
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    if (!t_is_wine()) {   /* an emulator's overload dereferences the run before validating it */
        CHECK_HR(IDWriteFactory2_CreateGlyphRunAnalysis(f2, NULL, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0, &a), GRA_E_INVALIDARG);
        CHECK_OUT_NULL(a);
    }
    if (t_is_wine()) { t_skip("emulator dereferences the unvalidated run pointer"); gra_close(&fx); return; }
    gra_run(&fx, "A", 24.0f);
    fx.run.fontFace = NULL;
    a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    CHECK_HR(gra_make2(&fx, &a, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0), GRA_E_INVALIDARG);
    fx.run.fontFace = fx.h.face; fx.run.glyphIndices = NULL;
    CHECK_HR(gra_make2(&fx, &a, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0), GRA_E_INVALIDARG);
    fx.run.glyphIndices = fx.gi; fx.run.fontEmSize = -1.0f;
    CHECK_HR(gra_make2(&fx, &a, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0), GRA_E_INVALIDARG);
    CHECK_OUT_NULL(a);
    gra_close(&fx);
}

TEST(gra_create_f2_nan_args) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "A", 24.0f);
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    CHECK_HR(gra_make2(&fx, &a, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, NAN, 0), GRA_E_INVALIDARG);
    CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    CHECK_HR(gra_make2(&fx, &a, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, NAN), GRA_E_INVALIDARG);
    CHECK_OUT_NULL(a);
    int k;
    for (k = 0; k < 6; k++) {
        DWRITE_MATRIX m = gra_ident; (&m.m11)[k] = NAN;
        a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        CHECKF(gra_make2(&fx, &a, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0) == GRA_E_INVALIDARG, "matrix member %d", k);
        CHECK_OUT_NULL(a);
    }
    gra_close(&fx);
}

TEST(gra_create_f2_matches_f1_bounds) {
    GRA_FIX(fx); GRA_NEED_F(fx, 2);
    gra_run(&fx, "AB", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 7, 9));
    GRA_REQ(gra_make2(&fx, &b, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 7, 9));
    RECT ra, rb;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &ra));
    CHECK_HR_OK(gra_bounds(b, DWRITE_TEXTURE_ALIASED_1x1, &rb));
    CHECK_RECT_EQ(ra, rb);
    CHECK_RECT(ra, GRA_X0(512) + 7, -GRA_HT(512) + 9, GRA_ADV(512) + GRA_X1(512) + 7, 9);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

/* ---- IDWriteFactory3 overload (DWRITE_RENDERING_MODE1) ------------------------------------------------------------- */
TEST(gra_create_f3_modes) {
    GRA_FIX(fx); GRA_NEED_F(fx, 3);
    gra_run(&fx, "A", 24.0f);
    int m;
    for (m = -1; m <= 9; m++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = gra_make3(&fx, &a, NULL, (DWRITE_RENDERING_MODE1)m, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0);
        /* valid: ALIASED..NATURAL_SYMMETRIC and the downsampled mode 7; DEFAULT/OUTLINE rejected by the constructor; >7 and <0 rejected up front */
        int valid = (m >= 1 && m <= 5) || m == 7;
        if (valid) CHECKF(hr == S_OK, "f3 mode %d (%s) -> %s", m, gra_mode_name(m), dw_hr_name(hr));
        else { CHECKF(hr == GRA_E_INVALIDARG, "f3 mode %d -> %s", m, dw_hr_name(hr)); CHECK_OUT_NULL(a); }
        if (SUCCEEDED(hr) && a && !GRA_NIL(a)) IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_create_f3_invalid_other_args) {
    GRA_FIX(fx); GRA_NEED_F(fx, 3);
    gra_run(&fx, "A", 24.0f);
    struct { const char *what; int mm, gf, aa; } c[] = {
        { "measuring 3", 3, 0, 0 }, { "measuring -1", -1, 0, 0 }, { "gridfit 3", 0, 3, 0 }, { "aa 2", 0, 0, 2 }, { "aa -1", 0, 0, -1 },
    };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
        HRESULT hr = gra_make3(&fx, &a, NULL, DWRITE_RENDERING_MODE1_NATURAL, (DWRITE_MEASURING_MODE)c[i].mm, (DWRITE_GRID_FIT_MODE)c[i].gf, (DWRITE_TEXT_ANTIALIAS_MODE)c[i].aa, 0, 0);
        CHECKF(hr == GRA_E_INVALIDARG, "f3 %s -> %s", c[i].what, dw_hr_name(hr));
        CHECK_OUT_NULL(a);
    }
    gra_close(&fx);
}

TEST(gra_create_f3_downsampled_mode_usable) {
    GRA_FIX(fx); GRA_NEED_F(fx, 3);
    gra_run(&fx, "A", 40.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make3(&fx, &a, NULL, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0));
    RECT r0, r1;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1));
    t_note("downsampled: type0 (%ld,%ld,%ld,%ld) type1 (%ld,%ld,%ld,%ld)", r0.left, r0.top, r0.right, r0.bottom, r1.left, r1.top, r1.right, r1.bottom);
    /* exactly one texture type matches the analysis; the other reports an all-zero rectangle */
    CHECK(gra_rect_empty(&r0) != gra_rect_empty(&r1));
    DW_RELEASE(a);
    gra_close(&fx);
}

TEST(gra_create_f3_equals_f2_for_shared_modes) {
    GRA_FIX(fx); GRA_NEED_F(fx, 3); GRA_NEED_F(fx, 2);
    gra_run(&fx, "AB", 512.0f);
    int m;
    for (m = 0; m < F_ALL_MODES; m++) {
        IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
        HRESULT h2 = gra_make2(&fx, &a, NULL, good_modes[m], DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 1, 2);
        HRESULT h3 = gra_make3(&fx, &b, NULL, (DWRITE_RENDERING_MODE1)good_modes[m], DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 1, 2);
        CHECK_HR_OK(h2); CHECK_HR_OK(h3);
        if (a && b) {
            RECT ra, rb;
            int t;
            for (t = 0; t < 2; t++) {
                CHECK_HR_OK(gra_bounds(a, (DWRITE_TEXTURE_TYPE)t, &ra));
                CHECK_HR_OK(gra_bounds(b, (DWRITE_TEXTURE_TYPE)t, &rb));
                CHECKF(!memcmp(&ra, &rb, sizeof ra), "mode %s texture %d: f2 (%ld,%ld,%ld,%ld) f3 (%ld,%ld,%ld,%ld)", gra_mode_name(good_modes[m]), t,
                       ra.left, ra.top, ra.right, ra.bottom, rb.left, rb.top, rb.right, rb.bottom);
            }
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    gra_close(&fx);
}

TEST(gra_create_many_objects_released) {
    GRA_FIX(fx);
    gra_run(&fx, "ABC", 48.0f);
    int i;
    for (i = 0; i < 200; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        HRESULT hr = gra_make(&fx, &a, 1.0f, NULL, good_modes[i % F_ALL_MODES], (DWRITE_MEASURING_MODE)(i % 3), (float)(i % 7), (float)(i % 5));
        if (FAILED(hr)) { CHECK_HR_OK(hr); break; }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}
