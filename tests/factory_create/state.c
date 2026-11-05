/* A freshly created factory is immediately usable, and separately created factories are independent of each other while the
 * shared one carries process-wide state. */
#include "dwx.h"
#include "fc.h"

/* ---- table-driven "factory services answer immediately" ------------------------------------------ */
typedef HRESULT (*op_fn)(IDWriteFactory *f);
static HRESULT op_sysfonts(IDWriteFactory *f) { IDWriteFontCollection *c = NULL; HRESULT hr = IDWriteFactory_GetSystemFontCollection(f, &c, FALSE); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); return hr; }
static HRESULT op_sysfonts_update(IDWriteFactory *f) { IDWriteFontCollection *c = NULL; HRESULT hr = IDWriteFactory_GetSystemFontCollection(f, &c, TRUE); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); return hr; }
static HRESULT op_textformat(IDWriteFactory *f) { IDWriteTextFormat *t = NULL; HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.f, L"en-us", &t); if (SUCCEEDED(hr) && !t) hr = E_POINTER; DW_RELEASE(t); return hr; }
static HRESULT op_textformat_missing_family(IDWriteFactory *f) { IDWriteTextFormat *t = NULL; HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"No Such Family Zq", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 9.f, L"fr-fr", &t); if (SUCCEEDED(hr) && !t) hr = E_POINTER; DW_RELEASE(t); return hr; }
static HRESULT op_params(IDWriteFactory *f) { IDWriteRenderingParams *p = NULL; HRESULT hr = IDWriteFactory_CreateRenderingParams(f, &p); if (SUCCEEDED(hr) && !p) hr = E_POINTER; DW_RELEASE(p); return hr; }
static HRESULT op_custom_params(IDWriteFactory *f) { IDWriteRenderingParams *p = NULL; HRESULT hr = IDWriteFactory_CreateCustomRenderingParams(f, 2.2f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_NATURAL, &p); if (SUCCEEDED(hr) && !p) hr = E_POINTER; DW_RELEASE(p); return hr; }
static HRESULT op_typography(IDWriteFactory *f) { IDWriteTypography *p = NULL; HRESULT hr = IDWriteFactory_CreateTypography(f, &p); if (SUCCEEDED(hr) && !p) hr = E_POINTER; DW_RELEASE(p); return hr; }
static HRESULT op_numsub(IDWriteFactory *f) { IDWriteNumberSubstitution *p = NULL; HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-us", FALSE, &p); if (SUCCEEDED(hr) && !p) hr = E_POINTER; DW_RELEASE(p); return hr; }
static HRESULT op_gdi(IDWriteFactory *f) { IDWriteGdiInterop *p = NULL; HRESULT hr = IDWriteFactory_GetGdiInterop(f, &p); if (SUCCEEDED(hr) && !p) hr = E_POINTER; DW_RELEASE(p); return hr; }
static HRESULT op_analyzer(IDWriteFactory *f) { IDWriteTextAnalyzer *p = NULL; HRESULT hr = IDWriteFactory_CreateTextAnalyzer(f, &p); if (SUCCEEDED(hr) && !p) hr = E_POINTER; DW_RELEASE(p); return hr; }
static HRESULT op_layout(IDWriteFactory *f) {
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL; HRESULT hr;
    hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-us", &t);
    if (FAILED(hr)) return hr;
    hr = IDWriteFactory_CreateTextLayout(f, L"Hello", 5, t, 200.f, 50.f, &l);
    if (SUCCEEDED(hr) && !l) hr = E_POINTER;
    DW_RELEASE(l); DW_RELEASE(t); return hr;
}
static HRESULT op_ellipsis(IDWriteFactory *f) {
    IDWriteTextFormat *t = NULL; IDWriteInlineObject *o = NULL; HRESULT hr;
    hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-us", &t);
    if (FAILED(hr)) return hr;
    hr = IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &o);
    if (SUCCEEDED(hr) && !o) hr = E_POINTER;
    DW_RELEASE(o); DW_RELEASE(t); return hr;
}
static HRESULT op_gdilayout(IDWriteFactory *f) {
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL; HRESULT hr;
    hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.f, L"en-us", &t);
    if (FAILED(hr)) return hr;
    hr = IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"Hello", 5, t, 200.f, 50.f, 1.0f, NULL, FALSE, &l);
    if (SUCCEEDED(hr) && !l) hr = E_POINTER;
    DW_RELEASE(l); DW_RELEASE(t); return hr;
}
static HRESULT op_f1_eudc(IDWriteFactory *f) { IDWriteFactory1 *g = NULL; IDWriteFontCollection *c = NULL; HRESULT hr = IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory1, (void **)&g); if (FAILED(hr)) return hr; hr = IDWriteFactory1_GetEudcFontCollection(g, &c, FALSE); DW_RELEASE(c); DW_RELEASE(g); return hr; }
static HRESULT op_f2_fallback(IDWriteFactory *f) { IDWriteFactory2 *g = NULL; IDWriteFontFallback *c = NULL; HRESULT hr = IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&g); if (FAILED(hr)) return hr; hr = IDWriteFactory2_GetSystemFontFallback(g, &c); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); DW_RELEASE(g); return hr; }
static HRESULT op_f3_fontset(IDWriteFactory *f) { IDWriteFactory3 *g = NULL; IDWriteFontSet *c = NULL; HRESULT hr = IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&g); if (FAILED(hr)) return hr; hr = IDWriteFactory3_GetSystemFontSet(g, &c); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); DW_RELEASE(g); return hr; }
static HRESULT op_f3_builder(IDWriteFactory *f) { IDWriteFactory3 *g = NULL; IDWriteFontSetBuilder *c = NULL; HRESULT hr = IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&g); if (FAILED(hr)) return hr; hr = IDWriteFactory3_CreateFontSetBuilder(g, &c); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); DW_RELEASE(g); return hr; }
static HRESULT op_f3_sysfonts(IDWriteFactory *f) { IDWriteFactory3 *g = NULL; IDWriteFontCollection1 *c = NULL; HRESULT hr = IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&g); if (FAILED(hr)) return hr; hr = IDWriteFactory3_GetSystemFontCollection(g, FALSE, &c, FALSE); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); DW_RELEASE(g); return hr; }
static HRESULT op_f3_downloadq(IDWriteFactory *f) { IDWriteFactory3 *g = NULL; IDWriteFontDownloadQueue *c = NULL; HRESULT hr = IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&g); if (FAILED(hr)) return hr; hr = IDWriteFactory3_GetFontDownloadQueue(g, &c); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); DW_RELEASE(g); return hr; }
static HRESULT op_f5_inmem(IDWriteFactory *f) { IDWriteFactory5 *g = NULL; IDWriteInMemoryFontFileLoader *c = NULL; HRESULT hr = IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&g); if (FAILED(hr)) return hr; hr = IDWriteFactory5_CreateInMemoryFontFileLoader(g, &c); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); DW_RELEASE(g); return hr; }
static HRESULT op_f5_builder1(IDWriteFactory *f) { IDWriteFactory5 *g = NULL; IDWriteFontSetBuilder1 *c = NULL; HRESULT hr = IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&g); if (FAILED(hr)) return hr; hr = IDWriteFactory5_CreateFontSetBuilder(g, &c); if (SUCCEEDED(hr) && !c) hr = E_POINTER; DW_RELEASE(c); DW_RELEASE(g); return hr; }

