/* GetGlyphs through GSUB lookup types 1,2,3,4 and 6 of the R2Latn synthetic font (see r2.h).  OpenType semantics hold on Windows:
 * lookups of the enabled features run in lookup-list order, a ligature set prefers its first matching entry, the clusterMap
 * of a ligature maps every component to the glyph that starts the cluster, a feature value of 0 switches a default-on feature off. */
#include "r2.h"

#define ON(t)  { (DWRITE_FONT_FEATURE_TAG)(t), 1 }
#define OFF(t) { (DWRITE_FONT_FEATURE_TAG)(t), 0 }
#define LATN() FX_BEGIN(x, FX_LATN); shaped s

/* ---- type 4 (ligature) ---- */
TEST(r2_shaping_features_liga_fi) { LATN(); CHECK_HR_OK(sh_run(&x, &s, L"fi", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FI); CHECK_CMAP(&s, 0, 0); CHECK_ADV(&s, 1500); fx_close(&x); }
TEST(r2_shaping_features_liga_ff) { LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ff", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FF); CHECK_CMAP(&s, 0, 0); CHECK_ADV(&s, 1400); fx_close(&x); }
TEST(r2_shaping_features_liga_ffi_longest_first)
{   /* the ligature set of 'f' lists ffi before ff before fi: the first match wins */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ffi", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FFI); CHECK_CMAP(&s, 0, 0, 0); CHECK_ADV(&s, 2000); fx_close(&x);
}
TEST(r2_shaping_features_liga_fff)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"fff", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FF, 33); CHECK_CMAP(&s, 0, 0, 1); CHECK_ADV(&s, 1400, 1000); fx_close(&x);
}
TEST(r2_shaping_features_liga_ffff)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ffff", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FF, G_FF); CHECK_CMAP(&s, 0, 0, 1, 1); fx_close(&x);
}
TEST(r2_shaping_features_liga_fffi)
{   /* f + ffi: the second f starts the ffi match */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"fffi", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FF, G_FI); CHECK_CMAP(&s, 0, 0, 1, 1); fx_close(&x);
}
TEST(r2_shaping_features_liga_in_word)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"afib", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_FI, 29); CHECK_CMAP(&s, 0, 1, 1, 2); CHECK_ADV(&s, 1000, 1500, 1000); fx_close(&x);
}
TEST(r2_shaping_features_liga_two_in_a_row)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"fifi", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FI, G_FI); CHECK_CMAP(&s, 0, 0, 1, 1); fx_close(&x);
}
TEST(r2_shaping_features_liga_no_match_f_alone)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"fa", FALSE, NULL, 0)); CHECK_GIDS(&s, 33, 28); CHECK_CMAP(&s, 0, 1); fx_close(&x);
}
TEST(r2_shaping_features_liga_not_across_space)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"f i", FALSE, NULL, 0)); CHECK_GIDS(&s, 33, 1, 36); fx_close(&x);
}
TEST(r2_shaping_features_liga_disabled)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_liga) };
    CHECK_HR_OK(sh_run(&x, &s, L"fi", FALSE, f, 1)); CHECK_GIDS(&s, 33, 36); CHECK_CMAP(&s, 0, 1); CHECK_ADV(&s, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_liga_explicit_on_same_as_default)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_liga) };
    CHECK_HR_OK(sh_run(&x, &s, L"ffi", FALSE, f, 1)); CHECK_GIDS(&s, G_FFI); fx_close(&x);
}
TEST(r2_shaping_features_liga_disabled_leaves_other_features)
{   /* switching liga off does not disable calt/clig */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_liga) };
    CHECK_HR_OK(sh_run(&x, &s, L"ckfi", FALSE, f, 1)); CHECK_GIDS(&s, G_CK, 33, 36); fx_close(&x);
}
TEST(r2_shaping_features_liga_glyph_props)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"afib", FALSE, NULL, 0));
    CHECK(s.actual == 3);
    CHECK(s.gp[0].isClusterStart); CHECK(s.gp[1].isClusterStart); CHECK(s.gp[2].isClusterStart);
    CHECK(!s.gp[0].isDiacritic); CHECK(!s.gp[1].isDiacritic); CHECK(!s.gp[2].isDiacritic);
    CHECK(!s.gp[1].isZeroWidthSpace);
    fx_close(&x);
}
TEST(r2_shaping_features_liga_text_props)
{   /* DWRITE_SHAPING_TEXT_PROPERTIES: one entry per UTF-16 unit; letters are neither shaping-blocked nor reversed */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"fi", FALSE, NULL, 0));
    CHECK(!s.tp[0].isShapedAlone); CHECK(!s.tp[1].isShapedAlone);
    CHECK(!s.tp[0].canBreakShapingAfter); fx_close(&x);
}
TEST(r2_shaping_features_clig_default_on)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ck", FALSE, NULL, 0)); CHECK_GIDS(&s, G_CK); CHECK_CMAP(&s, 0, 0); CHECK_ADV(&s, 1600); fx_close(&x);
}
TEST(r2_shaping_features_clig_disabled)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_clig) };
    CHECK_HR_OK(sh_run(&x, &s, L"ck", FALSE, f, 1)); CHECK_GIDS(&s, 30, 38); fx_close(&x);
}
TEST(r2_shaping_features_clig_and_liga_independent)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_clig) };
    CHECK_HR_OK(sh_run(&x, &s, L"cki", FALSE, f, 1)); CHECK_GIDS(&s, 30, 38, 36);
    CHECK_HR_OK(sh_run(&x, &s, L"fick", FALSE, f, 1)); CHECK_GIDS(&s, G_FI, 30, 38);
    fx_close(&x);
}
TEST(r2_shaping_features_rlig_default_on)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"xy", FALSE, NULL, 0)); CHECK_GIDS(&s, G_XY); CHECK_CMAP(&s, 0, 0); CHECK_ADV(&s, 1800); fx_close(&x);
}
TEST(r2_shaping_features_rlig_off_note)
{   /* 'rlig' is a required ligature feature; whether a client can switch it off is not specified */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_rlig) };
    CHECK_HR_OK(sh_run(&x, &s, L"xy", FALSE, f, 1));
    t_note("rlig=0 on 'xy': %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_rlig_reverse_order_no_match)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"yx", FALSE, NULL, 0)); CHECK_GIDS(&s, 52, 51); fx_close(&x);
}

