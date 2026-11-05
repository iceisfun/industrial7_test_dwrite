/* Property-style cross checks: the three hit-testing methods must agree with GetClusterMetrics / GetLineMetrics and with each other,
 * for many texts, fonts and alignment settings. Everything here holds for single-direction left-to-right text.
 */
#include "th.h"

static void variants(th_fx *x, IDWriteTextLayout *l, const char *file, int line)
{
    int v;
    for (v = 0; v < 5; v++) {
        switch (v) {
        case 1: IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER); break;
        case 2: IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING); break;
        case 3: IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_LEADING); IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_CENTER); break;
        case 4: IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_NEAR); IDWriteTextLayout_SetMaxWidth(l, 6 * CW); break;
        }
        th_check_consistency(l, file, line);
    }
}

#define CONSIST(name, fam, text, w) \
TEST(prop_consistent_##name) { \
    TH_BEGIN(); TH_LAYOUT_F(l, fam, text, w, 400.0f); \
    variants(&X, l, __FILE__, __LINE__); \
    TH_DONE(l); }

CONSIST(plain,        TH_FAM, L"Hello World", 1000.0f)
CONSIST(digits,       TH_FAM, L"0123456789", 1000.0f)
CONSIST(spaces,       TH_FAM, L"A   B  C", 1000.0f)
CONSIST(leading_ws,   TH_FAM, L"   AB", 1000.0f)
CONSIST(trailing_ws,  TH_FAM, L"AB   ", 1000.0f)
CONSIST(tabs,         TH_FAM, L"A\tB\tC", 1000.0f)
CONSIST(lf_lines,     TH_FAM, L"AB\nCD\nEF\nGH", 1000.0f)
CONSIST(crlf_lines,   TH_FAM, L"AB\r\nCD\r\nEF", 1000.0f)
CONSIST(blank_lines,  TH_FAM, L"A\n\n\nB", 1000.0f)
CONSIST(wrapped,      TH_FAM, L"The quick brown fox jumps over the lazy dog", 12 * CW)
CONSIST(wrapped_tight, TH_FAM, L"AA BB CC DD EE FF GG", 3 * CW)
CONSIST(long_word,    TH_FAM, L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghij", 10 * CW)
CONSIST(surrogates,   TH_FAM, L"A\xd800\xdc00" L"B\xd800\xdc05" L"C", 1000.0f)
CONSIST(combining,    TH_FAM, L"ae\x0301" L"o\x0301" L"b", 1000.0f)
CONSIST(ligature_font, TH_LIG, L"fish fine office fi", 1000.0f)
CONSIST(ligature_wrapped, TH_LIG, L"fifi fifi fifi fifi", 6 * CW)
CONSIST(ellipsis_char, TH_FAM, L"a\x2026" L"b", 1000.0f)
CONSIST(single,       TH_FAM, L"Z", 1000.0f)
CONSIST(line_sep_chars, TH_FAM, L"AB\x2028" L"CD\x2029" L"EF", 1000.0f)
CONSIST(nel_vt_ff,    TH_FAM, L"AB\x0085" L"CD\x000b" L"EF\x000c" L"GH", 1000.0f)

TEST(prop_roundtrip_ascii_every_position)
{
    /* HitTestPoint(HitTestTextPosition(i)) returns i for every position of a simple ASCII layout, with and without wrapping */
    static const float widths[] = { 1000.0f, 20 * CW, 12 * CW, 7 * CW, 4 * CW };
    TH_BEGIN();
    const WCHAR *text = L"The quick brown fox jumps over the lazy dog";
    unsigned wi; UINT32 len = (UINT32)wcslen(text);
    for (wi = 0; wi < sizeof widths / sizeof widths[0]; wi++) {
        IDWriteTextLayout *l = th_layout_(&X, TH_FAM, text, 16.0f, widths[wi], 400.0f);
        UINT32 i;
        if (!l) { TH_END(); t_skip("layout"); return; }
        for (i = 0; i < len; i++) {
            th_tp q = th_pos(l, i, FALSE);
            th_pt p = th_point(l, q.x + 0.1f * CW, q.y + q.m.height * 0.5f);
            CHECKF(p.m.textPosition == i && p.tr == 0 && p.in == 1, "width %g pos %u -> pos %u tr %d in %d", widths[wi], i, p.m.textPosition, (int)p.tr, (int)p.in);
            q = th_pos(l, i, TRUE);
            p = th_point(l, q.x - 0.1f * CW, q.y + q.m.height * 0.5f);
            CHECKF(p.m.textPosition == i && p.tr == 1 && p.in == 1, "width %g pos %u trailing -> pos %u tr %d", widths[wi], i, p.m.textPosition, (int)p.tr);
        }
        IDWriteTextLayout_Release(l);
    }
    TH_END();
}

TEST(prop_exact_caret_x_roundtrips_to_same_position)
{
    TH_BEGIN(); TH_LAYOUT(l, L"abcdefghij", 1000.0f, 200.0f);
    UINT32 i;
    for (i = 0; i < 10; i++) {
        th_tp q = th_pos(l, i, FALSE);
        th_pt p = th_point(l, q.x, q.y + 1.0f);                  /* exactly on the cluster's left edge belongs to the cluster */
        CHK_PT(p, i, 1, 0, 1);
        q = th_pos(l, i, TRUE);
        p = th_point(l, q.x, q.y + 1.0f);                        /* the right edge belongs to the next cluster, except at the very end */
        if (i + 1 < 10) CHK_PT(p, i + 1, 1, 0, 1); else CHK_PT(p, i, 1, 1, 1);
    }
    TH_DONE(l);
}

TEST(prop_range_tiles_every_subrange)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB\nCC DD", 6 * CW, 200.0f);
    UINT32 len = 15, s, n;
    for (s = 0; s <= len; s++)
        for (n = 0; s + n <= len; n += (n < 4 ? 1 : 5)) {
            th_rg r = th_range(l, s, n, 0, 0);
            UINT32 i, at = s, total = 0;
            CHECKF(r.count >= 1, "(%u,%u) no rect", s, n);
            for (i = 0; i < r.count; i++) {
                if (n == 0) break;
                CHECKF(r.m[i].textPosition == at, "(%u,%u) rect %u starts %u expected %u", s, n, i, r.m[i].textPosition, at);
                at += r.m[i].length; total += r.m[i].length;
                CHECKF(r.m[i].length > 0, "(%u,%u) empty rect %u", s, n, i);
            }
            if (n) CHECKF(total == n, "(%u,%u) covered %u", s, n, total);
        }
    TH_DONE(l);
}

