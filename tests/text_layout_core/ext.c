/* IDWriteTextLayout1 .. IDWriteTextLayout4: pair kerning, character spacing, extended metrics, vertical glyph orientation, last line
 * wrapping, optical alignment, font fallback, InvalidateLayout, DWRITE_LINE_SPACING, font axis values, automatic font axes.
 *
 * Validation facts used (all from the layout's own methods): SetPairKerning / SetCharacterSpacing check only the range for unsigned
 * wrap-around (and NaN leading/trailing, minimum advance < 0 or NaN); SetVerticalGlyphOrientation and SetOpticalAlignment accept 0..1;
 * SetLastLineWrapping accepts any int; SetAutomaticFontAxes accepts 0..1; SetLineSpacing(DWRITE_LINE_SPACING) needs a non-NULL pointer,
 * method 0..2, height >= 0, leadingBefore in [0,1], no NaN; SetFontAxisValues needs count < 0x10000 and a pointer when count > 0, and
 * stores the array verbatim (no sorting, de-duplication or tag validation). */
#include "tl.h"

#define LAYOUT_N(var, text) TL_LAYOUT(var, text, TL_UPM, 100000.0f, 100000.0f)
#define MAXR 0xFFFFFFFFu

/* ======================================================================================================================
 * IDWriteTextLayout1: pair kerning
 * ==================================================================================================================== */
TEST(pairkerning_default_off_full_range)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l1, IDWriteTextLayout1, l);
    BOOL k = 0xcc; DWRITE_TEXT_RANGE r = { 5, 5 };
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 0, &k, &r));
    EXPECT_EQ(k, FALSE); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 7, &k, &r)); EXPECT_EQ(k, FALSE); EXPECT_EQ(r.length, MAXR);
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 5000, &k, &r)); EXPECT_EQ(k, FALSE);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(pairkerning_null_range_allowed)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l1, IDWriteTextLayout1, l);
    BOOL k = 0xcc;
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 0, &k, NULL)); EXPECT_EQ(k, FALSE);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(pairkerning_range_runs)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l1, IDWriteTextLayout1, l);
    BOOL k; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(2, 3)));
    k = 9; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 1, &k, &r)); EXPECT_EQ(k, FALSE); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 2);
    k = 9; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 2, &k, &r)); EXPECT_EQ(k, TRUE); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 3);
    k = 9; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 4, &k, &r)); EXPECT_EQ(k, TRUE); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 3);
    k = 9; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 5, &k, &r)); EXPECT_EQ(k, FALSE); EXPECT_EQ(r.startPosition, 5); EXPECT_EQ(r.length, MAXR - 5);
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, FALSE, RNG(2, 3)));
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 3, &k, &r)); EXPECT_EQ(k, FALSE); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(pairkerning_any_nonzero_is_true)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l1, IDWriteTextLayout1, l);
    static const BOOL v[] = { 1, 2, -1, 0x100, INT_MIN, INT_MAX };
    size_t i; BOOL k; DWRITE_TEXT_RANGE r;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, v[i], RNG(0, 4)));
        k = 0xcc; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 0, &k, &r));
        CHECKF(k != 0, "Set(%d) reads back %d", v[i], k);
        t_note("GetPairKerning after Set(%d) -> %d", v[i], k);
        CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, 0, RNG(0, 4)));
    }
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(pairkerning_zero_length_noop_and_overflow)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l1, IDWriteTextLayout1, l);
    BOOL k = 9; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(1, 0)));
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 1, &k, &r)); EXPECT_EQ(k, FALSE); EXPECT_EQ(r.length, MAXR);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(1, MAXR)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(MAXR, 1)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0x80000000u, 0x80000000u)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, MAXR)));
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 3, &k, &r)); EXPECT_EQ(k, TRUE); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(pairkerning_beyond_text_and_merge)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l1, IDWriteTextLayout1, l);
    BOOL k; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(100, 4)));
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 101, &k, &r)); EXPECT_EQ(k, TRUE); EXPECT_EQ(r.startPosition, 100); EXPECT_EQ(r.length, 4);
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 100)));
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 50, &k, &r)); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 104);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(pairkerning_kern_table_font)
{
    /* a font with a legacy kern pair A-B (-200 units): enabling pair kerning may only narrow the text */
    TL_BEGIN();
    ttf_spec s; static const ttf_kernpair kp[] = { { 2, 3, -200 } };   /* gid 2 = 'A', gid 3 = 'B' */
    size_t sz; uint8_t *blob; dwx_mem_loader *ld = dwx_loader_new(); dwx_coll_loader *cl; IDWriteFontCollection *coll = NULL;
    IDWriteFactory *f2 = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    UINT32 key; IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS a, b;
    ttf_default_spec(&s); ttf_set_names(&s, "DwKern", "Regular", "DwKern", "DwKern-Regular"); s.kern = kp; s.nkern = 1;
    blob = ttf_build(&s, &sz);
    if (!f2 || !blob) { free(blob); DW_RELEASE(f2); dwx_release(ld); TL_END(); t_skip("setup"); return; }
    key = dwx_loader_add(ld, blob, sz); free(blob);
    cl = dwx_coll_new(ld); dwx_coll_add_key(cl, key);
    if (FAILED(dwx_loader_register(ld, f2)) || FAILED(dwx_coll_register(cl, f2)) || FAILED(dwx_coll_create(cl, f2, &coll))) goto out;
    if (FAILED(IDWriteFactory_CreateTextFormat(f2, L"DwKern", coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, TL_UPM, L"en-us", &t))) goto out;
    if (FAILED(IDWriteFactory_CreateTextLayout(f2, L"ABAB", 4, t, 100000.0f, 1000.0f, &l))) goto out;
    {
        IDWriteTextLayout1 *l1 = NULL;
        if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1))) {
            tl_metrics(l, &a);
            CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 4)));
            tl_metrics(l, &b);
            CHECK(b.width <= a.width);
            t_note("kern pair A-B at 2048: width default %g, pair kerning on %g", a.width, b.width);
            CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, FALSE, RNG(0, 4)));
            tl_metrics(l, &b); CHECK_FLOAT_EQ(b.width, a.width);
            IDWriteTextLayout1_Release(l1);
        }
    }
