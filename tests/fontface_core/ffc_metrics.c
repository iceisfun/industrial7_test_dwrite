/* IDWriteFontFace::GetMetrics and IDWriteFontFace1::GetMetrics (DWRITE_FONT_METRICS / DWRITE_FONT_METRICS1) on synthetic fonts
 * with controlled head / hhea / OS/2 / post values.
 *
 * Derivation (Windows 10 InitializeDefaultFontMetrics / FontFaceLight::GetFontMetrics decompilation):
 *   designUnitsPerEm = head.unitsPerEm (0x800 when head is missing);
 *   OS/2 fsSelection bit 7 (USE_TYPO_METRICS): ascent = typoAscender, descent = -typoDescender (clamped to >= 0), lineGap = typoLineGap,
 *       hasTypographicMetrics = TRUE;
 *   otherwise, when OS/2 winAscent or winDescent is non-zero and the font has no mort/morx-only layout (or has GSUB/GPOS/GDEF):
 *       ascent = winAscent, descent = winDescent, lineGap = max(0, hhea.ascender + |hhea.descender| + max(hhea.lineGap, 0) - winAscent - winDescent);
 *   otherwise ascent = hhea.ascender, descent = |hhea.descender|, lineGap = max(hhea.lineGap, 0);
 *   underline: post.underlinePosition / underlineThickness (thickness 0 or no post => (upm + 7) / 14; position without post => -((upm + 5) / 10));
 *   strikethrough: OS/2 yStrikeoutPosition / yStrikeoutSize (size <= 0 => underline thickness; without OS/2: size = underline thickness, position = upm / 3);
 *   capHeight / xHeight: OS/2 v2+ sCapHeight / sxHeight when ValidateHeights accepts them; fallbacks otherwise;
 *   METRICS1 glyph box = head xMin/yMax/xMax/yMin, sub/superscript from OS/2; the OBLIQUE simulation shears X positions of the glyph box and
 *   of the sub/superscript positions by (y * 0x5700) >> 16. */
#include "ffc.h"

typedef struct { int upm, asc, desc, gap, cap, xh, up, ut, sp, st; } ffc_exp;

static void ffc_chk_m(const DWRITE_FONT_METRICS *m, const ffc_exp *e, const char *tag) {
    CHECKF(m->designUnitsPerEm == e->upm, "%s upm %u want %d", tag, m->designUnitsPerEm, e->upm);
    CHECKF(m->ascent == e->asc, "%s ascent %u want %d", tag, m->ascent, e->asc);
    CHECKF(m->descent == e->desc, "%s descent %u want %d", tag, m->descent, e->desc);
    CHECKF(m->lineGap == e->gap, "%s lineGap %d want %d", tag, m->lineGap, e->gap);
    CHECKF(m->capHeight == e->cap, "%s capHeight %u want %d", tag, m->capHeight, e->cap);
    CHECKF(m->xHeight == e->xh, "%s xHeight %u want %d", tag, m->xHeight, e->xh);
    CHECKF(m->underlinePosition == e->up, "%s underlinePosition %d want %d", tag, m->underlinePosition, e->up);
    CHECKF(m->underlineThickness == e->ut, "%s underlineThickness %u want %d", tag, m->underlineThickness, e->ut);
    CHECKF(m->strikethroughPosition == e->sp, "%s strikethroughPosition %d want %d", tag, m->strikethroughPosition, e->sp);
    CHECKF(m->strikethroughThickness == e->st, "%s strikethroughThickness %u want %d", tag, m->strikethroughThickness, e->st);
}
/* Opens spec, fetches v0 metrics (and v1 metrics when the interface exists). Returns 0 after t_skip. */
static int ffc_get(IDWriteFactory *f, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim, DWRITE_FONT_METRICS *m, DWRITE_FONT_METRICS1 *m1, int *have1) {
    dwx_font h; IDWriteFontFace *face = ffc_open(&h, f, s, sim); IDWriteFontFace1 *f1;
    if (!face) return 0;
    memset(m, 0xCC, sizeof *m);
    IDWriteFontFace_GetMetrics(face, m);
    if (have1) *have1 = 0;
    if (m1) memset(m1, 0xCC, sizeof *m1);
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1);
    if (f1) { if (m1) IDWriteFontFace1_GetMetrics(f1, m1); if (have1) *have1 = 1; IDWriteFontFace1_Release(f1); }
    dwx_font_close(&h);
    return 1;
}
/* the default font's expected metrics (win metrics path) */
static ffc_exp ffc_def_exp(void) { ffc_exp e = { 2048, 1800, 450, 0, 1400, 1000, -200, 100, 600, 100 }; return e; }

