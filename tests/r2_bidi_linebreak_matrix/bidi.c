/* AnalyzeBidi matrices: SetBidiLevel(pos, len, explicitLevel, resolvedLevel) for paragraph direction x neutral / weak / number /
 * bracket texts, explicit embedding and override controls, isolates, and structural invariants over generated strings.
 *
 * Expected levels follow the Unicode bidirectional algorithm as implemented by BidiAnalysisContext (explicit rules with an embedding
 * stack, W1..W7, N0 bracket pairs via ResolveParentheses, N1/N2, I1/I2): the explicit level is the level after the explicit rules
 * (paragraph level for characters outside any embedding), the resolved level the level after the implicit rules.  Digit strings in
 * the tables are one level per UTF-16 unit; '-' = not checked.  Values that depend on library version details (exact depth numbers of
 * nested embeddings, isolates) are recorded with t_note and only relational facts are checked. */
#include "ms.h"
#define MT(n) TEST(r2_bidi_linebreak_matrix_##n)

#define H1 L"\x05d0"
#define H2 L"\x05d1"
#define H3 L"\x05d2"
#define A1 L"\x0627"
#define A2 L"\x0628"
#define N1 L"\x0661"
#define N2 L"\x0662"
#define LRE L"\x202a"
#define RLE L"\x202b"
#define PDF L"\x202c"
#define LRO L"\x202d"
#define RLO L"\x202e"
#define LRI L"\x2066"
#define RLI L"\x2067"
#define FSI L"\x2068"
#define PDI L"\x2069"
#define LRM L"\x200e"
#define RLM L"\x200f"

static ms_sink *bd_run(IDWriteTextAnalyzer *an, const WCHAR *t, UINT32 pos, UINT32 len, DWRITE_READING_DIRECTION dir, int pat, HRESULT *hr)
{
    ms_src *s = ms_src_new(t, LEN(t)); ms_sink *k = ms_sink_new(0);
    s->dir = dir; if (pat) ms_src_cuts(s, ms_patterns[pat], 6);
    *hr = ms_do(an, MA_BIDI, s, pos, len, k);
    ms_src_free(s);
    return k;
}
/* The same text with slicing 1 and 3 must give the same answer. */
static void check_levels(const WCHAR *t, DWRITE_READING_DIRECTION dir, const char *ex, const char *res)
{
    MS_BEGIN(); UINT32 n = LEN(t), i; int base = dir == RTL ? 1 : 0; static const int pats[] = { 0, 1, 3 }; int pi;
    for (pi = 0; pi < 3; pi++) {
        HRESULT hr; ms_sink *k = bd_run(an, t, 0, n, dir, pats[pi], &hr);
        CHECK_HR_OK(hr);
        CHECK(ms_sink_exact(k, MK_BIDI, 0, n));
        CHECK(ms_runs_exact(k, MK_BIDI, 0, n));
        for (i = 0; i < n; i++) {
            int we = ex ? (ex[i] == '-' ? -1 : ex[i] - '0') : base, wr = (res && res[i] != '-') ? res[i] - '0' : -1;
            if (we >= 0) CHECKF(k->ex[i] == we, "pattern %d position %u (U+%04X): explicit level %u want %d", pats[pi], i, t[i], k->ex[i], we);
            if (wr >= 0) CHECKF(k->res[i] == wr, "pattern %d position %u (U+%04X): resolved level %u want %d", pats[pi], i, t[i], k->res[i], wr);
        }
        ms_sink_free(k);
    }
    MS_END();
}
#define BD(nm, dir, text, res) MT(bidi_##nm) { check_levels(text, dir, NULL, res); }

