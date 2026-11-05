/* E_INVALIDARG catalogue for the factory family (IDWriteFactory .. IDWriteFactory6), the in-memory loader and GlyphRunAnalysis creation.
 * Every row is a branch visible in the dwrite.dll probing: argument predicate -> ArgumentException/-0x7ff8ffa9, with the out
 * parameter written to NULL before the check. */
#include "eh.h"

/* ---- CreateTextFormat --------------------------------------------------------------------------------------------------------- */
typedef struct { const WCHAR *family, *locale; DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; DWRITE_FONT_STRETCH sx; float size; const char *why; } tf_row;
#define TFOK L"Arial", L"en-us", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL
static const tf_row tf_bad[] = {
    { NULL, L"en-us", 400, 0, 5, 12.f, "family NULL" }, { L"Arial", NULL, 400, 0, 5, 12.f, "locale NULL" },
    { L"Arial", L"en-us", 0, 0, 5, 12.f, "weight 0" }, { L"Arial", L"en-us", 1000, 0, 5, 12.f, "weight 1000" },
    { L"Arial", L"en-us", 5000, 0, 5, 12.f, "weight 5000" }, { L"Arial", L"en-us", 0xFFFFFFFFu, 0, 5, 12.f, "weight -1" },
    { L"Arial", L"en-us", 400, 3, 5, 12.f, "style 3" }, { L"Arial", L"en-us", 400, 0xFFFFFFFFu, 5, 12.f, "style -1" },
    { L"Arial", L"en-us", 400, 0, 0, 12.f, "stretch 0" }, { L"Arial", L"en-us", 400, 0, 10, 12.f, "stretch 10" },
    { L"Arial", L"en-us", 400, 0, 0xFFFFFFFFu, 12.f, "stretch -1" },
    { TFOK, 0.f, "size 0" }, { TFOK, -1.f, "size -1" }, { TFOK, -0.0f, "size -0" }, { TFOK, -1e-20f, "size tiny negative" },
    { TFOK, NAN_F, "size NaN" }, { TFOK, -INF_F, "size -inf" },
};
TEST(textformat_invalid_arguments) {
    NEED_FACTORY(); unsigned i;
    for (i = 0; i < sizeof tf_bad / sizeof tf_bad[0]; i++) {
        const tf_row *r = &tf_bad[i]; IDWriteTextFormat *t = SENT(IDWriteTextFormat *);
        if (t_is_wine() && (!r->family || !r->locale)) continue;      /* the emulated implementation dereferences NULL strings */
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, r->family, NULL, r->w, r->st, r->sx, r->size, r->locale, &t);
        CHECKF(hr == E_INVALIDARG && t == NULL, "%s -> %s out=%p", r->why, dw_hr_name(hr), t);
        if (t && t != SENT(IDWriteTextFormat *)) DW_RELEASE(t);
    }
    DONE_FACTORY();
}
TEST(textformat_valid_boundaries) {
    NEED_FACTORY(); unsigned w, i; static const DWRITE_FONT_WEIGHT ws[] = { 1, 100, 400, 999 };
    for (i = 0; i < 4; i++) for (w = 0; w < 3; w++) {
        IDWriteTextFormat *t = NULL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, ws[i], (DWRITE_FONT_STYLE)w, (DWRITE_FONT_STRETCH)(1 + w * 4), 10.f, L"en-us", &t);
        CHECKF(hr == S_OK && t, "weight %d style %u -> %s", ws[i], w, dw_hr_name(hr)); DW_RELEASE(t);
    }
    for (i = 1; i <= 9; i++) {
        IDWriteTextFormat *t = NULL;
        CHECK_HR(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, (DWRITE_FONT_STRETCH)i, 10.f, L"en-us", &t), S_OK); DW_RELEASE(t);
    }
    static const float sizes[] = { 0.001f, 0.5f, 1.f, 1000.f, 100000.f };
    for (i = 0; i < 5; i++) { IDWriteTextFormat *t = NULL; CHECKF(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, 0, 5, sizes[i], L"en-us", &t) == S_OK, "size %g", sizes[i]); DW_RELEASE(t); }
    DONE_FACTORY();
}
TEST(textformat_unknown_family_and_locale_are_accepted) {
    /* Neither a missing family nor a made-up locale name is an error at creation time. */
    NEED_FACTORY(); IDWriteTextFormat *t = NULL;
    CHECK_HR(IDWriteFactory_CreateTextFormat(f, L"No Such Family Zq", NULL, 400, 0, 5, 12.f, L"en-us", &t), S_OK); DW_RELEASE(t);
    CHECK_HR(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, 0, 5, 12.f, L"zz-ZZ-nonsense", &t), S_OK); DW_RELEASE(t);
    CHECK_HR(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, 0, 5, 12.f, L"", &t), S_OK); DW_RELEASE(t);
    DONE_FACTORY();
}

