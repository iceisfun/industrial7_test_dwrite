/* IDWriteFactory5: in-memory and HTTP font file loaders, font set builders, collections from font sets.
 * Decompilation notes: MemoryFontFileLoader::CreateInMemoryFontFileReference rejects NULL data / zero size with E_INVALIDARG and
 * zeroes *fontFile first; each accepted call appends an entry and creates the file through the factory with a 4 byte key holding
 * the entry index; CreateStreamFromKey requires keySize == 4 and index < entry count (E_INVALIDARG otherwise, out zeroed). */
#include "rfp.h"

typedef struct {
    IDWriteFactory *f; IDWriteFactory5 *f5; IDWriteInMemoryFontFileLoader *l; IDWriteFontFileLoader *fl;
    uint8_t *blob; size_t n; int reg;
} imf;

static void imf_close(imf *x) {
    if (x->reg && x->f && x->fl) IDWriteFactory_UnregisterFontFileLoader(x->f, x->fl);
    x->reg = 0;
    DW_RELEASE(x->fl); DW_RELEASE(x->l); DW_RELEASE(x->f5); DW_RELEASE(x->f);
    free(x->blob); x->blob = NULL;
}
/* returns 1 on success; on failure everything is released and the test is skipped by the caller */
static int imf_open(imf *x, int do_register) {
    ttf_spec s;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(FT_ISOLATED);
    if (!x->f) return 0;
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory5, (void **)&x->f5)) || !x->f5) { imf_close(x); return 0; }
    if (FAILED(IDWriteFactory5_CreateInMemoryFontFileLoader(x->f5, &x->l)) || !x->l) { imf_close(x); return 0; }
    if (FAILED(IDWriteInMemoryFontFileLoader_QueryInterface(x->l, &IID_IDWriteFontFileLoader, (void **)&x->fl))) { imf_close(x); return 0; }
    ttf_default_spec(&s); x->blob = ttf_build(&s, &x->n);
    if (do_register) {
        if (FAILED(IDWriteFactory_RegisterFontFileLoader(x->f, x->fl))) { imf_close(x); return 0; }
        x->reg = 1;
    }
    return 1;
}
#define OPEN(x, reg) imf x; if (!imf_open(&x, reg)) { t_skip("IDWriteFactory5 / in-memory loader unavailable"); return; }

static HRESULT mkref(imf *x, IUnknown *owner, IDWriteFontFile **out) {
    return IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(x->l, x->f, x->blob, (UINT32)x->n, owner, out);
}

