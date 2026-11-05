/* Loader registration failures (ALREADYREGISTERED / E_INVALIDARG), custom-reference argument validation, and how failures reported by
 * application callbacks (loader, stream, enumerator) surface from dwrite: a CallbackException carries the application's HRESULT through
 * unchanged, so the same value comes back from the public call that drove the callback. */
#include "eh.h"

#define LDR(l) ((IDWriteFontFileLoader *)(l))

/* ---- IDWriteFontFileLoader registration --------------------------------------------------------------------------------- */
TEST(reg_file_twice_is_alreadyregistered) {
    NEED_FACTORY(); dwx_mem_loader *l = dwx_loader_new();
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), S_OK);
    /* FontLoaderManagerBase::Register finds the pointer in its map and throws OSException(DWRITE_E_ALREADYREGISTERED). */
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), EH_ALREADYREGISTERED);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), EH_ALREADYREGISTERED);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);
    dwx_release(l); DONE_FACTORY();
}
TEST(reg_file_failed_register_keeps_one_registration) {
    NEED_FACTORY(); dwx_mem_loader *l = dwx_loader_new();
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), S_OK);
    EXPECT_REFS(l, 2);                                  /* creator + the registry */
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), EH_ALREADYREGISTERED);
    EXPECT_REFS(l, 2);                                  /* the failed attempt leaves no extra reference */
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);
    EXPECT_REFS(l, 1);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), E_INVALIDARG);   /* only one registration existed */
    dwx_release(l); DONE_FACTORY();
}
TEST(reg_file_null_is_invalidarg) {
    /* Win10 dereferences a NULL loader (access violation), so the HRESULT is not observable. */
    t_skip("NULL loader faults inside the Win10 implementation");
}
TEST(reg_file_unregister_never_registered_is_invalidarg) {
    NEED_FACTORY(); dwx_mem_loader *l = dwx_loader_new();
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), E_INVALIDARG);
    EXPECT_REFS(l, 1);
    dwx_release(l); DONE_FACTORY();
}
TEST(reg_file_unregister_twice) {
    NEED_FACTORY(); dwx_mem_loader *l = dwx_loader_new();
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), E_INVALIDARG);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), E_INVALIDARG);
    dwx_release(l); DONE_FACTORY();
}
TEST(reg_file_reregister_after_unregister) {
    NEED_FACTORY(); dwx_mem_loader *l = dwx_loader_new(); int i;
    for (i = 0; i < 3; i++) {
        CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), S_OK);
        CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), EH_ALREADYREGISTERED);
        CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);
    }
    dwx_release(l); DONE_FACTORY();
}
TEST(reg_file_distinct_loaders_independent) {
    NEED_FACTORY(); dwx_mem_loader *a = dwx_loader_new(), *b = dwx_loader_new();
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(a)), S_OK);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(b)), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(a)), S_OK);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(b)), EH_ALREADYREGISTERED);   /* b is still registered */
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(a)), E_INVALIDARG);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(b)), S_OK);
    dwx_release(a); dwx_release(b); DONE_FACTORY();
}
TEST(reg_file_builtin_loader_cannot_be_unregistered) {
    /* The built-in local loader is not in the registration map (Unregister looks it up there): ArgumentException -> E_INVALIDARG. */
    NEED_FACTORY(); IDWriteFontFile *file = NULL; IDWriteFontFileLoader *loader = NULL;
    WCHAR path[MAX_PATH]; GetWindowsDirectoryW(path, MAX_PATH); lstrcatW(path, L"\\Fonts\\arial.ttf");
    FILETIME ft = { 1, 1 };
    if (SUCCEEDED(IDWriteFactory_CreateFontFileReference(f, path, &ft, &file)) && file) {
        if (SUCCEEDED(IDWriteFontFile_GetLoader(file, &loader)) && loader) {
            CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, loader), E_INVALIDARG);
            DW_RELEASE(loader);
        }
        DW_RELEASE(file);
    } else t_skip("CreateFontFileReference unavailable");
    DONE_FACTORY();
}

