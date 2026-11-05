/* Shared fixtures for the text_layout_wrap_trim area: word wrapping, trimming, tabs and alignment of IDWriteTextLayout.
 *
 * Everything is reached through DWriteCreateFactory (common/dw.c) and COM vtables. Deterministic text comes from a private
 * collection holding one synthetic family "DwWrap" (UPM 2048): every letter, digit, space and mapped punctuation/CJK/Hebrew code
 * point has advance 1000 design units, notdef 1200; tab, CR, LF, ZWSP, ZWNJ, ZWJ and word joiner map to a zero-advance glyph.
 * At font size 64 one character is exactly 31.25 DIPs wide (representable in binary floating point), so a layout width of
 * k * WT_CW holds exactly k characters; WT_FIT(k) = (k + 0.5) characters is used where the boundary itself is not under test.
 * Ascent 1800 + descent 450 (usWin metrics) give a line height of 70.3125 at size 64.
 */
#ifndef WT_H
#define WT_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define WT_FAM   L"DwWrap"
#define WT_LOC   L"en-us"
#define WT_SIZE  64.0f
#define WT_CW    31.25f                         /* width of one mapped character at WT_SIZE */
#define WT_LH    70.3125f                       /* line height at WT_SIZE (win ascent + descent) */
#define WT_FIT(k) (((float)(k) + 0.5f) * WT_CW) /* a width that holds k characters and not k + 1 */
#define WT_CH(k)  ((float)(k) * WT_CW)          /* a width that holds exactly k characters */
#define WT_BIGH   100000.0f

/* Non-ASCII text is spelled with string literal macros (adjacent wide literals concatenate): L"aa" ZWSP L"bb". */
#define NBSP    L"\x00a0"
#define ZWSP    L"\x200b"
#define ZWNJ    L"\x200c"
#define WJ      L"\x2060"
#define ELLIPSIS L"\x2026"
#define CJ0 L"\x4e00"
#define CJ1 L"\x4e01"
#define CJ2 L"\x4e02"
#define CJ3 L"\x4e03"
#define CJ4 L"\x4e04"
#define CJ5 L"\x4e05"
#define CJ6 L"\x4e06"
#define CJ7 L"\x4e07"
#define KFULL  L"\x3002"      /* ideographic full stop (CL) */
#define KCOMMA L"\x3001"      /* ideographic comma (CL) */
#define KOPEN  L"\x300c"      /* corner bracket (OP) */
#define KCLOSE L"\x300d"      /* corner bracket (CL) */
#define HIRA_A L"\x3042"
#define HIRA_I L"\x3044"
#define KATA_A L"\x30a2"
#define HE0 L"\x05d0"         /* Hebrew letters (right to left) */
#define HE1 L"\x05d1"
#define HE2 L"\x05d2"
#define HE3 L"\x05d3"
#define HE4 L"\x05d4"
#define HE5 L"\x05d5"

typedef struct wt_fx {
    IDWriteFactory *f;
    dwx_mem_loader *fl;
    dwx_coll_loader *cl;
    IDWriteFontCollection *coll;
    UINT32 key;
} wt_fx;
int  wt_open(wt_fx *x);
void wt_close(wt_fx *x);
#define WT_BEGIN() wt_fx X; memset(&X, 0, sizeof X); if (!wt_open(&X)) { wt_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define WT_END()   wt_close(&X)

