/* Shared fixtures for the text_hit_testing area (IDWriteTextLayout::HitTestPoint / HitTestTextPosition / HitTestTextRange).
 *
 * Everything is reached through DWriteCreateFactory (common/dw.c) and COM vtables. Deterministic text comes from a private font
 * collection with two synthetic families built on the default synthetic font (UPM 2048, every letter/digit/space advance 1000 units):
 *   "DwHit"     default glyphs + cmap for Hebrew U+05D0..05EA, Arabic U+0627/U+0628, U+0301 (zero-advance mark), U+2026, U+10000..U+1000F
 *   "DwHitLig"  same, plus a GSUB 'liga' lookup  f + i -> glyph 64
 * At font size 16 one character is exactly 7.8125 DIPs wide (1000 * 16 / 2048); sums of such widths are exact in binary floating point.
 * Line heights are NOT assumed from the font tables: tests take them from GetLineMetrics of the layout under test.
 */
#ifndef TH_H
#define TH_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define TH_FAM  L"DwHit"
#define TH_LIG  L"DwHitLig"
#define TH_LOC  L"en-us"
#define CW      7.8125f           /* advance of one letter/digit/space at size 16 */
#define EPS     0.002f

typedef struct th_fx {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; UINT32 key[2];
} th_fx;
int  th_open(th_fx *x);
void th_close(th_fx *x);
#define TH_BEGIN() th_fx X; memset(&X, 0, sizeof X); if (!th_open(&X)) { th_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define TH_END()   th_close(&X)

IDWriteTextFormat *th_format(th_fx *x, const WCHAR *family, float size);
IDWriteTextLayout *th_layout_(th_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h);
/* Layout over "DwHit" at size 16; skips the test (releasing the fixture) on failure. */
#define TH_LAYOUT(var, text, w, h) IDWriteTextLayout *var = th_layout_(&X, TH_FAM, text, 16.0f, w, h); if (!var) { TH_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
#define TH_LAYOUT_F(var, fam, text, w, h) IDWriteTextLayout *var = th_layout_(&X, fam, text, 16.0f, w, h); if (!var) { TH_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
#define TH_DONE(...) do { IDWriteTextLayout *a_[] = { __VA_ARGS__ }; unsigned i_; for (i_ = 0; i_ < sizeof a_ / sizeof a_[0]; i_++) if (a_[i_]) IDWriteTextLayout_Release(a_[i_]); TH_END(); } while (0)

/* ---- call wrappers: poison the outputs, require S_OK, require the outputs were written ---------------------- */
typedef struct { HRESULT hr; BOOL tr, in; DWRITE_HIT_TEST_METRICS m; } th_pt;
typedef struct { HRESULT hr; float x, y; DWRITE_HIT_TEST_METRICS m; } th_tp;
#define TH_MAXR 32
typedef struct { HRESULT hr; UINT32 count; DWRITE_HIT_TEST_METRICS m[TH_MAXR]; } th_rg;
th_pt th_point(IDWriteTextLayout *l, float x, float y);
th_tp th_pos(IDWriteTextLayout *l, UINT32 pos, BOOL trailing);
th_rg th_range(IDWriteTextLayout *l, UINT32 pos, UINT32 len, float ox, float oy);

/* ---- layout queries ----------------------------------------------------------------------------------------- */
void   th_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m);
UINT32 th_lines(IDWriteTextLayout *l);
float  th_lh(IDWriteTextLayout *l, UINT32 line);                 /* GetLineMetrics height of a line */
UINT32 th_clusters(IDWriteTextLayout *l, DWRITE_CLUSTER_METRICS *c, UINT32 cap);   /* count (0 on failure) */

float th_cw_sum(IDWriteTextLayout *l, UINT32 pos, UINT32 len);   /* sum of GetClusterMetrics widths of clusters starting in [pos,pos+len) */
static __inline int th_feq(float a, float b) { return fabsf(a - b) <= EPS; }
#define CHK_NEAR(a, b) t_check(th_feq((float)(a), (float)(b)), __FILE__, __LINE__, #a " ~= " #b, "got %g want %g", (double)(a), (double)(b))
void th_expect_pt(const th_pt *p, UINT32 pos, UINT32 len, int trailing, int inside, const char *file, int line);
#define CHK_PT(p, pos, len, tr, in) th_expect_pt(&(p), (pos), (len), (tr), (in), __FILE__, __LINE__)
void th_expect_m(const DWRITE_HIT_TEST_METRICS *m, UINT32 pos, UINT32 len, float left, float width, UINT32 bidi, int isText, const char *file, int line);
#define CHK_M(m, pos, len, left, width, bidi, isText) th_expect_m(&(m), (pos), (len), (left), (width), (bidi), (isText), __FILE__, __LINE__)

#define HEB1 L"\x05d0"
#define HEB2 L"\x05d0\x05d1"
#define HEB3 L"\x05d0\x05d1\x05d2"
#define HEB4 L"\x05d0\x05d1\x05d2\x05d3"
#define SENT_TL ((IDWriteTextLayout *)(ULONG_PTR)0x1)
#endif