out:
    DW_RELEASE(l); DW_RELEASE(t); DW_RELEASE(coll);
    dwx_coll_unregister(cl); dwx_loader_unregister(ld); dwx_release(cl); dwx_release(ld); DW_RELEASE(f2);
    TL_END();
}

/* ======================================================================================================================
 * IDWriteTextLayout1: character spacing
 * ==================================================================================================================== */
static HRESULT get_cs(IDWriteTextLayout1 *l1, UINT32 pos, float *lead, float *trail, float *minadv, DWRITE_TEXT_RANGE *r)
{
    *lead = *trail = *minadv = 123.0f;
    return IDWriteTextLayout1_GetCharacterSpacing(l1, pos, lead, trail, minadv, r);
}

TEST(charspacing_default_zero_full_range)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l1, IDWriteTextLayout1, l);
    float a, b, c; DWRITE_TEXT_RANGE r = { 5, 5 };
    CHECK_HR_OK(get_cs(l1, 0, &a, &b, &c, &r));
    CHECK_FLOAT_EQ(a, 0.0f); CHECK_FLOAT_EQ(b, 0.0f); CHECK_FLOAT_EQ(c, 0.0f); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    CHECK_HR_OK(get_cs(l1, 6000, &a, &b, &c, &r)); CHECK_FLOAT_EQ(a, 0.0f);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(charspacing_roundtrip_values)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l1, IDWriteTextLayout1, l);
    static const float v[][3] = { { 1, 2, 3 }, { -5, -7, 0 }, { 0.5f, 0.25f, 100.0f }, { 1e6f, -1e6f, 1e6f }, { 0, 0, 0 }, { -0.0f, -0.0f, -0.0f },
                                  { INFINITY, -INFINITY, 0 }, { FLT_MAX, -FLT_MAX, FLT_MAX }, { 0, 0, INFINITY } };
    size_t i; float a, b, c; DWRITE_TEXT_RANGE r;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        HRESULT hr = IDWriteTextLayout1_SetCharacterSpacing(l1, v[i][0], v[i][1], v[i][2], RNG(1, 4));
        CHECKF(hr == S_OK, "row %u -> %s", (unsigned)i, dw_hr_name(hr));
        CHECK_HR_OK(get_cs(l1, 2, &a, &b, &c, &r));
        /* equal neighbouring runs merge, so a stored -0.0 may come back as the default +0.0: compare by value */
        CHECK_FLOAT_EQ(a, v[i][0]); CHECK_FLOAT_EQ(b, v[i][1]); CHECK_FLOAT_EQ(c, v[i][2]);
        IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 0, RNG(1, 4));
    }
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(charspacing_invalid_arguments)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l1, IDWriteTextLayout1, l);
    static const float bad[][3] = {
        { NAN, 0, 0 }, { 0, NAN, 0 }, { 0, 0, NAN }, { -NAN, 0, 0 }, { 0, -NAN, 0 }, { 0, 0, -NAN }, { 0, 0, -1.0f }, { 0, 0, -0.001f }, { 1, 1, -FLT_MIN },
        { 0, 0, -INFINITY }, { 5, 5, -FLT_MAX },
    };
    size_t i; float a, b, c; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 3, 4, 5, RNG(0, 8)));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteTextLayout1_SetCharacterSpacing(l1, bad[i][0], bad[i][1], bad[i][2], RNG(0, 8));
        CHECKF(hr == E_INVALIDARG, "row %u -> %s", (unsigned)i, dw_hr_name(hr));
    }
    CHECK_HR_OK(get_cs(l1, 0, &a, &b, &c, &r)); CHECK_FLOAT_EQ(a, 3.0f); CHECK_FLOAT_EQ(b, 4.0f); CHECK_FLOAT_EQ(c, 5.0f);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 1, 1, 1, RNG(1, MAXR)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 1, 1, 1, RNG(MAXR, 2)), E_INVALIDARG);
    CHECK_HR_OK(get_cs(l1, 0, &a, &b, &c, &r)); CHECK_FLOAT_EQ(a, 3.0f);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(charspacing_invalid_even_with_empty_range)
{
    /* values are validated before the range length is looked at */
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l1, IDWriteTextLayout1, l);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, NAN, 0, 0, RNG(0, 0)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, -1, RNG(0, 0)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 1, 1, 1, RNG(0, 0)));
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(charspacing_range_runs)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l1, IDWriteTextLayout1, l);
    float a, b, c; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 10, 20, 30, RNG(2, 3)));
    CHECK_HR_OK(get_cs(l1, 1, &a, &b, &c, &r)); CHECK_FLOAT_EQ(a, 0.0f); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 2);
    CHECK_HR_OK(get_cs(l1, 3, &a, &b, &c, &r)); CHECK_FLOAT_EQ(a, 10.0f); CHECK_FLOAT_EQ(b, 20.0f); CHECK_FLOAT_EQ(c, 30.0f); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 3);
    CHECK_HR_OK(get_cs(l1, 5, &a, &b, &c, &r)); CHECK_FLOAT_EQ(a, 0.0f); EXPECT_EQ(r.startPosition, 5); EXPECT_EQ(r.length, MAXR - 5);
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 1, 1, 1, RNG(3, 0)));      /* zero length: nothing changes */
    CHECK_HR_OK(get_cs(l1, 3, &a, &b, &c, &r)); CHECK_FLOAT_EQ(a, 10.0f); EXPECT_EQ(r.length, 3);
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 0, RNG(2, 3)));
    CHECK_HR_OK(get_cs(l1, 3, &a, &b, &c, &r)); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(charspacing_getter_zeroes_outputs_first)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l1, IDWriteTextLayout1, l);
    float a = 9, b = 9, c = 9; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 0, &a, &b, &c, &r));
    CHECK_FLOAT_EQ(a, 0.0f); CHECK_FLOAT_EQ(b, 0.0f); CHECK_FLOAT_EQ(c, 0.0f);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(charspacing_widens_text)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l1, IDWriteTextLayout1, l);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 4000.0f);
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 100, 50, 0, RNG(0, 4)));
    tl_metrics(l, &m);
    CHECK(m.width > 4000.0f);
    t_note("ABCD with leading 100 trailing 50: width %g (4 * 1150 = 4600)", m.width);
    CHECK_NEAR(m.width, 4600.0, 0.5);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(charspacing_minimum_advance_width)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l1, IDWriteTextLayout1, l);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 500, RNG(0, 4)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 4000.0f);                      /* below the natural advance: no effect */
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 1500, RNG(0, 4)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 6000.0f);                      /* every advance stretched to 1500 */
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 1500, RNG(0, 2)));
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 0, RNG(2, 2)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 5000.0f);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(charspacing_range_only_affects_range)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l1, IDWriteTextLayout1, l);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 100, 0, 0, RNG(1, 2)));
    tl_metrics(l, &m); CHECK_NEAR(m.width, 4200.0, 0.5);
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 0, RNG(0, 4)));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.width, 4000.0f);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

