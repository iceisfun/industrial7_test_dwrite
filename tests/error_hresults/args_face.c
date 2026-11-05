/* Argument validation and buffer protocols for font faces, localized strings, the text analyzer and the analysis callbacks. */
#include "eh.h"

/* ---- IDWriteLocalizedStrings -------------------------------------------------------------------------------------------------- */
static IDWriteLocalizedStrings *face_names(IDWriteFontFace *face) {
    IDWriteFontFace3 *f3 = NULL; IDWriteLocalizedStrings *s = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) { IDWriteFontFace3_GetFamilyNames(f3, &s); IDWriteFontFace3_Release(f3); }
    return s;
}
#define NEED_STRINGS(s) NEED_FACTORY(); NEED_FACE(h, f); IDWriteLocalizedStrings *s = face_names(h.face); \
    if (!s || IDWriteLocalizedStrings_GetCount(s) == 0) { DW_RELEASE(s); dwx_font_close(&h); DONE_FACTORY(); t_skip("no family names on the face"); return; }
#define END_STRINGS(s) DW_RELEASE(s); dwx_font_close(&h); DONE_FACTORY()

TEST(strings_getstring_index_out_of_range_is_efail) {
    NEED_STRINGS(s); UINT32 n = IDWriteLocalizedStrings_GetCount(s); WCHAR buf[64]; UINT32 len = 55; unsigned i;
    static const UINT32 bad[] = { 0, 1, 2, 100, 0x7fffffff, 0xFFFFFFFFu };
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        UINT32 idx = n + bad[i]; if (idx < n) idx = 0xFFFFFFFFu;
        /* IndexOutOfRangeException carries E_FAIL; the destination is cleared before the index is looked at */
        buf[0] = 0x7777; CHECKF(IDWriteLocalizedStrings_GetString(s, idx, buf, 64) == E_FAIL, "GetString(%u)", idx); EXPECT_EQ(buf[0], 0);
        buf[0] = 0x7777; CHECKF(IDWriteLocalizedStrings_GetLocaleName(s, idx, buf, 64) == E_FAIL, "GetLocaleName(%u)", idx); EXPECT_EQ(buf[0], 0);
        len = 55; CHECKF(IDWriteLocalizedStrings_GetStringLength(s, idx, &len) == E_FAIL, "GetStringLength(%u)", idx); EXPECT_EQ(len, 0xFFFFFFFFu);
        len = 55; CHECKF(IDWriteLocalizedStrings_GetLocaleNameLength(s, idx, &len) == E_FAIL, "GetLocaleNameLength(%u)", idx); EXPECT_EQ(len, 0xFFFFFFFFu);
    }
    END_STRINGS(s);
}
TEST(strings_zero_size_buffer_is_insufficient_buffer) {
    NEED_STRINGS(s); WCHAR buf[4] = { 0x7777 };
    /* size 0 is answered before the destination or the index is touched */
    CHECK_HR(IDWriteLocalizedStrings_GetString(s, 0, buf, 0), EH_NSB); EXPECT_EQ(buf[0], 0x7777);
    CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, 0, buf, 0), EH_NSB); EXPECT_EQ(buf[0], 0x7777);
    CHECK_HR(IDWriteLocalizedStrings_GetString(s, 0, NULL, 0), EH_NSB);
    CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, 0, NULL, 0), EH_NSB);
    CHECK_HR(IDWriteLocalizedStrings_GetString(s, 0xFFFFFFFFu, buf, 0), EH_NSB);
    CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, 0xFFFFFFFFu, NULL, 0), EH_NSB);
    END_STRINGS(s);
}
TEST(strings_string_buffer_sizes) {
    NEED_STRINGS(s); UINT32 n = IDWriteLocalizedStrings_GetCount(s), i, len, k;
    for (i = 0; i < n; i++) {
        WCHAR buf[256], full[256]; unsigned j;
        CHECK_HR(IDWriteLocalizedStrings_GetStringLength(s, i, &len), S_OK); CHECK(len > 0 && len < 200);
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, i, full, len + 1), S_OK); EXPECT_EQ(lstrlenW(full), len);
        for (k = 1; k <= len; k++) {              /* every smaller capacity: NSB, truncated + terminated copy */
            for (j = 0; j < 256; j++) buf[j] = 0x7777;
            CHECKF(IDWriteLocalizedStrings_GetString(s, i, buf, k) == EH_NSB, "string %u capacity %u", i, k);
            CHECKF(buf[k - 1] == 0 && !memcmp(buf, full, (k - 1) * sizeof(WCHAR)), "string %u capacity %u: truncation wrong", i, k);
            CHECK(buf[k] == 0x7777);
        }
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, i, buf, 256), S_OK); EXPECT_WSTR(buf, full);
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleNameLength(s, i, &len), S_OK);
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, i, full, len + 1), S_OK); EXPECT_EQ(lstrlenW(full), len);
        for (k = 1; k <= len; k++) { for (j = 0; j < 256; j++) buf[j] = 0x7777; CHECKF(IDWriteLocalizedStrings_GetLocaleName(s, i, buf, k) == EH_NSB, "locale %u capacity %u", i, k); CHECK(buf[k - 1] == 0); }
    }
    END_STRINGS(s);
}
TEST(strings_findlocalename_arguments) {
    NEED_STRINGS(s); UINT32 idx = 55; BOOL ex = 7;
    if (!t_is_wine()) { CHECK_HR(IDWriteLocalizedStrings_FindLocaleName(s, NULL, &idx, &ex), E_INVALIDARG); EXPECT_EQ(idx, 0xFFFFFFFFu); EXPECT_EQ(ex, 0); }
    idx = 55; ex = 7;
    { /* a malformed locale name is rejected by the OS; a well-formed absent one is a miss, not an error */
        HRESULT hr = IDWriteLocalizedStrings_FindLocaleName(s, L"xx-nonexistent-locale", &idx, &ex);
        t_note("FindLocaleName(malformed absent locale) = %s", dw_hr_name(hr)); CHECK(hr == S_OK || hr == E_INVALIDARG);
        idx = 55; ex = 7; hr = IDWriteLocalizedStrings_FindLocaleName(s, L"zz-ZZ", &idx, &ex);
        t_note("FindLocaleName(zz-ZZ) = %s", dw_hr_name(hr)); if (hr == S_OK) { EXPECT_EQ(idx, 0xFFFFFFFFu); EXPECT_EQ(ex, 0); }
    }
    END_STRINGS(s);
}

