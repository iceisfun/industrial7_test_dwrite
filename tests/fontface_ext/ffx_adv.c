/* IDWriteFontFace1: GetDesignGlyphAdvances, GetGdiCompatibleGlyphAdvances, GetKerningPairAdjustments / HasKerningPairs,
 * GetVerticalGlyphVariants / HasVerticalGlyphVariants.
 *   GetDesignGlyphAdvances (dwrite.dll!0x68d90): advances zeroed first; sideways on raw CFF -> DWRITE_E_UNSUPPORTEDOPERATION;
 *       glyphCount != 0 with NULL indices -> E_INVALIDARG.
 *   GetGdiCompatibleGlyphAdvances (!0xdc300): advances zeroed first; glyphCount != 0 && !indices, emSize < 0, pixelsPerDip <= 0, NaN in the
 *       transform -> E_INVALIDARG; emSize == 0 -> S_OK with zeros.
 *   GetKerningPairAdjustments (!0x24eb0): zeroed first; count != 0 && !indices -> E_INVALIDARG; adjustment[i] belongs to the pair (i, i+1),
 *       the last entry is always 0; only the legacy 'kern' table (format 0, horizontal) is consulted (OpenTypeKerningPairs, !0x7a95c).
 *   GetVerticalGlyphVariants (!0xdd150, FontFace!0xfc408): count != 0 && !nominal -> vertical zero-filled, E_INVALIDARG; otherwise copy
 *       the input and substitute through the font's vertical-variant map; in-place is allowed. */
#include "ffx.h"

#define NSB ((HRESULT)0x8007007A)
#define EINVAL ((HRESULT)0x80070057)

static HRESULT design_adv(ffx *x, UINT32 n, const UINT16 *g, INT32 *a, BOOL sw) { return IDWriteFontFace1_GetDesignGlyphAdvances(x->f1, n, g, a, sw); }
static int default_adv(UINT16 g) { return g == 0 ? 1200 : g == 67 ? 500 : 1000; }

