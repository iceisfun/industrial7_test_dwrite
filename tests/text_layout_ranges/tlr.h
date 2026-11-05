/* Shared fixtures for the text_layout_ranges area: per-range formatting on IDWriteTextLayout (SetFont*, SetUnderline, SetStrikethrough,
 * SetDrawingEffect, SetInlineObject, SetTypography, SetLocaleName, SetFontCollection, SetCharacterSpacing, SetPairKerning) and the
 * range semantics of the matching Get* methods.
 *
 * Everything is reached through DWriteCreateFactory (LoadLibrary/GetProcAddress in common/dw.c) and COM vtables.
 *
 * Deterministic fonts live in a private collection (design units, UPM 2048; at font size 2048 one design unit is one DIP):
 *   family "DwTest"    Regular 400 advance 1000, Bold 700 advance 1500, Italic 800, Bold Italic 1200,
 *                      Condensed (stretch 3, via typographic family names) advance 600
 *   family "DwNarrow"  Regular advance 500
 *   family "DwWide"    Regular advance 2000
 * Letters, digits and the space all share the advance of their face; .notdef is 1200 in every face.
 *
 * Range model verified in the decompilation (MutableTextLayout / RunIndexer<RunFormat> / RunVector<RunFormat>):
 *   - every Set* stores its value in one table of runs covering [0, 0xFFFFFFFF); nothing is clipped to the text length;
 *   - a run is a maximal span of positions whose COMPLETE format is equal, so the range reported by any Get* is the run of the
 *     whole format and is split by a change of ANY property;
 *   - before the first change the single run is {0, 0xFFFFFFFF}; Get* never fails and never validates the position.
 */
#ifndef TLR_H
#define TLR_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define TLR_FAM   L"DwTest"
#define TLR_NARROW L"DwNarrow"
#define TLR_WIDE  L"DwWide"
#define TLR_LOC   L"en-us"
#define TLR_ALL   0xFFFFFFFFu
#define TLR_EINVALIDARG ((HRESULT)0x80070057)
#define TLR_ENOBUF      ((HRESULT)0x8007007A)

typedef struct tlr_fx {
    IDWriteFactory *f;
    dwx_mem_loader *fl;
    dwx_coll_loader *cl;
    IDWriteFontCollection *coll;
} tlr_fx;
int  tlr_open(tlr_fx *x);
void tlr_close(tlr_fx *x);
#define TLR_BEGIN() tlr_fx X; memset(&X, 0, sizeof X); if (!tlr_open(&X)) { tlr_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define TLR_END()   tlr_close(&X)

IDWriteTextFormat *tlr_format(tlr_fx *x, const WCHAR *family, float size);
IDWriteTextLayout *tlr_layout_(tlr_fx *x, const WCHAR *family, const WCHAR *text, float size);
/* Layout over `text` (DwTest, `size`, maxWidth/maxHeight 100000). Skips the test and releases the fixture on failure. */
#define TLR_LAYOUT(var, text, size) IDWriteTextLayout *var = tlr_layout_(&X, TLR_FAM, text, size); if (!var) { TLR_END(); t_skip("CreateTextLayout prerequisite failed"); return; }
#define TLR_LAYOUT_FAM(var, fam, text, size) IDWriteTextLayout *var = tlr_layout_(&X, fam, text, size); if (!var) { TLR_END(); t_skip("CreateTextLayout prerequisite failed"); return; }

void *tlr_qi(IUnknown *u, const IID *iid, const char *name);
#define TLR_QI(var, TYPE, obj) TYPE *var = (TYPE *)tlr_qi((IUnknown *)(obj), &IID_##TYPE, #TYPE); if (!var) { return; }

#define RNG(s, l) ((DWRITE_TEXT_RANGE){ (s), (l) })
#define POISON_RANGE ((DWRITE_TEXT_RANGE){ 0xdeadbeefu, 0xcafef00du })
#define CHECK_RANGE(r, s, l) t_check((r).startPosition == (UINT32)(s) && (r).length == (UINT32)(l), __FILE__, __LINE__, #r " == {" #s ", " #l "}", \
                                     "got {%u, %u} want {%u, %u}", (unsigned)(r).startPosition, (unsigned)(r).length, (unsigned)(UINT32)(s), (unsigned)(UINT32)(l))
#define CHECK_WEQ(a, b) do { const WCHAR *a_ = (a), *b_ = (b); t_check(!wcscmp(a_, b_), __FILE__, __LINE__, #a " == " #b, "wide string mismatch: got \"%ls\" want \"%ls\"", a_, b_); } while (0)
/* Locale tags: compare case-insensitively (casing normalisation is implementation dependent, noted separately). */
#define CHECK_WEQI(a, b) do { const WCHAR *a_ = (a), *b_ = (b); t_check(!_wcsicmp(a_, b_), __FILE__, __LINE__, #a " ~= " #b, "locale mismatch: got \"%ls\" want \"%ls\"", a_, b_); \
                              if (wcscmp(a_, b_)) t_note("locale casing: got \"%ls\" for \"%ls\"", a_, b_); } while (0)
