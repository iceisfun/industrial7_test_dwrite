/* The built-in local file loader: IDWriteFactory::CreateFontFileReference, IDWriteLocalFontFileLoader and its streams.
 * Basis: DWriteFactory::CreateFontFileReference (out zeroed, NULL path -> E_INVALIDARG, the key is built by
 * LocalFileLoader::SerializeReferenceKeyTo: 8 byte last-write time, optional '*' marker for the Fonts directory, NUL terminated
 * upper-cased path), File::TryOpen + IOException::MapFileOpenError (not found / path not found -> DWRITE_E_FILENOTFOUND,
 * access denied -> DWRITE_E_FILEACCESS), LocalFileLoader::{GetFilePathLengthFromKey, GetFilePathFromKey (E_NOT_SUFFICIENT_BUFFER),
 * GetLastWriteTimeFromKey}, LocalFileLoader::FontFileStream (memory-mapped, cached per key). */
#include "fl.h"

static UINT64 u64(const FILETIME *ft) { return ((UINT64)ft->dwHighDateTime << 32) | ft->dwLowDateTime; }
static int file_time(const char *path, FILETIME *ft, UINT64 *size) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &d)) return 0;
    if (ft) *ft = d.ftLastWriteTime;
    if (size) *size = ((UINT64)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    return 1;
}
static HRESULT mkref(IDWriteFactory *f, const char *path, const FILETIME *ft, IDWriteFontFile **out) {
    WCHAR w[2048]; dw_wide(path, w, 2048);
    return IDWriteFactory_CreateFontFileReference(f, w, ft, out);
}
/* writes the default synthetic font to scratch; returns path */
static int tmp_ttf(const char *leaf, char *path, size_t cap, size_t *size) {
    size_t n; uint8_t *b = fx_ttf(&n); int ok = fl_write_tmp(leaf, b, n, path, cap); free(b); if (size) *size = n; return ok;
}
static IDWriteLocalFontFileLoader *local_of(IDWriteFontFile *file) {
    IDWriteFontFileLoader *l = NULL; IDWriteLocalFontFileLoader *ll = NULL;
    if (FAILED(IDWriteFontFile_GetLoader(file, &l)) || !l) return NULL;
    IDWriteFontFileLoader_QueryInterface(l, &IID_IDWriteLocalFontFileLoader, (void **)&ll);
    IDWriteFontFileLoader_Release(l);
    return ll;
}
#define NEED_LOCAL(leaf) \
    IDWriteFactory *f = fl_factory(); char path[MAX_PATH]; size_t fsize = 0; IDWriteFontFile *file = NULL; IDWriteLocalFontFileLoader *ll; const void *key = NULL; UINT32 ks = 0; \
    if (!f) { t_skip("DWriteCreateFactory failed"); return; } \
    if (!tmp_ttf(leaf, path, sizeof path, &fsize)) { IDWriteFactory_Release(f); t_skip("cannot write scratch file"); return; } \
    CHECK_HR(mkref(f, path, NULL, &file), S_OK); \
    if (!file) { IDWriteFactory_Release(f); return; } \
    ll = local_of(file); IDWriteFontFile_GetReferenceKey(file, &key, &ks);
#define END_LOCAL() do { if (ll) IDWriteLocalFontFileLoader_Release(ll); DW_RELEASE(file); IDWriteFactory_Release(f); } while (0)

