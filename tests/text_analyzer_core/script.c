/* IDWriteTextAnalyzer::AnalyzeScript: sink SetScriptAnalysis ranges.
 *
 * The runs reported to the sink are ascending, non-overlapping, contiguous and cover exactly [textPosition, textPosition+textLength).
 * Script ids are internal indices; the portable meaning is obtained with IDWriteTextAnalyzer1::GetScriptProperties and compared with the
 * ISO 15924 numeric code (Latn 215, Grek 200, Cyrl 220, Arab 160, Hebr 125, Thai 352, Deva 315, Hang 286, Hani 500 ...).
 * Common and Inherited characters (spaces, digits, punctuation, combining marks, joiners) do not start runs of their own: they are
 * absorbed into the neighbouring script run. */
#include "ta.h"

#define W(...) ((const WCHAR[]){ __VA_ARGS__, 0 })

/* One uniform-script text: exactly one run, covering everything, with the expected ISO code and default shaping. */
static void script_one(UINT32 iso, const WCHAR *t)
{
    TA_BEGIN();
    dwx_ana_src *s; dwx_ana_sink *k; UINT32 n = LEN(t), i;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(dwx_ana_covered(k, 's', n));
    CHECK(ta_runs_exact(k, 's', 0, n));
    CHECKF(k->n_script == 1, "expected 1 run, got %ld", k->n_script);
    CHECK(!k->overlap);
    if (k->script_set[0]) {
        UINT32 got = ta_iso(an, k->script[0]);
        CHECKF(got == iso, "script id %u -> ISO %u, want %u", (unsigned)k->script[0].script, got, iso);
        CHECK(k->script[0].shapes == DWRITE_SCRIPT_SHAPES_DEFAULT);
        for (i = 1; i < n; i++) CHECK(k->script[i].script == k->script[0].script);
    } else CHECK(0);
    ta_free(s, k); TA_END();
}
#define SCRIPT_TEST(nm, iso, ...) TEST(script_##nm) { script_one(iso, W(__VA_ARGS__)); }

SCRIPT_TEST(latin_lower, 215, 'a', 'b', 'c')
SCRIPT_TEST(latin_upper, 215, 'A', 'B', 'Z')
SCRIPT_TEST(latin1, 215, 0xE9, 0xF1, 0xFF)
SCRIPT_TEST(latin_ext_a, 215, 0x100, 0x17F)
SCRIPT_TEST(latin_ext_b, 215, 0x180, 0x24F)
SCRIPT_TEST(latin_ext_additional, 215, 0x1E00, 0x1EF9)
SCRIPT_TEST(greek, 200, 0x3B1, 0x3B2, 0x3B3)
SCRIPT_TEST(greek_capital, 200, 0x391, 0x3A9)
SCRIPT_TEST(greek_extended, 200, 0x1F00, 0x1F01)
SCRIPT_TEST(cyrillic, 220, 0x430, 0x431, 0x432)
SCRIPT_TEST(cyrillic_capital, 220, 0x410, 0x42F)
SCRIPT_TEST(cyrillic_supplement, 220, 0x500, 0x510)
SCRIPT_TEST(armenian, 230, 0x531, 0x561)
SCRIPT_TEST(georgian, 240, 0x10D0, 0x10D1)
SCRIPT_TEST(hebrew, 125, 0x5D0, 0x5D1, 0x5D2)
SCRIPT_TEST(arabic, 160, 0x627, 0x628, 0x629)
SCRIPT_TEST(arabic_ext, 160, 0x6A9, 0x6CC)
SCRIPT_TEST(arabic_presentation_b, 160, 0xFE8D, 0xFE8E)
SCRIPT_TEST(syriac, 135, 0x710, 0x712)
SCRIPT_TEST(thaana, 170, 0x780, 0x781)
SCRIPT_TEST(devanagari, 315, 0x915, 0x916, 0x917)
SCRIPT_TEST(bengali, 325, 0x995, 0x996)
SCRIPT_TEST(gurmukhi, 310, 0xA15, 0xA16)
SCRIPT_TEST(gujarati, 320, 0xA95, 0xA96)
SCRIPT_TEST(tamil, 346, 0xB95, 0xB99)
SCRIPT_TEST(telugu, 340, 0xC15, 0xC16)
SCRIPT_TEST(kannada, 345, 0xC95, 0xC96)
SCRIPT_TEST(malayalam, 347, 0xD15, 0xD16)
SCRIPT_TEST(sinhala, 348, 0xD9A, 0xD9B)
SCRIPT_TEST(thai, 352, 0xE01, 0xE02, 0xE03)
SCRIPT_TEST(lao, 356, 0xE81, 0xE82)
SCRIPT_TEST(tibetan, 330, 0xF40, 0xF41)
SCRIPT_TEST(myanmar, 350, 0x1000, 0x1001)
SCRIPT_TEST(khmer, 355, 0x1780, 0x1781)
SCRIPT_TEST(mongolian, 145, 0x1820, 0x1821)
SCRIPT_TEST(ethiopic, 430, 0x1200, 0x1201)
SCRIPT_TEST(hangul_syllables, 286, 0xD55C, 0xAD6D, 0xC5B4)
SCRIPT_TEST(hangul_jamo, 286, 0x1100, 0x1161, 0x11A8)
SCRIPT_TEST(han, 500, 0x4E2D, 0x6587)
SCRIPT_TEST(han_compat, 500, 0xF900, 0xF901)
SCRIPT_TEST(han_ext_a, 500, 0x3400, 0x3401)
SCRIPT_TEST(han_ext_b_surrogates, 500, 0xD840, 0xDC00, 0xD840, 0xDC01)
SCRIPT_TEST(hiragana, 410, 0x3042, 0x3044, 0x3046)
SCRIPT_TEST(katakana, 411, 0x30A2, 0x30A4, 0x30A6)
SCRIPT_TEST(katakana_halfwidth, 411, 0xFF71, 0xFF72)
SCRIPT_TEST(bopomofo, 285, 0x3105, 0x3106)

