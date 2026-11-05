/* Factory independence and object lifetime across factories.
 *
 * Facts: DWriteCreateFactory(SHARED) returns the process-wide factory, ISOLATED builds a new one each time; objects created by
 * a factory keep what they need from it alive, so they stay usable after the creator's reference is released; releasing one
 * factory never disturbs objects or factories of another.
 */
#include "fto.h"

/* Create one object of every kind this area covers from `f`; returned in a bundle so tests can use them after f is gone. */
typedef struct bundle {
    IDWriteTextFormat *fmt; IDWriteTextLayout *layout, *gdi_layout; IDWriteInlineObject *sign; IDWriteTypography *typo;
    IDWriteNumberSubstitution *ns; IDWriteRenderingParams *rp, *crp; IDWriteGdiInterop *interop; IDWriteTextAnalyzer *an;
    IDWriteFontCollection *sysfc;
} bundle;

static int bundle_make(bundle *b, IDWriteFactory *f) {
    int ok = 1;
    memset(b, 0, sizeof *b);
    ok &= SUCCEEDED(IDWriteFactory_CreateTextFormat(f, L"Segoe UI", NULL, 400, 0, 5, 18.0f, L"en-us", &b->fmt));
    if (b->fmt) {
        ok &= SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"Bundle layout text", 18, b->fmt, 200.0f, 100.0f, &b->layout));
        ok &= SUCCEEDED(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"Gdi layout", 10, b->fmt, 200.0f, 100.0f, 1.0f, NULL, FALSE, &b->gdi_layout));
        ok &= SUCCEEDED(IDWriteFactory_CreateEllipsisTrimmingSign(f, b->fmt, &b->sign));
    }
    ok &= SUCCEEDED(IDWriteFactory_CreateTypography(f, &b->typo));
    ok &= SUCCEEDED(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-us", FALSE, &b->ns));
    ok &= SUCCEEDED(IDWriteFactory_CreateRenderingParams(f, &b->rp));
    ok &= SUCCEEDED(IDWriteFactory_CreateCustomRenderingParams(f, 2.0f, 0.5f, 0.5f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_NATURAL, &b->crp));
    ok &= SUCCEEDED(IDWriteFactory_GetGdiInterop(f, &b->interop));
    ok &= SUCCEEDED(IDWriteFactory_CreateTextAnalyzer(f, &b->an));
    ok &= SUCCEEDED(IDWriteFactory_GetSystemFontCollection(f, &b->sysfc, FALSE));
    return ok;
}

static void bundle_use(bundle *b) {
    DWRITE_TEXT_METRICS m;
    if (b->fmt) CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(b->fmt), 18.0f);
    if (b->layout) { CHECK_HR_OK(IDWriteTextLayout_GetMetrics(b->layout, &m)); CHECK(m.width > 0.0f); CHECK_FLOAT_EQ(m.layoutWidth, 200.0f); }
    if (b->gdi_layout) { CHECK_HR_OK(IDWriteTextLayout_GetMetrics(b->gdi_layout, &m)); CHECK(m.width > 0.0f); }
    if (b->sign) { DWRITE_INLINE_OBJECT_METRICS im; CHECK_HR_OK(IDWriteInlineObject_GetMetrics(b->sign, &im)); CHECK(im.width > 0.0f); }
    if (b->typo) { DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_KERNING, 1 }; UINT32 c0 = IDWriteTypography_GetFontFeatureCount(b->typo); CHECK_HR_OK(IDWriteTypography_AddFontFeature(b->typo, ft)); CHECK(IDWriteTypography_GetFontFeatureCount(b->typo) == c0 + 1); }
    if (b->crp) { CHECK_FLOAT_EQ(IDWriteRenderingParams_GetGamma(b->crp), 2.0f); CHECK(IDWriteRenderingParams_GetRenderingMode(b->crp) == DWRITE_RENDERING_MODE_NATURAL); }
    if (b->rp) CHECK(IDWriteRenderingParams_GetGamma(b->rp) > 0.0f);
    if (b->sysfc) CHECK(IDWriteFontCollection_GetFontFamilyCount(b->sysfc) > 0);
    if (b->interop) { IDWriteGdiInterop1 *g1 = NULL; if (SUCCEEDED(IDWriteGdiInterop_QueryInterface(b->interop, &IID_IDWriteGdiInterop1, (void **)&g1)) && g1) IDWriteGdiInterop1_Release(g1); }
    if (b->an) {
        static const WCHAR t[] = { 'a', 'b', 'c' };
        dwx_ana_src *src = dwx_ana_src_new(t, 3); dwx_ana_sink *sink = dwx_ana_sink_new();
        CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeScript(b->an, (IDWriteTextAnalysisSource *)src, 0, 3, (IDWriteTextAnalysisSink *)sink));
        CHECK(dwx_ana_covered(sink, 's', 3));
        dwx_release(src); dwx_release(sink);
    }
}

