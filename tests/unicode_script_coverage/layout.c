/* Unicode text through IDWriteTextLayout: surrogate pairs, invalid UTF-16, combining sequences, separators, hit testing and drawing.
 *
 * Fixture: font "UcTest" in a private collection, size 2048 => every mapped character is exactly 1000 DIPs wide (layout width 100000
 * keeps everything on one line unless a separator breaks it).  Facts relied on: a surrogate pair is never split into two clusters;
 * cluster lengths always add up to the text length; CreateTextLayout accepts any UTF-16 sequence including unpaired surrogates.
 */
#include "uc.h"
#include <math.h>

#define SZ 2048.0f
#define BIGW 100000.0f

static UINT32 cm_get(IDWriteTextLayout *l, DWRITE_CLUSTER_METRICS *m, UINT32 cap, HRESULT *hr)
{
    UINT32 n = 0xFFFFFFFFu; HRESULT h = IDWriteTextLayout_GetClusterMetrics(l, m, cap, &n);
    if (hr) *hr = h;
    return n;
}
static UINT32 sum_len(const DWRITE_CLUSTER_METRICS *m, UINT32 n) { UINT32 i, s = 0; for (i = 0; i < n; i++) s += m[i].length; return s; }
static float sum_w(const DWRITE_CLUSTER_METRICS *m, UINT32 n) { UINT32 i; float s = 0; for (i = 0; i < n; i++) s += m[i].width; return s; }

/* Table: text -> exact cluster length sequence (all characters mapped by the fixture font). */
static void clusters_exact(const WCHAR *t, UINT32 n, const UINT8 *lens, UINT32 nl, int width_each)
{
    UC_FX_BEGIN();
    IDWriteTextLayout *l = uc_layout(&X, t, n, SZ, BIGW, 100.0f); DWRITE_CLUSTER_METRICS m[64]; UINT32 c, i; HRESULT hr;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    c = cm_get(l, m, 64, &hr);
    CHECK_HR_OK(hr);
    CHECKF(c == nl, "%u clusters, want %u", c, nl);
    if (c == nl) for (i = 0; i < c; i++) {
        CHECKF(m[i].length == lens[i], "cluster %u length %u want %u", i, m[i].length, lens[i]);
        if (width_each) CHECKF(m[i].width == 1000.0f, "cluster %u width %g", i, m[i].width);
    }
    CHECK(sum_len(m, c) == n);
    DW_RELEASE(l); UC_FX_END();
}
#define CL_TEST(nm, width_each, ...) TEST(clusters_##nm) { static const WCHAR t[] = { __VA_ARGS__ , 0 }; \
    static const UINT8 L_[] = { 0 }; (void)L_; clusters_exact_wrap(t, width_each); }
/* expected lengths are derived from the text itself: pair => 2, everything else 1 (all fixtures here are mapped, non-combining) */
static void clusters_exact_wrap(const WCHAR *t, int width_each)
{
    UINT8 lens[64]; UINT32 n = LEN(t), i = 0, k = 0;
    while (i < n) {
        if (t[i] >= 0xD800 && t[i] < 0xDC00 && i + 1 < n && t[i + 1] >= 0xDC00 && t[i + 1] < 0xE000) { lens[k++] = 2; i += 2; }
        else { lens[k++] = 1; i++; }
    }
    clusters_exact(t, n, lens, k, width_each);
}
CL_TEST(ascii, 1, 'A', 'B', 'C')
CL_TEST(pair_in_middle, 1, 'A', 0xD83D, 0xDE00, 'B')
CL_TEST(pair_leading, 1, 0xD83D, 0xDE00, 'A')
CL_TEST(pair_trailing, 1, 'A', 0xD83D, 0xDE00)
CL_TEST(pair_only, 1, 0xD83D, 0xDE00)
CL_TEST(three_pairs, 1, 0xD83D, 0xDE00, 0xD800, 0xDC00, 0xDBFF, 0xDFFF)
CL_TEST(pairs_between_letters, 1, 'A', 0xD800, 0xDC00, 'B', 0xD83D, 0xDE00, 'C')
CL_TEST(plane2, 1, 0xD840, 0xDC00)
CL_TEST(plane15_pua, 1, 0xDB80, 0xDC00)
CL_TEST(plane16_last, 1, 0xDBFF, 0xDFFF)
CL_TEST(math_alnum, 1, 0xD835, 0xDC00, 'x')
CL_TEST(bmp_edges, 1, 0xD7FF, 0xE000, 'A')
CL_TEST(pua_f8ff, 1, 'A', 0xF8FF, 'B')
CL_TEST(replacement_char, 1, 'A', 0xFFFD, 'B')
CL_TEST(nbsp, 1, 'A', 0xA0, 'B')
CL_TEST(accented_precomposed, 1, 0xE9, 'A')
CL_TEST(single_pair_between_digits, 1, '1', 0xD83D, 0xDE00, '2')
CL_TEST(long_mixed, 1, 'A', 0xD83D, 0xDE00, 'B', 0xD800, 0xDC00, 'C', 0xE000, 'D', 0xDBFF, 0xDFFF, 'E', 'F')

