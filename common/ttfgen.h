/* Deterministic synthetic TrueType/OpenType font builder.
 *
 * ttf_build() turns a ttf_spec into an sfnt byte blob (malloc'd, release with free()).  Every byte is a pure
 * function of the spec, so expected metrics, advances, glyph indices and outlines can be computed by the test
 * and hard-checked on any Windows version (no dependence on installed fonts).
 *
 * Tables written (in tag order, 4-byte aligned, real checksums, head.checkSumAdjustment correct):
 *   head hhea maxp OS/2 hmtx cmap loca glyf name post          always (unless omitted via spec.omit)
 *   kern GPOS GSUB vhea vmtx BASE COLR CPAL gasp               when the matching spec fields are set
 * plus any raw tables in spec.raw.  Only glyf-flavoured (TrueType outline) fonts are generated.
 *
 * DEFAULT TEST FONT (ttf_default_spec / ttf_style_spec) - all numbers are in font design units, UPM = 2048:
 *   gid 0       .notdef      advance 1200, box (100,0)-(1100,1400)
 *   gid 1       space        advance 1000, no outline (empty glyph, 0 contours)
 *   gid 2..27   'A'..'Z'     advance 1000, box (100,0)-(900,1400)     (one clockwise contour, 4 on-curve points)
 *   gid 28..53  'a'..'z'     identical to the capitals
 *   gid 54..63  '0'..'9'     identical to the capitals
 *   gid 64      triangle     advance 1000, (100,0) (500,1400) (900,0), all on-curve
 *   gid 65      round        advance 1000, 8 points alternating on/off (quadratic circle-ish), see ttfgen.c
 *   gid 66      composite    advance 1000: 'A' at (0,0) + triangle at (0,1400); USE_MY_METRICS on the first
 *   gid 67      empty500     advance 500, no outline
 *   gid 68      ring         advance 1000, two contours: outer box (100,0)-(900,1400) cw, hole (300,300)-(700,1100) ccw
 *   gid 69..    (not in default font)
 *   cmap: (3,1) format 4 and (3,10) format 12: U+0020 -> 1, 'A'-'Z' -> 2.., 'a'-'z' -> 28.., '0'-'9' -> 54..
 *   metrics: hhea asc 1700 desc -400 gap 90; OS/2 typo 1600/-420/80, win 1800/450 (all different on purpose, so a
 *            test can tell which source an API reports); capHeight 1400, xHeight 1000; underline pos -200 th 100;
 *            strikeout pos 600 size 100; xAvgCharWidth 1000; fsSelection REGULAR; weight 400, width 5.
 */
#ifndef TTFGEN_H
#define TTFGEN_H
#include <stddef.h>
#include <stdint.h>

#define TTF_DEF_UPM        2048
#define TTF_DEF_ADV        1000
#define TTF_NOTDEF_ADV     1200
#define TTF_BOX_X0         100
#define TTF_BOX_X1         900
#define TTF_BOX_Y1         1400
#define TTF_GID_NOTDEF     0
#define TTF_GID_SPACE      1
#define TTF_GID_TRIANGLE   64
#define TTF_GID_ROUND      65
#define TTF_GID_COMPOSITE  66
#define TTF_GID_EMPTY500   67
#define TTF_GID_RING       68
#define TTF_DEF_NGLYPHS    69
/* default cmap glyph for a code point (0 when the default font does not map it) */
unsigned ttf_default_gid(unsigned cp);

typedef struct { short x, y; unsigned char on; } ttf_pt;                  /* on: 1 on-curve, 0 off-curve */
#define TTF_COMP_USE_MY_METRICS 0x0200
#define TTF_COMP_ROUND_XY       0x0004
#define TTF_COMP_SCALED_OFFSET  0x0800
typedef struct { unsigned short gid; short dx, dy; unsigned short flags; short scale; /* F2Dot14, 0 = none */ } ttf_comp;
typedef struct {
    unsigned short adv;                  /* hmtx advance width */
    int lsb_set; short lsb;              /* hmtx lsb; default xMin of the outline (0 when empty) */
    int npts, ncont;                     /* simple glyph: npts points, ncont contours ... */
    const ttf_pt *pts; const unsigned short *ends;   /* ... ends[] = index of the last point of each contour */
    int ncomp; const ttf_comp *comps;    /* composite glyph when ncomp > 0 (npts/ncont ignored) */
    unsigned short vadv;                 /* vmtx advance height, 0 = unitsPerEm */
    int tsb_set; short tsb;              /* vmtx top side bearing; default v_ascent - yMax */
} ttf_glyph;

