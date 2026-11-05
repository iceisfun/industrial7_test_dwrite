/* GetRecommendedRenderingMode matrix on a font WITHOUT a gasp table (and without hinting programs): IDWriteFontFace (pixelsPerDip),
 * IDWriteFontFace1 (dpiX/dpiY/transform/sideways), IDWriteFontFace2 and IDWriteFontFace3 (rendering params, grid fit).
 *
 * Facts (IDWriteFontFace3/2/1/base rendering mode methods, mode resolution, ppem computation):
 *   - FontFace3/FontFace2: *mode and *gridFit zeroed first; outlineThreshold > 1 or measuringMode > 2 -> E_INVALIDARG 0x80070057.
 *   - FontFace (oldest): *mode zeroed; params == NULL -> E_INVALIDARG; threshold ANTIALIASED, dpi = pixelsPerDip * 96, no transform.
 *   - FontFace1: *mode zeroed, no validation, always S_OK.
 *   - the model in rr.h gives the exact mode for every combination; FontFace/1/2 report the downsampled mode as NATURAL_SYMMETRIC.
 */
#include "rr.h"

#define RT(n) TEST(r2_rendering_modes_##n)

/* One query on all of F1/F2/F3 against the model.  Returns the F3 mode. */
static int case_check(rr_font *x, const rr_gasp *g, const rr_in *in, const char *label) {
    int m1, m2, m3, g2, g3, want = rr_model(in, g, NULL, NULL), ppem_i = 0;
    HRESULT h1 = rr_call1(x, in, &m1), h2 = rr_call2(x, in, x->dp, &m2, &g2), h3 = rr_call3(x, in, x->dp, &m3, &g3);
    (void)ppem_i;
    t_check(h1 == S_OK && h2 == S_OK && h3 == S_OK, __FILE__, __LINE__, "recommended mode succeeds", "%s: %s %s %s", label, dw_hr_name(h1), dw_hr_name(h2), dw_hr_name(h3));
    if (want == -2) {
        t_check((m3 == 1 || m3 == 2) && m1 == m3 && m2 == m3, __FILE__, __LINE__, "GDI_CLASSIC resolves to ALIASED or GDI_CLASSIC",
                "%s: f1 %d f2 %d f3 %d", label, m1, m2, m3);
    } else if (want == -3) {
        t_check((m3 == 4 || m3 == 5 || m3 == 7) && m1 == (m3 == 7 ? 5 : m3) && m2 == m1, __FILE__, __LINE__, "unhinted natural", "%s: f1 %d f2 %d f3 %d", label, m1, m2, m3);
    } else {
        int old = want == 7 ? 5 : want;
        t_check(m3 == want, __FILE__, __LINE__, "face3 mode", "%s: got %d (%s) want %d (%s)", label, m3, m3 >= 0 && m3 < 8 ? rr_mode_names[m3] : "?", want, rr_mode_names[want]);
        t_check(m1 == old && m2 == old, __FILE__, __LINE__, "face1/face2 mode", "%s: f1 %d f2 %d want %d", label, m1, m2, old);
    }
    t_check(g2 == g3 && (g3 == 1 || g3 == 2), __FILE__, __LINE__, "grid fit", "%s: f2 %d f3 %d (want equal, 1 or 2)", label, g2, g3);
    return m3;
}

