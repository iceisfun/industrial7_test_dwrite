/* IDWriteTextAnalyzer1::GetTextComplexity with synthetic cmaps.
 * Windows 10 (IDWriteTextAnalyzer::GetTextComplexity -> GetSimpleGlyphs / GetGlyphIndexIfSimple):
 *   - isTextSimple and textLengthRead are cleared first; fontFace NULL or (text NULL && length != 0) -> E_INVALIDARG.
 *   - Each UTF-16 code unit is looked up through the font's cmap; the glyph is accepted only when the character is not flagged by
 *     the Unicode property table 8 and the glyph id is below the font's glyph count (and is not 0).
 *   - The result describes the leading run: all simple (isTextSimple TRUE) or all complex (FALSE).
 */
#include "r2.h"

typedef struct cx { r2_ctx c; dwx_font h; ttf_map *map; int ok; } cx;
static int cx_open(cx *x, const ttf_map *m, int n, int fmt, unsigned plat, unsigned enc, int nglyphs) {
    ttf_spec s; ttf_cmap cm; uint8_t *blob; size_t sz; HRESULT hr;
    static ttf_glyph gl[1024];
    memset(x, 0, sizeof *x);
    if (!r2_open(&x->c)) return 0;
    ttf_default_spec(&s);
    x->map = (ttf_map *)malloc((n + 1) * sizeof *m); memcpy(x->map, m, n * sizeof *m);
    memset(&cm, 0, sizeof cm); cm.format = fmt; cm.plat = (unsigned short)plat; cm.enc = (unsigned short)enc; cm.n = n; cm.map = x->map;
    s.cmaps = &cm; s.ncmaps = 1;
    if (nglyphs > TTF_DEF_NGLYPHS) {
        int nd, i; const ttf_glyph *d = ttf_default_glyphs(&nd);
        memcpy(gl, d, nd * sizeof *d);
        for (i = nd; i < nglyphs; i++) { memset(&gl[i], 0, sizeof gl[i]); gl[i].adv = 1000; }
        s.glyphs = gl; s.nglyphs = nglyphs;
    }
    blob = ttf_build(&s, &sz);
    if (!blob) return 0;
    hr = dwx_font_open(&x->h, x->c.f, blob, sz, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(blob);
    x->ok = SUCCEEDED(hr) && x->h.face;
    return x->ok;
}
static void cx_close(cx *x) { dwx_font_close(&x->h); free(x->map); r2_close(&x->c); }
#define CX_OPEN(x, m, n, fmt, plat, enc, ng) cx x; if (!cx_open(&x, m, n, fmt, plat, enc, ng)) { cx_close(&x); t_skip("synthetic font unavailable"); return; }
/* Query helper: returns S_OK, fills simple / read / glyphs (glyph array of len entries, sentinel 0x7777 pre-filled) */
static HRESULT cx_query(cx *x, const WCHAR *t, UINT32 len, BOOL *simple, UINT32 *read, UINT16 *gl) {
    UINT32 i;
    if (gl) for (i = 0; i < len; i++) gl[i] = 0x7777;
    *simple = 0x55; *read = 0x55555555;
    return IDWriteTextAnalyzer1_GetTextComplexity(x->c.a1, t, len, x->h.face, simple, read, gl);
}
static const ttf_map k_ascii[] = { { 'A', 2 }, { 'B', 3 }, { 'C', 4 }, { 'D', 5 }, { 'a', 28 }, { 'b', 29 } };

RT(cx_glyph_below_glyph_count_is_simple) {
    ttf_map m[] = { { 'A', 68 } }; BOOL s; UINT32 r; UINT16 g[1];
    CX_OPEN(x, m, 1, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"A", 1, &s, &r, g), S_OK);
    EXPECT_EQ(s, 1); EXPECT_EQ(r, 1); EXPECT_EQ(g[0], 68);
    cx_close(&x);
}
RT(cx_glyph_at_glyph_count_is_complex) {
    /* glyph index must be < maxp.numGlyphs (69 in the default font) */
    ttf_map m[] = { { 'A', 69 } }; BOOL s; UINT32 r; UINT16 g[1];
    CX_OPEN(x, m, 1, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"A", 1, &s, &r, g), S_OK);
    EXPECT_EQ(s, 0); EXPECT_EQ(r, 1); EXPECT_EQ(g[0], 0);
    cx_close(&x);
}
RT(cx_glyph_far_beyond_glyph_count_is_complex) {
    ttf_map m[] = { { 'A', 40000 }, { 'B', 3 } }; BOOL s; UINT32 r; UINT16 g[2];
    CX_OPEN(x, m, 2, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"A", 1, &s, &r, g), S_OK); EXPECT_EQ(s, 0); EXPECT_EQ(g[0], 0);
    CHECK_HR(cx_query(&x, L"BA", 2, &s, &r, g), S_OK); EXPECT_EQ(s, 1); EXPECT_EQ(r, 1); EXPECT_EQ(g[0], 3);
    cx_close(&x);
}
RT(cx_larger_font_accepts_larger_glyph_ids) {
    ttf_map m[] = { { 'A', 69 }, { 'B', 299 }, { 'C', 300 } }; BOOL s; UINT32 r; UINT16 g[3];
    CX_OPEN(x, m, 3, 4, 3, 1, 300);
    CHECK_HR(cx_query(&x, L"AB", 2, &s, &r, g), S_OK); EXPECT_EQ(s, 1); EXPECT_EQ(r, 2); EXPECT_EQ(g[0], 69); EXPECT_EQ(g[1], 299);
    CHECK_HR(cx_query(&x, L"C", 1, &s, &r, g), S_OK); EXPECT_EQ(s, 0); EXPECT_EQ(g[0], 0);
    cx_close(&x);
}
RT(cx_mapping_to_notdef_is_complex) {
    ttf_map m[] = { { 'A', 0 }, { 'B', 3 } }; BOOL s; UINT32 r; UINT16 g[2];
    CX_OPEN(x, m, 2, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"A", 1, &s, &r, g), S_OK); EXPECT_EQ(s, 0); EXPECT_EQ(r, 1); EXPECT_EQ(g[0], 0);
    CHECK_HR(cx_query(&x, L"BA", 2, &s, &r, g), S_OK); EXPECT_EQ(s, 1); EXPECT_EQ(r, 1);
    cx_close(&x);
}
RT(cx_cmap_format4_platform3_enc1) {
    BOOL s; UINT32 r; UINT16 g[4];
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"ABab", 4, &s, &r, g), S_OK); EXPECT_EQ(s, 1); EXPECT_EQ(r, 4);
    EXPECT_EQ(g[0], 2); EXPECT_EQ(g[1], 3); EXPECT_EQ(g[2], 28); EXPECT_EQ(g[3], 29);
    cx_close(&x);
}
RT(cx_cmap_format12_platform3_enc10) {
    BOOL s; UINT32 r; UINT16 g[4];
    CX_OPEN(x, k_ascii, 6, 12, 3, 10, 0);
    CHECK_HR(cx_query(&x, L"ABab", 4, &s, &r, g), S_OK); EXPECT_EQ(s, 1); EXPECT_EQ(r, 4);
    EXPECT_EQ(g[0], 2); EXPECT_EQ(g[2], 28);
    cx_close(&x);
}
RT(cx_cmap_other_encodings_note) {
    static const struct { int fmt; unsigned plat, enc; const char *n; } k[] = {
        { 4, 0, 3, "format 4 (0,3)" }, { 12, 0, 4, "format 12 (0,4)" }, { 4, 0, 0, "format 4 (0,0)" }, { 6, 3, 1, "format 6 (3,1)" }, { 0, 1, 0, "format 0 (1,0)" },
        { 4, 3, 0, "format 4 symbol (3,0)" }, { 13, 3, 10, "format 13 (3,10)" },
    };
    unsigned i;
    for (i = 0; i < sizeof k / sizeof *k; i++) {
        cx x; BOOL s; UINT32 r; UINT16 g[4];
        if (!cx_open(&x, k_ascii, 6, k[i].fmt, k[i].plat, k[i].enc, 0)) { cx_close(&x); t_note("%s: font rejected", k[i].n); continue; }
        HRESULT hr = cx_query(&x, L"ABab", 4, &s, &r, g);
        CHECK_HR(hr, S_OK);
        t_note("%s: simple=%d read=%u glyphs=%u %u %u %u", k[i].n, s, r, g[0], g[1], g[2], g[3]);
        cx_close(&x);
    }
}
RT(cx_read_count_stops_at_first_unmapped_char) {
    BOOL s; UINT32 r; UINT16 g[6];
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"ABzAB", 5, &s, &r, g), S_OK);
    EXPECT_EQ(s, 1); EXPECT_EQ(r, 2); EXPECT_EQ(g[0], 2); EXPECT_EQ(g[1], 3);
    CHECK_HR(cx_query(&x, L"zzAB", 4, &s, &r, g), S_OK);
    EXPECT_EQ(s, 0); EXPECT_EQ(r, 2); EXPECT_EQ(g[0], 0); EXPECT_EQ(g[1], 0);
    cx_close(&x);
}
RT(cx_complex_run_then_simple_run) {
    BOOL s; UINT32 r; UINT16 g[8]; unsigned k;
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    static const WCHAR *t[] = { L"zAB", L"zzAB", L"zzzAB", L"zzzzzzzAB" };
    for (k = 0; k < 4; k++) {
        UINT32 len = (UINT32)lstrlenW(t[k]);
        CHECK_HR(cx_query(&x, t[k], len, &s, &r, g), S_OK);
        CHECKF(s == 0 && r == len - 2, "case %u: simple %d read %u", k, s, r);
    }
    cx_close(&x);
}
RT(cx_alternating_runs_report_only_the_first) {
    BOOL s; UINT32 r; UINT16 g[8];
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"AzAzA", 5, &s, &r, g), S_OK); EXPECT_EQ(s, 1); EXPECT_EQ(r, 1);
    CHECK_HR(cx_query(&x, L"zAzAz", 5, &s, &r, g), S_OK); EXPECT_EQ(s, 0); EXPECT_EQ(r, 1);
    CHECK_HR(cx_query(&x, L"ABzzAB", 6, &s, &r, g), S_OK); EXPECT_EQ(s, 1); EXPECT_EQ(r, 2);
    CHECK_HR(cx_query(&x, L"zzABzz", 6, &s, &r, g), S_OK); EXPECT_EQ(s, 0); EXPECT_EQ(r, 2);
    cx_close(&x);
}
RT(cx_glyph_array_written_only_for_read_prefix) {
    BOOL s; UINT32 r; UINT16 g[8];
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"ABzAB", 5, &s, &r, g), S_OK);
    EXPECT_EQ(r, 2); EXPECT_EQ(g[3], 0x7777); EXPECT_EQ(g[4], 0x7777);
    cx_close(&x);
}
RT(cx_null_glyph_array_still_reports) {
    BOOL s = 7; UINT32 r = 7;
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(x.c.a1, L"ABzAB", 5, x.h.face, &s, &r, NULL), S_OK);
    EXPECT_EQ(s, 1); EXPECT_EQ(r, 2);
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(x.c.a1, L"zAB", 3, x.h.face, &s, &r, NULL), S_OK);
    EXPECT_EQ(s, 0); EXPECT_EQ(r, 1);
    cx_close(&x);
}
RT(cx_length_prefix_limits_the_scan) {
    BOOL s; UINT32 r; UINT16 g[8]; UINT32 len;
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    for (len = 0; len <= 4; len++) {
        CHECK_HR(cx_query(&x, L"ABabz", len, &s, &r, g), S_OK);
        CHECKF(r == len, "len %u: read %u", len, r);
        if (len) CHECKF(s == 1, "len %u: simple %d", len, s);
    }
    cx_close(&x);
}
RT(cx_empty_text_results) {
    BOOL s = 7; UINT32 r = 7; UINT16 g[1] = { 0x7777 };
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(x.c.a1, L"A", 0, x.h.face, &s, &r, g), S_OK);
    EXPECT_EQ(r, 0); EXPECT_EQ(g[0], 0x7777);
    t_note("empty text: isTextSimple=%d", s);
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(x.c.a1, NULL, 0, x.h.face, &s, &r, NULL), S_OK);
    EXPECT_EQ(r, 0);
    cx_close(&x);
}
RT(cx_failure_clears_outputs) {
    BOOL s = 7; UINT32 r = 7;
    if (t_is_wine()) { t_skip("the Wine implementation dereferences a NULL text pointer"); return; }
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(x.c.a1, NULL, 3, x.h.face, &s, &r, NULL), R2_E_INVALIDARG);
    EXPECT_EQ(s, 0); EXPECT_EQ(r, 0);
    s = 7; r = 7;
    if (!t_is_wine()) { CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(x.c.a1, L"AB", 2, NULL, &s, &r, NULL), R2_E_INVALIDARG); EXPECT_EQ(s, 0); EXPECT_EQ(r, 0); }
    cx_close(&x);
}
RT(cx_surrogate_code_units_are_looked_up_individually) {
    /* the scan is per UTF-16 code unit: a cmap entry for a surrogate unit (non standard) is not what makes a pair simple */
    BOOL s; UINT32 r; UINT16 g[4];
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"A\xD83D\xDE00", 3, &s, &r, g), S_OK);
    EXPECT_EQ(s, 1); EXPECT_EQ(r, 1); EXPECT_EQ(g[0], 2);
    CHECK_HR(cx_query(&x, L"\xD83D\xDE00" L"A", 3, &s, &r, g), S_OK);
    EXPECT_EQ(s, 0); CHECK(r >= 1 && r <= 2);
    cx_close(&x);
}
RT(cx_astral_cmap_entry_is_complex) {
    ttf_map m[] = { { 'A', 2 }, { 0x1F600, 3 } }; BOOL s; UINT32 r; UINT16 g[4];
    CX_OPEN(x, m, 2, 12, 3, 10, 0);
    CHECK_HR(cx_query(&x, L"\xD83D\xDE00", 2, &s, &r, g), S_OK);
    EXPECT_EQ(s, 0); CHECK(r >= 1); EXPECT_EQ(g[0], 0);
    cx_close(&x);
}

