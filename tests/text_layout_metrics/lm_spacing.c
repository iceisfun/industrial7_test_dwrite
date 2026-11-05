/* Line spacing (default / uniform / proportional, font line gap usage) and DWRITE_TEXT_METRICS.
 *
 * Per-run ascent/descent come from the font (win ascent/descent plus the line gap above, unless the gap is disabled or the layout is
 * pixel-aligned/GDI-compatible and usage is DEFAULT). A line takes the largest ascent and the largest descent of its runs:
 *   DEFAULT       height = asc + desc, baseline = asc
 *   UNIFORM(h,b)  every run gets ascent b and descent h-b (inline objects and fonts are ignored): height = h, baseline = b
 *   PROPORTIONAL  total = (asc+desc)*H, asc' = asc*B, extra = total-asc'-desc, leadingBefore = extra*lb, leadingAfter = extra-lb*extra,
 *                 height = total, baseline = asc' + leadingBefore
 * Setters validate: spacing/height >= 0 and not NaN, baseline not NaN (negative accepted), method <= PROPORTIONAL, and for the struct form
 * leadingBefore in [0,1]; a failing call changes nothing. Storing a struct then reading it back is exact; the old three-argument setter
 * resets leadingBefore and fontLineGapUsage to 0. */
#include "tlm.h"

static void prop_expect(double H, double B, double lb, double asc, double desc, double *height, double *baseline, double *before, double *after)
{
    double total = (asc + desc) * H, a = asc * B, extra = total - a - desc;
    *height = total; *before = extra * lb; *after = extra - *before; *baseline = a + *before;
}

/* ---- uniform ------------------------------------------------------------------------------------------------------ */
TEST(ls_uniform_single_line)
{
    TM_BEGIN(); TM_L(l, L"ABC", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); NEAR(ln.v[0].height, 30.0f); NEAR(ln.v[0].baseline, 24.0f); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_uniform_every_line)
{
    TM_BEGIN(); TM_L(l, L"A\nB\nC\nD", 1000, 1000);
    tm_lines ln; UINT32 i;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 25.0f, 20.0f));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 4); for (i = 0; i < ln.n; i++) { NEAR(ln.v[i].height, 25.0f); NEAR(ln.v[i].baseline, 20.0f); } }
    DW_RELEASE(l); TM_END();
}
TEST(ls_uniform_table)
{
    TM_BEGIN();
    static const float hb[][2] = { {10, 8}, {20, 16}, {17.578125f, 14.0625f}, {50, 10}, {100, 90}, {5, 4}, {40, 0}, {40, 40}, {1, 0.5f}, {200, 150}, {30, -5} };
    size_t k;
    for (k = 0; k < sizeof hb / sizeof hb[0]; k++) {
        IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"AB\nCD", 5, SZ, 1000, 1000); tm_lines ln;
        if (!l) { CHECKF(0, "layout"); continue; }
        CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, hb[k][0], hb[k][1]));
        if (tm_get_lines(l, &ln) && ln.n == 2) {
            CHECKF(DW_APPROX(ln.v[0].height, hb[k][0], EPS) && DW_APPROX(ln.v[1].height, hb[k][0], EPS), "case %zu: heights %g %g want %g", k, ln.v[0].height, ln.v[1].height, hb[k][0]);
            CHECKF(DW_APPROX(ln.v[0].baseline, hb[k][1], EPS) && DW_APPROX(ln.v[1].baseline, hb[k][1], EPS), "case %zu: baselines %g %g want %g", k, ln.v[0].baseline, ln.v[1].baseline, hb[k][1]);
        } else CHECKF(0, "case %zu lines", k);
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(ls_uniform_zero_height)
{
    TM_BEGIN(); TM_L(l, L"A\nB", 1000, 1000);
    tm_lines ln; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 0.0f, 0.0f));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 2); NEAR(ln.v[0].height, 0.0f); NEAR(ln.v[0].baseline, 0.0f); }
    tm_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 2);
    NEAR(m.height, 0.0f);
    DW_RELEASE(l); TM_END();
}
TEST(ls_uniform_baseline_beyond_height)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 10.0f, 15.0f));
    if (tm_get_lines(l, &ln)) { t_note("uniform height 10 baseline 15 -> line height %g baseline %g", ln.v[0].height, ln.v[0].baseline); NEAR(ln.v[0].height, 10.0f); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_uniform_ignores_font_size)
{
    TM_BEGIN(); TM_L(l, L"AAAABBBB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 48.0f, RNG(2, 3)));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); NEAR(ln.v[0].height, 30.0f); NEAR(ln.v[0].baseline, 24.0f); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_uniform_ignores_font_family)
{
    TM_BEGIN(); TM_L(l, L"AAAABBBB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, FAM_ASYM, RNG(4, 4)));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 22.0f, 18.0f));
    if (tm_get_lines(l, &ln)) { NEAR(ln.v[0].height, 22.0f); NEAR(ln.v[0].baseline, 18.0f); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_uniform_ignores_inline_object)
{
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    dwx_inline *io = dwx_inline_new(20.0f, 80.0f, 60.0f, FALSE);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    if (tm_get_lines(l, &ln)) { NEAR(ln.v[0].height, 30.0f); NEAR(ln.v[0].baseline, 24.0f); }
    DW_RELEASE(l); dwx_release(io); TM_END();
}
TEST(ls_uniform_ignores_font_line_gap)
{
    TM_BEGIN(); TM_LF(l, FAM_GAP, L"AB\nCD", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    if (tm_get_lines(l, &ln)) { NEAR(ln.v[0].height, 30.0f); NEAR(ln.v[0].baseline, 24.0f); NEAR(ln.v[1].height, 30.0f); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_uniform_trailing_empty_line)
{
    TM_BEGIN(); TM_L(l, L"A\n", 1000, 1000);
    tm_lines ln; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 2); NEAR(ln.v[0].height, 30.0f); NEAR(ln.v[1].height, 30.0f); NEAR(ln.v[1].baseline, 24.0f); }
    tm_metrics(l, &m);
    NEAR(m.height, 60.0f);
    DW_RELEASE(l); TM_END();
}
TEST(ls_uniform_text_metrics_height)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB CCC DDD", 4 * CW, 1000);
    tm_lines ln; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 12.5f, 10.0f));
    tm_metrics(l, &m);
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 4); EXPECT_EQ(m.lineCount, 4); NEAR(m.height, 4 * 12.5f); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_default_ignores_values)
{
    TM_BEGIN(); TM_L(l, L"AB\nCD", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_DEFAULT, 50.0f, 40.0f));
    if (tm_get_lines(l, &ln) && ln.n == 2) { NEAR(ln.v[0].height, LH); NEAR(ln.v[0].baseline, ASC); NEAR(ln.v[1].height, LH); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_back_to_default_restores)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 40.0f, 30.0f));
    if (tm_get_lines(l, &ln)) NEAR(ln.v[0].height, 40.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_DEFAULT, 0.0f, 0.0f));
    if (tm_get_lines(l, &ln)) { NEAR(ln.v[0].height, LH); NEAR(ln.v[0].baseline, ASC); }
    DW_RELEASE(l); TM_END();
}

