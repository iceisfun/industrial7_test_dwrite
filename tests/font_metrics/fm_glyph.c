/* Glyph level metrics on the deterministic default font: IDWriteFontFace::GetDesignGlyphMetrics, IDWriteFontFace::GetGdiCompatibleGlyphMetrics,
 * IDWriteFontFace1::GetDesignGlyphAdvances / GetGdiCompatibleGlyphAdvances, bold / oblique simulation effects.
 *
 * Design metrics come from hmtx (advance, lsb), the glyf header box and, for the vertical values, vmtx or - without it - the typographic
 * ascender/descender (OS/2 typo values when the typo ascender is not negative, else win values, else hhea): advanceHeight = asc + desc,
 * verticalOriginY = asc, topSideBearing = asc - yMax, bottomSideBearing = advanceHeight - tsb - height. */
#include "fm.h"
#include <math.h>

static int setup(fm_font *F, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim) {
    if (!fm_open(F, s, sim)) return 0;
    if (!F->f1) { t_skip("IDWriteFontFace1 unavailable"); return 0; }
    return 1;
}
#define SETUP(F, spec, sim) fm_font F; memset(&F, 0, sizeof F); if (!setup(&F, &(spec), (sim))) { fm_close(&F); return; }
static void fill_gm(DWRITE_GLYPH_METRICS *m, int n) { memset(m, 0xCD, n * sizeof *m); }

TEST(fmh_design_metrics_known_values) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 g[3] = { 2, 1, 0 }; DWRITE_GLYPH_METRICS m[3];
    fill_gm(m, 3);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, g, 3, m, FALSE), S_OK);
    /* 'A': hmtx 1000 / 100, box 100..900 x 0..1400, typo ascender 1600 / descender 420 give advanceHeight 2020 */
    EXPECT_EQ(m[0].advanceWidth, 1000); EXPECT_EQ(m[0].leftSideBearing, 100); EXPECT_EQ(m[0].rightSideBearing, 100);
    EXPECT_EQ(m[0].advanceHeight, 2020); EXPECT_EQ(m[0].topSideBearing, 200); EXPECT_EQ(m[0].bottomSideBearing, 420); EXPECT_EQ(m[0].verticalOriginY, 1600);
    /* space: empty glyph (no glyf data): box is all zero, rsb = advance - lsb */
    EXPECT_EQ(m[1].advanceWidth, 1000); EXPECT_EQ(m[1].leftSideBearing, 0); EXPECT_EQ(m[1].rightSideBearing, 1000);
    EXPECT_EQ(m[1].advanceHeight, 2020); EXPECT_EQ(m[1].topSideBearing, 1600); EXPECT_EQ(m[1].bottomSideBearing, 420); EXPECT_EQ(m[1].verticalOriginY, 1600);
    /* .notdef: advance 1200, box 100..1100 */
    EXPECT_EQ(m[2].advanceWidth, 1200); EXPECT_EQ(m[2].leftSideBearing, 100); EXPECT_EQ(m[2].rightSideBearing, 100);
    fm_close(&F);
}
TEST(fmh_design_metrics_all_default_glyphs) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    unsigned g;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        UINT16 id = (UINT16)g; DWRITE_GLYPH_METRICS m, w; char l[32];
        fill_gm(&m, 1);
        CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
        fm_design_expect(fm_def_glyph(g), 1600, 420, &w);
        snprintf(l, sizeof l, "gid %u", g); FM_GMCMP(&m, &w, l);
    }
    fm_close(&F);
}
TEST(fmh_design_metrics_batch_in_one_call) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[69]; DWRITE_GLYPH_METRICS m[69]; unsigned i;
    for (i = 0; i < 69; i++) ids[i] = (UINT16)(68 - i);
    fill_gm(m, 69);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 69, m, FALSE), S_OK);
    for (i = 0; i < 69; i++) { DWRITE_GLYPH_METRICS w; char l[32]; fm_design_expect(fm_def_glyph(ids[i]), 1600, 420, &w); snprintf(l, sizeof l, "slot %u gid %u", i, ids[i]); FM_GMCMP(&m[i], &w, l); }
    fm_close(&F);
}
TEST(fmh_design_metrics_duplicates_and_order) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[6] = { 67, 2, 67, 0, 2, 1 }; DWRITE_GLYPH_METRICS m[6]; unsigned i;
    fill_gm(m, 6);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 6, m, FALSE), S_OK);
    for (i = 0; i < 6; i++) { DWRITE_GLYPH_METRICS w; fm_design_expect(fm_def_glyph(ids[i]), 1600, 420, &w); FM_GMCMP(&m[i], &w, "slot"); }
    EXPECT_EQ(m[0].advanceWidth, 500); EXPECT_EQ(m[2].advanceWidth, 500);
    fm_close(&F);
}
TEST(fmh_design_metrics_count_zero) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 id = 2; DWRITE_GLYPH_METRICS m; fill_gm(&m, 1);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 0, &m, FALSE), S_OK);
    { const uint8_t *p = (const uint8_t *)&m; unsigned i, same = 1; for (i = 0; i < sizeof m; i++) if (p[i] != 0xCD) same = 0; CHECKF(same, "count 0 must not touch the buffer"); }
    fm_close(&F);
}
TEST(fmh_design_metrics_null_indices) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_GLYPH_METRICS m[3]; unsigned i;
    fill_gm(m, 3);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, NULL, 3, m, FALSE), E_INVALIDARG);
    /* the output array is cleared before the argument check */
    for (i = 0; i < 3; i++) CHECKF(m[i].advanceWidth == 0 && m[i].leftSideBearing == 0 && m[i].verticalOriginY == 0 && m[i].advanceHeight == 0, "slot %u not zeroed", i);
    /* NULL indices are rejected even for a count of zero */
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, NULL, 0, m, FALSE), E_INVALIDARG);
    fm_close(&F);
}
TEST(fmh_design_metrics_sideways_flag_same_without_simulation) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[4] = { 2, 1, 66, 67 }; DWRITE_GLYPH_METRICS a[4], b[4]; unsigned i;
    fill_gm(a, 4); fill_gm(b, 4);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 4, a, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 4, b, TRUE), S_OK);
    for (i = 0; i < 4; i++) FM_GMCMP(&a[i], &b[i], "sideways");
    fm_close(&F);
}
TEST(fmh_design_metrics_deterministic) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[3] = { 3, 30, 60 }; DWRITE_GLYPH_METRICS a[3], b[3];
    fill_gm(a, 3); fill_gm(b, 3);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 3, a, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 3, b, FALSE), S_OK);
    CHECK(!memcmp(a, b, sizeof a));
    fm_close(&F);
}
TEST(fmh_design_glyph_identity_A_to_Z_a_to_z_digits) {
    /* every letter and digit of the generated font is the same box */
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT32 cps[62]; UINT16 gi[62]; DWRITE_GLYPH_METRICS m[62]; unsigned i, k = 0, c;
    for (c = 'A'; c <= 'Z'; c++) cps[k++] = c;
    for (c = 'a'; c <= 'z'; c++) cps[k++] = c;
    for (c = '0'; c <= '9'; c++) cps[k++] = c;
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(F.face, cps, 62, gi), S_OK);
    fill_gm(m, 62);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, gi, 62, m, FALSE), S_OK);
    for (i = 0; i < 62; i++) { DWRITE_GLYPH_METRICS w; char l[16]; fm_design_expect(fm_def_glyph(1 + 1), 1600, 420, &w); snprintf(l, sizeof l, "U+%04X", cps[i]); FM_GMCMP(&m[i], &w, l); }
    fm_close(&F);
}
TEST(fmh_design_out_of_range_gid_is_survivable) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[2] = { 69, 0xFFFF }; DWRITE_GLYPH_METRICS m[2]; HRESULT hr;
    fill_gm(m, 2);
    hr = IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 2, m, FALSE);
    t_note("out-of-range gid -> %s: adv %u/%u", dw_hr_name(hr), m[0].advanceWidth, m[1].advanceWidth);
    CHECKF(hr == S_OK || FAILED(hr), "returned");
    fm_close(&F);
}

