/* Property getters of IDWriteFontFaceReference: GetFontFaceIndex, GetSimulations, GetFontFile, GetFileSize, GetLocalFileSize,
 * GetFileTime, GetLocality, Equals, CreateFontFace, CreateFontFaceWithSimulations.
 *
 * Win10 dwrite.dll (DWriteFontFaceReferenceBase, !0xe42b0 GetFileSize, !0xe4530 GetLocalFileSize, !0xe4320 GetFileTime, !0x4e4d0 GetLocality,
 * !0xe4450 GetFontFile, !0xe4240 Equals, !0x2a210 CreateFontFace, !0x78360 CreateFontFaceWithSimulations):
 *   - GetFileSize / GetLocalFileSize / GetFileTime forward to the font file stream of the reference's loader: GetFileSize or GetLastWriteTime
 *     of the stream. A stream failing GetFileSize (any failure) or GetLastWriteTime (anything but E_NOTIMPL) raises a C++ exception, so those
 *     failure modes are deliberately not exercised here. E_NOTIMPL from GetLastWriteTime reads as time 0 with S_OK.
 *   - GetLocalFileSize asks the stream for IDWriteRemoteFontFileStream::GetLocalFileSize and falls back to GetFileSize.
 *   - GetLocality asks the LOADER for IDWriteRemoteFontFileLoader; without it the answer is DWRITE_LOCALITY_LOCAL (2); with it the loader's
 *     GetLocalityFromKey(key, keySize, &locality) answer is returned on every call.
 *   - GetFontFile always builds a NEW IDWriteFontFile (never a cached one) for the same loader and key.
 *   - Equals(NULL) is FALSE; otherwise the reference keys (loader identity, key bytes, face index, simulations, axis values) are compared.
 */
#include "frr.h"

#define SIM_BOLD DWRITE_FONT_SIMULATIONS_BOLD
#define SIM_OBL DWRITE_FONT_SIMULATIONS_OBLIQUE
#define SIM_NONE DWRITE_FONT_SIMULATIONS_NONE

static int wide_ends_with_i(const WCHAR *s, const WCHAR *suffix) {
    size_t a = lstrlenW(s), b = lstrlenW(suffix);
    return a >= b && !lstrcmpiW(s + a - b, suffix);
}

/* ---- index / simulations ---------------------------------------------------------------------------------------------- */
TEST(getters_index_simulations_matrix) {
    FRR_OPEN(x);
    static const UINT32 idx[] = { 0, 1, 2, 100 };
    size_t i; UINT32 s;
    for (i = 0; i < 4; i++) for (s = 0; s <= 3; s++) {
        IDWriteFontFaceReference *r = frr_ref(&x, idx[i], (DWRITE_FONT_SIMULATIONS)s);
        if (!r) continue;
        t_check(IDWriteFontFaceReference_GetFontFaceIndex(r) == idx[i], __FILE__, __LINE__, "index", "idx %u sim %u", idx[i], s);
        t_check(IDWriteFontFaceReference_GetSimulations(r) == (DWRITE_FONT_SIMULATIONS)s, __FILE__, __LINE__, "sim", "idx %u sim %u", idx[i], s);
        DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(getters_are_repeatable) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 3, SIM_BOLD | SIM_OBL);
    if (r) {
        int k;
        for (k = 0; k < 20; k++) {
            EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 3);
            EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), (SIM_BOLD | SIM_OBL));
        }
        EXPECT_REFS(r, 1);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(getters_reference1_matches_reference) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 1, SIM_OBL);
    IDWriteFontFaceReference1 *r1 = r ? frr_ref1(r) : NULL;
    if (r && r1) {
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontFaceIndex(r1), IDWriteFontFaceReference_GetFontFaceIndex(r));
        EXPECT_EQ(IDWriteFontFaceReference1_GetSimulations(r1), IDWriteFontFaceReference_GetSimulations(r));
        EXPECT_EQ(IDWriteFontFaceReference1_GetLocality(r1), IDWriteFontFaceReference_GetLocality(r));
        EXPECT_EQ(IDWriteFontFaceReference1_GetFileSize(r1), IDWriteFontFaceReference_GetFileSize(r));
        EXPECT_EQ(IDWriteFontFaceReference1_GetLocalFileSize(r1), IDWriteFontFaceReference_GetLocalFileSize(r));
        CHECK(IDWriteFontFaceReference1_Equals(r1, r));
        CHECK(IDWriteFontFaceReference_Equals(r, (IDWriteFontFaceReference *)r1));
    }
    DW_RELEASE(r1); DW_RELEASE(r);
    frr_close(&x);
}

