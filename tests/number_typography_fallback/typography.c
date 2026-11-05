#include "ntf.h"

/* IDWriteTypography on Windows 10 (DWriteTypography, a std::vector<DWRITE_FONT_FEATURE>):
 *   - CreateTypography zeroes the out pointer, always succeeds and returns a fresh object with one reference;
 *   - AddFontFeature stores {nameTag, parameter} verbatim and always returns S_OK: no tag or parameter validation, duplicates are kept in order;
 *   - GetFontFeatureCount is the vector size; GetFontFeature(index) copies the stored pair, an index >= count throws
 *     ArgumentOutOfRangeException("fontFeatureIndex") which the COM boundary reports as E_INVALIDARG, leaving *fontFeature untouched;
 *   - the object implements IUnknown + IDWriteTypography only;
 *   - IDWriteTextLayout::SetTypography(typo, range): range.startPosition + length overflow => E_INVALIDARG; length 0 is a no-op; NULL typography is
 *     accepted and clears the range; the layout keeps a reference; GetTypography returns (AddRef'd) object or NULL plus the run's range. */

static const struct { DWRITE_FONT_FEATURE_TAG tag; const char *cc; } g_tags[] = {
    { DWRITE_FONT_FEATURE_TAG_ALTERNATIVE_FRACTIONS, "afrc" },
    { DWRITE_FONT_FEATURE_TAG_PETITE_CAPITALS_FROM_CAPITALS, "c2pc" },
    { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS_FROM_CAPITALS, "c2sc" },
    { DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES, "calt" },
    { DWRITE_FONT_FEATURE_TAG_CASE_SENSITIVE_FORMS, "case" },
    { DWRITE_FONT_FEATURE_TAG_GLYPH_COMPOSITION_DECOMPOSITION, "ccmp" },
    { DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES, "clig" },
    { DWRITE_FONT_FEATURE_TAG_CAPITAL_SPACING, "cpsp" },
    { DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_SWASH, "cswh" },
    { DWRITE_FONT_FEATURE_TAG_CURSIVE_POSITIONING, "curs" },
    { DWRITE_FONT_FEATURE_TAG_DEFAULT, "dflt" },
    { DWRITE_FONT_FEATURE_TAG_DISCRETIONARY_LIGATURES, "dlig" },
    { DWRITE_FONT_FEATURE_TAG_EXPERT_FORMS, "expt" },
    { DWRITE_FONT_FEATURE_TAG_FRACTIONS, "frac" },
    { DWRITE_FONT_FEATURE_TAG_FULL_WIDTH, "fwid" },
    { DWRITE_FONT_FEATURE_TAG_HALF_FORMS, "half" },
    { DWRITE_FONT_FEATURE_TAG_HALANT_FORMS, "haln" },
    { DWRITE_FONT_FEATURE_TAG_ALTERNATE_HALF_WIDTH, "halt" },
    { DWRITE_FONT_FEATURE_TAG_HISTORICAL_FORMS, "hist" },
    { DWRITE_FONT_FEATURE_TAG_HORIZONTAL_KANA_ALTERNATES, "hkna" },
    { DWRITE_FONT_FEATURE_TAG_HISTORICAL_LIGATURES, "hlig" },
    { DWRITE_FONT_FEATURE_TAG_HALF_WIDTH, "hwid" },
    { DWRITE_FONT_FEATURE_TAG_HOJO_KANJI_FORMS, "hojo" },
    { DWRITE_FONT_FEATURE_TAG_JIS04_FORMS, "jp04" },
    { DWRITE_FONT_FEATURE_TAG_JIS78_FORMS, "jp78" },
    { DWRITE_FONT_FEATURE_TAG_JIS83_FORMS, "jp83" },
    { DWRITE_FONT_FEATURE_TAG_JIS90_FORMS, "jp90" },
    { DWRITE_FONT_FEATURE_TAG_KERNING, "kern" },
    { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, "liga" },
    { DWRITE_FONT_FEATURE_TAG_LINING_FIGURES, "lnum" },
    { DWRITE_FONT_FEATURE_TAG_LOCALIZED_FORMS, "locl" },
    { DWRITE_FONT_FEATURE_TAG_MARK_POSITIONING, "mark" },
    { DWRITE_FONT_FEATURE_TAG_MATHEMATICAL_GREEK, "mgrk" },
    { DWRITE_FONT_FEATURE_TAG_MARK_TO_MARK_POSITIONING, "mkmk" },
    { DWRITE_FONT_FEATURE_TAG_ALTERNATE_ANNOTATION_FORMS, "nalt" },
    { DWRITE_FONT_FEATURE_TAG_NLC_KANJI_FORMS, "nlck" },
    { DWRITE_FONT_FEATURE_TAG_OLD_STYLE_FIGURES, "onum" },
    { DWRITE_FONT_FEATURE_TAG_ORDINALS, "ordn" },
    { DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_ALTERNATE_WIDTH, "palt" },
    { DWRITE_FONT_FEATURE_TAG_PETITE_CAPITALS, "pcap" },
    { DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_FIGURES, "pnum" },
    { DWRITE_FONT_FEATURE_TAG_PROPORTIONAL_WIDTHS, "pwid" },
    { DWRITE_FONT_FEATURE_TAG_QUARTER_WIDTHS, "qwid" },
    { DWRITE_FONT_FEATURE_TAG_REQUIRED_LIGATURES, "rlig" },
    { DWRITE_FONT_FEATURE_TAG_RUBY_NOTATION_FORMS, "ruby" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_ALTERNATES, "salt" },
    { DWRITE_FONT_FEATURE_TAG_SCIENTIFIC_INFERIORS, "sinf" },
    { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, "smcp" },
    { DWRITE_FONT_FEATURE_TAG_SIMPLIFIED_FORMS, "smpl" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_1, "ss01" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_2, "ss02" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_3, "ss03" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_4, "ss04" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_5, "ss05" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_6, "ss06" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_7, "ss07" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_8, "ss08" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_9, "ss09" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_10, "ss10" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_11, "ss11" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_12, "ss12" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_13, "ss13" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_14, "ss14" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_15, "ss15" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_16, "ss16" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_17, "ss17" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_18, "ss18" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_19, "ss19" },
    { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_20, "ss20" },
    { DWRITE_FONT_FEATURE_TAG_SUBSCRIPT, "subs" },
    { DWRITE_FONT_FEATURE_TAG_SUPERSCRIPT, "sups" },
    { DWRITE_FONT_FEATURE_TAG_SWASH, "swsh" },
    { DWRITE_FONT_FEATURE_TAG_TITLING, "titl" },
    { DWRITE_FONT_FEATURE_TAG_TRADITIONAL_NAME_FORMS, "tnam" },
    { DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, "tnum" },
    { DWRITE_FONT_FEATURE_TAG_TRADITIONAL_FORMS, "trad" },
    { DWRITE_FONT_FEATURE_TAG_THIRD_WIDTHS, "twid" },
    { DWRITE_FONT_FEATURE_TAG_UNICASE, "unic" },
    { DWRITE_FONT_FEATURE_TAG_VERTICAL_WRITING, "vert" },
    { DWRITE_FONT_FEATURE_TAG_VERTICAL_ALTERNATES_AND_ROTATION, "vrt2" },
    { DWRITE_FONT_FEATURE_TAG_SLASHED_ZERO, "zero" }
};

