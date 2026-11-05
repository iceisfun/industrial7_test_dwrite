/* System font collection: acquisition, object identity, caching, QueryInterface, refcounts and lifetime.
 * Windows 10 behaviour (dwrite.dll decompilation): every GetSystemFontCollection overload funnels into
 * DWriteFactory::GetSystemFontCollection(model, includeDownloadable) which caches one collection object per
 * (model != typographic, includeDownloadable) pair inside the factory and always returns S_OK;
 * checkForUpdates is not used by the IDWriteFactory3 entry point. */
#include "fcs.h"

/* ---- plain acquisition through IDWriteFactory --------------------------------------------------------------- */
TEST(sys_get_false_ok) {
    NEED_F();
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE));
    CHECK(c != NULL && c != (void *)1);
    if (c == (void *)1) c = NULL;
    DW_RELEASE(c);
}
TEST(sys_get_true_ok) {
    NEED_F();
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, TRUE));
    CHECK(c != NULL);
    DW_RELEASE(c);
}
TEST(sys_get_nonzero_bool_values) {
    /* checkForUpdates is a BOOL: any non-zero value behaves as TRUE and none of them is rejected. */
    NEED_F();
    static const BOOL vals[] = { 0, 1, 2, -1, 0x100, 0x7fffffff };
    IDWriteFontCollection *base = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &base, FALSE));
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, vals[i]));
        CHECKF(c == base, "BOOL %d returned a different collection", vals[i]);
        DW_RELEASE(c);
    }
    DW_RELEASE(base);
}
TEST(sys_same_object_false_false) {
    NEED_F();
    IDWriteFontCollection *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &b, FALSE));
    CHECK(a == b);
    CHECK(fcs_same(a, b));
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_same_object_false_true) {
    /* The collection is cached in the factory; checkForUpdates does not change which object is returned. */
    NEED_F();
    IDWriteFontCollection *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &b, TRUE));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_same_object_true_true) {
    NEED_F();
    IDWriteFontCollection *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &a, TRUE));
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &b, TRUE));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_same_object_true_false) {
    NEED_F();
    IDWriteFontCollection *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &a, TRUE));
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &b, FALSE));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_pointer_stable_over_many_calls) {
    /* While a client reference is held the factory hands out that same object. */
    NEED_F();
    IDWriteFontCollection *first = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &first, FALSE));
    for (int i = 0; i < 200; i++) {
        IDWriteFontCollection *c = NULL;
        HRESULT hr = IDWriteFactory_GetSystemFontCollection(f, &c, (i & 1) ? TRUE : FALSE);
        if (hr != S_OK || c != first) { CHECKF(0, "iteration %d hr %s ptr %p want %p", i, dw_hr_name(hr), (void *)c, (void *)first); DW_RELEASE(c); break; }
        DW_RELEASE(c);
    }
    CHECK(1);
    DW_RELEASE(first);
}
TEST(sys_collection_usable_after_all_refs_dropped) {
    /* The collection is a ChildComObject: the factory only holds a weak slot, so it may be destroyed when the last client
     * reference goes away and rebuilt on the next request. Either way the next Get must return a working collection. */
    NEED_F();
    IDWriteFontCollection *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(a);
    void *pa = a;
    DW_RELEASE(a);
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &b, FALSE));
    CHECK(b != NULL);
    CHECK(IDWriteFontCollection_GetFontFamilyCount(b) == n);
    t_note("pointer after dropping every client reference: %s", (void *)b == pa ? "same" : "different");
    DW_RELEASE(b);
}
TEST(sys_identity_via_iunknown) {
    NEED_SYS();
    IUnknown *u1 = NULL, *u2 = NULL;
    CHECK_HR_OK(IDWriteFontCollection_QueryInterface(c, &IID_IUnknown, (void **)&u1));
    CHECK_HR_OK(IDWriteFontCollection_QueryInterface(c, &IID_IUnknown, (void **)&u2));
    CHECK(u1 != NULL && u1 == u2);
    CHECK(dw_same_identity(u1, (IUnknown *)c));
    DW_RELEASE(u1); DW_RELEASE(u2);
    END_SYS();
}
TEST(sys_vtable_shared_across_calls) {
    NEED_SYS();
    IDWriteFontCollection *c2 = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &c2, FALSE));
    CHECK(c2->lpVtbl == c->lpVtbl);
    CHECK(c->lpVtbl->GetFontFamilyCount != NULL && c->lpVtbl->FindFamilyName != NULL && c->lpVtbl->GetFontFamily != NULL && c->lpVtbl->GetFontFromFontFace != NULL);
    DW_RELEASE(c2);
    END_SYS();
}

