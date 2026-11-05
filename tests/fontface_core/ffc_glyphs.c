/* IDWriteFontFace::GetGlyphIndices, GetDesignGlyphMetrics, GetGdiCompatibleGlyphMetrics.
 *
 * GetGlyphIndices (IDWriteFontFace::GetGlyphIndicesW): the output array is zeroed first (count entries), then NULL codePoints with a
 * non-zero count is E_INVALIDARG; code points >= 0x110000 and unmapped code points give glyph 0 with S_OK; surrogates are looked up like any other
 * value (no special casing), count 0 succeeds without touching anything.
 *
 * GetDesignGlyphMetrics: metrics are zeroed first, NULL glyphIndices => E_INVALIDARG. Per glyph (TrueTypeFontMetricsBuilder::GetGlyphMetrics):
 *   advanceWidth / leftSideBearing come from hmtx, rightSideBearing = advance - lsb - inkWidth;
 *   without vhea/vmtx: advanceHeight = typoAscender + |typoDescender| (hhea when OS/2 is absent), verticalOriginY = typoAscender,
 *   topSideBearing = verticalOriginY - yMax, bottomSideBearing = advanceHeight - topSideBearing - inkHeight;
 *   BOLD simulation: glyphs with ink grow advanceWidth, advanceHeight and verticalOriginY by (upm + 25) / 50 (zero advances grow the right/top
 *   bearings negative instead); OBLIQUE: lsb += (yMin * 0x5700) >> 16, rsb -= (yMax * 0x5700) >> 16 for glyphs with ink.
 *   isSideways has no influence on the numbers (only the OBLIQUE+sideways combination takes another path). */
#include "ffc.h"

#define E_INVAL ((HRESULT)0x80070057)

/* ---- model of the simulations ------------------------------------------------------------------------------------------- */
static int ffc_has_size(const DWRITE_GLYPH_METRICS *m) {
    long long w = (long long)m->advanceWidth - m->rightSideBearing - m->leftSideBearing;
    long long h = (long long)m->advanceHeight - m->bottomSideBearing - m->topSideBearing;
    return w > 0 && h > 0;
}
static DWRITE_GLYPH_METRICS ffc_model_bold(DWRITE_GLYPH_METRICS m, int upm) {
    int d = (upm + 25) / 50;
    if (ffc_has_size(&m)) {
        if (m.advanceWidth == 0) m.rightSideBearing -= d; else m.advanceWidth += d;
        if (m.advanceHeight == 0) m.topSideBearing -= d; else { m.advanceHeight += d; m.verticalOriginY += d; }
    }
    return m;
}
static DWRITE_GLYPH_METRICS ffc_model_oblique(DWRITE_GLYPH_METRICS m) {
    if (ffc_has_size(&m)) {
        m.leftSideBearing += ffc_shear(m.bottomSideBearing - (int)m.advanceHeight + m.verticalOriginY);
        m.rightSideBearing -= ffc_shear(m.verticalOriginY - m.topSideBearing);
    }
    return m;
}
static int ffc_gm_eq(const DWRITE_GLYPH_METRICS *a, const DWRITE_GLYPH_METRICS *b) { return !memcmp(a, b, sizeof *a); }
#define CHECK_GM(m, l, a, r, t, ah, b, v) \
    CHECKF((m).leftSideBearing == (l) && (m).advanceWidth == (UINT32)(a) && (m).rightSideBearing == (r) && (m).topSideBearing == (t) && \
           (m).advanceHeight == (UINT32)(ah) && (m).bottomSideBearing == (b) && (m).verticalOriginY == (v), \
           "got lsb %d adv %u rsb %d tsb %d advH %u bsb %d vOrg %d; want %d %d %d %d %d %d %d", (m).leftSideBearing, (m).advanceWidth, (m).rightSideBearing, \
           (m).topSideBearing, (m).advanceHeight, (m).bottomSideBearing, (m).verticalOriginY, (int)(l), (int)(a), (int)(r), (int)(t), (int)(ah), (int)(b), (int)(v))

static int ffc_dgm(IDWriteFontFace *face, UINT16 gid, DWRITE_GLYPH_METRICS *m, BOOL sideways) {
    UINT16 g = gid; HRESULT hr;
    memset(m, 0xCC, sizeof *m);
    hr = IDWriteFontFace_GetDesignGlyphMetrics(face, &g, 1, m, sideways);
    CHECKF(hr == S_OK, "GetDesignGlyphMetrics(%u) -> %s", gid, dw_hr_name(hr));
    return hr == S_OK;
}

/* extra glyphs appended to the default font (gid 69 ...) */
static const ttf_pt ffc_box[4]   = { { 100, 0, 1 }, { 100, 1400, 1 }, { 900, 1400, 1 }, { 900, 0, 1 } };
static const ttf_pt ffc_boxlow[4] = { { 100, -300, 1 }, { 100, 1400, 1 }, { 900, 1400, 1 }, { 900, -300, 1 } };
static const ttf_pt ffc_mark[4]  = { { -50, 0, 1 }, { -50, 700, 1 }, { 50, 700, 1 }, { 50, 0, 1 } };
static const unsigned short ffc_end4[1] = { 3 };
#define FFC_G_LSB150   69    /* adv 1000, hmtx lsb 150 (outline starts at x=100) */
#define FFC_G_NARROW   70    /* adv 600, box 100..900 => negative rsb */
#define FFC_G_LOW      71    /* box with yMin -300 */
#define FFC_G_VERT     72    /* vmtx advance 900, tsb 300 */
#define FFC_G_MARK     73    /* advance 0 with ink -50..50 */
#define FFC_NG         74
static void ffc_extra_spec(ttf_spec *s) {
    int n; ttf_glyph *g = ffc_glyphs_default(&n); int i;
    ttf_default_spec(s);
    for (i = n; i < FFC_NG; i++) memset(&g[i], 0, sizeof g[i]);
    g[69].adv = 1000; g[69].lsb_set = 1; g[69].lsb = 150; g[69].npts = 4; g[69].ncont = 1; g[69].pts = ffc_box; g[69].ends = ffc_end4;
    g[70].adv = 600;  g[70].npts = 4; g[70].ncont = 1; g[70].pts = ffc_box; g[70].ends = ffc_end4;
    g[71].adv = 1000; g[71].npts = 4; g[71].ncont = 1; g[71].pts = ffc_boxlow; g[71].ends = ffc_end4;
    g[72].adv = 1000; g[72].vadv = 900; g[72].tsb_set = 1; g[72].tsb = 300; g[72].npts = 4; g[72].ncont = 1; g[72].pts = ffc_box; g[72].ends = ffc_end4;
    g[73].adv = 0;    g[73].npts = 4; g[73].ncont = 1; g[73].pts = ffc_mark; g[73].ends = ffc_end4;
    s->glyphs = g; s->nglyphs = FFC_NG;
}

