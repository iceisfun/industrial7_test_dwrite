/* DirectWrite helper layer (compiled into every test exe together with dw.c / dwx.c / ttfgen.c).
 *
 * dwrite.dll is NEVER linked: DWriteCreateFactory is resolved through GetProcAddress (dw_proc), every other call
 * goes through COM vtables (COBJMACROS style: IDWriteFactory_CreateTextFormat(f, ...) or ->lpVtbl->).
 *
 *   IDWriteFactory *f = dw_shared();                       // cached shared factory (do not Release)
 *   IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);  // NULL when unsupported
 *   CHECK_HR(IDWriteFactory_GetGdiInterop(f, &gdi), S_OK);
 */
#ifndef DW_H
#define DW_H
#include "t.h"
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <objbase.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include "ttfgen.h"

/* Interfaces newer than the installed mingw headers: IIDs only (QueryInterface must fail with E_NOINTERFACE where the
 * OS does not implement them, so tests can still probe for them). */
extern const IID DW_IID_IDWriteBitmapRenderTarget2, DW_IID_IDWriteBitmapRenderTarget3, DW_IID_IDWriteFactory8,
                 DW_IID_IDWriteFontFace7, DW_IID_IDWritePaintReader;

/* HRESULT constants not always in the SDK headers */
#ifndef DWRITE_E_FILEFORMAT
#define DWRITE_E_FILEFORMAT 0x88985000
#endif
#define DW_E_NOT_SUFFICIENT_BUFFER ((HRESULT)0x8007007A)   /* HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) */
#define DW_E_INVALIDARG            ((HRESULT)0x80070057)

/* ---- factory acquisition ---------------------------------------------------------------- */
typedef HRESULT (WINAPI *dw_create_fn)(DWRITE_FACTORY_TYPE, REFIID, IUnknown **);
dw_create_fn dw_proc(void);                         /* GetProcAddress("DWriteCreateFactory"), NULL if missing */
/* Raw call. If the export is missing returns HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND) and leaves *out NULL.
 * NEVER pass a NULL out pointer: the real DLL writes *ppFactory = NULL before anything else. */
HRESULT dw_create_factory(DWRITE_FACTORY_TYPE type, REFIID riid, void **out);
IDWriteFactory *dw_shared(void);                    /* cached SHARED factory (IDWriteFactory); owned by the helper */
IDWriteFactory *dw_isolated(void);                  /* cached ISOLATED factory; owned by the helper */
/* Cached factory of the given type QueryInterface'd to IDWriteFactory<ver> (ver 0..7); NULL when not supported.
 * The returned pointer is owned by the helper (never Release it). */
void *dw_factory_v(DWRITE_FACTORY_TYPE type, int ver);
const IID *dw_factory_iid(int ver);                 /* IID_IDWriteFactory, IID_IDWriteFactory1 ... IID_IDWriteFactory7 */
/* Fresh ref-counted factory (caller releases). */
IDWriteFactory *dw_new_factory(DWRITE_FACTORY_TYPE type);

/* ---- HRESULT helpers --------------------------------------------------------------------- */
const char *dw_hr_name(HRESULT hr);                 /* "S_OK", "DWRITE_E_NOFONT", ... or "0x%08lx" (static buffer) */
HRESULT dw_check_hr(HRESULT got, HRESULT want, const char *file, int line, const char *expr);
/* Hard check that expr returns exactly `expected`; reports only on mismatch, with symbolic names. Value of the macro = the HRESULT. */
#define CHECK_HR(expr, expected) dw_check_hr((expr), (HRESULT)(expected), __FILE__, __LINE__, #expr " == " #expected)
#define CHECK_HR_OK(expr)        CHECK_HR(expr, S_OK)
#define CHECK_HR_FAILED(expr)    do { HRESULT hr_ = (expr); t_check(FAILED(hr_), __FILE__, __LINE__, #expr " fails", "got %s", dw_hr_name(hr_)); } while (0)
#define CHECK_HR_OK_OR_FALSE(expr) do { HRESULT hr_ = (expr); t_check(hr_ == S_OK || hr_ == S_FALSE, __FILE__, __LINE__, #expr " S_OK/S_FALSE", "got %s", dw_hr_name(hr_)); } while (0)
#define CHECK_OUT_NULL(p)        CHECKF((p) == NULL, "out parameter not zeroed: %p", (void *)(p))
/* Release helper: releases and NULLs a COM pointer (safe on NULL). */
#define DW_RELEASE(p) do { if (p) { IUnknown_Release((IUnknown *)(p)); (p) = NULL; } } while (0)
/* Statement that creates an object; on failure skip the rest of the test. */
#define DW_REQUIRE_OK(expr) do { HRESULT hr_ = (expr); if (FAILED(hr_)) { t_skip("prerequisite failed: %s -> %s", #expr, dw_hr_name(hr_)); return; } } while (0)

