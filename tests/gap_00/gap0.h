/* Shared fixtures for gap_00 / dx_gap_00: callback-driven interfaces observed from dwrite.dll's side
 * (analysis source / sink, collection loader / enumerator) and the HTTP remote font loader, whose stream and async-result
 * objects are implemented by dwrite itself (the HTTP font loader and its streams in the Windows 10 build).
 */
#ifndef GAP0_H
#define GAP0_H
#include "dwx.h"
#include <wchar.h>

#define G_LEN(s) ((UINT32)wcslen(s))
#define G_N(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define DWRITE_E_REMOTEFONT_ ((HRESULT)0x8898500D)
#define G_E_NOTIMPL ((HRESULT)0x80004001)

/* Isolated factory + IDWriteFactory5 + a fresh HTTP font file loader (no network is touched by creating it). */
typedef struct gap_rfx {
    IDWriteFactory *f; IDWriteFactory5 *f5; IDWriteRemoteFontFileLoader *rl;
} gap_rfx;
/* 1 on success; on failure t_skip() was called and gap_rclose() is still safe. */
static int gap_ropen(gap_rfx *x)
{
    HRESULT hr;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("cannot create an isolated factory"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory5, (void **)&x->f5)) || !x->f5) { x->f5 = NULL; t_skip("IDWriteFactory5 unsupported"); return 0; }
    hr = IDWriteFactory5_CreateHttpFontFileLoader(x->f5, NULL, NULL, &x->rl);
    if (FAILED(hr) || !x->rl) { x->rl = NULL; t_skip("CreateHttpFontFileLoader: %s", dw_hr_name(hr)); return 0; }
    return 1;
}
static void gap_rclose(gap_rfx *x)
{
    DW_RELEASE(x->rl); DW_RELEASE(x->f5); DW_RELEASE(x->f);
    memset(x, 0, sizeof *x);
}
#define GAP_ROPEN(x) gap_rfx x; if (!gap_ropen(&x)) { gap_rclose(&x); return; }
/* The loader's key is the UTF-16 URL without terminator. */
#define GAP_KEY(url) (url), (UINT32)(lstrlenW(url) * 2)
static HRESULT gap_rstream(gap_rfx *x, const WCHAR *url, IDWriteRemoteFontFileStream **s)
{
    return IDWriteRemoteFontFileLoader_CreateRemoteStreamFromKey(x->rl, url, (UINT32)(lstrlenW(url) * 2), s);
}
static HRESULT gap_rplain(gap_rfx *x, const WCHAR *url, IDWriteFontFileStream **s)
{
    return IDWriteRemoteFontFileLoader_CreateStreamFromKey(x->rl, url, (UINT32)(lstrlenW(url) * 2), s);
}
static HRESULT gap_rloc(gap_rfx *x, const WCHAR *url, DWRITE_LOCALITY *l)
{
    return IDWriteRemoteFontFileLoader_GetLocalityFromKey(x->rl, url, (UINT32)(lstrlenW(url) * 2), l);
}
#endif
