/* Shared fixtures for the text_layout_metrics area (line, cluster and text metrics of IDWriteTextLayout).
 *
 * All objects come from DWriteCreateFactory (LoadLibrary/GetProcAddress in common/dw.c) and the COM vtables it returns.
 * A private font collection holds four synthetic families, UPM 2048, so that every expected number is exact:
 *   DwTest  letters/digits/space advance 1000, notdef 1200; win ascent 1800 / descent 450, lineGap 0
 *   DwGap   same glyphs; hhea lineGap 1000 so that the reported font lineGap is positive (read from the face, not assumed)
 *   DwAsym  win ascent 1500 / descent 700 (a different split of the same-order line height), lineGap 0
 *   DwUni   DwTest metrics plus: punctuation, combining marks (advance 0), CJK / kana (advance 2000), soft hyphen (500),
 *           ZWJ (0), Hebrew letters (1000), Hangul jamo (1000), astral plane letters (1500), hyphens (600).
 * At font size 16 one DwTest letter is 7.8125 DIPs and a line is 17.578125 DIPs (all exact binary fractions).
 */
#ifndef TLM_H
#define TLM_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define FAM_TEST  L"DwTest"
#define FAM_GAP   L"DwGap"
#define FAM_ASYM  L"DwAsym"
#define FAM_UNI   L"DwUni"
#define LOC       L"en-us"
#define SZ        16.0f
#define UPM       2048.0f
/* design units -> DIPs at size SZ */
#define U16(u)    ((float)(u) * 16.0f / 2048.0f)
#define CW        U16(1000)     /* 7.8125   one letter */
#define LH        U16(2250)     /* 17.578125 line height of DwTest */
#define ASC       U16(1800)     /* 14.0625 */
#define DESC      U16(450)      /* 3.515625 */
#define EPS       0.002f
#define NEAR(a, b) CHECK_NEAR((a), (b), EPS)

typedef struct tm_fx {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; UINT32 key[4];
} tm_fx;
int  tm_open(tm_fx *x);
void tm_close(tm_fx *x);
#define TM_BEGIN() tm_fx X; memset(&X, 0, sizeof X); if (!tm_open(&X)) { tm_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define TM_END()   tm_close(&X)

IDWriteTextFormat *tm_format(tm_fx *x, const WCHAR *family, float size);
/* Layout over family/size; NULL on failure (callers use TM_L which skips). */
IDWriteTextLayout *tm_layout_(tm_fx *x, const WCHAR *family, const WCHAR *text, UINT32 len, float size, float w, float h);
#define TM_L(var, text, w, h) IDWriteTextLayout *var = tm_layout_(&X, FAM_TEST, text, (UINT32)wcslen(text), SZ, w, h); if (!var) { TM_END(); t_skip("CreateTextLayout failed"); return; }
#define TM_LF(var, fam, text, w, h) IDWriteTextLayout *var = tm_layout_(&X, fam, text, (UINT32)wcslen(text), SZ, w, h); if (!var) { TM_END(); t_skip("CreateTextLayout failed"); return; }
#define TM_LS(var, fam, text, size, w, h) IDWriteTextLayout *var = tm_layout_(&X, fam, text, (UINT32)wcslen(text), size, w, h); if (!var) { TM_END(); t_skip("CreateTextLayout failed"); return; }

#define MAXL 96
typedef struct { DWRITE_LINE_METRICS v[MAXL]; UINT32 n; } tm_lines;
typedef struct { DWRITE_CLUSTER_METRICS v[MAXL * 4]; UINT32 n; } tm_clusters;
/* Protocol-correct fetch (NULL,0 probe then exact buffer); returns 1 when everything succeeded. */
int tm_get_lines(IDWriteTextLayout *l, tm_lines *o);
int tm_get_clusters(IDWriteTextLayout *l, tm_clusters *o);
void tm_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m);

/* Font metrics of a family of the fixture collection (through IDWriteFont::GetMetrics); 1 on success. */
int tm_font_metrics(tm_fx *x, const WCHAR *family, DWRITE_FONT_METRICS *m);

void *tm_qi(IUnknown *u, const IID *iid, const char *name);
#define TM_QI(var, TYPE, obj) TYPE *var = (TYPE *)tm_qi((IUnknown *)(obj), &IID_##TYPE, #TYPE); if (!var) { TM_END(); return; }

/* Sum helpers */
float tm_sum_cluster_width(const tm_clusters *c);
UINT32 tm_sum_cluster_len(const tm_clusters *c);
UINT32 tm_sum_line_len(const tm_lines *l);
float tm_sum_line_height(const tm_lines *l);
#define RNG(s, l) ((DWRITE_TEXT_RANGE){ (s), (l) })
static __inline int tm_fbits(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }
#endif