/* ---- defaults ------------------------------------------------------------------------------------------------------------ */
FTEST(ffcm_default_all_fields) {
    ttf_spec s; DWRITE_FONT_METRICS m; ffc_exp e = ffc_def_exp(); ttf_default_spec(&s);
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    ffc_chk_m(&m, &e, "default");
}
FTEST(ffcm_default_win_metrics_used_not_hhea_or_typo) {
    /* the default font deliberately differs in hhea (1700/400/90), typo (1600/420/80) and win (1800/450) */
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s);
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    CHECK(m.ascent != 1700 && m.ascent != 1600);
    CHECK(m.descent != 400 && m.descent != 420);
    EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450);
}
FTEST(ffcm_default_linegap_clamped_to_zero) {
    /* hhea 1700 + 400 + 90 - win 1800 - 450 = -60 -> 0 */
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s);
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.lineGap, 0);
}
FTEST(ffcm_v1_prefix_equals_v0) {
    ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1; ttf_default_spec(&s);
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) return;
    if (!h1) { t_skip("IDWriteFontFace1 unsupported"); return; }
    CHECK(!memcmp(&m, &m1, sizeof m));
}
FTEST(ffcm_v1_default_sub_super) {
    ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1; ttf_default_spec(&s);
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) return;
    if (!h1) { t_skip("IDWriteFontFace1 unsupported"); return; }
    /* synthetic OS/2: sub size upm/2, sub y offset upm/7 (reported negated), super size upm/2, super y offset upm/3, x offsets 0 */
    EXPECT_EQ(m1.subscriptPositionX, 0); EXPECT_EQ(m1.subscriptPositionY, -(2048 / 7));
    EXPECT_EQ(m1.subscriptSizeX, 1024); EXPECT_EQ(m1.subscriptSizeY, 1024);
    EXPECT_EQ(m1.superscriptPositionX, 0); EXPECT_EQ(m1.superscriptPositionY, 2048 / 3);
    EXPECT_EQ(m1.superscriptSizeX, 1024); EXPECT_EQ(m1.superscriptSizeY, 1024);
}
FTEST(ffcm_v1_glyph_box_from_head) {
    ttf_spec s; size_t n; uint8_t *b; const uint8_t *hd; unsigned hl; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1;
    ttf_default_spec(&s);
    b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    hd = ffc_blob_table(b, n, "head", &hl);
    if (!hd || hl < 44) { free(b); t_skip("head"); return; }
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) { free(b); return; }
    if (!h1) { free(b); t_skip("IDWriteFontFace1 unsupported"); return; }
    EXPECT_EQ(m1.glyphBoxLeft, ffc_s16(hd + 36)); EXPECT_EQ(m1.glyphBoxBottom, ffc_s16(hd + 38));
    EXPECT_EQ(m1.glyphBoxRight, ffc_s16(hd + 40)); EXPECT_EQ(m1.glyphBoxTop, ffc_s16(hd + 42));
    t_note("glyph box L%d B%d R%d T%d", m1.glyphBoxLeft, m1.glyphBoxBottom, m1.glyphBoxRight, m1.glyphBoxTop);
    free(b);
}
FTEST(ffcm_v1_has_typographic_false_by_default) {
    ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1; ttf_default_spec(&s);
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) return;
    if (!h1) { t_skip("IDWriteFontFace1 unsupported"); return; }
    EXPECT_EQ(m1.hasTypographicMetrics, 0);
}
FTEST(ffcm_repeated_calls_identical) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_FONT_METRICS a, b; int i; if (!face) return;
    IDWriteFontFace_GetMetrics(face, &a);
    for (i = 0; i < 20; i++) { IDWriteFontFace_GetMetrics(face, &b); CHECK(!memcmp(&a, &b, sizeof a)); }
    dwx_font_close(&h);
}
FTEST(ffcm_two_faces_same_font_same_metrics) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f), *x = NULL; DWRITE_FONT_METRICS a, b; if (!face) return;
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &x));
    IDWriteFontFace_GetMetrics(face, &a); if (x) IDWriteFontFace_GetMetrics(x, &b);
    if (x) CHECK(!memcmp(&a, &b, sizeof a));   /* bold simulation does not touch the font metrics */
    DW_RELEASE(x);
    dwx_font_close(&h);
}
FTEST(ffcm_style_does_not_change_metrics) {
    /* style names / weight / macStyle are not inputs of the metric derivation */
    static const int w[] = { 100, 400, 700, 900 }; int i;
    for (i = 0; i < 4; i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ffc_exp e = ffc_def_exp();
        ttf_style_spec(&s, "FfcMet", w[i], 5, i & 1);
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        ffc_chk_m(&m, &e, "styled");
    }
}

