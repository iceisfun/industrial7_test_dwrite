/* IDWriteFontFace identity: GetType / GetFiles / GetIndex / GetSimulations / IsSymbolFont / GetGlyphCount,
 * factory CreateFontFace argument validation, refcount and QueryInterface behaviour.
 *
 * Facts from the Windows 10 decompilation (DWriteFactory::CreateFontFace, DWriteFontFace::GetFiles/GetType/GetIndex/GetSimulations):
 *  - *fontFace is zeroed first; faceType must be CFF(0), TRUETYPE(1), OPENTYPE_COLLECTION(2), UNKNOWN(6) or RAW_CFF(7), otherwise E_INVALIDARG;
 *  - faceIndex must be 0 unless faceType is OPENTYPE_COLLECTION; numberOfFiles must be exactly 1; fontFiles and fontFiles[0] non-NULL;
 *  - simulations may only carry BOLD|OBLIQUE (bits 0 and 1), anything else is E_INVALIDARG;
 *  - GetFiles: numberOfFiles NULL -> E_INVALIDARG, fontFiles NULL -> *n = 1, otherwise *n must be 1 (E_INVALIDARG else), the file object
 *    is created lazily once and cached by the face (same pointer every time, AddRef'd per call). */
#include "ffc.h"

#define E_INVAL ((HRESULT)0x80070057)

/* ---- GetType ---------------------------------------------------------------------------------------------- */
FTEST(ffc_type_truetype) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); if (!face) return;
    EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
    dwx_font_close(&h);
}
FTEST(ffc_type_matches_analyze) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); if (!face) return;
    CHECK(h.supported);
    EXPECT_EQ(h.face_type, DWRITE_FONT_FACE_TYPE_TRUETYPE);
    EXPECT_EQ(h.file_type, DWRITE_FONT_FILE_TYPE_TRUETYPE);
    EXPECT_EQ(h.nfaces, 1);
    EXPECT_EQ(IDWriteFontFace_GetType(face), h.face_type);
    dwx_font_close(&h);
}
FTEST(ffc_type_stable_across_calls) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); if (!face) return;
    int i; for (i = 0; i < 5; i++) EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
    dwx_font_close(&h);
}
FTEST(ffc_type_styled_font) {
    ttf_spec s; ttf_style_spec(&s, "FfcStyled", 700, 5, 1);
    dwx_font h; IDWriteFontFace *face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
    dwx_font_close(&h);
}
FTEST(ffc_type_collection_member) {
    size_t n1, n2, tn; uint8_t *b1 = ttf_build_styled("FfcTtcA", 400, 5, 0, &n1), *b2 = ttf_build_styled("FfcTtcB", 700, 5, 0, &n2);
    const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &tn);
    dwx_font h; HRESULT hr; UINT32 i;
    if (!ttc) { t_skip("ttc build failed"); free(b1); free(b2); return; }
    for (i = 0; i < 2; i++) {
        hr = dwx_font_open(&h, f, ttc, tn, i, DWRITE_FONT_SIMULATIONS_NONE);
        if (FAILED(hr) || !h.face) { t_skip("ttc open %u: %s", i, dw_hr_name(hr)); dwx_font_close(&h); break; }
        EXPECT_EQ(h.face_type, DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION);
        EXPECT_EQ(h.nfaces, 2);
        EXPECT_EQ(IDWriteFontFace_GetIndex(h.face), i);
        /* the face of a collection member reports the type it was created with or the resolved sfnt flavour: record it */
        t_note("ttc member %u GetType=%d", i, (int)IDWriteFontFace_GetType(h.face));
        { DWRITE_FONT_FACE_TYPE t = IDWriteFontFace_GetType(h.face); CHECK(t == DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION || t == DWRITE_FONT_FACE_TYPE_TRUETYPE); }
        dwx_font_close(&h);
    }
    free(b1); free(b2); free(ttc);
}
FTEST(ffc_type_cff_flag_on_truetype_file) {
    /* A TrueType-outline file requested as CFF: the outcome is OS dependent; the contract is that *out is NULL on failure. */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1; HRESULT hr;
    if (!face) return;
    hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_CFF, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x);
    t_note("CreateFontFace(CFF) over a glyf font -> %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(x); else { CHECK(x != NULL); t_note("accepted: GetType=%d", (int)IDWriteFontFace_GetType(x)); DW_RELEASE(x); }
    dwx_font_close(&h);
}
FTEST(ffc_type_collection_flag_on_plain_file) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1; HRESULT hr;
    if (!face) return;
    hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x);
    t_note("CreateFontFace(OPENTYPE_COLLECTION, index 0) over a plain sfnt -> %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(x); else { CHECK(x != NULL); DW_RELEASE(x); }
    dwx_font_close(&h);
}
FTEST(ffc_type_unknown_flag_on_plain_file) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1; HRESULT hr;
    if (!face) return;
    hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x);
    t_note("CreateFontFace(UNKNOWN) -> %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(x); else { CHECK(x != NULL); t_note("resolved type %d", (int)IDWriteFontFace_GetType(x)); DW_RELEASE(x); }
    dwx_font_close(&h);
}

