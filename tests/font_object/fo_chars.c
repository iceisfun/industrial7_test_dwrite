/* IDWriteFont::HasCharacter and IDWriteFont1::GetUnicodeRanges.
 * Decomp: DWriteFont::HasCharacter(cp, BOOL*) always returns S_OK and stores CompactCmap::operator[](cp);
 * GetUnicodeRanges zeroes *count, zero-fills min(maxRangeCount,..) entries, walks the cmap with FindNextCodepoint until a value > 0x10FFFF,
 * counts every range, stores only the first maxRangeCount of them, returns E_NOT_SUFFICIENT_BUFFER (0x8007007A) when count > max and
 * E_INVALIDARG (0x80070057) for (max != 0 && ranges == NULL). */
#include "fo.h"

static int has(IDWriteFont *f, UINT32 cp) {
    BOOL e = 99; HRESULT hr = IDWriteFont_HasCharacter(f, cp, &e);
    if (hr != S_OK) return -100 - (int)(hr & 0xff);
    return (e == 0 || e == 1) ? (int)e : 50 + (int)e;
}
static int ranges_of(IDWriteFont *f, DWRITE_UNICODE_RANGE *r, UINT32 cap, UINT32 *cnt, HRESULT *hr_out) {
    IDWriteFont1 *f1 = NULL; HRESULT hr; UINT32 c = 0xAAAAAAAA;
    if (FAILED(IDWriteFont_QueryInterface(f, &IID_IDWriteFont1, (void **)&f1)) || !f1) return 0;
    hr = IDWriteFont1_GetUnicodeRanges(f1, cap, r, &c); IDWriteFont1_Release(f1);
    if (cnt) *cnt = c; if (hr_out) *hr_out = hr; return 1;
}

/* ---- HasCharacter on the default synthetic font -------------------------------------------------------------- */
TEST(font_has_default_ascii_table) {
    fo_fx x; ttf_spec s; UINT32 cp; fo_spec(&s, "FoHas1"); FO_OPEN1(x, s);
    for (cp = 0; cp < 0x80; cp++) { int want = ttf_default_gid(cp) != 0; int got = has(x.font[0], cp); CHECKF(got == want, "U+%04X: got %d want %d", cp, got, want); }
    fo_close(&x);
}
TEST(font_has_default_exact_set) {
    fo_fx x; ttf_spec s; UINT32 cp; int n = 0; fo_spec(&s, "FoHas2"); FO_OPEN1(x, s);
    for (cp = 0; cp < 0x3000; cp++) if (has(x.font[0], cp) == 1) n++;
    EXPECT_EQ(n, 1 + 26 + 26 + 10);          /* space, A-Z, a-z, 0-9 */
    fo_close(&x);
}
TEST(font_has_default_non_mapped_false) {
    static const UINT32 cps[] = { 0x00, 0x09, 0x0A, 0x0D, 0x1F, 0x21, 0x2F, 0x3A, 0x40, 0x5B, 0x60, 0x7B, 0x7E, 0x7F, 0xA0, 0xA9, 0xC0, 0xE9, 0xFF, 0x100, 0x3A9, 0x416, 0x5D0, 0x627, 0x0E01,
        0x2000, 0x200B, 0x2028, 0x20AC, 0x2603, 0x3042, 0x4E00, 0xAC00, 0xE000, 0xF8FF, 0xFB01, 0xFEFF, 0xFFFD, 0xFFFE, 0xFFFF, 0x10000, 0x1F600, 0x20000, 0xE0001, 0xF0000, 0x10FFFF };
    fo_fx x; ttf_spec s; size_t i; fo_spec(&s, "FoHas3"); FO_OPEN1(x, s);
    for (i = 0; i < sizeof cps / sizeof cps[0]; i++) { int got = has(x.font[0], cps[i]); CHECKF(got == 0, "U+%04X: got %d", cps[i], got); }
    fo_close(&x);
}
TEST(font_has_beyond_unicode_range) {
    /* values above U+10FFFF are not characters: S_OK with exists FALSE (the cmap lookup returns false) */
    static const UINT32 cps[] = { 0x110000, 0x110001, 0x1FFFFF, 0x200000, 0xFFFFFF, 0x1000000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu, 0x10FFFF + 0x10000 };
    fo_fx x; ttf_spec s; size_t i; fo_spec(&s, "FoHas4"); FO_OPEN1(x, s);
    for (i = 0; i < sizeof cps / sizeof cps[0]; i++) { BOOL e = 99; HRESULT hr = IDWriteFont_HasCharacter(x.font[0], cps[i], &e); CHECKF(hr == S_OK, "U+%X -> %s", cps[i], dw_hr_name(hr)); CHECKF(e == FALSE, "U+%X exists=%d", cps[i], e); }
    fo_close(&x);
}
TEST(font_has_surrogate_code_points_false) {
    UINT32 cps[] = { 0xD800, 0xD801, 0xDB7F, 0xDBFF, 0xDC00, 0xDC01, 0xDFFE, 0xDFFF }; size_t i;
    fo_fx x; ttf_spec s; fo_spec(&s, "FoHas5"); FO_OPEN1(x, s);
    for (i = 0; i < 8; i++) { int got = has(x.font[0], cps[i]); CHECKF(got == 0, "U+%04X: %d", cps[i], got); }
    fo_close(&x);
}
TEST(font_has_writes_exists_always) {
    fo_fx x; ttf_spec s; BOOL e; fo_spec(&s, "FoHas6"); FO_OPEN1(x, s);
    e = 77; CHECK_HR_OK(IDWriteFont_HasCharacter(x.font[0], 'A', &e)); EXPECT_EQ(e, 1);
    e = 77; CHECK_HR_OK(IDWriteFont_HasCharacter(x.font[0], 0x4E00, &e)); EXPECT_EQ(e, 0);
    e = 77; CHECK_HR_OK(IDWriteFont_HasCharacter(x.font[0], 0x110000, &e)); EXPECT_EQ(e, 0);
    fo_close(&x);
}
TEST(font_has_repeatable_and_order_independent) {
    fo_fx x; ttf_spec s; int i; fo_spec(&s, "FoHas7"); FO_OPEN1(x, s);
    for (i = 0; i < 200; i++) { UINT32 cp = (i * 37) % 0x90; CHECK(has(x.font[0], cp) == (ttf_default_gid(cp) != 0)); }
    fo_close(&x);
}

