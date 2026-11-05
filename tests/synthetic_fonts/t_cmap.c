/* cmap parsing through IDWriteFontFace::GetGlyphIndices.
 *
 * Decomp (OpenTypeCharacterMapSubtable, PlatformIdAndEncodingIdToCharacterEncoding, OpenTypeCharacterMap::Read, ReadFormat0/4/6/12or13):
 *   - among all encoding records the one with the highest CharacterEncoding value wins, the first record on ties:
 *       (0,4),(0,6),(3,10) = 9 > (3,1) = 8 > (3,5) = 7 > (3,4) = 6 > (3,3) = 5 > (3,2) = 4 > (3,0) symbol = 3 > (0,0..3),(0,5)... = 2 > (1,0) = 1 > anything else = 0 (unusable);
 *   - mapped glyph ids must be non-zero (format 0/4/6) and < the face's glyph count, otherwise the code point is left unmapped;
 *   - format 4: idDelta arithmetic is 16-bit, idRangeOffset indexes a glyph array (a 0 glyph there = unmapped), U+FFFF is never mapped, an
 *     0xFFFF/0xFFFF segment ends the walk, segments are clamped to start above the previous segment's end;
 *   - format 12 and 13 groups; format 14 and 2 are not used for Unicode encodings. */
#include "sf.h"

typedef struct { sf_probe p; sf_blob b; } cmf;
static int cm_open(cmf *c, const sf_cmap_rec *recs, int n, const char *label) {
    ttf_spec s; sf_buf t = sf_cmap_table(recs, n); ttf_rawtable raw;
    ttf_default_spec(&s); raw.tag[0] = 0; memcpy(raw.tag, "cmap", 5); raw.data = t.p; raw.len = (unsigned)t.n; s.raw = &raw; s.nraw = 1;
    c->b = sf_build(&s); sf_buf_free(&t);
    sf_survey_light = 0;
    if (!sf_open_face(&c->p, c->b.p, c->b.n)) { sf_close(&c->p); sf_free(&c->b); return 0; }
    sf_survey(label, &c->p);
    return 1;
}
static void cm_close(cmf *c) { sf_close(&c->p); sf_free(&c->b); }
#define G(c, cp) sf_gid((c).p.face, cp)
#define OPEN1(c, plat, enc, buf, label) cmf c; sf_cmap_rec r1_ = { plat, enc, (buf).p, (buf).n }; if (!cm_open(&c, &r1_, 1, label)) { sf_buf_free(&buf); return; } sf_buf_free(&buf); \
    if (!c.p.face) { CHECKF(0, "%s: no face (%s)", label, dw_hr_name(c.p.hr_face)); cm_close(&c); return; }
static const sf_cmap_rec NOREC = { 0, 0, NULL, 0 };

