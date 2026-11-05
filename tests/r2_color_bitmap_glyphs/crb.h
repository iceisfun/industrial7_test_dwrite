/* Shared fixtures for the r2_color_bitmap_glyphs area: glyph image data (IDWriteFontFace4::GetGlyphImageFormats /
 * GetGlyphImageData / ReleaseGlyphImageData), colour bitmap and SVG strikes, CPAL palette layouts and
 * IDWriteFactory4::TranslateColorGlyphRun over bitmap glyphs.
 *
 * Facts about dwrite.dll (Windows 10 19041) the expectations rest on:
 *   - OpenTypeFontFaceBuilder::ReadColorTables (!0x783bc) feeds three readers into GlyphGraphicDataBuilder: SvgTableReader::EnumerateGlyphData
 *     (!0x11f744), EmbeddedBitmapTableReader::EnumerateGlyphData (CBLC/CBDT, !0x11ce48, only strikes with ppemX == ppemY and bit depth 32) and
 *     SbixTableReader::EnumerateGlyphData (!0x11fa00).  The face-wide GetGlyphImageFormats() byte is the OR of glyf (1), CFF (2),
 *     COLR+CPAL (4) and the format byte of every entry that was accepted.
 *   - GlyphGraphicDataBuilder::SetEntry (!0x10ffe0) drops entries whose glyph id is >= maxp.numGlyphs, completes bitmap entries with
 *     metrics derived from the font (scale ppem / unitsPerEm) and orders the per glyph list by increasing ppem.
 *   - FontFace::GetGlyphImageData (!0xfb340) returns the first entry (smallest ppem) whose ppem >= the requested ppem and whose format matches;
 *     uniqueDataId and the data pointer both come from the absolute file offset of the image bytes.
 */
#ifndef CRB_H
#define CRB_H
#include "dwx.h"

#define F_SVG  DWRITE_GLYPH_IMAGE_FORMATS_SVG
#define F_PNG  DWRITE_GLYPH_IMAGE_FORMATS_PNG
#define F_JPEG DWRITE_GLYPH_IMAGE_FORMATS_JPEG
#define F_TIFF DWRITE_GLYPH_IMAGE_FORMATS_TIFF
#define F_PREM DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8
#define F_TT   DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE
#define F_CFF  DWRITE_GLYPH_IMAGE_FORMATS_CFF
#define F_COLR DWRITE_GLYPH_IMAGE_FORMATS_COLR
#define F_BITMAPS (F_SVG | F_PNG | F_JPEG | F_TIFF | F_PREM)
#define E_NOCOLOR_ ((HRESULT)0x8898500C)
#define E_INVALIDARG_ ((HRESULT)0x80070057)

/* ---- growable byte buffer (big endian writers) ---- */
typedef struct { uint8_t *p; size_t n, cap; } crb_buf;
void crb_put(crb_buf *b, const void *d, size_t n);
void crb_u8(crb_buf *b, unsigned v);
void crb_u16(crb_buf *b, unsigned v);
void crb_u32(crb_buf *b, unsigned v);
void crb_zero(crb_buf *b, size_t n);
void crb_set16(crb_buf *b, size_t at, unsigned v);
void crb_set32(crb_buf *b, size_t at, unsigned v);

/* ---- image blobs whose dimensions dwrite parses (GetImageDimensions, !0x10fbb0) ---- */
uint8_t *crb_png(unsigned w, unsigned h, unsigned tag, size_t *n);                 /* signature + IHDR + private chunk + IEND */
/* style 0: SOI APP0 SOF0 SOS EOI;  1: SOI DQT DHT SOF2 SOS EOI;  2: SOI, fill bytes, SOF1;  3: SOS before any SOF (dimensions unreadable) */
uint8_t *crb_jpeg(unsigned w, unsigned h, unsigned tag, int style, size_t *n);
/* big_endian: 'MM' else 'II'; wtype/htype: TIFF field types of the ImageWidth/ImageLength entries (1 BYTE, 3 SHORT, 4 LONG, 6 SBYTE, 8 SSHORT, 9 SLONG);
 * extra_first: put NewSubfileType (0xFE) before the dimension entries */
