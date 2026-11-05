/* GetRecommendedRenderingMode on synthetic fonts WITH a version-1 'gasp' table.
 *
 * Facts (dwrite.dll!0x48618 OpenTypeFontFaceBuilder::ReadRenderingModeRanges, !0x95c8c GetRenderingModeFromFontSize, !0x95d18 IsGridFitFontSize):
 *   The font loader turns the gasp ranges into three "toggle vectors" (flag 8 GASP_SYMMETRIC_SMOOTHING, flag 2 GASP_DOGRAY, flag 4
 *   GASP_SYMMETRIC_GRIDFIT).  A range with a non-increasing maxPPEM is skipped, neighbours with the same flag state merge, the last
 *   range extends to 0xFFFF.  The rounded ppem (half up) is looked up with lower_bound: an odd index means "flag set".
 *   NATURAL measuring on an axis-aligned face: flag 8 set -> NATURAL_SYMMETRIC, not set -> NATURAL (a gasp table exists, so the hinting
 *   heuristics of the no-gasp case are not used).  Sideways (or 90 degree rotated) text and unaligned transforms are promoted to NATURAL_SYMMETRIC.
 *   A grid fit mode derived from the gasp can only differ from the no-gasp answer by being DISABLED where flag 4 is clear.
 */
#include "rr.h"

#define RT(n) TEST(r2_rendering_modes_##n)

static int expect_mode(const rr_gasp *g, const rr_in *in) { return rr_model(in, g, NULL, NULL); }

static void sweep(const rr_gasp *g, int sw, const DWRITE_MATRIX *m, float dpi, int pmin, int pmax) {
    RR_FIX3(x, g);
    int p;
    for (p = pmin; p <= pmax; p++) {
        float em = (float)p * 96.0f / dpi;
        rr_in in = m ? rr_mkm(em, dpi, m, 1, 0) : rr_mk(em, dpi, 1, 0);
        int m1, m2, m3, g2, g3, want;
        in.sw = sw;
        want = expect_mode(g, &in);
        CHECK_HR_OK(rr_call1(&x, &in, &m1)); CHECK_HR_OK(rr_call2(&x, &in, x.dp, &m2, &g2)); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m3, &g3));
        if (want < 0) { t_note("model undecided at ppem %d", p); continue; }
        t_check(m3 == want && m1 == (want == 7 ? 5 : want) && m2 == m1, __FILE__, __LINE__, "gasp mode", "%s ppem %d sw %d dpi %g: f1 %d f2 %d f3 %d want %d", g->name, p, sw, dpi, m1, m2, m3, want);
    }
    rr_close(&x);
}
static void sweep_aligned(int k) { sweep(&rr_gasps[k], 0, NULL, 96, 1, 130); }
static void sweep_sideways(int k) { sweep(&rr_gasps[k], 1, NULL, 96, 1, 130); }
static void sweep_rot90(int k) { static const DWRITE_MATRIX r = { 0, 1, -1, 0, 0, 0 }; sweep(&rr_gasps[k], 0, &r, 96, 1, 130); }
static void sweep_rot90_sideways(int k) { static const DWRITE_MATRIX r = { 0, 1, -1, 0, 0, 0 }; sweep(&rr_gasps[k], 1, &r, 96, 1, 130); }
static void sweep_rot45(int k) { static const DWRITE_MATRIX r = { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 }; sweep(&rr_gasps[k], 0, &r, 96, 1, 130); }
static void sweep_dpi192(int k) { sweep(&rr_gasps[k], 0, NULL, 192, 1, 130); }
static void sweep_scale2(int k) { static const DWRITE_MATRIX r = { 2, 0, 0, 2, 0, 0 }; sweep(&rr_gasps[k], 0, &r, 96, 2, 130); }

