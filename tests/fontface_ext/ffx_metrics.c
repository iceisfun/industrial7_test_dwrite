/* IDWriteFontFace1::GetMetrics (DWRITE_FONT_METRICS1), GetGdiCompatibleMetrics, GetCaretMetrics, IsMonospacedFont.
 * Derivations come from the default font metrics, GetFontMetrics,
 * IDWriteFontFace::GetCaretMetrics and GetGdiCompatibleMetrics. */
#include "ffx.h"
#include <math.h>

/* ---- default synthetic font (see ttfgen.h) --------------------------------------------------------------------- */
static void expect_default_m1(const DWRITE_FONT_METRICS1 *m) {
    EXPECT_EQ(m->designUnitsPerEm, 2048);
    EXPECT_EQ(m->ascent, 1800);               /* OS/2 usWinAscent: hhea 1700, typo 1600, win 1800 - all differ on purpose */
    EXPECT_EQ(m->descent, 450);               /* usWinDescent */
    EXPECT_EQ(m->lineGap, 0);                 /* hhea gap 90 + 1700 + 400 - 1800 - 450 = -60 -> clamped to 0 */
    EXPECT_EQ(m->capHeight, 1400);            /* OS/2 v4 sCapHeight */
    EXPECT_EQ(m->xHeight, 1000);              /* OS/2 v4 sxHeight */
    EXPECT_EQ(m->underlinePosition, -200);    /* post */
    EXPECT_EQ(m->underlineThickness, 100);
    EXPECT_EQ(m->strikethroughPosition, 600); /* OS/2 yStrikeoutPosition */
    EXPECT_EQ(m->strikethroughThickness, 100);
}
TEST(m1_default_core_fields) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    expect_default_m1(&m);
    ffx_close(&x);
}
TEST(m1_default_glyph_box_from_head) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    /* head xMin/yMin/xMax/yMax over all glyphs: notdef (100..1100, 0..1400), composite = 'A' + triangle lifted by 1400 -> yMax 2800 */
    EXPECT_EQ(m.glyphBoxLeft, 100);
    EXPECT_EQ(m.glyphBoxTop, 2800);
    EXPECT_EQ(m.glyphBoxRight, 1100);
    EXPECT_EQ(m.glyphBoxBottom, 0);
    ffx_close(&x);
}
TEST(m1_default_sub_super_script_from_os2) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    /* OS/2 ySubscriptXSize/YSize = upm/2, XOffset 0, YOffset = upm/7 = 292 (reported negated: subscript sits below the baseline);
     * ySuperscript sizes upm/2, XOffset 0, YOffset upm/3 = 682 */
    EXPECT_EQ(m.subscriptSizeX, 1024); EXPECT_EQ(m.subscriptSizeY, 1024);
    EXPECT_EQ(m.subscriptPositionX, 0); EXPECT_EQ(m.subscriptPositionY, -292);
    EXPECT_EQ(m.superscriptSizeX, 1024); EXPECT_EQ(m.superscriptSizeY, 1024);
    EXPECT_EQ(m.superscriptPositionX, 0); EXPECT_EQ(m.superscriptPositionY, 682);
    ffx_close(&x);
}
TEST(m1_default_has_typographic_metrics_false) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    EXPECT_EQ(m.hasTypographicMetrics, FALSE);   /* fsSelection bit 7 (USE_TYPO_METRICS) clear */
    ffx_close(&x);
}
TEST(m1_all_bytes_written) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; memset(&m, 0xCC, sizeof m);
    EXPECT_EQ(sizeof(DWRITE_FONT_METRICS1), 48);
    ffx_c_metrics1(x.f1, &m);
    /* GetMetrics copies 0x30 bytes; the BOOL is a clean 0/1 and no field keeps the 0xCC fill */
    CHECK(m.hasTypographicMetrics == 0 || m.hasTypographicMetrics == 1);
    CHECK(m.designUnitsPerEm != 0xCCCC && m.glyphBoxTop != (INT16)0xCCCC && m.superscriptSizeY != (INT16)0xCCCC);
    ffx_close(&x);
}
TEST(m1_equals_base_metrics_prefix) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS b, e; DWRITE_FONT_METRICS1 m;
    IDWriteFontFace_GetMetrics(x.f0, &b);
    ffx_metrics1(&x, &m);
    /* both entry points copy from the same GetFontMetrics blob */
    CHECK(!memcmp(&b, &m, sizeof b));
    (void)e;
    ffx_close(&x);
}
TEST(m1_same_through_every_interface) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 a, b, c, d;
    ffx_c_metrics1(x.f1, &a); ffx_c_metrics1((IDWriteFontFace1 *)x.f2, &b); ffx_c_metrics1((IDWriteFontFace1 *)x.f3, &c); ffx_c_metrics1((IDWriteFontFace1 *)x.f5, &d);
    CHECK(!memcmp(&a, &b, sizeof a) && !memcmp(&a, &c, sizeof a) && !memcmp(&a, &d, sizeof a));
    ffx_close(&x);
}
TEST(m1_repeatable) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 a, b; ffx_metrics1(&x, &a); ffx_metrics1(&x, &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    ffx_close(&x);
}

