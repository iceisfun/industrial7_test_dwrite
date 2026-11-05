/* Shared fixture for the glyph_run_analysis area (IDWriteGlyphRunAnalysis).
 * Everything is LoadLibrary("dwrite.dll") -> GetProcAddress("DWriteCreateFactory") -> vtable calls (via common/dw.h).
 *
 * Deterministic geometry of the default synthetic font (UPM 2048, see common/ttfgen.h): letters/digits are the box
 * (100,0)-(900,1400), advance 1000. For em sizes that are multiples of 512 every box edge falls on an integer pixel
 * coordinate, so aliased bounds are exact integers:
 *     x0 = em*100/2048, x1 = em*900/2048, top = -em*1400/2048 (y grows downwards), bottom = 0, advance = em*1000/2048.
 */
#ifndef GRA_H
#define GRA_H
#include "dwx.h"
#include <float.h>

#define GRA_E_INVALIDARG ((HRESULT)0x80070057)
#define GRA_E_NOBUF      ((HRESULT)0x8007007A)
#define GRA_SENTINEL ((void *)(ULONG_PTR)0x1000)      /* never dereferenced: checks that the callee overwrote the out slot */
#define GRA_NIL(p) ((void *)(p) == GRA_SENTINEL)

#define GRA_X0(em) ((em) * 100 / 2048)
#define GRA_X1(em) ((em) * 900 / 2048)
#define GRA_HT(em)  ((em) * 1400 / 2048)
#define GRA_ADV(em) ((em) * 1000 / 2048)

typedef struct gra_fx {
    dwx_font h; IDWriteFactory *f; int ok;
    UINT16 gi[16]; float adv[16]; DWRITE_GLYPH_OFFSET off[16];
    DWRITE_GLYPH_RUN run;
} gra_fx;
/* Opens the default synthetic font on the shared factory. Returns 0 (and the test should skip) when unavailable. */
int gra_open(gra_fx *fx);
void gra_close(gra_fx *fx);
#define GRA_FIX(fx) gra_fx fx; if (!gra_open(&fx)) { t_skip("synthetic font unavailable"); gra_close(&fx); return; }
/* Skip (after closing the fixture) when a prerequisite call fails. */
#define GRA_REQ(expr) do { HRESULT hr_ = (expr); if (FAILED(hr_)) { t_skip("prerequisite failed: %s -> %s", #expr, dw_hr_name(hr_)); gra_close(&fx); return; } } while (0)
/* Run of the given ASCII characters at em size; no advances / offsets (font advances). */
void gra_run(gra_fx *fx, const char *chars, float em);
/* Same run but with explicit advances (n values). */
void gra_advances(gra_fx *fx, const float *adv);
void gra_offsets(gra_fx *fx, const DWRITE_GLYPH_OFFSET *off);
/* Run of explicit glyph ids. */
void gra_run_gids(gra_fx *fx, const UINT16 *gids, int n, float em);

/* Creation helpers: v1 overload (IDWriteFactory), IDWriteFactory2, IDWriteFactory3 overloads. *a is zeroed first. */
HRESULT gra_make(gra_fx *fx, IDWriteGlyphRunAnalysis **a, float ppd, const DWRITE_MATRIX *m, DWRITE_RENDERING_MODE mode,
                 DWRITE_MEASURING_MODE mm, float ox, float oy);
/* ppd 1 shortcut: aliased, natural measuring. */
HRESULT gra_make_al(gra_fx *fx, IDWriteGlyphRunAnalysis **a, float ox, float oy);
HRESULT gra_make2(gra_fx *fx, IDWriteGlyphRunAnalysis **a, const DWRITE_MATRIX *m, DWRITE_RENDERING_MODE mode, DWRITE_MEASURING_MODE mm,
                  DWRITE_GRID_FIT_MODE gf, DWRITE_TEXT_ANTIALIAS_MODE aa, float ox, float oy);   /* E_NOINTERFACE-like 0xDEAD0001 when unsupported */
HRESULT gra_make3(gra_fx *fx, IDWriteGlyphRunAnalysis **a, const DWRITE_MATRIX *m, DWRITE_RENDERING_MODE1 mode, DWRITE_MEASURING_MODE mm,
                  DWRITE_GRID_FIT_MODE gf, DWRITE_TEXT_ANTIALIAS_MODE aa, float ox, float oy);
#define GRA_UNSUPPORTED ((HRESULT)0xDEAD0001)
/* Skip the test when a factory2/3 overload is unavailable. */
#define GRA_NEED_F(fx, ver) do { if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver)) { t_skip("IDWriteFactory%d unsupported", ver); gra_close(&(fx)); return; } } while (0)

/* Texture fetch: bounds + the texture of exactly those bounds. */
typedef struct gra_tex { RECT rc; int w, h; size_t n; BYTE *p; HRESULT hb, ht; } gra_tex;
/* Returns 1 when the bounds are non-empty and the texture was produced (S_OK). p is malloc'd (gra_tex_free); filled with 0xCD before the call. */
int gra_tex_get(IDWriteGlyphRunAnalysis *a, DWRITE_TEXTURE_TYPE t, gra_tex *x);
/* Texture for caller-chosen bounds. */
int gra_tex_rect(IDWriteGlyphRunAnalysis *a, DWRITE_TEXTURE_TYPE t, const RECT *rc, gra_tex *x);
void gra_tex_free(gra_tex *x);
static __inline int gra_rect_empty(const RECT *r) { return r->right <= r->left || r->bottom <= r->top; }
static __inline int gra_rect_zero(const RECT *r) { return !r->left && !r->top && !r->right && !r->bottom; }
/* Bounds only (type), S_OK required by the caller. */
HRESULT gra_bounds(IDWriteGlyphRunAnalysis *a, DWRITE_TEXTURE_TYPE t, RECT *rc);
/* Counters over a texture (all bytes). */
size_t gra_count_eq(const gra_tex *x, BYTE v);
size_t gra_count_nz(const gra_tex *x);
unsigned long long gra_sum(const gra_tex *x);
size_t gra_count_not01(const gra_tex *x);      /* bytes that are neither 0 nor 255 */

#define CHECK_RECT(rc, l, t, r, b) t_check((rc).left == (l) && (rc).top == (t) && (rc).right == (r) && (rc).bottom == (b), __FILE__, __LINE__, \
    #rc " == (" #l "," #t "," #r "," #b ")", "got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)", (long)(rc).left, (long)(rc).top, (long)(rc).right, (long)(rc).bottom, \
    (long)(l), (long)(t), (long)(r), (long)(b))
#define CHECK_RECT_EQ(a_, b_) t_check((a_).left == (b_).left && (a_).top == (b_).top && (a_).right == (b_).right && (a_).bottom == (b_).bottom, __FILE__, __LINE__, \
    #a_ " == " #b_, "got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)", (long)(a_).left, (long)(a_).top, (long)(a_).right, (long)(a_).bottom, \
    (long)(b_).left, (long)(b_).top, (long)(b_).right, (long)(b_).bottom)
#define CHECK_RECT_ZERO(rc) CHECKF(gra_rect_zero(&(rc)), "rect (%ld,%ld,%ld,%ld) not all zero", (long)(rc).left, (long)(rc).top, (long)(rc).right, (long)(rc).bottom)

extern const DWRITE_MATRIX gra_ident;
const char *gra_mode_name(int mode);
#endif
