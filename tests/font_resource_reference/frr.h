/* Shared fixtures for the font_resource_reference area: IDWriteFontFaceReference / IDWriteFontFaceReference1 /
 * IDWriteFontResource / IDWriteFontDownloadQueue / IDWriteFontDownloadListener.
 *
 * Facts the area relies on (Windows 10 build 19041 dwrite.dll):
 *   - IDWriteFactory::CreateFontFaceReference (file / path / axis flavours) writes *ref = NULL first, rejects a NULL file or path,
 *     simulations with bits outside {BOLD,OBLIQUE} (value & ~3) and axis arrays that are NULL with count != 0 or have 0x10000+ entries
 *     with E_INVALIDARG; it does not open the font file (the reference is only a key: loader id, key bytes, face index, axes, simulations).
 *   - A reference whose loader does not implement IDWriteRemoteFontFileLoader is DWRITE_LOCALITY_LOCAL and its Enqueue* methods succeed
 *     without touching the download queue.
 */
#ifndef FRR_H
#define FRR_H
#include "dwx.h"

#define FRR_EINVALIDARG ((HRESULT)0x80070057)
#define FRR_ENOTSUFFICIENT ((HRESULT)0x8007007A)
#define FRR_FIXED(v) ((int32_t)((v) * 65536.0f))
/* Out parameters are pre-set to a sentinel; after the check they are reset so a non-conforming implementation cannot make cleanup crash. */
#define FRR_ZERO(p) do { CHECKF((p) == NULL, "out parameter not zeroed: %p", (void *)(p)); (p) = NULL; } while (0)

/* An isolated factory (with IDWriteFactory3 and, when present, IDWriteFactory6/5) plus one in-memory font loader/file. */
typedef struct frr_fix {
    IDWriteFactory *f; IDWriteFactory3 *f3; IDWriteFactory5 *f5; IDWriteFactory6 *f6;
    dwx_mem_loader *ld; UINT32 key; IDWriteFontFile *file;
} frr_fix;
/* Opens the fixture over a font blob. Returns 1 on success; on failure t_skip() was called (frr_close is still safe). */
int frr_open(frr_fix *x, const void *blob, size_t n);
int frr_open_default(frr_fix *x);                                     /* default synthetic font */
void frr_close(frr_fix *x);
/* Another IDWriteFontFile for blob `key` of the same loader (caller releases). */
IDWriteFontFile *frr_file(frr_fix *x);
/* Reference over the fixture file (caller releases); NULL when creation failed (a failed CHECK is recorded). */
IDWriteFontFaceReference *frr_ref(frr_fix *x, UINT32 index, DWRITE_FONT_SIMULATIONS sim);
IDWriteFontFaceReference1 *frr_ref1(IDWriteFontFaceReference *r);      /* QueryInterface helper (NULL when unsupported) */

#define FRR_OPEN(x) frr_fix x; if (!frr_open_default(&x)) { frr_close(&x); return; }
#define FRR_OPEN_NEED6(x) frr_fix x; if (!frr_open_default(&x)) { frr_close(&x); return; } \
    if (!x.f6) { t_skip("IDWriteFactory6 unsupported"); frr_close(&x); return; }

/* ---- synthetic variable font (fvar) ----------------------------------------------------------------------------- */
typedef struct { char tag[5]; float mn, df, mx; unsigned flags; unsigned nameid; const char *name; } frr_axis;
/* Default synthetic font + an fvar table with the given axes (name ids >= 256 get the given en-US names). malloc'd. */
uint8_t *frr_build_var(const frr_axis *ax, int n, size_t *size);
/* Two-axis font: wght 100/400/900 (name 256 "Weight"), XTRA 10/50/90 (name 257 "Extra Axis"). */
uint8_t *frr_build_var2(size_t *size);
#define FRR_TAG(a, b, c, d) DW_TAG(a, b, c, d)

/* ---- remote-flavoured loader --------------------------------------------------------------------------------------- */
/* IDWriteRemoteFontFileLoader wrapping a dwx_mem_loader: streams come from the inner loader, GetLocalityFromKey reports `locality`. */
typedef struct frr_remote {
    const void *lpVtbl; dwx_base b;
    dwx_mem_loader *inner; DWRITE_LOCALITY locality; HRESULT locality_hr;
    LONG n_locality, n_create, n_remote_create, n_url; UINT32 last_key_size; UINT8 last_key[16];
    int registered; IDWriteFactory *reg_factory;
} frr_remote;
frr_remote *frr_remote_new(dwx_mem_loader *inner, DWRITE_LOCALITY loc);   /* takes a reference on inner */
HRESULT frr_remote_register(frr_remote *r, IDWriteFactory *f);
void frr_remote_unregister(frr_remote *r);

/* IDWriteFontFace5::Equals through QueryInterface: 1 / 0, or -1 when IDWriteFontFace5 is unsupported. */
int frr_face_equals(IUnknown *a, IUnknown *b);

/* ---- system font helpers -------------------------------------------------------------------------------------------- */
/* Wide path of a system font file (relative file name in the fonts directory); 0 when absent. */
int frr_sys_path(const char *leaf, WCHAR *out, int cap);
/* Size and last-write time as reported by the file system. */
int frr_disk_info(const WCHAR *path, UINT64 *size, FILETIME *ft);
static __inline UINT64 frr_ft64(FILETIME ft) { return ((UINT64)ft.dwHighDateTime << 32) | ft.dwLowDateTime; }
static __inline FILETIME frr_ft_from64(UINT64 v) { FILETIME f; f.dwLowDateTime = (DWORD)v; f.dwHighDateTime = (DWORD)(v >> 32); return f; }
/* Reference to a system font file through the path overload of IDWriteFactory3::CreateFontFaceReference; NULL (and t_skip) if absent. */
IDWriteFontFaceReference *frr_sys_ref(IDWriteFactory3 *f3, const char *leaf, UINT32 index, DWRITE_FONT_SIMULATIONS sim);
/* The shared factory as IDWriteFactory3 (NULL when unsupported). */
IDWriteFactory3 *frr_shared3(void);
#endif