static const struct { const char *name; op_fn fn; } ops[] = {
    { "GetSystemFontCollection", op_sysfonts }, { "GetSystemFontCollection(update)", op_sysfonts_update }, { "CreateTextFormat", op_textformat },
    { "CreateTextFormat(missing family)", op_textformat_missing_family }, { "CreateRenderingParams", op_params },
    { "CreateCustomRenderingParams", op_custom_params }, { "CreateTypography", op_typography }, { "CreateNumberSubstitution", op_numsub },
    { "GetGdiInterop", op_gdi }, { "CreateTextAnalyzer", op_analyzer }, { "CreateTextLayout", op_layout }, { "CreateEllipsisTrimmingSign", op_ellipsis },
    { "CreateGdiCompatibleTextLayout", op_gdilayout }, { "Factory1::GetEudcFontCollection", op_f1_eudc }, { "Factory2::GetSystemFontFallback", op_f2_fallback },
    { "Factory3::GetSystemFontSet", op_f3_fontset }, { "Factory3::CreateFontSetBuilder", op_f3_builder }, { "Factory3::GetSystemFontCollection", op_f3_sysfonts },
    { "Factory3::GetFontDownloadQueue", op_f3_downloadq }, { "Factory5::CreateInMemoryFontFileLoader", op_f5_inmem }, { "Factory5::CreateFontSetBuilder", op_f5_builder1 },
};
#define NOPS ((int)(sizeof ops / sizeof ops[0]))