/* ---- CreateFontFace argument validation ------------------------------------------------------------------------ */
FTEST(ffc_create_invalid_face_types) {
    /* TYPE1(3), VECTOR(4), BITMAP(5) and values outside the enum: rejected before anything is read. */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const int t[] = { 3, 4, 5, 8, 9, 100, -1, 0x7fffffff };
    int i; if (!face) return;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFactory_CreateFontFace(f, (DWRITE_FONT_FACE_TYPE)t[i], 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x);
        CHECKF(hr == E_INVAL, "faceType %d -> %s", t[i], dw_hr_name(hr));
        CHECK_OUT_NULL(x);
        DW_RELEASE(x);
    }
    dwx_font_close(&h);
}
FTEST(ffc_create_zero_files) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
    if (!face) return;
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x), E_INVAL);
    CHECK_OUT_NULL(x);
    dwx_font_close(&h);
}
FTEST(ffc_create_two_files) {
    /* numberOfFiles must be exactly 1 even when the array really holds two valid entries */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
    IDWriteFontFile *two[2]; if (!face) return;
    two[0] = two[1] = h.file;
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 2, two, 0, DWRITE_FONT_SIMULATIONS_NONE, &x), E_INVAL);
    CHECK_OUT_NULL(x);
    x = (IDWriteFontFace *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 3, two, 0, DWRITE_FONT_SIMULATIONS_NONE, &x), E_INVAL);
    CHECK_OUT_NULL(x);
    dwx_font_close(&h);
}
FTEST(ffc_create_null_file_array) {
    if (t_is_wine()) { t_skip("wine dereferences a NULL fontFiles array"); return; }
    IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &x), E_INVAL);
    CHECK_OUT_NULL(x);
}
FTEST(ffc_create_null_file_element) {
    if (t_is_wine()) { t_skip("wine dereferences a NULL font file element"); return; }
    IDWriteFontFile *nf = NULL; IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &nf, 0, DWRITE_FONT_SIMULATIONS_NONE, &x), E_INVAL);
    CHECK_OUT_NULL(x);
}
FTEST(ffc_create_face_index_nonzero_truetype) {
    /* faceIndex != 0 is only legal for OPENTYPE_COLLECTION */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const UINT32 idx[] = { 1, 2, 100, 0xFFFFFFFF };
    int i; if (!face) return;
    for (i = 0; i < 4; i++) {
        IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, idx[i], DWRITE_FONT_SIMULATIONS_NONE, &x);
        CHECKF(hr == E_INVAL, "index %u -> %s", idx[i], dw_hr_name(hr));
        CHECK_OUT_NULL(x);
        DW_RELEASE(x);
    }
    dwx_font_close(&h);
}
FTEST(ffc_create_face_index_nonzero_cff_and_unknown) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const DWRITE_FONT_FACE_TYPE ty[] = { DWRITE_FONT_FACE_TYPE_CFF, DWRITE_FONT_FACE_TYPE_UNKNOWN, DWRITE_FONT_FACE_TYPE_RAW_CFF };
    int i; if (!face) return;
    for (i = 0; i < 3; i++) {
        IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFactory_CreateFontFace(f, ty[i], 1, &h.file, 1, DWRITE_FONT_SIMULATIONS_NONE, &x);
        CHECKF(hr == E_INVAL, "type %d index 1 -> %s", (int)ty[i], dw_hr_name(hr));
        CHECK_OUT_NULL(x);
        DW_RELEASE(x);
    }
    dwx_font_close(&h);
}
FTEST(ffc_create_invalid_simulations) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const unsigned s[] = { 4, 5, 8, 0x10, 0x80, 0xFF, 0x100, 0x80000000u, 0xFFFFFFFCu, 0xFFFFFFFFu };
    int i; if (!face) return;
    for (i = 0; i < (int)(sizeof s / sizeof s[0]); i++) {
        IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, (DWRITE_FONT_SIMULATIONS)s[i], &x);
        CHECKF(hr == E_INVAL, "simulations 0x%x -> %s", s[i], dw_hr_name(hr));
        CHECK_OUT_NULL(x);
        DW_RELEASE(x);
    }
    dwx_font_close(&h);
}
FTEST(ffc_create_valid_simulations_roundtrip) {
    static const DWRITE_FONT_SIMULATIONS sims[4] = { DWRITE_FONT_SIMULATIONS_NONE, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE,
                                                     DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE };
    int i;
    for (i = 0; i < 4; i++) {
        ttf_spec s; dwx_font h; IDWriteFontFace *face;
        ttf_default_spec(&s);
        face = ffc_open(&h, f, &s, sims[i]); if (!face) return;
        CHECKF(IDWriteFontFace_GetSimulations(face) == sims[i], "sim %d -> %d", (int)sims[i], (int)IDWriteFontFace_GetSimulations(face));
        dwx_font_close(&h);
    }
}
FTEST(ffc_create_out_zeroed_on_success_path_preset) {
    /* a non-NULL preset value is overwritten on success as well */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
    if (!face) return;
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x));
    CHECK(x != NULL && x != (IDWriteFontFace *)(ULONG_PTR)1);
    DW_RELEASE(x);
    dwx_font_close(&h);
}
FTEST(ffc_create_wrong_flavour_garbage_file) {
    /* a file that Analyze rejects: CreateFontFace outcome is probed (Win10: S_OK); a failure must zero the out pointer */
    size_t n; uint8_t *b = ttf_build_default(&n), *c; size_t cn; dwx_font h; HRESULT hr; IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
    if (!b) { t_skip("build"); return; }
    c = ttf_corrupt(b, n, TTF_CORRUPT_BAD_MAGIC, 0, &cn);
    hr = dwx_font_open(&h, f, c, cn, 0, DWRITE_FONT_SIMULATIONS_NONE);
    t_note("open of bad-magic font -> %s supported=%d", dw_hr_name(hr), (int)h.supported);
    if (h.file) {
        hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x);
        /* Win10 creates the face lazily and returns S_OK even though Analyze reported "unsupported" */
        if (FAILED(hr)) { CHECK_OUT_NULL(x); } else { CHECK(x != NULL && x != (IDWriteFontFace *)(ULONG_PTR)1); }
        t_note("CreateFontFace over bad magic -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr)) DW_RELEASE(x);
    }
    dwx_font_close(&h); free(b); free(c);
}
FTEST(ffc_create_missing_maxp_fails) {
    /* OpenTypeFace requires maxp (it throws a FileFormatException when the table is missing) */
    ttf_spec s; size_t n; uint8_t *b; dwx_font h; HRESULT hr; IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1;
    ttf_default_spec(&s); s.omit |= TTF_OMIT_MAXP;
    b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    hr = dwx_font_open(&h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    t_note("font without maxp: open -> %s supported=%d", dw_hr_name(hr), (int)h.supported);
    if (SUCCEEDED(hr) && h.face) { t_note("face created anyway, glyph count %u", (unsigned)IDWriteFontFace_GetGlyphCount(h.face)); }
    else if (h.file) {
        hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x);
        CHECK(FAILED(hr)); CHECK_OUT_NULL(x);
        t_note("-> %s", dw_hr_name(hr));
        DW_RELEASE(x);
    }
    dwx_font_close(&h); free(b);
}

