/* IDWriteTextAnalyzer1::GetBaseline.
 *
 * Windows 10 (DWriteTextAnalyzer::GetBaseline + BaseTable::GetBaseline):
 *   - *baselineCoordinate and *exists are cleared first (the pointers must be valid); fontFace == NULL or baseline > 8
 *     (DWRITE_BASELINE_MAXIMUM) -> E_INVALIDARG with both outputs left cleared.
 *   - DEFAULT is vertical ? CENTRAL : ROMAN. The locale is ignored for kinds 0..6 (NULL is treated as "").
 *   - exists = the font's BASE table defines the requested baseline for the script (and axis); it is only TRUE for kinds the
 *     table lists. Without a table, or with exists == FALSE and isSimulationAllowed == FALSE, the coordinate is 0.
 *   - With simulation, missing values are derived from the font metrics (design units, M = DWRITE_FONT_METRICS1):
 *       horizontal text: roman 0, ideographic bottom -descent, ideographic top ascent, central/math (bottom + top) / 2,
 *                        hanging (capHeight + xHeight) / 2 + roman, minimum -descent, maximum ascent;
 *       vertical text:   ideographic bottom 0, top ascent + descent, central top / 2, roman ((descent - ascent + 1) >> 1) + central,
 *                        minimum 0, maximum ascent + descent;
 *     and the returned coordinate is the value negated for horizontal text and the value itself for vertical text.
 *   - BASE data (horizontal axis, latn): the table's own roman / ideographic bottom replace the derived ones and, for horizontal
 *     text, are returned negated; ideographic top becomes unitsPerEm + ideographicBottom.
 */
#include "ta.h"

typedef struct bfix { dwx_font h; IDWriteFontFace1 *f1; DWRITE_FONT_METRICS1 m; DWRITE_SCRIPT_ANALYSIS latin; } bfix;
static int bf_open(bfix *b, ta_ctx *c, const ttf_spec *s) {
    memset(b, 0, sizeof *b);
    if (FAILED(ta_open_spec(&b->h, c->f, s)) || !b->h.face) return 0;
    if (FAILED(IDWriteFontFace_QueryInterface(b->h.face, &IID_IDWriteFontFace1, (void **)&b->f1)) || !b->f1) return 0;
    IDWriteFontFace1_GetMetrics(b->f1, &b->m);
    b->latin = ta_sa_of(c, L"A", 1);
    return b->latin.script != 0xFFFF;
}
static void bf_close(bfix *b) { DW_RELEASE(b->f1); dwx_font_close(&b->h); }
#define NEED_BF(spec_setup) \
    ttf_spec s_; ttf_default_spec(&s_); spec_setup; bfix bf; \
    if (!bf_open(&bf, &c_, &s_)) { bf_close(&bf); TA_END(); t_skip("fixture unavailable"); return; }

static HRESULT gb(IDWriteTextAnalyzer1 *a1, bfix *b, int kind, BOOL vert, BOOL sim, DWRITE_SCRIPT_ANALYSIS sa, const WCHAR *loc, INT32 *v, BOOL *ex) {
    return IDWriteTextAnalyzer1_GetBaseline(a1, b->h.face, (DWRITE_BASELINE)kind, vert, sim, sa, loc, v, ex);
}
/* expected simulated coordinate from metrics only (no BASE data) */
static int exp_nobase(int kind, int vertical, const DWRITE_FONT_METRICS1 *m) {
    int A = m->ascent, D = m->descent, cap = m->capHeight, xh = m->xHeight, v[9] = { 0 };
    if (!vertical) { v[5] = -D; v[6] = A; v[2] = (v[5] + v[6]) >> 1; v[3] = v[2]; v[1] = 0; v[4] = ((cap + xh) >> 1) + v[1]; v[8] = A; v[7] = -D; }
    else { v[5] = 0; v[6] = A + D; v[2] = (v[5] + v[6]) >> 1; v[3] = v[2]; v[1] = ((D - A + 1) >> 1) + v[2]; v[4] = ((cap + xh) >> 1) + v[1]; v[8] = A + D; v[7] = 0; }
    if (kind == 0) kind = vertical ? 2 : 1;
    return vertical ? v[kind] : -v[kind];
}
/* expected with BASE table: roman R, ideographic bottom I (horizontal axis only) */
static int exp_base(int kind, const DWRITE_FONT_METRICS1 *m, int R, int I) {
    int A = m->ascent, D = m->descent, U = m->designUnitsPerEm, cap = m->capHeight, xh = m->xHeight, v[9] = { 0 };
    v[1] = R; v[5] = I; v[6] = U + I; v[2] = (v[5] + v[6]) >> 1; v[3] = v[2]; v[4] = ((cap + xh) >> 1) + v[1]; v[7] = -D; v[8] = A;
    if (kind == 0) kind = 1;
    return -v[kind];
}
static const char *kname(int k) {
    static const char *n[] = { "DEFAULT", "ROMAN", "CENTRAL", "MATH", "HANGING", "IDEOGRAPHIC_BOTTOM", "IDEOGRAPHIC_TOP", "MINIMUM", "MAXIMUM" };
    return k >= 0 && k <= 8 ? n[k] : "?";
}

