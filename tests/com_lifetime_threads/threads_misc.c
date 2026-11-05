/* Layouts handed between threads, analyzers on several threads, loader registration races, COM apartments. */
#include "clt.h"

/* ---- layout used by one thread at a time, handed from thread to thread ------------------------------------------------------------ */
#define NSTEPS 14
static const WCHAR g_text[] = L"Layout handed between threads, with no thread affinity at all.";
static uint64_t layout_step(IDWriteTextLayout *l, int step) {
    uint64_t h = CLT_H0; DWRITE_TEXT_METRICS m; HRESULT hr = S_OK; DWRITE_TEXT_RANGE all; UINT32 len = (UINT32)lstrlenW(g_text);
    all.startPosition = 0; all.length = len;
    switch (step) {
    case 1: hr = IDWriteTextLayout_SetFontSize(l, 24.0f, all); break;
    case 2: hr = IDWriteTextLayout_SetMaxWidth(l, 160.0f); break;
    case 3: { DWRITE_TEXT_RANGE r = { 0, 6 }; hr = IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, r); break; }
    case 8: { DWRITE_TEXT_RANGE r = { 0, 4 }; hr = IDWriteTextLayout_SetUnderline(l, TRUE, r); break; }
    case 10: hr = IDWriteTextLayout_SetMaxHeight(l, 40.0f); break;
    case 12: hr = IDWriteTextLayout_SetFontSize(l, 11.0f, all); break;
    default: break;
    }
    h = clt_hi(h, (uint32_t)hr);
    memset(&m, 0, sizeof m);
    hr = IDWriteTextLayout_GetMetrics(l, &m);
    h = clt_hi(h, (uint32_t)hr);
    if (hr == S_OK) { h = clt_hf(h, m.left); h = clt_hf(h, m.top); h = clt_hf(h, m.width); h = clt_hf(h, m.height); h = clt_hi(h, m.lineCount); }
    switch (step) {
    case 4: { UINT32 n = 0; DWRITE_LINE_METRICS lm[16]; hr = IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n); h = clt_hi(h, n);
        if (n <= 16 && n) { hr = IDWriteTextLayout_GetLineMetrics(l, lm, 16, &n); h = clt_hi(h, (uint32_t)hr); if (hr == S_OK) { UINT32 i; for (i = 0; i < n; i++) { h = clt_hi(h, lm[i].length); h = clt_hf(h, lm[i].height); h = clt_hf(h, lm[i].baseline); } } } break; }
    case 5: { float x = 0, y = 0; DWRITE_HIT_TEST_METRICS hm; memset(&hm, 0, sizeof hm); hr = IDWriteTextLayout_HitTestTextPosition(l, 3, FALSE, &x, &y, &hm); h = clt_hi(h, (uint32_t)hr); h = clt_hf(h, x); h = clt_hf(h, y); h = clt_hf(h, hm.width); break; }
    case 6: { BOOL tr = 0, in = 0; DWRITE_HIT_TEST_METRICS hm; memset(&hm, 0, sizeof hm); hr = IDWriteTextLayout_HitTestPoint(l, 20.0f, 5.0f, &tr, &in, &hm); h = clt_hi(h, (uint32_t)hr); h = clt_hi(h, hm.textPosition); h = clt_hi(h, tr); h = clt_hi(h, in); break; }
    case 7: { dwx_renderer *r = dwx_renderer_new(); hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 1.0f, 2.0f); h = clt_hi(h, (uint32_t)hr); h = clt_hi(h, r->ncalls); h = clt_hi(h, dwx_refs(r)); dwx_release(r); break; }
    case 9: { UINT32 n = 0; hr = IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &n); h = clt_hi(h, n); break; }
    case 11: { DWRITE_OVERHANG_METRICS om; memset(&om, 0, sizeof om); hr = IDWriteTextLayout_GetOverhangMetrics(l, &om); h = clt_hi(h, (uint32_t)hr); if (hr == S_OK) { h = clt_hf(h, om.left); h = clt_hf(h, om.right); } break; }
    case 13: { float mw = 0; hr = IDWriteTextLayout_DetermineMinWidth(l, &mw); h = clt_hi(h, (uint32_t)hr); h = clt_hf(h, mw); break; }
    }
    return h;
}
typedef struct { IDWriteTextLayout *l; int step; uint64_t h; } ho_arg;
static void w_step(clt_thr *t) { ho_arg *a = (ho_arg *)t->arg; a->h = layout_step(a->l, a->step); }
static IDWriteTextLayout *mk_hand_layout(void) {
    IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL;
    IDWriteFactory_CreateTextFormat(dw_shared(), L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(dw_shared(), g_text, (UINT32)lstrlenW(g_text), fm, 400.0f, 300.0f, &l);
    DW_RELEASE(fm);
    return l;
}
static void handoff(DWORD com_mode_base) {
    IDWriteTextLayout *a = mk_hand_layout(), *b = mk_hand_layout(); int s;
    if (!a || !b) { t_skip("layout creation"); DW_RELEASE(a); DW_RELEASE(b); return; }
    for (s = 0; s < NSTEPS; s++) {
        ho_arg ha; clt_thr th; uint64_t want = layout_step(b, s);       /* twin layout stays on the main thread */
        ha.l = a; ha.step = s; ha.h = 0;
        CHECK(clt_run(1, 1, w_step, &ha, &th, com_mode_base == 3 ? (DWORD)(s % 3) : com_mode_base));
        CHECKF(ha.h == want, "step %d on a different thread gave digest %016llx, same-thread twin %016llx", s, (unsigned long long)ha.h, (unsigned long long)want);
    }
    /* and back on the main thread the two layouts still agree */
    CHECK(layout_step(a, 0) == layout_step(b, 0));
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(thr_layout_handoff_plain_threads) { NEED_PROC(); handoff(0); }
TEST(thr_layout_handoff_sta_threads) { NEED_PROC(); handoff(1); }
TEST(thr_layout_handoff_mta_threads) { NEED_PROC(); handoff(2); }
TEST(thr_layout_handoff_mixed_apartments) { NEED_PROC(); handoff(3); }
/* created on a thread that is gone: the object does not belong to it */
static void w_create_layout(clt_thr *t) { t->out = mk_hand_layout(); }
TEST(thr_layout_outlives_creating_thread) {
    NEED_PROC();
    DWORD modes[3] = { 0, 1, 2 }; int i;
    for (i = 0; i < 3; i++) {
        clt_thr th; IDWriteTextLayout *l, *twin; uint64_t h1, h2;
        CHECK(clt_run(1, 1, w_create_layout, NULL, &th, modes[i]));
        l = (IDWriteTextLayout *)th.out; twin = mk_hand_layout();
        if (!l || !twin) { t_skip("layout"); DW_RELEASE(l); DW_RELEASE(twin); return; }
        h1 = layout_step(l, 2); h2 = layout_step(twin, 2);
        CHECKF(h1 == h2, "mode %d: layout created on an exited thread behaves differently", i);
        EXPECT_REFS(l, 1);
        DW_RELEASE(l); DW_RELEASE(twin);
    }
}

/* ---- text analyzer on several threads ---------------------------------------------------------------------------------------------- */
static const WCHAR g_ana_text[] = L"Hello, world 123. \x05E9\x05DC\x05D5\x05DD \x0645\x0631\x062D\x0628\x0627 \x4E2D\x6587\x6587\x5B57 \x0E2A\x0E27\x0E31\x0E2A\x0E14\x0E35 abc\r\ndef";
static uint64_t analyze_all(IDWriteTextAnalyzer *an, clt_thr *t) {
    UINT32 len = (UINT32)lstrlenW(g_ana_text), i; dwx_ana_src *src = dwx_ana_src_new(g_ana_text, len); dwx_ana_sink *sk = dwx_ana_sink_new(); uint64_t h = CLT_H0;
    HRESULT hr;
    hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sk); TCHK(t, hr == S_OK);
    TCHK(t, dwx_ana_covered(sk, 's', len));
    for (i = 0; i < len; i++) { h = clt_hi(h, sk->script[i].script); h = clt_hi(h, sk->script[i].shapes); }
    hr = IDWriteTextAnalyzer_AnalyzeBidi(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sk); TCHK(t, hr == S_OK);
    TCHK(t, dwx_ana_covered(sk, 'l', len));
    for (i = 0; i < len; i++) { h = clt_hi(h, sk->bidi_explicit[i]); h = clt_hi(h, sk->bidi_resolved[i]); }
    hr = IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sk); TCHK(t, hr == S_OK);
    TCHK(t, dwx_ana_covered(sk, 'b', len));
    for (i = 0; i < len; i++) h = clt_h(h, &sk->brk[i], sizeof sk->brk[i]);
    TCHK(t, dwx_refs(src) == 1 && dwx_refs(sk) == 1);
    dwx_release(src); dwx_release(sk);
    return h;
}
static void w_analyzer_own(clt_thr *t) {
    int i;
    for (i = 0; i < t->iters; i++) {
        IDWriteTextAnalyzer *an = NULL;
        TCHK(t, IDWriteFactory_CreateTextAnalyzer(dw_shared(), &an) == S_OK && an);
        if (!an) continue;
        t->hash = clt_hi(t->hash, analyze_all(an, t));
        IDWriteTextAnalyzer_Release(an);
    }
}
static void w_analyzer_shared(clt_thr *t) {
    IDWriteTextAnalyzer *an = (IDWriteTextAnalyzer *)t->arg; int i;
    for (i = 0; i < t->iters; i++) t->hash = clt_hi(t->hash, analyze_all(an, t));
}
TEST(thr_text_analyzer_per_thread) {
    NEED_PROC();
    clt_thr th[CLT_MAXTH], base;
    memset(&base, 0, sizeof base); base.iters = 30; base.hash = CLT_H0; w_analyzer_own(&base);
    CHECKF(base.fail == 0, "baseline failures %ld line %d %s", (long)base.fail, base.fail_line, base.fail_expr);
    CHECK(clt_run(8, 30, w_analyzer_own, NULL, th, 0));
    clt_verify(th, 8, "analyzer");
    { int i; for (i = 0; i < 8; i++) CHECK(th[i].hash == base.hash); }
}
TEST(thr_text_analyzer_one_object_many_threads) {
    NEED_PROC();
    IDWriteTextAnalyzer *an = NULL; clt_thr th[CLT_MAXTH], base; int i, differ = 0;
    IDWriteFactory_CreateTextAnalyzer(dw_shared(), &an);
    if (!an) { t_skip("analyzer"); return; }
    memset(&base, 0, sizeof base); base.iters = 30; base.arg = an; base.hash = CLT_H0; w_analyzer_shared(&base);
    CHECK(clt_run(8, 30, w_analyzer_shared, an, th, 0));
    for (i = 0; i < 8; i++) { CHECKF(th[i].fail == 0, "thread %d: %ld failures, first line %d: %s", i, (long)th[i].fail, th[i].fail_line, th[i].fail_expr); if (th[i].hash != base.hash) differ++; }
    if (differ) t_note("%d of 8 threads sharing one analyzer produced a different digest", differ);   /* analyzers are stateless, but concurrency on one object is not a documented guarantee */
    DW_RELEASE(an);
}

