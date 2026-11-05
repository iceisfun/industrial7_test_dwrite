/* IDWriteFontCollectionLoader::CreateEnumeratorFromKey and IDWriteFontFileEnumerator::MoveNext / GetCurrentFontFile as dwrite.dll
 * drives them from CreateCustomFontCollection (measured on Win10 22H2, standard and elevated user).
 *   - the loader is asked once per collection creation, with the key bytes and size given by the caller and the creating factory
 *   - the enumerator is walked eagerly during creation: MoveNext n+1 times and GetCurrentFontFile n times for n files
 *   - a failing CreateEnumeratorFromKey or MoveNext fails the creation with the callback's HRESULT, a failing GetCurrentFontFile
 *     only drops that file
 *   - the enumerator is released before the call returns */
#include "dwx.h"

typedef struct efx { IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; } efx;
static int efx_open(efx *x, int nfonts)
{
    int i;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("factory unavailable"); return 0; }
    x->fl = dwx_loader_new(); x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < nfonts; i++) {
        ttf_spec s; size_t n; uint8_t *b; char nm[24];
        snprintf(nm, sizeof nm, "Eum Enum %c", 'A' + i);
        ttf_style_spec(&s, nm, 400, 5, 0); b = ttf_build(&s, &n);
        dwx_coll_add_key(x->cl, dwx_loader_add(x->fl, b, n)); free(b);
    }
    dwx_loader_register(x->fl, x->f); dwx_coll_register(x->cl, x->f);
    return 1;
}
static void efx_close(efx *x)
{
    dwx_coll_unregister(x->cl); dwx_loader_unregister(x->fl);
    dwx_release(x->cl); dwx_release(x->fl); DW_RELEASE(x->f);
}
#define EFX_BEGIN(n) efx X; if (!efx_open(&X, n)) return