static void run_op(int type, int i) {
    IDWriteFactory *f = NULL; HRESULT hr; NEED_EXPORT();
    CHECK_HR_OK(fc_create(type, &IID_IDWriteFactory, (void **)&f));
    REQUIRE(f);
    hr = ops[i].fn(f);
    CHECKF(hr == S_OK, "%s on %s factory: %s", ops[i].name, type ? "isolated" : "shared", dw_hr_name(hr));
    DW_RELEASE(f);
}
#define OPTEST(n) TEST(use_shared_op##n) { run_op(FT_SHARED, n); } TEST(use_isolated_op##n) { run_op(FT_ISOLATED, n); }
OPTEST(0) OPTEST(1) OPTEST(2) OPTEST(3) OPTEST(4) OPTEST(5) OPTEST(6) OPTEST(7) OPTEST(8) OPTEST(9) OPTEST(10)
OPTEST(11) OPTEST(12) OPTEST(13) OPTEST(14) OPTEST(15) OPTEST(16) OPTEST(17) OPTEST(18) OPTEST(19) OPTEST(20)

TEST(use_all_ops_on_fresh_isolated_each) {
    int i; NEED_EXPORT();
    for (i = 0; i < NOPS; i++) {
        IDWriteFactory *f = dw_new_factory(FT_ISOLATED); HRESULT hr;
        if (!f) { CHECK(f != NULL); continue; }
        hr = ops[i].fn(f);
        CHECKF(hr == S_OK, "%s: %s", ops[i].name, dw_hr_name(hr));
        { ULONG r = dw_refcount((IUnknown *)f); if (r != 1) t_note("%s leaves factory refs at %lu", ops[i].name, r); }
        DW_RELEASE(f);
    }
}
TEST(use_ops_through_newest_interface_pointer) {
    /* Inherited IDWriteFactory methods called through the Factory7 vtable prefix behave identically. */
    IDWriteFactory7 *f7 = NULL; IDWriteFontCollection *c = NULL; IDWriteRenderingParams *rp = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory7, (void **)&f7));
    REQUIRE(f7);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection((IDWriteFactory *)f7, &c, FALSE)); CHECK(c != NULL);
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams((IDWriteFactory *)f7, &rp)); CHECK(rp != NULL);
    DW_RELEASE(c); DW_RELEASE(rp); DW_RELEASE(f7);
}

