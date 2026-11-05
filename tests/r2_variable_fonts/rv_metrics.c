/* Variation effects on the synthetic fixture (rv.h): HVAR advance deltas, avar remapping, gvar outline deltas, layout widths.
 *
 * What is hard-checked here is what holds whether or not dwrite applies a given variation table: the default instance, glyphs without deltas, clamping
 * equivalences (a face made with wght 5000 behaves exactly like one made with 900), metrics that no table varies.  The exact variation results
 * (HVAR deltas, avar remapping, gvar movement) follow the OpenType rules and are hard-checked WHEN a change is observed, with a t_note saying whether
 * the table took effect (a face that did not change at all is reported, not failed).
 */
#include "rv.h"

static void effect(const char *what, int got, int base, int want)
{
    if (got == base && got != want) { t_note("%s: not applied (advance %d)", what, got); return; }
    CHECKF(got == want, "%s: advance %d, expected %d (default %d)", what, got, want, base);
    if (got != base) t_note("%s: applied (%d -> %d)", what, base, got);
}
static int adv_at(rv *x, UINT32 cp, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n)
{
    IDWriteFontFace5 *f = rv_face(x, v, n); int a = -1;
    if (f) { a = rv_adv((IDWriteFontFace *)f, rv_gid((IDWriteFontFace *)f, cp)); DW_RELEASE(f); }
    return a;
}
#define W(v) rv_av(TAG_WGHT, (v))

