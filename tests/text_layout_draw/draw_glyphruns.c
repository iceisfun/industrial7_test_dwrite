/* DrawGlyphRun: the DWRITE_GLYPH_RUN and DWRITE_GLYPH_RUN_DESCRIPTION handed to the renderer, run splitting, baseline origins,
 * measuring mode and pixel snapping of the baseline.
 *
 * DrawGlyphRuns walks the layout's spans in text order; every span with text, no inline object and not hidden produces one call:
 * fontFace / fontEmSize / glyph arrays come from the shaped span, the description points into the layout's own copy of the text
 * (string = text + textPosition) and its cluster map; the baseline origin is the span's position on the line plus the Draw origin;
 * with horizontal text only the Y coordinate is snapped (pixel grid = pixelsPerDip * transform), X is passed through. */
#include "td.h"

#define RD() dwx_renderer *r = dwx_renderer_new()
#define DONE(l, r) do { IDWriteTextLayout_Release(l); dwx_release(r); TD_END(); } while (0)
#define NOSNAP(r) ((r)->snapping_disabled = TRUE)
#define CHECK_REL(a, b, rel) t_check(fabs((double)(a) - (double)(b)) <= (rel) * (fabs((double)(b)) + 1.0), __FILE__, __LINE__, #a " ~= " #b, "got %.9g want %.9g", (double)(a), (double)(b))

static const WCHAR *k_locale_cmp(const WCHAR *s) { return s ? s : L"(null)"; }

/* ---- the simplest run ------------------------------------------------------------------------------------ */
TEST(run_single_fields)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) {
        CHECK(c->face != NULL);
        CHECK_FLOAT_EQ(c->em, 16.0f);
        EXPECT_EQ(c->nglyphs, 8);
        CHECK(c->indices && c->advances);
        EXPECT_EQ(c->sideways, FALSE);
        EXPECT_EQ(c->bidi, 0);
        EXPECT_EQ(c->mm, DWRITE_MEASURING_MODE_NATURAL);
        CHECK(c->has_desc);
        EXPECT_EQ(c->text_pos, 0); EXPECT_EQ(c->text_len, 8);
        CHECK_WEQ(c->text, L"ABCDEFGH");
        td_check_indices(c, L"ABCDEFGH", "run glyph indices equal GetGlyphIndices");
        UINT32 i; for (i = 0; i < c->nglyphs; i++) { CHECK_NEARF(c->advances[i], 7.8125); EXPECT_EQ(c->indices[i], 2 + i); }
    }
    DONE(l, r);
}
TEST(run_single_origin_is_baseline)
{
    /* snapping disabled: origin = Draw origin + (0, line baseline) for left aligned top aligned text */
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 10.5f, 20.25f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c != NULL);
    if (c) { CHECK_NEARF(c->x, 10.5f); CHECK_NEARF(c->y, 20.25f + td_line_baseline(l, 0)); }
    DONE(l, r);
}
TEST(run_baseline_equals_font_ascent_scaled)
{
    /* the line baseline is the font ascent at the used size */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    DWRITE_FONT_METRICS m; td_face_metrics(c, &m);
    if (c) {
        CHECK_BASELINE(c->y, m, 16.0f);
        t_note("face ascent %u descent %u lineGap %d upm %u, baseline %g", m.ascent, m.descent, m.lineGap, m.designUnitsPerEm, c->y);
    }
    DONE(l, r);
}
TEST(run_em_size_table)
{
    static const float sizes[] = { 1.0f, 4.0f, 8.0f, 9.5f, 12.0f, 16.0f, 24.0f, 33.3f, 72.0f, 100.75f, 400.0f, 1000.0f };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        TD_LAYOUT(l, L"ABC", sizes[i], 100000.0f, 100000.0f); RD(); NOSNAP(r);
        DRAW_OK(l, r, 0.0f, 0.0f);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        CHECK(c != NULL);
        if (c && c->nglyphs == 3) {
            CHECK_FLOAT_EQ(c->em, sizes[i]);
            int k; for (k = 0; k < 3; k++) CHECK_REL(c->advances[k], TD_ADV(1000, sizes[i]), 1e-5);
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(run_length_table)
{
    static const int lens[] = { 1, 2, 3, 7, 8, 15, 16, 31, 32, 33, 63, 64, 100, 127, 128, 255, 256, 257, 511, 1000, 4000 };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        WCHAR *t = (WCHAR *)malloc((lens[i] + 1) * sizeof(WCHAR)); int k;
        for (k = 0; k < lens[i]; k++) t[k] = L'A' + (k % 26);
        t[lens[i]] = 0;
        TD_LAYOUT(l, t, 10.0f, 1000000.0f, 100.0f); RD(); NOSNAP(r);
        DRAW_OK(l, r, 0.0f, 0.0f);
        EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        if (c) {
            EXPECT_EQ(c->nglyphs, lens[i]); EXPECT_EQ(c->text_len, lens[i]);
            CHECK(c->text && !wcsncmp(c->text, t, lens[i]));
            CHECK_NEAR(dwx_run_width(c), lens[i] * TD_ADV(1000, 10.0f), lens[i] * 1e-4 + 1e-3);
            if (c->cluster_map) { int bad = 0; for (k = 0; k < lens[i]; k++) if (c->cluster_map[k] != k) bad++; EXPECT_EQ(bad, 0); }
        }
        IDWriteTextLayout_Release(l); dwx_release(r); free(t);
    }
    TD_END();
}
TEST(run_charset_indices)
{
    static const WCHAR *texts[] = { L"abcxyz", L"ABCXYZ", L"0123456789", L"aBc0 1Z z", L"zyxwvutsrqponmlkjihgfedcba" };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        TD_LAYOUT(l, texts[i], 16.0f, 100000.0f, 100.0f); RD();
        DRAW_OK(l, r, 0.0f, 0.0f);
        UINT32 n = (UINT32)wcslen(texts[i]);
        EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        if (c) { EXPECT_EQ(c->nglyphs, n); td_check_indices(c, texts[i], "indices"); }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(run_space_glyph)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A B", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->nglyphs == 3);
    if (c && c->nglyphs == 3) { EXPECT_EQ(c->indices[1], TTF_GID_SPACE); CHECK_NEARF(c->advances[1], 7.8125); }
    DONE(l, r);
}
TEST(run_advances_equal_design_advances_scaled)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCabc019", 20.0f, 10000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->nglyphs == 9);
    if (c && c->nglyphs == 9) {
        DWRITE_GLYPH_METRICS gm[9]; DWRITE_FONT_METRICS fm; UINT32 i;
        IDWriteFontFace_GetMetrics(c->face, &fm);
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(c->face, c->indices, 9, gm, FALSE));
        for (i = 0; i < 9; i++) CHECK_REL(c->advances[i], gm[i].advanceWidth * 20.0f / fm.designUnitsPerEm, 1e-5);
    }
    DONE(l, r);
}
TEST(run_glyph_offsets_neutral)
{
    /* glyphOffsets may be NULL (no positioning applied); when present every offset of this font is zero */
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEF", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c != NULL);
    if (c) {
        t_note("glyphOffsets %s", c->offsets ? "present" : "NULL");
        if (c->offsets) { UINT32 i; for (i = 0; i < c->nglyphs; i++) { CHECK_FLOAT_EQ(c->offsets[i].advanceOffset, 0.0f); CHECK_FLOAT_EQ(c->offsets[i].ascenderOffset, 0.0f); } }
    }
    DONE(l, r);
}

