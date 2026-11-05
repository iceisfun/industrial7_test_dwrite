/* Incremental tab stops with left-to-right, right-to-left and mixed text.
 *
 * Facts: a tab advances the inline position to the next multiple of the incremental tab stop (measured from the line start,
 * which is the right edge in a right-to-left paragraph); consecutive tabs reach successive stops. A tab cluster is
 * white space; its width is the distance covered. Stops are chosen not to be multiples of the glyph width so the cluster
 * widths are unambiguous.
 */
#include "bx.h"

static void tab_widths(const WCHAR *text, int base, float stop, const float *want, int nwant)
{
    BX_BEGIN(); BX_LAYOUT(l, text, 300.0f, 100.0f); DWRITE_CLUSTER_METRICS cm[16]; UINT32 n, i;
    if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, stop));
    n = bx_clusters(l, cm, 16);
    CHECKF(n == (UINT32)nwant, "%u clusters, want %d", n, nwant);
    for (i = 0; i < n && i < (UINT32)nwant; i++) CHECKF(bx_feq(cm[i].width, want[i]), "cluster %u width %g want %g", i, cm[i].width, want[i]);
    BX_DONE(l);
}

#define TW(nm, text, base, stop, ...) \
    TEST(r2_layout_bidi_reorder_tabs_widths_##nm) { static const float w[] = { __VA_ARGS__ }; tab_widths(text, base, stop, w, (int)(sizeof w / sizeof w[0])); }
TW(latin_50,         L"a\tb",       0, 50.0f,  CW, 50.0f - CW, CW)
TW(latin_100,        L"a\tb",       0, 100.0f, CW, 100.0f - CW, CW)
TW(latin_33,         L"abc\td",     0, 33.0f,  CW, CW, CW, 33.0f - 3 * CW, CW)
TW(latin_past_stop,  L"abcdef\tg",  0, 33.0f,  CW, CW, CW, CW, CW, CW, 66.0f - 6 * CW, CW)          /* 6 chars = 46.9 > 33: next stop is 66 */
TW(latin_two_tabs,   L"a\t\tb",     0, 50.0f,  CW, 50.0f - CW, 50.0f, CW)
TW(latin_three_tabs, L"a\t\t\tb",   0, 40.0f,  CW, 40.0f - CW, 40.0f, 40.0f, CW)
TW(latin_leading_tab,L"\ta",        0, 50.0f,  50.0f, CW)
TW(hebrew_50,        L"\u05d0\t\u05d1", 0, 50.0f, CW, 50.0f - CW, CW)
TW(hebrew_50_rtl,    L"\u05d0\t\u05d1", 1, 50.0f, CW, 50.0f - CW, CW)
TW(hebrew_two_tabs_rtl, L"\u05d0\t\t\u05d1", 1, 50.0f, CW, 50.0f - CW, 50.0f, CW)
TW(latin_rtl_50,     L"a\tb",       1, 50.0f,  CW, 50.0f - CW, CW)
TW(mixed_ltr,        L"ab\t\u05d0\u05d1", 0, 50.0f, CW, CW, 50.0f - 2 * CW, CW, CW)
TW(mixed_rtl,        L"ab\t\u05d0\u05d1", 1, 50.0f, CW, CW, 50.0f - 2 * CW, CW, CW)
TW(hebrew_then_latin_rtl, L"\u05d0\u05d1\tab", 1, 50.0f, CW, CW, 50.0f - 2 * CW, CW, CW)
TW(small_stop_ltr,   L"a\tb",       0, 3.0f,   CW, 9.0f - CW, CW)      /* stops at 3,6,9: first stop past 7.8125 is 9 */
TW(fractional_stop,  L"a\tb",       0, 12.5f,  CW, 12.5f - CW, CW)

/* tab cluster flags */
TEST(r2_layout_bidi_reorder_tabs_cluster_flags)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\t\u05d1", 300.0f, 100.0f); DWRITE_CLUSTER_METRICS cm[4]; UINT32 n;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 50.0f));
    n = bx_clusters(l, cm, 4); CHECK(n == 3);
    if (n == 3) { CHECK(cm[1].isWhitespace); CHECK(!cm[1].isNewline); CHECK(cm[1].length == 1); CHECK(cm[0].isRightToLeft && cm[2].isRightToLeft); CHECK(!cm[0].isWhitespace); }
    BX_DONE(l);
}

