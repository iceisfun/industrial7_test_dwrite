/* Fixtures for the localized_strings area.
 *
 * Fonts are synthetic: the default test font with a hand-built 'name' table (any platform / encoding / language /
 * name id, arbitrary raw bytes), loaded through the in-memory loader. Every IDWriteLocalizedStrings is then
 * obtained through one of several entry points ("sources") so the same protocol checks run against all of them.
 */
#ifndef LSFX_H
#define LSFX_H
#include "dwx.h"

/* ---- name table records ------------------------------------------------------------------------- */
typedef struct {
    unsigned short plat, enc, lang, id;
    int kind;                       /* 0: raw bytes as stored; 1: host UTF-16 (wchar_t) stored big-endian */
    const void *p; size_t len;      /* len in bytes of the stored (kind 0) or host (kind 1) data */
} ls_rec;
#define RW(pl, en, la, id, ws)   { pl, en, la, id, 1, ws, sizeof(ws) - sizeof(WCHAR) }
#define RB(pl, en, la, id, bs)   { pl, en, la, id, 0, bs, sizeof(bs) - 1 }
#define RD(pl, en, la, id, ptr, n, kind) { pl, en, la, id, kind, ptr, n }
uint8_t *ls_name_table(const ls_rec *r, int n, size_t *len);      /* format 0, records sorted, malloc'd */

/* English platform-3 names that make the font a well formed single-face family "LsFam" (ids 1,2,4,6). */
#define LS_FAMILY L"LsFam"
extern const ls_rec ls_base_recs[4];

/* Font blob: default synthetic font + the given name records (no implicit names). malloc'd. */
uint8_t *ls_font_blob(const ls_rec *r, int n, size_t *size);

/* ---- fixtures -------------------------------------------------------------------------------------- */
typedef struct {
    dwx_font h;                                   /* loader + file + face */
    IDWriteFontFace *face; IDWriteFontFace3 *face3;
    /* collection path (only when opened with ls_open_coll) */
    dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFactory *f;
    IDWriteFontCollection *coll; IDWriteFontFamily *fam; IDWriteFont *font;
    IDWriteFontFace *cface; IDWriteFontFace3 *cface3;
    int is_coll;
} ls_fx;
/* Face-only: loader+file+face3. `base` != 0 prepends ls_base_recs. Returns S_OK or a failing HRESULT (skip). */
HRESULT ls_open_face(ls_fx *x, IDWriteFactory *f, int base, const ls_rec *r, int n);
/* Collection with one family/one font (family from the supplied records). */
HRESULT ls_open_coll(ls_fx *x, IDWriteFactory *f, int base, const ls_rec *r, int n);
void ls_close(ls_fx *x);
/* Standard multi-locale fixture (see lsfx.c: ls_multi_recs). */
HRESULT ls_open_multi_coll(ls_fx *x, IDWriteFactory *f);
HRESULT ls_open_multi_face(ls_fx *x, IDWriteFactory *f);

typedef struct { const char *tag; unsigned lcid; const WCHAR *fam, *sub; } ls_loc;
extern const ls_loc ls_multi[5];       /* en-us, de-de, fr-fr, ja-jp, ru-ru in that name-record order */
#define LS_NMULTI 5

/* ---- sources of IDWriteLocalizedStrings ------------------------------------------------------------ */
enum {
    LS_S_FACE_FAMILY, LS_S_FACE_FACE, LS_S_FACE_INFO_WIN32FAM, LS_S_FAMILY_FAMILY, LS_S_FONT_FACE,
    LS_S_FONT_INFO_WIN32FAM, LS_S_FONT_INFO_FULL, LS_S_FACE6_FAMILY, LS_S_FACE6_FACE, LS_S_COUNT
};
const char *ls_src_name(int src);
/* Obtains strings from the multi collection fixture. Returns S_OK (and *out != NULL), S_FALSE when the source is
 * unsupported on this system (e.g. IDWriteFontFace6), or the failing HRESULT. */
HRESULT ls_get_src(ls_fx *x, int src, IDWriteLocalizedStrings **out);
/* Expected English strings per source for the multi fixture (en-us entry). */
const WCHAR *ls_src_expect_en(int src);
/* Expected number of entries per source for the multi fixture. */
unsigned ls_src_expect_count(int src);

/* ---- helpers for reading strings ----------------------------------------------------------------------- */
WCHAR *ls_dup_string(IDWriteLocalizedStrings *s, UINT32 i);       /* malloc'd, NULL on failure */
WCHAR *ls_dup_locale(IDWriteLocalizedStrings *s, UINT32 i);
/* index of locale (case-insensitive FindLocaleName), -1 when absent */
int ls_index_of(IDWriteLocalizedStrings *s, const WCHAR *loc);
/* en-us string (malloc'd) or NULL */
WCHAR *ls_en(IDWriteLocalizedStrings *s);
int ls_wcsieq(const WCHAR *a, const WCHAR *b);                    /* ordinal ignore case equality */
/* Full invariant sweep over a strings object (all four index methods, buffers, find); returns number of problems. */
int ls_sweep(IDWriteLocalizedStrings *s, const char *what);
#define LS_NEED_FACTORY() IDWriteFactory *f = dw_shared(); if (!f) { t_skip("DWriteCreateFactory unavailable"); return; }
#define LS_NEED_MULTI(x) ls_fx x; memset(&x, 0, sizeof x); \
    { HRESULT hr_ = ls_open_multi_coll(&x, f); if (FAILED(hr_)) { ls_close(&x); t_skip("multi fixture: %s", dw_hr_name(hr_)); return; } }
#define LS_NEED_FACE(x, base, recs) ls_fx x; memset(&x, 0, sizeof x); \
    { HRESULT hr_ = ls_open_face(&x, f, base, recs, (int)(sizeof(recs) / sizeof((recs)[0]))); if (FAILED(hr_) || !x.face3) { ls_close(&x); t_skip("face fixture: %s", dw_hr_name(hr_)); return; } }
#endif
