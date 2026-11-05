/* RegisterFontFileLoader / UnregisterFontFileLoader / CreateCustomFontFileReference / IDWriteFontFile object semantics.
 * Behaviour basis: Register (duplicate -> OSException 0x88985006, NULL ->
 * ArgumentException = E_INVALIDARG), ::Unregister (unknown or built-in id <= 2 -> ArgumentException), ::GetIDFromObject
 * (unregistered loader -> ArgumentException), IDWriteFactory::CreateCustomFontFileReference (out zeroed first, NULL key
 * -> E_INVALIDARG, key is copied, file object holds a reference on the loader), IDWriteFontFile::GetLoader/GetReferenceKey. */
#include "fl.h"

#define NEED_F() IDWriteFactory *f = fl_factory(); if (!f) { t_skip("DWriteCreateFactory failed"); return; }

/* ---------------- register / unregister ---------------- */
TEST(reg_register_unregister_ok) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    CHECK_HR(fl_register(l, f), S_OK);
    CHECK_HR(fl_unregister(l), S_OK);
    fl_release(l); IDWriteFactory_Release(f);
}
TEST(reg_twice_already_registered) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    CHECK_HR(fl_register(l, f), S_OK);
    /* second registration of the same pointer: map lookup hits -> OSException(DWRITE_E_ALREADYREGISTERED) */
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), H_ALREADYREG);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), H_ALREADYREG);
    CHECK_HR(fl_unregister(l), S_OK);
    fl_release(l); IDWriteFactory_Release(f);
}
TEST(reg_twice_still_registered_after_failure) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *file = NULL;
    fl_register(l, f);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), H_ALREADYREG);
    CHECK_HR(fl_file4(l, f, 0, &file), S_OK);          /* the first registration is intact */
    DW_RELEASE(file);
    CHECK_HR(fl_unregister(l), S_OK);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), E_INVALIDARG);   /* only one registration existed */
    fl_release(l); IDWriteFactory_Release(f);
}
TEST(reg_null_is_invalidarg) {
    NEED_F();
    if (!t_is_wine()) { t_note("RegisterFontFileLoader(NULL) faults on the Windows runtime; not exercised"); IDWriteFactory_Release(f); return; }
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, NULL), E_INVALIDARG);
    IDWriteFactory_Release(f);
}
TEST(unreg_null_is_invalidarg) {
    NEED_F();
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, NULL), E_INVALIDARG);
    IDWriteFactory_Release(f);
}
TEST(unreg_unknown_is_invalidarg) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), E_INVALIDARG);
    EXPECT_REFS(l, 1);                                  /* nothing retained or released */
    fl_release(l); IDWriteFactory_Release(f);
}
TEST(unreg_twice_second_is_invalidarg) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    fl_register(l, f);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);
    l->registered = 0;
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), E_INVALIDARG);
    EXPECT_REFS(l, 1);
    fl_release(l); IDWriteFactory_Release(f);
}
TEST(reg_refcount_held_by_factory) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    EXPECT_EQ(fl_refs(l), 1);
    CHECK_HR(fl_register(l, f), S_OK);
    EXPECT_EQ(fl_refs(l), 2);                           /* the factory's map holds one reference */
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), H_ALREADYREG);
    EXPECT_EQ(fl_refs(l), 2);                           /* failed registration does not leak a reference */
    CHECK_HR(fl_unregister(l), S_OK);
    EXPECT_EQ(fl_refs(l), 1);
    CHECK(!l->destroyed);
    fl_release(l);
    CHECK(l->destroyed);
    IDWriteFactory_Release(f);
}
TEST(reg_failed_unregister_keeps_refs) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    fl_register(l, f);
    fl_loader *other = fl_new(NULL, 0);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(other)), E_INVALIDARG);
    EXPECT_EQ(fl_refs(l), 2); EXPECT_EQ(fl_refs(other), 1);
    fl_unregister(l); fl_release(l); fl_release(other); IDWriteFactory_Release(f);
}
TEST(reg_loader_not_released_before_unregister) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    fl_register(l, f);
    fl_release(l);                                      /* application drops its reference; the factory keeps the object alive */
    CHECK(!l->destroyed); EXPECT_EQ(fl_refs(l), 1);
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);
    CHECK(l->destroyed);                                /* the unregister dropped the last reference */
    IDWriteFactory_Release(f);
}
TEST(reg_reregister_after_unregister) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); int i;
    for (i = 0; i < 5; i++) {
        CHECK_HR(fl_register(l, f), S_OK);
        EXPECT_EQ(fl_refs(l), 2);
        CHECK_HR(fl_unregister(l), S_OK);
        EXPECT_EQ(fl_refs(l), 1);
    }
    fl_release(l); IDWriteFactory_Release(f);
}
TEST(reg_two_loaders_independent) {
    NEED_F(); fl_loader *a = fl_new(NULL, 0), *b = fl_new(NULL, 0); IDWriteFontFile *fa = NULL, *fb = NULL;
    CHECK_HR(fl_register(a, f), S_OK); CHECK_HR(fl_register(b, f), S_OK);
    CHECK_HR(fl_file4(a, f, 0, &fa), S_OK); CHECK_HR(fl_file4(b, f, 0, &fb), S_OK);
    CHECK_HR(fl_unregister(a), S_OK);
    IDWriteFontFile *fb2 = NULL;
    CHECK_HR(fl_file4(b, f, 1, &fb2), S_OK);            /* b unaffected */
    IDWriteFontFile *fa2 = (IDWriteFontFile *)1;
    CHECK_HR(fl_file4(a, f, 1, &fa2), E_INVALIDARG); CHECK_OUT_NULL(fa2);
    DW_RELEASE(fa); DW_RELEASE(fb); DW_RELEASE(fb2);
    fl_unregister(b); fl_release(a); fl_release(b); IDWriteFactory_Release(f);
}
TEST(reg_many_loaders_any_order) {
    NEED_F(); enum { N = 40 }; fl_loader *l[N]; int i;
    for (i = 0; i < N; i++) { l[i] = fl_new(NULL, 0); CHECK_HR(fl_register(l[i], f), S_OK); }
    for (i = 0; i < N; i++) EXPECT_REFS(l[i], 2) ;
    for (i = 0; i < N; i += 2) CHECK_HR(fl_unregister(l[i]), S_OK);
    for (i = N - 1; i >= 1; i -= 2) CHECK_HR(fl_unregister(l[i]), S_OK);
    for (i = 0; i < N; i++) { EXPECT_EQ(fl_refs(l[i]), 1); fl_release(l[i]); }
    IDWriteFactory_Release(f);
}
TEST(reg_many_loaders_all_usable) {
    NEED_F(); enum { N = 20 }; fl_loader *l[N]; IDWriteFontFile *fi[N]; int i;
    for (i = 0; i < N; i++) { l[i] = fl_new(NULL, 0); fl_register(l[i], f); CHECK_HR(fl_file4(l[i], f, (UINT32)i, &fi[i]), S_OK); }
    for (i = 0; i < N; i++) {
        IDWriteFontFileLoader *got = NULL; CHECK_HR(IDWriteFontFile_GetLoader(fi[i], &got), S_OK);
        CHECK(got == LDR(l[i])); DW_RELEASE(got);
        const void *k; UINT32 ks; CHECK_HR(IDWriteFontFile_GetReferenceKey(fi[i], &k, &ks), S_OK);
        EXPECT_EQ(ks, 4); EXPECT_EQ(*(const UINT32 *)k, (UINT32)i);
    }
    for (i = 0; i < N; i++) { DW_RELEASE(fi[i]); fl_unregister(l[i]); fl_release(l[i]); }
    IDWriteFactory_Release(f);
}
TEST(reg_same_loader_two_factories) {
    NEED_F(); IDWriteFactory *g = fl_factory(); fl_loader *l = fl_new(NULL, 0);
    if (!g) { fl_release(l); IDWriteFactory_Release(f); t_skip("second factory"); return; }
    HRESULT h1 = IDWriteFactory_RegisterFontFileLoader(f, LDR(l));
    HRESULT h2 = IDWriteFactory_RegisterFontFileLoader(g, LDR(l));
    t_note("isolated factories register the same loader: %s / %s (loader refs %ld)", dw_hr_name(h1), dw_hr_name(h2), (long)fl_refs(l));
    CHECK_HR(h1, S_OK);
    if (SUCCEEDED(h2)) IDWriteFactory_UnregisterFontFileLoader(g, LDR(l));
    if (SUCCEEDED(h1)) IDWriteFactory_UnregisterFontFileLoader(f, LDR(l));
    fl_release(l); IDWriteFactory_Release(g); IDWriteFactory_Release(f);
}
TEST(reg_factory_release_drops_loader_ref) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    fl_register(l, f);
    EXPECT_EQ(fl_refs(l), 2);
    ULONG r = IDWriteFactory_Release(f);
    EXPECT_EQ(r, 0);
    t_note("loader refcount after releasing the last factory reference with the loader still registered: %ld", (long)fl_refs(l));
    CHECK(fl_refs(l) >= 1); CHECK(!l->destroyed);
    l->registered = 0;
    fl_release(l);
}
TEST(reg_register_in_shared_factory) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no shared factory"); return; }
    fl_loader *l = fl_new(NULL, 0);
    CHECK_HR(fl_register(l, f), S_OK);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), H_ALREADYREG);
    CHECK_HR(fl_unregister(l), S_OK);
    fl_release(l);
}
TEST(reg_loader_qi_traffic) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    fl_register(l, f);
    t_note("QueryInterface calls on the loader made by Register: %ld", (long)l->qi);
    IDWriteFontFile *file = NULL; fl_file4(l, f, 0, &file);
    t_note("QueryInterface calls on the loader after CreateCustomFontFileReference: %ld", (long)l->qi);
    DW_RELEASE(file); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(reg_no_loader_callbacks_on_register) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0);
    fl_register(l, f);
    EXPECT_EQ(l->nev, 0);                               /* registration never asks the loader for a stream */
    fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}

