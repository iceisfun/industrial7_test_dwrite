/* IDWriteRemoteFontFileLoader::CreateFontFileReferenceFromUrl (the loader returned by IDWriteFactory3::CreateHttpFontFileLoader) and
 * CreateHttpFontFileLoader argument handling.
 * Win10 probing: CreateFontFileReferenceFromUrl writes *fontFile = NULL first, normalizes
 * the URL into a wide string and then calls the factory's CreateCustomFontFileReference
 * with (key = normalized URL as UTF-16, no terminator, size = 2 * length, loader = this). normalization starts with a 256-character
 * buffer; baseUrl NULL or "" -> InternetCanonicalizeUrlW(fileUrl, flags 0), otherwise InternetCombineUrlW(base, file, flags 0);
 * ERROR_INSUFFICIENT_BUFFER grows the buffer and retries; any other wininet error is returned as HRESULT_FROM_WIN32.
 * CreateFontFileReference through a factory that has not registered the loader fails with E_INVALIDARG.
 * IDWriteFactory::CreateHttpFontFileLoader zeroes the out pointer, copies referrer and headers (either may be NULL) and
 * always succeeds. */
#include "g2.h"

typedef struct { IDWriteFactory *f; IDWriteFactory5 *f5; IDWriteRemoteFontFileLoader *rl; int reg; } urlfx;
static int url_open(urlfx *x, int do_register)
{
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("factory"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory5, (void **)&x->f5))) { DW_RELEASE(x->f); t_skip("IDWriteFactory5 unsupported"); return 0; }
    if (FAILED(IDWriteFactory5_CreateHttpFontFileLoader(x->f5, NULL, NULL, &x->rl)) || !x->rl) { DW_RELEASE(x->f5); DW_RELEASE(x->f); t_skip("CreateHttpFontFileLoader failed"); return 0; }
    if (do_register) { if (SUCCEEDED(IDWriteFactory_RegisterFontFileLoader(x->f, (IDWriteFontFileLoader *)x->rl))) x->reg = 1; }
    return 1;
}
static void url_close(urlfx *x)
{
    if (x->reg) IDWriteFactory_UnregisterFontFileLoader(x->f, (IDWriteFontFileLoader *)x->rl);
    DW_RELEASE(x->rl); DW_RELEASE(x->f5); DW_RELEASE(x->f);
}
/* Creates the reference and returns the key as a wide string (malloc'd, NUL terminated copy) plus byte size. */
static HRESULT url_ref(urlfx *x, const WCHAR *base, const WCHAR *file, WCHAR **key, UINT32 *ksize)
{
    IDWriteFontFile *ff = (IDWriteFontFile *)G2_SENT; HRESULT hr;
    *key = NULL; *ksize = 0;
    hr = IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x->rl, x->f, base, file, &ff);
    if (FAILED(hr)) { CHECK_OUT_NULL(ff); return hr; }
    if (ff && ff != G2_SENT) {
        const void *k = NULL; UINT32 ks = 0;
        if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(ff, &k, &ks))) {
            *key = (WCHAR *)calloc(ks / 2 + 2, sizeof(WCHAR)); memcpy(*key, k, ks); *ksize = ks;
        }
        IDWriteFontFile_Release(ff);
    }
    return hr;
}
#define URL_EXPECT(x, base, file, want) do { WCHAR *k_; UINT32 s_; HRESULT hr_ = url_ref(&x, base, file, &k_, &s_); \
    CHECKF(hr_ == S_OK, "%ls + %ls -> %s", (base) ? (base) : L"(null)", (file) ? (file) : L"(null)", dw_hr_name(hr_)); \
    if (k_) { CHECKF(s_ == 2 * wcslen(want) && !wcsncmp(k_, want, wcslen(want)), "%ls + %ls -> key '%ls' (%u bytes) want '%ls'", (base) ? (base) : L"(null)", (file) ? (file) : L"(null)", k_, s_, want); free(k_); } \
    else CHECKF(0, "no key for %ls", (file) ? (file) : L"(null)"); } while (0)