/* ---- type 2 (multiple) ---- */
TEST(r2_shaping_features_ccmp_decomposes)
{   /* one character becomes two glyphs: the clusterMap has one entry, glyph 1 is part of the same cluster */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"\x00e9", FALSE, NULL, 0)); CHECK_GIDS(&s, 32, G_ACUTE); CHECK(s.cmap[0] == 0); fx_close(&x);
}
TEST(r2_shaping_features_ccmp_decomposes_advances)
{   /* the acute mark has advance 0; with mark attachment (offset) but unchanged pen advance */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"\x00e9", FALSE, NULL, 0)); CHECK_ADV(&s, 1000, 0); fx_close(&x);
}
TEST(r2_shaping_features_ccmp_disabled)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_ccmp) };
    CHECK_HR_OK(sh_run(&x, &s, L"\x00e9", FALSE, f, 1)); CHECK_GIDS(&s, G_EACUTE); CHECK_ADV(&s, 1000); fx_close(&x);
}
TEST(r2_shaping_features_ccmp_in_text)
{   /* clusterMap of the characters around a one-to-many substitution */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x00e9" L"b", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, 32, G_ACUTE, 29); CHECK_CMAP(&s, 0, 1, 3); fx_close(&x);
}
TEST(r2_shaping_features_ccmp_twice)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"\x00e9\x00e9", FALSE, NULL, 0)); CHECK_GIDS(&s, 32, G_ACUTE, 32, G_ACUTE); CHECK_CMAP(&s, 0, 2); fx_close(&x);
}
TEST(r2_shaping_features_ccmp_mark_props)
{   /* the second glyph of a decomposition is a mark of the same cluster: not a cluster start */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"\x00e9", FALSE, NULL, 0)); CHECK(s.actual == 2);
    CHECK(s.gp[0].isClusterStart); CHECK(!s.gp[1].isClusterStart); fx_close(&x);
}