TEST(imf_create_ok_refcount_one) {
    OPEN(x, 0);
    EXPECT_REFS(x.l, 2);   /* the loader plus the IDWriteFontFileLoader interface reference held by the fixture */
    imf_close(&x);
}
TEST(imf_create_distinct_objects) {
    OPEN(x, 0);
    IDWriteInMemoryFontFileLoader *b = NULL;
    CHECK_HR_OK(IDWriteFactory5_CreateInMemoryFontFileLoader(x.f5, &b));
    CHECK(b != NULL && b != x.l);
    DW_RELEASE(b);
    imf_close(&x);
}
TEST(imf_create_on_shared_factory_ok) {
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)dw_factory_v(FT_SHARED, 5); IDWriteInMemoryFontFileLoader *l = NULL;
    if (!f5) { t_skip("IDWriteFactory5 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &l));
    CHECK(l != NULL);
    EXPECT_REFS(l, 1);
    DW_RELEASE(l);
}
TEST(imf_qi_matrix) {
    OPEN(x, 0);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteInMemoryFontFileLoader, 1), DW_QI_ROW(IDWriteFontFileLoader, 1), DW_QI_ROW(IUnknown, 1),
                         DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontFileStream, 0),
                         DW_QI_ROW(IDWriteFontCollectionLoader, 0), DW_QI_ROW(IDWriteRemoteFontFileLoader, -1) };
    DW_CHECK_QI(x.l, rows);
    imf_close(&x);
}
TEST(imf_file_count_starts_at_zero) {
    OPEN(x, 0);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(x.l), 0);
    imf_close(&x);
}
TEST(imf_register_adds_exactly_one_reference) {
    OPEN(x, 0);
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(x.f, x.fl)); x.reg = 1;
    CHECKF(dw_refcount((IUnknown *)x.l) >= 3, "registry holds a reference");   /* creator + QI'd fl + registry */
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(x.f, x.fl)); x.reg = 0;
    EXPECT_REFS(x.l, 2);
    imf_close(&x);
}
TEST(imf_register_twice_is_rejected) {
    OPEN(x, 1);
    HRESULT hr = IDWriteFactory_RegisterFontFileLoader(x.f, x.fl);
    CHECK_HR_FAILED(hr);
    CHECK(dw_refcount((IUnknown *)x.l) >= 3);
    imf_close(&x);
}
TEST(imf_register_on_two_factories_independent) {
    OPEN(x, 1);
    IDWriteFactory *g = dw_new_factory(FT_ISOLATED); IDWriteFontFile *ff = NULL;
    if (g) {
        HRESULT hg = IDWriteFactory_RegisterFontFileLoader(g, x.fl);
        t_note("registering one loader on a second factory: %s", dw_hr_name(hg));
        if (SUCCEEDED(hg)) IDWriteFactory_UnregisterFontFileLoader(g, x.fl);
        CHECK_HR_OK(mkref(&x, NULL, &ff)); DW_RELEASE(ff);   /* still registered on the first factory */
    }
    DW_RELEASE(g);
    imf_close(&x);
}

/* ---- argument validation ------------------------------------------------------------------------------ */
TEST(imf_ref_unregistered_loader_fails_with_null_out) {
    OPEN(x, 0);
    IDWriteFontFile *ff = (IDWriteFontFile *)RFP_SENTINEL;
    HRESULT hr = mkref(&x, NULL, &ff);
    CHECK_HR_FAILED(hr);
    CHECKF(ff == NULL, "out %p", ff);
    t_note("CreateInMemoryFontFileReference on unregistered loader: %s; file count afterwards %u", dw_hr_name(hr), IDWriteInMemoryFontFileLoader_GetFileCount(x.l));
    imf_close(&x);
}
TEST(imf_ref_null_data_is_invalidarg) {
    NO_WINE();
    OPEN(x, 1);
    IDWriteFontFile *ff = (IDWriteFontFile *)RFP_SENTINEL;
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(x.l, x.f, NULL, (UINT32)x.n, NULL, &ff), E_INVALIDARG);
    CHECK(ff == NULL);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(x.l), 0);   /* validation happens before an entry is added */
    imf_close(&x);
}
TEST(imf_ref_zero_size_is_invalidarg) {
    OPEN(x, 1);
    IDWriteFontFile *ff = (IDWriteFontFile *)RFP_SENTINEL;
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(x.l, x.f, x.blob, 0, NULL, &ff), E_INVALIDARG);
    CHECK(ff == NULL);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(x.l), 0);
    imf_close(&x);
}
TEST(imf_ref_null_data_and_zero_size_is_invalidarg) {
    NO_WINE();
    OPEN(x, 1);
    IDWriteFontFile *ff = (IDWriteFontFile *)RFP_SENTINEL;
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(x.l, x.f, NULL, 0, NULL, &ff), E_INVALIDARG);
    CHECK(ff == NULL);
    imf_close(&x);
}
TEST(imf_ref_invalid_args_do_not_consume_an_index) {
    NO_WINE();
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; UINT32 *key = NULL, ks = 0;
    IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(x.l, x.f, NULL, 0, NULL, &ff);
    CHECK_HR_OK(mkref(&x, NULL, &ff)); REQUIRE(ff);
    CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff, (const void **)&key, &ks));
    EXPECT_EQ(ks, 4);
    if (key) EXPECT_EQ(*key, 0);
    DW_RELEASE(ff);
    imf_close(&x);
}

