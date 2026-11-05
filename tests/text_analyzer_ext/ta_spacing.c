/* IDWriteTextAnalyzer1::ApplyCharacterSpacing.
 *
 * Windows 10 behaviour (DWriteTextAnalyzer::ApplyCharacterSpacing / TextAnalyzer::ApplyCharacterSpacing):
 *   - modifiedGlyphAdvances / modifiedGlyphOffsets are zeroed first unless they alias the corresponding input pointer
 *     (so a failing call leaves distinct outputs zero and aliased outputs untouched);
 *   - E_INVALIDARG when leading or trailing is NaN, minimumAdvanceWidth is NaN or < 0, or - for glyphCount != 0 - when
 *     textLength == 0 or any of clusterMap / glyphAdvances / glyphOffsets / glyphProperties is NULL;
 *   - glyphCount == 0 succeeds (after the float checks) without touching anything but the zeroed outputs;
 *   - per cluster (a cluster is the glyph range [clusterMap[t], next distinct clusterMap value)): zero-width-space glyphs
 *     (glyph property bit 6) are skipped when choosing the first/last glyph; the leading amount is added to the offset of
 *     every glyph up to and including the first real glyph and to that glyph's advance; the trailing amount is added to the
 *     advance of the last real glyph and subtracted from the offsets of the zero-width glyphs following it;
 *   - negative spacing is limited so that the cluster width never drops below minimumAdvanceWidth; a cluster narrower than
 *     minimumAdvanceWidth is padded half/half on top of the positive spacing.
 * ta_model_spacing() restates that arithmetic; hand-computed cases pin it down independently.
 */
#include "ta.h"

void ta_model_spacing(float L, float T, float M, UINT32 n, UINT32 g, const UINT16 *cm, const float *adv, const DWRITE_GLYPH_OFFSET *off,
                      const DWRITE_SHAPING_GLYPH_PROPERTIES *gp, float *oadv, DWRITE_GLYPH_OFFSET *ooff) {
    UINT32 i, t = 0, start = 0, end;
    int clamp = (L < 0.0f) || (T < 0.0f);
    if (g == 0) return;
    for (i = 0; i < g; i++) { oadv[i] = adv[i]; ooff[i] = off[i]; }
    do {
        float sum = 0.0f, lead = L, trail = T, width, d, w2;
        UINT32 first;
        end = g;
        for (; t < n; t++) { UINT32 v = (UINT32)cm[t] - (UINT32)cm[0]; if (start < v) { end = v; break; } }
        first = end;
        for (i = start; i < end; i++) { sum += oadv[i]; if (i < first && !gp[i].isZeroWidthSpace) first = i; }
        width = sum;
        if (first < end) {
            if (clamp) {
                d = M - sum; w2 = sum;
                if (L < 0.0f) {
                    if (d < 0.0f) { lead = d; w2 = M; if (d <= L) { lead = L; w2 = sum + L; } }
                    else lead = 0.0f;
                }
                width = w2;
                if (T < 0.0f) {
                    if (d < 0.0f) {
                        if (d <= T) width = w2 + T;
                        else { trail = d; width = M; if (lead < 0.0f) width = w2 + d; }
                    } else trail = 0.0f;
                }
            }
            if (width < M) { float extra = M - width, half = extra * 0.5f; lead += half; trail += extra - half; }
            for (i = start; i <= first; i++) ooff[i].advanceOffset += lead;
            oadv[first] += lead;
            i = end;
            do { i--; if (!gp[i].isZeroWidthSpace) { oadv[i] += trail; break; } ooff[i].advanceOffset -= trail; } while (first < i);
        }
        start = end;
    } while (end < g);
}

/* ---- fixture ------------------------------------------------------------------------------------------------------ */
#define MAXG 80
typedef struct sp_case {
    UINT32 n, g; UINT16 cm[MAXG]; float adv[MAXG]; DWRITE_GLYPH_OFFSET off[MAXG]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[MAXG];
} sp_case;