TEST(pair_metrics_totals)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_TEXT_METRICS tm;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &tm));
    CHECK(tm.lineCount == 1);
    CHECK(tm.width == 3000.0f);
    CHECK(tm.widthIncludingTrailingWhitespace == 3000.0f);
    CHECK(tm.left == 0.0f);
    CHECK(IDWriteTextLayout_GetMaxWidth(l) == BIGW);
    DW_RELEASE(l); UC_FX_END();
}
TEST(pair_cluster_buffer_protocol)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_CLUSTER_METRICS m[4]; UINT32 n = 0; HRESULT hr;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    hr = IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &n);
    CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(n == 3);
    hr = IDWriteTextLayout_GetClusterMetrics(l, m, 2, &n);
    CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(n == 3);
    hr = IDWriteTextLayout_GetClusterMetrics(l, m, 3, &n);
    CHECK_HR_OK(hr); CHECK(n == 3);
    DW_RELEASE(l); UC_FX_END();
}

/* ---- hit testing across a pair ---------------------------------------------------------------------------------------------------------- */
TEST(hit_test_position_pair)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_HIT_TEST_METRICS hm; float x, y;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, 0, FALSE, &x, &y, &hm)); CHECK(x == 0.0f && hm.textPosition == 0 && hm.length == 1 && hm.width == 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, 1, FALSE, &x, &y, &hm)); CHECK(x == 1000.0f && hm.textPosition == 1 && hm.length == 2 && hm.left == 1000.0f && hm.width == 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, 1, TRUE, &x, &y, &hm));  CHECK(x == 2000.0f);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, 3, FALSE, &x, &y, &hm)); CHECK(x == 2000.0f && hm.textPosition == 3 && hm.length == 1);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, 0, TRUE, &x, &y, &hm));  CHECK(x == 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, 3, TRUE, &x, &y, &hm));  CHECK(x == 3000.0f);
    DW_RELEASE(l); UC_FX_END();
}
TEST(hit_test_position_inside_pair_note)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_HIT_TEST_METRICS hm; float x = -1, y = -1;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, 2, FALSE, &x, &y, &hm));
    CHECK(x >= 1000.0f && x <= 2000.0f);
    CHECK(hm.textPosition >= 1 && hm.textPosition <= 2);
    t_note("position 2 (low surrogate), leading: x=%g, metrics textPosition=%u length=%u left=%g width=%g", x, hm.textPosition, hm.length, hm.left, hm.width);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, 2, TRUE, &x, &y, &hm));
    t_note("position 2 (low surrogate), trailing: x=%g, metrics textPosition=%u length=%u", x, hm.textPosition, hm.length);
    DW_RELEASE(l); UC_FX_END();
}
TEST(hit_test_point_pair)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_HIT_TEST_METRICS hm; BOOL trail, inside;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(l, 1250.0f, 10.0f, &trail, &inside, &hm));
    CHECK(inside && !trail && hm.textPosition == 1 && hm.length == 2 && hm.left == 1000.0f && hm.width == 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(l, 1750.0f, 10.0f, &trail, &inside, &hm));
    CHECK(inside && trail && hm.textPosition == 1 && hm.length == 2);
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(l, 1000.0f, 10.0f, &trail, &inside, &hm));
    CHECK(inside && hm.textPosition == 1);
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(l, 2500.0f, 10.0f, &trail, &inside, &hm));
    CHECK(inside && hm.textPosition == 3 && hm.length == 1);
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(l, 50.0f, 10.0f, &trail, &inside, &hm));
    CHECK(inside && !trail && hm.textPosition == 0);
    DW_RELEASE(l); UC_FX_END();
}
TEST(hit_test_point_outside_returns_edge_clusters)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_HIT_TEST_METRICS hm; BOOL trail, inside;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(l, 9000.0f, 10.0f, &trail, &inside, &hm));
    CHECK(!inside && trail && hm.textPosition == 3);
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(l, -500.0f, 10.0f, &trail, &inside, &hm));
    CHECK(!inside && !trail && hm.textPosition == 0);
    DW_RELEASE(l); UC_FX_END();
}
TEST(hit_test_range_over_pair)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_HIT_TEST_METRICS hm[8]; UINT32 n = 0;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(l, 1, 2, 0.0f, 0.0f, hm, 8, &n));
    CHECK(n == 1 && hm[0].textPosition == 1 && hm[0].length == 2 && hm[0].left == 1000.0f && hm[0].width == 1000.0f);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(l, 0, 4, 10.0f, 5.0f, hm, 8, &n));
    CHECK(n == 1 && hm[0].left == 10.0f && hm[0].width == 3000.0f && hm[0].top == 5.0f);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(l, 0, 1, 0.0f, 0.0f, hm, 8, &n));
    CHECK(n == 1 && hm[0].width == 1000.0f);
    DW_RELEASE(l); UC_FX_END();
}
TEST(hit_test_range_half_pair_note)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_HIT_TEST_METRICS hm[8]; UINT32 n = 0; HRESULT hr;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    hr = IDWriteTextLayout_HitTestTextRange(l, 1, 1, 0.0f, 0.0f, hm, 8, &n);
    CHECK_HR_OK(hr);
    if (hr == S_OK && n) t_note("range (1,1) over the high surrogate: %u box(es), first textPosition=%u length=%u left=%g width=%g", n, hm[0].textPosition, hm[0].length, hm[0].left, hm[0].width);
    hr = IDWriteTextLayout_HitTestTextRange(l, 2, 1, 0.0f, 0.0f, hm, 8, &n);
    CHECK_HR_OK(hr);
    if (hr == S_OK && n) t_note("range (2,1) over the low surrogate: %u box(es), first textPosition=%u length=%u left=%g width=%g", n, hm[0].textPosition, hm[0].length, hm[0].left, hm[0].width);
    DW_RELEASE(l); UC_FX_END();
}

