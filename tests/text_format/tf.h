/* Shared fixtures for the text_format area (IDWriteTextFormat .. IDWriteTextFormat3).
 *
 * Everything reaches dwrite.dll through DWriteCreateFactory (LoadLibrary/GetProcAddress in common/dw.c) and then the COM
 * vtables of the returned objects. CreateTextFormat does not look at the font collection contents, so the property
 * surface can be tested with any family name; font-dependent behaviour (layout) is limited to relations.
 */
#ifndef TF_H
#define TF_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>

#define TF_NEED_FACTORY() IDWriteFactory *f = dw_shared(); if (!f) { t_skip("DWriteCreateFactory unavailable"); return; }

#define TF_FAM  L"Arial"
#define TF_LOC  L"en-us"

/* Compares wide strings by code unit (lstrcmpW does a locale word sort that ignores '-'). */
#define CHECK_WEQ(a, b) do { const WCHAR *a_ = (a), *b_ = (b); t_check(!wcscmp(a_, b_), __FILE__, __LINE__, #a " == " #b, "wide string mismatch: got \"%ls\" want \"%ls\"", a_, b_); } while (0)

/* Creates a format with explicit properties; skips the test on failure (the prerequisite is the thing under test elsewhere). */
static IDWriteTextFormat *tf_make(IDWriteFactory *f, const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE s,
                                  DWRITE_FONT_STRETCH st, FLOAT sz, const WCHAR *loc)
{
    IDWriteTextFormat *t = NULL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(f, fam, NULL, w, s, st, sz, loc, &t);
    if (FAILED(hr) || !t) { t_skip("CreateTextFormat prerequisite failed: %s", dw_hr_name(hr)); }
    return t;
}
static IDWriteTextFormat *tf_new(IDWriteFactory *f) { return tf_make(f, TF_FAM, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, TF_LOC); }
#define TF_NEW(var) IDWriteTextFormat *var; TF_NEED_FACTORY(); var = tf_new(f)

/* QueryInterface that skips the test when the interface is missing; caller releases. */
static void *tf_qi(IUnknown *u, const IID *iid, const char *name)
{
    void *p = NULL;
    HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    if (FAILED(hr) || !p) { t_skip("QueryInterface(%s) -> %s", name, dw_hr_name(hr)); }
    return p;
}
#define TF_QI(var, TYPE, obj) TYPE *var = (TYPE *)tf_qi((IUnknown *)(obj), &IID_##TYPE, #TYPE)

static int tf_fbits(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }
#define CHECK_FBITS(a, b) t_check(tf_fbits((float)(a), (float)(b)), __FILE__, __LINE__, #a " bit-identical to " #b, "got %g want %g", (double)(a), (double)(b))

/* Reads the family / locale through the buffer protocol; returns the HRESULT. */
static HRESULT tf_family(IDWriteTextFormat *t, WCHAR *buf, UINT32 cap) { return IDWriteTextFormat_GetFontFamilyName(t, buf, cap); }