/* ---- CreateTextLayout / CreateGdiCompatibleTextLayout ------------------------------------------------------------------------- */
TEST(textlayout_invalid_arguments) {
    NEED_FACTORY(); IDWriteTextFormat *fmt = eh_format(f, 12.f); IDWriteTextLayout *l; unsigned i;
    if (fmt) {
        static const struct { int null_text, null_fmt; float w, h; const char *why; } rows[] = {
            { 1, 0, 100, 100, "text NULL" }, { 0, 1, 100, 100, "format NULL" }, { 0, 0, -1, 100, "width -1" }, { 0, 0, 100, -1, "height -1" },
            { 0, 0, -0.5f, -0.5f, "both negative" }, { 0, 0, NAN_F, 100, "width NaN" }, { 0, 0, 100, NAN_F, "height NaN" }, { 1, 1, 100, 100, "text and format NULL" },
            { 0, 0, -INF_F, 100, "width -inf" },
        };
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            l = SENT(IDWriteTextLayout *);
            HRESULT hr = IDWriteFactory_CreateTextLayout(f, rows[i].null_text ? NULL : L"Hello", 5, rows[i].null_fmt ? NULL : fmt, rows[i].w, rows[i].h, &l);
            CHECKF(hr == E_INVALIDARG && l == NULL, "%s -> %s out=%p", rows[i].why, dw_hr_name(hr), l);
            if (l && l != SENT(IDWriteTextLayout *)) DW_RELEASE(l);
        }
        l = SENT(IDWriteTextLayout *);
        CHECK_HR(IDWriteFactory_CreateTextLayout(f, NULL, 0, fmt, 100, 100, &l), E_INVALIDARG); CHECK_OUT_NULL(l);        /* NULL string is rejected even for length 0 */
        DW_RELEASE(fmt);
    }
    DONE_FACTORY();
}
TEST(textlayout_valid_boundaries) {
    NEED_FACTORY(); IDWriteTextFormat *fmt = eh_format(f, 12.f); IDWriteTextLayout *l = NULL;
    if (fmt) {
        CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"", 0, fmt, 0, 0, &l), S_OK); DW_RELEASE(l);
        CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 0, fmt, 100, 100, &l), S_OK); DW_RELEASE(l);
        CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, 0, 100, &l), S_OK); DW_RELEASE(l);
        CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, 100, 0, &l), S_OK); DW_RELEASE(l);
        CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, INF_F, INF_F, &l), S_OK); DW_RELEASE(l);
        CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, 1e30f, 1e30f, &l), S_OK); DW_RELEASE(l);
        DW_RELEASE(fmt);
    }
    DONE_FACTORY();
}
TEST(gdi_textlayout_invalid_arguments) {
    NEED_FACTORY(); IDWriteTextFormat *fmt = eh_format(f, 12.f); IDWriteTextLayout *l; unsigned i;
    if (fmt) {
        DWRITE_MATRIX nanm = { 1, 0, 0, 1, NAN_F, 0 }; DWRITE_MATRIX ok = { 1, 0, 0, 1, 0, 0 };
        static const struct { int null_text, null_fmt; float w, h, ppd; const DWRITE_MATRIX *m; int nm; const char *why; } rows[] = {
            { 1, 0, 100, 100, 1.f, NULL, 0, "text NULL" }, { 0, 1, 100, 100, 1.f, NULL, 0, "format NULL" }, { 0, 0, -1, 100, 1.f, NULL, 0, "width -1" },
            { 0, 0, 100, -1, 1.f, NULL, 0, "height -1" }, { 0, 0, NAN_F, 100, 1.f, NULL, 0, "width NaN" }, { 0, 0, 100, NAN_F, 1.f, NULL, 0, "height NaN" },
            { 0, 0, 100, 100, NAN_F, NULL, 0, "pixelsPerDip NaN" }, { 0, 0, 100, 100, 1.f, NULL, 1, "transform with NaN dx" },
        };
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            l = SENT(IDWriteTextLayout *);
            HRESULT hr = IDWriteFactory_CreateGdiCompatibleTextLayout(f, rows[i].null_text ? NULL : L"Hello", 5, rows[i].null_fmt ? NULL : fmt, rows[i].w, rows[i].h, rows[i].ppd, rows[i].nm ? &nanm : rows[i].m, FALSE, &l);
            CHECKF(hr == E_INVALIDARG && l == NULL, "%s -> %s out=%p", rows[i].why, dw_hr_name(hr), l);
            if (l && l != SENT(IDWriteTextLayout *)) DW_RELEASE(l);
        }
        l = NULL;
        CHECK_HR(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"Hello", 5, fmt, 100, 100, 1.f, &ok, TRUE, &l), S_OK); DW_RELEASE(l);
        CHECK_HR(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"Hello", 5, fmt, 100, 100, 1.f, NULL, FALSE, &l), S_OK); DW_RELEASE(l);
        DW_RELEASE(fmt);
    }
    DONE_FACTORY();
}
TEST(ellipsis_trimming_sign_null_format) {
    NEED_FACTORY(); IDWriteInlineObject *o = SENT(IDWriteInlineObject *);
    CHECK_HR(IDWriteFactory_CreateEllipsisTrimmingSign(f, NULL, &o), E_INVALIDARG); CHECK_OUT_NULL(o);
    DONE_FACTORY();
}

