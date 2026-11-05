/* Concurrency: a shared factory used from 8 threads x 200 iterations, per-thread isolated factories, factory creation races,
 * reference counting under contention. Worker threads never call CHECK (the harness is single threaded): they record
 * failures with TCHK and a digest of every observable result; the main thread compares digests with a single-threaded run. */
#include "clt.h"

#define NTH 8
#define NIT 200

typedef struct { IDWriteFactory *f; dwx_mem_loader *ld; int only; } wl_arg;

static dwx_mem_loader *reg_syn(IDWriteFactory *f) {
    dwx_mem_loader *ld = dwx_loader_new(); size_t n; uint8_t *b = ttf_build_default(&n);
    dwx_loader_add(ld, b, n); free(b);
    if (FAILED(dwx_loader_register(ld, f))) { dwx_release(ld); return NULL; }
    return ld;
}
static void unreg_syn(dwx_mem_loader *ld) { if (!ld) return; dwx_loader_unregister(ld); dwx_release(ld); }

static void w_ops(clt_thr *t) {
    wl_arg *a = (wl_arg *)t->arg; int i;
    for (i = 0; i < t->iters; i++) {
        int op = a->only >= 0 ? a->only : i % CLT_NOPS;
        t->hash = clt_hi(t->hash, clt_op(t, a->f, op, i, a->ld));
    }
}
static void baseline(clt_thr *t, wl_arg *a, int iters) {
    memset(t, 0, sizeof *t); t->iters = iters; t->arg = a; t->hash = CLT_H0; w_ops(t);
}
static void shared_ops(int only, int nth, int iters) {
    IDWriteFactory *f = dw_shared(); wl_arg a; clt_thr th[CLT_MAXTH], base; dwx_mem_loader *ld;
    if (!f) { t_skip("no factory"); return; }
    ld = reg_syn(f);
    if (!ld) { t_skip("loader registration failed"); return; }
    a.f = f; a.ld = ld; a.only = only;
    baseline(&base, &a, iters);
    CHECKF(base.fail == 0, "single-threaded baseline failed %ld in-thread checks, first line %d: %s", (long)base.fail, base.fail_line, base.fail_expr);
    CHECK(clt_run(nth, iters, w_ops, &a, th, 0));
    clt_verify(th, nth, only >= 0 ? "op" : "mixed ops");
    {
        int i; for (i = 0; i < nth; i++) CHECKF(th[i].hash == base.hash, "thread %d digest %016llx differs from the single-threaded run %016llx", i, (unsigned long long)th[i].hash, (unsigned long long)base.hash);
    }
    unreg_syn(ld);
}
TEST(thr_shared_factory_mixed_ops_8x200) { NEED_PROC(); shared_ops(-1, NTH, NIT); }
TEST(thr_shared_factory_textformat_8x200) { NEED_PROC(); shared_ops(0, NTH, NIT); }
TEST(thr_shared_factory_layout_metrics_8x200) { NEED_PROC(); shared_ops(1, NTH, NIT); }
TEST(thr_shared_factory_collection_enumeration_8x200) { NEED_PROC(); shared_ops(2, NTH, 100); }
TEST(thr_shared_factory_findfamily_8x200) { NEED_PROC(); shared_ops(3, NTH, NIT); }
TEST(thr_shared_factory_arial_face_glyphs_8x200) { NEED_PROC(); shared_ops(4, NTH, NIT); }
TEST(thr_shared_factory_tryget_table_8x200) { NEED_PROC(); shared_ops(5, NTH, NIT); }
TEST(thr_shared_factory_custom_loader_face_8x200) { NEED_PROC(); shared_ops(6, NTH, NIT); }
TEST(thr_shared_factory_small_objects_8x200) { NEED_PROC(); shared_ops(7, NTH, NIT); }
TEST(thr_shared_factory_2_threads) { NEED_PROC(); shared_ops(-1, 2, NIT); }
TEST(thr_shared_factory_4_threads) { NEED_PROC(); shared_ops(-1, 4, NIT); }
TEST(thr_shared_factory_16_threads_short) { NEED_PROC(); shared_ops(-1, 16, 40); }