/* ---- USE_TYPO_METRICS --------------------------------------------------------------------------------------------------- */
FTEST(ffcm_typo_bit_selects_typo_metrics) {
    ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1; ffc_exp e = { 2048, 1600, 420, 80, 1400, 1000, -200, 100, 600, 100 };
    ttf_default_spec(&s); s.fs_selection |= 0x80;
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) return;
    ffc_chk_m(&m, &e, "typo");
    if (h1) EXPECT_EQ(m1.hasTypographicMetrics, 1);
}
FTEST(ffcm_typo_bit_value_table) {
    /* only bit 7 of fsSelection (low byte) matters; other bits (italic 0, bold 5, regular 6, wws 8, oblique 9) do not */
    static const unsigned fs[] = { 0x0000, 0x0001, 0x0020, 0x0040, 0x0041, 0x0061, 0x0100, 0x0200, 0x0300, 0x007F, 0x0080, 0x00C0, 0x00C1, 0x00A1, 0x0180, 0x03FF };
    int i;
    for (i = 0; i < (int)(sizeof fs / sizeof fs[0]); i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.fs_selection = fs[i];
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        if (fs[i] & 0x80) { CHECKF(m.ascent == 1600 && m.descent == 420 && m.lineGap == 80, "fsSelection 0x%x typo expected, got %u/%u/%d", fs[i], m.ascent, m.descent, m.lineGap); }
        else { CHECKF(m.ascent == 1800 && m.descent == 450 && m.lineGap == 0, "fsSelection 0x%x win expected, got %u/%u/%d", fs[i], m.ascent, m.descent, m.lineGap); }
    }
}
FTEST(ffcm_typo_other_fields_unchanged) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.fs_selection |= 0x80;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.designUnitsPerEm, 2048); EXPECT_EQ(m.capHeight, 1400); EXPECT_EQ(m.xHeight, 1000);
    EXPECT_EQ(m.underlinePosition, -200); EXPECT_EQ(m.underlineThickness, 100);
    EXPECT_EQ(m.strikethroughPosition, 600); EXPECT_EQ(m.strikethroughThickness, 100);
}
FTEST(ffcm_typo_values_table) {
    static const struct { int a, d, g; } t[] = { { 800, -200, 50 }, { 1000, -300, 0 }, { 2000, -1000, 200 }, { 1, -1, 1 }, { 3000, -2500, 400 }, { 0, 0, 0 }, { 1638, -410, 410 } };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.fs_selection |= 0x80; s.typo_asc = t[i].a; s.typo_desc = t[i].d; s.typo_gap = t[i].g;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.ascent == t[i].a && m.descent == -t[i].d && m.lineGap == t[i].g, "typo %d/%d/%d -> %u/%u/%d", t[i].a, t[i].d, t[i].g, m.ascent, m.descent, m.lineGap);
    }
}
FTEST(ffcm_typo_ignores_win_and_hhea) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.fs_selection |= 0x80;
    s.win_asc = 9000; s.win_desc = 8000; s.hhea_asc = 7000; s.hhea_desc = -6000; s.hhea_gap = 500;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.ascent, 1600); EXPECT_EQ(m.descent, 420); EXPECT_EQ(m.lineGap, 80);
}
FTEST(ffcm_typo_positive_descender_note) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.fs_selection |= 0x80; s.typo_desc = 300;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    t_note("typoDescender=+300 -> descent %u", m.descent);
    CHECK(m.descent <= 300);
}
FTEST(ffcm_typo_flag_without_os2_is_ignored) {
    ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) return;
    EXPECT_EQ(m.ascent, 1700); EXPECT_EQ(m.descent, 400); EXPECT_EQ(m.lineGap, 90);
    if (h1) EXPECT_EQ(m1.hasTypographicMetrics, 0);
}