/* grid fit: compared with the same query on a font without gasp */
static void grid_rel(int k, int mm) {
    rr_font a, b; int p;
    if (!rr_open(&a, &rr_gasps[k])) { t_skip("font"); rr_close(&a); return; }
    if (!rr_open(&b, NULL) || !a.f3 || !b.f3) { t_skip("font"); rr_close(&a); rr_close(&b); return; }
    for (p = 1; p <= 60; p++) {
        rr_in in = rr_mk((float)p, 96, 1, mm); int ma, mb, ga, gb, flag4;
        CHECK_HR_OK(rr_call3(&a, &in, a.dp, &ma, &ga)); CHECK_HR_OK(rr_call3(&b, &in, b.dp, &mb, &gb));
        flag4 = rr_gasp_flag(&rr_gasps[k], 4, p, 1);
        if (mm == 0 && !(ma == mb)) { /* modes may legitimately differ for NATURAL; the grid relation below still holds per mode */ }
        if (ma != mb) continue;
        if (flag4) t_check(ga == gb, __FILE__, __LINE__, "grid fit with flag 4", "%s mm %d ppem %d: gasp %d nogasp %d", rr_gasps[k].name, mm, p, ga, gb);
        else t_check(ga <= gb, __FILE__, __LINE__, "grid fit without flag 4", "%s mm %d ppem %d: gasp %d nogasp %d", rr_gasps[k].name, mm, p, ga, gb);
    }
    rr_close(&a); rr_close(&b);
}
static void grid_nat(int k) { grid_rel(k, 0); }
static void grid_classic(int k) { grid_rel(k, 1); }
static void grid_gdi_nat(int k) { grid_rel(k, 2); }

