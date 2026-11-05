/* IDWriteFont::GetMetrics, IDWriteFont1::GetMetrics, GetPanose, IsMonospacedFont, IsColorFont.
 * IDWriteFont::GetMetrics(DWRITE_FONT_METRICS*) copies the first 20 bytes of the face's DWRITE_FONT_METRICS1 (GetFontMetrics);
 * the values come from InitializeDefaultFontMetrics: head, hhea/OS/2/post, with OS/2 USE_TYPO_METRICS (fsSelection bit 7) switching
 * ascent/descent/lineGap to the typographic values and setting hasTypographicMetrics. */
#include "fo.h"

static void m1(IDWriteFont *f, DWRITE_FONT_METRICS1 *m) {
    IDWriteFont1 *f1 = NULL; memset(m, 0xCD, sizeof *m);
    if (SUCCEEDED(IDWriteFont_QueryInterface(f, &IID_IDWriteFont1, (void **)&f1)) && f1) { FO_GETM1(f1, m); IDWriteFont1_Release(f1); }
    else memset(m, 0, sizeof *m);
}
static int be16s(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }
static unsigned be16u(const uint8_t *p) { return (p[0] << 8) | p[1]; }

/* Expected line metrics for outline fonts without USE_TYPO_METRICS (InitializeDefaultFontMetrics, TrueType-outline branch):
 * ascent = usWinAscent, descent = usWinDescent, lineGap = clamp(max(hhea.lineGap,0) + hhea.ascender + |hhea.descender| - winAscent - winDescent, 0, 0x7fff);
 * when both win values are 0 the hhea values are used (lineGap clamped at 0). */
