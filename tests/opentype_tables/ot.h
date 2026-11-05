/* Byte-level OpenType parsing used as the oracle by the opentype_tables area, plus small fixtures. */
#ifndef OT_H
#define OT_H
#include "dwx.h"

#define BE16(p) ((uint16_t)(((const uint8_t *)(p))[0] << 8 | ((const uint8_t *)(p))[1]))
#define BE16S(p) ((int16_t)BE16(p))
#define BE32(p) ((uint32_t)(((const uint8_t *)(p))[0] << 24 | ((const uint8_t *)(p))[1] << 16 | ((const uint8_t *)(p))[2] << 8 | ((const uint8_t *)(p))[3]))

/* Little-endian packed tag as DirectWrite takes it (DWRITE_MAKE_OPENTYPE_TAG). */
#define TG(s) DW_TAG((s)[0], (s)[1], (s)[2], (s)[3])
static __inline UINT32 ot_tag(const char *s) { return TG(s); }
void ot_tagstr(UINT32 tag_le, char out[5]);        /* printable name of a LE tag */

typedef struct { UINT32 tag; uint32_t sum, off, len; } ot_rec;     /* tag: little-endian packing of the file bytes */
typedef struct { ot_rec r[96]; int n; uint32_t sfnt_off; uint32_t version; } ot_dir;
/* Parse the table directory of font `index` of a blob (sfnt or ttcf). Returns 0 on success. */
int ot_parse_dir(const uint8_t *blob, size_t len, uint32_t index, ot_dir *d);
const ot_rec *ot_find(const ot_dir *d, UINT32 tag_le);             /* first match in directory order */
uint32_t ot_checksum(const uint8_t *p, uint32_t len);              /* sum of big-endian ULONGs, zero padded */
int ot_ttc_count(const uint8_t *blob, size_t len);                 /* ttcf numFonts or -1 */

/* ---- cmap ---- */
/* Look up cp in the subtable (plat,enc). Returns -2 if the subtable is absent, otherwise the glyph id (0 = unmapped). */
long ot_cmap_lookup(const uint8_t *cmap, size_t len, int plat, int enc, uint32_t cp);
/* Pick the Unicode subtable in the order (3,10) (0,4) (0,6) (3,1) (0,3) (0,2) (0,1) (0,0); returns 0 if none, sets plat/enc. */
int ot_cmap_pick(const uint8_t *cmap, size_t len, int *plat, int *enc);
int ot_cmap_has(const uint8_t *cmap, size_t len, int plat, int enc, int *format);

/* ---- glyph metrics ---- */
typedef struct { int ok; int adv, lsb; int has_outline; int x0, y0, x1, y1; int ncont; } ot_glyph;
typedef struct { const uint8_t *head, *hhea, *maxp, *hmtx, *loca, *glyf; uint32_t hmtx_len, loca_len, glyf_len; int nglyphs, nhm, loca_fmt, upm; } ot_font;
/* Gather tables from a directory-parsed blob. */
int ot_font_init(ot_font *f, const uint8_t *blob, size_t len, const ot_dir *d);
int ot_glyph_get(const ot_font *f, int gid, ot_glyph *g);

/* ---- name ---- */
/* Find a name record (plat,enc,lang,id); copies the raw string bytes (big-endian UTF-16 for plat 0/3) into out; returns length or -1. */
int ot_name_get(const uint8_t *name, size_t len, int plat, int enc, int lang, int id, uint8_t *out, int cap);
int ot_name_count(const uint8_t *name, size_t len);

/* ---- kern format 0 (first MS-version subtable with coverage 0x0001); returns 1 if found, value in *v ---- */
int ot_kern_find(const uint8_t *kern, size_t len, unsigned l, unsigned r, int *v);

/* ---- fixtures ---- */
/* One synthetic font opened through the memory loader (default factory). */
typedef struct synth { dwx_font h; uint8_t *blob; size_t n; ot_dir dir; HRESULT hr; LONG base_out; } synth;
/* Build spec (or default when spec NULL), open it, parse the directory. Returns 1 on success, 0 on failure (caller still calls synth_close). */
int synth_open(synth *s, const ttf_spec *spec);
void synth_close(synth *s);
dwx_mem_stream *synth_stream(synth *s);                    /* first stream handed out by the loader (NULL if none yet) */

typedef struct tbl { const void *d; UINT32 n; void *ctx; BOOL ex; HRESULT hr; } tbl;
tbl tget(IDWriteFontFace *face, UINT32 tag_le);
void trel(IDWriteFontFace *face, tbl *t);

/* Whole file bytes of a face's (first) font file via the file's own loader/stream. malloc'd, NULL on failure. */
uint8_t *face_file_bytes(IDWriteFontFace *face, size_t *len);
/* Walk every table of a directory and compare TryGetFontTable output with the file bytes; returns number of mismatching tables. */
int compare_all_tables(IDWriteFontFace *face, const uint8_t *blob, size_t len, const ot_dir *d, int *ntables);

#define NEED_F() IDWriteFactory *f = dw_shared(); if (!f) { t_skip("DWriteCreateFactory unavailable"); return; }
#define NEED_SYNTH(s) synth s; memset(&s, 0, sizeof s); \
    if (!synth_open(&s, NULL)) { t_skip("synthetic font open failed: %s", dw_hr_name(s.hr)); synth_close(&s); return; }
#endif