/* ---- ranges that split a pair ---------------------------------------------------------------------------------------------------------- */
TEST(range_formats_split_pair_accepted)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_TEXT_RANGE r, q; DWRITE_FONT_WEIGHT w;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    r.startPosition = 2; r.length = 1;
    CHECK_HR_OK(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, r));
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 2, &w, &q));
    CHECK(w == DWRITE_FONT_WEIGHT_BOLD && q.startPosition == 2 && q.length == 1);
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 1, &w, &q));
    CHECK(w == DWRITE_FONT_WEIGHT_NORMAL && q.startPosition == 0 && q.length == 2);
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 3, &w, &q));
    t_note("weight at 3: %d range %u+%u", (int)w, q.startPosition, q.length);
    CHECK(w == DWRITE_FONT_WEIGHT_NORMAL && q.startPosition == 3 && q.length >= 1); /* final range may report an open-ended length */
    {
        DWRITE_CLUSTER_METRICS m[8]; HRESULT hr; UINT32 c = cm_get(l, m, 8, &hr);
        CHECK_HR_OK(hr);
        CHECK(sum_len(m, c) == 4);
        CHECK(c >= 3);
        t_note("pair split by a style range: %u clusters, widths total %g", c, sum_w(m, c));
    }
    DW_RELEASE(l); UC_FX_END();
}
TEST(range_underline_inside_pair_accepted)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_TEXT_RANGE r = { 1, 1 }, q; BOOL u;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, r));
    CHECK_HR_OK(IDWriteTextLayout_GetUnderline(l, 1, &u, &q)); CHECK(u && q.startPosition == 1 && q.length == 1);
    CHECK_HR_OK(IDWriteTextLayout_GetUnderline(l, 2, &u, &q)); CHECK(!u && q.startPosition == 2);
    DW_RELEASE(l); UC_FX_END();
}
TEST(range_pair_whole_style_keeps_cluster)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_TEXT_RANGE r = { 1, 2 }; DWRITE_CLUSTER_METRICS m[8]; UINT32 c; HRESULT hr;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, r));
    c = cm_get(l, m, 8, &hr); CHECK_HR_OK(hr);
    CHECK(c == 3 && m[1].length == 2 && m[1].width == 1000.0f);
    DW_RELEASE(l); UC_FX_END();
}
TEST(range_beyond_text_with_pair)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 100.0f); DWRITE_TEXT_RANGE r = { 3, 2 }, q; BOOL u;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, r));              /* clamps at the end of text */
    CHECK_HR_OK(IDWriteTextLayout_GetUnderline(l, 3, &u, &q)); CHECK(u && q.startPosition == 3);
    DW_RELEASE(l); UC_FX_END();
}

