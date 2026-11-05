/* IDWriteTextAnalyzer2::GetTypographicFeatures and ::CheckTypographicFeature.
 *
 * Windows 10 (DWriteTextAnalyzer::GetTypographicFeatures / CheckTypographicFeature, ShapingInterface::*):
 *   GetTypographicFeatures
 *     - *actualTagCount is cleared first; fontFace == NULL -> E_INVALIDARG; NULL localeName is treated as "" and an unknown
 *       locale is ignored. maxTagCount == 0 with tags == NULL is allowed.
 *     - A font without GSUB/GPOS lists nothing (S_OK, count 0). Otherwise *actualTagCount is the number of distinct features
 *       available for the script/language (the font's script list with fallback) and, when it exceeds maxTagCount, the
 *       call returns 0x8007007A (the required count is still reported).
 *   CheckTypographicFeature
 *     - featureApplies[0..glyphCount) is cleared first (also when the call fails); fontFace == NULL, or glyphIndices == NULL
 *       with glyphCount != 0 -> E_INVALIDARG; otherwise S_OK and each entry is 1 when the feature affects the glyph.
 */
#include "ta.h"

static const ttf_sub4 k_lig[] = { { { 2, 3 }, 2, 64 } };                   /* 'A''B' -> triangle glyph */
static const ttf_sub1 k_smcp[] = { { 28, 2 }, { 29, 3 } };                 /* 'a' -> 'A', 'b' -> 'B' */
static const ttf_sub3 k_salt[] = { { 2, 2, { 64, 65 } } };                 /* 'A' -> triangle | round */
static const ttf_feature k_gsub[] = {
    { "liga", TTF_LK_LIGATURE, 1, k_lig }, { "smcp", TTF_LK_SINGLE, 2, k_smcp }, { "salt", TTF_LK_ALTERNATE, 1, k_salt },
};
static const ttf_kernpair k_kern[] = { { 2, 3, -50 } };

static void feat_spec(ttf_spec *s) {
    ttf_default_spec(s);
    s->gsub = k_gsub; s->ngsub = 3; s->gpos_kern = k_kern; s->ngpos_kern = 1;
}
typedef struct ffix { dwx_font h; DWRITE_SCRIPT_ANALYSIS latin; } ffix;
static int ff_open(ffix *x, ta_ctx *c, int with_layout) {
    ttf_spec s; memset(x, 0, sizeof *x);
    if (with_layout) feat_spec(&s); else ttf_default_spec(&s);
    if (FAILED(ta_open_spec(&x->h, c->f, &s)) || !x->h.face) return 0;
    x->latin = ta_sa_of(c, L"A", 1);
    return x->latin.script != 0xFFFF;
}
#define NEED_FF(layout) ffix ff; if (!ff_open(&ff, &c_, layout)) { dwx_font_close(&ff.h); TA_END(); t_skip("synthetic font unavailable"); return; }
static int has_tag(const DWRITE_FONT_FEATURE_TAG *t, UINT32 n, UINT32 tag) { UINT32 i; for (i = 0; i < n; i++) if ((UINT32)t[i] == tag) return 1; return 0; }
static int distinct(const DWRITE_FONT_FEATURE_TAG *t, UINT32 n) { UINT32 i, j; for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) if (t[i] == t[j]) return 0; return 1; }

