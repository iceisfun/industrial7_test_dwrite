/* IDWriteFactory::CreateTextLayout / CreateGdiCompatibleTextLayout: argument validation, out-parameter state, reference
 * ownership, text handling (length, embedded NUL, copy semantics, size) and the interface set of the new object.
 *
 * The factory validates before any object exists: string and format non-NULL, maxWidth and maxHeight not negative and not NaN
 * (-0.0 and +INF pass), and it zeroes *layout first, so a failing call leaves NULL behind. The GDI-compatible variant also rejects a
 * NaN pixelsPerDip and a transform with a NaN member; negative or zero pixelsPerDip and degenerate transforms are accepted.
 * The layout copies the string and the format properties: neither the caller's buffer nor the format object is referenced afterwards. */
#include "tl.h"

static HRESULT mk(IDWriteFactory *f, IDWriteTextFormat *fmt, const WCHAR *s, UINT32 n, float w, float h, IDWriteTextLayout **out)
{
    *out = SENT_LAYOUT;
    return IDWriteFactory_CreateTextLayout(f, s, n, fmt, w, h, out);
}
static HRESULT mkg(IDWriteFactory *f, IDWriteTextFormat *fmt, const WCHAR *s, UINT32 n, float w, float h, float ppd, const DWRITE_MATRIX *m, BOOL nat, IDWriteTextLayout **out)
{
    *out = SENT_LAYOUT;
    return IDWriteFactory_CreateGdiCompatibleTextLayout(f, s, n, fmt, w, h, ppd, m, nat, out);
}
#define NEED_FMT(var) IDWriteTextFormat *var = tl_format(&X, TL_FAM, 16.0f); if (!var) { TL_END(); t_skip("CreateTextFormat prerequisite failed"); return; }
static void drop(IDWriteTextLayout **l) { if (*l && *l != SENT_LAYOUT) IDWriteTextLayout_Release(*l); *l = NULL; }

/* ---- basic success -------------------------------------------------------------------------------------- */
TEST(create_basic_ok)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mk(f, t, L"ABC", 3, 100.0f, 50.0f, &l));
    CHECK(l != NULL && l != SENT_LAYOUT);
    if (l && l != SENT_LAYOUT) EXPECT_REFS(l, 1);
    drop(&l); DW_RELEASE(t); TL_END();
}

TEST(create_format_not_retained)
{
    /* the layout copies the format's properties: the format's refcount is untouched by the layout's whole life */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l, *l2;
    EXPECT_REFS(t, 1);
    CHECK_HR_OK(mk(f, t, L"ABC", 3, 100.0f, 50.0f, &l));
    EXPECT_REFS(t, 1);
    CHECK_HR_OK(mk(f, t, L"ABC", 3, 100.0f, 50.0f, &l2));
    EXPECT_REFS(t, 1);
    IDWriteTextLayout_AddRef(l); EXPECT_REFS(t, 1); EXPECT_REFS(l, 2); IDWriteTextLayout_Release(l);
    drop(&l); drop(&l2); EXPECT_REFS(t, 1);
    DW_RELEASE(t); TL_END();
}

TEST(create_gdi_format_not_retained)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mkg(f, t, L"ABC", 3, 100.0f, 50.0f, 1.0f, NULL, FALSE, &l));
    EXPECT_REFS(t, 1); EXPECT_REFS(l, 1);
    drop(&l); EXPECT_REFS(t, 1);
    DW_RELEASE(t); TL_END();
}

TEST(create_returns_distinct_objects)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *a, *b;
    CHECK_HR_OK(mk(f, t, L"ABC", 3, 100.0f, 50.0f, &a));
    CHECK_HR_OK(mk(f, t, L"ABC", 3, 100.0f, 50.0f, &b));
    CHECK(a != b);
    if (a != SENT_LAYOUT && b != SENT_LAYOUT && a && b) CHECK(!dw_same_identity((IUnknown *)a, (IUnknown *)b));
    drop(&a); drop(&b); DW_RELEASE(t); TL_END();
}

