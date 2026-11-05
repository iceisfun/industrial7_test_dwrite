/* IDWriteFactory::CreateTextFormat: argument validation, getters, defaults, locale normalisation, collections.
 *
 * Facts (Win10 19041 dwrite.dll, DWriteFactory::CreateTextFormat):
 *   - *ppFormat is zeroed first; family==NULL, locale==NULL, weight-1 > 998 (i.e. 0 or >999), style > 2, stretch-1 > 8
 *     (i.e. 0 or >9), size <= 0 or NaN all return E_INVALIDARG before anything is built.
 *   - fontCollection == NULL selects the system collection (the checkForUpdates=FALSE one).
 *   - the locale string is validated and normalised by the format (letters lower-cased, subtags 1..8 chars of letters/digits,
 *     first subtag letters only, '-' or '_' separators); the family name is stored verbatim and never looked up at creation.
 *   - IncrementalTabStop defaults to 4 * fontSize.
 */
#include "fto.h"

static WCHAR *wdup(const WCHAR *s) { size_t n = lstrlenW(s); WCHAR *d = (WCHAR *)malloc((n + 1) * 2); memcpy(d, s, (n + 1) * 2); return d; }

/* ---- success + getter round trip -------------------------------------------------------------------- */
FTO_BOTH(fto_fmt_basic_roundtrip) {
    IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC,
                                                 DWRITE_FONT_STRETCH_CONDENSED, 16.5f, L"en-us", &fmt);
    CHECK_HR_OK(hr);
    if (FAILED(hr) || !fmt) { t_skip("CreateTextFormat failed on %s", tag); return; }
    CHECK(IDWriteTextFormat_GetFontWeight(fmt) == DWRITE_FONT_WEIGHT_BOLD);
    CHECK(IDWriteTextFormat_GetFontStyle(fmt) == DWRITE_FONT_STYLE_ITALIC);
    CHECK(IDWriteTextFormat_GetFontStretch(fmt) == DWRITE_FONT_STRETCH_CONDENSED);
    CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(fmt), 16.5f);
    EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(fmt), 5);
    EXPECT_EQ(IDWriteTextFormat_GetLocaleNameLength(fmt), 5);
    WCHAR *fam = fto_get_family(fmt), *loc = fto_get_locale(fmt);
    CHECK(fam && !lstrcmpW(fam, L"Arial"));
    CHECK(loc && !lstrcmpW(loc, L"en-us"));
    free(fam); free(loc);
    EXPECT_REFS(fmt, 1);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_fmt_defaults) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    DWRITE_TRIMMING tr; IDWriteInlineObject *sign = (IDWriteInlineObject *)FTO_SENTINEL;
    if (!fmt) { t_skip("no format"); return; }
    /* Documented defaults of a freshly created format. */
    CHECK(IDWriteTextFormat_GetTextAlignment(fmt) == DWRITE_TEXT_ALIGNMENT_LEADING);
    CHECK(IDWriteTextFormat_GetParagraphAlignment(fmt) == DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    CHECK(IDWriteTextFormat_GetWordWrapping(fmt) == DWRITE_WORD_WRAPPING_WRAP);
    CHECK(IDWriteTextFormat_GetReadingDirection(fmt) == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    CHECK(IDWriteTextFormat_GetFlowDirection(fmt) == DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    /* ctor stores fontSize * 4.0 as the incremental tab stop. */
    CHECK_FLOAT_EQ(IDWriteTextFormat_GetIncrementalTabStop(fmt), 48.0f);
    memset(&tr, 0xAB, sizeof tr);
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(fmt, &tr, &sign));
    CHECK(tr.granularity == DWRITE_TRIMMING_GRANULARITY_NONE);
    CHECK(sign == NULL);
    t_note("default trimming delimiter=%u count=%d", tr.delimiter, tr.delimiterCount);
    DWRITE_LINE_SPACING_METHOD m = (DWRITE_LINE_SPACING_METHOD)-1; FLOAT sp = -1, bl = -1;
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(fmt, &m, &sp, &bl));
    CHECK(m == DWRITE_LINE_SPACING_METHOD_DEFAULT);
    t_note("default line spacing=%g baseline=%g", sp, bl);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_fmt_tabstop_scales_with_size) {
    static const float sizes[] = { 1.0f, 8.0f, 10.5f, 24.0f, 100.0f };
    unsigned i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextFormat *fmt = fto_format(f, sizes[i]);
        if (!fmt) { t_skip("no format"); return; }
        CHECK_FLOAT_EQ(IDWriteTextFormat_GetIncrementalTabStop(fmt), sizes[i] * 4.0f);
        IDWriteTextFormat_Release(fmt);
    }
}