/* ---- win vs hhea fallback --------------------------------------------------------------------------------------------- */
FTEST(ffcm_win_zero_falls_back_to_hhea) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.win_asc = 0; s.win_desc = 0;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.ascent, 1700); EXPECT_EQ(m.descent, 400); EXPECT_EQ(m.lineGap, 90);
}
FTEST(ffcm_win_ascent_only_counts) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.win_asc = 1500; s.win_desc = 0;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.ascent, 1500); EXPECT_EQ(m.descent, 0);
    /* gap = 1700 + 400 + 90 - 1500 - 0 */
    EXPECT_EQ(m.lineGap, 690);
}
FTEST(ffcm_win_descent_only_counts) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.win_asc = 0; s.win_desc = 300;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.ascent, 0); EXPECT_EQ(m.descent, 300);
    EXPECT_EQ(m.lineGap, 1700 + 400 + 90 - 300);
}
FTEST(ffcm_linegap_table) {
    static const struct { int ha, hd, hg, wa, wd, gap; } t[] = {
        { 1700, -400, 90, 1800, 450, 0 },           /* negative -> 0 */
        { 2000, -500, 100, 1800, 450, 350 },
        { 1800, -450, 0, 1800, 450, 0 },
        { 1800, -450, 25, 1800, 450, 25 },
        { 1000, -200, 0, 900, 100, 200 },
        { 1000, 200, 0, 900, 100, 200 },            /* hhea.descender sign is irrelevant (absolute value) */
        { 2500, -700, 300, 1000, 1000, 1500 },
        { 1000, -200, -50, 900, 100, 200 },         /* negative hhea gap counts as 0 */
    };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s);
        s.hhea_asc = t[i].ha; s.hhea_desc = t[i].hd; s.hhea_gap = t[i].hg; s.win_asc = t[i].wa; s.win_desc = t[i].wd;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.ascent == t[i].wa && m.descent == t[i].wd && m.lineGap == t[i].gap, "row %d: got %u/%u/%d want %d/%d/%d", i, m.ascent, m.descent, m.lineGap, t[i].wa, t[i].wd, t[i].gap);
    }
}
FTEST(ffcm_hhea_fallback_table) {
    static const struct { int ha, hd, hg, a, d, g; } t[] = {
        { 1700, -400, 90, 1700, 400, 90 }, { 800, -200, 0, 800, 200, 0 }, { 900, 250, 10, 900, 250, 10 },
        { 1000, -300, -20, 1000, 300, 0 }, { 2048, -512, 128, 2048, 512, 128 }, { 100, -1, 1, 100, 1, 1 },
    };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s);
        s.hhea_asc = t[i].ha; s.hhea_desc = t[i].hd; s.hhea_gap = t[i].hg; s.win_asc = 0; s.win_desc = 0;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.ascent == t[i].a && m.descent == t[i].d && m.lineGap == t[i].g, "row %d: got %u/%u/%d want %d/%d/%d", i, m.ascent, m.descent, m.lineGap, t[i].a, t[i].d, t[i].g);
    }
}
FTEST(ffcm_no_os2_uses_hhea) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.designUnitsPerEm, 2048); EXPECT_EQ(m.ascent, 1700); EXPECT_EQ(m.descent, 400); EXPECT_EQ(m.lineGap, 90);
}
FTEST(ffcm_no_os2_strikethrough_defaults) {
    /* without OS/2: strikeout size = underline thickness, position = upm / 3 */
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.strikethroughThickness, 100); EXPECT_EQ(m.strikethroughPosition, 2048 / 3);
    EXPECT_EQ(m.underlinePosition, -200); EXPECT_EQ(m.underlineThickness, 100);
}
FTEST(ffcm_no_os2_sub_super_zero) {
    ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) return;
    if (!h1) { t_skip("IDWriteFontFace1 unsupported"); return; }
    EXPECT_EQ(m1.subscriptPositionX, 0); EXPECT_EQ(m1.subscriptPositionY, 0); EXPECT_EQ(m1.subscriptSizeX, 0); EXPECT_EQ(m1.subscriptSizeY, 0);
    EXPECT_EQ(m1.superscriptPositionX, 0); EXPECT_EQ(m1.superscriptPositionY, 0); EXPECT_EQ(m1.superscriptSizeX, 0); EXPECT_EQ(m1.superscriptSizeY, 0);
}
FTEST(ffcm_no_os2_heights_are_derived_note) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    t_note("no OS/2: capHeight %u xHeight %u (from glyph outlines / fallbacks)", m.capHeight, m.xHeight);
    CHECK(m.capHeight > 0); CHECK(m.xHeight > 0);
}
FTEST(ffcm_os2_version_table_win_path) {
    /* OS/2 version 0..5: the table is used as long as it is longer than 0x4d bytes (every real version) */
    int v;
    for (v = 0; v <= 5; v++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.os2_version = v;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.ascent == 1800 && m.descent == 450, "os/2 v%d: %u/%u", v, m.ascent, m.descent);
        CHECKF(m.strikethroughPosition == 600 && m.strikethroughThickness == 100, "os/2 v%d strike %d/%u", v, m.strikethroughPosition, m.strikethroughThickness);
    }
}
FTEST(ffcm_os2_version_heights) {
    /* sxHeight / sCapHeight exist from version 2 on (table length >= 0x60) */
    static const int v[] = { 2, 3, 4, 5 }; int i;
    for (i = 0; i < 4; i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.os2_version = v[i]; s.x_height = 900; s.cap_height = 1300;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.xHeight == 900 && m.capHeight == 1300, "os/2 v%d: x %u cap %u", v[i], m.xHeight, m.capHeight);
    }
}
FTEST(ffcm_os2_old_versions_heights_not_from_table) {
    /* v0 and v1 carry no height fields: the 1000/1400 of the spec must not appear because the spec's values are not written */
    int v;
    for (v = 0; v <= 1; v++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.os2_version = v; s.x_height = 777; s.cap_height = 999;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.xHeight != 777 && m.capHeight != 999, "v%d heights %u/%u", v, m.xHeight, m.capHeight);
        t_note("os/2 v%d: capHeight %u xHeight %u", v, m.capHeight, m.xHeight);
        CHECK(m.xHeight > 0 && m.capHeight > 0);
    }
}

