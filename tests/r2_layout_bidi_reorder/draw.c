/* IDWriteTextLayout::Draw on bidirectional text: what the recording IDWriteTextRenderer observes.
 *
 * Facts (TextLayout::DrawGlyphRuns): the layout walks its spans and calls DrawGlyphRun once per drawable span with
 * DWRITE_GLYPH_RUN {fontFace, fontEmSize, glyphCount, glyphIndices, glyphAdvances, glyphOffsets, isSideways, bidiLevel} and a
 * DWRITE_GLYPH_RUN_DESCRIPTION {locale, string, stringLength, clusterMap, textPosition}; the spans partition the text.
 * The origin of a right-to-left run is documented to be on the right; where exactly it falls is recorded, not asserted.
 */
#include "bx.h"

static UINT32 gid_of(WCHAR c)
{
    if (c >= 'A' && c <= 'Z') return 2 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 28 + (c - 'a');
    if (c >= '0' && c <= '9') return 54 + (c - '0');
    if (c >= 0x5d0 && c <= 0x5ea) return 2 + (c - 0x5d0);
    if (c == ' ') return 1;
    return 0;
}

/* glyph array / cluster map consistency of every run: glyphIndices[clusterMap[i]] is the glyph of text character i */
static void check_runs(const WCHAR *text, int base, int expect_runs_min)
{
    BX_BEGIN(); UINT32 len = (UINT32)wcslen(text), k; int i, nruns = 0;
    BX_LAYOUT(l, text, BX_W, 100.0f); UINT8 lev[64];
    if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    dwx_renderer *r = bx_draw(l, 0, 0);
    if (!r) { BX_DONE(l); return; }
    CHECK(bx_draw_levels(r, len, lev));
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *d = &r->calls[i];
        if (d->kind != DWX_DRAW_GLYPHRUN) continue;
        nruns++;
        CHECK(d->has_desc && d->face != NULL && d->indices && d->advances);
        CHK_NEAR(d->em, 16.0f);
        CHECKF(d->nglyphs == d->text_len, "run at %u: %u glyphs for %u characters (one glyph per character in this font)", d->text_pos, d->nglyphs, d->text_len);
        CHECK(d->text != NULL && d->cluster_map != NULL);
        CHECK(!d->sideways);                                         /* horizontal layout */
        for (k = 0; k < d->text_len && d->text && d->cluster_map; k++) {
            CHECK(d->text[k] == text[d->text_pos + k]);              /* the description's string points at the layout text */
            CHECK(d->cluster_map[k] < d->nglyphs);
            CHECKF(d->indices[d->cluster_map[k]] == gid_of(text[d->text_pos + k]), "run at %u char %u: glyph %u want %u", d->text_pos, k, d->indices[d->cluster_map[k]], gid_of(text[d->text_pos + k]));
        }
        {   /* glyph order is either logical or reversed, never anything else */
            int ident = 1, rev = 1;
            for (k = 0; k < d->text_len && d->cluster_map; k++) { if (d->cluster_map[k] != k) ident = 0; if (d->cluster_map[k] != d->text_len - 1 - k) rev = 0; }
            CHECK(ident || rev);
            if (d->bidi & 1) t_note("rtl run at %u: glyph order %s, origin x %g", d->text_pos, ident ? "logical" : "reversed", d->x);
        }
        for (k = 0; k < d->nglyphs; k++) CHK_NEAR(d->advances[k], CW);
        {   /* run width equals the hit-test box width of the run's text */
            bx_rg h = bx_range(l, d->text_pos, d->text_len); UINT32 j; float w = 0, lft = 1e9f, rgt = -1e9f;
            for (j = 0; j < h.count; j++) { w += h.m[j].width; if (h.m[j].left < lft) lft = h.m[j].left; if (h.m[j].left + h.m[j].width > rgt) rgt = h.m[j].left + h.m[j].width; }
            CHK_NEAR(dwx_run_width(d), w);
            /* the baseline origin is one of the run's horizontal edges */
            CHECKF(bx_feq(d->x, lft) || bx_feq(d->x, rgt), "run at %u origin x %g not at [%g, %g]", d->text_pos, d->x, lft, rgt);
            if (d->bidi & 1) t_note("rtl run origin is the %s edge", bx_feq(d->x, rgt) ? "right" : "left");
        }
    }
    CHECKF(nruns >= expect_runs_min, "%d glyph runs, want at least %d", nruns, expect_runs_min);
    dwx_release(r);
    BX_DONE(l);
}
#define RUNS(nm, text, base, minruns) TEST(r2_layout_bidi_reorder_draw_runs_##nm) { check_runs(text, base, minruns); }
RUNS(latin,             L"abc", 0, 1)
RUNS(hebrew,            L"\u05d0\u05d1\u05d2", 0, 1)
RUNS(hebrew_rtl,        L"\u05d0\u05d1\u05d2", 1, 1)
RUNS(mixed,             L"ab \u05d0\u05d1\u05d2 cd", 0, 3)
RUNS(mixed_rtl,         L"ab \u05d0\u05d1\u05d2 cd", 1, 3)
RUNS(hebrew_latin,      L"\u05d0\u05d1\u05d2 abc", 0, 2)
RUNS(latin_hebrew_rtl,  L"abc \u05d0\u05d1\u05d2", 1, 2)
RUNS(hebrew_digits,     L"\u05d0\u05d1\u05d2 123", 0, 2)
RUNS(three_scripts,     L"abc\u05d0\u05d1\u05d2xyz", 0, 3)
RUNS(alternating,       L"a\u05d0b\u05d1c\u05d2", 0, 6)