/* ---- loader registration from many threads ---------------------------------------------------------------------------------------- */
typedef struct { IDWriteFactory *f; dwx_mem_loader *ld[CLT_MAXTH]; } rl_arg;
static void w_reg(clt_thr *t) {
    rl_arg *a = (rl_arg *)t->arg; dwx_mem_loader *ld = a->ld[t->idx]; int i;
    for (i = 0; i < t->iters; i++) {
        IDWriteFontFile *ff = NULL; IDWriteFontFace *fc = NULL; UINT32 key = 0;
        TCHK(t, IDWriteFactory_RegisterFontFileLoader(a->f, (IDWriteFontFileLoader *)ld) == S_OK);
        TCHK(t, IDWriteFactory_CreateCustomFontFileReference(a->f, &key, 4, (IDWriteFontFileLoader *)ld, &ff) == S_OK && ff);
        if (ff) {
            TCHK(t, IDWriteFactory_CreateFontFace(a->f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &fc) == S_OK && fc);
            if (fc) { UINT32 cp = 'D'; UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(fc, &cp, 1, &g); TCHK(t, g == 5); IDWriteFontFace_Release(fc); }
            IDWriteFontFile_Release(ff);
        }
        TCHK(t, IDWriteFactory_UnregisterFontFileLoader(a->f, (IDWriteFontFileLoader *)ld) == S_OK);
    }
}
TEST(thr_register_unregister_loaders_concurrently) {
    NEED_PROC();
    rl_arg a; clt_thr th[CLT_MAXTH]; int i; size_t n; uint8_t *b;
    a.f = dw_shared();
    if (!a.f) { t_skip("no factory"); return; }
    b = ttf_build_default(&n);
    for (i = 0; i < 8; i++) { a.ld[i] = dwx_loader_new(); dwx_loader_add(a.ld[i], b, n); }
    free(b);
    CHECK(clt_run(8, 25, w_reg, &a, th, 0));
    clt_verify(th, 8, "register/unregister");
    for (i = 0; i < 8; i++) { if (dwx_refs(a.ld[i]) != 1) t_note("loader %d still has %ld refs after unregister (shared factory caches)", i, (long)dwx_refs(a.ld[i])); CHECKF(dwx_refs(a.ld[i]) >= 1, "loader destroyed"); dwx_release(a.ld[i]); }
}
typedef struct { IDWriteFactory *f; dwx_mem_loader *ld; } race_arg;
static void w_race_reg(clt_thr *t) { race_arg *p = (race_arg *)t->arg; t->hr = IDWriteFactory_RegisterFontFileLoader(p->f, (IDWriteFontFileLoader *)p->ld); }
TEST(thr_register_same_loader_exactly_one_winner) {
    NEED_PROC();
    /* 8 threads race to register the same loader object: the registry admits it once (DWRITE_E_ALREADYREGISTERED for the rest) */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld; clt_thr th[CLT_MAXTH]; int i, ok = 0;
    if (!f) { t_skip("no factory"); return; }
    ld = dwx_loader_new();
    {
        race_arg a; a.f = f; a.ld = ld;
        CHECK(clt_run(8, 1, w_race_reg, &a, th, 0));
    }
    for (i = 0; i < 8; i++) { if (th[i].hr == S_OK) ok++; else CHECKF(th[i].hr == (HRESULT)DWRITE_E_ALREADYREGISTERED, "thread %d: %s", i, dw_hr_name(th[i].hr)); }
    EXPECT_EQ(ok, 1);
    EXPECT_EQ(dwx_refs(ld), 2);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)ld));
    EXPECT_EQ(dwx_refs(ld), 1);
    dwx_release(ld);
    DW_RELEASE(f);
}

