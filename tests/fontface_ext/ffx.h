/* Shared fixtures for the fontface_ext area (IDWriteFontFace1 .. IDWriteFontFace7 on faces we build or obtain).
 * Everything goes LoadLibrary -> GetProcAddress("DWriteCreateFactory") -> vtable calls via common/dw.c. */
#ifndef FFX_H
#define FFX_H
#include "dwx.h"

/* One open font face with every extension interface the OS offers, QueryInterface'd once.
 * A face on Windows 10 19041 answers IDWriteFontFace .. IDWriteFontFace5 with the very same pointer. */
typedef struct ffx {
    dwx_font h;                 /* loader + file + face (synthetic fonts); zeroed for system faces */
    IDWriteFontFace  *f0;
    IDWriteFontFace1 *f1;
    IDWriteFontFace2 *f2;
    IDWriteFontFace3 *f3;
    IDWriteFontFace4 *f4;
    IDWriteFontFace5 *f5;
    int owns_h;
} ffx;

/* Open a face over a built spec (blob freed internally). Returns 1 when f0..f5 are all usable; otherwise reports t_skip
 * and returns 0 (caller returns; ffx_close is still safe). */
int  ffx_open_spec(ffx *x, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim);
int  ffx_open_default(ffx *x);
int  ffx_open_blob(ffx *x, const void *blob, size_t n, UINT32 index, DWRITE_FONT_SIMULATIONS sim);
/* A face from the system collection (first matching font of the family); skips with t_skip when missing. */
int  ffx_open_sys(ffx *x, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STRETCH st, DWRITE_FONT_STYLE sty);
void ffx_close(ffx *x);

/* DWRITE_FONT_METRICS1 helpers */
void ffx_metrics1(ffx *x, DWRITE_FONT_METRICS1 *m);

/* Failure HRESULT of QueryInterface for an interface the object does not implement. The Windows 10 DWriteFontFace::QueryInterface
 * falls through to `*ppv = NULL; return 0x80004001` (decompiled at dwrite.dll!0x75620). */
#define FFX_QI_FAIL ((HRESULT)0x80004001)
#define FFX_E_NOCOLOR ((HRESULT)0x8898500C)      /* DWRITE_E_NOCOLOR */
#define FFX_E_UNSUPPORTEDOPERATION ((HRESULT)0x88985009)

/* A synthetic spec with (family, weight, width, italic) names from ttf_style_spec. */
static __inline void ffx_style(ttf_spec *s, const char *fam, int w, int wd, int it) { ttf_style_spec(s, fam, w, wd, it); }

/* Gid of a Latin letter / digit in the default synthetic font. */
#define FFX_SAME(a, b) ((void *)(a) == (void *)(b))
#define G(c) ttf_default_gid((unsigned)(c))

/* Synthetic fixture specs shared by several files; the static data they point to lives in ffx_common.c. */
void ffx_spec_kern(ttf_spec *s);            /* legacy 'kern' table pairs, see ffx_kern_pairs */
void ffx_spec_gpos_kern(ttf_spec *s);       /* the same pairs in a GPOS 'kern' feature only */
void ffx_spec_vert(ttf_spec *s);            /* GSUB 'vert' single substitutions + vhea/vmtx */
void ffx_spec_color(ttf_spec *s);           /* COLR v0 + CPAL v0 */
void ffx_spec_ranges(ttf_spec *s);          /* sparse cmap, incl. supplementary plane (format 12) */
extern const ttf_kernpair ffx_kern_pairs[]; extern const int ffx_n_kern_pairs;
extern const ttf_sub1 ffx_vert_subs[]; extern const int ffx_n_vert_subs;
extern const uint32_t ffx_palette[]; extern const int ffx_n_palette;
int ffx_kern_value(unsigned l, unsigned r);   /* expected legacy kern adjustment for a glyph pair (0 when absent) */
#endif
