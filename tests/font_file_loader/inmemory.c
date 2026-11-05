/* IDWriteFactory5 loaders: IDWriteInMemoryFontFileLoader (MemoryFontFileLoader) and the HTTP / IDWriteRemoteFontFileLoader (NetFontLoader).
 * Basis: IDWriteFactory::CreateInMemoryFontFileLoader (object is NOT registered), CreateInMemoryFontFileReference
 * (NULL/0 -> E_INVALIDARG before anything else; entry pushed before the factory is asked; key = 4 byte entry index; copies the data
 * unless an owner object is supplied), CreateStreamFromKey (key size 4, index < count), the memory stream
 * (out-of-range read -> E_INVALIDARG, GetLastWriteTime -> E_NOTIMPL), CreateFontFileReferenceFromUrl. */
#include "fl.h"

#define NEED_F5() IDWriteFactory *f = fl_factory(); IDWriteFactory5 *f5; IDWriteInMemoryFontFileLoader *m = NULL; \
    if (!f) { t_skip("DWriteCreateFactory failed"); return; } \
    f5 = fl_factory5(f); if (!f5) { IDWriteFactory_Release(f); t_skip("IDWriteFactory5 not available"); return; } \
    CHECK_HR(IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &m), S_OK); \
    if (!m) { IDWriteFactory5_Release(f5); IDWriteFactory_Release(f); return; }
#define MLDR(m) ((IDWriteFontFileLoader *)(m))
#define REG_M() CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, MLDR(m)), S_OK)
#define END_F5() do { IDWriteFactory_UnregisterFontFileLoader(f, MLDR(m)); IDWriteInMemoryFontFileLoader_Release(m); IDWriteFactory5_Release(f5); IDWriteFactory_Release(f); } while (0)
#define END_F5_UNREG(reg) do { if (reg) IDWriteFactory_UnregisterFontFileLoader(f, MLDR(m)); IDWriteInMemoryFontFileLoader_Release(m); IDWriteFactory5_Release(f5); IDWriteFactory_Release(f); } while (0)

static HRESULT mkmem(IDWriteInMemoryFontFileLoader *m, IDWriteFactory *f, const void *d, UINT32 n, IUnknown *owner, IDWriteFontFile **out) {
    return IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m, f, d, n, owner, out);
}

/* ---------------- the loader object ---------------- */
TEST(mem_create_loader) {
    NEED_F5();
    EXPECT_REFS(m, 1);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), 0);
    END_F5_UNREG(0);
}
TEST(mem_create_loader_distinct_objects) {
    NEED_F5(); IDWriteInMemoryFontFileLoader *m2 = NULL;
    CHECK_HR(IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &m2), S_OK);
    CHECK(m2 != NULL && m2 != m);
    if (m2) { EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m2), 0); IDWriteInMemoryFontFileLoader_Release(m2); }
    END_F5_UNREG(0);
}
TEST(mem_create_loader_out_reset) {
    NEED_F5(); IDWriteInMemoryFontFileLoader *m2 = (IDWriteInMemoryFontFileLoader *)(ULONG_PTR)0x1234;
    CHECK_HR(IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &m2), S_OK);
    CHECK(m2 != (IDWriteInMemoryFontFileLoader *)(ULONG_PTR)0x1234 && m2 != NULL);
    if (m2) IDWriteInMemoryFontFileLoader_Release(m2);
    END_F5_UNREG(0);
}
TEST(mem_loader_qi_matrix) {
    NEED_F5();
    dw_qi_row rows[] = {
        DW_QI_ROW(IDWriteInMemoryFontFileLoader, 1), DW_QI_ROW(IDWriteFontFileLoader, 1),
        DW_QI_ROW(IDWriteLocalFontFileLoader, 0), DW_QI_ROW(IDWriteRemoteFontFileLoader, 0), DW_QI_ROW(IDWriteFontFileStream, 0),
        DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFactory5, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontCollectionLoader, 0),
        DW_QI_ROW(IDWriteFontFileEnumerator, 0), DW_QI_ROW(IDWriteRemoteFontFileStream, 0),
    };
    DW_CHECK_QI(m, rows);
    END_F5_UNREG(0);
}
TEST(mem_loader_refcounts) {
    NEED_F5();
    ULONG a = IDWriteInMemoryFontFileLoader_AddRef(m), b = IDWriteInMemoryFontFileLoader_AddRef(m), c = IDWriteInMemoryFontFileLoader_Release(m), d = IDWriteInMemoryFontFileLoader_Release(m);
    EXPECT_EQ(a, 2); EXPECT_EQ(b, 3); EXPECT_EQ(c, 2); EXPECT_EQ(d, 1);
    END_F5_UNREG(0);
}
TEST(mem_loader_not_auto_registered) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = (IDWriteFontFile *)1;
    HRESULT hr = mkmem(m, f, b, (UINT32)n, NULL, &file);
    CHECK_HR(hr, E_INVALIDARG);                         /* CreateCustomFontFileReference rejects a loader the factory does not know */
    CHECK_OUT_NULL(file);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), 1);   /* the entry is recorded before the factory is asked */
    free(b); END_F5_UNREG(0);
}
TEST(mem_register_twice) {
    NEED_F5();
    REG_M();
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, MLDR(m)), H_ALREADYREG);
    EXPECT_REFS(m, 2);
    END_F5();
}
TEST(mem_register_refcount) {
    NEED_F5();
    REG_M(); EXPECT_REFS(m, 2);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, MLDR(m)), S_OK); EXPECT_REFS(m, 1);
    END_F5_UNREG(0);
}

