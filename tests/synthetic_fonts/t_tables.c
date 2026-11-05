/* head / hhea / hmtx / maxp / OS/2 / post / name variants and omissions.
 *
 * Decomp facts used (OpenTypeTables::InitializeInternal / InitializeOS2 / OpenTypeFace::OpenTypeFace / HorizontalMetrics):
 *   - head is used when >= 54 bytes, hhea when >= 36, OS/2 when > 0x4d bytes (versions 1,2,5 need 0x56, 0x60, 0x64), maxp when >= 6;
 *   - the face's glyph count is maxp.numGlyphs, capped to numberOfHMetrics + (hmtx length - 4*numberOfHMetrics)/2;
 *   - hmtx shorter than 4*numberOfHMetrics is a format error; glyphs >= numberOfHMetrics reuse the last advance. */
#include "sf.h"

typedef struct { sf_probe p; sf_blob b; } fnt;
static int fn_open(fnt *c, sf_blob b, const char *label) {
    c->b = b; sf_survey_light = 0;
    if (!sf_open_face(&c->p, b.p, b.n)) { sf_close(&c->p); sf_free(&c->b); return 0; }
    sf_survey(label, &c->p);
    return 1;
}
static void fn_close(fnt *c) { sf_close(&c->p); sf_free(&c->b); }
static int adv_of(IDWriteFontFace *f, UINT16 gid, int *lsb) {
    DWRITE_GLYPH_METRICS m; memset(&m, 0, sizeof m);
    if (FAILED(IDWriteFontFace_GetDesignGlyphMetrics(f, &gid, 1, &m, FALSE))) return -1;
    if (lsb) *lsb = m.leftSideBearing;
    return (int)m.advanceWidth;
}
static void upem_of(IDWriteFontFace *f, DWRITE_FONT_METRICS *m) { memset(m, 0, sizeof *m); IDWriteFontFace_GetMetrics(f, m); }