/* ---- weight / style / stretch validation ----------------------------------------------------------- */
FTO_BOTH(fto_fmt_weight_table) {
    static const struct { UINT32 w; int ok; } rows[] = {
        { 1, 1 }, { 2, 1 }, { 100, 1 }, { 200, 1 }, { 300, 1 }, { 350, 1 }, { 400, 1 }, { 500, 1 }, { 550, 1 }, { 600, 1 }, { 700, 1 },
        { 800, 1 }, { 900, 1 }, { 950, 1 }, { 998, 1 }, { 999, 1 },
        { 0, 0 }, { 1000, 0 }, { 1001, 0 }, { 1500, 0 }, { 65535, 0 }, { 0x7fffffff, 0 }, { 0x80000000u, 0 }, { 0xffffffffu, 0 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, (DWRITE_FONT_WEIGHT)rows[i].w, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt);
        if (rows[i].ok) {
            CHECKF(hr == S_OK, "weight %u: %s", rows[i].w, dw_hr_name(hr));
            if (hr == S_OK && fmt) { CHECKF((UINT32)IDWriteTextFormat_GetFontWeight(fmt) == rows[i].w, "weight %u round trip", rows[i].w); IDWriteTextFormat_Release(fmt); }
        } else {
            CHECKF(hr == FTO_E_INVALIDARG, "weight %u: %s", rows[i].w, dw_hr_name(hr));
            CHECK_OUT_NULL(fmt);
            if (fmt && fmt != FTO_SENTINEL) IDWriteTextFormat_Release(fmt);
        }
    }
}

FTO_BOTH(fto_fmt_style_table) {
    static const struct { UINT32 s; int ok; } rows[] = {
        { DWRITE_FONT_STYLE_NORMAL, 1 }, { DWRITE_FONT_STYLE_OBLIQUE, 1 }, { DWRITE_FONT_STYLE_ITALIC, 1 },
        { 3, 0 }, { 4, 0 }, { 100, 0 }, { 0x7fffffff, 0 }, { 0xffffffffu, 0 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, (DWRITE_FONT_STYLE)rows[i].s,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt);
        if (rows[i].ok) {
            CHECKF(hr == S_OK, "style %u: %s", rows[i].s, dw_hr_name(hr));
            if (hr == S_OK && fmt) { CHECK((UINT32)IDWriteTextFormat_GetFontStyle(fmt) == rows[i].s); IDWriteTextFormat_Release(fmt); }
        } else {
            CHECKF(hr == FTO_E_INVALIDARG, "style %u: %s", rows[i].s, dw_hr_name(hr));
            CHECK_OUT_NULL(fmt);
            if (fmt && fmt != FTO_SENTINEL) IDWriteTextFormat_Release(fmt);
        }
    }
}

FTO_BOTH(fto_fmt_stretch_table) {
    static const struct { UINT32 s; int ok; } rows[] = {
        { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 1 }, { 5, 1 }, { 6, 1 }, { 7, 1 }, { 8, 1 }, { 9, 1 },
        { 0, 0 }, { 10, 0 }, { 11, 0 }, { 255, 0 }, { 0x7fffffff, 0 }, { 0xffffffffu, 0 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     (DWRITE_FONT_STRETCH)rows[i].s, 12.0f, L"en-us", &fmt);
        if (rows[i].ok) {
            CHECKF(hr == S_OK, "stretch %u: %s", rows[i].s, dw_hr_name(hr));
            if (hr == S_OK && fmt) { CHECK((UINT32)IDWriteTextFormat_GetFontStretch(fmt) == rows[i].s); IDWriteTextFormat_Release(fmt); }
        } else {
            CHECKF(hr == FTO_E_INVALIDARG, "stretch %u: %s", rows[i].s, dw_hr_name(hr));
            CHECK_OUT_NULL(fmt);
            if (fmt && fmt != FTO_SENTINEL) IDWriteTextFormat_Release(fmt);
        }
    }
}

/* Each field validated independently: an invalid one among otherwise-valid fields fails, all valid enum extremes together succeed. */
FTO_BOTH(fto_fmt_enum_extremes_together) {
    IDWriteTextFormat *fmt = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, (DWRITE_FONT_WEIGHT)999, DWRITE_FONT_STYLE_ITALIC,
                                                DWRITE_FONT_STRETCH_ULTRA_EXPANDED, 1.0f, L"en-us", &fmt));
    DW_RELEASE(fmt);
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, (DWRITE_FONT_WEIGHT)1, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_ULTRA_CONDENSED, 1.0f, L"en-us", &fmt));
    DW_RELEASE(fmt);
    CHECK_HR(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, (DWRITE_FONT_WEIGHT)1000, (DWRITE_FONT_STYLE)3,
                                             (DWRITE_FONT_STRETCH)10, 1.0f, L"en-us", &fmt), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(fmt);
}

