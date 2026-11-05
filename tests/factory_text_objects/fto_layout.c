/* IDWriteFactory::CreateTextLayout / CreateGdiCompatibleTextLayout / CreateEllipsisTrimmingSign.
 *
 * Facts (Win10 19041 dwrite.dll):
 *   CreateTextLayout: *ppLayout zeroed first; string==NULL, format==NULL, maxWidth<0 / NaN, maxHeight<0 / NaN -> E_INVALIDARG.
 *     (stringLength is not validated against the string; 0 is valid.)  The layout copies the string and the format's properties.
 *   CreateGdiCompatibleTextLayout: same plus pixelsPerDip NaN and any NaN in a non-NULL transform -> E_INVALIDARG;
 *     useGdiNatural selects GDI_NATURAL (nonzero) or GDI_CLASSIC measuring.
 *   CreateEllipsisTrimmingSign: format==NULL -> E_INVALIDARG; otherwise an IDWriteInlineObject laying out U+2026.
 * The synthetic fixture font has 1000/2048 em advance for A-Z, a-z, 0-9, so at 20 DIP a run of N such glyphs is N * 9.765625 wide.
 */
#include "fto.h"

#define ADV20 (1000.0f * 20.0f / 2048.0f)   /* 9.765625: exactly representable */

static IDWriteTextFormat *coll_format(IDWriteFactory *f, fto_coll *fc, float size) {
    IDWriteTextFormat *fmt = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(f, fc->family, fc->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt))) return NULL;
    return fmt;
}

/* Total of DWRITE_LINE_METRICS.length over all lines == number of text positions in the layout. */
static UINT32 text_len(IDWriteTextLayout *l) {
    UINT32 n = 0, i, total = 0;
    DWRITE_LINE_METRICS *lm;
    HRESULT hr = IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n);
    if (hr != FTO_E_NOT_SUFFICIENT_BUFFER && hr != S_OK) return (UINT32)-1;
    if (n == 0) return 0;
    lm = (DWRITE_LINE_METRICS *)calloc(n, sizeof *lm);
    if (!lm) return (UINT32)-1;
    if (FAILED(IDWriteTextLayout_GetLineMetrics(l, lm, n, &n))) { free(lm); return (UINT32)-1; }
    for (i = 0; i < n; i++) total += lm[i].length;
    free(lm);
    return total;
}

#define MAKE(l, f, txt, len, fmt, w, h) IDWriteFactory_CreateTextLayout(f, txt, len, fmt, w, h, &(l))