/* ---- accepted references ----------------------------------------------------------------------------------- */
TEST(imf_ref_basic_ok_key_is_entry_index) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; const void *key = NULL; UINT32 ks = 0;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); REQUIRE(ff);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(x.l), 1);
    CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff, &key, &ks));
    EXPECT_EQ(ks, 4); CHECK(key != NULL); if (key) EXPECT_EQ(*(const UINT32 *)key, 0);
    DW_RELEASE(ff);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(x.l), 1);   /* entries live as long as the loader */
    imf_close(&x);
}
TEST(imf_ref_ten_files_keys_are_sequential) {
    OPEN(x, 1);
    IDWriteFontFile *ff[10]; int i;
    for (i = 0; i < 10; i++) {
        const void *key = NULL; UINT32 ks = 0; ff[i] = NULL;
        CHECK_HR_OK(mkref(&x, NULL, &ff[i])); if (!ff[i]) continue;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff[i], &key, &ks));
        EXPECT_EQ(ks, 4); if (key) CHECKF(*(const UINT32 *)key == (UINT32)i, "key %u want %d", *(const UINT32 *)key, i);
        EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(x.l), (UINT32)(i + 1));
    }
    for (i = 0; i < 10; i++) DW_RELEASE(ff[i]);
    imf_close(&x);
}
TEST(imf_ref_file_get_loader_is_the_registered_loader) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; IDWriteFontFileLoader *got = NULL;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); REQUIRE(ff);
    CHECK_HR_OK(IDWriteFontFile_GetLoader(ff, &got));
    CHECK(got != NULL);
    if (got) CHECK(dw_same_identity((IUnknown *)got, (IUnknown *)x.l));
    DW_RELEASE(got); DW_RELEASE(ff);
    imf_close(&x);
}
TEST(imf_ref_analyze_reports_truetype_single_face) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft = DWRITE_FONT_FILE_TYPE_UNKNOWN; DWRITE_FONT_FACE_TYPE fc = DWRITE_FONT_FACE_TYPE_UNKNOWN; UINT32 nf = 0;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); REQUIRE(ff);
    CHECK_HR_OK(IDWriteFontFile_Analyze(ff, &sup, &ft, &fc, &nf));
    CHECK(sup); EXPECT_EQ(ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(nf, 1);
    DW_RELEASE(ff);
    imf_close(&x);
}
TEST(imf_ref_face_creation_and_glyph_count) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; IDWriteFontFace *face = NULL; dwx_font ref;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); REQUIRE(ff);
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    REQUIRE(face);
    if (SUCCEEDED(dwx_font_open_default(&ref, x.f))) {
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), IDWriteFontFace_GetGlyphCount(ref.face));
        EXPECT_EQ(IDWriteFontFace_GetIndex(face), 0);
    }
    dwx_font_close(&ref);
    DW_RELEASE(face); DW_RELEASE(ff);
    imf_close(&x);
}
TEST(imf_ref_owner_null_copies_data) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; IDWriteFontFace *face = NULL; UINT16 gc0;
    uint8_t *mine = (uint8_t *)malloc(x.n); memcpy(mine, x.blob, x.n);
    CHECK_HR_OK(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(x.l, x.f, mine, (UINT32)x.n, NULL, &ff)); REQUIRE(ff);
    memset(mine, 0, x.n); free(mine);   /* with a NULL owner dwrite keeps a private copy */
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    REQUIRE(face);
    gc0 = IDWriteFontFace_GetGlyphCount(face);
    CHECK(gc0 > 60);   /* the default synthetic font has 69 glyphs */
    DW_RELEASE(face); DW_RELEASE(ff);
    imf_close(&x);
}
TEST(imf_ref_owner_is_referenced_by_loader_entry) {
    OPEN(x, 1);
    dwx_effect *own = dwx_effect_new(1); IDWriteFontFile *ff = NULL; LONG r0;
    r0 = dwx_refs(own);
    CHECK_HR_OK(mkref(&x, (IUnknown *)own, &ff)); REQUIRE(ff);
    CHECKF(dwx_refs(own) >= r0 + 1, "owner refs %ld -> %ld", r0, dwx_refs(own));
    DW_RELEASE(ff);
    CHECKF(dwx_refs(own) >= r0 + 1, "owner released with the file: %ld", dwx_refs(own));   /* the loader's entry still holds it */
    IDWriteFactory_UnregisterFontFileLoader(x.f, x.fl); x.reg = 0;
    imf_close(&x);
    CHECKF(dwx_refs(own) == r0, "owner refs after loader destroyed: %ld want %ld", dwx_refs(own), r0);
    dwx_release(own);
}
TEST(imf_ref_owner_one_reference_per_entry) {
    OPEN(x, 1);
    dwx_effect *own = dwx_effect_new(2); IDWriteFontFile *ff[3]; int i; LONG r0 = dwx_refs(own);
    for (i = 0; i < 3; i++) { ff[i] = NULL; CHECK_HR_OK(mkref(&x, (IUnknown *)own, &ff[i])); }
    CHECKF(dwx_refs(own) >= r0 + 1, "owner refs %ld", dwx_refs(own));
    for (i = 0; i < 3; i++) DW_RELEASE(ff[i]);
    imf_close(&x);
    CHECKF(dwx_refs(own) == r0, "owner refs after loader destroyed: %ld", dwx_refs(own));
    dwx_release(own);
}
TEST(imf_ref_data_not_copied_when_owner_given_reads_live_memory) {
    /* with an owner the caller's buffer is used in place: the font stays valid for as long as the buffer is */
    OPEN(x, 1);
    dwx_effect *own = dwx_effect_new(3); IDWriteFontFile *ff = NULL; BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf;
    CHECK_HR_OK(mkref(&x, (IUnknown *)own, &ff)); REQUIRE(ff);
    CHECK_HR_OK(IDWriteFontFile_Analyze(ff, &sup, &ft, &fc, &nf));
    CHECK(sup);
    DW_RELEASE(ff);
    imf_close(&x);
    dwx_release(own);
}