TEST(gb_enum_values) {
    EXPECT_EQ(DWRITE_BASELINE_DEFAULT, 0); EXPECT_EQ(DWRITE_BASELINE_ROMAN, 1); EXPECT_EQ(DWRITE_BASELINE_CENTRAL, 2); EXPECT_EQ(DWRITE_BASELINE_MATH, 3);
    EXPECT_EQ(DWRITE_BASELINE_HANGING, 4); EXPECT_EQ(DWRITE_BASELINE_IDEOGRAPHIC_BOTTOM, 5); EXPECT_EQ(DWRITE_BASELINE_IDEOGRAPHIC_TOP, 6);
    EXPECT_EQ(DWRITE_BASELINE_MINIMUM, 7); EXPECT_EQ(DWRITE_BASELINE_MAXIMUM, 8);
}
TEST(gb_null_face_invalid_outputs_cleared) {
    TA_BEGIN(); INT32 v = 77; BOOL ex = 1; DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1);
    CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, NULL, DWRITE_BASELINE_ROMAN, FALSE, TRUE, sa, L"en-us", &v, &ex), TA_E_INVALIDARG);
    EXPECT_EQ(v, 0); EXPECT_EQ(ex, 0);
    TA_END();
}
TEST(gb_invalid_baseline_values) {
    TA_BEGIN(); NEED_BF((void)0); static const UINT32 bad[] = { 9, 10, 100, 255, 256, 0x7fffffff }; unsigned i; int vert;
    for (vert = 0; vert < 2; vert++) for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        INT32 v = 77; BOOL ex = 1;
        CHECK_HR(gb(a1, &bf, bad[i], vert, TRUE, bf.latin, L"en-us", &v, &ex), TA_E_INVALIDARG);
        EXPECT_EQ(v, 0); EXPECT_EQ(ex, 0);
    }
    bf_close(&bf); TA_END();
}
TEST(gb_all_valid_kinds_succeed) {
    TA_BEGIN(); NEED_BF((void)0); int k, vert, sim;
    for (k = 0; k <= 8; k++) for (vert = 0; vert < 2; vert++) for (sim = 0; sim < 2; sim++) {
        INT32 v = 77; BOOL ex = 5; CHECKF(gb(a1, &bf, k, vert, sim, bf.latin, L"en-us", &v, &ex) == S_OK, "kind %d vert %d sim %d", k, vert, sim);
    }
    bf_close(&bf); TA_END();
}
TEST(gb_no_base_table_no_simulation_is_zero) {
    TA_BEGIN(); NEED_BF((void)0); int k, vert;
    for (vert = 0; vert < 2; vert++) for (k = 0; k <= 8; k++) {
        INT32 v = 77; BOOL ex = 5;
        CHECK_HR(gb(a1, &bf, k, vert, FALSE, bf.latin, L"en-us", &v, &ex), S_OK);
        CHECKF(v == 0 && ex == 0, "%s vert %d: coordinate %d exists %d", kname(k), vert, v, ex);
    }
    bf_close(&bf); TA_END();
}
TEST(gb_no_base_table_simulated_horizontal) {
    TA_BEGIN(); NEED_BF((void)0); int k;
    for (k = 0; k <= 8; k++) {
        INT32 v = 77; BOOL ex = 5;
        CHECK_HR(gb(a1, &bf, k, FALSE, TRUE, bf.latin, L"en-us", &v, &ex), S_OK);
        CHECKF(v == exp_nobase(k, 0, &bf.m) && ex == 0, "%s: coordinate %d (want %d) exists %d", kname(k), v, exp_nobase(k, 0, &bf.m), ex);
    }
    bf_close(&bf); TA_END();
}
TEST(gb_no_base_table_simulated_vertical) {
    TA_BEGIN(); NEED_BF((void)0); int k;
    for (k = 0; k <= 8; k++) {
        INT32 v = 77; BOOL ex = 5;
        CHECK_HR(gb(a1, &bf, k, TRUE, TRUE, bf.latin, L"en-us", &v, &ex), S_OK);
        CHECKF(v == exp_nobase(k, 1, &bf.m) && ex == 0, "%s: coordinate %d (want %d) exists %d", kname(k), v, exp_nobase(k, 1, &bf.m), ex);
    }
    bf_close(&bf); TA_END();
}
TEST(gb_default_kind_resolution) {
    TA_BEGIN(); NEED_BF((void)0); INT32 d, r, c; BOOL e;
    gb(a1, &bf, 0, FALSE, TRUE, bf.latin, NULL, &d, &e); gb(a1, &bf, 1, FALSE, TRUE, bf.latin, NULL, &r, &e); EXPECT_EQ(d, r);
    gb(a1, &bf, 0, TRUE, TRUE, bf.latin, NULL, &d, &e); gb(a1, &bf, 2, TRUE, TRUE, bf.latin, NULL, &c, &e); EXPECT_EQ(d, c);
    bf_close(&bf); TA_END();
}
TEST(gb_horizontal_roman_is_zero_and_signs) {
    TA_BEGIN(); NEED_BF((void)0); INT32 r, bot, top; BOOL e;
    gb(a1, &bf, DWRITE_BASELINE_ROMAN, FALSE, TRUE, bf.latin, NULL, &r, &e); EXPECT_EQ(r, 0);
    gb(a1, &bf, DWRITE_BASELINE_IDEOGRAPHIC_BOTTOM, FALSE, TRUE, bf.latin, NULL, &bot, &e); gb(a1, &bf, DWRITE_BASELINE_IDEOGRAPHIC_TOP, FALSE, TRUE, bf.latin, NULL, &top, &e);
    CHECK(bot > 0 && top < 0);         /* y grows downwards: the bottom is below the roman baseline, the top above */
    EXPECT_EQ(bot, (INT32)bf.m.descent); EXPECT_EQ(top, -(INT32)bf.m.ascent);
    bf_close(&bf); TA_END();
}
TEST(gb_locale_is_ignored_for_regular_kinds) {
    TA_BEGIN(); NEED_BF((void)0); static const WCHAR *locs[] = { NULL, L"", L"en-us", L"fr-FR", L"ja-jp", L"not a locale", L"zz" }; unsigned i; int k;
    for (k = 0; k <= 6; k++) {
        INT32 ref; BOOL e; gb(a1, &bf, k, FALSE, TRUE, bf.latin, NULL, &ref, &e);
        for (i = 0; i < sizeof locs / sizeof *locs; i++) { INT32 v; BOOL ex; CHECK_HR(gb(a1, &bf, k, FALSE, TRUE, bf.latin, locs[i], &v, &ex), S_OK); CHECKF(v == ref, "%s loc %u", kname(k), i); }
    }
    bf_close(&bf); TA_END();
}
TEST(gb_script_is_ignored_without_base_table) {
    TA_BEGIN(); NEED_BF((void)0); static const WCHAR *samples[] = { L"A", L"\x0416", L"\x0627", L"\x4E00", L"\x0E01", L"\x05D0" }; unsigned i; INT32 ref; BOOL e;
    gb(a1, &bf, DWRITE_BASELINE_CENTRAL, FALSE, TRUE, bf.latin, NULL, &ref, &e);
    for (i = 0; i < sizeof samples / sizeof *samples; i++) {
        INT32 v; BOOL ex; DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, samples[i], 1);
        CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_CENTRAL, FALSE, TRUE, sa, NULL, &v, &ex), S_OK); EXPECT_EQ(v, ref); EXPECT_EQ(ex, 0);
    }
    bf_close(&bf); TA_END();
}
TEST(gb_script_index_out_of_range_still_succeeds) {
    /* the script id is only used as a table index (out of range ids map to entry 0) */
    TA_BEGIN(); NEED_BF((void)0); DWRITE_SCRIPT_ANALYSIS sa; INT32 v; BOOL e; sa.script = 0xFFFF; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_ROMAN, FALSE, TRUE, sa, NULL, &v, &e), S_OK);
    bf_close(&bf); TA_END();
}
TEST(gb_metric_variation_horizontal) {
    TA_BEGIN(); NEED_BF({ s_.upm = 1000; s_.hhea_asc = 800; s_.hhea_desc = -200; s_.hhea_gap = 0; s_.typo_asc = 800; s_.typo_desc = -200; s_.typo_gap = 0; s_.win_asc = 800; s_.win_desc = 200; s_.cap_height = 700; s_.x_height = 500; });
    int k;
    for (k = 0; k <= 8; k++) { INT32 v; BOOL e; CHECK_HR(gb(a1, &bf, k, FALSE, TRUE, bf.latin, NULL, &v, &e), S_OK); CHECKF(v == exp_nobase(k, 0, &bf.m), "%s: %d want %d", kname(k), v, exp_nobase(k, 0, &bf.m)); }
    t_note("upm %u asc %u desc %u cap %u x %u", bf.m.designUnitsPerEm, bf.m.ascent, bf.m.descent, bf.m.capHeight, bf.m.xHeight);
    bf_close(&bf); TA_END();
}
TEST(gb_metric_variation_vertical) {
    TA_BEGIN(); NEED_BF({ s_.upm = 1000; s_.hhea_asc = 900; s_.hhea_desc = -300; s_.hhea_gap = 0; s_.typo_asc = 900; s_.typo_desc = -300; s_.typo_gap = 0; s_.win_asc = 900; s_.win_desc = 300; s_.cap_height = 650; s_.x_height = 450; });
    int k;
    for (k = 0; k <= 8; k++) { INT32 v; BOOL e; CHECK_HR(gb(a1, &bf, k, TRUE, TRUE, bf.latin, NULL, &v, &e), S_OK); CHECKF(v == exp_nobase(k, 1, &bf.m), "%s: %d want %d", kname(k), v, exp_nobase(k, 1, &bf.m)); }
    bf_close(&bf); TA_END();
}

