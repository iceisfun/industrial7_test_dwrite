/* DWriteCreateFactory: factory type validation, riid handling, shared/isolated identity rules and the factory interface chain.
 * Decompilation: *ppFactory is zeroed first; type 0 = process-wide factory, type 1 = new DWriteFactory, other values raise an
 * argument exception mapped to E_INVALIDARG; QueryInterface failures give E_NOINTERFACE with the out pointer left NULL. */
#include "rfp.h"

static const GUID g_null_iid = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
static const GUID g_unknown_iid = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } };

/* ---- invalid factory types: E_INVALIDARG, out NULL, for each IID ------------------------------------- */
static const int bad_types[] = { -1, 2, 3, 4, 7, 8, 15, 16, 255, 256, 65535, 65536, 0x7fffffff, (int)0x80000000, (int)0xffffffff, (int)0xfffffffe, 0x01000000, 0x00010000 };
#define NBAD ((int)(sizeof bad_types / sizeof bad_types[0]))

TEST(ty_invalid_types_with_factory_iid) {
    int i; NEED_EXPORT();
    for (i = 0; i < NBAD; i++) {
        void *p = RFP_SENTINEL; HRESULT hr = rfp_create(bad_types[i], &IID_IDWriteFactory, &p);
        CHECKF(hr == E_INVALIDARG, "type %d -> %s", bad_types[i], dw_hr_name(hr));
        CHECKF(p == NULL, "type %d left %p", bad_types[i], p);
        if (p && p != RFP_SENTINEL) DW_RELEASE(p);
    }
}
TEST(ty_invalid_types_with_iunknown) {
    int i; NEED_EXPORT();
    for (i = 0; i < NBAD; i++) {
        void *p = RFP_SENTINEL; HRESULT hr = rfp_create(bad_types[i], &IID_IUnknown, &p);
        CHECKF(hr == E_INVALIDARG, "type %d -> %s", bad_types[i], dw_hr_name(hr));
        CHECKF(p == NULL, "type %d left %p", bad_types[i], p);
        if (p && p != RFP_SENTINEL) DW_RELEASE(p);
    }
}
TEST(ty_invalid_types_with_unknown_iid_still_invalidarg) {
    /* the type is validated before the interface is looked up */
    int i; NEED_EXPORT();
    for (i = 0; i < NBAD; i++) {
        void *p = RFP_SENTINEL; HRESULT hr = rfp_create(bad_types[i], &g_unknown_iid, &p);
        CHECKF(hr == E_INVALIDARG, "type %d -> %s", bad_types[i], dw_hr_name(hr));
        CHECK(p == NULL);
    }
}
TEST(ty_invalid_types_with_null_guid) {
    int i; NEED_EXPORT();
    for (i = 0; i < NBAD; i++) {
        void *p = RFP_SENTINEL; HRESULT hr = rfp_create(bad_types[i], &g_null_iid, &p);
        CHECKF(hr == E_INVALIDARG, "type %d -> %s", bad_types[i], dw_hr_name(hr));
        CHECK(p == NULL);
    }
}
TEST(ty_invalid_types_with_every_factory_version_iid) {
    int v, i; NEED_EXPORT();
    for (v = 0; v <= 7; v++) for (i = 0; i < NBAD; i += 3) {
        void *p = RFP_SENTINEL; HRESULT hr = rfp_create(bad_types[i], dw_factory_iid(v), &p);
        CHECKF(hr == E_INVALIDARG && p == NULL, "v%d type %d -> %s p=%p", v, bad_types[i], dw_hr_name(hr), p);
    }
}
TEST(ty_invalid_does_not_disturb_shared_refcount) {
    IDWriteFactory *f = dw_shared(); ULONG r0; int i; REQUIRE(f);
    r0 = dw_refcount((IUnknown *)f);
    for (i = 0; i < NBAD; i++) { void *p = NULL; rfp_create(bad_types[i], &IID_IDWriteFactory, &p); }
    EXPECT_REFS(f, r0);
}
TEST(ty_failure_overwrites_previous_object_pointer_without_release) {
    /* *ppFactory is set to NULL before validation: a live pointer passed in is overwritten and not released */
    IDWriteFactory *own = dw_new_factory(FT_ISOLATED); void *p;
    REQUIRE(own);
    p = own;
    CHECK_HR(rfp_create(5, &IID_IDWriteFactory, &p), E_INVALIDARG);
    CHECK(p == NULL);
    EXPECT_REFS(own, 1);
    DW_RELEASE(own);
}
TEST(ty_valid_types_return_s_ok_exactly) {
    void *p = NULL; NEED_EXPORT();
    EXPECT_EQ(rfp_create(0, &IID_IDWriteFactory, &p), S_OK); DW_RELEASE(p);
    EXPECT_EQ(rfp_create(1, &IID_IDWriteFactory, &p), S_OK); DW_RELEASE(p);
}

