/* Justification and character spacing relations that hold for every input (derived from DWriteTextAnalyzer::GetJustificationOpportunities,
 * TextAnalyzer::GetJustificationOpportunities and TextAnalyzer::ApplyCharacterSpacing of the Windows 10 dwrite.dll):
 *   - GetJustificationOpportunities without a font face computes every float from fontEmSize times power-of-two constants, so the
 *     entries scale exactly with the em size; the other fields do not depend on it.
 *   - ApplyCharacterSpacing adds the leading amount to the first non zero-width glyph of a cluster (and to the offsets up to it) and the
 *     trailing amount to the last non zero-width glyph; clusters are processed independently.
 */
#include "r2.h"

typedef struct { float lo, hi, cmax; UINT32 flags; } jopp;
static void opp_raw(const DWRITE_JUSTIFICATION_OPPORTUNITY *o, jopp *j) {
    memcpy(&j->lo, &o->expansionMinimum, 4); memcpy(&j->hi, &o->expansionMaximum, 4); memcpy(&j->cmax, &o->compressionMaximum, 4);
    memcpy(&j->flags, (const char *)o + 12, 4);
}
static int scaled(float base, float other, float ratio) {
    if (base == FLT_MAX) return other == FLT_MAX;
    return other == base * ratio;
}

static const struct { const WCHAR *s; const char *name; } k_scr[] = {
    { L"A", "Latin" }, { L"\x0627", "Arabic" }, { L"\x05D0", "Hebrew" }, { L"\x0E01", "Thai" }, { L"\x4E00", "Han" }, { L"\x3042", "Hiragana" },
    { L"\x0905", "Devanagari" }, { L"\x0416", "Cyrillic" }, { L"\xAC00", "Hangul" },
};
static const WCHAR k_chr[] = { L'A', L' ', 0x0640, 0x3042, 0x3000, 0x0627 };

static HRESULT jopp_call(r2_ctx *c, float em, DWRITE_SCRIPT_ANALYSIS sa, WCHAR ch, unsigned cls, UINT32 fill, DWRITE_JUSTIFICATION_OPPORTUNITY *o) {
    UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp; WCHAR t[1]; t[0] = ch;
    memset(&gp, 0, sizeof gp); gp.justification = cls; gp.isClusterStart = 1;
    memset(o, (int)fill, sizeof *o);
    return IDWriteTextAnalyzer1_GetJustificationOpportunities(c->a1, NULL, em, sa, 1, 1, t, cm, &gp, o);
}