/* ---- custom cmaps ---------------------------------------------------------------------------------------------- */
static void one_cmap(ttf_spec *s, const char *fam, ttf_cmap *cm, int fmt, unsigned plat, unsigned enc, const ttf_map *map, int n) {
    fo_spec(s, fam); cm->format = fmt; cm->plat = plat; cm->enc = enc; cm->n = n; cm->map = map; cm->nuvs = 0; cm->uvs = NULL; s->cmaps = cm; s->ncmaps = 1;
}
TEST(font_has_astral_format12) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x10000, 3 }, { 0x1F600, 4 }, { 0x1F601, 5 }, { 0x1F64F, 6 }, { 0x2F800, 7 }, { 0xE0001, 8 }, { 0x10FFFF, 9 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; size_t i; one_cmap(&s, "FoHasAst", &cm, 12, 3, 10, m, 8); FO_OPEN1(x, s);
    for (i = 0; i < 8; i++) CHECKF(has(x.font[0], m[i].cp) == 1, "U+%X", m[i].cp);
    EXPECT_EQ(has(x.font[0], 0x1F602), 0); EXPECT_EQ(has(x.font[0], 0x1F5FF), 0); EXPECT_EQ(has(x.font[0], 0x1F650), 0); EXPECT_EQ(has(x.font[0], 0x10001), 0);
    EXPECT_EQ(has(x.font[0], 0x10FFFE), 0); EXPECT_EQ(has(x.font[0], 0x110000), 0); EXPECT_EQ(has(x.font[0], 0x42), 0);
    EXPECT_EQ(has(x.font[0], 0xF600), 0); EXPECT_EQ(has(x.font[0], 0x0000), 0); EXPECT_EQ(has(x.font[0], 0xFFFF), 0);   /* no aliasing of the low 16 bits */
    fo_close(&x);
}
TEST(font_has_astral_alias_check_low_bits) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x1F641, 3 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; one_cmap(&s, "FoHasAl", &cm, 12, 3, 10, m, 2); FO_OPEN1(x, s);
    EXPECT_EQ(has(x.font[0], 0x41), 1); EXPECT_EQ(has(x.font[0], 0x1F641), 1); EXPECT_EQ(has(x.font[0], 0xF641), 0); EXPECT_EQ(has(x.font[0], 0x1F741), 0); EXPECT_EQ(has(x.font[0], 0x10041), 0);
    fo_close(&x);
}
TEST(font_has_bmp_edges) {
    static const ttf_map m[] = { { 0x20, 1 }, { 0xA0, 2 }, { 0xFF, 3 }, { 0x100, 4 }, { 0x7FF, 5 }, { 0x800, 6 }, { 0xD7FF, 7 }, { 0xE000, 8 }, { 0xF8FF, 9 }, { 0xFFFD, 10 }, { 0xFFFE, 11 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; size_t i; one_cmap(&s, "FoHasEdge", &cm, 4, 3, 1, m, 11); FO_OPEN1(x, s);
    for (i = 0; i < 11; i++) CHECKF(has(x.font[0], m[i].cp) == 1, "U+%04X", m[i].cp);
    EXPECT_EQ(has(x.font[0], 0xA1), 0); EXPECT_EQ(has(x.font[0], 0xFE), 0); EXPECT_EQ(has(x.font[0], 0x101), 0); EXPECT_EQ(has(x.font[0], 0x7FE), 0); EXPECT_EQ(has(x.font[0], 0x801), 0);
    EXPECT_EQ(has(x.font[0], 0xD800), 0); EXPECT_EQ(has(x.font[0], 0xDFFF), 0); EXPECT_EQ(has(x.font[0], 0xE001), 0); EXPECT_EQ(has(x.font[0], 0xF900), 0); EXPECT_EQ(has(x.font[0], 0xFFFC), 0);
    EXPECT_EQ(has(x.font[0], 0xFFFF), 0); EXPECT_EQ(has(x.font[0], 0x10000), 0); EXPECT_EQ(has(x.font[0], 0x1F), 0); EXPECT_EQ(has(x.font[0], 0x21), 0);
    fo_close(&x);
}
TEST(font_has_full_bmp_scan_matches_cmap) {
    ttf_map m[300]; unsigned lcg = 12345, i; fo_fx x; ttf_spec s; ttf_cmap cm; static unsigned char in[0x10000]; int n = 0; UINT32 cp, bad = 0;
    memset(in, 0, sizeof in);
    while (n < 300) { unsigned c; lcg = lcg * 1103515245u + 12345u; c = 0x20 + ((lcg >> 8) % 0xFFD0); if (c >= 0xD800 && c <= 0xDFFF) continue; if (in[c]) continue; in[c] = 1; m[n].cp = c; m[n].gid = 2 + n % 60; n++; }
    one_cmap(&s, "FoHasScan", &cm, 4, 3, 1, m, n); FO_OPEN1(x, s);
    for (cp = 0; cp < 0x10000; cp++) if (has(x.font[0], cp) != (in[cp] ? 1 : 0)) { if (bad++ < 5) CHECKF(0, "U+%04X: got %d want %d", cp, has(x.font[0], cp), in[cp]); }
    CHECKF(bad == 0, "%u mismatches over the BMP", bad);
    (void)i; fo_close(&x);
}
TEST(font_has_format12_only_covers_bmp) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 3 }, { 0x4E00, 4 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; one_cmap(&s, "FoHas12Bmp", &cm, 12, 3, 10, m, 3); FO_OPEN1(x, s);
    EXPECT_EQ(has(x.font[0], 0x41), 1); EXPECT_EQ(has(x.font[0], 0x42), 1); EXPECT_EQ(has(x.font[0], 0x4E00), 1); EXPECT_EQ(has(x.font[0], 0x43), 0);
    fo_close(&x);
}
TEST(font_has_both_bmp_and_full_cmap) {
    static const ttf_map bmp[] = { { 0x41, 2 }, { 0x42, 3 } };
    static const ttf_map full[] = { { 0x41, 2 }, { 0x42, 3 }, { 0x1F600, 4 } };
    ttf_cmap cm[2]; fo_fx x; ttf_spec s; fo_spec(&s, "FoHasBoth");
    cm[0].format = 4; cm[0].plat = 3; cm[0].enc = 1; cm[0].n = 2; cm[0].map = bmp; cm[0].nuvs = 0; cm[0].uvs = NULL;
    cm[1].format = 12; cm[1].plat = 3; cm[1].enc = 10; cm[1].n = 3; cm[1].map = full; cm[1].nuvs = 0; cm[1].uvs = NULL; s.cmaps = cm; s.ncmaps = 2;
    FO_OPEN1(x, s);
    EXPECT_EQ(has(x.font[0], 0x41), 1); EXPECT_EQ(has(x.font[0], 0x1F600), 1); EXPECT_EQ(has(x.font[0], 0x1F601), 0);
    fo_close(&x);
}
TEST(font_has_unicode_platform_cmaps_note) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 3 } };
    static const struct { unsigned plat, enc; int fmt; } t[] = { { 0, 3, 4 }, { 0, 4, 12 }, { 0, 0, 4 }, { 0, 1, 4 }, { 0, 2, 4 }, { 1, 0, 0 }, { 3, 0, 4 }, { 3, 1, 6 } };
    int i;
    for (i = 0; i < 8; i++) {
        fo_fx x; ttf_spec s; ttf_cmap cm; char f[32]; snprintf(f, sizeof f, "FoHasPl%d", i);
        one_cmap(&s, f, &cm, t[i].fmt, t[i].plat, t[i].enc, m, 2);
        if (!fo_open1(&x, &s)) { t_note("cmap (%u,%u) format %d only: font rejected (%s)", t[i].plat, t[i].enc, t[i].fmt, dw_hr_name(x.hr_coll)); fo_close(&x); continue; }
        t_note("cmap (%u,%u) format %d only: HasCharacter('A')=%d ('C')=%d symbol=%d", t[i].plat, t[i].enc, t[i].fmt, has(x.font[0], 0x41), has(x.font[0], 0x43), IDWriteFont_IsSymbolFont(x.font[0]));
        EXPECT_EQ(has(x.font[0], 0x43), 0);
        fo_close(&x);
    }
}
TEST(font_has_unicode_platform_0_3_supported) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 3 }, { 0x4E00, 4 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; one_cmap(&s, "FoHasP03", &cm, 4, 0, 3, m, 3);
    if (!fo_open1(&x, &s)) { t_note("platform 0 cmap rejected (%s)", dw_hr_name(x.hr_coll)); fo_close(&x); return; }
    t_note("platform 0 encoding 3: 'A' %d U+4E00 %d", has(x.font[0], 0x41), has(x.font[0], 0x4E00));
    fo_close(&x);
}
TEST(font_has_symbol_cmap) {
    fo_fx x; ttf_spec s; int i; fo_symbol_spec(&s, "FoHasSym"); FO_OPEN1(x, s);
    for (i = 0; i < 26; i++) CHECKF(has(x.font[0], 0xF041 + i) == 1, "U+%04X", 0xF041 + i);
    EXPECT_EQ(has(x.font[0], 0xF020), 1); EXPECT_EQ(has(x.font[0], 0xF05B), 0); EXPECT_EQ(has(x.font[0], 0xF040), 0); EXPECT_EQ(has(x.font[0], 0xF021), 0); EXPECT_EQ(has(x.font[0], 0xF061), 0);
    fo_close(&x);
}
TEST(font_has_symbol_cmap_cross_populates_low_range) {
    /* CompactCmapRegion::Create calls CrossPopulateSymbols for a (3,0) map: pages U+F000..U+F0FF and U+0000..U+00FF are OR-ed into each other */
    fo_fx x; ttf_spec s; int i; fo_symbol_spec(&s, "FoHasSym2"); FO_OPEN1(x, s);
    for (i = 0; i < 26; i++) CHECKF(has(x.font[0], 0x41 + i) == 1, "U+%04X", 0x41 + i);
    EXPECT_EQ(has(x.font[0], 0x20), 1); EXPECT_EQ(has(x.font[0], 0x21), 0); EXPECT_EQ(has(x.font[0], 0x40), 0); EXPECT_EQ(has(x.font[0], 0x5B), 0); EXPECT_EQ(has(x.font[0], 0x61), 0);
    EXPECT_EQ(has(x.font[0], 0x100), 0); EXPECT_EQ(has(x.font[0], 0xF100), 0); EXPECT_EQ(has(x.font[0], 0xEF41), 0);
    fo_close(&x);
}
TEST(font_has_symbol_cmap_low_codes_mirror_up) {
    /* the mirroring is symmetric: a (3,0) subtable that maps U+0041 directly also answers U+F041 */
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 3 }, { 0xF030, 4 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; fo_spec(&s, "FoHasSym3"); cm.format = 4; cm.plat = 3; cm.enc = 0; cm.n = 3; cm.map = m; cm.nuvs = 0; cm.uvs = NULL; s.cmaps = &cm; s.ncmaps = 1; FO_OPEN1(x, s);
    EXPECT_EQ(has(x.font[0], 0x41), 1); EXPECT_EQ(has(x.font[0], 0xF041), 1); EXPECT_EQ(has(x.font[0], 0x42), 1); EXPECT_EQ(has(x.font[0], 0xF042), 1);
    EXPECT_EQ(has(x.font[0], 0x30), 1); EXPECT_EQ(has(x.font[0], 0xF030), 1); EXPECT_EQ(has(x.font[0], 0x43), 0); EXPECT_EQ(has(x.font[0], 0xF043), 0);
    fo_close(&x);
}
TEST(font_has_unicode_cmap_does_not_mirror) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0xF042, 3 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; one_cmap(&s, "FoHasNoMir", &cm, 4, 3, 1, m, 2); FO_OPEN1(x, s);
    EXPECT_EQ(has(x.font[0], 0x41), 1); EXPECT_EQ(has(x.font[0], 0xF041), 0); EXPECT_EQ(has(x.font[0], 0xF042), 1); EXPECT_EQ(has(x.font[0], 0x42), 0);
    fo_close(&x);
}
TEST(font_has_glyph_zero_mapping_note) {
    static const ttf_map m[] = { { 0x41, 0 }, { 0x42, 3 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; one_cmap(&s, "FoHasG0", &cm, 4, 3, 1, m, 2); FO_OPEN1(x, s);
    t_note("cmap entry U+0041 -> glyph 0: HasCharacter = %d", has(x.font[0], 0x41)); EXPECT_EQ(has(x.font[0], 0x42), 1);
    fo_close(&x);
}
TEST(font_has_glyph_out_of_range_note) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 500 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; one_cmap(&s, "FoHasGo", &cm, 4, 3, 1, m, 2);
    if (!fo_open1(&x, &s)) { t_note("cmap with out-of-range glyph rejected (%s)", dw_hr_name(x.hr_coll)); fo_close(&x); return; }
    t_note("cmap U+0042 -> glyph 500 of 69: HasCharacter = %d", has(x.font[0], 0x42)); EXPECT_EQ(has(x.font[0], 0x41), 1);
    fo_close(&x);
}
TEST(font_has_format0_and_6_note) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 3 } };
    int fm[2] = { 0, 6 }, i;
    for (i = 0; i < 2; i++) {
        fo_fx x; ttf_spec s; ttf_cmap cm; char f[32]; snprintf(f, sizeof f, "FoHasF%d", fm[i]);
        one_cmap(&s, f, &cm, fm[i], 3, 1, m, 2);
        if (!fo_open1(&x, &s)) { t_note("(3,1) format %d rejected (%s)", fm[i], dw_hr_name(x.hr_coll)); fo_close(&x); continue; }
        t_note("(3,1) format %d: 'A'=%d 'B'=%d", fm[i], has(x.font[0], 0x41), has(x.font[0], 0x42)); fo_close(&x);
    }
}
TEST(font_has_format13_many_to_one_note) {
    static const ttf_map m[] = { { 0x1000, 2 }, { 0x1001, 2 }, { 0x1002, 2 }, { 0x10000, 2 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; one_cmap(&s, "FoHas13", &cm, 13, 3, 10, m, 4);
    if (!fo_open1(&x, &s)) { t_note("format 13 cmap rejected (%s)", dw_hr_name(x.hr_coll)); fo_close(&x); return; }
    t_note("format 13: U+1000=%d U+1002=%d U+10000=%d", has(x.font[0], 0x1000), has(x.font[0], 0x1002), has(x.font[0], 0x10000)); fo_close(&x);
}
TEST(font_has_matches_glyph_indices_synthetic) {
    fo_fx x; ttf_spec s; IDWriteFontFace *face = NULL; UINT32 cps[64]; UINT16 gi[64]; int i; fo_spec(&s, "FoHasGi"); FO_OPEN1(x, s);
    IDWriteFont_CreateFontFace(x.font[0], &face);
    if (face) {
        for (i = 0; i < 64; i++) cps[i] = (i < 40) ? 0x20 + i * 2 : 0x400 + i;
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cps, 64, gi));
        for (i = 0; i < 64; i++) CHECKF(has(x.font[0], cps[i]) == (gi[i] != 0), "U+%04X gid %u", cps[i], gi[i]);
        DW_RELEASE(face);
    } else CHECK(0);
    fo_close(&x);
}
TEST(font_has_matches_glyph_indices_arial_bmp) {
    FO_SYS_REG(f, L"Arial"); IDWriteFontFace *face = NULL; UINT32 cps[512]; UINT16 gi[512]; int base, i, bad = 0;
    IDWriteFont_CreateFontFace(f, &face);
    if (face) {
        for (base = 0; base < 0x10000; base += 512) {
            for (i = 0; i < 512; i++) cps[i] = base + i;
            IDWriteFontFace_GetGlyphIndices(face, cps, 512, gi);
            for (i = 0; i < 512; i++) if (has(f, cps[i]) != (gi[i] != 0)) { if (bad++ < 5) CHECKF(0, "U+%04X: HasCharacter %d glyph %u", cps[i], has(f, cps[i]), gi[i]); }
        }
        CHECKF(bad == 0, "%d mismatches", bad);
        DW_RELEASE(face);
    }
    DW_RELEASE(f);
}

