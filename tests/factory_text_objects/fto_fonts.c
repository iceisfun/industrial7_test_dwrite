/* Factory entry points that take fonts: CreateCustomFontCollection, CreateFontFileReference, CreateCustomFontFileReference,
 * CreateFontFace and the CreateGlyphRunAnalysis overloads.
 *
 * Facts (Win10 19041 dwrite.dll):
 *   CreateCustomFontCollection: loader==NULL -> E_INVALIDARG (*collection zeroed first); the key is copied into the collection
 *     key; the loader must be registered (its id is looked up).
 *   CreateFontFileReference: path==NULL -> E_INVALIDARG. With lastWriteTime==NULL the file is opened to read its last write
 *     time (GetFileLastWriteTimeTicks), so a missing file fails here; with a non-NULL lastWriteTime nothing touches the
 *     file and any path string is accepted. The reference key is: 8 bytes of write-time ticks, a 2-byte marker when the path
 *     lies inside the Fonts directory (and the directory prefix is dropped), the NUL-terminated wide path.
 *   CreateCustomFontFileReference: key==NULL -> E_INVALIDARG; no other check at the factory.
 *   CreateFontFace: E_INVALIDARG unless faceType in {CFF, TRUETYPE, OPENTYPE_COLLECTION, UNKNOWN, RAW_CFF}, fileCount == 1,
 *     fontFiles[0] != NULL, faceIndex == 0 (unless OPENTYPE_COLLECTION) and (simulations & ~3) == 0.
 *   CreateGlyphRunAnalysis: run, run->fontFace, run->glyphIndices non-NULL, fontEmSize >= 0, NaN anywhere (pixelsPerDip, origin,
 *     transform members) rejected, pixelsPerDip > 0 for the legacy overload, rendering mode <= 6 (<= 7 for RENDERING_MODE1),
 *     measuring mode <= 2, grid fit <= 2, antialias <= 1.
 */
#include "fto.h"

#define NIL_P(p) ((void *)(p) == FTO_SENTINEL)

/* ---- CreateCustomFontCollection ------------------------------------------------------------------------------------- */
FTO_BOTH(fto_cc_null_loader) {
    IDWriteFontCollection *c = (IDWriteFontCollection *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(f, NULL, "key", 3, &c), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(c);
    c = (IDWriteFontCollection *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(f, NULL, NULL, 0, &c), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(c);
}

FTO_BOTH(fto_cc_basic_enumeration) {
    fto_coll fc;
    fto_coll_setup(&fc, f, "FtoCcFamily");
    if (!fc.coll) { t_skip("custom collection unavailable: %s", dw_hr_name(fc.hr)); fto_coll_teardown(&fc); return; }
    EXPECT_REFS(fc.coll, 1);
    CHECK(IDWriteFontCollection_GetFontFamilyCount(fc.coll) == 1);
    /* The loader was asked for an enumerator with exactly the key bytes passed to the factory. */
    CHECK(fc.cl->n_create >= 1);
    CHECK(fc.cl->last_key_size == 4);
    CHECK(!memcmp(fc.cl->last_key, "dwx", 4));
    CHECK(fc.cl->last_factory != NULL && dw_same_identity((IUnknown *)fc.cl->last_factory, (IUnknown *)f));
    if (fc.cl->last_enum) {
        CHECK(fc.cl->last_enum->n_getcurrent >= 1);
        CHECK(fc.cl->last_enum->n_movenext >= 2);
        t_note("%s: enumerator CreateEnumerator=%ld MoveNext=%ld GetCurrentFontFile=%ld", tag, fc.cl->n_create, fc.cl->last_enum->n_movenext, fc.cl->last_enum->n_getcurrent);
    }
    UINT32 idx = 99; BOOL ex = FALSE;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(fc.coll, fc.family, &idx, &ex));
    CHECK(ex && idx == 0);
    idx = 99; ex = FALSE;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(fc.coll, L"ftoCCfamily", &idx, &ex));     /* case-insensitive */
    CHECK(ex && idx == 0);
    idx = 99; ex = TRUE;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(fc.coll, L"NoSuch", &idx, &ex));
    CHECK(!ex && idx == 0xFFFFFFFFu);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(fc.coll, 0, &fam));
    if (fam) {
        IDWriteLocalizedStrings *names = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &names));
        if (names) { char b[64]; CHECK_HR_OK(dw_ls_en(names, b, sizeof b)); EXPECT_STR(b, "FtoCcFamily"); IDWriteLocalizedStrings_Release(names); }
        IDWriteFontFamily_Release(fam);
    }
    IDWriteFontFamily *none = (IDWriteFontFamily *)FTO_SENTINEL;
    /* index >= count throws IndexOutOfRangeException in the collection; *family is zeroed first. */
    HRESULT hr_oor = IDWriteFontCollection_GetFontFamily(fc.coll, 1, &none);
    CHECKF(FAILED(hr_oor), "GetFontFamily(count): %s", dw_hr_name(hr_oor));
    t_note("%s: GetFontFamily(index == count) -> %s", tag, dw_hr_name(hr_oor));
    CHECK_OUT_NULL(none);
    fto_coll_teardown(&fc);
}

FTO_BOTH(fto_cc_collection_matches_font) {
    fto_coll fc;
    fto_coll_setup(&fc, f, "FtoCcFace");
    if (!fc.coll) { t_skip("custom collection unavailable"); fto_coll_teardown(&fc); return; }
    IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(fc.coll, 0, &fam));
    if (fam) {
        CHECK(IDWriteFontFamily_GetFontCount(fam) == 1);
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font));
        if (font) {
            CHECK(IDWriteFont_GetWeight(font) == DWRITE_FONT_WEIGHT_NORMAL);
            CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &face));
            if (face) {
                CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS);
                CHECK(IDWriteFontFace_GetType(face) == DWRITE_FONT_FACE_TYPE_TRUETYPE);
                DW_RELEASE(face);
            }
            IDWriteFont_Release(font);
        }
        IDWriteFontFamily_Release(fam);
    }
    fto_coll_teardown(&fc);
}