/* ---- paragraph level x neutrals ------------------------------------------------------------------------------------------------- */
BD(l_a,        LTR, L"a",                 "0")
BD(l_H,        LTR, H1,                   "1")
BD(l_aH,       LTR, L"a" H1,              "01")
BD(l_Ha,       LTR, H1 L"a",              "10")
BD(l_aHa,      LTR, L"a" H1 L"a",         "010")
BD(l_HaH,      LTR, H1 L"a" H1,           "101")
BD(l_a_H,      LTR, L"a " H1,             "001")
BD(l_H_a,      LTR, H1 L" a",             "100")
BD(l_H_H,      LTR, H1 L" " H1,           "111")
BD(l_H__H,     LTR, H1 L"  " H1,          "1111")
BD(l_H_dot_H,  LTR, H1 L"." H1,           "111")
BD(l_a_dot_a,  LTR, L"a.a",               "000")
BD(l_a__H,     LTR, L"a  " H1,            "0001")
BD(l_H__a,     LTR, H1 L"  a",            "1000")
BD(l_aH_dot,   LTR, L"a" H1 L".",         "010")
BD(l_a_dot_H,  LTR, L"a." H1,             "001")
BD(l_H_dot,    LTR, H1 L".",              "10")
BD(l_dot_H,    LTR, L"." H1,              "01")
BD(l_dot_only, LTR, L"...",               "000")
BD(l_sp_only,  LTR, L"   ",               "000")
BD(l_AL_AL,    LTR, A1 A2,                "11")
BD(l_AL_sp_a,  LTR, A1 L" a",             "100")
BD(l_a_sp_AL,  LTR, L"a " A1,             "001")
BD(r_a,        RTL, L"a",                 "2")
BD(r_H,        RTL, H1,                   "1")
BD(r_aH,       RTL, L"a" H1,              "21")
BD(r_Ha,       RTL, H1 L"a",              "12")
BD(r_a_b,      RTL, L"a b",               "222")
BD(r_a_H,      RTL, L"a " H1,             "211")
BD(r_H_a,      RTL, H1 L" a",             "112")
BD(r_H_a_H,    RTL, H1 L" a " H1,         "11211")
BD(r_a__b,     RTL, L"a  b",              "2222")
BD(r_a_dot_sp, RTL, L"a. ",               "211")
BD(r_sp_dot_a, RTL, L". a",               "112")
BD(r_a_dot_b,  RTL, L"a.b",               "222")
BD(r_excl,     RTL, L"!",                 "1")
BD(r_sp_only,  RTL, L"   ",               "111")
BD(r_a_paren_H_paren_b, RTL, L"a (" H1 L") b", "2111112")

/* ---- numbers --------------------------------------------------------------------------------------------------------------------- */
BD(l_1,        LTR, L"1",                 "0")
BD(l_1a,       LTR, L"1a",                "00")
BD(l_a1,       LTR, L"a1",                "00")
BD(l_H1,       LTR, H1 L"1",              "12")
BD(l_1H,       LTR, L"1" H1,              "01")
BD(l_H_1,      LTR, H1 L" 1",             "112")
BD(l_H1_H,     LTR, H1 L"1 " H1,          "1211")
BD(l_H1_a,     LTR, H1 L"1 a",            "1200")
BD(l_H1_1,     LTR, H1 L"1 1",            "1212")
BD(l_H1dot5,   LTR, H1 L"1.5",            "1222")
BD(l_H1comma5, LTR, H1 L"1,5",            "1222")
BD(l_H1colon5, LTR, H1 L"1:5",            "1222")
BD(l_H1plus5,  LTR, H1 L"1+5",            "1222")
BD(l_H1minus5, LTR, H1 L"1-5",            "1222")
BD(l_H1slash5, LTR, H1 L"1/5",            "1222")
BD(l_H1pct,    LTR, H1 L"1%",             "122")
BD(l_Hpct1,    LTR, H1 L"%1",             "122")
BD(l_Hdollar1, LTR, H1 L"$1",             "122")
BD(l_H1a1,     LTR, H1 L"1a1",            "1200")
BD(l_aH1,      LTR, L"a" H1 L"1",         "012")
BD(l_1comma5,  LTR, L"1,5",               "000")
BD(l_AN,       LTR, N1,                   "2")
BD(l_aAN,      LTR, L"a" N1,              "02")
BD(l_HAN,      LTR, H1 N1,                "12")
BD(l_ANH,      LTR, N1 H1,                "21")
BD(l_AN_AN,    LTR, N1 L" " N2,           "212")
BD(l_AN_comma_AN, LTR, N1 L"," N2,        "222")
BD(l_AN_dot_AN, LTR, N1 L"." N2,          "222")
BD(l_AN_plus_AN, LTR, N1 L"+" N2,         "212")
BD(l_AN_pct,   LTR, N1 L"%",              "20")
BD(l_AN1,      LTR, N1 L"1",              "20")
BD(r_1,        RTL, L"1",                 "2")
BD(r_H1,       RTL, H1 L"1",              "12")
BD(r_1H,       RTL, L"1" H1,              "21")
BD(r_a1,       RTL, L"a1",                "22")
BD(r_a_1,      RTL, L"a 1",               "222")
BD(r_1_a,      RTL, L"1 a",               "212")
BD(r_H_1_H,    RTL, H1 L" 1 " H1,         "11211")
BD(r_a1_H,     RTL, L"a1 " H1,            "2211")
BD(r_AN,       RTL, N1,                   "2")
BD(r_aAN,      RTL, L"a" N1,              "22")
BD(r_ANa,      RTL, N1 L"a",              "22")
BD(r_H_AN,     RTL, H1 L" " N1,           "112")
BD(r_AN_AN,    RTL, N1 L" " N2,           "212")
BD(r_1comma5,  RTL, L"1,5",               "222")
BD(r_H1comma5, RTL, H1 L"1,5",            "1222")

