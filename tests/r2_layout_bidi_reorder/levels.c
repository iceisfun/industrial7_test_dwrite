/* Resolved bidi levels and visual reordering of Hebrew / Arabic / mixed strings.
 *
 * Facts used (TextLayout::GetClusterMetrics, DrawGlyphRuns in dwrite.dll):
 *  - GetClusterMetrics walks the clusters in LOGICAL order and reports isRightToLeft from the odd bit of the span's bidi level;
 *  - every span handed to DrawGlyphRun carries its resolved level in DWRITE_GLYPH_RUN.bidiLevel and a description whose
 *    textPosition/stringLength cover the span's text; the spans of a layout partition the text;
 *  - the levels are the Unicode bidirectional algorithm's (W1-W7, N1-N2, I1-I2) with the paragraph level = reading direction;
 *  - HitTestTextPosition places an odd-level cluster's leading edge at its right edge (reordering rule L2).
 * Rows avoid trailing whitespace (rule L1 resets it to the paragraph level at a line end).
 */
#include "bx.h"

typedef struct { const char *name; int base; const WCHAR *text; const char *lev; } lv_row;

static int is_simple(const WCHAR *t)
{
    for (; *t; t++) {
        WCHAR c = *t;
        if ((c >= 0x300 && c <= 0x36f) || (c >= 0x591 && c <= 0x5bd) || (c >= 0x64b && c <= 0x652) || (c >= 0x200c && c <= 0x200f) || (c >= 0xd800 && c <= 0xdfff)) return 0;
    }
    return 1;
}