/* ---- IDWriteFontFace ----------------------------------------------------------------------------------------------------------- */
TEST(face_getfiles_protocol) {
    NEED_FACTORY(); NEED_FACE(h, f); UINT32 n = 55; IDWriteFontFile *file = SENT(IDWriteFontFile *);
    if (!t_is_wine()) CHECK_HR(IDWriteFontFace_GetFiles(h.face, NULL, &file), E_INVALIDARG);
    n = 55; CHECK_HR(IDWriteFontFace_GetFiles(h.face, &n, NULL), S_OK); EXPECT_EQ(n, 1);
    file = SENT(IDWriteFontFile *);
    { UINT32 two = 2; CHECK_HR(IDWriteFontFace_GetFiles(h.face, &two, &file), E_INVALIDARG); EXPECT_EQ(two, 2); }
    { UINT32 zero = 0; CHECK_HR(IDWriteFontFace_GetFiles(h.face, &zero, &file), E_INVALIDARG); }
    { UINT32 big = 0xFFFFFFFFu; CHECK_HR(IDWriteFontFace_GetFiles(h.face, &big, &file), E_INVALIDARG); }
    file = NULL; n = 1; CHECK_HR(IDWriteFontFace_GetFiles(h.face, &n, &file), S_OK); CHECK(file != NULL); DW_RELEASE(file);
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(face_design_glyph_metrics_arguments) {
    NEED_FACTORY(); NEED_FACE(h, f); DWRITE_GLYPH_METRICS m[3]; UINT16 idx[3] = { 2, 3, 4 }; size_t i;
    memset(m, 0xAA, sizeof m);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(h.face, NULL, 3, m, FALSE), E_INVALIDARG);
    for (i = 0; i < sizeof m; i++) if (((uint8_t *)m)[i]) { CHECKF(0, "metrics byte %zu not cleared before the argument check", i); break; }
    if (!t_is_wine()) CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(h.face, NULL, 0, m, FALSE), E_INVALIDARG);       /* NULL index array fails even for count 0 */
    memset(m, 0xAA, sizeof m);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(h.face, idx, 3, m, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(h.face, idx, 0, m, FALSE), S_OK);
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(face1_design_glyph_advances_arguments) {
    if (t_is_wine()) { t_skip("invalid arguments fault in the emulated implementation"); return; }
    NEED_FACTORY(); NEED_FACE(h, f); IDWriteFontFace1 *f1 = NULL; INT32 adv[3]; UINT16 idx[3] = { 2, 3, 4 };
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        memset(adv, 0x55, sizeof adv);
        CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 3, NULL, adv, FALSE), E_INVALIDARG);
        EXPECT_EQ(adv[0], 0); EXPECT_EQ(adv[2], 0);                        /* the output array is zeroed first */
        CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 0, NULL, adv, FALSE), S_OK);
        CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 3, idx, adv, FALSE), S_OK); EXPECT_EQ(adv[0], 1000);
        DW_RELEASE(f1);
    } else t_skip("IDWriteFontFace1 not available");
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(face_gdi_compatible_metrics_arguments) {
    NEED_FACTORY(); NEED_FACE(h, f); DWRITE_FONT_METRICS fm; DWRITE_MATRIX nanm = { 1, 0, 0, 1, NAN_F, 0 }, ok = { 1, 0, 0, 1, 0, 0 }; unsigned i;
    static const struct { float em, ppd; int nan; const char *why; } bad[] = {
        { 0.f, 1.f, 0, "emSize 0" }, { -1.f, 1.f, 0, "emSize negative" }, { NAN_F, 1.f, 0, "emSize NaN" }, { 16.f, 0.f, 0, "pixelsPerDip 0" },
        { 16.f, -1.f, 0, "pixelsPerDip negative" }, { 16.f, NAN_F, 0, "pixelsPerDip NaN" }, { 16.f, 1.f, 1, "transform NaN" },
    };
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        memset(&fm, 0xAA, sizeof fm);
        HRESULT hr = IDWriteFontFace_GetGdiCompatibleMetrics(h.face, bad[i].em, bad[i].ppd, bad[i].nan ? &nanm : NULL, &fm);
        CHECKF(hr == E_INVALIDARG, "%s -> %s", bad[i].why, dw_hr_name(hr));
        CHECKF(fm.designUnitsPerEm == 0 && fm.ascent == 0, "%s: metrics not cleared", bad[i].why);
    }
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleMetrics(h.face, 16.f, 1.f, &ok, &fm), S_OK);
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleMetrics(h.face, 16.f, 1.f, NULL, &fm), S_OK);
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(face_glyph_run_outline_arguments) {
    NEED_FACTORY(); NEED_FACE(h, f); dwx_geom *g = dwx_geom_new(); UINT16 idx[2] = { 2, 3 }; float adv[2] = { 8, 8 };
    IDWriteGeometrySink *sink = (IDWriteGeometrySink *)g;
    CHECK_HR(IDWriteFontFace_GetGlyphRunOutline(h.face, 16.f, idx, adv, NULL, 2, FALSE, FALSE, NULL), E_INVALIDARG);                  /* sink */
    CHECK_HR(IDWriteFontFace_GetGlyphRunOutline(h.face, 16.f, NULL, adv, NULL, 2, FALSE, FALSE, sink), E_INVALIDARG);                  /* indices with count > 0 */
    static const float em[] = { 0.f, -1.f, NAN_F, -INF_F };
    unsigned i;
    for (i = 0; i < 4; i++) { HRESULT hr = IDWriteFontFace_GetGlyphRunOutline(h.face, em[i], idx, adv, NULL, 2, FALSE, FALSE, sink); CHECKF(hr == E_INVALIDARG, "em %g -> %s", em[i], dw_hr_name(hr)); }
    EXPECT_EQ(g->nev, 0);                                           /* nothing is emitted for a rejected call */
    CHECK_HR(IDWriteFontFace_GetGlyphRunOutline(h.face, 16.f, NULL, NULL, NULL, 0, FALSE, FALSE, sink), S_OK);   /* empty run needs no indices */
    CHECK_HR(IDWriteFontFace_GetGlyphRunOutline(h.face, 16.f, idx, NULL, NULL, 2, FALSE, FALSE, sink), S_OK);
    dwx_release(g); dwx_font_close(&h); DONE_FACTORY();
}
TEST(face_tryget_font_table_missing_table) {
    NEED_FACTORY(); NEED_FACE(h, f); const void *data = SENT(const void *); UINT32 size = 55; void *ctx = SENT(void *); BOOL exists = 7;
    static const UINT32 tags[] = { DW_TAG('z', 'z', 'z', 'z'), DW_TAG('G', 'L', 'Y', 'F') + 1, DW_TAG('S', 'V', 'G', ' '), 0, 0xFFFFFFFFu };
    unsigned i;
    for (i = 0; i < sizeof tags / sizeof tags[0]; i++) {
        data = SENT(const void *); size = 55; ctx = SENT(void *); exists = 7;
        HRESULT hr = IDWriteFontFace_TryGetFontTable(h.face, tags[i], &data, &size, &ctx, &exists);
        CHECKF(hr == S_OK && !exists && data == NULL && size == 0 && ctx == NULL, "tag 0x%08x: hr=%s exists=%d size=%u", tags[i], dw_hr_name(hr), exists, size);
    }
    data = NULL; exists = 0;
    CHECK_HR(IDWriteFontFace_TryGetFontTable(h.face, DW_TAG('h', 'e', 'a', 'd'), &data, &size, &ctx, &exists), S_OK); CHECK(exists); CHECK(data != NULL); EXPECT_EQ(size, 54);
    IDWriteFontFace_ReleaseFontTable(h.face, ctx);
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(face_recommended_rendering_mode_arguments) {
    NEED_FACTORY(); NEED_FACE(h, f); IDWriteRenderingParams *p = NULL; DWRITE_RENDERING_MODE rm = (DWRITE_RENDERING_MODE)9;
    CHECK_HR(IDWriteFactory_CreateRenderingParams(f, &p), S_OK);
    if (p) {
        /* Face (v0): the rendering params object is mandatory */
        if (!t_is_wine()) { CHECK_HR(IDWriteFontFace_GetRecommendedRenderingMode(h.face, 16.f, 1.f, DWRITE_MEASURING_MODE_NATURAL, NULL, &rm), E_INVALIDARG); EXPECT_EQ(rm, 0); }
        rm = (DWRITE_RENDERING_MODE)9; CHECK_HR(IDWriteFontFace_GetRecommendedRenderingMode(h.face, 16.f, 1.f, DWRITE_MEASURING_MODE_NATURAL, p, &rm), S_OK);
        IDWriteFontFace2 *f2 = NULL; IDWriteFontFace3 *f3 = NULL;
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace2, (void **)&f2)) && f2) {
            DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)9; rm = (DWRITE_RENDERING_MODE)9;
            /* outline threshold <= 1, measuring mode <= 2; both outputs are zeroed first */
            CHECK_HR(IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.f, 96.f, 96.f, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)2, DWRITE_MEASURING_MODE_NATURAL, p, &rm, &gf), E_INVALIDARG);
            EXPECT_EQ(rm, 0); EXPECT_EQ(gf, 0);
            rm = (DWRITE_RENDERING_MODE)9; gf = (DWRITE_GRID_FIT_MODE)9;
            CHECK_HR(IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.f, 96.f, 96.f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)3, p, &rm, &gf), E_INVALIDARG);
            EXPECT_EQ(rm, 0); EXPECT_EQ(gf, 0);
            CHECK_HR(IDWriteFontFace2_GetRecommendedRenderingMode(f2, 16.f, 96.f, 96.f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, p, &rm, &gf), S_OK);
            DW_RELEASE(f2);
        }
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
            DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)9; DWRITE_RENDERING_MODE1 rm1 = (DWRITE_RENDERING_MODE1)9;
            CHECK_HR(IDWriteFontFace3_GetRecommendedRenderingMode(f3, 16.f, 96.f, 96.f, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)2, DWRITE_MEASURING_MODE_NATURAL, p, &rm1, &gf), E_INVALIDARG);
            EXPECT_EQ(rm1, 0); EXPECT_EQ(gf, 0);
            rm1 = (DWRITE_RENDERING_MODE1)9; gf = (DWRITE_GRID_FIT_MODE)9;
            CHECK_HR(IDWriteFontFace3_GetRecommendedRenderingMode(f3, 16.f, 96.f, 96.f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)7, p, &rm1, &gf), E_INVALIDARG);
            DW_RELEASE(f3);
        }
        DW_RELEASE(p);
    }
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(face3_family_model_range) {
    NEED_FACTORY(); NEED_FACE(h, f); IDWriteFontFace3 *f3 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
        IDWriteFontFace5 *f5 = NULL; (void)f5;
        DW_RELEASE(f3);
    }
    IDWriteFontFace4 *f4 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace4, (void **)&f4)) && f4) DW_RELEASE(f4);
    IDWriteFontFace6 *f6 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace6, (void **)&f6)) && f6) {
        IDWriteLocalizedStrings *s = SENT(IDWriteLocalizedStrings *);
        /* DWRITE_FONT_FAMILY_MODEL has two values; others are rejected and the out pointer is cleared */
        CHECK_HR(IDWriteFontFace6_GetFamilyNames(f6, (DWRITE_FONT_FAMILY_MODEL)2, &s), E_INVALIDARG); CHECK_OUT_NULL(s);
        s = SENT(IDWriteLocalizedStrings *);
        CHECK_HR(IDWriteFontFace6_GetFaceNames(f6, (DWRITE_FONT_FAMILY_MODEL)7, &s), E_INVALIDARG); CHECK_OUT_NULL(s);
        s = NULL; CHECK_HR(IDWriteFontFace6_GetFamilyNames(f6, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &s), S_OK); DW_RELEASE(s);
        s = NULL; CHECK_HR(IDWriteFontFace6_GetFaceNames(f6, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &s), S_OK); DW_RELEASE(s);
        DW_RELEASE(f6);
    }
    dwx_font_close(&h); DONE_FACTORY();
}