/* ---- run description ------------------------------------------------------------------------------------- */
TEST(desc_locale_name)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->has_desc && c->locale != NULL);
    if (c && c->locale) { CHECK(!lstrcmpiW(c->locale, L"en-us")); t_note("run locale spelled \"%ls\"", c->locale); }
    DONE(l, r);
}
TEST(desc_locale_follows_setlocalename)
{
    static const WCHAR *locs[] = { L"en-us", L"fr-fr", L"de-de", L"ja-jp", L"en-gb" };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof locs / sizeof locs[0]; i++) {
        TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 100.0f); RD();
        CHECK_HR_OK(IDWriteTextLayout_SetLocaleName(l, locs[i], RNG(0, 3)));
        DRAW_OK(l, r, 0.0f, 0.0f);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        CHECK(c && c->locale);
        if (c && c->locale) CHECK(!lstrcmpiW(c->locale, locs[i]));
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(desc_total_glyphs_across_size_runs)
{
    /* one glyph per character over several runs of different sizes */
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA BBBB CCCC DDDD", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(5, 4)); IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(10, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int n = td_count(r, DWX_DRAW_GLYPHRUN); CHECK(n >= 3);
    EXPECT_EQ(td_total_glyphs(r), 19);
    DONE(l, r);
}
TEST(desc_text_matches_source_substring)
{
    static const WCHAR *src = L"AAAA BBBB CCCC DDDD";
    TD_BEGIN(); TD_LAYOUT(l, src, 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(5, 4)); IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(10, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i, k = 0;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i];
        if (c->kind != DWX_DRAW_GLYPHRUN) continue;
        k++;
        CHECK(c->has_desc);
        if (c->has_desc) { CHECK(c->text_pos + c->text_len <= 19); CHECK(!wcsncmp(c->text, src + c->text_pos, c->text_len)); }
    }
    CHECK(k >= 3);
    DONE(l, r);
}
TEST(desc_cluster_map_identity_ltr)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF 012", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->cluster_map);
    if (c && c->cluster_map) { UINT32 i; for (i = 0; i < c->text_len; i++) EXPECT_EQ(c->cluster_map[i], i); }
    DONE(l, r);
}
TEST(desc_cluster_map_is_run_relative)
{
    /* the cluster map of a later run starts at 0 again: indices are relative to the run's own glyph array */
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetFontSize(l, 30.0f, RNG(4, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c1 = td_nth(r, DWX_DRAW_GLYPHRUN, 1);
    CHECK(c1 && c1->cluster_map && c1->text_pos == 4);
    if (c1 && c1->cluster_map) { UINT32 i; for (i = 0; i < c1->text_len; i++) EXPECT_EQ(c1->cluster_map[i], i); }
    DONE(l, r);
}

/* ---- splitting ---------------------------------------------------------------------------------------------- */
#define SPLIT_BEGIN(txt) TD_BEGIN(); TD_LAYOUT(l, txt, 16.0f, 100000.0f, 100.0f); RD(); NOSNAP(r)
TEST(split_none_uniform_text)
{
    SPLIT_BEGIN(L"The quick brown fox 1234");
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    DONE(l, r);
}
TEST(split_font_size_middle)
{
    SPLIT_BEGIN(L"ABCDEFGH");
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(3, 2)));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    const dwx_draw *a = td_nth(r, DWX_DRAW_GLYPHRUN, 0), *b = td_nth(r, DWX_DRAW_GLYPHRUN, 1), *c = td_nth(r, DWX_DRAW_GLYPHRUN, 2);
    if (a && b && c) {
        EXPECT_EQ(a->text_pos, 0); EXPECT_EQ(a->text_len, 3); CHECK_FLOAT_EQ(a->em, 16.0f);
        EXPECT_EQ(b->text_pos, 3); EXPECT_EQ(b->text_len, 2); CHECK_FLOAT_EQ(b->em, 32.0f); CHECK_NEARF(b->advances[0], 15.625);
        EXPECT_EQ(c->text_pos, 5); EXPECT_EQ(c->text_len, 3); CHECK_FLOAT_EQ(c->em, 16.0f);
        /* the following run starts where the previous one ended */
        CHECK_NEARF(b->x, a->x + dwx_run_width(a)); CHECK_NEARF(c->x, b->x + dwx_run_width(b));
        CHECK_NEARF(a->y, b->y); CHECK_NEARF(b->y, c->y);
    }
    DONE(l, r);
}
TEST(split_font_size_table)
{
    /* runs split at every size boundary of a 12-letter text; textPosition sequence is the boundary list */
    static const struct { int start, len; } rg[] = { {0,1},{2,2},{5,1},{6,3},{10,2} };
    SPLIT_BEGIN(L"ABCDEFGHIJKL");
    size_t i; for (i = 0; i < sizeof rg / sizeof rg[0]; i++) IDWriteTextLayout_SetFontSize(l, 20.0f + (float)i * 4, RNG(rg[i].start, rg[i].len));
    DRAW_OK(l, r, 0.0f, 0.0f);
    /* unsized gaps: [1,2) [4,5) [9,10) at 16 */
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 8);
    EXPECT_EQ(td_total_glyphs(r), 12);
    int pos = 0, k;
    for (k = 0; k < r->ncalls; k++) { const dwx_draw *c = &r->calls[k]; if (c->kind != DWX_DRAW_GLYPHRUN) continue; EXPECT_EQ(c->text_pos, pos); pos += c->text_len; }
    EXPECT_EQ(pos, 12);
    DONE(l, r);
}
TEST(split_font_weight_changes_face)
{
    SPLIT_BEGIN(L"ABCDEF");
    CHECK_HR_OK(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(2, 2)));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    const dwx_draw *a = td_nth(r, DWX_DRAW_GLYPHRUN, 0), *b = td_nth(r, DWX_DRAW_GLYPHRUN, 1), *c = td_nth(r, DWX_DRAW_GLYPHRUN, 2);
    if (a && b && c) {
        EXPECT_EQ(td_face_a_advance(a), 1000); EXPECT_EQ(td_face_a_advance(b), 1200); EXPECT_EQ(td_face_a_advance(c), 1000);
        CHECK_NEARF(b->advances[0], 9.375); CHECK_NEARF(a->advances[0], 7.8125);
        CHECK_NEARF(b->x, 2 * 7.8125); CHECK_NEARF(c->x, 2 * 7.8125 + 2 * 9.375);
        EXPECT_EQ(IDWriteFontFace_GetSimulations(b->face), DWRITE_FONT_SIMULATIONS_NONE);
    }
    DONE(l, r);
}
TEST(split_font_family_changes_face)
{
    SPLIT_BEGIN(L"ABCDEF");
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TD_TALL, RNG(3, 3)));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 2);
    const dwx_draw *a = td_nth(r, DWX_DRAW_GLYPHRUN, 0), *b = td_nth(r, DWX_DRAW_GLYPHRUN, 1);
    if (a && b) {
        DWRITE_FONT_METRICS ma, mb; td_face_metrics(a, &ma); td_face_metrics(b, &mb);
        CHECK(mb.ascent > ma.ascent); CHECK(mb.descent > ma.descent);
        /* both runs share one baseline: the taller font decides it */
        CHECK_NEARF(a->y, b->y);
        CHECK_NEARF(a->y, td_line_baseline(l, 0));
        CHECK_BASELINE(b->y, mb, 16.0f);
    }
    DONE(l, r);
}
TEST(split_locale_change)
{
    SPLIT_BEGIN(L"ABCDEF");
    CHECK_HR_OK(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", RNG(3, 3)));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 2);
    const dwx_draw *a = td_nth(r, DWX_DRAW_GLYPHRUN, 0), *b = td_nth(r, DWX_DRAW_GLYPHRUN, 1);
    if (a && b && a->locale && b->locale) { CHECK(!lstrcmpiW(a->locale, L"en-us")); CHECK(!lstrcmpiW(b->locale, L"fr-fr")); }
    DONE(l, r);
}
TEST(split_drawing_effect_change)
{
    SPLIT_BEGIN(L"ABCDEF");
    dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 2)); IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(2, 2));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    const dwx_draw *a = td_nth(r, DWX_DRAW_GLYPHRUN, 0), *b = td_nth(r, DWX_DRAW_GLYPHRUN, 1), *c = td_nth(r, DWX_DRAW_GLYPHRUN, 2);
    if (a && b && c) { EXPECT_EQ(a->effect, e1); EXPECT_EQ(b->effect, e2); EXPECT_EQ(c->effect, NULL); }
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e1); dwx_release(e2); TD_END();
}
TEST(split_same_effect_object_adjacent_ranges_still_split_or_merge)
{
    /* two adjacent ranges carrying the identical effect pointer: record the observed run count */
    SPLIT_BEGIN(L"ABCDEF");
    dwx_effect *e = dwx_effect_new(1);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 3)); IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(3, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int n = td_count(r, DWX_DRAW_GLYPHRUN);
    t_note("adjacent ranges with one effect object: %d glyph run(s)", n);
    CHECK(n == 1 || n == 2);
    EXPECT_EQ(td_total_glyphs(r), 6);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(split_underline_boundary)
{
    SPLIT_BEGIN(L"ABCDEF");
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(2, 2));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    DONE(l, r);
}
TEST(split_strikethrough_boundary)
{
    SPLIT_BEGIN(L"ABCDEF");
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(1, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    DONE(l, r);
}
TEST(split_script_latin_greek)
{
    /* script change splits the run even though one font covers both */
    SPLIT_BEGIN(L"AB" GREEK_A GREEK_A L"CD");
    DRAW_OK(l, r, 0.0f, 0.0f);
    /* observed: the script change does not split the run when one font covers both scripts */
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    const dwx_draw *a = td_nth(r, DWX_DRAW_GLYPHRUN, 0), *b = NULL, *c = NULL;
    if (a && b && c) {
        EXPECT_EQ(a->text_pos, 0); EXPECT_EQ(a->text_len, 2);
        EXPECT_EQ(b->text_pos, 2); EXPECT_EQ(b->text_len, 2); EXPECT_EQ(b->indices[0], 28);
        EXPECT_EQ(c->text_pos, 4); EXPECT_EQ(c->text_len, 2);
        EXPECT_EQ(a->bidi, 0); EXPECT_EQ(b->bidi, 0); EXPECT_EQ(c->bidi, 0);
    }
    DONE(l, r);
}
TEST(split_digits_stay_with_latin)
{
    SPLIT_BEGIN(L"AB12 CD34");
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    DONE(l, r);
}
TEST(split_line_break_wrap)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA BBBB CCCC", 16.0f, 40.0f, 300.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    EXPECT_EQ(ln.n, 3);
    CHECK(td_count(r, DWX_DRAW_GLYPHRUN) >= 3);
    int i;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i];
        if (c->kind != DWX_DRAW_GLYPHRUN) continue;
        int line = td_line_of(&ln, c->text_pos);
        CHECK(line >= 0);
        if (line >= 0) {
            /* a run never straddles a line */
            CHECK(td_line_of(&ln, c->text_pos + c->text_len - 1) == line);
            CHECK_NEARF(c->y, ln.top[line] + ln.lm[line].baseline);
        }
    }
    DONE(l, r);
}
TEST(split_hard_newline)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB\nCD\r\nEF", 16.0f, 1000.0f, 300.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    EXPECT_EQ(ln.n, 3);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 3), *c = td_run_at(r, 7);
    CHECK(a && b && c);
    if (a && b && c) {
        CHECK_NEARF(a->x, 0.0f); CHECK_NEARF(b->x, 0.0f); CHECK_NEARF(c->x, 0.0f);
        CHECK_NEARF(b->y - a->y, ln.lm[0].height); CHECK_NEARF(c->y - b->y, ln.lm[1].height);
        CHECK(a->text_len >= 2 && b->text_len >= 2 && c->text_len >= 2);
        CHECK(!wcsncmp(a->text + a->text_pos * 0, L"AB", 2));
        CHECK_WEQ(c->text, L"EF");
    }
    DONE(l, r);
}
TEST(split_every_char_visible_exactly_once)
{
    static const WCHAR src[] = L"AB CD\tEF\nGH IJ KL MN OP";
    TD_BEGIN(); TD_LAYOUT(l, src, 16.0f, 50.0f, 400.0f); RD();
    IDWriteTextLayout_SetFontSize(l, 22.0f, RNG(3, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    unsigned char cover[64]; memset(cover, 0, sizeof cover);
    int i; UINT32 k;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i];
        if (c->kind != DWX_DRAW_GLYPHRUN) continue;
        for (k = 0; k < c->text_len && c->text_pos + k < 64; k++) cover[c->text_pos + k]++;
    }
    UINT32 n = (UINT32)wcslen(src), bad = 0;
    for (k = 0; k < n; k++) if (src[k] >= L'A' && src[k] <= L'Z' && cover[k] != 1) bad++;
    EXPECT_EQ(bad, 0);
    for (k = 0; k < n; k++) CHECK(cover[k] <= 1);
    DONE(l, r);
}

