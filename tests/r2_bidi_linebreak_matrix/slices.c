/* Source slicing matrix: the text is handed out by GetTextAtPosition / GetTextBeforePosition in pieces that end at fixed absolute
 * boundaries (1, 2, 3, ... units, irregular cycles).  The analysis result must not depend on how the source slices its text:
 * line breakpoints, bidi levels and script runs are compared with the result for a source that returns everything at once, for the
 * whole text and for an interior sub-range.  Surrogate pairs straddle piece boundaries for the odd piece sizes (the analyzer
 * re-fetches at the pair, see TextIterator::Chunk), so they are part of the fixtures. */
#include "ms.h"

typedef struct { ms_src *s; ms_sink *k; HRESULT hr; } sl_run;
static sl_run sl_go(IDWriteTextAnalyzer *an, int kind, const WCHAR *t, UINT32 n, UINT32 pos, UINT32 len, DWRITE_READING_DIRECTION dir, int pat)
{
    sl_run r; r.s = ms_src_new(t, n); r.k = ms_sink_new(0);
    r.s->dir = dir; ms_src_cuts(r.s, ms_patterns[pat], 6);
    r.hr = ms_do(an, kind, r.s, pos, len, r.k);
    return r;
}
static void sl_free(sl_run *r) { ms_src_free(r->s); ms_sink_free(r->k); }

static int events_equal(const ms_sink *a, const ms_sink *b, int kind)
{
    int i, j = 0;
    for (i = 0; i < a->nev; i++) {
        if (a->ev[i].fam != kind) continue;
        while (j < b->nev && b->ev[j].fam != kind) j++;
        if (j >= b->nev) return 0;
        if (a->ev[i].pos != b->ev[j].pos || a->ev[i].len != b->ev[j].len || a->ev[i].ex != b->ev[j].ex || a->ev[i].res != b->ev[j].res) return 0;
        j++;
    }
    while (j < b->nev && b->ev[j].fam != kind) j++;
    return j >= b->nev;
}

static void slice_break_range(const WCHAR *t, UINT32 pos, UINT32 len)
{
    MS_BEGIN(); UINT32 n = LEN(t); int p; sl_run base;
    base = sl_go(an, MA_BREAK, t, n, pos, len, LTR, 0);
    CHECK_HR_OK(base.hr);
    CHECK(base.k->n[MK_BREAK] == 1);
    CHECK(ms_sink_exact(base.k, MK_BREAK, pos, len));
    for (p = 1; p < ms_npatterns; p++) {
        sl_run r = sl_go(an, MA_BREAK, t, n, pos, len, LTR, p);
        UINT32 i; int same = 1;
        CHECKF(r.hr == S_OK, "pattern %d: %s", p, dw_hr_name(r.hr));
        CHECKF(r.k->n[MK_BREAK] == 1, "pattern %d: %d sink calls", p, r.k->n[MK_BREAK]);
        CHECK(ms_sink_exact(r.k, MK_BREAK, pos, len));
        for (i = pos; i < pos + len; i++) {
            unsigned char a = *(unsigned char *)&r.k->brk[i], b = *(unsigned char *)&base.k->brk[i];
            if (a != b) { same = 0; CHECKF(0, "pattern %d position %u: breakpoint byte 0x%02x, whole-text source gives 0x%02x", p, i, a, b); }
        }
        (void)same;
        sl_free(&r);
    }
    sl_free(&base);
    MS_END();
}
/* Interior ranges never split a surrogate pair. */
static void fix_range(const WCHAR *t, UINT32 n, UINT32 *pos, UINT32 *len)
{
    UINT32 e = *pos + *len;
    if (*pos < n && t[*pos] >= 0xdc00 && t[*pos] <= 0xdfff) (*pos)++;
    if (e < n && e > 0 && t[e - 1] >= 0xd800 && t[e - 1] <= 0xdbff) e--;
    *len = e > *pos ? e - *pos : 0;
}
static void slice_break(const WCHAR *t) { UINT32 n = LEN(t); slice_break_range(t, 0, n); }
static void slice_break_sub(const WCHAR *t)
{
    UINT32 n = LEN(t);
    if (n < 4) { t_skip("text too short for an interior range"); return; }
    { UINT32 p = 1, l = n - 2; fix_range(t, n, &p, &l); if (l) slice_break_range(t, p, l); }
    { UINT32 p = 2, l = n - 3; fix_range(t, n, &p, &l); if (l) slice_break_range(t, p, l); }
}

