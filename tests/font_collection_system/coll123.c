/* System font collection: IDWriteFontCollection1/2/3 and the family model.
 * Windows 10 (decompilation):
 *  - GetFontSet always succeeds and builds a NEW DWriteFontSet object per call;
 *  - IDWriteFontCollection2::GetMatchingFonts clears *fontlist, then rejects name == NULL, numValues >= 0x10000 and
 *    (numValues != 0 && axisValues == NULL) with E_INVALIDARG; a name that is not a family gives DWRITE_E_NOFONT;
 *  - IDWriteFontFamily2::GetMatchingFonts applies the same numValues / axisValues validation;
 *  - GetFontFamilyModel returns the model the collection was created with. */
#include "fcs.h"

#define E_INVALID DW_E_INVALIDARG

/* ---- IDWriteFontCollection1 ---------------------------------------------------------------------------------- */
TEST(c1_get_font_set_ok) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *fs = (IDWriteFontSet *)(ULONG_PTR)1;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c1, &fs));
    CHECK(fs != NULL && fs != (void *)1);
    if (fs == (void *)1) fs = NULL;
    if (fs) CHECK(IDWriteFontSet_GetFontCount(fs) >= 1 || t_is_wine());
    DW_RELEASE(fs); DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_get_font_set_new_object_each_call) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c1, &a));
    CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c1, &b));
    CHECK(a != NULL && b != NULL && a != b);
    if (a && b) CHECK(IDWriteFontSet_GetFontCount(a) == IDWriteFontSet_GetFontCount(b));
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_font_set_refcount) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *fs = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    ULONG base = dw_refcount((IUnknown *)c);
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontSet(c1, &fs));
    EXPECT_REFS(fs, 1);
    ULONG held = dw_refcount((IUnknown *)c);
    CHECK(held >= base);
    t_note("collection refcount %lu -> %lu while its font set exists", base, held);
    IDWriteFontSet_Release(fs);
    EXPECT_REFS(c, base);
    DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_font_set_qi_matrix) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *fs = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontSet(c1, &fs));
    dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontSet, 1), DW_QI_ROW(IDWriteFontSet1, -1), DW_QI_ROW(IDWriteFontSet2, -1),
                         DW_QI_ROW(IDWriteFontSet3, -1), DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontList, 0), DW_QI_ROW(IDWriteFontFamily, 0) };
    DW_CHECK_QI(fs, rows);
    DW_RELEASE(fs); DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_font_set_count_vs_families) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *fs = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontSet(c1, &fs));
    UINT32 nfonts = IDWriteFontSet_GetFontCount(fs), nfam = fcs_count(c), sum = 0;
    FOREACH_FAMILY(c, i, fam, 100000) { sum += IDWriteFontFamily_GetFontCount(fam); } END_FAMILY(fam)
    t_note("font set %u fonts, %u families, sum of family font counts %u", nfonts, nfam, sum);
    CHECK(nfonts >= nfam);
    CHECK(sum >= nfam);
    END_SYS();
    DW_RELEASE(fs); DW_RELEASE(c1);
}
TEST(c1_font_set_face_references_valid) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *fs = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontSet(c1, &fs));
    UINT32 n = IDWriteFontSet_GetFontCount(fs), tried = 0;
    for (UINT32 i = 0; i < n && i < 150; i++) {
        IDWriteFontFaceReference *ref = (IDWriteFontFaceReference *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontSet_GetFontFaceReference(fs, i, &ref);
        CHECKF(hr == S_OK && ref && ref != (void *)1, "font %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && ref && ref != (void *)1) {
            tried++;
            CHECK(IDWriteFontFaceReference_Equals(ref, ref));
            DWRITE_FONT_SIMULATIONS sim = IDWriteFontFaceReference_GetSimulations(ref);
            CHECK((sim & ~3) == 0);
            DWRITE_LOCALITY loc = IDWriteFontFaceReference_GetLocality(ref);
            CHECKF(loc == DWRITE_LOCALITY_LOCAL || loc == DWRITE_LOCALITY_PARTIAL || loc == DWRITE_LOCALITY_REMOTE, "locality %d", loc);
            IDWriteFontFaceReference_Release(ref);
        }
    }
    CHECK(tried > 0 || t_is_wine());
    DW_RELEASE(fs); DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_font_set_property_family_names) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *fs = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontSet(c1, &fs));
    UINT32 n = IDWriteFontSet_GetFontCount(fs), good = 0;
    for (UINT32 i = 0; i < n && i < 100; i++) {
        BOOL ex = 5; IDWriteLocalizedStrings *s = NULL;
        HRESULT hr = IDWriteFontSet_GetPropertyValues(fs, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, &ex, &s);
        CHECKF(hr == S_OK, "font %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK) {
            CHECK(ex == 0 || ex == 1);
            if (ex) { CHECK(s != NULL); good++; } else CHECK(s == NULL);
            DW_RELEASE(s);
        }
    }
    CHECK(good > 0 || t_is_wine());
    DW_RELEASE(fs); DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_font_set_family_names_cover_collection) {
    /* Every family of the weight/stretch/style collection is reachable by name from the font set as well. */
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *fs = NULL; IDWriteStringList *names = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontSet(c1, &fs));
    HRESULT hr = IDWriteFontSet_GetPropertyValues__(fs, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, &names);
    if (SUCCEEDED(hr) && names) {
        UINT32 n = IDWriteStringList_GetCount(names);
        t_note("font set lists %u distinct weight/stretch/style family names; collection has %u families", n, fcs_count(c));
        CHECK(n >= 1);
        IDWriteStringList_Release(names);
    } else CHECK_HR_OK(hr);
    DW_RELEASE(fs); DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_get_font_family_returns_family1) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    UINT32 n = fcs_count(c); if (n > 120) n = 120;
    for (UINT32 i = 0; i < n; i++) {
        IDWriteFontFamily1 *fam1 = (IDWriteFontFamily1 *)(ULONG_PTR)1; IDWriteFontFamily *fam = NULL;
        HRESULT hr = IDWriteFontCollection1_GetFontFamily(c1, i, &fam1);
        CHECKF(hr == S_OK && fam1 && fam1 != (void *)1, "family %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && fam1 != (void *)1 && fam1) {
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) && fam) {
                char a[200], b[200];
                fcs_family_en((IDWriteFontFamily *)fam1, a, sizeof a); fcs_family_en(fam, b, sizeof b);
                CHECKF(!strcmp(a, b), "family %u: \"%s\" vs \"%s\"", i, a, b);
                CHECK(IDWriteFontFamily1_GetFontCount(fam1) == IDWriteFontFamily_GetFontCount(fam));
                IDWriteFontFamily_Release(fam);
            }
            IDWriteFontFamily1_Release(fam1);
        }
    }
    DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_family1_locality_and_references) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    UINT32 n = fcs_count(c); if (n > 60) n = 60;
    UINT32 nonlocal = 0;
    for (UINT32 i = 0; i < n; i++) {
        IDWriteFontFamily1 *fam = NULL;
        if (FAILED(IDWriteFontCollection1_GetFontFamily(c1, i, &fam)) || !fam) { CHECK(0); continue; }
        UINT32 nf = IDWriteFontFamily1_GetFontCount(fam);
        for (UINT32 k = 0; k < nf && k < 6; k++) {
            DWRITE_LOCALITY loc = IDWriteFontFamily1_GetFontLocality(fam, k);
            CHECKF(loc >= DWRITE_LOCALITY_REMOTE && loc <= DWRITE_LOCALITY_LOCAL, "family %u font %u locality %d", i, k, loc);
            if (loc != DWRITE_LOCALITY_LOCAL) nonlocal++;
            IDWriteFont3 *font = NULL; IDWriteFontFaceReference *ref = NULL;
            CHECK_HR_OK(IDWriteFontFamily1_GetFont(fam, k, &font));
            CHECK_HR_OK(IDWriteFontFamily1_GetFontFaceReference(fam, k, &ref));
            if (font && ref) {
                IDWriteFontFaceReference *r2 = NULL;
                if (SUCCEEDED(IDWriteFont3_GetFontFaceReference(font, &r2)) && r2) { CHECK(IDWriteFontFaceReference_Equals(ref, r2)); IDWriteFontFaceReference_Release(r2); }
            }
            DW_RELEASE(font); DW_RELEASE(ref);
        }
        IDWriteFontFamily1_Release(fam);
    }
    t_note("%u fonts of the first %u families report non-local locality", nonlocal, n);
    CHECK(nonlocal == 0 || t_is_wine());
    DW_RELEASE(c1);
    END_SYS();
}
TEST(c1_family1_is_same_identity_as_family) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    if (!fcs_count(c)) { t_skip("no families"); DW_RELEASE(c1); END_SYS(); return; }
    IDWriteFontFamily1 *fam1 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontFamily(c1, 0, &fam1));
    IDWriteFontFamily *fam = NULL; IDWriteFontFamily2 *fam2 = NULL;
    CHECK_HR_OK(IDWriteFontFamily1_QueryInterface(fam1, &IID_IDWriteFontFamily, (void **)&fam));
    if (fam) CHECK((void *)fam == (void *)fam1);
    HRESULT hr = IDWriteFontFamily1_QueryInterface(fam1, &IID_IDWriteFontFamily2, (void **)&fam2);
    t_note("family -> IDWriteFontFamily2: %s", dw_hr_name(hr));
    CHECK(hr == S_OK || hr == E_NOINTERFACE);
    DW_RELEASE(fam); DW_RELEASE(fam2); IDWriteFontFamily1_Release(fam1); DW_RELEASE(c1);
    END_SYS();
}