/* ======================================================================================================================
 * IDWriteTextLayout2 / IDWriteTextFormat1 views
 * ==================================================================================================================== */
TEST(metrics1_prefix_matches_metrics)
{
    TL_BEGIN(); LAYOUT_N(l, L"AB CD\nEFG"); TL_QI(l2, IDWriteTextLayout2, l);
    DWRITE_TEXT_METRICS a; DWRITE_TEXT_METRICS1 b;
    CHECK(sizeof(DWRITE_TEXT_METRICS1) == 40);
    tl_metrics(l, &a);
    memset(&b, 0xcc, sizeof b);
    CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l2, &b));
    CHECK_FBITS(b.left, a.left); CHECK_FBITS(b.top, a.top); CHECK_FBITS(b.width, a.width); CHECK_FBITS(b.widthIncludingTrailingWhitespace, a.widthIncludingTrailingWhitespace);
    CHECK_FBITS(b.height, a.height); CHECK_FBITS(b.layoutWidth, a.layoutWidth); CHECK_FBITS(b.layoutHeight, a.layoutHeight);
    EXPECT_EQ(b.maxBidiReorderingDepth, a.maxBidiReorderingDepth); EXPECT_EQ(b.lineCount, a.lineCount);
    CHECK(b.heightIncludingTrailingWhitespace >= b.height);
    CHECK_FLOAT_EQ(b.heightIncludingTrailingWhitespace, 2 * (TL_ASC + TL_DESC));
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

TEST(metrics1_trailing_whitespace_height)
{
    TL_BEGIN();
    static const WCHAR *const s[] = { L"AB", L"AB ", L"AB\n", L"AB\n  ", L"  ", L"AB \nCD ", L"\n\n" };
    size_t i;
    for (i = 0; i < sizeof s / sizeof s[0]; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, s[i], TL_UPM, 100000.0f, 100000.0f); IDWriteTextLayout2 *l2 = NULL; DWRITE_TEXT_METRICS1 m;
        if (!l) continue;
        if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2))) {
            CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l2, &m));
            CHECKF(m.heightIncludingTrailingWhitespace >= m.height, "%ls: %g < %g", s[i], m.heightIncludingTrailingWhitespace, m.height);
            t_note("%u lines for text #%u: height %g, with trailing whitespace %g", m.lineCount, (unsigned)i, m.height, m.heightIncludingTrailingWhitespace);
            IDWriteTextLayout2_Release(l2);
        }
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(metrics1_plain_text_heights_equal)
{
    TL_BEGIN(); LAYOUT_N(l, L"AB"); TL_QI(l2, IDWriteTextLayout2, l);
    DWRITE_TEXT_METRICS1 m;
    CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l2, &m));
    CHECK_FLOAT_EQ(m.height, TL_ASC + TL_DESC); CHECK_FLOAT_EQ(m.heightIncludingTrailingWhitespace, TL_ASC + TL_DESC);
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

TEST(vertical_glyph_orientation_roundtrip_and_views)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l2, IDWriteTextLayout2, l); TL_QI(f1, IDWriteTextFormat1, l);
    EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(f1), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(f1), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    CHECK_HR_OK(IDWriteTextFormat1_SetVerticalGlyphOrientation(f1, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT));
    EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    CHECK_HR(IDWriteTextFormat1_SetVerticalGlyphOrientation(f1, (DWRITE_VERTICAL_GLYPH_ORIENTATION)2), E_INVALIDARG);
    IDWriteTextFormat1_Release(f1); IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

TEST(vertical_glyph_orientation_inherited_from_format)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextFormat1 *t1 = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout2 *l2 = NULL;
    if (!t) { TL_END(); t_skip("format"); return; }
    if (FAILED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1))) { DW_RELEASE(t); TL_END(); t_skip("format1"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    CHECK_HR_OK(IDWriteTextFormat1_SetOpticalAlignment(t1, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS));
    CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(t1, FALSE));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 100.0f, 100.0f, &l));
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2))) {
        EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
        EXPECT_EQ(IDWriteTextLayout2_GetOpticalAlignment(l2), DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
        EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), FALSE);
        /* later format changes do not propagate */
        CHECK_HR_OK(IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT));
        EXPECT_EQ(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
        IDWriteTextLayout2_Release(l2);
    }
    DW_RELEASE(l); IDWriteTextFormat1_Release(t1); DW_RELEASE(t); TL_END();
}

TEST(last_line_wrapping_default_and_roundtrip)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l2, IDWriteTextLayout2, l); TL_QI(f1, IDWriteTextFormat1, l);
    EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), TRUE);          /* wrapping of the last line is on by default */
    CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
    EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), FALSE); EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(f1), FALSE);
    CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(f1, TRUE));
    EXPECT_EQ(IDWriteTextLayout2_GetLastLineWrapping(l2), TRUE);
    IDWriteTextFormat1_Release(f1); IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

TEST(last_line_wrapping_any_int)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l2, IDWriteTextLayout2, l);
    static const BOOL v[] = { 0, 1, 2, -1, 0x100, INT_MIN, INT_MAX };
    size_t i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECKF(IDWriteTextLayout2_SetLastLineWrapping(l2, v[i]) == S_OK, "Set(%d)", v[i]);
        CHECKF(IDWriteTextLayout2_GetLastLineWrapping(l2) == (v[i] != 0), "Set(%d) reads back %d", v[i], IDWriteTextLayout2_GetLastLineWrapping(l2));
    }
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

