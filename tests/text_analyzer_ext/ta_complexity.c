/* IDWriteTextAnalyzer1::GetTextComplexity.
 *
 * Windows 10 (DWriteTextAnalyzer::GetTextComplexity + FontFace::GetSimpleGlyphs):
 *   - *isTextSimple and *textLengthRead are cleared first (their pointers must be valid);
 *   - fontFace == NULL, or textString == NULL with textLength != 0 -> E_INVALIDARG (outputs stay cleared, glyphIndices untouched);
 *   - otherwise S_OK. The method inspects the LEADING run of the text: the characters are classified one UTF-16 unit at a time as
 *     "simple" (mapped by the font's cmap and not requiring shaping) or "complex"; textLengthRead is the length of the leading
 *     run of one class and isTextSimple tells which class it is. A zero length text reads zero characters and is "simple".
 *   - glyphIndices (optional, may be NULL) receives the cmap glyph for each simple character read and 0 for complex ones; it is
 *     never written beyond textLengthRead entries.
 *   - UTF-16 surrogate units are never simple (the lookup is per UTF-16 unit).
 */
#include "ta.h"

#define CANARY 0xBEEF
static HRESULT run(IDWriteTextAnalyzer1 *a1, const WCHAR *t, UINT32 n, IDWriteFontFace *face, BOOL *simple, UINT32 *read, UINT16 *g) {
    return IDWriteTextAnalyzer1_GetTextComplexity(a1, t, n, face, simple, read, g);
}
static void fill(UINT16 *g, int n) { int i; for (i = 0; i < n; i++) g[i] = CANARY; }

#define NEED_FONT() \
    dwx_font hf; if (FAILED(dwx_font_open_default(&hf, f)) || !hf.face) { dwx_font_close(&hf); TA_END(); t_skip("synthetic font unavailable"); return; } \
    IDWriteFontFace *face = hf.face