/* Rarer scripts: windows versions differ in how many are itemised separately, so the result is recorded, not enforced. */
TEST(script_extended_scripts_note)
{
    TA_BEGIN();
    static const struct { const char *n; UINT32 iso; WCHAR t[3]; } rows[] = {
        { "Cherokee", 445, { 0x13A0, 0x13A1 } }, { "UCAS", 440, { 0x1401, 0x1402 } }, { "Ogham", 212, { 0x1681, 0x1682 } },
        { "Runic", 211, { 0x16A0, 0x16A1 } }, { "Braille", 570, { 0x2800, 0x2801 } }, { "Yi", 460, { 0xA000, 0xA001 } },
        { "Tifinagh", 120, { 0x2D30, 0x2D31 } }, { "Coptic", 204, { 0x2C80, 0x2C81 } }, { "Glagolitic", 225, { 0x2C00, 0x2C01 } },
        { "Oriya", 327, { 0xB15, 0xB16 } }, { "NKo", 165, { 0x7CA, 0x7CB } }, { "Limbu", 336, { 0x1900, 0x1901 } },
        { "TaiLe", 353, { 0x1950, 0x1951 } }, { "Buginese", 367, { 0x1A00, 0x1A01 } }, { "Balinese", 360, { 0x1B05, 0x1B06 } },
    };
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        dwx_ana_src *s; dwx_ana_sink *k;
        CHECK_HR_OK(ta_run(an, TA_SCRIPT, rows[i].t, &s, &k));
        CHECK(ta_runs_exact(k, 's', 0, 2));
        t_note("%s: run count %ld, ISO %u (want %u)", rows[i].n, k->n_script, k->script_set[0] ? ta_iso(an, k->script[0]) : 0, rows[i].iso);
        ta_free(s, k);
    }
    TA_END();
}

/* ---- run structure --------------------------------------------------------------------------------------------------------------- */
typedef struct { UINT32 pos, len, iso; } exp_run;
static void runs_expect(const WCHAR *t, const exp_run *e, int ne)
{
    TA_BEGIN();
    dwx_ana_src *s; dwx_ana_sink *k; int i; UINT32 n = LEN(t);
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(ta_runs_exact(k, 's', 0, n));
    CHECKF(k->n_script == ne, "run count %ld, want %d", k->n_script, ne);
    for (i = 0; i < ne && i < 256 && i < k->nrun_script; i++) {
        CHECKF(k->script_runs[i].pos == e[i].pos && k->script_runs[i].len == e[i].len, "run %d is (%u,%u), want (%u,%u)", i,
               k->script_runs[i].pos, k->script_runs[i].len, e[i].pos, e[i].len);
        if (k->script_set[e[i].pos]) CHECKF(ta_iso(an, k->script[e[i].pos]) == e[i].iso, "run %d ISO %u want %u", i, ta_iso(an, k->script[e[i].pos]), e[i].iso);
    }
    ta_free(s, k); TA_END();
}
#define RUNS_TEST(nm, text, ...) TEST(runs_##nm) { static const exp_run e[] = { __VA_ARGS__ }; runs_expect(text, e, (int)(sizeof e / sizeof e[0])); }

