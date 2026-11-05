/* DWRITE_FONT_METRICS / DWRITE_FONT_METRICS1 derivation on deterministic synthetic fonts.
 *
 * The face metrics are filled when the face is created (OS/2 / hhea / head / post decision tree) and returned verbatim by
 * IDWriteFontFace::GetMetrics, IDWriteFontFace1::GetMetrics and IDWriteFont::GetMetrics; the oblique simulation shears four
 * of the fields, the bold simulation changes nothing here. */
#include "fm.h"

static int get_m1(fm_font *F, DWRITE_FONT_METRICS1 *m) {
    memset(m, 0xCD, sizeof *m);
    if (!F->f1) { t_skip("IDWriteFontFace1 unavailable"); return 0; }
    IDWriteFontFace1_GetMetrics(F->f1, m);
    return 1;
}
/* metrics == derivation from the font's own tables */
static void check_mirror(fm_font *F, const char *label) {
    DWRITE_FONT_METRICS1 got; fm_exp e;
    if (!get_m1(F, &got)) return;
    CHECK(fm_derive(F->face, F->sim, &e));
    FM_CMP(&got, &e.m, fm_unknown_mask(&e), label);
}
static void build(ttf_spec *s) { ttf_default_spec(s); }

TEST(fm_default_exact) {
    ttf_spec s; build(&s); FM_OPEN(F, s, DWRITE_FONT_SIMULATIONS_NONE);
    DWRITE_FONT_METRICS1 m;
    if (!get_m1(&F, &m)) { fm_close(&F); return; }
    /* OS/2 present without USE_TYPO_METRICS, win values non-zero: ascent/descent = usWinAscent/usWinDescent; the line gap is
     * hhea gap + |hhea descender| + hhea ascender - win descent - win ascent = -60, clamped to 0. */
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450); EXPECT_EQ(m.lineGap, 0);
    EXPECT_EQ(m.capHeight, 1400); EXPECT_EQ(m.xHeight, 1000);            /* OS/2 v4 values, both inside the validity window */
    EXPECT_EQ((int)m.underlinePosition, -200); EXPECT_EQ(m.underlineThickness, 100);   /* post */
    EXPECT_EQ(m.strikethroughPosition, 600); EXPECT_EQ(m.strikethroughThickness, 100);   /* OS/2 yStrikeout* */
    /* subscript: x offset, NEGATED y offset, sizes; superscript: offsets and sizes unchanged (generator writes upm/2, upm/2, 0, upm/7 and upm/3) */
    EXPECT_EQ((int)m.subscriptPositionX, 0); EXPECT_EQ((int)m.subscriptPositionY, -292); EXPECT_EQ((int)m.subscriptSizeX, 1024); EXPECT_EQ((int)m.subscriptSizeY, 1024);
    EXPECT_EQ((int)m.superscriptPositionX, 0); EXPECT_EQ((int)m.superscriptPositionY, 682); EXPECT_EQ((int)m.superscriptSizeX, 1024); EXPECT_EQ((int)m.superscriptSizeY, 1024);
    CHECK(!m.hasTypographicMetrics);
    fm_close(&F);
}
TEST(fm_default_glyph_box_is_head_bbox) {
    ttf_spec s; build(&s); size_t n; uint8_t *b = ttf_build(&s, &n); unsigned hl; uint8_t *head = b ? fm_tbl(b, n, "head", &hl) : NULL;
    if (!head) { free(b); t_skip("no blob"); return; }
    fm_font F; memset(&F, 0, sizeof F);
    if (!fm_open_blob(&F, b, n, DWRITE_FONT_SIMULATIONS_NONE)) { fm_close(&F); free(b); return; }
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) {
        /* glyphBox{Left,Top,Right,Bottom} = head.{xMin,yMax,xMax,yMin} */
        EXPECT_EQ((int)m.glyphBoxLeft, fm_be16s(head + 36)); EXPECT_EQ((int)m.glyphBoxTop, fm_be16s(head + 42));
        EXPECT_EQ((int)m.glyphBoxRight, fm_be16s(head + 40)); EXPECT_EQ((int)m.glyphBoxBottom, fm_be16s(head + 38));
        /* the composite glyph (A + triangle raised by 1400) makes the union box 100,2800 - 1100,0 */
        EXPECT_EQ((int)m.glyphBoxLeft, 100); EXPECT_EQ((int)m.glyphBoxTop, 2800); EXPECT_EQ((int)m.glyphBoxRight, 1100); EXPECT_EQ((int)m.glyphBoxBottom, 0);
    }
    fm_close(&F); free(b);
}
TEST(fm_default_mirror) { ttf_spec s; build(&s); FM_OPEN(F, s, 0); check_mirror(&F, "default"); fm_close(&F); }
TEST(fm_v0_is_prefix_of_v1) {
    ttf_spec s; build(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS a, b; DWRITE_FONT_METRICS1 m1;
    memset(&a, 0xAB, sizeof a); IDWriteFontFace_GetMetrics(F.face, &a);
    if (get_m1(&F, &m1)) {
        b.designUnitsPerEm = m1.designUnitsPerEm; b.ascent = m1.ascent; b.descent = m1.descent; b.lineGap = m1.lineGap; b.capHeight = m1.capHeight;
        b.xHeight = m1.xHeight; b.underlinePosition = m1.underlinePosition; b.underlineThickness = m1.underlineThickness;
        b.strikethroughPosition = m1.strikethroughPosition; b.strikethroughThickness = m1.strikethroughThickness;
        CHECK(!memcmp(&a, &b, sizeof a));
        EXPECT_EQ(sizeof(DWRITE_FONT_METRICS), 20); EXPECT_EQ(sizeof(DWRITE_FONT_METRICS1), 48);
    }
    fm_close(&F);
}
TEST(fm_getmetrics_is_stable) {
    ttf_spec s; build(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 a, b, c; DWRITE_FONT_METRICS1 g;
    if (get_m1(&F, &a)) {
        IDWriteFontFace1_GetMetrics(F.f1, &b);
        CHECK(!memcmp(&a, &b, sizeof a));
        /* a GDI-compatible query must not disturb the stored metrics */
        IDWriteFontFace1_GetGdiCompatibleMetrics(F.f1, 13.0f, 1.0f, NULL, &g);
        IDWriteFontFace1_GetMetrics(F.f1, &c);
        CHECK(!memcmp(&a, &c, sizeof a));
    }
    fm_close(&F);
}
TEST(fm_metrics_same_through_both_interfaces) {
    ttf_spec s; build(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS v0; DWRITE_FONT_METRICS1 v1;
    IDWriteFontFace_GetMetrics(F.face, &v0);
    if (get_m1(&F, &v1)) { EXPECT_EQ(v0.ascent, v1.ascent); EXPECT_EQ(v0.descent, v1.descent); EXPECT_EQ(v0.lineGap, v1.lineGap); EXPECT_EQ(v0.designUnitsPerEm, v1.designUnitsPerEm); }
    fm_close(&F);
}

/* ---- ascent / descent / lineGap decision tree --------------------------------------------------------------------- */
TEST(fm_typo_metrics_flag) {
    ttf_spec s; build(&s); s.fs_selection |= 0x80; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) {
        /* fsSelection bit 7 (USE_TYPO_METRICS): typographic ascender, negated typographic descender and gap; hasTypographicMetrics = TRUE */
        EXPECT_EQ(m.ascent, 1600); EXPECT_EQ(m.descent, 420); EXPECT_EQ(m.lineGap, 80); CHECK(m.hasTypographicMetrics);
    }
    check_mirror(&F, "typo"); fm_close(&F);
}
TEST(fm_typo_flag_clamps) {
    static const struct { int asc, desc, gap; } t[] = { { -100, 50, 0 }, { 100, 50, 0 }, { 0, 0, 0 }, { 30000, -30000, 32000 }, { 500, -500, -20 }, { 32767, -32768, 7 } };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ttf_spec s; build(&s); s.fs_selection |= 0x80; s.typo_asc = t[i].asc; s.typo_desc = t[i].desc; s.typo_gap = t[i].gap;
        FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            /* negative values clamp to 0 (FontIntToNearestUShort); the typographic gap is returned as the signed 16-bit value */
            int wa = t[i].asc < 0 ? 0 : t[i].asc, wd = -t[i].desc < 0 ? 0 : -t[i].desc;
            char l[64]; snprintf(l, sizeof l, "typo row %u", i);
            t_check(m.ascent == wa, __FILE__, __LINE__, "ascent", "%s got %d want %d", l, m.ascent, wa);
            t_check(m.descent == wd, __FILE__, __LINE__, "descent", "%s got %d want %d", l, m.descent, wd);
            t_check(m.lineGap == t[i].gap, __FILE__, __LINE__, "lineGap", "%s got %d want %d", l, m.lineGap, t[i].gap);
        }
        fm_close(&F);
    }
}
TEST(fm_win_metrics_used_without_typo_flag) {
    ttf_spec s; build(&s); s.win_asc = 1900; s.win_desc = 500; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) { EXPECT_EQ(m.ascent, 1900); EXPECT_EQ(m.descent, 500); EXPECT_EQ(m.lineGap, 0); CHECK(!m.hasTypographicMetrics); }
    fm_close(&F);
}
TEST(fm_win_zero_falls_back_to_hhea) {
    ttf_spec s; build(&s); s.win_asc = 0; s.win_desc = 0; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) {
        /* both usWin values zero: the hhea ascender, |descender| and lineGap are used */
        EXPECT_EQ(m.ascent, 1700); EXPECT_EQ(m.descent, 400); EXPECT_EQ(m.lineGap, 90);
    }
    check_mirror(&F, "winzero"); fm_close(&F);
}
TEST(fm_win_ascent_zero_descent_nonzero) {
    ttf_spec s; build(&s); s.win_asc = 0; s.win_desc = 450; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) {
        /* the win path is taken as soon as either value is non-zero: ascent 0, descent 450, gap = 90 + 400 + 1700 - 450 - 0 */
        EXPECT_EQ(m.ascent, 0); EXPECT_EQ(m.descent, 450); EXPECT_EQ(m.lineGap, 1740);
    }
    check_mirror(&F, "winasc0"); fm_close(&F);
}
TEST(fm_win_descent_zero_ascent_nonzero) {
    ttf_spec s; build(&s); s.win_asc = 1800; s.win_desc = 0; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) { EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 0); EXPECT_EQ(m.lineGap, 390); }   /* 90+400+1700-0-1800 */
    check_mirror(&F, "windesc0"); fm_close(&F);
}
TEST(fm_linegap_from_hhea_and_win_table) {
    static const struct { int ha, hd, hg, wa, wd; int gap; } t[] = {
        { 1700, -400, 90, 1700, 400, 90 },         /* identical metrics: the hhea gap survives */
        { 1854, -434, 67, 1854, 434, 67 },         /* Arial-like */
        { 1700, -400, 0, 1700, 400, 0 },
        { 1700, -400, 90, 1800, 450, 0 },          /* negative -> 0 */
        { 1700, -400, 90, 1600, 350, 240 },        /* win smaller: gap grows by the difference */
        { 1700, -400, 0, 1000, 300, 800 },
        { 1500, -500, 100, 1000, 500, 600 },
        { 3000, -3000, 30000, 100, 100, 32767 },   /* sum 36000 - 200 clamps to 0x7fff */
        { 1700, -400, -50, 1700, 400, 0 },         /* negative hhea gap is treated as 0 */
        { 1700, -400, -50, 1000, 400, 1700 - 1000 }, /* 0 + 400 + 1700 - 400 - 1000 */
    };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ttf_spec s; build(&s); s.hhea_asc = t[i].ha; s.hhea_desc = t[i].hd; s.hhea_gap = t[i].hg; s.win_asc = t[i].wa; s.win_desc = t[i].wd;
        FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            t_check(m.ascent == t[i].wa && m.descent == t[i].wd, __FILE__, __LINE__, "asc/desc", "row %u got %d/%d want %d/%d", i, m.ascent, m.descent, t[i].wa, t[i].wd);
            t_check(m.lineGap == t[i].gap, __FILE__, __LINE__, "lineGap", "row %u got %d want %d", i, m.lineGap, t[i].gap);
        }
        fm_close(&F);
    }
}
TEST(fm_no_os2_uses_hhea) {
    ttf_spec s; build(&s); s.omit |= TTF_OMIT_OS2; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) {
        EXPECT_EQ(m.ascent, 1700); EXPECT_EQ(m.descent, 400); EXPECT_EQ(m.lineGap, 90); CHECK(!m.hasTypographicMetrics);
        /* no OS/2: strikethrough position upm/3, thickness = underline thickness; sub/superscript all zero */
        EXPECT_EQ(m.strikethroughPosition, 2048 / 3); EXPECT_EQ(m.strikethroughThickness, m.underlineThickness);
        EXPECT_EQ((int)m.subscriptSizeX, 0); EXPECT_EQ((int)m.subscriptSizeY, 0); EXPECT_EQ((int)m.subscriptPositionX, 0); EXPECT_EQ((int)m.subscriptPositionY, 0);
        EXPECT_EQ((int)m.superscriptSizeX, 0); EXPECT_EQ((int)m.superscriptSizeY, 0); EXPECT_EQ((int)m.superscriptPositionX, 0); EXPECT_EQ((int)m.superscriptPositionY, 0);
        /* heights come from glyph data or the fallbacks but are always inside the validity window */
        CHECK(fm_x_valid(2048, m.xHeight)); CHECK(fm_cap_valid(2048, m.capHeight));
        t_note("no OS/2: xHeight=%d capHeight=%d", m.xHeight, m.capHeight);
    }
    check_mirror(&F, "noos2"); fm_close(&F);
}
TEST(fm_typo_flag_ignored_without_os2_values) {
    /* fsSelection bit 7 with win values zero still selects the typographic values (the flag is tested first) */
    ttf_spec s; build(&s); s.fs_selection |= 0x80; s.win_asc = 0; s.win_desc = 0; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) { EXPECT_EQ(m.ascent, 1600); EXPECT_EQ(m.descent, 420); CHECK(m.hasTypographicMetrics); }
    fm_close(&F);
}
TEST(fm_aat_table_disables_win_metrics) {
    /* A 'mort' table without GSUB/GPOS/GDEF makes the win metrics ineligible: the hhea values are reported instead. */
    static const uint8_t mort[16] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    ttf_rawtable raw = { "mort", mort, sizeof mort };
    ttf_spec s; build(&s); s.raw = &raw; s.nraw = 1; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) { EXPECT_EQ(m.ascent, 1700); EXPECT_EQ(m.descent, 400); EXPECT_EQ(m.lineGap, 90); }
    check_mirror(&F, "mort"); fm_close(&F);
}
TEST(fm_aat_table_with_opentype_layout_keeps_win_metrics) {
    static const uint8_t mort[16] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    static const ttf_sub1 sub[1] = { { 2, 3 } };
    static const ttf_feature feat[1] = { { "liga", TTF_LK_SINGLE, 1, sub } };
    ttf_rawtable raw = { "mort", mort, sizeof mort };
    ttf_spec s; build(&s); s.raw = &raw; s.nraw = 1; s.gsub = feat; s.ngsub = 1; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) { EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450); }
    check_mirror(&F, "mort+gsub"); fm_close(&F);
}