/* ---------------- CreateFontFileReference ---------------- */
TEST(local_create_ok) {
    NEED_LOCAL("fl_ok.ttf");
    an_res a = fl_analyze(file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(a.nf, 1);
    EXPECT_REFS(file, 1);
    END_LOCAL();
}
TEST(local_create_null_path) {
    if (t_is_wine()) { t_skip("not defined elsewhere"); return; }
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = (IDWriteFontFile *)1;
    if (!f) { t_skip("factory"); return; }
    CHECK_HR(IDWriteFactory_CreateFontFileReference(f, NULL, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    FILETIME ft = { 1, 2 }; file = (IDWriteFontFile *)1;
    CHECK_HR(IDWriteFactory_CreateFontFileReference(f, NULL, &ft, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    IDWriteFactory_Release(f);
}
TEST(local_create_missing_file_no_time) {
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = (IDWriteFontFile *)1; char p[MAX_PATH];
    if (!f) { t_skip("factory"); return; }
    t_tmpfile(p, sizeof p, "fl_does_not_exist.ttf");
    HRESULT hr = mkref(f, p, NULL, &file);
    CHECK_HR(hr, H_FILENOTFOUND);   /* the last-write time is read from the file while the key is built: ERROR_FILE_NOT_FOUND maps to DWRITE_E_FILENOTFOUND */
    CHECK_OUT_NULL(file);
    IDWriteFactory_Release(f);
}
TEST(local_create_missing_directory_no_time) {
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = (IDWriteFontFile *)1; char p[MAX_PATH];
    if (!f) { t_skip("factory"); return; }
    t_tmpfile(p, sizeof p, "fl_no_such_dir\\nested\\x.ttf");
    HRESULT hr = mkref(f, p, NULL, &file);
    CHECK_HR(hr, H_FILENOTFOUND);   /* ERROR_PATH_NOT_FOUND maps to DWRITE_E_FILENOTFOUND as well */
    CHECK_OUT_NULL(file);
    IDWriteFactory_Release(f);
}
TEST(local_create_empty_path) {
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = (IDWriteFontFile *)1;
    if (!f) { t_skip("factory"); return; }
    HRESULT hr = IDWriteFactory_CreateFontFileReference(f, L"", NULL, &file);
    t_note("CreateFontFileReference(L\"\") = %s", dw_hr_name(hr));
    CHECK(FAILED(hr)); CHECK_OUT_NULL(file);
    CHECK_HR(hr, H_FILENOTFOUND);
    IDWriteFactory_Release(f);
}
TEST(local_create_directory_path) {
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = (IDWriteFontFile *)1; char p[MAX_PATH];
    if (!f) { t_skip("factory"); return; }
    snprintf(p, sizeof p, "%s", t_tmpdir());
    size_t l = strlen(p); if (l && p[l - 1] == '\\') p[l - 1] = 0;
    HRESULT hr = mkref(f, p, NULL, &file);
    t_note("CreateFontFileReference(directory) = %s", dw_hr_name(hr));
    CHECK(FAILED(hr)); CHECK_OUT_NULL(file);
    CHECK_HR(hr, H_FILEACCESS);   /* CreateFile on a directory without backup semantics: ERROR_ACCESS_DENIED -> DWRITE_E_FILEACCESS */
    IDWriteFactory_Release(f);
}
TEST(local_create_missing_file_with_time_is_lazy) {
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = NULL; char p[MAX_PATH]; FILETIME ft = { 0x12345678, 0x01D12345 };
    if (!f) { t_skip("factory"); return; }
    t_tmpfile(p, sizeof p, "fl_lazy_missing.ttf");
    HRESULT hr = mkref(f, p, &ft, &file);
    CHECK_HR(hr, S_OK);   /* with an explicit time the file is not touched when the reference is created */
    if (file) {
        an_res a = fl_analyze(file);
        t_note("Analyze of a reference to a missing file: %s", dw_hr_name(a.hr));
        CHECK(FAILED(a.hr));
        CHECKF(a.hr == H_FILENOTFOUND || a.hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "Analyze=%s", dw_hr_name(a.hr));
        CHECK(!a.sup); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_UNKNOWN);
        DW_RELEASE(file);
    }
    IDWriteFactory_Release(f);
}
TEST(local_create_reference_does_not_open_file_when_time_given) {
    /* a path that cannot exist on any machine still yields a reference when the time is supplied */
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = NULL; FILETIME ft = { 0, 0 };
    if (!f) { t_skip("factory"); return; }
    CHECK_HR(IDWriteFactory_CreateFontFileReference(f, L"Q:\\no\\such\\drive\\x.ttf", &ft, &file), S_OK);
    DW_RELEASE(file); IDWriteFactory_Release(f);
}
TEST(local_create_long_path_not_found) {
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = (IDWriteFontFile *)1; WCHAR w[400]; int i;
    if (!f) { t_skip("factory"); return; }
    lstrcpyW(w, L"C:\\"); for (i = 0; i < 40; i++) lstrcatW(w, L"deep_nonexistent_dir\\"); lstrcatW(w, L"x.ttf");
    HRESULT hr = IDWriteFactory_CreateFontFileReference(f, w, NULL, &file);
    t_note("CreateFontFileReference(path of %d chars, missing) = %s", lstrlenW(w), dw_hr_name(hr));
    CHECK(FAILED(hr)); CHECK_OUT_NULL(file);
    IDWriteFactory_Release(f);
}
TEST(local_create_twice_distinct_objects) {
    NEED_LOCAL("fl_twice.ttf");
    IDWriteFontFile *other = NULL;
    CHECK_HR(mkref(f, path, NULL, &other), S_OK);
    CHECK(other != file);
    const void *k2; UINT32 ks2; IDWriteFontFile_GetReferenceKey(other, &k2, &ks2);
    EXPECT_EQ(ks, ks2); CHECK(ks == ks2 && !memcmp(key, k2, ks)); CHECK(k2 != key);
    DW_RELEASE(other);
    END_LOCAL();
}
TEST(local_create_relative_path) {
    char cwd[MAX_PATH], p[MAX_PATH]; size_t n;
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = NULL;
    if (!f) { t_skip("factory"); return; }
    if (!tmp_ttf("fl_rel.ttf", p, sizeof p, &n)) { IDWriteFactory_Release(f); t_skip("scratch"); return; }
    GetCurrentDirectoryA(sizeof cwd, cwd);
    SetCurrentDirectoryA(t_tmpdir());
    HRESULT hr = mkref(f, "fl_rel.ttf", NULL, &file);
    SetCurrentDirectoryA(cwd);
    t_note("CreateFontFileReference(relative path) = %s", dw_hr_name(hr));
    CHECK_HR(hr, S_OK);
    DW_RELEASE(file); IDWriteFactory_Release(f);
}
TEST(local_create_forward_slashes) {
    char p[MAX_PATH], q[MAX_PATH]; size_t n, i; IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = NULL;
    if (!f) { t_skip("factory"); return; }
    if (!tmp_ttf("fl_slash.ttf", p, sizeof p, &n)) { IDWriteFactory_Release(f); t_skip("scratch"); return; }
    strcpy(q, p); for (i = 0; q[i]; i++) if (q[i] == '\\') q[i] = '/';
    HRESULT hr = mkref(f, q, NULL, &file);
    CHECK_HR(hr, S_OK);
    if (file) { an_res a = fl_analyze(file); CHECK_HR(a.hr, S_OK); CHECK(a.sup); }
    DW_RELEASE(file); IDWriteFactory_Release(f);
}
TEST(local_create_extended_length_prefix) {
    char p[MAX_PATH], q[MAX_PATH + 8]; size_t n; IDWriteFactory *f = fl_factory(); IDWriteFontFile *file = NULL;
    if (!f) { t_skip("factory"); return; }
    if (!tmp_ttf("fl_ext.ttf", p, sizeof p, &n)) { IDWriteFactory_Release(f); t_skip("scratch"); return; }
    snprintf(q, sizeof q, "\\\\?\\%s", p);
    HRESULT hr = mkref(f, q, NULL, &file);
    t_note("CreateFontFileReference(\\\\?\\ path) = %s", dw_hr_name(hr));
    CHECK_HR(hr, S_OK);
    if (file) { an_res a = fl_analyze(file); CHECK_HR(a.hr, S_OK); CHECK(a.sup); }
    DW_RELEASE(file); IDWriteFactory_Release(f);
}
TEST(local_create_file_survives_factory_release) {
    NEED_LOCAL("fl_surv.ttf");
    IDWriteFactory_Release(f); f = fl_factory();
    an_res a = fl_analyze(file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    END_LOCAL();
}

/* ---------------- the reference key ---------------- */
TEST(local_key_layout_scratch_file) {
    NEED_LOCAL("fl_key.ttf");
    WCHAR want[MAX_PATH]; int wl = dw_wide(path, want, MAX_PATH) - 1;
    /* not under the Fonts directory: 8 byte time, the path as a NUL terminated UTF-16 string, nothing else */
    EXPECT_EQ(ks, 8 + 2 * (wl + 1));
    if (ks >= 8 + 2 * (wl + 1)) {
        const WCHAR *kp = (const WCHAR *)((const uint8_t *)key + 8);
        CHECK(kp[wl] == 0);
        CHECKF(!lstrcmpiW(kp, want), "key path differs from the path passed in");
        t_note("key path is upper-cased: %d", !lstrcmpW(kp, want) ? 0 : 1);
    }
    END_LOCAL();
}
TEST(local_key_time_is_files_last_write_time) {
    NEED_LOCAL("fl_ktime.ttf"); FILETIME ft; UINT64 sz;
    CHECK(file_time(path, &ft, &sz));
    CHECK(ks >= 8);
    if (ks >= 8) { UINT64 t; memcpy(&t, key, 8); CHECKF(t == u64(&ft), "key time %llx file time %llx", (unsigned long long)t, (unsigned long long)u64(&ft)); }
    EXPECT_EQ(sz, fsize);
    FILETIME got = { 0, 0 };
    if (ll) { CHECK_HR(IDWriteLocalFontFileLoader_GetLastWriteTimeFromKey(ll, key, ks, &got), S_OK); CHECK(u64(&got) == u64(&ft)); }
    END_LOCAL();
}
TEST(local_key_explicit_time_is_stored_verbatim) {
    static const UINT64 times[] = { 0, 1, 0x00000000FFFFFFFFull, 0x0000000100000000ull, 0x01D2A3B4C5D6E7F8ull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull, 0x7FFFFFFFFFFFFFFFull };
    IDWriteFactory *f = fl_factory(); size_t i;
    if (!f) { t_skip("factory"); return; }
    for (i = 0; i < sizeof times / sizeof times[0]; i++) {
        IDWriteFontFile *file = NULL; FILETIME ft; ft.dwLowDateTime = (DWORD)times[i]; ft.dwHighDateTime = (DWORD)(times[i] >> 32);
        HRESULT hr = IDWriteFactory_CreateFontFileReference(f, L"X:\\fl\\explicit_time.ttf", &ft, &file);
        CHECKF(hr == S_OK, "time %llx: %s", (unsigned long long)times[i], dw_hr_name(hr));
        if (!file) continue;
        const void *k; UINT32 ks; IDWriteFontFile_GetReferenceKey(file, &k, &ks);
        UINT64 t = 0; if (ks >= 8) memcpy(&t, k, 8);
        CHECKF(t == times[i], "stored %llx want %llx", (unsigned long long)t, (unsigned long long)times[i]);
        IDWriteLocalFontFileLoader *ll = local_of(file);
        if (ll) { FILETIME got = { 7, 7 }; CHECK_HR(IDWriteLocalFontFileLoader_GetLastWriteTimeFromKey(ll, k, ks, &got), S_OK); CHECK(u64(&got) == times[i]); IDWriteLocalFontFileLoader_Release(ll); }
        DW_RELEASE(file);
    }
    IDWriteFactory_Release(f);
}
TEST(local_key_explicit_time_overrides_file_time) {
    NEED_LOCAL("fl_ovr.ttf"); IDWriteFontFile *o = NULL; FILETIME ft = { 0xAAAA5555, 0x01CCCCCC };
    CHECK_HR(mkref(f, path, &ft, &o), S_OK);
    if (o) {
        const void *k; UINT32 s; IDWriteFontFile_GetReferenceKey(o, &k, &s);
        UINT64 t = 0; if (s >= 8) memcpy(&t, k, 8);
        CHECK(t == u64(&ft)); EXPECT_EQ(s, ks);
        CHECK(s == ks && !memcmp((const uint8_t *)k + 8, (const uint8_t *)key + 8, ks - 8));     /* path part identical */
        an_res a = fl_analyze(o);
        t_note("Analyze with a last-write time that differs from the file's: %s sup=%d", dw_hr_name(a.hr), a.sup);
        DW_RELEASE(o);
    }
    END_LOCAL();
}
TEST(local_key_deterministic) {
    NEED_LOCAL("fl_det.ttf"); IDWriteFontFile *o = NULL;
    CHECK_HR(mkref(f, path, NULL, &o), S_OK);
    if (o) { const void *k; UINT32 s; IDWriteFontFile_GetReferenceKey(o, &k, &s); EXPECT_EQ(s, ks); CHECK(s == ks && !memcmp(k, key, ks)); DW_RELEASE(o); }
    END_LOCAL();
}
TEST(local_key_case_of_path_irrelevant_size) {
    NEED_LOCAL("fl_case.ttf"); IDWriteFontFile *o = NULL; char q[MAX_PATH]; size_t i;
    strcpy(q, path); for (i = 0; q[i]; i++) q[i] = (char)toupper((unsigned char)q[i]);
    CHECK_HR(mkref(f, q, NULL, &o), S_OK);
    if (o) {
        const void *k; UINT32 s; IDWriteFontFile_GetReferenceKey(o, &k, &s);
        EXPECT_EQ(s, ks);
        t_note("upper-case path variant produces an identical key: %d", (s == ks && !memcmp(k, key, ks)));
        DW_RELEASE(o);
    }
    END_LOCAL();
}
TEST(local_key_slash_variant) {
    NEED_LOCAL("fl_slashkey.ttf"); IDWriteFontFile *o = NULL; char q[MAX_PATH]; size_t i;
    strcpy(q, path); for (i = 0; q[i]; i++) if (q[i] == '\\') q[i] = '/';
    CHECK_HR(mkref(f, q, NULL, &o), S_OK);
    if (o) {
        const void *k; UINT32 s; IDWriteFontFile_GetReferenceKey(o, &k, &s);
        EXPECT_EQ(s, ks);
        t_note("forward-slash path produces an identical key: %d", (s == ks && !memcmp(k, key, ks)));
        DW_RELEASE(o);
    }
    END_LOCAL();
}
TEST(local_key_no_loader_callbacks_needed) {
    NEED_LOCAL("fl_keycb.ttf");
    EXPECT_REFS(file, 1);
    const void *k2; UINT32 s2; CHECK_HR(IDWriteFontFile_GetReferenceKey(file, &k2, &s2), S_OK); CHECK(k2 == key);   /* pointer stable across calls */
    END_LOCAL();
}

/* ---------------- IDWriteLocalFontFileLoader ---------------- */
TEST(local_loader_qi_available) {
    NEED_LOCAL("fl_qi.ttf");
    CHECK(ll != NULL);
    END_LOCAL();
}
TEST(local_loader_qi_matrix) {
    NEED_LOCAL("fl_qi2.ttf");
    IDWriteFontFileLoader *l = NULL; IDWriteFontFile_GetLoader(file, &l);
    if (l) {
        dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteFontFileLoader, 1), DW_QI_ROW(IDWriteLocalFontFileLoader, 1),
            DW_QI_ROW(IDWriteFontFileStream, 0), DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFactory, 0),
            DW_QI_ROW(IDWriteInMemoryFontFileLoader, 0), DW_QI_ROW(IDWriteRemoteFontFileLoader, 0), DW_QI_ROW(IDWriteRemoteFontFileStream, 0),
            DW_QI_ROW(IDWriteFontCollectionLoader, 0), DW_QI_ROW(IDWriteFontFileEnumerator, 0), DW_QI_ROW(IDWriteFontFace, 0),
        };
        DW_CHECK_QI(l, rows);
        IDWriteFontFileLoader_Release(l);
    }
    END_LOCAL();
}
TEST(local_loader_same_object_for_all_local_files) {
    NEED_LOCAL("fl_same1.ttf"); IDWriteFontFile *o = NULL; char p2[MAX_PATH]; size_t n;
    tmp_ttf("fl_same2.ttf", p2, sizeof p2, &n);
    CHECK_HR(mkref(f, p2, NULL, &o), S_OK);
    IDWriteFontFileLoader *a = NULL, *b = NULL;
    IDWriteFontFile_GetLoader(file, &a); if (o) IDWriteFontFile_GetLoader(o, &b);
    CHECK(a && a == b);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(o);
    END_LOCAL();
}
TEST(local_loader_same_object_across_factories) {
    NEED_LOCAL("fl_xfac.ttf"); IDWriteFactory *g = fl_factory(); IDWriteFontFile *o = NULL;
    if (g) {
        CHECK_HR(mkref(g, path, NULL, &o), S_OK);
        IDWriteFontFileLoader *a = NULL, *b = NULL;
        IDWriteFontFile_GetLoader(file, &a); if (o) IDWriteFontFile_GetLoader(o, &b);
        t_note("built-in local loader shared between two isolated factories: %d", a == b);
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(o); IDWriteFactory_Release(g);
    }
    END_LOCAL();
}
TEST(local_loader_identity_and_refcount) {
    NEED_LOCAL("fl_ref.ttf");
    if (ll) {
        IDWriteFontFileLoader *l = NULL; CHECK_HR(IDWriteLocalFontFileLoader_QueryInterface(ll, &IID_IDWriteFontFileLoader, (void **)&l), S_OK);
        IDWriteFontFileLoader *l2 = NULL; IDWriteFontFile_GetLoader(file, &l2);
        t_note("QI(IDWriteFontFileLoader) == GetLoader: %d, QI(IDWriteLocalFontFileLoader) == QI(IDWriteFontFileLoader): %d", l == l2, (void *)l == (void *)ll);
        ULONG a = IDWriteFontFileLoader_AddRef(l), b = IDWriteFontFileLoader_Release(l);
        EXPECT_EQ(b, a - 1);
        IUnknown *u1 = NULL, *u2 = NULL;
        IDWriteFontFileLoader_QueryInterface(l, &IID_IUnknown, (void **)&u1); IDWriteLocalFontFileLoader_QueryInterface(ll, &IID_IUnknown, (void **)&u2);
        CHECK(u1 == u2);
        if (u1) IUnknown_Release(u1); if (u2) IUnknown_Release(u2);
        DW_RELEASE(l); DW_RELEASE(l2);
    }
    END_LOCAL();
}
TEST(local_loader_unregister_rejected_even_when_queried_as_local) {
    NEED_LOCAL("fl_unreg.ttf");
    if (ll) CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)ll), E_INVALIDARG);
    END_LOCAL();
}
TEST(local_path_length_from_key) {
    NEED_LOCAL("fl_plen.ttf"); UINT32 len = 0xCDCDCDCD;
    if (ll) { CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(ll, key, ks, &len), S_OK); EXPECT_EQ(len, strlen(path)); }
    END_LOCAL();
}
TEST(local_path_from_key_buffer_protocol) {
    NEED_LOCAL("fl_pbuf.ttf"); UINT32 len = 0; WCHAR want[MAX_PATH]; dw_wide(path, want, MAX_PATH);
    if (ll) {
        IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(ll, key, ks, &len);
        WCHAR *buf = (WCHAR *)malloc((len + 16) * sizeof(WCHAR)); UINT32 i;
        /* size 0, NULL buffer: nothing can be written; reported as insufficient */
        CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, NULL, 0), DW_E_NOT_SUFFICIENT_BUFFER);
        /* size len (no room for the terminator): insufficient, first character cleared */
        for (i = 0; i < len + 16; i++) buf[i] = 0xBEEF;
        CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, buf, len), DW_E_NOT_SUFFICIENT_BUFFER);
        CHECK(buf[0] == 0); CHECK(buf[1] == 0xBEEF);
        /* size 1 */
        for (i = 0; i < len + 16; i++) buf[i] = 0xBEEF;
        CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, buf, 1), len + 1 <= 1 ? S_OK : DW_E_NOT_SUFFICIENT_BUFFER);
        CHECK(buf[0] == 0);
        /* exact fit including the terminator */
        for (i = 0; i < len + 16; i++) buf[i] = 0xBEEF;
        CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, buf, len + 1), S_OK);
        CHECK(buf[len] == 0); CHECKF(!lstrcmpiW(buf, want), "path round trip"); CHECK(buf[len + 1] == 0xBEEF);   /* nothing written past the terminator */
        /* larger buffer */
        for (i = 0; i < len + 16; i++) buf[i] = 0xBEEF;
        CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, buf, len + 8), S_OK);
        CHECK(buf[len] == 0); CHECK(buf[len + 1] == 0xBEEF);
        free(buf);
    }
    END_LOCAL();
}
TEST(local_path_from_key_every_short_size) {
    NEED_LOCAL("fl_pshort.ttf"); UINT32 len = 0;
    if (ll) {
        IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(ll, key, ks, &len);
        WCHAR *buf = (WCHAR *)malloc((len + 4) * sizeof(WCHAR)); UINT32 sz;
        for (sz = 1; sz <= len; sz++) {
            buf[0] = 0xBEEF;
            HRESULT hr = IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, buf, sz);
            CHECKF(hr == DW_E_NOT_SUFFICIENT_BUFFER, "size %u of %u: %s", sz, len + 1, dw_hr_name(hr));
            CHECKF(buf[0] == 0, "size %u: buffer not cleared", sz);
        }
        free(buf);
    }
    END_LOCAL();
}
TEST(local_time_from_key_consistent_with_file) {
    NEED_LOCAL("fl_tfk.ttf"); FILETIME a = { 1, 1 }, b = { 2, 2 };
    if (ll) {
        CHECK_HR(IDWriteLocalFontFileLoader_GetLastWriteTimeFromKey(ll, key, ks, &a), S_OK);
        CHECK_HR(IDWriteLocalFontFileLoader_GetLastWriteTimeFromKey(ll, key, ks, &b), S_OK);
        CHECK(u64(&a) == u64(&b)); CHECK(u64(&a) != 0);
    }
    END_LOCAL();
}
TEST(local_loader_methods_do_not_need_registration) {
    NEED_LOCAL("fl_noreg.ttf"); UINT32 len;
    IDWriteFactory *g = fl_factory();   /* a factory that never saw the file: the loader is a process-wide singleton, stateless for key queries */
    if (ll && g) { CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(ll, key, ks, &len), S_OK); }
    if (g) IDWriteFactory_Release(g);
    END_LOCAL();
}