/* ---- capHeight / xHeight validation (ValidateHeights) ----------------------------------------------------------------------- */
static void ffc_xh_case(IDWriteFactory *f, int xh, int cap, int xvalid, int capvalid) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.x_height = xh; s.cap_height = cap;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    if (xvalid) CHECKF(m.xHeight == xh, "x %d valid -> %u", xh, m.xHeight); else CHECKF(m.xHeight != xh, "x %d invalid kept as %u", xh, m.xHeight);
    if (capvalid) CHECKF(m.capHeight == cap, "cap %d valid -> %u", cap, m.capHeight); else CHECKF(m.capHeight != cap, "cap %d invalid kept as %u", cap, m.capHeight);
}
/* x valid: upm < 10x and 10x < 9 upm  => 205 .. 1843 ; cap valid: cap <= upm and upm < 10 cap => 205 .. 2048 */
FTEST(ffcm_xh_lower_bound_valid) { ffc_xh_case(f, 205, 1400, 1, 1); }
FTEST(ffcm_xh_lower_bound_invalid) { ffc_xh_case(f, 204, 1400, 0, 1); }
FTEST(ffcm_xh_upper_bound_valid) { ffc_xh_case(f, 1843, 1400, 1, 1); }
FTEST(ffcm_xh_upper_bound_invalid) { ffc_xh_case(f, 1844, 1400, 0, 1); }
FTEST(ffcm_cap_lower_bound_valid) { ffc_xh_case(f, 1000, 205, 1, 1); }
FTEST(ffcm_cap_lower_bound_invalid) { ffc_xh_case(f, 1000, 204, 0 + 1, 0); }
FTEST(ffcm_cap_upper_bound_valid) { ffc_xh_case(f, 1000, 2048, 1, 1); }
FTEST(ffcm_cap_upper_bound_invalid) { ffc_xh_case(f, 1000, 2049, 1, 0); }
FTEST(ffcm_heights_zero_in_table) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.x_height = 0; s.cap_height = 0;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    t_note("OS/2 heights 0 -> cap %u x %u", m.capHeight, m.xHeight);
    CHECK(m.capHeight > 0 && m.xHeight > 0);
}
FTEST(ffcm_symbol_height_fallbacks) {
    /* symbol font without usable OS/2 heights: xHeight = upm / 2, capHeight = upm * 70 / 100 */
    static const ttf_map sm[] = { { 0xF020, 1 }, { 0xF041, 2 }, { 0xF061, 28 } };
    ttf_spec s; ttf_cmap c; DWRITE_FONT_METRICS m; ttf_default_spec(&s);
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 0; c.n = 3; c.map = sm;
    s.cmaps = &c; s.ncmaps = 1; s.os2_version = 1;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.xHeight, 1024); EXPECT_EQ(m.capHeight, 2048 * 70 / 100);
}
FTEST(ffcm_symbol_fallback_follows_upm) {
    static const int up[] = { 1000, 1024, 2000, 2048, 4096 }; static const ttf_map sm[] = { { 0xF020, 1 }, { 0xF041, 2 } }; int i;
    for (i = 0; i < 5; i++) {
        ttf_spec s; ttf_cmap c; DWRITE_FONT_METRICS m; ttf_default_spec(&s);
        memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 0; c.n = 2; c.map = sm;
        s.cmaps = &c; s.ncmaps = 1; s.os2_version = 1; s.upm = up[i];
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.xHeight == up[i] / 2 && m.capHeight == up[i] * 70 / 100, "upm %d: x %u cap %u", up[i], m.xHeight, m.capHeight);
    }
}

/* ---- units per em ----------------------------------------------------------------------------------------------------------------- */
FTEST(ffcm_upm_table) {
    static const int up[] = { 16, 64, 100, 128, 250, 256, 500, 512, 750, 1000, 1024, 1500, 2000, 2048, 2816, 4096, 8192, 16384 }; int i;
    for (i = 0; i < (int)(sizeof up / sizeof up[0]); i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.upm = up[i];
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.designUnitsPerEm == up[i], "upm %d -> %u", up[i], m.designUnitsPerEm);
    }
}
FTEST(ffcm_upm_does_not_scale_other_fields) {
    /* metric fields are design-unit values from the tables, not rescaled to 2048 */
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.upm = 1000;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450); EXPECT_EQ(m.underlinePosition, -200);
}