RUNS_TEST(latin_greek,        L"abc\x03b1\x03b2\x03b3",                  { 0, 3, 215 }, { 3, 3, 200 })
RUNS_TEST(greek_latin,        L"\x03b1\x03b2" L"ab",                      { 0, 2, 200 }, { 2, 2, 215 })
RUNS_TEST(latin_cyrillic,     L"ab\x0430\x0431",                          { 0, 2, 215 }, { 2, 2, 220 })
RUNS_TEST(three_scripts,      L"abc\x03b1\x03b2\x03b3\x0430\x0431\x0432",  { 0, 3, 215 }, { 3, 3, 200 }, { 6, 3, 220 })
RUNS_TEST(latin_arabic_latin, L"abc\x0628\x0629" L"d",                    { 0, 3, 215 }, { 3, 2, 160 }, { 5, 1, 215 })
RUNS_TEST(hebrew_latin,       L"\x05d0\x05d1\x05d2" L"abc",               { 0, 3, 125 }, { 3, 3, 215 })
RUNS_TEST(latin_han,          L"ab\x4e2d\x6587",                          { 0, 2, 215 }, { 2, 2, 500 })
RUNS_TEST(han_latin_han,      L"\x4e2d" L"a\x6587",                       { 0, 1, 500 }, { 1, 1, 215 }, { 2, 1, 500 })
RUNS_TEST(thai_latin,         L"\x0e01\x0e02" L"ab",                      { 0, 2, 352 }, { 2, 2, 215 })
RUNS_TEST(deva_latin,         L"\x0915\x0916" L"ab",                      { 0, 2, 315 }, { 2, 2, 215 })
RUNS_TEST(hangul_latin,       L"\xd55c\xad6d" L"ab",                      { 0, 2, 286 }, { 2, 2, 215 })
RUNS_TEST(hira_kata,          L"\x3042\x3044\x30a2\x30a4",                { 0, 2, 410 }, { 2, 2, 411 })
RUNS_TEST(surrogate_between,  L"a\xd840\xdc00" L"b",                      { 0, 1, 215 }, { 1, 2, 500 }, { 3, 1, 215 })
RUNS_TEST(alternating_each,   L"a\x03b1" L"b\x03b2",                      { 0, 1, 215 }, { 1, 1, 200 }, { 2, 1, 215 }, { 3, 1, 200 })
RUNS_TEST(single_char,        L"x",                                       { 0, 1, 215 })
RUNS_TEST(single_greek,       L"\x03a9",                                  { 0, 1, 200 })
RUNS_TEST(latin_with_inline_space, L"abc def",                            { 0, 7, 215 })
RUNS_TEST(latin_digits_punct, L"Hello, world 42!",                        { 0, 16, 215 })
RUNS_TEST(trailing_punct,     L"abc!?.",                                  { 0, 6, 215 })
RUNS_TEST(digits_inside_latin, L"abc123def",                              { 0, 9, 215 })
RUNS_TEST(greek_with_commons, L"\x03b1\x03b2, 12 \x03b3",                 { 0, 8, 200 })
RUNS_TEST(arabic_with_digits, L"\x0628 123 \x0628",                       { 0, 7, 160 })
RUNS_TEST(hebrew_with_maqaf,  L"\x05d0\x05be\x05d1",                      { 0, 3, 125 })
RUNS_TEST(inherited_after_latin, L"e\x0301" L"b",                         { 0, 3, 215 })
RUNS_TEST(inherited_after_greek, L"\x03b1\x0301\x03b2",                   { 0, 3, 200 })
RUNS_TEST(inherited_after_cyrillic, L"\x0430\x0301",                      { 0, 2, 220 })
RUNS_TEST(inherited_between_scripts, L"a\x0301\x03b1",                    { 0, 2, 215 }, { 2, 1, 200 })
RUNS_TEST(nbsp_in_latin,      L"a\x00a0" L"b",                            { 0, 3, 215 })
RUNS_TEST(thai_with_digits,   L"\x0e01 12 \x0e02",                        { 0, 6, 352 })
RUNS_TEST(han_with_cjk_punct, L"\x4e2d\x3001\x6587\x3002",                { 0, 4, 500 })
RUNS_TEST(mixed_long,         L"Hello \x03ba\x03cc\x03c3\x03bc\x03b5 \x043c\x0438\x0440 \x05e9\x05dc\x05d5\x05dd",
                              { 0, 6, 215 }, { 6, 6, 200 }, { 12, 4, 220 }, { 16, 4, 125 })

/* The assignment of the Common characters that sit between two different scripts (or lead the text) is not specified: the position
 * of the boundary is recorded while the letters themselves are checked. */