/* ---- fonts with a BASE table ----------------------------------------------------------------------------------------------- */
#define BASE_R 40
#define BASE_I (-300)
#define SETUP_BASE { s_.has_base = 1; s_.base_ideo = BASE_I; s_.base_romn = BASE_R; }
TEST(gb_base_table_roman_and_ideographic_exist) {
    TA_BEGIN(); NEED_BF(SETUP_BASE); INT32 v; BOOL e; int sim;
    for (sim = 0; sim < 2; sim++) {
        CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_ROMAN, FALSE, sim, bf.latin, L"en-us", &v, &e), S_OK); EXPECT_EQ(v, -BASE_R); EXPECT_EQ(e, 1);
        CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_DEFAULT, FALSE, sim, bf.latin, L"en-us", &v, &e), S_OK); EXPECT_EQ(v, -BASE_R); EXPECT_EQ(e, 1);
        CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_IDEOGRAPHIC_BOTTOM, FALSE, sim, bf.latin, L"en-us", &v, &e), S_OK); EXPECT_EQ(v, -BASE_I); EXPECT_EQ(e, 1);
    }
    bf_close(&bf); TA_END();
}
TEST(gb_base_table_unlisted_kinds_do_not_exist) {
    TA_BEGIN(); NEED_BF(SETUP_BASE); static const int ks[] = { 2, 3, 4, 6, 7, 8 }; unsigned i;
    for (i = 0; i < sizeof ks / sizeof *ks; i++) {
        INT32 v = 77; BOOL e = 5;
        CHECK_HR(gb(a1, &bf, ks[i], FALSE, FALSE, bf.latin, L"en-us", &v, &e), S_OK); EXPECT_EQ(v, 0); EXPECT_EQ(e, 0);
        CHECK_HR(gb(a1, &bf, ks[i], FALSE, TRUE, bf.latin, L"en-us", &v, &e), S_OK); EXPECT_EQ(e, 0);
        CHECKF(v == exp_base(ks[i], &bf.m, BASE_R, BASE_I), "%s: %d want %d", kname(ks[i]), v, exp_base(ks[i], &bf.m, BASE_R, BASE_I));
    }
    bf_close(&bf); TA_END();
}
TEST(gb_base_table_simulated_values_use_table_data) {
    TA_BEGIN(); NEED_BF(SETUP_BASE); int k;
    for (k = 0; k <= 8; k++) { INT32 v; BOOL e; CHECK_HR(gb(a1, &bf, k, FALSE, TRUE, bf.latin, NULL, &v, &e), S_OK); CHECKF(v == exp_base(k, &bf.m, BASE_R, BASE_I), "%s: %d want %d", kname(k), v, exp_base(k, &bf.m, BASE_R, BASE_I)); }
    bf_close(&bf); TA_END();
}
TEST(gb_base_table_horizontal_axis_only) {
    /* the table has no vertical axis: vertical text falls back to the metric based values and nothing exists */
    TA_BEGIN(); NEED_BF(SETUP_BASE); int k;
    for (k = 0; k <= 8; k++) {
        INT32 v = 77; BOOL e = 5;
        CHECK_HR(gb(a1, &bf, k, TRUE, FALSE, bf.latin, NULL, &v, &e), S_OK); EXPECT_EQ(v, 0); EXPECT_EQ(e, 0);
        CHECK_HR(gb(a1, &bf, k, TRUE, TRUE, bf.latin, NULL, &v, &e), S_OK); EXPECT_EQ(e, 0);
        CHECKF(v == exp_nobase(k, 1, &bf.m), "%s: %d want %d", kname(k), v, exp_nobase(k, 1, &bf.m));
    }
    bf_close(&bf); TA_END();
}
TEST(gb_base_table_locale_ignored_for_latin) {
    TA_BEGIN(); NEED_BF(SETUP_BASE); static const WCHAR *locs[] = { NULL, L"", L"en-us", L"tr-tr", L"zzzz" }; unsigned i;
    for (i = 0; i < sizeof locs / sizeof *locs; i++) { INT32 v; BOOL e; CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_ROMAN, FALSE, FALSE, bf.latin, locs[i], &v, &e), S_OK); EXPECT_EQ(v, -BASE_R); EXPECT_EQ(e, 1); }
    bf_close(&bf); TA_END();
}
TEST(gb_base_table_other_scripts_note) {
    /* only 'latn' is listed: other scripts reach the data through the table's fallback rules (implementation specific) */
    TA_BEGIN(); NEED_BF(SETUP_BASE); static const WCHAR *samples[] = { L"\x0416", L"\x0391", L"\x0627", L"\x4E00", L"\x0E01", L"\x0905" }; unsigned i;
    for (i = 0; i < sizeof samples / sizeof *samples; i++) {
        INT32 v; BOOL e; DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, samples[i], 1);
        CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_ROMAN, FALSE, TRUE, sa, NULL, &v, &e), S_OK);
        t_note("U+%04X (script %u): ROMAN %d exists %d", samples[i][0], sa.script, v, e);
        if (v != -BASE_R) t_note("  does not reuse the latn roman value (%d)", -BASE_R);
    }
    bf_close(&bf); TA_END();
}
TEST(gb_base_table_positive_ideographic_value) {
    TA_BEGIN(); NEED_BF({ s_.has_base = 1; s_.base_ideo = 120; s_.base_romn = -30; }); INT32 v; BOOL e;
    CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_ROMAN, FALSE, FALSE, bf.latin, NULL, &v, &e), S_OK); EXPECT_EQ(v, 30); EXPECT_EQ(e, 1);
    CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_IDEOGRAPHIC_BOTTOM, FALSE, FALSE, bf.latin, NULL, &v, &e), S_OK); EXPECT_EQ(v, -120); EXPECT_EQ(e, 1);
    bf_close(&bf); TA_END();
}
TEST(gb_base_table_zero_values) {
    TA_BEGIN(); NEED_BF({ s_.has_base = 1; s_.base_ideo = 0; s_.base_romn = 0; }); INT32 v = 77; BOOL e = 5;
    CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_ROMAN, FALSE, FALSE, bf.latin, NULL, &v, &e), S_OK); EXPECT_EQ(v, 0); EXPECT_EQ(e, 1);
    CHECK_HR(gb(a1, &bf, DWRITE_BASELINE_IDEOGRAPHIC_BOTTOM, FALSE, FALSE, bf.latin, NULL, &v, &e), S_OK); EXPECT_EQ(v, 0); EXPECT_EQ(e, 1);
    bf_close(&bf); TA_END();
}
TEST(gb_through_analyzer2_interface) {
    TA_BEGIN(); NEED_BF(SETUP_BASE); INT32 v; BOOL e;
    CHECK_HR(IDWriteTextAnalyzer2_GetBaseline(a2, bf.h.face, DWRITE_BASELINE_ROMAN, FALSE, FALSE, bf.latin, NULL, &v, &e), S_OK); EXPECT_EQ(v, -BASE_R); EXPECT_EQ(e, 1);
    bf_close(&bf); TA_END();
}
TEST(gb_face_refcount_balanced) {
    TA_BEGIN(); NEED_BF((void)0); INT32 v; BOOL e; ULONG r0 = dw_refcount((IUnknown *)bf.h.face), r1;
    gb(a1, &bf, DWRITE_BASELINE_ROMAN, FALSE, TRUE, bf.latin, NULL, &v, &e);
    r1 = dw_refcount((IUnknown *)bf.h.face); EXPECT_EQ(r1, r0);
    bf_close(&bf); TA_END();
}

