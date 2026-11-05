/* Helpers for the synthetic_fonts area: byte-level font editing, a one-call "probe" that records every
 * stage (file reference -> Analyze -> CreateFontFace -> accessors) and a tiny cmap/WOFF byte builder.
 * Everything reaches dwrite.dll through DWriteCreateFactory (GetProcAddress) and COM vtables. */
#ifndef SF_H
#define SF_H
#include "dwx.h"

#define DWRITE_E_FILEFORMAT_  ((HRESULT)0x88985000)
#define DWRITE_E_UNSUPPORTEDOPERATION_ ((HRESULT)0x88985009)

/* big-endian accessors */
static __inline unsigned sf_r16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
static __inline uint32_t sf_r32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static __inline void sf_w16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static __inline void sf_w32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

/* A growable owned byte buffer. */
typedef struct { uint8_t *p; size_t n; } sf_blob;
sf_blob sf_default(void);                              /* default synthetic font (ttfgen) */
sf_blob sf_build(const ttf_spec *s);                   /* ttf_build into a sf_blob */
sf_blob sf_dup(const sf_blob *b);
void    sf_free(sf_blob *b);
/* Directory access for a single-font sfnt blob (font at offset 0). */
int      sf_ntables(const sf_blob *b);
int      sf_find(const sf_blob *b, const char *tag);   /* directory index of the first record with this tag, -1 */
uint8_t *sf_dirent(const sf_blob *b, int i);           /* 16-byte record */
uint8_t *sf_table(const sf_blob *b, const char *tag, unsigned *len);   /* table body (first record), NULL if absent */
void     sf_set_ent(sf_blob *b, int i, uint32_t off, uint32_t len);
void     sf_set_tag(sf_blob *b, int i, const char *tag);
void     sf_set_numtables(sf_blob *b, unsigned n);
void     sf_swap_ents(sf_blob *b, int i, int j);
/* Replace a table body with new data (appended at the end of the file, directory record re-pointed; checksum recomputed). */
void     sf_replace_table(sf_blob *b, const char *tag, const void *data, size_t n);
/* Remove the directory record of `tag` (file body untouched). */
void     sf_drop_record(sf_blob *b, const char *tag);
/* Append a new directory record at the end of the directory (shifts nothing: records are rewritten, bodies re-appended). */
void     sf_fix_all_checksums(sf_blob *b);

/* Outcome of every stage of "load this blob as a font". */
typedef struct sf_probe {
    dwx_mem_loader *ld; IDWriteFactory *f; UINT32 key; IDWriteFontFile *file; IDWriteFontFace *face;
    HRESULT hr_reg, hr_file, hr_analyze, hr_face;
    BOOL supported; DWRITE_FONT_FILE_TYPE ftype; DWRITE_FONT_FACE_TYPE fctype; UINT32 nfaces;
    LONG reads_before_analyze, reads_after_analyze;
} sf_probe;
/* Register the blob and create the file reference + Analyze. Always call sf_close(). */
int  sf_open(sf_probe *p, const void *blob, size_t n);             /* 0 on setup failure (skip) */
/* CreateFontFace with explicit parameters. Replaces p->face. */
HRESULT sf_face(sf_probe *p, DWRITE_FONT_FACE_TYPE type, UINT32 index, DWRITE_FONT_SIMULATIONS sim);
/* sf_open + CreateFontFace(analyzed type or TRUETYPE when analysis failed). */
int  sf_open_face(sf_probe *p, const void *blob, size_t n);
void sf_close(sf_probe *p);
dwx_mem_stream *sf_stream(sf_probe *p);                            /* the stream dwrite opened (NULL until first use) */
#define SF_PROBE_DEFINE(p) sf_probe p; memset(&p, 0, sizeof p)

/* Soft expectation: never fails the test; reports only when the prediction (from the decompilation) did not hold. */
void sf_pred(int ok, const char *label, const char *expr);
#define PRED(label, cond) sf_pred(!!(cond), label, #cond)
/* Print one NOTE line with the outcome of each stage. */
void sf_note(const char *label, const sf_probe *p);
/* Glyph lookup through the face: returns glyph for a code point (0 when unmapped or on failure). */
UINT16 sf_gid(IDWriteFontFace *face, UINT32 cp);

/* byte-buffer builder for hand-made tables */
typedef struct { uint8_t *p; size_t n, cap; } sf_buf;
void sf_b8(sf_buf *b, unsigned v);
void sf_b16(sf_buf *b, unsigned v);
void sf_b32(sf_buf *b, uint32_t v);
void sf_bz(sf_buf *b, size_t n);
void sf_buf_free(sf_buf *b);

/* cmap table: header + n records, each (plat, enc, subtable bytes). Subtables are appended after the records. */
typedef struct { unsigned plat, enc; const uint8_t *data; size_t len; } sf_cmap_rec;
sf_buf sf_cmap_table(const sf_cmap_rec *recs, int n);
/* subtables */
sf_buf sf_cmap0(const uint8_t map[256]);
sf_buf sf_cmap6(unsigned first, const uint16_t *gids, unsigned n);
typedef struct { unsigned start, end; int delta; } sf_seg4;   /* idRangeOffset 0 segment */
typedef struct { unsigned start, end; int delta; const uint16_t *garr; } sf_seg4g;   /* garr != NULL: idRangeOffset into a glyph array of (end-start+1) entries */
sf_buf sf_cmap4(const sf_seg4g *segs, int n, int add_terminator);
typedef struct { uint32_t start, end, gid; } sf_grp;
sf_buf sf_cmap12(const sf_grp *g, int n, int fmt /*12 or 13*/);
#endif