/* ---- type 6 (chaining context) with a type 1 lookup ---- */
TEST(r2_shaping_features_calt_context_hit)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ab", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_CALTB); CHECK_ADV(&s, 1000, 950); CHECK_CMAP(&s, 0, 1); fx_close(&x);
}
TEST(r2_shaping_features_calt_context_miss_no_backtrack)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"b", FALSE, NULL, 0)); CHECK_GIDS(&s, 29); fx_close(&x);
}
TEST(r2_shaping_features_calt_context_miss_wrong_backtrack)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"cb", FALSE, NULL, 0)); CHECK_GIDS(&s, 30, 29); fx_close(&x);
}
TEST(r2_shaping_features_calt_only_second_b)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"abb", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_CALTB, 29); fx_close(&x);
}
TEST(r2_shaping_features_calt_every_pair)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"abab", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_CALTB, 28, G_CALTB); fx_close(&x);
}
TEST(r2_shaping_features_calt_disabled)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_calt) };
    CHECK_HR_OK(sh_run(&x, &s, L"ab", FALSE, f, 1)); CHECK_GIDS(&s, 28, 29); fx_close(&x);
}
TEST(r2_shaping_features_calt_backtrack_is_original_glyph)
{   /* smcp turns 'a' into small-cap A (not in the backtrack coverage), so the context no longer matches */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp) };
    CHECK_HR_OK(sh_run(&x, &s, L"ab", FALSE, f, 1));
    t_note("smcp+calt on 'ab': %s", sh_dump(&s)); CHECK(s.actual == 2); CHECK(s.gid[0] == G_SCA);
    fx_close(&x);
}

