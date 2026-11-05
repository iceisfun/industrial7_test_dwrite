/* DWriteCreateFactory(riid) and the factory interface family IDWriteFactory .. IDWriteFactory7.
 *
 * DWriteCreateFactory (decomp 0x1800434c0): writes *ppFactory = NULL first, creates (isolated) or fetches (shared) a
 * DWriteFactory and returns its QueryInterface(riid, ppFactory). The factory QueryInterface table
 * (ComInterfaceMap<DWriteFactory,...>::interfaceMap_) lists IDWriteFactory .. IDWriteFactory7 and IUnknown; an IID that is not
 * in the table gives E_NOINTERFACE (0x80004002) with *ppv = NULL. The table lookup has no NULL check on either argument,
 * so no test passes a NULL riid or a NULL ppv.
 */
#include "fq.h"

static HRESULT create_raw(DWRITE_FACTORY_TYPE t, REFIID riid, void **out) {
    dw_create_fn fn = dw_proc();
    if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    return fn(t, riid, (IUnknown **)out);
}

static const IID zero_iid = { 0 };
static const DWRITE_FACTORY_TYPE k_types[2] = { DWRITE_FACTORY_TYPE_SHARED, DWRITE_FACTORY_TYPE_ISOLATED };
static const char *const k_type_names[2] = { "shared", "isolated" };

static int is_factory_name(const char *n) {
    return !strncmp(n, "IDWriteFactory", 14) && (n[14] == 0 || (n[14] >= '1' && n[14] <= '7' && n[15] == 0));
}

TEST(create_export_present) {
    CHECK(t_module("dwrite.dll") != NULL);
    CHECK(dw_proc() != NULL);
    /* the only named export of the DLL */
    CHECK(GetProcAddress(t_module("dwrite.dll"), "DWriteCreateFactory") != NULL);
    CHECK(GetProcAddress(t_module("dwrite.dll"), "DWriteCreateFactory") == (FARPROC)dw_proc());
}

static void riid_matrix(int ti) {
    int n, i; const dw_qi_row *rows = dw_iid_table(&n);
    if (!dw_proc()) { t_skip("no export"); return; }
    for (i = 0; i < n; i++) {
        void *out = (void *)(ULONG_PTR)0x1;   /* DWriteCreateFactory must overwrite this with NULL before anything else */
        HRESULT hr = create_raw(k_types[ti], rows[i].iid, &out);
        if (is_factory_name(rows[i].name)) {
            CHECKF(hr == S_OK, "%s factory as %s: %s", k_type_names[ti], rows[i].name, dw_hr_name(hr));
            CHECKF(out != NULL && out != (void *)1, "%s: out %p", rows[i].name, out);
            if (out && out != (void *)1) IUnknown_Release((IUnknown *)out);
        } else {
            CHECKF(hr == E_NOINTERFACE, "%s factory as %s: %s", k_type_names[ti], rows[i].name, dw_hr_name(hr));
            CHECKF(out == NULL, "%s: out pointer not zeroed on failure (%p)", rows[i].name, out);
            if (SUCCEEDED(hr) && out && out != (void *)1) IUnknown_Release((IUnknown *)out);
        }
    }
}
TEST(create_riid_matrix_shared)   { riid_matrix(0); }
TEST(create_riid_matrix_isolated) { riid_matrix(1); }

static void riid_iunknown(int ti) {
    void *out = NULL; IUnknown *u2 = NULL;
    if (!dw_proc()) { t_skip("no export"); return; }
    CHECK_HR(create_raw(k_types[ti], &IID_IUnknown, &out), S_OK);
    CHECK(out != NULL);
    if (out) {
        /* IUnknown pointer of a factory is a legitimate factory identity: QI(IDWriteFactory) from it works */
        IDWriteFactory *f = NULL;
        CHECK_HR(IUnknown_QueryInterface((IUnknown *)out, &IID_IDWriteFactory, (void **)&f), S_OK);
        CHECK(f != NULL);
        if (f) { CHECK(dw_same_identity((IUnknown *)f, (IUnknown *)out)); IDWriteFactory_Release(f); }
        IUnknown_Release((IUnknown *)out);
    }
    (void)u2;
}
TEST(create_riid_iunknown_shared)   { riid_iunknown(0); }
TEST(create_riid_iunknown_isolated) { riid_iunknown(1); }