/* ---- GetFontFile -------------------------------------------------------------------------------------------------------- */
TEST(getfontfile_loader_and_key_identity) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        IDWriteFontFile *ff = (IDWriteFontFile *)(ULONG_PTR)8;
        CHECK_HR(IDWriteFontFaceReference_GetFontFile(r, &ff), S_OK);
        CHECK(ff != NULL && ff != (IDWriteFontFile *)(ULONG_PTR)8);
        if (ff && ff != (IDWriteFontFile *)(ULONG_PTR)8) {
            EXPECT_REFS(ff, 1);
            IDWriteFontFileLoader *ld = NULL; CHECK_HR(IDWriteFontFile_GetLoader(ff, &ld), S_OK);
            CHECK(ld == (IDWriteFontFileLoader *)x.ld);
            DW_RELEASE(ld);
            const void *key = NULL; UINT32 ks = 0;
            CHECK_HR(IDWriteFontFile_GetReferenceKey(ff, &key, &ks), S_OK);
            EXPECT_EQ(ks, 4);
            if (key && ks == 4) EXPECT_EQ(*(const UINT32 *)key, x.key);
        } else ff = NULL;
        DW_RELEASE(ff);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(getfontfile_returns_new_object_each_call) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        IDWriteFontFile *a = NULL, *b = NULL;
        CHECK_HR(IDWriteFontFaceReference_GetFontFile(r, &a), S_OK);
        CHECK_HR(IDWriteFontFaceReference_GetFontFile(r, &b), S_OK);
        CHECK(a && b && a != b);              /* both are alive at once, so they are different allocations */
        CHECK(a != x.file && b != x.file);
        DW_RELEASE(a); DW_RELEASE(b);
        EXPECT_REFS(r, 1);                    /* the file does not retain the reference */
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(getfontfile_analyze_matches_blob) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFile *ff = NULL;
    if (r && SUCCEEDED(IDWriteFontFaceReference_GetFontFile(r, &ff)) && ff) {
        BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)99; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)99; UINT32 n = 0;
        CHECK_HR(IDWriteFontFile_Analyze(ff, &sup, &ft, &fc, &n), S_OK);
        EXPECT_EQ(sup, TRUE);
        EXPECT_EQ(ft, DWRITE_FONT_FILE_TYPE_TRUETYPE);
        EXPECT_EQ(fc, DWRITE_FONT_FACE_TYPE_TRUETYPE);
        EXPECT_EQ(n, 1);
    }
    DW_RELEASE(ff); DW_RELEASE(r);
    frr_close(&x);
}
TEST(getfontfile_ignores_index_and_simulations) {
    FRR_OPEN(x);
    UINT32 s;
    for (s = 0; s <= 3; s++) {
        IDWriteFontFaceReference *r = frr_ref(&x, s, (DWRITE_FONT_SIMULATIONS)s);
        IDWriteFontFile *ff = NULL;
        if (r && SUCCEEDED(IDWriteFontFaceReference_GetFontFile(r, &ff)) && ff) {
            const void *key = NULL; UINT32 ks = 0;
            CHECK_HR(IDWriteFontFile_GetReferenceKey(ff, &key, &ks), S_OK);
            t_check(ks == 4 && key && *(const UINT32 *)key == x.key, __FILE__, __LINE__, "same key", "idx/sim %u", s);
        }
        DW_RELEASE(ff); DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(getfontfile_ttc_reports_two_faces) {
    size_t n1, n2, nt; uint8_t *b1 = ttf_build_styled("TtcA", 400, 5, 0, &n1), *b2 = ttf_build_styled("TtcB", 700, 5, 0, &n2);
    const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &nt);
    frr_fix x;
    if (b1 && b2 && ttc && frr_open(&x, ttc, nt)) {
        IDWriteFontFaceReference *r = frr_ref(&x, 1, SIM_NONE);
        IDWriteFontFile *ff = NULL;
        if (r && SUCCEEDED(IDWriteFontFaceReference_GetFontFile(r, &ff)) && ff) {
            BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 n = 0;
            CHECK_HR(IDWriteFontFile_Analyze(ff, &sup, &ft, &fc, &n), S_OK);
            EXPECT_EQ(sup, TRUE); EXPECT_EQ(n, 2);
            EXPECT_EQ(fc, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
        }
        DW_RELEASE(ff); DW_RELEASE(r);
    } else if (!(b1 && b2 && ttc)) t_skip("ttf_build_ttc failed");
    frr_close(&x);
    free(b1); free(b2); free(ttc);
}
TEST(getfontfile_path_reference_uses_local_loader) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; if (!frr_sys_path("arial.ttf", p, MAX_PATH)) { t_skip("arial.ttf not installed"); return; }
    IDWriteFontFaceReference *r = NULL; IDWriteFontFile *ff = NULL; IDWriteFontFileLoader *ld = NULL;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, 0, SIM_NONE, &r), S_OK);
    if (r && SUCCEEDED(IDWriteFontFaceReference_GetFontFile(r, &ff)) && ff && SUCCEEDED(IDWriteFontFile_GetLoader(ff, &ld)) && ld) {
        IDWriteLocalFontFileLoader *loc = NULL;
        CHECK_HR(IDWriteFontFileLoader_QueryInterface(ld, &IID_IDWriteLocalFontFileLoader, (void **)&loc), S_OK);
        const void *key = NULL; UINT32 ks = 0;
        if (loc && SUCCEEDED(IDWriteFontFile_GetReferenceKey(ff, &key, &ks))) {
            UINT32 len = 0; WCHAR got[MAX_PATH * 2];
            CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(loc, key, ks, &len), S_OK);
            if (len > 0 && len < MAX_PATH * 2 - 1) {
                CHECK_HR(IDWriteLocalFontFileLoader_GetFilePathFromKey(loc, key, ks, got, len + 1), S_OK);
                CHECK(!lstrcmpiW(got, p));          /* the key stores the path exactly as given */
            } else t_check(0, __FILE__, __LINE__, "path length", "len %u", len);
        }
        DW_RELEASE(loc);
    }
    DW_RELEASE(ld); DW_RELEASE(ff); DW_RELEASE(r);
}

