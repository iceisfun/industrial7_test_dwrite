/* Shared fixtures and an oracle for the color_glyphs area (IDWriteFactory2/4::TranslateColorGlyphRun, the color glyph run
 * enumerators, ComputeGlyphOrigins and the color related IDWriteFontFace2/4 methods).
 *
 * The oracle (cg_expect) encodes what the colour glyph run enumerator creation / AdvanceToBaseRun / AdvanceToColorRun /
 * InitializeColorGlyphPositions (Windows 10 19041) produce for the synthetic COLR fixture:
 *   - the formats actually used are  requested & face-supported; COLR is dropped when the palette index is >= the palette count;
 *     when no bit of 0xFC (COLR, SVG, PNG, JPEG, TIFF, PREMULTIPLIED) remains the call fails with DWRITE_E_NOCOLOR;
 *   - the FIRST run is always the "base run": the whole input run (same glyph count) in which every colour glyph is replaced by
 *     glyph 1, paletteIndex 0xFFFF, runColor all zero, baseline origin = the input origin; its glyphImageFormat is
 *     (used & 3), or NONE when every glyph of the run is a colour glyph;
 *   - then, per colour glyph in run order, one run per COLR layer: 1 glyph, palette colour (or zero colour and 0xFFFF for a
 *     foreground / out of range palette index), layers whose palette entry is fully transparent are skipped,
 *     glyphImageFormat (used & 3) | COLR, baseline x = origin.x -/+ (advance sum of the preceding glyphs), y = origin.y;
 *   - bitmap formats (SVG ...) give one run per group of consecutive colour glyphs of that format. */
#ifndef CG_H
#define CG_H
#include "dwx.h"

#define CG_E_NOCOLOR         ((HRESULT)0x8898500C)
#define CG_E_INVALIDARG      ((HRESULT)0x80070057)
#define CG_E_NOT_VALID_STATE ((HRESULT)0x8007139F)
#define CG_E_NOINTERFACE     ((HRESULT)0x80004002)

#define CGF_NONE   DWRITE_GLYPH_IMAGE_FORMATS_NONE
#define CGF_TT     DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE
#define CGF_CFF    DWRITE_GLYPH_IMAGE_FORMATS_CFF
#define CGF_COLR   DWRITE_GLYPH_IMAGE_FORMATS_COLR
#define CGF_SVG    DWRITE_GLYPH_IMAGE_FORMATS_SVG
#define CGF_PNG    DWRITE_GLYPH_IMAGE_FORMATS_PNG
#define CGF_JPEG   DWRITE_GLYPH_IMAGE_FORMATS_JPEG
#define CGF_TIFF   DWRITE_GLYPH_IMAGE_FORMATS_TIFF
#define CGF_PREM   DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8

/* ---- fixture glyph ids (default synthetic font, see ttfgen.h) ---- */
#define GID_A 2
#define GID_B 3
#define GID_C 4
#define GID_D 5
#define GID_E 6
#define GID_F 7
#define GID_G 8
#define GID_H 9
#define GID_a 28
#define GID_b 29
#define GID_c 30
#define GID_0 54
#define GID_TRI 64
#define GID_ROUND 65
#define GID_COMP 66
#define GID_RING 68

/* Palettes of the color fixtures: 2 palettes x 6 entries, ARGB. Entry 4 of palette 0 is fully transparent, of palette 1 it is opaque. */
#define CG_NPAL 2
#define CG_NENT 6
extern const uint32_t cg_pal[CG_NPAL][CG_NENT];
/* COLR layers of a colour glyph of the fixture; returns the layer count (0 = not a colour glyph). */
int cg_layers_of(unsigned gid, const ttf_colr_layer **layers);
/* Documents of the SVG variant: glyph ranges [3,3] [7,7] [29,30]. Returns the doc index (0..2) or -1. */
int cg_svg_doc_of(unsigned gid);
extern const char *const cg_svg_docs[3];
/* PNG variant: strikes ppem 16 and 32 for glyphs 8..9; the PNG blob per glyph. */
extern const uint8_t cg_png_blob[]; extern const unsigned cg_png_len;

/* The font model the oracle consults: COLR v0 base/layer records (sorted by glyph) and CPAL v0 palettes. */
typedef struct cg_model { const ttf_colr_base *base; int nbase; const ttf_colr_layer *lay; int nlay; int npal, nent; const uint32_t *argb; int svg; } cg_model;
extern const cg_model cg_fixture_model;
extern const cg_model *cg_model_override;       /* NULL = the fixture */
int  cg_model_from_face(IDWriteFontFace *face, cg_model *m);   /* parses COLR/CPAL v0 through TryGetFontTable; 0 when not parseable */
void cg_model_free(cg_model *m);