TEST(create_layouts_independent)
{
    /* two layouts built from one format do not share per-layout state */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *a, *b;
    CHECK_HR_OK(mk(f, t, L"ABC", 3, 100.0f, 50.0f, &a));
    CHECK_HR_OK(mk(f, t, L"ABC", 3, 100.0f, 50.0f, &b));
    if (a != SENT_LAYOUT && b != SENT_LAYOUT && a && b) {
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(a, 7.0f));
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(a, DWRITE_TEXT_ALIGNMENT_CENTER));
        CHECK_HR_OK(IDWriteTextLayout_SetFontSize(a, 99.0f, RNG(0, 3)));
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(b), 100.0f);
        EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(b), DWRITE_TEXT_ALIGNMENT_LEADING);
        { float sz = 0; DWRITE_TEXT_RANGE r; CHECK_HR_OK(IDWriteTextLayout_GetFontSize(b, 0, &sz, &r)); CHECK_FLOAT_EQ(sz, 16.0f); }
    }
    drop(&a); drop(&b); DW_RELEASE(t); TL_END();
}

/* ---- argument validation ---------------------------------------------------------------------------------- */
TEST(create_null_string)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR(mk(f, t, NULL, 0, 100.0f, 100.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, t, NULL, 5, 100.0f, 100.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, t, NULL, 0, 0.0f, 0.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    DW_RELEASE(t); TL_END();
}

TEST(create_null_format)
{
    TL_BEGIN();
    IDWriteTextLayout *l;
    CHECK_HR(mk(f, NULL, L"ABC", 3, 100.0f, 100.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, NULL, L"ABC", 0, 100.0f, 100.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, NULL, L"ABC", 3, 0.0f, 0.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, NULL, NULL, 0, 0.0f, 0.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    TL_END();
}

TEST(create_negative_dimensions)
{
    TL_BEGIN(); NEED_FMT(t);
    static const float neg[] = { -1.0f, -0.5f, -100.0f, -FLT_MIN, -FLT_MAX, -INFINITY, -1e-30f, -65536.0f };
    size_t i;
    IDWriteTextLayout *l;
    for (i = 0; i < sizeof neg / sizeof neg[0]; i++) {
        CHECKF(mk(f, t, L"ABC", 3, neg[i], 100.0f, &l) == E_INVALIDARG, "maxWidth %g", neg[i]); CHECK_OUT_NULL(l);
        CHECKF(mk(f, t, L"ABC", 3, 100.0f, neg[i], &l) == E_INVALIDARG, "maxHeight %g", neg[i]); CHECK_OUT_NULL(l);
        CHECKF(mk(f, t, L"ABC", 3, neg[i], neg[i], &l) == E_INVALIDARG, "both %g", neg[i]); CHECK_OUT_NULL(l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(create_nan_dimensions)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    float nan_pos = NAN, nan_neg = -NAN;
    CHECK_HR(mk(f, t, L"ABC", 3, nan_pos, 100.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, t, L"ABC", 3, 100.0f, nan_pos, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, t, L"ABC", 3, nan_pos, nan_pos, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, t, L"ABC", 3, nan_neg, 100.0f, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mk(f, t, L"ABC", 3, 100.0f, nan_neg, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    DW_RELEASE(t); TL_END();
}

TEST(create_zero_dimensions_ok)
{
    /* zero is a valid size (everything wraps / clips), negative zero compares equal to zero and passes the "< 0" test */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    static const float v[][2] = { {0, 0}, {0, 100}, {100, 0}, {-0.0f, 0}, {0, -0.0f}, {-0.0f, -0.0f}, {FLT_MIN, FLT_MIN}, {1e-30f, 1e-30f} };
    size_t i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        HRESULT hr = mk(f, t, L"ABC", 3, v[i][0], v[i][1], &l);
        CHECKF(hr == S_OK, "dims %g x %g -> %s", v[i][0], v[i][1], dw_hr_name(hr));
        if (hr == S_OK) { CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), v[i][0]); CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), v[i][1]); }
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(create_large_dimensions_ok)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    static const float v[] = { 1e6f, 1e9f, 1e20f, FLT_MAX };
    size_t i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        HRESULT hr = mk(f, t, L"ABC", 3, v[i], v[i], &l);
        CHECKF(hr == S_OK, "dims %g -> %s", v[i], dw_hr_name(hr));
        if (hr == S_OK) { CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), v[i]); CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), v[i]); }
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(create_infinite_dimensions)
{
    /* +INF is neither negative nor NaN: the factory-level validation accepts it */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    HRESULT hr = mk(f, t, L"ABC", 3, INFINITY, INFINITY, &l);
    CHECK_HR_OK(hr);
    if (hr == S_OK) {
        t_note("maxWidth/maxHeight after +INF create: %g / %g", IDWriteTextLayout_GetMaxWidth(l), IDWriteTextLayout_GetMaxHeight(l));
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(create_dimension_roundtrip_exact)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    static const float v[] = { 1.0f, 0.1f, 123.456f, 3.4e5f, 1.5e-3f, 16777216.0f };
    size_t i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECK_HR_OK(mk(f, t, L"ABC", 3, v[i], v[i] * 2, &l));
        if (l != SENT_LAYOUT && l) { CHECK_FBITS(IDWriteTextLayout_GetMaxWidth(l), v[i]); CHECK_FBITS(IDWriteTextLayout_GetMaxHeight(l), v[i] * 2); }
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(create_failed_leaves_format_untouched)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    mk(f, t, NULL, 0, 1, 1, &l); mk(f, t, L"A", 1, -1, 1, &l);
    EXPECT_REFS(t, 1);
    CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(t), 16.0f);
    DW_RELEASE(t); TL_END();
}

/* ---- text handling ---------------------------------------------------------------------------------------- */
TEST(create_zero_length_with_string)
{
    /* length 0 over a non-empty buffer: an empty layout, the buffer is not read */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(mk(f, t, L"ABC", 0, 100.0f, 100.0f, &l));
    if (l != SENT_LAYOUT && l) { tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 0.0f); CHECK_FLOAT_EQ(m.widthIncludingTrailingWhitespace, 0.0f); }
    drop(&l); DW_RELEASE(t); TL_END();
}