/* ---- unknown riid on valid types ---------------------------------------------------------------------- */
static const GUID *const foreign_iids[] = { &IID_IClassFactory, &IID_IMalloc, &IID_IStream, &IID_IDispatch, &IID_IPersist, &IID_IMarshal, &IID_IStorage,
                                            &IID_IDWriteFontFile, &IID_IDWriteFontFace, &IID_IDWriteTextFormat, &IID_IDWriteRenderingParams,
                                            &IID_IDWriteGdiInterop, &IID_IDWriteFontCollection, &IID_IDWriteTextLayout, &IID_IDWriteTextAnalyzer,
                                            &IID_IDWriteFont, &IID_IDWriteFontFamily, &IID_IDWriteLocalizedStrings, &g_unknown_iid, &g_null_iid };
#define NFOREIGN ((int)(sizeof foreign_iids / sizeof foreign_iids[0]))

static void foreign_for(int type) {
    int i; NEED_EXPORT();
    for (i = 0; i < NFOREIGN; i++) {
        void *p = RFP_SENTINEL; HRESULT hr = rfp_create(type, foreign_iids[i], &p);
        CHECKF(hr == E_NOINTERFACE, "iid #%d -> %s", i, dw_hr_name(hr));
        CHECKF(p == NULL, "iid #%d out %p", i, p);
        if (p && p != RFP_SENTINEL) DW_RELEASE(p);
    }
}
TEST(ty_foreign_iids_shared_no_interface) { foreign_for(FT_SHARED); }
TEST(ty_foreign_iids_isolated_no_interface) { foreign_for(FT_ISOLATED); }
TEST(ty_foreign_iid_shared_failure_does_not_leak_reference) {
    IDWriteFactory *f = dw_shared(); ULONG r0; int i; REQUIRE(f);
    r0 = dw_refcount((IUnknown *)f);
    for (i = 0; i < NFOREIGN; i++) { void *p = NULL; rfp_create(FT_SHARED, foreign_iids[i], &p); if (p) DW_RELEASE(p); }
    EXPECT_REFS(f, r0);
}
TEST(ty_foreign_iid_isolated_failure_destroys_the_new_factory) {
    /* An isolated factory created for a failing QueryInterface must not survive: observe through a loader registered on a
     * different factory is impossible, so check the cheaper invariant that repeated failures keep working. */
    int i; NEED_EXPORT();
    for (i = 0; i < 200; i++) { void *p = NULL; CHECK_HR(rfp_create(FT_ISOLATED, &g_unknown_iid, &p), E_NOINTERFACE); CHECK(p == NULL); }
    CHECK(dw_new_factory(FT_ISOLATED) != NULL);
}
TEST(ty_iid_win11_factory8_absent_on_19041) {
    void *p = RFP_SENTINEL; HRESULT hr; NEED_EXPORT();
    hr = rfp_create(FT_SHARED, &DW_IID_IDWriteFactory8, &p);
    if (SUCCEEDED(hr)) { t_note("IDWriteFactory8 supported (Windows 11 style build)"); DW_RELEASE(p); }
    else { CHECK_HR(hr, E_NOINTERFACE); CHECK(p == NULL); }
}

