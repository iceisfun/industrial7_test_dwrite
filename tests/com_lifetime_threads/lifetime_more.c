/* More lifetime contracts: DWriteCreateFactory reference results, copied keys, files held by faces, strings that outlive their
 * owners, repeated creation cycles. */
#include "clt.h"

/* ---- DWriteCreateFactory and the reference it returns ---------------------------------------------------------------------------- */
TEST(lc_create_every_iid_shared) {
    NEED_PROC();
    int n = 0, i; const dw_qi_row *rows = dw_iid_table(&n); IDWriteFactory *s = dw_shared(); ULONG b;
    if (!s) { t_skip("no factory"); return; }
    b = dw_refcount((IUnknown *)s);
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)1; HRESULT hr = dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, rows[i].iid, &p);
        if (SUCCEEDED(hr)) {
            CHECKF(p != NULL, "%s: S_OK with NULL", rows[i].name);
            if (p) {
                CHECKF(dw_same_identity((IUnknown *)p, (IUnknown *)s), "%s: not the shared factory object", rows[i].name);
                EXPECT_REFS(s, b + 1);                      /* the one reference the caller now owns */
                IUnknown_Release((IUnknown *)p);
            }
        } else {
            CHECKF(hr == E_NOINTERFACE, "%s: %s", rows[i].name, dw_hr_name(hr));
            CHECKF(p == NULL, "%s: out pointer not NULL on failure", rows[i].name);
        }
        EXPECT_REFS(s, b);
    }
}
TEST(lc_create_every_iid_isolated) {
    NEED_PROC();
    int n = 0, i; const dw_qi_row *rows = dw_iid_table(&n); int ok = 0;
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)1; HRESULT hr = dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, rows[i].iid, &p);
        if (SUCCEEDED(hr)) {
            ok++;
            CHECKF(p != NULL, "%s: S_OK with NULL", rows[i].name);
            if (p) { CHECKF(dw_refcount((IUnknown *)p) == 1, "%s: new isolated factory must be owned once (refs %lu)", rows[i].name, dw_refcount((IUnknown *)p)); IUnknown_Release((IUnknown *)p); }
        } else {
            CHECKF(hr == E_NOINTERFACE, "%s: %s", rows[i].name, dw_hr_name(hr));
            CHECKF(p == NULL, "%s: out pointer not NULL on failure", rows[i].name);
        }
    }
    CHECK(ok >= 2);        /* at least IUnknown and IDWriteFactory */
}
TEST(lc_create_release_cycles_shared) {
    NEED_PROC();
    IDWriteFactory *s = dw_shared(); ULONG b; int i;
    if (!s) { t_skip("no factory"); return; }
    b = dw_refcount((IUnknown *)s);
    for (i = 0; i < 2000; i++) {
        void *p = NULL;
        if (FAILED(dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &p)) || p != (void *)s) { CHECKF(0, "cycle %d", i); break; }
        IUnknown_Release((IUnknown *)p);
    }
    EXPECT_REFS(s, b);
}
TEST(lc_create_release_cycles_isolated) {
    NEED_PROC();
    int i;
    for (i = 0; i < 300; i++) {
        void *p = NULL;
        if (FAILED(dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &p)) || !p) { CHECKF(0, "cycle %d", i); break; }
        if (dw_refcount((IUnknown *)p) != 1) { CHECKF(0, "cycle %d refcount %lu", i, dw_refcount((IUnknown *)p)); IUnknown_Release((IUnknown *)p); break; }
        IUnknown_Release((IUnknown *)p);
    }
}
TEST(lc_create_with_work_then_release_cycles) {
    NEED_PROC();
    int i;
    for (i = 0; i < 25; i++) {
        IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m; IDWriteFontCollection *c = NULL;
        if (!f) { CHECKF(0, "cycle %d", i); break; }
        IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fm);
        if (fm) IDWriteFactory_CreateTextLayout(f, L"cycle", 5, fm, 100, 100, &l);
        if (l) IDWriteTextLayout_GetMetrics(l, &m);
        IDWriteFactory_GetSystemFontCollection(f, &c, FALSE);
        DW_RELEASE(l); DW_RELEASE(fm); DW_RELEASE(c);
        { ULONG left = IDWriteFactory_Release(f); CHECKF(left < 2, "factory still referenced after the last release (%lu)", left); }
    }
}
TEST(lc_factory_interfaces_one_object_one_count) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); int v, got = 0; IUnknown *p[8] = { 0 };
    if (!f) { t_skip("no factory"); return; }
    for (v = 0; v < 8; v++) {
        if (SUCCEEDED(IDWriteFactory_QueryInterface(f, dw_factory_iid(v), (void **)&p[v])) && p[v]) {
            got++;
            CHECKF(dw_refcount((IUnknown *)f) == (ULONG)(1 + got), "after QI %d the shared count is %lu want %d", v, dw_refcount((IUnknown *)f), 1 + got);
            CHECK(dw_same_identity(p[v], (IUnknown *)f));
        }
    }
    CHECK(got >= 3);     /* IDWriteFactory, 1 and 2 exist on every Windows 10 */
    for (v = 7; v >= 0; v--) if (p[v]) IUnknown_Release(p[v]);       /* reverse order */
    EXPECT_REFS(f, 1);
    DW_RELEASE(f);
}
TEST(lc_factory_release_through_derived_interface) {
    NEED_PROC();
    IDWriteFactory1 *q = NULL;
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory1, (void **)&q)) && q) {
        ULONG r;
        IDWriteFactory_Release(f);                       /* original pointer released first ... */
        r = IDWriteFactory1_AddRef(q); EXPECT_EQ(r, 2);  /* ... the object lives through the other interface pointer */
        r = IDWriteFactory1_Release(q); EXPECT_EQ(r, 1);
        r = IDWriteFactory1_Release(q); EXPECT_EQ(r, 0);
    } else { DW_RELEASE(f); t_skip("IDWriteFactory1 unsupported"); }
}