/* ---- font size ------------------------------------------------------------------------------------------ */
FTO_BOTH(fto_fmt_size_valid_table) {
    static const float v[] = { FLT_MIN, 1e-30f, 0.001f, 0.5f, 1.0f, 8.0f, 12.0f, 12.34567f, 72.0f, 1000.0f, 65536.0f, 1e6f };
    unsigned i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, v[i], L"en-us", &fmt);
        CHECKF(hr == S_OK, "size %g: %s", v[i], dw_hr_name(hr));
        if (hr == S_OK && fmt) { CHECKF(IDWriteTextFormat_GetFontSize(fmt) == v[i], "size %g round trip", v[i]); IDWriteTextFormat_Release(fmt); }
    }
}

FTO_BOTH(fto_fmt_size_invalid_table) {
    static const float v[] = { 0.0f, -0.0f, -1.0f, -12.0f, -0.001f, -FLT_MIN, -1e30f, -FLT_MAX, -INFINITY, NAN, -NAN };
    unsigned i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, v[i], L"en-us", &fmt);
        CHECKF(hr == FTO_E_INVALIDARG, "size %g: %s", v[i], dw_hr_name(hr));
        CHECK_OUT_NULL(fmt);
        if (fmt && fmt != FTO_SENTINEL) IDWriteTextFormat_Release(fmt);
    }
}

FTO_BOTH(fto_fmt_size_infinity_and_max) {
    /* +INF and FLT_MAX pass the argument check (size <= 0 / NaN only); what the constructor does with them is not specified. */
    static const float v[] = { INFINITY, FLT_MAX };
    unsigned i;
    for (i = 0; i < 2; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, v[i], L"en-us", &fmt);
        t_note("%s: size %g -> %s", tag, v[i], dw_hr_name(hr));
        if (SUCCEEDED(hr)) { CHECK(fmt != NULL); if (fmt) { CHECK(IDWriteTextFormat_GetFontSize(fmt) == v[i]); IDWriteTextFormat_Release(fmt); } }
        else CHECK_OUT_NULL(fmt);
    }
}

/* ---- NULL arguments ------------------------------------------------------------------------------------- */
FTO_BOTH(fto_fmt_null_family) {
    IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateTextFormat(f, NULL, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(fmt);
}

FTO_BOTH(fto_fmt_null_locale) {
    IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL, 12.0f, NULL, &fmt), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(fmt);
}

FTO_BOTH(fto_fmt_null_family_and_locale_both) {
    IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateTextFormat(f, NULL, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL, 12.0f, NULL, &fmt), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(fmt);
}

FTO_BOTH(fto_fmt_every_invalid_arg_zeroes_output) {
    IDWriteTextFormat *fmt;
    struct { const WCHAR *fam, *loc; UINT32 w, s, st; float sz; } rows[] = {
        { NULL, L"en-us", 400, 0, 5, 12 }, { L"A", NULL, 400, 0, 5, 12 }, { L"A", L"en-us", 0, 0, 5, 12 }, { L"A", L"en-us", 1000, 0, 5, 12 },
        { L"A", L"en-us", 400, 3, 5, 12 }, { L"A", L"en-us", 400, 0, 0, 12 }, { L"A", L"en-us", 400, 0, 10, 12 },
        { L"A", L"en-us", 400, 0, 5, 0 }, { L"A", L"en-us", 400, 0, 5, -3 }, { L"A", L"en-us", 400, 0, 5, NAN },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, rows[i].fam, NULL, (DWRITE_FONT_WEIGHT)rows[i].w, (DWRITE_FONT_STYLE)rows[i].s,
                                                     (DWRITE_FONT_STRETCH)rows[i].st, rows[i].sz, rows[i].loc, &fmt);
        CHECKF(hr == FTO_E_INVALIDARG, "row %u: %s", i, dw_hr_name(hr));
        CHECKF(fmt == NULL, "row %u: out parameter %p", i, (void *)fmt);
    }
}

/* ---- family names ----------------------------------------------------------------------------------------- */
FTO_BOTH(fto_fmt_empty_family) {
    IDWriteTextFormat *fmt = NULL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt);
    CHECK_HR_OK(hr);
    if (!fmt) return;
    /* The name is stored verbatim; nothing is resolved at creation. */
    EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(fmt), 0);
    WCHAR buf[4] = { 0x1234, 0x1234, 0x1234, 0x1234 };
    CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(fmt, buf, 4));
    CHECK(buf[0] == 0);
    CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(fmt, buf, 1));
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_fmt_nonexistent_family_succeeds) {
    static const WCHAR *names[] = { L"NoSuchFamily_zz9", L"  leading and trailing  ", L"\x00e9\x00e8\x4e2d\x6587", L"a,b;c/d\\e", L"Arial Bold Italic Condensed" };
    unsigned i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, names[i], NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt);
        CHECKF(hr == S_OK, "family %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && fmt) {
            WCHAR *fam = fto_get_family(fmt);
            CHECKF(fam && !lstrcmpW(fam, names[i]), "family %u stored verbatim", i);
            free(fam);
            IDWriteTextFormat_Release(fmt);
        }
    }
}

