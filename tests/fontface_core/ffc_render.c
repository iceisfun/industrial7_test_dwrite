/* GetGdiCompatibleMetrics (v0 and v1) and GetRecommendedRenderingMode (IDWriteFontFace, IDWriteFontFace1, IDWriteFontFace2, IDWriteFontFace3).
 *
 * GDI-compatible metrics (GetGdiCompatibleMetrics, RoundDesignValueToPixelGrid): with ppem = emSize * pixelsPerDip * scale(transform) and
 * upp = unitsPerEm / ppem design units per pixel, every metric v (except unitsPerEm and hasTypographicMetrics) becomes
 *     round_half_up( round_half_up(v / upp) * upp )
 * in single precision; no VDMX table => ascent and descent follow the same rule. Invalid input (emSize <= 0, pixelsPerDip <= 0, NaN in the matrix)
 * is E_INVALIDARG with the output zeroed first.
 *
 * Rendering mode (ResolveRenderingMode): ppem = emSize * dpi/96 * scale; ppem >= 100 (ANTIALIASED threshold) or >= 350 (ALIASED threshold) gives OUTLINE;
 * measuring GDI_NATURAL gives GDI_NATURAL; GDI_CLASSIC gives ALIASED or GDI_CLASSIC depending on the system smoothing setting; NATURAL gives NATURAL or
 * NATURAL_SYMMETRIC (gasp / hinting dependent, always SYMMETRIC for rotated transforms). IDWriteFontFace1/2 map NATURAL_SYMMETRIC_DOWNSAMPLED (7) to 5. */
#include "ffc.h"

#define E_INVAL ((HRESULT)0x80070057)

/* ---- single precision model of RoundDesignValueToPixelGrid ------------------------------------------------------------------------------ */
static int ffc_rhu(float x) { int i = (int)roundf(x); if ((float)i - x == -0.5f) i++; return i; }
static int ffc_snap(int v, float upp) {
    volatile float f = (float)v / upp;
    float g = f;
    if (fabsf(g) < 8388608.0f) g = (float)ffc_rhu(g);
    { volatile float p = g * upp; return ffc_rhu(p); }
}
static int ffc_snap_u16(int v, float upp) { return (unsigned short)ffc_snap(v, upp); }
static int ffc_snap_s16(int v, float upp) { return (short)ffc_snap(v, upp); }

/* Applies the model to a design-unit metrics block. */
static void ffc_model_gdi(const DWRITE_FONT_METRICS1 *d, float ppem, DWRITE_FONT_METRICS1 *o) {
    float upp = (float)d->designUnitsPerEm / ppem;
    *o = *d;
    if (!(fabsf(ppem) > 1e-6f) || !(fabsf(upp) > 1e-6f)) return;
    o->ascent = (UINT16)ffc_snap_u16(d->ascent, upp); o->descent = (UINT16)ffc_snap_u16(d->descent, upp);
    o->lineGap = (INT16)ffc_snap_s16(d->lineGap, upp);
    o->capHeight = (UINT16)ffc_snap_u16(d->capHeight, upp); o->xHeight = (UINT16)ffc_snap_u16(d->xHeight, upp);
    o->underlinePosition = (INT16)ffc_snap_s16(d->underlinePosition, upp); o->underlineThickness = (UINT16)ffc_snap_u16(d->underlineThickness, upp);
    o->strikethroughPosition = (INT16)ffc_snap_s16(d->strikethroughPosition, upp); o->strikethroughThickness = (UINT16)ffc_snap_u16(d->strikethroughThickness, upp);
    o->glyphBoxLeft = (INT16)ffc_snap_s16(d->glyphBoxLeft, upp); o->glyphBoxTop = (INT16)ffc_snap_s16(d->glyphBoxTop, upp);
    o->glyphBoxRight = (INT16)ffc_snap_s16(d->glyphBoxRight, upp); o->glyphBoxBottom = (INT16)ffc_snap_s16(d->glyphBoxBottom, upp);
    o->subscriptPositionX = (INT16)ffc_snap_s16(d->subscriptPositionX, upp); o->subscriptPositionY = (INT16)ffc_snap_s16(d->subscriptPositionY, upp);
    o->subscriptSizeX = (INT16)ffc_snap_s16(d->subscriptSizeX, upp); o->subscriptSizeY = (INT16)ffc_snap_s16(d->subscriptSizeY, upp);
    o->superscriptPositionX = (INT16)ffc_snap_s16(d->superscriptPositionX, upp); o->superscriptPositionY = (INT16)ffc_snap_s16(d->superscriptPositionY, upp);
    o->superscriptSizeX = (INT16)ffc_snap_s16(d->superscriptSizeX, upp); o->superscriptSizeY = (INT16)ffc_snap_s16(d->superscriptSizeY, upp);
}
static const char *ffc_diff_m1(const DWRITE_FONT_METRICS1 *a, const DWRITE_FONT_METRICS1 *b) {
#define F(n) if (a->n != b->n) return #n;
    F(designUnitsPerEm) F(ascent) F(descent) F(lineGap) F(capHeight) F(xHeight) F(underlinePosition) F(underlineThickness) F(strikethroughPosition)
    F(strikethroughThickness) F(glyphBoxLeft) F(glyphBoxTop) F(glyphBoxRight) F(glyphBoxBottom) F(subscriptPositionX) F(subscriptPositionY) F(subscriptSizeX)
    F(subscriptSizeY) F(superscriptPositionX) F(superscriptPositionY) F(superscriptSizeX) F(superscriptSizeY) F(hasTypographicMetrics)
#undef F
    return NULL;
}