/* ---- format 4 ------------------------------------------------------------------------------------------------------ */
TEST(cm4_single_segment_delta) {
    sf_seg4g s[] = { { 0x41, 0x43, 2 - 0x41, NULL } }; sf_buf t = sf_cmap4(s, 1, 1);
    OPEN1(c, 3, 1, t, "f4 single segment");
    CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x42) == 3); CHECK(G(c, 0x43) == 4); CHECK(G(c, 0x44) == 0); CHECK(G(c, 0x40) == 0); CHECK(G(c, 0) == 0);
    cm_close(&c);
}
TEST(cm4_delta_wraps_modulo_65536) {
    sf_seg4g s[] = { { 0x8000, 0x8002, (int)(5 - 0x8000), NULL }, { 0xFFF0, 0xFFF2, (int)(9 - 0xFFF0), NULL } };   /* second: delta 0x000A - 0x10000... */
    sf_buf t = sf_cmap4(s, 2, 1);
    OPEN1(c, 3, 1, t, "f4 delta wrap");
    CHECK(G(c, 0x8000) == 5); CHECK(G(c, 0x8001) == 6); CHECK(G(c, 0x8002) == 7);
    CHECK(G(c, 0xFFF0) == 9); CHECK(G(c, 0xFFF1) == 10); CHECK(G(c, 0xFFF2) == 11);
    cm_close(&c);
}
TEST(cm4_delta_addition_wraps_upwards) {
    /* code 0x0010 + delta 0xFFF0 = 0x1_0000 -> 0 (unmapped); code 0x0011 -> 1 */
    sf_seg4g s[] = { { 0x10, 0x12, (int)0xFFF0, NULL } }; sf_buf t = sf_cmap4(s, 1, 1);
    OPEN1(c, 3, 1, t, "f4 delta wraps to zero");
    CHECK(G(c, 0x10) == 0); CHECK(G(c, 0x11) == 1); CHECK(G(c, 0x12) == 2);
    cm_close(&c);
}
TEST(cm4_idrangeoffset_glyph_array) {
    static const uint16_t ga[] = { 28, 29, 30, 0, 32 };
    sf_seg4g s[] = { { 0x61, 0x65, 0, ga } }; sf_buf t = sf_cmap4(s, 1, 1);
    OPEN1(c, 3, 1, t, "f4 glyph array");
    CHECK(G(c, 0x61) == 28); CHECK(G(c, 0x62) == 29); CHECK(G(c, 0x63) == 30); CHECK(G(c, 0x64) == 0);    /* array entry 0 = missing glyph, delta is not applied */
    CHECK(G(c, 0x65) == 32); CHECK(G(c, 0x66) == 0);
    cm_close(&c);
}
TEST(cm4_idrangeoffset_adds_delta_to_array_entry) {
    static const uint16_t ga[] = { 10, 11, 0, 13 };
    sf_seg4g s[] = { { 0x70, 0x73, 5, ga } }; sf_buf t = sf_cmap4(s, 1, 1);
    OPEN1(c, 3, 1, t, "f4 glyph array + delta");
    CHECK(G(c, 0x70) == 15); CHECK(G(c, 0x71) == 16); CHECK(G(c, 0x72) == 0); CHECK(G(c, 0x73) == 18);
    cm_close(&c);
}
TEST(cm4_glyph_array_delta_wraps) {
    static const uint16_t ga[] = { 3, 4 };
    sf_seg4g s[] = { { 0x70, 0x71, (int)0xFFFE, ga } }; sf_buf t = sf_cmap4(s, 1, 1);
    OPEN1(c, 3, 1, t, "f4 glyph array + wrapping delta");
    CHECK(G(c, 0x70) == 1); CHECK(G(c, 0x71) == 2);
    cm_close(&c);
}
TEST(cm4_mixed_segments_in_order) {
    static const uint16_t ga[] = { 40, 41, 42 };
    sf_seg4g s[] = { { 0x20, 0x20, 1 - 0x20, NULL }, { 0x41, 0x43, 2 - 0x41, NULL }, { 0x61, 0x63, 0, ga }, { 0x100, 0x102, 60 - 0x100, NULL } };
    sf_buf t = sf_cmap4(s, 4, 1);
    OPEN1(c, 3, 1, t, "f4 four mixed segments");
    CHECK(G(c, 0x20) == 1); CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x43) == 4); CHECK(G(c, 0x61) == 40); CHECK(G(c, 0x63) == 42);
    CHECK(G(c, 0x100) == 60); CHECK(G(c, 0x102) == 62); CHECK(G(c, 0x103) == 0); CHECK(G(c, 0x50) == 0);
    cm_close(&c);
}
TEST(cm4_glyph_ids_at_or_above_glyph_count_dropped) {
    /* glyph count is 69: gid 68 is the last valid one */
    sf_seg4g s[] = { { 0x41, 0x44, 67 - 0x41, NULL } }; sf_buf t = sf_cmap4(s, 1, 1);      /* A->67 B->68 C->69 D->70 */
    OPEN1(c, 3, 1, t, "f4 gids at the glyph count");
    CHECK(G(c, 0x41) == 67); CHECK(G(c, 0x42) == 68); CHECK(G(c, 0x43) == 0); CHECK(G(c, 0x44) == 0);
    cm_close(&c);
}
TEST(cm4_far_out_of_range_glyph_ids_dropped) {
    sf_seg4g s[] = { { 0x41, 0x42, 1000 - 0x41, NULL } }; sf_buf t = sf_cmap4(s, 1, 1);
    OPEN1(c, 3, 1, t, "f4 gid 1000");
    CHECK(G(c, 0x41) == 0); CHECK(G(c, 0x42) == 0);
    cm_close(&c);
}
TEST(cm4_u_ffff_never_mapped) {
    sf_seg4g s[] = { { 0xFFFD, 0xFFFF, 3 - 0xFFFD, NULL } }; sf_buf t = sf_cmap4(s, 1, 0);          /* no terminating segment either */
    OPEN1(c, 3, 1, t, "f4 FFFD..FFFF without terminator");
    CHECK(G(c, 0xFFFD) == 3); CHECK(G(c, 0xFFFE) == 4); CHECK(G(c, 0xFFFF) == 0);
    cm_close(&c);
}
TEST(cm4_no_terminator_segment_is_fine) {
    sf_seg4g s[] = { { 0x41, 0x42, 2 - 0x41, NULL } }; sf_buf t = sf_cmap4(s, 1, 0);
    OPEN1(c, 3, 1, t, "f4 no terminator");
    CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x42) == 3);
    cm_close(&c);
}
TEST(cm4_terminator_ends_the_walk) {
    /* segments after an 0xFFFF/0xFFFF segment are ignored */
    sf_buf o = { 0, 0, 0 }; int i; unsigned ends[3] = { 0x42, 0xFFFF, 0x62 }, starts[3] = { 0x41, 0xFFFF, 0x61 };
    sf_b16(&o, 4); sf_b16(&o, 14 + 8 * 3 + 2); sf_b16(&o, 0); sf_b16(&o, 6); sf_b16(&o, 4); sf_b16(&o, 1); sf_b16(&o, 2);
    for (i = 0; i < 3; i++) sf_b16(&o, ends[i]); sf_b16(&o, 0);
    for (i = 0; i < 3; i++) sf_b16(&o, starts[i]);
    sf_b16(&o, (unsigned)(2 - 0x41) & 0xFFFF); sf_b16(&o, 1); sf_b16(&o, (unsigned)(28 - 0x61) & 0xFFFF);
    for (i = 0; i < 3; i++) sf_b16(&o, 0);
    { OPEN1(c, 3, 1, o, "f4 segment after the terminator");
      CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x42) == 3); CHECK(G(c, 0x61) == 0); CHECK(G(c, 0x62) == 0); cm_close(&c); }
}
TEST(cm4_overlapping_segments_first_wins) {
    sf_seg4g s[] = { { 0x41, 0x45, 2 - 0x41, NULL }, { 0x43, 0x48, 20 - 0x43, NULL } };
    sf_buf t = sf_cmap4(s, 2, 1);
    OPEN1(c, 3, 1, t, "f4 overlapping segments");
    CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x43) == 4); CHECK(G(c, 0x45) == 6);       /* overlap keeps the first segment */
    CHECK(G(c, 0x46) == 23); CHECK(G(c, 0x48) == 25);                              /* remainder from the second */
    cm_close(&c);
}
TEST(cm4_descending_segments_skipped) {
    /* a segment ending below the previous end contributes nothing */
    sf_seg4g s[] = { { 0x61, 0x63, 28 - 0x61, NULL }, { 0x41, 0x43, 2 - 0x41, NULL } };
    sf_buf t = sf_cmap4(s, 2, 1);
    OPEN1(c, 3, 1, t, "f4 descending segments");
    CHECK(G(c, 0x61) == 28);
    PRED("out-of-order segment ignored", G(c, 0x41) == 0);
    cm_close(&c);
}
TEST(cm4_end_before_start_ignored) {
    sf_seg4g s[] = { { 0x45, 0x41, 2, NULL }, { 0x61, 0x62, 28 - 0x61, NULL } };
    sf_buf t = sf_cmap4(s, 2, 1);
    OPEN1(c, 3, 1, t, "f4 end < start");
    CHECK(G(c, 0x61) == 28); CHECK(G(c, 0x62) == 29);
    cm_close(&c);
}
TEST(cm4_zero_segments) {
    sf_buf t = sf_cmap4(NULL, 0, 0);
    OPEN1(c, 3, 1, t, "f4 zero segments");
    CHECK(G(c, 0x41) == 0); CHECK(G(c, 0x20) == 0);
    cm_close(&c);
}
TEST(cm4_only_terminator) {
    sf_buf t = sf_cmap4(NULL, 0, 1);
    OPEN1(c, 3, 1, t, "f4 only the terminator");
    CHECK(G(c, 0x41) == 0); CHECK(G(c, 0xFFFF) == 0);
    cm_close(&c);
}
TEST(cm4_full_range_one_segment) {
    sf_seg4g s[] = { { 0x0000, 0xFFFE, 0, NULL } }; sf_buf t = sf_cmap4(s, 1, 1);
    OPEN1(c, 3, 1, t, "f4 identity 0..FFFE");
    CHECK(G(c, 0) == 0); CHECK(G(c, 1) == 1); CHECK(G(c, 68) == 68); CHECK(G(c, 69) == 0); CHECK(G(c, 0x1000) == 0);
    cm_close(&c);
}
TEST(cm4_glyph_array_past_end_of_table) {
    static const uint16_t ga[] = { 28, 29 };
    sf_seg4g s[] = { { 0x61, 0x62, 0, ga } }; sf_buf t = sf_cmap4(s, 1, 1);
    sf_w16(t.p + 14, 0x006A);               /* endCode[0] = 0x6A: the segment claims ten glyph array entries, only two exist */
    { OPEN1(c, 3, 1, t, "f4 glyph array shorter than the segment");
      t_note("0x61=%u 0x62=%u 0x63=%u 0x6A=%u", G(c, 0x61), G(c, 0x62), G(c, 0x63), G(c, 0x6A));
      PRED("entries inside the table map", G(c, 0x61) == 28 && G(c, 0x62) == 29);
      PRED("entries past the table stay unmapped", G(c, 0x63) == 0 && G(c, 0x6A) == 0); cm_close(&c); }
}
TEST(cm4_idrangeoffset_beyond_table) {
    static const uint16_t ga[] = { 28 };
    sf_seg4g s[] = { { 0x61, 0x61, 0, ga } }; sf_buf t = sf_cmap4(s, 1, 1);
    /* idRangeOffset[0] points far beyond the table */
    sf_w16(t.p + 14 + 2 * 2 + 2 + 2 * 2 + 2 * 2, 0x4000);
    { cmf c; sf_cmap_rec r = { 3, 1, t.p, t.n };
      if (cm_open(&c, &r, 1, "f4 idRangeOffset beyond the table")) { if (c.p.face) { t_note("0x61 -> %u", G(c, 0x61)); PRED("unmapped", G(c, 0x61) == 0); } cm_close(&c); } }
    sf_buf_free(&t);
}
TEST(cm4_truncated_subtable) {
    if (t_is_wine()) { t_skip("Wine crashes reading the damaged format 4 subtable"); return; }
    static const uint16_t ga[] = { 28 };
    sf_seg4g s[] = { { 0x61, 0x61, 0, ga } }; sf_buf t = sf_cmap4(s, 1, 1);
    size_t cut[] = { 0, 4, 6, 12, 13, 14, 16 }; size_t i;
    for (i = 0; i < sizeof cut / sizeof cut[0]; i++) {
        cmf c; sf_cmap_rec r; char l[64]; r.plat = 3; r.enc = 1; r.data = t.p; r.len = cut[i];
        snprintf(l, sizeof l, "f4 subtable cut to %u bytes", (unsigned)cut[i]);
        if (cm_open(&c, &r, 1, l)) { if (c.p.face) t_note("  cut %u: face created, 0x61 -> %u", (unsigned)cut[i], G(c, 0x61)); cm_close(&c); }
    }
    sf_buf_free(&t);
}
TEST(cm4_segcount_larger_than_table) {
    if (t_is_wine()) { t_skip("Wine crashes reading the damaged format 4 subtable"); return; }
    sf_seg4g s[] = { { 0x41, 0x42, 2 - 0x41, NULL } }; sf_buf t = sf_cmap4(s, 1, 1); cmf c; sf_cmap_rec r;
    sf_w16(t.p + 6, 2000);          /* segCountX2 = 2000: arrays run past the subtable */
    r.plat = 3; r.enc = 1; r.data = t.p; r.len = t.n;
    if (cm_open(&c, &r, 1, "f4 segCountX2 larger than the table")) { t_note("face %s", c.p.face ? "created" : "NOT created"); cm_close(&c); }
    sf_buf_free(&t);
}
TEST(cm4_odd_segcountx2) {
    sf_seg4g s[] = { { 0x41, 0x42, 2 - 0x41, NULL } }; sf_buf t = sf_cmap4(s, 1, 1); cmf c; sf_cmap_rec r;
    sf_w16(t.p + 6, 5);
    r.plat = 3; r.enc = 1; r.data = t.p; r.len = t.n;
    if (cm_open(&c, &r, 1, "f4 odd segCountX2")) { if (c.p.face) t_note("0x41 -> %u", G(c, 0x41)); cm_close(&c); }
    sf_buf_free(&t);
}