/* ---- CreateNumberSubstitution -------------------------------------------------------------------------------------------------- */
TEST(numsub_null_locale) {
    NEED_FACTORY(); IDWriteNumberSubstitution *n; int m;
    /* locale NULL is only legal for DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE (value 2) */
    for (m = 0; m <= 4; m++) {
        n = SENT(IDWriteNumberSubstitution *);
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, NULL, FALSE, &n);
        if (m == DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE) { CHECK_HR(hr, S_OK); CHECK(n != NULL); DW_RELEASE(n); }
        else { CHECKF(hr == E_INVALIDARG && n == NULL, "method %d -> %s out=%p", m, dw_hr_name(hr), n); if (n && n != SENT(IDWriteNumberSubstitution *)) DW_RELEASE(n); }
    }
    for (m = 0; m <= 4; m++) {
        n = NULL; CHECKF(IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, L"en-us", m & 1, &n) == S_OK, "method %d with locale", m); DW_RELEASE(n);
    }
    DONE_FACTORY();
}

/* ---- rendering params ------------------------------------------------------------------------------------------------------ */
TEST(custom_rendering_params_invalid_arguments) {
    NEED_FACTORY(); IDWriteRenderingParams *p; unsigned i;
    static const struct { float gamma, contrast, level; DWRITE_PIXEL_GEOMETRY pg; DWRITE_RENDERING_MODE rm; const char *why; } rows[] = {
        { 0.f, 0.5f, 1.f, 1, 0, "gamma 0" }, { -2.2f, 0.5f, 1.f, 1, 0, "gamma negative" }, { NAN_F, 0.5f, 1.f, 1, 0, "gamma NaN" }, { 256.5f, 0.5f, 1.f, 1, 0, "gamma 256.5" },
        { INF_F, 0.5f, 1.f, 1, 0, "gamma +inf" }, { 2.2f, -0.1f, 1.f, 1, 0, "enhanced contrast < 0" }, { 2.2f, 0.5f, -0.1f, 1, 0, "clearType level < 0" },
        { 2.2f, 0.5f, 1.f, 3, 0, "pixel geometry 3" }, { 2.2f, 0.5f, 1.f, (DWRITE_PIXEL_GEOMETRY)-1, 0, "pixel geometry -1" },
        { 2.2f, 0.5f, 1.f, 1, 7, "rendering mode 7" }, { 2.2f, 0.5f, 1.f, 1, (DWRITE_RENDERING_MODE)100, "rendering mode 100" },
        { 2.2f, 0.5f, 1.f, 1, (DWRITE_RENDERING_MODE)-1, "rendering mode -1" },
    };
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        p = SENT(IDWriteRenderingParams *);
        HRESULT hr = IDWriteFactory_CreateCustomRenderingParams(f, rows[i].gamma, rows[i].contrast, rows[i].level, rows[i].pg, rows[i].rm, &p);
        CHECKF(hr == E_INVALIDARG && p == NULL, "%s -> %s out=%p", rows[i].why, dw_hr_name(hr), p);
        if (p && p != SENT(IDWriteRenderingParams *)) DW_RELEASE(p);
    }
    DONE_FACTORY();
}
TEST(custom_rendering_params_valid_boundaries) {
    NEED_FACTORY(); IDWriteRenderingParams *p = NULL; int pg, rm;
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 0.0001f, 0.f, 0.f, 0, 0, &p), S_OK); DW_RELEASE(p);
    CHECK_HR(IDWriteFactory_CreateCustomRenderingParams(f, 256.f, 100.f, 100.f, 2, 6, &p), S_OK); DW_RELEASE(p);
    for (pg = 0; pg <= 2; pg++) for (rm = 0; rm <= 6; rm++) {
        p = NULL; CHECKF(IDWriteFactory_CreateCustomRenderingParams(f, 2.2f, 0.5f, 1.f, (DWRITE_PIXEL_GEOMETRY)pg, (DWRITE_RENDERING_MODE)rm, &p) == S_OK, "pg %d rm %d", pg, rm); DW_RELEASE(p);
    }
    DONE_FACTORY();
}
TEST(custom_rendering_params_factory1_validation) {
    NEED_FACTORY(); IDWriteFactory1 *f1 = NULL; IDWriteRenderingParams1 *p;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory1, (void **)&f1)) && f1) {
        p = SENT(IDWriteRenderingParams1 *);
        CHECK_HR(IDWriteFactory1_CreateCustomRenderingParams(f1, 0.f, 0.5f, 0.5f, 1.f, 1, 0, &p), E_INVALIDARG); CHECK_OUT_NULL(p);
        p = SENT(IDWriteRenderingParams1 *);
        CHECK_HR(IDWriteFactory1_CreateCustomRenderingParams(f1, 2.2f, 0.5f, -1.f, 1.f, 1, 0, &p), E_INVALIDARG); CHECK_OUT_NULL(p);     /* grayscale enhanced contrast < 0 */
        p = SENT(IDWriteRenderingParams1 *);
        CHECK_HR(IDWriteFactory1_CreateCustomRenderingParams(f1, 2.2f, 0.5f, 0.5f, -1.f, 1, 0, &p), E_INVALIDARG); CHECK_OUT_NULL(p);   /* clearType level < 0 */
        p = SENT(IDWriteRenderingParams1 *);
        CHECK_HR(IDWriteFactory1_CreateCustomRenderingParams(f1, 2.2f, 0.5f, 0.5f, 1.f, 3, 0, &p), E_INVALIDARG); CHECK_OUT_NULL(p);
        p = SENT(IDWriteRenderingParams1 *);
        CHECK_HR(IDWriteFactory1_CreateCustomRenderingParams(f1, 2.2f, 0.5f, 0.5f, 1.f, 1, 7, &p), E_INVALIDARG); CHECK_OUT_NULL(p);
        p = NULL; CHECK_HR(IDWriteFactory1_CreateCustomRenderingParams(f1, 2.2f, 0.5f, 0.5f, 1.f, 1, 6, &p), S_OK); DW_RELEASE(p);
        DW_RELEASE(f1);
    } else t_skip("IDWriteFactory1 not available");
    DONE_FACTORY();
}
TEST(custom_rendering_params_factory2_3_validation) {
    NEED_FACTORY(); IDWriteFactory3 *f3 = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) && f3) {
        IDWriteRenderingParams3 *p = SENT(IDWriteRenderingParams3 *);
        /* mode1 range is 0..7, grid fit 0..2 */
        CHECK_HR(IDWriteFactory3_CreateCustomRenderingParams(f3, 2.2f, .5f, .5f, 1.f, 1, (DWRITE_RENDERING_MODE1)8, DWRITE_GRID_FIT_MODE_DEFAULT, &p), E_INVALIDARG); CHECK_OUT_NULL(p);
        p = SENT(IDWriteRenderingParams3 *);
        CHECK_HR(IDWriteFactory3_CreateCustomRenderingParams(f3, 2.2f, .5f, .5f, 1.f, 1, DWRITE_RENDERING_MODE1_NATURAL, (DWRITE_GRID_FIT_MODE)3, &p), E_INVALIDARG); CHECK_OUT_NULL(p);
        p = SENT(IDWriteRenderingParams3 *);
        CHECK_HR(IDWriteFactory3_CreateCustomRenderingParams(f3, 0.f, .5f, .5f, 1.f, 1, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_GRID_FIT_MODE_ENABLED, &p), E_INVALIDARG); CHECK_OUT_NULL(p);
        p = NULL; CHECK_HR(IDWriteFactory3_CreateCustomRenderingParams(f3, 2.2f, .5f, .5f, 1.f, 1, DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC_DOWNSAMPLED, DWRITE_GRID_FIT_MODE_ENABLED, &p), S_OK); DW_RELEASE(p);
        DW_RELEASE(f3);
    } else t_skip("IDWriteFactory3 not available");
    DONE_FACTORY();
}