static void bundle_free(bundle *b) {
    DW_RELEASE(b->fmt); DW_RELEASE(b->layout); DW_RELEASE(b->gdi_layout); DW_RELEASE(b->sign); DW_RELEASE(b->typo); DW_RELEASE(b->ns);
    DW_RELEASE(b->rp); DW_RELEASE(b->crp); DW_RELEASE(b->interop); DW_RELEASE(b->an); DW_RELEASE(b->sysfc);
}

/* ---- factory identity --------------------------------------------------------------------------------------------- */
TEST(fto_life_shared_factory_is_process_wide) {
    void *a = NULL, *b = NULL;
    REQUIRE(dw_proc());
    CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &a));
    CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &b));
    CHECK(a != NULL && a == b);
    CHECK((IDWriteFactory *)a == dw_shared());
    if (a) {
        ULONG r1 = dw_refcount((IUnknown *)a);
        IUnknown_Release((IUnknown *)a);
        ULONG r2 = dw_refcount((IUnknown *)a);
        t_note("shared factory refcount %lu -> %lu after releasing one reference", r1, r2);
        CHECK(r2 <= r1);
    }
    if (b) IUnknown_Release((IUnknown *)b);
}

TEST(fto_life_isolated_factories_are_distinct) {
    void *a = NULL, *b = NULL;
    REQUIRE(dw_proc());
    CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &a));
    CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &b));
    CHECK(a && b && a != b);
    CHECK(a != (void *)dw_shared() && b != (void *)dw_shared());
    if (a) { EXPECT_REFS(a, 1); IUnknown_Release((IUnknown *)a); }
    if (b) { EXPECT_REFS(b, 1); IUnknown_Release((IUnknown *)b); }
}

TEST(fto_life_isolated_objects_from_one_factory_are_independent_of_another) {
    IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *b = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    bundle ba, bb;
    if (!a || !b) { t_skip("no factories"); DW_RELEASE(a); DW_RELEASE(b); return; }
    CHECK(bundle_make(&ba, a));
    CHECK(bundle_make(&bb, b));
    IDWriteFactory_Release(a);              /* A is gone: B's objects must be untouched, A's own still work */
    bundle_use(&bb);
    bundle_use(&ba);
    bundle_free(&ba);
    bundle_use(&bb);
    bundle_free(&bb);
    IDWriteFactory_Release(b);
}

TEST(fto_life_objects_outlive_their_isolated_factory) {
    IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    bundle ba;
    if (!a) { t_skip("no factory"); return; }
    CHECK(bundle_make(&ba, a));
    IDWriteFactory_Release(a);
    bundle_use(&ba);
    /* system collection stays usable for lookups too */
    if (ba.sysfc) { UINT32 i = 0; BOOL e = TRUE; CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(ba.sysfc, L"NoSuchFamily_zz9", &i, &e)); CHECK(!e); }
    bundle_free(&ba);
}

TEST(fto_life_shared_factory_unaffected_by_isolated_release) {
    IDWriteFactory *sh = dw_shared(), *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    bundle bs, bi;
    if (!sh || !iso) { t_skip("no factories"); DW_RELEASE(iso); return; }
    CHECK(bundle_make(&bs, sh));
    CHECK(bundle_make(&bi, iso));
    bundle_free(&bi);
    IDWriteFactory_Release(iso);
    bundle_use(&bs);
    IDWriteTextFormat *fmt = fto_format(sh, 9.0f);
    CHECK(fmt != NULL);
    DW_RELEASE(fmt);
    bundle_free(&bs);
}

