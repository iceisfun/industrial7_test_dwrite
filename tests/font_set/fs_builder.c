/* IDWriteFontSetBuilder / IDWriteFontSetBuilder1 / IDWriteFontSetBuilder2 and the sets they create.
 *
 * Win10 decompilation (DWriteFontSetBuilder, FontSetBuilder):
 *  - one DWriteFontSetBuilder class implements all three interfaces; every factory CreateFontSetBuilder variant returns it.
 *  - AddFontFaceReference(ref): NULL -> E_INVALIDARG; the entry is created WITHOUT properties and CreateFontSet extracts them
 *    from the font file (FontSetBuilder::ExtractMissingFontProperties only for entries whose property range is unset).
 *  - AddFontFaceReference(ref, props, n): NULL ref, or NULL props with n > 0 -> E_INVALIDARG; the supplied properties are
 *    the only ones the entry gets (nothing is extracted); a NULL propertyValue throws ArgumentException (E_INVALIDARG).
 *  - AddFontSet(NULL) -> E_INVALIDARG; otherwise every item (with its properties) is copied.
 *  - AddFontFile(file): NULL -> E_INVALIDARG, unsupported data -> DWRITE_E_FILEFORMAT (docs), otherwise one entry per face.
 *  - AddFont(file, faceIndex, simulations, axisValues, nAxis, ranges, nRanges, props, nProps): E_INVALIDARG for NULL file,
 *    simulations beyond BOLD|OBLIQUE, nAxis > 0xFFFF, or NULL arrays with non-zero counts.
 *  - CreateFontSet: out zeroed, always a new DWriteFontSet; an empty builder yields an empty set.
 */
#include "fs.h"

#define E_FILEFORMAT_ ((HRESULT)0x88985000)

TEST(fset_b_create_factory3)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = (IDWriteFontSetBuilder *)FS_SENT;
    HRESULT hr = IDWriteFactory3_CreateFontSetBuilder(x.f3, &b);
    CHECK_HR_OK(hr);
    CHECK(b != NULL && b != FS_SENT);
    if (SUCCEEDED(hr) && b) EXPECT_REFS(b, 1);
    FS_DROP(b, hr);
    fs_close(&x);
}

TEST(fset_b_create_factory5)
{
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 5);
    if (!f5) { t_skip("IDWriteFactory5 unsupported"); return; }
    IDWriteFontSetBuilder1 *b = (IDWriteFontSetBuilder1 *)FS_SENT;
    HRESULT hr = IDWriteFactory5_CreateFontSetBuilder(f5, &b);
    CHECK_HR_OK(hr);
    CHECK(b != NULL && b != FS_SENT);
    if (SUCCEEDED(hr) && b) EXPECT_REFS(b, 1);
    FS_DROP(b, hr);
}

TEST(fset_b_create_factory6)
{
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    IDWriteFontSetBuilder2 *b = (IDWriteFontSetBuilder2 *)FS_SENT;
    HRESULT hr = IDWriteFactory6_CreateFontSetBuilder(f6, &b);
    CHECK_HR_OK(hr);
    CHECK(b != NULL && b != FS_SENT);
    if (SUCCEEDED(hr) && b) EXPECT_REFS(b, 1);
    FS_DROP(b, hr);
}

TEST(fset_b_distinct_objects)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *a = fs_builder(&x), *b = fs_builder(&x);
    CHECK(a != NULL && b != NULL && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_qi_matrix)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (b) {
        dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontSetBuilder, 1), DW_QI_ROW(IDWriteFontSetBuilder1, 1),
                             DW_QI_ROW(IDWriteFontSetBuilder2, 1), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFactory, 0),
                             DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontFaceReference, 0) };
        DW_CHECK_QI(b, rows);
        DW_RELEASE(b);
    }
    fs_close(&x);
}

TEST(fset_b_same_object_all_interfaces)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    IDWriteFontSetBuilder2 *b2 = NULL;
    if (b) IDWriteFontSetBuilder_QueryInterface(b, &IID_IDWriteFontSetBuilder2, (void **)&b2);
    if (b && b1 && b2) {
        CHECK(dw_same_identity((IUnknown *)b, (IUnknown *)b1));
        CHECK(dw_same_identity((IUnknown *)b, (IUnknown *)b2));
    } else t_skip("builder interfaces missing");
    DW_RELEASE(b2); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_empty_create_set)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (!b) { fs_close(&x); return; }
    IDWriteFontSet *set = (IDWriteFontSet *)FS_SENT;
    HRESULT hr = IDWriteFontSetBuilder_CreateFontSet(b, &set);
    CHECK_HR_OK(hr);
    CHECK(set != NULL && set != FS_SENT);
    if (SUCCEEDED(hr) && set) {
        CHECK(IDWriteFontSet_GetFontCount(set) == 0);
        IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)FS_SENT;
        HRESULT h2 = IDWriteFontSet_GetFontFaceReference(set, 0, &r);
        CHECK_HR(h2, E_INVALIDARG);
        CHECK_OUT_NULL(r);
        FS_DROP(r, h2);
        IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
        CHECK(l != NULL);
        if (l) { CHECK(IDWriteStringList_GetCount(l) == 0); DW_RELEASE(l); }
        UINT32 cnt = 5; DWRITE_FONT_PROPERTY p = { DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, L"Anything", L"en-us" };
        CHECK_HR_OK(IDWriteFontSet_GetPropertyOccurrenceCount(set, &p, &cnt));
        CHECK(cnt == 0);
    }
    FS_DROP(set, hr);
    DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_create_set_twice)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "TwiceFam", 400, 5, 0);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    IDWriteFontFile *file = fs_file(&x, k);
    if (b1 && file) {
        IDWriteFontSet *s1 = NULL, *s2 = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, file));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s1));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s2));
        CHECK(s1 != NULL && s2 != NULL && s1 != s2);
        if (s1 && s2) CHECK(IDWriteFontSet_GetFontCount(s1) == 1 && IDWriteFontSet_GetFontCount(s2) == 1);
        DW_RELEASE(s1); DW_RELEASE(s2);
    } else t_skip("setup");
    DW_RELEASE(file); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_set_snapshot_semantics)
{
    FS_OPEN(x);
    UINT32 k1 = fs_add(&x, "SnapA", 400, 5, 0), k2 = fs_add(&x, "SnapB", 400, 5, 0);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    IDWriteFontFile *f1 = fs_file(&x, k1), *f2 = fs_file(&x, k2);
    if (b1 && f1 && f2) {
        IDWriteFontSet *s1 = NULL, *s2 = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, f1));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s1));
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, f2));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s2));
        /* a set made earlier does not grow when the builder is used again */
        if (s1) CHECK(IDWriteFontSet_GetFontCount(s1) == 1);
        if (s2) CHECK(IDWriteFontSet_GetFontCount(s2) == 2);
        DW_RELEASE(s1); DW_RELEASE(s2);
    } else t_skip("setup");
    DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_set_outlives_builder)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "OutlivesFam", 400, 5, 0);
    IDWriteFontSet *set = NULL;
    HRESULT hr = fs_build_keys(&x, &k, 1, &set);
    CHECK_HR_OK(hr);
    if (set) {
        CHECK(IDWriteFontSet_GetFontCount(set) == 1);
        char b[64];
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, b, sizeof b));
        CHECK(!strcmp(b, "OutlivesFam"));
        DW_RELEASE(set);
    }
    fs_close(&x);
}