/* ---- setter / getter ----------------------------------------------------------------------------------------------- */
TEST(ls_get_set_roundtrip)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    DWRITE_LINE_SPACING_METHOD m = (DWRITE_LINE_SPACING_METHOD)99; float s = -1, b = -1;
    CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &s, &b));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_DEFAULT);
    t_note("default line spacing: spacing %g baseline %g", s, b);
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 31.5f, 22.25f));
    CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &s, &b));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(s, 31.5f); CHECK_FLOAT_EQ(b, 22.25f);
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 0.75f));
    CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &s, &b));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL); CHECK_FLOAT_EQ(s, 1.5f); CHECK_FLOAT_EQ(b, 0.75f);
    DW_RELEASE(l); TM_END();
}
TEST(ls_set_invalid_method)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    static const int bad[] = { 3, 4, 100, -1, 0x7fffffff };
    size_t i; DWRITE_LINE_SPACING_METHOD m; float s, b;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.0f, 15.0f));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECKF(IDWriteTextLayout_SetLineSpacing(l, (DWRITE_LINE_SPACING_METHOD)bad[i], 20.0f, 15.0f) == DW_E_INVALIDARG, "method %d", bad[i]);
        IDWriteTextLayout_GetLineSpacing(l, &m, &s, &b);
        CHECKF(m == DWRITE_LINE_SPACING_METHOD_UNIFORM && s == 20.0f && b == 15.0f, "method %d changed state", bad[i]);
    }
    DW_RELEASE(l); TM_END();
}
TEST(ls_set_invalid_values)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    float nan = NAN; tm_lines ln;
    CHECK_HR(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, -1.0f, 5.0f), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, -0.001f, 5.0f), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, -INFINITY, 5.0f), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, nan, 5.0f), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 5.0f, nan), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_DEFAULT, nan, nan), DW_E_INVALIDARG);
    if (tm_get_lines(l, &ln)) NEAR(ln.v[0].height, LH);                    /* nothing was applied */
    DW_RELEASE(l); TM_END();
}
TEST(ls_set_accepts_negative_baseline_and_zero)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 0.0f, 0.0f));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, -0.0f, -3.0f));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 1e20f, 1e20f));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 2.0f));
    DW_RELEASE(l); TM_END();
}
TEST(ls_struct_validation)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    TM_QI(l3, IDWriteTextLayout3, l);
    DWRITE_LINE_SPACING ok = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.0f, 15.0f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT }, s;
    float nan = NAN;
    if (!t_is_wine()) CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, NULL), DW_E_INVALIDARG);   /* NULL is rejected before any read */
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &ok));
    s = ok; s.method = (DWRITE_LINE_SPACING_METHOD)3; CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, &s), DW_E_INVALIDARG);
    s = ok; s.height = -1.0f; CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, &s), DW_E_INVALIDARG);
    s = ok; s.height = nan; CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, &s), DW_E_INVALIDARG);
    s = ok; s.baseline = nan; CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, &s), DW_E_INVALIDARG);
    s = ok; s.leadingBefore = -0.01f; CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, &s), DW_E_INVALIDARG);
    s = ok; s.leadingBefore = 1.01f; CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, &s), DW_E_INVALIDARG);
    s = ok; s.leadingBefore = nan; CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, &s), DW_E_INVALIDARG);
    s = ok; s.leadingBefore = 0.0f; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s = ok; s.leadingBefore = 1.0f; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s = ok; s.baseline = -4.0f; CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &s));
    s = ok; s.fontLineGapUsage = (DWRITE_FONT_LINE_GAP_USAGE)3;
    t_note("fontLineGapUsage 3 -> %s", dw_hr_name(IDWriteTextLayout3_SetLineSpacing(l3, &s)));
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TM_END();
}
TEST(ls_struct_failure_keeps_state)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    TM_QI(l3, IDWriteTextLayout3, l);
    DWRITE_LINE_SPACING ok = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 1.0f, 0.25f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, bad = ok, got;
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &ok));
    bad.leadingBefore = 2.0f;
    CHECK_HR(IDWriteTextLayout3_SetLineSpacing(l3, &bad), DW_E_INVALIDARG);
    memset(&got, 0xcc, sizeof got);
    CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &got));
    EXPECT_EQ(got.method, ok.method); CHECK_FLOAT_EQ(got.height, ok.height); CHECK_FLOAT_EQ(got.baseline, ok.baseline);
    CHECK_FLOAT_EQ(got.leadingBefore, ok.leadingBefore); EXPECT_EQ(got.fontLineGapUsage, ok.fontLineGapUsage);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TM_END();
}
TEST(ls_struct_roundtrip_and_old_setter_resets)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    TM_QI(l3, IDWriteTextLayout3, l);
    DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.25f, 0.5f, 0.75f, DWRITE_FONT_LINE_GAP_USAGE_DISABLED }, got;
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &in));
    memset(&got, 0xcc, sizeof got);
    CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &got));
    EXPECT_EQ(got.method, in.method); CHECK_FLOAT_EQ(got.height, 1.25f); CHECK_FLOAT_EQ(got.baseline, 0.5f);
    CHECK_FLOAT_EQ(got.leadingBefore, 0.75f); EXPECT_EQ(got.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_DISABLED);
    { DWRITE_LINE_SPACING_METHOD m; float s, b;                     /* the three-argument getter sees the same method/height/baseline */
      CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &s, &b)); EXPECT_EQ(m, in.method); CHECK_FLOAT_EQ(s, 1.25f); CHECK_FLOAT_EQ(b, 0.5f); }
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 9.0f, 7.0f));
    memset(&got, 0xcc, sizeof got);
    CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &got));
    EXPECT_EQ(got.method, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(got.height, 9.0f); CHECK_FLOAT_EQ(got.baseline, 7.0f);
    CHECK_FLOAT_EQ(got.leadingBefore, 0.0f); EXPECT_EQ(got.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT);
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TM_END();
}