TEST(gap2_url_join_relative_file)
{
    urlfx x; if (!url_open(&x, 1)) return;
    URL_EXPECT(x, L"https://example.invalid/fonts/", L"a.ttf", L"https://example.invalid/fonts/a.ttf");
    url_close(&x);
}
TEST(gap2_url_join_replaces_last_segment)
{
    urlfx x; if (!url_open(&x, 1)) return;
    URL_EXPECT(x, L"https://example.invalid/fonts/index.css", L"a.ttf", L"https://example.invalid/fonts/a.ttf");
    url_close(&x);
}
TEST(gap2_url_join_dotdot)
{
    urlfx x; if (!url_open(&x, 1)) return;
    URL_EXPECT(x, L"https://example.invalid/fonts/sub/", L"../b.ttf", L"https://example.invalid/fonts/b.ttf");
    URL_EXPECT(x, L"https://example.invalid/fonts/sub/", L"../../c.ttf", L"https://example.invalid/c.ttf");
    url_close(&x);
}
TEST(gap2_url_join_root_relative)
{
    urlfx x; if (!url_open(&x, 1)) return;
    URL_EXPECT(x, L"https://example.invalid/fonts/deep/", L"/root.ttf", L"https://example.invalid/root.ttf");
    url_close(&x);
}
TEST(gap2_url_join_absolute_file_overrides_base)
{
    urlfx x; if (!url_open(&x, 1)) return;
    URL_EXPECT(x, L"https://example.invalid/fonts/", L"https://other.invalid/z/d.ttf", L"https://other.invalid/z/d.ttf");
    url_close(&x);
}
TEST(gap2_url_join_subdirectory)
{
    urlfx x; if (!url_open(&x, 1)) return;
    URL_EXPECT(x, L"https://example.invalid/fonts/", L"sub/e.ttf", L"https://example.invalid/fonts/sub/e.ttf");
    url_close(&x);
}
TEST(gap2_url_join_http_scheme)
{
    urlfx x; if (!url_open(&x, 1)) return;
    URL_EXPECT(x, L"http://example.invalid/f/", L"g.otf", L"http://example.invalid/f/g.otf");
    url_close(&x);
}
TEST(gap2_url_null_and_empty_base_canonicalize_only)
{
    urlfx x; if (!url_open(&x, 1)) return;
    URL_EXPECT(x, NULL, L"https://example.invalid/fonts/h.ttf", L"https://example.invalid/fonts/h.ttf");
    URL_EXPECT(x, L"", L"https://example.invalid/fonts/i.ttf", L"https://example.invalid/fonts/i.ttf");
    url_close(&x);
}
TEST(gap2_url_key_size_even_without_terminator)
{
    urlfx x; WCHAR *k; UINT32 s; HRESULT hr;
    if (!url_open(&x, 1)) return;
    hr = url_ref(&x, L"https://example.invalid/", L"k.ttf", &k, &s);
    CHECK_HR(hr, S_OK);
    if (k) { CHECK(s % 2 == 0); CHECKF(s == 2 * wcslen(L"https://example.invalid/k.ttf"), "size %u", s); free(k); }
    url_close(&x);
}
TEST(gap2_url_same_url_same_key_bytes)
{
    urlfx x; WCHAR *k1, *k2; UINT32 s1, s2;
    if (!url_open(&x, 1)) return;
    CHECK_HR_OK(url_ref(&x, L"https://example.invalid/", L"m.ttf", &k1, &s1));
    CHECK_HR_OK(url_ref(&x, NULL, L"https://example.invalid/m.ttf", &k2, &s2));
    if (k1 && k2) CHECK(s1 == s2 && !memcmp(k1, k2, s1));          /* base+relative and the equivalent absolute URL give the same key */
    free(k1); free(k2);
    url_close(&x);
}
TEST(gap2_url_different_urls_different_keys)
{
    urlfx x; WCHAR *k1, *k2; UINT32 s1, s2;
    if (!url_open(&x, 1)) return;
    CHECK_HR_OK(url_ref(&x, L"https://example.invalid/", L"n1.ttf", &k1, &s1));
    CHECK_HR_OK(url_ref(&x, L"https://example.invalid/", L"n2.ttf", &k2, &s2));
    if (k1 && k2) CHECK(s1 != s2 || memcmp(k1, k2, s1));
    free(k1); free(k2);
    url_close(&x);
}
TEST(gap2_url_buffer_growth_lengths)
{
    /* The normalization buffer starts at 256 characters and grows on ERROR_INSUFFICIENT_BUFFER. */
    static const unsigned lens[] = { 100, 200, 240, 250, 254, 255, 256, 257, 258, 300, 511, 512, 513, 1000, 1500, 2000 };
    urlfx x; size_t i;
    if (!url_open(&x, 1)) return;
    for (i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        WCHAR *url = (WCHAR *)calloc(lens[i] + 8, sizeof(WCHAR)), *k; UINT32 s; HRESULT hr; unsigned n = lens[i], j;
        wcscpy(url, L"https://example.invalid/");
        for (j = (unsigned)wcslen(url); j + 4 < n; j++) url[j] = L'a' + (j % 26);
        wcscpy(url + j, L".ttf");
        hr = url_ref(&x, NULL, url, &k, &s);
        CHECKF(hr == S_OK, "length %u: %s", n, dw_hr_name(hr));
        if (k) { CHECKF(s == 2 * wcslen(url) && !wcsncmp(k, url, wcslen(url)), "length %u: key size %u want %u", n, s, (unsigned)(2 * wcslen(url))); free(k); }
        free(url);
    }
    url_close(&x);
}
TEST(gap2_url_buffer_growth_via_combine)
{
    static const unsigned lens[] = { 200, 255, 256, 257, 400, 1000 };
    urlfx x; size_t i;
    if (!url_open(&x, 1)) return;
    for (i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        WCHAR rel[2100], want[2200], *k; UINT32 s; HRESULT hr; unsigned j;
        for (j = 0; j < lens[i]; j++) rel[j] = L'a' + (j % 26);
        rel[j] = 0; wcscat(rel, L".ttf");
        wcscpy(want, L"https://example.invalid/fonts/"); wcscat(want, rel);
        hr = url_ref(&x, L"https://example.invalid/fonts/", rel, &k, &s);
        CHECKF(hr == S_OK, "relative length %u: %s", lens[i], dw_hr_name(hr));
        if (k) { CHECKF(s == 2 * wcslen(want) && !wcsncmp(k, want, wcslen(want)), "relative length %u: key %u bytes", lens[i], s); free(k); }
    }
    url_close(&x);
}
TEST(gap2_url_unregistered_loader_rejected)
{
    urlfx x; IDWriteFontFile *ff = (IDWriteFontFile *)G2_SENT; HRESULT hr;
    if (!url_open(&x, 0)) return;
    hr = IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://example.invalid/", L"a.ttf", &ff);
    CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(ff);
    url_close(&x);
}
TEST(gap2_url_registered_with_other_factory_only)
{
    /* The factory argument decides: registered with A, called with B -> not registered there -> E_INVALIDARG. */
    urlfx x; IDWriteFactory *b; IDWriteFontFile *ff = (IDWriteFontFile *)G2_SENT; HRESULT hr;
    if (!url_open(&x, 1)) return;
    b = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (b) {
        hr = IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, b, L"https://example.invalid/", L"a.ttf", &ff);
        CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(ff);
        DW_RELEASE(b);
    }
    url_close(&x);
}
TEST(gap2_url_unregistered_after_use)
{
    urlfx x; IDWriteFontFile *ff = (IDWriteFontFile *)G2_SENT; HRESULT hr;
    if (!url_open(&x, 1)) return;
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(x.f, (IDWriteFontFileLoader *)x.rl)); x.reg = 0;
    hr = IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://example.invalid/", L"a.ttf", &ff);
    CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(ff);
    url_close(&x);
}
TEST(gap2_url_file_reports_loader)
{
    urlfx x; IDWriteFontFile *ff = NULL;
    if (!url_open(&x, 1)) return;
    CHECK_HR_OK(IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://example.invalid/", L"p.ttf", &ff));
    if (ff) {
        IDWriteFontFileLoader *l = NULL;
        CHECK_HR_OK(IDWriteFontFile_GetLoader(ff, &l));
        CHECK(l == (IDWriteFontFileLoader *)x.rl);
        DW_RELEASE(l);
        DW_RELEASE(ff);
    }
    url_close(&x);
}
TEST(gap2_url_file_holds_loader_reference)
{
    urlfx x; IDWriteFontFile *ff = NULL; ULONG r0, r1, r2;
    if (!url_open(&x, 1)) return;
    r0 = dw_refcount((IUnknown *)x.rl);
    CHECK_HR_OK(IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://example.invalid/", L"q.ttf", &ff));
    r1 = dw_refcount((IUnknown *)x.rl);
    DW_RELEASE(ff);
    r2 = dw_refcount((IUnknown *)x.rl);
    CHECKF(r1 >= r0, "refs %lu -> %lu", r0, r1);
    CHECKF(r2 == r0, "after releasing the file: %lu vs %lu", r2, r0);
    url_close(&x);
}
TEST(gap2_url_key_survives_loader_unregistration)
{
    urlfx x; IDWriteFontFile *ff = NULL;
    if (!url_open(&x, 1)) return;
    CHECK_HR_OK(IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(x.rl, x.f, L"https://example.invalid/", L"r.ttf", &ff));
    IDWriteFactory_UnregisterFontFileLoader(x.f, (IDWriteFontFileLoader *)x.rl); x.reg = 0;
    if (ff) {
        const void *k = NULL; UINT32 ks = 0;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff, &k, &ks));          /* the file object copied the key */
        CHECK(ks == 2 * wcslen(L"https://example.invalid/r.ttf") && k && !wcsncmp((const WCHAR *)k, L"https://example.invalid/r.ttf", ks / 2));
        DW_RELEASE(ff);
    }
    url_close(&x);
}
TEST(gap2_url_invalid_inputs_noted)
{
    urlfx x; static const struct { const WCHAR *b, *f; const char *what; } c[] = {
        { L"https://example.invalid/", NULL, "NULL fileUrl with base" }, { NULL, NULL, "both NULL" }, { L"https://example.invalid/", L"", "empty fileUrl" },
        { NULL, L"", "empty everything" }, { NULL, L"relative.ttf", "relative fileUrl without base" }, { L"not a url", L"a.ttf", "garbage base" },
    };
    size_t i;
    if (!url_open(&x, 1)) return;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        WCHAR *k = NULL; UINT32 s; HRESULT hr = url_ref(&x, c[i].b, c[i].f, &k, &s);
        t_note("%s: %s%s", c[i].what, dw_hr_name(hr), k ? " (key produced)" : "");
        if (FAILED(hr)) CHECK(k == NULL);
        free(k);
    }
    url_close(&x);
}
TEST(gap2_url_unicode_and_escape_noted)
{
    urlfx x; static const WCHAR *files[] = { L"with space.ttf", L"\x00e9\x00e8.ttf", L"a%20b.ttf", L"a+b.ttf", L"q.ttf?x=1&y=2", L"f.ttf#frag" }; size_t i;
    if (!url_open(&x, 1)) return;
    for (i = 0; i < sizeof files / sizeof files[0]; i++) {
        WCHAR *k = NULL; UINT32 s; HRESULT hr = url_ref(&x, L"https://example.invalid/", files[i], &k, &s);
        t_note("'%ls' -> %s key '%ls'", files[i], dw_hr_name(hr), k ? k : L"");
        free(k);
    }
    url_close(&x);
}

