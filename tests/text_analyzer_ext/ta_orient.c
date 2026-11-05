/* GetGlyphOrientationTransform (IDWriteTextAnalyzer1 and the origin-taking IDWriteTextAnalyzer2 overload) and
 * AnalyzeVerticalGlyphOrientation.
 *
 * GetGlyphOrientationTransform (TextAnalyzer::GetGlyphOrientationTransform):
 *   index = angle + (isSideways ? 1 : 0); matrix = rotation[index & 3] with rotation = identity, 90, 180, 270 degrees
 *   ({1,0,0,1}, {0,1,-1,0}, {-1,0,0,-1}, {0,-1,1,0}); angle values above 270 degrees (> 3) -> E_INVALIDARG.
 *   When the UNMASKED index is non-zero the translation rotates about (originX, originY):
 *     dx = (1 - m11) * originX - originY * m21,   dy = (1 - m22) * originY - originX * m12
 *   (the IDWriteTextAnalyzer1 overload is the same function with origin (0,0), so its translation is always 0).
 *   On the invalid-angle path the matrix is still overwritten with a default value before E_INVALIDARG is returned.
 *
 * AnalyzeVerticalGlyphOrientation (OrientationAnalyzer::Analyze + GetOrientationInfo):
 *   E_INVALIDARG for NULL source / NULL sink / textPosition + textLength overflowing 32 bits; otherwise the source's
 *   GetParagraphReadingDirection is queried once, then GetVerticalGlyphOrientation per source run, and
 *   SetGlyphOrientation runs of identical (angle, bidi, sideways, rtl) are coalesced:
 *     DEFAULT orientation: "upright" Unicode classes -> angle 90, isSideways FALSE; others -> angle 0, isSideways TRUE;
 *                          adjusted bidi level = source level, isRightToLeft = level & 1;
 *     STACKED orientation: adjusted bidi level = (level + 1, saturating at 62) rounded down to even;
 *                          upright form -> angle 90 (even level) or 270 (odd level), isSideways FALSE,
 *                          isRightToLeft = level & 1;
 *     any other orientation value -> angle 0, level 0, not sideways, not RTL.
 */
#include "ta.h"

static const DWRITE_MATRIX k_rot[4] = {
    { 1, 0, 0, 1, 0, 0 }, { 0, 1, -1, 0, 0, 0 }, { -1, 0, 0, -1, 0, 0 }, { 0, -1, 1, 0, 0, 0 },
};
static int mat_linear_eq(const DWRITE_MATRIX *a, const DWRITE_MATRIX *b) {
    return a->m11 == b->m11 && a->m12 == b->m12 && a->m21 == b->m21 && a->m22 == b->m22;
}
static const char *mname(const DWRITE_MATRIX *m) {
    static char b[4][96]; static int i; i = (i + 1) & 3;
    snprintf(b[i], sizeof b[i], "{%g,%g,%g,%g,%g,%g}", m->m11, m->m12, m->m21, m->m22, m->dx, m->dy);
    return b[i];
}
#define CHECK_MAT(got, want) t_check(mat_linear_eq(&(got), &(want)) && (got).dx == (want).dx && (got).dy == (want).dy, __FILE__, __LINE__, #got " == " #want, "got %s want %s", mname(&(got)), mname(&(want)))