/* ---- default instance and untouched glyphs ---------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_metrics_default_advances)
{
    RV_BEGIN(RVF_HVAR);
    EXPECT_EQ(adv_at(&x, 'A', NULL, 0), 1000); EXPECT_EQ(adv_at(&x, 'B', NULL, 0), 1000); EXPECT_EQ(adv_at(&x, 'C', NULL, 0), 1000); EXPECT_EQ(adv_at(&x, ' ', NULL, 0), 1000);
    RV_END();
}
TEST(r2_variable_fonts_metrics_explicit_default_equals_omitted)
{
    DWRITE_FONT_AXIS_VALUE a = W(400), b = rv_av(TAG_WDTH, 100), c = rv_av(TAG_OPSZ, 14);
    RV_BEGIN(RVF_HVAR);
    EXPECT_EQ(adv_at(&x, 'A', &a, 1), adv_at(&x, 'A', NULL, 0));
    EXPECT_EQ(adv_at(&x, 'A', &b, 1), adv_at(&x, 'A', NULL, 0));
    EXPECT_EQ(adv_at(&x, 'C', &c, 1), adv_at(&x, 'C', NULL, 0));
    RV_END();
}
TEST(r2_variable_fonts_metrics_glyph_without_deltas_never_moves)
{
    static const float w[] = { 100, 300, 650, 900 }; unsigned i;
    RV_BEGIN(RVF_HVAR | RVF_AVAR);
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        DWRITE_FONT_AXIS_VALUE a[3]; a[0] = W(w[i]); a[1] = rv_av(TAG_WDTH, 125); a[2] = rv_av(TAG_OPSZ, 144);
        EXPECT_EQ(adv_at(&x, 'D', a, 3), 1000); EXPECT_EQ(adv_at(&x, 'Z', a, 3), 1000); EXPECT_EQ(adv_at(&x, '0', a, 3), 1000);
    }
    RV_END();
}
TEST(r2_variable_fonts_metrics_hidden_and_fixed_axes_have_no_effect)
{
    DWRITE_FONT_AXIS_VALUE a[2]; int base;
    RV_BEGIN(RVF_HVAR);
    base = adv_at(&x, 'A', NULL, 0);
    a[0] = rv_av(TAG_HIDN, 10); a[1] = rv_av(TAG_FIXD, 5);
    EXPECT_EQ(adv_at(&x, 'A', a, 2), base);
    RV_END();
}

/* ---- clamping equivalence (holds with or without HVAR) --------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_metrics_clamped_values_behave_like_the_bounds)
{
    RV_BEGIN(RVF_HVAR);
    { DWRITE_FONT_AXIS_VALUE hi = W(5000), mx = W(900); EXPECT_EQ(adv_at(&x, 'A', &hi, 1), adv_at(&x, 'A', &mx, 1)); }
    { DWRITE_FONT_AXIS_VALUE lo = W(-5), mn = W(100); EXPECT_EQ(adv_at(&x, 'A', &lo, 1), adv_at(&x, 'A', &mn, 1)); }
    { DWRITE_FONT_AXIS_VALUE hi = rv_av(TAG_WDTH, 1000), mx = rv_av(TAG_WDTH, 125); EXPECT_EQ(adv_at(&x, 'A', &hi, 1), adv_at(&x, 'A', &mx, 1)); }
    { DWRITE_FONT_AXIS_VALUE hi = rv_av(TAG_OPSZ, 5000), mx = rv_av(TAG_OPSZ, 144); EXPECT_EQ(adv_at(&x, 'C', &hi, 1), adv_at(&x, 'C', &mx, 1)); }
    { DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_OPSZ, 0), mn = rv_av(TAG_OPSZ, 8); EXPECT_EQ(adv_at(&x, 'C', &lo, 1), adv_at(&x, 'C', &mn, 1)); }
    RV_END();
}
TEST(r2_variable_fonts_metrics_first_duplicate_decides_advance)
{
    DWRITE_FONT_AXIS_VALUE d[2], s = W(900);
    RV_BEGIN(RVF_HVAR);
    d[0] = W(900); d[1] = W(100);
    EXPECT_EQ(adv_at(&x, 'A', d, 2), adv_at(&x, 'A', &s, 1));
    RV_END();
}
TEST(r2_variable_fonts_metrics_unknown_axis_has_no_effect)
{
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_ZZZZ, 77);
    RV_BEGIN(RVF_HVAR);
    EXPECT_EQ(adv_at(&x, 'A', &a, 1), adv_at(&x, 'A', NULL, 0));
    RV_END();
}
TEST(r2_variable_fonts_metrics_advance_non_decreasing_in_weight)
{
    int step, prev = -1;
    RV_BEGIN(RVF_HVAR);
    for (step = 0; step <= 8; step++) { DWRITE_FONT_AXIS_VALUE a = W(400.0f + 500.0f * (float)step / 8.0f); int adv = adv_at(&x, 'A', &a, 1); CHECKF(adv >= prev, "step %d: %d below %d", step, adv, prev); prev = adv; }
    RV_END();
}
TEST(r2_variable_fonts_metrics_advance_non_increasing_toward_light)
{
    int step, prev = 100000;
    RV_BEGIN(RVF_HVAR);
    for (step = 0; step <= 6; step++) { DWRITE_FONT_AXIS_VALUE a = W(400.0f - 300.0f * (float)step / 6.0f); int adv = adv_at(&x, 'A', &a, 1); CHECKF(adv <= prev, "step %d: %d above %d", step, adv, prev); prev = adv; }
    RV_END();
}

/* ---- HVAR -------------------------------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_metrics_hvar_weight_max)
{
    DWRITE_FONT_AXIS_VALUE a = W(900);
    RV_BEGIN(RVF_HVAR);
    effect("A at wght 900", adv_at(&x, 'A', &a, 1), 1000, 1100);
    effect("B at wght 900", adv_at(&x, 'B', &a, 1), 1000, 1200);
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_weight_min)
{
    DWRITE_FONT_AXIS_VALUE a = W(100);
    RV_BEGIN(RVF_HVAR);
    effect("A at wght 100", adv_at(&x, 'A', &a, 1), 1000, 950);
    effect("B at wght 100", adv_at(&x, 'B', &a, 1), 1000, 900);
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_weight_midpoint_without_avar)
{
    DWRITE_FONT_AXIS_VALUE a = W(650);          /* (650 - 400) / (900 - 400) = 0.5 */
    RV_BEGIN(RVF_HVAR);
    effect("A at wght 650", adv_at(&x, 'A', &a, 1), 1000, 1050);
    effect("B at wght 650", adv_at(&x, 'B', &a, 1), 1000, 1100);
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_weight_lower_midpoint)
{
    DWRITE_FONT_AXIS_VALUE a = W(250);          /* (250 - 400) / (400 - 100) = -0.5 */
    RV_BEGIN(RVF_HVAR);
    effect("A at wght 250", adv_at(&x, 'A', &a, 1), 1000, 975);
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_width_max)
{
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WDTH, 125);
    RV_BEGIN(RVF_HVAR);
    effect("A at wdth 125", adv_at(&x, 'A', &a, 1), 1000, 1060);
    EXPECT_EQ(adv_at(&x, 'B', &a, 1), 1000);       /* B has no width delta */
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_width_min_has_no_region)
{
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WDTH, 75);
    RV_BEGIN(RVF_HVAR);
    EXPECT_EQ(adv_at(&x, 'A', &a, 1), 1000);        /* only a positive region exists for wdth */
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_width_quarter)
{
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WDTH, 106.25f);       /* 6.25 / 25 = 0.25 -> +15 */
    RV_BEGIN(RVF_HVAR);
    effect("A at wdth 106.25", adv_at(&x, 'A', &a, 1), 1000, 1015);
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_two_axes_add)
{
    DWRITE_FONT_AXIS_VALUE a[2]; a[0] = W(900); a[1] = rv_av(TAG_WDTH, 125);
    RV_BEGIN(RVF_HVAR);
    effect("A at wght 900 wdth 125", adv_at(&x, 'A', a, 2), 1000, 1160);
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_optical_size)
{
    DWRITE_FONT_AXIS_VALUE hi = rv_av(TAG_OPSZ, 144), lo = rv_av(TAG_OPSZ, 8), mid = rv_av(TAG_OPSZ, 11);     /* mid: (11 - 14) / 6 = -0.5 -> -15 */
    RV_BEGIN(RVF_HVAR);
    effect("C at opsz 144", adv_at(&x, 'C', &hi, 1), 1000, 1040);
    effect("C at opsz 8", adv_at(&x, 'C', &lo, 1), 1000, 970);
    effect("C at opsz 11", adv_at(&x, 'C', &mid, 1), 1000, 985);
    RV_END();
}
TEST(r2_variable_fonts_metrics_hvar_applied_note)
{
    DWRITE_FONT_AXIS_VALUE a = W(900);
    RV_BEGIN(RVF_HVAR);
    t_note("HVAR in a synthetic font %s", adv_at(&x, 'A', &a, 1) != 1000 ? "is applied by dwrite" : "is not applied by dwrite");
    RV_END();
}
TEST(r2_variable_fonts_metrics_without_hvar_advances_do_not_vary)
{
    DWRITE_FONT_AXIS_VALUE a = W(900);
    RV_BEGIN(0);          /* fvar + gvar, no HVAR: gvar phantom points carry no delta here, so nothing changes */
    EXPECT_EQ(adv_at(&x, 'A', &a, 1), 1000); EXPECT_EQ(adv_at(&x, 'B', &a, 1), 1000);
    RV_END();
}