typedef struct { unsigned short plat, enc, lang, id; char text[96]; } ttf_name;   /* UTF-8; stored UTF-16BE (plat 0/3) or bytes (plat 1) */
typedef struct { unsigned cp, gid; } ttf_map;
typedef struct { unsigned cp, vs; int is_default; unsigned gid; } ttf_uvs;         /* format 14 entry */
typedef struct { int format; unsigned short plat, enc; int n; const ttf_map *map; int nuvs; const ttf_uvs *uvs; } ttf_cmap;   /* format 0,4,6,12,13,14 */
typedef struct { unsigned short l, r; short v; } ttf_kernpair;
typedef struct { unsigned short from, to; } ttf_sub1;                                 /* GSUB type 1 */
typedef struct { unsigned short from; int nalt; unsigned short alt[6]; } ttf_sub3;    /* GSUB type 3 */
typedef struct { unsigned short comps[6]; int ncomp; unsigned short lig; } ttf_sub4;  /* GSUB type 4: comps[0..ncomp-1] -> lig */
#define TTF_LK_SINGLE    1
#define TTF_LK_ALTERNATE 3
#define TTF_LK_LIGATURE  4
typedef struct { char tag[5]; int type; int n; const void *data; } ttf_feature;       /* data: ttf_sub1/3/4 array of n */
typedef struct { unsigned short gid, first, n; } ttf_colr_base;
typedef struct { unsigned short gid, pal; } ttf_colr_layer;
typedef struct { unsigned short max_ppem, behavior; } ttf_gasp;
typedef struct { char tag[5]; const void *data; unsigned len; } ttf_rawtable;

#define TTF_OMIT_OS2   0x01
#define TTF_OMIT_POST  0x02
#define TTF_OMIT_NAME  0x04
#define TTF_OMIT_CMAP  0x08
#define TTF_OMIT_HHEA  0x10
#define TTF_OMIT_HMTX  0x20
#define TTF_OMIT_MAXP  0x40
#define TTF_OMIT_HEAD  0x80
#define TTF_OMIT_GLYF  0x100      /* also omits loca */

typedef struct ttf_spec {
    uint32_t sfnt_version;                       /* default 0x00010000 */
    unsigned omit;                               /* TTF_OMIT_* */
    int upm;
    int hhea_asc, hhea_desc, hhea_gap;           /* desc negative (below baseline) */
    int typo_asc, typo_desc, typo_gap;
    int win_asc, win_desc;                       /* usWinDescent is positive */
    int cap_height, x_height, x_avg_width;
    int underline_pos, underline_thick, strike_pos, strike_size;
    unsigned fs_selection;                       /* OS/2 fsSelection, bit7 = USE_TYPO_METRICS */
    unsigned mac_style;                          /* head.macStyle: 1 bold, 2 italic */
    int weight, width;                           /* usWeightClass, usWidthClass */
    unsigned fs_type;
    int os2_version;                             /* 0..5, default 4 (version 0 => 78 byte table) */
    int italic_angle_fixed;                      /* post.italicAngle, 16.16 */
    int is_fixed_pitch;
    unsigned unicode_range[4], codepage_range[2];
    unsigned first_char, last_char;              /* 0 = derive from cmap */
    unsigned head_flags;                         /* default 0x0003 */
    uint32_t revision_fixed;                     /* head.fontRevision 16.16 */
    int long_loca;                               /* force long offsets */
    int compress_hmtx;                           /* numberOfHMetrics < numGlyphs when trailing advances equal */
    const ttf_glyph *glyphs; int nglyphs;
    ttf_name names[40]; int nnames;
    const ttf_cmap *cmaps; int ncmaps;
    const ttf_kernpair *kern; int nkern;         /* legacy 'kern' table, format 0 */
    const ttf_kernpair *gpos_kern; int ngpos_kern;   /* GPOS 'kern' feature, PairPos format 1, XAdvance of the first glyph */
    const ttf_feature *gsub; int ngsub;          /* each feature = one lookup; scripts DFLT and latn */
    int vertical, v_ascent, v_descent;           /* vhea+vmtx */
    int has_base, base_ideo, base_romn;          /* BASE table, horizontal axis, script latn */
    const ttf_colr_base *colr_base; int ncolr_base;      /* COLR v0 + CPAL v0 (one palette) */
    const ttf_colr_layer *colr_layers; int ncolr_layers;
    const uint32_t *palette; int npalette;       /* 0xAARRGGBB */
    const ttf_gasp *gasp; int ngasp;
    const ttf_rawtable *raw; int nraw;           /* appended/replacing tables by tag */
} ttf_spec;

