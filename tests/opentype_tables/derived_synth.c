/* Facts DirectWrite derives from the raw tables, checked on synthetic fonts with exactly known tables.
 * Rules come from the dwrite.dll metrics initialisation (head/hhea/OS/2/post fields copied or combined) and the face methods
 * GetGlyphIndices, GetDesignGlyphMetrics/Advances, GetUnicodeRanges, GetCaretMetrics, GetKerningPairAdjustments. */
#include "ot.h"

static IDWriteFontFace1 *face1(synth *s) {
    IDWriteFontFace1 *f1 = NULL;
    if (FAILED(IDWriteFontFace_QueryInterface(s->h.face, &IID_IDWriteFontFace1, (void **)&f1))) return NULL;
    return f1;
}
/* Open the spec, read DWRITE_FONT_METRICS1 and return the INT16/UINT16 field at byte offset `off`. */
static int met_field(const ttf_spec *sp, size_t off, int *out) {
    synth s; IDWriteFontFace1 *f1; DWRITE_FONT_METRICS1 m; int ok = 0;
    memset(&s, 0, sizeof s);
    if (!synth_open(&s, sp)) { t_skip("open failed %s", dw_hr_name(s.hr)); synth_close(&s); return 0; }
    f1 = face1(&s);
    if (f1) { IDWriteFontFace1_GetMetrics(f1, &m); *out = (int)*(INT16 *)((char *)&m + off); ok = 1; IDWriteFontFace1_Release(f1); }
    else t_skip("IDWriteFontFace1 unsupported");
    synth_close(&s);
    return ok;
}
#define MF(id, setup, field, expr) TEST(dm_##id) { ttf_spec sp; int got; ttf_default_spec(&sp); setup; \
    if (met_field(&sp, offsetof(DWRITE_FONT_METRICS1, field), &got)) CHECKF(got == (int)(expr), #id ": got %d want %d", got, (int)(expr)); }
#define NOP (void)0
#define USE_TYPO sp.fs_selection = 0xC0

/* ---- design units / glyph box (head) ---- */
MF(upm_2048, NOP, designUnitsPerEm, 2048)
MF(upm_1000, sp.upm = 1000, designUnitsPerEm, 1000)
MF(upm_1024, sp.upm = 1024, designUnitsPerEm, 1024)
MF(upm_4096, sp.upm = 4096, designUnitsPerEm, 4096)
MF(upm_16384, sp.upm = 16384, designUnitsPerEm, 16384)
MF(box_left_is_head_xmin, NOP, glyphBoxLeft, 100)
MF(box_top_is_head_ymax, NOP, glyphBoxTop, 2800)   /* composite glyph: A plus a triangle raised by 1400 */
MF(box_right_is_head_xmax, NOP, glyphBoxRight, 1100)
MF(box_bottom_is_head_ymin, NOP, glyphBoxBottom, 0)

/* ---- ascent / descent / lineGap ---- */
/* DirectWrite uses usWinAscent/usWinDescent when either is non-zero, unless the font has AAT morph tables (mort/morx) and no
 * GDEF/GPOS/GSUB; lineGap is then what remains of the hhea line height: max(0, hheaAsc + |hheaDesc| + hheaGap - winAsc - winDesc).
 * With USE_TYPO_METRICS (OS/2 fsSelection bit 7) the typographic values are used; without OS/2 or with both win values 0, hhea. */
MF(ascent_win_default, NOP, ascent, 1800)
MF(descent_win_default, NOP, descent, 450)
MF(linegap_win_default_clamped_to_zero, NOP, lineGap, 0)
MF(linegap_win_remainder, sp.hhea_gap = 500, lineGap, 1700 + 400 + 500 - 1800 - 450)
MF(linegap_win_remainder_large, sp.hhea_gap = 2000; sp.hhea_asc = 3000, lineGap, 3000 + 400 + 2000 - 1800 - 450)
MF(ascent_win_custom, sp.win_asc = 1999, ascent, 1999)
MF(descent_win_custom, sp.win_desc = 777, descent, 777)
MF(ascent_win_zero_descent_nonzero, sp.win_asc = 0, ascent, 0)
MF(descent_win_zero_asc_nonzero, sp.win_asc = 0, descent, 450)
MF(ascent_both_win_zero_falls_back_to_hhea, sp.win_asc = 0; sp.win_desc = 0, ascent, 1700)
MF(descent_both_win_zero_falls_back_to_hhea, sp.win_asc = 0; sp.win_desc = 0, descent, 400)
MF(linegap_both_win_zero_is_hhea_gap, sp.win_asc = 0; sp.win_desc = 0, lineGap, 90)
MF(ascent_no_os2_is_hhea, sp.omit = TTF_OMIT_OS2, ascent, 1700)
MF(descent_no_os2_is_hhea_abs, sp.omit = TTF_OMIT_OS2, descent, 400)
MF(linegap_no_os2_is_hhea_gap, sp.omit = TTF_OMIT_OS2, lineGap, 90)
MF(ascent_use_typo, USE_TYPO, ascent, 1600)
MF(descent_use_typo_abs, USE_TYPO, descent, 420)
MF(linegap_use_typo, USE_TYPO, lineGap, 80)
MF(ascent_use_typo_custom, USE_TYPO; sp.typo_asc = 1234, ascent, 1234)
MF(descent_use_typo_custom, USE_TYPO; sp.typo_desc = -321, descent, 321)
MF(linegap_use_typo_zero, USE_TYPO; sp.typo_gap = 0, lineGap, 0)
MF(typo_flag_set, USE_TYPO, hasTypographicMetrics, 1)
MF(typo_flag_clear_by_default, NOP, hasTypographicMetrics, 0)
MF(typo_flag_clear_without_os2, sp.omit = TTF_OMIT_OS2, hasTypographicMetrics, 0)
MF(typo_flag_bit7_only, sp.fs_selection = 0x80, hasTypographicMetrics, 1)
MF(typo_flag_other_bits_ignored, sp.fs_selection = 0x7F, hasTypographicMetrics, 0)
TEST(dm_ascent_descent_is_one_of_win_or_hhea) {
    /* A relaxed form of the rule above: whichever source is used, ascent and descent come from one consistent pair. */
    ttf_spec sp; int a, d; ttf_default_spec(&sp);
    if (!met_field(&sp, offsetof(DWRITE_FONT_METRICS1, ascent), &a) || !met_field(&sp, offsetof(DWRITE_FONT_METRICS1, descent), &d)) return;
    CHECKF((a == 1800 && d == 450) || (a == 1700 && d == 400), "ascent %d descent %d", a, d);
}

/* ---- underline / strikethrough (post, OS/2) ---- */
MF(underline_pos_from_post, NOP, underlinePosition, -200)
MF(underline_thick_from_post, NOP, underlineThickness, 100)
MF(underline_pos_custom, sp.underline_pos = -333, underlinePosition, -333)
MF(underline_thick_custom, sp.underline_thick = 77, underlineThickness, 77)
MF(underline_pos_positive, sp.underline_pos = 55, underlinePosition, 55)
/* Without 'post': position = -(upm+5)/10 (truncated), thickness = (upm+7)/14. */
MF(underline_pos_no_post_2048, sp.omit = TTF_OMIT_POST, underlinePosition, -(2048 + 5) / 10)
MF(underline_thick_no_post_2048, sp.omit = TTF_OMIT_POST, underlineThickness, (2048 + 7) / 14)
MF(underline_pos_no_post_1000, sp.omit = TTF_OMIT_POST; sp.upm = 1000, underlinePosition, -(1000 + 5) / 10)
MF(underline_thick_no_post_1000, sp.omit = TTF_OMIT_POST; sp.upm = 1000, underlineThickness, (1000 + 7) / 14)
MF(strike_pos_from_os2, NOP, strikethroughPosition, 600)
MF(strike_thick_from_os2, NOP, strikethroughThickness, 100)
MF(strike_pos_custom, sp.strike_pos = 777, strikethroughPosition, 777)
MF(strike_thick_custom, sp.strike_size = 55, strikethroughThickness, 55)
MF(strike_thick_zero_uses_underline_thickness, sp.strike_size = 0, strikethroughThickness, 100)
MF(strike_thick_zero_uses_underline_thickness_custom, sp.strike_size = 0; sp.underline_thick = 61, strikethroughThickness, 61)
MF(strike_pos_no_os2_is_third_of_upm, sp.omit = TTF_OMIT_OS2, strikethroughPosition, 2048 / 3)
MF(strike_thick_no_os2_is_underline_thickness, sp.omit = TTF_OMIT_OS2, strikethroughThickness, 100)

/* ---- sub/superscript (OS/2) ---- */
MF(subscript_size_x, NOP, subscriptSizeX, 1024)
MF(subscript_size_y, NOP, subscriptSizeY, 1024)
MF(subscript_pos_x, NOP, subscriptPositionX, 0)
MF(subscript_pos_y_is_negated_os2_offset, NOP, subscriptPositionY, -(2048 / 7))
MF(superscript_size_x, NOP, superscriptSizeX, 1024)
MF(superscript_size_y, NOP, superscriptSizeY, 1024)
MF(superscript_pos_x, NOP, superscriptPositionX, 0)
MF(superscript_pos_y, NOP, superscriptPositionY, 2048 / 3)
MF(subscript_pos_y_scales_with_upm, sp.upm = 1000, subscriptPositionY, -(1000 / 7))
MF(superscript_pos_y_scales_with_upm, sp.upm = 1000, superscriptPositionY, 1000 / 3)

/* ---- cap height / x height ---- */
MF(capheight_os2_v4, NOP, capHeight, 1400)
MF(xheight_os2_v4, NOP, xHeight, 1000)
MF(capheight_custom, sp.cap_height = 1300, capHeight, 1300)
MF(xheight_custom, sp.x_height = 900, xHeight, 900)
MF(capheight_os2_v2, sp.os2_version = 2, capHeight, 1400)
MF(xheight_os2_v2, sp.os2_version = 2, xHeight, 1000)
MF(capheight_os2_v3, sp.os2_version = 3, capHeight, 1400)
MF(capheight_os2_v5, sp.os2_version = 5, capHeight, 1400)
TEST(dm_heights_without_os2_heights_are_synthesised) {
    /* OS/2 v0/v1 have no sxHeight/sCapHeight: DirectWrite measures glyphs (or falls back to fractions of the em); both are non-zero. */
    int v; for (v = 0; v <= 1; v++) {
        ttf_spec sp; int c, x; ttf_default_spec(&sp); sp.os2_version = v;
        if (!met_field(&sp, offsetof(DWRITE_FONT_METRICS1, capHeight), &c) || !met_field(&sp, offsetof(DWRITE_FONT_METRICS1, xHeight), &x)) return;
        CHECKF(c > 0 && x > 0, "v%d cap %d x %d", v, c, x);
        t_note("OS/2 v%d: capHeight %d xHeight %d (glyph-derived)", v, c, x);
    }
}

/* ---- old struct == prefix of the new one ---- */
TEST(dm_getmetrics_old_is_prefix_of_metrics1) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s);
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    {
        DWRITE_FONT_METRICS m0; DWRITE_FONT_METRICS1 m1; memset(&m0, 0x5A, sizeof m0); memset(&m1, 0x5A, sizeof m1);
        IDWriteFontFace_GetMetrics(s.h.face, &m0);
        IDWriteFontFace1_GetMetrics(f1, &m1);
        CHECK(!memcmp(&m0, &m1, sizeof m0));
        EXPECT_EQ(m0.designUnitsPerEm, 2048);
        EXPECT_EQ(sizeof(DWRITE_FONT_METRICS), 20); EXPECT_EQ(sizeof(DWRITE_FONT_METRICS1), 48);
    }
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(dm_getmetrics_idempotent) {
    NEED_SYNTH(s); DWRITE_FONT_METRICS a, b;
    IDWriteFontFace_GetMetrics(s.h.face, &a); IDWriteFontFace_GetMetrics(s.h.face, &b);
    CHECK(!memcmp(&a, &b, sizeof a)); synth_close(&s);
}
TEST(dm_metrics_independent_of_simulation_flags_in_design_units) {
    NEED_SYNTH(s); DWRITE_FONT_METRICS a, b; IDWriteFontFace *fb = NULL;
    HRESULT hr = IDWriteFactory_CreateFontFace(s.h.f, s.h.face_type, 1, &s.h.file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &fb);
    CHECK_HR_OK(hr);
    if (fb) { IDWriteFontFace_GetMetrics(s.h.face, &a); IDWriteFontFace_GetMetrics(fb, &b); EXPECT_EQ(a.designUnitsPerEm, b.designUnitsPerEm); EXPECT_EQ(a.ascent, b.ascent); EXPECT_EQ(a.descent, b.descent); EXPECT_EQ(a.underlinePosition, b.underlinePosition); DW_RELEASE(fb); }
    synth_close(&s);
}