/* ---------------- built-in loaders ---------------- */
static int tmp_font(IDWriteFactory *f, IDWriteFontFile **file, char *path, size_t cap) {
    size_t n; uint8_t *b = fx_ttf(&n); int ok;
    ok = fl_write_tmp("fl_builtin.ttf", b, n, path, cap); free(b);
    if (!ok) return 0;
    WCHAR w[MAX_PATH]; dw_wide(path, w, MAX_PATH);
    return SUCCEEDED(IDWriteFactory_CreateFontFileReference(f, w, NULL, file)) && *file;
}
TEST(reg_unregister_builtin_local_loader_invalidarg) {
    NEED_F(); IDWriteFontFile *file = NULL; char p[MAX_PATH];
    if (!tmp_font(f, &file, p, sizeof p)) { IDWriteFactory_Release(f); t_skip("no local file"); return; }
    IDWriteFontFileLoader *loader = NULL; CHECK_HR(IDWriteFontFile_GetLoader(file, &loader), S_OK);
    /* built-in loaders carry ids <= 2; Unregister rejects them (ArgumentException) */
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, loader), E_INVALIDARG);
    IDWriteFontFileLoader *again = NULL; CHECK_HR(IDWriteFontFile_GetLoader(file, &again), S_OK);
    CHECK(again == loader);                            /* still the same object */
    DW_RELEASE(again); DW_RELEASE(loader); DW_RELEASE(file); IDWriteFactory_Release(f);
}
TEST(reg_register_builtin_local_loader) {
    NEED_F(); IDWriteFontFile *file = NULL; char p[MAX_PATH];
    if (!tmp_font(f, &file, p, sizeof p)) { IDWriteFactory_Release(f); t_skip("no local file"); return; }
    IDWriteFontFileLoader *loader = NULL; IDWriteFontFile_GetLoader(file, &loader);
    HRESULT hr = IDWriteFactory_RegisterFontFileLoader(f, loader);
    t_note("RegisterFontFileLoader(built-in local loader) = %s", dw_hr_name(hr));
    CHECK(hr == S_OK || hr == H_ALREADYREG);
    if (hr == S_OK) CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, loader), S_OK);
    DW_RELEASE(loader); DW_RELEASE(file); IDWriteFactory_Release(f);
}
TEST(custom_ref_with_builtin_loader_and_local_key) {
    /* the built-in local loader is a valid argument of CreateCustomFontFileReference (found in the built-in table);
     * a key taken from a real local file reference therefore yields a working equivalent reference */
    NEED_F(); IDWriteFontFile *file = NULL, *copy = NULL; char p[MAX_PATH];
    if (!tmp_font(f, &file, p, sizeof p)) { IDWriteFactory_Release(f); t_skip("no local file"); return; }
    IDWriteFontFileLoader *loader = NULL; IDWriteFontFile_GetLoader(file, &loader);
    const void *k; UINT32 ks; CHECK_HR(IDWriteFontFile_GetReferenceKey(file, &k, &ks), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, k, ks, loader, &copy), S_OK);
    if (copy) {
        an_res a = fl_analyze(copy);
        CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(a.nf, 1);
        const void *k2; UINT32 ks2; CHECK_HR(IDWriteFontFile_GetReferenceKey(copy, &k2, &ks2), S_OK);
        EXPECT_EQ(ks2, ks); CHECK(ks2 == ks && !memcmp(k, k2, ks));
        CHECK(k2 != k);
    }
    DW_RELEASE(copy); DW_RELEASE(loader); DW_RELEASE(file); IDWriteFactory_Release(f);
}

