/* QueryInterface / identity / lifetime of font faces with respect to the extension interfaces IDWriteFontFace1..7.
 * IDWriteFontFace::QueryInterface answers IUnknown, IDWriteFontFace, IDWriteFontFace1..5 with the object pointer
 * itself, IDWriteFontFaceReference with an inner sub-object, and falls through to `*ppv = NULL; return 0x80004001`. */
#include "ffx.h"

static HRESULT qi(void *obj, const IID *iid, void **out) { *out = (void *)(ULONG_PTR)0x1; return IUnknown_QueryInterface((IUnknown *)obj, iid, out); }

static void expect_qi_fail(void *obj, const IID *iid, const char *name) {
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = qi(obj, iid, &p);
    t_check(FAILED(hr), __FILE__, __LINE__, name, "QI unexpectedly succeeded");
    t_check(p == NULL, __FILE__, __LINE__, name, "ppv not zeroed on failure: %p", p);
    /* the exact code is a fact about this implementation (E_NOTIMPL, not E_NOINTERFACE) */
    t_check(hr == FFX_QI_FAIL, __FILE__, __LINE__, name, "QI failure code %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
}

TEST(qi_face_family_supported) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    const IID *ids[] = { &IID_IUnknown, &IID_IDWriteFontFace, &IID_IDWriteFontFace1, &IID_IDWriteFontFace2, &IID_IDWriteFontFace3,
                         &IID_IDWriteFontFace4, &IID_IDWriteFontFace5 };
    const char *nm[] = { "IUnknown", "IDWriteFontFace", "IDWriteFontFace1", "IDWriteFontFace2", "IDWriteFontFace3", "IDWriteFontFace4", "IDWriteFontFace5" };
    int i;
    for (i = 0; i < 7; i++) {
        void *p = NULL; ULONG before = dw_refcount((IUnknown *)x.f0);
        HRESULT hr = IDWriteFontFace_QueryInterface(x.f0, ids[i], &p);
        t_check(hr == S_OK, __FILE__, __LINE__, nm[i], "hr %s", dw_hr_name(hr));
        if (hr == S_OK) {
            /* AddRef(this) is applied to the whole object: every IID of the face family succeeds with the same pointer */
            t_check(p == (void *)x.f0, __FILE__, __LINE__, nm[i], "pointer %p differs from the face %p", p, (void *)x.f0);
            ULONG after = dw_refcount((IUnknown *)x.f0);
            t_check(after == before + 1, __FILE__, __LINE__, nm[i], "refcount %lu -> %lu", before, after);
            IUnknown_Release((IUnknown *)p);
        }
    }
    ffx_close(&x);
}
TEST(qi_face6_unsupported_on_win10) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    /* IDWriteFontFace6 (c4b1fe1b-...) is not in the IID list of the Windows 10 19041 IDWriteFontFace::QueryInterface, and no
     * IDWriteFontFace::GetFamilyNames(DWRITE_FONT_FAMILY_MODEL, ...) exists in the module. */
    expect_qi_fail(x.f0, &IID_IDWriteFontFace6, "IDWriteFontFace6");
    ffx_close(&x);
}
TEST(qi_face7_unsupported_on_win10) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    expect_qi_fail(x.f0, &DW_IID_IDWriteFontFace7, "IDWriteFontFace7");
    ffx_close(&x);
}
TEST(qi_unrelated_interfaces_fail) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    struct { const IID *iid; const char *n; } t[] = {
        { &IID_IDWriteFactory, "IDWriteFactory" }, { &IID_IDWriteFont, "IDWriteFont" }, { &IID_IDWriteFontFile, "IDWriteFontFile" },
        { &IID_IDWriteFontFamily, "IDWriteFontFamily" }, { &IID_IDWriteFontCollection, "IDWriteFontCollection" },
        { &IID_IDWriteLocalizedStrings, "IDWriteLocalizedStrings" }, { &IID_IDWriteTextFormat, "IDWriteTextFormat" },
        { &IID_IDWriteTextLayout, "IDWriteTextLayout" }, { &IID_IDWriteFont1, "IDWriteFont1" }, { &IID_IDWriteFont3, "IDWriteFont3" },
        { &IID_IDWriteFontResource, "IDWriteFontResource" }, { &IID_IDWriteFontFaceReference1, "IDWriteFontFaceReference1" },
        { &IID_IDWriteRenderingParams, "IDWriteRenderingParams" }, { &IID_IDWriteTextAnalyzer, "IDWriteTextAnalyzer" },
        { &IID_IDWriteFontFileStream, "IDWriteFontFileStream" }, { &IID_IDWriteFontFileLoader, "IDWriteFontFileLoader" },
        { &IID_IDWriteFontSet, "IDWriteFontSet" }, { &IID_IDWriteFontList, "IDWriteFontList" }, { &IID_IDWriteBitmapRenderTarget, "IDWriteBitmapRenderTarget" },
    };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = qi(x.f0, t[i].iid, &p);
        t_check(FAILED(hr) && p == NULL, __FILE__, __LINE__, t[i].n, "hr=%s p=%p", dw_hr_name(hr), p);
        if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    }
    ffx_close(&x);
}
TEST(qi_null_guid_family_fails) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const IID zero = { 0 };
    static const IID ones = { 0xffffffff, 0xffff, 0xffff, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };
    expect_qi_fail(x.f0, &zero, "GUID_NULL");
    expect_qi_fail(x.f0, &ones, "all-ones GUID");
    ffx_close(&x);
}
TEST(qi_reference_subobject) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    void *r = NULL; ULONG before = dw_refcount((IUnknown *)x.f0);
    /* IDWriteFontFaceReference (5e7fa7ca-...) is answered with the inner IDWriteFontFaceReference sub-object and AddRef(this). */
    HRESULT hr = IDWriteFontFace_QueryInterface(x.f0, &IID_IDWriteFontFaceReference, &r);
    CHECK_HR(hr, S_OK);
    if (hr == S_OK) {
        CHECK(r != NULL); CHECK(r != (void *)x.f0);
        ULONG after = dw_refcount((IUnknown *)x.f0);
        EXPECT_EQ(after, before + 1);
        CHECK(dw_same_identity((IUnknown *)r, (IUnknown *)x.f0));
        IUnknown_Release((IUnknown *)r);
    }
    ffx_close(&x);
}
TEST(qi_iunknown_identity_stable) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IUnknown *a = NULL, *b = NULL, *c = NULL;
    CHECK_HR_OK(IDWriteFontFace1_QueryInterface(x.f1, &IID_IUnknown, (void **)&a));
    CHECK_HR_OK(IDWriteFontFace5_QueryInterface(x.f5, &IID_IUnknown, (void **)&b));
    CHECK_HR_OK(IDWriteFontFace3_QueryInterface(x.f3, &IID_IUnknown, (void **)&c));
    CHECK(a == b && b == c);
    CHECK(a == (IUnknown *)x.f0);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    ffx_close(&x);
}
TEST(qi_chain_from_every_level) {
    /* From any interface in the chain every other member of the family is reachable (same QI implementation). */
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IUnknown *lv[6] = { (IUnknown *)x.f0, (IUnknown *)x.f1, (IUnknown *)x.f2, (IUnknown *)x.f3, (IUnknown *)x.f4, (IUnknown *)x.f5 };
    const IID *ids[6] = { &IID_IDWriteFontFace, &IID_IDWriteFontFace1, &IID_IDWriteFontFace2, &IID_IDWriteFontFace3, &IID_IDWriteFontFace4, &IID_IDWriteFontFace5 };
    int i, j;
    for (i = 0; i < 6; i++) for (j = 0; j < 6; j++) {
        void *p = NULL; HRESULT hr = IUnknown_QueryInterface(lv[i], ids[j], &p);
        t_check(hr == S_OK && p == (void *)x.f0, __FILE__, __LINE__, "chain", "from level %d to %d: %s", i, j, dw_hr_name(hr));
        if (hr == S_OK && p) IUnknown_Release((IUnknown *)p);
    }
    ffx_close(&x);
}
TEST(qi_refcount_balance_many) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    ULONG base = dw_refcount((IUnknown *)x.f0);
    IDWriteFontFace3 *p[16]; int i;
    for (i = 0; i < 16; i++) { p[i] = NULL; CHECK_HR_OK(IDWriteFontFace_QueryInterface(x.f0, &IID_IDWriteFontFace3, (void **)&p[i])); }
    ULONG mid = dw_refcount((IUnknown *)x.f0);
    EXPECT_EQ(mid, base + 16);
    for (i = 0; i < 16; i++) IDWriteFontFace3_Release(p[i]);
    EXPECT_REFS(x.f0, base);
    ffx_close(&x);
}
TEST(qi_addref_release_return_values) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    ULONG base = dw_refcount((IUnknown *)x.f0);
    ULONG a = IDWriteFontFace4_AddRef(x.f4);          /* AddRef through a different interface pointer than the one we release with */
    ULONG b = IDWriteFontFace2_AddRef(x.f2);
    CHECK(a == base + 1); CHECK(b == base + 2);
    ULONG c = IDWriteFontFace5_Release(x.f5);
    ULONG d = IDWriteFontFace1_Release(x.f1);
    CHECK(c == base + 1); CHECK(d == base);
    ffx_close(&x);
}
TEST(qi_face_lifetime_outlives_file_release) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    /* the face keeps working after the application releases its IDWriteFontFile reference */
    DW_RELEASE(x.h.file);
    DWRITE_FONT_METRICS1 m; ffx_metrics1(&x, &m);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    UINT32 n = 0; CHECK_HR_OK(IDWriteFontFace_GetFiles(x.f0, &n, NULL)); EXPECT_EQ(n, 1);
    ffx_close(&x);
}
TEST(qi_face_survives_extra_factory_ref_release) {
    /* A face built through an isolated factory keeps working after the creator's factory reference is gone as long as someone holds the factory. */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f) { t_skip("no isolated factory"); return; }
    ULONG r0 = dw_refcount((IUnknown *)f);
    dwx_font h; size_t n; uint8_t *b = ttf_build_default(&n);
    HRESULT hr = dwx_font_open(&h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE); free(b);
    if (FAILED(hr) || !h.face) { dwx_font_close(&h); IDWriteFactory_Release(f); t_skip("open failed"); return; }
    ULONG r1 = dw_refcount((IUnknown *)f);
    t_note("isolated factory refcount: before face %lu, with loader+file+face %lu", r0, r1);
    IDWriteFontFace1 *f1 = NULL;
    CHECK_HR_OK(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace1, (void **)&f1));
    DWRITE_FONT_METRICS1 m; memset(&m, 0, sizeof m);
    if (f1) { IDWriteFontFace1_GetMetrics(f1, &m); EXPECT_EQ(m.designUnitsPerEm, 2048); }
    DW_RELEASE(f1);
    dwx_font_close(&h);
    ULONG r2 = dw_refcount((IUnknown *)f);
    t_note("factory refcount after closing: %lu", r2);
    IDWriteFactory_Release(f);
}
TEST(qi_two_faces_distinct_objects) {
    ffx a, b; int oa = ffx_open_default(&a); int ob = oa ? ffx_open_default(&b) : 0;
    if (oa && ob) {
        /* the same font bytes through two different loaders produce two face objects (different file keys); both answer the whole chain */
        CHECK(a.f0 != b.f0);
        CHECK(!FFX_SAME(a.f1, b.f1));
        IDWriteFontFace1 *x1 = NULL;
        CHECK_HR_OK(IDWriteFontFace_QueryInterface(a.f0, &IID_IDWriteFontFace1, (void **)&x1));
        CHECK(FFX_SAME(x1, a.f1));
        DW_RELEASE(x1);
    }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(qi_system_face_chain_arial) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    CHECK(FFX_SAME(x.f1, x.f0) && FFX_SAME(x.f5, x.f0));
    expect_qi_fail(x.f0, &IID_IDWriteFontFace6, "IDWriteFontFace6 (Arial)");
    expect_qi_fail(x.f0, &DW_IID_IDWriteFontFace7, "IDWriteFontFace7 (Arial)");
    ffx_close(&x);
}
TEST(qi_system_face_chain_bahnschrift) {
    ffx x; if (!ffx_open_sys(&x, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    CHECK(FFX_SAME(x.f5, x.f0));
    expect_qi_fail(x.f0, &IID_IDWriteFontFace6, "IDWriteFontFace6 (Bahnschrift)");
    ffx_close(&x);
}
TEST(qi_face_from_gdi_interop_chain) {
    /* IDWriteFontFace from IDWriteFont::CreateFontFace (Segoe UI) is the same implementation class */
    ffx x; if (!ffx_open_sys(&x, L"Segoe UI", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    CHECK(FFX_SAME(x.f3, x.f0) && FFX_SAME(x.f4, x.f0));
    ffx_close(&x);
}
TEST(qi_sim_face_chain) {
    ttf_spec s; ttf_default_spec(&s);
    static const DWRITE_FONT_SIMULATIONS sims[] = { DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE };
    int i;
    for (i = 0; i < 3; i++) {
        ffx x; if (!ffx_open_spec(&x, &s, sims[i])) { ffx_close(&x); return; }
        CHECK(FFX_SAME(x.f5, x.f0));
        EXPECT_EQ(IDWriteFontFace5_GetSimulations(x.f5), sims[i]);
        ffx_close(&x);
    }
}
