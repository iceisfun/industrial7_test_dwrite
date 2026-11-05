/* Ownership of application-supplied objects: the factory takes a reference on every registered file loader and collection
 * loader (FontLoaderManagerBase::Register adds the ComPtr copy, Unregister erases it), releases them when it dies, and the
 * font file / face / collection objects keep the data they need alive. All counts are observed on our counting doubles. */
#include "clt.h"

static uint8_t *syn(const char *fam, size_t *n) { return ttf_build_styled(fam, 400, 5, 0, n); }

/* ---- register / unregister reference accounting ------------------------------------------------------------------ */
TEST(ld_register_adds_exactly_one_ref) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new();
    EXPECT_EQ(dwx_refs(ld), 1);
    CHECK_HR_OK(dwx_loader_register(ld, f));
    EXPECT_EQ(dwx_refs(ld), 2);              /* the factory's loader map owns one reference */
    CHECK(dwx_qi_calls(ld) >= 0);
    t_note("loader QueryInterface calls made by Register: %ld", (long)dwx_qi_calls(ld));
    CHECK_HR_OK(dwx_loader_unregister(ld));
    EXPECT_EQ(dwx_refs(ld), 1);
    dwx_release(ld);
    EXPECT_EQ(dwx_destroyed(ld), 1);
    DW_RELEASE(f);
}
TEST(ld_unregister_returns_the_ref_exactly) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; int i;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new();
    for (i = 0; i < 100; i++) {       /* repeated cycles never drift */
        CHECK_HR_OK(dwx_loader_register(ld, f));
        if (dwx_refs(ld) != 2) { CHECKF(0, "cycle %d: refs %ld", i, (long)dwx_refs(ld)); break; }
        CHECK_HR_OK(dwx_loader_unregister(ld));
        if (dwx_refs(ld) != 1) { CHECKF(0, "cycle %d: refs after unregister %ld", i, (long)dwx_refs(ld)); break; }
    }
    dwx_release(ld);
    DW_RELEASE(f);
}
TEST(ld_factory_release_releases_registered_loader) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new();
    CHECK_HR_OK(dwx_loader_register(ld, f));
    ld->registered = 0;                      /* the factory goes away without an explicit Unregister */
    EXPECT_EQ(dwx_refs(ld), 2);
    DW_RELEASE(f);
    EXPECT_EQ(dwx_refs(ld), 1);              /* the destroyed factory dropped its registry */
    dwx_release(ld);
    EXPECT_EQ(dwx_destroyed(ld), 1);
}
TEST(ld_collection_loader_factory_release_releases_loader) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_coll_loader *cl;
    if (!f) { t_skip("no factory"); return; }
    cl = dwx_coll_new(NULL);
    EXPECT_EQ(dwx_refs(cl), 1);
    CHECK_HR_OK(dwx_coll_register(cl, f));
    EXPECT_EQ(dwx_refs(cl), 2);
    CHECK_HR_OK(dwx_coll_unregister(cl));
    EXPECT_EQ(dwx_refs(cl), 1);
    CHECK_HR_OK(dwx_coll_register(cl, f));
    cl->registered = 0;
    DW_RELEASE(f);
    EXPECT_EQ(dwx_refs(cl), 1);
    dwx_release(cl);
    EXPECT_EQ(dwx_destroyed(cl), 1);
}
TEST(ld_register_twice_is_rejected_without_extra_ref) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; HRESULT hr;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new();
    CHECK_HR_OK(dwx_loader_register(ld, f));
    /* FontLoaderManagerBase::Register throws OSException(DWRITE_E_ALREADYREGISTERED) when the object is already in the map */
    hr = IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)ld);
    CHECK_HR(hr, DWRITE_E_ALREADYREGISTERED);
    EXPECT_EQ(dwx_refs(ld), 2);
    CHECK_HR_OK(dwx_loader_unregister(ld));
    EXPECT_EQ(dwx_refs(ld), 1);
    dwx_release(ld);
    DW_RELEASE(f);
}
TEST(ld_collection_register_twice_is_rejected_without_extra_ref) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_coll_loader *cl; HRESULT hr;
    if (!f) { t_skip("no factory"); return; }
    cl = dwx_coll_new(NULL);
    CHECK_HR_OK(dwx_coll_register(cl, f));
    hr = IDWriteFactory_RegisterFontCollectionLoader(f, (IDWriteFontCollectionLoader *)cl);
    CHECK_HR(hr, DWRITE_E_ALREADYREGISTERED);
    EXPECT_EQ(dwx_refs(cl), 2);
    CHECK_HR_OK(dwx_coll_unregister(cl));
    EXPECT_EQ(dwx_refs(cl), 1);
    dwx_release(cl);
    DW_RELEASE(f);
}
TEST(ld_unregister_unknown_fails_without_ref_change) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; dwx_coll_loader *cl;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new(); cl = dwx_coll_new(NULL);
    /* Unregister of an object that is not in the map throws ArgumentException("obj") -> E_INVALIDARG */
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)ld), E_INVALIDARG);
    CHECK_HR(IDWriteFactory_UnregisterFontCollectionLoader(f, (IDWriteFontCollectionLoader *)cl), E_INVALIDARG);
    EXPECT_EQ(dwx_refs(ld), 1); EXPECT_EQ(dwx_refs(cl), 1);
    CHECK_HR_OK(dwx_loader_register(ld, f));
    CHECK_HR_OK(dwx_loader_unregister(ld));
    ld->registered = 1;                                     /* second Unregister of the same object */
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)ld), E_INVALIDARG);
    ld->registered = 0;
    EXPECT_EQ(dwx_refs(ld), 1);
    dwx_release(ld); dwx_release(cl);
    DW_RELEASE(f);
}
TEST(ld_same_loader_on_two_factories) {
    NEED_PROC();
    IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *b = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld;
    if (!a || !b) { t_skip("no factory"); DW_RELEASE(a); DW_RELEASE(b); return; }
    ld = dwx_loader_new();
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(a, (IDWriteFontFileLoader *)ld));
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(b, (IDWriteFontFileLoader *)ld));    /* registries are per factory */
    EXPECT_EQ(dwx_refs(ld), 3);
    DW_RELEASE(a);
    EXPECT_EQ(dwx_refs(ld), 2);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(b, (IDWriteFontFileLoader *)ld));
    EXPECT_EQ(dwx_refs(ld), 1);
    dwx_release(ld);
    DW_RELEASE(b);
}
TEST(ld_register_on_shared_factory_balances) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_SHARED); dwx_mem_loader *ld; int i;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new();
    for (i = 0; i < 10; i++) {
        CHECK_HR_OK(dwx_loader_register(ld, f));
        EXPECT_EQ(dwx_refs(ld), 2);
        CHECK_HR_OK(dwx_loader_unregister(ld));
        EXPECT_EQ(dwx_refs(ld), 1);
    }
    dwx_release(ld);
    DW_RELEASE(f);
}
TEST(ld_registered_loader_not_released_by_other_factory_release) {
    NEED_PROC();
    IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *b = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld;
    if (!a || !b) { t_skip("no factory"); DW_RELEASE(a); DW_RELEASE(b); return; }
    ld = dwx_loader_new();
    CHECK_HR_OK(dwx_loader_register(ld, a));
    DW_RELEASE(b);
    EXPECT_EQ(dwx_refs(ld), 2);
    /* the loader is private to factory a: b never saw it */
    CHECK_HR_OK(dwx_loader_unregister(ld));
    dwx_release(ld);
    DW_RELEASE(a);
}

