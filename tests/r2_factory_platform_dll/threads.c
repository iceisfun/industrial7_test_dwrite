/* Multithreaded use of the shared factory: layouts, formats, number substitutions, system collection. Thread count <= 16, bounded loops,
 * bounded waits. Test doubles are created on the main thread. The shared factory is free-threaded. */
#include "rfp.h"

#define MAXT 16
typedef struct {
    int idx, iters;
    HANDLE go;
    volatile LONG fail;
    float width;                 /* result of the layout measurement */
    void *aux;                   /* per-thread test double */
    void *shared_obj;            /* object shared by all threads */
    UINT32 count;
} wctx;

typedef DWORD (WINAPI *worker_fn)(void *);
static void run(int n, worker_fn fn, wctx *c, int iters, void *shared_obj) {
    HANDLE th[MAXT]; int i; HANDLE go = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (n > MAXT) n = MAXT;
    for (i = 0; i < n; i++) { c[i].idx = i; c[i].iters = iters; c[i].go = go; c[i].fail = 0; c[i].shared_obj = shared_obj; th[i] = CreateThread(NULL, 0, fn, &c[i], 0, NULL); if (!th[i]) c[i].fail++; }
    SetEvent(go);
    for (i = 0; i < n; i++) if (th[i]) { if (WaitForSingleObject(th[i], 30000) != WAIT_OBJECT_0) { TerminateThread(th[i], 1); c[i].fail++; } CloseHandle(th[i]); }
    CloseHandle(go);
}
static int total_fail(wctx *c, int n) { int i, t = 0; for (i = 0; i < n; i++) t += c[i].fail; return t; }

/* ---- layouts from the shared factory ---------------------------------------------------------------------------- */
static DWORD WINAPI w_layout(void *p) {
    wctx *c = (wctx *)p; IDWriteFactory *f = dw_shared(); int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        IDWriteTextFormat *tf = rfp_fmt(f, 16.0f, L"en-us"); IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
        if (!tf) { c->fail++; continue; }
        if (FAILED(IDWriteFactory_CreateTextLayout(f, L"The quick brown fox", 19, tf, 10000.0f, 100.0f, &l)) || !l) { c->fail++; IDWriteTextFormat_Release(tf); continue; }
        if (FAILED(IDWriteTextLayout_GetMetrics(l, &m))) c->fail++;
        else if (i == 0) c->width = m.widthIncludingTrailingWhitespace;
        else if (m.widthIncludingTrailingWhitespace != c->width) c->fail++;
        IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(tf);
    }
    return 0;
}
static void layout_threads(int n, int iters) {
    wctx c[MAXT]; int i;
    if (!dw_shared() || !dw_have_font(dw_shared(), L"Arial")) { t_skip("no shared factory / Arial"); return; }
    memset(c, 0, sizeof c);
    run(n, w_layout, c, iters, NULL);
    EXPECT_EQ(total_fail(c, n), 0);
    for (i = 1; i < n; i++) CHECK(c[i].width == c[0].width);   /* the same text and font measure identically on every thread */
    CHECK(c[0].width > 0.0f);
}
TEST(mt_layouts_2_threads) { layout_threads(2, 100); }
TEST(mt_layouts_4_threads) { layout_threads(4, 100); }
TEST(mt_layouts_8_threads) { layout_threads(8, 60); }
TEST(mt_layouts_16_threads) { layout_threads(16, 30); }

/* ---- shared layout read concurrently ------------------------------------------------------------------------------ */
static DWORD WINAPI w_layout_read(void *p) {
    wctx *c = (wctx *)p; IDWriteTextLayout *l = (IDWriteTextLayout *)c->shared_obj; int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        DWRITE_TEXT_METRICS m;
        if (FAILED(IDWriteTextLayout_GetMetrics(l, &m))) { c->fail++; continue; }
        if (i == 0) c->width = m.width; else if (m.width != c->width) c->fail++;
    }
    return 0;
}
TEST(mt_one_layout_read_by_8_threads) {
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *tf; IDWriteTextLayout *l = NULL; wctx c[8]; int i;
    if (!f || !dw_have_font(f, L"Arial")) { t_skip("no Arial"); return; }
    tf = rfp_fmt(f, 16.0f, L"en-us"); REQUIRE(tf);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"Concurrent readers", 18, tf, 10000.0f, 100.0f, &l)); REQUIRE(l);
    memset(c, 0, sizeof c);
    run(8, w_layout_read, c, 200, l);
    EXPECT_EQ(total_fail(c, 8), 0);
    for (i = 1; i < 8; i++) CHECK(c[i].width == c[0].width);
    DW_RELEASE(l); DW_RELEASE(tf);
}