FTO_BOTH(fto_cc_unregistered_loader_note) {
    dwx_mem_loader *fl = dwx_loader_new();
    dwx_coll_loader *cl = dwx_coll_new(fl);
    IDWriteFontCollection *c = (IDWriteFontCollection *)FTO_SENTINEL;
    /* The loader id is looked up when the collection key is built: an unregistered loader is a caller error. */
    HRESULT hr = dwx_coll_create(cl, f, &c);
    t_note("%s: CreateCustomFontCollection with an unregistered loader -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr)) { CHECK(c && !NIL_P(c)); if (c && !NIL_P(c)) { t_note("family count %u", IDWriteFontCollection_GetFontFamilyCount(c)); IDWriteFontCollection_Release(c); } }
    else CHECK_OUT_NULL(c);
    dwx_release(cl); dwx_release(fl);
}

FTO_BOTH(fto_cc_key_sizes) {
    static const UINT32 sizes[] = { 0, 1, 4, 16, 64, 1000 };
    unsigned i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ttf_spec s; size_t n = 0; uint8_t *b;
        dwx_mem_loader *fl = dwx_loader_new();
        dwx_coll_loader *cl;
        uint8_t key[1000];
        ttf_style_spec(&s, "FtoKeySize", 400, 5, 0);
        b = ttf_build(&s, &n);
        dwx_loader_add(fl, b, n); free(b);
        fl->fail_key_size_check = 0;
        dwx_loader_register(fl, f);
        cl = dwx_coll_new(fl);
        dwx_coll_add_key(cl, 0);
        dwx_coll_register(cl, f);
        memset(key, 0x5A, sizeof key);
        IDWriteFontCollection *c = (IDWriteFontCollection *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateCustomFontCollection(f, (IDWriteFontCollectionLoader *)cl, sizes[i] ? key : NULL, sizes[i], &c);
        t_note("%s: key size %u -> %s", tag, sizes[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && c && !NIL_P(c)) {
            (void)IDWriteFontCollection_GetFontFamilyCount(c);
            if (cl->n_create) {
                CHECKF(cl->last_key_size == sizes[i], "loader saw key size %u want %u", cl->last_key_size, sizes[i]);
                if (sizes[i] >= 4) CHECK(cl->last_key[0] == 0x5A);
            }
            IDWriteFontCollection_Release(c);
        } else CHECK_OUT_NULL(c);
        dwx_coll_unregister(cl); dwx_loader_unregister(fl);
        dwx_release(cl); dwx_release(fl);
    }
}

FTO_BOTH(fto_cc_key_is_copied) {
    ttf_spec s; size_t n = 0; uint8_t *b;
    dwx_mem_loader *fl = dwx_loader_new();
    dwx_coll_loader *cl;
    char key[8] = "dwx";
    IDWriteFontCollection *c = NULL;
    ttf_style_spec(&s, "FtoKeyCopy", 400, 5, 0);
    b = ttf_build(&s, &n);
    dwx_loader_add(fl, b, n); free(b);
    dwx_loader_register(fl, f);
    cl = dwx_coll_new(fl); dwx_coll_add_key(cl, 0); dwx_coll_register(cl, f);
    HRESULT hr = IDWriteFactory_CreateCustomFontCollection(f, (IDWriteFontCollectionLoader *)cl, key, 4, &c);
    CHECK_HR_OK(hr);
    memset(key, 0, sizeof key);                               /* caller's buffer goes away / changes */
    if (c) {
        CHECK(IDWriteFontCollection_GetFontFamilyCount(c) == 1);
        CHECK(!memcmp(cl->last_key, "dwx", 4));
        IDWriteFontCollection_Release(c);
    }
    dwx_coll_unregister(cl); dwx_loader_unregister(fl);
    dwx_release(cl); dwx_release(fl);
}

FTO_BOTH(fto_cc_failing_enumerator_creation_note) {
    fto_coll fc;
    ttf_spec s; size_t n = 0; uint8_t *b;
    memset(&fc, 0, sizeof fc);
    ttf_style_spec(&s, "FtoCcFail", 400, 5, 0);
    b = ttf_build(&s, &n);
    fc.fl = dwx_loader_new(); fc.key = dwx_loader_add(fc.fl, b, n); free(b);
    dwx_loader_register(fc.fl, f);
    fc.cl = dwx_coll_new(fc.fl); dwx_coll_add_key(fc.cl, fc.key);
    dwx_coll_register(fc.cl, f);
    fc.cl->fail_create = E_FAIL;           /* CreateEnumeratorFromKey fails */
    IDWriteFontCollection *c = (IDWriteFontCollection *)FTO_SENTINEL;
    HRESULT hr = dwx_coll_create(fc.cl, f, &c);
    t_note("%s: loader failing CreateEnumeratorFromKey -> creation %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && c && !NIL_P(c)) {
        t_note("%s: family count of the failed collection: %u", tag, IDWriteFontCollection_GetFontFamilyCount(c));
        IDWriteFontCollection_Release(c);
    } else CHECK_OUT_NULL(c);
    fto_coll_teardown(&fc);
}

FTO_BOTH(fto_cc_two_collections_same_key) {
    fto_coll fc;
    fto_coll_setup(&fc, f, "FtoCcTwice");
    if (!fc.coll) { t_skip("custom collection unavailable"); fto_coll_teardown(&fc); return; }
    IDWriteFontCollection *c2 = NULL;
    CHECK_HR_OK(dwx_coll_create(fc.cl, f, &c2));
    if (c2) {
        CHECK(IDWriteFontCollection_GetFontFamilyCount(c2) == 1);
        t_note("%s: same loader+key twice -> %s object", tag, c2 == fc.coll ? "the same" : "a new");
        IDWriteFontCollection_Release(c2);
    }
    fto_coll_teardown(&fc);
}

/* ---- CreateFontFileReference ---------------------------------------------------------------------------------------- */
static int write_file(const char *path, const void *data, size_t n) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w = 0; BOOL ok;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ok = WriteFile(h, data, (DWORD)n, &w, NULL);
    CloseHandle(h);
    return ok && w == n;
}
static int wide_path(const char *p, WCHAR *out, int cap) { return MultiByteToWideChar(CP_ACP, 0, p, -1, out, cap); }

FTO_BOTH(fto_ffr_null_path) {
    IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateFontFileReference(f, NULL, NULL, &ff), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(ff);
    FILETIME ft = { 1, 2 };
    ff = (IDWriteFontFile *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateFontFileReference(f, NULL, &ft, &ff), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(ff);
}

FTO_BOTH(fto_ffr_nonexistent_path_null_time_fails) {
    IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
    /* lastWriteTime == NULL: the file is opened to learn its time, so a missing file cannot produce a reference. */
    HRESULT hr = IDWriteFactory_CreateFontFileReference(f, L"C:\\no\\such\\dir\\nothing_here.ttf", NULL, &ff);
    CHECKF(FAILED(hr), "missing file with NULL lastWriteTime: %s", dw_hr_name(hr));
    t_note("%s: missing file -> %s", tag, dw_hr_name(hr));
    CHECK_OUT_NULL(ff);
    if (ff && ff != FTO_SENTINEL) IDWriteFontFile_Release(ff);
}

FTO_BOTH(fto_ffr_nonexistent_path_with_time_is_lazy) {
    IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
    FILETIME ft = { 0x11111111, 0x01D00000 };
    /* A caller supplied write time means nothing is opened at creation; failure shows when the file is used. */
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, L"C:\\no\\such\\dir\\nothing_here.ttf", &ft, &ff));
    if (ff && !NIL_P(ff)) {
        EXPECT_REFS(ff, 1);
        BOOL sup = TRUE; DWRITE_FONT_FILE_TYPE t = (DWRITE_FONT_FILE_TYPE)99; DWRITE_FONT_FACE_TYPE fct = (DWRITE_FONT_FACE_TYPE)99; UINT32 nf = 77;
        HRESULT hr = IDWriteFontFile_Analyze(ff, &sup, &t, &fct, &nf);
        t_note("%s: Analyze(missing file) -> %s supported=%d type=%d faces=%u", tag, dw_hr_name(hr), sup, t, nf);
        CHECK(FAILED(hr) || !sup);
        IDWriteFontFile_Release(ff);
    }
}