/* ---- every IID of the factory chain, both types --------------------------------------------------------- */
static void chain_for(int type) {
    int v; NEED_EXPORT();
    for (v = 0; v <= 7; v++) {
        void *p = RFP_SENTINEL; HRESULT hr = rfp_create(type, dw_factory_iid(v), &p);
        if (v <= 5) { CHECKF(hr == S_OK && p && p != RFP_SENTINEL, "IDWriteFactory%d -> %s", v, dw_hr_name(hr)); }
        else { if (hr != S_OK) t_note("IDWriteFactory%d: %s", v, dw_hr_name(hr)); else CHECK(p != NULL); }
        if (hr != S_OK) CHECK(p == NULL);
        if (hr == S_OK && p) {
            IUnknown *u = NULL;
            CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, &IID_IUnknown, (void **)&u));
            DW_RELEASE(u);
            DW_RELEASE(p);
        }
    }
}
TEST(chain_every_version_shared) { chain_for(FT_SHARED); }
TEST(chain_every_version_isolated) { chain_for(FT_ISOLATED); }

#define CHAIN_QI(name, fromv, tov) \
TEST(chain_qi_##name) { \
    IUnknown *base; void *p = NULL; HRESULT hr; NEED_EXPORT(); \
    base = (IUnknown *)dw_factory_v(FT_ISOLATED, fromv); \
    if (!base) { t_skip("source version unsupported"); return; } \
    hr = IUnknown_QueryInterface(base, dw_factory_iid(tov), &p); \
    if (tov <= 5) CHECK_HR(hr, S_OK); else if (hr != S_OK) t_note("QI IDWriteFactory%d: %s", tov, dw_hr_name(hr)); \
    if (hr == S_OK) { CHECK(p != NULL); CHECK(dw_same_identity(base, (IUnknown *)p)); DW_RELEASE(p); } else CHECK(p == NULL); \
}
CHAIN_QI(0_to_1, 0, 1)
CHAIN_QI(0_to_2, 0, 2)
CHAIN_QI(0_to_3, 0, 3)
CHAIN_QI(0_to_4, 0, 4)
CHAIN_QI(0_to_5, 0, 5)
CHAIN_QI(0_to_6, 0, 6)
CHAIN_QI(0_to_7, 0, 7)
CHAIN_QI(1_to_0, 1, 0)
CHAIN_QI(2_to_0, 2, 0)
CHAIN_QI(3_to_1, 3, 1)
CHAIN_QI(3_to_2, 3, 2)
CHAIN_QI(4_to_3, 4, 3)
CHAIN_QI(5_to_4, 5, 4)
CHAIN_QI(5_to_2, 5, 2)
CHAIN_QI(5_to_6, 5, 6)
CHAIN_QI(5_to_7, 5, 7)

TEST(chain_qi_from_factory7_down_to_0) {
    IUnknown *f7 = (IUnknown *)dw_factory_v(FT_ISOLATED, 7); int v;
    if (!f7) { t_skip("IDWriteFactory7 unsupported"); return; }
    for (v = 0; v <= 7; v++) {
        void *p = NULL; CHECK_HR_OK(IUnknown_QueryInterface(f7, dw_factory_iid(v), &p));
        CHECK(p != NULL && dw_same_identity(f7, (IUnknown *)p)); DW_RELEASE(p);
    }
}
TEST(chain_factory8_not_obtainable_by_qi_from_older) {
    IUnknown *f = (IUnknown *)dw_shared(); void *p = RFP_SENTINEL; HRESULT hr; REQUIRE(f);
    hr = IUnknown_QueryInterface(f, &DW_IID_IDWriteFactory8, &p);
    if (hr == S_OK) { t_note("IDWriteFactory8 available"); DW_RELEASE(p); } else { CHECK_HR(hr, E_NOINTERFACE); CHECK(p == NULL); }
}
TEST(chain_version_support_note) {
    int v;
    NEED_EXPORT();
    for (v = 0; v <= 7; v++) t_note("IDWriteFactory%d on shared: %s", v, dw_factory_v(FT_SHARED, v) ? "supported" : "absent");
}