/* ---- full BMP scan: one cmap maps every BMP code point to a valid glyph, so the verdict is purely the character property ----------- */
static ttf_map g_bmp[0xFFFF];
RT(cx_bmp_scan_character_properties) {
    int n = 0; unsigned cp; BOOL s; UINT32 r; UINT16 g[1];
    for (cp = 0x20; cp < 0xFFFF; cp++) { if (cp >= 0xD800 && cp < 0xE000) continue; g_bmp[n].cp = cp; g_bmp[n].gid = 2; n++; }
    CX_OPEN(x, g_bmp, n, 12, 3, 10, 0);
    unsigned ncomplex = 0, bad_read = 0, bad_gid = 0, start = 0, in_run = 0, runs = 0;
    static const unsigned must_simple[] = { 0x20, 0x21, 0x2D, 0x30, 0x39, 0x41, 0x5A, 0x61, 0x7A, 0x7E, 0xA0, 0xA9, 0xC0, 0xE9, 0xFF, 0x0391, 0x0416, 0x2013, 0x20AC };
    for (cp = 0x20; cp < 0xFFFF; cp++) {
        WCHAR w = (WCHAR)cp;
        if (cp >= 0xD800 && cp < 0xE000) continue;
        HRESULT hr = cx_query(&x, &w, 1, &s, &r, g);
        if (hr != S_OK) { CHECKF(0, "U+%04X: %s", cp, dw_hr_name(hr)); break; }
        if (r != 1) bad_read++;
        if (s != 0 && s != 1) bad_read++;
        if (s ? g[0] != 2 : g[0] != 0) bad_gid++;
        if (!s) {
            ncomplex++;
            if (!in_run) { start = cp; in_run = 1; }
        } else if (in_run) {
            if (runs++ < 150) t_note("complex U+%04X..U+%04X", start, cp - 1);
            in_run = 0;
        }
    }
    if (in_run && runs++ < 150) t_note("complex U+%04X..U+FFFE", start);
    CHECKF(bad_read == 0, "%u code points with read != 1 or a flag outside 0/1", bad_read);
    CHECKF(bad_gid == 0, "%u code points whose glyph disagrees with the verdict", bad_gid);
    t_note("%u of %d mapped BMP code points are complex (%u runs)", ncomplex, n, runs);
    for (cp = 0; cp < sizeof must_simple / sizeof *must_simple; cp++) {
        WCHAR w = (WCHAR)must_simple[cp];
        CHECK_HR(cx_query(&x, &w, 1, &s, &r, g), S_OK);
        CHECKF(s == 1, "U+%04X is simple when mapped", must_simple[cp]);
    }
    cx_close(&x);
}
RT(cx_bmp_scan_unmapped_control_characters_are_complex) {
    /* the default synthetic font maps only space, digits and letters: everything else has no glyph and is complex */
    r2_ctx c; dwx_font h; BOOL s; UINT32 r; UINT16 g[1]; unsigned cp, bad = 0;
    if (!r2_open(&c) || FAILED(dwx_font_open_default(&h, c.f)) || !h.face) { dwx_font_close(&h); r2_close(&c); t_skip("default font unavailable"); return; }
    for (cp = 0; cp < 0x80; cp++) {
        WCHAR w = (WCHAR)cp;
        HRESULT hr = IDWriteTextAnalyzer1_GetTextComplexity(c.a1, &w, 1, h.face, &s, &r, g);
        int mapped = ttf_default_gid(cp) != 0;
        if (hr != S_OK || s != mapped || r != 1) bad++;
    }
    CHECKF(bad == 0, "%u ASCII code units with the wrong verdict", bad);
    dwx_font_close(&h); r2_close(&c);
}