/* ---------------- CreateInMemoryFontFileReference ---------------- */
TEST(mem_reference_ok) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL;
    REG_M();
    CHECK_HR(mkmem(m, f, b, (UINT32)n, NULL, &file), S_OK);
    CHECK(file != NULL);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), 1);
    if (file) {
        an_res a = fl_analyze(file);
        CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(a.nf, 1);
        EXPECT_REFS(file, 1);
        DW_RELEASE(file);
    }
    free(b); END_F5();
}
TEST(mem_reference_key_is_entry_index) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *files[5] = { 0 }; int i;
    REG_M();
    for (i = 0; i < 5; i++) {
        CHECK_HR(mkmem(m, f, b, (UINT32)n, NULL, &files[i]), S_OK);
        EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), i + 1);
        if (!files[i]) continue;
        const void *k; UINT32 ks; CHECK_HR(IDWriteFontFile_GetReferenceKey(files[i], &k, &ks), S_OK);
        EXPECT_EQ(ks, 4); if (ks == 4) EXPECT_EQ(*(const UINT32 *)k, (UINT32)i);
    }
    for (i = 0; i < 5; i++) DW_RELEASE(files[i]);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), 5);       /* entries live as long as the loader, not the file objects */
    free(b); END_F5();
}
TEST(mem_reference_same_data_twice_two_entries) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *a = NULL, *c = NULL;
    REG_M();
    mkmem(m, f, b, (UINT32)n, NULL, &a); mkmem(m, f, b, (UINT32)n, NULL, &c);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), 2); CHECK(a != c);
    DW_RELEASE(a); DW_RELEASE(c); free(b); END_F5();
}
TEST(mem_reference_file_loader_identity) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; IDWriteFontFileLoader *g = NULL;
    REG_M();
    mkmem(m, f, b, (UINT32)n, NULL, &file);
    if (file) {
        CHECK_HR(IDWriteFontFile_GetLoader(file, &g), S_OK);
        CHECK(g == MLDR(m));
        EXPECT_REFS(m, 4);                                /* app + registry + file + g */
        DW_RELEASE(g); DW_RELEASE(file);
        EXPECT_REFS(m, 2);
    }
    free(b); END_F5();
}
TEST(mem_reference_invalid_arguments) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file;
    REG_M();
    if (t_is_wine()) { t_skip("other implementations dereference the NULL"); free(b); END_F5(); return; }
    file = (IDWriteFontFile *)1; CHECK_HR(mkmem(m, f, NULL, (UINT32)n, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    file = (IDWriteFontFile *)1; CHECK_HR(mkmem(m, f, b, 0, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    file = (IDWriteFontFile *)1; CHECK_HR(mkmem(m, f, NULL, 0, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), 0);    /* rejected before an entry is recorded */
    free(b); END_F5();
}
TEST(mem_reference_invalid_arguments_unregistered) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = (IDWriteFontFile *)1;
    if (t_is_wine()) { t_skip("other implementations dereference the NULL"); free(b); END_F5_UNREG(0); return; }
    CHECK_HR(mkmem(m, f, NULL, 10, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), 0);
    free(b); END_F5_UNREG(0);
}
TEST(mem_reference_after_unregister) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = (IDWriteFontFile *)1;
    REG_M(); CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, MLDR(m)), S_OK);
    CHECK_HR(mkmem(m, f, b, (UINT32)n, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    free(b); END_F5_UNREG(0);
}
TEST(mem_reference_registered_in_other_factory) {
    NEED_F5(); IDWriteFactory *g = fl_factory(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = (IDWriteFontFile *)1;
    REG_M();
    if (g) {
        /* the factory argument is the one asked to create the reference */
        HRESULT hr = mkmem(m, g, b, (UINT32)n, NULL, &file);
        t_note("CreateInMemoryFontFileReference with a factory that does not have the loader registered: %s", dw_hr_name(hr));
        CHECK_HR(hr, E_INVALIDARG);
        if (FAILED(hr)) CHECK_OUT_NULL(file); else DW_RELEASE(file);
        IDWriteFactory_Release(g);
    }
    free(b); END_F5();
}
TEST(mem_reference_copies_data_without_owner) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL;
    REG_M();
    mkmem(m, f, b, (UINT32)n, NULL, &file);
    memset(b, 0, n);                                  /* the loader took a private copy */
    if (file) { an_res a = fl_analyze(file); CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); }
    DW_RELEASE(file); free(b); END_F5();
}
TEST(mem_reference_aliases_data_with_owner) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; dwx_effect *owner = dwx_effect_new(1);
    REG_M();
    CHECK_HR(mkmem(m, f, b, (UINT32)n, (IUnknown *)owner, &file), S_OK);
    if (file) {
        an_res a = fl_analyze(file); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE);
        b[0] = 'O'; b[1] = 'T'; b[2] = 'T'; b[3] = 'O';  /* with an owner the memory is used in place: caller-side edits are visible */
        a = fl_analyze(file); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_CFF);
    }
    DW_RELEASE(file);
    END_F5(); dwx_release(owner); free(b);
}
TEST(mem_owner_reference_counting) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *f1 = NULL, *f2 = NULL; dwx_effect *owner = dwx_effect_new(2);
    REG_M();
    EXPECT_EQ(dwx_refs(owner), 1);
    CHECK_HR(mkmem(m, f, b, (UINT32)n, (IUnknown *)owner, &f1), S_OK);
    EXPECT_EQ(dwx_refs(owner), 2);                    /* the loader's entry holds one reference */
    CHECK_HR(mkmem(m, f, b, (UINT32)n, (IUnknown *)owner, &f2), S_OK);
    EXPECT_EQ(dwx_refs(owner), 3);
    fl_analyze(f1);
    DW_RELEASE(f1); DW_RELEASE(f2);
    EXPECT_EQ(dwx_refs(owner), 3);                    /* released files do not release entries */
    dwx_release(owner);
    CHECK(!dwx_destroyed(owner));
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, MLDR(m)), S_OK);
    IDWriteInMemoryFontFileLoader_Release(m);         /* last reference: entries are destroyed, owners released */
    CHECK(dwx_destroyed(owner));
    IDWriteFactory5_Release(f5); IDWriteFactory_Release(f); free(b);
}
TEST(mem_owner_retained_even_when_registration_missing) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = (IDWriteFontFile *)1; dwx_effect *owner = dwx_effect_new(3);
    HRESULT hr = mkmem(m, f, b, (UINT32)n, (IUnknown *)owner, &file);
    CHECK_HR(hr, E_INVALIDARG);
    EXPECT_EQ(dwx_refs(owner), 2);                    /* the failed attempt left its entry (and the owner reference) in the loader */
    IDWriteInMemoryFontFileLoader_Release(m);
    CHECK(dwx_refs(owner) == 1);
    IDWriteFactory5_Release(f5); IDWriteFactory_Release(f); dwx_release(owner); free(b);
}
TEST(mem_owner_not_touched_by_queries) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; dwx_effect *owner = dwx_effect_new(4);
    REG_M();
    mkmem(m, f, b, (UINT32)n, (IUnknown *)owner, &file);
    fl_analyze(file);
    EXPECT_EQ(dwx_qi_calls(owner), 0);                /* the owner is only kept alive, never queried */
    DW_RELEASE(file);
    END_F5(); dwx_release(owner); free(b);
}
TEST(mem_unaligned_source_with_and_without_owner) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); uint8_t *odd = (uint8_t *)malloc(n + 8); IDWriteFontFile *a = NULL, *c = NULL; dwx_effect *owner = dwx_effect_new(5);
    memcpy(odd + 3, b, n);
    REG_M();
    CHECK_HR(mkmem(m, f, odd + 3, (UINT32)n, NULL, &a), S_OK);
    CHECK_HR(mkmem(m, f, odd + 3, (UINT32)n, (IUnknown *)owner, &c), S_OK);
    if (a) { an_res r = fl_analyze(a); CHECK(r.sup); }
    if (c) { an_res r = fl_analyze(c); CHECK(r.sup); }
    DW_RELEASE(a); DW_RELEASE(c);
    END_F5(); dwx_release(owner); free(b); free(odd);
}
TEST(mem_many_files_in_one_loader) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); enum { N = 150 }; IDWriteFontFile *files[N]; int i;
    REG_M();
    for (i = 0; i < N; i++) { files[i] = NULL; CHECK_HR(mkmem(m, f, b, 0x80, NULL, &files[i]), S_OK); }
    EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), N);
    for (i = 0; i < N; i += 37) { an_res a = fl_analyze(files[i]); CHECK_HR(a.hr, S_OK); CHECK(a.sup); }
    {
        const void *k; UINT32 ks; IDWriteFontFile_GetReferenceKey(files[N - 1], &k, &ks); EXPECT_EQ(*(const UINT32 *)k, N - 1);
    }
    for (i = 0; i < N; i++) DW_RELEASE(files[i]);
    free(b); END_F5();
}
TEST(mem_different_blobs_per_entry) {
    NEED_F5(); IDWriteFontFile *fa = NULL, *fb = NULL, *fc = NULL, *fd = NULL; size_t n1, n2, n3; uint8_t *b1 = fx_ttf(&n1), *b2 = fx_ttc_hdr(4, 0x90, &n2), *b3 = fx_magic(0x77777777, 0x90, &n3);
    REG_M();
    mkmem(m, f, b1, (UINT32)n1, NULL, &fa); mkmem(m, f, b2, (UINT32)n2, NULL, &fb); mkmem(m, f, b3, (UINT32)n3, NULL, &fc);
    mkmem(m, f, b1, 12, NULL, &fd);
    an_res a1 = fl_analyze(fa), a2 = fl_analyze(fb), a3 = fl_analyze(fc);
    EXPECT_EQ(a1.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a2.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION); EXPECT_EQ(a2.nf, 4); CHECK(!a3.sup); EXPECT_EQ(a3.hr, S_OK);
    an_res a4 = fl_analyze(fd);                          /* 12 bytes: shorter than the 0x75 byte probe */
    CHECK(a4.hr == S_OK || a4.hr == H_FILEFORMAT); CHECK(!a4.sup);
    DW_RELEASE(fa); DW_RELEASE(fb); DW_RELEASE(fc); DW_RELEASE(fd);
    free(b1); free(b2); free(b3); END_F5();
}
TEST(mem_stream_traffic_of_analyze_is_internal) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    if (file) {
        int i; for (i = 0; i < 3; i++) { an_res a = fl_analyze(file); CHECK_HR(a.hr, S_OK); }
        EXPECT_EQ(IDWriteInMemoryFontFileLoader_GetFileCount(m), 1);
    }
    DW_RELEASE(file); free(b); END_F5();
}

