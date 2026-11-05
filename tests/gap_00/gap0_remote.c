/* IDWriteRemoteFontFileLoader / IDWriteRemoteFontFileStream as implemented by dwrite.dll's HTTP loader
 * (IDWriteFactory5::CreateHttpFontFileLoader -> NetFontLoader; its streams are NetFontStream objects).
 *
 * Windows 10 build 19041 decomp facts the tests rely on (NetFontLoader::CreateRemoteStreamFromKey !0x147a40, ::CreateStreamFromKey !0x147b00,
 * ::GetLocalityFromKey !0x147bf0, ::GetStream !0x147cd0, NetFontStream::* !0x148e90 .. !0x149190):
 *   - the reference key is a UTF-16 URL; the byte size is rounded down to an even number (size & ~1); the URL scheme must be http or https
 *     (anything else, or an unparsable URL, is thrown as a C++ exception from the stream constructor: deliberately never exercised);
 *   - streams are cached per loader by key content: the same key yields the same stream object while one is alive;
 *   - creating the stream does not download anything: until a download completes the stream is empty and remote
 *     (GetLocality = DWRITE_LOCALITY_REMOTE, GetFileSize = DWRITE_E_REMOTEFONT with *size = 0, GetLocalFileSize = S_OK/0,
 *      ReadFileFragment = DWRITE_E_REMOTEFONT, CreateStreamFromKey = DWRITE_E_REMOTEFONT, GetLocalityFromKey = S_OK/REMOTE);
 *   - GetFileFragmentLocality / BeginDownload validate fragments with IsFragmentInRange(offset, size): offset <= fileSize && size <= fileSize - offset,
 *     failing with E_INVALIDARG; with the empty stream only the zero fragment (0, 0) is in range;
 *   - GetLastWriteTime is a stub: *time = 0 and E_NOTIMPL.
 * The live download (network traffic, even to a name that cannot resolve) is in tests/dx_gap_00. */
#include "gap0.h"

#define URL_A L"https://gap0.example.invalid/fonts/a.ttf"
#define URL_B L"https://gap0.example.invalid/fonts/b.ttf"
#define URL_C L"http://gap0.example.invalid:8080/c.otf"

/* ---- stream creation -------------------------------------------------------------------------------------------------- */
TEST(gap0_remote_stream_create_ok)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = (IDWriteRemoteFontFileStream *)(ULONG_PTR)1;
    CHECK_HR(gap_rstream(&x, URL_A, &s), S_OK);
    CHECK(s != NULL && s != (IDWriteRemoteFontFileStream *)(ULONG_PTR)1);     /* *ppStream is written (NULL first, then the stream) */
    if (s && s != (IDWriteRemoteFontFileStream *)(ULONG_PTR)1) IDWriteRemoteFontFileStream_Release(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_create_http_and_port)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    CHECK_HR(gap_rstream(&x, URL_C, &s), S_OK);          /* plain http with an explicit port parses as well */
    CHECK(s != NULL);
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_qi_matrix)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        /* NetFontStream::QueryInterface accepts exactly IUnknown, IDWriteFontFileStream and IDWriteRemoteFontFileStream. */
        dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteRemoteFontFileStream, 1), DW_QI_ROW(IDWriteFontFileStream, 1),
            DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFontFileLoader, 0), DW_QI_ROW(IDWriteRemoteFontFileLoader, 0),
            DW_QI_ROW(IDWriteAsyncResult, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteLocalFontFileLoader, 0),
            DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteInMemoryFontFileLoader, 0),
        };
        DW_CHECK_QI(s, rows);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_qi_same_pointer)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; void *a = NULL, *b = NULL, *u = NULL;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        CHECK_HR(IDWriteRemoteFontFileStream_QueryInterface(s, &IID_IDWriteFontFileStream, &a), S_OK);
        CHECK_HR(IDWriteRemoteFontFileStream_QueryInterface(s, &IID_IDWriteRemoteFontFileStream, &b), S_OK);
        CHECK_HR(IDWriteRemoteFontFileStream_QueryInterface(s, &IID_IUnknown, &u), S_OK);
        CHECK(a == (void *)s && b == (void *)s && u == (void *)s);      /* single inheritance: *ppv = this for all three */
        if (a) IUnknown_Release((IUnknown *)a);
        if (b) IUnknown_Release((IUnknown *)b);
        if (u) IUnknown_Release((IUnknown *)u);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_qi_failure_zeroes_out)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; void *p = (void *)(ULONG_PTR)1;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        CHECK_HR(IDWriteRemoteFontFileStream_QueryInterface(s, &IID_IDWriteFontFile, &p), E_NOINTERFACE);
        CHECK_OUT_NULL(p);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_addref_release_pairs)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        ULONG a1 = IDWriteRemoteFontFileStream_AddRef(s);
        ULONG a2 = IDWriteRemoteFontFileStream_AddRef(s);
        ULONG r2 = IDWriteRemoteFontFileStream_Release(s);
        ULONG r1 = IDWriteRemoteFontFileStream_Release(s);
        CHECK(a2 == a1 + 1);            /* AddRef returns the incremented count (NetFontStream::AddRef !0x1489c0) */
        CHECK(r2 == a1 + 0);
        CHECK(r1 == a1 - 1);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}

