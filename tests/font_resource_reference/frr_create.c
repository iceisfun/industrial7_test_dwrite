/* Creation of IDWriteFontFaceReference objects: IDWriteFactory3::CreateFontFaceReference_ (file overload) and CreateFontFaceReference
 * (path + last-write-time overload), argument validation, QueryInterface matrix, refcounts and object lifetime.
 *
 * Win10 dwrite.dll (DWriteFactory::CreateFontFaceReference, thunks at !0xd6e80 / !0xd7150, workers !0x2af10 / !0xd6ec0):
 *   - *reference is zeroed first;
 *   - NULL file / NULL path, simulations & ~3 != 0 -> E_INVALIDARG (0x80070057);
 *   - nothing else is validated: the face index, the font file contents and the path are only stored in the reference key and
 *     examined when a face is created from the reference.
 */
#include "frr.h"

#define SIM_BOLD DWRITE_FONT_SIMULATIONS_BOLD
#define SIM_OBL DWRITE_FONT_SIMULATIONS_OBLIQUE

static const UINT32 bad_sims[] = { 4, 5, 6, 7, 8, 0x10, 0x100, 0x10000, 0x40000000, 0x80000000, 0xFFFFFFFC, 0xFFFFFFFF };

/* ---- file overload -------------------------------------------------------------------------------------------------- */
TEST(file_create_basic) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference_(x.f3, x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), S_OK);
    CHECK(r != NULL && r != (IDWriteFontFaceReference *)(ULONG_PTR)8);
    if (r) EXPECT_REFS(r, 1);
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(file_create_null_file_invalidarg) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference_(x.f3, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), FRR_EINVALIDARG);
    FRR_ZERO(r);
    frr_close(&x);
}
TEST(file_create_invalid_simulations) {
    FRR_OPEN(x);
    size_t i;
    for (i = 0; i < sizeof bad_sims / sizeof bad_sims[0]; i++) {
        IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)8;
        HRESULT hr = IDWriteFactory3_CreateFontFaceReference_(x.f3, x.file, 0, (DWRITE_FONT_SIMULATIONS)bad_sims[i], &r);
        t_check(hr == FRR_EINVALIDARG, __FILE__, __LINE__, "bad simulations", "0x%x -> %s", bad_sims[i], dw_hr_name(hr));
        t_check(r == NULL, __FILE__, __LINE__, "out zeroed", "0x%x: out %p", bad_sims[i], (void *)r);
        r = NULL;
    }
    frr_close(&x);
}
TEST(file_create_valid_simulations_roundtrip) {
    FRR_OPEN(x);
    UINT32 s;
    for (s = 0; s <= 3; s++) {
        IDWriteFontFaceReference *r = frr_ref(&x, 0, (DWRITE_FONT_SIMULATIONS)s);
        if (!r) continue;
        t_check(IDWriteFontFaceReference_GetSimulations(r) == (DWRITE_FONT_SIMULATIONS)s, __FILE__, __LINE__, "sim roundtrip", "sims %u", s);
        DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(file_create_index_roundtrip) {
    FRR_OPEN(x);
    static const UINT32 idx[] = { 0, 1, 2, 3, 7, 15, 255, 1000, 65535 };
    size_t i;
    for (i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        IDWriteFontFaceReference *r = frr_ref(&x, idx[i], DWRITE_FONT_SIMULATIONS_NONE);
        if (!r) continue;
        UINT32 got = IDWriteFontFaceReference_GetFontFaceIndex(r);
        t_check(got == idx[i], __FILE__, __LINE__, "index roundtrip", "stored %u got %u", idx[i], got);
        DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(file_create_huge_index_is_accepted_lazily) {
    FRR_OPEN(x);
    /* The index is not range-checked at creation (no font data is read); whether the stored value survives is version specific. */
    IDWriteFontFaceReference *r = NULL;
    HRESULT hr = IDWriteFactory3_CreateFontFaceReference_(x.f3, x.file, 0xFFFFFFFF, DWRITE_FONT_SIMULATIONS_NONE, &r);
    t_note("CreateFontFaceReference_(index 0xFFFFFFFF) -> %s, GetFontFaceIndex %u", dw_hr_name(hr), r ? IDWriteFontFaceReference_GetFontFaceIndex(r) : 0);
    CHECK(SUCCEEDED(hr) ? r != NULL : r == NULL);
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(file_create_does_not_retain_the_file) {
    FRR_OPEN(x);
    ULONG before = dw_refcount((IUnknown *)x.file);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    ULONG during = dw_refcount((IUnknown *)x.file);
    EXPECT_EQ(during, before);       /* the reference stores loader + key bytes, not the IDWriteFontFile */
    DW_RELEASE(r);
    EXPECT_REFS(x.file, before);
    frr_close(&x);
}
TEST(file_create_two_references_are_distinct_objects) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *a = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE), *b = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    CHECK(a && b && a != b);
    if (a && b) { EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); }
    DW_RELEASE(a); DW_RELEASE(b);
    frr_close(&x);
}
TEST(file_create_survives_file_release) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_BOLD);
    if (!r) { frr_close(&x); return; }
    DW_RELEASE(x.file);
    EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 0);
    EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), SIM_BOLD);
    IDWriteFontFace3 *face = NULL;
    CHECK_HR(IDWriteFontFaceReference_CreateFontFace(r, &face), S_OK);   /* the loader is still registered, the key is enough */
    CHECK(face != NULL);
    DW_RELEASE(face); DW_RELEASE(r);
    frr_close(&x);
}
TEST(file_create_after_loader_unregistered_fails_at_face_creation) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (!r) { frr_close(&x); return; }
    DW_RELEASE(x.file);
    dwx_loader_unregister(x.ld);
    IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)8;
    HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
    t_note("CreateFontFace after UnregisterFontFileLoader -> %s", dw_hr_name(hr));
    CHECK_HR_FAILED(hr);
    FRR_ZERO(face);
    DW_RELEASE(face); DW_RELEASE(r);
    frr_close(&x);
}
TEST(file_create_from_ttc_member_indices) {
    size_t n1, n2, nt; uint8_t *b1 = ttf_build_styled("TtcA", 400, 5, 0, &n1), *b2 = ttf_build_styled("TtcB", 700, 5, 0, &n2);
    const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &nt);
    if (b1 && b2 && ttc) {
        frr_fix x;
        if (frr_open(&x, ttc, nt)) {
            UINT32 i;
            for (i = 0; i < 2; i++) {
                IDWriteFontFaceReference *r = frr_ref(&x, i, DWRITE_FONT_SIMULATIONS_NONE);
                if (!r) continue;
                EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), i);
                DW_RELEASE(r);
            }
        }
        frr_close(&x);
    } else t_skip("ttf_build_ttc failed");
    free(b1); free(b2); free(ttc);
}
TEST(file_create_two_files_same_key_give_independent_references) {
    FRR_OPEN(x);
    IDWriteFontFile *f2 = frr_file(&x);
    IDWriteFontFaceReference *a = NULL, *b = NULL;
    if (f2) {
        CHECK_HR(IDWriteFactory3_CreateFontFaceReference_(x.f3, x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &a), S_OK);
        CHECK_HR(IDWriteFactory3_CreateFontFaceReference_(x.f3, f2, 0, DWRITE_FONT_SIMULATIONS_NONE, &b), S_OK);
        CHECK(a && b && a != b);
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f2);
    frr_close(&x);
}
TEST(file_create_via_all_newer_factory_interfaces) {
    /* The same method is reachable through every IDWriteFactory3+ interface and behaves identically. */
    FRR_OPEN(x);
    struct { int ver; } t[] = { { 3 }, { 4 }, { 5 }, { 6 }, { 7 } };
    size_t i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        void *fv = NULL;
        if (FAILED(IDWriteFactory_QueryInterface(x.f, dw_factory_iid(t[i].ver), &fv)) || !fv) { t_note("IDWriteFactory%d unsupported", t[i].ver); continue; }
        IDWriteFactory3 *as3 = (IDWriteFactory3 *)fv;      /* IDWriteFactory3's vtable prefix is shared by newer factories */
        IDWriteFontFaceReference *r = NULL;
        CHECK_HR(IDWriteFactory3_CreateFontFaceReference_(as3, x.file, 0, SIM_OBL, &r), S_OK);
        if (r) EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), SIM_OBL);
        DW_RELEASE(r);
        CHECK_HR(IDWriteFactory3_CreateFontFaceReference_(as3, NULL, 0, 0, &r), FRR_EINVALIDARG);
        FRR_ZERO(r);
        IUnknown_Release((IUnknown *)fv);
    }
    frr_close(&x);
}
TEST(file_create_garbage_file_content_is_accepted_lazily) {
    /* Creation never reads the font file, so even a blob that is not a font yields a reference. */
    uint8_t junk[64]; memset(junk, 0x5A, sizeof junk);
    frr_fix x;
    if (frr_open(&x, junk, sizeof junk)) {
        IDWriteFontFaceReference *r = NULL;
        HRESULT hr = IDWriteFactory3_CreateFontFaceReference_(x.f3, x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &r);
        CHECK_HR(hr, S_OK);
        if (r) {
            IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)8;
            hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
            t_note("CreateFontFace over a non-font blob -> %s", dw_hr_name(hr));
            CHECK_HR_FAILED(hr);
            FRR_ZERO(face);
            DW_RELEASE(face);
        }
        DW_RELEASE(r);
    }
    frr_close(&x);
}