/* ================================== GetGlyphIndices ===================================================================== */
FTEST(ffcg_idx_default_letters) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp[] = { 'A', 'B', 'Z', 'a', 'm', 'z', '0', '5', '9', ' ' }; UINT16 gi[10]; int i;
    if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 10, gi));
    for (i = 0; i < 10; i++) CHECKF(gi[i] == ttf_default_gid(cp[i]), "U+%04X -> %u want %u", cp[i], gi[i], ttf_default_gid(cp[i]));
    EXPECT_EQ(gi[0], 2); EXPECT_EQ(gi[3], 28); EXPECT_EQ(gi[6], 54); EXPECT_EQ(gi[9], 1);
    dwx_font_close(&h);
}
FTEST(ffcg_idx_missing_is_zero_with_s_ok) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp[] = { '!', '~', 0xE9, 0x4E2D, 0x1F600, 0, 0xFFFD }; UINT16 gi[7]; int i;
    if (!face) return;
    for (i = 0; i < 7; i++) gi[i] = 0xFFFF;
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(face, cp, 7, gi), S_OK);
    for (i = 0; i < 7; i++) CHECKF(gi[i] == 0, "U+%04X -> %u", cp[i], gi[i]);
    dwx_font_close(&h);
}
FTEST(ffcg_idx_zero_count_succeeds) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp = 'A'; UINT16 gi = 0x1234; if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 0, &gi));
    EXPECT_EQ(gi, 0x1234);   /* nothing written */
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, NULL, 0, &gi));
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, NULL, 0, NULL));
    dwx_font_close(&h);
}
FTEST(ffcg_idx_null_codepoints_invalidarg_zeroes_output) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT16 gi[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF }; int i; if (!face) return;
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(face, NULL, 3, gi), E_INVAL);
    for (i = 0; i < 3; i++) EXPECT_EQ(gi[i], 0);       /* zeroed before validation */
    EXPECT_EQ(gi[3], 0xFFFF);                           /* but only count entries */
    dwx_font_close(&h);
}
FTEST(ffcg_idx_output_prefilled_overwritten) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp[3] = { 'Q', '!', 'q' }; UINT16 gi[4] = { 0xEEEE, 0xEEEE, 0xEEEE, 0xEEEE }; if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 3, gi));
    EXPECT_EQ(gi[0], 18); EXPECT_EQ(gi[1], 0); EXPECT_EQ(gi[2], 44); EXPECT_EQ(gi[3], 0xEEEE);
    dwx_font_close(&h);
}
FTEST(ffcg_idx_beyond_unicode) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp[] = { 0x10FFFF, 0x110000, 0x110041, 0x1000000, 0x7FFFFFFF, 0x80000041, 0xFFFFFFFF, 0xFFFFFF41 }; UINT16 gi[8]; int i; if (!face) return;
    for (i = 0; i < 8; i++) gi[i] = 0xFFFF;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 8, gi));
    for (i = 0; i < 8; i++) CHECKF(gi[i] == 0, "U+%X -> %u", cp[i], gi[i]);
    dwx_font_close(&h);
}
FTEST(ffcg_idx_surrogate_range_unmapped) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp[2048]; UINT16 gi[2048]; int i; if (!face) return;
    for (i = 0; i < 2048; i++) { cp[i] = 0xD800 + i; gi[i] = 0xFFFF; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 2048, gi));
    for (i = 0; i < 2048; i++) if (gi[i] != 0) { CHECKF(0, "U+%04X -> %u", cp[i], gi[i]); break; }
    dwx_font_close(&h);
}
FTEST(ffcg_idx_noncharacters) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp[] = { 0xFFFE, 0xFFFF, 0x1FFFE, 0x1FFFF, 0x10FFFE, 0x10FFFF, 0xFDD0, 0xFDEF }; UINT16 gi[8]; int i; if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 8, gi));
    for (i = 0; i < 8; i++) CHECKF(gi[i] == 0, "U+%X -> %u", cp[i], gi[i]);
    dwx_font_close(&h);
}
FTEST(ffcg_idx_all_ascii_and_latin1_match_model) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp[512]; UINT16 gi[512]; int i, bad = 0; if (!face) return;
    for (i = 0; i < 512; i++) cp[i] = i;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 512, gi));
    for (i = 0; i < 512; i++) if (gi[i] != ttf_default_gid(cp[i])) { if (!bad++) CHECKF(0, "U+%04X -> %u want %u", cp[i], gi[i], ttf_default_gid(cp[i])); }
    dwx_font_close(&h);
}
FTEST(ffcg_idx_batch_equals_single) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static UINT32 cp[4096]; static UINT16 gi[4096]; int i, bad = 0; if (!face) return;
    for (i = 0; i < 4096; i++) cp[i] = (UINT32)i * 3 + (i & 1 ? 0x20 : 0x30);
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 4096, gi));
    for (i = 0; i < 4096; i += 7) { UINT16 one = 0xFFFF; CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp[i], 1, &one)); if (one != gi[i] && !bad++) CHECKF(0, "U+%X batch %u single %u", cp[i], gi[i], one); }
    dwx_font_close(&h);
}
FTEST(ffcg_idx_repeated_calls_stable) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 cp[6] = { 'a', 'b', 'c', 0x100, 'd', 0x1F600 }; UINT16 a[6], b[6]; int i; if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 6, a));
    for (i = 0; i < 50; i++) { CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 6, b)); CHECK(!memcmp(a, b, sizeof a)); }
    dwx_font_close(&h);
}
FTEST(ffcg_idx_deterministic_sparse_scan_matches_model) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static UINT32 cp[6000]; static UINT16 gi[6000]; unsigned x = 12345; int i, bad = 0; if (!face) return;
    for (i = 0; i < 6000; i++) { x = x * 1103515245u + 12345u; cp[i] = (x >> 8) % 0x110000; if (i % 5 == 0) cp[i] = 0x20 + (x >> 12) % 0x60; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 6000, gi));
    for (i = 0; i < 6000; i++) if (gi[i] != ttf_default_gid(cp[i]) && !bad++) CHECKF(0, "U+%X -> %u want %u", cp[i], gi[i], ttf_default_gid(cp[i]));
    dwx_font_close(&h);
}
FTEST(ffcg_idx_independent_of_simulations) {
    int s; UINT32 cp[3] = { 'A', 'z', '7' };
    for (s = 0; s < 4; s++) {
        ttf_spec sp; dwx_font h; IDWriteFontFace *face; UINT16 gi[3]; ttf_default_spec(&sp);
        face = ffc_open(&h, f, &sp, (DWRITE_FONT_SIMULATIONS)s); if (!face) return;
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 3, gi));
        EXPECT_EQ(gi[0], 2); EXPECT_EQ(gi[1], 53); EXPECT_EQ(gi[2], 61);
        dwx_font_close(&h);
    }
}