/* ---- stream identity and caching ---------------------------------------------------------------------------------------- */
TEST(gap0_remote_same_key_same_stream)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *a = NULL, *b = NULL;
    CHECK_HR(gap_rstream(&x, URL_A, &a), S_OK);
    CHECK_HR(gap_rstream(&x, URL_A, &b), S_OK);
    CHECK(a != NULL && a == b);          /* GetStream finds the live stream in the loader's hash table */
    DW_RELEASE(a); DW_RELEASE(b);
    gap_rclose(&x);
}
TEST(gap0_remote_different_keys_different_streams)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *a = NULL, *b = NULL, *c = NULL;
    CHECK_HR(gap_rstream(&x, URL_A, &a), S_OK);
    CHECK_HR(gap_rstream(&x, URL_B, &b), S_OK);
    CHECK_HR(gap_rstream(&x, URL_C, &c), S_OK);
    CHECK(a && b && c && a != b && a != c && b != c);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    gap_rclose(&x);
}
TEST(gap0_remote_two_loaders_do_not_share_streams)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileLoader *rl2 = NULL; IDWriteRemoteFontFileStream *a = NULL, *b = NULL;
    CHECK_HR(IDWriteFactory5_CreateHttpFontFileLoader(x.f5, NULL, NULL, &rl2), S_OK);
    if (rl2) {
        CHECK_HR(gap_rstream(&x, URL_A, &a), S_OK);
        CHECK_HR(IDWriteRemoteFontFileLoader_CreateRemoteStreamFromKey(rl2, URL_A, (UINT32)sizeof(URL_A) - 2, &b), S_OK);
        CHECK(a && b && a != b);         /* the hash table is a member of the loader */
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(rl2);
    }
    gap_rclose(&x);
}
TEST(gap0_remote_odd_key_size_drops_last_byte)
{
    GAP_ROPEN(x);
    /* key = URL (even size) followed by one stray byte: the loader uses size & ~1 so the stray byte is ignored. */
    WCHAR url[] = URL_A;
    UINT32 n = (UINT32)lstrlenW(url) * 2;
    BYTE key[256]; IDWriteRemoteFontFileStream *a = NULL, *b = NULL;
    memcpy(key, url, n); key[n] = 0x5A;
    CHECK_HR(IDWriteRemoteFontFileLoader_CreateRemoteStreamFromKey(x.rl, key, n, &a), S_OK);
    CHECK_HR(IDWriteRemoteFontFileLoader_CreateRemoteStreamFromKey(x.rl, key, n + 1, &b), S_OK);
    CHECK(a != NULL && a == b);
    DW_RELEASE(a); DW_RELEASE(b);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_released_and_recreated)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *a = NULL; int i;
    for (i = 0; i < 20; i++) {
        a = NULL;
        CHECK_HR(gap_rstream(&x, URL_A, &a), S_OK);
        if (!a) break;
        CHECK(IDWriteRemoteFontFileStream_GetLocality(a) == DWRITE_LOCALITY_REMOTE);
        IDWriteRemoteFontFileStream_Release(a);
    }
    gap_rclose(&x);
}
TEST(gap0_remote_stream_survives_loader_release)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        DW_RELEASE(x.rl);                /* the stream holds a reference on its loader while the client holds the stream */
        CHECK(IDWriteRemoteFontFileStream_GetLocality(s) == DWRITE_LOCALITY_REMOTE);
        {
            UINT64 sz = 123;
            CHECK_HR(IDWriteRemoteFontFileStream_GetLocalFileSize(s, &sz), S_OK);
            CHECK(sz == 0);
        }
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_survives_factory_release)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        DW_RELEASE(x.f5); DW_RELEASE(x.f);
        CHECK(IDWriteRemoteFontFileStream_GetLocality(s) == DWRITE_LOCALITY_REMOTE);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}