FTO_BOTH(fto_ffr_path_shapes_accepted_with_time) {
    WCHAR *longp = fto_repeat('x', 5000);
    const WCHAR *paths[] = { L"relative.ttf", L"C:\\", L"\\\\server\\share\\a.ttf", L"C:\\dir with spaces\\a b.ttf", L"\\\\?\\C:\\a.ttf",
                             L"a", L"\x4e2d\x6587.ttf", L"C:\\a\\..\\b.ttf", longp };
    FILETIME ft = { 5, 6 };
    unsigned i;
    for (i = 0; i < sizeof paths / sizeof paths[0]; i++) {
        IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateFontFileReference(f, paths[i], &ft, &ff);
        CHECKF(hr == S_OK, "path #%u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && ff && !NIL_P(ff)) IDWriteFontFile_Release(ff);
    }
    free(longp);
}

FTO_BOTH(fto_ffr_empty_path_note) {
    IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
    FILETIME ft = { 5, 6 };
    HRESULT hr = IDWriteFactory_CreateFontFileReference(f, L"", &ft, &ff);
    t_note("%s: empty path with a time -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && ff && !NIL_P(ff)) {
        BOOL sup = TRUE; DWRITE_FONT_FILE_TYPE t; DWRITE_FONT_FACE_TYPE fct; UINT32 nf;
        hr = IDWriteFontFile_Analyze(ff, &sup, &t, &fct, &nf);
        CHECK(FAILED(hr) || !sup);
        IDWriteFontFile_Release(ff);
    } else CHECK_OUT_NULL(ff);
    ff = (IDWriteFontFile *)FTO_SENTINEL;
    hr = IDWriteFactory_CreateFontFileReference(f, L"", NULL, &ff);
    t_note("%s: empty path without a time -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && ff && !NIL_P(ff)) IDWriteFontFile_Release(ff); else CHECK_OUT_NULL(ff);
}

FTO_BOTH(fto_ffr_loader_is_the_local_loader) {
    IDWriteFontFile *a = NULL, *b = NULL;
    IDWriteFontFileLoader *la = NULL, *lb = NULL;
    FILETIME ft = { 1, 2 };
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, L"C:\\a.ttf", &ft, &a));
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, L"C:\\b.ttf", &ft, &b));
    if (a && b) {
        CHECK(a != b);
        CHECK_HR_OK(IDWriteFontFile_GetLoader(a, &la));
        CHECK_HR_OK(IDWriteFontFile_GetLoader(b, &lb));
        CHECK(la != NULL && la == lb);            /* one shared local loader per process */
        const void *k = NULL; UINT32 ks = 0;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(a, &k, &ks));
        CHECK(k != NULL && ks > 0);
        const void *k2 = NULL; UINT32 ks2 = 0;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(b, &k2, &ks2));
        CHECK(ks2 > 0 && (ks != ks2 || memcmp(k, k2, ks)));    /* different paths give different keys */
    }
    DW_RELEASE(la); DW_RELEASE(lb); DW_RELEASE(a); DW_RELEASE(b);
}

FTO_BOTH(fto_ffr_reference_key_layout) {
    /* Key = 8 bytes of write-time ticks followed by the NUL terminated wide path (outside the Fonts directory). */
    static const WCHAR path[] = L"C:\\fto_keylayout\\Some File.TTF";
    IDWriteFontFile *a = NULL;
    FILETIME ft = { 0x89ABCDEF, 0x01234567 };
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, path, &ft, &a));
    if (a) {
        const void *k = NULL; UINT32 ks = 0;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(a, &k, &ks));
        UINT32 want = 8 + (UINT32)(lstrlenW(path) + 1) * 2;
        CHECKF(ks == want, "key size %u want %u", ks, want);
        if (k && ks >= 8) {
            UINT64 ticks; memcpy(&ticks, k, 8);
            CHECK(ticks == (((UINT64)ft.dwHighDateTime << 32) | ft.dwLowDateTime));
        }
        if (k && ks == want) {
            const WCHAR *wp = (const WCHAR *)((const BYTE *)k + 8);
            CHECK(!lstrcmpiW(wp, path));                       /* path stored with normalised case */
            t_note("%s: stored path text '%ls'", tag, wp);
        }
        IDWriteFontFile_Release(a);
    }
}

FTO_BOTH(fto_ffr_write_time_is_part_of_key) {
    IDWriteFontFile *a = NULL, *b = NULL, *c = NULL;
    FILETIME t1 = { 1, 1 }, t2 = { 2, 1 };
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, L"C:\\fto_same.ttf", &t1, &a));
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, L"C:\\fto_same.ttf", &t1, &b));
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, L"C:\\fto_same.ttf", &t2, &c));
    if (a && b && c) {
        const void *ka, *kb, *kc; UINT32 sa = 0, sb = 0, sc = 0;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(a, &ka, &sa));
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(b, &kb, &sb));
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(c, &kc, &sc));
        CHECK(sa == sb && sa > 0 && !memcmp(ka, kb, sa));
        CHECK(sa == sc && memcmp(ka, kc, sa) != 0);
        CHECK(a != b);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
}

FTO_BOTH(fto_ffr_path_case_in_key_note) {
    IDWriteFontFile *a = NULL, *b = NULL;
    FILETIME t = { 1, 1 };
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, L"C:\\Fto_Case\\A.TTF", &t, &a));
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, L"c:\\fto_case\\a.ttf", &t, &b));
    if (a && b) {
        const void *ka, *kb; UINT32 sa = 0, sb = 0;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(a, &ka, &sa));
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(b, &kb, &sb));
        CHECK(sa == sb);
        t_note("%s: keys for paths differing only in case are %s", tag, (sa == sb && !memcmp(ka, kb, sa)) ? "identical (case normalised)" : "different");
    }
    DW_RELEASE(a); DW_RELEASE(b);
}

FTO_BOTH(fto_ffr_fonts_dir_prefix_note) {
    WCHAR win[MAX_PATH], path[MAX_PATH + 32];
    IDWriteFontFile *a = NULL;
    FILETIME t = { 1, 1 };
    if (!GetWindowsDirectoryW(win, MAX_PATH)) { t_skip("no windows dir"); return; }
    wsprintfW(path, L"%s\\Fonts\\fto_prefix.ttf", win);
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, path, &t, &a));
    if (a) {
        const void *k; UINT32 ks = 0;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(a, &k, &ks));
        UINT32 full = 8 + (UINT32)(lstrlenW(path) + 1) * 2;
        t_note("%s: key for a path inside the Fonts directory has %u bytes (uncompressed would be %u)", tag, ks, full);
        CHECK(ks > 0 && ks <= full);        /* the Fonts directory prefix may be elided, never grown */
        IDWriteFontFile_Release(a);
    }
}