/* ---- format 0 ------------------------------------------------------------------------------------------------------ */
TEST(cm0_basic_on_unicode_platform) {
    uint8_t m[256]; int i; memset(m, 0, sizeof m);
    for (i = 0; i < 26; i++) m['A' + i] = (uint8_t)(2 + i);
    m[0x20] = 1; m[0xFF] = 7; m[0x80] = 8;
    { sf_buf t = sf_cmap0(m); OPEN1(c, 3, 1, t, "f0 under (3,1)");
      CHECK(G(c, 'A') == 2); CHECK(G(c, 'Z') == 27); CHECK(G(c, 0x20) == 1); CHECK(G(c, 0xFF) == 7); CHECK(G(c, 0x80) == 8); CHECK(G(c, 'a') == 0); CHECK(G(c, 0x100) == 0);
      cm_close(&c); }
}
TEST(cm0_glyph_ids_at_or_above_count_dropped) {
    uint8_t m[256]; memset(m, 0, sizeof m); m['A'] = 68; m['B'] = 69; m['C'] = 200; m['D'] = 255;
    { sf_buf t = sf_cmap0(m); OPEN1(c, 3, 1, t, "f0 gid limits");
      CHECK(G(c, 'A') == 68); CHECK(G(c, 'B') == 0); CHECK(G(c, 'C') == 0); CHECK(G(c, 'D') == 0); cm_close(&c); }
}
TEST(cm0_on_unicode_platform_zero) {
    uint8_t m[256]; int i; memset(m, 0, sizeof m); for (i = 0; i < 26; i++) m['a' + i] = (uint8_t)(28 + i);
    { sf_buf t = sf_cmap0(m); OPEN1(c, 0, 3, t, "f0 under (0,3)");
      CHECK(G(c, 'a') == 28); CHECK(G(c, 'z') == 53); cm_close(&c); }
}
TEST(cm0_mac_roman_translation) {
    uint8_t m[256]; int i; memset(m, 0, sizeof m);
    for (i = 0; i < 26; i++) m['A' + i] = (uint8_t)(2 + i);
    m[0x80] = 30; m[0x81] = 31; m[0x8E] = 32; m[0xA9] = 33; m[0xD0] = 34; m[0xC4] = 35;
    { sf_buf t = sf_cmap0(m); OPEN1(c, 1, 0, t, "f0 under (1,0) MacRoman");
      CHECK(G(c, 'A') == 2); CHECK(G(c, 'Z') == 27);
      CHECK(G(c, 0xC4) == 30);          /* 0x80 = A diaeresis */
      CHECK(G(c, 0xC5) == 31);          /* 0x81 = A ring */
      CHECK(G(c, 0xE9) == 32);          /* 0x8E = e acute */
      CHECK(G(c, 0xA9) == 33);          /* 0xA9 = copyright */
      CHECK(G(c, 0x2013) == 34);        /* 0xD0 = en dash */
      CHECK(G(c, 0x192) == 35);         /* 0xC4 = florin */
      CHECK(G(c, 0x80) == 0);           /* raw byte value is not a Unicode code point here */
      cm_close(&c); }
}
TEST(cm0_short_subtable) {
    uint8_t m[256]; sf_buf t; cmf c; sf_cmap_rec r; memset(m, 0, sizeof m); m['A'] = 2;
    t = sf_cmap0(m); r.plat = 3; r.enc = 1; r.data = t.p; r.len = 100;             /* shorter than 262 bytes */
    if (cm_open(&c, &r, 1, "f0 truncated to 100 bytes")) { t_note("face %s", c.p.face ? "created" : "NOT created"); cm_close(&c); }
    sf_buf_free(&t);
}