/* ---- caret metrics (hhea) ---- */
static void caret_check(const ttf_spec *sp, int rise, int run, int off) {
    synth s; IDWriteFontFace1 *f1; DWRITE_CARET_METRICS c; memset(&s, 0, sizeof s);
    if (!synth_open(&s, sp)) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) { memset(&c, 0x77, sizeof c); IDWriteFontFace1_GetCaretMetrics(f1, &c); EXPECT_EQ(c.slopeRise, rise); EXPECT_EQ(c.slopeRun, run); EXPECT_EQ(c.offset, off); IDWriteFontFace1_Release(f1); }
    else t_skip("no FontFace1");
    synth_close(&s);
}
TEST(dm_caret_upright) { ttf_spec sp; ttf_default_spec(&sp); caret_check(&sp, 1, 0, 0); }
TEST(dm_caret_italic_from_hhea) { ttf_spec sp; ttf_default_spec(&sp); sp.italic_angle_fixed = -12 * 65536; caret_check(&sp, 1000, 213, 0); }
TEST(dm_caret_struct_layout) { EXPECT_EQ(sizeof(DWRITE_CARET_METRICS), 6); }

/* ---- GetGlyphIndices ---- */
TEST(dg_default_map_all_ascii) {
    NEED_SYNTH(s); UINT32 cps[128]; UINT16 g[128]; int i, bad = 0;
    for (i = 0; i < 128; i++) cps[i] = i;
    memset(g, 0x55, sizeof g);
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 128, g));
    for (i = 0; i < 128; i++) if (g[i] != ttf_default_gid(i)) { if (bad++ < 5) CHECKF(0, "U+%04X got %u want %u", i, g[i], ttf_default_gid(i)); }
    synth_close(&s);
}
TEST(dg_default_map_bmp_sweep) {
    NEED_SYNTH(s); static UINT32 cps[0x3000]; static UINT16 g[0x3000]; unsigned i, bad = 0;
    for (i = 0; i < 0x3000; i++) cps[i] = i;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 0x3000, g));
    for (i = 0; i < 0x3000; i++) if (g[i] != ttf_default_gid(i)) bad++;
    EXPECT_EQ(bad, 0); synth_close(&s);
}
TEST(dg_known_values) {
    NEED_SYNTH(s); UINT32 cps[] = { ' ', 'A', 'Z', 'a', 'z', '0', '9', 'B' }; UINT16 g[8];
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 8, g));
    EXPECT_EQ(g[0], 1); EXPECT_EQ(g[1], 2); EXPECT_EQ(g[2], 27); EXPECT_EQ(g[3], 28); EXPECT_EQ(g[4], 53); EXPECT_EQ(g[5], 54); EXPECT_EQ(g[6], 63); EXPECT_EQ(g[7], 3);
    synth_close(&s);
}
TEST(dg_unmapped_is_glyph_zero) {
    NEED_SYNTH(s); UINT32 cps[] = { 0, 0x1F, 0x21, 0x7E, 0x80, 0xFF, 0x3000, 0xFFFD, 0xFFFF, 0x10000, 0x1F600, 0x10FFFF }; UINT16 g[12]; int i;
    memset(g, 0x55, sizeof g);
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 12, g));
    for (i = 0; i < 12; i++) CHECKF(g[i] == 0, "cp %x -> %u", cps[i], g[i]);
    synth_close(&s);
}
TEST(dg_beyond_unicode_range_is_zero) {
    NEED_SYNTH(s); UINT32 cps[] = { 0x110000, 0x200000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFFu }; UINT16 g[5]; int i;
    memset(g, 0x55, sizeof g);
    /* Code points >= 0x110000 index past the compact cmap (the lookup keeps cp>>6 below 0x4400): glyph 0, S_OK. */
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 5, g));
    for (i = 0; i < 5; i++) CHECKF(g[i] == 0, "cp %x -> %u", cps[i], g[i]);
    synth_close(&s);
}
TEST(dg_surrogates_unmapped) {
    NEED_SYNTH(s); UINT32 cps[] = { 0xD800, 0xDBFF, 0xDC00, 0xDFFF }; UINT16 g[4]; int i;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 4, g));
    for (i = 0; i < 4; i++) CHECK(g[i] == 0);
    synth_close(&s);
}
TEST(dg_count_zero_ok) {
    NEED_SYNTH(s); UINT16 g[1] = { 0x5555 }; UINT32 c[1] = { 'A' };
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, c, 0, g));
    EXPECT_EQ(g[0], 0x5555);
    synth_close(&s);
}
TEST(dg_null_codepoints_count_zero_ok) {
    NEED_SYNTH(s); UINT16 g[1] = { 0x5555 };
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, NULL, 0, g));
    synth_close(&s);
}
TEST(dg_null_codepoints_with_count_invalid) {
    NEED_SYNTH(s); UINT16 g[4]; memset(g, 0x55, sizeof g);
    /* The output is cleared first, then a NULL input array with a non-zero count is E_INVALIDARG. */
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(s.h.face, NULL, 4, g), E_INVALIDARG);
    { int i; for (i = 0; i < 4; i++) CHECKF(g[i] == 0, "g[%d]=%x", i, g[i]); }
    synth_close(&s);
}
TEST(dg_repeated_and_unsorted_input) {
    NEED_SYNTH(s); UINT32 cps[] = { 'z', 'A', 'z', ' ', 'A', 'Q', 'q', 'A' }; UINT16 g[8]; int i;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 8, g));
    for (i = 0; i < 8; i++) CHECKF(g[i] == ttf_default_gid(cps[i]), "i=%d", i);
    synth_close(&s);
}
TEST(dg_single_codepoint_calls_match_batch) {
    NEED_SYNTH(s); UINT32 i; UINT16 one; UINT32 cps[128]; UINT16 all[128];
    for (i = 0; i < 128; i++) cps[i] = i;
    IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 128, all);
    for (i = 0; i < 128; i++) { IDWriteFontFace_GetGlyphIndices(s.h.face, &cps[i], 1, &one); CHECKF(one == all[i], "cp %u", i); }
    synth_close(&s);
}
TEST(dg_has_character_matches_glyph) {
    NEED_SYNTH(s); IDWriteFontFace3 *f3 = NULL;
    if (FAILED(IDWriteFontFace_QueryInterface(s.h.face, &IID_IDWriteFontFace3, (void **)&f3)) || !f3) { t_skip("no FontFace3"); synth_close(&s); return; }
    {
        UINT32 cp; for (cp = 0; cp < 0x200; cp++) { BOOL has = IDWriteFontFace3_HasCharacter(f3, cp); CHECKF((has != 0) == (ttf_default_gid(cp) != 0), "U+%04X has=%d", cp, has); }
        CHECK(!IDWriteFontFace3_HasCharacter(f3, 0x110000)); CHECK(!IDWriteFontFace3_HasCharacter(f3, 0xFFFFFFFFu));
    }
    IDWriteFontFace3_Release(f3); synth_close(&s);
}
TEST(dg_custom_cmap_format12_nonbmp) {
    static const ttf_map m[] = { { 'A', 2 }, { 0x1F600, 3 }, { 0x10FFFF, 4 }, { 0x20000, 5 } };
    static const ttf_cmap cm[] = { { 12, 3, 10, 4, m, 0, NULL }, { 4, 3, 1, 1, m, 0, NULL } };
    ttf_spec sp; synth s; UINT32 cps[] = { 'A', 0x1F600, 0x10FFFF, 0x20000, 0x1F601 }; UINT16 g[5];
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.cmaps = cm; sp.ncmaps = 2;
    if (!synth_open(&s, &sp)) { t_note("not created %s", dw_hr_name(s.hr)); synth_close(&s); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.h.face, cps, 5, g));
    EXPECT_EQ(g[0], 2); EXPECT_EQ(g[1], 3); EXPECT_EQ(g[2], 4); EXPECT_EQ(g[3], 5); EXPECT_EQ(g[4], 0);
    synth_close(&s);
}