TEST(tc_null_face_invalid_outputs_cleared) {
    TA_BEGIN(); BOOL simple = 1; UINT32 read = 77; UINT16 g[4]; fill(g, 4);
    CHECK_HR(run(a1, L"ABC", 3, NULL, &simple, &read, g), TA_E_INVALIDARG);
    EXPECT_EQ(simple, 0); EXPECT_EQ(read, 0); EXPECT_EQ(g[0], CANARY);
    TA_END();
}
TEST(tc_null_face_zero_length_still_invalid) {
    TA_BEGIN(); BOOL simple = 1; UINT32 read = 77;
    CHECK_HR(run(a1, NULL, 0, NULL, &simple, &read, NULL), TA_E_INVALIDARG);
    EXPECT_EQ(simple, 0); EXPECT_EQ(read, 0);
    TA_END();
}
TEST(tc_null_text_with_length_invalid) {
    TA_BEGIN(); NEED_FONT(); BOOL simple = 1; UINT32 read = 77; UINT16 g[4]; fill(g, 4);
    CHECK_HR(run(a1, NULL, 3, face, &simple, &read, g), TA_E_INVALIDARG);
    EXPECT_EQ(simple, 0); EXPECT_EQ(read, 0); EXPECT_EQ(g[0], CANARY);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_null_text_zero_length_ok) {
    TA_BEGIN(); NEED_FONT(); BOOL simple = 0; UINT32 read = 77; UINT16 g[2]; fill(g, 2);
    CHECK_HR(run(a1, NULL, 0, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 0); EXPECT_EQ(g[0], CANARY);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_empty_text_is_simple_and_reads_nothing) {
    TA_BEGIN(); NEED_FONT(); BOOL simple = 0; UINT32 read = 77;
    CHECK_HR(run(a1, L"ABC", 0, face, &simple, &read, NULL), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 0);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_ascii_text_is_simple_with_cmap_glyphs) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR t[] = L"Hello World 0123456789"; UINT32 n = (UINT32)lstrlenW(t), i; BOOL simple = 0; UINT32 read = 0; UINT16 g[64]; fill(g, 64);
    CHECK_HR(run(a1, t, n, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, n);
    for (i = 0; i < n; i++) CHECKF(g[i] == ttf_default_gid(t[i]), "char %u '%c': glyph %u want %u", i, t[i], g[i], ttf_default_gid(t[i]));
    EXPECT_EQ(g[n], CANARY);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_every_mapped_ascii_char_alone_is_simple) {
    TA_BEGIN(); NEED_FONT(); WCHAR c; int bad = 0;
    for (c = 0x20; c < 0x7F; c++) {
        unsigned want = ttf_default_gid(c); BOOL simple = 3; UINT32 read = 9; UINT16 g = CANARY;
        if (!want) continue;
        if (run(a1, &c, 1, face, &simple, &read, &g) != S_OK || simple != 1 || read != 1 || g != want) { bad++; t_check(0, __FILE__, __LINE__, "simple", "U+%04X: simple %d read %u glyph %u want %u", c, simple, read, g, want); }
    }
    EXPECT_EQ(bad, 0);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_glyph_array_optional) {
    TA_BEGIN(); NEED_FONT(); BOOL simple = 0; UINT32 read = 0;
    CHECK_HR(run(a1, L"ABC", 3, face, &simple, &read, NULL), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 3);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_length_limits_the_read) {
    TA_BEGIN(); NEED_FONT(); BOOL simple = 0; UINT32 read = 0; UINT16 g[8]; fill(g, 8);
    CHECK_HR(run(a1, L"ABCDEF", 3, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 3); EXPECT_EQ(g[2], ttf_default_gid('C')); EXPECT_EQ(g[3], CANARY);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_unmapped_first_char_is_complex) {
    TA_BEGIN(); NEED_FONT(); BOOL simple = 1; UINT32 read = 0; UINT16 g[4]; fill(g, 4);
    CHECK_HR(run(a1, L"\x4E00", 1, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 0); EXPECT_EQ(read, 1); EXPECT_EQ(g[0], 0);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_simple_run_stops_before_complex_char) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR t[] = { 'A', 'B', 0x4E00, 'C' }; BOOL simple = 0; UINT32 read = 0; UINT16 g[8]; fill(g, 8);
    CHECK_HR(run(a1, t, 4, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 2); EXPECT_EQ(g[0], ttf_default_gid('A')); EXPECT_EQ(g[1], ttf_default_gid('B'));
    EXPECT_EQ(g[2], CANARY);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_complex_run_stops_before_simple_char) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR t[] = { 0x4E00, 'A', 'B' }; BOOL simple = 1; UINT32 read = 0; UINT16 g[8]; fill(g, 8);
    CHECK_HR(run(a1, t, 3, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 0); EXPECT_EQ(read, 1); EXPECT_EQ(g[0], 0); EXPECT_EQ(g[1], CANARY);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_complex_run_of_several_chars) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR t[] = { 0x4E00, 0x4E01, 0x4E02, 'A' }; BOOL simple = 1; UINT32 read = 0; UINT16 g[8]; fill(g, 8);
    CHECK_HR(run(a1, t, 4, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 0); EXPECT_EQ(read, 3);
    EXPECT_EQ(g[0], 0); EXPECT_EQ(g[1], 0); EXPECT_EQ(g[2], 0);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_alternating_classes_read_only_leading_run) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR t[] = { 'A', 0x4E00, 'B', 0x4E01, 'C' }; BOOL simple = 0; UINT32 read = 0;
    CHECK_HR(run(a1, t, 5, face, &simple, &read, NULL), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 1);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_surrogate_pair_alone_is_complex) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR t[] = { 0xD83D, 0xDE00 }; BOOL simple = 1; UINT32 read = 0; UINT16 g[4]; fill(g, 4);
    CHECK_HR(run(a1, t, 2, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 0); CHECK(read >= 1 && read <= 2); EXPECT_EQ(g[0], 0);
    t_note("emoji pair alone: simple=%d read=%u", simple, read);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_surrogates_after_simple_text) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR t[] = { 'A', 'B', 0xD83D, 0xDE00, 'C' }; BOOL simple = 0; UINT32 read = 0;
    CHECK_HR(run(a1, t, 5, face, &simple, &read, NULL), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 2);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_lone_surrogates_are_complex) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR hi[] = { 0xD800 }, lo[] = { 0xDC00 }; BOOL simple = 1; UINT32 read = 0;
    CHECK_HR(run(a1, hi, 1, face, &simple, &read, NULL), S_OK); EXPECT_EQ(simple, 0); EXPECT_EQ(read, 1);
    simple = 1; read = 0;
    CHECK_HR(run(a1, lo, 1, face, &simple, &read, NULL), S_OK); EXPECT_EQ(simple, 0); EXPECT_EQ(read, 1);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_control_characters_unmapped_are_complex) {
    TA_BEGIN(); NEED_FONT(); static const WCHAR cs[] = { 0x0000, 0x0009, 0x000A, 0x000D, 0x001F }; unsigned i;
    for (i = 0; i < sizeof cs / sizeof *cs; i++) {
        BOOL simple = 1; UINT32 read = 0; UINT16 g = CANARY;
        CHECK_HR(run(a1, &cs[i], 1, face, &simple, &read, &g), S_OK);
        EXPECT_EQ(simple, 0); EXPECT_EQ(read, 1); EXPECT_EQ(g, 0);
    }
    dwx_font_close(&hf); TA_END();
}
TEST(tc_custom_cmap_changes_glyphs) {
    /* the glyph indices come from the face passed in */
    TA_BEGIN(); ttf_spec s; int ng, nc; static const ttf_map map[] = { { ' ', 1 }, { 'A', 3 }, { 'B', 2 } }; static ttf_cmap cm[1];
    ttf_default_spec(&s); (void)ng; (void)nc;
    cm[0].format = 4; cm[0].plat = 3; cm[0].enc = 1; cm[0].n = 3; cm[0].map = map; cm[0].nuvs = 0; cm[0].uvs = NULL;
    s.cmaps = cm; s.ncmaps = 1;
    dwx_font h2; if (FAILED(ta_open_spec(&h2, f, &s)) || !h2.face) { dwx_font_close(&h2); TA_END(); t_skip("custom font unavailable"); return; }
    BOOL simple = 0; UINT32 read = 0; UINT16 g[4]; fill(g, 4);
    CHECK_HR(run(a1, L"AB ", 3, h2.face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 3); EXPECT_EQ(g[0], 3); EXPECT_EQ(g[1], 2); EXPECT_EQ(g[2], 1);
    simple = 1; read = 0;
    CHECK_HR(run(a1, L"C", 1, h2.face, &simple, &read, g), S_OK);      /* 'C' is not in this cmap */
    EXPECT_EQ(simple, 0); EXPECT_EQ(read, 1);
    dwx_font_close(&h2); TA_END();
}
TEST(tc_long_text_1000_chars) {
    TA_BEGIN(); NEED_FONT(); WCHAR *t = (WCHAR *)malloc(1001 * sizeof(WCHAR)); UINT16 *g = (UINT16 *)malloc(1002 * sizeof(UINT16)); UINT32 i; BOOL simple = 0; UINT32 read = 0;
    for (i = 0; i < 1000; i++) t[i] = (WCHAR)('A' + i % 26);
    t[1000] = 0; g[1000] = CANARY; g[1001] = CANARY;
    CHECK_HR(run(a1, t, 1000, face, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 1000); EXPECT_EQ(g[1000], CANARY);
    for (i = 0; i < 1000; i++) if (g[i] != ttf_default_gid(t[i])) { CHECK(0); break; }
    free(t); free(g); dwx_font_close(&hf); TA_END();
}
TEST(tc_repeatable) {
    TA_BEGIN(); NEED_FONT(); BOOL s1, s2; UINT32 r1, r2; UINT16 g1[8], g2[8];
    run(a1, L"ABC", 3, face, &s1, &r1, g1); run(a1, L"ABC", 3, face, &s2, &r2, g2);
    CHECK(s1 == s2 && r1 == r2 && !memcmp(g1, g2, 3 * sizeof(UINT16)));
    dwx_font_close(&hf); TA_END();
}
TEST(tc_face_not_released_or_retained) {
    TA_BEGIN(); NEED_FONT(); BOOL simple; UINT32 read; ULONG r0 = dw_refcount((IUnknown *)face), r1;
    run(a1, L"ABC", 3, face, &simple, &read, NULL);
    r1 = dw_refcount((IUnknown *)face); EXPECT_EQ(r1, r0);
    dwx_font_close(&hf); TA_END();
}
TEST(tc_through_analyzer2_interface) {
    TA_BEGIN(); NEED_FONT(); BOOL simple = 0; UINT32 read = 0;
    CHECK_HR(IDWriteTextAnalyzer2_GetTextComplexity(a2, L"AB", 2, face, &simple, &read, NULL), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 2);
    dwx_font_close(&hf); TA_END();
}

/* ---- Arial relations ---------------------------------------------------------------------------------------------------- */
#define NEED_ARIAL() \
    IDWriteFontFace *arial = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); \
    if (!arial) { TA_END(); t_skip("Arial not installed"); return; }

TEST(tc_arial_latin_simple_matches_getglyphindices) {
    TA_BEGIN(); NEED_ARIAL(); static const WCHAR t[] = L"The quick brown fox 0123456789"; UINT32 n = (UINT32)lstrlenW(t), i; BOOL simple = 0; UINT32 read = 0; UINT16 g[64], ref[64]; fill(g, 64);
    CHECK_HR(run(a1, t, n, arial, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, n);
    UINT32 cps[64]; for (i = 0; i < n; i++) cps[i] = t[i];
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(arial, cps, n, ref), S_OK);
    for (i = 0; i < n; i++) CHECKF(g[i] == ref[i] && g[i] != 0, "char %u: %u vs %u", i, g[i], ref[i]);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_arabic_is_complex) {
    TA_BEGIN(); NEED_ARIAL(); static const WCHAR t[] = { 0x0627, 0x0644, 0x0639, 0x0631, 0x0628, 0x064A, 0x0629 }; BOOL simple = 1; UINT32 read = 0; UINT16 g[8]; fill(g, 8);
    CHECK_HR(run(a1, t, 7, arial, &simple, &read, g), S_OK);
    EXPECT_EQ(simple, 0); CHECK(read >= 1 && read <= 7);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_hebrew_note) {
    TA_BEGIN(); NEED_ARIAL(); static const WCHAR t[] = { 0x05E9, 0x05DC, 0x05D5, 0x05DD }; BOOL simple = 1; UINT32 read = 0;
    CHECK_HR(run(a1, t, 4, arial, &simple, &read, NULL), S_OK);
    t_note("Hebrew in Arial: simple=%d read=%u", simple, read);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_devanagari_note) {
    TA_BEGIN(); NEED_ARIAL(); static const WCHAR t[] = { 0x0928, 0x092E, 0x0938, 0x094D, 0x0924, 0x0947 }; BOOL simple = 1; UINT32 read = 0;
    CHECK_HR(run(a1, t, 6, arial, &simple, &read, NULL), S_OK);
    EXPECT_EQ(simple, 0);                         /* the font has no Devanagari glyphs at all */
    t_note("Devanagari in Arial: read=%u", read);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_combining_mark_after_latin_note) {
    TA_BEGIN(); NEED_ARIAL(); static const WCHAR t[] = { 'e', 0x0301, 'a' }; BOOL simple = 1; UINT32 read = 9; UINT16 g[4]; fill(g, 4);
    CHECK_HR(run(a1, t, 3, arial, &simple, &read, g), S_OK);
    CHECK(read <= 3);
    t_note("e + U+0301 + a in Arial: simple=%d read=%u glyphs %u %u %u", simple, read, g[0], g[1], g[2]);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_combining_mark_alone_note) {
    TA_BEGIN(); NEED_ARIAL(); static const WCHAR t[] = { 0x0301 }; BOOL simple = 1; UINT32 read = 9;
    CHECK_HR(run(a1, t, 1, arial, &simple, &read, NULL), S_OK);
    t_note("U+0301 alone in Arial: simple=%d read=%u", simple, read);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_bidi_controls_note) {
    TA_BEGIN(); NEED_ARIAL(); static const WCHAR cs[] = { 0x200E, 0x200F, 0x202A, 0x202B, 0x202C, 0x202D, 0x202E, 0x2066, 0x2067, 0x2068, 0x2069, 0x200D, 0x200C }; unsigned i;
    for (i = 0; i < sizeof cs / sizeof *cs; i++) {
        BOOL simple = 9; UINT32 read = 9; CHECK_HR(run(a1, &cs[i], 1, arial, &simple, &read, NULL), S_OK);
        t_note("U+%04X alone in Arial: simple=%d read=%u", cs[i], simple, read);
    }
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_ligature_candidates_note) {
    TA_BEGIN(); NEED_ARIAL(); BOOL simple = 0; UINT32 read = 0;
    CHECK_HR(run(a1, L"fi ffl", 6, arial, &simple, &read, NULL), S_OK);
    t_note("\"fi ffl\" in Arial: simple=%d read=%u", simple, read);
    CHECK(read >= 1 && read <= 6);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_cjk_missing_glyph_is_complex_or_mapped) {
    TA_BEGIN(); NEED_ARIAL(); BOOL simple = 0; UINT32 read = 0; UINT16 g = CANARY;
    CHECK_HR(run(a1, L"\x4E00", 1, arial, &simple, &read, &g), S_OK);
    EXPECT_EQ(read, 1);                           /* Arial has no CJK glyph: reported as a complex run of one */
    if (simple) t_note("Arial maps U+4E00 (glyph %u)", g); else EXPECT_EQ(g, 0);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(tc_arial_space_and_punctuation_are_simple) {
    TA_BEGIN(); NEED_ARIAL(); BOOL simple = 0; UINT32 read = 0;
    CHECK_HR(run(a1, L" .,;:!?()-", 10, arial, &simple, &read, NULL), S_OK);
    EXPECT_EQ(simple, 1); EXPECT_EQ(read, 10);
    IDWriteFontFace_Release(arial); TA_END();
}