static void slice_bidi_range(const WCHAR *t, UINT32 pos, UINT32 len, DWRITE_READING_DIRECTION dir)
{
    MS_BEGIN(); UINT32 n = LEN(t); int p; sl_run base;
    base = sl_go(an, MA_BIDI, t, n, pos, len, dir, 0);
    CHECK_HR_OK(base.hr);
    CHECK(ms_sink_exact(base.k, MK_BIDI, pos, len));
    CHECK(ms_runs_exact(base.k, MK_BIDI, pos, len));
    for (p = 1; p < ms_npatterns; p++) {
        sl_run r = sl_go(an, MA_BIDI, t, n, pos, len, dir, p);
        UINT32 i;
        CHECKF(r.hr == S_OK, "pattern %d: %s", p, dw_hr_name(r.hr));
        CHECK(ms_sink_exact(r.k, MK_BIDI, pos, len));
        CHECK(ms_runs_exact(r.k, MK_BIDI, pos, len));
        CHECKF(events_equal(r.k, base.k, MK_BIDI), "pattern %d: callback list differs from the whole-text source (%d vs %d calls)", p, r.k->n[MK_BIDI], base.k->n[MK_BIDI]);
        for (i = pos; i < pos + len; i++) {
            CHECKF(r.k->ex[i] == base.k->ex[i], "pattern %d position %u: explicit level %u want %u", p, i, r.k->ex[i], base.k->ex[i]);
            CHECKF(r.k->res[i] == base.k->res[i], "pattern %d position %u: resolved level %u want %u", p, i, r.k->res[i], base.k->res[i]);
        }
        sl_free(&r);
    }
    sl_free(&base);
    MS_END();
}
static void slice_bidi(const WCHAR *t, DWRITE_READING_DIRECTION d) { slice_bidi_range(t, 0, LEN(t), d); }
static void slice_bidi_sub(const WCHAR *t, DWRITE_READING_DIRECTION d)
{
    UINT32 n = LEN(t);
    if (n < 4) { t_skip("text too short for an interior range"); return; }
    { UINT32 p = 1, l = n - 2; fix_range(t, n, &p, &l); if (l) slice_bidi_range(t, p, l, d); }
}

static void slice_script(const WCHAR *t)
{
    MS_BEGIN(); UINT32 n = LEN(t); int p; sl_run base;
    base = sl_go(an, MA_SCRIPT, t, n, 0, n, LTR, 0);
    CHECK_HR_OK(base.hr);
    CHECK(ms_sink_exact(base.k, MK_SCRIPT, 0, n));
    CHECK(ms_runs_exact(base.k, MK_SCRIPT, 0, n));
    for (p = 1; p < ms_npatterns; p++) {
        sl_run r = sl_go(an, MA_SCRIPT, t, n, 0, n, LTR, p);
        UINT32 i;
        CHECKF(r.hr == S_OK, "pattern %d: %s", p, dw_hr_name(r.hr));
        CHECK(ms_sink_exact(r.k, MK_SCRIPT, 0, n));
        CHECK(ms_runs_exact(r.k, MK_SCRIPT, 0, n));
        for (i = 0; i < n; i++) {
            CHECKF(r.k->script[i].script == base.k->script[i].script && r.k->script[i].shapes == base.k->script[i].shapes,
                   "pattern %d position %u: script %u/%d want %u/%d", p, i, r.k->script[i].script, r.k->script[i].shapes, base.k->script[i].script, base.k->script[i].shapes);
        }
        if (r.k->n[MK_SCRIPT] != base.k->n[MK_SCRIPT]) t_note("pattern %d reports %d script runs, whole-text source %d", p, r.k->n[MK_SCRIPT], base.k->n[MK_SCRIPT]);
        sl_free(&r);
    }
    sl_free(&base);
    MS_END();
}