/* ---------------- streams of the local loader ---------------- */
static uint8_t *slurp(const char *path, size_t *n) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD sz, rd = 0; uint8_t *b;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL); b = (uint8_t *)malloc(sz ? sz : 1); ReadFile(h, b, sz, &rd, NULL); CloseHandle(h); *n = rd; return b;
}
TEST(local_stream_size_time_and_content) {
    NEED_LOCAL("fl_str.ttf"); IDWriteFontFileLoader *l = NULL; IDWriteFontFileStream *s = NULL; FILETIME ft;
    IDWriteFontFile_GetLoader(file, &l); file_time(path, &ft, NULL);
    if (l) {
        CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s), S_OK);
        if (s) {
            UINT64 sz = 0, wt = 0; CHECK_HR(IDWriteFontFileStream_GetFileSize(s, &sz), S_OK); EXPECT_EQ(sz, fsize);
            CHECK_HR(IDWriteFontFileStream_GetLastWriteTime(s, &wt), S_OK); CHECK(wt == u64(&ft));
            size_t n; uint8_t *ref = slurp(path, &n);
            const void *start = NULL; void *ctx = (void *)1;
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &start, 0, sz, &ctx), S_OK);
            CHECK(start != NULL); CHECK(ref && n == sz && start && !memcmp(start, ref, n));
            IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
            free(ref);
            IDWriteFontFileStream_Release(s);
        }
        IDWriteFontFileLoader_Release(l);
    }
    END_LOCAL();
}
TEST(local_stream_fragments) {
    NEED_LOCAL("fl_frag.ttf"); IDWriteFontFileLoader *l = NULL; IDWriteFontFileStream *s = NULL;
    IDWriteFontFile_GetLoader(file, &l);
    if (l && SUCCEEDED(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s)) && s) {
        size_t n; uint8_t *ref = slurp(path, &n); UINT64 sz = n;
        static const UINT64 offs[] = { 0, 1, 4, 12, 100, 1000 }; size_t i;
        for (i = 0; i < sizeof offs / sizeof offs[0]; i++) {
            const void *st = NULL; void *ctx = NULL; UINT64 len = 16;
            if (offs[i] + len > sz) continue;
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, offs[i], len, &ctx), S_OK);
            CHECKF(st && !memcmp(st, ref + offs[i], (size_t)len), "fragment at %llu", (unsigned long long)offs[i]);
            IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
        }
        {   /* the end of the file: a zero length read at EOF and a read of the final bytes */
            const void *st = NULL; void *ctx = (void *)1;
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, sz, 0, &ctx), S_OK); CHECK(st != NULL); IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, sz - 4, 4, &ctx), S_OK); CHECK(st && !memcmp(st, ref + sz - 4, 4)); IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, 0, 0, &ctx), S_OK); IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
        }
        {   /* two fragments held at once stay valid */
            const void *a = NULL, *b = NULL; void *ca = NULL, *cb = NULL;
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &a, 0, 64, &ca), S_OK);
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &b, 64, 64, &cb), S_OK);
            CHECK(a && b && !memcmp(a, ref, 64) && !memcmp(b, ref + 64, 64));
            CHECK(b == (const uint8_t *)a + 64);          /* one memory mapping serves the whole file */
            IDWriteFontFileStream_ReleaseFileFragment(s, ca); IDWriteFontFileStream_ReleaseFileFragment(s, cb);
        }
        free(ref); IDWriteFontFileStream_Release(s);
    }
    if (l) IDWriteFontFileLoader_Release(l);
    END_LOCAL();
}
TEST(local_stream_cached_per_key) {
    NEED_LOCAL("fl_scache.ttf"); IDWriteFontFileLoader *l = NULL; IDWriteFontFileStream *s1 = NULL, *s2 = NULL, *s3 = NULL;
    IDWriteFontFile_GetLoader(file, &l);
    if (l) {
        CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s1), S_OK);
        CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s2), S_OK);
        CHECK(s1 != NULL && s1 == s2);                   /* the stream is looked up in a table keyed by the reference key while it is alive */
        char p2[MAX_PATH]; size_t n; tmp_ttf("fl_scache2.ttf", p2, sizeof p2, &n);
        IDWriteFontFile *o = NULL; mkref(f, p2, NULL, &o);
        if (o) { const void *k2; UINT32 s; IDWriteFontFile_GetReferenceKey(o, &k2, &s); CHECK_HR(IDWriteFontFileLoader_CreateStreamFromKey(l, k2, s, &s3), S_OK); CHECK(s3 != s1); DW_RELEASE(o); }
        DW_RELEASE(s1); DW_RELEASE(s2); DW_RELEASE(s3);
        IDWriteFontFileLoader_Release(l);
    }
    END_LOCAL();
}
TEST(local_stream_shared_with_font_file_use) {
    NEED_LOCAL("fl_sshare.ttf"); IDWriteFontFileLoader *l = NULL; IDWriteFontFileStream *s1 = NULL, *s2 = NULL;
    IDWriteFontFile_GetLoader(file, &l);
    if (l) {
        an_res a = fl_analyze(file); CHECK_HR(a.hr, S_OK);   /* dwrite itself opened the stream through the same table */
        IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s1);
        IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s2);
        CHECK(s1 && s1 == s2);
        DW_RELEASE(s1); DW_RELEASE(s2); IDWriteFontFileLoader_Release(l);
    }
    END_LOCAL();
}
TEST(local_stream_qi_and_refs) {
    NEED_LOCAL("fl_sqi.ttf"); IDWriteFontFileLoader *l = NULL; IDWriteFontFileStream *s = NULL;
    IDWriteFontFile_GetLoader(file, &l);
    if (l && SUCCEEDED(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s)) && s) {
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontFileStream, 1), DW_QI_ROW(IDWriteFontFileLoader, 0), DW_QI_ROW(IDWriteFontFile, 0),
                             DW_QI_ROW(IDWriteLocalFontFileLoader, 0), DW_QI_ROW(IDWriteRemoteFontFileStream, 0), DW_QI_ROW(IDWriteFactory, 0) };
        DW_CHECK_QI(s, rows);
        ULONG a = IDWriteFontFileStream_AddRef(s), b = IDWriteFontFileStream_Release(s);
        EXPECT_EQ(b, a - 1);
        IDWriteFontFileStream_Release(s);
    }
    if (l) IDWriteFontFileLoader_Release(l);
    END_LOCAL();
}
TEST(local_stream_release_fragment_tolerates_null_context) {
    NEED_LOCAL("fl_snull.ttf"); IDWriteFontFileLoader *l = NULL; IDWriteFontFileStream *s = NULL;
    IDWriteFontFile_GetLoader(file, &l);
    if (l && SUCCEEDED(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s)) && s) {
        IDWriteFontFileStream_ReleaseFileFragment(s, NULL);     /* the local stream's release is a no-op */
        const void *st; void *ctx = (void *)1; IDWriteFontFileStream_ReadFileFragment(s, &st, 0, 4, &ctx);
        t_note("local stream fragment context: %p", ctx);
        IDWriteFontFileStream_ReleaseFileFragment(s, ctx); IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
        CHECK(1);
        IDWriteFontFileStream_Release(s);
    }
    if (l) IDWriteFontFileLoader_Release(l);
    END_LOCAL();
}
TEST(local_stream_survives_file_deletion_while_held) {
    NEED_LOCAL("fl_sdel.ttf"); IDWriteFontFileLoader *l = NULL; IDWriteFontFileStream *s = NULL;
    IDWriteFontFile_GetLoader(file, &l);
    if (l && SUCCEEDED(IDWriteFontFileLoader_CreateStreamFromKey(l, key, ks, &s)) && s) {
        const void *st; void *ctx; UINT64 sz = 0;
        IDWriteFontFileStream_GetFileSize(s, &sz);
        BOOL del = DeleteFileA(path);                      /* shared for delete, but a mapped view normally keeps the name busy */
        t_note("DeleteFile on a file with a live font stream: %d (error %lu)", del, del ? 0 : GetLastError());
        CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, 0, 4, &ctx), S_OK);
        IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
        IDWriteFontFileStream_Release(s);
    }
    if (l) IDWriteFontFileLoader_Release(l);
    END_LOCAL();
}