/* ---------------- CreateCustomFontFileReference ---------------- */
TEST(custom_ref_unregistered_loader) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *file = (IDWriteFontFile *)0x1; UINT32 key = 7;
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, LDR(l), &file), E_INVALIDARG);   /* GetIDFromObject: not registered */
    CHECK_OUT_NULL(file);
    EXPECT_EQ(l->nev, 0); EXPECT_REFS(l, 1);
    fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_after_unregister) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *file = NULL;
    fl_register(l, f); fl_unregister(l);
    CHECK_HR(fl_file4(l, f, 0, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    fl_register(l, f);
    CHECK_HR(fl_file4(l, f, 0, &file), S_OK); CHECK(file != NULL);
    DW_RELEASE(file); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_null_key) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *file = (IDWriteFontFile *)0x1;
    fl_register(l, f);
    if (!t_is_wine()) {
        CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, NULL, 4, LDR(l), &file), E_INVALIDARG); CHECK_OUT_NULL(file);
        file = (IDWriteFontFile *)0x1;
        CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, NULL, 0, LDR(l), &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    }
    EXPECT_EQ(l->nev, 0); EXPECT_REFS(l, 2);
    fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_null_loader_documented_only) {
    /* NULL loader: the lookup compares against the built-in table and the cache slot; behaviour is not part of the contract. */
    NEED_F(); IDWriteFontFile *file = (IDWriteFontFile *)0x1; UINT32 key = 1;
    HRESULT hr = IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, NULL, &file);
    t_note("CreateCustomFontFileReference(loader=NULL) = %s, out=%p", dw_hr_name(hr), (void *)file);
    if (FAILED(hr)) CHECK_OUT_NULL(file);
    if (SUCCEEDED(hr)) DW_RELEASE(file);
    IDWriteFactory_Release(f);
}
TEST(custom_ref_is_lazy) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *file = NULL;
    fl_register(l, f);
    CHECK_HR(fl_file4(l, f, 0, &file), S_OK);
    EXPECT_EQ(l->n_create, 0); EXPECT_EQ(l->nev, 0); CHECK(l->qi <= 1);   /* the stream is only requested on first use */
    DW_RELEASE(file);
    EXPECT_EQ(l->n_create, 0);
    fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_file_holds_loader_ref) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL, *b = NULL;
    fl_register(l, f);
    EXPECT_EQ(fl_refs(l), 2);
    fl_file4(l, f, 0, &a);
    EXPECT_EQ(fl_refs(l), 3);                          /* FontFileReference keeps a ComPtr<IDWriteFontFileLoader> */
    fl_file4(l, f, 1, &b);
    EXPECT_EQ(fl_refs(l), 4);
    DW_RELEASE(a); EXPECT_EQ(fl_refs(l), 3);
    DW_RELEASE(b); EXPECT_EQ(fl_refs(l), 2);
    fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_keeps_loader_alive_after_unregister) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL;
    fl_register(l, f); fl_file4(l, f, 0, &a);
    CHECK_HR(fl_unregister(l), S_OK);
    fl_release(l);                                     /* app reference gone, registry reference gone: only the file holds it */
    CHECK(!l->destroyed); EXPECT_EQ(fl_refs(l), 1);
    DW_RELEASE(a);
    CHECK(l->destroyed);
    IDWriteFactory_Release(f);
}
TEST(custom_ref_object_refcounting) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL;
    fl_register(l, f); fl_file4(l, f, 0, &a);
    EXPECT_REFS(a, 1);
    ULONG r1 = IDWriteFontFile_AddRef(a), r2 = IDWriteFontFile_AddRef(a), r3 = IDWriteFontFile_Release(a), r4 = IDWriteFontFile_Release(a);
    EXPECT_EQ(r1, 2); EXPECT_EQ(r2, 3); EXPECT_EQ(r3, 2); EXPECT_EQ(r4, 1);
    EXPECT_EQ(fl_refs(l), 3);
    ULONG r5 = IDWriteFontFile_Release(a);
    EXPECT_EQ(r5, 0);
    EXPECT_EQ(fl_refs(l), 2);
    fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_distinct_objects_same_key) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL, *b = NULL;
    fl_register(l, f); fl_file4(l, f, 5, &a); fl_file4(l, f, 5, &b);
    CHECK(a && b && a != b);
    DW_RELEASE(a); DW_RELEASE(b); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_key_is_copied) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL; uint8_t key[16], orig[16]; int i;
    for (i = 0; i < 16; i++) orig[i] = key[i] = (uint8_t)(0xA0 + i);
    fl_register(l, f);
    CHECK_HR(fl_file(l, f, key, 16, &a), S_OK);
    memset(key, 0, sizeof key);                         /* the caller may reuse its buffer at once */
    const void *k = NULL; UINT32 ks = 0;
    CHECK_HR(IDWriteFontFile_GetReferenceKey(a, &k, &ks), S_OK);
    EXPECT_EQ(ks, 16); CHECK(k != NULL && k != key); CHECK(k && !memcmp(k, orig, 16));
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_key_pointer_stable) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL, *b = NULL; UINT32 key = 0x01020304;
    fl_register(l, f); fl_file(l, f, &key, 4, &a); fl_file(l, f, &key, 4, &b);
    const void *k1, *k2, *k3; UINT32 s1, s2, s3;
    IDWriteFontFile_GetReferenceKey(a, &k1, &s1); IDWriteFontFile_GetReferenceKey(a, &k2, &s2); IDWriteFontFile_GetReferenceKey(b, &k3, &s3);
    CHECK(k1 == k2); EXPECT_EQ(s1, s2); EXPECT_EQ(s3, 4); CHECK(k3 != k1);   /* each file object owns its copy */
    DW_RELEASE(a); DW_RELEASE(b); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_key_sizes) {
    static const UINT32 sizes[] = { 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 63, 64, 100, 255, 256, 1000 };
    NEED_F(); fl_loader *l = fl_new(NULL, 0); uint8_t buf[1024]; size_t s, i;
    fl_register(l, f);
    for (s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        IDWriteFontFile *a = NULL; const void *k = NULL; UINT32 ks = 0;
        for (i = 0; i < sizes[s]; i++) buf[i] = (uint8_t)(i * 7 + sizes[s]);
        CHECK_HR(fl_file(l, f, buf, sizes[s], &a), S_OK);
        if (!a) continue;
        CHECK_HR(IDWriteFontFile_GetReferenceKey(a, &k, &ks), S_OK);
        CHECKF(ks == sizes[s], "size %u got %u", sizes[s], ks);
        CHECKF(k && ks == sizes[s] && !memcmp(k, buf, sizes[s]), "key bytes size %u", sizes[s]);
        a->lpVtbl->Release(a);
    }
    fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_key_size_zero_note) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = (IDWriteFontFile *)1; uint8_t b = 1;
    fl_register(l, f);
    HRESULT hr = fl_file(l, f, &b, 0, &a);
    t_note("CreateCustomFontFileReference(keySize=0) = %s", dw_hr_name(hr));
    if (SUCCEEDED(hr)) { const void *k; UINT32 ks = 99; HRESULT h2 = IDWriteFontFile_GetReferenceKey(a, &k, &ks); t_note("  GetReferenceKey = %s size %u", dw_hr_name(h2), ks); DW_RELEASE(a); }
    else CHECK_OUT_NULL(a);
    fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_key_reaches_loader_unchanged) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL; uint8_t key[40]; int i;
    for (i = 0; i < 40; i++) key[i] = (uint8_t)(255 - i * 3);
    fl_register(l, f); fl_file(l, f, key, 40, &a);
    an_res r = fl_analyze(a);
    (void)r;
    EXPECT_EQ(l->n_create, 1);
    EXPECT_EQ(l->last_key_size, 40); CHECK(!memcmp(l->last_key, key, 40));
    const void *k; UINT32 ks; IDWriteFontFile_GetReferenceKey(a, &k, &ks);
    CHECK(l->last_key_ptr == k);                        /* dwrite hands out its own copy, the same pointer GetReferenceKey reports */
    CHECK(l->last_key_ptr != key);
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_key_edit_after_create_not_seen_by_loader) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL; uint8_t key[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    fl_register(l, f); fl_file(l, f, key, 8, &a);
    memset(key, 0xEE, 8);
    fl_analyze(a);
    CHECK(l->last_key[0] == 1 && l->last_key[7] == 8);
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_out_pointer_reset_before_work) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = (IDWriteFontFile *)(ULONG_PTR)0xDEAD0000; UINT32 key = 1;
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, LDR(l), &a), E_INVALIDARG);
    CHECK_OUT_NULL(a);
    fl_release(l); IDWriteFactory_Release(f);
}

