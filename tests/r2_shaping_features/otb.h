/* Hand-built OpenType layout tables (GSUB lookup types 1,2,3,4,6; GPOS lookup types 1,2,4,6; GDEF glyph classes) for the
 * synthetic shaping fonts of this area.  Everything is little code on top of a byte buffer: every output byte is a pure
 * function of the input description, so the expected glyph ids / advances / offsets are exact. */
#ifndef OTB_H
#define OTB_H
#include <stdint.h>
#include <stddef.h>

typedef struct { uint8_t *p; size_t n; } otb_blob;

/* ---- GSUB subtables ---- */
typedef struct { unsigned short from, to; } otb_s1;
typedef struct { unsigned short from; int n; unsigned short out[8]; } otb_seq;      /* type 2 (multiple) / type 3 (alternates) */
typedef struct { unsigned short comps[6]; int ncomp; unsigned short lig; } otb_lig;  /* type 4: comps[0] is the first glyph */
otb_blob otb_gsub1_list(const otb_s1 *m, int n);                                    /* format 2 */
otb_blob otb_gsub1_delta(const unsigned short *gids, int n, int delta);             /* format 1 */
otb_blob otb_gsub2(const otb_seq *m, int n);
otb_blob otb_gsub3(const otb_seq *m, int n);
otb_blob otb_gsub4(const otb_lig *m, int n);                                        /* order inside one first glyph = preference */
/* type 6 format 3: coverage lists; backtrack[0] is the glyph nearest to the input sequence */
typedef struct { const unsigned short *g; int n; } otb_glyphset;
typedef struct { unsigned short seq, lookup; } otb_seqrec;
otb_blob otb_gsub6(const otb_glyphset *back, int nback, const otb_glyphset *in, int nin,
                   const otb_glyphset *ahead, int nahead, const otb_seqrec *rec, int nrec);

/* ---- GPOS subtables (value format bits: 1 xPlacement, 2 yPlacement, 4 xAdvance, 8 yAdvance) ---- */
typedef struct { short v[4]; } otb_val;
otb_blob otb_gpos1_one(const unsigned short *gids, int n, unsigned vf, otb_val v);                    /* format 1 */
otb_blob otb_gpos1_each(const unsigned short *gids, int n, unsigned vf, const otb_val *v);            /* format 2 */
typedef struct { unsigned short l, r; short xadv1, xadv2; } otb_pair;
otb_blob otb_gpos2_pairs(const otb_pair *p, int n);                                                    /* format 1 */
/* format 2: first glyphs with class1, second glyphs with class2, matrix [c1][c2] of first-glyph xAdvance */
typedef struct { unsigned short gid, cls; } otb_cls;
otb_blob otb_gpos2_class(const otb_cls *first, int nf, const otb_cls *second, int ns, int ncls1, int ncls2, const short *xadv);
typedef struct { short x, y; } otb_anchor;
typedef struct { unsigned short gid; unsigned short cls; otb_anchor a; } otb_mark;
typedef struct { unsigned short gid; otb_anchor a[4]; } otb_base;                                     /* one anchor per mark class */
/* type 4 (mark-to-base) and type 6 (mark-to-mark) share the layout: marks attach to bases */
otb_blob otb_gpos_mark(const otb_mark *marks, int nm, const otb_base *bases, int nb, int nclass);

/* ---- layout assembly ---- */
typedef struct { int type; unsigned flag; int nsub; otb_blob sub[4]; } otb_lookup;
typedef struct { char tag[5]; int nl; int lk[8]; } otb_feature;
typedef struct { char tag[5]; int nf; const char *f[24]; } otb_langsys;
typedef struct { char tag[5]; int nf; const char *f[24]; int nlang; otb_langsys lang[3]; } otb_script;
otb_blob otb_layout(const otb_script *sc, int nsc, const otb_feature *ft, int nft, const otb_lookup *lk, int nlk);
/* GDEF with a glyph class definition: cls[gid] = 0 none, 1 base, 2 ligature, 3 mark, 4 component */
otb_blob otb_gdef(const unsigned char *cls, int nglyphs);
void otb_free(otb_blob *b);
#endif