TEST(fset_b_refcount_balance)
{
    FS_OPEN(x);
    ULONG base = dw_refcount((IUnknown *)x.f);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (b) {
        ULONG with = dw_refcount((IUnknown *)x.f);
        CHECKF(with == base + 1, "builder retains the factory: %lu -> %lu", base, with);
        DW_RELEASE(b);
        EXPECT_REFS(x.f, base);
    }
    IDWriteFontSet *set = NULL;
    UINT32 k = fs_add(&x, "RefFam", 400, 5, 0);
    if (SUCCEEDED(fs_build_keys(&x, &k, 1, &set)) && set) {
        ULONG with = dw_refcount((IUnknown *)x.f);
        CHECKF(with > base, "set retains the factory: %lu -> %lu", base, with);
        DW_RELEASE(set);
        EXPECT_REFS(x.f, base);
    }
    fs_close(&x);
}

/* ---- AddFontFile ---------------------------------------------------------------------------------- */
TEST(fset_b_addfontfile_null)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    if (b1) {
        if (!t_is_wine()) CHECK_HR(IDWriteFontSetBuilder1_AddFontFile(b1, NULL), E_INVALIDARG);   /* Wine dereferences NULL */
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) { CHECK(IDWriteFontSet_GetFontCount(set) == 0); DW_RELEASE(set); }
    } else t_skip("no builder1");
    DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addfontfile_single_face_props)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "OneFace", 700, 5, 1);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(fs_build_keys(&x, &k, 1, &set));
    if (set) {
        char b[96];
        CHECK(IDWriteFontSet_GetFontCount(set) == 1);
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, b, sizeof b) && !strcmp(b, "OneFace"));
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, b, sizeof b) && !strcmp(b, "OneFace Bold Italic"));
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, b, sizeof b) && !strcmp(b, "OneFace-BoldItalic"));
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT, b, sizeof b) && !strcmp(b, "700"));
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_STRETCH, b, sizeof b) && !strcmp(b, "5"));
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_STYLE, b, sizeof b) && !strcmp(b, "2"));
        DW_RELEASE(set);
    }
    fs_close(&x);
}

TEST(fset_b_addfontfile_style_ladder)
{
    FS_OPEN(x);
    static const int weights[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900 };
    UINT32 keys[9]; int i;
    for (i = 0; i < 9; i++) keys[i] = fs_add(&x, "Ladder", weights[i], 5, 0);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(fs_build_keys(&x, keys, 9, &set));
    if (set) {
        CHECK(IDWriteFontSet_GetFontCount(set) == 9);
        for (i = 0; i < 9; i++) {
            char w[16]; snprintf(w, sizeof w, "%d", weights[i]);
            CHECKF(fs_find_by(set, DWRITE_FONT_PROPERTY_ID_WEIGHT, w) != FS_NOT_FOUND, "weight %s present", w);
        }
        IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT);
        if (l) { CHECK(IDWriteStringList_GetCount(l) == 9); DW_RELEASE(l); }
        DW_RELEASE(set);
    }
    fs_close(&x);
}

TEST(fset_b_addfontfile_stretch_ladder)
{
    FS_OPEN(x);
    UINT32 keys[9]; int i;
    for (i = 0; i < 9; i++) keys[i] = fs_add(&x, "Wide", 400, i + 1, 0);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(fs_build_keys(&x, keys, 9, &set));
    if (set) {
        CHECK(IDWriteFontSet_GetFontCount(set) == 9);
        for (i = 0; i < 9; i++) {
            char w[16]; snprintf(w, sizeof w, "%d", i + 1);
            CHECKF(fs_find_by(set, DWRITE_FONT_PROPERTY_ID_STRETCH, w) != FS_NOT_FOUND, "stretch %s present", w);
        }
        DW_RELEASE(set);
    }
    fs_close(&x);
}

TEST(fset_b_addfontfile_many)
{
    FS_OPEN(x);
    enum { N = 100 };
    UINT32 keys[N]; int i;
    for (i = 0; i < N; i++) { char fam[32]; snprintf(fam, sizeof fam, "Bulk%03d", i); keys[i] = fs_add(&x, fam, 400, 5, 0); }
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(fs_build_keys(&x, keys, N, &set));
    if (set) {
        CHECK(IDWriteFontSet_GetFontCount(set) == N);
        IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
        if (l) { CHECK(IDWriteStringList_GetCount(l) == N); DW_RELEASE(l); }
        /* every added file is findable and the found indexes form a permutation */
        unsigned char seen[N]; memset(seen, 0, sizeof seen);
        for (i = 0; i < N; i++) {
            IDWriteFontFaceReference *r = fs_ref(&x, keys[i]); UINT32 idx = 0; BOOL ex = FALSE;
            if (!r) continue;
            CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, r, &idx, &ex));
            CHECKF(ex == TRUE, "file %d found", i);
            if (ex && idx < N) { CHECKF(!seen[idx], "index %u used twice", idx); seen[idx] = 1; }
            DW_RELEASE(r);
        }
        DW_RELEASE(set);
    }
    fs_close(&x);
}

