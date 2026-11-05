/* RegisterFontCollectionLoader / UnregisterFontCollectionLoader / CreateCustomFontCollection: argument validation,
 * registration lifetime, reference counting and the way the collection loader is called.
 *
 * Win10 observed facts used below:
 *  - Register: stores a ComPtr (+1 ref) in a per-factory map keyed by the loader pointer; a second registration of the same
 *    pointer throws OSException(DWRITE_E_ALREADYREGISTERED 0x88985006); NULL throws ArgumentException (E_INVALIDARG).
 *  - Unregister: an unknown or NULL pointer throws ArgumentException (E_INVALIDARG); success erases the map entry (-1 ref).
 *  - CreateCustomFontCollection: *ppCollection = NULL first; NULL loader -> E_INVALIDARG; an unregistered loader makes
 *    GetIDFromObject throw ArgumentException -> E_INVALIDARG. The collection is built inside the call (the loader is
 *    invoked synchronously); the collection only remembers the loader's numeric id, not the loader object.
 */
#include "fcl.h"

#define CLP(x) ((IDWriteFontCollectionLoader *)(x))

/* ------------------------------------------------------------------------------------------------ Register */
TEST(fcl_reg_ok_takes_one_reference)
{
    FCL_OPEN_BARE(x);
    LONG before = dwx_refs(x.cl);
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    EXPECT_EQ(dwx_refs(x.cl), before + 1);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    EXPECT_EQ(dwx_refs(x.cl), before);
    fcl_close(&x);
}
TEST(fcl_reg_twice_is_already_registered)
{
    FCL_OPEN_BARE(x);
    CHECK_HR_OK(dwx_coll_register(x.cl, x.f));
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)), DWRITE_E_ALREADYREGISTERED);
    fcl_close(&x);
}
TEST(fcl_reg_twice_leaves_refcount_alone)
{
    FCL_OPEN_BARE(x);
    CHECK_HR_OK(dwx_coll_register(x.cl, x.f));
    LONG r = dwx_refs(x.cl);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)), DWRITE_E_ALREADYREGISTERED);
    EXPECT_EQ(dwx_refs(x.cl), r);
    /* a single Unregister still removes it, so only one registration existed */
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl))); x.cl->registered = 0;
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    fcl_close(&x);
}
TEST(fcl_reg_null_loader)
{
    FCL_OPEN_BARE(x);
    CHECK_HR(IDWriteFactory_RegisterFontCollectionLoader(x.f, NULL), E_INVALIDARG);
    fcl_close(&x);
}
TEST(fcl_unreg_null_loader)
{
    FCL_OPEN_BARE(x);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, NULL), E_INVALIDARG);
    fcl_close(&x);
}
TEST(fcl_unreg_never_registered)
{
    FCL_OPEN_BARE(x);
    LONG r = dwx_refs(x.cl);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    EXPECT_EQ(dwx_refs(x.cl), r);
    fcl_close(&x);
}
TEST(fcl_unreg_twice)
{
    FCL_OPEN_BARE(x);
    CHECK_HR_OK(dwx_coll_register(x.cl, x.f));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl))); x.cl->registered = 0;
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    fcl_close(&x);
}
TEST(fcl_reg_again_after_unreg)
{
    FCL_OPEN_BARE(x);
    int i;
    for (i = 0; i < 5; i++) {
        CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
        CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    }
    EXPECT_EQ(dwx_refs(x.cl), 1);
    fcl_close(&x);
}
TEST(fcl_reg_unrelated_object_is_not_a_collection_loader_registration)
{
    /* The file loader registry and the collection loader registry are separate: a file loader registration does not make
     * the collection loader usable. */
    FCL_OPEN_BARE(x);
    CHECK_HR_OK(dwx_loader_register(x.fl, x.f));
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(fcl_create(&x, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    fcl_close(&x);
}
TEST(fcl_reg_two_distinct_loaders)
{
    fcl_px a;
    if (!fcl_px_open(&a, 0)) return;
    fcl_cl *second = fcl_cl_new(a.fl);
    CHECK_HR_OK(fcl_cl_register(a.cl, a.f)); a.reg_cl = 1;
    CHECK_HR_OK(fcl_cl_register(second, a.f));
    EXPECT_EQ(second->ref, 2);
    CHECK_HR_OK(fcl_cl_unregister(second, a.f));
    EXPECT_EQ(second->ref, 1);
    /* the first stays registered */
    CHECK_HR(fcl_cl_unregister(second, a.f), E_INVALIDARG);
    CHECK_HR_OK(fcl_cl_unregister(a.cl, a.f)); a.reg_cl = 0;
    fcl_cl_release(second);
    fcl_px_close(&a);
}
TEST(fcl_reg_sixteen_loaders)
{
    FCL_OPEN_BARE(x);
    fcl_cl *l[16]; int i;
    CHECK_HR_OK(dwx_loader_register(x.fl, x.f));
    UINT32 k = fcl_add_family(&x, "Sixteen");
    for (i = 0; i < 16; i++) { l[i] = fcl_cl_new(x.fl); fcl_cl_add(l[i], k); l[i]->expect_factory = x.f; CHECK_HR_OK(fcl_cl_register(l[i], x.f)); }
    for (i = 0; i < 16; i++) {
        IDWriteFontCollection *c = NULL; char key[16]; int n = fcl_uniq_key(key);
        CHECK_HR_OK(IDWriteFactory_CreateCustomFontCollection(x.f, CLP(l[i]), key, n, &c));
        EXPECT_EQ(l[i]->n_create, 1);
        if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        DW_RELEASE(c);
    }
    for (i = 15; i >= 0; i--) { CHECK_HR_OK(fcl_cl_unregister(l[i], x.f)); fcl_cl_release(l[i]); }
    fcl_close(&x);
}
TEST(fcl_reg_same_loader_on_two_factories)
{
    FCL_OPEN_BARE(x);
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { fcl_close(&x); t_skip("second factory"); return; }
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(x.cl)));
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(g, CLP(x.cl)));
    EXPECT_EQ(dwx_refs(x.cl), 3);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)));
    EXPECT_EQ(dwx_refs(x.cl), 2);
    /* registrations are per factory: x.f no longer knows the loader, g does */
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(g, CLP(x.cl)));
    EXPECT_EQ(dwx_refs(x.cl), 1);
    DW_RELEASE(g);
    fcl_close(&x);
}
TEST(fcl_reg_is_per_factory_for_create)
{
    FCL_OPEN(x);
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { fcl_close(&x); t_skip("second factory"); return; }
    fcl_add_family(&x, "PerFactory");
    char key[16]; int n = fcl_uniq_key(key);
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    /* registered on x.f only: g must not find it */
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(g, CLP(x.cl), key, n, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(x.cl->n_create, 0);
    CHECK_HR_OK(IDWriteFactory_CreateCustomFontCollection(x.f, CLP(x.cl), key, n, &c));
    DW_RELEASE(c);
    DW_RELEASE(g);
    fcl_close(&x);
}
TEST(fcl_reg_same_loader_both_factories_create_on_each)
{
    fcl_px p;
    if (!fcl_px_open(&p, 1)) return;
    IDWriteFactory *g = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g) { fcl_px_close(&p); t_skip("second factory"); return; }
    IDWriteFactory_RegisterFontFileLoader(g, (IDWriteFontFileLoader *)p.fl);   /* file loader on g as well, so the same file keys resolve */
    CHECK_HR_OK(fcl_cl_register(p.cl, g));
    fcl_px_add_style(&p, "TwoFactories", 400, 5, 0);
    IDWriteFontCollection *c1 = NULL, *c2 = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c1));
    EXPECT_EQ(p.cl->n_create, 1);
    EXPECT_EQ(p.cl->last_factory != NULL, 1);
    CHECK_HR_OK(IDWriteFactory_CreateCustomFontCollection(g, CLP(p.cl), "gkey0001", 8, &c2));
    EXPECT_EQ(p.cl->n_create, 2);
    CHECK(p.cl->last_factory != NULL && dw_same_identity((IUnknown *)p.cl->last_factory, (IUnknown *)g));
    if (c1) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c1), 1);
    if (c2) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c2), 1);
    DW_RELEASE(c1); DW_RELEASE(c2);
    CHECK_HR_OK(fcl_cl_unregister(p.cl, g));
    IDWriteFactory_UnregisterFontFileLoader(g, (IDWriteFontFileLoader *)p.fl);
    DW_RELEASE(g);
    fcl_px_close(&p);
}
TEST(fcl_reg_on_shared_factory)
{
    IDWriteFactory *f = dw_shared();
    if (!f) { t_skip("no factory"); return; }
    dwx_mem_loader *fl = dwx_loader_new();
    dwx_coll_loader *cl = dwx_coll_new(fl);
    CHECK_HR_OK(dwx_loader_register(fl, f));
    CHECK_HR_OK(dwx_coll_register(cl, f));
    size_t n; uint8_t *b = ttf_build_styled("SharedReg", 400, 5, 0, &n);
    UINT32 k = dwx_loader_add(fl, b, n); free(b);
    dwx_coll_add_key(cl, k);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(dwx_coll_create(cl, f, &c));
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        CHECK(fcl_find(c, "SharedReg") == 0);
    }
    DW_RELEASE(c);
    CHECK_HR_OK(dwx_coll_unregister(cl));
    CHECK_HR_OK(dwx_loader_unregister(fl));
    dwx_release(cl); dwx_release(fl);
}

