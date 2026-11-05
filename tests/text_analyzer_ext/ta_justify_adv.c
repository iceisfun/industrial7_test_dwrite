/* IDWriteTextAnalyzer1::JustifyGlyphAdvances and ::GetJustifiedGlyphs.
 *
 * JustifyGlyphAdvances (DWriteTextAnalyzer::JustifyGlyphAdvances -> TextAnalyzer::GetJustifiedGlyphAdvanceAdjustments +
 * ApplyGlyphAdvanceAdjustments, natural measuring mode):
 *   - lineWidth < 0 or NaN -> E_INVALIDARG; glyphCount == 0 -> S_OK; NULL glyphAdvances / glyphOffsets / opportunities with
 *     glyphCount != 0 -> E_INVALIDARG. The outputs are NOT cleared on failure. The outputs may alias the inputs.
 *   - E = lineWidth - sum(advances). E == 0 (or smaller than sum/count * 2^-23): nothing changes.
 *   - E > 0 (expansion): per priority level p = 1, 2, ... (priority 0 is never used by the regular passes) the glyphs whose
 *     max adjustment (expansionMaximum once per enabled edge) is positive take a share: unbounded (>= FLT_MAX) ones split
 *     what the bounded ones do not use, bounded ones take at most their maximum; when the maxima cannot absorb E the shares
 *     are scaled down proportionally. A share below expansionMinimum is dropped. Whatever is left is split over the edges of
 *     the glyphs with allowResidualExpansion.
 *   - E < 0 (compression): same idea per compressionPriority with compressionMaximum (limited by the glyph's advance); then
 *     allowResidualCompression glyphs absorb the rest, never more than their advance.
 *   - The adjustment goes into the glyph advance; with applyToLeadingEdge the leading part (half when both edges apply) is also
 *     added to the glyph's advanceOffset, and following zero-advance glyphs are pulled back by the trailing part.
 *
 * GetJustifiedGlyphs (DWriteTextAnalyzer::GetJustifiedGlyphs):
 *   - *actualGlyphCount is cleared first. fontEmSize < 0, NULL clusterMap / modifiedClusterMap with textLength != 0, NULL
 *     modified arrays with maxGlyphCount != 0, or NULL glyph inputs with glyphCount != 0 -> E_INVALIDARG.
 *   - glyphCount == 0 -> S_OK. Otherwise the result copies cluster map / glyph indices / justified advances / justified offsets
 *     (inserting script extender glyphs, e.g. tatweel, where the justified advance exceeds the natural advance) and
 *     *actualGlyphCount is the number of resulting glyphs; if that exceeds maxGlyphCount the call fails with 0x8007007A and the
 *     required count is still reported.
 */
#include "ta.h"
#include "ta_shape.h"

#define FMAX FLT_MAX
static DWRITE_JUSTIFICATION_OPPORTUNITY mkop(float emin, float emax, float cmax, unsigned ep, unsigned cp, unsigned rexp, unsigned rcomp, unsigned lead, unsigned trail) {
    DWRITE_JUSTIFICATION_OPPORTUNITY o; memset(&o, 0, sizeof o);
    o.expansionMinimum = emin; o.expansionMaximum = emax; o.compressionMaximum = cmax; o.expansionPriority = ep; o.compressionPriority = cp;
    o.allowResidualExpansion = rexp; o.allowResidualCompression = rcomp; o.applyToLeadingEdge = lead; o.applyToTrailingEdge = trail;
    return o;
}
static DWRITE_JUSTIFICATION_OPPORTUNITY none_op(void) { return mkop(0, 0, 0, 0, 0, 0, 0, 0, 0); }

typedef struct jcase { UINT32 n; DWRITE_JUSTIFICATION_OPPORTUNITY op[16]; float adv[16]; DWRITE_GLYPH_OFFSET off[16]; } jcase;
static void jc_init(jcase *c, UINT32 n, const float *adv) {
    UINT32 i; memset(c, 0, sizeof *c); c->n = n;
    for (i = 0; i < n; i++) { c->adv[i] = adv[i]; c->op[i] = none_op(); }
}
static float sumf(const float *a, UINT32 n) { float s = 0; UINT32 i; for (i = 0; i < n; i++) s += a[i]; return s; }
static HRESULT jrun(IDWriteTextAnalyzer1 *a1, float lw, const jcase *c, float *oa, DWRITE_GLYPH_OFFSET *oo) {
    return IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, lw, c->n, c->op, c->adv, c->off, oa, oo);
}
#define SETUP_J(N, ...) jcase c; float oa[16]; DWRITE_GLYPH_OFFSET oo[16]; { static const float av_[] = { __VA_ARGS__ }; jc_init(&c, N, av_); }
#define CHECK_ADV(...) do { static const float w_[] = { __VA_ARGS__ }; unsigned i_; for (i_ = 0; i_ < sizeof w_ / sizeof *w_; i_++) \
    t_check(oa[i_] == w_[i_], __FILE__, __LINE__, "advance", "glyph %u: got %g want %g", i_, oa[i_], w_[i_]); } while (0)
#define CHECK_OFF(...) do { static const float w_[] = { __VA_ARGS__ }; unsigned i_; for (i_ = 0; i_ < sizeof w_ / sizeof *w_; i_++) \
    t_check(oo[i_].advanceOffset == w_[i_], __FILE__, __LINE__, "offset", "glyph %u: got %g want %g", i_, oo[i_].advanceOffset, w_[i_]); } while (0)

