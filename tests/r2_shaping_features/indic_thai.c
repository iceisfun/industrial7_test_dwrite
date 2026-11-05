/* Devanagari and Thai shaping on synthetic fonts (see r2.c).  Indic reordering and syllable formation are done by the DirectWrite
 * shaping engine, driven by the font's cmap/GSUB: the pre-base i-matra moves in front of its consonant, 'half' forms replace
 * consonant + virama, 'rphf' forms the reph, 'nukt' composes consonant + nukta.  Cluster maps of Indic syllables are only
 * recorded (t_note); glyph identities and order are asserted. */
#include "r2.h"
#define DEVA() FX_BEGIN(x, FX_DEVA); shaped s
#define THAI() FX_BEGIN(x, FX_THAI); shaped s
#define KA  L"\x0915"
#define SSA L"\x0937"
#define VIR L"\x094d"
#define IM  L"\x093f"
#define AAM L"\x093e"
#define RA  L"\x0930"
#define NUK L"\x093c"

static int script_number(fx *x, const WCHAR *t, UINT32 n)
{
    DWRITE_SCRIPT_PROPERTIES p; DWRITE_SCRIPT_ANALYSIS sa = fx_script(x, t, n);
    if (!x->an1) return -1;
    memset(&p, 0, sizeof p);
    if (FAILED(IDWriteTextAnalyzer1_GetScriptProperties(x->an1, sa, &p))) return -2;
    return (int)p.isoScriptNumber;
}
TEST(r2_shaping_features_deva_script_number) { DEVA(); int n = script_number(&x, KA, 1); if (n == -1) t_skip("IDWriteTextAnalyzer1 unsupported"); else EXPECT_EQ(n, 315); fx_close(&x); }
TEST(r2_shaping_features_deva_single_consonant) { DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA, FALSE, NULL, 0)); CHECK_GIDS(&s, D_KA); CHECK_CMAP(&s, 0); CHECK_ADV(&s, 900); fx_close(&x); }
TEST(r2_shaping_features_deva_two_consonants) { DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA SSA, FALSE, NULL, 0)); CHECK_GIDS(&s, D_KA, D_SSA); CHECK_CMAP(&s, 0, 1); CHECK_ADV(&s, 900, 900); fx_close(&x); }
TEST(r2_shaping_features_deva_post_base_matra)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA AAM, FALSE, NULL, 0)); CHECK_GIDS(&s, D_KA, D_AA);
    t_note("ka+aa-matra cmap %u %u, isClusterStart %d %d", s.cmap[0], s.cmap[1], s.gp[0].isClusterStart, s.gp[1].isClusterStart); fx_close(&x);
}
TEST(r2_shaping_features_deva_pre_base_matra_reordered)
{   /* U+093F is displayed to the left of its consonant: glyph order i-matra, ka */
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA IM, FALSE, NULL, 0)); CHECK_GIDS(&s, D_I, D_KA); CHECK_ADV(&s, 400, 900);
    t_note("ka+i-matra clusterMap %u %u", s.cmap[0], s.cmap[1]); CHECK(s.cmap[0] <= 1 && s.cmap[1] <= 1); fx_close(&x);
}
TEST(r2_shaping_features_deva_pre_base_matra_cluster_start)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA IM, FALSE, NULL, 0)); CHECK(s.actual == 2); CHECK(s.gp[0].isClusterStart);
    t_note("second glyph isClusterStart %d", s.gp[1].isClusterStart); fx_close(&x);
}
TEST(r2_shaping_features_deva_matra_reorder_only_within_syllable)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA IM SSA, FALSE, NULL, 0)); CHECK_GIDS(&s, D_I, D_KA, D_SSA); fx_close(&x);
}
TEST(r2_shaping_features_deva_two_syllables_with_matra)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA IM SSA IM, FALSE, NULL, 0)); CHECK_GIDS(&s, D_I, D_KA, D_I, D_SSA); fx_close(&x);
}
TEST(r2_shaping_features_deva_half_form)
{   /* 'half' ligature ka+virama -> half ka; the virama is consumed */
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA VIR SSA, FALSE, NULL, 0)); CHECK_GIDS(&s, D_HALF_KA, D_SSA); CHECK_ADV(&s, 600, 900);
    t_note("ka+virama+ssa clusterMap %u %u %u", s.cmap[0], s.cmap[1], s.cmap[2]); fx_close(&x);
}
TEST(r2_shaping_features_deva_half_form_cluster_map_ligature_part)
{   /* the two characters folded into the half form map to the same glyph */
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA VIR SSA, FALSE, NULL, 0)); CHECK(s.cmap[0] == s.cmap[1]); CHECK(s.cmap[2] != s.cmap[0]); fx_close(&x);
}
TEST(r2_shaping_features_deva_trailing_virama_no_half)
{   /* a final virama (no following consonant): half form is not formed */
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA VIR, FALSE, NULL, 0)); CHECK(s.actual >= 1);
    t_note("ka+virama alone: %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_deva_half_form_with_matra)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA VIR SSA IM, FALSE, NULL, 0));
    CHECK(s.actual == 3); CHECK(sh_has(&s, D_I)); CHECK(sh_has(&s, D_HALF_KA)); CHECK(sh_has(&s, D_SSA));
    t_note("ka+virama+ssa+i: %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_deva_reph_formed)
{   /* ra + virama + ka: 'rphf' forms the reph glyph, which is moved after the base consonant */
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, RA VIR KA, FALSE, NULL, 0)); CHECK(s.actual == 2); CHECK(sh_has(&s, D_REPH)); CHECK(sh_has(&s, D_KA));
    t_note("ra+virama+ka: %s cmap %u %u %u", sh_dump(&s), s.cmap[0], s.cmap[1], s.cmap[2]); fx_close(&x);
}
TEST(r2_shaping_features_deva_reph_position_note)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, RA VIR KA, FALSE, NULL, 0));
    t_note("reph glyph index %d (0 = before base, 1 = after base)", sh_index_of(&s, D_REPH)); fx_close(&x);
}
TEST(r2_shaping_features_deva_ra_without_virama_is_plain)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, RA KA, FALSE, NULL, 0)); CHECK_GIDS(&s, D_RA, D_KA); fx_close(&x);
}
TEST(r2_shaping_features_deva_nukta_composition)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA NUK, FALSE, NULL, 0)); CHECK_GIDS(&s, D_KA_NUKTA); CHECK_CMAP(&s, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_deva_nukta_then_matra)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA NUK IM, FALSE, NULL, 0)); CHECK_GIDS(&s, D_I, D_KA_NUKTA); fx_close(&x);
}
TEST(r2_shaping_features_deva_half_disabled_note)
{
    DEVA(); static const DWRITE_FONT_FEATURE f[] = { { (DWRITE_FONT_FEATURE_TAG)T_half, 0 } };
    CHECK_HR_OK(sh_run(&x, &s, KA VIR SSA, FALSE, f, 1)); CHECK(s.actual >= 2); t_note("half=0: %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_deva_advances_no_gpos)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA SSA, FALSE, NULL, 0)); CHECK_OFF(&s, 0, 0, 0); CHECK_OFF(&s, 1, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_deva_space_between_syllables)
{
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, KA IM L" " KA, FALSE, NULL, 0)); CHECK_GIDS(&s, D_I, D_KA, 1, D_KA); fx_close(&x);
}
TEST(r2_shaping_features_deva_virama_glyph_without_ligature)
{   /* virama before a non-consonant stays a glyph */
    DEVA(); CHECK_HR_OK(sh_run(&x, &s, SSA VIR L" ", FALSE, NULL, 0)); CHECK(s.actual >= 2); t_note("ssa virama space: %s", sh_dump(&s)); fx_close(&x);
}