/* ---- GetDesignGlyphMetrics / Advances ---- */
static void dgm_all(const ttf_spec *sp, int check_vmtx) {
    synth s; ot_font f; ot_glyph g; DWRITE_FONT_METRICS fm; int i, n; UINT16 ids[512]; DWRITE_GLYPH_METRICS gm[512]; memset(&s, 0, sizeof s);
    if (!synth_open(&s, sp)) { t_skip("open"); synth_close(&s); return; }
    ot_font_init(&f, s.blob, s.n, &s.dir);
    n = sp->nglyphs; for (i = 0; i < n; i++) ids[i] = (UINT16)i;
    IDWriteFontFace_GetMetrics(s.h.face, &fm);
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, ids, n, gm, FALSE));
    for (i = 0; i < n; i++) {
        const DWRITE_GLYPH_METRICS *m = &gm[i];
        int h = 0, w = 0, y1 = 0;
        ot_glyph_get(&f, i, &g);
        if (g.has_outline) { w = g.x1 - g.x0; h = g.y1 - g.y0; y1 = g.y1; }
        CHECKF(m->advanceWidth == (UINT32)g.adv, "gid %d advanceWidth %u want %d", i, m->advanceWidth, g.adv);
        CHECKF(m->leftSideBearing == g.lsb, "gid %d lsb %d want %d", i, m->leftSideBearing, g.lsb);
        CHECKF(m->rightSideBearing == g.adv - g.lsb - w, "gid %d rsb %d want %d", i, m->rightSideBearing, g.adv - g.lsb - w);
        /* vertical: top + height + bottom == advance height; origin = top + yMax */
        CHECKF((int)m->advanceHeight == m->topSideBearing + h + m->bottomSideBearing, "gid %d vertical sum", i);
        CHECKF(m->verticalOriginY == m->topSideBearing + y1, "gid %d verticalOrigin %d tsb %d yMax %d", i, m->verticalOriginY, m->topSideBearing, y1);
        if (check_vmtx) {
            ot_font ff = f; (void)ff;
            CHECKF(m->advanceHeight == (UINT32)(sp->glyphs[i].vadv ? sp->glyphs[i].vadv : sp->upm), "gid %d advanceHeight %u", i, m->advanceHeight);
            CHECKF(m->verticalOriginY == sp->v_ascent, "gid %d verticalOrigin %d want %d", i, m->verticalOriginY, sp->v_ascent);
        }
    }
    synth_close(&s);
}
TEST(dgm_all_default_glyphs_horizontal_and_vertical_consistency) { ttf_spec sp; ttf_default_spec(&sp); dgm_all(&sp, 0); }
TEST(dgm_all_default_glyphs_long_loca) { ttf_spec sp; ttf_default_spec(&sp); sp.long_loca = 1; dgm_all(&sp, 0); }
TEST(dgm_all_default_glyphs_compressed_hmtx) { ttf_spec sp; ttf_default_spec(&sp); sp.compress_hmtx = 1; dgm_all(&sp, 0); }
TEST(dgm_all_default_glyphs_with_vmtx) { ttf_spec sp; ttf_default_spec(&sp); sp.vertical = 1; sp.v_ascent = 1500; sp.v_descent = -548; dgm_all(&sp, 1); }
TEST(dgm_all_with_upm_1000) { ttf_spec sp; ttf_default_spec(&sp); sp.upm = 1000; dgm_all(&sp, 0); }
TEST(dgm_hmtx_lsb_not_glyph_xmin) {
    /* lsb is the hmtx value, rsb = advance - lsb - (xMax - xMin): a glyph whose hmtx lsb disagrees with its outline shows which is used. */
    int n; const ttf_glyph *dg = ttf_default_glyphs(&n); ttf_glyph gl[80]; ttf_spec sp; synth s; DWRITE_GLYPH_METRICS gm; UINT16 id = 2;
    memcpy(gl, dg, n * sizeof gl[0]);
    gl[2].lsb_set = 1; gl[2].lsb = 50;
    ttf_default_spec(&sp); sp.glyphs = gl; sp.nglyphs = n;
    memset(&s, 0, sizeof s);
    if (!synth_open(&s, &sp)) { t_note("not created"); synth_close(&s); return; }
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, &id, 1, &gm, FALSE));
    EXPECT_EQ(gm.leftSideBearing, 50); EXPECT_EQ(gm.advanceWidth, 1000); EXPECT_EQ(gm.rightSideBearing, 1000 - 50 - 800);
    synth_close(&s);
}
TEST(dgm_known_glyph_values) {
    NEED_SYNTH(s); UINT16 ids[] = { 0, 1, 2, 67 }; DWRITE_GLYPH_METRICS gm[4];
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, ids, 4, gm, FALSE));
    EXPECT_EQ(gm[0].advanceWidth, 1200); EXPECT_EQ(gm[0].leftSideBearing, 100); EXPECT_EQ(gm[0].rightSideBearing, 1200 - 100 - 1000);
    EXPECT_EQ(gm[1].advanceWidth, 1000); EXPECT_EQ(gm[1].leftSideBearing, 0); EXPECT_EQ(gm[1].rightSideBearing, 1000);     /* empty glyph: no box */
    EXPECT_EQ(gm[2].advanceWidth, 1000); EXPECT_EQ(gm[2].leftSideBearing, 100); EXPECT_EQ(gm[2].rightSideBearing, 100);
    EXPECT_EQ(gm[3].advanceWidth, 500);
    synth_close(&s);
}
TEST(dgm_vertical_default_source_note) {
    NEED_SYNTH(s); UINT16 id = 2; DWRITE_GLYPH_METRICS gm; DWRITE_FONT_METRICS fm;
    IDWriteFontFace_GetMetrics(s.h.face, &fm);
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, &id, 1, &gm, FALSE));
    t_note("no vmtx: advanceHeight %u verticalOriginY %d topSideBearing %d bottomSideBearing %d (metrics asc %u desc %u)", gm.advanceHeight, gm.verticalOriginY, gm.topSideBearing, gm.bottomSideBearing, fm.ascent, fm.descent);
    synth_close(&s);
}
TEST(dgm_count_zero_ok) { NEED_SYNTH(s); UINT16 id = 2; DWRITE_GLYPH_METRICS gm; CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, &id, 0, &gm, FALSE)); synth_close(&s); }
TEST(dgm_null_indices_invalid) {
    NEED_SYNTH(s); DWRITE_GLYPH_METRICS gm[2]; memset(gm, 0x5A, sizeof gm);
    /* glyphIndices == NULL is rejected irrespective of the count, after the outputs are cleared. */
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, NULL, 2, gm, FALSE), E_INVALIDARG);
    { const unsigned char *p = (const unsigned char *)gm; size_t i, nz = 0; for (i = 0; i < sizeof gm; i++) nz += p[i] != 0; EXPECT_EQ(nz, 0); }
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, NULL, 0, gm, FALSE), E_INVALIDARG);
    synth_close(&s);
}
TEST(dgm_sideways_flag_accepted) {
    NEED_SYNTH(s); UINT16 id = 2; DWRITE_GLYPH_METRICS a, b;
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, &id, 1, &a, FALSE));
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, &id, 1, &b, TRUE));
    t_note("sideways: adv %u/%u lsb %d/%d tsb %d/%d advH %u/%u", a.advanceWidth, b.advanceWidth, a.leftSideBearing, b.leftSideBearing, a.topSideBearing, b.topSideBearing, a.advanceHeight, b.advanceHeight);
    synth_close(&s);
}
TEST(dgm_struct_layout) {
    EXPECT_EQ(sizeof(DWRITE_GLYPH_METRICS), 28);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_METRICS, advanceWidth), 4); EXPECT_EQ(offsetof(DWRITE_GLYPH_METRICS, rightSideBearing), 8);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_METRICS, topSideBearing), 12); EXPECT_EQ(offsetof(DWRITE_GLYPH_METRICS, advanceHeight), 16);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_METRICS, bottomSideBearing), 20); EXPECT_EQ(offsetof(DWRITE_GLYPH_METRICS, verticalOriginY), 24);
}
TEST(dga_design_advances_match_hmtx) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s); ot_font f; ot_glyph g; UINT16 ids[TTF_DEF_NGLYPHS]; INT32 adv[TTF_DEF_NGLYPHS]; int i;
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    ot_font_init(&f, s.blob, s.n, &s.dir);
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) ids[i] = (UINT16)i;
    CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, TTF_DEF_NGLYPHS, ids, adv, FALSE));
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) { ot_glyph_get(&f, i, &g); CHECKF(adv[i] == g.adv, "gid %d adv %d want %d", i, adv[i], g.adv); }
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(dga_null_indices_with_count_invalid_and_zeroed) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s); INT32 adv[3] = { 7, 7, 7 };
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    if (t_is_wine()) { t_note("NULL glyph index array faults under the emulator"); IDWriteFontFace1_Release(f1); synth_close(&s); return; }
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 3, NULL, adv, FALSE), E_INVALIDARG);
    EXPECT_EQ(adv[0], 0); EXPECT_EQ(adv[1], 0); EXPECT_EQ(adv[2], 0);
    CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 0, NULL, adv, FALSE));     /* count 0 with NULL indices is fine here */
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(dga_empty_and_repeated) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s); UINT16 ids[] = { 1, 67, 1, 0, 67 }; INT32 adv[5];
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 5, ids, adv, FALSE));
    EXPECT_EQ(adv[0], 1000); EXPECT_EQ(adv[1], 500); EXPECT_EQ(adv[2], 1000); EXPECT_EQ(adv[3], 1200); EXPECT_EQ(adv[4], 500);
    IDWriteFontFace1_Release(f1); synth_close(&s);
}

