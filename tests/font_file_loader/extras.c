/* Cross-cutting behaviour: locality queries that probe the application loader, re-entrancy, concurrency, large keys,
 * and installed fonts reached through the system collection. */
#include "fl.h"

static IDWriteFontFaceReference *face_ref(rig *r) {
    void *q = NULL; IDWriteFontFaceReference *ref = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(r->f, &IID_IDWriteFactory3, &q)) || !q) return NULL;
    IDWriteFactory3_CreateFontFaceReference_((IDWriteFactory3 *)q, r->file, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    IDWriteFactory3_Release((IDWriteFactory3 *)q);
    return ref;
}

/* ---------------- locality: GetLocality queries the loader for IDWriteRemoteFontFileLoader ---------------- */
TEST(loc_plain_loader_is_local) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    IDWriteFontFaceReference *ref = face_ref(&r);
    if (!ref) { rig_close(&r); free(b); t_skip("IDWriteFactory3 not available"); return; }
    LONG q0 = r.ld->n_qi_remote;
    DWRITE_LOCALITY loc = IDWriteFontFaceReference_GetLocality(ref);
    EXPECT_EQ(loc, DWRITE_LOCALITY_LOCAL);               /* no remote interface on the loader: the font is local */
    CHECK(r.ld->n_qi_remote > q0);                       /* ... which dwrite finds out by asking the loader for IDWriteRemoteFontFileLoader */
    CHECK(IsEqualIID(&r.ld->last_qi_iid, &IID_IDWriteRemoteFontFileLoader));
    EXPECT_EQ(r.ld->n_loc, 0);
    DW_RELEASE(ref); rig_close(&r); free(b);
}
TEST(loc_plain_loader_query_per_call) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    IDWriteFontFaceReference *ref = face_ref(&r);
    if (!ref) { rig_close(&r); free(b); t_skip("IDWriteFactory3 not available"); return; }
    IDWriteFontFaceReference_GetLocality(ref);
    LONG a = r.ld->n_qi_remote;
    IDWriteFontFaceReference_GetLocality(ref); IDWriteFontFaceReference_GetLocality(ref);
    t_note("loader QI(IDWriteRemoteFontFileLoader) calls: %ld after 1 GetLocality, %ld after 3", (long)a, (long)r.ld->n_qi_remote);
    CHECK(r.ld->n_qi_remote >= a);
    DW_RELEASE(ref); rig_close(&r); free(b);
}
TEST(loc_remote_capable_loader_reports_its_value) {
    static const DWRITE_LOCALITY vals[] = { DWRITE_LOCALITY_REMOTE, DWRITE_LOCALITY_PARTIAL, DWRITE_LOCALITY_LOCAL };
    size_t i;
    for (i = 0; i < 3; i++) {
        rig r; size_t n; uint8_t *b = fx_ttf(&n);
        if (!rig_open(&r, b, n)) { rig_close(&r); free(b); continue; }
        r.ld->remote = 1; r.ld->loc = vals[i];
        IDWriteFontFaceReference *ref = face_ref(&r);
        if (!ref) { rig_close(&r); free(b); t_skip("IDWriteFactory3 not available"); return; }
        DWRITE_LOCALITY loc = IDWriteFontFaceReference_GetLocality(ref);
        CHECKF(loc == vals[i], "loader says %d, GetLocality says %d", vals[i], loc);
        CHECK(r.ld->n_loc >= 1);
        EXPECT_EQ(r.ld->loc_key_size, 4);                /* GetLocalityFromKey receives the reference key of the file */
        CHECK(r.ld->loc_key_size == 4 && *(UINT32 *)r.ld->loc_key == 0);
        DW_RELEASE(ref); rig_close(&r); free(b);
    }
}
TEST(loc_remote_capable_loader_key_is_forwarded) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    DW_RELEASE(r.file);
    r.ld->remote = 1; r.ld->loc = DWRITE_LOCALITY_PARTIAL;
    uint8_t key[12] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 11, 12 };
    CHECK_HR(fl_file(r.ld, r.f, key, 12, &r.file), S_OK);
    IDWriteFontFaceReference *ref = face_ref(&r);
    if (ref) {
        CHECK(IDWriteFontFaceReference_GetLocality(ref) == DWRITE_LOCALITY_PARTIAL);
        EXPECT_EQ(r.ld->loc_key_size, 12); CHECK(!memcmp(r.ld->loc_key, key, 12));
        DW_RELEASE(ref);
    } else t_skip("IDWriteFactory3 not available");
    rig_close(&r); free(b);
}
TEST(loc_remote_capable_loader_still_streams_through_createstream) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    r.ld->remote = 1;
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);                   /* Analyze uses IDWriteFontFileLoader::CreateStreamFromKey even on a remote capable loader */
    EXPECT_EQ(r.ld->n_create, 1);
    rig_close(&r); free(b);
}
TEST(loc_local_builtin_loader_is_local) {
    IDWriteFactory *f = fl_factory(); char p[MAX_PATH]; size_t n; IDWriteFontFace *face = NULL;
    if (!f) { t_skip("factory"); return; }
    uint8_t *b = fx_ttf(&n); int ok = fl_write_tmp("fl_loc.ttf", b, n, p, sizeof p); free(b);
    void *q = NULL; WCHAR w[MAX_PATH]; dw_wide(p, w, MAX_PATH);
    if (ok && SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, &q)) && q) {
        IDWriteFontFaceReference *ref = NULL;
        HRESULT hr = IDWriteFactory3_CreateFontFaceReference((IDWriteFactory3 *)q, w, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
        CHECK_HR(hr, S_OK);
        if (ref) {
            EXPECT_EQ(IDWriteFontFaceReference_GetLocality(ref), DWRITE_LOCALITY_LOCAL);
            EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(ref), n);
            EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(ref), 0);
            IDWriteFontFile *file = NULL; CHECK_HR(IDWriteFontFaceReference_GetFontFile(ref, &file), S_OK);
            if (file) { an_res a = fl_analyze(file); CHECK(a.sup); DW_RELEASE(file); }
            DW_RELEASE(ref);
        }
        IDWriteFactory3_Release((IDWriteFactory3 *)q);
    } else t_skip("scratch file or IDWriteFactory3 unavailable");
    (void)face;
    IDWriteFactory_Release(f);
}
TEST(loc_face_reference_over_missing_local_file) {
    IDWriteFactory *f = fl_factory(); void *q = NULL; char p[MAX_PATH]; WCHAR w[MAX_PATH];
    if (!f) { t_skip("factory"); return; }
    t_tmpfile(p, sizeof p, "fl_ref_missing.ttf"); dw_wide(p, w, MAX_PATH);
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, &q)) && q) {
        IDWriteFontFaceReference *ref = (IDWriteFontFaceReference *)1;
        HRESULT hr = IDWriteFactory3_CreateFontFaceReference((IDWriteFactory3 *)q, w, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
        t_note("CreateFontFaceReference(missing path, NULL time) = %s", dw_hr_name(hr));
        CHECK(FAILED(hr)); CHECK_OUT_NULL(ref);
        CHECK_HR(hr, H_FILENOTFOUND);
        IDWriteFactory3_Release((IDWriteFactory3 *)q);
    } else t_skip("IDWriteFactory3 not available");
    IDWriteFactory_Release(f);
}

