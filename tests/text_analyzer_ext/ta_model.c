/* Reference models of the arithmetic of IDWriteTextAnalyzer1::JustifyGlyphAdvances (and the ApplyCharacterSpacing wrapper), written
 * from the Windows 10 implementation.  They are used by the model driven tests and, when the area is built with -DTA_USE_MODEL,
 * can stand in for the DLL to check the hand computed expectations of the other tests against the model. */
#include "ta.h"

static float maxadj(const DWRITE_JUSTIFICATION_OPPORTUNITY *o, int expand) {
    float m = expand ? o->expansionMaximum : o->compressionMaximum, f = 0.0f;
    if (o->applyToLeadingEdge) f = m + 0.0f;
    if (o->applyToTrailingEdge) f = f + m;
    return f;
}
static float resid(const DWRITE_JUSTIFICATION_OPPORTUNITY *o, float v) {
    float f = 0.0f;
    if (o->applyToLeadingEdge) f = v + 0.0f;
    if (o->applyToTrailingEdge) f = f + v;
    return f;
}
#define EPS23 1.1920928955078125e-07f

/* adj[i]: signed advance change; adj must be zeroed by the caller. */
void ta_model_justify_adjustments(float lw, UINT32 n, const DWRITE_JUSTIFICATION_OPPORTUNITY *op, const float *adv, float *adj) {
    float sum = 0.0f, E; UINT32 i, k, maxp = 0, edges = 0; int expand;
    UINT32 cnt[256], nb[256]; float S[256];
    for (i = 0; i < n; i++) sum += adv[i];
    E = lw - sum;
    if (n == 0 || E == 0.0f) return;
    if (E <= 0.0f) expand = 0;
    else { if (E < (sum / (float)n) * EPS23) return; expand = 1; }
    memset(cnt, 0, sizeof cnt); memset(nb, 0, sizeof nb); for (k = 0; k < 256; k++) S[k] = 0.0f;
    for (i = 0; i < n; i++) {
        UINT32 pri = expand ? op[i].expansionPriority : op[i].compressionPriority; float m = maxadj(&op[i], expand);
        if (expand ? op[i].allowResidualExpansion : op[i].allowResidualCompression) { if (op[i].applyToLeadingEdge) edges++; if (op[i].applyToTrailingEdge) edges++; }
        if (0.0f < m) {
            float f19 = m;
            cnt[pri]++; if (pri > maxp) maxp = pri;
            if (!expand && adv[i] <= m && m != adv[i]) f19 = adv[i];
            if (m < FLT_MAX) { nb[pri]++; S[pri] = f19 + S[pri]; }
        }
    }
    if (expand) {
        for (k = 1; k <= maxp; k++) {
            if (E <= EPS23) return;
            if (cnt[k]) {
                float unb = (float)(cnt[k] - nb[k]), share = E / (float)cnt[k], total = unb * share + S[k];
                if (0.0f < total) {
                    float ratio = E / total; int fit = total < E;
                    if (fit && cnt[k] - nb[k] != 0) share = (E - S[k]) / unb;
                    for (i = 0; i < n; i++) {
                        float m;
                        if (op[i].expansionPriority == k && (m = maxadj(&op[i], 1)) > 0.0f) {
                            float x = m;
                            if (FLT_MAX <= x) x = share;
                            if (!fit) x = x * ratio;
                            if (x < op[i].expansionMinimum) x = 0.0f;
                            E = E - x; adj[i] = adj[i] + x;
                        }
                    }
                }
            }
        }
        if (EPS23 < E && edges != 0) {
            for (i = 0; i < n; i++) if (op[i].allowResidualExpansion) adj[i] = adj[i] + resid(&op[i], E / (float)edges);
        }
    } else {
        const float thr = -EPS23;
        for (k = 1; k <= maxp; k++) {
            if (thr <= E) break;
            if (cnt[k] && S[k] > 0.0f) {
                float s = S[k], ratio = -(E / s); int fit = !(-s > E);
                for (i = 0; i < n; i++) {
                    float m;
                    if (0.0f <= E) break;
                    if (op[i].compressionPriority == k && (m = maxadj(&op[i], 0)) > 0.0f) {
                        float x = adv[i] < m ? adv[i] : m;
                        if (fit) x = x * ratio;
                        E = E + x; adj[i] = adj[i] - x;
                    }
                }
            }
        }
        if (edges != 0 && E < thr) {
            for (i = 0; i < n; i++) if (op[i].allowResidualCompression) {
                float x = resid(&op[i], -(E / (float)edges));
                if (adv[i] <= x) x = adv[i];
                adj[i] = adj[i] - x;
            }
        }
    }
}

HRESULT ta_model_justify(float lw, UINT32 n, const DWRITE_JUSTIFICATION_OPPORTUNITY *op, const float *adv, const DWRITE_GLYPH_OFFSET *off, float *oadv, DWRITE_GLYPH_OFFSET *ooff) {
    float adj[1024]; UINT32 i, j;
    if (!(0.0f <= lw)) return E_INVALIDARG;
    if (n == 0) return S_OK;
    if (!adv || !off || !op) return E_INVALIDARG;
    memset(adj, 0, sizeof adj);
    ta_model_justify_adjustments(lw, n, op, adv, adj);
    if (oadv != adv) for (i = 0; i < n; i++) oadv[i] = adv[i];
    if (ooff != off) for (i = 0; i < n; i++) ooff[i] = off[i];
    for (i = 0; i < n; i++) {
        float x = adj[i];
        if (x == 0.0f) continue;
        oadv[i] = x + oadv[i];
        if (op[i].applyToLeadingEdge) {
            float l = x;
            if (op[i].applyToTrailingEdge) l = x * 0.5f;
            x = x - l; ooff[i].advanceOffset = l + ooff[i].advanceOffset;
        }
        for (j = i + 1; j < n; j++) { if (oadv[j] != 0.0f || adj[j] != 0.0f) break; ooff[j].advanceOffset -= x; }
    }
    return S_OK;
}

HRESULT ta_model_spacing_call(float L, float T, float M, UINT32 n, UINT32 g, const UINT16 *cm, const float *adv, const DWRITE_GLYPH_OFFSET *off,
                              const DWRITE_SHAPING_GLYPH_PROPERTIES *gp, float *oadv, DWRITE_GLYPH_OFFSET *ooff) {
    UINT32 i;
    if (adv != oadv || !oadv) for (i = 0; i < g; i++) oadv[i] = 0.0f;
    if (off != (const DWRITE_GLYPH_OFFSET *)ooff || !ooff) for (i = 0; i < g; i++) { ooff[i].advanceOffset = 0.0f; ooff[i].ascenderOffset = 0.0f; }
    if (L != L || T != T || !(0.0f <= M)) return E_INVALIDARG;
    if (g == 0) return S_OK;
    if (!adv || !off || !gp || !cm || n == 0) return E_INVALIDARG;
    ta_model_spacing(L, T, M, n, g, cm, adv, off, gp, oadv, ooff);
    return S_OK;
}