/* isolated factory per thread: factories are fully independent */
static void w_iso(clt_thr *t) {
    int i, k;
    for (i = 0; i < t->iters; i++) {
        IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
        TCHK(t, f != NULL);
        if (!f) continue;
        {
            dwx_mem_loader *ld = reg_syn(f);
            TCHK(t, ld != NULL);
            for (k = 0; k < CLT_NOPS; k++) {
                if (k == 2 && i > 0) continue;          /* the system collection of an isolated factory is built once per factory: keep it cheap */
                t->hash = clt_hi(t->hash, clt_op(t, f, k, i, ld));
            }
            if (ld) { TCHK(t, dwx_loader_unregister(ld) == S_OK); TCHK(t, dwx_refs(ld) == 1); dwx_release(ld); }
        }
        IDWriteFactory_Release(f);
    }
}
TEST(thr_isolated_factory_per_thread) {
    NEED_PROC();
    clt_thr th[CLT_MAXTH], base;
    memset(&base, 0, sizeof base); base.iters = 4; base.hash = CLT_H0; w_iso(&base);
    CHECKF(base.fail == 0, "baseline failures %ld line %d %s", (long)base.fail, base.fail_line, base.fail_expr);
    CHECK(clt_run(NTH, 4, w_iso, NULL, th, 0));
    clt_verify(th, NTH, "isolated");
    { int i; for (i = 0; i < NTH; i++) CHECK(th[i].hash == base.hash); }
}
static void w_iso_churn(clt_thr *t) {
    IDWriteFactory *keep[16]; int i, n = 0;
    for (i = 0; i < t->iters; i++) {
        IDWriteFactory *f = dw_new_factory(i & 1 ? DWRITE_FACTORY_TYPE_ISOLATED : DWRITE_FACTORY_TYPE_SHARED);
        TCHK(t, f != NULL);
        if (!f) continue;
        {   IDWriteTypography *ty = NULL; TCHK(t, IDWriteFactory_CreateTypography(f, &ty) == S_OK && ty); DW_RELEASE(ty); }
        if (n < 16) keep[n++] = f; else IDWriteFactory_Release(f);
        if (n == 16) { int k; for (k = 0; k < 16; k++) IDWriteFactory_Release(keep[k]); n = 0; }
    }
    { int k; for (k = 0; k < n; k++) IDWriteFactory_Release(keep[k]); }
}
TEST(thr_factory_creation_churn) {
    NEED_PROC();
    clt_thr th[CLT_MAXTH]; IDWriteFactory *s = dw_shared(); ULONG b = dw_refcount((IUnknown *)s);
    CHECK(clt_run(NTH, NIT, w_iso_churn, NULL, th, 0));
    clt_verify(th, NTH, "churn");
    EXPECT_REFS(s, b);                              /* every shared reference taken by the threads has been returned */
}
static void w_shared_ptr(clt_thr *t) {
    int i;
    for (i = 0; i < t->iters; i++) {
        void *p = NULL; HRESULT hr = dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &p);
        TCHK(t, hr == S_OK && p);
        if (i == 0) t->ptr = p;
        TCHK(t, p == t->ptr);
        if (p) IUnknown_Release((IUnknown *)p);
    }
}
TEST(thr_shared_factory_pointer_identity_across_threads) {
    NEED_PROC();
    clt_thr th[CLT_MAXTH]; int i; IDWriteFactory *s = dw_shared(); ULONG b = dw_refcount((IUnknown *)s);
    CHECK(clt_run(NTH, NIT, w_shared_ptr, NULL, th, 0));
    clt_verify(th, NTH, "shared pointer");
    for (i = 0; i < NTH; i++) CHECKF(th[i].ptr == (void *)s, "thread %d saw %p, main sees %p", i, th[i].ptr, (void *)s);
    EXPECT_REFS(s, b);
}
static void w_iso_ptr(clt_thr *t) {
    void *ps[50]; int i, j;
    for (i = 0; i < 50; i++) { ps[i] = NULL; TCHK(t, dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &ps[i]) == S_OK && ps[i]); }
    for (i = 0; i < 50; i++) for (j = i + 1; j < 50; j++) TCHK(t, ps[i] != ps[j]);
    t->out = (void *)(ULONG_PTR)ps[0];
    for (i = 0; i < 50; i++) if (ps[i]) IUnknown_Release((IUnknown *)ps[i]);
}
TEST(thr_isolated_factories_are_distinct_objects) {
    NEED_PROC();
    clt_thr th[CLT_MAXTH];
    CHECK(clt_run(NTH, 1, w_iso_ptr, NULL, th, 0));
    clt_verify(th, NTH, "isolated distinct");
}