FTO_BOTH(fto_fmt_long_family_names) {
    static const size_t lens[] = { 31, 32, 254, 255, 256, 257, 511, 1000, 4096, 32767 };
    unsigned i;
    for (i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        WCHAR *name = fto_repeat('F', lens[i]);
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, name, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt);
        /* The constructor copies the string with no length limit. */
        CHECKF(hr == S_OK, "family length %zu: %s", lens[i], dw_hr_name(hr));
        if (hr == S_OK && fmt) {
            CHECKF(IDWriteTextFormat_GetFontFamilyNameLength(fmt) == lens[i], "length %zu", lens[i]);
            WCHAR *got = fto_get_family(fmt);
            CHECKF(got && !lstrcmpW(got, name), "content of length %zu", lens[i]);
            free(got);
            IDWriteTextFormat_Release(fmt);
        }
        free(name);
    }
}

FTO_BOTH(fto_fmt_family_string_is_copied) {
    WCHAR buf[16];
    IDWriteTextFormat *fmt = NULL;
    lstrcpyW(buf, L"Original");
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, buf, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt));
    lstrcpyW(buf, L"Clobbered");
    if (!fmt) return;
    WCHAR *fam = fto_get_family(fmt);
    CHECK(fam && !lstrcmpW(fam, L"Original"));
    free(fam);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_fmt_locale_string_is_copied) {
    WCHAR buf[16];
    IDWriteTextFormat *fmt = NULL;
    lstrcpyW(buf, L"fr-fr");
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 12.0f, buf, &fmt));
    lstrcpyW(buf, L"de-de");
    if (!fmt) return;
    WCHAR *loc = fto_get_locale(fmt);
    CHECK(loc && !lstrcmpW(loc, L"fr-fr"));
    free(loc);
    IDWriteTextFormat_Release(fmt);
}

/* ---- locale handling (LanguageTagImpl::ValidateAndNormalize) --------------------------------------------- */
FTO_BOTH(fto_fmt_locale_accepted_and_normalised) {
    /* in -> out: letters are lower-cased in every subtag, '-' and '_' separators are kept, digits only after the first subtag. */
    static const WCHAR *rows[][2] = {
        { L"en-us", L"en-us" }, { L"EN-US", L"en-us" }, { L"En-Us", L"en-us" }, { L"en-US", L"en-us" }, { L"en_US", L"en_us" },
        { L"en", L"en" }, { L"EN", L"en" }, { L"a", L"a" }, { L"Z", L"z" }, { L"i-klingon", L"i-klingon" },
        { L"xx-invalid", L"xx-invalid" }, { L"xx-INVALID", L"xx-invalid" }, { L"zz", L"zz" },
        { L"zh-Hans-CN", L"zh-hans-cn" }, { L"sr-Latn-RS", L"sr-latn-rs" }, { L"de-DE-1996", L"de-de-1996" },
        { L"en-US-x-private", L"en-us-x-private" }, { L"ja-JP-u-ca-japanese", L"ja-jp-u-ca-japanese" },
        { L"abcdefgh", L"abcdefgh" }, { L"ABCDEFGH", L"abcdefgh" }, { L"en-abcdefgh", L"en-abcdefgh" }, { L"en-12345678", L"en-12345678" },
        { L"en-a1", L"en-a1" }, { L"en-1", L"en-1" }, { L"en-u", L"en-u" }, { L"en-US-POSIX", L"en-us-posix" },
        { L"a-b-c-d-e-f-g-h", L"a-b-c-d-e-f-g-h" }, { L"en-x-a_b", L"en-x-a_b" }, { L"en_us_posix", L"en_us_posix" },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, rows[i][0], &fmt);
        CHECKF(hr == S_OK, "locale %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && fmt) {
            WCHAR *loc = fto_get_locale(fmt);
            CHECKF(loc && !lstrcmpW(loc, rows[i][1]), "locale row %u normalised form", i);
            if (loc && lstrcmpW(loc, rows[i][1])) t_note("row %u: in %ls got %ls want %ls", i, rows[i][0], loc, rows[i][1]);
            EXPECT_EQ(IDWriteTextFormat_GetLocaleNameLength(fmt), lstrlenW(rows[i][1]));
            free(loc);
            IDWriteTextFormat_Release(fmt);
        }
    }
}