/* ---- format 6 ------------------------------------------------------------------------------------------------------ */
TEST(cm6_basic) {
    static const uint16_t g[] = { 2, 3, 0, 5, 200 };
    sf_buf t = sf_cmap6(0x41, g, 5);
    OPEN1(c, 3, 1, t, "f6 five entries from 0x41");
    CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x42) == 3); CHECK(G(c, 0x43) == 0); CHECK(G(c, 0x44) == 5); CHECK(G(c, 0x45) == 0); CHECK(G(c, 0x46) == 0); CHECK(G(c, 0x40) == 0);
    cm_close(&c);
}
TEST(cm6_zero_entries) {
    sf_buf t = sf_cmap6(0x41, NULL, 0);
    OPEN1(c, 3, 1, t, "f6 zero entries");
    CHECK(G(c, 0x41) == 0);
    cm_close(&c);
}
TEST(cm6_high_first_code) {
    static const uint16_t g[] = { 9, 10 };
    sf_buf t = sf_cmap6(0xFFF0, g, 2);
    OPEN1(c, 3, 1, t, "f6 first code 0xFFF0");
    CHECK(G(c, 0xFFF0) == 9); CHECK(G(c, 0xFFF1) == 10);
    cm_close(&c);
}
TEST(cm6_count_exceeds_table) {
    static const uint16_t g[] = { 2, 3 };
    sf_buf t = sf_cmap6(0x41, g, 2); cmf c; sf_cmap_rec r;
    sf_w16(t.p + 8, 500);           /* entryCount 500, only 2 present */
    r.plat = 3; r.enc = 1; r.data = t.p; r.len = t.n;
    if (cm_open(&c, &r, 1, "f6 entryCount larger than the table")) { t_note("face %s", c.p.face ? "created" : "NOT created"); cm_close(&c); }
    sf_buf_free(&t);
}