/* ---- base + combining mark sequences: only invariants are hard, the property dependent verdicts are noted --------------------------- */
RT(cx_base_plus_combining_mark_invariants) {
    ttf_map m[] = { { 'A', 2 }, { 'B', 3 }, { 0x0300, 4 }, { 0x0301, 5 }, { 0x0308, 6 }, { 0x200D, 7 }, { 0xFE0F, 8 } }; BOOL s; UINT32 r; UINT16 g[8]; unsigned i;
    CX_OPEN(x, m, 7, 4, 3, 1, 0);
    static const WCHAR *t[] = { L"A\x0301", L"AB\x0301", L"A\x0300\x0301", L"\x0301", L"A\x200D" L"B", L"A\xFE0F", L"AB\x0308" L"A", L"\x0300" L"AB" };
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        UINT32 len = (UINT32)lstrlenW(t[i]), k;
        HRESULT hr = cx_query(&x, t[i], len, &s, &r, g);
        CHECK_HR(hr, S_OK);
        CHECKF(r <= len, "case %u: read %u > %u", i, r, len);
        CHECKF(s == 0 || s == 1, "case %u: flag %d", i, s);
        if (s == 1) for (k = 0; k < r; k++) CHECKF(g[k] != 0 && g[k] != 0x7777, "case %u: simple run holds glyph 0x%x at %u", i, g[k], k);
        t_note("case %u: simple=%d read=%u of %u", i, s, r, len);
    }
    cx_close(&x);
}
RT(cx_non_combining_complex_char_ends_simple_prefix) {
    /* U+0627 is a letter, not a combining mark: when it is complex on its own, "AB" + U+0627 reads exactly "AB" as simple */
    ttf_map m[] = { { 'A', 2 }, { 'B', 3 }, { 0x0627, 4 } }; BOOL s; UINT32 r; UINT16 g[4]; BOOL alone;
    CX_OPEN(x, m, 3, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"\x0627", 1, &alone, &r, g), S_OK);
    if (alone) { cx_close(&x); t_skip("U+0627 is simple when mapped"); return; }
    CHECK_HR(cx_query(&x, L"AB\x0627", 3, &s, &r, g), S_OK); EXPECT_EQ(s, 1); EXPECT_EQ(r, 2);
    CHECK_HR(cx_query(&x, L"\x0627" L"AB", 3, &s, &r, g), S_OK); EXPECT_EQ(s, 0); EXPECT_EQ(r, 1);
    CHECK_HR(cx_query(&x, L"\x0627\x0627" L"AB", 4, &s, &r, g), S_OK); EXPECT_EQ(s, 0); EXPECT_EQ(r, 2);
    cx_close(&x);
}
RT(cx_repeatable_and_stateless) {
    BOOL s1, s2; UINT32 r1, r2; UINT16 g1[5], g2[5];
    CX_OPEN(x, k_ascii, 6, 4, 3, 1, 0);
    CHECK_HR(cx_query(&x, L"ABzAB", 5, &s1, &r1, g1), S_OK);
    CHECK_HR(cx_query(&x, L"ABzAB", 5, &s2, &r2, g2), S_OK);
    CHECK(s1 == s2 && r1 == r2 && g1[0] == g2[0] && g1[1] == g2[1]);
    cx_close(&x);
}
RT(cx_two_faces_independent) {
    cx a, b; BOOL s; UINT32 r; UINT16 g[2];
    static const ttf_map ma[] = { { 'A', 2 } }, mb[] = { { 'A', 9 }, { 'B', 3 } };
    if (!cx_open(&a, ma, 1, 4, 3, 1, 0) || !cx_open(&b, mb, 2, 4, 3, 1, 0)) { cx_close(&a); cx_close(&b); t_skip("fonts unavailable"); return; }
    CHECK_HR(cx_query(&a, L"AB", 2, &s, &r, g), S_OK); EXPECT_EQ(r, 1); EXPECT_EQ(g[0], 2);
    CHECK_HR(cx_query(&b, L"AB", 2, &s, &r, g), S_OK); EXPECT_EQ(r, 2); EXPECT_EQ(g[0], 9); EXPECT_EQ(g[1], 3);
    cx_close(&a); cx_close(&b);
}
RT(cx_through_analyzer_object_lifetime) {
    r2_ctx c; dwx_font h; BOOL s; UINT32 r; UINT16 g[2]; IDWriteTextAnalyzer1 *a1;
    if (!r2_open(&c) || FAILED(dwx_font_open_default(&h, c.f)) || !h.face) { dwx_font_close(&h); r2_close(&c); t_skip("default font unavailable"); return; }
    a1 = c.a1; IDWriteTextAnalyzer1_AddRef(a1);
    r2_close(&c);                                                  /* drop the other references: the extra one keeps the analyzer alive */
    CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(a1, L"AB", 2, h.face, &s, &r, g), S_OK);
    EXPECT_EQ(s, 1); EXPECT_EQ(r, 2); EXPECT_EQ(g[0], ttf_default_gid('A'));
    IDWriteTextAnalyzer1_Release(a1);
    dwx_font_close(&h);
}