/* ---- avar -------------------------------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_metrics_avar_remaps_midpoint)
{
    DWRITE_FONT_AXIS_VALUE a = W(650);          /* normalised 0.5, avar maps it to 0.25 -> +25 */
    RV_BEGIN(RVF_HVAR | RVF_AVAR);
    effect("A at wght 650 with avar", adv_at(&x, 'A', &a, 1), 1000, 1025);
    RV_END();
}
TEST(r2_variable_fonts_metrics_avar_endpoints_unchanged)
{
    DWRITE_FONT_AXIS_VALUE hi = W(900), lo = W(100);
    RV_BEGIN(RVF_HVAR | RVF_AVAR);
    effect("A at wght 900 with avar", adv_at(&x, 'A', &hi, 1), 1000, 1100);
    effect("A at wght 100 with avar", adv_at(&x, 'A', &lo, 1), 1000, 950);
    RV_END();
}
TEST(r2_variable_fonts_metrics_avar_default_unchanged)
{
    DWRITE_FONT_AXIS_VALUE a = W(400);
    RV_BEGIN(RVF_HVAR | RVF_AVAR);
    EXPECT_EQ(adv_at(&x, 'A', &a, 1), 1000);
    RV_END();
}
TEST(r2_variable_fonts_metrics_avar_does_not_change_axis_api)
{
    DWRITE_FONT_AXIS_VALUE a = W(650); IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE v[16]; UINT32 n; int k;
    RV_BEGIN(RVF_HVAR | RVF_AVAR);
    f = rv_face(&x, &a, 1);
    if (f) { n = IDWriteFontFace5_GetFontAxisValueCount(f); if (n && n <= 16) { CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, v, n)); k = rv_find_value(v, n, TAG_WGHT); CHECK(k >= 0 && v[k].value == 650.0f); } DW_RELEASE(f); }       /* the API reports user-space values, not normalised ones */
    RV_END();
}