/* ---- format 12 / 13 -------------------------------------------------------------------------------------------------- */
TEST(cm12_groups) {
    sf_grp g[] = { { 0x20, 0x20, 1 }, { 0x41, 0x5A, 2 }, { 0x61, 0x7A, 28 } }; sf_buf t = sf_cmap12(g, 3, 12);
    OPEN1(c, 3, 10, t, "f12 three groups under (3,10)");
    CHECK(G(c, 0x20) == 1); CHECK(G(c, 'A') == 2); CHECK(G(c, 'Z') == 27); CHECK(G(c, 'a') == 28); CHECK(G(c, 'z') == 53); CHECK(G(c, 0x30) == 0); CHECK(G(c, 0x7B) == 0);
    cm_close(&c);
}
TEST(cm12_supplementary_plane) {
    sf_grp g[] = { { 0x1F600, 0x1F602, 10 }, { 0x10000, 0x10000, 20 }, { 0x10FFFF, 0x10FFFF, 30 } }; sf_buf t = sf_cmap12(g, 3, 12);
    OPEN1(c, 3, 10, t, "f12 supplementary plane");
    CHECK(G(c, 0x1F600) == 10); CHECK(G(c, 0x1F601) == 11); CHECK(G(c, 0x1F602) == 12); CHECK(G(c, 0x1F603) == 0);
    CHECK(G(c, 0x10000) == 20); CHECK(G(c, 0x10FFFF) == 30);
    CHECK(G(c, 0x110000) == 0); CHECK(G(c, 0xFFFFFFFF) == 0);
    cm_close(&c);
}
TEST(cm12_glyph_limit_inside_group) {
    sf_grp g[] = { { 0x1000, 0x1010, 60 } }; sf_buf t = sf_cmap12(g, 1, 12);       /* 60..76: only 60..68 exist */
    OPEN1(c, 3, 10, t, "f12 group running past the glyph count");
    CHECK(G(c, 0x1000) == 60); CHECK(G(c, 0x1008) == 68); CHECK(G(c, 0x1009) == 0); CHECK(G(c, 0x1010) == 0);
    cm_close(&c);
}
TEST(cm12_group_end_ffffffff) {
    sf_grp g[] = { { 0xFFFFFFF0u, 0xFFFFFFFFu, 5 } }; sf_buf t = sf_cmap12(g, 1, 12);
    OPEN1(c, 3, 10, t, "f12 group to 0xFFFFFFFF");
    t_note("0xFFFFFFF0 -> %u, 0xFFFFFFFE -> %u", G(c, 0xFFFFFFF0u), G(c, 0xFFFFFFFEu));
    cm_close(&c);
}
TEST(cm12_overlapping_groups) {
    sf_grp g[] = { { 0x41, 0x45, 2 }, { 0x43, 0x47, 40 } }; sf_buf t = sf_cmap12(g, 2, 12);
    OPEN1(c, 3, 10, t, "f12 overlapping groups");
    t_note("A=%u C=%u E=%u F=%u G=%u", G(c, 0x41), G(c, 0x43), G(c, 0x45), G(c, 0x46), G(c, 0x47));
    CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x42) == 3);
    cm_close(&c);
}
TEST(cm12_unsorted_groups) {
    sf_grp g[] = { { 0x61, 0x62, 28 }, { 0x41, 0x42, 2 } }; sf_buf t = sf_cmap12(g, 2, 12);
    OPEN1(c, 3, 10, t, "f12 descending groups");
    CHECK(G(c, 0x61) == 28); PRED("descending group mapped", G(c, 0x41) == 2);
    t_note("0x41 -> %u", G(c, 0x41));
    cm_close(&c);
}
TEST(cm12_group_count_exceeds_table) {
    sf_grp g[] = { { 0x41, 0x42, 2 } }; sf_buf t = sf_cmap12(g, 1, 12); cmf c; sf_cmap_rec r;
    sf_w32(t.p + 12, 1000);         /* nGroups = 1000 */
    r.plat = 3; r.enc = 10; r.data = t.p; r.len = t.n;
    if (cm_open(&c, &r, 1, "f12 nGroups larger than the table")) { t_note("face %s", c.p.face ? "created" : "NOT created"); cm_close(&c); }
    sf_buf_free(&t);
}
TEST(cm12_zero_groups) {
    sf_buf t = sf_cmap12(NULL, 0, 12);
    OPEN1(c, 3, 10, t, "f12 zero groups");
    CHECK(G(c, 0x41) == 0);
    cm_close(&c);
}
TEST(cm12_under_unicode_platform_4) {
    sf_grp g[] = { { 0x41, 0x42, 2 } }; sf_buf t = sf_cmap12(g, 1, 12);
    OPEN1(c, 0, 4, t, "f12 under (0,4)");
    CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x42) == 3);
    cm_close(&c);
}
TEST(cm12_under_unicode_platform_6) {
    sf_grp g[] = { { 0x41, 0x42, 2 } }; sf_buf t = sf_cmap12(g, 1, 12);
    OPEN1(c, 0, 6, t, "f12 under (0,6)");
    CHECK(G(c, 0x41) == 2);
    cm_close(&c);
}
TEST(cm12_under_bmp_record_3_1) {
    sf_grp g[] = { { 0x41, 0x42, 2 }, { 0x1F600, 0x1F600, 9 } }; sf_buf t = sf_cmap12(g, 2, 12);
    OPEN1(c, 3, 1, t, "f12 under (3,1)");
    CHECK(G(c, 0x41) == 2); t_note("0x1F600 under (3,1) -> %u", G(c, 0x1F600));
    cm_close(&c);
}
TEST(cm13_many_to_one) {
    sf_grp g[] = { { 0x4E00, 0x4E10, 9 }, { 0x4E20, 0x4E21, 11 } }; sf_buf t = sf_cmap12(g, 2, 13);
    OPEN1(c, 3, 10, t, "f13 many-to-one");
    CHECK(G(c, 0x4E00) == 9); CHECK(G(c, 0x4E08) == 9); CHECK(G(c, 0x4E10) == 9); CHECK(G(c, 0x4E11) == 0); CHECK(G(c, 0x4E20) == 11); CHECK(G(c, 0x4E21) == 11);
    cm_close(&c);
}
TEST(cm13_glyph_limit) {
    sf_grp g[] = { { 0x4E00, 0x4E03, 200 } }; sf_buf t = sf_cmap12(g, 1, 13);
    OPEN1(c, 3, 10, t, "f13 glyph beyond the count");
    CHECK(G(c, 0x4E00) == 0);
    cm_close(&c);
}
TEST(cm14_uvs_only_font_maps_nothing) {
    /* format 14 records are not read for base-character lookup */
    sf_buf t = { 0, 0, 0 }; sf_b16(&t, 14); sf_b32(&t, 10); sf_b32(&t, 0);
    OPEN1(c, 0, 5, t, "f14 only (0,5)");
    CHECK(G(c, 0x41) == 0); CHECK(G(c, 0x20) == 0);
    cm_close(&c);
}
TEST(cm14_alongside_a_bmp_table) {
    sf_seg4g s[] = { { 0x41, 0x42, 2 - 0x41, NULL } }; sf_buf t4 = sf_cmap4(s, 1, 1), t14 = { 0, 0, 0 }; cmf c; sf_cmap_rec r[2];
    sf_b16(&t14, 14); sf_b32(&t14, 10); sf_b32(&t14, 0);
    r[0].plat = 3; r[0].enc = 1; r[0].data = t4.p; r[0].len = t4.n; r[1].plat = 0; r[1].enc = 5; r[1].data = t14.p; r[1].len = t14.n;
    if (cm_open(&c, r, 2, "f4 (3,1) + f14 (0,5)")) { if (c.p.face) { CHECK(G(c, 0x41) == 2); CHECK(G(c, 0x42) == 3); } cm_close(&c); }
    sf_buf_free(&t4); sf_buf_free(&t14);
}
TEST(cm14_nonsense_uvs_table) {
    sf_buf t = { 0, 0, 0 }; sf_b16(&t, 14); sf_b32(&t, 4000000000u); sf_b32(&t, 0xFFFFFFFF);
    sf_seg4g s[] = { { 0x41, 0x42, 2 - 0x41, NULL } }; sf_buf t4 = sf_cmap4(s, 1, 1); cmf c; sf_cmap_rec r[2];
    r[0].plat = 3; r[0].enc = 1; r[0].data = t4.p; r[0].len = t4.n; r[1].plat = 0; r[1].enc = 5; r[1].data = t.p; r[1].len = t.n;
    if (cm_open(&c, r, 2, "garbage f14 next to f4")) { if (c.p.face) CHECK(G(c, 0x41) == 2); cm_close(&c); }
    sf_buf_free(&t4); sf_buf_free(&t);
}