static void riid_extra(int ti) {
    static const struct { const IID *iid; const char *n; } e[] = {
        { &IID_IStream, "IStream" }, { &IID_IDispatch, "IDispatch" }, { &IID_IMarshal, "IMarshal" }, { &IID_IPersist, "IPersist" },
        { &IID_IClassFactory, "IClassFactory" }, { &zero_iid, "GUID_NULL" }, { &IID_ID2D1Factory, "ID2D1Factory" },
    };
    size_t i;
    if (!dw_proc()) { t_skip("no export"); return; }
    for (i = 0; i < sizeof e / sizeof e[0]; i++) {
        void *out = (void *)(ULONG_PTR)0x1;
        HRESULT hr = create_raw(k_types[ti], e[i].iid, &out);
        CHECKF(hr == E_NOINTERFACE, "%s: %s", e[i].n, dw_hr_name(hr));
        CHECKF(out == NULL, "%s: out %p", e[i].n, out);
        if (SUCCEEDED(hr) && out && out != (void *)1) IUnknown_Release((IUnknown *)out);
    }
}
TEST(create_riid_unrelated_shared)   { riid_extra(0); }
TEST(create_riid_unrelated_isolated) { riid_extra(1); }

TEST(create_failure_leaves_shared_factory_usable) {
    void *out = NULL; IDWriteFactory *f = NULL; IDWriteTypography *t = NULL;
    if (!dw_proc()) { t_skip("no export"); return; }
    CHECK_HR(create_raw(DWRITE_FACTORY_TYPE_SHARED, &IID_IStream, &out), E_NOINTERFACE);
    CHECK_HR(create_raw(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (void **)&f), S_OK);
    if (f) { CHECK_HR(IDWriteFactory_CreateTypography(f, &t), S_OK); DW_RELEASE(t); IDWriteFactory_Release(f); }
}
TEST(create_failure_isolated_then_success) {
    void *out = NULL; IDWriteFactory *f = NULL;
    int i;
    if (!dw_proc()) { t_skip("no export"); return; }
    for (i = 0; i < 20; i++) CHECK_HR(create_raw(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IMarshal, &out), E_NOINTERFACE);
    CHECK_HR(create_raw(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, (void **)&f), S_OK);
    if (f) { EXPECT_REFS(f, 1); IDWriteFactory_Release(f); }
}