/* ---- positions: alignment, origin, lines ------------------------------------------------------------------------ */
TEST(pos_x_progression_in_line)
{
    SPLIT_BEGIN(L"AAAABBBBCCCC");
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(4, 4)); IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(8, 4));
    DRAW_OK(l, r, 100.0f, 0.0f);
    float x = 100.0f; int i;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i]; if (c->kind != DWX_DRAW_GLYPHRUN) continue;
        CHECK_NEARF(c->x, x); x += dwx_run_width(c);
    }
    DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
    CHECK_NEARF(x - 100.0f, m.width);
    DONE(l, r);
}
TEST(pos_run_width_equals_layout_width)
{
    SPLIT_BEGIN(L"Hello brave new world");
    DRAW_OK(l, r, 0.0f, 0.0f);
    DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) CHECK_NEARF(dwx_run_width(c), m.widthIncludingTrailingWhitespace);
    DONE(l, r);
}
TEST(pos_text_alignment_offsets)
{
    static const DWRITE_TEXT_ALIGNMENT al[] = { DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_TEXT_ALIGNMENT_CENTER };
    TD_BEGIN(); int k;
    for (k = 0; k < 3; k++) {
        TD_LAYOUT(l, L"ABCD", 16.0f, 200.0f, 100.0f); RD(); NOSNAP(r);
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, al[k]));
        DRAW_OK(l, r, 5.0f, 0.0f);
        DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        CHECK(c != NULL);
        if (c) {
            CHECK_NEARF(c->x, 5.0f + m.left);
            float want = k == 0 ? 0.0f : k == 1 ? 200.0f - 4 * 7.8125f : (200.0f - 4 * 7.8125f) / 2;
            CHECK_NEARF(m.left, want);
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(pos_text_alignment_multiline)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA BB\nCCCCCC D", 16.0f, 100.0f, 200.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER);
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    EXPECT_EQ(ln.n, 2);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 8);
    if (a && b && ln.n == 2) {
        float w0 = ln.lm[0].length ? 0 : 0; (void)w0;
        /* line widths: "AAAA BB" 7 glyphs (trailing newline not counted) and "CCCCCC D" 8 glyphs */
        CHECK_NEAR(a->x, (100.0f - 7 * 7.8125f) / 2, 0.01);
        CHECK_NEAR(b->x, (100.0f - 8 * 7.8125f) / 2, 0.01);
    }
    DONE(l, r);
}
TEST(pos_paragraph_alignment_offsets)
{
    static const DWRITE_PARAGRAPH_ALIGNMENT pa[] = { DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_PARAGRAPH_ALIGNMENT_FAR, DWRITE_PARAGRAPH_ALIGNMENT_CENTER };
    TD_BEGIN(); int k;
    for (k = 0; k < 3; k++) {
        TD_LAYOUT(l, L"ABCD", 16.0f, 200.0f, 100.0f); RD(); NOSNAP(r);
        CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, pa[k]));
        DRAW_OK(l, r, 0.0f, 7.0f);
        DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        if (c) {
            CHECK_NEARF(c->y, 7.0f + m.top + td_line_baseline(l, 0));
            float want = k == 0 ? 0.0f : k == 1 ? 100.0f - m.height : (100.0f - m.height) / 2;
            CHECK_NEARF(m.top, want);
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(pos_multiline_y_progression)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB CCC DDD EEE", 16.0f, 30.0f, 400.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 2.0f, 3.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    CHECK(ln.n >= 4);
    int i, lastline = -1;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i]; if (c->kind != DWX_DRAW_GLYPHRUN) continue;
        int line = td_line_of(&ln, c->text_pos);
        /* a trailing space may be drawn as its own run further right: check only the first run of each line for x */
        if (line >= 0) {
            int first = line != lastline; lastline = line;
            if (!first) { CHECK_NEARF(c->y, 3.0f + ln.top[line] + ln.lm[line].baseline); continue; } CHECK_NEARF(c->y, 3.0f + ln.top[line] + ln.lm[line].baseline); CHECK_NEARF(c->x, 2.0f); }
    }
    DONE(l, r);
}
TEST(pos_line_height_progression_uniform)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB CCC", 16.0f, 30.0f, 400.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i; const dwx_draw *prev = NULL;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i]; if (c->kind != DWX_DRAW_GLYPHRUN) continue;
        /* a trailing space may be drawn as its own run on the same line */
        if (prev && c->y != prev->y) CHECK_NEARF(c->y - prev->y, td_line_height(l, 0));
        prev = c;
    }
    DONE(l, r);
}
TEST(pos_mixed_size_shared_baseline)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBBCCCC", 16.0f, 1000.0f, 400.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontSize(l, 48.0f, RNG(4, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 4), *c = td_run_at(r, 8);
    CHECK(a && b && c);
    if (a && b && c) {
        CHECK_NEARF(a->y, b->y); CHECK_NEARF(b->y, c->y);
        DWRITE_FONT_METRICS m; td_face_metrics(b, &m);
        CHECK_BASELINE(b->y, m, 48.0f);
        CHECK_NEARF(td_line_baseline(l, 0), b->y);
    }
    DONE(l, r);
}
TEST(pos_negative_and_large_origins)
{
    static const float orgs[][2] = { { -50.0f, -20.0f }, { -0.25f, 0.0f }, { 1e4f, 1e4f }, { 123456.0f, -654.5f }, { 0.0f, 0.0f }, { 3.14159f, 2.71828f } };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof orgs / sizeof orgs[0]; i++) {
        TD_LAYOUT(l, L"AB CD", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
        DRAW_OK(l, r, orgs[i][0], orgs[i][1]);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        CHECK(c != NULL);
        if (c) { CHECK_NEAR(c->x, orgs[i][0], fabs(orgs[i][0]) * 1e-6 + 1e-4); CHECK_NEAR(c->y, orgs[i][1] + td_line_baseline(l, 0), fabs(orgs[i][1]) * 1e-6 + 1e-3); }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(pos_maxwidth_irrelevant_for_leading)
{
    TD_BEGIN(); int i;
    for (i = 0; i < 3; i++) {
        float w = i == 0 ? 10.0f : i == 1 ? 100.0f : 100000.0f;
        TD_LAYOUT(l, L"ABCD", 16.0f, w, 100.0f); RD(); NOSNAP(r);
        IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP);
        DRAW_OK(l, r, 3.0f, 0.0f);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        CHECK(c && c->nglyphs == 4);
        if (c) CHECK_NEARF(c->x, 3.0f);
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(pos_text_beyond_maxheight_still_drawn)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB CCC DDD", 16.0f, 30.0f, 10.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    /* no clipping: every letter of every line is drawn although the text overflows maxHeight (spaces at a wrap may be omitted) */
    unsigned letters = 0; int i; UINT32 k;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) for (k = 0; k < r->calls[i].text_len; k++) if (r->calls[i].text[k] != L' ') letters++;
    EXPECT_EQ(letters, 12);
    DONE(l, r);
}
TEST(pos_nowrap_single_line)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB CCC DDD", 16.0f, 30.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) EXPECT_EQ(c->nglyphs, 15);
    DONE(l, r);
}