/* ---- underline / strikethrough --------------------------------------------------------------------------------------------- */
FTEST(ffcm_underline_from_post_table) {
    static const struct { int pos, th; } t[] = { { -200, 100 }, { -100, 50 }, { 0, 1 }, { 150, 10 }, { -1000, 300 }, { -32768, 1 }, { 32767, 65 } }; int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.underline_pos = t[i].pos; s.underline_thick = t[i].th;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.underlinePosition == t[i].pos && m.underlineThickness == t[i].th, "post %d/%d -> %d/%u", t[i].pos, t[i].th, m.underlinePosition, m.underlineThickness);
    }
}
FTEST(ffcm_underline_thickness_zero_derived) {
    static const int up[] = { 1000, 1024, 2000, 2048, 4096 }; int i;
    for (i = 0; i < 5; i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.upm = up[i]; s.underline_thick = 0; s.underline_pos = -77;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.underlineThickness == (up[i] + 7) / 14, "upm %d thickness 0 -> %u want %d", up[i], m.underlineThickness, (up[i] + 7) / 14);
        EXPECT_EQ(m.underlinePosition, -77);
    }
}
FTEST(ffcm_no_post_underline_defaults) {
    static const int up[] = { 1000, 1024, 2000, 2048, 4096 }; int i;
    for (i = 0; i < 5; i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.upm = up[i]; s.omit |= TTF_OMIT_POST;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.underlinePosition == -((up[i] + 5) / 10), "upm %d position %d want %d", up[i], m.underlinePosition, -((up[i] + 5) / 10));
        CHECKF(m.underlineThickness == (up[i] + 7) / 14, "upm %d thickness %u want %d", up[i], m.underlineThickness, (up[i] + 7) / 14);
    }
}
FTEST(ffcm_strikethrough_table) {
    static const struct { int pos, sz; } t[] = { { 600, 100 }, { 500, 50 }, { 0, 20 }, { -50, 30 }, { 1000, 200 } }; int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.strike_pos = t[i].pos; s.strike_size = t[i].sz;
        if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
        CHECKF(m.strikethroughPosition == t[i].pos && m.strikethroughThickness == t[i].sz, "strike %d/%d -> %d/%u", t[i].pos, t[i].sz, m.strikethroughPosition, m.strikethroughThickness);
    }
}
FTEST(ffcm_strikethrough_size_zero_uses_underline_thickness) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.strike_size = 0; s.underline_thick = 77;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.strikethroughThickness, 77); EXPECT_EQ(m.strikethroughPosition, 600);
}
FTEST(ffcm_strikethrough_independent_of_underline) {
    ttf_spec s; DWRITE_FONT_METRICS m; ttf_default_spec(&s); s.strike_size = 40; s.underline_thick = 80; s.underline_pos = -120;
    if (!ffc_get(f, &s, 0, &m, NULL, NULL)) return;
    EXPECT_EQ(m.strikethroughThickness, 40); EXPECT_EQ(m.underlineThickness, 80); EXPECT_EQ(m.underlinePosition, -120);
}

/* ---- simulations ------------------------------------------------------------------------------------------------------------------ */
FTEST(ffcm_sim_bold_leaves_all_metrics) {
    ttf_spec s; DWRITE_FONT_METRICS a, b; DWRITE_FONT_METRICS1 a1, b1; int h1, h2; ttf_default_spec(&s);
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_NONE, &a, &a1, &h1)) return;
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_BOLD, &b, &b1, &h2)) return;
    CHECK(!memcmp(&a, &b, sizeof a));
    if (h1 && h2) CHECK(!memcmp(&a1, &b1, sizeof a1));
}
FTEST(ffcm_sim_oblique_v0_unchanged) {
    ttf_spec s; DWRITE_FONT_METRICS a, b; ttf_default_spec(&s);
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_NONE, &a, NULL, NULL)) return;
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE, &b, NULL, NULL)) return;
    CHECK(!memcmp(&a, &b, sizeof a));
}
FTEST(ffcm_sim_oblique_v1_shears_box_and_scripts) {
    ttf_spec s; DWRITE_FONT_METRICS a, b; DWRITE_FONT_METRICS1 a1, b1; int h1, h2; ttf_default_spec(&s);
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_NONE, &a, &a1, &h1)) return;
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE, &b, &b1, &h2)) return;
    if (!h1 || !h2) { t_skip("IDWriteFontFace1 unsupported"); return; }
    /* X positions move by (Y * 0x5700) >> 16: glyph box right by the top, left by the bottom, sub/superscript X by their Y */
    EXPECT_EQ(b1.glyphBoxRight, a1.glyphBoxRight + ffc_shear(a1.glyphBoxTop));
    EXPECT_EQ(b1.glyphBoxLeft, a1.glyphBoxLeft + ffc_shear(a1.glyphBoxBottom));
    EXPECT_EQ(b1.subscriptPositionX, a1.subscriptPositionX + ffc_shear(a1.subscriptPositionY));
    EXPECT_EQ(b1.superscriptPositionX, a1.superscriptPositionX + ffc_shear(a1.superscriptPositionY));
    /* everything else untouched */
    EXPECT_EQ(b1.glyphBoxTop, a1.glyphBoxTop); EXPECT_EQ(b1.glyphBoxBottom, a1.glyphBoxBottom);
    EXPECT_EQ(b1.subscriptPositionY, a1.subscriptPositionY); EXPECT_EQ(b1.superscriptPositionY, a1.superscriptPositionY);
    EXPECT_EQ(b1.subscriptSizeX, a1.subscriptSizeX); EXPECT_EQ(b1.superscriptSizeY, a1.superscriptSizeY);
    EXPECT_EQ(b1.hasTypographicMetrics, a1.hasTypographicMetrics);
}
FTEST(ffcm_sim_oblique_shear_is_positive_for_box_right) {
    ttf_spec s; DWRITE_FONT_METRICS a, b; DWRITE_FONT_METRICS1 a1, b1; int h1, h2; ttf_default_spec(&s);
    if (!ffc_get(f, &s, 0, &a, &a1, &h1)) return;
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE, &b, &b1, &h2)) return;
    if (!h1 || !h2) { t_skip("IDWriteFontFace1 unsupported"); return; }
    CHECK(b1.glyphBoxRight > a1.glyphBoxRight);
    t_note("oblique glyph box right %d -> %d", a1.glyphBoxRight, b1.glyphBoxRight);
}
FTEST(ffcm_sim_both_equals_oblique_for_metrics) {
    ttf_spec s; DWRITE_FONT_METRICS a, b; DWRITE_FONT_METRICS1 a1, b1; int h1, h2; ttf_default_spec(&s);
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE, &a, &a1, &h1)) return;
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE, &b, &b1, &h2)) return;
    CHECK(!memcmp(&a, &b, sizeof a));
    if (h1 && h2) CHECK(!memcmp(&a1, &b1, sizeof a1));
}
FTEST(ffcm_sim_oblique_with_typo_metrics) {
    ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1; ttf_default_spec(&s); s.fs_selection |= 0x80;
    if (!ffc_get(f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE, &m, &m1, &h1)) return;
    EXPECT_EQ(m.ascent, 1600); EXPECT_EQ(m.descent, 420);
    if (h1) EXPECT_EQ(m1.hasTypographicMetrics, 1);
}

