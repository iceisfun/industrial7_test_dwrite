/* GetGlyphPlacements through GPOS lookup types 1, 2 (formats 1 and 2), 4 and 6 of the R2Latn synthetic font (see r2.h), at em 2048
 * so that design units are DIPs.  GPOS pair adjustments change the advance of the first glyph; mark attachment is reported as a
 * glyph offset RELATIVE TO THE MARK'S OWN PEN POSITION (advanceOffset = baseAnchor.x - markAnchor.x - distance from the base
 * origin to the mark's pen, ascenderOffset = baseAnchor.y - markAnchor.y, y up). */
#include "r2.h"
#define ON(t)  { (DWRITE_FONT_FEATURE_TAG)(t), 1 }
#define OFF(t) { (DWRITE_FONT_FEATURE_TAG)(t), 0 }
#define LATN() FX_BEGIN(x, FX_LATN); shaped s

/* ---- kern: pair format 1 ---- */
TEST(r2_shaping_features_kern_pair_av)  { LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AV", FALSE, NULL, 0)); CHECK_GIDS(&s, 2, 23); CHECK_ADV(&s, 900, 1000); fx_close(&x); }
TEST(r2_shaping_features_kern_pair_va)  { LATN(); CHECK_HR_OK(sh_run(&x, &s, L"VA", FALSE, NULL, 0)); CHECK_ADV(&s, 900, 1000); fx_close(&x); }
TEST(r2_shaping_features_kern_pair_to)  { LATN(); CHECK_HR_OK(sh_run(&x, &s, L"To", FALSE, NULL, 0)); CHECK_ADV(&s, 850, 1000); fx_close(&x); }
TEST(r2_shaping_features_kern_pair_ay)  { LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AY", FALSE, NULL, 0)); CHECK_ADV(&s, 920, 1000); fx_close(&x); }
TEST(r2_shaping_features_kern_first_glyph_covered_pair_absent)
{   /* 'A' is covered but A+B is not a pair: falls through to the class subtable, which does not know A as first glyph */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AB", FALSE, NULL, 0)); CHECK_ADV(&s, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_chain_avas)
{   /* A V A: pairs (A,V) and (V,A) both apply, each to the first glyph of the pair */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AVA", FALSE, NULL, 0)); CHECK_ADV(&s, 900, 900, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_lowercase_not_kerned)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"av", FALSE, NULL, 0)); CHECK_ADV(&s, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_not_across_space)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"A V", FALSE, NULL, 0)); CHECK_ADV(&s, 1000, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_disabled)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_kern) };
    CHECK_HR_OK(sh_run(&x, &s, L"AVTo", FALSE, f, 1)); CHECK_ADV(&s, 1000, 1000, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_explicit_on)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_kern) };
    CHECK_HR_OK(sh_run(&x, &s, L"AV", FALSE, f, 1)); CHECK_ADV(&s, 900, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_does_not_change_glyphs_or_clusters)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AVTo", FALSE, NULL, 0)); CHECK_GIDS(&s, 2, 23, 21, 42); CHECK_CMAP(&s, 0, 1, 2, 3); fx_close(&x);
}
TEST(r2_shaping_features_kern_leaves_offsets_zero)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AV", FALSE, NULL, 0)); CHECK_OFF(&s, 0, 0, 0); CHECK_OFF(&s, 1, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_kern_scales_with_em)
{
    LATN(); static const struct { float em, a0, a1; } k[] = { { 1024, 450, 500 }, { 4096, 1800, 2000 }, { 512, 225, 250 }, { 256, 112.5f, 125 } };
    int i;
    CHECK_HR_OK(sh_glyphs(&x, &s, L"AV", 2, FALSE, NULL, NULL, 0));
    for (i = 0; i < 4; i++) { CHECK_HR_OK(sh_place(&x, &s, k[i].em, NULL, 0)); CHECKF(s.adv[0] == k[i].a0 && s.adv[1] == k[i].a1, "em %g: %g %g", k[i].em, s.adv[0], s.adv[1]); }
    fx_close(&x);
}
TEST(r2_shaping_features_kern_at_em_16)
{   /* 900 * 16 / 2048 = 7.03125 and 1000 * 16 / 2048 = 7.8125 are exact in float */
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"AV", 2, FALSE, NULL, NULL, 0)); CHECK_HR_OK(sh_place(&x, &s, 16.0f, NULL, 0));
    CHECK_FLOAT_EQ(s.adv[0], 7.03125f); CHECK_FLOAT_EQ(s.adv[1], 7.8125f); fx_close(&x);
}
TEST(r2_shaping_features_kern_range_split_between_pair)
{   /* kern off for the second glyph's range only; the lookup applies to the pair when the first glyph's range has kern */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_kern) }; featrange fr[] = { { NULL, 0, 1 }, { f, 1, 1 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"AV", 2, FALSE, NULL, fr, 2)); CHECK_HR_OK(sh_place(&x, &s, EM2048, fr, 2));
    t_note("kern off for 'V' only: %s adv %g %g", sh_dump(&s), s.adv[0], s.adv[1]); CHECK(s.adv[1] == 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_range_first_glyph_off)
{   /* kern off for the first glyph only: the adjustment would land on that glyph, so it is not applied */
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_kern) }; featrange fr[] = { { f, 1, 1 }, { NULL, 0, 1 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"AV", 2, FALSE, NULL, fr, 2)); CHECK_HR_OK(sh_place(&x, &s, EM2048, fr, 2));
    t_note("kern off for 'A' only: adv %g %g", s.adv[0], s.adv[1]); CHECK(s.adv[1] == 1000); fx_close(&x);
}

