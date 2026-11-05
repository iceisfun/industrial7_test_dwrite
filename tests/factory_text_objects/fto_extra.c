/* Additional creation-time conformance for layouts built by IDWriteFactory::CreateTextLayout from a synthetic font.
 * The fixture font advances every A-Z a-z 0-9 glyph by 1000/2048 em, so natural-mode widths are N * 1000 * size / 2048. */
#include "fto.h"

#define ADV(sz) (1000.0 * (sz) / 2048.0)

static IDWriteTextFormat *xfmt(IDWriteFactory *f, fto_coll *fc, float size) {
    IDWriteTextFormat *fmt = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(f, fc->family, fc->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt))) return NULL;
    return fmt;
}

/* Run body with a layout of `txt` over the fixture family at `size` in a box of w x h. */
typedef void (*xbody)(IDWriteFactory *f, IDWriteTextFormat *fmt, const char *tag);
static void with_fixture(IDWriteFactory *f, const char *tag, const char *fam, float size, xbody b) {
    fto_coll fc; IDWriteTextFormat *fmt;
    fto_coll_setup(&fc, f, fam);
    if (!fc.coll) { t_skip("custom collection unavailable: %s", dw_hr_name(fc.hr)); fto_coll_teardown(&fc); return; }
    fmt = xfmt(f, &fc, size);
    CHECK(fmt != NULL);
    if (fmt) b(f, fmt, tag);
    DW_RELEASE(fmt);
    fto_coll_teardown(&fc);
}

/* ---- width scales linearly with font size -------------------------------------------------------------- */
static void width_scale_body(IDWriteFactory *f, IDWriteTextFormat *fmt0, const char *tag) {
    static const float sizes[] = { 8.0f, 10.0f, 16.0f, 20.0f, 32.0f, 64.0f, 100.0f };
    static const UINT32 counts[] = { 1, 2, 5, 13 };
    fto_coll fc; unsigned i, j;
    (void)fmt0;
    fto_coll_setup(&fc, f, "FtoWidthScale");
    if (!fc.coll) { t_skip("custom collection unavailable"); fto_coll_teardown(&fc); return; }
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextFormat *fmt = xfmt(f, &fc, sizes[i]);
        if (!fmt) { CHECK(0); continue; }
        for (j = 0; j < sizeof counts / sizeof counts[0]; j++) {
            IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
            WCHAR *t = fto_repeat('m', counts[j]);
            CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, t, counts[j], fmt, 100000.0f, 1000.0f, &l));
            if (l && SUCCEEDED(IDWriteTextLayout_GetMetrics(l, &m))) {
                CHECKF(fabs(m.width - counts[j] * 1000.0 * sizes[i] / 2048.0) < 0.01, "%s size %g x%u: width %g", tag, sizes[i], counts[j], m.width);
                CHECK(m.lineCount == 1);
                CHECK_FLOAT_EQ(m.layoutWidth, 100000.0f);
            }
            DW_RELEASE(l);
            free(t);
        }
        IDWriteTextFormat_Release(fmt);
    }
    fto_coll_teardown(&fc);
}
FTO_BOTH(fto_x_width_scales_with_size) { width_scale_body(f, NULL, tag); }

/* ---- newline splits lines ------------------------------------------------------------------------------- */
static void newline_body(IDWriteFactory *f, IDWriteTextFormat *fmt, const char *tag) {
    static const struct { const WCHAR *t; UINT32 len, lines, nl0; } rows[] = {
        { L"ab\ncd", 5, 2, 1 }, { L"ab\r\ncd", 6, 2, 2 }, { L"ab\rcd", 5, 2, 1 }, { L"a\nb\nc", 5, 3, 1 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextLayout *l = NULL; DWRITE_LINE_METRICS lm[4]; UINT32 n = 0;
        CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, rows[i].t, rows[i].len, fmt, 1000.0f, 1000.0f, &l));
        if (!l) continue;
        CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, lm, 4, &n));
        CHECKF(n == rows[i].lines, "%s row %u: %u lines", tag, i, n);
        if (n == rows[i].lines) {
            CHECKF(lm[0].newlineLength == rows[i].nl0, "row %u newlineLength %u", i, lm[0].newlineLength);
            CHECKF(lm[n - 1].newlineLength == 0, "row %u last line newlineLength %u", i, lm[n - 1].newlineLength);
            CHECK(lm[0].length == (i == 1 ? 4u : 3u) || i == 3);
        }
        DW_RELEASE(l);
    }
}
FTO_BOTH(fto_x_newline_splits_lines) { with_fixture(f, tag, "FtoNewline", 20.0f, newline_body); }

/* ---- alignment is reflected in metrics.left / top ----------------------------------------------------- */
static void align_body(IDWriteFactory *f, IDWriteTextFormat *fmt, const char *tag) {
    static const struct { DWRITE_TEXT_ALIGNMENT a; float k; } rows[] = {
        { DWRITE_TEXT_ALIGNMENT_LEADING, 0.0f }, { DWRITE_TEXT_ALIGNMENT_TRAILING, 1.0f }, { DWRITE_TEXT_ALIGNMENT_CENTER, 0.5f },
    };
    unsigned i;
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
        CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(fmt, rows[i].a));
        CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, 200.0f, 100.0f, &l));
        if (l && SUCCEEDED(IDWriteTextLayout_GetMetrics(l, &m))) {
            CHECKF(fabs(m.left - rows[i].k * (200.0 - m.width)) < 0.01, "%s align %d: left %g width %g", tag, rows[i].a, m.left, m.width);
            CHECK(fabs(m.width - 3 * ADV(20.0)) < 0.01);
        }
        DW_RELEASE(l);
    }
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(fmt, DWRITE_TEXT_ALIGNMENT_LEADING));
}
FTO_BOTH(fto_x_text_alignment_in_metrics) { with_fixture(f, tag, "FtoAlign", 20.0f, align_body); }