/* ---- shared identity rules ------------------------------------------------------------------------------ */
TEST(sh_same_pointer_across_calls_for_iunknown) {
    IUnknown *a = NULL, *b = NULL, *c = NULL; NEED_EXPORT();
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&a));
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&b));
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&c));
    CHECK(a == b && b == c);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
}
TEST(sh_same_identity_for_every_version_iid) {
    IUnknown *base = NULL; int v; NEED_EXPORT();
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&base));
    for (v = 0; v <= 7; v++) {
        void *p = NULL; HRESULT hr = rfp_create(FT_SHARED, dw_factory_iid(v), &p);
        if (hr != S_OK) { CHECK(v >= 6); continue; }
        CHECK(dw_same_identity(base, (IUnknown *)p));
        DW_RELEASE(p);
    }
    DW_RELEASE(base);
}
TEST(sh_interface_pointer_values_of_chain_note) {
    int v; NEED_EXPORT();
    for (v = 0; v <= 7; v++) {
        void *p = NULL; if (SUCCEEDED(rfp_create(FT_SHARED, dw_factory_iid(v), &p))) { t_note("shared IDWriteFactory%d pointer %p", v, p); DW_RELEASE(p); }
    }
}
TEST(sh_each_create_adds_one_reference_for_each_iid) {
    IDWriteFactory *f = dw_shared(); int v; REQUIRE(f);
    for (v = 0; v <= 7; v++) {
        void *p = NULL; ULONG r0 = dw_refcount((IUnknown *)f); HRESULT hr = rfp_create(FT_SHARED, dw_factory_iid(v), &p);
        if (hr != S_OK) continue;
        EXPECT_REFS(f, r0 + 1);
        DW_RELEASE(p);
        EXPECT_REFS(f, r0);
    }
}
TEST(sh_factory_usable_after_dropping_all_own_references) {
    IUnknown *a = NULL, *b = NULL; IDWriteTextFormat *tf; NEED_EXPORT();
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&a));
    DW_RELEASE(a);
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&b));
    tf = rfp_fmt((IDWriteFactory *)b, 12.0f, L"en-us");
    CHECK(tf != NULL);
    DW_RELEASE(tf); DW_RELEASE(b);
}
TEST(sh_unbalanced_release_pairs_do_not_free_it) {
    IDWriteFactory *f = dw_shared(); int i; REQUIRE(f);
    for (i = 0; i < 50; i++) { IUnknown_AddRef((IUnknown *)f); IUnknown_Release((IUnknown *)f); }
    CHECK(rfp_fmt(f, 10.0f, L"en-us") != NULL);
}
TEST(sh_object_created_from_shared_reports_same_factory) {
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *tf; IDWriteFontCollection *c = NULL; REQUIRE(f);
    tf = rfp_fmt(f, 12.0f, L"en-us"); REQUIRE(tf);
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(tf, &c));
    CHECK(c != NULL);
    if (c) { IDWriteFactory *got = NULL; /* IDWriteFontCollection has no GetFactory; identity of collection is the factory's system collection */
             IDWriteFontCollection *sys = NULL; CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE)); t_note("text format default collection is the factory system collection: %s", sys == c ? "yes" : "no"); DW_RELEASE(sys); }
    DW_RELEASE(c); DW_RELEASE(tf);
}