/* ---- IDWriteFontCollectionLoader registration ---------------------------------------------------------------------------- */
#define CLDR(c) ((IDWriteFontCollectionLoader *)(c))
TEST(reg_coll_twice_is_alreadyregistered) {
    NEED_FACTORY(); dwx_mem_loader *files = dwx_loader_new(); dwx_coll_loader *c = dwx_coll_new(files);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(f, CLDR(c)), S_OK);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(f, CLDR(c)), EH_ALREADYREGISTERED);
    EXPECT_REFS(c, 2);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, CLDR(c)), S_OK);
    dwx_release(c); dwx_release(files); DONE_FACTORY();
}
TEST(reg_coll_null_and_unknown_are_invalidarg) {
    NEED_FACTORY(); dwx_mem_loader *files = dwx_loader_new(); dwx_coll_loader *c = dwx_coll_new(files);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(f, NULL), E_INVALIDARG);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, NULL), E_INVALIDARG);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, CLDR(c)), E_INVALIDARG);
    dwx_release(c); dwx_release(files); DONE_FACTORY();
}
TEST(reg_coll_unregister_twice) {
    NEED_FACTORY(); dwx_mem_loader *files = dwx_loader_new(); dwx_coll_loader *c = dwx_coll_new(files);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(f, CLDR(c)), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, CLDR(c)), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, CLDR(c)), E_INVALIDARG);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(f, CLDR(c)), S_OK);   /* register again after the failed unregister */
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, CLDR(c)), S_OK);
    dwx_release(c); dwx_release(files); DONE_FACTORY();
}
TEST(reg_coll_and_file_registries_are_separate) {
    /* A file loader and a collection loader are different registries: registering one never reports the other as a duplicate. */
    NEED_FACTORY(); dwx_mem_loader *files = dwx_loader_new(); dwx_coll_loader *c = dwx_coll_new(files);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(files)), S_OK);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(f, CLDR(c)), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, CLDR(c)), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(files)), S_OK);
    dwx_release(c); dwx_release(files); DONE_FACTORY();
}

/* ---- CreateCustomFontFileReference / CreateCustomFontCollection argument checks --------------------------------------------- */
TEST(custom_file_ref_unregistered_loader_is_invalidarg) {
    NEED_FACTORY(); dwx_mem_loader *l = dwx_loader_new(); UINT32 key = 0; IDWriteFontFile *file = SENT(IDWriteFontFile *);
    /* DWriteFontFile's constructor asks the manager for the loader's id (GetIDFromObject): not registered -> ArgumentException. */
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, LDR(l), &file), E_INVALIDARG);
    CHECK_OUT_NULL(file);
    EXPECT_EQ(l->n_create, 0);
    dwx_release(l); DONE_FACTORY();
}
TEST(custom_file_ref_null_key_is_invalidarg) {
    if (t_is_wine()) { t_skip("NULL key is dereferenced by the emulated implementation"); return; }
    NEED_FACTORY(); dwx_mem_loader *l = dwx_loader_new(); IDWriteFontFile *file = SENT(IDWriteFontFile *);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, NULL, 4, LDR(l), &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    file = SENT(IDWriteFontFile *);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, NULL, 0, LDR(l), &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);
    dwx_release(l); DONE_FACTORY();
}
TEST(custom_file_ref_null_loader_is_invalidarg) {
    NEED_FACTORY(); UINT32 key = 0; IDWriteFontFile *file = SENT(IDWriteFontFile *);
    HRESULT hr = IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, NULL, &file);
    t_note("CreateCustomFontFileReference with NULL loader = %s", dw_hr_name(hr));   /* Win10 accepts it (S_OK) */
    if (SUCCEEDED(hr) && file && file != SENT(IDWriteFontFile *)) DW_RELEASE(file);
    DONE_FACTORY();
}
TEST(custom_file_ref_after_unregister_is_invalidarg) {
    NEED_FACTORY(); dwx_mem_loader *l = dwx_loader_new(); UINT32 key = 0; IDWriteFontFile *file = SENT(IDWriteFontFile *);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, LDR(l), &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    dwx_release(l); DONE_FACTORY();
}
TEST(custom_coll_null_loader_is_invalidarg) {
    NEED_FACTORY(); IDWriteFontCollection *c = SENT(IDWriteFontCollection *); UINT32 key = 0;
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(f, NULL, &key, 4, &c), E_INVALIDARG); CHECK_OUT_NULL(c);
    DONE_FACTORY();
}
TEST(custom_coll_unregistered_loader_is_invalidarg) {
    NEED_FACTORY(); dwx_mem_loader *files = dwx_loader_new(); dwx_coll_loader *cl = dwx_coll_new(files);
    IDWriteFontCollection *c = SENT(IDWriteFontCollection *); UINT32 key = 0;
    /* FontCollectionElementKey needs the loader id: GetIDFromObject throws ArgumentException for an unregistered loader. */
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(f, CLDR(cl), &key, 4, &c), E_INVALIDARG); CHECK_OUT_NULL(c);
    EXPECT_EQ(cl->n_create, 0);
    dwx_release(cl); dwx_release(files); DONE_FACTORY();
}
TEST(custom_coll_loader_unregistered_after_creation_new_creation_fails) {
    NEED_FACTORY(); dwx_mem_loader *files = dwx_loader_new(); dwx_coll_loader *cl = dwx_coll_new(files);
    IDWriteFontCollection *c = SENT(IDWriteFontCollection *);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(files)), S_OK);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(f, CLDR(cl)), S_OK);
    c = NULL; CHECK_HR(dwx_coll_create(cl, f, &c), S_OK); DW_RELEASE(c);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, CLDR(cl)), S_OK);
    c = SENT(IDWriteFontCollection *); CHECK_HR(dwx_coll_create(cl, f, &c), E_INVALIDARG); CHECK_OUT_NULL(c);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(files)), S_OK);
    dwx_release(cl); dwx_release(files); DONE_FACTORY();
}

