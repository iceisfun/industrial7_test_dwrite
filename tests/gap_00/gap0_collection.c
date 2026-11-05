/* IDWriteFontCollectionLoader::CreateEnumeratorFromKey, IDWriteFontFileEnumerator::MoveNext / GetCurrentFontFile as driven by
 * IDWriteFactory::CreateCustomFontCollection (Windows 10 build 19041, FontCollectionConstructionTask / FontCollectionBuilder::AddFontFiles):
 *
 *   loader->CreateEnumeratorFromKey(factory, key, keySize, &e);   once per collection construction, on the calling thread
 *   loop { e->MoveNext(&has); if (!has) break; e->GetCurrentFontFile(&file); file->Release(); }
 *   e->Release();
 * MoveNext is therefore called (files + 1) times and GetCurrentFontFile (files) times; the loader sees the application's key bytes verbatim.
 * No reference to the enumerator, the files or the loader is kept by a collection that was built from them. */
#include "gap0.h"

typedef struct gc_fx {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; int ok;
} gc_fx;
static int gc_open(gc_fx *x, int reg)
{
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    x->fl = dwx_loader_new(); x->cl = dwx_coll_new(x->fl);
    x->ok = 1;
    if (reg) {
        if (FAILED(dwx_loader_register(x->fl, x->f)) || FAILED(dwx_coll_register(x->cl, x->f))) { t_skip("loader registration failed"); return 0; }
    }
    return 1;
}
static void gc_close(gc_fx *x)
{
    if (!x->ok) return;
    dwx_coll_unregister(x->cl); dwx_loader_unregister(x->fl);
    dwx_release(x->cl); dwx_release(x->fl); DW_RELEASE(x->f);
    x->ok = 0;
}
#define GC_OPEN(x) gc_fx x; if (!gc_open(&x, 1)) { gc_close(&x); return; }
/* Adds one single-style font family to the file loader and to the enumeration. */
static void gc_add(gc_fx *x, int i)
{
    ttf_spec s; size_t n; uint8_t *b; char fam[32];
    snprintf(fam, sizeof fam, "Gap0 Family %02d", i);
    ttf_style_spec(&s, fam, 400, 5, 0);
    b = ttf_build(&s, &n);
    dwx_coll_add_key(x->cl, dwx_loader_add(x->fl, b, n));
    free(b);
}
static HRESULT gc_create(gc_fx *x, const void *key, UINT32 ks, IDWriteFontCollection **c)
{
    return IDWriteFactory_CreateCustomFontCollection(x->f, (IDWriteFontCollectionLoader *)x->cl, key, ks, c);
}