RT(jo_entries_scale_exactly_with_em_for_every_class) {
    R2_BEGIN(); unsigned s, ch, cls, bad = 0, calls = 0; static const float ems[] = { 8, 32, 64, 0.5f, 1 };
    for (s = 0; s < sizeof k_scr / sizeof *k_scr; s++) {
        DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, k_scr[s].s, 1);
        for (ch = 0; ch < sizeof k_chr / sizeof *k_chr; ch++) for (cls = 0; cls < 16; cls++) {
            DWRITE_JUSTIFICATION_OPPORTUNITY b, o; jopp jb, jo; unsigned e;
            if (jopp_call(&c_, 16, sa, k_chr[ch], cls, 0, &b) != S_OK) { CHECKF(0, "%s class %u: call failed", k_scr[s].name, cls); continue; }
            opp_raw(&b, &jb);
            for (e = 0; e < sizeof ems / sizeof *ems; e++) {
                float ratio = ems[e] / 16.0f;
                calls++;
                if (jopp_call(&c_, ems[e], sa, k_chr[ch], cls, 0, &o) != S_OK) { bad++; continue; }
                opp_raw(&o, &jo);
                if (!scaled(jb.lo, jo.lo, ratio) || !scaled(jb.hi, jo.hi, ratio) || !scaled(jb.cmax, jo.cmax, ratio) || jb.flags != jo.flags) {
                    if (bad++ < 8) CHECKF(0, "%s U+%04X class %u em %g: (%g %g %g 0x%x) vs base (%g %g %g 0x%x)", k_scr[s].name, k_chr[ch], cls, ems[e], jo.lo, jo.hi, jo.cmax, jo.flags, jb.lo, jb.hi, jb.cmax, jb.flags);
                }
            }
        }
    }
    CHECKF(bad == 0, "%u of %u em comparisons differ", bad, calls);
    R2_END();
}
RT(jo_result_independent_of_previous_buffer_contents) {
    R2_BEGIN(); unsigned s, cls, bad = 0;
    for (s = 0; s < sizeof k_scr / sizeof *k_scr; s++) {
        DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, k_scr[s].s, 1);
        for (cls = 0; cls < 16; cls++) {
            DWRITE_JUSTIFICATION_OPPORTUNITY a, b, d; jopp ja, jb, jd;
            jopp_call(&c_, 16, sa, k_scr[s].s[0], cls, 0x00, &a); jopp_call(&c_, 16, sa, k_scr[s].s[0], cls, 0xFF, &b); jopp_call(&c_, 16, sa, k_scr[s].s[0], cls, 0x5A, &d);
            opp_raw(&a, &ja); opp_raw(&b, &jb); opp_raw(&d, &jd);
            if (memcmp(&ja, &jb, sizeof ja) || memcmp(&ja, &jd, sizeof ja)) { if (bad++ < 5) CHECKF(0, "%s class %u depends on the buffer contents", k_scr[s].name, cls); }
        }
    }
    CHECK(bad == 0);
    R2_END();
}
RT(jo_priorities_fit_their_bit_fields) {
    /* flags: expansionPriority:8, compressionPriority:8, allowResidualExpansion:1, allowResidualCompression:1, applyToLeadingEdge:1, applyToTrailingEdge:1, reserved:12 */
    R2_BEGIN(); unsigned s, ch, cls, bad = 0;
    for (s = 0; s < sizeof k_scr / sizeof *k_scr; s++) {
        DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, k_scr[s].s, 1);
        for (ch = 0; ch < sizeof k_chr / sizeof *k_chr; ch++) for (cls = 0; cls < 16; cls++) {
            DWRITE_JUSTIFICATION_OPPORTUNITY o; jopp j;
            jopp_call(&c_, 16, sa, k_chr[ch], cls, 0, &o); opp_raw(&o, &j);
            if (j.flags >> 20) { if (bad++ < 5) CHECKF(0, "%s class %u: reserved flag bits set (0x%x)", k_scr[s].name, cls, j.flags); }
            if (!(j.lo >= 0.0f) || !(j.hi >= j.lo) || !(j.cmax >= 0.0f)) { if (bad++ < 5) CHECKF(0, "%s class %u: bounds %g %g %g", k_scr[s].name, cls, j.lo, j.hi, j.cmax); }
        }
    }
    CHECK(bad == 0);
    R2_END();
}
RT(jo_text_longer_than_glyph_run_zero_width_cluster_tail) {
    /* two glyphs for one cluster: the continuation glyph (not a cluster start) copies the cluster's opportunity */
    R2_BEGIN(); UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[2]; DWRITE_JUSTIFICATION_OPPORTUNITY o[2]; WCHAR t[1] = { L'A' };
    memset(gp, 0, sizeof gp); memset(o, 0, sizeof o); gp[0].justification = 0; gp[0].isClusterStart = 1; gp[1].isClusterStart = 0;
    CHECK_HR(IDWriteTextAnalyzer1_GetJustificationOpportunities(a1, NULL, 16.0f, r2_sa_of(&c_, L"A", 1), 1, 2, t, cm, gp, o), S_OK);
    jopp j0, j1; opp_raw(&o[0], &j0); opp_raw(&o[1], &j1);
    CHECKF(j0.lo == j1.lo && j0.hi == j1.hi && j0.cmax == j1.cmax, "continuation glyph entry differs: (%g %g %g) (%g %g %g)", j0.lo, j0.hi, j0.cmax, j1.lo, j1.hi, j1.cmax);
    R2_END();
}