/* ---- tag constants ---------------------------------------------------------------------------------------------------------- */
TEST(ft_tag_constants_match_opentype_tags) {
    static const struct { UINT32 v; const char *t; } k[] = {
        { DWRITE_FONT_FEATURE_TAG_KERNING, "kern" }, { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, "liga" }, { DWRITE_FONT_FEATURE_TAG_GLYPH_COMPOSITION_DECOMPOSITION, "ccmp" },
        { DWRITE_FONT_FEATURE_TAG_LOCALIZED_FORMS, "locl" }, { DWRITE_FONT_FEATURE_TAG_MARK_POSITIONING, "mark" }, { DWRITE_FONT_FEATURE_TAG_MARK_TO_MARK_POSITIONING, "mkmk" },
        { DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES, "calt" }, { DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES, "clig" }, { DWRITE_FONT_FEATURE_TAG_DISCRETIONARY_LIGATURES, "dlig" },
        { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, "smcp" }, { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS_FROM_CAPITALS, "c2sc" }, { DWRITE_FONT_FEATURE_TAG_OLD_STYLE_FIGURES, "onum" },
        { DWRITE_FONT_FEATURE_TAG_LINING_FIGURES, "lnum" }, { DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_FIGURES, "pnum" }, { DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, "tnum" },
        { DWRITE_FONT_FEATURE_TAG_FRACTIONS, "frac" }, { DWRITE_FONT_FEATURE_TAG_ORDINALS, "ordn" }, { DWRITE_FONT_FEATURE_TAG_SUPERSCRIPT, "sups" },
        { DWRITE_FONT_FEATURE_TAG_SUBSCRIPT, "subs" }, { DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES, "salt" }, { DWRITE_FONT_FEATURE_TAG_SWASH, "swsh" },
        { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_1, "ss01" }, { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_10, "ss10" }, { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_20, "ss20" },
        { DWRITE_FONT_FEATURE_TAG_SLASHED_ZERO, "zero" }, { DWRITE_FONT_FEATURE_TAG_HISTORICAL_FORMS, "hist" }, { DWRITE_FONT_FEATURE_TAG_HISTORICAL_LIGATURES, "hlig" },
        { DWRITE_FONT_FEATURE_TAG_CASE_SENSITIVE_FORMS, "case" }, { DWRITE_FONT_FEATURE_TAG_CAPITAL_SPACING, "cpsp" }, { DWRITE_FONT_FEATURE_TAG_CURSIVE_POSITIONING, "curs" },
        { DWRITE_FONT_FEATURE_TAG_REQUIRED_LIGATURES, "rlig" }, { DWRITE_FONT_FEATURE_TAG_VERTICAL_WRITING, "vert" }, { DWRITE_FONT_FEATURE_TAG_VERTICAL_ALTERNATES_AND_ROTATION, "vrt2" },
        { DWRITE_FONT_FEATURE_TAG_TITLING, "titl" }, { DWRITE_FONT_FEATURE_TAG_UNICASE, "unic" }, { DWRITE_FONT_FEATURE_TAG_FULL_WIDTH, "fwid" }, { DWRITE_FONT_FEATURE_TAG_HALF_WIDTH, "hwid" },
        { DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_WIDTHS, "pwid" }, { DWRITE_FONT_FEATURE_TAG_PETITE_CAPITALS, "pcap" },
    };
    unsigned i;
    for (i = 0; i < sizeof k / sizeof *k; i++) CHECKF(k[i].v == TA_TAG(k[i].t[0], k[i].t[1], k[i].t[2], k[i].t[3]), "%s: 0x%08x", k[i].t, k[i].v);
}