FTO_BOTH(fto_fmt_locale_rejected) {
    static const WCHAR *rows[] = {
        L"abcdefghi",               /* first subtag longer than 8 */
        L"en-abcdefghi",            /* later subtag longer than 8 */
        L"en-",                     /* empty trailing subtag */
        L"en--us",                  /* empty middle subtag */
        L"-en", L"_en", L"-",       /* leading separator / empty first subtag */
        L"en-us-",
        L"en_",
        L"1en", L"e1", L"12", L"1",   /* digits in the first subtag */
        L"en us", L" en", L"en ",   /* whitespace */
        L"en.us", L"en+us", L"en/us", L"en@us", L"en:us", L"en;us", L"en,us",
        L"\x00e9n-us", L"en-\x00fc", L"\x4e2d\x6587", /* non-ASCII */
        L"en-us\n", L"en\t-us",
        L"en-123456789",
        L"a-b-c-d-e-f-g-h-",
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, rows[i], &fmt);
        CHECKF(hr == FTO_E_INVALIDARG, "locale row %u (%ls): %s", i, rows[i], dw_hr_name(hr));
        CHECKF(fmt == NULL, "locale row %u out parameter %p", i, (void *)fmt);
        if (fmt && fmt != FTO_SENTINEL) IDWriteTextFormat_Release(fmt);
    }
}

FTO_BOTH(fto_fmt_empty_locale) {
    IDWriteTextFormat *fmt = NULL;
    /* An empty locale is not parsed at all (Initialize returns early for length 0). */
    HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"", &fmt);
    CHECK_HR_OK(hr);
    if (!fmt) return;
    EXPECT_EQ(IDWriteTextFormat_GetLocaleNameLength(fmt), 0);
    WCHAR b[2] = { 0x77, 0x77 };
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(fmt, b, 2));
    CHECK(b[0] == 0);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_fmt_long_valid_locale) {
    static const size_t lens[] = { 2, 10, 11, 100, 254, 255, 256, 257, 400, 1000 };
    unsigned i;
    for (i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        WCHAR *loc = fto_long_locale(lens[i]);
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, loc, &fmt);
        /* The language tag has no overall length limit, only per-subtag limits. */
        CHECKF(hr == S_OK, "locale length %zu: %s", lens[i], dw_hr_name(hr));
        if (hr == S_OK && fmt) {
            CHECKF(IDWriteTextFormat_GetLocaleNameLength(fmt) == lens[i], "locale length %zu", lens[i]);
            WCHAR *got = fto_get_locale(fmt);
            CHECKF(got && !lstrcmpW(got, loc), "locale content length %zu", lens[i]);
            free(got);
            IDWriteTextFormat_Release(fmt);
        }
        free(loc);
    }
}

FTO_BOTH(fto_fmt_long_invalid_locale) {
    static const size_t lens[] = { 9, 255, 256, 1000 };
    unsigned i;
    for (i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        WCHAR *loc = fto_repeat('a', lens[i]);     /* one subtag, longer than 8 letters */
        IDWriteTextFormat *fmt = (IDWriteTextFormat *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 12.0f, loc, &fmt);
        CHECKF(hr == FTO_E_INVALIDARG, "locale 'a' x %zu: %s", lens[i], dw_hr_name(hr));
        CHECK_OUT_NULL(fmt);
        free(loc);
    }
}

FTO_BOTH(fto_fmt_locale_is_not_checked_against_os) {
    static const WCHAR *rows[] = { L"qq", L"zz-zz", L"xx-yy-zz", L"x-unknown", L"en-aaaaaaaa" };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextFormat *fmt = NULL;
        CHECKF(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, 12.0f, rows[i], &fmt) == S_OK, "locale %ls", rows[i]);
        DW_RELEASE(fmt);
    }
}

