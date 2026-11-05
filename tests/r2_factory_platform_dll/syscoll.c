/* System font collection / system font set at factory level: caching and identity.
 * Decompilation: DWriteFactory::GetSystemFontCollection keeps one cached collection object per (variant) slot under the factory
 * critical section and hands out an AddRef'd pointer to it; GetSystemFontSet caches the DWriteFontSet the same way. The
 * IDWriteFactory pointers of every generation share one object, hence one cache. */
#include "rfp.h"

static UINT32 famcount(IDWriteFactory *f, BOOL upd) {
    IDWriteFontCollection *c = NULL; UINT32 n = 0;
    if (SUCCEEDED(IDWriteFactory_GetSystemFontCollection(f, &c, upd)) && c) { n = IDWriteFontCollection_GetFontFamilyCount(c); IDWriteFontCollection_Release(c); }
    return n;
}

TEST(sc_get_ok_nonnull_and_nonempty) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = (IDWriteFontCollection *)RFP_SENTINEL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE));
    CHECK(c != NULL && c != RFP_SENTINEL);
    if (c && c != RFP_SENTINEL) { CHECK(IDWriteFontCollection_GetFontFamilyCount(c) > 10); DW_RELEASE(c); }
}
TEST(sc_same_object_on_repeated_calls) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *a = NULL, *b = NULL, *c = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &b, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE));
    CHECK(a == b && b == c);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
}
TEST(sc_each_call_adds_exactly_one_reference) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *a = NULL, *b = NULL, *c = NULL; ULONG r0; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE)); r0 = dw_refcount((IUnknown *)a);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &b, FALSE)); EXPECT_REFS(a, r0 + 1);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)); EXPECT_REFS(a, r0 + 2);
    DW_RELEASE(c); EXPECT_REFS(a, r0 + 1); DW_RELEASE(b); EXPECT_REFS(a, r0); DW_RELEASE(a);
}
TEST(sc_family_count_stable_over_100_calls) {
    IDWriteFactory *f = dw_shared(); UINT32 n0; int i; REQUIRE(f);
    n0 = famcount(f, FALSE); CHECK(n0 > 0);
    for (i = 0; i < 100; i++) if (famcount(f, FALSE) != n0) { CHECK(0); break; }
}
TEST(sc_same_object_through_every_factory_generation_pointer) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *base = NULL; int v; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &base, FALSE)); REQUIRE(base);
    for (v = 0; v <= 7; v++) {
        IDWriteFactory *fv = (IDWriteFactory *)dw_factory_v(FT_SHARED, v); IDWriteFontCollection *c = NULL;
        if (!fv) continue;
        CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(fv, &c, FALSE));
        CHECKF(c == base, "IDWriteFactory%d returned another collection object", v);
        DW_RELEASE(c);
    }
    DW_RELEASE(base);
}
TEST(sc_shared_and_isolated_factories_have_separate_collection_objects) {
    IDWriteFactory *s = dw_shared(), *i = dw_new_factory(FT_ISOLATED); IDWriteFontCollection *cs = NULL, *ci = NULL; REQUIRE(s && i);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(s, &cs, FALSE)); CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(i, &ci, FALSE));
    CHECK(cs != ci);
    if (cs && ci) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(cs), IDWriteFontCollection_GetFontFamilyCount(ci));
    DW_RELEASE(cs); DW_RELEASE(ci); DW_RELEASE(i);
}
TEST(sc_two_isolated_factories_have_distinct_collections) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *b = dw_new_factory(FT_ISOLATED); IDWriteFontCollection *ca = NULL, *cb = NULL; REQUIRE(a && b);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(a, &ca, FALSE)); CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(b, &cb, FALSE));
    CHECK(ca != cb);
    DW_RELEASE(ca); DW_RELEASE(cb); DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sc_check_for_updates_true_ok_and_same_family_count) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = (IDWriteFontCollection *)RFP_SENTINEL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, TRUE));
    CHECK(c != NULL && c != RFP_SENTINEL);
    DW_RELEASE(c);
    EXPECT_EQ(famcount(f, TRUE), famcount(f, FALSE));   /* nothing changes the installed fonts during the run */
}
TEST(sc_check_for_updates_identity_note) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *a = NULL, *b = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &b, TRUE));
    t_note("collection object after checkForUpdates=TRUE is %s", a == b ? "unchanged" : "a new object");
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sc_old_collection_stays_valid_after_update_check) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *a = NULL, *b = NULL; UINT32 n; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE)); REQUIRE(a);
    n = IDWriteFontCollection_GetFontFamilyCount(a);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &b, TRUE)); DW_RELEASE(b);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(a), n);
    DW_RELEASE(a);
}
TEST(sc_true_flag_nonzero_values_equivalent) {
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    EXPECT_EQ(famcount(f, 2), famcount(f, TRUE));    /* BOOL: any non-zero value means TRUE */
    EXPECT_EQ(famcount(f, -1), famcount(f, TRUE));
}
TEST(sc_collection_qi_chain_note) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)); REQUIRE(c);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontCollection, 1), DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontCollection1, 1),
                         DW_QI_ROW(IDWriteFontCollection2, -1), DW_QI_ROW(IDWriteFontCollection3, -1), DW_QI_ROW(IDWriteFactory, 0),
                         DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFontSet, 0) };
    DW_CHECK_QI(c, rows);
    DW_RELEASE(c);
}
TEST(sc_factory3_collection_matches_factory0_collection_family_count) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontCollection1 *c1 = NULL; UINT32 n1;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &c1, FALSE)); REQUIRE(c1);
    n1 = IDWriteFontCollection1_GetFontFamilyCount(c1);
    EXPECT_EQ(n1, famcount((IDWriteFactory *)f3, FALSE));
    DW_RELEASE(c1);
}
TEST(sc_factory3_collection_repeat_identity) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontCollection1 *a = NULL, *b = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &a, FALSE));
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &b, FALSE));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sc_factory3_include_downloadable_variant_ok) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontCollection1 *a = NULL, *b = NULL; HRESULT hr;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &a, FALSE));
    hr = IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &b, FALSE);
    CHECK_HR_OK(hr);
    if (a && b) { t_note("includeDownloadableFonts: same object=%d, families %u vs %u", a == b, IDWriteFontCollection1_GetFontFamilyCount(a), IDWriteFontCollection1_GetFontFamilyCount(b)); CHECK(IDWriteFontCollection1_GetFontFamilyCount(b) >= IDWriteFontCollection1_GetFontFamilyCount(a)); }
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sc_collection_family_names_findable) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; UINT32 n, i; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)); REQUIRE(c);
    n = IDWriteFontCollection_GetFontFamilyCount(c); if (n > 12) n = 12;
    for (i = 0; i < n; i++) {
        IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *names = NULL; WCHAR nm[256]; UINT32 idx = 0; BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, i, &fam)); if (!fam) continue;
        CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &names));
        if (names && SUCCEEDED(IDWriteLocalizedStrings_GetString(names, 0, nm, 256))) {
            CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, nm, &idx, &ex));
            CHECKF(ex, "family #%u '%ls' not found by its own name", i, nm);
            if (ex) CHECK(idx < IDWriteFontCollection_GetFontFamilyCount(c));
        }
        DW_RELEASE(names); DW_RELEASE(fam);
    }
    DW_RELEASE(c);
}
TEST(sc_find_family_name_missing_gives_exists_false) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; UINT32 idx = 5; BOOL ex = TRUE; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)); REQUIRE(c);
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"No Such Family Zq 9981", &idx, &ex));
    CHECK(ex == FALSE);
    EXPECT_EQ(idx, 0xFFFFFFFFu);   /* documented: index is UINT_MAX when the family does not exist */
    DW_RELEASE(c);
}
TEST(sc_find_family_name_case_insensitive) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; UINT32 a = 0, b = 0; BOOL ea = FALSE, eb = FALSE; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)); REQUIRE(c);
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"Arial", &a, &ea));
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"ARIAL", &b, &eb));
    if (!ea) { t_skip("Arial not installed"); DW_RELEASE(c); return; }
    CHECK(eb); EXPECT_EQ(a, b);
    DW_RELEASE(c);
}
TEST(sc_get_family_out_of_range_fails) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = (IDWriteFontFamily *)RFP_SENTINEL; HRESULT hr; UINT32 n; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)); REQUIRE(c);
    n = IDWriteFontCollection_GetFontFamilyCount(c);
    hr = IDWriteFontCollection_GetFontFamily(c, n, &fam);
    CHECK_HR_FAILED(hr);
    t_note("GetFontFamily(count): %s, out %p", dw_hr_name(hr), (void *)fam);
    if (fam && fam != RFP_SENTINEL) IDWriteFontFamily_Release(fam);
    fam = (IDWriteFontFamily *)RFP_SENTINEL;
    hr = IDWriteFontCollection_GetFontFamily(c, 0xFFFFFFFF, &fam);
    CHECK_HR_FAILED(hr);
    if (fam && fam != RFP_SENTINEL) IDWriteFontFamily_Release(fam);
    DW_RELEASE(c);
}
TEST(sc_collection_outlives_isolated_factory) {
    IDWriteFactory *f = dw_new_factory(FT_ISOLATED); IDWriteFontCollection *c = NULL; UINT32 n; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)); REQUIRE(c);
    n = IDWriteFontCollection_GetFontFamilyCount(c);
    DW_RELEASE(f);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), n);
    {   IDWriteFontFamily *fam = NULL; CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam)); DW_RELEASE(fam); }
    DW_RELEASE(c);
}
TEST(sc_text_format_collection_follows_factory_cache) {
    IDWriteFactory *f = dw_new_factory(FT_ISOLATED); IDWriteTextFormat *tf; IDWriteFontCollection *c1 = NULL, *c2 = NULL; REQUIRE(f);
    tf = rfp_fmt(f, 12.0f, L"en-us"); REQUIRE(tf);
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(tf, &c1)); CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c2, FALSE));
    CHECK(c1 == c2);
    DW_RELEASE(c1); DW_RELEASE(c2); DW_RELEASE(tf); DW_RELEASE(f);
}