/* The last run of a layout may report its length either clamped to the text or open ended (UINT32_MAX - start); accept both. */
static int tail_len_ok(UINT32 start, UINT32 len, UINT32 want) { return len == want || len == 0xffffffffu - start || len == 0xffffffffu; }

static UINT32 cc32(const char *s) { return DW_TAG(s[0], s[1], s[2], s[3]); }

TEST(typo_tag_constants_are_little_endian_fourcc)
{
    unsigned i;
    for (i = 0; i < sizeof g_tags / sizeof g_tags[0]; i++)
        CHECKF((UINT32)g_tags[i].tag == cc32(g_tags[i].cc), "tag #%u: 0x%08x vs '%s'", i, (UINT32)g_tags[i].tag, g_tags[i].cc);
    EXPECT_EQ(sizeof g_tags / sizeof g_tags[0], 81);
}

TEST(typo_tag_well_known_values)
{
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_KERNING, 0x6e72656b);
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0x6167696c);
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 0x70636d73);
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS_FROM_CAPITALS, 0x63733263);
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 0x6d756e74);
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_OLD_STYLE_FIGURES, 0x6d756e6f);
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_1, 0x31307373);
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_20, 0x30327373);
    EXPECT_EQ(DWRITE_FONT_FEATURE_TAG_DISCRETIONARY_LIGATURES, 0x67696c64);
}

TEST(typo_tag_stylistic_sets_are_consecutive_ss01_to_ss20)
{
    int i; UINT32 base = DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_1;
    for (i = 1; i <= 20; i++) {
        char cc[5]; sprintf(cc, "ss%02d", i);
        UINT32 want = cc32(cc);
        UINT32 got = (i == 1) ? base : (i == 2) ? DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_2 : 0;
        if (i <= 2) EXPECT_EQ(got, want);
    }
    /* the whole ladder through the table */
    {   unsigned k, seen = 0;
        for (k = 0; k < sizeof g_tags / sizeof g_tags[0]; k++) if (!strncmp(g_tags[k].cc, "ss", 2)) seen++;
        EXPECT_EQ(seen, 20);
    }
}

TEST(typo_create_basic)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    CHECK(t != NULL);
    if (t) { EXPECT_REFS(t, 1); EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), 0); }
    DW_RELEASE(t);
}

TEST(typo_create_distinct_objects)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &a)); CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &b));
    CHECK(a && b && a != b);
    if (a && b) {
        DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_KERNING, 1 };
        CHECK_HR_OK(IDWriteTypography_AddFontFeature(a, ft));
        EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(a), 1);
        EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(b), 0);     /* no shared state */
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(typo_create_via_all_factory_versions)
{
    int v;
    for (v = 0; v <= 7; v++) {
        IDWriteFactory *fv = (v == 0) ? dw_shared() : (IDWriteFactory *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v);
        IDWriteTypography *t = NULL;
        if (!fv) { t_note("IDWriteFactory%d unsupported", v); continue; }
        CHECK_HR_OK(IDWriteFactory_CreateTypography(fv, &t)); CHECK(t != NULL); DW_RELEASE(t);
    }
}