/* ---- cmap flavours -------------------------------------------------------------------------------------------------------- */
static void ffc_idx_cmap(IDWriteFactory *f, int format, unsigned plat, unsigned enc, const ttf_map *m, int n, const UINT32 *probe, const UINT16 *want, int np, int hard) {
    ttf_spec s; ttf_cmap c; dwx_font h; IDWriteFontFace *face; UINT16 gi[32]; int i;
    ttf_default_spec(&s);
    memset(&c, 0, sizeof c); c.format = format; c.plat = plat; c.enc = enc; c.n = n; c.map = m;
    s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    for (i = 0; i < np; i++) gi[i] = 0xFFFF;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, probe, np, gi));
    for (i = 0; i < np; i++) {
        if (hard) CHECKF(gi[i] == want[i], "format %d (%u,%u): U+%X -> %u want %u", format, plat, enc, probe[i], gi[i], want[i]);
        else if (gi[i] != want[i]) t_note("format %d (%u,%u): U+%X -> %u (model %u)", format, plat, enc, probe[i], gi[i], want[i]);
    }
    dwx_font_close(&h);
}
static const ttf_map ffc_m4[] = { { 0x20, 1 }, { 0x41, 2 }, { 0x42, 3 }, { 0x43, 4 }, { 0x100, 9 }, { 0x102, 11 }, { 0x3000, 13 }, { 0xFF21, 15 } };
static const UINT32 ffc_p4[] = { 0x20, 0x41, 0x42, 0x43, 0x44, 0x100, 0x101, 0x102, 0x3000, 0x3001, 0xFF21, 0xFF22, 0x10041, 0x0 };
static const UINT16 ffc_w4[] = { 1, 2, 3, 4, 0, 9, 0, 11, 13, 0, 15, 0, 0, 0 };
FTEST(ffcg_cmap_format4_3_1) { ffc_idx_cmap(f, 4, 3, 1, ffc_m4, 8, ffc_p4, ffc_w4, 14, 1); }
FTEST(ffcg_cmap_format4_unicode_platform_0_3) { ffc_idx_cmap(f, 4, 0, 3, ffc_m4, 8, ffc_p4, ffc_w4, 14, 0); }
FTEST(ffcg_cmap_format4_unicode_platform_0_4) { ffc_idx_cmap(f, 4, 0, 4, ffc_m4, 8, ffc_p4, ffc_w4, 14, 0); }
static const ttf_map ffc_m12[] = { { 0x41, 2 }, { 0x42, 3 }, { 0xE000, 5 }, { 0x10000, 6 }, { 0x1F600, 7 }, { 0x1F601, 8 }, { 0x10FFFF, 9 } };
static const UINT32 ffc_p12[] = { 0x41, 0x42, 0x43, 0xE000, 0x10000, 0x10001, 0x1F600, 0x1F601, 0x1F602, 0x10FFFF, 0x10FFFE, 0x110000 };
static const UINT16 ffc_w12[] = { 2, 3, 0, 5, 6, 0, 7, 8, 0, 9, 0, 0 };
FTEST(ffcg_cmap_format12_3_10) { ffc_idx_cmap(f, 12, 3, 10, ffc_m12, 7, ffc_p12, ffc_w12, 12, 1); }
FTEST(ffcg_cmap_format12_unicode_platform_0_4) { ffc_idx_cmap(f, 12, 0, 4, ffc_m12, 7, ffc_p12, ffc_w12, 12, 0); }
FTEST(ffcg_cmap_format12_3_10_supplementary_only_in_format12) {
    /* a (3,1) format 4 cmap cannot map supplementary-plane characters */
    static const UINT32 p[] = { 0x10000, 0x1F600 }; static const UINT16 w[] = { 0, 0 };
    ffc_idx_cmap(f, 4, 3, 1, ffc_m4, 8, p, w, 2, 1);
}
FTEST(ffcg_cmap_format0_mac_roman) {
    static const ttf_map m[] = { { 0x20, 1 }, { 0x41, 2 }, { 0x42, 3 }, { 0x7E, 5 } };
    static const UINT32 p[] = { 0x20, 0x41, 0x42, 0x43, 0x7E, 0x100 }; static const UINT16 w[] = { 1, 2, 3, 0, 5, 0 };
    ffc_idx_cmap(f, 0, 1, 0, m, 4, p, w, 6, 0);
}
FTEST(ffcg_cmap_format6_unicode_platform) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 3 }, { 0x43, 4 } };
    static const UINT32 p[] = { 0x40, 0x41, 0x42, 0x43, 0x44 }; static const UINT16 w[] = { 0, 2, 3, 4, 0 };
    ffc_idx_cmap(f, 6, 0, 3, m, 3, p, w, 5, 0);
}
FTEST(ffcg_cmap_format13_many_to_one) {
    static const ttf_map m[] = { { 0x41, 7 }, { 0x42, 7 }, { 0x43, 7 } };
    static const UINT32 p[] = { 0x41, 0x42, 0x43 }; static const UINT16 w[] = { 7, 7, 7 };
    ffc_idx_cmap(f, 13, 3, 10, m, 3, p, w, 3, 0);
}
FTEST(ffcg_cmap_3_1_and_3_10_prefer_note) {
    ttf_spec s; ttf_cmap c[2]; static const ttf_map a[] = { { 0x41, 5 } }, b[] = { { 0x41, 6 } }; dwx_font h; IDWriteFontFace *face; UINT32 cp = 0x41; UINT16 gi = 0;
    ttf_default_spec(&s);
    memset(c, 0, sizeof c);
    c[0].format = 4; c[0].plat = 3; c[0].enc = 1; c[0].n = 1; c[0].map = a;
    c[1].format = 12; c[1].plat = 3; c[1].enc = 10; c[1].n = 1; c[1].map = b;
    s.cmaps = c; s.ncmaps = 2;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gi));
    t_note("conflicting (3,1)->5 and (3,10)->6: U+0041 -> %u", gi);
    CHECK(gi == 5 || gi == 6);
    dwx_font_close(&h);
}
FTEST(ffcg_cmap_symbol_f000_range) {
    static const ttf_map m[] = { { 0xF020, 1 }, { 0xF041, 2 }, { 0xF042, 3 }, { 0xF0FF, 4 } };
    ttf_spec s; ttf_cmap c; dwx_font h; IDWriteFontFace *face; UINT32 cp[] = { 0xF020, 0xF041, 0xF042, 0xF0FF, 0xF043 }; UINT16 gi[5]; int i; static const UINT16 w[5] = { 1, 2, 3, 4, 0 };
    ttf_default_spec(&s);
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 0; c.n = 4; c.map = m; s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 5, gi));
    for (i = 0; i < 5; i++) CHECKF(gi[i] == w[i], "symbol U+%04X -> %u want %u", cp[i], gi[i], w[i]);
    dwx_font_close(&h);
}
FTEST(ffcg_cmap_symbol_low_codes_note) {
    static const ttf_map m[] = { { 0xF020, 1 }, { 0xF041, 2 }, { 0xF042, 3 } };
    ttf_spec s; ttf_cmap c; dwx_font h; IDWriteFontFace *face; UINT32 cp[] = { 0x20, 0x41, 0x42 }; UINT16 gi[3];
    ttf_default_spec(&s);
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 0; c.n = 3; c.map = m; s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 3, gi));
    t_note("symbol cmap, U+0020/0041/0042 -> %u %u %u (0xF0xx mapped to 1 2 3)", gi[0], gi[1], gi[2]);
    CHECK(gi[0] == 0 || gi[0] == 1);
    dwx_font_close(&h);
}
FTEST(ffcg_cmap_missing_note) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; UINT32 cp[2] = { 'A', ' ' }; UINT16 gi[2] = { 0xFFFF, 0xFFFF };
    ttf_default_spec(&s); s.omit |= TTF_OMIT_CMAP;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE);
    if (!face) { t_note("font without cmap cannot be opened as a face"); return; }
    { HRESULT hr = IDWriteFontFace_GetGlyphIndices(face, cp, 2, gi); t_note("no cmap: hr %s gi %u %u", dw_hr_name(hr), gi[0], gi[1]); CHECK(hr == S_OK || FAILED(hr)); if (hr == S_OK) { EXPECT_EQ(gi[0], 0); EXPECT_EQ(gi[1], 0); } }
    dwx_font_close(&h);
}
FTEST(ffcg_cmap_gid_beyond_glyph_count_note) {
    static const ttf_map m[] = { { 0x41, 5000 }, { 0x42, 2 } };
    ttf_spec s; ttf_cmap c; dwx_font h; IDWriteFontFace *face; UINT32 cp[2] = { 0x41, 0x42 }; UINT16 gi[2] = { 7, 7 };
    ttf_default_spec(&s);
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 1; c.n = 2; c.map = m; s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 2, gi));
    t_note("cmap maps U+0041 to glyph 5000 of %u glyphs -> %u", (unsigned)IDWriteFontFace_GetGlyphCount(face), gi[0]);
    EXPECT_EQ(gi[1], 2);
    dwx_font_close(&h);
}
FTEST(ffcg_cmap_explicit_notdef_mapping) {
    static const ttf_map m[] = { { 0x41, 0 }, { 0x42, 2 } };
    ttf_spec s; ttf_cmap c; dwx_font h; IDWriteFontFace *face; UINT32 cp[2] = { 0x41, 0x42 }; UINT16 gi[2] = { 9, 9 };
    ttf_default_spec(&s);
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 1; c.n = 2; c.map = m; s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 2, gi));
    EXPECT_EQ(gi[0], 0); EXPECT_EQ(gi[1], 2);
    dwx_font_close(&h);
}
FTEST(ffcg_cmap_dense_bmp_run) {
    /* 0x4E00..0x4E3F mapped to consecutive glyphs (one segment, idDelta) */
    static ttf_map m[64]; UINT32 cp[64]; UINT16 gi[64]; ttf_spec s; ttf_cmap c; dwx_font h; IDWriteFontFace *face; int i, bad = 0;
    for (i = 0; i < 64; i++) { m[i].cp = 0x4E00 + i; m[i].gid = 1 + i; cp[i] = 0x4E00 + i; }
    ttf_default_spec(&s);
    { int dn; ttf_glyph *g = ffc_glyphs_default(&dn); int k; for (k = dn; k < 70; k++) { memset(&g[k], 0, sizeof g[k]); g[k].adv = 500; } s.glyphs = g; s.nglyphs = 70; }
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 1; c.n = 64; c.map = m; s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 64, gi));
    for (i = 0; i < 64; i++) if (gi[i] != 1 + i && !bad++) CHECKF(0, "U+%X -> %u want %d", cp[i], gi[i], 1 + i);
    dwx_font_close(&h);
}