/* ---- kern: pair format 2 (class based) ---- */
TEST(r2_shaping_features_kern_class_wa) { LATN(); CHECK_HR_OK(sh_run(&x, &s, L"WA", FALSE, NULL, 0)); CHECK_ADV(&s, 940, 1000); fx_close(&x); }
TEST(r2_shaping_features_kern_class_wo) { LATN(); CHECK_HR_OK(sh_run(&x, &s, L"Wo", FALSE, NULL, 0)); CHECK_ADV(&s, 940, 1000); fx_close(&x); }
TEST(r2_shaping_features_kern_class_zero_class_no_kern)
{   /* second glyph in class 0 (not listed in classDef2): matrix [1][0] = 0 */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"WB", FALSE, NULL, 0)); CHECK_ADV(&s, 1000, 1000); CHECK_HR_OK(sh_run(&x, &s, L"Wa", FALSE, NULL, 0)); CHECK_ADV(&s, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_class_first_not_covered)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"XA", FALSE, NULL, 0)); CHECK_ADV(&s, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_class_disabled)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_kern) };
    CHECK_HR_OK(sh_run(&x, &s, L"WA", FALSE, f, 1)); CHECK_ADV(&s, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_both_subtables_in_text)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"WAVoAV", FALSE, NULL, 0));
    /* W+A (class) -60, A+V (pair) -100, V+o none, o+A none, A+V -100 */
    CHECK_ADV(&s, 940, 900, 1000, 1000, 900, 1000); fx_close(&x);
}