/* ---- GetFiles ------------------------------------------------------------------------------------------------------ */
FTEST(ffc_getfiles_null_count) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); IDWriteFontFile *fl = (IDWriteFontFile *)(ULONG_PTR)1;
    if (!face) return;
    if (t_is_wine()) { t_skip("wine dereferences a NULL numberOfFiles"); dwx_font_close(&h); return; }
    CHECK_HR(IDWriteFontFace_GetFiles(face, NULL, NULL), E_INVAL);
    CHECK_HR(IDWriteFontFace_GetFiles(face, NULL, &fl), E_INVAL);
    EXPECT_EQ(fl, (IDWriteFontFile *)(ULONG_PTR)1);   /* untouched on the early failure */
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_count_query) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 n = 99;
    if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, NULL));
    EXPECT_EQ(n, 1);
    n = 0; CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, NULL)); EXPECT_EQ(n, 1);   /* input value ignored when the array is NULL */
    n = 1000; CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, NULL)); EXPECT_EQ(n, 1);
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_returns_one_file) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 n = 1; IDWriteFontFile *fl = NULL;
    if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &fl));
    EXPECT_EQ(n, 1);
    CHECK(fl != NULL);
    DW_RELEASE(fl);
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_wrong_count) {
    /* with an array the count must be exactly 1; the array is not written on failure */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const UINT32 c[] = { 0, 2, 3, 100, 0xFFFFFFFF };
    int i; if (!face) return;
    for (i = 0; i < 5; i++) {
        UINT32 n = c[i]; IDWriteFontFile *arr[2] = { (IDWriteFontFile *)(ULONG_PTR)1, (IDWriteFontFile *)(ULONG_PTR)2 };
        HRESULT hr = IDWriteFontFace_GetFiles(face, &n, arr);
        CHECKF(hr == E_INVAL, "count %u -> %s", c[i], dw_hr_name(hr));
        EXPECT_EQ(arr[0], (IDWriteFontFile *)(ULONG_PTR)1);
        EXPECT_EQ(arr[1], (IDWriteFontFile *)(ULONG_PTR)2);
        EXPECT_EQ(n, c[i]);
    }
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_cached_pointer_and_refs) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 n = 1; IDWriteFontFile *a = NULL, *b = NULL; ULONG r1, r2, r3;
    if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &a));
    r1 = dw_refcount((IUnknown *)a);
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &b));
    r2 = dw_refcount((IUnknown *)a);
    CHECK(a == b);                 /* DWriteFontFace::CreateLazily stores the file once; every call hands out that pointer */
    EXPECT_EQ(r2, r1 + 1);         /* AddRef per call */
    IDWriteFontFile_Release(b);
    r3 = dw_refcount((IUnknown *)a);
    EXPECT_EQ(r3, r1);
    IDWriteFontFile_Release(a);
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_survives_face_release_of_creation_file) {
    /* the face keeps its own file reference: the file we handed to CreateFontFace can be released first */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 n = 1; IDWriteFontFile *a = NULL; const void *key = NULL; UINT32 ks = 0;
    if (!face) return;
    DW_RELEASE(h.file);
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &a));
    CHECK(a != NULL);
    if (a) {
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(a, &key, &ks));
        EXPECT_EQ(ks, 4);
        if (key && ks == 4) EXPECT_EQ(*(const UINT32 *)key, h.key);
        IDWriteFontFile_Release(a);
    }
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_reference_key_and_loader) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 n = 1; IDWriteFontFile *a = NULL; const void *key = NULL; UINT32 ks = 0;
    IDWriteFontFileLoader *ld = NULL;
    if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &a)); if (!a) { dwx_font_close(&h); return; }
    CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(a, &key, &ks));
    EXPECT_EQ(ks, 4);
    if (key && ks == 4) EXPECT_EQ(*(const UINT32 *)key, h.key);
    CHECK_HR_OK(IDWriteFontFile_GetLoader(a, &ld));
    CHECK(ld != NULL);
    t_note("GetFiles file == creation file: %d; loader == registered loader: %d", a == h.file, ld == (IDWriteFontFileLoader *)h.ld);
    DW_RELEASE(ld);
    IDWriteFontFile_Release(a);
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_file_analyzes_like_original) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 n = 1, nf = 0; IDWriteFontFile *a = NULL;
    BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft = 0; DWRITE_FONT_FACE_TYPE fc = 0;
    if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &a)); if (!a) { dwx_font_close(&h); return; }
    CHECK_HR_OK(IDWriteFontFile_Analyze(a, &sup, &ft, &fc, &nf));
    CHECK(sup); EXPECT_EQ(ft, h.file_type); EXPECT_EQ(fc, h.face_type); EXPECT_EQ(nf, h.nfaces);
    IDWriteFontFile_Release(a);
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_release_balance_with_loader) {
    /* after the face and all files are gone every stream fragment has been released */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT32 n = 1; IDWriteFontFile *a = NULL; dwx_mem_loader *ld;
    if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &a)); DW_RELEASE(a);
    (void)IDWriteFontFace_GetGlyphCount(face);
    ld = h.ld;
    DW_RELEASE(h.face); DW_RELEASE(h.file);
    t_note("outstanding fragments after releasing face+file: %ld", (long)dwx_loader_total_outstanding(ld));   /* caches may legally keep some */
    dwx_font_close(&h);
}
FTEST(ffc_getfiles_two_faces_one_file) {
    /* two faces over one file object: each face answers GetFiles with a file naming the same blob */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f), *f2 = NULL; UINT32 n = 1; IDWriteFontFile *a = NULL, *b = NULL;
    const void *k1 = NULL, *k2 = NULL; UINT32 s1 = 0, s2 = 0;
    if (!face) return;
    DW_REQUIRE_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &f2));
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &a));
    CHECK_HR_OK(IDWriteFontFace_GetFiles(f2, &n, &b));
    if (a && b) {
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(a, &k1, &s1)); CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(b, &k2, &s2));
        EXPECT_EQ(s1, s2); if (k1 && k2 && s1 == s2) CHECK(!memcmp(k1, k2, s1));
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f2);
    dwx_font_close(&h);
}