/* ---- v1 interface specifics ---------------------------------------------------------------------------------------------------- */
FTEST(ffcm_v1_all_fields_table_typo) {
    ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1; ttf_default_spec(&s); s.fs_selection |= 0x80;
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) return;
    if (!h1) { t_skip("IDWriteFontFace1 unsupported"); return; }
    EXPECT_EQ(m1.designUnitsPerEm, 2048); EXPECT_EQ(m1.ascent, 1600); EXPECT_EQ(m1.descent, 420); EXPECT_EQ(m1.lineGap, 80);
    EXPECT_EQ(m1.hasTypographicMetrics, 1);
}
FTEST(ffcm_v1_sub_super_follow_os2) {
    /* the OS/2 fields are copied: subscript X/Y size and offsets, superscript likewise; Y offsets of the subscript are reported negated */
    size_t n; uint8_t *b; const uint8_t *os2; unsigned ol; ttf_spec s; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1; int h1;
    ttf_default_spec(&s);
    b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    os2 = ffc_blob_table(b, n, "OS/2", &ol);
    if (!os2 || ol < 78) { free(b); t_skip("OS/2"); return; }
    if (!ffc_get(f, &s, 0, &m, &m1, &h1)) { free(b); return; }
    if (!h1) { free(b); t_skip("IDWriteFontFace1 unsupported"); return; }
    EXPECT_EQ(m1.subscriptSizeX, ffc_s16(os2 + 10)); EXPECT_EQ(m1.subscriptSizeY, ffc_s16(os2 + 12));
    EXPECT_EQ(m1.subscriptPositionX, ffc_s16(os2 + 14)); EXPECT_EQ(m1.subscriptPositionY, -ffc_s16(os2 + 16));
    EXPECT_EQ(m1.superscriptSizeX, ffc_s16(os2 + 18)); EXPECT_EQ(m1.superscriptSizeY, ffc_s16(os2 + 20));
    EXPECT_EQ(m1.superscriptPositionX, ffc_s16(os2 + 22)); EXPECT_EQ(m1.superscriptPositionY, ffc_s16(os2 + 24));
    free(b);
}
FTEST(ffcm_v1_through_face2_matches_face1) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace1 *a; IDWriteFontFace2 *b; DWRITE_FONT_METRICS1 ma, mb; if (!face) return;
    a = (IDWriteFontFace1 *)ffc_face_v(face, 1); b = (IDWriteFontFace2 *)ffc_face_v(face, 2);
    if (a && b) {
        IDWriteFontFace1_GetMetrics(a, &ma); IDWriteFontFace1_GetMetrics((IDWriteFontFace1 *)b, &mb);
        CHECK(!memcmp(&ma, &mb, sizeof ma));
    } else t_skip("face1/face2 unsupported");
    if (a) IDWriteFontFace1_Release(a);
    if (b) IDWriteFontFace2_Release(b);
    dwx_font_close(&h);
}