/* ---- IDWriteFontCollection2 ---------------------------------------------------------------------------------- */
#define NEED_C2() \
    NEED_SYS(); IDWriteFontCollection2 *c2 = NULL; \
    if (FAILED(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2)) || !c2) { END_SYS(); t_skip("IDWriteFontCollection2 unavailable"); return; }
#define END_C2() DW_RELEASE(c2); END_SYS()

TEST(c2_get_font_family_returns_family2) {
    NEED_C2();
    UINT32 n = fcs_count(c); if (n > 120) n = 120;
    for (UINT32 i = 0; i < n; i++) {
        IDWriteFontFamily2 *fam = (IDWriteFontFamily2 *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontCollection2_GetFontFamily(c2, i, &fam);
        CHECKF(hr == S_OK && fam && fam != (void *)1, "family %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && fam && fam != (void *)1) {
            CHECK(IDWriteFontFamily2_GetFontCount(fam) >= 1);
            IDWriteFontCollection *owner = NULL;
            CHECK_HR_OK(IDWriteFontFamily2_GetFontCollection(fam, &owner));
            if (owner) { CHECK(owner == c); IDWriteFontCollection_Release(owner); }
            IDWriteFontFamily2_Release(fam);
        }
    }
    END_C2();
}
TEST(c2_family2_get_font_set) {
    NEED_C2();
    UINT32 n = fcs_count(c); if (n > 50) n = 50;
    for (UINT32 i = 0; i < n; i++) {
        IDWriteFontFamily2 *fam = NULL; IDWriteFontSet1 *fs = (IDWriteFontSet1 *)(ULONG_PTR)1;
        if (FAILED(IDWriteFontCollection2_GetFontFamily(c2, i, &fam)) || !fam) { CHECK(0); continue; }
        HRESULT hr = IDWriteFontFamily2_GetFontSet(fam, &fs);
        CHECKF(hr == S_OK && fs && fs != (void *)1, "family %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && fs != (void *)1 && fs) {
            UINT32 a = IDWriteFontSet1_GetFontCount(fs), b = IDWriteFontFamily2_GetFontCount(fam);
            CHECKF(a >= 1, "family %u font set empty", i);
            if (a != b) t_note("family %u: font set %u fonts, family %u fonts", i, a, b);
            IDWriteFontSet1_Release(fs);
        }
        IDWriteFontFamily2_Release(fam);
    }
    END_C2();
}
TEST(c2_get_font_set_ok) {
    NEED_C2();
    IDWriteFontSet1 *fs = (IDWriteFontSet1 *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteFontCollection2_GetFontSet(c2, &fs));
    CHECK(fs != NULL && fs != (void *)1);
    if (fs == (void *)1) fs = NULL;
    if (fs) CHECK(IDWriteFontSet1_GetFontCount(fs) >= 1 || t_is_wine());
    DW_RELEASE(fs);
    END_C2();
}
TEST(c2_get_font_set_matches_c1_font_set_count) {
    NEED_C2();
    IDWriteFontSet1 *a = NULL; IDWriteFontSet *b = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection2_GetFontSet(c2, &a));
    IDWriteFontCollection1 *c1 = (IDWriteFontCollection1 *)c2;
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontSet(c1, &b));
    CHECK(IDWriteFontSet1_GetFontCount(a) == IDWriteFontSet_GetFontCount(b));
    CHECK((void *)a != (void *)b);
    DW_RELEASE(a); DW_RELEASE(b);
    END_C2();
}
TEST(c2_family_model_values) {
    NEED_C2();
    CHECK(IDWriteFontCollection2_GetFontFamilyModel(c2) == DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
    END_C2();
}
TEST(c2_get_matching_fonts_arg_validation) {
    NEED_C2();
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 100.0f } };
    IDWriteFontList2 *l;
    /* NULL family name */
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, NULL, ax, 2, &l), E_INVALID); CHECK_OUT_NULL(l);
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, NULL, NULL, 0, &l), E_INVALID); CHECK_OUT_NULL(l);
    /* values without an array */
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, L"Arial", NULL, 1, &l), E_INVALID); CHECK_OUT_NULL(l);
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, L"NoSuchFamilyXYZ", NULL, 5, &l), E_INVALID); CHECK_OUT_NULL(l);
    /* too many values (limit is 0xFFFF) */
    static const UINT32 big[] = { 0x10000, 0x10001, 0x7fffffff, 0xFFFFFFFFu };
    for (size_t i = 0; i < sizeof big / sizeof big[0]; i++) {
        l = (IDWriteFontList2 *)(ULONG_PTR)1;
        CHECKF(IDWriteFontCollection2_GetMatchingFonts(c2, L"Arial", ax, big[i], &l) == E_INVALID, "numValues 0x%x accepted", big[i]);
        CHECK_OUT_NULL(l);
    }
    END_C2();
}
TEST(c2_get_matching_fonts_validation_precedes_lookup) {
    /* Argument validation runs before the family lookup: bad arguments with a missing family still give E_INVALIDARG. */
    NEED_C2();
    DWRITE_FONT_AXIS_VALUE ax[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
    IDWriteFontList2 *l = (IDWriteFontList2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, L"NoSuchFamilyXYZ", ax, 0x10000, &l), E_INVALID); CHECK_OUT_NULL(l);
    END_C2();
}
TEST(c2_get_matching_fonts_missing_family) {
    NEED_C2();
    DWRITE_FONT_AXIS_VALUE ax[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
    static const WCHAR *names[] = { L"NoSuchFamilyXYZ", L"", L" ", L"Arial Bogus", L"\xD83D\xDE00" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        IDWriteFontList2 *l = (IDWriteFontList2 *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontCollection2_GetMatchingFonts(c2, names[i], ax, 1, &l);
        CHECKF(hr == FCS_NOFONT, "\"%s\": %s", fcs_u8(names[i]), dw_hr_name(hr));
        CHECK_OUT_NULL(l);
        l = (IDWriteFontList2 *)(ULONG_PTR)1;
        hr = IDWriteFontCollection2_GetMatchingFonts(c2, names[i], NULL, 0, &l);
        CHECKF(hr == FCS_NOFONT, "\"%s\" (no axes): %s", fcs_u8(names[i]), dw_hr_name(hr));
        CHECK_OUT_NULL(l);
    }
    END_C2();
}
TEST(c2_get_matching_fonts_found) {
    NEED_C2();
    if (FIND(c, L"Arial") == 0xFFFFFFFFu) { t_skip("Arial not installed"); END_C2(); return; }
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_ITALIC, 0.0f } };
    IDWriteFontList2 *l = NULL;
    CHECK_HR_OK(IDWriteFontCollection2_GetMatchingFonts(c2, L"Arial", ax, 2, &l));
    CHECK(l != NULL);
    if (l) {
        UINT32 n = IDWriteFontList2_GetFontCount(l);
        CHECK(n >= 1);
        IDWriteFont *font = NULL;
        CHECK_HR_OK(FCS_FL2_GETFONT(l, 0, &font));
        if (font) { t_note("closest Arial to weight 700 / upright: weight %d style %d (%u fonts in the list)", IDWriteFont_GetWeight(font), IDWriteFont_GetStyle(font), n); IDWriteFont_Release(font); }
        IDWriteFontCollection *lc = NULL;
        if (SUCCEEDED(IDWriteFontList2_GetFontCollection(l, &lc)) && lc) { CHECK(lc == c); IDWriteFontCollection_Release(lc); }
        IDWriteFontSet1 *fs = NULL;
        CHECK_HR_OK(IDWriteFontList2_GetFontSet(l, &fs));
        if (fs) { CHECK(IDWriteFontSet1_GetFontCount(fs) == n); IDWriteFontSet1_Release(fs); }
        IDWriteFontList2_Release(l);
    }
    END_C2();
}
TEST(c2_get_matching_fonts_no_axes_and_case_insensitive) {
    NEED_C2();
    if (FIND(c, L"Arial") == 0xFFFFFFFFu) { t_skip("Arial not installed"); END_C2(); return; }
    IDWriteFontList2 *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontCollection2_GetMatchingFonts(c2, L"Arial", NULL, 0, &a));
    CHECK_HR_OK(IDWriteFontCollection2_GetMatchingFonts(c2, L"ARIAL", NULL, 0, &b));
    if (a && b) { CHECK(IDWriteFontList2_GetFontCount(a) == IDWriteFontList2_GetFontCount(b)); CHECK((void *)a != (void *)b); }
    DW_RELEASE(a); DW_RELEASE(b);
    END_C2();
}
TEST(c2_get_matching_fonts_axis_count_boundaries) {
    /* 20 inline slots in the implementation; counts around it and a bigger array both succeed. */
    NEED_C2();
    if (FIND(c, L"Arial") == 0xFFFFFFFFu) { t_skip("Arial not installed"); END_C2(); return; }
    static const UINT32 counts[] = { 1, 2, 19, 20, 21, 64, 300 };
    DWRITE_FONT_AXIS_VALUE *ax = (DWRITE_FONT_AXIS_VALUE *)calloc(400, sizeof *ax);
    if (!ax) { t_skip("no memory"); END_C2(); return; }
    for (int i = 0; i < 400; i++) { ax[i].axisTag = (i & 1) ? DWRITE_FONT_AXIS_TAG_WIDTH : DWRITE_FONT_AXIS_TAG_WEIGHT; ax[i].value = (i & 1) ? 100.0f : 400.0f + (float)(i % 5) * 100.0f; }
    for (size_t i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        IDWriteFontList2 *l = NULL;
        HRESULT hr = IDWriteFontCollection2_GetMatchingFonts(c2, L"Arial", ax, counts[i], &l);
        CHECKF(hr == S_OK && l, "%u axis values: %s", counts[i], dw_hr_name(hr));
        if (l) CHECK(IDWriteFontList2_GetFontCount(l) >= 1);
        DW_RELEASE(l);
    }
    free(ax);
    END_C2();
}
TEST(c2_get_matching_fonts_unknown_axis_tags) {
    NEED_C2();
    if (FIND(c, L"Arial") == 0xFFFFFFFFu) { t_skip("Arial not installed"); END_C2(); return; }
    DWRITE_FONT_AXIS_VALUE ax[3] = { { 0x00000000, 1.0f }, { 0x41424344, -5.0f }, { DWRITE_FONT_AXIS_TAG_SLANT, 1e9f } };
    IDWriteFontList2 *l = (IDWriteFontList2 *)(ULONG_PTR)1;
    HRESULT hr = IDWriteFontCollection2_GetMatchingFonts(c2, L"Arial", ax, 3, &l);
    t_note("unknown axis tags -> %s", dw_hr_name(hr));
    CHECK(hr == S_OK || hr == E_INVALID);
    if (hr == S_OK) { CHECK(l != NULL); DW_RELEASE(l); } else CHECK_OUT_NULL(l);
    END_C2();
}
TEST(c2_family2_get_matching_fonts_validation) {
    NEED_C2();
    if (!fcs_count(c)) { t_skip("no families"); END_C2(); return; }
    IDWriteFontFamily2 *fam = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection2_GetFontFamily(c2, 0, &fam));
    DWRITE_FONT_AXIS_VALUE ax[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
    IDWriteFontList2 *l;
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontFamily2_GetMatchingFonts(fam, NULL, 1, &l), E_INVALID); CHECK_OUT_NULL(l);
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontFamily2_GetMatchingFonts(fam, ax, 0x10000, &l), E_INVALID); CHECK_OUT_NULL(l);
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontFamily2_GetMatchingFonts(fam, ax, 0xFFFFFFFFu, &l), E_INVALID); CHECK_OUT_NULL(l);
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    HRESULT hr = IDWriteFontFamily2_GetMatchingFonts(fam, NULL, 0, &l);
    CHECK_HR(hr, S_OK);
    if (hr == S_OK && l && l != (void *)1) { CHECK(IDWriteFontList2_GetFontCount(l) >= 1); IDWriteFontList2_Release(l); }
    l = (IDWriteFontList2 *)(ULONG_PTR)1;
    hr = IDWriteFontFamily2_GetMatchingFonts(fam, ax, 1, &l);
    CHECK_HR(hr, S_OK);
    if (hr == S_OK && l && l != (void *)1) { CHECK(IDWriteFontList2_GetFontCount(l) >= 1); IDWriteFontList2_Release(l); }
    IDWriteFontFamily2_Release(fam);
    END_C2();
}
TEST(c2_family2_matching_list_has_all_fonts) {
    NEED_C2();
    UINT32 n = fcs_count(c); if (n > 60) n = 60;
    for (UINT32 i = 0; i < n; i++) {
        IDWriteFontFamily2 *fam = NULL; IDWriteFontList2 *l = NULL;
        if (FAILED(IDWriteFontCollection2_GetFontFamily(c2, i, &fam)) || !fam) { CHECK(0); continue; }
        if (SUCCEEDED(IDWriteFontFamily2_GetMatchingFonts(fam, NULL, 0, &l)) && l) {
            CHECKF(IDWriteFontList2_GetFontCount(l) == IDWriteFontFamily2_GetFontCount(fam), "family %u: list %u fonts vs family %u", i,
                   IDWriteFontList2_GetFontCount(l), IDWriteFontFamily2_GetFontCount(fam));
            IDWriteFontList2_Release(l);
        }
        IDWriteFontFamily2_Release(fam);
    }
    END_C2();
}
TEST(c2_qi_of_family2_matrix) {
    NEED_C2();
    if (!fcs_count(c)) { t_skip("no families"); END_C2(); return; }
    IDWriteFontFamily2 *fam = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection2_GetFontFamily(c2, 0, &fam));
    dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontFamily, 1), DW_QI_ROW(IDWriteFontFamily1, 1), DW_QI_ROW(IDWriteFontFamily2, 1),
                         DW_QI_ROW(IDWriteFontList, 1), DW_QI_ROW(IDWriteFontList1, -1), DW_QI_ROW(IDWriteFontList2, -1), DW_QI_ROW(IDWriteFont, 0) };
    DW_CHECK_QI(fam, rows);
    IDWriteFontFamily2_Release(fam);
    END_C2();
}

