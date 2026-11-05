/* IDWriteFactory3::GetFontDownloadQueue, IDWriteFontDownloadQueue and IDWriteFontDownloadListener on a machine with no remote fonts.
 *
 * Win10 dwrite.dll (IDWriteFactory::GetFontDownloadQueue, IDWriteFontDownloadQueue: AddListener,
 * RemoveListener, IsEmpty, BeginDownload, CancelDownload,
 * GetGenerationCount, completion notification, listener registration):
 *   - GetFontDownloadQueue writes *queue = NULL first and always succeeds; the queue is created lazily and cached on the factory (one
 *     queue per factory object); it is a child object: the first external reference also references the factory, so it keeps the factory alive.
 *     QueryInterface answers IUnknown and IDWriteFontDownloadQueue only.
 *   - AddListener zeroes *token first; a NULL listener is E_INVALIDARG; otherwise the listener is AddRef'd and the token is (slot index + 1) of
 *     a registry that reuses the lowest free slot, so tokens on a fresh queue are 1, 2, 3, ... and a freed token is handed out again.
 *   - RemoveListener releases the listener; a token that is 0, out of range or already removed is E_INVALIDARG.
 *   - IsEmpty is TRUE when no download is pending or running. Nothing in this corpus can make it FALSE.
 *   - BeginDownload on an idle queue completes immediately on the calling thread: it increments GetGenerationCount (before any callback),
 *     then, when the context is non-NULL and implements IDWriteFontDownloadListener, calls that context's DownloadCompleted(queue, context,
 *     S_OK), then calls DownloadCompleted(queue, context, S_OK) of every registered listener in slot order, and returns S_OK. The context is not
 *     retained. CancelDownload on an idle queue returns S_OK and does not change the generation count or call anybody.
 */
#include "frr.h"

typedef struct ql_rec { IDWriteFontDownloadQueue *q; IUnknown *ctx; HRESULT hr; DWORD tid; LONG seq; UINT64 gen_inside; BOOL empty_inside; } ql_rec;
typedef struct ql {
    const void *lpVtbl; volatile LONG ref; int tag;
    ql_rec recs[64]; volatile LONG n;
    IDWriteFontDownloadQueue *probe;                 /* when set, the callback reads IsEmpty / GetGenerationCount on this queue */
    int *order_log; int *order_pos;                  /* optional shared log of tags in call order */
} ql;
static volatile LONG g_ql_seq;
static const IID *const ids_ql[] = { &IID_IUnknown, &IID_IDWriteFontDownloadListener };
static HRESULT WINAPI ql_QI(IDWriteFontDownloadListener *This, REFIID r, void **o) {
    int i; *o = NULL;
    for (i = 0; i < 2; i++) if (IsEqualIID(r, ids_ql[i])) { *o = This; IDWriteFontDownloadListener_AddRef(This); return S_OK; }
    return E_NOINTERFACE;
}
static ULONG WINAPI ql_AR(IDWriteFontDownloadListener *This) { return InterlockedIncrement(&((ql *)This)->ref); }
static ULONG WINAPI ql_RL(IDWriteFontDownloadListener *This) { return InterlockedDecrement(&((ql *)This)->ref); }
static void WINAPI ql_Done(IDWriteFontDownloadListener *This, IDWriteFontDownloadQueue *q, IUnknown *ctx, HRESULT hr) {
    ql *l = (ql *)This; LONG i = InterlockedIncrement(&l->n) - 1;
    if (l->order_log) { int p = (int)InterlockedIncrement((volatile LONG *)l->order_pos) - 1; if (p < 64) l->order_log[p] = l->tag; }
    if (i < 64) {
        l->recs[i].q = q; l->recs[i].ctx = ctx; l->recs[i].hr = hr; l->recs[i].tid = GetCurrentThreadId();
        l->recs[i].seq = InterlockedIncrement(&g_ql_seq);
        if (l->probe) { l->recs[i].gen_inside = IDWriteFontDownloadQueue_GetGenerationCount(l->probe); l->recs[i].empty_inside = IDWriteFontDownloadQueue_IsEmpty(l->probe); }
    }
}
static const IDWriteFontDownloadListenerVtbl ql_vt = { ql_QI, ql_AR, ql_RL, ql_Done };
static ql *ql_new(int tag) { ql *l = (ql *)calloc(1, sizeof *l); l->lpVtbl = &ql_vt; l->ref = 1; l->tag = tag; return l; }
#define QL(l) ((IDWriteFontDownloadListener *)(l))