TEST(last_line_wrapping_effect)
{
    TL_BEGIN(); TL_LAYOUT(l, L"ABCD EFGH IJ", TL_UPM, 5000.0f, 100000.0f); TL_QI(l2, IDWriteTextLayout2, l);
    DWRITE_TEXT_METRICS a, b;
    tl_metrics(l, &a); EXPECT_EQ(a.lineCount, 3);
    CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
    tl_metrics(l, &b);
    t_note("last line wrapping off: %u lines (on: %u), width %g (on: %g)", b.lineCount, a.lineCount, b.width, a.width);
    CHECK(b.lineCount <= a.lineCount);
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

TEST(optical_alignment_default_roundtrip_views)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l2, IDWriteTextLayout2, l); TL_QI(f1, IDWriteTextFormat1, l);
    EXPECT_EQ(IDWriteTextLayout2_GetOpticalAlignment(l2), DWRITE_OPTICAL_ALIGNMENT_NONE);
    CHECK_HR_OK(IDWriteTextLayout2_SetOpticalAlignment(l2, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS));
    EXPECT_EQ(IDWriteTextFormat1_GetOpticalAlignment(f1), DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
    CHECK_HR(IDWriteTextLayout2_SetOpticalAlignment(l2, (DWRITE_OPTICAL_ALIGNMENT)2), E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat1_SetOpticalAlignment(f1, (DWRITE_OPTICAL_ALIGNMENT)2), E_INVALIDARG);
    EXPECT_EQ(IDWriteTextLayout2_GetOpticalAlignment(l2), DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
    CHECK_HR_OK(IDWriteTextFormat1_SetOpticalAlignment(f1, DWRITE_OPTICAL_ALIGNMENT_NONE));
    EXPECT_EQ(IDWriteTextLayout2_GetOpticalAlignment(l2), DWRITE_OPTICAL_ALIGNMENT_NONE);
    IDWriteTextFormat1_Release(f1); IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

TEST(optical_alignment_effect_on_metrics)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l2, IDWriteTextLayout2, l);
    DWRITE_TEXT_METRICS a, b;
    tl_metrics(l, &a);
    CHECK_HR_OK(IDWriteTextLayout2_SetOpticalAlignment(l2, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS));
    tl_metrics(l, &b);
    t_note("optical alignment on: left %g width %g (off: %g %g)", b.left, b.width, a.left, a.width);
    EXPECT_EQ(b.lineCount, a.lineCount);
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

TEST(fontfallback_default_and_refcounts)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l2, IDWriteTextLayout2, l);
    IDWriteFontFallback *got = (IDWriteFontFallback *)(ULONG_PTR)1; tl_fb fb;
    HRESULT hr = IDWriteTextLayout2_GetFontFallback(l2, &got);
    CHECK_HR_OK(hr);
    CHECK(got == NULL);                                           /* no client fallback unless one was set */
    DW_RELEASE(got);
    tl_fb_init(&fb);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, (IDWriteFontFallback *)&fb));
    EXPECT_EQ(fb.ref, 2);                                         /* retained by the layout */
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &got)); CHECK(got == (IDWriteFontFallback *)&fb); EXPECT_EQ(fb.ref, 3);
    IDWriteFontFallback_Release(got); EXPECT_EQ(fb.ref, 2);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, (IDWriteFontFallback *)&fb)); EXPECT_EQ(fb.ref, 2);   /* same object again */
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, NULL)); EXPECT_EQ(fb.ref, 1);
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &got)); CHECK(got == NULL); DW_RELEASE(got);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, (IDWriteFontFallback *)&fb)); EXPECT_EQ(fb.ref, 2);
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l);
    EXPECT_EQ(fb.ref, 1);                                          /* released with the layout */
    TL_END();
}

TEST(fontfallback_replace_releases_old)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l2, IDWriteTextLayout2, l);
    tl_fb a, b; tl_fb_init(&a); tl_fb_init(&b);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, (IDWriteFontFallback *)&a)); EXPECT_EQ(a.ref, 2);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, (IDWriteFontFallback *)&b)); EXPECT_EQ(a.ref, 1); EXPECT_EQ(b.ref, 2);
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); EXPECT_EQ(b.ref, 1);
    TL_END();
}

TEST(fontfallback_shared_with_format_view)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(f1, IDWriteTextFormat1, l);
    tl_fb fb; IDWriteFontFallback *got = NULL; tl_fb_init(&fb);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(f1, (IDWriteFontFallback *)&fb));
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(f1, &got)); CHECK(got == (IDWriteFontFallback *)&fb); DW_RELEASE(got);
    IDWriteTextFormat1_Release(f1); DW_RELEASE(l); EXPECT_EQ(fb.ref, 1);
    TL_END();
}

TEST(fontfallback_inherited_from_format_and_does_not_follow)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextFormat1 *t1 = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout2 *l2 = NULL;
    tl_fb fb, fb2; IDWriteFontFallback *got = NULL; tl_fb_init(&fb); tl_fb_init(&fb2);
    if (!t || FAILED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1))) { DW_RELEASE(t); TL_END(); t_skip("format1"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, (IDWriteFontFallback *)&fb));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 100.0f, 100.0f, &l));
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2))) {
        CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &got)); CHECK(got == (IDWriteFontFallback *)&fb); DW_RELEASE(got);
        CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, (IDWriteFontFallback *)&fb2));
        CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &got)); CHECK(got == (IDWriteFontFallback *)&fb); DW_RELEASE(got);
        IDWriteTextLayout2_Release(l2);
    }
    DW_RELEASE(l); IDWriteTextFormat1_SetFontFallback(t1, NULL); IDWriteTextFormat1_Release(t1); DW_RELEASE(t);
    EXPECT_EQ(fb.ref, 1); EXPECT_EQ(fb2.ref, 1);
    TL_END();
}

TEST(fontfallback_calls_for_available_glyphs)
{
    /* every character is in the primary font; whether the client fallback is still consulted is only recorded */
    TL_BEGIN(); LAYOUT_N(l, L"ABCD EF"); TL_QI(l2, IDWriteTextLayout2, l);
    tl_fb fb; tl_fb_init(&fb);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, (IDWriteFontFallback *)&fb));
    tl_lines(l);
    t_note("client fallback MapCharacters calls for text fully covered by the primary font: %d", (int)fb.map);
    IDWriteTextLayout2_SetFontFallback(l2, NULL);
    IDWriteTextLayout2_Release(l2); DW_RELEASE(l); TL_END();
}