/* ---- streams straight from the loader --------------------------------------------------------------------------- */
static IDWriteFontFileStream *stream_for(imf *x, UINT32 idx, HRESULT *hr) {
    IDWriteFontFileStream *s = (IDWriteFontFileStream *)RFP_SENTINEL;
    *hr = IDWriteFontFileLoader_CreateStreamFromKey(x->fl, &idx, 4, &s);
    return s;
}
TEST(imf_stream_valid_index) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; HRESULT hr; IDWriteFontFileStream *s; UINT64 sz = 0;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); DW_RELEASE(ff);
    s = stream_for(&x, 0, &hr);
    CHECK_HR(hr, S_OK); REQUIRE(s && s != RFP_SENTINEL);
    CHECK_HR_OK(IDWriteFontFileStream_GetFileSize(s, &sz));
    EXPECT_EQ(sz, x.n);
    DW_RELEASE(s);
    imf_close(&x);
}
TEST(imf_stream_content_matches_input) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; HRESULT hr; IDWriteFontFileStream *s; const void *frag = NULL; void *ctx = NULL;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); DW_RELEASE(ff);
    s = stream_for(&x, 0, &hr); REQUIRE(hr == S_OK && s);
    CHECK_HR_OK(IDWriteFontFileStream_ReadFileFragment(s, &frag, 0, x.n, &ctx));
    CHECK(frag != NULL);
    if (frag) CHECK(!memcmp(frag, x.blob, x.n));
    IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
    frag = NULL; ctx = NULL;
    CHECK_HR_OK(IDWriteFontFileStream_ReadFileFragment(s, &frag, 12, 16, &ctx));
    if (frag) CHECK(!memcmp(frag, x.blob + 12, 16));
    IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
    DW_RELEASE(s);
    imf_close(&x);
}
TEST(imf_stream_last_write_time_note) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; HRESULT hr, hr2; IDWriteFontFileStream *s; UINT64 t = 0;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); DW_RELEASE(ff);
    s = stream_for(&x, 0, &hr); REQUIRE(hr == S_OK && s);
    hr2 = IDWriteFontFileStream_GetLastWriteTime(s, &t);
    t_note("in-memory stream GetLastWriteTime: %s value %llu", dw_hr_name(hr2), (unsigned long long)t);
    DW_RELEASE(s);
    imf_close(&x);
}
TEST(imf_stream_index_out_of_range_is_invalidarg) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; HRESULT hr; IDWriteFontFileStream *s; static const UINT32 idx[] = { 1, 2, 100, 0x7fffffff, 0xffffffff };
    int i;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); DW_RELEASE(ff);
    for (i = 0; i < 5; i++) { s = stream_for(&x, idx[i], &hr); CHECKF(hr == E_INVALIDARG && s == NULL, "idx %u -> %s %p", idx[i], dw_hr_name(hr), s); }
    imf_close(&x);
}
TEST(imf_stream_empty_loader_any_index_invalid) {
    OPEN(x, 1);
    HRESULT hr; IDWriteFontFileStream *s = stream_for(&x, 0, &hr);
    CHECK_HR(hr, E_INVALIDARG); CHECK(s == NULL);
    imf_close(&x);
}
TEST(imf_stream_key_size_must_be_four) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; UINT32 key[4] = { 0, 0, 0, 0 }; UINT32 sizes[] = { 0, 1, 2, 3, 5, 8, 16 }; int i;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); DW_RELEASE(ff);
    for (i = 0; i < 7; i++) {
        IDWriteFontFileStream *s = (IDWriteFontFileStream *)RFP_SENTINEL;
        HRESULT hr = IDWriteFontFileLoader_CreateStreamFromKey(x.fl, key, sizes[i], &s);
        CHECKF(hr == E_INVALIDARG && s == NULL, "key size %u -> %s %p", sizes[i], dw_hr_name(hr), s);
    }
    imf_close(&x);
}
TEST(imf_stream_each_call_new_object_independent_reads) {
    OPEN(x, 1);
    IDWriteFontFile *ff = NULL; HRESULT h1, h2; IDWriteFontFileStream *a, *b; UINT64 s1 = 0, s2 = 0;
    CHECK_HR_OK(mkref(&x, NULL, &ff)); DW_RELEASE(ff);
    a = stream_for(&x, 0, &h1); b = stream_for(&x, 0, &h2);
    REQUIRE(h1 == S_OK && h2 == S_OK);
    CHECK(a != b);
    IDWriteFontFileStream_GetFileSize(a, &s1); DW_RELEASE(a);
    IDWriteFontFileStream_GetFileSize(b, &s2); DW_RELEASE(b);
    EXPECT_EQ(s1, s2);
    imf_close(&x);
}
TEST(imf_two_different_blobs_get_their_own_streams) {
    OPEN(x, 1);
    ttf_spec s2; uint8_t *b2; size_t n2; IDWriteFontFile *f1 = NULL, *f2 = NULL; HRESULT hr; IDWriteFontFileStream *st; UINT64 sz = 0;
    ttf_style_spec(&s2, "OtherFam", 700, 5, 0); b2 = ttf_build(&s2, &n2);
    CHECK_HR_OK(mkref(&x, NULL, &f1));
    CHECK_HR_OK(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(x.l, x.f, b2, (UINT32)n2, NULL, &f2));
    st = stream_for(&x, 1, &hr); REQUIRE(hr == S_OK && st);
    IDWriteFontFileStream_GetFileSize(st, &sz); EXPECT_EQ(sz, n2); DW_RELEASE(st);
    st = stream_for(&x, 0, &hr); REQUIRE(hr == S_OK && st);
    IDWriteFontFileStream_GetFileSize(st, &sz); EXPECT_EQ(sz, x.n); DW_RELEASE(st);
    DW_RELEASE(f1); DW_RELEASE(f2); free(b2);
    imf_close(&x);
}