TEST(fset_b_addfontfile_ttc_all_faces)
{
    FS_OPEN(x);
    ttf_spec s1, s2; size_t n1, n2, nt;
    fs_style(&s1, "CollA", 400, 5, 0); fs_style(&s2, "CollB", 700, 5, 0);
    uint8_t *b1 = ttf_build(&s1, &n1), *b2 = ttf_build(&s2, &n2);
    const uint8_t *blobs[2] = { b1, b2 }; size_t sizes[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(blobs, sizes, 2, &nt);
    UINT32 k = fs_add_blob(&x, ttc, nt);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(fs_build_keys(&x, &k, 1, &set));
    if (set) {
        /* AddFontFile adds a reference for every face of the file */
        CHECKF(IDWriteFontSet_GetFontCount(set) == 2, "count %u", IDWriteFontSet_GetFontCount(set));
        CHECK(fs_find_by(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "CollA") != FS_NOT_FOUND);
        CHECK(fs_find_by(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "CollB") != FS_NOT_FOUND);
        unsigned idxmask = 0; UINT32 i;
        for (i = 0; i < IDWriteFontSet_GetFontCount(set); i++) {
            IDWriteFontFaceReference *r = NULL;
            if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(set, i, &r)) && r) {
                UINT32 fi = IDWriteFontFaceReference_GetFontFaceIndex(r);
                if (fi < 2) idxmask |= 1u << fi;
                DW_RELEASE(r);
            }
        }
        CHECKF(idxmask == 3, "face index mask %u", idxmask);
        DW_RELEASE(set);
    }
    free(b1); free(b2); free(ttc);
    fs_close(&x);
}

TEST(fset_b_addfontfile_invalid_data)
{
    FS_OPEN(x);
    uint8_t junk[512]; int i;
    for (i = 0; i < 512; i++) junk[i] = (uint8_t)(i * 7 + 3);
    UINT32 k = fs_add_blob(&x, junk, sizeof junk);
    IDWriteFontFile *file = fs_file(&x, k);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    if (file && b1) {
        HRESULT hr = IDWriteFontSetBuilder1_AddFontFile(b1, file);
        /* docs: not a supported OpenType font file -> DWRITE_E_FILEFORMAT */
        CHECK_HR(hr, E_FILEFORMAT_);
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) { CHECK(IDWriteFontSet_GetFontCount(set) == 0); DW_RELEASE(set); }
    } else t_skip("setup");
    DW_RELEASE(file); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addfontfile_corrupt_variants)
{
    FS_OPEN(x);
    ttf_spec s; size_t n; fs_style(&s, "Corrupt", 400, 5, 0);
    uint8_t *good = ttf_build(&s, &n);
    struct { int kind; size_t param; const char *name; } c[] = {
        { TTF_CORRUPT_BAD_MAGIC, 0, "bad magic" }, { TTF_CORRUPT_TRUNCATE, 8, "truncated to 8 bytes" },
        { TTF_CORRUPT_TRUNCATE, 0, "empty" }, { TTF_CORRUPT_NUMTABLES, 0, "zero tables" } };
    unsigned i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        size_t cn; uint8_t *bad = ttf_corrupt(good, n, c[i].kind, c[i].param, &cn);
        if (!bad) continue;
        UINT32 k = fs_add_blob(&x, bad, cn);
        IDWriteFontFile *file = fs_file(&x, k);
        IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
        if (file && b1) {
            HRESULT hr = IDWriteFontSetBuilder1_AddFontFile(b1, file);
            CHECKF(FAILED(hr), "%s -> %s", c[i].name, dw_hr_name(hr));
            t_note("%s -> %s", c[i].name, dw_hr_name(hr));
        }
        DW_RELEASE(file); DW_RELEASE(b1); DW_RELEASE(b);
        free(bad);
    }
    free(good);
    fs_close(&x);
}

TEST(fset_b_addfontfile_duplicate_same_file)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "DupFile", 400, 5, 0);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    IDWriteFontFile *file = fs_file(&x, k);
    if (b1 && file) {
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, file));
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, file));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            UINT32 n = IDWriteFontSet_GetFontCount(set);
            CHECKF(n == 1 || n == 2, "count %u", n);
            t_note("same file added twice -> %u entries", n);
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(file); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addfontfile_duplicate_two_file_objects)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "DupObj", 400, 5, 0);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    IDWriteFontFile *f1 = fs_file(&x, k), *f2 = fs_file(&x, k);
    if (b1 && f1 && f2) {
        IDWriteFontSet *set = NULL;
        CHECK(f1 != f2);
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, f1));
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, f2));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            UINT32 n = IDWriteFontSet_GetFontCount(set);
            CHECKF(n == 1 || n == 2, "count %u", n);
            t_note("two file objects with the same key -> %u entries", n);
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addfontfile_stream_balance)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "StreamBal", 400, 5, 0);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(fs_build_keys(&x, &k, 1, &set));
    CHECKF(dwx_loader_total_reads(x.fl) > 0, "the font file was parsed through the stream");
    CHECKF(dwx_loader_total_outstanding(x.fl) == 0, "outstanding fragments %ld", (long)dwx_loader_total_outstanding(x.fl));
    t_note("reads for one AddFontFile + CreateFontSet: %ld", (long)dwx_loader_total_reads(x.fl));
    if (set) CHECK(IDWriteFontSet_GetFontCount(set) == 1);
    DW_RELEASE(set);
    CHECKF(dwx_loader_total_outstanding(x.fl) == 0, "outstanding fragments after release %ld", (long)dwx_loader_total_outstanding(x.fl));
    fs_close(&x);
}