/* ======================================================================================================================
 * IDWriteTextLayout3
 * ==================================================================================================================== */
TEST(invalidatelayout_ok_and_repeatable)
{
    TL_BEGIN(); LAYOUT_N(l, L"AB CD\nEF"); TL_QI(l3, IDWriteTextLayout3, l);
    DWRITE_TEXT_METRICS a, b; int i;
    tl_metrics(l, &a);
    for (i = 0; i < 10; i++) { CHECK_HR_OK(IDWriteTextLayout3_InvalidateLayout(l3)); tl_metrics(l, &b); CHECK(!memcmp(&a, &b, sizeof a)); }
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(invalidatelayout_before_any_layout_and_empty)
{
    TL_BEGIN(); TL_LAYOUT(l, L"", 16.0f, 10.0f, 10.0f); TL_QI(l3, IDWriteTextLayout3, l);
    CHECK_HR_OK(IDWriteTextLayout3_InvalidateLayout(l3)); CHECK_HR_OK(IDWriteTextLayout3_InvalidateLayout(l3));
    EXPECT_EQ(tl_lines(l), 1);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(invalidatelayout_calls_inline_object_again)
{
    /* a laid-out inline object is asked for its metrics again after invalidation of the layout */
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l3, IDWriteTextLayout3, l);
    dwx_inline *o = dwx_inline_new(500, 500, 400, FALSE); LONG n1, n2;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(1, 1)));
    tl_lines(l); n1 = o->n_metrics; CHECK(n1 >= 1);
    tl_lines(l); EXPECT_EQ(o->n_metrics, n1);                    /* cached layout: no new callbacks */
    CHECK_HR_OK(IDWriteTextLayout3_InvalidateLayout(l3));
    tl_lines(l); n2 = o->n_metrics; CHECK(n2 > n1);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); dwx_release(o); TL_END();
}

TEST(linespacing_struct_default)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l3, IDWriteTextLayout3, l);
    DWRITE_LINE_SPACING s; memset(&s, 0xcc, sizeof s);
    CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &s));
    EXPECT_EQ(s.method, DWRITE_LINE_SPACING_METHOD_DEFAULT); CHECK_FLOAT_EQ(s.height, 0.0f); CHECK_FLOAT_EQ(s.baseline, 0.0f);
    CHECK_FLOAT_EQ(s.leadingBefore, 0.0f); EXPECT_EQ(s.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT);
    CHECK(sizeof s == 20);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(linespacing_struct_roundtrip)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l3, IDWriteTextLayout3, l);
    static const DWRITE_LINE_SPACING v[] = {
        { DWRITE_LINE_SPACING_METHOD_DEFAULT, 0, 0, 0, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT },
        { DWRITE_LINE_SPACING_METHOD_UNIFORM, 30, 20, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED },
        { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 0, 1.0f, DWRITE_FONT_LINE_GAP_USAGE_DISABLED },
        { DWRITE_LINE_SPACING_METHOD_UNIFORM, 0, -10, 0.25f, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT },
        { DWRITE_LINE_SPACING_METHOD_DEFAULT, 12, 7, 0.0f, (DWRITE_FONT_LINE_GAP_USAGE)3 },
        { DWRITE_LINE_SPACING_METHOD_UNIFORM, 1e6f, -1e6f, 0.75f, (DWRITE_FONT_LINE_GAP_USAGE)0x7fffffff },
        { DWRITE_LINE_SPACING_METHOD_UNIFORM, 5, 5, 1.0f, (DWRITE_FONT_LINE_GAP_USAGE)0xffffffffu },
    };
    size_t i; DWRITE_LINE_SPACING g;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        HRESULT hr = IDWriteTextLayout3_SetLineSpacing(l3, &v[i]);
        CHECKF(hr == S_OK, "row %u -> %s", (unsigned)i, dw_hr_name(hr));
        memset(&g, 0xcc, sizeof g);
        CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &g));
        EXPECT_EQ(g.method, v[i].method); CHECK_FBITS(g.height, v[i].height); CHECK_FBITS(g.baseline, v[i].baseline);
        CHECK_FBITS(g.leadingBefore, v[i].leadingBefore); EXPECT_EQ(g.fontLineGapUsage, v[i].fontLineGapUsage);
    }
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(linespacing_struct_invalid)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l3, IDWriteTextLayout3, l);
    static const DWRITE_LINE_SPACING good = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 30, 20, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED };
    DWRITE_LINE_SPACING bad[16]; int n = 0, i; DWRITE_LINE_SPACING g;
    bad[n] = good; bad[n++].method = (DWRITE_LINE_SPACING_METHOD)3;
    bad[n] = good; bad[n++].method = (DWRITE_LINE_SPACING_METHOD)0xffffffffu;
    bad[n] = good; bad[n++].height = -1.0f;
    bad[n] = good; bad[n++].height = -FLT_MIN;
    bad[n] = good; bad[n++].height = NAN;
    bad[n] = good; bad[n++].baseline = NAN;
    bad[n] = good; bad[n++].leadingBefore = -0.001f;
    bad[n] = good; bad[n++].leadingBefore = 1.001f;
    bad[n] = good; bad[n++].leadingBefore = 2.0f;
    bad[n] = good; bad[n++].leadingBefore = NAN;
    bad[n] = good; bad[n++].leadingBefore = INFINITY;
    bad[n] = good; bad[n++].leadingBefore = -INFINITY;
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &good));
    for (i = 0; i < n; i++) {
        HRESULT hr = IDWriteTextLayout3_SetLineSpacing(l3, &bad[i]);
        CHECKF(hr == E_INVALIDARG, "row %d -> %s", i, dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &g));
        EXPECT_EQ(g.method, good.method); CHECK_FLOAT_EQ(g.height, 30.0f); CHECK_FLOAT_EQ(g.leadingBefore, 0.5f);
    }
    if (!t_is_wine()) CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, NULL), E_INVALIDARG);     /* the emulated implementation dereferences it */
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(linespacing_struct_boundaries_accepted)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l3, IDWriteTextLayout3, l);
    DWRITE_LINE_SPACING s = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 0, 0, 0, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT };
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s.leadingBefore = 1.0f; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s.leadingBefore = -0.0f; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s.height = -0.0f; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s.height = INFINITY; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s.baseline = -INFINITY; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s.baseline = FLT_MAX; s.height = FLT_MAX; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(linespacing_struct_and_triple_share_state)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l3, IDWriteTextLayout3, l);
    DWRITE_LINE_SPACING s = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 3.0f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, g;
    DWRITE_LINE_SPACING_METHOD m; float h, b;
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &h, &b));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL); CHECK_FLOAT_EQ(h, 2.0f); CHECK_FLOAT_EQ(b, 3.0f);
    /* the triple setter resets the two fields it does not know about */
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 40.0f, 30.0f));
    CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &g));
    EXPECT_EQ(g.method, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(g.height, 40.0f); CHECK_FLOAT_EQ(g.baseline, 30.0f);
    CHECK_FLOAT_EQ(g.leadingBefore, 0.0f); EXPECT_EQ(g.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(linespacing_struct_via_format2_view)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l3, IDWriteTextLayout3, l); TL_QI(f2, IDWriteTextFormat2, l);
    DWRITE_LINE_SPACING s = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 33, 22, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_DISABLED }, g;
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(f2, &s));
    CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &g));
    EXPECT_EQ(g.method, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(g.height, 33.0f); CHECK_FLOAT_EQ(g.leadingBefore, 0.5f);
    s.height = 44; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(f2, &g)); CHECK_FLOAT_EQ(g.height, 44.0f);
    s.leadingBefore = 2.0f; CHECK_HR(IDWriteTextFormat2_SetLineSpacing(f2, &s), E_INVALIDARG);
    IDWriteTextFormat2_Release(f2); IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