/* An isolated factory with its queue. */
typedef struct qfix { IDWriteFactory *f; IDWriteFactory3 *f3; IDWriteFontDownloadQueue *q; } qfix;
static int qopen(qfix *x) {
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("no isolated factory"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory3, (void **)&x->f3)) || !x->f3) { x->f3 = NULL; t_skip("IDWriteFactory3 unsupported"); return 0; }
    if (FAILED(IDWriteFactory3_GetFontDownloadQueue(x->f3, &x->q)) || !x->q) { t_skip("GetFontDownloadQueue failed"); return 0; }
    return 1;
}
static void qclose(qfix *x) { DW_RELEASE(x->q); DW_RELEASE(x->f3); DW_RELEASE(x->f); memset(x, 0, sizeof *x); }
#define QOPEN(x) qfix x; if (!qopen(&x)) { qclose(&x); return; }

/* ---- acquisition ---------------------------------------------------------------------------------------------------------- */
TEST(get_queue_basic) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontDownloadQueue *q = (IDWriteFontDownloadQueue *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory3_GetFontDownloadQueue(f3, &q), S_OK);
    CHECK(q != NULL && q != (IDWriteFontDownloadQueue *)(ULONG_PTR)8);
    if (q == (IDWriteFontDownloadQueue *)(ULONG_PTR)8) q = NULL;
    DW_RELEASE(q);
}
TEST(get_queue_is_a_singleton_per_factory) {
    QOPEN(x);
    IDWriteFontDownloadQueue *a = NULL, *b = NULL;
    CHECK_HR(IDWriteFactory3_GetFontDownloadQueue(x.f3, &a), S_OK);
    CHECK_HR(IDWriteFactory3_GetFontDownloadQueue(x.f3, &b), S_OK);
    CHECK(a == x.q && b == x.q);
    ULONG base = dw_refcount((IUnknown *)x.q);
    IDWriteFontDownloadQueue *c = NULL;
    IDWriteFactory3_GetFontDownloadQueue(x.f3, &c);
    EXPECT_REFS(x.q, base + 1);
    DW_RELEASE(c); DW_RELEASE(a); DW_RELEASE(b);
    qclose(&x);
}
TEST(get_queue_differs_between_factories) {
    QOPEN(x);
    qfix y;
    if (qopen(&y)) {
        CHECK(x.q != y.q);
        IDWriteFactory3 *sh = frr_shared3(); IDWriteFontDownloadQueue *s = NULL;
        if (sh && SUCCEEDED(IDWriteFactory3_GetFontDownloadQueue(sh, &s))) { CHECK(s != x.q && s != y.q); }
        DW_RELEASE(s);
    }
    qclose(&y); qclose(&x);
}
TEST(get_queue_shared_factory_is_stable) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontDownloadQueue *a = NULL, *b = NULL;
    IDWriteFactory3_GetFontDownloadQueue(f3, &a); IDWriteFactory3_GetFontDownloadQueue(f3, &b);
    CHECK(a && a == b);
    DW_RELEASE(a); DW_RELEASE(b);
    IDWriteFontDownloadQueue *c = NULL;
    IDWriteFactory3_GetFontDownloadQueue(f3, &c);
    CHECK(c != NULL);       /* releasing every external reference does not destroy the factory-owned queue */
    DW_RELEASE(c);
}
TEST(get_queue_through_newer_factory_interfaces) {
    QOPEN(x);
    int v;
    for (v = 4; v <= 7; v++) {
        void *fv = NULL;
        if (FAILED(IDWriteFactory_QueryInterface(x.f, dw_factory_iid(v), &fv)) || !fv) { t_note("IDWriteFactory%d unsupported", v); continue; }
        IDWriteFontDownloadQueue *q = NULL;
        CHECK_HR(IDWriteFactory3_GetFontDownloadQueue((IDWriteFactory3 *)fv, &q), S_OK);
        t_check(q == x.q, __FILE__, __LINE__, "same queue", "IDWriteFactory%d", v);
        DW_RELEASE(q); IUnknown_Release((IUnknown *)fv);
    }
    qclose(&x);
}
TEST(queue_qi_matrix) {
    QOPEN(x);
    dw_qi_row rows[] = {
        DW_QI_ROW(IDWriteFontDownloadQueue, 1), DW_QI_ROW(IDWriteFontDownloadListener, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFactory3, 0),
        DW_QI_ROW(IDWriteFontFaceReference, 0), DW_QI_ROW(IDWriteFontResource, 0), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFontFile, 0),
        DW_QI_ROW(IDWriteLocalizedStrings, 0), DW_QI_ROW(IDWriteFontFileLoader, 0),
    };
    DW_CHECK_QI(x.q, rows);
    qclose(&x);
}
TEST(queue_qi_failure_zeroes_output) {
    QOPEN(x);
    void *pv = (void *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFontDownloadQueue_QueryInterface(x.q, &IID_IDWriteFactory, &pv), E_NOINTERFACE);
    FRR_ZERO(pv);
    qclose(&x);
}
TEST(queue_refcounts) {
    QOPEN(x);
    ULONG b = dw_refcount((IUnknown *)x.q);
    ULONG a1 = IDWriteFontDownloadQueue_AddRef(x.q), a2 = IDWriteFontDownloadQueue_AddRef(x.q);
    EXPECT_EQ(a1, b + 1); EXPECT_EQ(a2, b + 2);
    ULONG r1 = IDWriteFontDownloadQueue_Release(x.q), r2 = IDWriteFontDownloadQueue_Release(x.q);
    EXPECT_EQ(r1, b + 1); EXPECT_EQ(r2, b);
    qclose(&x);
}
TEST(queue_keeps_factory_alive) {
    QOPEN(x);
    IDWriteFontDownloadQueue *q = x.q; x.q = NULL;
    DW_RELEASE(x.f3); DW_RELEASE(x.f);            /* the only reference left is the one on the queue, which pins the factory */
    EXPECT_EQ(IDWriteFontDownloadQueue_IsEmpty(q), TRUE);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(q, NULL), S_OK);
    EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(q) >= 1, TRUE);
    DW_RELEASE(q);
}

