/* Direction handling of GetGlyphs/GetGlyphPlacements with isRightToLeft set.  The glyph array of an RTL run may be logical or
 * visual (reversed) order; the tests assert only direction independent properties and record the observed order. */
#include "r2.h"
#define LATN() FX_BEGIN(x, FX_LATN); shaped s

TEST(r2_shaping_features_rtl_latin_order_note)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ABC", TRUE, NULL, 0)); CHECK(s.actual == 3); CHECK(sh_order(&s) >= 0);
    t_note("rtl ABC glyphs %s cmap %u %u %u (order %d: 0 logical, 1 reversed)", sh_dump(&s), s.cmap[0], s.cmap[1], s.cmap[2], sh_order(&s));
    CHECK_FORMS(&s, 3, 2, 3, 4); fx_close(&x);
}
TEST(r2_shaping_features_rtl_ltr_flag_is_identity_order)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"ABC", FALSE, NULL, 0)); CHECK_GIDS(&s, 2, 3, 4); CHECK_CMAP(&s, 0, 1, 2); fx_close(&x);
}
TEST(r2_shaping_features_rtl_liga_forms)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"fi", TRUE, NULL, 0)); CHECK_FORMS(&s, 1, G_FI, G_FI); fx_close(&x);
}
TEST(r2_shaping_features_rtl_liga_in_word)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"afib", TRUE, NULL, 0)); CHECK_FORMS(&s, 3, 28, G_FI, G_FI, 29); fx_close(&x);
}
TEST(r2_shaping_features_rtl_kern_pair_note)
{   /* a GPOS pair adjusts the first glyph in LOGICAL order; the array position of that glyph depends on the output order */
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"AV", TRUE, NULL, 0)); CHECK_FORMS(&s, 2, 2, 23);
    t_note("rtl AV: advances by array index %g %g, glyphs %s", s.adv[0], s.adv[1], sh_dump(&s));
    CHECK(s.adv[0] + s.adv[1] == 1900.0f); fx_close(&x);
}
TEST(r2_shaping_features_rtl_kern_total_width_same_as_ltr)
{
    LATN(); shaped t; CHECK_HR_OK(sh_run(&x, &s, L"AVAo", TRUE, NULL, 0)); CHECK_HR_OK(sh_run(&x, &t, L"AVAo", FALSE, NULL, 0));
    float a = 0, b = 0; UINT32 i; for (i = 0; i < s.actual; i++) a += s.adv[i]; for (i = 0; i < t.actual; i++) b += t.adv[i];
    CHECKF(a == b, "rtl width %g ltr width %g", a, b); fx_close(&x);
}
TEST(r2_shaping_features_rtl_calt_forms)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"cab", TRUE, NULL, 0));
    t_note("rtl 'cab' (calt a|b): %s", sh_dump(&s)); CHECK(s.actual == 3); fx_close(&x);
}
TEST(r2_shaping_features_rtl_mark_offset_note)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"a\x0301", TRUE, NULL, 0)); CHECK(s.actual == 2);
    int m = sh_index_of(&s, G_ACUTE); CHECK(m >= 0);
    if (m >= 0) { CHECKF(s.off[m].ascenderOffset == 1400.0f, "ascender offset %g", s.off[m].ascenderOffset); t_note("rtl mark index %d advanceOffset %g", m, s.off[m].advanceOffset); }
    fx_close(&x);
}
TEST(r2_shaping_features_rtl_cluster_map_covers_every_unit)
{
    LATN(); CHECK_HR_OK(sh_run(&x, &s, L"abcdef", TRUE, NULL, 0)); CHECK(s.actual == 6);
    UINT32 seen = 0, i; for (i = 0; i < 6; i++) { CHECKF(s.cmap[i] < 6, "cmap[%u] = %u", i, s.cmap[i]); if (s.cmap[i] < 6) seen |= 1u << s.cmap[i]; }
    CHECK(seen == 0x3f); fx_close(&x);
}