/* ---- CreateTextLayout argument validation ------------------------------------------------------------------ */
FTO_BOTH(fto_lay_null_string) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR(MAKE(l, f, NULL, 0, fmt, 100, 100), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL;
    CHECK_HR(MAKE(l, f, NULL, 5, fmt, 100, 100), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_null_format) {
    IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
    CHECK_HR(MAKE(l, f, L"abc", 3, NULL, 100, 100), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL;
    CHECK_HR(MAKE(l, f, L"", 0, NULL, 100, 100), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
}

FTO_BOTH(fto_lay_null_string_and_format) {
    IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
    CHECK_HR(MAKE(l, f, NULL, 0, NULL, 0, 0), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
}

FTO_BOTH(fto_lay_invalid_extents_table) {
    static const float bad[] = { -1.0f, -0.001f, -FLT_MIN, -1e30f, -FLT_MAX, -INFINITY, NAN, -NAN };
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    unsigned i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
        HRESULT hr = MAKE(l, f, L"abc", 3, fmt, bad[i], 100.0f);
        CHECKF(hr == FTO_E_INVALIDARG, "maxWidth %g: %s", bad[i], dw_hr_name(hr)); CHECK_OUT_NULL(l);
        l = (IDWriteTextLayout *)FTO_SENTINEL;
        hr = MAKE(l, f, L"abc", 3, fmt, 100.0f, bad[i]);
        CHECKF(hr == FTO_E_INVALIDARG, "maxHeight %g: %s", bad[i], dw_hr_name(hr)); CHECK_OUT_NULL(l);
        l = (IDWriteTextLayout *)FTO_SENTINEL;
        hr = MAKE(l, f, L"abc", 3, fmt, bad[i], bad[i]);
        CHECKF(hr == FTO_E_INVALIDARG, "both %g: %s", bad[i], dw_hr_name(hr)); CHECK_OUT_NULL(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_valid_extents_table) {
    static const float ok[] = { 0.0f, -0.0f, FLT_MIN, 0.5f, 1.0f, 100.0f, 12345.678f, 1e6f, 1e20f, FLT_MAX };
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    unsigned i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        IDWriteTextLayout *l = NULL;
        HRESULT hr = MAKE(l, f, L"abc", 3, fmt, ok[i], ok[(i + 3) % (sizeof ok / sizeof ok[0])]);
        CHECKF(hr == S_OK, "extent %g: %s", ok[i], dw_hr_name(hr));
        if (hr == S_OK && l) {
            CHECK(IDWriteTextLayout_GetMaxWidth(l) == ok[i]);
            CHECK(IDWriteTextLayout_GetMaxHeight(l) == ok[(i + 3) % (sizeof ok / sizeof ok[0])]);
            IDWriteTextLayout_Release(l);
        }
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_infinite_extent_not_nan_checked) {
    /* +INF is neither negative nor NaN, so the argument check lets it through; the outcome after that is a note. */
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
    if (!fmt) { t_skip("no format"); return; }
    HRESULT hr = MAKE(l, f, L"abc", 3, fmt, INFINITY, INFINITY);
    t_note("%s: +INF extents -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && l) IDWriteTextLayout_Release(l); else CHECK_OUT_NULL(l);
    IDWriteTextFormat_Release(fmt);
}

/* ---- text length semantics -------------------------------------------------------------------------------------- */
FTO_BOTH(fto_lay_empty_text) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    DWRITE_TEXT_METRICS m;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(MAKE(l, f, L"", 0, fmt, 200, 100));
    if (l) {
        CHECK(text_len(l) == 0);
        memset(&m, 0xCD, sizeof m);
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
        CHECK_FLOAT_EQ(m.layoutWidth, 200.0f);
        CHECK_FLOAT_EQ(m.layoutHeight, 100.0f);
        CHECK_FLOAT_EQ(m.width, 0.0f);
        t_note("%s: empty layout lineCount=%u height=%g", tag, m.lineCount, m.height);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_zero_length_with_text) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    /* stringLength is authoritative: 0 means empty even though the buffer has characters. */
    CHECK_HR_OK(MAKE(l, f, L"abcdef", 0, fmt, 200, 100));
    if (l) { CHECK(text_len(l) == 0); IDWriteTextLayout_Release(l); }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_prefix_length) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    unsigned n;
    if (!fmt) { t_skip("no format"); return; }
    for (n = 1; n <= 6; n++) {
        IDWriteTextLayout *l = NULL;
        CHECK_HR_OK(MAKE(l, f, L"abcdef", n, fmt, 500, 100));
        if (l) { CHECKF(text_len(l) == n, "prefix %u: text length %u", n, text_len(l)); IDWriteTextLayout_Release(l); }
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_embedded_nul_counts) {
    static const WCHAR txt[] = { 'a', 0, 'b', 0, 'c' };
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(MAKE(l, f, txt, 5, fmt, 500, 100));
    if (l) { CHECK(text_len(l) == 5); IDWriteTextLayout_Release(l); }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_surrogates) {
    static const WCHAR pair[] = { 'A', 0xD83D, 0xDE00, 'B' };       /* A U+1F600 B */
    static const WCHAR lone_hi[] = { 'A', 0xD800, 'B' };
    static const WCHAR lone_lo[] = { 'A', 0xDC00, 'B' };
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(MAKE(l, f, pair, 4, fmt, 500, 100));
    if (l) { CHECK(text_len(l) == 4); IDWriteTextLayout_Release(l); }
    l = NULL;
    HRESULT hr = MAKE(l, f, lone_hi, 3, fmt, 500, 100);
    t_note("%s: lone high surrogate -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && l) { CHECK(text_len(l) == 3); IDWriteTextLayout_Release(l); }
    l = NULL;
    hr = MAKE(l, f, lone_lo, 3, fmt, 500, 100);
    t_note("%s: lone low surrogate -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && l) { CHECK(text_len(l) == 3); IDWriteTextLayout_Release(l); }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_large_text) {
    static const size_t lens[] = { 1000, 10000, 50000 };
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    unsigned i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < 3; i++) {
        WCHAR *t = fto_repeat('a', lens[i]);
        IDWriteTextLayout *l = NULL;
        CHECKF(MAKE(l, f, t, (UINT32)lens[i], fmt, 1e6f, 1e6f) == S_OK, "length %zu", lens[i]);
        if (l) { CHECKF(text_len(l) == lens[i], "length %zu text_len=%u", lens[i], text_len(l)); IDWriteTextLayout_Release(l); }
        free(t);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_string_is_copied) {
    fto_coll fc;
    IDWriteTextFormat *fmt;
    IDWriteTextLayout *l = NULL;
    WCHAR buf[8];
    fto_coll_setup(&fc, f, "FtoLayFam");
    if (!fc.coll) { t_skip("custom collection unavailable: %s", dw_hr_name(fc.hr)); fto_coll_teardown(&fc); return; }
    fmt = coll_format(f, &fc, 20.0f);
    lstrcpyW(buf, L"ABC");
    CHECK_HR_OK(MAKE(l, f, buf, 3, fmt, 500, 100));
    lstrcpyW(buf, L"XYZ");              /* the layout must not read the caller's buffer afterwards */
    if (l) {
        dwx_renderer *r = dwx_renderer_new();
        int cur = 0;
        CHECK_HR_OK(IDWriteTextLayout_Draw(l, (void *)0x1234, (IDWriteTextRenderer *)r, 0, 0));
        const dwx_draw *d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
        CHECK(d != NULL);
        if (d && d->nglyphs == 3) {
            CHECK(d->indices[0] == ttf_default_gid('A') && d->indices[1] == ttf_default_gid('B') && d->indices[2] == ttf_default_gid('C'));
            if (d->has_desc) CHECK(d->text_len == 3 && d->text && !memcmp(d->text, L"ABC", 6));
        } else if (d) CHECKF(0, "expected one run of 3 glyphs, got %u", d->nglyphs);
        dwx_release(r);
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(fmt);
    fto_coll_teardown(&fc);
}

/* ---- exact geometry from the synthetic font --------------------------------------------------------------------- */
FTO_BOTH(fto_lay_synthetic_run_geometry) {
    fto_coll fc;
    IDWriteTextFormat *fmt;
    IDWriteTextLayout *l = NULL;
    DWRITE_TEXT_METRICS m;
    fto_coll_setup(&fc, f, "FtoLayGeo");
    if (!fc.coll) { t_skip("custom collection unavailable: %s", dw_hr_name(fc.hr)); fto_coll_teardown(&fc); return; }
    fmt = coll_format(f, &fc, 20.0f);
    CHECK_HR_OK(MAKE(l, f, L"ABCD", 4, fmt, 1000, 500));
    if (l) {
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
        CHECK_NEAR(m.width, 4 * ADV20, 0.01);
        CHECK_NEAR(m.widthIncludingTrailingWhitespace, 4 * ADV20, 0.01);
        CHECK_FLOAT_EQ(m.layoutWidth, 1000.0f);
        CHECK_FLOAT_EQ(m.layoutHeight, 500.0f);
        CHECK(m.lineCount == 1);
        CHECK_NEAR(m.left, 0.0f, 0.001);
        CHECK_NEAR(m.top, 0.0f, 0.001);
        CHECK(m.height > 0.0f);
        FLOAT minw = 0;
        CHECK_HR_OK(IDWriteTextLayout_DetermineMinWidth(l, &minw));
        CHECK_NEAR(minw, 4 * ADV20, 0.01);      /* no break opportunity inside one word */
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(fmt);
    fto_coll_teardown(&fc);
}

FTO_BOTH(fto_lay_synthetic_draw_callbacks) {
    fto_coll fc;
    IDWriteTextFormat *fmt;
    IDWriteTextLayout *l = NULL;
    fto_coll_setup(&fc, f, "FtoLayDraw");
    if (!fc.coll) { t_skip("custom collection unavailable: %s", dw_hr_name(fc.hr)); fto_coll_teardown(&fc); return; }
    fmt = coll_format(f, &fc, 20.0f);
    CHECK_HR_OK(MAKE(l, f, L"abc", 3, fmt, 1000, 500));
    if (l) {
        dwx_renderer *r = dwx_renderer_new();
        int cur = 0;
        void *ctx = (void *)(ULONG_PTR)0xC0FFEE;
        CHECK_HR_OK(IDWriteTextLayout_Draw(l, ctx, (IDWriteTextRenderer *)r, 5.0f, 7.0f));
        const dwx_draw *d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
        CHECK(d != NULL);
        CHECK(r->count[DWX_DRAW_GLYPHRUN] == 1);
        CHECK(r->count[DWX_DRAW_UNDERLINE] == 0 && r->count[DWX_DRAW_STRIKETHROUGH] == 0 && r->count[DWX_DRAW_INLINE] == 0);
        if (d) {
            CHECK(d->ctx == ctx);
            CHECK_NEAR(d->x, 5.0f, 0.001);
            CHECK(d->y > 7.0f);                       /* baseline below the top of the box */
            CHECK(d->nglyphs == 3);
            CHECK_FLOAT_EQ(d->em, 20.0f);
            if (d->nglyphs == 3) {
                unsigned i;
                for (i = 0; i < 3; i++) {
                    CHECK(d->indices[i] == ttf_default_gid('a' + i));
                    CHECK_NEAR(d->advances[i], ADV20, 0.001);
                }
            }
            CHECK(d->bidi == 0 && !d->sideways);
        }
        dwx_release(r);
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(fmt);
    fto_coll_teardown(&fc);
}

/* ---- the layout snapshots the format ------------------------------------------------------------------------------ */
FTO_BOTH(fto_lay_inherits_format_properties) {
    IDWriteTextFormat *fmt = NULL;
    IDWriteTextLayout *l = NULL;
    DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Consolas", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_OBLIQUE,
                                                DWRITE_FONT_STRETCH_SEMI_EXPANDED, 23.5f, L"fr-ca", &fmt));
    if (!fmt) return;
    IDWriteTextFormat_SetTextAlignment(fmt, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat_SetParagraphAlignment(fmt, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    IDWriteTextFormat_SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteTextFormat_SetIncrementalTabStop(fmt, 77.0f);
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    CHECK_HR_OK(MAKE(l, f, L"Hello", 5, fmt, 321.0f, 123.0f));
    if (l) {
        CHECK(IDWriteTextLayout_GetTextAlignment(l) == DWRITE_TEXT_ALIGNMENT_TRAILING);
        CHECK(IDWriteTextLayout_GetParagraphAlignment(l) == DWRITE_PARAGRAPH_ALIGNMENT_FAR);
        CHECK(IDWriteTextLayout_GetWordWrapping(l) == DWRITE_WORD_WRAPPING_NO_WRAP);
        CHECK(IDWriteTextLayout_GetReadingDirection(l) == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), 77.0f);
        DWRITE_LINE_SPACING_METHOD m = DWRITE_LINE_SPACING_METHOD_DEFAULT; FLOAT sp = 0, bl = 0;
        CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &sp, &bl));
        CHECK(m == DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(sp, 30.0f); CHECK_FLOAT_EQ(bl, 24.0f);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), 321.0f);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), 123.0f);
        /* the layout also exposes the IDWriteTextFormat getters for the base format of position 0 */
        DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; DWRITE_FONT_STRETCH sr; FLOAT sz;
        memset(&r, 0xAA, sizeof r);
        CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 0, &w, &r)); CHECK(w == DWRITE_FONT_WEIGHT_SEMI_BOLD);
        CHECK(r.startPosition == 0 && r.length == 5);
        CHECK_HR_OK(IDWriteTextLayout_GetFontStyle(l, 0, &st, &r)); CHECK(st == DWRITE_FONT_STYLE_OBLIQUE);
        CHECK(r.startPosition == 0 && r.length == 5);
        CHECK_HR_OK(IDWriteTextLayout_GetFontStretch(l, 0, &sr, &r)); CHECK(sr == DWRITE_FONT_STRETCH_SEMI_EXPANDED);
        CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 0, &sz, &r)); CHECK_FLOAT_EQ(sz, 23.5f);
        CHECK(r.startPosition == 0 && r.length == 5);
        UINT32 n = 0; WCHAR name[32];
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &n, &r)); CHECK(n == 8);
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, name, 32, &r)); CHECK(!lstrcmpW(name, L"Consolas"));
        CHECK_HR_OK(IDWriteTextLayout_GetLocaleNameLength(l, 0, &n, &r)); CHECK(n == 5);
        CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 0, name, 32, &r)); CHECK(!lstrcmpW(name, L"fr-ca"));
        CHECK(r.startPosition == 0 && r.length == 5);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_format_changes_after_creation_do_not_leak) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(MAKE(l, f, L"abc", 3, fmt, 100, 100));
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(fmt, DWRITE_TEXT_ALIGNMENT_CENTER));
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_CHARACTER));
    CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(fmt, 5.0f));
    if (l) {
        CHECK(IDWriteTextLayout_GetTextAlignment(l) == DWRITE_TEXT_ALIGNMENT_LEADING);
        CHECK(IDWriteTextLayout_GetWordWrapping(l) == DWRITE_WORD_WRAPPING_WRAP);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), 48.0f);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_survives_format_release) {
    IDWriteTextFormat *fmt = fto_format(f, 17.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(MAKE(l, f, L"hello world", 11, fmt, 400, 100));
    IDWriteTextFormat_Release(fmt);
    if (l) {
        FLOAT sz = 0; DWRITE_TEXT_RANGE r;
        CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 0, &sz, &r));
        CHECK_FLOAT_EQ(sz, 17.0f);
        DWRITE_TEXT_METRICS m;
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
        CHECK(m.width > 0.0f);
        IDWriteTextLayout_Release(l);
    }
}