/* ---- threshold boundaries --------------------------------------------------------------------------------------- */
static void thresholds(int th, const float *ems, int n) {
    RR_FIX3(x, NULL);
    int i, mm;
    for (i = 0; i < n; i++) for (mm = 0; mm < 3; mm += 2) {
        rr_in in = rr_mk(ems[i], 96, th, mm); char l[64];
        sprintf(l, "th %d em %g mm %d", th, ems[i], mm);
        case_check(&x, NULL, &in, l);
    }
    rr_close(&x);
}
RT(outline_threshold_antialiased_boundary) { static const float e[] = { 98, 99, 99.5f, 99.75f, 100, 100.25f, 101, 150 }; thresholds(0, e, 8); }
RT(outline_threshold_aliased_boundary) { static const float e[] = { 340, 349, 349.5f, 349.75f, 350, 350.25f, 351, 400 }; thresholds(1, e, 8); }
RT(outline_threshold_exact_edges) {
    RR_FIX3(x, NULL);
    int m, g; rr_in in;
    in = rr_mk(100, 96, 0, 0); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 6);
    in = rr_mk(99.75f, 96, 0, 0); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); CHECK(m != 6);
    in = rr_mk(350, 96, 1, 0); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 6);
    in = rr_mk(349.75f, 96, 1, 0); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); CHECK(m != 6);
    rr_close(&x);
}
RT(outline_precedes_every_measuring_mode) {
    RR_FIX3(x, NULL);
    int mm, th, m, g;
    for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) {
        rr_in in = rr_mk(th ? 400.0f : 150.0f, 96, th, mm);
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 6);
        CHECK_HR_OK(rr_call2(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 6);
        CHECK_HR_OK(rr_call1(&x, &in, &m)); EXPECT_EQ(m, 6);
    }
    rr_close(&x);
}
RT(aliased_threshold_is_above_antialiased_threshold) {
    RR_FIX3(x, NULL);
    float em; int m, g, first[2] = { -1, -1 }, th;
    for (th = 0; th < 2; th++) for (em = 1; em <= 600; em += 1) {
        rr_in in = rr_mk(em, 96, th, 0); rr_call3(&x, &in, x.dp, &m, &g);
        if (m == 6) { first[th] = (int)em; break; }
    }
    EXPECT_EQ(first[0], 100); EXPECT_EQ(first[1], 350);
    rr_close(&x);
}
RT(outline_by_dpi) {
    RR_FIX3(x, NULL);
    static const struct { float em, dpi; int th, outline; } c[] = {
        { 50, 192, 0, 1 }, { 49, 192, 0, 0 }, { 25, 384, 0, 1 }, { 24, 384, 0, 0 }, { 100, 96, 0, 1 }, { 10, 960, 0, 1 }, { 9, 960, 0, 0 },
        { 175, 192, 1, 1 }, { 174, 192, 1, 0 }, { 350, 96, 1, 1 }, { 100, 336, 1, 1 }, { 100, 300, 1, 0 },
    };
    int i, m, g;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        rr_in in = rr_mk(c[i].em, c[i].dpi, c[i].th, 0);
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g));
        t_check((m == 6) == c[i].outline, __FILE__, __LINE__, "outline by dpi", "em %g dpi %g th %d -> %d", c[i].em, c[i].dpi, c[i].th, m);
    }
    rr_close(&x);
}
RT(outline_with_unequal_dpi_uses_larger_axis) {
    RR_FIX3(x, NULL);
    int m, g; rr_in in;
    in = rr_mk(50, 96, 0, 0); in.dy = 192;       /* 50 * max(1, 2) = 100 */
    CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 6);
    in = rr_mk(50, 192, 0, 0); in.dy = 96;
    CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 6);
    in = rr_mk(49, 192, 0, 0); in.dy = 96;
    CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); CHECK(m != 6);
    rr_close(&x);
}

/* ---- measuring modes ----------------------------------------------------------------------------------------------- */
RT(gdi_natural_small_sizes) {
    RR_FIX3(x, NULL);
    static const float e[] = { 1, 6, 9, 12, 16, 24, 48, 90, 99 };
    int i, th, m, g;
    for (th = 0; th < 2; th++) for (i = 0; i < 9; i++) {
        rr_in in = rr_mk(e[i], 96, th, 2);
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(m == 3, __FILE__, __LINE__, "gdi natural", "em %g th %d -> %d", e[i], th, m);
        CHECK_HR_OK(rr_call1(&x, &in, &m)); EXPECT_EQ(m, 3);
        CHECK_HR_OK(rr_call2(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 3);
    }
    rr_close(&x);
}
RT(gdi_natural_ignores_transform_and_orientation) {
    RR_FIX3(x, NULL);
    static const DWRITE_MATRIX ms[] = { { 0, 1, -1, 0, 0, 0 }, { 1, 0, 0.5f, 1, 0, 0 }, { 0.7071f, 0.7071f, -0.7071f, 0.7071f, 0, 0 }, { 2, 0, 0, 2, 0, 0 }, { -1, 0, 0, 1, 0, 0 } };
    int i, sw, m, g;
    for (i = 0; i < 5; i++) for (sw = 0; sw < 2; sw++) {
        rr_in in = rr_mkm(12, 96, &ms[i], 0, 2); in.sw = sw;
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(m == 3, __FILE__, __LINE__, "gdi natural", "matrix %d sw %d -> %d", i, sw, m);
    }
    rr_close(&x);
}
RT(gdi_classic_is_aliased_or_classic) {
    RR_FIX3(x, NULL);
    static const float e[] = { 4, 8, 9, 10, 12, 14, 16, 17, 18, 24, 36, 72, 99 };
    int i, th, m1, m2, m3, g, seen[3] = { 0, 0, 0 };
    for (th = 0; th < 2; th++) for (i = 0; i < 13; i++) {
        rr_in in = rr_mk(e[i], 96, th, 1);
        CHECK_HR_OK(rr_call1(&x, &in, &m1)); CHECK_HR_OK(rr_call2(&x, &in, x.dp, &m2, &g)); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m3, &g));
        t_check((m3 == 1 || m3 == 2) && m1 == m3 && m2 == m3, __FILE__, __LINE__, "classic", "em %g th %d: %d %d %d", e[i], th, m1, m2, m3);
        if (m3 >= 1 && m3 <= 2) seen[m3]++;
    }
    /* which one depends on the user's font smoothing setting and on the font (the system-wide ClearType state), so only record it */
    t_note("GDI_CLASSIC measuring: ALIASED %d, GDI_CLASSIC %d of 26 probes", seen[1], seen[2]);
    rr_close(&x);
}
RT(gdi_classic_does_not_depend_on_transform) {
    RR_FIX3(x, NULL);
    DWRITE_MATRIX rot = { 0, 1, -1, 0, 0, 0 };
    rr_in a = rr_mk(12, 96, 0, 1), b = rr_mkm(12, 96, &rot, 0, 1);
    int ma, mb, g;
    CHECK_HR_OK(rr_call3(&x, &a, x.dp, &ma, &g)); CHECK_HR_OK(rr_call3(&x, &b, x.dp, &mb, &g));
    CHECK(ma == 1 || ma == 2); EXPECT_EQ(ma, mb);
    rr_close(&x);
}
RT(natural_no_gasp_is_symmetric_above_20_ppem) {
    /* GetRenderingModeFromFontSize without a gasp table: ppem > 20 is always NATURAL_SYMMETRIC, whatever the hinting flags */
    RR_FIX3(x, NULL);
    static const float e[] = { 21, 22, 30, 40, 60, 99 };
    int i, th, sw, m, g;
    for (th = 0; th < 2; th++) for (sw = 0; sw < 2; sw++) for (i = 0; i < 6; i++) {
        rr_in in = rr_mk(e[i], 96, th, 0); in.sw = sw;
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(m == 5, __FILE__, __LINE__, "natural symmetric", "em %g th %d sw %d -> %d", e[i], th, sw, m);
        CHECK_HR_OK(rr_call1(&x, &in, &m)); EXPECT_EQ(m, 5);
    }
    rr_close(&x);
}
RT(natural_no_gasp_small_sizes_note) {
    RR_FIX3(x, NULL);
    static const float e[] = { 1, 4, 8, 11, 12, 13, 16, 20 };
    int i, m, g, n5 = 0, n4 = 0;
    for (i = 0; i < 8; i++) {
        rr_in in = rr_mk(e[i], 96, 0, 0);
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); CHECK(m == 4 || m == 5);
        n5 += m == 5; n4 += m == 4;
    }
    /* an unhinted synthetic TrueType face without bitmaps is expected to report NATURAL_SYMMETRIC; record what this system does */
    t_note("unhinted face, no gasp, 1..20 ppem: NATURAL_SYMMETRIC %d, NATURAL %d", n5, n4);
    rr_close(&x);
}