/* ---- keys are copied, files held by faces ------------------------------------------------------------------------------------------ */
TEST(lc_file_reference_key_is_copied) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; IDWriteFontFile *ff = NULL; size_t n; uint8_t *b; UINT32 key, k2;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new(); b = ttf_build_default(&n); key = dwx_loader_add(ld, b, n); dwx_loader_add(ld, b, n); free(b);
    CHECK_HR_OK(dwx_loader_register(ld, f));
    k2 = key;                                               /* key 0 */
    CHECK_HR_OK(IDWriteFactory_CreateCustomFontFileReference(f, &k2, 4, (IDWriteFontFileLoader *)ld, &ff));
    k2 = 1;                                                 /* the caller reuses its buffer for something else */
    if (ff) {
        const void *kp = NULL; UINT32 ks = 0; UINT32 got = 0xFFFFFFFF; IDWriteFontFace *fc = NULL;
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff, &kp, &ks));
        EXPECT_EQ(ks, 4);
        if (kp && ks == 4) { memcpy(&got, kp, 4); EXPECT_EQ(got, 0); CHECK(kp != (const void *)&k2); }
        CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &fc));
        EXPECT_EQ(ld->last_key_value, 0);                   /* CreateStreamFromKey saw the original key, not the overwritten buffer */
        EXPECT_EQ(ld->last_key_size, 4);
        DW_RELEASE(fc);
    }
    DW_RELEASE(ff);
    CHECK_HR_OK(dwx_loader_unregister(ld));
    dwx_release(ld);
    DW_RELEASE(f);
}
TEST(lc_file_getloader_returns_registered_loader) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (h.file) {
        IDWriteFontFileLoader *got = NULL; LONG before = dwx_refs(h.ld);
        CHECK_HR_OK(IDWriteFontFile_GetLoader(h.file, &got));
        CHECK(got == (IDWriteFontFileLoader *)h.ld);        /* our loader object itself */
        EXPECT_EQ(dwx_refs(h.ld), before + 1);               /* GetLoader returns an owned reference */
        DW_RELEASE(got);
        EXPECT_EQ(dwx_refs(h.ld), before);
    }
    dwx_font_close(&h);
    DW_RELEASE(f);
}
TEST(lc_face_getfiles_reference_counting) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (h.face) {
        UINT32 n = 1; IDWriteFontFile *got = NULL; ULONG fr = dw_refcount((IUnknown *)h.file);
        CHECK_HR_OK(IDWriteFontFace_GetFiles(h.face, &n, &got));
        EXPECT_EQ(n, 1);
        CHECK(got != NULL);
        if (got) {
            t_note("GetFiles returns %s file object than the one the face was built from", got == h.file ? "the same" : "a different");
            CHECKF(dw_refcount((IUnknown *)got) >= 1, "returned file has no reference");
            if (got == h.file) EXPECT_REFS(got, fr + 1);
            { const void *kp; UINT32 ks; CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(got, &kp, &ks)); EXPECT_EQ(ks, 4); }
            DW_RELEASE(got);
            if (h.file) EXPECT_REFS(h.file, fr);
        }
    }
    dwx_font_close(&h);
    DW_RELEASE(f);
}
TEST(lc_face_works_after_file_released) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (h.face) {
        UINT32 cp = 'M'; UINT16 g = 0; UINT32 len = 0; uint8_t *t;
        DW_RELEASE(h.file);                                  /* the application's file reference is gone, the face keeps its own */
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(h.face, &cp, 1, &g)); EXPECT_EQ(g, 14);
        t = dw_face_table(h.face, DW_TAG('O', 'S', '/', '2'), &len); CHECK(t != NULL && len >= 78); free(t);
        {   UINT32 n = 0; CHECK_HR_OK(IDWriteFontFace_GetFiles(h.face, &n, NULL)); EXPECT_EQ(n, 1); }
    }
    dwx_font_close(&h);
    DW_RELEASE(f);
}
TEST(lc_face_survives_loader_object_release_by_app) {
    NEED_PROC();
    /* the application drops its own reference to the loader (keeps it registered through the factory's reference only) */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; size_t n; uint8_t *b; UINT32 key; IDWriteFontFile *ff = NULL; IDWriteFontFace *fc = NULL;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new(); b = ttf_build_default(&n); key = dwx_loader_add(ld, b, n); free(b);
    CHECK_HR_OK(dwx_loader_register(ld, f));
    CHECK_HR_OK(dwx_loader_file(ld, f, key, &ff));
    if (ff) IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &fc);
    DW_RELEASE(ff);
    {   LONG before = dwx_refs(ld);
        dwx_release(ld);                                     /* our reference only: the registry and the face keep the loader alive */
        CHECKF(!dwx_destroyed(ld), "loader destroyed although registered (refs %ld -> %ld)", (long)before, (long)dwx_refs(ld)); }
    if (fc) { UINT32 cp = 'A'; UINT16 g = 0; CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(fc, &cp, 1, &g)); EXPECT_EQ(g, 2); }
    DW_RELEASE(fc);
    ld->registered = 0;
    DW_RELEASE(f);                                           /* registry dropped with the factory: last reference goes away */
    EXPECT_EQ(dwx_destroyed(ld), 1);
}