/* Fill spec with the default test font (see header comment). */
void ttf_default_spec(ttf_spec *s);
/* Default font with family/style names for weight ladder / RIBBI tests:
 *   weight 400/700 with width 5 => legacy grouping (id1 = family, id2 = Regular/Bold/Italic/Bold Italic),
 *   other => id1 = "<family> <StyleWord>", id2 = Regular/Italic, id16 = family, id17 = style word.
 * Names are written for (3,1,0x409), (1,0,0) and (0,3,0); full name id4, unique id3, postscript id6 (no spaces). */
void ttf_style_spec(ttf_spec *s, const char *family, int weight, int width, int italic);
/* Replace the name set with the standard four names (ids 1,2,4,6) in (3,1,0x409) and (1,0,0). */
void ttf_set_names(ttf_spec *s, const char *family, const char *sub, const char *full, const char *ps);
void ttf_clear_names(ttf_spec *s);
void ttf_add_name(ttf_spec *s, unsigned plat, unsigned enc, unsigned lang, unsigned id, const char *utf8);
/* Static default glyph/cmap storage (read-only), for specs that extend the default font. */
const ttf_glyph *ttf_default_glyphs(int *n);
const ttf_cmap *ttf_default_cmaps(int *n);
/* A family weight ladder: weights[] = 100..900 step 100; style word for a CSS-like weight ("Thin","Light",...). */
const char *ttf_weight_word(int weight);

/* Build the font; returns a malloc'd blob (NULL on invalid spec), *size set. */
uint8_t *ttf_build(const ttf_spec *s, size_t *size);
/* Convenience: default font with a style. */
uint8_t *ttf_build_styled(const char *family, int weight, int width, int italic, size_t *size);
uint8_t *ttf_build_default(size_t *size);
/* Collection (ttcf v1.0) of standalone sfnt blobs; tables are copied (not shared). */
uint8_t *ttf_build_ttc(const uint8_t *const *blobs, const size_t *sizes, int n, size_t *size);
/* WOFF 1.0 wrapper with stored (uncompressed) tables. */
uint8_t *ttf_wrap_woff(const uint8_t *sfnt, size_t len, size_t *size);
/* Locate a table in a blob (font index 0 for collections); returns pointer inside blob or NULL. */
const uint8_t *ttf_find_table(const uint8_t *blob, size_t len, const char *tag, unsigned *tlen);

/* Corruption: each returns a malloc'd modified copy (size via *size). */
enum {
    TTF_CORRUPT_TRUNCATE,        /* param = new length */
    TTF_CORRUPT_BAD_MAGIC,       /* sfnt version replaced with 'XXXX' */
    TTF_CORRUPT_NUMTABLES,       /* param = new numTables (directory count) */
    TTF_CORRUPT_TABLE_OFFSET,    /* param = table index; its offset points beyond the end of the file */
    TTF_CORRUPT_TABLE_LENGTH,    /* param = table index; its length extends beyond the end of the file */
    TTF_CORRUPT_TABLE_CHECKSUM,  /* param = table index; its checksum flipped */
    TTF_CORRUPT_HEAD_MAGIC,      /* head.magicNumber replaced */
    TTF_CORRUPT_ZERO_TABLE,      /* param = table index; length set to 0 */
    TTF_CORRUPT_ZERO_FILL        /* param = table index; table bytes zeroed (directory intact) */
};
uint8_t *ttf_corrupt(const uint8_t *blob, size_t len, int kind, size_t param, size_t *size);
#endif
