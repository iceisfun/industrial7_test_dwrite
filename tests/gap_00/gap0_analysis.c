/* IDWriteTextAnalysisSource / IDWriteTextAnalysisSink (+ Source1 / Sink1) as driven by IDWriteTextAnalyzer.
 *
 * The source and sink are the application's objects, so every expectation here is about what dwrite.dll asks of them and reports to them
 * (observed on Windows 10 build 19041):
 *   - NULL source / NULL sink / textPosition + textLength overflowing 32 bits => E_INVALIDARG before any callback;
 *   - GetParagraphReadingDirection is asked once per AnalyzeBidi / AnalyzeNumberSubstitution call and never by script or line-break analysis;
 *   - the first GetTextAtPosition of AnalyzeScript is at textPosition and none is made outside [textPosition, textPosition + textLength];
 *   - sink reports are ascending, contiguous, non-overlapping and cover exactly the requested range; line breakpoints arrive in ONE call;
 *   - a failing callback ends the walk with a failure code (a success code such as S_FALSE does not);
 *   - neither source nor sink is retained after the call returns.
 * The matrix tests run these invariants over many texts, windows and source chunk sizes. */
#include "gap0.h"

#define K_SCRIPT 1
#define K_BIDI 2
#define K_BREAK 3
#define K_NUMSUB 4

typedef struct { const WCHAR *t; int rtl; } gtext;
static const gtext g_texts[] = {
    { L"Hello world", 0 },
    { L"abc 123 def", 0 },
    { L"a1b2c3", 0 },
    { L"\x05d0\x05d1\x05d2 abc \x0628\x0629", 1 },
    { L"a\xd840\xdc00" L"b\xd83d\xde00" L"c", 0 },
    { L"\x3053\x3093\x306b\x3061\x306f", 0 },
    { L"line1\nline2\r\nline3", 0 },
    { L"tab\tsep  spaced", 0 },
    { L"\x0e01\x0e02\x0e03 thai 42", 0 },
    { L"e\x0301\x0301x \x03b1\x03b2\x03b3", 0 },
    { L"1.5,2% 3/4 -7", 0 },
    { L"\x0915\x0916 \x0410\x0411\x0412 end", 0 },
    { L"x", 0 },
    { L"\x00e9t\x00e9 in Paris, 2024-11-04.", 0 },
};
static const UINT32 g_chunks[] = { 0, 1, 3, 7 };

static HRESULT run_kind(IDWriteTextAnalyzer *an, int kind, dwx_ana_src *s, UINT32 pos, UINT32 len, dwx_ana_sink *k)
{
    IDWriteTextAnalysisSource *src = (IDWriteTextAnalysisSource *)s; IDWriteTextAnalysisSink *snk = (IDWriteTextAnalysisSink *)k;
    switch (kind) {
    case K_SCRIPT: return IDWriteTextAnalyzer_AnalyzeScript(an, src, pos, len, snk);
    case K_BIDI: return IDWriteTextAnalyzer_AnalyzeBidi(an, src, pos, len, snk);
    case K_NUMSUB: return IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, src, pos, len, snk);
    default: return IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, src, pos, len, snk);
    }
}
#define AN_OPEN() IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; \
    if (!f || FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) { t_skip("CreateTextAnalyzer unavailable"); return; }
#define AN_CLOSE() DW_RELEASE(an)

static int is_hi(WCHAR c) { return c >= 0xD800 && c < 0xDC00; }
static int is_lo(WCHAR c) { return c >= 0xDC00 && c < 0xE000; }
/* Window [pos, pos+len) that never starts or ends between the halves of a surrogate pair. */
static void window(const WCHAR *t, UINT32 n, int w, UINT32 *pos, UINT32 *len)
{
    UINT32 a, b;
    switch (w) {
    case 0: a = 0; b = n; break;
    case 1: a = n > 1 ? 1 : 0; b = n; break;
    case 2: a = 0; b = n > 1 ? n - 1 : n; break;
    case 3: a = n / 3; b = a + (n / 3 ? n / 3 : 1); break;
    default: a = n > 1 ? n - 1 : 0; b = n; break;
    }
    if (b > n) b = n;
    if (a > 0 && a < n && is_lo(t[a]) && is_hi(t[a - 1])) a--;
    if (b < n && b > 0 && is_lo(t[b]) && is_hi(t[b - 1])) b++;
    if (b <= a) b = a + 1 > n ? n : a + 1;
    *pos = a; *len = b - a;
}
#define NWIN 5
/* Texts with surrogate pairs are excluded from the chunk-invariance comparison: a chunk boundary may fall between the halves of a pair. */
static int has_sur(const WCHAR *t) { for (; *t; t++) if (is_hi(*t)) return 1; return 0; }

