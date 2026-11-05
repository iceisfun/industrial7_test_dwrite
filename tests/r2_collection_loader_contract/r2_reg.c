/* RegisterFontCollectionLoader / UnregisterFontCollectionLoader bookkeeping.
 *
 * Win10 observed (FontLoaderManagerBase<IDWriteFontCollectionLoader,1>, one instance per factory):
 *  - Register(obj): NULL -> ArgumentException (E_INVALIDARG); an object pointer that is already in the map ->
 *    OSException(0x88985006 DWRITE_E_ALREADYREGISTERED); otherwise a new 64-bit id is allocated, the map owns +1 reference
 *    on the loader, the call returns S_OK.  The loader is not called.
 *  - Unregister(obj): NULL or an unknown pointer -> ArgumentException (E_INVALIDARG); success erases the entry (-1 reference).
 *  - Collections are keyed by (loader id, key bytes, factory): a factory knows nothing about loaders registered elsewhere.
 * (Duplicate registration reports DWRITE_E_ALREADYREGISTERED, not E_INVALIDARG.)
 */
#include "r2.h"

#define BARE(x) r2_fx x; if (!r2_open(&x, DWRITE_FACTORY_TYPE_ISOLATED, 1, 0)) return

/* ------------------------------------------------------------------------- reference accounting */
RT(register_succeeds_with_s_ok_and_takes_one_reference)
{
    BARE(x);
    LONG before = x.cl->ref;
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)), S_OK);
    EXPECT_EQ(x.cl->ref, before + 1);
    x.reg_cl = 1;
    r2_close(&x);
    EXPECT_EQ(x.cl->destroyed, 1);
}
RT(unregister_succeeds_with_s_ok_and_gives_the_reference_back)
{
    BARE(x);
    LONG before = x.cl->ref;
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), S_OK);
    EXPECT_EQ(x.cl->ref, before);
    r2_close(&x);
}
RT(registration_never_calls_the_loader)
{
    BARE(x);
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    EXPECT_EQ(x.cl->n_create, 0);
    EXPECT_EQ(r2_count(EV_CL_CREATE), 0);
    t_note("QueryInterface calls on the loader while registering/unregistering: %ld", (long)x.cl->n_qi);
    r2_close(&x);
}
RT(one_reference_per_factory_when_registered_twice_on_different_factories)
{
    BARE(x);
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { t_skip("no second factory"); r2_close(&x); return; }
    LONG base = x.cl->ref;
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(g, CLP(x.cl)));
    EXPECT_EQ(x.cl->ref, base + 2);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(g, CLP(x.cl)));
    EXPECT_EQ(x.cl->ref, base + 1);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    EXPECT_EQ(x.cl->ref, base);
    DW_RELEASE(g);
    r2_close(&x);
}
RT(register_unregister_cycles_are_balanced)
{
    BARE(x);
    int i; LONG base = x.cl->ref;
    for (i = 0; i < 100; i++) {
        HRESULT a = IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl));
        HRESULT b = IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl));
        if (a != S_OK || b != S_OK) { CHECKF(0, "cycle %d: register %s unregister %s", i, dw_hr_name(a), dw_hr_name(b)); break; }
    }
    EXPECT_EQ(x.cl->ref, base);
    r2_close(&x);
}
RT(factory_release_drops_the_registration_references)
{
    BARE(x);
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { t_skip("no second factory"); r2_close(&x); return; }
    LONG base = x.cl->ref;
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(g, CLP(x.cl)));
    EXPECT_EQ(x.cl->ref, base + 1);
    ULONG left = IDWriteFactory_Release(g);
    EXPECT_EQ(left, 0);
    t_note("loader refs after the registering factory was destroyed: %ld (base %ld)", (long)x.cl->ref, (long)base);
    EXPECT_EQ(x.cl->ref, base);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- invalid calls */
