/* Shared scaffolding for the r2_variable_fonts area: variable fonts and axis semantics.
 *
 * Fixture font "RVVar" (synthetic, UPM 2048, default glyph set of ttfgen: 'A'..'Z' = gid 2..27, advance 1000, box 100..900 x 0..1400) with
 *
 *   fvar   five axes in this order:  wght 100/400/900 (name id 256)   wdth 75/100/125 (257)   opsz 8/14/144 (258)
 *                                    HIDN 0/0/10, axis flags = HIDDEN (259)   FIXD 5/5/5 (min == max, 260)
 *          four named instances: Light (wght 300), Regular (400), Bold (700), Condensed Bold (wght 700, wdth 75); name ids 261..264
 *   avar   (optional) wght segment map  -1->-1, 0->0, 0.5->0.25, 1->1  (other axes identity)
 *   HVAR   (optional) advance deltas, no mapping (outer 0, inner = glyph id), regions  R0 wght [0,1,1]  R1 wght [-1,-1,0]  R2 wdth [0,1,1]  R3 opsz [0,1,1]  R4 opsz [-1,-1,0]
 *            gid 2 'A': R0 +100  R1 -50  R2 +60        gid 3 'B': R0 +200  R1 -100  R2 0
 *            gid 4 'C': R3 +40  R4 -30        all other glyphs 0
 *   gvar   (always) glyph 'A' moves all four outline points by +100 design units in x at wght = +1 (region peak wght 1.0); no other glyph varies
 *
 * dwrite also adds fixed standard axes (min == default == max) for every tag of {wght, wdth, ital, slnt} the font does not define
 * (standard axes added): the fixture therefore reports RV_NALL = 7 axes, a static font 4.
 * Expected values by the OpenType variation rules (scalar = normalised coordinate inside the region, avar applied first):
 *   advance('A') at wght 900 = 1100, at wght 100 = 950, at wdth 125 = 1060, default 1000; 'B' at wght 900 = 1200.
 * Whether dwrite applies HVAR/gvar to a face built from this synthetic file is observed with t_note first and the exact value is only
 * hard-checked once a change is seen; the axis API itself (counts, defaults, ranges, attributes, canonicalisation) is hard-checked.
 */
#ifndef RV_H
#define RV_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define RV_TAG(a, b, c, d) DW_TAG(a, b, c, d)
#define TAG_WGHT DWRITE_FONT_AXIS_TAG_WEIGHT
#define TAG_WDTH DWRITE_FONT_AXIS_TAG_WIDTH
#define TAG_OPSZ DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE
#define TAG_SLNT DWRITE_FONT_AXIS_TAG_SLANT
#define TAG_ITAL DWRITE_FONT_AXIS_TAG_ITALIC
#define TAG_HIDN RV_TAG('H', 'I', 'D', 'N')
#define TAG_FIXD RV_TAG('F', 'I', 'X', 'D')
#define TAG_ZSTA RV_TAG('Z', 'S', 'T', 'A')
#define TAG_ZZZZ RV_TAG('z', 'z', 'z', 'z')
#define RV_NAXES 5               /* fvar axes of the fixture */
#define RV_NALL 7                /* + the standard axes dwrite adds for the tags missing from {wght, wdth, ital, slnt}: ital and slnt */
#define RV_NINST 4
#define RV_SENT(T) ((T)(ULONG_PTR)1)

/* fixture flavours */
#define RVF_AVAR 1
#define RVF_HVAR 2
#define RVF_NOGVAR 4
#define RVF_STAT 16              /* STAT with design axes 'ZSTA' (not in fvar) and 'wght', no axis values */
#define RVF_NOFVAR 8             /* static font with the same names (control) */
typedef struct rv_axis { char tag[5]; float mn, df, mx; unsigned flags; unsigned nameid; const char *name; } rv_axis;
extern const rv_axis rv_axes[RV_NAXES];
uint8_t *rv_build(unsigned flags, size_t *size);          /* malloc'd blob */
/* the same fixture with a caller defined axis list (n <= 8, no instances, no avar/HVAR/gvar content beyond an empty gvar) */
uint8_t *rv_build_axes(const rv_axis *ax, int n, size_t *size);

/* one synthetic font on an isolated factory, resource created from the file */
typedef struct rv {
    IDWriteFactory *f; IDWriteFactory6 *f6; dwx_mem_loader *ld; IDWriteFontFile *file; IDWriteFontResource *res; UINT32 key; int ok;
} rv;
int  rv_open(rv *x, const uint8_t *blob, size_t n);       /* 0 + t_skip on failure (caller still calls rv_close) */
void rv_close(rv *x);
#define RV_BEGIN(flags) size_t n_; uint8_t *b_ = rv_build((flags), &n_); rv x; \
    if (!b_) { t_skip("font build failed"); return; } \
    if (!rv_open(&x, b_, n_)) { rv_close(&x); free(b_); return; }
#define RV_END() do { rv_close(&x); free(b_); } while (0)

/* face with axis values (NULL + CHECK failure when creation fails) */
IDWriteFontFace5 *rv_face(rv *x, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n);
int rv_axis_index(IDWriteFontResource *res, UINT32 tag);  /* index in the resource's axis order, -1 when absent */
int rv_find_value(const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, UINT32 tag);
/* design advance of a glyph index (UPM 2048 design units); returns -1 when the call fails */
int rv_adv(IDWriteFontFace *face, UINT16 gid);
UINT16 rv_gid(IDWriteFontFace *face, UINT32 cp);
/* design glyph box width of a glyph at the given face (right - left of the outline bounds in design units); -1 on failure */
int rv_outline_x(IDWriteFontFace *face, UINT16 gid, float *xmin, float *xmax);

/* Bahnschrift (variable system font, Windows 10 1709+) through the shared factory; ok = 0 when the font file is absent */
typedef struct rb {
    IDWriteFactory *f; IDWriteFactory6 *f6; IDWriteFontFile *file; IDWriteFontResource *res; char path[MAX_PATH]; int ok;
} rb;
int  rb_open(rb *b);
void rb_close(rb *b);
#define RB_BEGIN() rb b; if (!rb_open(&b)) { rb_close(&b); t_skip("Bahnschrift not available"); return; }
#define RB_END() rb_close(&b)

/* a private collection holding one blob (family "RVVar"), for text format / layout tests */
typedef struct rc {
    IDWriteFactory *f; IDWriteFactory6 *f6; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; int ok;
} rc;
int  rc_open(rc *c, const uint8_t *blob, size_t n);
void rc_close(rc *c);
#define RC_BEGIN(flags) size_t n_; uint8_t *b_ = rv_build((flags), &n_); rc c; \
    if (!b_) { t_skip("font build failed"); return; } \
    if (!rc_open(&c, b_, n_)) { rc_close(&c); free(b_); return; }
#define RC_END() do { rc_close(&c); free(b_); } while (0)
#define RV_FAM L"RVVar"

static __inline DWRITE_FONT_AXIS_VALUE rv_av(UINT32 tag, float v) { DWRITE_FONT_AXIS_VALUE a; a.axisTag = (DWRITE_FONT_AXIS_TAG)tag; a.value = v; return a; }
#endif