/* ---- threads ---------------------------------------------------------------------------------------------------------------------- */
typedef struct { IDWriteFontFace *face; int bad; } ffc_thr;
static DWORD WINAPI ffc_idx_thread(LPVOID p) {
    ffc_thr *t = (ffc_thr *)p; int i, k;
    for (i = 0; i < 300; i++) {
        UINT32 cp[8]; UINT16 gi[8];
        for (k = 0; k < 8; k++) cp[k] = 'A' + ((i + k) % 26);
        if (FAILED(IDWriteFontFace_GetGlyphIndices(t->face, cp, 8, gi))) { t->bad++; continue; }
        for (k = 0; k < 8; k++) if (gi[k] != ttf_default_gid(cp[k])) t->bad++;
    }
    return 0;
}
FTEST(ffcg_idx_concurrent_lookups) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); HANDLE th[4]; ffc_thr t[4]; int i; if (!face) return;
    for (i = 0; i < 4; i++) { t[i].face = face; t[i].bad = 0; th[i] = CreateThread(NULL, 0, ffc_idx_thread, &t[i], 0, NULL); }
    for (i = 0; i < 4; i++) if (th[i]) { CHECK(WaitForSingleObject(th[i], 20000) == WAIT_OBJECT_0); CloseHandle(th[i]); }
    for (i = 0; i < 4; i++) EXPECT_EQ(t[i].bad, 0);
    dwx_font_close(&h);
}

/* ---- system fonts --------------------------------------------------------------------------------------------------------------------- */
/* (3,1) format 4 lookup straight from a cmap table dump */
static int ffc_cmap31_lookup(const uint8_t *cm, unsigned len, unsigned cp, unsigned *gid, int *found_table) {
    unsigned n = ffc_be16(cm + 2), i, off = 0;
    for (i = 0; i < n; i++) { const uint8_t *e = cm + 4 + 8 * i; if (ffc_be16(e) == 3 && ffc_be16(e + 2) == 1) off = ffc_be32(e + 4); }
    *found_table = 0; *gid = 0;
    if (!off || off + 14 > len || ffc_be16(cm + off) != 4) return 0;
    *found_table = 1;
    {
        const uint8_t *t = cm + off; unsigned segx2 = ffc_be16(t + 6), sc = segx2 / 2, k;
        const uint8_t *ends = t + 14, *starts = ends + segx2 + 2, *deltas = starts + segx2, *ranges = deltas + segx2;
        if (cp > 0xFFFF) return 1;
        for (k = 0; k < sc; k++) {
            if (cp <= ffc_be16(ends + 2 * k)) {
                unsigned st = ffc_be16(starts + 2 * k), ro = ffc_be16(ranges + 2 * k), d = ffc_be16(deltas + 2 * k);
                if (cp < st) return 1;
                if (!ro) *gid = (cp + d) & 0xFFFF;
                else { const uint8_t *g = ranges + 2 * k + ro + 2 * (cp - st); unsigned v = ffc_be16(g); *gid = v ? ((v + d) & 0xFFFF) : 0; }
                return 1;
            }
        }
    }
    return 1;
}
static void ffc_sys_idx(IDWriteFactory *f, const WCHAR *family) {
    IDWriteFontFace *face = dw_sysface(f, family, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    UINT32 cl = 0; uint8_t *cm; UINT32 cp[600]; UINT16 gi[600]; int i, bad = 0, found = 0;
    if (!face) { t_skip("%ls not installed", family); return; }
    cm = dw_face_table(face, DW_TAG('c','m','a','p'), &cl);
    if (!cm) { t_skip("no cmap table"); IDWriteFontFace_Release(face); return; }
    for (i = 0; i < 600; i++) cp[i] = i < 400 ? (UINT32)i : 0x2000 + (i - 400);
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 600, gi));
    for (i = 0; i < 600; i++) {
        unsigned want = 0; int ft = 0; ffc_cmap31_lookup(cm, cl, cp[i], &want, &ft);
        if (!ft) { t_note("%ls has no (3,1) format 4 subtable", family); break; }
        found = 1;
        if (gi[i] != want && !bad++) CHECKF(0, "%ls U+%04X -> %u, cmap table says %u", family, cp[i], gi[i], want);
    }
    if (found) { CHECK(gi['A'] != 0); CHECK(gi[' '] != 0); CHECK(gi['A'] != gi['B']); }
    free(cm); IDWriteFontFace_Release(face);
}
FTEST(ffcg_sys_arial_matches_cmap_table) { ffc_sys_idx(f, L"Arial"); }
FTEST(ffcg_sys_times_matches_cmap_table) { ffc_sys_idx(f, L"Times New Roman"); }
FTEST(ffcg_sys_segoeui_matches_cmap_table) { ffc_sys_idx(f, L"Segoe UI"); }
FTEST(ffcg_sys_consolas_matches_cmap_table) { ffc_sys_idx(f, L"Consolas"); }
FTEST(ffcg_sys_marlett_symbol_range) {
    IDWriteFontFace *face = dw_sysface(f, L"Marlett", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); UINT32 cp[] = { 0x30, 0x31, 0xF030, 0xF031, 0x72 }; UINT16 gi[5];
    if (!face) { t_skip("Marlett not installed"); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 5, gi));
    t_note("Marlett: U+0030 %u U+0031 %u U+F030 %u U+F031 %u U+0072 %u", gi[0], gi[1], gi[2], gi[3], gi[4]);
    CHECK(gi[2] != 0 || gi[0] != 0);
    IDWriteFontFace_Release(face);
}
FTEST(ffcg_sys_arial_symbols_present) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); UINT32 cp[] = { 0xE9, 0x20AC, 0x0416, 0x03A9, 0x2022 }; UINT16 gi[5]; int i;
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 5, gi));
    for (i = 0; i < 5; i++) CHECKF(gi[i] != 0, "Arial lacks U+%04X", cp[i]);
    IDWriteFontFace_Release(face);
}