TEST(eum_enum_protocol_counts)
{
    int n;
    for (n = 0; n <= 4; n++) {
        EFX_BEGIN(n); IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(dwx_coll_create(X.cl, X.f, &c));
        EXPECT_EQ(X.cl->n_create, 1);
        EXPECT_EQ(X.cl->nenums, 1);
        CHECK(X.cl->last_enum != NULL);
        if (X.cl->last_enum) {
            CHECKF(X.cl->last_enum->n_movenext == n + 1, "n=%d MoveNext %ld", n, X.cl->last_enum->n_movenext);
            CHECKF(X.cl->last_enum->n_getcurrent == n, "n=%d GetCurrentFontFile %ld", n, X.cl->last_enum->n_getcurrent);
            CHECK(X.cl->last_enum->b.destroyed);               /* released again before CreateCustomFontCollection returned */
        }
        if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), n);
        /* family access after creation does not walk the enumerator again */
        if (X.cl->last_enum) CHECK(X.cl->last_enum->n_movenext == n + 1);
        EXPECT_EQ(X.cl->n_create, 1);
        DW_RELEASE(c); efx_close(&X);
    }
}
TEST(eum_enum_key_and_factory_passed_through)
{
    EFX_BEGIN(2); IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(dwx_coll_create(X.cl, X.f, &c));
    EXPECT_EQ(X.cl->last_key_size, 4);
    CHECK(!memcmp(X.cl->last_key, "dwx", 3));
    CHECK(X.cl->last_factory == X.f);
    /* control: a different key reaches the loader with its own bytes and size */
    { IDWriteFontCollection *c2 = NULL; static const char k2[] = "second-key";
      CHECK_HR_OK(IDWriteFactory_CreateCustomFontCollection(X.f, (IDWriteFontCollectionLoader *)X.cl, k2, (UINT32)sizeof k2, &c2));
      EXPECT_EQ(X.cl->last_key_size, sizeof k2);
      CHECK(!memcmp(X.cl->last_key, k2, sizeof k2));
      EXPECT_EQ(X.cl->n_create, 2);
      DW_RELEASE(c2); }
    DW_RELEASE(c); efx_close(&X);
}
TEST(eum_enum_create_failure_is_the_result)
{
    static const HRESULT hrs[] = { E_FAIL, E_INVALIDARG, E_ACCESSDENIED, (HRESULT)0x80ABCDEF };
    int i;
    for (i = 0; i < 4; i++) {
        EFX_BEGIN(2); IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
        X.cl->fail_create = hrs[i];
        CHECK_HR(dwx_coll_create(X.cl, X.f, &c), hrs[i]);
        CHECK_OUT_NULL(c);
        EXPECT_EQ(X.cl->n_create, 1);
        EXPECT_EQ(X.cl->nenums, 0);
        /* control: with the failure cleared the same fixture succeeds */
        X.cl->fail_create = 0;
        CHECK_HR_OK(dwx_coll_create(X.cl, X.f, &c));
        CHECK(c != NULL);
        DW_RELEASE(c); efx_close(&X);
    }
}
TEST(eum_enum_movenext_failure_is_the_result)
{
    static const HRESULT hrs[] = { E_FAIL, E_ACCESSDENIED, (HRESULT)0x80ABCDEF };
    int i;
    for (i = 0; i < 3; i++) {
        EFX_BEGIN(3); IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1; dwx_enum *e;
        X.cl->enum_fail_move = hrs[i];
        CHECK_HR(dwx_coll_create(X.cl, X.f, &c), hrs[i]);
        CHECK_OUT_NULL(c);
        e = X.cl->last_enum;
        CHECK(e && e->n_movenext == 1 && e->n_getcurrent == 0 && e->b.destroyed);   /* stops at the first failure, enumerator released */
        /* control: a working enumerator on the same loader succeeds */
        X.cl->enum_fail_move = 0;
        { static const char k2[] = "ok"; IDWriteFontCollection *c2 = NULL;
          CHECK_HR_OK(IDWriteFactory_CreateCustomFontCollection(X.f, (IDWriteFontCollectionLoader *)X.cl, k2, 3, &c2));
          DW_RELEASE(c2); }
        efx_close(&X);
    }
}
TEST(eum_enum_getcurrent_failure_drops_the_file)
{
    EFX_BEGIN(3); IDWriteFontCollection *c = NULL;
    X.cl->enum_fail_get = E_FAIL;
    CHECK_HR_OK(dwx_coll_create(X.cl, X.f, &c));
    CHECK(c != NULL);
    if (X.cl->last_enum) { EXPECT_EQ(X.cl->last_enum->n_movenext, 4); EXPECT_EQ(X.cl->last_enum->n_getcurrent, 3); }
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 0);      /* every file was dropped */
    DW_RELEASE(c);
    /* control: without the failure the three families exist */
    X.cl->enum_fail_get = 0;
    { IDWriteFontCollection *c2 = NULL; static const char k2[] = "other";
      CHECK_HR_OK(IDWriteFactory_CreateCustomFontCollection(X.f, (IDWriteFontCollectionLoader *)X.cl, k2, 6, &c2));
      if (c2) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c2), 3);
      DW_RELEASE(c2); }
    efx_close(&X);
}

/* ---- the doubles' own protocol, called directly -------------------------------------------------------------------------- */
TEST(eum_enum_double_direct_protocol)
{
    EFX_BEGIN(2); IDWriteFontCollection *c = NULL;
    IDWriteFontFileEnumerator *e = NULL; BOOL has = 7; IDWriteFontFile *ff = NULL;
    /* a fresh enumerator straight from the loader interface */
    CHECK_HR_OK(IDWriteFontCollectionLoader_CreateEnumeratorFromKey((IDWriteFontCollectionLoader *)X.cl, X.f, "dwx", 4, &e));
    CHECK(e != NULL);
    if (e) {
        CHECK_HR_OK(IDWriteFontFileEnumerator_MoveNext(e, &has)); EXPECT_EQ(has, 1);
        CHECK_HR_OK(IDWriteFontFileEnumerator_GetCurrentFontFile(e, &ff)); CHECK(ff != NULL); DW_RELEASE(ff);
        CHECK_HR_OK(IDWriteFontFileEnumerator_MoveNext(e, &has)); EXPECT_EQ(has, 1);
        CHECK_HR_OK(IDWriteFontFileEnumerator_MoveNext(e, &has)); EXPECT_EQ(has, 0);
        CHECK_HR(IDWriteFontFileEnumerator_GetCurrentFontFile(e, &ff), E_FAIL);       /* past the end */
        CHECK_OUT_NULL(ff);
        IDWriteFontFileEnumerator_Release(e);
    }
    /* control: the loader interface counted the direct call too */
    EXPECT_EQ(X.cl->n_create, 1);
    CHECK_HR_OK(dwx_coll_create(X.cl, X.f, &c));
    EXPECT_EQ(X.cl->n_create, 2);
    DW_RELEASE(c); efx_close(&X);
}