/* ---- the empty, not yet downloaded stream ---------------------------------------------------------------------------------- */
TEST(gap0_remote_stream_locality_is_remote)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        DWRITE_LOCALITY l = IDWriteRemoteFontFileStream_GetLocality(s);
        CHECK(l == DWRITE_LOCALITY_REMOTE);          /* NetFontStream::GetLocality: 2 (LOCAL) only once the download completed, else 0 */
        l = IDWriteRemoteFontFileStream_GetLocality(s);
        CHECK(l == DWRITE_LOCALITY_REMOTE);          /* repeated query is stable */
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_local_file_size_zero)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; UINT64 sz = 0xDEADBEEFDEADBEEFull;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        CHECK_HR(IDWriteRemoteFontFileStream_GetLocalFileSize(s, &sz), S_OK);
        CHECK(sz == 0);
        sz = 1;
        CHECK_HR(IDWriteRemoteFontFileStream_GetLocalFileSize(s, &sz), S_OK);
        CHECK(sz == 0);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_get_file_size_remotefont)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; UINT64 sz = 0xDEADBEEFDEADBEEFull;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        /* The size field is stored before the "is downloaded" test, the result is DWRITE_E_REMOTEFONT while remote. */
        CHECK_HR(IDWriteFontFileStream_GetFileSize((IDWriteFontFileStream *)s, &sz), DWRITE_E_REMOTEFONT_);
        CHECK(sz == 0);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_last_write_time_notimpl)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; UINT64 t = 0x1122334455667788ull;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        CHECK_HR(IDWriteFontFileStream_GetLastWriteTime((IDWriteFontFileStream *)s, &t), G_E_NOTIMPL);
        CHECK(t == 0);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_read_fragment_remotefont)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    static const struct { UINT64 off, size; } rows[] = { { 0, 0 }, { 0, 1 }, { 0, 4 }, { 1, 1 }, { 100, 50 }, { 0xFFFFFFFFull, 1 }, { ~0ull, ~0ull } };
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        int i;
        for (i = 0; i < G_N(rows); i++) {
            const void *start = (const void *)(ULONG_PTR)1; void *ctx = (void *)(ULONG_PTR)1;
            HRESULT hr = IDWriteFontFileStream_ReadFileFragment((IDWriteFontFileStream *)s, &start, rows[i].off, rows[i].size, &ctx);
            t_check(hr == DWRITE_E_REMOTEFONT_, __FILE__, __LINE__, "ReadFileFragment remote", "row %d: got %s", i, dw_hr_name(hr));
            CHECKF(start == NULL, "row %d: fragmentStart not zeroed", i);
            CHECKF(ctx == NULL, "row %d: fragmentContext not zeroed", i);
        }
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_release_fragment_null_is_harmless)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        IDWriteFontFileStream_ReleaseFileFragment((IDWriteFontFileStream *)s, NULL);     /* NetFontStream::ReleaseFileFragment is an empty stub */
        CHECK(IDWriteRemoteFontFileStream_GetLocality(s) == DWRITE_LOCALITY_REMOTE);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}

