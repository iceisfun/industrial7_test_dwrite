/* Shared helpers for the factory_text_objects area (factory creation entry points).
 * Everything here is LoadLibrary("dwrite.dll") -> GetProcAddress("DWriteCreateFactory") -> vtable calls (via common/dw.h). */
#ifndef FTO_H
#define FTO_H
#include "dwx.h"
#include <float.h>

#define FTO_E_INVALIDARG ((HRESULT)0x80070057)
#define FTO_E_NOT_SUFFICIENT_BUFFER ((HRESULT)0x8007007A)
#define FTO_SENTINEL ((void *)(ULONG_PTR)0x1000)   /* never dereferenced: only checks that the callee overwrote the out slot */

/* Run a body once per factory kind: the process-wide shared factory and a freshly created isolated factory.
 * The body receives the IDWriteFactory and a tag ("shared"/"isolated") for t_note. */
typedef void (*fto_body_fn)(IDWriteFactory *f, const char *tag);
void fto_both(fto_body_fn fn);
#define FTO_BOTH(name) \
    static void name##_body(IDWriteFactory *f, const char *tag); \
    TEST(name) { fto_both(name##_body); } \
    static void name##_body(IDWriteFactory *f, const char *tag)

/* A new buffer of n copies of c followed by NUL (caller frees). */
WCHAR *fto_repeat(WCHAR c, size_t n);
/* Locale made of "en" followed by n-2 characters in valid 8-letter subtags; total length exactly n (n >= 2, free()). */
WCHAR *fto_long_locale(size_t n);

/* A standard text format on the given factory (Arial-independent: family need not exist). NULL on failure. */
IDWriteTextFormat *fto_format(IDWriteFactory *f, float size);

/* Custom collection with one synthetic font family (fixture). Owns everything; teardown always safe. */
typedef struct fto_coll {
    dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; IDWriteFactory *f; HRESULT hr;
    UINT32 key; WCHAR family[32];
} fto_coll;
/* Builds a family named `family` (ASCII) with regular weight and registers loaders on f. fc->coll NULL and fc->hr set on failure. */
void fto_coll_setup(fto_coll *fc, IDWriteFactory *f, const char *family);
void fto_coll_teardown(fto_coll *fc);

/* Read a whole text-format locale / family name into a malloc'd buffer (NULL on failure). */
WCHAR *fto_get_locale(IDWriteTextFormat *fmt);
WCHAR *fto_get_family(IDWriteTextFormat *fmt);

#endif