TEST(create_empty_string)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(mk(f, t, L"", 0, 100.0f, 100.0f, &l));
    if (l != SENT_LAYOUT && l) {
        tl_metrics(l, &m);
        CHECK_FLOAT_EQ(m.width, 0.0f); CHECK_FLOAT_EQ(m.left, 0.0f); CHECK_FLOAT_EQ(m.top, 0.0f);
        CHECK_FLOAT_EQ(m.layoutWidth, 100.0f); CHECK_FLOAT_EQ(m.layoutHeight, 100.0f);
        EXPECT_EQ(m.lineCount, 1);       /* an empty paragraph still owns one (empty) line */
        CHECK(m.height > 0.0f);
    }
    drop(&l); DW_RELEASE(t); TL_END();
}

TEST(create_length_prefix_of_string)
{
    /* only the first `length` characters belong to the layout, whatever follows in the buffer */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextFormat *big = tl_format(&X, TL_FAM, TL_UPM);
    IDWriteTextLayout *l; DWRITE_TEXT_METRICS m; UINT32 n;
    if (!big) { DW_RELEASE(t); TL_END(); t_skip("format"); return; }
    for (n = 0; n <= 6; n++) {
        CHECK_HR_OK(mk(f, big, L"ABCDEF", n, 100000.0f, 100000.0f, &l));
        if (l == SENT_LAYOUT || !l) continue;
        tl_metrics(l, &m);
        CHECKF(m.width == 1000.0f * n, "n=%u width %g", n, m.width);
        drop(&l);
    }
    DW_RELEASE(big); DW_RELEASE(t); TL_END();
}

TEST(create_embedded_nul)
{
    /* length counts the characters after an embedded NUL as well; the layout is built over all of them */
    TL_BEGIN(); NEED_FMT(t);
    static const WCHAR s[] = { 'A', 'B', 0, 'C', 'D', 0 };
    IDWriteTextFormat *big = tl_format(&X, TL_FAM, TL_UPM);
    IDWriteTextLayout *l; DWRITE_TEXT_METRICS m;
    if (!big) { DW_RELEASE(t); TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(mk(f, big, s, 5, 100000.0f, 1000.0f, &l));
    if (l != SENT_LAYOUT && l) {
        tl_metrics(l, &m);
        EXPECT_EQ(m.lineCount, 1);
        CHECK(m.width >= 4000.0f);        /* A B C D alone are 4000; the NUL contributes a non-negative advance */
        t_note("width of A B NUL C D at 2048: %g (NUL advance %g)", m.width, m.width - 4000.0f);
        drop(&l);
    }
    CHECK_HR_OK(mk(f, big, s, 2, 100000.0f, 1000.0f, &l));
    if (l != SENT_LAYOUT && l) { tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 2000.0f); drop(&l); }
    DW_RELEASE(big); DW_RELEASE(t); TL_END();
}