/* GetFileFragmentLocality(offset, size, &isLocal, &partialSize): outputs are always written; result E_INVALIDARG when the fragment is outside the (empty) file. */
TEST(gap0_remote_fragment_locality_table)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    static const struct { UINT64 off, size; HRESULT hr; int local; } rows[] = {
        { 0, 0, S_OK, 1 },                                  /* the empty fragment is "in range" of an empty file */
        { 0, 1, E_INVALIDARG, 0 }, { 1, 0, E_INVALIDARG, 0 }, { 1, 1, E_INVALIDARG, 0 },
        { 0, 0x10000, E_INVALIDARG, 0 }, { 0x10000, 0, E_INVALIDARG, 0 },
        { 0xFFFFFFFFull, 0, E_INVALIDARG, 0 }, { 0x100000000ull, 0, E_INVALIDARG, 0 },
        { ~0ull, 0, E_INVALIDARG, 0 }, { 0, ~0ull, E_INVALIDARG, 0 }, { ~0ull, ~0ull, E_INVALIDARG, 0 },
    };
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        int i;
        for (i = 0; i < G_N(rows); i++) {
            BOOL local = 77; UINT64 partial = 99;
            HRESULT hr = IDWriteRemoteFontFileStream_GetFileFragmentLocality(s, rows[i].off, rows[i].size, &local, &partial);
            t_check(hr == rows[i].hr, __FILE__, __LINE__, "GetFileFragmentLocality hr", "row %d (%llu,%llu): got %s", i, rows[i].off, rows[i].size, dw_hr_name(hr));
            CHECKF(local == rows[i].local, "row %d: isLocal %d want %d", i, (int)local, rows[i].local);
            /* partialSize = size when the fragment is in range, else 0 */
            CHECKF(partial == (rows[i].hr == S_OK ? rows[i].size : 0), "row %d: partialSize %llu", i, partial);
        }
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_stream_methods_repeatable)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL; int i;
    if (SUCCEEDED(gap_rstream(&x, URL_B, &s)) && s) {
        for (i = 0; i < 100; i++) {
            UINT64 sz = 5; BOOL loc; UINT64 part;
            CHECK(IDWriteRemoteFontFileStream_GetLocality(s) == DWRITE_LOCALITY_REMOTE);
            CHECK_HR(IDWriteRemoteFontFileStream_GetLocalFileSize(s, &sz), S_OK);
            CHECK(sz == 0);
            CHECK_HR(IDWriteRemoteFontFileStream_GetFileFragmentLocality(s, 0, 1, &loc, &part), E_INVALIDARG);
        }
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}

/* ---- BeginDownload argument validation (rejected before any request is created) ----------------------------------------------- */
TEST(gap0_remote_begin_download_fragment_out_of_range)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    static const DWRITE_FILE_FRAGMENT bad[] = { { 0, 1 }, { 1, 0 }, { 0, 0x1000 }, { 0x1000, 0x10 }, { ~0ull, 1 }, { 0, ~0ull } };
    static const GUID op = { 0x6e2b5d58, 0x1c2f, 0x4a0e, { 0x9a, 0x01, 0x55, 0x10, 0x0d, 0x6b, 0x77, 0x01 } };
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        int i;
        for (i = 0; i < G_N(bad); i++) {
            IDWriteAsyncResult *r = (IDWriteAsyncResult *)(ULONG_PTR)1;
            HRESULT hr = IDWriteRemoteFontFileStream_BeginDownload(s, &op, &bad[i], 1, &r);
            t_check(hr == E_INVALIDARG, __FILE__, __LINE__, "BeginDownload bad fragment", "row %d: got %s", i, dw_hr_name(hr));
            CHECKF(r == NULL, "row %d: asyncResult not zeroed", i);
        }
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_begin_download_any_bad_fragment_in_list)
{
    GAP_ROPEN(x);
    IDWriteRemoteFontFileStream *s = NULL;
    /* the validation loop stops at the first out-of-range fragment, whatever its position */
    static const DWRITE_FILE_FRAGMENT list1[] = { { 0, 0 }, { 0, 1 } };
    static const DWRITE_FILE_FRAGMENT list2[] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 5, 5 } };
    static const GUID op = { 0x6e2b5d58, 0x1c2f, 0x4a0e, { 0x9a, 0x01, 0x55, 0x10, 0x0d, 0x6b, 0x77, 0x02 } };
    if (SUCCEEDED(gap_rstream(&x, URL_A, &s)) && s) {
        IDWriteAsyncResult *r = (IDWriteAsyncResult *)(ULONG_PTR)1;
        CHECK_HR(IDWriteRemoteFontFileStream_BeginDownload(s, &op, list1, 2, &r), E_INVALIDARG); CHECK_OUT_NULL(r);
        r = (IDWriteAsyncResult *)(ULONG_PTR)1;
        CHECK_HR(IDWriteRemoteFontFileStream_BeginDownload(s, &op, list2, 4, &r), E_INVALIDARG); CHECK_OUT_NULL(r);
        CHECK(IDWriteRemoteFontFileStream_GetLocality(s) == DWRITE_LOCALITY_REMOTE);
    }
    DW_RELEASE(s);
    gap_rclose(&x);
}