/* ---- idle state ------------------------------------------------------------------------------------------------------------- */
TEST(idle_is_empty_true) {
    QOPEN(x);
    int k;
    for (k = 0; k < 5; k++) { BOOL e = IDWriteFontDownloadQueue_IsEmpty(x.q); EXPECT_EQ(e, 1); }
    qclose(&x);
}
TEST(idle_generation_is_stable) {
    QOPEN(x);
    UINT64 g0 = IDWriteFontDownloadQueue_GetGenerationCount(x.q); int k;
    t_note("fresh queue generation count = %llu", (unsigned long long)g0);
    for (k = 0; k < 10; k++) EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(x.q), g0);
    qclose(&x);
}
TEST(idle_generation_unaffected_by_reference_use) {
    FRR_OPEN(x);
    IDWriteFontDownloadQueue *q = NULL;
    IDWriteFactory3_GetFontDownloadQueue(x.f3, &q);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (q && r) {
        UINT64 g0 = IDWriteFontDownloadQueue_GetGenerationCount(q);
        IDWriteFontFace3 *face = NULL;
        IDWriteFontFaceReference_CreateFontFace(r, &face);
        IDWriteFontFaceReference_GetFileSize(r);
        IDWriteFontFaceReference_EnqueueFontDownloadRequest(r);
        DW_RELEASE(face);
        EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(q), g0);
        EXPECT_EQ(IDWriteFontDownloadQueue_IsEmpty(q), TRUE);
    }
    DW_RELEASE(r); DW_RELEASE(q);
    frr_close(&x);
}

