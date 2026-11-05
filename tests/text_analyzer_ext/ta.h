/* Shared helpers for the text_analyzer_ext area: IDWriteTextAnalyzer1 / IDWriteTextAnalyzer2.
 * Everything is LoadLibrary("dwrite.dll") -> GetProcAddress("DWriteCreateFactory") -> factory -> CreateTextAnalyzer -> vtable calls.
 *
 * Facts established from the Windows 10 19041 dwrite.dll decompilation (DWriteTextAnalyzer::* vtable methods) and used throughout:
 *   - Every pointer-style argument that is reported as invalid yields E_INVALIDARG (0x80070057); a too small output buffer yields
 *     0x8007007A (HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)).
 *   - Output counters / flags of several methods are cleared BEFORE the arguments are validated (so the out pointers themselves must
 *     always be valid: a NULL out pointer crashes the real DLL).
 */
#ifndef TA_H
#define TA_H
#include "dwx.h"
#include <float.h>

#define TA_E_INVALIDARG ((HRESULT)0x80070057)
#define TA_E_BUFFER     ((HRESULT)0x8007007A)

typedef struct ta_ctx {
    IDWriteFactory *f;
    IDWriteTextAnalyzer *a;          /* base interface from CreateTextAnalyzer (owned reference) */
    IDWriteTextAnalyzer1 *a1;        /* QueryInterface'd (owned reference), NULL when unsupported */
    IDWriteTextAnalyzer2 *a2;
} ta_ctx;

/* Opens the shared factory and an analyzer with both extension interfaces. Returns 1 on success. */
int ta_open(ta_ctx *c);
void ta_close(ta_ctx *c);
/* Declares a1 / a2 / a0 / f in the test body; skips the test when the interfaces are unavailable. */
#define TA_BEGIN() \
    ta_ctx c_; \
    if (!ta_open(&c_)) { ta_close(&c_); t_skip("IDWriteTextAnalyzer1/2 unavailable"); return; } \
    IDWriteFactory *f = c_.f; IDWriteTextAnalyzer *a0 = c_.a; IDWriteTextAnalyzer1 *a1 = c_.a1; IDWriteTextAnalyzer2 *a2 = c_.a2; \
    (void)f; (void)a0; (void)a1; (void)a2
#define TA_END() ta_close(&c_)

/* Script id (DWRITE_SCRIPT_ANALYSIS.script) the analyzer reports for the first character of text. 0xFFFF when AnalyzeScript fails. */
UINT16 ta_script_of(ta_ctx *c, const WCHAR *text, UINT32 len);
/* Full DWRITE_SCRIPT_ANALYSIS of position 0. */
DWRITE_SCRIPT_ANALYSIS ta_sa_of(ta_ctx *c, const WCHAR *text, UINT32 len);

static __inline DWRITE_SHAPING_GLYPH_PROPERTIES ta_gp(unsigned just, unsigned cluster_start, unsigned diacritic, unsigned zws) {
    DWRITE_SHAPING_GLYPH_PROPERTIES p; memset(&p, 0, sizeof p);
    p.justification = just; p.isClusterStart = cluster_start; p.isDiacritic = diacritic; p.isZeroWidthSpace = zws;
    return p;
}
/* DWRITE_FONT_FEATURE_TAG from four characters (little endian packing used by DirectWrite). */
#define TA_TAG(a, b, c, d) ((UINT32)(a) | ((UINT32)(b) << 8) | ((UINT32)(c) << 16) | ((UINT32)(d) << 24))

/* Bit-exact float comparison helper (also distinguishes -0 from +0 on purpose: expectations never rely on the sign of zero). */
#define TA_FEQ(a, b) ((float)(a) == (float)(b))
#define CHECK_F(a, b) t_check(TA_FEQ(a, b), __FILE__, __LINE__, #a " == " #b, "got %.9g want %.9g", (double)(a), (double)(b))

/* Reference model of ApplyCharacterSpacing (the arithmetic of the Windows implementation, see ta_spacing.c). */
void ta_model_spacing(float lead, float trail, float minadv, UINT32 textlen, UINT32 nglyphs, const UINT16 *cmap, const float *adv,
                      const DWRITE_GLYPH_OFFSET *off, const DWRITE_SHAPING_GLYPH_PROPERTIES *gp, float *oadv, DWRITE_GLYPH_OFFSET *ooff);

/* Synthetic font with a custom cmap/BASE/GSUB; closes with dwx_font_close. */
HRESULT ta_open_spec(dwx_font *h, IDWriteFactory *f, const ttf_spec *s);
#endif
