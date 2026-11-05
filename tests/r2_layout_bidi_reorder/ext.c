/* IDWriteTextLayout2: SetLastLineWrapping and SetOpticalAlignment with bidirectional text.
 *
 * Facts: SetLastLineWrapping stores (value != 0) and invalidates the layout (IDWriteTextLayout::SetLastLineWrapping);
 * SetOpticalAlignment accepts DWRITE_OPTICAL_ALIGNMENT_NONE (0) and _USING_SIDE_BEARINGS (1) only. How the two change
 * the geometry is recorded rather than asserted, except for relations that hold whatever the engine does.
 */
#include "bx.h"

#define HWORDS L"\u05d0\u05d1\u05d2 \u05d3\u05d4\u05d5 \u05d6\u05d7\u05d8"       /* 11 chars */

static UINT32 lines_with(const WCHAR *text, float w, int rtl, BOOL llw)
{
    BX_BEGIN(); BX_LAYOUT(l, text, w, 200.0f); BX_QI(l2, IDWriteTextLayout2, l); DWRITE_TEXT_METRICS tm; UINT32 n;
    if (rtl) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, llw));
    bx_metrics(l, &tm); n = tm.lineCount;
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
    return n;
}

TEST(r2_layout_bidi_reorder_ext_lastline_line_counts_recorded)
{
    float widths[] = { 3.5f * CW, 6.5f * CW, 7.5f * CW, 9.5f * CW }; unsigned i; int rtl;
    for (rtl = 0; rtl < 2; rtl++) for (i = 0; i < 4; i++) {
        UINT32 on = lines_with(HWORDS, widths[i], rtl, TRUE), off = lines_with(HWORDS, widths[i], rtl, FALSE);
        CHECK(on >= 1 && off >= 1);
        CHECK(off <= on);                                              /* turning wrapping of the last line off cannot add lines */
        t_note("hebrew, %s paragraph, width %.2f CW: last-line wrapping on -> %u lines, off -> %u lines", rtl ? "rtl" : "ltr", widths[i] / CW, on, off);
    }
}

TEST(r2_layout_bidi_reorder_ext_lastline_independent_of_direction)
{
    unsigned i; float widths[] = { 3.5f * CW, 6.5f * CW, 9.5f * CW };
    for (i = 0; i < 3; i++) {
        CHECK(lines_with(HWORDS, widths[i], 0, TRUE) == lines_with(HWORDS, widths[i], 1, TRUE));
        CHECK(lines_with(HWORDS, widths[i], 0, FALSE) == lines_with(HWORDS, widths[i], 1, FALSE));
    }
}

TEST(r2_layout_bidi_reorder_ext_lastline_single_line_unaffected)
{
    CHECK(lines_with(L"\u05d0\u05d1\u05d2", 300.0f, 1, TRUE) == 1);
    CHECK(lines_with(L"\u05d0\u05d1\u05d2", 300.0f, 1, FALSE) == 1);
    CHECK(lines_with(L"abc", 300.0f, 0, FALSE) == 1);
}

TEST(r2_layout_bidi_reorder_ext_lastline_multiple_paragraphs)
{
    /* a newline ends a paragraph: lines before it are never "last lines" of the layout */
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 \u05d3\u05d4\u05d5\n\u05d6\u05d7\u05d8 \u05d9\u05db\u05dc", 4.5f * CW, 200.0f); BX_QI(l2, IDWriteTextLayout2, l);
    DWRITE_TEXT_METRICS a, b;
    IDWriteTextLayout2_SetLastLineWrapping(l2, TRUE);  bx_metrics(l, &a);
    IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE); bx_metrics(l, &b);
    CHECK(a.lineCount >= 2 && b.lineCount >= 2 && b.lineCount <= a.lineCount);
    t_note("two paragraphs, narrow: wrapping on %u lines, off %u lines", a.lineCount, b.lineCount);
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_ext_lastline_toggle_roundtrip_layout)
{
    BX_BEGIN(); BX_LAYOUT(l, HWORDS, 6.5f * CW, 200.0f); BX_QI(l2, IDWriteTextLayout2, l); DWRITE_TEXT_METRICS a, b, c;
    bx_metrics(l, &a);
    IDWriteTextLayout2_SetLastLineWrapping(l2, !IDWriteTextLayout2_GetLastLineWrapping(l2)); bx_metrics(l, &b);
    IDWriteTextLayout2_SetLastLineWrapping(l2, !IDWriteTextLayout2_GetLastLineWrapping(l2)); bx_metrics(l, &c);
    CHECK(a.lineCount == c.lineCount); CHK_NEAR(a.width, c.width); CHK_NEAR(a.height, c.height);
    t_note("default last-line wrapping %d; lines %u -> %u -> %u", IDWriteTextLayout2_GetLastLineWrapping(l2), a.lineCount, b.lineCount, c.lineCount);
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_ext_lastline_format_value_inherited)
{
    BX_BEGIN(); IDWriteTextFormat *t = bx_format(&X, BX_FAM, 16.0f); IDWriteTextFormat1 *t1 = NULL; IDWriteTextLayout *l = NULL;
    if (!t || FAILED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1)) || !t1) { if (t) IDWriteTextFormat_Release(t); BX_END(); t_skip("IDWriteTextFormat1"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(t1, FALSE));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, L"\u05d0\u05d1", 2, t, 100.0f, 50.0f, &l));
    if (l) {
        IDWriteTextLayout2 *l2 = NULL;
        if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2)) && l2) {
            CHECK(IDWriteTextLayout2_GetLastLineWrapping(l2) == FALSE);
            IDWriteTextLayout2_Release(l2);
        }
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat1_Release(t1); IDWriteTextFormat_Release(t); BX_END();
}