/* ---- argument validation -------------------------------------------------------------------------------------------------- */
TEST(ja_negative_width_invalid) {
    TA_BEGIN(); SETUP_J(2, 10, 10);
    CHECK_HR(jrun(a1, -1.0f, &c, oa, oo), TA_E_INVALIDARG);
    CHECK_HR(jrun(a1, -FLT_MIN, &c, oa, oo), TA_E_INVALIDARG);
    CHECK_HR(jrun(a1, -INFINITY, &c, oa, oo), TA_E_INVALIDARG);
    TA_END();
}
TEST(ja_nan_width_invalid) { TA_BEGIN(); SETUP_J(2, 10, 10); CHECK_HR(jrun(a1, NAN, &c, oa, oo), TA_E_INVALIDARG); TA_END(); }
TEST(ja_negative_zero_width_valid) {
    TA_BEGIN(); SETUP_J(2, 10, 10); CHECK_HR(jrun(a1, -0.0f, &c, oa, oo), S_OK); TA_END();
}
TEST(ja_zero_glyph_count_ok_with_null_arrays) {
    TA_BEGIN();
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 100.0f, 0, NULL, NULL, NULL, NULL, NULL), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 0.0f, 0, NULL, NULL, NULL, NULL, NULL), S_OK);
    TA_END();
}
TEST(ja_zero_glyph_count_negative_width_still_invalid) {
    TA_BEGIN(); CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, -1.0f, 0, NULL, NULL, NULL, NULL, NULL), TA_E_INVALIDARG); TA_END();
}
TEST(ja_null_inputs_invalid) {
    TA_BEGIN(); SETUP_J(2, 10, 10);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 30, 2, NULL, c.adv, c.off, oa, oo), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 30, 2, c.op, NULL, c.off, oa, oo), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 30, 2, c.op, c.adv, NULL, oa, oo), TA_E_INVALIDARG);
    TA_END();
}
TEST(ja_failure_leaves_outputs_untouched) {
    TA_BEGIN(); SETUP_J(2, 10, 10); oa[0] = 7; oa[1] = 8; oo[0].advanceOffset = 5; oo[0].ascenderOffset = 6; oo[1] = oo[0];
    CHECK_HR(jrun(a1, -1.0f, &c, oa, oo), TA_E_INVALIDARG);
    CHECK_F(oa[0], 7.0f); CHECK_F(oa[1], 8.0f); CHECK_F(oo[0].advanceOffset, 5.0f); CHECK_F(oo[1].ascenderOffset, 6.0f);
    TA_END();
}