#define GT(idx, nm) \
    RT(gasp_##nm##_aligned_sweep) { sweep_aligned(idx); } \
    RT(gasp_##nm##_sideways_sweep) { sweep_sideways(idx); } \
    RT(gasp_##nm##_rot90_sweep) { sweep_rot90(idx); } \
    RT(gasp_##nm##_rot90_sideways_sweep) { sweep_rot90_sideways(idx); } \
    RT(gasp_##nm##_rot45_sweep) { sweep_rot45(idx); } \
    RT(gasp_##nm##_dpi192_sweep) { sweep_dpi192(idx); } \
    RT(gasp_##nm##_scale2_sweep) { sweep_scale2(idx); } \
    RT(gasp_##nm##_grid_fit_relation) { grid_nat(idx); grid_classic(idx); grid_gdi_nat(idx); }
GT(0, all_flags)
GT(1, no_symmetric_smoothing)
GT(2, no_flags)
GT(3, smoothing_only)
GT(4, smoothing_9_to_16)
GT(5, smoothing_up_to_12)
GT(6, smoothing_above_20)
GT(7, alternating)
GT(8, single_50_smoothing)
GT(9, single_50_plain)
GT(10, duplicate_max_skipped)
GT(11, descending_range_skipped)
GT(12, fine_toggles)
GT(13, grid_and_smoothing_split)
GT(14, grid_only)
GT(15, gray_then_grid)

/* ---- spot checks with literal expectations (independent of the model) ---------------------------------------------------- */
static int nat(rr_font *x, float em, float dpi, int sw) {
    rr_in in = rr_mk(em, dpi, 1, 0); int m, g;
    in.sw = sw;
    if (FAILED(rr_call3(x, &in, x->dp, &m, &g))) return -1;
    return m;
}
RT(gasp_literal_smoothing_range_9_to_16) {
    RR_FIX3(x, &rr_gasps[4]);          /* {8: 0x3}, {16: 0xB}, {65535: 0x3} */
    EXPECT_EQ(nat(&x, 1, 96, 0), 4); EXPECT_EQ(nat(&x, 8, 96, 0), 4);
    EXPECT_EQ(nat(&x, 9, 96, 0), 5); EXPECT_EQ(nat(&x, 12, 96, 0), 5); EXPECT_EQ(nat(&x, 16, 96, 0), 5);
    EXPECT_EQ(nat(&x, 17, 96, 0), 4); EXPECT_EQ(nat(&x, 40, 96, 0), 4); EXPECT_EQ(nat(&x, 120, 96, 0), 4);
    rr_close(&x);
}
RT(gasp_literal_rounding_half_up) {
    RR_FIX3(x, &rr_gasps[4]);
    EXPECT_EQ(nat(&x, 8.4375f, 96, 0), 4);       /* rounds to 8 */
    EXPECT_EQ(nat(&x, 8.5f, 96, 0), 5);          /* tie rounds up to 9 */
    EXPECT_EQ(nat(&x, 16.4375f, 96, 0), 5);      /* 16 */
    EXPECT_EQ(nat(&x, 16.5f, 96, 0), 4);         /* 17 */
    EXPECT_EQ(nat(&x, 8.5f, 96, 1), 5);          /* sideways is promoted anyway */
    rr_close(&x);
}
RT(gasp_literal_ppem_from_dpi) {
    RR_FIX3(x, &rr_gasps[4]);
    EXPECT_EQ(nat(&x, 6, 144, 0), 5);            /* 6 * 1.5 = 9 */
    EXPECT_EQ(nat(&x, 5, 144, 0), 4);            /* 7.5 -> 8 */
    EXPECT_EQ(nat(&x, 8, 192, 0), 4 + (16 <= 16 ? 1 : 0));   /* 16 */
    EXPECT_EQ(nat(&x, 9, 192, 0), 4);            /* 18 */
    rr_close(&x);
}
RT(gasp_literal_no_symmetric_flag_means_natural_aligned) {
    RR_FIX3(x, &rr_gasps[1]);
    static const float e[] = { 1, 6, 9, 12, 16, 20, 21, 30, 60, 99 };
    int i;
    for (i = 0; i < 10; i++) { EXPECT_EQ(nat(&x, e[i], 96, 0), 4); EXPECT_EQ(nat(&x, e[i], 96, 1), 5); }
    rr_close(&x);
}
RT(gasp_literal_symmetric_everywhere) {
    RR_FIX3(x, &rr_gasps[0]);
    static const float e[] = { 1, 6, 9, 12, 16, 20, 21, 30, 60, 99 };
    int i;
    for (i = 0; i < 10; i++) EXPECT_EQ(nat(&x, e[i], 96, 0), 5);
    rr_close(&x);
}
RT(gasp_literal_last_range_extends_beyond_maxppem) {
    RR_FIX3(x, &rr_gasps[8]);          /* single range up to 50 ppem with flag 8 */
    EXPECT_EQ(nat(&x, 50, 96, 0), 5); EXPECT_EQ(nat(&x, 51, 96, 0), 5); EXPECT_EQ(nat(&x, 90, 96, 0), 5);
    rr_close(&x);
}
RT(gasp_literal_plain_single_range_is_natural_beyond_maxppem) {
    RR_FIX3(x, &rr_gasps[9]);
    EXPECT_EQ(nat(&x, 12, 96, 0), 4); EXPECT_EQ(nat(&x, 50, 96, 0), 4); EXPECT_EQ(nat(&x, 90, 96, 0), 4);
    rr_close(&x);
}
RT(gasp_literal_duplicate_maxppem_range_ignored) {
    RR_FIX3(x, &rr_gasps[10]);         /* {10: 8}, {10: 0}, {65535: 0} -> the second range is skipped, flag 8 up to 10 */
    EXPECT_EQ(nat(&x, 10, 96, 0), 5); EXPECT_EQ(nat(&x, 11, 96, 0), 4); EXPECT_EQ(nat(&x, 5, 96, 0), 5);
    rr_close(&x);
}
RT(gasp_literal_descending_range_ignored) {
    RR_FIX3(x, &rr_gasps[11]);         /* {20: 8}, {10: 0}, {65535: 0} */
    EXPECT_EQ(nat(&x, 15, 96, 0), 5); EXPECT_EQ(nat(&x, 20, 96, 0), 5); EXPECT_EQ(nat(&x, 21, 96, 0), 4); EXPECT_EQ(nat(&x, 80, 96, 0), 4);
    rr_close(&x);
}
RT(gasp_literal_alternating) {
    RR_FIX3(x, &rr_gasps[7]);          /* {6: 8}, {10: 0}, {14: 8}, {65535: 0} */
    static const int want[] = { 5,5,5,5,5,5, 4,4,4,4, 5,5,5,5, 4,4,4,4,4,4 };
    int p;
    for (p = 1; p <= 20; p++) { int m = nat(&x, (float)p, 96, 0); t_check(m == want[p - 1], __FILE__, __LINE__, "alternating", "ppem %d -> %d want %d", p, m, want[p - 1]); }
    rr_close(&x);
}
RT(gasp_literal_fine_toggles) {
    RR_FIX3(x, &rr_gasps[12]);         /* {1: 8}, {2: 0}, {3: 8}, {4: 0}, {65535: 8} */
    static const int want[] = { 5, 4, 5, 4, 5, 5, 5, 5, 5, 5 };
    int p;
    for (p = 1; p <= 10; p++) { int m = nat(&x, (float)p, 96, 0); t_check(m == want[p - 1], __FILE__, __LINE__, "fine toggles", "ppem %d -> %d want %d", p, m, want[p - 1]); }
    rr_close(&x);
}
RT(gasp_literal_grid_only_flag_leaves_natural) {
    RR_FIX3(x, &rr_gasps[14]);         /* {65535: 4}: flag 8 never set */
    EXPECT_EQ(nat(&x, 12, 96, 0), 4); EXPECT_EQ(nat(&x, 40, 96, 0), 4);
    rr_close(&x);
}
RT(gasp_outline_and_gdi_ignore_gasp) {
    int k;
    for (k = 0; k < rr_ngasps; k++) {
        RR_FIX3(x, &rr_gasps[k]);
        rr_in in = rr_mk(12, 96, 0, 2); int m, g;
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(m == 3, __FILE__, __LINE__, "gdi natural", "%s: %d", rr_gasps[k].name, m);
        in = rr_mk(150, 96, 0, 0); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(m == 6, __FILE__, __LINE__, "outline", "%s: %d", rr_gasps[k].name, m);
        in = rr_mk(12, 96, 0, 1); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(m == 1 || m == 2, __FILE__, __LINE__, "classic", "%s: %d", rr_gasps[k].name, m);
        rr_close(&x);
    }
}
RT(gasp_downsample_only_for_symmetric_result) {
    int k;
    for (k = 0; k < rr_ngasps; k++) {
        RR_FIX3(x, &rr_gasps[k]);
        int p;
        for (p = 5; p <= 40; p += 5) {
            float em = (float)p * 96.0f / 288.0f; rr_in in = rr_mk(em, 288, 0, 0); int m3, m2, g, flag = rr_gasp_flag(&rr_gasps[k], 8, p, 1);
            CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m3, &g)); CHECK_HR_OK(rr_call2(&x, &in, x.dp, &m2, &g));
            t_check(m3 == (flag ? 7 : 4) && m2 == (flag ? 5 : 4), __FILE__, __LINE__, "downsample follows symmetric", "%s ppem %d: f3 %d f2 %d", rr_gasps[k].name, p, m3, m2);
        }
        rr_close(&x);
    }
}
RT(gasp_font_without_gasp_vs_all_flags_agree_above_20) {
    rr_font a, b;
    if (!rr_open(&a, NULL) || !rr_open(&b, &rr_gasps[0]) || !a.f3 || !b.f3) { t_skip("font"); rr_close(&a); rr_close(&b); return; }
    int p;
    for (p = 21; p <= 99; p += 7) {
        rr_in in = rr_mk((float)p, 96, 0, 0); int ma, mb, g;
        CHECK_HR_OK(rr_call3(&a, &in, a.dp, &ma, &g)); CHECK_HR_OK(rr_call3(&b, &in, b.dp, &mb, &g)); EXPECT_EQ(ma, 5); EXPECT_EQ(mb, 5);
    }
    rr_close(&a); rr_close(&b);
}
RT(gasp_two_faces_do_not_share_ranges) {
    rr_font a, b;
    if (!rr_open(&a, &rr_gasps[0]) || !rr_open(&b, &rr_gasps[1]) || !a.f3 || !b.f3) { t_skip("font"); rr_close(&a); rr_close(&b); return; }
    EXPECT_EQ(nat(&a, 12, 96, 0), 5); EXPECT_EQ(nat(&b, 12, 96, 0), 4); EXPECT_EQ(nat(&a, 12, 96, 0), 5);
    rr_close(&a); rr_close(&b);
}
RT(gasp_face_from_shared_and_isolated_factory_agree) {
    rr_font a;
    if (!rr_open(&a, &rr_gasps[4]) || !a.f3) { t_skip("font"); rr_close(&a); return; }
    {
        ttf_spec s; size_t n; uint8_t *blob; dwx_font h; IDWriteFontFace3 *f3 = NULL; IDWriteFactory *iso = dw_isolated();
        ttf_default_spec(&s); s.gasp = rr_gasps[4].r; s.ngasp = rr_gasps[4].n;
        blob = ttf_build(&s, &n);
        if (iso && blob && SUCCEEDED(dwx_font_open(&h, iso, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE))) {
            rr_font b; memset(&b, 0, sizeof b); b.f3 = NULL;
            if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace3, (void **)&f3))) {
                int p;
                for (p = 6; p <= 20; p++) {
                    rr_in in = rr_mk((float)p, 96, 1, 0); int ma, mb, g;
                    DWRITE_RENDERING_MODE1 mm = (DWRITE_RENDERING_MODE1)RR_SENT; DWRITE_GRID_FIT_MODE gg;
                    CHECK_HR_OK(rr_call3(&a, &in, a.dp, &ma, &g));
                    CHECK_HR_OK((f3->lpVtbl->IDWriteFontFace3_GetRecommendedRenderingMode)(f3, in.em, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, &mm, &gg));
                    mb = (int)mm; EXPECT_EQ(ma, mb);
                }
                IDWriteFontFace3_Release(f3);
            }
        }
        free(blob); dwx_font_close(&h);
    }
    rr_close(&a);
}
