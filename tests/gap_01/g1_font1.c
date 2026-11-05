/* IDWriteFont1::GetPanose, IDWriteFont1::IsMonospacedFont and IDWriteFont2::IsColorFont, with their IDWriteFontFace1/2/3 twins.
 *
 * Win10 dwrite.dll: DWriteFont::GetPanose (!0x928a0) copies the 10 panose bytes (an 8 byte and a 2 byte store, nothing else is written) from the
 * cached face data, DWriteFontFace::GetPanose (!0x92520) does the same from the face's own cache; DWriteFont::IsMonospacedFont (!0xe3150) is bit 1 and
 * DWriteFont::IsColorFont (!0xe3130) is bit 7 of the 16 bit flags word of the same cached data that DWriteFontFace::IsMonospacedFont (!0x8f720) and
 * DWriteFontFace::IsColorFont (!0x208b0) read. The two vtables of one font therefore always agree, the answers are exactly TRUE or FALSE (0 / 1) and
 * never fail. The panose bytes are the 10 bytes at offset 32 of the OS/2 table.
 */
#include "g1.h"

typedef struct sysf { IDWriteFont *font; IDWriteFontFace *face; IDWriteFont1 *f1; IDWriteFont2 *f2; IDWriteFontFace1 *ff1; IDWriteFontFace2 *ff2; IDWriteFontFace3 *ff3; } sysf;
static void sysf_open(sysf *s, IDWriteFont *font)
{
    memset(s, 0, sizeof *s);
    s->font = font; IDWriteFont_AddRef(font);
    IDWriteFont_CreateFontFace(font, &s->face);
    s->f1 = g1_qi((IUnknown *)font, &IID_IDWriteFont1); s->f2 = g1_qi((IUnknown *)font, &IID_IDWriteFont2);
    if (s->face) { s->ff1 = g1_qi((IUnknown *)s->face, &IID_IDWriteFontFace1); s->ff2 = g1_qi((IUnknown *)s->face, &IID_IDWriteFontFace2); s->ff3 = g1_qi((IUnknown *)s->face, &IID_IDWriteFontFace3); }
}
static void sysf_close(sysf *s) { DW_RELEASE(s->f1); DW_RELEASE(s->f2); DW_RELEASE(s->ff1); DW_RELEASE(s->ff2); DW_RELEASE(s->ff3); DW_RELEASE(s->face); DW_RELEASE(s->font); }

/* Cross-checks one font; reports its flags. */
static void cross_check(const char *label, IDWriteFont *font, int *mono, int *color)
{
    sysf s; DWRITE_PANOSE a, b; uint8_t *os2; UINT32 len = 0;
    sysf_open(&s, font);
    *mono = -1; *color = -1;
    if (s.f1) {
        int m = IDWriteFont1_IsMonospacedFont(s.f1);
        CHECKF(m == 0 || m == 1, "%s: IsMonospacedFont returned %d", label, m);
        *mono = m;
        if (s.ff1) CHECKF(IDWriteFontFace1_IsMonospacedFont(s.ff1) == m, "%s: font and face disagree on monospace", label);
        memset(&a, 0xAB, sizeof a);
        IDWriteFont1_GetPanose(s.f1, &a);
        if (s.ff3) {
            memset(&b, 0xCD, sizeof b);
            IDWriteFontFace3_GetPanose(s.ff3, &b);
            CHECKF(!memcmp(&a, &b, 10), "%s: font and face disagree on the panose", label);
        }
        if (s.face && (os2 = dw_face_table(s.face, DW_TAG('O', 'S', '/', '2'), &len)) != NULL) {
            if (len >= 42) CHECKF(!memcmp(&a, os2 + 32, 10), "%s: panose differs from the OS/2 bytes", label);
            free(os2);
        }
    }
    if (s.f2) {
        int c = IDWriteFont2_IsColorFont(s.f2);
        CHECKF(c == 0 || c == 1, "%s: IsColorFont returned %d", label, c);
        *color = c;
        if (s.ff2) CHECKF(IDWriteFontFace2_IsColorFont(s.ff2) == c, "%s: font and face disagree on colour", label);
    }
    sysf_close(&s);
}
/* All fonts of a system family; returns count (0 when absent). */
static UINT32 each_font(const WCHAR *family, int *any_mono, int *all_mono, int *any_color, int *all_color)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; UINT32 idx = 0, i, n = 0; BOOL ex = FALSE;
    *any_mono = *any_color = 0; *all_mono = *all_color = 1;
    if (!f || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return 0;
    IDWriteFontCollection_FindFamilyName(c, family, &idx, &ex);
    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam) {
        n = IDWriteFontFamily_GetFontCount(fam);
        for (i = 0; i < n && i < 40; i++) {
            IDWriteFont *font = NULL; char label[96]; int m, col;
            if (FAILED(IDWriteFontFamily_GetFont(fam, i, &font)) || !font) continue;
            snprintf(label, sizeof label, "%ls #%u", family, i);
            cross_check(label, font, &m, &col);
            if (m == 1) *any_mono = 1; else *all_mono = 0;
            if (col == 1) *any_color = 1; else *all_color = 0;
            IDWriteFont_Release(font);
        }
    }
    DW_RELEASE(fam); DW_RELEASE(c);
    return n;
}
#define FAMILY_TEST(name, fam, mono_expect, color_expect) \
    TEST(name) { int am, lm, ac, lc; UINT32 n = each_font(fam, &am, &lm, &ac, &lc); \
        if (!n) { t_skip("%ls is not installed", fam); return; } \
        if ((mono_expect) == 1) { CHECKF(lm == 1, "%ls: every font should be monospaced", fam); } \
        if ((mono_expect) == 0) { CHECKF(am == 0, "%ls: no font should be monospaced", fam); } \
        if ((color_expect) == 1) { CHECKF(lc == 1, "%ls: every font should be a colour font", fam); } \
        if ((color_expect) == 0) { CHECKF(ac == 0, "%ls: no font should be a colour font", fam); } \
        t_note("%ls: %u fonts, monospaced any=%d all=%d, colour any=%d all=%d", fam, n, am, lm, ac, lc); }