/* ---- no-op cases ----------------------------------------------------------------------------------------------------------- */
TEST(ja_exact_width_changes_nothing) {
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); c.op[1] = mkop(0, FMAX, 5, 1, 1, 1, 1, 1, 1);
    c.off[2].advanceOffset = 1.5f; c.off[2].ascenderOffset = 2.5f;
    CHECK_HR(jrun(a1, 40.0f, &c, oa, oo), S_OK);
    CHECK_ADV(10, 10, 10, 10); CHECK_OFF(0, 0, 1.5f, 0); CHECK_F(oo[2].ascenderOffset, 2.5f);
    TA_END();
}
TEST(ja_no_opportunities_changes_nothing) {
    TA_BEGIN(); SETUP_J(3, 10, 11, 12);
    CHECK_HR(jrun(a1, 100.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 11, 12);
    CHECK_HR(jrun(a1, 5.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 11, 12);
    TA_END();
}
TEST(ja_priority_zero_is_not_a_regular_pass) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 100, 0, 0, 0, 0, 0, 1); c.op[1] = c.op[0];
    CHECK_HR(jrun(a1, 30.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10);
    CHECK_HR(jrun(a1, 10.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10);
    TA_END();
}
TEST(ja_edges_disabled_means_no_capacity) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 5, 1, 1, 1, 1, 0, 0); c.op[1] = c.op[0];
    CHECK_HR(jrun(a1, 40.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10);
    CHECK_HR(jrun(a1, 10.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10);
    TA_END();
}

/* ---- expansion ------------------------------------------------------------------------------------------------------------- */
TEST(ja_expand_unbounded_split_evenly) {
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); c.op[1] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1); c.op[3] = c.op[1];
    CHECK_HR(jrun(a1, 50.0f, &c, oa, oo), S_OK);
    CHECK_ADV(10, 15, 10, 15); CHECK_OFF(0, 0, 0, 0);
    TA_END();
}
TEST(ja_expand_conserves_width) {
    TA_BEGIN(); SETUP_J(4, 7, 3, 5, 1); int i; for (i = 0; i < 4; i++) c.op[i] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 32.0f, &c, oa, oo), S_OK);
    CHECK_F(sumf(oa, 4), 32.0f);
    for (i = 0; i < 4; i++) CHECK_F(oa[i] - c.adv[i], 4.0f);       /* equal shares of E = 16 */
    TA_END();
}
TEST(ja_expand_single_opportunity_takes_all) {
    TA_BEGIN(); SETUP_J(3, 10, 10, 10); c.op[2] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 45.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10, 25);
    TA_END();
}
TEST(ja_expand_bounded_fits) {
    /* maxima (2 each) absorb only 4 of the 10 available: the remainder stays unassigned */
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); c.op[1] = mkop(0, 2, 0, 1, 1, 0, 0, 0, 1); c.op[3] = c.op[1];
    CHECK_HR(jrun(a1, 50.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 12, 10, 12);
    TA_END();
}
TEST(ja_expand_bounded_overflow_scaled) {
    /* maxima 8 + 8 exceed E = 10: both shares are scaled by 10/16 */
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); c.op[1] = mkop(0, 8, 0, 1, 1, 0, 0, 0, 1); c.op[3] = c.op[1];
    CHECK_HR(jrun(a1, 50.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 15, 10, 15);
    TA_END();
}
TEST(ja_expand_mixed_bounded_unbounded) {
    /* glyph 0 is capped at 2, glyph 1 unbounded: E = 10 -> 2 and 8 */
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, 2, 0, 1, 1, 0, 0, 0, 1); c.op[1] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 30.0f, &c, oa, oo), S_OK); CHECK_ADV(12, 18);
    TA_END();
}
TEST(ja_expand_priorities_in_order) {
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); c.op[1] = mkop(0, 3, 0, 1, 1, 0, 0, 0, 1); c.op[3] = mkop(0, FMAX, 0, 2, 2, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 50.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 13, 10, 17);
    TA_END();
}
TEST(ja_expand_lower_priority_first_even_if_listed_later) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 2, 2, 0, 0, 0, 1); c.op[1] = mkop(0, 3, 0, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 30.0f, &c, oa, oo), S_OK); CHECK_ADV(17, 13);
    TA_END();
}
TEST(ja_expand_sparse_priorities) {
    /* priority 1 is empty, priority 5 is used */
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 5, 5, 0, 0, 0, 1); c.op[1] = none_op();
    CHECK_HR(jrun(a1, 26.0f, &c, oa, oo), S_OK); CHECK_ADV(16, 10);
    TA_END();
}
TEST(ja_expand_priority_255) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 255, 255, 0, 0, 0, 1); c.op[1] = none_op();
    CHECK_HR(jrun(a1, 26.0f, &c, oa, oo), S_OK); CHECK_ADV(16, 10);
    TA_END();
}
TEST(ja_expand_minimum_drops_small_shares) {
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); c.op[1] = mkop(6, FMAX, 0, 1, 1, 0, 0, 0, 1); c.op[3] = c.op[1];
    CHECK_HR(jrun(a1, 50.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10, 10, 10);          /* share 5 < minimum 6 */
    TA_END();
}
TEST(ja_expand_minimum_met_exactly) {
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); c.op[1] = mkop(5, FMAX, 0, 1, 1, 0, 0, 0, 1); c.op[3] = c.op[1];
    CHECK_HR(jrun(a1, 50.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 15, 10, 15);
    TA_END();
}
TEST(ja_expand_both_edges_double_capacity) {
    /* maximum is counted once per enabled edge: 2 * 4 = 8 for the glyph, E = 4 is half of it */
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, 4, 0, 1, 1, 0, 0, 1, 1);
    CHECK_HR(jrun(a1, 24.0f, &c, oa, oo), S_OK); CHECK_ADV(14, 10); CHECK_OFF(2, 0);        /* leading half (2) shifts the glyph */
    TA_END();
}
TEST(ja_expand_both_edges_capacity_exhausted) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, 3, 0, 1, 1, 0, 0, 1, 1);
    CHECK_HR(jrun(a1, 40.0f, &c, oa, oo), S_OK); CHECK_ADV(16, 10); CHECK_OFF(3, 0);
    TA_END();
}
TEST(ja_expand_leading_edge_only_offsets_glyph) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 1, 0);
    CHECK_HR(jrun(a1, 26.0f, &c, oa, oo), S_OK); CHECK_ADV(16, 10); CHECK_OFF(6, 0);
    TA_END();
}
TEST(ja_expand_trailing_edge_only_keeps_offset) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 26.0f, &c, oa, oo), S_OK); CHECK_ADV(16, 10); CHECK_OFF(0, 0);
    TA_END();
}
TEST(ja_expand_unbounded_both_edges) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 1, 1);
    CHECK_HR(jrun(a1, 30.0f, &c, oa, oo), S_OK); CHECK_ADV(20, 10); CHECK_OFF(5, 0);
    TA_END();
}
TEST(ja_expand_follower_zero_advance_glyphs_pulled_back) {
    /* glyph 1 is a zero-advance mark after glyph 0: its offset follows the trailing part of the expansion */
    TA_BEGIN(); SETUP_J(3, 10, 0, 10); c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 26.0f, &c, oa, oo), S_OK); CHECK_ADV(16, 0, 10); CHECK_OFF(0, -6, 0);
    TA_END();
}
TEST(ja_expand_follower_with_leading_only_edge_not_moved) {
    TA_BEGIN(); SETUP_J(3, 10, 0, 10); c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 1, 0);
    CHECK_HR(jrun(a1, 26.0f, &c, oa, oo), S_OK); CHECK_ADV(16, 0, 10); CHECK_OFF(6, 0, 0);
    TA_END();
}
TEST(ja_expand_follower_with_both_edges_moves_by_trailing_half) {
    TA_BEGIN(); SETUP_J(3, 10, 0, 10); c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 1, 1);
    CHECK_HR(jrun(a1, 30.0f, &c, oa, oo), S_OK); CHECK_ADV(20, 0, 10); CHECK_OFF(5, -5, 0);
    TA_END();
}
TEST(ja_expand_residual_split_over_edges) {
    TA_BEGIN(); SETUP_J(3, 10, 10, 10); c.op[0] = mkop(0, 0, 0, 0, 0, 1, 0, 0, 1); c.op[1] = c.op[0];
    CHECK_HR(jrun(a1, 36.0f, &c, oa, oo), S_OK); CHECK_ADV(13, 13, 10);
    TA_END();
}
TEST(ja_expand_residual_two_edges_count_double) {
    /* glyph 0 has two edges (weight 2), glyph 1 one edge: 6 / 3 per edge -> 4 and 2 on the glyphs... 2*2=4, 1*2=2 */
    TA_BEGIN(); SETUP_J(3, 10, 10, 10); c.op[0] = mkop(0, 0, 0, 0, 0, 1, 0, 1, 1); c.op[1] = mkop(0, 0, 0, 0, 0, 1, 0, 0, 1);
    CHECK_HR(jrun(a1, 36.0f, &c, oa, oo), S_OK); CHECK_ADV(14, 12, 10);
    TA_END();
}
TEST(ja_expand_residual_after_regular_pass) {
    /* regular pass uses 4 (2 + 2); the remaining 6 goes to the residual glyph */
    TA_BEGIN(); SETUP_J(3, 10, 10, 10); c.op[0] = mkop(0, 2, 0, 1, 1, 0, 0, 0, 1); c.op[1] = c.op[0]; c.op[2] = mkop(0, 0, 0, 0, 0, 1, 0, 0, 1);
    CHECK_HR(jrun(a1, 40.0f, &c, oa, oo), S_OK); CHECK_ADV(12, 12, 16);
    TA_END();
}
TEST(ja_expand_residual_flag_without_edge_is_ignored) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, 0, 0, 0, 0, 1, 0, 0, 0);
    CHECK_HR(jrun(a1, 30.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10);
    TA_END();
}
TEST(ja_expand_compress_flags_do_not_affect_expansion) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, 0, 0, 0, 0, 0, 1, 0, 1);    /* residual COMPRESSION only */
    CHECK_HR(jrun(a1, 30.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10);
    TA_END();
}
TEST(ja_expand_priority_uses_expansion_priority_field) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 1, 7, 0, 0, 0, 1); c.op[1] = mkop(0, 4, 0, 2, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 30.0f, &c, oa, oo), S_OK);
    CHECK_ADV(20, 10);                  /* priority 1 (glyph 0) takes everything; glyph 1 at priority 2 is never reached */
    TA_END();
}