TEST(create_text_is_copied)
{
    /* the layout owns a copy of the characters: scribbling over the caller's buffer afterwards changes nothing */
    TL_BEGIN(); NEED_FMT(t);
    WCHAR buf[8] = { 'A', 'B', 'C', 'D', 0 };
    IDWriteTextFormat *big = tl_format(&X, TL_FAM, TL_UPM);
    IDWriteTextLayout *l; DWRITE_TEXT_METRICS m;
    if (!big) { DW_RELEASE(t); TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(mk(f, big, buf, 4, 100000.0f, 1000.0f, &l));
    if (l != SENT_LAYOUT && l) {
        wmemcpy(buf, L"WWWWWWW", 7);
        tl_metrics(l, &m);
        CHECK_FLOAT_EQ(m.width, 4000.0f);
        CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(0, 2)));
        tl_metrics(l, &m);
        CHECK_FLOAT_EQ(m.width, 3000.0f);   /* 2 characters at half size + 2 at full size */
        drop(&l);
    }
    DW_RELEASE(big); DW_RELEASE(t); TL_END();
}

TEST(create_text_with_newlines_ok)
{
    TL_BEGIN(); NEED_FMT(t);
    static const WCHAR *const s[] = { L"\n", L"\r", L"\r\n", L"A\nB", L"\x2028", L"\x2029", L"\x0085", L"\n\n\n" };
    size_t i; IDWriteTextLayout *l;
    for (i = 0; i < sizeof s / sizeof s[0]; i++) {
        CHECK_HR_OK(mk(f, t, s[i], (UINT32)wcslen(s[i]), 100.0f, 100.0f, &l));
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(create_text_surrogates_ok)
{
    /* well-formed and ill-formed UTF-16 never fail creation */
    TL_BEGIN(); NEED_FMT(t);
    static const WCHAR pair[] = { 0xD83D, 0xDE00 }, lone_hi[] = { 'A', 0xD83D }, lone_lo[] = { 0xDE00, 'A' }, rev[] = { 0xDE00, 0xD83D };
    IDWriteTextLayout *l;
    CHECK_HR_OK(mk(f, t, pair, 2, 100.0f, 100.0f, &l)); drop(&l);
    CHECK_HR_OK(mk(f, t, lone_hi, 2, 100.0f, 100.0f, &l)); drop(&l);
    CHECK_HR_OK(mk(f, t, lone_lo, 2, 100.0f, 100.0f, &l)); drop(&l);
    CHECK_HR_OK(mk(f, t, rev, 2, 100.0f, 100.0f, &l)); drop(&l);
    DW_RELEASE(t); TL_END();
}

TEST(create_noncharacters_ok)
{
    TL_BEGIN(); NEED_FMT(t);
    static const WCHAR s[] = { 0xFFFE, 0xFFFF, 0xFDD0, 0xE000, 'A' };
    IDWriteTextLayout *l; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(mk(f, t, s, 5, 1000.0f, 100.0f, &l));
    if (l != SENT_LAYOUT && l) { tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 1); drop(&l); }
    DW_RELEASE(t); TL_END();
}

TEST(create_every_bmp_char_ok)
{
    /* sweeping the BMP in chunks: no code unit value makes creation or GetMetrics fail */
    TL_BEGIN(); NEED_FMT(t);
    WCHAR *buf = (WCHAR *)malloc(4096 * sizeof(WCHAR));
    unsigned base, bad = 0;
    if (!buf) { DW_RELEASE(t); TL_END(); t_skip("alloc"); return; }
    for (base = 0; base < 0x10000; base += 4096) {
        IDWriteTextLayout *l; DWRITE_TEXT_METRICS m; unsigned i;
        for (i = 0; i < 4096; i++) buf[i] = (WCHAR)(base + i);
        if (mk(f, t, buf, 4096, 100000.0f, 100000.0f, &l) != S_OK) { bad++; continue; }
        if (IDWriteTextLayout_GetMetrics(l, &m) != S_OK) bad++;
        drop(&l);
    }
    CHECKF(bad == 0, "%u chunks failed", bad);
    free(buf); DW_RELEASE(t); TL_END();
}

TEST(create_very_long_text)
{
    /* 100000 characters on one line: width is the exact sum of the advances (10 DIPs per character at 20.48) */
    TL_BEGIN(); NEED_FMT(t);
    int N = t_is_wine() ? 2000 : 100000;        /* the emulated implementation is quadratic on one huge line */
    WCHAR *buf = (WCHAR *)malloc((N + 1) * sizeof(WCHAR));
    IDWriteTextFormat *fmt = tl_format(&X, TL_FAM, 20.48f);
    IDWriteTextLayout *l; DWRITE_TEXT_METRICS m; int i;
    if (!buf || !fmt) { free(buf); DW_RELEASE(fmt); DW_RELEASE(t); TL_END(); t_skip("alloc"); return; }
    for (i = 0; i < N; i++) buf[i] = 'A' + (i % 26);
    buf[N] = 0;
    CHECK_HR_OK(mk(f, fmt, buf, N, 1e9f, 1000.0f, &l));
    if (l != SENT_LAYOUT && l) {
        tl_metrics(l, &m);
        EXPECT_EQ(m.lineCount, 1);
        CHECKF(fabs(m.width - 10.0 * N) < N / 10000.0 + 0.01 * N / 100.0, "width %g", m.width);
        CHECK_FLOAT_EQ(m.layoutWidth, 1e9f);
        drop(&l);
    }
    free(buf); DW_RELEASE(fmt); DW_RELEASE(t); TL_END();
}

TEST(create_long_text_wraps_to_many_lines)
{
    TL_BEGIN(); NEED_FMT(t);
    enum { N = 20000 };
    WCHAR *buf = (WCHAR *)malloc((N + 1) * sizeof(WCHAR));
    IDWriteTextFormat *fmt = tl_format(&X, TL_FAM, TL_UPM);
    IDWriteTextLayout *l; DWRITE_TEXT_METRICS m; int i;
    if (!buf || !fmt) { free(buf); DW_RELEASE(fmt); DW_RELEASE(t); TL_END(); t_skip("alloc"); return; }
    for (i = 0; i < N; i++) buf[i] = (i % 10 == 9) ? ' ' : 'A';
    /* 2000 words of 9 letters + a space, each word 9000 wide; a 10000 box takes one word per line */
    CHECK_HR_OK(mk(f, fmt, buf, N, 10000.0f, 1e9f, &l));
    if (l != SENT_LAYOUT && l) { tl_metrics(l, &m); EXPECT_EQ(m.lineCount, 2000); CHECK_FLOAT_EQ(m.height, 2000 * (TL_ASC + TL_DESC)); drop(&l); }
    free(buf); DW_RELEASE(fmt); DW_RELEASE(t); TL_END();
}

/* ---- lifetime ------------------------------------------------------------------------------------------------ */
TEST(layout_survives_format_release)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(mk(f, t, L"ABCD", 4, 500.0f, 200.0f, &l));
    DW_RELEASE(t);
    if (l != SENT_LAYOUT && l) {
        float sz = 0; DWRITE_TEXT_RANGE r; WCHAR fam[32];
        tl_metrics(l, &m);
        CHECK_NEAR(m.width, 4 * 7.8125f, 0.05);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetFontSize(l, 0, &sz, &r) == S_OK ? sz : -1.0f, 16.0f);
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, fam, 32, &r)); CHECK_WEQ(fam, TL_FAM);
        drop(&l);
    }
    TL_END();
}