/* ---- invalid UTF-16 ------------------------------------------------------------------------------------------------------------------------- */
static void bad_utf16(const WCHAR *t, UINT32 n, const char *what)
{
    UC_FX_BEGIN();
    IDWriteTextLayout *l = NULL; IDWriteTextFormat *fmt = NULL; HRESULT hr; DWRITE_CLUSTER_METRICS m[16]; UINT32 c; DWRITE_TEXT_METRICS tm;
    DW_REQUIRE_OK(IDWriteFactory_CreateTextFormat(X.f, L"UcTest", X.coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, SZ, L"en-us", &fmt));
    hr = IDWriteFactory_CreateTextLayout(X.f, t, n, fmt, BIGW, 100.0f, &l);
    CHECKF(hr == S_OK, "%s: CreateTextLayout -> %s", what, dw_hr_name(hr));
    DW_RELEASE(fmt);
    if (hr == S_OK && l) {
        CHECK(IDWriteTextLayout_GetMaxWidth(l) == BIGW);
        hr = IDWriteTextLayout_GetMetrics(l, &tm);
        CHECK_HR_OK(hr);
        c = cm_get(l, m, 16, &hr);
        CHECK_HR_OK(hr);
        CHECK(sum_len(m, c) == n);
        t_note("%s: %u clusters, lengths %u %u %u %u, width %g", what, c, c > 0 ? m[0].length : 0, c > 1 ? m[1].length : 0, c > 2 ? m[2].length : 0, c > 3 ? m[3].length : 0, tm.width);
    }
    DW_RELEASE(l); UC_FX_END();
}
TEST(invalid_lone_high_between_letters) { static const WCHAR t[] = L"A\xD83D" L"B"; bad_utf16(t, 3, "A + lone high surrogate + B"); }
TEST(invalid_lone_low_between_letters)  { static const WCHAR t[] = L"A\xDE00" L"B"; bad_utf16(t, 3, "A + lone low surrogate + B"); }
TEST(invalid_reversed_pair)             { static const WCHAR t[] = L"A\xDE00\xD83D" L"B"; bad_utf16(t, 4, "A + low,high + B"); }
TEST(invalid_high_at_end)               { static const WCHAR t[] = L"AB\xD83D"; bad_utf16(t, 3, "text ending in a high surrogate"); }
TEST(invalid_low_at_start)              { static const WCHAR t[] = L"\xDE00" L"AB"; bad_utf16(t, 3, "text starting with a low surrogate"); }
TEST(invalid_only_high)                 { static const WCHAR t[] = L"\xD83D"; bad_utf16(t, 1, "single high surrogate"); }
TEST(invalid_only_low)                  { static const WCHAR t[] = L"\xDE00"; bad_utf16(t, 1, "single low surrogate"); }
TEST(invalid_two_highs)                 { static const WCHAR t[] = L"\xD83D\xD83D\xDE00"; bad_utf16(t, 3, "high, high, low"); }
TEST(invalid_high_low_low)              { static const WCHAR t[] = L"\xD83D\xDE00\xDE00"; bad_utf16(t, 3, "high, low, low"); }
TEST(invalid_many_highs)                { static const WCHAR t[] = L"\xD800\xD801\xD802\xD803\xD804\xD805"; bad_utf16(t, 6, "six high surrogates"); }
TEST(invalid_many_lows)                 { static const WCHAR t[] = L"\xDC00\xDC01\xDC02\xDC03\xDC04\xDC05"; bad_utf16(t, 6, "six low surrogates"); }
TEST(invalid_high_before_newline)       { static const WCHAR t[] = L"A\xD83D\nB"; bad_utf16(t, 4, "high surrogate before LF"); }
TEST(invalid_valid_pair_after_lone)     { static const WCHAR t[] = L"\xD83D\xD83D\xDE00" L"B"; bad_utf16(t, 4, "lone high then a valid pair"); }
TEST(invalid_nul_in_text)               { static const WCHAR t[] = { 'A', 0, 'B' }; bad_utf16(t, 3, "embedded NUL"); }
TEST(invalid_noncharacters)             { static const WCHAR t[] = L"A\xFFFE\xFFFF" L"B"; bad_utf16(t, 4, "U+FFFE U+FFFF"); }
TEST(invalid_c0_controls)               { static const WCHAR t[] = L"A\x01\x02\x1F" L"B"; bad_utf16(t, 5, "C0 controls"); }

TEST(invalid_lone_surrogate_does_not_join_neighbour_pair_note)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"\xDE00\xD83D\xDE00";             /* lone low surrogate followed by a valid pair */
    IDWriteTextLayout *l = uc_layout(&X, t, 3, SZ, BIGW, 100.0f); DWRITE_CLUSTER_METRICS m[8]; UINT32 c; HRESULT hr;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    c = cm_get(l, m, 8, &hr); CHECK_HR_OK(hr);
    CHECK(sum_len(m, c) == 3);
    CHECK(c >= 2);
    CHECKF(m[c - 1].length == 2, "the valid pair is not one cluster (last cluster length %u)", m[c - 1].length);
    DW_RELEASE(l); UC_FX_END();
}

