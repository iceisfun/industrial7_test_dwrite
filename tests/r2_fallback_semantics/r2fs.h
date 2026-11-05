/* Shared fixtures for the r2_fallback_semantics area: IDWriteFontFallback / IDWriteFontFallback1 / IDWriteFontFallbackBuilder,
 * the system fallback, text format / layout SetFontFallback and an application-implemented IDWriteFontFallback.
 *
 * A private collection (isolated factory) holds deterministic synthetic fonts, all UPM 2048 (see common/ttfgen.h for the default
 * glyph set: gid 1 space 1000, gid 2.. letters 1000, gid 64 triangle 1000, gid 65 round 1000, gid 67 empty500 advance 500, gid 68 ring 1000):
 *   R2Main    regular   ASCII 0x20..0x7e (default gids), U+00E9 -> gid 30
 *   R2Alt     regular   ASCII (default gids)
 *   R2Cjk     regular   U+0020 -> 1, U+3042 U+3044 U+3046 -> 67 (advance 500), U+4E00 -> 64, U+4E01 -> 65, U+4E02 -> 68
 *   R2Cjk2    regular   U+0020 -> 1, same ideographs as R2Cjk but U+3042 U+3044 U+3046 -> 64, U+4E00 -> 65
 *   R2Sym     regular   U+0020 -> 1, U+2603 -> 64, U+2605 -> 65, U+2600 -> 68
 *   R2Emoji   regular   U+0020 -> 1, U+1F600 -> 64, U+1F601 -> 65, U+1F468 U+1F469 -> 68, U+200D U+FE0F -> 67, U+2764 -> 65
 *   R2Mark    regular   U+0020 -> 1, U+0300 U+0301 U+0308 U+0323 -> 67
 *   R2Ladder  weights 300/400/700/900 upright, 400/700 italic, condensed (stretch 3) 400, expanded (stretch 7) 400;
 *             every face: ASCII + U+3042 U+3044 -> 64
 *   R2Part    400 upright (ASCII) and 700 upright (ASCII + U+3042 -> 64)
 *   R2Dup     (in a second collection) U+3042 -> 65 ; the same family name in the main collection maps U+3042 -> 64
 */
#ifndef R2FS_H
#define R2FS_H
#include "dwx.h"
#include <wchar.h>
#include <float.h>

#define RT(n) TEST(r2_fallback_semantics_##n)

#define R2_MAIN   L"R2Main"
#define R2_ALT    L"R2Alt"
#define R2_CJK    L"R2Cjk"
#define R2_CJK2   L"R2Cjk2"
#define R2_SYM    L"R2Sym"
#define R2_EMOJI  L"R2Emoji"
#define R2_MARK   L"R2Mark"
#define R2_LADDER L"R2Ladder"
#define R2_PART   L"R2Part"
#define R2_DUP    L"R2Dup"

typedef struct r2_fx {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll;
    dwx_coll_loader *cl2; IDWriteFontCollection *coll2;       /* second collection: R2Dup only (U+3042 -> 65) */
} r2_fx;
int  r2_open(r2_fx *x);
void r2_close(r2_fx *x);
#define R2_BEGIN() r2_fx X; memset(&X, 0, sizeof X); if (!r2_open(&X)) { r2_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define R2_END()   r2_close(&X)

/* IDWriteFactory2 view (AddRef'd, NULL when unsupported) and a fresh fallback builder. */
IDWriteFactory2 *r2_f2(IDWriteFactory *f);
IDWriteFontFallbackBuilder *r2_builder(IDWriteFactory *f);
#define R2_NEED_B(b) IDWriteFontFallbackBuilder *b = r2_builder(f); if (!b) { R2_END(); t_skip("IDWriteFactory2::CreateFontFallbackBuilder unavailable"); return; }
/* AddMapping with one range and one target family. */
HRESULT r2_add(IDWriteFontFallbackBuilder *b, UINT32 lo, UINT32 hi, const WCHAR *target, IDWriteFontCollection *coll, const WCHAR *loc, const WCHAR *base, float scale);
IDWriteFontFallback *r2_fb(IDWriteFontFallbackBuilder *b);          /* CreateFontFallback, NULL on failure */
/* The standard mapping set used by the walk tables:
 *   [3040-30FF] [4E00-9FFF] -> R2Cjk, [2600-26FF] -> R2Sym, [1F600-1F64F] [1F468-1F469] -> R2Emoji, [0300-036F] -> R2Mark   (all scale 1, system-independent) */
HRESULT r2_add_standard(IDWriteFontFallbackBuilder *b, IDWriteFontCollection *coll);
/* Builder with the standard set -> fallback (NULL on failure); the builder is released. */
IDWriteFontFallback *r2_std_fallback(r2_fx *x);