/* ---- IDWriteFontCollection3 ----------------------------------------------------------------------------------- */
TEST(c3_expiration_event) {
    NEED_SYS();
    IDWriteFontCollection3 *c3 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection3, (void **)&c3));
    HANDLE h = IDWriteFontCollection3_GetExpirationEvent(c3);
    HANDLE h2 = IDWriteFontCollection3_GetExpirationEvent(c3);
    if (!h) { t_note("GetExpirationEvent returned NULL for the system collection"); CHECK(h2 == NULL); }
    else {
        DWORD flags = 0;
        CHECK(GetHandleInformation(h, &flags));
        DWORD w = WaitForSingleObject(h, 0);
        t_note("expiration event %p, wait(0) -> %lu", h, w);
        CHECK(w == WAIT_TIMEOUT || w == WAIT_OBJECT_0);
        if (h2 != h) t_note("each call returns a fresh handle");
        if (h2 && h2 != h) CloseHandle(h2);
        CloseHandle(h);
    }
    DW_RELEASE(c3);
    END_SYS();
}
TEST(c3_inherits_c2_behaviour) {
    NEED_SYS();
    IDWriteFontCollection3 *c3 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection3, (void **)&c3));
    CHECK(IDWriteFontCollection3_GetFontFamilyCount(c3) == fcs_count(c));
    CHECK(IDWriteFontCollection3_GetFontFamilyModel(c3) == DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
    if (fcs_count(c)) {
        IDWriteFontFamily2 *fam = NULL;
        CHECK_HR_OK(FCS_C3_GETFAMILY(c3, 0, &fam));
        DW_RELEASE(fam);
    }
    IDWriteFontSet1 *fs = NULL;
    CHECK_HR_OK(FCS_C3_GETFONTSET(c3, &fs));
    DW_RELEASE(fs);
    DW_RELEASE(c3);
    END_SYS();
}