uint8_t *crb_tiff(unsigned w, unsigned h, int big_endian, int wtype, int htype, int extra_first, unsigned tag, size_t *n);

/* ---- CBLC / CBDT ---- */
typedef struct { uint8_t h, w; int8_t bx, by; uint8_t adv; int8_t vbx, vby; uint8_t vadv; } crb_met;
typedef struct { unsigned gid; crb_met m; const uint8_t *img; size_t imglen; } crb_cbglyph;
typedef struct {
    unsigned ppem_x, ppem_y, depth;
    int imgfmt;                       /* 17 small metrics, 18 big metrics, 19 metrics in the index subtable (idxfmt 2) */
    int idxfmt;                       /* 1 (u32 offsets), 3 (u16 offsets), 2 (constant size + big metrics, imgfmt 19) */
    crb_met sub;                      /* idxfmt 2: metrics stored in the subtable */
    const crb_cbglyph *g; int ng;     /* consecutive glyph ids g[0].gid, +1, ... */
} crb_cbstrike;
void crb_cbtables(const crb_cbstrike *s, int ns, uint8_t **cblc, size_t *nlc, uint8_t **cbdt, size_t *ndt);

/* ---- SVG ---- */
typedef struct { unsigned first, last; const char *doc; size_t len; } crb_svgdoc;   /* equal doc pointers share one stored document */
uint8_t *crb_svg(const crb_svgdoc *d, int n, size_t *len);

/* ---- sbix ---- */
typedef struct { unsigned gid; uint32_t type; int16_t ox, oy; const uint8_t *data; size_t len; int trunc; } crb_sbrec;   /* data = the bytes after the 8 byte header; trunc>0: write only the first trunc bytes (< 8) of the record */
typedef struct { unsigned ppem, ppi; const crb_sbrec *r; int nr; } crb_sbstrike;     /* records sorted by gid */
uint8_t *crb_sbix(const crb_sbstrike *s, int ns, unsigned nglyphs, unsigned version, size_t *n);
#define CRB_TAG(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

/* ---- fonts ---- */
/* default synthetic font (69 glyphs; nglyphs > 69 appends empty glyphs) plus raw tables. */
uint8_t *crb_blob(const ttf_rawtable *raw, int nraw, int nglyphs, size_t *n);
/* same, plus COLR (layers) / CPAL from the generator spec */
typedef struct crb {
    dwx_font h; IDWriteFontFace4 *f4; IDWriteFontFace2 *f2; uint8_t *blob; size_t n; int ok;
} crb;
int crb_open(crb *x, uint8_t *blob, size_t n);       /* takes ownership of blob; t_skip + returns 0 on failure */
void crb_close(crb *x);
const uint8_t *crb_find(const crb *x, const void *needle, size_t len);   /* position of needle in the font file, NULL when absent */
unsigned crb_table_offset(const crb *x, const char *tag, unsigned *len);   /* absolute file offset of a table, 0 when absent */
UINT32 crb_face_formats(crb *x);
HRESULT crb_gif(crb *x, UINT16 g, UINT32 a, UINT32 b, UINT32 *out);
HRESULT crb_gid(crb *x, UINT16 g, UINT32 ppem, UINT32 fmt, DWRITE_GLYPH_IMAGE_DATA *d, void **ctx);
int crb_is_zero(const void *p, size_t n);
#define CRB_NEED(x, blob, n) crb x; if (!crb_open(&x, blob, n)) { crb_close(&x); return; }

IDWriteFactory4 *crb_f4(void);
IDWriteFactory2 *crb_f2(void);
#define T(n) TEST(r2_color_bitmap_glyphs_##n)

/* A ready-made CBDT font used by several files: strikes (file order) and glyphs, see crb.c. */
uint8_t *crb_cbdt_font(size_t *n);
/* the entries dwrite must accept from that font (strikes with depth != 32 or ppemX != ppemY are not in the list) */
typedef struct { unsigned gid, ppem; int imgfmt; crb_met m; const uint8_t *png; size_t pnglen; } crb_cbexp;
const crb_cbexp *crb_cb_entries(int *n);
#endif