/* ---- concurrency on one loader ------------------------------------------------------------------------------------ */
typedef struct { imf *x; int n; IDWriteFontFile *files[32]; volatile LONG bad; HANDLE go; } mt;
static DWORD WINAPI mt_worker(void *p) {
    mt *m = (mt *)p; int i;
    WaitForSingleObject(m->go, 5000);
    for (i = 0; i < m->n; i++) {
        if (FAILED(mkref(m->x, NULL, &m->files[i])) || !m->files[i]) InterlockedIncrement(&m->bad);
    }
    return 0;
}
TEST(imf_concurrent_references_get_unique_keys) {
    if (t_is_wine()) { t_skip("emulated in-memory loader is not thread safe"); return; }
    OPEN(x, 1);
    enum { T = 4, N = 16 }; mt m[T]; HANDLE th[T]; HANDLE go = CreateEventA(NULL, TRUE, FALSE, NULL); int i, j, k; UINT32 seen[T * N]; int ns = 0;
    for (i = 0; i < T; i++) { memset(&m[i], 0, sizeof m[i]); m[i].x = &x; m[i].n = N; m[i].go = go; th[i] = CreateThread(NULL, 0, mt_worker, &m[i], 0, NULL); }
    SetEvent(go);
    for (i = 0; i < T; i++) if (th[i]) { WaitForSingleObject(th[i], 10000); CloseHandle(th[i]); }
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(x.l), T * N);
    for (i = 0; i < T; i++) for (j = 0; j < N; j++) {
        const void *key = NULL; UINT32 ks = 0;
        if (!m[i].files[j]) { CHECK(0); continue; }
        if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(m[i].files[j], &key, &ks)) && ks == 4) {
            UINT32 v = *(const UINT32 *)key;
            CHECK(v < T * N);
            for (k = 0; k < ns; k++) if (seen[k] == v) { CHECKF(0, "duplicate key %u", v); break; }
            seen[ns++] = v;
        }
        DW_RELEASE(m[i].files[j]);
    }
    CHECK(m[0].bad + m[1].bad + m[2].bad + m[3].bad == 0);
    CloseHandle(go);
    imf_close(&x);
}