/* ---- ascent / descent / lineGap source selection ------------------------------------------------------------------ */
TEST(m1_use_typo_metrics_selects_typo_values) {
    ttf_spec s; ttf_default_spec(&s); s.fs_selection |= 0x80;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    /* bit 7 of fsSelection: ascent = sTypoAscender, descent = -sTypoDescender, lineGap = sTypoLineGap, flag reported */
    EXPECT_EQ(m.ascent, 1600); EXPECT_EQ(m.descent, 420); EXPECT_EQ(m.lineGap, 80);
    EXPECT_EQ(m.hasTypographicMetrics, TRUE);
    ffx_close(&x);
}
TEST(m1_use_typo_metrics_leaves_other_fields) {
    ttf_spec s; ttf_default_spec(&s); s.fs_selection |= 0x80;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    EXPECT_EQ(m.designUnitsPerEm, 2048); EXPECT_EQ(m.capHeight, 1400); EXPECT_EQ(m.xHeight, 1000);
    EXPECT_EQ(m.glyphBoxTop, 2800); EXPECT_EQ(m.subscriptPositionY, -292);
    ffx_close(&x);
}
static const struct { int ta, td, tg; } typo_cases[] = { { 1500, -300, 0 }, { 1900, -500, 200 }, { 1024, -1024, 512 }, { 100, -1, 1 }, { 1700, -400, 90 } };
TEST(m1_typo_values_table) {
    int i;
    for (i = 0; i < (int)(sizeof typo_cases / sizeof typo_cases[0]); i++) {
        ttf_spec s; ttf_default_spec(&s); s.fs_selection |= 0x80;
        s.typo_asc = typo_cases[i].ta; s.typo_desc = typo_cases[i].td; s.typo_gap = typo_cases[i].tg;
        ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
        DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
        t_check(m.ascent == typo_cases[i].ta && m.descent == -typo_cases[i].td && m.lineGap == typo_cases[i].tg, __FILE__, __LINE__, "typo case", "case %d: %d/%d/%d", i, m.ascent, m.descent, m.lineGap);
        ffx_close(&x);
    }
}
TEST(m1_win_metrics_with_positive_linegap) {
    ttf_spec s; ttf_default_spec(&s); s.win_asc = 1500; s.win_desc = 300;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    /* lineGap = hhea gap + hhea asc + |hhea desc| - winAsc - winDesc = 90 + 1700 + 400 - 1500 - 300 */
    EXPECT_EQ(m.ascent, 1500); EXPECT_EQ(m.descent, 300); EXPECT_EQ(m.lineGap, 390);
    ffx_close(&x);
}
static const struct { int wa, wd, ha, hd, hg; } win_cases[] = {
    { 2000, 500, 1700, -400, 90 }, { 1000, 200, 1000, -200, 0 }, { 1800, 450, 1800, -450, 100 }, { 500, 100, 2000, -800, 0 }, { 1500, 0, 1500, 0, 0 },
};
TEST(m1_win_linegap_formula_table) {
    int i;
    for (i = 0; i < (int)(sizeof win_cases / sizeof win_cases[0]); i++) {
        ttf_spec s; ttf_default_spec(&s); s.win_asc = win_cases[i].wa; s.win_desc = win_cases[i].wd;
        s.hhea_asc = win_cases[i].ha; s.hhea_desc = win_cases[i].hd; s.hhea_gap = win_cases[i].hg;
        ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
        DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
        int gap = win_cases[i].hg + win_cases[i].ha - win_cases[i].hd - win_cases[i].wa - win_cases[i].wd; if (gap < 0) gap = 0;
        t_check(m.ascent == win_cases[i].wa && m.descent == win_cases[i].wd && m.lineGap == gap, __FILE__, __LINE__, "win case", "case %d: got %d/%d/%d want %d/%d/%d", i, m.ascent, m.descent, m.lineGap, win_cases[i].wa, win_cases[i].wd, gap);
        ffx_close(&x);
    }
}
TEST(m1_without_os2_uses_hhea) {
    ttf_spec s; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    /* no OS/2: ascent = hhea ascender, descent = |hhea descender|, lineGap = hhea lineGap, no sub/superscript data, no typo flag */
    EXPECT_EQ(m.ascent, 1700); EXPECT_EQ(m.descent, 400); EXPECT_EQ(m.lineGap, 90);
    EXPECT_EQ(m.hasTypographicMetrics, FALSE);
    EXPECT_EQ(m.subscriptSizeX, 0); EXPECT_EQ(m.subscriptSizeY, 0); EXPECT_EQ(m.subscriptPositionX, 0); EXPECT_EQ(m.subscriptPositionY, 0);
    EXPECT_EQ(m.superscriptSizeX, 0); EXPECT_EQ(m.superscriptSizeY, 0); EXPECT_EQ(m.superscriptPositionX, 0); EXPECT_EQ(m.superscriptPositionY, 0);
    ffx_close(&x);
}
TEST(m1_without_os2_strikeout_defaults) {
    ttf_spec s; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    /* strikeout thickness = underline thickness, strikeout position = unitsPerEm / 3 */
    EXPECT_EQ(m.strikethroughThickness, m.underlineThickness);
    EXPECT_EQ(m.strikethroughPosition, 2048 / 3);
    ffx_close(&x);
}
TEST(m1_strikeout_size_zero_falls_back_to_underline_thickness) {
    ttf_spec s; ttf_default_spec(&s); s.strike_size = 0; s.strike_pos = 500; s.underline_thick = 77;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    EXPECT_EQ(m.strikethroughThickness, 77); EXPECT_EQ(m.strikethroughPosition, 500);
    ffx_close(&x);
}
static const struct { int up, ut, sp, ss; } line_cases[] = { { -100, 50, 400, 40 }, { -300, 150, 700, 120 }, { 0, 1, 1, 1 }, { 120, 10, -20, 5 }, { -2000, 2000, 2000, 2000 } };
TEST(m1_underline_strike_table) {
    int i;
    for (i = 0; i < (int)(sizeof line_cases / sizeof line_cases[0]); i++) {
        ttf_spec s; ttf_default_spec(&s); s.underline_pos = line_cases[i].up; s.underline_thick = line_cases[i].ut; s.strike_pos = line_cases[i].sp; s.strike_size = line_cases[i].ss;
        ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
        DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
        t_check(m.underlinePosition == line_cases[i].up && m.underlineThickness == line_cases[i].ut && m.strikethroughPosition == line_cases[i].sp && m.strikethroughThickness == line_cases[i].ss,
                __FILE__, __LINE__, "line metrics", "case %d: ul %d/%d st %d/%d", i, m.underlinePosition, m.underlineThickness, m.strikethroughPosition, m.strikethroughThickness);
        ffx_close(&x);
    }
}
TEST(m1_without_post_underline_defaults) {
    ttf_spec s; ttf_default_spec(&s); s.omit |= TTF_OMIT_POST;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    /* no post table: position = -((upm + 5) / 10), thickness = ((u - u/7) / 2 + u/7) >> 3 with u = upm + 7 */
    unsigned u = 2048 + 7, q = u / 7;
    EXPECT_EQ(m.underlinePosition, -((2048 + 5) / 10));
    EXPECT_EQ(m.underlineThickness, (((u - q) >> 1) + q) >> 3);
    ffx_close(&x);
}
TEST(m1_post_zero_thickness_uses_default) {
    ttf_spec s; ttf_default_spec(&s); s.underline_thick = 0; s.underline_pos = -150;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    unsigned u = 2048 + 7, q = u / 7;
    EXPECT_EQ(m.underlinePosition, -150);
    EXPECT_EQ(m.underlineThickness, (((u - q) >> 1) + q) >> 3);
    ffx_close(&x);
}
TEST(m1_cap_x_height_from_os2) {
    ttf_spec s; ttf_default_spec(&s); s.cap_height = 1300; s.x_height = 900;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    EXPECT_EQ(m.capHeight, 1300); EXPECT_EQ(m.xHeight, 900);
    ffx_close(&x);
}
TEST(m1_upm_variants) {
    static const int upms[] = { 1000, 1024, 2048, 4096, 2000 };
    int i;
    for (i = 0; i < 5; i++) {
        ttf_spec s; ttf_default_spec(&s); s.upm = upms[i];
        ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
        DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
        t_check(m.designUnitsPerEm == upms[i], __FILE__, __LINE__, "upm", "got %d want %d", m.designUnitsPerEm, upms[i]);
        /* sub/superscript sizes in the OS/2 table are upm/2 and offsets upm/7, upm/3 for this generator */
        t_check(m.subscriptSizeX == upms[i] / 2 && m.subscriptPositionY == -(upms[i] / 7) && m.superscriptPositionY == upms[i] / 3, __FILE__, __LINE__, "sub/super", "upm %d: %d %d %d", upms[i], m.subscriptSizeX, m.subscriptPositionY, m.superscriptPositionY);
        ffx_close(&x);
    }
}