/* Reordering: the runs, laid out by hit testing, tile the line without gaps or overlap. */
TEST(r2_layout_bidi_reorder_draw_runs_tile_the_line)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", BX_W, 100.0f);
    dwx_renderer *r = bx_draw(l, 0, 0); int i, n = 0; float edges[16][2], covered = 0;
    if (r) {
        for (i = 0; i < r->ncalls; i++) {
            const dwx_draw *d = &r->calls[i]; bx_rg h; UINT32 j;
            if (d->kind != DWX_DRAW_GLYPHRUN || n >= 16) continue;
            h = bx_range(l, d->text_pos, d->text_len);
            edges[n][0] = 1e9f; edges[n][1] = -1e9f;
            for (j = 0; j < h.count; j++) { if (h.m[j].left < edges[n][0]) edges[n][0] = h.m[j].left; if (h.m[j].left + h.m[j].width > edges[n][1]) edges[n][1] = h.m[j].left + h.m[j].width; }
            covered += edges[n][1] - edges[n][0]; n++;
        }
        CHK_NEAR(covered, 8 * CW);
        {   int a, b; for (a = 0; a < n; a++) for (b = a + 1; b < n; b++) CHECK(edges[a][1] <= edges[b][0] + EPS || edges[b][1] <= edges[a][0] + EPS); }
        dwx_release(r);
    }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_draw_call_order_recorded)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", BX_W, 100.0f);
    dwx_renderer *r = bx_draw(l, 0, 0); int i, k = 0;
    if (r) {
        char b[300]; int o = 0;
        for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && o < 260) { o += snprintf(b + o, sizeof b - o, "[pos %u len %u lvl %u x %g] ", r->calls[i].text_pos, r->calls[i].text_len, r->calls[i].bidi, r->calls[i].x); k++; }
        CHECK(k >= 3);
        t_note("draw order: %s", b);
        dwx_release(r);
    }
    BX_DONE(l);
}

/* The draw origin offsets every run by exactly (ox, oy). */
TEST(r2_layout_bidi_reorder_draw_origin_offsets_runs)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", BX_W, 100.0f);
    dwx_renderer *a = bx_draw(l, 0, 0), *b = bx_draw(l, 10.0f, 20.0f); int i;
    if (a && b) {
        CHECK(a->ncalls == b->ncalls);
        for (i = 0; i < a->ncalls && i < b->ncalls; i++) {
            CHECK(a->calls[i].kind == b->calls[i].kind);
            if (a->calls[i].kind != DWX_DRAW_GLYPHRUN) continue;
            CHECK(a->calls[i].text_pos == b->calls[i].text_pos && a->calls[i].bidi == b->calls[i].bidi);
            CHK_NEAR(b->calls[i].x - a->calls[i].x, 10.0f); CHK_NEAR(b->calls[i].y - a->calls[i].y, 20.0f);
        }
    }
    if (a) dwx_release(a);
    if (b) dwx_release(b);
    BX_DONE(l);
}

