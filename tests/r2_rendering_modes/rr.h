/* Shared scaffolding for the r2_rendering_modes area: rendering-mode resolution (IDWriteFontFace*::GetRecommendedRenderingMode)
 * and the glyph run analysis matrix.  Everything is LoadLibrary("dwrite.dll") -> GetProcAddress("DWriteCreateFactory") -> vtable calls.
 *
 * Observed facts the model below reproduces (Windows 10 19041 dwrite.dll):
 *   ComputePpem            ppem = fontEmSize * scale; scale = dpi/96 per axis combined with the matrix.  An axis-aligned matrix (m12 == 0 &&
 *                          m21 == 0, or m11 == 0 && m22 == 0) gives max(|sx|,|sy|); anything else the largest singular value.
 *                          dpiX == dpiY without a matrix is "aligned" (flag 1); isSideways (or a 90 degree matrix) sets flag 2, both together cancel.
 *   ResolveRenderingMode   ppem >= 100 (OUTLINE_THRESHOLD_ANTIALIASED) or >= 350 (OUTLINE_THRESHOLD_ALIASED) -> OUTLINE.
 *                          GDI_NATURAL measuring -> GDI_NATURAL.  GDI_CLASSIC measuring -> ALIASED or GDI_CLASSIC (system smoothing dependent).
 *                          NATURAL measuring: not axis aligned -> NATURAL_SYMMETRIC; aligned -> gasp table (rounded ppem, half up):
 *                          a version-1 gasp with GASP_SYMMETRIC_SMOOTHING (0x8) -> NATURAL_SYMMETRIC else NATURAL; no gasp and no hinting program -> NATURAL_SYMMETRIC.
 *                          A NATURAL result with flag 2 (sideways / 90 degrees) and no embedded bitmaps becomes NATURAL_SYMMETRIC.
 *                          NATURAL_SYMMETRIC becomes NATURAL_SYMMETRIC_DOWNSAMPLED (7) when min(dpiX,dpiY) * 100 >= 17280.
 *                          Only IDWriteFontFace3 reports 7; FontFace/1/2 map 7 to 5.
 *   Gasp ranges            OpenTypeFontFaceBuilder::ReadRenderingModeRanges builds toggle vectors per behavior flag (8: smoothing, 2: gray, 4: grid fit);
 *                          lookup is lower_bound(vector, ppem) and an odd index means "flag set".  Ranges whose maxPPEM does not increase are skipped,
 *                          the last range extends to 0xFFFF.
 */
#ifndef RR_H
#define RR_H
#include "dwx.h"
#include <float.h>

#define RR_INVALIDARG ((HRESULT)0x80070057)
#define RR_NOBUF      ((HRESULT)0x8007007A)
#define RR_SENT       0x5A5A
#define RR_DOWNSAMPLE_DPI 172.8f

/* ---- gasp model ------------------------------------------------------------------------------------------------ */
typedef struct { ttf_gasp r[8]; int n; const char *name; } rr_gasp;

/* the toggle vector for one behavior flag; returns its length (0 when no gasp). Mirrors ReadRenderingModeRanges. */
static __inline int rr_gasp_vec(const rr_gasp *g, unsigned flag, unsigned short *out) {
    int cnt = 0, i; unsigned short prev; unsigned b;
    if (!g || g->n <= 0) return 0;
    b = g->r[0].behavior; prev = g->r[0].max_ppem;
    if (b & flag) out[cnt++] = 0;
    out[cnt++] = prev;
    for (i = 1; i < g->n; i++) {
        unsigned short m = g->r[i].max_ppem; unsigned nb = g->r[i].behavior;
        if (prev < m) {
            if (((nb ^ b) & flag) == 0) out[cnt - 1] = m; else out[cnt++] = m;
            b = nb; prev = m;
        }
    }
    if (prev != 0xFFFF) out[cnt - 1] = 0xFFFF;
    return cnt;
}
/* 1 when `flag` applies at the (integer) ppem; defdef is the answer when the font has no gasp */
static __inline int rr_gasp_flag(const rr_gasp *g, unsigned flag, unsigned ppem, int defdef) {
    unsigned short v[20]; int n = rr_gasp_vec(g, flag, v), i = 0;
    unsigned short p = ppem > 0xFFFF ? 0xFFFF : (unsigned short)ppem;
    if (n == 0) return defdef;
    while (i < n && v[i] < p) i++;
    return i & 1;
}
static __inline int rr_round_half_up(float x) { return (int)floorf(x + 0.5f); }

/* ---- the input of one query and the model of its outcome -------------------------------------------------------- */
typedef struct { float em, dx, dy; int hasm; DWRITE_MATRIX m; int sw, th, mm; } rr_in;
typedef struct { float ppem; int axis; int mode; int gdiclassic_either; } rr_out;