/* expectation values: 1 hard TRUE for every font, 0 hard FALSE for every font, -1 only recorded */
FAMILY_TEST(gap1_f1_sys_arial, L"Arial", 0, 0)
FAMILY_TEST(gap1_f1_sys_times_new_roman, L"Times New Roman", 0, 0)
FAMILY_TEST(gap1_f1_sys_courier_new, L"Courier New", 1, 0)
FAMILY_TEST(gap1_f1_sys_consolas, L"Consolas", 1, 0)
FAMILY_TEST(gap1_f1_sys_lucida_console, L"Lucida Console", 1, 0)
FAMILY_TEST(gap1_f1_sys_segoe_ui, L"Segoe UI", 0, 0)
FAMILY_TEST(gap1_f1_sys_tahoma, L"Tahoma", 0, 0)
FAMILY_TEST(gap1_f1_sys_verdana, L"Verdana", 0, 0)
FAMILY_TEST(gap1_f1_sys_georgia, L"Georgia", 0, 0)
FAMILY_TEST(gap1_f1_sys_calibri, L"Calibri", 0, 0)
FAMILY_TEST(gap1_f1_sys_cambria, L"Cambria", -1, 0)
FAMILY_TEST(gap1_f1_sys_segoe_ui_emoji, L"Segoe UI Emoji", 0, 1)
FAMILY_TEST(gap1_f1_sys_segoe_ui_symbol, L"Segoe UI Symbol", 0, -1)
FAMILY_TEST(gap1_f1_sys_marlett, L"Marlett", -1, 0)
FAMILY_TEST(gap1_f1_sys_webdings, L"Webdings", -1, 0)
FAMILY_TEST(gap1_f1_sys_wingdings, L"Wingdings", -1, 0)
FAMILY_TEST(gap1_f1_sys_symbol, L"Symbol", -1, 0)
FAMILY_TEST(gap1_f1_sys_impact, L"Impact", 0, 0)
FAMILY_TEST(gap1_f1_sys_ms_gothic, L"MS Gothic", -1, 0)
FAMILY_TEST(gap1_f1_sys_bahnschrift, L"Bahnschrift", 0, 0)
FAMILY_TEST(gap1_f1_sys_lucida_sans_unicode, L"Lucida Sans Unicode", 0, 0)
FAMILY_TEST(gap1_f1_sys_comic_sans, L"Comic Sans MS", 0, 0)
FAMILY_TEST(gap1_f1_sys_trebuchet, L"Trebuchet MS", 0, 0)
FAMILY_TEST(gap1_f1_sys_palatino_linotype, L"Palatino Linotype", 0, 0)