TEST(linespacing_struct_inherited_from_format2)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextFormat2 *t2 = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout3 *l3 = NULL;
    DWRITE_LINE_SPACING s = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 33, 22, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, g;
    if (!t || FAILED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat2, (void **)&t2))) { DW_RELEASE(t); TL_END(); t_skip("format2"); return; }
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(t2, &s));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 100.0f, 100.0f, &l));
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout3, (void **)&l3))) {
        CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &g));
        EXPECT_EQ(g.method, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(g.height, 33.0f); CHECK_FLOAT_EQ(g.baseline, 22.0f);
        CHECK_FLOAT_EQ(g.leadingBefore, 0.5f); EXPECT_EQ(g.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_ENABLED);
        IDWriteTextLayout3_Release(l3);
    }
    DW_RELEASE(l); IDWriteTextFormat2_Release(t2); DW_RELEASE(t); TL_END();
}

TEST(linespacing_uniform_sets_line_height)
{
    TL_BEGIN(); LAYOUT_N(l, L"A\nB\nC");
    DWRITE_TEXT_METRICS m; DWRITE_LINE_METRICS lm[4]; UINT32 n = 0;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 3000.0f, 2000.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.height, 9000.0f); EXPECT_EQ(m.lineCount, 3);
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, lm, 4, &n));
    EXPECT_EQ(n, 3);
    if (n == 3) { CHECK_FLOAT_EQ(lm[0].height, 3000.0f); CHECK_FLOAT_EQ(lm[0].baseline, 2000.0f); CHECK_FLOAT_EQ(lm[2].height, 3000.0f); }
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 100.0f, 80.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.height, 300.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_DEFAULT, 5.0f, 5.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.height, 3 * (TL_ASC + TL_DESC));         /* DEFAULT ignores height and baseline */
    DW_RELEASE(l); TL_END();
}

TEST(linespacing_proportional_scales_line_height)
{
    TL_BEGIN(); LAYOUT_N(l, L"A\nB");
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 0.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.height, 2 * 2 * (TL_ASC + TL_DESC));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 0.5f, 0.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.height, 2 * 0.5f * (TL_ASC + TL_DESC));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.0f, 0.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.height, 2 * (TL_ASC + TL_DESC));
    DW_RELEASE(l); TL_END();
}

TEST(linemetrics1_default_values)
{
    TL_BEGIN(); LAYOUT_N(l, L"AB\nC "); TL_QI(l3, IDWriteTextLayout3, l);
    DWRITE_LINE_METRICS1 lm[4]; UINT32 n = 0;
    memset(lm, 0xcc, sizeof lm);
    CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, lm, 4, &n));
    EXPECT_EQ(n, 2);
    if (n == 2) {
        EXPECT_EQ(lm[0].length, 3); EXPECT_EQ(lm[0].newlineLength, 1); EXPECT_EQ(lm[1].length, 2); EXPECT_EQ(lm[1].trailingWhitespaceLength, 1);
        CHECK_FLOAT_EQ(lm[0].height, TL_ASC + TL_DESC); CHECK_FLOAT_EQ(lm[0].baseline, TL_ASC);
        CHECK_FLOAT_EQ(lm[0].leadingBefore, 0.0f); CHECK_FLOAT_EQ(lm[0].leadingAfter, 0.0f);
    }
    n = 99; CHECK_HR(IDWriteTextLayout3_GetLineMetrics(l3, lm, 1, &n), DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 2);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TL_END();
}

/* ======================================================================================================================
 * IDWriteTextLayout4 / IDWriteTextFormat3: font axis values and automatic axes
 * ==================================================================================================================== */
static const DWRITE_FONT_AXIS_VALUE AX2[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 650.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 87.5f } };

