/* Flow direction, vertical reading directions and glyph orientation.
 *
 * Facts: SetReadingDirection / SetFlowDirection accept all four enum values independently (DWriteTextLayout setters);
 * documented valid pairings are horizontal reading (LTR/RTL) with a vertical flow (TOP_TO_BOTTOM/BOTTOM_TO_TOP) and vertical
 * reading (TOP_TO_BOTTOM/BOTTOM_TO_TOP) with a horizontal flow (LEFT_TO_RIGHT/RIGHT_TO_LEFT). In a vertical layout
 * DrawGlyphRun reports isSideways for glyphs that are rotated 90 degrees; DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED keeps
 * every glyph upright, DEFAULT rotates glyphs of scripts that are written sideways (Latin).
 */
#include "bx.h"

static const char *rd_name[] = { "ltr", "rtl", "ttb", "btt" };
static const char *fd_name[] = { "ttb", "btt", "ltr", "rtl" };

static void combo(int rd, int fd, int valid)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", 200.0f, 200.0f); DWRITE_TEXT_METRICS tm; HRESULT hr;
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)rd));
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)fd));
    memset(&tm, 0xcc, sizeof tm);
    hr = IDWriteTextLayout_GetMetrics(l, &tm);
    if (valid) {
        CHECK_HR_OK(hr);
        CHECK(tm.lineCount >= 1);
        CHECK(tm.width > 0.0f && tm.height > 0.0f);
        {   DWRITE_CLUSTER_METRICS cm[16]; UINT32 n = bx_clusters(l, cm, 16), i, sum = 0; CHECK(n == 9); for (i = 0; i < n; i++) sum += cm[i].length; CHECK(sum == 9); }
        {   dwx_renderer *r = bx_draw(l, 0, 0); if (r) { CHECK(r->count[DWX_DRAW_GLYPHRUN] >= 1); dwx_release(r); } }
    } else {
        t_note("reading %s + flow %s (undocumented pairing): GetMetrics -> %s", rd_name[rd], fd_name[fd], dw_hr_name(hr));
    }
    BX_DONE(l);
}
#define COMBO(r, f, valid) TEST(r2_layout_bidi_reorder_flow_combo_##r##_##f) { combo(DWRITE_READING_DIRECTION_##r, DWRITE_FLOW_DIRECTION_##f, valid); }
/* horizontal reading */
COMBO(LEFT_TO_RIGHT, TOP_TO_BOTTOM, 1)
COMBO(RIGHT_TO_LEFT, TOP_TO_BOTTOM, 1)
COMBO(LEFT_TO_RIGHT, BOTTOM_TO_TOP, 1)
COMBO(RIGHT_TO_LEFT, BOTTOM_TO_TOP, 1)
COMBO(LEFT_TO_RIGHT, LEFT_TO_RIGHT, 0)
COMBO(RIGHT_TO_LEFT, LEFT_TO_RIGHT, 0)
COMBO(LEFT_TO_RIGHT, RIGHT_TO_LEFT, 0)
COMBO(RIGHT_TO_LEFT, RIGHT_TO_LEFT, 0)
/* vertical reading */
COMBO(TOP_TO_BOTTOM, LEFT_TO_RIGHT, 1)
COMBO(TOP_TO_BOTTOM, RIGHT_TO_LEFT, 1)
COMBO(BOTTOM_TO_TOP, LEFT_TO_RIGHT, 1)
COMBO(BOTTOM_TO_TOP, RIGHT_TO_LEFT, 1)
COMBO(TOP_TO_BOTTOM, TOP_TO_BOTTOM, 0)
COMBO(TOP_TO_BOTTOM, BOTTOM_TO_TOP, 0)
COMBO(BOTTOM_TO_TOP, TOP_TO_BOTTOM, 0)
COMBO(BOTTOM_TO_TOP, BOTTOM_TO_TOP, 0)