TEST(typo_refcount_protocol)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; ULONG r;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    r = IDWriteTypography_AddRef(t); EXPECT_EQ(r, 2);
    r = IDWriteTypography_AddRef(t); EXPECT_EQ(r, 3);
    r = IDWriteTypography_Release(t); EXPECT_EQ(r, 2);
    r = IDWriteTypography_Release(t); EXPECT_EQ(r, 1);
    r = IDWriteTypography_Release(t); EXPECT_EQ(r, 0);
}

TEST(typo_qi_matrix)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; IUnknown *u = NULL, *u2 = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    NTF_QI_ONLY(t, &IID_IDWriteTypography);
    CHECK_HR_OK(IDWriteTypography_QueryInterface(t, &IID_IUnknown, (void **)&u));
    CHECK_HR_OK(IDWriteTypography_QueryInterface(t, &IID_IDWriteTypography, (void **)&u2));
    CHECK(u == (IUnknown *)t && u2 == (IUnknown *)t);
    DW_RELEASE(u); DW_RELEASE(u2);
    EXPECT_REFS(t, 1);
    DW_RELEASE(t);
}

TEST(typo_qi_failure_zeroes_out)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; void *pv = (void *)(ULONG_PTR)8;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    CHECK_HR(IDWriteTypography_QueryInterface(t, &IID_IDWriteNumberSubstitution, &pv), E_NOINTERFACE);
    CHECK(pv == NULL);
    DW_RELEASE(t);
}

TEST(typo_outlives_factory)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_LINING_FIGURES, 1 }, out;
    if (!f) { t_skip("no isolated factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    DW_RELEASE(f);
    if (!t) return;
    CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, ft));
    CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, 0, &out));
    EXPECT_EQ(out.nameTag, DWRITE_FONT_FEATURE_TAG_LINING_FIGURES);
    DW_RELEASE(t);
}

TEST(typo_add_and_get_roundtrip)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0 }, out = { 0, 0 };
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, ft));
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), 1);
    CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, 0, &out));
    EXPECT_EQ(out.nameTag, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES); EXPECT_EQ(out.parameter, 0);
    DW_RELEASE(t);
}

TEST(typo_add_table_all_tags_in_order)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; unsigned i, n = sizeof g_tags / sizeof g_tags[0];
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    for (i = 0; i < n; i++) { DWRITE_FONT_FEATURE ft; ft.nameTag = g_tags[i].tag; ft.parameter = i + 1; CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, ft)); }
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), n);
    for (i = 0; i < n; i++) {
        DWRITE_FONT_FEATURE out = { 0xdeadbeef, 0xdeadbeef };
        CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, i, &out));
        CHECKF(out.nameTag == g_tags[i].tag && out.parameter == i + 1, "feature %u: tag 0x%08x param %u", i, out.nameTag, out.parameter);
    }
    DW_RELEASE(t);
}

TEST(typo_add_duplicates_kept)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE a = { DWRITE_FONT_FEATURE_TAG_KERNING, 1 }, b = { DWRITE_FONT_FEATURE_TAG_KERNING, 0 }, o;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, a)); CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, b)); CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, a));
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), 3);         /* no de-duplication, no replacement */
    CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, 0, &o)); EXPECT_EQ(o.parameter, 1);
    CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, 1, &o)); EXPECT_EQ(o.parameter, 0);
    CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, 2, &o)); EXPECT_EQ(o.parameter, 1);
    DW_RELEASE(t);
}

TEST(typo_add_exact_duplicate_pairs)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE a = { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1 }; int i;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    for (i = 0; i < 10; i++) CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, a));
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), 10);
    DW_RELEASE(t);
}

TEST(typo_add_arbitrary_tags_and_parameters)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; unsigned i;
    static const struct { UINT32 tag, param; } v[] = {
        { 0, 0 }, { 0, 1 }, { 0xffffffffu, 0xffffffffu }, { 0x80000000u, 0x7fffffffu }, { 0x01010101u, 3 }, { DW_TAG('c', 'v', '0', '1'), 2 },
        { DW_TAG('c', 'v', '9', '9'), 99 }, { DW_TAG('x', 'x', 'x', 'x'), 5 }, { DW_TAG(' ', ' ', ' ', ' '), 1 }, { DW_TAG('k', 'e', 'r', 'n'), 0xfffffffeu },
        { DW_TAG('S', 'M', 'C', 'P'), 1 }, { DW_TAG('Z', 'Z', 'Z', 'Z'), 0 }, { 0x41, 1 }, { 0xff000000u, 1 },
    };
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        DWRITE_FONT_FEATURE ft; ft.nameTag = (DWRITE_FONT_FEATURE_TAG)v[i].tag; ft.parameter = v[i].param;
        CHECKF(IDWriteTypography_AddFontFeature(t, ft) == S_OK, "AddFontFeature(0x%08x, %u)", v[i].tag, v[i].param);     /* nothing is validated */
    }
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), sizeof v / sizeof v[0]);
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        DWRITE_FONT_FEATURE o = { 1, 1 };
        CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, i, &o));
        CHECKF(o.nameTag == v[i].tag && o.parameter == v[i].param, "entry %u: 0x%08x %u", i, o.nameTag, o.parameter);
    }
    DW_RELEASE(t);
}