/* design metrics + GDI metrics of one face through IDWriteFontFace1 */
static int ffc_gdi_case(IDWriteFontFace *face, float em, float ppd, const DWRITE_MATRIX *x, float scale, const char *tag) {
    IDWriteFontFace1 *f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); DWRITE_FONT_METRICS1 d, g, w; const char *bad; HRESULT hr;
    if (!f1) { t_skip("IDWriteFontFace1 unsupported"); return 0; }
    IDWriteFontFace1_GetMetrics(f1, &d);
    memset(&g, 0xCC, sizeof g);
    hr = IDWriteFontFace1_GetGdiCompatibleMetrics(f1, em, ppd, x, &g);
    CHECKF(hr == S_OK, "%s: GetGdiCompatibleMetrics(%g,%g) -> %s", tag, em, ppd, dw_hr_name(hr));
    if (hr == S_OK) {
        ffc_model_gdi(&d, em * ppd * scale, &w);
        bad = ffc_diff_m1(&g, &w);
        CHECKF(bad == NULL, "%s: em %g ppd %g scale %g: field %s differs (got asc %u desc %u gap %d cap %u x %u ul %d/%u st %d/%u; want asc %u desc %u gap %d cap %u x %u ul %d/%u st %d/%u)",
               tag, em, ppd, scale, bad ? bad : "", g.ascent, g.descent, g.lineGap, g.capHeight, g.xHeight, g.underlinePosition, g.underlineThickness, g.strikethroughPosition, g.strikethroughThickness,
               w.ascent, w.descent, w.lineGap, w.capHeight, w.xHeight, w.underlinePosition, w.underlineThickness, w.strikethroughPosition, w.strikethroughThickness);
    }
    IDWriteFontFace1_Release(f1);
    return 1;
}

/* ================================ GetGdiCompatibleMetrics ============================================================================ */
FTEST(ffcr_gdi_model_default_font_integer_sizes) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const float em[] = { 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 26, 28, 32, 36, 40, 48, 64, 72, 96, 100, 128, 200 }; int i;
    if (!face) return;
    for (i = 0; i < (int)(sizeof em / sizeof em[0]); i++) if (!ffc_gdi_case(face, em[i], 1, NULL, 1, "default")) break;
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_model_fractional_sizes) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const float em[] = { 8.5f, 9.75f, 10.5f, 11.25f, 12.5f, 13.33f, 15.75f, 17.9f, 23.1f, 0.5f, 1.0f, 2.5f }; int i;
    if (!face) return;
    for (i = 0; i < (int)(sizeof em / sizeof em[0]); i++) if (!ffc_gdi_case(face, em[i], 1, NULL, 1, "fractional")) break;
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_model_pixels_per_dip) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const float ppd[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f, 4.0f }; int i, k; static const float em[] = { 9, 12, 16 };
    if (!face) return;
    for (k = 0; k < 3; k++) for (i = 0; i < 10; i++) if (!ffc_gdi_case(face, em[k], ppd[i], NULL, 1, "ppd")) goto done;