/* ---------------- IDWriteFontFile object ---------------- */
TEST(file_qi_matrix) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL;
    fl_register(l, f); fl_file4(l, f, 0, &a);
    int n, i; const dw_qi_row *t = dw_iid_table(&n);
    dw_qi_row *rows = (dw_qi_row *)malloc(n * sizeof *rows);
    for (i = 0; i < n; i++) { rows[i] = t[i]; rows[i].expect = IsEqualIID(t[i].iid, &IID_IDWriteFontFile) ? 1 : 0; }
    dw_qi_check_((IUnknown *)a, rows, n, __FILE__, __LINE__);   /* observed: only IID_IDWriteFontFile and IUnknown are accepted */
    free(rows);
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(file_qi_identity_and_refs) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL, *q = NULL; IUnknown *u = NULL, *u2 = NULL;
    fl_register(l, f); fl_file4(l, f, 0, &a);
    CHECK_HR(IDWriteFontFile_QueryInterface(a, &IID_IDWriteFontFile, (void **)&q), S_OK);
    CHECK(q == a); EXPECT_REFS(a, 2); IDWriteFontFile_Release(q);
    CHECK_HR(IDWriteFontFile_QueryInterface(a, &IID_IUnknown, (void **)&u), S_OK);
    CHECK_HR(IDWriteFontFile_QueryInterface(a, &IID_IUnknown, (void **)&u2), S_OK);
    CHECK(u == u2 && (void *)u == (void *)a);
    IUnknown_Release(u); IUnknown_Release(u2);
    EXPECT_REFS(a, 1);
    void *bad = (void *)1;
    CHECK_HR(IDWriteFontFile_QueryInterface(a, &IID_IDWriteFontFileLoader, &bad), E_NOINTERFACE); CHECK_OUT_NULL(bad);
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(file_getloader_identity) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL;
    fl_register(l, f); fl_file4(l, f, 0, &a);
    IDWriteFontFileLoader *g1 = NULL, *g2 = NULL;
    CHECK_HR(IDWriteFontFile_GetLoader(a, &g1), S_OK);
    CHECK(g1 == LDR(l)); EXPECT_EQ(fl_refs(l), 4);       /* registry + file + caller + app */
    CHECK_HR(IDWriteFontFile_GetLoader(a, &g2), S_OK);
    CHECK(g2 == g1); EXPECT_EQ(fl_refs(l), 5);
    DW_RELEASE(g1); DW_RELEASE(g2); EXPECT_EQ(fl_refs(l), 3);
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(file_getloader_null_out) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL;
    fl_register(l, f); fl_file4(l, f, 0, &a);
    if (!t_is_wine()) CHECK_HR(IDWriteFontFile_GetLoader(a, NULL), E_INVALIDARG);
    EXPECT_EQ(fl_refs(l), 3);
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(file_getloader_after_unregister) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL; IDWriteFontFileLoader *g = NULL;
    fl_register(l, f); fl_file4(l, f, 0, &a);
    fl_unregister(l);
    CHECK_HR(IDWriteFontFile_GetLoader(a, &g), S_OK);   /* the file's own reference is used, not the registry */
    CHECK(g == LDR(l)); DW_RELEASE(g);
    DW_RELEASE(a); fl_release(l); IDWriteFactory_Release(f);
}
TEST(file_survives_factory_release) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL; IDWriteFontFileLoader *g = NULL;
    fl_register(l, f); fl_file4(l, f, 3, &a);
    IDWriteFactory_Release(f);
    CHECK_HR(IDWriteFontFile_GetLoader(a, &g), S_OK); CHECK(g == LDR(l)); DW_RELEASE(g);
    const void *k; UINT32 ks; CHECK_HR(IDWriteFontFile_GetReferenceKey(a, &k, &ks), S_OK);
    EXPECT_EQ(ks, 4); EXPECT_EQ(*(const UINT32 *)k, 3);
    CHECK(!l->destroyed);
    DW_RELEASE(a);
    l->registered = 0;
    t_note("loader refs after file and factory released: %ld", (long)fl_refs(l));
    fl_release(l);
}
TEST(file_getreferencekey_null_args) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL; UINT32 key = 0x55667788;
    fl_register(l, f); fl_file(l, f, &key, 4, &a);
    const void *k = (const void *)1; UINT32 ks = 77;
    if (!t_is_wine()) {     /* a different implementation dereferences the NULLs */
        CHECK_HR(IDWriteFontFile_GetReferenceKey(a, NULL, NULL), E_INVALIDARG);
        CHECK_HR(IDWriteFontFile_GetReferenceKey(a, &k, NULL), E_INVALIDARG); CHECK_OUT_NULL(k);    /* non-NULL outputs are cleared first */
        CHECK_HR(IDWriteFontFile_GetReferenceKey(a, NULL, &ks), E_INVALIDARG); EXPECT_EQ(ks, 0);
    }
    k = (const void *)1; ks = 77;
    CHECK_HR(IDWriteFontFile_GetReferenceKey(a, &k, &ks), S_OK);
    CHECK(k != NULL); EXPECT_EQ(ks, 4); EXPECT_EQ(*(const UINT32 *)k, key);
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(file_getreferencekey_no_loader_callbacks) {
    NEED_F(); fl_loader *l = fl_new(NULL, 0); IDWriteFontFile *a = NULL; const void *k; UINT32 ks;
    fl_register(l, f); fl_file4(l, f, 0, &a);
    IDWriteFontFile_GetReferenceKey(a, &k, &ks); IDWriteFontFile *x = NULL; IDWriteFontFileLoader *g = NULL; IDWriteFontFile_GetLoader(a, &g); DW_RELEASE(g); (void)x;
    EXPECT_EQ(l->nev, 0);
    DW_RELEASE(a); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}

