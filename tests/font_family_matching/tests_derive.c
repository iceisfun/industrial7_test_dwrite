/* How the weight / stretch / style of a family member is derived from the font file (OS/2 usWeightClass, usWidthClass, fsSelection,
 * head.macStyle).  Windows 10: GetFontWeightFromFace, GetFontStretchFromFace, GetFontStyleFromFace (dwrite.dll).
 *   weight : OS/2 usWeightClass: 0 -> 1, 1..9 -> *100, 10..999 as is, >= 1000 -> 999.  Without OS/2: head.macStyle bold ? 700 : 400.
 *   stretch: OS/2 usWidthClass: 0 -> 5, 1..9 as is, >= 10 -> 9.  Without OS/2: macStyle condensed (0x20) -> 3, extended (0x40) -> 7, else 5.
 *   style  : OS/2 version >= 4 and fsSelection OBLIQUE (0x200) -> OBLIQUE; else fsSelection ITALIC (1) -> ITALIC; else NORMAL.
 *            Without OS/2: macStyle italic (2) -> ITALIC.
 * Names (family subfamily strings) play no part.
 */
#include "fm.h"

static int g_wc, g_wd, g_ver, g_fs, g_mac, g_omit;
static void tw_attrs(ttf_spec *s, int i) {
    (void)i;
    s->weight = g_wc; s->width = g_wd; s->os2_version = g_ver; s->fs_selection = g_fs; s->mac_style = g_mac; s->omit |= g_omit;
}
/* one single-face family; returns the attributes the family's only font reports, or 0 when the family could not be created */
static int one_font(const char *nm, fm_face *out, int *sims) {
    static const fm_face f = { 400, 5, 0 };
    fm m;
    if (!fm_open(&m, nm, &f, 1, tw_attrs)) return 0;
    EXPECT_EQ(m.count, 1);
    if (m.count >= 1) { *out = m.obs[0]; *sims = m.obs_sims[0]; }
    fm_close(&m);
    return m.count >= 1;
}
static void defaults(void) { g_wc = 400; g_wd = 5; g_ver = 4; g_fs = 0x40; g_mac = 0; g_omit = 0; }

static int exp_weight(int wc) { return wc == 0 ? 1 : wc < 10 ? wc * 100 : wc < 1000 ? wc : 999; }
static int exp_stretch(int wd) { return wd == 0 ? 5 : wd < 10 ? wd : 9; }
static int exp_style(int ver, int fs) { return (ver >= 4 && (fs & 0x200)) ? 1 : (fs & 1) ? 2 : 0; }

static void weight_case(int wc) {
    fm_face got; int s; char nm[40];
    defaults(); g_wc = wc;
    snprintf(nm, sizeof nm, "FmWc%d", wc);
    if (!one_font(nm, &got, &s)) return;
    CHECKF(got.w == exp_weight(wc), "usWeightClass %d -> weight %d, expected %d", wc, got.w, exp_weight(wc));
    CHECKF(got.wd == 5 && got.st == 0 && s == 0, "usWeightClass %d disturbed other attributes: wd=%d st=%d sims=%d", wc, got.wd, got.st, s);
}
#define WC_TEST(v) TEST(dv_usweightclass_##v) { weight_case(v); }
WC_TEST(0) WC_TEST(1) WC_TEST(2) WC_TEST(3) WC_TEST(4) WC_TEST(5) WC_TEST(6) WC_TEST(7) WC_TEST(8) WC_TEST(9) WC_TEST(10) WC_TEST(11)
WC_TEST(99) WC_TEST(100) WC_TEST(101) WC_TEST(200) WC_TEST(250) WC_TEST(300) WC_TEST(350) WC_TEST(399) WC_TEST(400) WC_TEST(401) WC_TEST(450)
WC_TEST(500) WC_TEST(550) WC_TEST(551) WC_TEST(600) WC_TEST(700) WC_TEST(800) WC_TEST(900) WC_TEST(950) WC_TEST(999) WC_TEST(1000) WC_TEST(1001)
WC_TEST(2000) WC_TEST(32768) WC_TEST(65535)