static __inline int rr_zero(float v) { return fabsf(v) < 1.0f / 65536.0f; }
static __inline float rr_maxscale(float a, float b, float c, float d) {
    if (b == 0.0f && c == 0.0f) { float x = fabsf(d), y = fabsf(a); return x <= y ? y : x; }
    {
        float f5 = a * a + b * b, f4 = c * c + d * d, f3 = d * b + c * a, f2 = f5 - f4;
        float s = sqrtf(f3 * 4.0f * f3 + f2 * f2);
        return sqrtf((f4 + f5 + s) * 0.5f);
    }
}
/* ppem and axis flags exactly as ComputePpem / the inline copy in IDWriteFontFace3 */
static __inline void rr_ppem(const rr_in *in, float *ppem, int *axis) {
    float sx = in->dx / 96.0f, sy = in->dy / 96.0f, p;
    int ax;
    if (in->dx == in->dy && !in->hasm) { ax = in->sw ? 3 : 1; p = sx; }
    else {
        float a = sx, b = 0, c = 0, d = sy, f20 = 0, f22 = 0;
        if (in->hasm) { a = sx * in->m.m11; b = sy * in->m.m12; c = sx * in->m.m21; d = sy * in->m.m22; }
        if (rr_zero(b)) {
            if (rr_zero(c)) { ax = 1; f20 = a; f22 = d; } else ax = 4;
        } else if (rr_zero(a)) {
            if (rr_zero(d)) { ax = 3; f20 = b; f22 = c; } else ax = 6;
        } else if (rr_zero(c)) ax = 4;
        else ax = rr_zero(d) ? 6 : 0;
        if (ax & 1) { p = fabsf(f22) <= fabsf(f20) ? fabsf(f20) : fabsf(f22); if (in->sw) ax ^= 2; }
        else p = rr_maxscale(a, b, c, d);
    }
    *ppem = p * in->em; *axis = ax;
}
/* Expected DWRITE_RENDERING_MODE1 (-2: ALIASED or GDI_CLASSIC, -3: NATURAL or the symmetric modes) for default rendering params and a font with the given gasp (NULL = none, no hinting program). */
static __inline int rr_model(const rr_in *in, const rr_gasp *g, int *ppem_i, int *axis_o) {
    float ppem; int axis, mode;
    float thr = in->th == 0 ? 100.0f : 350.0f;
    rr_ppem(in, &ppem, &axis);
    if (ppem_i) *ppem_i = rr_round_half_up(ppem);
    if (axis_o) *axis_o = axis;
    if (ppem >= thr) return 6;
    if (in->mm == 2) return 3;
    if (in->mm == 1) return -2;                     /* ALIASED or GDI_CLASSIC (system dependent) */
    if (!(axis & 1)) mode = 5;
    else if (ppem <= 0.0f) mode = 4;
    else {
        int ri = rr_round_half_up(ppem);
        if ((!g || g->n <= 0) && ri <= 20) return -3;   /* no gasp: 4 or 5 depending on the hinting/bitmap flags of the face (5 for the synthetic fonts) */
        mode = rr_gasp_flag(g, 8, (unsigned)ri, 1) ? 5 : 4;
        if ((axis & 2) && mode == 4) mode = 5;     /* no embedded bitmaps in the synthetic fonts */
    }
    if (mode == 5) { float mn = in->dx <= in->dy ? in->dx : in->dy; if (mn * 100.0f >= 17280.0f) mode = 7; }
    return mode;
}

/* ---- font fixture ------------------------------------------------------------------------------------------------ */
typedef struct rr_font {
    dwx_font h; IDWriteFactory *f; int ok;
    IDWriteFontFace *f0; IDWriteFontFace1 *f1; IDWriteFontFace2 *f2; IDWriteFontFace3 *f3;
    IDWriteRenderingParams *dp;                  /* factory default rendering params */
} rr_font;
/* Opens the default synthetic font, optionally with a version-1 gasp table. Returns 0 (skip) when unavailable. */
int rr_open(rr_font *x, const rr_gasp *g);
void rr_close(rr_font *x);
#define RR_FIX(x, g) rr_font x; if (!rr_open(&x, (g))) { t_skip("synthetic font unavailable"); rr_close(&x); return; }
#define RR_FIX3(x, g) RR_FIX(x, g) if (!x.f3 || !x.f2 || !x.f1) { t_skip("IDWriteFontFace1/2/3 unsupported"); rr_close(&x); return; }

