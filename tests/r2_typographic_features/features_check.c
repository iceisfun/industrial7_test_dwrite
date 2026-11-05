/* IDWriteTextAnalyzer2::CheckTypographicFeature with fonts whose GSUB / GPOS lookups cover known glyph sets (otl.c).
 * Windows 10 facts (IDWriteTextAnalyzer::CheckTypographicFeature, CheckTypographicFeature,
 * ReadLookupCoverageRanges, CoverageTableHelper::ReadLookupCoverage):
 *   - featureApplies[0..glyphCount) is cleared first (even on failure); the call fails with E_INVALIDARG only for fontFace NULL or
 *     glyphIndices NULL with a non zero count.
 *   - A glyph is flagged when it is in the coverage table of some subtable of some lookup of the feature selected through the same
 *     script / language fallback as GetTypographicFeatures (single, multiple, alternate, ligature (first glyph) and GPOS lookups;
 *     extension lookups are unwrapped). Everything else, including glyphs beyond the font, is 0.
 */
#include "r2.h"

#define LATIN() r2_sa_of(&c_, L"A", 1)
#define FONT_OR_SKIP(x, nglyphs, gsub, gpos) \
    r2_font x; \
    if (FAILED(r2_font_open(&x, f, nglyphs, gsub, gpos)) || !x.h.face) { r2_font_close(&x); R2_END(); t_skip("synthetic layout font rejected by the factory"); return; }

static otl_table *one_feature(otl_table *t, const char *tag, int type, int fmt, int nsub, int ext, const unsigned short *g, int n) {
    int lk, ft, s;
    otl_init(t);
    lk = otl_add_lookup(t, type, fmt, nsub, ext, g, n); ft = otl_add_feature(t, tag, 1, lk);
    s = otl_add_script(t, "latn"); otl_add_langsys(t, s, "dflt", -1, 1, ft);
    return t;
}
/* All glyphs 0..nglyphs-1 queried at once; expect exactly the members of set. */
static void expect_set(r2_ctx *c, IDWriteFontFace *face, DWRITE_SCRIPT_ANALYSIS sa, const WCHAR *loc, const char *tag, int nglyphs, const unsigned short *set, int nset, const char *what) {
    UINT16 gl[1024]; UINT8 out[1024]; int i, j, bad = 0;
    for (i = 0; i < nglyphs; i++) gl[i] = (UINT16)i;
    memset(out, 0xEE, sizeof out);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c->a2, face, sa, loc, (DWRITE_FONT_FEATURE_TAG)r2_tag(tag), nglyphs, gl, out), S_OK);
    for (i = 0; i < nglyphs; i++) {
        int in = 0; for (j = 0; j < nset; j++) if (set[j] == i) in = 1;
        if (out[i] != in) { if (bad++ < 6) CHECKF(0, "%s: glyph %d flagged %u, member %d", what, i, out[i], in); }
    }
    if (!bad) CHECK(1);
}

