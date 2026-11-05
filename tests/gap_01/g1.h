/* Shared scaffolding for the gap_01 area: thin-coverage methods (font file stream write time, inline object callbacks,
 * GDI-compatible placements, panose / monospace / colour flags, justification, last line wrapping, download queue,
 * font resource axis names, font set axis ranges and locality, expiration events, font list font sets).
 *
 * Fixture font family "G1Fam" (synthetic, UPM 2048): letters, digits and space have advance 1000 design units. At font size 64 one
 * character is exactly 31.25 DIPs (binary exact), so layout widths and positions can be computed. Win ascent 1800 + descent 450.
 */
#ifndef G1_H
#define G1_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define G1_FAM  L"G1Fam"
#define G1_SIZE 64.0f
#define G1_CW   31.25f
#define G1_CH(k)  ((float)(k) * G1_CW)
#define G1_FIT(k) (((float)(k) + 0.5f) * G1_CW)
#define G1_BIG  100000.0f
#define G1_SENT(T) ((T)(ULONG_PTR)1)

typedef struct g1_fx { IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; UINT32 key; } g1_fx;
int  g1_open(g1_fx *x);
void g1_close(g1_fx *x);
#define G1_BEGIN() g1_fx X; memset(&X, 0, sizeof X); if (!g1_open(&X)) { g1_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define G1_END()   g1_close(&X)

IDWriteTextFormat *g1_format(g1_fx *x, float size);
IDWriteTextLayout *g1_layout(g1_fx *x, const WCHAR *text, float width, float height, float size);
typedef struct g1_lines { UINT32 n; DWRITE_LINE_METRICS lm[64]; } g1_lines;
HRESULT g1_get_lines(IDWriteTextLayout *l, g1_lines *o);
/* Compare line lengths with an expectation (reports the actual lengths on mismatch). */
void g1_check_lens_(IDWriteTextLayout *l, const UINT32 *want, int n, const char *file, int line);
#define G1_LENS(l, ...) do { UINT32 e_[] = { __VA_ARGS__ }; g1_check_lens_((l), e_, (int)(sizeof e_ / sizeof e_[0]), __FILE__, __LINE__); } while (0)
/* "3,3,2" style description of the line lengths of a layout (static buffer). */
const char *g1_lens_str(IDWriteTextLayout *l);
/* Line break analysis of text by IDWriteTextAnalyzer (n <= 256); 1 on success. */
int g1_analyze_breaks(IDWriteFactory *f, const WCHAR *text, UINT32 n, DWRITE_LINE_BREAKPOINT *out);

/* ---- recording inline object ---------------------------------------------------------------------------------------- */
#define G1_IO_LOG 16
typedef struct g1_io {
    const IDWriteInlineObjectVtbl *lpVtbl;
    volatile LONG ref; int destroyed;
    DWRITE_INLINE_OBJECT_METRICS metrics; DWRITE_OVERHANG_METRICS overhang;
    DWRITE_BREAK_CONDITION out_before, out_after;                 /* values GetBreakConditions writes */
    int write_break;                                              /* 0: leave the caller's values untouched (the default) */
    HRESULT hr_metrics, hr_overhang, hr_break, hr_draw;           /* returned by the callbacks */
    int write_on_fail;                                            /* still write outputs when a callback returns a failure */
    LONG n_draw, n_metrics, n_overhang, n_break, n_qi;
    int nlog; DWRITE_BREAK_CONDITION in_before[G1_IO_LOG], in_after[G1_IO_LOG];   /* values the caller passed in */
    float draw_x, draw_y; void *draw_ctx; BOOL draw_sideways, draw_rtl;
} g1_io;
g1_io *g1_io_new(float width, float height, float baseline);
#define G1_IO(o) ((IDWriteInlineObject *)(o))

/* ---- remote-flavoured font file loader (IDWriteRemoteFontFileLoader) over a dwx_mem_loader -------------------------------- */
typedef struct g1_remote {
    const void *lpVtbl; volatile LONG ref;
    dwx_mem_loader *inner; DWRITE_LOCALITY locality; HRESULT locality_hr;
    LONG n_locality, n_create, n_remote_create; UINT32 last_key_size, last_key;
    int registered; IDWriteFactory *reg_factory;
} g1_remote;
g1_remote *g1_remote_new(dwx_mem_loader *inner, DWRITE_LOCALITY loc);
HRESULT g1_remote_register(g1_remote *r, IDWriteFactory *f);
void g1_remote_close(g1_remote *r);                              /* unregisters (if registered) and drops the creator's reference */

/* ---- synthetic font helpers ------------------------------------------------------------------------------------------------ */
/* Opens a spec through the in-memory loader on an isolated factory; face may be NULL on failure (t_note). */
typedef struct g1_face { dwx_font h; int ok; IDWriteFactory *f; } g1_face;
int  g1_face_open(g1_face *g, const ttf_spec *s);
void g1_face_close(g1_face *g);
#define G1_FACE(g, spec) g1_face g; memset(&g, 0, sizeof g); if (!g1_face_open(&g, &(spec))) { g1_face_close(&g); t_skip("synthetic face unavailable"); return; }
void *g1_qi(IUnknown *u, const IID *iid);                      /* QueryInterface helper (NULL when unsupported) */
/* System font object for a family (regular style unless given); NULL when the family is not installed. */
IDWriteFont *g1_sysfont(IDWriteFactory *f, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st);

/* ---- one synthetic font reached through a private custom collection (IDWriteFont, face and family) ------------------------ */
typedef struct g1_fnt {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; IDWriteFontFamily *fam; IDWriteFont *font; IDWriteFontFace *face; int ok;
} g1_fnt;
int  g1_fnt_open(g1_fnt *g, const ttf_spec *s);
void g1_fnt_close(g1_fnt *g);
/* Patches bytes into a spec's OS/2 table (copy kept for the life of the process). Returns 1 on success. */
int  g1_patch_os2(ttf_spec *s, unsigned off, const void *bytes, unsigned len);
#define G1_FNT(g, spec) g1_fnt g; memset(&g, 0, sizeof g); if (!g1_fnt_open(&g, &(spec))) { g1_fnt_close(&g); t_skip("synthetic font collection unavailable"); return; }

/* ---- synthetic variable fonts (fvar + gvar, optional STAT axis values and fvar named instances) ---------------------------- */
typedef struct g1_axis { char tag[5]; float mn, df, mx; unsigned nameid; const char *name; } g1_axis;
typedef struct g1_stat_val { int format; int axis; unsigned nameid; const char *name; float value, lo, hi, linked; } g1_stat_val;   /* format 1 (value), 2 (nominal value, lo..hi), 3 (value + linked) */
typedef struct g1_inst { unsigned nameid; const char *name; float coord[4]; } g1_inst;
/* Default synthetic glyphs, family `family`, with an fvar of the axes, a STAT table when nv > 0 and nin fvar named instances. malloc'd blob. */
uint8_t *g1_build_var(const char *family, const g1_axis *ax, int n, const g1_stat_val *sv, int nv, const g1_inst *in, int nin, size_t *size);
/* Two axes: wght 100/400/900 (name 256 "Weight"), XTRA 10/50/90 (name 257 "Extra Axis"). */
uint8_t *g1_build_var2(const char *family, size_t *size);
#define G1_TAG(a, b, c, d) DW_TAG(a, b, c, d)
#endif