/* ---- head / OS/2 patched in the blob ------------------------------------------------------------------------------ */
typedef struct { int hx0, hy0, hx1, hy1; } headbox;
static void patch_head(uint8_t *b, size_t n, void *ctx) {
    headbox *h = ctx; uint8_t *t = ffx_tbl(b, n, "head", NULL);
    if (!t) return;
    ffx_put16(t, 36, h->hx0); ffx_put16(t, 38, h->hy0); ffx_put16(t, 40, h->hx1); ffx_put16(t, 42, h->hy1);
}
static const headbox head_cases[] = { { -50, -300, 3000, 2500 }, { 0, 0, 0, 0 }, { -1000, -2000, 1000, 2000 }, { 32767, -32768, -1, 1 }, { 10, 20, 30, 40 } };
TEST(m1_glyph_box_follows_head_table) {
    int i;
    for (i = 0; i < (int)(sizeof head_cases / sizeof head_cases[0]); i++) {
        ttf_spec s; ttf_default_spec(&s); headbox h = head_cases[i];
        ffx x; if (!ffx_open_patched(&x, &s, DWRITE_FONT_SIMULATIONS_NONE, patch_head, &h)) { ffx_close(&x); return; }
        DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
        /* glyphBoxLeft = xMin, Top = yMax, Right = xMax, Bottom = yMin, straight from head */
        t_check(m.glyphBoxLeft == (short)h.hx0 && m.glyphBoxBottom == (short)h.hy0 && m.glyphBoxRight == (short)h.hx1 && m.glyphBoxTop == (short)h.hy1, __FILE__, __LINE__, "head box",
                "case %d: L%d T%d R%d B%d", i, m.glyphBoxLeft, m.glyphBoxTop, m.glyphBoxRight, m.glyphBoxBottom);
        ffx_close(&x);
    }
}
typedef struct { int sxs, sys, sxo, syo, pxs, pys, pxo, pyo; } os2sub;
static void patch_os2sub(uint8_t *b, size_t n, void *ctx) {
    os2sub *o = ctx; uint8_t *t = ffx_tbl(b, n, "OS/2", NULL);
    if (!t) return;
    ffx_put16(t, 10, o->sxs); ffx_put16(t, 12, o->sys); ffx_put16(t, 14, o->sxo); ffx_put16(t, 16, o->syo);
    ffx_put16(t, 18, o->pxs); ffx_put16(t, 20, o->pys); ffx_put16(t, 22, o->pxo); ffx_put16(t, 24, o->pyo);
}
static const os2sub sub_cases[] = {
    { 1300, 1400, 10, 140, 1200, 1100, -20, 480 }, { 1, 2, 3, 4, 5, 6, 7, 8 }, { 700, 650, 0, -200, 700, 650, 0, -100 }, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 900, 800, -30, 300, 910, 810, 30, 600 },
};
TEST(m1_sub_super_follow_os2_table) {
    int i;
    for (i = 0; i < (int)(sizeof sub_cases / sizeof sub_cases[0]); i++) {
        ttf_spec s; ttf_default_spec(&s); os2sub o = sub_cases[i];
        ffx x; if (!ffx_open_patched(&x, &s, DWRITE_FONT_SIMULATIONS_NONE, patch_os2sub, &o)) { ffx_close(&x); return; }
        DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
        /* subscript Y offset is negated; every other field is passed through (superscript Y offset is not negated) */
        int ok = m.subscriptSizeX == o.sxs && m.subscriptSizeY == o.sys && m.subscriptPositionX == o.sxo && m.subscriptPositionY == -o.syo &&
                 m.superscriptSizeX == o.pxs && m.superscriptSizeY == o.pys && m.superscriptPositionX == o.pxo && m.superscriptPositionY == o.pyo;
        t_check(ok, __FILE__, __LINE__, "sub/super", "case %d: sub %d,%d @%d,%d super %d,%d @%d,%d", i, m.subscriptSizeX, m.subscriptSizeY, m.subscriptPositionX, m.subscriptPositionY,
                m.superscriptSizeX, m.superscriptSizeY, m.superscriptPositionX, m.superscriptPositionY);
        ffx_close(&x);
    }
}

