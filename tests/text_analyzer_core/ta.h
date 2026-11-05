/* Shared fixtures for the text_analyzer_core area (IDWriteTextAnalyzer driven with our own analysis source and sink).
 *
 * The analyzer is reached through DWriteCreateFactory -> IDWriteFactory::CreateTextAnalyzer.  The analysis source/sink are the
 * recording doubles of common/dwx; the text pointer handed to a source is NOT copied, so every fixture string outlives its source.
 *
 * Facts every test in this area relies on (observed behaviour of the four Analyze* entry points):
 *   - source == NULL, sink == NULL or textPosition + textLength overflowing 32 bits  => E_INVALIDARG, no callback of any kind
 *   - everything else succeeds with S_OK unless a callback (source or sink) fails; the callback's HRESULT stops the walk
 *   - line breakpoints: ONE sink call covers the whole range; zero length returns before any source call
 */
#ifndef TA_H
#define TA_H
#include "dwx.h"
#include <wchar.h>
#include <float.h>
#include <limits.h>
#include <math.h>

enum { TA_SCRIPT = 1, TA_BIDI, TA_NUMSUB, TA_BREAK };
#define BC_NEUTRAL   DWRITE_BREAK_CONDITION_NEUTRAL
#define BC_CAN       DWRITE_BREAK_CONDITION_CAN_BREAK
#define BC_MAYNOT    DWRITE_BREAK_CONDITION_MAY_NOT_BREAK
#define BC_MUST      DWRITE_BREAK_CONDITION_MUST_BREAK

/* Analyzer fixture: ta_open() on the shared factory; test is skipped when the factory/analyzer is unavailable. */
#define TA_BEGIN() \
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; \
    if (!f || FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) { t_skip("CreateTextAnalyzer prerequisite failed"); return; } \
    (void)f
#define TA_END() DW_RELEASE(an)

/* Run one analysis kind (TA_*) over [pos, pos+len) of the source into the sink. */
HRESULT ta_do(IDWriteTextAnalyzer *an, int kind, dwx_ana_src *s, UINT32 pos, UINT32 len, dwx_ana_sink *k);
/* Convenience: text of wcslen units analysed from 0, fresh source/sink created, result returned; caller releases both (dwx_release). */
HRESULT ta_run(IDWriteTextAnalyzer *an, int kind, const WCHAR *text, dwx_ana_src **ps, dwx_ana_sink **pk);
HRESULT ta_run_n(IDWriteTextAnalyzer *an, int kind, const WCHAR *text, UINT32 n, UINT32 chunk, dwx_ana_src **ps, dwx_ana_sink **pk);
void ta_free(dwx_ana_src *s, dwx_ana_sink *k);

/* ISO 15924 numeric code of a script analysis (via IDWriteTextAnalyzer1::GetScriptProperties); 0 when unavailable. */
UINT32 ta_iso(IDWriteTextAnalyzer *an, DWRITE_SCRIPT_ANALYSIS sa);
/* Reports whether the runs of one family are ascending, contiguous and cover exactly [pos, pos+len). */
int ta_runs_exact(const dwx_ana_sink *k, char fam, UINT32 pos, UINT32 len);
/* bitfield accessors for DWRITE_LINE_BREAKPOINT (the dwx sink stores them verbatim) */
#define BRK_BEFORE(b) ((int)(b).breakConditionBefore)
#define BRK_AFTER(b)  ((int)(b).breakConditionAfter)

/* Synthetic font fixtures (see common/ttfgen.h default font): 'A'..'Z' gid 2..27, 'a'..'z' 28..53, '0'..'9' 54..63, space 1,
 * every advance 1000 of UPM 2048, notdef 1200.  The "shaping" variant adds
 *   GSUB liga:  f i -> gid 65 (the "round" glyph), the single pair f+i
 *   GPOS kern:  A V -100, V A -100, T o -150, A Y -80 (design units; applied to the advance of the first glyph)
 * 'f' = 33, 'i' = 36, 'V' = 23, 'A' = 2, 'T' = 21, 'o' = 42, 'Y' = 26. */
typedef struct ta_font { dwx_font h; IDWriteFontFace *face; int ok; } ta_font;
int  ta_font_open(ta_font *t, IDWriteFactory *f, int shaping);    /* 1 on success */
void ta_font_close(ta_font *t);
#define TA_FONT_BEGIN(name, shaping) ta_font name; memset(&name, 0, sizeof name); \
    if (!ta_font_open(&name, f, shaping)) { ta_font_close(&name); TA_END(); t_skip("synthetic font unavailable"); return; }

/* Full shaping result for one run. */
#define TA_MAXG 512
typedef struct ta_shaped {
    HRESULT hr; UINT32 len, nglyph, actual; UINT16 cluster[TA_MAXG]; DWRITE_SHAPING_TEXT_PROPERTIES tp[TA_MAXG];
    UINT16 gid[TA_MAXG]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[TA_MAXG];
    float adv[TA_MAXG]; DWRITE_GLYPH_OFFSET off[TA_MAXG]; HRESULT hr_place;
    DWRITE_SCRIPT_ANALYSIS sa; const WCHAR *loc;
} ta_shaped;
/* Analyse the script of `text` (script of the first character), then shape with the given face.  maxg = glyph capacity (<= TA_MAXG). */
HRESULT ta_shape(IDWriteTextAnalyzer *an, IDWriteFontFace *face, const WCHAR *text, UINT32 len, BOOL rtl, BOOL sideways,
                 const WCHAR *locale, const DWRITE_TYPOGRAPHIC_FEATURES **feat, const UINT32 *flen, UINT32 nfeat,
                 UINT32 maxg, ta_shaped *out);
/* Placement of a shaped run (GetGlyphPlacements). */
HRESULT ta_place(IDWriteTextAnalyzer *an, IDWriteFontFace *face, const WCHAR *text, BOOL rtl, BOOL sideways, float em,
                 const DWRITE_TYPOGRAPHIC_FEATURES **feat, const UINT32 *flen, UINT32 nfeat, ta_shaped *s);
DWRITE_SCRIPT_ANALYSIS ta_script_of(IDWriteTextAnalyzer *an, const WCHAR *text, UINT32 len);

/* A bare-bones analysis source with scripted failures (stack object, never freed): GetTextAtPosition / GetTextBeforePosition
 * return fail_at / fail_before once `ok_calls` successful calls to that method have been made. */
typedef struct ta_fsrc {
    const void *lpVtbl; LONG ref; const WCHAR *text; UINT32 len;
    HRESULT fail_at, fail_before; LONG ok_at, ok_before;      /* number of calls that succeed before the failure starts */
    LONG n_at, n_before, n_dir, n_locale, n_subst;
    DWRITE_READING_DIRECTION dir; IDWriteNumberSubstitution *subst; HRESULT fail_subst;
} ta_fsrc;
void ta_fsrc_init(ta_fsrc *s, const WCHAR *text, UINT32 len);

/* a callback failure must abort the call with a failure; the exact code is only recorded (the HRESULT mapping is not visible in the observations) */
#define CHECK_HR_PROP(hr, code) do { HRESULT p_ = (hr); CHECK_HR_FAILED(p_); if (p_ != (HRESULT)(code)) t_note("callback failed with %s, analyzer returned %s", dw_hr_name((HRESULT)(code)), dw_hr_name(p_)); } while (0)

#define LEN(s) ((UINT32)wcslen(s))
#endif