/* ---------------- the loader's own streams ---------------- */
static IDWriteFontFileStream *mem_stream(IDWriteInMemoryFontFileLoader *m, UINT32 idx, HRESULT *hr) {
    IDWriteFontFileStream *s = (IDWriteFontFileStream *)1;
    *hr = IDWriteInMemoryFontFileLoader_CreateStreamFromKey(m, &idx, 4, &s);
    return s;
}
TEST(mem_stream_basic) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; HRESULT hr;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    IDWriteFontFileStream *s = mem_stream(m, 0, &hr);
    CHECK_HR(hr, S_OK); CHECK(s != NULL);
    if (s) {
        UINT64 sz = 9; CHECK_HR(IDWriteFontFileStream_GetFileSize(s, &sz), S_OK); EXPECT_EQ(sz, n);
        UINT64 t = 0x1111; HRESULT h2 = IDWriteFontFileStream_GetLastWriteTime(s, &t);
        CHECK_HR(h2, H_E_NOTIMPL); EXPECT_EQ(t, 0);       /* in-memory data has no write time */
        const void *st = (const void *)1; void *ctx = (void *)1;
        CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, 0, n, &ctx), S_OK);
        CHECK(st != NULL && !memcmp(st, b, n)); CHECK(ctx == NULL);
        IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
        IDWriteFontFileStream_ReleaseFileFragment(s, NULL);
        IDWriteFontFileStream_Release(s);
    }
    DW_RELEASE(file); free(b); END_F5();
}
TEST(mem_stream_read_bounds) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; HRESULT hr;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    IDWriteFontFileStream *s = mem_stream(m, 0, &hr);
    if (s) {
        static const struct { UINT64 off, len; int ok; } t[] = {
            { 0, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }, { 0, 0, 1 }, { 100, 50, 1 },
        };
        size_t i; const void *st; void *ctx;
        for (i = 0; i < sizeof t / sizeof t[0]; i++) {
            st = (const void *)1; ctx = (void *)1;
            hr = IDWriteFontFileStream_ReadFileFragment(s, &st, t[i].off, t[i].len, &ctx);
            CHECKF(hr == S_OK && st != NULL && ctx == NULL, "read(%llu,%llu): %s", (unsigned long long)t[i].off, (unsigned long long)t[i].len, dw_hr_name(hr));
            if (st) CHECK(st == (const uint8_t *)st && !memcmp(st, b + t[i].off, (size_t)t[i].len));
        }
        {   /* ranges that touch the end */
            st = (const void *)1; ctx = (void *)1;
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, n, 0, &ctx), S_OK); CHECK(st != NULL); CHECK(ctx == NULL);
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, n - 1, 1, &ctx), S_OK);
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, 0, n, &ctx), S_OK);
        }
        {   /* ranges beyond it fail with E_INVALIDARG and clear both outputs */
            static const struct { UINT64 off, len; } bad[] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
            UINT64 o[6], l[6]; int k;
            o[0] = n; l[0] = 1; o[1] = n - 1; l[1] = 2; o[2] = 0; l[2] = n + 1; o[3] = n + 1; l[3] = 0; o[4] = 0xFFFFFFFFFFFFFFFFull; l[4] = 1; o[5] = 1; l[5] = 0xFFFFFFFFFFFFFFFFull;
            (void)bad;
            for (k = 0; k < 6; k++) {
                st = (const void *)1; ctx = (void *)1;
                hr = IDWriteFontFileStream_ReadFileFragment(s, &st, o[k], l[k], &ctx);
                CHECKF(hr == E_INVALIDARG, "read(%llx,%llx): %s", (unsigned long long)o[k], (unsigned long long)l[k], dw_hr_name(hr));
                CHECKF(st == NULL && ctx == NULL, "read(%llx,%llx) outputs %p %p", (unsigned long long)o[k], (unsigned long long)l[k], st, ctx);
            }
        }
        IDWriteFontFileStream_Release(s);
    }
    DW_RELEASE(file); free(b); END_F5();
}
TEST(mem_stream_fragment_pointer_in_place_with_owner) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *fo = NULL, *fc = NULL; dwx_effect *owner = dwx_effect_new(6); HRESULT hr;
    REG_M();
    mkmem(m, f, b, (UINT32)n, (IUnknown *)owner, &fo);      /* entry 0: aliased */
    mkmem(m, f, b, (UINT32)n, NULL, &fc);                   /* entry 1: private copy */
    IDWriteFontFileStream *s0 = mem_stream(m, 0, &hr), *s1 = mem_stream(m, 1, &hr);
    const void *p0 = NULL, *p1 = NULL; void *c;
    if (s0) { IDWriteFontFileStream_ReadFileFragment(s0, &p0, 10, 4, &c); CHECK(p0 == b + 10); }
    if (s1) { IDWriteFontFileStream_ReadFileFragment(s1, &p1, 10, 4, &c); CHECK(p1 != NULL && p1 != b + 10 && !memcmp(p1, b + 10, 4)); }
    DW_RELEASE(s0); DW_RELEASE(s1); DW_RELEASE(fo); DW_RELEASE(fc);
    END_F5(); dwx_release(owner); free(b);
}
TEST(mem_stream_invalid_keys) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; IDWriteFontFileStream *s;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    uint8_t key[16] = { 0 }; static const UINT32 sizes[] = { 0, 1, 2, 3, 5, 8, 16 }; size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        s = (IDWriteFontFileStream *)1;
        HRESULT hr = IDWriteInMemoryFontFileLoader_CreateStreamFromKey(m, key, sizes[i], &s);
        CHECKF(hr == E_INVALIDARG && s == NULL, "key size %u: %s stream=%p", sizes[i], dw_hr_name(hr), (void *)s);
    }
    s = (IDWriteFontFileStream *)1;
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateStreamFromKey(m, NULL, 0, &s), E_INVALIDARG); CHECK_OUT_NULL(s);
    {   UINT32 idx; static const UINT32 bad[] = { 1, 2, 100, 0x7FFFFFFF, 0xFFFFFFFF };
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            idx = bad[i]; s = (IDWriteFontFileStream *)1;
            HRESULT hr = IDWriteInMemoryFontFileLoader_CreateStreamFromKey(m, &idx, 4, &s);
            CHECKF(hr == E_INVALIDARG && s == NULL, "index %u: %s", idx, dw_hr_name(hr));
        }
    }
    DW_RELEASE(file); free(b); END_F5();
}
TEST(mem_stream_independent_objects) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; HRESULT hr;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    IDWriteFontFileStream *s1 = mem_stream(m, 0, &hr), *s2 = mem_stream(m, 0, &hr);
    CHECK(s1 && s2 && s1 != s2);
    if (s1) EXPECT_REFS(s1, 1);
    if (s1) { dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontFileStream, 1), DW_QI_ROW(IDWriteFontFileLoader, 0), DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteRemoteFontFileStream, 0) }; DW_CHECK_QI(s1, rows); }
    DW_RELEASE(s1); DW_RELEASE(s2); DW_RELEASE(file); free(b); END_F5();
}
TEST(mem_stream_survives_file_and_data_independence) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; HRESULT hr;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    IDWriteFontFileStream *s = mem_stream(m, 0, &hr);
    DW_RELEASE(file); memset(b, 0, n);
    if (s) { const void *st; void *ctx; CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, 0, 4, &ctx), S_OK); CHECK(st && ((const uint8_t *)st)[1] == 1); DW_RELEASE(s); }
    free(b); END_F5();
}
TEST(mem_stream_via_file_matches_loader_stream) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    if (file) {
        const void *k; UINT32 ks; IDWriteFontFile_GetReferenceKey(file, &k, &ks);
        IDWriteFontFileStream *s = NULL;
        CHECK_HR(IDWriteInMemoryFontFileLoader_CreateStreamFromKey(m, k, ks, &s), S_OK);
        if (s) { UINT64 sz = 0; IDWriteFontFileStream_GetFileSize(s, &sz); EXPECT_EQ(sz, n); DW_RELEASE(s); }
    }
    DW_RELEASE(file); free(b); END_F5();
}

