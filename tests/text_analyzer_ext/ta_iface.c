/* Interface plumbing of the text analyzer: QueryInterface to IDWriteTextAnalyzer1 / IDWriteTextAnalyzer2, identity, vtable
 * layout, lifetime relative to the factory, independence of analyzer instances and thread safety of the stateless methods. */
#include "ta.h"

TEST(ti_factory_analyzer_has_all_three_interfaces) {
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *a = NULL; IDWriteTextAnalyzer1 *a1 = NULL; IDWriteTextAnalyzer2 *a2 = NULL;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a)); if (!a) return;
    CHECK_HR(IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&a1), S_OK); CHECK(a1 != NULL);
    CHECK_HR(IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer2, (void **)&a2), S_OK); CHECK(a2 != NULL);
    DW_RELEASE(a2); DW_RELEASE(a1); DW_RELEASE(a);
}
TEST(ti_qi_refcounting) {
    ta_ctx c; if (!ta_open(&c)) { ta_close(&c); t_skip("analyzer interfaces unavailable"); return; }
    ULONG base = dw_refcount((IUnknown *)c.a);
    CHECK(base >= 1);
    IDWriteTextAnalyzer1 *x = NULL;
    CHECK_HR(IDWriteTextAnalyzer_QueryInterface(c.a, &IID_IDWriteTextAnalyzer1, (void **)&x), S_OK);
    EXPECT_REFS(c.a, base + 1); IDWriteTextAnalyzer1_Release(x); EXPECT_REFS(c.a, base);
    ta_close(&c);
}
TEST(ti_identity_across_interfaces) {
    ta_ctx c; if (!ta_open(&c)) { ta_close(&c); t_skip("analyzer interfaces unavailable"); return; }
    CHECK(dw_same_identity((IUnknown *)c.a, (IUnknown *)c.a1)); CHECK(dw_same_identity((IUnknown *)c.a1, (IUnknown *)c.a2));
    IUnknown *u0 = NULL, *u2 = NULL;
    CHECK_HR(IDWriteTextAnalyzer_QueryInterface(c.a, &IID_IUnknown, (void **)&u0), S_OK);
    CHECK_HR(IDWriteTextAnalyzer2_QueryInterface(c.a2, &IID_IUnknown, (void **)&u2), S_OK);
    CHECK(u0 == u2); DW_RELEASE(u0); DW_RELEASE(u2);
    t_note("interface pointers: base %p a1 %p a2 %p", (void *)c.a, (void *)c.a1, (void *)c.a2);
    ta_close(&c);
}
TEST(ti_qi_between_interfaces) {
    ta_ctx c; if (!ta_open(&c)) { ta_close(&c); t_skip("analyzer interfaces unavailable"); return; }
    IDWriteTextAnalyzer *b = NULL; IDWriteTextAnalyzer1 *x1 = NULL; IDWriteTextAnalyzer2 *x2 = NULL;
    CHECK_HR(IDWriteTextAnalyzer2_QueryInterface(c.a2, &IID_IDWriteTextAnalyzer, (void **)&b), S_OK); DW_RELEASE(b);
    CHECK_HR(IDWriteTextAnalyzer2_QueryInterface(c.a2, &IID_IDWriteTextAnalyzer1, (void **)&x1), S_OK); DW_RELEASE(x1);
    CHECK_HR(IDWriteTextAnalyzer1_QueryInterface(c.a1, &IID_IDWriteTextAnalyzer2, (void **)&x2), S_OK); DW_RELEASE(x2);
    CHECK_HR(IDWriteTextAnalyzer1_QueryInterface(c.a1, &IID_IDWriteTextAnalyzer, (void **)&b), S_OK); DW_RELEASE(b);
    ta_close(&c);
}
TEST(ti_qi_unsupported_interfaces) {
    ta_ctx c; if (!ta_open(&c)) { ta_close(&c); t_skip("analyzer interfaces unavailable"); return; }
    static const IID *const bad[] = { &IID_IDWriteTextAnalysisSource, &IID_IDWriteTextAnalysisSink, &IID_IDWriteFactory, &IID_IDWriteFontFace, &IID_IDWriteTextLayout, &IID_IDWriteTextFormat, &IID_IDWriteNumberSubstitution };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        void *p = (void *)(ULONG_PTR)0x1000;
        HRESULT hr = IDWriteTextAnalyzer2_QueryInterface(c.a2, bad[i], &p);
        CHECK_HR(hr, E_NOINTERFACE); CHECK_OUT_NULL(p);
    }
    ta_close(&c);
}
TEST(ti_vtable_prefix_shared) {
    /* IDWriteTextAnalyzer2 extends 1 extends the base: the leading vtable slots are the same functions */
    ta_ctx c; if (!ta_open(&c)) { ta_close(&c); t_skip("analyzer interfaces unavailable"); return; }
    const void *const *v0 = (const void *const *)c.a->lpVtbl, *const *v1 = (const void *const *)c.a1->lpVtbl, *const *v2 = (const void *const *)c.a2->lpVtbl;
    unsigned i, n0 = sizeof(IDWriteTextAnalyzerVtbl) / sizeof(void *), n1 = sizeof(IDWriteTextAnalyzer1Vtbl) / sizeof(void *);
    if (c.a == c.a1 && c.a1 == c.a2) {
        for (i = 0; i < n0; i++) CHECKF(v0[i] == v1[i], "slot %u", i);
        for (i = 0; i < n1; i++) CHECKF(v1[i] == v2[i], "slot %u", i);
    } else t_note("analyzer interfaces use distinct interface pointers (base %p a1 %p a2 %p)", (void *)c.a, (void *)c.a1, (void *)c.a2);
    ta_close(&c);
}
TEST(ti_two_analyzers_are_independent_objects) {
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *a = NULL, *b = NULL; if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a)); CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &b));
    if (a && b) { CHECK(!dw_same_identity((IUnknown *)a, (IUnknown *)b) || a == b); EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); }
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(ti_methods_work_after_factory_release) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteTextAnalyzer *a = NULL; IDWriteTextAnalyzer1 *a1 = NULL; IDWriteTextAnalyzer2 *a2 = NULL;
    if (!f) { t_skip("no isolated factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a));
    if (a) { IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&a1); IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer2, (void **)&a2); }
    IDWriteFactory_Release(f);
    if (a1 && a2) {
        DWRITE_MATRIX m; DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p; sa.script = 1; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
        CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, 1, FALSE, &m), S_OK); CHECK_F(m.m12, 1.0f);
        CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, 1, FALSE, 3.0f, 4.0f, &m), S_OK); CHECK_F(m.dx, 7.0f);
        CHECK_HR(IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, &p), S_OK);
        float oa[1]; DWRITE_GLYPH_OFFSET oo[1]; float adv[1] = { 5 }; DWRITE_GLYPH_OFFSET off[1] = { { 0, 0 } }; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(0, 1, 0, 0) };
        CHECK_HR(IDWriteTextAnalyzer1_ApplyCharacterSpacing(a1, 1, 1, 0, 1, 1, cm, adv, off, gp, oa, oo), S_OK); CHECK_F(oa[0], 7.0f);
    }
    DW_RELEASE(a2); DW_RELEASE(a1); DW_RELEASE(a);
}
TEST(ti_release_order_any) {
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *a = NULL; IDWriteTextAnalyzer1 *a1 = NULL; IDWriteTextAnalyzer2 *a2 = NULL; if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &a)); if (!a) return;
    IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&a1); IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer2, (void **)&a2);
    DW_RELEASE(a);
    if (a1) { DWRITE_MATRIX m; CHECK_HR(IDWriteTextAnalyzer1_GetGlyphOrientationTransform(a1, 0, FALSE, &m), S_OK); }
    DW_RELEASE(a1);
    if (a2) { DWRITE_MATRIX m; CHECK_HR(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(a2, 3, TRUE, 0, 0, &m), S_OK); }
    DW_RELEASE(a2);
}
TEST(ti_isolated_and_shared_factory_analyzers_agree) {
    IDWriteFactory *fs = dw_shared(), *fi = dw_isolated(); IDWriteTextAnalyzer1 *x = NULL, *y = NULL; IDWriteTextAnalyzer *a = NULL, *b = NULL; UINT32 i; int diff = 0;
    if (!fs || !fi) { t_skip("factories unavailable"); return; }
    IDWriteFactory_CreateTextAnalyzer(fs, &a); IDWriteFactory_CreateTextAnalyzer(fi, &b);
    if (a && b && SUCCEEDED(IDWriteTextAnalyzer_QueryInterface(a, &IID_IDWriteTextAnalyzer1, (void **)&x)) && SUCCEEDED(IDWriteTextAnalyzer_QueryInterface(b, &IID_IDWriteTextAnalyzer1, (void **)&y))) {
        for (i = 0; i < 153; i++) {
            DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p, q; sa.script = (UINT16)i; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
            IDWriteTextAnalyzer1_GetScriptProperties(x, sa, &p); IDWriteTextAnalyzer1_GetScriptProperties(y, sa, &q);
            if (memcmp(&p, &q, sizeof p)) diff++;
        }
    }
    EXPECT_EQ(diff, 0);
    DW_RELEASE(x); DW_RELEASE(y); DW_RELEASE(a); DW_RELEASE(b);
}