/* ---- strings and small objects outliving their owners -------------------------------------------------------------------------------- */
TEST(lc_localized_strings_outlive_family_and_collection) {
    NEED_PROC();
    clt_cfix x; IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL; WCHAR nm[64] = { 0 };
    if (!clt_cfix_open(&x, 2)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    dwx_coll_create(x.cl, x.f, &c);
    if (c) IDWriteFontCollection_GetFontFamily(c, 0, &fam);
    if (fam) IDWriteFontFamily_GetFamilyNames(fam, &s);
    DW_RELEASE(fam); DW_RELEASE(c);
    CHECK(s != NULL);
    if (s) {
        CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1);
        CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, nm, 64));
        EXPECT_WSTR(nm, L"CltFam0");
        DW_RELEASE(s);
    }
    clt_cfix_close(&x);
}
TEST(lc_font_names_outlive_font) {
    NEED_PROC();
    clt_cfix x; IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *fn = NULL; IDWriteLocalizedStrings *s = NULL; WCHAR nm[64] = { 0 };
    if (!clt_cfix_open(&x, 1)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    dwx_coll_create(x.cl, x.f, &c);
    if (c) IDWriteFontCollection_GetFontFamily(c, 0, &fam);
    if (fam) IDWriteFontFamily_GetFont(fam, 0, &fn);
    if (fn) IDWriteFont_GetFaceNames(fn, &s);
    DW_RELEASE(fn); DW_RELEASE(fam); DW_RELEASE(c);
    if (s) { CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, nm, 64)); EXPECT_WSTR(nm, L"Regular"); DW_RELEASE(s); } else t_skip("face names");
    clt_cfix_close(&x);
}
TEST(lc_font_metrics_and_family_after_collection_release) {
    NEED_PROC();
    clt_cfix x; IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL, *back = NULL; IDWriteFont *fn = NULL; DWRITE_FONT_METRICS m;
    if (!clt_cfix_open(&x, 1)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    dwx_coll_create(x.cl, x.f, &c);
    if (c) IDWriteFontCollection_GetFontFamily(c, 0, &fam);
    if (fam) IDWriteFontFamily_GetFont(fam, 0, &fn);
    DW_RELEASE(fam); DW_RELEASE(c);
    if (fn) {
        IDWriteFont_GetMetrics(fn, &m); EXPECT_EQ(m.designUnitsPerEm, 2048);
        CHECK_HR_OK(IDWriteFont_GetFontFamily(fn, &back));
        if (back) { CHECK(IDWriteFontFamily_GetFontCount(back) >= 1); DW_RELEASE(back); }
        { BOOL ex = FALSE; CHECK_HR_OK(IDWriteFont_HasCharacter(fn, 'A', &ex)); CHECK(ex); }
        DW_RELEASE(fn);
    } else t_skip("font");
    clt_cfix_close(&x);
}
TEST(lc_typography_independent_of_layout_after_set) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTypography *ty = NULL; IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1 };
    IDWriteFactory_CreateTypography(f, &ty);
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(f, L"fi", 2, fm, 100, 100, &l);
    DW_RELEASE(fm);
    if (!ty || !l) { t_skip("objects"); DW_RELEASE(ty); DW_RELEASE(l); return; }
    { DWRITE_TEXT_RANGE r = { 0, 2 }; CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, ty, r)); }
    CHECK_HR_OK(IDWriteTypography_AddFontFeature(ty, ft));   /* mutating the typography afterwards is allowed; layout shares the object */
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(ty), 1);
    DW_RELEASE(ty);                                           /* the layout keeps the typography alive */
    {   IDWriteTypography *got = NULL; DWRITE_TEXT_RANGE r;
        CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 0, &got, &r));
        CHECK(got != NULL);
        if (got) { EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(got), 1); DW_RELEASE(got); } }
    DW_RELEASE(l);
}
TEST(lc_numbersubstitution_kept_by_analysis_source) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; static const WCHAR txt[] = L"123 456"; dwx_ana_src *src; ULONG b;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL, L"ar-SA", FALSE, &ns));
    if (!ns) { t_skip("substitution"); return; }
    src = dwx_ana_src_new(txt, 7); src->subst = ns;
    b = dw_refcount((IUnknown *)ns);
    {
        IDWriteTextAnalyzer *an = NULL; dwx_ana_sink *sk = dwx_ana_sink_new();
        IDWriteFactory_CreateTextAnalyzer(f, &an);
        if (an) { CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, 0, 7, (IDWriteTextAnalysisSink *)sk)); }
        CHECK(sk->n_subst >= 1);
        DW_RELEASE(an); dwx_release(sk);
    }
    t_note("number substitution refs: before analysis %lu, after %lu (+ our own)", b, dw_refcount((IUnknown *)ns));
    dwx_release(src);
    DW_RELEASE(ns);
}
TEST(lc_system_collection_repeat_calls_balance) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontCollection *a = NULL, *b2 = NULL; ULONG ra, i;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &a, FALSE));
    ra = dw_refcount((IUnknown *)a);
    for (i = 0; i < 50; i++) { b2 = NULL; CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &b2, FALSE)); if (b2 == a) { EXPECT_REFS(a, ra + 1); } DW_RELEASE(b2); }
    EXPECT_REFS(a, ra);
    DW_RELEASE(a);
    DW_RELEASE(f);
}
TEST(lc_gdi_interop_repeat_calls_balance) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGdiInterop *a = NULL; ULONG r0; int i;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a));
    r0 = dw_refcount((IUnknown *)a);
    for (i = 0; i < 20; i++) { IDWriteGdiInterop *g = NULL; CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g)); DW_RELEASE(g); }
    EXPECT_REFS(a, r0);
    DW_RELEASE(a);
    DW_RELEASE(f);
}
TEST(lc_renderer_not_retained_by_layout) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; dwx_renderer *r = dwx_renderer_new(); int i;
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(f, L"draw", 4, fm, 100, 100, &l);
    DW_RELEASE(fm);
    if (!l) { t_skip("layout"); dwx_release(r); return; }
    for (i = 0; i < 20; i++) { IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0, 0); EXPECT_EQ(dwx_refs(r), 1); }
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(r), 1);
    dwx_release(r);
}
TEST(lc_listener_and_queue_balance) {
    NEED_PROC();
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontDownloadQueue *q = NULL; dwx_listener *ls;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    if (FAILED(IDWriteFactory3_GetFontDownloadQueue(f3, &q)) || !q) { t_skip("download queue unavailable"); return; }
    ls = dwx_listener_new();
    {
        UINT32 tok = 0; HRESULT hr = IDWriteFontDownloadQueue_AddListener(q, (IDWriteFontDownloadListener *)ls, &tok);
        if (FAILED(hr)) t_note("AddListener -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr)) {
            CHECKF(dwx_refs(ls) >= 2, "queue must retain the registered listener (refs %ld)", (long)dwx_refs(ls));
            CHECK_HR_OK(IDWriteFontDownloadQueue_RemoveListener(q, tok));
            EXPECT_EQ(dwx_refs(ls), 1);                       /* released on removal */
            t_note("second RemoveListener of the same token -> %s", dw_hr_name(IDWriteFontDownloadQueue_RemoveListener(q, tok)));
        }
    }
    dwx_release(ls);
    DW_RELEASE(q);
}