/* ---- format 2 --------------------------------------------------------------------------------------------------------- */
static sf_buf cmap2_single_bytes(unsigned first, unsigned count, const uint16_t *gids) {
    sf_buf o = { 0, 0, 0 }; unsigned i;
    sf_b16(&o, 2); sf_b16(&o, 6 + 512 + 8 + 2 * count); sf_b16(&o, 0);
    for (i = 0; i < 256; i++) sf_b16(&o, 0);                 /* every lead byte -> subheader 0 (single byte codes) */
    sf_b16(&o, first); sf_b16(&o, count); sf_b16(&o, 0); sf_b16(&o, 2);
    for (i = 0; i < count; i++) sf_b16(&o, gids[i]);
    return o;
}
TEST(cm2_single_byte_shift_jis) {
    static const uint16_t g[] = { 2, 3, 4 };
    sf_buf t = cmap2_single_bytes(0x41, 3, g);
    OPEN1(c, 3, 2, t, "f2 under (3,2) ShiftJIS");
    t_note("A=%u B=%u C=%u D=%u", G(c, 0x41), G(c, 0x42), G(c, 0x43), G(c, 0x44));
    PRED("single byte codes map", G(c, 0x41) == 2 && G(c, 0x43) == 4 && G(c, 0x44) == 0);
    cm_close(&c);
}
TEST(cm2_under_unicode_encoding_is_ignored) {
    static const uint16_t g[] = { 2, 3, 4 };
    sf_buf t = cmap2_single_bytes(0x41, 3, g);
    OPEN1(c, 3, 1, t, "f2 under (3,1)");
    CHECK(G(c, 0x41) == 0);
    cm_close(&c);
}
TEST(cm2_prc_big5_wansung_variants) {
    static const uint16_t g[] = { 2, 3, 4 }; unsigned enc[] = { 3, 4, 5 }; int i;
    for (i = 0; i < 3; i++) {
        sf_buf t = cmap2_single_bytes(0x41, 3, g); cmf c; sf_cmap_rec r; char l[64]; r.plat = 3; r.enc = enc[i]; r.data = t.p; r.len = t.n;
        snprintf(l, sizeof l, "f2 under (3,%u)", enc[i]);
        if (cm_open(&c, &r, 1, l)) { if (c.p.face) t_note("(3,%u) A=%u", enc[i], G(c, 0x41)); cm_close(&c); }
        sf_buf_free(&t);
    }
}