/* call wrappers; outputs are pre-filled with RR_SENT */
HRESULT rr_call0(rr_font *x, float em, float ppd, int mm, IDWriteRenderingParams *p, int *mode);
HRESULT rr_call1(rr_font *x, const rr_in *in, int *mode);
HRESULT rr_call2(rr_font *x, const rr_in *in, IDWriteRenderingParams *p, int *mode, int *grid);
HRESULT rr_call3(rr_font *x, const rr_in *in, IDWriteRenderingParams *p, int *mode, int *grid);
static __inline rr_in rr_mk(float em, float dpi, int th, int mm) { rr_in i; memset(&i, 0, sizeof i); i.em = em; i.dx = i.dy = dpi; i.th = th; i.mm = mm; return i; }
static __inline rr_in rr_mkm(float em, float dpi, const DWRITE_MATRIX *m, int th, int mm) { rr_in i = rr_mk(em, dpi, th, mm); i.hasm = 1; i.m = *m; return i; }

/* gasp fixtures (see rr_util.c) */
extern const rr_gasp rr_gasps[];
extern const int rr_ngasps;

/* ---- glyph run analysis scaffolding ------------------------------------------------------------------------------ */
typedef struct rr_run {
    rr_font x; UINT16 gi[16]; float adv[16]; DWRITE_GLYPH_RUN run; int ok;
} rr_run;
int rr_run_open(rr_run *r);
void rr_run_close(rr_run *r);
void rr_run_set(rr_run *r, const char *chars, float em);
#define RR_RUN(r) rr_run r; if (!rr_run_open(&r)) { t_skip("synthetic font unavailable"); rr_run_close(&r); return; }
HRESULT rr_an(rr_run *r, IDWriteGlyphRunAnalysis **a, float ppd, const DWRITE_MATRIX *m, int mode, int mm, float ox, float oy);
HRESULT rr_an2(rr_run *r, IDWriteGlyphRunAnalysis **a, const DWRITE_MATRIX *m, int mode, int mm, int gf, int aa, float ox, float oy);   /* RR_UNSUP when no Factory2 */
HRESULT rr_an3(rr_run *r, IDWriteGlyphRunAnalysis **a, const DWRITE_MATRIX *m, int mode1, int mm, int gf, int aa, float ox, float oy);  /* RR_UNSUP when no Factory3 */
#define RR_UNSUP ((HRESULT)0xDEAD0001)
HRESULT rr_bounds(IDWriteGlyphRunAnalysis *a, int type, RECT *rc);
/* texture of exactly the bounds of `type`; p malloc'd; returns 1 when produced */
typedef struct { RECT rc; int w, h; size_t n; BYTE *p; } rr_tex;
int rr_tex_get(IDWriteGlyphRunAnalysis *a, int type, rr_tex *t);
int rr_tex_rect(IDWriteGlyphRunAnalysis *a, int type, const RECT *rc, rr_tex *t);
void rr_tex_free(rr_tex *t);
static __inline int rr_rect_empty(const RECT *r) { return r->right <= r->left || r->bottom <= r->top; }
static __inline size_t rr_count(const rr_tex *t, int lo, int hi) { size_t i, c = 0; for (i = 0; i < t->n; i++) c += t->p[i] >= lo && t->p[i] <= hi; return c; }
static __inline unsigned long long rr_sum(const rr_tex *t) { size_t i; unsigned long long s = 0; for (i = 0; i < t->n; i++) s += t->p[i]; return s; }
extern const DWRITE_MATRIX rr_ident;
#define CHECK_RECT(rc, l, t, r, b) t_check((rc).left == (l) && (rc).top == (t) && (rc).right == (r) && (rc).bottom == (b), __FILE__, __LINE__, \
    #rc " == (" #l "," #t "," #r "," #b ")", "got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)", (long)(rc).left, (long)(rc).top, (long)(rc).right, (long)(rc).bottom, \
    (long)(l), (long)(t), (long)(r), (long)(b))
#define CHECK_RECT_EQ(a_, b_) t_check((a_).left == (b_).left && (a_).top == (b_).top && (a_).right == (b_).right && (a_).bottom == (b_).bottom, __FILE__, __LINE__, \
    #a_ " == " #b_, "got (%ld,%ld,%ld,%ld) want (%ld,%ld,%ld,%ld)", (long)(a_).left, (long)(a_).top, (long)(a_).right, (long)(a_).bottom, \
    (long)(b_).left, (long)(b_).top, (long)(b_).right, (long)(b_).bottom)
/* default synthetic font: letters are the box (100,0)-(900,1400) of 2048 units; at em = 512*k every edge is an integer pixel */
#define RR_X0(em) ((em) * 100 / 2048)
#define RR_X1(em) ((em) * 900 / 2048)
#define RR_HT(em) ((em) * 1400 / 2048)
#define RR_ADV(em) ((em) * 1000 / 2048)
static const char *const rr_mode_names[] = { "DEFAULT", "ALIASED", "GDI_CLASSIC", "GDI_NATURAL", "NATURAL", "NATURAL_SYMMETRIC", "OUTLINE", "DOWNSAMPLED" };
#endif