typedef struct cgfont {
    dwx_font h; int owns;
    IDWriteFontFace *face; IDWriteFontFace2 *f2; IDWriteFontFace4 *f4;
} cgfont;
enum { CGFX_COLR = 1, CGFX_SVG = 2, CGFX_PNG = 4 };
/* Opens a synthetic face (returns 0 and t_skip's on failure). */
int  cg_open_blob(cgfont *x, const void *blob, size_t n);
int  cg_open_plain(cgfont *x);                 /* default font, no colour tables */
int  cg_open_fx(cgfont *x, unsigned fx);       /* default font + COLR/CPAL (2 palettes), optionally + 'SVG ' and/or CBLC/CBDT */
int  cg_open_sys(cgfont *x, const WCHAR *fam); /* system font face, t_skip when the family is missing */
void cg_close(cgfont *x);
uint8_t *cg_build_fx(unsigned fx, size_t *n);  /* the blob (malloc'd) */
uint8_t *cg_build_colr(const ttf_colr_base *b, int nb, const ttf_colr_layer *l, int nl, size_t *n);
uint8_t *cg_build_cpal(int npal, int nent, const uint32_t *argb /* npal*nent */, size_t *n);

IDWriteFactory2 *cg_f2(void);
IDWriteFactory4 *cg_f4(void);
#define CG_NEED_F2(v) IDWriteFactory2 *v = cg_f2(); if (!v) { t_skip("IDWriteFactory2 unsupported"); return; }
#define CG_NEED_F4(v) IDWriteFactory4 *v = cg_f4(); if (!v) { t_skip("IDWriteFactory4 unsupported"); return; }
#define CG_OPEN(x, fx) cgfont x; if (!cg_open_fx(&x, fx)) { cg_close(&x); return; }

/* ---- runs ---- */
typedef struct cgrun {
    DWRITE_GLYPH_RUN run; UINT16 gl[64]; float adv[64]; DWRITE_GLYPH_OFFSET off[64];
} cgrun;
/* with_adv: 0 = NULL advances, 1 = design advances * em/2048, 2 = distinct made-up advances (7 + 3*i + gid%5);
 * with_off: 0 = NULL, 1 = distinct offsets (0.25*(i+1), -0.5*(i+1)) */
void cg_mkrun(cgrun *r, IDWriteFontFace *face, float em, const UINT16 *g, int n, int with_adv, int with_off, BOOL sideways, UINT32 bidi);
int  cg_design_adv(unsigned gid);              /* default synthetic font advance in design units */

/* ---- oracle ---- */
typedef struct cg_exp {
    int base; UINT32 n; UINT16 gl[8]; float adv[8]; float x, y; float c[4]; UINT16 pal; UINT32 fmt; int first_glyph; int svg; unsigned advmask; /* bit k: adv[k] is checked */
} cg_exp;
#define CG_MAXEXP 4096
/* face_formats: formats the face reports (TRUETYPE|COLR [|SVG]); palette: requested palette index. Returns the number of
 * expected runs, or 0 when the call must fail with DWRITE_E_NOCOLOR. */
int cg_expect(const DWRITE_GLYPH_RUN *run, float ox, float oy, UINT32 req_formats, UINT32 face_formats, UINT32 palette, cg_exp *out);
/* Walk an enumerator against the oracle: every MoveNext/GetCurrentRun pair, both interface versions when available.
 * Returns the number of runs enumerated. tag identifies the scenario in failure details. */
int cg_walk(IDWriteColorGlyphRunEnumerator *e, const char *tag, const DWRITE_GLYPH_RUN *in, const cg_exp *exp, int nexp, DWRITE_MEASURING_MODE mm);
/* One-call helper: oracle + Translate via the given factory 2 call + walk. Returns S_OK or the HRESULT of the call (checked against the oracle). */
HRESULT cg_check_f2(IDWriteFactory2 *f, const char *tag, const DWRITE_GLYPH_RUN *run, float ox, float oy, DWRITE_MEASURING_MODE mm,
                    const DWRITE_MATRIX *m, UINT32 palette, UINT32 face_formats);
HRESULT cg_check_f4(IDWriteFactory4 *f, const char *tag, const DWRITE_GLYPH_RUN *run, float ox, float oy, UINT32 formats, DWRITE_MEASURING_MODE mm,
                    const DWRITE_MATRIX *m, UINT32 palette, UINT32 face_formats);
#endif