/* ---- gvar -------------------------------------------------------------------------------------------------------------------------------------------- */
static int bounds_of(rv *x, UINT32 cp, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, float *x0, float *x1)
{
    IDWriteFontFace5 *f = rv_face(x, v, n); int r = -1;
    if (f) { r = rv_outline_x((IDWriteFontFace *)f, rv_gid((IDWriteFontFace *)f, cp), x0, x1); DW_RELEASE(f); }
    return r;
}
TEST(r2_variable_fonts_metrics_default_outline_bounds)
{
    float x0 = -1, x1 = -1;
    RV_BEGIN(0);
    CHECK(bounds_of(&x, 'A', NULL, 0, &x0, &x1) == 0);
    CHECK(x0 == 100.0f && x1 == 900.0f);        /* box (100,0)-(900,1400) in design units */
    CHECK(bounds_of(&x, 'B', NULL, 0, &x0, &x1) == 0);
    CHECK(x0 == 100.0f && x1 == 900.0f);
    RV_END();
}
TEST(r2_variable_fonts_metrics_gvar_moves_a_at_weight_max)
{
    float x0 = -1, x1 = -1; DWRITE_FONT_AXIS_VALUE a = W(900);
    RV_BEGIN(0);
    CHECK(bounds_of(&x, 'A', &a, 1, &x0, &x1) == 0);
    if (x0 == 100.0f && x1 == 900.0f) t_note("gvar in a synthetic font is not applied to outlines");
    else CHECKF(x0 == 200.0f && x1 == 1000.0f, "outline x range [%g, %g], expected [200, 1000]", x0, x1);
    RV_END();
}
TEST(r2_variable_fonts_metrics_gvar_midpoint_scales)
{
    float x0 = -1, x1 = -1; DWRITE_FONT_AXIS_VALUE a = W(650);       /* scalar 0.5 -> +50 (no avar in this flavour) */
    RV_BEGIN(0);
    CHECK(bounds_of(&x, 'A', &a, 1, &x0, &x1) == 0);
    if (!(x0 == 100.0f && x1 == 900.0f)) CHECKF(x0 == 150.0f && x1 == 950.0f, "outline x range [%g, %g], expected [150, 950]", x0, x1);
    RV_END();
}
TEST(r2_variable_fonts_metrics_gvar_with_avar_midpoint)
{
    float x0 = -1, x1 = -1; DWRITE_FONT_AXIS_VALUE a = W(650);       /* avar 0.5 -> 0.25 -> +25 */
    RV_BEGIN(RVF_AVAR);
    CHECK(bounds_of(&x, 'A', &a, 1, &x0, &x1) == 0);
    if (!(x0 == 100.0f && x1 == 900.0f)) CHECKF(x0 == 125.0f && x1 == 925.0f, "outline x range [%g, %g], expected [125, 925]", x0, x1);
    RV_END();
}
TEST(r2_variable_fonts_metrics_gvar_lighter_side_unchanged)
{
    float x0 = -1, x1 = -1; DWRITE_FONT_AXIS_VALUE a = W(100);       /* the tuple's region only covers wght > default */
    RV_BEGIN(0);
    CHECK(bounds_of(&x, 'A', &a, 1, &x0, &x1) == 0);
    CHECK(x0 == 100.0f && x1 == 900.0f);
    RV_END();
}
TEST(r2_variable_fonts_metrics_gvar_other_axes_do_not_move_a)
{
    float x0 = -1, x1 = -1; DWRITE_FONT_AXIS_VALUE a[2]; a[0] = rv_av(TAG_WDTH, 125); a[1] = rv_av(TAG_OPSZ, 144);
    RV_BEGIN(0);
    CHECK(bounds_of(&x, 'A', a, 2, &x0, &x1) == 0);
    CHECK(x0 == 100.0f && x1 == 900.0f);
    RV_END();
}
TEST(r2_variable_fonts_metrics_gvar_other_glyphs_do_not_move)
{
    float x0 = -1, x1 = -1; DWRITE_FONT_AXIS_VALUE a = W(900);
    RV_BEGIN(0);
    CHECK(bounds_of(&x, 'B', &a, 1, &x0, &x1) == 0); CHECK(x0 == 100.0f && x1 == 900.0f);
    CHECK(bounds_of(&x, 'Z', &a, 1, &x0, &x1) == 0); CHECK(x0 == 100.0f && x1 == 900.0f);
    RV_END();
}
TEST(r2_variable_fonts_metrics_gvar_clamped_value_matches_bound)
{
    float a0 = -1, a1 = -1, b0 = -1, b1 = -1; DWRITE_FONT_AXIS_VALUE hi = W(5000), mx = W(900);
    RV_BEGIN(0);
    CHECK(bounds_of(&x, 'A', &hi, 1, &a0, &a1) == 0); CHECK(bounds_of(&x, 'A', &mx, 1, &b0, &b1) == 0);
    CHECK(a0 == b0 && a1 == b1);
    RV_END();
}