RT(ck_single_format1_coverage_exact) {
    R2_BEGIN(); otl_table t; static const unsigned short g[] = { 2, 3, 5, 40, 41, 68 };
    one_feature(&t, "liga", OTL_SINGLE, 1, 1, 0, g, 6);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, g, 6, "single/fmt1");
    r2_font_close(&x); R2_END();
}
RT(ck_single_format2_range_coverage_exact) {
    R2_BEGIN(); otl_table t; static const unsigned short g[] = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 30, 33, 34, 35 };
    one_feature(&t, "liga", OTL_SINGLE, 2, 1, 0, g, 15);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, g, 15, "single/fmt2");
    r2_font_close(&x); R2_END();
}
RT(ck_multiple_lookup_coverage) {
    R2_BEGIN(); otl_table t; static const unsigned short g[] = { 4, 6, 8, 9, 50 };
    one_feature(&t, "ccmp", OTL_MULTIPLE, 1, 1, 0, g, 5);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "ccmp", 69, g, 5, "multiple");
    r2_font_close(&x); R2_END();
}
RT(ck_alternate_lookup_coverage) {
    R2_BEGIN(); otl_table t; static const unsigned short g[] = { 2, 28, 54 };
    one_feature(&t, "salt", OTL_ALTERNATE, 1, 1, 0, g, 3);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "salt", 69, g, 3, "alternate");
    r2_font_close(&x); R2_END();
}
RT(ck_alternate_format2_coverage) {
    R2_BEGIN(); otl_table t; static const unsigned short g[] = { 20, 21, 22, 23, 24, 25, 26, 27 };
    one_feature(&t, "salt", OTL_ALTERNATE, 2, 1, 0, g, 8);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "salt", 69, g, 8, "alternate/fmt2");
    r2_font_close(&x); R2_END();
}
RT(ck_ligature_first_glyph_is_covered) {
    /* ligatures are keyed by their first glyph: those glyphs must be flagged; flags of the component glyph (3) are only noted */
    R2_BEGIN(); otl_table t; static const unsigned short g[] = { 10, 11, 12 };
    one_feature(&t, "liga", OTL_LIGATURE, 1, 1, 0, g, 3);
    FONT_OR_SKIP(x, 0, &t, NULL);
    DWRITE_SCRIPT_ANALYSIS sa = LATIN(); int i;
    for (i = 0; i < 3; i++) CHECKF(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("liga"), g[i]) == 1, "glyph %u", g[i]);
    for (i = 0; i < 69; i++) if (i != 3 && i != 10 && i != 11 && i != 12) CHECKF(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("liga"), (UINT16)i) == 0, "glyph %d", i);
    t_note("ligature component glyph 3 flagged: %d", r2_covered(&c_, x.h.face, sa, NULL, r2_tag("liga"), 3));
    r2_font_close(&x); R2_END();
}
RT(ck_extension_lookup_is_unwrapped) {
    R2_BEGIN(); otl_table t; static const unsigned short g[] = { 3, 4, 5, 20 };
    one_feature(&t, "smcp", OTL_SINGLE, 1, 1, 1, g, 4);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "smcp", 69, g, 4, "gsub extension");
    r2_font_close(&x); R2_END();
}
RT(ck_extension_alternate_and_ligature_unwrapped) {
    R2_BEGIN(); otl_table t; static const unsigned short g[] = { 6, 7 }; int s, l1, l2, f1, f2; static const unsigned short h[] = { 30, 31 };
    otl_init(&t); l1 = otl_add_lookup(&t, OTL_ALTERNATE, 1, 1, 1, g, 2); l2 = otl_add_lookup(&t, OTL_LIGATURE, 2, 1, 1, h, 2);
    f1 = otl_add_feature(&t, "salt", 1, l1); f2 = otl_add_feature(&t, "liga", 1, l2);
    s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", -1, 2, f1, f2);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "salt", 69, g, 2, "ext alternate");
    DWRITE_SCRIPT_ANALYSIS sa = LATIN();
    CHECK(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("liga"), 30) == 1); CHECK(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("liga"), 31) == 1);
    r2_font_close(&x); R2_END();
}
RT(ck_union_over_split_subtables) {
    R2_BEGIN(); otl_table t; unsigned short g[24]; int i;
    for (i = 0; i < 24; i++) g[i] = (unsigned short)(2 + 2 * i);          /* 2,4,...,48 */
    one_feature(&t, "liga", OTL_SINGLE, 1, 4, 0, g, 24);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, g, 24, "4 subtables");
    r2_font_close(&x); R2_END();
}
RT(ck_union_over_split_subtables_format2_extension) {
    R2_BEGIN(); otl_table t; unsigned short g[30]; int i;
    for (i = 0; i < 30; i++) g[i] = (unsigned short)(5 + i + (i >= 10) * 3 + (i >= 20) * 4);
    one_feature(&t, "liga", OTL_SINGLE, 2, 3, 1, g, 30);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, g, 30, "3 ext subtables");
    r2_font_close(&x); R2_END();
}
RT(ck_union_over_lookups_of_one_feature) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 3 }, b[] = { 10, 11 }, c[] = { 40 }, u[] = { 2, 3, 10, 11, 40 };
    int l1, l2, l3, f1, s; otl_init(&t);
    l1 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, a, 2); l2 = otl_add_lookup(&t, OTL_ALTERNATE, 2, 1, 0, b, 2); l3 = otl_add_lookup(&t, OTL_MULTIPLE, 1, 1, 0, c, 1);
    f1 = otl_add_feature(&t, "liga", 3, l1, l2, l3); s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", -1, 1, f1);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, u, 5, "3 lookups");
    r2_font_close(&x); R2_END();
}
RT(ck_features_have_independent_coverage) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 3, 4 }, b[] = { 3, 4, 5, 6 }; int l1, l2, f1, f2, s; otl_init(&t);
    l1 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, a, 3); l2 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, b, 4);
    f1 = otl_add_feature(&t, "liga", 1, l1); f2 = otl_add_feature(&t, "smcp", 1, l2);
    s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", -1, 2, f1, f2);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, a, 3, "liga");
    expect_set(&c_, x.h.face, LATIN(), NULL, "smcp", 69, b, 4, "smcp");
    r2_font_close(&x); R2_END();
}
RT(ck_features_sharing_a_lookup) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 3, 4 }; int l1, f1, f2, s; otl_init(&t);
    l1 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, a, 3);
    f1 = otl_add_feature(&t, "liga", 1, l1); f2 = otl_add_feature(&t, "clig", 1, l1);
    s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", -1, 2, f1, f2);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, a, 3, "liga");
    expect_set(&c_, x.h.face, LATIN(), NULL, "clig", 69, a, 3, "clig");
    r2_font_close(&x); R2_END();
}
RT(ck_duplicate_feature_records_union) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 3 }, b[] = { 10, 11 }, u[] = { 2, 3, 10, 11 }; int l1, l2, f1, f2, s; otl_init(&t);
    l1 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, a, 2); l2 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, b, 2);
    f1 = otl_add_feature(&t, "liga", 1, l1); f2 = otl_add_feature(&t, "liga", 1, l2);
    s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", -1, 2, f1, f2);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, u, 4, "duplicate liga");
    r2_font_close(&x); R2_END();
}
RT(ck_required_feature_coverage) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 3 }, b[] = { 20, 21 }; int l1, l2, f1, f2, s; otl_init(&t);
    l1 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, a, 2); l2 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, b, 2);
    f1 = otl_add_feature(&t, "liga", 1, l1); f2 = otl_add_feature(&t, "rlig", 1, l2);
    s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", f2, 1, f1);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "rlig", 69, b, 2, "required");
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, a, 2, "optional");
    r2_font_close(&x); R2_END();
}
RT(ck_gsub_and_gpos_same_tag_union) {
    R2_BEGIN(); otl_table g, p; static const unsigned short a[] = { 2, 3 }, b[] = { 10, 11 }, u[] = { 2, 3, 10, 11 };
    one_feature(&g, "mark", OTL_SINGLE, 1, 1, 0, a, 2); one_feature(&p, "mark", OTL_SINGLE, 1, 1, 0, b, 2);
    FONT_OR_SKIP(x, 0, &g, &p);
    expect_set(&c_, x.h.face, LATIN(), NULL, "mark", 69, u, 4, "GSUB+GPOS");
    r2_font_close(&x); R2_END();
}
RT(ck_gpos_single_adjustment_coverage) {
    R2_BEGIN(); otl_table p; static const unsigned short a[] = { 2, 3, 30 };
    one_feature(&p, "kern", OTL_SINGLE, 1, 1, 0, a, 3);
    FONT_OR_SKIP(x, 0, NULL, &p);
    expect_set(&c_, x.h.face, LATIN(), NULL, "kern", 69, a, 3, "GPOS single");
    r2_font_close(&x); R2_END();
}
RT(ck_gpos_pair_first_glyph_is_covered) {
    R2_BEGIN(); otl_table p; static const unsigned short a[] = { 2, 4, 28 };
    one_feature(&p, "kern", OTL_MULTIPLE, 1, 1, 0, a, 3);
    FONT_OR_SKIP(x, 0, NULL, &p);
    DWRITE_SCRIPT_ANALYSIS sa = LATIN(); int i;
    for (i = 0; i < 3; i++) CHECKF(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("kern"), a[i]) == 1, "first glyph %u", a[i]);
    for (i = 40; i < 69; i++) CHECKF(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("kern"), (UINT16)i) == 0, "glyph %d", i);
    t_note("pair second glyph 3 flagged: %d", r2_covered(&c_, x.h.face, sa, NULL, r2_tag("kern"), 3));
    r2_font_close(&x); R2_END();
}
RT(ck_gpos_extension_pair_first_glyph) {
    R2_BEGIN(); otl_table p; static const unsigned short a[] = { 12, 13 };
    one_feature(&p, "kern", OTL_MULTIPLE, 2, 1, 1, a, 2);
    FONT_OR_SKIP(x, 0, NULL, &p);
    DWRITE_SCRIPT_ANALYSIS sa = LATIN();
    CHECK(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("kern"), 12) == 1); CHECK(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("kern"), 13) == 1);
    CHECK(r2_covered(&c_, x.h.face, sa, NULL, r2_tag("kern"), 14) == 0);
    r2_font_close(&x); R2_END();
}
/* ---- large coverage sets (bit-packed ranges) --------------------------------------------------------------------------------------- */
static int run_pattern(const char *name, const unsigned short *g, int n, int fmt, int nglyphs) {
    r2_ctx c_; otl_table t; int ok = 0;
    if (!r2_open(&c_)) { r2_close(&c_); return 0; }
    IDWriteFactory *f = c_.f;
    one_feature(&t, "liga", OTL_SINGLE, fmt, 1, 0, g, n);
    {
        r2_font x;
        if (SUCCEEDED(r2_font_open(&x, f, nglyphs, &t, NULL)) && x.h.face) {
            expect_set(&c_, x.h.face, LATIN(), NULL, "liga", nglyphs, g, n, name);
            ok = 1;
        }
        r2_font_close(&x);
    }
    r2_close(&c_);
    return ok;
}
RT(ck_sparse_every_third_glyph) {
    unsigned short g[96]; int i;
    for (i = 0; i < 96; i++) g[i] = (unsigned short)(1 + 3 * i);
    if (!run_pattern("every third", g, 96, 1, 300)) t_skip("font rejected");
}
RT(ck_sparse_every_third_glyph_format2) {
    unsigned short g[96]; int i;
    for (i = 0; i < 96; i++) g[i] = (unsigned short)(1 + 3 * i);
    if (!run_pattern("every third/fmt2", g, 96, 2, 300)) t_skip("font rejected");
}
RT(ck_pairs_straddling_16_glyph_boundaries) {
    unsigned short g[96]; int i, n = 0;
    for (i = 1; i < 12 && n < 94; i++) { g[n++] = (unsigned short)(16 * i - 1); g[n++] = (unsigned short)(16 * i); }
    if (!run_pattern("16 boundary pairs", g, n, 1, 300)) t_skip("font rejected");
}
RT(ck_pairs_straddling_16_glyph_boundaries_format2) {
    unsigned short g[96]; int i, n = 0;
    for (i = 1; i < 12 && n < 94; i++) { g[n++] = (unsigned short)(16 * i - 1); g[n++] = (unsigned short)(16 * i); }
    if (!run_pattern("16 boundary pairs/fmt2", g, n, 2, 300)) t_skip("font rejected");
}
RT(ck_one_long_run_then_gap_then_run) {
    unsigned short g[96]; int i, n = 0;
    for (i = 100; i < 140; i++) g[n++] = (unsigned short)i;
    for (i = 200; i < 230; i++) g[n++] = (unsigned short)i;
    if (!run_pattern("two runs", g, n, 2, 300)) t_skip("font rejected");
}
RT(ck_isolated_first_and_last_glyphs) {
    unsigned short g[] = { 0, 1, 298, 299 };
    if (!run_pattern("ends", g, 4, 1, 300)) t_skip("font rejected");
}
RT(ck_glyph_zero_coverage) {
    unsigned short g[] = { 0 };
    if (!run_pattern("notdef only", g, 1, 1, 69)) t_skip("font rejected");
}
RT(ck_dense_block_of_96) {
    unsigned short g[96]; int i;
    for (i = 0; i < 96; i++) g[i] = (unsigned short)(40 + i);
    if (!run_pattern("dense 40..135", g, 96, 1, 300)) t_skip("font rejected");
}
RT(ck_dense_block_of_96_format2) {
    unsigned short g[96]; int i;
    for (i = 0; i < 96; i++) g[i] = (unsigned short)(40 + i);
    if (!run_pattern("dense 40..135/fmt2", g, 96, 2, 300)) t_skip("font rejected");
}
RT(ck_irregular_gaps) {
    unsigned short g[48]; int i, v = 3;
    for (i = 0; i < 48; i++) { g[i] = (unsigned short)v; v += 1 + (i * 7 + 3) % 6; }
    if (!run_pattern("irregular", g, 48, 1, 400)) t_skip("font rejected");
}