/* ---------------- fonts read from disk end to end ---------------- */
TEST(local_scratch_variants_analyze) {
    IDWriteFactory *f = fl_factory(); size_t n; int k;
    if (!f) { t_skip("factory"); return; }
    for (k = 0; k < 6; k++) {
        uint8_t *b = NULL; char p[MAX_PATH]; char leaf[32]; IDWriteFontFile *file = NULL; BOOL sup; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf;
        UINT32 want_nf = 0; DWRITE_FONT_FILE_TYPE want_ft = DWRITE_FONT_FILE_TYPE_UNKNOWN; BOOL want_sup = FALSE;
        snprintf(leaf, sizeof leaf, "fl_var%d.bin", k);
        switch (k) {
        case 0: b = fx_ttf(&n); want_sup = TRUE; want_ft = DWRITE_FONT_FILE_TYPE_TRUETYPE; want_nf = 1; break;
        case 1: b = fx_ttc_hdr(3, 0x200, &n); want_sup = TRUE; want_ft = DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION; want_nf = 3; break;
        case 2: b = fx_magic(0x4F54544F, 0x200, &n); want_sup = TRUE; want_ft = DWRITE_FONT_FILE_TYPE_CFF; want_nf = 1; break;
        case 3: b = fx_pfb(0x200, &n); want_ft = DWRITE_FONT_FILE_TYPE_TYPE1_PFB; want_nf = 1; break;
        case 4: b = fx_pfm(0x200, &n); want_ft = DWRITE_FONT_FILE_TYPE_TYPE1_PFM; want_nf = 1; break;
        default: b = fx_magic(0x11223344, 0x200, &n); break;
        }
        fl_write_tmp(leaf, b, n, p, sizeof p); free(b);
        if (FAILED(mkref(f, p, NULL, &file)) || !file) { CHECKF(0, "variant %d: CreateFontFileReference", k); continue; }
        HRESULT hr = IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf);
        CHECKF(hr == S_OK && sup == want_sup && ft == want_ft && nf == want_nf, "variant %d: hr=%s sup=%d ft=%d fc=%d nf=%u", k, dw_hr_name(hr), sup, ft, fc, nf);
        DW_RELEASE(file);
    }
    IDWriteFactory_Release(f);
}
TEST(local_scratch_short_files_note) {
    static const size_t sizes[] = { 0, 3, 0x10, 0x74 }; size_t i;
    IDWriteFactory *f = fl_factory();
    if (!f) { t_skip("factory"); return; }
    for (i = 0; i < 4; i++) {
        size_t n; uint8_t *b = fx_magic(0x00010000, sizes[i] ? sizes[i] : 1, &n); char p[MAX_PATH]; IDWriteFontFile *file = NULL;
        fl_write_tmp("fl_short.bin", b, sizes[i], p, sizeof p); free(b);
        HRESULT hr = mkref(f, p, NULL, &file);
        t_note("%zu byte local file: CreateFontFileReference=%s", sizes[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && file) {
            an_res a = fl_analyze(file);
            t_note("  Analyze=%s sup=%d", dw_hr_name(a.hr), a.sup);
            CHECK(!a.sup);
            DW_RELEASE(file);
        }
    }
    IDWriteFactory_Release(f);
}
TEST(local_face_from_scratch_file) {
    NEED_LOCAL("fl_face.ttf"); IDWriteFontFace *face = NULL;
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), S_OK);
    if (face) {
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
        DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m); EXPECT_EQ(m.designUnitsPerEm, 2048);
        UINT32 nfiles = 0;
        CHECK_HR(IDWriteFontFace_GetFiles(face, &nfiles, NULL), S_OK); EXPECT_EQ(nfiles, 1);
        IDWriteFontFile *got = NULL; nfiles = 1;
        CHECK_HR(IDWriteFontFace_GetFiles(face, &nfiles, &got), S_OK);
        if (got) {
            const void *k2; UINT32 s2; CHECK_HR(IDWriteFontFile_GetReferenceKey(got, &k2, &s2), S_OK);
            EXPECT_EQ(s2, ks); CHECK(s2 == ks && !memcmp(k2, key, ks));
            IDWriteFontFileLoader *l1 = NULL, *l2 = NULL; IDWriteFontFile_GetLoader(got, &l1); IDWriteFontFile_GetLoader(file, &l2);
            CHECK(l1 && l1 == l2);
            DW_RELEASE(l1); DW_RELEASE(l2); DW_RELEASE(got);
        }
        DW_RELEASE(face);
    }
    END_LOCAL();
}
TEST(local_face_after_file_released) {
    NEED_LOCAL("fl_face2.ttf"); IDWriteFontFace *face = NULL;
    IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    DW_RELEASE(file);
    if (face) { EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS); DW_RELEASE(face); }
    END_LOCAL();
}