/* ---- compression ------------------------------------------------------------------------------------------------------------ */
TEST(ja_compress_fits_scaled) {
    /* capacity 2 per glyph (8) covers |E| = 4: every share scaled by 4/8 */
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); int i; for (i = 0; i < 4; i++) c.op[i] = mkop(0, 0, 2, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 36.0f, &c, oa, oo), S_OK); CHECK_ADV(9, 9, 9, 9);
    TA_END();
}
TEST(ja_compress_capacity_too_small) {
    TA_BEGIN(); SETUP_J(4, 10, 10, 10, 10); int i; for (i = 0; i < 4; i++) c.op[i] = mkop(0, 0, 1, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 34.0f, &c, oa, oo), S_OK); CHECK_ADV(9, 9, 9, 9);
    TA_END();
}
TEST(ja_compress_limited_by_glyph_advance) {
    TA_BEGIN(); SETUP_J(2, 1, 1); c.op[0] = mkop(0, 0, 5, 1, 1, 0, 0, 0, 1); c.op[1] = c.op[0];
    CHECK_HR(jrun(a1, 1.0f, &c, oa, oo), S_OK); CHECK_ADV(0.5f, 0.5f);
    TA_END();
}
TEST(ja_compress_priorities_in_order) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, 0, 2, 1, 1, 0, 0, 0, 1); c.op[1] = mkop(0, 0, 8, 2, 2, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 14.0f, &c, oa, oo), S_OK); CHECK_ADV(8, 6);
    TA_END();
}
TEST(ja_compress_uses_compression_priority_field) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, 0, 2, 5, 2, 0, 0, 0, 1); c.op[1] = mkop(0, 0, 8, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 14.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 4);    /* compressionPriority 1 (glyph 1) first: capacity 8, scaled 6/8 -> 6 */
    TA_END();
}
TEST(ja_compress_zero_capacity_changes_nothing) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1); c.op[1] = c.op[0];
    CHECK_HR(jrun(a1, 10.0f, &c, oa, oo), S_OK); CHECK_ADV(10, 10);
    TA_END();
}
TEST(ja_compress_both_edges_double_capacity) {
    TA_BEGIN(); SETUP_J(1, 10); c.op[0] = mkop(0, 0, 2, 1, 1, 0, 0, 1, 1);
    CHECK_HR(jrun(a1, 8.0f, &c, oa, oo), S_OK); CHECK_ADV(8.0f);          /* capacity 4 >= |E| 2 */
    TA_END();
}
TEST(ja_compress_residual_limited_by_advance) {
    TA_BEGIN(); SETUP_J(3, 10, 10, 10); c.op[0] = mkop(0, 0, 0, 0, 0, 0, 1, 0, 1); c.op[1] = c.op[0];
    CHECK_HR(jrun(a1, 24.0f, &c, oa, oo), S_OK); CHECK_ADV(7, 7, 10);
    TA_END();
}
TEST(ja_compress_residual_never_exceeds_advance) {
    TA_BEGIN(); SETUP_J(2, 2, 10); c.op[0] = mkop(0, 0, 0, 0, 0, 0, 1, 0, 1); c.op[1] = c.op[0];
    CHECK_HR(jrun(a1, 4.0f, &c, oa, oo), S_OK);
    CHECK(oa[0] >= 0.0f); CHECK_F(oa[0], 0.0f);           /* needs -4 -> share 4 each, glyph 0 only has 2 */
    TA_END();
}
TEST(ja_compress_leading_edge_offsets) {
    TA_BEGIN(); SETUP_J(1, 10); c.op[0] = mkop(0, 0, 4, 1, 1, 0, 0, 1, 0);
    CHECK_HR(jrun(a1, 8.0f, &c, oa, oo), S_OK); CHECK_ADV(8.0f); CHECK_OFF(-2.0f);
    TA_END();
}