/* ---- unicode ranges ---- */
TEST(du_default_font_ranges) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s); DWRITE_UNICODE_RANGE r[8]; UINT32 n = 99;
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    memset(r, 0x55, sizeof r);
    CHECK_HR_OK(IDWriteFontFace1_GetUnicodeRanges(f1, 8, r, &n));
    EXPECT_EQ(n, 4);
    if (n == 4) {
        EXPECT_EQ(r[0].first, 0x20); EXPECT_EQ(r[0].last, 0x20);
        EXPECT_EQ(r[1].first, '0'); EXPECT_EQ(r[1].last, '9');
        EXPECT_EQ(r[2].first, 'A'); EXPECT_EQ(r[2].last, 'Z');
        EXPECT_EQ(r[3].first, 'a'); EXPECT_EQ(r[3].last, 'z');
    }
    { int i; for (i = 4; i < 8; i++) CHECKF(r[i].first == 0 && r[i].last == 0, "entry %d not cleared", i); }   /* the buffer is zeroed first */
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(du_count_query_returns_insufficient_buffer) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s); UINT32 n = 99;
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 0, NULL, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    EXPECT_EQ(n, 4);
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(du_exact_and_short_buffers) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s); DWRITE_UNICODE_RANGE r[4]; UINT32 n = 99;
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    CHECK_HR_OK(IDWriteFontFace1_GetUnicodeRanges(f1, 4, r, &n)); EXPECT_EQ(n, 4);
    n = 99; CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 3, r, &n), DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 4);
    n = 99; CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 1, r, &n), DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 4);
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(du_null_ranges_with_capacity_invalid) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s); UINT32 n = 99;
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 4, NULL, &n), E_INVALIDARG);
    EXPECT_EQ(n, 0);       /* actualRangeCount is cleared first */
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(du_ranges_follow_the_cmap) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 3 }, { 0x43, 4 }, { 0x100, 5 }, { 0x3000, 6 }, { 0x3001, 7 }, { 0x1F600, 8 } };
    static const ttf_cmap cm[] = { { 12, 3, 10, 7, m, 0, NULL }, { 4, 3, 1, 6, m, 0, NULL } };
    ttf_spec sp; synth s; IDWriteFontFace1 *f1; DWRITE_UNICODE_RANGE r[8]; UINT32 n = 0;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.cmaps = cm; sp.ncmaps = 2;
    if (!synth_open(&s, &sp)) { t_note("not created"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) {
        CHECK_HR_OK(IDWriteFontFace1_GetUnicodeRanges(f1, 8, r, &n));
        EXPECT_EQ(n, 4);
        if (n == 4) { EXPECT_EQ(r[0].first, 0x41); EXPECT_EQ(r[0].last, 0x43); EXPECT_EQ(r[1].first, 0x100); EXPECT_EQ(r[1].last, 0x100); EXPECT_EQ(r[2].first, 0x3000); EXPECT_EQ(r[2].last, 0x3001); EXPECT_EQ(r[3].first, 0x1F600); EXPECT_EQ(r[3].last, 0x1F600); }
        IDWriteFontFace1_Release(f1);
    }
    synth_close(&s);
}

