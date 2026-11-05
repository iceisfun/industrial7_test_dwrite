/* IDWriteRemoteFontFileStream::BeginDownload / IDWriteAsyncResult::GetWaitHandle / GetResult against the HTTP font loader.
 *
 * These tests start a real WinInet request, so they live in a dx_ area (run with /destructive). The host name uses the reserved ".invalid"
 * top-level domain, which never resolves: no font data is ever transferred and the download is expected to complete with a failure.
 *
 * Windows 10 build 19041 observed facts (font stream BeginDownload, download object construction,
 * SetErrorState, GetResult, GetWaitHandle):
 *   - with no fragments the stream creates one NetDownload (cached in the stream while it is pending) and starts the request asynchronously;
 *   - the result code of a NetDownload starts as E_PENDING (0x8000000A) and is replaced by the failure when the request fails, at which
 *     moment the manual-reset completion event returned by GetWaitHandle is signalled;
 *   - the wait handle is a plain field: every GetWaitHandle call on one async result returns the same handle, owned by the async result
 *     (the caller must not close it);
 *   - a stream whose download finished (successfully or not) forgets it, so the next BeginDownload starts a new one.
 * Host-dependent facts (the exact failure code, proxies that answer for any host) are notes. */
#include "../gap_00/gap0.h"

#define DL_URL L"https://gap0-download.example.invalid/fonts/dl.ttf"
#define DL_WAIT_MS 20000

static const GUID g_op = { 0x2c1d7f10, 0x44aa, 0x4d6b, { 0xb2, 0x8e, 0x7a, 0x31, 0x0c, 0x99, 0x03, 0x01 } };

/* Waits for the async result to complete; returns 1 when signalled. */
static int dl_wait(IDWriteAsyncResult *r)
{
    HANDLE h = IDWriteAsyncResult_GetWaitHandle(r);
    DWORD w;
    if (!h) return 0;
    w = WaitForSingleObject(h, DL_WAIT_MS);
    return w == WAIT_OBJECT_0;
}