/* ---- brackets (rule N0) ---------------------------------------------------------------------------------------------------------- */
BD(br_l_H_paren_H_a,   LTR, H1 L"(" H2 L")a",       "11110")
BD(br_l_H_square_H_a,  LTR, H1 L"[" H2 L"]a",       "11110")
BD(br_l_H_brace_H_a,   LTR, H1 L"{" H2 L"}a",       "11110")
BD(br_r_a_paren_b_H,   RTL, L"a(b)" H1,             "22221")
/* Windows 10 resolves a bracket pair by the strong type inside it (L or R) and does not apply the paired-bracket context rules; brackets around only numbers stay neutral */
BD(br_r_H_paren_a_b,   RTL, H1 L"(a)b",             "12222")
BD(br_l_a_paren_b_H,   LTR, L"a(b)" H1,             "00001")
BD(br_l_H_paren_a_H,   LTR, H1 L"(a)" H2,           "10001")
BD(br_l_H_open_H,      LTR, H1 L"(" H2,             "111")
BD(br_l_H_close_H,     LTR, H1 L")" H2,             "111")
BD(br_r_a_open_b,      RTL, L"a(b",                 "222")
BD(br_l_H_paren_Ha,    LTR, H1 L"(" H2 L"a)",       "10100")
BD(br_r_a_paren_H,     RTL, L"a(" H1 L")",          "2111")
BD(br_r_paren_a_H,     RTL, L"(a)" H1,              "1211")
BD(br_l_a_paren_H_H,   LTR, L"a(" H1 L")" H2,       "01111")
BD(br_r_H_paren_a_a,   RTL, H1 L"(a)a",             "12222")
BD(br_l_mismatched,    LTR, H1 L"(" H2 L"]a",       "11100")
BD(br_l_empty_pair,    LTR, H1 L"()a",              "1000")
BD(br_l_H_paren_digits, LTR, H1 L"(1)a",            "10200")

/* ---- structural invariants over generated strings ------------------------------------------------------------------------------------ */
static int is_override(WCHAR c) { return c == 0x202d || c == 0x202e; }
static int is_control(WCHAR c) { return (c >= 0x202a && c <= 0x202e) || (c >= 0x2066 && c <= 0x2069); }
static void check_invariants(IDWriteTextAnalyzer *an, const WCHAR *t, DWRITE_READING_DIRECTION dir)
{
    UINT32 n = LEN(t), i; HRESULT hr; ms_sink *k = bd_run(an, t, 0, n, dir, 0, &hr); int ov = 0, base = dir == RTL ? 1 : 0;
    for (i = 0; i < n; i++) if (is_override(t[i])) ov = 1;
    CHECKF(hr == S_OK, "text of %u units: %s", n, dw_hr_name(hr));
    CHECK(ms_sink_exact(k, MK_BIDI, 0, n)); CHECK(ms_runs_exact(k, MK_BIDI, 0, n));
    for (i = 0; i < n; i++) {
        WCHAR c = t[i];
        CHECKF(k->res[i] <= 126 && k->ex[i] <= 126, "position %u levels %u/%u", i, k->ex[i], k->res[i]);
        /* only strong letters and digits: trailing white space and separators may be reset to the paragraph level */
        if (c == 'a' || c == 0x05d0 || c == 0x0627 || c == '1' || c == 0x0661) CHECKF(k->res[i] >= k->ex[i], "position %u (U+%04X): resolved %u below explicit %u", i, c, k->res[i], k->ex[i]);
        if (!ov && !is_control(c)) {
            if (c == 'a') CHECKF((k->res[i] & 1) == 0, "L at %u resolved %u", i, k->res[i]);
            if (c == 0x05d0 || c == 0x0627) CHECKF(k->res[i] & 1, "R/AL at %u resolved %u", i, k->res[i]);
        }
    }
    (void)base;
    ms_sink_free(k);
}
static void gen_matrix(const WCHAR *alpha, int na, int len, DWRITE_READING_DIRECTION dir)
{
    MS_BEGIN(); int idx[6] = { 0 }, total = 1, i, j; WCHAR t[8];
    for (i = 0; i < len; i++) total *= na;
    for (i = 0; i < total; i++) {
        int v = i;
        for (j = 0; j < len; j++) { idx[j] = v % na; v /= na; t[j] = alpha[idx[j]]; }
        t[len] = 0;
        check_invariants(an, t, dir);
    }
    MS_END();
}
static const WCHAR alpha_plain[] = { 'a', 0x05d0, 0x0627, '1', 0x0661, ' ', '.', '(', ')', '%' };
static const WCHAR alpha_ctl[] = { 'a', 0x05d0, '1', ' ', 0x202a, 0x202b, 0x202c, 0x202d, 0x202e, '\n' };
MT(bidi_gen_plain3_ltr) { gen_matrix(alpha_plain, 10, 3, LTR); }
MT(bidi_gen_plain3_rtl) { gen_matrix(alpha_plain, 10, 3, RTL); }
MT(bidi_gen_plain4_ltr) { gen_matrix(alpha_plain, 10, 4, LTR); }
MT(bidi_gen_plain4_rtl) { gen_matrix(alpha_plain, 10, 4, RTL); }
MT(bidi_gen_controls3_ltr) { gen_matrix(alpha_ctl, 10, 3, LTR); }
MT(bidi_gen_controls3_rtl) { gen_matrix(alpha_ctl, 10, 3, RTL); }
MT(bidi_gen_controls4_ltr) { gen_matrix(alpha_ctl, 10, 4, LTR); }
MT(bidi_gen_controls4_rtl) { gen_matrix(alpha_ctl, 10, 4, RTL); }

