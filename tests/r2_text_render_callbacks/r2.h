/* Fixtures for the r2_text_render_callbacks area: IDWriteTextLayout::Draw / IDWriteInlineObject driven against a hook renderer.
 *
 * Facts used here (Windows 10 dwrite.dll: DWriteTextLayout::Draw, TextLayout::Draw, DrawGlyphRuns/DrawInlineObjects/DrawUnderlines/
 * DrawStrikethroughs, InlineLayout, FPUStateSSE / FPUStateSandboxSSE):
 *   - Draw rejects a NULL renderer with E_INVALIDARG; otherwise it saves MXCSR and, when (MXCSR & 0xff80) != 0x1f80, loads 0x1f80
 *     (all exceptions masked, round to nearest, no flush-to-zero) for the whole call and restores the caller's value on exit;
 *   - every client callback is wrapped in a sandbox that restores the MXCSR value that was current before the callback;
 *   - IsPixelSnappingDisabled is called with *isDisabled preset to FALSE; only a FALSE answer leads to GetPixelsPerDip and then
 *     GetCurrentTransform; a negative HRESULT from any of the three aborts Draw with that HRESULT, non-negative codes are accepted;
 *   - the Draw* helpers ignore the HRESULT of DrawGlyphRun/DrawInlineObject/DrawUnderline/DrawStrikethrough.
 *   - CreateEllipsisTrimmingSign builds an InlineLayout around a one-character layout; its Draw forwards to that layout's Draw
 *     with the origin shifted so that the glyph baseline lands on (originX, originY); it ignores isSideways/isRightToLeft/effect.
 *
 * Deterministic fonts (private collection): "R2Font" regular (letters/digits/space advance 1000, notdef 1200, UPM 2048) and bold
 * (letters 1200, space 600).  At size 16 a regular letter is 7.8125 DIPs, a bold one 9.375.
 */
#ifndef R2_H
#define R2_H
#include "dwx.h"
#include <wchar.h>
#include <math.h>
#include <xmmintrin.h>

#define R2_FAM  L"R2Font"
#define R2_LOC  L"en-us"
#define R2_ADV(units, size) ((float)(units) * (float)(size) / 2048.0f)
#define R2_NAME(n) r2_text_render_callbacks_##n
#define RT(n) TEST(r2_text_render_callbacks_##n)

typedef struct r2_fx { IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; } r2_fx;
int  r2_open(r2_fx *x);
void r2_close(r2_fx *x);
#define R2_BEGIN() r2_fx X; memset(&X, 0, sizeof X); if (!r2_open(&X)) { r2_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define R2_END()   r2_close(&X)