FTO_BOTH(fto_ffr_synthetic_file_on_disk) {
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    size_t n = 0; uint8_t *blob = ttf_build_default(&n);
    t_tmpfile(path, sizeof path, "fto_default.ttf");
    if (!write_file(path, blob, n)) { t_skip("cannot write scratch font"); free(blob); return; }
    free(blob);
    wide_path(path, wpath, MAX_PATH);
    IDWriteFontFile *ff = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, wpath, NULL, &ff));
    if (!ff) return;
    BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)99; DWRITE_FONT_FACE_TYPE fct = (DWRITE_FONT_FACE_TYPE)99; UINT32 nf = 0;
    CHECK_HR_OK(IDWriteFontFile_Analyze(ff, &sup, &ft, &fct, &nf));
    CHECK(sup);
    CHECK(ft == DWRITE_FONT_FILE_TYPE_TRUETYPE);
    CHECK(fct == DWRITE_FONT_FACE_TYPE_TRUETYPE);
    CHECK(nf == 1);
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, fct, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    if (face) {
        CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS);
        CHECK(IDWriteFontFace_GetType(face) == DWRITE_FONT_FACE_TYPE_TRUETYPE);
        CHECK(IDWriteFontFace_GetIndex(face) == 0);
        CHECK(IDWriteFontFace_GetSimulations(face) == DWRITE_FONT_SIMULATIONS_NONE);
        DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m);
        CHECK(m.designUnitsPerEm == TTF_DEF_UPM);
        UINT32 cps[3] = { 'A', ' ', 'z' }; UINT16 gi[3] = { 0xFFFF, 0xFFFF, 0xFFFF };
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cps, 3, gi));
        CHECK(gi[0] == 2 && gi[1] == 1 && gi[2] == ttf_default_gid('z'));
        UINT32 nfiles = 0; IDWriteFontFile *files[2] = { 0, 0 };
        CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &nfiles, NULL));
        CHECK(nfiles == 1);
        nfiles = 1;
        CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &nfiles, files));
        if (files[0]) {
            IDWriteFontFileLoader *l1 = NULL, *l2 = NULL;
            IDWriteFontFile_GetLoader(files[0], &l1); IDWriteFontFile_GetLoader(ff, &l2);
            CHECK(l1 != NULL && l1 == l2);
            DW_RELEASE(l1); DW_RELEASE(l2);
            IDWriteFontFile_Release(files[0]);
        }
        DW_RELEASE(face);
    }
    IDWriteFontFile_Release(ff);
}

FTO_BOTH(fto_ffr_garbage_file_not_supported) {
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    uint8_t junk[512]; unsigned i;
    for (i = 0; i < sizeof junk; i++) junk[i] = (uint8_t)(i * 31 + 7);
    t_tmpfile(path, sizeof path, "fto_junk.ttf");
    if (!write_file(path, junk, sizeof junk)) { t_skip("cannot write scratch file"); return; }
    wide_path(path, wpath, MAX_PATH);
    IDWriteFontFile *ff = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, wpath, NULL, &ff));
    if (!ff) return;
    BOOL sup = TRUE; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)99; DWRITE_FONT_FACE_TYPE fct = (DWRITE_FONT_FACE_TYPE)99; UINT32 nf = 5;
    HRESULT hr = IDWriteFontFile_Analyze(ff, &sup, &ft, &fct, &nf);
    t_note("%s: Analyze(garbage) -> %s supported=%d filetype=%d facetype=%d faces=%u", tag, dw_hr_name(hr), sup, ft, fct, nf);
    CHECK(FAILED(hr) || !sup);
    if (SUCCEEDED(hr) && !sup) { CHECK(nf == 0 || nf == 5); }
    IDWriteFontFace *face = (IDWriteFontFace *)FTO_SENTINEL;
    hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    CHECKF(FAILED(hr), "CreateFontFace on a non-font file: %s", dw_hr_name(hr));
    t_note("%s: CreateFontFace(garbage) -> %s", tag, dw_hr_name(hr));
    CHECK_OUT_NULL(face);
    IDWriteFontFile_Release(ff);
}

FTO_BOTH(fto_ffr_system_arial) {
    static const char *const cand[] = { "arial.ttf", NULL };
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    if (!dw_find_system_font(cand, path, sizeof path)) { t_skip("arial.ttf not installed"); return; }
    wide_path(path, wpath, MAX_PATH);
    IDWriteFontFile *ff = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, wpath, NULL, &ff));
    if (!ff) return;
    BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf = 0;
    CHECK_HR_OK(IDWriteFontFile_Analyze(ff, &sup, &ft, &fct, &nf));
    CHECK(sup && ft == DWRITE_FONT_FILE_TYPE_TRUETYPE && fct == DWRITE_FONT_FACE_TYPE_TRUETYPE && nf == 1);
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, fct, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    if (face) {
        DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m);
        CHECK(m.designUnitsPerEm == 2048);
        CHECK(IDWriteFontFace_GetGlyphCount(face) > 1000);
        t_note("Arial glyph count %u", IDWriteFontFace_GetGlyphCount(face));
        DW_RELEASE(face);
    }
    IDWriteFontFile_Release(ff);
}

FTO_BOTH(fto_ffr_lastwrite_time_argument) {
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    size_t n = 0; uint8_t *blob = ttf_build_default(&n);
    t_tmpfile(path, sizeof path, "fto_lw.ttf");
    if (!write_file(path, blob, n)) { t_skip("cannot write scratch font"); free(blob); return; }
    free(blob);
    wide_path(path, wpath, MAX_PATH);
    WIN32_FILE_ATTRIBUTE_DATA ad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &ad)) { t_skip("no attributes"); return; }
    IDWriteFontFile *ff = NULL;
    BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf;
    /* Correct time: behaves like NULL. */
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, wpath, &ad.ftLastWriteTime, &ff));
    if (ff) { CHECK_HR_OK(IDWriteFontFile_Analyze(ff, &sup, &ft, &fct, &nf)); CHECK(sup); DW_RELEASE(ff); }
    /* Wrong time: the reference is created; whether use fails is recorded. */
    FILETIME wrong = { 0x1234, 0x5678 };
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, wpath, &wrong, &ff));
    if (ff) { HRESULT hr = IDWriteFontFile_Analyze(ff, &sup, &ft, &fct, &nf); t_note("%s: Analyze with a stale lastWriteTime -> %s supported=%d", tag, dw_hr_name(hr), sup); DW_RELEASE(ff); }
}

/* ---- CreateCustomFontFileReference ------------------------------------------------------------------------------------- */
FTO_BOTH(fto_cfr_null_key) {
    if (t_is_wine()) { t_skip("NULL key crashes the Wine implementation"); return; }
    dwx_mem_loader *ld = dwx_loader_new();
    IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
    dwx_loader_register(ld, f);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, NULL, 4, (IDWriteFontFileLoader *)ld, &ff), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(ff);
    ff = (IDWriteFontFile *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, NULL, 0, (IDWriteFontFileLoader *)ld, &ff), FTO_E_INVALIDARG);
    CHECK_OUT_NULL(ff);
    CHECK(ld->n_create == 0);
    dwx_loader_unregister(ld); dwx_release(ld);
}

FTO_BOTH(fto_cfr_null_loader_note) {
    UINT32 key = 0;
    IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
    if (t_is_wine()) { t_skip("NULL loader crashes the Wine implementation"); return; }
    HRESULT hr = IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, NULL, &ff);
    t_note("%s: NULL loader -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && ff && !NIL_P(ff)) IDWriteFontFile_Release(ff); else CHECK_OUT_NULL(ff);
}