/* ---- path overload ---------------------------------------------------------------------------------------------------- */
TEST(path_create_basic_arial) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (!r) return;
    EXPECT_REFS(r, 1);
    EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 0);
    EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), DWRITE_FONT_SIMULATIONS_NONE);
    DW_RELEASE(r);
}
TEST(path_create_null_path_invalidarg) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, NULL, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), FRR_EINVALIDARG);
    FRR_ZERO(r);
    FILETIME ft = frr_ft_from64(0x01D5A3B2C4D5E6F7ull);
    r = (IDWriteFontFaceReference *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, NULL, &ft, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), FRR_EINVALIDARG);
    FRR_ZERO(r);
}
TEST(path_create_invalid_simulations) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; if (!frr_sys_path("arial.ttf", p, MAX_PATH)) { t_skip("arial.ttf not installed"); return; }
    size_t i;
    for (i = 0; i < sizeof bad_sims / sizeof bad_sims[0]; i++) {
        IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)8;
        HRESULT hr = IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, 0, (DWRITE_FONT_SIMULATIONS)bad_sims[i], &r);
        t_check(hr == FRR_EINVALIDARG, __FILE__, __LINE__, "bad simulations", "0x%x -> %s", bad_sims[i], dw_hr_name(hr));
        t_check(r == NULL, __FILE__, __LINE__, "out zeroed", "0x%x", bad_sims[i]);
        r = NULL;
    }
}
TEST(path_create_valid_simulations_and_indices) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; if (!frr_sys_path("arial.ttf", p, MAX_PATH)) { t_skip("arial.ttf not installed"); return; }
    UINT32 s, i;
    for (s = 0; s <= 3; s++) for (i = 0; i < 3; i++) {
        IDWriteFontFaceReference *r = NULL;
        CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, i, (DWRITE_FONT_SIMULATIONS)s, &r), S_OK);
        if (!r) continue;
        t_check(IDWriteFontFaceReference_GetSimulations(r) == (DWRITE_FONT_SIMULATIONS)s, __FILE__, __LINE__, "sim", "s=%u i=%u", s, i);
        t_check(IDWriteFontFaceReference_GetFontFaceIndex(r) == i, __FILE__, __LINE__, "index", "s=%u i=%u", s, i);
        DW_RELEASE(r);
    }
}
TEST(path_create_missing_file_is_accepted_lazily) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    /* The path is serialised into the key without touching the disk; failure shows up when a face is created. */
    const WCHAR *missing = L"C:\\dwritet_no_such_dir\\no_such_font.ttf";
    IDWriteFontFaceReference *r = NULL;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, missing, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), S_OK);
    if (r) {
        IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)8;
        HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
        t_note("CreateFontFace for a missing path -> %s (DWRITE_E_FILENOTFOUND is 0x88985003)", dw_hr_name(hr));
        CHECK_HR_FAILED(hr);
        FRR_ZERO(face);
        DW_RELEASE(face);
    }
    DW_RELEASE(r);
}
TEST(path_create_with_matching_write_time) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; FILETIME ft; UINT64 sz;
    if (!frr_sys_path("arial.ttf", p, MAX_PATH) || !frr_disk_info(p, &sz, &ft)) { t_skip("arial.ttf not available"); return; }
    IDWriteFontFaceReference *r = NULL;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, &ft, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), S_OK);
    if (r) {
        IDWriteFontFace3 *face = NULL;
        CHECK_HR(IDWriteFontFaceReference_CreateFontFace(r, &face), S_OK);
        CHECK(face != NULL);
        DW_RELEASE(face);
    }
    DW_RELEASE(r);
}
TEST(path_create_with_stale_write_time) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    WCHAR p[MAX_PATH]; FILETIME ft; UINT64 sz;
    if (!frr_sys_path("arial.ttf", p, MAX_PATH) || !frr_disk_info(p, &sz, &ft)) { t_skip("arial.ttf not available"); return; }
    FILETIME stale = frr_ft_from64(frr_ft64(ft) + 10000000ull * 3600);     /* one hour later than the real write time */
    IDWriteFontFaceReference *r = NULL;
    CHECK_HR(IDWriteFactory3_CreateFontFaceReference(f3, p, &stale, 0, DWRITE_FONT_SIMULATIONS_NONE, &r), S_OK);
    if (r) {
        IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)8;
        HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
        t_note("CreateFontFace with a mismatching last-write time -> %s", dw_hr_name(hr));
        if (FAILED(hr)) FRR_ZERO(face);
        if (SUCCEEDED(hr)) DW_RELEASE(face);
    }
    DW_RELEASE(r);
}
TEST(path_create_out_of_range_index_fails_at_face_creation) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 5, DWRITE_FONT_SIMULATIONS_NONE);
    if (!r) return;
    EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 5);
    IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)8;
    HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
    t_note("CreateFontFace(arial.ttf, index 5) -> %s", dw_hr_name(hr));
    CHECK_HR_FAILED(hr);
    FRR_ZERO(face);
    DW_RELEASE(face); DW_RELEASE(r);
}
TEST(path_create_ttc_member_indices) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    UINT32 i; int made = 0;
    for (i = 0; i < 3; i++) {
        IDWriteFontFaceReference *r = frr_sys_ref(f3, "msgothic.ttc", i, DWRITE_FONT_SIMULATIONS_NONE);
        if (!r) return;
        IDWriteFontFace3 *face = NULL;
        HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
        CHECK_HR(hr, S_OK);
        if (face) { EXPECT_EQ(IDWriteFontFace3_GetIndex(face), i); made++; }
        DW_RELEASE(face); DW_RELEASE(r);
    }
    t_note("msgothic.ttc members opened: %d", made);
}
TEST(path_create_survives_factory_release) {
    IDWriteFactory *own = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!own) { t_skip("no isolated factory"); return; }
    IDWriteFactory3 *f3 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(own, &IID_IDWriteFactory3, (void **)&f3))) { IDWriteFactory_Release(own); t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 0, SIM_BOLD);
    IDWriteFactory3_Release(f3); IDWriteFactory_Release(own);
    if (!r) return;
    /* the reference keeps its factory alive */
    EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), SIM_BOLD);
    IDWriteFontFace3 *face = NULL;
    CHECK_HR(IDWriteFontFaceReference_CreateFontFace(r, &face), S_OK);
    CHECK(face != NULL);
    DW_RELEASE(face); DW_RELEASE(r);
}