/* One glyph per character, advances given. */
static void sp_simple(sp_case *c, UINT32 g, const float *adv) {
    UINT32 i; memset(c, 0, sizeof *c);
    c->n = c->g = g;
    for (i = 0; i < g; i++) { c->cm[i] = (UINT16)i; c->adv[i] = adv[i]; c->gp[i] = ta_gp(0, 1, 0, 0); }
}
/* Clusters described by the glyph count of each cluster (one text char per cluster). */
static void sp_clusters(sp_case *c, UINT32 nclusters, const UINT32 *glyphs_per, const float *adv) {
    UINT32 i, k, gi = 0; memset(c, 0, sizeof *c);
    c->n = nclusters;
    for (i = 0; i < nclusters; i++) {
        c->cm[i] = (UINT16)gi;
        for (k = 0; k < glyphs_per[i]; k++, gi++) { c->adv[gi] = adv[gi]; c->gp[gi] = ta_gp(0, k == 0, 0, 0); }
    }
    c->g = gi;
}
static HRESULT sp_run(IDWriteTextAnalyzer1 *a, float L, float T, float M, const sp_case *c, float *oadv, DWRITE_GLYPH_OFFSET *ooff) {
    return IDWriteTextAnalyzer1_ApplyCharacterSpacing(a, L, T, M, c->n, c->g, c->cm, c->adv, c->off, c->gp, oadv, ooff);
}
static int sp_matches_model(IDWriteTextAnalyzer1 *a, float L, float T, float M, const sp_case *c) {
    float oadv[MAXG], madv[MAXG]; DWRITE_GLYPH_OFFSET ooff[MAXG], moff[MAXG]; UINT32 i; int ok = 1;
    if (sp_run(a, L, T, M, c, oadv, ooff) != S_OK) return 0;
    ta_model_spacing(L, T, M, c->n, c->g, c->cm, c->adv, c->off, c->gp, madv, moff);
    for (i = 0; i < c->g; i++)
        if (oadv[i] != madv[i] || ooff[i].advanceOffset != moff[i].advanceOffset || ooff[i].ascenderOffset != moff[i].ascenderOffset) {
            t_check(0, __FILE__, __LINE__, "model match", "L=%g T=%g M=%g glyph %u: adv %g/%g off %g/%g", L, T, M, i, oadv[i], madv[i],
                    ooff[i].advanceOffset, moff[i].advanceOffset);
            ok = 0;
        }
    return ok;
}