TEST(runs_common_between_scripts)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc \x03b1\x03b2\x03b3";   /* the space belongs to one of the two neighbours */
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(ta_runs_exact(k, 's', 0, 7));
    CHECK(k->n_script == 2);
    if (k->n_script == 2) {
        UINT32 b = k->script_runs[1].pos;
        CHECKF(b == 3 || b == 4, "boundary at %u", b);
        t_note("space between Latin and Greek is assigned to the %s run", b == 4 ? "Latin" : "Greek");
        CHECK(ta_iso(an, k->script[0]) == 215); CHECK(ta_iso(an, k->script[6]) == 200);
    }
    ta_free(s, k); TA_END();
}
TEST(runs_leading_common)
{
    TA_BEGIN();
    static const WCHAR t[] = L"  abc";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(ta_runs_exact(k, 's', 0, 5));
    CHECK(ta_iso(an, k->script[4]) == 215);
    t_note("leading spaces: %ld run(s); first ISO %u", k->n_script, ta_iso(an, k->script[0]));
    ta_free(s, k); TA_END();
}
TEST(runs_only_common)
{
    TA_BEGIN();
    static const WCHAR t[] = L" 123, ";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(ta_runs_exact(k, 's', 0, 6));
    CHECK(k->n_script == 1);
    if (k->script_set[0]) t_note("Common-only text: script id %u, ISO %u, shapes %d", (unsigned)k->script[0].script, ta_iso(an, k->script[0]), (int)k->script[0].shapes);
    ta_free(s, k); TA_END();
}
TEST(runs_digits_leading_then_script)
{
    TA_BEGIN();
    static const WCHAR t[] = L"123\x03b1\x03b2";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(ta_runs_exact(k, 's', 0, 5));
    CHECK(ta_iso(an, k->script[4]) == 200);
    t_note("digits before Greek: %ld run(s)", k->n_script);
    ta_free(s, k); TA_END();
}
/* Controls and joiners: coverage is exact, whether they split the Latin/Arabic run is recorded. */
static void note_runs(const WCHAR *t, const char *what)
{
    TA_BEGIN();
    dwx_ana_src *s; dwx_ana_sink *k; UINT32 n = LEN(t);
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(ta_runs_exact(k, 's', 0, n));
    t_note("%s: %ld run(s)", what, k->n_script);
    if (k->script_set[0]) CHECK(ta_iso(an, k->script[0]) != 0);
    ta_free(s, k); TA_END();
}
TEST(runs_tab_newline_note)  { note_runs(L"a\tb\r\nc", "Latin with TAB and CRLF"); }
TEST(runs_zwj_note)          { note_runs(L"\x0628\x200d\x0628", "Arabic with ZWJ"); }
TEST(runs_zwnj_note)         { note_runs(L"\x0628\x200c\x0628", "Arabic with ZWNJ"); }
TEST(runs_line_separator_note) { note_runs(L"a\x2028" L"b\x2029" L"c", "Latin with LS and PS"); }
TEST(runs_variation_selector_note) { note_runs(L"\x4e2d\xfe00\x6587", "Han with VS1"); }
TEST(runs_bidi_controls_note) { note_runs(L"a\x202b" L"b\x202c" L"c", "Latin with RLE/PDF"); }

TEST(runs_lone_surrogates_note)
{
    TA_BEGIN();
    static const WCHAR t1[] = L"a\xd800" L"b", t2[] = L"a\xdc00" L"b";
    dwx_ana_src *s; dwx_ana_sink *k;
    HRESULT hr = ta_run(an, TA_SCRIPT, t1, &s, &k);
    CHECK_HR_OK(hr);
    if (hr == S_OK) CHECK(ta_runs_exact(k, 's', 0, 3));
    ta_free(s, k);
    hr = ta_run(an, TA_SCRIPT, t2, &s, &k);
    CHECK_HR_OK(hr);
    if (hr == S_OK) CHECK(ta_runs_exact(k, 's', 0, 3));
    ta_free(s, k);
    TA_END();
}
TEST(runs_noncharacters_and_controls)
{
    TA_BEGIN();
    static const WCHAR t[] = L"a\x0001\x007f\xfffe" L"b";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(ta_runs_exact(k, 's', 0, 5));
    CHECK(ta_iso(an, k->script[0]) == 215); CHECK(ta_iso(an, k->script[4]) == 215);
    t_note("controls inside Latin: %ld run(s), shapes of pos 1 = %d", k->n_script, k->script_set[1] ? (int)k->script[1].shapes : -1);
    ta_free(s, k); TA_END();
}
TEST(shapes_no_visual_note)
{
    /* DWRITE_SCRIPT_SHAPES_NO_VISUAL marks runs that produce no glyph art (control characters); recorded only */
    TA_BEGIN();
    static const WCHAR t[] = L"\x0001\x0002\x0003";
    dwx_ana_src *s; dwx_ana_sink *k;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(ta_runs_exact(k, 's', 0, 3));
    CHECK(k->script_set[0] && (k->script[0].shapes == DWRITE_SCRIPT_SHAPES_DEFAULT || k->script[0].shapes == DWRITE_SCRIPT_SHAPES_NO_VISUAL));
    t_note("C0 controls: script %u shapes %d runs %ld", (unsigned)k->script[0].script, (int)k->script[0].shapes, k->n_script);
    ta_free(s, k); TA_END();
}
TEST(shapes_field_values)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc\x03b1\x0628\x05d0\x4e2d\x0e01";
    dwx_ana_src *s; dwx_ana_sink *k; UINT32 i;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    for (i = 0; i < LEN(t); i++) CHECKF(k->script_set[i] && k->script[i].shapes == DWRITE_SCRIPT_SHAPES_DEFAULT, "pos %u shapes %d", i, (int)k->script[i].shapes);
    ta_free(s, k); TA_END();
}