/* Runs of one family: ascending, contiguous, covering [pos, pos+len). */
typedef struct { UINT32 pos, len; } prun;
static int runs_cover(const prun *r, int n, UINT32 pos, UINT32 len)
{
    UINT32 at = pos; int i;
    if (n < 1 || n > 256) return 0;
    for (i = 0; i < n; i++) { if (r[i].pos != at || r[i].len == 0) return 0; at += r[i].len; }
    return at == pos + len;
}
static int set_exact(const unsigned char *set, UINT32 pos, UINT32 len)
{
    UINT32 i;
    for (i = 0; i < DWX_ANA_MAX; i++) if ((set[i] != 0) != (i >= pos && i < pos + len)) return 0;
    return 1;
}

/* ---- direct use of the source object ----------------------------------------------------------------------------------------------- */
TEST(gap0_source_get_locale_name_direct)
{
    static const WCHAR t[] = L"abcdef";
    dwx_ana_src *s = dwx_ana_src_new(t, 6); UINT32 pos;
    lstrcpyW(s->locale, L"fr-FR");
    for (pos = 0; pos <= 8; pos++) {
        UINT32 len = 99; const WCHAR *loc = NULL;
        CHECK_HR(IDWriteTextAnalysisSource_GetLocaleName((IDWriteTextAnalysisSource *)s, pos, &len, &loc), S_OK);
        CHECK(loc != NULL && !lstrcmpW(loc, L"fr-FR"));
        CHECKF(len == (pos < 6 ? 6 - pos : 0), "pos %u: run length %u", pos, len);
    }
    EXPECT_EQ(s->n_locale, 9);
    dwx_release(s);
}