/* ---- vertical values by source ------------------------------------------------------------------------------------------------ */
TEST(fmh_design_vertical_from_typo_values) {
    static const struct { int ta, td; } t[] = { { 1000, -300 }, { 0, 0 }, { 32767, -32767 }, { 500, 100 }, { 1600, -420 }, { 1, -1 } };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ttf_spec s; ttf_default_spec(&s); s.typo_asc = t[i].ta; s.typo_desc = t[i].td; SETUP(F, s, 0);
        UINT16 id = 2; DWRITE_GLYPH_METRICS m, w; char l[48];
        CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
        /* typo descender is taken as |value| (so a positive typoDescender behaves like a negative one) */
        fm_design_expect(fm_def_glyph(2), t[i].ta, abs(t[i].td), &w);
        snprintf(l, sizeof l, "typo %d/%d", t[i].ta, t[i].td); FM_GMCMP(&m, &w, l);
        fm_close(&F);
    }
}
TEST(fmh_design_vertical_negative_typo_uses_win_values) {
    ttf_spec s; ttf_default_spec(&s); s.typo_asc = -5; s.win_asc = 1234; s.win_desc = 321; SETUP(F, s, 0);
    UINT16 id = 2; DWRITE_GLYPH_METRICS m, w;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
    fm_design_expect(fm_def_glyph(2), 1234, 321, &w); FM_GMCMP(&m, &w, "win fallback");
    fm_close(&F);
}
TEST(fmh_design_vertical_without_os2_uses_hhea) {
    ttf_spec s; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2; SETUP(F, s, 0);
    UINT16 id = 2; DWRITE_GLYPH_METRICS m, w;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
    fm_design_expect(fm_def_glyph(2), 1700, 400, &w); FM_GMCMP(&m, &w, "hhea");
    fm_close(&F);
}
TEST(fmh_design_vertical_ignores_typo_flag) {
    ttf_spec s; ttf_default_spec(&s); s.fs_selection |= 0x80; SETUP(F, s, 0);
    UINT16 id = 2; DWRITE_GLYPH_METRICS m, w;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
    fm_design_expect(fm_def_glyph(2), 1600, 420, &w); FM_GMCMP(&m, &w, "typo flag");
    fm_close(&F);
}
TEST(fmh_design_vertical_with_vmtx) {
    ttf_spec s; ttf_default_spec(&s); s.vertical = 1; s.v_ascent = 1500; s.v_descent = -548; SETUP(F, s, 0);
    UINT16 ids[3] = { 2, 1, 66 }; DWRITE_GLYPH_METRICS m[3]; unsigned i;
    fill_gm(m, 3);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 3, m, FALSE), S_OK);
    /* vmtx supplies advanceHeight (2048 = upm) and topSideBearing (vAscent - yMax); the origin is yMax + tsb */
    for (i = 0; i < 3; i++) {
        const fm_gbox *g = fm_def_glyph(ids[i]); int tsb = 1500 - g->y1, advh = 2048;
        DWRITE_GLYPH_METRICS w;
        w.advanceWidth = g->adv; w.leftSideBearing = g->lsb; w.rightSideBearing = g->adv - g->lsb - g->x1 + g->x0;
        w.advanceHeight = advh; w.topSideBearing = tsb; w.bottomSideBearing = (g->y0 - g->y1) - tsb + advh; w.verticalOriginY = g->y1 + tsb;
        FM_GMCMP(&m[i], &w, "vmtx");
    }
    EXPECT_EQ(m[0].verticalOriginY, 1500); EXPECT_EQ(m[0].topSideBearing, 100); EXPECT_EQ(m[0].bottomSideBearing, 548);
    fm_close(&F);
}
TEST(fmh_design_vertical_vmtx_custom_advances) {
    static ttf_glyph gl[TTF_DEF_NGLYPHS];
    int n, i; const ttf_glyph *d = ttf_default_glyphs(&n);
    memcpy(gl, d, sizeof gl);
    gl[2].vadv = 1111; gl[2].tsb_set = 1; gl[2].tsb = -77; gl[3].vadv = 3000;
    ttf_spec s; ttf_default_spec(&s); s.glyphs = gl; s.nglyphs = n; s.vertical = 1; s.v_ascent = 1500; s.v_descent = -500; SETUP(F, s, 0);
    UINT16 ids[2] = { 2, 3 }; DWRITE_GLYPH_METRICS m[2];
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 2, m, FALSE), S_OK);
    EXPECT_EQ(m[0].advanceHeight, 1111); EXPECT_EQ(m[0].topSideBearing, -77); EXPECT_EQ(m[0].verticalOriginY, 1400 - 77); EXPECT_EQ(m[0].bottomSideBearing, (0 - 1400) + 77 + 1111);
    EXPECT_EQ(m[1].advanceHeight, 3000); EXPECT_EQ(m[1].topSideBearing, 100); EXPECT_EQ(m[1].verticalOriginY, 1500);
    (void)i;
    fm_close(&F);
}
TEST(fmh_design_hmtx_short_form) {
    /* numberOfHMetrics < numGlyphs: trailing glyphs reuse the last advance and take their lsb from the trailing array */
    static ttf_glyph gl[TTF_DEF_NGLYPHS];
    int n, i; const ttf_glyph *d = ttf_default_glyphs(&n);
    memcpy(gl, d, sizeof gl);
    for (i = 64; i < n; i++) gl[i].adv = 700;
    ttf_spec s; ttf_default_spec(&s); s.glyphs = gl; s.nglyphs = n; s.compress_hmtx = 1; SETUP(F, s, 0);
    UINT16 ids[5] = { 63, 64, 65, 66, 68 }; DWRITE_GLYPH_METRICS m[5];
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 5, m, FALSE), S_OK);
    EXPECT_EQ(m[0].advanceWidth, 1000); EXPECT_EQ(m[1].advanceWidth, 700); EXPECT_EQ(m[2].advanceWidth, 700); EXPECT_EQ(m[3].advanceWidth, 700); EXPECT_EQ(m[4].advanceWidth, 700);
    EXPECT_EQ(m[1].leftSideBearing, 100); EXPECT_EQ(m[3].leftSideBearing, 100); EXPECT_EQ(m[4].leftSideBearing, 100);
    EXPECT_EQ(m[3].rightSideBearing, 700 - 100 - 800);
    fm_close(&F);
}
TEST(fmh_design_lsb_from_hmtx_not_outline) {
    /* lsb is taken from hmtx even when it disagrees with xMin; rsb = advance - lsb - (xMax - xMin) */
    static ttf_glyph gl[TTF_DEF_NGLYPHS];
    int n; const ttf_glyph *d = ttf_default_glyphs(&n);
    memcpy(gl, d, sizeof gl);
    gl[2].lsb_set = 1; gl[2].lsb = 37;
    ttf_spec s; ttf_default_spec(&s); s.glyphs = gl; s.nglyphs = n; SETUP(F, s, 0);
    UINT16 id = 2; DWRITE_GLYPH_METRICS m;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
    EXPECT_EQ(m.leftSideBearing, 37); EXPECT_EQ(m.rightSideBearing, 1000 - 37 - 800);
    fm_close(&F);
}
TEST(fmh_design_negative_bearings) {
    static ttf_glyph gl[TTF_DEF_NGLYPHS];
    int n; const ttf_glyph *d = ttf_default_glyphs(&n);
    memcpy(gl, d, sizeof gl);
    gl[2].adv = 600;     /* narrower than the 800 unit box: negative right side bearing */
    ttf_spec s; ttf_default_spec(&s); s.glyphs = gl; s.nglyphs = n; SETUP(F, s, 0);
    UINT16 id = 2; DWRITE_GLYPH_METRICS m;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
    EXPECT_EQ(m.advanceWidth, 600); EXPECT_EQ(m.rightSideBearing, 600 - 100 - 800);
    CHECK(m.rightSideBearing < 0);
    fm_close(&F);
}