/* ---- kern vs marks and lookup flags ---- */
TEST(r2_shaping_features_kern_blocked_by_mark_without_ignore_flag)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"A\x0301V", FALSE, NULL, 0)); CHECK_GIDS(&s, 2, G_ACUTE, 23); CHECK_ADV(&s, 1000, 0, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_ignore_marks_flag)
{   /* lookupFlag IgnoreMarks (0x8): the mark between A and V is skipped when matching the pair */
    FX_BEGIN(x, FX_LATN_IGNM); shaped s;
    CHECK_HR_OK(sh_run(&x, &s, L"A\x0301V", FALSE, NULL, 0)); CHECK_GIDS(&s, 2, G_ACUTE, 23); CHECK_ADV(&s, 900, 0, 1000); fx_close(&x);
}
TEST(r2_shaping_features_kern_ignore_marks_flag_plain_pair_unchanged)
{
    FX_BEGIN(x, FX_LATN_IGNM); shaped s;
    CHECK_HR_OK(sh_run(&x, &s, L"AV", FALSE, NULL, 0)); CHECK_ADV(&s, 900, 1000); fx_close(&x);
}
TEST(r2_shaping_features_liga_blocked_by_mark_without_ignore_flag)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"f\x0301i", FALSE, NULL, 0)); CHECK_GIDS(&s, 33, G_ACUTE, 36); fx_close(&x);
}
TEST(r2_shaping_features_liga_ignore_marks_flag)
{   /* with IgnoreMarks the mark does not interrupt f..i: the ligature forms and the mark follows it */
    FX_BEGIN(x, FX_LATN_IGNM); shaped s;
    CHECK_HR_OK(sh_run(&x, &s, L"f\x0301i", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FI, G_ACUTE);
    t_note("clusterMap for f+acute+i with IgnoreMarks: %u %u %u", s.cmap[0], s.cmap[1], s.cmap[2]); fx_close(&x);
}

/* ---- mark-to-base (type 4) ---- */
TEST(r2_shaping_features_mark_attach_offset)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0301", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_ACUTE); CHECK_ADV(&s, 1000, 0);
    CHECK_OFF(&s, 0, 0, 0); CHECK_OFF(&s, 1, -600, 1400); fx_close(&x);
}
TEST(r2_shaping_features_mark_circumflex_same_anchor)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0302", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_CIRC); CHECK_OFF(&s, 1, -600, 1400); fx_close(&x);
}
TEST(r2_shaping_features_mark_on_capital)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"E\x0301", FALSE, NULL, 0)); CHECK_OFF(&s, 1, -600, 1400); fx_close(&x);
}
TEST(r2_shaping_features_mark_cluster_map_and_props)
{   /* base + combining mark form one cluster: both text units map to glyph 0, the mark glyph does not start a cluster */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0301", FALSE, NULL, 0)); CHECK_CMAP(&s, 0, 0);
    CHECK(s.gp[0].isClusterStart); CHECK(!s.gp[1].isClusterStart); fx_close(&x);
}
TEST(r2_shaping_features_mark_glyph_props_note)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0301", FALSE, NULL, 0)); CHECK(s.actual == 2);
    t_note("acute: isDiacritic %d isZeroWidthSpace %d justification %d; base: isDiacritic %d justification %d", s.gp[1].isDiacritic, s.gp[1].isZeroWidthSpace, s.gp[1].justification, s.gp[0].isDiacritic, s.gp[0].justification);
    CHECK(!s.gp[0].isDiacritic); fx_close(&x);
}
TEST(r2_shaping_features_mark_followed_by_next_base)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0301" L"b", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_ACUTE, G_CALTB == 77 ? 29 : 29); CHECK_ADV(&s, 1000, 0, 1000); CHECK_CMAP(&s, 0, 0, 2);
    CHECK_OFF(&s, 1, -600, 1400); CHECK_OFF(&s, 2, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_mark_two_bases)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0301" L"c\x0302", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_ACUTE, 30, G_CIRC);
    CHECK_OFF(&s, 1, -600, 1400); CHECK_OFF(&s, 3, -600, 1400); fx_close(&x);
}
TEST(r2_shaping_features_mark_disabled)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_mark) };
    CHECK_HR_OK(sh_run(&x, &s, L"a\x0301", FALSE, f, 1)); CHECK_GIDS(&s, 28, G_ACUTE); CHECK_OFF(&s, 1, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_mark_on_uncovered_base)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"1\x0301", FALSE, NULL, 0)); CHECK_GIDS(&s, 55, G_ACUTE); CHECK_OFF(&s, 1, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_mark_on_space_uncovered)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L" \x0301", FALSE, NULL, 0)); CHECK_GIDS(&s, 1, G_ACUTE); CHECK_OFF(&s, 1, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_mark_scales_with_em)
{
    LATN(); static const struct { float em, dx, dy; } k[] = { { 1024, -300, 700 }, { 4096, -1200, 2800 }, { 512, -150, 350 } }; int i;
    CHECK_HR_OK(sh_glyphs(&x, &s, L"a\x0301", 2, FALSE, NULL, NULL, 0));
    for (i = 0; i < 3; i++) { CHECK_HR_OK(sh_place(&x, &s, k[i].em, NULL, 0)); CHECK_OFF(&s, 1, k[i].dx, k[i].dy); }
    fx_close(&x);
}
TEST(r2_shaping_features_mark_offset_includes_base_advance_after_kern)
{   /* the base advance is reduced by kern before the mark: A V kerned, then the mark on V (pen at 900+0) */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AV\x0301", FALSE, NULL, 0)); CHECK_ADV(&s, 900, 1000, 0);
    CHECK_OFF(&s, 2, -600, 1400); fx_close(&x);
}
TEST(r2_shaping_features_mark_on_ligature_not_attached)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"fi\x0301", FALSE, NULL, 0)); CHECK_GIDS(&s, G_FI, G_ACUTE); CHECK_OFF(&s, 1, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_mark_after_calt_glyph)
{   /* calt turns b into glyph 77 which is not in the mark base coverage: no attachment */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ab\x0301", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_CALTB, G_ACUTE); CHECK_OFF(&s, 2, 0, 0); fx_close(&x);
}