/* ---- sizes / times --------------------------------------------------------------------------------------------------------- */
TEST(size_matches_blob_and_stream_is_consulted) {
    size_t n; uint8_t *b = ttf_build_default(&n); frr_fix x;
    if (b && frr_open(&x, b, n)) {
        IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
        if (r) {
            EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r), (UINT64)n);
            EXPECT_EQ(IDWriteFontFaceReference_GetLocalFileSize(r), (UINT64)n);
            CHECK(x.ld->nstreams >= 1);
            LONG sizes = 0; int i;
            for (i = 0; i < x.ld->nstreams; i++) sizes += x.ld->streams[i]->n_getsize;
            CHECK(sizes >= 2);                    /* GetFileSize and the GetLocalFileSize fallback both ask the stream (a stream is not queried through any cache) */
        }
        DW_RELEASE(r);
    } else if (!b) t_skip("ttf_build_default failed");
    frr_close(&x); free(b);
}
TEST(size_of_each_blob_is_reported_per_reference) {
    size_t n1, n2; uint8_t *b1 = ttf_build_styled("SizeA", 400, 5, 0, &n1), *b2 = ttf_build_styled("SizeB", 700, 5, 1, &n2);
    frr_fix x; IDWriteFontFile *f2 = NULL; IDWriteFontFaceReference *r1 = NULL, *r2 = NULL;
    if (b1 && b2 && frr_open(&x, b1, n1)) {
        UINT32 k2 = dwx_loader_add(x.ld, b2, n2);
        if (SUCCEEDED(dwx_loader_file(x.ld, x.f, k2, &f2)) && f2) {
            r1 = frr_ref(&x, 0, SIM_NONE);
            CHECK_HR(IDWriteFactory3_CreateFontFaceReference_(x.f3, f2, 0, SIM_NONE, &r2), S_OK);
            if (r1 && r2) {
                EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r1), (UINT64)n1);
                EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r2), (UINT64)n2);
                EXPECT_EQ(IDWriteFontFaceReference_GetLocalFileSize(r2), (UINT64)n2);
            }
        }
    } else if (!(b1 && b2)) t_skip("ttf_build_styled failed");
    DW_RELEASE(r1); DW_RELEASE(r2); DW_RELEASE(f2);
    frr_close(&x); free(b1); free(b2);
}
TEST(size_arial_matches_disk) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; UINT64 disk = 0; FILETIME ft;
    if (!frr_sys_path("arial.ttf", p, MAX_PATH) || !frr_disk_info(p, &disk, &ft)) { t_skip("arial.ttf not installed"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 0, SIM_NONE);
    if (!r) return;
    EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r), disk);
    EXPECT_EQ(IDWriteFontFaceReference_GetLocalFileSize(r), disk);
    CHECK(disk > 0);
    DW_RELEASE(r);
}
TEST(size_msgothic_ttc_is_the_whole_collection) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; UINT64 disk = 0; FILETIME ft;
    if (!frr_sys_path("msgothic.ttc", p, MAX_PATH) || !frr_disk_info(p, &disk, &ft)) { t_skip("msgothic.ttc not installed"); return; }
    UINT32 i;
    for (i = 0; i < 3; i++) {
        IDWriteFontFaceReference *r = frr_sys_ref(f3, "msgothic.ttc", i, SIM_NONE);
        if (!r) return;
        t_check(IDWriteFontFaceReference_GetFileSize(r) == disk, __FILE__, __LINE__, "ttc size", "member %u", i);
        DW_RELEASE(r);
    }
}
TEST(filetime_comes_from_stream_last_write) {
    size_t n; uint8_t *b = ttf_build_default(&n); frr_fix x;
    if (b && frr_open(&x, b, n)) {
        static const UINT64 vals[] = { 0x01D0000000000000ull, 0x01D5A3B2C4D5E6F7ull, 0x0000000100000002ull, 0x00000000FFFFFFFFull, 0x7FFFFFFFFFFFFFFFull, 0x1ull };
        size_t i;
        for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
            size_t k;
            size_t n2 = n; uint8_t *copy = (uint8_t *)malloc(n2); memcpy(copy, b, n2);
            UINT32 key = dwx_loader_add(x.ld, copy, n2); free(copy);
            x.ld->last_write = vals[i];
            IDWriteFontFile *f = NULL; IDWriteFontFaceReference *r = NULL;
            if (SUCCEEDED(dwx_loader_file(x.ld, x.f, key, &f)) && f && SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, f, 0, SIM_NONE, &r)) && r) {
                FILETIME ft = { 0xDEADBEEF, 0xFEEDFACE };
                CHECK_HR(IDWriteFontFaceReference_GetFileTime(r, &ft), S_OK);
                t_check(frr_ft64(ft) == vals[i], __FILE__, __LINE__, "filetime", "want %016llx got %016llx", (unsigned long long)vals[i], (unsigned long long)frr_ft64(ft));
            }
            DW_RELEASE(r); DW_RELEASE(f);
        }
    } else if (!b) t_skip("ttf_build_default failed");
    frr_close(&x); free(b);
}
TEST(filetime_repeated_calls_agree) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        FILETIME a, b; int k;
        CHECK_HR(IDWriteFontFaceReference_GetFileTime(r, &a), S_OK);
        for (k = 0; k < 5; k++) { CHECK_HR(IDWriteFontFaceReference_GetFileTime(r, &b), S_OK); EXPECT_EQ(frr_ft64(a), frr_ft64(b)); }
        EXPECT_EQ(frr_ft64(a), x.ld->last_write);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(filetime_notimpl_stream_reads_as_zero) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        int i;
        IDWriteFontFaceReference_GetFileSize(r);          /* forces the stream to exist */
        for (i = 0; i < x.ld->nstreams; i++) x.ld->streams[i]->lastwrite_notimpl = 1;
        FILETIME ft = { 0x1234, 0x5678 };
        CHECK_HR(IDWriteFontFaceReference_GetFileTime(r, &ft), S_OK);     /* E_NOTIMPL is the one failure GetFileTime tolerates */
        EXPECT_EQ(frr_ft64(ft), 0);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(filetime_arial_matches_disk) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; UINT64 disk = 0; FILETIME dft;
    if (!frr_sys_path("arial.ttf", p, MAX_PATH) || !frr_disk_info(p, &disk, &dft)) { t_skip("arial.ttf not installed"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 0, SIM_NONE);
    if (!r) return;
    FILETIME ft = { 1, 2 };
    CHECK_HR(IDWriteFontFaceReference_GetFileTime(r, &ft), S_OK);
    EXPECT_EQ(frr_ft64(ft), frr_ft64(dft));
    DW_RELEASE(r);
}
TEST(filetime_path_reference_with_explicit_time_keeps_disk_time) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; UINT64 disk = 0; FILETIME dft;
    if (!frr_sys_path("arial.ttf", p, MAX_PATH) || !frr_disk_info(p, &disk, &dft)) { t_skip("arial.ttf not installed"); return; }
    IDWriteFontFaceReference *r = NULL;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, &dft, 0, SIM_NONE, &r), S_OK);
    if (r) {
        FILETIME ft = { 1, 2 };
        CHECK_HR(IDWriteFontFaceReference_GetFileTime(r, &ft), S_OK);
        EXPECT_EQ(frr_ft64(ft), frr_ft64(dft));
        EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r), disk);
    }
    DW_RELEASE(r);
}