/* ---- design advances ----------------------------------------------------------------------------------------------- */
TEST(da_default_font_values) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[] = { 0, 1, 2, 27, 28, 53, 54, 63, 64, 65, 66, 67, 68 }; INT32 a[13]; int i;
    CHECK_HR_OK(design_adv(&x, 13, g, a, FALSE));
    for (i = 0; i < 13; i++) t_check(a[i] == default_adv(g[i]), __FILE__, __LINE__, "advance", "glyph %u: %d want %d", g[i], a[i], default_adv(g[i]));
    ffx_close(&x);
}
TEST(da_every_glyph_matches_design_metrics) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[TTF_DEF_NGLYPHS]; INT32 a[TTF_DEF_NGLYPHS]; DWRITE_GLYPH_METRICS gm[TTF_DEF_NGLYPHS]; int i;
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) g[i] = (UINT16)i;
    CHECK_HR_OK(design_adv(&x, TTF_DEF_NGLYPHS, g, a, FALSE));
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(x.f0, g, TTF_DEF_NGLYPHS, gm, FALSE));
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) t_check(a[i] == (INT32)gm[i].advanceWidth, __FILE__, __LINE__, "adv vs metrics", "glyph %d: %d vs %u", i, a[i], gm[i].advanceWidth);
    ffx_close(&x);
}
TEST(da_zero_count_with_null_pointers) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    CHECK_HR_OK(design_adv(&x, 0, NULL, NULL, FALSE));
    CHECK_HR_OK(design_adv(&x, 0, NULL, NULL, TRUE));
    UINT16 g = 2; CHECK_HR_OK(design_adv(&x, 0, &g, NULL, FALSE));
    ffx_close(&x);
}
TEST(da_null_indices_zero_output_invalid) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    INT32 a[5]; memset(a, 0xCC, sizeof a);
    CHECK_HR(design_adv(&x, 5, NULL, a, FALSE), EINVAL);
    int i, bad = 0; for (i = 0; i < 5; i++) if (a[i] != 0) bad++;
    EXPECT_EQ(bad, 0);
    memset(a, 0xCC, sizeof a);
    CHECK_HR(design_adv(&x, 5, NULL, a, TRUE), EINVAL);
    for (i = 0, bad = 0; i < 5; i++) if (a[i] != 0) bad++;
    EXPECT_EQ(bad, 0);
    ffx_close(&x);
}
TEST(da_single_and_pair_requests_consistent) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[3] = { 2, 67, 0 }; INT32 all[3], one[3]; int i;
    CHECK_HR_OK(design_adv(&x, 3, g, all, FALSE));
    for (i = 0; i < 3; i++) { one[i] = 0x7777; CHECK_HR_OK(design_adv(&x, 1, &g[i], &one[i], FALSE)); EXPECT_EQ(one[i], all[i]); }
    ffx_close(&x);
}
TEST(da_repeated_and_unsorted_glyphs) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[12] = { 67, 67, 0, 2, 0, 67, 66, 1, 1, 68, 64, 0 }; INT32 a[12]; int i;
    CHECK_HR_OK(design_adv(&x, 12, g, a, FALSE));
    for (i = 0; i < 12; i++) t_check(a[i] == default_adv(g[i]), __FILE__, __LINE__, "unsorted", "pos %d glyph %u: %d", i, g[i], a[i]);
    ffx_close(&x);
}
TEST(da_large_batch_100) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[100]; INT32 a[100]; unsigned seed = 12345; int i;
    for (i = 0; i < 100; i++) { seed = seed * 1103515245u + 12345u; g[i] = (UINT16)((seed >> 16) % TTF_DEF_NGLYPHS); }
    CHECK_HR_OK(design_adv(&x, 100, g, a, FALSE));
    int bad = 0; for (i = 0; i < 100; i++) if (a[i] != default_adv(g[i])) bad++;
    EXPECT_EQ(bad, 0);
    ffx_close(&x);
}
TEST(da_compressed_hmtx_repeats_last_advance) {
    int n; const ttf_glyph *dg = ttf_default_glyphs(&n); ttf_glyph gl[TTF_DEF_NGLYPHS]; int i;
    memcpy(gl, dg, sizeof gl);
    gl[67].adv = 1000;         /* make every glyph from 1 on share one advance so hmtx is stored compressed */
    ttf_spec s; ttf_default_spec(&s); s.glyphs = gl; s.compress_hmtx = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[TTF_DEF_NGLYPHS]; INT32 a[TTF_DEF_NGLYPHS];
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) g[i] = (UINT16)i;
    CHECK_HR_OK(design_adv(&x, TTF_DEF_NGLYPHS, g, a, FALSE));
    int bad = 0; for (i = 0; i < TTF_DEF_NGLYPHS; i++) if (a[i] != (i == 0 ? 1200 : 1000)) bad++;
    EXPECT_EQ(bad, 0);
    ffx_close(&x);
}
TEST(da_custom_advances_follow_hmtx) {
    int n; const ttf_glyph *dg = ttf_default_glyphs(&n); ttf_glyph gl[TTF_DEF_NGLYPHS]; int i;
    memcpy(gl, dg, sizeof gl);
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) gl[i].adv = (unsigned short)(300 + i * 17);
    ttf_spec s; ttf_default_spec(&s); s.glyphs = gl;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[TTF_DEF_NGLYPHS]; INT32 a[TTF_DEF_NGLYPHS];
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) g[i] = (UINT16)i;
    CHECK_HR_OK(design_adv(&x, TTF_DEF_NGLYPHS, g, a, FALSE));
    /* glyph 66 is a composite flagged USE_MY_METRICS: only hinted metrics may follow its component, so it is left out */
    int bad = 0; for (i = 0; i < TTF_DEF_NGLYPHS; i++) if (i != 66 && a[i] != 300 + i * 17) bad++;
    EXPECT_EQ(bad, 0);
    ffx_close(&x);
}
TEST(da_sideways_succeeds_on_truetype) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[3] = { 2, 0, 67 }; INT32 a[3], b[3];
    CHECK_HR_OK(design_adv(&x, 3, g, a, FALSE));
    memset(b, 0xCC, sizeof b);
    CHECK_HR_OK(design_adv(&x, 3, g, b, TRUE));
    t_note("sideways advances for A, notdef, empty500: %d %d %d (upright %d %d %d)", b[0], b[1], b[2], a[0], a[1], a[2]);
    int i; for (i = 0; i < 3; i++) CHECK(b[i] != (INT32)0xCCCCCCCC);
    ffx_close(&x);
}
TEST(da_sideways_matches_sideways_design_metrics) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[4] = { 2, 0, 67, 1 }; INT32 a[4]; DWRITE_GLYPH_METRICS gm[4]; int i;
    CHECK_HR_OK(design_adv(&x, 4, g, a, TRUE));
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(x.f0, g, 4, gm, TRUE));
    for (i = 0; i < 4; i++) t_note("glyph %u sideways: advance %d, metrics advanceWidth %u advanceHeight %u", g[i], a[i], gm[i].advanceWidth, gm[i].advanceHeight);
    /* sideways: the advance reported is the glyph's vertical advance, i.e. DWRITE_GLYPH_METRICS.advanceHeight */
    for (i = 0; i < 4; i++) t_check(a[i] == (INT32)gm[i].advanceHeight, __FILE__, __LINE__, "sideways", "glyph %u: %d vs advanceHeight %u", g[i], a[i], gm[i].advanceHeight);
    ffx_close(&x);
}
TEST(da_sideways_with_vertical_metrics_font) {
    ttf_spec s; ffx_spec_vert(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[3] = { 2, 0, 67 }; INT32 a[3]; DWRITE_GLYPH_METRICS gm[3]; int i;
    CHECK_HR_OK(design_adv(&x, 3, g, a, TRUE));
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(x.f0, g, 3, gm, TRUE));
    /* vmtx advance of every glyph is unitsPerEm (2048) in the generator */
    for (i = 0; i < 3; i++) t_check(a[i] == (INT32)gm[i].advanceHeight, __FILE__, __LINE__, "vmtx", "glyph %u: %d vs %u", g[i], a[i], gm[i].advanceHeight);
    for (i = 0; i < 3; i++) t_check(a[i] == 2048, __FILE__, __LINE__, "vmtx", "glyph %u sideways advance %d", g[i], a[i]);
    ffx_close(&x);
}
TEST(da_bold_simulation_adds_embolden_delta) {
    /* FontFace::GetDesignGlyphAdvances (!0x68e40) with the bold simulation: every glyph that has a box and a non-zero advance grows by
     * (unitsPerEm + 25) / 50 design units (41 at 2048); empty glyphs are unchanged. The same delta applies to DWRITE_GLYPH_METRICS.advanceWidth. */
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_BOLD) : 0;
    if (oa && ob) {
        UINT16 g[6] = { 2, 28, 64, 68, 1, 67 }; INT32 r[6], bo[6]; DWRITE_GLYPH_METRICS gm[6]; int i;
        CHECK_HR_OK(design_adv(&a, 6, g, r, FALSE)); CHECK_HR_OK(design_adv(&b, 6, g, bo, FALSE));
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(b.f0, g, 6, gm, FALSE));
        for (i = 0; i < 4; i++) t_check(bo[i] == r[i] + 41, __FILE__, __LINE__, "bold delta", "glyph %u: %d vs %d + 41", g[i], bo[i], r[i]);
        for (i = 4; i < 6; i++) t_check(bo[i] == r[i], __FILE__, __LINE__, "bold empty", "glyph %u: %d vs %d", g[i], bo[i], r[i]);
        for (i = 0; i < 6; i++) t_check(bo[i] == (INT32)gm[i].advanceWidth, __FILE__, __LINE__, "bold metrics", "glyph %u: %d vs %u", g[i], bo[i], gm[i].advanceWidth);
    }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(da_bold_simulation_never_narrower) {
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_BOLD) : 0;
    if (oa && ob) {
        UINT16 g[5] = { 2, 28, 1, 0, 67 }; INT32 r[5], bo[5]; int i;
        CHECK_HR_OK(design_adv(&a, 5, g, r, FALSE)); CHECK_HR_OK(design_adv(&b, 5, g, bo, FALSE));
        for (i = 0; i < 5; i++) { CHECK(bo[i] >= r[i]); t_note("glyph %u: regular %d, bold simulation %d", g[i], r[i], bo[i]); }
    }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(da_oblique_simulation_same_advances) {
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_OBLIQUE) : 0;
    if (oa && ob) {
        UINT16 g[5] = { 2, 28, 1, 0, 67 }; INT32 r[5], o[5];
        CHECK_HR_OK(design_adv(&a, 5, g, r, FALSE)); CHECK_HR_OK(design_adv(&b, 5, g, o, FALSE));
        CHECK(!memcmp(r, o, sizeof r));
    }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
