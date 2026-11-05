/* IDWriteTextAnalyzer2::GetTypographicFeatures with fonts whose GSUB / GPOS tables are known byte for byte (otl.c).
 * Expectations come from IDWriteTextAnalyzer::GetTypographicFeatures, GetTypographicFeatures,
 * GetFeatureList, FindScriptLanguageIndexWithFallback and FontFeatureCoverageRegionBuilder::{ReadTable,
 * AddLanguageFeatures,WriteRegion} of the Windows 10 dwrite.dll. */
#include "r2.h"

#define MAXT 80
static const unsigned short G_A[] = { 2, 3, 4 };           /* 'A' 'B' 'C' */
static const unsigned short G_B[] = { 28, 29 };            /* 'a' 'b' */
static const unsigned short G_C[] = { 54, 55, 56, 57 };    /* '0'.. */

/* Adds a one-lookup feature (single substitution over g) and returns the feature index. */
static int feat1(otl_table *t, const char *tag, const unsigned short *g, int n) {
    int lk = otl_add_lookup(t, OTL_SINGLE, 1, 1, 0, g, n);
    return otl_add_feature(t, tag, 1, lk);
}
static int same_list(const DWRITE_FONT_FEATURE_TAG *t, UINT32 n, const char *const *want, UINT32 nw) {
    UINT32 i;
    if (n != nw) return 0;
    for (i = 0; i < n; i++) if ((UINT32)t[i] != r2_tag(want[i])) return 0;
    return 1;
}
static void dump(const char *what, const DWRITE_FONT_FEATURE_TAG *t, UINT32 n) {
    char b[400]; UINT32 i; int o = 0; b[0] = 0;
    for (i = 0; i < n && o < 380; i++) o += sprintf(b + o, "%c%c%c%c ", (char)(t[i] & 0xff), (char)((t[i] >> 8) & 0xff), (char)((t[i] >> 16) & 0xff), (char)((t[i] >> 24) & 0xff));
    t_note("%s: %u tags: %s", what, n, b);
}
/* Expect the exact feature list for (face, script, locale). */
static void expect_list(r2_ctx *c, IDWriteFontFace *face, DWRITE_SCRIPT_ANALYSIS sa, const WCHAR *loc, const char *const *want, UINT32 nw, const char *what) {
    DWRITE_FONT_FEATURE_TAG t[MAXT]; UINT32 n = 77; HRESULT hr;
    hr = r2_features(c, face, sa, loc, MAXT, &n, t);
    CHECKF(hr == S_OK, "%s: hr %s", what, dw_hr_name(hr));
    CHECKF(same_list(t, n, want, nw), "%s: got %u tags, wanted %u", what, n, nw);
    if (!same_list(t, n, want, nw)) dump(what, t, n);
}
#define FONT_OR_SKIP(x, gsub, gpos) \
    r2_font x; \
    if (FAILED(r2_font_open(&x, f, 0, gsub, gpos)) || !x.h.face) { r2_font_close(&x); R2_END(); t_skip("synthetic layout font rejected by the factory"); return; }
#define LATIN() r2_sa_of(&c_, L"A", 1)

/* ---- ordering ------------------------------------------------------------------------------------------------------------ */
RT(gt_order_is_alphabetical_not_table_order) {
    /* WriteRegion sorts each script/language feature list with a comparator on the byte-swapped (big endian) tag. */
    R2_BEGIN(); otl_table g; otl_init(&g);
    static const char *in[] = { "zero", "smcp", "liga", "aalt", "ss01", "c2sc", "onum", "ccmp", "dlig" };
    int s = otl_add_script(&g, "latn"), i, fi[16];
    for (i = 0; i < 9; i++) fi[i] = feat1(&g, in[i], G_A, 3);
    otl_add_langsys(&g, s, "dflt", -1, 9, fi[0], fi[1], fi[2], fi[3], fi[4], fi[5], fi[6], fi[7], fi[8]);
    otl_table p; otl_init(&p); int ps = otl_add_script(&p, "latn"); int k = feat1(&p, "kern", G_A, 3); otl_add_langsys(&p, ps, "dflt", -1, 1, k);
    FONT_OR_SKIP(x, &g, &p);
    static const char *want[] = { "aalt", "c2sc", "ccmp", "dlig", "kern", "liga", "onum", "smcp", "ss01", "zero" };
    expect_list(&c_, x.h.face, LATIN(), L"en-us", want, 10, "order");
    r2_font_close(&x); R2_END();
}
RT(gt_order_independent_of_reverse_input) {
    R2_BEGIN(); otl_table g; otl_init(&g);
    static const char *in[] = { "zero", "smcp", "liga", "kern", "dlig", "ccmp", "c2sc" };
    int s = otl_add_script(&g, "latn"), i; int fi[8];
    for (i = 0; i < 7; i++) fi[i] = feat1(&g, in[i], G_A, 3);
    otl_add_langsys(&g, s, "dflt", -1, 7, fi[6], fi[5], fi[4], fi[3], fi[2], fi[1], fi[0]);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "c2sc", "ccmp", "dlig", "kern", "liga", "smcp", "zero" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 7, "reverse");
    r2_font_close(&x); R2_END();
}
static int cmp_tag_text(const void *a, const void *b) { return memcmp(a, b, 4); }
/* Lists the first nf of 60 single-lookup features (ss30..ss01 then cv30..cv01, deliberately descending, all sharing one lookup)
 * and expects them back ascending by tag text. More than 32 elements are sorted with the partitioning sort rather than
 * insertion sort. Win10 22H2 returns all counts from 1 to 60 without a fault. */