/* ---- normalization insensitivity ----------------------------------------------------------------------------------------------------------- */
TEST(normalization_precomposed_one_cluster)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"\x00E9";
    IDWriteTextLayout *l = uc_layout(&X, t, 1, SZ, BIGW, 100.0f); DWRITE_CLUSTER_METRICS m[4]; UINT32 c; HRESULT hr;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    c = cm_get(l, m, 4, &hr); CHECK_HR_OK(hr);
    CHECK(c == 1 && m[0].length == 1 && m[0].width == 1000.0f);
    DW_RELEASE(l); UC_FX_END();
}
TEST(normalization_decomposed_synthetic_note)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"e\x0301";
    IDWriteTextLayout *l = uc_layout(&X, t, 2, SZ, BIGW, 100.0f); DWRITE_CLUSTER_METRICS m[4]; UINT32 c; HRESULT hr;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    c = cm_get(l, m, 4, &hr); CHECK_HR_OK(hr);
    CHECK(sum_len(m, c) == 2);
    CHECK(c >= 1 && c <= 2);
    t_note("e + U+0301 in the synthetic font: %u cluster(s), widths %g %g, lengths %u %u", c, m[0].width, c > 1 ? m[1].width : 0.0f, m[0].length, c > 1 ? m[1].length : 0);
    DW_RELEASE(l); UC_FX_END();
}
static void sys_cluster_count(const WCHAR *family, const WCHAR *t, UINT32 n, UINT32 *clusters_out, UINT32 *width_out, int *ok)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *l = NULL; DWRITE_CLUSTER_METRICS m[32]; UINT32 c; HRESULT hr;
    *ok = 0;
    if (!f || !dw_have_font(f, family)) return;
    if (FAILED(IDWriteFactory_CreateTextFormat(f, family, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &fmt))) return;
    hr = IDWriteFactory_CreateTextLayout(f, t, n, fmt, 10000.0f, 100.0f, &l);
    DW_RELEASE(fmt);
    t_check(hr == S_OK, __FILE__, __LINE__, "CreateTextLayout", "%s", dw_hr_name(hr));
    if (hr != S_OK) return;
    c = cm_get(l, m, 32, &hr);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetClusterMetrics", "%s", dw_hr_name(hr));
    t_check(sum_len(m, c) == n, __FILE__, __LINE__, "sum of cluster lengths", "%u vs %u", sum_len(m, c), n);
    *clusters_out = c; *width_out = (UINT32)(sum_w(m, c) * 1000.0f); *ok = 1;
    DW_RELEASE(l);
}
TEST(normalization_arial_precomposed_vs_decomposed)
{
    UINT32 c1 = 0, w1 = 0, c2 = 0, w2 = 0; int ok1, ok2;
    sys_cluster_count(L"Arial", L"\x00E9", 1, &c1, &w1, &ok1);
    sys_cluster_count(L"Arial", L"e\x0301", 2, &c2, &w2, &ok2);
    if (!ok1 || !ok2) { t_skip("Arial layout unavailable"); return; }
    CHECK(c1 == 1);
    CHECKF(c2 == 1, "decomposed e + combining acute formed %u clusters with Arial", c2);
    t_note("Arial widths (x1000): precomposed %u decomposed %u", w1, w2);
}
TEST(normalization_arial_multiple_marks_one_cluster)
{
    UINT32 c = 0, w = 0; int ok;
    sys_cluster_count(L"Arial", L"a\x0300\x0301\x0302", 4, &c, &w, &ok);
    if (!ok) { t_skip("Arial layout unavailable"); return; }
    CHECKF(c == 1, "base + 3 marks formed %u clusters", c);
}
TEST(normalization_arial_pair_cluster)
{
    UINT32 c = 0, w = 0; int ok;
    sys_cluster_count(L"Arial", L"A\xD83D\xDE00" L"B", 4, &c, &w, &ok);
    if (!ok) { t_skip("Arial layout unavailable"); return; }
    CHECKF(c == 3, "A + U+1F600 + B formed %u clusters", c);
}
TEST(normalization_segoe_emoji_pair_cluster)
{
    UINT32 c = 0, w = 0; int ok;
    sys_cluster_count(L"Segoe UI Emoji", L"\xD83D\xDE00", 2, &c, &w, &ok);
    if (!ok) { t_skip("Segoe UI Emoji unavailable"); return; }
    CHECK(c == 1);
}
TEST(sequences_emoji_zwj_flag_keycap_note)
{
    static const struct { const char *n; const WCHAR *t; UINT32 len; } rows[] = {
        { "family ZWJ", L"\xD83D\xDC68\x200D\xD83D\xDC69\x200D\xD83D\xDC67", 8 }, { "flag US", L"\xD83C\xDDFA\xD83C\xDDF8", 4 },
        { "keycap 1", L"1\xFE0F\x20E3", 3 }, { "thumbs up + skin tone", L"\xD83D\xDC4D\xD83C\xDFFD", 4 }, { "heart VS16", L"\x2764\xFE0F", 2 },
        { "tag sequence", L"\xD83C\xDFF4\xDB40\xDC67\xDB40\xDC62\xDB40\xDC7F", 8 } };
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        UINT32 c = 0, w = 0; int ok;
        sys_cluster_count(L"Segoe UI Emoji", rows[i].t, rows[i].len, &c, &w, &ok);
        if (ok) t_note("%s: %u cluster(s) in Segoe UI Emoji", rows[i].n, c);
    }
}
TEST(hangul_jamo_and_syllables_cluster_note)
{
    UINT32 c = 0, w = 0; int ok;
    sys_cluster_count(L"Malgun Gothic", L"\x1112\x1161\x11AB", 3, &c, &w, &ok);
    if (ok) t_note("Hangul conjoining jamo L+V+T in Malgun Gothic: %u cluster(s)", c);
    sys_cluster_count(L"Malgun Gothic", L"\xD55C\xAE00", 2, &c, &w, &ok);
    if (ok) CHECK(c == 2);
}
TEST(indic_cluster_note)
{
    UINT32 c = 0, w = 0; int ok;
    sys_cluster_count(L"Nirmala UI", L"\x0915\x094D\x0937", 3, &c, &w, &ok);
    if (ok) t_note("Devanagari KA+VIRAMA+SSA in Nirmala UI: %u cluster(s)", c);
    sys_cluster_count(L"Segoe UI", L"\x0E01\x0E33", 2, &c, &w, &ok);
    if (ok) t_note("Thai KO KAI + SARA AM in Segoe UI: %u cluster(s)", c);
}

