/* Glyph orientation analysis (IDWriteTextAnalyzer1::AnalyzeVerticalGlyphOrientation / GetGlyphOrientationTransform) as the vertical layout
 * sees it, and the agreement between the analyzer's answer and what a vertical layout hands to the renderer.
 *
 * OrientationAnalyzer::Analyze / GetOrientationInfo (GetOrientationInfo(class, orientation, bidiLevel) -> {angle, adjustedBidi, isSideways, isRTL}):
 *   DEFAULT (0): adjustedBidi = bidi, isRTL = bidi & 1, and exactly one of  (angle 90 degrees, isSideways FALSE)  /  (angle 0, isSideways TRUE)
 *                depending on the Unicode vertical-orientation class of the character;
 *   STACKED (1): adjustedBidi = (bidi + 1, but bidi itself above 61) & 0xFE, then either (angle 0, isSideways TRUE, isRTL FALSE) or
 *                (angle 90 for even / 270 for odd bidi, isSideways FALSE, isRTL = bidi & 1);
 *   other values: the all-zero result.
 * The 90 degree + not-sideways form and the 0 degree + sideways form select the same GetGlyphOrientationTransform matrix (angle + isSideways = 1).
 * A span that is sideways sits on the central baseline of a vertical line (GetBaselineDelta uses CENTRAL for sideways spans and ROMAN otherwise),
 * i.e. the sideways form belongs to upright ideographs.
 */
#include "vto.h"

#define ANGLE_0  DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES
#define ANGLE_90 DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES
#define ANGLE_270 DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES

typedef struct an { IDWriteTextAnalyzer1 *a1; IDWriteTextAnalyzer2 *a2; dwx_ana_src *src; dwx_ana_sink *sk; } an;
static int an_open(vt_fx *x, an *o)
{
    IDWriteTextAnalyzer *a = NULL;
    memset(o, 0, sizeof *o);
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(x->f, &a))) return 0;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&o->a1))) { DW_RELEASE(a); return 0; }
    IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer2, (void **)&o->a2);
    DW_RELEASE(a);
    return 1;
}
static void an_close(an *o) { DW_RELEASE(o->a1); DW_RELEASE(o->a2); if (o->src) dwx_release(o->src); if (o->sk) dwx_release(o->sk); }
#define AN_BEGIN() VT_BEGIN(); an N; if (!an_open(&X, &N)) { an_close(&N); VT_END(); t_skip("IDWriteTextAnalyzer1 unavailable"); return; }
#define AN_END() do { an_close(&N); VT_END(); } while (0)
static HRESULT an_run(an *o, const WCHAR *t, UINT32 len, DWRITE_VERTICAL_GLYPH_ORIENTATION ori, UINT8 bidi)
{
    if (o->src) dwx_release(o->src); if (o->sk) dwx_release(o->sk);
    o->src = dwx_ana_src_new(t, len); o->sk = dwx_ana_sink_new();
    o->src->support_v1 = 1; o->sk->support_v1 = 1; o->src->orientation = ori; o->src->orient_bidi = bidi;
    return IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(o->a1, (IDWriteTextAnalysisSource1 *)o->src, 0, len, (IDWriteTextAnalysisSink1 *)o->sk);
}
#define SK (N.sk)