static int check_large_sorted(r2_ctx *c_, int nf) {
    IDWriteFactory *f = c_->f;
    otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"), i, fi[64], lk;
    char tag[5], want[64][4]; DWRITE_FONT_FEATURE_TAG t[MAXT]; UINT32 n = 0;
    lk = otl_add_lookup(&g, OTL_SINGLE, 1, 1, 0, G_A, 3);
    for (i = 0; i < 30; i++) { sprintf(tag, "ss%02d", 30 - i); fi[i] = otl_add_feature(&g, tag, 1, lk); memcpy(want[i], tag, 4); }
    for (i = 0; i < 30; i++) { sprintf(tag, "cv%02d", 30 - i); fi[30 + i] = otl_add_feature(&g, tag, 1, lk); memcpy(want[30 + i], tag, 4); }
    { otl_langsys *l; otl_add_langsys(&g, s, "dflt", -1, 0); l = &g.sc[s].ls[0]; for (i = 0; i < nf; i++) l->f[l->nf++] = fi[i]; }
    qsort(want, nf, 4, cmp_tag_text);
    r2_font x;
    if (FAILED(r2_font_open(&x, f, 0, &g, NULL)) || !x.h.face) { r2_font_close(&x); return 0; }
    CHECK_HR(r2_features(c_, x.h.face, r2_sa_of(c_, L"A", 1), NULL, MAXT, &n, t), S_OK); EXPECT_EQ(n, nf);
    for (i = 0; i < nf && (int)n == nf; i++) CHECKF((UINT32)t[i] == r2_tag(want[i]), "count %d slot %d", nf, i);
    r2_font_close(&x);
    return 1;
}
RT(gt_order_forty_features_exercises_large_sort) { R2_BEGIN(); int ok = check_large_sorted(&c_, 40); R2_END(); if (!ok) t_skip("synthetic layout font rejected by the factory"); }
RT(gt_order_sixty_features_all_returned_ascending) { R2_BEGIN(); int ok = check_large_sorted(&c_, 60); R2_END(); if (!ok) t_skip("synthetic layout font rejected by the factory"); }
RT(gt_tags_are_the_raw_four_bytes_of_the_table) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"), k = feat1(&g, "kern", G_A, 3);
    DWRITE_FONT_FEATURE_TAG t[4]; UINT32 n = 0;
    otl_add_langsys(&g, s, "dflt", -1, 1, k);
    FONT_OR_SKIP(x, &g, NULL);
    CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, 4, &n, t), S_OK);
    EXPECT_EQ(n, 1); EXPECT_EQ((UINT32)t[0], 0x6e72656b);
    EXPECT_EQ((UINT32)t[0], (UINT32)DWRITE_FONT_FEATURE_TAG_KERNING);
    r2_font_close(&x); R2_END();
}

/* ---- merging ------------------------------------------------------------------------------------------------------------ */
RT(gt_duplicate_tag_listed_once) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn");
    int a = feat1(&g, "liga", G_A, 3), b = feat1(&g, "liga", G_B, 2), c = feat1(&g, "smcp", G_C, 4);
    otl_add_langsys(&g, s, "dflt", -1, 3, a, b, c);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "liga", "smcp" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 2, "dup");
    r2_font_close(&x); R2_END();
}
RT(gt_same_feature_index_twice_listed_once) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"), a = feat1(&g, "liga", G_A, 3);
    otl_add_langsys(&g, s, "dflt", -1, 3, a, a, a);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "liga" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 1, "same index");
    r2_font_close(&x); R2_END();
}
RT(gt_gsub_and_gpos_features_are_merged) {
    R2_BEGIN(); otl_table g, p; otl_init(&g); otl_init(&p);
    int gs = otl_add_script(&g, "latn"), g1 = feat1(&g, "liga", G_A, 3), g2 = feat1(&g, "smcp", G_B, 2);
    int ps = otl_add_script(&p, "latn"), p1 = feat1(&p, "kern", G_A, 3), p2 = feat1(&p, "liga", G_C, 4);
    otl_add_langsys(&g, gs, "dflt", -1, 2, g1, g2); otl_add_langsys(&p, ps, "dflt", -1, 2, p1, p2);
    FONT_OR_SKIP(x, &g, &p);
    static const char *want[] = { "kern", "liga", "smcp" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 3, "gsub+gpos");
    r2_font_close(&x); R2_END();
}
RT(gt_gpos_only_font) {
    R2_BEGIN(); otl_table p; otl_init(&p); int ps = otl_add_script(&p, "latn"), p1 = feat1(&p, "kern", G_A, 3), p2 = feat1(&p, "mark", G_B, 2);
    otl_add_langsys(&p, ps, "dflt", -1, 2, p1, p2);
    FONT_OR_SKIP(x, NULL, &p);
    static const char *want[] = { "kern", "mark" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 2, "gpos only");
    r2_font_close(&x); R2_END();
}
RT(gt_gsub_only_font) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"), a = feat1(&g, "liga", G_A, 3), b = feat1(&g, "ccmp", G_B, 2);
    otl_add_langsys(&g, s, "dflt", -1, 2, a, b);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "ccmp", "liga" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 2, "gsub only");
    r2_font_close(&x); R2_END();
}
RT(gt_gsub_and_gpos_languages_do_not_mix) {
    /* entries are keyed by (script, language): the GSUB default list and the GPOS 'TRK ' list are different entries */
    R2_BEGIN(); UINT32 trk = r2_ot_lang(L"tr-TR"); char lt[5];
    if (trk == R2_TAG('d', 'f', 'l', 't') || trk == r2_ot_lang(L"en-US")) { R2_END(); t_skip("tr-TR has no distinct OpenType language tag here"); return; }
    memcpy(lt, &trk, 4); lt[4] = 0;
    otl_table g, p; otl_init(&g); otl_init(&p);
    int gs = otl_add_script(&g, "latn"), g1 = feat1(&g, "liga", G_A, 3); otl_add_langsys(&g, gs, "dflt", -1, 1, g1);
    int ps = otl_add_script(&p, "latn"), p1 = feat1(&p, "kern", G_A, 3); otl_add_langsys(&p, ps, "dflt", -1, 0); otl_add_langsys(&p, ps, lt, -1, 1, p1);
    FONT_OR_SKIP(x, &g, &p);
    static const char *w_def[] = { "liga" }, *w_tr[] = { "kern" };
    expect_list(&c_, x.h.face, LATIN(), L"en-US", w_def, 1, "en-US");
    expect_list(&c_, x.h.face, LATIN(), L"tr-TR", w_tr, 1, "tr-TR");
    r2_font_close(&x); R2_END();
}