/* ---- script / language selection ------------------------------------------------------------------------------------------------ */
RT(ck_coverage_follows_script_selection) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 3 }, b[] = { 10, 11 }, d[] = { 20, 21 };
    int l1, l2, l3, f1, f2, f3, s; otl_init(&t);
    l1 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, a, 2); l2 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, b, 2); l3 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, d, 2);
    f1 = otl_add_feature(&t, "liga", 1, l1); f2 = otl_add_feature(&t, "liga", 1, l2); f3 = otl_add_feature(&t, "liga", 1, l3);
    s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", -1, 1, f1);
    s = otl_add_script(&t, "arab"); otl_add_langsys(&t, s, "dflt", -1, 1, f2);
    s = otl_add_script(&t, "DFLT"); otl_add_langsys(&t, s, "dflt", -1, 1, f3);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, a, 2, "latin");
    expect_set(&c_, x.h.face, r2_sa_of(&c_, L"\x0627", 1), NULL, "liga", 69, b, 2, "arabic");
    expect_set(&c_, x.h.face, r2_sa_of(&c_, L"\x0391", 1), NULL, "liga", 69, d, 2, "greek -> DFLT");
    r2_font_close(&x); R2_END();
}
RT(ck_coverage_follows_language_selection) {
    R2_BEGIN(); UINT32 tr = r2_ot_lang(L"tr-TR"), dflt = R2_TAG('d', 'f', 'l', 't'); char ttr[5];
    if (tr == dflt || tr == r2_ot_lang(L"en-US")) { R2_END(); t_skip("tr-TR has no distinct OpenType language tag"); return; }
    memcpy(ttr, &tr, 4); ttr[4] = 0;
    otl_table t; static const unsigned short a[] = { 2, 3 }, b[] = { 10, 11 }; int l1, l2, f1, f2, s; otl_init(&t);
    l1 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, a, 2); l2 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, b, 2);
    f1 = otl_add_feature(&t, "liga", 1, l1); f2 = otl_add_feature(&t, "liga", 1, l2);
    s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", -1, 1, f1); otl_add_langsys(&t, s, ttr, -1, 1, f2);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), L"tr-TR", "liga", 69, b, 2, "tr-TR");
    expect_set(&c_, x.h.face, LATIN(), L"en-US", "liga", 69, a, 2, "en-US");
    expect_set(&c_, x.h.face, LATIN(), NULL, "liga", 69, a, 2, "NULL locale");
    r2_font_close(&x); R2_END();
}
RT(ck_feature_absent_in_selected_language_is_false) {
    R2_BEGIN(); UINT32 tr = r2_ot_lang(L"tr-TR"), dflt = R2_TAG('d', 'f', 'l', 't'); char ttr[5];
    if (tr == dflt || tr == r2_ot_lang(L"en-US")) { R2_END(); t_skip("tr-TR has no distinct OpenType language tag"); return; }
    memcpy(ttr, &tr, 4); ttr[4] = 0;
    otl_table t; static const unsigned short a[] = { 2, 3 }; int l1, f1, f2, s; otl_init(&t);
    l1 = otl_add_lookup(&t, OTL_SINGLE, 1, 1, 0, a, 2);
    f1 = otl_add_feature(&t, "liga", 1, l1); f2 = otl_add_feature(&t, "smcp", 1, l1);
    s = otl_add_script(&t, "latn"); otl_add_langsys(&t, s, "dflt", -1, 2, f1, f2); otl_add_langsys(&t, s, ttr, -1, 1, f1);
    FONT_OR_SKIP(x, 0, &t, NULL);
    expect_set(&c_, x.h.face, LATIN(), L"tr-TR", "smcp", 69, a, 0, "smcp in tr-TR");
    expect_set(&c_, x.h.face, LATIN(), L"en-US", "smcp", 69, a, 2, "smcp in en-US");
    r2_font_close(&x); R2_END();
}
RT(ck_script_index_out_of_range_like_index_zero) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 3 };
    one_feature(&t, "liga", OTL_SINGLE, 1, 1, 0, a, 2);
    FONT_OR_SKIP(x, 0, &t, NULL);
    DWRITE_SCRIPT_ANALYSIS sa = LATIN(); sa.script = 0xFFFF;
    UINT16 gl[3] = { 2, 3, 4 }; UINT8 out[3] = { 9, 9, 9 };
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, sa, NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 3, gl, out), S_OK);
    t_note("script 0xFFFF flags: %u %u %u (the font only has 'latn')", out[0], out[1], out[2]);
    r2_font_close(&x); R2_END();
}