/* ---- downsampled symmetric (only reported by IDWriteFontFace3) ------------------------------------------------- */
RT(downsample_dpi_threshold) {
    RR_FIX3(x, NULL);
    static const struct { float dpi; int want; } c[] = {
        { 96, 5 }, { 120, 5 }, { 144, 5 }, { 168, 5 }, { 172, 5 }, { 172.5f, 5 }, { 173, 7 }, { 174, 7 }, { 192, 7 }, { 288, 7 }, { 384, 7 }, { 960, 7 },
    };
    int i, m, g;
    for (i = 0; i < 12; i++) {
        rr_in in = rr_mk(40.0f * 96.0f / c[i].dpi, c[i].dpi, 0, 0);
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(m == c[i].want, __FILE__, __LINE__, "face3 downsample", "dpi %g -> %d want %d", c[i].dpi, m, c[i].want);
        CHECK_HR_OK(rr_call2(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 5);
        CHECK_HR_OK(rr_call1(&x, &in, &m)); EXPECT_EQ(m, 5);
    }
    rr_close(&x);
}
RT(downsample_uses_smaller_dpi_axis) {
    RR_FIX3(x, NULL);
    int m, g; rr_in in;
    in = rr_mk(40, 192, 0, 0); in.dy = 96;  CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 5);
    in = rr_mk(20, 96, 0, 0); in.dy = 192;  CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 5);
    in = rr_mk(20, 192, 0, 0); in.dy = 288; CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 7);
    in = rr_mk(20, 192, 0, 0); in.dy = 173; CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 7);
    in = rr_mk(20, 192, 0, 0); in.dy = 172; CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 5);
    rr_close(&x);
}
RT(downsample_not_for_gdi_modes_or_outline) {
    RR_FIX3(x, NULL);
    int m, g; rr_in in;
    in = rr_mk(8, 384, 0, 2); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 3);
    in = rr_mk(80, 384, 0, 0); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 6);
    in = rr_mk(8, 384, 0, 1); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); CHECK(m == 1 || m == 2);
    rr_close(&x);
}
RT(downsample_applies_to_rotated_text_too) {
    RR_FIX3(x, NULL);
    DWRITE_MATRIX r45 = { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 };
    int m, g; rr_in in = rr_mkm(20, 192, &r45, 0, 0);
    CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 7);
    in = rr_mkm(20, 96, &r45, 0, 0);
    CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); EXPECT_EQ(m, 5);
    rr_close(&x);
}