/* ---- buffer protocols of GetFontFamilyName / GetLocaleName ---------------------------------------------- */
FTO_BOTH(fto_fmt_getname_buffer_protocol) {
    IDWriteTextFormat *fmt = NULL;
    WCHAR buf[32];
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Tahoma", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt));
    if (!fmt) return;
    /* size 0 -> E_NOT_SUFFICIENT_BUFFER, buffer untouched */
    buf[0] = 0x5a5a;
    CHECK_HR(IDWriteTextFormat_GetFontFamilyName(fmt, buf, 0), FTO_E_NOT_SUFFICIENT_BUFFER);
    CHECK(buf[0] == 0x5a5a);
    buf[0] = 0x5a5a;
    CHECK_HR(IDWriteTextFormat_GetLocaleName(fmt, buf, 0), FTO_E_NOT_SUFFICIENT_BUFFER);
    CHECK(buf[0] == 0x5a5a);
    /* exactly the string length (no room for the NUL) -> insufficient; the copy is truncated and NUL terminated */
    CHECK_HR(IDWriteTextFormat_GetFontFamilyName(fmt, buf, 6), FTO_E_NOT_SUFFICIENT_BUFFER);
    CHECK(buf[5] == 0);
    CHECK_HR(IDWriteTextFormat_GetLocaleName(fmt, buf, 5), FTO_E_NOT_SUFFICIENT_BUFFER);
    CHECK(buf[4] == 0);
    CHECK_HR(IDWriteTextFormat_GetFontFamilyName(fmt, buf, 1), FTO_E_NOT_SUFFICIENT_BUFFER);
    CHECK(buf[0] == 0);
    /* length + 1 -> success */
    memset(buf, 0x55, sizeof buf);
    CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(fmt, buf, 7));
    CHECK(!lstrcmpW(buf, L"Tahoma"));
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(fmt, buf, 6));
    CHECK(!lstrcmpW(buf, L"en-us"));
    /* larger buffer is fine too */
    CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(fmt, buf, 32));
    CHECK(!lstrcmpW(buf, L"Tahoma"));
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(fmt, buf, 32));
    CHECK(!lstrcmpW(buf, L"en-us"));
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_fmt_getname_null_buffer) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    if (!fmt) { t_skip("no format"); return; }
    if (t_is_wine()) { t_skip("NULL destination buffer crashes the Wine implementation"); IDWriteTextFormat_Release(fmt); return; }
    /* The copy helpers throw ArgumentException for a NULL destination before looking at the size. */
    CHECK_HR(IDWriteTextFormat_GetFontFamilyName(fmt, NULL, 10), FTO_E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_GetLocaleName(fmt, NULL, 10), FTO_E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_GetFontFamilyName(fmt, NULL, 0), FTO_E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_GetLocaleName(fmt, NULL, 0), FTO_E_INVALIDARG);
    IDWriteTextFormat_Release(fmt);
}

/* ---- font collection argument ------------------------------------------------------------------------------- */
FTO_BOTH(fto_fmt_null_collection_is_system_collection) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    IDWriteFontCollection *fc = NULL, *sys = NULL;
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(fmt, &fc));
    CHECK(fc != NULL);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE));
    CHECK(sys != NULL);
    if (fc && sys) CHECK(dw_same_identity((IUnknown *)fc, (IUnknown *)sys));
    DW_RELEASE(fc); DW_RELEASE(sys);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_fmt_explicit_system_collection_roundtrip) {
    IDWriteTextFormat *fmt = NULL;
    IDWriteFontCollection *sys = NULL, *got = NULL;
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE));
    if (!sys) return;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", sys, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fmt));
    if (fmt) {
        CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(fmt, &got));
        CHECK(got == sys);
        DW_RELEASE(got);
        IDWriteTextFormat_Release(fmt);
    }
    IDWriteFontCollection_Release(sys);
}

FTO_BOTH(fto_fmt_custom_collection_roundtrip_and_refs) {
    fto_coll fc;
    fto_coll_setup(&fc, f, "FtoFmtFam");
    if (!fc.coll) { t_skip("custom collection unavailable: %s", dw_hr_name(fc.hr)); fto_coll_teardown(&fc); return; }
    {
        IDWriteTextFormat *fmt = NULL;
        IDWriteFontCollection *got = NULL;
        ULONG before = dw_refcount((IUnknown *)fc.coll), during, after;
        CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, fc.family, fc.coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                    DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &fmt));
        if (fmt) {
            during = dw_refcount((IUnknown *)fc.coll);
            CHECKF(during > before, "format holds a reference on its collection: %lu -> %lu", before, during);
            CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(fmt, &got));
            CHECK(got == fc.coll);
            DW_RELEASE(got);
            IDWriteTextFormat_Release(fmt);
            after = dw_refcount((IUnknown *)fc.coll);
            CHECKF(after == before, "collection refcount restored after format release: %lu vs %lu", after, before);
        }
    }
    fto_coll_teardown(&fc);
}

FTO_BOTH(fto_fmt_collection_survives_caller_release) {
    fto_coll fc;
    fto_coll_setup(&fc, f, "FtoFmtFam2");
    if (!fc.coll) { t_skip("custom collection unavailable"); fto_coll_teardown(&fc); return; }
    IDWriteTextFormat *fmt = NULL;
    IDWriteFontCollection *got = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, fc.family, fc.coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &fmt));
    IDWriteFontCollection *raw = fc.coll;
    fc.coll = NULL;
    IDWriteFontCollection_Release(raw);          /* the format keeps the collection alive */
    if (fmt) {
        CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(fmt, &got));
        CHECK(got == raw);
        if (got) CHECK(IDWriteFontCollection_GetFontFamilyCount(got) >= 1);
        DW_RELEASE(got);
        IDWriteTextFormat_Release(fmt);
    }
    fto_coll_teardown(&fc);
}