TEST(prop_range_rect_count_equals_line_count_for_ltr)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB CCCC DDDD EEEE", 6 * CW, 400.0f);
    UINT32 lines = th_lines(l);
    th_rg r = th_range(l, 0, 24, 0, 0);
    CHECK(r.count == lines);
    UINT32 i;
    for (i = 1; i < r.count; i++) CHECK(r.m[i].top > r.m[i - 1].top);
    TH_DONE(l);
}

TEST(prop_range_width_is_sum_of_cluster_widths)
{
    TH_BEGIN(); TH_LAYOUT(l, L"Hello\tWorld foo", 1000.0f, 200.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 70.0f));
    UINT32 s, n;
    for (s = 0; s < 15; s += 2)
        for (n = 1; s + n <= 15; n += 3) {
            th_rg r = th_range(l, s, n, 0, 0);
            CHECK(r.count == 1);
            if (r.count == 1) CHK_NEAR(r.m[0].width, th_cw_sum(l, s, n));
        }
    TH_DONE(l);
}

TEST(prop_large_text_positions)
{
    TH_BEGIN();
    enum { N = 3000 };
    static WCHAR big[N + 1]; int i;
    for (i = 0; i < N; i++) big[i] = (i % 11 == 10) ? L' ' : (WCHAR)(L'A' + i % 26);
    big[N] = 0;
    IDWriteTextLayout *l = th_layout_(&X, TH_FAM, big, 16.0f, 40 * CW, 100000.0f);
    if (!l) { TH_END(); t_skip("layout"); return; }
    int step[] = { 0, 1, 39, 40, 41, 1000, 1500, 2999 };
    for (i = 0; i < (int)(sizeof step / sizeof step[0]); i++) {
        th_tp q = th_pos(l, step[i], FALSE);
        th_pt p = th_point(l, q.x + 0.1f * CW, q.y + q.m.height * 0.5f);
        CHECKF(p.m.textPosition == (UINT32)step[i], "pos %d -> %u", step[i], p.m.textPosition);
    }
    th_tp e = th_pos(l, N, FALSE);
    CHECK(e.m.textPosition == N && e.m.length == 0);
    CHECK(e.y > 0);
    UINT32 cnt = 0;
    CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0, N, 0, 0, NULL, 0, &cnt), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK(cnt == th_lines(l));
    IDWriteTextLayout_Release(l);
    TH_END();
}
