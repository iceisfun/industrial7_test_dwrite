/* Shared helpers for the r2_typographic_features area: typography queries (GetTypographicFeatures / CheckTypographicFeature),
 * text complexity, script properties, justification.  Everything is
 *   LoadLibrary("dwrite.dll") -> GetProcAddress("DWriteCreateFactory") -> factory -> CreateTextAnalyzer -> vtable calls.
 *
 * Facts from the Windows 10 19041 dwrite.dll probing that the typography tests rely on:
 *   IDWriteTextAnalyzer::GetTypographicFeatures  : *actualTagCount = 0 first; fontFace NULL -> E_INVALIDARG; NULL locale == L"";
 *        then GetTypographicFeatures; result 0x8007007A when maxTagCount < *actualTagCount.
 *   GetTypographicFeatures     : fonts without GSUB and without GPOS list nothing; otherwise the cached
 *        FontFeatureCoverage (built from GSUB, then GPOS) is searched with FindScriptLanguageIndexWithFallback.
 *   GetFeatureList          : *count = number of features of the script/language entry, copies
 *        min(count, maxTagCount) tags in table order (which is ascending order of the big-endian tag, i.e. alphabetical).
 *   FindScriptLanguageIndexWithFallback          : tries the script's second tag (when it has one), its first tag, 'DFLT', 'latn';
 *        inside a script the language tag from LOCALE_SOPENTYPELANGUAGETAG (LangGetOpenTypeLanguageTag) is tried, then 'dflt'.
 *   IDWriteTextAnalyzer::CheckTypographicFeature  : featureApplies[0..glyphCount) cleared first; fontFace NULL or
 *        (glyphIndices NULL && glyphCount) -> E_INVALIDARG, otherwise S_OK.
 */
#ifndef R2_H
#define R2_H
#include "dwx.h"
#include <float.h>

#define R2_E_INVALIDARG ((HRESULT)0x80070057)
#define R2_E_BUFFER     ((HRESULT)0x8007007A)
#define RT(n) TEST(r2_typographic_features_##n)

typedef struct r2_ctx {
    IDWriteFactory *f;
    IDWriteTextAnalyzer *a;
    IDWriteTextAnalyzer1 *a1;
    IDWriteTextAnalyzer2 *a2;
} r2_ctx;
int r2_open(r2_ctx *c);
void r2_close(r2_ctx *c);
/* Declares f, a0, a1, a2 (may be NULL) and skips the test when the analyzer is unavailable. a1/a2 required. */
#define R2_BEGIN() \
    r2_ctx c_; \
    if (!r2_open(&c_)) { r2_close(&c_); t_skip("IDWriteTextAnalyzer1/2 unavailable"); return; } \
    IDWriteFactory *f = c_.f; IDWriteTextAnalyzer *a0 = c_.a; IDWriteTextAnalyzer1 *a1 = c_.a1; IDWriteTextAnalyzer2 *a2 = c_.a2; \
    (void)f; (void)a0; (void)a1; (void)a2
#define R2_END() r2_close(&c_)

/* Script analysis of the first character of text (script field 0xFFFF when AnalyzeScript fails). */
DWRITE_SCRIPT_ANALYSIS r2_sa_of(r2_ctx *c, const WCHAR *text, UINT32 len);

/* OpenType tag, in the little endian packing used by DWRITE_FONT_FEATURE_TAG ('k','e','r','n' -> 0x6e72656b). */
#define R2_TAG(a, b, c, d) ((UINT32)(unsigned char)(a) | ((UINT32)(unsigned char)(b) << 8) | ((UINT32)(unsigned char)(c) << 16) | ((UINT32)(unsigned char)(d) << 24))
UINT32 r2_tag(const char *s);                      /* "kern" -> packed tag */

/* ---- tiny OpenType layout (GSUB / GPOS) builder -------------------------------------------------------------------------- */
#define OTL_SINGLE    1        /* GSUB 1 (format 1, delta 0) / GPOS 1 (format 1, XAdvance) */
#define OTL_MULTIPLE  2        /* GSUB 2 / GPOS 2 pair adjustment (format 1, first glyph coverage) */
#define OTL_ALTERNATE 3        /* GSUB 3 */
#define OTL_LIGATURE  4        /* GSUB 4: each covered glyph + glyph 3 -> glyph 3 */
#define OTL_MAX_LK 16
#define OTL_MAX_FT 64
#define OTL_MAX_SC 10
typedef struct otl_lookup {
    int type;                  /* OTL_* */
    int ext;                   /* wrap each subtable in an extension lookup (GSUB 7 / GPOS 9) */
    int cov_fmt;               /* coverage table format 1 (glyph array) or 2 (ranges); 0 = 1 */
    int nsub;                  /* number of subtables the glyph list is split over (0 = 1) */
    int n; unsigned short gids[96];   /* covered glyphs, strictly ascending */
} otl_lookup;
typedef struct otl_feature { char tag[5]; int nlk; int lk[6]; } otl_feature;
typedef struct otl_langsys { char tag[5]; int req; int nf; int f[64]; } otl_langsys;   /* tag "dflt" => the script's DefaultLangSys; req -1 = none */
typedef struct otl_script { char tag[5]; int nls; otl_langsys ls[6]; } otl_script;
typedef struct otl_table {
    int nsc; otl_script sc[OTL_MAX_SC];
    int nft; otl_feature ft[OTL_MAX_FT];      /* written in the given order (not sorted): indices are stable */
    int nlk; otl_lookup lk[OTL_MAX_LK];
} otl_table;
/* Serialises a GSUB (gpos == 0) or GPOS table; returns malloc'd bytes. */
uint8_t *otl_build(const otl_table *t, int gpos, size_t *size);
/* Convenience constructors */
void otl_init(otl_table *t);
int otl_add_lookup(otl_table *t, int type, int cov_fmt, int nsub, int ext, const unsigned short *gids, int n);
int otl_add_feature(otl_table *t, const char *tag, int nlk, ...);          /* lookup indices */
int otl_add_script(otl_table *t, const char *tag);                           /* returns script index */
void otl_add_langsys(otl_table *t, int script, const char *lang, int req, int nf, ...);   /* feature indices */

/* Synthetic font = default test font with extra glyphs (all empty, advance 1000) up to nglyphs and optional raw GSUB / GPOS. */
typedef struct r2_font { dwx_font h; uint8_t *gsub, *gpos; size_t ngsub, ngpos; } r2_font;
HRESULT r2_font_open(r2_font *x, IDWriteFactory *f, int nglyphs, const otl_table *gsub, const otl_table *gpos);
void r2_font_close(r2_font *x);
/* Script id (DWRITE_SCRIPT_ANALYSIS) for a sample character. */

/* Feature listing helper: calls GetTypographicFeatures with a buffer of cap entries; returns the HRESULT. */
HRESULT r2_features(r2_ctx *c, IDWriteFontFace *face, DWRITE_SCRIPT_ANALYSIS sa, const WCHAR *loc, UINT32 cap, UINT32 *n, DWRITE_FONT_FEATURE_TAG *t);
/* 1 when the glyph is covered by the feature. */
int r2_covered(r2_ctx *c, IDWriteFontFace *face, DWRITE_SCRIPT_ANALYSIS sa, const WCHAR *loc, UINT32 tag, UINT16 gid);
/* OpenType language tag of a locale as reported by the OS (LOCALE_SOPENTYPELANGUAGETAG); 'dflt' when unavailable. */
UINT32 r2_ot_lang(const WCHAR *loc);
#endif