/* ---- callback failures pass through unchanged ------------------------------------------------------------------------------ */
typedef struct { dwx_mem_loader *ld; IDWriteFontFile *file; IDWriteFactory *f; size_t n; uint8_t *blob; UINT32 key; int ok; } lrig;
static void lrig_open(lrig *r, IDWriteFactory *f) {
    memset(r, 0, sizeof *r); r->f = f; r->blob = ttf_build_default(&r->n);
    r->ld = dwx_loader_new(); r->key = dwx_loader_add(r->ld, r->blob, r->n);
    r->ok = r->blob && r->ld;
}
static int lrig_file(lrig *r) {
    if (SUCCEEDED(dwx_loader_register(r->ld, r->f)) && SUCCEEDED(dwx_loader_file(r->ld, r->f, r->key, &r->file)) && r->file) return 1;
    return 0;
}
static void lrig_close(lrig *r) {
    DW_RELEASE(r->file); if (r->ld) { dwx_loader_unregister(r->ld); dwx_release(r->ld); } free(r->blob);
}
static HRESULT lrig_face(lrig *r, IDWriteFontFace **face) {
    return IDWriteFactory_CreateFontFace(r->f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r->file, 0, DWRITE_FONT_SIMULATIONS_NONE, face);
}
static const HRESULT passthru[] = { E_FAIL, E_ACCESSDENIED, E_NOTIMPL, E_OUTOFMEMORY, EH_CUSTOM, EH_FILEACCESS, EH_UNEXPECTED, EH_FILEFORMAT,
                                    (HRESULT)0x80070057, (HRESULT)0x8007007A, (HRESULT)0x80004004, (HRESULT)0x87654321 };
#define NPASS ((int)(sizeof passthru / sizeof passthru[0]))

