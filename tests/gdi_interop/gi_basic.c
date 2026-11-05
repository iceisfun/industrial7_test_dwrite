/* IDWriteGdiInterop object identity: acquisition from the factory, caching, QueryInterface, reference counts, lifetime.
 * GetGdiInterop (DWriteFactory) creates one DWriteGdiInterop per factory on first use and publishes it with a compare-exchange; every
 * call returns that same child object with one more reference.  The object is a ChildComObject: its 0 -> 1 AddRef pins the factory. */
#include "gi.h"

TEST(gi_get_shared_ok)
{
    IDWriteGdiInterop *g = NULL;
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(dw_shared(), &g));
    CHECK(g != NULL);
    DW_RELEASE(g);
}
TEST(gi_get_isolated_ok)
{
    IDWriteGdiInterop *g = NULL;
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(dw_isolated(), &g));
    CHECK(g != NULL);
    DW_RELEASE(g);
}
TEST(gi_get_via_every_factory_version)
{
    int v;
    for (v = 0; v <= 7; v++) {
        IDWriteFactory *f = (IDWriteFactory *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v);
        IDWriteGdiInterop *g = NULL;
        if (!f) { t_note("IDWriteFactory%d not supported", v); continue; }
        CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g));
        CHECK(g != NULL);
        DW_RELEASE(g);
    }
}
TEST(gi_get_returns_cached_object)
{
    IDWriteGdiInterop *a = NULL, *b = NULL, *c = NULL;
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a));
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &b));
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &c));
    CHECK(a != NULL && a == b && b == c);   /* the single published child object */
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    DW_RELEASE(f);
}
TEST(gi_get_shared_cached_object)
{
    IDWriteGdiInterop *a = NULL, *b = NULL;
    IDWriteFactory *f = dw_shared();
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a));
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &b));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(gi_get_refcount_grows_per_call)
{
    IDWriteGdiInterop *a = NULL, *b = NULL, *c = NULL;
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a));
    EXPECT_REFS(a, 1);   /* flag bit cleared on publication, one reference handed to the caller */
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &b));
    EXPECT_REFS(a, 2);
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &c));
    EXPECT_REFS(a, 3);
    DW_RELEASE(c); EXPECT_REFS(a, 2);
    DW_RELEASE(b); EXPECT_REFS(a, 1);
    DW_RELEASE(a);
    DW_RELEASE(f);
}
TEST(gi_get_zero_refs_reacquire)
{
    /* after the last reference is dropped the factory still owns the child (it stays cached and can be handed out again) */
    IDWriteGdiInterop *a = NULL, *b = NULL;
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a));
    DW_RELEASE(a);
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &b));
    CHECK(b != NULL);
    if (b) EXPECT_REFS(b, 1);
    DW_RELEASE(b);
    DW_RELEASE(f);
}
TEST(gi_two_factories_two_interops)
{
    IDWriteGdiInterop *a = NULL, *b = NULL;
    IDWriteFactory *f1 = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *f2 = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f1 || !f2) { t_skip("no factories"); DW_RELEASE(f1); DW_RELEASE(f2); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f1, &a));
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f2, &b));
    CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f1); DW_RELEASE(f2);
}
TEST(gi_shared_vs_isolated_differ)
{
    IDWriteGdiInterop *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(dw_shared(), &a));
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(dw_isolated(), &b));
    CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(gi_interop_pins_factory)
{
    /* ChildComObject::AddRef takes a reference on the parent factory on the 0 -> 1 transition */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteGdiInterop *g = NULL; ULONG before, during, after;
    if (!f) { t_skip("no factory"); return; }
    before = dw_refcount((IUnknown *)f);
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g));
    during = dw_refcount((IUnknown *)f);
    DW_RELEASE(g);
    after = dw_refcount((IUnknown *)f);
    CHECK(during >= before);
    CHECK(after == before);
    t_note("factory refs before/during/after GetGdiInterop: %lu/%lu/%lu", before, during, after);
    DW_RELEASE(f);
}
TEST(gi_use_after_factory_release)
{
    /* the interop keeps its factory alive: it must stay usable after the application dropped the factory */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteGdiInterop *g = NULL; IDWriteBitmapRenderTarget *rt = NULL; IDWriteFont *font = NULL; LOGFONTW lf;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g));
    DW_RELEASE(f);
    if (!g) return;
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 16, 16, &rt));
    CHECK(rt != NULL);
    DW_RELEASE(rt);
    gi_lf(&lf, "NoSuchFaceForLifetime", 400, 0, 1);
    CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font), DWRITE_E_NOFONT);
    CHECK_OUT_NULL(font);
    DW_RELEASE(g);
}
TEST(gi_font_outlives_factory_and_interop)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteGdiInterop *g = NULL; IDWriteFont *font = NULL; LOGFONTW lf; BOOL sys = 7; IDWriteFontFamily *fam = NULL;
    if (!f) { t_skip("no factory"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(f); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g));
    gi_lf(&lf, "Arial", 400, 0, 1);
    CHECK_HR_OK(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font));
    DW_RELEASE(g); DW_RELEASE(f);
    if (!font) return;
    CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &fam));
    CHECK(fam != NULL);
    DW_RELEASE(fam);
    DW_RELEASE(font);
}
TEST(gi_addref_release_values)
{
    IDWriteGdiInterop *g = gi_get(dw_isolated());
    ULONG a, b, c;
    if (!g) { t_skip("no interop"); return; }
    a = IDWriteGdiInterop_AddRef(g);
    b = IDWriteGdiInterop_AddRef(g);
    CHECK(b == a + 1);
    c = IDWriteGdiInterop_Release(g);
    CHECK(c == b - 1);
    c = IDWriteGdiInterop_Release(g);
    CHECK(c == a - 1);
    DW_RELEASE(g);
}
TEST(gi_qi_matrix)
{
    IDWriteGdiInterop *g = gi_get(dw_isolated());
    static const dw_qi_row rows[] = {
        DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteGdiInterop, 1), DW_QI_ROW(IDWriteGdiInterop1, 1),
        DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFace, 0),
        DW_QI_ROW(IDWriteBitmapRenderTarget, 0), DW_QI_ROW(IDWriteBitmapRenderTarget1, 0), DW_QI_ROW(IDWriteFontCollection, 0),
        DW_QI_ROW(IDWriteRenderingParams, 0), DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteTextLayout, 0),
        DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0),
    };
    if (!g) { t_skip("no interop"); return; }
    DW_CHECK_QI(g, rows);
    DW_RELEASE(g);
}
TEST(gi_qi_all_public_iids)
{
    /* the QueryInterface list of DWriteGdiInterop is {IUnknown, IDWriteGdiInterop, IDWriteGdiInterop1}; nothing else */
    IDWriteGdiInterop *g = gi_get(dw_isolated());
    int n = 0, i, hits = 0; const dw_qi_row *tab = dw_iid_table(&n); ULONG r0;
    if (!g) { t_skip("no interop"); return; }
    r0 = dw_refcount((IUnknown *)g);
    for (i = 0; i < n; i++) {
        void *p = (void *)1; HRESULT hr; int allowed;
        allowed = !strcmp(tab[i].name, "IDWriteGdiInterop") || !strcmp(tab[i].name, "IDWriteGdiInterop1") || !strcmp(tab[i].name, "IUnknown");
        hr = IDWriteGdiInterop_QueryInterface(g, tab[i].iid, &p);
        if (allowed) {
            hits++;
            CHECKF(hr == S_OK && p != NULL, "%s: %s", tab[i].name, dw_hr_name(hr));
            if (p) IUnknown_Release((IUnknown *)p);
        } else {
            CHECKF(hr == E_NOINTERFACE, "%s: %s", tab[i].name, dw_hr_name(hr));
            CHECKF(p == NULL, "%s: out param not zeroed", tab[i].name);
        }
    }
    t_note("interfaces tried %d, accepted %d", n, hits);
    EXPECT_REFS(g, r0);   /* every successful QI was released */
    DW_RELEASE(g);
}
TEST(gi_qi_balance)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteGdiInterop *g = NULL; IDWriteGdiInterop1 *g1 = NULL; IUnknown *u = NULL;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g));
    EXPECT_REFS(g, 1);
    CHECK_HR_OK(IDWriteGdiInterop_QueryInterface(g, &IID_IDWriteGdiInterop1, (void **)&g1));
    EXPECT_REFS(g, 2);
    CHECK_HR_OK(IDWriteGdiInterop_QueryInterface(g, &IID_IUnknown, (void **)&u));
    EXPECT_REFS(g, 3);
    DW_RELEASE(u); DW_RELEASE(g1);
    EXPECT_REFS(g, 1);
    DW_RELEASE(g); DW_RELEASE(f);
}
TEST(gi_qi_identity)
{
    IDWriteGdiInterop *g = gi_get(dw_isolated());
    IDWriteGdiInterop1 *g1; IUnknown *u1 = NULL, *u2 = NULL, *u3 = NULL;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    CHECK(g1 != NULL);
    CHECK_HR_OK(IDWriteGdiInterop_QueryInterface(g, &IID_IUnknown, (void **)&u1));
    CHECK_HR_OK(IDWriteGdiInterop_QueryInterface(g, &IID_IUnknown, (void **)&u2));
    if (g1) CHECK_HR_OK(IDWriteGdiInterop1_QueryInterface(g1, &IID_IUnknown, (void **)&u3));
    CHECK(u1 != NULL && u1 == u2);
    if (g1) CHECK(u1 == u3);
    CHECK(dw_same_identity((IUnknown *)g, (IUnknown *)g1));
    if (g1) t_note("IDWriteGdiInterop1 pointer %s the IDWriteGdiInterop pointer", (void *)g1 == (void *)g ? "equals" : "differs from");
    DW_RELEASE(u1); DW_RELEASE(u2); DW_RELEASE(u3); DW_RELEASE(g1); DW_RELEASE(g);
}
TEST(gi_qi_from_interop1_down)
{
    IDWriteGdiInterop *g = gi_get(dw_isolated()), *down = NULL;
    IDWriteGdiInterop1 *g1;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1) { t_skip("IDWriteGdiInterop1 unsupported"); DW_RELEASE(g); return; }
    CHECK_HR_OK(IDWriteGdiInterop1_QueryInterface(g1, &IID_IDWriteGdiInterop, (void **)&down));
    CHECK(down == g);
    DW_RELEASE(down); DW_RELEASE(g1); DW_RELEASE(g);
}
static const IID gi_null_iid = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
TEST(gi_qi_failure_zeroes_out)
{
    IDWriteGdiInterop *g = gi_get(dw_isolated());
    void *p = (void *)(ULONG_PTR)0x1234; HRESULT hr;
    if (!g) { t_skip("no interop"); return; }
    hr = IDWriteGdiInterop_QueryInterface(g, &IID_IDWriteFactory, &p);
    CHECK_HR(hr, E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    p = (void *)(ULONG_PTR)0x1234;
    hr = IDWriteGdiInterop_QueryInterface(g, &gi_null_iid, &p);
    CHECK_HR(hr, E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    DW_RELEASE(g);
}
TEST(gi_vtable_stable_across_calls)
{
    IDWriteGdiInterop *a = gi_get(dw_isolated()), *b = gi_get(dw_isolated());
    if (!a || !b) { t_skip("no interop"); DW_RELEASE(a); DW_RELEASE(b); return; }
    CHECK(a->lpVtbl == b->lpVtbl);
    CHECK(a->lpVtbl->CreateFontFromLOGFONT != NULL && a->lpVtbl->ConvertFontToLOGFONT != NULL &&
          a->lpVtbl->ConvertFontFaceToLOGFONT != NULL && a->lpVtbl->CreateFontFaceFromHdc != NULL && a->lpVtbl->CreateBitmapRenderTarget != NULL);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(gi_interop1_vtable_extends_interop)
{
    IDWriteGdiInterop *g = gi_get(dw_isolated()); IDWriteGdiInterop1 *g1;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1) { t_skip("no interop1"); DW_RELEASE(g); return; }
    /* the inherited slots may resolve to shared code or to per-vtable thunks; the identity is implementation detail, so only record it */
    t_note("inherited slots shared: %d %d %d %d",
           (void *)g1->lpVtbl->ConvertFontToLOGFONT == (void *)g->lpVtbl->ConvertFontToLOGFONT,
           (void *)g1->lpVtbl->ConvertFontFaceToLOGFONT == (void *)g->lpVtbl->ConvertFontFaceToLOGFONT,
           (void *)g1->lpVtbl->CreateFontFaceFromHdc == (void *)g->lpVtbl->CreateFontFaceFromHdc,
           (void *)g1->lpVtbl->CreateBitmapRenderTarget == (void *)g->lpVtbl->CreateBitmapRenderTarget);
    CHECK(g1->lpVtbl->GetFontSignature != NULL && g1->lpVtbl->GetFontSignature_ != NULL && g1->lpVtbl->GetMatchingFontsByLOGFONT != NULL);
    DW_RELEASE(g1); DW_RELEASE(g);
}
TEST(gi_leak_many_cycles)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); int i;
    if (!f) { t_skip("no factory"); return; }
    for (i = 0; i < 500; i++) {
        IDWriteGdiInterop *g = NULL; IDWriteGdiInterop1 *g1 = NULL;
        if (FAILED(IDWriteFactory_GetGdiInterop(f, &g)) || !g) { CHECK(0); break; }
        IDWriteGdiInterop_QueryInterface(g, &IID_IDWriteGdiInterop1, (void **)&g1);
        DW_RELEASE(g1); DW_RELEASE(g);
    }
    {
        IDWriteGdiInterop *g = NULL;
        CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g));
        if (g) EXPECT_REFS(g, 1);
        DW_RELEASE(g);
    }
    DW_RELEASE(f);
}
static DWORD WINAPI gi_thr_get(LPVOID arg)
{
    IDWriteFactory *f = (IDWriteFactory *)arg; int i; void **out = NULL; (void)out;
    for (i = 0; i < 200; i++) {
        IDWriteGdiInterop *g = NULL;
        if (FAILED(IDWriteFactory_GetGdiInterop(f, &g)) || !g) return 1;
        IDWriteGdiInterop_Release(g);
    }
    return 0;
}
TEST(gi_threads_race_first_creation)
{
    /* the child object is published with a compare-exchange: concurrent first callers all end up with one object */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); HANDLE th[8]; int i; DWORD rc, bad = 0;
    IDWriteGdiInterop *g = NULL;
    if (!f) { t_skip("no factory"); return; }
    for (i = 0; i < 8; i++) th[i] = CreateThread(NULL, 0, gi_thr_get, f, 0, NULL);
    for (i = 0; i < 8; i++) { if (!th[i]) { bad++; continue; } if (WaitForSingleObject(th[i], 10000) != WAIT_OBJECT_0) bad++; else { GetExitCodeThread(th[i], &rc); if (rc) bad++; } CloseHandle(th[i]); }
    CHECK(bad == 0);
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &g));
    if (g) EXPECT_REFS(g, 1);
    DW_RELEASE(g); DW_RELEASE(f);
}
TEST(gi_factory_qi_gdi_not_an_interface_of_factory)
{
    void *p = (void *)1;
    HRESULT hr = IDWriteFactory_QueryInterface(dw_shared(), &IID_IDWriteGdiInterop, &p);
    CHECK_HR(hr, E_NOINTERFACE);   /* the interop is reached only through GetGdiInterop */
    CHECK_OUT_NULL(p);
}