/* ---- buffer protocol ------------------------------------------------------------------------------------------------------ */
static void ten_font_tables(otl_table *g) {
    static const char *in[] = { "liga", "smcp", "onum", "kern", "calt" };
    int i, s, fi[5]; otl_init(g); s = otl_add_script(g, "latn");
    for (i = 0; i < 5; i++) fi[i] = feat1(g, in[i], G_A, 3);
    otl_add_langsys(g, s, "dflt", -1, 5, fi[0], fi[1], fi[2], fi[3], fi[4]);
}
RT(gt_short_buffer_receives_sorted_prefix) {
    /* GetFeatureList copies min(count, maxTagCount) tags and the caller gets 0x8007007A with the full count */
    R2_BEGIN(); otl_table g; UINT32 cap, n, i; DWRITE_FONT_FEATURE_TAG t[8];
    static const char *want[] = { "calt", "kern", "liga", "onum", "smcp" };
    ten_font_tables(&g);
    FONT_OR_SKIP(x, &g, NULL);
    for (cap = 0; cap <= 6; cap++) {
        HRESULT want_hr = cap < 5 ? R2_E_BUFFER : S_OK;
        for (i = 0; i < 8; i++) t[i] = (DWRITE_FONT_FEATURE_TAG)0x5A5A5A5A;
        n = 33;
        HRESULT hr = r2_features(&c_, x.h.face, LATIN(), NULL, cap, &n, cap ? t : NULL);
        CHECKF(hr == want_hr, "cap %u: %s", cap, dw_hr_name(hr));
        CHECKF(n == 5, "cap %u: count %u", cap, n);
        for (i = 0; i < 8; i++) {
            UINT32 expect = i < cap && i < 5 ? r2_tag(want[i]) : 0x5A5A5A5A;
            CHECKF((UINT32)t[i] == expect, "cap %u slot %u: 0x%08x want 0x%08x", cap, i, (UINT32)t[i], expect);
        }
    }
    r2_font_close(&x); R2_END();
}
RT(gt_count_cleared_before_validation) {
    R2_BEGIN(); otl_table g; UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4];
    ten_font_tables(&g);
    FONT_OR_SKIP(x, &g, NULL);
    if (!t_is_wine()) { CHECK_HR(r2_features(&c_, NULL, LATIN(), NULL, 4, &n, t), R2_E_INVALIDARG); EXPECT_EQ(n, 0); }
    n = 99; CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, 0, &n, NULL), R2_E_BUFFER); EXPECT_EQ(n, 5);
    r2_font_close(&x); R2_END();
}
RT(gt_null_tags_zero_capacity_for_empty_list) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"); UINT32 n = 99;
    otl_add_langsys(&g, s, "dflt", -1, 0);
    FONT_OR_SKIP(x, &g, NULL);
    CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, 0, &n, NULL), S_OK); EXPECT_EQ(n, 0);
    r2_font_close(&x); R2_END();
}
RT(gt_capacity_larger_than_count_leaves_tail) {
    R2_BEGIN(); otl_table g; UINT32 n = 0, i; DWRITE_FONT_FEATURE_TAG t[MAXT];
    ten_font_tables(&g);
    FONT_OR_SKIP(x, &g, NULL);
    for (i = 0; i < MAXT; i++) t[i] = (DWRITE_FONT_FEATURE_TAG)0x77777777;
    CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, MAXT, &n, t), S_OK); EXPECT_EQ(n, 5);
    for (i = 5; i < MAXT; i++) if ((UINT32)t[i] != 0x77777777) { CHECKF(0, "slot %u overwritten", i); break; }
    r2_font_close(&x); R2_END();
}
RT(gt_listing_is_idempotent) {
    R2_BEGIN(); otl_table g; UINT32 n1 = 0, n2 = 0; DWRITE_FONT_FEATURE_TAG a[MAXT], b[MAXT];
    ten_font_tables(&g);
    FONT_OR_SKIP(x, &g, NULL);
    CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, MAXT, &n1, a), S_OK); CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, MAXT, &n2, b), S_OK);
    CHECK(n1 == n2 && !memcmp(a, b, n1 * sizeof a[0]));
    r2_font_close(&x); R2_END();
}
RT(gt_second_analyzer_agrees) {
    R2_BEGIN(); otl_table g; UINT32 n1 = 0, n2 = 0; DWRITE_FONT_FEATURE_TAG a[MAXT], b[MAXT]; IDWriteTextAnalyzer *o = NULL; IDWriteTextAnalyzer2 *o2 = NULL;
    ten_font_tables(&g);
    FONT_OR_SKIP(x, &g, NULL);
    if (SUCCEEDED(IDWriteFactory_CreateTextAnalyzer(f, &o)) && o && SUCCEEDED(IDWriteTextAnalyzer_QueryInterface(o, &IID_IDWriteTextAnalyzer2, (void **)&o2))) {
        CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, MAXT, &n1, a), S_OK);
        CHECK_HR(IDWriteTextAnalyzer2_GetTypographicFeatures(o2, x.h.face, LATIN(), NULL, MAXT, &n2, b), S_OK);
        CHECK(n1 == n2 && !memcmp(a, b, n1 * sizeof a[0]));
    }
    DW_RELEASE(o2); DW_RELEASE(o);
    r2_font_close(&x); R2_END();
}
RT(gt_face_refcount_unchanged) {
    R2_BEGIN(); otl_table g; UINT32 n; DWRITE_FONT_FEATURE_TAG t[MAXT]; ULONG r0;
    ten_font_tables(&g);
    FONT_OR_SKIP(x, &g, NULL);
    r0 = dw_refcount((IUnknown *)x.h.face);
    r2_features(&c_, x.h.face, LATIN(), NULL, MAXT, &n, t); r2_features(&c_, x.h.face, LATIN(), NULL, 0, &n, NULL);
    EXPECT_REFS(x.h.face, r0);
    r2_font_close(&x); R2_END();
}