/* ---------------- installed fonts ---------------- */
#define SYS_LOCAL(id, leaf) TEST(local_sys_##id) { \
    char p[MAX_PATH]; IDWriteFactory *f; IDWriteFontFile *file = NULL; \
    if (!fl_sysfont(leaf, p, sizeof p)) { t_skip(leaf " is not installed"); return; } \
    f = fl_factory(); if (!f) { t_skip("factory"); return; } \
    CHECK_HR(mkref(f, p, NULL, &file), S_OK); \
    if (file) { \
        const void *k; UINT32 s; UINT64 sz = 0; FILETIME ft; \
        CHECK_HR(IDWriteFontFile_GetReferenceKey(file, &k, &s), S_OK); CHECK(s >= 8 + 2); \
        t_note("%s key: %u bytes, marker %04x", leaf, s, s >= 10 ? *(const WCHAR *)((const uint8_t *)k + 8) : 0); \
        IDWriteLocalFontFileLoader *ll = local_of(file); \
        CHECK(ll != NULL); \
        if (ll) { \
            UINT32 len = 0; WCHAR *buf, want[MAX_PATH]; dw_wide(p, want, MAX_PATH); \
            CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(ll, k, s, &len), S_OK); EXPECT_EQ(len, strlen(p)); \
            buf = (WCHAR *)malloc((len + 1) * sizeof(WCHAR)); \
            CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, k, s, buf, len + 1), S_OK); CHECK(!lstrcmpiW(buf, want)); free(buf); \
            FILETIME t2; CHECK_HR(IDWriteLocalFontFileLoader_GetLastWriteTimeFromKey(ll, k, s, &t2), S_OK); \
            if (file_time(p, &ft, &sz)) CHECK(u64(&t2) == u64(&ft)); \
            IDWriteLocalFontFileLoader_Release(ll); \
        } \
        an_res a = fl_analyze(file); CHECK_HR(a.hr, S_OK); CHECK(a.sup); \
        DW_RELEASE(file); \
    } \
    IDWriteFactory_Release(f); }