/* ---- IDWriteFactory3::GetSystemFontSet --------------------------------------------------------------------------------- */
TEST(fs_system_font_set_ok_nonempty) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontSet *s = (IDWriteFontSet *)RFP_SENTINEL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s));
    CHECK(s != NULL && s != RFP_SENTINEL);
    if (s && s != RFP_SENTINEL) { CHECK(IDWriteFontSet_GetFontCount(s) > 10); DW_RELEASE(s); }
}
TEST(fs_system_font_set_same_object_on_repeat) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontSet *a = NULL, *b = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &a)); CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &b));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(fs_system_font_set_each_call_one_reference) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontSet *a = NULL, *b = NULL; ULONG r0;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &a)); REQUIRE(a);
    r0 = dw_refcount((IUnknown *)a);
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &b)); EXPECT_REFS(a, r0 + 1);
    DW_RELEASE(b); EXPECT_REFS(a, r0); DW_RELEASE(a);
}
TEST(fs_system_font_set_identical_through_each_factory_generation) {
    IDWriteFontSet *base = NULL; int v; IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3);
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &base)); REQUIRE(base);
    for (v = 3; v <= 7; v++) {
        IDWriteFactory3 *fv = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, v); IDWriteFontSet *s = NULL;
        if (!fv) continue;
        CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(fv, &s));
        CHECKF(s == base, "factory%d returned another font set object", v);
        DW_RELEASE(s);
    }
    DW_RELEASE(base);
}
TEST(fs_system_font_set_count_at_least_family_count) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontSet *s = NULL; UINT32 nf;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s)); REQUIRE(s);
    nf = famcount((IDWriteFactory *)f3, FALSE);
    CHECKF(IDWriteFontSet_GetFontCount(s) >= nf, "fonts %u families %u", IDWriteFontSet_GetFontCount(s), nf);
    DW_RELEASE(s);
}
TEST(fs_system_font_set_face_reference_for_first_and_last) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontSet *s = NULL; IDWriteFontFaceReference *r = NULL; UINT32 n;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s)); REQUIRE(s);
    n = IDWriteFontSet_GetFontCount(s); REQUIRE(n > 0);
    CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(s, 0, &r)); DW_RELEASE(r);
    CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(s, n - 1, &r)); DW_RELEASE(r);
    r = (IDWriteFontFaceReference *)RFP_SENTINEL;
    CHECK_HR_FAILED(IDWriteFontSet_GetFontFaceReference(s, n, &r));
    CHECK(r == NULL || r == RFP_SENTINEL);
    DW_RELEASE(s);
}
TEST(fs_system_font_set_isolated_distinct_from_shared) {
    IDWriteFactory3 *s3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFactory *i = dw_new_factory(FT_ISOLATED); IDWriteFactory3 *i3 = NULL; IDWriteFontSet *a = NULL, *b = NULL;
    if (!s3 || !i || FAILED(IDWriteFactory_QueryInterface(i, &IID_IDWriteFactory3, (void **)&i3))) { t_skip("IDWriteFactory3 unsupported"); DW_RELEASE(i); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(s3, &a)); CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(i3, &b));
    CHECK(a != b);
    if (a && b) EXPECT_EQ(IDWriteFontSet_GetFontCount(a), IDWriteFontSet_GetFontCount(b));
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(i3); DW_RELEASE(i);
}
TEST(fs_system_font_set_qi_note) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontSet *s = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s)); REQUIRE(s);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontSet, 1), DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontSet1, -1), DW_QI_ROW(IDWriteFontSet2, -1),
                         DW_QI_ROW(IDWriteFontSet3, -1), DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontSetBuilder, 0), DW_QI_ROW(IDWriteFactory, 0) };
    DW_CHECK_QI(s, rows);
    DW_RELEASE(s);
}
TEST(fs_collection_from_system_font_set_family_count_note) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontSet *s = NULL; IDWriteFontCollection1 *c = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s)); REQUIRE(s);
    CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, s, &c));
    if (c) {
        CHECK(c != NULL); EXPECT_REFS(c, 1);
        t_note("collection from system font set: %u families (system collection %u)", IDWriteFontCollection1_GetFontFamilyCount(c), famcount((IDWriteFactory *)f3, FALSE));
        CHECK(IDWriteFontCollection1_GetFontFamilyCount(c) > 0);
    }
    DW_RELEASE(c); DW_RELEASE(s);
}
TEST(fs_collection_from_font_set_distinct_objects) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); IDWriteFontSet *s = NULL; IDWriteFontCollection1 *a = NULL, *b = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s)); REQUIRE(s);
    CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, s, &a));
    CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, s, &b));
    CHECK(a != b);   /* a fresh DWriteFontCollection per call (decompiled: operator_new each time) */
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(s);
}
TEST(fs_gdi_interop_per_factory_object_stable) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *a = NULL, *b = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a)); CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &b));
    CHECK(a != NULL && b != NULL);
    t_note("GetGdiInterop returns the same object twice: %s", a == b ? "yes" : "no");
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(fs_gdi_interop_qi_matrix) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g)); REQUIRE(g);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteGdiInterop, 1), DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteGdiInterop1, -1), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteBitmapRenderTarget, 0) };
    DW_CHECK_QI(g, rows);
    DW_RELEASE(g);
}