TEST(fto_life_shared_objects_survive_isolated_factory_churn) {
    IDWriteFactory *sh = dw_shared();
    bundle bs;
    int i;
    if (!sh) { t_skip("no factory"); return; }
    CHECK(bundle_make(&bs, sh));
    for (i = 0; i < 20; i++) {
        IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
        bundle bi;
        if (!iso) break;
        CHECK(bundle_make(&bi, iso));
        bundle_free(&bi);
        IDWriteFactory_Release(iso);
    }
    bundle_use(&bs);
    bundle_free(&bs);
}

TEST(fto_life_order_of_release_does_not_matter) {
    int order;
    for (order = 0; order < 3; order++) {
        IDWriteFactory *a = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
        IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *l = NULL;
        if (!a) { t_skip("no factory"); return; }
        fmt = fto_format(a, 14.0f);
        if (!fmt) { t_skip("no format"); IDWriteFactory_Release(a); return; }
        CHECK_HR_OK(IDWriteFactory_CreateTextLayout(a, L"order", 5, fmt, 100, 50, &l));
        if (order == 0) { IDWriteFactory_Release(a); IDWriteTextFormat_Release(fmt); }
        else if (order == 1) { IDWriteTextFormat_Release(fmt); IDWriteFactory_Release(a); }
        else { IDWriteTextFormat_Release(fmt); }
        if (l) { DWRITE_TEXT_METRICS m; CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m)); CHECK(m.width > 0.0f); IDWriteTextLayout_Release(l); }
        if (order == 2) IDWriteFactory_Release(a);
    }
}

TEST(fto_life_many_factories_create_destroy) {
    int i;
    for (i = 0; i < 50; i++) {
        IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
        IDWriteTextFormat *fmt;
        if (!f) { t_skip("no factory"); return; }
        fmt = fto_format(f, 10.0f + (float)i);
        CHECK(fmt != NULL);
        if (fmt) { CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(fmt), 10.0f + (float)i); IDWriteTextFormat_Release(fmt); }
        EXPECT_REFS(f, 1);
        IDWriteFactory_Release(f);
    }
}

TEST(fto_life_factory_refcount_with_children_note) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteTextFormat *fmt; IDWriteTypography *ty = NULL;
    if (!f) { t_skip("no factory"); return; }
    ULONG r0 = dw_refcount((IUnknown *)f);
    fmt = fto_format(f, 12.0f);
    IDWriteFactory_CreateTypography(f, &ty);
    ULONG r1 = dw_refcount((IUnknown *)f);
    t_note("isolated factory refcount: %lu bare, %lu with a text format and a typography alive", r0, r1);
    CHECK(r0 == 1);
    DW_RELEASE(fmt); DW_RELEASE(ty);
    CHECK(dw_refcount((IUnknown *)f) <= r1);
    IDWriteFactory_Release(f);
}

/* ---- the same call on both factories gives equivalent results ------------------------------------------------------------ */
TEST(fto_life_shared_and_isolated_agree_on_validation) {
    IDWriteFactory *fs = dw_shared(), *fi = dw_isolated();
    if (!fs || !fi) { t_skip("no factories"); return; }
    IDWriteTextFormat *a = (IDWriteTextFormat *)FTO_SENTINEL, *b = (IDWriteTextFormat *)FTO_SENTINEL;
    HRESULT ha = IDWriteFactory_CreateTextFormat(fs, L"Arial", NULL, 1000, 0, 5, 12, L"en-us", &a);
    HRESULT hb = IDWriteFactory_CreateTextFormat(fi, L"Arial", NULL, 1000, 0, 5, 12, L"en-us", &b);
    CHECK(ha == hb);
    CHECK(a == NULL && b == NULL);
    a = b = NULL;
    ha = IDWriteFactory_CreateTextFormat(fs, L"Arial", NULL, 400, 0, 5, 12, L"en-us", &a);
    hb = IDWriteFactory_CreateTextFormat(fi, L"Arial", NULL, 400, 0, 5, 12, L"en-us", &b);
    CHECK(ha == S_OK && hb == S_OK);
    DW_RELEASE(a); DW_RELEASE(b);
}