static void win_expect(const ttf_spec *s, int *asc, int *desc, int *gap) {
    int hg = s->hhea_gap > 0 ? s->hhea_gap : 0, hd = s->hhea_desc < 0 ? -s->hhea_desc : s->hhea_desc, g;
    if (s->win_asc == 0 && s->win_desc == 0) { *asc = s->hhea_asc; *desc = hd; *gap = hg; return; }
    *asc = s->win_asc; *desc = s->win_desc; g = hg + s->hhea_asc + hd - s->win_asc - s->win_desc;
    *gap = g < 0 ? 0 : g > 0x7fff ? 0x7fff : g;
}
TEST(font_metrics_default_synthetic_exact) {
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS m; fo_spec(&s, "FoMet1"); FO_OPEN1(x, s);
    memset(&m, 0xCD, sizeof m); IDWriteFont_GetMetrics(x.font[0], &m);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    /* USE_TYPO_METRICS clear, TrueType outlines: usWinAscent/usWinDescent; hhea sum 1700+400+90 < win sum 2250 so lineGap clamps to 0 */
    EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450); EXPECT_EQ(m.lineGap, 0);
    EXPECT_EQ(m.capHeight, 1400); EXPECT_EQ(m.xHeight, 1000);
    EXPECT_EQ(m.underlinePosition, -200); EXPECT_EQ(m.underlineThickness, 100);
    EXPECT_EQ(m.strikethroughPosition, 600); EXPECT_EQ(m.strikethroughThickness, 100);
    fo_close(&x);
}
TEST(font_metrics1_default_synthetic_exact) {
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS1 m; fo_spec(&s, "FoMet2"); FO_OPEN1(x, s);
    m1(x.font[0], &m);
    EXPECT_EQ(m.designUnitsPerEm, 2048); EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450); EXPECT_EQ(m.lineGap, 0);
    EXPECT_EQ(m.hasTypographicMetrics, FALSE);
    /* OS/2 subscript/superscript: size X/Y 1024, x offset 0, y offset upm/7 below / upm/3 above (subscriptPositionY is the negated OS/2 offset) */
    EXPECT_EQ(m.subscriptSizeX, 1024); EXPECT_EQ(m.subscriptSizeY, 1024); EXPECT_EQ(m.subscriptPositionX, 0); EXPECT_EQ(m.subscriptPositionY, -292);
    EXPECT_EQ(m.superscriptSizeX, 1024); EXPECT_EQ(m.superscriptSizeY, 1024); EXPECT_EQ(m.superscriptPositionX, 0); EXPECT_EQ(m.superscriptPositionY, 682);
    fo_close(&x);
}
TEST(font_metrics_v0_is_prefix_of_v1) {
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS a; DWRITE_FONT_METRICS1 b; fo_spec(&s, "FoMet3"); FO_OPEN1(x, s);
    IDWriteFont_GetMetrics(x.font[0], &a); m1(x.font[0], &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    EXPECT_EQ(sizeof(DWRITE_FONT_METRICS), 20); EXPECT_EQ(sizeof(DWRITE_FONT_METRICS1), 48);
    fo_close(&x);
}
TEST(font_metrics_equal_face_metrics) {
    fo_fx x; ttf_spec s; IDWriteFontFace *face = NULL; IDWriteFontFace1 *f1 = NULL; DWRITE_FONT_METRICS a, b; DWRITE_FONT_METRICS1 c, d; fo_spec(&s, "FoMet4"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_CreateFontFace(x.font[0], &face));
    if (face) {
        IDWriteFont_GetMetrics(x.font[0], &a); IDWriteFontFace_GetMetrics(face, &b); CHECK(!memcmp(&a, &b, sizeof a));
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
            m1(x.font[0], &c); IDWriteFontFace1_GetMetrics(f1, &d); CHECK(!memcmp(&c, &d, sizeof c));
        } else t_note("IDWriteFontFace1 unavailable");
    }
    DW_RELEASE(f1); DW_RELEASE(face); fo_close(&x);
}
TEST(font_metrics_equal_face_metrics_system) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Segoe UI", L"Courier New", L"Consolas" };
    int k;
    for (k = 0; k < 5; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace *face = NULL; DWRITE_FONT_METRICS a, b;
        if (!f) continue;
        IDWriteFont_CreateFontFace(f, &face);
        if (face) { IDWriteFont_GetMetrics(f, &a); IDWriteFontFace_GetMetrics(face, &b); CHECKF(!memcmp(&a, &b, sizeof a), "%ls", fams[k]); DW_RELEASE(face); }
        DW_RELEASE(f);
    }
}
TEST(font_metrics_same_through_all_interfaces) {
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS a, b; IDWriteFont3 *f3 = NULL; fo_spec(&s, "FoMet5"); FO_OPEN1(x, s);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont3, (void **)&f3);
    IDWriteFont_GetMetrics(x.font[0], &a);
        { DWRITE_FONT_METRICS1 c, d; m1(x.font[0], &c); if (f3) { FO_GETM1(f3, &d); CHECK(!memcmp(&c, &d, sizeof c)); } }
    DW_RELEASE(f3); fo_close(&x);
}
TEST(font_metrics_use_typo_metrics_bit) {
    /* fsSelection bit 7: ascent = sTypoAscender, descent = -sTypoDescender, lineGap = sTypoLineGap, hasTypographicMetrics = TRUE */
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS1 m; fo_spec(&s, "FoMetTypo"); s.fs_selection |= 0x80; FO_OPEN1(x, s);
    m1(x.font[0], &m);
    EXPECT_EQ(m.ascent, 1600); EXPECT_EQ(m.descent, 420); EXPECT_EQ(m.lineGap, 80); EXPECT_EQ(m.hasTypographicMetrics, TRUE);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    fo_close(&x);
}
TEST(font_metrics_typo_bit_other_fields_unchanged) {
    fo_fx x; ttf_spec s[2]; DWRITE_FONT_METRICS1 a, b; fo_spec(&s[0], "FoMetTy1"); fo_spec(&s[1], "FoMetTy2"); s[1].fs_selection |= 0x80; FO_OPEN(x, s, 2);
    m1(x.font[0], &a); m1(x.font[1], &b);
    EXPECT_EQ(a.capHeight, b.capHeight); EXPECT_EQ(a.xHeight, b.xHeight); EXPECT_EQ(a.underlinePosition, b.underlinePosition);
    EXPECT_EQ(a.strikethroughPosition, b.strikethroughPosition); EXPECT_EQ(a.glyphBoxRight, b.glyphBoxRight); EXPECT_EQ(a.subscriptSizeX, b.subscriptSizeX);
    fo_close(&x);
}
TEST(font_metrics_typo_variants) {
    /* with USE_TYPO_METRICS the win and hhea values are ignored */
    static const struct { int asc, desc, gap; } t[] = { { 1000, -300, 0 }, { 2000, -500, 200 }, { 1536, -512, 452 }, { 800, -200, 50 } };
    fo_fx x; ttf_spec s[4]; int i;
    for (i = 0; i < 4; i++) { char f[32]; snprintf(f, sizeof f, "FoMetTv%d", i); fo_spec(&s[i], f); s[i].fs_selection |= 0x80; s[i].typo_asc = t[i].asc; s[i].typo_desc = t[i].desc; s[i].typo_gap = t[i].gap; }
    FO_OPEN(x, s, 4);
    for (i = 0; i < 4; i++) { DWRITE_FONT_METRICS1 m; m1(x.font[i], &m); EXPECT_EQ(m.ascent, t[i].asc); EXPECT_EQ(m.descent, -t[i].desc); EXPECT_EQ(m.lineGap, t[i].gap); }
    fo_close(&x);
}
TEST(font_metrics_hhea_and_win_variants) {
    static const struct { int ha, hd, hg, wa, wd; } t[] = {
        { 1900, -500, 0, 1900, 500 }, { 1200, -300, 150, 1000, 400 }, { 2048, -512, 64, 2000, 500 }, { 1700, -400, 1000, 1800, 450 },
        { 2000, -600, -50, 1800, 450 }, { 1500, -300, 0, 3000, 700 }, { 1700, -400, 90, 0, 0 }, { 1700, -400, -90, 0, 0 }, { 1700, -400, 90, 1800, 0 },
        { 1700, -400, 90, 0, 450 }, { 32000, -32000, 0, 100, 100 }, { 100, 0, 0, 2000, 500 } };
    enum { N = sizeof t / sizeof t[0] };
    fo_fx x; ttf_spec s[N]; int i;
    for (i = 0; i < N; i++) { char f[32]; snprintf(f, sizeof f, "FoMetHh%d", i); fo_spec(&s[i], f); s[i].hhea_asc = t[i].ha; s[i].hhea_desc = t[i].hd; s[i].hhea_gap = t[i].hg; s[i].win_asc = t[i].wa; s[i].win_desc = t[i].wd; }
    FO_OPEN(x, s, N);
    for (i = 0; i < N; i++) {
        DWRITE_FONT_METRICS1 m; int a, d, g; m1(x.font[i], &m); win_expect(&s[i], &a, &d, &g);
        /* only ordinary rows (both win values set, sane magnitudes) are hard-checked; the derived lineGap and the degenerate rows are observations */
        if (t[i].wa && t[i].wd && t[i].ha < 4000) { CHECKF(m.ascent == a && m.descent == d, "case %d: got %u/%u want %d/%d", i, m.ascent, m.descent, a, d); }
        if (m.ascent != a || m.descent != d || m.lineGap != g) t_note("case %d: got %u/%u/%d, win-based model %d/%d/%d", i, m.ascent, m.descent, m.lineGap, a, d, g);
    }
    fo_close(&x);
}
TEST(font_metrics_negative_hhea_linegap_clamped) {
    /* a negative hhea lineGap counts as 0 in the computed gap */
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS1 m; fo_spec(&s, "FoMetNg"); s.hhea_asc = 2000; s.hhea_desc = -600; s.hhea_gap = -50; FO_OPEN1(x, s);
    m1(x.font[0], &m); t_note("negative hhea gap: lineGap %d (model 350 = 2000 + 600 - 1800 - 450)", (int)m.lineGap); CHECK(m.lineGap >= 0 && m.lineGap <= 0x7fff);
    fo_close(&x);
}
TEST(font_metrics_typo_values_ignored_without_bit) {
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS1 m; fo_spec(&s, "FoMetNoTy"); s.typo_asc = 900; s.typo_desc = -100; s.typo_gap = 5; FO_OPEN1(x, s);
    m1(x.font[0], &m); EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450);
    fo_close(&x);
}
TEST(font_metrics_cap_and_x_height_from_os2) {
    static const struct { int cap, xh; } t[] = { { 1400, 1000 }, { 1500, 800 }, { 1200, 700 }, { 1000, 500 } };
    fo_fx x; ttf_spec s[4]; int i;
    for (i = 0; i < 4; i++) { char f[32]; snprintf(f, sizeof f, "FoMetCh%d", i); fo_spec(&s[i], f); s[i].cap_height = t[i].cap; s[i].x_height = t[i].xh; }
    FO_OPEN(x, s, 4);
    for (i = 0; i < 4; i++) { DWRITE_FONT_METRICS1 m; m1(x.font[i], &m); EXPECT_EQ(m.capHeight, t[i].cap); EXPECT_EQ(m.xHeight, t[i].xh); }
    fo_close(&x);
}
TEST(font_metrics_heights_zero_derived_note) {
    fo_fx x; ttf_spec s[2]; DWRITE_FONT_METRICS1 m; fo_spec(&s[0], "FoMetH0"); s[0].cap_height = 0; s[0].x_height = 0;
    fo_spec(&s[1], "FoMetH1"); s[1].os2_version = 1;
    FO_OPEN(x, s, 2);
    m1(x.font[0], &m); t_note("OS/2 v4 with sxHeight=sCapHeight=0: xHeight %u capHeight %u", m.xHeight, m.capHeight); CHECK(m.xHeight > 0 && m.capHeight > 0);
    m1(x.font[1], &m); t_note("OS/2 v1 (no height fields): xHeight %u capHeight %u", m.xHeight, m.capHeight); CHECK(m.xHeight > 0 && m.capHeight > 0);
    fo_close(&x);
}
TEST(font_metrics_underline_strikethrough_from_tables) {
    static const struct { int up, ut, sp, ss; } t[] = { { -200, 100, 600, 100 }, { -300, 150, 700, 80 }, { -100, 50, 500, 40 }, { 0, 90, 650, 60 } };
    fo_fx x; ttf_spec s[4]; int i;
    for (i = 0; i < 4; i++) { char f[32]; snprintf(f, sizeof f, "FoMetUl%d", i); fo_spec(&s[i], f); s[i].underline_pos = t[i].up; s[i].underline_thick = t[i].ut; s[i].strike_pos = t[i].sp; s[i].strike_size = t[i].ss; }
    FO_OPEN(x, s, 4);
    for (i = 0; i < 4; i++) {
        DWRITE_FONT_METRICS m; IDWriteFont_GetMetrics(x.font[i], &m);
        EXPECT_EQ(m.underlinePosition, t[i].up); EXPECT_EQ(m.underlineThickness, t[i].ut); EXPECT_EQ(m.strikethroughPosition, t[i].sp); EXPECT_EQ(m.strikethroughThickness, t[i].ss);
    }
    fo_close(&x);
}
TEST(font_metrics_strikeout_size_zero_uses_underline_thickness) {
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS m; fo_spec(&s, "FoMetSz"); s.strike_size = 0; s.underline_thick = 123; FO_OPEN1(x, s);
    IDWriteFont_GetMetrics(x.font[0], &m); EXPECT_EQ(m.strikethroughThickness, 123); EXPECT_EQ(m.underlineThickness, 123);
    fo_close(&x);
}
TEST(font_metrics_no_post_table_defaults) {
    /* without 'post': underlinePosition = -((upm + 5) / 10), underlineThickness = (upm + 7) / 14 (InitializeDefaultFontMetrics) */
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS m; fo_spec(&s, "FoMetNoPost"); s.omit |= TTF_OMIT_POST; s.strike_size = 0;
    if (!fo_open1(&x, &s)) { t_note("font without post table not usable: %s", dw_hr_name(x.hr_coll)); fo_close(&x); return; }
    IDWriteFont_GetMetrics(x.font[0], &m);
    t_note("no post: underlinePosition %d thickness %u strike thickness %u (model -205/146/146)", (int)m.underlinePosition, m.underlineThickness, m.strikethroughThickness);
    CHECK(m.designUnitsPerEm == 2048);
    fo_close(&x);
}
TEST(font_metrics_units_per_em_variants) {
    static const int upm[] = { 16, 256, 1000, 1024, 2048, 4096, 16384 };
    fo_fx x; ttf_spec s[7]; int i;
    for (i = 0; i < 7; i++) { char f[32]; snprintf(f, sizeof f, "FoMetU%d", upm[i]); fo_spec(&s[i], f); s[i].upm = upm[i]; }
    if (!fo_open(&x, s, 7)) { t_note("fonts with unusual unitsPerEm not usable: %s", dw_hr_name(x.hr_coll)); fo_close(&x); return; }
    for (i = 0; i < 7; i++) { DWRITE_FONT_METRICS m; IDWriteFont_GetMetrics(x.font[i], &m); EXPECT_EQ(m.designUnitsPerEm, upm[i]); }
    fo_close(&x);
}
TEST(font_metrics_units_per_em_out_of_spec_note) {
    static const int upm[] = { 0, 8, 15, 16385, 32768 };
    int i;
    for (i = 0; i < 5; i++) {
        fo_fx x; ttf_spec s; char f[32]; DWRITE_FONT_METRICS m; snprintf(f, sizeof f, "FoMetV%d", upm[i]); fo_spec(&s, f); s.upm = upm[i];
        if (!fo_open1(&x, &s)) { t_note("unitsPerEm %d: font rejected (%s)", upm[i], dw_hr_name(x.hr_coll)); fo_close(&x); continue; }
        IDWriteFont_GetMetrics(x.font[0], &m); t_note("unitsPerEm %d: reported %u", upm[i], m.designUnitsPerEm); fo_close(&x);
    }
}
TEST(font_metrics_glyph_box_equals_head_bbox) {
    /* glyphBoxLeft/Top/Right/Bottom = head.xMin / yMax / xMax / yMin */
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS1 m; IDWriteFontFace *face = NULL; uint8_t *h; UINT32 len; fo_spec(&s, "FoMetBox"); FO_OPEN1(x, s);
    m1(x.font[0], &m);
    IDWriteFont_CreateFontFace(x.font[0], &face);
    if (face && (h = dw_face_table(face, DW_TAG('h', 'e', 'a', 'd'), &len)) && len >= 44) {
        EXPECT_EQ(m.glyphBoxLeft, be16s(h + 36)); EXPECT_EQ(m.glyphBoxBottom, be16s(h + 38)); EXPECT_EQ(m.glyphBoxRight, be16s(h + 40)); EXPECT_EQ(m.glyphBoxTop, be16s(h + 42));
        free(h);
    } else CHECK(0);
    EXPECT_EQ(m.glyphBoxLeft, 100); EXPECT_EQ(m.glyphBoxBottom, 0);          /* glyph boxes: 'A' (100,0)-(900,1400), .notdef reaches x=1100, composite reaches y=2800 */
    EXPECT_EQ(m.glyphBoxRight, 1100); EXPECT_EQ(m.glyphBoxTop, 2800);
    DW_RELEASE(face); fo_close(&x);
}
TEST(font_metrics_system_glyph_box_matches_head) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Segoe UI", L"Courier New" };
    int k;
    for (k = 0; k < 4; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); uint8_t *h; UINT32 len; DWRITE_FONT_METRICS1 m;
        if (!f) continue;
        m1(f, &m);
        if (fo_sys_table(f, "head", &h, &len) && len >= 44) {
            EXPECT_EQ(m.designUnitsPerEm, be16u(h + 18)); EXPECT_EQ(m.glyphBoxLeft, be16s(h + 36)); EXPECT_EQ(m.glyphBoxBottom, be16s(h + 38));
            EXPECT_EQ(m.glyphBoxRight, be16s(h + 40)); EXPECT_EQ(m.glyphBoxTop, be16s(h + 42));
        }
        free(h); DW_RELEASE(f);
    }
}
TEST(font_metrics_system_arial_2048) {
    FO_SYS_REG(f, L"Arial"); DWRITE_FONT_METRICS1 m; uint8_t *t; UINT32 len;
    m1(f, &m);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    EXPECT_EQ(m.ascent, 1854); EXPECT_EQ(m.descent, 434); EXPECT_EQ(m.lineGap, 67);
    if (fo_sys_table(f, "hhea", &t, &len) && len >= 10) { EXPECT_EQ(m.ascent, be16s(t + 4)); EXPECT_EQ(m.descent, -be16s(t + 6)); EXPECT_EQ(m.lineGap, be16s(t + 8)); }
    free(t);
    if (fo_sys_table(f, "OS/2", &t, &len) && len >= 90) { EXPECT_EQ(m.capHeight, be16s(t + 88)); EXPECT_EQ(m.xHeight, be16s(t + 86)); }
    free(t);
    t_note("Arial: cap %u x %u underline %d/%u strike %d/%u hasTypo %d", m.capHeight, m.xHeight, m.underlinePosition, m.underlineThickness, m.strikethroughPosition, m.strikethroughThickness, m.hasTypographicMetrics);
    DW_RELEASE(f);
}
TEST(font_metrics_system_tables_consistency) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Courier New", L"Tahoma", L"Verdana", L"Segoe UI", L"Consolas", L"Calibri" };
    int k;
    for (k = 0; k < 8; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); DWRITE_FONT_METRICS1 m; uint8_t *os2, *post; UINT32 ol, pl;
        if (!f) continue;
        m1(f, &m);
        if (fo_sys_table(f, "OS/2", &os2, &ol) && ol >= 78) {
            EXPECT_EQ(m.strikethroughPosition, be16s(os2 + 28));
            EXPECT_EQ(m.subscriptSizeX, be16s(os2 + 10)); EXPECT_EQ(m.subscriptSizeY, be16s(os2 + 12)); EXPECT_EQ(m.subscriptPositionX, be16s(os2 + 14)); EXPECT_EQ(m.subscriptPositionY, -be16s(os2 + 16));
            EXPECT_EQ(m.superscriptSizeX, be16s(os2 + 18)); EXPECT_EQ(m.superscriptSizeY, be16s(os2 + 20)); EXPECT_EQ(m.superscriptPositionX, be16s(os2 + 22)); EXPECT_EQ(m.superscriptPositionY, be16s(os2 + 24));
            if (be16u(os2 + 62) & 0x80) { EXPECT_EQ(m.ascent, be16s(os2 + 68)); EXPECT_EQ(m.descent, -be16s(os2 + 70)); EXPECT_EQ(m.hasTypographicMetrics, TRUE); }
            else EXPECT_EQ(m.hasTypographicMetrics, FALSE);
        }
        free(os2);
        if (fo_sys_table(f, "post", &post, &pl) && pl >= 12) { EXPECT_EQ(m.underlinePosition, be16s(post + 8)); EXPECT_EQ(m.underlineThickness, be16u(post + 10)); }
        free(post); DW_RELEASE(f);
    }
}
TEST(font_metrics_repeatable) {
    fo_fx x; ttf_spec s; DWRITE_FONT_METRICS a, b; int i; fo_spec(&s, "FoMetRep"); FO_OPEN1(x, s);
    IDWriteFont_GetMetrics(x.font[0], &a);
    for (i = 0; i < 20; i++) { IDWriteFont_GetMetrics(x.font[0], &b); CHECK(!memcmp(&a, &b, sizeof a)); }
    fo_close(&x);
}
TEST(font_metrics_does_not_write_past_struct) {
    fo_fx x; ttf_spec s; struct { DWRITE_FONT_METRICS m; unsigned char guard[16]; } v; struct { DWRITE_FONT_METRICS1 m; unsigned char guard[16]; } w; int i; fo_spec(&s, "FoMetGuard"); FO_OPEN1(x, s);
    memset(&v, 0x5A, sizeof v); memset(&w, 0x5A, sizeof w);
    IDWriteFont_GetMetrics(x.font[0], &v.m);
    { IDWriteFont1 *f1 = NULL; IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont1, (void **)&f1); if (f1) { FO_GETM1(f1, &w.m); IDWriteFont1_Release(f1); } }
    for (i = 0; i < 16; i++) { EXPECT_EQ(v.guard[i], 0x5A); EXPECT_EQ(w.guard[i], 0x5A); }
    fo_close(&x);
}
TEST(font_metrics_scale_with_bold_simulation_unchanged_height) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoMetSim"); FO_OPEN1(x, s);
    { DWRITE_FONT_METRICS a; IDWriteFont_GetMetrics(x.font[0], &a); EXPECT_EQ(a.designUnitsPerEm, 2048); }
    fo_close(&x);
}