FTO_BOTH(fto_cfr_unregistered_loader_note) {
    dwx_mem_loader *ld = dwx_loader_new();
    UINT32 key = dwx_loader_add(ld, "x", 1);
    IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
    HRESULT hr = IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, (IDWriteFontFileLoader *)ld, &ff);
    t_note("%s: unregistered loader -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && ff && !NIL_P(ff)) IDWriteFontFile_Release(ff); else CHECK_OUT_NULL(ff);
    dwx_release(ld);
}

FTO_BOTH(fto_cfr_roundtrip_loader_and_key) {
    size_t n = 0; uint8_t *blob = ttf_build_default(&n);
    dwx_mem_loader *ld = dwx_loader_new();
    UINT32 k = dwx_loader_add(ld, blob, n);
    free(blob);
    CHECK_HR_OK(dwx_loader_register(ld, f));
    IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
    UINT32 keybuf = k;
    CHECK_HR_OK(IDWriteFactory_CreateCustomFontFileReference(f, &keybuf, sizeof keybuf, (IDWriteFontFileLoader *)ld, &ff));
    keybuf = 0xDEADBEEF;                       /* the reference owns a copy of the key */
    if (ff && !NIL_P(ff)) {
        EXPECT_REFS(ff, 1);
        IDWriteFontFileLoader *got = NULL;
        CHECK_HR_OK(IDWriteFontFile_GetLoader(ff, &got));
        CHECK(got == (IDWriteFontFileLoader *)ld);
        DW_RELEASE(got);
        const void *rk = NULL; UINT32 rks = 0;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff, &rk, &rks));
        CHECK(rks == 4 && rk != NULL);
        if (rk && rks == 4) { UINT32 v; memcpy(&v, rk, 4); CHECK(v == k); }
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontFile, 1), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontFileLoader, 0), DW_QI_ROW(IDWriteFactory, 0) };
        DW_CHECK_QI(ff, rows);
        IDWriteFontFile_Release(ff);
    }
    dwx_loader_unregister(ld); dwx_release(ld);
}

FTO_BOTH(fto_cfr_key_sizes) {
    static const UINT32 sizes[] = { 0, 1, 3, 4, 5, 64, 1024 };
    uint8_t key[1024];
    unsigned i;
    dwx_mem_loader *ld = dwx_loader_new();
    memset(key, 0x77, sizeof key);
    dwx_loader_register(ld, f);
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteFontFile *ff = (IDWriteFontFile *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateCustomFontFileReference(f, key, sizes[i], (IDWriteFontFileLoader *)ld, &ff);
        t_note("%s: key size %u -> %s", tag, sizes[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && ff && !NIL_P(ff)) {
            const void *rk = NULL; UINT32 rks = 99999;
            CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff, &rk, &rks));
            CHECKF(rks == sizes[i], "reference key size %u want %u", rks, sizes[i]);
            if (rk && rks) CHECK(!memcmp(rk, key, rks));
            IDWriteFontFile_Release(ff);
        } else CHECK_OUT_NULL(ff);
    }
    dwx_loader_unregister(ld); dwx_release(ld);
}

FTO_BOTH(fto_cfr_distinct_objects_and_lazy_stream) {
    dwx_font h;
    memset(&h, 0, sizeof h);
    size_t n = 0; uint8_t *blob = ttf_build_default(&n);
    h.ld = dwx_loader_new(); h.f = f;
    h.key = dwx_loader_add(h.ld, blob, n); free(blob);
    dwx_loader_register(h.ld, f);
    IDWriteFontFile *a = NULL, *b = NULL;
    CHECK_HR_OK(dwx_loader_file(h.ld, f, h.key, &a));
    CHECK_HR_OK(dwx_loader_file(h.ld, f, h.key, &b));
    CHECK(a != b);
    t_note("%s: CreateStreamFromKey calls right after reference creation: %ld", tag, h.ld->n_create);
    if (a) {
        BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf = 0;
        CHECK_HR_OK(IDWriteFontFile_Analyze(a, &sup, &ft, &fct, &nf));
        CHECK(sup && nf == 1);
        CHECK(h.ld->n_create >= 1);
        CHECK(h.ld->last_key_size == 4 && h.ld->last_key_value == h.key);
        CHECK(dwx_loader_total_reads(h.ld) >= 1);
        CHECK(dwx_loader_total_outstanding(h.ld) == 0);         /* every fragment handed out was released */
        CHECK(h.ld->streams[0]->bad_release_ctx == 0);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    dwx_loader_unregister(h.ld); dwx_release(h.ld);
}

/* ---- CreateFontFace argument validation ------------------------------------------------------------------------------ */
static HRESULT cff(IDWriteFactory *f, UINT32 type, UINT32 count, IDWriteFontFile **files, UINT32 idx, UINT32 sims, IDWriteFontFace **out) {
    return IDWriteFactory_CreateFontFace(f, (DWRITE_FONT_FACE_TYPE)type, count, files, idx, (DWRITE_FONT_SIMULATIONS)sims, out);
}

FTO_BOTH(fto_cff_success_and_simulations) {
    dwx_font h;
    if (FAILED(dwx_font_open_default(&h, f))) { t_skip("default font unavailable"); dwx_font_close(&h); return; }
    {
        unsigned sim;
        for (sim = 0; sim <= 3; sim++) {
            IDWriteFontFace *face = (IDWriteFontFace *)FTO_SENTINEL;
            HRESULT hr = cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, sim, &face);
            CHECKF(hr == S_OK, "simulations %u: %s", sim, dw_hr_name(hr));
            if (hr == S_OK && face) {
                CHECKF((UINT32)IDWriteFontFace_GetSimulations(face) == sim, "GetSimulations %u", sim);
                CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS);
                IDWriteFontFace_Release(face);
            }
        }
    }
    dwx_font_close(&h);
}

FTO_BOTH(fto_cff_invalid_face_type) {
    dwx_font h;
    static const UINT32 bad[] = { 3, 4, 5, 8, 9, 10, 100, 0x7fffffff };
    unsigned i;
    if (FAILED(dwx_font_open_default(&h, f))) { t_skip("default font unavailable"); dwx_font_close(&h); return; }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteFontFace *face = (IDWriteFontFace *)FTO_SENTINEL;
        HRESULT hr = cff(f, bad[i], 1, &h.file, 0, 0, &face);
        CHECKF(hr == FTO_E_INVALIDARG, "face type %u: %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(face);
    }
    dwx_font_close(&h);
}