/* ---- proportional ----------------------------------------------------------------------------------------------------- */
TEST(ls_proportional_formula)
{
    TM_BEGIN();
    static const double c[][3] = { {1, 1, 0}, {1, 1, 1}, {2, 1, 0}, {2, 1, 1}, {2, 1, 0.5}, {1.5, 1, 0.25}, {1, 2, 0.5}, {0.5, 1, 0}, {1, 0.5, 1},
                                   {3, 1, 0.1}, {1, 1, 0.5}, {0.25, 0.25, 0.5}, {0, 0, 0}, {2, 2, 1}, {1.2, 0.8, 0.3} };
    size_t k;
    for (k = 0; k < sizeof c / sizeof c[0]; k++) {
        IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"AB\nCD", 5, SZ, 1000, 1000); IDWriteTextLayout3 *l3 = NULL; DWRITE_LINE_METRICS1 m[4]; UINT32 n = 0, i;
        double eh, eb, ebf, eaf;
        if (!l || FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout3, (void **)&l3))) { DW_RELEASE(l); t_skip("IDWriteTextLayout3 unavailable"); break; }
        { DWRITE_LINE_SPACING sp; sp.method = DWRITE_LINE_SPACING_METHOD_PROPORTIONAL; sp.height = (float)c[k][0]; sp.baseline = (float)c[k][1]; sp.leadingBefore = (float)c[k][2]; sp.fontLineGapUsage = DWRITE_FONT_LINE_GAP_USAGE_DEFAULT;
          CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &sp)); }
        prop_expect(c[k][0], c[k][1], c[k][2], ASC, DESC, &eh, &eb, &ebf, &eaf);
        CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, m, 4, &n));
        EXPECT_EQ(n, 2);
        for (i = 0; i < n && i < 2; i++) {
            CHECKF(DW_APPROX(m[i].height, eh, 0.005), "case %zu line %u: height %g want %g", k, i, m[i].height, eh);
            CHECKF(DW_APPROX(m[i].baseline, eb, 0.005), "case %zu line %u: baseline %g want %g", k, i, m[i].baseline, eb);
            CHECKF(DW_APPROX(m[i].leadingBefore, ebf, 0.005), "case %zu line %u: leadingBefore %g want %g", k, i, m[i].leadingBefore, ebf);
            CHECKF(DW_APPROX(m[i].leadingAfter, eaf, 0.005), "case %zu line %u: leadingAfter %g want %g", k, i, m[i].leadingAfter, eaf);
        }
        IDWriteTextLayout3_Release(l3); DW_RELEASE(l);
    }
    TM_END();
}
TEST(ls_proportional_v0_lines_match)
{
    /* the plain DWRITE_LINE_METRICS entries carry the same height and baseline as the extended ones */
    TM_BEGIN(); TM_L(l, L"AB\nCD", 1000, 1000);
    TM_QI(l3, IDWriteTextLayout3, l);
    DWRITE_LINE_SPACING sp = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 1.0f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT };
    tm_lines a; DWRITE_LINE_METRICS1 b[4]; UINT32 n = 0, i;
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &sp));
    if (tm_get_lines(l, &a)) {
        CHECK_HR_OK(IDWriteTextLayout3_GetLineMetrics(l3, b, 4, &n));
        EXPECT_EQ(n, a.n);
        for (i = 0; i < n && i < a.n; i++) { CHECK_FLOAT_EQ(a.v[i].height, b[i].height); CHECK_FLOAT_EQ(a.v[i].baseline, b[i].baseline); }
        NEAR(a.v[0].height, 2 * LH); NEAR(a.v[0].baseline, ASC + 0.5f * LH);
    }
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TM_END();
}
TEST(ls_proportional_via_three_arg_setter)
{
    /* the three-argument setter also accepts PROPORTIONAL: height and baseline are scale factors, leadingBefore is 0 */
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 1.0f));
    if (tm_get_lines(l, &ln)) { NEAR(ln.v[0].height, 1.5f * LH); NEAR(ln.v[0].baseline, ASC); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_proportional_total_height_in_metrics)
{
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 1000);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 1.0f));
    tm_metrics(l, &m);
    NEAR(m.height, 3 * 2 * LH);
    EXPECT_EQ(m.lineCount, 3);
    DW_RELEASE(l); TM_END();
}
TEST(ls_proportional_mixed_sizes_per_line)
{
    /* the factor applies to each line's own computed height */
    TM_BEGIN(); TM_L(l, L"AA\nBB", 1000, 1000);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(3, 2)));
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 1.0f));
    if (tm_get_lines(l, &ln) && ln.n == 2) { NEAR(ln.v[0].height, 1.5f * LH); NEAR(ln.v[1].height, 1.5f * 2 * LH); }
    DW_RELEASE(l); TM_END();
}
TEST(ls_proportional_with_line_gap_font)
{
    TM_BEGIN(); TM_LF(l, FAM_GAP, L"AB", 1000, 1000);
    tm_lines base, scaled;
    if (tm_get_lines(l, &base)) {
        CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 1.0f));
        if (tm_get_lines(l, &scaled)) { NEAR(scaled.v[0].height, 2 * base.v[0].height); NEAR(scaled.v[0].baseline, base.v[0].baseline); }
    }
    DW_RELEASE(l); TM_END();
}