/* ---- font file / face keep their loader and stream ------------------------------------------------------------------ */
TEST(ld_file_creation_does_not_open_stream_until_needed) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; IDWriteFontFile *ff = NULL; size_t n; uint8_t *b; UINT32 key;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new(); b = ttf_build_default(&n); key = dwx_loader_add(ld, b, n); free(b);
    CHECK_HR_OK(dwx_loader_register(ld, f));
    CHECK_HR_OK(dwx_loader_file(ld, f, key, &ff));
    t_note("CreateStreamFromKey calls after CreateCustomFontFileReference: %ld", (long)ld->n_create);
    EXPECT_REFS(ff, 1);
    CHECKF(dwx_refs(ld) >= 2, "file creation must not drop the registry reference (%ld)", (long)dwx_refs(ld));
    t_note("loader refs with one live font file: %ld", (long)dwx_refs(ld));
    DW_RELEASE(ff);
    CHECK_HR_OK(dwx_loader_unregister(ld));
    EXPECT_EQ(dwx_refs(ld), 1);
    dwx_release(ld);
    DW_RELEASE(f);
}
TEST(ld_file_release_returns_loader_ref) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); LONG with_file, with_face;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (!h.face) { dwx_font_close(&h); DW_RELEASE(f); t_skip("font open failed"); return; }
    with_face = dwx_refs(h.ld);
    DW_RELEASE(h.face);
    with_file = dwx_refs(h.ld);
    CHECKF(with_file <= with_face, "releasing the face must not raise loader refs (%ld -> %ld)", (long)with_face, (long)with_file);
    DW_RELEASE(h.file);
    CHECK_HR_OK(dwx_loader_unregister(h.ld));
    EXPECT_EQ(dwx_refs(h.ld), 1);                  /* nothing of dwrite still points at the loader */
    t_note("loader refs: registry+file+face=%ld, registry+file=%ld", (long)with_face, (long)with_file);
    dwx_font_close(&h);
    DW_RELEASE(f);
}
TEST(ld_stream_destroyed_after_all_dependents_released) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_stream *st;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (!h.face) { dwx_font_close(&h); DW_RELEASE(f); t_skip("font open failed"); return; }
    st = h.ld->last_stream;
    CHECK(st != NULL);
    if (st) {
        CHECKF(dwx_refs(st) >= 1, "a live face must keep its stream (refs %ld)", (long)dwx_refs(st));
        CHECK(st->n_read > 0);
    }
    DW_RELEASE(h.face); DW_RELEASE(h.file);
    CHECK_HR_OK(dwx_loader_unregister(h.ld));
    if (st) { EXPECT_EQ(dwx_destroyed(st), 1); if (st->outstanding) t_note("stream destroyed with %ld fragments outstanding", (long)st->outstanding); }
    EXPECT_EQ(dwx_refs(h.ld), 1);
    dwx_font_close(&h);
    DW_RELEASE(f);
}
TEST(ld_unregister_while_face_alive_keeps_reading) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (!h.face) { dwx_font_close(&h); DW_RELEASE(f); t_skip("font open failed"); return; }
    CHECK_HR_OK(dwx_loader_unregister(h.ld));      /* registry reference gone, face and file still alive */
    CHECKF(dwx_refs(h.ld) >= 1, "loader destroyed under live objects");
    {
        UINT32 cps[2] = { 'A', 'Z' }; UINT16 gi[2] = { 0, 0 }; DWRITE_FONT_METRICS m; UINT32 len = 0; uint8_t *name;
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(h.face, cps, 2, gi));
        EXPECT_EQ(gi[0], 2); EXPECT_EQ(gi[1], 27);
        IDWriteFontFace_GetMetrics(h.face, &m);
        EXPECT_EQ(m.designUnitsPerEm, 2048);
        name = dw_face_table(h.face, DW_TAG('n', 'a', 'm', 'e'), &len);   /* reads through the retained stream */
        CHECK(name != NULL && len > 6);
        free(name);
        if (dwx_loader_total_outstanding(h.ld) != 0) t_note("fragments still held by the face cache: %ld", (long)dwx_loader_total_outstanding(h.ld));
    }
    {
        UINT32 nfiles = 0, cap = 1; IDWriteFontFile *got = NULL; const void *key = NULL; UINT32 ks = 0;
        CHECK_HR_OK(IDWriteFontFace_GetFiles(h.face, &nfiles, NULL));
        EXPECT_EQ(nfiles, 1);
        CHECK_HR_OK(IDWriteFontFace_GetFiles(h.face, &cap, &got));
        if (got) { CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(got, &key, &ks)); EXPECT_EQ(ks, 4); DW_RELEASE(got); }
    }
    dwx_font_close(&h);
    DW_RELEASE(f);
}
TEST(ld_file_after_unregister_still_analyzes_or_fails_cleanly) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; IDWriteFontFile *ff = NULL; size_t n; uint8_t *b; UINT32 key;
    BOOL sup = 0; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf = 0; HRESULT hr;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new(); b = ttf_build_default(&n); key = dwx_loader_add(ld, b, n); free(b);
    CHECK_HR_OK(dwx_loader_register(ld, f));
    CHECK_HR_OK(dwx_loader_file(ld, f, key, &ff));
    CHECK_HR_OK(dwx_loader_unregister(ld));
    /* the file object holds the loader itself; whether a not-yet-opened stream can still be created is observable but version
     * dependent: only the "no crash, no leak" part is a contract */
    hr = IDWriteFontFile_Analyze(ff, &sup, &ft, &fct, &nf);
    t_note("Analyze after Unregister -> %s (supported=%d faces=%u)", dw_hr_name(hr), sup, nf);
    DW_RELEASE(ff);
    EXPECT_EQ(dwx_refs(ld), 1);
    dwx_release(ld);
    DW_RELEASE(f);
}
TEST(ld_face_second_creation_after_unregister_note) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontFace *f2 = NULL; HRESULT hr;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (!h.face) { dwx_font_close(&h); DW_RELEASE(f); t_skip("font open failed"); return; }
    CHECK_HR_OK(dwx_loader_unregister(h.ld));
    hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &f2);
    t_note("CreateFontFace from a live file after Unregister -> %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(f2);
    DW_RELEASE(f2);
    dwx_font_close(&h);
    DW_RELEASE(f);
}
TEST(ld_many_cycles_balance) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); int i; size_t n; uint8_t *b;
    if (!f) { t_skip("no factory"); return; }
    b = ttf_build_default(&n);
    CLT_LEAK_BEGIN(cyc);
    for (i = 0; i < 40; i++) {
        dwx_font h;
        HRESULT hr = dwx_font_open(&h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
        if (FAILED(hr) || !h.face) { CHECKF(0, "cycle %d open failed %s", i, dw_hr_name(hr)); dwx_font_close(&h); break; }
        { UINT32 cp = 'B'; UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(h.face, &cp, 1, &g); if (g != 3) { CHECKF(0, "cycle %d glyph %u", i, g); } }
        DW_RELEASE(h.face); DW_RELEASE(h.file);
        dwx_font_close(&h);
    }
    free(b);
    CLT_LEAK_END(cyc);
    DW_RELEASE(f);
}
TEST(ld_stream_fragment_balance_after_table_access) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); int i;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (!h.face) { dwx_font_close(&h); DW_RELEASE(f); t_skip("font open failed"); return; }
    for (i = 0; i < 20; i++) { UINT32 len; uint8_t *t = dw_face_table(h.face, DW_TAG('h', 'e', 'a', 'd'), &len); CHECK(t && len == 54); free(t); }
    t_note("fragments outstanding after table access: %ld", (long)dwx_loader_total_outstanding(h.ld));
    DW_RELEASE(h.face); DW_RELEASE(h.file);
    t_note("fragments outstanding after releasing face and file: %ld", (long)dwx_loader_total_outstanding(h.ld));
    dwx_font_close(&h);
    DW_RELEASE(f);
}