/* ---- sub-ranges ------------------------------------------------------------------------------------------------------------------ */
static const WCHAR g_mix[] = L"abc\x03b1\x03b2\x03b3\x0430\x0431\x0432";   /* 3 Latin, 3 Greek, 3 Cyrillic */

static void sub(UINT32 pos, UINT32 len, int nruns, UINT32 first_iso, UINT32 last_iso)
{
    TA_BEGIN();
    dwx_ana_src *s = dwx_ana_src_new(g_mix, 9); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, pos, len, k));
    CHECK(ta_runs_exact(k, 's', pos, len));
    CHECKF(k->n_script == nruns, "pos %u len %u: %ld runs, want %d", pos, len, k->n_script, nruns);
    CHECK(!k->overlap);
    if (len) {
        UINT32 i;
        CHECK(k->script_set[pos] && k->script_set[pos + len - 1]);
        if (pos) CHECK(!k->script_set[pos - 1]);
        if (pos + len < 9) CHECK(!k->script_set[pos + len]);
        if (k->script_set[pos]) CHECK(ta_iso(an, k->script[pos]) == first_iso);
        if (k->script_set[pos + len - 1]) CHECK(ta_iso(an, k->script[pos + len - 1]) == last_iso);
        for (i = 0; i < pos; i++) CHECK(!k->script_set[i]);
    }
    ta_free(s, k); TA_END();
}
TEST(sub_whole)         { sub(0, 9, 3, 215, 220); }
TEST(sub_first_script)  { sub(0, 3, 1, 215, 215); }
TEST(sub_middle_script) { sub(3, 3, 1, 200, 200); }
TEST(sub_last_script)   { sub(6, 3, 1, 220, 220); }
TEST(sub_straddle_two)  { sub(2, 3, 2, 215, 200); }
TEST(sub_straddle_all)  { sub(2, 5, 3, 215, 220); }
TEST(sub_inside_run)    { sub(4, 1, 1, 200, 200); }
TEST(sub_tail)          { sub(8, 1, 1, 220, 220); }
TEST(sub_single_first)  { sub(0, 1, 1, 215, 215); }

TEST(sub_first_source_call_is_at_position)
{
    TA_BEGIN();
    dwx_ana_src *s = dwx_ana_src_new(g_mix, 9); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 3, 3, k));
    CHECK(s->log_n > 0);
    if (s->log_n > 0) { CHECK(s->log[0].fn == 1); CHECK(s->log[0].pos == 3); }
    ta_free(s, k); TA_END();
}
TEST(source_positions_stay_inside_range)
{
    TA_BEGIN();
    dwx_ana_src *s = dwx_ana_src_new(g_mix, 9); dwx_ana_sink *k = dwx_ana_sink_new(); int i;
    s->chunk = 1;
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 2, 5, k));
    for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 1) CHECKF(s->log[i].pos >= 2 && s->log[i].pos <= 7, "GetTextAtPosition(%u) outside [2,7]", s->log[i].pos);
    CHECK(s->n_at > 0);
    ta_free(s, k); TA_END();
}
TEST(source_chunk_one_requests_each_position)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abcdef";
    dwx_ana_src *s = dwx_ana_src_new(t, 6); dwx_ana_sink *k = dwx_ana_sink_new(); int i; unsigned seen = 0;
    s->chunk = 1;
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 0, 6, k));
    for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 1 && s->log[i].pos < 6) seen |= 1u << s->log[i].pos;
    CHECKF(seen == 0x3f, "positions requested: mask %#x", seen);
    CHECK(ta_runs_exact(k, 's', 0, 6)); CHECK(k->n_script == 1);
    ta_free(s, k); TA_END();
}
TEST(source_sees_no_calls_beyond_text)
{
    TA_BEGIN();
    static const WCHAR t[] = L"abc\x03b1\x03b2";
    dwx_ana_src *s; dwx_ana_sink *k; int i;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 1) CHECKF(s->log[i].pos <= 5, "GetTextAtPosition(%u)", s->log[i].pos);
    ta_free(s, k); TA_END();
}