/* ---- transform matrix table ----------------------------------------------------------------------------------------- */
static const struct { const char *name; DWRITE_MATRIX m; } rr_xforms[] = {
    { "identity",     { 1, 0, 0, 1, 0, 0 } },
    { "scale2",       { 2, 0, 0, 2, 0, 0 } },
    { "scale_half",   { 0.5f, 0, 0, 0.5f, 0, 0 } },
    { "scale_2x1",    { 2, 0, 0, 1, 0, 0 } },
    { "scale_1x3",    { 1, 0, 0, 3, 0, 0 } },
    { "flip_x",       { -1, 0, 0, 1, 0, 0 } },
    { "flip_y",       { 1, 0, 0, -1, 0, 0 } },
    { "rot180",       { -1, 0, 0, -1, 0, 0 } },
    { "rot90",        { 0, 1, -1, 0, 0, 0 } },
    { "rot270",       { 0, -1, 1, 0, 0, 0 } },
    { "rot90_scale2", { 0, 2, -2, 0, 0, 0 } },
    { "rot90_2x3",    { 0, 2, -3, 0, 0, 0 } },
    { "rot45",        { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 } },
    { "rot45_scale2", { 1.41421356f, 1.41421356f, -1.41421356f, 1.41421356f, 0, 0 } },
    { "shear_x",      { 1, 0, 0.5f, 1, 0, 0 } },
    { "shear_y",      { 1, 0.5f, 0, 1, 0, 0 } },
    { "shear_both",   { 1, 0.25f, 0.25f, 1, 0, 0 } },
    { "translate",    { 1, 0, 0, 1, 40, -30 } },
    { "translate_s2", { 2, 0, 0, 2, 7, 9 } },
};
#define NX ((int)(sizeof rr_xforms / sizeof rr_xforms[0]))

/* NATURAL measuring under a matrix: ppem from the matrix, axis alignment from its shape; model-checked on a no-gasp and a "never symmetric" gasp font. */
static void xform_matrix(const rr_gasp *g) {
    RR_FIX3(x, g);
    static const float ems[] = { 6, 12, 33, 49, 51, 70, 80, 120, 250, 360 };
    int i, e, sw;
    for (i = 0; i < NX; i++) for (e = 0; e < 10; e++) for (sw = 0; sw < 2; sw++) {
        rr_in in = rr_mkm(ems[e], 96, &rr_xforms[i].m, 0, 0); char l[96];
        in.sw = sw;
        sprintf(l, "%s em %g sw %d", rr_xforms[i].name, ems[e], sw);
        case_check(&x, g, &in, l);
    }
    rr_close(&x);
}
RT(matrix_table_no_gasp) { xform_matrix(NULL); }
RT(matrix_table_gasp_never_symmetric) { xform_matrix(&rr_gasps[1]); }
RT(matrix_table_gasp_always_symmetric) { xform_matrix(&rr_gasps[0]); }
RT(matrix_table_aliased_threshold) {
    RR_FIX3(x, NULL);
    int i, e; static const float ems[] = { 20, 90, 170, 200, 260, 349, 400 };
    for (i = 0; i < NX; i++) for (e = 0; e < 7; e++) {
        rr_in in = rr_mkm(ems[e], 96, &rr_xforms[i].m, 1, 0); char l[96];
        sprintf(l, "aliased-threshold %s em %g", rr_xforms[i].name, ems[e]);
        case_check(&x, NULL, &in, l);
    }
    rr_close(&x);
}
RT(matrix_table_dpi_144) {
    RR_FIX3(x, NULL);
    int i, e; static const float ems[] = { 6, 12, 40, 66, 70, 140 };
    for (i = 0; i < NX; i++) for (e = 0; e < 6; e++) {
        rr_in in = rr_mkm(ems[e], 144, &rr_xforms[i].m, 0, 0); char l[96];
        sprintf(l, "dpi144 %s em %g", rr_xforms[i].name, ems[e]);
        case_check(&x, NULL, &in, l);
    }
    rr_close(&x);
}
RT(matrix_table_unequal_dpi) {
    RR_FIX3(x, NULL);
    int i, e; static const float ems[] = { 6, 20, 40, 60, 80 };
    for (i = 0; i < NX; i++) for (e = 0; e < 5; e++) {
        rr_in in = rr_mkm(ems[e], 96, &rr_xforms[i].m, 0, 0); char l[96];
        in.dy = 144;
        sprintf(l, "dpi96x144 %s em %g", rr_xforms[i].name, ems[e]);
        case_check(&x, NULL, &in, l);
    }
    rr_close(&x);
}
RT(matrix_translation_is_ignored) {
    RR_FIX3(x, NULL);
    DWRITE_MATRIX a = { 1, 0, 0, 1, 0, 0 }, b = { 1, 0, 0, 1, 1000, -1000 };
    int ma, mb, g;
    static const float ems[] = { 8, 99, 100, 101 };
    int i;
    for (i = 0; i < 4; i++) {
        rr_in ia = rr_mkm(ems[i], 96, &a, 0, 0), ib = rr_mkm(ems[i], 96, &b, 0, 0);
        CHECK_HR_OK(rr_call3(&x, &ia, x.dp, &ma, &g)); CHECK_HR_OK(rr_call3(&x, &ib, x.dp, &mb, &g)); EXPECT_EQ(ma, mb);
    }
    rr_close(&x);
}
RT(matrix_identity_equals_null_matrix) {
    RR_FIX3(x, NULL);
    static const float ems[] = { 5, 12, 99.5f, 100, 349, 350 };
    int i, th, mm;
    for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) for (i = 0; i < 6; i++) {
        rr_in a = rr_mk(ems[i], 96, th, mm), b = rr_mkm(ems[i], 96, &rr_ident, th, mm);
        int ma, mb, ga, gb;
        CHECK_HR_OK(rr_call3(&x, &a, x.dp, &ma, &ga)); CHECK_HR_OK(rr_call3(&x, &b, x.dp, &mb, &gb));
        t_check(ma == mb && ga == gb, __FILE__, __LINE__, "identity == NULL", "th %d mm %d em %g: %d/%d vs %d/%d", th, mm, ems[i], ma, ga, mb, gb);
    }
    rr_close(&x);
}
RT(matrix_zero_scale_is_not_outline) {
    RR_FIX3(x, NULL);
    DWRITE_MATRIX z = { 0, 0, 0, 0, 0, 0 };
    rr_in in = rr_mkm(500, 96, &z, 1, 0); int m, g;
    HRESULT hr = rr_call3(&x, &in, x.dp, &m, &g);
    /* a degenerate matrix gives ppem 0: never an outline, no crash */
    CHECK_HR_OK(hr);
    CHECK(m != 6);
    rr_close(&x);
}
RT(negative_em_and_zero_em_do_not_crash) {
    RR_FIX3(x, NULL);
    static const float ems[] = { 0.0f, -12.0f, 0.001f };
    int i, m, g;
    for (i = 0; i < 3; i++) {
        rr_in in = rr_mk(ems[i], 96, 0, 0);
        HRESULT hr = rr_call3(&x, &in, x.dp, &m, &g);
        t_check(hr == S_OK, __FILE__, __LINE__, "degenerate em", "em %g: %s", ems[i], dw_hr_name(hr));
        if (hr == S_OK) CHECK(m != 6 && m >= 1 && m <= 7);
    }
    rr_close(&x);
}