RT(register_null_is_invalid_argument)
{
    BARE(x);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(x.f, NULL), E_INVALIDARG);
    r2_close(&x);
}
RT(unregister_null_is_invalid_argument)
{
    BARE(x);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, NULL), E_INVALIDARG);
    r2_close(&x);
}
RT(duplicate_registration_is_already_registered)
{
    BARE(x);
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    LONG refs = x.cl->ref;
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)), R2_E_ALREADYREGISTERED);
    EXPECT_EQ(x.cl->ref, refs);
    x.reg_cl = 1;
    r2_close(&x);
}
RT(duplicate_registration_ten_times_never_stacks)
{
    BARE(x);
    int i;
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    for (i = 0; i < 10; i++) CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)), R2_E_ALREADYREGISTERED);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);      /* one Unregister removed it entirely */
    r2_close(&x);
}
RT(duplicate_registration_through_another_factory_interface_pointer)
{
    BARE(x);
    void *p3 = NULL;
    if (FAILED(IUnknown_QueryInterface((IUnknown *)x.f, &IID_IDWriteFactory3, &p3)) || !p3) { t_skip("IDWriteFactory3 not supported"); r2_close(&x); return; }
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader((IDWriteFactory *)p3, CLP(x.cl)), R2_E_ALREADYREGISTERED);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader((IDWriteFactory *)p3, CLP(x.cl)));
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    IUnknown_Release((IUnknown *)p3);
    r2_close(&x);
}
RT(unregister_of_a_loader_that_was_never_registered)
{
    BARE(x);
    LONG refs = x.cl->ref;
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    EXPECT_EQ(x.cl->ref, refs);
    r2_close(&x);
}
RT(unregister_twice)
{
    BARE(x);
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    LONG refs = x.cl->ref;
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    EXPECT_EQ(x.cl->ref, refs);
    r2_close(&x);
}
RT(registration_is_by_object_identity_not_by_vtable)
{
    BARE(x);
    r2_cl *peer = r2_cl_new(x.fl);
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(peer)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(peer)));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(peer)));
    r2_obj_release(peer);
    r2_close(&x);
}
RT(unregister_order_is_free)
{
    BARE(x);
    r2_cl *a = r2_cl_new(x.fl), *b = r2_cl_new(x.fl), *c = r2_cl_new(x.fl);
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(a)));
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(b)));
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(c)));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(b)));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(a)));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(c)));
    EXPECT_EQ(a->ref, 1); EXPECT_EQ(b->ref, 1); EXPECT_EQ(c->ref, 1);
    r2_obj_release(a); r2_obj_release(b); r2_obj_release(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- loader identity in the collection key */
RT(eight_loaders_with_the_same_key_each_build_their_own_collection)
{
    BARE(x);
    enum { N = 8 };
    r2_cl *l[N]; IDWriteFontCollection *c[N]; int i;
    for (i = 0; i < N; i++) {
        char nm[32]; size_t n; uint8_t *b; UINT32 k;
        snprintf(nm, sizeof nm, "Own%d", i);
        b = ttf_build_styled(nm, 400, 5, 0, &n); k = r2_fl_add(x.fl, b, n); free(b);
        l[i] = r2_cl_new(x.fl); r2_cl_step_file(l[i], k);
        CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(l[i])));
    }
    for (i = 0; i < N; i++) {
        c[i] = NULL;
        CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, CLP(l[i]), "samekey!", 8, &c[i]), S_OK);   /* identical key bytes for all loaders */
    }
    for (i = 0; i < N; i++) {
        char nm[32]; snprintf(nm, sizeof nm, "Own%d", i);
        EXPECT_EQ(l[i]->n_create, 1);
        if (c[i]) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c[i]), 1); CHECKF(r2_find_family(c[i], nm) == 0, "loader %d produced the wrong family", i); }
    }
    for (i = 0; i < N; i++) { DW_RELEASE(c[i]); IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(l[i])); r2_obj_release(l[i]); }
    r2_close(&x);
}
RT(reregistration_is_a_new_identity_and_reaches_the_loader_again)
{
    FX(x);
    r2_add_family(&x, "Again");
    IDWriteFontCollection *c1 = NULL, *c2 = NULL;
    CHECK_HR(r2_create_key(&x, "stable!!", 8, &c1), S_OK);
    EXPECT_EQ(x.cl->n_create, 1);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR(r2_create_key(&x, "stable!!", 8, &c2), S_OK);
    EXPECT_EQ(x.cl->n_create, 2);
    if (c1) CHECK(r2_find_family(c1, "Again") == 0);          /* the old collection keeps working */
    if (c2) CHECK(r2_find_family(c2, "Again") == 0);
    CHECK(c1 != c2);
    DW_RELEASE(c1); DW_RELEASE(c2);
    r2_close(&x);
}
RT(same_key_twice_on_one_registration_note)
{
    FX(x);
    r2_add_family(&x, "Cached");
    IDWriteFontCollection *c1 = NULL, *c2 = NULL;
    CHECK_HR(r2_create_key(&x, "twice!!!", 8, &c1), S_OK);
    CHECK_HR(r2_create_key(&x, "twice!!!", 8, &c2), S_OK);
    t_note("same key twice: loader called %ld time(s); collections %s", (long)x.cl->n_create, c1 == c2 ? "identical object" : "distinct objects");
    CHECK(x.cl->n_create == 1 || x.cl->n_create == 2);
    if (c1) CHECK(r2_find_family(c1, "Cached") == 0);
    if (c2) CHECK(r2_find_family(c2, "Cached") == 0);
    DW_RELEASE(c1); DW_RELEASE(c2);
    r2_close(&x);
}
RT(different_key_bytes_always_reach_the_loader)
{
    FX(x);
    int i;
    for (i = 0; i < 12; i++) {
        IDWriteFontCollection *c = NULL; char k[8]; snprintf(k, sizeof k, "k%06d", i);
        CHECK_HR(r2_create_key(&x, k, 7, &c), S_OK);
        EXPECT_EQ(x.cl->n_create, i + 1);
        DW_RELEASE(c);
    }
    r2_close(&x);
}
RT(same_bytes_different_size_are_different_keys)
{
    FX(x);
    IDWriteFontCollection *a = NULL, *b = NULL, *c = NULL;
    CHECK_HR(r2_create_key(&x, "keyzzzzz", 4, &a), S_OK);
    CHECK_HR(r2_create_key(&x, "keyzzzzz", 5, &b), S_OK);
    CHECK_HR(r2_create_key(&x, "keyzzzzz", 8, &c), S_OK);
    EXPECT_EQ(x.cl->n_create, 3);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- per factory visibility */
RT(loader_registered_on_one_factory_is_unknown_to_another)
{
    FX(x);
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { t_skip("no second factory"); r2_close(&x); return; }
    r2_add_family(&x, "Local");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(g, CLP(x.cl), "abcdefgh", 8, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(x.cl->n_create, 0);
    CHECK_HR(r2_create(&x, &c), S_OK);
    DW_RELEASE(c);
    DW_RELEASE(g);
    r2_close(&x);
}
RT(unregister_on_the_wrong_factory_changes_nothing)
{
    FX(x);
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { t_skip("no second factory"); r2_close(&x); return; }
    r2_add_family(&x, "Stays");
    LONG refs = x.cl->ref;
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(g, CLP(x.cl)), E_INVALIDARG);
    EXPECT_EQ(x.cl->ref, refs);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    DW_RELEASE(c);
    DW_RELEASE(g);
    r2_close(&x);
}
RT(one_loader_on_two_factories_each_with_its_own_factory_argument)
{
    FX(x);
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { t_skip("no second factory"); r2_close(&x); return; }
    CHECK_HR_OK(r2_fl_register(x.fl, g));
    x.fl->registered_on = x.f;
    r2_add_family(&x, "Both");
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(g, CLP(x.cl)));
    IDWriteFontCollection *c1 = NULL, *c2 = NULL;
    char k[16]; int n = r2_uniq_key(k);
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, CLP(x.cl), k, n, &c1), S_OK);
    CHECKF(x.cl->last_factory && dw_same_identity((IUnknown *)x.cl->last_factory, (IUnknown *)x.f), "first call got the wrong factory");
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(g, CLP(x.cl), k, n, &c2), S_OK);
    CHECKF(x.cl->last_factory && dw_same_identity((IUnknown *)x.cl->last_factory, (IUnknown *)g), "second call got the wrong factory");
    EXPECT_EQ(x.cl->n_create, 2);              /* the same key on another factory is a different cache scope */
    DW_RELEASE(c1); DW_RELEASE(c2);
    IDWriteFactory_UnregisterFontCollectionLoader(g, CLP(x.cl));
    IDWriteFactory_UnregisterFontFileLoader(g, (IDWriteFontFileLoader *)x.fl);
    DW_RELEASE(g);
    r2_close(&x);
}
RT(unregistering_on_one_factory_leaves_the_other_working)
{
    FX(x);
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { t_skip("no second factory"); r2_close(&x); return; }
    r2_fl_register(x.fl, g);
    x.fl->registered_on = x.f;
    r2_add_family(&x, "Remain");
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(g, CLP(x.cl)));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    x.reg_cl = 0;
    IDWriteFontCollection *c = NULL, *d = (IDWriteFontCollection *)(ULONG_PTR)1;
    char k[16]; int n = r2_uniq_key(k);
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, CLP(x.cl), k, n, &d), E_INVALIDARG);
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(g, CLP(x.cl), k, n, &c), S_OK);
    DW_RELEASE(c);
    IDWriteFactory_UnregisterFontCollectionLoader(g, CLP(x.cl));
    IDWriteFactory_UnregisterFontFileLoader(g, (IDWriteFontFileLoader *)x.fl);
    DW_RELEASE(g);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- the shared factory */
