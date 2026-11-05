/* DrawUnderline / DrawStrikethrough: grouping, geometry and the values of DWRITE_UNDERLINE / DWRITE_STRIKETHROUGH.
 *
 * DrawUnderlines works per line over groups of consecutive underlined spans: spans merge into one call while their locale string,
 * bidi parity and drawing effect agree (font size, weight and family may differ). The merged width is the sum of the span widths,
 * thickness is the width-weighted average of underlineThickness * size / upm, offset the width-weighted average of
 * -underlinePosition * size / upm (then PixelSnapper::SnapUnderlineOffset), runHeight the maximum of capHeight * size / upm.
 * The origin is the left of the first span at the (snapped) baseline.
 * DrawStrikethroughs makes one call per underlined... struck span (never merged): width = span width, thickness =
 * strikethroughThickness * size / upm, offset = -strikethroughPosition * size / upm, baseline origin NOT pixel snapped. */
#include "td.h"

#define RD() dwx_renderer *r = dwx_renderer_new()
#define DONE(l, r) do { IDWriteTextLayout_Release(l); dwx_release(r); TD_END(); } while (0)
#define NOSNAP(r) ((r)->snapping_disabled = TRUE)
#define UL(r, n) td_nth((r), DWX_DRAW_UNDERLINE, (n))
#define ST(r, n) td_nth((r), DWX_DRAW_STRIKETHROUGH, (n))

static float snap_ul(float off, float ppd)
{
    float s = off * ppd, e = floorf(s + 0.5f);
    if (e == 0.0f && s != 0.0f) e = s < 0 ? -1.0f : 1.0f;
    return e / ppd;
}