/* ---- DEFAULT orientation ------------------------------------------------------------------------------------------ */
TEST(default_forms_are_the_two_documented_tuples)
{
    static const WCHAR t[] = { 'A', 'z', '5', ' ', 0x4e00, 0x3042, 0xff21, '(', '.' };
    AN_BEGIN(); UINT32 i;
    CHECK_HR_OK(an_run(&N, t, 9, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0));
    for (i = 0; i < 9; i++) {
        BOOL rot = SK->angle[i] == ANGLE_90 && !SK->orient_sideways[i], up = SK->angle[i] == ANGLE_0 && SK->orient_sideways[i];
        CHECKF(SK->orient_set[i] && (rot || up), "U+%04X: angle %d sideways %d", t[i], SK->angle[i], SK->orient_sideways[i]);
        EXPECT_EQ(SK->orient_bidi[i], 0); CHECK(!SK->orient_rtl[i]);
    }
    AN_END();
}
TEST(default_latin_is_rotated_form)
{
    /* Latin letters are rotated with the line: 90 degrees, isSideways FALSE (horizontal metrics). */
    AN_BEGIN(); UINT32 i;
    CHECK_HR_OK(an_run(&N, L"Hello", 5, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0));
    for (i = 0; i < 5; i++) { t_note("Latin[%u]: angle %d sideways %d", i, SK->angle[i], SK->orient_sideways[i]); CHECK(SK->orient_set[i]); }
    EXPECT_EQ(SK->n_orient, 1);
    AN_END();
}
TEST(default_han_is_upright_form)
{
    /* Ideographs are upright: angle 0, isSideways TRUE (vertical metrics, the run is rotated by the layout frame only). */
    static const WCHAR t[] = { 0x4e00, 0x4e8c, 0x4e09 };
    AN_BEGIN(); UINT32 i;
    CHECK_HR_OK(an_run(&N, t, 3, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0));
    for (i = 0; i < 3; i++) { t_note("Han[%u]: angle %d sideways %d", i, SK->angle[i], SK->orient_sideways[i]); CHECK(SK->orient_set[i]); }
    EXPECT_EQ(SK->n_orient, 1);
    AN_END();
}
TEST(default_latin_and_han_differ)
{
    static const WCHAR t[] = { 'A', 0x4e00 };
    AN_BEGIN();
    CHECK_HR_OK(an_run(&N, t, 2, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0));
    CHECK(SK->orient_sideways[0] != SK->orient_sideways[1]); CHECK(SK->angle[0] != SK->angle[1]); EXPECT_EQ(SK->n_orient, 2);
    AN_END();
}
TEST(default_bidi_level_passes_through_for_every_level)
{
    AN_BEGIN(); int lv;
    for (lv = 0; lv < 128; lv++) {
        CHECK_HR_OK(an_run(&N, L"A", 1, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, (UINT8)lv));
        CHECKF(SK->orient_bidi[0] == lv && SK->orient_rtl[0] == (lv & 1), "level %d -> adjusted %u rtl %d", lv, SK->orient_bidi[0], SK->orient_rtl[0]);
    }
    AN_END();
}
TEST(default_run_boundaries_follow_orientation_changes)
{
    static const WCHAR t[] = { 'a', 'b', 0x4e00, 0x4e01, 0x4e02, 'c' };
    AN_BEGIN();
    CHECK_HR_OK(an_run(&N, t, 6, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0));
    EXPECT_EQ(SK->n_orient, 3);
    CHECK(SK->orient_sideways[2] && SK->orient_sideways[3] && SK->orient_sideways[4]);
    CHECK(!SK->orient_sideways[0] && !SK->orient_sideways[1] && !SK->orient_sideways[5]);
    AN_END();
}
/* ---- STACKED orientation ------------------------------------------------------------------------------------------ */
TEST(stacked_adjusted_bidi_level_table)
{
    static const struct { int in, out; } t[] = { { 0, 0 }, { 1, 2 }, { 2, 2 }, { 3, 4 }, { 4, 4 }, { 5, 6 }, { 60, 60 }, { 61, 62 }, { 62, 62 }, { 63, 62 }, { 100, 100 }, { 101, 100 }, { 125, 124 } };
    AN_BEGIN(); int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        UINT8 in = (UINT8)t[i].in;
        CHECK_HR_OK(an_run(&N, L"A", 1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, in));
        CHECKF(SK->orient_bidi[0] == t[i].out, "STACKED level %d -> adjusted %u want %d", t[i].in, SK->orient_bidi[0], t[i].out);
        CHECKF((SK->orient_bidi[0] & 1) == 0, "adjusted level %u must be even", SK->orient_bidi[0]);
    }
    AN_END();
}
TEST(stacked_forms_by_parity)
{
    static const WCHAR t[] = { 'A', 0x4e00, '1' };
    AN_BEGIN(); int lv; UINT32 i;
    for (lv = 0; lv < 6; lv++) {
        CHECK_HR_OK(an_run(&N, t, 3, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, (UINT8)lv));
        for (i = 0; i < 3; i++) {
            int formA = SK->angle[i] == ANGLE_0 && SK->orient_sideways[i] && !SK->orient_rtl[i];
            int formB = SK->angle[i] == (lv & 1 ? ANGLE_270 : ANGLE_90) && !SK->orient_sideways[i] && SK->orient_rtl[i] == (lv & 1);
            CHECKF(formA || formB, "STACKED level %d U+%04X: angle %d sideways %d rtl %d", lv, t[i], SK->angle[i], SK->orient_sideways[i], SK->orient_rtl[i]);
        }
    }
    AN_END();
}
TEST(stacked_classes_noted)
{
    static const WCHAR t[] = { 'A', 'a', '1', ' ', '.', 0x4e00, 0x3042, 0xff21 };
    AN_BEGIN(); UINT32 i;
    CHECK_HR_OK(an_run(&N, t, 8, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0));
    for (i = 0; i < 8; i++) t_note("STACKED U+%04X level 0: angle %d sideways %d", t[i], SK->angle[i], SK->orient_sideways[i]);
    AN_END();
}
TEST(invalid_orientation_values_give_the_zero_result)
{
    static const UINT32 bad[] = { 2, 3, 7, 255, 0x7fffffff, 0xffffffffu };
    AN_BEGIN(); int i;
    for (i = 0; i < 6; i++) {
        CHECK_HR_OK(an_run(&N, L"A", 1, (DWRITE_VERTICAL_GLYPH_ORIENTATION)bad[i], 5));
        CHECKF(SK->angle[0] == ANGLE_0 && SK->orient_bidi[0] == 0 && !SK->orient_sideways[0] && !SK->orient_rtl[0], "orientation %u: angle %d bidi %u sideways %d rtl %d", bad[i], SK->angle[0], SK->orient_bidi[0], SK->orient_sideways[0], SK->orient_rtl[0]);
    }
    AN_END();
}