/* ---- Thai ---- */
TEST(r2_shaping_features_thai_script_number) { THAI(); int n = script_number(&x, L"\x0e01", 1); if (n == -1) t_skip("IDWriteTextAnalyzer1 unsupported"); else EXPECT_EQ(n, 352); fx_close(&x); }
TEST(r2_shaping_features_thai_single) { THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01", FALSE, NULL, 0)); CHECK_GIDS(&s, TH_KO); CHECK_ADV(&s, 900); fx_close(&x); }
TEST(r2_shaping_features_thai_base_plus_tone_mark)
{   /* base + non-spacing tone mark: one cluster, the mark has advance 0 */
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e48", FALSE, NULL, 0)); CHECK_GIDS(&s, TH_KO, TH_MAIEK); CHECK_CMAP(&s, 0, 0); CHECK_ADV(&s, 900, 0);
    CHECK(s.gp[0].isClusterStart); CHECK(!s.gp[1].isClusterStart); fx_close(&x);
}
TEST(r2_shaping_features_thai_vowel_and_tone)
{
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e34\x0e48", FALSE, NULL, 0)); CHECK_GIDS(&s, TH_KO, TH_SARAI, TH_MAIEK); CHECK_CMAP(&s, 0, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_thai_below_vowel)
{
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e38", FALSE, NULL, 0)); CHECK_GIDS(&s, TH_KO, TH_SARAU); CHECK_CMAP(&s, 0, 0); CHECK_ADV(&s, 900, 0); fx_close(&x);
}
TEST(r2_shaping_features_thai_leading_vowel_stays_logical)
{   /* the leading vowel is stored before the consonant in Unicode: no reordering */
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e40\x0e01", FALSE, NULL, 0)); CHECK_GIDS(&s, TH_SARAE, TH_KO); CHECK_CMAP(&s, 0, 1); fx_close(&x);
}
TEST(r2_shaping_features_thai_spacing_vowel_own_cluster)
{
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e32", FALSE, NULL, 0)); CHECK_GIDS(&s, TH_KO, TH_SARAA); CHECK_CMAP(&s, 0, 1); CHECK_ADV(&s, 900, 500); fx_close(&x);
}
TEST(r2_shaping_features_thai_two_clusters)
{
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e48\x0e01\x0e49", FALSE, NULL, 0)); CHECK_GIDS(&s, TH_KO, TH_MAIEK, TH_KO, TH_MAITHO); CHECK_CMAP(&s, 0, 0, 2, 2); fx_close(&x);
}
TEST(r2_shaping_features_thai_sara_am_with_glyph_note)
{
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e33", FALSE, NULL, 0)); CHECK(s.actual >= 2); CHECK(s.gid[0] == TH_KO);
    t_note("ko + sara am (font maps U+0E33): %s cmap %u %u", sh_dump(&s), s.cmap[0], s.cmap[1]); fx_close(&x);
}
TEST(r2_shaping_features_thai_sara_am_without_glyph_note)
{
    FX_BEGIN(x, FX_THAI_NOAM); shaped s;
    CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e33", FALSE, NULL, 0)); CHECK(s.actual >= 2);
    t_note("ko + sara am (no U+0E33 glyph): %s", sh_dump(&s)); fx_close(&x);
}
TEST(r2_shaping_features_thai_no_offsets_without_gpos)
{
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e48", FALSE, NULL, 0)); CHECK_OFF(&s, 0, 0, 0); CHECK_OFF(&s, 1, 0, 0); fx_close(&x);
}
TEST(r2_shaping_features_thai_glyph_props_note)
{
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e48", FALSE, NULL, 0));
    t_note("tone mark: isDiacritic %d isZeroWidthSpace %d justification %d", s.gp[1].isDiacritic, s.gp[1].isZeroWidthSpace, s.gp[1].justification); fx_close(&x);
}
TEST(r2_shaping_features_thai_rtl_flag_does_not_reorder_clusters_internally)
{
    THAI(); CHECK_HR_OK(sh_run(&x, &s, L"\x0e01\x0e48", TRUE, NULL, 0)); CHECK_FORMS(&s, 2, TH_KO, TH_KO); fx_close(&x);
}