/* ---- type 1 (single) ---- */
TEST(r2_shaping_features_smcp_off_by_default)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"cd", FALSE, NULL, 0)); CHECK_GIDS(&s, 30, 31);
    CHECK_HR_OK(sh_run(&x, &s, L"ba", FALSE, NULL, 0)); CHECK_GIDS(&s, 29, 28); fx_close(&x);
}
TEST(r2_shaping_features_smcp_on)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp) };
    CHECK_HR_OK(sh_run(&x, &s, L"ba", FALSE, f, 1)); CHECK_GIDS(&s, G_SCB, G_SCA); CHECK_ADV(&s, 700, 800); CHECK_CMAP(&s, 0, 1); fx_close(&x);
}
TEST(r2_shaping_features_smcp_uncovered_letters_unchanged)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp) };
    CHECK_HR_OK(sh_run(&x, &s, L"cA1", FALSE, f, 1)); CHECK_GIDS(&s, 30, 2, 55); fx_close(&x);
}
TEST(r2_shaping_features_smcp_value_zero_is_off)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_smcp) };
    CHECK_HR_OK(sh_run(&x, &s, L"ab", FALSE, f, 1)); CHECK_GIDS(&s, 28, G_CALTB); fx_close(&x);
}
TEST(r2_shaping_features_c2sc_on)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_c2sc) };
    CHECK_HR_OK(sh_run(&x, &s, L"AB", FALSE, f, 1)); CHECK_GIDS(&s, G_SCA, G_SCB); fx_close(&x);
}
TEST(r2_shaping_features_c2sc_leaves_lowercase)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_c2sc) };
    CHECK_HR_OK(sh_run(&x, &s, L"aB", FALSE, f, 1)); CHECK_GIDS(&s, 28, G_SCB); fx_close(&x);
}
TEST(r2_shaping_features_smcp_and_c2sc_together)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp), ON(T_c2sc) };
    CHECK_HR_OK(sh_run(&x, &s, L"aAbB", FALSE, f, 2)); CHECK_GIDS(&s, G_SCA, G_SCA, G_SCB, G_SCB); fx_close(&x);
}
TEST(r2_shaping_features_ss01_on)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_ss01) };
    CHECK_HR_OK(sh_run(&x, &s, L"ac", FALSE, f, 1)); CHECK_GIDS(&s, G_SS01A, 30); CHECK_ADV(&s, 900, 1000); fx_close(&x);
}
TEST(r2_shaping_features_ss01_off_by_default)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ac", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, 30); fx_close(&x);
}
TEST(r2_shaping_features_pnum_on)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_pnum) };
    CHECK_HR_OK(sh_run(&x, &s, L"1", FALSE, f, 1)); CHECK_GIDS(&s, G_P1); CHECK_ADV(&s, 600); fx_close(&x);
}
TEST(r2_shaping_features_tnum_on)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_tnum) };
    CHECK_HR_OK(sh_run(&x, &s, L"1", FALSE, f, 1)); CHECK_GIDS(&s, G_T1); CHECK_ADV(&s, 1000); fx_close(&x);
}
TEST(r2_shaping_features_pnum_and_tnum_lookup_order)
{   /* both on: pnum (lookup 10) runs first, its output glyph is not covered by tnum (lookup 11) */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_pnum), ON(T_tnum) };
    CHECK_HR_OK(sh_run(&x, &s, L"1", FALSE, f, 2)); CHECK_GIDS(&s, G_P1); fx_close(&x);
}
TEST(r2_shaping_features_pnum_off_by_default)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"11", FALSE, NULL, 0)); CHECK_GIDS(&s, 55, 55); fx_close(&x);
}
TEST(r2_shaping_features_zero_delta_format)
{   /* GSUB single substitution format 1 (delta 32): 0 -> 86 */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_zero) };
    CHECK_HR_OK(sh_run(&x, &s, L"10", FALSE, f, 1)); CHECK_GIDS(&s, 55, G_ZERO); fx_close(&x);
}
TEST(r2_shaping_features_zero_off_by_default)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"0", FALSE, NULL, 0)); CHECK_GIDS(&s, 54); fx_close(&x);
}

/* ---- type 3 (alternates) ---- */
TEST(r2_shaping_features_salt_first_alternate)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { { (DWRITE_FONT_FEATURE_TAG)T_salt, 1 } };
    CHECK_HR_OK(sh_run(&x, &s, L"A", FALSE, f, 1)); CHECK_GIDS(&s, G_ALT1); CHECK_ADV(&s, 1100); fx_close(&x);
}
TEST(r2_shaping_features_salt_second_alternate)
{   /* the feature parameter selects the alternate (1-based) */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { { (DWRITE_FONT_FEATURE_TAG)T_salt, 2 } };
    CHECK_HR_OK(sh_run(&x, &s, L"A", FALSE, f, 1)); CHECK_GIDS(&s, G_ALT2); CHECK_ADV(&s, 1200); fx_close(&x);
}
TEST(r2_shaping_features_salt_zero_is_off)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { { (DWRITE_FONT_FEATURE_TAG)T_salt, 0 } };
    CHECK_HR_OK(sh_run(&x, &s, L"A", FALSE, f, 1)); CHECK_GIDS(&s, 2); fx_close(&x);
}
TEST(r2_shaping_features_salt_index_beyond_set_note)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { { (DWRITE_FONT_FEATURE_TAG)T_salt, 3 } };
    CHECK_HR_OK(sh_run(&x, &s, L"A", FALSE, f, 1)); CHECK(s.actual == 1);
    t_note("salt=3 (only 2 alternates): glyph %u", s.gid[0]); fx_close(&x);
}
TEST(r2_shaping_features_salt_uncovered_glyph_unchanged)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { { (DWRITE_FONT_FEATURE_TAG)T_salt, 1 } };
    CHECK_HR_OK(sh_run(&x, &s, L"BA", FALSE, f, 1)); CHECK_GIDS(&s, 3, G_ALT1); fx_close(&x);
}
TEST(r2_shaping_features_salt_off_by_default)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"A", FALSE, NULL, 0)); CHECK_GIDS(&s, 2); fx_close(&x);
}