/* ---- refcounts ------------------------------------------------------------------------------------------------ */
TEST(sys_refcount_rises_per_get) {
    NEED_SYS();
    ULONG r1 = dw_refcount((IUnknown *)c), r2;
    IDWriteFontCollection *c2 = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &c2, FALSE));
    r2 = dw_refcount((IUnknown *)c);
    CHECKF(r2 == r1 + 1, "refcount %lu -> %lu after a second Get", r1, r2);
    DW_RELEASE(c2);
    EXPECT_REFS(c, r1);
    CHECK(r1 >= 1);
    END_SYS();
}
TEST(sys_addref_release_return_values) {
    NEED_SYS();
    ULONG base = dw_refcount((IUnknown *)c);
    ULONG a1 = IDWriteFontCollection_AddRef(c);
    ULONG a2 = IDWriteFontCollection_AddRef(c);
    CHECKF(a1 == base + 1, "AddRef returned %lu want %lu", a1, base + 1);
    CHECKF(a2 == base + 2, "AddRef returned %lu want %lu", a2, base + 2);
    ULONG r2 = IDWriteFontCollection_Release(c);
    ULONG r1 = IDWriteFontCollection_Release(c);
    CHECKF(r2 == base + 1, "Release returned %lu want %lu", r2, base + 1);
    CHECKF(r1 == base, "Release returned %lu want %lu", r1, base);
    END_SYS();
}
TEST(sys_refcount_many_gets_balance) {
    NEED_SYS();
    ULONG base = dw_refcount((IUnknown *)c);
    IDWriteFontCollection *v[40];
    for (int i = 0; i < 40; i++) { v[i] = NULL; DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &v[i], i & 1)); }
    EXPECT_REFS(c, base + 40);
    for (int i = 0; i < 40; i++) DW_RELEASE(v[i]);
    EXPECT_REFS(c, base);
    END_SYS();
}
TEST(sys_refcount_not_shared_with_downloadable_variant) {
    /* includeDownloadable selects another cache slot with its own object, so its refcount is independent. */
    NEED_SYS();
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    if (!f3) { t_skip("IDWriteFactory3 not supported"); END_SYS(); return; }
    IDWriteFontCollection1 *d = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &d, FALSE));
    ULONG before = dw_refcount((IUnknown *)c);
    IDWriteFontCollection1 *d2 = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &d2, FALSE));
    EXPECT_REFS(c, before);
    DW_RELEASE(d); DW_RELEASE(d2);
    END_SYS();
}
TEST(sys_qi_adds_one_reference) {
    NEED_SYS();
    ULONG base = dw_refcount((IUnknown *)c);
    IDWriteFontCollection1 *c1 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    EXPECT_REFS(c, base + 1);
    DW_RELEASE(c1);
    EXPECT_REFS(c, base);
    END_SYS();
}