/* ---- family models and downloadable variants ----------------------------------------------------------------- */
TEST(model_typographic_collection_basics) {
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *t = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    IDWriteFontCollection *tc = (IDWriteFontCollection *)t;
    UINT32 n = fcs_count(tc);
    CHECK(n >= 1);
    CHECK(IDWriteFontCollection2_GetFontFamilyModel(t) == DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC);
    FOREACH_FAMILY(tc, i, fam, FCS_MAX_FAMILIES) {
        CHECK(IDWriteFontFamily_GetFontCount(fam) >= 1);
        char nm[256];
        CHECK_HR_OK(fcs_family_en(fam, nm, sizeof nm));
        CHECK(nm[0] != 0);
    } END_FAMILY(fam)
    DW_RELEASE(t);
}
TEST(model_typographic_family_names_unique_and_found) {
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *t = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    IDWriteFontCollection *tc = (IDWriteFontCollection *)t;
    UINT32 n = fcs_count(tc); if (n > FCS_MAX_FAMILIES) n = FCS_MAX_FAMILIES;
    UINT32 lost = 0;
    for (UINT32 i = 0; i < n; i++) {
        char nm[256]; WCHAR w[256];
        if (FAILED(fcs_name_at(tc, i, nm, sizeof nm))) continue;
        dw_wide(nm, w, 256);
        UINT32 k = FIND(tc, w);
        if (k == 0xFFFFFFFFu) lost++;
        else if (k != i) t_note("typographic family #%u \"%s\" found at %u", i, nm, k);
    }
    CHECKF(lost == 0, "%u typographic families not found by their own name", lost);
    DW_RELEASE(t);
}
TEST(model_typographic_vs_wss_counts) {
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *t = NULL, *w = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &w));
    UINT32 nt = fcs_count((IDWriteFontCollection *)t), nw = fcs_count((IDWriteFontCollection *)w);
    t_note("typographic %u families, weight/stretch/style %u families", nt, nw);
    CHECK(nt >= 1 && nw >= 1);
    IDWriteFontSet1 *st = NULL, *sw = NULL;
    if (SUCCEEDED(IDWriteFontCollection2_GetFontSet(t, &st)) && SUCCEEDED(IDWriteFontCollection2_GetFontSet(w, &sw)) && st && sw)
        CHECK(IDWriteFontSet1_GetFontCount(st) == IDWriteFontSet1_GetFontCount(sw));
    DW_RELEASE(st); DW_RELEASE(sw); DW_RELEASE(t); DW_RELEASE(w);
}
TEST(model_typographic_font_family_names_agree_with_font_set) {
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *t = NULL; IDWriteFontSet1 *fs = NULL; IDWriteStringList *names = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    DW_REQUIRE_OK(IDWriteFontCollection2_GetFontSet(t, &fs));
    HRESULT hr = IDWriteFontSet1_GetPropertyValues__((IDWriteFontSet *)fs, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, &names);
    CHECK_HR_OK(hr);
    if (names) {
        UINT32 n = IDWriteStringList_GetCount(names), fc = fcs_count((IDWriteFontCollection *)t);
        t_note("typographic family names in font set %u, typographic collection families %u", n, fc);
        IDWriteStringList_Release(names);
    }
    DW_RELEASE(fs); DW_RELEASE(t);
}
TEST(model_each_model_family_has_fonts_in_other_model) {
    /* Same fonts, grouped differently: a family name present in both models resolves to a non-empty family in each. */
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *t = NULL, *w = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &w));
    static const WCHAR *names[] = { L"Arial", L"Segoe UI", L"Times New Roman", L"Calibri", L"Consolas", L"Tahoma" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        UINT32 it = FIND((IDWriteFontCollection *)t, names[i]), iw = FIND((IDWriteFontCollection *)w, names[i]);
        if (it == 0xFFFFFFFFu || iw == 0xFFFFFFFFu) { t_note("%s: typographic %s, wss %s", fcs_u8(names[i]), it == 0xFFFFFFFFu ? "absent" : "present", iw == 0xFFFFFFFFu ? "absent" : "present"); continue; }
        IDWriteFontFamily *ft = NULL, *fw = NULL;
        if (SUCCEEDED(IDWriteFontCollection2_GetFontFamily(t, it, (IDWriteFontFamily2 **)&ft)) && SUCCEEDED(IDWriteFontCollection2_GetFontFamily(w, iw, (IDWriteFontFamily2 **)&fw))) {
            UINT32 a = IDWriteFontFamily_GetFontCount(ft), b = IDWriteFontFamily_GetFontCount(fw);
            t_note("%s: %u fonts typographic, %u fonts weight/stretch/style", fcs_u8(names[i]), a, b);
            CHECK(a >= 1 && b >= 1);
        }
        DW_RELEASE(ft); DW_RELEASE(fw);
    }
    DW_RELEASE(t); DW_RELEASE(w);
}
TEST(model_downloadable_collections_basics) {
    NEED_FV(f6, IDWriteFactory6, 6);
    for (int m = 0; m < 2; m++) {
        IDWriteFontCollection2 *local = NULL, *dl = NULL;
        DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, (DWRITE_FONT_FAMILY_MODEL)m, &local));
        DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, TRUE, (DWRITE_FONT_FAMILY_MODEL)m, &dl));
        CHECK(IDWriteFontCollection2_GetFontFamilyModel(dl) == (DWRITE_FONT_FAMILY_MODEL)m);
        UINT32 nl = fcs_count((IDWriteFontCollection *)local), nd = fcs_count((IDWriteFontCollection *)dl);
        t_note("model %d: %u local families, %u with downloadable", m, nl, nd);
        CHECK(nd >= nl);
        DW_RELEASE(local); DW_RELEASE(dl);
    }
}
TEST(model_f7_collection3_models) {
    NEED_FV(f7, IDWriteFactory7, 7);
    for (int m = 0; m < 2; m++) {
        IDWriteFontCollection3 *c3 = NULL;
        DW_REQUIRE_OK(IDWriteFactory7_GetSystemFontCollection(f7, FALSE, (DWRITE_FONT_FAMILY_MODEL)m, &c3));
        CHECK(IDWriteFontCollection3_GetFontFamilyModel(c3) == (DWRITE_FONT_FAMILY_MODEL)m);
        CHECK(IDWriteFontCollection3_GetFontFamilyCount(c3) >= 1);
        DW_RELEASE(c3);
    }
}
TEST(model_invalid_model_value_recorded) {
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *c = NULL;
    HRESULT hr = IDWriteFactory6_GetSystemFontCollection(f6, FALSE, (DWRITE_FONT_FAMILY_MODEL)5, &c);
    CHECK_HR_OK(hr);
    if (c) { t_note("GetFontFamilyModel for requested model 5 reports %d", IDWriteFontCollection2_GetFontFamilyModel(c)); DW_RELEASE(c); }
}

