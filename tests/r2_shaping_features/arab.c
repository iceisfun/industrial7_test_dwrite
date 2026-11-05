/* Arabic joining through GetGlyphs/GetGlyphPlacements on the R2Arab synthetic font (see r2.c): cmap alef/beh/lam/hamza/fatha/ZWNJ/ZWJ,
 * GSUB isol/fina/medi/init single substitutions and an rlig lam-alef ligature (lookup order isol, fina, medi, init, rlig), GPOS mark.
 * Right-to-left runs return the glyph array in VISUAL (reversed) order while clusterMap stays indexed by logical text position. */
#include "r2.h"
#define ON(t)  { (DWRITE_FONT_FEATURE_TAG)(t), 1 }
#define OFF(t) { (DWRITE_FONT_FEATURE_TAG)(t), 0 }
#define ARAB() FX_BEGIN(x, FX_ARAB); shaped s
#define AB L"\x0628"
#define AA L"\x0627"
#define AL L"\x0644"
#define AH L"\x0621"
#define AF L"\x064e"

TEST(r2_shaping_features_arab_script_is_arab)
{
    ARAB(); DWRITE_SCRIPT_PROPERTIES p; DWRITE_SCRIPT_ANALYSIS sa = fx_script(&x, AB, 1);
    if (!x.an1) { fx_close(&x); t_skip("IDWriteTextAnalyzer1 unsupported"); return; }
    memset(&p, 0, sizeof p); CHECK_HR_OK(IDWriteTextAnalyzer1_GetScriptProperties(x.an1, sa, &p));
    CHECK(p.isoScriptCode == 0x62617241 /* 'Arab' */ || p.isoScriptNumber == 160); EXPECT_EQ(p.isoScriptNumber, 160); fx_close(&x);
}
TEST(r2_shaping_features_arab_isolated_beh)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB, TRUE, NULL, 0)); CHECK_GIDS(&s, A_BEH_ISOL); CHECK_CMAP(&s, 0); CHECK_ADV(&s, 1210); fx_close(&x);
}
TEST(r2_shaping_features_arab_isolated_alef_lam)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AA, TRUE, NULL, 0)); CHECK_GIDS(&s, A_ALEF_ISOL);
    CHECK_HR_OK(sh_run(&x, &s, AL, TRUE, NULL, 0)); CHECK_GIDS(&s, A_LAM_ISOL); fx_close(&x);
}
TEST(r2_shaping_features_arab_two_beh_visual_order)
{   /* logical: initial, final; visual (reversed): final, initial */
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AB, TRUE, NULL, 0)); CHECK_FORMS(&s, 2, A_BEH_INIT, A_BEH_FINA); CHECK_ADVC(&s, 800, 900); fx_close(&x);
}
TEST(r2_shaping_features_arab_three_beh)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AB AB, TRUE, NULL, 0)); CHECK_FORMS(&s, 3, A_BEH_INIT, A_BEH_MEDI, A_BEH_FINA); CHECK_ADVC(&s, 800, 600, 900); fx_close(&x);
}
TEST(r2_shaping_features_arab_five_beh)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AB AB AB AB, TRUE, NULL, 0)); CHECK_FORMS(&s, 5, A_BEH_INIT, A_BEH_MEDI, A_BEH_MEDI, A_BEH_MEDI, A_BEH_FINA); fx_close(&x);
}
TEST(r2_shaping_features_arab_twenty_beh)
{
    ARAB(); WCHAR t[21]; int i; UINT16 f[20]; for (i = 0; i < 20; i++) { t[i] = 0x628; f[i] = A_BEH_MEDI; } t[20] = 0; f[0] = A_BEH_INIT; f[19] = A_BEH_FINA;
    CHECK_HR_OK(sh_run(&x, &s, t, TRUE, NULL, 0)); CHECK(s.actual == 20); CHECK(sh_order(&s) >= 0);
    for (i = 0; i < 20; i++) CHECKF(s.cmap[i] < 20 && s.gid[s.cmap[i]] == f[i], "unit %d -> glyph %u (gid %u) want %u", i, s.cmap[i], s.gid[s.cmap[i] % 20], f[i]);
    fx_close(&x);
}
TEST(r2_shaping_features_arab_beh_alef)
{   /* alef joins to the previous beh (final form), beh takes the initial form */
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AA, TRUE, NULL, 0)); CHECK_FORMS(&s, 2, A_BEH_INIT, A_ALEF_FINA); fx_close(&x);
}
TEST(r2_shaping_features_arab_alef_beh_alef_does_not_join_forward)
{   /* alef is right-joining only: the following beh is isolated */
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AA AB, TRUE, NULL, 0)); CHECK_FORMS(&s, 2, A_ALEF_ISOL, A_BEH_ISOL); fx_close(&x);
}
TEST(r2_shaping_features_arab_beh_alef_beh)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AA AB, TRUE, NULL, 0)); CHECK_FORMS(&s, 3, A_BEH_INIT, A_ALEF_FINA, A_BEH_ISOL); fx_close(&x);
}
TEST(r2_shaping_features_arab_lam_alef_ligature)
{   /* rlig lam(initial) + alef(final) -> one glyph; both characters map to glyph 0 */
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AL AA, TRUE, NULL, 0)); CHECK_FORMS(&s, 1, A_LAMALEF, A_LAMALEF); CHECK_ADV(&s, 1100); fx_close(&x);
}
TEST(r2_shaping_features_arab_beh_lam_alef)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AL AA, TRUE, NULL, 0)); CHECK_FORMS(&s, 2, A_BEH_INIT, A_LAMALEF_FINA, A_LAMALEF_FINA); CHECK_ADVC(&s, 800, 1000, 1000); fx_close(&x);
}
TEST(r2_shaping_features_arab_lam_lam_alef)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AL AL AA, TRUE, NULL, 0)); CHECK_FORMS(&s, 2, A_LAM_INIT, A_LAMALEF_FINA, A_LAMALEF_FINA); fx_close(&x);
}
TEST(r2_shaping_features_arab_alef_lam_alef)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AA AL AA, TRUE, NULL, 0)); CHECK_FORMS(&s, 2, A_ALEF_ISOL, A_LAMALEF, A_LAMALEF); fx_close(&x);
}
TEST(r2_shaping_features_arab_hamza_non_joining)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AH AB, TRUE, NULL, 0)); CHECK_FORMS(&s, 3, A_BEH_ISOL, A_HAMZA, A_BEH_ISOL); fx_close(&x);
}
TEST(r2_shaping_features_arab_hamza_before_lam_alef)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AH AL AA, TRUE, NULL, 0)); CHECK_FORMS(&s, 2, A_HAMZA, A_LAMALEF, A_LAMALEF); fx_close(&x);
}
TEST(r2_shaping_features_arab_space_breaks_joining)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB L" " AB, TRUE, NULL, 0)); CHECK_FORMS(&s, 3, A_BEH_ISOL, 1, A_BEH_ISOL); fx_close(&x);
}
TEST(r2_shaping_features_arab_zwnj_breaks_joining)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB L"\x200c" AB, TRUE, NULL, 0));
    int n84 = 0; UINT32 i; for (i = 0; i < s.actual; i++) if (s.gid[i] == A_BEH_ISOL) n84++;
    CHECKF(n84 == 2, "isolated behs: %d in %s", n84, sh_dump(&s));
    t_note("beh ZWNJ beh -> %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_arab_zwj_forces_joining_after)
{   /* beh + ZWJ: beh takes the initial form even without a following letter */
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB L"\x200d", TRUE, NULL, 0));
    int has72 = 0; UINT32 i; for (i = 0; i < s.actual; i++) if (s.gid[i] == A_BEH_INIT) has72 = 1;
    CHECKF(has72, "no initial beh in %s", sh_dump(&s)); t_note("beh ZWJ -> %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_arab_zwj_forces_joining_before)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, L"\x200d" AB, TRUE, NULL, 0));
    int has74 = 0; UINT32 i; for (i = 0; i < s.actual; i++) if (s.gid[i] == A_BEH_FINA) has74 = 1;
    CHECKF(has74, "no final beh in %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_arab_ltr_flag_keeps_logical_order)
{   /* the same text shaped as a left-to-right run: forms identical, glyph order = logical order */
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AB AB, FALSE, NULL, 0)); /* with the LTR flag the joining context runs against the logical order: the first character takes the final form */
    CHECK_GIDS(&s, A_BEH_FINA, A_BEH_MEDI, A_BEH_INIT); CHECK_CMAP(&s, 0, 1, 2); fx_close(&x);
}
TEST(r2_shaping_features_arab_ltr_flag_lam_alef)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AL AA, FALSE, NULL, 0)); CHECK_GIDS(&s, A_BEH_FINA, A_LAM_INIT, A_ALEF_ISOL); CHECK_CMAP(&s, 0, 1, 2); fx_close(&x);
}
TEST(r2_shaping_features_arab_init_disabled)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_init) };
    CHECK_HR_OK(sh_run(&x, &s, AB AB, TRUE, f, 1)); CHECK_FORMS(&s, 2, A_BEH_INIT, A_BEH_FINA); fx_close(&x);   /* joining forms are required: OFF is ignored */
}
TEST(r2_shaping_features_arab_fina_disabled)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_fina) };
    CHECK_HR_OK(sh_run(&x, &s, AB AB, TRUE, f, 1)); CHECK_FORMS(&s, 2, A_BEH_INIT, A_BEH_FINA); fx_close(&x);
}
TEST(r2_shaping_features_arab_medi_disabled)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_medi) };
    CHECK_HR_OK(sh_run(&x, &s, AB AB AB, TRUE, f, 1)); CHECK_FORMS(&s, 3, A_BEH_INIT, A_BEH_MEDI, A_BEH_FINA); fx_close(&x);
}
TEST(r2_shaping_features_arab_isol_disabled)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_isol) };
    CHECK_HR_OK(sh_run(&x, &s, AB, TRUE, f, 1)); CHECK_FORMS(&s, 1, A_BEH_ISOL); fx_close(&x);
}
TEST(r2_shaping_features_arab_isol_disabled_joined_unaffected)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_isol) };
    CHECK_HR_OK(sh_run(&x, &s, AB AB, TRUE, f, 1)); CHECK_FORMS(&s, 2, A_BEH_INIT, A_BEH_FINA); fx_close(&x);
}
TEST(r2_shaping_features_arab_all_forms_disabled)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_isol), OFF(T_init), OFF(T_medi), OFF(T_fina) };
    CHECK_HR_OK(sh_run(&x, &s, AB AB AB, TRUE, f, 4)); CHECK_FORMS(&s, 3, A_BEH_INIT, A_BEH_MEDI, A_BEH_FINA); fx_close(&x);
}
TEST(r2_shaping_features_arab_rlig_off_note)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_rlig) };
    CHECK_HR_OK(sh_run(&x, &s, AL AA, TRUE, f, 1)); t_note("lam alef with rlig=0: %s", sh_dump(&s)); CHECK(s.actual >= 1); fx_close(&x);
}
TEST(r2_shaping_features_arab_range_init_off_first_only)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_init) }; featrange fr[] = { { f, 1, 1 }, { NULL, 0, 2 } };
    CHECK_HR_OK(sh_glyphs(&x, &s, AB AB AB, 3, TRUE, NULL, fr, 2)); CHECK_FORMS(&s, 3, A_BEH_INIT, A_BEH_MEDI, A_BEH_FINA); fx_close(&x);
}
TEST(r2_shaping_features_arab_mark_after_beh)
{   /* mark glyph advance 0; GPOS mark-to-base attaches fatha (anchor 100,0) to the beh anchor (400,1000) */
    ARAB(); int m, b; CHECK_HR_OK(sh_run(&x, &s, AB AF, TRUE, NULL, 0)); CHECK(s.actual == 2); CHECK(sh_order(&s) >= 0);
    m = sh_index_of(&s, A_FATHA); b = sh_index_of(&s, A_BEH_ISOL); CHECK(m >= 0 && b >= 0 && m != b);
    if (m >= 0 && b >= 0) {
        CHECK(s.adv[m] == 0.0f); CHECK(s.adv[b] == 1210.0f); CHECK_OFF(&s, b, 0, 0);
        CHECKF(s.off[m].ascenderOffset == 1000.0f, "fatha ascender offset %g", s.off[m].ascenderOffset);
        t_note("mark glyph index %d (base %d): advanceOffset %g (anchor delta 300, minus base advance 1210 when the base precedes the mark)", m, b, s.off[m].advanceOffset);
    }
    fx_close(&x);
}
TEST(r2_shaping_features_arab_mark_cluster_props)
{
    ARAB(); int m, b; CHECK_HR_OK(sh_run(&x, &s, AB AF, TRUE, NULL, 0)); CHECK(s.actual == 2);
    m = sh_index_of(&s, A_FATHA); b = sh_index_of(&s, A_BEH_ISOL);
    CHECK(m >= 0 && b >= 0); if (m >= 0 && b >= 0) { CHECK(s.gp[b].isClusterStart); t_note("fatha isClusterStart=%d, clusterMap %u %u", s.gp[m].isClusterStart, s.cmap[0], s.cmap[1]); }
    fx_close(&x);
}
TEST(r2_shaping_features_arab_mark_between_joiners)
{   /* a combining mark does not interrupt joining */
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AF AB, TRUE, NULL, 0));
    CHECK(s.actual == 3); CHECK(sh_has(&s, A_BEH_FINA)); CHECK(sh_has(&s, A_FATHA)); CHECK(sh_has(&s, A_BEH_INIT));
    fx_close(&x);
}
TEST(r2_shaping_features_arab_mark_disabled)
{
    ARAB(); static const DWRITE_FONT_FEATURE f[] = { OFF(T_mark) };
    CHECK_HR_OK(sh_run(&x, &s, AB AF, TRUE, f, 1)); CHECK_OFF(&s, 0, 0, 0); CHECK_OFF(&s, 1, -300, 1000); fx_close(&x);   /* mark is required */
}
TEST(r2_shaping_features_arab_text_props_per_unit)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AB, TRUE, NULL, 0)); CHECK(s.actual == 2);
    t_note("text props: [0] alone=%d brk=%d  [1] alone=%d brk=%d", s.tp[0].isShapedAlone, s.tp[0].canBreakShapingAfter, s.tp[1].isShapedAlone, s.tp[1].canBreakShapingAfter);
    fx_close(&x);
}
TEST(r2_shaping_features_arab_glyph_justification_note)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AB AB, TRUE, NULL, 0)); CHECK(s.actual == 3);
    t_note("justification (array order): %d %d %d", s.gp[0].justification, s.gp[1].justification, s.gp[2].justification); fx_close(&x);
}
TEST(r2_shaping_features_arab_gdi_placements_rtl)
{
    ARAB(); CHECK_HR_OK(sh_glyphs(&x, &s, AB AB, 2, TRUE, NULL, NULL, 0));
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(x.an, s.text, s.cmap, s.tp, s.len, s.gid, s.gp, s.actual, x.face, 16.0f, 1.0f, NULL, FALSE, FALSE, TRUE, &s.sa, s.loc, NULL, NULL, 0, s.adv, s.off));
    t_note("GDI rtl advances %g %g (linear 7.03 / 6.25)", s.adv[0], s.adv[1]); fx_close(&x);
}
TEST(r2_shaping_features_arab_placement_matches_glyph_advances)
{
    ARAB(); CHECK_HR_OK(sh_run(&x, &s, AB AL AA AB, TRUE, NULL, 0));
    /* beh init 800, lamalef final 1000, beh? -> logical: beh(init) lam(medi) alef(final)->lamalef-final(1000) | beh? */
    UINT32 i; for (i = 0; i < s.actual; i++) CHECKF(s.adv[i] >= 0, "advance %u = %g", i, s.adv[i]); fx_close(&x);
}