/* ---------------- stream caching / lifetime through the file object ---------------- */
TEST(stream_created_once_per_file_object) {
    rig r; uint8_t *b; size_t n; b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    int i; for (i = 0; i < 5; i++) fl_analyze(r.file);
    EXPECT_EQ(r.ld->n_create, 1);                      /* the stream is cached inside the file reference */
    EXPECT_EQ(r.ld->nstreams, 1);
    rig_close(&r); free(b);
}
TEST(stream_one_per_file_object_two_files) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    IDWriteFontFile *second = NULL; fl_file4(r.ld, r.f, 0, &second);
    fl_analyze(r.file); fl_analyze(second); fl_analyze(second);
    EXPECT_EQ(r.ld->n_create, 2);                      /* no sharing between file objects with the same key */
    EXPECT_EQ(r.ld->nstreams, 2); CHECK(r.ld->streams[0] != r.ld->streams[1]);
    DW_RELEASE(second); rig_close(&r); free(b);
}
TEST(stream_destroyed_when_file_released) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    fl_analyze(r.file);
    EXPECT_EQ(r.ld->nstreams, 1);
    fl_stream *s = r.ld->streams[0];
    EXPECT_EQ(s->ref, 1); CHECK(!s->destroyed);        /* the file reference caches the single reference returned by CreateStreamFromKey */
    DW_RELEASE(r.file);
    CHECK(s->destroyed);
    rig_close(&r); free(b);
}
TEST(stream_not_created_if_never_used) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    const void *k; UINT32 ks; IDWriteFontFile_GetReferenceKey(r.file, &k, &ks);
    IDWriteFontFileLoader *g = NULL; IDWriteFontFile_GetLoader(r.file, &g); DW_RELEASE(g);
    IDWriteFontFile_AddRef(r.file); IDWriteFontFile_Release(r.file);
    EXPECT_EQ(r.ld->n_create, 0);
    rig_close(&r); free(b);
}
TEST(analyze_works_after_unregister) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    CHECK_HR(fl_unregister(r.ld), S_OK);
    an_res a = fl_analyze(r.file);                     /* stream creation uses the loader the file holds, not the registry */
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE);
    EXPECT_EQ(r.ld->n_create, 1);
    rig_close(&r); free(b);
}
TEST(analyze_works_after_loader_app_ref_dropped) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    fl_release(r.ld);                                  /* registry + file still hold it */
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    fl_addref(r.ld);
    rig_close(&r); free(b);
}
TEST(analyze_after_factory_release_note) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    IDWriteFactory_Release(r.f); r.f = NULL; r.ld->registered = 0;
    an_res a = fl_analyze(r.file);
    t_note("Analyze after the factory was released: %s sup=%d", dw_hr_name(a.hr), a.sup);
    CHECK(SUCCEEDED(a.hr) || a.hr != S_OK);
    rig_close(&r); free(b);
}