/* ---- GetTypographicFeatures ---------------------------------------------------------------------------------------------------- */
TEST(ft_null_face_invalid_count_cleared) {
    TA_BEGIN(); UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4]; DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1);
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, NULL, sa, L"en-us", 4, &n, t), TA_E_INVALIDARG);
    EXPECT_EQ(n, 0);
    TA_END();
}
TEST(ft_font_without_layout_tables_lists_nothing) {
    TA_BEGIN(); NEED_FF(0); UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4];
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 0, &n, NULL), S_OK); EXPECT_EQ(n, 0);
    n = 99; CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, NULL, 4, &n, t), S_OK); EXPECT_EQ(n, 0);
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_size_query_with_zero_buffer) {
    TA_BEGIN(); NEED_FF(1); UINT32 n = 99;
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 0, &n, NULL), TA_E_BUFFER);
    CHECK(n >= 3); t_note("synthetic font latn feature count: %u", n);
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_lists_gsub_features) {
    TA_BEGIN(); NEED_FF(1); UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[16];
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 16, &n, t), S_OK);
    CHECK(n >= 3 && n <= 16);
    CHECK(has_tag(t, n, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES)); CHECK(has_tag(t, n, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS)); CHECK(has_tag(t, n, DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES));
    CHECK(distinct(t, n));
    if (!has_tag(t, n, DWRITE_FONT_FEATURE_TAG_KERNING)) t_note("GPOS kern not listed"); else t_note("GPOS kern listed (count %u)", n);
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_exact_buffer_succeeds) {
    TA_BEGIN(); NEED_FF(1); UINT32 n = 0, m = 0; DWRITE_FONT_FEATURE_TAG t[20], big[20];
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 0, &n, NULL);
    CHECK(n >= 1 && n <= 16);
    t[n] = (DWRITE_FONT_FEATURE_TAG)0x5A5A5A5A;
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", n, &m, t), S_OK);
    EXPECT_EQ(m, n); EXPECT_EQ((UINT32)t[n], 0x5A5A5A5A);
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 16, &m, big);
    EXPECT_EQ(m, n); CHECK(!memcmp(t, big, n * sizeof t[0]));
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_one_short_buffer_reports_required_count) {
    TA_BEGIN(); NEED_FF(1); UINT32 n = 0, m = 0; DWRITE_FONT_FEATURE_TAG t[20];
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 0, &n, NULL);
    t[n - 1] = (DWRITE_FONT_FEATURE_TAG)0x5A5A5A5A;
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", n - 1, &m, t), TA_E_BUFFER);
    EXPECT_EQ(m, n); EXPECT_EQ((UINT32)t[n - 1], 0x5A5A5A5A);          /* nothing written past maxTagCount */
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_buffer_larger_than_needed_untouched_tail) {
    TA_BEGIN(); NEED_FF(1); UINT32 n = 0, m = 0, i; DWRITE_FONT_FEATURE_TAG t[16];
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 0, &n, NULL);
    for (i = 0; i < 16; i++) t[i] = (DWRITE_FONT_FEATURE_TAG)0x5A5A5A5A;
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 16, &m, t), S_OK);
    for (i = m; i < 16; i++) EXPECT_EQ((UINT32)t[i], 0x5A5A5A5A);
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_locale_variants_agree) {
    TA_BEGIN(); NEED_FF(1); static const WCHAR *locs[] = { NULL, L"", L"en-us", L"de-de", L"cadabra", L"x" }; unsigned i; UINT32 n0 = 0, n;
    DWRITE_FONT_FEATURE_TAG t0[16], t[16];
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, locs[0], 16, &n0, t0), S_OK);
    for (i = 1; i < sizeof locs / sizeof *locs; i++) {
        n = 0; CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, locs[i], 16, &n, t), S_OK);
        CHECKF(n == n0 && !memcmp(t, t0, n * sizeof t[0]), "locale %u", i);
    }
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_unknown_script_uses_fallback) {
    /* the font has DFLT and latn only: an unrelated script is served by the fallback script (or yields nothing) */
    TA_BEGIN(); NEED_FF(1); static const WCHAR *samples[] = { L"\x0627", L"\x0416", L"\x4E00", L"\x0905" }; unsigned i; UINT32 n0 = 0, n; DWRITE_FONT_FEATURE_TAG t[16];
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, NULL, 16, &n0, t);
    for (i = 0; i < sizeof samples / sizeof *samples; i++) {
        DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, samples[i], 1); HRESULT hr; n = 99;
        hr = IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, sa, NULL, 16, &n, t);
        CHECK_HR(hr, S_OK);
        CHECKF(n == 0 || n == n0, "U+%04X: %u features (latn has %u)", samples[i][0], n, n0);
        t_note("U+%04X (script %u): %u features", samples[i][0], sa.script, n);
    }
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_script_shapes_field_ignored) {
    TA_BEGIN(); NEED_FF(1); UINT32 n0 = 0, n = 0; DWRITE_SCRIPT_ANALYSIS sa = ff.latin;
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, NULL, 0, &n0, NULL);
    sa.shapes = DWRITE_SCRIPT_SHAPES_NO_VISUAL;
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, sa, NULL, 0, &n, NULL);
    EXPECT_EQ(n, n0);
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_repeatable_and_order_stable) {
    TA_BEGIN(); NEED_FF(1); UINT32 n = 0, m = 0; DWRITE_FONT_FEATURE_TAG a[16], b[16];
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, NULL, 16, &n, a); IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, NULL, 16, &m, b);
    CHECK(n == m && !memcmp(a, b, n * sizeof a[0]));
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_via_other_face_objects_of_same_font) {
    TA_BEGIN(); NEED_FF(1); UINT32 n0 = 0, n = 0, nf = 1; IDWriteFontFace *f2 = NULL; IDWriteFontFile *file = NULL;
    IDWriteFontFace_GetFiles(ff.h.face, &nf, &file);
    if (file && SUCCEEDED(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &f2)) && f2) {
        IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, NULL, 0, &n0, NULL);
        IDWriteTextAnalyzer2_GetTypographicFeatures(a2, f2, ff.latin, NULL, 0, &n, NULL);
        EXPECT_EQ(n, n0); IDWriteFontFace_Release(f2);
    }
    DW_RELEASE(file);
    dwx_font_close(&ff.h); TA_END();
}
TEST(ft_face_refcount_balanced) {
    TA_BEGIN(); NEED_FF(1); UINT32 n; DWRITE_FONT_FEATURE_TAG t[16]; ULONG r0 = dw_refcount((IUnknown *)ff.h.face);
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, NULL, 16, &n, t);
    EXPECT_REFS(ff.h.face, r0);
    dwx_font_close(&ff.h); TA_END();
}