/* ---- mark-to-mark (type 6) ---- */
TEST(r2_shaping_features_mkmk_stacked)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0301\x0302", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_ACUTE, G_CIRC); CHECK_ADV(&s, 1000, 0, 0);
    CHECK_OFF(&s, 1, -600, 1400); CHECK_OFF(&s, 2, -600, 2000); fx_close(&x);
}
TEST(r2_shaping_features_mkmk_cluster_map)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0301\x0302", FALSE, NULL, 0)); CHECK_CMAP(&s, 0, 0, 0);
    CHECK(s.gp[0].isClusterStart); CHECK(!s.gp[1].isClusterStart); CHECK(!s.gp[2].isClusterStart); fx_close(&x);
}
TEST(r2_shaping_features_mkmk_disabled_falls_back_to_base)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_mkmk) };
    CHECK_HR_OK(sh_run(&x, &s, L"a\x0301\x0302", FALSE, f, 1)); CHECK_OFF(&s, 1, -600, 1400); CHECK_OFF(&s, 2, -600, 1400); fx_close(&x);
}
TEST(r2_shaping_features_mkmk_reversed_order_not_matched)
{   /* circumflex then acute: mkmk only attaches 79 to 78 (not 78 to 79), so acute goes to the base */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0302\x0301", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_CIRC, G_ACUTE);
    CHECK_OFF(&s, 1, -600, 1400); CHECK_OFF(&s, 2, -600, 1400); fx_close(&x);
}
TEST(r2_shaping_features_mkmk_both_disabled)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_mark), OFF(T_mkmk) };
    CHECK_HR_OK(sh_run(&x, &s, L"a\x0301\x0302", FALSE, f, 2)); CHECK_OFF(&s, 1, 0, 0); CHECK_OFF(&s, 2, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_mkmk_scaled)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"a\x0301\x0302", 3, FALSE, NULL, NULL, 0)); CHECK_HR_OK(sh_place(&x, &s, 1024.0f, NULL, 0));
    CHECK_OFF(&s, 1, -300, 700); CHECK_OFF(&s, 2, -300, 1000); fx_close(&x);
}