/* ---------------- re-entrancy ---------------- */
static IDWriteFactory *g_reent_f; static fl_loader *g_reent_other; static int g_reent_ok, g_reent_calls;
static void reent_hook(fl_loader *l, UINT64 off, UINT64 size) {
    IDWriteFontFile *file = NULL; HRESULT hr;
    if (g_reent_calls++ > 0) return;
    hr = fl_file4(g_reent_other, g_reent_f, 0, &file);          /* the application calls back into dwrite from inside ReadFileFragment */
    if (SUCCEEDED(hr) && file) {
        an_res a = fl_analyze(file);
        g_reent_ok = (a.hr == S_OK && a.sup);
        IDWriteFontFile_Release(file);
    }
}
TEST(reentrant_factory_calls_from_read_callback) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    g_reent_other = fl_new(b, n); g_reent_f = r.f; g_reent_ok = 0; g_reent_calls = 0;
    CHECK_HR(fl_register(g_reent_other, r.f), S_OK);
    r.ld->on_read = reent_hook;
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    CHECK(g_reent_ok);
    EXPECT_EQ(g_reent_other->n_create, 1);
    r.ld->on_read = NULL;
    fl_unregister(g_reent_other); fl_release(g_reent_other);
    rig_close(&r); free(b);
}
static void reent_reg_hook(fl_loader *l, UINT64 off, UINT64 size) {
    if (g_reent_calls++ > 0) return;
    g_reent_ok = SUCCEEDED(fl_register(g_reent_other, g_reent_f)) && SUCCEEDED(fl_unregister(g_reent_other));
}
TEST(reentrant_register_from_read_callback) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    g_reent_other = fl_new(NULL, 0); g_reent_f = r.f; g_reent_ok = 0; g_reent_calls = 0;
    r.ld->on_read = reent_reg_hook;
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); CHECK(g_reent_ok);
    r.ld->on_read = NULL;
    fl_release(g_reent_other);
    rig_close(&r); free(b);
}
static void reent_self_hook(fl_loader *l, UINT64 off, UINT64 size) {
    IDWriteFontFile *file = NULL;
    if (g_reent_calls++ > 0) return;
    if (SUCCEEDED(fl_file4(l, g_reent_f, 0, &file)) && file) { an_res a = fl_analyze(file); g_reent_ok = a.hr == S_OK && a.sup; IDWriteFontFile_Release(file); }
}
TEST(reentrant_analyze_of_second_file_over_same_loader) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    g_reent_f = r.f; g_reent_ok = 0; g_reent_calls = 0;
    r.ld->on_read = reent_self_hook;
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); CHECK(g_reent_ok);
    EXPECT_EQ(r.ld->n_create, 2);
    r.ld->on_read = NULL;
    rig_close(&r); free(b);
}