/* ---- IDWriteTextAnalyzer1 ------------------------------------------------------------------------------------------- */
TEST(ot1_all_angles_not_sideways) {
    TA_BEGIN(); int i;
    for (i = 0; i < 4; i++) {
        DWRITE_MATRIX m; memset(&m, 0x55, sizeof m);
        CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)i, FALSE, &m), S_OK);
        CHECK_MAT(m, k_rot[i]);
    }
    TA_END();
}
TEST(ot1_all_angles_sideways) {
    TA_BEGIN(); int i;
    for (i = 0; i < 4; i++) {
        DWRITE_MATRIX m; memset(&m, 0x55, sizeof m);
        CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)i, TRUE, &m), S_OK);
        CHECK_MAT(m, k_rot[(i + 1) & 3]);
    }
    TA_END();
}
TEST(ot1_sideways_is_any_nonzero_bool) {
    TA_BEGIN(); static const BOOL vals[] = { 1, 2, -1, 0x100, 0x80000000, 0x7fffffff }; int i;
    for (i = 0; i < (int)(sizeof vals / sizeof *vals); i++) {
        DWRITE_MATRIX m;
        CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES, vals[i], &m), S_OK);
        CHECK_MAT(m, k_rot[1]);
    }
    TA_END();
}
TEST(ot1_translation_always_zero) {
    TA_BEGIN(); int i, s;
    for (s = 0; s < 2; s++) for (i = 0; i < 4; i++) {
        DWRITE_MATRIX m; memset(&m, 0x55, sizeof m);
        CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)i, s, &m), S_OK);
        CHECK_F(m.dx, 0.0f); CHECK_F(m.dy, 0.0f);
    }
    TA_END();
}
TEST(ot1_invalid_angle) {
    TA_BEGIN(); static const UINT32 bad[] = { 4, 5, 7, 8, 255, 256, 0x7fffffff, 0x80000000, 0xffffffff }; int i, s;
    for (s = 0; s < 2; s++) for (i = 0; i < (int)(sizeof bad / sizeof *bad); i++) {
        DWRITE_MATRIX m; memset(&m, 0x55, sizeof m);
        CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, (DWRITE_GLYPH_ORIENTATION_ANGLE)bad[i], s, &m), TA_E_INVALIDARG);
        /* the matrix is overwritten with a default value before the angle is validated */
        CHECK(!(m.m11 != m.m11) && memcmp(&m, "\x55\x55\x55\x55", 4) != 0);
        if (i == 0 && s == 0) t_note("matrix left by invalid angle: %s", mname(&m));
    }
    TA_END();
}
TEST(ot1_matrices_are_rotations) {
    /* determinant 1, orthonormal rows */
    TA_BEGIN(); int i, s;
    for (s = 0; s < 2; s++) for (i = 0; i < 4; i++) {
        DWRITE_MATRIX m; IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, i, s, &m);
        CHECK_F(m.m11 * m.m22 - m.m12 * m.m21, 1.0f);
        CHECK_F(m.m11 * m.m11 + m.m12 * m.m12, 1.0f);
        CHECK_F(m.m21 * m.m21 + m.m22 * m.m22, 1.0f);
        CHECK_F(m.m11 * m.m21 + m.m12 * m.m22, 0.0f);
    }
    TA_END();
}
TEST(ot1_composition_adds_angles) {
    /* sideways(angle) == non-sideways(angle + 90 degrees): the two lookups index the same table */
    TA_BEGIN(); int i;
    for (i = 0; i < 4; i++) {
        DWRITE_MATRIX x, y;
        IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, i, TRUE, &x);
        if (i < 3) { IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, i + 1, FALSE, &y); CHECK_MAT(x, y); }
        else { IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, 0, FALSE, &y); CHECK_MAT(x, y); }
    }
    TA_END();
}
TEST(ot1_repeatable_and_stateless) {
    TA_BEGIN(); DWRITE_MATRIX x, y;
    IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, 2, TRUE, &x);
    IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, 0, FALSE, &y);
    IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, 2, TRUE, &y);
    CHECK(!memcmp(&x, &y, sizeof x));
    TA_END();
}