FTO_BOTH(fto_lay_format_refcount_untouched) {
    /* Creating a layout from a format neither keeps nor leaks references on the format object itself. */
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    ULONG before = dw_refcount((IUnknown *)fmt);
    CHECK_HR_OK(MAKE(l, f, L"abc", 3, fmt, 100, 100));
    if (l) {
        ULONG during = dw_refcount((IUnknown *)fmt);
        t_note("%s: format refcount %lu -> %lu while a layout exists", tag, before, during);
        IDWriteTextLayout_Release(l);
        CHECK(dw_refcount((IUnknown *)fmt) == before);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_qi_matrix) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(MAKE(l, f, L"abc", 3, fmt, 100, 100));
    if (l) {
        dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteTextLayout, 1), DW_QI_ROW(IDWriteTextLayout1, 1), DW_QI_ROW(IDWriteTextLayout2, 1), DW_QI_ROW(IDWriteTextLayout3, 1),
            DW_QI_ROW(IDWriteTextFormat, 1), DW_QI_ROW(IDWriteTextFormat1, 1), DW_QI_ROW(IDWriteTextFormat2, 1), DW_QI_ROW(IDWriteTextFormat3, 1),
            DW_QI_ROW(IDWriteTextLayout4, -1),
            DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontCollection, 0),
            DW_QI_ROW(IDWriteInlineObject, 0), DW_QI_ROW(IDWriteTypography, 0), DW_QI_ROW(IDWriteTextAnalyzer, 0),
        };
        DW_CHECK_QI(l, rows);
        EXPECT_REFS(l, 1);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_lay_distinct_layouts_from_one_format) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *a = NULL, *b = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(MAKE(a, f, L"abc", 3, fmt, 100, 100));
    CHECK_HR_OK(MAKE(b, f, L"abcdef", 6, fmt, 200, 300));
    if (a && b) {
        CHECK(a != b);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(a), 100.0f);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(b), 200.0f);
        DWRITE_TEXT_RANGE r03 = { 0, 3 };
        CHECK_HR_OK(IDWriteTextLayout_SetFontSize(a, 40.0f, r03));
        FLOAT sz = 0; DWRITE_TEXT_RANGE r;
        IDWriteTextLayout_GetFontSize(b, 0, &sz, &r);
        CHECK_FLOAT_EQ(sz, 12.0f);
        CHECK(text_len(a) == 3 && text_len(b) == 6);
    }
    DW_RELEASE(a); DW_RELEASE(b); IDWriteTextFormat_Release(fmt);
}