/* ---- single underline ------------------------------------------------------------------------------------------ */
TEST(ul_basic_values_unsnapped)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, RNG(2, 4)));
    DRAW_OK(l, r, 10.0f, 5.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    const dwx_draw *u = UL(r, 0), *g = td_run_at(r, 2);
    CHECK(u != NULL);
    if (u) {
        DWRITE_FONT_METRICS m; td_face_metrics(td_nth(r, DWX_DRAW_GLYPHRUN, 0), &m);
        CHECK_NEARF(u->ul.width, 4 * 7.8125);
        CHECK_NEARF(u->ul.thickness, m.underlineThickness * 16.0f / m.designUnitsPerEm);
        CHECK_NEARF(u->ul.offset, -(float)m.underlinePosition * 16.0f / m.designUnitsPerEm);
        t_note("runHeight %g (capHeight-scaled expectation %g)", u->ul.runHeight, m.capHeight * 16.0f / m.designUnitsPerEm);
        EXPECT_EQ(m.underlinePosition, -200); EXPECT_EQ(m.underlineThickness, 100);
        CHECK_NEARF(u->ul.thickness, 0.78125); CHECK_NEARF(u->ul.offset, 1.5625);
        EXPECT_EQ(u->ul.readingDirection, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        EXPECT_EQ(u->ul.flowDirection, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
        EXPECT_EQ(u->ul.measuringMode, DWRITE_MEASURING_MODE_NATURAL);
        CHECK(u->line_locale && !lstrcmpiW(u->line_locale, L"en-us"));
        CHECK_NEARF(u->x, 10.0f + 2 * 7.8125);
        CHECK_NEARF(u->y, 5.0f + td_line_baseline(l, 0));
        CHECK_NEARF(u->x, g ? g->x : -1); CHECK_NEARF(u->y, g ? g->y : -1);
    }
    DONE(l, r);
}
TEST(ul_whole_text)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    const dwx_draw *u = UL(r, 0);
    if (u) { CHECK_NEARF(u->ul.width, 8 * 7.8125); CHECK_NEARF(u->x, 0.0f); }
    DONE(l, r);
}
TEST(ul_snapped_offset_table)
{
    /* default renderer: the offset goes through SnapUnderlineOffset (whole pixels, at least one pixel), thickness is untouched */
    static const float sizes[] = { 4.0f, 8.0f, 12.0f, 16.0f, 20.0f, 24.0f, 40.0f, 100.0f };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        TD_LAYOUT(l, L"ABC", sizes[i], 1000.0f, 400.0f); RD();
        IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3));
        DRAW_OK(l, r, 0.0f, 0.0f);
        const dwx_draw *u = UL(r, 0);
        CHECK(u != NULL);
        if (u) {
            float raw = 200.0f * sizes[i] / 2048.0f;
            CHECKF(fabsf(u->ul.offset - snap_ul(raw, 1.0f)) < 1e-4f, "size %g: offset %g want %g (raw %g)", sizes[i], u->ul.offset, snap_ul(raw, 1.0f), raw);
            CHECK_NEARF(u->ul.thickness, 100.0f * sizes[i] / 2048.0f);
            CHECK_NEARF(u->ul.offset, floorf(u->ul.offset));
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(ul_snapped_offset_ppd)
{
    static const float ppd[] = { 1.5f, 2.0f, 3.0f };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof ppd / sizeof ppd[0]; i++) {
        TD_LAYOUT(l, L"ABC", 12.0f, 1000.0f, 400.0f); RD(); r->pixels_per_dip = ppd[i];
        IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3));
        DRAW_OK(l, r, 0.0f, 0.0f);
        const dwx_draw *u = UL(r, 0);
        if (u) CHECK_NEAR(u->ul.offset, snap_ul(200.0f * 12.0f / 2048.0f, ppd[i]), 1e-4);
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(ul_baseline_y_snapped_with_text)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 400.0f); RD();
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3));
    DRAW_OK(l, r, 0.0f, 0.3f);
    const dwx_draw *u = UL(r, 0), *g = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (u && g) { CHECK_NEARF(u->y, g->y); CHECK_NEARF(u->y, floorf(u->y)); CHECK_NEARF(u->x, g->x); }
    DONE(l, r);
}
TEST(ul_range_inside_run_geometry)
{
    static const struct { int s, n; } rg[] = { {0,1}, {0,3}, {1,1}, {3,5}, {7,1}, {0,8}, {4,4} };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof rg / sizeof rg[0]; i++) {
        TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
        IDWriteTextLayout_SetUnderline(l, TRUE, RNG(rg[i].s, rg[i].n));
        DRAW_OK(l, r, 3.0f, 0.0f);
        EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
        const dwx_draw *u = UL(r, 0);
        if (u) { CHECK_NEARF(u->x, 3.0f + rg[i].s * 7.8125); CHECK_NEARF(u->ul.width, rg[i].n * 7.8125); }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(ul_cleared_by_false)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    IDWriteTextLayout_SetUnderline(l, FALSE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 0);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    DONE(l, r);
}
TEST(ul_clear_middle_splits)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8)); IDWriteTextLayout_SetUnderline(l, FALSE, RNG(3, 2));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 2);
    const dwx_draw *a = UL(r, 0), *b = UL(r, 1);
    if (a && b) { CHECK_NEARF(a->ul.width, 3 * 7.8125); CHECK_NEARF(b->ul.width, 3 * 7.8125); CHECK_NEARF(b->x - a->x, 5 * 7.8125); }
    DONE(l, r);
}
TEST(ul_two_disjoint_ranges)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGHIJ", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(1, 2)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(6, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 2);
    const dwx_draw *a = UL(r, 0), *b = UL(r, 1);
    if (a && b) { CHECK_NEARF(a->x, 7.8125); CHECK_NEARF(a->ul.width, 2 * 7.8125); CHECK_NEARF(b->x, 6 * 7.8125); CHECK_NEARF(b->ul.width, 3 * 7.8125); }
    DONE(l, r);
}
TEST(ul_adjacent_ranges_merge)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(1, 2)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(3, 2));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    const dwx_draw *u = UL(r, 0);
    if (u) CHECK_NEARF(u->ul.width, 4 * 7.8125);
    DONE(l, r);
}
TEST(ul_getunderline_roundtrip)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f);
    BOOL v = 99; DWRITE_TEXT_RANGE rg;
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(2, 3));
    CHECK_HR_OK(IDWriteTextLayout_GetUnderline(l, 3, &v, &rg));
    EXPECT_EQ(v, TRUE); EXPECT_EQ(rg.startPosition, 2); EXPECT_EQ(rg.length, 3);
    IDWriteTextLayout_Release(l); TD_END();
}