/* ---- argument checks specific to the source ---------------------------------------------------------------------- */
TEST(invalid_paragraph_reading_direction_is_rejected)
{
    AN_BEGIN(); HRESULT hr;
    N.src = dwx_ana_src_new(L"AB", 2); N.sk = dwx_ana_sink_new(); N.src->support_v1 = 1; N.sk->support_v1 = 1;
    N.src->dir = (DWRITE_READING_DIRECTION)4;
    hr = IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(N.a1, (IDWriteTextAnalysisSource1 *)N.src, 0, 2, (IDWriteTextAnalysisSink1 *)N.sk);
    CHECK_HR(hr, VT_E_INVALIDARG); EXPECT_EQ(N.sk->n_orient, 0);
    N.src->dir = RD_BTT;                                     /* every value 0..3 is accepted */
    CHECK_HR_OK(IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(N.a1, (IDWriteTextAnalysisSource1 *)N.src, 0, 2, (IDWriteTextAnalysisSink1 *)N.sk));
    AN_END();
}
TEST(sink_failure_surfaces_as_that_hresult)
{
    AN_BEGIN(); HRESULT hr;
    N.src = dwx_ana_src_new(L"AB", 2); N.sk = dwx_ana_sink_new(); N.src->support_v1 = 1; N.sk->support_v1 = 1; N.sk->fail_orient = E_ABORT;
    hr = IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(N.a1, (IDWriteTextAnalysisSource1 *)N.src, 0, 2, (IDWriteTextAnalysisSink1 *)N.sk);
    CHECKF(hr == E_ABORT, "got %s", dw_hr_name(hr));
    AN_END();
}
TEST(source_failure_surfaces_as_that_hresult)
{
    AN_BEGIN(); HRESULT hr;
    N.src = dwx_ana_src_new(L"AB", 2); N.sk = dwx_ana_sink_new(); N.src->support_v1 = 1; N.sk->support_v1 = 1; N.src->fail_orient = E_ABORT;
    hr = IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(N.a1, (IDWriteTextAnalysisSource1 *)N.src, 0, 2, (IDWriteTextAnalysisSink1 *)N.sk);
    CHECKF(hr == E_ABORT, "got %s", dw_hr_name(hr)); EXPECT_EQ(N.sk->n_orient, 0);
    AN_END();
}