/* ---- listeners ----------------------------------------------------------------------------------------------------------------- */
TEST(addlistener_null_is_invalidarg_and_zeroes_token) {
    QOPEN(x);
    UINT32 tok = 0xABCD;
    CHECK_HR(IDWriteFontDownloadQueue_AddListener(x.q, NULL, &tok), FRR_EINVALIDARG);
    EXPECT_EQ(tok, 0);
    qclose(&x);
}
TEST(addlistener_basic_addrefs_listener) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0;
    CHECK_HR(IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok), S_OK);
    EXPECT_EQ(tok, 1);
    EXPECT_EQ(l->ref, 2);
    CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(x.q, tok), S_OK);
    EXPECT_EQ(l->ref, 1);
    qclose(&x); free(l);
}
TEST(addlistener_tokens_are_sequential_from_one) {
    QOPEN(x);
    ql *l[5]; UINT32 tok[5]; int i;
    for (i = 0; i < 5; i++) { l[i] = ql_new(i); tok[i] = 0; CHECK_HR(IDWriteFontDownloadQueue_AddListener(x.q, QL(l[i]), &tok[i]), S_OK); t_check(tok[i] == (UINT32)i + 1, __FILE__, __LINE__, "token", "#%d got %u", i, tok[i]); }
    for (i = 0; i < 5; i++) CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(x.q, tok[i]), S_OK);
    for (i = 0; i < 5; i++) { EXPECT_EQ(l[i]->ref, 1); }
    qclose(&x);
    for (i = 0; i < 5; i++) free(l[i]);
}
TEST(addlistener_lowest_free_token_is_reused) {
    QOPEN(x);
    ql *a = ql_new(1), *b = ql_new(2), *c = ql_new(3), *d = ql_new(4), *e = ql_new(5);
    UINT32 ta = 0, tb = 0, tc = 0, td = 0, te = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(a), &ta); IDWriteFontDownloadQueue_AddListener(x.q, QL(b), &tb); IDWriteFontDownloadQueue_AddListener(x.q, QL(c), &tc);
    EXPECT_EQ(ta, 1); EXPECT_EQ(tb, 2); EXPECT_EQ(tc, 3);
    CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(x.q, tb), S_OK);
    CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(x.q, ta), S_OK);
    CHECK_HR(IDWriteFontDownloadQueue_AddListener(x.q, QL(d), &td), S_OK);
    EXPECT_EQ(td, 1);
    CHECK_HR(IDWriteFontDownloadQueue_AddListener(x.q, QL(e), &te), S_OK);
    EXPECT_EQ(te, 2);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tc); IDWriteFontDownloadQueue_RemoveListener(x.q, td); IDWriteFontDownloadQueue_RemoveListener(x.q, te);
    qclose(&x); free(a); free(b); free(c); free(d); free(e);
}
TEST(addlistener_same_listener_twice) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 t1 = 0, t2 = 0;
    CHECK_HR(IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &t1), S_OK);
    CHECK_HR(IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &t2), S_OK);
    CHECK(t1 != 0 && t2 != 0 && t1 != t2);
    EXPECT_EQ(l->ref, 3);
    IDWriteFontDownloadQueue_RemoveListener(x.q, t1);
    EXPECT_EQ(l->ref, 2);
    IDWriteFontDownloadQueue_RemoveListener(x.q, t2);
    EXPECT_EQ(l->ref, 1);
    qclose(&x); free(l);
}
TEST(removelistener_invalid_tokens) {
    QOPEN(x);
    static const UINT32 bad[] = { 0, 1, 2, 3, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteFontDownloadQueue_RemoveListener(x.q, bad[i]);
        t_check(hr == FRR_EINVALIDARG, __FILE__, __LINE__, "empty registry", "token %u -> %s", bad[i], dw_hr_name(hr));
    }
    ql *l = ql_new(1); UINT32 tok = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (bad[i] == tok) continue;
        HRESULT hr = IDWriteFontDownloadQueue_RemoveListener(x.q, bad[i]);
        t_check(hr == FRR_EINVALIDARG, __FILE__, __LINE__, "unknown token", "token %u -> %s", bad[i], dw_hr_name(hr));
    }
    EXPECT_EQ(l->ref, 2);                        /* failed removals did not release the live listener */
    CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(x.q, tok), S_OK);
    qclose(&x); free(l);
}
TEST(removelistener_twice_second_is_invalidarg) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(x.q, tok), S_OK);
    CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(x.q, tok), FRR_EINVALIDARG);
    EXPECT_EQ(l->ref, 1);
    qclose(&x); free(l);
}
TEST(queue_teardown_releases_registered_listeners) {
    QOPEN(x);
    ql *a = ql_new(1), *b = ql_new(2); UINT32 ta = 0, tb = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(a), &ta); IDWriteFontDownloadQueue_AddListener(x.q, QL(b), &tb);
    EXPECT_EQ(a->ref, 2); EXPECT_EQ(b->ref, 2);
    qclose(&x);                                  /* last factory and queue references gone: the queue destructor releases its listeners */
    EXPECT_EQ(a->ref, 1); EXPECT_EQ(b->ref, 1);
    free(a); free(b);
}
TEST(listener_object_without_queue_use_is_not_called) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    IDWriteTextFormat *tf = NULL;
    IDWriteFactory_CreateTextFormat(x.f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf);
    DW_RELEASE(tf);
    IDWriteFontCollection *fc = NULL;
    IDWriteFactory_GetSystemFontCollection(x.f, &fc, FALSE);
    DW_RELEASE(fc);
    EXPECT_EQ(l->n, 0);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    qclose(&x); free(l);
}