FTO_BOTH(fto_cff_invalid_file_count_and_pointers) {
    dwx_font h;
    IDWriteFontFile *two[2];
    IDWriteFontFile *nul[1] = { NULL };
    IDWriteFontFace *face;
    static const UINT32 counts[] = { 0, 2, 3, 100, 0xffffffffu };
    unsigned i;
    if (FAILED(dwx_font_open_default(&h, f))) { t_skip("default font unavailable"); dwx_font_close(&h); return; }
    two[0] = two[1] = h.file;
    for (i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        face = (IDWriteFontFace *)FTO_SENTINEL;
        HRESULT hr = cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, counts[i], two, 0, 0, &face);
        CHECKF(hr == FTO_E_INVALIDARG, "file count %u: %s", counts[i], dw_hr_name(hr));
        CHECK_OUT_NULL(face);
    }
    if (t_is_wine()) { t_skip("NULL / zero file arrays crash the Wine implementation"); dwx_font_close(&h); return; }
    face = (IDWriteFontFace *)FTO_SENTINEL;
    CHECK_HR(cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, NULL, 0, 0, &face), FTO_E_INVALIDARG); CHECK_OUT_NULL(face);
    face = (IDWriteFontFace *)FTO_SENTINEL;
    CHECK_HR(cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, nul, 0, 0, &face), FTO_E_INVALIDARG); CHECK_OUT_NULL(face);
    face = (IDWriteFontFace *)FTO_SENTINEL;
    CHECK_HR(cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, NULL, 0, 0, &face), FTO_E_INVALIDARG); CHECK_OUT_NULL(face);
    dwx_font_close(&h);
}

FTO_BOTH(fto_cff_invalid_face_index) {
    dwx_font h;
    static const UINT32 bad[] = { 1, 2, 100, 0xffffffffu };
    unsigned i;
    if (FAILED(dwx_font_open_default(&h, f))) { t_skip("default font unavailable"); dwx_font_close(&h); return; }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteFontFace *face = (IDWriteFontFace *)FTO_SENTINEL;
        /* faceIndex must be 0 unless the face type is OPENTYPE_COLLECTION */
        HRESULT hr = cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, bad[i], 0, &face);
        CHECKF(hr == FTO_E_INVALIDARG, "TRUETYPE index %u: %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(face);
        face = (IDWriteFontFace *)FTO_SENTINEL;
        hr = cff(f, DWRITE_FONT_FACE_TYPE_CFF, 1, &h.file, bad[i], 0, &face);
        CHECKF(hr == FTO_E_INVALIDARG, "CFF index %u: %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(face);
        face = (IDWriteFontFace *)FTO_SENTINEL;
        hr = cff(f, DWRITE_FONT_FACE_TYPE_RAW_CFF, 1, &h.file, bad[i], 0, &face);
        CHECKF(hr == FTO_E_INVALIDARG, "RAW_CFF index %u: %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(face);
    }
    dwx_font_close(&h);
}

FTO_BOTH(fto_cff_invalid_simulations) {
    dwx_font h;
    static const UINT32 bad[] = { 4, 5, 8, 0x10, 0xff, 0x100, 0x80000000u, 0xfffffffcu, 0xffffffffu };
    unsigned i;
    if (FAILED(dwx_font_open_default(&h, f))) { t_skip("default font unavailable"); dwx_font_close(&h); return; }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteFontFace *face = (IDWriteFontFace *)FTO_SENTINEL;
        HRESULT hr = cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, bad[i], &face);
        CHECKF(hr == FTO_E_INVALIDARG, "simulations 0x%x: %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(face);
    }
    dwx_font_close(&h);
}

FTO_BOTH(fto_cff_wrong_type_for_file_note) {
    dwx_font h;
    static const struct { UINT32 type; const char *name; } t[] = { { 0, "CFF" }, { 2, "OPENTYPE_COLLECTION" }, { 6, "UNKNOWN" }, { 7, "RAW_CFF" } };
    unsigned i;
    if (FAILED(dwx_font_open_default(&h, f))) { t_skip("default font unavailable"); dwx_font_close(&h); return; }
    for (i = 0; i < 4; i++) {
        IDWriteFontFace *face = (IDWriteFontFace *)FTO_SENTINEL;
        HRESULT hr = cff(f, t[i].type, 1, &h.file, 0, 0, &face);
        t_note("%s: CreateFontFace(%s) over a TrueType file -> %s", tag, t[i].name, dw_hr_name(hr));
        if (SUCCEEDED(hr)) { CHECK(face && !NIL_P(face)); if (face && !NIL_P(face)) IDWriteFontFace_Release(face); }
        else CHECK_OUT_NULL(face);
    }
    dwx_font_close(&h);
}

FTO_BOTH(fto_cff_collection_indices) {
    size_t n0, n1, tn;
    uint8_t *a = ttf_build_styled("FtoColA", 400, 5, 0, &n0), *b = ttf_build_styled("FtoColB", 700, 5, 0, &n1);
    const uint8_t *blobs[2] = { a, b }; size_t sizes[2] = { n0, n1 };
    uint8_t *ttc = ttf_build_ttc(blobs, sizes, 2, &tn);
    dwx_mem_loader *ld = dwx_loader_new();
    UINT32 key = dwx_loader_add(ld, ttc, tn);
    free(a); free(b); free(ttc);
    dwx_loader_register(ld, f);
    IDWriteFontFile *ff = NULL;
    CHECK_HR_OK(dwx_loader_file(ld, f, key, &ff));
    if (ff) {
        BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)99; DWRITE_FONT_FACE_TYPE fct = (DWRITE_FONT_FACE_TYPE)99; UINT32 nf = 0;
        CHECK_HR_OK(IDWriteFontFile_Analyze(ff, &sup, &ft, &fct, &nf));
        CHECK(sup);
        CHECK(ft == DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION);
        CHECK(fct == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
        CHECK(nf == 2);
        unsigned i;
        for (i = 0; i < 2; i++) {
            IDWriteFontFace *face = (IDWriteFontFace *)FTO_SENTINEL;
            HRESULT hr = cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &ff, i, 0, &face);
            CHECKF(hr == S_OK, "collection index %u: %s", i, dw_hr_name(hr));
            if (hr == S_OK && face) {
                CHECK(IDWriteFontFace_GetIndex(face) == i);
                CHECK(IDWriteFontFace_GetType(face) == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
                CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS);
                IDWriteFontFace_Release(face);
            }
        }
        IDWriteFontFace *face = (IDWriteFontFace *)FTO_SENTINEL;
        HRESULT hr = cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &ff, 2, 0, &face);
        CHECKF(FAILED(hr), "index 2 of a 2-font collection: %s", dw_hr_name(hr));
        t_note("%s: collection index 2 -> %s", tag, dw_hr_name(hr));
        CHECK_OUT_NULL(face);
        face = (IDWriteFontFace *)FTO_SENTINEL;
        hr = cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, 0, &face);
        t_note("%s: TRUETYPE type over a collection file, index 0 -> %s", tag, dw_hr_name(hr));
        if (SUCCEEDED(hr) && face && !NIL_P(face)) IDWriteFontFace_Release(face); else CHECK_OUT_NULL(face);
        IDWriteFontFile_Release(ff);
    }
    dwx_loader_unregister(ld); dwx_release(ld);
}