/* Position 0xFFFFFFFF is valid on Windows; the emulator layer used for headless validation faults on it. */
#define TLR_SKIP_MAXPOS(pos) if (t_is_wine() && (pos) == 0xFFFFFFFFu) continue;
static __inline int tlr_fbits(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }
#define CHECK_FBITS(a, b) t_check(tlr_fbits((float)(a), (float)(b)), __FILE__, __LINE__, #a " bit-identical to " #b, "got %g want %g", (double)(a), (double)(b))

/* ---- getters: each requires S_OK, poisons the outputs first, and returns the value; the range goes to *r (may be NULL: not passed as NULL). */
DWRITE_FONT_WEIGHT  tlr_weight(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
DWRITE_FONT_STYLE   tlr_style(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
DWRITE_FONT_STRETCH tlr_stretch(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
float               tlr_size(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
BOOL                tlr_underline(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
BOOL                tlr_strike(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
/* family / locale as wide strings (static ring of buffers, valid until 8 more calls); length reported by the *Length call is verified against wcslen. */
const WCHAR *tlr_family(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
const WCHAR *tlr_locale(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
/* Interface-valued getters: return the pointer (AddRef'd by the call, caller releases), require S_OK. */
IUnknown *tlr_effect(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
IDWriteInlineObject *tlr_inline(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
IDWriteTypography *tlr_typo(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
IDWriteFontCollection *tlr_coll(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r);
#define TLR_SENT(T) ((T)(ULONG_PTR)0x1)
/* Full-length widths of the layout. */
float tlr_width(IDWriteTextLayout *l);
float tlr_height(IDWriteTextLayout *l);
UINT32 tlr_lines(IDWriteTextLayout *l);

/* Reference model of a position-indexed run table, used by the model based tests. */
#define TLR_MODEL_N 160
typedef struct tlr_cell { int weight, style, stretch; float size; int ul, st; int effect; } tlr_cell;
typedef struct tlr_model { tlr_cell c[TLR_MODEL_N]; tlr_cell def; } tlr_model;
void tlr_model_init(tlr_model *m, const tlr_cell *def);
/* Run of cells equal to the cell at `pos` (positions >= TLR_MODEL_N hold the default and extend to 0xFFFFFFFF). */
void tlr_model_run(const tlr_model *m, UINT32 pos, tlr_cell *value, DWRITE_TEXT_RANGE *r);
int  tlr_cell_eq(const tlr_cell *a, const tlr_cell *b);
/* Deterministic PRNG. */
UINT32 tlr_rand(UINT32 *s);

/* ---- property table: the same range semantics are exercised for every per-range property ------------------------------------------
 * Each property has value index 0 = the layout default (taken from the format), 1 and 2 = distinct non-default values
 * (2 may equal 1 for boolean properties: nvals == 2). */
typedef struct tlr_env {
    tlr_fx x;
    IDWriteTextLayout *l; IDWriteTextLayout1 *l1;
    dwx_effect *eff[3]; dwx_inline *inl[3]; IDWriteTypography *typo[3]; IDWriteFontCollection *coll[3];
} tlr_env;
typedef struct tlr_prop {
    const char *name; int nvals;
    HRESULT (*set)(tlr_env *e, int v, DWRITE_TEXT_RANGE r);
    int (*get)(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r);       /* value index (-1 unknown); poisons the range and requires S_OK */
} tlr_prop;
extern const tlr_prop tlr_props[];
extern const int tlr_nprops;
int  tlr_env_open(tlr_env *e);               /* fixture + helper objects; 0 on failure */
int  tlr_env_layout(tlr_env *e, const WCHAR *text);   /* (re)creates e->l over `text`; 0 on failure */
void tlr_env_close(tlr_env *e);
extern const WCHAR *tlr_cur_text;
#define TLR_ENV_BEGIN(text) tlr_env E; tlr_cur_text = (text); memset(&E, 0, sizeof E); if (!tlr_env_open(&E) || !tlr_env_layout(&E, text)) { tlr_env_close(&E); t_skip("fixture unavailable"); return; }
#define TLR_ENV_END() tlr_env_close(&E)
#define A(n) (TLR_ALL - (UINT32)(n))        /* length of the unmodified tail starting at n */
#define FOR_PROPS(p) for (p = tlr_props; p < tlr_props + tlr_nprops; p++)
#endif