/* ---- loader level (IDWriteFontFileLoader / IDWriteRemoteFontFileLoader seen from the application) ------------------------------- */
TEST(gap0_remote_loader_create_stream_not_local)
{
    GAP_ROPEN(x);
    IDWriteFontFileStream *s = (IDWriteFontFileStream *)(ULONG_PTR)1;
    /* The IDWriteFontFileLoader entry point only hands out streams that are already local. */
    CHECK_HR(gap_rplain(&x, URL_A, &s), DWRITE_E_REMOTEFONT_);
    CHECK_OUT_NULL(s);
    gap_rclose(&x);
}
TEST(gap0_remote_loader_create_stream_not_local_repeat)
{
    GAP_ROPEN(x);
    IDWriteFontFileStream *s = NULL; IDWriteRemoteFontFileStream *r = NULL; int i;
    CHECK_HR(gap_rstream(&x, URL_A, &r), S_OK);         /* the cached stream exists; CreateStreamFromKey must still refuse it */
    for (i = 0; i < 5; i++) {
        s = (IDWriteFontFileStream *)(ULONG_PTR)1;
        CHECK_HR(gap_rplain(&x, URL_A, &s), DWRITE_E_REMOTEFONT_);
        CHECK_OUT_NULL(s);
    }
    if (r) CHECK(IDWriteRemoteFontFileStream_GetLocality(r) == DWRITE_LOCALITY_REMOTE);
    DW_RELEASE(r);
    gap_rclose(&x);
}
TEST(gap0_remote_loader_locality_from_key)
{
    GAP_ROPEN(x);
    static const WCHAR *const urls[] = { URL_A, URL_B, URL_C, L"https://gap0.example.invalid/", L"https://gap0.example.invalid/x/y/z/q.ttc?x=1" };
    int i;
    for (i = 0; i < G_N(urls); i++) {
        DWRITE_LOCALITY loc = (DWRITE_LOCALITY)9;
        HRESULT hr = gap_rloc(&x, urls[i], &loc);
        t_check(hr == S_OK, __FILE__, __LINE__, "GetLocalityFromKey", "%d: %s", i, dw_hr_name(hr));
        CHECKF(loc == DWRITE_LOCALITY_REMOTE, "%d: locality %d", i, (int)loc);       /* a stream with no local data reports REMOTE (never fails) */
    }
    gap_rclose(&x);
}
TEST(gap0_remote_loader_locality_then_stream)
{
    GAP_ROPEN(x);
    DWRITE_LOCALITY loc = (DWRITE_LOCALITY)9; IDWriteRemoteFontFileStream *s = NULL;
    CHECK_HR(gap_rloc(&x, URL_B, &loc), S_OK);
    CHECK(loc == DWRITE_LOCALITY_REMOTE);
    CHECK_HR(gap_rstream(&x, URL_B, &s), S_OK);
    CHECK(s != NULL);
    DW_RELEASE(s);
    gap_rclose(&x);
}
TEST(gap0_remote_loader_locality_odd_key_size)
{
    GAP_ROPEN(x);
    WCHAR url[] = URL_A; BYTE key[256]; DWRITE_LOCALITY loc = (DWRITE_LOCALITY)9; UINT32 n = (UINT32)lstrlenW(url) * 2;
    memcpy(key, url, n); key[n] = 1;
    CHECK_HR(IDWriteRemoteFontFileLoader_GetLocalityFromKey(x.rl, key, n + 1, &loc), S_OK);
    CHECK(loc == DWRITE_LOCALITY_REMOTE);
    gap_rclose(&x);
}
TEST(gap0_remote_loader_com_basics)
{
    GAP_ROPEN(x);
    IDWriteFontFileLoader *fl = NULL; IDWriteRemoteFontFileLoader *again = NULL;
    CHECK_HR(IDWriteRemoteFontFileLoader_QueryInterface(x.rl, &IID_IDWriteFontFileLoader, (void **)&fl), S_OK);
    CHECK(fl == (IDWriteFontFileLoader *)x.rl);
    CHECK_HR(IDWriteFontFileLoader_QueryInterface(fl, &IID_IDWriteRemoteFontFileLoader, (void **)&again), S_OK);
    CHECK(again == x.rl);
    DW_RELEASE(fl); DW_RELEASE(again);
    EXPECT_REFS(x.rl, 1);
    gap_rclose(&x);
}