/* ---- optical alignment ---------------------------------------------------------------------------------------- */
static void optical_metrics(const WCHAR *text, int rtl, DWRITE_TEXT_ALIGNMENT al, DWRITE_TEXT_METRICS *none, DWRITE_TEXT_METRICS *sb, float *x_none, float *x_sb)
{
    BX_BEGIN(); BX_LAYOUT(l, text, 100.0f, 50.0f); BX_QI(l2, IDWriteTextLayout2, l);
    if (rtl) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetTextAlignment(l, al);
    CHECK_HR_OK(IDWriteTextLayout2_SetOpticalAlignment(l2, DWRITE_OPTICAL_ALIGNMENT_NONE));
    bx_metrics(l, none); { dwx_renderer *r = bx_draw(l, 0, 0); *x_none = r && r->ncalls ? r->calls[0].x : 0; if (r) dwx_release(r); }
    CHECK_HR_OK(IDWriteTextLayout2_SetOpticalAlignment(l2, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS));
    bx_metrics(l, sb);   { dwx_renderer *r = bx_draw(l, 0, 0); *x_sb = r && r->ncalls ? r->calls[0].x : 0; if (r) dwx_release(r); }
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
}

typedef struct { const char *nm; const WCHAR *text; int rtl; DWRITE_TEXT_ALIGNMENT al; } op_row;
static void run_optical(const op_row *r)
{
    DWRITE_TEXT_METRICS a, b; float xa, xb;
    optical_metrics(r->text, r->rtl, r->al, &a, &b, &xa, &xb);
    CHECK(a.lineCount == b.lineCount);
    CHECK(b.width > 0.0f);
    t_note("%s: none left %g width %g first run x %g | side-bearings left %g width %g first run x %g (glyph lsb 100/2048 em = %g DIP)",
           r->nm, a.left, a.width, xa, b.left, b.width, xb, 100.0f * 16 / 2048);
}
#define OP(nm, text, rtl, al) static const op_row op_##nm = { #nm, text, rtl, al }; TEST(r2_layout_bidi_reorder_ext_optical_##nm) { run_optical(&op_##nm); }
OP(latin_leading,        L"abc", 0, DWRITE_TEXT_ALIGNMENT_LEADING)
OP(latin_trailing,       L"abc", 0, DWRITE_TEXT_ALIGNMENT_TRAILING)
OP(latin_center,         L"abc", 0, DWRITE_TEXT_ALIGNMENT_CENTER)
OP(hebrew_leading_ltr,   L"\u05d0\u05d1\u05d2", 0, DWRITE_TEXT_ALIGNMENT_LEADING)
OP(hebrew_leading_rtl,   L"\u05d0\u05d1\u05d2", 1, DWRITE_TEXT_ALIGNMENT_LEADING)
OP(hebrew_trailing_rtl,  L"\u05d0\u05d1\u05d2", 1, DWRITE_TEXT_ALIGNMENT_TRAILING)
OP(hebrew_center_rtl,    L"\u05d0\u05d1\u05d2", 1, DWRITE_TEXT_ALIGNMENT_CENTER)
OP(mixed_leading_rtl,    L"ab \u05d0\u05d1\u05d2", 1, DWRITE_TEXT_ALIGNMENT_LEADING)
OP(mixed_trailing_ltr,   L"ab \u05d0\u05d1\u05d2", 0, DWRITE_TEXT_ALIGNMENT_TRAILING)

/* Relations that hold for any engine: NONE leaves the advance geometry of a left-aligned line untouched, and the setting is
 * sticky across layout-wide changes. */
TEST(r2_layout_bidi_reorder_ext_optical_none_matches_default)
{
    BX_BEGIN(); BX_LAYOUT(a, L"ab \u05d0\u05d1\u05d2", 100.0f, 50.0f); BX_LAYOUT(b, L"ab \u05d0\u05d1\u05d2", 100.0f, 50.0f); BX_QI(b2, IDWriteTextLayout2, b);
    DWRITE_TEXT_METRICS ma, mb;
    CHECK_HR_OK(IDWriteTextLayout2_SetOpticalAlignment(b2, DWRITE_OPTICAL_ALIGNMENT_NONE));
    bx_metrics(a, &ma); bx_metrics(b, &mb);
    CHK_NEAR(ma.left, mb.left); CHK_NEAR(ma.width, mb.width); CHK_NEAR(ma.height, mb.height);
    IDWriteTextLayout2_Release(b2); BX_DONE(a, b);
}