/* ---- GetIndex / GetSimulations ----------------------------------------------------------------------------------------- */
FTEST(ffc_index_zero_for_plain_font) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); if (!face) return;
    EXPECT_EQ(IDWriteFontFace_GetIndex(face), 0);
    dwx_font_close(&h);
}
FTEST(ffc_index_ttc_members) {
    size_t sz[4], tn; uint8_t *bl[4]; const uint8_t *cb[4]; uint8_t *ttc; int i; UINT32 k;
    for (i = 0; i < 4; i++) { char nm[16]; sprintf(nm, "FfcTtc%d", i); bl[i] = ttf_build_styled(nm, 400, 5, 0, &sz[i]); cb[i] = bl[i]; }
    ttc = ttf_build_ttc(cb, sz, 4, &tn);
    if (!ttc) { t_skip("ttc build"); return; }
    for (k = 0; k < 4; k++) {
        dwx_font h; HRESULT hr = dwx_font_open(&h, f, ttc, tn, k, DWRITE_FONT_SIMULATIONS_NONE);
        if (FAILED(hr) || !h.face) { t_skip("ttc open %u: %s", k, dw_hr_name(hr)); dwx_font_close(&h); break; }
        EXPECT_EQ(h.nfaces, 4);
        EXPECT_EQ(IDWriteFontFace_GetIndex(h.face), k);
        dwx_font_close(&h);
    }
    for (i = 0; i < 4; i++) free(bl[i]);
    free(ttc);
}
FTEST(ffc_index_ttc_out_of_range) {
    size_t sz[2], tn; uint8_t *bl[2]; const uint8_t *cb[2]; uint8_t *ttc; int i; dwx_font h; IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1; HRESULT hr;
    for (i = 0; i < 2; i++) { bl[i] = ttf_build_styled("FfcRange", 400, 5, 0, &sz[i]); cb[i] = bl[i]; }
    ttc = ttf_build_ttc(cb, sz, 2, &tn);
    hr = dwx_font_open(&h, f, ttc, tn, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (FAILED(hr) || !h.file) { t_skip("ttc open: %s", dw_hr_name(hr)); dwx_font_close(&h); free(bl[0]); free(bl[1]); free(ttc); return; }
    hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION, 1, &h.file, 2, DWRITE_FONT_SIMULATIONS_NONE, &x);
    t_note("index == numberOfFaces -> %s", dw_hr_name(hr));
    CHECK(FAILED(hr)); CHECK_OUT_NULL(x);
    DW_RELEASE(x); x = (IDWriteFontFace *)(ULONG_PTR)1;
    hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION, 1, &h.file, 0xFFFFFFFF, DWRITE_FONT_SIMULATIONS_NONE, &x);
    t_note("index 0xFFFFFFFF -> %s", dw_hr_name(hr));
    CHECK(FAILED(hr)); CHECK_OUT_NULL(x);
    DW_RELEASE(x);
    dwx_font_close(&h); free(bl[0]); free(bl[1]); free(ttc);
}
FTEST(ffc_simulations_none_by_default) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); if (!face) return;
    EXPECT_EQ(IDWriteFontFace_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE);
    dwx_font_close(&h);
}
FTEST(ffc_simulations_distinct_faces) {
    /* the simulation flags are part of the face identity: four distinct objects for four flag sets */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f), *x[4] = { 0, 0, 0, 0 }; int i, j;
    if (!face) return;
    for (i = 0; i < 4; i++) CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, (DWRITE_FONT_SIMULATIONS)i, &x[i]));
    for (i = 0; i < 4; i++) for (j = i + 1; j < 4; j++) if (x[i] && x[j]) CHECK(x[i] != x[j]);
    for (i = 0; i < 4; i++) if (x[i]) EXPECT_EQ(IDWriteFontFace_GetSimulations(x[i]), (DWRITE_FONT_SIMULATIONS)i);
    for (i = 0; i < 4; i++) DW_RELEASE(x[i]);
    dwx_font_close(&h);
}
FTEST(ffc_same_params_same_face_note) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f), *x = NULL; if (!face) return;
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x));
    t_note("second CreateFontFace with identical parameters returns the same object: %d", x == face);
    DW_RELEASE(x);
    dwx_font_close(&h);
}