static void width_case(int wd) {
    fm_face got; int s; char nm[40];
    defaults(); g_wd = wd;
    snprintf(nm, sizeof nm, "FmWd%d", wd);
    if (!one_font(nm, &got, &s)) return;
    CHECKF(got.wd == exp_stretch(wd), "usWidthClass %d -> stretch %d, expected %d", wd, got.wd, exp_stretch(wd));
    CHECKF(got.w == 400 && got.st == 0, "usWidthClass %d disturbed other attributes: w=%d st=%d", wd, got.w, got.st);
}
#define WD_TEST(v) TEST(dv_uswidthclass_##v) { width_case(v); }
WD_TEST(0) WD_TEST(1) WD_TEST(2) WD_TEST(3) WD_TEST(4) WD_TEST(5) WD_TEST(6) WD_TEST(7) WD_TEST(8) WD_TEST(9) WD_TEST(10) WD_TEST(11) WD_TEST(100) WD_TEST(65535)

static void style_case(int ver, int fs) {
    fm_face got; int s; char nm[48];
    defaults(); g_ver = ver; g_fs = fs;
    snprintf(nm, sizeof nm, "FmSt%d_%x", ver, fs);
    if (!one_font(nm, &got, &s)) return;
    CHECKF(got.st == exp_style(ver, fs), "OS/2 version %d fsSelection 0x%x -> style %d, expected %d", ver, fs, got.st, exp_style(ver, fs));
    CHECKF(got.w == 400 && got.wd == 5, "style case disturbed other attributes");
}
#define STY_TEST(v, fs) TEST(dv_style_os2v##v##_fs_##fs) { style_case(v, 0x##fs); }
STY_TEST(0, 0) STY_TEST(0, 1) STY_TEST(0, 200) STY_TEST(0, 201)
STY_TEST(1, 1) STY_TEST(1, 200) STY_TEST(2, 201) STY_TEST(3, 200) STY_TEST(3, 201) STY_TEST(3, 41)
STY_TEST(4, 0) STY_TEST(4, 1) STY_TEST(4, 40) STY_TEST(4, 41) STY_TEST(4, 200) STY_TEST(4, 201) STY_TEST(4, 240) STY_TEST(4, 241) STY_TEST(4, 300) STY_TEST(4, 301)
STY_TEST(5, 1) STY_TEST(5, 200) STY_TEST(5, 201) STY_TEST(5, 2A0)

TEST(dv_mac_style_ignored_when_os2_present) {
    int mac; fm_face got; int s; char nm[40];
    for (mac = 0; mac < 8; mac++) {
        defaults(); g_mac = mac | 0x20 * (mac >> 2); g_fs = 0x40;
        snprintf(nm, sizeof nm, "FmMacIgn%d", mac);
        if (!one_font(nm, &got, &s)) return;
        /* OS/2 says regular, normal width, upright whatever head.macStyle claims */
        CHECKF(got.w == 400 && got.wd == 5 && got.st == 0, "macStyle 0x%x changed w=%d wd=%d st=%d", g_mac, got.w, got.wd, got.st);
    }
}
TEST(dv_fsselection_bold_bit_does_not_change_weight) {
    fm_face got; int s;
    defaults(); g_fs = 0x20;   /* fsSelection BOLD with usWeightClass 400 */
    if (!one_font("FmFsBold", &got, &s)) return;
    EXPECT_EQ(got.w, 400);
}
TEST(dv_names_do_not_decide_weight) {
    /* the naming block of the synthetic font says "W400S5N / Regular" while usWeightClass is 900 and 100: attributes follow OS/2 */
    fm_face got; int s;
    defaults(); g_wc = 900;
    if (one_font("FmNameW9", &got, &s)) EXPECT_EQ(got.w, 900);
    defaults(); g_wc = 100;
    if (one_font("FmNameW1", &got, &s)) EXPECT_EQ(got.w, 100);
}
TEST(dv_italic_angle_does_not_decide_style) {
    /* post.italicAngle is not consulted; fsSelection decides. */
    fm_face f = { 400, 5, 0 }; fm m;
    defaults();
    if (!fm_open(&m, "FmAngle", &f, 1, tw_attrs)) return;   /* upright style, italicAngle 0 */
    EXPECT_EQ(m.obs[0].st, 0);
    fm_close(&m);
}
TEST(dv_simulations_of_family_members_are_none) {
    fm_face got; int s;
    defaults(); g_wc = 100;
    if (one_font("FmSimNone", &got, &s)) EXPECT_EQ(s, 0);
}