IDWriteTextFormat *r2_format(r2_fx *x, float size, DWRITE_FONT_WEIGHT weight);
IDWriteTextLayout *r2_layout_w(r2_fx *x, const WCHAR *text, float size, float w, float h, DWRITE_FONT_WEIGHT weight);
#define R2_LAYOUT(var, text, size, w, h) IDWriteTextLayout *var = r2_layout_w(&X, text, size, w, h, DWRITE_FONT_WEIGHT_NORMAL); \
    if (!var) { R2_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
#define RNG(s, l) ((DWRITE_TEXT_RANGE){ (s), (l) })
#define CTX_A ((void *)(ULONG_PTR)0x2468ace0)
#define CTX_B ((void *)(ULONG_PTR)0x13579bd0)
#define CHECK_NEARF(a, b) CHECK_NEAR((a), (b), 0.0001)
static __inline int r2_fbits(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }
#define CHECK_FBITS(a, b) t_check(r2_fbits((float)(a), (float)(b)), __FILE__, __LINE__, #a " bit-identical to " #b, "got %g want %g", (double)(a), (double)(b))

/* ---- hook renderer -------------------------------------------------------------------------------------- */
enum { RH_QI = 1, RH_SNAP, RH_XFORM, RH_PPD, RH_GLYPH, RH_UNDER, RH_STRIKE, RH_INLINE, RH_NTYPES };
#define RH_MAXG 40
typedef struct rh_ev {
    int type, r1; DWRITE_GLYPH_ORIENTATION_ANGLE angle;
    void *ctx; float x, y; DWRITE_MEASURING_MODE mm;
    unsigned mxcsr;                         /* MXCSR on entry to the callback */
    HRESULT hr;                             /* what the callback returned */
    IUnknown *effect; LONG effect_refs;     /* raw effect, refcount seen at the call when it is a dwx_effect (else -1) */
    IID iid;                                /* RH_QI */
    /* glyph run */
    const void *run_ptr, *offsets_ptr, *advances_ptr, *indices_ptr, *text_ptr, *cmap_ptr;
    IDWriteFontFace *face; float em; UINT32 n; UINT16 idx[RH_MAXG]; float adv[RH_MAXG]; DWRITE_GLYPH_OFFSET off[RH_MAXG];
    BOOL sideways; UINT8 bidi;
    int has_desc; WCHAR locale[24]; WCHAR text[RH_MAXG]; UINT32 tlen; UINT16 cmap[RH_MAXG]; const void *locale_ptr;
    /* decorations */
    DWRITE_UNDERLINE ul; DWRITE_STRIKETHROUGH st; WCHAR dloc[24];
    /* inline */
    IDWriteInlineObject *obj; BOOL osw, ortl;
} rh_ev;
#define RH_MAXEV 512
typedef struct rh {
    const void *lpVtbl; LONG ref, addref, release;
    rh_ev ev[RH_MAXEV]; int nev; int count[RH_NTYPES];
    int allow_r1;                           /* QI(IDWriteTextRenderer1) succeeds (default 0) */
    /* results */
    int snap_write; BOOL snap_val;          /* write *isDisabled (default 1, FALSE) */
    int ppd_write;  float ppd_val;          /* default 1, 1.0f */
    int xf_write;   DWRITE_MATRIX xf;       /* default 1, identity */
    int ret_set[RH_NTYPES]; HRESULT ret[RH_NTYPES]; int ret_nth[RH_NTYPES];   /* force a return value (every call, or only the nth when ret_nth) */
    int set_csr[RH_NTYPES]; unsigned csr_val[RH_NTYPES];                      /* load MXCSR with this inside the callback */
    void (*hook)(struct rh *r, rh_ev *e, void *arg); void *hook_arg;           /* runs inside every non-QI callback after recording */
    int in_cb;                              /* callbacks currently running */
    int max_depth;
} rh;
rh *rh_new(void);
void rh_free(rh *r);
void rh_clear(rh *r);
static __inline HRESULT rh_draw(IDWriteTextLayout *l, rh *r, void *ctx, float x, float y) { return IDWriteTextLayout_Draw(l, ctx, (IDWriteTextRenderer *)r, x, y); }
#define RH_DRAW_OK(l, r, ox, oy) CHECK_HR_OK(rh_draw((l), (r), CTX_A, (ox), (oy)))
const rh_ev *rh_nth(const rh *r, int type, int n);          /* n-th event of that type, NULL if absent */
int rh_index(const rh *r, const rh_ev *e);
/* All non-QI event types in call order, for order checks. */
int rh_first(const rh *r, int type);                         /* index of the first event of that type or -1 */
int rh_last(const rh *r, int type);
int rh_qi_asked(const rh *r, const IID *iid);
UINT32 rh_total_glyphs(const rh *r);
float rh_run_width(const rh_ev *e);

/* default MXCSR with all exceptions masked */
#define R2_CSR_DEFAULT 0x1f80u
static __inline unsigned r2_getcsr(void) { return _mm_getcsr(); }
static __inline void r2_setcsr(unsigned v) { _mm_setcsr(v); }
#define R2_CSR_MASK 0xff80u
#endif