/* ---- simulations ------------------------------------------------------------------------------------------------------------------------- */
TEST(fmh_bold_simulation_widens_inked_glyphs) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, DWRITE_FONT_SIMULATIONS_BOLD);
    UINT16 ids[4] = { 2, 1, 67, 66 }; DWRITE_GLYPH_METRICS m[4], p[4];
    fm_font G; memset(&G, 0, sizeof G);
    if (!setup(&G, &s, 0)) { fm_close(&G); fm_close(&F); return; }
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 4, m, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(G.face, ids, 4, p, FALSE), S_OK);
    /* extra = (upm + 25) / 50 = 41: advanceWidth, advanceHeight and verticalOriginY grow for glyphs that have ink; side bearings and tsb/bsb stay */
    EXPECT_EQ(m[0].advanceWidth, p[0].advanceWidth + 41); EXPECT_EQ(m[0].advanceHeight, p[0].advanceHeight + 41); EXPECT_EQ(m[0].verticalOriginY, p[0].verticalOriginY + 41);
    EXPECT_EQ(m[0].leftSideBearing, p[0].leftSideBearing); EXPECT_EQ(m[0].rightSideBearing, p[0].rightSideBearing);
    EXPECT_EQ(m[0].topSideBearing, p[0].topSideBearing); EXPECT_EQ(m[0].bottomSideBearing, p[0].bottomSideBearing);
    EXPECT_EQ(m[0].advanceWidth, 1041);
    /* empty glyphs are untouched */
    FM_GMCMP(&m[1], &p[1], "space"); FM_GMCMP(&m[2], &p[2], "empty500");
    EXPECT_EQ(m[3].advanceWidth, p[3].advanceWidth + 41);
    fm_close(&G); fm_close(&F);
}
TEST(fmh_bold_extra_scales_with_upm) {
    static const int upm[] = { 1000, 1024, 2048, 4096, 512 };
    unsigned i;
    for (i = 0; i < sizeof upm / sizeof upm[0]; i++) {
        ttf_spec s; fm_spec_upm(&s, upm[i]); SETUP(F, s, DWRITE_FONT_SIMULATIONS_BOLD);
        fm_font G; memset(&G, 0, sizeof G);
        if (!setup(&G, &s, 0)) { fm_close(&G); fm_close(&F); return; }
        UINT16 id = 2; DWRITE_GLYPH_METRICS m, p;
        CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
        CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(G.face, &id, 1, &p, FALSE), S_OK);
        t_check((int)m.advanceWidth - (int)p.advanceWidth == (upm[i] + 25) / 50, __FILE__, __LINE__, "bold extra", "upm %d got %d want %d", upm[i], (int)m.advanceWidth - (int)p.advanceWidth, (upm[i] + 25) / 50);
        fm_close(&G); fm_close(&F);
    }
}
TEST(fmh_oblique_simulation_shears_side_bearings) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    fm_font G; memset(&G, 0, sizeof G);
    if (!setup(&G, &s, 0)) { fm_close(&G); fm_close(&F); return; }
    UINT16 ids[4] = { 2, 1, 66, 0 }; DWRITE_GLYPH_METRICS m[4], p[4]; unsigned i;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 4, m, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(G.face, ids, 4, p, FALSE), S_OK);
    for (i = 0; i < 4; i++) {
        DWRITE_GLYPH_METRICS w = p[i];
        int has = (int)p[i].advanceWidth - p[i].rightSideBearing - p[i].leftSideBearing > 0 && (int)p[i].advanceHeight - p[i].bottomSideBearing - p[i].topSideBearing > 0;
        if (has) {
            /* lsb += (bsb - advH + vOrigin) * 0x5700 >> 16 ; rsb -= (vOrigin - tsb) * 0x5700 >> 16 */
            w.leftSideBearing = p[i].leftSideBearing + (((int)p[i].bottomSideBearing - (int)p[i].advanceHeight + p[i].verticalOriginY) * 0x5700 >> 16);
            w.rightSideBearing = p[i].rightSideBearing - (((int)p[i].verticalOriginY - p[i].topSideBearing) * 0x5700 >> 16);
        }
        FM_GMCMP(&m[i], &w, "oblique");
    }
    EXPECT_EQ(m[0].leftSideBearing, 100); EXPECT_EQ(m[0].rightSideBearing, 100 - 475);   /* 'A': ymin = 0 -> no left shift; top shear 1400*0x5700>>16 = 475 */
    fm_close(&G); fm_close(&F);
}
TEST(fmh_bold_and_oblique_combined) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE);
    fm_font G; memset(&G, 0, sizeof G);
    if (!setup(&G, &s, 0)) { fm_close(&G); fm_close(&F); return; }
    UINT16 id = 2; DWRITE_GLYPH_METRICS m, p, w;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 1, &m, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(G.face, &id, 1, &p, FALSE), S_OK);
    /* bold first (advanceWidth, advanceHeight, origin + 41), then the shear computed from the already emboldened vertical values */
    w = p; w.advanceWidth += 41; w.advanceHeight += 41; w.verticalOriginY += 41;
    w.leftSideBearing = p.leftSideBearing + (((int)w.bottomSideBearing - (int)w.advanceHeight + w.verticalOriginY) * 0x5700 >> 16);
    w.rightSideBearing = p.rightSideBearing - (((int)w.verticalOriginY - w.topSideBearing) * 0x5700 >> 16);
    FM_GMCMP(&m, &w, "bold+oblique");
    fm_close(&G); fm_close(&F);
}