/* ---- single adjustment (type 1) ---- */
TEST(r2_shaping_features_cpsp_off_by_default)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AB", FALSE, NULL, 0)); CHECK_ADV(&s, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_cpsp_on_adds_advance)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_cpsp) };
    CHECK_HR_OK(sh_run(&x, &s, L"ABc", FALSE, f, 1)); CHECK_ADV(&s, 1100, 1100, 1000); fx_close(&x);
}
TEST(r2_shaping_features_cpsp_with_kern)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_cpsp) };
    CHECK_HR_OK(sh_run(&x, &s, L"AV", FALSE, f, 1)); CHECK_ADV(&s, 1000, 1100); fx_close(&x);
}
TEST(r2_shaping_features_cpsp_all_capitals)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_cpsp) }; WCHAR t[27]; int i; for (i = 0; i < 26; i++) t[i] = 'A' + i; t[26] = 0;
    CHECK_HR_OK(sh_run(&x, &s, t, FALSE, f, 1)); CHECK(s.actual == 26);
    /* A-V, V-A, T-o, A-Y pairs: kern changes the first glyph, so only compare the letters without a kern partner */
    CHECKF(s.adv[1] == 1100 && s.adv[2] == 1100 && s.adv[5] == 1100 && s.adv[25] == 1100, "B %g C %g F %g Z %g", s.adv[1], s.adv[2], s.adv[5], s.adv[25]);
    fx_close(&x);
}
TEST(r2_shaping_features_ss02_placement_offset)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_ss02) };
    CHECK_HR_OK(sh_run(&x, &s, L"az", FALSE, f, 1)); CHECK_ADV(&s, 1000, 1000); CHECK_OFF(&s, 0, 0, 0); CHECK_OFF(&s, 1, 50, 70); fx_close(&x);
}
TEST(r2_shaping_features_ss02_off_by_default)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"z", FALSE, NULL, 0)); CHECK_OFF(&s, 0, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_ss02_offset_scales)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_ss02) }; featrange fr = { f, 1, 1 };
    CHECK_HR_OK(sh_glyphs(&x, &s, L"z", 1, FALSE, NULL, &fr, 1)); CHECK_HR_OK(sh_place(&x, &s, 1024.0f, &fr, 1)); CHECK_OFF(&s, 0, 25, 35); fx_close(&x);
}
TEST(r2_shaping_features_ss02_with_mark_then_offset_wins_for_base)
{
    LATN(); static const DWRITE_FONT_FEATURE f[] = { ON(T_ss02) };
    CHECK_HR_OK(sh_run(&x, &s, L"z\x0301", FALSE, f, 1)); CHECK(s.actual == 2); CHECK_OFF(&s, 0, 50, 70);
    t_note("mark on a base that has a placement offset: (%g,%g)", s.off[1].advanceOffset, s.off[1].ascenderOffset); fx_close(&x);
}

/* ---- placement result invariants ---- */
TEST(r2_shaping_features_place_zwsp_zero_advance)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x200b" L"b", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, G_ZWSP, 29); CHECK_ADV(&s, 1000, 0, 1000);
    t_note("ZWSP glyph isZeroWidthSpace=%d isDiacritic=%d", s.gp[1].isZeroWidthSpace, s.gp[1].isDiacritic); fx_close(&x);
}
TEST(r2_shaping_features_place_space_advance_and_props_note)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a b", FALSE, NULL, 0)); CHECK_GIDS(&s, 28, 1, 29); CHECK_ADV(&s, 1000, 1000, 1000);
    t_note("justification: a=%d space=%d b=%d", s.gp[0].justification, s.gp[1].justification, s.gp[2].justification); fx_close(&x);
}
TEST(r2_shaping_features_place_default_offsets_zero)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"abcdef", FALSE, NULL, 0)); { int i; for (i = 0; i < 6; i++) CHECK_OFF(&s, i, 0, 0); } fx_close(&x);
}
TEST(r2_shaping_features_place_idempotent)
{
    LATN(); shaped t; CHECK_HR_OK(sh_run(&x, &s, L"AVa\x0301", FALSE, NULL, 0)); CHECK_HR_OK(sh_run(&x, &t, L"AVa\x0301", FALSE, NULL, 0));
    CHECK(s.actual == t.actual && !memcmp(s.adv, t.adv, s.actual * sizeof(float)) && !memcmp(s.off, t.off, s.actual * sizeof(DWRITE_GLYPH_OFFSET))); fx_close(&x);
}
TEST(r2_shaping_features_place_ligature_advance_is_glyph_advance)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ffi", FALSE, NULL, 0)); CHECK_ADV(&s, 2000); fx_close(&x);
}