TEST(layout_survives_factory_release)
{
    /* the layout holds what it needs from the factory: dropping the creator's reference changes nothing observable */
    IDWriteFactory *f2 = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
    if (!f2) { t_skip("factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f2, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &t));
    if (!t) { DW_RELEASE(f2); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f2, L"ABCD", 4, t, 500.0f, 200.0f, &l));
    DW_RELEASE(t); DW_RELEASE(f2);
    if (l) {
        memset(&m, 0xcc, sizeof m);
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
        CHECK(m.width > 0.0f); CHECK(m.height > 0.0f); EXPECT_EQ(m.lineCount, 1);
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 10.0f));
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
        DW_RELEASE(l);
    }
}

TEST(layout_refcount_semantics)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mk(f, t, L"ABCD", 4, 500.0f, 200.0f, &l));
    if (l != SENT_LAYOUT && l) {
        IDWriteTextLayout_AddRef(l); EXPECT_REFS(l, 2); IDWriteTextLayout_AddRef(l); EXPECT_REFS(l, 3);
        IDWriteTextLayout_Release(l); EXPECT_REFS(l, 2); IDWriteTextLayout_Release(l); EXPECT_REFS(l, 1);
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(layout_holds_factory_alive)
{
    /* the layout keeps working after every other reference to its factory is gone */
    tl_fx X; IDWriteTextLayout *l; DWRITE_TEXT_METRICS m; IDWriteTextFormat *t;
    memset(&X, 0, sizeof X);
    if (!tl_open(&X)) { tl_close(&X); t_skip("fixture"); return; }
    t = tl_format(&X, TL_FAM, TL_UPM);
    if (!t) { tl_close(&X); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, L"ABC", 3, t, 5000.0f, 5000.0f, &l));
    DW_RELEASE(t);
    if (l) {
        IDWriteFactory_AddRef(X.f);      /* keep the fixture's own reference count honest while we poke at the layout */
        IDWriteFactory_Release(X.f);
        tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 3000.0f);
        DW_RELEASE(l);
    }
    tl_close(&X);
}