/* ---- glyph orientation: isSideways of the glyph runs ----------------------------------------------------------- */
typedef struct { const char *nm; const WCHAR *text; int orient; int rd; int fd; int want; /* 1 sideways, 0 upright, -1 record */ } so_row;
static void sideways(const so_row *r)
{
    BX_BEGIN(); BX_LAYOUT(l, r->text, 300.0f, 300.0f); BX_QI(l2, IDWriteTextLayout2, l); int i, runs = 0;
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)r->rd));
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)r->fd));
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, (DWRITE_VERTICAL_GLYPH_ORIENTATION)r->orient));
    dwx_renderer *d = bx_draw(l, 0, 0);
    if (d) {
        for (i = 0; i < d->ncalls; i++) if (d->calls[i].kind == DWX_DRAW_GLYPHRUN) {
            runs++;
            if (r->want >= 0) CHECKF(d->calls[i].sideways == (BOOL)r->want, "run at %u: isSideways %d want %d", d->calls[i].text_pos, d->calls[i].sideways, r->want);
            else t_note("%s: run at %u len %u isSideways %d level %u", r->nm, d->calls[i].text_pos, d->calls[i].text_len, d->calls[i].sideways, d->calls[i].bidi);
        }
        CHECK(runs >= 1);
        dwx_release(d);
    }
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
}
#define TTB DWRITE_READING_DIRECTION_TOP_TO_BOTTOM
#define BTT DWRITE_READING_DIRECTION_BOTTOM_TO_TOP
#define LTRd DWRITE_READING_DIRECTION_LEFT_TO_RIGHT
#define RTLd DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
#define FLTR DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT
#define FRTL DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT
#define FTTB DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM
#define OR_DEF DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT
#define OR_STK DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED
#define SO(nm, text, o, rd, fd, want) static const so_row so_##nm = { #nm, text, o, rd, fd, want }; TEST(r2_layout_bidi_reorder_flow_sideways_##nm) { sideways(&so_##nm); }
SO(latin_default_ttb_rtl,   L"ab",   OR_DEF, TTB, FRTL, 1)
SO(latin_default_ttb_ltr,   L"ab",   OR_DEF, TTB, FLTR, 1)
SO(latin_default_btt_rtl,   L"ab",   OR_DEF, BTT, FRTL, 1)
SO(latin_default_btt_ltr,   L"ab",   OR_DEF, BTT, FLTR, 1)
SO(latin_stacked_ttb_rtl,   L"ab",   OR_STK, TTB, FRTL, 0)
SO(latin_stacked_ttb_ltr,   L"ab",   OR_STK, TTB, FLTR, 0)
SO(latin_stacked_btt_rtl,   L"ab",   OR_STK, BTT, FRTL, 0)
SO(cjk_default_ttb_rtl,     L"\u4e00\u4e01", OR_DEF, TTB, FRTL, 0)
SO(cjk_stacked_ttb_rtl,     L"\u4e00\u4e01", OR_STK, TTB, FRTL, 0)
SO(hiragana_default,        L"\u3042\u3044", OR_DEF, TTB, FRTL, 0)
SO(hebrew_default_ttb,      L"\u05d0\u05d1", OR_DEF, TTB, FRTL, -1)
SO(hebrew_stacked_ttb,      L"\u05d0\u05d1", OR_STK, TTB, FRTL, 0)
SO(digits_default_ttb,      L"123", OR_DEF, TTB, FRTL, -1)
SO(horizontal_ltr_default,  L"ab",   OR_DEF, LTRd, FTTB, 0)           /* horizontal layouts never draw sideways glyph runs */
SO(horizontal_ltr_stacked,  L"ab",   OR_STK, LTRd, FTTB, 0)
SO(horizontal_rtl_stacked,  L"\u05d0\u05d1", OR_STK, RTLd, FTTB, 0)
SO(horizontal_rtl_default,  L"\u05d0\u05d1", OR_DEF, RTLd, FTTB, 0)
SO(mixed_default_ttb,       L"ab\u4e00\u05d0", OR_DEF, TTB, FRTL, -1)

/* orientation can differ per range through IDWriteTextAnalysisSource only; a layout-wide setter re-lays out immediately */
TEST(r2_layout_bidi_reorder_flow_orientation_switch_relayouts)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab", 300.0f, 300.0f); BX_QI(l2, IDWriteTextLayout2, l); int i; dwx_renderer *d;
    IDWriteTextLayout_SetReadingDirection(l, TTB); IDWriteTextLayout_SetFlowDirection(l, FRTL);
    IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, OR_DEF);
    d = bx_draw(l, 0, 0); if (d) { for (i = 0; i < d->ncalls; i++) if (d->calls[i].kind == DWX_DRAW_GLYPHRUN) CHECK(d->calls[i].sideways); dwx_release(d); }
    IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, OR_STK);
    d = bx_draw(l, 0, 0); if (d) { for (i = 0; i < d->ncalls; i++) if (d->calls[i].kind == DWX_DRAW_GLYPHRUN) CHECK(!d->calls[i].sideways); dwx_release(d); }
    IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, OR_DEF);
    d = bx_draw(l, 0, 0); if (d) { for (i = 0; i < d->ncalls; i++) if (d->calls[i].kind == DWX_DRAW_GLYPHRUN) CHECK(d->calls[i].sideways); dwx_release(d); }
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
}