/* ------------------------------------------------------------------------------------------ Create: arguments */
TEST(fcl_create_null_loader)
{
    FCL_OPEN(x);
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, NULL, "k", 1, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    fcl_close(&x);
}
TEST(fcl_create_unregistered_loader)
{
    FCL_OPEN_BARE(x);
    fcl_add_family(&x, "Never");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(fcl_create(&x, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(x.cl->n_create, 0);   /* never called */
    fcl_close(&x);
}
TEST(fcl_create_after_unregister)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Gone");
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_create(&x, &c));
    DW_RELEASE(c);
    CHECK_HR_OK(dwx_coll_unregister(x.cl));
    c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(fcl_create(&x, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(x.cl->n_create, 1);
    fcl_close(&x);
}
TEST(fcl_create_ok_basics)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Basic");
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_create(&x, &c));
    CHECK(c != NULL);
    EXPECT_EQ(x.cl->n_create, 1);
    EXPECT_EQ(dwx_refs(x.cl), 2);   /* only the registration + ours */
    if (c) EXPECT_REFS(c, 1);
    EXPECT_REFS(x.f, 1);            /* a collection does not hold a reference on its factory */
    DW_RELEASE(c);
    EXPECT_REFS(x.f, 1);
    fcl_close(&x);
}
TEST(fcl_create_loader_is_called_inside_the_call)
{
    /* The collection is built eagerly: by the time CreateCustomFontCollection returns the enumerator has been created, drained and released. */
    FCL_PX(p);
    fcl_px_add_style(&p, "Eager", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    EXPECT_EQ(p.cl->n_create, 1);
    CHECK(p.cl->last_en != NULL);
    if (p.cl->last_en) { EXPECT_EQ(p.cl->last_en->n_move, 2); EXPECT_EQ(p.cl->last_en->n_get, 1); CHECK(p.cl->last_en->destroyed); }
    EXPECT_EQ(strcmp(fcl_ev, "CMGME"), 0);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_create_passes_the_factory)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "FactoryArg", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    CHECK(p.cl->last_factory != NULL);
    EXPECT_EQ(p.cl->factory_identity_ok, 1);        /* same COM object as the factory the collection was requested from */
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_create_factory_arg_is_usable_for_file_references)
{
    /* The enumerator double builds its IDWriteFontFile objects through the factory pointer it was handed. */
    FCL_PX(p);
    fcl_px_add_style(&p, "FactoryUse", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    CHECK(p.cl->last_en && p.cl->last_en->nfiles == 1);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    DW_RELEASE(c);
    fcl_px_close(&p);
}

static const struct { UINT32 size; } key_sizes[] = { {1}, {2}, {3}, {4}, {7}, {8}, {15}, {16}, {17}, {31}, {32}, {33}, {63}, {64}, {100}, {127} };
TEST(fcl_create_key_bytes_roundtrip)
{
    unsigned i;
    for (i = 0; i < sizeof key_sizes / sizeof key_sizes[0]; i++) {
        FCL_PX(p);
        UINT32 ks = key_sizes[i].size, j;
        uint8_t key[128];
        for (j = 0; j < ks; j++) key[j] = (uint8_t)(0x30 + (j * 7 + i) % 200);
        key[0] = (uint8_t)(0x41 + i);
        fcl_px_add_style(&p, "KeyRoundTrip", 400, 5, 0);
        IDWriteFontCollection *c = NULL;
        HRESULT hr = fcl_px_create_key(&p, key, ks, &c);
        CHECKF(hr == S_OK, "key size %u: %s", ks, dw_hr_name(hr));
        EXPECT_EQ(p.cl->n_create, 1);
        EXPECT_EQ(p.cl->last_ks, ks);
        CHECKF(!p.cl->key_was_null, "key size %u: key pointer NULL", ks);
        CHECKF(!memcmp(p.cl->keycopy, key, ks < 128 ? ks : 128), "key size %u: bytes differ", ks);
        DW_RELEASE(c);
        fcl_px_close(&p);
    }
}
TEST(fcl_create_key_with_embedded_zeros)
{
    FCL_PX(p);
    static const uint8_t key[12] = { 0, 0, 0, 0, 1, 0, 2, 0, 0, 0, 0, 0xFF };
    fcl_px_add_style(&p, "ZeroKey", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create_key(&p, key, sizeof key, &c));
    EXPECT_EQ(p.cl->last_ks, sizeof key);
    CHECK(!memcmp(p.cl->keycopy, key, sizeof key));
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_create_key_size_zero_with_pointer)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "EmptyKey", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create_key(&p, "x", 0, &c));
    EXPECT_EQ(p.cl->n_create, 1);
    EXPECT_EQ(p.cl->last_ks, 0);
    /* the key is copied into the loader call; an empty key is handed over as NULL (observed: begin==end -> NULL) */
    t_note("key size 0 with non-NULL pointer: loader saw key=%s", p.cl->key_was_null ? "NULL" : "non-NULL");
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_create_key_null_size_zero)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "NullKey", 400, 5, 0);
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = fcl_px_create_key(&p, NULL, 0, &c);
    CHECK_HR_OK(hr);
    if (hr == S_OK) { EXPECT_EQ(p.cl->last_ks, 0); EXPECT_EQ(p.cl->n_create, 1); CHECK(c != NULL); }
    t_note("NULL key: loader saw key=%s", p.cl->key_was_null ? "NULL" : "non-NULL");
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_create_key_is_copied_not_borrowed)
{
    /* The loader receives its own copy of the key bytes (never the caller's pointer) and the caller may reuse the buffer afterwards. */
    FCL_PX(p);
    char *key = (char *)malloc(16);
    strcpy(key, "copykey1");
    fcl_px_add_style(&p, "KeyCopy", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create_key(&p, key, 8, &c));
    t_note("loader key pointer %s the caller's pointer", p.cl->last_key == key ? "equals" : "differs from");
    memset(key, 0xEE, 16);
    free(key);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        CHECK(fcl_find(c, "KeyCopy") == 0);
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_create_distinct_keys_reach_the_loader_each_time)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "DistinctKeys", 400, 5, 0);
    IDWriteFontCollection *c[6]; int i;
    for (i = 0; i < 6; i++) {
        char key[16]; snprintf(key, sizeof key, "distinct%d", i);
        c[i] = NULL;
        CHECK_HR_OK(fcl_px_create_key(&p, key, (UINT32)strlen(key), &c[i]));
        EXPECT_EQ(p.cl->n_create, i + 1);
        CHECK(!strncmp((char *)p.cl->keycopy, key, strlen(key)));
    }
    for (i = 0; i < 6; i++) for (int j = i + 1; j < 6; j++) CHECK(c[i] != c[j]);
    for (i = 0; i < 6; i++) DW_RELEASE(c[i]);
    fcl_px_close(&p);
}
TEST(fcl_create_same_key_twice)
{
    /* The factory may serve the second request from its cache; both calls must succeed and describe the same fonts. */
    FCL_PX(p);
    fcl_px_add_style(&p, "SameKey", 400, 5, 0);
    IDWriteFontCollection *a = NULL, *b = NULL;
    CHECK_HR_OK(fcl_px_create_key(&p, "samekey1", 8, &a));
    CHECK_HR_OK(fcl_px_create_key(&p, "samekey1", 8, &b));
    t_note("same key twice: loader called %ld time(s), collections %s", (long)p.cl->n_create, a == b ? "identical" : "distinct objects");
    CHECK(p.cl->n_create == 1 || p.cl->n_create == 2);
    if (a && b) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(a), IDWriteFontCollection_GetFontFamilyCount(b));
        CHECK(fcl_find(a, "SameKey") == fcl_find(b, "SameKey"));
    }
    DW_RELEASE(a); DW_RELEASE(b);
    fcl_px_close(&p);
}
TEST(fcl_create_does_not_retain_the_loader)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "NoRetain", 400, 5, 0);
    LONG r0 = p.cl->ref;
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    EXPECT_EQ(p.cl->ref, r0);                 /* registration ref only: temporary references were all dropped */
    DW_RELEASE(c);
    EXPECT_EQ(p.cl->ref, r0);
    fcl_px_close(&p);
}
TEST(fcl_loader_refs_drop_after_unregister_with_live_collection)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "LoaderGone", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    CHECK_HR_OK(fcl_cl_unregister(p.cl, p.f)); p.reg_cl = 0;
    EXPECT_EQ(p.cl->ref, 1);
    ULONG last = fcl_cl_release(p.cl);        /* drop our reference: the collection must not be keeping the loader alive */
    EXPECT_EQ(last, 0);
    CHECK(p.cl->destroyed);
    p.cl->ref = 1; p.cl->destroyed = 0;       /* restore for the fixture teardown */
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); CHECK(fcl_find(c, "LoaderGone") == 0); }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_collection_survives_loader_unregistration)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Survivor", 400, 5, 0);
    fcl_add_style(&x, "Survivor", 700, 5, 0);
    fcl_add_style(&x, "Other Fam", 400, 5, 0);
    FCL_COLL(x, c);
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c);
    EXPECT_EQ(n, 2);
    CHECK_HR_OK(dwx_coll_unregister(x.cl));
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), n);
    CHECK(fcl_find(c, "Survivor") >= 0);
    CHECK(fcl_find(c, "Other Fam") >= 0);
    char nm[64];
    for (UINT32 i = 0; i < n; i++) { CHECK(fcl_name_at(c, i, nm, sizeof nm)); CHECK(nm[0] != 0); }
    long si = fcl_find(c, "Survivor");
    if (si >= 0) CHECK(fcl_font_count(c, (UINT32)si) >= 2);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_collection_survives_unregister_then_text_format)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "FormatFam", 400, 5, 0);
    FCL_COLL(x, c);
    CHECK_HR_OK(dwx_coll_unregister(x.cl));
    IDWriteTextFormat *fmt = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(x.f, L"FormatFam", c, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &fmt));
    if (fmt) {
        IDWriteFontCollection *back = NULL;
        CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(fmt, &back));
        CHECK(back != NULL && dw_same_identity((IUnknown *)back, (IUnknown *)c));
        DW_RELEASE(back);
    }
    DW_RELEASE(fmt);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_reregistered_loader_is_a_new_registration)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Rereg");
    IDWriteFontCollection *a = NULL, *b = NULL;
    CHECK_HR_OK(fcl_create_with(&x, "reregkey", 8, &a));
    CHECK_HR_OK(dwx_coll_unregister(x.cl));
    CHECK_HR_OK(dwx_coll_register(x.cl, x.f));
    /* same key bytes, but the loader got a new id on re-registration, so it must be asked again */
    CHECK_HR_OK(fcl_create_with(&x, "reregkey", 8, &b));
    EXPECT_EQ(x.cl->n_create, 2);
    if (b) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(b), 1);
    DW_RELEASE(a); DW_RELEASE(b);
    fcl_close(&x);
}
TEST(fcl_create_release_order_collection_last)
{
    /* Both loaders are unregistered while the collection is alive; it keeps working and is released last. */
    FCL_PX(p);
    fcl_px_add_style(&p, "ReleaseOrder", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    CHECK_HR_OK(fcl_cl_unregister(p.cl, p.f)); p.reg_cl = 0;
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        IDWriteFontFamily *fam = NULL;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_create_many_collections)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "Many", 400, 5, 0);
    IDWriteFontCollection *c[40]; int i;
    for (i = 0; i < 40; i++) { c[i] = NULL; CHECK_HR_OK(fcl_px_create(&p, &c[i])); }
    EXPECT_EQ(p.cl->n_create, 40);
    for (i = 39; i >= 0; i--) { if (c[i]) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c[i]), 1); DW_RELEASE(c[i]); }
    fcl_px_close(&p);
}
TEST(fcl_create_runs_on_calling_thread)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "SameThread", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    EXPECT_EQ(p.cl->tid, GetCurrentThreadId());
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_loader_gets_no_unexpected_interface_queries)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "QiProbe", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    t_note("loader QueryInterface calls during Register+Create: %ld", (long)p.cl->n_qi);
    int i;
    for (i = 0; i < p.cl->n_qi_iids; i++) {
        const IID *g = &p.cl->qi_iids[i];
        t_note("  loader QI iid %08lx-%04x", g->Data1, g->Data2);
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}

