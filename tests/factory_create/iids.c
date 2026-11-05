/* IID handling of DWriteCreateFactory: the factory object implements IDWriteFactory .. IDWriteFactory7 (and IUnknown);
 * every other IID fails with E_NOINTERFACE and a zeroed out pointer. The IID lookup happens after the object exists,
 * so for the SHARED type a failed lookup leaves the process-wide object untouched. */
#include "fc.h"

static const char *const ver_names[8] = { "IDWriteFactory", "IDWriteFactory1", "IDWriteFactory2", "IDWriteFactory3",
                                          "IDWriteFactory4", "IDWriteFactory5", "IDWriteFactory6", "IDWriteFactory7" };

static void check_create_ver(int type, int ver) {
    void *p = FC_SENTINEL, *base = NULL; HRESULT hr; NEED_EXPORT();
    hr = fc_create(type, dw_factory_iid(ver), &p);
    CHECKF(hr == S_OK, "%s type %d: %s", ver_names[ver], type, dw_hr_name(hr));
    if (hr != S_OK) { CHECK_OUT_NULL(p); return; }
    CHECK(p != NULL && p != FC_SENTINEL);
    if (type == FT_ISOLATED) EXPECT_REFS(p, 1);
    /* Same object as the one reachable through IDWriteFactory. */
    CHECK_HR_OK(fc_create(type, &IID_IDWriteFactory, &base));
    if (type == FT_SHARED && base) CHECK(dw_same_identity((IUnknown *)p, (IUnknown *)base));
    /* The base interface is reachable from every generation, and every generation from the base. */
    {
        void *q = NULL;
        CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteFactory, &q));
        DW_RELEASE(q);
        CHECK_HR_OK(IUnknown_QueryInterface((IUnknown *)p, &IID_IUnknown, &q));
        DW_RELEASE(q);
    }
    DW_RELEASE(base);
    DW_RELEASE(p);
}
#define VER_TEST(v) \
TEST(iid_shared_factory##v) { check_create_ver(FT_SHARED, v); } \
TEST(iid_isolated_factory##v) { check_create_ver(FT_ISOLATED, v); }
VER_TEST(0) VER_TEST(1) VER_TEST(2) VER_TEST(3) VER_TEST(4) VER_TEST(5) VER_TEST(6) VER_TEST(7)

static void check_create_iunknown(int type) {
    void *p = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(type, &IID_IUnknown, &p));
    CHECK(p != NULL && p != FC_SENTINEL);
    if (type == FT_ISOLATED) EXPECT_REFS(p, 1);
    DW_RELEASE(p);
}
TEST(iid_shared_iunknown) { check_create_iunknown(FT_SHARED); }
TEST(iid_isolated_iunknown) { check_create_iunknown(FT_ISOLATED); }

/* Interfaces that exist in the public headers but are not implemented by the factory. */
static void check_foreign_iid(int type, const dw_qi_row *r) {
    void *p = FC_SENTINEL; HRESULT hr;
    hr = fc_create(type, r->iid, &p);
    CHECKF(hr == E_NOINTERFACE, "%s type %d: got %s", r->name, type, dw_hr_name(hr));
    CHECKF(p == NULL, "%s: out pointer %p", r->name, p);
    if (p && p != FC_SENTINEL) DW_RELEASE(p);
}
static int is_factory_iid(const IID *iid) {
    int v;
    if (IsEqualIID(iid, &IID_IUnknown)) return 1;
    for (v = 0; v < 8; v++) if (IsEqualIID(iid, dw_factory_iid(v))) return 1;
    return 0;
}
TEST(iid_public_interfaces_shared_all) {
    int n = 0, i; const dw_qi_row *rows; NEED_EXPORT();
    rows = dw_iid_table(&n);
    CHECK(n > 60);
    for (i = 0; i < n; i++) if (!is_factory_iid(rows[i].iid) && strncmp(rows[i].name, "IDWriteFactory", 14)) check_foreign_iid(FT_SHARED, &rows[i]);
}
TEST(iid_public_interfaces_isolated_all) {
    int n = 0, i; const dw_qi_row *rows; NEED_EXPORT();
    rows = dw_iid_table(&n);
    for (i = 0; i < n; i++) if (!is_factory_iid(rows[i].iid) && strncmp(rows[i].name, "IDWriteFactory", 14)) check_foreign_iid(FT_ISOLATED, &rows[i]);
}
/* One focused test per commonly confused interface. */
#define FOREIGN(nm) \
TEST(iid_foreign_##nm) { dw_qi_row r = { &IID_##nm, #nm, 0 }; NEED_EXPORT(); check_foreign_iid(FT_SHARED, &r); check_foreign_iid(FT_ISOLATED, &r); }
FOREIGN(IDWriteFontFace) FOREIGN(IDWriteFontFace1) FOREIGN(IDWriteFontFace2) FOREIGN(IDWriteFontFace3) FOREIGN(IDWriteFontFace4)
FOREIGN(IDWriteFontFace5) FOREIGN(IDWriteFontFace6) FOREIGN(IDWriteFont) FOREIGN(IDWriteFont1) FOREIGN(IDWriteFont2) FOREIGN(IDWriteFont3)
FOREIGN(IDWriteFontFamily) FOREIGN(IDWriteFontFamily1) FOREIGN(IDWriteFontFamily2) FOREIGN(IDWriteFontList) FOREIGN(IDWriteFontList1)
FOREIGN(IDWriteFontList2) FOREIGN(IDWriteFontCollection) FOREIGN(IDWriteFontCollection1) FOREIGN(IDWriteFontCollection2)
FOREIGN(IDWriteFontCollection3) FOREIGN(IDWriteFontSet) FOREIGN(IDWriteFontSet1) FOREIGN(IDWriteFontSet2) FOREIGN(IDWriteFontSet3)
FOREIGN(IDWriteFontSetBuilder) FOREIGN(IDWriteFontSetBuilder1) FOREIGN(IDWriteFontSetBuilder2) FOREIGN(IDWriteFontFile)
FOREIGN(IDWriteFontFileLoader) FOREIGN(IDWriteFontFileStream) FOREIGN(IDWriteFontCollectionLoader) FOREIGN(IDWriteFontFileEnumerator)
FOREIGN(IDWriteInMemoryFontFileLoader) FOREIGN(IDWriteFontDownloadQueue) FOREIGN(IDWriteFontDownloadListener)
FOREIGN(IDWriteTextFormat) FOREIGN(IDWriteTextFormat1) FOREIGN(IDWriteTextFormat2) FOREIGN(IDWriteTextFormat3) FOREIGN(IDWriteTextLayout)
FOREIGN(IDWriteTextLayout1) FOREIGN(IDWriteTextLayout2) FOREIGN(IDWriteTextLayout3) FOREIGN(IDWriteTextLayout4)
FOREIGN(IDWriteTextAnalyzer) FOREIGN(IDWriteTextAnalyzer1) FOREIGN(IDWriteTextAnalyzer2) FOREIGN(IDWriteTextAnalysisSource)
FOREIGN(IDWriteTextAnalysisSink) FOREIGN(IDWriteTextRenderer) FOREIGN(IDWriteTextRenderer1) FOREIGN(IDWritePixelSnapping)
FOREIGN(IDWriteGdiInterop) FOREIGN(IDWriteGdiInterop1) FOREIGN(IDWriteBitmapRenderTarget) FOREIGN(IDWriteBitmapRenderTarget1)
FOREIGN(IDWriteRenderingParams) FOREIGN(IDWriteRenderingParams1) FOREIGN(IDWriteRenderingParams2) FOREIGN(IDWriteRenderingParams3)
FOREIGN(IDWriteGlyphRunAnalysis) FOREIGN(IDWriteColorGlyphRunEnumerator) FOREIGN(IDWriteColorGlyphRunEnumerator1)
FOREIGN(IDWriteLocalizedStrings) FOREIGN(IDWriteTypography) FOREIGN(IDWriteInlineObject) FOREIGN(IDWriteFontFallback)
FOREIGN(IDWriteFontFallback1) FOREIGN(IDWriteFontFallbackBuilder) FOREIGN(IDWriteNumberSubstitution) FOREIGN(IDWriteFontResource)
FOREIGN(IDWriteFontFaceReference) FOREIGN(IDWriteFontFaceReference1) FOREIGN(IDWriteLocalFontFileLoader)

TEST(iid_win11_interfaces_absent) {
    /* IDWriteFactory8 and the Windows 11 interfaces are not part of the 19041 ComInterfaceList. */
    dw_qi_row r8 = { &DW_IID_IDWriteFactory8, "IDWriteFactory8", 0 };
    NEED_EXPORT();
    check_foreign_iid(FT_SHARED, &r8); check_foreign_iid(FT_ISOLATED, &r8);
    { dw_qi_row r = { &DW_IID_IDWriteFontFace7, "IDWriteFontFace7", 0 }; check_foreign_iid(FT_SHARED, &r); check_foreign_iid(FT_ISOLATED, &r); }
    { dw_qi_row r = { &DW_IID_IDWritePaintReader, "IDWritePaintReader", 0 }; check_foreign_iid(FT_SHARED, &r); check_foreign_iid(FT_ISOLATED, &r); }
}
TEST(iid_null_guid) {
    void *p = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR(fc_create(FT_SHARED, &fc_null_guid, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
    p = FC_SENTINEL;
    CHECK_HR(fc_create(FT_ISOLATED, &fc_null_guid, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
}
TEST(iid_random_guids) {
    int i, bad = 0; NEED_EXPORT();
    for (i = 0; i < 64; i++) {
        GUID g; void *p = FC_SENTINEL; HRESULT hr;
        g.Data1 = 0x9e3779b1u * (i + 1); g.Data2 = (WORD)(0x1234 + i); g.Data3 = (WORD)(0xabcd ^ i);
        memset(g.Data4, (i * 7 + 3) & 0xff, 8);
        hr = fc_create(i & 1, &g, &p);
        if (hr != E_NOINTERFACE || p != NULL) { bad++; DW_RELEASE(p); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(iid_near_misses_of_factory_iid) {
    /* One flipped bit anywhere in the IID of IDWriteFactory must not be accepted. */
    int bit, bad = 0; NEED_EXPORT();
    for (bit = 0; bit < 128; bit++) {
        GUID g = IID_IDWriteFactory; void *p = FC_SENTINEL; HRESULT hr;
        ((BYTE *)&g)[bit / 8] ^= (BYTE)(1 << (bit % 8));
        hr = fc_create(FT_SHARED, &g, &p);
        if (hr != E_NOINTERFACE || p != NULL) { bad++; DW_RELEASE(p); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(iid_near_misses_of_iunknown) {
    int bit, bad = 0; NEED_EXPORT();
    for (bit = 0; bit < 128; bit++) {
        GUID g = IID_IUnknown; void *p = FC_SENTINEL; HRESULT hr;
        ((BYTE *)&g)[bit / 8] ^= (BYTE)(1 << (bit % 8));
        hr = fc_create(FT_ISOLATED, &g, &p);
        if (hr != E_NOINTERFACE || p != NULL) { bad++; DW_RELEASE(p); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(iid_near_misses_of_factory7) {
    int bit, bad = 0; NEED_EXPORT();
    for (bit = 0; bit < 128; bit++) {
        GUID g = IID_IDWriteFactory7; void *p = FC_SENTINEL; HRESULT hr;
        ((BYTE *)&g)[bit / 8] ^= (BYTE)(1 << (bit % 8));
        hr = fc_create(FT_ISOLATED, &g, &p);
        if (hr != E_NOINTERFACE || p != NULL) { bad++; DW_RELEASE(p); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(iid_ole_interfaces_not_implemented) {
    /* Plain OLE interfaces that a DirectWrite factory does not expose. */
    static const struct { const IID *iid; const char *n; } t[] = {
        { &IID_IDispatch, "IDispatch" }, { &IID_IPersist, "IPersist" }, { &IID_IStream, "IStream" }, { &IID_IClassFactory, "IClassFactory" },
        { &IID_IEnumUnknown, "IEnumUnknown" }, { &IID_IPropertyBag, "IPropertyBag" }, { &IID_IDataObject, "IDataObject" },
        { &IID_ISequentialStream, "ISequentialStream" }, { &IID_IStorage, "IStorage" },
    };
    size_t i; NEED_EXPORT();
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        dw_qi_row r; r.iid = t[i].iid; r.name = t[i].n; r.expect = 0;
        check_foreign_iid(FT_SHARED, &r); check_foreign_iid(FT_ISOLATED, &r);
    }
}
TEST(iid_marshaling_and_agility_probes) {
    /* Whether the factory is agile or marshalable is an implementation detail: record it. */
    static const struct { const IID *iid; const char *n; } t[] = {
        { &IID_IMarshal, "IMarshal" }, { &IID_INoMarshal, "INoMarshal" }, { &IID_IAgileObject, "IAgileObject" },
        { &IID_IStdMarshalInfo, "IStdMarshalInfo" }, { &IID_IExternalConnection, "IExternalConnection" },
    };
    size_t i; NEED_EXPORT();
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        void *p = FC_SENTINEL; HRESULT hr = fc_create(FT_ISOLATED, t[i].iid, &p);
        t_note("isolated factory QI %s -> %s", t[i].n, dw_hr_name(hr));
        CHECK(SUCCEEDED(hr) ? (p != NULL && p != FC_SENTINEL) : p == NULL);
        if (SUCCEEDED(hr)) DW_RELEASE(p);
    }
}
TEST(iid_shared_failure_keeps_refcount) {
    void *a = NULL, *bad = FC_SENTINEL; ULONG r1, r2; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IUnknown, &a));
    if (!a) return;
    r1 = dw_refcount((IUnknown *)a);
    CHECK_HR(fc_create(FT_SHARED, &IID_IDWriteFontFace, &bad), E_NOINTERFACE);
    CHECK_OUT_NULL(bad);
    r2 = dw_refcount((IUnknown *)a);
    CHECK_FLOAT_EQ((float)r1, (float)r2);
    DW_RELEASE(a);
}
TEST(iid_factory_matrix_shared) {
    dw_qi_row rows[11]; int v; void *p = NULL; NEED_EXPORT();
    for (v = 0; v < 8; v++) { rows[v].iid = dw_factory_iid(v); rows[v].name = ver_names[v]; rows[v].expect = 1; }
    rows[8].iid = &IID_IUnknown; rows[8].name = "IUnknown"; rows[8].expect = 1;
    rows[9].iid = &DW_IID_IDWriteFactory8; rows[9].name = "IDWriteFactory8"; rows[9].expect = 0;
    rows[10].iid = &IID_IDWriteFontFace; rows[10].name = "IDWriteFontFace"; rows[10].expect = 0;
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, &p));
    if (p) { DW_CHECK_QI(p, rows); DW_RELEASE(p); }
}
TEST(iid_factory_matrix_isolated) {
    dw_qi_row rows[11]; int v; void *p = NULL; NEED_EXPORT();
    for (v = 0; v < 8; v++) { rows[v].iid = dw_factory_iid(v); rows[v].name = ver_names[v]; rows[v].expect = 1; }
    rows[8].iid = &IID_IUnknown; rows[8].name = "IUnknown"; rows[8].expect = 1;
    rows[9].iid = &DW_IID_IDWriteFactory8; rows[9].name = "IDWriteFactory8"; rows[9].expect = 0;
    rows[10].iid = &IID_IDWriteTextFormat; rows[10].name = "IDWriteTextFormat"; rows[10].expect = 0;
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory, &p));
    if (p) { DW_CHECK_QI(p, rows); DW_RELEASE(p); }
}
TEST(iid_qi_from_newest_back_to_oldest) {
    /* From a Factory7 pointer every older interface is reachable (the object implements the whole chain). */
    void *p7 = NULL; int v; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory7, &p7));
    if (!p7) return;
    for (v = 0; v < 8; v++) {
        void *q = FC_SENTINEL;
        HRESULT hr = IUnknown_QueryInterface((IUnknown *)p7, dw_factory_iid(v), &q);
        CHECKF(hr == S_OK, "%s: %s", ver_names[v], dw_hr_name(hr));
        if (hr == S_OK) CHECK(dw_same_identity((IUnknown *)p7, (IUnknown *)q));
        DW_RELEASE(q);
    }
    DW_RELEASE(p7);
}
TEST(iid_qi_from_oldest_up_to_newest) {
    void *p0 = NULL; int v; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory, &p0));
    if (!p0) return;
    for (v = 0; v < 8; v++) {
        void *q = FC_SENTINEL;
        HRESULT hr = IUnknown_QueryInterface((IUnknown *)p0, dw_factory_iid(v), &q);
        CHECKF(hr == S_OK, "%s: %s", ver_names[v], dw_hr_name(hr));
        DW_RELEASE(q);
    }
    DW_RELEASE(p0);
}
/* Calling a method that only exists in generation N through the pointer proves the vtable is the right one. */
TEST(iid_vtable_factory1_eudc) {
    IDWriteFactory1 *f1 = NULL; IDWriteFontCollection *c = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory1, (void **)&f1));
    if (!f1) return;
    CHECK_HR_OK(IDWriteFactory1_GetEudcFontCollection(f1, &c, FALSE));
    CHECK(c != NULL);
    DW_RELEASE(c); DW_RELEASE(f1);
}
TEST(iid_vtable_factory2_fallback) {
    IDWriteFactory2 *f2 = NULL; IDWriteFontFallback *fb = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory2, (void **)&f2));
    if (!f2) return;
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &fb));
    CHECK(fb != NULL);
    DW_RELEASE(fb); DW_RELEASE(f2);
}
TEST(iid_vtable_factory2_fallback_builder) {
    IDWriteFactory2 *f2 = NULL; IDWriteFontFallbackBuilder *b = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory2, (void **)&f2));
    if (!f2) return;
    CHECK_HR_OK(IDWriteFactory2_CreateFontFallbackBuilder(f2, &b));
    CHECK(b != NULL);
    DW_RELEASE(b); DW_RELEASE(f2);
}
TEST(iid_vtable_factory3_fontsetbuilder) {
    IDWriteFactory3 *f3 = NULL; IDWriteFontSetBuilder *b = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory3, (void **)&f3));
    if (!f3) return;
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(f3, &b));
    CHECK(b != NULL);
    DW_RELEASE(b); DW_RELEASE(f3);
}
TEST(iid_vtable_factory3_system_fontset) {
    IDWriteFactory3 *f3 = NULL; IDWriteFontSet *s = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory3, (void **)&f3));
    if (!f3) return;
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s));
    CHECK(s != NULL);
    if (s) t_note("system font set has %u fonts", IDWriteFontSet_GetFontCount(s));
    DW_RELEASE(s); DW_RELEASE(f3);
}
TEST(iid_vtable_factory5_fontsetbuilder1) {
    IDWriteFactory5 *f5 = NULL; IDWriteFontSetBuilder1 *b = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory5, (void **)&f5));
    if (!f5) return;
    CHECK_HR_OK(IDWriteFactory5_CreateFontSetBuilder(f5, &b));
    CHECK(b != NULL);
    DW_RELEASE(b); DW_RELEASE(f5);
}
TEST(iid_vtable_factory0_rendering_params) {
    IDWriteFactory *f = NULL; IDWriteRenderingParams *rp = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory, (void **)&f));
    if (!f) return;
    CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(f, &rp));
    CHECK(rp != NULL);
    DW_RELEASE(rp); DW_RELEASE(f);
}