/* ---- BeginDownload on an idle queue ----------------------------------------------------------------------------------------------- */
TEST(begin_download_without_listeners) {
    QOPEN(x);
    UINT64 g = IDWriteFontDownloadQueue_GetGenerationCount(x.q);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, NULL), S_OK);
    EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(x.q), g + 1);
    EXPECT_EQ(IDWriteFontDownloadQueue_IsEmpty(x.q), TRUE);
    qclose(&x);
}
TEST(begin_download_null_context_notifies_registered_listener) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0; l->probe = x.q;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    UINT64 g = IDWriteFontDownloadQueue_GetGenerationCount(x.q);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, NULL), S_OK);
    EXPECT_EQ(l->n, 1);                           /* synchronous: already delivered when the call returns */
    if (l->n >= 1) {
        EXPECT_EQ(l->recs[0].q, x.q);
        EXPECT_EQ(l->recs[0].ctx, NULL);
        EXPECT_EQ(l->recs[0].hr, S_OK);
        EXPECT_EQ(l->recs[0].tid, GetCurrentThreadId());
        EXPECT_EQ(l->recs[0].gen_inside, g + 1);  /* the generation is bumped before the callbacks run */
        EXPECT_EQ(l->recs[0].empty_inside, TRUE); /* and the queue can be queried from inside the callback */
    }
    EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(x.q), g + 1);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    qclose(&x); free(l);
}
TEST(begin_download_passes_context_pointer_unchanged) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0;
    dwx_effect *ctx = dwx_effect_new(7);
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, (IUnknown *)ctx), S_OK);
    EXPECT_EQ(l->n, 1);
    if (l->n >= 1) EXPECT_EQ(l->recs[0].ctx, (IUnknown *)ctx);
    EXPECT_EQ(dwx_refs(ctx), 1);                  /* the context is not retained */
    EXPECT_EQ(dwx_destroyed(ctx), 0);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    dwx_release(ctx);
    qclose(&x); free(l);
}
TEST(begin_download_context_without_listener_interface_is_still_passed) {
    QOPEN(x);
    dwx_effect *ctx = dwx_effect_new(1);
    dwx_listener *l = dwx_listener_new(); UINT32 tok = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, (IDWriteFontDownloadListener *)l, &tok);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, (IUnknown *)ctx), S_OK);
    EXPECT_EQ(l->n_completed, 1);
    EXPECT_EQ(l->last_context, (IUnknown *)ctx);
    EXPECT_EQ(l->last_queue, x.q);
    EXPECT_EQ(l->last_hr, S_OK);
    EXPECT_EQ(dwx_qi_calls(ctx) >= 1, TRUE);     /* the queue asks the context whether it is a listener */
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    dwx_release(l); dwx_release(ctx);
    qclose(&x);
}
TEST(begin_download_context_listener_is_called_before_registered_ones) {
    QOPEN(x);
    int log[64], pos = 0; memset(log, 0xFF, sizeof log);
    ql *ctx = ql_new(100), *a = ql_new(1), *b = ql_new(2), *c = ql_new(3);
    ctx->order_log = a->order_log = b->order_log = c->order_log = log; ctx->order_pos = a->order_pos = b->order_pos = c->order_pos = &pos;
    UINT32 ta = 0, tb = 0, tc = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(a), &ta); IDWriteFontDownloadQueue_AddListener(x.q, QL(b), &tb); IDWriteFontDownloadQueue_AddListener(x.q, QL(c), &tc);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, (IUnknown *)ctx), S_OK);
    EXPECT_EQ(pos, 4);
    if (pos == 4) { EXPECT_EQ(log[0], 100); EXPECT_EQ(log[1], 1); EXPECT_EQ(log[2], 2); EXPECT_EQ(log[3], 3); }
    EXPECT_EQ(ctx->n, 1);
    if (ctx->n >= 1) { EXPECT_EQ(ctx->recs[0].ctx, (IUnknown *)ctx); EXPECT_EQ(ctx->recs[0].q, x.q); EXPECT_EQ(ctx->recs[0].hr, S_OK); }
    EXPECT_EQ(ctx->ref, 1);
    IDWriteFontDownloadQueue_RemoveListener(x.q, ta); IDWriteFontDownloadQueue_RemoveListener(x.q, tb); IDWriteFontDownloadQueue_RemoveListener(x.q, tc);
    qclose(&x); free(ctx); free(a); free(b); free(c);
}
TEST(begin_download_context_that_is_also_registered_is_called_twice) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, (IUnknown *)l), S_OK);
    EXPECT_EQ(l->n, 2);
    if (l->n >= 2) { EXPECT_EQ(l->recs[0].ctx, (IUnknown *)l); EXPECT_EQ(l->recs[1].ctx, (IUnknown *)l); CHECK(l->recs[0].seq < l->recs[1].seq); }
    EXPECT_EQ(l->ref, 2);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    EXPECT_EQ(l->ref, 1);
    qclose(&x); free(l);
}
TEST(begin_download_listeners_called_in_slot_order) {
    QOPEN(x);
    int log[64], pos = 0; memset(log, 0xFF, sizeof log);
    ql *a = ql_new(1), *b = ql_new(2), *c = ql_new(3), *d = ql_new(4);
    a->order_log = b->order_log = c->order_log = d->order_log = log; a->order_pos = b->order_pos = c->order_pos = d->order_pos = &pos;
    UINT32 ta = 0, tb = 0, tc = 0, td = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(a), &ta); IDWriteFontDownloadQueue_AddListener(x.q, QL(b), &tb); IDWriteFontDownloadQueue_AddListener(x.q, QL(c), &tc);
    IDWriteFontDownloadQueue_RemoveListener(x.q, ta);                    /* slot 0 becomes free ... */
    IDWriteFontDownloadQueue_AddListener(x.q, QL(d), &td);               /* ... and is reused by d */
    EXPECT_EQ(td, 1);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, NULL), S_OK);
    EXPECT_EQ(pos, 3);
    if (pos == 3) { EXPECT_EQ(log[0], 4); EXPECT_EQ(log[1], 2); EXPECT_EQ(log[2], 3); }
    EXPECT_EQ(a->n, 0);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tb); IDWriteFontDownloadQueue_RemoveListener(x.q, tc); IDWriteFontDownloadQueue_RemoveListener(x.q, td);
    qclose(&x); free(a); free(b); free(c); free(d);
}
TEST(begin_download_removed_listener_is_not_called) {
    QOPEN(x);
    ql *a = ql_new(1), *b = ql_new(2); UINT32 ta = 0, tb = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(a), &ta); IDWriteFontDownloadQueue_AddListener(x.q, QL(b), &tb);
    IDWriteFontDownloadQueue_BeginDownload(x.q, NULL);
    EXPECT_EQ(a->n, 1); EXPECT_EQ(b->n, 1);
    IDWriteFontDownloadQueue_RemoveListener(x.q, ta);
    IDWriteFontDownloadQueue_BeginDownload(x.q, NULL);
    EXPECT_EQ(a->n, 1); EXPECT_EQ(b->n, 2);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tb);
    IDWriteFontDownloadQueue_BeginDownload(x.q, NULL);
    EXPECT_EQ(a->n, 1); EXPECT_EQ(b->n, 2);
    qclose(&x); free(a); free(b);
}
TEST(begin_download_repeated_counts_generations) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0; int i;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    UINT64 g = IDWriteFontDownloadQueue_GetGenerationCount(x.q);
    for (i = 0; i < 50; i++) {
        dwx_effect *ctx = dwx_effect_new(i);
        t_check(IDWriteFontDownloadQueue_BeginDownload(x.q, (IUnknown *)ctx) == S_OK, __FILE__, __LINE__, "BeginDownload", "#%d", i);
        dwx_release(ctx);
    }
    EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(x.q), g + 50);
    EXPECT_EQ(l->n, 50);
    for (i = 1; i < 50 && i < 64; i++) t_check(l->recs[i].seq > l->recs[i - 1].seq, __FILE__, __LINE__, "order", "%d", i);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    qclose(&x); free(l);
}
TEST(begin_download_generation_seen_by_each_callback_increases) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0; l->probe = x.q; int i;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    UINT64 g = IDWriteFontDownloadQueue_GetGenerationCount(x.q);
    for (i = 0; i < 5; i++) IDWriteFontDownloadQueue_BeginDownload(x.q, NULL);
    EXPECT_EQ(l->n, 5);
    for (i = 0; i < 5 && i < l->n; i++) t_check(l->recs[i].gen_inside == g + (UINT64)i + 1, __FILE__, __LINE__, "gen inside", "#%d got %llu want %llu", i, (unsigned long long)l->recs[i].gen_inside, (unsigned long long)(g + i + 1));
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    qclose(&x); free(l);
}
TEST(begin_download_signals_listener_event) {
    QOPEN(x);
    dwx_listener *l = dwx_listener_new(); UINT32 tok = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, (IDWriteFontDownloadListener *)l, &tok);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, NULL), S_OK);
    DWORD wr = WaitForSingleObject(l->event, 0);                        /* auto-reset event: wait only once */
    EXPECT_EQ(wr, WAIT_OBJECT_0);        /* delivered before the call returned */
    EXPECT_EQ(l->n_completed, 1);
    EXPECT_EQ(l->last_queue, x.q);
    EXPECT_EQ(l->last_context, NULL);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    dwx_release(l);
    qclose(&x);
}
TEST(begin_download_on_shared_factory_queue_with_temporary_listener) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontDownloadQueue *q = NULL; ql *l = ql_new(1); UINT32 tok = 0;
    if (SUCCEEDED(IDWriteFactory3_GetFontDownloadQueue(f3, &q)) && q) {
        UINT64 g = IDWriteFontDownloadQueue_GetGenerationCount(q);
        if (IDWriteFontDownloadQueue_IsEmpty(q)) {
            CHECK_HR(IDWriteFontDownloadQueue_AddListener(q, QL(l), &tok), S_OK);
            CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(q, NULL), S_OK);
            EXPECT_EQ(l->n, 1);
            CHECK(IDWriteFontDownloadQueue_GetGenerationCount(q) >= g + 1);
            CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(q, tok), S_OK);
        }
    }
    EXPECT_EQ(l->ref, 1);
    DW_RELEASE(q); free(l);
}