FTO_BOTH(fto_cff_face_keeps_file_alive) {
    dwx_font h;
    if (FAILED(dwx_font_open_default(&h, f))) { t_skip("default font unavailable"); dwx_font_close(&h); return; }
    IDWriteFontFace *face = NULL;
    CHECK_HR_OK(cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, 0, &face));
    DW_RELEASE(h.file);                       /* the caller's file reference is not needed any more */
    if (face) {
        const void *d = NULL; UINT32 sz = 0; void *ctx = NULL; BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteFontFace_TryGetFontTable(face, DW_TAG('h', 'e', 'a', 'd'), &d, &sz, &ctx, &ex));
        CHECK(ex && sz == 54 && d != NULL);
        if (ex) IDWriteFontFace_ReleaseFontTable(face, ctx);
        CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS);
        DW_RELEASE(face);
    }
    dwx_font_close(&h);
}

FTO_BOTH(fto_cff_same_input_twice_note) {
    dwx_font h;
    if (FAILED(dwx_font_open_default(&h, f))) { t_skip("default font unavailable"); dwx_font_close(&h); return; }
    IDWriteFontFace *a = NULL, *b = NULL;
    CHECK_HR_OK(cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, 0, &a));
    CHECK_HR_OK(cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, 0, &b));
    if (a && b) {
        t_note("%s: same file/index/sim twice -> %s face object", tag, a == b ? "the same" : "different");
        CHECK(IDWriteFontFace_GetGlyphCount(a) == IDWriteFontFace_GetGlyphCount(b));
    }
    DW_RELEASE(a); DW_RELEASE(b);
    IDWriteFontFace *c = NULL;
    CHECK_HR_OK(cff(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &c));
    if (c) { CHECK(IDWriteFontFace_GetSimulations(c) == DWRITE_FONT_SIMULATIONS_BOLD); DW_RELEASE(c); }
    dwx_font_close(&h);
}

/* ---- CreateGlyphRunAnalysis ---------------------------------------------------------------------------------------------- */
typedef struct { dwx_font h; UINT16 gi[3]; DWRITE_GLYPH_RUN run; } garun;
static int garun_open(garun *g, IDWriteFactory *f) {
    memset(g, 0, sizeof *g);
    if (FAILED(dwx_font_open_default(&g->h, f)) || !g->h.face) return 0;
    g->gi[0] = ttf_default_gid('A'); g->gi[1] = ttf_default_gid('B'); g->gi[2] = ttf_default_gid('C');
    g->run.fontFace = g->h.face; g->run.fontEmSize = 20.0f; g->run.glyphCount = 3; g->run.glyphIndices = g->gi;
    return 1;
}
static const DWRITE_MATRIX ident = { 1, 0, 0, 1, 0, 0 };

FTO_BOTH(fto_gra_valid_default) {
    garun g;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a));
    if (a && !NIL_P(a)) {
        EXPECT_REFS(a, 1);
        RECT rc; memset(&rc, 0xAA, sizeof rc);
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        CHECK(rc.right > rc.left && rc.bottom > rc.top);
        CHECK(rc.top < 0);                                  /* glyph boxes rise above the baseline */
        CHECK(rc.right - rc.left >= 15 && rc.right - rc.left <= 45);
        t_note("%s: aliased bounds of 'ABC' at 20: %ld,%ld - %ld,%ld", tag, rc.left, rc.top, rc.right, rc.bottom);
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteGlyphRunAnalysis, 1), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteTextFormat, 0) };
        DW_CHECK_QI(a, rows);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    dwx_font_close(&g.h);
}

FTO_BOTH(fto_gra_origin_translates_bounds) {
    garun g;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a));
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 100.0f, 50.0f, &b));
    if (a && b) {
        RECT ra, rb;
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_ALIASED_1x1, &ra));
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(b, DWRITE_TEXTURE_ALIASED_1x1, &rb));
        CHECK(rb.left == ra.left + 100 && rb.right == ra.right + 100);
        CHECK(rb.top == ra.top + 50 && rb.bottom == ra.bottom + 50);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    dwx_font_close(&g.h);
}

FTO_BOTH(fto_gra_invalid_run_pointers) {
    garun g;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    IDWriteGlyphRunAnalysis *a;
    DWRITE_GLYPH_RUN r;
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, NULL, 1.0f, NULL, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    r = g.run; r.fontFace = NULL;
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &r, 1.0f, NULL, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    r = g.run; r.glyphIndices = NULL;
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &r, 1.0f, NULL, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    r = g.run; r.fontEmSize = -1.0f;
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &r, 1.0f, NULL, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    r = g.run; r.fontEmSize = -1e-20f;
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &r, 1.0f, NULL, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    dwx_font_close(&g.h);
}

FTO_BOTH(fto_gra_invalid_scalars) {
    garun g;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    static const float badppd[] = { 0.0f, -0.0f, -1.0f, -FLT_MIN, -INFINITY, NAN };
    unsigned i;
    for (i = 0; i < sizeof badppd / sizeof badppd[0]; i++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, badppd[i], NULL, 0, 0, 0, 0, &a);
        CHECKF(hr == FTO_E_INVALIDARG, "pixelsPerDip %g: %s", badppd[i], dw_hr_name(hr)); CHECK_OUT_NULL(a);
    }
    IDWriteGlyphRunAnalysis *a;
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, 0, 0, NAN, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, 0, 0, 0, NAN, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, 7, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, 100, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, 0, 3, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, 0, 100, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    dwx_font_close(&g.h);
}

FTO_BOTH(fto_gra_transform_nan_members) {
    garun g;
    unsigned i;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX m = ident;
        FLOAT *p = &m.m11 + i;
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
        *p = NAN;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, &m, 0, 0, 0, 0, &a);
        CHECKF(hr == FTO_E_INVALIDARG, "NaN in matrix member %u: %s", i, dw_hr_name(hr)); CHECK_OUT_NULL(a);
    }
    dwx_font_close(&g.h);
}

FTO_BOTH(fto_gra_mode_matrix) {
    garun g;
    unsigned rm, mm;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    for (rm = 0; rm <= 5; rm++)
        for (mm = 0; mm <= 2; mm++) {
            IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
            HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, &ident, (DWRITE_RENDERING_MODE)rm, (DWRITE_MEASURING_MODE)mm, 0, 0, &a);
            if (rm == 0) t_note("%s: rendering mode DEFAULT measuring %u -> %s", tag, mm, dw_hr_name(hr));     /* resolved inside Create */
            else CHECKF(hr == S_OK, "rendering mode %u measuring mode %u: %s", rm, mm, dw_hr_name(hr));
            if (SUCCEEDED(hr) && a && !NIL_P(a)) IDWriteGlyphRunAnalysis_Release(a);
        }
    for (mm = 0; mm <= 2; mm++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, &ident, DWRITE_RENDERING_MODE_OUTLINE, (DWRITE_MEASURING_MODE)mm, 0, 0, &a);
        t_note("%s: OUTLINE rendering mode, measuring %u -> %s", tag, mm, dw_hr_name(hr));
        if (SUCCEEDED(hr) && a && !NIL_P(a)) IDWriteGlyphRunAnalysis_Release(a); else CHECK_OUT_NULL(a);
    }
    dwx_font_close(&g.h);
}