/* ---- argument validation --------------------------------------------------------------------------------------------- */
TEST(sp_nan_leading_invalid) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, NAN, 0, 0, &c, oa, oo), TA_E_INVALIDARG);
    TA_END();
}
TEST(sp_nan_trailing_invalid) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 0, NAN, 0, &c, oa, oo), TA_E_INVALIDARG);
    TA_END();
}
TEST(sp_nan_minadv_invalid) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 0, 0, NAN, &c, oa, oo), TA_E_INVALIDARG);
    TA_END();
}
TEST(sp_negative_minadv_invalid) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    static const float bad[] = { -1.0f, -0.001f, -1e-30f, -FLT_MAX, -INFINITY };
    for (int i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) CHECK_HR(sp_run(a1, 0, 0, bad[i], &c, oa, oo), TA_E_INVALIDARG);
    TA_END();
}
TEST(sp_negative_zero_minadv_valid) {
    /* the check is 0.0 <= minimumAdvanceWidth, which holds for -0.0f */
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 0, 0, -0.0f, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 10.0f);
    TA_END();
}
TEST(sp_infinite_spacing_valid_args) {
    /* infinity is not NaN: argument validation passes (only NaN and negative minimum are rejected) */
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, INFINITY, 0, 0, &c, oa, oo), S_OK);
    CHECK_HR(sp_run(a1, 0, INFINITY, 0, &c, oa, oo), S_OK);
    TA_END();
}
TEST(sp_zero_glyphcount_succeeds_with_null_arrays) {
    TA_BEGIN();
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1, 1, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1, 1, 0, 5, 0, NULL, NULL, NULL, NULL, NULL, NULL), S_OK);
    TA_END();
}
TEST(sp_zero_glyphcount_float_checks_first) {
    /* the float checks precede the glyphCount == 0 early exit */
    TA_BEGIN();
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, NAN, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 0, NAN, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 0, 0, -1, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL), TA_E_INVALIDARG);
    TA_END();
}
TEST(sp_zero_textlength_invalid) {
    TA_BEGIN(); sp_case c; float adv[2] = { 10, 10 }; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; sp_simple(&c, 2, adv);
    c.n = 0;
    CHECK_HR(sp_run(a1, 1, 1, 0, &c, oa, oo), TA_E_INVALIDARG);
    TA_END();
}
TEST(sp_null_inputs_invalid) {
    TA_BEGIN(); sp_case c; float adv[2] = { 10, 10 }; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; sp_simple(&c, 2, adv);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1, 1, 0, c.n, c.g, NULL, c.adv, c.off, c.gp, oa, oo), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1, 1, 0, c.n, c.g, c.cm, NULL, c.off, c.gp, oa, oo), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1, 1, 0, c.n, c.g, c.cm, c.adv, NULL, c.gp, oa, oo), TA_E_INVALIDARG);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1, 1, 0, c.n, c.g, c.cm, c.adv, c.off, NULL, oa, oo), TA_E_INVALIDARG);
    TA_END();
}
TEST(sp_failure_zeroes_distinct_outputs) {
    /* outputs that do not alias the inputs are cleared before validation */
    TA_BEGIN(); sp_case c; float adv[3] = { 10, 11, 12 }; float oa[3] = { 7, 7, 7 }; DWRITE_GLYPH_OFFSET oo[3]; UINT32 i; sp_simple(&c, 3, adv);
    for (i = 0; i < 3; i++) { oo[i].advanceOffset = 5; oo[i].ascenderOffset = 6; }
    CHECK_HR(sp_run(a1, NAN, 0, 0, &c, oa, oo), TA_E_INVALIDARG);
    for (i = 0; i < 3; i++) { CHECK_F(oa[i], 0.0f); CHECK_F(oo[i].advanceOffset, 0.0f); CHECK_F(oo[i].ascenderOffset, 0.0f); }
    for (i = 0; i < 3; i++) { oa[i] = 7; oo[i].advanceOffset = 5; oo[i].ascenderOffset = 6; }
    CHECK_HR(sp_run(a1, 0, 0, -1, &c, oa, oo), TA_E_INVALIDARG);
    for (i = 0; i < 3; i++) { CHECK_F(oa[i], 0.0f); CHECK_F(oo[i].advanceOffset, 0.0f); }
    TA_END();
}
TEST(sp_failure_zeroes_even_for_zero_textlength) {
    TA_BEGIN(); sp_case c; float adv[2] = { 10, 11 }; float oa[2] = { 7, 7 }; DWRITE_GLYPH_OFFSET oo[2] = { { 5, 6 }, { 5, 6 } }; sp_simple(&c, 2, adv);
    c.n = 0;
    CHECK_HR(sp_run(a1, 1, 1, 0, &c, oa, oo), TA_E_INVALIDARG);
    CHECK_F(oa[0], 0.0f); CHECK_F(oa[1], 0.0f); CHECK_F(oo[0].advanceOffset, 0.0f); CHECK_F(oo[1].ascenderOffset, 0.0f);
    TA_END();
}
TEST(sp_failure_keeps_aliased_outputs) {
    TA_BEGIN(); sp_case c; float adv[3] = { 10, 11, 12 }; UINT32 i; sp_simple(&c, 3, adv);
    for (i = 0; i < 3; i++) { c.off[i].advanceOffset = 5; c.off[i].ascenderOffset = 6; }
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, NAN, 0, 0, c.n, c.g, c.cm, c.adv, c.off, c.gp, c.adv, c.off), TA_E_INVALIDARG);
    for (i = 0; i < 3; i++) { CHECK_F(c.adv[i], adv[i]); CHECK_F(c.off[i].advanceOffset, 5.0f); CHECK_F(c.off[i].ascenderOffset, 6.0f); }
    TA_END();
}
TEST(sp_failure_alias_only_advances) {
    /* each output is aliased (or not) independently of the other */
    TA_BEGIN(); sp_case c; float adv[2] = { 10, 11 }; DWRITE_GLYPH_OFFSET oo[2] = { { 9, 9 }, { 9, 9 } }; sp_simple(&c, 2, adv);
    c.off[0].advanceOffset = 3; c.off[1].advanceOffset = 4;
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, NAN, 0, 0, c.n, c.g, c.cm, c.adv, c.off, c.gp, c.adv, oo), TA_E_INVALIDARG);
    CHECK_F(c.adv[0], 10.0f); CHECK_F(c.adv[1], 11.0f);
    CHECK_F(oo[0].advanceOffset, 0.0f); CHECK_F(oo[1].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_ascender_offset_untouched) {
    TA_BEGIN(); sp_case c; float adv[3] = { 10, 11, 12 }; float oa[3]; DWRITE_GLYPH_OFFSET oo[3]; UINT32 i; sp_simple(&c, 3, adv);
    for (i = 0; i < 3; i++) { c.off[i].advanceOffset = (float)i; c.off[i].ascenderOffset = 100.0f + i; }
    CHECK_HR(sp_run(a1, 2, 3, 0, &c, oa, oo), S_OK);
    for (i = 0; i < 3; i++) CHECK_F(oo[i].ascenderOffset, 100.0f + i);
    for (i = 0; i < 3; i++) { CHECK_F(oo[i].advanceOffset, (float)i + 2.0f); CHECK_F(oa[i], adv[i] + 5.0f); }
    TA_END();
}
TEST(sp_inputs_not_modified) {
    TA_BEGIN(); sp_case c, k; float adv[3] = { 10, 11, 12 }; float oa[3]; DWRITE_GLYPH_OFFSET oo[3]; sp_simple(&c, 3, adv); k = c;
    CHECK_HR(sp_run(a1, 2, 3, 20, &c, oa, oo), S_OK);
    CHECK(!memcmp(&c, &k, sizeof c));
    TA_END();
}