/* ---- required feature, empty lists ---------------------------------------------------------------------------------------- */
RT(gt_required_feature_is_listed) {
    /* AddLanguageFeatures starts with the LangSys required feature index (when it is not 0xFFFF), then the feature index list */
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn");
    int a = feat1(&g, "liga", G_A, 3), req = feat1(&g, "ccmp", G_B, 2);
    otl_add_langsys(&g, s, "dflt", req, 1, a);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "ccmp", "liga" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 2, "required");
    r2_font_close(&x); R2_END();
}
RT(gt_required_feature_alone_is_listed) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"); int req = feat1(&g, "rlig", G_B, 2);
    otl_add_langsys(&g, s, "dflt", req, 0);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "rlig" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 1, "required only");
    r2_font_close(&x); R2_END();
}
RT(gt_required_feature_also_in_list_counted_once) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"); int a = feat1(&g, "liga", G_A, 3);
    otl_add_langsys(&g, s, "dflt", a, 1, a);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "liga" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 1, "required twice");
    r2_font_close(&x); R2_END();
}
RT(gt_feature_without_lookups_is_listed) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"); int a = otl_add_feature(&g, "frac", 0), b = feat1(&g, "liga", G_A, 3);
    otl_add_langsys(&g, s, "dflt", -1, 2, a, b);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "frac", "liga" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 2, "no lookups");
    UINT8 out[3] = { 9, 9, 9 }; UINT16 gl[3] = { 2, 3, 4 };
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("frac"), 3, gl, out), S_OK);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[1], 0); EXPECT_EQ(out[2], 0);
    r2_font_close(&x); R2_END();
}
RT(gt_empty_script_list_lists_nothing) {
    R2_BEGIN(); otl_table g; otl_init(&g); int a = feat1(&g, "liga", G_A, 3); (void)a;
    UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4];
    FONT_OR_SKIP(x, &g, NULL);
    CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, 4, &n, t), S_OK); EXPECT_EQ(n, 0);
    r2_font_close(&x); R2_END();
}
RT(gt_empty_feature_list_lists_nothing) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"); UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4];
    otl_add_langsys(&g, s, "dflt", -1, 0);
    FONT_OR_SKIP(x, &g, NULL);
    CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, 4, &n, t), S_OK); EXPECT_EQ(n, 0);
    r2_font_close(&x); R2_END();
}
RT(gt_empty_script_found_blocks_fallback) {
    /* the script is found (its default LangSys exists) so DFLT is not consulted even though the list is empty */
    R2_BEGIN(); otl_table g; otl_init(&g); int sl = otl_add_script(&g, "latn"), sd = otl_add_script(&g, "DFLT"), a = feat1(&g, "liga", G_A, 3);
    UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4];
    otl_add_langsys(&g, sl, "dflt", -1, 0); otl_add_langsys(&g, sd, "dflt", -1, 1, a);
    FONT_OR_SKIP(x, &g, NULL);
    CHECK_HR(r2_features(&c_, x.h.face, LATIN(), NULL, 4, &n, t), S_OK); EXPECT_EQ(n, 0);
    static const char *want[] = { "liga" };
    expect_list(&c_, x.h.face, r2_sa_of(&c_, L"\x0416", 1), NULL, want, 1, "cyrillic via DFLT");
    r2_font_close(&x); R2_END();
}
RT(gt_extension_lookup_features_listed) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn");
    int lk = otl_add_lookup(&g, OTL_SINGLE, 1, 1, 1, G_A, 3), f1 = otl_add_feature(&g, "liga", 1, lk);
    otl_add_langsys(&g, s, "dflt", -1, 1, f1);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "liga" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 1, "extension");
    r2_font_close(&x); R2_END();
}
RT(gt_each_lookup_type_lists_its_feature) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn");
    int l1 = otl_add_lookup(&g, OTL_SINGLE, 1, 1, 0, G_A, 3), l2 = otl_add_lookup(&g, OTL_MULTIPLE, 1, 1, 0, G_A, 3), l3 = otl_add_lookup(&g, OTL_ALTERNATE, 1, 1, 0, G_A, 3), l4 = otl_add_lookup(&g, OTL_LIGATURE, 1, 1, 0, G_A, 3);
    int f1 = otl_add_feature(&g, "smcp", 1, l1), f2 = otl_add_feature(&g, "ccmp", 1, l2), f3 = otl_add_feature(&g, "salt", 1, l3), f4 = otl_add_feature(&g, "liga", 1, l4);
    otl_add_langsys(&g, s, "dflt", -1, 4, f1, f2, f3, f4);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *want[] = { "ccmp", "liga", "salt", "smcp" };
    expect_list(&c_, x.h.face, LATIN(), NULL, want, 4, "types");
    r2_font_close(&x); R2_END();
}