/* ---- simulations --------------------------------------------------------------------------------------------------- */
static short obl(int v) { return (short)((unsigned)(v * 0x5700) >> 16); }
static void expect_oblique_adjusted(const DWRITE_FONT_METRICS1 *base, const DWRITE_FONT_METRICS1 *m) {
    DWRITE_FONT_METRICS1 e = *base;
    /* GetFontMetrics: with the oblique simulation x-extents grow by y * 0x5700 / 65536 (~0.34) */
    e.glyphBoxRight = (INT16)(base->glyphBoxRight + obl(base->glyphBoxTop));
    e.glyphBoxLeft = (INT16)(base->glyphBoxLeft + obl(base->glyphBoxBottom));
    e.subscriptPositionX = (INT16)(base->subscriptPositionX + obl(base->subscriptPositionY));
    e.superscriptPositionX = (INT16)(base->superscriptPositionX + obl(base->superscriptPositionY));
    CHECK(!memcmp(&e, m, sizeof e));
    EXPECT_EQ(m->glyphBoxRight, e.glyphBoxRight); EXPECT_EQ(m->glyphBoxLeft, e.glyphBoxLeft);
    EXPECT_EQ(m->subscriptPositionX, e.subscriptPositionX); EXPECT_EQ(m->superscriptPositionX, e.superscriptPositionX);
}
TEST(m1_oblique_simulation_adjusts_x_extents) {
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE) : 0;
    if (oa && ob) { DWRITE_FONT_METRICS1 m0, m1; ffx_metrics1(&a, &m0); ffx_metrics1(&b, &m1); expect_oblique_adjusted(&m0, &m1); t_note("oblique glyph box right %d -> %d", m0.glyphBoxRight, m1.glyphBoxRight); }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(m1_bold_oblique_simulation_same_as_oblique) {
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE) : 0;
    if (oa && ob) { DWRITE_FONT_METRICS1 m0, m1; ffx_metrics1(&a, &m0); ffx_metrics1(&b, &m1); CHECK(!memcmp(&m0, &m1, sizeof m0)); }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(m1_bold_simulation_leaves_metrics_unchanged) {
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_BOLD) : 0;
    if (oa && ob) { DWRITE_FONT_METRICS1 m0, m1; ffx_metrics1(&a, &m0); ffx_metrics1(&b, &m1); CHECK(!memcmp(&m0, &m1, sizeof m0)); }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(m1_oblique_italic_font_with_simulation) {
    /* the simulation flag is applied whatever the style of the font bytes */
    ttf_spec s; ttf_style_spec(&s, "ObFam", 400, 5, 1);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE) : 0;
    if (oa && ob) { DWRITE_FONT_METRICS1 m0, m1; ffx_metrics1(&a, &m0); ffx_metrics1(&b, &m1); expect_oblique_adjusted(&m0, &m1); }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}