/* ---- through IDWriteFontFile references created by the loader -------------------------------------------------------------------- */
TEST(gap0_remote_file_reference_key_drives_loader)
{
    GAP_ROPEN(x);
    IDWriteFontFile *file = NULL; const void *key = NULL; UINT32 ks = 0;
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(x.f, (IDWriteFontFileLoader *)x.rl), S_OK);
    HRESULT hr = IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://gap0.example.invalid/fonts/", L"d.ttf", &file);
    CHECK_HR(hr, S_OK);
    if (SUCCEEDED(hr) && file) {
        IDWriteFontFileLoader *ld = NULL;
        CHECK_HR(IDWriteFontFile_GetReferenceKey(file, &key, &ks), S_OK);
        CHECK(key != NULL && ks >= 2 && (ks % 2) == 0);
        CHECK_HR(IDWriteFontFile_GetLoader(file, &ld), S_OK);
        CHECK(ld == (IDWriteFontFileLoader *)x.rl);
        DW_RELEASE(ld);
        if (key && ks) {
            DWRITE_LOCALITY loc = (DWRITE_LOCALITY)9; IDWriteRemoteFontFileStream *s = NULL;
            t_note("reference key text: %.*ls", (int)(ks / 2), (const WCHAR *)key);
            CHECK_HR(IDWriteRemoteFontFileLoader_GetLocalityFromKey(x.rl, key, ks, &loc), S_OK);
            CHECK(loc == DWRITE_LOCALITY_REMOTE);
            CHECK_HR(IDWriteRemoteFontFileLoader_CreateRemoteStreamFromKey(x.rl, key, ks, &s), S_OK);
            CHECK(s != NULL);
            if (s) CHECK(IDWriteRemoteFontFileStream_GetLocality(s) == DWRITE_LOCALITY_REMOTE);
            DW_RELEASE(s);
        }
    }
    DW_RELEASE(file);
    IDWriteFactory_UnregisterFontFileLoader(x.f, (IDWriteFontFileLoader *)x.rl);
    gap_rclose(&x);
}
TEST(gap0_remote_file_reference_two_urls_two_keys)
{
    GAP_ROPEN(x);
    IDWriteFontFile *a = NULL, *b = NULL;
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(x.f, (IDWriteFontFileLoader *)x.rl), S_OK);
    IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://gap0.example.invalid/fonts/", L"e1.ttf", &a);
    IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://gap0.example.invalid/fonts/", L"e2.ttf", &b);
    if (a && b) {
        const void *ka, *kb; UINT32 sa = 0, sb = 0;
        CHECK_HR(IDWriteFontFile_GetReferenceKey(a, &ka, &sa), S_OK);
        CHECK_HR(IDWriteFontFile_GetReferenceKey(b, &kb, &sb), S_OK);
        CHECK(!(sa == sb && !memcmp(ka, kb, sa)));
    } else t_skip("CreateFontFileReferenceFromUrl unavailable");
    DW_RELEASE(a); DW_RELEASE(b);
    IDWriteFactory_UnregisterFontFileLoader(x.f, (IDWriteFontFileLoader *)x.rl);
    gap_rclose(&x);
}
TEST(gap0_remote_file_analyze_note)
{
    GAP_ROPEN(x);
    IDWriteFontFile *file = NULL;
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(x.f, (IDWriteFontFileLoader *)x.rl), S_OK);
    IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://gap0.example.invalid/fonts/", L"f.ttf", &file);
    if (file) {
        BOOL sup = TRUE; DWRITE_FONT_FILE_TYPE ft = DWRITE_FONT_FILE_TYPE_UNKNOWN; DWRITE_FONT_FACE_TYPE fc = DWRITE_FONT_FACE_TYPE_UNKNOWN; UINT32 nf = 7;
        HRESULT hr = IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf);
        /* Analysis needs the bytes: the loader's local-only stream path refuses a remote stream. */
        t_note("Analyze on a not-downloaded remote file: %s supported=%d type=%d face=%d faces=%u", dw_hr_name(hr), (int)sup, (int)ft, (int)fc, nf);
        CHECK(FAILED(hr) || !sup);
        DW_RELEASE(file);
    }
    IDWriteFactory_UnregisterFontFileLoader(x.f, (IDWriteFontFileLoader *)x.rl);
    gap_rclose(&x);
}
