/* Threading and COM-apartment independence of DWriteCreateFactory. The factories are free-threaded objects: creation needs no
 * CoInitialize, works from STA/MTA threads, and the shared object is the same everywhere. */
#include "fc.h"

#define MAXTH 16
typedef struct {
    int idx, type, iters;
    HANDLE start;
    void *first;            /* first pointer seen (shared) */
    volatile LONG fail;
    void *ptrs[64];
    int nptrs;
    int com_mode;           /* 0 none, 1 STA, 2 MTA, 3 OleInitialize */
} tctx;

static void com_enter(int mode, int *entered) {
    HRESULT hr = S_OK;
    *entered = 0;
    if (mode == 1) hr = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    else if (mode == 2) hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    else if (mode == 3) hr = U(OleInitialize)(NULL);
    *entered = mode && SUCCEEDED(hr);
}
static void com_leave(int mode, int entered) {
    if (!entered) return;
    if (mode == 3) U(OleUninitialize)(); else U(CoUninitialize)();
}

static DWORD WINAPI worker_create(void *arg) {
    tctx *c = (tctx *)arg; int i, entered;
    com_enter(c->com_mode, &entered);
    WaitForSingleObject(c->start, 10000);
    for (i = 0; i < c->iters; i++) {
        void *p = NULL; HRESULT hr = fc_create(c->type, &IID_IDWriteFactory, &p);
        if (hr != S_OK || !p) { InterlockedIncrement(&c->fail); continue; }
        if (c->nptrs < 64) c->ptrs[c->nptrs++] = p;
        else IUnknown_Release((IUnknown *)p);
    }
    com_leave(c->com_mode, entered);
    return 0;
}
static int run_threads(int n, int type, int iters, int mode, tctx *ctx) {
    HANDLE th[MAXTH]; HANDLE start = CreateEventA(NULL, TRUE, FALSE, NULL); int i, started = 0;
    for (i = 0; i < n; i++) {
        memset(&ctx[i], 0, sizeof ctx[i]);
        ctx[i].idx = i; ctx[i].type = type; ctx[i].iters = iters; ctx[i].start = start;
        ctx[i].com_mode = mode == 4 ? (i % 4) : mode;
        th[i] = CreateThread(NULL, 0, worker_create, &ctx[i], 0, NULL);
        if (th[i]) started++;
    }
    SetEvent(start);
    for (i = 0; i < n; i++) if (th[i]) { if (WaitForSingleObject(th[i], 20000) != WAIT_OBJECT_0) { TerminateThread(th[i], 1); InterlockedIncrement(&ctx[i].fail); } CloseHandle(th[i]); }
    CloseHandle(start);
    return started;
}
static void drain(tctx *ctx, int n) { int i, j; for (i = 0; i < n; i++) for (j = 0; j < ctx[i].nptrs; j++) DW_RELEASE(ctx[i].ptrs[j]); }