/* ================================== GetDesignGlyphMetrics ============================================================================= */
FTEST(ffcg_dgm_box_glyph_exact) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS m; if (!face) return;
    if (ffc_dgm(face, 2, &m, FALSE)) CHECK_GM(m, 100, 1000, 100, 200, 2020, 420, 1600);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_all_letters_same_as_box) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT16 g[62]; DWRITE_GLYPH_METRICS m[62]; int i; if (!face) return;
    for (i = 0; i < 62; i++) g[i] = 2 + i;     /* gid 2..63: A-Z a-z 0-9, all the same box */
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 62, m, FALSE));
    for (i = 0; i < 62; i++) CHECKF(m[i].leftSideBearing == 100 && m[i].advanceWidth == 1000 && m[i].rightSideBearing == 100 && m[i].topSideBearing == 200 &&
                                    m[i].advanceHeight == 2020 && m[i].bottomSideBearing == 420 && m[i].verticalOriginY == 1600, "gid %u", g[i]);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_notdef) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS m; if (!face) return;
    if (ffc_dgm(face, 0, &m, FALSE)) CHECK_GM(m, 100, 1200, 100, 200, 2020, 420, 1600);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_triangle_and_ring_same_bbox) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS m; if (!face) return;
    if (ffc_dgm(face, TTF_GID_TRIANGLE, &m, FALSE)) CHECK_GM(m, 100, 1000, 100, 200, 2020, 420, 1600);
    if (ffc_dgm(face, TTF_GID_RING, &m, FALSE)) CHECK_GM(m, 100, 1000, 100, 200, 2020, 420, 1600);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_composite_glyph) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS m; if (!face) return;
    /* 'A' box plus a triangle lifted by 1400: ink 0..2800, advance from the first component (hmtx says 1000) */
    if (ffc_dgm(face, TTF_GID_COMPOSITE, &m, FALSE)) CHECK_GM(m, 100, 1000, 100, -1200, 2020, 420, 1600);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_round_glyph_consistency) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS m; if (!face) return;
    if (ffc_dgm(face, TTF_GID_ROUND, &m, FALSE)) {
        EXPECT_EQ(m.advanceWidth, 1000); EXPECT_EQ(m.advanceHeight, 2020); EXPECT_EQ(m.verticalOriginY, 1600);
        CHECK((long long)m.advanceWidth - m.leftSideBearing - m.rightSideBearing > 0);
        CHECK((long long)m.advanceHeight - m.topSideBearing - m.bottomSideBearing > 0);
        t_note("round glyph: lsb %d rsb %d tsb %d bsb %d", m.leftSideBearing, m.rightSideBearing, m.topSideBearing, m.bottomSideBearing);
    }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_space_and_empty_glyphs) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS m; if (!face) return;
    if (ffc_dgm(face, TTF_GID_SPACE, &m, FALSE)) { EXPECT_EQ(m.advanceWidth, 1000); EXPECT_EQ(m.leftSideBearing, 0); EXPECT_EQ(m.advanceHeight, 2020); EXPECT_EQ(m.verticalOriginY, 1600);
        t_note("space: rsb %d tsb %d bsb %d", m.rightSideBearing, m.topSideBearing, m.bottomSideBearing); CHECK(!ffc_has_size(&m)); }
    if (ffc_dgm(face, TTF_GID_EMPTY500, &m, FALSE)) { EXPECT_EQ(m.advanceWidth, 500); EXPECT_EQ(m.leftSideBearing, 0); CHECK(!ffc_has_size(&m)); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_ink_extent_identity) {
    /* advance - lsb - rsb = ink width and advanceHeight - tsb - bsb = ink height for every glyph with an outline */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const UINT16 g[] = { 0, 2, 28, 54, 64, 66, 68 }; static const int w[] = { 1000, 800, 800, 800, 800, 800, 800 }, ht[] = { 1400, 1400, 1400, 1400, 1400, 2800, 1400 }; int i;
    if (!face) return;
    for (i = 0; i < 7; i++) {
        DWRITE_GLYPH_METRICS m; if (!ffc_dgm(face, g[i], &m, FALSE)) continue;
        CHECKF((int)m.advanceWidth - m.leftSideBearing - m.rightSideBearing == w[i], "gid %u width", g[i]);
        CHECKF((int)m.advanceHeight - m.topSideBearing - m.bottomSideBearing == ht[i], "gid %u height", g[i]);
    }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_hmtx_lsb_decouples_from_outline) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ffc_extra_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    /* lsb comes from hmtx (150) while the outline starts at 100: rsb = adv - lsb - width = 1000 - 150 - 800 */
    if (ffc_dgm(face, FFC_G_LSB150, &m, FALSE)) { EXPECT_EQ(m.leftSideBearing, 150); EXPECT_EQ(m.rightSideBearing, 50); EXPECT_EQ(m.advanceWidth, 1000); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_negative_rsb) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ffc_extra_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    if (ffc_dgm(face, FFC_G_NARROW, &m, FALSE)) { EXPECT_EQ(m.advanceWidth, 600); EXPECT_EQ(m.leftSideBearing, 100); EXPECT_EQ(m.rightSideBearing, 600 - 100 - 800); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_negative_bottom_bearing_glyph) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ffc_extra_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    /* ink -300..1400: bottomSideBearing = 2020 - 200 - 1700 = 120 */
    if (ffc_dgm(face, FFC_G_LOW, &m, FALSE)) CHECK_GM(m, 100, 1000, 100, 200, 2020, 120, 1600);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_zero_advance_mark) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ffc_extra_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    /* ink -50..50, advance 0: lsb -50, rsb = adv - lsb - width = 0 + 50 - 100 = -50 */
    if (ffc_dgm(face, FFC_G_MARK, &m, FALSE)) { EXPECT_EQ(m.advanceWidth, 0); EXPECT_EQ(m.leftSideBearing, -50); EXPECT_EQ(m.rightSideBearing, -50); EXPECT_EQ(m.topSideBearing, 900); EXPECT_EQ(m.bottomSideBearing, 420); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_vertical_metrics_from_vmtx) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ffc_extra_spec(&s);
    s.vertical = 1; s.v_ascent = 1500; s.v_descent = -500;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    /* box glyph: vmtx advance = upm (2048), tsb = vAscent - yMax = 100, vertical origin = yMax + tsb = 1500, bsb = 2048 - 100 - 1400 */
    if (ffc_dgm(face, 2, &m, FALSE)) CHECK_GM(m, 100, 1000, 100, 100, 2048, 548, 1500);
    /* explicit vmtx entry: advance 900, tsb 300 => origin 1700, bsb = 900 - 300 - 1400 = -800 */
    if (ffc_dgm(face, FFC_G_VERT, &m, FALSE)) CHECK_GM(m, 100, 1000, 100, 300, 900, -800, 1700);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_vertical_defaults_follow_typo_metrics) {
    /* no vhea: advanceHeight = typoAscender + |typoDescender|, origin = typoAscender (independent of USE_TYPO_METRICS) */
    static const struct { int a, d, fs; } t[] = { { 1600, -420, 0x40 }, { 1000, -300, 0x40 }, { 1000, -300, 0xC0 }, { 2000, -1, 0x40 }, { 0, 0, 0x40 } }; int i;
    for (i = 0; i < 5; i++) {
        ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ttf_default_spec(&s); s.typo_asc = t[i].a; s.typo_desc = t[i].d; s.fs_selection = t[i].fs;
        face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
        if (ffc_dgm(face, 2, &m, FALSE)) {
            CHECKF((int)m.advanceHeight == t[i].a - t[i].d, "row %d advanceHeight %u want %d", i, m.advanceHeight, t[i].a - t[i].d);
            CHECKF(m.verticalOriginY == t[i].a, "row %d origin %d want %d", i, m.verticalOriginY, t[i].a);
            CHECKF(m.topSideBearing == t[i].a - 1400, "row %d tsb %d", i, m.topSideBearing);
        }
        dwx_font_close(&h);
    }
}
FTEST(ffcg_dgm_vertical_defaults_without_os2_use_hhea) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    /* CalculateTypoAscenderDescender takes hhea.ascender / hhea.descender as is when OS/2 is missing (no absolute value): a normal negative
     * descender therefore contributes 0 to the default advance height */
    if (ffc_dgm(face, 2, &m, FALSE)) { CHECK(m.verticalOriginY == m.advanceHeight || m.verticalOriginY > 0); t_note("no-OS/2 default vertical: origin=%d advanceHeight=%d", (int)m.verticalOriginY, (int)m.advanceHeight); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_sideways_flag_no_effect) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const UINT16 g[] = { 0, 1, 2, 64, 66, 67 }; int i; if (!face) return;
    for (i = 0; i < 6; i++) { DWRITE_GLYPH_METRICS a, b; if (ffc_dgm(face, g[i], &a, FALSE) && ffc_dgm(face, g[i], &b, TRUE)) CHECKF(ffc_gm_eq(&a, &b), "gid %u differs with isSideways", g[i]); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_array_equals_singles) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT16 g[9] = { 68, 0, 2, 2, 1, 66, 64, 67, 65 }; DWRITE_GLYPH_METRICS m[9], one; int i; if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 9, m, FALSE));
    for (i = 0; i < 9; i++) if (ffc_dgm(face, g[i], &one, FALSE)) CHECKF(ffc_gm_eq(&m[i], &one), "index %d gid %u", i, g[i]);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_large_batch) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static UINT16 g[2000]; static DWRITE_GLYPH_METRICS m[2000]; int i, bad = 0; if (!face) return;
    for (i = 0; i < 2000; i++) g[i] = (UINT16)(i % TTF_DEF_NGLYPHS);
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 2000, m, FALSE));
    for (i = TTF_DEF_NGLYPHS; i < 2000; i++) if (!ffc_gm_eq(&m[i], &m[i - TTF_DEF_NGLYPHS]) && !bad++) CHECKF(0, "entry %d differs from entry %d", i, i - TTF_DEF_NGLYPHS);
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_null_indices_invalidarg_zeroes) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS m[3]; int i; if (!face) return;
    memset(m, 0xCC, sizeof m);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(face, NULL, 3, m, FALSE), E_INVAL);
    for (i = 0; i < 3; i++) { static const DWRITE_GLYPH_METRICS z; CHECK(ffc_gm_eq(&m[i], &z)); }
    memset(m, 0xCC, sizeof m);
    { HRESULT hr0 = IDWriteFontFace_GetDesignGlyphMetrics(face, NULL, 0, m, FALSE); t_note("NULL indices with count 0 -> 0x%08lx", (unsigned long)hr0); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_zero_count_succeeds) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT16 g = 2; DWRITE_GLYPH_METRICS m; if (!face) return;
    memset(&m, 0xCC, sizeof m);
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, &g, 0, &m, FALSE));
    { DWRITE_GLYPH_METRICS c; memset(&c, 0xCC, sizeof c); CHECK(ffc_gm_eq(&m, &c)); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_out_of_range_glyph_note) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT16 g[3] = { TTF_DEF_NGLYPHS, 0x7FFF, 0xFFFF }; int i; if (!face) return;
    for (i = 0; i < 3; i++) {
        DWRITE_GLYPH_METRICS m; HRESULT hr; memset(&m, 0xCC, sizeof m);
        hr = IDWriteFontFace_GetDesignGlyphMetrics(face, &g[i], 1, &m, FALSE);
        t_note("glyph %u (count %d): %s lsb %d adv %u rsb %d tsb %d advH %u bsb %d org %d", g[i], TTF_DEF_NGLYPHS, dw_hr_name(hr), m.leftSideBearing, m.advanceWidth, m.rightSideBearing, m.topSideBearing, m.advanceHeight, m.bottomSideBearing, m.verticalOriginY);
    }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_compressed_hmtx_tail) {
    /* numberOfHMetrics < numGlyphs: the trailing glyphs repeat the last advance and take their lsb from the array that follows */
    int dn, k; ttf_glyph *g = ffc_glyphs_default(&dn); ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m;
    ttf_default_spec(&s);
    for (k = dn; k < 100; k++) { memset(&g[k], 0, sizeof g[k]); g[k].adv = 500; }
    g[68].adv = 500;     /* last "real" advance equals the tail so the writer can compress */
    s.glyphs = g; s.nglyphs = 100; s.compress_hmtx = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    if (ffc_dgm(face, 99, &m, FALSE)) { EXPECT_EQ(m.advanceWidth, 500); EXPECT_EQ(m.leftSideBearing, 0); }
    if (ffc_dgm(face, 90, &m, FALSE)) { EXPECT_EQ(m.advanceWidth, 500); }
    if (ffc_dgm(face, 2, &m, FALSE)) { EXPECT_EQ(m.advanceWidth, 1000); EXPECT_EQ(m.leftSideBearing, 100); }
    dwx_font_close(&h);
}
FTEST(ffcg_dgm_upm_independence) {
    /* values are design units, whatever the em size */
    static const int up[] = { 1000, 2048, 4096 }; int i;
    for (i = 0; i < 3; i++) {
        ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ttf_default_spec(&s); s.upm = up[i];
        face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
        if (ffc_dgm(face, 2, &m, FALSE)) CHECK_GM(m, 100, 1000, 100, 200, 2020, 420, 1600);
        dwx_font_close(&h);
    }
}