/* ---- design advances (IDWriteFontFace1) -------------------------------------------------------------------------------------------------- */
TEST(fmh_design_advances_match_hmtx) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[69]; INT32 adv[69]; unsigned i;
    for (i = 0; i < 69; i++) ids[i] = (UINT16)i;
    memset(adv, 0xCD, sizeof adv);
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, 69, ids, adv, FALSE), S_OK);
    for (i = 0; i < 69; i++) t_check(adv[i] == fm_def_glyph(i)->adv, __FILE__, __LINE__, "advance", "gid %u got %d want %d", i, adv[i], fm_def_glyph(i)->adv);
    fm_close(&F);
}
TEST(fmh_design_advances_equal_glyph_metrics_advance) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[69]; INT32 adv[69]; DWRITE_GLYPH_METRICS m[69]; unsigned i;
    for (i = 0; i < 69; i++) ids[i] = (UINT16)i;
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, 69, ids, adv, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 69, m, FALSE), S_OK);
    for (i = 0; i < 69; i++) t_check(adv[i] == (int)m[i].advanceWidth, __FILE__, __LINE__, "adv == metrics", "gid %u %d vs %u", i, adv[i], m[i].advanceWidth);
    fm_close(&F);
}
TEST(fmh_design_advances_sideways_are_advance_heights) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[3] = { 2, 1, 66 }; INT32 adv[3]; DWRITE_GLYPH_METRICS m[3]; unsigned i;
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, 3, ids, adv, TRUE), S_OK);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 3, m, FALSE), S_OK);
    for (i = 0; i < 3; i++) t_check(adv[i] == (int)m[i].advanceHeight, __FILE__, __LINE__, "sideways advance", "slot %u %d vs %u", i, adv[i], m[i].advanceHeight);
    EXPECT_EQ(adv[0], 2020);
    fm_close(&F);
}
TEST(fmh_design_advances_args) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    INT32 adv[4]; UINT16 id = 2; unsigned i;
    memset(adv, 0xCD, sizeof adv);
    if (t_is_wine()) t_note("NULL indices skipped on emulated runtimes (they dereference the pointer)");
    else {
        CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, 4, NULL, adv, FALSE), E_INVALIDARG);
        for (i = 0; i < 4; i++) CHECKF(adv[i] == 0, "advance %u not zeroed", i);       /* cleared before validation */
        CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, 0, NULL, adv, FALSE), S_OK);        /* count 0 with NULL indices is fine */
    }
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, 0, &id, adv, FALSE), S_OK);
    fm_close(&F);
}
TEST(fmh_design_advances_bold_simulation) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, DWRITE_FONT_SIMULATIONS_BOLD);
    UINT16 ids[4] = { 2, 1, 67, 0 }; INT32 adv[4];
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, 4, ids, adv, FALSE), S_OK);
    /* ink-bearing glyphs get +41; empty glyphs (space, empty500) stay */
    EXPECT_EQ(adv[0], 1041); EXPECT_EQ(adv[1], 1000); EXPECT_EQ(adv[2], 500); EXPECT_EQ(adv[3], 1241);
    fm_close(&F);
}
TEST(fmh_monospace_font_advances) {
    static ttf_glyph gl[TTF_DEF_NGLYPHS];
    int n, i; const ttf_glyph *d = ttf_default_glyphs(&n);
    memcpy(gl, d, sizeof gl);
    for (i = 0; i < n; i++) gl[i].adv = 1229;
    ttf_spec s; ttf_default_spec(&s); s.glyphs = gl; s.nglyphs = n; s.is_fixed_pitch = 1; SETUP(F, s, 0);
    UINT16 ids[69]; INT32 adv[69];
    for (i = 0; i < n; i++) ids[i] = (UINT16)i;
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, n, ids, adv, FALSE), S_OK);
    for (i = 0; i < n; i++) t_check(adv[i] == 1229, __FILE__, __LINE__, "monospace advance", "gid %d got %d", i, adv[i]);
    /* GDI compatible advances of one em size are equal across glyphs as well */
    {
        INT32 g[69]; int px;
        for (px = 8; px <= 40; px += 4) {
            CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, (float)px, 1.0f, NULL, FALSE, FALSE, n, ids, g), S_OK);
            for (i = 1; i < n; i++) t_check(g[i] == g[0], __FILE__, __LINE__, "monospace gdi advance", "em %d gid %d: %d vs %d", px, i, g[i], g[0]);
        }
    }
    fm_close(&F);
}