/* ---- locality ----------------------------------------------------------------------------------------------------------------- */
TEST(locality_custom_loader_is_local) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        int k;
        for (k = 0; k < 3; k++) EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_LOCAL);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(locality_system_files_are_local) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    static const char *const names[] = { "arial.ttf", "times.ttf", "cour.ttf", "segoeui.ttf", "tahoma.ttf", "msgothic.ttc", "bahnschrift.ttf" };
    size_t i; int any = 0;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        WCHAR p[MAX_PATH]; IDWriteFontFaceReference *r = NULL;
        if (!frr_sys_path(names[i], p, MAX_PATH)) continue;
        CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, 0, SIM_NONE, &r), S_OK);
        if (!r) continue;
        any = 1;
        t_check(IDWriteFontFaceReference_GetLocality(r) == DWRITE_LOCALITY_LOCAL, __FILE__, __LINE__, "local", "%s", names[i]);
        DW_RELEASE(r);
    }
    if (!any) t_skip("no system font files found");
}
TEST(locality_remote_loader_answers_per_call) {
    FRR_OPEN(x);
    frr_remote *rm = frr_remote_new(x.ld, DWRITE_LOCALITY_REMOTE);
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *r = NULL;
    HRESULT hr = frr_remote_register(rm, x.f);
    if (SUCCEEDED(hr) && SUCCEEDED(IDWriteFactory_CreateCustomFontFileReference(x.f, &x.key, sizeof x.key, (IDWriteFontFileLoader *)rm, &file)) && file &&
        SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, file, 0, SIM_NONE, &r)) && r) {
        static const DWRITE_LOCALITY seq[] = { DWRITE_LOCALITY_REMOTE, DWRITE_LOCALITY_PARTIAL, DWRITE_LOCALITY_LOCAL, DWRITE_LOCALITY_PARTIAL, DWRITE_LOCALITY_REMOTE };
        size_t i;
        for (i = 0; i < sizeof seq / sizeof seq[0]; i++) {
            LONG before = rm->n_locality;
            rm->locality = seq[i];
            DWRITE_LOCALITY got = IDWriteFontFaceReference_GetLocality(r);
            t_check(got == seq[i], __FILE__, __LINE__, "locality", "step %u want %d got %d", (unsigned)i, (int)seq[i], (int)got);
            t_check(rm->n_locality == before + 1, __FILE__, __LINE__, "loader asked once", "step %u calls %ld->%ld", (unsigned)i, before, rm->n_locality);
        }
        EXPECT_EQ(rm->last_key_size, 4);
        EXPECT_EQ(*(UINT32 *)rm->last_key, x.key);
    } else t_skip("cannot build a reference over the remote-flavoured loader: %s", dw_hr_name(hr));
    DW_RELEASE(r); DW_RELEASE(file);
    frr_remote_unregister(rm); dwx_release(rm);
    frr_close(&x);
}
TEST(locality_remote_loader_value_is_passed_through_unvalidated) {
    FRR_OPEN(x);
    frr_remote *rm = frr_remote_new(x.ld, DWRITE_LOCALITY_PARTIAL);
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *r = NULL;
    if (SUCCEEDED(frr_remote_register(rm, x.f)) && SUCCEEDED(IDWriteFactory_CreateCustomFontFileReference(x.f, &x.key, sizeof x.key, (IDWriteFontFileLoader *)rm, &file)) &&
        file && SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, file, 0, SIM_NONE, &r)) && r) {
        EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_PARTIAL);
        rm->locality = (DWRITE_LOCALITY)7;      /* out-of-range enum values are not filtered by the reference */
        DWRITE_LOCALITY got = IDWriteFontFaceReference_GetLocality(r);
        t_note("loader answered 7 -> reference reports %d", (int)got);
        CHECK(got == (DWRITE_LOCALITY)7);
    } else t_skip("cannot build a reference over the remote-flavoured loader");
    DW_RELEASE(r); DW_RELEASE(file);
    frr_remote_unregister(rm); dwx_release(rm);
    frr_close(&x);
}
TEST(locality_remote_loader_sizes_come_from_inner_stream) {
    FRR_OPEN(x);
    size_t n; uint8_t *b = ttf_build_default(&n);
    frr_remote *rm = frr_remote_new(x.ld, DWRITE_LOCALITY_PARTIAL);
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *r = NULL;
    if (SUCCEEDED(frr_remote_register(rm, x.f)) && SUCCEEDED(IDWriteFactory_CreateCustomFontFileReference(x.f, &x.key, sizeof x.key, (IDWriteFontFileLoader *)rm, &file)) &&
        file && SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, file, 0, SIM_NONE, &r)) && r) {
        /* The wrapped stream has no IDWriteRemoteFontFileStream, so GetLocalFileSize degrades to GetFileSize. */
        EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r), (UINT64)n);
        EXPECT_EQ(IDWriteFontFaceReference_GetLocalFileSize(r), (UINT64)n);
        CHECK(rm->n_create >= 1);
    } else t_skip("cannot build a reference over the remote-flavoured loader");
    DW_RELEASE(r); DW_RELEASE(file);
    frr_remote_unregister(rm); dwx_release(rm); free(b);
    frr_close(&x);
}