/* ---- merging rules --------------------------------------------------------------------------------------------- */
TEST(ul_merges_across_font_sizes)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(4, 4)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 2);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    const dwx_draw *u = UL(r, 0);
    if (u) {
        float w1 = 4 * 7.8125f, w2 = 4 * 15.625f, W = w1 + w2;
        CHECK_NEARF(u->ul.width, W);
        CHECK_NEARF(u->ul.thickness, (0.78125f * w1 + 1.5625f * w2) / W);
        CHECK_NEARF(u->ul.offset, (1.5625f * w1 + 3.125f * w2) / W);
        t_note("runHeight %g (capHeight-scaled expectation %g)", u->ul.runHeight, 1400.0f * 32.0f / 2048.0f);
        CHECK_NEARF(u->x, 0.0f);
    }
    DONE(l, r);
}
TEST(ul_merges_across_weights_and_families)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBBCCCC", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(4, 4)); IDWriteTextLayout_SetFontFamilyName(l, TD_TALL, RNG(8, 4));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 12));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    const dwx_draw *u = UL(r, 0);
    if (u) CHECK_NEARF(u->ul.width, 4 * 7.8125f + 4 * 9.375f + 4 * 7.8125f);
    DONE(l, r);
}
TEST(ul_merges_across_script_change)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB" GREEK_A GREEK_A L"CD", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 6));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    const dwx_draw *u = UL(r, 0);
    if (u) CHECK_NEARF(u->ul.width, 6 * 7.8125);
    DONE(l, r);
}
TEST(ul_splits_on_effect_change)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 4)); IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(4, 4));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 2);
    const dwx_draw *a = UL(r, 0), *b = UL(r, 1);
    if (a && b) { EXPECT_EQ(a->effect, e1); EXPECT_EQ(b->effect, e2); CHECK_NEARF(a->ul.width, 31.25); CHECK_NEARF(b->ul.width, 31.25); CHECK_NEARF(b->x, 31.25); }
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e1); dwx_release(e2); TD_END();
}
TEST(ul_one_effect_object_two_ranges_merges)
{
    /* pointer equality of the effect is what counts */
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    dwx_effect *e = dwx_effect_new(1);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 8));
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(4, 4)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    const dwx_draw *u = UL(r, 0);
    if (u) EXPECT_EQ(u->effect, e);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(ul_splits_on_locale_change)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetLocaleName(l, L"fr-fr", RNG(4, 4)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 2);
    const dwx_draw *a = UL(r, 0), *b = UL(r, 1);
    if (a && b && a->line_locale && b->line_locale) { CHECK(!lstrcmpiW(a->line_locale, L"en-us")); CHECK(!lstrcmpiW(b->line_locale, L"fr-fr")); }
    DONE(l, r);
}
TEST(ul_splits_per_line)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA BBBB CCCC", 16.0f, 40.0f, 300.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 14));
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    EXPECT_EQ(ln.n, 3);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 3);
    int k;
    for (k = 0; k < 3; k++) {
        const dwx_draw *u = UL(r, k);
        if (u) { CHECK_NEARF(u->y, ln.top[k] + ln.lm[k].baseline); CHECK_NEARF(u->x, 0.0f); CHECK(u->ul.width >= 4 * 7.8125f - 0.01f); }
    }
    DONE(l, r);
}
TEST(ul_hard_newline_two_underlines)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB\nCD", 16.0f, 1000.0f, 300.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 5));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 2);
    const dwx_draw *a = UL(r, 0), *b = UL(r, 1);
    if (a && b) { CHECK(a->ul.width >= 2 * 7.8125f - 0.01f); CHECK_NEARF(b->ul.width, 2 * 7.8125); CHECK_NEARF(b->y - a->y, td_line_height(l, 0)); CHECK_NEARF(b->x, 0.0f); t_note("underline over \"AB\\n\" is %g wide", a->ul.width); }
    DONE(l, r);
}
TEST(ul_splits_on_bidi_parity)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB" HEB L"CD", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 7));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int n = td_count(r, DWX_DRAW_UNDERLINE);
    t_note("underline over latin/hebrew/latin: %d call(s)", n);
    CHECK(n >= 3);
    int i; float tot = 0; for (i = 0; i < n; i++) tot += UL(r, i)->ul.width;
    CHECK_NEAR(tot, 7 * 7.8125f, 0.01);
    for (i = 0; i < n; i++) { const dwx_draw *u = UL(r, i); CHECK(u->ul.readingDirection == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT || u->ul.readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT); }
    DONE(l, r);
}
TEST(ul_hebrew_reading_direction)
{
    TD_BEGIN(); TD_LAYOUT(l, HEB, 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    const dwx_draw *u = UL(r, 0);
    if (u) { CHECK_NEARF(u->ul.width, 3 * 7.8125); t_note("underline of a Hebrew run: readingDirection %d, x=%g", u->ul.readingDirection, u->x); }
    DONE(l, r);
}
TEST(ul_inside_rtl_paragraph_latin)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 100.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *u = UL(r, 0), *g = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(u && g);
    if (u && g) { CHECK_NEARF(u->ul.width, 31.25); CHECK_NEARF(u->x, 100.0f - 31.25f); t_note("RTL paragraph underline readingDirection=%d", u->ul.readingDirection); }
    DONE(l, r);
}