static void shared_threads(int n, int iters, int mode) {
    tctx ctx[MAXTH]; int i, j, bad = 0, fails = 0; void *ref;
    NEED_EXPORT();
    ref = dw_shared();
    run_threads(n, FT_SHARED, iters, mode, ctx);
    for (i = 0; i < n; i++) { fails += ctx[i].fail; for (j = 0; j < ctx[i].nptrs; j++) if (ctx[i].ptrs[j] != ref) bad++; }
    CHECKF(fails == 0, "failed creations %d", fails);
    CHECKF(bad == 0, "pointers differing from the shared factory: %d", bad);
    drain(ctx, n);
}
static void isolated_threads(int n, int iters, int mode) {
    tctx ctx[MAXTH]; int i, j, k, l, dup = 0, fails = 0; NEED_EXPORT();
    run_threads(n, FT_ISOLATED, iters, mode, ctx);
    for (i = 0; i < n; i++) {
        fails += ctx[i].fail;
        for (j = 0; j < ctx[i].nptrs; j++)
            for (k = i; k < n; k++) for (l = (k == i ? j + 1 : 0); l < ctx[k].nptrs; l++) if (ctx[i].ptrs[j] == ctx[k].ptrs[l]) dup++;
    }
    CHECKF(fails == 0, "failed creations %d", fails);
    CHECKF(dup == 0, "duplicate isolated factory pointers: %d", dup);
    for (i = 0; i < n; i++) for (j = 0; j < ctx[i].nptrs; j++) if (ctx[i].ptrs[j]) EXPECT_REFS(ctx[i].ptrs[j], 1);
    drain(ctx, n);
}
TEST(thr_shared_2_threads) { shared_threads(2, 20, 0); }
TEST(thr_shared_4_threads) { shared_threads(4, 20, 0); }
TEST(thr_shared_8_threads) { shared_threads(8, 20, 0); }
TEST(thr_shared_16_threads) { shared_threads(16, 8, 0); }
TEST(thr_shared_sta_threads) { shared_threads(4, 10, 1); }
TEST(thr_shared_mta_threads) { shared_threads(4, 10, 2); }
TEST(thr_shared_ole_threads) { shared_threads(4, 10, 3); }
TEST(thr_shared_mixed_apartment_threads) { shared_threads(8, 10, 4); }
TEST(thr_isolated_2_threads) { isolated_threads(2, 10, 0); }
TEST(thr_isolated_4_threads) { isolated_threads(4, 10, 0); }
TEST(thr_isolated_8_threads) { isolated_threads(8, 6, 0); }
TEST(thr_isolated_16_threads) { isolated_threads(16, 3, 0); }
TEST(thr_isolated_sta_threads) { isolated_threads(4, 6, 1); }
TEST(thr_isolated_mta_threads) { isolated_threads(4, 6, 2); }
TEST(thr_isolated_mixed_apartment_threads) { isolated_threads(8, 4, 4); }

typedef struct { int type; int ok; HANDLE start; } usectx;
static DWORD WINAPI worker_use(void *arg) {
    usectx *u = (usectx *)arg; IDWriteFactory *f = NULL; int i;
    WaitForSingleObject(u->start, 10000);
    if (FAILED(fc_create(u->type, &IID_IDWriteFactory, (void **)&f)) || !f) return 1;
    for (i = 0; i < 5; i++) {
        IDWriteFontCollection *c = NULL; IDWriteRenderingParams *rp = NULL; IDWriteTextFormat *tf = NULL; IDWriteTextAnalyzer *ta = NULL;
        if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return 2;
        if (FAILED(IDWriteFactory_CreateRenderingParams(f, &rp)) || !rp) return 3;
        if (FAILED(IDWriteFactory_CreateTextAnalyzer(f, &ta)) || !ta) return 4;
        IDWriteTextFormat *t2 = NULL;
        if (FAILED(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                   DWRITE_FONT_STRETCH_NORMAL, 10.f + i, L"en-us", &tf)) || !tf) return 5;
        t2 = tf;
        IDWriteFontCollection_Release(c); IDWriteRenderingParams_Release(rp); IDWriteTextAnalyzer_Release(ta); IDWriteTextFormat_Release(t2);
    }
    IDWriteFactory_Release(f);
    u->ok = 1;
    return 0;
}
static void use_threads(int type, int n) {
    HANDLE th[MAXTH]; usectx u[MAXTH]; HANDLE start = CreateEventA(NULL, TRUE, FALSE, NULL); int i, bad = 0; NEED_EXPORT();
    for (i = 0; i < n; i++) { u[i].type = type; u[i].ok = 0; u[i].start = start; th[i] = CreateThread(NULL, 0, worker_use, &u[i], 0, NULL); }
    SetEvent(start);
    for (i = 0; i < n; i++) {
        DWORD code = 99;
        if (th[i]) {
            if (WaitForSingleObject(th[i], 30000) != WAIT_OBJECT_0) { TerminateThread(th[i], 1); bad++; }
            else { GetExitCodeThread(th[i], &code); if (code) { bad++; t_note("worker %d step %lu failed", i, code); } }
            CloseHandle(th[i]);
        } else bad++;
    }
    CloseHandle(start);
    CHECKF(bad == 0, "bad workers %d", bad);
}
TEST(thr_use_shared_immediately_4) { use_threads(FT_SHARED, 4); }
TEST(thr_use_shared_immediately_8) { use_threads(FT_SHARED, 8); }
TEST(thr_use_isolated_immediately_4) { use_threads(FT_ISOLATED, 4); }
TEST(thr_use_isolated_immediately_8) { use_threads(FT_ISOLATED, 8); }