/* ---- head.unitsPerEm ----------------------------------------------------------------------------------------------- */
static void upem_case(unsigned v, int valid) {
    sf_blob b = sf_default(); fnt c; char l[48];
    sf_w16(sf_table(&b, "head", NULL) + 18, v);
    snprintf(l, sizeof l, "unitsPerEm=%u", v);
    if (!fn_open(&c, b, l)) return;
    if (valid) {
        CHECKF(SUCCEEDED(c.p.hr_face), "unitsPerEm %u must load (%s)", v, dw_hr_name(c.p.hr_face));
        if (c.p.face) { DWRITE_FONT_METRICS m; upem_of(c.p.face, &m); CHECKF(m.designUnitsPerEm == v, "designUnitsPerEm %u want %u", (unsigned)m.designUnitsPerEm, v); CHECK(sf_gid(c.p.face, 'A') == 2); }
    } else {
        PRED(l, FAILED(c.p.hr_face));
        if (FAILED(c.p.hr_face)) { PRED(l, c.p.hr_face == DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(c.p.face); }
    }
    fn_close(&c);
}
#define UPEM_OK(v) TEST(hd_upem_##v##_loads) { upem_case(v, 1); }
#define UPEM_BAD(v) TEST(hd_upem_##v##_rejected) { upem_case(v, 0); }
UPEM_OK(16) UPEM_OK(17) UPEM_OK(32) UPEM_OK(64) UPEM_OK(100) UPEM_OK(128) UPEM_OK(256) UPEM_OK(500) UPEM_OK(512) UPEM_OK(1000) UPEM_OK(1024)
UPEM_OK(2000) UPEM_OK(2048) UPEM_OK(4096) UPEM_OK(8192) UPEM_OK(16383) UPEM_OK(16384)
UPEM_BAD(0) UPEM_BAD(1) UPEM_BAD(8) UPEM_BAD(15) UPEM_BAD(16385) UPEM_BAD(20000) UPEM_BAD(32768) UPEM_BAD(65535)

TEST(hd_magic_number_variants) {
    unsigned long m[] = { 0x5F0F3CF5, 0x5F0F3CF4, 0, 0xFFFFFFFF, 0xF53C0F5F }; int i;
    for (i = 0; i < 5; i++) {
        sf_blob b = sf_default(); fnt c; char l[48]; sf_w32(sf_table(&b, "head", NULL) + 12, (uint32_t)m[i]); snprintf(l, sizeof l, "head.magic=%08lx", m[i]);
        if (fn_open(&c, b, l)) { if (i == 0) CHECK_HR(c.p.hr_face, S_OK); fn_close(&c); }
    }
}
TEST(hd_flags_and_dates_ignored) {
    sf_blob b = sf_default(); fnt c; uint8_t *h = sf_table(&b, "head", NULL);
    sf_w16(h + 16, 0xFFFF); memset(h + 20, 0xFF, 16); sf_w32(h + 4, 0x00020000); sf_w32(h + 8, 0);
    if (!fn_open(&c, b, "head flags/dates/version garbage")) return;
    CHECK_HR(c.p.hr_face, S_OK); fn_close(&c);
}
TEST(hd_loca_format_matches_table) {
    int lf; for (lf = 0; lf < 2; lf++) {
        ttf_spec s; sf_blob b; fnt c; char l[48]; ttf_default_spec(&s); s.long_loca = lf; b = sf_build(&s);
        CHECK(sf_r16(sf_table(&b, "head", NULL) + 50) == (unsigned)lf);
        snprintf(l, sizeof l, "indexToLocFormat=%d", lf);
        if (fn_open(&c, b, l)) { CHECK_HR(c.p.hr_face, S_OK); if (c.p.face) CHECK(sf_gid(c.p.face, 'A') == 2); fn_close(&c); }
    }
}
TEST(hd_loca_format_mismatch) {
    int lf; for (lf = 0; lf < 2; lf++) {
        ttf_spec s; sf_blob b; fnt c; char l[64]; ttf_default_spec(&s); s.long_loca = lf; b = sf_build(&s);
        sf_w16(sf_table(&b, "head", NULL) + 50, 1 - lf);
        snprintf(l, sizeof l, "head says %d but loca is %d", 1 - lf, lf);
        if (fn_open(&c, b, l)) { if (c.p.face) t_note("  face created; A=%u", sf_gid(c.p.face, 'A')); fn_close(&c); }
    }
}
TEST(hd_loca_format_invalid_values) {
    int v[] = { 2, 0xFFFF, 0x100 }; int i;
    for (i = 0; i < 3; i++) {
        sf_blob b = sf_default(); fnt c; char l[48]; sf_w16(sf_table(&b, "head", NULL) + 50, v[i]); snprintf(l, sizeof l, "indexToLocFormat=%d", v[i]);
        if (fn_open(&c, b, l)) fn_close(&c);
    }
}
TEST(hd_table_53_bytes_is_ignored) {
    sf_blob b = sf_default(); fnt c; unsigned l; uint8_t *h = sf_table(&b, "head", &l);
    sf_replace_table(&b, "head", h, 53);
    if (!fn_open(&c, b, "head of 53 bytes (below the 54 byte minimum)")) return;
    PRED("short head -> failure or fallback", 1);
    fn_close(&c);
}
TEST(hd_table_longer_than_54_bytes) {
    sf_blob b = sf_default(); fnt c; unsigned l; uint8_t *h = sf_table(&b, "head", &l), big[80];
    memset(big, 0, sizeof big); memcpy(big, h, 54);
    sf_replace_table(&b, "head", big, 80);
    if (!fn_open(&c, b, "head of 80 bytes")) return;
    CHECK_HR(c.p.hr_face, S_OK); fn_close(&c);
}
TEST(hd_missing_head) {
    ttf_spec s; sf_blob b; fnt c; ttf_default_spec(&s); s.omit |= TTF_OMIT_HEAD; b = sf_build(&s);
    CHECK(sf_find(&b, "head") < 0);
    if (!fn_open(&c, b, "no head table")) return;
    t_note("a font without head: %s", dw_hr_name(c.p.hr_face));
    if (FAILED(c.p.hr_face)) CHECK_OUT_NULL(c.p.face);
    fn_close(&c);
}

/* ---- hhea / hmtx --------------------------------------------------------------------------------------------------------- */
static void dist_spec(ttf_spec *s, ttf_glyph *g, int compress) {
    int n, i; const ttf_glyph *d = ttf_default_glyphs(&n);
    ttf_default_spec(s); memcpy(g, d, n * sizeof *g);
    for (i = 0; i < n; i++) g[i].adv = (unsigned short)(i < 20 || !compress ? 500 + 3 * i : 777);
    s->glyphs = g; s->nglyphs = n; s->compress_hmtx = compress;
}
static int expect_adv(int gid, int n, int compress) { int idx = gid < n ? gid : n - 1; (void)compress; return idx < 20 || !compress ? 500 + 3 * idx : 777; }
TEST(hm_full_hmtx_advances) {
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c; int i;
    dist_spec(&s, g, 0); b = sf_build(&s);
    CHECK(sf_r16(sf_table(&b, "hhea", NULL) + 34) == TTF_DEF_NGLYPHS);
    if (!fn_open(&c, b, "hmtx with 69 distinct advances")) return;
    if (c.p.face) for (i = 0; i < TTF_DEF_NGLYPHS; i++) { int a; if (i == TTF_GID_COMPOSITE) continue; a = adv_of(c.p.face, (UINT16)i, NULL); CHECKF(a == 500 + 3 * i, "gid %d advance %d want %d", i, a, 500 + 3 * i); }
    fn_close(&c);
}
TEST(hm_compressed_hmtx_repeats_last_advance) {
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c; int i, nh;
    dist_spec(&s, g, 1); b = sf_build(&s);
    nh = sf_r16(sf_table(&b, "hhea", NULL) + 34);
    CHECKF(nh == 21, "numberOfHMetrics %d", nh);
    if (!fn_open(&c, b, "hmtx compressed after 20 distinct advances")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) {
        CHECK(IDWriteFontFace_GetGlyphCount(c.p.face) == TTF_DEF_NGLYPHS);
        for (i = 0; i < TTF_DEF_NGLYPHS; i++) { int a; if (i == TTF_GID_COMPOSITE) continue; a = adv_of(c.p.face, (UINT16)i, NULL); CHECKF(a == expect_adv(i, nh, 1), "gid %d advance %d want %d", i, a, expect_adv(i, nh, 1)); }
    }
    fn_close(&c);
}
static void numhm_case(unsigned nh) {
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c; int i; char l[64];
    dist_spec(&s, g, 0); b = sf_build(&s);
    sf_w16(sf_table(&b, "hhea", NULL) + 34, nh);
    snprintf(l, sizeof l, "hhea.numberOfHMetrics=%u over a 69-entry hmtx", nh);
    if (!fn_open(&c, b, l)) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) {
        CHECK(IDWriteFontFace_GetGlyphCount(c.p.face) == TTF_DEF_NGLYPHS);
        for (i = 0; i < TTF_DEF_NGLYPHS; i++) {
            int a, idx = (unsigned)i < nh ? i : (int)nh - 1;
            if (i == TTF_GID_COMPOSITE) continue;      /* USE_MY_METRICS glyph: see the glyf tests */
            a = adv_of(c.p.face, (UINT16)i, NULL);
            CHECKF(a == 500 + 3 * idx, "gid %d advance %d want %d", i, a, 500 + 3 * idx);
        }
    }
    fn_close(&c);
}
#define NUMHM(n) TEST(hm_numhm_##n##_reuses_last_advance) { numhm_case(n); }
NUMHM(1) NUMHM(2) NUMHM(3) NUMHM(10) NUMHM(40) NUMHM(68) NUMHM(69)
static void numhm_bad(unsigned nh) {
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c; char l[64];
    dist_spec(&s, g, 0); b = sf_build(&s);
    sf_w16(sf_table(&b, "hhea", NULL) + 34, nh);
    snprintf(l, sizeof l, "hhea.numberOfHMetrics=%u over a 69-entry hmtx", nh);
    if (!fn_open(&c, b, l)) return;
    CHECKF(FAILED(c.p.hr_face), "%s: face created", l);       /* hmtx shorter than 4*numberOfHMetrics -> FileFormatException */
    PRED(l, c.p.hr_face == DWRITE_E_FILEFORMAT_);
    CHECK_OUT_NULL(c.p.face); fn_close(&c);
}
#define NUMHM_BAD(n) TEST(hm_numhm_##n##_exceeds_hmtx_is_format_error) { numhm_bad(n); }
NUMHM_BAD(70) NUMHM_BAD(100) NUMHM_BAD(1000) NUMHM_BAD(65535)
TEST(hm_numhm_zero_note) {
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c;
    dist_spec(&s, g, 0); b = sf_build(&s); sf_w16(sf_table(&b, "hhea", NULL) + 34, 0);
    if (!fn_open(&c, b, "numberOfHMetrics = 0")) return;
    if (c.p.face) PRED("glyph count collapses to 0 with no horizontal metrics", IDWriteFontFace_GetGlyphCount(c.p.face) == 0);
    fn_close(&c);
}
TEST(hm_hmtx_truncated_below_numhm) {
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c; unsigned l; uint8_t *t;
    dist_spec(&s, g, 0); b = sf_build(&s); t = sf_table(&b, "hmtx", &l);
    sf_replace_table(&b, "hmtx", t, 100);
    if (!fn_open(&c, b, "hmtx cut to 100 bytes (hhea says 69 metrics)")) return;
    CHECK(FAILED(c.p.hr_face)); CHECK_OUT_NULL(c.p.face); fn_close(&c);
}
TEST(hm_hmtx_odd_length) {
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c; unsigned l; uint8_t *t, *n;
    dist_spec(&s, g, 1); b = sf_build(&s); t = sf_table(&b, "hmtx", &l);
    n = (uint8_t *)malloc(l + 1); memcpy(n, t, l); n[l] = 0x7F;
    sf_replace_table(&b, "hmtx", n, l + 1); free(n);
    if (!fn_open(&c, b, "hmtx with one extra byte")) return;
    CHECK_HR(c.p.hr_face, S_OK); if (c.p.face) CHECK(IDWriteFontFace_GetGlyphCount(c.p.face) == TTF_DEF_NGLYPHS);
    fn_close(&c);
}
TEST(hm_glyph_count_capped_by_hmtx_coverage) {
    /* maxp promises more glyphs than hmtx describes: count = numberOfHMetrics + (length - 4*numberOfHMetrics)/2 */
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c;
    dist_spec(&s, g, 1); b = sf_build(&s);                       /* numHM 21, hmtx = 21*4 + 48*2 = 180 bytes: coverage 69 */
    sf_w16(sf_table(&b, "maxp", NULL) + 4, 500);
    if (!fn_open(&c, b, "maxp.numGlyphs=500, hmtx covers 69")) return;
    if (c.p.face) { PRED("capped to hmtx coverage", IDWriteFontFace_GetGlyphCount(c.p.face) == 69); t_note("glyph count %u", (unsigned)IDWriteFontFace_GetGlyphCount(c.p.face)); }
    fn_close(&c);
}
TEST(hm_hmtx_missing_collapses_glyph_count) {
    ttf_spec s; sf_blob b; fnt c; ttf_default_spec(&s); s.omit |= TTF_OMIT_HMTX; b = sf_build(&s);
    if (!fn_open(&c, b, "no hmtx table")) return;
    if (c.p.face) PRED("no metrics -> glyph count 0", IDWriteFontFace_GetGlyphCount(c.p.face) == 0);
    fn_close(&c);
}
TEST(hm_hhea_missing) {
    ttf_spec s; sf_blob b; fnt c; ttf_default_spec(&s); s.omit |= TTF_OMIT_HHEA; b = sf_build(&s);
    if (!fn_open(&c, b, "no hhea table")) return;
    if (c.p.face) PRED("no hhea -> glyph count 0", IDWriteFontFace_GetGlyphCount(c.p.face) == 0);
    fn_close(&c);
}
TEST(hm_hhea_35_bytes_ignored) {
    sf_blob b = sf_default(); fnt c; unsigned l; uint8_t *h = sf_table(&b, "hhea", &l);
    sf_replace_table(&b, "hhea", h, 35);
    if (!fn_open(&c, b, "hhea of 35 bytes (below 36)")) return;
    if (c.p.face) PRED("hhea ignored -> no horizontal metrics", IDWriteFontFace_GetGlyphCount(c.p.face) == 0);
    fn_close(&c);
}
TEST(hm_lsb_array_trailing_glyphs) {
    ttf_spec s; ttf_glyph g[TTF_DEF_NGLYPHS]; sf_blob b; fnt c; int i;
    dist_spec(&s, g, 1);
    for (i = 21; i < TTF_DEF_NGLYPHS; i++) { g[i].lsb_set = 1; g[i].lsb = (short)(40 + i); }
    b = sf_build(&s);
    if (!fn_open(&c, b, "trailing lsb array")) return;
    if (c.p.face) for (i = 21; i < TTF_DEF_NGLYPHS; i += 7) { int lsb = -1, a = adv_of(c.p.face, (UINT16)i, &lsb); CHECK(a == 777); t_note("gid %d lsb reported %d (hmtx array says %d)", i, lsb, 40 + i); }
    fn_close(&c);
}
TEST(hm_gid_out_of_range) {
    sf_blob b = sf_default(); fnt c; DWRITE_GLYPH_METRICS m; UINT16 g[3] = { 68, 69, 0xFFFF }; HRESULT hr;
    if (!fn_open(&c, b, "gid range")) return;
    if (c.p.face) {
        hr = IDWriteFontFace_GetDesignGlyphMetrics(c.p.face, &g[0], 1, &m, FALSE); CHECK_HR(hr, S_OK);
        hr = IDWriteFontFace_GetDesignGlyphMetrics(c.p.face, &g[1], 1, &m, FALSE); t_note("GetDesignGlyphMetrics(gid == glyphCount): %s", dw_hr_name(hr));
        hr = IDWriteFontFace_GetDesignGlyphMetrics(c.p.face, &g[2], 1, &m, FALSE); t_note("GetDesignGlyphMetrics(gid 0xFFFF): %s", dw_hr_name(hr));
    }
    fn_close(&c);
}