TEST(r2_layout_bidi_reorder_ext_optical_sticky_across_setters)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2", 100.0f, 50.0f); BX_QI(l2, IDWriteTextLayout2, l);
    CHECK_HR_OK(IDWriteTextLayout2_SetOpticalAlignment(l2, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS));
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP);
    CHECK(IDWriteTextLayout2_GetOpticalAlignment(l2) == DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_ext_optical_format_value_inherited)
{
    BX_BEGIN(); IDWriteTextFormat *t = bx_format(&X, BX_FAM, 16.0f); IDWriteTextFormat1 *t1 = NULL; IDWriteTextLayout *l = NULL;
    if (!t || FAILED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&t1)) || !t1) { if (t) IDWriteTextFormat_Release(t); BX_END(); t_skip("IDWriteTextFormat1"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetOpticalAlignment(t1, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS));
    CHECK_HR(IDWriteTextFormat1_SetOpticalAlignment(t1, (DWRITE_OPTICAL_ALIGNMENT)2), DW_E_INVALIDARG);
    CHECK(IDWriteTextFormat1_GetOpticalAlignment(t1) == DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, L"\u05d0\u05d1", 2, t, 100.0f, 50.0f, &l));
    if (l) {
        IDWriteTextLayout2 *l2 = NULL;
        if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2)) && l2) {
            CHECK(IDWriteTextLayout2_GetOpticalAlignment(l2) == DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
            IDWriteTextLayout2_Release(l2);
        }
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat1_Release(t1); IDWriteTextFormat_Release(t); BX_END();
}

/* GetMetrics1: IDWriteTextLayout2::GetMetrics fills DWRITE_TEXT_METRICS1 whose first fields agree with GetMetrics */
TEST(r2_layout_bidi_reorder_ext_metrics1_agrees_rtl)
{
    BX_BEGIN(); BX_LAYOUT(l, HWORDS, 6.5f * CW, 200.0f); BX_QI(l2, IDWriteTextLayout2, l); DWRITE_TEXT_METRICS a; DWRITE_TEXT_METRICS1 b;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    bx_metrics(l, &a);
    memset(&b, 0xcc, sizeof b);
    CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l2, &b));
    CHK_NEAR(a.left, b.left); CHK_NEAR(a.top, b.top); CHK_NEAR(a.width, b.width); CHK_NEAR(a.height, b.height);
    CHK_NEAR(a.widthIncludingTrailingWhitespace, b.widthIncludingTrailingWhitespace);
    CHECK(a.lineCount == b.lineCount && a.maxBidiReorderingDepth == b.maxBidiReorderingDepth);
    CHECK(b.heightIncludingTrailingWhitespace >= b.height - EPS);
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
}

/* maxBidiReorderingDepth: 1 for text without reordering, > 1 for a line that mixes directions; other shapes are recorded. */
typedef struct { const char *nm; const WCHAR *text; int base; int mode; /* 0: == 1, 1: > 1, 2: record */ } dp_row;
static void run_depth(const dp_row *r)
{
    BX_BEGIN(); BX_LAYOUT(l, r->text, BX_W, 100.0f); DWRITE_TEXT_METRICS tm;
    if (r->base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    bx_metrics(l, &tm);
    CHECK(tm.maxBidiReorderingDepth >= 1);
    if (r->mode == 0) CHECKF(tm.maxBidiReorderingDepth == 1, "maxBidiReorderingDepth %u want 1", tm.maxBidiReorderingDepth);
    else if (r->mode == 1) CHECKF(tm.maxBidiReorderingDepth > 1, "maxBidiReorderingDepth %u want > 1", tm.maxBidiReorderingDepth);
    else t_note("%s: maxBidiReorderingDepth %u", r->nm, tm.maxBidiReorderingDepth);
    BX_DONE(l);
}
#define DP(nm, text, base, mode) static const dp_row dp_##nm = { #nm, text, base, mode }; TEST(r2_layout_bidi_reorder_ext_depth_##nm) { run_depth(&dp_##nm); }
DP(latin_ltr,        L"abc", 0, 0)
DP(single_char,      L"a", 0, 0)
DP(space_only,       L" ", 0, 0)
DP(digits_ltr,       L"123", 0, 0)
DP(mixed_ltr,        L"ab \u05d0\u05d1", 0, 1)
DP(mixed_rtl,        L"ab \u05d0\u05d1", 1, 1)
DP(hebrew_latin_hebrew, L"\u05d0 ab \u05d1", 0, 1)
DP(hebrew_ltr,       L"\u05d0\u05d1\u05d2", 0, 2)
DP(hebrew_rtl,       L"\u05d0\u05d1\u05d2", 1, 2)
DP(latin_rtl,        L"abc", 1, 2)
DP(digits_in_hebrew, L"\u05d0\u05d1 123", 0, 2)
DP(digits_in_hebrew_rtl, L"\u05d0\u05d1 123", 1, 2)
DP(nested,           L"a \u05d0 12 \u05d1 b", 0, 2)