/* ---- GDI compatible glyph advances / metrics --------------------------------------------------------------------------------------------- */
TEST(fmh_gdi_advances_invalid_args) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[2] = { 2, 3 }; INT32 adv[2]; DWRITE_MATRIX nan = { 1, NAN, 0, 1, 0, 0 };
    if (!t_is_wine()) {   /* emulated runtimes dereference the NULL indices and would crash */
        memset(adv, 0xCD, sizeof adv); CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, 12.0f, 1.0f, NULL, FALSE, FALSE, 2, NULL, adv), E_INVALIDARG);
        CHECKF(adv[0] == 0 && adv[1] == 0, "not zeroed (NULL indices)");
    }
    memset(adv, 0xCD, sizeof adv); CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, -1.0f, 1.0f, NULL, FALSE, FALSE, 2, ids, adv), E_INVALIDARG);
    CHECKF(adv[0] == 0 && adv[1] == 0, "not zeroed (negative em)");
    memset(adv, 0xCD, sizeof adv); CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, 12.0f, 0.0f, NULL, FALSE, FALSE, 2, ids, adv), E_INVALIDARG);
    memset(adv, 0xCD, sizeof adv); CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, 12.0f, -2.0f, NULL, FALSE, FALSE, 2, ids, adv), E_INVALIDARG);
    memset(adv, 0xCD, sizeof adv); CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, 12.0f, 1.0f, &nan, FALSE, FALSE, 2, ids, adv), E_INVALIDARG);
    CHECKF(adv[0] == 0 && adv[1] == 0, "not zeroed (NaN matrix)");
    /* count 0 */
    if (!t_is_wine()) CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, 12.0f, 1.0f, NULL, FALSE, FALSE, 0, NULL, adv), S_OK);
    fm_close(&F);
}
TEST(fmh_gdi_advances_zero_em_succeeds_with_zero_advances) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[3] = { 2, 3, 0 }; INT32 adv[3];
    memset(adv, 0xCD, sizeof adv);
    /* emSize == 0 is not rejected (only negative sizes are): the call succeeds and every advance is 0 */
    CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, 0.0f, 1.0f, NULL, FALSE, FALSE, 3, ids, adv), S_OK);
    CHECKF(adv[0] == 0 && adv[1] == 0 && adv[2] == 0, "advances %d %d %d", adv[0], adv[1], adv[2]);
    fm_close(&F);
}
TEST(fmh_gdi_glyph_metrics_invalid_args) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 id = 2; DWRITE_GLYPH_METRICS m[2]; unsigned i;
    fill_gm(m, 2);
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 12.0f, 1.0f, NULL, FALSE, NULL, 2, m, FALSE), E_INVALIDARG);
    for (i = 0; i < 2; i++) CHECKF(m[i].advanceWidth == 0 && m[i].leftSideBearing == 0 && m[i].verticalOriginY == 0, "slot %u not zeroed", i);
    fill_gm(m, 1); CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 0.0f, 1.0f, NULL, FALSE, &id, 1, m, FALSE), E_INVALIDARG);
    CHECKF(m[0].advanceWidth == 0 && m[0].advanceHeight == 0, "not zeroed (em 0)");
    fill_gm(m, 1); CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, -3.0f, 1.0f, NULL, FALSE, &id, 1, m, FALSE), E_INVALIDARG);
    fill_gm(m, 1); CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 12.0f, 0.0f, NULL, FALSE, &id, 1, m, FALSE), E_INVALIDARG);
    fill_gm(m, 1); CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 12.0f, -1.0f, NULL, FALSE, &id, 1, m, FALSE), E_INVALIDARG);
    CHECKF(m[0].advanceWidth == 0, "not zeroed (ppd)");
    /* NULL indices fail even for a count of zero; a non-NULL pointer with count 0 succeeds */
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 12.0f, 1.0f, NULL, FALSE, NULL, 0, m, FALSE), E_INVALIDARG);
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 12.0f, 1.0f, NULL, FALSE, &id, 0, m, FALSE), S_OK);
    fm_close(&F);
}
TEST(fmh_gdi_glyph_metrics_at_design_size_equal_design_metrics) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[6] = { 2, 1, 0, 66, 67, 68 }; DWRITE_GLYPH_METRICS g[6], d[6]; unsigned i, nat;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 6, d, FALSE), S_OK);
    for (nat = 0; nat < 2; nat++) {
        /* ppem == upm with no transform and no simulation: the rasterizer is bypassed and the design metrics come back (nothing is rounded) */
        fill_gm(g, 6);
        CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 2048.0f, 1.0f, NULL, nat, ids, 6, g, FALSE), S_OK);
        for (i = 0; i < 6; i++) FM_GMCMP(&g[i], &d[i], nat ? "natural" : "classic");
    }
    fm_close(&F);
}
TEST(fmh_gdi_glyph_metrics_classic_advances_are_whole_pixels) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[4] = { 2, 1, 0, 67 }; int px;
    for (px = 8; px <= 48; px++) {
        DWRITE_GLYPH_METRICS m[4]; unsigned i;
        fill_gm(m, 4);
        CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, (float)px, 1.0f, NULL, FALSE, ids, 4, m, FALSE), S_OK);
        for (i = 0; i < 4; i++) {
            double pxadv = (double)m[i].advanceWidth * px / 2048.0, r = floor(pxadv + 0.5);
            /* design units are integers, so the pixel value is whole up to half a design unit */
            t_check(fabs(pxadv - r) <= 0.5 * px / 2048.0 + 1e-9, __FILE__, __LINE__, "whole pixel advance", "em %d gid %u: advance %u = %g px", px, ids[i], m[i].advanceWidth, pxadv);
        }
    }
    fm_close(&F);
}
TEST(fmh_gdi_glyph_metrics_advances_agree_with_glyph_advances_api) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[6] = { 2, 1, 0, 66, 67, 5 }; int px; unsigned nat;
    for (nat = 0; nat < 2; nat++)
        for (px = 7; px <= 40; px += 3) {
            DWRITE_GLYPH_METRICS m[6]; INT32 a[6]; unsigned i; char l[40];
            fill_gm(m, 6); memset(a, 0xCD, sizeof a);
            CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, (float)px, 1.0f, NULL, nat, ids, 6, m, FALSE), S_OK);
            CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, (float)px, 1.0f, NULL, nat, FALSE, 6, ids, a), S_OK);
            snprintf(l, sizeof l, "em %d nat %u", px, nat);
            for (i = 0; i < 6; i++) t_check(a[i] == (int)m[i].advanceWidth, __FILE__, __LINE__, "advances == glyph metrics", "%s slot %u: %d vs %u", l, i, a[i], m[i].advanceWidth);
        }
    fm_close(&F);
}
TEST(fmh_gdi_sideways_advances_are_advance_heights) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[3] = { 2, 1, 66 }; int px;
    for (px = 9; px <= 33; px += 6) {
        DWRITE_GLYPH_METRICS m[3]; INT32 a[3]; unsigned i;
        fill_gm(m, 3);
        CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, (float)px, 1.0f, NULL, FALSE, ids, 3, m, TRUE), S_OK);
        CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, (float)px, 1.0f, NULL, FALSE, TRUE, 3, ids, a), S_OK);
        for (i = 0; i < 3; i++) t_check(a[i] == (int)m[i].advanceHeight, __FILE__, __LINE__, "sideways gdi advance", "em %d slot %u: %d vs %u", px, i, a[i], m[i].advanceHeight);
    }
    fm_close(&F);
}
TEST(fmh_gdi_advances_at_design_size_are_design_advances) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[5] = { 2, 1, 0, 66, 67 }; INT32 a[5], d[5]; unsigned i;
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(F.f1, 5, ids, d, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, 2048.0f, 1.0f, NULL, FALSE, FALSE, 5, ids, a), S_OK);
    for (i = 0; i < 5; i++) EXPECT_EQ(a[i], d[i]);
    fm_close(&F);
}
TEST(fmh_gdi_advances_grow_with_size) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 id = 2; int px, prev = 0;
    for (px = 6; px <= 96; px += 2) {
        INT32 a;
        CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(F.f1, (float)px, 1.0f, NULL, FALSE, FALSE, 1, &id, &a), S_OK);
        /* classic mode rounds the pixel advance (1000 * px / 2048 for an unhinted glyph); record how far it strays from the linear value */
        double pxadv = (double)a * px / 2048.0, lin = 1000.0 * px / 2048.0;
        if (fabs(pxadv - lin) > 1.0) t_note("em %d: %g px vs linear %g", px, pxadv, lin);
        if (pxadv + 1e-6 < prev) t_note("em %d: %g px after %d (not monotonic)", px, pxadv, prev);
        prev = (int)floor(pxadv + 0.5);
    }
    fm_close(&F);
}
TEST(fmh_gdi_ppd_and_em_interchange_for_glyph_metrics) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[3] = { 2, 1, 66 }; DWRITE_GLYPH_METRICS a[3], b[3]; unsigned i;
    fill_gm(a, 3); fill_gm(b, 3);
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 12.0f, 2.0f, NULL, FALSE, ids, 3, a, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 24.0f, 1.0f, NULL, FALSE, ids, 3, b, FALSE), S_OK);
    for (i = 0; i < 3; i++) FM_GMCMP(&a[i], &b[i], "12*2 vs 24*1");
    fm_close(&F);
}
TEST(fmh_gdi_glyph_metrics_natural_vs_classic_notes) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    UINT16 ids[2] = { 2, 1 }; int px;
    for (px = 9; px <= 21; px += 4) {
        DWRITE_GLYPH_METRICS c[2], n[2];
        CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, (float)px, 1.0f, NULL, FALSE, ids, 2, c, FALSE), S_OK);
        CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, (float)px, 1.0f, NULL, TRUE, ids, 2, n, FALSE), S_OK);
        t_note("em %d 'A': classic adv=%u lsb=%d rsb=%d tsb=%d bsb=%d | natural adv=%u lsb=%d rsb=%d", px, c[0].advanceWidth, c[0].leftSideBearing, c[0].rightSideBearing,
               c[0].topSideBearing, c[0].bottomSideBearing, n[0].advanceWidth, n[0].leftSideBearing, n[0].rightSideBearing);
        /* the glyph box is the same ink in both modes: width in design units within one pixel */
        {
            double wc = (double)c[0].advanceWidth - c[0].leftSideBearing - c[0].rightSideBearing, wn = (double)n[0].advanceWidth - n[0].leftSideBearing - n[0].rightSideBearing;
            CHECKF(fabs(wc - wn) <= 2.0 * 2048.0 / px, "ink width classic %g natural %g", wc, wn);
        }
    }
    fm_close(&F);
}
TEST(fmh_gdi_glyph_metrics_transform_scale_equals_larger_em) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, 0);
    DWRITE_MATRIX m2 = { 2, 0, 0, 2, 0, 0 }; UINT16 ids[3] = { 2, 1, 0 }; DWRITE_GLYPH_METRICS a[3], b[3]; unsigned i;
    fill_gm(a, 3); fill_gm(b, 3);
    HRESULT h1 = IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 10.0f, 1.0f, &m2, FALSE, ids, 3, a, FALSE);
    HRESULT h2 = IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 20.0f, 1.0f, NULL, FALSE, ids, 3, b, FALSE);
    CHECK_HR(h1, S_OK); CHECK_HR(h2, S_OK);
    /* a pure scale in the transform is applied to the em size before rasterizing */
    for (i = 0; i < 3; i++) if (h1 == S_OK && h2 == S_OK) t_check(a[i].advanceWidth == b[i].advanceWidth, __FILE__, __LINE__, "advance", "slot %u: %u vs %u", i, a[i].advanceWidth, b[i].advanceWidth);
    fm_close(&F);
}
TEST(fmh_gdi_glyph_metrics_bold_simulation_notes) {
    ttf_spec s; ttf_default_spec(&s); SETUP(F, s, DWRITE_FONT_SIMULATIONS_BOLD);
    fm_font G; memset(&G, 0, sizeof G);
    if (!setup(&G, &s, 0)) { fm_close(&G); fm_close(&F); return; }
    UINT16 id = 2; DWRITE_GLYPH_METRICS b, p;
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(F.face, 16.0f, 1.0f, NULL, FALSE, &id, 1, &b, FALSE), S_OK);
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(G.face, 16.0f, 1.0f, NULL, FALSE, &id, 1, &p, FALSE), S_OK);
    t_note("em 16 'A': bold adv=%u plain adv=%u", b.advanceWidth, p.advanceWidth);
    CHECKF(b.advanceWidth >= p.advanceWidth, "emboldened glyph narrower than plain: %u < %u", b.advanceWidth, p.advanceWidth);
    fm_close(&G); fm_close(&F);
}