/* ---- caret metrics ------------------------------------------------------------------------------------------------- */
static void get_caret(ffx *x, DWRITE_CARET_METRICS *c) { memset(c, 0xCC, sizeof *c); IDWriteFontFace1_GetCaretMetrics(x->f1, c); }
TEST(caret_upright_default) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_CARET_METRICS c; get_caret(&x, &c);
    EXPECT_EQ(c.slopeRise, 1); EXPECT_EQ(c.slopeRun, 0); EXPECT_EQ(c.offset, 0);
    EXPECT_EQ(sizeof c, 6);
    ffx_close(&x);
}
TEST(caret_italic_style_font) {
    ttf_spec s; ttf_style_spec(&s, "ItFam", 400, 5, 1);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_CARET_METRICS c; get_caret(&x, &c);
    /* hhea caretSlopeRise/Run of the italic generator: 1000 / 213 */
    EXPECT_EQ(c.slopeRise, 1000); EXPECT_EQ(c.slopeRun, 213); EXPECT_EQ(c.offset, 0);
    ffx_close(&x);
}
typedef struct { int rise, run, off; } caretp;
static void patch_caret(uint8_t *b, size_t n, void *ctx) {
    caretp *c = ctx; uint8_t *t = ffx_tbl(b, n, "hhea", NULL);
    if (!t) return;
    ffx_put16(t, 18, c->rise); ffx_put16(t, 20, c->run); ffx_put16(t, 22, c->off);
}
static const caretp caret_cases[] = { { 1, 0, 0 }, { 5, 1, 7 }, { 1000, 213, -9 }, { 0, 0, 0 }, { -3, 2, 100 }, { 32767, -32768, -1 }, { 2048, 400, 33 } };
TEST(caret_follows_hhea_table) {
    int i;
    for (i = 0; i < (int)(sizeof caret_cases / sizeof caret_cases[0]); i++) {
        ttf_spec s; ttf_default_spec(&s); caretp cp = caret_cases[i];
        ffx x; if (!ffx_open_patched(&x, &s, DWRITE_FONT_SIMULATIONS_NONE, patch_caret, &cp)) { ffx_close(&x); return; }
        DWRITE_CARET_METRICS c; get_caret(&x, &c);
        t_check(c.slopeRise == (short)cp.rise && c.slopeRun == (short)cp.run && c.offset == (short)cp.off, __FILE__, __LINE__, "caret", "case %d: %d %d %d", i, c.slopeRise, c.slopeRun, c.offset);
        ffx_close(&x);
    }
}
TEST(caret_oblique_simulation_tilts_upright_font) {
    ttf_spec s; ttf_default_spec(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE)) { ffx_close(&x); return; }
    DWRITE_CARET_METRICS c; get_caret(&x, &c);
    CHECK(c.slopeRise > 0); CHECK(c.slopeRun > 0);
    t_note("oblique-simulated caret: rise %d run %d offset %d", c.slopeRise, c.slopeRun, c.offset);
    if (c.slopeRise > 0) { double r = (double)c.slopeRun / c.slopeRise; CHECK(r > 0.25 && r < 0.45); }   /* simulated slant is ~0x5700/65536 = 0.34 */
    ffx_close(&x);
}
TEST(caret_bold_simulation_unchanged) {
    ttf_spec s; ttf_default_spec(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_BOLD)) { ffx_close(&x); return; }
    DWRITE_CARET_METRICS c; get_caret(&x, &c);
    EXPECT_EQ(c.slopeRise, 1); EXPECT_EQ(c.slopeRun, 0); EXPECT_EQ(c.offset, 0);
    ffx_close(&x);
}
TEST(caret_same_through_interfaces) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_CARET_METRICS a, b, c; memset(&a, 1, sizeof a); memset(&b, 2, sizeof b); memset(&c, 3, sizeof c);
    IDWriteFontFace1_GetCaretMetrics(x.f1, &a); IDWriteFontFace3_GetCaretMetrics(x.f3, &b); IDWriteFontFace5_GetCaretMetrics(x.f5, &c);
    CHECK(!memcmp(&a, &b, sizeof a) && !memcmp(&a, &c, sizeof a));
    ffx_close(&x);
}