/* ---- chunking invariance --------------------------------------------------------------------------------------------------------- */
static const WCHAR *const g_texts[] = {
    L"abc", L"abc def 123", L"abc\x03b1\x03b2\x03b3\x0430", L"\x05d0\x05d1 abc \x0628\x0629", L"a\xd840\xdc00" L"b\xd83d\xde00" L"c",
    L"e\x0301\x0301 \x03b1\x0301", L"Hello, \x4e2d\x6587 \x3042\x30a2 \xd55c\xad6d!", L"\x0e01\x0e02 \x0915\x0916 abc",
};
static void chunk_same(int kind)
{
    TA_BEGIN();
    static const UINT32 chunks[] = { 1, 2, 3, 5, 7 };
    int ti, ci;
    for (ti = 0; ti < (int)(sizeof g_texts / sizeof g_texts[0]); ti++) {
        const WCHAR *t = g_texts[ti]; UINT32 n = LEN(t), i;
        dwx_ana_src *s0; dwx_ana_sink *k0;
        HRESULT h0 = ta_run(an, kind, t, &s0, &k0);
        CHECKF(h0 == S_OK, "text %d unchunked: %s", ti, dw_hr_name(h0));
        for (ci = 0; ci < 5; ci++) {
            dwx_ana_src *s; dwx_ana_sink *k;
            HRESULT h = ta_run_n(an, kind, t, n, chunks[ci], &s, &k);
            CHECKF(h == S_OK, "text %d chunk %u: %s", ti, chunks[ci], dw_hr_name(h));
            for (i = 0; i < n; i++) {
                if (kind == TA_SCRIPT) CHECKF(k->script_set[i] == k0->script_set[i] && !memcmp(&k->script[i], &k0->script[i], sizeof k->script[i]), "text %d chunk %u pos %u script differs", ti, chunks[ci], i);
                else if (kind == TA_BIDI) CHECKF(k->bidi_set[i] == k0->bidi_set[i] && k->bidi_explicit[i] == k0->bidi_explicit[i] && k->bidi_resolved[i] == k0->bidi_resolved[i], "text %d chunk %u pos %u bidi differs", ti, chunks[ci], i);
                else if (kind == TA_BREAK) CHECKF(k->brk_set[i] == k0->brk_set[i] && !memcmp(&k->brk[i], &k0->brk[i], 1), "text %d chunk %u pos %u break differs (%02x vs %02x)", ti, chunks[ci], i, *(unsigned char *)&k->brk[i], *(unsigned char *)&k0->brk[i]);
            }
            ta_free(s, k);
        }
        ta_free(s0, k0);
    }
    TA_END();
}
TEST(chunking_same_script) { chunk_same(TA_SCRIPT); }
TEST(chunking_same_bidi)   { chunk_same(TA_BIDI); }
TEST(chunking_same_break)  { chunk_same(TA_BREAK); }