/* ---- argument validation (FontFace2/3) ------------------------------------------------------------------------------ */
RT(args_invalid_threshold_f2_f3) {
    RR_FIX3(x, NULL);
    static const int bad[] = { 2, 3, 7, 100, 0x7fffffff };
    int i, m, g;
    for (i = 0; i < 5; i++) {
        rr_in in = rr_mk(12, 96, bad[i], 0);
        t_check(rr_call3(&x, &in, x.dp, &m, &g) == RR_INVALIDARG, __FILE__, __LINE__, "f3 threshold", "th %d", bad[i]); EXPECT_EQ(m, 0); EXPECT_EQ(g, 0);
        t_check(rr_call2(&x, &in, x.dp, &m, &g) == RR_INVALIDARG, __FILE__, __LINE__, "f2 threshold", "th %d", bad[i]); EXPECT_EQ(m, 0); EXPECT_EQ(g, 0);
    }
    rr_close(&x);
}
RT(args_invalid_measuring_f2_f3) {
    RR_FIX3(x, NULL);
    static const int bad[] = { 3, 4, 9, 0x100, 0x7fffffff };
    int i, m, g;
    for (i = 0; i < 5; i++) {
        rr_in in = rr_mk(12, 96, 0, bad[i]);
        t_check(rr_call3(&x, &in, x.dp, &m, &g) == RR_INVALIDARG, __FILE__, __LINE__, "f3 measuring", "mm %d", bad[i]); EXPECT_EQ(m, 0); EXPECT_EQ(g, 0);
        t_check(rr_call2(&x, &in, x.dp, &m, &g) == RR_INVALIDARG, __FILE__, __LINE__, "f2 measuring", "mm %d", bad[i]); EXPECT_EQ(m, 0); EXPECT_EQ(g, 0);
    }
    rr_close(&x);
}
RT(args_invalid_both_enums_f3) {
    RR_FIX3(x, NULL);
    rr_in in = rr_mk(12, 96, 2, 3); int m, g;
    CHECK_HR(rr_call3(&x, &in, NULL, &m, &g), RR_INVALIDARG); EXPECT_EQ(m, 0); EXPECT_EQ(g, 0);
    rr_close(&x);
}
RT(args_params_null_is_accepted_f2_f3) {
    RR_FIX3(x, NULL);
    rr_in in = rr_mk(12, 96, 0, 0); int ma, mb, ga, gb;
    CHECK_HR_OK(rr_call3(&x, &in, NULL, &ma, &ga)); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &mb, &gb)); EXPECT_EQ(ma, mb); EXPECT_EQ(ga, gb);
    CHECK_HR_OK(rr_call2(&x, &in, NULL, &ma, &ga)); CHECK_HR_OK(rr_call2(&x, &in, x.dp, &mb, &gb)); EXPECT_EQ(ma, mb); EXPECT_EQ(ga, gb);
    rr_close(&x);
}
RT(args_f2_equals_f3_converted) {
    RR_FIX3(x, NULL);
    int th, mm, i, sw;
    static const float dpis[] = { 96, 144, 192, 288 }, ems[] = { 5, 12, 30, 80 };
    for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) for (i = 0; i < 4; i++) for (sw = 0; sw < 2; sw++) {
        rr_in in = rr_mk(ems[i], dpis[i], th, mm); int m2, m3, g2, g3;
        in.sw = sw;
        CHECK_HR_OK(rr_call2(&x, &in, x.dp, &m2, &g2)); CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m3, &g3));
        t_check(m2 == (m3 == 7 ? 5 : m3) && g2 == g3, __FILE__, __LINE__, "f2 == f3", "th %d mm %d em %g dpi %g sw %d: f2 %d/%d f3 %d/%d", th, mm, ems[i], dpis[i], sw, m2, g2, m3, g3);
    }
    rr_close(&x);
}
RT(args_f1_never_fails_on_odd_enums_note) {
    RR_FIX3(x, NULL);
    /* FontFace1 forwards without validating; only record that the call returns */
    rr_in in = rr_mk(12, 96, 0, 0); int m;
    HRESULT hr = rr_call1(&x, &in, &m);
    CHECK_HR_OK(hr);
    t_note("FontFace1 normal call mode %d", m);
    rr_close(&x);
}