/* ---- fonts without an OS/2 table: head.macStyle ---------------------------------------------------------- */
static void nos2_case(int mac) {
    fm_face got; int s; char nm[40];
    int w = (mac & 1) ? 700 : 400, wd = (mac & 0x20) ? 3 : ((mac & 0x40) | 0xA0) >> 5, st = mac & 2;
    defaults(); g_omit = TTF_OMIT_OS2; g_mac = mac;
    snprintf(nm, sizeof nm, "FmNoOs2_%x", mac);
    fm_set_quiet(1);
    if (!one_font(nm, &got, &s)) { fm_set_quiet(0); t_note("a font without OS/2 (macStyle 0x%x) is not accepted into a custom collection", mac); return; }
    fm_set_quiet(0);
    CHECKF(got.w == w && got.wd == wd && got.st == st, "no OS/2, macStyle 0x%x -> w=%d wd=%d st=%d, expected w=%d wd=%d st=%d", mac, got.w, got.wd, got.st, w, wd, st);
}
#define NO2_TEST(m) TEST(dv_no_os2_macstyle_##m) { nos2_case(0x##m); }
NO2_TEST(0) NO2_TEST(1) NO2_TEST(2) NO2_TEST(3) NO2_TEST(20) NO2_TEST(21) NO2_TEST(22) NO2_TEST(23) NO2_TEST(40) NO2_TEST(41) NO2_TEST(42) NO2_TEST(43)
NO2_TEST(60)

/* ---- derived attributes feed the matcher ------------------------------------------------------------------ */
static void tw_two(ttf_spec *s, int i) {
    /* face 0 claims weight class 0 (reported as 1), face 1 claims 5 (reported as 500) */
    s->weight = i == 0 ? 0 : 5;
}
TEST(dv_derived_weights_are_used_for_matching) {
    fm_face f[2] = { {400,5,0}, {401,5,0} }; fm m;
    if (!fm_open(&m, "FmDerM", f, 2, tw_two)) return;
    if (m.count == 2) {
        fm_expect_first(&m, 1, 5, 0, fm_find(&m, 1, 5, 0), 0);
        fm_expect_first(&m, 500, 5, 0, fm_find(&m, 500, 5, 0), 0);
        fm_expect_first(&m, 400, 5, 0, fm_find(&m, 500, 5, 0), 0);    /* 500 is closer to 400 than 1 is */
        fm_expect_first(&m, 100, 5, 0, fm_find(&m, 1, 5, 0), 0);
    } else CHECKF(0, "family has %u fonts", m.count);
    fm_close(&m);
}
static void tw_wide(ttf_spec *s, int i) { s->width = i == 0 ? 0 : 10; }
TEST(dv_derived_stretch_is_used_for_matching) {
    fm_face f[2] = { {400,5,0}, {401,5,0} }; fm m;
    if (!fm_open(&m, "FmDerS", f, 2, tw_wide)) return;
    if (m.count == 2) {
        /* width class 0 -> normal (5), 10 -> ultra-expanded (9) */
        fm_expect_first(&m, 400, 5, 0, fm_find(&m, 400, 5, 0), 0);
        fm_expect_first(&m, 400, 9, 0, fm_find(&m, 401, 9, 0), 0);
    } else CHECKF(0, "family has %u fonts", m.count);
    fm_close(&m);
}