/* ---- CreateGlyphRunAnalysis ----------------------------------------------------------------------------------------------------- */
typedef struct { int null_run, null_face, null_idx; float em; float ppd; int nan_origin, nan_matrix; int rm, mm; const char *why; } gra_row;
static const gra_row gra_bad[] = {
    { 1, 0, 0, 16, 1, 0, 0, 0, 0, "run NULL" }, { 0, 1, 0, 16, 1, 0, 0, 0, 0, "fontFace NULL" }, { 0, 0, 1, 16, 1, 0, 0, 0, 0, "glyphIndices NULL" },
    { 0, 0, 0, -1, 1, 0, 0, 0, 0, "fontEmSize negative" }, { 0, 0, 0, 16, 0, 0, 0, 0, 0, "pixelsPerDip 0" }, { 0, 0, 0, 16, -1, 0, 0, 0, 0, "pixelsPerDip negative" },
    { 0, 0, 0, 16, 1, 1, 0, 0, 0, "origin NaN" }, { 0, 0, 0, 16, 1, 0, 1, 0, 0, "matrix NaN" }, { 0, 0, 0, 16, 1, 0, 0, 7, 0, "rendering mode 7" },
    { 0, 0, 0, 16, 1, 0, 0, 0, 3, "measuring mode 3" }, { 0, 0, 0, 16, 1, 0, 0, -1, 0, "rendering mode -1" }, { 0, 0, 0, 16, 1, 0, 0, 0, -1, "measuring mode -1" },
};
TEST(glyph_run_analysis_invalid_arguments) {
    NEED_FACTORY(); NEED_FACE(h, f); unsigned i; UINT16 idx[2] = { 2, 3 }; float adv[2] = { 8, 8 };
    for (i = 0; i < sizeof gra_bad / sizeof gra_bad[0]; i++) {
        const gra_row *r = &gra_bad[i]; DWRITE_GLYPH_RUN run; DWRITE_MATRIX nm = { 1, 0, 0, 1, NAN_F, 0 }; IDWriteGlyphRunAnalysis *a = SENT(IDWriteGlyphRunAnalysis *);
        memset(&run, 0, sizeof run); run.fontFace = r->null_face ? NULL : h.face; run.fontEmSize = r->em; run.glyphCount = 2; run.glyphIndices = r->null_idx ? NULL : idx; run.glyphAdvances = adv;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, r->null_run ? NULL : &run, r->ppd, r->nan_matrix ? &nm : NULL, (DWRITE_RENDERING_MODE)r->rm, (DWRITE_MEASURING_MODE)r->mm, r->nan_origin ? NAN_F : 0.f, 0.f, &a);
        CHECKF(hr == E_INVALIDARG && a == NULL, "%s -> %s out=%p", r->why, dw_hr_name(hr), a);
        if (a && a != SENT(IDWriteGlyphRunAnalysis *)) DW_RELEASE(a);
    }
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(glyph_run_analysis_zero_glyph_count_and_size) {
    /* fontEmSize 0 is not rejected by the argument check (only negative sizes are). */
    NEED_FACTORY(); NEED_FACE(h, f); UINT16 idx[1] = { 2 }; DWRITE_GLYPH_RUN run; IDWriteGlyphRunAnalysis *a = NULL;
    memset(&run, 0, sizeof run); run.fontFace = h.face; run.fontEmSize = 0.f; run.glyphCount = 1; run.glyphIndices = idx;
    HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &run, 1.f, NULL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL, 0.f, 0.f, &a);
    t_note("CreateGlyphRunAnalysis(fontEmSize 0) = %s", dw_hr_name(hr));
    CHECK(hr != E_INVALIDARG || a == NULL);
    DW_RELEASE(a); dwx_font_close(&h); DONE_FACTORY();
}
TEST(glyph_run_analysis_factory2_3_validation) {
    NEED_FACTORY(); NEED_FACE(h, f); IDWriteFactory3 *f3 = NULL; UINT16 idx[2] = { 2, 3 }; float adv[2] = { 8, 8 }; DWRITE_GLYPH_RUN run;
    memset(&run, 0, sizeof run); run.fontFace = h.face; run.fontEmSize = 16; run.glyphCount = 2; run.glyphIndices = idx; run.glyphAdvances = adv;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) && f3) {
        IDWriteGlyphRunAnalysis *a = SENT(IDWriteGlyphRunAnalysis *);
        /* Factory3: rendering mode1 <= 7, measuring <= 2, grid fit <= 2, antialias <= 1 */
        CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, &run, NULL, (DWRITE_RENDERING_MODE1)8, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0, &a), E_INVALIDARG); CHECK_OUT_NULL(a);
        a = SENT(IDWriteGlyphRunAnalysis *);
        CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, &run, NULL, DWRITE_RENDERING_MODE1_NATURAL, (DWRITE_MEASURING_MODE)3, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0, &a), E_INVALIDARG); CHECK_OUT_NULL(a);
        a = SENT(IDWriteGlyphRunAnalysis *);
        CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, &run, NULL, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_MEASURING_MODE_NATURAL, (DWRITE_GRID_FIT_MODE)3, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0, &a), E_INVALIDARG); CHECK_OUT_NULL(a);
        a = SENT(IDWriteGlyphRunAnalysis *);
        CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, &run, NULL, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, (DWRITE_TEXT_ANTIALIAS_MODE)2, 0, 0, &a), E_INVALIDARG); CHECK_OUT_NULL(a);
        a = SENT(IDWriteGlyphRunAnalysis *);
        if (!t_is_wine()) { CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, NULL, NULL, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 0, 0, &a), E_INVALIDARG); CHECK_OUT_NULL(a); }
        a = SENT(IDWriteGlyphRunAnalysis *);
        CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, &run, NULL, DWRITE_RENDERING_MODE1_NATURAL, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, NAN_F, 0, &a), E_INVALIDARG); CHECK_OUT_NULL(a);
        DW_RELEASE(f3);
    } else t_skip("IDWriteFactory3 not available");
    dwx_font_close(&h); DONE_FACTORY();
}