/* ---- encoding record selection -------------------------------------------------------------------------------------- */
static sf_buf one_a(unsigned gid) {          /* format 4: 'A' -> gid */
    sf_seg4g s[1]; s[0].start = 0x41; s[0].end = 0x41; s[0].delta = (int)gid - 0x41; s[0].garr = NULL; return sf_cmap4(s, 1, 1);
}
static sf_buf one_a12(unsigned gid) { sf_grp g; g.start = g.end = 0x41; g.gid = gid; return sf_cmap12(&g, 1, 12); }
static sf_buf mac_a(unsigned gid) { uint8_t m[256]; memset(m, 0, 256); m[0x41] = (uint8_t)gid; return sf_cmap0(m); }
typedef struct { unsigned plat, enc; int rank; const char *name; int fmt12; } enc_t;
static const enc_t ENCS[] = {
    { 0, 4, 9, "(0,4)", 1 }, { 3, 10, 9, "(3,10)", 1 }, { 0, 6, 9, "(0,6)", 1 }, { 3, 1, 8, "(3,1)", 0 },
    { 0, 3, 2, "(0,3)", 0 }, { 0, 0, 2, "(0,0)", 0 }, { 0, 2, 2, "(0,2)", 0 }, { 1, 0, 1, "(1,0)", 0 },
};
/* the winner for 'A' among two records, in both orders */
static void pair(const enc_t *a, const enc_t *b, unsigned ga, unsigned gb, int expect_first_listed_wins_tie) {
    sf_buf ta = a->fmt12 ? one_a12(ga) : (a->plat == 1 ? mac_a(ga) : one_a(ga));
    sf_buf tb = b->fmt12 ? one_a12(gb) : (b->plat == 1 ? mac_a(gb) : one_a(gb));
    int order; (void)expect_first_listed_wins_tie;
    for (order = 0; order < 2; order++) {
        cmf c; sf_cmap_rec r[2]; char l[96]; unsigned got, want;
        if (!order) { r[0].plat = a->plat; r[0].enc = a->enc; r[0].data = ta.p; r[0].len = ta.n; r[1].plat = b->plat; r[1].enc = b->enc; r[1].data = tb.p; r[1].len = tb.n; }
        else        { r[0].plat = b->plat; r[0].enc = b->enc; r[0].data = tb.p; r[0].len = tb.n; r[1].plat = a->plat; r[1].enc = a->enc; r[1].data = ta.p; r[1].len = ta.n; }
        if (a->rank > b->rank) want = ga; else if (a->rank < b->rank) want = gb; else want = order ? gb : ga;
        snprintf(l, sizeof l, "records %s,%s order %d", order ? b->name : a->name, order ? a->name : b->name, order);
        if (!cm_open(&c, r, 2, l)) continue;
        if (c.p.face) { got = G(c, 0x41); CHECKF(got == want, "%s: 'A' -> %u want %u", l, got, want); }
        cm_close(&c);
    }
    sf_buf_free(&ta); sf_buf_free(&tb);
}
TEST(cmsel_priority_ladder_pairs) {
    int i, j;
    for (i = 0; i < (int)(sizeof ENCS / sizeof ENCS[0]); i++) for (j = i + 1; j < (int)(sizeof ENCS / sizeof ENCS[0]); j++) pair(&ENCS[i], &ENCS[j], 10 + i, 30 + j, 1);
}
TEST(cmsel_3_10_beats_3_1) {
    sf_buf a = one_a(10), b = one_a12(20); cmf c; sf_cmap_rec r[2];
    r[0].plat = 3; r[0].enc = 1; r[0].data = a.p; r[0].len = a.n; r[1].plat = 3; r[1].enc = 10; r[1].data = b.p; r[1].len = b.n;
    if (cm_open(&c, r, 2, "(3,1) listed before (3,10)")) { if (c.p.face) CHECK(G(c, 0x41) == 20); cm_close(&c); }
    sf_buf_free(&a); sf_buf_free(&b);
}
TEST(cmsel_3_1_beats_unicode_platform_0) {
    sf_buf a = one_a(10), b = one_a(20); cmf c; sf_cmap_rec r[2];
    r[0].plat = 0; r[0].enc = 3; r[0].data = a.p; r[0].len = a.n; r[1].plat = 3; r[1].enc = 1; r[1].data = b.p; r[1].len = b.n;
    if (cm_open(&c, r, 2, "(0,3) before (3,1)")) { if (c.p.face) CHECK(G(c, 0x41) == 20); cm_close(&c); }
    sf_buf_free(&a); sf_buf_free(&b);
}
TEST(cmsel_unicode_platform_beats_mac) {
    sf_buf a = mac_a(10), b = one_a(20); cmf c; sf_cmap_rec r[2];
    r[0].plat = 1; r[0].enc = 0; r[0].data = a.p; r[0].len = a.n; r[1].plat = 0; r[1].enc = 3; r[1].data = b.p; r[1].len = b.n;
    if (cm_open(&c, r, 2, "(1,0) before (0,3)")) { if (c.p.face) CHECK(G(c, 0x41) == 20); cm_close(&c); }
    sf_buf_free(&a); sf_buf_free(&b);
}
TEST(cmsel_equal_rank_first_record_wins) {
    sf_buf a = one_a12(10), b = one_a12(20); cmf c; sf_cmap_rec r[3];
    r[0].plat = 3; r[0].enc = 10; r[0].data = a.p; r[0].len = a.n; r[1].plat = 0; r[1].enc = 4; r[1].data = b.p; r[1].len = b.n; r[2] = r[0];
    if (cm_open(&c, r, 2, "(3,10) then (0,4)")) { if (c.p.face) CHECK(G(c, 0x41) == 10); cm_close(&c); }
    r[0].plat = 0; r[0].enc = 4; r[0].data = b.p; r[0].len = b.n; r[1].plat = 3; r[1].enc = 10; r[1].data = a.p; r[1].len = a.n;
    if (cm_open(&c, r, 2, "(0,4) then (3,10)")) { if (c.p.face) CHECK(G(c, 0x41) == 20); cm_close(&c); }
    sf_buf_free(&a); sf_buf_free(&b);
}
TEST(cmsel_unusable_records_only) {
    sf_buf a = one_a(10); unsigned pl[] = { 3, 3, 2, 4, 1, 0xFFFF }, en[] = { 6, 7, 0, 0, 1, 0 }; int i;
    for (i = 0; i < 6; i++) {
        cmf c; sf_cmap_rec r; char l[64]; r.plat = pl[i]; r.enc = en[i]; r.data = a.p; r.len = a.n;
        snprintf(l, sizeof l, "only record (%u,%u)", pl[i], en[i]);
        if (cm_open(&c, &r, 1, l)) { if (c.p.face) CHECKF(G(c, 0x41) == 0, "(%u,%u) must not be used", pl[i], en[i]); cm_close(&c); }
    }
    sf_buf_free(&a);
}
TEST(cmsel_unusable_record_does_not_shadow_usable) {
    sf_buf a = one_a(10), b = one_a(20); cmf c; sf_cmap_rec r[2];
    r[0].plat = 3; r[0].enc = 7; r[0].data = a.p; r[0].len = a.n; r[1].plat = 0; r[1].enc = 3; r[1].data = b.p; r[1].len = b.n;
    if (cm_open(&c, r, 2, "(3,7) then (0,3)")) { if (c.p.face) CHECK(G(c, 0x41) == 20); cm_close(&c); }
    sf_buf_free(&a); sf_buf_free(&b);
}
TEST(cmsel_zero_records) {
    cmf c;
    if (cm_open(&c, &NOREC, 0, "cmap with zero encoding records")) { if (c.p.face) { CHECK(G(c, 0x41) == 0); CHECK(sf_gid(c.p.face, 0x20) == 0); } cm_close(&c); }
}
TEST(cmsel_symbol_encoding_is_symbol_font) {
    sf_seg4g s[] = { { 0xF041, 0xF043, 2 - 0xF041, NULL } }; sf_buf t = sf_cmap4(s, 1, 1);
    OPEN1(c, 3, 0, t, "(3,0) symbol cmap");
    CHECK(IDWriteFontFace_IsSymbolFont(c.p.face) == TRUE);
    CHECK(G(c, 0xF041) == 2); CHECK(G(c, 0xF043) == 4);
    t_note("symbol cmap: U+0041 -> %u (cross-populated from U+F041?)", G(c, 0x41));
    cm_close(&c);
}
TEST(cmsel_unicode_encoding_is_not_symbol_font) {
    sf_buf t = one_a(2);
    OPEN1(c, 3, 1, t, "(3,1) cmap");
    CHECK(IDWriteFontFace_IsSymbolFont(c.p.face) == FALSE);
    cm_close(&c);
}
TEST(cmsel_symbol_plus_unicode_is_not_symbol_font) {
    sf_seg4g s[] = { { 0xF041, 0xF043, 2 - 0xF041, NULL } }; sf_buf a = sf_cmap4(s, 1, 1), b = one_a(9); cmf c; sf_cmap_rec r[2];
    r[0].plat = 3; r[0].enc = 0; r[0].data = a.p; r[0].len = a.n; r[1].plat = 3; r[1].enc = 1; r[1].data = b.p; r[1].len = b.n;
    if (cm_open(&c, r, 2, "(3,0) + (3,1)")) { if (c.p.face) { PRED("not symbol when (3,1) exists", IDWriteFontFace_IsSymbolFont(c.p.face) == FALSE); CHECK(G(c, 0x41) == 9); } cm_close(&c); }
    sf_buf_free(&a); sf_buf_free(&b);
}
TEST(cmsel_cmap_version_ignored) {
    sf_buf t = one_a(2); cmf c; sf_cmap_rec r; r.plat = 3; r.enc = 1; r.data = t.p; r.len = t.n;
    if (cm_open(&c, &r, 1, "baseline")) { cm_close(&c); }
    {   /* patch cmap.version to 1 */
        ttf_spec s; sf_buf tab = sf_cmap_table(&r, 1); ttf_rawtable raw; sf_blob b; sf_probe p;
        sf_w16(tab.p, 1);
        ttf_default_spec(&s); memcpy(raw.tag, "cmap", 5); raw.data = tab.p; raw.len = (unsigned)tab.n; s.raw = &raw; s.nraw = 1; b = sf_build(&s);
        if (sf_open_face(&p, b.p, b.n)) { sf_survey("cmap version 1", &p); if (p.face) PRED("version ignored", sf_gid(p.face, 0x41) == 2); }
        sf_close(&p); sf_free(&b); sf_buf_free(&tab);
    }
    sf_buf_free(&t);
}
TEST(cmsel_record_offset_beyond_cmap) {
    sf_buf t = one_a(2); cmf c; sf_cmap_rec r; sf_buf tab; ttf_spec s; ttf_rawtable raw; sf_blob b; sf_probe p;
    r.plat = 3; r.enc = 1; r.data = t.p; r.len = t.n; tab = sf_cmap_table(&r, 1);
    sf_w32(tab.p + 8, 0x10000);          /* subtable offset past the end of the cmap table */
    ttf_default_spec(&s); memcpy(raw.tag, "cmap", 5); raw.data = tab.p; raw.len = (unsigned)tab.n; s.raw = &raw; s.nraw = 1; b = sf_build(&s);
    if (sf_open_face(&p, b.p, b.n)) { sf_survey("cmap record offset 0x10000", &p); if (p.face) t_note("A -> %u", sf_gid(p.face, 0x41)); }
    sf_close(&p); sf_free(&b); sf_buf_free(&tab); sf_buf_free(&t); (void)c;
}
TEST(cmsel_numrecords_larger_than_table) {
    sf_buf t = one_a(2); sf_cmap_rec r; sf_buf tab; ttf_spec s; ttf_rawtable raw; sf_blob b; sf_probe p;
    r.plat = 3; r.enc = 1; r.data = t.p; r.len = t.n; tab = sf_cmap_table(&r, 1);
    sf_w16(tab.p + 2, 5000);
    ttf_default_spec(&s); memcpy(raw.tag, "cmap", 5); raw.data = tab.p; raw.len = (unsigned)tab.n; s.raw = &raw; s.nraw = 1; b = sf_build(&s);
    if (sf_open_face(&p, b.p, b.n)) { sf_survey("cmap numTables 5000", &p); }
    sf_close(&p); sf_free(&b); sf_buf_free(&tab); sf_buf_free(&t);
}
TEST(cmsel_cmap_shorter_than_header) {
    ttf_spec s; ttf_rawtable raw; sf_blob b; sf_probe p; static const uint8_t two[2] = { 0, 0 };
    ttf_default_spec(&s); memcpy(raw.tag, "cmap", 5); raw.data = two; raw.len = 2; s.raw = &raw; s.nraw = 1; b = sf_build(&s);
    if (sf_open_face(&p, b.p, b.n)) { sf_survey("cmap table of 2 bytes", &p); }
    sf_close(&p); sf_free(&b);
}