IDWriteTextFormat *wt_format(wt_fx *x, float size);
/* Layout of text (wcslen) with the given width in DIPs; height WT_BIGH unless set otherwise. NULL on failure. */
IDWriteTextLayout *wt_layout_(wt_fx *x, const WCHAR *text, float width, float height, float size);
#define WT_LAYOUT(var, text, width) IDWriteTextLayout *var = wt_layout_(&X, (text), (width), WT_BIGH, WT_SIZE); if (!var) { WT_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
#define WT_LAYOUT_H(var, text, width, h) IDWriteTextLayout *var = wt_layout_(&X, (text), (width), (h), WT_SIZE); if (!var) { WT_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
/* Same, with a word wrapping mode applied. */
IDWriteTextLayout *wt_layout_wrap(wt_fx *x, const WCHAR *text, float width, DWRITE_WORD_WRAPPING w);
#define WT_LAYOUT_W(var, text, width, wrap) IDWriteTextLayout *var = wt_layout_wrap(&X, (text), (width), (wrap)); if (!var) { WT_END(); t_skip("layout prerequisite failed"); return; }

typedef struct wt_lines { UINT32 n; DWRITE_LINE_METRICS lm[128]; UINT32 start[128]; } wt_lines;
HRESULT wt_get_lines(IDWriteTextLayout *l, wt_lines *out);          /* two-call protocol, fills start[] */
UINT32 wt_nlines(IDWriteTextLayout *l);
void wt_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m);       /* GetMetrics requiring S_OK */
/* Compare the line lengths of the layout against the expectation; reports the actual lengths on mismatch. */
void wt_check_lens_(IDWriteTextLayout *l, const UINT32 *want, int n, const char *file, int line);
#define WT_LENS(l, ...) do { UINT32 e_[] = { __VA_ARGS__ }; wt_check_lens_((l), e_, (int)(sizeof e_ / sizeof e_[0]), __FILE__, __LINE__); } while (0)
/* Cluster metrics of the whole text. Returns the count (0 on failure). */
UINT32 wt_clusters(IDWriteTextLayout *l, DWRITE_CLUSTER_METRICS *cm, UINT32 cap);
/* x of the leading edge of text position pos (HitTestTextPosition trailing = FALSE). */
float wt_x(IDWriteTextLayout *l, UINT32 pos);
float wt_x_trailing(IDWriteTextLayout *l, UINT32 pos);               /* trailing edge of the character at pos */
float wt_y(IDWriteTextLayout *l, UINT32 pos);
/* Next glyph run that is not made only of white space / line break characters (Win10 emits separate runs for blanks and newlines). */
const dwx_draw *wt_next_run(dwx_renderer *r, int *cur);
/* Draw into a fresh recording renderer (caller releases with dwx_release). */
dwx_renderer *wt_draw(IDWriteTextLayout *l, float ox, float oy);
/* Legal line break between text positions p-1 and p according to IDWriteTextAnalyzer (1 yes, 0 no, -1 analyzer unavailable). */
typedef struct wt_brk { int ok; UINT32 len; DWRITE_LINE_BREAKPOINT bp[512]; } wt_brk;
void wt_analyze(IDWriteFactory *f, const WCHAR *text, UINT32 len, wt_brk *b);
int  wt_can_break(const wt_brk *b, UINT32 p);

/* QI that skips the test when missing; caller releases. */
void *wt_qi(IUnknown *u, const IID *iid, const char *name);
#define WT_QI(var, TYPE, obj) TYPE *var = (TYPE *)wt_qi((IUnknown *)(obj), &IID_##TYPE, #TYPE); if (!var) { WT_END(); return; }
#define RNG(s, l) ((DWRITE_TEXT_RANGE){ (s), (l) })

/* Validate a complete wrapping result against the break opportunities of the analyzer:
 * lengths sum to the text length, every internal break is a legal break position (unless emergency is allowed),
 * every line except one holding a single unbreakable segment fits the width, and no line could take more of the next
 * segment (greedy). mode: 0 check legality + fit + greedy (word-wrapped), 1 legality only. */
void wt_check_greedy_(wt_fx *x, IDWriteTextLayout *l, const WCHAR *text, float width, int check_fit, const char *file, int line);
#define WT_CHECK_GREEDY(l, text, width) wt_check_greedy_(&X, (l), (text), (width), 1, __FILE__, __LINE__)

/* Test doubles */
dwx_inline *wt_sign(float w, float h, float base);
#endif