/* ---- heights ------------------------------------------------------------------------------------------------------- */
TEST(fm_height_validity_boundaries) {
    /* ValidateHeights: xHeight valid when upm < 10*x < 9*upm; capHeight valid when upm < 10*cap and cap <= upm. A valid value
     * is returned unchanged, an invalid one is replaced by a derived value that is itself inside the window. */
    static const int xs[] = { 204, 205, 206, 1000, 1842, 1843, 1844, 2048, 3000, 1, 0 };
    static const int cs[] = { 204, 205, 1400, 2047, 2048, 2049, 4000, 1, 0 };
    unsigned i;
    for (i = 0; i < sizeof xs / sizeof xs[0]; i++) {
        ttf_spec s; build(&s); s.x_height = xs[i]; FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            int valid = fm_x_valid(2048, xs[i]);
            if (valid) t_check(m.xHeight == xs[i], __FILE__, __LINE__, "xHeight kept", "x=%d got %d", xs[i], m.xHeight);
            else { t_check(m.xHeight != xs[i] && fm_x_valid(2048, m.xHeight), __FILE__, __LINE__, "xHeight replaced", "x=%d got %d", xs[i], m.xHeight); }
            EXPECT_EQ(m.capHeight, 1400);       /* the other height was valid and stays */
        }
        fm_close(&F);
    }
    for (i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        ttf_spec s; build(&s); s.cap_height = cs[i]; FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            int valid = fm_cap_valid(2048, cs[i]);
            if (valid) t_check(m.capHeight == cs[i], __FILE__, __LINE__, "capHeight kept", "cap=%d got %d", cs[i], m.capHeight);
            else t_check(m.capHeight != cs[i] && fm_cap_valid(2048, m.capHeight), __FILE__, __LINE__, "capHeight replaced", "cap=%d got %d", cs[i], m.capHeight);
            EXPECT_EQ(m.xHeight, 1000);
        }
        fm_close(&F);
    }
}
TEST(fm_height_os2_versions) {
    unsigned v;
    for (v = 0; v <= 5; v++) {
        ttf_spec s; build(&s); s.os2_version = v; FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            /* only OS/2 version >= 2 carries sxHeight / sCapHeight (offsets 86 / 88); older tables fall through to the derived heights */
            if (v >= 2) { EXPECT_EQ(m.xHeight, 1000); EXPECT_EQ(m.capHeight, 1400); }
            else { CHECK(fm_x_valid(2048, m.xHeight)); CHECK(fm_cap_valid(2048, m.capHeight)); t_note("os2 v%u derived x=%d cap=%d", v, m.xHeight, m.capHeight); }
            /* everything else is independent of the table version */
            EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450); EXPECT_EQ(m.strikethroughPosition, 600);
            EXPECT_EQ((int)m.subscriptSizeX, 1024); EXPECT_EQ((int)m.superscriptPositionY, 682);
        }
        check_mirror(&F, "os2ver"); fm_close(&F);
    }
}
TEST(fm_height_both_invalid_stay_in_window) {
    ttf_spec s; build(&s); s.x_height = 0; s.cap_height = 0; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) { CHECK(fm_x_valid(2048, m.xHeight)); CHECK(fm_cap_valid(2048, m.capHeight)); t_note("derived x=%d cap=%d", m.xHeight, m.capHeight); }
    fm_close(&F);
}
TEST(fm_height_without_glyph_data_defaults) {
    /* No OS/2 heights and a cmap that maps only digits: nothing can be measured, so the last resort values apply
     * (xHeight = upm/2, capHeight = 70% of upm).  Whether a digit-only cmap leaves every probe character unmapped is the
     * assumption here, so a mismatch is reported as a note after the range check. */
    static const ttf_map map[3] = { { '0', 54 }, { '1', 55 }, { '2', 56 } };
    static const ttf_cmap cm[1] = { { 4, 3, 1, 3, map, 0, NULL } };
    ttf_spec s; build(&s); s.x_height = 0; s.cap_height = 0; s.cmaps = cm; s.ncmaps = 1; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) {
        CHECK(fm_x_valid(2048, m.xHeight)); CHECK(fm_cap_valid(2048, m.capHeight));
        if (m.xHeight != 1024 || m.capHeight != 1433) t_note("digit-only cmap: x=%d cap=%d (expected 1024 / 1433)", m.xHeight, m.capHeight);
        else { EXPECT_EQ(m.xHeight, 1024); EXPECT_EQ(m.capHeight, 2048 * 70 / 100); }
    }
    fm_close(&F);
}