RT(shared_factory_registration_roundtrip)
{
    r2_fx x;
    if (!r2_open(&x, DWRITE_FACTORY_TYPE_SHARED, 1, 1)) return;
    r2_add_family(&x, "SharedFam");
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) CHECK(r2_find_family(c, "SharedFam") == 0);
    DW_RELEASE(c);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl))); x.reg_cl = 0;
    CHECK_HR(r2_create(&x, &c), E_INVALIDARG);
    r2_close(&x);
}
RT(shared_factory_registration_is_visible_through_the_second_pointer)
{
    r2_fx x;
    if (!r2_open(&x, DWRITE_FACTORY_TYPE_SHARED, 1, 1)) return;
    IDWriteFactory *second = dw_new_factory(DWRITE_FACTORY_TYPE_SHARED);
    if (!second) { t_skip("no second shared pointer"); r2_close(&x); return; }
    CHECK(dw_same_identity((IUnknown *)x.f, (IUnknown *)second));
    r2_add_family(&x, "SharedTwo");
    IDWriteFontCollection *c = NULL;
    char k[16]; int n = r2_uniq_key(k);
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(second, CLP(x.cl), k, n, &c), S_OK);
    if (c) CHECK(r2_find_family(c, "SharedTwo") == 0);
    DW_RELEASE(c);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(second, CLP(x.cl)), R2_E_ALREADYREGISTERED);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(second, CLP(x.cl)));
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    x.reg_cl = 0;
    DW_RELEASE(second);
    r2_close(&x);
}
RT(shared_factory_loader_survives_a_released_pointer)
{
    r2_fx x;
    if (!r2_open(&x, DWRITE_FACTORY_TYPE_SHARED, 1, 1)) return;
    IDWriteFactory *second = dw_new_factory(DWRITE_FACTORY_TYPE_SHARED);
    if (!second) { t_skip("no second shared pointer"); r2_close(&x); return; }
    r2_add_family(&x, "SharedKeep");
    DW_RELEASE(second);                               /* the shared factory stays alive: x.f still holds it, so does the process-wide cache */
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(shared_and_isolated_factories_do_not_share_registrations)
{
    FX(x);
    IDWriteFactory *sh = dw_new_factory(DWRITE_FACTORY_TYPE_SHARED);
    if (!sh) { t_skip("no shared factory"); r2_close(&x); return; }
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(sh, CLP(x.cl), "abcdefgh", 8, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(x.cl->n_create, 0);
    DW_RELEASE(sh);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- factory interface versions */
RT(registry_is_one_per_factory_object_for_every_interface_version)
{
    int v;
    for (v = 0; v <= 7; v++) {
        BARE(x);
        void *p = NULL;
        if (FAILED(IUnknown_QueryInterface((IUnknown *)x.f, dw_factory_iid(v), &p)) || !p) { r2_close(&x); continue; }
        HRESULT a = IDWriteFactory_RegisterFontCollectionLoader((IDWriteFactory *)p, CLP(x.cl));
        CHECKF(a == S_OK, "v%d register: %s", v, dw_hr_name(a));
        CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)), R2_E_ALREADYREGISTERED);
        CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), S_OK);
        CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader((IDWriteFactory *)p, CLP(x.cl)), E_INVALIDARG);
        IUnknown_Release((IUnknown *)p);
        r2_close(&x);
    }
}