TEST(axes_default_empty)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l4, IDWriteTextLayout4, l); TL_QI(f3, IDWriteTextFormat3, l);
    DWRITE_FONT_AXIS_VALUE v[4]; DWRITE_TEXT_RANGE r = { 5, 5 };
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 0), 0);
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 7), 0);
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 5000), 0);
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(f3), 0);
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 0, v, 4, &r)); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 0, v, 0, &r));
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 0, NULL, 0, NULL));
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(f3, v, 0));
    IDWriteTextFormat3_Release(f3); IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_range_roundtrip)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l4, IDWriteTextLayout4, l);
    DWRITE_FONT_AXIS_VALUE v[4]; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, AX2, 2, RNG(2, 3)));
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 1), 0);
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 2), 2);
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 4), 2);
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 5), 0);
    memset(v, 0xcc, sizeof v);
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 3, v, 4, &r));
    EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 3);
    EXPECT_EQ(v[0].axisTag, DWRITE_FONT_AXIS_TAG_WEIGHT); CHECK_FLOAT_EQ(v[0].value, 650.0f);
    EXPECT_EQ(v[1].axisTag, DWRITE_FONT_AXIS_TAG_WIDTH); CHECK_FLOAT_EQ(v[1].value, 87.5f);
    EXPECT_EQ(v[2].axisTag, 0); CHECK_FLOAT_EQ(v[2].value, 0.0f); EXPECT_EQ(v[3].axisTag, 0);          /* the unused tail of the buffer is zeroed */
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 0, v, 4, &r)); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 2);
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 6, v, 4, &r)); EXPECT_EQ(r.startPosition, 5); EXPECT_EQ(r.length, MAXR - 5);
    IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_stored_verbatim)
{
    /* no sorting, de-duplication or tag validation */
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l);
    static const DWRITE_FONT_AXIS_VALUE in[] = { { DWRITE_FONT_AXIS_TAG_WIDTH, 1.0f }, { DWRITE_FONT_AXIS_TAG_WEIGHT, 2.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 3.0f },
                                                 { (DWRITE_FONT_AXIS_TAG)0, 4.0f }, { (DWRITE_FONT_AXIS_TAG)0xffffffffu, NAN }, { DWRITE_FONT_AXIS_TAG_ITALIC, -1e30f }, { DWRITE_FONT_AXIS_TAG_SLANT, INFINITY } };
    DWRITE_FONT_AXIS_VALUE out[8]; DWRITE_TEXT_RANGE r; int i;
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, in, 7, RNG(0, 4)));
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 0), 7);
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 0, out, 8, &r));
    for (i = 0; i < 7; i++) { EXPECT_EQ(out[i].axisTag, in[i].axisTag); CHECK_FBITS(out[i].value, in[i].value); }
    EXPECT_EQ(out[7].axisTag, 0);
    IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_get_buffer_protocol)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l);
    DWRITE_FONT_AXIS_VALUE v[4]; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, AX2, 2, RNG(0, 4)));
    CHECK_HR(IDWriteTextLayout4_GetFontAxisValues(l4, 0, v, 1, &r), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR(IDWriteTextLayout4_GetFontAxisValues(l4, 0, v, 0, &r), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR(IDWriteTextLayout4_GetFontAxisValues(l4, 0, NULL, 0, &r), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 0, v, 2, &r));
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 0, v, 2, NULL));
    IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_set_invalid_arguments)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l); TL_QI(f3, IDWriteTextFormat3, l);
    static DWRITE_FONT_AXIS_VALUE big[0x10000];
    if (!t_is_wine()) {          /* the emulated implementation reads the array before validating the count */
        CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, NULL, 1, RNG(0, 4)), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, NULL, 0x10, RNG(0, 4)), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, big, 0xffffffffu, RNG(0, 4)), E_INVALIDARG);
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(f3, NULL, 1), E_INVALIDARG);
    }
    CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, big, 0x10000, RNG(0, 4)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, AX2, 2, RNG(1, MAXR)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, AX2, 2, RNG(MAXR, 1)), E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(f3, big, 0x10000), E_INVALIDARG);
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 0), 0);
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, NULL, 0, RNG(0, 4)));          /* zero count needs no pointer */
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(f3, NULL, 0));
    IDWriteTextFormat3_Release(f3); IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_set_max_count_accepted)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l);
    static DWRITE_FONT_AXIS_VALUE big[0xffff]; UINT32 i;
    for (i = 0; i < 0xffff; i++) { big[i].axisTag = (DWRITE_FONT_AXIS_TAG)(0x20202020 + i); big[i].value = (float)i; }
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, big, 0xffff, RNG(0, 4)));
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 2), 0xffff);
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, NULL, 0, RNG(0, 4)));
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 2), 0);
    IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_set_zero_length_range_noop)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l);
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, AX2, 2, RNG(1, 0)));
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 1), 0);
    IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_set_is_copied)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l);
    DWRITE_FONT_AXIS_VALUE in[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 100.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 50.0f } }, out[2];
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, in, 2, RNG(0, 4)));
    in[0].value = 999.0f; in[1].axisTag = 0;
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 0, out, 2, NULL));
    CHECK_FLOAT_EQ(out[0].value, 100.0f); EXPECT_EQ(out[1].axisTag, DWRITE_FONT_AXIS_TAG_WIDTH);
    IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_layout_wide_set_covers_everything)
{
    /* the IDWriteTextFormat3 form changes the layout-wide default and every range of the layout */
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l4, IDWriteTextLayout4, l); TL_QI(f3, IDWriteTextFormat3, l);
    DWRITE_FONT_AXIS_VALUE v[2]; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, &AX2[1], 1, RNG(2, 2)));
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(f3, AX2, 2));
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(f3), 2);
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 0), 2);
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 3), 2);                  /* the earlier ranged value is overwritten */
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 5000), 2);
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 3, v, 2, &r)); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(f3, v, 2)); EXPECT_EQ(v[0].axisTag, DWRITE_FONT_AXIS_TAG_WEIGHT); CHECK_FLOAT_EQ(v[1].value, 87.5f);
    CHECK_HR(IDWriteTextFormat3_GetFontAxisValues(f3, v, 1), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR(IDWriteTextFormat3_GetFontAxisValues(f3, v, 0), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(f3, NULL, 0));
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 3), 0); EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(f3), 0);
    IDWriteTextFormat3_Release(f3); IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_ranged_set_leaves_layout_wide_default)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l4, IDWriteTextLayout4, l); TL_QI(f3, IDWriteTextFormat3, l);
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, AX2, 2, RNG(0, 8)));
    EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 3), 2);
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(f3), 0);
    IDWriteTextFormat3_Release(f3); IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_inherited_from_format3)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextFormat3 *t3 = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout4 *l4 = NULL;
    DWRITE_FONT_AXIS_VALUE v[2];
    if (!t || FAILED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat3, (void **)&t3))) { DW_RELEASE(t); TL_END(); t_skip("format3"); return; }
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, AX2, 2));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 100.0f, 100.0f, &l));
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4))) {
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 1), 2);
        CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 1, v, 2, NULL)); CHECK_FLOAT_EQ(v[0].value, 650.0f);
        CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, NULL, 0));              /* later change does not follow */
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l4, 1), 2);
        IDWriteTextLayout4_Release(l4);
    }
    DW_RELEASE(l); IDWriteTextFormat3_Release(t3); DW_RELEASE(t); TL_END();
}