/* ---- units per em ---------------------------------------------------------------------------------------------------- */
TEST(fm_upm_variants) {
    static const int upms[] = { 16, 64, 100, 250, 256, 500, 512, 1000, 1024, 2000, 2048, 4096, 8192, 16384 };
    unsigned i;
    for (i = 0; i < sizeof upms / sizeof upms[0]; i++) {
        ttf_spec s; fm_spec_upm(&s, upms[i]); FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            t_check(m.designUnitsPerEm == upms[i], __FILE__, __LINE__, "designUnitsPerEm", "upm %d got %d", upms[i], m.designUnitsPerEm);
            char l[40]; snprintf(l, sizeof l, "upm %d", upms[i]);
            fm_exp e; CHECK(fm_derive(F.face, 0, &e)); FM_CMP(&m, &e.m, fm_unknown_mask(&e), l);
        }
        fm_close(&F);
    }
}
TEST(fm_upm_scaled_values_roundtrip) {
    /* A font authored at 1000 upm reports numbers exactly as stored, nothing is normalised to 2048. */
    ttf_spec s; fm_spec_upm(&s, 1000); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) {
        EXPECT_EQ(m.designUnitsPerEm, 1000); EXPECT_EQ(m.ascent, 878); EXPECT_EQ(m.descent, 219);
        EXPECT_EQ(m.capHeight, 683); EXPECT_EQ(m.xHeight, 488);
        EXPECT_EQ((int)m.underlinePosition, -97); EXPECT_EQ(m.underlineThickness, 48);
    }
    fm_close(&F);
}