/* ---- threads ----------------------------------------------------------------------------------------------------------------------- */
typedef struct { IDWriteFontDownloadQueue *q; int n; volatile LONG failures; } bd_arg;
static DWORD WINAPI bd_thread(LPVOID p) {
    bd_arg *a = (bd_arg *)p; int i;
    for (i = 0; i < a->n; i++) if (IDWriteFontDownloadQueue_BeginDownload(a->q, NULL) != S_OK) InterlockedIncrement(&a->failures);
    return 0;
}
TEST(begin_download_from_several_threads) {
    QOPEN(x);
    dwx_listener *l = dwx_listener_new(); UINT32 tok = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, (IDWriteFontDownloadListener *)l, &tok);
    enum { NT = 4, PER = 25 };
    HANDLE th[NT]; bd_arg arg = { x.q, PER, 0 }; int i, started = 0;
    UINT64 g = IDWriteFontDownloadQueue_GetGenerationCount(x.q);
    for (i = 0; i < NT; i++) { th[i] = CreateThread(NULL, 0, bd_thread, &arg, 0, NULL); if (th[i]) started++; }
    for (i = 0; i < NT; i++) if (th[i]) { if (WaitForSingleObject(th[i], 10000) != WAIT_OBJECT_0) t_check(0, __FILE__, __LINE__, "thread finished", "thread %d timed out", i); CloseHandle(th[i]); }
    EXPECT_EQ(started, NT);
    EXPECT_EQ(arg.failures, 0);
    EXPECT_EQ(l->n_completed, NT * PER);
    EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(x.q), g + NT * PER);
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    dwx_release(l);
    qclose(&x);
}