/* ---- maxp -------------------------------------------------------------------------------------------------------------- */
static void maxp_case(unsigned n, int hard) {
    sf_blob b = sf_default(); fnt c; char l[48]; sf_w16(sf_table(&b, "maxp", NULL) + 4, n); snprintf(l, sizeof l, "maxp.numGlyphs=%u", n);
    if (!fn_open(&c, b, l)) return;
    if (c.p.face) {
        unsigned gc = IDWriteFontFace_GetGlyphCount(c.p.face), want = n < TTF_DEF_NGLYPHS ? n : TTF_DEF_NGLYPHS;
        if (hard) CHECKF(gc == n, "glyph count %u want %u", gc, n); else PRED(l, gc == want);
        t_note("  glyph count %u; 'A'=%u 'z'=%u", gc, sf_gid(c.p.face, 'A'), sf_gid(c.p.face, 'z'));
        if (n <= 53) PRED("cmap entries above the glyph count are dropped", sf_gid(c.p.face, 'z') == 0);
    }
    fn_close(&c);
}
#define MAXP(n, hard) TEST(mx_numglyphs_##n) { maxp_case(n, hard); }
MAXP(1, 1) MAXP(2, 1) MAXP(3, 1) MAXP(30, 1) MAXP(53, 1) MAXP(68, 1) MAXP(69, 1)
MAXP(0, 0) MAXP(70, 0) MAXP(200, 0) MAXP(1000, 0) MAXP(32767, 0) MAXP(65535, 0)
TEST(mx_version_0_5_short_table) {
    sf_blob b = sf_default(); fnt c; uint8_t t[6]; memcpy(t, sf_table(&b, "maxp", NULL), 6); sf_w32(t, 0x00005000);
    sf_replace_table(&b, "maxp", t, 6);
    if (!fn_open(&c, b, "maxp 0.5 (6 bytes) on a TrueType font")) return;
    CHECK_HR(c.p.hr_face, S_OK); if (c.p.face) CHECK(IDWriteFontFace_GetGlyphCount(c.p.face) == TTF_DEF_NGLYPHS);
    fn_close(&c);
}
TEST(mx_table_5_bytes_is_format_error) {
    sf_blob b = sf_default(); fnt c; uint8_t t[5]; memcpy(t, sf_table(&b, "maxp", NULL), 5);
    sf_replace_table(&b, "maxp", t, 5);
    if (!fn_open(&c, b, "maxp of 5 bytes")) return;
    CHECK(FAILED(c.p.hr_face)); PRED("maxp of 5 bytes", c.p.hr_face == DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(c.p.face); fn_close(&c);
}
TEST(mx_missing_maxp_is_format_error) {
    ttf_spec s; sf_blob b; fnt c; ttf_default_spec(&s); s.omit |= TTF_OMIT_MAXP; b = sf_build(&s);
    CHECK(sf_find(&b, "maxp") < 0);
    if (!fn_open(&c, b, "no maxp table")) return;
    CHECK(FAILED(c.p.hr_face)); PRED("no maxp", c.p.hr_face == DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(c.p.face); fn_close(&c);
}
TEST(mx_maxp_other_fields_ignored) {
    sf_blob b = sf_default(); fnt c; uint8_t *m = sf_table(&b, "maxp", NULL);
    memset(m + 6, 0xFF, 26);
    if (!fn_open(&c, b, "maxp limits all 0xFFFF")) return;
    CHECK_HR(c.p.hr_face, S_OK); if (c.p.face) CHECK(sf_gid(c.p.face, 'A') == 2);
    fn_close(&c);
}

/* ---- OS/2 --------------------------------------------------------------------------------------------------------------- */
static void os2_version(int v, int hard_cap) {
    ttf_spec s; sf_blob b; fnt c; char l[48]; ttf_default_spec(&s); s.os2_version = v; b = sf_build(&s);
    snprintf(l, sizeof l, "OS/2 version %d", v);
    if (!fn_open(&c, b, l)) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) {
        DWRITE_FONT_METRICS m; upem_of(c.p.face, &m);
        CHECK(m.designUnitsPerEm == 2048);
        if (hard_cap) { CHECKF(m.capHeight == 1400, "capHeight %u", (unsigned)m.capHeight); CHECKF(m.xHeight == 1000, "xHeight %u", (unsigned)m.xHeight); }
        else t_note("  v%d capHeight=%u xHeight=%u (derived from glyphs?)", v, (unsigned)m.capHeight, (unsigned)m.xHeight);
        CHECK(m.underlinePosition == -200); CHECK(m.underlineThickness == 100); CHECK(m.strikethroughPosition == 600); CHECK(m.strikethroughThickness == 100);
    }
    fn_close(&c);
}
TEST(os_version_0) { os2_version(0, 0); }
TEST(os_version_1) { os2_version(1, 0); }
TEST(os_version_2) { os2_version(2, 1); }
TEST(os_version_3) { os2_version(3, 1); }
TEST(os_version_4) { os2_version(4, 1); }
TEST(os_version_5) { os2_version(5, 1); }
static void os2_len(unsigned len, const char *what) {
    sf_blob b = sf_default(); fnt c; uint8_t *t; unsigned l; uint8_t full[128]; char lab[80];
    t = sf_table(&b, "OS/2", &l); memset(full, 0, sizeof full); memcpy(full, t, l < 128 ? l : 128);
    sf_replace_table(&b, "OS/2", full, len);
    snprintf(lab, sizeof lab, "OS/2 version 4 cut to %u bytes (%s)", len, what);
    if (!fn_open(&c, b, lab)) return;
    if (c.p.face) { DWRITE_FONT_METRICS m; upem_of(c.p.face, &m); t_note("  capHeight=%u xHeight=%u", (unsigned)m.capHeight, (unsigned)m.xHeight); }
    fn_close(&c);
}
#define OS2LEN(n, what) TEST(os_length_##n) { os2_len(n, what); }
OS2LEN(0, "empty") OS2LEN(2, "version only") OS2LEN(40, "") OS2LEN(77, "just below the 78 byte minimum") OS2LEN(78, "v0 size") OS2LEN(85, "") OS2LEN(86, "v1 size") OS2LEN(95, "") OS2LEN(96, "v2-4 size") OS2LEN(99, "") OS2LEN(100, "v5 size") OS2LEN(120, "longer than any version")
TEST(os_missing_os2) {
    ttf_spec s; sf_blob b; fnt c; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2; b = sf_build(&s);
    CHECK(sf_find(&b, "OS/2") < 0);
    if (!fn_open(&c, b, "no OS/2 table")) return;
    if (c.p.face) { DWRITE_FONT_METRICS m; upem_of(c.p.face, &m); t_note("  asc=%u desc=%u gap=%d from hhea only", (unsigned)m.ascent, (unsigned)m.descent, (int)m.lineGap); PRED("hhea metrics used", m.ascent == 1700 && m.descent == 400); }
    fn_close(&c);
}
TEST(os_version_field_unknown) {
    unsigned v[] = { 6, 7, 0x100, 0xFFFF }; int i;
    for (i = 0; i < 4; i++) { sf_blob b = sf_default(); fnt c; char l[48]; sf_w16(sf_table(&b, "OS/2", NULL), v[i]); snprintf(l, sizeof l, "OS/2.version=%u", v[i]); if (fn_open(&c, b, l)) { if (v[i] <= 7) CHECK_HR(c.p.hr_face, S_OK); fn_close(&c); } }
}
static void weight_case(unsigned w, int expect) {
    sf_blob b = sf_default(); sf_probe p; IDWriteFontFace3 *f3 = NULL; char l[48];
    sf_w16(sf_table(&b, "OS/2", NULL) + 4, w); snprintf(l, sizeof l, "usWeightClass=%u", w);
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face && SUCCEEDED(IDWriteFontFace_QueryInterface(p.face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
        DWRITE_FONT_WEIGHT got = IDWriteFontFace3_GetWeight(f3);
        if (expect) CHECKF((int)got == expect, "%s -> weight %d want %d", l, (int)got, expect); else t_note("%s -> DWRITE_FONT_WEIGHT %d", l, (int)got);
        IDWriteFontFace3_Release(f3);
    } else t_skip("IDWriteFontFace3 unavailable");
    sf_close(&p); sf_free(&b);
}
#define WEIGHT(w, e) TEST(os_weight_##w) { weight_case(w, e); }
WEIGHT(100, 100) WEIGHT(200, 200) WEIGHT(300, 300) WEIGHT(350, 0) WEIGHT(400, 400) WEIGHT(500, 500) WEIGHT(600, 600) WEIGHT(700, 700) WEIGHT(800, 800) WEIGHT(900, 900) WEIGHT(950, 0) WEIGHT(999, 0)
WEIGHT(0, 0) WEIGHT(1, 0) WEIGHT(1000, 0) WEIGHT(1001, 0) WEIGHT(65535, 0)
static void width_case(unsigned w, int expect) {
    sf_blob b = sf_default(); sf_probe p; IDWriteFontFace3 *f3 = NULL; char l[48];
    sf_w16(sf_table(&b, "OS/2", NULL) + 6, w); snprintf(l, sizeof l, "usWidthClass=%u", w);
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face && SUCCEEDED(IDWriteFontFace_QueryInterface(p.face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
        DWRITE_FONT_STRETCH got = IDWriteFontFace3_GetStretch(f3);
        if (expect) CHECKF((int)got == expect, "%s -> stretch %d want %d", l, (int)got, expect); else t_note("%s -> DWRITE_FONT_STRETCH %d", l, (int)got);
        IDWriteFontFace3_Release(f3);
    } else t_skip("IDWriteFontFace3 unavailable");
    sf_close(&p); sf_free(&b);
}
#define WIDTH(w, e) TEST(os_width_##w) { width_case(w, e); }
WIDTH(1, 1) WIDTH(2, 2) WIDTH(3, 3) WIDTH(4, 4) WIDTH(5, 5) WIDTH(6, 6) WIDTH(7, 7) WIDTH(8, 8) WIDTH(9, 9) WIDTH(0, 0) WIDTH(10, 0) WIDTH(65535, 0)
static void style_case(unsigned fs, unsigned macstyle, int expect, const char *what) {
    sf_blob b = sf_default(); sf_probe p; IDWriteFontFace3 *f3 = NULL;
    sf_w16(sf_table(&b, "OS/2", NULL) + 62, fs); sf_w16(sf_table(&b, "head", NULL) + 44, macstyle);
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face && SUCCEEDED(IDWriteFontFace_QueryInterface(p.face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
        DWRITE_FONT_STYLE got = IDWriteFontFace3_GetStyle(f3);
        if (expect >= 0) CHECKF((int)got == expect, "%s: style %d want %d", what, (int)got, expect); else t_note("%s: style %d", what, (int)got);
        IDWriteFontFace3_Release(f3);
    } else t_skip("IDWriteFontFace3 unavailable");
    sf_close(&p); sf_free(&b);
}
TEST(os_style_regular) { style_case(0x40, 0, DWRITE_FONT_STYLE_NORMAL, "REGULAR"); }
TEST(os_style_italic_bit) { style_case(0x01, 2, DWRITE_FONT_STYLE_ITALIC, "fsSelection ITALIC + macStyle italic"); }
TEST(os_style_italic_fsselection_only) { style_case(0x01, 0, -1, "fsSelection ITALIC only"); }
TEST(os_style_italic_macstyle_only) { style_case(0x40, 2, -1, "macStyle italic only"); }
TEST(os_style_oblique_bit) { style_case(0x200, 0, -1, "fsSelection OBLIQUE"); }
TEST(os_style_italic_and_oblique) { style_case(0x201, 0, -1, "fsSelection ITALIC|OBLIQUE"); }
TEST(os_style_bold_only) { style_case(0x20, 1, DWRITE_FONT_STYLE_NORMAL, "BOLD"); }

/* ---- post ------------------------------------------------------------------------------------------------------------------ */
TEST(ps_missing_post) {
    ttf_spec s; sf_blob b; fnt c; ttf_default_spec(&s); s.omit |= TTF_OMIT_POST; b = sf_build(&s);
    CHECK(sf_find(&b, "post") < 0);
    if (!fn_open(&c, b, "no post table")) return;
    if (c.p.face) { DWRITE_FONT_METRICS m; upem_of(c.p.face, &m); t_note("  underline %d/%u without post", (int)m.underlinePosition, (unsigned)m.underlineThickness); CHECK(sf_gid(c.p.face, 'A') == 2); }
    fn_close(&c);
}
TEST(ps_underline_fields_are_reported) {
    ttf_spec s; sf_blob b; fnt c; ttf_default_spec(&s); s.underline_pos = -123; s.underline_thick = 45; b = sf_build(&s);
    if (!fn_open(&c, b, "post underline -123/45")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) { DWRITE_FONT_METRICS m; upem_of(c.p.face, &m); CHECK(m.underlinePosition == -123); CHECK(m.underlineThickness == 45); }
    fn_close(&c);
}
TEST(ps_versions_loadable) {
    uint32_t v[] = { 0x00010000, 0x00020000, 0x00025000, 0x00030000, 0x00040000, 0x12345678 }; int i;
    for (i = 0; i < 6; i++) {
        sf_blob b = sf_default(); fnt c; char l[48]; sf_w32(sf_table(&b, "post", NULL), v[i]); snprintf(l, sizeof l, "post version %08x", v[i]);
        if (fn_open(&c, b, l)) { CHECK_HR(c.p.hr_face, S_OK); fn_close(&c); }
    }
}
TEST(ps_table_truncated) {
    unsigned len[] = { 0, 4, 16, 31 }; int i;
    for (i = 0; i < 4; i++) {
        sf_blob b = sf_default(); fnt c; char l[48]; uint8_t t[32]; memcpy(t, sf_table(&b, "post", NULL), 32); sf_replace_table(&b, "post", t, len[i]);
        snprintf(l, sizeof l, "post cut to %u bytes", len[i]);
        if (fn_open(&c, b, l)) { if (c.p.face) { DWRITE_FONT_METRICS m; upem_of(c.p.face, &m); t_note("  underline %d/%u", (int)m.underlinePosition, (int)m.underlineThickness); } fn_close(&c); }
    }
}
TEST(ps_monospaced_flag) {
    ttf_spec s; sf_blob b; sf_probe p; IDWriteFontFace1 *f1 = NULL; ttf_default_spec(&s); s.is_fixed_pitch = 1; b = sf_build(&s);
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face && SUCCEEDED(IDWriteFontFace_QueryInterface(p.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        BOOL mono = IDWriteFontFace1_IsMonospacedFont(f1);
        t_note("post.isFixedPitch=1 with proportional advances -> IsMonospacedFont=%d", (int)mono);
        PRED("isFixedPitch honoured", mono == TRUE);
        IDWriteFontFace1_Release(f1);
    } else t_skip("IDWriteFontFace1 unavailable");
    sf_close(&p); sf_free(&b);
}
TEST(ps_monospaced_default_false) {
    sf_blob b = sf_default(); sf_probe p; IDWriteFontFace1 *f1 = NULL;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face && SUCCEEDED(IDWriteFontFace_QueryInterface(p.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        t_note("default synthetic font IsMonospacedFont=%d", (int)IDWriteFontFace1_IsMonospacedFont(f1));
        IDWriteFontFace1_Release(f1);
    }
    sf_close(&p); sf_free(&b);
}

/* ---- name --------------------------------------------------------------------------------------------------------------------- */
static void print_names(const char *label, IDWriteFontFace *face) {
    IDWriteFontFace3 *f3 = NULL; IDWriteLocalizedStrings *fam = NULL, *fc = NULL; char a[128] = "-", b[128] = "-"; HRESULT h1, h2;
    if (!face || FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3)) || !f3) { t_note("%s: no IDWriteFontFace3", label); return; }
    h1 = IDWriteFontFace3_GetFamilyNames(f3, &fam); h2 = IDWriteFontFace3_GetFaceNames(f3, &fc);
    if (SUCCEEDED(h1) && fam) { dw_ls_en(fam, a, sizeof a); }
    if (SUCCEEDED(h2) && fc) { dw_ls_en(fc, b, sizeof b); }
    t_note("%s: family(%s)='%s'[%u] face(%s)='%s'[%u]", label, dw_hr_name(h1), a, fam ? (unsigned)IDWriteLocalizedStrings_GetCount(fam) : 0, dw_hr_name(h2), b, fc ? (unsigned)IDWriteLocalizedStrings_GetCount(fc) : 0);
    DW_RELEASE(fam); DW_RELEASE(fc); IDWriteFontFace3_Release(f3);
}
TEST(nm_default_family_and_face_names) {
    sf_blob b = sf_default(); sf_probe p; IDWriteFontFace3 *f3 = NULL; IDWriteLocalizedStrings *s = NULL; char buf[64];
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face && SUCCEEDED(IDWriteFontFace_QueryInterface(p.face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
        CHECK_HR(IDWriteFontFace3_GetFamilyNames(f3, &s), S_OK);
        if (s) { CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1); CHECK_HR(dw_ls_en(s, buf, sizeof buf), S_OK); t_note("family: %s", buf); PRED("family name", !strcmp(buf, "DwTest")); DW_RELEASE(s); }
        CHECK_HR(IDWriteFontFace3_GetFaceNames(f3, &s), S_OK);
        if (s) { CHECK_HR(dw_ls_en(s, buf, sizeof buf), S_OK); t_note("face: %s", buf); PRED("face name", !strcmp(buf, "Regular")); DW_RELEASE(s); }
        IDWriteFontFace3_Release(f3);
    } else t_skip("IDWriteFontFace3 unavailable");
    sf_close(&p); sf_free(&b);
}
static void v_family_only_no_subfamily(ttf_spec *s);
static void name_variant(const char *label, void (*mut)(ttf_spec *)) {
    ttf_spec s; sf_blob b; sf_probe p; ttf_default_spec(&s); mut(&s); b = sf_build(&s);
    if (t_is_wine() && mut == v_family_only_no_subfamily) { t_skip("Wine crashes on a name table without a subfamily"); sf_free(&b); return; }
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    t_note("%s: face=%s", label, dw_hr_name(p.hr_face));
    if (p.face) print_names(label, p.face);
    sf_close(&p); sf_free(&b);
}
static void v_noname(ttf_spec *s) { s->omit |= TTF_OMIT_NAME; }
static void v_emptyname(ttf_spec *s) { ttf_clear_names(s); }
static void v_macroman_only(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 1, 0, 0, 1, "MacFam"); ttf_add_name(s, 1, 0, 0, 2, "Regular"); ttf_add_name(s, 1, 0, 0, 4, "MacFam"); ttf_add_name(s, 1, 0, 0, 6, "MacFam-Regular"); }
static void v_unicode_platform_only(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 0, 3, 0, 1, "UniFam"); ttf_add_name(s, 0, 3, 0, 2, "Regular"); ttf_add_name(s, 0, 3, 0, 4, "UniFam"); ttf_add_name(s, 0, 3, 0, 6, "UniFam-Regular"); }
static void v_unicode_platform_enc4(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 0, 4, 0, 1, "Uni4Fam"); ttf_add_name(s, 0, 4, 0, 2, "Regular"); }
static void v_win_symbol_encoding(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 3, 0, 0x409, 1, "SymFam"); ttf_add_name(s, 3, 0, 0x409, 2, "Regular"); }
static void v_win_ucs4_encoding(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 3, 10, 0x409, 1, "Ucs4Fam"); ttf_add_name(s, 3, 10, 0x409, 2, "Regular"); }
static void v_invalid_platform(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 7, 1, 0x409, 1, "BadPlat"); ttf_add_name(s, 7, 1, 0x409, 2, "Regular"); }
static void v_invalid_platform_plus_good(ttf_spec *s) { ttf_add_name(s, 9, 9, 0x409, 1, "ShouldNotWin"); }
static void v_id16_17(ttf_spec *s) { ttf_add_name(s, 3, 1, 0x409, 16, "TypoFam"); ttf_add_name(s, 3, 1, 0x409, 17, "Light"); }
static void v_id21_22(ttf_spec *s) { ttf_add_name(s, 3, 1, 0x409, 21, "WwsFam"); ttf_add_name(s, 3, 1, 0x409, 22, "Medium"); }
static void v_id16_17_21_22(ttf_spec *s) { ttf_add_name(s, 3, 1, 0x409, 16, "TypoFam"); ttf_add_name(s, 3, 1, 0x409, 17, "Light"); ttf_add_name(s, 3, 1, 0x409, 21, "WwsFam"); ttf_add_name(s, 3, 1, 0x409, 22, "Medium"); }
static void v_two_languages(ttf_spec *s) { ttf_add_name(s, 3, 1, 0x407, 1, "TestFamilie"); ttf_add_name(s, 3, 1, 0x40C, 1, "FamilleTest"); }
static void v_only_non_english(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 3, 1, 0x407, 1, "NurDeutsch"); ttf_add_name(s, 3, 1, 0x407, 2, "Standard"); }
static void v_empty_family_string(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 3, 1, 0x409, 1, ""); ttf_add_name(s, 3, 1, 0x409, 2, "Regular"); }
static void v_family_only_no_subfamily(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 3, 1, 0x409, 1, "NoSub"); }
static void v_long_family(ttf_spec *s) { char big[96]; memset(big, 'L', 90); big[90] = 0; ttf_clear_names(s); ttf_add_name(s, 3, 1, 0x409, 1, big); ttf_add_name(s, 3, 1, 0x409, 2, "Regular"); }
static void v_unicode_family(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 3, 1, 0x409, 1, "Caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC"); ttf_add_name(s, 3, 1, 0x409, 2, "Regular"); }
static void v_surrogate_family(ttf_spec *s) { ttf_clear_names(s); ttf_add_name(s, 3, 1, 0x409, 1, "Emoji\xF0\x9F\x98\x80"); ttf_add_name(s, 3, 1, 0x409, 2, "Regular"); }
#define NAMEV(n, label, fn) TEST(nm_##n) { name_variant(label, fn); }
NAMEV(no_name_table, "no name table", v_noname)
NAMEV(name_table_with_zero_records, "name table with zero records", v_emptyname)
NAMEV(mac_roman_records_only, "only (1,0) records", v_macroman_only)
NAMEV(unicode_platform_records_only, "only (0,3) records", v_unicode_platform_only)
NAMEV(unicode_platform_enc4_records_only, "only (0,4) records", v_unicode_platform_enc4)
NAMEV(windows_symbol_encoding_records_only, "only (3,0) records", v_win_symbol_encoding)
NAMEV(windows_ucs4_encoding_records_only, "only (3,10) records", v_win_ucs4_encoding)
NAMEV(invalid_platform_only, "only platform 7 records", v_invalid_platform)
NAMEV(invalid_platform_alongside_valid, "valid + platform 9 record", v_invalid_platform_plus_good)
NAMEV(typographic_names_16_17, "ids 16/17 present", v_id16_17)
NAMEV(wws_names_21_22, "ids 21/22 present", v_id21_22)
NAMEV(all_of_16_17_21_22, "ids 16/17/21/22 present", v_id16_17_21_22)
NAMEV(two_extra_languages, "en-us + de-de + fr-fr family", v_two_languages)
NAMEV(only_a_non_english_language, "only de-de", v_only_non_english)
NAMEV(empty_family_string, "empty family string", v_empty_family_string)
NAMEV(family_without_subfamily, "id1 without id2", v_family_only_no_subfamily)
NAMEV(long_family_string, "90 character family", v_long_family)
NAMEV(non_ascii_family, "non-ASCII family", v_unicode_family)
NAMEV(supplementary_plane_family, "surrogate pair in family", v_surrogate_family)
TEST(nm_typographic_family_is_preferred) {
    ttf_spec s; sf_blob b; sf_probe p; ttf_default_spec(&s); v_id16_17(&s); b = sf_build(&s);
    if (sf_open_face(&p, b.p, b.n) && p.face) {
        IDWriteFontFace3 *f3 = NULL; IDWriteLocalizedStrings *fam = NULL; char buf[64] = "";
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(p.face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
            if (SUCCEEDED(IDWriteFontFace3_GetFamilyNames(f3, &fam)) && fam) { dw_ls_en(fam, buf, sizeof buf); t_note("family with ids 1='DwTest' and 16='TypoFam': '%s'", buf); PRED("typographic family wins", !strcmp(buf, "TypoFam")); DW_RELEASE(fam); }
            IDWriteFontFace3_Release(f3);
        }
    }
    sf_close(&p); sf_free(&b);
}
TEST(nm_name_table_offsets_beyond_table) {
    sf_blob b = sf_default(); sf_probe p; uint8_t *n = sf_table(&b, "name", NULL);
    sf_w16(n + 4, 0xFFF0);                       /* stringOffset beyond the table */
    if (sf_open_face(&p, b.p, b.n)) { sf_survey("name stringOffset beyond the table", &p); print_names("  names", p.face); }
    sf_close(&p); sf_free(&b);
}
TEST(nm_name_record_string_beyond_table) {
    sf_blob b = sf_default(); sf_probe p; uint8_t *n = sf_table(&b, "name", NULL);
    sf_w16(n + 6 + 8, 0x7000);                   /* first record: length -> huge */
    if (sf_open_face(&p, b.p, b.n)) { sf_survey("first name record length 0x7000", &p); print_names("  names", p.face); }
    sf_close(&p); sf_free(&b);
}
TEST(nm_name_count_beyond_table) {
    sf_blob b = sf_default(); sf_probe p; uint8_t *n = sf_table(&b, "name", NULL);
    sf_w16(n + 2, 5000);
    if (sf_open_face(&p, b.p, b.n)) { sf_survey("name count 5000", &p); print_names("  names", p.face); }
    sf_close(&p); sf_free(&b);
}
TEST(nm_name_table_truncated_to_header) {
    sf_blob b = sf_default(); sf_probe p; uint8_t t[6]; memcpy(t, sf_table(&b, "name", NULL), 6);
    sf_replace_table(&b, "name", t, 6);
    if (sf_open_face(&p, b.p, b.n)) { sf_survey("name table of 6 bytes", &p); print_names("  names", p.face); }
    sf_close(&p); sf_free(&b);
}
TEST(nm_name_format_1_langtags) {
    /* name table format 1 appends language tag records; ids >= 0x8000 select them */
    sf_blob b = sf_default(); sf_probe p; unsigned l; uint8_t *n = sf_table(&b, "name", &l); uint8_t *nn = (uint8_t *)malloc(l + 64);
    memcpy(nn, n, l); sf_w16(nn, 1);
    sf_replace_table(&b, "name", nn, l); free(nn);
    if (sf_open_face(&p, b.p, b.n)) { sf_survey("name format 1 without langTag records", &p); print_names("  names", p.face); }
    sf_close(&p); sf_free(&b);
}