/* ---- QueryInterface ----------------------------------------------------------------------------------------- */
TEST(sys_qi_matrix) {
    NEED_SYS();
    /* DWriteFontCollection answers IUnknown and the whole IDWriteFontCollection .. IDWriteFontCollection3 chain
     * (ComObject::QueryInterface compares IDWriteFontCollection3 first, then a type list of 2, 1, 0). */
    dw_qi_row rows[] = {
        DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontCollection, 1), DW_QI_ROW(IDWriteFontCollection1, 1),
        DW_QI_ROW(IDWriteFontCollection2, 1), DW_QI_ROW(IDWriteFontCollection3, 1),
        DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFactory3, 0), DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFontList, 0),
        DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFontSet1, 0),
        DW_QI_ROW(IDWriteLocalizedStrings, 0), DW_QI_ROW(IDWriteFontCollectionLoader, 0), DW_QI_ROW(IDWriteFontFileLoader, 0),
        DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteTextLayout, 0), DW_QI_ROW(IDWriteGdiInterop, 0),
    };
    DW_CHECK_QI(c, rows);
    END_SYS();
}
TEST(sys_qi_full_iid_table_records) {
    /* Every public IID: record what the collection answers; nothing outside the collection chain may succeed. */
    NEED_SYS();
    int n = 0; const dw_qi_row *t = dw_iid_table(&n);
    int ok = 0;
    for (int i = 0; i < n; i++) {
        void *p = NULL;
        HRESULT hr = IUnknown_QueryInterface((IUnknown *)c, t[i].iid, &p);
        int in_chain = !strcmp(t[i].name, "IDWriteFontCollection") || !strcmp(t[i].name, "IDWriteFontCollection1") ||
                       !strcmp(t[i].name, "IDWriteFontCollection2") || !strcmp(t[i].name, "IDWriteFontCollection3");
        if (SUCCEEDED(hr)) {
            ok++;
            CHECKF(in_chain, "collection unexpectedly answers QI(%s)", t[i].name);
            DW_RELEASE(p);
        } else {
            CHECKF(hr == E_NOINTERFACE && p == NULL, "QI(%s) -> %s ptr %p", t[i].name, dw_hr_name(hr), p);
        }
    }
    CHECK(ok >= 1);
    t_note("system collection answers %d of %d public IIDs", ok, n);
    END_SYS();
}
TEST(sys_qi_random_iid_fails) {
    NEED_SYS();
    static const IID bogus = { 0x1badf00d, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    void *p = (void *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection_QueryInterface(c, &bogus, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    END_SYS();
}
TEST(sys_qi_null_iid_guid_fails) {
    static const IID zero_iid = { 0 };
    NEED_SYS();
    void *p = (void *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection_QueryInterface(c, &zero_iid, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    END_SYS();
}
TEST(sys_qi_all_chain_same_pointer) {
    /* One COM identity, one primary vtable: every interface in the chain is the same address. */
    NEED_SYS();
    void *c1 = NULL, *c2 = NULL, *c3 = NULL;
    HRESULT h1 = IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, &c1);
    HRESULT h2 = IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection2, &c2);
    HRESULT h3 = IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection3, &c3);
    if (h1 == S_OK) CHECK(c1 == (void *)c);
    if (h2 == S_OK) CHECK(c2 == (void *)c);
    if (h3 == S_OK) CHECK(c3 == (void *)c);
    CHECK_HR_OK(h1); CHECK_HR_OK(h2); CHECK_HR_OK(h3);
    DW_RELEASE(c1); DW_RELEASE(c2); DW_RELEASE(c3);
    END_SYS();
}
TEST(sys_qi_down_from_collection3) {
    NEED_SYS();
    IDWriteFontCollection3 *c3 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection3, (void **)&c3));
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontCollection, 1), DW_QI_ROW(IDWriteFontCollection1, 1), DW_QI_ROW(IDWriteFontCollection2, 1),
                         DW_QI_ROW(IDWriteFontCollection3, 1), DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontFamily, 0) };
    DW_CHECK_QI(c3, rows);
    CHECK(fcs_same(c3, c));
    DW_RELEASE(c3);
    END_SYS();
}
TEST(sys_qi_from_collection1_and_2_return_same) {
    NEED_SYS();
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontCollection2 *c2 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    DW_REQUIRE_OK(IDWriteFontCollection1_QueryInterface(c1, &IID_IDWriteFontCollection2, (void **)&c2));
    void *back = NULL;
    CHECK_HR_OK(IDWriteFontCollection2_QueryInterface(c2, &IID_IDWriteFontCollection, &back));
    CHECK(back == (void *)c);
    DW_RELEASE(back); DW_RELEASE(c2); DW_RELEASE(c1);
    END_SYS();
}