/* ---- metrics no table varies ------------------------------------------------------------------------------------------------------------------------ */
TEST(r2_variable_fonts_metrics_face_metrics_do_not_vary_without_mvar)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE v = W(900); DWRITE_FONT_METRICS m1, m2;
    RV_BEGIN(RVF_HVAR | RVF_AVAR);
    a = rv_face(&x, NULL, 0); b = rv_face(&x, &v, 1);
    if (a && b) {
        IDWriteFontFace_GetMetrics((IDWriteFontFace *)a, &m1); IDWriteFontFace_GetMetrics((IDWriteFontFace *)b, &m2);
        CHECK(!memcmp(&m1, &m2, sizeof m1));
        EXPECT_EQ(m1.designUnitsPerEm, 2048);
        EXPECT_EQ(m1.capHeight, 1400); EXPECT_EQ(m1.xHeight, 1000);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_metrics_glyph_count_and_indices_do_not_vary)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE v = W(900);
    RV_BEGIN(RVF_HVAR);
    a = rv_face(&x, NULL, 0); b = rv_face(&x, &v, 1);
    if (a && b) {
        UINT32 cp[3] = { 'A', 'a', '0' }; UINT16 g1[3], g2[3];
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount((IDWriteFontFace *)a), TTF_DEF_NGLYPHS); EXPECT_EQ(IDWriteFontFace_GetGlyphCount((IDWriteFontFace *)b), TTF_DEF_NGLYPHS);
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices((IDWriteFontFace *)a, cp, 3, g1)); CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices((IDWriteFontFace *)b, cp, 3, g2)); CHECK(!memcmp(g1, g2, sizeof g1));
        EXPECT_EQ(g1[0], 2);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_metrics_design_metrics_of_untouched_glyph)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE v = W(900); DWRITE_GLYPH_METRICS m; UINT16 g;
    RV_BEGIN(RVF_HVAR);
    f = rv_face(&x, &v, 1);
    if (f) {
        g = rv_gid((IDWriteFontFace *)f, 'D');
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics((IDWriteFontFace *)f, &g, 1, &m, FALSE));
        EXPECT_EQ(m.advanceWidth, 1000); EXPECT_EQ(m.leftSideBearing, 100); EXPECT_EQ(m.rightSideBearing, 100);       /* box 100..900 in an advance of 1000 */
        DW_RELEASE(f);
    }
    RV_END();
}