/* positions of the character after a tab */
static void tab_pos(const WCHAR *text, int base, float stop, UINT32 pos, float lead_x)
{
    BX_BEGIN(); BX_LAYOUT(l, text, 300.0f, 100.0f); bx_tp p;
    if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, stop));
    p = bx_pos(l, pos, FALSE);
    CHK_NEAR(p.x, lead_x);
    BX_DONE(l);
}
#define TP(nm, text, base, stop, pos, x) TEST(r2_layout_bidi_reorder_tabs_pos_##nm) { tab_pos(text, base, stop, pos, x); }
TP(latin_after_tab,     L"a\tb", 0, 50.0f, 2, 50.0f)
TP(latin_after_two,     L"a\t\tb", 0, 50.0f, 3, 100.0f)
TP(latin_tab_leading,   L"a\tb", 0, 50.0f, 1, CW)
TP(latin_tab_trailing_is_stop, L"a\tb", 0, 50.0f, 2, 50.0f)
/* In an RTL paragraph a Hebrew letter after the tab is at stop distance from the right edge: its leading edge is the right
 * edge of the cluster, 50 DIPs from the line start. */
TP(hebrew_rtl_after_tab,L"\u05d0\t\u05d1", 1, 50.0f, 2, 300.0f - 50.0f)
TP(hebrew_rtl_first,    L"\u05d0\t\u05d1", 1, 50.0f, 0, 300.0f)
TP(hebrew_rtl_two_tabs, L"\u05d0\t\t\u05d1", 1, 50.0f, 3, 300.0f - 100.0f)
TP(hebrew_rtl_100,      L"\u05d0\t\u05d1", 1, 100.0f, 2, 300.0f - 100.0f)

/* total width: stop + last glyph for a tab-separated pair, both directions */
TEST(r2_layout_bidi_reorder_tabs_layout_width)
{
    BX_BEGIN(); BX_LAYOUT(a, L"a\tb", 300.0f, 100.0f); BX_LAYOUT(b, L"\u05d0\t\u05d1", 300.0f, 100.0f); DWRITE_TEXT_METRICS ta, tb;
    IDWriteTextLayout_SetIncrementalTabStop(a, 50.0f); IDWriteTextLayout_SetIncrementalTabStop(b, 50.0f);
    bx_metrics(a, &ta); bx_metrics(b, &tb);
    CHK_NEAR(ta.width, 50.0f + CW); CHK_NEAR(tb.width, 50.0f + CW);
    IDWriteTextLayout_SetReadingDirection(b, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    bx_metrics(b, &tb);
    CHK_NEAR(tb.width, 50.0f + CW); CHK_NEAR(tb.left, 300.0f - 50.0f - CW);
    BX_DONE(a, b);
}

/* changing the tab stop re-lays out */
TEST(r2_layout_bidi_reorder_tabs_change_stop_relayouts)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\t\u05d1", 300.0f, 100.0f); DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetIncrementalTabStop(l, 50.0f); bx_metrics(l, &tm); CHK_NEAR(tm.width, 50.0f + CW);
    IDWriteTextLayout_SetIncrementalTabStop(l, 80.0f); bx_metrics(l, &tm); CHK_NEAR(tm.width, 80.0f + CW);
    CHECK_HR(IDWriteTextLayout_SetIncrementalTabStop(l, 0.0f), DW_E_INVALIDARG);
    bx_metrics(l, &tm); CHK_NEAR(tm.width, 80.0f + CW);
    BX_DONE(l);
}

/* a tab whose stop lies beyond the layout width in a wrapping layout: recorded, the layout stays valid */
TEST(r2_layout_bidi_reorder_tabs_stop_wider_than_layout)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\t\u05d1", 40.0f, 100.0f); DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetIncrementalTabStop(l, 100.0f);
    bx_metrics(l, &tm);
    CHECK(tm.lineCount >= 1);
    t_note("tab stop 100 in a 40-wide RTL layout: %u lines, width %g, left %g", tm.lineCount, tm.width, tm.left);
    BX_DONE(l);
}

/* the tab in a draw: it does not produce a visible glyph run of its own width mismatch (advance sum equals the cluster width) */
TEST(r2_layout_bidi_reorder_tabs_draw_run_widths)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab\t\u05d0\u05d1", 300.0f, 100.0f); dwx_renderer *r; int i; float sum = 0;
    IDWriteTextLayout_SetIncrementalTabStop(l, 50.0f);
    r = bx_draw(l, 0, 0);
    if (r) {
        for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) sum += dwx_run_width(&r->calls[i]);
        CHK_NEAR(sum, 4 * CW);                                      /* the tab is a gap, not a glyph run: ab + two Hebrew letters */
        dwx_release(r);
    }
    BX_DONE(l);
}