/* ---- ApplyCharacterSpacing algebra ------------------------------------------------------------------------------------------------------------------ */
typedef struct { UINT32 ntext, nglyph; UINT16 cm[8]; float adv[8]; DWRITE_GLYPH_OFFSET off[8]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[8]; } fix;
static void fix_zero(fix *x) { memset(x, 0, sizeof *x); }
static void fix_a(fix *x) {                      /* four single-glyph clusters */
    unsigned i; fix_zero(x); x->ntext = 4; x->nglyph = 4;
    for (i = 0; i < 4; i++) { x->cm[i] = (UINT16)i; x->adv[i] = 10.0f * (i + 1); x->gp[i].isClusterStart = 1; x->off[i].advanceOffset = (float)i; x->off[i].ascenderOffset = 7.0f; }
}
static void fix_b(fix *x) {                      /* glyph 0 | glyphs 1,2 (two glyph cluster) | glyph 3 */
    fix_zero(x); x->ntext = 4; x->nglyph = 4; x->cm[0] = 0; x->cm[1] = 1; x->cm[2] = 1; x->cm[3] = 3;
    x->adv[0] = 10; x->adv[1] = 20; x->adv[2] = 6; x->adv[3] = 30;
    x->gp[0].isClusterStart = 1; x->gp[1].isClusterStart = 1; x->gp[2].isClusterStart = 0; x->gp[3].isClusterStart = 1;
}
static void fix_c(fix *x) {                      /* clusters with zero width space glyphs at both ends */
    fix_zero(x); x->ntext = 3; x->nglyph = 5; x->cm[0] = 0; x->cm[1] = 3; x->cm[2] = 4;
    x->adv[0] = 0; x->adv[1] = 12; x->adv[2] = 0; x->adv[3] = 9; x->adv[4] = 14;
    x->gp[0].isClusterStart = 1; x->gp[0].isZeroWidthSpace = 1; x->gp[1].isClusterStart = 0; x->gp[2].isClusterStart = 0; x->gp[2].isZeroWidthSpace = 1;
    x->gp[3].isClusterStart = 1; x->gp[4].isClusterStart = 1;
    x->cm[1] = 0; x->cm[2] = 3;                  /* text 0..1 -> glyphs 0..2, text 2 -> glyph 3 ... keep cluster starts at glyph 0, 3, 4 */
    x->ntext = 3; x->cm[0] = 0; x->cm[1] = 3; x->cm[2] = 4;
}
typedef void (*fixfn)(fix *);
static const fixfn k_fix[] = { fix_a, fix_b, fix_c };

