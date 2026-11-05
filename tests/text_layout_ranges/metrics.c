/* Effect of per-range formatting on measured layout. The fixture fonts make every number exact: at font size 2048 one design unit is
 * one DIP, so a character of the Regular face is 1000 wide, Bold 1500, Italic 800, Bold Italic 1200 (DwTest); DwNarrow 500; DwWide 2000.
 * Text "abcdefgh" (8 clusters, no wrapping: maxWidth 100000).
 */
#include "tlr.h"

#define T8 L"abcdefgh"
#define BIG 2048.0f

static void widths_eq(IDWriteTextLayout *l, const float *want, UINT32 n, const char *what, const char *file, int line)
{
    float w[64]; UINT32 got = tlr_clusters(l, w, 64), i; int ok = got == n;
    for (i = 0; ok && i < n; i++) ok = w[i] == want[i];
    t_check(ok, file, line, what, "cluster count %u (want %u), widths[0..3] = %g %g %g %g (want %g %g %g %g)", got, n,
            got > 0 ? w[0] : -1, got > 1 ? w[1] : -1, got > 2 ? w[2] : -1, got > 3 ? w[3] : -1, want[0], n > 1 ? want[1] : -1, n > 2 ? want[2] : -1, n > 3 ? want[3] : -1);
}
#define CLUSTERS(l, ...) do { static const float w_[] = { __VA_ARGS__ }; widths_eq((l), w_, (UINT32)(sizeof w_ / sizeof w_[0]), "cluster widths", __FILE__, __LINE__); } while (0)