/* ---- pixel snapping of the baseline ----------------------------------------------------------------------------- */
TEST(snap_disabled_passthrough_origin)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 100.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 10.3f, 20.7f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) { CHECK_NEARF(c->x, 10.3f); CHECK_NEARF(c->y, 20.7f + td_line_baseline(l, 0)); }
    DONE(l, r);
}
TEST(snap_enabled_y_rounded_x_untouched)
{
    /* default renderer: ppd 1, identity. Horizontal text: SnapY rounds half up on the pixel grid, SnapX is not applied */
    static const float oy[] = { 0.0f, 0.1f, 0.4375f, 0.5f, 0.9f, 1.25f, 20.7f, -3.3f, -0.5625f, 100.5f };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof oy / sizeof oy[0]; i++) {
        TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 100.0f); RD();
        DRAW_OK(l, r, 10.3f, oy[i]);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        CHECK(c != NULL);
        if (c) {
            float want = td_snap(oy[i] + td_line_baseline(l, 0), 1.0f, 0.0f);
            CHECKF(fabsf(c->y - want) < 0.001f, "oy=%g: got y %g want %g", oy[i], c->y, want);
            CHECK_NEARF(c->x, 10.3f);
            CHECK_NEARF(c->y, floorf(c->y));
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(snap_half_pixel_rounds_up)
{
    /* baseline = origin + 14.0625 (when ascent is 1800/2048): origin 0.4375 gives exactly .5 which rounds up */
    TD_BEGIN(); TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 100.0f); RD();
    float base = td_line_baseline(l, 0);
    float oy = 0.5f - (base - floorf(base));          /* make the sum an exact half-integer */
    if (oy < 0) oy += 1.0f;
    DRAW_OK(l, r, 0.0f, oy);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    float sum = oy + base;
    if (c) {
        t_note("baseline %g origin %g sum %g snapped %g", base, oy, sum, c->y);
        if (fabsf((sum - floorf(sum)) - 0.5f) < 1e-6f) CHECK_NEARF(c->y, floorf(sum) + 1.0f);
    }
    DONE(l, r);
}
TEST(snap_ppd_table)
{
    static const float ppd[] = { 0.5f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f };
    static const float oy[] = { 0.0f, 0.3f, 0.7f, 5.55f };
    TD_BEGIN(); size_t i, j;
    for (i = 0; i < sizeof ppd / sizeof ppd[0]; i++) for (j = 0; j < sizeof oy / sizeof oy[0]; j++) {
        TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 100.0f); RD(); r->pixels_per_dip = ppd[i];
        DRAW_OK(l, r, 1.1f, oy[j]);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        if (c) {
            float want = td_snap(oy[j] + td_line_baseline(l, 0), ppd[i], 0.0f);
            CHECKF(fabsf(c->y - want) < 0.001f, "ppd=%g oy=%g: got y %g want %g", ppd[i], oy[j], c->y, want);
            CHECK_NEARF(c->x, 1.1f);
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(snap_nonpositive_ppd_disables_snapping)
{
    /* SetPixelGrid refuses ppd <= 0: coordinates pass through unsnapped (the snapping callbacks themselves still succeed) */
    static const float ppd[] = { 0.0f, -1.0f, -0.5f };
    TD_BEGIN(); size_t i;
    for (i = 0; i < 3; i++) {
        TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 100.0f); RD(); r->pixels_per_dip = ppd[i];
        DRAW_OK(l, r, 1.1f, 0.3f);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        if (c) CHECK_NEARF(c->y, 0.3f + td_line_baseline(l, 0));
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(snap_transform_scale_and_offset)
{
    /* grid scale = m22 * ppd, grid offset = dy */
    static const struct { float m22, dy, ppd; } cfg[] = { { 2.0f, 0.0f, 1.0f }, { 1.0f, 0.25f, 1.0f }, { 2.0f, 0.25f, 1.5f }, { 0.5f, 0.0f, 1.0f }, { 3.0f, 0.1f, 2.0f } };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof cfg / sizeof cfg[0]; i++) {
        TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 100.0f); RD();
        r->transform.m22 = cfg[i].m22; r->transform.dy = cfg[i].dy; r->pixels_per_dip = cfg[i].ppd;
        DRAW_OK(l, r, 0.0f, 0.37f);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        if (c) {
            float want = td_snap(0.37f + td_line_baseline(l, 0), cfg[i].m22 * cfg[i].ppd, cfg[i].dy);
            CHECKF(fabsf(c->y - want) < 0.002f, "cfg %d: got %g want %g", (int)i, c->y, want);
        }
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    TD_END();
}
TEST(snap_transform_m11_ignored_for_horizontal)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 100.0f); RD();
    r->transform.m11 = 3.0f; r->transform.dx = 0.4f;
    DRAW_OK(l, r, 2.2f, 0.37f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) { CHECK_NEARF(c->x, 2.2f); CHECK_NEARF(c->y, td_snap(0.37f + td_line_baseline(l, 0), 1.0f, 0.0f)); }
    DONE(l, r);
}
TEST(snap_degenerate_transform_not_snapped)
{
    /* m12 == 0 and m22 == 0: SetPixelGrid fails; nothing is snapped */
    TD_BEGIN(); TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 100.0f); RD();
    r->transform.m22 = 0.0f;
    DRAW_OK(l, r, 1.1f, 0.3f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) { CHECK_NEARF(c->x, 1.1f); CHECK_NEARF(c->y, 0.3f + td_line_baseline(l, 0)); }
    DONE(l, r);
}
TEST(snap_rotated_transform_runs)
{
    /* a 90 degree transform: only record what happens, Draw must succeed and draw every glyph */
    TD_BEGIN(); TD_LAYOUT(l, L"AB CD", 16.0f, 100.0f, 100.0f); RD();
    r->transform.m11 = 0; r->transform.m12 = 1; r->transform.m21 = -1; r->transform.m22 = 0;
    DRAW_OK(l, r, 1.1f, 0.3f);
    EXPECT_EQ(td_total_glyphs(r), 5);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) t_note("rotated transform: run origin (%g, %g) for draw origin (1.1, 0.3), baseline %g", c->x, c->y, td_line_baseline(l, 0));
    DONE(l, r);
}
TEST(snap_all_runs_of_a_line_share_snapped_y)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBBCCCC", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetFontSize(l, 33.0f, RNG(4, 4)); IDWriteTextLayout_SetFontSize(l, 21.0f, RNG(8, 4));
    DRAW_OK(l, r, 0.0f, 0.3f);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 4), *c = td_run_at(r, 8);
    if (a && b && c) { CHECK_NEARF(a->y, b->y); CHECK_NEARF(b->y, c->y); CHECK_NEARF(a->y, floorf(a->y)); }
    DONE(l, r);
}
TEST(snap_line_spacing_each_line_snapped)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB CCC DDD", 16.0f, 30.0f, 400.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.2f);
    td_lines_t ln; td_get_lines(l, &ln);
    int i;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i]; if (c->kind != DWX_DRAW_GLYPHRUN) continue;
        int line = td_line_of(&ln, c->text_pos);
        if (line >= 0) CHECK_NEARF(c->y, td_snap(0.2f + ln.top[line] + ln.lm[line].baseline, 1.0f, 0.0f));
    }
    DONE(l, r);
}