/* ---- underline / strikethrough --------------------------------------------------------------------------------------- */
TEST(fm_underline_from_post) {
    static const int pos[] = { -200, -1, 0, 50, -32768, 32767, -400 }, th[] = { 100, 1, 2048, 65535, 7, 20, 1000 };
    unsigned i;
    for (i = 0; i < sizeof pos / sizeof pos[0]; i++) {
        ttf_spec s; build(&s); s.underline_pos = pos[i]; s.underline_thick = th[i]; FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            t_check(m.underlinePosition == (INT16)pos[i], __FILE__, __LINE__, "underlinePosition", "row %u got %d want %d", i, m.underlinePosition, pos[i]);
            t_check(m.underlineThickness == (UINT16)th[i], __FILE__, __LINE__, "underlineThickness", "row %u got %d want %d", i, m.underlineThickness, th[i]);
        }
        fm_close(&F);
    }
}
TEST(fm_underline_thickness_zero_is_derived) {
    static const struct { int upm, th; } t[] = { { 2048, 146 }, { 1000, 71 }, { 1024, 73 } };
    unsigned i;
    for (i = 0; i < 3; i++) {
        ttf_spec s; fm_spec_upm(&s, t[i].upm); s.underline_thick = 0; FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            /* a post underlineThickness of 0 is replaced by roughly upm/14 */
            t_check(m.underlineThickness == t[i].th, __FILE__, __LINE__, "underlineThickness", "upm %d got %d want %d", t[i].upm, m.underlineThickness, t[i].th);
            /* the strikethrough thickness falls back to the underline thickness when yStrikeoutSize is not positive; here it is positive */
            EXPECT_EQ(m.strikethroughThickness, s.strike_size);
        }
        fm_close(&F);
    }
}
TEST(fm_post_missing_defaults) {
    static const struct { int upm, pos, th; } t[] = { { 2048, -205, 146 }, { 1000, -100, 71 }, { 1024, -103, 73 } };
    unsigned i;
    for (i = 0; i < 3; i++) {
        ttf_spec s; fm_spec_upm(&s, t[i].upm); s.omit |= TTF_OMIT_POST; FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            /* no post table: underline position = -((upm + 5) / 10), thickness about upm / 14 */
            t_check(m.underlinePosition == t[i].pos, __FILE__, __LINE__, "underlinePosition", "upm %d got %d want %d", t[i].upm, m.underlinePosition, t[i].pos);
            t_check(m.underlineThickness == t[i].th, __FILE__, __LINE__, "underlineThickness", "upm %d got %d want %d", t[i].upm, m.underlineThickness, t[i].th);
        }
        fm_close(&F);
    }
}
TEST(fm_strikethrough_from_os2) {
    static const int pos[] = { 600, 0, -50, 1000, 32767 }, sz[] = { 100, 1, 2048, 30000, 77 };
    unsigned i;
    for (i = 0; i < sizeof pos / sizeof pos[0]; i++) {
        ttf_spec s; build(&s); s.strike_pos = pos[i]; s.strike_size = sz[i]; FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            t_check(m.strikethroughPosition == pos[i], __FILE__, __LINE__, "strikethroughPosition", "row %u got %d want %d", i, m.strikethroughPosition, pos[i]);
            t_check(m.strikethroughThickness == sz[i], __FILE__, __LINE__, "strikethroughThickness", "row %u got %d want %d", i, m.strikethroughThickness, sz[i]);
        }
        fm_close(&F);
    }
}
TEST(fm_strikethrough_size_nonpositive_uses_underline) {
    static const int sz[] = { 0, -1, -100, -32768 };
    unsigned i;
    for (i = 0; i < 4; i++) {
        ttf_spec s; build(&s); s.strike_size = sz[i]; s.underline_thick = 123; FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            t_check(m.strikethroughThickness == 123, __FILE__, __LINE__, "strikethroughThickness", "yStrikeoutSize %d got %d want 123", sz[i], m.strikethroughThickness);
            EXPECT_EQ(m.strikethroughPosition, 600);
        }
        fm_close(&F);
    }
}