/* ---- IDWriteFactory3 / IDWriteFactory5 ---------------------------------------------------------------------- */
TEST(sys_f3_false_ok) {
    NEED_FV(f3, IDWriteFactory3, 3);
    IDWriteFontCollection1 *c = (IDWriteFontCollection1 *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &c, FALSE));
    CHECK(c != NULL && c != (void *)1);
    if (c == (void *)1) c = NULL;
    DW_RELEASE(c);
}
TEST(sys_f3_false_equals_legacy) {
    /* IDWriteFactory::GetSystemFontCollection forwards to the IDWriteFactory3 entry with includeDownloadable = 0. */
    NEED_FV(f3, IDWriteFactory3, 3);
    NEED_SYS();
    IDWriteFontCollection1 *d = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &d, FALSE));
    CHECK((void *)d == (void *)c);
    CHECK(fcs_same(d, c));
    DW_RELEASE(d);
    END_SYS();
}
TEST(sys_f3_check_updates_ignored) {
    NEED_FV(f3, IDWriteFactory3, 3);
    IDWriteFontCollection1 *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &b, TRUE));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_f3_downloadable_ok) {
    NEED_FV(f3, IDWriteFactory3, 3);
    IDWriteFontCollection1 *d = NULL;
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &d, FALSE));
    CHECK(d != NULL);
    DW_RELEASE(d);
}
TEST(sys_f3_downloadable_cached) {
    NEED_FV(f3, IDWriteFactory3, 3);
    IDWriteFontCollection1 *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &b, TRUE));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_f3_downloadable_is_separate_object) {
    /* The downloadable-including collection lives in its own cache slot. */
    NEED_FV(f3, IDWriteFactory3, 3);
    IDWriteFontCollection1 *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &b, FALSE));
    CHECK(a != b);
    CHECK(!fcs_same(a, b));
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_f3_downloadable_same_families_when_nothing_downloaded) {
    /* Without downloaded fonts both collections hold the installed fonts; the downloadable one never has fewer. */
    NEED_FV(f3, IDWriteFactory3, 3);
    IDWriteFontCollection1 *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &b, FALSE));
    UINT32 na = IDWriteFontCollection1_GetFontFamilyCount(a), nb = IDWriteFontCollection1_GetFontFamilyCount(b);
    t_note("family count local %u, with downloadable %u", na, nb);
    CHECK(nb >= na);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_f3_downloadable_nonzero_bool) {
    NEED_FV(f3, IDWriteFactory3, 3);
    IDWriteFontCollection1 *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, 1, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, 0x40, &b, FALSE));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_f5_matches_f3) {
    NEED_FV(f5, IDWriteFactory5, 5);
    NEED_FV(f3, IDWriteFactory3, 3);
    for (int dl = 0; dl < 2; dl++) {
        IDWriteFontCollection1 *a = NULL, *b = NULL;
        DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, dl, &a, FALSE));
        DW_REQUIRE_OK(FCS_F5_GETSYS(f5, dl, &b, FALSE));
        CHECKF(a == b, "includeDownloadable=%d differs between factory interfaces", dl);
        DW_RELEASE(a); DW_RELEASE(b);
    }
}

/* ---- IDWriteFactory6 / IDWriteFactory7 (family model overloads) ---------------------------------------------- */
TEST(sys_f6_model_wss_equals_legacy) {
    /* The legacy entry point asks for the weight/stretch/style model internally, so it shares the model-1 slot. */
    NEED_FV(f6, IDWriteFactory6, 6);
    NEED_SYS();
    IDWriteFontCollection2 *w = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &w));
    CHECK((void *)w == (void *)c);
    DW_RELEASE(w);
    END_SYS();
}
TEST(sys_f6_model_typographic_separate) {
    NEED_FV(f6, IDWriteFactory6, 6);
    NEED_SYS();
    IDWriteFontCollection2 *t = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    CHECK(t != NULL);
    CHECK((void *)t != (void *)c);
    CHECK(!fcs_same(t, c));
    DW_RELEASE(t);
    END_SYS();
}
TEST(sys_f6_typographic_cached) {
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &a));
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &b));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_f6_four_distinct_slots) {
    /* (typographic | wss) x (local | downloadable): four cache slots, four objects. */
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *v[4] = { 0 };
    int n = 0;
    for (int dl = 0; dl < 2; dl++) for (int m = 0; m < 2; m++) {
        DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, dl, (DWRITE_FONT_FAMILY_MODEL)m, &v[n++]));
    }
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) CHECKF(v[i] != v[j], "slots %d and %d share an object", i, j);
    for (int i = 0; i < 4; i++) DW_RELEASE(v[i]);
}
TEST(sys_f6_invalid_model_is_not_rejected) {
    /* The factory does not validate the model: any non-zero value selects the weight/stretch/style slot. */
    NEED_FV(f6, IDWriteFactory6, 6);
    static const int models[] = { 2, 3, 99, -1, 0x7fffffff };
    IDWriteFontCollection2 *wss = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &wss));
    for (size_t i = 0; i < sizeof models / sizeof models[0]; i++) {
        IDWriteFontCollection2 *c = (IDWriteFontCollection2 *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFactory6_GetSystemFontCollection(f6, FALSE, (DWRITE_FONT_FAMILY_MODEL)models[i], &c);
        CHECKF(hr == S_OK, "model %d -> %s", models[i], dw_hr_name(hr));
        if (hr == S_OK) {
            if (c != wss) t_note("model %d returned a different collection than model 1", models[i]);
            CHECK(c != NULL);
            DW_RELEASE(c);
        } else CHECK_OUT_NULL(c);
    }
    DW_RELEASE(wss);
}
TEST(sys_f6_check_family_model_values) {
    NEED_FV(f6, IDWriteFactory6, 6);
    IDWriteFontCollection2 *t = NULL, *w = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &w));
    CHECK(IDWriteFontCollection2_GetFontFamilyModel(t) == DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC);
    CHECK(IDWriteFontCollection2_GetFontFamilyModel(w) == DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
    DW_RELEASE(t); DW_RELEASE(w);
}
TEST(sys_f7_matches_f6) {
    NEED_FV(f7, IDWriteFactory7, 7);
    NEED_FV(f6, IDWriteFactory6, 6);
    for (int dl = 0; dl < 2; dl++) for (int m = 0; m < 2; m++) {
        IDWriteFontCollection2 *a = NULL; IDWriteFontCollection3 *b = NULL;
        DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, dl, (DWRITE_FONT_FAMILY_MODEL)m, &a));
        DW_REQUIRE_OK(IDWriteFactory7_GetSystemFontCollection(f7, dl, (DWRITE_FONT_FAMILY_MODEL)m, &b));
        CHECKF((void *)a == (void *)b, "dl=%d model=%d differs between IDWriteFactory6 and 7", dl, m);
        DW_RELEASE(a); DW_RELEASE(b);
    }
}
TEST(sys_f7_collection3_ok) {
    NEED_FV(f7, IDWriteFactory7, 7);
    IDWriteFontCollection3 *c = (IDWriteFontCollection3 *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteFactory7_GetSystemFontCollection(f7, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &c));
    CHECK(c != NULL && c != (void *)1);
    if (c == (void *)1) c = NULL;
    DW_RELEASE(c);
}