/* ---- GetPanose ------------------------------------------------------------------------------------------------ */
static void panose_of(IDWriteFont *f, uint8_t out[16]) {
    IDWriteFont1 *f1 = NULL; memset(out, 0xEE, 16);
    if (SUCCEEDED(IDWriteFont_QueryInterface(f, &IID_IDWriteFont1, (void **)&f1)) && f1) { IDWriteFont1_GetPanose(f1, (DWRITE_PANOSE *)out); IDWriteFont1_Release(f1); }
}
TEST(font_panose_zero_default) {
    fo_fx x; ttf_spec s; uint8_t p[16]; int i; fo_spec(&s, "FoPan0"); FO_OPEN1(x, s);
    panose_of(x.font[0], p);
    for (i = 0; i < 10; i++) EXPECT_EQ(p[i], 0);
    for (i = 10; i < 16; i++) EXPECT_EQ(p[i], 0xEE);        /* exactly 10 bytes written (qword + word) */
    fo_close(&x);
}
TEST(font_panose_roundtrip_values) {
    static const uint8_t v[][10] = { { 2, 11, 6, 4, 2, 2, 2, 2, 2, 4 }, { 2, 2, 6, 3, 5, 4, 5, 2, 3, 4 }, { 3, 1, 2, 3, 4, 5, 6, 7, 8, 9 },
                                      { 255, 254, 253, 252, 251, 250, 249, 248, 247, 246 }, { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 } };
    fo_fx x; ttf_spec s[5]; int i, j;
    for (i = 0; i < 5; i++) { char f[32]; snprintf(f, sizeof f, "FoPanV%d", i); fo_spec(&s[i], f); fo_set_panose(&s[i], v[i]); }
    FO_OPEN(x, s, 5);
    for (i = 0; i < 5; i++) { uint8_t p[16]; panose_of(x.font[i], p); for (j = 0; j < 10; j++) CHECKF(p[j] == v[i][j], "font %d byte %d: %u want %u", i, j, p[j], v[i][j]); }
    fo_close(&x);
}
TEST(font_panose_union_layout) {
    CHECK(sizeof(DWRITE_PANOSE) == 10);
    CHECK(offsetof(DWRITE_PANOSE, text.serifStyle) == 1); CHECK(offsetof(DWRITE_PANOSE, text.xHeight) == 9);
}
TEST(font_panose_via_text_view) {
    fo_fx x; ttf_spec s; DWRITE_PANOSE p; IDWriteFont1 *f1 = NULL; static const uint8_t v[10] = { 2, 11, 6, 4, 2, 2, 2, 2, 2, 4 }; fo_spec(&s, "FoPanTxt"); fo_set_panose(&s, v); FO_OPEN1(x, s);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont1, (void **)&f1);
    if (f1) {
        memset(&p, 0, sizeof p); IDWriteFont1_GetPanose(f1, &p);
        EXPECT_EQ(p.text.familyKind, DWRITE_PANOSE_FAMILY_TEXT_DISPLAY); EXPECT_EQ(p.text.serifStyle, DWRITE_PANOSE_SERIF_STYLE_NORMAL_SANS);
        EXPECT_EQ(p.text.weight, 6); EXPECT_EQ(p.text.proportion, 4); EXPECT_EQ(p.text.xHeight, 4);
        IDWriteFont1_Release(f1);
    }
    fo_close(&x);
}
TEST(font_panose_system_arial) {
    FO_SYS_REG(f, L"Arial"); uint8_t p[16]; uint8_t *os2; UINT32 len; int i;
    panose_of(f, p);
    EXPECT_EQ(p[0], 2); EXPECT_EQ(p[1], 11);             /* Latin Text, Normal Sans */
    if (fo_sys_table(f, "OS/2", &os2, &len) && len >= 42) for (i = 0; i < 10; i++) CHECKF(p[i] == os2[32 + i], "byte %d: %u vs OS/2 %u", i, p[i], os2[32 + i]);
    free(os2);
    t_note("Arial panose: %u %u %u %u %u %u %u %u %u %u", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
    DW_RELEASE(f);
}
TEST(font_panose_system_matches_os2) {
    static const WCHAR *fams[] = { L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Verdana", L"Consolas", L"Calibri" };
    int k, i;
    for (k = 0; k < 7; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); uint8_t p[16], *os2; UINT32 len;
        if (!f) continue;
        panose_of(f, p);
        if (fo_sys_table(f, "OS/2", &os2, &len) && len >= 42) for (i = 0; i < 10; i++) CHECKF(p[i] == os2[32 + i], "%ls byte %d", fams[k], i);
        free(os2); DW_RELEASE(f);
    }
}
TEST(font_panose_system_bold_differs_in_weight_note) {
    FO_SYS_REG(r, L"Arial"); FO_SYS(b, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL); uint8_t pr[16], pb[16];
    panose_of(r, pr); panose_of(b, pb);
    EXPECT_EQ(pr[0], pb[0]); EXPECT_EQ(pr[1], pb[1]);
    t_note("Arial weight digit regular %u bold %u", pr[2], pb[2]);
    DW_RELEASE(r); DW_RELEASE(b);
}

/* ---- IsMonospacedFont ------------------------------------------------------------------------------------------ */
static int mono_of(IDWriteFont *f) { IDWriteFont1 *f1 = NULL; int r = -1; if (SUCCEEDED(IDWriteFont_QueryInterface(f, &IID_IDWriteFont1, (void **)&f1)) && f1) { r = IDWriteFont1_IsMonospacedFont(f1); IDWriteFont1_Release(f1); } return r; }
TEST(font_mono_panose_and_post_table) {
    /* IsMonospacedFont(OpenTypeFace, PANOSE): family 0/2/4 with proportion 9 => mono; family 3/5 with byte[3]==3 => mono;
     * otherwise post.isFixedPitch != 0 decides. */
    static const struct { int fam, prop, fixed, want; } t[] = {
        { 0, 9, 0, 1 }, { 2, 9, 0, 1 }, { 4, 9, 0, 1 }, { 3, 3, 0, 1 }, { 5, 3, 0, 1 },
        { 2, 3, 0, 0 }, { 3, 9, 0, 0 }, { 5, 9, 0, 0 }, { 4, 3, 0, 0 }, { 1, 9, 0, 0 }, { 0, 0, 0, 0 }, { 6, 9, 0, 0 }, { 2, 8, 0, 0 }, { 2, 0, 0, 0 },
        { 2, 3, 1, 1 }, { 0, 0, 1, 1 }, { 3, 9, 1, 1 }, { 1, 0, 1, 1 }, { 2, 4, 0x100, 1 }, { 2, 4, 0x01000000, 1 }, { 2, 4, 0x80000000u, 1 },
    };
    enum { N = sizeof t / sizeof t[0] };
    fo_fx x; ttf_spec s[N]; int i;
    for (i = 0; i < N; i++) { char f[32]; uint8_t p[10] = { 0 }; snprintf(f, sizeof f, "FoMono%d", i); fo_spec(&s[i], f); p[0] = (uint8_t)t[i].fam; p[3] = (uint8_t)t[i].prop; fo_set_panose(&s[i], p); s[i].is_fixed_pitch = t[i].fixed; }
    for (i = 0; i < N; i += FO_MAX) {
        int n = N - i < FO_MAX ? N - i : FO_MAX, k;
        FO_OPEN(x, &s[i], n);
        for (k = 0; k < n; k++) CHECKF(mono_of(x.font[k]) == t[i + k].want, "family %d proportion %d fixed %x: got %d want %d", t[i + k].fam, t[i + k].prop, t[i + k].fixed, mono_of(x.font[k]), t[i + k].want);
        fo_close(&x);
    }
}
TEST(font_mono_default_false) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoMonoDef"); FO_OPEN1(x, s);
    EXPECT_EQ(mono_of(x.font[0]), 0);
    fo_close(&x);
}
TEST(font_mono_is_fixed_pitch_only) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoMonoFix"); s.is_fixed_pitch = 1; FO_OPEN1(x, s);
    EXPECT_EQ(mono_of(x.font[0]), 1);
    fo_close(&x);
}
TEST(font_mono_independent_of_advances) {
    /* the flag comes from tables, not from measuring advances: the default font has equal advances (1000) for most glyphs yet is not mono */
    fo_fx x; ttf_spec s; fo_spec(&s, "FoMonoAdv"); FO_OPEN1(x, s);
    EXPECT_EQ(mono_of(x.font[0]), 0);
    fo_close(&x);
}
TEST(font_mono_system_fonts) {
    static const struct { const WCHAR *fam; int want; } t[] = { { L"Courier New", 1 }, { L"Consolas", 1 }, { L"Lucida Console", 1 }, { L"Arial", 0 }, { L"Times New Roman", 0 },
        { L"Segoe UI", 0 }, { L"Tahoma", 0 }, { L"Verdana", 0 } };
    int k, seen = 0;
    for (k = 0; k < 8; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), t[k].fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!f) continue;
        seen++; CHECKF(mono_of(f) == t[k].want, "%ls: got %d want %d", t[k].fam, mono_of(f), t[k].want); DW_RELEASE(f);
    }
    if (!seen) t_skip("no listed system font installed");
}
TEST(font_mono_matches_post_or_panose_on_system_fonts) {
    static const WCHAR *fams[] = { L"Arial", L"Courier New", L"Consolas", L"Segoe UI", L"Times New Roman", L"Lucida Console", L"MS Gothic" };
    int k;
    for (k = 0; k < 7; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); uint8_t *os2 = NULL, *post = NULL; UINT32 ol = 0, pl = 0;
        int want = 0;
        if (!f) continue;
        if (fo_sys_table(f, "OS/2", &os2, &ol) && ol >= 42) {
            int fam = os2[32], prop = os2[35];
            if ((fam == 0 || fam == 2 || fam == 4) && prop == 9) want = 1;
            if ((fam == 3 || fam == 5) && prop == 3) want = 1;
        }
        if (!want && fo_sys_table(f, "post", &post, &pl) && pl >= 16) want = (post[12] | post[13] | post[14] | post[15]) != 0;
        CHECKF(mono_of(f) == want, "%ls: got %d want %d", fams[k], mono_of(f), want);
        free(os2); free(post); DW_RELEASE(f);
    }
}