/* ---- CreateHttpFontFileLoader ---------------------------------------------------------------------------------------- */
TEST(gap2_http_loader_argument_variants)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory5 *f5 = NULL; size_t i, j;
    static const WCHAR *ref[] = { NULL, L"", L"https://example.invalid/", L"x" }; static const WCHAR *hdr[] = { NULL, L"", L"Accept: */*\r\n", L"X-A: 1\r\nX-B: 2" };
    if (!f) { t_skip("factory"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&f5))) { DW_RELEASE(f); t_skip("Factory5 unsupported"); return; }
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
        IDWriteRemoteFontFileLoader *rl = (IDWriteRemoteFontFileLoader *)G2_SENT;
        HRESULT hr = IDWriteFactory5_CreateHttpFontFileLoader(f5, ref[i], hdr[j], &rl);
        CHECKF(hr == S_OK && rl != NULL && rl != G2_SENT, "referrer %zu headers %zu: %s", i, j, dw_hr_name(hr));
        if (SUCCEEDED(hr) && rl != G2_SENT) { EXPECT_REFS(rl, 1); DW_RELEASE(rl); }
    }
    DW_RELEASE(f5); DW_RELEASE(f);
}
TEST(gap2_http_loader_distinct_objects)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory5 *f5 = NULL; IDWriteRemoteFontFileLoader *a = NULL, *b = NULL;
    if (!f) { t_skip("factory"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&f5))) { DW_RELEASE(f); t_skip("Factory5 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, NULL, NULL, &a));
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, NULL, NULL, &b));
    CHECK(a != NULL && b != NULL && a != b);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f5); DW_RELEASE(f);
}
TEST(gap2_http_loader_qi_matrix)
{
    urlfx x; IDWriteFontFileLoader *l = NULL; IDWriteRemoteFontFileLoader *r2 = NULL; IUnknown *u = NULL;
    if (!url_open(&x, 0)) return;
    { HRESULT hq = IDWriteRemoteFontFileLoader_QueryInterface(x.rl, &IID_IDWriteFontFileLoader, (void **)&l);
      CHECK(hq == S_OK || hq == E_NOINTERFACE);
      t_note("QI IDWriteFontFileLoader on remote loader: %s", dw_hr_name(hq));
      if (hq == S_OK) CHECK(l == (IDWriteFontFileLoader *)x.rl); else CHECK(l == NULL); }
    CHECK_HR_OK(IDWriteRemoteFontFileLoader_QueryInterface(x.rl, &IID_IDWriteRemoteFontFileLoader, (void **)&r2));
    CHECK(r2 == x.rl);
    CHECK_HR_OK(IDWriteRemoteFontFileLoader_QueryInterface(x.rl, &IID_IUnknown, (void **)&u));
    CHECK(u != NULL);
    DW_RELEASE(l); DW_RELEASE(r2); DW_RELEASE(u);
    { void *p = G2_SENT; CHECK_HR(IDWriteRemoteFontFileLoader_QueryInterface(x.rl, &IID_IDWriteFactory, &p), E_NOINTERFACE); CHECK(p == NULL); }
    { void *p = G2_SENT; CHECK_HR(IDWriteRemoteFontFileLoader_QueryInterface(x.rl, &IID_IDWriteFontFileStream, &p), E_NOINTERFACE); CHECK(p == NULL); }
    EXPECT_REFS(x.rl, 1);
    url_close(&x);
}
TEST(gap2_http_loader_outlives_factory_release)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory5 *f5 = NULL; IDWriteRemoteFontFileLoader *rl = NULL;
    if (!f) { t_skip("factory"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&f5))) { DW_RELEASE(f); t_skip("Factory5 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, L"https://r.invalid/", NULL, &rl));
    DW_RELEASE(f5); DW_RELEASE(f);
    if (rl) {
        IDWriteFontFileLoader *l = NULL;
        HRESULT hq = IDWriteRemoteFontFileLoader_QueryInterface(rl, &IID_IDWriteFontFileLoader, (void **)&l);
        CHECK(hq == S_OK || hq == E_NOINTERFACE);
        DW_RELEASE(l);
        DW_RELEASE(rl);
    }
}