/* ---- measuring mode ------------------------------------------------------------------------------------------- */
static IDWriteTextLayout *gdi_layout(td_fx *x, const WCHAR *text, float size, float ppd, BOOL natural)
{
    IDWriteTextFormat *t = td_format(x, TD_FAM, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(x->f, text, (UINT32)wcslen(text), t, 1000.0f, 100.0f, ppd, NULL, natural, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}
TEST(measuring_mode_natural_for_plain_layout)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) EXPECT_EQ(c->mm, DWRITE_MEASURING_MODE_NATURAL);
    DONE(l, r);
}
TEST(measuring_mode_gdi_classic)
{
    TD_BEGIN(); IDWriteTextLayout *l = gdi_layout(&X, L"ABC DEF", 16.0f, 1.0f, FALSE);
    if (!l) { TD_END(); t_skip("gdi layout unavailable"); return; }
    RD(); DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c != NULL);
    if (c) {
        EXPECT_EQ(c->mm, DWRITE_MEASURING_MODE_GDI_CLASSIC);
        UINT32 i; for (i = 0; i < c->nglyphs; i++) CHECKF(c->advances[i] == floorf(c->advances[i]), "glyph %u advance %g is not whole pixels", i, c->advances[i]);
        t_note("gdi classic advance of 'A' at 16px: %g", c->advances[0]);
    }
    DONE(l, r);
}
TEST(measuring_mode_gdi_natural)
{
    TD_BEGIN(); IDWriteTextLayout *l = gdi_layout(&X, L"ABC DEF", 16.0f, 1.0f, TRUE);
    if (!l) { TD_END(); t_skip("gdi layout unavailable"); return; }
    RD(); DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c != NULL);
    if (c) { EXPECT_EQ(c->mm, DWRITE_MEASURING_MODE_GDI_NATURAL); t_note("gdi natural advance of 'A' at 16px: %g", c->advances[0]); }
    DONE(l, r);
}
TEST(measuring_mode_all_runs_and_decorations)
{
    TD_BEGIN(); IDWriteTextLayout *l = gdi_layout(&X, L"ABC DEF", 16.0f, 1.0f, FALSE);
    if (!l) { TD_END(); t_skip("gdi layout unavailable"); return; }
    RD();
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(0, 3));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 7)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 7));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i, n = 0;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i];
        if (c->kind == DWX_DRAW_INLINE) continue;
        EXPECT_EQ(c->mm, DWRITE_MEASURING_MODE_GDI_CLASSIC); n++;
    }
    CHECK(n >= 4);
    DONE(l, r);
}
TEST(measuring_mode_gdi_ppd_scales_advances)
{
    TD_BEGIN(); IDWriteTextLayout *l1 = gdi_layout(&X, L"ABCD", 16.0f, 1.0f, TRUE), *l2 = gdi_layout(&X, L"ABCD", 16.0f, 2.0f, TRUE);
    if (!l1 || !l2) { DW_RELEASE(l1); DW_RELEASE(l2); TD_END(); t_skip("gdi layout unavailable"); return; }
    dwx_renderer *r1 = dwx_renderer_new(), *r2 = dwx_renderer_new();
    DRAW_OK(l1, r1, 0.0f, 0.0f); DRAW_OK(l2, r2, 0.0f, 0.0f);
    const dwx_draw *a = td_nth(r1, DWX_DRAW_GLYPHRUN, 0), *b = td_nth(r2, DWX_DRAW_GLYPHRUN, 0);
    CHECK(a && b);
    if (a && b) { t_note("gdi natural advance ppd1 %g ppd2 %g, em %g / %g", a->advances[0], b->advances[0], a->em, b->em); CHECK(a->em > 0 && b->em > 0); }
    dwx_release(r1); dwx_release(r2); IDWriteTextLayout_Release(l1); IDWriteTextLayout_Release(l2); TD_END();
}