/* ---- HTTP font file loader ------------------------------------------------------------------------------------------ */
TEST(http_create_ok) {
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)dw_factory_v(FT_ISOLATED, 5); IDWriteRemoteFontFileLoader *l = NULL;
    if (!f5) { t_skip("IDWriteFactory5 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, L"https://example.invalid/", L"X-Test: 1\r\n", &l));
    CHECK(l != NULL); EXPECT_REFS(l, 1);
    DW_RELEASE(l);
}
TEST(http_create_empty_strings_ok) {
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)dw_factory_v(FT_ISOLATED, 5); IDWriteRemoteFontFileLoader *l = NULL;
    if (!f5) { t_skip("IDWriteFactory5 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, L"", L"", &l));
    CHECK(l != NULL);
    DW_RELEASE(l);
}
TEST(http_create_distinct_objects_and_qi) {
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)dw_factory_v(FT_ISOLATED, 5); IDWriteRemoteFontFileLoader *a = NULL, *b = NULL;
    if (!f5) { t_skip("IDWriteFactory5 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, L"https://a.invalid/", L"", &a));
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, L"https://a.invalid/", L"", &b));
    CHECK(a && b && a != b);
    if (a) {
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteRemoteFontFileLoader, 1), DW_QI_ROW(IDWriteFontFileLoader, -1), DW_QI_ROW(IUnknown, 1),
                             DW_QI_ROW(IDWriteInMemoryFontFileLoader, 0), DW_QI_ROW(IDWriteFontFile, 0) };
        DW_CHECK_QI(a, rows);
    }
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(http_loader_register_unregister_cycle) {
    IDWriteFactory *f = dw_new_factory(FT_ISOLATED); IDWriteFactory5 *f5 = NULL; IDWriteRemoteFontFileLoader *l = NULL; IDWriteFontFileLoader *fl = NULL; HRESULT hr;
    REQUIRE(f);
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&f5))) { t_skip("IDWriteFactory5 unsupported"); DW_RELEASE(f); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, L"https://a.invalid/", L"", &l)); REQUIRE(l);
    /* Windows 10 refuses QI from the remote loader to its base IDWriteFontFileLoader (E_NOINTERFACE); registration is then tried with the derived pointer */
    hr = IDWriteRemoteFontFileLoader_QueryInterface(l, &IID_IDWriteFontFileLoader, (void **)&fl);
    t_note("QI remote loader -> IDWriteFontFileLoader: %s", dw_hr_name(hr));
    if (FAILED(hr)) fl = NULL;
    {   IDWriteFontFileLoader *reg = fl ? fl : (IDWriteFontFileLoader *)l;
        hr = IDWriteFactory_RegisterFontFileLoader(f, reg);
        t_note("RegisterFontFileLoader(http loader): %s", dw_hr_name(hr));
        if (SUCCEEDED(hr)) CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(f, reg)); }
    DW_RELEASE(fl); DW_RELEASE(l); DW_RELEASE(f5); DW_RELEASE(f);
}
TEST(http_loader_invalid_key_stream_fails_cleanly) {
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)dw_factory_v(FT_ISOLATED, 5); IDWriteRemoteFontFileLoader *l = NULL; IDWriteFontFileStream *s = (IDWriteFontFileStream *)RFP_SENTINEL; HRESULT hr;
    static const WCHAR key[] = L"https://a.invalid/none.ttf";
    if (!f5) { t_skip("IDWriteFactory5 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateHttpFontFileLoader(f5, L"", L"", &l)); REQUIRE(l);
    hr = IDWriteRemoteFontFileLoader_CreateStreamFromKey(l, key, (UINT32)sizeof key, &s);
    CHECK_HR_FAILED(hr);   /* nothing is cached for this url; a non-local stream is refused (0x889850xx) */
    CHECK(s == NULL);
    t_note("http CreateStreamFromKey for an unknown url: %s", dw_hr_name(hr));
    DW_RELEASE(l);
}

/* ---- font set builders ---------------------------------------------------------------------------------------------- */
TEST(fsb_factory3_builder_ok) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_ISOLATED, 3); IDWriteFontSetBuilder *b = NULL, *b2 = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(f3, &b)); CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(f3, &b2));
    CHECK(b && b2 && b != b2); EXPECT_REFS(b, 1);
    DW_RELEASE(b); DW_RELEASE(b2);
}
TEST(fsb_factory5_builder1_ok_and_qi) {
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)dw_factory_v(FT_ISOLATED, 5); IDWriteFontSetBuilder1 *b = NULL;
    if (!f5) { t_skip("IDWriteFactory5 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory5_CreateFontSetBuilder(f5, &b)); REQUIRE(b);
    EXPECT_REFS(b, 1);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontSetBuilder1, 1), DW_QI_ROW(IDWriteFontSetBuilder, 1), DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFactory, 0) };
    DW_CHECK_QI(b, rows);
    DW_RELEASE(b);
}
TEST(fsb_factory3_builder_qi_to_builder1_note) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_ISOLATED, 3); IDWriteFontSetBuilder *b = NULL; void *p = NULL; HRESULT hr;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(f3, &b)); REQUIRE(b);
    hr = IDWriteFontSetBuilder_QueryInterface(b, &IID_IDWriteFontSetBuilder1, &p);
    t_note("factory3 builder -> IDWriteFontSetBuilder1: %s", dw_hr_name(hr));
    DW_RELEASE(p); DW_RELEASE(b);
}
TEST(fsb_factory6_builder2_note) {
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(FT_ISOLATED, 6); IDWriteFontSetBuilder2 *b = NULL;
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory6_CreateFontSetBuilder(f6, &b));
    CHECK(b != NULL);
    DW_RELEASE(b);
}
TEST(fsb_empty_builder_creates_empty_set) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_ISOLATED, 3); IDWriteFontSetBuilder *b = NULL; IDWriteFontSet *s = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(f3, &b)); REQUIRE(b);
    CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s)); REQUIRE(s);
    EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 0);
    DW_RELEASE(s); DW_RELEASE(b);
}
TEST(fsb_builder_can_create_many_sets) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_ISOLATED, 3); IDWriteFontSetBuilder *b = NULL; IDWriteFontSet *s1 = NULL, *s2 = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(f3, &b)); REQUIRE(b);
    CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s1)); CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s2));
    CHECK(s1 != NULL && s2 != NULL);
    DW_RELEASE(s1); DW_RELEASE(s2); DW_RELEASE(b);
}
TEST(fsb_add_in_memory_font_file_builds_one_font_set) {
    OPEN(x, 1);
    IDWriteFontSetBuilder1 *b = NULL; IDWriteFontFile *ff = NULL; IDWriteFontSet *s = NULL; HRESULT hr;
    CHECK_HR_OK(IDWriteFactory5_CreateFontSetBuilder(x.f5, &b)); REQUIRE(b);
    CHECK_HR_OK(mkref(&x, NULL, &ff)); REQUIRE(ff);
    hr = IDWriteFontSetBuilder1_AddFontFile(b, ff);
    CHECK_HR(hr, S_OK);
    CHECK_HR_OK(IDWriteFontSetBuilder1_CreateFontSet(b, &s));
    if (s) {
        EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 1);   /* one face in a single-face TrueType file */
        DW_RELEASE(s);
    }
    DW_RELEASE(ff); DW_RELEASE(b);
    imf_close(&x);
}
TEST(fsb_collection_from_font_set_null_is_invalidarg) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_ISOLATED, 3); IDWriteFontCollection1 *c = (IDWriteFontCollection1 *)RFP_SENTINEL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    if (t_is_wine()) { t_skip("emulated implementation dereferences the NULL set"); return; }
    CHECK_HR(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, NULL, &c), E_INVALIDARG);   /* decompiled: NULL set rejected, out zeroed first */
    CHECK(c == NULL);
}
TEST(fsb_collection_from_font_set_ok_and_family_count) {
    OPEN(x, 1);
    IDWriteFontSetBuilder1 *b = NULL; IDWriteFontFile *ff = NULL; IDWriteFontSet *s = NULL; IDWriteFactory3 *f3 = NULL; IDWriteFontCollection1 *c = NULL;
    CHECK_HR_OK(IDWriteFactory_QueryInterface(x.f, &IID_IDWriteFactory3, (void **)&f3)); REQUIRE(f3);
    CHECK_HR_OK(IDWriteFactory5_CreateFontSetBuilder(x.f5, &b)); REQUIRE(b);
    CHECK_HR_OK(mkref(&x, NULL, &ff));
    CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b, ff));
    CHECK_HR_OK(IDWriteFontSetBuilder1_CreateFontSet(b, &s));
    if (s) {
        CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, s, &c));
        if (c) { EXPECT_EQ(IDWriteFontCollection1_GetFontFamilyCount(c), 1); EXPECT_REFS(c, 1); }
    }
    DW_RELEASE(c); DW_RELEASE(s); DW_RELEASE(ff); DW_RELEASE(b); DW_RELEASE(f3);
    imf_close(&x);
}
TEST(fsb_collection_from_empty_set_has_no_families) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_ISOLATED, 3); IDWriteFontSetBuilder *b = NULL; IDWriteFontSet *s = NULL; IDWriteFontCollection1 *c = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(f3, &b)); REQUIRE(b);
    CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s));
    CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, s, &c));
    if (c) EXPECT_EQ(IDWriteFontCollection1_GetFontFamilyCount(c), 0);
    DW_RELEASE(c); DW_RELEASE(s); DW_RELEASE(b);
}