TEST(typo_get_out_of_range_empty)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE o = { 0x11111111, 0x22222222 };
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    CHECK_HR(IDWriteTypography_GetFontFeature(t, 0, &o), E_INVALIDARG);
    EXPECT_EQ(o.nameTag, 0x11111111); EXPECT_EQ(o.parameter, 0x22222222);     /* untouched on failure */
    CHECK_HR(IDWriteTypography_GetFontFeature(t, 1, &o), E_INVALIDARG);
    CHECK_HR(IDWriteTypography_GetFontFeature(t, 0xffffffffu, &o), E_INVALIDARG);
    DW_RELEASE(t);
}

TEST(typo_get_out_of_range_table)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; unsigned n, k;
    static const unsigned counts[] = { 1, 2, 3, 7, 8, 9, 16, 17, 33 };
    for (k = 0; k < sizeof counts / sizeof counts[0]; k++) {
        UINT32 idx[5]; unsigned j;
        CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
        if (!t) return;
        for (n = 0; n < counts[k]; n++) { DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_KERNING + n, n }; CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, ft)); }
        idx[0] = counts[k] - 1; idx[1] = counts[k]; idx[2] = counts[k] + 1; idx[3] = 0x7fffffffu; idx[4] = 0xffffffffu;
        for (j = 0; j < 5; j++) {
            DWRITE_FONT_FEATURE o = { 5, 6 }; HRESULT hr = IDWriteTypography_GetFontFeature(t, idx[j], &o);
            if (j == 0) { CHECK_HR(hr, S_OK); EXPECT_EQ(o.parameter, counts[k] - 1); }
            else { CHECKF(hr == E_INVALIDARG, "count %u index %u -> %s", counts[k], idx[j], dw_hr_name(hr)); EXPECT_EQ(o.nameTag, 5); }
        }
        DW_RELEASE(t);
    }
}

TEST(typo_many_features_grow)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; UINT32 i;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    for (i = 0; i < 2000; i++) { DWRITE_FONT_FEATURE ft = { (DWRITE_FONT_FEATURE_TAG)(0x41414141u + i), i * 3 }; CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, ft)); }
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), 2000);
    for (i = 0; i < 2000; i += 97) { DWRITE_FONT_FEATURE o; CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, i, &o)); EXPECT_EQ(o.nameTag, 0x41414141u + i); EXPECT_EQ(o.parameter, i * 3); }
    { DWRITE_FONT_FEATURE o; CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, 1999, &o)); EXPECT_EQ(o.parameter, 1999 * 3); CHECK_HR(IDWriteTypography_GetFontFeature(t, 2000, &o), E_INVALIDARG); }
    DW_RELEASE(t);
}

TEST(typo_get_does_not_modify)
{
    IDWriteFactory *f = dw_shared(); IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_ORDINALS, 7 }, a, b;
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (!t) return;
    CHECK_HR_OK(IDWriteTypography_AddFontFeature(t, ft));
    CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, 0, &a)); a.parameter = 99;       /* the copy is independent */
    CHECK_HR_OK(IDWriteTypography_GetFontFeature(t, 0, &b));
    EXPECT_EQ(b.parameter, 7);
    DW_RELEASE(t);
}

/* ---- layout integration: Get/SetTypography ------------------------------------------------------------------------------ */

TEST(typo_layout_unset_returns_null_and_whole_range)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"abcdef", 20, 500, 100); IDWriteTypography *t = (IDWriteTypography *)(void *)0;
    DWRITE_TEXT_RANGE r = { 99, 99 };
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    t = NULL;
    CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 0, &t, &r));
    CHECK(t == NULL); EXPECT_EQ(r.startPosition, 0); CHECK(tail_len_ok(0, r.length, 6));
    t_note("unset typography range length %u", r.length);
    t = NULL; CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 5, &t, &r));
    CHECK(t == NULL); EXPECT_EQ(r.startPosition, 0); CHECK(tail_len_ok(0, r.length, 6));
    IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_layout_set_get_roundtrip_and_refcount)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"abcdef", 20, 500, 100); IDWriteTypography *t = NULL, *g = NULL; DWRITE_TEXT_RANGE r = { 0, 0 };
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTypography(X.f, &t));
    EXPECT_REFS(t, 1);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(1, 3)));
    EXPECT_REFS(t, 2);                                   /* the layout holds a reference */
    CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 2, &g, &r));
    CHECK(g == t); EXPECT_EQ(r.startPosition, 1); EXPECT_EQ(r.length, 3);
    EXPECT_REFS(t, 3);                                   /* GetTypography AddRefs the result */
    DW_RELEASE(g);
    EXPECT_REFS(t, 2);
    IDWriteTextLayout_Release(l);
    EXPECT_REFS(t, 1);                                   /* released with the layout */
    DW_RELEASE(t); NTF_END();
}

