/* Buffer protocol and argument handling of GetGlyphs/GetGlyphPlacements when shaping changes the glyph count (observed:
 * IDWriteTextAnalyzer::GetGlyphs clears *actualGlyphCount first, fontFace NULL -> E_INVALIDARG, length 0 -> S_OK, maxGlyphCount 0 ->
 * 0x8007007A, and the shaping status mapping -3 -> 0x8007007A). */
#include "r2.h"
#define LATN() FX_BEGIN(x, FX_LATN); shaped s
#define INSUFF ((HRESULT)0x8007007A)

static HRESULT raw_glyphs(fx *x, const WCHAR *t, UINT32 n, UINT32 maxg, UINT16 *cmap, DWRITE_SHAPING_TEXT_PROPERTIES *tp, UINT16 *gid, DWRITE_SHAPING_GLYPH_PROPERTIES *gp, UINT32 *act)
{
    DWRITE_SCRIPT_ANALYSIS sa = fx_script(x, t, n);
    return IDWriteTextAnalyzer_GetGlyphs(x->an, t, n, x->face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, maxg, cmap, tp, gid, gp, act);
}
TEST(r2_shaping_features_buf_expansion_needs_two_glyphs)
{   /* ccmp turns U+00E9 into two glyphs: a one glyph buffer is too small */
    LATN(); UINT16 cm[4], g[4]; DWRITE_SHAPING_TEXT_PROPERTIES tp[4]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[4]; UINT32 act = 77;
    CHECK_HR(raw_glyphs(&x, L"\x00e9", 1, 1, cm, tp, g, gp, &act), INSUFF);
    act = 77; CHECK_HR_OK(raw_glyphs(&x, L"\x00e9", 1, 2, cm, tp, g, gp, &act)); EXPECT_EQ(act, 2); EXPECT_EQ(g[0], 32); EXPECT_EQ(g[1], G_ACUTE);
    fx_close(&x);
}
TEST(r2_shaping_features_buf_zero_max_is_insufficient)
{
    LATN(); UINT16 cm[2], g[2]; DWRITE_SHAPING_TEXT_PROPERTIES tp[2]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[2]; UINT32 act = 77;
    CHECK_HR(raw_glyphs(&x, L"ab", 2, 0, cm, tp, g, gp, &act), INSUFF); EXPECT_EQ(act, 0); fx_close(&x);
}
TEST(r2_shaping_features_buf_ligature_fits_in_fewer_glyphs_than_chars_note)
{
    LATN(); UINT16 cm[3], g[3]; DWRITE_SHAPING_TEXT_PROPERTIES tp[3]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3]; UINT32 act = 77;
    HRESULT hr = raw_glyphs(&x, L"ffi", 3, 1, cm, tp, g, gp, &act);
    t_note("ffi with maxGlyphCount 1: %s actual %u", dw_hr_name(hr), act); CHECK(hr == S_OK || hr == INSUFF); fx_close(&x);
}
TEST(r2_shaping_features_buf_exact_count_succeeds)
{
    LATN(); UINT16 cm[3], g[3]; DWRITE_SHAPING_TEXT_PROPERTIES tp[3]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3]; UINT32 act = 77;
    CHECK_HR_OK(raw_glyphs(&x, L"abc", 3, 3, cm, tp, g, gp, &act)); EXPECT_EQ(act, 3); fx_close(&x);
}
TEST(r2_shaping_features_buf_one_short_of_text_length)
{
    LATN(); UINT16 cm[3], g[3]; DWRITE_SHAPING_TEXT_PROPERTIES tp[3]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3]; UINT32 act = 77;
    HRESULT hr = raw_glyphs(&x, L"abc", 3, 2, cm, tp, g, gp, &act); CHECK_HR(hr, INSUFF); fx_close(&x);
}
TEST(r2_shaping_features_buf_actual_cleared_first)
{
    LATN(); UINT16 cm[3], g[3]; DWRITE_SHAPING_TEXT_PROPERTIES tp[3]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3]; UINT32 act = 0x1234;
    DWRITE_SCRIPT_ANALYSIS sa = fx_script(&x, L"ab", 2);
    if (t_is_wine()) { fx_close(&x); t_skip("emulator dereferences a NULL face"); return; }
    CHECK_HR(IDWriteTextAnalyzer_GetGlyphs(x.an, L"ab", 2, NULL, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 3, cm, tp, g, gp, &act), E_INVALIDARG); EXPECT_EQ(act, 0);
    act = 0x1234; CHECK_HR_OK(IDWriteTextAnalyzer_GetGlyphs(x.an, L"ab", 0, x.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 3, cm, tp, g, gp, &act)); EXPECT_EQ(act, 0);
    fx_close(&x);
}
TEST(r2_shaping_features_buf_longer_than_needed_reports_actual)
{
    LATN(); UINT16 cm[64], g[64]; DWRITE_SHAPING_TEXT_PROPERTIES tp[64]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[64]; UINT32 act = 77;
    CHECK_HR_OK(raw_glyphs(&x, L"fi", 2, 64, cm, tp, g, gp, &act)); EXPECT_EQ(act, 1); EXPECT_EQ(g[0], G_FI); EXPECT_EQ(cm[0], 0); EXPECT_EQ(cm[1], 0); fx_close(&x);
}
TEST(r2_shaping_features_buf_unused_cluster_map_tail_untouched)
{   /* clusterMap/textProps have textLength entries only */
    LATN(); UINT16 cm[8], g[8]; DWRITE_SHAPING_TEXT_PROPERTIES tp[8]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[8]; UINT32 act = 77; int i;
    for (i = 0; i < 8; i++) cm[i] = 0xBEEF;
    CHECK_HR_OK(raw_glyphs(&x, L"abc", 3, 8, cm, tp, g, gp, &act));
    for (i = 3; i < 8; i++) CHECKF(cm[i] == 0xBEEF, "clusterMap[%d] = 0x%x", i, cm[i]); fx_close(&x);
}
TEST(r2_shaping_features_shape_is_deterministic)
{
    LATN(); shaped t; int i;
    for (i = 0; i < 20; i++) { CHECK_HR_OK(sh_run(&x, &s, L"ffi AV a\x0301 ab ck xy", FALSE, NULL, 0)); if (i == 0) t = s; else CHECK(s.actual == t.actual && !memcmp(s.gid, t.gid, s.actual * 2) && !memcmp(s.adv, t.adv, s.actual * 4)); }
    fx_close(&x);
}
TEST(r2_shaping_features_shape_mixed_sentence)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ffi AV a\x0301 ab", FALSE, NULL, 0));
    CHECK_GIDS(&s, G_FFI, 1, 2, 23, 1, 28, G_ACUTE, 1, 28, G_CALTB);
    CHECK_ADV(&s, 2000, 1000, 900, 1000, 1000, 1000, 0, 1000, 1000, 950); fx_close(&x);
}
TEST(r2_shaping_features_shape_cluster_map_mixed_sentence)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ffi AV a\x0301 ab", FALSE, NULL, 0));
    CHECK_CMAP(&s, 0, 0, 0, 1, 2, 3, 4, 5, 5, 7, 8, 9); fx_close(&x);
}
TEST(r2_shaping_features_placement_requires_matching_arrays)
{   /* GetGlyphPlacements with glyphCount 0 returns S_OK and does not touch advances/offsets (observed) */
    LATN(); float adv[2] = { 5, 5 }; DWRITE_GLYPH_OFFSET off[2] = { { 3, 3 }, { 3, 3 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"ab", 2, FALSE, NULL, NULL, 0));
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGlyphPlacements(x.an, L"ab", s.cmap, s.tp, 2, s.gid, s.gp, 0, x.face, 2048.0f, FALSE, FALSE, &s.sa, L"en-us", NULL, NULL, 0, adv, off));
    EXPECT_EQ((int)adv[0], 5); EXPECT_EQ((int)off[0].advanceOffset, 3); fx_close(&x);
}
TEST(r2_shaping_features_placement_em_zero_gives_zero_advances_note)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"AV", 2, FALSE, NULL, NULL, 0)); CHECK_HR_OK(sh_place(&x, &s, 0.0f, NULL, 0)); CHECK_ADV(&s, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_range_lengths_sum_mismatch_note)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { { (DWRITE_FONT_FEATURE_TAG)T_liga, 0 } };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { (DWRITE_FONT_FEATURE *)f, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *ptf[1] = { &tf }; UINT32 len[1] = { 1 };
    DWRITE_SCRIPT_ANALYSIS sa = fx_script(&x, L"fi", 2); UINT32 act = 0; UINT16 cm[4], g[4]; DWRITE_SHAPING_TEXT_PROPERTIES tp[4]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[4];
    HRESULT hr = IDWriteTextAnalyzer_GetGlyphs(x.an, L"fi", 2, x.face, FALSE, FALSE, &sa, L"en-us", NULL, ptf, len, 1, 4, cm, tp, g, gp, &act);
    t_note("range length 1 for 2 characters: %s, %u glyphs %s", dw_hr_name(hr), act, act ? (g[0] == G_FI ? "(ligated)" : "(not ligated)") : ""); fx_close(&x);
}
TEST(r2_shaping_features_script_analysis_shapes_default_vs_no_visual)
{   /* DWRITE_SCRIPT_SHAPES_NO_VISUAL on a script run: record behaviour of the shapes flag */
    LATN(); DWRITE_SCRIPT_ANALYSIS sa = fx_script(&x, L"fi", 2); UINT32 act = 0; UINT16 cm[4], g[4]; DWRITE_SHAPING_TEXT_PROPERTIES tp[4]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[4];
    sa.shapes = DWRITE_SCRIPT_SHAPES_NO_VISUAL;
    HRESULT hr = IDWriteTextAnalyzer_GetGlyphs(x.an, L"fi", 2, x.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 4, cm, tp, g, gp, &act);
    t_note("shapes=NO_VISUAL: %s n=%u g0=%u", dw_hr_name(hr), act, act ? g[0] : 0); CHECK(SUCCEEDED(hr)); fx_close(&x);
}