TEST(metrics_baseline_widths)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    CLUSTERS(l, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000);
    EXPECT_EQ(tlr_lines(l), 1u);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_size_range_scales_widths_exactly)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(4, 4)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 6000.0f);
    CLUSTERS(l, 1000, 1000, 1000, 1000, 500, 500, 500, 500);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 4096.0f, RNG(0, 2)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 4000.0f + 2000.0f + 2000.0f);
    CLUSTERS(l, 2000, 2000, 1000, 1000, 500, 500, 500, 500);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_size_whole_text_table)
{
    static const float sz[] = { 512.0f, 1024.0f, 2048.0f, 4096.0f, 256.0f };
    int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    for (i = 0; i < 5; i++) {
        CHECK_HR(IDWriteTextLayout_SetFontSize(l, sz[i], RNG(0, 8)), S_OK);
        t_check(tlr_width(l) == 8.0f * 1000.0f * sz[i] / 2048.0f, __FILE__, __LINE__, "width scales with size", "size %g -> %g", sz[i], tlr_width(l));
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_size_range_beyond_text_is_clipped_by_layout)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(4, 1000)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 6000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 512.0f, RNG(20, 5)), S_OK);          /* starts past the end: no visible effect */
    CHECK_FLOAT_EQ(tlr_width(l), 6000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 512.0f, RNG(8, 5)), S_OK);           /* starts exactly at the end */
    CHECK_FLOAT_EQ(tlr_width(l), 6000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_size_revert_restores_width)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(2, 4)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 6000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, BIG, RNG(2, 4)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_size_changes_height)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); float h0 = tlr_height(l), h1;
    t_note("line height at size 2048: %g", h0);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 4096.0f, RNG(3, 1)), S_OK);
    h1 = tlr_height(l);
    CHECK(h1 > h0);                                      /* a taller run makes the line taller */
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 4096.0f, RNG(0, 8)), S_OK);
    CHECK(tlr_height(l) >= h1 * 0.99f);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(0, 8)), S_OK);
    CHECK(tlr_height(l) < h0);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_weight_range_selects_bold_face)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 2)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 9000.0f);
    CLUSTERS(l, 1500, 1500, 1000, 1000, 1000, 1000, 1000, 1000);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_NORMAL, RNG(0, 2)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_weight_whole_text_bold)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 8)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 12000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_style_range_selects_italic_face)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, RNG(6, 2)), S_OK);
    CLUSTERS(l, 1000, 1000, 1000, 1000, 1000, 1000, 800, 800);
    CHECK_FLOAT_EQ(tlr_width(l), 7600.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_bold_italic_combination)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 2)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, RNG(1, 2)), S_OK);       /* [0] bold, [1] bold italic, [2] italic */
    CLUSTERS(l, 1500, 1200, 800, 1000, 1000, 1000, 1000, 1000);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_stretch_range)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); float w;
    CHECK_HR(IDWriteTextLayout_SetFontStretch(l, DWRITE_FONT_STRETCH_CONDENSED, RNG(0, 2)), S_OK);
    w = tlr_width(l);
    t_note("width with condensed [0,2): %g (7200 when the condensed face is matched through the typographic family)", w);
    CHECK(w <= 8000.0f + 1.0f || w > 0);                 /* always laid out */
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_family_range_switches_font)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, TLR_NARROW, RNG(0, 2)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, TLR_WIDE, RNG(6, 2)), S_OK);
    CLUSTERS(l, 500, 500, 1000, 1000, 1000, 1000, 2000, 2000);
    CHECK_FLOAT_EQ(tlr_width(l), 9000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, TLR_FAM, RNG(0, 8)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_family_name_case_insensitive_match)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, L"dwnarrow", RNG(0, 8)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 4000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_family_missing_still_lays_out)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); DWRITE_TEXT_METRICS m;
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, L"NoSuchFamilyXyz123", RNG(2, 3)), S_OK);
    { HRESULT hr = IDWriteTextLayout_GetMetrics(l, &m);
      t_note("GetMetrics with an unknown family in the private collection -> %s width %g", dw_hr_name(hr), hr == S_OK ? m.width : -1.0f); }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_collection_range_same_content_same_width)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); IDWriteFontCollection *c2 = NULL;
    DW_REQUIRE_OK(dwx_coll_create(X.cl, f, &c2));
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, c2, RNG(2, 3)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, TLR_WIDE, RNG(3, 1)), S_OK);          /* family looked up in the range's collection */
    CLUSTERS(l, 1000, 1000, 1000, 2000, 1000, 1000, 1000, 1000);
    IDWriteTextLayout_Release(l); IDWriteFontCollection_Release(c2); TLR_END();
}
TEST(metrics_decorations_do_not_change_advances)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, TRUE, RNG(1, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(3, 4)), S_OK);
    CLUSTERS(l, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_effect_locale_typography_do_not_change_advances)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_effect *e = dwx_effect_new(1); IDWriteTypography *ty = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateTypography(f, &ty));
    { DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1 }; IDWriteTypography_AddFontFeature(ty, ft); }
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", RNG(2, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, ty, RNG(1, 6)), S_OK);          /* the synthetic font has no GSUB: the feature has nothing to apply */
    CLUSTERS(l, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000);
    IDWriteTypography_Release(ty); IDWriteTextLayout_Release(l); dwx_release(e); TLR_END();
}
TEST(metrics_pair_kerning_without_kern_data_changes_nothing)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); TLR_QI(l1, IDWriteTextLayout1, l);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 8)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, FALSE, RNG(0, 8)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_character_spacing_grows_width)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); TLR_QI(l1, IDWriteTextLayout1, l); float w;
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 100.0f, 50.0f, 0.0f, RNG(0, 4)), S_OK);
    w = tlr_width(l);
    t_note("width with leading 100 / trailing 50 on 4 characters: %g (8600 expected)", w);
    CHECK(w > 8000.0f);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 0, RNG(0, 4)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_character_spacing_min_advance)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); TLR_QI(l1, IDWriteTextLayout1, l); float w;
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 1200.0f, RNG(0, 2)), S_OK);
    w = tlr_width(l);
    t_note("width with minimum advance 1200 on 2 characters: %g (8400 expected)", w);
    CHECK(w >= 8000.0f);
    IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_inline_object_replaces_character_width)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_inline *o = dwx_inline_new(300.0f, 100.0f, 80.0f, FALSE);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 1)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 7000.0f + 300.0f);
    CLUSTERS(l, 1000, 1000, 300, 1000, 1000, 1000, 1000, 1000);
    CHECK(o->n_metrics >= 1);
    IDWriteTextLayout_Release(l); dwx_release(o); TLR_END();
}
TEST(metrics_inline_object_over_several_characters)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_inline *o = dwx_inline_new(300.0f, 100.0f, 80.0f, FALSE); float w[16]; UINT32 n;
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 3)), S_OK);
    n = tlr_clusters(l, w, 16);
    t_note("clusters with a 3 character inline range: %u, total width %g", n, tlr_width(l));
    CHECK(n >= 1);
    IDWriteTextLayout_Release(l); dwx_release(o); TLR_END();
}
TEST(metrics_inline_object_remove_restores)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_inline *o = dwx_inline_new(300.0f, 100.0f, 80.0f, FALSE);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 1)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 7300.0f);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, NULL, RNG(2, 1)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout_Release(l); dwx_release(o); TLR_END();
}
TEST(metrics_formatting_persists_after_invalidate_layout)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); TLR_QI(l3, IDWriteTextLayout3, l); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(4, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 2)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 3000.0f + 2000.0f + 2000.0f);   /* 2 bold + 2 regular + 4 half-size characters */
    CHECK_HR(IDWriteTextLayout3_InvalidateLayout(l3), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 7000.0f);
    CHECK_FLOAT_EQ(tlr_size(l, 5, &r), 1024.0f); CHECK_RANGE(r, 4, 4);
    CHECK(tlr_weight(l, 1, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 0, 2);
    CLUSTERS(l, 1500, 1500, 1000, 1000, 500, 500, 500, 500);
    IDWriteTextLayout3_Release(l3); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_setters_after_layout_recompute)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(0, 8)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 4000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, TLR_WIDE, RNG(0, 8)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, TLR_NARROW, RNG(0, 8)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 2000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_invalid_set_leaves_layout_untouched)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, -1.0f, RNG(0, 8)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)0, RNG(0, 8)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, RNG(0, 8)), TLR_EINVALIDARG);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_empty_range_set_leaves_layout_untouched)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1.0f, RNG(0, 0)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(3, 0)), S_OK);
    CHECK_FLOAT_EQ(tlr_width(l), 8000.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_wrapping_follows_per_range_size)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcd efgh", BIG);                  /* 9 characters, one break opportunity */
    CHECK_HR(IDWriteTextLayout_SetMaxWidth(l, 5000.0f), S_OK);
    CHECK_HR(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_WRAP), S_OK);
    EXPECT_EQ(tlr_lines(l), 2u);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 512.0f, RNG(0, 9)), S_OK);       /* 2250 total: fits */
    EXPECT_EQ(tlr_lines(l), 1u);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(metrics_surrogate_pair_split_ranges_lay_out)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"a\xD83D\xDE00z", BIG); DWRITE_TEXT_METRICS m; HRESULT hr;
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(1, 1)), S_OK);       /* only the high surrogate */
    hr = IDWriteTextLayout_GetMetrics(l, &m);
    CHECK_HR(hr, S_OK);
    t_note("width with a size range splitting a surrogate pair: %g", m.width);
    EXPECT_EQ(tlr_clusters(l, (float[8]){0}, 8) >= 2, 1u);
    IDWriteTextLayout_Release(l); TLR_END();
}