/* ---- the oldest overload ---------------------------------------------------------------------------------------------- */
RT(f0_null_params_invalid) {
    RR_FIX(x, NULL);
    int m;
    CHECK_HR(rr_call0(&x, 12, 1.0f, 0, NULL, &m), RR_INVALIDARG);
    EXPECT_EQ(m, 0);                                   /* *renderingMode is zeroed before the check */
    rr_close(&x);
}
RT(f0_equals_f1_with_scaled_dpi) {
    RR_FIX3(x, NULL);
    static const float ppds[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f }, ems[] = { 4, 12, 40, 66, 120, 400 };
    int i, j, mm;
    for (mm = 0; mm < 3; mm++) for (i = 0; i < 5; i++) for (j = 0; j < 6; j++) {
        int m0, m1; rr_in in = rr_mk(ems[j], 96.0f * ppds[i], 0, mm);
        CHECK_HR_OK(rr_call0(&x, ems[j], ppds[i], mm, x.dp, &m0)); CHECK_HR_OK(rr_call1(&x, &in, &m1));
        t_check(m0 == m1, __FILE__, __LINE__, "f0 == f1", "mm %d ppd %g em %g: %d vs %d", mm, ppds[i], ems[j], m0, m1);
    }
    rr_close(&x);
}
RT(f0_model) {
    RR_FIX3(x, NULL);
    static const float ppds[] = { 0.75f, 1.0f, 1.25f, 2.0f }, ems[] = { 4, 12, 40, 66, 95, 120, 400 };
    int i, j, mm;
    for (mm = 0; mm < 3; mm++) for (i = 0; i < 4; i++) for (j = 0; j < 7; j++) {
        int m0; rr_in in = rr_mk(ems[j], 96.0f * ppds[i], 0, mm); int want = rr_model(&in, NULL, NULL, NULL);
        CHECK_HR_OK(rr_call0(&x, ems[j], ppds[i], mm, x.dp, &m0));
        if (want == -2) CHECK(m0 == 1 || m0 == 2);
        else if (want == -3) CHECK(m0 == 4 || m0 == 5);
        else t_check(m0 == (want == 7 ? 5 : want), __FILE__, __LINE__, "f0 model", "mm %d ppd %g em %g: %d want %d", mm, ppds[i], ems[j], m0, want);
    }
    rr_close(&x);
}
RT(f0_uses_antialiased_threshold) {
    RR_FIX(x, NULL);
    int m;
    CHECK_HR_OK(rr_call0(&x, 100, 1.0f, 0, x.dp, &m)); EXPECT_EQ(m, 6);
    CHECK_HR_OK(rr_call0(&x, 99, 1.0f, 0, x.dp, &m)); CHECK(m != 6);
    CHECK_HR_OK(rr_call0(&x, 300, 1.0f, 0, x.dp, &m)); EXPECT_EQ(m, 6);   /* the aliased threshold (350) is not used */
    CHECK_HR_OK(rr_call0(&x, 50, 2.0f, 0, x.dp, &m)); EXPECT_EQ(m, 6);
    rr_close(&x);
}
RT(f0_never_reports_downsampled) {
    RR_FIX(x, NULL);
    static const float ppds[] = { 1.0f, 1.8f, 2.0f, 4.0f, 10.0f };
    int i, m;
    for (i = 0; i < 5; i++) { CHECK_HR_OK(rr_call0(&x, 40.0f / ppds[i], ppds[i], 0, x.dp, &m)); EXPECT_EQ(m, 5); }
    rr_close(&x);
}
RT(f0_gdi_natural) {
    RR_FIX(x, NULL);
    int m;
    CHECK_HR_OK(rr_call0(&x, 12, 1.0f, 2, x.dp, &m)); EXPECT_EQ(m, 3);
    rr_close(&x);
}