static HRESULT apply(r2_ctx *c, float L, float T, float M, const fix *x, const float *adv, const DWRITE_GLYPH_OFFSET *off, float *oadv, DWRITE_GLYPH_OFFSET *ooff) {
    return IDWriteTextAnalyzer1_ApplyCharacterSpacing(c->a1, L, T, M, x->ntext, x->nglyph, x->cm, adv, off, x->gp, oadv, ooff);
}
static int same_run(const fix *x, const float *a, const DWRITE_GLYPH_OFFSET *ao, const float *b, const DWRITE_GLYPH_OFFSET *bo) {
    UINT32 i;
    for (i = 0; i < x->nglyph; i++) if (a[i] != b[i] || ao[i].advanceOffset != bo[i].advanceOffset || ao[i].ascenderOffset != bo[i].ascenderOffset) return 0;
    return 1;
}
RT(sc_positive_spacing_is_additive) {
    R2_BEGIN(); unsigned fi, a, b, bad = 0;
    static const float v[][2] = { { 0, 0 }, { 1, 0 }, { 0, 2 }, { 3, 4 }, { 8, 1 }, { 0.5f, 0.25f } };
    for (fi = 0; fi < 3; fi++) for (a = 0; a < 6; a++) for (b = 0; b < 6; b++) {
        fix x; float adv1[8], adv2[8], adv3[8]; DWRITE_GLYPH_OFFSET off1[8], off2[8], off3[8];
        k_fix[fi](&x);
        if (apply(&c_, v[a][0], v[a][1], 0, &x, x.adv, x.off, adv1, off1) != S_OK) { bad++; continue; }
        if (apply(&c_, v[b][0], v[b][1], 0, &x, adv1, off1, adv2, off2) != S_OK) { bad++; continue; }
        if (apply(&c_, v[a][0] + v[b][0], v[a][1] + v[b][1], 0, &x, x.adv, x.off, adv3, off3) != S_OK) { bad++; continue; }
        if (!same_run(&x, adv2, off2, adv3, off3)) { if (bad++ < 6) CHECKF(0, "fixture %u pairs %u,%u: two passes differ from one", fi, a, b); }
    }
    CHECK(bad == 0);
    R2_END();
}
RT(sc_total_advance_grows_by_spacing_per_cluster) {
    /* every cluster that holds a non zero-width glyph gains exactly leading + trailing */
    R2_BEGIN(); unsigned fi, k, bad = 0; static const float v[][2] = { { 1, 2 }, { 5, 0 }, { 0, 7 }, { 0.5f, 1.5f } };
    static const unsigned clusters[3] = { 4, 3, 3 };
    for (fi = 0; fi < 3; fi++) for (k = 0; k < 4; k++) {
        fix x; float oadv[8]; DWRITE_GLYPH_OFFSET ooff[8]; double before = 0, after = 0; UINT32 i;
        k_fix[fi](&x);
        if (apply(&c_, v[k][0], v[k][1], 0, &x, x.adv, x.off, oadv, ooff) != S_OK) { bad++; continue; }
        for (i = 0; i < x.nglyph; i++) { before += x.adv[i]; after += oadv[i]; }
        if (after - before != (double)clusters[fi] * (v[k][0] + v[k][1])) { if (bad++ < 6) CHECKF(0, "fixture %u spacing %g,%g: grew by %g, expected %g", fi, v[k][0], v[k][1], after - before, clusters[fi] * (v[k][0] + v[k][1])); }
    }
    CHECK(bad == 0);
    R2_END();
}
RT(sc_scaling_inputs_by_two_scales_outputs) {
    R2_BEGIN(); unsigned fi, k, bad = 0;
    static const float v[][3] = { { 1, 2, 0 }, { 0, 0, 40 }, { -3, 0, 0 }, { 0, -4, 0 }, { -2, -2, 25 }, { 3, -1, 33 }, { 0.5f, 0.5f, 15 } };
    for (fi = 0; fi < 3; fi++) for (k = 0; k < 7; k++) {
        fix x, y; float a1_[8], a2_[8]; DWRITE_GLYPH_OFFSET o1[8], o2[8]; UINT32 i;
        k_fix[fi](&x); y = x;
        for (i = 0; i < y.nglyph; i++) { y.adv[i] *= 2; y.off[i].advanceOffset *= 2; y.off[i].ascenderOffset *= 2; }
        if (apply(&c_, v[k][0], v[k][1], v[k][2], &x, x.adv, x.off, a1_, o1) != S_OK) { bad++; continue; }
        if (apply(&c_, v[k][0] * 2, v[k][1] * 2, v[k][2] * 2, &y, y.adv, y.off, a2_, o2) != S_OK) { bad++; continue; }
        for (i = 0; i < x.nglyph; i++)
            if (a2_[i] != a1_[i] * 2 || o2[i].advanceOffset != o1[i].advanceOffset * 2 || o2[i].ascenderOffset != o1[i].ascenderOffset * 2) { if (bad++ < 6) CHECKF(0, "fixture %u case %u glyph %u does not scale", fi, k, i); break; }
    }
    CHECK(bad == 0);
    R2_END();
}
RT(sc_min_advance_is_idempotent) {
    R2_BEGIN(); unsigned fi, m, bad = 0; static const float mins[] = { 5, 15, 25, 50, 100 };
    for (fi = 0; fi < 3; fi++) for (m = 0; m < 5; m++) {
        fix x; float a1_[8], a2_[8]; DWRITE_GLYPH_OFFSET o1[8], o2[8];
        k_fix[fi](&x);
        apply(&c_, 0, 0, mins[m], &x, x.adv, x.off, a1_, o1);
        apply(&c_, 0, 0, mins[m], &x, a1_, o1, a2_, o2);
        if (!same_run(&x, a1_, o1, a2_, o2)) { if (bad++ < 6) CHECKF(0, "fixture %u min %g changes on the second pass", fi, mins[m]); }
    }
    CHECK(bad == 0);
    R2_END();
}
RT(sc_min_advance_guarantees_cluster_width) {
    R2_BEGIN(); unsigned m, bad = 0; static const float mins[] = { 5, 15, 25, 50, 100 };
    for (m = 0; m < 5; m++) {
        fix x; float oadv[8]; DWRITE_GLYPH_OFFSET ooff[8];
        fix_a(&x);
        apply(&c_, 0, 0, mins[m], &x, x.adv, x.off, oadv, ooff);
        { UINT32 i; for (i = 0; i < 4; i++) if (oadv[i] < mins[m]) { if (bad++ < 6) CHECKF(0, "min %g: cluster %u has width %g", mins[m], i, oadv[i]); } }
    }
    CHECK(bad == 0);
    R2_END();
}
RT(sc_clusters_are_independent) {
    /* spacing a run of clusters equals spacing each cluster on its own */
    R2_BEGIN(); fix x; float all_adv[8], one_adv[8]; DWRITE_GLYPH_OFFSET all_off[8], one_off[8]; unsigned bad = 0, g;
    fix_a(&x);
    CHECK_HR(apply(&c_, 2, 3, 12, &x, x.adv, x.off, all_adv, all_off), S_OK);
    for (g = 0; g < 4; g++) {
        fix y; fix_zero(&y); y.ntext = 1; y.nglyph = 1; y.cm[0] = 0; y.adv[0] = x.adv[g]; y.off[0] = x.off[g]; y.gp[0].isClusterStart = 1;
        CHECK_HR(apply(&c_, 2, 3, 12, &y, y.adv, y.off, one_adv, one_off), S_OK);
        if (one_adv[0] != all_adv[g] || one_off[0].advanceOffset != all_off[g].advanceOffset || one_off[0].ascenderOffset != all_off[g].ascenderOffset) bad++;
    }
    CHECKF(bad == 0, "%u clusters differ from their stand-alone result", bad);
    R2_END();
}
RT(sc_inputs_are_not_modified_when_outputs_are_separate) {
    R2_BEGIN(); fix x, y; float oadv[8]; DWRITE_GLYPH_OFFSET ooff[8];
    fix_b(&x); y = x;
    CHECK_HR(apply(&c_, 4, 5, 50, &x, x.adv, x.off, oadv, ooff), S_OK);
    CHECK(!memcmp(&x, &y, sizeof x));
    R2_END();
}
RT(sc_output_buffers_may_hold_anything_beforehand) {
    R2_BEGIN(); fix x; float a1_[8], a2_[8]; DWRITE_GLYPH_OFFSET o1[8], o2[8];
    fix_b(&x);
    memset(a1_, 0, sizeof a1_); memset(o1, 0, sizeof o1); memset(a2_, 0xFF, sizeof a2_); memset(o2, 0xFF, sizeof o2);
    CHECK_HR(apply(&c_, 1, 2, 3, &x, x.adv, x.off, a1_, o1), S_OK); CHECK_HR(apply(&c_, 1, 2, 3, &x, x.adv, x.off, a2_, o2), S_OK);
    CHECK(same_run(&x, a1_, o1, a2_, o2));
    R2_END();
}
RT(sc_ascender_offsets_are_never_touched) {
    R2_BEGIN(); fix x; float oadv[8]; DWRITE_GLYPH_OFFSET ooff[8]; UINT32 i;
    fix_a(&x);
    CHECK_HR(apply(&c_, 3, -2, 25, &x, x.adv, x.off, oadv, ooff), S_OK);
    for (i = 0; i < x.nglyph; i++) EXPECT_EQ((int)ooff[i].ascenderOffset, 7);
    R2_END();
}
