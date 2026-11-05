/* IDWriteFontFileStream::GetLastWriteTime as consumed by dwrite.dll.
 *
 * Win10 dwrite.dll: IDWriteFontFaceReference::GetFileTime forwards to the stream's GetLastWriteTime (E_NOTIMPL reads as time 0 with S_OK; other failures
 * leave through a C++ exception and are not exercised); the built-in local loader's stream returns the last-write time stored in the reference key
 * (GetLastWriteTime copies it, always S_OK) and IDWriteLocalFontFileLoader::GetLastWriteTimeFromKey
 * deserializes the first 8 key bytes (verbatim, every bit pattern).
 */
#include "g1.h"

static UINT64 u64(const FILETIME *ft) { return ((UINT64)ft->dwHighDateTime << 32) | ft->dwLowDateTime; }

typedef struct s_fx { g1_fx dummy; IDWriteFactory *f; IDWriteFactory3 *f3; dwx_mem_loader *ld; uint8_t *blob; size_t n; int ok; } s_fx;
static int s_open(s_fx *x)
{
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f || FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory3, (void **)&x->f3))) { DW_RELEASE(x->f); t_skip("IDWriteFactory3 unavailable"); return 0; }
    x->ld = dwx_loader_new();
    x->blob = ttf_build_default(&x->n);
    if (!x->blob || FAILED(dwx_loader_register(x->ld, x->f))) { t_skip("fixture failed"); return 0; }
    x->ok = 1;
    return 1;
}
static void s_close(s_fx *x)
{
    if (x->ld) { dwx_loader_unregister(x->ld); dwx_release(x->ld); }
    free(x->blob); DW_RELEASE(x->f3); DW_RELEASE(x->f);
}
#define S_BEGIN() s_fx x; if (!s_open(&x)) { s_close(&x); return; }
#define S_END() s_close(&x)
/* A new blob key whose stream will report `t`. */
static IDWriteFontFaceReference *ref_with_time(s_fx *x, UINT64 t, IDWriteFontFile **file_out)
{
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *r = NULL; UINT32 key;
    x->ld->last_write = t;
    key = dwx_loader_add(x->ld, x->blob, x->n);
    if (FAILED(dwx_loader_file(x->ld, x->f, key, &file)) || !file) return NULL;
    if (FAILED(IDWriteFactory3_CreateFontFaceReference_(x->f3, file, 0, DWRITE_FONT_SIMULATIONS_NONE, &r))) r = NULL;
    if (r) IDWriteFontFaceReference_GetFileSize(r);     /* makes the loader create the stream now, with the time set above */
    if (file_out) *file_out = file; else IDWriteFontFile_Release(file);
    return r;
}
static UINT64 filetime_of(IDWriteFontFaceReference *r, HRESULT *hr_out)
{
    FILETIME ft = { 0xDEADBEEF, 0xFEEDFACE }; HRESULT hr = IDWriteFontFaceReference_GetFileTime(r, &ft);
    if (hr_out) *hr_out = hr;
    return u64(&ft);
}