static void para_body(IDWriteFactory *f, IDWriteTextFormat *fmt, const char *tag) {
    static const struct { DWRITE_PARAGRAPH_ALIGNMENT a; float k; } rows[] = {
        { DWRITE_PARAGRAPH_ALIGNMENT_NEAR, 0.0f }, { DWRITE_PARAGRAPH_ALIGNMENT_FAR, 1.0f }, { DWRITE_PARAGRAPH_ALIGNMENT_CENTER, 0.5f },
    };
    unsigned i;
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
        CHECK_HR_OK(IDWriteTextFormat_SetParagraphAlignment(fmt, rows[i].a));
        CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, 200.0f, 300.0f, &l));
        if (l && SUCCEEDED(IDWriteTextLayout_GetMetrics(l, &m))) {
            CHECKF(fabs(m.top - rows[i].k * (300.0 - m.height)) < 0.01, "%s para %d: top %g height %g", tag, rows[i].a, m.top, m.height);
            CHECK(m.height > 0 && m.height < 300.0f);
        }
        DW_RELEASE(l);
    }
    CHECK_HR_OK(IDWriteTextFormat_SetParagraphAlignment(fmt, DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
}
FTO_BOTH(fto_x_paragraph_alignment_in_metrics) { with_fixture(f, tag, "FtoPara", 20.0f, para_body); }

/* ---- NO_WRAP keeps one line wider than the box --------------------------------------------------------- */
static void nowrap_body(IDWriteFactory *f, IDWriteTextFormat *fmt, const char *tag) {
    IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abcdefghij", 10, fmt, 30.0f, 100.0f, &l));
    if (l && SUCCEEDED(IDWriteTextLayout_GetMetrics(l, &m))) {
        CHECK(m.lineCount == 1);
        CHECKF(fabs(m.width - 10 * ADV(20.0)) < 0.01, "%s: width %g", tag, m.width);
        CHECK_FLOAT_EQ(m.layoutWidth, 30.0f);
        CHECK(m.widthIncludingTrailingWhitespace >= m.width);
    }
    DW_RELEASE(l);
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_WRAP));
}
FTO_BOTH(fto_x_no_wrap_overflows_box) { with_fixture(f, tag, "FtoNoWrap", 20.0f, nowrap_body); }

/* ---- more format state copied into the layout ---------------------------------------------------------- */
FTO_BOTH(fto_x_layout_copies_direction_and_trimming) {
    IDWriteTextFormat *fmt = fto_format(f, 14.0f);
    IDWriteTextLayout *l = NULL;
    DWRITE_TRIMMING t = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }, g; IDWriteInlineObject *s = (IDWriteInlineObject *)FTO_SENTINEL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(fmt, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(fmt, &t, NULL));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, 50.0f, 50.0f, &l));
    if (l) {
        CHECK(IDWriteTextLayout_GetReadingDirection(l) == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        memset(&g, 0xAB, sizeof g);
        CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &g, &s));
        CHECK(g.granularity == DWRITE_TRIMMING_GRANULARITY_CHARACTER);
        CHECK(s == NULL);
        /* changing the layout afterwards does not alter the format */
        CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT));
        CHECK(IDWriteTextFormat_GetReadingDirection(fmt) == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_x_layout_locale_is_normalised) {
    static const WCHAR *rows[][2] = { { L"EN-GB", L"en-gb" }, { L"De_DE", L"de_de" }, { L"zh-Hans-CN", L"zh-hans-cn" }};
    unsigned i;
    for (i = 0; i < 3; i++) {
        IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *l = NULL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, rows[i][0], &fmt);
        if (hr != S_OK) { CHECK_HR_OK(hr); continue; }
        if (SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, 50.0f, 50.0f, &l)) && l) {
            WCHAR name[32] = L"zzz"; UINT32 n = 99; DWRITE_TEXT_RANGE r;
            CHECK_HR_OK(IDWriteTextLayout_GetLocaleNameLength(l, 0, &n, &r));
            CHECKF(n == (UINT32)lstrlenW(rows[i][1]), "row %u locale length %u", i, n);
            CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 0, name, 32, &r));
            CHECKF(!lstrcmpW(name, rows[i][1]), "row %u locale name", i);
            IDWriteTextLayout_Release(l);
        }
        IDWriteTextFormat_Release(fmt);
    }
}

/* Many layouts from one format: each independent, all releasable in any order. */
FTO_BOTH(fto_x_many_layouts_one_format) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteTextLayout *ls[32]; unsigned i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < 32; i++) { ls[i] = NULL; CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"hello", 5, fmt, 10.0f + i, 20.0f, &ls[i])); }
    for (i = 0; i < 32; i++) if (ls[i]) {
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(ls[i]), 10.0f + i);
        CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(ls[i], 100.0f + i));
    }
    for (i = 0; i < 32; i++) if (ls[i]) CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(ls[i]), 100.0f + i);
    for (i = 0; i < 32; i++) DW_RELEASE(ls[(i * 7) % 32]);
    EXPECT_REFS(fmt, 1);
    IDWriteTextFormat_Release(fmt);
}