/* ---- Equals ----------------------------------------------------------------------------------------------------------------------- */
TEST(equals_null_and_self) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        BOOL nul = IDWriteFontFaceReference_Equals(r, NULL), self = IDWriteFontFaceReference_Equals(r, r);
        EXPECT_EQ(nul, 0);
        EXPECT_EQ(self, 1);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(equals_matrix_index_and_simulations) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r[3][4]; int i, s, j, t;
    memset(r, 0, sizeof r);
    for (i = 0; i < 3; i++) for (s = 0; s < 4; s++) r[i][s] = frr_ref(&x, (UINT32)i, (DWRITE_FONT_SIMULATIONS)s);
    for (i = 0; i < 3; i++) for (s = 0; s < 4; s++) for (j = 0; j < 3; j++) for (t = 0; t < 4; t++) {
        if (!r[i][s] || !r[j][t]) continue;
        BOOL got = IDWriteFontFaceReference_Equals(r[i][s], r[j][t]);
        BOOL want = (i == j && s == t);
        t_check(got == want, __FILE__, __LINE__, "equals", "(%d,%d) vs (%d,%d): got %d want %d", i, s, j, t, got, want);
    }
    for (i = 0; i < 3; i++) for (s = 0; s < 4; s++) DW_RELEASE(r[i][s]);
    frr_close(&x);
}
TEST(equals_distinct_file_objects_same_key) {
    FRR_OPEN(x);
    IDWriteFontFile *f2 = frr_file(&x); IDWriteFontFaceReference *a = frr_ref(&x, 0, SIM_NONE), *b = NULL;
    if (f2 && a && SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, f2, 0, SIM_NONE, &b)) && b) {
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, b), 1);       /* loader identity + key bytes decide, not the IDWriteFontFile object */
        EXPECT_EQ(IDWriteFontFaceReference_Equals(b, a), 1);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f2);
    frr_close(&x);
}
TEST(equals_different_key_same_loader) {
    FRR_OPEN(x);
    size_t n; uint8_t *bl = ttf_build_default(&n);
    UINT32 k2 = dwx_loader_add(x.ld, bl, n);                        /* identical bytes, different key */
    IDWriteFontFile *f2 = NULL; IDWriteFontFaceReference *a = frr_ref(&x, 0, SIM_NONE), *b = NULL;
    if (a && SUCCEEDED(dwx_loader_file(x.ld, x.f, k2, &f2)) && f2 && SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, f2, 0, SIM_NONE, &b)) && b) {
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, b), 0);
        EXPECT_EQ(IDWriteFontFaceReference_Equals(b, a), 0);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f2); free(bl);
    frr_close(&x);
}
TEST(equals_same_bytes_different_loader) {
    FRR_OPEN(x);
    dwx_mem_loader *l2 = dwx_loader_new();
    size_t n; uint8_t *bl = ttf_build_default(&n);
    UINT32 k2 = dwx_loader_add(l2, bl, n);
    IDWriteFontFile *f2 = NULL; IDWriteFontFaceReference *a = frr_ref(&x, 0, SIM_NONE), *b = NULL;
    if (a && SUCCEEDED(dwx_loader_register(l2, x.f)) && SUCCEEDED(dwx_loader_file(l2, x.f, k2, &f2)) && f2 &&
        SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, f2, 0, SIM_NONE, &b)) && b) {
        CHECK(k2 == x.key);                                           /* same 4-byte key value, different loader object */
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, b), 0);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f2);
    dwx_loader_unregister(l2); dwx_release(l2); free(bl);
    frr_close(&x);
}
TEST(equals_path_references) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; UINT64 sz; FILETIME ft;
    if (!frr_sys_path("arial.ttf", p, MAX_PATH) || !frr_disk_info(p, &sz, &ft)) { t_skip("arial.ttf not installed"); return; }
    IDWriteFontFaceReference *a = NULL, *b = NULL, *c = NULL, *d = NULL, *e = NULL;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, 0, SIM_NONE, &a), S_OK);
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, 0, SIM_NONE, &b), S_OK);
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, 0, SIM_BOLD, &c), S_OK);
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, 1, SIM_NONE, &d), S_OK);
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, L"C:\\dwritet_other\\arial.ttf", NULL, 0, SIM_NONE, &e), S_OK);
    if (a && b && c && d && e) {
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, b), 1);
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, c), 0);
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, d), 0);
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, e), 0);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c); DW_RELEASE(d); DW_RELEASE(e);
}
TEST(equals_path_reference_with_and_without_time) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; UINT64 sz; FILETIME ft;
    if (!frr_sys_path("arial.ttf", p, MAX_PATH) || !frr_disk_info(p, &sz, &ft)) { t_skip("arial.ttf not installed"); return; }
    IDWriteFontFaceReference *a = NULL, *b = NULL, *c = NULL;
    FILETIME other = frr_ft_from64(frr_ft64(ft) + 1);
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, &ft, 0, SIM_NONE, &a), S_OK);
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, &ft, 0, SIM_NONE, &b), S_OK);
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, &other, 0, SIM_NONE, &c), S_OK);
    if (a && b && c) {
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, b), 1);
        t_note("Equals(same path, time T vs T+1 tick) = %d", IDWriteFontFaceReference_Equals(a, c));
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
}
TEST(equals_is_symmetric_and_transitive) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *a = frr_ref(&x, 0, SIM_OBL), *b = frr_ref(&x, 0, SIM_OBL), *c = frr_ref(&x, 0, SIM_OBL);
    if (a && b && c) {
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, b), 1);
        EXPECT_EQ(IDWriteFontFaceReference_Equals(b, a), 1);
        EXPECT_EQ(IDWriteFontFaceReference_Equals(b, c), 1);
        EXPECT_EQ(IDWriteFontFaceReference_Equals(a, c), 1);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    frr_close(&x);
}