TEST(typo_layout_ranges_split_runs)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"abcdef", 20, 500, 100); IDWriteTypography *t = NULL, *g = NULL; DWRITE_TEXT_RANGE r;
    UINT32 pos; static const UINT32 want_s[] = { 0, 1, 1, 1, 4, 4 }, want_l[] = { 1, 3, 3, 3, 2, 2 }; static const int set[] = { 0, 1, 1, 1, 0, 0 };
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTypography(X.f, &t));
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(1, 3)));
    for (pos = 0; pos < 6; pos++) {
        g = NULL; r.startPosition = r.length = 77;
        CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, pos, &g, &r));
        CHECKF(r.startPosition == want_s[pos] && (r.length == want_l[pos] || (pos >= 4 && tail_len_ok(r.startPosition, r.length, want_l[pos]))), "pos %u: range %u+%u", pos, r.startPosition, r.length);
        CHECKF((g != NULL) == set[pos], "pos %u: object %p", pos, (void *)g);
        DW_RELEASE(g);
    }
    DW_RELEASE(t); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_layout_replace_and_clear)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"abcdef", 20, 500, 100); IDWriteTypography *a = NULL, *b = NULL, *g = NULL; DWRITE_TEXT_RANGE r;
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTypography(X.f, &a)); CHECK_HR_OK(IDWriteFactory_CreateTypography(X.f, &b));
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, a, NTF_RNG(0, 6)));
    EXPECT_REFS(a, 2);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, b, NTF_RNG(2, 2)));
    { ULONG ra = dw_refcount((IUnknown *)a); CHECKF(ra >= 2 && ra <= 3, "a refs %lu", ra); t_note("a is held by two disjoint runs: refs=%lu", ra); }
    EXPECT_REFS(b, 2);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, b, NTF_RNG(0, 6)));
    EXPECT_REFS(a, 1);                                  /* no run refers to a any more */
    CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 3, &g, &r)); CHECK(g == b); DW_RELEASE(g);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, NULL, NTF_RNG(0, 6)));      /* NULL clears */
    EXPECT_REFS(b, 1);
    CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 3, &g, &r)); CHECK(g == NULL);
    DW_RELEASE(a); DW_RELEASE(b); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_layout_set_invalid_range)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"abcdef", 20, 500, 100); IDWriteTypography *t = NULL; static const UINT32 st[] = { 0xffffffffu, 0xfffffff0u, 1, 7 };
    unsigned i;
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTypography(X.f, &t));
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(0xffffffffu, 2)), E_INVALIDARG);        /* start + length wraps around */
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(2, 0xffffffffu)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(0x80000000u, 0x80000000u)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(0xfffffff0u, 0x20)), E_INVALIDARG);
    EXPECT_REFS(t, 1);                                  /* rejected before anything was retained */
    for (i = 0; i < 4; i++) (void)st[i];
    DW_RELEASE(t); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_layout_set_zero_length_is_noop)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"abcdef", 20, 500, 100); IDWriteTypography *t = NULL, *g = NULL; DWRITE_TEXT_RANGE r;
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTypography(X.f, &t));
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(2, 0)));
    EXPECT_REFS(t, 1);
    CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 2, &g, &r)); CHECK(g == NULL);
    DW_RELEASE(t); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_layout_set_beyond_text_note)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"abcdef", 20, 500, 100); IDWriteTypography *t = NULL; HRESULT hr;
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTypography(X.f, &t));
    hr = IDWriteTextLayout_SetTypography(l, t, NTF_RNG(4, 100));
    t_note("SetTypography(4,100) on 6 chars -> %s refs=%lu", dw_hr_name(hr), dw_refcount((IUnknown *)t));
    hr = IDWriteTextLayout_SetTypography(l, t, NTF_RNG(100, 4));
    t_note("SetTypography(100,4) on 6 chars -> %s refs=%lu", dw_hr_name(hr), dw_refcount((IUnknown *)t));
    DW_RELEASE(t); IDWriteTextLayout_Release(l); NTF_END();
}

/* ---- effect on glyph selection (synthetic NtfMain: see ntf.h for the GSUB lookups) ----------------------------------------- */

static void layout_glyphs(ntf_fx *X, const WCHAR *text, IDWriteTypography *t, DWRITE_TEXT_RANGE rng, unsigned short *g, int *n)
{
    IDWriteTextLayout *l = ntf_layout(X, NTF_MAIN, text, 20, 1000, 100); dwx_renderer *r;
    *n = 0;
    if (!l) return;
    if (t) CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, rng));
    r = ntf_draw(l);
    *n = ntf_glyphs(r, g, 32);
    dwx_release(r); IDWriteTextLayout_Release(l);
}