/* ---- simulations on design glyph metrics -------------------------------------------------------------------------------------------------- */
static void ffc_sim_glyph_case(IDWriteFactory *f, UINT16 gid, int bold, int oblique) {
    ttf_spec s; dwx_font h0, h1; IDWriteFontFace *base, *sim; DWRITE_GLYPH_METRICS b, m, want; DWRITE_FONT_SIMULATIONS sm = (bold ? DWRITE_FONT_SIMULATIONS_BOLD : 0) | (oblique ? DWRITE_FONT_SIMULATIONS_OBLIQUE : 0);
    ffc_extra_spec(&s);
    base = ffc_open(&h0, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!base) return;
    sim = ffc_open(&h1, f, &s, sm); if (!sim) { dwx_font_close(&h0); return; }
    if (ffc_dgm(base, gid, &b, FALSE) && ffc_dgm(sim, gid, &m, FALSE)) {
        want = b;
        if (bold) want = ffc_model_bold(want, FFC_UPM);
        if (oblique) want = ffc_model_oblique(want);
        CHECKF(ffc_gm_eq(&m, &want), "gid %u bold %d oblique %d: got lsb %d adv %u rsb %d tsb %d advH %u bsb %d org %d / want %d %u %d %d %u %d %d", gid, bold, oblique,
               m.leftSideBearing, m.advanceWidth, m.rightSideBearing, m.topSideBearing, m.advanceHeight, m.bottomSideBearing, m.verticalOriginY,
               want.leftSideBearing, want.advanceWidth, want.rightSideBearing, want.topSideBearing, want.advanceHeight, want.bottomSideBearing, want.verticalOriginY);
    }
    dwx_font_close(&h1); dwx_font_close(&h0);
}
FTEST(ffcg_sim_bold_box_exact) {
    dwx_font h; ttf_spec s; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ttf_default_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_BOLD); if (!face) return;
    /* d = (2048 + 25) / 50 = 41: advanceWidth, advanceHeight and verticalOriginY grow, bearings stay */
    if (ffc_dgm(face, 2, &m, FALSE)) CHECK_GM(m, 100, 1041, 100, 200, 2061, 420, 1641);
    dwx_font_close(&h);
}
FTEST(ffcg_sim_bold_space_unchanged) {
    dwx_font h; ttf_spec s; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ttf_default_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_BOLD); if (!face) return;
    if (ffc_dgm(face, TTF_GID_SPACE, &m, FALSE)) { t_note("bold space advance %u advH %u", m.advanceWidth, m.advanceHeight); EXPECT_EQ(m.advanceWidth, 1000); EXPECT_EQ(m.advanceHeight, 2020); }
    dwx_font_close(&h);
}
FTEST(ffcg_sim_bold_amount_follows_upm) {
    static const int up[] = { 1000, 1024, 2048, 2500, 4096 }; int i;
    for (i = 0; i < 5; i++) {
        dwx_font h; ttf_spec s; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; int d = (up[i] + 25) / 50; ttf_default_spec(&s); s.upm = up[i];
        face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_BOLD); if (!face) return;
        if (ffc_dgm(face, 2, &m, FALSE)) { CHECKF((int)m.advanceWidth == 1000 + d, "upm %d adv %u want %d", up[i], m.advanceWidth, 1000 + d); CHECK((int)m.advanceHeight == 2020 + d); }
        dwx_font_close(&h);
    }
}
FTEST(ffcg_sim_bold_model_all_glyphs) {
    UINT16 g; for (g = 0; g < FFC_NG; g++) ffc_sim_glyph_case(f, g, 1, 0);
}
FTEST(ffcg_sim_bold_zero_advance_mark) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ffc_extra_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_BOLD); if (!face) return;
    /* a zero advance cannot grow: the right side bearing absorbs the emboldening */
    if (ffc_dgm(face, FFC_G_MARK, &m, FALSE)) { EXPECT_EQ(m.advanceWidth, 0); EXPECT_EQ(m.rightSideBearing, -50 - 41); EXPECT_EQ(m.advanceHeight, 2020 + 41); }
    dwx_font_close(&h);
}
FTEST(ffcg_sim_bold_vertical_font) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ffc_extra_spec(&s); s.vertical = 1; s.v_ascent = 1500; s.v_descent = -500;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_BOLD); if (!face) return;
    if (ffc_dgm(face, 2, &m, FALSE)) CHECK_GM(m, 100, 1041, 100, 100, 2048 + 41, 548, 1541);
    dwx_font_close(&h);
}
FTEST(ffcg_sim_oblique_box_exact) {
    dwx_font h; ttf_spec s; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ttf_default_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE); if (!face) return;
    /* yMin = 0 => lsb unchanged; yMax = 1400 => rsb -= (1400 * 0x5700) >> 16 = 475 */
    if (ffc_dgm(face, 2, &m, FALSE)) CHECK_GM(m, 100, 1000, 100 - 475, 200, 2020, 420, 1600);
    dwx_font_close(&h);
}
FTEST(ffcg_sim_oblique_descending_glyph) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ffc_extra_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE); if (!face) return;
    /* yMin = -300 => lsb += (-300 * 0x5700) >> 16 = -102 (arithmetic shift rounds toward minus infinity) */
    if (ffc_dgm(face, FFC_G_LOW, &m, FALSE)) { EXPECT_EQ(m.leftSideBearing, 100 + ffc_shear(-300)); EXPECT_EQ(m.leftSideBearing, -2); EXPECT_EQ(m.rightSideBearing, 100 - 475); }
    dwx_font_close(&h);
}
FTEST(ffcg_sim_oblique_composite_tall_glyph) {
    dwx_font h; ttf_spec s; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m; ttf_default_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE); if (!face) return;
    if (ffc_dgm(face, TTF_GID_COMPOSITE, &m, FALSE)) { EXPECT_EQ(m.rightSideBearing, 100 - ffc_shear(2800)); EXPECT_EQ(m.leftSideBearing, 100); }
    dwx_font_close(&h);
}
FTEST(ffcg_sim_oblique_empty_glyph_unchanged) {
    dwx_font h; ttf_spec s; IDWriteFontFace *face, *base; dwx_font h0; DWRITE_GLYPH_METRICS m, b; ttf_default_spec(&s);
    base = ffc_open(&h0, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!base) return;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE); if (!face) { dwx_font_close(&h0); return; }
    if (ffc_dgm(face, TTF_GID_SPACE, &m, FALSE) && ffc_dgm(base, TTF_GID_SPACE, &b, FALSE)) CHECK(ffc_gm_eq(&m, &b));
    if (ffc_dgm(face, TTF_GID_EMPTY500, &m, FALSE) && ffc_dgm(base, TTF_GID_EMPTY500, &b, FALSE)) CHECK(ffc_gm_eq(&m, &b));
    dwx_font_close(&h); dwx_font_close(&h0);
}
FTEST(ffcg_sim_oblique_model_all_glyphs) {
    UINT16 g; for (g = 0; g < FFC_NG; g++) ffc_sim_glyph_case(f, g, 0, 1);
}
FTEST(ffcg_sim_bold_oblique_model_all_glyphs) {
    UINT16 g; for (g = 0; g < FFC_NG; g++) ffc_sim_glyph_case(f, g, 1, 1);
}
FTEST(ffcg_sim_oblique_sideways_note) {
    dwx_font h; ttf_spec s; IDWriteFontFace *face; DWRITE_GLYPH_METRICS m, n; ttf_default_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE); if (!face) return;
    if (ffc_dgm(face, 2, &m, FALSE) && ffc_dgm(face, 2, &n, TRUE)) t_note("oblique sideways metrics equal upright metrics: %d", ffc_gm_eq(&m, &n));
    dwx_font_close(&h);
}
FTEST(ffcg_sim_bold_does_not_change_glyph_indices_or_count) {
    dwx_font h; ttf_spec s; IDWriteFontFace *face; UINT32 cp = 'A'; UINT16 gi; ttf_default_spec(&s);
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gi)); EXPECT_EQ(gi, 2);
    EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
    dwx_font_close(&h);
}