/* ---- IDWriteTextAnalyzer ------------------------------------------------------------------------------------------------------- */
#define NEED_ANALYZER(a) NEED_FACTORY(); IDWriteTextAnalyzer *a = NULL; \
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(f, &a)) || !a) { DONE_FACTORY(); t_skip("no text analyzer"); return; }
#define END_ANALYZER(a) DW_RELEASE(a); DONE_FACTORY()
#define NEED_A1(a, a1) IDWriteTextAnalyzer1 *a1 = NULL; \
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&a1)) || !a1) { END_ANALYZER(a); t_skip("IDWriteTextAnalyzer1 not available"); return; }
typedef HRESULT (*ana_call)(IDWriteTextAnalyzer *a, IDWriteTextAnalysisSource *src, UINT32 pos, UINT32 len, void *sink);
static HRESULT c_script(IDWriteTextAnalyzer *a, IDWriteTextAnalysisSource *s, UINT32 p, UINT32 l, void *k) { return IDWriteTextAnalyzer_AnalyzeScript(a, s, p, l, (IDWriteTextAnalysisSink *)k); }
static HRESULT c_bidi(IDWriteTextAnalyzer *a, IDWriteTextAnalysisSource *s, UINT32 p, UINT32 l, void *k) { return IDWriteTextAnalyzer_AnalyzeBidi(a, s, p, l, (IDWriteTextAnalysisSink *)k); }
static HRESULT c_brk(IDWriteTextAnalyzer *a, IDWriteTextAnalysisSource *s, UINT32 p, UINT32 l, void *k) { return IDWriteTextAnalyzer_AnalyzeLineBreakpoints(a, s, p, l, (IDWriteTextAnalysisSink *)k); }
static HRESULT c_num(IDWriteTextAnalyzer *a, IDWriteTextAnalysisSource *s, UINT32 p, UINT32 l, void *k) { return IDWriteTextAnalyzer_AnalyzeNumberSubstitution(a, s, p, l, (IDWriteTextAnalysisSink *)k); }
static HRESULT c_vert(IDWriteTextAnalyzer *a, IDWriteTextAnalysisSource *s, UINT32 p, UINT32 l, void *k) {
    IDWriteTextAnalyzer2 *a2 = NULL; HRESULT hr = IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer2, (void **)&a2);
    if (FAILED(hr)) return E_NOINTERFACE;
    hr = IDWriteTextAnalyzer2_AnalyzeVerticalGlyphOrientation(a2, (IDWriteTextAnalysisSource1 *)s, p, l, (IDWriteTextAnalysisSink1 *)k); IDWriteTextAnalyzer2_Release(a2); return hr;
}
static const struct { const char *name; ana_call fn; } ana_calls[] = { { "AnalyzeScript", c_script }, { "AnalyzeBidi", c_bidi }, { "AnalyzeLineBreakpoints", c_brk }, { "AnalyzeNumberSubstitution", c_num }, { "AnalyzeVerticalGlyphOrientation", c_vert } };
#define NANA 5
TEST(analyzer_null_source_sink_and_range_overflow) {
    if (t_is_wine()) { t_skip("invalid arguments fault in the emulated implementation"); return; }
    NEED_ANALYZER(a); static const WCHAR txt[] = L"Hello"; dwx_ana_src *src = dwx_ana_src_new(txt, 5); dwx_ana_sink *snk = dwx_ana_sink_new(); int k;
    src->support_v1 = 1; snk->support_v1 = 1;
    for (k = 0; k < NANA; k++) {
        const char *n = ana_calls[k].name; IDWriteTextAnalysisSource *s = (IDWriteTextAnalysisSource *)src;
        HRESULT probe = ana_calls[k].fn(a, s, 0, 0, snk);
        if (probe == E_NOINTERFACE) continue;
        if (!t_is_wine()) {
            CHECKF(ana_calls[k].fn(a, NULL, 0, 5, snk) == E_INVALIDARG, "%s: NULL source", n);
            CHECKF(ana_calls[k].fn(a, s, 0, 5, NULL) == E_INVALIDARG, "%s: NULL sink", n);
            CHECKF(ana_calls[k].fn(a, NULL, 0, 5, NULL) == E_INVALIDARG, "%s: NULL both", n);
        }
        CHECKF(ana_calls[k].fn(a, s, 0xFFFFFFFFu, 1, snk) == E_INVALIDARG, "%s: position+length wraps", n);
        CHECKF(ana_calls[k].fn(a, s, 3, 0xFFFFFFFFu, snk) == E_INVALIDARG, "%s: length -1 from 3", n);
        CHECKF(ana_calls[k].fn(a, s, 0x80000000u, 0x80000000u, snk) == E_INVALIDARG, "%s: wraps to 0", n);
        CHECKF(ana_calls[k].fn(a, s, 0, 5, snk) == S_OK, "%s: valid", n);
    }
    dwx_release(src); dwx_release(snk); END_ANALYZER(a);
}
TEST(analyzer_rejected_calls_make_no_callbacks) {
    if (t_is_wine()) { t_skip("invalid arguments fault in the emulated implementation"); return; }
    NEED_ANALYZER(a); static const WCHAR txt[] = L"Hello"; dwx_ana_src *src = dwx_ana_src_new(txt, 5); dwx_ana_sink *snk = dwx_ana_sink_new(); int k;
    src->support_v1 = 1; snk->support_v1 = 1;
    for (k = 0; k < 4; k++) {
        ana_calls[k].fn(a, (IDWriteTextAnalysisSource *)src, 0xFFFFFFFFu, 2, snk);
    }
    EXPECT_EQ(src->n_at + src->n_before + src->n_dir + src->n_locale + src->n_subst, 0);
    EXPECT_EQ(snk->n_script + snk->n_break + snk->n_bidi + snk->n_subst, 0);
    dwx_release(src); dwx_release(snk); END_ANALYZER(a);
}
TEST(analyzer_sink_failure_passes_through) {
    NEED_ANALYZER(a); static const WCHAR txt[] = L"Hello world"; int i;
    static const HRESULT codes[] = { E_FAIL, EH_CUSTOM, EH_FILEACCESS, E_ACCESSDENIED, (HRESULT)0x87654321 };
    for (i = 0; i < 5; i++) {
        dwx_ana_src *src = dwx_ana_src_new(txt, 11); dwx_ana_sink *snk = dwx_ana_sink_new();
        snk->fail_script = codes[i];
        /* AnalyzeScript routes every sink result through CallbackException::ThrowIfFailed */
        HRESULT hr = IDWriteTextAnalyzer_AnalyzeScript(a, (IDWriteTextAnalysisSource *)src, 0, 11, (IDWriteTextAnalysisSink *)snk);
        CHECKF(hr == codes[i], "script sink injected %s got %s", dw_hr_name(codes[i]), dw_hr_name(hr));
        dwx_release(src); dwx_release(snk);
        src = dwx_ana_src_new(txt, 11); snk = dwx_ana_sink_new(); snk->fail_break = codes[i];
        hr = IDWriteTextAnalyzer_AnalyzeLineBreakpoints(a, (IDWriteTextAnalysisSource *)src, 0, 11, (IDWriteTextAnalysisSink *)snk);
        t_note("breakpoint sink injected %s -> %s", dw_hr_name(codes[i]), dw_hr_name(hr));
        CHECKF(hr == S_OK || hr == codes[i], "breakpoint sink: %s", dw_hr_name(hr));
        dwx_release(src); dwx_release(snk);
        src = dwx_ana_src_new(txt, 11); snk = dwx_ana_sink_new(); snk->fail_bidi = codes[i];
        hr = IDWriteTextAnalyzer_AnalyzeBidi(a, (IDWriteTextAnalysisSource *)src, 0, 11, (IDWriteTextAnalysisSink *)snk);
        t_note("bidi sink injected %s -> %s", dw_hr_name(codes[i]), dw_hr_name(hr));
        CHECKF(hr == S_OK || hr == codes[i], "bidi sink: %s", dw_hr_name(hr));
        dwx_release(src); dwx_release(snk);
    }
    END_ANALYZER(a);
}
TEST(analyzer_source_failure_passes_through) {
    NEED_ANALYZER(a); static const WCHAR txt[] = L"Hello world"; int i;
    static const HRESULT codes[] = { E_FAIL, EH_CUSTOM, E_NOTIMPL };
    for (i = 0; i < 3; i++) {
        dwx_ana_src *src = dwx_ana_src_new(txt, 11); dwx_ana_sink *snk = dwx_ana_sink_new();
        src->fail_locale = codes[i];
        HRESULT hr = IDWriteTextAnalyzer_AnalyzeScript(a, (IDWriteTextAnalysisSource *)src, 0, 11, (IDWriteTextAnalysisSink *)snk);
        t_note("source GetLocaleName injected %s -> %s (locale callbacks=%ld)", dw_hr_name(codes[i]), dw_hr_name(hr), src->n_locale);
        CHECKF(hr == S_OK || hr == codes[i], "locale: %s", dw_hr_name(hr));
        dwx_release(src); dwx_release(snk);
        src = dwx_ana_src_new(txt, 11); snk = dwx_ana_sink_new();
        src->fail_subst = codes[i];
        hr = IDWriteTextAnalyzer_AnalyzeNumberSubstitution(a, (IDWriteTextAnalysisSource *)src, 0, 11, (IDWriteTextAnalysisSink *)snk);
        t_note("source GetNumberSubstitution injected %s -> %s", dw_hr_name(codes[i]), dw_hr_name(hr));
        CHECKF(hr == S_OK || hr == codes[i], "subst: %s", dw_hr_name(hr));
        dwx_release(src); dwx_release(snk);
    }
    END_ANALYZER(a);
}
TEST(analyzer_gettextcomplexity_arguments) {
    if (t_is_wine()) { t_skip("invalid arguments fault in the emulated implementation"); return; }
    NEED_ANALYZER(a); NEED_A1(a, a1); NEED_FACE(h, f); BOOL simple = 7; UINT32 len = 55; UINT16 idx[8];
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(a1, L"abc", 3, NULL, &simple, &len, idx), E_INVALIDARG); EXPECT_EQ(simple, 0); EXPECT_EQ(len, 0);
    simple = 7; len = 55;
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(a1, NULL, 3, h.face, &simple, &len, idx), E_INVALIDARG); EXPECT_EQ(simple, 0); EXPECT_EQ(len, 0);
    simple = 7; len = 55;
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(a1, NULL, 0, h.face, &simple, &len, idx), S_OK);
    simple = 7; len = 55;
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(a1, L"abc", 3, h.face, &simple, &len, idx), S_OK); EXPECT_EQ(simple, 1); EXPECT_EQ(len, 3);
    dwx_font_close(&h); DW_RELEASE(a1); END_ANALYZER(a);
}
TEST(analyzer_getglyphs_arguments) {
    if (t_is_wine()) { t_skip("invalid arguments fault in the emulated implementation"); return; }
    NEED_ANALYZER(a); NEED_FACE(h, f); DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT }; UINT16 cm[16], gi[32]; DWRITE_SHAPING_TEXT_PROPERTIES tp[16]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[32]; UINT32 actual = 55;
    CHECK_HR(IDWriteTextAnalyzer_GetGlyphs(a, L"Hello", 5, NULL, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 32, cm, tp, gi, gp, &actual), E_INVALIDARG); EXPECT_EQ(actual, 0);
    actual = 55; CHECK_HR(IDWriteTextAnalyzer_GetGlyphs(a, L"Hello", 0, h.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 32, cm, tp, gi, gp, &actual), S_OK);    /* empty text */
    actual = 55; CHECK_HR(IDWriteTextAnalyzer_GetGlyphs(a, L"Hello", 5, h.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 0, cm, tp, gi, gp, &actual), EH_NSB);     /* maxGlyphCount 0 */
    actual = 55; CHECK_HR(IDWriteTextAnalyzer_GetGlyphs(a, L"Hello", 5, h.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 32, cm, tp, gi, gp, &actual), S_OK); EXPECT_EQ(actual, 5);
    actual = 55; { HRESULT hr = IDWriteTextAnalyzer_GetGlyphs(a, L"Hello", 5, h.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 2, cm, tp, gi, gp, &actual);
                   t_note("GetGlyphs maxGlyphCount 2 for 5 characters -> %s actual=%u", dw_hr_name(hr), actual); CHECK_HR(hr, EH_NSB); }
    dwx_font_close(&h); END_ANALYZER(a);
}
TEST(analyzer_getglyphplacements_arguments) {
    if (t_is_wine()) { t_skip("invalid arguments fault in the emulated implementation"); return; }
    NEED_ANALYZER(a); NEED_FACE(h, f); DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT }; UINT16 cm[8] = { 0, 1, 2, 3, 4 }, gi[8] = { 2, 3, 4, 5, 6 };
    DWRITE_SHAPING_TEXT_PROPERTIES tp[8]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[8]; float adv[8]; DWRITE_GLYPH_OFFSET off[8]; memset(tp, 0, sizeof tp); memset(gp, 0, sizeof gp);
    CHECK_HR(IDWriteTextAnalyzer_GetGlyphPlacements(a, L"Hello", cm, tp, 5, gi, gp, 5, NULL, 16.f, FALSE, FALSE, &sa, L"en-us", NULL, NULL, 0, adv, off), E_INVALIDARG);   /* fontFace */
    CHECK_HR(IDWriteTextAnalyzer_GetGlyphPlacements(a, L"Hello", cm, tp, 5, gi, gp, 0, h.face, 16.f, FALSE, FALSE, &sa, L"en-us", NULL, NULL, 0, adv, off), S_OK);       /* no glyphs */
    CHECK_HR(IDWriteTextAnalyzer_GetGlyphPlacements(a, L"Hello", cm, tp, 5, gi, gp, 5, h.face, 16.f, FALSE, FALSE, &sa, L"en-us", NULL, NULL, 0, adv, off), S_OK);
    dwx_font_close(&h); END_ANALYZER(a);
}
TEST(analyzer_getbaseline_arguments) {
    NEED_ANALYZER(a); NEED_A1(a, a1); NEED_FACE(h, f); DWRITE_SCRIPT_ANALYSIS sa = { 0, 0 }; INT32 b = 55; BOOL ex = 7; int bl;
    CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, NULL, DWRITE_BASELINE_ROMAN, FALSE, FALSE, sa, L"en-us", &b, &ex), E_INVALIDARG); EXPECT_EQ(b, 0); EXPECT_EQ(ex, 0);
    for (bl = 9; bl <= 12; bl++) { b = 55; ex = 7; CHECKF(IDWriteTextAnalyzer1_GetBaseline(a1, h.face, (DWRITE_BASELINE)bl, FALSE, FALSE, sa, L"en-us", &b, &ex) == E_INVALIDARG, "baseline %d", bl); EXPECT_EQ(b, 0); EXPECT_EQ(ex, 0); }
    b = 55; ex = 7; CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, h.face, (DWRITE_BASELINE)-1, FALSE, FALSE, sa, L"en-us", &b, &ex), E_INVALIDARG);
    for (bl = 0; bl <= 8; bl++) { b = 55; CHECKF(IDWriteTextAnalyzer1_GetBaseline(a1, h.face, (DWRITE_BASELINE)bl, FALSE, FALSE, sa, L"en-us", &b, &ex) == S_OK, "baseline %d", bl); }
    dwx_font_close(&h); DW_RELEASE(a1); END_ANALYZER(a);
}
TEST(analyzer_character_spacing_arguments) {
    if (t_is_wine()) { t_skip("invalid arguments fault in the emulated implementation"); return; }
    NEED_ANALYZER(a); NEED_A1(a, a1); UINT16 cm[3] = { 0, 1, 2 }; float adv[3] = { 8, 8, 8 }, outa[3]; DWRITE_GLYPH_OFFSET off[3], outo[3]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3]; memset(off, 0, sizeof off); memset(gp, 0, sizeof gp);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, 1.f, 0.f, 3, 3, cm, adv, off, gp, outa, outo), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, NAN_F, 1.f, 0.f, 3, 3, cm, adv, off, gp, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, NAN_F, 0.f, 3, 3, cm, adv, off, gp, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, 1.f, -1.f, 3, 3, cm, adv, off, gp, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, 1.f, 0.f, 3, 3, NULL, adv, off, gp, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, 1.f, 0.f, 3, 3, cm, NULL, off, gp, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, 1.f, 0.f, 3, 3, cm, adv, NULL, gp, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, 1.f, 0.f, 3, 3, cm, adv, off, NULL, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, 1.f, 0.f, 0, 3, cm, adv, off, gp, outa, outo), E_INVALIDARG);          /* textLength 0 with glyphs */
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.f, 1.f, 0.f, 3, 0, NULL, NULL, NULL, NULL, outa, outo), S_OK);             /* no glyphs: nothing else is read */
    DW_RELEASE(a1); END_ANALYZER(a);
}
TEST(analyzer_justify_advances_arguments) {
    NEED_ANALYZER(a); NEED_A1(a, a1); DWRITE_JUSTIFICATION_OPPORTUNITY jo[3]; float adv[3] = { 8, 8, 8 }, outa[3]; DWRITE_GLYPH_OFFSET off[3], outo[3]; memset(jo, 0, sizeof jo); memset(off, 0, sizeof off);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 100.f, 3, jo, adv, off, outa, outo), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, -1.f, 3, jo, adv, off, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 100.f, 3, NULL, adv, off, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 100.f, 3, jo, NULL, off, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 100.f, 3, jo, adv, NULL, outa, outo), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 100.f, 0, NULL, NULL, NULL, NULL, NULL), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, -1.f, 0, NULL, NULL, NULL, NULL, NULL), E_INVALIDARG);
    DW_RELEASE(a1); END_ANALYZER(a);
}
TEST(analyzer_glyph_orientation_transform_arguments) {
    NEED_ANALYZER(a); NEED_A1(a, a1); IDWriteTextAnalyzer2 *a2 = NULL; DWRITE_MATRIX m; int ang;
    for (ang = 0; ang < 4; ang++) CHECKF(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)ang, FALSE, &m) == S_OK, "angle %d", ang);
    for (ang = 4; ang < 8; ang++) {
        memset(&m, 0x77, sizeof m);
        CHECKF(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)ang, FALSE, &m) == E_INVALIDARG, "angle %d", ang);
        /* whether the output matrix is written on failure is implementation detail: not asserted */
    }
    CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)-1, TRUE, &m), E_INVALIDARG);
    if (SUCCEEDED(IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer2, (void **)&a2)) && a2) {
        for (ang = 0; ang < 4; ang++) CHECKF(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, (DWRITE_GLYPH_ORIENTATION_ANGLE)ang, FALSE, 1.f, 2.f, &m) == S_OK, "angle %d (origin form)", ang);
        memset(&m, 0x77, sizeof m);
        CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, (DWRITE_GLYPH_ORIENTATION_ANGLE)4, FALSE, 1.f, 2.f, &m), E_INVALIDARG);
        DW_RELEASE(a2);
    }
    DW_RELEASE(a1); END_ANALYZER(a);
}