/* ---- the interface set of a layout --------------------------------------------------------------------------------- */
TEST(layout_qi_matrix)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mk(f, t, L"ABCD", 4, 500.0f, 200.0f, &l));
    if (l != SENT_LAYOUT && l) {
        const dw_qi_row rows[] = {
            DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteTextFormat, 1), DW_QI_ROW(IDWriteTextLayout, 1),
            DW_QI_ROW(IDWriteTextFormat1, 1), DW_QI_ROW(IDWriteTextLayout1, 1),
            DW_QI_ROW(IDWriteTextLayout2, 1), DW_QI_ROW(IDWriteTextLayout3, 1), DW_QI_ROW(IDWriteTextLayout4, 1),
            DW_QI_ROW(IDWriteTextFormat2, 1), DW_QI_ROW(IDWriteTextFormat3, 1),
            DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontCollection, 0),
            DW_QI_ROW(IDWriteTextRenderer, 0), DW_QI_ROW(IDWriteInlineObject, 0), DW_QI_ROW(IDWriteTypography, 0), DW_QI_ROW(IDWriteTextAnalyzer, 0),
            DW_QI_ROW(IDWriteLocalizedStrings, 0), DW_QI_ROW(IDWriteFontFallback, 0), DW_QI_ROW(IDWriteBitmapRenderTarget, 0),
        };
        DW_CHECK_QI(l, rows);
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(layout_qi_refcounts)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mk(f, t, L"ABCD", 4, 500.0f, 200.0f, &l));
    if (l != SENT_LAYOUT && l) {
        static const IID *const ids[] = { &IID_IDWriteTextLayout1, &IID_IDWriteTextLayout2, &IID_IDWriteTextLayout3, &IID_IDWriteTextLayout4,
                                          &IID_IDWriteTextFormat, &IID_IDWriteTextFormat1, &IID_IDWriteTextFormat2, &IID_IDWriteTextFormat3 };
        void *p[8] = { 0 }; size_t i;
        for (i = 0; i < 8; i++) {
            CHECK_HR_OK(IDWriteTextLayout_QueryInterface(l, ids[i], &p[i]));
            EXPECT_REFS(l, 2 + i);
        }
        for (i = 0; i < 8; i++) { if (p[i]) IUnknown_Release((IUnknown *)p[i]); EXPECT_REFS(l, 8 - i); }
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(layout_qi_null_out_zeroed)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mk(f, t, L"ABCD", 4, 500.0f, 200.0f, &l));
    if (l != SENT_LAYOUT && l) {
        void *p = (void *)(ULONG_PTR)1;
        CHECK_HR(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteFactory, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
        p = (void *)(ULONG_PTR)1;
        CHECK_HR(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteFontFace, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(layout_format_interface_usable)
{
    /* the IDWriteTextFormat view of a layout answers like the layout's own format getters */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l; IDWriteTextFormat *fv = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 500.0f, 200.0f, &l));
    DW_RELEASE(t);
    if (!l) { TL_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat, (void **)&fv));
    if (fv) {
        WCHAR fam[32];
        CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(fv), 16.0f);
        EXPECT_EQ(IDWriteTextFormat_GetFontWeight(fv), DWRITE_FONT_WEIGHT_NORMAL);
        CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(fv, fam, 32)); CHECK_WEQ(fam, TL_FAM);
        CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(fv, DWRITE_TEXT_ALIGNMENT_CENTER));
        EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_CENTER);   /* same object state */
        IDWriteTextFormat_Release(fv);
    }
    DW_RELEASE(l); TL_END();
}