/* ---- factories --------------------------------------------------------------------------------------------- */
TEST(sys_shared_and_isolated_differ) {
    IDWriteFactory *s = dw_shared(), *i = dw_isolated();
    if (!s || !i) { t_skip("factories unavailable"); return; }
    IDWriteFontCollection *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(s, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(i, &b, FALSE));
    CHECK(a != b);
    CHECK(!fcs_same(a, b));
    UINT32 na = IDWriteFontCollection_GetFontFamilyCount(a), nb = IDWriteFontCollection_GetFontFamilyCount(b);
    if (na != nb) t_note("family counts differ shared %u isolated %u", na, nb);
    CHECK(nb > 0);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_isolated_factories_have_own_collections) {
    IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *b = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); t_skip("cannot create isolated factories"); return; }
    IDWriteFontCollection *ca = NULL, *cb = NULL, *ca2 = NULL;
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(a, &ca, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(b, &cb, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(a, &ca2, TRUE));
    CHECK(ca != cb);
    CHECK(ca == ca2);
    DW_RELEASE(ca); DW_RELEASE(cb); DW_RELEASE(ca2); DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sys_shared_factory_twice_same_collection) {
    /* DWriteCreateFactory(SHARED) returns the process-wide factory, hence the same cached collection. */
    NEED_F();
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_SHARED);
    if (!g) { t_skip("second shared factory unavailable"); return; }
    IDWriteFontCollection *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(g, &b, FALSE));
    if (g != f) t_note("second DWriteCreateFactory(SHARED) returned a different object (%p vs %p)", (void *)g, (void *)f);
    if (g == f) CHECK(a == b);
    else if (a != b) t_note("shared factories do not share the collection object");
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(g);
}
TEST(sys_collection_outlives_factory) {
    /* A collection holds a reference on its factory while it has outstanding references (ChildComObject::AddRef
     * forwards the first reference to the parent). */
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!iso) { t_skip("isolated factory unavailable"); return; }
    IDWriteFontCollection *c = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(iso, &c, FALSE));
    UINT32 n0 = IDWriteFontCollection_GetFontFamilyCount(c);
    IDWriteFactory_Release(iso);
    UINT32 n1 = IDWriteFontCollection_GetFontFamilyCount(c);
    CHECK(n0 == n1);
    if (n1) {
        IDWriteFontFamily *fam = NULL;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
        if (fam) {
            IDWriteLocalizedStrings *s = NULL;
            CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &s));
            DW_RELEASE(s);
            IDWriteFontFamily_Release(fam);
        }
    }
    UINT32 idx = 0; BOOL ex = 0;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex));
    DW_RELEASE(c);
}
TEST(sys_family_outlives_collection_and_factory) {
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!iso) { t_skip("isolated factory unavailable"); return; }
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(iso, &c, FALSE));
    if (IDWriteFontCollection_GetFontFamilyCount(c) == 0) { DW_RELEASE(c); IDWriteFactory_Release(iso); t_skip("no families"); return; }
    DW_REQUIRE_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    DW_RELEASE(c);
    IDWriteFactory_Release(iso);
    UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
    CHECK(nf >= 1);
    IDWriteFont *font = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, 0, &font));
    if (font) {
        IDWriteFontFace *face = NULL;
        CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &face));
        if (face) { CHECK(IDWriteFontFace_GetGlyphCount(face) > 0); IDWriteFontFace_Release(face); }
        IDWriteFont_Release(font);
    }
    IDWriteFontCollection *back = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(fam, &back));
    if (back) CHECK(IDWriteFontCollection_GetFontFamilyCount(back) > 0);
    DW_RELEASE(back);
    IDWriteFontFamily_Release(fam);
}
TEST(sys_new_isolated_after_release_works) {
    for (int i = 0; i < 6; i++) {
        IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
        if (!iso) { t_skip("isolated factory unavailable"); return; }
        IDWriteFontCollection *c = NULL;
        HRESULT hr = IDWriteFactory_GetSystemFontCollection(iso, &c, FALSE);
        CHECKF(hr == S_OK && c, "round %d hr %s", i, dw_hr_name(hr));
        if (c) CHECK(IDWriteFontCollection_GetFontFamilyCount(c) > 0 || t_is_wine());
        DW_RELEASE(c);
        IDWriteFactory_Release(iso);
    }
}