TEST(fto_lay_outlives_isolated_factory) {
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteTextFormat *fmt;
    IDWriteTextLayout *l = NULL;
    if (!iso) { t_skip("no factory"); return; }
    fmt = fto_format(iso, 14.0f);
    if (!fmt) { t_skip("no format"); IDWriteFactory_Release(iso); return; }
    CHECK_HR_OK(MAKE(l, iso, L"The quick brown fox", 19, fmt, 60.0f, 400.0f));
    IDWriteTextFormat_Release(fmt);
    IDWriteFactory_Release(iso);
    if (l) {
        DWRITE_TEXT_METRICS m;
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
        CHECK(m.lineCount >= 2);                    /* 19 characters in a 60 DIP box at 14 DIP must wrap */
        CHECK(text_len(l) == 19);
        dwx_renderer *r = dwx_renderer_new();
        CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0, 0));
        CHECK(r->count[DWX_DRAW_GLYPHRUN] >= 1);
        dwx_release(r);
        IDWriteTextLayout_Release(l);
    }
}

TEST(fto_lay_cross_factory_format) {
    /* A format created by one factory used to build a layout on another: not specified; must neither crash nor leak. */
    IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *b = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!a || !b) { t_skip("no factories"); DW_RELEASE(a); DW_RELEASE(b); return; }
    IDWriteTextFormat *fmt = fto_format(a, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (fmt) {
        HRESULT hr = MAKE(l, b, L"abc", 3, fmt, 100, 100);
        t_note("layout from foreign-factory format -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr) && l) IDWriteTextLayout_Release(l); else CHECK_OUT_NULL(l);
        IDWriteTextFormat_Release(fmt);
    }
    IDWriteFactory_Release(a); IDWriteFactory_Release(b);
}

/* ---- CreateGdiCompatibleTextLayout ---------------------------------------------------------------------------------- */
#define GMAKE(l, f, txt, len, fmt, w, h, ppd, xf, nat) IDWriteFactory_CreateGdiCompatibleTextLayout(f, txt, len, fmt, w, h, ppd, xf, nat, &(l))

FTO_BOTH(fto_gdi_lay_basic_success) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(GMAKE(l, f, L"hello", 5, fmt, 300.0f, 200.0f, 1.0f, NULL, FALSE));
    if (l && l != FTO_SENTINEL) {
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), 300.0f);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), 200.0f);
        CHECK(text_len(l) == 5);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_gdi_lay_null_and_negative_args) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l;
    if (!fmt) { t_skip("no format"); return; }
    l = (IDWriteTextLayout *)FTO_SENTINEL; CHECK_HR(GMAKE(l, f, NULL, 0, fmt, 100, 100, 1.0f, NULL, FALSE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL; CHECK_HR(GMAKE(l, f, L"a", 1, NULL, 100, 100, 1.0f, NULL, FALSE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL; CHECK_HR(GMAKE(l, f, L"a", 1, fmt, -1, 100, 1.0f, NULL, FALSE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL; CHECK_HR(GMAKE(l, f, L"a", 1, fmt, 100, -1, 1.0f, NULL, FALSE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL; CHECK_HR(GMAKE(l, f, L"a", 1, fmt, NAN, 100, 1.0f, NULL, FALSE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL; CHECK_HR(GMAKE(l, f, L"a", 1, fmt, 100, NAN, 1.0f, NULL, FALSE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL; CHECK_HR(GMAKE(l, f, L"a", 1, fmt, -INFINITY, 100, 1.0f, NULL, FALSE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_gdi_lay_pixels_per_dip_nan) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR(GMAKE(l, f, L"a", 1, fmt, 100, 100, NAN, NULL, FALSE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    l = (IDWriteTextLayout *)FTO_SENTINEL;
    CHECK_HR(GMAKE(l, f, L"a", 1, fmt, 100, 100, -NAN, NULL, TRUE), FTO_E_INVALIDARG); CHECK_OUT_NULL(l);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_gdi_lay_pixels_per_dip_values) {
    /* Only NaN is rejected by the factory; zero / negative / huge values are handed to the layout (outcome noted). */
    static const float v[] = { 0.0f, -1.0f, 0.5f, 1.0f, 1.25f, 1.5f, 2.0f, 4.0f, 100.0f };
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    unsigned i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
        HRESULT hr = GMAKE(l, f, L"abc", 3, fmt, 100, 100, v[i], NULL, FALSE);
        if (v[i] > 0.0f) CHECKF(hr == S_OK, "pixelsPerDip %g: %s", v[i], dw_hr_name(hr));
        else t_note("%s: pixelsPerDip %g -> %s", tag, v[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && l) IDWriteTextLayout_Release(l); else CHECK_OUT_NULL(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_gdi_lay_transform_nan_each_member) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    unsigned i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX m = { 1, 0, 0, 1, 0, 0 };
        FLOAT *p = &m.m11 + i;
        IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
        *p = NAN;
        HRESULT hr = GMAKE(l, f, L"abc", 3, fmt, 100, 100, 1.0f, &m, FALSE);
        CHECKF(hr == FTO_E_INVALIDARG, "NaN in matrix member %u: %s", i, dw_hr_name(hr));
        CHECK_OUT_NULL(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_gdi_lay_transform_values) {
    static const DWRITE_MATRIX ms[] = {
        { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 2, 0, 0 }, { 0.5f, 0, 0, 0.5f, 10, 20 }, { 1, 0.5f, 0, 1, 0, 0 }, { 0, 1, -1, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0 }, { -1, 0, 0, -1, 0, 0 }, { 1e10f, 0, 0, 1e10f, 0, 0 },
    };
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    unsigned i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < sizeof ms / sizeof ms[0]; i++) {
        IDWriteTextLayout *l = (IDWriteTextLayout *)FTO_SENTINEL;
        HRESULT hr = GMAKE(l, f, L"abc", 3, fmt, 100, 100, 1.0f, &ms[i], i & 1);
        if (i <= 4) CHECKF(hr == S_OK, "matrix %u: %s", i, dw_hr_name(hr));
        else t_note("%s: degenerate matrix %u -> %s", tag, i, dw_hr_name(hr));
        if (SUCCEEDED(hr) && l) IDWriteTextLayout_Release(l); else CHECK_OUT_NULL(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_gdi_lay_use_gdi_natural_values) {
    /* The BOOL is a plain flag: every nonzero value means GDI natural. */
    static const BOOL v[] = { 0, 1, 2, -1, 0x100, (BOOL)0x80000000 };
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    unsigned i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteTextLayout *l = NULL;
        CHECKF(GMAKE(l, f, L"abc", 3, fmt, 100, 100, 1.0f, NULL, v[i]) == S_OK, "useGdiNatural %d", v[i]);
        DW_RELEASE(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_gdi_lay_lengths_and_text) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(GMAKE(l, f, L"", 0, fmt, 100, 100, 1.0f, NULL, FALSE));
    if (l) { CHECK(text_len(l) == 0); IDWriteTextLayout_Release(l); }
    l = NULL;
    CHECK_HR_OK(GMAKE(l, f, L"abcdef", 4, fmt, 100, 100, 1.0f, NULL, FALSE));
    if (l) { CHECK(text_len(l) == 4); IDWriteTextLayout_Release(l); }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_gdi_lay_pixel_widths_note) {
    fto_coll fc;
    IDWriteTextFormat *fmt;
    unsigned mode;
    fto_coll_setup(&fc, f, "FtoGdiFam");
    if (!fc.coll) { t_skip("custom collection unavailable"); fto_coll_teardown(&fc); return; }
    fmt = coll_format(f, &fc, 20.0f);
    for (mode = 0; mode < 2; mode++) {
        IDWriteTextLayout *l = NULL, *nat = NULL;
        DWRITE_TEXT_METRICS m, mn;
        CHECK_HR_OK(GMAKE(l, f, L"ABCDEFGH", 8, fmt, 1000, 100, 1.0f, NULL, mode));
        CHECK_HR_OK(MAKE(nat, f, L"ABCDEFGH", 8, fmt, 1000, 100));
        if (l && nat) {
            CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
            CHECK_HR_OK(IDWriteTextLayout_GetMetrics(nat, &mn));
            t_note("%s: gdi-compatible (natural=%u) width %g vs ideal %g (8 glyphs of %g)", tag, mode, m.width, mn.width, ADV20);
            CHECK_NEAR(mn.width, 8 * ADV20, 0.01);
            CHECK(m.lineCount == 1);
            /* GDI-compatible measuring uses whole-pixel advances at 1 dip/pixel */
            CHECK(DW_APPROX(m.width, (float)(int)(m.width + 0.5f), 0.001));
        }
        DW_RELEASE(l); DW_RELEASE(nat);
    }
    DW_RELEASE(fmt);
    fto_coll_teardown(&fc);
}

FTO_BOTH(fto_gdi_lay_qi_and_refs) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *l = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(GMAKE(l, f, L"abc", 3, fmt, 100, 100, 1.0f, NULL, TRUE));
    if (l) {
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteTextLayout, 1), DW_QI_ROW(IDWriteTextLayout1, 1), DW_QI_ROW(IDWriteTextFormat, 1), DW_QI_ROW(IDWriteFont, 0) };
        DW_CHECK_QI(l, rows);
        EXPECT_REFS(l, 1);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(fmt);
}

/* ---- CreateEllipsisTrimmingSign ------------------------------------------------------------------------------------- */
FTO_BOTH(fto_sign_null_format) {
    IDWriteInlineObject *s = (IDWriteInlineObject *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateEllipsisTrimmingSign(f, NULL, &s), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(s);
}

FTO_BOTH(fto_sign_basic) {
    IDWriteTextFormat *fmt = fto_format(f, 16.0f);
    IDWriteInlineObject *s = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, fmt, &s));
    if (s) {
        EXPECT_REFS(s, 1);
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteInlineObject, 1), DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteTextLayout, 0),
                             DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteTypography, 0) };
        DW_CHECK_QI(s, rows);
        DWRITE_INLINE_OBJECT_METRICS m; memset(&m, 0, sizeof m);
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &m));
        CHECK(m.width > 0.0f);
        CHECK(m.height > 0.0f);
        CHECK(m.baseline > 0.0f && m.baseline <= m.height + 0.01f);
        t_note("%s: sign metrics w=%g h=%g baseline=%g sideways=%d", tag, m.width, m.height, m.baseline, m.supportsSideways);
        DWRITE_OVERHANG_METRICS o; memset(&o, 0, sizeof o);
        CHECK_HR_OK(IDWriteInlineObject_GetOverhangMetrics(s, &o));
        t_note("%s: sign overhangs l=%g t=%g r=%g b=%g", tag, o.left, o.top, o.right, o.bottom);
        DWRITE_BREAK_CONDITION b = (DWRITE_BREAK_CONDITION)99, a = (DWRITE_BREAK_CONDITION)99;
        CHECK_HR_OK(IDWriteInlineObject_GetBreakConditions(s, &b, &a));
        t_note("%s: sign break conditions before=%d after=%d", tag, b, a);
        CHECK(b <= DWRITE_BREAK_CONDITION_MUST_BREAK && a <= DWRITE_BREAK_CONDITION_MUST_BREAK);
        IDWriteInlineObject_Release(s);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_sign_size_follows_format) {
    IDWriteTextFormat *small_f = fto_format(f, 10.0f), *big_f = fto_format(f, 40.0f);
    IDWriteInlineObject *a = NULL, *b = NULL;
    if (!small_f || !big_f) { t_skip("no format"); DW_RELEASE(small_f); DW_RELEASE(big_f); return; }
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, small_f, &a));
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, big_f, &b));
    if (a && b) {
        DWRITE_INLINE_OBJECT_METRICS ma, mb;
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(a, &ma));
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(b, &mb));
        CHECK(a != b);
        CHECK(mb.width > ma.width * 3.0f && mb.width < ma.width * 5.0f);       /* 4x font size => about 4x width */
        CHECK(mb.height > ma.height * 3.0f && mb.height < ma.height * 5.0f);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(small_f); DW_RELEASE(big_f);
}

FTO_BOTH(fto_sign_draw_reaches_renderer) {
    IDWriteTextFormat *fmt = fto_format(f, 16.0f);
    IDWriteInlineObject *s = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, fmt, &s));
    if (s) {
        dwx_renderer *r = dwx_renderer_new();
        int cur = 0;
        void *ctx = (void *)(ULONG_PTR)0xABCDEF;
        CHECK_HR_OK(IDWriteInlineObject_Draw(s, ctx, (IDWriteTextRenderer *)r, 10.0f, 20.0f, FALSE, FALSE, NULL));
        CHECK(r->count[DWX_DRAW_GLYPHRUN] >= 1);
        CHECK(r->count[DWX_DRAW_INLINE] == 0);
        const dwx_draw *d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
        if (d) {
            CHECK(d->ctx == ctx);
            CHECK(d->x >= 10.0f - 0.01f);
            CHECK(d->y >= 20.0f);
            CHECK(d->nglyphs == 1);
            if (d->has_desc) { CHECK(d->text_len == 1 && d->text && d->text[0] == 0x2026); }
            else t_note("%s: sign glyph run has no description", tag);
        }
        dwx_release(r);
        IDWriteInlineObject_Release(s);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_sign_used_for_trimming) {
    fto_coll fc;
    IDWriteTextFormat *fmt;
    IDWriteInlineObject *s = NULL;
    IDWriteTextLayout *l = NULL;
    fto_coll_setup(&fc, f, "FtoSignFam");
    if (!fc.coll) { t_skip("custom collection unavailable"); fto_coll_teardown(&fc); return; }
    fmt = coll_format(f, &fc, 20.0f);
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, fmt, &s));
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(fmt, &tr, s));
    {
        DWRITE_TRIMMING got; IDWriteInlineObject *gs = NULL;
        CHECK_HR_OK(IDWriteTextFormat_GetTrimming(fmt, &got, &gs));
        CHECK(got.granularity == DWRITE_TRIMMING_GRANULARITY_CHARACTER);
        CHECK(gs == s);
        DW_RELEASE(gs);
    }
    CHECK_HR_OK(MAKE(l, f, L"ABCDEFGHIJKLMNOP", 16, fmt, 90.0f, 100.0f));
    if (l && s) {
        dwx_renderer *r = dwx_renderer_new();
        CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0, 0));
        CHECK(r->count[DWX_DRAW_INLINE] == 1);
        int cur = 0;
        const dwx_draw *d = dwx_renderer_next(r, DWX_DRAW_INLINE, &cur);
        if (d) CHECK(d->obj == s);
        DWRITE_TEXT_METRICS m;
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
        CHECK(m.lineCount == 1);
        dwx_release(r);
    }
    DW_RELEASE(l); DW_RELEASE(s); DW_RELEASE(fmt);
    fto_coll_teardown(&fc);
}

TEST(fto_sign_outlives_format_and_factory) {
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteInlineObject *s = NULL;
    if (!iso) { t_skip("no factory"); return; }
    IDWriteTextFormat *fmt = fto_format(iso, 22.0f);
    if (!fmt) { t_skip("no format"); IDWriteFactory_Release(iso); return; }
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(iso, fmt, &s));
    IDWriteTextFormat_Release(fmt);
    IDWriteFactory_Release(iso);
    if (s) {
        DWRITE_INLINE_OBJECT_METRICS m;
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &m));
        CHECK(m.width > 0.0f);
        dwx_renderer *r = dwx_renderer_new();
        CHECK_HR_OK(IDWriteInlineObject_Draw(s, NULL, (IDWriteTextRenderer *)r, 0, 0, FALSE, FALSE, NULL));
        CHECK(r->count[DWX_DRAW_GLYPHRUN] >= 1);
        dwx_release(r);
        IDWriteInlineObject_Release(s);
    }
}