/* ---- refcount / leak helpers ----------------------------------------------------------------- */
ULONG dw_refcount(IUnknown *u);                     /* AddRef then Release: returns the count after the pair (the current count) */
#define EXPECT_REFS(obj, n) do { ULONG r_ = dw_refcount((IUnknown *)(obj)); t_check(r_ == (ULONG)(n), __FILE__, __LINE__, "refcount(" #obj ") == " #n, "got %lu want %lu", r_, (ULONG)(n)); } while (0)

/* ---- QueryInterface matrix ------------------------------------------------------------------ */
typedef struct { const IID *iid; const char *name; int expect; } dw_qi_row;   /* expect: 1 supported, 0 unsupported (E_NOINTERFACE, or E_NOTIMPL as Win10 font faces answer), -1 OS dependent (t_note) */
/* For every row: QI succeeds/fails as expected; failure => E_NOINTERFACE or E_NOTIMPL with *ppv == NULL; success => refcount rose by 1 and
 * the pointer's IUnknown identity equals obj's; releases afterwards. Also checks QI(IUnknown) identity, AddRef/Release balance. */
void dw_qi_check_(IUnknown *obj, const dw_qi_row *rows, int n, const char *file, int line);
#define DW_CHECK_QI(obj, rows) dw_qi_check_((IUnknown *)(obj), rows, (int)(sizeof(rows) / sizeof((rows)[0])), __FILE__, __LINE__)
#define DW_QI_ROW(name, expect) { &IID_##name, #name, expect }
/* All public dwrite interface IIDs (dwrite.h..dwrite_3.h) with names; expect = -1 in every row. */
const dw_qi_row *dw_iid_table(int *n);
/* QI identity: obj's IUnknown, queried twice and via two interfaces, are the same pointer. */
BOOL dw_same_identity(IUnknown *a, IUnknown *b);

/* ---- strings ----------------------------------------------------------------------------------- */
int dw_wide(const char *utf8, WCHAR *out, int cap);       /* UTF-8 -> UTF-16, returns count incl. NUL (0 on failure) */
int dw_narrow(const WCHAR *w, char *out, int cap);        /* UTF-16 -> UTF-8, returns count incl. NUL */
/* IDWriteLocalizedStrings helpers */
HRESULT dw_ls_find(IDWriteLocalizedStrings *s, const WCHAR *locale, UINT32 *index, BOOL *exists);
/* Reads string #idx as UTF-8 into buf. */
HRESULT dw_ls_get(IDWriteLocalizedStrings *s, UINT32 idx, char *buf, int cap);
/* English (en-us) string if present, else string 0; buf UTF-8. */
HRESULT dw_ls_en(IDWriteLocalizedStrings *s, char *buf, int cap);

/* ---- system fonts ------------------------------------------------------------------------------ */
void dw_fonts_dir(char *out, size_t cap);                 /* "%WINDIR%\\Fonts\\" */
/* First existing file among candidates (file names relative to the fonts dir, NULL terminated); returns 1 and full path. */
int dw_find_system_font(const char *const *candidates, char *out, size_t cap);
BOOL dw_have_font(IDWriteFactory *f, const WCHAR *family);        /* family present in the system collection */
/* Font face for a system family (first matching font); NULL if the family does not exist. Caller releases. */
IDWriteFontFace *dw_sysface(IDWriteFactory *f, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STRETCH s, DWRITE_FONT_STYLE st);
/* Raw bytes of a font table via TryGetFontTable; returns malloc'd copy (caller frees) or NULL; *len set. */
uint8_t *dw_face_table(IDWriteFontFace *face, UINT32 tag_le, UINT32 *len);
#define DW_TAG(a, b, c, d) ((UINT32)(a) | ((UINT32)(b) << 8) | ((UINT32)(c) << 16) | ((UINT32)(d) << 24))

/* ---- numeric helpers ------------------------------------------------------------------------------ */
#define DW_APPROX(a, b, eps) (((a) > (b) ? (a) - (b) : (b) - (a)) <= (eps))
#define CHECK_NEAR(a, b, eps) t_check(DW_APPROX((double)(a), (double)(b), (double)(eps)), __FILE__, __LINE__, #a " ~= " #b, "got %g want %g", (double)(a), (double)(b))
#define CHECK_FLOAT_EQ(a, b)  t_check((float)(a) == (float)(b), __FILE__, __LINE__, #a " == " #b, "got %g want %g", (double)(a), (double)(b))
/* design units -> DIPs for a font of given em size */
#define DW_DU(units, upm, em) ((float)(units) * (float)(em) / (float)(upm))
#endif