typedef struct thr { IDWriteTextAnalyzer2 *a2; volatile LONG bad; } thr;
static DWORD WINAPI worker(void *p) {
    thr *t = (thr *)p; int i;
    for (i = 0; i < 3000; i++) {
        DWRITE_MATRIX m; DWRITE_SCRIPT_PROPERTIES sp; DWRITE_SCRIPT_ANALYSIS sa; sa.script = (UINT16)(i % 153); sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
        if (FAILED(IDWriteTextAnalyzer2_GetGlyphOrientationTransform(t->a2, (DWRITE_GLYPH_ORIENTATION_ANGLE)(i & 3), (i >> 2) & 1, 10.0f, 20.0f, &m)) ||
            m.m11 * m.m22 - m.m12 * m.m21 != 1.0f) InterlockedIncrement(&t->bad);
        if (FAILED(IDWriteTextAnalyzer2_GetScriptProperties(t->a2, sa, &sp))) InterlockedIncrement(&t->bad);
        { float adv[2] = { 10, 10 }, oa[2]; DWRITE_GLYPH_OFFSET off[2] = { { 0, 0 }, { 0, 0 } }, oo[2]; UINT16 cm[2] = { 0, 1 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[2] = { ta_gp(0, 1, 0, 0), ta_gp(0, 1, 0, 0) };
          if (FAILED(IDWriteTextAnalyzer2_ApplyCharacterSpacing(t->a2, 1, 2, 0, 2, 2, cm, adv, off, gp, oa, oo)) || oa[0] != 13.0f || oa[1] != 13.0f || oo[1].advanceOffset != 1.0f) InterlockedIncrement(&t->bad); }
    }
    return 0;
}
TEST(ti_concurrent_calls_on_one_analyzer) {
    ta_ctx c; HANDLE h[8]; thr t; int i; if (!ta_open(&c)) { ta_close(&c); t_skip("analyzer interfaces unavailable"); return; }
    t.a2 = c.a2; t.bad = 0;
    for (i = 0; i < 8; i++) h[i] = CreateThread(NULL, 0, worker, &t, 0, NULL);
    for (i = 0; i < 8; i++) if (h[i]) { if (WaitForSingleObject(h[i], 30000) != WAIT_OBJECT_0) CHECK(0); CloseHandle(h[i]); }
    EXPECT_EQ(t.bad, 0);
    ta_close(&c);
}
typedef struct qthr { IDWriteTextAnalyzer *a; volatile LONG bad; } qthr;
static DWORD WINAPI qworker(void *p) {
    qthr *t = (qthr *)p; int i;
    for (i = 0; i < 2000; i++) {
        IDWriteTextAnalyzer1 *x = NULL; IDWriteTextAnalyzer2 *y = NULL;
        if (FAILED(IDWriteTextAnalyzer_QueryInterface(t->a, &IID_IDWriteTextAnalyzer1, (void **)&x)) || !x) { InterlockedIncrement(&t->bad); continue; }
        if (FAILED(IDWriteTextAnalyzer1_QueryInterface(x, &IID_IDWriteTextAnalyzer2, (void **)&y)) || !y) InterlockedIncrement(&t->bad);
        if (y) IDWriteTextAnalyzer2_Release(y);
        IDWriteTextAnalyzer1_Release(x);
    }
    return 0;
}
TEST(ti_concurrent_qi_and_release) {
    ta_ctx c; HANDLE h[4]; qthr t; int i; if (!ta_open(&c)) { ta_close(&c); t_skip("analyzer interfaces unavailable"); return; }
    ULONG r0 = dw_refcount((IUnknown *)c.a); t.a = c.a; t.bad = 0;
    for (i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, qworker, &t, 0, NULL);
    for (i = 0; i < 4; i++) if (h[i]) { if (WaitForSingleObject(h[i], 30000) != WAIT_OBJECT_0) CHECK(0); CloseHandle(h[i]); }
    EXPECT_EQ(t.bad, 0); EXPECT_REFS(c.a, r0);
    ta_close(&c);
}