TEST(gap1_f1_sys_arial_panose_note)
{
    IDWriteFont *font = g1_sysfont(dw_shared(), L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFont1 *f1; DWRITE_PANOSE p;
    if (!font) { t_skip("Arial not installed"); return; }
    f1 = g1_qi((IUnknown *)font, &IID_IDWriteFont1);
    if (f1) {
        char buf[64] = "";
        memset(&p, 0xEE, sizeof p);
        IDWriteFont1_GetPanose(f1, &p);
        { int i; for (i = 0; i < 10; i++) snprintf(buf + strlen(buf), 8, "%02x ", p.values[i]); }
        t_note("Arial panose: %s", buf);
        CHECK(p.familyKind == p.values[0]);
        DW_RELEASE(f1);
    }
    DW_RELEASE(font);
}

/* ===== synthetic panose patterns =========================================================================================== */
static void pat_fill(uint8_t p[10], int kind)
{
    int i;
    for (i = 0; i < 10; i++) {
        switch (kind) {
        case 0: p[i] = 0; break;
        case 1: p[i] = (uint8_t)(i + 1); break;
        case 2: p[i] = 0xFF; break;
        case 3: p[i] = (uint8_t)(0x80 + i); break;
        case 4: p[i] = (uint8_t)(i == 0 ? 0xFE : 0); break;
        case 5: p[i] = (uint8_t)(i == 9 ? 0x7F : 0); break;
        case 6: p[i] = (uint8_t)(i & 1 ? 0xAA : 0x55); break;
        case 7: p[i] = (uint8_t)(255 - i * 25); break;
        default: p[i] = (uint8_t)(kind * 7 + i * 3); break;
        }
    }
}
TEST(gap1_f1_panose_bytes_roundtrip)
{
    int kind;
    for (kind = 0; kind < 16; kind++) {
        uint8_t want[10]; ttf_spec s; char fam[32];
        pat_fill(want, kind);
        ttf_default_spec(&s); snprintf(fam, sizeof fam, "G1Pan%d", kind); ttf_set_names(&s, fam, "Regular", fam, fam);
        if (!g1_patch_os2(&s, 32, want, 10)) { t_note("patch %d failed", kind); continue; }
        G1_FNT(g, s);
        {
            IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); IDWriteFontFace3 *f3 = g1_qi((IUnknown *)g.face, &IID_IDWriteFontFace3); DWRITE_PANOSE p;
            if (f1) { memset(&p, 0x5C, sizeof p); IDWriteFont1_GetPanose(f1, &p); CHECKF(!memcmp(&p, want, 10), "pattern %d via IDWriteFont1", kind); }
            if (f3) { memset(&p, 0x5C, sizeof p); IDWriteFontFace3_GetPanose(f3, &p); CHECKF(!memcmp(&p, want, 10), "pattern %d via IDWriteFontFace3", kind); }
            DW_RELEASE(f1); DW_RELEASE(f3);
        }
        g1_fnt_close(&g);
    }
}
TEST(gap1_f1_panose_writes_exactly_ten_bytes)
{
    uint8_t want[10]; ttf_spec s; pat_fill(want, 1);
    ttf_default_spec(&s); ttf_set_names(&s, "G1PanGuard", "Regular", "G1PanGuard", "G1PanGuard");
    g1_patch_os2(&s, 32, want, 10);
    G1_FNT(g, s);
    {
        IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); IDWriteFontFace3 *f3 = g1_qi((IUnknown *)g.face, &IID_IDWriteFontFace3); uint8_t buf[24]; int i;
        if (f1) {
            memset(buf, 0xA5, sizeof buf);
            IDWriteFont1_GetPanose(f1, (DWRITE_PANOSE *)(buf + 4));
            CHECK(!memcmp(buf + 4, want, 10));
            for (i = 0; i < 4; i++) CHECK(buf[i] == 0xA5);
            for (i = 14; i < 24; i++) CHECK(buf[i] == 0xA5);
        }
        if (f3) {
            memset(buf, 0xA5, sizeof buf);
            IDWriteFontFace3_GetPanose(f3, (DWRITE_PANOSE *)(buf + 3));         /* unaligned destination */
            CHECK(!memcmp(buf + 3, want, 10));
            for (i = 0; i < 3; i++) CHECK(buf[i] == 0xA5);
            for (i = 13; i < 24; i++) CHECK(buf[i] == 0xA5);
        }
        DW_RELEASE(f1); DW_RELEASE(f3);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_panose_union_views_agree)
{
    uint8_t want[10]; ttf_spec s; pat_fill(want, 1);
    ttf_default_spec(&s); ttf_set_names(&s, "G1PanUnion", "Regular", "G1PanUnion", "G1PanUnion");
    g1_patch_os2(&s, 32, want, 10);
    G1_FNT(g, s);
    {
        IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); DWRITE_PANOSE p;
        if (f1) {
            memset(&p, 0, sizeof p);
            IDWriteFont1_GetPanose(f1, &p);
            EXPECT_EQ(p.familyKind, 1); EXPECT_EQ(p.text.familyKind, 1); EXPECT_EQ(p.text.serifStyle, 2); EXPECT_EQ(p.text.weight, 3); EXPECT_EQ(p.text.proportion, 4);
            EXPECT_EQ(p.text.contrast, 5); EXPECT_EQ(p.text.strokeVariation, 6); EXPECT_EQ(p.text.armStyle, 7); EXPECT_EQ(p.text.letterform, 8);
            EXPECT_EQ(p.text.midline, 9); EXPECT_EQ(p.text.xHeight, 10);
            EXPECT_EQ(sizeof(DWRITE_PANOSE), 10);
        }
        DW_RELEASE(f1);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_panose_repeatable_and_independent_of_calls)
{
    uint8_t want[10]; ttf_spec s; int i; pat_fill(want, 6);
    ttf_default_spec(&s); ttf_set_names(&s, "G1PanRep", "Regular", "G1PanRep", "G1PanRep");
    g1_patch_os2(&s, 32, want, 10);
    G1_FNT(g, s);
    {
        IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); DWRITE_PANOSE p;
        for (i = 0; f1 && i < 50; i++) {
            memset(&p, i, sizeof p);
            IDWriteFont1_GetPanose(f1, &p);
            CHECK(!memcmp(&p, want, 10));
        }
        DW_RELEASE(f1);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_panose_survives_family_collection_release)
{
    uint8_t want[10]; ttf_spec s; pat_fill(want, 3);
    ttf_default_spec(&s); ttf_set_names(&s, "G1PanLife", "Regular", "G1PanLife", "G1PanLife");
    g1_patch_os2(&s, 32, want, 10);
    G1_FNT(g, s);
    {
        IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); DWRITE_PANOSE p;
        DW_RELEASE(g.fam); DW_RELEASE(g.coll); DW_RELEASE(g.face);
        if (f1) { memset(&p, 0, sizeof p); IDWriteFont1_GetPanose(f1, &p); CHECK(!memcmp(&p, want, 10)); }
        DW_RELEASE(f1);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_panose_not_changed_by_simulations)
{
    uint8_t want[10]; ttf_spec s; pat_fill(want, 7);
    ttf_default_spec(&s); ttf_set_names(&s, "G1PanSim", "Regular", "G1PanSim", "G1PanSim");
    g1_patch_os2(&s, 32, want, 10);
    G1_FNT(g, s);
    {
        IDWriteFont *bold = NULL, *obl = NULL; DWRITE_PANOSE p;
        IDWriteFontFamily_GetFirstMatchingFont(g.fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &bold);
        IDWriteFontFamily_GetFirstMatchingFont(g.fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &obl);
        IDWriteFont *fonts[2] = { bold, obl }; int i;
        for (i = 0; i < 2; i++) {
            IDWriteFont1 *f1 = fonts[i] ? g1_qi((IUnknown *)fonts[i], &IID_IDWriteFont1) : NULL;
            if (f1) {
                IDWriteFont_GetSimulations(fonts[i]);
                memset(&p, 0, sizeof p); IDWriteFont1_GetPanose(f1, &p); CHECKF(!memcmp(&p, want, 10), "simulated font %d", i);
                DW_RELEASE(f1);
            }
        }
        DW_RELEASE(bold); DW_RELEASE(obl);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_panose_old_os2_versions)
{
    int ver;
    for (ver = 0; ver <= 5; ver++) {
        uint8_t want[10]; ttf_spec s; char fam[32]; pat_fill(want, 9 + ver);
        ttf_default_spec(&s); snprintf(fam, sizeof fam, "G1PanV%d", ver); ttf_set_names(&s, fam, "Regular", fam, fam);
        s.os2_version = ver;
        if (!g1_patch_os2(&s, 32, want, 10)) { t_note("patch for OS/2 version %d failed", ver); continue; }
        G1_FNT(g, s);
        {
            IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); DWRITE_PANOSE p;
            if (f1) { memset(&p, 0x77, sizeof p); IDWriteFont1_GetPanose(f1, &p); CHECKF(!memcmp(&p, want, 10), "OS/2 version %d", ver); }
            DW_RELEASE(f1);
        }
        g1_fnt_close(&g);
    }
}
TEST(gap1_f1_panose_without_os2_table_note)
{
    ttf_spec s; ttf_default_spec(&s); ttf_set_names(&s, "G1PanNone", "Regular", "G1PanNone", "G1PanNone"); s.omit |= TTF_OMIT_OS2;
    G1_FNT(g, s);
    {
        IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); IDWriteFont2 *f2 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont2); DWRITE_PANOSE p; char buf[64] = ""; int i;
        if (f1) {
            memset(&p, 0xEE, sizeof p); IDWriteFont1_GetPanose(f1, &p);
            for (i = 0; i < 10; i++) snprintf(buf + strlen(buf), 8, "%02x ", p.values[i]);
            t_note("panose of a font without an OS/2 table: %s monospaced %d", buf, IDWriteFont1_IsMonospacedFont(f1));
        }
        if (f2) { int c = IDWriteFont2_IsColorFont(f2); CHECK(c == 0 || c == 1); EXPECT_EQ(c, 0); }
        DW_RELEASE(f1); DW_RELEASE(f2);
    }
    g1_fnt_close(&g);
}