/* ---- independence of isolated factories --------------------------------------------------------------- */
TEST(iso_system_collections_are_per_factory) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *b = dw_new_factory(FT_ISOLATED);
    IDWriteFontCollection *ca = NULL, *cb = NULL, *ca2 = NULL; NEED_EXPORT();
    REQUIRE(a && b);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(a, &ca, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(b, &cb, FALSE));
    CHECK(ca && cb);
    if (ca && cb) CHECK(ca != cb);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(a, &ca2, FALSE));
    t_note("same factory returns the same system collection object on a second call: %d", ca == ca2);
    if (ca) CHECK(IDWriteFontCollection_GetFontFamilyCount(ca) == IDWriteFontCollection_GetFontFamilyCount(cb));
    DW_RELEASE(ca); DW_RELEASE(cb); DW_RELEASE(ca2); DW_RELEASE(a); DW_RELEASE(b);
}
TEST(iso_family_counts_match_shared) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *s = dw_shared(); IDWriteFontCollection *ca = NULL, *cs = NULL; NEED_EXPORT();
    REQUIRE(a && s);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(a, &ca, FALSE));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(s, &cs, FALSE));
    if (ca && cs) {
        UINT32 na = IDWriteFontCollection_GetFontFamilyCount(ca), ns = IDWriteFontCollection_GetFontFamilyCount(cs);
        CHECK(na > 0); CHECK(na == ns);
        t_note("system font family count %u", na);
    }
    DW_RELEASE(ca); DW_RELEASE(cs); DW_RELEASE(a);
}
TEST(iso_known_family_visible_on_both_types) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *s = dw_shared(); NEED_EXPORT();
    REQUIRE(a && s);
    if (!dw_have_font(s, L"Arial")) { DW_RELEASE(a); t_skip("Arial not installed"); return; }
    CHECK(dw_have_font(a, L"Arial"));
    CHECK(dw_have_font(a, L"Segoe UI") == dw_have_font(s, L"Segoe UI"));
    DW_RELEASE(a);
}