/* ---- geometry of vertical layouts ----------------------------------------------------------------------------- */
TEST(r2_layout_bidi_reorder_flow_vertical_positions_advance_downwards)
{
    BX_BEGIN(); BX_LAYOUT(l, L"abc", 300.0f, 300.0f); bx_tp p0, p1, p2;
    IDWriteTextLayout_SetReadingDirection(l, TTB); IDWriteTextLayout_SetFlowDirection(l, FRTL);
    p0 = bx_pos(l, 0, FALSE); p1 = bx_pos(l, 1, FALSE); p2 = bx_pos(l, 2, FALSE);
    CHECK(p1.y > p0.y && p2.y > p1.y);                               /* characters stack top to bottom */
    CHK_NEAR(p1.y - p0.y, p2.y - p1.y);
    CHK_NEAR(p0.x, p1.x);                                            /* and share one column */
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_flow_vertical_bottom_to_top_advance_upwards)
{
    BX_BEGIN(); BX_LAYOUT(l, L"abc", 300.0f, 300.0f); bx_tp p0, p1, p2;
    IDWriteTextLayout_SetReadingDirection(l, BTT); IDWriteTextLayout_SetFlowDirection(l, FRTL);
    p0 = bx_pos(l, 0, FALSE); p1 = bx_pos(l, 1, FALSE); p2 = bx_pos(l, 2, FALSE);
    CHECK(p1.y < p0.y && p2.y < p1.y);
    CHK_NEAR(p0.x, p1.x);
    BX_DONE(l);
}

static void line_columns(DWRITE_FLOW_DIRECTION fd)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab\ncd", 300.0f, 300.0f); bx_tp a, b;
    IDWriteTextLayout_SetReadingDirection(l, TTB); IDWriteTextLayout_SetFlowDirection(l, fd);
    a = bx_pos(l, 0, FALSE); b = bx_pos(l, 3, FALSE);
    if (fd == FRTL) CHECK(b.x < a.x);                                 /* the next line is placed to the left */
    else CHECK(b.x > a.x);                                            /* ... or to the right */
    CHK_NEAR(a.y, b.y);                                               /* both lines start at the same height */
    BX_DONE(l);
}
TEST(r2_layout_bidi_reorder_flow_vertical_lines_flow_rtl) { line_columns(FRTL); }
TEST(r2_layout_bidi_reorder_flow_vertical_lines_flow_ltr) { line_columns(FLTR); }

/* hebrew in a vertical layout: still a right-to-left run for bidi purposes (record level and ordering) */
TEST(r2_layout_bidi_reorder_flow_vertical_hebrew_levels_recorded)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab\u05d0\u05d1\u05d2", 300.0f, 300.0f); DWRITE_CLUSTER_METRICS cm[8]; UINT32 n, i; char b[64]; int k = 0;
    IDWriteTextLayout_SetReadingDirection(l, TTB); IDWriteTextLayout_SetFlowDirection(l, FRTL);
    n = bx_clusters(l, cm, 8); CHECK(n == 5);
    for (i = 0; i < n; i++) k += snprintf(b + k, sizeof b - k, "%u", cm[i].isRightToLeft);
    t_note("vertical ttb layout, 'ab' + 3 Hebrew: isRightToLeft per cluster = %s", b);
    for (i = 0; i < n; i++) CHECK(cm[i].length == 1);
    BX_DONE(l);
}

/* switching between horizontal and vertical reading changes the layout, and back restores it */
TEST(r2_layout_bidi_reorder_flow_horizontal_vertical_horizontal)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1", 200.0f, 200.0f); DWRITE_TEXT_METRICS a, b, c;
    bx_metrics(l, &a);
    IDWriteTextLayout_SetReadingDirection(l, TTB); IDWriteTextLayout_SetFlowDirection(l, FRTL);
    bx_metrics(l, &b);
    IDWriteTextLayout_SetReadingDirection(l, LTRd); IDWriteTextLayout_SetFlowDirection(l, FTTB);
    bx_metrics(l, &c);
    CHK_NEAR(a.width, c.width); CHK_NEAR(a.height, c.height); CHK_NEAR(a.left, c.left); CHK_NEAR(a.top, c.top);
    t_note("horizontal %g x %g; vertical %g x %g (left %g top %g)", a.width, a.height, b.width, b.height, b.left, b.top);
    BX_DONE(l);
}

/* flow direction BOTTOM_TO_TOP / reading direction RTL: lines stack upward, text still reads right to left */
TEST(r2_layout_bidi_reorder_flow_bottom_to_top_lines)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\n\u05d2\u05d3", 100.0f, 100.0f); bx_tp a, b; DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout_SetReadingDirection(l, RTLd); IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP);
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &tm));
    a = bx_pos(l, 0, FALSE); b = bx_pos(l, 3, FALSE);
    t_note("flow BTT: line 0 y=%g, line 1 y=%g (x %g / %g)", a.y, b.y, a.x, b.x);
    CHECK(a.y != b.y);
    BX_DONE(l);
}