TEST(fset_b_addfontfile_read_failure)
{
    FS_OPEN(x);
    UINT32 bad = fs_add(&x, "FailRead", 400, 5, 0), good = fs_add(&x, "GoodRead", 400, 5, 0);
    x.fl->stream_fail_read = E_FAIL;
    IDWriteFontFile *fbad = fs_file(&x, bad);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    if (b1 && fbad) {
        HRESULT hr = IDWriteFontSetBuilder1_AddFontFile(b1, fbad);
        CHECKF(FAILED(hr), "unreadable file -> %s", dw_hr_name(hr));
        t_note("AddFontFile with failing stream -> %s", dw_hr_name(hr));
        x.fl->stream_fail_read = 0;
        IDWriteFontFile *fgood = fs_file(&x, good);
        if (fgood) {
            CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, fgood));
            IDWriteFontSet *set = NULL;
            CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
            if (set) { CHECKF(IDWriteFontSet_GetFontCount(set) == 1, "count %u", IDWriteFontSet_GetFontCount(set)); DW_RELEASE(set); }
            DW_RELEASE(fgood);
        }
    } else t_skip("setup");
    DW_RELEASE(fbad); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addfontfile_getsize_failure)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "FailSize", 400, 5, 0);
    x.fl->stream_fail_getsize = E_FAIL;
    IDWriteFontFile *file = fs_file(&x, k);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    if (b1 && file) {
        HRESULT hr = IDWriteFontSetBuilder1_AddFontFile(b1, file);
        CHECKF(FAILED(hr), "GetFileSize failure -> %s", dw_hr_name(hr));
        t_note("AddFontFile with failing GetFileSize -> %s", dw_hr_name(hr));
    } else t_skip("setup");
    DW_RELEASE(file); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addfontfile_loader_create_failure)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "FailCreate", 400, 5, 0);
    IDWriteFontFile *file = fs_file(&x, k);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    if (b1 && file) {
        x.fl->fail_create = E_ACCESSDENIED;
        HRESULT hr = IDWriteFontSetBuilder1_AddFontFile(b1, file);
        x.fl->fail_create = 0;
        CHECKF(FAILED(hr), "CreateStreamFromKey failure -> %s", dw_hr_name(hr));
        t_note("AddFontFile with failing CreateStreamFromKey -> %s", dw_hr_name(hr));
    } else t_skip("setup");
    DW_RELEASE(file); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

/* ---- AddFontFaceReference ------------------------------------------------------------------------- */
TEST(fset_b_addref_null_all_overloads)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (b) {
        fs_prop p; fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "X", "en-us");
        if (!t_is_wine()) {   /* Wine dereferences NULL */
            CHECK_HR(IDWriteFontSetBuilder_AddFontFaceReference(b, NULL), E_INVALIDARG);
            CHECK_HR(IDWriteFontSetBuilder_AddFontFaceReference_(b, NULL, &p.p, 1), E_INVALIDARG);
            CHECK_HR(IDWriteFontSetBuilder_AddFontFaceReference_(b, NULL, NULL, 0), E_INVALIDARG);
        }
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) { CHECK(IDWriteFontSet_GetFontCount(set) == 0); DW_RELEASE(set); }
        DW_RELEASE(b);
    }
    fs_close(&x);
}