/* ---- script selection ---------------------------------------------------------------------------------------------------- */
static void three_scripts(otl_table *g, int with_dflt, int with_latn, int with_arab) {
    int s, a, b, c;
    otl_init(g);
    a = feat1(g, "zero", G_A, 3); b = feat1(g, "liga", G_B, 2); c = feat1(g, "calt", G_C, 4);
    if (with_dflt) { s = otl_add_script(g, "DFLT"); otl_add_langsys(g, s, "dflt", -1, 1, a); }
    if (with_latn) { s = otl_add_script(g, "latn"); otl_add_langsys(g, s, "dflt", -1, 1, b); }
    if (with_arab) { s = otl_add_script(g, "arab"); otl_add_langsys(g, s, "dflt", -1, 1, c); }
}
RT(gt_script_own_tag_wins) {
    R2_BEGIN(); otl_table g; three_scripts(&g, 1, 1, 1);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *w_zero[] = { "zero" }, *w_liga[] = { "liga" }, *w_calt[] = { "calt" };
    expect_list(&c_, x.h.face, LATIN(), NULL, w_liga, 1, "latin");
    expect_list(&c_, x.h.face, r2_sa_of(&c_, L"\x0627", 1), NULL, w_calt, 1, "arabic");
    expect_list(&c_, x.h.face, r2_sa_of(&c_, L"\x0416", 1), NULL, w_zero, 1, "cyrillic -> DFLT");
    r2_font_close(&x); R2_END();
}
RT(gt_dflt_script_is_tried_before_latn) {
    /* FindScriptLanguageIndexWithFallback: own tags, then 'DFLT' (0x544c4644), then 'latn' (0x6e74616c) */
    R2_BEGIN(); otl_table g; three_scripts(&g, 1, 1, 0);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *w_zero[] = { "zero" };
    static const WCHAR *samples[] = { L"\x0391", L"\x05D0", L"\x4E00", L"\x0E01", L"\x0627" };
    unsigned i;
    for (i = 0; i < sizeof samples / sizeof *samples; i++) expect_list(&c_, x.h.face, r2_sa_of(&c_, samples[i], 1), NULL, w_zero, 1, "non latin via DFLT");
    r2_font_close(&x); R2_END();
}
RT(gt_latn_is_the_last_resort) {
    R2_BEGIN(); otl_table g; three_scripts(&g, 0, 1, 1);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *w_liga[] = { "liga" };
    static const WCHAR *samples[] = { L"\x0391", L"\x05D0", L"\x4E00", L"\x0E01" };
    unsigned i;
    for (i = 0; i < sizeof samples / sizeof *samples; i++) expect_list(&c_, x.h.face, r2_sa_of(&c_, samples[i], 1), NULL, w_liga, 1, "non latin via latn");
    r2_font_close(&x); R2_END();
}
RT(gt_no_matching_script_lists_nothing) {
    R2_BEGIN(); otl_table g; three_scripts(&g, 0, 0, 1);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *w_calt[] = { "calt" };
    static const WCHAR *samples[] = { L"A", L"\x0391", L"\x05D0", L"\x4E00" };
    unsigned i; UINT32 n; DWRITE_FONT_FEATURE_TAG t[4];
    for (i = 0; i < sizeof samples / sizeof *samples; i++) { n = 99; CHECK_HR(r2_features(&c_, x.h.face, r2_sa_of(&c_, samples[i], 1), NULL, 4, &n, t), S_OK); EXPECT_EQ(n, 0); }
    expect_list(&c_, x.h.face, r2_sa_of(&c_, L"\x0627", 1), NULL, w_calt, 1, "arabic");
    r2_font_close(&x); R2_END();
}
RT(gt_script_index_out_of_range_behaves_like_index_zero) {
    /* GetScriptProperties-style masking: index >= 0x99 is replaced by 0 before the table is read */
    R2_BEGIN(); otl_table g; three_scripts(&g, 1, 1, 1);
    FONT_OR_SKIP(x, &g, NULL);
    DWRITE_SCRIPT_ANALYSIS z = LATIN(), big; UINT32 n0 = 0, n1 = 0; DWRITE_FONT_FEATURE_TAG t0[4], t1[4]; unsigned i;
    static const UINT16 ids[] = { 0x99, 0x9A, 0x100, 0x7FFF, 0xFFFF };
    z.script = 0; z.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    CHECK_HR(r2_features(&c_, x.h.face, z, NULL, 4, &n0, t0), S_OK);
    for (i = 0; i < sizeof ids / sizeof *ids; i++) {
        big = z; big.script = ids[i]; n1 = 99;
        CHECK_HR(r2_features(&c_, x.h.face, big, NULL, 4, &n1, t1), S_OK);
        CHECKF(n1 == n0 && !memcmp(t0, t1, n0 * sizeof t0[0]), "script id 0x%x", ids[i]);
    }
    r2_font_close(&x); R2_END();
}
RT(gt_every_script_index_resolves_to_a_font_script) {
    R2_BEGIN(); otl_table g; three_scripts(&g, 1, 1, 0);
    FONT_OR_SKIP(x, &g, NULL);
    DWRITE_SCRIPT_ANALYSIS sa = LATIN(), lat = LATIN(); unsigned i, via_latn = 0; UINT32 n; DWRITE_FONT_FEATURE_TAG t[4];
    for (i = 0; i < 153; i++) {
        sa.script = (UINT16)i; n = 99;
        HRESULT hr = r2_features(&c_, x.h.face, sa, NULL, 4, &n, t);
        CHECKF(hr == S_OK && n == 1, "index %u: %s n=%u", i, dw_hr_name(hr), n);
        if (n == 1 && (UINT32)t[0] == r2_tag("liga")) { via_latn++; if (i != lat.script) t_note("script index %u selects the 'latn' script", i); }
        else if (n == 1) CHECKF((UINT32)t[0] == r2_tag("zero"), "index %u: unexpected feature", i);
    }
    CHECKF(via_latn >= 1, "the latin index selected 'latn'");
    r2_font_close(&x); R2_END();
}
RT(gt_script_tag_table) {
    /* a font with DFLT{zero} and one specific script {liga}: the sample text must pick the specific script when the tag is the one
     * the analysis script maps to; rows marked soft only note a deviation */
    static const struct { const WCHAR *s; const char *tag; int hard; const char *name; } rows[] = {
        { L"A", "latn", 1, "Latin" }, { L"\x0391", "grek", 1, "Greek" }, { L"\x0416", "cyrl", 1, "Cyrillic" }, { L"\x0531", "armn", 1, "Armenian" },
        { L"\x05D0", "hebr", 1, "Hebrew" }, { L"\x0627", "arab", 1, "Arabic" }, { L"\x0710", "syrc", 1, "Syriac" }, { L"\x0780", "thaa", 1, "Thaana" },
        { L"\x0E01", "thai", 1, "Thai" }, { L"\x0E81", "lao ", 1, "Lao" }, { L"\x0F40", "tibt", 1, "Tibetan" }, { L"\x10D0", "geor", 1, "Georgian" },
        { L"\x1200", "ethi", 1, "Ethiopic" }, { L"\x13A0", "cher", 1, "Cherokee" }, { L"\x1780", "khmr", 1, "Khmer" }, { L"\x1820", "mong", 1, "Mongolian" },
        { L"\x3042", "kana", 1, "Hiragana" }, { L"\x30A2", "kana", 1, "Katakana" }, { L"\x4E00", "hani", 1, "Han" }, { L"\xAC00", "hang", 0, "Hangul" },
        { L"\x3105", "bopo", 1, "Bopomofo" }, { L"\xA000", "yi  ", 1, "Yi" }, { L"\x1401", "cans", 1, "Canadian Syllabics" }, { L"\x1680", "ogam", 1, "Ogham" },
        { L"\x16A0", "runr", 1, "Runic" }, { L"\x0D85", "sinh", 1, "Sinhala" }, { L"\x1000", "mymr", 0, "Myanmar" }, { L"\x07C0", "nko ", 0, "N'Ko" },
    };
    R2_BEGIN(); unsigned i;
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        otl_table g; otl_init(&g);
        int a = feat1(&g, "zero", G_A, 3), b = feat1(&g, "liga", G_B, 2), sd = otl_add_script(&g, "DFLT"), ss;
        otl_add_langsys(&g, sd, "dflt", -1, 1, a);
        if (strcmp(rows[i].tag, "latn")) { ss = otl_add_script(&g, rows[i].tag); otl_add_langsys(&g, ss, "dflt", -1, 1, b); }
        else { ss = otl_add_script(&g, "latn"); otl_add_langsys(&g, ss, "dflt", -1, 1, b); }
        FONT_OR_SKIP(x, &g, NULL);
        DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, rows[i].s, (UINT32)lstrlenW(rows[i].s)); UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4];
        HRESULT hr = r2_features(&c_, x.h.face, sa, NULL, 4, &n, t);
        int ok = hr == S_OK && n == 1 && (UINT32)t[0] == r2_tag("liga");
        if (rows[i].hard) CHECKF(ok, "%s selects script tag '%s' (script index %u, n=%u)", rows[i].name, rows[i].tag, sa.script, n);
        else if (!ok) t_note("%s: script tag '%s' not selected (script index %u, n=%u)", rows[i].name, rows[i].tag, sa.script, n);
        r2_font_close(&x);
    }
    R2_END();
}
RT(gt_indic_scripts_accept_old_and_new_tags) {
    /* Indic scripts have two OpenType tags (e.g. 'deva' and 'dev2'); a font carrying either one is served */
    static const struct { const WCHAR *s; const char *old_tag, *new_tag; const char *name; } rows[] = {
        { L"\x0905", "deva", "dev2", "Devanagari" }, { L"\x0985", "beng", "bng2", "Bengali" }, { L"\x0A05", "guru", "gur2", "Gurmukhi" },
        { L"\x0A85", "gujr", "gjr2", "Gujarati" }, { L"\x0B05", "orya", "ory2", "Oriya" }, { L"\x0B85", "taml", "tml2", "Tamil" },
        { L"\x0C05", "telu", "tel2", "Telugu" }, { L"\x0C85", "knda", "knd2", "Kannada" }, { L"\x0D05", "mlym", "mlm2", "Malayalam" },
    };
    R2_BEGIN(); unsigned i, k;
    for (i = 0; i < sizeof rows / sizeof *rows; i++) for (k = 0; k < 3; k++) {
        otl_table g; otl_init(&g);
        int a = feat1(&g, "zero", G_A, 3), b = feat1(&g, "liga", G_B, 2), sd = otl_add_script(&g, "DFLT"), ss;
        otl_add_langsys(&g, sd, "dflt", -1, 1, a);
        if (k == 0 || k == 2) { ss = otl_add_script(&g, rows[i].old_tag); otl_add_langsys(&g, ss, "dflt", -1, 1, b); }
        if (k == 1 || k == 2) { ss = otl_add_script(&g, rows[i].new_tag); otl_add_langsys(&g, ss, "dflt", -1, 1, b); }
        FONT_OR_SKIP(x, &g, NULL);
        DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, rows[i].s, 1); UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4];
        HRESULT hr = r2_features(&c_, x.h.face, sa, NULL, 4, &n, t);
        int ok = hr == S_OK && n == 1 && (UINT32)t[0] == r2_tag("liga");
        if (i < 3 || (k == 2)) CHECKF(ok, "%s variant %u (0 old, 1 new, 2 both): n=%u", rows[i].name, k, n);
        else if (!ok) t_note("%s variant %u: not selected", rows[i].name, k);
        r2_font_close(&x);
    }
    R2_END();
}
RT(gt_indic_both_tags_prefers_which_note) {
    R2_BEGIN(); otl_table g; otl_init(&g);
    int a = feat1(&g, "zero", G_A, 3), b = feat1(&g, "liga", G_B, 2), s1 = otl_add_script(&g, "deva"), s2 = otl_add_script(&g, "dev2");
    otl_add_langsys(&g, s1, "dflt", -1, 1, a); otl_add_langsys(&g, s2, "dflt", -1, 1, b);
    FONT_OR_SKIP(x, &g, NULL);
    DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, L"\x0905", 1); UINT32 n = 99; DWRITE_FONT_FEATURE_TAG t[4];
    CHECK_HR(r2_features(&c_, x.h.face, sa, NULL, 4, &n, t), S_OK); EXPECT_EQ(n, 1);
    CHECK(n == 1 && ((UINT32)t[0] == r2_tag("zero") || (UINT32)t[0] == r2_tag("liga")));
    t_note("Devanagari with both 'deva' and 'dev2': %s", n == 1 && (UINT32)t[0] == r2_tag("liga") ? "dev2 (new tag) wins" : "deva wins");
    r2_font_close(&x); R2_END();
}