/* ---------------- concurrency ---------------- */
struct regctx { IDWriteFactory *f; fl_loader *l[40]; int ok; };
static DWORD WINAPI reg_worker(LPVOID p) {
    struct regctx *c = (struct regctx *)p; int i, ok = 1;
    for (i = 0; i < 40; i++) c->l[i] = fl_new(NULL, 0);
    for (i = 0; i < 40; i++) if (FAILED(fl_register(c->l[i], c->f))) ok = 0;
    for (i = 0; i < 40; i++) {
        IDWriteFontFile *file = NULL;
        if (FAILED(fl_file4(c->l[i], c->f, (UINT32)i, &file)) || !file) ok = 0; else IDWriteFontFile_Release(file);
    }
    for (i = 39; i >= 0; i--) if (FAILED(fl_unregister(c->l[i]))) ok = 0;
    c->ok = ok; return 0;
}
TEST(reg_concurrent_register_use_unregister) {
    IDWriteFactory *f = fl_factory(); struct regctx cs[4]; HANDLE th[4]; int i, k;
    if (!f) { t_skip("factory"); return; }
    for (i = 0; i < 4; i++) { memset(&cs[i], 0, sizeof cs[i]); cs[i].f = f; th[i] = CreateThread(NULL, 0, reg_worker, &cs[i], 0, NULL); }
    WaitForMultipleObjects(4, th, TRUE, 30000);
    for (i = 0; i < 4; i++) { CloseHandle(th[i]); CHECKF(cs[i].ok, "thread %d", i); for (k = 0; k < 40; k++) if (cs[i].l[k]) { EXPECT_EQ(fl_refs(cs[i].l[k]), 1); fl_release(cs[i].l[k]); } }
    IDWriteFactory_Release(f);
}
struct anctx { IDWriteFactory *f; fl_loader *l; int ok; };
static DWORD WINAPI an_worker(LPVOID p) {
    struct anctx *c = (struct anctx *)p; int i, ok = 1;
    for (i = 0; i < 20; i++) {
        IDWriteFontFile *file = NULL;
        if (FAILED(fl_file4(c->l, c->f, 0, &file)) || !file) { ok = 0; continue; }
        an_res a = fl_analyze(file);
        if (a.hr != S_OK || !a.sup) ok = 0;
        IDWriteFontFile_Release(file);
    }
    c->ok = ok; return 0;
}
TEST(concurrent_files_over_one_loader) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n); struct anctx cs[4]; HANDLE th[4]; int i;
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    for (i = 0; i < 4; i++) { cs[i].f = r.f; cs[i].l = r.ld; cs[i].ok = 0; th[i] = CreateThread(NULL, 0, an_worker, &cs[i], 0, NULL); }
    WaitForMultipleObjects(4, th, TRUE, 30000);
    for (i = 0; i < 4; i++) { CloseHandle(th[i]); CHECKF(cs[i].ok, "thread %d", i); }
    EXPECT_EQ(r.ld->n_create, 80);                        /* one stream per file object */
    EXPECT_EQ(r.ld->outstanding, 0); EXPECT_EQ(r.ld->bad_rel, 0);
    rig_close(&r); free(b);
}