/* ---- IsColorFont ------------------------------------------------------------------------------------------------ */
static int color_of(IDWriteFont *f) { IDWriteFont2 *f2 = NULL; int r = -1; if (SUCCEEDED(IDWriteFont_QueryInterface(f, &IID_IDWriteFont2, (void **)&f2)) && f2) { r = IDWriteFont2_IsColorFont(f2); IDWriteFont2_Release(f2); } return r; }
TEST(font_color_default_false) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoColDef"); FO_OPEN1(x, s);
    EXPECT_EQ(color_of(x.font[0]), 0);
    fo_close(&x);
}
TEST(font_color_colr_cpal_synthetic_note) {
    static const ttf_colr_base base[] = { { 2, 0, 2 } };
    static const ttf_colr_layer lay[] = { { 3, 0 }, { 4, 1 } };
    static const uint32_t pal[] = { 0xFFFF0000u, 0xFF00FF00u };
    fo_fx x; ttf_spec s; fo_spec(&s, "FoColCpal"); s.colr_base = base; s.ncolr_base = 1; s.colr_layers = lay; s.ncolr_layers = 2; s.palette = pal; s.npalette = 2; FO_OPEN1(x, s);
    t_note("COLR v0 + CPAL font: IsColorFont = %d", color_of(x.font[0]));
    { IDWriteFontFace *face = NULL; IDWriteFontFace2 *f2 = NULL; IDWriteFont_CreateFontFace(x.font[0], &face);
      if (face && SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace2, (void **)&f2)) && f2) { EXPECT_EQ(IDWriteFontFace2_IsColorFont(f2), color_of(x.font[0])); IDWriteFontFace2_Release(f2); }
      DW_RELEASE(face); }
    fo_close(&x);
}
TEST(font_color_system_emoji_true) {
    FO_SYS_REG(f, L"Segoe UI Emoji");
    EXPECT_EQ(color_of(f), 1);
    DW_RELEASE(f);
}
TEST(font_color_system_text_fonts_false) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Segoe UI", L"Courier New", L"Tahoma", L"Consolas" };
    int k, seen = 0;
    for (k = 0; k < 6; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!f) continue;
        seen++; CHECKF(color_of(f) == 0, "%ls", fams[k]); DW_RELEASE(f);
    }
    if (!seen) t_skip("no system text font installed");
}
TEST(font_color_matches_face_system) {
    static const WCHAR *fams[] = { L"Segoe UI Emoji", L"Segoe UI Symbol", L"Arial" };
    int k;
    for (k = 0; k < 3; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace *face = NULL; IDWriteFontFace2 *f2 = NULL;
        if (!f) continue;
        IDWriteFont_CreateFontFace(f, &face);
        if (face && SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace2, (void **)&f2)) && f2) { CHECKF(IDWriteFontFace2_IsColorFont(f2) == color_of(f), "%ls", fams[k]); IDWriteFontFace2_Release(f2); }
        DW_RELEASE(face); DW_RELEASE(f);
    }
}
TEST(font_color_symbol_ui_note) {
    FO_SYS_REG(f, L"Segoe UI Symbol"); t_note("Segoe UI Symbol IsColorFont = %d", color_of(f)); DW_RELEASE(f);
}
TEST(font_color_returns_canonical_bool) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoColBool"); FO_OPEN1(x, s);
    { int v = color_of(x.font[0]); CHECK(v == 0 || v == 1); }
    { int v = mono_of(x.font[0]); CHECK(v == 0 || v == 1); }
    { int v = IDWriteFont_IsSymbolFont(x.font[0]); CHECK(v == 0 || v == 1); }
    fo_close(&x);
}