/* ---- hand computed semantics -------------------------------------------------------------------------------------------- */
TEST(sp_zero_spacing_is_identity) {
    TA_BEGIN(); sp_case c; float adv[3] = { 10, 11, 12 }; float oa[3]; DWRITE_GLYPH_OFFSET oo[3]; UINT32 i; sp_simple(&c, 3, adv);
    for (i = 0; i < 3; i++) { c.off[i].advanceOffset = 1.5f * i; c.off[i].ascenderOffset = -2.0f * i; }
    CHECK_HR(sp_run(a1, 0, 0, 0, &c, oa, oo), S_OK);
    for (i = 0; i < 3; i++) { CHECK_F(oa[i], adv[i]); CHECK_F(oo[i].advanceOffset, c.off[i].advanceOffset); CHECK_F(oo[i].ascenderOffset, c.off[i].ascenderOffset); }
    TA_END();
}
TEST(sp_single_glyph_leading_trailing) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 2, 3, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 15.0f); CHECK_F(oo[0].advanceOffset, 2.0f);   /* glyph shifted by the leading amount only */
    TA_END();
}
TEST(sp_leading_only) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 4.5f, 0, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 14.5f); CHECK_F(oo[0].advanceOffset, 4.5f);
    TA_END();
}
TEST(sp_trailing_only) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 0, 4.5f, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 14.5f); CHECK_F(oo[0].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_every_cluster_gets_spacing) {
    TA_BEGIN(); sp_case c; float adv[4] = { 10, 20, 30, 40 }; float oa[4]; DWRITE_GLYPH_OFFSET oo[4]; UINT32 i; sp_simple(&c, 4, adv);
    CHECK_HR(sp_run(a1, 1, 2, 0, &c, oa, oo), S_OK);
    for (i = 0; i < 4; i++) { CHECK_F(oa[i], adv[i] + 3.0f); CHECK_F(oo[i].advanceOffset, 1.0f); }
    TA_END();
}
TEST(sp_multi_glyph_cluster_edges) {
    /* cluster of 3 glyphs followed by a single glyph cluster: leading on glyph 0, trailing on glyph 2 */
    TA_BEGIN(); sp_case c; const UINT32 per[2] = { 3, 1 }; float adv[4] = { 10, 20, 30, 40 }; float oa[4]; DWRITE_GLYPH_OFFSET oo[4];
    sp_clusters(&c, 2, per, adv);
    CHECK_HR(sp_run(a1, 1, 2, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 11.0f); CHECK_F(oa[1], 20.0f); CHECK_F(oa[2], 32.0f); CHECK_F(oa[3], 43.0f);
    CHECK_F(oo[0].advanceOffset, 1.0f); CHECK_F(oo[1].advanceOffset, 0.0f); CHECK_F(oo[2].advanceOffset, 0.0f); CHECK_F(oo[3].advanceOffset, 1.0f);
    TA_END();
}
TEST(sp_ligature_two_chars_one_glyph) {
    /* 3 characters, 2 glyphs: clusterMap = {0,0,1} */
    TA_BEGIN(); sp_case c; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; memset(&c, 0, sizeof c);
    c.n = 3; c.g = 2; c.cm[0] = 0; c.cm[1] = 0; c.cm[2] = 1; c.adv[0] = 20; c.adv[1] = 10; c.gp[0] = ta_gp(0, 1, 0, 0); c.gp[1] = ta_gp(0, 1, 0, 0);
    CHECK_HR(sp_run(a1, 1, 2, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 23.0f); CHECK_F(oa[1], 13.0f); CHECK_F(oo[0].advanceOffset, 1.0f); CHECK_F(oo[1].advanceOffset, 1.0f);
    TA_END();
}
TEST(sp_one_char_many_glyphs_decomposition) {
    /* clusterMap {0,3}: first character produced 3 glyphs, second 1 */
    TA_BEGIN(); sp_case c; float oa[4]; DWRITE_GLYPH_OFFSET oo[4]; memset(&c, 0, sizeof c);
    c.n = 2; c.g = 4; c.cm[0] = 0; c.cm[1] = 3; c.adv[0] = 1; c.adv[1] = 2; c.adv[2] = 3; c.adv[3] = 4;
    c.gp[0] = ta_gp(0, 1, 0, 0); c.gp[1] = ta_gp(0, 0, 0, 0); c.gp[2] = ta_gp(0, 0, 0, 0); c.gp[3] = ta_gp(0, 1, 0, 0);
    CHECK_HR(sp_run(a1, 0.5f, 0.25f, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 1.5f); CHECK_F(oa[1], 2.0f); CHECK_F(oa[2], 3.25f); CHECK_F(oa[3], 4.75f);
    CHECK_F(oo[3].advanceOffset, 0.5f); CHECK_F(oo[1].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_leading_zws_glyph_first) {
    /* cluster {ZWS, real}: the zero-width glyph moves with the leading amount, the real glyph gets the advance */
    TA_BEGIN(); sp_case c; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; memset(&c, 0, sizeof c);
    c.n = 1; c.g = 2; c.cm[0] = 0; c.adv[0] = 0; c.adv[1] = 10; c.gp[0] = ta_gp(0, 1, 0, 1); c.gp[1] = ta_gp(0, 0, 0, 0);
    CHECK_HR(sp_run(a1, 3, 5, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 0.0f); CHECK_F(oa[1], 18.0f); CHECK_F(oo[0].advanceOffset, 3.0f); CHECK_F(oo[1].advanceOffset, 3.0f);
    TA_END();
}
TEST(sp_trailing_zws_glyph_last) {
    /* cluster {real, ZWS}: the advance goes to the real glyph, the zero-width glyph is pulled back by the trailing amount */
    TA_BEGIN(); sp_case c; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; memset(&c, 0, sizeof c);
    c.n = 1; c.g = 2; c.cm[0] = 0; c.adv[0] = 10; c.adv[1] = 0; c.gp[0] = ta_gp(0, 1, 0, 0); c.gp[1] = ta_gp(0, 0, 0, 1);
    CHECK_HR(sp_run(a1, 3, 5, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 18.0f); CHECK_F(oa[1], 0.0f); CHECK_F(oo[0].advanceOffset, 3.0f); CHECK_F(oo[1].advanceOffset, -5.0f);
    TA_END();
}
TEST(sp_zws_both_ends) {
    TA_BEGIN(); sp_case c; float oa[3]; DWRITE_GLYPH_OFFSET oo[3]; memset(&c, 0, sizeof c);
    c.n = 1; c.g = 3; c.cm[0] = 0; c.adv[0] = 0; c.adv[1] = 10; c.adv[2] = 0;
    c.gp[0] = ta_gp(0, 1, 0, 1); c.gp[1] = ta_gp(0, 0, 0, 0); c.gp[2] = ta_gp(0, 0, 0, 1);
    CHECK_HR(sp_run(a1, 2, 4, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 0.0f); CHECK_F(oa[1], 16.0f); CHECK_F(oa[2], 0.0f);
    CHECK_F(oo[0].advanceOffset, 2.0f); CHECK_F(oo[1].advanceOffset, 2.0f); CHECK_F(oo[2].advanceOffset, -4.0f);
    TA_END();
}
TEST(sp_all_zws_cluster_unchanged) {
    TA_BEGIN(); sp_case c; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; memset(&c, 0, sizeof c);
    c.n = 1; c.g = 2; c.cm[0] = 0; c.adv[0] = 0; c.adv[1] = 0; c.gp[0] = ta_gp(0, 1, 0, 1); c.gp[1] = ta_gp(0, 0, 0, 1);
    CHECK_HR(sp_run(a1, 3, 5, 9, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 0.0f); CHECK_F(oa[1], 0.0f); CHECK_F(oo[0].advanceOffset, 0.0f); CHECK_F(oo[1].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_diacritic_flag_is_not_special) {
    /* only the zero-width-space bit matters, a diacritic glyph is an ordinary member of the cluster */
    TA_BEGIN(); sp_case c; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; memset(&c, 0, sizeof c);
    c.n = 1; c.g = 2; c.cm[0] = 0; c.adv[0] = 10; c.adv[1] = 0; c.gp[0] = ta_gp(0, 1, 0, 0); c.gp[1] = ta_gp(0, 0, 1, 0);
    CHECK_HR(sp_run(a1, 1, 2, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 11.0f); CHECK_F(oa[1], 2.0f); CHECK_F(oo[0].advanceOffset, 1.0f); CHECK_F(oo[1].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_min_advance_pads_narrow_cluster) {
    /* width 4 < 10: the missing 6 is split half/half between leading and trailing */
    TA_BEGIN(); sp_case c; float adv[1] = { 4 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 0, 0, 10, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 10.0f); CHECK_F(oo[0].advanceOffset, 3.0f);
    TA_END();
}
TEST(sp_min_advance_not_needed) {
    TA_BEGIN(); sp_case c; float adv[2] = { 10, 12 }; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; sp_simple(&c, 2, adv);
    CHECK_HR(sp_run(a1, 0, 0, 10, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 10.0f); CHECK_F(oa[1], 12.0f); CHECK_F(oo[0].advanceOffset, 0.0f); CHECK_F(oo[1].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_min_advance_equal_width_unchanged) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 0, 0, 10, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 10.0f); CHECK_F(oo[0].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_min_advance_on_top_of_positive_spacing) {
    /* positive spacing does not count towards the cluster width compared against minimumAdvanceWidth */
    TA_BEGIN(); sp_case c; float adv[1] = { 4 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 1, 1, 10, &c, oa, oo), S_OK);
    CHECK_F(oo[0].advanceOffset, 4.0f); CHECK_F(oa[0], 12.0f);
    TA_END();
}
TEST(sp_min_advance_multi_glyph_cluster_sums_advances) {
    TA_BEGIN(); sp_case c; const UINT32 per[1] = { 3 }; float adv[3] = { 2, 2, 2 }; float oa[3]; DWRITE_GLYPH_OFFSET oo[3];
    sp_clusters(&c, 1, per, adv);
    CHECK_HR(sp_run(a1, 0, 0, 10, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 4.0f); CHECK_F(oa[1], 2.0f); CHECK_F(oa[2], 4.0f); CHECK_F(oo[0].advanceOffset, 2.0f);
    TA_END();
}
TEST(sp_negative_leading_plain) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, -3, 0, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 7.0f); CHECK_F(oo[0].advanceOffset, -3.0f);
    TA_END();
}
TEST(sp_negative_trailing_plain) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 0, -3, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 7.0f); CHECK_F(oo[0].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_negative_leading_limited_to_cluster_width) {
    /* -30 cannot shrink a width-10 cluster below the minimum 0: limited to -10 */
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, -30, 0, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 0.0f); CHECK_F(oo[0].advanceOffset, -10.0f);
    TA_END();
}
TEST(sp_negative_trailing_limited_to_cluster_width) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 0, -30, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 0.0f); CHECK_F(oo[0].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_negative_limited_by_min_advance) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, -5, 0, 8, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 8.0f); CHECK_F(oo[0].advanceOffset, -2.0f);
    CHECK_HR(sp_run(a1, 0, -5, 8, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 8.0f); CHECK_F(oo[0].advanceOffset, 0.0f);
    TA_END();
}
TEST(sp_negative_both_fit) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, -2, -3, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 5.0f); CHECK_F(oo[0].advanceOffset, -2.0f);
    TA_END();
}
TEST(sp_negative_both_exceed_width) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, -8, -8, 0, &c, oa, oo), S_OK);
    CHECK(oa[0] >= 0.0f);        /* never negative: the shrink is shared out within the cluster width */
    CHECK(sp_matches_model(a1, -8, -8, 0, &c));
    TA_END();
}
TEST(sp_negative_both_with_min_advance_restores_floor) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, -1.5f, -1.5f, 8, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 8.0f); CHECK_F(oo[0].advanceOffset, -1.0f);
    TA_END();
}
TEST(sp_negative_leading_positive_trailing) {
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, -3, 2, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 9.0f); CHECK_F(oo[0].advanceOffset, -3.0f);
    CHECK_HR(sp_run(a1, 2, -3, 0, &c, oa, oo), S_OK);
    CHECK_F(oa[0], 9.0f); CHECK_F(oo[0].advanceOffset, 2.0f);
    TA_END();
}
TEST(sp_negative_spacing_ignored_when_cluster_below_min) {
    /* width 4 < minimum 10: negative leading is dropped (nothing may shrink), the 6 missing units are added */
    TA_BEGIN(); sp_case c; float adv[1] = { 4 }; float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, -1, 2, 10, &c, oa, oo), S_OK);
    CHECK_F(oo[0].advanceOffset, 3.0f); CHECK_F(oa[0], 4.0f + 3.0f + 5.0f);
    TA_END();
}
TEST(sp_existing_offsets_accumulate) {
    TA_BEGIN(); sp_case c; float adv[2] = { 10, 10 }; float oa[2]; DWRITE_GLYPH_OFFSET oo[2]; sp_simple(&c, 2, adv);
    c.off[0].advanceOffset = 1.25f; c.off[1].advanceOffset = -2.5f;
    CHECK_HR(sp_run(a1, 2, 0, 0, &c, oa, oo), S_OK);
    CHECK_F(oo[0].advanceOffset, 3.25f); CHECK_F(oo[1].advanceOffset, -0.5f);
    TA_END();
}
TEST(sp_swapped_arguments_model_rtl_use) {
    /* the method has no direction argument: callers working on right-to-left runs swap leading and trailing. The advances
       are symmetric, only the glyph offset follows the (new) leading amount. */
    TA_BEGIN(); sp_case c; float adv[1] = { 10 }; float oa[1], ob[1]; DWRITE_GLYPH_OFFSET oo[1], op[1]; sp_simple(&c, 1, adv);
    CHECK_HR(sp_run(a1, 2, 5, 0, &c, oa, oo), S_OK);
    CHECK_HR(sp_run(a1, 5, 2, 0, &c, ob, op), S_OK);
    CHECK_F(oa[0], ob[0]); CHECK_F(oo[0].advanceOffset, 2.0f); CHECK_F(op[0].advanceOffset, 5.0f);
    TA_END();
}
TEST(sp_aliased_equals_distinct) {
    TA_BEGIN(); sp_case c, d; float adv[5] = { 3, 4, 5, 6, 7 }; float oa[5]; DWRITE_GLYPH_OFFSET oo[5]; UINT32 i; sp_simple(&c, 5, adv);
    for (i = 0; i < 5; i++) { c.off[i].advanceOffset = (float)i; c.off[i].ascenderOffset = 9; }
    d = c;
    CHECK_HR(sp_run(a1, 1.5f, 0.5f, 6, &c, oa, oo), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1.5f, 0.5f, 6, d.n, d.g, d.cm, d.adv, d.off, d.gp, d.adv, d.off), S_OK);
    for (i = 0; i < 5; i++) { CHECK_F(d.adv[i], oa[i]); CHECK_F(d.off[i].advanceOffset, oo[i].advanceOffset); CHECK_F(d.off[i].ascenderOffset, oo[i].ascenderOffset); }
    TA_END();
}
TEST(sp_mixed_alias_outputs) {
    TA_BEGIN(); sp_case c, d; float adv[3] = { 3, 4, 5 }; float oa[3]; DWRITE_GLYPH_OFFSET oo[3]; sp_simple(&c, 3, adv); d = c;
    CHECK_HR(sp_run(a1, 1, 1, 0, &c, oa, oo), S_OK);
    CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1, 1, 0, d.n, d.g, d.cm, d.adv, d.off, d.gp, d.adv, oo), S_OK);   /* advances aliased only */
    for (int i = 0; i < 3; i++) { CHECK_F(d.adv[i], oa[i]); CHECK_F(oo[i].advanceOffset, 1.0f); }
    TA_END();
}
TEST(sp_via_analyzer2_interface_same_result) {
    TA_BEGIN(); sp_case c; float adv[2] = { 10, 20 }; float oa[2], ob[2]; DWRITE_GLYPH_OFFSET oo[2], op[2]; sp_simple(&c, 2, adv);
    CHECK_HR(sp_run(a1, 1, 2, 0, &c, oa, oo), S_OK);
    CHECK_HR(IDWriteTextAnalyzer2_ApplyCharacterSpacing(a2, 1, 2, 0, c.n, c.g, c.cm, c.adv, c.off, c.gp, ob, op), S_OK);
    CHECK_F(oa[0], ob[0]); CHECK_F(oa[1], ob[1]); CHECK_F(oo[1].advanceOffset, op[1].advanceOffset);
    TA_END();
}