/* ---------------- big keys ---------------- */
TEST(custom_ref_large_keys) {
    static const UINT32 sizes[] = { 4096, 65535, 65536, 70000, 1000000 }; size_t s;
    IDWriteFactory *f = fl_factory(); fl_loader *l; uint8_t *buf;
    if (!f) { t_skip("factory"); return; }
    l = fl_new(NULL, 0); fl_register(l, f); buf = (uint8_t *)malloc(1000000);
    for (s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        IDWriteFontFile *file = NULL; UINT32 i;
        for (i = 0; i < sizes[s]; i++) buf[i] = (uint8_t)(i * 31 + s);
        CHECK_HR(fl_file(l, f, buf, sizes[s], &file), S_OK);
        if (file) {
            const void *k; UINT32 ks; CHECK_HR(IDWriteFontFile_GetReferenceKey(file, &k, &ks), S_OK);
            CHECK(ks == sizes[s] && !memcmp(k, buf, sizes[s]));
            IDWriteFontFile_Release(file);
        }
    }
    free(buf); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_binary_keys_with_zeros) {
    IDWriteFactory *f = fl_factory(); fl_loader *l; IDWriteFontFile *file = NULL; uint8_t key[16] = { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0xFF, 0, 0, 0, 0, 0, 0 };
    if (!f) { t_skip("factory"); return; }
    l = fl_new(NULL, 0); fl_register(l, f);
    CHECK_HR(fl_file(l, f, key, 16, &file), S_OK);
    fl_analyze(file);
    EXPECT_EQ(l->last_key_size, 16); CHECK(!memcmp(l->last_key, key, 16));
    DW_RELEASE(file); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}
TEST(custom_ref_all_zero_and_all_ones_keys) {
    IDWriteFactory *f = fl_factory(); fl_loader *l; uint8_t z[32], o[32];
    if (!f) { t_skip("factory"); return; }
    memset(z, 0, 32); memset(o, 0xFF, 32); l = fl_new(NULL, 0); fl_register(l, f);
    IDWriteFontFile *a = NULL, *b = NULL;
    CHECK_HR(fl_file(l, f, z, 32, &a), S_OK); CHECK_HR(fl_file(l, f, o, 32, &b), S_OK);
    const void *k; UINT32 ks; IDWriteFontFile_GetReferenceKey(a, &k, &ks); CHECK(ks == 32 && !memcmp(k, z, 32));
    IDWriteFontFile_GetReferenceKey(b, &k, &ks); CHECK(ks == 32 && !memcmp(k, o, 32));
    DW_RELEASE(a); DW_RELEASE(b); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
}