/* ---- GDI compatible placements ---- */
static HRESULT gdi(fx *x, shaped *s, float em, float ppd, BOOL natural, BOOL rtl)
{
    s->hr_place = IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(x->an, s->text, s->cmap, s->tp, s->len, s->gid, s->gp, s->actual, x->face, em, ppd, NULL,
                                                                      natural, FALSE, rtl, &s->sa, s->loc, NULL, NULL, 0, s->adv, s->off);
    return s->hr_place;
}
TEST(r2_shaping_features_gdi_classic_rounds_to_pixels)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"abc", 3, FALSE, NULL, NULL, 0));
    CHECK_HR_OK(gdi(&x, &s, 16.0f, 1.0f, FALSE, FALSE)); CHECK_ADV(&s, 8, 8, 8);
    CHECK_HR_OK(gdi(&x, &s, 20.0f, 1.0f, FALSE, FALSE)); CHECK_ADV(&s, 10, 10, 10);
    CHECK_HR_OK(gdi(&x, &s, 12.0f, 1.0f, FALSE, FALSE)); CHECK_ADV(&s, 6, 6, 6);
    fx_close(&x);
}
TEST(r2_shaping_features_gdi_pixels_per_dip)
{   /* advances are whole PIXELS at em*pixelsPerDip, reported back in DIPs */
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"abc", 3, FALSE, NULL, NULL, 0));
    CHECK_HR_OK(gdi(&x, &s, 20.0f, 2.0f, FALSE, FALSE)); CHECK_ADV(&s, 10, 10, 10);   /* 40 ppem: 19.53 -> 20 px -> 10 DIPs */
    CHECK_HR_OK(gdi(&x, &s, 10.0f, 3.0f, FALSE, FALSE)); CHECK_ADV(&s, 5, 5, 5);      /* 30 ppem: 14.65 -> 15 px -> 5 DIPs */
    CHECK_HR_OK(gdi(&x, &s, 12.0f, 1.5f, FALSE, FALSE)); CHECK_ADV(&s, 6, 6, 6);      /* 18 ppem: 8.79 -> 9 px -> 6 DIPs */
    fx_close(&x);
}
TEST(r2_shaping_features_gdi_ligature_and_zero_advance)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"fia\x0301", 4, FALSE, NULL, NULL, 0)); CHECK(s.actual == 3);
    CHECK_HR_OK(gdi(&x, &s, 16.0f, 1.0f, FALSE, FALSE));
    CHECKF(s.adv[0] == 12.0f, "fi: %g (1500*16/2048 = 11.72 -> 12)", s.adv[0]); CHECK(s.adv[1] == 8.0f); CHECK(s.adv[2] == 0.0f); fx_close(&x);
}
TEST(r2_shaping_features_gdi_kern_note)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"AV", 2, FALSE, NULL, NULL, 0)); CHECK_HR_OK(gdi(&x, &s, 16.0f, 1.0f, FALSE, FALSE));
    t_note("GDI classic AV at 16: %g %g", s.adv[0], s.adv[1]); CHECK(s.adv[1] == 8.0f); fx_close(&x);
}
TEST(r2_shaping_features_gdi_natural_note)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"abc", 3, FALSE, NULL, NULL, 0)); CHECK_HR_OK(gdi(&x, &s, 16.0f, 1.0f, TRUE, FALSE));
    t_note("GDI natural abc at 16: %g %g %g", s.adv[0], s.adv[1], s.adv[2]); CHECK(s.adv[0] > 7.0f && s.adv[0] < 9.0f); fx_close(&x);
}
TEST(r2_shaping_features_gdi_offsets_for_marks_note)
{
    LATN(); CHECK_HR_OK(sh_glyphs(&x, &s, L"a\x0301", 2, FALSE, NULL, NULL, 0)); CHECK_HR_OK(gdi(&x, &s, 16.0f, 1.0f, FALSE, FALSE));
    t_note("GDI mark offset at 16: (%g,%g) vs linear (%g,%g)", s.off[1].advanceOffset, s.off[1].ascenderOffset, -600 * 16.0 / 2048, 1400 * 16.0 / 2048);
    CHECK(s.adv[1] == 0.0f); fx_close(&x);
}
TEST(r2_shaping_features_gdi_same_glyph_ids_not_modified)
{
    LATN(); UINT16 before[SH_MAX]; CHECK_HR_OK(sh_glyphs(&x, &s, L"fiAV", 4, FALSE, NULL, NULL, 0)); memcpy(before, s.gid, sizeof before);
    CHECK_HR_OK(gdi(&x, &s, 16.0f, 1.0f, FALSE, FALSE)); CHECK(!memcmp(before, s.gid, sizeof before)); fx_close(&x);
}