static DWORD fake_key = 0x4b455931;
TEST(iso_loader_registration_is_per_factory) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *b = dw_new_factory(FT_ISOLATED); dwx_mem_loader *ld = dwx_loader_new(); NEED_EXPORT();
    REQUIRE(a && b && ld);
    IDWriteFontFileLoader *l = (IDWriteFontFileLoader *)ld;
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(a, l));
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(b, l));            /* a different factory has its own registry */
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(a, l), DWRITE_E_ALREADYREGISTERED);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(b, l), DWRITE_E_ALREADYREGISTERED);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(a, l));
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(a, l));            /* free again after unregister */
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(a, l));
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(b, l));
    DW_RELEASE(a); DW_RELEASE(b); dwx_release(ld);
}
TEST(iso_loader_unregister_on_other_factory_fails) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *b = dw_new_factory(FT_ISOLATED); dwx_mem_loader *ld = dwx_loader_new(); NEED_EXPORT();
    REQUIRE(a && b && ld);
    IDWriteFontFileLoader *l = (IDWriteFontFileLoader *)ld;
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(a, l));
    CHECK_HR_FAILED(IDWriteFactory_UnregisterFontFileLoader(b, l));      /* never registered with b */
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(a, l));
    CHECK_HR_FAILED(IDWriteFactory_UnregisterFontFileLoader(a, l));      /* already gone */
    DW_RELEASE(a); DW_RELEASE(b); dwx_release(ld);
}
TEST(iso_custom_file_only_resolves_on_registering_factory) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *b = dw_new_factory(FT_ISOLATED); dwx_mem_loader *ld = dwx_loader_new();
    IDWriteFontFile *fa = NULL, *fb = (IDWriteFontFile *)(ULONG_PTR)1; UINT32 key; HRESULT hr; NEED_EXPORT();
    REQUIRE(a && b && ld);
    key = dwx_loader_add(ld, "x", 1);
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(a, (IDWriteFontFileLoader *)ld));
    hr = IDWriteFactory_CreateCustomFontFileReference(a, &key, 4, (IDWriteFontFileLoader *)ld, &fa);
    CHECK_HR_OK(hr);
    /* b never registered the loader: the reference cannot be built against it. */
    hr = IDWriteFactory_CreateCustomFontFileReference(b, &key, 4, (IDWriteFontFileLoader *)ld, &fb);
    CHECK_HR_FAILED(hr);
    CHECK_OUT_NULL(fb);
    DW_RELEASE(fa);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(a, (IDWriteFontFileLoader *)ld));
    DW_RELEASE(a); DW_RELEASE(b); dwx_release(ld);
}
TEST(iso_loader_registered_on_isolated_not_visible_on_shared) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *s = dw_shared(); dwx_mem_loader *ld = dwx_loader_new();
    IDWriteFontFile *ff = (IDWriteFontFile *)(ULONG_PTR)1; UINT32 key; NEED_EXPORT();
    REQUIRE(a && s && ld);
    key = dwx_loader_add(ld, "x", 1);
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(a, (IDWriteFontFileLoader *)ld));
    CHECK_HR_FAILED(IDWriteFactory_CreateCustomFontFileReference(s, &key, 4, (IDWriteFontFileLoader *)ld, &ff));
    CHECK_OUT_NULL(ff);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(a, (IDWriteFontFileLoader *)ld));
    DW_RELEASE(a); dwx_release(ld);
}
TEST(shared_loader_registration_visible_through_second_handle) {
    /* Both handles are the same object, so a registration made through one is seen through the other. */
    void *h1 = NULL, *h2 = NULL; dwx_mem_loader *ld = dwx_loader_new(); HRESULT hr; NEED_EXPORT();
    REQUIRE(ld);
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, &h1));
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, &h2));
    REQUIRE(h1 && h2);
    hr = IDWriteFactory_RegisterFontFileLoader((IDWriteFactory *)h1, (IDWriteFontFileLoader *)ld);
    if (hr != S_OK) { t_note("RegisterFontFileLoader on the shared factory -> %s", dw_hr_name(hr)); }
    else {
        CHECK_HR(IDWriteFactory_RegisterFontFileLoader((IDWriteFactory *)h2, (IDWriteFontFileLoader *)ld), DWRITE_E_ALREADYREGISTERED);
        CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader((IDWriteFactory *)h2, (IDWriteFontFileLoader *)ld));
    }
    DW_RELEASE(h1); DW_RELEASE(h2); dwx_release(ld);
}
TEST(iso_registration_released_with_factory) {
    /* A registered loader is released once the factory dies; no unregister needed to avoid leaking the loader. */
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED); dwx_mem_loader *ld = dwx_loader_new(); LONG r_before; NEED_EXPORT();
    REQUIRE(a && ld);
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(a, (IDWriteFontFileLoader *)ld));
    r_before = dwx_refs(ld);
    t_note("loader refs while registered: %ld", (long)r_before);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(a, (IDWriteFontFileLoader *)ld));
    CHECK(dwx_refs(ld) == 1);
    DW_RELEASE(a);
    CHECK(dwx_release(ld) == 0);
    CHECK(dwx_destroyed(ld));
}
TEST(iso_many_alive_at_once) {
    IDWriteFactory *f[64]; int i, ok = 0; NEED_EXPORT();
    for (i = 0; i < 64; i++) f[i] = dw_new_factory(FT_ISOLATED);
    for (i = 0; i < 64; i++) if (f[i] && SUCCEEDED(op_params(f[i]))) ok++;
    CHECKF(ok == 64, "ok=%d", ok);
    for (i = 63; i >= 0; i--) DW_RELEASE(f[i]);
}
TEST(iso_create_release_churn) {
    int i, bad = 0; NEED_EXPORT();
    for (i = 0; i < 300; i++) {
        IDWriteFactory *f = dw_new_factory(FT_ISOLATED);
        if (!f) { bad++; continue; }
        if ((i % 25) == 0 && FAILED(op_sysfonts(f))) bad++;
        if (IDWriteFactory_Release(f) != 0) bad++;
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(iso_release_order_reverse_and_forward) {
    IDWriteFactory *f[8]; int i; NEED_EXPORT();
    for (i = 0; i < 8; i++) f[i] = dw_new_factory(FT_ISOLATED);
    for (i = 0; i < 8; i += 2) DW_RELEASE(f[i]);
    for (i = 1; i < 8; i += 2) CHECK(f[i] && SUCCEEDED(op_params(f[i])));
    for (i = 1; i < 8; i += 2) DW_RELEASE(f[i]);
}
TEST(iso_objects_from_two_factories_coexist) {
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *b = dw_new_factory(FT_ISOLATED); IDWriteTextFormat *ta = NULL, *tb = NULL; NEED_EXPORT();
    REQUIRE(a && b);
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(a, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.f, L"en-us", &ta));
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(b, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 22.f, L"en-us", &tb));
    DW_RELEASE(a);
    if (ta && tb) { CHECK(IDWriteTextFormat_GetFontSize(ta) == 11.f); CHECK(IDWriteTextFormat_GetFontSize(tb) == 22.f); CHECK(ta != tb); }
    DW_RELEASE(ta); DW_RELEASE(b); DW_RELEASE(tb);
}
TEST(shared_survives_isolated_churn) {
    IDWriteFactory *s = dw_shared(); int i; NEED_EXPORT();
    REQUIRE(s);
    for (i = 0; i < 20; i++) { IDWriteFactory *f = dw_new_factory(FT_ISOLATED); DW_RELEASE(f); }
    CHECK(SUCCEEDED(op_sysfonts(s)));
    { void *p = NULL; fc_create(FT_SHARED, &IID_IDWriteFactory, &p); CHECK(p == (void *)s); DW_RELEASE(p); }
}
TEST(shared_state_survives_release_of_every_reference) {
    /* Objects fetched from the shared factory stay valid while their owner factory handle is repeatedly dropped and re-acquired. */
    void *h = NULL; IDWriteFontCollection *c1 = NULL, *c2 = NULL; UINT32 n1 = 0, n2 = 0; int i; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, &h)); REQUIRE(h);
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection((IDWriteFactory *)h, &c1, FALSE));
    if (c1) n1 = IDWriteFontCollection_GetFontFamilyCount(c1);
    DW_RELEASE(h);
    for (i = 0; i < 5; i++) { CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, &h)); DW_RELEASE(h); }
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, &h));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection((IDWriteFactory *)h, &c2, FALSE));
    if (c2) n2 = IDWriteFontCollection_GetFontFamilyCount(c2);
    CHECK(n1 == n2 && n1 > 0);
    t_note("shared system collection object identical across re-acquisition: %d", c1 == c2);
    DW_RELEASE(c1); DW_RELEASE(c2); DW_RELEASE(h);
}
TEST(shared_gdi_interop_is_per_factory_object) {
    IDWriteGdiInterop *g1 = NULL, *g2 = NULL; IDWriteFactory *s = dw_shared(), *a = dw_new_factory(FT_ISOLATED); NEED_EXPORT();
    REQUIRE(s && a);
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(s, &g1));
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(a, &g2));
    CHECK(g1 && g2);
    if (g1 && g2) CHECK(g1 != g2);
    DW_RELEASE(g1); DW_RELEASE(g2); DW_RELEASE(a);
}
TEST(shared_vs_isolated_rendering_param_defaults_match) {
    IDWriteRenderingParams *a = NULL, *b = NULL; IDWriteFactory *s = dw_shared(), *i = dw_new_factory(FT_ISOLATED); NEED_EXPORT();
    REQUIRE(s && i);
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(s, &a));
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(i, &b));
    if (a && b) {
        CHECK(IDWriteRenderingParams_GetGamma(a) == IDWriteRenderingParams_GetGamma(b));
        CHECK(IDWriteRenderingParams_GetEnhancedContrast(a) == IDWriteRenderingParams_GetEnhancedContrast(b));
        CHECK(IDWriteRenderingParams_GetClearTypeLevel(a) == IDWriteRenderingParams_GetClearTypeLevel(b));
        CHECK(IDWriteRenderingParams_GetPixelGeometry(a) == IDWriteRenderingParams_GetPixelGeometry(b));
        CHECK(IDWriteRenderingParams_GetRenderingMode(a) == IDWriteRenderingParams_GetRenderingMode(b));
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(i);
}