TEST(fset_b_addref_props_null_count_nonzero)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "PropsNull", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        CHECK_HR(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, NULL, 1), E_INVALIDARG);
        CHECK_HR(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, NULL, 0xFFFFFFFFu), E_INVALIDARG);
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) { CHECK(IDWriteFontSet_GetFontCount(set) == 0); DW_RELEASE(set); }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_plain_extracts_properties)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "Extracted", 700, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        IDWriteFontSet *set = NULL; char buf[96];
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, r));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            CHECK(IDWriteFontSet_GetFontCount(set) == 1);
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf) && !strcmp(buf, "Extracted"));
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, buf, sizeof buf) && !strcmp(buf, "Extracted Bold"));
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT, buf, sizeof buf) && !strcmp(buf, "700"));
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_plain_equals_addfontfile)
{
    FS_OPEN(x);
    UINT32 k1 = fs_add(&x, "SameProps", 400, 5, 1), k2 = fs_add(&x, "SameProps", 400, 5, 1);
    IDWriteFontFaceReference *r = fs_ref(&x, k1); IDWriteFontSetBuilder *b = fs_builder(&x);
    IDWriteFontSet *viaref = NULL, *viafile = NULL;
    if (r && b) {
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, r));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &viaref));
    }
    CHECK_HR_OK(fs_build_keys(&x, &k2, 1, &viafile));
    if (viaref && viafile) {
        int id;
        for (id = 1; id <= 13; id++) {
            char a[128], c[128]; BOOL ea = fs_font_prop(viaref, 0, (DWRITE_FONT_PROPERTY_ID)id, a, sizeof a), ec = fs_font_prop(viafile, 0, (DWRITE_FONT_PROPERTY_ID)id, c, sizeof c);
            CHECKF(ea == ec && !strcmp(a, c), "id %d: ref \"%s\" (%d) vs file \"%s\" (%d)", id, a, ea, c, ec);
        }
    }
    DW_RELEASE(viaref); DW_RELEASE(viafile); DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_zero_props)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "ZeroProps", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, NULL, 0));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            /* the entry exists but nothing was extracted from the font */
            CHECK(IDWriteFontSet_GetFontCount(set) == 1);
            char buf[64];
            FS_SOFT(!fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf));
            FS_SOFT(!fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, buf, sizeof buf));
            IDWriteFontFaceReference *r2 = NULL; UINT32 idx = 9; BOOL ex = FALSE;
            if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(set, 0, &r2)) && r2) {
                CHECK(IDWriteFontFaceReference_Equals(r, r2));
                CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, r, &idx, &ex));
                CHECK(ex == TRUE && idx == 0);
                DW_RELEASE(r2);
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_props_override)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "RealName", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        fs_prop p[4]; IDWriteFontSet *set = NULL; char buf[96];
        fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Overridden Family", "en-us");
        fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_FULL_NAME, "Overridden Full", "en-us");
        fs_prop_set(&p[2], DWRITE_FONT_PROPERTY_ID_WEIGHT, "300", NULL);
        fs_prop_set(&p[3], DWRITE_FONT_PROPERTY_ID_STYLE, "2", NULL);
        DWRITE_FONT_PROPERTY arr[4]; int i;
        for (i = 0; i < 4; i++) arr[i] = p[i].p;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, arr, 4));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            CHECK(IDWriteFontSet_GetFontCount(set) == 1);
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf) && !strcmp(buf, "Overridden Family"));
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, buf, sizeof buf) && !strcmp(buf, "Overridden Full"));
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT, buf, sizeof buf) && !strcmp(buf, "300"));
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_STYLE, buf, sizeof buf) && !strcmp(buf, "2"));
            /* the real names were not merged in: only the supplied ids have values */
            FS_SOFT(!fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, buf, sizeof buf));
            FS_SOFT(!fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, buf, sizeof buf));
            IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
            if (l) { CHECK(IDWriteStringList_GetCount(l) == 1); CHECK(fs_sl_has(l, "Overridden Family")); CHECK(!fs_sl_has(l, "RealName")); DW_RELEASE(l); }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_props_multi_locale_values)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "MultiLoc", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        fs_prop p[3]; DWRITE_FONT_PROPERTY arr[3]; IDWriteFontSet *set = NULL; int i;
        fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Family EN", "en-us");
        fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Famille FR", "fr-fr");
        fs_prop_set(&p[2], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Familie DE", "de-de");
        for (i = 0; i < 3; i++) arr[i] = p[i].p;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, arr, 3));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            BOOL ex = FALSE; IDWriteLocalizedStrings *ls = NULL;
            CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, &ex, &ls));
            CHECK(ex == TRUE && ls != NULL);
            if (ls) {
                CHECKF(IDWriteLocalizedStrings_GetCount(ls) == 3, "count %u", IDWriteLocalizedStrings_GetCount(ls));
                UINT32 idx = 99; BOOL f = FALSE; WCHAR w[64];
                CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(ls, L"fr-fr", &idx, &f));
                CHECK(f == TRUE);
                if (f) { CHECK_HR_OK(IDWriteLocalizedStrings_GetString(ls, idx, w, 64)); CHECK(!wcscmp(w, L"Famille FR")); }
                CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(ls, L"de-de", &idx, &f));
                CHECK(f == TRUE);
                if (f) { CHECK_HR_OK(IDWriteLocalizedStrings_GetString(ls, idx, w, 64)); CHECK(!wcscmp(w, L"Familie DE")); }
                DW_RELEASE(ls);
            }
            IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
            if (l) { CHECKF(IDWriteStringList_GetCount(l) == 3, "set-wide count %u", IDWriteStringList_GetCount(l)); DW_RELEASE(l); }
            /* locale-filtered flavour: nearest language per font */
            IDWriteStringList *lf = NULL;
            CHECK_HR_OK(IDWriteFontSet_GetPropertyValues_(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, L"fr-fr", &lf));
            if (lf) { CHECK(IDWriteStringList_GetCount(lf) == 1); CHECK(fs_sl_has(lf, "Famille FR")); DW_RELEASE(lf); }
            lf = NULL;
            CHECK_HR_OK(IDWriteFontSet_GetPropertyValues_(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, L"de-de", &lf));
            if (lf) { CHECK(IDWriteStringList_GetCount(lf) == 1); CHECK(fs_sl_has(lf, "Familie DE")); DW_RELEASE(lf); }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_props_locale_null_and_case)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "LocNull", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        fs_prop p[2]; DWRITE_FONT_PROPERTY arr[2]; IDWriteFontSet *set = NULL;
        fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "NoLocale", NULL);
        fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_FULL_NAME, "Mixed Case Locale", "EN-us");
        arr[0] = p[0].p; arr[1] = p[1].p;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, arr, 2));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
            if (l) {
                char loc[64];
                CHECK(IDWriteStringList_GetCount(l) == 1);
                CHECK(fs_sl_has(l, "NoLocale"));
                fs_sl_locale(l, 0, loc, sizeof loc);
                t_note("NULL locale is reported as \"%s\"", loc);
                DW_RELEASE(l);
            }
            l = fs_props(set, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
            if (l) {
                char loc[64];
                CHECK(fs_sl_has(l, "Mixed Case Locale"));
                fs_sl_locale(l, 0, loc, sizeof loc);
                CHECKF(!_stricmp(loc, "en-us"), "locale \"%s\"", loc);
                t_note("\"EN-us\" is normalised to \"%s\"", loc);
                DW_RELEASE(l);
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_props_null_value)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "NullValue", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        DWRITE_FONT_PROPERTY p = { DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, NULL, L"en-us" };
        /* FontSetBuilder::AddProperties throws ArgumentException("sourceProperty.propertyValue") */
        HRESULT hr = IDWriteFontSetBuilder_AddFontFaceReference_(b, r, &p, 1);
        CHECKF(FAILED(hr), "NULL propertyValue -> %s", dw_hr_name(hr));
        t_note("NULL propertyValue -> %s", dw_hr_name(hr));
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_props_all_ids_accepted)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "AllIds", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        fs_prop p[14]; DWRITE_FONT_PROPERTY arr[14]; int i; IDWriteFontSet *set = NULL;
        for (i = 0; i < 14; i++) {
            char v[32];
            if (i == 10) strcpy(v, "500"); else if (i == 11) strcpy(v, "3"); else if (i == 12) strcpy(v, "1"); else snprintf(v, sizeof v, "Value%d", i);
            fs_prop_set(&p[i], (DWRITE_FONT_PROPERTY_ID)i, v, "en-us");
            arr[i] = p[i].p;
        }
        /* ids 0..13, including NONE and the script/semantic tags */
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, arr, 14));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            CHECK(IDWriteFontSet_GetFontCount(set) == 1);
            for (i = 1; i < 14; i++) {
                char buf[64], want[32]; BOOL ex = fs_font_prop(set, 0, (DWRITE_FONT_PROPERTY_ID)i, buf, sizeof buf);
                snprintf(want, sizeof want, "Value%d", i);
                if (i == 10) strcpy(want, "500"); else if (i == 11) strcpy(want, "3"); else if (i == 12) strcpy(want, "1");
                if (i == 7 || i == 8 || i == 9) { t_note("id %d: exists=%d \"%s\"", i, ex, buf); continue; }   /* language tags are validated/normalised */
                CHECKF(ex && !strcmp(buf, want), "id %d: exists=%d \"%s\" want \"%s\"", i, ex, buf, want);
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_simulations_key)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "SimKey", 400, 5, 0);
    IDWriteFontFaceReference *plain = fs_ref(&x, k), *bold = fs_ref_ex(&x, k, 0, DWRITE_FONT_SIMULATIONS_BOLD);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (plain && bold && b) {
        CHECK(!IDWriteFontFaceReference_Equals(plain, bold));
        CHECK(IDWriteFontFaceReference_GetSimulations(bold) == DWRITE_FONT_SIMULATIONS_BOLD);
        IDWriteFontSet *set = NULL; UINT32 idx = 0; BOOL ex = TRUE;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, bold));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            CHECK(IDWriteFontSet_GetFontCount(set) >= 1);
            CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, bold, &idx, &ex));
            CHECK(ex == TRUE);
            CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, plain, &idx, &ex));
            FS_SOFT(ex == FALSE);   /* the key includes the simulations */
            t_note("plain reference found in a set holding only the bold-simulated one: %d", ex);
            IDWriteFontFaceReference *r = NULL;
            if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(set, 0, &r)) && r) {
                CHECK(IDWriteFontFaceReference_GetSimulations(r) == DWRITE_FONT_SIMULATIONS_BOLD);
                DW_RELEASE(r);
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(plain); DW_RELEASE(bold); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addref_face_index_preserved)
{
    FS_OPEN(x);
    ttf_spec s1, s2; size_t n1, n2, nt;
    fs_style(&s1, "IdxA", 400, 5, 0); fs_style(&s2, "IdxB", 400, 5, 0);
    uint8_t *b1 = ttf_build(&s1, &n1), *b2 = ttf_build(&s2, &n2);
    const uint8_t *blobs[2] = { b1, b2 }; size_t sizes[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(blobs, sizes, 2, &nt);
    UINT32 k = fs_add_blob(&x, ttc, nt);
    IDWriteFontFaceReference *r1 = fs_ref_ex(&x, k, 1, DWRITE_FONT_SIMULATIONS_NONE); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r1 && b) {
        IDWriteFontSet *set = NULL;
        CHECK(IDWriteFontFaceReference_GetFontFaceIndex(r1) == 1);
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, r1));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            char buf[64];
            CHECK(IDWriteFontSet_GetFontCount(set) == 1);
            /* only the second face of the collection is in the set, and its properties come from that face */
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf) && !strcmp(buf, "IdxB"));
            IDWriteFontFaceReference *r = NULL;
            if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(set, 0, &r)) && r) { CHECK(IDWriteFontFaceReference_GetFontFaceIndex(r) == 1); DW_RELEASE(r); }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r1); DW_RELEASE(b);
    free(b1); free(b2); free(ttc);
    fs_close(&x);
}