/* ---- the layout reports what the analyzer says -------------------------------------------------------------------- */
TEST(layout_runs_match_analyzer_default)
{
    static const WCHAR text[] = { 'A', 'B', 0x4e00, 0x4e01, 'C' };
    AN_BEGIN(); IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, text, 16.0f, 100, 200, RD_TTB, FD_RTL); dwx_renderer *r = vt_r1(); int i;
    if (!l) { AN_END(); t_skip("layout"); return; }
    CHECK_HR_OK(an_run(&N, text, 5, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0));
    CHECK_HR_OK(vt_draw(l, r, 0, 0));
    for (i = 0; i < vt_runs(r); i++) {
        const dwx_draw *d = vt_run(r, i); UINT32 p = d->text_pos;
        CHECKF(d->angle == SK->angle[p] && (!!d->sideways) == (!!SK->orient_sideways[p]), "run %d at %u: layout (%d,%d) analyzer (%d,%d)", i, p, d->angle, d->sideways, SK->angle[p], SK->orient_sideways[p]);
    }
    EXPECT_EQ(vt_runs(r), 3);
    dwx_release(r); DW_RELEASE(l); AN_END();
}
TEST(layout_runs_match_analyzer_stacked)
{
    static const WCHAR text[] = { 'A', 'B', 0x4e00, 0x4e01, 'C' };
    AN_BEGIN(); IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, text, 16.0f, 100, 200, RD_TTB, FD_RTL); IDWriteTextLayout2 *l2 = NULL; dwx_renderer *r = vt_r1(); int i;
    if (!l) { AN_END(); t_skip("layout"); return; }
    DW_REQUIRE_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2));
    CHECK_HR_OK(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    CHECK_HR_OK(an_run(&N, text, 5, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0));
    CHECK_HR_OK(vt_draw(l, r, 0, 0));
    for (i = 0; i < vt_runs(r); i++) {
        const dwx_draw *d = vt_run(r, i); UINT32 p = d->text_pos;
        CHECKF(d->angle == SK->angle[p] && (!!d->sideways) == (!!SK->orient_sideways[p]), "run %d at %u: layout (%d,%d) analyzer (%d,%d)", i, p, d->angle, d->sideways, SK->angle[p], SK->orient_sideways[p]);
    }
    dwx_release(r); DW_RELEASE(l2); DW_RELEASE(l); AN_END();
}
TEST(layout_run_angle_and_sideways_select_the_90_degree_matrix)
{
    AN_BEGIN(); IDWriteTextLayout *l = vt_layout_(&X, VT_FAM, L"AB\x4e00\x4e01", 16.0f, 100, 200, RD_TTB, FD_RTL); dwx_renderer *r = vt_r1(); int i;
    if (!l) { AN_END(); t_skip("layout"); return; }
    CHECK_HR_OK(vt_draw(l, r, 0, 0));
    for (i = 0; i < vt_runs(r); i++) {
        const dwx_draw *d = vt_run(r, i); DWRITE_MATRIX m; memset(&m, 0xcc, sizeof m);
        CHECK_HR_OK(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(N.a1, d->angle, d->sideways, &m));
        CHECK(m.m11 == 0.0f && m.m12 == 1.0f && m.m21 == -1.0f && m.m22 == 0.0f && m.dx == 0.0f && m.dy == 0.0f);
    }
    dwx_release(r); DW_RELEASE(l); AN_END();
}