static void check_sys_adv(const WCHAR *fam, const char *chars) {
    ffx x; if (!ffx_open_sys(&x, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cp[64]; UINT16 g[64]; INT32 a[64]; DWRITE_GLYPH_METRICS gm[64]; int n = (int)strlen(chars), i;
    for (i = 0; i < n; i++) cp[i] = (unsigned char)chars[i];
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.f0, cp, n, g));
    CHECK_HR_OK(design_adv(&x, n, g, a, FALSE));
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(x.f0, g, n, gm, FALSE));
    int bad = 0; for (i = 0; i < n; i++) if (a[i] != (INT32)gm[i].advanceWidth) bad++;
    EXPECT_EQ(bad, 0);
    t_note("first advances: %d %d %d", a[0], n > 1 ? a[1] : 0, n > 2 ? a[2] : 0);
    ffx_close(&x);
}
TEST(da_system_arial) { check_sys_adv(L"Arial", "Hello, World 0123456789 !?"); }
TEST(da_system_times) { check_sys_adv(L"Times New Roman", "AVATAR fi 0123456789"); }
TEST(da_system_segoe_ui) { check_sys_adv(L"Segoe UI", "The quick brown fox"); }
TEST(da_system_consolas_digits_equal) {
    ffx x; if (!ffx_open_sys(&x, L"Consolas", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cp[12]; UINT16 g[12]; INT32 a[12]; int i;
    for (i = 0; i < 10; i++) cp[i] = '0' + i; cp[10] = 'W'; cp[11] = 'i';
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.f0, cp, 12, g));
    CHECK_HR_OK(design_adv(&x, 12, g, a, FALSE));
    for (i = 1; i < 12; i++) EXPECT_EQ(a[i], a[0]);
    ffx_close(&x);
}
TEST(da_arial_regular_advances_note) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cp[3] = { ' ', 'A', '0' }; UINT16 g[3]; INT32 a[3];
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.f0, cp, 3, g)); CHECK_HR_OK(design_adv(&x, 3, g, a, FALSE));
    t_note("Arial advances (2048 upm): space %d A %d zero %d", a[0], a[1], a[2]);
    EXPECT_EQ(a[0], 569); EXPECT_EQ(a[1], 1366); EXPECT_EQ(a[2], 1139);
    ffx_close(&x);
}