/* ---- locl and language systems ---- */
TEST(r2_shaping_features_locl_not_applied_for_english)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"i", 1, FALSE, L"en-us", NULL, 0)); CHECK_GIDS(&s, 36); fx_close(&x);
}
TEST(r2_shaping_features_locl_turkish_language_system)
{   /* locl only exists in the 'TRK ' language system of latn */
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"i", 1, FALSE, L"tr-tr", NULL, 0));
    t_note("tr-tr 'i': %s", sh_dump(&s)); CHECK(s.actual == 1); fx_close(&x);
}
TEST(r2_shaping_features_locl_explicit_off_for_turkish)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_locl) }; featrange fr = { f, 1, 1 };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"i", 1, FALSE, L"tr-tr", &fr, 1)); CHECK_GIDS(&s, 36); fx_close(&x);
}
TEST(r2_shaping_features_locl_unknown_locale_is_default_language)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"i", 1, FALSE, L"xx-yy", NULL, 0)); CHECK_GIDS(&s, 36); fx_close(&x);
}
TEST(r2_shaping_features_locale_null_and_empty)
{
    LATN(); DWRITE_SCRIPT_ANALYSIS sa = fx_script(&x, L"fi", 2); UINT32 n = 0;
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGlyphs(x.an, L"fi", 2, x.face, FALSE, FALSE, &sa, NULL, NULL, NULL, NULL, 0, 16, s.cmap, s.tp, s.gid, s.gp, &n));
    CHECK(n == 1 && s.gid[0] == G_FI);
    n = 0;
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGlyphs(x.an, L"fi", 2, x.face, FALSE, FALSE, &sa, L"", NULL, NULL, NULL, 0, 16, s.cmap, s.tp, s.gid, s.gp, &n));
    CHECK(n == 1 && s.gid[0] == G_FI);
    fx_close(&x);
}