/* ---- font line gap usage ------------------------------------------------------------------------------------------------ */
static void gap_usage_case(tm_fx *x, DWRITE_FONT_LINE_GAP_USAGE u, int expect_gap)
{
    DWRITE_FONT_METRICS fm; IDWriteTextLayout *l = tm_layout_(x, FAM_GAP, L"AB\nCD", 5, SZ, 1000, 1000); IDWriteTextLayout3 *l3 = NULL; tm_lines ln;
    DWRITE_LINE_SPACING sp;
    if (!l || !tm_font_metrics(x, FAM_GAP, &fm) || FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout3, (void **)&l3))) { DW_RELEASE(l); t_skip("prerequisite missing"); return; }
    sp.method = DWRITE_LINE_SPACING_METHOD_DEFAULT; sp.height = 0; sp.baseline = 0; sp.leadingBefore = 0; sp.fontLineGapUsage = u;
    CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &sp));
    if (tm_get_lines(l, &ln) && ln.n == 2) {
        float s = SZ / fm.designUnitsPerEm; float gap = expect_gap ? (float)fm.lineGap : 0.0f; UINT32 i;
        for (i = 0; i < 2; i++) {
            CHECKF(DW_APPROX(ln.v[i].height, (fm.ascent + fm.descent + gap) * s, EPS), "usage %d: height %g want %g", (int)u, ln.v[i].height, (fm.ascent + fm.descent + gap) * s);
            CHECKF(DW_APPROX(ln.v[i].baseline, (fm.ascent + gap) * s, EPS), "usage %d: baseline %g want %g", (int)u, ln.v[i].baseline, (fm.ascent + gap) * s);
        }
    }
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l);
}
TEST(ls_gap_usage_default_includes_gap)  { TM_BEGIN(); gap_usage_case(&X, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT, 1); TM_END(); }
TEST(ls_gap_usage_enabled_includes_gap)  { TM_BEGIN(); gap_usage_case(&X, DWRITE_FONT_LINE_GAP_USAGE_ENABLED, 1); TM_END(); }
TEST(ls_gap_usage_disabled_drops_gap)    { TM_BEGIN(); gap_usage_case(&X, DWRITE_FONT_LINE_GAP_USAGE_DISABLED, 0); TM_END(); }
TEST(ls_gap_usage_no_effect_without_gap)
{
    TM_BEGIN(); TM_L(l, L"AB", 1000, 1000);
    TM_QI(l3, IDWriteTextLayout3, l);
    static const DWRITE_FONT_LINE_GAP_USAGE us[] = { DWRITE_FONT_LINE_GAP_USAGE_DEFAULT, DWRITE_FONT_LINE_GAP_USAGE_DISABLED, DWRITE_FONT_LINE_GAP_USAGE_ENABLED };
    size_t i; tm_lines ln;
    for (i = 0; i < 3; i++) {
        DWRITE_LINE_SPACING sp = { DWRITE_LINE_SPACING_METHOD_DEFAULT, 0, 0, 0, us[i] };
        CHECK_HR_OK(IDWriteTextLayout3_SetLineSpacing(l3, &sp));
        if (tm_get_lines(l, &ln)) { NEAR(ln.v[0].height, LH); NEAR(ln.v[0].baseline, ASC); }
    }
    IDWriteTextLayout3_Release(l3); DW_RELEASE(l); TM_END();
}
TEST(ls_gap_usage_independent_of_size)
{
    TM_BEGIN();
    static const float sizes[] = { 8.0f, 16.0f, 40.0f };
    size_t i; DWRITE_FONT_METRICS fm;
    if (!tm_font_metrics(&X, FAM_GAP, &fm)) { TM_END(); t_skip("font metrics unavailable"); return; }
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = tm_layout_(&X, FAM_GAP, L"AB", 2, sizes[i], 1000, 1000); tm_lines ln;
        if (l && tm_get_lines(l, &ln)) {
            float s = sizes[i] / fm.designUnitsPerEm;
            CHECKF(DW_APPROX(ln.v[0].height, (fm.ascent + fm.descent + fm.lineGap) * s, 0.002f + sizes[i] * 1e-5f), "size %g height %g", sizes[i], ln.v[0].height);
        }
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(ls_gdi_classic_default_drops_gap_and_rounds)
{
    /* in a GDI-compatible (pixel aligned) layout the DEFAULT usage leaves the line gap out and heights are whole pixels */
    TM_BEGIN();
    IDWriteTextFormat *fmt = tm_format(&X, FAM_GAP, SZ);
    IDWriteTextLayout *natural = NULL, *classic = NULL, *gdinat = NULL; tm_lines a, b, c; DWRITE_FONT_METRICS fm;
    if (!fmt || !tm_font_metrics(&X, FAM_GAP, &fm)) { DW_RELEASE(fmt); TM_END(); t_skip("prerequisite missing"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, L"AB", 2, fmt, 1000, 1000, &natural));
    CHECK_HR_OK(IDWriteFactory_CreateGdiCompatibleTextLayout(X.f, L"AB", 2, fmt, 1000, 1000, 1.0f, NULL, FALSE, &classic));
    CHECK_HR_OK(IDWriteFactory_CreateGdiCompatibleTextLayout(X.f, L"AB", 2, fmt, 1000, 1000, 1.0f, NULL, TRUE, &gdinat));
    if (natural && classic && gdinat && tm_get_lines(natural, &a) && tm_get_lines(classic, &b) && tm_get_lines(gdinat, &c)) {
        float s = SZ / fm.designUnitsPerEm, nogap = (fm.ascent + fm.descent) * s;
        t_note("natural height %g, gdi classic %g baseline %g, gdi natural %g baseline %g (no-gap %g)", a.v[0].height, b.v[0].height, b.v[0].baseline, c.v[0].height, c.v[0].baseline, nogap);
        CHECK(a.v[0].height > nogap + 1.0f);
        CHECK(b.v[0].height < a.v[0].height);
        CHECK(DW_APPROX(b.v[0].height, floorf(b.v[0].height + 0.5f), 0.0001f));
        CHECK(DW_APPROX(b.v[0].baseline, floorf(b.v[0].baseline + 0.5f), 0.0001f));
        CHECK(DW_APPROX(b.v[0].height, nogap, 1.0f));
        CHECK(DW_APPROX(c.v[0].height, floorf(c.v[0].height + 0.5f), 0.0001f));
    }
    DW_RELEASE(natural); DW_RELEASE(classic); DW_RELEASE(gdinat); DW_RELEASE(fmt); TM_END();
}

/* ---- spacing inherited from the text format -------------------------------------------------------------------------------- */
TEST(ls_format_uniform_inherited)
{
    TM_BEGIN();
    IDWriteTextFormat *fmt = tm_format(&X, FAM_TEST, SZ); IDWriteTextLayout *l = NULL; tm_lines ln;
    DWRITE_LINE_SPACING_METHOD m; float s, b;
    if (!fmt) { TM_END(); t_skip("format failed"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_UNIFORM, 28.0f, 21.0f));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, L"AB\nCD", 5, fmt, 1000, 1000, &l));
    if (l) {
        CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &s, &b));
        EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(s, 28.0f); CHECK_FLOAT_EQ(b, 21.0f);
        if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 2); NEAR(ln.v[0].height, 28.0f); NEAR(ln.v[0].baseline, 21.0f); NEAR(ln.v[1].height, 28.0f); }
    }
    DW_RELEASE(l); DW_RELEASE(fmt); TM_END();
}
TEST(ls_format_change_after_layout_ignored)
{
    TM_BEGIN();
    IDWriteTextFormat *fmt = tm_format(&X, FAM_TEST, SZ); IDWriteTextLayout *l = NULL; tm_lines ln;
    if (!fmt) { TM_END(); t_skip("format failed"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, L"AB", 2, fmt, 1000, 1000, &l));
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_UNIFORM, 50.0f, 40.0f));
    if (l && tm_get_lines(l, &ln)) { NEAR(ln.v[0].height, LH); NEAR(ln.v[0].baseline, ASC); }
    DW_RELEASE(l); DW_RELEASE(fmt); TM_END();
}
TEST(ls_format_struct_inherited)
{
    TM_BEGIN();
    IDWriteTextFormat *fmt = tm_format(&X, FAM_TEST, SZ); IDWriteTextFormat2 *f2 = NULL; IDWriteTextLayout *l = NULL; tm_lines ln;
    if (!fmt) { TM_END(); t_skip("format failed"); return; }
    if (FAILED(IDWriteTextFormat_QueryInterface(fmt, &IID_IDWriteTextFormat2, (void **)&f2)) || !f2) { DW_RELEASE(fmt); TM_END(); t_skip("IDWriteTextFormat2 unavailable"); return; }
    { DWRITE_LINE_SPACING sp = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 1.0f, 1.0f, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT }, got;
      CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(f2, &sp));
      memset(&got, 0xcc, sizeof got);
      CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(f2, &got));
      EXPECT_EQ(got.method, sp.method); CHECK_FLOAT_EQ(got.height, 2.0f); CHECK_FLOAT_EQ(got.leadingBefore, 1.0f);
      sp.leadingBefore = 5.0f; CHECK_HR(IDWriteTextFormat2_SetLineSpacing(f2, &sp), DW_E_INVALIDARG);
      sp.leadingBefore = 0.5f;
      if (!t_is_wine()) CHECK_HR(IDWriteTextFormat2_SetLineSpacing(f2, NULL), DW_E_INVALIDARG);   /* a NULL struct is rejected before any read */ }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, L"AB", 2, fmt, 1000, 1000, &l));
    if (l && tm_get_lines(l, &ln)) { NEAR(ln.v[0].height, 2 * LH); NEAR(ln.v[0].baseline, ASC + LH); }
    DW_RELEASE(l); IDWriteTextFormat2_Release(f2); DW_RELEASE(fmt); TM_END();
}
TEST(ls_format_setter_validation)
{
    TM_BEGIN();
    IDWriteTextFormat *fmt = tm_format(&X, FAM_TEST, SZ);
    if (!fmt) { TM_END(); t_skip("format failed"); return; }
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(fmt, (DWRITE_LINE_SPACING_METHOD)3, 10.0f, 5.0f), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_UNIFORM, -1.0f, 5.0f), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_UNIFORM, NAN, 5.0f), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_UNIFORM, 5.0f, NAN), DW_E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 1.0f));
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(fmt, DWRITE_LINE_SPACING_METHOD_UNIFORM, 5.0f, -2.0f));
    { DWRITE_LINE_SPACING_METHOD m; float s, b; CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(fmt, &m, &s, &b)); EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(s, 5.0f); CHECK_FLOAT_EQ(b, -2.0f); }
    DW_RELEASE(fmt); TM_END();
}