/* The client context pointer is handed through untouched to every drawing callback. */
TEST(r2_layout_bidi_reorder_draw_context_passthrough)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2", BX_W, 100.0f);
    dwx_renderer *r = dwx_renderer_new(); int i, n = 0;
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, (void *)(ULONG_PTR)0x1234, (IDWriteTextRenderer *)r, 0, 0));
    for (i = 0; i < r->ncalls; i++) { CHECK(r->calls[i].ctx == (void *)(ULONG_PTR)0x1234); n++; }
    CHECK(n >= 2);
    dwx_release(r); BX_DONE(l);
}

/* A failing DrawGlyphRun callback: TextLayout::DrawGlyphRuns discards the callback's HRESULT, so the remaining spans are still
 * drawn; whether Draw itself reports the failure is recorded. */
TEST(r2_layout_bidi_reorder_draw_renderer_failure_behaviour)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", BX_W, 100.0f);
    dwx_renderer *r = dwx_renderer_new(); HRESULT hr;
    r->fail_draw = E_ABORT; r->fail_at = 2;
    hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0, 0);
    CHECK(r->ncalls >= 2);
    t_note("Draw returned %s after the 2nd of %d DrawGlyphRun callbacks failed with E_ABORT", dw_hr_name(hr), r->ncalls);
    dwx_release(r); BX_DONE(l);
}

/* Latin text in an RTL paragraph is level 2 and left-to-right; Hebrew stays level 1. */
TEST(r2_layout_bidi_reorder_draw_levels_rtl_paragraph)
{
    BX_BEGIN(); BX_LAYOUT(l, L"abc \u05d0\u05d1\u05d2", BX_W, 100.0f); UINT8 lev[16];
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    dwx_renderer *r = bx_draw(l, 0, 0);
    if (r) {
        CHECK(bx_draw_levels(r, 7, lev));
        CHECK(lev[0] == 2 && lev[1] == 2 && lev[2] == 2 && lev[3] == 1 && lev[4] == 1 && lev[6] == 1);
        dwx_release(r);
    }
    BX_DONE(l);
}

/* A range override of the font size splits a run but keeps bidi levels. */
TEST(r2_layout_bidi_reorder_draw_format_split_keeps_levels)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2\u05d3", BX_W, 100.0f); UINT8 lev[8];
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(1, 2)));
    dwx_renderer *r = bx_draw(l, 0, 0); int i, runs = 0;
    if (r) {
        CHECK(bx_draw_levels(r, 4, lev));
        for (i = 0; i < 4; i++) CHECK(lev[i] == 1);
        for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) runs++;
        CHECKF(runs == 3, "%d runs for three font-size segments", runs);
        dwx_release(r);
    }
    BX_DONE(l);
}

/* Drawing twice gives identical callbacks (layout is cached, no state is consumed). */
TEST(r2_layout_bidi_reorder_draw_repeatable)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", BX_W, 100.0f);
    dwx_renderer *a = bx_draw(l, 3, 4), *b = bx_draw(l, 3, 4); int i;
    if (a && b) {
        CHECK(a->ncalls == b->ncalls);
        for (i = 0; i < a->ncalls && i < b->ncalls; i++) {
            CHECK(a->calls[i].kind == b->calls[i].kind && a->calls[i].bidi == b->calls[i].bidi && a->calls[i].text_pos == b->calls[i].text_pos);
            CHK_NEAR(a->calls[i].x, b->calls[i].x); CHK_NEAR(a->calls[i].y, b->calls[i].y);
        }
    }
    if (a) dwx_release(a);
    if (b) dwx_release(b);
    BX_DONE(l);
}

/* Bidi level is reported for sideways runs too (vertical layouts are covered in vertical.c). */
TEST(r2_layout_bidi_reorder_draw_surrogate_run_description)
{
    BX_BEGIN(); BX_LAYOUT(l, L"a\U00010800\U00010801b", BX_W, 100.0f); UINT8 lev[8];
    dwx_renderer *r = bx_draw(l, 0, 0);
    if (r) {
        CHECK(bx_draw_levels(r, 6, lev));
        CHECK(lev[0] == 0 && lev[1] == 1 && lev[2] == 1 && lev[3] == 1 && lev[4] == 1 && lev[5] == 0);
        { int i; for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && r->calls[i].bidi == 1) CHECK(r->calls[i].nglyphs == 2 && r->calls[i].text_len == 4); }
        dwx_release(r);
    }
    BX_DONE(l);
}