done:
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_model_uniform_scale_transform) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const float sc[] = { 0.5f, 2.0f, 3.0f, 1.5f }; int i; if (!face) return;
    for (i = 0; i < 4; i++) {
        DWRITE_MATRIX m; m.m11 = sc[i]; m.m12 = 0; m.m21 = 0; m.m22 = sc[i]; m.dx = 0; m.dy = 0;
        if (!ffc_gdi_case(face, 8, 1, &m, sc[i], "scale")) break;
    }
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_scale_equivalence) {
    /* emSize 8 with a 2x matrix is the same request as emSize 16; pixelsPerDip multiplies in as well */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace1 *f1; DWRITE_MATRIX m = { 2, 0, 0, 2, 0, 0 }; DWRITE_FONT_METRICS1 a, b, c, d; if (!face) return;
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); if (!f1) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleMetrics(f1, 16, 1, NULL, &a));
    CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleMetrics(f1, 8, 1, &m, &b));
    CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleMetrics(f1, 8, 2, NULL, &c));
    CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleMetrics(f1, 32, 0.5f, NULL, &d));
    CHECK(!memcmp(&a, &b, sizeof a)); CHECK(!memcmp(&a, &c, sizeof a)); CHECK(!memcmp(&a, &d, sizeof a));
    IDWriteFontFace1_Release(f1); dwx_font_close(&h);
}
FTEST(ffcr_gdi_identity_matrix_equals_null) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 }, tr = { 1, 0, 0, 1, 10, -20 }; DWRITE_FONT_METRICS a, b, c; if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, 13, 1, NULL, &a));
    CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, 13, 1, &id, &b));
    CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, 13, 1, &tr, &c));
    CHECK(!memcmp(&a, &b, sizeof a)); CHECK(!memcmp(&a, &c, sizeof a));   /* translation plays no role */
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_v0_equals_v1_prefix) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace1 *f1; DWRITE_FONT_METRICS a; DWRITE_FONT_METRICS1 b; int i; static const float em[] = { 7, 9, 12, 14.5f, 33 };
    if (!face) return;
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); if (!f1) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    for (i = 0; i < 5; i++) {
        CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, em[i], 1, NULL, &a));
        CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleMetrics(f1, em[i], 1, NULL, &b));
        CHECKF(!memcmp(&a, &b, sizeof a), "em %g", em[i]);
    }
    IDWriteFontFace1_Release(f1); dwx_font_close(&h);
}
FTEST(ffcr_gdi_v0_model_all_fields) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_FONT_METRICS d, g; int i; static const float em[] = { 6, 9, 11, 13, 15, 17, 19, 25 };
    if (!face) return;
    IDWriteFontFace_GetMetrics(face, &d);
    for (i = 0; i < 8; i++) {
        float upp = 2048.0f / em[i];
        CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, em[i], 1, NULL, &g));
        EXPECT_EQ(g.designUnitsPerEm, 2048);
        CHECKF(g.ascent == ffc_snap_u16(d.ascent, upp) && g.descent == ffc_snap_u16(d.descent, upp), "em %g asc %u/%d desc %u/%d", em[i], g.ascent, ffc_snap_u16(d.ascent, upp), g.descent, ffc_snap_u16(d.descent, upp));
        CHECKF(g.capHeight == ffc_snap_u16(d.capHeight, upp) && g.xHeight == ffc_snap_u16(d.xHeight, upp), "em %g heights", em[i]);
        CHECKF(g.underlinePosition == ffc_snap_s16(d.underlinePosition, upp) && g.underlineThickness == ffc_snap_u16(d.underlineThickness, upp), "em %g underline", em[i]);
        CHECKF(g.strikethroughPosition == ffc_snap_s16(d.strikethroughPosition, upp) && g.strikethroughThickness == ffc_snap_u16(d.strikethroughThickness, upp), "em %g strike", em[i]);
        CHECKF(g.lineGap == ffc_snap_s16(d.lineGap, upp), "em %g gap", em[i]);
    }
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_pixel_grid_multiples_when_upp_integral) {
    /* 2048 / ppem integral for ppem 8, 16, 32, 64: every rounded value is a whole number of pixels */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const int pp[] = { 8, 16, 32, 64 }; int i; if (!face) return;
    for (i = 0; i < 4; i++) {
        DWRITE_FONT_METRICS g; int upp = 2048 / pp[i];
        CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, (float)pp[i], 1, NULL, &g));
        CHECKF(g.ascent % upp == 0 && g.descent % upp == 0 && g.capHeight % upp == 0 && g.xHeight % upp == 0, "ppem %d: %u %u %u %u (pixel %d units)", pp[i], g.ascent, g.descent, g.capHeight, g.xHeight, upp);
        CHECKF(g.underlineThickness % upp == 0 && g.strikethroughThickness % upp == 0 && g.underlinePosition % upp == 0 && g.strikethroughPosition % upp == 0, "ppem %d lines", pp[i]);
    }
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_exact_values_ppem16) {
    /* upp = 128: ascent 1800/128 = 14.06 -> 14 px = 1792; descent 450/128 = 3.5 -> 4 px = 512 (half rounds up); cap 1400 -> 11 px = 1408; x 1000 -> 8 px = 1024 */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_FONT_METRICS g; if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, 16, 1, NULL, &g));
    EXPECT_EQ(g.ascent, 1792); EXPECT_EQ(g.descent, 512); EXPECT_EQ(g.capHeight, 1408); EXPECT_EQ(g.xHeight, 1024);
    EXPECT_EQ(g.underlinePosition, -256); EXPECT_EQ(g.underlineThickness, 128); EXPECT_EQ(g.strikethroughPosition, 640); EXPECT_EQ(g.strikethroughThickness, 128);
    EXPECT_EQ(g.lineGap, 0); EXPECT_EQ(g.designUnitsPerEm, 2048);
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_half_pixel_rounds_up) {
    /* descent 450 at ppem 16 sits exactly on 3.5 pixels (upp 128 -> 450/128 = 3.5156, not exact): use a synthetic descent of 448 = 3.5 px */
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_FONT_METRICS g; ttf_default_spec(&s); s.win_desc = 448; s.win_asc = 1856;   /* 14.5 px */
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, 16, 1, NULL, &g));
    EXPECT_EQ(g.descent, 512);    /* 3.5 px -> 4 px */
    EXPECT_EQ(g.ascent, 15 * 128);  /* 14.5 px -> 15 px */
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_negative_half_rounds_toward_plus_infinity) {
    /* underline position -448 = -3.5 px -> -3 px = -384 (round half up) */
    ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_FONT_METRICS g; ttf_default_spec(&s); s.underline_pos = -448;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, 16, 1, NULL, &g));
    EXPECT_EQ(g.underlinePosition, -384);
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_tiny_size_returns_design_units) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_FONT_METRICS d, g; if (!face) return;
    IDWriteFontFace_GetMetrics(face, &d);
    CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, 1e-12f, 1, NULL, &g));
    CHECK(!memcmp(&d, &g, sizeof d));
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_invalid_arguments) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const float em[] = { 0, -1, 16, 16, 0, -16, -0.0f }, pp[] = { 1, 1, 0, -1, 0, -1, 1 }; int i; if (!face) return;
    for (i = 0; i < 7; i++) {
        DWRITE_FONT_METRICS m; DWRITE_FONT_METRICS z; memset(&m, 0xCC, sizeof m); memset(&z, 0, sizeof z);
        HRESULT hr = IDWriteFontFace_GetGdiCompatibleMetrics(face, em[i], pp[i], NULL, &m);
        CHECKF(hr == E_INVAL, "emSize %g ppd %g -> %s", em[i], pp[i], dw_hr_name(hr));
        CHECKF(!memcmp(&m, &z, sizeof m), "output not zeroed for emSize %g ppd %g", em[i], pp[i]);
    }
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_invalid_arguments_v1) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace1 *f1; static const float em[] = { 0, -1, 16, 16 }, pp[] = { 1, 1, 0, -1 }; int i; if (!face) return;
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); if (!f1) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    for (i = 0; i < 4; i++) {
        DWRITE_FONT_METRICS1 m, z; memset(&m, 0xCC, sizeof m); memset(&z, 0, sizeof z);
        HRESULT hr = IDWriteFontFace1_GetGdiCompatibleMetrics(f1, em[i], pp[i], NULL, &m);
        CHECKF(hr == E_INVAL, "emSize %g ppd %g -> %s", em[i], pp[i], dw_hr_name(hr));
        CHECKF(!memcmp(&m, &z, sizeof m), "output not zeroed");
    }
    IDWriteFontFace1_Release(f1); dwx_font_close(&h);
}
FTEST(ffcr_gdi_nan_matrix_elements) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); int i; float nan = nanf(""); if (!face) return;
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX m = { 1, 0, 0, 1, 0, 0 }; float *p = &m.m11; DWRITE_FONT_METRICS g, z; memset(&g, 0xCC, sizeof g); memset(&z, 0, sizeof z);
        p[i] = nan;
        HRESULT hr = IDWriteFontFace_GetGdiCompatibleMetrics(face, 16, 1, &m, &g);
        CHECKF(hr == E_INVAL, "NaN in matrix element %d -> %s", i, dw_hr_name(hr));
        CHECKF(!memcmp(&g, &z, sizeof g), "output not zeroed (element %d)", i);
    }
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_typo_font_gap_rounding) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; s.fs_selection = 0; ttf_default_spec(&s); s.fs_selection |= 0x80; s.typo_gap = 200;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    ffc_gdi_case(face, 12, 1, NULL, 1, "typo"); ffc_gdi_case(face, 16, 1, NULL, 1, "typo");
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_model_upm_1000) {
    ttf_spec s; dwx_font h; IDWriteFontFace *face; int i; static const float em[] = { 9, 12, 13, 16, 33 }; ttf_default_spec(&s); s.upm = 1000;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    for (i = 0; i < 5; i++) if (!ffc_gdi_case(face, em[i], 1, NULL, 1, "upm1000")) break;
    dwx_font_close(&h);
}
FTEST(ffcr_gdi_simulations_do_not_change_metrics) {
    int sm;
    for (sm = 0; sm < 4; sm++) {
        ttf_spec s; dwx_font h; IDWriteFontFace *face; DWRITE_FONT_METRICS g; ttf_default_spec(&s);
        face = ffc_open(&h, f, &s, (DWRITE_FONT_SIMULATIONS)sm); if (!face) return;
        CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleMetrics(face, 16, 1, NULL, &g));
        EXPECT_EQ(g.ascent, 1792); EXPECT_EQ(g.descent, 512);
        dwx_font_close(&h);
    }
}
FTEST(ffcr_gdi_sys_arial_model) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace1 *f1; DWRITE_FONT_METRICS1 d, g; int i; static const float em[] = { 8, 9, 10, 11, 12, 13, 14, 16, 20, 24, 36 };
    if (!face) { t_skip("Arial not installed"); return; }
    f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); if (!f1) { t_skip("no IDWriteFontFace1"); IDWriteFontFace_Release(face); return; }
    IDWriteFontFace1_GetMetrics(f1, &d);
    for (i = 0; i < 11; i++) {
        float upp = (float)d.designUnitsPerEm / em[i];
        CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleMetrics(f1, em[i], 1, NULL, &g));
        /* Arial ships a VDMX table: ascent/descent may follow it; the other fields follow the grid rule */
        CHECKF(g.capHeight == ffc_snap_u16(d.capHeight, upp), "Arial em %g cap", em[i]);
        CHECKF(g.xHeight == ffc_snap_u16(d.xHeight, upp), "Arial em %g x", em[i]);
        CHECKF(g.underlineThickness == ffc_snap_u16(d.underlineThickness, upp), "Arial em %g ul", em[i]);
        t_note("Arial em %g: gdi asc %u desc %u (design %u/%u) gap %d", em[i], g.ascent, g.descent, d.ascent, d.descent, g.lineGap);
    }
    IDWriteFontFace1_Release(f1); IDWriteFontFace_Release(face);
}