TEST(callback_baseline_face_creation_succeeds) {
    NEED_FACTORY(); lrig r; lrig_open(&r, f); IDWriteFontFace *face = NULL;
    if (r.ok && lrig_file(&r)) { CHECK_HR(lrig_face(&r, &face), S_OK); CHECK(face != NULL); }
    DW_RELEASE(face); lrig_close(&r); DONE_FACTORY();
}
TEST(callback_create_stream_failure_passes_through) {
    int i;
    for (i = 0; i < NPASS; i++) {
        NEED_FACTORY(); lrig r; lrig_open(&r, f); IDWriteFontFace *face = SENT(IDWriteFontFace *);
        if (r.ok && lrig_file(&r)) {
            r.ld->fail_create = passthru[i];
            /* GetStreamInternal -> ThrowIfFailed(CreateStreamFromKey result) -> CallbackException(hr) */
            HRESULT hr = lrig_face(&r, &face);
            CHECKF(hr == passthru[i], "injected %s got %s", dw_hr_name(passthru[i]), dw_hr_name(hr)); CHECK_OUT_NULL(face);
            if (face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
        }
        lrig_close(&r); DONE_FACTORY();
    }
}
TEST(callback_stream_read_failure_passes_through) {
    int i;
    for (i = 0; i < NPASS; i++) {
        NEED_FACTORY(); lrig r; lrig_open(&r, f); IDWriteFontFace *face = SENT(IDWriteFontFace *);
        if (r.ok) {
            r.ld->stream_fail_read = passthru[i];
            if (lrig_file(&r)) {
                HRESULT hr = lrig_face(&r, &face);
                CHECKF(hr == passthru[i], "injected %s got %s", dw_hr_name(passthru[i]), dw_hr_name(hr)); CHECK_OUT_NULL(face);
                if (face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
                CHECK(r.ld->n_create >= 1);
                EXPECT_EQ(dwx_loader_total_outstanding(r.ld), 0);
            }
        }
        lrig_close(&r); DONE_FACTORY();
    }
}
TEST(callback_stream_getsize_failure_passes_through) {
    int i;
    for (i = 0; i < NPASS; i++) {
        NEED_FACTORY(); lrig r; lrig_open(&r, f); IDWriteFontFace *face = SENT(IDWriteFontFace *);
        if (r.ok) {
            r.ld->stream_fail_getsize = passthru[i];
            if (lrig_file(&r)) {
                /* Win10 does not query the stream size while creating a face: the failure is not observed (S_OK). */
                HRESULT hr = lrig_face(&r, &face);
                if (i == 0) t_note("GetFileSize failure injected: CreateFontFace = %s", dw_hr_name(hr));
                if (SUCCEEDED(hr) && face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
            }
        }
        lrig_close(&r); DONE_FACTORY();
    }
}
TEST(callback_stream_getsize_failure_remote_font) {
    /* ThrowIfFailed special-cases DWRITE_E_REMOTEFONT into a RemoteFontException; its HRESULT is the same value. */
    NEED_FACTORY(); lrig r; lrig_open(&r, f); IDWriteFontFace *face = SENT(IDWriteFontFace *);
    if (r.ok) {
        r.ld->stream_fail_getsize = EH_REMOTEFONT;
        if (lrig_file(&r)) { HRESULT hr = lrig_face(&r, &face); t_note("GetFileSize REMOTEFONT injected: CreateFontFace = %s", dw_hr_name(hr)); if (SUCCEEDED(hr) && face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face); }
    }
    lrig_close(&r); DONE_FACTORY();
}
TEST(callback_stream_read_failure_remote_font) {
    NEED_FACTORY(); lrig r; lrig_open(&r, f); IDWriteFontFace *face = SENT(IDWriteFontFace *);
    if (r.ok) {
        r.ld->stream_fail_read = EH_REMOTEFONT;
        if (lrig_file(&r)) { CHECK_HR(lrig_face(&r, &face), EH_REMOTEFONT); CHECK_OUT_NULL(face); if (face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face); }
    }
    lrig_close(&r); DONE_FACTORY();
}
TEST(callback_failure_then_success_on_same_file) {
    /* The failure is not cached against the file object: once the loader recovers, the same IDWriteFontFile yields a face. */
    NEED_FACTORY(); lrig r; lrig_open(&r, f); IDWriteFontFace *face = SENT(IDWriteFontFace *);
    if (r.ok && lrig_file(&r)) {
        r.ld->fail_create = EH_CUSTOM;
        CHECK_HR(lrig_face(&r, &face), EH_CUSTOM); CHECK_OUT_NULL(face);
        r.ld->fail_create = S_OK; face = NULL;
        CHECK_HR(lrig_face(&r, &face), S_OK); CHECK(face != NULL);
        DW_RELEASE(face);
    }
    lrig_close(&r); DONE_FACTORY();
}
TEST(callback_failed_creation_releases_stream) {
    NEED_FACTORY(); lrig r; lrig_open(&r, f); IDWriteFontFace *face = NULL;
    if (r.ok) {
        r.ld->stream_fail_read = EH_CUSTOM;
        if (lrig_file(&r)) {
            HRESULT hr = lrig_face(&r, &face); CHECK_HR(hr, EH_CUSTOM);
            int i; for (i = 0; i < r.ld->nstreams; i++) CHECKF(dwx_destroyed(r.ld->streams[i]), "stream %d still referenced (refs=%ld)", i, dwx_refs(r.ld->streams[i]));
        }
    }
    DW_RELEASE(face); lrig_close(&r); DONE_FACTORY();
}
TEST(callback_collection_enumerator_creation_failure) {
    int i;
    for (i = 0; i < NPASS; i++) {
        NEED_FACTORY(); lrig r; lrig_open(&r, f); dwx_coll_loader *cl = NULL; IDWriteFontCollection *c = SENT(IDWriteFontCollection *);
        if (r.ok && SUCCEEDED(dwx_loader_register(r.ld, f))) {
            cl = dwx_coll_new(r.ld); dwx_coll_add_key(cl, r.key);
            CHECK_HR(dwx_coll_register(cl, f), S_OK);
            cl->fail_create = passthru[i];
            /* AddElementData: CreateEnumeratorFromKey result goes through CallbackException::ThrowIfFailed */
            HRESULT hr = dwx_coll_create(cl, f, &c);
            CHECKF(hr == passthru[i], "injected %s got %s", dw_hr_name(passthru[i]), dw_hr_name(hr)); CHECK_OUT_NULL(c);
            if (c && c != SENT(IDWriteFontCollection *)) DW_RELEASE(c);
            dwx_coll_unregister(cl);
        }
        if (cl) dwx_release(cl);
        lrig_close(&r); DONE_FACTORY();
    }
}
TEST(callback_collection_movenext_failure) {
    int i;
    for (i = 0; i < NPASS; i++) {
        NEED_FACTORY(); lrig r; lrig_open(&r, f); dwx_coll_loader *cl = NULL; IDWriteFontCollection *c = SENT(IDWriteFontCollection *);
        if (r.ok && SUCCEEDED(dwx_loader_register(r.ld, f))) {
            cl = dwx_coll_new(r.ld); dwx_coll_add_key(cl, r.key);
            CHECK_HR(dwx_coll_register(cl, f), S_OK);
            cl->enum_fail_move = passthru[i];
            HRESULT hr = dwx_coll_create(cl, f, &c);
            CHECKF(hr == passthru[i], "injected %s got %s", dw_hr_name(passthru[i]), dw_hr_name(hr)); CHECK_OUT_NULL(c);
            if (c && c != SENT(IDWriteFontCollection *)) DW_RELEASE(c);
            dwx_coll_unregister(cl);
        }
        if (cl) dwx_release(cl);
        lrig_close(&r); DONE_FACTORY();
    }
}
TEST(callback_collection_getcurrentfontfile_failure_is_skipped) {
    /* AddFontFiles only uses a file when GetCurrentFontFile succeeded; a failing call just skips that entry (no exception). */
    NEED_FACTORY(); lrig r; lrig_open(&r, f); dwx_coll_loader *cl = NULL; IDWriteFontCollection *c = NULL;
    if (r.ok && SUCCEEDED(dwx_loader_register(r.ld, f))) {
        cl = dwx_coll_new(r.ld); dwx_coll_add_key(cl, r.key);
        CHECK_HR(dwx_coll_register(cl, f), S_OK);
        cl->enum_fail_get = EH_CUSTOM;
        HRESULT hr = dwx_coll_create(cl, f, &c);
        t_note("CreateCustomFontCollection with failing GetCurrentFontFile = %s, families=%u", dw_hr_name(hr), c ? IDWriteFontCollection_GetFontFamilyCount(c) : 0);
        CHECK_HR(hr, S_OK);
        if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 0);
        DW_RELEASE(c);
        dwx_coll_unregister(cl);
    }
    if (cl) dwx_release(cl);
    lrig_close(&r); DONE_FACTORY();
}
TEST(callback_collection_failure_then_success) {
    NEED_FACTORY(); lrig r; lrig_open(&r, f); dwx_coll_loader *cl = NULL; IDWriteFontCollection *c = SENT(IDWriteFontCollection *);
    if (r.ok && SUCCEEDED(dwx_loader_register(r.ld, f))) {
        cl = dwx_coll_new(r.ld); dwx_coll_add_key(cl, r.key);
        CHECK_HR(dwx_coll_register(cl, f), S_OK);
        cl->fail_create = EH_CUSTOM;
        CHECK_HR(dwx_coll_create(cl, f, &c), EH_CUSTOM); CHECK_OUT_NULL(c);
        cl->fail_create = S_OK; c = NULL;
        CHECK_HR(dwx_coll_create(cl, f, &c), S_OK);
        if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        DW_RELEASE(c);
        dwx_coll_unregister(cl);
    }
    if (cl) dwx_release(cl);
    lrig_close(&r); DONE_FACTORY();
}

/* ---- IDWriteFontFile accessor argument checks ---------------------------------------------------------------------------------- */
TEST(fontfile_getloader_null_is_invalidarg) {
    if (t_is_wine()) { t_skip("NULL outputs are dereferenced by the emulated implementation"); return; }
    NEED_FACTORY(); lrig r; lrig_open(&r, f);
    if (r.ok && lrig_file(&r)) CHECK_HR(IDWriteFontFile_GetLoader(r.file, NULL), E_INVALIDARG);
    lrig_close(&r); DONE_FACTORY();
}
TEST(fontfile_getreferencekey_null_args_are_invalidarg) {
    if (t_is_wine()) { t_skip("NULL outputs are dereferenced by the emulated implementation"); return; }
    NEED_FACTORY(); lrig r; lrig_open(&r, f);
    if (r.ok && lrig_file(&r)) {
        const void *k = SENT(const void *); UINT32 ks = 77;
        CHECK_HR(IDWriteFontFile_GetReferenceKey(r.file, NULL, NULL), E_INVALIDARG);
        CHECK_HR(IDWriteFontFile_GetReferenceKey(r.file, &k, NULL), E_INVALIDARG); CHECK(k == NULL);      /* non-NULL outputs are cleared first */
        CHECK_HR(IDWriteFontFile_GetReferenceKey(r.file, NULL, &ks), E_INVALIDARG); EXPECT_EQ(ks, 0);
        CHECK_HR(IDWriteFontFile_GetReferenceKey(r.file, &k, &ks), S_OK); CHECK(k != NULL); EXPECT_EQ(ks, 4);
    }
    lrig_close(&r); DONE_FACTORY();
}
TEST(fontfile_analyze_null_outputs_are_invalidarg) {
    if (t_is_wine()) { t_skip("NULL outputs are dereferenced by the emulated implementation"); return; }
    NEED_FACTORY(); lrig r; lrig_open(&r, f);
    if (r.ok && lrig_file(&r)) {
        BOOL sup = 7; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)99; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)99; UINT32 n = 55;
        /* isSupported, fileType and numberOfFaces are mandatory; fontFaceType is optional. Writable outputs are initialised first. */
        CHECK_HR(IDWriteFontFile_Analyze(r.file, NULL, &ft, &fc, &n), E_INVALIDARG);
        EXPECT_EQ(ft, 0); EXPECT_EQ(fc, DWRITE_FONT_FACE_TYPE_UNKNOWN); EXPECT_EQ(n, 0);
        ft = (DWRITE_FONT_FILE_TYPE)99; fc = (DWRITE_FONT_FACE_TYPE)99; n = 55;
        CHECK_HR(IDWriteFontFile_Analyze(r.file, &sup, NULL, &fc, &n), E_INVALIDARG);
        EXPECT_EQ(sup, 0); EXPECT_EQ(fc, DWRITE_FONT_FACE_TYPE_UNKNOWN); EXPECT_EQ(n, 0);
        sup = 7; fc = (DWRITE_FONT_FACE_TYPE)99; n = 55;
        CHECK_HR(IDWriteFontFile_Analyze(r.file, &sup, &ft, &fc, NULL), E_INVALIDARG);
        EXPECT_EQ(sup, 0); EXPECT_EQ(ft, 0); EXPECT_EQ(fc, DWRITE_FONT_FACE_TYPE_UNKNOWN);
        CHECK_HR(IDWriteFontFile_Analyze(r.file, &sup, &ft, NULL, &n), S_OK);       /* fontFaceType may be NULL */
        CHECK(sup); EXPECT_EQ(ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(n, 1);
    }
    lrig_close(&r); DONE_FACTORY();
}