/* ---- Factory5/6 ---------------------------------------------------------------------------------------------------------------- */
TEST(factory6_fontresource_and_collection_null_arguments) {
    if (t_is_wine()) { t_skip("NULL arguments are dereferenced by the emulated implementation"); return; }
    NEED_FACTORY(); IDWriteFactory6 *f6 = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory6, (void **)&f6)) && f6) {
        IDWriteFontResource *r = SENT(IDWriteFontResource *); IDWriteFontCollection2 *c = SENT(IDWriteFontCollection2 *);
        CHECK_HR(IDWriteFactory6_CreateFontResource(f6, NULL, 0, &r), E_INVALIDARG); CHECK_OUT_NULL(r);
        CHECK_HR(IDWriteFactory6_CreateFontCollectionFromFontSet(f6, NULL, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &c), E_INVALIDARG); CHECK_OUT_NULL(c);
        DW_RELEASE(f6);
    } else t_skip("IDWriteFactory6 not available");
    DONE_FACTORY();
}
TEST(factory3_createfontcollectionfromfontset_null) {
    if (t_is_wine()) { t_skip("NULL arguments are dereferenced by the emulated implementation"); return; }
    NEED_FACTORY(); IDWriteFactory3 *f3 = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) && f3) {
        IDWriteFontCollection1 *c = SENT(IDWriteFontCollection1 *);
        CHECK_HR(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, NULL, &c), E_INVALIDARG); CHECK_OUT_NULL(c);
        DW_RELEASE(f3);
    } else t_skip("IDWriteFactory3 not available");
    DONE_FACTORY();
}
TEST(factory5_inmemory_loader_arguments) {
    if (t_is_wine()) { t_skip("NULL arguments are dereferenced by the emulated implementation"); return; }
    NEED_FACTORY(); IDWriteFactory5 *f5 = NULL; IDWriteInMemoryFontFileLoader *ld = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&f5)) && f5 && SUCCEEDED(IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &ld)) && ld) {
        size_t n; uint8_t *b = eh_blob(&n); IDWriteFontFile *file = SENT(IDWriteFontFile *);
        CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)ld), S_OK);
        CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(ld, f, NULL, (UINT32)n, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
        file = SENT(IDWriteFontFile *);
        CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(ld, f, b, 0, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
        file = NULL;
        CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(ld, f, b, (UINT32)n, NULL, &file), S_OK); CHECK(file != NULL);
        if (file) {
            IDWriteFontFace *face = NULL;
            CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, 0, &face), S_OK); DW_RELEASE(face);
        }
        DW_RELEASE(file);
        /* CreateStreamFromKey: keys are 4-byte indices into the loader's entry table */
        static const UINT32 sizes[] = { 0, 1, 2, 3, 5, 8, 16 }; unsigned i; UINT32 key = 0;
        for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
            IDWriteFontFileStream *st = SENT(IDWriteFontFileStream *);
            HRESULT hr = IDWriteFontFileLoader_CreateStreamFromKey((IDWriteFontFileLoader *)ld, &key, sizes[i], &st);
            CHECKF(hr == E_INVALIDARG && st == NULL, "key size %u -> %s", sizes[i], dw_hr_name(hr));
            if (st && st != SENT(IDWriteFontFileStream *)) DW_RELEASE(st);
        }
        for (key = 1; key <= 3; key++) {
            IDWriteFontFileStream *st = SENT(IDWriteFontFileStream *);
            HRESULT hr = IDWriteFontFileLoader_CreateStreamFromKey((IDWriteFontFileLoader *)ld, &key, 4, &st);
            CHECKF(hr == E_INVALIDARG && st == NULL, "key index %u of 1 entry -> %s", key, dw_hr_name(hr));
            if (st && st != SENT(IDWriteFontFileStream *)) DW_RELEASE(st);
        }
        key = 0xFFFFFFFFu;
        { IDWriteFontFileStream *st = SENT(IDWriteFontFileStream *); CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey((IDWriteFontFileLoader *)ld, &key, 4, &st), E_INVALIDARG); CHECK_OUT_NULL(st); }
        key = 0;
        { IDWriteFontFileStream *st = NULL; CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey((IDWriteFontFileLoader *)ld, &key, 4, &st), S_OK); DW_RELEASE(st); }
        IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)ld);
        free(b);
    } else t_skip("IDWriteFactory5 not available");
    if (ld) IDWriteInMemoryFontFileLoader_Release(ld);
    DW_RELEASE(f5); DONE_FACTORY();
}
TEST(factory5_inmemory_loader_not_registered_reference) {
    /* The loader must be registered before a reference over it can be made: same rule as any custom loader. */
    NEED_FACTORY(); IDWriteFactory5 *f5 = NULL; IDWriteInMemoryFontFileLoader *ld = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&f5)) && f5 && SUCCEEDED(IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &ld)) && ld) {
        size_t n; uint8_t *b = eh_blob(&n); IDWriteFontFile *file = SENT(IDWriteFontFile *);
        HRESULT hr = IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(ld, f, b, (UINT32)n, NULL, &file);
        t_note("CreateInMemoryFontFileReference on an unregistered loader = %s", dw_hr_name(hr));
        if (FAILED(hr)) CHECK_OUT_NULL(file); else DW_RELEASE(file);
        free(b);
    } else t_skip("IDWriteFactory5 not available");
    if (ld) IDWriteInMemoryFontFileLoader_Release(ld);
    DW_RELEASE(f5); DONE_FACTORY();
}
TEST(factory3_fontdownloadqueue_listener_arguments) {
    NEED_FACTORY(); IDWriteFactory3 *f3 = NULL; IDWriteFontDownloadQueue *q = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) && f3 && SUCCEEDED(IDWriteFactory3_GetFontDownloadQueue(f3, &q)) && q) {
        UINT32 tok = 77; dwx_listener *l = dwx_listener_new();
        CHECK_HR(IDWriteFontDownloadQueue_AddListener(q, NULL, &tok), E_INVALIDARG); EXPECT_EQ(tok, 0);
        tok = 0;
        CHECK_HR(IDWriteFontDownloadQueue_AddListener(q, (IDWriteFontDownloadListener *)l, &tok), S_OK);
        t_note("listener token = %u", tok);
        CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(q, tok), S_OK);
        CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(q, tok), E_INVALIDARG);          /* RemoveListener reports "not found" */
        CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(q, 0xDEADBEEF), E_INVALIDARG);
        CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(q, 0), E_INVALIDARG);
        CHECK(IDWriteFontDownloadQueue_IsEmpty(q));
        dwx_release(l);
    } else t_skip("font download queue not available");
    DW_RELEASE(q); DW_RELEASE(f3); DONE_FACTORY();
}
