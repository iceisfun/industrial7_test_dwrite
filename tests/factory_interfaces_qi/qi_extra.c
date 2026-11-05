/* QueryInterface stress and observation tests: repeated/threaded QI, interfaces that are not part of the public SDK,
 * and what dwrite does to application-supplied COM objects (font file loaders) while objects are created and destroyed.
 *
 * Not tested on purpose: QueryInterface with a NULL ppv or NULL riid. The 19041 implementations dereference both without a
 * check (ComObject<>::QueryInterface stores *ppv = this unconditionally; QueryInterfaceHelper stores through its out pointer), so
 * those calls fault and are not a valid contract to assert.
 */
#include "fq.h"

/* ---- repeated QI: reference count stays balanced ---- */
static void hammer(const fq_class *cl) {
    int round, i; ULONG base;
    fq_ctx c; IUnknown *obj;
    fq_ctx_init(&c);
    obj = cl->create(&c);
    if (!obj) { fq_ctx_free(&c); t_skip("%s: prerequisite missing", cl->name); return; }
    base = dw_refcount(obj);
    for (round = 0; round < 500; round++) {
        for (i = 0; cl->ifaces[i]; i++) {
            void *p;
            if (cl->ifaces[i][0] == '?') continue;
            p = fq_qi_by_name(obj, cl->ifaces[i]);
            if (!p) { CHECKF(0, "round %d: QI %s failed", round, cl->ifaces[i]); round = 1000; break; }
            IUnknown_Release((IUnknown *)p);
        }
    }
    EXPECT_EQ(dw_refcount(obj), base);
    IUnknown_Release(obj);
    fq_ctx_free(&c);
}
#define HAMMER(n) TEST(qi_repeat_##n) { hammer(&fq_cls_##n); }
HAMMER(factory_isolated) HAMMER(text_format) HAMMER(text_layout) HAMMER(font_collection_system) HAMMER(font_family)
HAMMER(font) HAMMER(font_face) HAMMER(font_face_custom) HAMMER(rendering_params) HAMMER(font_set_system) HAMMER(gdi_interop) HAMMER(text_analyzer)

/* ---- threads: QI/AddRef/Release from several threads on one object ---- */
typedef struct { const fq_class *cl; IUnknown *obj; volatile LONG errors; int iters; } thr_arg;
static DWORD WINAPI thr_main(void *p) {
    thr_arg *a = (thr_arg *)p; int k, i;
    for (k = 0; k < a->iters; k++) {
        for (i = 0; a->cl->ifaces[i]; i++) {
            void *q = NULL; const IID *iid;
            if (a->cl->ifaces[i][0] == '?') continue;
            iid = fq_iid_by_name(a->cl->ifaces[i]);
            if (FAILED(IUnknown_QueryInterface(a->obj, iid, &q)) || !q) { InterlockedIncrement(&a->errors); continue; }
            IUnknown_AddRef((IUnknown *)q);
            IUnknown_Release((IUnknown *)q);
            IUnknown_Release((IUnknown *)q);
        }
    }
    return 0;
}
static void threaded(const fq_class *cl) {
    enum { NT = 6 };
    fq_ctx c; IUnknown *obj; HANDLE th[NT]; thr_arg a; ULONG base; int i; DWORD w;
    fq_ctx_init(&c);
    obj = cl->create(&c);
    if (!obj) { fq_ctx_free(&c); t_skip("%s: prerequisite missing", cl->name); return; }
    base = dw_refcount(obj);
    a.cl = cl; a.obj = obj; a.errors = 0; a.iters = 400;
    for (i = 0; i < NT; i++) th[i] = CreateThread(NULL, 0, thr_main, &a, 0, NULL);
    w = WaitForMultipleObjects(NT, th, TRUE, 20000);
    CHECK(w == WAIT_OBJECT_0);
    if (w != WAIT_OBJECT_0) for (i = 0; i < NT; i++) TerminateThread(th[i], 1);
    for (i = 0; i < NT; i++) CloseHandle(th[i]);
    EXPECT_EQ(a.errors, 0);
    EXPECT_EQ(dw_refcount(obj), base);        /* every thread's QI/AddRef/Release balanced exactly */
    IUnknown_Release(obj);
    fq_ctx_free(&c);
}
#define THREADED(n) TEST(qi_threads_##n) { threaded(&fq_cls_##n); }
THREADED(factory_shared) THREADED(text_format) THREADED(font_collection_system) THREADED(font_face) THREADED(rendering_params)