/* ---- system fonts ------------------------------------------------------------------------------------------------------------------------- */
FTEST(ffcg_sys_arial_dgm_matches_hmtx) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); UINT32 hl = 0, hhl = 0; uint8_t *hmtx, *hhea; UINT32 cp[] = { 'A', 'g', ' ', 'W', '.', 'i' }; UINT16 gi[6]; int i;
    if (!face) { t_skip("Arial not installed"); return; }
    hmtx = dw_face_table(face, DW_TAG('h','m','t','x'), &hl); hhea = dw_face_table(face, DW_TAG('h','h','e','a'), &hhl);
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 6, gi));
    if (hmtx && hhea && hhl >= 36) {
        unsigned nh = ffc_be16(hhea + 34);
        for (i = 0; i < 6; i++) {
            DWRITE_GLYPH_METRICS m; unsigned adv, lsb;
            if (!ffc_dgm(face, gi[i], &m, FALSE)) continue;
            if (gi[i] < nh) { adv = ffc_be16(hmtx + 4 * gi[i]); lsb = ffc_be16(hmtx + 4 * gi[i] + 2); } else { adv = ffc_be16(hmtx + 4 * (nh - 1)); lsb = ffc_be16(hmtx + 4 * nh + 2 * (gi[i] - nh)); }
            CHECKF(m.advanceWidth == adv, "gid %u advance %u hmtx %u", gi[i], m.advanceWidth, adv);
            CHECKF(m.leftSideBearing == (short)lsb, "gid %u lsb %d hmtx %d", gi[i], m.leftSideBearing, (short)lsb);
            if (cp[i] != ' ') CHECKF((int)m.advanceWidth - m.leftSideBearing - m.rightSideBearing > 0, "gid %u has no ink width", gi[i]);
            t_note("Arial U+%04X gid %u: adv %u lsb %d rsb %d tsb %d bsb %d advH %u org %d", cp[i], gi[i], m.advanceWidth, m.leftSideBearing, m.rightSideBearing, m.topSideBearing, m.bottomSideBearing, m.advanceHeight, m.verticalOriginY);
        }
    }
    free(hmtx); free(hhea); IDWriteFontFace_Release(face);
}
FTEST(ffcg_sys_arial_vertical_defaults) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); UINT32 ol = 0; uint8_t *os2; UINT32 cp = 'A'; UINT16 gi; DWRITE_GLYPH_METRICS m;
    if (!face) { t_skip("Arial not installed"); return; }
    os2 = dw_face_table(face, DW_TAG('O','S','/','2'), &ol);
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gi));
    if (os2 && ol >= 78 && ffc_dgm(face, gi, &m, FALSE)) {
        int ta = ffc_s16(os2 + 68), td = ffc_s16(os2 + 70); (void)td;
        if (ta >= 0) { EXPECT_EQ(m.verticalOriginY, ta); EXPECT_EQ(m.advanceHeight, ta + abs(td)); }
    }
    free(os2); IDWriteFontFace_Release(face);
}
FTEST(ffcg_sys_arial_bold_sim_model) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL), *bold = NULL; UINT32 cp = 'H'; UINT16 gi; DWRITE_GLYPH_METRICS b, s; DWRITE_FONT_METRICS fm; UINT32 n = 1; IDWriteFontFile *file = NULL;
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &file));
    if (file && SUCCEEDED(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &bold)) && bold) {
        IDWriteFontFace_GetMetrics(face, &fm);
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gi));
        if (ffc_dgm(face, gi, &b, FALSE) && ffc_dgm(bold, gi, &s, FALSE)) { DWRITE_GLYPH_METRICS w = ffc_model_bold(b, fm.designUnitsPerEm); CHECK(ffc_gm_eq(&s, &w)); }
        DW_RELEASE(bold);
    }
    DW_RELEASE(file); IDWriteFontFace_Release(face);
}