/* ---- offsets, aliasing, sweeps ------------------------------------------------------------------------------------------------ */
TEST(ja_input_offsets_are_carried_over) {
    TA_BEGIN(); SETUP_J(3, 10, 10, 10); c.off[0].advanceOffset = 1; c.off[0].ascenderOffset = 2; c.off[1].advanceOffset = 3; c.off[1].ascenderOffset = 4;
    c.op[2] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1);
    CHECK_HR(jrun(a1, 40.0f, &c, oa, oo), S_OK);
    CHECK_OFF(1, 3, 0); CHECK_F(oo[0].ascenderOffset, 2.0f); CHECK_F(oo[1].ascenderOffset, 4.0f);
    TA_END();
}
TEST(ja_inputs_not_modified) {
    TA_BEGIN(); SETUP_J(3, 10, 10, 10); jcase k; c.op[2] = mkop(0, FMAX, 0, 1, 1, 0, 0, 1, 1); k = c;
    CHECK_HR(jrun(a1, 50.0f, &c, oa, oo), S_OK); CHECK(!memcmp(&c, &k, sizeof c));
    TA_END();
}
TEST(ja_in_place_equals_out_of_place) {
    TA_BEGIN(); SETUP_J(4, 10, 10, 0, 10); jcase d; int i; c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 1, 1); c.op[3] = mkop(0, 3, 0, 1, 1, 0, 0, 0, 1); d = c;
    CHECK_HR(jrun(a1, 50.0f, &c, oa, oo), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 50.0f, d.n, d.op, d.adv, d.off, d.adv, d.off), S_OK);
    for (i = 0; i < 4; i++) { CHECK_F(d.adv[i], oa[i]); CHECK_F(d.off[i].advanceOffset, oo[i].advanceOffset); }
    TA_END();
}
TEST(ja_conservation_sweep_expansion) {
    /* with an unbounded opportunity somewhere, every width >= natural is reached exactly (dyadic arithmetic) */
    TA_BEGIN(); SETUP_J(6, 8, 4, 16, 2, 8, 6); int i, k; static const float lw[] = { 44, 48, 64, 100, 128, 1000 };
    for (i = 0; i < 6; i++) c.op[i] = i % 2 ? mkop(0, FMAX, 0, 1 + i % 3, 1, 0, 0, 0, 1) : none_op();
    for (k = 0; k < 6; k++) { CHECK_HR(jrun(a1, lw[k], &c, oa, oo), S_OK); CHECKF(fabsf(sumf(oa, 6) - lw[k]) < 0.01f, "width %g got %g", lw[k], sumf(oa, 6)); }
    TA_END();
}
TEST(ja_conservation_sweep_compression) {
    TA_BEGIN(); SETUP_J(6, 8, 4, 16, 2, 8, 6); int i, k; static const float lw[] = { 44, 40, 36, 30, 24, 22 };
    for (i = 0; i < 6; i++) c.op[i] = mkop(0, 0, 4, 1, 1, 0, 0, 0, 1);
    for (k = 0; k < 6; k++) { CHECK_HR(jrun(a1, lw[k], &c, oa, oo), S_OK); CHECKF(fabsf(sumf(oa, 6) - lw[k]) < 0.01f, "width %g got %g", lw[k], sumf(oa, 6)); }
    TA_END();
}
TEST(ja_advances_never_negative_sweep) {
    TA_BEGIN(); SETUP_J(5, 3, 3, 3, 3, 3); int i, k; static const float lw[] = { 0, 1, 5, 9, 12 };
    for (i = 0; i < 5; i++) c.op[i] = mkop(0, 0, FMAX, 1, 1, 0, 1, 1, 1);
    for (k = 0; k < 5; k++) { CHECK_HR(jrun(a1, lw[k], &c, oa, oo), S_OK); for (i = 0; i < 5; i++) CHECKF(oa[i] >= 0.0f, "width %g glyph %d adv %g", lw[k], i, oa[i]); }
    TA_END();
}
TEST(ja_large_run_1000_glyphs) {
    TA_BEGIN(); float *adv = (float *)malloc(1000 * sizeof(float)), *oa2 = (float *)malloc(1000 * sizeof(float)); DWRITE_JUSTIFICATION_OPPORTUNITY *op = (DWRITE_JUSTIFICATION_OPPORTUNITY *)malloc(1000 * sizeof *op);
    DWRITE_GLYPH_OFFSET *off = (DWRITE_GLYPH_OFFSET *)calloc(1000, sizeof *off), *oo2 = (DWRITE_GLYPH_OFFSET *)calloc(1000, sizeof *off); int i;
    for (i = 0; i < 1000; i++) { adv[i] = 8.0f; op[i] = i % 10 == 9 ? mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1) : none_op(); }
    CHECK_HR(IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, 8000.0f + 100.0f, 1000, op, adv, off, oa2, oo2), S_OK);
    CHECK_F(sumf(oa2, 1000), 8100.0f); CHECK_F(oa2[9], 8.0f + 1.0f); CHECK_F(oa2[8], 8.0f);
    free(adv); free(oa2); free(op); free(off); free(oo2); TA_END();
}
TEST(ja_via_analyzer2_interface) {
    TA_BEGIN(); SETUP_J(2, 10, 10); c.op[0] = mkop(0, FMAX, 0, 1, 1, 0, 0, 0, 1);
    CHECK_HR(IDWriteTextAnalyzer2_JustifyGlyphAdvances(a2, 26.0f, c.n, c.op, c.adv, c.off, oa, oo), S_OK); CHECK_ADV(16, 10);
    TA_END();
}

/* ---- GetJustifiedGlyphs ----------------------------------------------------------------------------------------------------- */
typedef struct gj {
    UINT16 cmap[16], idx[16]; float adv[16], jadv[16]; DWRITE_GLYPH_OFFSET joff[16]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[16];
    UINT32 tl, gc;
    UINT32 actual; UINT16 mcmap[64], midx[64]; float madv[64]; DWRITE_GLYPH_OFFSET moff[64];
} gj;
static void gj_init(gj *g, UINT32 n) {
    UINT32 i; memset(g, 0, sizeof *g); g->tl = g->gc = n;
    for (i = 0; i < n; i++) { g->cmap[i] = (UINT16)i; g->idx[i] = (UINT16)(i + 2); g->adv[i] = 10; g->jadv[i] = 10; g->gp[i] = ta_gp(0, 1, 0, 0); }
    for (i = 0; i < 64; i++) { g->mcmap[i] = 0xAAAA; g->midx[i] = 0xBBBB; g->madv[i] = -1; g->moff[i].advanceOffset = -2; g->moff[i].ascenderOffset = -3; }
}
static HRESULT gj_call(IDWriteTextAnalyzer1 *a1, IDWriteFontFace *face, float em, DWRITE_SCRIPT_ANALYSIS sa, gj *g, UINT32 maxg) {
    return IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, face, em, sa, g->tl, g->gc, maxg, g->cmap, g->idx, g->adv, g->jadv, g->joff, g->gp, &g->actual, g->mcmap, g->midx, g->madv, g->moff);
}
static DWRITE_SCRIPT_ANALYSIS SA0(void) { DWRITE_SCRIPT_ANALYSIS s; s.script = 0; s.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT; return s; }