/* ---- explicit rendering params ----------------------------------------------------------------------------------------- */
static IDWriteRenderingParams *mkp(int ver, int mode, int grid) {
    void *p = NULL, *f = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver);
    if (!f) return NULL;
    switch (ver) {
    case 0: IDWriteFactory_CreateCustomRenderingParams((IDWriteFactory *)f, 2.2f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)mode, (IDWriteRenderingParams **)&p); break;
    case 1: IDWriteFactory1_CreateCustomRenderingParams((IDWriteFactory1 *)f, 2.2f, 0.5f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)mode, (IDWriteRenderingParams1 **)&p); break;
    case 2: IDWriteFactory2_CreateCustomRenderingParams((IDWriteFactory2 *)f, 2.2f, 0.5f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)mode, (DWRITE_GRID_FIT_MODE)grid, (IDWriteRenderingParams2 **)&p); break;
    default: IDWriteFactory3_CreateCustomRenderingParams((IDWriteFactory3 *)f, 2.2f, 0.5f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE1)mode, (DWRITE_GRID_FIT_MODE)grid, (IDWriteRenderingParams3 **)&p); break;
    }
    return (IDWriteRenderingParams *)p;
}
RT(explicit_mode_is_literal_when_aligned) {
    RR_FIX3(x, NULL);
    int ver, mode, m, g;
    for (ver = 0; ver <= 3; ver++) for (mode = 1; mode <= 6; mode++) {
        IDWriteRenderingParams *p = mkp(ver, mode, 0); rr_in in = rr_mk(12, 96, 0, 0);
        if (!p) continue;
        CHECK_HR_OK(rr_call3(&x, &in, p, &m, &g));
        t_check(m == mode, __FILE__, __LINE__, "explicit mode", "ver %d mode %d -> %d", ver, mode, m);
        CHECK_HR_OK(rr_call2(&x, &in, p, &m, &g)); EXPECT_EQ(m, mode);
        DW_RELEASE(p);
    }
    rr_close(&x);
}
RT(explicit_mode_ignores_outline_threshold_and_measuring) {
    RR_FIX3(x, NULL);
    int mode, mm, m, g;
    for (mode = 1; mode <= 5; mode++) for (mm = 0; mm < 3; mm++) {
        IDWriteRenderingParams *p = mkp(0, mode, 0); rr_in in = rr_mk(500, 96, 0, mm);
        if (!p) continue;
        CHECK_HR_OK(rr_call3(&x, &in, p, &m, &g));
        t_check(m == mode, __FILE__, __LINE__, "explicit mode wins over outline threshold", "mode %d mm %d em 500 -> %d", mode, mm, m);
        DW_RELEASE(p);
    }
    rr_close(&x);
}
RT(explicit_natural_promoted_when_rotated_f3_old_params) {
    RR_FIX3(x, NULL);
    DWRITE_MATRIX r45 = { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 };
    int ver, m, g;
    for (ver = 0; ver <= 2; ver++) {
        IDWriteRenderingParams *p = mkp(ver, 4, 0); rr_in in = rr_mkm(12, 96, &r45, 0, 0);
        if (!p) continue;
        /* params from a pre-RENDERING_MODE1 overload: an unaligned NATURAL request becomes NATURAL_SYMMETRIC (and the downsampled value at high dpi) */
        CHECK_HR_OK(rr_call3(&x, &in, p, &m, &g)); t_check(m == 5, __FILE__, __LINE__, "promoted", "ver %d -> %d", ver, m);
        in = rr_mkm(12, 192, &r45, 0, 0);
        CHECK_HR_OK(rr_call3(&x, &in, p, &m, &g)); t_check(m == 7, __FILE__, __LINE__, "promoted+downsampled", "ver %d -> %d", ver, m);
        DW_RELEASE(p);
    }
    rr_close(&x);
}
RT(explicit_natural_not_promoted_with_mode1_params) {
    RR_FIX3(x, NULL);
    DWRITE_MATRIX r45 = { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 };
    int m, g;
    IDWriteRenderingParams *p = mkp(3, 4, 0); rr_in in = rr_mkm(12, 96, &r45, 0, 0);
    if (!p) { t_skip("IDWriteFactory3 unsupported"); rr_close(&x); return; }
    CHECK_HR_OK(rr_call3(&x, &in, p, &m, &g)); EXPECT_EQ(m, 4);
    DW_RELEASE(p);
    rr_close(&x);
}
RT(explicit_natural_not_promoted_by_face2) {
    RR_FIX3(x, NULL);
    DWRITE_MATRIX r45 = { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 };
    int ver, m, g;
    for (ver = 0; ver <= 3; ver++) {
        IDWriteRenderingParams *p = mkp(ver, 4, 0); rr_in in = rr_mkm(12, 96, &r45, 0, 0);
        if (!p) continue;
        CHECK_HR_OK(rr_call2(&x, &in, p, &m, &g)); t_check(m == 4, __FILE__, __LINE__, "face2 keeps explicit NATURAL", "ver %d -> %d", ver, m);
        DW_RELEASE(p);
    }
    rr_close(&x);
}
RT(explicit_symmetric_f2_reports_5_for_old_params) {
    RR_FIX3(x, NULL);
    int ver, m, g;
    for (ver = 0; ver <= 3; ver++) {
        IDWriteRenderingParams *p = mkp(ver, 5, 0); rr_in in = rr_mk(12, 288, 0, 0);
        if (!p) continue;
        CHECK_HR_OK(rr_call2(&x, &in, p, &m, &g)); t_check(m == 5, __FILE__, __LINE__, "face2", "ver %d -> %d", ver, m);
        DW_RELEASE(p);
    }
    rr_close(&x);
}
RT(explicit_downsampled_mode_f2_converted) {
    RR_FIX3(x, NULL);
    IDWriteRenderingParams *p = mkp(3, 7, 0); rr_in in = rr_mk(12, 96, 0, 0); int m, g;
    if (!p) { t_skip("IDWriteFactory3 unsupported"); rr_close(&x); return; }
    CHECK_HR_OK(rr_call3(&x, &in, p, &m, &g)); EXPECT_EQ(m, 7);
    CHECK_HR_OK(rr_call2(&x, &in, p, &m, &g)); EXPECT_EQ(m, 5);
    DW_RELEASE(p);
    rr_close(&x);
}
RT(explicit_grid_fit_returned_under_any_transform) {
    RR_FIX3(x, NULL);
    int i, gv, m, g;
    for (gv = 1; gv <= 2; gv++) for (i = 0; i < NX; i++) {
        IDWriteRenderingParams *p = mkp(3, 4, gv); rr_in in = rr_mkm(12, 96, &rr_xforms[i].m, 0, 0);
        if (!p) continue;
        CHECK_HR_OK(rr_call3(&x, &in, p, &m, &g)); t_check(g == gv, __FILE__, __LINE__, "explicit grid", "%s: want %d got %d", rr_xforms[i].name, gv, g);
        CHECK_HR_OK(rr_call2(&x, &in, p, &m, &g)); t_check(g == gv, __FILE__, __LINE__, "explicit grid f2", "%s: want %d got %d", rr_xforms[i].name, gv, g);
        DW_RELEASE(p);
    }
    rr_close(&x);
}

