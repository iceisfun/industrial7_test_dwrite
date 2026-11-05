/* Shared fixtures for the text_layout_draw area: IDWriteTextLayout::Draw and the IDWriteTextRenderer callbacks it makes.
 *
 * What Draw does on Windows 10 (TextLayout::Draw and its DrawGlyphRuns / DrawInlineObjects / DrawUnderlines / DrawStrikethroughs helpers):
 *   - a NULL renderer is rejected with E_INVALIDARG before anything else happens; the layout is (re)built first (DoLayout);
 *   - the renderer is asked for IDWriteTextRenderer1 (QueryInterface); a vertical layout and a renderer without it fail with DWRITE_E_TEXTRENDERERINCOMPATIBLE;
 *   - IsPixelSnappingDisabled is called once; only when it answers FALSE are GetPixelsPerDip and GetCurrentTransform called (once each);
 *     a failing snapping callback aborts Draw with that HRESULT (CallbackException::ThrowIfFailed);
 *   - then, over the WHOLE layout and in this order: all glyph runs, all inline objects, all underlines, all strikethroughs.
 *     The HRESULTs returned by those four callbacks are never inspected: a failing DrawGlyphRun does not stop the Draw.
 *
 * Deterministic text: a private collection holds
 *   "DwDraw" regular  every letter/digit advance 1000, space 1000, notdef 1200, UPM 2048 (plus Greek alpha..gamma, Hebrew alef..gimel, CJK, ellipsis)
 *   "DwDraw" bold     letters 1200, space 600
 *   "DwTall"          same outlines as regular, much taller ascent/descent metrics
 * At font size 16 one regular letter is exactly 7.8125 DIPs, a bold one 9.375.
 */
#ifndef TD_H
#define TD_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define TD_FAM    L"DwDraw"
#define TD_TALL   L"DwTall"
#define TD_SHAPE  L"DwShape"   /* GSUB liga: A B -> glyph 68; GPOS kern: C D -> -200 units */
#define TD_LOC    L"en-us"
#define TD_UPM    2048.0f
#define TD_ADV(units, size)  ((float)(units) * (float)(size) / 2048.0f)

#ifndef DWRITE_E_TEXTRENDERERINCOMPATIBLE
#define DWRITE_E_TEXTRENDERERINCOMPATIBLE ((HRESULT)0x8898500A)
#endif
#define GREEK_A  L"α"
#define HEB      L"אבג"        /* alef bet gimel -> glyphs 2,3,4 */
#define CJK      L"一丁丂"        /* -> glyphs 2,3,4 */

typedef struct td_fx {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; UINT32 key[4];
} td_fx;
int  td_open(td_fx *x);
void td_close(td_fx *x);
#define TD_BEGIN() td_fx X; memset(&X, 0, sizeof X); if (!td_open(&X)) { td_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define TD_END()   td_close(&X)

IDWriteTextFormat *td_format(td_fx *x, const WCHAR *family, float size);
IDWriteTextLayout *td_layout_(td_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h);
#define TD_LAYOUT(var, text, size, w, h) IDWriteTextLayout *var = td_layout_(&X, TD_FAM, text, size, w, h); if (!var) { TD_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
/* A renderer double + draw; hr returned. */
HRESULT td_draw(IDWriteTextLayout *l, dwx_renderer *r, void *ctx, float x, float y);
#define RNG(s, l) ((DWRITE_TEXT_RANGE){ (s), (l) })
#define CTX_A ((void *)(ULONG_PTR)0x1234abcd)

/* Draw once with a fresh default renderer (snapping callbacks on, ppd 1) and ask for S_OK. */
#define DRAW_OK(l, r, ox, oy) CHECK_HR_OK(td_draw((l), (r), CTX_A, (ox), (oy)))