/* ---- GetGlyphIndices argument handling -------------------------------------------------------------------------------- */
TEST(cmapi_unmapped_and_out_of_range) {
    cmf c; sf_blob b = sf_default(); sf_probe p;
    (void)c;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face) {
        UINT32 cp[] = { 'A', 0, 0x20, 0x7F, 0xD800, 0xDFFF, 0xFFFF, 0x10FFFF, 0x110000, 0x7FFFFFFF, 0xFFFFFFFF, 'z' };
        UINT16 g[12]; int i;
        for (i = 0; i < 12; i++) g[i] = 0xABAB;
        CHECK_HR(IDWriteFontFace_GetGlyphIndices(p.face, cp, 12, g), S_OK);
        CHECK(g[0] == 2); CHECK(g[1] == 0); CHECK(g[2] == 1);
        for (i = 3; i < 11; i++) CHECKF(g[i] == 0, "cp %08x -> %u", cp[i], g[i]);
        CHECK(g[11] == 53);
    }
    sf_close(&p); sf_free(&b);
}
TEST(cmapi_zero_count) {
    sf_blob b = sf_default(); sf_probe p; UINT16 g = 0xABAB; UINT32 cp = 'A';
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face) { CHECK_HR(IDWriteFontFace_GetGlyphIndices(p.face, &cp, 0, &g), S_OK); CHECK(g == 0xABAB); }
    sf_close(&p); sf_free(&b);
}
TEST(cmapi_null_codepoints_with_count) {
    sf_blob b = sf_default(); sf_probe p; UINT16 g[3] = { 0xABAB, 0xABAB, 0xABAB };
    if (t_is_wine()) { t_skip("Wine dereferences NULL"); sf_free(&b); return; }
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face) {
        CHECK_HR(IDWriteFontFace_GetGlyphIndices(p.face, NULL, 3, g), E_INVALIDARG);
        t_note("outputs after the failed call: %04x %04x %04x (decomp: zeroed before the argument check)", g[0], g[1], g[2]);
        PRED("outputs zeroed", g[0] == 0 && g[1] == 0 && g[2] == 0);
    }
    sf_close(&p); sf_free(&b);
}
TEST(cmapi_zero_count_null_codepoints) {
    sf_blob b = sf_default(); sf_probe p; UINT16 g = 1;
    if (t_is_wine()) { t_skip("Wine dereferences NULL"); sf_free(&b); return; }
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face) CHECK_HR(IDWriteFontFace_GetGlyphIndices(p.face, NULL, 0, &g), S_OK);
    sf_close(&p); sf_free(&b);
}
TEST(cmapi_large_batch) {
    sf_blob b = sf_default(); sf_probe p; enum { N = 5000 }; UINT32 *cp = (UINT32 *)malloc(N * 4); UINT16 *g = (UINT16 *)malloc(N * 2); int i, bad = 0;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); free(cp); free(g); return; }
    for (i = 0; i < N; i++) cp[i] = (UINT32)(i * 7) % 0x300;
    if (p.face) {
        CHECK_HR(IDWriteFontFace_GetGlyphIndices(p.face, cp, N, g), S_OK);
        for (i = 0; i < N; i++) if (g[i] != ttf_default_gid(cp[i])) bad++;
        CHECKF(bad == 0, "%d wrong lookups in a batch of %d", bad, N);
    }
    sf_close(&p); sf_free(&b); free(cp); free(g);
}
TEST(cmapi_default_font_full_ascii) {
    sf_blob b = sf_default(); sf_probe p; UINT32 cp; int bad = 0;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face) { for (cp = 0; cp < 0x200; cp++) if (sf_gid(p.face, cp) != ttf_default_gid(cp)) bad++; CHECKF(bad == 0, "%d wrong lookups in 0..0x1FF", bad); }
    sf_close(&p); sf_free(&b);
}