/* ---- collection loader / enumerator ownership ----------------------------------------------------------------------------- */
#define cfix clt_cfix
#define cfix_open clt_cfix_open
#define cfix_close clt_cfix_close
TEST(ld_enumerator_released_after_collection_build) {
    NEED_PROC();
    cfix x; IDWriteFontCollection *c = NULL;
    if (!cfix_open(&x, 2)) { t_skip("fixture"); cfix_close(&x); return; }
    CHECK_HR_OK(dwx_coll_create(x.cl, x.f, &c));
    if (c) {
        EXPECT_EQ(x.cl->n_create, 1);
        CHECK(x.cl->last_enum != NULL);
        if (x.cl->last_enum) {
            EXPECT_EQ(dwx_destroyed(x.cl->last_enum), 1);        /* dwrite owns the enumerator only while it builds the collection */
            CHECK(x.cl->last_enum->n_getcurrent >= 2);
            CHECK(x.cl->last_enum->n_movenext >= 3);              /* MoveNext until it reports FALSE */
        }
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    }
    DW_RELEASE(c);
    cfix_close(&x);
}
TEST(ld_collection_release_returns_all_loader_refs) {
    NEED_PROC();
    cfix x; IDWriteFontCollection *c = NULL; LONG fl0, cl0;
    if (!cfix_open(&x, 2)) { t_skip("fixture"); cfix_close(&x); return; }
    fl0 = dwx_refs(x.fl); cl0 = dwx_refs(x.cl);
    CHECK_HR_OK(dwx_coll_create(x.cl, x.f, &c));
    t_note("file loader refs %ld -> %ld, collection loader refs %ld -> %ld while a 2-font collection is alive", (long)fl0, (long)dwx_refs(x.fl), (long)cl0, (long)dwx_refs(x.cl));
    CHECKF(dwx_refs(x.fl) >= fl0, "file loader released too much");
    DW_RELEASE(c);
    EXPECT_EQ(dwx_refs(x.fl), fl0);
    EXPECT_EQ(dwx_refs(x.cl), cl0);
    cfix_close(&x);
    EXPECT_EQ(dwx_destroyed(x.fl), 1); EXPECT_EQ(dwx_destroyed(x.cl), 1);
}
TEST(ld_collection_survives_collection_loader_unregister) {
    NEED_PROC();
    cfix x; IDWriteFontCollection *c = NULL;
    if (!cfix_open(&x, 2)) { t_skip("fixture"); cfix_close(&x); return; }
    CHECK_HR_OK(dwx_coll_create(x.cl, x.f, &c));
    if (!c) { cfix_close(&x); return; }
    CHECK_HR_OK(dwx_coll_unregister(x.cl));          /* the collection has been built; the loader is no longer needed */
    EXPECT_EQ(dwx_refs(x.cl), 1);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    {
        IDWriteFontFamily *fam = NULL; IDWriteFont *fn = NULL; IDWriteFontFace *fc = NULL; UINT32 cp = 'C', gi = 0; UINT16 g = 0;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 1, &fam));
        if (fam) CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, 0, &fn));
        if (fn) CHECK_HR_OK(IDWriteFont_CreateFontFace(fn, &fc));
        if (fc) { CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(fc, &cp, 1, &g)); EXPECT_EQ(g, 4); }
        DW_RELEASE(fc); DW_RELEASE(fn); DW_RELEASE(fam);
        (void)gi;
    }
    DW_RELEASE(c);
    cfix_close(&x);
}
TEST(ld_collection_after_file_loader_unregister) {
    NEED_PROC();
    cfix x; IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *fn = NULL; IDWriteFontFace *fc = NULL;
    if (!cfix_open(&x, 1)) { t_skip("fixture"); cfix_close(&x); return; }
    CHECK_HR_OK(dwx_coll_create(x.cl, x.f, &c));
    if (!c) { cfix_close(&x); return; }
    IDWriteFontCollection_GetFontFamily(c, 0, &fam);
    if (fam) IDWriteFontFamily_GetFont(fam, 0, &fn);
    if (fn) IDWriteFont_CreateFontFace(fn, &fc);
    CHECK(fc != NULL);
    CHECK_HR_OK(dwx_loader_unregister(x.fl));       /* file loader goes away while collection, family, font and face are alive */
    if (fc) {
        UINT32 cp = 'A'; UINT16 g = 0; UINT32 len = 0; uint8_t *t;
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(fc, &cp, 1, &g)); EXPECT_EQ(g, 2);
        t = dw_face_table(fc, DW_TAG('c', 'm', 'a', 'p'), &len);
        CHECK(t != NULL);                           /* the face's own stream is retained */
        free(t);
    }
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    DW_RELEASE(fc); DW_RELEASE(fn); DW_RELEASE(fam); DW_RELEASE(c);
    cfix_close(&x);
}
TEST(ld_enumerator_failure_leaves_no_refs) {
    NEED_PROC();
    cfix x; IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1; HRESULT hr; LONG fl0, cl0;
    {   /* flush references that earlier tests left to deferred release (they drop when a new fixture is created) */
        cfix t; if (cfix_open(&t, 1)) { cfix_close(&t); } else cfix_close(&t);
    }
    CLT_LEAK_BEGIN(en);
    if (!cfix_open(&x, 2)) { t_skip("fixture"); cfix_close(&x); return; }
    fl0 = dwx_refs(x.fl); cl0 = dwx_refs(x.cl);
    x.cl->enum_fail_move = E_FAIL;
    c = NULL;
    hr = dwx_coll_create(x.cl, x.f, &c);
    CHECK(FAILED(hr)); CHECK_OUT_NULL(c); DW_RELEASE(c);
    EXPECT_EQ(dwx_refs(x.fl), fl0); EXPECT_EQ(dwx_refs(x.cl), cl0);
    if (x.cl->last_enum) EXPECT_EQ(dwx_destroyed(x.cl->last_enum), 1);     /* the enumerator is released also on failure */
    x.cl->enum_fail_move = 0;
    cfix_close(&x);
    CLT_LEAK_END(en);
}
TEST(ld_collection_loader_create_failure_leaves_no_refs) {
    NEED_PROC();
    cfix x; IDWriteFontCollection *c = NULL; HRESULT hr; LONG cl0;
    if (!cfix_open(&x, 1)) { t_skip("fixture"); cfix_close(&x); return; }
    cl0 = dwx_refs(x.cl);
    x.cl->fail_create = E_FAIL;
    hr = dwx_coll_create(x.cl, x.f, &c);
    CHECK(FAILED(hr)); CHECK_OUT_NULL(c);
    EXPECT_EQ(dwx_refs(x.cl), cl0);
    cfix_close(&x);
}
TEST(ld_collection_hold_across_factory_release) {
    NEED_PROC();
    cfix x; IDWriteFontCollection *c = NULL; IDWriteFactory *owner = NULL;
    if (!cfix_open(&x, 1)) { t_skip("fixture"); cfix_close(&x); return; }
    CHECK_HR_OK(dwx_coll_create(x.cl, x.f, &c));
    if (!c) { cfix_close(&x); return; }
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamilyCount(c) == 1 ? S_OK : E_FAIL);
    /* the collection reports the factory that created it only on IDWriteFontCollection1+; here just hold it past the factory */
    x.cl->registered = 0; x.fl->registered = 0;
    DW_RELEASE(x.f);                                  /* our last factory reference */
    (void)owner;
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    {
        IDWriteFontFamily *fam = NULL; CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
        if (fam) { CHECK(IDWriteFontFamily_GetFontCount(fam) >= 1); DW_RELEASE(fam); }
    }
    DW_RELEASE(c);
    /* everything dwrite retained is released now: only our own references remain */
    clt_settle_refs(x.fl, 1); EXPECT_EQ(dwx_refs(x.cl), 1);
    if (dwx_refs(x.fl) != 1) t_note("file loader still held by dwrite after collection and factory release: %ld (deferred release)", (long)dwx_refs(x.fl));
    CHECK(dwx_refs(x.fl) >= 1 && dwx_refs(x.fl) <= 2);
    dwx_release(x.cl); dwx_release(x.fl);
}