TEST(gap0x_download_returns_async_result)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r = (IDWriteAsyncResult *)(ULONG_PTR)1;
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s) {
        HRESULT hr = IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r);
        CHECK_HR(hr, S_OK);
        CHECK(r != NULL && r != (IDWriteAsyncResult *)(ULONG_PTR)1);
        if (SUCCEEDED(hr) && r && r != (IDWriteAsyncResult *)(ULONG_PTR)1) {
            dl_wait(r);
            IDWriteAsyncResult_Release(r);
        }
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0x_async_result_qi)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r = NULL;
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s && SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r)) && r) {
        dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteAsyncResult, 1),
            DW_QI_ROW(IDWriteRemoteFontFileStream, 0), DW_QI_ROW(IDWriteFontFileStream, 0), DW_QI_ROW(IDWriteFontFile, 0),
            DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteRemoteFontFileLoader, 0),
        };
        DW_CHECK_QI(r, rows);
        dl_wait(r);
    } else t_skip("BeginDownload prerequisite failed");
    DW_RELEASE(r); DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0x_async_result_wait_handle_stable)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r = NULL;
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s && SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r)) && r) {
        HANDLE h1 = IDWriteAsyncResult_GetWaitHandle(r), h2 = IDWriteAsyncResult_GetWaitHandle(r);
        DWORD w;
        CHECK(h1 != NULL && h1 != INVALID_HANDLE_VALUE);
        CHECK(h1 == h2);
        w = WaitForSingleObject(h1, 0);
        CHECKF(w == WAIT_OBJECT_0 || w == WAIT_TIMEOUT, "WaitForSingleObject(wait handle, 0) = %lu", w);       /* a valid waitable handle */
        dl_wait(r);
        CHECK(IDWriteAsyncResult_GetWaitHandle(r) == h1);        /* still the same handle after completion */
    } else t_skip("BeginDownload prerequisite failed");
    DW_RELEASE(r); DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0x_async_result_pending_then_failed)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r = NULL;
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s && SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r)) && r) {
        HRESULT early = IDWriteAsyncResult_GetResult(r), done;
        /* Either the request is still in flight (E_PENDING) or it has already failed. */
        CHECKF(early == E_PENDING || FAILED(early), "early result %s", dw_hr_name(early));
        if (dl_wait(r)) {
            done = IDWriteAsyncResult_GetResult(r);
            CHECK(done != E_PENDING);
            if (SUCCEEDED(done)) t_note("download of an unresolvable host reported success (%s): a proxy answers for any host", dw_hr_name(done));
            else t_note("completed download result: %s", dw_hr_name(done));
            CHECK(IDWriteAsyncResult_GetResult(r) == done);              /* the result is stable once completed */
        } else t_note("download did not complete within %d ms", DL_WAIT_MS);
    } else t_skip("BeginDownload prerequisite failed");
    DW_RELEASE(r); DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0x_stream_stays_remote_after_failed_download)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r = NULL;
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s && SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r)) && r) {
        if (dl_wait(r) && FAILED(IDWriteAsyncResult_GetResult(r))) {
            UINT64 sz = 9, fsz = 9;
            CHECK(IDWriteRemoteFontFileStream_GetLocality(s) == DWRITE_LOCALITY_REMOTE);
            CHECK_HR(IDWriteRemoteFontFileStream_GetLocalFileSize(s, &sz), S_OK);
            CHECK(sz == 0);
            CHECK_HR(IDWriteFontFileStream_GetFileSize((IDWriteFontFileStream *)s, &fsz), DWRITE_E_REMOTEFONT_);
            CHECK(fsz == 0);
        }
    } else t_skip("BeginDownload prerequisite failed");
    DW_RELEASE(r); DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0x_begin_download_again_after_failure_is_new)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r1 = NULL, *r2 = NULL;
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s && SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r1)) && r1) {
        if (dl_wait(r1) && FAILED(IDWriteAsyncResult_GetResult(r1))) {
            /* the stream drops a completed download when it is next consulted, so this is a fresh request */
            HRESULT hr = IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r2);
            CHECK_HR(hr, S_OK);
            CHECK(r2 != NULL && r2 != r1);
            if (r2) dl_wait(r2);
        }
    } else t_skip("BeginDownload prerequisite failed");
    DW_RELEASE(r1); DW_RELEASE(r2); DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0x_begin_download_while_pending_shares_result)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r1 = NULL, *r2 = NULL;
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s && SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r1)) && r1) {
        HRESULT hr = IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r2);
        CHECK_HR(hr, S_OK);
        CHECK(r2 != NULL);
        /* A request still in flight is returned again; if it already finished a new one was started. */
        if (r2 == r1) t_note("second BeginDownload returned the pending async result");
        else t_note("second BeginDownload started a new request (first had completed)");
        if (r1) dl_wait(r1);
        if (r2 && r2 != r1) dl_wait(r2);
    } else t_skip("BeginDownload prerequisite failed");
    DW_RELEASE(r1); DW_RELEASE(r2); DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0x_download_with_zero_fragment)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r = NULL;
    static const DWRITE_FILE_FRAGMENT frag = { 0, 0 };         /* in range of the empty file: accepted, the whole file is downloaded */
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s) {
        HRESULT hr = IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, &frag, 1, &r);
        CHECK_HR(hr, S_OK);
        CHECK(r != NULL);
        if (r) dl_wait(r);
    }
    DW_RELEASE(r); DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0x_release_everything_while_pending)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; IDWriteAsyncResult *r = NULL;
    if (SUCCEEDED(gap_rstream(&x, DL_URL, &s)) && s && SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(s, &g_op, NULL, 0, &r)) && r) {
        /* Drop the application's references while the request may still be in flight; the callbacks must not touch freed objects. */
        DW_RELEASE(r); DW_RELEASE(s); DW_RELEASE(x.rl); DW_RELEASE(x.f5); DW_RELEASE(x.f);
        Sleep(1500);
    } else t_skip("BeginDownload prerequisite failed");
    DW_RELEASE(r); DW_RELEASE(s);
    gap_rclose(&x);
    /* the process is still healthy: a new factory works */
    {
        IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
        CHECK(f != NULL);
        DW_RELEASE(f);
    }
}
TEST(gap0x_two_streams_two_downloads)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *a = NULL, *b = NULL; IDWriteAsyncResult *ra = NULL, *rb = NULL;
    if (SUCCEEDED(gap_rstream(&x, L"https://gap0-download.example.invalid/a.ttf", &a)) && a &&
        SUCCEEDED(gap_rstream(&x, L"https://gap0-download.example.invalid/b.ttf", &b)) && b &&
        SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(a, &g_op, NULL, 0, &ra)) &&
        SUCCEEDED(IDWriteRemoteFontFileStream_BeginDownload(b, &g_op, NULL, 0, &rb)) && ra && rb) {
        CHECK(ra != rb);
        CHECK(IDWriteAsyncResult_GetWaitHandle(ra) != IDWriteAsyncResult_GetWaitHandle(rb));
        dl_wait(ra); dl_wait(rb);
    } else t_skip("BeginDownload prerequisite failed");
    DW_RELEASE(ra); DW_RELEASE(rb); DW_RELEASE(a); DW_RELEASE(b);
    gap_rclose(&x);
}