static IDWriteTypography *mk(ntf_fx *X, DWRITE_FONT_FEATURE_TAG tag, UINT32 p, DWRITE_FONT_FEATURE_TAG tag2, UINT32 p2)
{
    IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE ft;
    if (FAILED(IDWriteFactory_CreateTypography(X->f, &t))) return NULL;
    ft.nameTag = tag; ft.parameter = p; IDWriteTypography_AddFontFeature(t, ft);
    if (tag2) { ft.nameTag = tag2; ft.parameter = p2; IDWriteTypography_AddFontFeature(t, ft); }
    return t;
}

TEST(typo_effect_default_liga_on)
{
    NTF_BEGIN(); unsigned short g[32]; int n;
    layout_glyphs(&X, L"ab", NULL, NTF_RNG(0, 0), g, &n);
    EXPECT_EQ(n, 1); if (n == 1) EXPECT_EQ(g[0], 67);          /* 'liga' is on by default: "ab" -> glyph 67 */
    layout_glyphs(&X, L"ba", NULL, NTF_RNG(0, 0), g, &n);
    EXPECT_EQ(n, 2); if (n == 2) { EXPECT_EQ(g[0], 29); EXPECT_EQ(g[1], 28); }
    NTF_END();
}

TEST(typo_effect_liga_off)
{
    NTF_BEGIN(); unsigned short g[32]; int n; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0, 0, 0);
    layout_glyphs(&X, L"ab", t, NTF_RNG(0, 2), g, &n);
    EXPECT_EQ(n, 2); if (n == 2) { EXPECT_EQ(g[0], 28); EXPECT_EQ(g[1], 29); }
    DW_RELEASE(t); NTF_END();
}

TEST(typo_effect_liga_explicit_on)
{
    NTF_BEGIN(); unsigned short g[32]; int n; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1, 0, 0);
    layout_glyphs(&X, L"ab", t, NTF_RNG(0, 2), g, &n);
    EXPECT_EQ(n, 1); if (n == 1) EXPECT_EQ(g[0], 67);
    DW_RELEASE(t); NTF_END();
}

TEST(typo_effect_liga_off_partial_range)
{
    NTF_BEGIN(); unsigned short g[32]; int n; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0, 0, 0);
    layout_glyphs(&X, L"ab ab", t, NTF_RNG(0, 2), g, &n);     /* only the first "ab" has liga off */
    EXPECT_EQ(n, 4); if (n == 4) { EXPECT_EQ(g[0], 28); EXPECT_EQ(g[1], 29); EXPECT_EQ(g[2], 1); EXPECT_EQ(g[3], 67); }
    DW_RELEASE(t); NTF_END();
}

TEST(typo_effect_dlig_on)
{
    NTF_BEGIN(); unsigned short g[32]; int n; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_DISCRETIONARY_LIGATURES, 1, 0, 0);
    layout_glyphs(&X, L"cd", NULL, NTF_RNG(0, 0), g, &n);
    EXPECT_EQ(n, 2); if (n == 2) { EXPECT_EQ(g[0], 30); EXPECT_EQ(g[1], 31); }      /* dlig is off by default */
    layout_glyphs(&X, L"cd", t, NTF_RNG(0, 2), g, &n);
    EXPECT_EQ(n, 1); if (n == 1) EXPECT_EQ(g[0], 64);
    DW_RELEASE(t); NTF_END();
}

typedef struct { DWRITE_FONT_FEATURE_TAG tag; UINT32 param; const WCHAR *text; unsigned short want[4]; int nwant; const char *what; } eff_row;

TEST(typo_effect_single_substitution_table)
{
    NTF_BEGIN(); unsigned i;
    static const eff_row rows[] = {
        { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, L"a", { 65 }, 1, "smcp a" },
        { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, L"b", { 64 }, 1, "smcp b" },
        { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, L"c", { 30 }, 1, "smcp leaves c" },
        { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 0, L"a", { 28 }, 1, "smcp param 0 is off" },
        { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 5, L"a", { 65 }, 1, "smcp param 5" },
        { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS_FROM_CAPITALS, 1, L"A", { 64 }, 1, "c2sc A" },
        { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS_FROM_CAPITALS, 1, L"a", { 28 }, 1, "c2sc leaves a" },
        { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_1, 1, L"b", { 68 }, 1, "ss01 b" },
        { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_1, 0, L"b", { 29 }, 1, "ss01 off" },
        { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_2, 1, L"b", { 64 }, 1, "ss02 b" },
        { DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_3, 1, L"b", { 29 }, 1, "ss03 absent from the font" },
        { DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1, L"b", { 29 }, 1, "tnum absent from the font" },
        { (DWRITE_FONT_FEATURE_TAG)DW_TAG('x', 'x', 'x', 'x'), 1, L"b", { 29 }, 1, "unknown tag" },
        { (DWRITE_FONT_FEATURE_TAG)0, 1, L"b", { 29 }, 1, "zero tag" },
    };
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTypography *t = mk(&X, rows[i].tag, rows[i].param, 0, 0); unsigned short g[32]; int n, k;
        layout_glyphs(&X, rows[i].text, t, NTF_RNG(0, (UINT32)wcslen(rows[i].text)), g, &n);
        CHECKF(n == rows[i].nwant, "%s: %d glyphs", rows[i].what, n);
        for (k = 0; k < n && k < rows[i].nwant; k++) CHECKF(g[k] == rows[i].want[k], "%s: glyph %d = %u want %u", rows[i].what, k, g[k], rows[i].want[k]);
        DW_RELEASE(t);
    }
    NTF_END();
}

