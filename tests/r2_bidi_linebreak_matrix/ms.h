/* Fixtures for the r2_bidi_linebreak_matrix area: a text-analysis source with scripted slicing (the text is handed out in
 * pieces that end at fixed absolute boundaries) and a call log, and a sink that records every callback in order.
 *
 * Facts about IDWriteTextAnalyzer that the tests of this area rely on (Windows 10 build 19041 dwrite.dll):
 *   - AnalyzeLineBreakpoints (LineBreakpointContext::Analyze): length 0 returns before any source call; textPosition != 0 first
 *     asks GetTextBeforePosition(textPosition) once and looks only at the LAST UTF-16 unit of what it returns (NULL or length 0
 *     = no preceding text); the text itself is fetched through GetTextAtPosition(textPosition) and, when the first piece is
 *     shorter than the range, re-fetched piece by piece; unless the last unit is a hard-break class, GetTextAtPosition
 *     (textPosition + textLength) is asked once for the character after the range (NULL or length 0 = end of text).
 *     SetLineBreakpoints is called ONCE for the whole range.
 *   - AnalyzeBidi: GetParagraphReadingDirection is the first source call, GetTextAtPosition(textPosition) follows, then
 *     GetLocaleName(position) once per locale run (a returned length of 0 means "the rest of the range", NULL is "").
 *   - AnalyzeVerticalGlyphOrientation: GetVerticalGlyphOrientation once per source run (length 0 = rest of the range),
 *     SetGlyphOrientation once per run of identical (angle, adjusted level, sideways, rtl).
 */
#ifndef MS_H
#define MS_H
#include "dw.h"
#include "dwx.h"
#include <wchar.h>
#include <limits.h>

#define MS_MAX 2048
#define MS_EVMAX 1024
enum { MF_AT = 1, MF_BEFORE, MF_DIR, MF_LOCALE, MF_SUBST, MF_ORIENT };
typedef struct { int fn; UINT32 pos; UINT32 ret; const WCHAR *ptr; } ms_call;

typedef struct ms_src {
    const void *lpVtbl; LONG ref;
    const WCHAR *text; UINT32 len;
    UINT32 cuts[16]; int ncuts;              /* cyclic piece sizes defining absolute piece boundaries; 0 pieces = whole text */
    DWRITE_READING_DIRECTION dir;
    const WCHAR *locale; UINT32 locale_run;  /* locale pointer (may be NULL); 0 = report the rest of the text */
    int null_before, null_after;             /* GetTextBeforePosition / GetTextAtPosition(pos >= nafter_from) return NULL, 0 */
    UINT32 or_len[16]; DWRITE_VERTICAL_GLYPH_ORIENTATION or_o[16]; UINT8 or_bidi[16]; int nor; /* orientation runs from 0; last repeats */
    HRESULT fail_orient;
    IDWriteNumberSubstitution *subst; UINT32 subst_run;   /* returned (AddRef'ed) by GetNumberSubstitution; run length, 0 = rest of the text */
    ms_call log[MS_EVMAX]; int nlog;
    int n[8];                                /* per MF_* count */
} ms_src;
ms_src *ms_src_new(const WCHAR *text, UINT32 len);
void ms_src_cuts(ms_src *s, const UINT32 *cuts, int n);
void ms_src_free(ms_src *s);                 /* checks the reference count is back to 1, then frees */
int  ms_log_count(const ms_src *s, int fn, UINT32 pos);   /* occurrences of (fn,pos); pos == 0xffffffff matches any */

typedef struct { int fam; UINT32 pos, len; DWRITE_SCRIPT_ANALYSIS sa; UINT8 ex, res; DWRITE_GLYPH_ORIENTATION_ANGLE ang; UINT8 abl; BOOL sw, rtl; IDWriteNumberSubstitution *ns; } ms_ev;
enum { MK_SCRIPT = 1, MK_BREAK, MK_BIDI, MK_SUBST, MK_ORIENT };
typedef struct ms_sink {
    const void *lpVtbl; LONG ref;
    ms_ev ev[MS_EVMAX]; int nev;
    int n[8];
    DWRITE_SCRIPT_ANALYSIS script[MS_MAX]; DWRITE_LINE_BREAKPOINT brk[MS_MAX];
    UINT8 ex[MS_MAX], res[MS_MAX];
    DWRITE_GLYPH_ORIENTATION_ANGLE ang[MS_MAX]; UINT8 abl[MS_MAX]; BOOL sw[MS_MAX], rtl[MS_MAX];
    unsigned char hits[8][MS_MAX];           /* how many times each position was reported by each family */
    int overflow;
    int fail_kind; int fail_after; HRESULT fail_hr;   /* fail the Nth (1-based fail_after) callback of kind fail_kind */
    int support_v1;
} ms_sink;
ms_sink *ms_sink_new(int support_v1);
void ms_sink_free(ms_sink *k);
/* every position of [pos, pos+len) was reported exactly once by family k and nothing outside */
int ms_sink_exact(const ms_sink *k, int kind, UINT32 pos, UINT32 len);
/* callbacks of kind `kind` are ascending, contiguous and cover [pos,pos+len) */
int ms_runs_exact(const ms_sink *k, int kind, UINT32 pos, UINT32 len);

typedef struct ms_ctx { IDWriteFactory *f; IDWriteTextAnalyzer *a; IDWriteTextAnalyzer2 *a2; } ms_ctx;
int  ms_open(ms_ctx *c, int need_v2);
void ms_close(ms_ctx *c);
#define MS_BEGIN() ms_ctx c_; if (!ms_open(&c_, 0)) { ms_close(&c_); t_skip("CreateTextAnalyzer unavailable"); return; } IDWriteTextAnalyzer *an = c_.a; (void)an
#define MS_BEGIN2() ms_ctx c_; if (!ms_open(&c_, 1)) { ms_close(&c_); t_skip("IDWriteTextAnalyzer2 unavailable"); return; } IDWriteTextAnalyzer *an = c_.a; IDWriteTextAnalyzer2 *an2 = c_.a2; (void)an; (void)an2
#define MS_END() ms_close(&c_)

enum { MA_SCRIPT = 1, MA_BIDI, MA_BREAK, MA_NUMSUB };
HRESULT ms_do(IDWriteTextAnalyzer *an, int kind, ms_src *s, UINT32 pos, UINT32 len, ms_sink *k);
HRESULT ms_orient(IDWriteTextAnalyzer2 *a2, ms_src *s, UINT32 pos, UINT32 len, ms_sink *k);

/* Piece-size patterns used by the slicing matrices (0 terminated). */
extern const UINT32 ms_patterns[][6];
extern const int ms_npatterns;
#define BRK_BEFORE(b) ((int)(b).breakConditionBefore)
#define BRK_AFTER(b)  ((int)(b).breakConditionAfter)
#define BC_NEUTRAL DWRITE_BREAK_CONDITION_NEUTRAL
#define BC_CAN     DWRITE_BREAK_CONDITION_CAN_BREAK
#define BC_MAYNOT  DWRITE_BREAK_CONDITION_MAY_NOT_BREAK
#define BC_MUST    DWRITE_BREAK_CONDITION_MUST_BREAK
#define LEN(s) ((UINT32)wcslen(s))
#define LTR DWRITE_READING_DIRECTION_LEFT_TO_RIGHT
#define RTL DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
#endif