/* ---- AddFontSet ----------------------------------------------------------------------------------- */
TEST(fset_b_addfontset_null)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (b) { CHECK_HR(IDWriteFontSetBuilder_AddFontSet(b, NULL), E_INVALIDARG); DW_RELEASE(b); }
    fs_close(&x);
}

TEST(fset_b_addfontset_roundtrip)
{
    FS_STD(s);
    IDWriteFontSetBuilder *b = fs_builder(&s.x);
    if (b) {
        IDWriteFontSet *copy = NULL; UINT32 i, n = IDWriteFontSet_GetFontCount(s.set);
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(b, s.set));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &copy));
        if (copy) {
            CHECK(copy != s.set);
            CHECKF(IDWriteFontSet_GetFontCount(copy) == n, "count %u vs %u", IDWriteFontSet_GetFontCount(copy), n);
            for (i = 0; i < n; i++) {
                IDWriteFontFaceReference *r = NULL; UINT32 idx = 0; BOOL ex = FALSE;
                if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(s.set, i, &r)) && r) {
                    CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(copy, r, &idx, &ex));
                    CHECKF(ex == TRUE, "ref %u in copy", i);
                    if (ex) {
                        int id;
                        for (id = 1; id <= 13; id++) {
                            char a[128], c[128];
                            BOOL ea = fs_font_prop(s.set, i, (DWRITE_FONT_PROPERTY_ID)id, a, sizeof a), ec = fs_font_prop(copy, idx, (DWRITE_FONT_PROPERTY_ID)id, c, sizeof c);
                            CHECKF(ea == ec && !strcmp(a, c), "font %u id %d \"%s\" vs \"%s\"", i, id, a, c);
                        }
                    }
                    DW_RELEASE(r);
                }
            }
            DW_RELEASE(copy);
        }
        DW_RELEASE(b);
    }
    fs_std_close(&s);
}

TEST(fset_b_addfontset_plus_file)
{
    FS_STD(s);
    UINT32 k = fs_add(&s.x, "ExtraFam", 400, 5, 0);
    IDWriteFontSetBuilder *b = fs_builder(&s.x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    IDWriteFontFile *file = fs_file(&s.x, k);
    if (b1 && file) {
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(b, s.set));
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, file));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            CHECK(IDWriteFontSet_GetFontCount(set) == 6);
            CHECK(fs_find_by(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "ExtraFam") != FS_NOT_FOUND);
            CHECK(fs_find_by(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB") != FS_NOT_FOUND);
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(file); DW_RELEASE(b1); DW_RELEASE(b);
    fs_std_close(&s);
}

TEST(fset_b_addfontset_twice)
{
    FS_STD(s);
    IDWriteFontSetBuilder *b = fs_builder(&s.x);
    if (b) {
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(b, s.set));
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(b, s.set));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            UINT32 n = IDWriteFontSet_GetFontCount(set);
            CHECKF(n == 5 || n == 10, "count %u", n);
            t_note("the same 5-font set added twice -> %u entries", n);
            DW_RELEASE(set);
        }
        DW_RELEASE(b);
    }
    fs_std_close(&s);
}

TEST(fset_b_addfontset_empty)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *e = fs_builder(&x), *b = fs_builder(&x);
    if (e && b) {
        IDWriteFontSet *empty = NULL, *out = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(e, &empty));
        if (empty) {
            CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(b, empty));
            CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &out));
            if (out) { CHECK(IDWriteFontSet_GetFontCount(out) == 0); DW_RELEASE(out); }
            DW_RELEASE(empty);
        }
    }
    DW_RELEASE(e); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_b_addfontset_preserves_overrides)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "Hidden", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x), *b2 = fs_builder(&x);
    if (r && b && b2) {
        fs_prop p; DWRITE_FONT_PROPERTY arr[1]; IDWriteFontSet *s1 = NULL, *s2 = NULL;
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Visible Name", "en-us"); arr[0] = p.p;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, arr, 1));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s1));
        if (s1) {
            CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(b2, s1));
            CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b2, &s2));
            if (s2) {
                char buf[64];
                CHECK(IDWriteFontSet_GetFontCount(s2) == 1);
                /* the overriding properties travel with the item; the real family name is not re-extracted */
                CHECK(fs_font_prop(s2, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf) && !strcmp(buf, "Visible Name"));
                DW_RELEASE(s2);
            }
            DW_RELEASE(s1);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b); DW_RELEASE(b2);
    fs_close(&x);
}

TEST(fset_b_addfontset_system_subset)
{
    NEED_SYSSET(sys);
    FS_OPEN(x);
    IDWriteFontSet *arial = NULL;
    if (FAILED(IDWriteFontSet_GetMatchingFonts_(sys, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &arial)) || !arial
        || IDWriteFontSet_GetFontCount(arial) == 0) {
        t_skip("Arial not in the system set"); DW_RELEASE(arial); fs_close(&x); DW_RELEASE(sys); return;
    }
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (b) {
        IDWriteFontSet *out = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(b, arial));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &out));
        if (out) {
            UINT32 na = IDWriteFontSet_GetFontCount(arial), no = IDWriteFontSet_GetFontCount(out);
            CHECKF(no == na, "count %u vs %u", no, na);
            DW_RELEASE(out);
        }
        DW_RELEASE(b);
    }
    DW_RELEASE(arial); fs_close(&x); DW_RELEASE(sys);
}