TEST(gj_negative_em_invalid_actual_cleared) {
    TA_BEGIN(); gj g; gj_init(&g, 3); g.actual = 99;
    CHECK_HR(gj_call(a1, NULL, -1.0f, SA0(), &g, 16), TA_E_INVALIDARG);
    EXPECT_EQ(g.actual, 0);
    TA_END();
}
TEST(gj_zero_glyph_count_ok_all_null) {
    TA_BEGIN(); UINT32 actual = 99;
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, &actual, NULL, NULL, NULL, NULL), S_OK);
    EXPECT_EQ(actual, 0);
    TA_END();
}
TEST(gj_zero_glyph_count_negative_em_invalid) {
    TA_BEGIN(); UINT32 actual = 99;
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, -1.0f, SA0(), 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, &actual, NULL, NULL, NULL, NULL), TA_E_INVALIDARG);
    EXPECT_EQ(actual, 0);
    TA_END();
}
TEST(gj_null_cluster_maps_invalid) {
    TA_BEGIN(); gj g; gj_init(&g, 3);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, NULL, g.idx, g.adv, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, g.midx, g.madv, g.moff), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, g.adv, g.jadv, g.joff, g.gp, &g.actual, NULL, g.midx, g.madv, g.moff), TA_E_INVALIDARG);
    TA_END();
}
TEST(gj_null_modified_arrays_invalid) {
    TA_BEGIN(); gj g; gj_init(&g, 3);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, g.adv, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, NULL, g.madv, g.moff), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, g.adv, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, g.midx, NULL, g.moff), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, g.adv, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, g.midx, g.madv, NULL), TA_E_INVALIDARG);
    TA_END();
}
TEST(gj_null_glyph_inputs_invalid) {
    TA_BEGIN(); gj g; gj_init(&g, 3);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, NULL, g.adv, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, g.midx, g.madv, g.moff), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, NULL, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, g.midx, g.madv, g.moff), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, g.adv, NULL, g.joff, g.gp, &g.actual, g.mcmap, g.midx, g.madv, g.moff), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, g.adv, g.jadv, NULL, g.gp, &g.actual, g.mcmap, g.midx, g.madv, g.moff), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, g.adv, g.jadv, g.joff, NULL, &g.actual, g.mcmap, g.midx, g.madv, g.moff), TA_E_INVALIDARG);
    TA_END();
}
TEST(gj_actual_cleared_on_every_failure) {
    TA_BEGIN(); gj g; gj_init(&g, 3); g.actual = 77;
    IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, NULL, g.adv, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, g.midx, g.madv, g.moff);
    EXPECT_EQ(g.actual, 0);
    TA_END();
}
TEST(gj_copy_without_extension) {
    /* a face-less call (or any script without extender glyph) reproduces the justified arrays one to one */
    TA_BEGIN(); gj g; UINT32 i; gj_init(&g, 4);
    for (i = 0; i < 4; i++) { g.jadv[i] = 10.0f + (float)i; g.joff[i].advanceOffset = (float)i; g.joff[i].ascenderOffset = -(float)i; }
    CHECK_HR(gj_call(a1, NULL, 16.0f, SA0(), &g, 16), S_OK);
    EXPECT_EQ(g.actual, 4);
    for (i = 0; i < 4; i++) {
        EXPECT_EQ(g.mcmap[i], g.cmap[i]); EXPECT_EQ(g.midx[i], g.idx[i]); CHECK_F(g.madv[i], g.jadv[i]);
        CHECK_F(g.moff[i].advanceOffset, g.joff[i].advanceOffset); CHECK_F(g.moff[i].ascenderOffset, g.joff[i].ascenderOffset);
    }
    EXPECT_EQ(g.midx[4], 0xBBBB); EXPECT_EQ(g.mcmap[4], 0xAAAA);            /* nothing written past the copy */
    TA_END();
}
TEST(gj_copy_uses_justified_not_natural_advances) {
    TA_BEGIN(); gj g; gj_init(&g, 2); g.adv[0] = 10; g.jadv[0] = 14; g.adv[1] = 10; g.jadv[1] = 6;
    CHECK_HR(gj_call(a1, NULL, 16.0f, SA0(), &g, 16), S_OK);
    CHECK_F(g.madv[0], 14.0f); CHECK_F(g.madv[1], 6.0f);
    TA_END();
}
TEST(gj_cluster_map_has_text_length_entries) {
    /* 5 characters, 3 glyphs (ligature-style map) */
    TA_BEGIN(); gj g; gj_init(&g, 3); g.tl = 5; g.cmap[0] = 0; g.cmap[1] = 0; g.cmap[2] = 1; g.cmap[3] = 1; g.cmap[4] = 2;
    CHECK_HR(gj_call(a1, NULL, 16.0f, SA0(), &g, 16), S_OK);
    EXPECT_EQ(g.actual, 3);
    EXPECT_EQ(g.mcmap[0], 0); EXPECT_EQ(g.mcmap[1], 0); EXPECT_EQ(g.mcmap[2], 1); EXPECT_EQ(g.mcmap[3], 1); EXPECT_EQ(g.mcmap[4], 2); EXPECT_EQ(g.mcmap[5], 0xAAAA);
    TA_END();
}
TEST(gj_exact_capacity_succeeds) {
    TA_BEGIN(); gj g; gj_init(&g, 5);
    CHECK_HR(gj_call(a1, NULL, 16.0f, SA0(), &g, 5), S_OK); EXPECT_EQ(g.actual, 5); EXPECT_EQ(g.midx[4], g.idx[4]);
    TA_END();
}
TEST(gj_capacity_one_short_reports_required_count) {
    TA_BEGIN(); gj g; gj_init(&g, 5);
    CHECK_HR(gj_call(a1, NULL, 16.0f, SA0(), &g, 4), TA_E_BUFFER);
    EXPECT_EQ(g.actual, 5);
    EXPECT_EQ(g.midx[0], 0xBBBB);                  /* nothing copied */
    TA_END();
}
TEST(gj_capacity_zero_reports_required_count) {
    TA_BEGIN(); gj g; gj_init(&g, 5);
    CHECK_HR(IDWriteTextAnalyzer1_GetJustifiedGlyphs(a1, NULL, 16.0f, SA0(), g.tl, g.gc, 0, g.cmap, g.idx, g.adv, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, NULL, NULL, NULL), TA_E_BUFFER);
    EXPECT_EQ(g.actual, 5);
    TA_END();
}
TEST(gj_negative_zero_em_valid) {
    TA_BEGIN(); gj g; gj_init(&g, 2);
    CHECK_HR(gj_call(a1, NULL, 0.0f, SA0(), &g, 16), S_OK); EXPECT_EQ(g.actual, 2);
    CHECK_HR(gj_call(a1, NULL, -0.0f, SA0(), &g, 16), S_OK);
    TA_END();
}
TEST(gj_latin_with_synthetic_face_copies) {
    TA_BEGIN(); dwx_font hf; if (FAILED(dwx_font_open_default(&hf, f)) || !hf.face) { dwx_font_close(&hf); TA_END(); t_skip("synthetic font unavailable"); return; }
    gj g; gj_init(&g, 4); g.jadv[1] = 25;
    DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"A", 1);
    CHECK_HR(gj_call(a1, hf.face, 16.0f, sa, &g, 16), S_OK);
    EXPECT_EQ(g.actual, 4); CHECK_F(g.madv[1], 25.0f);
    dwx_font_close(&hf); TA_END();
}