/* ---- GDI compatible glyph advances ---------------------------------------------------------------------------------- */
static HRESULT gdi_adv(ffx *x, float em, float ppd, const DWRITE_MATRIX *t, BOOL nat, BOOL sw, UINT32 n, const UINT16 *g, INT32 *a) {
    return IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(x->f1, em, ppd, t, nat, sw, n, g, a);
}
TEST(ga_invalid_arguments_zero_output) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[3] = { 2, 3, 4 }; INT32 a[3];
    struct { float em, ppd; int nullidx; const char *what; } c[] = {
        { -1.0f, 1.0f, 0, "negative em" }, { 16.0f, 0.0f, 0, "zero ppd" }, { 16.0f, -2.0f, 0, "negative ppd" }, { 16.0f, 1.0f, 1, "null indices" },
        { -0.5f, 0.5f, 0, "negative em, small ppd" }, { 0.0f, 0.0f, 0, "zero em and ppd" }, { -100.0f, -100.0f, 1, "everything wrong" },
    };
    int i, j;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        memset(a, 0xCC, sizeof a);
        HRESULT hr = gdi_adv(&x, c[i].em, c[i].ppd, NULL, FALSE, FALSE, 3, c[i].nullidx ? NULL : g, a);
        t_check(hr == EINVAL, __FILE__, __LINE__, c[i].what, "hr %s", dw_hr_name(hr));
        for (j = 0; j < 3; j++) t_check(a[j] == 0, __FILE__, __LINE__, c[i].what, "advance %d not zeroed (%d)", j, a[j]);
    }
    ffx_close(&x);
}
TEST(ga_zero_em_is_success_with_zero_advances) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[3] = { 2, 3, 0 }; INT32 a[3]; int i;
    memset(a, 0xCC, sizeof a);
    CHECK_HR_OK(gdi_adv(&x, 0.0f, 1.0f, NULL, FALSE, FALSE, 3, g, a));
    for (i = 0; i < 3; i++) EXPECT_EQ(a[i], 0);
    memset(a, 0xCC, sizeof a);
    CHECK_HR_OK(gdi_adv(&x, 0.0f, 1.0f, NULL, TRUE, TRUE, 3, g, a));
    for (i = 0; i < 3; i++) EXPECT_EQ(a[i], 0);
    ffx_close(&x);
}
TEST(ga_zero_count) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    CHECK_HR_OK(gdi_adv(&x, 16.0f, 1.0f, NULL, FALSE, FALSE, 0, NULL, NULL));
    ffx_close(&x);
}
TEST(ga_nan_transform_rejected) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[2] = { 2, 3 }; INT32 a[2]; float nan = nanf(""); int i;
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX t = { 1, 0, 0, 1, 0, 0 }; ((float *)&t)[i] = nan; memset(a, 0xCC, sizeof a);
        HRESULT hr = gdi_adv(&x, 16.0f, 1.0f, &t, FALSE, FALSE, 2, g, a);
        t_check(hr == EINVAL && a[0] == 0 && a[1] == 0, __FILE__, __LINE__, "NaN transform", "element %d: %s", i, dw_hr_name(hr));
    }
    ffx_close(&x);
}
TEST(ga_classic_rounds_to_whole_pixels_16ppem) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[5] = { 2, 28, 1, 0, 67 }; INT32 a[5]; int i;
    CHECK_HR_OK(gdi_adv(&x, 16.0f, 1.0f, NULL, FALSE, FALSE, 5, g, a));
    /* 128 design units per pixel at 16 ppem; the advance is a whole number of pixels, nearest to the design advance */
    for (i = 0; i < 5; i++) {
        int d = default_adv(g[i]);
        t_check(a[i] % 128 == 0 && abs(a[i] - d) <= 64, __FILE__, __LINE__, "classic", "glyph %u: %d (design %d)", g[i], a[i], d);
    }
    EXPECT_EQ(a[0], 1024);
    ffx_close(&x);
}
TEST(ga_classic_multiple_ppem_grid) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const int ppem[] = { 8, 16, 32, 64, 128 };
    UINT16 g[4] = { 2, 0, 67, 1 }; int k, i;
    for (k = 0; k < 5; k++) {
        INT32 a[4]; CHECK_HR_OK(gdi_adv(&x, (float)ppem[k], 1.0f, NULL, FALSE, FALSE, 4, g, a));
        int unit = 2048 / ppem[k];
        for (i = 0; i < 4; i++) {
            int d = default_adv(g[i]);
            t_check(a[i] % unit == 0 && abs(a[i] - d) <= unit / 2 + 1, __FILE__, __LINE__, "classic grid", "ppem %d glyph %u: %d (design %d)", ppem[k], g[i], a[i], d);
        }
    }
    ffx_close(&x);
}
TEST(ga_identity_at_upm_equals_design) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[5] = { 2, 28, 1, 0, 67 }; INT32 a[5], d[5];
    CHECK_HR_OK(gdi_adv(&x, 2048.0f, 1.0f, NULL, FALSE, FALSE, 5, g, a));
    CHECK_HR_OK(design_adv(&x, 5, g, d, FALSE));
    CHECK(!memcmp(a, d, sizeof a));
    ffx_close(&x);
}
TEST(ga_pixels_per_dip_scales) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[4] = { 2, 0, 67, 1 }; INT32 a[4], b[4];
    CHECK_HR_OK(gdi_adv(&x, 16.0f, 1.0f, NULL, FALSE, FALSE, 4, g, a));
    CHECK_HR_OK(gdi_adv(&x, 8.0f, 2.0f, NULL, FALSE, FALSE, 4, g, b));
    CHECK(!memcmp(a, b, sizeof a));
    ffx_close(&x);
}
TEST(ga_scale_transform_equals_bigger_em) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[4] = { 2, 0, 67, 1 }; INT32 a[4], b[4]; DWRITE_MATRIX t = { 2, 0, 0, 2, 0, 0 };
    CHECK_HR_OK(gdi_adv(&x, 16.0f, 1.0f, NULL, FALSE, FALSE, 4, g, a));
    CHECK_HR_OK(gdi_adv(&x, 8.0f, 1.0f, &t, FALSE, FALSE, 4, g, b));
    CHECK(!memcmp(a, b, sizeof a));
    ffx_close(&x);
}
TEST(ga_batch_equals_single_calls) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[6] = { 2, 28, 1, 0, 67, 66 }; INT32 all[6]; int i;
    CHECK_HR_OK(gdi_adv(&x, 13.0f, 1.0f, NULL, FALSE, FALSE, 6, g, all));
    for (i = 0; i < 6; i++) { INT32 one = 0x7777; CHECK_HR_OK(gdi_adv(&x, 13.0f, 1.0f, NULL, FALSE, FALSE, 1, &g[i], &one)); t_check(one == all[i], __FILE__, __LINE__, "batch", "glyph %u: %d vs %d", g[i], one, all[i]); }
    ffx_close(&x);
}
TEST(ga_natural_vs_classic_note) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[3] = { 2, 0, 67 }; INT32 c[3], n[3]; int i;
    CHECK_HR_OK(gdi_adv(&x, 11.0f, 1.0f, NULL, FALSE, FALSE, 3, g, c));
    CHECK_HR_OK(gdi_adv(&x, 11.0f, 1.0f, NULL, TRUE, FALSE, 3, g, n));
    for (i = 0; i < 3; i++) t_note("11 ppem glyph %u: classic %d natural %d (design %d)", g[i], c[i], n[i], default_adv(g[i]));
    for (i = 0; i < 3; i++) CHECK(abs(n[i] - default_adv(g[i])) <= 2048 / 11 && abs(c[i] - default_adv(g[i])) <= 2048 / 11);
    ffx_close(&x);
}
TEST(ga_sideways_succeeds) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[3] = { 2, 0, 67 }; INT32 a[3]; int i;
    CHECK_HR_OK(gdi_adv(&x, 16.0f, 1.0f, NULL, FALSE, TRUE, 3, g, a));
    for (i = 0; i < 3; i++) t_note("sideways gdi advance glyph %u: %d", g[i], a[i]);
    ffx_close(&x);
}
TEST(ga_arial_12pt_advances_are_whole_pixels) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cp[8] = { 'H', 'e', 'l', 'l', 'o', ' ', 'W', 'i' }; UINT16 g[8]; INT32 a[8]; int i;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.f0, cp, 8, g));
    CHECK_HR_OK(gdi_adv(&x, 16.0f, 1.0f, NULL, FALSE, FALSE, 8, g, a));
    for (i = 0; i < 8; i++) t_check(a[i] % 128 == 0 && a[i] > 0, __FILE__, __LINE__, "arial", "glyph %u: %d", g[i], a[i]);
    t_note("Arial 16 ppem classic px: %d %d %d %d %d %d %d %d", a[0] / 128, a[1] / 128, a[2] / 128, a[3] / 128, a[4] / 128, a[5] / 128, a[6] / 128, a[7] / 128);
    ffx_close(&x);
}