/* ---- custom streams, observed through IDWriteFontFaceReference::GetFileTime ---------------------------------------------- */
TEST(gap1_lwt_value_matrix)
{
    static const UINT64 vals[] = { 0, 1, 2, 0xFF, 0x100, 0xFFFF, 0x10000, 0xFFFFFFFFull, 0x100000000ull, 0x0000000100000001ull, 0x01D0000000000000ull,
                                   0x01D5A3B2C4D5E6F7ull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0x8000000000000001ull, 0xFFFFFFFF00000000ull,
                                   0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFFull, 0xAAAAAAAA55555555ull, 0x5555555AAAAAAAAull };
    unsigned i;
    S_BEGIN();
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        IDWriteFontFaceReference *r = ref_with_time(&x, vals[i], NULL); HRESULT hr; UINT64 got;
        if (!r) { t_note("reference %u unavailable", i); continue; }
        got = filetime_of(r, &hr);
        CHECK_HR(hr, S_OK);
        t_check(got == vals[i], __FILE__, __LINE__, "stream time reaches the caller verbatim", "want %016llx got %016llx", (unsigned long long)vals[i], (unsigned long long)got);
        DW_RELEASE(r);
    }
    S_END();
}
TEST(gap1_lwt_interleaved_references_report_stream_times)
{
    S_BEGIN();
    IDWriteFontFaceReference *a = ref_with_time(&x, 0x1111111111111111ull, NULL), *b = ref_with_time(&x, 0x2222222222222222ull, NULL), *c = ref_with_time(&x, 0x3333333333333333ull, NULL);
    HRESULT hr;
    if (a && b && c) {
        UINT64 tb = filetime_of(b, &hr); CHECK_HR(hr, S_OK);
        UINT64 ta = filetime_of(a, &hr); CHECK_HR(hr, S_OK);
        UINT64 tc = filetime_of(c, &hr); CHECK_HR(hr, S_OK);
        /* each answer is one of the times the loader handed to a stream it created */
        CHECK(ta == 0x1111111111111111ull || ta == 0x2222222222222222ull || ta == 0x3333333333333333ull);
        CHECK(tb == 0x1111111111111111ull || tb == 0x2222222222222222ull || tb == 0x3333333333333333ull);
        CHECK(tc == 0x1111111111111111ull || tc == 0x2222222222222222ull || tc == 0x3333333333333333ull);
        t_note("times after interleaved queries: a=%016llx b=%016llx c=%016llx (streams created: %ld)", (unsigned long long)ta, (unsigned long long)tb, (unsigned long long)tc, x.ld->n_create);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    S_END();
}
TEST(gap1_lwt_two_references_report_loader_times)
{
    S_BEGIN();
    IDWriteFontFaceReference *a = ref_with_time(&x, 0xAAAA000000000001ull, NULL), *b = ref_with_time(&x, 0xBBBB000000000002ull, NULL);
    HRESULT hr;
    if (a && b) {
        UINT64 ta = filetime_of(a, &hr); CHECK_HR(hr, S_OK);
        UINT64 tb = filetime_of(b, &hr); CHECK_HR(hr, S_OK);
        UINT64 ta2 = filetime_of(a, &hr); CHECK_HR(hr, S_OK);
        CHECK(ta == 0xAAAA000000000001ull || ta == 0xBBBB000000000002ull); CHECK(tb == 0xAAAA000000000001ull || tb == 0xBBBB000000000002ull);
        t_note("a=%016llx b=%016llx a again=%016llx (streams created: %ld)", (unsigned long long)ta, (unsigned long long)tb, (unsigned long long)ta2, x.ld->n_create);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    S_END();
}
TEST(gap1_lwt_stream_called_and_balanced)
{
    S_BEGIN();
    IDWriteFontFaceReference *r = ref_with_time(&x, 0x42, NULL); HRESULT hr; int i;
    if (r) {
        EXPECT_EQ(dwx_loader_total_outstanding(x.ld), 0);
        for (i = 0; i < 3; i++) { filetime_of(r, &hr); CHECK_HR(hr, S_OK); }
        { LONG calls = 0; int k; for (k = 0; k < x.ld->nstreams; k++) calls += x.ld->streams[k]->n_lastwrite; CHECK(calls >= 3); t_note("GetLastWriteTime calls for 3 queries: %ld, streams created: %ld", calls, x.ld->n_create); }
        EXPECT_EQ(dwx_loader_total_outstanding(x.ld), 0);
    }
    DW_RELEASE(r);
    S_END();
}
TEST(gap1_lwt_not_asked_for_size_or_reference_creation)
{
    S_BEGIN();
    IDWriteFontFaceReference *r = ref_with_time(&x, 0x42, NULL); int k; LONG calls = 0;
    if (r) {
        for (k = 0; k < x.ld->nstreams; k++) calls += x.ld->streams[k]->n_lastwrite;
        EXPECT_EQ(calls, 0);         /* creating a reference and asking for the file size never asks for the write time */
    }
    DW_RELEASE(r);
    S_END();
}
TEST(gap1_lwt_notimpl_stream_time_zero_ok)
{
    S_BEGIN();
    IDWriteFontFaceReference *r = ref_with_time(&x, 0x7777777777777777ull, NULL); HRESULT hr;
    if (r) {
        int k;
        IDWriteFontFaceReference_GetFileSize(r);              /* makes the stream exist */
        for (k = 0; k < x.ld->nstreams; k++) x.ld->streams[k]->lastwrite_notimpl = 1;
        EXPECT_EQ(filetime_of(r, &hr), 0); CHECK_HR(hr, S_OK);
        for (k = 0; k < x.ld->nstreams; k++) x.ld->streams[k]->lastwrite_notimpl = 0;
        EXPECT_EQ(filetime_of(r, &hr), 0x7777777777777777ull); CHECK_HR(hr, S_OK);
    }
    DW_RELEASE(r);
    S_END();
}
TEST(gap1_lwt_zero_time_is_a_valid_answer)
{
    S_BEGIN();
    IDWriteFontFaceReference *r = ref_with_time(&x, 0, NULL); HRESULT hr;
    if (r) { EXPECT_EQ(filetime_of(r, &hr), 0); CHECK_HR(hr, S_OK); }
    DW_RELEASE(r);
    S_END();
}
TEST(gap1_lwt_reference_survives_file_release)
{
    S_BEGIN();
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *r = ref_with_time(&x, 0x0123456789ABCDEFull, &file); HRESULT hr;
    if (r && file) {
        DW_RELEASE(file);
        EXPECT_EQ(filetime_of(r, &hr), 0x0123456789ABCDEFull); CHECK_HR(hr, S_OK);
    }
    DW_RELEASE(file); DW_RELEASE(r);
    S_END();
}
TEST(gap1_lwt_via_font_file_reference_of_face)
{
    /* a reference obtained from a face reports the time of the face's stream */
    S_BEGIN();
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *r = ref_with_time(&x, 0x00C0FFEE00C0FFEEull, &file);
    if (r && file) {
        IDWriteFontFace3 *face = NULL; IDWriteFontFaceReference *r2 = NULL; HRESULT hr;
        if (SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &face)) && face) {
            if (SUCCEEDED(IDWriteFontFace3_GetFontFaceReference(face, &r2)) && r2) { EXPECT_EQ(filetime_of(r2, &hr), 0x00C0FFEE00C0FFEEull); CHECK_HR(hr, S_OK); }
            DW_RELEASE(r2); DW_RELEASE(face);
        }
    }
    DW_RELEASE(file); DW_RELEASE(r);
    S_END();
}
TEST(gap1_lwt_multiple_references_same_file)
{
    S_BEGIN();
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *r1 = ref_with_time(&x, 0x1234567800000001ull, &file), *r2 = NULL, *r3 = NULL; HRESULT hr;
    if (r1 && file) {
        IDWriteFactory3_CreateFontFaceReference_(x.f3, file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &r2);
        IDWriteFactory3_CreateFontFaceReference_(x.f3, file, 0, DWRITE_FONT_SIMULATIONS_OBLIQUE, &r3);
        EXPECT_EQ(filetime_of(r1, &hr), 0x1234567800000001ull);
        if (r2) { EXPECT_EQ(filetime_of(r2, &hr), 0x1234567800000001ull); CHECK_HR(hr, S_OK); }
        if (r3) { EXPECT_EQ(filetime_of(r3, &hr), 0x1234567800000001ull); CHECK_HR(hr, S_OK); }
    }
    DW_RELEASE(file); DW_RELEASE(r1); DW_RELEASE(r2); DW_RELEASE(r3);
    S_END();
}