FTO_BOTH(fto_gra_pixels_per_dip_and_transform_valid) {
    garun g;
    static const float ppd[] = { 0.001f, 0.5f, 1.0f, 1.5f, 2.0f, 8.0f, 100.0f };
    static const DWRITE_MATRIX xf[] = { { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 2, 5, 5 }, { 0, 1, -1, 0, 0, 0 }, { 1, 0.3f, 0, 1, 0, 0 }, { -1, 0, 0, 1, 10, 0 } };
    unsigned i;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    for (i = 0; i < sizeof ppd / sizeof ppd[0]; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, ppd[i], NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
        CHECKF(hr == S_OK, "pixelsPerDip %g: %s", ppd[i], dw_hr_name(hr));
        DW_RELEASE(a);
    }
    for (i = 0; i < sizeof xf / sizeof xf[0]; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, &xf[i], DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a);
        CHECKF(hr == S_OK, "transform #%u: %s", i, dw_hr_name(hr));
        DW_RELEASE(a);
    }
    dwx_font_close(&g.h);
}

FTO_BOTH(fto_gra_degenerate_runs_note) {
    garun g;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    DWRITE_GLYPH_RUN r = g.run;
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    r.glyphCount = 0;
    HRESULT hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &r, 1.0f, NULL, 1, 0, 0, 0, &a);
    t_note("%s: zero glyphs -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && a && !NIL_P(a)) { RECT rc; HRESULT h2 = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc); t_note("   bounds -> %s %ld,%ld-%ld,%ld", dw_hr_name(h2), rc.left, rc.top, rc.right, rc.bottom); IDWriteGlyphRunAnalysis_Release(a); }
    else CHECK_OUT_NULL(a);
    r = g.run; r.fontEmSize = 0.0f;
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &r, 1.0f, NULL, 1, 0, 0, 0, &a);
    t_note("%s: zero em size -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && a && !NIL_P(a)) IDWriteGlyphRunAnalysis_Release(a); else CHECK_OUT_NULL(a);
    r = g.run; r.fontEmSize = 20.0f; r.glyphIndices = (const UINT16[]){ 0xFFFF, 0xFFFE, 0 };
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    hr = IDWriteFactory_CreateGlyphRunAnalysis(f, &r, 1.0f, NULL, 1, 0, 0, 0, &a);
    t_note("%s: out-of-range glyph indices -> %s", tag, dw_hr_name(hr));
    if (SUCCEEDED(hr) && a && !NIL_P(a)) IDWriteGlyphRunAnalysis_Release(a); else CHECK_OUT_NULL(a);
    dwx_font_close(&g.h);
}

TEST(fto_gra2_validation_and_success) {
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2);
    garun g;
    if (!f2) { t_skip("IDWriteFactory2 unsupported"); return; }
    if (!garun_open(&g, dw_shared())) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    unsigned gf, aa;
    for (gf = 0; gf < 5; gf++)
        for (aa = 0; aa < 4; aa++) {
            a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
            HRESULT hr = IDWriteFactory2_CreateGlyphRunAnalysis(f2, &g.run, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, (DWRITE_GRID_FIT_MODE)gf, (DWRITE_TEXT_ANTIALIAS_MODE)aa, 0, 0, &a);
            if (gf <= 2 && aa <= 1) { CHECKF(hr == S_OK, "gridfit %u aa %u: %s", gf, aa, dw_hr_name(hr)); if (hr == S_OK && a) IDWriteGlyphRunAnalysis_Release(a); }
            else { CHECKF(hr == FTO_E_INVALIDARG, "gridfit %u aa %u: %s", gf, aa, dw_hr_name(hr)); CHECK_OUT_NULL(a); }
        }
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory2_CreateGlyphRunAnalysis(f2, &g.run, NULL, (DWRITE_RENDERING_MODE)7, 0, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory2_CreateGlyphRunAnalysis(f2, &g.run, NULL, 0, (DWRITE_MEASURING_MODE)3, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory2_CreateGlyphRunAnalysis(f2, NULL, NULL, 0, 0, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory2_CreateGlyphRunAnalysis(f2, &g.run, NULL, 0, 0, 0, 0, NAN, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory2_CreateGlyphRunAnalysis(f2, &g.run, NULL, 0, 0, 0, 0, 0, NAN, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    {
        DWRITE_MATRIX m = ident; m.dy = NAN;
        a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
        CHECK_HR(IDWriteFactory2_CreateGlyphRunAnalysis(f2, &g.run, &m, 0, 0, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    }
    dwx_font_close(&g.h);
}

TEST(fto_gra2_no_pixels_per_dip_so_zero_origin_ok) {
    /* The v2 overload has no pixelsPerDip argument: nothing to reject beyond NaN, and large origins are accepted. */
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2);
    garun g;
    if (!f2) { t_skip("IDWriteFactory2 unsupported"); return; }
    if (!garun_open(&g, dw_shared())) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    IDWriteGlyphRunAnalysis *a = NULL;
    CHECK_HR_OK(IDWriteFactory2_CreateGlyphRunAnalysis(f2, &g.run, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE, 1e5f, -1e5f, &a));
    DW_RELEASE(a);
    dwx_font_close(&g.h);
}

TEST(fto_gra3_rendering_mode1_range) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    garun g;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    if (!garun_open(&g, dw_shared())) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    unsigned m;
    for (m = 0; m < 12; m++) {
        IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
        HRESULT hr = IDWriteFactory3_CreateGlyphRunAnalysis(f3, &g.run, NULL, (DWRITE_RENDERING_MODE1)m, DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT, DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE, 0, 0, &a);
        if (m <= 5) { CHECKF(hr == S_OK, "mode1 %u: %s", m, dw_hr_name(hr)); if (hr == S_OK && a) IDWriteGlyphRunAnalysis_Release(a); }
        else if (m <= 7) { t_note("mode1 %u -> %s", m, dw_hr_name(hr)); if (SUCCEEDED(hr) && a && !NIL_P(a)) IDWriteGlyphRunAnalysis_Release(a); else CHECK_OUT_NULL(a); }
        else { CHECKF(hr == FTO_E_INVALIDARG, "mode1 %u: %s", m, dw_hr_name(hr)); CHECK_OUT_NULL(a); }
    }
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, &g.run, NULL, 0, 3, 0, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, &g.run, NULL, 0, 0, 3, 0, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    a = (IDWriteGlyphRunAnalysis *)FTO_SENTINEL;
    CHECK_HR(IDWriteFactory3_CreateGlyphRunAnalysis(f3, &g.run, NULL, 0, 0, 0, 2, 0, 0, &a), FTO_E_INVALIDARG); CHECK_OUT_NULL(a);
    dwx_font_close(&g.h);
}

FTO_BOTH(fto_gra_analysis_keeps_face_alive_note) {
    garun g;
    if (!garun_open(&g, f)) { t_skip("default font unavailable"); dwx_font_close(&g.h); return; }
    IDWriteGlyphRunAnalysis *a = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(f, &g.run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a));
    if (a) {
        ULONG before = dw_refcount((IUnknown *)g.h.face);
        IDWriteGlyphRunAnalysis_Release(a);
        ULONG after = dw_refcount((IUnknown *)g.h.face);
        t_note("%s: face refcount %lu while the analysis lived, %lu after it was released", tag, before, after);
        CHECK(after <= before);
    }
    dwx_font_close(&g.h);
}