/* ===== synthetic monospace / colour flags ================================================================================== */
TEST(gap1_f1_flags_are_independent)
{
    /* post.isFixedPitch makes a font monospaced; the colour flag comes from the colour tables only */
    static const uint32_t pal[2] = { 0xFFFF0000u, 0xFF00FF00u };
    static const ttf_colr_base base[1] = { { 2, 0, 1 } };
    static const ttf_colr_layer lay[1] = { { 64, 0 } };
    int fixed, color;
    for (fixed = 0; fixed <= 1; fixed++) for (color = 0; color <= 1; color++) {
        ttf_spec s; char fam[32]; snprintf(fam, sizeof fam, "G1Flag%d%d", fixed, color);
        ttf_default_spec(&s); ttf_set_names(&s, fam, "Regular", fam, fam);
        s.is_fixed_pitch = fixed;
        if (color) { s.colr_base = base; s.ncolr_base = 1; s.colr_layers = lay; s.ncolr_layers = 1; s.palette = pal; s.npalette = 2; }
        G1_FNT(g, s);
        {
            IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); IDWriteFont2 *f2 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont2);
            IDWriteFontFace1 *ff1 = g1_qi((IUnknown *)g.face, &IID_IDWriteFontFace1); IDWriteFontFace2 *ff2 = g1_qi((IUnknown *)g.face, &IID_IDWriteFontFace2);
            if (f1) EXPECT_EQ(IDWriteFont1_IsMonospacedFont(f1), fixed);
            if (ff1) EXPECT_EQ(IDWriteFontFace1_IsMonospacedFont(ff1), fixed);
            if (f2) EXPECT_EQ(IDWriteFont2_IsColorFont(f2), color);
            if (ff2) EXPECT_EQ(IDWriteFontFace2_IsColorFont(ff2), color);
            DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(ff1); DW_RELEASE(ff2);
        }
        g1_fnt_close(&g);
    }
}
TEST(gap1_f1_flags_return_exactly_zero_or_one)
{
    ttf_spec s; ttf_default_spec(&s); ttf_set_names(&s, "G1Exact", "Regular", "G1Exact", "G1Exact"); s.is_fixed_pitch = 7;
    G1_FNT(g, s);
    {
        IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); IDWriteFontFace1 *ff1 = g1_qi((IUnknown *)g.face, &IID_IDWriteFontFace1);
        if (f1) { BOOL m = IDWriteFont1_IsMonospacedFont(f1); EXPECT_EQ(m, 1); }
        if (ff1) { BOOL m = IDWriteFontFace1_IsMonospacedFont(ff1); EXPECT_EQ(m, 1); }
        DW_RELEASE(f1); DW_RELEASE(ff1);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_flags_stable_across_calls)
{
    ttf_spec s; int i; ttf_default_spec(&s); ttf_set_names(&s, "G1Stable", "Regular", "G1Stable", "G1Stable"); s.is_fixed_pitch = 1;
    G1_FNT(g, s);
    {
        IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); IDWriteFont2 *f2 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont2);
        for (i = 0; i < 40; i++) {
            if (f1) EXPECT_EQ(IDWriteFont1_IsMonospacedFont(f1), 1);
            if (f2) EXPECT_EQ(IDWriteFont2_IsColorFont(f2), 0);
        }
        DW_RELEASE(f1); DW_RELEASE(f2);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_flags_same_for_every_font_object_of_one_file)
{
    ttf_spec s; ttf_default_spec(&s); ttf_set_names(&s, "G1Same", "Regular", "G1Same", "G1Same"); s.is_fixed_pitch = 1;
    G1_FNT(g, s);
    {
        IDWriteFont *a = NULL, *b = NULL, *c = NULL;
        IDWriteFontFamily_GetFont(g.fam, 0, &a);
        IDWriteFontFamily_GetFirstMatchingFont(g.fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &b);
        IDWriteFontFamily_GetFirstMatchingFont(g.fam, DWRITE_FONT_WEIGHT_THIN, DWRITE_FONT_STRETCH_EXPANDED, DWRITE_FONT_STYLE_ITALIC, &c);
        IDWriteFont *fs[3] = { a, b, c }; int i;
        for (i = 0; i < 3; i++) {
            IDWriteFont1 *f1 = fs[i] ? g1_qi((IUnknown *)fs[i], &IID_IDWriteFont1) : NULL; IDWriteFont2 *f2 = fs[i] ? g1_qi((IUnknown *)fs[i], &IID_IDWriteFont2) : NULL;
            if (f1) EXPECT_EQ(IDWriteFont1_IsMonospacedFont(f1), 1);
            if (f2) EXPECT_EQ(IDWriteFont2_IsColorFont(f2), 0);
            DW_RELEASE(f1); DW_RELEASE(f2);
        }
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_interfaces_identity_and_refcounts)
{
    ttf_spec s; ttf_default_spec(&s); ttf_set_names(&s, "G1Ident", "Regular", "G1Ident", "G1Ident");
    G1_FNT(g, s);
    {
        IDWriteFont1 *f1 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont1); IDWriteFont2 *f2 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont2);
        IDWriteFont3 *f3 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont3);
        if (f1 && f2) { CHECK(dw_same_identity((IUnknown *)f1, (IUnknown *)f2)); CHECK(dw_same_identity((IUnknown *)f1, (IUnknown *)g.font)); }
        if (f1) { ULONG a = dw_refcount((IUnknown *)f1); IDWriteFont1_AddRef(f1); EXPECT_EQ(dw_refcount((IUnknown *)f1), a + 1); IDWriteFont1_Release(f1); }
        if (f3) CHECK(dw_same_identity((IUnknown *)f3, (IUnknown *)g.font));
        DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(f3);
    }
    g1_fnt_close(&g);
}
TEST(gap1_f1_color_flag_with_cpal_only_or_colr_only_note)
{
    static const uint32_t pal[2] = { 0xFFFF0000u, 0xFF00FF00u };
    static const ttf_colr_base base[1] = { { 2, 0, 1 } };
    static const ttf_colr_layer lay[1] = { { 64, 0 } };
    int variant;
    for (variant = 0; variant < 2; variant++) {
        ttf_spec s; char fam[32]; snprintf(fam, sizeof fam, "G1Half%d", variant);
        ttf_default_spec(&s); ttf_set_names(&s, fam, "Regular", fam, fam);
        if (variant == 0) { s.colr_base = base; s.ncolr_base = 1; s.colr_layers = lay; s.ncolr_layers = 1; }     /* the builder writes COLR and CPAL together; patch below */
        else { s.palette = pal; s.npalette = 2; }
        G1_FNT(g, s);
        {
            IDWriteFont2 *f2 = g1_qi((IUnknown *)g.font, &IID_IDWriteFont2); IDWriteFontFace2 *ff2 = g1_qi((IUnknown *)g.face, &IID_IDWriteFontFace2);
            if (f2 && ff2) { int c = IDWriteFont2_IsColorFont(f2); EXPECT_EQ(IDWriteFontFace2_IsColorFont(ff2), c); t_note("variant %d: IsColorFont %d", variant, c); }
            DW_RELEASE(f2); DW_RELEASE(ff2);
        }
        g1_fnt_close(&g);
    }
}