/* ---- the callback protocol ------------------------------------------------------------------------------------------------------ */
TEST(gap0_first_script_call_is_text_at_start)
{
    AN_OPEN();
    int i;
    for (i = 0; i < G_N(g_texts); i++) {
        const WCHAR *t = g_texts[i].t; UINT32 n = G_LEN(t), pos, len;
        dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
        window(t, n, 3, &pos, &len);
        CHECK_HR(run_kind(an, K_SCRIPT, s, pos, len, k), S_OK);
        CHECKF(s->log_n > 0 && s->log[0].fn == 1 && s->log[0].pos == pos, "text %d: first source call fn=%d pos=%u want GetTextAtPosition(%u)", i, s->log_n ? s->log[0].fn : -1, s->log_n ? s->log[0].pos : 0, pos);
        dwx_release(s); dwx_release(k);
    }
    AN_CLOSE();
}
TEST(gap0_reading_direction_asked_once_per_call)
{
    AN_OPEN();
    int i, kind; static const int want[] = { 0, 0, 1, 0, 1 };      /* indexed by K_*: script 0, bidi 1, break 0, numsub 1 */
    for (kind = K_SCRIPT; kind <= K_NUMSUB; kind++)
        for (i = 0; i < G_N(g_texts); i++) {
            const WCHAR *t = g_texts[i].t; UINT32 n = G_LEN(t);
            dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
            s->chunk = 2;
            CHECK_HR(run_kind(an, kind, s, 0, n, k), S_OK);
            CHECKF(s->n_dir == (kind == K_BIDI || kind == K_NUMSUB ? 1 : 0), "kind %d text %d: GetParagraphReadingDirection x%ld", kind, i, s->n_dir);
            dwx_release(s); dwx_release(k);
        }
    (void)want;
    AN_CLOSE();
}
TEST(gap0_reading_direction_value_does_not_change_call_count)
{
    AN_OPEN();
    static const DWRITE_READING_DIRECTION dirs[] = { DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT };
    int d;
    for (d = 0; d < 2; d++) {
        static const WCHAR t[] = L"abc \x05d0\x05d1 123";
        dwx_ana_src *s = dwx_ana_src_new(t, G_LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new();
        s->dir = dirs[d];
        CHECK_HR(run_kind(an, K_BIDI, s, 0, G_LEN(t), k), S_OK);
        CHECK(s->n_dir == 1);
        CHECK(runs_cover((const prun *)k->bidi_runs, k->nrun_bidi, 0, G_LEN(t)));
        /* the paragraph direction decides the base level: even for LTR, odd for RTL */
        CHECKF((k->bidi_explicit[0] & 1) == (UINT8)d, "dir %d: explicit level of the first character %u", d, k->bidi_explicit[0]);
        dwx_release(s); dwx_release(k);
    }
    AN_CLOSE();
}
TEST(gap0_sources_and_sinks_not_retained)
{
    AN_OPEN();
    int kind, i;
    for (kind = K_SCRIPT; kind <= K_NUMSUB; kind++)
        for (i = 0; i < G_N(g_texts); i++) {
            const WCHAR *t = g_texts[i].t; UINT32 n = G_LEN(t);
            dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
            CHECK_HR(run_kind(an, kind, s, 0, n, k), S_OK);
            CHECKF(dwx_refs(s) == 1 && dwx_refs(k) == 1, "kind %d text %d: refs src %ld sink %ld", kind, i, (long)dwx_refs(s), (long)dwx_refs(k));
            dwx_release(s); dwx_release(k);
            CHECK(dwx_destroyed(s) && dwx_destroyed(k));
        }
    AN_CLOSE();
}
TEST(gap0_source_not_queried_for_interfaces_before_use)
{
    /* The analyzer is handed the interface pointer it needs: whether it ever calls QueryInterface on it is a note, never a requirement. */
    AN_OPEN();
    static const WCHAR t[] = L"abc 123";
    int kind;
    for (kind = K_SCRIPT; kind <= K_NUMSUB; kind++) {
        dwx_ana_src *s = dwx_ana_src_new(t, G_LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new();
        CHECK_HR(run_kind(an, kind, s, 0, G_LEN(t), k), S_OK);
        t_note("kind %d: QueryInterface calls on source %ld, on sink %ld", kind, (long)dwx_qi_calls(s), (long)dwx_qi_calls(k));
        dwx_release(s); dwx_release(k);
    }
    AN_CLOSE();
}

/* ---- matrices: text x window x chunk -------------------------------------------------------------------------------------------- */
static void matrix_script(IDWriteTextAnalyzer *an)
{
    int i, w, c;
    for (i = 0; i < G_N(g_texts); i++)
        for (w = 0; w < NWIN; w++) {
            const WCHAR *t = g_texts[i].t; UINT32 n = G_LEN(t), pos, len; dwx_ana_sink *first = NULL;
            window(t, n, w, &pos, &len);
            for (c = 0; c < G_N(g_chunks); c++) {
                dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new(); int j;
                s->chunk = g_chunks[c];
                CHECK_HR(run_kind(an, K_SCRIPT, s, pos, len, k), S_OK);
                CHECKF(runs_cover((const prun *)k->script_runs, k->nrun_script, pos, len), "text %d win %d chunk %u: runs do not cover [%u,%u)", i, w, g_chunks[c], pos, pos + len);
                CHECKF(set_exact(k->script_set, pos, len) && !k->overlap && !k->overflow, "text %d win %d chunk %u: positions reported outside / twice", i, w, g_chunks[c]);
                CHECK(k->n_script == k->nrun_script);
                for (j = 0; j < s->log_n; j++) if (s->log[j].fn == 1) CHECKF(s->log[j].pos >= pos && s->log[j].pos < pos + len, "GetTextAtPosition(%u) outside [%u,%u)", s->log[j].pos, pos, pos + len);
                if (!first) first = k;
                else if (has_sur(t)) dwx_release(k);
                else {
                    /* the chunk size the source chooses never changes the result */
                    for (j = (int)pos; j < (int)(pos + len); j++)
                        if (first->script[j].script != k->script[j].script || first->script[j].shapes != k->script[j].shapes) { CHECKF(0, "text %d win %d chunk %u: script differs at %d", i, w, g_chunks[c], j); break; }
                    dwx_release(k);
                }
                dwx_release(s);
            }
            if (first) dwx_release(first);
        }
}
TEST(gap0_matrix_script) { AN_OPEN(); matrix_script(an); AN_CLOSE(); }

TEST(gap0_matrix_bidi)
{
    AN_OPEN();
    int i, w, c;
    for (i = 0; i < G_N(g_texts); i++)
        for (w = 0; w < NWIN; w++) {
            const WCHAR *t = g_texts[i].t; UINT32 n = G_LEN(t), pos, len; dwx_ana_sink *first = NULL;
            window(t, n, w, &pos, &len);
            for (c = 0; c < G_N(g_chunks); c++) {
                dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new(); UINT32 j;
                s->chunk = g_chunks[c];
                CHECK_HR(run_kind(an, K_BIDI, s, pos, len, k), S_OK);
                CHECKF(runs_cover((const prun *)k->bidi_runs, k->nrun_bidi, pos, len), "text %d win %d chunk %u: bidi runs do not cover [%u,%u)", i, w, g_chunks[c], pos, pos + len);
                CHECKF(set_exact(k->bidi_set, pos, len) && !k->overlap && !k->overflow, "text %d win %d chunk %u: bidi positions outside / twice", i, w, g_chunks[c]);
                CHECK(s->n_dir == 1);
                for (j = pos; j < pos + len; j++) {
                    CHECKF(k->bidi_explicit[j] <= 125 && k->bidi_resolved[j] <= 126, "text %d pos %u: levels %u/%u out of range", i, j, k->bidi_explicit[j], k->bidi_resolved[j]);
                    if (!g_texts[i].rtl) CHECKF(k->bidi_explicit[j] == 0 && k->bidi_resolved[j] == 0, "text %d pos %u: LTR-only text has levels %u/%u", i, j, k->bidi_explicit[j], k->bidi_resolved[j]);
                }
                if (!first) first = k;
                else if (has_sur(t)) dwx_release(k);
                else {
                    for (j = pos; j < pos + len; j++)
                        if (first->bidi_explicit[j] != k->bidi_explicit[j] || first->bidi_resolved[j] != k->bidi_resolved[j]) { CHECKF(0, "text %d win %d chunk %u: levels differ at %u", i, w, g_chunks[c], j); break; }
                    dwx_release(k);
                }
                dwx_release(s);
            }
            if (first) dwx_release(first);
        }
    AN_CLOSE();
}
static int has_cr(const WCHAR *t) { for (; *t; t++) if (*t == 0x0d) return 1; return 0; }
TEST(gap0_matrix_line_breakpoints)
{
    AN_OPEN();
    int i, w, c;
    for (i = 0; i < G_N(g_texts); i++)
        for (w = 0; w < NWIN; w++) {
            const WCHAR *t = g_texts[i].t; UINT32 n = G_LEN(t), pos, len; dwx_ana_sink *first = NULL;
            window(t, n, w, &pos, &len);
            for (c = 0; c < G_N(g_chunks); c++) {
                dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new(); UINT32 j;
                s->chunk = g_chunks[c];
                CHECK_HR(run_kind(an, K_BREAK, s, pos, len, k), S_OK);
                CHECKF(k->n_break == 1, "text %d win %d chunk %u: %ld SetLineBreakpoints calls", i, w, g_chunks[c], k->n_break);
                CHECKF(runs_cover((const prun *)k->break_runs, k->nrun_break, pos, len), "text %d win %d: break run != [%u,%u)", i, w, pos, pos + len);
                CHECKF(set_exact(k->brk_set, pos, len) && !k->overlap && !k->overflow, "text %d win %d: breakpoints outside the range", i, w);
                CHECK(s->n_dir == 0);
                /* adjacent characters: the "after" condition of one is the "before" condition of the next unless the boundary belongs to neighbouring text */
                for (j = pos; j + 1 < pos + len; j++) CHECKF(k->brk[j].breakConditionAfter == k->brk[j + 1].breakConditionBefore, "text %d pos %u: boundary conditions disagree", i, j);
                if (!first) first = k;
                else if (has_sur(t) || has_cr(t)) dwx_release(k);   /* a CR's break conditions depend on whether its LF arrives in the same chunk */
                else {
                    for (j = pos; j < pos + len; j++)
                        if (first->brk[j].breakConditionBefore != k->brk[j].breakConditionBefore || first->brk[j].breakConditionAfter != k->brk[j].breakConditionAfter) { CHECKF(0, "text %d win %d chunk %u: breakpoints differ at %u", i, w, g_chunks[c], j); break; }
                    dwx_release(k);
                }
                dwx_release(s);
            }
            if (first) dwx_release(first);
        }
    AN_CLOSE();
}
static int has_digit(const WCHAR *t, UINT32 pos, UINT32 len) { UINT32 i; for (i = pos; i < pos + len; i++) if (t[i] >= '0' && t[i] <= '9') return 1; return 0; }
TEST(gap0_matrix_number_substitution)
{
    AN_OPEN();
    IDWriteNumberSubstitution *ns = NULL; int i, w;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", TRUE, &ns));
    for (i = 0; i < G_N(g_texts); i++)
        for (w = 0; w < NWIN; w++) {
            const WCHAR *t = g_texts[i].t; UINT32 n = G_LEN(t), pos, len, j;
            dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
            window(t, n, w, &pos, &len);
            s->subst = ns;
            CHECK_HR(run_kind(an, K_NUMSUB, s, pos, len, k), S_OK);
            CHECK(s->n_dir == 1);
            if (!has_digit(t, pos, len)) CHECKF(k->n_subst == 0, "text %d win %d: %ld substitution reports for digit-free text", i, w, k->n_subst);
            else CHECKF(k->n_subst >= 1, "text %d win %d: no substitution report for digits", i, w);
            for (j = 0; j < DWX_ANA_MAX; j++) if (k->subst_set[j]) {
                CHECKF(j >= pos && j < pos + len, "text %d win %d: report at %u outside [%u,%u)", i, w, j, pos, pos + len);
                CHECK(k->subst[j] == ns);
            }
            dwx_release(s); dwx_release(k);
        }
    EXPECT_REFS(ns, 1);               /* every reference the source handed out was released again */
    DW_RELEASE(ns);
    AN_CLOSE();
}

/* ---- failure injection --------------------------------------------------------------------------------------------------------- */
static const HRESULT g_codes[] = { E_FAIL, E_OUTOFMEMORY, E_ABORT, E_NOTIMPL, E_INVALIDARG, (HRESULT)0x88985001, (HRESULT)0x80ABCDEF };
TEST(gap0_sink_failures_end_the_walk)
{
    AN_OPEN();
    static const WCHAR t[] = L"abc \x05d0\x05d1 123 \x03b1\x03b2";
    int kind, c;
    for (kind = K_SCRIPT; kind <= K_NUMSUB; kind++)
        for (c = 0; c < G_N(g_codes); c++) {
            IDWriteNumberSubstitution *ns = NULL;
            dwx_ana_src *s = dwx_ana_src_new(t, G_LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
            if (kind == K_NUMSUB) { IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", TRUE, &ns); s->subst = ns; }
            if (kind == K_SCRIPT) k->fail_script = g_codes[c];
            if (kind == K_BIDI) k->fail_bidi = g_codes[c];
            if (kind == K_BREAK) k->fail_break = g_codes[c];
            if (kind == K_NUMSUB) k->fail_subst = g_codes[c];
            hr = run_kind(an, kind, s, 0, G_LEN(t), k);
            CHECKF(FAILED(hr), "kind %d code %s: analyzer returned %s", kind, dw_hr_name(g_codes[c]), dw_hr_name(hr));
            if (hr != g_codes[c]) t_note("kind %d: sink failed with %s, analyzer returned %s", kind, dw_hr_name(g_codes[c]), dw_hr_name(hr));
            CHECKF(dwx_refs(s) == 1 && dwx_refs(k) == 1, "kind %d: objects retained after a failure", kind);
            dwx_release(s); dwx_release(k); DW_RELEASE(ns);
        }
    AN_CLOSE();
}
TEST(gap0_sink_success_codes_do_not_abort)
{
    AN_OPEN();
    static const WCHAR t[] = L"abc \x05d0\x05d1 123 \x03b1\x03b2";
    static const HRESULT ok[] = { S_FALSE, (HRESULT)1, (HRESULT)0x00040001 };
    int kind, c;
    for (kind = K_SCRIPT; kind <= K_BREAK; kind++)
        for (c = 0; c < G_N(ok); c++) {
            dwx_ana_src *s = dwx_ana_src_new(t, G_LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
            if (kind == K_SCRIPT) k->fail_script = ok[c];
            if (kind == K_BIDI) k->fail_bidi = ok[c];
            if (kind == K_BREAK) k->fail_break = ok[c];
            hr = run_kind(an, kind, s, 0, G_LEN(t), k);
            CHECKF(SUCCEEDED(hr), "kind %d success code %#lx: analyzer returned %s", kind, (long)ok[c], dw_hr_name(hr));
            dwx_release(s); dwx_release(k);
        }
    AN_CLOSE();
}
TEST(gap0_source_failures_end_the_walk)
{
    AN_OPEN();
    static const WCHAR t[] = L"abc 123";
    int c;
    for (c = 0; c < G_N(g_codes); c++) {
        IDWriteNumberSubstitution *ns = NULL;
        dwx_ana_src *s = dwx_ana_src_new(t, G_LEN(t)); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
        s->fail_locale = g_codes[c];
        hr = run_kind(an, K_BIDI, s, 0, G_LEN(t), k);
        CHECKF(FAILED(hr), "GetLocaleName %s: analyzer returned %s", dw_hr_name(g_codes[c]), dw_hr_name(hr));
        CHECK(k->n_bidi == 0);
        dwx_release(s); dwx_release(k);

        IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", TRUE, &ns);
        s = dwx_ana_src_new(t, G_LEN(t)); k = dwx_ana_sink_new();
        s->subst = ns; s->fail_subst = g_codes[c];
        hr = run_kind(an, K_NUMSUB, s, 0, G_LEN(t), k);
        CHECKF(FAILED(hr), "GetNumberSubstitution %s: analyzer returned %s", dw_hr_name(g_codes[c]), dw_hr_name(hr));
        CHECK(k->n_subst == 0);
        dwx_release(s); dwx_release(k); DW_RELEASE(ns);
    }
    AN_CLOSE();
}

/* ---- invalid arguments are rejected before any callback ---------------------------------------------------------------------------- */
TEST(gap0_invalid_ranges_touch_no_callback)
{
    AN_OPEN();
    if (t_is_wine()) { AN_CLOSE(); t_skip("emulator walks the text before validating the range (crashes)"); return; }
    static const WCHAR t[] = L"abcdef";
    static const struct { UINT32 pos, len; } bad[] = { { 0xFFFFFFFFu, 1 }, { 1, 0xFFFFFFFFu }, { 0x80000000u, 0x80000000u }, { 0xFFFFFFF0u, 0x20 }, { 0xFFFFFFFFu, 0xFFFFFFFFu } };
    int kind, b;
    for (kind = K_SCRIPT; kind <= K_NUMSUB; kind++)
        for (b = 0; b < G_N(bad); b++) {
            dwx_ana_src *s = dwx_ana_src_new(t, 6); dwx_ana_sink *k = dwx_ana_sink_new();
            HRESULT hr = run_kind(an, kind, s, bad[b].pos, bad[b].len, k);
            CHECKF(hr == E_INVALIDARG, "kind %d range (%u,%u): %s", kind, bad[b].pos, bad[b].len, dw_hr_name(hr));
            CHECK(s->log_n == 0);
            CHECK(k->n_script == 0 && k->n_break == 0 && k->n_bidi == 0 && k->n_subst == 0);
            dwx_release(s); dwx_release(k);
        }
    AN_CLOSE();
}

/* ---- IDWriteTextAnalysisSource1 / IDWriteTextAnalysisSink1 -------------------------------------------------------------------------- */
static IDWriteTextAnalyzer1 *an1_open(void)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *a = NULL; IDWriteTextAnalyzer1 *a1 = NULL;
    if (!f || FAILED(IDWriteFactory_CreateTextAnalyzer(f, &a)) || !a) return NULL;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&a1))) a1 = NULL;
    DW_RELEASE(a);
    return a1;
}
TEST(gap0_orientation_matrix)
{
    IDWriteTextAnalyzer1 *a1 = an1_open();
    static const DWRITE_VERTICAL_GLYPH_ORIENTATION ori[] = { DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED };
    int i, o, c;
    if (!a1) { t_skip("IDWriteTextAnalyzer1 unavailable"); return; }
    if (t_is_wine()) { DW_RELEASE(a1); t_skip("emulator's AnalyzeVerticalGlyphOrientation is a stub (E_NOTIMPL)"); return; }
    for (i = 0; i < G_N(g_texts); i++)
        for (o = 0; o < 2; o++)
            for (c = 0; c < G_N(g_chunks); c++) {
                const WCHAR *t = g_texts[i].t; UINT32 n = G_LEN(t), j;
                dwx_ana_src *s = dwx_ana_src_new(t, n); dwx_ana_sink *k = dwx_ana_sink_new();
                s->support_v1 = 1; k->support_v1 = 1; s->orientation = ori[o]; s->orient_run = g_chunks[c];
                CHECK_HR(IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(a1, (IDWriteTextAnalysisSource1 *)s, 0, n, (IDWriteTextAnalysisSink1 *)k), S_OK);
                CHECKF(k->n_orient >= 1 && k->n_orient <= (LONG)n, "text %d: %ld SetGlyphOrientation calls", i, k->n_orient);
                CHECKF(s->n_orient >= 1, "text %d: GetVerticalGlyphOrientation never called", i);
                for (j = 0; j < n; j++) {
                    CHECKF(k->orient_set[j] == 1, "text %d: position %u not reported once", i, j);
                    CHECKF((int)k->angle[j] >= 0 && (int)k->angle[j] <= 3, "text %d pos %u: angle %d", i, j, (int)k->angle[j]);
                }
                CHECK(dwx_refs(s) == 1 && dwx_refs(k) == 1);
                dwx_release(s); dwx_release(k);
            }
    DW_RELEASE(a1);
}
TEST(gap0_orientation_invalid_arguments)
{
    IDWriteTextAnalyzer1 *a1 = an1_open();
    static const WCHAR t[] = L"abc";
    dwx_ana_src *s; dwx_ana_sink *k;
    if (!a1) { t_skip("IDWriteTextAnalyzer1 unavailable"); return; }
    if (t_is_wine()) { DW_RELEASE(a1); t_skip("emulator's AnalyzeVerticalGlyphOrientation is a stub (E_NOTIMPL)"); return; }
    s = dwx_ana_src_new(t, 3); k = dwx_ana_sink_new(); s->support_v1 = 1; k->support_v1 = 1;
    CHECK_HR(IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(a1, (IDWriteTextAnalysisSource1 *)s, 0xFFFFFFFFu, 2, (IDWriteTextAnalysisSink1 *)k), E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(a1, (IDWriteTextAnalysisSource1 *)s, 2, 0xFFFFFFFFu, (IDWriteTextAnalysisSink1 *)k), E_INVALIDARG);
    CHECK(s->log_n == 0 && k->n_orient == 0);
    dwx_release(s); dwx_release(k); DW_RELEASE(a1);
}
TEST(gap0_orientation_failures)
{
    IDWriteTextAnalyzer1 *a1 = an1_open();
    static const WCHAR t[] = L"ab";
    int c;
    if (!a1) { t_skip("IDWriteTextAnalyzer1 unavailable"); return; }
    if (t_is_wine()) { DW_RELEASE(a1); t_skip("emulator's AnalyzeVerticalGlyphOrientation is a stub (E_NOTIMPL)"); return; }
    for (c = 0; c < G_N(g_codes); c++) {
        dwx_ana_src *s = dwx_ana_src_new(t, 2); dwx_ana_sink *k = dwx_ana_sink_new(); HRESULT hr;
        s->support_v1 = 1; k->support_v1 = 1; k->fail_orient = g_codes[c];
        hr = IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(a1, (IDWriteTextAnalysisSource1 *)s, 0, 2, (IDWriteTextAnalysisSink1 *)k);
        CHECKF(FAILED(hr), "SetGlyphOrientation %s: analyzer returned %s", dw_hr_name(g_codes[c]), dw_hr_name(hr));
        dwx_release(s); dwx_release(k);
        s = dwx_ana_src_new(t, 2); k = dwx_ana_sink_new();
        s->support_v1 = 1; k->support_v1 = 1; s->fail_orient = g_codes[c];
        hr = IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(a1, (IDWriteTextAnalysisSource1 *)s, 0, 2, (IDWriteTextAnalysisSink1 *)k);
        CHECKF(FAILED(hr), "GetVerticalGlyphOrientation %s: analyzer returned %s", dw_hr_name(g_codes[c]), dw_hr_name(hr));
        CHECK(k->n_orient == 0);
        dwx_release(s); dwx_release(k);
    }
    DW_RELEASE(a1);
}
