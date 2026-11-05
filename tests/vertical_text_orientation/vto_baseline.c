/* IDWriteTextAnalyzer1::GetBaseline for fonts without a BASE table (synthetic fonts), and with one.
 *
 * DWriteTextAnalyzer::GetBaseline zeroes both outputs, rejects a NULL font face or a baseline above DWRITE_BASELINE_MAXIMUM (8) with E_INVALIDARG and
 * otherwise runs BaseTable::GetBaseline: DEFAULT means ROMAN for horizontal text and CENTRAL for vertical text; MINIMUM and MAXIMUM ignore the locale.
 * Without a BASE entry the coordinate is only produced when simulation is allowed, from the font metrics (ascent A, descent D, capHeight C, xHeight X):
 *   horizontal (returned value = - coordinate):  ideographic top = A, ideographic bottom = -D, central = math = (A - D) >> 1, roman = 0,
 *                                                hanging = ((X + C) >> 1) + roman, maximum = A, minimum = -D
 *   vertical   (returned value = + coordinate):  ideographic top = A + D, ideographic bottom = 0, central = math = (A + D) >> 1,
 *                                                roman = ((D - A + 1) >> 1) + central, hanging = ((X + C) >> 1) + roman, maximum = A + D, minimum = 0
 * and *exists stays FALSE. With simulation disallowed and no BASE entry the result is 0 / FALSE.
 */
#include "vto.h"

typedef struct bl { IDWriteTextAnalyzer1 *a1; IDWriteFontFace *face; IDWriteFontFace1 *f1; DWRITE_FONT_METRICS1 m; } bl;
static int bl_open(vt_fx *x, bl *b, const WCHAR *fam)
{
    IDWriteTextAnalyzer *a = NULL;
    memset(b, 0, sizeof *b);
    b->face = vt_face(x, fam);
    if (!b->face || FAILED(IDWriteFontFace_QueryInterface(b->face, &IID_IDWriteFontFace1, (void **)&b->f1))) return 0;
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(x->f, &a))) return 0;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&b->a1))) { DW_RELEASE(a); return 0; }
    DW_RELEASE(a);
    IDWriteFontFace1_GetMetrics(b->f1, &b->m);
    return 1;
}
static void bl_close(bl *b) { DW_RELEASE(b->a1); DW_RELEASE(b->f1); DW_RELEASE(b->face); }
#define BL_BEGIN(fam) VT_BEGIN(); bl B; if (!bl_open(&X, &B, fam)) { bl_close(&B); VT_END(); t_skip("analyzer1 / fontface1 unavailable"); return; }
#define BL_END() do { bl_close(&B); VT_END(); } while (0)

static int expected(const DWRITE_FONT_METRICS1 *m, int baseline, int vertical)
{
    int A = m->ascent, D = m->descent, C = m->capHeight, X = m->xHeight, v;
    int top, bot, central, roman, hang;
    if (baseline == 0) baseline = vertical ? 2 : 1;
    if (vertical) { top = A + D; bot = 0; central = (bot + top) >> 1; roman = ((D - A + 1) >> 1) + central; }
    else { top = A; bot = -D; central = (bot + top) >> 1; roman = 0; }
    hang = ((X + C) >> 1) + roman;
    switch (baseline) {
    case 1: v = roman; break;
    case 2: v = central; break;
    case 3: v = central; break;
    case 4: v = hang; break;
    case 5: v = bot; break;
    case 6: v = top; break;
    case 7: v = bot; break;
    default: v = top; break;
    }
    return vertical ? v : -v;
}
static HRESULT getbl(bl *b, int baseline, BOOL vertical, BOOL sim, int *coord, BOOL *exists)
{
    DWRITE_SCRIPT_ANALYSIS sa; INT32 c = 0x7777; BOOL e = 0x55; HRESULT hr;
    sa.script = 0; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    hr = IDWriteTextAnalyzer1_GetBaseline(b->a1, b->face, (DWRITE_BASELINE)baseline, vertical, sim, sa, L"en-us", &c, &e);
    *coord = c; *exists = e;
    return hr;
}