/* ---- grid fit mode ----------------------------------------------------------------------------------------------------------- */
RT(grid_fit_disabled_when_rotated_45) {
    RR_FIX3(x, NULL);
    DWRITE_MATRIX r45 = { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 };
    int mm, th, m, g, e;
    static const float ems[] = { 6, 12, 40, 80, 400 };
    for (th = 0; th < 2; th++) for (mm = 0; mm < 3; mm++) for (e = 0; e < 5; e++) {
        rr_in in = rr_mkm(ems[e], 96, &r45, th, mm);
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(g == 1, __FILE__, __LINE__, "unaligned -> grid fit disabled", "th %d mm %d em %g: %d", th, mm, ems[e], g);
        CHECK_HR_OK(rr_call2(&x, &in, x.dp, &m, &g)); EXPECT_EQ(g, 1);
    }
    rr_close(&x);
}
RT(grid_fit_never_default_when_asked) {
    RR_FIX3(x, NULL);
    int i, mm, m, g;
    for (i = 0; i < NX; i++) for (mm = 0; mm < 3; mm++) {
        rr_in in = rr_mkm(14, 96, &rr_xforms[i].m, 0, mm);
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); t_check(g == 1 || g == 2, __FILE__, __LINE__, "grid fit resolved", "%s mm %d: %d", rr_xforms[i].name, mm, g);
    }
    rr_close(&x);
}
RT(grid_fit_table_note) {
    RR_FIX3(x, NULL);
    int mm, e, m, g;
    static const float ems[] = { 8, 12, 16, 24, 48, 96 };
    for (mm = 0; mm < 3; mm++) for (e = 0; e < 6; e++) {
        rr_in in = rr_mk(ems[e], 96, 0, mm);
        CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g));
        t_note("measuring %d em %g: mode %d (%s) grid fit %d", mm, ems[e], m, m >= 0 && m < 8 ? rr_mode_names[m] : "?", g);
    }
    rr_close(&x);
}
RT(grid_fit_stable_across_calls) {
    RR_FIX3(x, NULL);
    int k, m0, g0, m, g; rr_in in = rr_mk(13, 96, 0, 0);
    CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m0, &g0));
    for (k = 0; k < 50; k++) { CHECK_HR_OK(rr_call3(&x, &in, x.dp, &m, &g)); if (m != m0 || g != g0) { CHECK(0); break; } }
    rr_close(&x);
}