/* ---- kerning ------------------------------------------------------------------------------------------------------- */
static HRESULT kern(ffx *x, UINT32 n, const UINT16 *g, INT32 *a) { return IDWriteFontFace1_GetKerningPairAdjustments(x->f1, n, g, a); }
static void expect_kern_oracle(ffx *x, const UINT16 *g, int n, const char *what) {
    INT32 a[128]; int i;
    memset(a, 0xCC, sizeof a);
    HRESULT hr = kern(x, n, g, a);
    t_check(hr == S_OK, __FILE__, __LINE__, what, "hr %s", dw_hr_name(hr));
    for (i = 0; i < n; i++) {
        int want = (i + 1 < n) ? ffx_kern_value(g[i], g[i + 1]) : 0;
        t_check(a[i] == want, __FILE__, __LINE__, what, "pair %d (%u,%u): %d want %d", i, g[i], i + 1 < n ? g[i + 1] : 0, a[i], want);
    }
}
TEST(kp_known_sequence) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[9] = { 2, 23, 2, 26, 21, 42, 13, 21, 2 };      /* A V A Y T o L T A */
    INT32 a[9]; CHECK_HR_OK(kern(&x, 9, g, a));
    static const INT32 want[9] = { -120, -100, -80, 0, -150, 0, 30, 0, 0 };
    int i; for (i = 0; i < 9; i++) t_check(a[i] == want[i], __FILE__, __LINE__, "known", "pair %d: %d want %d", i, a[i], want[i]);
    ffx_close(&x);
}
TEST(kp_has_kerning_pairs_true_for_kern_table) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace1_HasKerningPairs(x.f1), TRUE);
    ffx_close(&x);
}
TEST(kp_has_kerning_pairs_false_without_tables) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace1_HasKerningPairs(x.f1), FALSE);
    ffx_close(&x);
}
TEST(kp_no_kern_table_all_zero) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[5] = { 2, 23, 2, 26, 21 }; INT32 a[5]; memset(a, 0xCC, sizeof a);
    CHECK_HR_OK(kern(&x, 5, g, a));
    int i; for (i = 0; i < 5; i++) EXPECT_EQ(a[i], 0);
    ffx_close(&x);
}
TEST(kp_zero_count_ok_with_null) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    CHECK_HR_OK(kern(&x, 0, NULL, NULL));
    UINT16 g = 2; CHECK_HR_OK(kern(&x, 0, &g, NULL));
    ffx_close(&x);
}
TEST(kp_null_indices_invalid_and_zeroed) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    INT32 a[4]; memset(a, 0xCC, sizeof a);
    CHECK_HR(kern(&x, 4, NULL, a), EINVAL);
    int i; for (i = 0; i < 4; i++) EXPECT_EQ(a[i], 0);
    ffx_close(&x);
}
TEST(kp_single_glyph_is_zero) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g = 2; INT32 a = 0x7777;
    CHECK_HR_OK(kern(&x, 1, &g, &a));
    EXPECT_EQ(a, 0);          /* the last (here only) entry has no right neighbour */
    ffx_close(&x);
}
TEST(kp_two_glyph_pairs_table) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    int i;
    for (i = 0; i < ffx_n_kern_pairs; i++) {
        UINT16 g[2] = { ffx_kern_pairs[i].l, ffx_kern_pairs[i].r }; INT32 a[2];
        CHECK_HR_OK(kern(&x, 2, g, a));
        t_check(a[0] == ffx_kern_pairs[i].v && a[1] == 0, __FILE__, __LINE__, "pair", "(%u,%u): %d,%d want %d,0", g[0], g[1], a[0], a[1], ffx_kern_pairs[i].v);
    }
    ffx_close(&x);
}
TEST(kp_reverse_pair_not_kerned) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[2] = { 42, 21 }; INT32 a[2];        /* (o,T) is not in the table, (T,o) is */
    CHECK_HR_OK(kern(&x, 2, g, a));
    EXPECT_EQ(a[0], 0);
    ffx_close(&x);
}
TEST(kp_notdef_pair) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[3] = { 0, 0, 0 }; INT32 a[3];
    CHECK_HR_OK(kern(&x, 3, g, a));
    EXPECT_EQ(a[0], -5); EXPECT_EQ(a[1], -5); EXPECT_EQ(a[2], 0);
    ffx_close(&x);
}
TEST(kp_repeated_pairs_every_position) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[16]; int i; for (i = 0; i < 16; i++) g[i] = (i & 1) ? 23 : 2;      /* A V A V ... */
    expect_kern_oracle(&x, g, 16, "AVAV");
    ffx_close(&x);
}
TEST(kp_random_sequences_match_oracle) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    static const UINT16 pool[] = { 0, 2, 3, 13, 21, 23, 26, 42, 1, 67 };
    unsigned seed = 777; int t;
    for (t = 0; t < 60; t++) {
        UINT16 g[40]; int n = 2 + (int)((seed = seed * 1664525u + 1013904223u) >> 24) % 38, i;
        for (i = 0; i < n; i++) { seed = seed * 1664525u + 1013904223u; g[i] = pool[(seed >> 16) % 10]; }
        expect_kern_oracle(&x, g, n, "random");
    }
    ffx_close(&x);
}
TEST(kp_zeroed_before_work_for_big_buffer) {
    ttf_spec s; ffx_spec_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[100]; INT32 a[100]; int i; for (i = 0; i < 100; i++) g[i] = (i % 3 == 0) ? 2 : (i % 3 == 1) ? 23 : 3;
    memset(a, 0x55, sizeof a);
    CHECK_HR_OK(kern(&x, 100, g, a));
    EXPECT_EQ(a[99], 0);
    int bad = 0; for (i = 0; i < 99; i++) if (a[i] != ffx_kern_value(g[i], g[i + 1])) bad++;
    EXPECT_EQ(bad, 0);
    ffx_close(&x);
}
TEST(kp_bold_oblique_simulation_same_adjustments) {
    ttf_spec s; ffx_spec_kern(&s);
    DWRITE_FONT_SIMULATIONS sims[] = { DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE };
    int k;
    for (k = 0; k < 2; k++) {
        ffx x; if (!ffx_open_spec(&x, &s, sims[k])) { ffx_close(&x); return; }
        UINT16 g[4] = { 2, 23, 2, 26 };
        expect_kern_oracle(&x, g, 4, "simulated");
        EXPECT_EQ(IDWriteFontFace1_HasKerningPairs(x.f1), TRUE);
        ffx_close(&x);
    }
}
TEST(kp_gpos_only_font_consistency) {
    ttf_spec s; ffx_spec_gpos_kern(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    BOOL has = IDWriteFontFace1_HasKerningPairs(x.f1);
    UINT16 g[3] = { 2, 23, 2 }; INT32 a[3]; CHECK_HR_OK(kern(&x, 3, g, a));
    t_note("GPOS-only kern font: HasKerningPairs=%d, adjustments for A V A: %d %d %d", has, a[0], a[1], a[2]);
    /* only the legacy 'kern' table feeds this API, so a GPOS-only font reports no pair adjustments */
    EXPECT_EQ(a[0], 0); EXPECT_EQ(a[1], 0); EXPECT_EQ(a[2], 0);
    ffx_close(&x);
}
TEST(kp_kern_and_gpos_font_uses_kern_values) {
    ttf_spec s; ffx_spec_kern(&s); s.gpos_kern = ffx_kern_pairs; s.ngpos_kern = ffx_n_kern_pairs;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[4] = { 2, 23, 2, 26 };
    expect_kern_oracle(&x, g, 4, "kern+GPOS");
    ffx_close(&x);
}
TEST(kp_large_negative_and_positive_values) {
    static const ttf_kernpair kp[] = { { 2, 3, -32768 }, { 3, 4, 32767 }, { 4, 5, 1 }, { 5, 6, -1 } };
    ttf_spec s; ttf_default_spec(&s); s.kern = kp; s.nkern = 4;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 g[5] = { 2, 3, 4, 5, 6 }; INT32 a[5]; CHECK_HR_OK(kern(&x, 5, g, a));
    EXPECT_EQ(a[0], -32768); EXPECT_EQ(a[1], 32767); EXPECT_EQ(a[2], 1); EXPECT_EQ(a[3], -1); EXPECT_EQ(a[4], 0);
    ffx_close(&x);
}
TEST(kp_many_pairs_binary_search) {
    ttf_kernpair kp[60]; int i, n = 0;
    for (i = 0; i < 26; i++) { kp[n].l = (unsigned short)(2 + i); kp[n].r = (unsigned short)(2 + (i * 7 + 3) % 26); kp[n].v = (short)(-10 - i); n++; }
    for (i = 0; i < 26; i++) { kp[n].l = (unsigned short)(28 + i); kp[n].r = (unsigned short)(28 + (i * 5 + 1) % 26); kp[n].v = (short)(10 + i); n++; }
    ttf_spec s; ttf_default_spec(&s); s.kern = kp; s.nkern = n;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    int bad = 0;
    for (i = 0; i < n; i++) { UINT16 g[2] = { kp[i].l, kp[i].r }; INT32 a[2]; kern(&x, 2, g, a); if (a[0] != kp[i].v) bad++; }
    EXPECT_EQ(bad, 0);
    UINT16 miss[2] = { 2, 2 }; INT32 am[2]; kern(&x, 2, miss, am);
    int hit = 0; for (i = 0; i < n; i++) if (kp[i].l == 2 && kp[i].r == 2) hit = 1;
    if (!hit) EXPECT_EQ(am[0], 0);
    ffx_close(&x);
}

/* kern table of a real font: parse its first horizontal format-0 subtable and compare */
static int parse_kern0(const uint8_t *k, UINT32 len, const uint8_t **pairs, int *npairs) {
    if (len < 18 || ffx_getu16(k, 0) != 0) return 0;
    unsigned nt = ffx_getu16(k, 2), off = 4, t;
    for (t = 0; t < nt && off + 14 <= len; t++) {
        unsigned l = ffx_getu16(k, off + 2), cov = ffx_getu16(k, off + 4);
        if ((cov & 0xFF00) == 0 && (cov & 1)) { *npairs = (int)ffx_getu16(k, off + 6); *pairs = k + off + 14; return off + 14 + (unsigned)*npairs * 6 <= len; }
        off += l;
    }
    return 0;
}
static void check_kern_vs_table(ffx *x) {
    UINT32 len = 0; uint8_t *k = dw_face_table(x->f0, DW_TAG('k','e','r','n'), &len);
    BOOL has = IDWriteFontFace1_HasKerningPairs(x->f1);
    const uint8_t *p; int np = 0, i, bad = 0, tested = 0;
    if (!k) { t_note("font has no kern table; HasKerningPairs=%d", has); EXPECT_EQ(has, FALSE); free(k); return; }
    if (!parse_kern0(k, len, &p, &np)) { t_note("kern table not format 0/horizontal (len %u): HasKerningPairs=%d", len, has); free(k); return; }
    EXPECT_EQ(has, TRUE);
    for (i = 0; i < np && i < 400; i += (np > 400 ? np / 400 : 1)) {
        UINT16 g[2] = { (UINT16)ffx_getu16(p, i * 6), (UINT16)ffx_getu16(p, i * 6 + 2) }; INT32 a[2] = { 0x77, 0x77 };
        int v = ffx_get16(p, i * 6 + 4);
        kern(x, 2, g, a); tested++;
        if (a[0] != v || a[1] != 0) bad++;
    }
    t_note("%d kern pairs in the table, %d compared", np, tested);
    EXPECT_EQ(bad, 0);
    free(k);
}
TEST(kp_arial_matches_kern_table) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    check_kern_vs_table(&x); ffx_close(&x);
}
TEST(kp_times_matches_kern_table) {
    ffx x; if (!ffx_open_sys(&x, L"Times New Roman", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    check_kern_vs_table(&x); ffx_close(&x);
}
TEST(kp_tahoma_matches_kern_table) {
    ffx x; if (!ffx_open_sys(&x, L"Tahoma", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    check_kern_vs_table(&x); ffx_close(&x);
}
TEST(kp_courier_matches_kern_table) {
    ffx x; if (!ffx_open_sys(&x, L"Courier New", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    check_kern_vs_table(&x); ffx_close(&x);
}
TEST(kp_segoe_ui_matches_kern_table) {
    ffx x; if (!ffx_open_sys(&x, L"Segoe UI", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    check_kern_vs_table(&x); ffx_close(&x);
}

/* ---- vertical glyph variants ---------------------------------------------------------------------------------------- */
static HRESULT vvar(ffx *x, UINT32 n, const UINT16 *in, UINT16 *out) { return IDWriteFontFace1_GetVerticalGlyphVariants(x->f1, n, in, out); }
TEST(vv_zero_count_null_pointers_ok) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    CHECK_HR_OK(vvar(&x, 0, NULL, NULL));
    UINT16 g = 2; CHECK_HR_OK(vvar(&x, 0, &g, NULL));
    CHECK_HR_OK(vvar(&x, 0, NULL, &g));
    EXPECT_EQ(g, 2);
    ffx_close(&x);
}
TEST(vv_null_nominal_zero_fills_output) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 out[6]; memset(out, 0xFF, sizeof out);
    CHECK_HR(vvar(&x, 6, NULL, out), EINVAL);
    int i; for (i = 0; i < 6; i++) EXPECT_EQ(out[i], 0);
    ffx_close(&x);
}
TEST(vv_no_variants_identity) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 in[8] = { 0, 1, 2, 3, 28, 64, 67, 68 }, out[8]; memset(out, 0xFF, sizeof out);
    CHECK_HR_OK(vvar(&x, 8, in, out));
    CHECK(!memcmp(in, out, sizeof in));
    EXPECT_EQ(IDWriteFontFace1_HasVerticalGlyphVariants(x.f1), FALSE);
    ffx_close(&x);
}
TEST(vv_in_place_matches_out_of_place) {
    ttf_spec s; ffx_spec_vert(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 in[8] = { 2, 3, 4, 5, 28, 64, 0, 2 }, out[8], ip[8]; memcpy(ip, in, sizeof in);
    CHECK_HR_OK(vvar(&x, 8, in, out));
    CHECK_HR_OK(vvar(&x, 8, ip, ip));
    CHECK(!memcmp(out, ip, sizeof out));
    CHECK(!memcmp(in, (UINT16[8]){ 2, 3, 4, 5, 28, 64, 0, 2 }, sizeof in));   /* input untouched by the out-of-place call */
    ffx_close(&x);
}
TEST(vv_vert_feature_consistency) {
    ttf_spec s; ffx_spec_vert(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 in[TTF_DEF_NGLYPHS], out[TTF_DEF_NGLYPHS]; int i, changed = 0, bad = 0;
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) in[i] = (UINT16)i;
    CHECK_HR_OK(vvar(&x, TTF_DEF_NGLYPHS, in, out));
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) {
        int sub = -1, k; for (k = 0; k < ffx_n_vert_subs; k++) if (ffx_vert_subs[k].from == i) sub = ffx_vert_subs[k].to;
        if (out[i] != in[i]) { changed++; if (sub < 0 || out[i] != sub) bad++; }
        if (sub < 0 && out[i] != in[i]) bad++;
    }
    BOOL has = IDWriteFontFace1_HasVerticalGlyphVariants(x.f1);
    t_note("GSUB 'vert' (latn) font: %d glyphs substituted, HasVerticalGlyphVariants=%d", changed, has);
    EXPECT_EQ(bad, 0);                    /* whatever is substituted must come from the GSUB single-substitution lookup */
    if (changed) CHECK(has == TRUE);
    if (!has) EXPECT_EQ(changed, 0);
    ffx_close(&x);
}
TEST(vv_unmapped_glyphs_unchanged_with_vert_font) {
    ttf_spec s; ffx_spec_vert(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 in[6] = { 0, 1, 5, 28, 54, 67 }, out[6];
    CHECK_HR_OK(vvar(&x, 6, in, out));
    CHECK(!memcmp(in, out, sizeof in));
    ffx_close(&x);
}
TEST(vv_vrt2_feature_font_note) {
    static const ttf_feature f[] = { { "vrt2", TTF_LK_SINGLE, 3, ffx_vert_subs } };
    ttf_spec s; ffx_spec_vert(&s); s.gsub = f; s.ngsub = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 in[3] = { 2, 3, 4 }, out[3]; CHECK_HR_OK(vvar(&x, 3, in, out));
    t_note("GSUB 'vrt2' font: A->%u B->%u C->%u, Has=%d", out[0], out[1], out[2], IDWriteFontFace1_HasVerticalGlyphVariants(x.f1));
    int i; for (i = 0; i < 3; i++) CHECK(out[i] == in[i] || out[i] == ffx_vert_subs[i].to);
    ffx_close(&x);
}
TEST(vv_both_vert_and_vrt2_note) {
    static const ttf_sub1 alt[] = { { 2, 65 }, { 3, 64 } };
    static const ttf_feature f[] = { { "vert", TTF_LK_SINGLE, 3, ffx_vert_subs }, { "vrt2", TTF_LK_SINGLE, 2, alt } };
    ttf_spec s; ffx_spec_vert(&s); s.gsub = f; s.ngsub = 2;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 in[3] = { 2, 3, 4 }, out[3]; CHECK_HR_OK(vvar(&x, 3, in, out));
    t_note("vert A->64 B->65 C->68 and vrt2 A->65 B->64: got A->%u B->%u C->%u", out[0], out[1], out[2]);
    ffx_close(&x);
}
TEST(vv_non_vertical_feature_ignored) {
    static const ttf_sub1 sm[] = { { 2, 64 } };
    static const ttf_feature f[] = { { "smcp", TTF_LK_SINGLE, 1, sm } };
    ttf_spec s; ttf_default_spec(&s); s.gsub = f; s.ngsub = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT16 in[2] = { 2, 3 }, out[2]; CHECK_HR_OK(vvar(&x, 2, in, out));
    CHECK(!memcmp(in, out, sizeof in));
    EXPECT_EQ(IDWriteFontFace1_HasVerticalGlyphVariants(x.f1), FALSE);
    ffx_close(&x);
}
TEST(vv_has_returns_clean_bool) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    BOOL r = IDWriteFontFace1_HasVerticalGlyphVariants(x.f1);
    CHECK(r == 0 || r == 1);
    ffx_close(&x);
}
TEST(vv_ms_gothic_variants_note) {
    ffx x; if (!ffx_open_sys(&x, L"MS Gothic", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cp[6] = { 0x3001, 0x3002, 0x300C, 0x300D, 0x30FC, 0xFF08 }; UINT16 g[6], v[6]; int i, changed = 0;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.f0, cp, 6, g));
    CHECK_HR_OK(vvar(&x, 6, g, v));
    for (i = 0; i < 6; i++) if (v[i] != g[i]) changed++;
    BOOL has = IDWriteFontFace1_HasVerticalGlyphVariants(x.f1);
    t_note("MS Gothic: Has=%d, %d of 6 CJK punctuation glyphs have vertical forms", has, changed);
    if (changed) CHECK(has == TRUE);
    ffx_close(&x);
}
TEST(vv_arial_has_no_vertical_forms_note) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cp[4] = { 'A', 'b', '(', '-' }; UINT16 g[4], v[4];
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.f0, cp, 4, g));
    CHECK_HR_OK(vvar(&x, 4, g, v));
    t_note("Arial HasVerticalGlyphVariants=%d", IDWriteFontFace1_HasVerticalGlyphVariants(x.f1));
    if (!IDWriteFontFace1_HasVerticalGlyphVariants(x.f1)) CHECK(!memcmp(g, v, sizeof g));
    ffx_close(&x);
}