static IDWriteTextLayout *lv_make(bx_fx *x, const lv_row *r)
{
    IDWriteTextLayout *l = bx_layout_(x, BX_FAM, r->text, 16.0f, BX_W, 100.0f);
    if (l && r->base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    return l;
}

static void lv_expected(const lv_row *r, UINT8 *e)
{
    int i; for (i = 0; r->lev[i]; i++) e[i] = (UINT8)(r->lev[i] - '0');
}

static void run_draw(const lv_row *r)
{
    BX_BEGIN(); UINT32 n = (UINT32)wcslen(r->text), i; UINT8 e[64], got[64];
    IDWriteTextLayout *l = lv_make(&X, r);
    if (!l) { BX_END(); t_skip("layout"); return; }
    CHECK(strlen(r->lev) == n);
    lv_expected(r, e);
    dwx_renderer *rd = bx_draw(l, 0, 0);
    if (rd) {
        CHECK(bx_draw_levels(rd, n, got));                       /* the runs partition the text */
        for (i = 0; i < n; i++) CHECKF(got[i] == e[i], "text position %u: DrawGlyphRun bidiLevel %u want %u", i, got[i], e[i]);
        dwx_release(rd);
    }
    BX_DONE(l);
}

static void run_cluster(const lv_row *r)
{
    BX_BEGIN(); UINT32 n = (UINT32)wcslen(r->text), i, nc, pos = 0; UINT8 e[64]; DWRITE_CLUSTER_METRICS cm[64];
    IDWriteTextLayout *l = lv_make(&X, r);
    if (!l) { BX_END(); t_skip("layout"); return; }
    lv_expected(r, e);
    nc = bx_clusters(l, cm, 64);
    CHECK(nc >= 1 && nc <= n);
    for (i = 0; i < nc; i++) {
        CHECK(cm[i].length >= 1);
        CHECKF(cm[i].isRightToLeft == (e[pos] & 1), "cluster %u at text %u: isRightToLeft %u want %u", i, pos, cm[i].isRightToLeft, e[pos] & 1);
        pos += cm[i].length;                                       /* logical order: each cluster starts where the previous ended */
    }
    CHECK(pos == n);
    if (is_simple(r->text)) { CHECK(nc == n); for (i = 0; i < nc; i++) CHK_NEAR(cm[i].width, CW); }
    BX_DONE(l);
}

static void run_hit(const lv_row *r)
{
    BX_BEGIN(); UINT32 n = (UINT32)wcslen(r->text), i; UINT8 e[64];
    IDWriteTextLayout *l = lv_make(&X, r);
    if (!l) { BX_END(); t_skip("layout"); return; }
    lv_expected(r, e);
    for (i = 0; i < n; i++) {
        bx_tp q = bx_pos(l, i, FALSE);
        CHECKF(q.m.bidiLevel == e[i], "HitTestTextPosition(%u).bidiLevel %u want %u", i, q.m.bidiLevel, e[i]);
        CHECK(q.m.textPosition <= i && i < q.m.textPosition + q.m.length);
    }
    BX_DONE(l);
}

static void run_visual(const lv_row *r)
{
    BX_BEGIN(); UINT32 n = (UINT32)wcslen(r->text), i, s, inv[64], vis[64]; UINT8 e[64], rl[64]; float off;
    IDWriteTextLayout *l = lv_make(&X, r);
    if (!l) { BX_END(); t_skip("layout"); return; }
    if (!is_simple(r->text)) { BX_DONE(l); t_skip("string has zero-width clusters"); return; }
    lv_expected(r, e);
    bx_ref_bidi(r->text, n, r->base, rl, vis);
    for (i = 0; i < n; i++) CHECKF(rl[i] == e[i], "reference level %u at %u want %u", rl[i], i, e[i]);
    for (s = 0; s < n; s++) inv[vis[s]] = s;
    off = r->base ? BX_W - n * CW : 0.0f;                          /* leading alignment of an RTL paragraph is the right edge */
    for (i = 0; i < n; i++) {
        float left = off + inv[i] * CW; int odd = e[i] & 1;
        bx_tp a = bx_pos(l, i, FALSE), b = bx_pos(l, i, TRUE);
        CHK_NEAR(a.m.left, left); CHK_NEAR(a.m.width, CW);
        CHK_NEAR(a.x, odd ? left + CW : left);                     /* leading hit: the edge the reading direction of the cluster starts at */
        CHK_NEAR(b.x, odd ? left : left + CW);
    }
    {   bx_tp end = bx_pos(l, n, FALSE); int odd = e[n - 1] & 1; float left = off + inv[n - 1] * CW;
        CHK_NEAR(end.x, odd ? left : left + CW); }                  /* end of text = trailing edge of the last logical cluster */
    BX_DONE(l);
}

#define LV(nm, base, text, lev) \
    static const lv_row row_##nm = { #nm, base, text, lev }; \
    TEST(r2_layout_bidi_reorder_levels_draw_##nm)    { run_draw(&row_##nm); } \
    TEST(r2_layout_bidi_reorder_levels_cluster_##nm) { run_cluster(&row_##nm); } \
    TEST(r2_layout_bidi_reorder_levels_hit_##nm)     { run_hit(&row_##nm); } \
    TEST(r2_layout_bidi_reorder_levels_visual_##nm)  { run_visual(&row_##nm); }

LV(latin_ltr,           0, L"abc", "000")
LV(hebrew_ltr,          0, L"\u05d0\u05d1\u05d2", "111")
LV(hebrew_in_latin,     0, L"ab \u05d0\u05d1\u05d2 cd", "000111000")
LV(latin_in_hebrew,     0, L"\u05d0\u05d1\u05d2 abc \u05d3\u05d4\u05d5", "11100000111")
LV(latin_in_hebrew_rtl, 1, L"\u05d0\u05d1\u05d2 abc \u05d3\u05d4\u05d5", "11112221111")
LV(latin_digits_hebrew, 0, L"abc 123 \u05d0\u05d1\u05d2", "00000000111")
LV(hebrew_digits,       0, L"\u05d0\u05d1\u05d2 123", "1111222")
LV(hebrew_digits_rtl,   1, L"\u05d0\u05d1\u05d2 123", "1111222")
LV(arabic_digits,       0, L"\u0627\u0628\u062c 123", "1111222")
LV(latin_rtl,           1, L"abc", "222")
LV(latin_words_rtl,     1, L"abc def", "2222222")
LV(latin_hebrew_rtl,    1, L"abc \u05d0\u05d1\u05d2", "2221111")
LV(punct_l_r,           0, L"abc!\u05d0\u05d1\u05d2", "0000111")
LV(punct_r_l,           0, L"\u05d0\u05d1\u05d2!abc", "1110000")
LV(punct_r_r,           0, L"\u05d0\u05d1\u05d2!\u05d3\u05d4\u05d5", "1111111")
LV(et_after_hebrew,     0, L"\u05d0\u05d1\u05d2$12", "111222")
LV(cs_numbers_ltr,      0, L"1,2", "000")
LV(cs_numbers_rtl,      1, L"1,2", "222")
LV(hebrew_cs_numbers,   0, L"\u05d0\u05d1\u05d2 1,2", "1111222")
LV(cs_between_hebrew,   0, L"\u05d0.\u05d1", "111")
LV(arabic_hebrew,       0, L"\u05d0\u05d1 \u0627\u0628", "11111")
LV(an_after_latin,      0, L"abc \u0661\u0662\u0663", "0000222")
LV(an_after_hebrew,     0, L"\u05d0\u05d1\u05d2\u0661\u0662", "11122")
LV(lrm_in_hebrew,       0, L"\u05d0\u05d1\u200e\u05d3\u05d4", "11011")
LV(rlm_after_latin,     0, L"abc\u200f\u05d0\u05d1", "000111")
LV(leading_space_hebrew,0, L" \u05d0\u05d1", "011")
LV(leading_space_rtl,   1, L" abc", "1222")
LV(trailing_latin,      0, L"\u05d0\u05d1a", "110")
LV(digits_ltr,          0, L"12", "00")
LV(digits_rtl,          1, L"12", "22")
LV(hebrew_percent,      0, L"\u05d0\u05d1 5%", "11122")
LV(hebrew_minus_digit,  0, L"\u05d0\u05d1-5", "1112")
LV(combining_latin,     0, L"a\u0301b", "000")
LV(hebrew_point,        0, L"\u05d0\u05b0\u05d1", "111")
LV(arabic_mark,         0, L"\u0627\u064e\u0628", "111")
LV(cjk_ltr,             0, L"\u4e00\u4e01", "00")
LV(cjk_rtl,             1, L"\u4e00\u4e01", "22")
LV(cjk_between_hebrew,  1, L"\u05d0\u4e00\u05d1", "121")
LV(surrogate_rtl_char,  0, L"\U00010800\U00010801", "1111")
LV(surrogate_in_latin,  0, L"a\U00010800b", "0110")
LV(three_runs_rtl,      1, L"\u05d0 ab \u05d1 cd \u05d2", "11221112211")
LV(hebrew_latin_hebrew_tight, 0, L"\u05d0a\u05d1", "101")
LV(latin_hebrew_latin_tight,  0, L"a\u05d0b", "010")
LV(arabic_latin_digits, 0, L"\u0627a1", "100")
LV(hebrew_digits_hebrew,0, L"\u05d0 12 \u05d1", "112211")

/* ---- generated: every 3-symbol string over a small alphabet, both paragraph directions -------------------------- */
static const WCHAR alpha[] = { L'a', 0x05d0, 0x0627, L'1', 0x0663, L' ', L'!', L',', L'$' };
#define NALPHA ((int)(sizeof alpha / sizeof alpha[0]))

static void exhaustive(int base, int len)
{
    BX_BEGIN(); int total = 1, k, i, bad = 0, done = 0;
    for (i = 0; i < len; i++) total *= NALPHA;
    for (k = 0; k < total; k++) {
        WCHAR t[8]; int v = k; UINT8 rl[8], got[8]; UINT32 vis[8];
        for (i = 0; i < len; i++) { t[i] = alpha[v % NALPHA]; v /= NALPHA; }
        t[len] = 0;
        if (t[len - 1] == L' ') continue;                          /* rule L1 */
        IDWriteTextLayout *l = bx_layout_(&X, BX_FAM, t, 16.0f, BX_W, 100.0f);
        if (!l) { bad++; continue; }
        if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        bx_ref_bidi(t, len, base, rl, vis);
        dwx_renderer *rd = bx_draw(l, 0, 0);
        if (rd) {
            int ok = bx_draw_levels(rd, len, got);
            for (i = 0; i < len; i++) if (!ok || got[i] != rl[i]) { if (bad++ < 5) t_note("level mismatch base=%d k=%d pos %d got %u want %u", base, k, i, got[i], rl[i]); break; }
            dwx_release(rd);
        }
        IDWriteTextLayout_Release(l); done++;
    }
    CHECKF(bad == 0, "%d strings differ from the reference algorithm (of %d)", bad, done);
    BX_END();
}
TEST(r2_layout_bidi_reorder_levels_exhaustive3_ltr) { exhaustive(0, 3); }
TEST(r2_layout_bidi_reorder_levels_exhaustive3_rtl) { exhaustive(1, 3); }

static void exhaustive_visual(int base)
{
    BX_BEGIN(); int len = 3, total = NALPHA * NALPHA * NALPHA, k, i, bad = 0;
    for (k = 0; k < total; k++) {
        WCHAR t[8]; int v = k; UINT8 rl[8]; UINT32 vis[8], inv[8], s; float off;
        for (i = 0; i < len; i++) { t[i] = alpha[v % NALPHA]; v /= NALPHA; }
        t[len] = 0;
        if (t[len - 1] == L' ') continue;
        IDWriteTextLayout *l = bx_layout_(&X, BX_FAM, t, 16.0f, BX_W, 100.0f);
        if (!l) { bad++; continue; }
        if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        bx_ref_bidi(t, len, base, rl, vis);
        for (s = 0; s < (UINT32)len; s++) inv[vis[s]] = s;
        off = base ? BX_W - len * CW : 0.0f;
        for (i = 0; i < len; i++) {
            float px, py; DWRITE_HIT_TEST_METRICS m;
            HRESULT hr = IDWriteTextLayout_HitTestTextPosition(l, i, FALSE, &px, &py, &m);
            if (hr != S_OK || !bx_feq(m.left, off + inv[i] * CW)) { if (bad++ < 5) t_note("visual mismatch base=%d k=%d pos %d hr %s left %g want %g", base, k, i, dw_hr_name(hr), hr == S_OK ? m.left : -1.0f, off + inv[i] * CW); break; }
        }
        IDWriteTextLayout_Release(l);
    }
    CHECKF(bad == 0, "%d strings laid out in a different visual order than rule L2", bad);
    BX_END();
}
TEST(r2_layout_bidi_reorder_levels_exhaustive3_visual_ltr) { exhaustive_visual(0); }
TEST(r2_layout_bidi_reorder_levels_exhaustive3_visual_rtl) { exhaustive_visual(1); }