/* ---- IsMonospacedFont ---------------------------------------------------------------------------------------------- */
TEST(mono_default_font_false) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace1_IsMonospacedFont(x.f1), FALSE);   /* post.isFixedPitch = 0, advances 1200/1000/500 */
    ffx_close(&x);
}
TEST(mono_is_fixed_pitch_flag_note) {
    ttf_spec s; ttf_default_spec(&s); s.is_fixed_pitch = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    BOOL r = IDWriteFontFace1_IsMonospacedFont(x.f1);
    CHECK(r == TRUE || r == FALSE);
    t_note("post.isFixedPitch=1 with proportional advances -> IsMonospacedFont %d", r);
    ffx_close(&x);
}
TEST(mono_returns_clean_bool) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    BOOL r = IDWriteFontFace1_IsMonospacedFont(x.f1);
    CHECK(r == 0 || r == 1);          /* (flags >> 1) & 1 */
    ffx_close(&x);
}
#define SYS_MONO(NAME, FAM, WANT) \
TEST(NAME) { ffx x; if (!ffx_open_sys(&x, FAM, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; } \
    BOOL r = IDWriteFontFace1_IsMonospacedFont(x.f1); \
    if ((WANT) >= 0) { EXPECT_EQ(r, (WANT)); } else t_note(FAM_NAME_FMT " monospaced=%d", r); ffx_close(&x); }
#define FAM_NAME_FMT "font"
SYS_MONO(mono_courier_new_true, L"Courier New", TRUE)
SYS_MONO(mono_consolas_true, L"Consolas", TRUE)
SYS_MONO(mono_arial_false, L"Arial", FALSE)
SYS_MONO(mono_times_new_roman_false, L"Times New Roman", FALSE)
SYS_MONO(mono_segoe_ui_false, L"Segoe UI", FALSE)
SYS_MONO(mono_tahoma_false, L"Tahoma", FALSE)
SYS_MONO(mono_lucida_console_note, L"Lucida Console", -1)
SYS_MONO(mono_ms_gothic_note, L"MS Gothic", -1)

/* ---- real fonts: metrics must be consistent with the font tables ---------------------------------------------------- */
static void check_m1_against_tables(ffx *x) {
    DWRITE_FONT_METRICS1 m; ffx_metrics1(x, &m);
    UINT32 hl = 0, ol = 0, hhl = 0, pl = 0;
    uint8_t *head = dw_face_table(x->f0, DW_TAG('h','e','a','d'), &hl), *os2 = dw_face_table(x->f0, DW_TAG('O','S','/','2'), &ol),
            *hhea = dw_face_table(x->f0, DW_TAG('h','h','e','a'), &hhl), *post = dw_face_table(x->f0, DW_TAG('p','o','s','t'), &pl);
    if (head && hl >= 54) {
        EXPECT_EQ(m.designUnitsPerEm, ffx_getu16(head, 18));
        EXPECT_EQ(m.glyphBoxLeft, ffx_get16(head, 36)); EXPECT_EQ(m.glyphBoxBottom, ffx_get16(head, 38));
        EXPECT_EQ(m.glyphBoxRight, ffx_get16(head, 40)); EXPECT_EQ(m.glyphBoxTop, ffx_get16(head, 42));
    } else t_note("no usable head table");
    if (os2 && ol >= 78) {
        EXPECT_EQ(m.subscriptSizeX, ffx_get16(os2, 10)); EXPECT_EQ(m.subscriptSizeY, ffx_get16(os2, 12));
        EXPECT_EQ(m.subscriptPositionX, ffx_get16(os2, 14)); EXPECT_EQ(m.subscriptPositionY, -ffx_get16(os2, 16));
        EXPECT_EQ(m.superscriptSizeX, ffx_get16(os2, 18)); EXPECT_EQ(m.superscriptSizeY, ffx_get16(os2, 20));
        EXPECT_EQ(m.superscriptPositionX, ffx_get16(os2, 22)); EXPECT_EQ(m.superscriptPositionY, ffx_get16(os2, 24));
        BOOL typo = (ffx_getu16(os2, 62) & 0x80) != 0;
        EXPECT_EQ(m.hasTypographicMetrics, typo);
        if (typo) { EXPECT_EQ(m.ascent, ffx_getu16(os2, 68)); EXPECT_EQ(m.descent, -ffx_get16(os2, 70)); EXPECT_EQ(m.lineGap, ffx_get16(os2, 72)); }
        else t_note("USE_TYPO_METRICS clear: ascent %d descent %d gap %d (win %u/%u)", m.ascent, m.descent, m.lineGap, ffx_getu16(os2, 74), ffx_getu16(os2, 76));
    } else t_note("no usable OS/2 table");
    if (post && pl >= 12) {
        EXPECT_EQ(m.underlinePosition, ffx_get16(post, 8)); 
        if (ffx_getu16(post, 10)) EXPECT_EQ(m.underlineThickness, ffx_getu16(post, 10));
    }
    t_note("upm %d asc %d desc %d gap %d cap %d xh %d box [%d %d %d %d]", m.designUnitsPerEm, m.ascent, m.descent, m.lineGap, m.capHeight, m.xHeight, m.glyphBoxLeft, m.glyphBoxBottom, m.glyphBoxRight, m.glyphBoxTop);
    DWRITE_FONT_METRICS b; IDWriteFontFace_GetMetrics(x->f0, &b);
    CHECK(!memcmp(&b, &m, sizeof b));
    free(head); free(os2); free(hhea); free(post);
}
#define SYS_TABLES(NAME, FAM, W, S) \
TEST(NAME) { ffx x; if (!ffx_open_sys(&x, FAM, W, DWRITE_FONT_STRETCH_NORMAL, S)) { ffx_close(&x); return; } check_m1_against_tables(&x); ffx_close(&x); }
SYS_TABLES(m1_tables_arial, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_arial_bold, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_arial_italic, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_ITALIC)
SYS_TABLES(m1_tables_times, L"Times New Roman", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_courier, L"Courier New", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_segoe_ui, L"Segoe UI", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_tahoma, L"Tahoma", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_consolas, L"Consolas", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_marlett, L"Marlett", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_segoe_symbol, L"Segoe UI Symbol", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_ms_gothic, L"MS Gothic", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)
SYS_TABLES(m1_tables_bahnschrift, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL)

TEST(m1_arial_values_note) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    UINT32 ol = 0; uint8_t *os2 = dw_face_table(x.f0, DW_TAG('O','S','/','2'), &ol);
    if (os2 && ol >= 78 && !(ffx_getu16(os2, 62) & 0x80)) { EXPECT_EQ(m.ascent, ffx_getu16(os2, 74)); EXPECT_EQ(m.descent, ffx_getu16(os2, 76)); }
    t_note("Arial: asc %d desc %d gap %d cap %d xh %d ul %d/%d st %d/%d", m.ascent, m.descent, m.lineGap, m.capHeight, m.xHeight, m.underlinePosition, m.underlineThickness, m.strikethroughPosition, m.strikethroughThickness);
    free(os2); ffx_close(&x);
}
TEST(caret_arial_italic_vs_hhea) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC)) { ffx_close(&x); return; }
    DWRITE_CARET_METRICS c; get_caret(&x, &c);
    UINT32 l = 0; uint8_t *hhea = dw_face_table(x.f0, DW_TAG('h','h','e','a'), &l);
    if (hhea && l >= 24) { EXPECT_EQ(c.slopeRise, ffx_get16(hhea, 18)); EXPECT_EQ(c.slopeRun, ffx_get16(hhea, 20)); EXPECT_EQ(c.offset, ffx_get16(hhea, 22)); }
    CHECK(c.slopeRun != 0 || (IDWriteFontFace3_GetStyle(x.f3) == DWRITE_FONT_STYLE_NORMAL));
    t_note("Arial italic caret %d/%d off %d", c.slopeRise, c.slopeRun, c.offset);
    free(hhea); ffx_close(&x);
}
TEST(caret_arial_upright_vs_hhea) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    DWRITE_CARET_METRICS c; get_caret(&x, &c);
    UINT32 l = 0; uint8_t *hhea = dw_face_table(x.f0, DW_TAG('h','h','e','a'), &l);
    if (hhea && l >= 24) { EXPECT_EQ(c.slopeRise, ffx_get16(hhea, 18)); EXPECT_EQ(c.slopeRun, ffx_get16(hhea, 20)); EXPECT_EQ(c.offset, ffx_get16(hhea, 22)); }
    EXPECT_EQ(c.slopeRun, 0);
    free(hhea); ffx_close(&x);
}

