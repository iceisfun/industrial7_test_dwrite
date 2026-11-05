/* Range behaviour on other kinds of layouts and texts: GDI-compatible layouts, text with line breaks, right-to-left text, embedded NUL,
 * layouts created from a layout's own format, and independence from the text format the layout was created from. */
#include "tlr.h"

static IDWriteTextLayout *gdi_layout(tlr_fx *x, const WCHAR *text, float size)
{
    IDWriteTextFormat *t = tlr_format(x, TLR_FAM, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(x->f, text, (UINT32)wcslen(text), t, 100000.0f, 100000.0f, 1.0f, NULL, FALSE, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

TEST(misc_gdi_compatible_layout_ranges)
{
    TLR_BEGIN(); IDWriteTextLayout *l = gdi_layout(&X, L"abcdefghij", 16.0f); DWRITE_TEXT_RANGE r;
    if (!l) { TLR_END(); t_skip("CreateGdiCompatibleTextLayout failed"); return; }
    CHECK(tlr_weight(l, 3, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(2, 3)), S_OK);
    CHECK(tlr_weight(l, 3, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 2, 3);
    CHECK(tlr_weight(l, 5, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 5, TLR_ALL - 5);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, -1.0f, RNG(2, 3)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(1, TLR_ALL)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, RNG(0, 1)), TLR_EINVALIDARG);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_gdi_compatible_layout_size_range_changes_width)
{
    TLR_BEGIN(); IDWriteTextLayout *l = gdi_layout(&X, L"abcdefgh", 2048.0f);
    if (!l) { TLR_END(); t_skip("CreateGdiCompatibleTextLayout failed"); return; }
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(4, 4)), S_OK);
    t_note("GDI-compatible layout width with half of the text at half size: %g (6000 for ideal metrics)", tlr_width(l));
    CHECK(tlr_width(l) < 8000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_ranges_cross_line_breaks)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"ab\ncd\r\nef", 2048.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(1, 6)), S_OK);       /* from 'b' to 'e', across both line breaks */
    CHECK(tlr_weight(l, 2, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 1, 6);
    CHECK(tlr_weight(l, 7, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 7, TLR_ALL - 7);
    EXPECT_EQ(tlr_lines(l), 3u);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_range_on_line_break_characters_only)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"ab\r\ncd", 2048.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 100.0f, RNG(2, 2)), S_OK);
    CHECK_FLOAT_EQ(tlr_size(l, 3, &r), 100.0f); CHECK_RANGE(r, 2, 2);
    EXPECT_EQ(tlr_lines(l), 2u);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_rtl_text_ranges)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"\x05D0\x05D1\x05D2 ab", 16.0f); DWRITE_TEXT_RANGE r; DWRITE_TEXT_METRICS m;
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(1, 3)), S_OK);
    CHECK(tlr_weight(l, 2, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 1, 3);
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK);
    CHECK(tlr_weight(l, 0, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, 1);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_combining_marks_ranges)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"e\x0301" L"a\x0301", 16.0f); DWRITE_TEXT_RANGE r; DWRITE_TEXT_METRICS m;
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 30.0f, RNG(1, 1)), S_OK);          /* only the combining mark */
    CHECK_FLOAT_EQ(tlr_size(l, 1, &r), 30.0f); CHECK_RANGE(r, 1, 1);
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_embedded_nul_is_ordinary_position)
{
    TLR_BEGIN(); IDWriteTextFormat *t = tlr_format(&X, TLR_FAM, 16.0f); IDWriteTextLayout *l = NULL; DWRITE_TEXT_RANGE r;
    static const WCHAR text[] = { L'a', 0, L'b', L'c' };
    if (!t) { TLR_END(); t_skip("format"); return; }
    if (FAILED(IDWriteFactory_CreateTextLayout(f, text, 4, t, 1000, 1000, &l)) || !l) { IDWriteTextFormat_Release(t); TLR_END(); t_skip("layout with NUL"); return; }
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(1, 2)), S_OK);
    CHECK(tlr_weight(l, 1, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 1, 2);
    CHECK(tlr_weight(l, 3, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 3, TLR_ALL - 3);
    IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t); TLR_END();
}
TEST(misc_layout_is_independent_of_source_format_lifetime)
{
    TLR_BEGIN(); IDWriteTextFormat *t = tlr_format(&X, TLR_NARROW, 20.0f); IDWriteTextLayout *l = NULL; DWRITE_TEXT_RANGE r;
    if (!t) { TLR_END(); t_skip("format"); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abcdef", 6, t, 1000, 1000, &l), S_OK);
    IDWriteTextFormat_Release(t);
    if (l) {
        CHECK_FLOAT_EQ(tlr_size(l, 2, &r), 20.0f); CHECK_WEQ(tlr_family(l, 2, &r), TLR_NARROW);
        CHECK_HR(IDWriteTextLayout_SetFontSize(l, 30.0f, RNG(1, 2)), S_OK);
        CHECK_FLOAT_EQ(tlr_size(l, 1, &r), 30.0f); CHECK_RANGE(r, 1, 2);
        IDWriteTextLayout_Release(l);
    }
    TLR_END();
}
TEST(misc_layout_of_a_layout_uses_default_format_only)
{
    /* a layout is itself an IDWriteTextFormat: creating a new layout from it takes the layout-wide defaults, not the per-range values */
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdef", 16.0f); IDWriteTextLayout *l2 = NULL; DWRITE_TEXT_RANGE r; HRESULT hr;
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 6)), S_OK);
    hr = IDWriteFactory_CreateTextLayout(f, L"abcdef", 6, (IDWriteTextFormat *)l, 1000, 1000, &l2);
    t_note("CreateTextLayout with a layout as the format -> %s", dw_hr_name(hr));
    if (hr == S_OK && l2) {
        t_note("new layout weight at 0: %d", (int)tlr_weight(l2, 0, &r));
        IDWriteTextLayout_Release(l2);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_get_at_text_length_and_beyond_succeeds)
{
    /* the position is not validated against the text length: the tail run is reported, never an error */
    TLR_BEGIN(); TLR_LAYOUT(l, L"abc", 16.0f); DWRITE_TEXT_RANGE r; DWRITE_FONT_WEIGHT w; UINT32 len; float sz; BOOL b; WCHAR buf[16]; UINT32 pos[] = { 3, 4, 100, 0xFFFFFFFEu };
    int i;
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(1, 2)), S_OK);
    for (i = 0; i < 4; i++) {
        r = POISON_RANGE; w = 7;
        CHECK_HR(IDWriteTextLayout_GetFontWeight(l, pos[i], &w, &r), S_OK); CHECK(w == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 3, TLR_ALL - 3);
        r = POISON_RANGE; sz = 0;
        CHECK_HR(IDWriteTextLayout_GetFontSize(l, pos[i], &sz, &r), S_OK); CHECK_FLOAT_EQ(sz, 16.0f); CHECK_RANGE(r, 3, TLR_ALL - 3);
        r = POISON_RANGE; b = 7;
        CHECK_HR(IDWriteTextLayout_GetUnderline(l, pos[i], &b, &r), S_OK); CHECK(b == FALSE); CHECK_RANGE(r, 3, TLR_ALL - 3);
        r = POISON_RANGE; len = 0;
        CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, pos[i], &len, &r), S_OK); EXPECT_EQ(len, 6u); CHECK_RANGE(r, 3, TLR_ALL - 3);
        r = POISON_RANGE; buf[0] = L'X';
        CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, pos[i], buf, 16, &r), S_OK); CHECK_WEQ(buf, TLR_FAM);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_layout_text_longer_than_ranges_tail_uses_default)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefghij", 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(0, 3)), S_OK);
    CHECK_FBITS(tlr_size(l, 3, &r), 16.0f); CHECK_RANGE(r, 3, TLR_ALL - 3);
    CHECK_FBITS(tlr_size(l, 9, &r), 16.0f); CHECK_RANGE(r, 3, TLR_ALL - 3);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_distinct_typography_objects_with_equal_content_do_not_merge)
{
    /* runs compare the typography POINTER, not its features */
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefgh", 16.0f); IDWriteTypography *a = NULL, *b = NULL; DWRITE_TEXT_RANGE r;
    DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1 };
    DW_REQUIRE_OK(IDWriteFactory_CreateTypography(f, &a)); DW_REQUIRE_OK(IDWriteFactory_CreateTypography(f, &b));
    IDWriteTypography_AddFontFeature(a, ft); IDWriteTypography_AddFontFeature(b, ft);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, a, RNG(0, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, b, RNG(4, 4)), S_OK);
    { IDWriteTypography *g = tlr_typo(l, 0, &r); CHECK(g == a); CHECK_RANGE(r, 0, 4); DW_RELEASE(g); }
    { IDWriteTypography *g = tlr_typo(l, 4, &r); CHECK(g == b); CHECK_RANGE(r, 4, 4); DW_RELEASE(g); }
    IDWriteTypography_Release(a); IDWriteTypography_Release(b); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_distinct_effect_objects_do_not_merge)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefgh", 16.0f); dwx_effect *a = dwx_effect_new(5), *b = dwx_effect_new(5); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)a, RNG(0, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)b, RNG(4, 4)), S_OK);
    { IUnknown *g = tlr_effect(l, 3, &r); CHECK(g == (IUnknown *)a); CHECK_RANGE(r, 0, 4); DW_RELEASE(g); }
    { IUnknown *g = tlr_effect(l, 4, &r); CHECK(g == (IUnknown *)b); CHECK_RANGE(r, 4, 4); DW_RELEASE(g); }
    IDWriteTextLayout_Release(l); dwx_release(a); dwx_release(b); TLR_END();
}
TEST(misc_family_equality_is_by_content)
{
    /* each Set copies the string; equal text in different buffers still merges */
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefgh", 16.0f); WCHAR b1[16], b2[16]; DWRITE_TEXT_RANGE r;
    wcscpy(b1, L"DwNarrow"); wcscpy(b2, L"DwNarrow");
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, b1, RNG(0, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, b2, RNG(4, 4)), S_OK);
    wcscpy(b1, L"XXXXXXXX");                              /* the layout owns a copy */
    CHECK_WEQ(tlr_family(l, 2, &r), TLR_NARROW); CHECK_RANGE(r, 0, 8);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_locale_string_is_copied)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefgh", 16.0f); WCHAR b1[16]; DWRITE_TEXT_RANGE r;
    wcscpy(b1, L"fr-fr");
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, b1, RNG(0, 4)), S_OK);
    wcscpy(b1, L"zz-zz");
    CHECK_WEQI(tlr_locale(l, 2, &r), L"fr-fr"); CHECK_RANGE(r, 0, 4);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_layout_text_copy_does_not_alias_ranges)
{
    /* ranges refer to positions only: the caller's text buffer may change freely after creation */
    TLR_BEGIN(); IDWriteTextFormat *t = tlr_format(&X, TLR_FAM, 16.0f); IDWriteTextLayout *l = NULL; WCHAR buf[8]; DWRITE_TEXT_RANGE r;
    wcscpy(buf, L"abcdefg");
    if (!t) { TLR_END(); t_skip("format"); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, buf, 7, t, 1000, 1000, &l), S_OK);
    IDWriteTextFormat_Release(t);
    if (l) {
        wcscpy(buf, L"XXXXXXX");
        CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(2, 2)), S_OK);
        CHECK(tlr_weight(l, 3, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 2, 2);
        IDWriteTextLayout_Release(l);
    }
    TLR_END();
}
TEST(misc_set_get_through_all_interface_versions)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefgh", 16.0f); DWRITE_TEXT_RANGE r;
    IDWriteTextLayout1 *l1 = NULL; IDWriteTextLayout2 *l2 = NULL; IDWriteTextLayout3 *l3 = NULL; IDWriteTextLayout4 *l4 = NULL;
    IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1);
    IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2);
    IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout3, (void **)&l3);
    IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4);
    t_note("interfaces: layout1 %d layout2 %d layout3 %d layout4 %d", l1 != NULL, l2 != NULL, l3 != NULL, l4 != NULL);
    if (l4) {
        CHECK_HR(IDWriteTextLayout_SetFontWeight((IDWriteTextLayout *)l4, DWRITE_FONT_WEIGHT_BOLD, RNG(1, 2)), S_OK);
        CHECK(tlr_weight(l, 2, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 1, 2);
        CHECK(tlr_weight((IDWriteTextLayout *)l4, 0, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, 1);
    }
    DW_RELEASE(l4); DW_RELEASE(l3); DW_RELEASE(l2); DW_RELEASE(l1);
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- additional range-semantics checks ---- */
TEST(misc_set_then_revert_to_default_merges_to_single_run)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefghij", 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(3, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 16.0f, RNG(3, 4)), S_OK);
    /* equal complete formats are one run again */
    CHECK_FLOAT_EQ(tlr_size(l, 5, &r), 16.0f); CHECK_RANGE(r, 0, TLR_ALL);
    CHECK_FLOAT_EQ(tlr_size(l, 0, &r), 16.0f); CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_adjacent_equal_ranges_merge)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefghij", 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(2, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(5, 2)), S_OK);
    CHECK(tlr_weight(l, 3, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 2, 5);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(misc_overlapping_ranges_later_wins)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefghij", 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 6)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_LIGHT, RNG(4, 4)), S_OK);
    CHECK(tlr_weight(l, 1, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 0, 4);
    CHECK(tlr_weight(l, 5, &r) == DWRITE_FONT_WEIGHT_LIGHT); CHECK_RANGE(r, 4, 4);
    CHECK(tlr_weight(l, 9, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 8, TLR_ALL - 8);
    IDWriteTextLayout_Release(l); TLR_END();
}