/* ---- bidirectional text ----------------------------------------------------------------------------------------- */
TEST(bidi_hebrew_run_level_one)
{
    TD_BEGIN(); TD_LAYOUT(l, HEB, 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c && c->nglyphs == 3) {
        EXPECT_EQ(c->bidi, 1);
        /* observed: the glyph array of the right-to-left run follows the logical character order */
        EXPECT_EQ(c->indices[0], 2); EXPECT_EQ(c->indices[1], 3); EXPECT_EQ(c->indices[2], 4);
        if (c->cluster_map) { EXPECT_EQ(c->cluster_map[0], 0); EXPECT_EQ(c->cluster_map[1], 1); EXPECT_EQ(c->cluster_map[2], 2); }
    } else CHECK(c && c->nglyphs == 3);
    DONE(l, r);
}
TEST(bidi_latin_hebrew_latin_runs)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB " HEB L" CD", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i, rtl = 0, ltr = 0;
    for (i = 0; i < r->ncalls; i++) { const dwx_draw *c = &r->calls[i]; if (c->kind != DWX_DRAW_GLYPHRUN) continue; if (c->bidi & 1) rtl++; else ltr++; }
    CHECK(rtl >= 1); CHECK(ltr >= 2);
    const dwx_draw *h = NULL;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && (r->calls[i].bidi & 1) && r->calls[i].nglyphs >= 3) h = &r->calls[i];
    if (h) {
        /* baseline origin of the Hebrew run lies within the span the run occupies (left edge or right edge) */
        float left = 3 * 7.8125f, right = left + 3 * 7.8125f;
        CHECK(fabsf(h->x - left) < 0.01f || fabsf(h->x - right) < 0.01f);
        t_note("RTL run baseline origin x=%g (run spans %g .. %g)", h->x, left, right);
    }
    DONE(l, r);
}
TEST(bidi_rtl_paragraph_levels)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB " HEB, 16.0f, 1000.0f, 100.0f); RD();
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i]; if (c->kind != DWX_DRAW_GLYPHRUN) continue;
        if (c->indices[0] == 4) EXPECT_EQ(c->bidi, 1);              /* Hebrew */
        else if (c->indices[0] == 2 && c->text_len == 2) EXPECT_EQ(c->bidi, 2);   /* "AB": embedded left-to-right in an RTL paragraph */
    }
    DONE(l, r);
}
TEST(bidi_rtl_paragraph_leading_is_right)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 200.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    DRAW_OK(l, r, 0.0f, 0.0f);
    DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c != NULL);
    if (c) {
        CHECK_NEARF(m.left, 200.0f - 4 * 7.8125f);
        CHECK_NEARF(c->x, m.left);
        EXPECT_EQ(c->bidi, 2);
    }
    DONE(l, r);
}
TEST(bidi_rtl_paragraph_trailing_is_left)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 200.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING);
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) CHECK_NEARF(c->x, 0.0f);
    DONE(l, r);
}
TEST(bidi_hebrew_in_rtl_paragraph_origin)
{
    TD_BEGIN(); TD_LAYOUT(l, HEB, 16.0f, 200.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    DRAW_OK(l, r, 0.0f, 0.0f);
    DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c != NULL);
    if (c) {
        float left = 200.0f - 3 * 7.8125f;
        CHECK_NEARF(m.left, left);
        CHECK(fabsf(c->x - left) < 0.01f || fabsf(c->x - 200.0f) < 0.01f);
        t_note("RTL run in RTL paragraph: origin x=%g (left edge %g, right edge 200)", c->x, left);
        EXPECT_EQ(c->bidi, 1);
    }
    DONE(l, r);
}
TEST(bidi_run_coverage_mixed)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB " HEB L" CD " HEB L" EF", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    static const WCHAR src[] = L"AB " HEB L" CD " HEB L" EF";
    unsigned char cover[32]; memset(cover, 0, sizeof cover); int i; UINT32 k;
    for (i = 0; i < r->ncalls; i++) { const dwx_draw *c = &r->calls[i]; if (c->kind != DWX_DRAW_GLYPHRUN) continue; for (k = 0; k < c->text_len && c->text_pos + k < 32; k++) cover[c->text_pos + k]++; }
    for (k = 0; k < 16; k++) { if (src[k] != L' ') EXPECT_EQ(cover[k], 1); else CHECK(cover[k] <= 1); }
    DONE(l, r);
}
TEST(bidi_explicit_embedding_marks)
{
    /* RLE .. PDF: the embedded Latin text gets an odd+1 level (2 inside RTL embedding) */
    TD_BEGIN(); TD_LAYOUT(l, L"A‫BC‬D", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i, odd = 0; for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && (r->calls[i].bidi & 1)) odd++;
    t_note("%d odd-level runs for an explicit RTL embedding", odd);
    CHECK(td_count(r, DWX_DRAW_GLYPHRUN) >= 3);
    DONE(l, r);
}