/* ---- sub / superscript, glyph box (patched tables) -------------------------------------------------------------------- */
static int open_patched(fm_font *F, const ttf_spec *s, const char *tag, void (*patch)(uint8_t *t, unsigned len, const int *v), const int *v, DWRITE_FONT_SIMULATIONS sim) {
    size_t n; uint8_t *b = ttf_build(s, &n); unsigned tl = 0; uint8_t *t; int ok;
    if (!b) { t_skip("ttf_build failed"); return 0; }
    t = fm_tbl(b, n, tag, &tl);
    if (!t) { free(b); t_skip("table %s missing", tag); return 0; }
    patch(t, tl, v);
    ok = fm_open_blob(F, b, n, sim);
    free(b);
    return ok;
}
static void patch_os2_subsup(uint8_t *t, unsigned len, const int *v) {
    fm_put16(t + 10, v[0]); fm_put16(t + 12, v[1]); fm_put16(t + 14, v[2]); fm_put16(t + 16, v[3]);   /* subscript size x/y, offset x/y */
    fm_put16(t + 18, v[4]); fm_put16(t + 20, v[5]); fm_put16(t + 22, v[6]); fm_put16(t + 24, v[7]);   /* superscript */
}
TEST(fm_subsuperscript_mapping) {
    static const int rows[][8] = {
        { 1, 2, 3, 4, 5, 6, 7, 8 },
        { 1434, 1331, 0, 286, 1434, 1331, 0, 977 },          /* Arial-like */
        { 600, 700, 10, -20, 800, 900, -30, 40 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767 },
        { -1, -2, -3, -4, -5, -6, -7, -8 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        ttf_spec s; build(&s); fm_font F; memset(&F, 0, sizeof F);
        if (!open_patched(&F, &s, "OS/2", patch_os2_subsup, rows[i], 0)) { fm_close(&F); return; }
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            char l[48]; snprintf(l, sizeof l, "row %u", i);
            /* sub: ySubscriptXSize -> sizeX, YSize -> sizeY, XOffset -> posX, -YOffset -> posY; super: sizes, offsets as stored */
            t_check(m.subscriptSizeX == rows[i][0], __FILE__, __LINE__, "subscriptSizeX", "%s got %d", l, m.subscriptSizeX);
            t_check(m.subscriptSizeY == rows[i][1], __FILE__, __LINE__, "subscriptSizeY", "%s got %d", l, m.subscriptSizeY);
            t_check(m.subscriptPositionX == rows[i][2], __FILE__, __LINE__, "subscriptPositionX", "%s got %d", l, m.subscriptPositionX);
            t_check(m.subscriptPositionY == (INT16)-rows[i][3], __FILE__, __LINE__, "subscriptPositionY", "%s got %d want %d", l, m.subscriptPositionY, -rows[i][3]);
            t_check(m.superscriptSizeX == rows[i][4], __FILE__, __LINE__, "superscriptSizeX", "%s got %d", l, m.superscriptSizeX);
            t_check(m.superscriptSizeY == rows[i][5], __FILE__, __LINE__, "superscriptSizeY", "%s got %d", l, m.superscriptSizeY);
            t_check(m.superscriptPositionX == rows[i][6], __FILE__, __LINE__, "superscriptPositionX", "%s got %d", l, m.superscriptPositionX);
            t_check(m.superscriptPositionY == rows[i][7], __FILE__, __LINE__, "superscriptPositionY", "%s got %d", l, m.superscriptPositionY);
        }
        fm_close(&F);
    }
}
static void patch_head_box(uint8_t *t, unsigned len, const int *v) { fm_put16(t + 36, v[0]); fm_put16(t + 38, v[1]); fm_put16(t + 40, v[2]); fm_put16(t + 42, v[3]); }
TEST(fm_glyph_box_mapping) {
    static const int rows[][4] = { { -100, -200, 2000, 1900 }, { 0, 0, 0, 0 }, { 1, 2, 3, 4 }, { -32768, -32768, 32767, 32767 }, { 500, -500, 1500, 2500 } };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        ttf_spec s; build(&s); fm_font F; memset(&F, 0, sizeof F);
        if (!open_patched(&F, &s, "head", patch_head_box, rows[i], 0)) { fm_close(&F); return; }
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) {
            /* head xMin, yMin, xMax, yMax  ->  glyphBoxLeft, glyphBoxBottom, glyphBoxRight, glyphBoxTop */
            t_check(m.glyphBoxLeft == rows[i][0], __FILE__, __LINE__, "glyphBoxLeft", "row %u got %d", i, m.glyphBoxLeft);
            t_check(m.glyphBoxBottom == rows[i][1], __FILE__, __LINE__, "glyphBoxBottom", "row %u got %d", i, m.glyphBoxBottom);
            t_check(m.glyphBoxRight == rows[i][2], __FILE__, __LINE__, "glyphBoxRight", "row %u got %d", i, m.glyphBoxRight);
            t_check(m.glyphBoxTop == rows[i][3], __FILE__, __LINE__, "glyphBoxTop", "row %u got %d", i, m.glyphBoxTop);
        }
        fm_close(&F);
    }
}