/* ---- every version reachable through QueryInterface, from every starting version ---- */
TEST(factory_all_versions_supported) {
    int v, t;
    for (t = 0; t < 2; t++) for (v = 0; v <= 7; v++) {
        void *p = dw_factory_v(k_types[t], v);
        CHECKF(p != NULL, "%s factory: IDWriteFactory%d not available", k_type_names[t], v);
    }
}
TEST(factory_version_8_unsupported) {
    int t;
    for (t = 0; t < 2; t++) {
        IDWriteFactory *f = k_types[t] == DWRITE_FACTORY_TYPE_SHARED ? dw_shared() : dw_new_factory(k_types[t]);
        void *p = (void *)1;
        if (!f) { t_skip("no factory"); return; }
        CHECK_HR(IDWriteFactory_QueryInterface(f, &DW_IID_IDWriteFactory8, &p), E_NOINTERFACE);
        CHECK(p == NULL);
        if (k_types[t] == DWRITE_FACTORY_TYPE_ISOLATED) IDWriteFactory_Release(f);
    }
}
TEST(factory_cross_version_qi) {
    /* from IDWriteFactory<a> reach IDWriteFactory<b> for every a,b */
    int a, b, t;
    for (t = 0; t < 2; t++) {
        IDWriteFactory *base = dw_new_factory(k_types[t]);
        if (!base) { t_skip("no factory"); return; }
        for (a = 0; a <= 7; a++) {
            void *pa = NULL;
            if (FAILED(IUnknown_QueryInterface((IUnknown *)base, dw_factory_iid(a), &pa))) { CHECKF(0, "QI factory%d", a); continue; }
            for (b = 0; b <= 7; b++) {
                void *pb = NULL; HRESULT hr = IUnknown_QueryInterface((IUnknown *)pa, dw_factory_iid(b), &pb);
                CHECKF(hr == S_OK, "%s: factory%d -> factory%d: %s", k_type_names[t], a, b, dw_hr_name(hr));
                if (pb) { CHECKF(dw_same_identity((IUnknown *)pb, (IUnknown *)base), "identity %d->%d", a, b); IUnknown_Release((IUnknown *)pb); }
            }
            IUnknown_Release((IUnknown *)pa);
        }
        EXPECT_REFS(base, 1);
        IDWriteFactory_Release(base);
    }
}
TEST(factory_pointer_layout_note) {
    /* The QI table maps IUnknown to a secondary base (+8) and IDWriteFactory..7 to offset 0 in the object returned by
     * DWriteCreateFactory; record what this build does. */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); int v; void *u = NULL;
    if (!f) { t_skip("no factory"); return; }
    IUnknown_QueryInterface((IUnknown *)f, &IID_IUnknown, &u);
    for (v = 0; v <= 7; v++) {
        void *p = NULL;
        IUnknown_QueryInterface((IUnknown *)f, dw_factory_iid(v), &p);
        t_note("factory%d interface pointer %p (IUnknown %p, delta %lld)", v, p, u, (long long)((char *)u - (char *)p));
        CHECKF(p == (void *)f, "IDWriteFactory%d pointer %p differs from the pointer DWriteCreateFactory returned (%p)", v, p, (void *)f);
        if (p) IUnknown_Release((IUnknown *)p);
    }
    if (u) IUnknown_Release((IUnknown *)u);
    IDWriteFactory_Release(f);
}