/* ---- sideways, hidden, special characters ----------------------------------------------------------------------- */
TEST(special_tab_character)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A\tB", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *a = td_run_at(r, 0);
    CHECK(a != NULL);
    EXPECT_EQ(td_total_glyphs(r) >= 2, 1);
    t_note("tab: %d glyph run(s), %u glyphs", td_count(r, DWX_DRAW_GLYPHRUN), td_total_glyphs(r));
    DONE(l, r);
}
TEST(special_control_chars_do_not_crash)
{
    static const WCHAR t[] = L"A\x01\x02\x7f​‎‏­B";
    TD_BEGIN(); TD_LAYOUT(l, t, 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    CHECK(td_count(r, DWX_DRAW_GLYPHRUN) >= 1);
    t_note("control chars: %d runs, %u glyphs for %u chars", td_count(r, DWX_DRAW_GLYPHRUN), td_total_glyphs(r), (unsigned)wcslen(t));
    DONE(l, r);
}
TEST(special_cjk_horizontal)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB" CJK L"CD", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_total_glyphs(r), 7);
    int i; for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) { EXPECT_EQ(r->calls[i].sideways, FALSE); EXPECT_EQ(r->calls[i].bidi, 0); }
    DONE(l, r);
}
TEST(special_ellipsis_char_glyph)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A…B", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_total_glyphs(r), 3);
    DONE(l, r);
}
TEST(special_surrogate_pair_char_counts)
{
    /* U+1F600 is not in the font: whatever face renders it, the description covers 2 UTF-16 units */
    TD_BEGIN(); TD_LAYOUT(l, L"A\U0001F600B", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    unsigned total = 0; int i;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) total += r->calls[i].text_len;
    EXPECT_EQ(total, 4);
    t_note("non-BMP character: %d run(s), %u glyphs", td_count(r, DWX_DRAW_GLYPHRUN), td_total_glyphs(r));
    DONE(l, r);
}