/* Snapshot of every public property of a format, to prove a failing call changed nothing. */
typedef struct tf_state {
    DWRITE_TEXT_ALIGNMENT ta; DWRITE_PARAGRAPH_ALIGNMENT pa; DWRITE_WORD_WRAPPING ww; DWRITE_READING_DIRECTION rd; DWRITE_FLOW_DIRECTION fd;
    float tab; DWRITE_TRIMMING trim; IDWriteInlineObject *sign;
    DWRITE_LINE_SPACING_METHOD lsm; float ls, bl;
    DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE s; DWRITE_FONT_STRETCH st; float size;
    UINT32 famlen, loclen; WCHAR fam[64], loc[64];
} tf_state;
static void tf_snap(IDWriteTextFormat *t, tf_state *o)
{
    memset(o, 0, sizeof *o);
    o->ta = IDWriteTextFormat_GetTextAlignment(t); o->pa = IDWriteTextFormat_GetParagraphAlignment(t);
    o->ww = IDWriteTextFormat_GetWordWrapping(t); o->rd = IDWriteTextFormat_GetReadingDirection(t); o->fd = IDWriteTextFormat_GetFlowDirection(t);
    o->tab = IDWriteTextFormat_GetIncrementalTabStop(t);
    IDWriteTextFormat_GetTrimming(t, &o->trim, &o->sign);
    if (o->sign) IDWriteInlineObject_Release(o->sign);      /* identity is kept for comparison, the reference is not */
    IDWriteTextFormat_GetLineSpacing(t, &o->lsm, &o->ls, &o->bl);
    o->w = IDWriteTextFormat_GetFontWeight(t); o->s = IDWriteTextFormat_GetFontStyle(t); o->st = IDWriteTextFormat_GetFontStretch(t);
    o->size = IDWriteTextFormat_GetFontSize(t);
    o->famlen = IDWriteTextFormat_GetFontFamilyNameLength(t); o->loclen = IDWriteTextFormat_GetLocaleNameLength(t);
    IDWriteTextFormat_GetFontFamilyName(t, o->fam, 64); IDWriteTextFormat_GetLocaleName(t, o->loc, 64);
}
static int tf_state_eq(const tf_state *a, const tf_state *b)
{
    return a->ta == b->ta && a->pa == b->pa && a->ww == b->ww && a->rd == b->rd && a->fd == b->fd && tf_fbits(a->tab, b->tab) &&
           a->trim.granularity == b->trim.granularity && a->trim.delimiter == b->trim.delimiter && a->trim.delimiterCount == b->trim.delimiterCount &&
           a->sign == b->sign && a->lsm == b->lsm && tf_fbits(a->ls, b->ls) && tf_fbits(a->bl, b->bl) && a->w == b->w && a->s == b->s && a->st == b->st &&
           tf_fbits(a->size, b->size) && a->famlen == b->famlen && a->loclen == b->loclen && !wcscmp(a->fam, b->fam) && !wcscmp(a->loc, b->loc);
}
#define CHECK_STATE_SAME(t, snap) do { tf_state now_; tf_snap((t), &now_); t_check(tf_state_eq(&now_, &(snap)), __FILE__, __LINE__, "format state unchanged", "a failing call modified the format"); } while (0)

/* Minimal client IDWriteFontFallback (never asked to map anything); counts reference traffic. */
typedef struct tf_fb { const IDWriteFontFallbackVtbl *lpVtbl; volatile LONG ref; LONG addref, release, qi, map; } tf_fb;
static HRESULT WINAPI tf_fb_QI(IDWriteFontFallback *This, REFIID riid, void **ppv)
{
    tf_fb *o = (tf_fb *)This; InterlockedIncrement(&o->qi);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFallback)) { *ppv = This; InterlockedIncrement(&o->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI tf_fb_AddRef(IDWriteFontFallback *This) { tf_fb *o = (tf_fb *)This; InterlockedIncrement(&o->addref); return InterlockedIncrement(&o->ref); }
static ULONG WINAPI tf_fb_Release(IDWriteFontFallback *This) { tf_fb *o = (tf_fb *)This; InterlockedIncrement(&o->release); return InterlockedDecrement(&o->ref); }
static HRESULT WINAPI tf_fb_Map(IDWriteFontFallback *This, IDWriteTextAnalysisSource *src, UINT32 pos, UINT32 len, IDWriteFontCollection *base,
                                const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE s, DWRITE_FONT_STRETCH st, UINT32 *mapped,
                                IDWriteFont **font, FLOAT *scale)
{
    tf_fb *o = (tf_fb *)This; InterlockedIncrement(&o->map);
    if (mapped) *mapped = 0; if (font) *font = NULL; if (scale) *scale = 1.0f; return S_OK;
}
static const IDWriteFontFallbackVtbl tf_fb_vtbl = { tf_fb_QI, tf_fb_AddRef, tf_fb_Release, tf_fb_Map };
static void tf_fb_init(tf_fb *o) { memset(o, 0, sizeof *o); o->lpVtbl = &tf_fb_vtbl; o->ref = 1; }

#endif