/* ---- GetGdiCompatibleMetrics ---------------------------------------------------------------------------------------- */
static HRESULT gdi_m1(ffx *x, float em, float ppd, const DWRITE_MATRIX *t, DWRITE_FONT_METRICS1 *m) {
    memset(m, 0xCC, sizeof *m);
    return ffx_c_gdi_metrics1(x->f1, em, ppd, t, m);
}
static int m1_zero(const DWRITE_FONT_METRICS1 *m) { static const char z[sizeof(DWRITE_FONT_METRICS1)] = { 0 }; return !memcmp(m, z, sizeof *m); }
TEST(gdi_m1_invalid_args_zero_output) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const struct { float em, ppd; } bad[] = { { 0.0f, 1.0f }, { -1.0f, 1.0f }, { 16.0f, 0.0f }, { 16.0f, -1.0f }, { 0.0f, 0.0f }, { -16.0f, -1.0f }, { -0.0f, 1.0f } };
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        DWRITE_FONT_METRICS1 m; HRESULT hr = gdi_m1(&x, bad[i].em, bad[i].ppd, NULL, &m);
        t_check(hr == (HRESULT)0x80070057, __FILE__, __LINE__, "gdi m1", "em %g ppd %g -> %s", bad[i].em, bad[i].ppd, dw_hr_name(hr));
        t_check(m1_zero(&m), __FILE__, __LINE__, "gdi m1 zero", "output not zeroed for em %g ppd %g", bad[i].em, bad[i].ppd);
    }
    ffx_close(&x);
}
TEST(gdi_m1_nan_matrix_rejected) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    float nan = nanf("");
    DWRITE_MATRIX m0 = { 1, 0, 0, 1, 0, 0 };
    float *f = &m0.m11; int i;
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX t = m0; ((float *)&t)[i] = nan;
        DWRITE_FONT_METRICS1 m; HRESULT hr = gdi_m1(&x, 16.0f, 1.0f, &t, &m);
        t_check(hr == (HRESULT)0x80070057 && m1_zero(&m), __FILE__, __LINE__, "NaN matrix", "element %d -> %s", i, dw_hr_name(hr));
    }
    (void)f;
    ffx_close(&x);
}
TEST(gdi_m1_null_transform_ok) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 m; CHECK_HR_OK(gdi_m1(&x, 16.0f, 1.0f, NULL, &m));
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    ffx_close(&x);
}
TEST(gdi_m1_identity_transform_equals_null) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 };
    DWRITE_FONT_METRICS1 a, b; CHECK_HR_OK(gdi_m1(&x, 16.0f, 1.0f, NULL, &a)); CHECK_HR_OK(gdi_m1(&x, 16.0f, 1.0f, &id, &b));
    CHECK(!memcmp(&a, &b, sizeof a));
    ffx_close(&x);
}
static void check_pixel_grid(const DWRITE_FONT_METRICS1 *r, const DWRITE_FONT_METRICS1 *o, int unit) {
    /* every rounded design value is a whole number of pixels (unit design units per pixel), within half a pixel of the original */
    struct { const char *n; int rv, ov; } f[] = {
        { "ascent", r->ascent, o->ascent }, { "descent", r->descent, o->descent }, { "lineGap", r->lineGap, o->lineGap },
        { "capHeight", r->capHeight, o->capHeight }, { "xHeight", r->xHeight, o->xHeight },
        { "underlinePosition", r->underlinePosition, o->underlinePosition }, { "underlineThickness", r->underlineThickness, o->underlineThickness },
        { "strikethroughPosition", r->strikethroughPosition, o->strikethroughPosition }, { "strikethroughThickness", r->strikethroughThickness, o->strikethroughThickness },
        { "glyphBoxLeft", r->glyphBoxLeft, o->glyphBoxLeft }, { "glyphBoxTop", r->glyphBoxTop, o->glyphBoxTop }, { "glyphBoxRight", r->glyphBoxRight, o->glyphBoxRight }, { "glyphBoxBottom", r->glyphBoxBottom, o->glyphBoxBottom },
        { "subscriptPositionX", r->subscriptPositionX, o->subscriptPositionX }, { "subscriptPositionY", r->subscriptPositionY, o->subscriptPositionY },
        { "subscriptSizeX", r->subscriptSizeX, o->subscriptSizeX }, { "subscriptSizeY", r->subscriptSizeY, o->subscriptSizeY },
        { "superscriptPositionX", r->superscriptPositionX, o->superscriptPositionX }, { "superscriptPositionY", r->superscriptPositionY, o->superscriptPositionY },
        { "superscriptSizeX", r->superscriptSizeX, o->superscriptSizeX }, { "superscriptSizeY", r->superscriptSizeY, o->superscriptSizeY },
    };
    int i;
    for (i = 0; i < (int)(sizeof f / sizeof f[0]); i++) {
        t_check(f[i].rv % unit == 0, __FILE__, __LINE__, f[i].n, "%s=%d is not a multiple of %d design units/pixel", f[i].n, f[i].rv, unit);
        t_check(abs(f[i].rv - f[i].ov) <= unit / 2 + 1, __FILE__, __LINE__, f[i].n, "%s moved from %d to %d (> half a pixel of %d)", f[i].n, f[i].ov, f[i].rv, unit);
    }
}
TEST(gdi_m1_rounds_to_pixel_grid_16ppem) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 o, r; ffx_metrics1(&x, &o); CHECK_HR_OK(gdi_m1(&x, 16.0f, 1.0f, NULL, &r));
    EXPECT_EQ(r.designUnitsPerEm, 2048);
    check_pixel_grid(&r, &o, 2048 / 16);
    EXPECT_EQ(r.hasTypographicMetrics, o.hasTypographicMetrics);
    ffx_close(&x);
}
TEST(gdi_m1_rounds_to_pixel_grid_other_sizes) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const int ppems[] = { 8, 10, 32, 64, 128 };   /* divisors of 2048 only: 8, 32, 64, 128; 10 gives a fractional unit and is skipped below */
    DWRITE_FONT_METRICS1 o; ffx_metrics1(&x, &o);
    int i;
    for (i = 0; i < 5; i++) {
        if (2048 % ppems[i]) continue;
        DWRITE_FONT_METRICS1 r; CHECK_HR_OK(gdi_m1(&x, (float)ppems[i], 1.0f, NULL, &r));
        check_pixel_grid(&r, &o, 2048 / ppems[i]);
    }
    ffx_close(&x);
}
TEST(gdi_m1_pixels_per_dip_scales_em) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 a, b; CHECK_HR_OK(gdi_m1(&x, 16.0f, 1.0f, NULL, &a)); CHECK_HR_OK(gdi_m1(&x, 8.0f, 2.0f, NULL, &b));
    CHECK(!memcmp(&a, &b, sizeof a));    /* physical size = em * pixelsPerDip */
    ffx_close(&x);
}
TEST(gdi_m1_scale_transform_equals_bigger_em) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_MATRIX t = { 2, 0, 0, 2, 0, 0 };
    DWRITE_FONT_METRICS1 a, b; CHECK_HR_OK(gdi_m1(&x, 16.0f, 1.0f, NULL, &a)); CHECK_HR_OK(gdi_m1(&x, 8.0f, 1.0f, &t, &b));
    CHECK(!memcmp(&a, &b, sizeof a));
    ffx_close(&x);
}
TEST(gdi_m1_em_equals_upm_is_identity_rounding) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 o, r; ffx_metrics1(&x, &o); CHECK_HR_OK(gdi_m1(&x, 2048.0f, 1.0f, NULL, &r));
    CHECK(!memcmp(&o, &r, sizeof o));    /* one design unit per pixel: rounding is a no-op */
    ffx_close(&x);
}
TEST(gdi_m1_large_and_tiny_sizes_succeed) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const float ems[] = { 0.01f, 0.5f, 1.0f, 3.0f, 100.0f, 255.0f, 256.0f, 1000.0f, 10000.0f };
    int i;
    for (i = 0; i < 9; i++) {
        DWRITE_FONT_METRICS1 m; HRESULT hr = gdi_m1(&x, ems[i], 1.0f, NULL, &m);
        t_check(hr == S_OK && m.designUnitsPerEm == 2048, __FILE__, __LINE__, "gdi size", "em %g -> %s upm %d", ems[i], dw_hr_name(hr), m.designUnitsPerEm);
    }
    ffx_close(&x);
}
TEST(gdi_m1_base_interface_prefix_matches) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS b; DWRITE_FONT_METRICS1 m;
    memset(&b, 0xCC, sizeof b);
    CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(x.f0, 16.0f, 1.0f, NULL, &b));
    CHECK_HR_OK(gdi_m1(&x, 16.0f, 1.0f, NULL, &m));
    CHECK(!memcmp(&b, &m, sizeof b));
    /* the base entry point zeroes its output first and fails like the extended one */
    memset(&b, 0xCC, sizeof b);
    HRESULT hr = IDWriteFontFace_GetGdiCompatibleMetrics(x.f0, -1.0f, 1.0f, NULL, &b);
    CHECK_HR(hr, 0x80070057);
    { static const char z[sizeof(DWRITE_FONT_METRICS)] = { 0 }; CHECK(!memcmp(&b, z, sizeof b)); }
    ffx_close(&x);
}
TEST(gdi_m1_simulated_oblique_has_adjusted_box) {
    ttf_spec s; ttf_default_spec(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 o, r; ffx_metrics1(&x, &o); CHECK_HR_OK(gdi_m1(&x, 2048.0f, 1.0f, NULL, &r));
    CHECK(!memcmp(&o, &r, sizeof o));    /* GDI-compatible metrics start from the (already simulated) metrics */
    ffx_close(&x);
}
TEST(gdi_m1_arial_16ppem_note) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    DWRITE_FONT_METRICS1 o, r; ffx_metrics1(&x, &o);
    CHECK_HR_OK(gdi_m1(&x, 16.0f, 1.0f, NULL, &r));
    t_note("Arial 16ppem gdi: asc %d (%d) desc %d (%d) gap %d (%d)", r.ascent, o.ascent, r.descent, o.descent, r.lineGap, o.lineGap);
    /* Arial has VDMX data: the GDI ascent/descent at 16 ppem come from there; either way the line is at least one pixel */
    CHECK(r.ascent + r.descent >= 2048 / 16);
    ffx_close(&x);
}