/* ---- IDWriteTextAnalyzer2 (with origin) -------------------------------------------------------------------------------- */
static void expect_origin_translation(IDWriteTextAnalyzer2 *a2, int angle, int sideways, float ox, float oy) {
    DWRITE_MATRIX m; int idx = (angle + (sideways ? 1 : 0)) & 3; float edx, edy;
    memset(&m, 0x55, sizeof m);
    CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, (DWRITE_GLYPH_ORIENTATION_ANGLE)angle, sideways, ox, oy, &m), S_OK);
    t_check(mat_linear_eq(&m, &k_rot[idx]), __FILE__, __LINE__, "linear part", "angle %d sw %d: got %s", angle, sideways, mname(&m));
    if (angle + (sideways ? 1 : 0) == 0) { edx = 0.0f; edy = 0.0f; }
    else {
        const DWRITE_MATRIX *r = &k_rot[idx];
        edx = (1.0f - r->m11) * ox - oy * r->m21; edy = (1.0f - r->m22) * oy - ox * r->m12;
    }
    t_check(m.dx == edx && m.dy == edy, __FILE__, __LINE__, "translation", "angle %d sw %d origin (%g,%g): got (%g,%g) want (%g,%g)", angle, sideways, ox, oy, m.dx, m.dy, edx, edy);
}
TEST(ot2_zero_origin_equals_analyzer1) {
    TA_BEGIN(); int i, s;
    for (s = 0; s < 2; s++) for (i = 0; i < 4; i++) {
        DWRITE_MATRIX x, y;
        CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, i, s, &x), S_OK);
        CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, i, s, 0.0f, 0.0f, &y), S_OK);
        CHECK(!memcmp(&x, &y, sizeof x) || (mat_linear_eq(&x, &y) && x.dx == y.dx && x.dy == y.dy));
    }
    TA_END();
}
TEST(ot2_origin_translation_table) {
    TA_BEGIN(); static const float o[][2] = { {10, 20}, {1, 0}, {0, 1}, {-5, 7}, {100.5f, -3.25f}, {0, 0}, {-2, -2}, {1024, 768} };
    int i, k, s;
    for (k = 0; k < (int)(sizeof o / sizeof o[0]); k++) for (s = 0; s < 2; s++) for (i = 0; i < 4; i++) expect_origin_translation(a2, i, s, o[k][0], o[k][1]);
    TA_END();
}
TEST(ot2_closed_forms_90) {
    /* 90 degrees: dx = x + y, dy = y - x */
    TA_BEGIN(); DWRITE_MATRIX m;
    CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES, FALSE, 10.0f, 20.0f, &m), S_OK);
    CHECK_F(m.dx, 30.0f); CHECK_F(m.dy, 10.0f);
    TA_END();
}
TEST(ot2_closed_forms_180) {
    TA_BEGIN(); DWRITE_MATRIX m;
    CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, DWRITE_GLYPH_ORIENTATION_ANGLE_180_DEGREES, FALSE, 10.0f, 20.0f, &m), S_OK);
    CHECK_F(m.dx, 20.0f); CHECK_F(m.dy, 40.0f);
    TA_END();
}
TEST(ot2_closed_forms_270) {
    /* 270 degrees: dx = x - y, dy = y + x */
    TA_BEGIN(); DWRITE_MATRIX m;
    CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES, FALSE, 10.0f, 20.0f, &m), S_OK);
    CHECK_F(m.dx, -10.0f); CHECK_F(m.dy, 30.0f);
    TA_END();
}
TEST(ot2_no_translation_at_zero_degrees) {
    TA_BEGIN(); DWRITE_MATRIX m;
    CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES, FALSE, 123.0f, -45.0f, &m), S_OK);
    CHECK_MAT(m, k_rot[0]);
    TA_END();
}
TEST(ot2_sideways_270_wraps_to_identity_without_translation) {
    /* index 270 + sideways = 4 is non-zero (translation is evaluated) but selects the identity rotation: dx = dy = 0 */
    TA_BEGIN(); DWRITE_MATRIX m;
    CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES, TRUE, 10.0f, 20.0f, &m), S_OK);
    CHECK_MAT(m, k_rot[0]);
    TA_END();
}
TEST(ot2_origin_is_a_fixed_point) {
    /* point (ox, oy) maps to itself under (x, y) -> (x*m11 + y*m21 + dx, x*m12 + y*m22 + dy) */
    TA_BEGIN(); static const float o[][2] = { {10, 20}, {-3, 4}, {64, 64}, {0.5f, 8} }; int i, k, s;
    for (k = 0; k < 4; k++) for (s = 0; s < 2; s++) for (i = 0; i < 4; i++) {
        DWRITE_MATRIX m; float x = o[k][0], y = o[k][1];
        CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, i, s, x, y, &m), S_OK);
        CHECK_F(x * m.m11 + y * m.m21 + m.dx, x); CHECK_F(x * m.m12 + y * m.m22 + m.dy, y);
    }
    TA_END();
}
TEST(ot2_invalid_angle) {
    TA_BEGIN(); static const UINT32 bad[] = { 4, 8, 0x7fffffff, 0xffffffff }; int i;
    for (i = 0; i < (int)(sizeof bad / sizeof *bad); i++) {
        DWRITE_MATRIX m; memset(&m, 0x55, sizeof m);
        CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, (DWRITE_GLYPH_ORIENTATION_ANGLE)bad[i], FALSE, 1.0f, 2.0f, &m), TA_E_INVALIDARG);
        CHECK(memcmp(&m, "\x55\x55\x55\x55", 4) != 0);
    }
    TA_END();
}
TEST(ot2_sideways_is_any_nonzero_bool) {
    TA_BEGIN(); DWRITE_MATRIX x, y;
    CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, 1, TRUE, 3.0f, 4.0f, &x), S_OK);
    CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, 1, 0x100, 3.0f, 4.0f, &y), S_OK);
    CHECK(!memcmp(&x, &y, sizeof x));
    TA_END();
}