TEST(typo_effect_not_applied_without_typography)
{
    NTF_BEGIN(); unsigned short g[32]; int n;
    layout_glyphs(&X, L"abcAB", NULL, NTF_RNG(0, 0), g, &n);
    EXPECT_EQ(n, 4);          /* "ab" ligature + c A B */
    if (n == 4) { EXPECT_EQ(g[0], 67); EXPECT_EQ(g[1], 30); EXPECT_EQ(g[2], 2); EXPECT_EQ(g[3], 3); }
    NTF_END();
}

TEST(typo_effect_range_limited)
{
    NTF_BEGIN(); unsigned short g[32]; int n; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, 0, 0);
    layout_glyphs(&X, L"a a a", t, NTF_RNG(2, 1), g, &n);
    EXPECT_EQ(n, 5);
    if (n == 5) { EXPECT_EQ(g[0], 28); EXPECT_EQ(g[1], 1); EXPECT_EQ(g[2], 65); EXPECT_EQ(g[3], 1); EXPECT_EQ(g[4], 28); }
    DW_RELEASE(t); NTF_END();
}

TEST(typo_effect_two_features_in_one_typography)
{
    NTF_BEGIN(); unsigned short g[32]; int n;
    IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1);
    layout_glyphs(&X, L"ab", t, NTF_RNG(0, 2), g, &n);
    EXPECT_EQ(n, 2); if (n == 2) { EXPECT_EQ(g[0], 65); EXPECT_EQ(g[1], 64); }       /* liga off lets smcp reach both letters */
    DW_RELEASE(t); NTF_END();
}

TEST(typo_effect_later_duplicate_wins_note)
{
    NTF_BEGIN(); unsigned short g[32]; int n;
    IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1);
    layout_glyphs(&X, L"ab", t, NTF_RNG(0, 2), g, &n);
    t_note("liga 0 then liga 1 on \"ab\": %d glyph(s), first %u", n, n ? g[0] : 0);
    CHECK(n == 1 || n == 2);
    DW_RELEASE(t); NTF_END();
}

TEST(typo_effect_two_typographies_two_ranges)
{
    NTF_BEGIN(); unsigned short g[32]; int n; IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"b b", 20, 1000, 100); dwx_renderer *r;
    IDWriteTypography *t1 = mk(&X, DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_1, 1, 0, 0), *t2 = mk(&X, DWRITE_FONT_FEATURE_TAG_STYLISTIC_SET_2, 1, 0, 0);
    if (!l) { DW_RELEASE(t1); DW_RELEASE(t2); NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t1, NTF_RNG(0, 1))); CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t2, NTF_RNG(2, 1)));
    r = ntf_draw(l); n = ntf_glyphs(r, g, 32);
    EXPECT_EQ(n, 3); if (n == 3) { EXPECT_EQ(g[0], 68); EXPECT_EQ(g[1], 1); EXPECT_EQ(g[2], 64); }
    dwx_release(r); DW_RELEASE(t1); DW_RELEASE(t2); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_effect_clear_restores_default)
{
    NTF_BEGIN(); unsigned short g[32]; int n; IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"ab", 20, 1000, 100); dwx_renderer *r;
    IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0, 0, 0);
    if (!l) { DW_RELEASE(t); NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(0, 2)));
    r = ntf_draw(l); n = ntf_glyphs(r, g, 32); EXPECT_EQ(n, 2); dwx_release(r);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, NULL, NTF_RNG(0, 2)));
    r = ntf_draw(l); n = ntf_glyphs(r, g, 32); EXPECT_EQ(n, 1); if (n == 1) EXPECT_EQ(g[0], 67); dwx_release(r);
    DW_RELEASE(t); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_effect_advance_widths_follow_glyphs)
{
    /* glyph 67 (the ligature) is 500 units wide: "ab" is 500 units with liga and 2000 units without */
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"ab", 20, 1000, 100); DWRITE_TEXT_METRICS m; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0, 0, 0);
    if (!l) { DW_RELEASE(t); NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m)); CHECK_NEAR(m.width, NTF_ADV(500, 20), 0.01);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(0, 2)));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m)); CHECK_NEAR(m.width, NTF_ADV(2000, 20), 0.01);
    DW_RELEASE(t); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_effect_kerning_default_and_off)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"AV", 20, 1000, 100); DWRITE_TEXT_METRICS m; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_KERNING, 0, 0, 0);
    if (!l) { DW_RELEASE(t); NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_NEAR(m.width, NTF_ADV(2000 - 200, 20), 0.02);          /* GPOS kern pair A,V = -200 units is applied by default */
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(0, 2)));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_NEAR(m.width, NTF_ADV(2000, 20), 0.02);                /* kern off */
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, NULL, NTF_RNG(0, 2)));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_NEAR(m.width, NTF_ADV(2000 - 200, 20), 0.02);
    DW_RELEASE(t); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_effect_kerning_pair_other_order_unaffected)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"VA", 20, 1000, 100); DWRITE_TEXT_METRICS m;
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_NEAR(m.width, NTF_ADV(2000, 20), 0.02);                /* only the (A,V) pair is kerned */
    IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_effect_kerning_explicit_on)
{
    NTF_BEGIN(); IDWriteTextLayout *l = ntf_layout(&X, NTF_MAIN, L"AV", 20, 1000, 100); DWRITE_TEXT_METRICS m; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_KERNING, 1, 0, 0);
    if (!l) { DW_RELEASE(t); NTF_END(); t_skip("no layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(0, 2)));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_NEAR(m.width, NTF_ADV(1800, 20), 0.02);
    DW_RELEASE(t); IDWriteTextLayout_Release(l); NTF_END();
}

TEST(typo_effect_arial_av_kerning_relation)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m1, m2; IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_KERNING, 0 };
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 48, L"en-us", &fmt));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"AVAVAV", 6, fmt, 1000, 200, &l));
    CHECK_HR_OK(IDWriteFactory_CreateTypography(f, &t));
    if (l && t) {
        IDWriteTypography_AddFontFeature(t, ft);
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m1));
        CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, t, NTF_RNG(0, 6)));
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m2));
        t_note("Arial 48px AVAVAV width kerned %g unkerned %g", m1.width, m2.width);
        CHECK(m1.width < m2.width);        /* Arial kerns A-V and V-A */
    }
    DW_RELEASE(t); DW_RELEASE(l); DW_RELEASE(fmt);
}