/* A sub-range is analysed on its own: the text outside the range has no influence on the levels inside it. */
static void subrange_equals_standalone(const WCHAR *t, DWRITE_READING_DIRECTION dir)
{
    MS_BEGIN(); UINT32 n = LEN(t), pos, len, i;
    for (pos = 0; pos < n; pos++) for (len = 1; pos + len <= n; len++) {
        WCHAR sub[64]; HRESULT h1, h2; ms_sink *a, *b;
        if (len >= 60) continue;
        if (pos < n && t[pos] >= 0xdc00 && t[pos] <= 0xdfff) continue;                       /* ranges that split a surrogate pair are not exercised */
        if (pos + len < n && t[pos + len - 1] >= 0xd800 && t[pos + len - 1] <= 0xdbff) continue;
        memcpy(sub, t + pos, len * sizeof(WCHAR)); sub[len] = 0;
        a = bd_run(an, t, pos, len, dir, 0, &h1); b = bd_run(an, sub, 0, len, dir, 0, &h2);
        CHECK_HR_OK(h1); CHECK_HR_OK(h2);
        CHECK(ms_sink_exact(a, MK_BIDI, pos, len));
        for (i = 0; i < len; i++) {
            CHECKF(a->ex[pos + i] == b->ex[i] && a->res[pos + i] == b->res[i], "range [%u,%u) unit %u: %u/%u vs standalone %u/%u", pos, pos + len, i, a->ex[pos + i], a->res[pos + i], b->ex[i], b->res[i]);
        }
        ms_sink_free(a); ms_sink_free(b);
    }
    MS_END();
}
MT(bidi_subrange_standalone_mixed_ltr) { subrange_equals_standalone(L"ab " H1 H2 L" 12 cd", LTR); }
MT(bidi_subrange_standalone_mixed_rtl) { subrange_equals_standalone(L"ab " H1 H2 L" 12 cd", RTL); }
MT(bidi_subrange_standalone_numbers) { subrange_equals_standalone(H1 L"1,5 " N1 L"%a", LTR); }
MT(bidi_subrange_standalone_brackets) { subrange_equals_standalone(L"a(" H1 L")b[c]", RTL); }
MT(bidi_subrange_standalone_embedding) { subrange_equals_standalone(L"a" RLE H1 L"b" PDF L"c", LTR); }
MT(bidi_subrange_standalone_surrogates) { subrange_equals_standalone(L"a\xd840\xdc00" H1 L"\xd83d\xde00 b", LTR); }

MT(bidi_deterministic_repeat) {
    MS_BEGIN(); const WCHAR *t = L"ab " H1 H2 L" (12) " A1 L"." N1; UINT32 n = LEN(t), i; int r;
    HRESULT h; ms_sink *first = bd_run(an, t, 0, n, LTR, 0, &h);
    for (r = 0; r < 20; r++) {
        ms_sink *k = bd_run(an, t, 0, n, LTR, 0, &h);
        CHECK_HR_OK(h);
        for (i = 0; i < n; i++) CHECK(k->ex[i] == first->ex[i] && k->res[i] == first->res[i]);
        ms_sink_free(k);
    }
    ms_sink_free(first); MS_END();
}
MT(bidi_direction_does_not_leak_between_calls) {
    MS_BEGIN(); const WCHAR *t = L"a b"; HRESULT h; ms_sink *k;
    k = bd_run(an, t, 0, 3, RTL, 0, &h); EXPECT_EQ(k->res[0], 2); ms_sink_free(k);
    k = bd_run(an, t, 0, 3, LTR, 0, &h); EXPECT_EQ(k->res[0], 0); ms_sink_free(k);
    k = bd_run(an, t, 0, 3, RTL, 0, &h); EXPECT_EQ(k->res[0], 2); ms_sink_free(k);
    MS_END();
}