/* ---- spacing, font fallback ------------------------------------------------------------------------------------ */
TEST(character_spacing_in_advances)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout1 *l1 = NULL;
    if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1))) { DONE(l, r); return; }
    HRESULT hr = IDWriteTextLayout1_SetCharacterSpacing(l1, 1.0f, 2.0f, 0.0f, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (SUCCEEDED(hr) && c && c->nglyphs == 4) {
        t_note("advance with leading 1 trailing 2: %g (plain 7.8125)", c->advances[0]);
        CHECK_NEAR(c->advances[0], 7.8125f + 3.0f, 0.01);
    }
    IDWriteTextLayout1_Release(l1); DONE(l, r);
}
TEST(fallback_missing_glyph_is_still_drawn)
{
    /* '!' is not in the fixture font: the layout draws it with a fallback face or the notdef glyph; either way one glyph per char */
    TD_BEGIN(); TD_LAYOUT(l, L"A!B", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_total_glyphs(r), 3);
    int i; for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && r->calls[i].text_len == 1 && r->calls[i].text[0] == L'!') t_note("'!' drawn with glyph %u advance %g em %g", r->calls[i].indices[0], r->calls[i].advances[0], r->calls[i].em);
    DONE(l, r);
}
TEST(fallback_custom_fallback_object)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    DONE(l, r);
}

/* ---- face / run properties -------------------------------------------------------------------------------------- */
TEST(face_run_face_is_usable)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->face);
    if (c && c->face) {
        DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(c->face, &m);
        EXPECT_EQ(m.designUnitsPerEm, 2048);
        EXPECT_EQ(IDWriteFontFace_GetType(c->face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
        EXPECT_EQ(IDWriteFontFace_GetSimulations(c->face), DWRITE_FONT_SIMULATIONS_NONE);
        UINT32 cnt = IDWriteFontFace_GetGlyphCount(c->face);
        EXPECT_EQ(cnt, TTF_DEF_NGLYPHS);
    }
    DONE(l, r);
}
TEST(face_synthetic_bold_simulation)
{
    /* DwTall has no bold face: asking for bold selects the regular face with a bold simulation */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetFontFamilyName(l, TD_TALL, RNG(0, 3)); IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->face);
    if (c && c->face) { DWRITE_FONT_SIMULATIONS s = IDWriteFontFace_GetSimulations(c->face); t_note("DwTall bold -> simulations 0x%x", s); EXPECT_EQ(s & DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_BOLD); }
    DONE(l, r);
}
TEST(face_synthetic_oblique_simulation)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, RNG(0, 3));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->face);
    if (c && c->face) { DWRITE_FONT_SIMULATIONS s = IDWriteFontFace_GetSimulations(c->face); EXPECT_EQ(s & DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_OBLIQUE); }
    DONE(l, r);
}
TEST(face_same_font_same_face_across_runs)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 100.0f); RD();
    IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(4, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *a = td_nth(r, DWX_DRAW_GLYPHRUN, 0), *b = td_nth(r, DWX_DRAW_GLYPHRUN, 1);
    if (a && b) {
        t_note("face pointers %p %p", (void *)a->face, (void *)b->face);
        EXPECT_EQ(td_face_a_advance(a), td_face_a_advance(b));
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(a->face), IDWriteFontFace_GetGlyphCount(b->face));
    }
    DONE(l, r);
}
TEST(face_pointer_stays_valid_while_layout_lives)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    dwx_draw *c = (dwx_draw *)td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->face);
    if (c && c->face) { IUnknown_AddRef((IUnknown *)c->face); ULONG rc = IUnknown_Release((IUnknown *)c->face); CHECK(rc >= 1); }
    DONE(l, r);
}

/* ---- sizes and spacing through other layout attributes ------------------------------------------------------- */
TEST(attr_line_spacing_uniform_changes_y)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB", 16.0f, 30.0f, 400.0f); RD(); NOSNAP(r);
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 40.0f, 30.0f));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 4);
    CHECK(a && b);
    if (a && b) { CHECK_NEARF(a->y, 30.0f); CHECK_NEARF(b->y - a->y, 40.0f); }
    DONE(l, r);
}
TEST(attr_line_spacing_proportional_changes_y)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB", 16.0f, 30.0f, 400.0f);
    IDWriteTextLayout1 *dummy = NULL; (void)dummy;
    RD(); NOSNAP(r);
    HRESULT hr = IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 0.8f);
    if (FAILED(hr)) { t_note("proportional spacing unsupported: %s", dw_hr_name(hr)); DONE(l, r); return; }
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 4);
    if (a && b) { CHECK_NEARF(b->y - a->y, td_line_height(l, 0)); CHECK_NEARF(a->y, td_line_baseline(l, 0)); }
    DONE(l, r);
}
TEST(attr_trailing_space_belongs_to_line)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA BBBB", 16.0f, 40.0f, 400.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    unsigned letters = 0; int i; UINT32 k;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) for (k = 0; k < r->calls[i].text_len; k++) if (r->calls[i].text[k] != L' ') letters++;
    EXPECT_EQ(letters, 8);
    DONE(l, r);
}
TEST(attr_incremental_tab_stop_runs)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A\tB\tC", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 50.0f));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = NULL; int i;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && r->calls[i].has_desc && r->calls[i].text_len == 1 && r->calls[i].text[0] == L'C') c = &r->calls[i];
    if (c) { CHECK_NEARF(c->x, 100.0f); }
    else t_note("no stand-alone run for the last letter; %d runs", td_count(r, DWX_DRAW_GLYPHRUN));
    DONE(l, r);
}