/* ---- number substitution objects from the shared factory ------------------------------------------------------------- */
static DWORD WINAPI w_numsub(void *p) {
    wctx *c = (wctx *)p; IDWriteFactory *f = dw_shared(); int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        IDWriteNumberSubstitution *s = NULL; int m = (i + c->idx) % 5;
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, m == 2 ? NULL : L"ar-SA", i & 1, &s);
        if (hr != S_OK || !s) { c->fail++; continue; }
        {   IUnknown *u = NULL; if (SUCCEEDED(IDWriteNumberSubstitution_QueryInterface(s, &IID_IUnknown, (void **)&u))) IUnknown_Release(u); else c->fail++; }
        IDWriteNumberSubstitution_Release(s);
    }
    return 0;
}
static void numsub_threads(int n) {
    wctx c[MAXT]; memset(c, 0, sizeof c);
    NEED_EXPORT();
    run(n, w_numsub, c, 300, NULL);
    EXPECT_EQ(total_fail(c, n), 0);
}
TEST(mt_numsubst_create_4_threads) { numsub_threads(4); }
TEST(mt_numsubst_create_16_threads) { numsub_threads(16); }

static DWORD WINAPI w_numsub_analyze(void *p) {
    wctx *c = (wctx *)p; IDWriteFactory *f = dw_shared(); dwx_ana_src *src = (dwx_ana_src *)c->aux; int i;
    IDWriteTextAnalyzer *an = NULL; IDWriteTextAnalyzer1 *an1 = NULL;
    WaitForSingleObject(c->go, 5000);
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) { c->fail++; return 0; }
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer1, (void **)&an1)) || !an1) { IDWriteTextAnalyzer_Release(an); c->fail++; return 0; }
    for (i = 0; i < c->iters; i++) {
        IDWriteNumberSubstitution *s = NULL; dwx_ana_sink *sink = (dwx_ana_sink *)((void **)c->shared_obj)[c->idx];
        if (FAILED(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"ar-SA", FALSE, &s)) || !s) { c->fail++; continue; }
        if (src->subst) IDWriteNumberSubstitution_Release(src->subst);
        src->subst = s;   /* the double hands out AddRef'd copies of what it holds */
        if (FAILED(IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, 0, src->len, (IDWriteTextAnalysisSink *)sink))) c->fail++;
    }
    IDWriteTextAnalyzer1_Release(an1); IDWriteTextAnalyzer_Release(an);
    return 0;
}
TEST(mt_numsubst_in_analysis_8_threads) {
    enum { N = 8 }; wctx c[N]; dwx_ana_src *src[N]; void *sinks[N]; int i;
    static const WCHAR text[] = L"abc 123 def 456";
    NEED_EXPORT();
    if (t_is_wine()) { t_skip("emulated analyzer crashes with concurrent use of the shared factory"); return; }
    memset(c, 0, sizeof c);
    for (i = 0; i < N; i++) {
        src[i] = dwx_ana_src_new(text, 15); src[i]->support_v1 = 1; c[i].aux = src[i]; sinks[i] = dwx_ana_sink_new();
    }
    {   HANDLE th[N]; HANDLE go = CreateEventA(NULL, TRUE, FALSE, NULL);
        for (i = 0; i < N; i++) { c[i].idx = i; c[i].iters = 50; c[i].go = go; c[i].shared_obj = sinks; th[i] = CreateThread(NULL, 0, w_numsub_analyze, &c[i], 0, NULL); if (!th[i]) c[i].fail++; }
        SetEvent(go);
        for (i = 0; i < N; i++) if (th[i]) { if (WaitForSingleObject(th[i], 30000) != WAIT_OBJECT_0) { TerminateThread(th[i], 1); c[i].fail++; } CloseHandle(th[i]); }
        CloseHandle(go); }
    EXPECT_EQ(total_fail(c, N), 0);
    for (i = 0; i < N; i++) { t_note("thread %d: source GetNumberSubstitution calls %ld", i, (long)src[i]->n_subst); if (src[i]->subst) { IDWriteNumberSubstitution_Release(src[i]->subst); src[i]->subst = NULL; } dwx_release(src[i]); dwx_release(sinks[i]); }
}

