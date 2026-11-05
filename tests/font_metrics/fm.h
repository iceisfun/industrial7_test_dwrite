/* Shared helpers of the font_metrics area (derivation mirror of DWRITE_FONT_METRICS1, GDI rounding mirror, font open helpers). */
#ifndef FM_H
#define FM_H
#include "dwx.h"

/* ---- big-endian table readers ---- */
static __inline unsigned fm_be16(const uint8_t *p) { return ((unsigned)p[0] << 8) | p[1]; }
static __inline int fm_be16s(const uint8_t *p) { return (int16_t)fm_be16(p); }
static __inline void fm_put16(uint8_t *p, int v) { p[0] = (uint8_t)((unsigned)v >> 8); p[1] = (uint8_t)v; }

/* ---- fonts ---- */
typedef struct { dwx_font h; IDWriteFontFace *face; IDWriteFontFace1 *f1; DWRITE_FONT_SIMULATIONS sim; } fm_font;
/* Builds the font from a blob (ownership stays with the caller) and opens a face; 1 = ok.  On failure the test is skipped by the
 * caller (call fm_close and return). */
int fm_open_blob(fm_font *F, const void *blob, size_t n, DWRITE_FONT_SIMULATIONS sim);
int fm_open(fm_font *F, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim);
void fm_close(fm_font *F);
#define FM_OPEN(F, spec, sim) fm_font F; memset(&F, 0, sizeof F); \
    do { if (!fm_open(&F, &(spec), (sim))) { fm_close(&F); return; } } while (0)
/* Table inside a built blob (writable view), NULL if missing. */
uint8_t *fm_tbl(uint8_t *blob, size_t n, const char *tag, unsigned *len);
/* default spec with the given upm and proportionally scaled vertical metrics (cap/x heights etc.) */
void fm_spec_upm(ttf_spec *s, int upm);

/* ---- DWRITE_FONT_METRICS1 field table ---- */
#define FM_NFIELDS 23
const char *fm_field_name(int i);
int fm_field_get(const DWRITE_FONT_METRICS1 *m, int i);
void fm_field_set(DWRITE_FONT_METRICS1 *m, int i, int v);
int fm_field_signed(int i);
enum { FM_UPM, FM_ASC, FM_DESC, FM_GAP, FM_CAP, FM_XH, FM_ULPOS, FM_ULTH, FM_STPOS, FM_STTH, FM_BOXL, FM_BOXT, FM_BOXR, FM_BOXB,
       FM_SUBX, FM_SUBY, FM_SUBSX, FM_SUBSY, FM_SUPX, FM_SUPY, FM_SUPSX, FM_SUPSY, FM_TYPO };
/* Compare every field of got/want except those whose bit is set in skip_mask; label is added to messages. */
void fm_cmp_(const char *file, int line, const DWRITE_FONT_METRICS1 *got, const DWRITE_FONT_METRICS1 *want, unsigned skip_mask, const char *label);
#define FM_CMP(got, want, skip, label) fm_cmp_(__FILE__, __LINE__, got, want, skip, label)
#define FM_BIT(f) (1u << (f))
void fm_dump(const char *label, const DWRITE_FONT_METRICS1 *m);

/* ---- derivation of the metrics from the font's own tables (head/hhea/OS-2/post) ---- */
typedef struct {
    DWRITE_FONT_METRICS1 m; DWRITE_CARET_METRICS caret;
    int ok;                         /* tables readable */
    int x_known, cap_known;         /* x/cap height are fixed by OS/2 (otherwise derived from glyphs / PCLT / defaults) */
    int post_present, os2_present, os2_version;
} fm_exp;
int fm_derive(IDWriteFontFace *face, DWRITE_FONT_SIMULATIONS sims, fm_exp *e);
/* mask of fields that cannot be predicted from the tables alone */
unsigned fm_unknown_mask(const fm_exp *e);
/* ValidateHeights bounds: x in (upm/10, 9upm/10) exclusive via 10x, cap in (upm/10, upm] */
int fm_x_valid(unsigned upm, unsigned x);
int fm_cap_valid(unsigned upm, unsigned cap);
/* oblique simulation shear (value * 0x5700 >> 16) */
int fm_shear(int v);

/* ---- GDI compatible metrics mirror ---- */
int fm_rnd(float x);                                    /* round half up */
float fm_scale(const DWRITE_MATRIX *m);                 /* scale factor DirectWrite derives from a transform (NULL = 1) */
void fm_gdi_expect(const DWRITE_FONT_METRICS1 *base, float em, float ppd, const DWRITE_MATRIX *m, DWRITE_FONT_METRICS1 *out);
int fm_has_table(IDWriteFontFace *face, UINT32 tag);

/* ---- glyph metrics ---- */
typedef struct { int adv, lsb, x0, y0, x1, y1; } fm_gbox;      /* hmtx advance/lsb and glyf bounding box */
void fm_design_expect(const fm_gbox *g, int vasc, int vdesc, DWRITE_GLYPH_METRICS *out);
void fm_gm_cmp_(const char *file, int line, const DWRITE_GLYPH_METRICS *got, const DWRITE_GLYPH_METRICS *want, const char *label);
#define FM_GMCMP(got, want, label) fm_gm_cmp_(__FILE__, __LINE__, got, want, label)
/* The default synthetic font: gid -> box (see ttfgen.h) */
const fm_gbox *fm_def_glyph(unsigned gid);

/* system fonts */
IDWriteFontFace *fm_sys(const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st);   /* NULL if missing (caller t_skip) */
IDWriteFontFace1 *fm_f1(IDWriteFontFace *f);                                                  /* QI, caller releases */
#endif