/* ---- the built-in local loader ------------------------------------------------------------------------------------------- */
static int write_scratch(const char *leaf, const void *data, size_t n, WCHAR *wpath, int cap)
{
    char p[MAX_PATH]; HANDLE h; DWORD wr = 0;
    t_tmpfile(p, sizeof p, leaf);
    h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    WriteFile(h, data, (DWORD)n, &wr, NULL); CloseHandle(h);
    return wr == n && dw_wide(p, wpath, cap) > 0;
}
static IDWriteFontFileLoader *loader_of(IDWriteFontFile *file, const void **key, UINT32 *ks)
{
    IDWriteFontFileLoader *l = NULL;
    if (FAILED(IDWriteFontFile_GetLoader(file, &l)) || !l) return NULL;
    if (FAILED(IDWriteFontFile_GetReferenceKey(file, key, ks))) { IDWriteFontFileLoader_Release(l); return NULL; }
    return l;
}
TEST(gap1_lwt_local_stream_returns_key_time)
{
    static const UINT64 vals[] = { 0, 1, 0x00000000FFFFFFFFull, 0x0000000100000000ull, 0x01D0123456789ABCull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull };
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); size_t n; uint8_t *b = ttf_build_default(&n); WCHAR wp[MAX_PATH]; unsigned i;
    if (!f || !b || !write_scratch("g1_lwt_local.ttf", b, n, wp, MAX_PATH)) { DW_RELEASE(f); free(b); t_skip("scratch file unavailable"); return; }
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        IDWriteFontFile *file = NULL; FILETIME ft; const void *key = NULL; UINT32 ks = 0; IDWriteFontFileLoader *l;
        ft.dwLowDateTime = (DWORD)vals[i]; ft.dwHighDateTime = (DWORD)(vals[i] >> 32);
        CHECK_HR(IDWriteFactory_CreateFontFileReference(f, wp, &ft, &file), S_OK);
        if (!file) continue;
        l = loader_of(file, &key, &ks);
        if (l) {
            IDWriteFontFileStream *s = NULL; UINT64 t = 0x5A5A; IDWriteLocalFontFileLoader *ll = NULL;
            { HRESULT chr = IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s); t_note("key time %016llx: CreateStreamFromKey -> %s", (unsigned long long)vals[i], dw_hr_name(chr)); }
            if (s) {
                CHECK_HR(IDWriteFontFileStream_GetLastWriteTime(s, &t), S_OK);
                t_check(t == vals[i], __FILE__, __LINE__, "stream last write time", "want %016llx got %016llx", (unsigned long long)vals[i], (unsigned long long)t);
                IDWriteFontFileStream_Release(s);
            }
            if (SUCCEEDED(IDWriteFontFileLoader_QueryInterface(l, &IID_IDWriteLocalFontFileLoader, (void **)&ll)) && ll) {
                FILETIME g = { 7, 7 };
                CHECK_HR(IDWriteLocalFontFileLoader_GetLastWriteTimeFromKey(ll, key, ks, &g), S_OK);
                EXPECT_EQ(u64(&g), vals[i]);
                IDWriteLocalFontFileLoader_Release(ll);
            }
            IDWriteFontFileLoader_Release(l);
        }
        DW_RELEASE(file);
    }
    free(b); IDWriteFactory_Release(f);
}
TEST(gap1_lwt_local_reference_file_time_matches_key)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory3 *f3 = NULL; size_t n; uint8_t *b = ttf_build_default(&n); WCHAR wp[MAX_PATH];
    if (!f || !b || FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) || !write_scratch("g1_lwt_ref.ttf", b, n, wp, MAX_PATH)) {
        DW_RELEASE(f3); DW_RELEASE(f); free(b); t_skip("fixture unavailable"); return;
    }
    {
        FILETIME ft = { 0x89ABCDEF, 0x01234567 }; IDWriteFontFaceReference *r = NULL; HRESULT hr;
        CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, wp, &ft, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), S_OK);
        if (r) { UINT64 t = filetime_of(r, &hr); t_note("explicit time %016llx -> %016llx, %s", (unsigned long long)u64(&ft), (unsigned long long)t, dw_hr_name(hr)); DW_RELEASE(r); }
        r = NULL;
        CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, wp, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), S_OK);
        if (r) {
            WIN32_FILE_ATTRIBUTE_DATA d; char p[MAX_PATH]; dw_narrow(wp, p, sizeof p);
            if (GetFileAttributesExA(p, GetFileExInfoStandard, &d)) EXPECT_EQ(filetime_of(r, &hr), u64(&d.ftLastWriteTime));
            DW_RELEASE(r);
        }
    }
    DW_RELEASE(f3); DW_RELEASE(f); free(b);
}
TEST(gap1_lwt_local_stream_time_stable_across_calls)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); size_t n; uint8_t *b = ttf_build_default(&n); WCHAR wp[MAX_PATH];
    if (!f || !b || !write_scratch("g1_lwt_stab.ttf", b, n, wp, MAX_PATH)) { DW_RELEASE(f); free(b); t_skip("scratch file unavailable"); return; }
    {
        IDWriteFontFile *file = NULL; const void *key = NULL; UINT32 ks = 0; IDWriteFontFileLoader *l;
        CHECK_HR(IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file), S_OK);
        l = file ? loader_of(file, &key, &ks) : NULL;
        if (l) {
            IDWriteFontFileStream *s1 = NULL, *s2 = NULL; int i;
            CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s1), S_OK);
            CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s2), S_OK);
            if (s1 && s2) {
                UINT64 a = 0, c = 0;
                IDWriteFontFileStream_GetLastWriteTime(s1, &a);
                for (i = 0; i < 10; i++) { c = 1; CHECK_HR(IDWriteFontFileStream_GetLastWriteTime(s2, &c), S_OK); EXPECT_EQ(c, a); }
                CHECK(a != 0);
            }
            DW_RELEASE(s1); DW_RELEASE(s2); IDWriteFontFileLoader_Release(l);
        }
        DW_RELEASE(file);
    }
    free(b); IDWriteFactory_Release(f);
}
TEST(gap1_lwt_system_font_stream_time_matches_disk_note)
{
    IDWriteFactory *f = dw_shared(); const char *c[] = { "arial.ttf", NULL }; char path[MAX_PATH]; WCHAR wp[MAX_PATH]; WIN32_FILE_ATTRIBUTE_DATA d;
    if (!f || !dw_find_system_font(c, path, sizeof path) || !GetFileAttributesExA(path, GetFileExInfoStandard, &d)) { t_skip("arial.ttf not installed"); return; }
    dw_wide(path, wp, MAX_PATH);
    {
        IDWriteFontFile *file = NULL; const void *key = NULL; UINT32 ks = 0; IDWriteFontFileLoader *l;
        CHECK_HR(IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file), S_OK);
        l = file ? loader_of(file, &key, &ks) : NULL;
        if (l) {
            IDWriteFontFileStream *s = NULL; UINT64 t = 0;
            CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s), S_OK);
            if (s) { CHECK_HR(IDWriteFontFileStream_GetLastWriteTime(s, &t), S_OK); EXPECT_EQ(t, u64(&d.ftLastWriteTime)); IDWriteFontFileStream_Release(s); }
            IDWriteFontFileLoader_Release(l);
        }
        DW_RELEASE(file);
    }
}