/* ---- shared factory semantics ---- */
TEST(shared_factory_is_singleton) {
    IDWriteFactory *a = NULL, *b = NULL; ULONG r1, r2;
    if (!dw_proc()) { t_skip("no export"); return; }
    CHECK_HR(create_raw(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (void **)&a), S_OK);
    r1 = dw_refcount((IUnknown *)a);
    CHECK_HR(create_raw(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (void **)&b), S_OK);
    r2 = dw_refcount((IUnknown *)a);
    CHECK(a == b);
    CHECK(dw_same_identity((IUnknown *)a, (IUnknown *)b));
    EXPECT_EQ(r2, r1 + 1);
    t_note("shared factory refcount: %lu with one caller reference, %lu with two", r1, r2);
    IDWriteFactory_Release(b);
    EXPECT_EQ(dw_refcount((IUnknown *)a), r1);
    IDWriteFactory_Release(a);
}
TEST(shared_factory_all_versions_one_identity) {
    int v; IUnknown *id0 = NULL;
    IDWriteFactory *f = dw_shared();
    if (!f) { t_skip("no factory"); return; }
    IUnknown_QueryInterface((IUnknown *)f, &IID_IUnknown, (void **)&id0);
    for (v = 1; v <= 7; v++) {
        void *p = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v);
        CHECKF(p && dw_same_identity((IUnknown *)p, id0), "factory%d identity", v);
    }
    if (id0) IUnknown_Release(id0);
}
TEST(shared_factory_survives_caller_releases) {
    /* Releasing every caller reference of the shared factory must not invalidate later DWriteCreateFactory(SHARED) calls. */
    int i;
    for (i = 0; i < 5; i++) {
        IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_SHARED); IDWriteTypography *t = NULL;
        CHECKF(f != NULL, "iteration %d", i);
        if (!f) break;
        CHECK_HR(IDWriteFactory_CreateTypography(f, &t), S_OK);
        DW_RELEASE(t);
        IDWriteFactory_Release(f);
    }
}
TEST(isolated_factories_are_distinct) {
    IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *b = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *s = dw_new_factory(DWRITE_FACTORY_TYPE_SHARED);
    if (!a || !b || !s) { t_skip("no factory"); return; }
    CHECK(a != b);
    CHECK(!dw_same_identity((IUnknown *)a, (IUnknown *)b));
    CHECK(!dw_same_identity((IUnknown *)a, (IUnknown *)s));
    CHECK(!dw_same_identity((IUnknown *)b, (IUnknown *)s));
    /* decomp 0x1800434c0: ctor, AddRef, QueryInterface (+1), Release -> exactly one reference for the caller */
    EXPECT_REFS(a, 1); EXPECT_REFS(b, 1);
    IDWriteFactory_Release(a);
    EXPECT_REFS(b, 1);
    IDWriteFactory_Release(b); IDWriteFactory_Release(s);
}
TEST(isolated_factory_refcount_through_versions) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); void *p[8]; int v; ULONG r;
    if (!f) { t_skip("no factory"); return; }
    EXPECT_REFS(f, 1);
    for (v = 0; v <= 7; v++) { p[v] = NULL; IUnknown_QueryInterface((IUnknown *)f, dw_factory_iid(v), &p[v]); EXPECT_REFS(f, 2 + v); }
    for (v = 0; v <= 7; v++) { r = IUnknown_Release((IUnknown *)p[v]); EXPECT_EQ(r, (ULONG)(8 - v)); }
    EXPECT_REFS(f, 1);
    r = IDWriteFactory_Release(f);
    EXPECT_EQ(r, 0);
}
TEST(isolated_factory_survives_release_of_creator_pointer) {
    /* hold IDWriteFactory7 only; release the pointer DWriteCreateFactory returned; factory still works */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory7 *f7 = NULL; IDWriteTypography *t = NULL;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory7, (void **)&f7), S_OK);
    IDWriteFactory_Release(f);
    if (f7) {
        EXPECT_REFS(f7, 1);
        /* ABI: the IDWriteFactory prefix of an IDWriteFactory7 vtable is callable through the base type */
        CHECK_HR(IDWriteFactory_CreateTypography((IDWriteFactory *)f7, &t), S_OK);
        DW_RELEASE(t);
        { ULONG r = IDWriteFactory7_Release(f7); EXPECT_EQ(r, 0); }
    }
}
TEST(factory_vtable_slot_counts) {
    /* IDWriteFactory methods 3..26, then Factory1 (+3), Factory2 (+...). The SDK vtable sizes pin the layout. */
    EXPECT_EQ(fq_slots_of("IDWriteFactory"), 3 + 21);
    CHECK(fq_slots_of("IDWriteFactory1") > fq_slots_of("IDWriteFactory"));
    CHECK(fq_slots_of("IDWriteFactory2") > fq_slots_of("IDWriteFactory1"));
    CHECK(fq_slots_of("IDWriteFactory3") > fq_slots_of("IDWriteFactory2"));
    CHECK(fq_slots_of("IDWriteFactory4") > fq_slots_of("IDWriteFactory3"));
    CHECK(fq_slots_of("IDWriteFactory5") > fq_slots_of("IDWriteFactory4"));
    CHECK(fq_slots_of("IDWriteFactory6") > fq_slots_of("IDWriteFactory5"));
    CHECK(fq_slots_of("IDWriteFactory7") > fq_slots_of("IDWriteFactory6"));
}
TEST(factory_vtable_prefix_note) {
    /* Do the IDWriteFactory prefix slots of the Factory7 vtable point at the same code as the IDWriteFactory vtable? Record only. */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); int v, k, same;
    if (!f) { t_skip("no factory"); return; }
    for (v = 1; v <= 7; v++) {
        IUnknown *p = NULL; void **vt0 = *(void ***)f, **vtv; int n = fq_slots_of("IDWriteFactory");
        IUnknown_QueryInterface((IUnknown *)f, dw_factory_iid(v), (void **)&p);
        if (!p) continue;
        vtv = *(void ***)p; same = 0;
        for (k = 0; k < n; k++) if (vt0[k] == vtv[k]) same++;
        t_note("IDWriteFactory%d: %d of %d IDWriteFactory slots share code with the IDWriteFactory vtable (same vtable: %d)", v, same, n, vt0 == vtv);
        IUnknown_Release(p);
    }
    IDWriteFactory_Release(f);
}