/* ---- QueryInterface / refcounts -------------------------------------------------------------------------------------------- */
TEST(ref_qi_matrix) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (r) {
        dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteFontFaceReference, 1), DW_QI_ROW(IDWriteFontFaceReference1, 1),
            DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontFace3, 0), DW_QI_ROW(IDWriteFontFace5, 0),
            DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontResource, 0), DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFontSet, 0),
            DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFactory3, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0),
            DW_QI_ROW(IDWriteFontDownloadQueue, 0), DW_QI_ROW(IDWriteFontDownloadListener, 0), DW_QI_ROW(IDWriteTextFormat, 0),
            DW_QI_ROW(IDWriteFontFileLoader, 0), DW_QI_ROW(IDWriteFontFileStream, 0),
        };
        DW_CHECK_QI(r, rows);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(ref_qi_failure_zeroes_output) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (r) {
        void *pv = (void *)(ULONG_PTR)8;
        HRESULT hr = IDWriteFontFaceReference_QueryInterface(r, &IID_IDWriteFontFile, &pv);
        CHECK_HR(hr, E_NOINTERFACE);
        FRR_ZERO(pv);
        EXPECT_REFS(r, 1);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(ref_qi_to_reference1_shares_the_object) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 1, SIM_BOLD);
    IDWriteFontFaceReference1 *r1 = r ? frr_ref1(r) : NULL;
    CHECK(r1 != NULL);
    if (r && r1) {
        EXPECT_REFS(r, 2);
        CHECK(dw_same_identity((IUnknown *)r, (IUnknown *)r1));
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontFaceIndex(r1), 1);
        EXPECT_EQ(IDWriteFontFaceReference1_GetSimulations(r1), SIM_BOLD);
    }
    DW_RELEASE(r1); DW_RELEASE(r);
    frr_close(&x);
}
TEST(ref_addref_release_sequence) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (r) {
        ULONG a = IDWriteFontFaceReference_AddRef(r), b = IDWriteFontFaceReference_AddRef(r);
        EXPECT_EQ(a, 2); EXPECT_EQ(b, 3);
        ULONG c = IDWriteFontFaceReference_Release(r), d = IDWriteFontFaceReference_Release(r);
        EXPECT_EQ(c, 2); EXPECT_EQ(d, 1);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(ref_iunknown_identity_is_stable) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (r) {
        IUnknown *u1 = NULL, *u2 = NULL;
        CHECK_HR(IDWriteFontFaceReference_QueryInterface(r, &IID_IUnknown, (void **)&u1), S_OK);
        CHECK_HR(IDWriteFontFaceReference_QueryInterface(r, &IID_IUnknown, (void **)&u2), S_OK);
        CHECK(u1 == u2 && u1 != NULL);
        DW_RELEASE(u1); DW_RELEASE(u2);
        EXPECT_REFS(r, 1);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(ref_survives_loader_object_release_while_registered) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (r) {
        IDWriteFontFace3 *f1 = NULL, *f2 = NULL;
        CHECK_HR(IDWriteFontFaceReference_CreateFontFace(r, &f1), S_OK);
        CHECK_HR(IDWriteFontFaceReference_CreateFontFace(r, &f2), S_OK);    /* a reference can be used repeatedly */
        CHECK(f1 && f2);
        DW_RELEASE(f1); DW_RELEASE(f2);
    }
    DW_RELEASE(r);
    frr_close(&x);
}

/* ---- factory 6 axis flavour ---------------------------------------------------------------------------------------------------- */
TEST(axis_create_basic_and_validation) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 450.0f } };
    IDWriteFontFaceReference1 *r = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, av, 1, &r), S_OK);
    CHECK(r != NULL && r != (IDWriteFontFaceReference1 *)(ULONG_PTR)8);
    DW_RELEASE(r);
    r = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory6_CreateFontFaceReference(x.f6, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, av, 1, &r), FRR_EINVALIDARG);
    FRR_ZERO(r);
    r = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, (DWRITE_FONT_SIMULATIONS)4, av, 1, &r), FRR_EINVALIDARG);
    FRR_ZERO(r);
    r = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, &r), S_OK);
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(axis_create_too_many_values_invalidarg) {
    FRR_OPEN_NEED6(x);
    /* The count is limited to 0xFFFF; the array must be real memory because the float->fixed conversion runs before the check. */
    UINT32 n = 0x10000, i;
    DWRITE_FONT_AXIS_VALUE *av = (DWRITE_FONT_AXIS_VALUE *)calloc(n, sizeof *av);
    if (!av) { t_skip("alloc"); frr_close(&x); return; }
    for (i = 0; i < n; i++) av[i].axisTag = DWRITE_FONT_AXIS_TAG_WEIGHT;
    IDWriteFontFaceReference1 *r = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, av, n, &r), FRR_EINVALIDARG);
    FRR_ZERO(r);
    free(av);
    frr_close(&x);
}
TEST(axis_create_survives_multiple_axes_and_simulations) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av[3] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700 }, { DWRITE_FONT_AXIS_TAG_WIDTH, 87.5f }, { DWRITE_FONT_AXIS_TAG_SLANT, -9 } };
    UINT32 s;
    for (s = 0; s <= 3; s++) {
        IDWriteFontFaceReference1 *r = NULL;
        CHECK_HR(IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, (DWRITE_FONT_SIMULATIONS)s, av, 3, &r), S_OK);
        if (!r) continue;
        t_check(IDWriteFontFaceReference1_GetSimulations(r) == (DWRITE_FONT_SIMULATIONS)s, __FILE__, __LINE__, "sim", "%u", s);
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 3);
        DW_RELEASE(r);
    }
    frr_close(&x);
}