/* ---- CheckTypographicFeature ------------------------------------------------------------------------------------------------------ */
static HRESULT chk(IDWriteTextAnalyzer2 *a2, ffix *ff, UINT32 tag, UINT32 n, const UINT16 *g, UINT8 *out) {
    return IDWriteTextAnalyzer2_CheckTypographicFeature(a2, ff->h.face, ff->latin, L"en-us", (DWRITE_FONT_FEATURE_TAG)tag, n, g, out);
}
TEST(fc_null_face_invalid_outputs_cleared) {
    TA_BEGIN(); UINT16 g[3] = { 2, 3, 4 }; UINT8 out[3] = { 7, 7, 7 }; DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, NULL, sa, L"en-us", DWRITE_FONT_FEATURE_TAG_KERNING, 3, g, out), TA_E_INVALIDARG);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[1], 0); EXPECT_EQ(out[2], 0);
    TA_END();
}
TEST(fc_null_glyph_indices_invalid_outputs_cleared) {
    TA_BEGIN(); NEED_FF(1); UINT8 out[3] = { 7, 7, 7 };
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_KERNING, 3, NULL, out), TA_E_INVALIDARG);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[2], 0);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_zero_glyph_count_ok_with_nulls) {
    TA_BEGIN(); NEED_FF(1);
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_KERNING, 0, NULL, NULL), S_OK);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, ff.h.face, ff.latin, NULL, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0, NULL, NULL), S_OK);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_buffer_beyond_count_untouched) {
    TA_BEGIN(); NEED_FF(1); UINT16 g[2] = { 2, 3 }; UINT8 out[4] = { 7, 7, 9, 9 };
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 2, g, out), S_OK);
    EXPECT_EQ(out[2], 9); EXPECT_EQ(out[3], 9);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_font_without_layout_tables_all_false) {
    TA_BEGIN(); NEED_FF(0); UINT16 g[3] = { 2, 3, 28 }; UINT8 out[3] = { 7, 7, 7 };
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_KERNING, 3, g, out), S_OK);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[1], 0); EXPECT_EQ(out[2], 0);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_feature_not_in_font_is_false_for_all) {
    TA_BEGIN(); NEED_FF(1); UINT16 g[5] = { 0, 2, 3, 28, 64 }; UINT8 out[5]; unsigned i; memset(out, 7, sizeof out);
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_SWASH, 5, g, out), S_OK);
    for (i = 0; i < 5; i++) EXPECT_EQ(out[i], 0);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_single_substitution_glyph_coverage) {
    TA_BEGIN(); NEED_FF(1); UINT16 g[6] = { 28, 29, 30, 2, 0, 66 }; UINT8 out[6]; memset(out, 7, sizeof out);
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 6, g, out), S_OK);
    EXPECT_EQ(out[0], 1); EXPECT_EQ(out[1], 1); EXPECT_EQ(out[2], 0); EXPECT_EQ(out[4], 0); EXPECT_EQ(out[5], 0);
    t_note("smcp on target glyph 'A'(2): %u", out[3]);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_alternate_and_ligature_note) {
    TA_BEGIN(); NEED_FF(1); UINT16 g[4] = { 2, 3, 28, 64 }; UINT8 out[4]; unsigned i;
    memset(out, 7, sizeof out); CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES, 4, g, out), S_OK);
    EXPECT_EQ(out[0], 1); EXPECT_EQ(out[2], 0); EXPECT_EQ(out[3], 0);
    memset(out, 7, sizeof out); CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 4, g, out), S_OK);
    EXPECT_EQ(out[2], 0);            /* 'a' is not part of any ligature */
    for (i = 0; i < 4; i++) t_note("liga glyph %u: %u", g[i], out[i]);
    memset(out, 7, sizeof out); CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_KERNING, 4, g, out), S_OK);
    for (i = 0; i < 4; i++) t_note("kern glyph %u: %u", g[i], out[i]);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_results_are_zero_or_one) {
    TA_BEGIN(); NEED_FF(1); UINT16 g[16]; UINT8 out[16]; unsigned i, t; static const UINT32 tags[] = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES, DWRITE_FONT_FEATURE_TAG_KERNING };
    for (i = 0; i < 16; i++) g[i] = (UINT16)(i * 4);
    for (t = 0; t < 4; t++) { memset(out, 7, sizeof out); CHECK_HR(chk(a2, &ff, tags[t], 16, g, out), S_OK); for (i = 0; i < 16; i++) CHECK(out[i] <= 1); }
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_out_of_range_glyph_indices_are_false) {
    TA_BEGIN(); NEED_FF(1); UINT16 g[4] = { 0xFFFF, 0x8000, 500, 69 }; UINT8 out[4]; unsigned i; memset(out, 7, sizeof out);
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 4, g, out), S_OK);
    for (i = 0; i < 4; i++) EXPECT_EQ(out[i], 0);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_locale_null_and_unknown_accepted) {
    TA_BEGIN(); NEED_FF(1); UINT16 g[1] = { 28 }; UINT8 a = 7, b = 7, c = 7;
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, ff.h.face, ff.latin, NULL, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, g, &a), S_OK);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, ff.h.face, ff.latin, L"cadabra", DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, g, &b), S_OK);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, ff.h.face, ff.latin, L"en-us", DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, g, &c), S_OK);
    EXPECT_EQ(a, c); EXPECT_EQ(b, c);
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_unlisted_tag_values) {
    TA_BEGIN(); NEED_FF(1); UINT16 g[1] = { 28 }; UINT8 out; static const UINT32 tags[] = { 0, 0xFFFFFFFFu, 0x20202020, DWRITE_FONT_FEATURE_TAG_DEFAULT };
    unsigned i;
    for (i = 0; i < sizeof tags / sizeof *tags; i++) { out = 7; CHECK_HR(chk(a2, &ff, tags[i], 1, g, &out), S_OK); EXPECT_EQ(out, 0); }
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_consistent_with_feature_listing) {
    /* a tag the face does not list for the script applies to no glyph */
    TA_BEGIN(); NEED_FF(1); UINT32 n = 0, i, j; DWRITE_FONT_FEATURE_TAG t[16]; UINT16 g[70]; UINT8 out[70];
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 16, &n, t);
    for (i = 0; i < 70; i++) g[i] = (UINT16)i;
    for (j = 0; j < 40; j++) {
        UINT32 tag = DWRITE_FONT_FEATURE_TAG_ALTERNATIVE_FRACTIONS + (UINT32)j * 0x01000193u; unsigned any = 0;      /* arbitrary 32-bit values */
        if (has_tag(t, n, tag)) continue;
        memset(out, 7, sizeof out); CHECK_HR(chk(a2, &ff, tag, 70, g, out), S_OK);
        for (i = 0; i < 70; i++) any |= out[i];
        EXPECT_EQ(any, 0);
    }
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_each_listed_feature_applies_to_some_glyph) {
    TA_BEGIN(); NEED_FF(1); UINT32 n = 0, i, k; DWRITE_FONT_FEATURE_TAG t[16]; UINT16 g[70]; UINT8 out[70];
    IDWriteTextAnalyzer2_GetTypographicFeatures(a2, ff.h.face, ff.latin, L"en-us", 16, &n, t);
    for (i = 0; i < 70; i++) g[i] = (UINT16)i;
    for (k = 0; k < n; k++) {
        unsigned any = 0; memset(out, 7, sizeof out);
        CHECK_HR(chk(a2, &ff, t[k], 70, g, out), S_OK);
        for (i = 0; i < 70; i++) any |= out[i];
        if (!any) t_note("feature 0x%08x is listed but applies to no glyph of 0..69", t[k]);
    }
    dwx_font_close(&ff.h); TA_END();
}
TEST(fc_large_glyph_array) {
    TA_BEGIN(); NEED_FF(1); UINT16 *g = (UINT16 *)malloc(5000 * sizeof(UINT16)); UINT8 *out = (UINT8 *)malloc(5000), ref[70]; UINT16 g0[70]; unsigned i, bad = 0;
    for (i = 0; i < 70; i++) g0[i] = (UINT16)i;
    memset(ref, 7, sizeof ref);
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 70, g0, ref), S_OK);
    for (i = 0; i < 5000; i++) g[i] = (UINT16)(i % 70);
    memset(out, 7, 5000);
    CHECK_HR(chk(a2, &ff, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 5000, g, out), S_OK);
    for (i = 0; i < 5000; i++) if (out[i] != ref[g[i]]) bad++;        /* the answer depends on the glyph only */
    EXPECT_EQ(bad, 0);
    free(g); free(out); dwx_font_close(&ff.h); TA_END();
}