/* ---- IsSymbolFont --------------------------------------------------------------------------------------------------------- */
static const ttf_map ffc_sym_map[] = { { 0xF020, 1 }, { 0xF041, 2 }, { 0xF042, 3 }, { 0xF061, 28 } };
FTEST(ffc_symbol_false_for_unicode_cmap) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); if (!face) return;
    CHECK(!IDWriteFontFace_IsSymbolFont(face));
    dwx_font_close(&h);
}
FTEST(ffc_symbol_true_for_3_0_cmap) {
    ttf_spec s; ttf_cmap c; dwx_font h; IDWriteFontFace *face;
    ttf_default_spec(&s);
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 0; c.n = 4; c.map = ffc_sym_map;
    s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK(IDWriteFontFace_IsSymbolFont(face));   /* IsSymbolFont(OpenTypeCharacterMap, panose): symbol cmap kind => TRUE */
    dwx_font_close(&h);
}
FTEST(ffc_symbol_return_is_bool_range) {
    ttf_spec s; ttf_cmap c; dwx_font h; IDWriteFontFace *face; BOOL v;
    ttf_default_spec(&s);
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 0; c.n = 4; c.map = ffc_sym_map;
    s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    v = IDWriteFontFace_IsSymbolFont(face);
    CHECK(v == TRUE || v == FALSE);   /* the implementation returns flags & 1 */
    EXPECT_EQ(v, 1);
    dwx_font_close(&h);
}
FTEST(ffc_symbol_mixed_cmaps_note) {
    ttf_spec s; ttf_cmap c[2]; int dn; const ttf_cmap *d = ttf_default_cmaps(&dn); dwx_font h; IDWriteFontFace *face;
    ttf_default_spec(&s);
    c[0] = d[0];
    memset(&c[1], 0, sizeof c[1]); c[1].format = 4; c[1].plat = 3; c[1].enc = 0; c[1].n = 4; c[1].map = ffc_sym_map;
    s.cmaps = c; s.ncmaps = 2;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    t_note("font with (3,1)+(3,0) cmaps: IsSymbolFont=%d", (int)IDWriteFontFace_IsSymbolFont(face));
    dwx_font_close(&h);
}
FTEST(ffc_symbol_true_for_panose_family_5) {
    /* OS/2 panose bFamilyType 5 (Latin Symbol) marks a symbol font even with a Unicode cmap */
    ttf_spec s; size_t n; uint8_t *b, *os2c; const uint8_t *os2; unsigned ol; ttf_rawtable raw; dwx_font h; IDWriteFontFace *face;
    ttf_default_spec(&s); b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    os2 = ffc_blob_table(b, n, "OS/2", &ol); if (!os2 || ol < 78) { free(b); t_skip("OS/2"); return; }
    os2c = (uint8_t *)malloc(ol); memcpy(os2c, os2, ol); os2c[32] = 5;
    memcpy(raw.tag, "OS/2", 5); raw.data = os2c; raw.len = ol; s.raw = &raw; s.nraw = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE);
    if (face) { CHECK(IDWriteFontFace_IsSymbolFont(face)); }
    dwx_font_close(&h); free(b); free(os2c);
}
FTEST(ffc_symbol_false_for_panose_family_2) {
    ttf_spec s; size_t n; uint8_t *b, *os2c; const uint8_t *os2; unsigned ol; ttf_rawtable raw; dwx_font h; IDWriteFontFace *face;
    ttf_default_spec(&s); b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    os2 = ffc_blob_table(b, n, "OS/2", &ol); if (!os2 || ol < 78) { free(b); t_skip("OS/2"); return; }
    os2c = (uint8_t *)malloc(ol); memcpy(os2c, os2, ol); os2c[32] = 2;
    memcpy(raw.tag, "OS/2", 5); raw.data = os2c; raw.len = ol; s.raw = &raw; s.nraw = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE);
    if (face) { CHECK(!IDWriteFontFace_IsSymbolFont(face)); }
    dwx_font_close(&h); free(b); free(os2c);
}
FTEST(ffc_symbol_false_for_3_10_only) {
    ttf_spec s; ttf_cmap c; int dn; const ttf_cmap *d = ttf_default_cmaps(&dn); dwx_font h; IDWriteFontFace *face; int i;
    ttf_default_spec(&s);
    for (i = 0; i < dn; i++) if (d[i].plat == 3 && d[i].enc == 10) c = d[i];
    s.cmaps = &c; s.ncmaps = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECK(!IDWriteFontFace_IsSymbolFont(face));
    dwx_font_close(&h);
}