/* ---- panose, monospace, symbol ---- */
TEST(dp_panose_zero_in_synth) {
    NEED_SYNTH(s); IDWriteFontFace3 *f3 = NULL; DWRITE_PANOSE p; int i, nz = 0;
    if (FAILED(IDWriteFontFace_QueryInterface(s.h.face, &IID_IDWriteFontFace3, (void **)&f3)) || !f3) { t_skip("no FontFace3"); synth_close(&s); return; }
    memset(&p, 0x77, sizeof p);
    IDWriteFontFace3_GetPanose(f3, &p);
    for (i = 0; i < 10; i++) nz += p.values[i] != 0;
    EXPECT_EQ(nz, 0);
    IDWriteFontFace3_Release(f3); synth_close(&s);
}
TEST(dp_is_symbol_false_for_unicode_cmap) {
    NEED_SYNTH(s); IDWriteFontFace_IsSymbolFont(s.h.face) ? CHECK(0) : CHECK(1); synth_close(&s);
}
TEST(dp_monospace_flag_note) {
    ttf_spec sp; synth s; IDWriteFontFace1 *f1; ttf_default_spec(&sp);
    memset(&s, 0, sizeof s);
    sp.is_fixed_pitch = 1;
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) { t_note("post.isFixedPitch=1 -> IsMonospacedFont %d", IDWriteFontFace1_IsMonospacedFont(f1)); IDWriteFontFace1_Release(f1); }
    synth_close(&s);
}
TEST(dp_monospace_false_for_default) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s);
    if (f1) { CHECK(!IDWriteFontFace1_IsMonospacedFont(f1)); IDWriteFontFace1_Release(f1); } else t_skip("no FontFace1");
    synth_close(&s);
}