TEST(typo_effect_analyzer_features_on_getglyphs)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); unsigned short g[64], cl[64]; UINT32 n;
    IDWriteTypography *off = mk(&X, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0, 0, 0), *sc = mk(&X, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, 0, 0);
    if (!face) { DW_RELEASE(off); DW_RELEASE(sc); NTF_END(); t_skip("no face"); return; }
    CHECK_HR(ntf_shape(X.f, face, L"ab", L"en-us", NULL, NULL, g, cl, &n), S_OK); EXPECT_EQ(n, 1); if (n == 1) EXPECT_EQ(g[0], 67);
    CHECK_HR(ntf_shape(X.f, face, L"ab", L"en-us", NULL, off, g, cl, &n), S_OK); EXPECT_EQ(n, 2); if (n == 2) { EXPECT_EQ(g[0], 28); EXPECT_EQ(g[1], 29); }
    CHECK_HR(ntf_shape(X.f, face, L"a", L"en-us", NULL, sc, g, cl, &n), S_OK); EXPECT_EQ(n, 1); if (n == 1) EXPECT_EQ(g[0], 65);
    DW_RELEASE(off); DW_RELEASE(sc); DW_RELEASE(face); NTF_END();
}

TEST(typo_effect_analyzer_cluster_map_for_ligature)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); unsigned short g[64], cl[64]; UINT32 n;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    CHECK_HR(ntf_shape(X.f, face, L"xaby", L"en-us", NULL, NULL, g, cl, &n), S_OK);
    EXPECT_EQ(n, 3);
    if (n == 3) { EXPECT_EQ(cl[0], 0); EXPECT_EQ(cl[1], 1); EXPECT_EQ(cl[2], 1); EXPECT_EQ(cl[3], 2); }      /* "ab" share cluster glyph 1 */
    DW_RELEASE(face); NTF_END();
}

TEST(typo_effect_analyzer_feature_range_length_validation)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); IDWriteTextAnalyzer *an = NULL; IDWriteTypography *t = mk(&X, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1, 0, 0);
    DWRITE_SCRIPT_ANALYSIS sa; unsigned short cl[8], g[8]; DWRITE_SHAPING_TEXT_PROPERTIES tp[8]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[8]; UINT32 n = 99;
    DWRITE_FONT_FEATURE ff; DWRITE_TYPOGRAPHIC_FEATURES tf; const DWRITE_TYPOGRAPHIC_FEATURES *tfp = &tf; UINT32 rl; HRESULT hr;
    if (!face) { DW_RELEASE(t); NTF_END(); t_skip("no face"); return; }
    IDWriteTypography_GetFontFeature(t, 0, &ff); tf.features = &ff; tf.featureCount = 1;
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(X.f, &an));
    memset(&sa, 0, sizeof sa);
    rl = 2;       /* feature ranges cover 2 characters but the text has 1: the range length sum exceeds the text */
    hr = IDWriteTextAnalyzer_GetGlyphs(an, L"a", 1, face, FALSE, FALSE, &sa, L"en-us", NULL, &tfp, &rl, 1, 8, cl, tp, g, gp, &n);
    t_note("GetGlyphs with feature range longer than the text -> %s", dw_hr_name(hr));
    CHECK(hr == S_OK || hr == E_INVALIDARG);
    DW_RELEASE(an); DW_RELEASE(t); DW_RELEASE(face); NTF_END();
}