/* ---------------- installed fonts reached through the system collection ---------------- */
TEST(sys_collection_face_files_are_local_files) {
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face; char want[MAX_PATH];
    if (!f) { t_skip("no shared factory"); return; }
    face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Arial is not installed"); return; }
    UINT32 nf = 0; CHECK_HR(IDWriteFontFace_GetFiles(face, &nf, NULL), S_OK); EXPECT_EQ(nf, 1);
    IDWriteFontFile *file = NULL; nf = 1; CHECK_HR(IDWriteFontFace_GetFiles(face, &nf, &file), S_OK);
    if (file) {
        IDWriteLocalFontFileLoader *ll = NULL; IDWriteFontFileLoader *l = NULL;
        CHECK_HR(IDWriteFontFile_GetLoader(file, &l), S_OK);
        CHECK(l != NULL);
        if (l) CHECK_HR(IDWriteFontFileLoader_QueryInterface(l, &IID_IDWriteLocalFontFileLoader, (void **)&ll), S_OK);
        const void *k; UINT32 ks; CHECK_HR(IDWriteFontFile_GetReferenceKey(file, &k, &ks), S_OK);
        if (ll) {
            UINT32 len = 0; CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(ll, k, ks, &len), S_OK);
            WCHAR *buf = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
            CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, k, ks, buf, len + 1), S_OK);
            dw_narrow(buf, want, sizeof want);
            t_note("Arial regular comes from %s", want);
            CHECK(GetFileAttributesA(want) != INVALID_FILE_ATTRIBUTES);
            size_t wl = strlen(want); CHECK(wl > 4 && !lstrcmpiA(want + wl - 4, ".ttf"));
            FILETIME t; CHECK_HR(IDWriteLocalFontFileLoader_GetLastWriteTimeFromKey(ll, k, ks, &t), S_OK);
            free(buf);
            IDWriteLocalFontFileLoader_Release(ll);
        }
        an_res a = fl_analyze(file); CHECK_HR(a.hr, S_OK); CHECK(a.sup);
        if (l) IDWriteFontFileLoader_Release(l);
        IDWriteFontFile_Release(file);
    }
    IDWriteFontFace_Release(face);
}
TEST(sys_collection_all_faces_have_one_local_file_each) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *coll = NULL; UINT32 i, checked = 0;
    if (!f) { t_skip("no shared factory"); return; }
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &coll, FALSE)) || !coll) { t_skip("no system collection"); return; }
    UINT32 nfam = IDWriteFontCollection_GetFontFamilyCount(coll);
    for (i = 0; i < nfam && checked < 25; i += 7) {
        IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
        if (FAILED(IDWriteFontCollection_GetFontFamily(coll, i, &fam)) || !fam) continue;
        if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) && font && SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
            UINT32 nf = 0; IDWriteFontFace_GetFiles(face, &nf, NULL);
            CHECK(nf >= 1);
            IDWriteFontFile *files[4] = { 0 }; if (nf > 4) nf = 4;
            if (SUCCEEDED(IDWriteFontFace_GetFiles(face, &nf, files))) {
                UINT32 k;
                for (k = 0; k < nf; k++) {
                    IDWriteFontFileLoader *l = NULL; IDWriteLocalFontFileLoader *ll = NULL;
                    if (files[k] && SUCCEEDED(IDWriteFontFile_GetLoader(files[k], &l)) && l) {
                        HRESULT hr = IDWriteFontFileLoader_QueryInterface(l, &IID_IDWriteLocalFontFileLoader, (void **)&ll);
                        CHECKF(hr == S_OK, "family %u file %u: QI(IDWriteLocalFontFileLoader)=%s", i, k, dw_hr_name(hr));
                        if (ll) IDWriteLocalFontFileLoader_Release(ll);
                        IDWriteFontFileLoader_Release(l);
                    }
                    DW_RELEASE(files[k]);
                }
            }
            checked++;
        }
        DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam);
    }
    t_note("checked %u system faces", checked);
    CHECK(checked > 0);
    IDWriteFontCollection_Release(coll);
}