/* ---- language selection --------------------------------------------------------------------------------------------------- */
RT(gt_language_specific_lists) {
    /* LangGetOpenTypeLanguageTag(locale) = GetLocaleInfoEx(locale, LOCALE_SOPENTYPELANGUAGETAG), 'dflt' for "" or an unknown name;
     * the fixture uses the tags the OS itself reports so that it holds on every Windows 10 */
    R2_BEGIN(); UINT32 tr = r2_ot_lang(L"tr-TR"), de = r2_ot_lang(L"de-DE"), en = r2_ot_lang(L"en-US"), dflt = R2_TAG('d', 'f', 'l', 't'); char ttr[5], tde[5];
    if (tr == dflt || de == dflt || tr == de || tr == en || de == en) { R2_END(); t_skip("locales lack distinct OpenType language tags"); return; }
    memcpy(ttr, &tr, 4); ttr[4] = 0; memcpy(tde, &de, 4); tde[4] = 0;
    otl_table g; otl_init(&g);
    int a = feat1(&g, "liga", G_A, 3), b = feat1(&g, "smcp", G_B, 2), c = feat1(&g, "zero", G_C, 4), s = otl_add_script(&g, "latn");
    otl_add_langsys(&g, s, "dflt", -1, 1, a); otl_add_langsys(&g, s, ttr, -1, 2, a, b); otl_add_langsys(&g, s, tde, -1, 1, c);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *w_def[] = { "liga" }, *w_tr[] = { "liga", "smcp" }, *w_de[] = { "zero" };
    expect_list(&c_, x.h.face, LATIN(), L"tr-TR", w_tr, 2, "tr-TR");
    expect_list(&c_, x.h.face, LATIN(), L"de-DE", w_de, 1, "de-DE");
    expect_list(&c_, x.h.face, LATIN(), L"en-US", w_def, 1, "en-US");
    expect_list(&c_, x.h.face, LATIN(), NULL, w_def, 1, "NULL");
    expect_list(&c_, x.h.face, LATIN(), L"", w_def, 1, "empty");
    expect_list(&c_, x.h.face, LATIN(), L"cadabra", w_def, 1, "unknown locale");
    expect_list(&c_, x.h.face, LATIN(), L"x", w_def, 1, "x");
    r2_font_close(&x); R2_END();
}
RT(gt_locale_name_case_insensitive) {
    R2_BEGIN(); UINT32 tr = r2_ot_lang(L"tr-TR"), dflt = R2_TAG('d', 'f', 'l', 't'); char ttr[5];
    if (tr == dflt || tr == r2_ot_lang(L"en-US")) { R2_END(); t_skip("tr-TR has no distinct OpenType language tag"); return; }
    memcpy(ttr, &tr, 4); ttr[4] = 0;
    otl_table g; otl_init(&g); int a = feat1(&g, "liga", G_A, 3), b = feat1(&g, "smcp", G_B, 2), s = otl_add_script(&g, "latn");
    otl_add_langsys(&g, s, "dflt", -1, 1, a); otl_add_langsys(&g, s, ttr, -1, 1, b);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *w_tr[] = { "smcp" };
    expect_list(&c_, x.h.face, LATIN(), L"TR-tr", w_tr, 1, "TR-tr");
    expect_list(&c_, x.h.face, LATIN(), L"tr-tr", w_tr, 1, "tr-tr");
    r2_font_close(&x); R2_END();
}
RT(gt_script_without_default_langsys_needs_language_match) {
    /* latn has only a language system for tr-TR: other locales fall through to DFLT, tr-TR is served by latn */
    R2_BEGIN(); UINT32 tr = r2_ot_lang(L"tr-TR"), dflt = R2_TAG('d', 'f', 'l', 't'); char ttr[5];
    if (tr == dflt || tr == r2_ot_lang(L"en-US")) { R2_END(); t_skip("tr-TR has no distinct OpenType language tag"); return; }
    memcpy(ttr, &tr, 4); ttr[4] = 0;
    otl_table g; otl_init(&g); int a = feat1(&g, "zero", G_A, 3), b = feat1(&g, "smcp", G_B, 2), sl = otl_add_script(&g, "latn"), sd = otl_add_script(&g, "DFLT");
    otl_add_langsys(&g, sl, ttr, -1, 1, b); otl_add_langsys(&g, sd, "dflt", -1, 1, a);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *w_dflt[] = { "zero" }, *w_tr[] = { "smcp" };
    expect_list(&c_, x.h.face, LATIN(), L"en-US", w_dflt, 1, "en-US -> DFLT");
    expect_list(&c_, x.h.face, LATIN(), NULL, w_dflt, 1, "NULL -> DFLT");
    expect_list(&c_, x.h.face, LATIN(), L"tr-TR", w_tr, 1, "tr-TR -> latn/TRK");
    r2_font_close(&x); R2_END();
}
RT(gt_language_does_not_leak_across_scripts) {
    R2_BEGIN(); UINT32 tr = r2_ot_lang(L"tr-TR"), dflt = R2_TAG('d', 'f', 'l', 't'); char ttr[5];
    if (tr == dflt || tr == r2_ot_lang(L"en-US")) { R2_END(); t_skip("tr-TR has no distinct OpenType language tag"); return; }
    memcpy(ttr, &tr, 4); ttr[4] = 0;
    otl_table g; otl_init(&g); int a = feat1(&g, "zero", G_A, 3), b = feat1(&g, "smcp", G_B, 2), sl = otl_add_script(&g, "latn"), sa_ = otl_add_script(&g, "arab");
    otl_add_langsys(&g, sl, "dflt", -1, 1, a); otl_add_langsys(&g, sa_, "dflt", -1, 1, a); otl_add_langsys(&g, sa_, ttr, -1, 1, b);
    FONT_OR_SKIP(x, &g, NULL);
    static const char *w_zero[] = { "zero" }, *w_smcp[] = { "smcp" };
    expect_list(&c_, x.h.face, LATIN(), L"tr-TR", w_zero, 1, "latin tr-TR");
    expect_list(&c_, x.h.face, r2_sa_of(&c_, L"\x0627", 1), L"tr-TR", w_smcp, 1, "arabic tr-TR");
    r2_font_close(&x); R2_END();
}
RT(gt_locale_ot_tag_matches_os_database_note) {
    static const WCHAR *loc[] = { L"en-US", L"tr-TR", L"de-DE", L"ar-SA", L"he-IL", L"th-TH", L"ja-JP", L"ko-KR", L"zh-CN", L"hi-IN", L"ru-RU", L"fr-FR" };
    unsigned i;
    for (i = 0; i < sizeof loc / sizeof *loc; i++) { UINT32 t = r2_ot_lang(loc[i]); t_note("LOCALE_SOPENTYPELANGUAGETAG %ls = %c%c%c%c", loc[i], (char)t, (char)(t >> 8), (char)(t >> 16), (char)(t >> 24)); }
    CHECK(r2_ot_lang(L"") == R2_TAG('d', 'f', 'l', 't'));
}