/* ---- CancelDownload -------------------------------------------------------------------------------------------------------------------- */
typedef struct { IDWriteFontDownloadQueue *q; HRESULT hr; } cancel_arg;
static DWORD WINAPI cancel_thread(LPVOID p) { cancel_arg *a = (cancel_arg *)p; a->hr = IDWriteFontDownloadQueue_CancelDownload(a->q); return 0; }
/* CancelDownload waits for the (idle) download thread; run it on a helper thread so a hang is reported instead of stalling the run. */
static int cancel_bounded(IDWriteFontDownloadQueue *q, HRESULT *hr) {
    cancel_arg a = { q, (HRESULT)0xC0DEC0DE }; HANDLE h = CreateThread(NULL, 0, cancel_thread, &a, 0, NULL);
    if (!h) return 0;
    if (WaitForSingleObject(h, 5000) != WAIT_OBJECT_0) { CloseHandle(h); return 0; }
    CloseHandle(h); *hr = a.hr; return 1;
}
TEST(cancel_download_idle_is_ok_and_silent) {
    QOPEN(x);
    ql *l = ql_new(1); UINT32 tok = 0;
    IDWriteFontDownloadQueue_AddListener(x.q, QL(l), &tok);
    UINT64 g = IDWriteFontDownloadQueue_GetGenerationCount(x.q);
    HRESULT hr = 0;
    if (cancel_bounded(x.q, &hr)) {
        CHECK_HR(hr, S_OK);
        EXPECT_EQ(l->n, 0);
        EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(x.q), g);
        EXPECT_EQ(IDWriteFontDownloadQueue_IsEmpty(x.q), TRUE);
    } else t_check(0, __FILE__, __LINE__, "CancelDownload returns", "did not return within 5 s on an idle queue");
    IDWriteFontDownloadQueue_RemoveListener(x.q, tok);
    qclose(&x); free(l);
}
TEST(cancel_download_repeated_and_after_begin) {
    QOPEN(x);
    HRESULT hr = 0; int k;
    for (k = 0; k < 3; k++) { if (!cancel_bounded(x.q, &hr)) { t_check(0, __FILE__, __LINE__, "CancelDownload returns", "round %d", k); break; } CHECK_HR(hr, S_OK); }
    UINT64 g = IDWriteFontDownloadQueue_GetGenerationCount(x.q);
    CHECK_HR(IDWriteFontDownloadQueue_BeginDownload(x.q, NULL), S_OK);
    EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(x.q), g + 1);
    if (cancel_bounded(x.q, &hr)) { CHECK_HR(hr, S_OK); EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(x.q), g + 1); }
    else t_check(0, __FILE__, __LINE__, "CancelDownload returns", "after BeginDownload");
    qclose(&x);
}