/* ---- AnalyzeVerticalGlyphOrientation ---------------------------------------------------------------------------------- */
typedef struct oa { dwx_ana_src *src; dwx_ana_sink *sk; } oa;
static void oa_make(oa *o, const WCHAR *text, UINT32 len, DWRITE_VERTICAL_GLYPH_ORIENTATION ori, UINT8 bidi) {
    o->src = dwx_ana_src_new(text, len); o->sk = dwx_ana_sink_new();
    o->src->support_v1 = 1; o->sk->support_v1 = 1; o->src->orientation = ori; o->src->orient_bidi = bidi;
}
static void oa_free(oa *o) { dwx_release(o->src); dwx_release(o->sk); }
static HRESULT oa_run(IDWriteTextAnalyzer1 *a1, oa *o, UINT32 pos, UINT32 len) {
    return IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(a1, (IDWriteTextAnalysisSource1 *)o->src, pos, len, (IDWriteTextAnalysisSink1 *)o->sk);
}
#define ANGLE_90 DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES
#define ANGLE_0  DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES

TEST(vo_null_source_invalid) {
    TA_BEGIN(); oa o; oa_make(&o, L"A", 1, 0, 0);
    CHECK_HR(IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(a1, NULL, 0, 1, (IDWriteTextAnalysisSink1 *)o.sk), TA_E_INVALIDARG);
    EXPECT_EQ(o.sk->n_orient, 0);
    oa_free(&o); TA_END();
}
TEST(vo_null_sink_invalid) {
    TA_BEGIN(); oa o; oa_make(&o, L"A", 1, 0, 0);
    CHECK_HR(IDWriteTextAnalyzer1_AnalyzeVerticalGlyphOrientation(a1, (IDWriteTextAnalysisSource1 *)o.src, 0, 1, NULL), TA_E_INVALIDARG);
    EXPECT_EQ(o.src->n_orient, 0); EXPECT_EQ(o.src->n_dir, 0); EXPECT_EQ(o.src->n_at, 0);
    oa_free(&o); TA_END();
}
TEST(vo_range_overflow_invalid) {
    TA_BEGIN(); oa o; oa_make(&o, L"AB", 2, 0, 0);
    CHECK_HR(oa_run(a1, &o, 0xFFFFFFFFu, 2), TA_E_INVALIDARG);
    CHECK_HR(oa_run(a1, &o, 0x80000000u, 0x80000000u), TA_E_INVALIDARG);
    CHECK_HR(oa_run(a1, &o, 1, 0xFFFFFFFFu), TA_E_INVALIDARG);
    EXPECT_EQ(o.src->n_orient, 0); EXPECT_EQ(o.src->n_dir, 0); EXPECT_EQ(o.sk->n_orient, 0);
    oa_free(&o); TA_END();
}
TEST(vo_zero_length_queries_direction_only) {
    TA_BEGIN(); oa o; oa_make(&o, L"AB", 2, 0, 0);
    CHECK_HR(oa_run(a1, &o, 0, 0), S_OK);
    EXPECT_EQ(o.src->n_dir, 1); EXPECT_EQ(o.src->n_at, 0); EXPECT_EQ(o.src->n_orient, 0); EXPECT_EQ(o.sk->n_orient, 0);
    oa_free(&o); TA_END();
}
TEST(vo_reading_direction_queried_once) {
    TA_BEGIN(); oa o; oa_make(&o, L"ABCDEFGH", 8, 0, 0); o.src->orient_run = 2;
    CHECK_HR(oa_run(a1, &o, 0, 8), S_OK);
    EXPECT_EQ(o.src->n_dir, 1);
    oa_free(&o); TA_END();
}
TEST(vo_source_queried_per_run) {
    TA_BEGIN(); oa o; int i, n = 0; UINT32 pos[8]; oa_make(&o, L"ABCDEFGH", 8, 0, 0); o.src->orient_run = 3;
    CHECK_HR(oa_run(a1, &o, 0, 8), S_OK);
    for (i = 0; i < o.src->log_n; i++) if (o.src->log[i].fn == 6 && n < 8) pos[n++] = o.src->log[i].pos;
    EXPECT_EQ(o.src->n_orient, 3); CHECK(n == 3);
    if (n == 3) { EXPECT_EQ(pos[0], 0); EXPECT_EQ(pos[1], 3); EXPECT_EQ(pos[2], 6); }
    oa_free(&o); TA_END();
}
TEST(vo_covers_exact_range_with_offset) {
    TA_BEGIN(); oa o; UINT32 i; oa_make(&o, L"ABCDEFGH", 8, 0, 0);
    CHECK_HR(oa_run(a1, &o, 2, 4), S_OK);
    for (i = 0; i < 8; i++) { if (i >= 2 && i < 6) CHECK(o.sk->orient_set[i]); else CHECK(!o.sk->orient_set[i]); }
    for (i = 0; i < 8; i++) if (o.src->log[i].fn == 6) { CHECK(o.src->log[i].pos >= 2); break; }
    oa_free(&o); TA_END();
}
TEST(vo_latin_default_is_sideways_angle0) {
    TA_BEGIN(); oa o; UINT32 i; oa_make(&o, L"Hello 123", 9, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0);
    CHECK_HR(oa_run(a1, &o, 0, 9), S_OK);
    for (i = 0; i < 5; i++) { CHECK(o.sk->orient_set[i]); EXPECT_EQ(o.sk->angle[i], ANGLE_0); CHECK(o.sk->orient_sideways[i]); CHECK(!o.sk->orient_rtl[i]); EXPECT_EQ(o.sk->orient_bidi[i], 0); }
    for (i = 6; i < 9; i++) { CHECK(o.sk->orient_set[i]); CHECK(o.sk->orient_sideways[i]); }   /* digits are rotated like Latin letters */
    oa_free(&o); TA_END();
}
TEST(vo_cjk_default_is_upright_angle90) {
    static const WCHAR t[] = { 0x4E00, 0x4E8C, 0x4E09, 0x56DB, 0x4E94 };
    TA_BEGIN(); oa o; UINT32 i; oa_make(&o, t, 5, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0);
    CHECK_HR(oa_run(a1, &o, 0, 5), S_OK);
    for (i = 0; i < 5; i++) { CHECK(o.sk->orient_set[i]); EXPECT_EQ(o.sk->angle[i], ANGLE_90); CHECK(!o.sk->orient_sideways[i]); CHECK(!o.sk->orient_rtl[i]); }
    EXPECT_EQ(o.sk->n_orient, 1);                       /* identical info coalesces into one callback */
    oa_free(&o); TA_END();
}
TEST(vo_kana_hangul_default_upright) {
    static const WCHAR t[] = { 0x3042, 0x30A2, 0xAC00, 0xD55C };       /* hiragana, katakana, hangul syllables */
    TA_BEGIN(); oa o; UINT32 i; oa_make(&o, t, 4, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0);
    CHECK_HR(oa_run(a1, &o, 0, 4), S_OK);
    for (i = 0; i < 4; i++) { EXPECT_EQ(o.sk->angle[i], ANGLE_90); CHECK(!o.sk->orient_sideways[i]); }
    oa_free(&o); TA_END();
}
TEST(vo_mixed_latin_cjk_splits_runs) {
    static const WCHAR t[] = { 'A', 'B', 0x4E00, 0x4E8C, 'C' };
    TA_BEGIN(); oa o; oa_make(&o, t, 5, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0);
    CHECK_HR(oa_run(a1, &o, 0, 5), S_OK);
    EXPECT_EQ(o.sk->n_orient, 3);
    CHECK(o.sk->orient_sideways[0] && o.sk->orient_sideways[1] && !o.sk->orient_sideways[2] && !o.sk->orient_sideways[3] && o.sk->orient_sideways[4]);
    oa_free(&o); TA_END();
}
TEST(vo_surrogate_pair_is_one_unit) {
    /* U+20000 (CJK extension B) is upright; both UTF-16 units get the same info and no run boundary splits the pair */
    static const WCHAR t[] = { 'A', 0xD840, 0xDC00, 'B' };
    TA_BEGIN(); oa o; oa_make(&o, t, 4, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0);
    CHECK_HR(oa_run(a1, &o, 0, 4), S_OK);
    CHECK(o.sk->orient_set[1] && o.sk->orient_set[2]);
    CHECK(o.sk->orient_sideways[1] == o.sk->orient_sideways[2]); EXPECT_EQ(o.sk->angle[1], o.sk->angle[2]);
    CHECK(!o.sk->orient_sideways[1]); EXPECT_EQ(o.sk->angle[1], ANGLE_90);
    oa_free(&o); TA_END();
}
TEST(vo_emoji_default_upright_note) {
    static const WCHAR t[] = { 0xD83D, 0xDE00 };      /* U+1F600 */
    TA_BEGIN(); oa o; oa_make(&o, t, 2, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0);
    CHECK_HR(oa_run(a1, &o, 0, 2), S_OK);
    CHECK(o.sk->orient_set[0] && o.sk->orient_set[1]); CHECK(o.sk->orient_sideways[0] == o.sk->orient_sideways[1]);
    t_note("emoji U+1F600 DEFAULT: angle=%d sideways=%d", o.sk->angle[0], o.sk->orient_sideways[0]);
    oa_free(&o); TA_END();
}
TEST(vo_punctuation_note) {
    static const WCHAR t[] = { 0x3001, 0x3002, 0x300C, 0x300D, 0xFF08, 0xFF09, 0x30FC, '(', ')', '.', ',', '-' };
    TA_BEGIN(); oa o; UINT32 i; oa_make(&o, t, 12, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0);
    CHECK_HR(oa_run(a1, &o, 0, 12), S_OK);
    for (i = 0; i < 12; i++) { CHECK(o.sk->orient_set[i]); t_note("U+%04X DEFAULT: angle=%d sideways=%d", t[i], o.sk->angle[i], o.sk->orient_sideways[i]); }
    /* ASCII brackets and punctuation are rotated with the Latin text around them */
    CHECK(o.sk->orient_sideways[7] && o.sk->orient_sideways[8]);
    oa_free(&o); TA_END();
}
TEST(vo_default_bidi_level_passthrough) {
    TA_BEGIN(); UINT8 lv;
    for (lv = 0; lv < 8; lv++) {
        oa o; oa_make(&o, L"AB", 2, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, lv);
        CHECK_HR(oa_run(a1, &o, 0, 2), S_OK);
        EXPECT_EQ(o.sk->orient_bidi[0], lv); EXPECT_EQ(o.sk->orient_rtl[0], (lv & 1)); EXPECT_EQ(o.sk->orient_rtl[1], (lv & 1));
        oa_free(&o);
    }
    TA_END();
}
TEST(vo_default_bidi_level_cjk) {
    static const WCHAR t[] = { 0x4E00 };
    TA_BEGIN(); UINT8 lv;
    for (lv = 0; lv < 4; lv++) {
        oa o; oa_make(&o, t, 1, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, lv);
        CHECK_HR(oa_run(a1, &o, 0, 1), S_OK);
        EXPECT_EQ(o.sk->orient_bidi[0], lv); EXPECT_EQ(o.sk->orient_rtl[0], (lv & 1)); EXPECT_EQ(o.sk->angle[0], ANGLE_90); CHECK(!o.sk->orient_sideways[0]);
        oa_free(&o);
    }
    TA_END();
}
TEST(vo_stacked_latin_even_levels) {
    TA_BEGIN(); static const UINT8 in[] = { 0, 2, 4, 60 }; static const UINT8 out[] = { 0, 2, 4, 60 }; int i;
    for (i = 0; i < 4; i++) {
        oa o; oa_make(&o, L"A", 1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, in[i]);
        CHECK_HR(oa_run(a1, &o, 0, 1), S_OK);
        EXPECT_EQ(o.sk->orient_bidi[0], out[i]); CHECK(!o.sk->orient_rtl[0]); CHECK(!o.sk->orient_sideways[0]); EXPECT_EQ(o.sk->angle[0], ANGLE_90);
        oa_free(&o);
    }
    TA_END();
}
TEST(vo_stacked_latin_odd_levels) {
    /* odd level: rotated 270 degrees, right-to-left, level rounded to the next even value */
    TA_BEGIN(); static const UINT8 in[] = { 1, 3, 5, 61 }; static const UINT8 out[] = { 2, 4, 6, 62 }; int i;
    for (i = 0; i < 4; i++) {
        oa o; oa_make(&o, L"A", 1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, in[i]);
        CHECK_HR(oa_run(a1, &o, 0, 1), S_OK);
        EXPECT_EQ(o.sk->orient_bidi[0], out[i]); CHECK(o.sk->orient_rtl[0]); CHECK(!o.sk->orient_sideways[0]);
        EXPECT_EQ(o.sk->angle[0], DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES);
        oa_free(&o);
    }
    TA_END();
}
TEST(vo_stacked_level_saturation) {
    /* levels above 61 are not incremented before the low bit is cleared */
    TA_BEGIN(); static const UINT8 in[] = { 62, 63, 64, 125 }; static const UINT8 out[] = { 62, 62, 64, 124 }; int i;
    for (i = 0; i < 4; i++) {
        oa o; oa_make(&o, L"A", 1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, in[i]);
        CHECK_HR(oa_run(a1, &o, 0, 1), S_OK);
        EXPECT_EQ(o.sk->orient_bidi[0], out[i]);
        oa_free(&o);
    }
    TA_END();
}
TEST(vo_stacked_cjk_same_as_latin) {
    static const WCHAR t[] = { 0x4E00 };
    TA_BEGIN(); oa o; oa_make(&o, t, 1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0);
    CHECK_HR(oa_run(a1, &o, 0, 1), S_OK);
    CHECK(!o.sk->orient_sideways[0]); EXPECT_EQ(o.sk->angle[0], ANGLE_90); EXPECT_EQ(o.sk->orient_bidi[0], 0);
    oa_free(&o); TA_END();
}
TEST(vo_stacked_mixed_text_single_run) {
    /* in STACKED mode Latin and CJK get identical info, so the whole text is reported in one callback */
    static const WCHAR t[] = { 'A', 'B', 0x4E00, 0x4E8C, 'C' };
    TA_BEGIN(); oa o; oa_make(&o, t, 5, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0);
    CHECK_HR(oa_run(a1, &o, 0, 5), S_OK);
    EXPECT_EQ(o.sk->n_orient, 1);
    oa_free(&o); TA_END();
}
TEST(vo_unknown_orientation_value_gives_zero_info) {
    TA_BEGIN(); static const UINT32 vals[] = { 2, 3, 100, 0xffffffffu }; int i;
    for (i = 0; i < 4; i++) {
        oa o; oa_make(&o, L"A", 1, (DWRITE_VERTICAL_GLYPH_ORIENTATION)vals[i], 5);
        CHECK_HR(oa_run(a1, &o, 0, 1), S_OK);
        CHECK(o.sk->orient_set[0]); EXPECT_EQ(o.sk->angle[0], ANGLE_0); EXPECT_EQ(o.sk->orient_bidi[0], 0); CHECK(!o.sk->orient_sideways[0]); CHECK(!o.sk->orient_rtl[0]);
        oa_free(&o);
    }
    TA_END();
}
TEST(vo_same_info_across_source_runs_coalesces) {
    TA_BEGIN(); oa o; oa_make(&o, L"ABCDEFGH", 8, 0, 0); o.src->orient_run = 2;
    CHECK_HR(oa_run(a1, &o, 0, 8), S_OK);
    EXPECT_EQ(o.src->n_orient, 4); EXPECT_EQ(o.sk->n_orient, 1);
    oa_free(&o); TA_END();
}
TEST(vo_sink_calls_in_ascending_position_order) {
    /* the dwx sink only keeps per-position data: check complete, non-overlapping coverage of a mixed string */
    static const WCHAR t[] = { 'a', 0x4E00, 'b', 0x4E8C, 'c', 0x4E09 };
    TA_BEGIN(); oa o; UINT32 i; oa_make(&o, t, 6, 0, 0);
    CHECK_HR(oa_run(a1, &o, 0, 6), S_OK);
    EXPECT_EQ(o.sk->n_orient, 6); for (i = 0; i < 6; i++) CHECK(o.sk->orient_set[i]);
    oa_free(&o); TA_END();
}
TEST(vo_text_fetched_through_source) {
    TA_BEGIN(); oa o; oa_make(&o, L"ABC", 3, 0, 0);
    CHECK_HR(oa_run(a1, &o, 0, 3), S_OK);
    CHECK(o.src->n_at >= 1);
    oa_free(&o); TA_END();
}
TEST(vo_chunked_text_source) {
    /* the text may be handed out in small pieces */
    static const WCHAR t[] = { 'A', 'B', 0x4E00, 0x4E8C, 'C', 'D' };
    TA_BEGIN(); oa o; oa_make(&o, t, 6, 0, 0); o.src->chunk = 1;
    CHECK_HR(oa_run(a1, &o, 0, 6), S_OK);
    CHECK(o.sk->orient_sideways[0] && o.sk->orient_sideways[1] && !o.sk->orient_sideways[2] && !o.sk->orient_sideways[3] && o.sk->orient_sideways[4] && o.sk->orient_sideways[5]);
    EXPECT_EQ(o.sk->n_orient, 3);
    oa_free(&o); TA_END();
}
TEST(vo_refcounts_balanced) {
    TA_BEGIN(); oa o; oa_make(&o, L"AB", 2, 0, 0);
    CHECK_HR(oa_run(a1, &o, 0, 2), S_OK);
    EXPECT_EQ(dwx_refs(o.src), 1); EXPECT_EQ(dwx_refs(o.sk), 1);
    oa_free(&o); TA_END();
}
TEST(vo_through_analyzer2_interface) {
    TA_BEGIN(); oa o; oa_make(&o, L"AB", 2, 0, 0);
    CHECK_HR(IDWriteTextAnalyzer2_AnalyzeVerticalGlyphOrientation(a2, (IDWriteTextAnalysisSource1 *)o.src, 0, 2, (IDWriteTextAnalysisSink1 *)o.sk), S_OK);
    CHECK(o.sk->orient_set[0] && o.sk->orient_set[1]);
    oa_free(&o); TA_END();
}