/* ---- DWRITE_TEXT_METRICS ---------------------------------------------------------------------------------------------------- */
TEST(tx_basic_fields)
{
    TM_BEGIN(); TM_L(l, L"AAA", 100.0f, 50.0f);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    NEAR(m.left, 0.0f); NEAR(m.top, 0.0f); NEAR(m.width, 3 * CW); NEAR(m.widthIncludingTrailingWhitespace, 3 * CW); NEAR(m.height, LH);
    CHECK_FLOAT_EQ(m.layoutWidth, 100.0f); CHECK_FLOAT_EQ(m.layoutHeight, 50.0f);
    EXPECT_EQ(m.lineCount, 1); EXPECT_EQ(m.maxBidiReorderingDepth, 1);
    DW_RELEASE(l); TM_END();
}
TEST(tx_layout_size_echo)
{
    TM_BEGIN();
    static const float sz[][2] = { {0, 0}, {1, 1}, {100, 200}, {12345.5f, 0.25f}, {1e6f, 1e6f}, {7.8125f, 17.578125f} };
    size_t i;
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"AB CD", 5, SZ, sz[i][0], sz[i][1]); DWRITE_TEXT_METRICS m;
        if (!l) { CHECKF(0, "layout"); continue; }
        tm_metrics(l, &m);
        CHECKF(tm_fbits(m.layoutWidth, sz[i][0]) && tm_fbits(m.layoutHeight, sz[i][1]), "case %zu: %g x %g", i, m.layoutWidth, m.layoutHeight);
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(tx_layout_size_follows_setters)
{
    TM_BEGIN(); TM_L(l, L"AB CD", 100, 100);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 33.5f)); CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 77.25f));
    tm_metrics(l, &m);
    CHECK_FLOAT_EQ(m.layoutWidth, 33.5f); CHECK_FLOAT_EQ(m.layoutHeight, 77.25f);
    DW_RELEASE(l); TM_END();
}
TEST(tx_height_is_sum_of_lines)
{
    TM_BEGIN();
    int k;
    for (k = 1; k <= 16; k++) {
        IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"AAAA BBBB CCCC", 14, SZ, k * CW, 1000); DWRITE_TEXT_METRICS m; tm_lines ln;
        if (!l) { CHECKF(0, "layout"); continue; }
        tm_metrics(l, &m);
        if (tm_get_lines(l, &ln)) {
            CHECKF(m.lineCount == ln.n, "width %d: lineCount %u vs %u", k, m.lineCount, ln.n);
            CHECKF(DW_APPROX(m.height, ln.n * LH, 0.01f), "width %d: height %g want %g", k, m.height, ln.n * LH);
        }
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(tx_width_is_widest_line)
{
    TM_BEGIN(); TM_L(l, L"AA\nBBBB\nC", 1000, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    NEAR(m.width, 4 * CW); NEAR(m.widthIncludingTrailingWhitespace, 4 * CW); EXPECT_EQ(m.lineCount, 3);
    DW_RELEASE(l); TM_END();
}
TEST(tx_width_after_wrap)
{
    TM_BEGIN(); TM_L(l, L"AAAA BBBB", 6 * CW, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    NEAR(m.width, 4 * CW); EXPECT_EQ(m.lineCount, 2);
    CHECK(m.widthIncludingTrailingWhitespace + EPS >= m.width);
    CHECK(m.width <= m.layoutWidth + EPS);
    DW_RELEASE(l); TM_END();
}
TEST(tx_trailing_space_width_split)
{
    TM_BEGIN(); TM_L(l, L"AAAA BBBB ", 6 * CW, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    NEAR(m.width, 4 * CW);
    CHECK(m.widthIncludingTrailingWhitespace >= m.width);
    t_note("\"AAAA BBBB \" at %g: width %g, with trailing whitespace %g", 6 * CW, m.width, m.widthIncludingTrailingWhitespace);
    DW_RELEASE(l); TM_END();
}
TEST(tx_no_wrap_long_text)
{
    TM_BEGIN(); TM_L(l, L"AAAA BBBB CCCC DDDD", 5 * CW, 1000);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    tm_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 1);
    NEAR(m.width, 19 * CW);
    CHECK_FLOAT_EQ(m.layoutWidth, 5 * CW);
    CHECK(m.width > m.layoutWidth);
    DW_RELEASE(l); TM_END();
}
TEST(tx_width_vs_max_width_sweep)
{
    /* breakable text never reports a width above the layout width (as long as the longest word fits) */
    TM_BEGIN();
    int k;
    for (k = 4; k <= 20; k++) {
        IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"AAAA BBBB CCCC DDDD", 19, SZ, k * CW, 1000); DWRITE_TEXT_METRICS m;
        if (!l) { CHECKF(0, "layout"); continue; }
        tm_metrics(l, &m);
        CHECKF(m.width <= k * CW + EPS, "width %d letters: metrics.width %g", k, m.width);
        CHECKF(m.widthIncludingTrailingWhitespace + EPS >= m.width, "width %d: incl %g < %g", k, m.widthIncludingTrailingWhitespace, m.width);
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(tx_after_setmaxwidth_change)
{
    TM_BEGIN(); TM_L(l, L"AAAA BBBB CCCC", 100 * CW, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m); EXPECT_EQ(m.lineCount, 1); NEAR(m.width, 14 * CW);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 5 * CW));
    tm_metrics(l, &m); EXPECT_EQ(m.lineCount, 3); NEAR(m.width, 4 * CW); NEAR(m.height, 3 * LH); CHECK_FLOAT_EQ(m.layoutWidth, 5 * CW);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 9 * CW));
    tm_metrics(l, &m); EXPECT_EQ(m.lineCount, 2); NEAR(m.width, 9 * CW);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 100 * CW));
    tm_metrics(l, &m); EXPECT_EQ(m.lineCount, 1); NEAR(m.height, LH);
    DW_RELEASE(l); TM_END();
}
TEST(tx_after_fontsize_change)
{
    TM_BEGIN(); TM_L(l, L"AAAA", 1000, 1000);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(0, 4)));
    tm_metrics(l, &m);
    NEAR(m.width, 8 * CW); NEAR(m.height, 2 * LH);
    DW_RELEASE(l); TM_END();
}
TEST(tx_max_height_does_not_clip_metrics)
{
    TM_BEGIN(); TM_L(l, L"A\nB\nC", 1000, 5.0f);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    NEAR(m.height, 3 * LH); CHECK_FLOAT_EQ(m.layoutHeight, 5.0f); EXPECT_EQ(m.lineCount, 3);
    DW_RELEASE(l); TM_END();
}
TEST(tx_empty_text)
{
    TM_BEGIN();
    IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"", 0, SZ, 100, 100); DWRITE_TEXT_METRICS m;
    if (!l) { TM_END(); t_skip("CreateTextLayout failed"); return; }
    tm_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 1); NEAR(m.width, 0.0f); NEAR(m.widthIncludingTrailingWhitespace, 0.0f);
    CHECK_FLOAT_EQ(m.layoutWidth, 100.0f); CHECK_FLOAT_EQ(m.layoutHeight, 100.0f);
    t_note("empty layout height %g", m.height);
    DW_RELEASE(l); TM_END();
}
TEST(tx_trailing_newline_counts_empty_line)
{
    TM_BEGIN(); TM_L(l, L"A\n", 1000, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    EXPECT_EQ(m.lineCount, 2);
    NEAR(m.width, CW);
    t_note("\"A\\n\": height %g (two lines %g)", m.height, 2 * LH);
    DW_RELEASE(l); TM_END();
}
TEST(tx_family_heights)
{
    TM_BEGIN();
    static const struct { const WCHAR *fam; int du; } ft[] = { { FAM_TEST, 2250 }, { FAM_ASYM, 2200 }, { FAM_UNI, 2250 } };
    size_t i;
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = tm_layout_(&X, ft[i].fam, L"AB", 2, SZ, 1000, 1000); DWRITE_TEXT_METRICS m;
        if (!l) { CHECKF(0, "layout"); continue; }
        tm_metrics(l, &m);
        CHECKF(DW_APPROX(m.height, U16(ft[i].du), EPS), "family %zu: height %g want %g", i, m.height, U16(ft[i].du));
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(tx_alignment_offsets)
{
    /* LEADING keeps left at 0; CENTER / TRAILING move the text block inside the layout width */
    TM_BEGIN(); TM_L(l, L"AAAA", 20 * CW, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m); NEAR(m.left, 0.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    tm_metrics(l, &m); NEAR(m.left, 8 * CW); NEAR(m.width, 4 * CW);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    tm_metrics(l, &m); NEAR(m.left, 16 * CW);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_LEADING));
    tm_metrics(l, &m); NEAR(m.left, 0.0f);
    DW_RELEASE(l); TM_END();
}
TEST(tx_paragraph_alignment_offsets)
{
    TM_BEGIN(); TM_L(l, L"AAAA", 100.0f, 100.0f);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m); NEAR(m.top, 0.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    tm_metrics(l, &m); NEAR(m.top, (100.0f - LH) / 2);
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    tm_metrics(l, &m); NEAR(m.top, 100.0f - LH);
    NEAR(m.height, LH);
    DW_RELEASE(l); TM_END();
}
TEST(tx_repeatable)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB\nCC", 4 * CW, 100);
    DWRITE_TEXT_METRICS a, b;
    tm_metrics(l, &a); tm_metrics(l, &b);
    CHECK(!memcmp(&a, &b, sizeof a));
    DW_RELEASE(l); TM_END();
}
TEST(tx_matches_clusters_and_lines_after_edits)
{
    TM_BEGIN(); TM_L(l, L"AA BB CC DD", 1000, 1000);
    DWRITE_TEXT_METRICS m; tm_lines ln; tm_clusters c; int k;
    for (k = 0; k < 6; k++) {
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, (2 + k) * CW));
        CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 16.0f + 4.0f * (k & 1), RNG(0, 11)));
        tm_metrics(l, &m);
        if (tm_get_lines(l, &ln) && tm_get_clusters(l, &c)) {
            CHECKF(m.lineCount == ln.n, "step %d: lineCount", k);
            CHECKF(DW_APPROX(m.height, tm_sum_line_height(&ln), 0.01f), "step %d: height", k);
            CHECKF(tm_sum_cluster_len(&c) == 11 && tm_sum_line_len(&ln) == 11, "step %d: coverage", k);
        }
    }
    DW_RELEASE(l); TM_END();
}
TEST(tx_rtl_hebrew_depth_note)
{
    TM_BEGIN(); TM_LF(l, FAM_UNI, L"A אב B", 1000, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    CHECK(m.maxBidiReorderingDepth >= 1);
    t_note("A HEBREW B in an LTR paragraph: maxBidiReorderingDepth %u", m.maxBidiReorderingDepth);
    DW_RELEASE(l); TM_END();
}