/* ---- concurrency: the factory is free-threaded ------------------------------------------------------------------------------ */
typedef struct { IDWriteFactory *f; int iters; volatile LONG failures; } thr_ctx;
static DWORD WINAPI thr_proc(LPVOID p) {
    thr_ctx *c = (thr_ctx *)p;
    int i;
    for (i = 0; i < c->iters; i++) {
        IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *l = NULL; IDWriteTypography *ty = NULL; IDWriteNumberSubstitution *ns = NULL;
        IDWriteRenderingParams *rp = NULL; IDWriteFontCollection *fc = NULL; IDWriteTextAnalyzer *an = NULL;
        if (FAILED(IDWriteFactory_CreateTextFormat(c->f, L"Arial", NULL, 400, 0, 5, 12.0f + (float)(i % 7), L"en-us", &fmt)) || !fmt) { InterlockedIncrement(&c->failures); continue; }
        if (FAILED(IDWriteFactory_CreateTextLayout(c->f, L"thread text", 11, fmt, 100.0f, 50.0f, &l)) || !l) InterlockedIncrement(&c->failures);
        if (FAILED(IDWriteFactory_CreateTypography(c->f, &ty)) || !ty) InterlockedIncrement(&c->failures);
        if (FAILED(IDWriteFactory_CreateNumberSubstitution(c->f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, NULL, FALSE, &ns)) || !ns) InterlockedIncrement(&c->failures);
        if (FAILED(IDWriteFactory_CreateRenderingParams(c->f, &rp)) || !rp) InterlockedIncrement(&c->failures);
        if (FAILED(IDWriteFactory_GetSystemFontCollection(c->f, &fc, FALSE)) || !fc) InterlockedIncrement(&c->failures);
        if (FAILED(IDWriteFactory_CreateTextAnalyzer(c->f, &an)) || !an) InterlockedIncrement(&c->failures);
        if (fmt && IDWriteTextFormat_GetFontSize(fmt) != 12.0f + (float)(i % 7)) InterlockedIncrement(&c->failures);
        DW_RELEASE(l); DW_RELEASE(ty); DW_RELEASE(ns); DW_RELEASE(rp); DW_RELEASE(fc); DW_RELEASE(an); DW_RELEASE(fmt);
    }
    return 0;
}

static void run_threads(IDWriteFactory *f, int nthreads, int iters) {
    HANDLE h[16]; thr_ctx c; int i, started = 0;
    c.f = f; c.iters = iters; c.failures = 0;
    for (i = 0; i < nthreads && i < 16; i++) { h[i] = CreateThread(NULL, 0, thr_proc, &c, 0, NULL); if (h[i]) started++; }
    for (i = 0; i < started; i++) {
        DWORD w = WaitForSingleObject(h[i], 20000);
        CHECKF(w == WAIT_OBJECT_0, "thread %d did not finish in time", i);
        if (w == WAIT_OBJECT_0) CloseHandle(h[i]);     /* a stuck thread is left alone: killing it would corrupt shared state */
    }
    CHECKF(c.failures == 0, "%ld creation failures across threads", (long)c.failures);
    CHECK(started == nthreads);
}

TEST(fto_life_concurrent_creation_shared_factory) {
    IDWriteFactory *f = dw_shared();
    if (!f) { t_skip("no factory"); return; }
    if (t_is_wine()) { t_skip("concurrent creation crashes the Wine implementation"); return; }
    run_threads(f, 8, 100);
}

TEST(fto_life_concurrent_creation_isolated_factory) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    if (t_is_wine()) { t_skip("concurrent creation crashes the Wine implementation"); IDWriteFactory_Release(f); return; }
    run_threads(f, 8, 100);
    IDWriteFactory_Release(f);
}

static DWORD WINAPI rc_worker(LPVOID p) {
    IDWriteTextFormat *f = (IDWriteTextFormat *)p; int k;
    for (k = 0; k < 5000; k++) { IDWriteTextFormat_AddRef(f); IDWriteTextFormat_Release(f); }
    return 0;
}

TEST(fto_life_concurrent_refcounting_of_one_object) {
    IDWriteFactory *sh = dw_shared();
    IDWriteTextFormat *fmt = sh ? fto_format(sh, 12.0f) : NULL;
    HANDLE h[8]; int i;
    if (!fmt) { t_skip("no format"); return; }
    for (i = 0; i < 8; i++) h[i] = CreateThread(NULL, 0, rc_worker, fmt, 0, NULL);
    for (i = 0; i < 8; i++) { if (h[i]) { CHECK(WaitForSingleObject(h[i], 20000) == WAIT_OBJECT_0); CloseHandle(h[i]); } }
    EXPECT_REFS(fmt, 1);
    IDWriteTextFormat_Release(fmt);
}