TEST(metrics_used_by_the_baseline_formulas)
{
    BL_BEGIN(VT_FAM);
    EXPECT_EQ(B.m.designUnitsPerEm, 2048);
    t_note("font metrics: ascent %u descent %u cap %u xHeight %u", B.m.ascent, B.m.descent, B.m.capHeight, B.m.xHeight);
    CHECK(B.m.ascent > 0 && B.m.descent > 0);
    BL_END();
}
TEST(horizontal_baselines_from_font_metrics)
{
    BL_BEGIN(VT_FAM); int b;
    for (b = 0; b <= 8; b++) {
        int c; BOOL e; HRESULT hr = getbl(&B, b, FALSE, TRUE, &c, &e);
        CHECK_HR_OK(hr);
        CHECKF(c == expected(&B.m, b, 0), "horizontal baseline %d: got %d want %d", b, c, expected(&B.m, b, 0));
        CHECKF(e == FALSE, "baseline %d exists=%d", b, e);
    }
    BL_END();
}
TEST(vertical_baselines_from_font_metrics)
{
    BL_BEGIN(VT_FAM); int b;
    for (b = 0; b <= 8; b++) {
        int c; BOOL e; HRESULT hr = getbl(&B, b, TRUE, TRUE, &c, &e);
        CHECK_HR_OK(hr);
        CHECKF(c == expected(&B.m, b, 1), "vertical baseline %d: got %d want %d", b, c, expected(&B.m, b, 1));
        CHECKF(e == FALSE, "baseline %d exists=%d", b, e);
    }
    BL_END();
}
TEST(plain_font_gives_the_same_formulas)
{
    BL_BEGIN(VT_PLAIN); int b, v;
    for (v = 0; v < 2; v++) for (b = 0; b <= 8; b++) { int c; BOOL e; CHECK_HR_OK(getbl(&B, b, v, TRUE, &c, &e)); CHECKF(c == expected(&B.m, b, v), "vertical %d baseline %d: got %d want %d", v, b, c, expected(&B.m, b, v)); }
    BL_END();
}
TEST(default_baseline_horizontal_is_roman)
{
    BL_BEGIN(VT_FAM); int d, r; BOOL e;
    CHECK_HR_OK(getbl(&B, DWRITE_BASELINE_DEFAULT, FALSE, TRUE, &d, &e)); CHECK_HR_OK(getbl(&B, DWRITE_BASELINE_ROMAN, FALSE, TRUE, &r, &e));
    EXPECT_EQ(d, r); EXPECT_EQ(d, 0);
    BL_END();
}
TEST(default_baseline_vertical_is_central)
{
    BL_BEGIN(VT_FAM); int d, c; BOOL e;
    CHECK_HR_OK(getbl(&B, DWRITE_BASELINE_DEFAULT, TRUE, TRUE, &d, &e)); CHECK_HR_OK(getbl(&B, DWRITE_BASELINE_CENTRAL, TRUE, TRUE, &c, &e));
    EXPECT_EQ(d, c);
    BL_END();
}
TEST(simulation_disallowed_without_base_table_gives_zero)
{
    BL_BEGIN(VT_FAM); int b, v;
    for (v = 0; v < 2; v++) for (b = 0; b <= 8; b++) {
        int c; BOOL e; CHECK_HR_OK(getbl(&B, b, v, FALSE, &c, &e));
        CHECKF(c == 0 && e == FALSE, "vertical %d baseline %d: coordinate %d exists %d", v, b, c, e);
    }
    BL_END();
}
TEST(central_is_between_roman_and_ideographic_top_vertical)
{
    BL_BEGIN(VT_FAM); int roman, central, top, bot; BOOL e;
    CHECK_HR_OK(getbl(&B, 1, TRUE, TRUE, &roman, &e)); CHECK_HR_OK(getbl(&B, 2, TRUE, TRUE, &central, &e));
    CHECK_HR_OK(getbl(&B, 6, TRUE, TRUE, &top, &e)); CHECK_HR_OK(getbl(&B, 5, TRUE, TRUE, &bot, &e));
    CHECK(bot <= central && central <= top); CHECK(bot <= roman && roman <= top);
    BL_END();
}
TEST(minimum_and_maximum_bracket_all_baselines)
{
    BL_BEGIN(VT_FAM); int v, b, lo, hi; BOOL e;
    for (v = 0; v < 2; v++) {
        CHECK_HR_OK(getbl(&B, 7, v, TRUE, &lo, &e)); CHECK_HR_OK(getbl(&B, 8, v, TRUE, &hi, &e));
        for (b = 1; b <= 6; b++) { int c; CHECK_HR_OK(getbl(&B, b, v, TRUE, &c, &e)); if (v) CHECKF(c >= lo && c <= hi, "vertical baseline %d = %d outside [%d,%d]", b, c, lo, hi); else CHECKF(c <= lo && c >= hi, "horizontal baseline %d = %d outside [%d,%d]", b, c, hi, lo); }
    }
    BL_END();
}
TEST(horizontal_sign_convention_is_negated_vertical)
{
    /* horizontal coordinates are measured up from the roman baseline and returned negated; vertical ones are returned as stored. */
    BL_BEGIN(VT_FAM); int top_h, top_v; BOOL e;
    CHECK_HR_OK(getbl(&B, 6, FALSE, TRUE, &top_h, &e)); CHECK_HR_OK(getbl(&B, 6, TRUE, TRUE, &top_v, &e));
    EXPECT_EQ(top_h, -(int)B.m.ascent); EXPECT_EQ(top_v, (int)B.m.ascent + (int)B.m.descent);
    BL_END();
}
TEST(invalid_baseline_value_is_invalid_and_outputs_zeroed)
{
    BL_BEGIN(VT_FAM); int bad[] = { 9, 10, 255, 1000, 0x7fffffff }; int i;
    for (i = 0; i < 5; i++) { int c; BOOL e; HRESULT hr = getbl(&B, bad[i], FALSE, TRUE, &c, &e); CHECKF(hr == VT_E_INVALIDARG, "baseline %d -> %s", bad[i], dw_hr_name(hr)); CHECK(c == 0 && e == FALSE); }
    BL_END();
}
TEST(null_font_face_invalid_and_outputs_zeroed)
{
    BL_BEGIN(VT_FAM); DWRITE_SCRIPT_ANALYSIS sa; INT32 c = 5; BOOL e = 5; sa.script = 0; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    CHECK_HR(IDWriteTextAnalyzer1_GetBaseline(B.a1, NULL, DWRITE_BASELINE_ROMAN, FALSE, TRUE, sa, L"en-us", &c, &e), VT_E_INVALIDARG);
    CHECK(c == 0 && e == FALSE);
    BL_END();
}
TEST(null_locale_is_accepted)
{
    BL_BEGIN(VT_FAM); DWRITE_SCRIPT_ANALYSIS sa; INT32 c = 5; BOOL e = 5; sa.script = 0; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    CHECK_HR_OK(IDWriteTextAnalyzer1_GetBaseline(B.a1, B.face, DWRITE_BASELINE_CENTRAL, TRUE, TRUE, sa, NULL, &c, &e));
    EXPECT_EQ(c, expected(&B.m, 2, 1));
    BL_END();
}
TEST(script_and_locale_do_not_change_simulated_values)
{
    BL_BEGIN(VT_FAM); const WCHAR *loc[] = { L"en-us", L"ja-jp", L"zh-cn", L"", L"x-unknown" }; int i, base; BOOL e; DWRITE_SCRIPT_ANALYSIS sa;
    sa.script = 0; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    for (i = 0; i < 5; i++) {
        INT32 c = 0; int k;
        for (k = 0; k < 3; k++) {
            sa.script = (UINT16)(k * 5);
            base = k % 2 ? 2 : 1;
            CHECK_HR_OK(IDWriteTextAnalyzer1_GetBaseline(B.a1, B.face, (DWRITE_BASELINE)base, TRUE, TRUE, sa, loc[i], &c, &e));
            CHECKF(c == expected(&B.m, base, 1), "locale %ls script %u baseline %d: %d", loc[i], sa.script, base, c);
        }
    }
    BL_END();
}
TEST(repeated_calls_are_stable)
{
    BL_BEGIN(VT_FAM); int a, b, i; BOOL e;
    CHECK_HR_OK(getbl(&B, 2, TRUE, TRUE, &a, &e));
    for (i = 0; i < 20; i++) { CHECK_HR_OK(getbl(&B, 2, TRUE, TRUE, &b, &e)); EXPECT_EQ(a, b); }
    BL_END();
}
TEST(base_table_font_reports_existing_baselines_noted)
{
    /* A font with a BASE table (horizontal axis, latn script, ideo + romn coordinates). Which DWRITE_BASELINE a BASE tag lands in is recorded, not asserted. */
    VT_BEGIN(); ttf_spec s; size_t sz; uint8_t *blob; dwx_font h; IDWriteFontFace1 *f1 = NULL; IDWriteTextAnalyzer *a = NULL; IDWriteTextAnalyzer1 *a1 = NULL; int b;
    ttf_default_spec(&s); ttf_set_names(&s, "DwBase", "Regular", "DwBase", "DwBase-Regular");
    s.has_base = 1; s.base_ideo = -300; s.base_romn = 0;
    blob = ttf_build(&s, &sz);
    if (!blob) { VT_END(); t_skip("font build"); return; }
    memset(&h, 0, sizeof h);
    if (FAILED(dwx_font_open(&h, f, blob, sz, 0, DWRITE_FONT_SIMULATIONS_NONE)) || !h.face) { dwx_font_close(&h); free(blob); VT_END(); t_skip("font open"); return; }
    IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace1, (void **)&f1);
    IDWriteFactory_CreateTextAnalyzer(f, &a);
    if (a) IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&a1);
    if (a1) {
        DWRITE_SCRIPT_ANALYSIS sa; sa.script = 0; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
        for (b = 0; b <= 8; b++) {
            INT32 c = 0; BOOL e = 0; HRESULT hr = IDWriteTextAnalyzer1_GetBaseline(a1, h.face, (DWRITE_BASELINE)b, FALSE, FALSE, sa, L"en-us", &c, &e);
            CHECK_HR_OK(hr);
            t_note("BASE font baseline %d (no simulation): coordinate %d exists %d", b, c, e);
            if (!e) CHECK(c == 0);
        }
    }
    DW_RELEASE(a1); DW_RELEASE(a); DW_RELEASE(f1); dwx_font_close(&h); free(blob); VT_END();
}