/* ---- system collection and font set ----------------------------------------------------------------------------------- */
static DWORD WINAPI w_syscoll(void *p) {
    wctx *c = (wctx *)p; IDWriteFactory *f = dw_shared(); int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        IDWriteFontCollection *col = NULL; UINT32 n;
        if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &col, FALSE)) || !col) { c->fail++; continue; }
        n = IDWriteFontCollection_GetFontFamilyCount(col);
        if (i == 0) c->count = n; else if (n != c->count) c->fail++;
        if (col != (IDWriteFontCollection *)c->shared_obj) c->fail++;
        IDWriteFontCollection_Release(col);
    }
    return 0;
}
TEST(mt_system_collection_same_object_on_all_threads) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *col = NULL; wctx c[8]; int i; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &col, FALSE)); REQUIRE(col);
    memset(c, 0, sizeof c);
    run(8, w_syscoll, c, 100, col);
    EXPECT_EQ(total_fail(c, 8), 0);
    for (i = 1; i < 8; i++) CHECK(c[i].count == c[0].count);
    DW_RELEASE(col);
}
TEST(mt_system_collection_refcount_balanced_after_threads) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *col = NULL; wctx c[8]; ULONG r0; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &col, FALSE)); REQUIRE(col);
    r0 = dw_refcount((IUnknown *)col);
    memset(c, 0, sizeof c);
    run(8, w_syscoll, c, 100, col);
    EXPECT_REFS(col, r0);
    DW_RELEASE(col);
}
static DWORD WINAPI w_fontset(void *p) {
    wctx *c = (wctx *)p; IDWriteFactory3 *f3 = (IDWriteFactory3 *)c->shared_obj; int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        IDWriteFontSet *s = NULL; UINT32 n;
        if (FAILED(IDWriteFactory3_GetSystemFontSet(f3, &s)) || !s) { c->fail++; continue; }
        n = IDWriteFontSet_GetFontCount(s);
        if (i == 0) c->count = n; else if (n != c->count) c->fail++;
        IDWriteFontSet_Release(s);
    }
    return 0;
}
TEST(mt_system_font_set_8_threads) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(FT_SHARED, 3); wctx c[8]; int i;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    memset(c, 0, sizeof c);
    run(8, w_fontset, c, 50, f3);
    EXPECT_EQ(total_fail(c, 8), 0);
    for (i = 1; i < 8; i++) CHECK(c[i].count == c[0].count);
    CHECK(c[0].count > 0);
}
static DWORD WINAPI w_checkupdates(void *p) {
    wctx *c = (wctx *)p; IDWriteFactory *f = dw_shared(); int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        IDWriteFontCollection *col = NULL;
        if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &col, (i + c->idx) & 1)) || !col) { c->fail++; continue; }
        if (IDWriteFontCollection_GetFontFamilyCount(col) == 0) c->fail++;
        IDWriteFontCollection_Release(col);
    }
    return 0;
}
TEST(mt_system_collection_check_for_updates_mixed_4_threads) {
    wctx c[4]; memset(c, 0, sizeof c);
    NEED_EXPORT();
    if (t_is_wine()) { t_skip("emulated collection refresh is not thread safe"); return; }
    run(4, w_checkupdates, c, 30, NULL);
    EXPECT_EQ(total_fail(c, 4), 0);
}

/* ---- shared factory pointer and refcount under churn ---------------------------------------------------------------------- */
static DWORD WINAPI w_churn(void *p) {
    wctx *c = (wctx *)p; int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        IUnknown *u = NULL;
        if (rfp_create(FT_SHARED, &IID_IUnknown, (void **)&u) != S_OK || !u) { c->fail++; continue; }
        if (u != (IUnknown *)c->shared_obj) c->fail++;
        IUnknown_AddRef(u); IUnknown_Release(u); IUnknown_Release(u);
    }
    return 0;
}
TEST(mt_shared_pointer_identity_and_balance_16_threads) {
    IUnknown *base = NULL; wctx c[16]; ULONG r0; NEED_EXPORT();
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&base)); REQUIRE(base);
    r0 = dw_refcount(base);
    memset(c, 0, sizeof c);
    run(16, w_churn, c, 500, base);
    EXPECT_EQ(total_fail(c, 16), 0);
    EXPECT_REFS(base, r0);
    DW_RELEASE(base);
}
static DWORD WINAPI w_iso(void *p) {
    wctx *c = (wctx *)p; int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        IDWriteFactory *f = dw_new_factory(FT_ISOLATED); IDWriteTextFormat *tf;
        if (!f) { c->fail++; continue; }
        tf = rfp_fmt(f, 12.0f, L"en-us"); if (!tf) c->fail++; else IDWriteTextFormat_Release(tf);
        IDWriteFactory_Release(f);
    }
    return 0;
}
TEST(mt_isolated_factory_churn_8_threads) {
    wctx c[8]; memset(c, 0, sizeof c);
    NEED_EXPORT();
    run(8, w_iso, c, 25, NULL);
    EXPECT_EQ(total_fail(c, 8), 0);
}

/* ---- custom loader registration on the shared factory is serialized ---------------------------------------------------------- */
static DWORD WINAPI w_reg(void *p) {
    wctx *c = (wctx *)p; dwx_mem_loader *ld = (dwx_mem_loader *)c->aux; IDWriteFactory *f = dw_shared(); int i;
    WaitForSingleObject(c->go, 5000);
    for (i = 0; i < c->iters; i++) {
        IDWriteFontFileLoader *fl = (IDWriteFontFileLoader *)ld;
        if (SUCCEEDED(IDWriteFactory_RegisterFontFileLoader(f, fl))) { if (FAILED(IDWriteFactory_UnregisterFontFileLoader(f, fl))) c->fail++; }
        /* a loss of the race (E_INVALIDARG for an existing registration) is fine; no outcome may corrupt the registry */
    }
    return 0;
}
TEST(mt_register_unregister_distinct_loaders_8_threads) {
    wctx c[8]; dwx_mem_loader *ld[8]; int i;
    NEED_EXPORT();
    if (t_is_wine()) { t_skip("emulated loader registry is not thread safe"); return; }
    memset(c, 0, sizeof c);
    for (i = 0; i < 8; i++) { ld[i] = dwx_loader_new(); c[i].aux = ld[i]; }
    run(8, w_reg, c, 100, NULL);
    EXPECT_EQ(total_fail(c, 8), 0);
    for (i = 0; i < 8; i++) { EXPECT_EQ(dwx_refs(ld[i]), 1); dwx_release(ld[i]); }
}
