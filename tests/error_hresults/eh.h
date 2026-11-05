/* Shared definitions for the error_hresults area: the failure HRESULTs dwrite.dll produces, provoked through public paths.
 * Exception-to-HRESULT translation inside dwrite (MapExceptionToHresult and the exception constructors) is what makes the values:
 *   ArgumentException -> E_INVALIDARG, ArgumentInsufficientBufferException -> 0x8007007A, IndexOutOfRangeException -> E_FAIL,
 *   FileFormatException -> DWRITE_E_FILEFORMAT, NotSupportedException -> DWRITE_E_UNSUPPORTEDOPERATION,
 *   InvalidCacheDataException -> DWRITE_E_CACHEFORMAT, RemoteFontException -> DWRITE_E_REMOTEFONT, std::bad_alloc -> E_OUTOFMEMORY,
 *   IntegerOverflowException -> 0x80070216, CallbackException(hr) -> hr (application callback failures pass through unchanged),
 *   IOException -> IOException::MapFileOpenError(GetLastError()). */
#ifndef EH_H
#define EH_H
#include "dwx.h"

#define EH_FILEFORMAT              ((HRESULT)0x88985000)
#define EH_UNEXPECTED              ((HRESULT)0x88985001)
#define EH_NOFONT                  ((HRESULT)0x88985002)
#define EH_FILENOTFOUND            ((HRESULT)0x88985003)
#define EH_FILEACCESS              ((HRESULT)0x88985004)
#define EH_FONTCOLLECTIONOBSOLETE  ((HRESULT)0x88985005)
#define EH_ALREADYREGISTERED       ((HRESULT)0x88985006)
#define EH_CACHEFORMAT             ((HRESULT)0x88985007)
#define EH_CACHEVERSION            ((HRESULT)0x88985008)
#define EH_UNSUPPORTEDOPERATION    ((HRESULT)0x88985009)
#define EH_TEXTRENDERERINCOMPATIBLE ((HRESULT)0x8898500A)
#define EH_FLOWDIRECTIONCONFLICTS  ((HRESULT)0x8898500B)
#define EH_NOCOLOR                 ((HRESULT)0x8898500C)
#define EH_REMOTEFONT              ((HRESULT)0x8898500D)
#define EH_DOWNLOADCANCELLED       ((HRESULT)0x8898500E)
#define EH_DOWNLOADFAILED          ((HRESULT)0x8898500F)
#define EH_TOOMANYDOWNLOADS        ((HRESULT)0x88985010)
#define EH_INVALIDARG              ((HRESULT)0x80070057)
#define EH_NSB                     ((HRESULT)0x8007007A)   /* HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) */
#define EH_E_FAIL                  ((HRESULT)0x80004005)
#define EH_CUSTOM                  ((HRESULT)0x80041234)   /* an application-chosen failure used to prove callback pass-through */

#define SENT(T)  ((T)(ULONG_PTR)1)      /* "was not written" sentinel for out parameters */
#define NAN_F    (__builtin_nanf(""))
#define INF_F    (__builtin_inff())

/* Isolated factory per test: registrations and caches never leak between tests. */
#define NEED_FACTORY() IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); \
    if (!f) { t_skip("DWriteCreateFactory(isolated) failed"); return; }
#define DONE_FACTORY() IDWriteFactory_Release(f)

/* Text format over a system font family (fallback applies when the family is absent, so creation never depends on the host). */
static __inline IDWriteTextFormat *eh_format(IDWriteFactory *f, float size) {
    IDWriteTextFormat *t = NULL;
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &t);
    return t;
}
static __inline IDWriteTextLayout *eh_layout(IDWriteFactory *f, const WCHAR *text, float w, float h, IDWriteTextFormat **fmt_out) {
    IDWriteTextFormat *t = eh_format(f, 16.f); IDWriteTextLayout *l = NULL;
    if (t) IDWriteFactory_CreateTextLayout(f, text, (UINT32)lstrlenW(text), t, w, h, &l);
    if (fmt_out) *fmt_out = t; else if (t) IDWriteTextFormat_Release(t);
    return l;
}
/* Synthetic default font blob (caller frees). */
static __inline uint8_t *eh_blob(size_t *n) { return ttf_build_default(n); }

/* Open the synthetic default font through the in-memory loader; skips the test on failure. */
#define NEED_FACE(h, f) dwx_font h; memset(&h, 0, sizeof h); \
    if (FAILED(dwx_font_open_default(&h, f)) || !h.face) { dwx_font_close(&h); DONE_FACTORY(); t_skip("synthetic face unavailable"); return; }
#endif