/* Iteration helpers over recorded calls. */
int  td_count(dwx_renderer *r, int kind);
const dwx_draw *td_nth(dwx_renderer *r, int kind, int n);          /* n-th call of that kind, NULL if absent */
/* Font metrics of the face the run was drawn with. */
void td_face_metrics(const dwx_draw *d, DWRITE_FONT_METRICS *m);
/* Line baseline / height from the layout's own line metrics. */
float td_line_baseline(IDWriteTextLayout *l, UINT32 line);
float td_line_height(IDWriteTextLayout *l, UINT32 line);
/* Number of lines. */
UINT32 td_lines(IDWriteTextLayout *l);
/* Advance of 'A' in design units according to the run's face (1000 regular / 1200 bold). */
int td_face_a_advance(const dwx_draw *d);
/* Sum of glyphCount over glyph-run calls. */
UINT32 td_total_glyphs(dwx_renderer *r);

static __inline int td_fbits(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }
#define CHECK_FBITS(a, b) t_check(td_fbits((float)(a), (float)(b)), __FILE__, __LINE__, #a " bit-identical to " #b, "got %g want %g", (double)(a), (double)(b))
#define CHECK_WEQ(a, b) do { const WCHAR *a_ = (a), *b_ = (b); t_check(a_ && b_ && !wcscmp(a_, b_), __FILE__, __LINE__, #a " == " #b, "wide string mismatch: got \"%ls\" want \"%ls\"", a_ ? a_ : L"(null)", b_ ? b_ : L"(null)"); } while (0)
#define CHECK_NEARF(a, b) CHECK_NEAR((a), (b), 0.0001)

/* Layout line helpers. */
typedef struct td_lines_t { DWRITE_LINE_METRICS lm[32]; UINT32 n; float top[33]; } td_lines_t;
void td_get_lines(IDWriteTextLayout *l, td_lines_t *o);          /* top[i] = sum of heights of the lines before i */
int  td_line_of(const td_lines_t *o, UINT32 textpos);            /* line containing the text position, -1 if none */
/* Expected snapped value of a baseline coordinate: round half up on the device grid (scale = ppd * m22, offset = dy). */
float td_snap(float v, float scale, float offset);
/* Glyph run starting exactly at text position `pos` (nth of that kind with the position), NULL if none. */
const dwx_draw *td_run_at(dwx_renderer *r, UINT32 pos);
/* Baseline y (relative to the draw origin) must be the face ascent scaled to `size`, plus at most the line gap (how a gap is distributed is a font-metrics matter). */
void td_check_baseline_(float y, const DWRITE_FONT_METRICS *m, float size, const char *file, int line);
#define CHECK_BASELINE(y, m, size) td_check_baseline_((y), &(m), (size), __FILE__, __LINE__)
/* Check the run's glyph indices against IDWriteFontFace::GetGlyphIndices of the run's own face (LTR order). */
void td_check_indices(const dwx_draw *d, const WCHAR *src, const char *what);

/* ---- order-recording renderer ---------------------------------------------------------------------------
 * Wraps a dwx_renderer (which records the arguments) and additionally logs every callback in order, including
 * QueryInterface requests and the pixel snapping callbacks. */
enum { TE_QI = 1, TE_SNAP, TE_XFORM, TE_PPD, TE_GLYPH, TE_UNDER, TE_STRIKE, TE_INLINE };
typedef struct td_rd {
    const void *lpVtbl;
    LONG ref, addref, release;
    dwx_renderer *in;                     /* receives the forwarded callbacks */
    struct { int type; IID iid; HRESULT hr; void *ctx; } log[2048]; int nlog;
    int allow_pixel_snapping_qi;          /* QI(IDWritePixelSnapping) succeeds (default 1) */
    int allow_r1;                         /* QI(IDWriteTextRenderer1) succeeds (default 0) */
    int allow_renderer;                   /* QI(IDWriteTextRenderer) succeeds (default 1) */
    int mark_destroyed;
} td_rd;
td_rd *td_rd_new(void);
void td_rd_free(td_rd *r);
int  td_rd_count(const td_rd *r, int type);
int  td_rd_first(const td_rd *r, int type);   /* index of the first logged event of that type or -1 */
int  td_rd_qi_asked(const td_rd *r, const IID *iid);
#endif