/* ---- CreateFontFace ------------------------------------------------------------------------------------------------------------------- */
TEST(createface_basic_properties) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)8;
    if (r) {
        CHECK_HR(IDWriteFontFaceReference_CreateFontFace(r, &face), S_OK);
        CHECK(face != NULL && face != (IDWriteFontFace3 *)(ULONG_PTR)8);
        if (face && face != (IDWriteFontFace3 *)(ULONG_PTR)8) {
            EXPECT_REFS(face, 1);
            EXPECT_EQ(IDWriteFontFace3_GetIndex(face), 0);
            EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), SIM_NONE);
            EXPECT_EQ(IDWriteFontFace3_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
            EXPECT_EQ(IDWriteFontFace3_GetGlyphCount(face), TTF_DEF_NGLYPHS);
            DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics((IDWriteFontFace *)face, &m);
            EXPECT_EQ(m.designUnitsPerEm, TTF_DEF_UPM);
        } else face = NULL;
    }
    DW_RELEASE(face); DW_RELEASE(r);
    frr_close(&x);
}
TEST(createface_supports_face_interfaces_up_to_5) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFace3 *face = NULL;
    if (r && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &face)) && face) {
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontFace, 1), DW_QI_ROW(IDWriteFontFace1, 1), DW_QI_ROW(IDWriteFontFace2, 1), DW_QI_ROW(IDWriteFontFace3, 1),
                             DW_QI_ROW(IDWriteFontFace4, 1), DW_QI_ROW(IDWriteFontFace5, 1), DW_QI_ROW(IDWriteFontFaceReference, -1), DW_QI_ROW(IDWriteFontResource, -1) };
        DW_CHECK_QI(face, rows);
    }
    DW_RELEASE(face); DW_RELEASE(r);
    frr_close(&x);
}
TEST(createface_simulations_follow_the_reference) {
    FRR_OPEN(x);
    UINT32 s;
    for (s = 0; s <= 3; s++) {
        IDWriteFontFaceReference *r = frr_ref(&x, 0, (DWRITE_FONT_SIMULATIONS)s);
        IDWriteFontFace3 *face = NULL;
        if (r && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &face)) && face)
            t_check(IDWriteFontFace3_GetSimulations(face) == (DWRITE_FONT_SIMULATIONS)s, __FILE__, __LINE__, "face sims", "ref sims %u -> %u", s, IDWriteFontFace3_GetSimulations(face));
        DW_RELEASE(face); DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(createface_with_simulations_override_matrix) {
    FRR_OPEN(x);
    UINT32 from, to;
    for (from = 0; from <= 3; from++) for (to = 0; to <= 3; to++) {
        IDWriteFontFaceReference *r = frr_ref(&x, 0, (DWRITE_FONT_SIMULATIONS)from);
        IDWriteFontFace3 *face = NULL;
        if (!r) continue;
        HRESULT hr = IDWriteFontFaceReference_CreateFontFaceWithSimulations(r, (DWRITE_FONT_SIMULATIONS)to, &face);
        t_check(hr == S_OK, __FILE__, __LINE__, "CreateFontFaceWithSimulations", "%u->%u: %s", from, to, dw_hr_name(hr));
        if (face) t_check(IDWriteFontFace3_GetSimulations(face) == (DWRITE_FONT_SIMULATIONS)to, __FILE__, __LINE__, "override", "%u->%u got %u", from, to, IDWriteFontFace3_GetSimulations(face));
        t_check(IDWriteFontFaceReference_GetSimulations(r) == (DWRITE_FONT_SIMULATIONS)from, __FILE__, __LINE__, "ref unchanged", "%u->%u", from, to);
        DW_RELEASE(face); DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(createface_with_simulations_invalid_values_are_reported) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        static const UINT32 bad[] = { 4, 8, 0xFFFFFFFF };
        size_t i;
        for (i = 0; i < 3; i++) {
            IDWriteFontFace3 *face = NULL;
            HRESULT hr = IDWriteFontFaceReference_CreateFontFaceWithSimulations(r, (DWRITE_FONT_SIMULATIONS)bad[i], &face);
            t_note("CreateFontFaceWithSimulations(0x%x) -> %s face %p sims %d", bad[i], dw_hr_name(hr), (void *)face, face ? (int)IDWriteFontFace3_GetSimulations(face) : -1);
            if (FAILED(hr)) CHECK(face == NULL);
            DW_RELEASE(face);
        }
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(createface_repeated_calls_produce_equivalent_faces) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFace3 *a = NULL, *b = NULL;
    if (r && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &a)) && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &b)) && a && b) {
        t_note("two faces from one reference: same pointer %d, Equals %d", a == b, frr_face_equals((IUnknown *)a, (IUnknown *)b));
        UINT32 cp[4] = { 'A', 'z', '5', ' ' }; UINT16 ga[4], gb[4];
        CHECK_HR(IDWriteFontFace3_GetGlyphIndices(a, cp, 4, ga), S_OK);
        CHECK_HR(IDWriteFontFace3_GetGlyphIndices(b, cp, 4, gb), S_OK);
        CHECK(!memcmp(ga, gb, sizeof ga));
        EXPECT_EQ(ga[0], ttf_default_gid('A'));
        EXPECT_EQ(ga[3], TTF_GID_SPACE);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(r);
    frr_close(&x);
}
TEST(createface_matches_factory_created_face) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFace3 *a = NULL; IDWriteFontFace *b = NULL;
    if (r && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &a)) && a &&
        SUCCEEDED(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &x.file, 0, SIM_NONE, &b)) && b) {
        EXPECT_EQ(IDWriteFontFace3_GetGlyphCount(a), IDWriteFontFace_GetGlyphCount(b));
        DWRITE_FONT_METRICS ma, mb; IDWriteFontFace_GetMetrics((IDWriteFontFace *)a, &ma); IDWriteFontFace_GetMetrics(b, &mb);
        CHECK(!memcmp(&ma, &mb, sizeof ma));
        t_note("reference face vs factory face: Equals %d same pointer %d", frr_face_equals((IUnknown *)a, (IUnknown *)b), (void *)a == (void *)b);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(r);
    frr_close(&x);
}
TEST(createface_index_out_of_range_fails) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 3, SIM_NONE);
    if (r) {
        IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)8;
        HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
        t_note("CreateFontFace(index 3 of a single-face font) -> %s", dw_hr_name(hr));
        CHECK_HR_FAILED(hr);
        FRR_ZERO(face);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(createface_loader_failure_propagates) {
    FRR_OPEN(x);
    size_t n; uint8_t *b = ttf_build_default(&n);
    UINT32 k2 = dwx_loader_add(x.ld, b, n);
    x.ld->fail_create = E_FAIL;
    IDWriteFontFile *f2 = NULL; IDWriteFontFaceReference *r = NULL;
    if (SUCCEEDED(dwx_loader_file(x.ld, x.f, k2, &f2)) && f2 && SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, f2, 0, SIM_NONE, &r)) && r) {
        IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)8;
        HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
        t_note("CreateFontFace with a failing loader -> %s", dw_hr_name(hr));
        CHECK_HR_FAILED(hr);
        FRR_ZERO(face);
        x.ld->fail_create = 0;
    } else t_note("file/reference creation failed up front with a failing loader");
    x.ld->fail_create = 0;
    DW_RELEASE(r); DW_RELEASE(f2); free(b);
    frr_close(&x);
}
TEST(createface_ttc_members) {
    size_t n1, n2, nt; uint8_t *b1 = ttf_build_styled("TtcA", 400, 5, 0, &n1), *b2 = ttf_build_styled("TtcB", 700, 5, 0, &n2);
    const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &nt);
    frr_fix x;
    if (b1 && b2 && ttc && frr_open(&x, ttc, nt)) {
        static const DWRITE_FONT_WEIGHT wt[2] = { DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_WEIGHT_BOLD };
        UINT32 i;
        for (i = 0; i < 2; i++) {
            IDWriteFontFaceReference *r = frr_ref(&x, i, SIM_NONE);
            IDWriteFontFace3 *face = NULL;
            if (r && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &face)) && face) {
                EXPECT_EQ(IDWriteFontFace3_GetIndex(face), i);
                t_check(IDWriteFontFace3_GetWeight(face) == wt[i], __FILE__, __LINE__, "weight", "member %u: %d", i, (int)IDWriteFontFace3_GetWeight(face));
                EXPECT_EQ(IDWriteFontFace3_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
            }
            DW_RELEASE(face); DW_RELEASE(r);
        }
    } else if (!(b1 && b2 && ttc)) t_skip("ttf_build_ttc failed");
    frr_close(&x);
    free(b1); free(b2); free(ttc);
}
TEST(createface_arial_properties) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 0, SIM_NONE);
    IDWriteFontFace3 *face = NULL;
    if (r && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &face)) && face) {
        DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics((IDWriteFontFace *)face, &m);
        EXPECT_EQ(m.designUnitsPerEm, 2048);
        EXPECT_EQ(IDWriteFontFace3_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
        EXPECT_EQ(IDWriteFontFace3_GetWeight(face), DWRITE_FONT_WEIGHT_REGULAR);
        EXPECT_EQ(IDWriteFontFace3_GetStretch(face), DWRITE_FONT_STRETCH_NORMAL);
        EXPECT_EQ(IDWriteFontFace3_GetStyle(face), DWRITE_FONT_STYLE_NORMAL);
        UINT32 cp = 'A'; UINT16 g = 0;
        CHECK_HR(IDWriteFontFace3_GetGlyphIndices(face, &cp, 1, &g), S_OK);
        CHECK(g != 0);
    }
    DW_RELEASE(face); DW_RELEASE(r);
}
TEST(createface_arial_bold_simulation_changes_face_weight) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 0, SIM_NONE);
    IDWriteFontFace3 *plain = NULL, *bold = NULL;
    if (r && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &plain)) && SUCCEEDED(IDWriteFontFaceReference_CreateFontFaceWithSimulations(r, SIM_BOLD, &bold)) && plain && bold) {
        EXPECT_EQ(IDWriteFontFace3_GetSimulations(plain), SIM_NONE);
        EXPECT_EQ(IDWriteFontFace3_GetSimulations(bold), SIM_BOLD);
        t_note("Arial regular weight %d; simulated bold face weight %d", (int)IDWriteFontFace3_GetWeight(plain), (int)IDWriteFontFace3_GetWeight(bold));
        CHECK(IDWriteFontFace3_GetWeight(bold) >= IDWriteFontFace3_GetWeight(plain));
        CHECK(frr_face_equals((IUnknown *)plain, (IUnknown *)bold) != 1);
    }
    DW_RELEASE(plain); DW_RELEASE(bold); DW_RELEASE(r);
}