/* ================================== GetGdiCompatibleGlyphMetrics ============================================================================ */
static HRESULT ffc_gdi_gm(IDWriteFontFace *face, float em, float ppd, const DWRITE_MATRIX *x, BOOL natural, UINT16 gid, DWRITE_GLYPH_METRICS *m, BOOL side) {
    IDWriteFontFace1 *f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); HRESULT hr = E_NOINTERFACE; UINT16 g = gid;
    memset(m, 0xCC, sizeof *m);
    if (f1) { hr = IDWriteFontFace1_GetGdiCompatibleGlyphMetrics(f1, em, ppd, x, natural, &g, 1, m, side); IDWriteFontFace1_Release(f1); }
    return hr;
}
FTEST(ffcg_gdi_gm_null_indices) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace1 *f1; DWRITE_GLYPH_METRICS m[2]; if (!face) return;
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); if (!f1) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    memset(m, 0xCC, sizeof m);
    CHECK_HR(IDWriteFontFace1_GetGdiCompatibleGlyphMetrics(f1, 16, 1, NULL, FALSE, NULL, 2, m, FALSE), E_INVAL);
    { static const DWRITE_GLYPH_METRICS z; CHECK(ffc_gm_eq(&m[0], &z)); CHECK(ffc_gm_eq(&m[1], &z)); }
    IDWriteFontFace1_Release(f1); dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_bad_sizes) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const float em[] = { 0, -1, 16, 16, 0, -16 }, pp[] = { 16, 1, 0, -1, 0, -1 }; int i; if (!face) return;
    for (i = 0; i < 6; i++) {
        DWRITE_GLYPH_METRICS m; HRESULT hr = ffc_gdi_gm(face, em[i], pp[i], NULL, FALSE, 2, &m, FALSE);
        if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); break; }
        CHECKF(hr == E_INVAL, "emSize %g ppd %g -> %s", em[i], pp[i], dw_hr_name(hr));
        { static const DWRITE_GLYPH_METRICS z; CHECK(ffc_gm_eq(&m, &z)); }
    }
    dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_zero_count_ok) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace1 *f1; UINT16 g = 2; DWRITE_GLYPH_METRICS m; if (!face) return;
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); if (!f1) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleGlyphMetrics(f1, 16, 1, NULL, FALSE, &g, 0, &m, FALSE));
    IDWriteFontFace1_Release(f1); dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_within_a_pixel_of_design) {
    /* GDI-compatible advances differ from the scaled design advance by less than one pixel (here: one design pixel = upm / ppem units) */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const float sizes[] = { 8, 10, 12, 13, 16, 20, 24, 32, 48, 72 }; int i, nat; if (!face) return;
    for (nat = 0; nat < 2; nat++) for (i = 0; i < 10; i++) {
        DWRITE_GLYPH_METRICS m; HRESULT hr = ffc_gdi_gm(face, sizes[i], 1, NULL, nat, 2, &m, FALSE); double upp = 2048.0 / sizes[i];
        if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
        CHECK_HR_OK(hr);
        CHECKF(fabs((double)m.advanceWidth - 1000.0) <= upp, "em %g natural %d: advance %u vs design 1000 (pixel = %g units)", sizes[i], nat, m.advanceWidth, upp);
        t_note("gdi glyph metrics 'A' em %g natural %d: lsb %d adv %u rsb %d | advH %u org %d", sizes[i], nat, m.leftSideBearing, m.advanceWidth, m.rightSideBearing, m.advanceHeight, m.verticalOriginY);
    }
    dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_identity_matrix_equals_null) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 }; DWRITE_GLYPH_METRICS a, b; HRESULT ha, hb; int nat; if (!face) return;
    for (nat = 0; nat < 2; nat++) {
        ha = ffc_gdi_gm(face, 16, 1, NULL, nat, 2, &a, FALSE); hb = ffc_gdi_gm(face, 16, 1, &id, nat, 2, &b, FALSE);
        if (ha == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); break; }
        CHECK_HR_OK(ha); CHECK_HR_OK(hb); CHECK(ffc_gm_eq(&a, &b));
    }
    dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_size_product_symmetry) {
    /* emSize * pixelsPerDip is what counts: 8 * 2 behaves like 16 * 1 */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS a, b, c; HRESULT ha; if (!face) return;
    ha = ffc_gdi_gm(face, 16, 1, NULL, FALSE, 2, &a, FALSE); if (ha == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    CHECK_HR_OK(ffc_gdi_gm(face, 8, 2, NULL, FALSE, 2, &b, FALSE)); CHECK_HR_OK(ffc_gdi_gm(face, 32, 0.5f, NULL, FALSE, 2, &c, FALSE));
    CHECK(ffc_gm_eq(&a, &b)); CHECK(ffc_gm_eq(&a, &c));
    dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_classic_vs_natural_note) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS a, b; int i; static const float sizes[] = { 9, 11, 13, 15, 17, 19 }; if (!face) return;
    for (i = 0; i < 6; i++) {
        if (ffc_gdi_gm(face, sizes[i], 1, NULL, FALSE, 2, &a, FALSE) == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); break; }
        ffc_gdi_gm(face, sizes[i], 1, NULL, TRUE, 2, &b, FALSE);
        t_note("em %g: classic advance %u, natural advance %u (design 1000, %.3f px)", sizes[i], a.advanceWidth, b.advanceWidth, 1000.0 * sizes[i] / 2048.0);
    }
    dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_space_has_no_ink) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_GLYPH_METRICS m; if (!face) return;
    if (ffc_gdi_gm(face, 16, 1, NULL, FALSE, TTF_GID_SPACE, &m, FALSE) == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    t_note("gdi space 16px: adv %u lsb %d rsb %d", m.advanceWidth, m.leftSideBearing, m.rightSideBearing);
    CHECK(m.advanceWidth > 0);
    dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_array_equals_singles) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace1 *f1; UINT16 g[5] = { 2, 0, 66, 1, 68 }; DWRITE_GLYPH_METRICS m[5], one; int i; if (!face) return;
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); if (!f1) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleGlyphMetrics(f1, 14, 1, NULL, TRUE, g, 5, m, FALSE));
    for (i = 0; i < 5; i++) { ffc_gdi_gm(face, 14, 1, NULL, TRUE, g[i], &one, FALSE); CHECKF(ffc_gm_eq(&m[i], &one), "index %d", i); }
    IDWriteFontFace1_Release(f1); dwx_font_close(&h);
}
FTEST(ffcg_gdi_gm_sys_arial_pixel_advances) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); UINT32 cp = 'A'; UINT16 gi; DWRITE_GLYPH_METRICS m, d; int i; static const float sizes[] = { 9, 10, 11, 12, 13, 14, 16, 20, 24 };
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gi));
    if (!ffc_dgm(face, gi, &d, FALSE)) { IDWriteFontFace_Release(face); return; }
    for (i = 0; i < 9; i++) {
        double upp = 2.0 * 2048.0 / sizes[i];   /* GDI-classic advances are hinted to whole pixels; Win10 deviates from design by just over 1 px */
        if (ffc_gdi_gm(face, sizes[i], 1, NULL, FALSE, gi, &m, FALSE) == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); break; }
        t_note("Arial 'A' %g px: gdi-classic advance %u (design %u)", sizes[i], m.advanceWidth, d.advanceWidth);
        CHECKF(fabs((double)m.advanceWidth - d.advanceWidth) <= upp, "em %g", sizes[i]);
    }
    IDWriteFontFace_Release(face);
}