/* ---- layout widths through a private collection ------------------------------------------------------------------------------------------------------ */
static float lay_width(rc *c, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, DWRITE_AUTOMATIC_FONT_AXES au, const WCHAR *text, float size, int *ok)
{
    IDWriteTextFormat3 *t = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m; float w = -1.0f; *ok = 0;
    HRESULT hr = IDWriteFactory6_CreateTextFormat(c->f6, RV_FAM, c->coll, v, n, size, L"en-us", &t);
    CHECK_HR(hr, S_OK);
    if (t) {
        CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t, au));
        if (SUCCEEDED(IDWriteFactory6_CreateTextLayout(c->f6, text, (UINT32)wcslen(text), (IDWriteTextFormat *)t, 10000.0f, 1000.0f, &l)) && l) {
            if (SUCCEEDED(IDWriteTextLayout_GetMetrics(l, &m))) { w = m.widthIncludingTrailingWhitespace; *ok = 1; }
            DW_RELEASE(l);
        }
        DW_RELEASE(t);
    }
    return w;
}
TEST(r2_variable_fonts_metrics_layout_default_width)
{
    int ok; float w;
    RC_BEGIN(RVF_HVAR);
    w = lay_width(&c, NULL, 0, 0, L"AAAA", 64.0f, &ok);
    CHECK(ok); CHECKF(w == 125.0f, "width %g, expected 4 * 1000 / 2048 * 64 = 125", w);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_default_width_explicit_defaults)
{
    int ok; float w; DWRITE_FONT_AXIS_VALUE a[2]; a[0] = W(400); a[1] = rv_av(TAG_WDTH, 100);
    RC_BEGIN(RVF_HVAR);
    w = lay_width(&c, a, 2, 0, L"AAAA", 64.0f, &ok);
    CHECK(ok); CHECKF(w == 125.0f, "width %g", w);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_weight_max_width)
{
    int ok; float w; DWRITE_FONT_AXIS_VALUE a = W(900);
    RC_BEGIN(RVF_HVAR);
    w = lay_width(&c, &a, 1, 0, L"AAAA", 64.0f, &ok);
    CHECK(ok);
    if (w == 125.0f) t_note("layout does not apply HVAR deltas of the synthetic font");
    else CHECKF(w == 137.5f, "width %g, expected 4 * 1100 / 2048 * 64 = 137.5", w);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_clamped_weight_width_equals_bound)
{
    int ok1, ok2; float w1, w2; DWRITE_FONT_AXIS_VALUE hi = W(5000), mx = W(900);
    RC_BEGIN(RVF_HVAR);
    w1 = lay_width(&c, &hi, 1, 0, L"AAAA", 64.0f, &ok1); w2 = lay_width(&c, &mx, 1, 0, L"AAAA", 64.0f, &ok2);
    CHECK(ok1 && ok2); CHECK(w1 == w2);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_untouched_glyphs_keep_width)
{
    int ok; float w; DWRITE_FONT_AXIS_VALUE a[2]; a[0] = W(900); a[1] = rv_av(TAG_WDTH, 125);
    RC_BEGIN(RVF_HVAR);
    w = lay_width(&c, a, 2, 0, L"DDDD", 64.0f, &ok);
    CHECK(ok); CHECKF(w == 125.0f, "width %g", w);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_automatic_optical_size_large)
{
    /* size 200: whatever the DIP -> point conversion, the value is above the axis maximum 144, so 'C' gets the full +40 */
    int ok1, ok2; float a, b;
    RC_BEGIN(RVF_HVAR);
    a = lay_width(&c, NULL, 0, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"C", 200.0f, &ok1);
    b = lay_width(&c, NULL, 0, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE, L"C", 200.0f, &ok2);
    CHECK(ok1 && ok2);
    CHECKF(a == 1000.0f / 2048.0f * 200.0f, "no automatic axes: width %g", a);
    if (a == b) t_note("automatic optical size has no visible effect on the synthetic font");
    else CHECKF(b == 1040.0f / 2048.0f * 200.0f, "automatic optical size: width %g, expected %g", b, 1040.0f / 2048.0f * 200.0f);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_automatic_optical_size_small)
{
    /* size 4 is below the axis minimum 8 for any conversion */
    int ok1, ok2; float a, b;
    RC_BEGIN(RVF_HVAR);
    a = lay_width(&c, NULL, 0, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"C", 4.0f, &ok1);
    b = lay_width(&c, NULL, 0, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE, L"C", 4.0f, &ok2);
    CHECK(ok1 && ok2);
    CHECKF(a == 1000.0f / 2048.0f * 4.0f, "no automatic axes: width %g", a);
    if (a != b) CHECKF(b == 970.0f / 2048.0f * 4.0f, "automatic optical size: width %g, expected %g", b, 970.0f / 2048.0f * 4.0f);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_explicit_optical_size_beats_automatic)
{
    int ok1, ok2; float a, b; DWRITE_FONT_AXIS_VALUE o = rv_av(TAG_OPSZ, 8);
    RC_BEGIN(RVF_HVAR);
    a = lay_width(&c, &o, 1, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"C", 200.0f, &ok1);
    b = lay_width(&c, &o, 1, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE, L"C", 200.0f, &ok2);
    CHECK(ok1 && ok2);
    CHECKF(a == b, "explicit opsz must win over the automatic one: %g vs %g", a, b);     /* documented: an explicit 'opsz' takes priority */
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_automatic_axes_do_not_touch_other_glyphs)
{
    int ok1, ok2; float a, b;
    RC_BEGIN(RVF_HVAR);
    a = lay_width(&c, NULL, 0, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"DDDD", 200.0f, &ok1);
    b = lay_width(&c, NULL, 0, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE, L"DDDD", 200.0f, &ok2);
    CHECK(ok1 && ok2); CHECK(a == b);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_ranged_axes)
{
    IDWriteTextFormat3 *t = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout4 *l4 = NULL; DWRITE_TEXT_METRICS m; float w;
    RC_BEGIN(RVF_HVAR);
    if (SUCCEEDED(IDWriteFactory6_CreateTextFormat(c.f6, RV_FAM, c.coll, NULL, 0, 64.0f, L"en-us", &t)) && t &&
        SUCCEEDED(IDWriteFactory6_CreateTextLayout(c.f6, L"AAAA", 4, (IDWriteTextFormat *)t, 10000.0f, 1000.0f, &l)) && l &&
        SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4)) && l4) {
        DWRITE_FONT_AXIS_VALUE a = W(900); DWRITE_TEXT_RANGE r = { 0, 2 };
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, &a, 1, r));
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m)); w = m.widthIncludingTrailingWhitespace;
        if (w == 125.0f) t_note("ranged axis values: HVAR deltas not applied by the layout");
        else CHECKF(w == 131.25f, "width %g, expected 2 * 1100 / 2048 * 64 + 2 * 31.25 = 131.25", w);
    } else CHECK(0);
    DW_RELEASE(l4); DW_RELEASE(l); DW_RELEASE(t);
    RC_END();
}
TEST(r2_variable_fonts_metrics_layout_font_face_of_run_has_axis_values)
{
    /* the face a run is drawn with reports the axis values the layout resolved */
    IDWriteTextFormat3 *t = NULL; IDWriteTextLayout *l = NULL; DWRITE_FONT_AXIS_VALUE a = W(700);
    RC_BEGIN(RVF_HVAR);
    if (SUCCEEDED(IDWriteFactory6_CreateTextFormat(c.f6, RV_FAM, c.coll, &a, 1, 64.0f, L"en-us", &t)) && t &&
        SUCCEEDED(IDWriteFactory6_CreateTextLayout(c.f6, L"AAAA", 4, (IDWriteTextFormat *)t, 10000.0f, 1000.0f, &l)) && l) {
        dwx_renderer *r = dwx_renderer_new();
        if (r && SUCCEEDED(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0, 0)) && r->count[DWX_DRAW_GLYPHRUN] > 0) {
            IDWriteFontFace5 *f5 = NULL;
            if (r->calls[0].face && SUCCEEDED(IDWriteFontFace_QueryInterface(r->calls[0].face, &IID_IDWriteFontFace5, (void **)&f5)) && f5) {
                DWRITE_FONT_AXIS_VALUE v[16]; UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(f5);
                if (n && n <= 16 && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(f5, v, n))) { int k = rv_find_value(v, n, TAG_WGHT); t_note("drawn face wght = %g (requested 700)", k >= 0 ? v[k].value : -1.0f); CHECK(k >= 0 && v[k].value == 700.0f); }
                DW_RELEASE(f5);
            } else t_note("drawn face has no IDWriteFontFace5");
        } else t_note("draw produced no glyph run");
        if (r) dwx_release(r);
    } else CHECK(0);
    DW_RELEASE(l); DW_RELEASE(t);
    RC_END();
}