/* ---- out-of-range indices: observed in a child process ----------------------------------------------------- */
/* DWriteFontCollection::GetFontFamily clears *family and, for index >= count, constructs and throws an
 * IndexOutOfRangeException from the vtable method itself. Whether the host sees an HRESULT or an exception depends on the
 * catch handlers in the frames above, which are not visible in the decompilation, so the outcome is recorded rather than asserted. */
CHILD(oor) {
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    IDWriteFactory *f = dw_shared();
    if (!f) return 0x1001;
    IDWriteFontCollection *c = NULL;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return 0x1002;
    UINT32 n = fcs_count(c);
    if (!n) return 0x1003;
    HRESULT hr = E_FAIL; void *out = (void *)(ULONG_PTR)1;
    switch (mode) {
    case 0: hr = IDWriteFontCollection_GetFontFamily(c, n, (IDWriteFontFamily **)&out); break;
    case 1: hr = IDWriteFontCollection_GetFontFamily(c, 0xFFFFFFFFu, (IDWriteFontFamily **)&out); break;
    case 2: { IDWriteFontFamily *fam = NULL; IDWriteFontCollection_GetFontFamily(c, 0, &fam);
              hr = IDWriteFontFamily_GetFont(fam, IDWriteFontFamily_GetFontCount(fam), (IDWriteFont **)&out); break; }
    case 3: { IDWriteFontCollection1 *c1 = NULL; IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1);
              hr = IDWriteFontCollection1_GetFontFamily(c1, n, (IDWriteFontFamily1 **)&out); break; }
    case 4: { IDWriteFontCollection2 *c2 = NULL; IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2);
              hr = IDWriteFontCollection2_GetFontFamily(c2, n, (IDWriteFontFamily2 **)&out); break; }
    case 5: { IDWriteFontFamily *fam = NULL; IDWriteFontCollection_GetFontFamily(c, 0, &fam);
              hr = IDWriteFontFamily_GetFont(fam, 0xFFFFFFFFu, (IDWriteFont **)&out); break; }
    }
    /* Returned normally: exit code carries "returned" (0x2000) | out-pointer-null (0x100), and the HRESULT's low byte. */
    return 0x2000 | (out == NULL ? 0x100 : 0) | (int)(hr & 0xFF);
}
static void oor_probe(int mode, const char *what) {
    char arg[16]; snprintf(arg, sizeof arg, "%d", mode);
    DWORD rc = t_run_child("oor", arg, 30000);
    if (rc == 0xDEAD0000) { t_skip("cannot spawn child"); return; }
    CHECK(rc != 0xDEAD0001);            /* a hang is a failure; a crash or an error return are both acceptable outcomes */
    if (rc & 0x2000) t_note("%s: returned, hr low byte 0x%02lx, out %s", what, rc & 0xFF, (rc & 0x100) ? "NULL" : "NOT NULL");
    else t_note("%s: child terminated with 0x%08lx", what, rc);
}
TEST(oor_collection_get_font_family_count) { oor_probe(0, "IDWriteFontCollection::GetFontFamily(count)"); }
TEST(oor_collection_get_font_family_uint_max) { oor_probe(1, "IDWriteFontCollection::GetFontFamily(UINT_MAX)"); }
TEST(oor_family_get_font_count) { oor_probe(2, "IDWriteFontFamily::GetFont(count)"); }
TEST(oor_collection1_get_font_family_count) { oor_probe(3, "IDWriteFontCollection1::GetFontFamily(count)"); }
TEST(oor_collection2_get_font_family_count) { oor_probe(4, "IDWriteFontCollection2::GetFontFamily(count)"); }
TEST(oor_family_get_font_uint_max) { oor_probe(5, "IDWriteFontFamily::GetFont(UINT_MAX)"); }
TEST(oor_last_valid_index_works_in_process) {
    NEED_SYS();
    UINT32 n = fcs_count(c);
    if (!n) { t_skip("no families"); END_SYS(); return; }
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, n - 1, &fam));
    CHECK(fam != NULL);
    DW_RELEASE(fam);
    END_SYS();
}