/* ---- IDWriteFontSetBuilder2::AddFont and AddFontFile(path) ------------------------------------------- */
#define NEED_B2(x, b2, file, key) \
    fs_fx x; if (!fs_open(&x)) return; \
    UINT32 key = fs_add(&x, "AddFontFam", 400, 5, 0); IDWriteFontFile *file = fs_file(&x, key); \
    IDWriteFontSetBuilder *bb_ = fs_builder(&x); IDWriteFontSetBuilder2 *b2 = NULL; \
    if (bb_) IDWriteFontSetBuilder_QueryInterface(bb_, &IID_IDWriteFontSetBuilder2, (void **)&b2); \
    if (!b2 || !file) { t_skip("setup"); DW_RELEASE(b2); DW_RELEASE(bb_); DW_RELEASE(file); fs_close(&x); return; }
#define END_B2(x, b2, file) DW_RELEASE(b2); DW_RELEASE(bb_); DW_RELEASE(file); fs_close(&x)

TEST(fset_b2_addfont_invalid_args)
{
    NEED_B2(x, b2, file, key);
    fs_prop p; fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Z", "en-us");
    DWRITE_FONT_AXIS_VALUE av[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
    DWRITE_FONT_AXIS_RANGE ar[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 100.0f, 900.0f } };
    struct { const char *what; IDWriteFontFile *f; DWRITE_FONT_SIMULATIONS sim; const DWRITE_FONT_AXIS_VALUE *av; UINT32 nav; const DWRITE_FONT_AXIS_RANGE *ar; UINT32 nar; const DWRITE_FONT_PROPERTY *pr; UINT32 npr; } t[] = {
        { "NULL file", NULL, 0, NULL, 0, NULL, 0, &p.p, 1 },
        { "simulations 4", file, (DWRITE_FONT_SIMULATIONS)4, NULL, 0, NULL, 0, &p.p, 1 },
        { "simulations 0x80000000", file, (DWRITE_FONT_SIMULATIONS)0x80000000u, NULL, 0, NULL, 0, &p.p, 1 },
        { "simulations 0xFFFFFFFC", file, (DWRITE_FONT_SIMULATIONS)0xFFFFFFFCu, NULL, 0, NULL, 0, &p.p, 1 },
        { "axis count 0x10000", file, 0, av, 0x10000, NULL, 0, &p.p, 1 },
        { "axis count 0xFFFFFFFF", file, 0, av, 0xFFFFFFFFu, NULL, 0, &p.p, 1 },
        { "NULL axis values, count 1", file, 0, NULL, 1, NULL, 0, &p.p, 1 },
        { "NULL ranges, count 1", file, 0, NULL, 0, NULL, 1, &p.p, 1 },
        { "NULL ranges, count 0xFFFFFFFF", file, 0, NULL, 0, NULL, 0xFFFFFFFFu, &p.p, 1 },
        { "NULL props, count 1", file, 0, av, 1, ar, 1, NULL, 1 },
        { "NULL props, count 0xFFFFFFFF", file, 0, NULL, 0, NULL, 0, NULL, 0xFFFFFFFFu },
    };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        HRESULT hr = IDWriteFontSetBuilder2_AddFont(b2, t[i].f, 0, t[i].sim, t[i].av, t[i].nav, t[i].ar, t[i].nar, t[i].pr, t[i].npr);
        CHECKF(hr == E_INVALIDARG, "%s -> %s", t[i].what, dw_hr_name(hr));
    }
    /* none of the rejected calls added anything */
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(b2, &set));
    if (set) { CHECK(IDWriteFontSet_GetFontCount(set) == 0); DW_RELEASE(set); }
    END_B2(x, b2, file);
}

TEST(fset_b2_addfont_simulation_values_accepted)
{
    NEED_B2(x, b2, file, key);
    fs_prop p; fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SimFam", "en-us");
    int sim;
    for (sim = 0; sim <= 3; sim++) {
        HRESULT hr = IDWriteFontSetBuilder2_AddFont(b2, file, 0, (DWRITE_FONT_SIMULATIONS)sim, NULL, 0, NULL, 0, &p.p, 1);
        CHECKF(hr == S_OK, "simulations %d -> %s", sim, dw_hr_name(hr));
    }
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(b2, &set));
    if (set) {
        UINT32 n = IDWriteFontSet_GetFontCount(set);
        CHECKF(n >= 1 && n <= 4, "count %u", n);
        t_note("four simulation combinations of one face -> %u entries", n);
        DW_RELEASE(set);
    }
    END_B2(x, b2, file);
}

TEST(fset_b2_addfont_basic)
{
    NEED_B2(x, b2, file, key);
    fs_prop p[3]; DWRITE_FONT_PROPERTY arr[3]; int i;
    fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "AddFont Family", "en-us");
    fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_WEIGHT, "250", NULL);
    fs_prop_set(&p[2], DWRITE_FONT_PROPERTY_ID_STRETCH, "3", NULL);
    for (i = 0; i < 3; i++) arr[i] = p[i].p;
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(b2, file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, arr, 3));
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(b2, &set));
    if (set) {
        char buf[64];
        CHECK(IDWriteFontSet_GetFontCount(set) == 1);
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf) && !strcmp(buf, "AddFont Family"));
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT, buf, sizeof buf) && !strcmp(buf, "250"));
        CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_STRETCH, buf, sizeof buf) && !strcmp(buf, "3"));
        DW_RELEASE(set);
    }
    END_B2(x, b2, file);
}

TEST(fset_b2_addfont_no_props)
{
    NEED_B2(x, b2, file, key);
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(b2, file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 0));
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(b2, &set));
    if (set) {
        CHECK(IDWriteFontSet_GetFontCount(set) == 1);
        char buf[64];
        /* like AddFontFaceReference with an empty property list: nothing is extracted */
        FS_SOFT(!fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf));
        DW_RELEASE(set);
    }
    END_B2(x, b2, file);
}