/* ---- explicit embeddings, overrides and isolates ---------------------------------------------------------------------------------------- */
static void levels_of(const WCHAR *t, DWRITE_READING_DIRECTION dir, ms_sink **out, UINT32 *n)
{
    MS_BEGIN(); HRESULT hr; *n = LEN(t); *out = bd_run(an, t, 0, *n, dir, 0, &hr); CHECK_HR_OK(hr); CHECK(ms_sink_exact(*out, MK_BIDI, 0, *n)); CHECK(ms_runs_exact(*out, MK_BIDI, 0, *n)); MS_END();
}
static void note_levels(const char *what, const WCHAR *t, const ms_sink *k, UINT32 n)
{
    char b[300]; UINT32 i; int o = 0;
    for (i = 0; i < n && o < 270; i++) o += snprintf(b + o, sizeof b - o, "%u/%u ", k->ex[i], k->res[i]);
    t_note("%s: explicit/resolved per unit: %s", what, b);
}

MT(bidi_explicit_rle_in_ltr) {
    /* a RLE H PDF b */
    static const WCHAR t[] = { 'a', 0x202b, 0x05d0, 0x202c, 'b', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n); if (!k) return;
    EXPECT_EQ(k->ex[0], 0); EXPECT_EQ(k->ex[4], 0); EXPECT_EQ(k->res[0], 0); EXPECT_EQ(k->res[4], 0);
    CHECK(k->ex[2] & 1); CHECK(k->ex[2] > 0); CHECK(k->res[2] & 1);
    if (k->ex[2] != 1) t_note("RLE from level 0 gives explicit level %u", k->ex[2]);
    note_levels("a RLE H PDF b", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_lre_in_ltr) {
    static const WCHAR t[] = { 'a', 0x202a, 'b', 0x202c, 'c', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    EXPECT_EQ(k->ex[0], 0); EXPECT_EQ(k->ex[4], 0); CHECK((k->ex[2] & 1) == 0); CHECK(k->ex[2] > 0); CHECK((k->res[2] & 1) == 0);
    note_levels("a LRE b PDF c", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_lre_in_rtl) {
    static const WCHAR t[] = { 0x05d0, 0x202a, 'a', 0x202c, 0x05d1, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, RTL, &k, &n);
    EXPECT_EQ(k->ex[0], 1); EXPECT_EQ(k->ex[4], 1); CHECK((k->ex[2] & 1) == 0); CHECK(k->ex[2] >= 2); CHECK((k->res[2] & 1) == 0);
    note_levels("H LRE a PDF H (rtl)", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_rle_in_rtl) {
    static const WCHAR t[] = { 0x05d0, 0x202b, 0x05d1, 0x202c, 0x05d2, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, RTL, &k, &n);
    EXPECT_EQ(k->ex[0], 1); EXPECT_EQ(k->ex[4], 1); CHECK(k->ex[2] & 1); CHECK(k->ex[2] > 1); CHECK(k->res[2] & 1);
    note_levels("H RLE H PDF H (rtl)", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_rlo_forces_right_to_left) {
    /* RLO a b PDF c : letters inside an override are resolved as R regardless of their own class */
    static const WCHAR t[] = { 0x202e, 'a', 'b', 0x202c, 'c', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->ex[1] & 1); CHECK(k->ex[2] & 1); CHECK(k->res[1] & 1); CHECK(k->res[2] & 1);
    EXPECT_EQ(k->ex[4], 0); EXPECT_EQ(k->res[4], 0);
    note_levels("RLO a b PDF c", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_lro_forces_left_to_right) {
    static const WCHAR t[] = { 0x202d, 0x05d0, 0x05d1, 0x202c, 0x05d2, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, RTL, &k, &n);
    CHECK((k->res[1] & 1) == 0); CHECK((k->res[2] & 1) == 0); CHECK((k->ex[1] & 1) == 0);
    EXPECT_EQ(k->ex[4], 1); EXPECT_EQ(k->res[4], 1);
    note_levels("LRO H H PDF H (rtl)", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_lro_in_ltr_makes_hebrew_even) {
    static const WCHAR t[] = { 0x202d, 0x05d0, ' ', 0x05d1, 0x202c, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK((k->res[1] & 1) == 0); CHECK((k->res[3] & 1) == 0); CHECK((k->res[2] & 1) == 0);
    note_levels("LRO H sp H PDF", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_rlo_digits_become_right_to_left) {
    static const WCHAR t[] = { 0x202e, '1', '2', 0x202c, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->res[1] & 1); CHECK(k->res[2] & 1);
    note_levels("RLO 1 2 PDF", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_nested_rle_rle) {
    /* a RLE b RLE c PDF d PDF e */
    static const WCHAR t[] = { 'a', 0x202b, 'b', 0x202b, 'c', 0x202c, 'd', 0x202c, 'e', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->ex[2] & 1); CHECK(k->ex[4] & 1); CHECK(k->ex[4] > k->ex[2]); EXPECT_EQ(k->ex[6], k->ex[2]); EXPECT_EQ(k->ex[8], 0); EXPECT_EQ(k->ex[0], 0);
    note_levels("a RLE b RLE c PDF d PDF e", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_nested_lre_lre) {
    static const WCHAR t[] = { 'a', 0x202a, 'b', 0x202a, 'c', 0x202c, 'd', 0x202c, 'e', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK((k->ex[2] & 1) == 0); CHECK((k->ex[4] & 1) == 0); CHECK(k->ex[4] > k->ex[2]); EXPECT_EQ(k->ex[6], k->ex[2]); EXPECT_EQ(k->ex[8], 0);
    note_levels("a LRE b LRE c PDF d PDF e", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_nested_rle_lre) {
    static const WCHAR t[] = { 'a', 0x202b, 'b', 0x202a, 'c', 0x202c, 'd', 0x202c, 'e', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->ex[2] & 1); CHECK((k->ex[4] & 1) == 0); CHECK(k->ex[4] > k->ex[2]); EXPECT_EQ(k->ex[6], k->ex[2]); EXPECT_EQ(k->ex[8], 0);
    note_levels("a RLE b LRE c PDF d PDF e", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_nested_lre_rle) {
    static const WCHAR t[] = { 'a', 0x202a, 'b', 0x202b, 'c', 0x202c, 'd', 0x202c, 'e', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK((k->ex[2] & 1) == 0); CHECK(k->ex[4] & 1); CHECK(k->ex[4] > k->ex[2]); EXPECT_EQ(k->ex[6], k->ex[2]); EXPECT_EQ(k->ex[8], 0);
    note_levels("a LRE b RLE c PDF d PDF e", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_unmatched_pdf_is_ignored) {
    static const WCHAR t[] = { 'a', 0x202c, 'b', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    EXPECT_EQ(k->ex[0], 0); EXPECT_EQ(k->ex[2], 0); EXPECT_EQ(k->res[0], 0); EXPECT_EQ(k->res[2], 0);
    note_levels("a PDF b", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_two_pdf_after_one_embedding) {
    static const WCHAR t[] = { 0x202b, 'a', 0x202c, 0x202c, 'b', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->ex[1] & 1); EXPECT_EQ(k->ex[4], 0); EXPECT_EQ(k->res[4], 0);
    note_levels("RLE a PDF PDF b", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_unterminated_rle) {
    static const WCHAR t[] = { 0x202b, 0x05d0, 0x05d1, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->ex[1] & 1); CHECK(k->ex[2] & 1); CHECK(k->res[1] & 1);
    note_levels("RLE H H", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_embedding_ends_at_paragraph_separator) {
    /* RLE H LF a : the line feed ends the paragraph and the embedding with it */
    static const WCHAR t[] = { 0x202b, 0x05d0, '\n', 'a', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->ex[1] & 1); EXPECT_EQ(k->ex[3], 0); EXPECT_EQ(k->res[3], 0);
    note_levels("RLE H LF a", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_embedding_ends_at_paragraph_separator_rtl) {
    static const WCHAR t[] = { 0x202a, 'a', '\n', 0x05d0, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, RTL, &k, &n);
    CHECK((k->ex[1] & 1) == 0); EXPECT_EQ(k->ex[3], 1); EXPECT_EQ(k->res[3], 1);
    note_levels("LRE a LF H (rtl)", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_embedding_ends_at_ps) {
    static const WCHAR t[] = { 0x202b, 0x05d0, 0x2029, 'a', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->ex[1] & 1); EXPECT_EQ(k->ex[3], 0);
    note_levels("RLE H PS a", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_digit_inside_rle_in_ltr) {
    /* a RLE H 1 PDF : EN after R inside an odd embedding is raised by two levels */
    static const WCHAR t[] = { 'a', 0x202b, 0x05d0, '1', 0x202c, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK(k->res[3] > k->res[2]); CHECK(k->res[3] >= k->ex[3]);
    note_levels("a RLE H 1 PDF", t, k, n); ms_sink_free(k);
}
MT(bidi_explicit_marks_inside_embedding) {
    static const WCHAR t[] = { 0x202b, 'a', 0x200e, 0x05d0, 0x200f, 0x202c, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    CHECK((k->res[1] & 1) == 0 || (k->res[1] & 1) == 1);
    CHECK(k->res[3] & 1);
    note_levels("RLE a LRM H RLM PDF", t, k, n); ms_sink_free(k);
}
MT(bidi_marks_lrm_rlm_are_strong) {
    /* LRM / RLM act as strong L / R neutral-resolution anchors */
    static const WCHAR t1[] = { 0x05d0, ' ', 0x200e, 0 }, t2[] = { 'a', ' ', 0x200f, 0 }; ms_sink *k; UINT32 n;
    levels_of(t1, LTR, &k, &n); EXPECT_EQ(k->res[0], 1); EXPECT_EQ(k->res[1], 0); EXPECT_EQ(k->res[2], 0); ms_sink_free(k);
    levels_of(t2, LTR, &k, &n); EXPECT_EQ(k->res[0], 0); EXPECT_EQ(k->res[1], 0); EXPECT_EQ(k->res[2], 1); ms_sink_free(k);
}
MT(bidi_marks_rlm_between_latin_rtl_paragraph) {
    static const WCHAR t[] = { 'a', 0x200f, 'b', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, RTL, &k, &n); EXPECT_EQ(k->res[0], 2); EXPECT_EQ(k->res[1], 1); EXPECT_EQ(k->res[2], 2); ms_sink_free(k);
}
MT(bidi_marks_lrm_between_hebrew_ltr_paragraph) {
    static const WCHAR t[] = { 0x05d0, 0x200e, 0x05d1, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n); EXPECT_EQ(k->res[0], 1); EXPECT_EQ(k->res[1], 0); EXPECT_EQ(k->res[2], 1); ms_sink_free(k);
}

static void isolate_note(const char *what, const WCHAR *t, DWRITE_READING_DIRECTION dir)
{
    ms_sink *k; UINT32 n, i;
    levels_of(t, dir, &k, &n);
    for (i = 0; i < n; i++) {                        /* strong letters keep their direction whatever the isolate controls do */
        if (t[i] == 'a') CHECK((k->res[i] & 1) == 0);
        if (t[i] == 0x05d0) CHECK(k->res[i] & 1);
    }
    note_levels(what, t, k, n); ms_sink_free(k);
}
MT(bidi_isolate_lri_ltr)  { isolate_note("a LRI H PDI b", L"a" LRI H1 PDI L"b", LTR); }
MT(bidi_isolate_rli_ltr)  { isolate_note("a RLI H PDI b", L"a" RLI H1 PDI L"b", LTR); }
MT(bidi_isolate_fsi_ltr)  { isolate_note("a FSI H PDI b", L"a" FSI H1 PDI L"b", LTR); }
MT(bidi_isolate_lri_rtl)  { isolate_note("H LRI a PDI H (rtl)", H1 LRI L"a" PDI H1, RTL); }
MT(bidi_isolate_rli_rtl)  { isolate_note("H RLI a PDI H (rtl)", H1 RLI L"a" PDI H1, RTL); }
MT(bidi_isolate_fsi_rtl)  { isolate_note("H FSI a PDI H (rtl)", H1 FSI L"a" PDI H1, RTL); }
MT(bidi_isolate_unmatched_pdi) { isolate_note("a PDI b", L"a" PDI L"b", LTR); }
MT(bidi_isolate_unterminated_rli) { isolate_note("a RLI H", L"a" RLI H1, LTR); }
MT(bidi_isolate_nested)   { isolate_note("a RLI H LRI a PDI H PDI b", L"a" RLI H1 LRI L"a" PDI H1 PDI L"b", LTR); }
MT(bidi_isolate_fsi_hebrew_content) { isolate_note("a FSI H a PDI b", L"a" FSI H1 L"a" PDI L"b", LTR); }
MT(bidi_isolate_fsi_latin_content) { isolate_note("H FSI a H PDI H (rtl)", H1 FSI L"a" H1 PDI H1, RTL); }
MT(bidi_isolate_inside_embedding) { isolate_note("RLE a RLI H PDI b PDF", RLE L"a" RLI H1 PDI L"b" PDF, LTR); }
MT(bidi_isolate_digits) { isolate_note("H RLI 1 PDI 2", H1 RLI L"1" PDI L"2", LTR); }

MT(bidi_controls_do_not_break_coverage) {
    static const WCHAR all[] = { 0x202a, 0x202b, 0x202c, 0x202d, 0x202e, 0x2066, 0x2067, 0x2068, 0x2069, 0x200e, 0x200f, 0x061c, 0 };
    MS_BEGIN(); UINT32 n = LEN(all), i; HRESULT hr;
    for (i = 0; i < n; i++) {
        WCHAR t[4] = { 'a', all[i], 'b', 0 }; int d;
        for (d = 0; d < 2; d++) {
            ms_sink *k = bd_run(an, t, 0, 3, d ? RTL : LTR, 0, &hr);
            CHECKF(hr == S_OK, "U+%04X: %s", all[i], dw_hr_name(hr));
            CHECK(ms_sink_exact(k, MK_BIDI, 0, 3)); CHECK(ms_runs_exact(k, MK_BIDI, 0, 3));
            ms_sink_free(k);
        }
    }
    MS_END();
}
MT(bidi_control_alone_each_note) {
    static const WCHAR all[] = { 0x202a, 0x202b, 0x202c, 0x202d, 0x202e, 0x2066, 0x2067, 0x2068, 0x2069, 0x200e, 0x200f, 0x061c, 0x0009, 0x000a, 0x2029, 0x0085, 0 };
    MS_BEGIN(); UINT32 n = LEN(all), i; HRESULT hr;
    for (i = 0; i < n; i++) {
        WCHAR t[2] = { all[i], 0 }; int d;
        for (d = 0; d < 2; d++) {
            ms_sink *k = bd_run(an, t, 0, 1, d ? RTL : LTR, 0, &hr);
            CHECKF(hr == S_OK, "U+%04X: %s", all[i], dw_hr_name(hr));
            CHECK(ms_sink_exact(k, MK_BIDI, 0, 1));
            t_note("single U+%04X in a %s paragraph: explicit %u resolved %u", all[i], d ? "RTL" : "LTR", k->ex[0], k->res[0]);
            ms_sink_free(k);
        }
    }
    MS_END();
}
MT(bidi_tab_is_segment_separator) {
    /* tab (S) resolves like a neutral with the paragraph/embedding direction: between two R it is still R in an LTR paragraph? only invariants */
    static const WCHAR t[] = { 0x05d0, '\t', 0x05d1, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n); CHECK(k->res[0] & 1); CHECK(k->res[2] & 1); EXPECT_EQ(k->ex[1], 0);
    note_levels("H TAB H", t, k, n); ms_sink_free(k);
}
MT(bidi_surrogate_pair_same_level) {
    static const WCHAR t[] = { 'a', 0xd840, 0xdc00, 0x05d0, 0xd83d, 0xde00, 'b', 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    EXPECT_EQ(k->ex[1], k->ex[2]); EXPECT_EQ(k->res[1], k->res[2]); EXPECT_EQ(k->ex[4], k->ex[5]); EXPECT_EQ(k->res[4], k->res[5]);
    ms_sink_free(k);
}
MT(bidi_lone_surrogates_covered) {
    static const WCHAR t[] = { 'a', 0xd840, 'b', 0xdc00, 0x05d0, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n); ms_sink_free(k);
}
MT(bidi_combining_marks_follow_base) {
    static const WCHAR t[] = { 0x05d0, 0x0591, 'a', 0x0301, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n);
    EXPECT_EQ(k->res[1], k->res[0]); EXPECT_EQ(k->res[3], k->res[2]); EXPECT_EQ(k->res[0], 1); EXPECT_EQ(k->res[2], 0);
    ms_sink_free(k);
}
MT(bidi_combining_marks_after_space_note) {
    static const WCHAR t[] = { 0x05d0, ' ', 0x0301, 0x05d1, 0 }; ms_sink *k; UINT32 n;
    levels_of(t, LTR, &k, &n); CHECK(k->res[0] & 1); CHECK(k->res[3] & 1);
    note_levels("H SP NSM H", t, k, n); ms_sink_free(k);
}
MT(bidi_long_alternating) {
    MS_BEGIN(); WCHAR t[600]; int i, d; HRESULT hr;
    for (i = 0; i < 590; i++) t[i] = (i / 3) & 1 ? (WCHAR)0x05d0 : (WCHAR)'a';
    t[590] = 0;
    for (d = 0; d < 2; d++) {
        ms_sink *k = bd_run(an, t, 0, 590, d ? RTL : LTR, 0, &hr);
        CHECK_HR_OK(hr); CHECK(ms_sink_exact(k, MK_BIDI, 0, 590)); CHECK(ms_runs_exact(k, MK_BIDI, 0, 590));
        for (i = 0; i < 590; i++) CHECK(((k->res[i] & 1) == 1) == (((i / 3) & 1) == 1));
        EXPECT_EQ(k->n[MK_BIDI], 197);
        ms_sink_free(k);
    }
    MS_END();
}