/* ------------------------------------------------------------------------- threads */
typedef struct { IDWriteFactory *f; r2_fl *fl; int n; int failures; } reg_thr;
static DWORD WINAPI reg_worker(void *p)
{
    reg_thr *a = (reg_thr *)p; int i;
    for (i = 0; i < a->n; i++) {
        r2_cl *l = r2_cl_new(a->fl);
        if (IDWriteFactory_RegisterFontCollectionLoader(a->f, CLP(l)) != S_OK) a->failures++;
        if (IDWriteFactory_UnregisterFontCollectionLoader(a->f, CLP(l)) != S_OK) a->failures++;
        r2_obj_release(l);
    }
    return 0;
}
RT(concurrent_register_and_unregister_from_four_threads)
{
    BARE(x);
    enum { N = 4 };
    reg_thr a[N]; HANDLE h[N]; int i;
    memset(a, 0, sizeof a);
    for (i = 0; i < N; i++) { a[i].f = x.f; a[i].fl = x.fl; a[i].n = 25; h[i] = CreateThread(NULL, 0, reg_worker, &a[i], 0, NULL); }
    for (i = 0; i < N; i++) if (h[i]) { EXPECT_EQ(WaitForSingleObject(h[i], 20000), WAIT_OBJECT_0); CloseHandle(h[i]); }
    for (i = 0; i < N; i++) EXPECT_EQ(a[i].failures, 0);
    r2_close(&x);
}