/* ---- system fonts: relations and notes ---------------------------------------------------------------------------------------- */
TEST(gb_arial_horizontal_relations) {
    TA_BEGIN(); IDWriteFontFace *arial = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!arial) { TA_END(); t_skip("Arial not installed"); return; }
    DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1); INT32 r = 77, bot, top; BOOL e;
    CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, arial, DWRITE_BASELINE_ROMAN, FALSE, TRUE, sa, L"en-us", &r, &e), S_OK);
    t_note("Arial ROMAN %d exists %d", r, e);
    CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, arial, DWRITE_BASELINE_IDEOGRAPHIC_BOTTOM, FALSE, TRUE, sa, L"en-us", &bot, &e), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, arial, DWRITE_BASELINE_IDEOGRAPHIC_TOP, FALSE, TRUE, sa, L"en-us", &top, &e), S_OK);
    t_note("Arial ideographic bottom %d top %d", bot, top);
    CHECK(top < bot);                   /* top is above (smaller y) than bottom */
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(gb_arial_vertical_default_is_central) {
    TA_BEGIN(); IDWriteFontFace *arial = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!arial) { TA_END(); t_skip("Arial not installed"); return; }
    DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1); INT32 d, c; BOOL e;
    CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, arial, DWRITE_BASELINE_DEFAULT, TRUE, TRUE, sa, NULL, &d, &e), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, arial, DWRITE_BASELINE_CENTRAL, TRUE, TRUE, sa, NULL, &c, &e), S_OK);
    EXPECT_EQ(d, c);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(gb_arial_matches_metric_formulas) {
    /* Arial has no BASE table on the supported builds; when it has one this test degrades to a note */
    TA_BEGIN(); IDWriteFontFace *arial = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!arial) { TA_END(); t_skip("Arial not installed"); return; }
    UINT32 len = 0; uint8_t *base = dw_face_table(arial, DW_TAG('B', 'A', 'S', 'E'), &len);
    IDWriteFontFace1 *f1 = NULL; DWRITE_FONT_METRICS1 m; DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1); int k, vert;
    if (base) { free(base); IDWriteFontFace_Release(arial); TA_END(); t_skip("Arial has a BASE table"); return; }
    if (FAILED(IDWriteFontFace_QueryInterface(arial, &IID_IDWriteFontFace1, (void **)&f1))) { IDWriteFontFace_Release(arial); TA_END(); t_skip("no IDWriteFontFace1"); return; }
    IDWriteFontFace1_GetMetrics(f1, &m);
    for (vert = 0; vert < 2; vert++) for (k = 0; k <= 8; k++) {
        INT32 v; BOOL e; CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(a1, arial, (DWRITE_BASELINE)k, vert, TRUE, sa, NULL, &v, &e), S_OK);
        CHECKF(v == exp_nobase(k, vert, &m) && !e, "%s vert %d: %d want %d", kname(k), vert, v, exp_nobase(k, vert, &m));
    }
    IDWriteFontFace1_Release(f1); IDWriteFontFace_Release(arial); TA_END();
}