/* ---------------- faces over in-memory data ---------------- */
TEST(mem_face_creation) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), S_OK);
    if (face) {
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
        UINT32 cps[2] = { 'A', 'Z' }; UINT16 g[2]; CHECK_HR(IDWriteFontFace_GetGlyphIndices(face, cps, 2, g), S_OK); EXPECT_EQ(g[0], 2); EXPECT_EQ(g[1], 27);
        UINT32 nf = 1; IDWriteFontFile *got = NULL; CHECK_HR(IDWriteFontFace_GetFiles(face, &nf, &got), S_OK);
        if (got) { const void *k; UINT32 ks; IDWriteFontFile_GetReferenceKey(got, &k, &ks); EXPECT_EQ(ks, 4); DW_RELEASE(got); }
        DW_RELEASE(face);
    }
    DW_RELEASE(file); free(b); END_F5();
}
TEST(mem_face_after_loader_released) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    DW_RELEASE(file);
    IDWriteFactory_UnregisterFontFileLoader(f, MLDR(m)); IDWriteInMemoryFontFileLoader_Release(m); m = NULL;
    memset(b, 0, n);
    if (face) {
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
        DWRITE_GLYPH_METRICS gm; UINT16 id = 2; HRESULT hr = IDWriteFontFace_GetDesignGlyphMetrics(face, &id, 1, &gm, FALSE);
        t_note("glyph metrics after unregistering the loader and clobbering the source buffer: %s adv=%u", dw_hr_name(hr), gm.advanceWidth);
        DW_RELEASE(face);
    }
    IDWriteFactory5_Release(f5); IDWriteFactory_Release(f); free(b);
}
TEST(mem_face_with_owner_keeps_data_alive) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL; dwx_effect *owner = dwx_effect_new(7);
    REG_M(); mkmem(m, f, b, (UINT32)n, (IUnknown *)owner, &file);
    IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    DW_RELEASE(file);
    END_F5();
    dwx_release(owner);
    t_note("owner destroyed while a face created from its memory is still alive: %d", dwx_destroyed(owner));
    if (face) { EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS); DW_RELEASE(face); }
    free(b);
}
TEST(mem_ttc_faces) {
    NEED_F5(); const uint8_t *bl[3]; size_t sz[3]; int i; IDWriteFontFile *file = NULL;
    for (i = 0; i < 3; i++) { char nm[24]; snprintf(nm, sizeof nm, "FlMemTtc%d", i); bl[i] = ttf_build_styled(nm, 400, 5, 0, &sz[i]); }
    size_t n; uint8_t *c = ttf_build_ttc(bl, sz, 3, &n);
    REG_M(); mkmem(m, f, c, (UINT32)n, NULL, &file);
    if (file) {
        an_res a = fl_analyze(file); CHECK_HR(a.hr, S_OK); EXPECT_EQ(a.nf, 3);
        for (i = 0; i < 3; i++) { IDWriteFontFace *face = NULL; CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &file, (UINT32)i, DWRITE_FONT_SIMULATIONS_NONE, &face), S_OK); if (face) { EXPECT_EQ(IDWriteFontFace_GetIndex(face), (UINT32)i); DW_RELEASE(face); } }
    }
    DW_RELEASE(file); free(c); for (i = 0; i < 3; i++) free((void *)bl[i]); END_F5();
}