/* ---- buffer behaviour ------------------------------------------------------------------------------------------------------------------ */
RT(ck_output_order_follows_input_with_repeats) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 9 };
    one_feature(&t, "liga", OTL_SINGLE, 1, 1, 0, a, 2);
    FONT_OR_SKIP(x, 0, &t, NULL);
    UINT16 gl[8] = { 5, 2, 5, 9, 2, 0, 9, 3 }; UINT8 out[8]; static const UINT8 want[8] = { 0, 1, 0, 1, 1, 0, 1, 0 }; int i;
    memset(out, 0xEE, sizeof out);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 8, gl, out), S_OK);
    for (i = 0; i < 8; i++) CHECKF(out[i] == want[i], "slot %d: %u", i, out[i]);
    CHECK(gl[0] == 5 && gl[1] == 2 && gl[7] == 3);
    r2_font_close(&x); R2_END();
}
RT(ck_only_requested_count_is_written) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2, 3, 4, 5 };
    one_feature(&t, "liga", OTL_SINGLE, 1, 1, 0, a, 4);
    FONT_OR_SKIP(x, 0, &t, NULL);
    UINT16 gl[4] = { 2, 3, 4, 5 }; UINT8 out[6] = { 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE };
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 2, gl, out), S_OK);
    EXPECT_EQ(out[0], 1); EXPECT_EQ(out[1], 1); EXPECT_EQ(out[2], 0xEE); EXPECT_EQ(out[5], 0xEE);
    r2_font_close(&x); R2_END();
}
RT(ck_stale_outputs_are_cleared_first) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2 };
    one_feature(&t, "liga", OTL_SINGLE, 1, 1, 0, a, 1);
    FONT_OR_SKIP(x, 0, &t, NULL);
    UINT16 gl[5] = { 3, 3, 3, 2, 3 }; UINT8 out[5] = { 1, 1, 1, 1, 1 };
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 5, gl, out), S_OK);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[2], 0); EXPECT_EQ(out[3], 1); EXPECT_EQ(out[4], 0);
    r2_font_close(&x); R2_END();
}
RT(ck_large_glyph_array) {
    R2_BEGIN(); otl_table t; unsigned short a[40]; int i;
    for (i = 0; i < 40; i++) a[i] = (unsigned short)(i + 20);
    one_feature(&t, "liga", OTL_SINGLE, 2, 1, 0, a, 40);
    FONT_OR_SKIP(x, 0, &t, NULL);
    static UINT16 gl[5000]; static UINT8 out[5000]; int bad = 0;
    for (i = 0; i < 5000; i++) gl[i] = (UINT16)(i * 7 % 200);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 5000, gl, out), S_OK);
    for (i = 0; i < 5000; i++) { int in = gl[i] >= 20 && gl[i] < 60; if (out[i] != in) bad++; }
    CHECKF(bad == 0, "%d wrong flags", bad);
    r2_font_close(&x); R2_END();
}
RT(ck_glyphs_beyond_the_font_are_false) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2 };
    one_feature(&t, "liga", OTL_SINGLE, 1, 1, 0, a, 1);
    FONT_OR_SKIP(x, 0, &t, NULL);
    UINT16 gl[6] = { 69, 70, 1000, 0x7FFF, 0x8000, 0xFFFF }; UINT8 out[6]; int i;
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 6, gl, out), S_OK);
    for (i = 0; i < 6; i++) CHECKF(out[i] == 0, "glyph 0x%x", gl[i]);
    r2_font_close(&x); R2_END();
}
RT(ck_null_buffer_with_zero_count_ok_with_layout_font) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2 };
    one_feature(&t, "liga", OTL_SINGLE, 1, 1, 0, a, 1);
    FONT_OR_SKIP(x, 0, &t, NULL);
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 0, NULL, NULL), S_OK);
    UINT8 out[2] = { 7, 7 };
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 2, NULL, out), R2_E_INVALIDARG);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[1], 0);
    r2_font_close(&x); R2_END();
}
RT(ck_face_refcount_unchanged) {
    R2_BEGIN(); otl_table t; static const unsigned short a[] = { 2 };
    one_feature(&t, "liga", OTL_SINGLE, 1, 1, 0, a, 1);
    FONT_OR_SKIP(x, 0, &t, NULL);
    ULONG r0 = dw_refcount((IUnknown *)x.h.face); UINT16 g = 2; UINT8 o;
    IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("liga"), 1, &g, &o);
    EXPECT_REFS(x.h.face, r0);
    r2_font_close(&x); R2_END();
}
RT(ck_font_without_gsub_gpos_all_false_even_for_stale_buffer) {
    R2_BEGIN(); r2_font x;
    if (FAILED(r2_font_open(&x, f, 0, NULL, NULL)) || !x.h.face) { r2_font_close(&x); R2_END(); t_skip("font rejected"); return; }
    UINT16 gl[3] = { 2, 3, 4 }; UINT8 out[3] = { 0xFF, 0xFF, 0xFF };
    CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(c_.a2, x.h.face, LATIN(), NULL, (DWRITE_FONT_FEATURE_TAG)r2_tag("kern"), 3, gl, out), S_OK);
    EXPECT_EQ(out[0], 0); EXPECT_EQ(out[1], 0); EXPECT_EQ(out[2], 0);
    r2_font_close(&x); R2_END();
}