/* Extender (tatweel) insertion: a synthetic font maps U+0640 to a glyph with a visible outline (advance 1000 of 2048). */
static const ttf_map k_ext_map[] = { { ' ', 1 }, { 0x0640, 2 }, { 'A', 2 } };
static HRESULT open_ext_font(dwx_font *h, IDWriteFactory *f) {
    static ttf_cmap cm[1]; ttf_spec s; ttf_default_spec(&s);
    cm[0].format = 4; cm[0].plat = 3; cm[0].enc = 1; cm[0].n = 3; cm[0].map = k_ext_map; cm[0].nuvs = 0; cm[0].uvs = NULL;
    s.cmaps = cm; s.ncmaps = 1;
    return ta_open_spec(h, f, &s);
}
TEST(gj_arabic_extender_class_scan) {
    TA_BEGIN(); dwx_font hf; if (FAILED(open_ext_font(&hf, f)) || !hf.face) { dwx_font_close(&hf); TA_END(); t_skip("synthetic font unavailable"); return; }
    const float em = 16.0f, ext_w = 1000.0f * em / 2048.0f;           /* 7.8125 */
    DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"\x0628", 1); unsigned cls, extended = 0;
    if (sa.script == 0xFFFF) { dwx_font_close(&hf); TA_END(); t_skip("no script"); return; }
    for (cls = 0; cls < 16; cls++) {
        gj g; HRESULT hr; gj_init(&g, 2); g.gp[0] = ta_gp(cls, 1, 0, 0); g.gp[1] = ta_gp(cls, 1, 0, 0);
        g.adv[0] = 10; g.adv[1] = 10; g.jadv[0] = 10.0f + 3.0f * ext_w; g.jadv[1] = 10;
        g.idx[0] = 3; g.idx[1] = 4;
        hr = gj_call(a1, hf.face, em, sa, &g, 16);
        if (hr != S_OK) { CHECKF(0, "class %u: %s", cls, dw_hr_name(hr)); continue; }
        if (g.actual == 2) { EXPECT_EQ(g.midx[0], 3); CHECK_F(g.madv[0], g.jadv[0]); continue; }
        extended++;
        t_note("justification class %u: %u glyphs inserted", cls, g.actual - 2);
        /* extender glyphs (the tatweel glyph, zero advance) are inserted in front of the stretched glyph */
        CHECK(g.actual > 2);
        UINT32 i, ins = 0; float sum = 0, jsum = g.jadv[0] + g.jadv[1];
        for (i = 0; i < g.actual; i++) { sum += g.madv[i]; if (g.midx[i] == 2) { ins++; CHECK_F(g.madv[i], 0.0f); } }
        EXPECT_EQ(ins, g.actual - 2); EXPECT_EQ(g.midx[0], 3);       /* the stretched glyph comes first, its extenders follow */
        CHECK_F(sum, jsum);
        EXPECT_EQ(g.midx[g.actual - 1], 4);
        EXPECT_EQ(g.mcmap[0], 0);                    /* the cluster of the first character starts at the first inserted glyph */
        EXPECT_EQ(g.mcmap[1], g.actual - 1);
    }
    t_note("%u of 16 justification classes receive extender glyphs", extended);
    dwx_font_close(&hf); TA_END();
}
TEST(gj_arabic_extender_overflow_reports_count) {
    TA_BEGIN(); dwx_font hf; if (FAILED(open_ext_font(&hf, f)) || !hf.face) { dwx_font_close(&hf); TA_END(); t_skip("synthetic font unavailable"); return; }
    const float em = 16.0f, ext_w = 1000.0f * em / 2048.0f; DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"\x0628", 1); unsigned cls, found = 0;
    for (cls = 0; cls < 16 && !found; cls++) {
        gj g; gj_init(&g, 1); g.tl = 1; g.gp[0] = ta_gp(cls, 1, 0, 0); g.jadv[0] = 10.0f + 3.0f * ext_w; g.idx[0] = 3;
        if (gj_call(a1, hf.face, em, sa, &g, 16) != S_OK || g.actual == 1) continue;
        found = 1;
        UINT32 need = g.actual; gj h; gj_init(&h, 1); h.gp[0] = g.gp[0]; h.jadv[0] = g.jadv[0]; h.idx[0] = 3;
        CHECK_HR(gj_call(a1, hf.face, em, sa, &h, need - 1), TA_E_BUFFER);
        EXPECT_EQ(h.actual, need);
        CHECK_HR(gj_call(a1, hf.face, em, sa, &h, need), S_OK);
        CHECK_HR(gj_call(a1, hf.face, em, sa, &h, 0), TA_E_BUFFER);
    }
    if (!found) t_skip("no justification class produced extender glyphs");
    dwx_font_close(&hf); TA_END();
}
TEST(gj_extender_count_is_ceiling_of_extra_over_glyph_width) {
    TA_BEGIN(); dwx_font hf; if (FAILED(open_ext_font(&hf, f)) || !hf.face) { dwx_font_close(&hf); TA_END(); t_skip("synthetic font unavailable"); return; }
    const float em = 16.0f, ext_w = 1000.0f * em / 2048.0f; DWRITE_SCRIPT_ANALYSIS sa = ta_sa_of(&c_, L"\x0628", 1); unsigned cls, found = 0;
    static const float extra_mult[] = { 0.25f, 1.0f, 1.5f, 3.0f, 3.25f, 6.0f };
    for (cls = 0; cls < 16 && !found; cls++) {
        gj g; gj_init(&g, 1); g.gp[0] = ta_gp(cls, 1, 0, 0); g.jadv[0] = 10.0f + 3.0f * ext_w; g.idx[0] = 3;
        if (gj_call(a1, hf.face, em, sa, &g, 16) != S_OK || g.actual == 1) continue;
        found = 1;
        for (unsigned k = 0; k < sizeof extra_mult / sizeof *extra_mult; k++) {
            gj h; gj_init(&h, 1); h.gp[0] = g.gp[0]; h.idx[0] = 3; h.jadv[0] = 10.0f + extra_mult[k] * ext_w;
            CHECK_HR(gj_call(a1, hf.face, em, sa, &h, 16), S_OK);
            EXPECT_EQ(h.actual, 1 + (UINT32)ceilf(extra_mult[k]));
        }
        /* no extra width: no insertion; shrinking: no insertion */
        gj z; gj_init(&z, 1); z.gp[0] = g.gp[0]; z.jadv[0] = 10.0f; CHECK_HR(gj_call(a1, hf.face, em, sa, &z, 16), S_OK); EXPECT_EQ(z.actual, 1);
        z.jadv[0] = 4.0f; CHECK_HR(gj_call(a1, hf.face, em, sa, &z, 16), S_OK); EXPECT_EQ(z.actual, 1);
    }
    if (!found) t_skip("no justification class produced extender glyphs");
    dwx_font_close(&hf); TA_END();
}
TEST(gj_latin_script_never_inserts_with_class_none) {
    TA_BEGIN(); dwx_font hf; if (FAILED(open_ext_font(&hf, f)) || !hf.face) { dwx_font_close(&hf); TA_END(); t_skip("synthetic font unavailable"); return; }
    gj g; gj_init(&g, 3); g.jadv[1] = 200;
    CHECK_HR(gj_call(a1, hf.face, 16.0f, ta_sa_of(&c_, L"A", 1), &g, 16), S_OK);
    EXPECT_EQ(g.actual, 3);                   /* class NONE never receives extender glyphs */
    dwx_font_close(&hf); TA_END();
}
TEST(gj_via_analyzer2_interface) {
    TA_BEGIN(); gj g; gj_init(&g, 2);
    CHECK_HR(IDWriteTextAnalyzer2_GetJustifiedGlyphs(a2, NULL, 16.0f, SA0(), g.tl, g.gc, 16, g.cmap, g.idx, g.adv, g.jadv, g.joff, g.gp, &g.actual, g.mcmap, g.midx, g.madv, g.moff), S_OK);
    EXPECT_EQ(g.actual, 2);
    TA_END();
}