/* ---- one font face shared by 8 threads ------------------------------------------------------------------------------------- */
typedef struct { IDWriteFontFace *face; int synthetic; } face_arg;
static void w_face(clt_thr *t) {
    face_arg *a = (face_arg *)t->arg; IDWriteFontFace *fc = a->face; int i;
    for (i = 0; i < t->iters; i++) {
        UINT32 cps[26]; UINT16 gi[26]; DWRITE_GLYPH_METRICS gm[26]; DWRITE_FONT_METRICS fm; int k;
        const void *d = NULL; UINT32 sz = 0; void *ctx = NULL; BOOL ex = FALSE;
        for (k = 0; k < 26; k++) cps[k] = 'A' + k;
        TCHK(t, IDWriteFontFace_GetGlyphIndices(fc, cps, 26, gi) == S_OK);
        t->hash = clt_h(t->hash, gi, sizeof gi);
        if (a->synthetic) TCHK(t, gi[0] == 2 && gi[25] == 27);
        memset(gm, 0, sizeof gm);
        TCHK(t, IDWriteFontFace_GetDesignGlyphMetrics(fc, gi, 26, gm, FALSE) == S_OK);
        for (k = 0; k < 26; k++) { t->hash = clt_hi(t->hash, gm[k].advanceWidth); t->hash = clt_hi(t->hash, gm[k].leftSideBearing); }
        if (a->synthetic) TCHK(t, gm[0].advanceWidth == 1000 && gm[25].advanceWidth == 1000);
        IDWriteFontFace_GetMetrics(fc, &fm);
        t->hash = clt_hi(t->hash, fm.designUnitsPerEm); t->hash = clt_hi(t->hash, fm.ascent); t->hash = clt_hi(t->hash, fm.descent); t->hash = clt_hi(t->hash, fm.capHeight);
        TCHK(t, IDWriteFontFace_TryGetFontTable(fc, DW_TAG('h', 'e', 'a', 'd'), &d, &sz, &ctx, &ex) == S_OK && ex && sz == 54);
        if (ex && d) { t->hash = clt_h(t->hash, d, sz); IDWriteFontFace_ReleaseFontTable(fc, ctx); }
        d = NULL; ctx = NULL; ex = FALSE;
        TCHK(t, IDWriteFontFace_TryGetFontTable(fc, DW_TAG('c', 'm', 'a', 'p'), &d, &sz, &ctx, &ex) == S_OK && ex);
        if (ex && d) { t->hash = clt_hi(t->hash, sz); IDWriteFontFace_ReleaseFontTable(fc, ctx); }
        {   /* a table that does not exist */
            d = (const void *)1; ctx = (void *)1; ex = TRUE;
            TCHK(t, IDWriteFontFace_TryGetFontTable(fc, DW_TAG('Z', 'z', 'Z', 'z'), &d, &sz, &ctx, &ex) == S_OK && !ex);
        }
        {   /* outline of one glyph into a per-thread sink */
            dwx_geom *g = dwx_geom_new(); UINT16 gl = gi[0]; float adv = 10.0f;
            HRESULT hr = IDWriteFontFace_GetGlyphRunOutline(fc, 100.0f, &gl, &adv, NULL, 1, FALSE, FALSE, (IDWriteGeometrySink *)g);
            TCHK(t, hr == S_OK && g->nfigures_begun >= 1 && g->nfigures_begun == g->nfigures_ended && g->order_violations == 0);
            t->hash = clt_hi(t->hash, g->npts);
            { float x0, y0, x1, y1; dwx_geom_bounds(g, &x0, &y0, &x1, &y1); t->hash = clt_hf(t->hash, x0); t->hash = clt_hf(t->hash, y1); }
            if (a->synthetic) TCHK(t, g->npts == 5 || g->npts == 4 || g->npts == 6);
            dwx_release(g);
        }
        {   UINT32 nf = 0; TCHK(t, IDWriteFontFace_GetFiles(fc, &nf, NULL) == S_OK && nf == 1); }
        TCHK(t, IDWriteFontFace_GetIndex(fc) == 0);
        TCHK(t, IDWriteFontFace_GetSimulations(fc) == DWRITE_FONT_SIMULATIONS_NONE);
    }
}
static void face_threads(IDWriteFontFace *fc, int synthetic, int nth, int iters) {
    face_arg a; clt_thr th[CLT_MAXTH], base; ULONG b;
    a.face = fc; a.synthetic = synthetic;
    memset(&base, 0, sizeof base); base.iters = iters; base.arg = &a; base.hash = CLT_H0; w_face(&base);
    CHECKF(base.fail == 0, "baseline failures %ld line %d %s", (long)base.fail, base.fail_line, base.fail_expr);
    b = dw_refcount((IUnknown *)fc);
    CHECK(clt_run(nth, iters, w_face, &a, th, 0));
    clt_verify(th, nth, "face");
    { int i; for (i = 0; i < nth; i++) CHECK(th[i].hash == base.hash); }
    EXPECT_REFS(fc, b);
}
TEST(thr_one_synthetic_face_8_threads) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (h.face) face_threads(h.face, 1, NTH, NIT); else t_skip("font open failed");
    EXPECT_EQ(dwx_loader_total_outstanding(h.ld) >= 0, 1);
    dwx_font_close(&h);
    DW_RELEASE(f);
}
TEST(thr_one_arial_face_8_threads) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *fc;
    if (!f) { t_skip("no factory"); return; }
    fc = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!fc) { t_skip("Arial not installed"); return; }
    face_threads(fc, 0, NTH, NIT);
    DW_RELEASE(fc);
}
TEST(thr_one_synthetic_face_16_threads_short) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (h.face) face_threads(h.face, 1, 16, 30); else t_skip("font open failed");
    dwx_font_close(&h);
    DW_RELEASE(f);
}
/* a face obtained on one thread is the same kind of object everywhere: each thread builds its own face from the shared factory */
static void w_face_each(clt_thr *t) {
    wl_arg *a = (wl_arg *)t->arg; int i;
    for (i = 0; i < t->iters; i++) {
        IDWriteFontFile *ff = NULL; IDWriteFontFace *fc = NULL; UINT32 key = 0;
        TCHK(t, IDWriteFactory_CreateCustomFontFileReference(a->f, &key, 4, (IDWriteFontFileLoader *)a->ld, &ff) == S_OK && ff);
        if (!ff) continue;
        TCHK(t, IDWriteFactory_CreateFontFace(a->f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &fc) == S_OK && fc);
        if (fc) { UINT32 cp = 'Q'; UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(fc, &cp, 1, &g); TCHK(t, g == 18); t->hash = clt_hi(t->hash, g); IDWriteFontFace_Release(fc); }
        IDWriteFontFile_Release(ff);
    }
}
TEST(thr_face_per_thread_from_one_loader) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); wl_arg a; clt_thr th[CLT_MAXTH]; dwx_mem_loader *ld; LONG r0;
    if (!f) { t_skip("no factory"); return; }
    ld = reg_syn(f);
    if (!ld) { t_skip("loader registration"); return; }
    a.f = f; a.ld = ld; a.only = -1; r0 = dwx_refs(ld);
    CHECK(clt_run(NTH, NIT, w_face_each, &a, th, 0));
    clt_verify(th, NTH, "face per thread");
    CHECKF(dwx_refs(ld) >= 1, "loader destroyed");
    t_note("loader refs before/after the run: %ld / %ld (cached files may keep references)", (long)r0, (long)dwx_refs(ld));
    unreg_syn(ld);
}