/* ---- values by size/font ------------------------------------------------------------------------------------------ */
TEST(ul_thickness_scales_with_size)
{
    static const float sizes[] = { 5.0f, 10.0f, 16.0f, 22.5f, 48.0f, 72.0f, 150.0f };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        TD_LAYOUT(l, L"AB", sizes[i], 1000.0f, 400.0f); RD(); NOSNAP(r);
        IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 2));
        DRAW_OK(l, r, 0.0f, 0.0f);
        const dwx_draw *u = UL(r, 0);
        if (u) {
            CHECK_NEAR(u->ul.thickness, 100.0f * sizes[i] / 2048.0f, 1e-4);
            CHECK_NEAR(u->ul.offset, 200.0f * sizes[i] / 2048.0f, 1e-4);
            t_note("runHeight %g (capHeight-scaled expectation %g)", u->ul.runHeight, 1400.0f * sizes[i] / 2048.0f);
            CHECK_NEAR(u->ul.width, 2 * 1000.0f * sizes[i] / 2048.0f, 1e-3);
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(ul_bold_face_same_metrics)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 4)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *u = UL(r, 0);
    if (u) { CHECK_NEARF(u->ul.width, 4 * 9.375); CHECK_NEARF(u->ul.thickness, 0.78125); }
    DONE(l, r);
}
TEST(ul_tall_font_baseline_shared)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontFamilyName(l, TD_TALL, RNG(4, 4)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *u = UL(r, 0), *g = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (u && g) CHECK_NEARF(u->y, g->y);
    DONE(l, r);
}
TEST(ul_measuring_mode_gdi)
{
    TD_BEGIN(); IDWriteTextFormat *t = td_format(&X, TD_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t || FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"ABCD", 4, t, 1000.0f, 100.0f, 1.0f, NULL, FALSE, &l))) { DW_RELEASE(t); TD_END(); t_skip("gdi layout unavailable"); return; }
    IDWriteTextFormat_Release(t);
    RD(); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *u = UL(r, 0), *s = ST(r, 0);
    CHECK(u && s);
    if (u && s) { EXPECT_EQ(u->ul.measuringMode, DWRITE_MEASURING_MODE_GDI_CLASSIC); EXPECT_EQ(s->st.measuringMode, DWRITE_MEASURING_MODE_GDI_CLASSIC); t_note("gdi classic underline: width %g thickness %g offset %g", u->ul.width, u->ul.thickness, u->ul.offset); }
    DONE(l, r);
}
TEST(ul_effect_passthrough)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e = dwx_effect_new(7);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 4)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i, n = 0;
    for (i = 0; i < r->ncalls; i++) { EXPECT_EQ(r->calls[i].effect, e); n++; }
    CHECK(n >= 3);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1); EXPECT_EQ(td_count(r, DWX_DRAW_STRIKETHROUGH), 1);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(ul_null_effect_passthrough)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i; for (i = 0; i < r->ncalls; i++) EXPECT_EQ(r->calls[i].effect, NULL);
    DONE(l, r);
}
TEST(ul_renderer1_angle_zero)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD(); r->support_r1 = 1;
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *u = UL(r, 0), *s = ST(r, 0);
    CHECK(u && s);
    if (u && s) { EXPECT_EQ(u->has_orientation, 1); EXPECT_EQ(u->angle, 0); EXPECT_EQ(s->has_orientation, 1); EXPECT_EQ(s->angle, 0); }
    DONE(l, r);
}
TEST(ul_not_drawn_for_inline_object)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB CD", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(20.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(2, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 5));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int n = td_count(r, DWX_DRAW_UNDERLINE);
    t_note("underline over an inline object: %d call(s)", n);
    CHECK(n >= 1);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(io); TD_END();
}