/* ---------------- font sets / face references fed from custom files ---------------- */
TEST(mem_fontsetbuilder_add_file) {
    NEED_F5(); size_t n; uint8_t *b = fx_ttf(&n); IDWriteFontFile *file = NULL; IDWriteFontSetBuilder1 *bld = NULL;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    CHECK_HR(IDWriteFactory5_CreateFontSetBuilder(f5, &bld), S_OK);
    if (bld && file) {
        CHECK_HR(IDWriteFontSetBuilder1_AddFontFile(bld, file), S_OK);
        IDWriteFontSet *set = NULL; CHECK_HR(IDWriteFontSetBuilder1_CreateFontSet(bld, &set), S_OK);
        if (set) { EXPECT_EQ(IDWriteFontSet_GetFontCount(set), 1); DW_RELEASE(set); }
    }
    if (bld) {
        if (!t_is_wine()) CHECK_HR(IDWriteFontSetBuilder1_AddFontFile(bld, NULL), E_INVALIDARG);
        IDWriteFontSetBuilder1_Release(bld);
    }
    DW_RELEASE(file); free(b); END_F5();
}
TEST(mem_fontsetbuilder_garbage_file) {
    NEED_F5(); size_t n; uint8_t *b = fx_magic(0x11223344, 0x100, &n); IDWriteFontFile *file = NULL; IDWriteFontSetBuilder1 *bld = NULL;
    REG_M(); mkmem(m, f, b, (UINT32)n, NULL, &file);
    IDWriteFactory5_CreateFontSetBuilder(f5, &bld);
    if (bld && file) {
        HRESULT hr = IDWriteFontSetBuilder1_AddFontFile(bld, file);
        t_note("IDWriteFontSetBuilder1::AddFontFile(unsupported file) = %s", dw_hr_name(hr));
        IDWriteFontSet *set = NULL; IDWriteFontSetBuilder1_CreateFontSet(bld, &set);
        if (set) { t_note("  font set count %u", IDWriteFontSet_GetFontCount(set)); DW_RELEASE(set); }
    }
    if (bld) IDWriteFontSetBuilder1_Release(bld);
    DW_RELEASE(file); free(b); END_F5();
}
TEST(mem_fontsetbuilder_custom_loader_traffic) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    IDWriteFactory5 *f5 = fl_factory5(r.f); IDWriteFontSetBuilder1 *bld = NULL;
    if (f5 && SUCCEEDED(IDWriteFactory5_CreateFontSetBuilder(f5, &bld)) && bld) {
        fl_clear(r.ld);
        HRESULT hr = IDWriteFontSetBuilder1_AddFontFile(bld, r.file);
        CHECK_HR(hr, S_OK);
        t_note("AddFontFile over a custom loader: %ld creates, %ld size queries, %ld reads, %ld last-write queries", (long)r.ld->n_create, (long)r.ld->n_size, (long)r.ld->n_read, (long)r.ld->n_lastw);
        CHECK(r.ld->n_create >= 1); CHECK(r.ld->n_read >= 1);
        EXPECT_EQ(r.ld->outstanding, 0);
        IDWriteFontSet *set = NULL; IDWriteFontSetBuilder1_CreateFontSet(bld, &set);
        if (set) { EXPECT_EQ(IDWriteFontSet_GetFontCount(set), 1); DW_RELEASE(set); }
        IDWriteFontSetBuilder1_Release(bld);
    } else t_skip("IDWriteFactory5 not available");
    if (f5) IDWriteFactory5_Release(f5);
    rig_close(&r); free(b);
}
TEST(mem_font_face_reference_from_file) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    void *q = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(r.f, &IID_IDWriteFactory3, &q)) && q) {
        IDWriteFactory3 *f3 = (IDWriteFactory3 *)q; IDWriteFontFaceReference *ref = NULL;
        fl_clear(r.ld);
        HRESULT hr = IDWriteFactory3_CreateFontFaceReference_(f3, r.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
        CHECK_HR(hr, S_OK);
        t_note("CreateFontFaceReference over a custom loader: %ld creates, %ld reads", (long)r.ld->n_create, (long)r.ld->n_read);
        if (ref) {
            EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(ref), 0);
            IDWriteFontFile *got = NULL; CHECK_HR(IDWriteFontFaceReference_GetFontFile(ref, &got), S_OK);
            if (got) {
                const void *k1, *k2; UINT32 s1, s2; IDWriteFontFile_GetReferenceKey(got, &k1, &s1); IDWriteFontFile_GetReferenceKey(r.file, &k2, &s2);
                CHECK(s1 == s2 && !memcmp(k1, k2, s1));
                IDWriteFontFileLoader *l = NULL; IDWriteFontFile_GetLoader(got, &l); CHECK(l == LDR(r.ld)); DW_RELEASE(l);
                DW_RELEASE(got);
            }
            UINT64 sz = IDWriteFontFaceReference_GetFileSize(ref);
            t_note("  reference file size %llu (blob %zu), locality %d", (unsigned long long)sz, n, (int)IDWriteFontFaceReference_GetLocality(ref));
            IDWriteFontFace3 *face = NULL; hr = IDWriteFontFaceReference_CreateFontFace(ref, &face);
            CHECK_HR(hr, S_OK); if (face) { EXPECT_EQ(IDWriteFontFace3_GetGlyphCount(face), TTF_DEF_NGLYPHS); DW_RELEASE(face); }
            DW_RELEASE(ref);
        }
        IDWriteFactory3_Release(f3);
    } else t_skip("IDWriteFactory3 not available");
    rig_close(&r); free(b);
}