/* Properties of an IDWriteFont. fam is the en-us (else first) family name, UTF-8. */
typedef struct { char fam[64]; DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; DWRITE_FONT_STRETCH sx; } r2_finfo;
void r2_font_info(IDWriteFont *font, r2_finfo *out);
void r2_face_info(IDWriteFontFace5 *face, r2_finfo *out);          /* family from the face; weight/style/stretch from the face */
/* Does the font / face have a glyph for code point cp. */
BOOL r2_font_has(IDWriteFont *font, UINT32 cp);
UINT16 r2_font_gid(IDWriteFont *font, UINT32 cp);                    /* glyph id via CreateFontFace + GetGlyphIndices, 0xffff on failure */

/* One MapCharacters (v0). Result record; font is owned by the caller (release with DW_RELEASE). */
typedef struct r2_res { HRESULT hr; UINT32 ml; IDWriteFont *font; float scale; r2_finfo fi; } r2_res;
typedef struct r2_args {
    UINT32 pos, len;                                    /* len == (UINT32)-1 means "to the end of the text" */
    IDWriteFontCollection *coll; const WCHAR *family;
    DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; DWRITE_FONT_STRETCH sx;
    const WCHAR *locale;                                /* source locale; NULL keeps "en-us" */
    UINT32 locale_run, chunk;                           /* dwx_ana_src.locale_run / chunk */
} r2_args;
/* Defaults: pos 0, len to end, the fixture collection and R2Main, normal/normal/normal. */
void r2_defaults(r2_args *a, const r2_fx *x);
HRESULT r2_map(IDWriteFontFallback *fb, const WCHAR *text, UINT32 textlen, const r2_args *a, r2_res *out, dwx_ana_src **src_out);

/* Walk the whole text with repeated MapCharacters calls; the result is formatted "len:Family|len:Family" ("-" for a NULL font).
 * Returns the number of segments, or -1 on a failed call / a zero mapped length / a length overrun (buf then holds what was walked).
 * *covered_first_ok is cleared when a returned font does not contain the first code point of its segment. */
int r2_walk(IDWriteFontFallback *fb, const WCHAR *text, const r2_args *a, char *buf, int cap, int *covered_first_ok);
/* Hard check that walking text yields expected (string form above). */
#define R2_WALK_EQ(fb, text, args, expected) r2_walk_check_(fb, text, args, expected, __FILE__, __LINE__)
void r2_walk_check_(IDWriteFontFallback *fb, const WCHAR *text, const r2_args *a, const char *expected, const char *file, int line);
/* Invariants for any text: segments tile the text, lengths >= 1, valid surrogate pairs never split, returned fonts cover the first
 * code point of their segment. Notes the walk string. */
void r2_walk_invariants(IDWriteFontFallback *fb, const WCHAR *text, const r2_args *a, const char *file, int line);

/* ---- application-implemented IDWriteFontFallback (v0 only) ---------------------------------------------------------------------------- */
typedef struct r2_rule { UINT32 lo, hi; const WCHAR *family; float scale; } r2_rule;
#define R2_CFB_LOG 32
typedef struct r2_cfb_call {
    UINT32 pos, len, textlen_seen; IDWriteFontCollection *coll; WCHAR family[64]; int has_family;
    DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; DWRITE_FONT_STRETCH sx;
    WCHAR text[96]; UINT32 ntext;                       /* characters [pos, pos+len) read back from the source (<= 95) */
    WCHAR locale[LOCALE_NAME_MAX_LENGTH]; UINT32 locale_len;
    UINT32 ret_ml; int ret_font;
} r2_cfb_call;
typedef struct r2_cfb {
    const void *lpVtbl; volatile LONG ref; int destroyed;
    LONG n_map, n_qi, n_addref, n_release;
    r2_rule rules[8]; int nrules; IDWriteFontCollection *font_coll;     /* rules' families are looked up in font_coll */
    UINT32 max_ml;                                      /* clamp of the mapped length per call (0 = none) */
    HRESULT fail_hr;                                    /* returned by MapCharacters when non-zero */
    r2_cfb_call log[R2_CFB_LOG]; int nlog;
} r2_cfb;
r2_cfb *r2_cfb_new(IDWriteFontCollection *font_coll);
void r2_cfb_rule(r2_cfb *c, UINT32 lo, UINT32 hi, const WCHAR *family, float scale);
void r2_cfb_free(r2_cfb *c);                            /* only after the refcount reached 0 */
#define R2_CFB(c) ((IDWriteFontFallback *)(c))

/* ---- layout helpers ---------------------------------------------------------------------------------------------------------------------- */
IDWriteTextFormat *r2_format(r2_fx *x, const WCHAR *family, float size);
IDWriteTextLayout *r2_layout(r2_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h);
dwx_renderer *r2_draw(IDWriteTextLayout *l);
/* Family (UTF-8) of the face of the n-th recorded glyph run. */
void r2_run_family(dwx_renderer *r, int n, char *buf, int cap);
/* Glyph ids of all glyph runs concatenated in call order. */
int r2_glyphs(dwx_renderer *r, unsigned short *out, int cap);
#define R2_ADV(units, size) ((float)(units) * (float)(size) / 2048.0f)
#endif