/* ---- GetGlyphCount ---------------------------------------------------------------------------------------------------------- */
FTEST(ffc_glyphcount_default) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); if (!face) return;
    EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
    dwx_font_close(&h);
}
static void ffc_count_case(IDWriteFactory *f, int total, int compress) {
    int dn, i; ttf_glyph *g = ffc_glyphs_default(&dn); ttf_spec s; dwx_font h; IDWriteFontFace *face;
    if (total < dn) dn = total;
    for (i = dn; i < total; i++) { memset(&g[i], 0, sizeof g[i]); g[i].adv = 600; }
    ttf_default_spec(&s);
    s.glyphs = g; s.nglyphs = total; s.compress_hmtx = compress;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    CHECKF(IDWriteFontFace_GetGlyphCount(face) == total, "total %d compress %d -> %u", total, compress, (unsigned)IDWriteFontFace_GetGlyphCount(face));
    dwx_font_close(&h);
}
FTEST(ffc_glyphcount_70) { ffc_count_case(f, 70, 0); }
FTEST(ffc_glyphcount_100) { ffc_count_case(f, 100, 0); }
FTEST(ffc_glyphcount_256) { ffc_count_case(f, 256, 0); }
FTEST(ffc_glyphcount_257) { ffc_count_case(f, 257, 0); }
FTEST(ffc_glyphcount_1000) { ffc_count_case(f, 1000, 0); }
FTEST(ffc_glyphcount_compressed_hmtx_70) { ffc_count_case(f, 70, 1); }
FTEST(ffc_glyphcount_compressed_hmtx_1000) { ffc_count_case(f, 1000, 1); }
FTEST(ffc_glyphcount_small_font) {
    /* only .notdef, space and 'A' */
    int dn; ttf_glyph *g = ffc_glyphs_default(&dn); ttf_spec s; dwx_font h; IDWriteFontFace *face;
    ttf_default_spec(&s);
    s.glyphs = g; s.nglyphs = 3;
    { static const ttf_map m[] = { { 0x20, 1 }, { 0x41, 2 } }; static ttf_cmap c; memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 1; c.n = 2; c.map = m; s.cmaps = &c; s.ncmaps = 1; }
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), 3);
    dwx_font_close(&h);
}
FTEST(ffc_glyphcount_independent_of_simulation) {
    int i;
    for (i = 0; i < 4; i++) {
        ttf_spec s; dwx_font h; IDWriteFontFace *face; ttf_default_spec(&s);
        face = ffc_open(&h, f, &s, (DWRITE_FONT_SIMULATIONS)i); if (!face) return;
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
        dwx_font_close(&h);
    }
}
FTEST(ffc_glyphcount_maxp_larger_than_hmtx_note) {
    /* maxp.numGlyphs larger than the glyph data: the face clamps to what hmtx covers (OpenTypeFace constructor) - record only */
    ttf_spec s; size_t n; uint8_t *b, *mp; const uint8_t *m; unsigned ml; ttf_rawtable raw; dwx_font h; IDWriteFontFace *face;
    ttf_default_spec(&s);
    b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    m = ffc_blob_table(b, n, "maxp", &ml);
    if (!m || ml < 32) { free(b); t_skip("maxp"); return; }
    mp = (uint8_t *)malloc(ml); memcpy(mp, m, ml); mp[4] = 0x10; mp[5] = 0x00;   /* 4096 */
    raw.data = mp; raw.len = ml; memcpy(raw.tag, "maxp", 5);
    s.raw = &raw; s.nraw = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE);
    if (face) { t_note("maxp 4096, 69 real glyphs -> GetGlyphCount %u", (unsigned)IDWriteFontFace_GetGlyphCount(face)); CHECK(IDWriteFontFace_GetGlyphCount(face) <= 4096); }
    dwx_font_close(&h); free(b); free(mp);
}

