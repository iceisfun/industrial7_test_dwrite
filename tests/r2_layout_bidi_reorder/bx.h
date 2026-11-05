/* Shared fixtures for the r2_layout_bidi_reorder area: IDWriteTextLayout with Hebrew / Arabic / mixed text on a deterministic
 * synthetic font, observed through GetClusterMetrics, the hit-test methods and a recording IDWriteTextRenderer.
 *
 * Private font collection (all through DWriteCreateFactory + COM vtables):
 *   "DwBidi"      the default synthetic font; every mapped base character (Latin, digits, Hebrew U+05D0.., Arabic letters,
 *                 Arabic-Indic digits, CJK sample, supplementary samples, punctuation) has advance 1000 units; combining marks,
 *                 ZWJ/ZWNJ/LRM/RLM have a zero-advance empty glyph. UPM 2048.
 *   "DwBidiKern"  same glyphs plus a legacy 'kern' table: (A,A) -64, (A,B) -128, (A,V) -256, (V,A) -192 design units;
 *                 Hebrew aleph/bet map to the same glyphs as A/B, so U+05D0 U+05D1 is the pair (A,B).
 * At font size 16 one character is exactly 7.8125 DIPs (CW); kern adjustments are -0.5, -1.0, -2.0, -1.5 DIPs.
 */
#ifndef BX_H
#define BX_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define BX_FAM   L"DwBidi"
#define BX_KERN  L"DwBidiKern"
#define BX_LOC   L"en-us"
#define CW       7.8125f
#define EPS      0.002f
#define BX_W     200.0f      /* default layout width: wide enough for every one-line fixture */

typedef struct bx_fx {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; UINT32 key[2];
} bx_fx;
int  bx_open(bx_fx *x);
void bx_close(bx_fx *x);
#define BX_BEGIN() bx_fx X; memset(&X, 0, sizeof X); if (!bx_open(&X)) { bx_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define BX_END()   bx_close(&X)

IDWriteTextFormat *bx_format(bx_fx *x, const WCHAR *family, float size);
IDWriteTextLayout *bx_layout_(bx_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h);
/* layout over DwBidi at size 16; on failure releases the fixture and skips */
#define BX_LAYOUT(var, text, w, h) IDWriteTextLayout *var = bx_layout_(&X, BX_FAM, text, 16.0f, w, h); if (!var) { BX_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
#define BX_LAYOUT_F(var, fam, text, w, h) IDWriteTextLayout *var = bx_layout_(&X, fam, text, 16.0f, w, h); if (!var) { BX_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
#define BX_DONE(...) do { IDWriteTextLayout *a_[] = { __VA_ARGS__ }; unsigned i_; for (i_ = 0; i_ < sizeof a_ / sizeof a_[0]; i_++) if (a_[i_]) IDWriteTextLayout_Release(a_[i_]); BX_END(); } while (0)
/* QI to IDWriteTextLayout<N>; skip the test (releasing everything) when unsupported */
#define BX_QI(var, TYPE, l) TYPE *var = NULL; if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_##TYPE, (void **)&var)) || !var) { BX_DONE(l); t_skip(#TYPE " not supported"); return; }

typedef struct { HRESULT hr; BOOL tr, in; DWRITE_HIT_TEST_METRICS m; } bx_pt;
typedef struct { HRESULT hr; float x, y; DWRITE_HIT_TEST_METRICS m; } bx_tp;
#define BX_MAXR 32
typedef struct { HRESULT hr; UINT32 count; DWRITE_HIT_TEST_METRICS m[BX_MAXR]; } bx_rg;
bx_pt bx_point(IDWriteTextLayout *l, float x, float y);
bx_tp bx_pos(IDWriteTextLayout *l, UINT32 pos, BOOL trailing);
bx_rg bx_range(IDWriteTextLayout *l, UINT32 pos, UINT32 len);

void   bx_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m);
UINT32 bx_clusters(IDWriteTextLayout *l, DWRITE_CLUSTER_METRICS *c, UINT32 cap);   /* count, 0 on failure */
UINT32 bx_lines(IDWriteTextLayout *l, DWRITE_LINE_METRICS *lm, UINT32 cap);

/* Draw the layout into a fresh recording renderer at origin (ox, oy); the caller releases it with dwx_release. NULL on failure. */
dwx_renderer *bx_draw(IDWriteTextLayout *l, float ox, float oy);
/* Per-character resolved bidi level as seen by DrawGlyphRun descriptions: lev[i] = bidiLevel of the run that covers text position i,
 * 0xff when no run covers it. Returns 1 when every position of [0,len) is covered exactly once. */
int bx_draw_levels(dwx_renderer *r, UINT32 len, UINT8 *lev);

/* Reference Unicode bidi algorithm output for the fixture alphabet (see bidi_ref.c): fills lev[0..n) with the resolved
 * levels (rules W1-W7, N1-N2, I1-I2; no explicit embeddings except LRM/RLM as strong types) and vis[0..n) with the visual
 * order (vis[k] = logical index shown at visual slot k, rule L2). base = 0 or 1. */
void bx_ref_bidi(const WCHAR *t, UINT32 n, int base, UINT8 *lev, UINT32 *vis);

static __inline int bx_feq(float a, float b) { return fabsf(a - b) <= EPS; }
#define CHK_NEAR(a, b) t_check(bx_feq((float)(a), (float)(b)), __FILE__, __LINE__, #a " ~= " #b, "got %g want %g", (double)(a), (double)(b))
#define RNG(s, l) ((DWRITE_TEXT_RANGE){ (s), (l) })
#define CHECK_RANGE(r, s, l) t_check((r).startPosition == (UINT32)(s) && (r).length == (UINT32)(l), __FILE__, __LINE__, #r " == (" #s "," #l ")", "got (%u,%u)", (r).startPosition, (r).length)
/* A range that runs to the end of the property's last run: the run may be reported as extending to 0xFFFFFFFF (an unset tail is
 * not clipped to the text length) or clipped to the text length; the start must be exact. The choice is recorded with t_note. */
#define CHECK_RANGE_TAIL(r, s, tl) do { \
    t_check((r).startPosition == (UINT32)(s) && ((r).length == (UINT32)((tl) - (s)) || (r).length == (UINT32)(0xFFFFFFFFu - (UINT32)(s))), \
            __FILE__, __LINE__, #r " tail range from " #s, "got (%u,%u)", (r).startPosition, (r).length); \
    t_note("tail range length %s", (r).length == (UINT32)((tl) - (s)) ? "clipped to text length" : "extends to 0xFFFFFFFF"); } while (0)
#endif