/* Create in one thread, release in another. */
typedef struct { int type; void *p; } xctx;
static DWORD WINAPI worker_make(void *arg) { xctx *x = (xctx *)arg; return FAILED(fc_create(x->type, &IID_IDWriteFactory, &x->p)) || !x->p; }
static DWORD WINAPI worker_free(void *arg) { xctx *x = (xctx *)arg; ULONG r = IUnknown_Release((IUnknown *)x->p); x->p = NULL; return (DWORD)r; }
static void cross_thread(int type) {
    xctx x; HANDLE t; DWORD code = 99; NEED_EXPORT();
    x.type = type; x.p = NULL;
    t = CreateThread(NULL, 0, worker_make, &x, 0, NULL);
    REQUIRE(t);
    WaitForSingleObject(t, 10000); GetExitCodeThread(t, &code); CloseHandle(t);
    CHECK(code == 0 && x.p != NULL);
    if (!x.p) return;
    {
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection((IDWriteFactory *)x.p, &c, FALSE));
        DW_RELEASE(c);
    }
    t = CreateThread(NULL, 0, worker_free, &x, 0, NULL);
    WaitForSingleObject(t, 10000); GetExitCodeThread(t, &code); CloseHandle(t);
    if (type == FT_ISOLATED) CHECK(code == 0);
    else CHECK(code >= 1);
}
TEST(thr_created_in_thread_released_in_other_isolated) { cross_thread(FT_ISOLATED); }
TEST(thr_created_in_thread_released_in_other_shared) { cross_thread(FT_SHARED); }

TEST(thr_thread_exit_does_not_kill_factory) {
    xctx x; HANDLE t; DWORD code = 99; NEED_EXPORT();
    x.type = FT_ISOLATED; x.p = NULL;
    t = CreateThread(NULL, 0, worker_make, &x, 0, NULL);
    REQUIRE(t);
    WaitForSingleObject(t, 10000); GetExitCodeThread(t, &code); CloseHandle(t);   /* the creating thread is gone */
    REQUIRE(x.p);
    {
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection((IDWriteFactory *)x.p, &c, FALSE));
        DW_RELEASE(c);
    }
    EXPECT_REFS(x.p, 1);
    DW_RELEASE(x.p);
}
TEST(thr_shared_thread_exit_keeps_shared_alive) {
    xctx x; HANDLE t; DWORD code = 99; NEED_EXPORT();
    x.type = FT_SHARED; x.p = NULL;
    t = CreateThread(NULL, 0, worker_make, &x, 0, NULL);
    REQUIRE(t);
    WaitForSingleObject(t, 10000); GetExitCodeThread(t, &code); CloseHandle(t);
    REQUIRE(x.p);
    CHECK(x.p == (void *)dw_shared());
    DW_RELEASE(x.p);
}

