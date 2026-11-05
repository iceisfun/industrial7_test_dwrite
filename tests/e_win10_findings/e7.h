/* e_win10_findings: regression guards for behaviours of Windows 10 22H2 dwrite.dll that earlier passes had to learn the hard way.
 * Every expectation below was probed on a real Windows 10 machine (standard and administrator tokens) before it was pinned.
 * Each group ends with a negative control: a check on the neighbouring behaviour that must differ, so a test that
 * silently turned vacuous (or an implementation that applies one rule everywhere) is caught.
 * Fonts are synthetic (ttfgen) unless a test says otherwise, so nothing depends on installed fonts. */
#ifndef E7_H
#define E7_H
#include "dwx.h"

/* A synthetic family served from a private isolated factory through a custom collection. */
typedef struct e7_fam {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; IDWriteFontFamily *fam;
    UINT32 count; int w[16], st[16], sims[16];
} e7_fam;
typedef struct { int weight, italic; } e7_face;
/* family built from faces (weight, italic); returns 1 on success, else a failed CHECK / skip */
int  e7_fam_open(e7_fam *m, const char *family, const e7_face *faces, int n);
void e7_fam_close(e7_fam *m);
/* number of members with this (weight, style, simulations); style: 0 normal 1 oblique 2 italic */
int  e7_fam_has(const e7_fam *m, int w, int st, int sims);
/* A family with the default synthetic glyphs for text-layout tests: returns a text format over a private collection. */
typedef struct e7_lay {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; IDWriteTextFormat *fmt;
} e7_lay;
int  e7_lay_open(e7_lay *m, float size);
void e7_lay_close(e7_lay *m);
IDWriteTextLayout *e7_layout(e7_lay *m, const WCHAR *text, float w, float h);
#endif