/* ---- consumers of the cached collection ------------------------------------------------------------------- */
TEST(sys_text_format_default_collection_is_system) {
    /* CreateTextFormat(NULL collection) fetches the cached system collection (weight/stretch/style, no downloadable). */
    NEED_SYS();
    IDWriteTextFormat *tf = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf));
    IDWriteFontCollection *tc = NULL;
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(tf, &tc));
    CHECK(tc == c);
    DW_RELEASE(tc); IDWriteTextFormat_Release(tf);
    END_SYS();
}
TEST(sys_text_format_explicit_collection_kept) {
    NEED_SYS();
    IDWriteTextFormat *tf = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateTextFormat(f, L"Segoe UI", c, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 20.0f, L"en-us", &tf));
    IDWriteFontCollection *tc = NULL;
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(tf, &tc));
    CHECK(tc == c);
    DW_RELEASE(tc); IDWriteTextFormat_Release(tf);
    END_SYS();
}
TEST(sys_text_format_holds_collection_reference) {
    NEED_SYS();
    ULONG base = dw_refcount((IUnknown *)c);
    IDWriteTextFormat *tf = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", c, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf));
    ULONG held = dw_refcount((IUnknown *)c);
    CHECKF(held >= base, "refcount fell %lu -> %lu", base, held);
    if (held == base) t_note("text format does not hold a counted reference on the collection");
    IDWriteTextFormat_Release(tf);
    EXPECT_REFS(c, base);
    END_SYS();
}
TEST(sys_text_format_null_collection_refcount_balanced) {
    NEED_SYS();
    ULONG base = dw_refcount((IUnknown *)c);
    for (int i = 0; i < 20; i++) {
        IDWriteTextFormat *tf = NULL;
        if (FAILED(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf))) { CHECK(0); break; }
        IDWriteTextFormat_Release(tf);
    }
    EXPECT_REFS(c, base);
    END_SYS();
}
TEST(sys_eudc_collection_ok) {
    NEED_FV(f2, IDWriteFactory2, 2);
    for (int chk = 0; chk < 2; chk++) {
        IDWriteFontCollection *e = (IDWriteFontCollection *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFactory2_GetEudcFontCollection(f2, &e, chk);
        CHECKF(hr == S_OK, "GetEudcFontCollection(%d) -> %s", chk, dw_hr_name(hr));
        if (hr == S_OK && e != (void *)1) {
            t_note("EUDC collection(%d): %p families %u", chk, (void *)e, e ? IDWriteFontCollection_GetFontFamilyCount(e) : 0);
            DW_RELEASE(e);
        }
    }
}
TEST(sys_eudc_differs_from_system) {
    NEED_FV(f2, IDWriteFactory2, 2);
    NEED_SYS();
    IDWriteFontCollection *e = NULL;
    if (FAILED(IDWriteFactory2_GetEudcFontCollection(f2, &e, FALSE)) || !e) { t_skip("no EUDC collection object"); END_SYS(); return; }
    CHECK(e != c);
    DW_RELEASE(e);
    END_SYS();
}
TEST(sys_font_set_from_factory3_matches_collection_size) {
    NEED_FV(f3, IDWriteFactory3, 3);
    NEED_SYS();
    IDWriteFontSet *fs = NULL; IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *cs = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontSet(f3, &fs));
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    DW_REQUIRE_OK(IDWriteFontCollection1_GetFontSet(c1, &cs));
    UINT32 a = IDWriteFontSet_GetFontCount(fs), b = IDWriteFontSet_GetFontCount(cs);
    t_note("system font set %u fonts, collection font set %u fonts", a, b);
    CHECK(a >= 1 && b >= 1);
    CHECK(a == b);
    DW_RELEASE(fs); DW_RELEASE(cs); DW_RELEASE(c1);
    END_SYS();
}