/* ================================ GetRecommendedRenderingMode ========================================================================= */
static IDWriteRenderingParams *ffc_params(IDWriteFactory *f, DWRITE_RENDERING_MODE mode) {
    IDWriteRenderingParams *p = NULL;
    if (FAILED(IDWriteFactory_CreateCustomRenderingParams(f, 2.2f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, mode, &p))) return NULL;
    return p;
}
#define RM_V1(face, em, ppd, mm, p, out) IDWriteFontFace_GetRecommendedRenderingMode(face, em, ppd, mm, p, out)

FTEST(ffcr_rm_v1_null_params) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_RENDERING_MODE m = (DWRITE_RENDERING_MODE)99; if (!face) return;
    CHECK_HR(RM_V1(face, 12, 1, DWRITE_MEASURING_MODE_NATURAL, NULL, &m), E_INVAL);
    EXPECT_EQ(m, DWRITE_RENDERING_MODE_DEFAULT);
    dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_outline_from_100_ppem) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteRenderingParams *p = ffc_params(f, DWRITE_RENDERING_MODE_DEFAULT); int mm; if (!face || !p) { if (p) IDWriteRenderingParams_Release(p); dwx_font_close(&h); if (!p) t_skip("no params"); return; }
    for (mm = 0; mm < 3; mm++) {
        DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99;
        CHECK_HR_OK(RM_V1(face, 100, 1, (DWRITE_MEASURING_MODE)mm, p, &r)); CHECKF(r == DWRITE_RENDERING_MODE_OUTLINE, "measuring %d em 100 -> %d", mm, (int)r);
        CHECK_HR_OK(RM_V1(face, 200, 1, (DWRITE_MEASURING_MODE)mm, p, &r)); CHECKF(r == DWRITE_RENDERING_MODE_OUTLINE, "measuring %d em 200 -> %d", mm, (int)r);
        CHECK_HR_OK(RM_V1(face, 50, 2, (DWRITE_MEASURING_MODE)mm, p, &r)); CHECKF(r == DWRITE_RENDERING_MODE_OUTLINE, "measuring %d em 50 ppd 2 -> %d", mm, (int)r);
        CHECK_HR_OK(RM_V1(face, 99.5f, 1, (DWRITE_MEASURING_MODE)mm, p, &r)); CHECKF(r != DWRITE_RENDERING_MODE_OUTLINE, "measuring %d em 99.5 -> OUTLINE", mm);
        CHECK_HR_OK(RM_V1(face, 49.5f, 2, (DWRITE_MEASURING_MODE)mm, p, &r)); CHECKF(r != DWRITE_RENDERING_MODE_OUTLINE, "measuring %d em 49.5 ppd 2 -> OUTLINE", mm);
    }
    IDWriteRenderingParams_Release(p); dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_gdi_natural_is_gdi_natural) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteRenderingParams *p = ffc_params(f, DWRITE_RENDERING_MODE_DEFAULT); int i; static const float em[] = { 6, 8, 9, 10, 12, 14, 16, 20, 24, 36, 72, 99 };
    if (!face || !p) { if (p) IDWriteRenderingParams_Release(p); dwx_font_close(&h); if (!p) t_skip("no params"); return; }
    for (i = 0; i < 12; i++) { DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; CHECK_HR_OK(RM_V1(face, em[i], 1, DWRITE_MEASURING_MODE_GDI_NATURAL, p, &r)); CHECKF(r == DWRITE_RENDERING_MODE_GDI_NATURAL, "em %g -> %d", em[i], (int)r); }
    IDWriteRenderingParams_Release(p); dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_gdi_classic_aliased_or_classic) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteRenderingParams *p = ffc_params(f, DWRITE_RENDERING_MODE_DEFAULT); int i; static const float em[] = { 6, 8, 9, 10, 12, 14, 16, 20, 24, 36, 72 };
    if (!face || !p) { if (p) IDWriteRenderingParams_Release(p); dwx_font_close(&h); if (!p) t_skip("no params"); return; }
    for (i = 0; i < 11; i++) {
        DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; CHECK_HR_OK(RM_V1(face, em[i], 1, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, &r));
        CHECKF(r == DWRITE_RENDERING_MODE_ALIASED || r == DWRITE_RENDERING_MODE_GDI_CLASSIC, "em %g -> %d", em[i], (int)r);
        t_note("GDI_CLASSIC em %g -> %d", em[i], (int)r);
    }
    IDWriteRenderingParams_Release(p); dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_natural_without_gasp_is_symmetric_above_20ppem) {
    /* synthetic font: no gasp, no hinting program => NATURAL_SYMMETRIC for every size (and certainly above 20 ppem) */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteRenderingParams *p = ffc_params(f, DWRITE_RENDERING_MODE_DEFAULT); int i; static const float em[] = { 21, 24, 32, 48, 72, 99 };
    if (!face || !p) { if (p) IDWriteRenderingParams_Release(p); dwx_font_close(&h); if (!p) t_skip("no params"); return; }
    for (i = 0; i < 6; i++) { DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; CHECK_HR_OK(RM_V1(face, em[i], 1, DWRITE_MEASURING_MODE_NATURAL, p, &r)); CHECKF(r == DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, "em %g -> %d", em[i], (int)r); }
    IDWriteRenderingParams_Release(p); dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_natural_small_sizes_are_natural_family) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteRenderingParams *p = ffc_params(f, DWRITE_RENDERING_MODE_DEFAULT); int i; if (!face || !p) { if (p) IDWriteRenderingParams_Release(p); dwx_font_close(&h); if (!p) t_skip("no params"); return; }
    for (i = 6; i <= 20; i++) {
        DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; CHECK_HR_OK(RM_V1(face, (float)i, 1, DWRITE_MEASURING_MODE_NATURAL, p, &r));
        CHECKF(r == DWRITE_RENDERING_MODE_NATURAL || r == DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, "em %d -> %d", i, (int)r);
        t_note("synthetic font NATURAL em %d -> %d", i, (int)r);
    }
    IDWriteRenderingParams_Release(p); dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_all_sizes_8_to_72_in_range) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteRenderingParams *p = ffc_params(f, DWRITE_RENDERING_MODE_DEFAULT); int em, mm; if (!face || !p) { if (p) IDWriteRenderingParams_Release(p); dwx_font_close(&h); if (!p) t_skip("no params"); return; }
    for (mm = 0; mm < 3; mm++) for (em = 8; em <= 72; em++) {
        DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; HRESULT hr = RM_V1(face, (float)em, 1, (DWRITE_MEASURING_MODE)mm, p, &r);
        CHECKF(hr == S_OK && r >= DWRITE_RENDERING_MODE_ALIASED && r <= DWRITE_RENDERING_MODE_OUTLINE, "measuring %d em %d -> %s %d", mm, em, dw_hr_name(hr), (int)r);
    }
    IDWriteRenderingParams_Release(p); dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_explicit_params_modes_returned) {
    /* a non-default mode stored in the rendering parameters wins over everything else, also above the outline threshold */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const DWRITE_RENDERING_MODE modes[] = { DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_GDI_CLASSIC, DWRITE_RENDERING_MODE_GDI_NATURAL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_RENDERING_MODE_OUTLINE };
    int i, mm; if (!face) return;
    for (i = 0; i < 6; i++) {
        IDWriteRenderingParams *p = ffc_params(f, modes[i]); if (!p) { t_skip("no params"); break; }
        for (mm = 0; mm < 3; mm++) {
            DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99, r2 = (DWRITE_RENDERING_MODE)99;
            CHECK_HR_OK(RM_V1(face, 12, 1, (DWRITE_MEASURING_MODE)mm, p, &r)); CHECKF(r == modes[i], "params mode %d measuring %d em 12 -> %d", (int)modes[i], mm, (int)r);
            CHECK_HR_OK(RM_V1(face, 200, 1, (DWRITE_MEASURING_MODE)mm, p, &r2)); CHECKF(r2 == modes[i], "params mode %d measuring %d em 200 -> %d", (int)modes[i], mm, (int)r2);
        }
        IDWriteRenderingParams_Release(p);
    }
    dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_system_default_params_note) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteRenderingParams *p = NULL; DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; if (!face) return;
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(f, &p));
    CHECK_HR_OK(RM_V1(face, 12, 1, DWRITE_MEASURING_MODE_NATURAL, p, &r));
    t_note("system default params: NATURAL em12 -> %d (params mode %d)", (int)r, (int)IDWriteRenderingParams_GetRenderingMode(p));
    CHECK(r >= 1 && r <= 6);
    IDWriteRenderingParams_Release(p); dwx_font_close(&h);
}
FTEST(ffcr_rm_v1_sys_arial_sizes_note) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteRenderingParams *p = ffc_params(f, DWRITE_RENDERING_MODE_DEFAULT); int em;
    char line[600]; int n = 0; uint8_t *gasp; UINT32 gl = 0;
    if (!face || !p) { if (face) IDWriteFontFace_Release(face); if (p) IDWriteRenderingParams_Release(p); t_skip("Arial/params unavailable"); return; }
    gasp = dw_face_table(face, DW_TAG('g','a','s','p'), &gl);
    if (gasp && gl >= 4) { unsigned nr = ffc_be16(gasp + 2), k; for (k = 0; k < nr && 4 + 4 * k + 4 <= gl; k++) t_note("Arial gasp range %u: maxPPEM %u behavior 0x%x", k, ffc_be16(gasp + 4 + 4 * k), ffc_be16(gasp + 6 + 4 * k)); }
    for (em = 8; em <= 72; em++) {
        DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; CHECK_HR_OK(RM_V1(face, (float)em, 1, DWRITE_MEASURING_MODE_NATURAL, p, &r));
        CHECKF(r >= 1 && r <= 6, "em %d -> %d", em, (int)r);
        n += sprintf(line + n % 400, "%d:%d ", em, (int)r);
        if (n > 380) { t_note("Arial NATURAL %s", line); n = 0; line[0] = 0; }
    }
    if (n) t_note("Arial NATURAL %s", line);
    free(gasp); IDWriteRenderingParams_Release(p); IDWriteFontFace_Release(face);
}