/* ---- consistency with CheckTypographicFeature ------------------------------------------------------------------------------ */
RT(gt_every_listed_feature_applies_to_its_coverage) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn");
    int a = feat1(&g, "liga", G_A, 3), b = feat1(&g, "smcp", G_B, 2), c = feat1(&g, "zero", G_C, 4);
    otl_add_langsys(&g, s, "dflt", -1, 3, a, b, c);
    FONT_OR_SKIP(x, &g, NULL);
    DWRITE_FONT_FEATURE_TAG t[MAXT]; UINT32 n = 0, i; DWRITE_SCRIPT_ANALYSIS la = LATIN();
    CHECK_HR(r2_features(&c_, x.h.face, la, NULL, MAXT, &n, t), S_OK); EXPECT_EQ(n, 3);
    for (i = 0; i < n; i++) {
        const unsigned short *g_ = (UINT32)t[i] == r2_tag("liga") ? G_A : (UINT32)t[i] == r2_tag("smcp") ? G_B : G_C;
        CHECKF(r2_covered(&c_, x.h.face, la, NULL, t[i], g_[0]) == 1, "feature %u not applying to its own glyph", i);
        CHECKF(r2_covered(&c_, x.h.face, la, NULL, t[i], 1) == 0, "feature %u applies to space", i);
    }
    r2_font_close(&x); R2_END();
}
RT(gt_unlisted_feature_never_applies) {
    R2_BEGIN(); otl_table g; otl_init(&g); int s = otl_add_script(&g, "latn"), a = feat1(&g, "liga", G_A, 3);
    otl_add_langsys(&g, s, "dflt", -1, 1, a);
    FONT_OR_SKIP(x, &g, NULL);
    unsigned i; DWRITE_SCRIPT_ANALYSIS la = LATIN();
    static const char *others[] = { "smcp", "kern", "zero", "ccmp", "    ", "abcd" };
    for (i = 0; i < sizeof others / sizeof *others; i++) CHECKF(r2_covered(&c_, x.h.face, la, NULL, r2_tag(others[i]), 2) == 0, "tag %s", others[i]);
    CHECK(r2_covered(&c_, x.h.face, la, NULL, 0, 2) == 0);
    CHECK(r2_covered(&c_, x.h.face, la, NULL, r2_tag("liga"), 2) == 1);
    r2_font_close(&x); R2_END();
}