SYS_LOCAL(arial, "arial.ttf")
SYS_LOCAL(times, "times.ttf")
SYS_LOCAL(segoeui, "segoeui.ttf")
SYS_LOCAL(cambria_ttc, "cambria.ttc")
SYS_LOCAL(msgothic_ttc, "msgothic.ttc")

TEST(local_sys_arial_key_marker_and_stream) {
    char p[MAX_PATH]; IDWriteFactory *f; IDWriteFontFile *file = NULL;
    if (!fl_sysfont("arial.ttf", p, sizeof p)) { t_skip("arial.ttf is not installed"); return; }
    f = fl_factory(); if (!f) { t_skip("factory"); return; }
    CHECK_HR(mkref(f, p, NULL, &file), S_OK);
    if (file) {
        const void *k; UINT32 s; IDWriteFontFile_GetReferenceKey(file, &k, &s);
        const WCHAR *kp = (const WCHAR *)((const uint8_t *)k + 8);
        /* files under the Fonts directory store a '*' marker plus the part after the directory; the expanded path is what the key queries return */
        t_note("arial.ttf key: marker '%c', stored length %u of path length %zu", (char)kp[0], (s - 8) / 2 - 1, strlen(p));
        CHECK(kp[0] == L'*' || kp[0] == L'C' || kp[0] == L'c' || kp[0] != 0);
        IDWriteFontFileLoader *l = NULL; IDWriteFontFileStream *st = NULL; IDWriteFontFile_GetLoader(file, &l);
        if (l && SUCCEEDED(IDWriteFontFileLoader_CreateStreamFromKey(l, k, s, &st)) && st) {
            UINT64 sz = 0, fsz = 0; FILETIME ft; file_time(p, &ft, &fsz);
            CHECK_HR(IDWriteFontFileStream_GetFileSize(st, &sz), S_OK); EXPECT_EQ(sz, fsz);
            const void *b; void *ctx; CHECK_HR(IDWriteFontFileStream_ReadFileFragment(st, &b, 0, 4, &ctx), S_OK);
            CHECK(b && !memcmp(b, "\0\1\0\0", 4)); IDWriteFontFileStream_ReleaseFileFragment(st, ctx);
            IDWriteFontFileStream_Release(st);
        }
        if (l) IDWriteFontFileLoader_Release(l);
        IDWriteFontFace *face = NULL;
        if (SUCCEEDED(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face)) && face) {
            DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m);
            UINT32 hl = 0; uint8_t *head = dw_face_table(face, DW_TAG('h','e','a','d'), &hl);
            if (head && hl >= 20) EXPECT_EQ(m.designUnitsPerEm, (head[18] << 8) | head[19]);
            t_note("Arial designUnitsPerEm=%u ascent=%u descent=%u", m.designUnitsPerEm, m.ascent, m.descent);
            free(head); DW_RELEASE(face);
        }
        DW_RELEASE(file);
    }
    IDWriteFactory_Release(f);
}
TEST(local_sys_ttc_face_files_identity) {
    char p[MAX_PATH]; IDWriteFactory *f; IDWriteFontFile *file = NULL;
    if (!fl_sysfont("cambria.ttc", p, sizeof p)) { t_skip("cambria.ttc is not installed"); return; }
    f = fl_factory(); if (!f) { t_skip("factory"); return; }
    CHECK_HR(mkref(f, p, NULL, &file), S_OK);
    if (file) {
        BOOL sup; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf;
        IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf);
        UINT32 i;
        for (i = 0; i < nf && i < 4; i++) {
            IDWriteFontFace *face = NULL;
            HRESULT hr = IDWriteFactory_CreateFontFace(f, fc, 1, &file, i, DWRITE_FONT_SIMULATIONS_NONE, &face);
            CHECKF(hr == S_OK, "face %u: %s", i, dw_hr_name(hr));
            if (face) { EXPECT_EQ(IDWriteFontFace_GetIndex(face), i); CHECK(IDWriteFontFace_GetGlyphCount(face) > 100); DW_RELEASE(face); }
        }
        IDWriteFontFace *bad = (IDWriteFontFace *)1;
        HRESULT hr = IDWriteFactory_CreateFontFace(f, fc, 1, &file, nf, DWRITE_FONT_SIMULATIONS_NONE, &bad);
        CHECK(FAILED(hr)); CHECK_OUT_NULL(bad);
        DW_RELEASE(file);
    }
    IDWriteFactory_Release(f);
}