/* ---- concurrency -------------------------------------------------------------------------------------------- */
typedef struct { IDWriteFactory *f; void *seen[64]; int n, bad; } thr_ctx;
static DWORD WINAPI getter(LPVOID p) {
    thr_ctx *x = (thr_ctx *)p;
    for (int i = 0; i < 64; i++) {
        IDWriteFontCollection *c = NULL;
        HRESULT hr = IDWriteFactory_GetSystemFontCollection(x->f, &c, i & 1);
        if (hr != S_OK || !c) { x->bad++; continue; }
        x->seen[x->n++] = c;
        IDWriteFontCollection_Release(c);
    }
    return 0;
}
TEST(sys_concurrent_gets_same_object) {
    if (t_is_wine()) { t_skip("emulator's collection cache is not thread-safe"); return; }
    NEED_SYS();
    enum { N = 8 };
    HANDLE h[N]; thr_ctx ctx[N]; int started = 0;
    memset(ctx, 0, sizeof ctx);
    for (int i = 0; i < N; i++) { ctx[i].f = f; h[i] = CreateThread(NULL, 0, getter, &ctx[i], 0, NULL); if (h[i]) started++; }
    for (int i = 0; i < N; i++) if (h[i]) { CHECK(WaitForSingleObject(h[i], 20000) == WAIT_OBJECT_0); CloseHandle(h[i]); }
    CHECK(started == N);
    int mismatches = 0, bad = 0;
    for (int i = 0; i < N; i++) { bad += ctx[i].bad; for (int j = 0; j < ctx[i].n; j++) if (ctx[i].seen[j] != (void *)c) mismatches++; }
    CHECKF(bad == 0, "%d failed Get calls", bad);
    CHECKF(mismatches == 0, "%d Get calls returned another object", mismatches);
    END_SYS();
}
TEST(sys_concurrent_isolated_first_get) {
    if (t_is_wine()) { t_skip("emulator's collection cache is not thread-safe"); return; }
    /* Several threads race to build the collection of a fresh factory: all must end up with one object. */
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!iso) { t_skip("isolated factory unavailable"); return; }
    enum { N = 6 };
    HANDLE h[N]; thr_ctx ctx[N];
    memset(ctx, 0, sizeof ctx);
    for (int i = 0; i < N; i++) { ctx[i].f = iso; h[i] = CreateThread(NULL, 0, getter, &ctx[i], 0, NULL); }
    for (int i = 0; i < N; i++) if (h[i]) { WaitForSingleObject(h[i], 30000); CloseHandle(h[i]); }
    void *first = NULL; int diff = 0, bad = 0;
    for (int i = 0; i < N; i++) { bad += ctx[i].bad; for (int j = 0; j < ctx[i].n; j++) { if (!first) first = ctx[i].seen[j]; else if (ctx[i].seen[j] != first) diff++; } }
    CHECK(bad == 0); CHECK(diff == 0);
    IDWriteFactory_Release(iso);
}