/* ---- feature ranges ---- */
TEST(r2_shaping_features_range_first_half)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp) }; featrange fr[] = { { f, 1, 2 }, { NULL, 0, 2 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"bacd", 4, FALSE, NULL, fr, 2)); CHECK_GIDS(&s, G_SCB, G_SCA, 30, 31); fx_close(&x);
}
TEST(r2_shaping_features_range_second_half)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp) }; featrange fr[] = { { NULL, 0, 2 }, { f, 1, 2 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"cdba", 4, FALSE, NULL, fr, 2)); CHECK_GIDS(&s, 30, 31, G_SCB, G_SCA); fx_close(&x);
}
TEST(r2_shaping_features_range_middle)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp) }; featrange fr[] = { { NULL, 0, 1 }, { f, 1, 2 }, { NULL, 0, 1 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"cbad", 4, FALSE, NULL, fr, 3)); CHECK_GIDS(&s, 30, G_SCB, G_SCA, 31); fx_close(&x);
}
TEST(r2_shaping_features_range_different_features)
{
    LATN(); static const DWRITE_FONT_FEATURE f1[] = { ON(T_smcp) }, f2[] = { ON(T_ss01) }; featrange fr[] = { { f1, 1, 1 }, { f2, 1, 1 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"aa", 2, FALSE, NULL, fr, 2)); CHECK_GIDS(&s, G_SCA, G_SS01A); fx_close(&x);
}
TEST(r2_shaping_features_range_ligature_not_split_by_disabled_range)
{   /* liga off for the second character only: the f+i ligature needs both characters in a range with liga */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_liga) }; featrange fr[] = { { NULL, 0, 1 }, { f, 1, 1 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"fi", 2, FALSE, NULL, fr, 2));
    t_note("fi with liga off on 'i' only: %s", sh_dump(&s)); CHECK(s.actual == 1 || s.actual == 2); fx_close(&x);
}
TEST(r2_shaping_features_range_liga_off_whole_first_word)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_liga) }; featrange fr[] = { { f, 1, 3 }, { NULL, 0, 3 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"fi fi", 5, FALSE, NULL, fr, 2));
    t_note("ranges over 'fi fi' (5 chars, ranges 3+3): %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_range_exact_cover)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_liga) }; featrange fr[] = { { f, 1, 2 }, { NULL, 0, 3 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"fi fi", 5, FALSE, NULL, fr, 2)); CHECK_GIDS(&s, 33, 36, 1, G_FI); CHECK_CMAP(&s, 0, 1, 2, 3, 3); fx_close(&x);
}
TEST(r2_shaping_features_range_single_range_equals_whole)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp) }; shaped t; featrange fr = { f, 1, 4 };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"bacd", 4, FALSE, NULL, &fr, 1)); CHECK_HR_OK(sh_run(&x, &t, L"bacd", FALSE, f, 1));
    CHECK(s.actual == t.actual && !memcmp(s.gid, t.gid, 4 * sizeof(UINT16))); fx_close(&x);
}
TEST(r2_shaping_features_range_empty_feature_list)
{
    LATN(); featrange fr = { NULL, 0, 2 };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"fi", 2, FALSE, NULL, &fr, 1)); CHECK_GIDS(&s, G_FI); fx_close(&x);
}
TEST(r2_shaping_features_range_many_features_one_range)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_smcp), ON(T_ss01), ON(T_pnum), OFF(T_liga), OFF(T_calt), ON(T_c2sc) };
    CHECK_HR_OK(sh_run(&x, &s, L"fiA1a", FALSE, f, 6)); CHECK_GIDS(&s, 33, 36, G_SCA, G_P1, G_SCA); fx_close(&x);
}
TEST(r2_shaping_features_duplicate_feature_last_wins_note)
{   /* the same tag twice in one list with different values */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_liga), OFF(T_liga) };
    CHECK_HR_OK(sh_run(&x, &s, L"fi", FALSE, f, 2)); CHECK(s.actual >= 1);
    t_note("liga=1 then liga=0: %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_unknown_feature_tag_ignored)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { FEAT(TG('z','z','z','z'), 1) };
    CHECK_HR_OK(sh_run(&x, &s, L"fi", FALSE, f, 1)); CHECK_GIDS(&s, G_FI); fx_close(&x);
}
TEST(r2_shaping_features_feature_with_large_value)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { FEAT(T_smcp, 0xFFFFFFFFu) };
    CHECK_HR_OK(sh_run(&x, &s, L"ab", FALSE, f, 1)); CHECK(s.actual == 2);
    t_note("smcp=0xFFFFFFFF: %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_scales_with_text_length)
{   /* longer runs: 50 'fi' pairs */
    LATN(); WCHAR t[101]; int i; for (i = 0; i < 50; i++) { t[2 * i] = 'f'; t[2 * i + 1] = 'i'; } t[100] = 0;
    CHECK_HR_OK(sh_run(&x, &s, t, FALSE, NULL, 0)); CHECK(s.actual == 50);
    for (i = 0; i < 50; i++) CHECKF(s.gid[i] == G_FI && s.cmap[2 * i] == i && s.cmap[2 * i + 1] == i, "pair %d: gid %u cmap %u %u", i, s.gid[i], s.cmap[2 * i], s.cmap[2 * i + 1]);
    fx_close(&x);
}
