/* Shared fixtures for the text_layout_core area (IDWriteTextLayout .. IDWriteTextLayout4 creation and layout-wide properties).
 *
 * Every object is reached through DWriteCreateFactory (LoadLibrary/GetProcAddress in common/dw.c) and the COM vtables of what it returns.
 * Deterministic text: a private font collection holds two synthetic families
 *   "DwTest"   every letter/digit advance 1000 design units, space 1000, notdef 1200, UPM 2048, ascent 1800, descent 450, lineGap 0
 *   "DwNarrow" same outlines, letter/digit advance 500, space 250
 * so at font size 2048 a character is exactly 1000 (or 500) DIPs wide and a line is exactly 2250 DIPs high.
 * At size 16 one DwTest character is 7.8125 DIPs and a line 17.578125 DIPs (all exactly representable in binary floating point).
 */
#ifndef TL_H
#define TL_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define TL_FAM     L"DwTest"
#define TL_NARROW  L"DwNarrow"
#define TL_LOC     L"en-us"
#define TL_UPM     2048.0f
#define TL_ASC     1800.0f
#define TL_DESC    450.0f

typedef struct tl_fx {
    IDWriteFactory *f;
    dwx_mem_loader *fl;
    dwx_coll_loader *cl;
    IDWriteFontCollection *coll;
    UINT32 key[2];
} tl_fx;
int  tl_open(tl_fx *x);      /* 1 on success */
void tl_close(tl_fx *x);
#define TL_BEGIN() tl_fx X; memset(&X, 0, sizeof X); if (!tl_open(&X)) { tl_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define TL_END()   tl_close(&X)

/* Format over the fixture collection (weight normal, style normal, stretch normal, en-us). NULL on failure. */
IDWriteTextFormat *tl_format(tl_fx *x, const WCHAR *family, float size);
/* Layout of `text` (wcslen) over a DwTest format of `size`. Skips the test (and releases the fixture) on failure. */
IDWriteTextLayout *tl_layout_(tl_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h);
#define TL_LAYOUT(var, text, size, w, h) IDWriteTextLayout *var = tl_layout_(&X, TL_FAM, text, size, w, h); if (!var) { TL_END(); t_skip("CreateTextLayout prerequisite failed"); return; }

/* Metrics helper: GetMetrics with a poisoned struct, requiring S_OK. */
void tl_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m);
UINT32 tl_lines(IDWriteTextLayout *l);

/* QueryInterface that skips the test when the interface is missing; caller releases. */
void *tl_qi(IUnknown *u, const IID *iid, const char *name);
#define TL_QI(var, TYPE, obj) TYPE *var = (TYPE *)tl_qi((IUnknown *)(obj), &IID_##TYPE, #TYPE); if (!var) return

static __inline int tl_fbits(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }
#define CHECK_FBITS(a, b) t_check(tl_fbits((float)(a), (float)(b)), __FILE__, __LINE__, #a " bit-identical to " #b, "got %g want %g", (double)(a), (double)(b))
#define CHECK_WEQ(a, b) do { const WCHAR *a_ = (a), *b_ = (b); t_check(!wcscmp(a_, b_), __FILE__, __LINE__, #a " == " #b, "wide string mismatch: got \"%ls\" want \"%ls\"", a_, b_); } while (0)
#define RNG(s, l) ((DWRITE_TEXT_RANGE){ (s), (l) })
#define SENT_LAYOUT ((IDWriteTextLayout *)(ULONG_PTR)0x1)

/* Snapshot of everything the layout-wide (IDWriteTextFormat style) getters report, to prove a failing call changed nothing. */
typedef struct tl_state {
    DWRITE_TEXT_ALIGNMENT ta; DWRITE_PARAGRAPH_ALIGNMENT pa; DWRITE_WORD_WRAPPING ww; DWRITE_READING_DIRECTION rd; DWRITE_FLOW_DIRECTION fd;
    float tab; DWRITE_TRIMMING trim; IDWriteInlineObject *sign;
    DWRITE_LINE_SPACING_METHOD lsm; float ls, bl; float maxw, maxh;
} tl_state;
void tl_snap(IDWriteTextLayout *l, tl_state *o);
int  tl_state_eq(const tl_state *a, const tl_state *b);
#define CHECK_STATE_SAME(l, snap) do { tl_state now_; tl_snap((l), &now_); t_check(tl_state_eq(&now_, &(snap)), __FILE__, __LINE__, "layout state unchanged", "a failing call modified the layout"); } while (0)

/* Minimal client IDWriteFontFallback counting reference traffic. */
typedef struct tl_fb { const IDWriteFontFallbackVtbl *lpVtbl; volatile LONG ref; LONG addref, release, qi, map; } tl_fb;
void tl_fb_init(tl_fb *o);
#endif