/* ---- shared immutable objects: system collection / family / font ---------------------------------------------------------- */
static void w_collection(clt_thr *t) {
    IDWriteFontCollection *c = (IDWriteFontCollection *)t->arg; int i; UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c);
    for (i = 0; i < t->iters; i++) {
        IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL; UINT32 idx = (UINT32)(i * 13) % n, found = 0; BOOL ex = FALSE; WCHAR nm[128] = { 0 };
        TCHK(t, IDWriteFontCollection_GetFontFamily(c, idx, &fam) == S_OK && fam);
        if (!fam) continue;
        TCHK(t, IDWriteFontFamily_GetFamilyNames(fam, &s) == S_OK && s);
        if (s) {
            TCHK(t, IDWriteLocalizedStrings_GetString(s, 0, nm, 128) == S_OK);
            t->hash = clt_hw(t->hash, nm);
            TCHK(t, IDWriteFontCollection_FindFamilyName(c, nm, &found, &ex) == S_OK && ex && found == idx);   /* names are unique in the collection */
            IDWriteLocalizedStrings_Release(s);
        }
        {   IDWriteFont *fn = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &fn)) && fn) {
                t->hash = clt_hi(t->hash, IDWriteFont_GetWeight(fn)); t->hash = clt_hi(t->hash, IDWriteFont_GetStyle(fn));
                IDWriteFont_Release(fn);
            } else TCHK(t, 0); }
        t->hash = clt_hi(t->hash, IDWriteFontFamily_GetFontCount(fam));
        IDWriteFontFamily_Release(fam);
    }
}
TEST(thr_one_system_collection_8_threads) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; clt_thr th[CLT_MAXTH], base; ULONG b;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE));
    if (!c || IDWriteFontCollection_GetFontFamilyCount(c) == 0) { DW_RELEASE(c); t_skip("no system fonts"); return; }
    memset(&base, 0, sizeof base); base.iters = NIT; base.arg = c; base.hash = CLT_H0; w_collection(&base);
    CHECKF(base.fail == 0, "baseline failures %ld line %d %s", (long)base.fail, base.fail_line, base.fail_expr);
    b = dw_refcount((IUnknown *)c);
    CHECK(clt_run(NTH, NIT, w_collection, c, th, 0));
    clt_verify(th, NTH, "collection");
    { int i; for (i = 0; i < NTH; i++) CHECK(th[i].hash == base.hash); }
    EXPECT_REFS(c, b);
    DW_RELEASE(c);
}
TEST(thr_custom_collection_8_threads) {
    NEED_PROC();
    clt_cfix x; IDWriteFontCollection *c = NULL; clt_thr th[CLT_MAXTH], base;
    if (!clt_cfix_open(&x, 3)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    CHECK_HR_OK(dwx_coll_create(x.cl, x.f, &c));
    if (c) {
        memset(&base, 0, sizeof base); base.iters = NIT; base.arg = c; base.hash = CLT_H0; w_collection(&base);
        CHECKF(base.fail == 0, "baseline failures %ld line %d %s", (long)base.fail, base.fail_line, base.fail_expr);
        CHECK(clt_run(NTH, NIT, w_collection, c, th, 0));
        clt_verify(th, NTH, "custom collection");
        { int i; for (i = 0; i < NTH; i++) CHECK(th[i].hash == base.hash); }
    }
    DW_RELEASE(c);
    clt_cfix_close(&x);
}
/* concurrent custom collection creation: the application's loader and enumerator are called from several threads */
typedef struct { IDWriteFactory *f; dwx_coll_loader *cl; } cc_arg;
static void w_cc(clt_thr *t) {
    cc_arg *a = (cc_arg *)t->arg; int i;
    for (i = 0; i < t->iters; i++) {
        IDWriteFontCollection *c = NULL; HRESULT hr = dwx_coll_create(a->cl, a->f, &c);
        TCHK(t, hr == S_OK && c);
        if (c) { TCHK(t, IDWriteFontCollection_GetFontFamilyCount(c) == 2); IDWriteFontCollection_Release(c); }
    }
}
TEST(thr_concurrent_custom_collection_creation) {
    NEED_PROC();
    clt_cfix x; clt_thr th[CLT_MAXTH]; cc_arg a; LONG fl0, cl0;
    if (!clt_cfix_open(&x, 2)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    a.f = x.f; a.cl = x.cl; fl0 = dwx_refs(x.fl); cl0 = dwx_refs(x.cl);
    /* the dwx doubles allocate enumerators/streams under a lock; the contract under test is dwrite's own locking */
    CHECK(clt_run(4, 10, w_cc, &a, th, 0));
    clt_verify(th, 4, "custom collection creation");
    EXPECT_EQ(dwx_refs(x.cl), cl0);
    EXPECT_EQ(dwx_refs(x.fl), fl0);
    CHECKF(x.cl->n_create >= 1, "collection loader never called");
    clt_cfix_close(&x);
}

/* ---- reference counting under contention ------------------------------------------------------------------------------------------- */
typedef struct { IUnknown *obj; int n; } rc_arg;
static void w_addref(clt_thr *t) { rc_arg *a = (rc_arg *)t->arg; int i; for (i = 0; i < a->n; i++) IUnknown_AddRef(a->obj); }
static void w_release(clt_thr *t) { rc_arg *a = (rc_arg *)t->arg; int i; for (i = 0; i < a->n; i++) IUnknown_Release(a->obj); }
static void w_addrel(clt_thr *t) { rc_arg *a = (rc_arg *)t->arg; int i; for (i = 0; i < a->n; i++) { IUnknown_AddRef(a->obj); IUnknown_Release(a->obj); } }
static void w_qirel(clt_thr *t) {
    rc_arg *a = (rc_arg *)t->arg; int i;
    for (i = 0; i < a->n; i++) { IUnknown *p = NULL; TCHK(t, IUnknown_QueryInterface(a->obj, &IID_IUnknown, (void **)&p) == S_OK && p); if (p) IUnknown_Release(p); }
}
static void hammer(IUnknown *o) {
    rc_arg a; clt_thr th[CLT_MAXTH]; ULONG b = dw_refcount(o), now;
    a.obj = o; a.n = 5000;
    CHECK(clt_run(NTH, 1, w_addrel, &a, th, 0)); clt_verify(th, NTH, "addref/release pairs");
    EXPECT_REFS(o, b);
    CHECK(clt_run(NTH, 1, w_qirel, &a, th, 0)); clt_verify(th, NTH, "qi/release pairs");
    EXPECT_REFS(o, b);
    CHECK(clt_run(NTH, 1, w_addref, &a, th, 0)); clt_verify(th, NTH, "addref");
    now = dw_refcount(o);
    CHECKF(now == b + NTH * 5000, "after %d concurrent AddRefs the count is %lu, want %lu (lost updates)", NTH * 5000, now, b + NTH * 5000);
    CHECK(clt_run(NTH, 1, w_release, &a, th, 0)); clt_verify(th, NTH, "release");
    EXPECT_REFS(o, b);
}
TEST(thr_refcount_hammer_shared_factory) { NEED_PROC(); hammer((IUnknown *)dw_shared()); }
TEST(thr_refcount_hammer_isolated_factory) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    hammer((IUnknown *)f); DW_RELEASE(f);
}
TEST(thr_refcount_hammer_text_format) {
    NEED_PROC();
    IDWriteTextFormat *fm = NULL;
    IDWriteFactory_CreateTextFormat(dw_shared(), L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fm);
    if (!fm) { t_skip("format"); return; }
    hammer((IUnknown *)fm); DW_RELEASE(fm);
}
TEST(thr_refcount_hammer_text_layout) {
    NEED_PROC();
    IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL;
    IDWriteFactory_CreateTextFormat(dw_shared(), L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(dw_shared(), L"hammer", 6, fm, 100, 100, &l);
    DW_RELEASE(fm);
    if (!l) { t_skip("layout"); return; }
    hammer((IUnknown *)l); DW_RELEASE(l);
}
TEST(thr_refcount_hammer_font_face) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    dwx_font_open_default(&h, f);
    if (h.face) hammer((IUnknown *)h.face); else t_skip("font");
    dwx_font_close(&h); DW_RELEASE(f);
}
TEST(thr_refcount_hammer_font_file) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    dwx_font_open_default(&h, f);
    if (h.file) hammer((IUnknown *)h.file); else t_skip("font");
    dwx_font_close(&h); DW_RELEASE(f);
}
TEST(thr_refcount_hammer_system_collection) {
    NEED_PROC();
    IDWriteFontCollection *c = NULL;
    IDWriteFactory_GetSystemFontCollection(dw_shared(), &c, FALSE);
    if (!c) { t_skip("collection"); return; }
    hammer((IUnknown *)c); DW_RELEASE(c);
}
TEST(thr_refcount_hammer_custom_collection) {
    NEED_PROC();
    clt_cfix x; IDWriteFontCollection *c = NULL;
    if (!clt_cfix_open(&x, 1)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    dwx_coll_create(x.cl, x.f, &c);
    if (c) hammer((IUnknown *)c);
    DW_RELEASE(c);
    clt_cfix_close(&x);
}
TEST(thr_refcount_hammer_typography_and_params) {
    NEED_PROC();
    IDWriteTypography *ty = NULL; IDWriteRenderingParams *rp = NULL; IDWriteTextAnalyzer *an = NULL;
    IDWriteFactory_CreateTypography(dw_shared(), &ty);
    IDWriteFactory_CreateRenderingParams(dw_shared(), &rp);
    IDWriteFactory_CreateTextAnalyzer(dw_shared(), &an);
    if (ty) hammer((IUnknown *)ty);
    if (rp) hammer((IUnknown *)rp);
    if (an) hammer((IUnknown *)an);
    DW_RELEASE(ty); DW_RELEASE(rp); DW_RELEASE(an);
}
/* the last Release from any of several threads destroys the object exactly once: observable through the inline object held by a layout */
typedef struct { IDWriteTextLayout *l; } lastrel_arg;
static void w_lastrel(clt_thr *t) { lastrel_arg *a = (lastrel_arg *)t->arg; (void)t; if (t->idx != 0) IDWriteTextLayout_Release(a->l); }
TEST(thr_last_release_from_worker_destroys_once) {
    NEED_PROC();
    int round;
    for (round = 0; round < 20; round++) {
        IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; dwx_inline *io = dwx_inline_new(10, 10, 8, FALSE); lastrel_arg a; clt_thr th[CLT_MAXTH]; int i;
        IDWriteFactory_CreateTextFormat(dw_shared(), L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fm);
        if (fm) IDWriteFactory_CreateTextLayout(dw_shared(), L"abc", 3, fm, 100, 100, &l);
        DW_RELEASE(fm);
        if (!l) { t_skip("layout"); dwx_release(io); return; }
        { DWRITE_TEXT_RANGE rg = { 0, 3 }; IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, rg); }
        for (i = 1; i < 4; i++) IDWriteTextLayout_AddRef(l);       /* three extra owners, dropped by threads 1..3 plus our own below */
        a.l = l;
        CHECK(clt_run(4, 1, w_lastrel, &a, th, 0));                 /* threads 1,2,3 release; thread 0 does nothing */
        EXPECT_EQ(dwx_refs(io), 2);                                 /* layout still alive: our original reference remains */
        IDWriteTextLayout_Release(l);
        EXPECT_EQ(dwx_refs(io), 1);                                 /* destroyed exactly once, releasing its inline object */
        dwx_release(io);
    }
}