TEST(axes_do_not_change_metrics_of_static_font)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l);
    DWRITE_TEXT_METRICS a, b;
    tl_metrics(l, &a);
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, AX2, 2, RNG(0, 4)));
    tl_metrics(l, &b); CHECK_FLOAT_EQ(a.width, b.width); CHECK_FLOAT_EQ(a.height, b.height);
    IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(axes_share_run_list_with_other_properties)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCDEFGH"); TL_QI(l4, IDWriteTextLayout4, l);
    DWRITE_TEXT_RANGE r; DWRITE_FONT_WEIGHT w;
    CHECK_HR_OK(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(2, 2)));
    CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l4, 3, NULL, 0, &r)); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 2);
    CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, AX2, 2, RNG(3, 3)));
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 2, &w, &r)); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 1);
    IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(automaticaxes_default_roundtrip)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l); TL_QI(f3, IDWriteTextFormat3, l);
    EXPECT_EQ(IDWriteTextLayout4_GetAutomaticFontAxes(l4), DWRITE_AUTOMATIC_FONT_AXES_NONE);
    CHECK_HR_OK(IDWriteTextLayout4_SetAutomaticFontAxes(l4, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
    EXPECT_EQ(IDWriteTextLayout4_GetAutomaticFontAxes(l4), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(f3), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(f3, DWRITE_AUTOMATIC_FONT_AXES_NONE));
    EXPECT_EQ(IDWriteTextLayout4_GetAutomaticFontAxes(l4), DWRITE_AUTOMATIC_FONT_AXES_NONE);
    IDWriteTextFormat3_Release(f3); IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(automaticaxes_invalid_values_on_layout)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD"); TL_QI(l4, IDWriteTextLayout4, l); TL_QI(f3, IDWriteTextFormat3, l);
    static const UINT32 bad[] = { 2, 3, 4, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    size_t i;
    CHECK_HR_OK(IDWriteTextLayout4_SetAutomaticFontAxes(l4, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECKF(IDWriteTextLayout4_SetAutomaticFontAxes(l4, (DWRITE_AUTOMATIC_FONT_AXES)bad[i]) == E_INVALIDARG, "layout value %#x", bad[i]);
        CHECKF(IDWriteTextFormat3_SetAutomaticFontAxes(f3, (DWRITE_AUTOMATIC_FONT_AXES)bad[i]) == E_INVALIDARG, "format view value %#x", bad[i]);
    }
    EXPECT_EQ(IDWriteTextLayout4_GetAutomaticFontAxes(l4), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
    IDWriteTextFormat3_Release(f3); IDWriteTextLayout4_Release(l4); DW_RELEASE(l); TL_END();
}

TEST(automaticaxes_format_accepts_out_of_range_layout_keeps_it)
{
    /* a format object stores any value; a layout created from it reports the stored value but only accepts 0..1 itself */
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextFormat3 *t3 = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout4 *l4 = NULL;
    if (!t || FAILED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat3, (void **)&t3))) { DW_RELEASE(t); TL_END(); t_skip("format3"); return; }
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t3, (DWRITE_AUTOMATIC_FONT_AXES)2));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 100.0f, 100.0f, &l));
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4))) {
        EXPECT_EQ(IDWriteTextLayout4_GetAutomaticFontAxes(l4), 2);
        CHECK_HR(IDWriteTextLayout4_SetAutomaticFontAxes(l4, (DWRITE_AUTOMATIC_FONT_AXES)2), E_INVALIDARG);
        EXPECT_EQ(IDWriteTextLayout4_GetAutomaticFontAxes(l4), 2);
        CHECK_HR_OK(IDWriteTextLayout4_SetAutomaticFontAxes(l4, DWRITE_AUTOMATIC_FONT_AXES_NONE));
        IDWriteTextLayout4_Release(l4);
    }
    DW_RELEASE(l); IDWriteTextFormat3_Release(t3); DW_RELEASE(t); TL_END();
}

/* ======================================================================================================================
 * views of one object
 * ==================================================================================================================== */
TEST(views_share_identity_and_state)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD");
    TL_QI(l1, IDWriteTextLayout1, l); TL_QI(l2, IDWriteTextLayout2, l); TL_QI(l3, IDWriteTextLayout3, l); TL_QI(l4, IDWriteTextLayout4, l);
    CHECK(dw_same_identity((IUnknown *)l1, (IUnknown *)l)); CHECK(dw_same_identity((IUnknown *)l2, (IUnknown *)l));
    CHECK(dw_same_identity((IUnknown *)l3, (IUnknown *)l)); CHECK(dw_same_identity((IUnknown *)l4, (IUnknown *)l));
    CHECK_HR_OK(IDWriteTextLayout4_SetMaxWidth(l4, 77.0f));
    CHECK_FLOAT_EQ(IDWriteTextLayout1_GetMaxWidth(l1), 77.0f); CHECK_FLOAT_EQ(IDWriteTextLayout2_GetMaxWidth(l2), 77.0f); CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), 77.0f);
    CHECK_HR_OK(IDWriteTextLayout2_SetTextAlignment(l2, DWRITE_TEXT_ALIGNMENT_CENTER));
    EXPECT_EQ(IDWriteTextLayout3_GetTextAlignment(l3), DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextLayout4_Release(l4); IDWriteTextLayout3_Release(l3); IDWriteTextLayout2_Release(l2); IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TL_END();
}

TEST(views_object_dies_with_last_reference)
{
    TL_BEGIN(); LAYOUT_N(l, L"ABCD");
    IDWriteTextLayout4 *l4 = NULL;
    dwx_effect *e = dwx_effect_new(1);
    CHECK_HR_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4));
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 4)));
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(e) >= 2, 1);                       /* still alive through the other view */
    if (l4) IDWriteTextLayout4_Release(l4);
    EXPECT_EQ(dwx_refs(e), 1);
    dwx_release(e); TL_END();
}