/* ---- kerning ---- */
static const ttf_kernpair kp[] = { { 2, 3, -100 }, { 3, 2, 50 }, { 28, 29, -7 }, { 2, 2, 300 } };
static void kern_face(synth *s, int use_kern_table, int use_gpos) {
    ttf_spec sp; ttf_default_spec(&sp);
    if (use_kern_table) { sp.kern = kp; sp.nkern = 4; }
    if (use_gpos) { sp.gpos_kern = kp; sp.ngpos_kern = 4; }
    memset(s, 0, sizeof *s);
    synth_open(s, &sp);
}
TEST(dk_kern_table_pair_values) {
    synth s; IDWriteFontFace1 *f1; UINT16 g[] = { 2, 3, 2, 2, 28, 29, 30 }; INT32 adj[7];
    kern_face(&s, 1, 0);
    if (!s.h.face) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    memset(adj, 0x55, sizeof adj);
    CHECK_HR_OK(IDWriteFontFace1_GetKerningPairAdjustments(f1, 7, g, adj));
    /* adjustment[i] belongs to the pair (g[i], g[i+1]); the last entry has no partner and is 0. */
    EXPECT_EQ(adj[0], -100);     /* (2,3) */
    EXPECT_EQ(adj[1], 50);       /* (3,2) */
    EXPECT_EQ(adj[2], 300);      /* (2,2) */
    EXPECT_EQ(adj[3], 0);        /* (2,28) */
    EXPECT_EQ(adj[4], -7);       /* (28,29) */
    EXPECT_EQ(adj[5], 0);        /* (29,30) */
    EXPECT_EQ(adj[6], 0);
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(dk_pair_3_2_is_present_in_table) {
    synth s; IDWriteFontFace1 *f1; UINT16 g[] = { 3, 2 }; INT32 adj[2];
    kern_face(&s, 1, 0);
    if (!s.h.face) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) { IDWriteFontFace1_GetKerningPairAdjustments(f1, 2, g, adj); EXPECT_EQ(adj[0], 50); EXPECT_EQ(adj[1], 0); IDWriteFontFace1_Release(f1); }
    synth_close(&s);
}
TEST(dk_has_kerning_pairs_with_kern_table) {
    synth s; IDWriteFontFace1 *f1; kern_face(&s, 1, 0);
    if (!s.h.face) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) { CHECK(IDWriteFontFace1_HasKerningPairs(f1)); IDWriteFontFace1_Release(f1); }
    synth_close(&s);
}
TEST(dk_no_kerning_font) {
    NEED_SYNTH(s); IDWriteFontFace1 *f1 = face1(&s); UINT16 g[] = { 2, 3, 2 }; INT32 adj[3];
    if (!f1) { t_skip("no FontFace1"); synth_close(&s); return; }
    CHECK(!IDWriteFontFace1_HasKerningPairs(f1));
    memset(adj, 0x55, sizeof adj);
    CHECK_HR_OK(IDWriteFontFace1_GetKerningPairAdjustments(f1, 3, g, adj));
    EXPECT_EQ(adj[0], 0); EXPECT_EQ(adj[1], 0); EXPECT_EQ(adj[2], 0);
    IDWriteFontFace1_Release(f1); synth_close(&s);
}
TEST(dk_gpos_only_font_note) {
    synth s; IDWriteFontFace1 *f1; UINT16 g[] = { 2, 3, 2 }; INT32 adj[3];
    kern_face(&s, 0, 1);
    if (!s.h.face) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) {
        /* GetKerningPairAdjustments reads only the legacy 'kern' table; a GPOS-only font is reported through HasKerningPairs per the font flags. */
        IDWriteFontFace1_GetKerningPairAdjustments(f1, 3, g, adj);
        t_note("GPOS-only kerning: HasKerningPairs %d adjustments %d %d %d", IDWriteFontFace1_HasKerningPairs(f1), adj[0], adj[1], adj[2]);
        IDWriteFontFace1_Release(f1);
    }
    synth_close(&s);
}
TEST(dk_count_zero_and_single) {
    synth s; IDWriteFontFace1 *f1; UINT16 g[] = { 2 }; INT32 adj[1] = { 0x55 };
    kern_face(&s, 1, 0);
    if (!s.h.face) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) {
        CHECK_HR_OK(IDWriteFontFace1_GetKerningPairAdjustments(f1, 0, g, adj)); EXPECT_EQ(adj[0], 0x55);
        CHECK_HR_OK(IDWriteFontFace1_GetKerningPairAdjustments(f1, 1, g, adj)); EXPECT_EQ(adj[0], 0);
        IDWriteFontFace1_Release(f1);
    }
    synth_close(&s);
}
TEST(dk_null_indices_with_count_invalid) {
    synth s; IDWriteFontFace1 *f1; INT32 adj[3] = { 9, 9, 9 };
    kern_face(&s, 1, 0);
    if (!s.h.face) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) {
        CHECK_HR(IDWriteFontFace1_GetKerningPairAdjustments(f1, 3, NULL, adj), E_INVALIDARG);
        EXPECT_EQ(adj[0], 0); EXPECT_EQ(adj[1], 0); EXPECT_EQ(adj[2], 0);        /* zeroed before validation */
        IDWriteFontFace1_Release(f1);
    }
    synth_close(&s);
}
TEST(dk_extreme_values_roundtrip) {
    static const ttf_kernpair ext[] = { { 2, 3, -32768 }, { 3, 4, 32767 } };
    ttf_spec sp; synth s; IDWriteFontFace1 *f1; UINT16 g[] = { 2, 3, 4 }; INT32 adj[3];
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.kern = ext; sp.nkern = 2;
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    f1 = face1(&s);
    if (f1) { IDWriteFontFace1_GetKerningPairAdjustments(f1, 3, g, adj); EXPECT_EQ(adj[0], -32768); EXPECT_EQ(adj[1], 32767); EXPECT_EQ(adj[2], 0); IDWriteFontFace1_Release(f1); }
    synth_close(&s);
}