/* ------------------------------------------------------------------------------------------ threads */
typedef struct { fcl_px *p; int fails; int made; } thr_arg;
static DWORD WINAPI thr_create(LPVOID a)
{
    thr_arg *t = (thr_arg *)a; int i;
    for (i = 0; i < 6; i++) {
        IDWriteFontCollection *c = NULL;
        HRESULT hr = fcl_px_create(t->p, &c);
        if (hr != S_OK || !c) { t->fails++; DW_RELEASE(c); continue; }
        if (IDWriteFontCollection_GetFontFamilyCount(c) != 1) t->fails++;
        t->made++;
        DW_RELEASE(c);
    }
    return 0;
}
TEST(fcl_create_from_several_threads)
{
    FCL_PX(p);
    fcl_px_add_style(&p, "Threaded", 400, 5, 0);
    HANDLE h[4]; thr_arg a[4]; int i, made = 0, fails = 0;
    for (i = 0; i < 4; i++) { a[i].p = &p; a[i].fails = a[i].made = 0; h[i] = CreateThread(NULL, 0, thr_create, &a[i], 0, NULL); }
    for (i = 0; i < 4; i++) { if (h[i]) { if (WaitForSingleObject(h[i], 20000) != WAIT_OBJECT_0) { t_note("thread timeout"); fails++; } CloseHandle(h[i]); } made += a[i].made; fails += a[i].fails; }
    EXPECT_EQ(fails, 0);
    EXPECT_EQ(made, 24);
    EXPECT_EQ(p.cl->n_create, 24);
    fcl_px_close(&p);
}