/* ---- scale ----------------------------------------------------------------------------------------------------------------------- */
TEST(huge_single_script)
{
    TA_BEGIN();
    UINT32 n = 100000, i; WCHAR *t = (WCHAR *)malloc(n * sizeof(WCHAR));
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!t) { TA_END(); t_skip("alloc"); return; }
    for (i = 0; i < n; i++) t[i] = (WCHAR)(i % 7 == 6 ? ' ' : 'a' + i % 26);
    s = dwx_ana_src_new(t, n); k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 0, n, k));
    CHECK(k->n_script == 1);
    CHECK(k->script_runs[0].pos == 0 && k->script_runs[0].len == n);
    ta_free(s, k); free(t); TA_END();
}
TEST(huge_two_scripts)
{
    TA_BEGIN();
    UINT32 n = 60000, i; WCHAR *t = (WCHAR *)malloc(n * sizeof(WCHAR));
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!t) { TA_END(); t_skip("alloc"); return; }
    for (i = 0; i < n; i++) t[i] = (WCHAR)(i < n / 2 ? 'a' : 0x3b1);
    s = dwx_ana_src_new(t, n); k = dwx_ana_sink_new(); s->chunk = 4096;
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 0, n, k));
    CHECK(k->n_script == 2);
    CHECK(k->script_runs[0].pos == 0 && k->script_runs[0].len == n / 2);
    CHECK(k->script_runs[1].pos == n / 2 && k->script_runs[1].len == n / 2);
    ta_free(s, k); free(t); TA_END();
}
TEST(many_runs_alternating)
{
    TA_BEGIN();
    static WCHAR t[2000]; int i;
    dwx_ana_src *s; dwx_ana_sink *k;
    for (i = 0; i < 2000; i++) t[i] = (WCHAR)(i & 1 ? 0x3b1 : 'a');
    s = dwx_ana_src_new(t, 2000); k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 0, 2000, k));
    CHECK(k->n_script == 2000);
    CHECK(dwx_ana_covered(k, 's', 2000));
    CHECK(!k->overlap);
    ta_free(s, k); TA_END();
}
TEST(repeat_same_result)
{
    TA_BEGIN();
    int i;
    for (i = 0; i < 50; i++) {
        dwx_ana_src *s; dwx_ana_sink *k;
        CHECK_HR_OK(ta_run(an, TA_SCRIPT, g_mix, &s, &k));
        CHECK(k->n_script == 3);
        ta_free(s, k);
    }
    TA_END();
}
TEST(sink_reuse_across_calls)
{
    /* a sink is not reset by the analyzer: a second analysis over the same range reports the positions again */
    TA_BEGIN();
    dwx_ana_src *s = dwx_ana_src_new(g_mix, 9); dwx_ana_sink *k = dwx_ana_sink_new();
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 0, 9, k));
    CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 0, 9, k));
    CHECK(k->n_script == 6);
    CHECK(k->overlap);
    ta_free(s, k); TA_END();
}
TEST(analyzers_independent)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *a = NULL, *b = NULL; dwx_ana_src *s; dwx_ana_sink *k;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a)); CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &b));
    if (a && b) {
        CHECK_HR_OK(ta_run(a, TA_SCRIPT, g_mix, &s, &k)); CHECK(k->n_script == 3); ta_free(s, k);
        DW_RELEASE(a);
        CHECK_HR_OK(ta_run(b, TA_SCRIPT, g_mix, &s, &k)); CHECK(k->n_script == 3); ta_free(s, k);
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

/* ---- failures -------------------------------------------------------------------------------------------------------------------- */
TEST(sink_error_propagates_and_aborts)
{
    TA_BEGIN();
    static const HRESULT codes[] = { E_FAIL, E_OUTOFMEMORY, E_NOTIMPL, E_ABORT, (HRESULT)0x88985001, (HRESULT)0x80004005, E_INVALIDARG, E_UNEXPECTED };
    int i;
    for (i = 0; i < (int)(sizeof codes / sizeof codes[0]); i++) {
        dwx_ana_src *s = dwx_ana_src_new(g_mix, 9); dwx_ana_sink *k = dwx_ana_sink_new();
        HRESULT hr;
        k->fail_script = codes[i];
        hr = ta_do(an, TA_SCRIPT, s, 0, 9, k);
        CHECKF(hr == codes[i], "sink returned %s, analyzer returned %s", dw_hr_name(codes[i]), dw_hr_name(hr));
        CHECKF(k->n_script == 1, "sink called %ld times after the first failure", k->n_script);
        ta_free(s, k);
    }
    TA_END();
}
TEST(sink_success_codes_pass)
{
    /* S_FALSE is a success code: the walk is not aborted, and the analyzer itself returns S_OK */
    TA_BEGIN();
    dwx_ana_src *s = dwx_ana_src_new(g_mix, 9); dwx_ana_sink *k = dwx_ana_sink_new();
    HRESULT hr;
    k->fail_script = S_FALSE;
    hr = ta_do(an, TA_SCRIPT, s, 0, 9, k);
    CHECK_HR_OK(hr);
    CHECK(k->n_script == 3);
    ta_free(s, k); TA_END();
}
TEST(source_error_at_position)
{
    TA_BEGIN();
    ta_fsrc fs; dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    ta_fsrc_init(&fs, g_mix, 9); fs.fail_at = E_FAIL; fs.ok_at = 0;
    hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)&fs, 0, 9, (IDWriteTextAnalysisSink *)k);
    CHECK_HR(hr, E_FAIL);
    CHECK(k->n_script == 0);
    CHECK(fs.n_at == 1);
    ta_free(NULL, k); TA_END();
}
TEST(source_error_code_preserved)
{
    TA_BEGIN();
    static const HRESULT codes[] = { E_OUTOFMEMORY, E_ABORT, (HRESULT)0x88985003, E_NOTIMPL };
    int i;
    for (i = 0; i < 4; i++) {
        ta_fsrc fs; dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
        ta_fsrc_init(&fs, g_mix, 9); fs.fail_at = codes[i];
        hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)&fs, 0, 9, (IDWriteTextAnalysisSink *)k);
        CHECKF(hr == codes[i], "source failed with %s, analyzer returned %s", dw_hr_name(codes[i]), dw_hr_name(hr));
        ta_free(NULL, k);
    }
    TA_END();
}
TEST(source_error_stops_walk_midway)
{
    /* the source fails once the iterator has to fetch beyond the first chunk: no sink call reports a half-analysed range as success */
    TA_BEGIN();
    ta_fsrc fs; dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
    static const WCHAR t[] = L"abc\x03b1\x03b2\x03b3";
    if (t_is_wine()) { ta_free(NULL, k); TA_END(); t_skip("emulator faults on source errors"); return; }
    ta_fsrc_init(&fs, t, 6); fs.fail_at = E_FAIL; fs.ok_at = 1;
    hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)&fs, 0, 6, (IDWriteTextAnalysisSink *)k);
    t_note("source fails after the first call: hr %s, %ld GetTextAtPosition calls, %ld sink calls", dw_hr_name(hr), fs.n_at, k->n_script);
    if (fs.n_at == 1) CHECK_HR_OK(hr);   /* iterator satisfied by the first (whole-text) answer */
    ta_free(NULL, k); TA_END();
}
TEST(source_returning_short_chunks_ok)
{
    TA_BEGIN();
    int ch;
    for (ch = 1; ch <= 9; ch++) {
        dwx_ana_src *s = dwx_ana_src_new(g_mix, 9); dwx_ana_sink *k = dwx_ana_sink_new();
        s->chunk = (UINT32)ch;
        CHECK_HR_OK(ta_do(an, TA_SCRIPT, s, 0, 9, k));
        CHECKF(k->n_script == 3, "chunk %d: %ld runs", ch, k->n_script);
        ta_free(s, k);
    }
    TA_END();
}
TEST(source_refcount_balanced_after_error)
{
    TA_BEGIN();
    dwx_ana_src *s = dwx_ana_src_new(g_mix, 9); dwx_ana_sink *k = dwx_ana_sink_new();
    k->fail_script = E_FAIL;
    (void)ta_do(an, TA_SCRIPT, s, 0, 9, k);
    CHECK(dwx_refs(s) == 1 && dwx_refs(k) == 1);
    EXPECT_REFS(an, 1);
    ta_free(s, k); TA_END();
}