TEST(gap0_coll_call_counts_per_file_count)
{
    int n;
    for (n = 0; n <= 8; n++) {
        GC_OPEN(x);
        IDWriteFontCollection *c = NULL; int i;
        for (i = 0; i < n; i++) gc_add(&x, i);
        CHECK_HR(gc_create(&x, "gap0", 4, &c), S_OK);
        CHECK(x.cl->n_create == 1);
        if (x.cl->last_enum) {
            CHECKF(x.cl->last_enum->n_movenext == n + 1, "%d files: MoveNext x%ld", n, x.cl->last_enum->n_movenext);
            CHECKF(x.cl->last_enum->n_getcurrent == n, "%d files: GetCurrentFontFile x%ld", n, x.cl->last_enum->n_getcurrent);
            CHECK(dwx_destroyed(x.cl->last_enum));                    /* the enumerator is released before CreateCustomFontCollection returns */
        } else CHECK(0);
        if (c) CHECKF(IDWriteFontCollection_GetFontFamilyCount(c) == (UINT32)n, "%d files: %u families", n, IDWriteFontCollection_GetFontFamilyCount(c));
        DW_RELEASE(c);
        gc_close(&x);
    }
}
TEST(gap0_coll_factory_argument_is_the_calling_factory)
{
    GC_OPEN(x);
    IDWriteFontCollection *c = NULL; IDWriteFactory3 *f3 = NULL;
    gc_add(&x, 0);
    CHECK_HR(gc_create(&x, "gap0", 4, &c), S_OK);
    CHECK(x.cl->last_factory != NULL && dw_same_identity((IUnknown *)x.cl->last_factory, (IUnknown *)x.f));
    DW_RELEASE(c);
    /* the same factory reached through a newer interface: identical object identity */
    if (SUCCEEDED(IDWriteFactory_QueryInterface(x.f, &IID_IDWriteFactory3, (void **)&f3)) && f3) {
        char key[8] = "gap0b";
        CHECK_HR(IDWriteFactory3_CreateCustomFontCollection(f3, (IDWriteFontCollectionLoader *)x.cl, key, 6, &c), S_OK);
        CHECK(x.cl->n_create == 2);
        CHECK(dw_same_identity((IUnknown *)x.cl->last_factory, (IUnknown *)x.f));
        DW_RELEASE(c); DW_RELEASE(f3);
    }
    gc_close(&x);
}
TEST(gap0_coll_key_bytes_reach_the_loader)
{
    static const UINT32 sizes[] = { 1, 2, 3, 4, 5, 7, 8, 15, 16, 31, 32, 33, 63, 64 };
    int i;
    GC_OPEN(x);
    gc_add(&x, 0);
    for (i = 0; i < G_N(sizes); i++) {
        uint8_t key[64]; IDWriteFontCollection *c = NULL; UINT32 j;
        for (j = 0; j < sizes[i]; j++) key[j] = (uint8_t)(0xA0 + i * 7 + j * 13);
        key[0] = (uint8_t)(i + 1);                                   /* every key differs, so the factory's cache cannot answer */
        CHECK_HR(gc_create(&x, key, sizes[i], &c), S_OK);
        CHECKF(x.cl->n_create == i + 1, "size %u: CreateEnumeratorFromKey x%ld", sizes[i], x.cl->n_create);
        CHECKF(x.cl->last_key_size == sizes[i], "size %u: loader got size %u", sizes[i], x.cl->last_key_size);
        CHECKF(!memcmp(x.cl->last_key, key, sizes[i]), "size %u: key bytes differ", sizes[i]);
        DW_RELEASE(c);
    }
    gc_close(&x);
}
TEST(gap0_coll_key_buffer_not_needed_after_return)
{
    GC_OPEN(x);
    IDWriteFontCollection *c = NULL; char *key = (char *)malloc(16);
    gc_add(&x, 0);
    memcpy(key, "heapkey-0123456", 16);
    CHECK_HR(gc_create(&x, key, 16, &c), S_OK);
    memset(key, 0xCC, 16); free(key);               /* the application may free its key as soon as the call returns */
    CHECK(c != NULL && IDWriteFontCollection_GetFontFamilyCount(c) == 1);
    if (c) {
        IDWriteFontFamily *fam = NULL;
        CHECK_HR(IDWriteFontCollection_GetFontFamily(c, 0, &fam), S_OK);
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    gc_close(&x);
}
TEST(gap0_coll_enumeration_is_synchronous)
{
    GC_OPEN(x);
    IDWriteFontCollection *c = NULL; int i;
    for (i = 0; i < 3; i++) gc_add(&x, i);
    CHECK_HR(gc_create(&x, "sync", 4, &c), S_OK);
    /* by the time CreateCustomFontCollection returns the enumeration is complete and no callback happens later */
    {
        LONG cr = x.cl->n_create, mv = x.cl->last_enum ? x.cl->last_enum->n_movenext : -1, gc = x.cl->last_enum ? x.cl->last_enum->n_getcurrent : -1;
        Sleep(50);
        if (c) { UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c); CHECK(n == 3); }
        CHECK(x.cl->n_create == cr);
        CHECK(x.cl->last_enum && x.cl->last_enum->n_movenext == mv && x.cl->last_enum->n_getcurrent == gc);
    }
    DW_RELEASE(c);
    gc_close(&x);
}
TEST(gap0_coll_loader_balanced_after_collection_release)
{
    GC_OPEN(x);
    IDWriteFontCollection *c = NULL; int i; LONG r0 = dwx_refs(x.cl);
    for (i = 0; i < 4; i++) gc_add(&x, i);
    CHECK_HR(gc_create(&x, "bal", 3, &c), S_OK);
    DW_RELEASE(c);
    CHECKF(dwx_refs(x.cl) == r0, "collection loader refs %ld -> %ld (registration still holds its reference)", (long)r0, (long)dwx_refs(x.cl));
    CHECK(x.cl->last_enum == NULL || dwx_destroyed(x.cl->last_enum));
    CHECK_HR(dwx_coll_unregister(x.cl), S_OK);
    CHECKF(dwx_refs(x.cl) == 1, "after UnregisterFontCollectionLoader the application owns the only reference (%ld)", (long)dwx_refs(x.cl));
    gc_close(&x);
}
TEST(gap0_coll_two_loaders_independent)
{
    GC_OPEN(x);
    dwx_mem_loader *fl2 = dwx_loader_new(); dwx_coll_loader *cl2 = dwx_coll_new(fl2);
    IDWriteFontCollection *c1 = NULL, *c2 = NULL; ttf_spec s; size_t n; uint8_t *b;
    gc_add(&x, 0);
    ttf_style_spec(&s, "Gap0 Other A", 400, 5, 0); b = ttf_build(&s, &n); dwx_coll_add_key(cl2, dwx_loader_add(fl2, b, n)); free(b);
    ttf_style_spec(&s, "Gap0 Other B", 400, 5, 0); b = ttf_build(&s, &n); dwx_coll_add_key(cl2, dwx_loader_add(fl2, b, n)); free(b);
    if (SUCCEEDED(dwx_loader_register(fl2, x.f)) && SUCCEEDED(dwx_coll_register(cl2, x.f))) {
        CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, (IDWriteFontCollectionLoader *)cl2, "two", 3, &c2), S_OK);
        CHECK_HR(gc_create(&x, "one", 3, &c1), S_OK);
        CHECK(x.cl->n_create == 1 && cl2->n_create == 1);
        if (c1) CHECK(IDWriteFontCollection_GetFontFamilyCount(c1) == 1);
        if (c2) CHECK(IDWriteFontCollection_GetFontFamilyCount(c2) == 2);
        CHECK(cl2->last_key_size == 3 && !memcmp(cl2->last_key, "two", 3));
        CHECK(x.cl->last_key_size == 3 && !memcmp(x.cl->last_key, "one", 3));
    }
    DW_RELEASE(c1); DW_RELEASE(c2);
    dwx_coll_unregister(cl2); dwx_loader_unregister(fl2); dwx_release(cl2); dwx_release(fl2);
    gc_close(&x);
}
TEST(gap0_coll_enumerator_stops_at_first_false)
{
    GC_OPEN(x);
    IDWriteFontCollection *c = NULL; int i;
    for (i = 0; i < 5; i++) gc_add(&x, i);
    CHECK_HR(gc_create(&x, "stop", 4, &c), S_OK);
    /* MoveNext is not called again after it reported FALSE */
    CHECK(x.cl->last_enum && x.cl->last_enum->n_movenext == 6);
    DW_RELEASE(c);
    gc_close(&x);
}
TEST(gap0_coll_one_family_per_enumerated_file)
{
    GC_OPEN(x);
    IDWriteFontCollection *c = NULL; int i; static const char *const names[] = { "Zeta Gap0", "Alpha Gap0", "Mid Gap0" };
    for (i = 0; i < 3; i++) {
        ttf_spec s; size_t n; uint8_t *b;
        ttf_style_spec(&s, names[i], 400, 5, 0); b = ttf_build(&s, &n); dwx_coll_add_key(x.cl, dwx_loader_add(x.fl, b, n)); free(b);
    }
    CHECK_HR(gc_create(&x, "ord", 3, &c), S_OK);
    if (c) {
        /* every file yields one family; the family order itself is only noted */
        char got[3][64]; UINT32 k;
        CHECK(IDWriteFontCollection_GetFontFamilyCount(c) == 3);
        for (k = 0; k < 3; k++) {
            IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *ls = NULL; got[k][0] = 0;
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, k, &fam)) && fam && SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &ls)) && ls) dw_ls_en(ls, got[k], sizeof got[k]);
            DW_RELEASE(ls); DW_RELEASE(fam);
        }
        {
            int has_a = 0, has_m = 0, has_z = 0;
            for (k = 0; k < 3; k++) { has_a |= !strcmp(got[k], "Alpha Gap0"); has_m |= !strcmp(got[k], "Mid Gap0"); has_z |= !strcmp(got[k], "Zeta Gap0"); }
            CHECKF(has_a && has_m && has_z, "families: %s / %s / %s", got[0], got[1], got[2]);       /* every enumerated file became exactly one family */
            t_note("family order for enumeration Zeta, Alpha, Mid: %s / %s / %s", got[0], got[1], got[2]);
        }
    }
    DW_RELEASE(c);
    gc_close(&x);
}
TEST(gap0_coll_enumerator_failures_table)
{
    static const HRESULT codes[] = { E_FAIL, E_INVALIDARG, E_ACCESSDENIED, E_NOTIMPL, E_UNEXPECTED, (HRESULT)0x88985004, (HRESULT)0x88985003, (HRESULT)0x80ABCDEF };
    int i;
    for (i = 0; i < G_N(codes); i++) {
        GC_OPEN(x);
        IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1; HRESULT hr; int j;
        for (j = 0; j < 3; j++) gc_add(&x, j);
        x.cl->fail_create = codes[i];
        hr = gc_create(&x, "fail", 4, &c);
        t_check(hr == codes[i], __FILE__, __LINE__, "CreateEnumeratorFromKey failure propagates", "%s -> %s", dw_hr_name(codes[i]), dw_hr_name(hr));
        CHECK_OUT_NULL(c);
        x.cl->fail_create = 0;
        c = NULL;
        CHECK_HR(gc_create(&x, "fail2", 5, &c), S_OK);      /* the loader recovers */
        DW_RELEASE(c);
        gc_close(&x);
    }
}
TEST(gap0_coll_movenext_failures_table)
{
    static const HRESULT codes[] = { E_FAIL, E_INVALIDARG, E_OUTOFMEMORY, (HRESULT)0x88985004, (HRESULT)0x80ABCDEF };
    int i;
    for (i = 0; i < G_N(codes); i++) {
            GC_OPEN(x);
            IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1; HRESULT hr; int j;
            for (j = 0; j < 3; j++) gc_add(&x, j);
            x.cl->enum_fail_move = codes[i];
            /* the double fails every MoveNext; the first call is the one that matters */
            hr = gc_create(&x, "mfail", 5, &c);
            t_check(hr == codes[i], __FILE__, __LINE__, "MoveNext failure propagates", "%s -> %s", dw_hr_name(codes[i]), dw_hr_name(hr));
            CHECK_OUT_NULL(c);
            CHECK(x.cl->last_enum && x.cl->last_enum->n_movenext == 1 && x.cl->last_enum->n_getcurrent == 0);
            CHECK(x.cl->last_enum && dwx_destroyed(x.cl->last_enum));
            gc_close(&x);
        }
}
TEST(gap0_coll_getcurrent_failure_skips_the_file)
{
    GC_OPEN(x);
    IDWriteFontCollection *c = NULL; int j;
    for (j = 0; j < 3; j++) gc_add(&x, j);
    x.cl->enum_fail_get = E_FAIL;
    CHECK_HR(gc_create(&x, "gfail", 5, &c), S_OK);       /* every GetCurrentFontFile fails: all files are skipped, the collection is empty */
    CHECK(x.cl->last_enum && x.cl->last_enum->n_movenext == 4 && x.cl->last_enum->n_getcurrent == 3);
    if (c) CHECK(IDWriteFontCollection_GetFontFamilyCount(c) == 0);
    DW_RELEASE(c);
    gc_close(&x);
}
TEST(gap0_coll_no_leaked_doubles)
{
    DWX_LEAK_BEGIN(lk);
    {
        GC_OPEN(x);
        IDWriteFontCollection *c = NULL; int j;
        for (j = 0; j < 6; j++) gc_add(&x, j);
        CHECK_HR(gc_create(&x, "leak", 4, &c), S_OK);
        if (c) {
            IDWriteFontFamily *fam = NULL;
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) && fam) {
                IDWriteFont *fnt = NULL; IDWriteFontFace *face = NULL;
                if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &fnt)) && fnt) IDWriteFont_CreateFontFace(fnt, &face);
                DW_RELEASE(face); DW_RELEASE(fnt);
            }
            DW_RELEASE(fam);
        }
        DW_RELEASE(c);
        gc_close(&x);
    }
    /* the loaders themselves were released by gc_close; the streams and files dwrite used went away with the collection */
    DWX_LEAK_END(lk);
}