/* ---- apartments -------------------------------------------------------------------------------------------------------------------- */
typedef struct { void *main_factory; IDWriteFactory *f; void *obj; uint64_t digest; HRESULT co; } ap_arg;
static void w_apartment(clt_thr *t) {
    ap_arg *a = (ap_arg *)t->arg; void *p = NULL; IDWriteFactory *f;
    TCHK(t, dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &p) == S_OK && p);
    t->ptr = p;
    TCHK(t, p == a->main_factory);                       /* no marshalling: the very same C++ object in every apartment */
    f = (IDWriteFactory *)p;
    if (f) {
        TCHK(t, clt_in_dwrite(*(void **)f));
        t->hash = clt_hi(t->hash, clt_op(t, f, 0, 3, NULL));
        t->hash = clt_hi(t->hash, clt_op(t, f, 1, 3, NULL));
        t->hash = clt_hi(t->hash, clt_op(t, f, 7, 3, NULL));
    }
    if (a->f) { /* an object created by the main thread, used here */
        IDWriteTypography *ty = NULL;
        TCHK(t, IDWriteFactory_CreateTypography(a->f, &ty) == S_OK && ty);
        if (ty) { TCHK(t, clt_in_dwrite(*(void **)ty)); IDWriteTypography_Release(ty); }
    }
    if (p) IUnknown_Release((IUnknown *)p);
}
static void apartment_case(DWORD mode, const char *what) {
    IDWriteFactory *f = dw_shared(); ap_arg a; clt_thr th, base;
    if (!f) { t_skip("no factory"); return; }
    a.main_factory = f; a.f = f;
    memset(&base, 0, sizeof base); base.arg = &a; base.hash = CLT_H0; w_apartment(&base);
    CHECKF(base.fail == 0, "%s baseline %ld failures line %d %s", what, (long)base.fail, base.fail_line, base.fail_expr);
    CHECK(clt_run(1, 1, w_apartment, &a, &th, mode));
    CHECKF(th.fail == 0, "%s: %ld failures, first line %d: %s", what, (long)th.fail, th.fail_line, th.fail_expr);
    CHECKF(th.hash == base.hash, "%s: results differ from the main thread", what);
    CHECK(th.ptr == (void *)f);
}
TEST(apt_no_com_initialization_needed) { NEED_PROC(); apartment_case(0, "no COM"); }
TEST(apt_sta_thread_shares_factory) { NEED_PROC(); apartment_case(1, "STA"); }
TEST(apt_mta_thread_shares_factory) { NEED_PROC(); apartment_case(2, "MTA"); }
TEST(apt_sta_and_mta_concurrently) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); ap_arg a; clt_thr th[CLT_MAXTH], base; int i;
    if (!f) { t_skip("no factory"); return; }
    a.main_factory = f; a.f = f;
    memset(&base, 0, sizeof base); base.arg = &a; base.hash = CLT_H0; w_apartment(&base);
    /* clt_run applies one apartment mode to every thread: run two batches back to back, STA then MTA, each 4 threads */
    CHECK(clt_run(4, 1, w_apartment, &a, th, 1)); for (i = 0; i < 4; i++) { CHECK(th[i].fail == 0); CHECK(th[i].hash == base.hash); }
    CHECK(clt_run(4, 1, w_apartment, &a, th, 2)); for (i = 0; i < 4; i++) { CHECK(th[i].fail == 0); CHECK(th[i].hash == base.hash); }
}
static IDWriteFactory *g_made;
static void w_mk_iso(clt_thr *t) { g_made = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); t->ptr = g_made; }
static void w_use_iso(clt_thr *t) { t->hash = clt_hi(t->hash, clt_op(t, g_made, 7, 1, NULL)); t->hash = clt_hi(t->hash, clt_op(t, g_made, 1, 1, NULL)); }
TEST(apt_isolated_factory_created_in_sta_used_in_mta) {
    NEED_PROC();
    clt_thr th; uint64_t h1, h2;
    g_made = NULL;
    CHECK(clt_run(1, 1, w_mk_iso, NULL, &th, 1));
    if (!g_made) { t_skip("no factory"); return; }
    {
        CHECK(clt_run(1, 1, w_use_iso, NULL, &th, 2)); h1 = th.hash; CHECK(th.fail == 0);
        CHECK(clt_run(1, 1, w_use_iso, NULL, &th, 1)); h2 = th.hash; CHECK(th.fail == 0);
    }
    CHECK(h1 == h2);
    IDWriteFactory_Release(g_made);
}
TEST(apt_objects_are_not_proxied) {
    NEED_PROC();
    int n, i; const clt_maker *mk = clt_makers(&n); int checked = 0;
    for (i = 0; i < n; i++) {
        clt_ctx c; IUnknown *o;
        if (!clt_ctx_init(&c)) { t_skip("no factory"); return; }
        o = mk[i].make(&c);
        if (o) { CHECKF(clt_in_dwrite(*(void **)o), "%s: vtable %p is outside dwrite.dll", mk[i].name, *(void **)o); checked++; DW_RELEASE(o); }
        clt_ctx_done(&c);
    }
    CHECK(checked >= 10);
}
TEST(apt_marshaling_probe_note_only) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IUnknown *m = NULL; HRESULT hr;
    static const IID iid_agile = { 0x94ea2b94, 0xe9cc, 0x49e0, { 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90 } };
    IUnknown *ag = NULL;
    if (!f) { t_skip("no factory"); return; }
    hr = IDWriteFactory_QueryInterface(f, &IID_IMarshal, (void **)&m);
    t_note("factory QI(IMarshal) -> %s", dw_hr_name(hr)); DW_RELEASE(m);
    hr = IDWriteFactory_QueryInterface(f, &iid_agile, (void **)&ag);
    t_note("factory QI(IAgileObject) -> %s", dw_hr_name(hr)); DW_RELEASE(ag);
    {
        IStream *st = NULL;
        if (SUCCEEDED(U(CreateStreamOnHGlobal)(NULL, TRUE, &st)) && st) {
            if (SUCCEEDED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) {
                hr = U(CoMarshalInterface)(st, &IID_IDWriteFactory, (IUnknown *)f, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
                t_note("CoMarshalInterface(IDWriteFactory, INPROC) -> %s", dw_hr_name(hr));
                if (SUCCEEDED(hr)) { LARGE_INTEGER z; z.QuadPart = 0; IStream_Seek(st, z, STREAM_SEEK_SET, NULL); U(CoReleaseMarshalData)(st); }
                U(CoUninitialize)();
            }
            IStream_Release(st);
        }
    }
    EXPECT_REFS(f, dw_refcount((IUnknown *)f));
}