/* ---- isolated identity rules ----------------------------------------------------------------------------- */
TEST(iso_distinct_pointer_each_call) {
    IUnknown *p[16]; int i, j; NEED_EXPORT();
    for (i = 0; i < 16; i++) { p[i] = NULL; CHECK_HR_OK(rfp_create(FT_ISOLATED, &IID_IUnknown, (void **)&p[i])); }
    for (i = 0; i < 16; i++) for (j = i + 1; j < 16; j++) CHECK(p[i] != p[j]);
    for (i = 0; i < 16; i++) DW_RELEASE(p[i]);
}
TEST(iso_distinct_from_shared) {
    IUnknown *s = NULL, *i1 = NULL; NEED_EXPORT();
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&s));
    CHECK_HR_OK(rfp_create(FT_ISOLATED, &IID_IUnknown, (void **)&i1));
    CHECK(s != i1);
    DW_RELEASE(s); DW_RELEASE(i1);
}
TEST(iso_initial_refcount_one_for_every_iid) {
    int v; NEED_EXPORT();
    for (v = 0; v <= 7; v++) {
        void *p = NULL; if (rfp_create(FT_ISOLATED, dw_factory_iid(v), &p) != S_OK) continue;
        EXPECT_REFS(p, 1); DW_RELEASE(p);
    }
}
TEST(iso_release_returns_zero_for_sole_reference) {
    IUnknown *u = NULL; ULONG r; NEED_EXPORT();
    CHECK_HR_OK(rfp_create(FT_ISOLATED, &IID_IUnknown, (void **)&u));
    r = IUnknown_Release(u);
    EXPECT_EQ(r, 0);
}
TEST(iso_qi_across_versions_shares_one_count) {
    IDWriteFactory *f = dw_new_factory(FT_ISOLATED); void *p2 = NULL, *p3 = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, &p2));
    CHECK_HR_OK(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, &p3));
    EXPECT_REFS(f, 3);
    EXPECT_REFS(p2, 3);
    DW_RELEASE(p2); EXPECT_REFS(p3, 2); DW_RELEASE(p3); EXPECT_REFS(f, 1);
    DW_RELEASE(f);
}
TEST(iso_objects_survive_factory_release) {
    IDWriteFactory *f = dw_new_factory(FT_ISOLATED); IDWriteTextFormat *tf; IDWriteTextLayout *l = NULL; REQUIRE(f);
    tf = rfp_fmt(f, 14.0f, L"en-us"); REQUIRE(tf);
    DW_RELEASE(f);
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(tf, DWRITE_TEXT_ALIGNMENT_CENTER));
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(tf), DWRITE_TEXT_ALIGNMENT_CENTER);
    DW_RELEASE(tf);
}
TEST(iso_churn_200_cycles_with_use) {
    int i; NEED_EXPORT();
    for (i = 0; i < 200; i++) {
        IDWriteFactory *f = dw_new_factory(FT_ISOLATED); IDWriteTextFormat *tf; if (!f) { CHECK(0); break; }
        tf = rfp_fmt(f, 12.0f, L"en-us"); CHECK(tf != NULL); DW_RELEASE(tf); DW_RELEASE(f);
    }
}
TEST(iso_two_factories_have_independent_loader_registries) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *b = dw_new_factory(FT_ISOLATED);
    dwx_mem_loader *ld; IDWriteFontFile *ff = NULL; UINT32 key; HRESULT hr; uint8_t *blob; size_t n; ttf_spec s;
    if (!a || !b) { t_skip("no factory"); DW_RELEASE(a); DW_RELEASE(b); return; }
    ttf_default_spec(&s); blob = ttf_build(&s, &n);
    ld = dwx_loader_new(); key = dwx_loader_add(ld, blob, n); free(blob);
    CHECK_HR_OK(dwx_loader_register(ld, a));
    CHECK_HR_OK(dwx_loader_file(ld, a, key, &ff)); DW_RELEASE(ff);
    hr = dwx_loader_file(ld, b, key, &ff);   /* loader unknown to factory b */
    CHECK_HR_FAILED(hr); CHECK(ff == NULL);
    dwx_loader_unregister(ld);
    dwx_release(ld);
    DW_RELEASE(a); DW_RELEASE(b);
}