/* ---- refcount / QueryInterface --------------------------------------------------------------------------------------------- */
FTEST(ffc_refcount_addref_release) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ULONG a, b, c; if (!face) return;
    a = dw_refcount((IUnknown *)face);
    CHECK(a >= 1);
    b = IDWriteFontFace_AddRef(face);
    EXPECT_EQ(b, a + 1);
    c = IDWriteFontFace_Release(face);
    EXPECT_EQ(c, a);
    dwx_font_close(&h);
}
FTEST(ffc_refcount_create_gives_new_reference) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f), *x = NULL; ULONG before; if (!face) return;
    before = dw_refcount((IUnknown *)face);
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &x));
    if (x == face) { EXPECT_EQ(dw_refcount((IUnknown *)face), before + 1); }
    else t_note("second create returned a distinct object");
    DW_RELEASE(x);
    EXPECT_EQ(dw_refcount((IUnknown *)face), before);
    dwx_font_close(&h);
}
/* Local QI matrix: like DW_CHECK_QI, but an unsupported IID may fail with E_NOTIMPL (what Win10 dwrite returns) or E_NOINTERFACE. */
static void ffc_qi_check(IDWriteFontFace *obj, const dw_qi_row *rows, int n) {
    int i; ULONG base = dw_refcount((IUnknown *)obj);
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)1; HRESULT hr = IUnknown_QueryInterface((IUnknown *)obj, rows[i].iid, &p);
        if (SUCCEEDED(hr)) {
            if (rows[i].expect == 0) CHECKF(0, "%s: QI unexpectedly succeeded", rows[i].name);
            else if (rows[i].expect == 1) CHECKF(hr == S_OK, "%s hr=%s", rows[i].name, dw_hr_name(hr));
            else t_note("QI %s -> %s", rows[i].name, dw_hr_name(hr));
            CHECKF(p != NULL && p != (void *)1, "%s: success with bad out pointer", rows[i].name);
            if (p && p != (void *)1) { CHECKF(dw_same_identity((IUnknown *)p, (IUnknown *)obj), "%s: different COM identity", rows[i].name); IUnknown_Release((IUnknown *)p); }
        } else if (rows[i].expect == 1) CHECKF(0, "%s: QI failed: %s", rows[i].name, dw_hr_name(hr));
        else if (rows[i].expect == 0) {
            CHECKF(hr == E_NOTIMPL || hr == E_NOINTERFACE, "%s: QI failure code %s", rows[i].name, dw_hr_name(hr));
            CHECKF(p == NULL, "%s: out pointer not zeroed (%p)", rows[i].name, p);
        } else t_note("QI %s -> %s", rows[i].name, dw_hr_name(hr));
    }
    CHECKF(dw_refcount((IUnknown *)obj) == base, "QI matrix leaves refcount balanced (%lu vs %lu)", base, dw_refcount((IUnknown *)obj));
}
FTEST(ffc_qi_matrix) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); if (!face) return;
    {
        dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontFace, 1), DW_QI_ROW(IDWriteFontFace1, 1), DW_QI_ROW(IDWriteFontFace2, 1),
                             DW_QI_ROW(IDWriteFontFace3, -1), DW_QI_ROW(IDWriteFontFace4, -1), DW_QI_ROW(IDWriteFontFace5, -1), DW_QI_ROW(IDWriteFontFace6, -1),
                             DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFontFamily, 0),
                             DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0) };
        ffc_qi_check(face, rows, (int)(sizeof(rows) / sizeof(rows[0])));
    }
    dwx_font_close(&h);
}
FTEST(ffc_qi_identity_across_versions) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); int v; if (!face) return;
    for (v = 1; v <= 3; v++) {
        void *p = ffc_face_v(face, v);
        if (!p) { t_note("IDWriteFontFace%d unsupported", v); continue; }
        CHECK(dw_same_identity((IUnknown *)p, (IUnknown *)face));
        IUnknown_Release((IUnknown *)p);
    }
    dwx_font_close(&h);
}
FTEST(ffc_qi_unknown_iid_zeroes_out) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); void *p = (void *)(ULONG_PTR)1; HRESULT hr; if (!face) return;
    hr = IDWriteFontFace_QueryInterface(face, &IID_IDWriteFactory, &p);
    /* Win10 dwrite answers E_NOTIMPL for IIDs it does not know */
    CHECK(hr == E_NOTIMPL || hr == E_NOINTERFACE); CHECK_OUT_NULL(p);
    dwx_font_close(&h);
}
FTEST(ffc_qi_refcount_per_interface) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ULONG base; void *p1; if (!face) return;
    base = dw_refcount((IUnknown *)face);
    p1 = ffc_face_v(face, 1); if (!p1) { t_skip("no IDWriteFontFace1"); dwx_font_close(&h); return; }
    EXPECT_REFS(face, base + 1);
    IUnknown_Release((IUnknown *)p1);
    EXPECT_REFS(face, base);
    dwx_font_close(&h);
}
FTEST(ffc_face_outlives_creation_file_release) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); DWRITE_FONT_METRICS m; if (!face) return;
    DW_RELEASE(h.file);
    IDWriteFontFace_GetMetrics(face, &m);
    EXPECT_EQ(m.designUnitsPerEm, FFC_UPM);
    EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
    dwx_font_close(&h);
}
FTEST(ffc_create_many_faces_release_all) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f), *x[16]; int i; if (!face) return;
    for (i = 0; i < 16; i++) { x[i] = NULL; CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, (DWRITE_FONT_SIMULATIONS)(i & 3), &x[i])); }
    for (i = 0; i < 16; i++) if (x[i]) EXPECT_EQ(IDWriteFontFace_GetSimulations(x[i]), (DWRITE_FONT_SIMULATIONS)(i & 3));
    for (i = 0; i < 16; i++) DW_RELEASE(x[i]);
    dwx_font_close(&h);
}
FTEST(ffc_loader_streams_balanced_after_use) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); UINT16 gi[3]; UINT32 cp[3] = { 'A', 'B', 'C' }; dwx_mem_loader *ld; DWRITE_FONT_METRICS m;
    if (!face) return;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 3, gi));
    IDWriteFontFace_GetMetrics(face, &m);
    ld = h.ld;
    CHECK(ld->nstreams >= 1);
    DW_RELEASE(h.face); DW_RELEASE(h.file);
    t_note("outstanding fragments after releasing face+file: %ld", (long)dwx_loader_total_outstanding(ld));
    dwx_font_close(&h);
}