/* ---- model driven sweeps ------------------------------------------------------------------------------------------------- */
static const float k_spacing_vals[] = { 0.0f, 0.25f, 1.0f, 2.5f, 7.0f, -0.25f, -1.0f, -2.5f, -7.0f, -40.0f };
static const float k_min_vals[] = { 0.0f, 3.0f, 10.0f, 25.0f };

TEST(sp_sweep_single_glyph_clusters) {
    TA_BEGIN(); sp_case c; float adv[3] = { 4, 10, 30 }; UINT32 i, j, k; int bad = 0; sp_simple(&c, 3, adv);
    for (i = 0; i < sizeof k_spacing_vals / sizeof *k_spacing_vals; i++)
        for (j = 0; j < sizeof k_spacing_vals / sizeof *k_spacing_vals; j++)
            for (k = 0; k < sizeof k_min_vals / sizeof *k_min_vals; k++)
                if (!sp_matches_model(a1, k_spacing_vals[i], k_spacing_vals[j], k_min_vals[k], &c)) bad++;
    CHECK(bad == 0);
    TA_END();
}
TEST(sp_sweep_multi_glyph_clusters) {
    TA_BEGIN(); sp_case c; const UINT32 per[4] = { 1, 3, 2, 4 }; float adv[10] = { 5, 3, 4, 3, 6, 6, 1, 2, 3, 2 }; UINT32 i, j, k; int bad = 0;
    sp_clusters(&c, 4, per, adv);
    for (i = 0; i < sizeof k_spacing_vals / sizeof *k_spacing_vals; i++)
        for (j = 0; j < sizeof k_spacing_vals / sizeof *k_spacing_vals; j++)
            for (k = 0; k < sizeof k_min_vals / sizeof *k_min_vals; k++)
                if (!sp_matches_model(a1, k_spacing_vals[i], k_spacing_vals[j], k_min_vals[k], &c)) bad++;
    CHECK(bad == 0);
    TA_END();
}
TEST(sp_sweep_zero_width_glyphs) {
    TA_BEGIN(); sp_case c; UINT32 i, j, k; int bad = 0; memset(&c, 0, sizeof c);
    /* clusters: {z,r,z} {r,r} {z,z} {z,r} */
    static const unsigned zw[9] = { 1, 0, 1, 0, 0, 1, 1, 1, 0 }; static const float av[9] = { 0, 8, 0, 5, 6, 0, 0, 0, 9 };
    c.n = 4; c.g = 9; c.cm[0] = 0; c.cm[1] = 3; c.cm[2] = 5; c.cm[3] = 7;
    for (i = 0; i < 9; i++) { c.adv[i] = av[i]; c.gp[i] = ta_gp(0, i == 0 || i == 3 || i == 5 || i == 7, 0, zw[i]); }
    for (i = 0; i < sizeof k_spacing_vals / sizeof *k_spacing_vals; i++)
        for (j = 0; j < sizeof k_spacing_vals / sizeof *k_spacing_vals; j++)
            for (k = 0; k < sizeof k_min_vals / sizeof *k_min_vals; k++)
                if (!sp_matches_model(a1, k_spacing_vals[i], k_spacing_vals[j], k_min_vals[k], &c)) bad++;
    CHECK(bad == 0);
    TA_END();
}
TEST(sp_sweep_ligature_maps) {
    /* several characters sharing one glyph cluster, and characters without own glyph */
    TA_BEGIN(); sp_case c; UINT32 i, j, k; int bad = 0; memset(&c, 0, sizeof c);
    c.n = 6; c.g = 4; c.cm[0] = 0; c.cm[1] = 0; c.cm[2] = 0; c.cm[3] = 1; c.cm[4] = 2; c.cm[5] = 2;
    for (i = 0; i < 4; i++) { c.adv[i] = 4.0f + i; c.gp[i] = ta_gp(0, 1, 0, 0); }
    for (i = 0; i < sizeof k_spacing_vals / sizeof *k_spacing_vals; i++)
        for (j = 0; j < sizeof k_spacing_vals / sizeof *k_spacing_vals; j++)
            for (k = 0; k < sizeof k_min_vals / sizeof *k_min_vals; k++)
                if (!sp_matches_model(a1, k_spacing_vals[i], k_spacing_vals[j], k_min_vals[k], &c)) bad++;
    CHECK(bad == 0);
    TA_END();
}
TEST(sp_sweep_preserves_text_order_invariants) {
    /* for non-negative spacing every glyph advance grows by a known amount and the sum grows by exactly L+T per cluster
       (plus padding to the minimum) */
    TA_BEGIN(); sp_case c; float adv[6] = { 3, 4, 5, 6, 7, 8 }; float oa[6]; DWRITE_GLYPH_OFFSET oo[6]; UINT32 i; sp_simple(&c, 6, adv);
    CHECK_HR(sp_run(a1, 1.25f, 0.75f, 0, &c, oa, oo), S_OK);
    for (i = 0; i < 6; i++) CHECK_F(oa[i] - adv[i], 2.0f);
    TA_END();
}
TEST(sp_long_run_64_glyphs) {
    TA_BEGIN(); sp_case c; float adv[64]; float oa[64]; DWRITE_GLYPH_OFFSET oo[64]; UINT32 i;
    for (i = 0; i < 64; i++) adv[i] = 1.0f + (float)(i % 5);
    sp_simple(&c, 64, adv);
    CHECK_HR(sp_run(a1, 0.5f, 1.5f, 0, &c, oa, oo), S_OK);
    for (i = 0; i < 64; i++) { CHECK_F(oa[i], adv[i] + 2.0f); CHECK_F(oo[i].advanceOffset, 0.5f); }
    CHECK(sp_matches_model(a1, -0.5f, 1.5f, 2, &c));
    TA_END();
}
TEST(sp_non_cluster_start_flag_irrelevant_in_retail) {
    /* the cluster boundaries come from clusterMap; isClusterStart on the glyph properties does not alter the arithmetic */
    TA_BEGIN(); sp_case c, d; float adv[2] = { 10, 10 }; float oa[2], ob[2]; DWRITE_GLYPH_OFFSET oo[2], op[2]; sp_simple(&c, 2, adv); d = c;
    d.gp[0] = ta_gp(0, 1, 0, 0); d.gp[1] = ta_gp(5, 1, 1, 0);
    CHECK_HR(sp_run(a1, 1, 2, 0, &c, oa, oo), S_OK);
    CHECK_HR(sp_run(a1, 1, 2, 0, &d, ob, op), S_OK);
    CHECK_F(oa[0], ob[0]); CHECK_F(oa[1], ob[1]);
    TA_END();
}