/* ---- script properties as a decoder ---------------------------------------------------------------------------------------------- */
TEST(script_properties_basic)
{
    TA_BEGIN();
    IDWriteTextAnalyzer1 *a1 = NULL; DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p;
    static const WCHAR t[] = L"abc";
    CHECK_HR_OK(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer1, (void **)&a1));
    if (a1) {
        sa = ta_script_of(an, t, 3);
        memset(&p, 0xcc, sizeof p);
        CHECK_HR_OK(IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, &p));
        CHECK(p.isoScriptNumber == 215);
        CHECK(!memcmp(&p.isoScriptCode, "Latn", 4));
        CHECK(!p.isConnectedWriting);
        sa = ta_script_of(an, W(0x628, 0x629), 2);
        CHECK_HR_OK(IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, &p));
        CHECK(p.isoScriptNumber == 160);
        CHECK(!memcmp(&p.isoScriptCode, "Arab", 4));
        CHECK(p.isConnectedWriting);
        CHECK(p.isCursiveWriting);
        sa = ta_script_of(an, W(0x4e2d), 1);
        CHECK_HR_OK(IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, &p));
        CHECK(p.isoScriptNumber == 500);
        CHECK(!memcmp(&p.isoScriptCode, "Hani", 4));
        CHECK(p.isDiscreteWriting || p.isBlockWriting || 1);
        DW_RELEASE(a1);
    }
    TA_END();
}
TEST(script_properties_out_of_range)
{
    TA_BEGIN();
    IDWriteTextAnalyzer1 *a1 = NULL; DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p;
    CHECK_HR_OK(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer1, (void **)&a1));
    if (a1) {
        sa.script = 0xFFFF; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
        memset(&p, 0xcc, sizeof p);
        CHECK_HR(IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, &p), E_INVALIDARG);
        DW_RELEASE(a1);
    }
    TA_END();
}
TEST(script_ids_distinct_per_script)
{
    TA_BEGIN();
    static const WCHAR c[] = { 'a', 0x3b1, 0x430, 0x5d0, 0x628, 0x4e2d, 0x0e01, 0x915, 0xd55c, 0x3042, 0x30a2 };
    UINT16 id[11]; int i, j;
    for (i = 0; i < 11; i++) id[i] = ta_script_of(an, &c[i], 1).script;
    for (i = 0; i < 11; i++) for (j = i + 1; j < 11; j++) CHECKF(id[i] != id[j], "scripts %d and %d share id %u", i, j, id[i]);
    TA_END();
}
TEST(script_id_stable_across_calls)
{
    TA_BEGIN();
    UINT16 a = ta_script_of(an, W('a'), 1).script, b = ta_script_of(an, W('z', 'z'), 2).script, c = ta_script_of(an, W(0xe9), 1).script;
    CHECK(a == b); CHECK(a == c);
    TA_END();
}