FTO_BOTH(fto_fmt_two_formats_share_system_collection) {
    IDWriteTextFormat *a = fto_format(f, 10.0f), *b = fto_format(f, 30.0f);
    IDWriteFontCollection *ca = NULL, *cb = NULL;
    if (!a || !b) { t_skip("no format"); DW_RELEASE(a); DW_RELEASE(b); return; }
    IDWriteTextFormat_GetFontCollection(a, &ca);
    IDWriteTextFormat_GetFontCollection(b, &cb);
    CHECK(ca != NULL && ca == cb);
    DW_RELEASE(ca); DW_RELEASE(cb); DW_RELEASE(a); DW_RELEASE(b);
}

/* ---- object model ------------------------------------------------------------------------------------------------ */
FTO_BOTH(fto_fmt_qi_and_refcount) {
    IDWriteTextFormat *fmt = fto_format(f, 12.0f);
    if (!fmt) { t_skip("no format"); return; }
    dw_qi_row rows[] = {
        DW_QI_ROW(IDWriteTextFormat, 1), DW_QI_ROW(IDWriteTextFormat1, -1), DW_QI_ROW(IDWriteTextFormat2, -1), DW_QI_ROW(IDWriteTextFormat3, -1),
        DW_QI_ROW(IDWriteTextLayout, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontCollection, 0),
        DW_QI_ROW(IDWriteTypography, 0), DW_QI_ROW(IDWriteInlineObject, 0), DW_QI_ROW(IDWriteNumberSubstitution, 0), DW_QI_ROW(IDWriteFontFace, 0),
    };
    DW_CHECK_QI(fmt, rows);
    EXPECT_REFS(fmt, 1);
    IDWriteTextFormat_AddRef(fmt);
    EXPECT_REFS(fmt, 2);
    IDWriteTextFormat_Release(fmt);
    EXPECT_REFS(fmt, 1);
    IDWriteTextFormat_Release(fmt);
}

FTO_BOTH(fto_fmt_distinct_objects_per_call) {
    IDWriteTextFormat *a = fto_format(f, 12.0f), *b = fto_format(f, 12.0f);
    if (!a || !b) { t_skip("no format"); DW_RELEASE(a); DW_RELEASE(b); return; }
    CHECK(a != b);
    /* Mutating one format must not affect the other. */
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(a, DWRITE_TEXT_ALIGNMENT_CENTER));
    CHECK(IDWriteTextFormat_GetTextAlignment(a) == DWRITE_TEXT_ALIGNMENT_CENTER);
    CHECK(IDWriteTextFormat_GetTextAlignment(b) == DWRITE_TEXT_ALIGNMENT_LEADING);
    IDWriteTextFormat_Release(a); IDWriteTextFormat_Release(b);
}

FTO_BOTH(fto_fmt_many_formats_stress) {
    enum { N = 200 };
    IDWriteTextFormat *v[N];
    int i;
    for (i = 0; i < N; i++) {
        v[i] = NULL;
        CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                    DWRITE_FONT_STRETCH_NORMAL, 8.0f + (float)i, L"en-us", &v[i]));
    }
    for (i = 0; i < N; i++) if (v[i]) { CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(v[i]), 8.0f + (float)i); IDWriteTextFormat_Release(v[i]); }
}

/* ---- the format outlives the factory that made it ------------------------------------------------------------------ */
TEST(fto_fmt_outlives_isolated_factory) {
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteTextFormat *fmt = NULL;
    if (!iso) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(iso, L"Arial", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 33.0f, L"de-de", &fmt));
    IDWriteFactory_Release(iso);
    if (!fmt) return;
    CHECK(IDWriteTextFormat_GetFontWeight(fmt) == DWRITE_FONT_WEIGHT_BOLD);
    CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(fmt), 33.0f);
    WCHAR *loc = fto_get_locale(fmt);
    CHECK(loc && !lstrcmpW(loc, L"de-de"));
    free(loc);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(fmt, &c));
    CHECK(c != NULL);
    if (c) CHECK(IDWriteFontCollection_GetFontFamilyCount(c) > 0);
    DW_RELEASE(c);
    IDWriteTextFormat_Release(fmt);
}