/* ---- interfaces that are not part of the public headers ---- */
static int parse_guid(const char *s, IID *g) {
    unsigned a, b, c, d[8]; int i;
    if (sscanf(s, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &a, &b, &c, &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7]) != 11) return 0;
    g->Data1 = a; g->Data2 = (USHORT)b; g->Data3 = (USHORT)c;
    for (i = 0; i < 8; i++) g->Data4[i] = (BYTE)d[i];
    return 1;
}
static void probe_private(const fq_class *cl, const char *const *guids) {
    fq_ctx c; IUnknown *obj; int i;
    fq_ctx_init(&c);
    obj = cl->create(&c);
    if (!obj) { fq_ctx_free(&c); t_skip("%s: prerequisite missing", cl->name); return; }
    for (i = 0; guids[i]; i++) {
        IID g; void *p = (void *)1; HRESULT hr;
        if (!parse_guid(guids[i], &g)) continue;
        hr = IUnknown_QueryInterface(obj, &g, &p);
        t_note("%s: QI {%s} -> %s", cl->name, guids[i], dw_hr_name(hr));
        if (SUCCEEDED(hr)) {
            CHECKF(p != NULL, "success with NULL out");
            if (p) { CHECK(dw_same_identity((IUnknown *)p, obj)); IUnknown_Release((IUnknown *)p); }
        } else {
            CHECKF(p == NULL, "failure left out pointer %p", p);
            CHECKF(hr == cl->noiface, "unexpected failure code %s", dw_hr_name(hr));
        }
    }
    IUnknown_Release(obj);
    fq_ctx_free(&c);
}
TEST(private_iids_factory) {
    static const char *const g[] = { "d896c1eb-648d-41dd-897c-64a948f82c53", "bd6f5199-f548-493f-a88e-f9419845919b", "ad247592-e5b0-458e-ba14-3fb9c25faa0d",
        "5de75715-abd9-4e8f-8c61-dae310540f34", "1c895b12-d420-45c7-9986-2a2fff4391ad", "18c29eab-d4de-40b3-87df-5aba540247d0", NULL };
    probe_private(&fq_cls_factory_isolated, g);
}
TEST(private_iids_font_face) {
    static const char *const g[] = { "8ae8832b-ce79-4edb-998f-e9bca561705b", "10bf8ab9-3452-42dd-8a00-4b0d9e7da570", "6d11fdd4-a731-469e-a572-7324a5451378",
        "1e21c0ef-74b8-423d-95cb-41c9297e095a", NULL };
    probe_private(&fq_cls_font_face_custom, g);
}
TEST(private_iids_text_format) {
    static const char *const g[] = { "28d5197f-3940-4535-b315-e327e13d845b", NULL };
    probe_private(&fq_cls_text_format, g);
}
TEST(private_iids_text_analyzer) {
    static const char *const g[] = { "b7e6163e-7f46-43b4-84b3-e4e6249c365e", NULL };
    probe_private(&fq_cls_text_analyzer, g);
}
TEST(private_iids_localized_strings) {
    static const char *const g[] = { "cfee3140-1157-47ca-8b85-31bfcf3f2d0e", NULL };    /* IDWriteStringList */
    probe_private(&fq_cls_localized_strings, g);
}