/* ---- GetGlyphOrientationTransform table -------------------------------------------------------------------------- */
static const float k_m[4][4] = { { 1, 0, 0, 1 }, { 0, 1, -1, 0 }, { -1, 0, 0, -1 }, { 0, -1, 1, 0 } };
TEST(transform_all_eight_combinations_analyzer1)
{
    AN_BEGIN(); int a, s;
    for (a = 0; a < 4; a++) for (s = 0; s < 2; s++) {
        DWRITE_MATRIX m; int idx = (a + s) & 3; memset(&m, 0xcc, sizeof m);
        CHECK_HR_OK(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(N.a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)a, s, &m));
        CHECKF(m.m11 == k_m[idx][0] && m.m12 == k_m[idx][1] && m.m21 == k_m[idx][2] && m.m22 == k_m[idx][3] && m.dx == 0.0f && m.dy == 0.0f, "angle %d sideways %d: {%g %g %g %g %g %g}", a, s, m.m11, m.m12, m.m21, m.m22, m.dx, m.dy);
    }
    AN_END();
}
TEST(transform_origin_translation_analyzer2)
{
    AN_BEGIN(); int a, s; float ox = 50.0f, oy = 60.0f;
    if (!N.a2) { AN_END(); t_skip("IDWriteTextAnalyzer2"); return; }
    for (a = 0; a < 4; a++) for (s = 0; s < 2; s++) {
        DWRITE_MATRIX m; int idx = (a + s) & 3; float dx = 0, dy = 0;
        CHECK_HR_OK(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(N.a2, (DWRITE_GLYPH_ORIENTATION_ANGLE)a, s, ox, oy, &m));
        if (a + s != 0) { dx = (1.0f - k_m[idx][0]) * ox - oy * k_m[idx][2]; dy = (1.0f - k_m[idx][3]) * oy - ox * k_m[idx][1]; }
        CHECKF(m.m11 == k_m[idx][0] && m.m22 == k_m[idx][3] && fabs(m.dx - dx) < 1e-4 && fabs(m.dy - dy) < 1e-4, "angle %d sideways %d: dx %g/%g dy %g/%g", a, s, m.dx, dx, m.dy, dy);
    }
    AN_END();
}
TEST(transform_rotates_the_origin_onto_itself)
{
    AN_BEGIN(); int a; float ox = 12.5f, oy = -7.25f;
    if (!N.a2) { AN_END(); t_skip("IDWriteTextAnalyzer2"); return; }
    for (a = 0; a < 4; a++) {
        DWRITE_MATRIX m; float px, py;
        CHECK_HR_OK(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(N.a2, (DWRITE_GLYPH_ORIENTATION_ANGLE)a, FALSE, ox, oy, &m));
        px = ox * m.m11 + oy * m.m21 + m.dx; py = ox * m.m12 + oy * m.m22 + m.dy;
        CHECKF(fabs(px - ox) < 1e-4 && fabs(py - oy) < 1e-4, "angle %d moves the origin to (%g,%g)", a, px, py);
    }
    AN_END();
}
TEST(transform_invalid_angle_zeroes_matrix)
{
    static const UINT32 bad[] = { 4, 5, 100, 0x80000000u, 0xffffffffu };
    AN_BEGIN(); int i;
    for (i = 0; i < 5; i++) {
        DWRITE_MATRIX m; memset(&m, 0xcc, sizeof m);
        CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(N.a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)bad[i], FALSE, &m), VT_E_INVALIDARG);
        CHECK(m.m11 == 0.0f && m.m12 == 0.0f && m.m21 == 0.0f && m.m22 == 0.0f && m.dx == 0.0f && m.dy == 0.0f);
        if (N.a2) {
            memset(&m, 0xcc, sizeof m);
            CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(N.a2, (DWRITE_GLYPH_ORIENTATION_ANGLE)bad[i], TRUE, 3.0f, 4.0f, &m), VT_E_INVALIDARG);
            CHECK(m.m11 == 0.0f && m.dx == 0.0f && m.dy == 0.0f);
        }
    }
    AN_END();
}
TEST(transform_composition_of_rotations)
{
    /* (angle a, upright) followed by (angle b, upright) is the rotation by a + b: matrix product of the table entries. */
    AN_BEGIN(); int a, b;
    for (a = 0; a < 4; a++) for (b = 0; b < 4; b++) {
        DWRITE_MATRIX ma, mb, mc; float m11, m12, m21, m22;
        CHECK_HR_OK(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(N.a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)a, FALSE, &ma));
        CHECK_HR_OK(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(N.a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)b, FALSE, &mb));
        CHECK_HR_OK(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(N.a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)((a + b) & 3), FALSE, &mc));
        m11 = ma.m11 * mb.m11 + ma.m12 * mb.m21; m12 = ma.m11 * mb.m12 + ma.m12 * mb.m22; m21 = ma.m21 * mb.m11 + ma.m22 * mb.m21; m22 = ma.m21 * mb.m12 + ma.m22 * mb.m22;
        CHECKF(m11 == mc.m11 && m12 == mc.m12 && m21 == mc.m21 && m22 == mc.m22, "%d + %d", a, b);
    }
    AN_END();
}