TEST(fto_fmt_two_isolated_factories_have_independent_system_collections) {
    IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *b = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteTextFormat *fa = NULL, *fb = NULL;
    IDWriteFontCollection *ca = NULL, *cb = NULL;
    if (!a || !b) { t_skip("no factories"); DW_RELEASE(a); DW_RELEASE(b); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(a, L"Arial", NULL, 400, 0, 5, 12, L"en-us", &fa));
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(b, L"Arial", NULL, 400, 0, 5, 12, L"en-us", &fb));
    if (fa && fb) {
        IDWriteTextFormat_GetFontCollection(fa, &ca);
        IDWriteTextFormat_GetFontCollection(fb, &cb);
        CHECK(ca && cb);
        if (ca && cb) CHECK(ca != cb);
    }
    DW_RELEASE(ca); DW_RELEASE(cb); DW_RELEASE(fa);
    IDWriteFactory_Release(a);               /* b's objects keep working after a is gone */
    if (fb) {
        CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(fb), 12.0f);
        IDWriteFontCollection *c2 = NULL;
        CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(fb, &c2));
        if (c2) CHECK(IDWriteFontCollection_GetFontFamilyCount(c2) > 0);
        DW_RELEASE(c2);
        IDWriteTextFormat_Release(fb);
    }
    IDWriteFactory_Release(b);
}

/* ---- IDWriteFactory6::CreateTextFormat (font axes), only on builds exposing the interface ---------------------------- */
TEST(fto_fmt_axes_validation) {
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 75.0f } };
    IDWriteTextFormat3 *fmt;
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    /* family NULL, locale NULL, size <= 0 / NaN, count > 0xFFFF, count > 0 with NULL values: E_INVALIDARG with output zeroed. */
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, NULL, NULL, ax, 2, 12.0f, L"en-us", &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, ax, 2, 12.0f, NULL, &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, ax, 2, 0.0f, L"en-us", &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, ax, 2, -4.0f, L"en-us", &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, ax, 2, NAN, L"en-us", &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, ax, 0x10000, 12.0f, L"en-us", &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, ax, 0xffffffffu, 12.0f, L"en-us", &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
    if (t_is_wine()) { t_skip("NULL axis array with a non-zero count crashes the Wine implementation"); return; }
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 1, 12.0f, L"en-us", &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 0xffff, 12.0f, L"en-us", &fmt), FTO_E_INVALIDARG); CHECK_OUT_NULL(fmt);
}

TEST(fto_fmt_axes_success_roundtrip) {
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 650.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 80.0f } }, out[2];
    IDWriteTextFormat3 *fmt = NULL;
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Bahnschrift", NULL, ax, 2, 18.0f, L"en-us", &fmt));
    if (!fmt) return;
    CHECK_FLOAT_EQ(IDWriteTextFormat3_GetFontSize(fmt), 18.0f);
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(fmt), 2);
    memset(out, 0, sizeof out);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(fmt, out, 2));
    CHECK(out[0].axisTag == ax[0].axisTag && out[0].value == ax[0].value);
    CHECK(out[1].axisTag == ax[1].axisTag && out[1].value == ax[1].value);
    /* GetFontAxisValues zero-fills the caller's array first and always returns S_OK, also for a smaller count. */
    out[0].axisTag = (DWRITE_FONT_AXIS_TAG)0x1234; out[0].value = 7.0f;
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(fmt, out, 1));
    IDWriteTextFormat3_Release(fmt);
}

TEST(fto_fmt_axes_zero_count_allows_null_values) {
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    IDWriteTextFormat3 *fmt = NULL;
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 0, 12.0f, L"en-us", &fmt));
    if (!fmt) return;
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(fmt), 0);
    /* The axes constructor fixes weight 400, style normal, stretch normal. */
    CHECK(IDWriteTextFormat3_GetFontWeight(fmt) == DWRITE_FONT_WEIGHT_NORMAL);
    CHECK(IDWriteTextFormat3_GetFontStyle(fmt) == DWRITE_FONT_STYLE_NORMAL);
    CHECK(IDWriteTextFormat3_GetFontStretch(fmt) == DWRITE_FONT_STRETCH_NORMAL);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(IDWriteTextFormat3_GetFontCollection(fmt, &c));
    CHECK(c != NULL);
    DW_RELEASE(c);
    IDWriteTextFormat3_Release(fmt);
}

TEST(fto_fmt_axes_locale_rules_same_as_plain) {
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    IDWriteTextFormat3 *fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 0, 12.0f, L"EN-US", &fmt));
    if (fmt && fmt != FTO_SENTINEL) {
        WCHAR *loc = fto_get_locale((IDWriteTextFormat *)fmt);
        CHECK(loc && !lstrcmpW(loc, L"en-us"));
        free(loc);
        IDWriteTextFormat3_Release(fmt);
    }
    fmt = (IDWriteTextFormat3 *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 0, 12.0f, L"en--us", &fmt), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(fmt);
}