/* ---- model driven sweep ---------------------------------------------------------------------------------------------------------- */
static UINT32 lcg(UINT32 *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }
static int justify_matches_model(IDWriteTextAnalyzer1 *a1, const jcase *c, float lw, unsigned seed) {
    float oa[16], ma[16]; DWRITE_GLYPH_OFFSET oo[16], mo[16]; UINT32 i; int ok = 1;
    if (IDWriteTextAnalyzer1_JustifyGlyphAdvances(a1, lw, c->n, c->op, c->adv, c->off, oa, oo) != S_OK) return 0;
    ta_model_justify(lw, c->n, c->op, c->adv, c->off, ma, mo);
    for (i = 0; i < c->n; i++)
        if (oa[i] != ma[i] || oo[i].advanceOffset != mo[i].advanceOffset || oo[i].ascenderOffset != mo[i].ascenderOffset) {
            t_check(0, __FILE__, __LINE__, "model match", "seed %u lw %g glyph %u: adv %g/%g off %g/%g", seed, lw, i, oa[i], ma[i], oo[i].advanceOffset, mo[i].advanceOffset);
            ok = 0;
        }
    return ok;
}
TEST(ja_sweep_random_opportunities_match_model) {
    /* dyadic advances / limits keep every intermediate exact, so the comparison is bit exact */
    TA_BEGIN(); static const float lims[] = { 0.0f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, FMAX };
    UINT32 seed = 12345, trial; int bad = 0;
    for (trial = 0; trial < 300; trial++) {
        jcase c; UINT32 i, n = 2 + lcg(&seed) % 9; float adv[16], lw, sum = 0; memset(&c, 0, sizeof c);
        for (i = 0; i < n; i++) { adv[i] = (float)(lcg(&seed) % 5) * 2.0f + (i % 4 == 3 ? 0.0f : 2.0f); sum += adv[i]; }
        jc_init(&c, n, adv);
        for (i = 0; i < n; i++) {
            UINT32 r = lcg(&seed);
            if (r % 3 == 0) continue;
            c.op[i] = mkop(lims[lcg(&seed) % 3] * (lcg(&seed) % 2), lims[lcg(&seed) % 7], lims[lcg(&seed) % 7], lcg(&seed) % 4, lcg(&seed) % 4, lcg(&seed) % 2, lcg(&seed) % 2, lcg(&seed) % 2, lcg(&seed) % 2);
            c.off[i].advanceOffset = (float)(lcg(&seed) % 3); c.off[i].ascenderOffset = 1.0f;
        }
        lw = sum + (float)((int)(lcg(&seed) % 41) - 20) * 0.5f; if (lw < 0) lw = 0;
        if (!justify_matches_model(a1, &c, lw, trial)) bad++;
    }
    CHECK(bad == 0);
    TA_END();
}