/* ---- system fonts ------------------------------------------------------------------------------------------------------------------- */
FTEST(ffcm_sys_arial_derivation_matches_tables) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    DWRITE_FONT_METRICS m; UINT32 hl = 0, ol = 0, pl = 0; uint8_t *head, *os2, *post, *hhea; UINT32 hhl = 0;
    if (!face) { t_skip("Arial not installed"); return; }
    IDWriteFontFace_GetMetrics(face, &m);
    head = dw_face_table(face, DW_TAG('h','e','a','d'), &hl); os2 = dw_face_table(face, DW_TAG('O','S','/','2'), &ol);
    post = dw_face_table(face, DW_TAG('p','o','s','t'), &pl); hhea = dw_face_table(face, DW_TAG('h','h','e','a'), &hhl);
    if (head && hl >= 20) EXPECT_EQ(m.designUnitsPerEm, ffc_be16(head + 18));
    if (os2 && ol >= 78) {
        unsigned fsel = ffc_be16(os2 + 62);
        if (fsel & 0x80) { EXPECT_EQ(m.ascent, ffc_s16(os2 + 68)); EXPECT_EQ(m.descent, -ffc_s16(os2 + 70)); }
        else if (ffc_be16(os2 + 74) || ffc_be16(os2 + 76)) { EXPECT_EQ(m.ascent, ffc_be16(os2 + 74)); EXPECT_EQ(m.descent, ffc_be16(os2 + 76)); }
        EXPECT_EQ(m.strikethroughPosition, ffc_s16(os2 + 28)); EXPECT_EQ(m.strikethroughThickness, ffc_s16(os2 + 26));
        if (ol >= 96) { EXPECT_EQ(m.capHeight, ffc_be16(os2 + 88)); EXPECT_EQ(m.xHeight, ffc_be16(os2 + 86)); }
    }
    if (post && pl >= 12) { EXPECT_EQ(m.underlinePosition, ffc_s16(post + 8)); EXPECT_EQ(m.underlineThickness, ffc_be16(post + 10)); }
    t_note("Arial: upm %u asc %u desc %u gap %d cap %u x %u ul %d/%u st %d/%u", m.designUnitsPerEm, m.ascent, m.descent, m.lineGap, m.capHeight, m.xHeight,
           m.underlinePosition, m.underlineThickness, m.strikethroughPosition, m.strikethroughThickness);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    free(head); free(os2); free(post); free(hhea);
    IDWriteFontFace_Release(face);
}
FTEST(ffcm_sys_arial_ascent_descent_ratio) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    DWRITE_FONT_METRICS m;
    if (!face) { t_skip("Arial not installed"); return; }
    IDWriteFontFace_GetMetrics(face, &m);
    CHECK(m.ascent > m.descent); CHECK(m.ascent + m.descent > m.designUnitsPerEm / 2); CHECK(m.xHeight < m.capHeight);
    IDWriteFontFace_Release(face);
}
FTEST(ffcm_sys_fonts_sane_metrics) {
    static const WCHAR *fam[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Consolas", L"Segoe UI Symbol", L"Marlett" }; int i;
    for (i = 0; i < 8; i++) {
        IDWriteFontFace *face = dw_sysface(f, fam[i], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); DWRITE_FONT_METRICS m;
        if (!face) { t_note("%ls not installed", fam[i]); continue; }
        IDWriteFontFace_GetMetrics(face, &m);
        CHECKF(m.designUnitsPerEm >= 16 && m.designUnitsPerEm <= 16384, "%ls upm %u", fam[i], m.designUnitsPerEm);
        CHECKF(m.ascent > 0 && m.descent > 0, "%ls asc/desc %u/%u", fam[i], m.ascent, m.descent);
        CHECKF(m.underlineThickness > 0 && m.strikethroughThickness > 0, "%ls thickness", fam[i]);
        CHECKF(m.xHeight > 0 && m.capHeight > 0, "%ls heights", fam[i]);
        t_note("%ls: upm %u asc %u desc %u gap %d cap %u x %u", fam[i], m.designUnitsPerEm, m.ascent, m.descent, m.lineGap, m.capHeight, m.xHeight);
        IDWriteFontFace_Release(face);
    }
}
FTEST(ffcm_sys_bold_vs_regular_same_upm) {
    IDWriteFontFace *r = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL), *b = dw_sysface(f, L"Arial", 700, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    DWRITE_FONT_METRICS mr, mb;
    if (!r || !b) { t_skip("Arial regular/bold not installed"); if (r) IDWriteFontFace_Release(r); if (b) IDWriteFontFace_Release(b); return; }
    IDWriteFontFace_GetMetrics(r, &mr); IDWriteFontFace_GetMetrics(b, &mb);
    EXPECT_EQ(mr.designUnitsPerEm, mb.designUnitsPerEm);
    IDWriteFontFace_Release(r); IDWriteFontFace_Release(b);
}
FTEST(ffcm_sys_metrics_v1_prefix) {
    IDWriteFontFace *face = dw_sysface(f, L"Segoe UI", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace1 *f1; DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS1 m1;
    if (!face) { t_skip("Segoe UI not installed"); return; }
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1);
    if (f1) { IDWriteFontFace_GetMetrics(face, &m); IDWriteFontFace1_GetMetrics(f1, &m1); CHECK(!memcmp(&m, &m1, sizeof m)); t_note("Segoe UI hasTypographicMetrics=%d", m1.hasTypographicMetrics); IDWriteFontFace1_Release(f1); }
    IDWriteFontFace_Release(face);
}