/* ---------------- remote (HTTP) loader: object level only, no network is used ---------------- */
TEST(remote_create_http_loader) {
    NEED_F5(); IDWriteRemoteFontFileLoader *rl = (IDWriteRemoteFontFileLoader *)1;
    CHECK_HR(IDWriteFactory5_CreateHttpFontFileLoader(f5, NULL, NULL, &rl), S_OK);
    CHECK(rl != NULL && rl != (IDWriteRemoteFontFileLoader *)1);
    if (rl && rl != (IDWriteRemoteFontFileLoader *)1) {
        dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteRemoteFontFileLoader, 1), DW_QI_ROW(IDWriteFontFileLoader, -1),
            DW_QI_ROW(IDWriteInMemoryFontFileLoader, 0), DW_QI_ROW(IDWriteLocalFontFileLoader, 0), DW_QI_ROW(IDWriteFontFileStream, 0), DW_QI_ROW(IDWriteFontFile, 0),
        };
        DW_CHECK_QI(rl, rows);
        EXPECT_REFS(rl, 1);
        IDWriteRemoteFontFileLoader_Release(rl);
    }
    END_F5_UNREG(0);
}
TEST(remote_create_http_loader_with_strings) {
    NEED_F5(); IDWriteRemoteFontFileLoader *rl = NULL;
    CHECK_HR(IDWriteFactory5_CreateHttpFontFileLoader(f5, L"https://example.invalid/", L"X-Test: 1\r\n", &rl), S_OK);
    CHECK(rl != NULL);
    if (rl) IDWriteRemoteFontFileLoader_Release(rl);
    END_F5_UNREG(0);
}
TEST(remote_loader_distinct_objects) {
    NEED_F5(); IDWriteRemoteFontFileLoader *a = NULL, *b = NULL;
    IDWriteFactory5_CreateHttpFontFileLoader(f5, NULL, NULL, &a); IDWriteFactory5_CreateHttpFontFileLoader(f5, NULL, NULL, &b);
    CHECK(a && b && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    END_F5_UNREG(0);
}
TEST(remote_reference_requires_registration) {
    NEED_F5(); IDWriteRemoteFontFileLoader *rl = NULL; IDWriteFontFile *file = (IDWriteFontFile *)1;
    IDWriteFactory5_CreateHttpFontFileLoader(f5, NULL, NULL, &rl);
    if (rl) {
        HRESULT hr = IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(rl, f, L"https://example.invalid/fonts/", L"a.ttf", &file);
        CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(file);       /* goes through CreateCustomFontFileReference, which needs a registered loader */
        IDWriteRemoteFontFileLoader_Release(rl);
    }
    END_F5_UNREG(0);
}
TEST(remote_reference_registered) {
    NEED_F5(); IDWriteRemoteFontFileLoader *rl = NULL; IDWriteFontFile *file = NULL;
    IDWriteFactory5_CreateHttpFontFileLoader(f5, NULL, NULL, &rl);
    if (rl) {
        CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)rl), S_OK);
        HRESULT hr = IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(rl, f, L"https://example.invalid/fonts/", L"a.ttf", &file);
        CHECK_HR(hr, S_OK);
        if (file) {
            const void *k; UINT32 ks; CHECK_HR(IDWriteFontFile_GetReferenceKey(file, &k, &ks), S_OK);
            CHECK(ks > 0 && (ks % 2) == 0);                    /* UTF-16 URL, no terminator */
            t_note("remote reference key: %u bytes", ks);
            IDWriteFontFileLoader *l = NULL; CHECK_HR(IDWriteFontFile_GetLoader(file, &l), S_OK); CHECK(l == (IDWriteFontFileLoader *)rl); DW_RELEASE(l);
            DW_RELEASE(file);
        }
        CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)rl), S_OK);
        IDWriteRemoteFontFileLoader_Release(rl);
    }
    END_F5_UNREG(0);
}
TEST(remote_reference_url_normalisation_note) {
    NEED_F5(); IDWriteRemoteFontFileLoader *rl = NULL;
    IDWriteFactory5_CreateHttpFontFileLoader(f5, NULL, NULL, &rl);
    if (rl) {
        static const struct { const WCHAR *base, *rel; } u[] = {
            { L"https://example.invalid/fonts/", L"a.ttf" }, { L"https://example.invalid/fonts", L"a.ttf" }, { NULL, L"https://example.invalid/fonts/a.ttf" },
            { L"https://example.invalid/fonts/x/", L"../a.ttf" }, { L"https://EXAMPLE.invalid/Fonts/", L"A.TTF" },
        };
        size_t i;
        IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)rl);
        for (i = 0; i < sizeof u / sizeof u[0]; i++) {
            IDWriteFontFile *file = NULL;
            HRESULT hr = IDWriteRemoteFontFileLoader_CreateFontFileReferenceFromUrl(rl, f, u[i].base, u[i].rel, &file);
            if (file) { const void *k; UINT32 ks = 0; IDWriteFontFile_GetReferenceKey(file, &k, &ks); t_note("url #%zu: %s key %u bytes: %.*ls", i, dw_hr_name(hr), ks, (int)(ks / 2), (const WCHAR *)k); DW_RELEASE(file); }
            else t_note("url #%zu: %s", i, dw_hr_name(hr));
        }
        IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)rl);
        IDWriteRemoteFontFileLoader_Release(rl);
    }
    END_F5_UNREG(0);
}
TEST(remote_interface_not_offered_by_other_loaders) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (rig_open(&r, b, n)) {
        IDWriteFontFileLoader *g = NULL; void *p = (void *)1;
        IDWriteFontFile_GetLoader(r.file, &g);
        /* our own loader is just an object we own; this checks the file/factory side instead: the factory's file is not remote */
        CHECK_HR(IDWriteFontFile_QueryInterface(r.file, &IID_IDWriteRemoteFontFileLoader, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
        p = (void *)1;
        CHECK_HR(IDWriteFactory_QueryInterface(r.f, &IID_IDWriteRemoteFontFileLoader, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
        DW_RELEASE(g);
    }
    rig_close(&r); free(b);
}