/* ---- strikethrough ----------------------------------------------------------------------------------------------- */
TEST(st_basic_values_unsnapped)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    CHECK_HR_OK(IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(2, 4)));
    DRAW_OK(l, r, 10.0f, 5.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_STRIKETHROUGH), 1);
    const dwx_draw *s = ST(r, 0);
    CHECK(s != NULL);
    if (s) {
        DWRITE_FONT_METRICS m; td_face_metrics(td_nth(r, DWX_DRAW_GLYPHRUN, 0), &m);
        EXPECT_EQ(m.strikethroughPosition, 600); EXPECT_EQ(m.strikethroughThickness, 100);
        CHECK_NEARF(s->st.width, 4 * 7.8125);
        CHECK_NEARF(s->st.thickness, m.strikethroughThickness * 16.0f / m.designUnitsPerEm);
        CHECK_NEARF(s->st.offset, -(float)m.strikethroughPosition * 16.0f / m.designUnitsPerEm);
        CHECK_NEARF(s->st.offset, -4.6875);
        EXPECT_EQ(s->st.readingDirection, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        EXPECT_EQ(s->st.flowDirection, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
        EXPECT_EQ(s->st.measuringMode, DWRITE_MEASURING_MODE_NATURAL);
        CHECK_NEARF(s->x, 10.0f + 2 * 7.8125); CHECK_NEARF(s->y, 5.0f + td_line_baseline(l, 0));
        t_note("strikethrough localeName is %s", s->line_locale ? "set" : "NULL");
    }
    DONE(l, r);
}
TEST(st_baseline_not_pixel_snapped)
{
    /* DrawStrikethroughs applies no PixelSnapper: the baseline keeps its fractional part although glyph runs are snapped */
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.3f);
    const dwx_draw *s = ST(r, 0), *g = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(s && g);
    if (s && g) { CHECK_NEARF(s->y, 0.3f + td_line_baseline(l, 0)); CHECK_NEARF(g->y, floorf(g->y)); CHECK_NEARF(s->st.offset, -4.6875); }
    DONE(l, r);
}
TEST(st_never_merged)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(4, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_STRIKETHROUGH), 2);
    const dwx_draw *a = ST(r, 0), *b = ST(r, 1);
    if (a && b) {
        CHECK_NEARF(a->st.width, 31.25); CHECK_NEARF(b->st.width, 62.5);
        CHECK_NEARF(a->st.thickness, 0.78125); CHECK_NEARF(b->st.thickness, 1.5625);
        CHECK_NEARF(a->st.offset, -4.6875); CHECK_NEARF(b->st.offset, -9.375);
        CHECK_NEARF(b->x, 31.25);
    }
    DONE(l, r);
}
TEST(st_one_per_span_even_with_same_effect)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 200.0f); RD();
    dwx_effect *e = dwx_effect_new(3);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 8));
    IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(4, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_STRIKETHROUGH), td_count(r, DWX_DRAW_GLYPHRUN));
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(st_scales_with_size)
{
    static const float sizes[] = { 5.0f, 10.0f, 16.0f, 22.5f, 48.0f, 72.0f, 150.0f };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        TD_LAYOUT(l, L"AB", sizes[i], 1000.0f, 400.0f); RD(); NOSNAP(r);
        IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 2));
        DRAW_OK(l, r, 0.0f, 0.0f);
        const dwx_draw *s = ST(r, 0);
        if (s) {
            CHECK_NEAR(s->st.thickness, 100.0f * sizes[i] / 2048.0f, 1e-4);
            CHECK_NEAR(s->st.offset, -600.0f * sizes[i] / 2048.0f, 1e-4);
            CHECK_NEAR(s->st.width, 2000.0f * sizes[i] / 2048.0f, 1e-3);
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(st_per_line)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA BBBB CCCC", 16.0f, 40.0f, 300.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 14));
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    CHECK(td_count(r, DWX_DRAW_STRIKETHROUGH) >= 3);
    int i;
    for (i = 0; i < td_count(r, DWX_DRAW_STRIKETHROUGH); i++) {
        const dwx_draw *s = ST(r, i);
        int hit = 0, k;
        for (k = 0; k < (int)ln.n; k++) if (fabsf(s->y - (ln.top[k] + ln.lm[k].baseline)) < 0.001f) hit = 1;
        CHECK(hit);
    }
    DONE(l, r);
}
TEST(st_bold_stays_same_offset)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *s = ST(r, 0);
    if (s) { CHECK_NEARF(s->st.width, 37.5); CHECK_NEARF(s->st.offset, -4.6875); }
    DONE(l, r);
}
TEST(st_hebrew_direction)
{
    TD_BEGIN(); TD_LAYOUT(l, HEB, 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *s = ST(r, 0);
    CHECK(s != NULL);
    if (s) { CHECK_NEARF(s->st.width, 3 * 7.8125); t_note("hebrew strikethrough: readingDirection %d offset %g x %g", s->st.readingDirection, s->st.offset, s->x); }
    DONE(l, r);
}
TEST(st_and_ul_together_both_emitted)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1); EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1); EXPECT_EQ(td_count(r, DWX_DRAW_STRIKETHROUGH), 1);
    EXPECT_EQ(r->ncalls, 3);
    EXPECT_EQ(r->calls[1].kind, DWX_DRAW_UNDERLINE); EXPECT_EQ(r->calls[2].kind, DWX_DRAW_STRIKETHROUGH);
    DONE(l, r);
}
TEST(st_underline_only_range_no_strike)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(2, 2)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(5, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_STRIKETHROUGH), 1); EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 1);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 4);
    DONE(l, r);
}
TEST(st_getstrikethrough_roundtrip)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f);
    BOOL v = 99; DWRITE_TEXT_RANGE rg;
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(1, 4));
    CHECK_HR_OK(IDWriteTextLayout_GetStrikethrough(l, 2, &v, &rg));
    EXPECT_EQ(v, TRUE); EXPECT_EQ(rg.startPosition, 1); EXPECT_EQ(rg.length, 4);
    IDWriteTextLayout_Release(l); TD_END();
}
TEST(st_ppd_does_not_change_values)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD(); r->pixels_per_dip = 2.0f;
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.3f);
    const dwx_draw *s = ST(r, 0);
    if (s) { CHECK_NEARF(s->st.offset, -4.6875); CHECK_NEARF(s->st.thickness, 0.78125); CHECK_NEARF(s->y, 0.3f + td_line_baseline(l, 0)); }
    DONE(l, r);
}
TEST(st_ul_locale_string_matches_run)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetLocaleName(l, L"de-de", RNG(0, 4)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *u = UL(r, 0), *g = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (u && g && u->line_locale && g->locale) CHECK(!lstrcmpiW(u->line_locale, g->locale));
    else CHECK(u && g);
    DONE(l, r);
}

TEST(ul_group_wide_values_shared_by_split_calls)
{
    /* thickness, offset and runHeight are computed once for a whole group of consecutive underlined spans, then every
     * effect/locale-delimited piece of the group is reported with those group values and its own width and origin */
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 200.0f); RD(); NOSNAP(r);
    dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(4, 4));
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 4)); IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(4, 4));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 2);
    const dwx_draw *a = UL(r, 0), *b = UL(r, 1);
    if (a && b) {
        float w1 = 31.25f, w2 = 62.5f, W = w1 + w2;
        CHECK_NEARF(a->ul.width, w1); CHECK_NEARF(b->ul.width, w2); CHECK_NEARF(b->x, w1);
        CHECK_NEARF(a->ul.thickness, (0.78125f * w1 + 1.5625f * w2) / W); CHECK_NEARF(b->ul.thickness, a->ul.thickness);
        CHECK_NEARF(a->ul.offset, (1.5625f * w1 + 3.125f * w2) / W);     CHECK_NEARF(b->ul.offset, a->ul.offset);
        t_note("runHeight %g", a->ul.runHeight);                          CHECK_NEARF(b->ul.runHeight, a->ul.runHeight);
    }
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e1); dwx_release(e2); TD_END();
}