/* ---- system fonts ------------------------------------------------------------------------------------------------ */
TEST(font_has_system_arial_samples) {
    FO_SYS_REG(f, L"Arial");
    static const UINT32 yes[] = { 'A', 'Z', 'a', 'z', '0', '9', ' ', 0xA0, 0xE9, 0xFF, 0x153, 0x20AC, 0x2122, 0x2022, 0x627, 0x5D0, 0x416, 0x3A9, 0x3B1, 0x2190, 0x25A0 };
    static const UINT32 no[] = { 0x4E00, 0x3042, 0xAC00, 0x1F600, 0x1F4A9, 0xD800, 0xDC00, 0x110000, 0xFFFFFFFFu, 0xE000, 0x10FFFF };
    size_t i;
    for (i = 0; i < sizeof yes / sizeof yes[0]; i++) CHECKF(has(f, yes[i]) == 1, "Arial U+%04X", yes[i]);
    for (i = 0; i < sizeof no / sizeof no[0]; i++) CHECKF(has(f, no[i]) == 0, "Arial U+%04X", no[i]);
    DW_RELEASE(f);
}
TEST(font_has_system_arial_thai_note) {
    FO_SYS_REG(f, L"Arial"); t_note("Arial U+0E01 Thai: %d, U+0905 Devanagari: %d, U+0E01: %d", has(f, 0xE01), has(f, 0x905), has(f, 0xE01)); DW_RELEASE(f);
}
TEST(font_has_system_emoji_font) {
    FO_SYS_REG(f, L"Segoe UI Emoji");
    EXPECT_EQ(has(f, 0x1F600), 1); EXPECT_EQ(has(f, 0x1F4A9), 1); EXPECT_EQ(has(f, 0x2764), 1); EXPECT_EQ(has(f, 0x110000), 0); EXPECT_EQ(has(f, 0xD83D), 0);
    DW_RELEASE(f);
}
TEST(font_has_system_gothic_cjk) {
    FO_SYS_REG(f, L"MS Gothic");
    EXPECT_EQ(has(f, 0x3042), 1); EXPECT_EQ(has(f, 0x30A2), 1); EXPECT_EQ(has(f, 0x4E00), 1); EXPECT_EQ(has(f, 'A'), 1); EXPECT_EQ(has(f, 0x1F600), 0);
    DW_RELEASE(f);
}
TEST(font_has_system_courier_consolas) {
    static const WCHAR *fams[] = { L"Courier New", L"Consolas", L"Times New Roman", L"Segoe UI", L"Tahoma" };
    int k;
    for (k = 0; k < 5; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!f) continue;
        CHECKF(has(f, 'A') == 1, "%ls A", fams[k]); CHECKF(has(f, 0x20AC) == 1, "%ls euro", fams[k]); CHECKF(has(f, 0xD800) == 0, "%ls D800", fams[k]); CHECKF(has(f, 0x110000) == 0, "%ls beyond", fams[k]);
        DW_RELEASE(f);
    }
}
TEST(font_has_system_symbol_fonts_note) {
    static const WCHAR *fams[] = { L"Wingdings", L"Webdings", L"Marlett", L"Symbol" };
    int k;
    for (k = 0; k < 4; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!f) { t_note("%ls not installed", fams[k]); continue; }
        t_note("%ls: U+0041=%d U+F041=%d U+0020=%d U+F020=%d U+0030=%d U+F030=%d", fams[k], has(f, 0x41), has(f, 0xF041), has(f, 0x20), has(f, 0xF020), has(f, 0x30), has(f, 0xF030));
        DW_RELEASE(f);
    }
}
TEST(font_has_bold_italic_share_coverage_system) {
    FO_SYS_REG(r, L"Arial"); FO_SYS(b, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    UINT32 cp; int diff = 0;
    for (cp = 0x20; cp < 0x500; cp++) if (has(r, cp) != has(b, cp)) diff++;
    t_note("Arial regular vs bold coverage differences in U+0020..U+04FF: %d", diff);
    EXPECT_EQ(has(b, 'A'), 1);
    DW_RELEASE(r); DW_RELEASE(b);
}

/* ---- GetUnicodeRanges -------------------------------------------------------------------------------------------- */
TEST(font_ranges_default_exact) {
    fo_fx x; ttf_spec s; DWRITE_UNICODE_RANGE r[8]; UINT32 n = 0; HRESULT hr; fo_spec(&s, "FoRng1"); FO_OPEN1(x, s);
    memset(r, 0xCC, sizeof r);
    CHECK(ranges_of(x.font[0], r, 8, &n, &hr)); CHECK_HR_OK(hr); EXPECT_EQ(n, 4);
    EXPECT_EQ(r[0].first, 0x20); EXPECT_EQ(r[0].last, 0x20); EXPECT_EQ(r[1].first, 0x30); EXPECT_EQ(r[1].last, 0x39);
    EXPECT_EQ(r[2].first, 0x41); EXPECT_EQ(r[2].last, 0x5A); EXPECT_EQ(r[3].first, 0x61); EXPECT_EQ(r[3].last, 0x7A);
    fo_close(&x);
}
TEST(font_ranges_unused_entries_zeroed) {
    /* the whole buffer (maxRangeCount entries) is zero-filled before the ranges are stored */
    fo_fx x; ttf_spec s; DWRITE_UNICODE_RANGE r[8]; UINT32 n = 0; HRESULT hr; int i; fo_spec(&s, "FoRng2"); FO_OPEN1(x, s);
    memset(r, 0xCC, sizeof r);
    ranges_of(x.font[0], r, 8, &n, &hr);
    for (i = 4; i < 8; i++) { EXPECT_EQ(r[i].first, 0); EXPECT_EQ(r[i].last, 0); }
    fo_close(&x);
}
TEST(font_ranges_count_query_with_zero_max) {
    /* maxRangeCount 0 with a NULL buffer: E_NOT_SUFFICIENT_BUFFER and the required count */
    fo_fx x; ttf_spec s; UINT32 n = 0; HRESULT hr = S_OK; fo_spec(&s, "FoRng3"); FO_OPEN1(x, s);
    CHECK(ranges_of(x.font[0], NULL, 0, &n, &hr)); CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 4);
    fo_close(&x);
}
TEST(font_ranges_zero_max_with_buffer_untouched) {
    fo_fx x; ttf_spec s; DWRITE_UNICODE_RANGE r[2]; UINT32 n = 0; HRESULT hr = S_OK; fo_spec(&s, "FoRng4"); FO_OPEN1(x, s);
    memset(r, 0xCC, sizeof r);
    ranges_of(x.font[0], r, 0, &n, &hr); CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 4);
    EXPECT_EQ(r[0].first, 0xCCCCCCCC); EXPECT_EQ(r[1].last, 0xCCCCCCCC);
    fo_close(&x);
}
TEST(font_ranges_buffer_too_small_partial_fill) {
    fo_fx x; ttf_spec s; DWRITE_UNICODE_RANGE r[8]; UINT32 n = 0; HRESULT hr = S_OK; int m; fo_spec(&s, "FoRng5"); FO_OPEN1(x, s);
    for (m = 1; m <= 3; m++) {
        memset(r, 0xCC, sizeof r);
        ranges_of(x.font[0], r, m, &n, &hr);
        CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECKF(n == 4, "max %d: count %u", m, n);
        EXPECT_EQ(r[0].first, 0x20);
        if (m >= 2) EXPECT_EQ(r[1].first, 0x30);
        if (m >= 3) EXPECT_EQ(r[2].last, 0x5A);
        EXPECT_EQ(r[m].first, 0xCCCCCCCC);                  /* nothing stored past maxRangeCount */
    }
    fo_close(&x);
}
TEST(font_ranges_exact_fit) {
    fo_fx x; ttf_spec s; DWRITE_UNICODE_RANGE r[5]; UINT32 n = 0; HRESULT hr = E_FAIL; fo_spec(&s, "FoRng6"); FO_OPEN1(x, s);
    memset(r, 0xCC, sizeof r);
    ranges_of(x.font[0], r, 4, &n, &hr); CHECK_HR_OK(hr); EXPECT_EQ(n, 4); EXPECT_EQ(r[3].first, 0x61); EXPECT_EQ(r[3].last, 0x7A); EXPECT_EQ(r[4].first, 0xCCCCCCCC);
    fo_close(&x);
}
TEST(font_ranges_null_buffer_with_nonzero_max_is_invalid) {
    fo_fx x; ttf_spec s; UINT32 n = 0; HRESULT hr = S_OK; UINT32 caps[] = { 1, 4, 100, 0xFFFFFFFF }; int i; fo_spec(&s, "FoRng7"); FO_OPEN1(x, s);
    for (i = 0; i < 4; i++) { n = 0x55; ranges_of(x.font[0], NULL, caps[i], &n, &hr); CHECKF(hr == DW_E_INVALIDARG, "max %u -> %s", caps[i], dw_hr_name(hr)); EXPECT_EQ(n, 0); }
    fo_close(&x);
}
TEST(font_ranges_count_is_zeroed_first) {
    fo_fx x; ttf_spec s; DWRITE_UNICODE_RANGE r[1]; UINT32 n = 0xDEADBEEF; HRESULT hr; IDWriteFont1 *f1 = NULL; fo_spec(&s, "FoRng8"); FO_OPEN1(x, s);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont1, (void **)&f1);
    if (f1) { hr = IDWriteFont1_GetUnicodeRanges(f1, 1, NULL, &n); CHECK_HR(hr, DW_E_INVALIDARG); EXPECT_EQ(n, 0); (void)r; IDWriteFont1_Release(f1); }
    fo_close(&x);
}
TEST(font_ranges_merge_adjacent_code_points) {
    static const ttf_map m[] = { { 0x41, 2 }, { 0x42, 3 }, { 0x43, 4 }, { 0x45, 5 }, { 0x100, 6 }, { 0x101, 7 }, { 0x10000, 8 }, { 0x10001, 9 }, { 0x10FFFF, 10 } };
    fo_fx x; ttf_spec s; ttf_cmap cm; DWRITE_UNICODE_RANGE r[16]; UINT32 n = 0; HRESULT hr; one_cmap(&s, "FoRngMrg", &cm, 12, 3, 10, m, 9); FO_OPEN1(x, s);
    ranges_of(x.font[0], r, 16, &n, &hr); CHECK_HR_OK(hr); EXPECT_EQ(n, 5);
    if (n == 5) {
        EXPECT_EQ(r[0].first, 0x41); EXPECT_EQ(r[0].last, 0x43); EXPECT_EQ(r[1].first, 0x45); EXPECT_EQ(r[1].last, 0x45); EXPECT_EQ(r[2].first, 0x100); EXPECT_EQ(r[2].last, 0x101);
        EXPECT_EQ(r[3].first, 0x10000); EXPECT_EQ(r[3].last, 0x10001); EXPECT_EQ(r[4].first, 0x10FFFF); EXPECT_EQ(r[4].last, 0x10FFFF);
    }
    fo_close(&x);
}
TEST(font_ranges_many_isolated_code_points) {
    ttf_map m[200]; fo_fx x; ttf_spec s; ttf_cmap cm; DWRITE_UNICODE_RANGE *r; UINT32 n = 0, i; HRESULT hr; int k;
    for (k = 0; k < 200; k++) { m[k].cp = 0x100 + 2 * k; m[k].gid = 2 + k % 60; }
    one_cmap(&s, "FoRngMany", &cm, 4, 3, 1, m, 200); FO_OPEN1(x, s);
    ranges_of(x.font[0], NULL, 0, &n, &hr); CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 200);
    r = (DWRITE_UNICODE_RANGE *)calloc(n + 1, sizeof *r);
    ranges_of(x.font[0], r, n - 1, &n, &hr); CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 200);
    ranges_of(x.font[0], r, n, &n, &hr); CHECK_HR_OK(hr);
    for (i = 0; i < n; i++) { CHECKF(r[i].first == 0x100 + 2 * i && r[i].last == r[i].first, "range %u: %X-%X", i, r[i].first, r[i].last); }
    free(r); fo_close(&x);
}
TEST(font_ranges_symbol_font_mirrored_ranges) {
    /* symbol map F020, F041-F05A is reported with its cross-populated low copies: 0020, 0041-005A, F020, F041-F05A */
    fo_fx x; ttf_spec s; DWRITE_UNICODE_RANGE r[16]; UINT32 n = 0; HRESULT hr; fo_symbol_spec(&s, "FoRngSym"); FO_OPEN1(x, s);
    ranges_of(x.font[0], r, 16, &n, &hr); CHECK_HR_OK(hr); EXPECT_EQ(n, 4);
    if (n == 4) { EXPECT_EQ(r[0].first, 0x20); EXPECT_EQ(r[0].last, 0x20); EXPECT_EQ(r[1].first, 0x41); EXPECT_EQ(r[1].last, 0x5A); EXPECT_EQ(r[2].first, 0xF020); EXPECT_EQ(r[2].last, 0xF020); EXPECT_EQ(r[3].first, 0xF041); EXPECT_EQ(r[3].last, 0xF05A); }
    fo_close(&x);
}
TEST(font_ranges_consistent_with_has_character_synthetic) {
    fo_fx x; ttf_spec s; DWRITE_UNICODE_RANGE r[16]; UINT32 n = 0, i, total = 0, cp, hits = 0; HRESULT hr; fo_spec(&s, "FoRngCons"); FO_OPEN1(x, s);
    ranges_of(x.font[0], r, 16, &n, &hr);
    for (i = 0; i < n; i++) total += r[i].last - r[i].first + 1;
    for (cp = 0; cp < 0x1000; cp++) if (has(x.font[0], cp) == 1) hits++;
    EXPECT_EQ(total, hits);
    for (i = 0; i < n; i++) { CHECK(has(x.font[0], r[i].first) == 1); CHECK(has(x.font[0], r[i].last) == 1); CHECK(has(x.font[0], r[i].first - 1) == 0); CHECK(has(x.font[0], r[i].last + 1) == 0); }
    fo_close(&x);
}
TEST(font_ranges_system_arial_protocol) {
    FO_SYS_REG(f, L"Arial"); UINT32 n = 0, n2 = 0, i; HRESULT hr; DWRITE_UNICODE_RANGE *r;
    ranges_of(f, NULL, 0, &n, &hr); CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(n > 50);
    r = (DWRITE_UNICODE_RANGE *)calloc(n + 2, sizeof *r);
    ranges_of(f, r, n - 1, &n2, &hr); CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n2, n);
    ranges_of(f, r, n, &n2, &hr); CHECK_HR_OK(hr); EXPECT_EQ(n2, n);
    ranges_of(f, r, n + 2, &n2, &hr); CHECK_HR_OK(hr); EXPECT_EQ(n2, n);
    for (i = 0; i < n; i++) {
        CHECKF(r[i].first <= r[i].last && r[i].last <= 0x10FFFF, "range %u: %X-%X", i, r[i].first, r[i].last);
        if (i) CHECKF(r[i].first > r[i - 1].last + 1, "range %u not separated/ascending", i);
    }
    EXPECT_EQ(r[n].first, 0); EXPECT_EQ(r[n + 1].last, 0);
    t_note("Arial has %u unicode ranges", n);
    free(r); DW_RELEASE(f);
}
TEST(font_ranges_system_arial_cover_has_character) {
    FO_SYS_REG(f, L"Arial"); UINT32 n = 0, i, cp; HRESULT hr; DWRITE_UNICODE_RANGE *r; unsigned char *in = (unsigned char *)calloc(0x110000, 1); UINT32 bad = 0;
    ranges_of(f, NULL, 0, &n, &hr); r = (DWRITE_UNICODE_RANGE *)calloc(n + 1, sizeof *r); ranges_of(f, r, n, &n, &hr);
    for (i = 0; i < n; i++) for (cp = r[i].first; cp <= r[i].last; cp++) in[cp] = 1;
    for (cp = 0; cp < 0x10000; cp++) if (has(f, cp) != in[cp]) { if (bad++ < 5) CHECKF(0, "U+%04X: HasCharacter %d range %d", cp, has(f, cp), in[cp]); }
    CHECKF(bad == 0, "%u mismatches", bad);
    for (cp = 0x10000; cp < 0x110000; cp += 997) CHECK(in[cp] == 0 || has(f, cp) == 1);
    free(in); free(r); DW_RELEASE(f);
}
TEST(font_ranges_emoji_has_astral) {
    FO_SYS_REG(f, L"Segoe UI Emoji"); UINT32 n = 0, i; HRESULT hr; DWRITE_UNICODE_RANGE *r; int astral = 0;
    ranges_of(f, NULL, 0, &n, &hr); r = (DWRITE_UNICODE_RANGE *)calloc(n + 1, sizeof *r); ranges_of(f, r, n, &n, &hr); CHECK_HR_OK(hr);
    for (i = 0; i < n; i++) if (r[i].first <= 0x1F600 && r[i].last >= 0x1F600) astral = 1;
    CHECK(astral); free(r); DW_RELEASE(f);
}
TEST(font_ranges_through_font2_font3) {
    fo_fx x; ttf_spec s; IDWriteFont2 *f2 = NULL; IDWriteFont3 *f3 = NULL; DWRITE_UNICODE_RANGE r[8]; UINT32 n = 0; fo_spec(&s, "FoRngIf"); FO_OPEN1(x, s);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont2, (void **)&f2); IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont3, (void **)&f3);
    if (f2 && f3) {
        CHECK_HR_OK(IDWriteFont2_GetUnicodeRanges(f2, 8, r, &n)); EXPECT_EQ(n, 4);
        CHECK_HR(IDWriteFont3_GetUnicodeRanges(f3, 0, NULL, &n), DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, 4);
    } else CHECK(0);
    DW_RELEASE(f2); DW_RELEASE(f3); fo_close(&x);
}
TEST(font_ranges_simulated_same_as_base) {
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL; IDWriteFont *b = NULL; UINT32 n1 = 0, n2 = 0; HRESULT hr; fo_spec(&s, "FoRngSim"); FO_OPEN1(x, s);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) { IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &b); DW_RELEASE(fam); }
    if (b) { ranges_of(x.font[0], NULL, 0, &n1, &hr); ranges_of(b, NULL, 0, &n2, &hr); EXPECT_EQ(n1, n2); DW_RELEASE(b); }
    fo_close(&x);
}