/* ---- fonts with layout features: the simple-glyph verdict may depend on the glyph being touched by GSUB lookups ----------------------------------- */
RT(cx_glyphs_touched_by_layout_features_note) {
    /* GetGlyphIndexIfSimple additionally consults an optional per-glyph bit set; which glyphs it excludes is recorded here for
     * each feature kind (glyph 'A'..'H' = gids 2..9 each covered by a different feature) */
    r2_ctx c; r2_font x; otl_table g, p; unsigned i; int s, l, ft;
    static const char *gsub_tags[] = { "ccmp", "locl", "rlig", "liga", "calt", "smcp" };
    unsigned short one[1];
    if (!r2_open(&c)) { r2_close(&c); t_skip("analyzer unavailable"); return; }
    otl_init(&g); otl_init(&p);
    s = otl_add_script(&g, "latn"); otl_add_langsys(&g, s, "dflt", -1, 0);
    for (i = 0; i < 6; i++) { one[0] = (unsigned short)(2 + i); l = otl_add_lookup(&g, OTL_SINGLE, 1, 1, 0, one, 1); ft = otl_add_feature(&g, gsub_tags[i], 1, l); g.sc[s].ls[0].f[g.sc[s].ls[0].nf++] = ft; }
    s = otl_add_script(&p, "latn"); otl_add_langsys(&p, s, "dflt", -1, 0);
    one[0] = 8; l = otl_add_lookup(&p, OTL_MULTIPLE, 1, 1, 0, one, 1); ft = otl_add_feature(&p, "kern", 1, l); p.sc[s].ls[0].f[p.sc[s].ls[0].nf++] = ft;
    one[0] = 9; l = otl_add_lookup(&p, OTL_SINGLE, 1, 1, 0, one, 1); ft = otl_add_feature(&p, "mark", 1, l); p.sc[s].ls[0].f[p.sc[s].ls[0].nf++] = ft;
    if (FAILED(r2_font_open(&x, c.f, 0, &g, &p)) || !x.h.face) { r2_font_close(&x); r2_close(&c); t_skip("font rejected"); return; }
    for (i = 0; i < 8; i++) {
        WCHAR w = (WCHAR)('A' + i); BOOL simple = 0; UINT32 read = 0; UINT16 gl[1] = { 0x7777 };
        static const char *names[] = { "ccmp", "locl", "rlig", "liga", "calt", "smcp", "GPOS kern", "GPOS mark" };
        CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(c.a1, &w, 1, x.h.face, &simple, &read, gl), S_OK);
        CHECKF(read == 1, "'%c': read %u", (char)w, read);
        CHECKF(simple ? gl[0] == 2u + i : gl[0] == 0, "'%c': glyph 0x%x does not match the verdict", (char)w, gl[0]);
        t_note("glyph covered by %s: simple=%d glyph=%u", names[i], simple, gl[0]);
    }
    { WCHAR w = L'Z'; BOOL simple = 0; UINT32 read = 0; UINT16 gl[1];
      CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(c.a1, &w, 1, x.h.face, &simple, &read, gl), S_OK);
      EXPECT_EQ(simple, 1); EXPECT_EQ(gl[0], ttf_default_gid('Z')); }
    r2_font_close(&x); r2_close(&c);
}