/* ---- relations with faces and system fonts --------------------------------------------------------------------------------------------- */
TEST(face_get_font_face_reference_round_trip) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_BOLD);
    IDWriteFontFace3 *face = NULL; IDWriteFontFaceReference *back = (IDWriteFontFaceReference *)(ULONG_PTR)8;
    if (r && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &face)) && face) {
        CHECK_HR(IDWriteFontFace3_GetFontFaceReference(face, &back), S_OK);
        CHECK(back != NULL && back != (IDWriteFontFaceReference *)(ULONG_PTR)8);
        if (back && back != (IDWriteFontFaceReference *)(ULONG_PTR)8) {
            EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(back), 0);
            EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(back), SIM_BOLD);
            EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(back), IDWriteFontFaceReference_GetFileSize(r));
            EXPECT_EQ(IDWriteFontFaceReference_GetLocality(back), DWRITE_LOCALITY_LOCAL);
            t_note("face->GetFontFaceReference Equals original: %d", IDWriteFontFaceReference_Equals(back, r));
            IDWriteFontFile *ff = NULL; IDWriteFontFileLoader *ld = NULL;
            if (SUCCEEDED(IDWriteFontFaceReference_GetFontFile(back, &ff)) && ff && SUCCEEDED(IDWriteFontFile_GetLoader(ff, &ld))) CHECK(ld == (IDWriteFontFileLoader *)x.ld);
            DW_RELEASE(ld); DW_RELEASE(ff);
        } else back = NULL;
    }
    DW_RELEASE(back); DW_RELEASE(face); DW_RELEASE(r);
    frr_close(&x);
}
TEST(sysfont_reference_from_font_object) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no factory"); return; }
    IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Arial not installed"); return; }
    IDWriteFontFace3 *f3 = NULL; IDWriteFontFaceReference *r = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3)) && f3 && SUCCEEDED(IDWriteFontFace3_GetFontFaceReference(f3, &r)) && r) {
        EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_LOCAL);
        EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), SIM_NONE);
        CHECK(IDWriteFontFaceReference_GetFileSize(r) > 0);
        EXPECT_EQ(IDWriteFontFaceReference_GetLocalFileSize(r), IDWriteFontFaceReference_GetFileSize(r));
        IDWriteFontFile *ff = NULL; IDWriteFontFileLoader *ld = NULL; IDWriteLocalFontFileLoader *loc = NULL;
        if (SUCCEEDED(IDWriteFontFaceReference_GetFontFile(r, &ff)) && ff && SUCCEEDED(IDWriteFontFile_GetLoader(ff, &ld)) && ld &&
            SUCCEEDED(IDWriteFontFileLoader_QueryInterface(ld, &IID_IDWriteLocalFontFileLoader, (void **)&loc)) && loc) {
            const void *key; UINT32 ks, len = 0; WCHAR path[MAX_PATH * 2];
            if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(ff, &key, &ks)) && SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(loc, key, ks, &len)) && len < MAX_PATH * 2 - 1 &&
                SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathFromKey(loc, key, ks, path, len + 1))) {
                CHECK(wide_ends_with_i(path, L"arial.ttf"));
                UINT64 disk = 0; FILETIME dft, ft;
                if (frr_disk_info(path, &disk, &dft)) {
                    EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r), disk);
                    CHECK_HR(IDWriteFontFaceReference_GetFileTime(r, &ft), S_OK);
                    EXPECT_EQ(frr_ft64(ft), frr_ft64(dft));
                }
            }
        }
        DW_RELEASE(loc); DW_RELEASE(ld); DW_RELEASE(ff);
    } else t_skip("no IDWriteFontFace3 / reference for Arial");
    DW_RELEASE(r); DW_RELEASE(f3); DW_RELEASE(face);
}
TEST(sysfont_reference_equals_path_reference) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFactory *f = dw_shared();
    IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Arial not installed"); return; }
    IDWriteFontFace3 *face3 = NULL; IDWriteFontFaceReference *sys = NULL, *path = frr_sys_ref(f3, "arial.ttf", 0, SIM_NONE);
    if (path && SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&face3)) && SUCCEEDED(IDWriteFontFace3_GetFontFaceReference(face3, &sys)) && sys) {
        t_note("system-font reference Equals path reference (same file, no explicit time): %d / reverse %d", IDWriteFontFaceReference_Equals(sys, path), IDWriteFontFaceReference_Equals(path, sys));
        EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(sys), IDWriteFontFaceReference_GetFontFaceIndex(path));
    }
    DW_RELEASE(sys); DW_RELEASE(path); DW_RELEASE(face3); DW_RELEASE(face);
}