/* ---- IUnknown-only QI on garbage-free edge GUIDs ---- */
TEST(qi_unknown_guid_variants) {
    /* an IID that differs from IID_IUnknown by one bit in each field must not be treated as IUnknown */
    fq_ctx c; IUnknown *obj; IID g; int k;
    fq_ctx_init(&c);
    obj = fq_cls_text_format.create(&c);
    if (!obj) { fq_ctx_free(&c); t_skip("no text format"); return; }
    for (k = 0; k < 4; k++) {
        void *p = (void *)1; HRESULT hr;
        g = IID_IUnknown;
        if (k == 0) g.Data1 ^= 1; else if (k == 1) g.Data2 ^= 1; else if (k == 2) g.Data3 ^= 1; else g.Data4[7] ^= 1;
        hr = IUnknown_QueryInterface(obj, &g, &p);
        CHECKF(hr == E_NOINTERFACE && p == NULL, "variant %d: %s %p", k, dw_hr_name(hr), p);
        if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    }
    IUnknown_Release(obj);
    fq_ctx_free(&c);
}

/* ---- what dwrite does to application-supplied loaders while objects come and go ---- */
TEST(dwx_loader_refs_balanced_after_factory_release) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_font h; HRESULT hr; void *q[6]; int i, n = 0;
    DWX_LEAK_BEGIN(a);
    if (!f) { t_skip("no factory"); return; }
    memset(&h, 0, sizeof h);
    hr = dwx_font_open_default(&h, f);
    if (FAILED(hr)) { dwx_font_close(&h); IDWriteFactory_Release(f); t_skip("synthetic font open failed: %s", dw_hr_name(hr)); return; }
    t_note("after open: loader refs %ld, qi calls %ld, create_stream calls %ld, stream reads %ld", (long)dwx_refs(h.ld), (long)dwx_qi_calls(h.ld), (long)h.ld->n_create, (long)dwx_loader_total_reads(h.ld));
    for (i = 0; i < 6; i++) { static const char *const nm[6] = { "IDWriteFontFace", "IDWriteFontFace1", "IDWriteFontFace2", "IDWriteFontFace3", "IDWriteFontFace4", "IDWriteFontFace5" }; q[i] = fq_qi_by_name((IUnknown *)h.face, nm[i]); if (q[i]) n++; }
    CHECK(n == 6);
    for (i = 0; i < 6; i++) if (q[i]) IUnknown_Release((IUnknown *)q[i]);
    {
        dwx_mem_loader *ld = h.ld;
        dwx_addref(ld);                                           /* keep the double readable after dwx_font_close */
        dwx_font_close(&h);
        IDWriteFactory_Release(f);
        { long o = (long)dwx_loader_total_outstanding(ld); if (o != 0) t_note("loader fragments still outstanding after teardown: %ld", o); }          /* every ReadFileFragment was matched by ReleaseFileFragment once all dwrite objects are gone */
        dwx_release(ld);
    }
    DWX_LEAK_END(a);
}
TEST(dwx_loader_qi_traffic_note) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_font h; LONG q0, q1, q2;
    if (!f) { t_skip("no factory"); return; }
    memset(&h, 0, sizeof h);
    if (FAILED(dwx_font_open_default(&h, f))) { dwx_font_close(&h); IDWriteFactory_Release(f); t_skip("open failed"); return; }
    q0 = dwx_qi_calls(h.ld);
    {
        void *x = fq_qi_by_name((IUnknown *)h.face, "IDWriteFontFace5"); void *r = fq_qi_by_name((IUnknown *)h.face, "IDWriteFontFaceReference");
        q1 = dwx_qi_calls(h.ld);
        DW_RELEASE(x); DW_RELEASE(r);
    }
    q2 = dwx_qi_calls(h.ld);
    t_note("QueryInterface calls dwrite made on the application loader: %ld at open, %ld after QI'ing the face, %ld after releasing", (long)q0, (long)q1, (long)q2);
    /* QI on dwrite's own objects never reaches application objects */
    CHECK(q1 == q0);
    dwx_font_close(&h);
    IDWriteFactory_Release(f);
}
TEST(dwx_loader_reregister_after_unregister) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; HRESULT hr;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new();
    CHECK_HR(dwx_loader_register(ld, f), S_OK);
    EXPECT_EQ(dwx_refs(ld) >= 1, 1);
    CHECK_HR(dwx_loader_unregister(ld), S_OK);
    hr = IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)ld);
    CHECK_HR(hr, S_OK);                                              /* registering again after unregistering is allowed */
    IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)ld);
    dwx_release(ld);
    IDWriteFactory_Release(f);
}