#define MT(n) TEST(r2_bidi_linebreak_matrix_##n)
#define SLICE_TESTS(nm, text) \
    MT(slice_break_##nm)     { slice_break(text); } \
    MT(slice_break_sub_##nm) { slice_break_sub(text); } \
    MT(slice_bidi_ltr_##nm)  { slice_bidi(text, LTR); } \
    MT(slice_bidi_rtl_##nm)  { slice_bidi(text, RTL); } \
    MT(slice_bidi_sub_##nm)  { slice_bidi_sub(text, LTR); } \
    MT(slice_script_##nm)    { slice_script(text); }

SLICE_TESTS(latin,      L"The quick brown fox jumps.")
SLICE_TESTS(spaces,     L"a b  c-d e\tf   g")
SLICE_TESTS(hebrew_mix, L"abc \x05d0\x05d1\x05d2 def 123 \x05d3")
SLICE_TESTS(arabic,     L"\x0627\x0644\x0639 \x0631\x0628\x064a 12 \x0627")
SLICE_TESTS(surrogates, L"a\xd840\xdc00" L"b\xd83d\xde00 c\xd840\xdc01\xd840\xdc02")
SLICE_TESTS(cjk,        L"\x4e2d\x6587\x3002\x300c\x6587\x300d\x3042\x30a2\x30fc")
SLICE_TESTS(numbers,    L"$1,234.56 (50%) a/b 3-4")
SLICE_TESTS(hardbreaks, L"line1\r\nline2\nline3\x2028x\x2029y\x0085z\r")
SLICE_TESTS(combining,  L"e\x0301\x0302 a\x200b" L"b\x200d" L"c\x00ad" L"d")
SLICE_TESTS(scripts,    L"abc\x0391\x0392\x0430\x0431 \x05d0\x0627 \x4e2d\x0e01")
SLICE_TESTS(thai,       L"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35\x0e04\x0e23\x0e31\x0e1a \x0e44\x0e17\x0e22")
SLICE_TESTS(brackets,   L"a(\x05d0)b[c]\x05d1{d}")
SLICE_TESTS(explicit,   L"a\x202b\x05d0\x05d1\x202c" L"b\x202a" L"c\x202c d")
SLICE_TESTS(nbsp,       L"a\x00a0" L"b\x2060" L"c\x202f" L"d e")
SLICE_TESTS(emoji,      L"\xd83d\xde00\xd83d\xde01 \xd83c\xddfa\xd83c\xddf8 x")
SLICE_TESTS(single,     L"abcd")

/* long texts: more pieces than any callback list keeps */
static WCHAR *long_text(int words, int flavour)
{
    WCHAR *t = (WCHAR *)calloc(words * 8 + 8, sizeof(WCHAR)); int i, o = 0;
    for (i = 0; i < words; i++) {
        if (flavour == 1 && i % 5 == 4) { t[o++] = 0x05d0; t[o++] = 0x05d1; } else { t[o++] = 'w'; t[o++] = 'o'; t[o++] = 'r'; t[o++] = 'd'; }
        if (flavour == 2 && i % 3 == 0) { t[o++] = 0x4e2d; t[o++] = 0x6587; }
        t[o++] = (i % 7 == 6) ? '\n' : ' ';
    }
    return t;
}
MT(slice_long_break)  { WCHAR *t = long_text(120, 2); slice_break(t); free(t); }
MT(slice_long_bidi)   { WCHAR *t = long_text(120, 1); slice_bidi(t, LTR); free(t); }
MT(slice_long_bidi_rtl) { WCHAR *t = long_text(120, 1); slice_bidi(t, RTL); free(t); }
MT(slice_long_script) { WCHAR *t = long_text(120, 2); slice_script(t); free(t); }