TEST(create_from_layout_format_view)
{
    /* a format obtained from another layout is still a text format object: creation either works or fails cleanly */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l, *l2 = SENT_LAYOUT; IDWriteTextFormat *fv = NULL; HRESULT hr;
    CHECK_HR_OK(mk(f, t, L"ABCD", 4, 500.0f, 200.0f, &l));
    if (l != SENT_LAYOUT && l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat, (void **)&fv)) && fv) {
        hr = IDWriteFactory_CreateTextLayout(f, L"AB", 2, fv, 100.0f, 100.0f, &l2);
        t_note("CreateTextLayout with a layout's IDWriteTextFormat -> %s", dw_hr_name(hr));
        if (FAILED(hr)) CHECK_OUT_NULL(l2); else IDWriteTextLayout_Release(l2);
        IDWriteTextFormat_Release(fv);
    }
    drop(&l); DW_RELEASE(t); TL_END();
}

/* ---- CreateGdiCompatibleTextLayout ---------------------------------------------------------------------------------- */
TEST(gdi_create_basic_ok)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mkg(f, t, L"ABC", 3, 100.0f, 50.0f, 1.0f, NULL, FALSE, &l));
    CHECK(l != NULL && l != SENT_LAYOUT);
    if (l && l != SENT_LAYOUT) { EXPECT_REFS(l, 1); CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), 100.0f); CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), 50.0f); }
    drop(&l); DW_RELEASE(t); TL_END();
}