TEST(fset_b2_addfont_axis_count_limit)
{
    NEED_B2(x, b2, file, key);
    UINT32 n = 0xFFFF;
    DWRITE_FONT_AXIS_VALUE *av = (DWRITE_FONT_AXIS_VALUE *)calloc(n, sizeof *av);
    if (av) {
        UINT32 i;
        for (i = 0; i < n; i++) { av[i].axisTag = DWRITE_FONT_AXIS_TAG_WEIGHT; av[i].value = 400.0f; }
        fs_prop p; fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "BigAxis", "en-us");
        /* 0xFFFF is the largest accepted axis value count */
        HRESULT hr = IDWriteFontSetBuilder2_AddFont(b2, file, 0, DWRITE_FONT_SIMULATIONS_NONE, av, n, NULL, 0, &p.p, 1);
        CHECK_HR_OK(hr);
        free(av);
    }
    END_B2(x, b2, file);
}

TEST(fset_b2_addfont_axis_ranges_and_values_on_static_font)
{
    NEED_B2(x, b2, file, key);
    DWRITE_FONT_AXIS_VALUE av[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
    DWRITE_FONT_AXIS_RANGE ar[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 100.0f, 900.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 50.0f, 200.0f } };
    fs_prop p; fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "AxisFam", "en-us");
    HRESULT hr = IDWriteFontSetBuilder2_AddFont(b2, file, 0, DWRITE_FONT_SIMULATIONS_NONE, av, 1, ar, 2, &p.p, 1);
    CHECK_HR_OK(hr);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(b2, &set));
    if (set) {
        CHECK(IDWriteFontSet_GetFontCount(set) == 1);
        IDWriteFontSet1 *s1 = fs_set1(set);
        if (s1) {
            UINT32 actual = 0; DWRITE_FONT_AXIS_RANGE out[8];
            HRESULT h2 = IDWriteFontSet1_GetFontAxisRanges_(s1, 0, out, 8, &actual);
            (void)h2;
            /* the ranges given to AddFont are reported back (slot 0 form: the whole set's ranges) */
            t_note("GetFontAxisRanges -> %s, %u ranges", dw_hr_name(h2), actual);
            DW_RELEASE(s1);
        }
        DW_RELEASE(set);
    }
    END_B2(x, b2, file);
}

TEST(fset_b2_addfont_face_index_not_validated)
{
    NEED_B2(x, b2, file, key);
    fs_prop p; fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "BadIndex", "en-us");
    HRESULT hr = IDWriteFontSetBuilder2_AddFont(b2, file, 7, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, &p.p, 1);
    CHECK_HR_OK(hr);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(b2, &set));
    if (set) {
        FS_SOFT(IDWriteFontSet_GetFontCount(set) == 1);
        IDWriteFontFaceReference *r = NULL;
        if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(set, 0, &r)) && r) {
            FS_SOFT(IDWriteFontFaceReference_GetFontFaceIndex(r) == 7);
            IDWriteFontFace3 *face = (IDWriteFontFace3 *)FS_SENT;
            HRESULT h2 = IDWriteFontFaceReference_CreateFontFace(r, &face);
            CHECKF(FAILED(h2), "CreateFontFace for face index 7 of a one-face file -> %s", dw_hr_name(h2));
            CHECK_OUT_NULL(face);
            FS_DROP(face, h2);
            DW_RELEASE(r);
        }
        DW_RELEASE(set);
    }
    END_B2(x, b2, file);
}

TEST(fset_b2_addfontfile_path)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *bb = fs_builder(&x); IDWriteFontSetBuilder2 *b2 = NULL;
    if (bb) IDWriteFontSetBuilder_QueryInterface(bb, &IID_IDWriteFontSetBuilder2, (void **)&b2);
    if (!b2) { t_skip("no builder2"); DW_RELEASE(bb); fs_close(&x); return; }
    {
        size_t n; ttf_spec s; fs_style(&s, "PathFam", 400, 5, 0);
        uint8_t *blob = ttf_build(&s, &n);
        char path[MAX_PATH]; WCHAR wpath[MAX_PATH]; FILE *fp;
        t_tmpfile(path, sizeof path, "pathfam.ttf");
        fp = fopen(path, "wb");
        if (fp) { fwrite(blob, 1, n, fp); fclose(fp); }
        free(blob);
        dw_wide(path, wpath, MAX_PATH);
        HRESULT hr = IDWriteFontSetBuilder2_AddFontFile(b2, wpath);
        CHECK_HR_OK(hr);
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(b2, &set));
        if (set) {
            char buf[64];
            CHECK(IDWriteFontSet_GetFontCount(set) == 1);
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf) && !strcmp(buf, "PathFam"));
            DW_RELEASE(set);
        }
        DeleteFileA(path);
    }
    DW_RELEASE(b2); DW_RELEASE(bb);
    fs_close(&x);
}

TEST(fset_b2_addfontfile_path_errors)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *bb = fs_builder(&x); IDWriteFontSetBuilder2 *b2 = NULL;
    if (bb) IDWriteFontSetBuilder_QueryInterface(bb, &IID_IDWriteFontSetBuilder2, (void **)&b2);
    if (!b2) { t_skip("no builder2"); DW_RELEASE(bb); fs_close(&x); return; }
    CHECK_HR(IDWriteFontSetBuilder2_AddFontFile(b2, NULL), E_INVALIDARG);
    {
        char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
        t_tmpfile(path, sizeof path, "does_not_exist.ttf");
        dw_wide(path, wpath, MAX_PATH);
        HRESULT hr = IDWriteFontSetBuilder2_AddFontFile(b2, wpath);
        CHECKF(FAILED(hr), "missing file -> %s", dw_hr_name(hr));
        t_note("AddFontFile(missing path) -> %s", dw_hr_name(hr));
        /* garbage content */
        t_tmpfile(path, sizeof path, "garbage.ttf");
        FILE *fp = fopen(path, "wb");
        if (fp) { fwrite("this is not a font file at all, just text", 1, 41, fp); fclose(fp); }
        dw_wide(path, wpath, MAX_PATH);
        hr = IDWriteFontSetBuilder2_AddFontFile(b2, wpath);
        CHECKF(FAILED(hr), "garbage file -> %s", dw_hr_name(hr));
        t_note("AddFontFile(garbage file) -> %s", dw_hr_name(hr));
        DeleteFileA(path);
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(b2, &set));
        if (set) { CHECK(IDWriteFontSet_GetFontCount(set) == 0); DW_RELEASE(set); }
    }
    DW_RELEASE(b2); DW_RELEASE(bb);
    fs_close(&x);
}