/* ---- Arial -------------------------------------------------------------------------------------------------------------------------- */
/* Minimal reader of the GSUB/GPOS script and feature lists. */
static unsigned be16(const uint8_t *p) { return (unsigned)(p[0] << 8 | p[1]); }
static int collect_features(const uint8_t *tbl, UINT32 len, const char *script, UINT32 *tags, int max, int *all_mode) {
    int n = 0; unsigned sl, fl, i, nsc, nf;
    if (len < 10) return 0;
    sl = be16(tbl + 4); fl = be16(tbl + 6);
    if (sl + 2 > len || fl + 2 > len) return 0;
    nsc = be16(tbl + sl); nf = be16(tbl + fl);
    if (*all_mode) { for (i = 0; i < nf && n < max; i++) { const uint8_t *r = tbl + fl + 2 + i * 6; tags[n++] = r[0] | r[1] << 8 | r[2] << 16 | (UINT32)r[3] << 24; } return n; }
    for (i = 0; i < nsc; i++) {
        const uint8_t *r = tbl + sl + 2 + i * 6;
        if (!memcmp(r, script, 4)) {
            unsigned so = sl + be16(r + 4), dl = be16(tbl + so), k, nfi;
            if (!dl) return 0;
            { const uint8_t *ls = tbl + so + dl; nfi = be16(ls + 4);
              for (k = 0; k < nfi && n < max; k++) { unsigned fi = be16(ls + 6 + k * 2); const uint8_t *fr = tbl + fl + 2 + fi * 6; tags[n++] = fr[0] | fr[1] << 8 | fr[2] << 16 | (UINT32)fr[3] << 24; } }
            return n;
        }
    }
    return 0;
}
TEST(ft_arial_latin_features) {
    TA_BEGIN(); IDWriteFontFace *arial = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!arial) { TA_END(); t_skip("Arial not installed"); return; }
    DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1); UINT32 n = 0, i, gl = 0, pl = 0; DWRITE_FONT_FEATURE_TAG t[128];
    uint8_t *gsub = dw_face_table(arial, DW_TAG('G', 'S', 'U', 'B'), &gl), *gpos = dw_face_table(arial, DW_TAG('G', 'P', 'O', 'S'), &pl);
    CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, arial, sa, L"en-us", 0, &n, NULL), (gsub || gpos) ? TA_E_BUFFER : S_OK);
    CHECK(n <= 128);
    if (n <= 128) {
        UINT32 m = 0, all[256], na = 0, def[256], nd = 0; int allm = 1, defm = 0;
        CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(a2, arial, sa, L"en-us", 128, &m, t), S_OK);
        CHECK(distinct(t, m));
        if (gsub) { na += collect_features(gsub, gl, "latn", all + na, 256 - na, &allm); nd += collect_features(gsub, gl, "latn", def + nd, 256 - nd, &defm); }
        if (gpos) { na += collect_features(gpos, pl, "latn", all + na, 256 - na, &allm); nd += collect_features(gpos, pl, "latn", def + nd, 256 - nd, &defm); }
        for (i = 0; i < m; i++) { UINT32 k; int found = 0; for (k = 0; k < na; k++) if (all[k] == (UINT32)t[i]) found = 1; CHECKF(found, "tag 0x%08x is not in the font's feature list", t[i]); }
        { UINT32 miss = 0; for (i = 0; i < nd; i++) if (!has_tag(t, m, def[i])) miss++; if (miss) t_note("%u of %u default-langsys latn features not listed", miss, nd); }
        { char s[600]; int p = 0; for (i = 0; i < m && p < 560; i++) p += snprintf(s + p, sizeof s - p, "%c%c%c%c ", t[i] & 255, (t[i] >> 8) & 255, (t[i] >> 16) & 255, (t[i] >> 24) & 255); t_note("Arial latn features (%u): %s", m, s); }
    }
    free(gsub); free(gpos); IDWriteFontFace_Release(arial); TA_END();
}
TEST(fc_arial_kerning_applies_to_letters_note) {
    TA_BEGIN(); IDWriteFontFace *arial = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!arial) { TA_END(); t_skip("Arial not installed"); return; }
    DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1); UINT32 cps[4] = { 'A', 'V', 'e', ' ' }; UINT16 g[4]; UINT8 out[4]; unsigned i;
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(arial, cps, 4, g), S_OK);
    memset(out, 7, sizeof out);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, arial, sa, L"en-us", DWRITE_FONT_FEATURE_TAG_KERNING, 4, g, out), S_OK);
    for (i = 0; i < 4; i++) { CHECK(out[i] <= 1); t_note("Arial kern glyph %u ('%c'): %u", g[i], (char)cps[i], out[i]); }
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(fc_arial_missing_feature_false) {
    TA_BEGIN(); IDWriteFontFace *arial = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!arial) { TA_END(); t_skip("Arial not installed"); return; }
    DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1); UINT32 cps[3] = { 'A', 'b', '1' }; UINT16 g[3]; UINT8 out[3]; unsigned i;
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(arial, cps, 3, g), S_OK);
    memset(out, 7, sizeof out);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, arial, sa, L"en-us", TA_TAG(1, 2, 3, 4), 3, g, out), S_OK);
    for (i = 0; i < 3; i++) EXPECT_EQ(out[i], 0);          /* a bogus tag never applies */
    IDWriteFontFace_Release(arial); TA_END();
}