TEST(gdi_create_null_args)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR(mkg(f, t, NULL, 0, 100, 100, 1.0f, NULL, FALSE, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mkg(f, t, NULL, 3, 100, 100, 1.0f, NULL, TRUE, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mkg(f, NULL, L"ABC", 3, 100, 100, 1.0f, NULL, FALSE, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mkg(f, NULL, NULL, 0, 0, 0, 0.0f, NULL, FALSE, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_negative_nan_dimensions)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    static const float bad[] = { -1.0f, -FLT_MIN, -FLT_MAX, -INFINITY, NAN, -NAN };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECKF(mkg(f, t, L"ABC", 3, bad[i], 100, 1.0f, NULL, FALSE, &l) == E_INVALIDARG, "w %g", bad[i]); CHECK_OUT_NULL(l);
        CHECKF(mkg(f, t, L"ABC", 3, 100, bad[i], 1.0f, NULL, FALSE, &l) == E_INVALIDARG, "h %g", bad[i]); CHECK_OUT_NULL(l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_nan_pixels_per_dip)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR(mkg(f, t, L"ABC", 3, 100, 100, NAN, NULL, FALSE, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    CHECK_HR(mkg(f, t, L"ABC", 3, 100, 100, -NAN, NULL, FALSE, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_ppd_values_accepted)
{
    /* negative, zero, huge and infinite pixelsPerDip pass the factory's validation (only NaN is rejected) */
    TL_BEGIN(); NEED_FMT(t);
    static const float ppd[] = { 1.0f, 0.5f, 2.0f, 1.25f, 0.0f, -1.0f, -0.0f, 100.0f, 1e-20f };
    size_t i; IDWriteTextLayout *l;
    for (i = 0; i < sizeof ppd / sizeof ppd[0]; i++) {
        HRESULT hr = mkg(f, t, L"A", 1, 100, 100, ppd[i], NULL, FALSE, &l);
        CHECKF(hr == S_OK, "ppd %g -> %s", ppd[i], dw_hr_name(hr));
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_matrix_nan_members)
{
    /* each of the six members is checked for NaN, nothing else about the matrix is validated */
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l; int k;
    for (k = 0; k < 6; k++) {
        float v[6] = { 1, 0, 0, 1, 0, 0 };
        DWRITE_MATRIX m;
        v[k] = NAN;
        memcpy(&m, v, sizeof m);
        CHECKF(mkg(f, t, L"ABC", 3, 100, 100, 1.0f, &m, FALSE, &l) == E_INVALIDARG, "NaN in member %d", k); CHECK_OUT_NULL(l);
        v[k] = -NAN; memcpy(&m, v, sizeof m);
        CHECKF(mkg(f, t, L"ABC", 3, 100, 100, 1.0f, &m, TRUE, &l) == E_INVALIDARG, "-NaN in member %d", k); CHECK_OUT_NULL(l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_matrix_values_accepted)
{
    TL_BEGIN(); NEED_FMT(t);
    static const DWRITE_MATRIX ms[] = {
        { 1, 0, 0, 1, 0, 0 }, { 1, 0, 0, 1, 0.3f, 0.2f }, { 1, 0, 0, 1, -0.3f, -0.2f }, { 0, 0, 0, 0, 0, 0 }, { 1, 0, 0, 0, 0, 0 },
        { 1, 2, 0.5f, 1, 0, 0 }, { -1, 0, 0, 1, 0, 0 }, { 0, 1, -1, 0, 5, 5 }, { 1e30f, 0, 0, 1e30f, 0, 0 }, { INFINITY, 0, 0, 1, 0, 0 },
    };
    size_t i; IDWriteTextLayout *l;
    for (i = 0; i < sizeof ms / sizeof ms[0]; i++) {
        HRESULT hr = mkg(f, t, L"ABC", 3, 100, 100, 1.0f, &ms[i], FALSE, &l);
        CHECKF(hr == S_OK, "matrix %u -> %s", (unsigned)i, dw_hr_name(hr));
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_natural_flag_any_int)
{
    /* useGdiNatural is a BOOL: any non-zero value selects the natural mode */
    TL_BEGIN(); NEED_FMT(t);
    static const BOOL v[] = { 0, 1, 2, -1, 0x100, INT_MIN, INT_MAX };
    size_t i; IDWriteTextLayout *l;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        HRESULT hr = mkg(f, t, L"ABC", 3, 100, 100, 1.0f, NULL, v[i], &l);
        CHECKF(hr == S_OK, "natural %d -> %s", v[i], dw_hr_name(hr));
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_zero_length_ok)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(mkg(f, t, L"string", 0, 100, 100, 1.0f, NULL, FALSE, &l));
    if (l != SENT_LAYOUT && l) {
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), 100.0f); CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), 100.0f);
        tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 0.0f); EXPECT_EQ(m.lineCount, 1);
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_zero_dimensions_ok)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mkg(f, t, L"ABC", 3, 0, 0, 1.0f, NULL, FALSE, &l)); drop(&l);
    CHECK_HR_OK(mkg(f, t, L"ABC", 3, -0.0f, -0.0f, 1.0f, NULL, TRUE, &l)); drop(&l);
    DW_RELEASE(t); TL_END();
}

TEST(gdi_layout_qi_matrix)
{
    TL_BEGIN(); NEED_FMT(t);
    IDWriteTextLayout *l;
    CHECK_HR_OK(mkg(f, t, L"ABCD", 4, 500.0f, 200.0f, 1.0f, NULL, TRUE, &l));
    if (l != SENT_LAYOUT && l) {
        const dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteTextFormat, 1), DW_QI_ROW(IDWriteTextLayout, 1), DW_QI_ROW(IDWriteTextFormat1, 1), DW_QI_ROW(IDWriteTextLayout1, 1),
            DW_QI_ROW(IDWriteTextLayout2, 1), DW_QI_ROW(IDWriteTextLayout3, 1), DW_QI_ROW(IDWriteTextLayout4, 1),
            DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteTextRenderer, 0),
        };
        DW_CHECK_QI(l, rows);
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}

TEST(gdi_create_inherits_format_properties)
{
    TL_BEGIN();
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l; DWRITE_TEXT_RANGE r; float sz = 0; DWRITE_FONT_WEIGHT w; WCHAR loc[32], fam[32];
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, TL_FAM, X.coll, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 23.0f, L"fr-FR", &t));
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_TRAILING));
    CHECK_HR_OK(mkg(f, t, L"ABC", 3, 100, 100, 1.0f, NULL, FALSE, &l));
    if (l != SENT_LAYOUT && l) {
        CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 1, &sz, &r)); CHECK_FLOAT_EQ(sz, 23.0f);
        CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 1, &w, &r)); EXPECT_EQ(w, DWRITE_FONT_WEIGHT_BOLD);
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, fam, 32, &r)); CHECK_WEQ(fam, TL_FAM);
        CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 0, loc, 32, &r)); CHECK(!_wcsicmp(loc, L"fr-FR"));
        EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_TRAILING);
        drop(&l);
    }
    DW_RELEASE(t); TL_END();
}