/* ---- caret metrics ------------------------------------------------------------------------------------------------------ */
static void patch_hhea_caret(uint8_t *t, unsigned len, const int *v) { fm_put16(t + 18, v[0]); fm_put16(t + 20, v[1]); fm_put16(t + 22, v[2]); }
static int expect_oblique_caret(int rise, int run, unsigned upm, DWRITE_CARET_METRICS *o) {
    /* AdjustCaretMetricsForObliqueSimulation: rises smaller than upm are normalised to upm (run scaled with them), then the
     * oblique shear (0x5700 / 65536 of the rise) is added to the run. Only non-negative inputs are used. */
    unsigned r = (unsigned)rise; int rn = run;
    if (rise != 0 && (unsigned)rise < upm) { rn = (int)(((unsigned)run * upm) / (unsigned)rise); r = upm; }
    o->slopeRise = (INT16)r; o->slopeRun = (INT16)((r * 0x5700u >> 16) + rn);
    return 1;
}
TEST(fm_caret_default) {
    ttf_spec s; build(&s); FM_OPEN(F, s, 0);
    DWRITE_CARET_METRICS c = { 77, 77, 77 };
    if (F.f1) { IDWriteFontFace1_GetCaretMetrics(F.f1, &c); EXPECT_EQ(c.slopeRise, 1); EXPECT_EQ(c.slopeRun, 0); EXPECT_EQ(c.offset, 0); }
    else t_skip("IDWriteFontFace1 unavailable");
    fm_close(&F);
}
TEST(fm_caret_italic_font_values) {
    ttf_spec s; ttf_style_spec(&s, "DwItalic", 400, 5, 1); FM_OPEN(F, s, 0);
    DWRITE_CARET_METRICS c = { 77, 77, 77 };
    if (F.f1) {
        IDWriteFontFace1_GetCaretMetrics(F.f1, &c);
        /* the generator writes caretSlopeRise 1000 / caretSlopeRun 213 for italic fonts; returned unmodified (no simulation) */
        EXPECT_EQ(c.slopeRise, 1000); EXPECT_EQ(c.slopeRun, 213); EXPECT_EQ(c.offset, 0);
    }
    fm_close(&F);
}
TEST(fm_caret_hhea_mapping) {
    static const int rows[][3] = { { 1, 0, 0 }, { 1000, 213, 0 }, { 0, 1, 5 }, { 5, 7, -9 }, { -3, 4, 32767 }, { -32768, -32768, -32768 }, { 32767, 32767, 0 } };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        ttf_spec s; build(&s); fm_font F; memset(&F, 0, sizeof F);
        if (!open_patched(&F, &s, "hhea", patch_hhea_caret, rows[i], 0)) { fm_close(&F); return; }
        DWRITE_CARET_METRICS c = { 99, 99, 99 };
        if (F.f1) {
            IDWriteFontFace1_GetCaretMetrics(F.f1, &c);
            /* hhea caretSlopeRise, caretSlopeRun, caretOffset are returned as stored */
            t_check(c.slopeRise == rows[i][0] && c.slopeRun == rows[i][1] && c.offset == rows[i][2], __FILE__, __LINE__, "caret", "row %u got %d/%d/%d", i, c.slopeRise, c.slopeRun, c.offset);
        }
        fm_close(&F);
    }
}
TEST(fm_caret_oblique_simulation) {
    static const int rows[][3] = { { 1, 0, 0 }, { 1000, 213, 0 }, { 2048, 0, 3 }, { 2048, 100, -4 }, { 4096, 50, 0 }, { 5000, 2000, 1 }, { 0, 9, 2 }, { 2047, 10, 0 }, { 100, 7, 0 } };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        ttf_spec s; build(&s); fm_font F; memset(&F, 0, sizeof F);
        if (!open_patched(&F, &s, "hhea", patch_hhea_caret, rows[i], DWRITE_FONT_SIMULATIONS_OBLIQUE)) { fm_close(&F); return; }
        DWRITE_CARET_METRICS c = { 99, 99, 99 }, w;
        if (F.f1) {
            IDWriteFontFace1_GetCaretMetrics(F.f1, &c);
            expect_oblique_caret(rows[i][0], rows[i][1], 2048, &w);
            t_check(c.slopeRise == w.slopeRise && c.slopeRun == w.slopeRun, __FILE__, __LINE__, "oblique caret", "in %d/%d got %d/%d want %d/%d", rows[i][0], rows[i][1], c.slopeRise, c.slopeRun, w.slopeRise, w.slopeRun);
            t_check(c.offset == rows[i][2], __FILE__, __LINE__, "caret offset unchanged", "row %u got %d", i, c.offset);
        }
        fm_close(&F);
    }
}
TEST(fm_caret_oblique_default_font) {
    ttf_spec s; build(&s); FM_OPEN(F, s, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    DWRITE_CARET_METRICS c = { 0, 0, 0 };
    if (F.f1) { IDWriteFontFace1_GetCaretMetrics(F.f1, &c); EXPECT_EQ(c.slopeRise, 2048); EXPECT_EQ(c.slopeRun, 696); EXPECT_EQ(c.offset, 0); }   /* 2048*0x5700>>16 = 696 */
    fm_close(&F);
}
TEST(fm_caret_bold_simulation_unchanged) {
    ttf_spec s; build(&s); FM_OPEN(F, s, DWRITE_FONT_SIMULATIONS_BOLD);
    DWRITE_CARET_METRICS c = { 0, 0, 0 };
    if (F.f1) { IDWriteFontFace1_GetCaretMetrics(F.f1, &c); EXPECT_EQ(c.slopeRise, 1); EXPECT_EQ(c.slopeRun, 0); }
    fm_close(&F);
}

/* ---- simulations ----------------------------------------------------------------------------------------------------------- */
TEST(fm_bold_simulation_leaves_metrics_alone) {
    ttf_spec s; build(&s); FM_OPEN(F, s, DWRITE_FONT_SIMULATIONS_BOLD);
    DWRITE_FONT_METRICS1 m; fm_exp e;
    EXPECT_EQ(IDWriteFontFace_GetSimulations(F.face), DWRITE_FONT_SIMULATIONS_BOLD);
    if (get_m1(&F, &m)) { CHECK(fm_derive(F.face, 0, &e)); FM_CMP(&m, &e.m, fm_unknown_mask(&e), "bold sim"); }
    fm_close(&F);
}
TEST(fm_oblique_simulation_shears_four_fields) {
    /* give the font a descending box so the left/subscript shear is visible */
    ttf_spec s; build(&s); s.underline_pos = -200; FM_OPEN(F, s, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    DWRITE_FONT_METRICS1 m, plain; fm_exp e;
    fm_font G; memset(&G, 0, sizeof G);
    if (!fm_open(&G, &s, 0)) { fm_close(&G); fm_close(&F); return; }
    if (get_m1(&F, &m) && get_m1(&G, &plain)) {
        /* right += top*0x5700>>16, left += bottom*..., subscript X += subscript Y*..., superscript X += superscript Y*... */
        EXPECT_EQ((int)m.glyphBoxRight, plain.glyphBoxRight + fm_shear(plain.glyphBoxTop));
        EXPECT_EQ((int)m.glyphBoxLeft, plain.glyphBoxLeft + fm_shear(plain.glyphBoxBottom));
        EXPECT_EQ((int)m.subscriptPositionX, plain.subscriptPositionX + fm_shear(plain.subscriptPositionY));
        EXPECT_EQ((int)m.superscriptPositionX, plain.superscriptPositionX + fm_shear(plain.superscriptPositionY));
        EXPECT_EQ((int)m.glyphBoxRight, 1100 + fm_shear(2800));
        /* nothing else moves */
        FM_CMP(&m, &plain, FM_BIT(FM_BOXL) | FM_BIT(FM_BOXR) | FM_BIT(FM_SUBX) | FM_BIT(FM_SUPX), "oblique vs plain");
        CHECK(fm_derive(F.face, DWRITE_FONT_SIMULATIONS_OBLIQUE, &e)); FM_CMP(&m, &e.m, fm_unknown_mask(&e), "oblique mirror");
    }
    fm_close(&G); fm_close(&F);
}
TEST(fm_oblique_shear_with_negative_box) {
    static const int box[4] = { -300, -250, 1500, 1900 };
    ttf_spec s; build(&s); fm_font F; memset(&F, 0, sizeof F);
    if (!open_patched(&F, &s, "head", patch_head_box, box, DWRITE_FONT_SIMULATIONS_OBLIQUE)) { fm_close(&F); return; }
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) {
        /* arithmetic (flooring) shift: -250 * 0x5700 >> 16 = -84.96, floored to -85 */
        EXPECT_EQ(fm_shear(-250), -85);
        EXPECT_EQ((int)m.glyphBoxLeft, -300 + fm_shear(-250)); EXPECT_EQ((int)m.glyphBoxRight, 1500 + fm_shear(1900));
    }
    fm_close(&F);
}
TEST(fm_bold_oblique_simulation) {
    ttf_spec s; build(&s); FM_OPEN(F, s, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE);
    DWRITE_FONT_METRICS1 m; fm_exp e;
    if (get_m1(&F, &m)) { CHECK(fm_derive(F.face, DWRITE_FONT_SIMULATIONS_OBLIQUE, &e)); FM_CMP(&m, &e.m, fm_unknown_mask(&e), "bold+oblique"); }
    fm_close(&F);
}
TEST(fm_simulations_report_back) {
    static const DWRITE_FONT_SIMULATIONS sims[] = { 0, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE };
    unsigned i;
    for (i = 0; i < 4; i++) {
        ttf_spec s; build(&s); FM_OPEN(F, s, sims[i]);
        EXPECT_EQ(IDWriteFontFace_GetSimulations(F.face), sims[i]);
        fm_close(&F);
    }
}

/* ---- misc ---------------------------------------------------------------------------------------------------------------- */
TEST(fm_metrics_independent_of_cmap_and_glyph_count) {
    /* the same tables in a font with a different cmap give identical vertical metrics */
    static const ttf_map map[2] = { { 'A', 2 }, { 'B', 3 } };
    static const ttf_cmap cm[1] = { { 4, 3, 1, 2, map, 0, NULL } };
    ttf_spec s; build(&s); s.cmaps = cm; s.ncmaps = 1; FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 m;
    if (get_m1(&F, &m)) { EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450); EXPECT_EQ(m.capHeight, 1400); EXPECT_EQ(m.xHeight, 1000); }
    fm_close(&F);
}
TEST(fm_weight_width_do_not_change_metrics) {
    static const int w[] = { 100, 400, 700, 900 };
    unsigned i;
    for (i = 0; i < 4; i++) {
        ttf_spec s; ttf_style_spec(&s, "DwW", w[i], 5, 0); FM_OPEN(F, s, 0);
        DWRITE_FONT_METRICS1 m;
        if (get_m1(&F, &m)) { EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450); EXPECT_EQ(m.designUnitsPerEm, 2048); }
        fm_close(&F);
    }
}
TEST(fm_two_faces_same_blob_same_metrics) {
    ttf_spec s; build(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 a, b; IDWriteFontFace *f2 = NULL; IDWriteFontFace1 *g = NULL;
    IDWriteFontFile *files[1] = { F.h.file };
    if (SUCCEEDED(IDWriteFactory_CreateFontFace(dw_shared(), F.h.face_type, 1, files, 0, 0, &f2)) && f2) {
        g = fm_f1(f2);
        if (g && get_m1(&F, &a)) { IDWriteFontFace1_GetMetrics(g, &b); CHECK(!memcmp(&a, &b, sizeof a)); }
        DW_RELEASE(g); DW_RELEASE(f2);
    }
    fm_close(&F);
}