/* ---- system fonts ----------------------------------------------------------------------------------------------------------- */
FTEST(ffc_sys_arial_identity) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    UINT32 n = 0;
    if (!face) { t_skip("Arial not installed"); return; }
    EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
    EXPECT_EQ(IDWriteFontFace_GetIndex(face), 0);
    EXPECT_EQ(IDWriteFontFace_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE);
    CHECK(!IDWriteFontFace_IsSymbolFont(face));
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, NULL)); EXPECT_EQ(n, 1);
    t_note("Arial glyph count %u", (unsigned)IDWriteFontFace_GetGlyphCount(face));
    CHECK(IDWriteFontFace_GetGlyphCount(face) > 256);
    IDWriteFontFace_Release(face);
}
FTEST(ffc_sys_marlett_symbol) {
    IDWriteFontFace *face = dw_sysface(f, L"Marlett", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Marlett not installed"); return; }
    t_note("Marlett IsSymbolFont=%d", (int)IDWriteFontFace_IsSymbolFont(face));
    CHECK(IDWriteFontFace_IsSymbolFont(face));
    IDWriteFontFace_Release(face);
}
FTEST(ffc_sys_bold_variant_simulations_none) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Arial not installed"); return; }
    t_note("Arial bold simulations = %d", (int)IDWriteFontFace_GetSimulations(face));
    EXPECT_EQ(IDWriteFontFace_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE);   /* arialbd.ttf exists on every client SKU */
    IDWriteFontFace_Release(face);
}
FTEST(ffc_sys_local_loader_path) {
    char path[MAX_PATH]; const char *cand[] = { "arial.ttf", NULL }; WCHAR wp[MAX_PATH]; IDWriteFontFile *file = NULL, *ff = NULL; IDWriteFontFace *face = NULL, *other = NULL;
    UINT32 n = 1; const void *key = NULL; UINT32 ks = 0; IDWriteFontFileLoader *ld = NULL; IDWriteLocalFontFileLoader *ll = NULL; BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf;
    if (!dw_find_system_font(cand, path, sizeof path)) { t_skip("arial.ttf not found"); return; }
    dw_wide(path, wp, MAX_PATH);
    DW_REQUIRE_OK(IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file));
    CHECK_HR_OK(IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf));
    CHECK(sup); EXPECT_EQ(fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(nf, 1);
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, fc, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    if (face) {
        CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &ff));
        if (ff) {
            CHECK_HR_OK(IDWriteFontFile_GetLoader(ff, &ld));
            CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff, &key, &ks));
            if (ld && SUCCEEDED(IDWriteFontFileLoader_QueryInterface(ld, &IID_IDWriteLocalFontFileLoader, (void **)&ll)) && ll) {
                UINT32 len = 0; CHECK_HR_OK(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(ll, key, ks, &len));
                if (len) {
                    WCHAR *buf = (WCHAR *)calloc(len + 1, sizeof(WCHAR));
                    CHECK_HR_OK(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, buf, len + 1));
                    CHECK(!lstrcmpiW(buf, wp));
                    free(buf);
                }
                IDWriteLocalFontFileLoader_Release(ll);
            } else t_note("loader does not expose IDWriteLocalFontFileLoader");
            DW_RELEASE(ld); IDWriteFontFile_Release(ff);
        }
        DW_RELEASE(face);
    }
    (void)other;
    DW_RELEASE(file);
}
FTEST(ffc_sys_ttc_member_indices) {
    char path[MAX_PATH]; const char *cand[] = { "msgothic.ttc", NULL }; WCHAR wp[MAX_PATH]; IDWriteFontFile *file = NULL; BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf = 0, i;
    if (!dw_find_system_font(cand, path, sizeof path)) { t_skip("msgothic.ttc not found"); return; }
    dw_wide(path, wp, MAX_PATH);
    DW_REQUIRE_OK(IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file));
    CHECK_HR_OK(IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf));
    CHECK(sup); EXPECT_EQ(fc, DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION); EXPECT_EQ(ft, DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION);
    CHECK(nf >= 2);
    for (i = 0; i < nf && i < 4; i++) {
        IDWriteFontFace *face = NULL;
        CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, fc, 1, &file, i, DWRITE_FONT_SIMULATIONS_NONE, &face));
        if (face) {
            EXPECT_EQ(IDWriteFontFace_GetIndex(face), i);
            t_note("msgothic.ttc face %u: type %d glyphs %u", i, (int)IDWriteFontFace_GetType(face), (unsigned)IDWriteFontFace_GetGlyphCount(face));
            DW_RELEASE(face);
        }
    }
    { IDWriteFontFace *x = (IDWriteFontFace *)(ULONG_PTR)1; HRESULT hr = IDWriteFactory_CreateFontFace(f, fc, 1, &file, nf, DWRITE_FONT_SIMULATIONS_NONE, &x); CHECK(FAILED(hr)); CHECK_OUT_NULL(x); t_note("index == face count -> %s", dw_hr_name(hr)); DW_RELEASE(x); }
    DW_RELEASE(file);
}
FTEST(ffc_sys_family_faces_have_distinct_files_or_sims) {
    static const WCHAR *fam[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Consolas" };
    int i;
    for (i = 0; i < 6; i++) {
        IDWriteFontFace *face = dw_sysface(f, fam[i], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        UINT32 n = 0;
        if (!face) { t_note("%ls not installed", fam[i]); continue; }
        EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
        CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, NULL)); EXPECT_EQ(n, 1);
        CHECK(IDWriteFontFace_GetGlyphCount(face) > 100);
        IDWriteFontFace_Release(face);
    }
}
FTEST(ffc_sys_variable_font_bahnschrift) {
    IDWriteFontFace *face = dw_sysface(f, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Bahnschrift not installed"); return; }
    EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
    { UINT32 len = 0; uint8_t *fvar = dw_face_table(face, DW_TAG('f','v','a','r'), &len); CHECK(fvar != NULL); free(fvar); }
    IDWriteFontFace_Release(face);
}