/* ---- separators and break characters ----------------------------------------------------------------------------------------------------- */
static void separator(const WCHAR *t, UINT32 n, UINT32 newline_len, int hard, const char *what)
{
    UC_FX_BEGIN();
    IDWriteTextLayout *l = uc_layout(&X, t, n, SZ, BIGW, 1000.0f); DWRITE_LINE_METRICS lm[4]; UINT32 nl = 0; HRESULT hr; DWRITE_TEXT_METRICS tm;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &tm));
    hr = IDWriteTextLayout_GetLineMetrics(l, lm, 4, &nl);
    CHECK_HR_OK(hr);
    if (hard) {
        CHECKF(nl == 2 && tm.lineCount == 2, "%s: %u lines", what, nl);
        if (nl == 2) {
            CHECKF(lm[0].newlineLength == newline_len, "%s: newlineLength %u want %u", what, lm[0].newlineLength, newline_len);
            CHECKF(lm[0].length == 1 + newline_len, "%s: first line length %u", what, lm[0].length);
            CHECK(lm[1].newlineLength == 0 && lm[1].length == 1);
        }
    } else t_note("%s: %u line(s), first line length %u newlineLength %u", what, nl, nl ? lm[0].length : 0, nl ? lm[0].newlineLength : 0);
    DW_RELEASE(l); UC_FX_END();
}
TEST(separator_lf)      { static const WCHAR t[] = L"A\nB"; separator(t, 3, 1, 1, "LF"); }
TEST(separator_cr)      { static const WCHAR t[] = L"A\rB"; separator(t, 3, 1, 1, "CR"); }
TEST(separator_crlf)    { static const WCHAR t[] = L"A\r\nB"; separator(t, 4, 2, 1, "CRLF"); }
TEST(separator_ls)      { static const WCHAR t[] = L"A\x2028" L"B"; separator(t, 3, 1, 1, "U+2028"); }
TEST(separator_ps)      { static const WCHAR t[] = L"A\x2029" L"B"; separator(t, 3, 1, 1, "U+2029"); }
TEST(separator_nel)     { static const WCHAR t[] = L"A\x0085" L"B"; separator(t, 3, 1, 0, "U+0085"); }
TEST(separator_vt)      { static const WCHAR t[] = L"A\x000B" L"B"; separator(t, 3, 1, 0, "VT"); }
TEST(separator_ff)      { static const WCHAR t[] = L"A\x000C" L"B"; separator(t, 3, 1, 0, "FF"); }
TEST(separator_lfcr_two_breaks)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\n\rB";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 1000.0f); DWRITE_TEXT_METRICS tm;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &tm));
    CHECKF(tm.lineCount == 3, "LF CR is two breaks, got %u lines", tm.lineCount);
    DW_RELEASE(l); UC_FX_END();
}
TEST(separator_zwsp_break_opportunity)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"AAAA\x200B" L"BBBBB";
    IDWriteTextLayout *l = uc_layout(&X, t, 10, SZ, 5200.0f, 1000.0f); DWRITE_LINE_METRICS lm[4]; UINT32 nl = 0; DWRITE_TEXT_METRICS tm;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &tm));
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(l, lm, 4, &nl));
    CHECKF(nl == 2, "%u lines (ZWSP is a break opportunity)", nl);
    if (nl == 2) { CHECK(lm[0].length == 5 && lm[1].length == 5); }
    t_note("ZWSP wrap: line lengths %u %u, widths %g", nl > 0 ? lm[0].length : 0, nl > 1 ? lm[1].length : 0, tm.width);
    DW_RELEASE(l); UC_FX_END();
}
TEST(separator_nbsp_prevents_break)
{
    UC_FX_BEGIN();
    /* width 4500 fits four 1000-wide glyphs; a space offers a break after "A ", a no-break space does not */
    static const WCHAR t1[] = L"A AA AA", t2[] = L"A AA\x00A0" L"AA";
    IDWriteTextLayout *a = uc_layout(&X, t1, 7, SZ, 4500.0f, 1000.0f), *b = uc_layout(&X, t2, 7, SZ, 4500.0f, 1000.0f);
    DWRITE_LINE_METRICS la[4], lb[4]; UINT32 na = 0, nb = 0;
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(a, la, 4, &na)); CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(b, lb, 4, &nb));
    t_note("space: %u lines, first len %u; nbsp: %u lines, first len %u", na, na ? la[0].length : 0, nb, nb ? lb[0].length : 0);
    CHECK(na >= 2 && nb >= 2);
    if (na >= 2 && nb >= 2) CHECKF(la[0].length != lb[0].length, "NBSP did not change the break (%u vs %u)", la[0].length, lb[0].length);
    DW_RELEASE(a); DW_RELEASE(b); UC_FX_END();
}
TEST(separator_softhyphen_note)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"AAAA\x00AD" L"BBBB";
    IDWriteTextLayout *l = uc_layout(&X, t, 9, SZ, 4500.0f, 1000.0f); DWRITE_TEXT_METRICS tm;
    if (!l) { UC_FX_END(); t_skip("CreateTextLayout failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &tm));
    t_note("soft hyphen wrap: %u line(s), width %g", tm.lineCount, tm.width);
    DW_RELEASE(l); UC_FX_END();
}

/* ---- drawing: glyph runs for pairs, mirroring in RTL paragraphs ---------------------------------------------------------------------------- */
TEST(draw_pair_single_glyph_run)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 1000.0f); dwx_renderer *r = dwx_renderer_new(); const dwx_draw *d; int cur = 0;
    if (!l || !r) { DW_RELEASE(l); if (r) dwx_release(r); UC_FX_END(); t_skip("fixture failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f));
    d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
    CHECK(d != NULL);
    if (d) {
        CHECK(d->nglyphs == 3);
        CHECK(d->indices[0] == uc_gid(UC_V_BOTH, 'A') && d->indices[1] == uc_gid(UC_V_BOTH, 0x1F600) && d->indices[2] == uc_gid(UC_V_BOTH, 'B'));
        CHECK(d->has_desc && d->text_pos == 0 && d->text_len == 4);
        if (d->has_desc && d->cluster_map) CHECK(d->cluster_map[0] == 0 && d->cluster_map[1] == 1 && d->cluster_map[2] == 1 && d->cluster_map[3] == 2);
        CHECK(d->advances[0] == 1000.0f && d->advances[1] == 1000.0f && d->advances[2] == 1000.0f);
        CHECK(d->bidi == 0);
    }
    CHECK(dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur) == NULL);
    DW_RELEASE(l); dwx_release(r); UC_FX_END();
}
TEST(draw_rtl_paragraph_mirrors_brackets)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"(";
    IDWriteTextLayout *l = uc_layout(&X, t, 1, SZ, BIGW, 1000.0f); dwx_renderer *r = dwx_renderer_new(); const dwx_draw *d; int cur = 0;
    if (!l || !r) { DW_RELEASE(l); if (r) dwx_release(r); UC_FX_END(); t_skip("fixture failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f));
    d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
    CHECK(d != NULL);
    if (d) {
        CHECK(d->nglyphs == 1);
        CHECK((d->bidi & 1) == 1);                                  /* neutral in an RTL paragraph resolves to RTL */
        CHECKF(d->indices[0] == uc_gid(UC_V_BOTH, ')'), "glyph %u want the glyph of ')' (%u)", d->indices[0], uc_gid(UC_V_BOTH, ')'));
    }
    DW_RELEASE(l); dwx_release(r); UC_FX_END();
}
TEST(draw_ltr_paragraph_keeps_brackets)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"(A)";
    IDWriteTextLayout *l = uc_layout(&X, t, 3, SZ, BIGW, 1000.0f); dwx_renderer *r = dwx_renderer_new(); const dwx_draw *d; int cur = 0;
    if (!l || !r) { DW_RELEASE(l); if (r) dwx_release(r); UC_FX_END(); t_skip("fixture failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f));
    d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
    CHECK(d != NULL);
    if (d) { CHECK(d->nglyphs == 3 && d->indices[0] == uc_gid(UC_V_BOTH, '(') && d->indices[2] == uc_gid(UC_V_BOTH, ')') && d->bidi == 0); }
    DW_RELEASE(l); dwx_release(r); UC_FX_END();
}
TEST(draw_rtl_paragraph_every_run_mirrors_by_level)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"(A)[B]";
    IDWriteTextLayout *l = uc_layout(&X, t, 6, SZ, BIGW, 1000.0f); dwx_renderer *r = dwx_renderer_new(); const dwx_draw *d; int cur = 0, runs = 0;
    if (!l || !r) { DW_RELEASE(l); if (r) dwx_release(r); UC_FX_END(); t_skip("fixture failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f));
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
        UINT32 i; runs++;
        if (!d->has_desc || !d->text) { CHECK(0); continue; }
        for (i = 0; i < d->nglyphs; i++) {
            WCHAR c = d->text[i]; unsigned want = uc_gid(UC_V_BOTH, c);
            if (d->bidi & 1) switch (c) { case '(': want = uc_gid(UC_V_BOTH, ')'); break; case ')': want = uc_gid(UC_V_BOTH, '('); break; case '[': want = uc_gid(UC_V_BOTH, ']'); break; case ']': want = uc_gid(UC_V_BOTH, '['); break; }
            CHECKF(d->indices[i] == want, "run bidi %u glyph %u for U+%04X is %u want %u", d->bidi, i, c, d->indices[i], want);
        }
    }
    CHECK(runs >= 1);
    DW_RELEASE(l); dwx_release(r); UC_FX_END();
}
TEST(draw_supplementary_in_rtl_paragraph)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"\xD83D\xDE00";
    IDWriteTextLayout *l = uc_layout(&X, t, 2, SZ, BIGW, 1000.0f); dwx_renderer *r = dwx_renderer_new(); const dwx_draw *d; int cur = 0;
    if (!l || !r) { DW_RELEASE(l); if (r) dwx_release(r); UC_FX_END(); t_skip("fixture failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f));
    d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
    CHECK(d != NULL);
    if (d) { CHECK(d->nglyphs == 1 && d->indices[0] == uc_gid(UC_V_BOTH, 0x1F600)); CHECK(d->text_len == 2); }
    DW_RELEASE(l); dwx_release(r); UC_FX_END();
}
TEST(draw_private_use_and_nonchars_note)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xE000\xF8FF" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 4, SZ, BIGW, 1000.0f); dwx_renderer *r = dwx_renderer_new(); const dwx_draw *d; int cur = 0; UINT32 total = 0;
    if (!l || !r) { DW_RELEASE(l); if (r) dwx_release(r); UC_FX_END(); t_skip("fixture failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f));
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
        if (d->nglyphs == 4 && d->indices) { CHECK(d->indices[1] == uc_gid(UC_V_BOTH, 0xE000) && d->indices[2] == uc_gid(UC_V_BOTH, 0xF8FF)); }
        total += d->nglyphs;
    }
    CHECK(total == 4);
    DW_RELEASE(l); dwx_release(r); UC_FX_END();
}
TEST(draw_lone_surrogate_completes)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"A\xD83D" L"B";
    IDWriteTextLayout *l = uc_layout(&X, t, 3, SZ, BIGW, 1000.0f); dwx_renderer *r = dwx_renderer_new(); const dwx_draw *d; int cur = 0; UINT32 total = 0;
    if (!l || !r) { DW_RELEASE(l); if (r) dwx_release(r); UC_FX_END(); t_skip("fixture failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f));
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) total += d->nglyphs;
    CHECK(total >= 3 || total >= 2);
    t_note("A + lone high surrogate + B drew %u glyph(s) in %d run(s)", total, cur);
    DW_RELEASE(l); dwx_release(r); UC_FX_END();
}

/* ---- text analysis through a layout: script run splitting inside layout output --------------------------------------------------------------- */
TEST(draw_mixed_scripts_split_runs_with_fixture_font)
{
    UC_FX_BEGIN();
    static const WCHAR t[] = L"AB\x0627\x05D0" L"CD";
    IDWriteTextLayout *l = uc_layout(&X, t, 6, SZ, BIGW, 1000.0f); dwx_renderer *r = dwx_renderer_new(); const dwx_draw *d; int cur = 0, runs = 0; UINT32 total = 0;
    if (!l || !r) { DW_RELEASE(l); if (r) dwx_release(r); UC_FX_END(); t_skip("fixture failed"); return; }
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f));
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) { runs++; total += d->nglyphs; }
    CHECK(total == 6);
    CHECKF(runs >= 3, "Latin, Arabic and Hebrew should not share a glyph run (%d)", runs);
    DW_RELEASE(l); dwx_release(r); UC_FX_END();
}