/* ---- IDWriteFontFace1 (no rendering parameters) ----------------------------------------------------------------------------------------- */
static HRESULT ffc_rm_f1(IDWriteFontFace *face, float em, float dx, float dy, const DWRITE_MATRIX *x, BOOL side, DWRITE_OUTLINE_THRESHOLD th, DWRITE_MEASURING_MODE mm, DWRITE_RENDERING_MODE *out) {
    IDWriteFontFace1 *f1 = (IDWriteFontFace1 *)ffc_face_v(face, 1); HRESULT hr = E_NOINTERFACE;
    if (f1) { hr = IDWriteFontFace1_GetRecommendedRenderingMode(f1, em, dx, dy, x, side, th, mm, out); IDWriteFontFace1_Release(f1); }
    return hr;
}
FTEST(ffcr_rm_f1_outline_thresholds) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_RENDERING_MODE r; HRESULT hr; if (!face) return;
    hr = ffc_rm_f1(face, 100, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, &r);
    if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    CHECK_HR_OK(hr); EXPECT_EQ(r, DWRITE_RENDERING_MODE_OUTLINE);
    r = 99; CHECK_HR_OK(ffc_rm_f1(face, 99.9f, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, &r)); CHECK(r != DWRITE_RENDERING_MODE_OUTLINE);
    r = 99; CHECK_HR_OK(ffc_rm_f1(face, 50, 192, 192, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_OUTLINE);
    r = 99; CHECK_HR_OK(ffc_rm_f1(face, 49.9f, 192, 192, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_GDI_NATURAL);
    /* ALIASED threshold: outline only from 350 ppem */
    r = 99; CHECK_HR_OK(ffc_rm_f1(face, 349, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_GDI_NATURAL);
    r = 99; CHECK_HR_OK(ffc_rm_f1(face, 350, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_OUTLINE);
    r = 99; CHECK_HR_OK(ffc_rm_f1(face, 200, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_GDI_NATURAL);
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f1_dpi_scales_ppem) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_RENDERING_MODE r; HRESULT hr; if (!face) return;
    hr = ffc_rm_f1(face, 50, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r);
    if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    EXPECT_EQ(r, DWRITE_RENDERING_MODE_GDI_NATURAL);
    CHECK_HR_OK(ffc_rm_f1(face, 25, 384, 384, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_OUTLINE);
    CHECK_HR_OK(ffc_rm_f1(face, 100, 48, 48, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_GDI_NATURAL);   /* 50 ppem */
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f1_transform_scale) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_RENDERING_MODE r; DWRITE_MATRIX m2 = { 2, 0, 0, 2, 0, 0 }; HRESULT hr; if (!face) return;
    hr = ffc_rm_f1(face, 60, 96, 96, &m2, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r);
    if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    CHECK_HR_OK(hr); EXPECT_EQ(r, DWRITE_RENDERING_MODE_OUTLINE);                                  /* 120 ppem */
    CHECK_HR_OK(ffc_rm_f1(face, 40, 96, 96, &m2, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_GDI_NATURAL);   /* 80 ppem */
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f1_rotated_natural_is_symmetric) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_RENDERING_MODE r; DWRITE_MATRIX rot = { 0.70710678f, 0.70710678f, -0.70710678f, 0.70710678f, 0, 0 }; HRESULT hr; int sd; if (!face) return;
    hr = ffc_rm_f1(face, 12, 96, 96, &rot, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, &r);
    if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    CHECK_HR_OK(hr); EXPECT_EQ(r, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC);
    for (sd = 0; sd < 2; sd++) { r = 99; CHECK_HR_OK(ffc_rm_f1(face, 12, 192, 192, &rot, sd, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, &r)); EXPECT_EQ(r, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC); }   /* never the downsampled value 7 */
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f1_never_returns_downsampled) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); int dpi, em, mm; if (!face) return;
    for (mm = 0; mm < 3; mm++) for (dpi = 96; dpi <= 384; dpi += 96) for (em = 6; em <= 90; em += 6) {
        DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; HRESULT hr = ffc_rm_f1(face, (float)em, (float)dpi, (float)dpi, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)mm, &r);
        if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
        CHECKF(hr == S_OK && r >= 1 && r <= 6, "mm %d dpi %d em %d -> %s %d", mm, dpi, em, dw_hr_name(hr), (int)r);
    }
    dwx_font_close(&h);
}

/* ---- IDWriteFontFace2 / IDWriteFontFace3 ---------------------------------------------------------------------------------------------------- */
static HRESULT ffc_rm_f2(IDWriteFontFace *face, float em, float dx, float dy, const DWRITE_MATRIX *x, BOOL side, DWRITE_OUTLINE_THRESHOLD th, DWRITE_MEASURING_MODE mm, IDWriteRenderingParams *p, DWRITE_RENDERING_MODE *out, DWRITE_GRID_FIT_MODE *gf) {
    IDWriteFontFace2 *f2 = (IDWriteFontFace2 *)ffc_face_v(face, 2); HRESULT hr = E_NOINTERFACE;
    if (f2) { hr = IDWriteFontFace2_GetRecommendedRenderingMode(f2, em, dx, dy, x, side, th, mm, p, out, gf); IDWriteFontFace2_Release(f2); }
    return hr;
}
static HRESULT ffc_rm_f3(IDWriteFontFace *face, float em, float dx, float dy, const DWRITE_MATRIX *x, BOOL side, DWRITE_OUTLINE_THRESHOLD th, DWRITE_MEASURING_MODE mm, IDWriteRenderingParams *p, DWRITE_RENDERING_MODE1 *out, DWRITE_GRID_FIT_MODE *gf) {
    IDWriteFontFace3 *f3 = (IDWriteFontFace3 *)ffc_face_v(face, 3); HRESULT hr = E_NOINTERFACE;
    if (f3) { hr = IDWriteFontFace3_GetRecommendedRenderingMode(f3, em, dx, dy, x, side, th, mm, p, out, gf); IDWriteFontFace3_Release(f3); }
    return hr;
}
FTEST(ffcr_rm_f2_invalid_threshold_and_measuring) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const int th[] = { 2, 3, 100, -1 }, mm[] = { 3, 4, 100, -1 }; int i; if (!face) return;
    for (i = 0; i < 4; i++) {
        DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)99;
        HRESULT hr = ffc_rm_f2(face, 12, 96, 96, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)th[i], DWRITE_MEASURING_MODE_NATURAL, NULL, &r, &g);
        if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace2"); break; }
        CHECKF(hr == E_INVAL, "threshold %d -> %s", th[i], dw_hr_name(hr)); EXPECT_EQ(r, 0); EXPECT_EQ(g, 0);
        r = (DWRITE_RENDERING_MODE)99; g = (DWRITE_GRID_FIT_MODE)99;
        hr = ffc_rm_f2(face, 12, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)mm[i], NULL, &r, &g);
        CHECKF(hr == E_INVAL, "measuring %d -> %s", mm[i], dw_hr_name(hr)); EXPECT_EQ(r, 0); EXPECT_EQ(g, 0);
    }
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f2_null_params_allowed) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)99; HRESULT hr; if (!face) return;
    hr = ffc_rm_f2(face, 120, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, &r, &g);
    if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace2"); dwx_font_close(&h); return; }
    CHECK_HR_OK(hr); EXPECT_EQ(r, DWRITE_RENDERING_MODE_OUTLINE);
    CHECKF(g == DWRITE_GRID_FIT_MODE_DISABLED || g == DWRITE_GRID_FIT_MODE_ENABLED || g == DWRITE_GRID_FIT_MODE_DEFAULT, "grid fit %d", (int)g);
    t_note("em 120 outline: grid fit mode %d", (int)g);
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f2_matches_f1_without_params) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); int em, mm; if (!face) return;
    for (mm = 0; mm < 3; mm++) for (em = 6; em <= 120; em += 7) {
        DWRITE_RENDERING_MODE a = (DWRITE_RENDERING_MODE)99, b = (DWRITE_RENDERING_MODE)98; DWRITE_GRID_FIT_MODE g; HRESULT h1 = ffc_rm_f1(face, (float)em, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)mm, &a);
        HRESULT h2 = ffc_rm_f2(face, (float)em, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)mm, NULL, &b, &g);
        if (h1 == E_NOINTERFACE || h2 == E_NOINTERFACE) { t_skip("no IDWriteFontFace1/2"); dwx_font_close(&h); return; }
        CHECKF(h1 == S_OK && h2 == S_OK && a == b, "mm %d em %d: face1 %d face2 %d", mm, em, (int)a, (int)b);
    }
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f2_explicit_grid_fit_returned) {
    IDWriteFactory2 *f2 = NULL; dwx_font h; IDWriteFontFace *face; IDWriteRenderingParams2 *p = NULL; static const DWRITE_GRID_FIT_MODE gm[2] = { DWRITE_GRID_FIT_MODE_DISABLED, DWRITE_GRID_FIT_MODE_ENABLED }; int i;
    face = ffc_open_def(&h, f); if (!face) return;
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2)) || !f2) { t_skip("no IDWriteFactory2"); dwx_font_close(&h); return; }
    for (i = 0; i < 2; i++) {
        DWRITE_RENDERING_MODE r = (DWRITE_RENDERING_MODE)99; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)99; HRESULT hr;
        hr = IDWriteFactory2_CreateCustomRenderingParams(f2, 2.2f, 0.5f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, gm[i], &p);
        if (FAILED(hr) || !p) { t_skip("CreateCustomRenderingParams(v2): %s", dw_hr_name(hr)); break; }
        hr = ffc_rm_f2(face, 12, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, (IDWriteRenderingParams *)p, &r, &g);
        if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace2"); IDWriteRenderingParams2_Release(p); p = NULL; break; }
        CHECK_HR_OK(hr); EXPECT_EQ(g, gm[i]);
        IDWriteRenderingParams2_Release(p); p = NULL;
    }
    IDWriteFactory2_Release(f2); dwx_font_close(&h);
}
FTEST(ffcr_rm_f3_invalid_arguments) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_RENDERING_MODE1 r = (DWRITE_RENDERING_MODE1)99; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)99; HRESULT hr; if (!face) return;
    hr = ffc_rm_f3(face, 12, 96, 96, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)2, DWRITE_MEASURING_MODE_NATURAL, NULL, &r, &g);
    if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace3"); dwx_font_close(&h); return; }
    CHECK(hr == E_INVAL); EXPECT_EQ(r, 0); EXPECT_EQ(g, 0);
    r = (DWRITE_RENDERING_MODE1)99; g = (DWRITE_GRID_FIT_MODE)99;
    hr = ffc_rm_f3(face, 12, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)3, NULL, &r, &g);
    CHECK(hr == E_INVAL); EXPECT_EQ(r, 0); EXPECT_EQ(g, 0);
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f3_outline_and_grid_fit) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_RENDERING_MODE1 r = (DWRITE_RENDERING_MODE1)99; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)99; HRESULT hr; if (!face) return;
    hr = ffc_rm_f3(face, 100, 96, 96, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, &r, &g);
    if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace3"); dwx_font_close(&h); return; }
    CHECK_HR_OK(hr); EXPECT_EQ(r, DWRITE_RENDERING_MODE1_OUTLINE);
    CHECKF(g == DWRITE_GRID_FIT_MODE_DISABLED || g == DWRITE_GRID_FIT_MODE_ENABLED, "grid fit %d", (int)g);
    dwx_font_close(&h);
}
FTEST(ffcr_rm_f3_downsampled_at_high_dpi_note) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); int dpi; if (!face) return;
    for (dpi = 96; dpi <= 288; dpi += 48) {
        DWRITE_RENDERING_MODE1 r = (DWRITE_RENDERING_MODE1)99; DWRITE_GRID_FIT_MODE g; DWRITE_RENDERING_MODE r2 = (DWRITE_RENDERING_MODE)99; DWRITE_GRID_FIT_MODE g2;
        HRESULT hr = ffc_rm_f3(face, 24, (float)dpi, (float)dpi, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, &r, &g);
        if (hr == E_NOINTERFACE) { t_skip("no IDWriteFontFace3"); break; }
        CHECK_HR_OK(hr);
        CHECK_HR_OK(ffc_rm_f2(face, 24, (float)dpi, (float)dpi, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_NATURAL, NULL, &r2, &g2));
        t_note("dpi %d em 24 NATURAL: face3 mode %d, face2 mode %d", dpi, (int)r, (int)r2);
        /* face2 reports the downsampled value as NATURAL_SYMMETRIC, face3 keeps 7 */
        CHECKF((int)r == (int)r2 || ((int)r == 7 && (int)r2 == 5), "dpi %d: face3 %d face2 %d", dpi, (int)r, (int)r2);
        CHECK(r2 != (DWRITE_RENDERING_MODE)7);
    }
    dwx_font_close(&h);
}