/* COM initialisation neither changes nor is required by creation. */
typedef struct { int mode; void *before, *after_init, *after_uninit; HRESULT hr_before, hr_init; int ok_iso; } comctx;
static DWORD WINAPI worker_com(void *arg) {
    comctx *c = (comctx *)arg; HRESULT hr; IDWriteFactory *iso = NULL; IDWriteFontCollection *col = NULL;
    c->hr_before = fc_create(FT_SHARED, &IID_IDWriteFactory, &c->before);
    if (c->mode == 3) hr = U(OleInitialize)(NULL); else hr = U(CoInitializeEx)(NULL, c->mode == 1 ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
    c->hr_init = hr;
    fc_create(FT_SHARED, &IID_IDWriteFactory, &c->after_init);
    fc_create(FT_ISOLATED, &IID_IDWriteFactory, (void **)&iso);
    if (c->mode == 3) U(OleUninitialize)(); else U(CoUninitialize)();
    fc_create(FT_SHARED, &IID_IDWriteFactory, &c->after_uninit);
    if (iso && SUCCEEDED(IDWriteFactory_GetSystemFontCollection(iso, &col, FALSE)) && col) c->ok_iso = 1;     /* usable after CoUninitialize */
    DW_RELEASE(col); DW_RELEASE(iso);
    return 0;
}
static void com_case(int mode) {
    comctx c; HANDLE t; NEED_EXPORT();
    memset(&c, 0, sizeof c); c.mode = mode;
    t = CreateThread(NULL, 0, worker_com, &c, 0, NULL);
    REQUIRE(t);
    WaitForSingleObject(t, 20000); CloseHandle(t);
    CHECK(c.hr_before == S_OK);
    CHECK(SUCCEEDED(c.hr_init));
    CHECK(c.before != NULL && c.before == c.after_init && c.before == c.after_uninit);
    CHECK(c.before == (void *)dw_shared());
    CHECK(c.ok_iso);
    DW_RELEASE(c.before); DW_RELEASE(c.after_init); DW_RELEASE(c.after_uninit);
}
TEST(com_shared_same_across_sta_init) { com_case(1); }
TEST(com_shared_same_across_mta_init) { com_case(2); }
TEST(com_shared_same_across_ole_init) { com_case(3); }

CHILD(com_first_use) {
    /* In a fresh process: the very first creation happens with COM never initialised, then COM is initialised. */
    HMODULE m = LoadLibraryA("dwrite.dll"); dw_create_fn fn; void *a = NULL, *b = NULL, *c = NULL, *iso = NULL; HRESULT hr;
    if (!m) return 10;
    fn = (dw_create_fn)GetProcAddress(m, "DWriteCreateFactory"); if (!fn) return 11;
    if (fn(FT_SHARED, &IID_IDWriteFactory, (IUnknown **)&a) != S_OK || !a) return 12;
    if (fn(FT_ISOLATED, &IID_IDWriteFactory, (IUnknown **)&iso) != S_OK || !iso) return 13;
    hr = U(OleInitialize)(NULL);
    if (FAILED(hr)) return 14;
    if (fn(FT_SHARED, &IID_IDWriteFactory, (IUnknown **)&b) != S_OK || b != a) return 15;
    U(OleUninitialize)();
    if (fn(FT_SHARED, &IID_IDWriteFactory, (IUnknown **)&c) != S_OK || c != a) return 16;
    { IDWriteFontCollection *col = NULL; if (FAILED(IDWriteFactory_GetSystemFontCollection((IDWriteFactory *)iso, &col, FALSE)) || !col) return 17; IDWriteFontCollection_Release(col); }
    IUnknown_Release((IUnknown *)a); IUnknown_Release((IUnknown *)b); IUnknown_Release((IUnknown *)c); IUnknown_Release((IUnknown *)iso);
    return 0;
}
TEST(com_first_use_before_com_init_in_child) { DWORD code = t_run_child("com_first_use", "", 60000); CHECKF(code == 0, "child exit %lu", code); }

CHILD(race_first_create) {
    /* Fresh process: 16 threads race the lazy construction of the shared factory; everyone must get the same object. */
    HMODULE m = LoadLibraryA("dwrite.dll"); HANDLE th[MAXTH]; tctx ctx[MAXTH]; int i, j, n = MAXTH; void *ref = NULL;
    if (!m) return 10;
    if (!GetProcAddress(m, "DWriteCreateFactory")) return 11;
    run_threads(n, FT_SHARED, 4, 0, ctx);
    for (i = 0; i < n; i++) {
        if (ctx[i].fail) return 20;
        for (j = 0; j < ctx[i].nptrs; j++) { if (!ref) ref = ctx[i].ptrs[j]; if (ctx[i].ptrs[j] != ref) return 21; }
    }
    if (!ref) return 22;
    drain(ctx, n);
    (void)th;
    return 0;
}
TEST(thr_race_first_shared_creation_in_child) { DWORD code = t_run_child("race_first_create", "", 60000); CHECKF(code == 0, "child exit %lu", code); }
CHILD(race_mixed_types) {
    HMODULE m = LoadLibraryA("dwrite.dll"); tctx a[MAXTH], b[MAXTH]; int i, j;
    if (!m) return 10;
    run_threads(8, FT_ISOLATED, 3, 0, a);
    run_threads(8, FT_SHARED, 3, 0, b);
    for (i = 0; i < 8; i++) for (j = 0; j < b[i].nptrs; j++) if (b[i].ptrs[j] != b[0].ptrs[0]) return 21;
    for (i = 0; i < 8; i++) for (j = 0; j < a[i].nptrs; j++) if (a[i].ptrs[j] == b[0].ptrs[0]) return 22;
    drain(a, 8); drain(b, 8);
    return 0;
}
TEST(thr_race_mixed_types_in_child) { DWORD code = t_run_child("race_mixed_types", "", 60000); CHECKF(code == 0, "child exit %lu", code); }
