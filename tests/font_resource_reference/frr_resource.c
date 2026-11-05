/* IDWriteFontResource: IDWriteFactory6::CreateFontResource(file, faceIndex), IDWriteFontFace5::GetFontResource, and every method of the
 * interface (GetFontFile, GetFontFaceIndex, GetFontAxisCount, GetDefaultFontAxisValues, GetFontAxisRanges, GetFontAxisAttributes, GetAxisNames,
 * GetAxisValueNameCount, GetAxisValueNames, HasVariations, CreateFontFace, CreateFontFaceReference).
 *
 * Win10 dwrite.dll (IDWriteFactory::CreateFontResource, IDWriteFontFace::GetFontResource, methods
 * GetFontFile, GetFontFaceIndex, GetFontAxisCount, GetDefaultFontAxisValues, GetFontAxisRanges,
 * GetFontAxisAttributes, GetAxisNames, GetAxisValueNameCount, GetAxisValueNames, HasVariations,
 * CreateFontFace, CreateFontFaceReference):
 *   - CreateFontResource zeroes *resource, rejects a NULL file with E_INVALIDARG, does not read the font; the resource AddRefs the file it was
 *     created from and GetFontFile hands that same object back (AddRef'd). A resource made by GetFontResource builds its IDWriteFontFile on
 *     the first GetFontFile call and keeps returning that one object.
 *   - GetDefaultFontAxisValues / GetFontAxisRanges fill the caller buffer (zeroed first) by iterating over the BUFFER COUNT and indexing the
 *     font's axis table, so the count must equal GetFontAxisCount(): a smaller buffer raises ArgumentInsufficientBufferException and a larger
 *     one indexes past the table. Both are C++ exceptions leaving the call, so only the exact count is exercised.
 *   - GetFontAxisAttributes / GetAxisNames / GetAxisValueNameCount / GetAxisValueNames throw ArgumentException for an axis (or value) index that
 *     is out of range; those indices are therefore never passed.
 *   - HasVariations is bit 0 of the face's font flags (fvar present). GetAxisNames / GetAxisValueNames return S_OK and an object even when the
 *     axis has no names.
 *   - CreateFontFace / CreateFontFaceReference: simulations & ~3, count > 0xFFFF, or NULL values with a non-zero count are E_INVALIDARG.
 */
#include "frr.h"

#define SIM_NONE DWRITE_FONT_SIMULATIONS_NONE
#define SIM_BOLD DWRITE_FONT_SIMULATIONS_BOLD
#define TAG_W DWRITE_FONT_AXIS_TAG_WEIGHT
#define TAG_D DWRITE_FONT_AXIS_TAG_WIDTH
#define TAG_XTRA DW_TAG('X', 'T', 'R', 'A')
#define TAG_HIDN DW_TAG('H', 'I', 'D', 'N')
#define TAG_FIXD DW_TAG('F', 'I', 'X', 'D')

static IDWriteFontResource *mkres(frr_fix *x, UINT32 idx) {
    IDWriteFontResource *r = NULL;
    HRESULT hr = IDWriteFactory6_CreateFontResource(x->f6, x->file, idx, &r);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? r : NULL;
}
static int find_tag(const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, UINT32 tag) { UINT32 i; for (i = 0; i < n; i++) if (v[i].axisTag == tag) return (int)i; return -1; }
static int find_range(const DWRITE_FONT_AXIS_RANGE *v, UINT32 n, UINT32 tag) { UINT32 i; for (i = 0; i < n; i++) if (v[i].axisTag == tag) return (int)i; return -1; }

/* ---- creation ------------------------------------------------------------------------------------------------------------------- */
TEST(create_basic_and_refcount) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = (IDWriteFontResource *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory6_CreateFontResource(x.f6, x.file, 0, &r), S_OK);
    CHECK(r != NULL && r != (IDWriteFontResource *)(ULONG_PTR)8);
    if (r && r != (IDWriteFontResource *)(ULONG_PTR)8) EXPECT_REFS(r, 1); else r = NULL;
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(create_null_file_invalidarg) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = (IDWriteFontResource *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFactory6_CreateFontResource(x.f6, NULL, 0, &r), FRR_EINVALIDARG);
    FRR_ZERO(r);
    frr_close(&x);
}
TEST(create_retains_the_file_object) {
    FRR_OPEN_NEED6(x);
    ULONG before = dw_refcount((IUnknown *)x.file);
    IDWriteFontResource *r = mkres(&x, 0);
    if (r) {
        EXPECT_REFS(x.file, before + 1);
        DW_RELEASE(r);
        EXPECT_REFS(x.file, before);
    }
    frr_close(&x);
}
TEST(create_does_not_validate_face_index) {
    FRR_OPEN_NEED6(x);
    static const UINT32 idx[] = { 0, 1, 5, 1000 };
    size_t i;
    for (i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        IDWriteFontResource *r = (IDWriteFontResource *)(ULONG_PTR)8;
        HRESULT hr = IDWriteFactory6_CreateFontResource(x.f6, x.file, idx[i], &r);
        t_note("CreateFontResource(index %u over a single-face font) -> %s", idx[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && r != (IDWriteFontResource *)(ULONG_PTR)8) { CHECK(r != NULL); if (r) EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(r), idx[i]); DW_RELEASE(r); }
        else { FRR_ZERO(r); }
    }
    frr_close(&x);
}
TEST(create_two_resources_are_distinct) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *a = mkres(&x, 0), *b = mkres(&x, 0);
    CHECK(a && b && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    frr_close(&x);
}
TEST(create_via_every_factory_interface_with_the_method) {
    FRR_OPEN_NEED6(x);
    int v;
    for (v = 6; v <= 7; v++) {
        void *fv = NULL;
        if (FAILED(IDWriteFactory_QueryInterface(x.f, dw_factory_iid(v), &fv)) || !fv) { t_note("IDWriteFactory%d unsupported", v); continue; }
        IDWriteFontResource *r = NULL;
        CHECK_HR(IDWriteFactory6_CreateFontResource((IDWriteFactory6 *)fv, x.file, 0, &r), S_OK);
        CHECK(r != NULL);
        DW_RELEASE(r); IUnknown_Release((IUnknown *)fv);
    }
    frr_close(&x);
}
TEST(resource_qi_matrix) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    if (r) {
        dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteFontResource, 1), DW_QI_ROW(IDWriteFontFaceReference, 0), DW_QI_ROW(IDWriteFontFaceReference1, 0), DW_QI_ROW(IDWriteFontFace, 0),
            DW_QI_ROW(IDWriteFontFace5, 0), DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0),
            DW_QI_ROW(IDWriteLocalizedStrings, 0), DW_QI_ROW(IDWriteFontDownloadQueue, 0),
        };
        DW_CHECK_QI(r, rows);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(resource_qi_failure_zeroes_output) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    if (r) {
        void *pv = (void *)(ULONG_PTR)8;
        CHECK_HR(IDWriteFontResource_QueryInterface(r, &IID_IDWriteFontFile, &pv), E_NOINTERFACE);
        FRR_ZERO(pv);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(resource_survives_file_and_factory_release) {
    frr_fix x;
    size_t n; uint8_t *b = ttf_build_default(&n);
    if (b && frr_open(&x, b, n) && x.f6) {
        IDWriteFontResource *r = mkres(&x, 0);
        if (r) {
            DW_RELEASE(x.file);
            dwx_loader_unregister(x.ld);       /* nothing may touch the font data from here on */
            DW_RELEASE(x.f6); DW_RELEASE(x.f5); DW_RELEASE(x.f3); DW_RELEASE(x.f);       /* the resource keeps its factory alive */
            EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(r), 0);
            IDWriteFontFile *ff = NULL;
            CHECK_HR(IDWriteFontResource_GetFontFile(r, &ff), S_OK);
            CHECK(ff != NULL);
            DW_RELEASE(ff); DW_RELEASE(r);
        }
    } else if (b && x.f3 && !x.f6) t_skip("IDWriteFactory6 unsupported");
    frr_close(&x); free(b);
}

/* ---- GetFontFile / GetFontFaceIndex ------------------------------------------------------------------------------------------------- */
TEST(getfontfile_returns_the_creating_file_object) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    if (r) {
        ULONG before = dw_refcount((IUnknown *)x.file);
        IDWriteFontFile *a = (IDWriteFontFile *)(ULONG_PTR)8, *b = NULL;
        CHECK_HR(IDWriteFontResource_GetFontFile(r, &a), S_OK);
        CHECK_HR(IDWriteFontResource_GetFontFile(r, &b), S_OK);
        CHECK(a == x.file && b == x.file);
        EXPECT_REFS(x.file, before + 2);               /* each call hands out a new reference */
        if (a != x.file) a = NULL;
        DW_RELEASE(a); DW_RELEASE(b);
        EXPECT_REFS(x.file, before);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(getfontfile_loader_and_key) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    IDWriteFontFile *ff = NULL; IDWriteFontFileLoader *ld = NULL;
    if (r && SUCCEEDED(IDWriteFontResource_GetFontFile(r, &ff)) && ff && SUCCEEDED(IDWriteFontFile_GetLoader(ff, &ld))) {
        CHECK(ld == (IDWriteFontFileLoader *)x.ld);
        const void *key = NULL; UINT32 ks = 0;
        CHECK_HR(IDWriteFontFile_GetReferenceKey(ff, &key, &ks), S_OK);
        t_check(ks == 4 && key && *(const UINT32 *)key == x.key, __FILE__, __LINE__, "key", "size %u", ks);
    }
    DW_RELEASE(ld); DW_RELEASE(ff); DW_RELEASE(r);
    frr_close(&x);
}
TEST(face_index_roundtrip) {
    FRR_OPEN_NEED6(x);
    static const UINT32 idx[] = { 0, 1, 2, 7 };
    size_t i;
    for (i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        IDWriteFontResource *r = (IDWriteFontResource *)(ULONG_PTR)0;
        if (FAILED(IDWriteFactory6_CreateFontResource(x.f6, x.file, idx[i], &r)) || !r) continue;
        t_check(IDWriteFontResource_GetFontFaceIndex(r) == idx[i], __FILE__, __LINE__, "index", "%u", idx[i]);
        DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(ttc_member_resource_reports_its_index) {
    size_t n1, n2, nt; uint8_t *b1 = ttf_build_styled("TtcA", 400, 5, 0, &n1), *b2 = ttf_build_styled("TtcB", 700, 5, 0, &n2);
    const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &nt);
    frr_fix x;
    if (b1 && b2 && ttc && frr_open(&x, ttc, nt) && x.f6) {
        IDWriteFontResource *r = mkres(&x, 1);
        IDWriteFontFace5 *f5 = NULL;
        if (r) {
            EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(r), 1);
            if (SUCCEEDED(IDWriteFontResource_CreateFontFace(r, SIM_NONE, NULL, 0, &f5)) && f5) {
                EXPECT_EQ(IDWriteFontFace5_GetIndex(f5), 1);
                EXPECT_EQ(IDWriteFontFace5_GetWeight(f5), DWRITE_FONT_WEIGHT_BOLD);
            }
        }
        DW_RELEASE(f5); DW_RELEASE(r);
    } else if (!(b1 && b2 && ttc)) t_skip("ttf_build_ttc failed");
    else if (x.f3 && !x.f6) t_skip("IDWriteFactory6 unsupported");
    frr_close(&x);
    free(b1); free(b2); free(ttc);
}

/* ---- IDWriteFontFace5::GetFontResource ------------------------------------------------------------------------------------------------- */
TEST(face_getfontresource_basic) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *ref = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFace5 *f5 = NULL; IDWriteFontResource *r = (IDWriteFontResource *)(ULONG_PTR)8;
    IDWriteFontFace3 *f3 = NULL;
    if (ref && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(ref, &f3)) && f3 && SUCCEEDED(IDWriteFontFace3_QueryInterface(f3, &IID_IDWriteFontFace5, (void **)&f5)) && f5) {
        CHECK_HR(IDWriteFontFace5_GetFontResource(f5, &r), S_OK);
        CHECK(r != NULL && r != (IDWriteFontResource *)(ULONG_PTR)8);
        if (r && r != (IDWriteFontResource *)(ULONG_PTR)8) {
            EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(r), 0);
            EXPECT_EQ(IDWriteFontResource_HasVariations(r), FALSE);
        } else r = NULL;
    }
    DW_RELEASE(r); DW_RELEASE(f5); DW_RELEASE(f3); DW_RELEASE(ref);
    frr_close(&x);
}
TEST(face_getfontresource_file_is_created_lazily_and_cached) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *ref = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFace3 *f3 = NULL; IDWriteFontFace5 *f5 = NULL; IDWriteFontResource *r = NULL;
    if (ref && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(ref, &f3)) && f3 && SUCCEEDED(IDWriteFontFace3_QueryInterface(f3, &IID_IDWriteFontFace5, (void **)&f5)) && f5 &&
        SUCCEEDED(IDWriteFontFace5_GetFontResource(f5, &r)) && r) {
        IDWriteFontFile *a = NULL, *b = NULL;
        CHECK_HR(IDWriteFontResource_GetFontFile(r, &a), S_OK);
        CHECK_HR(IDWriteFontResource_GetFontFile(r, &b), S_OK);
        CHECK(a != NULL && a == b);              /* built on first use, then the same object every time */
        CHECK(a != x.file);
        IDWriteFontFileLoader *ld = NULL;
        if (a && SUCCEEDED(IDWriteFontFile_GetLoader(a, &ld))) CHECK(ld == (IDWriteFontFileLoader *)x.ld);
        DW_RELEASE(ld); DW_RELEASE(a); DW_RELEASE(b);
    }
    DW_RELEASE(r); DW_RELEASE(f5); DW_RELEASE(f3); DW_RELEASE(ref);
    frr_close(&x);
}
TEST(face_getfontresource_ttc_index) {
    size_t n1, n2, nt; uint8_t *b1 = ttf_build_styled("TtcA", 400, 5, 0, &n1), *b2 = ttf_build_styled("TtcB", 700, 5, 0, &n2);
    const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &nt);
    frr_fix x;
    if (b1 && b2 && ttc && frr_open(&x, ttc, nt)) {
        UINT32 i;
        for (i = 0; i < 2; i++) {
            IDWriteFontFaceReference *ref = frr_ref(&x, i, SIM_NONE);
            IDWriteFontFace3 *f3 = NULL; IDWriteFontFace5 *f5 = NULL; IDWriteFontResource *r = NULL;
            if (ref && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(ref, &f3)) && f3 && SUCCEEDED(IDWriteFontFace3_QueryInterface(f3, &IID_IDWriteFontFace5, (void **)&f5)) && f5 &&
                SUCCEEDED(IDWriteFontFace5_GetFontResource(f5, &r)) && r)
                t_check(IDWriteFontResource_GetFontFaceIndex(r) == i, __FILE__, __LINE__, "index", "member %u", i);
            DW_RELEASE(r); DW_RELEASE(f5); DW_RELEASE(f3); DW_RELEASE(ref);
        }
    } else if (!(b1 && b2 && ttc)) t_skip("ttf_build_ttc failed");
    frr_close(&x);
    free(b1); free(b2); free(ttc);
}

/* ---- static font ------------------------------------------------------------------------------------------------------------------------ */
TEST(static_font_has_no_variations) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    if (r) {
        BOOL hv = IDWriteFontResource_HasVariations(r);
        EXPECT_EQ(hv, 0);
        UINT32 na = IDWriteFontResource_GetFontAxisCount(r);
        t_note("static synthetic font: GetFontAxisCount = %u", na);
        if (na == 0) {
            CHECK_HR(IDWriteFontResource_GetDefaultFontAxisValues(r, NULL, 0), S_OK);
            CHECK_HR(IDWriteFontResource_GetFontAxisRanges(r, NULL, 0), S_OK);
        }
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(static_system_fonts_have_no_variations) {
    IDWriteFactory3 *f3 = frr_shared3(); IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    if (!f3 || !f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    static const char *const names[] = { "arial.ttf", "times.ttf", "cour.ttf", "segoeui.ttf", "tahoma.ttf" };
    size_t i; int any = 0;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        WCHAR p[MAX_PATH]; IDWriteFontFile *file = NULL; IDWriteFontResource *r = NULL;
        if (!frr_sys_path(names[i], p, MAX_PATH) || FAILED(IDWriteFactory_CreateFontFileReference(dw_shared(), p, NULL, &file)) || !file) continue;
        if (SUCCEEDED(IDWriteFactory6_CreateFontResource(f6, file, 0, &r)) && r) {
            any = 1;
            t_check(IDWriteFontResource_HasVariations(r) == FALSE, __FILE__, __LINE__, "static", "%s", names[i]);
            t_note("%s: axis count %u", names[i], IDWriteFontResource_GetFontAxisCount(r));
            DW_RELEASE(r);
        }
        DW_RELEASE(file);
    }
    if (!any) t_skip("no system fonts found");
}

/* ---- synthetic variable font ------------------------------------------------------------------------------------------------------------- */
static int open_var(frr_fix *x, IDWriteFontResource **r, uint8_t **blob) {
    size_t n; *r = NULL; *blob = frr_build_var2(&n);
    if (!*blob) { memset(x, 0, sizeof *x); t_skip("frr_build_var2 failed"); return 0; }
    if (!frr_open(x, *blob, n)) return 0;
    if (!x->f6) { t_skip("IDWriteFactory6 unsupported"); return 0; }
    *r = mkres(x, 0);
    return *r != NULL;
}
#define OPEN_VAR(x, r, b) frr_fix x; IDWriteFontResource *r; uint8_t *b; if (!open_var(&x, &r, &b)) { DW_RELEASE(r); frr_close(&x); free(b); return; }
#define CLOSE_VAR(x, r, b) do { DW_RELEASE(r); frr_close(&x); free(b); } while (0)

/* DirectWrite may list axes the font does not declare (the standard weight / width / slant / italic set), so the axis count is only bounded from
 * below by the fvar axis count; the fvar axes are located by tag. */
#define MAXAX 24
typedef struct { UINT32 n; DWRITE_FONT_AXIS_VALUE def[MAXAX]; DWRITE_FONT_AXIS_RANGE rg[MAXAX]; } axes;
static int load_axes(IDWriteFontResource *r, axes *a) {
    memset(a, 0, sizeof *a);
    a->n = IDWriteFontResource_GetFontAxisCount(r);
    if (a->n == 0) return 1;
    if (a->n > MAXAX) { t_check(0, __FILE__, __LINE__, "axis count", "implausible axis count %u", a->n); return 0; }
    CHECK_HR(IDWriteFontResource_GetDefaultFontAxisValues(r, a->def, a->n), S_OK);
    CHECK_HR(IDWriteFontResource_GetFontAxisRanges(r, a->rg, a->n), S_OK);
    return 1;
}
TEST(var_has_variations_and_axis_count) {
    OPEN_VAR(x, r, b);
    BOOL hv = IDWriteFontResource_HasVariations(r);
    EXPECT_EQ(hv, 1);
    UINT32 na = IDWriteFontResource_GetFontAxisCount(r);
    t_note("synthetic font with 2 fvar axes: GetFontAxisCount = %u", na);
    CHECK(na >= 2);
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(r), na);     /* stable */
    CLOSE_VAR(x, r, b);
}
TEST(var_default_values) {
    OPEN_VAR(x, r, b);
    axes a;
    if (load_axes(r, &a)) {
        int w = find_tag(a.def, a.n, TAG_W), xt = find_tag(a.def, a.n, TAG_XTRA); UINT32 i;
        CHECK(w >= 0 && xt >= 0 && w != xt);
        if (w >= 0) EXPECT_EQ(a.def[w].value, 400.0f);
        if (xt >= 0) EXPECT_EQ(a.def[xt].value, 50.0f);
        for (i = 0; i < a.n; i++) t_note("axis %u tag %08x default %g", i, a.def[i].axisTag, a.def[i].value);
    }
    CLOSE_VAR(x, r, b);
}
TEST(var_default_values_buffer_is_overwritten) {
    OPEN_VAR(x, r, b);
    UINT32 na = IDWriteFontResource_GetFontAxisCount(r);
    if (na > 0 && na <= MAXAX) {
        DWRITE_FONT_AXIS_VALUE v[MAXAX]; memset(v, 0xCC, sizeof v);
        CHECK_HR(IDWriteFontResource_GetDefaultFontAxisValues(r, v, na), S_OK);
        UINT32 i; int stale = 0;
        for (i = 0; i < na; i++) if (v[i].axisTag == 0xCCCCCCCC) stale++;
        EXPECT_EQ(stale, 0);
        EXPECT_EQ(v[na].axisTag, 0xCCCCCCCC);        /* nothing is written past the requested count */
    }
    CLOSE_VAR(x, r, b);
}
TEST(var_axis_ranges) {
    OPEN_VAR(x, r, b);
    axes a;
    if (load_axes(r, &a)) {
        int w = find_range(a.rg, a.n, TAG_W), xt = find_range(a.rg, a.n, TAG_XTRA);
        CHECK(w >= 0 && xt >= 0 && w != xt);
        if (w >= 0) { EXPECT_EQ(a.rg[w].minValue, 100.0f); EXPECT_EQ(a.rg[w].maxValue, 900.0f); }
        if (xt >= 0) { EXPECT_EQ(a.rg[xt].minValue, 10.0f); EXPECT_EQ(a.rg[xt].maxValue, 90.0f); }
    }
    CLOSE_VAR(x, r, b);
}
TEST(var_ranges_and_defaults_use_the_same_axis_order) {
    OPEN_VAR(x, r, b);
    axes a;
    if (load_axes(r, &a)) {
        UINT32 i;
        for (i = 0; i < a.n; i++) {
            t_check(a.def[i].axisTag == a.rg[i].axisTag, __FILE__, __LINE__, "same order", "slot %u: %08x vs %08x", i, a.def[i].axisTag, a.rg[i].axisTag);
            t_check(a.def[i].value >= a.rg[i].minValue && a.def[i].value <= a.rg[i].maxValue, __FILE__, __LINE__, "default inside range", "slot %u", i);
        }
    }
    CLOSE_VAR(x, r, b);
}
TEST(var_axis_tags_are_unique) {
    OPEN_VAR(x, r, b);
    axes a;
    if (load_axes(r, &a)) {
        UINT32 i, j;
        for (i = 0; i < a.n; i++) for (j = i + 1; j < a.n; j++) t_check(a.def[i].axisTag != a.def[j].axisTag, __FILE__, __LINE__, "unique tags", "slots %u and %u: %08x", i, j, a.def[i].axisTag);
    }
    CLOSE_VAR(x, r, b);
}
TEST(var_attributes_of_declared_axes) {
    OPEN_VAR(x, r, b);
    axes a;
    if (load_axes(r, &a)) {
        UINT32 i;
        for (i = 0; i < a.n; i++) {
            DWRITE_FONT_AXIS_ATTRIBUTES at = IDWriteFontResource_GetFontAxisAttributes(r, i);
            t_check((at & ~3) == 0, __FILE__, __LINE__, "known bits only", "axis %u (%08x) attrs %d", i, a.def[i].axisTag, (int)at);
            if (a.def[i].axisTag == TAG_W || a.def[i].axisTag == TAG_XTRA) {
                t_check((at & DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE) != 0, __FILE__, __LINE__, "VARIABLE", "axis %u (%08x) attrs %d", i, a.def[i].axisTag, (int)at);
                t_check((at & DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN) == 0, __FILE__, __LINE__, "not HIDDEN", "axis %u (%08x) attrs %d", i, a.def[i].axisTag, (int)at);
            } else t_note("undeclared axis %u (%08x) attributes %d range [%g, %g]", i, a.def[i].axisTag, (int)at, a.rg[i].minValue, a.rg[i].maxValue);
        }
    }
    CLOSE_VAR(x, r, b);
}
TEST(var_axis_names_come_from_the_name_table) {
    OPEN_VAR(x, r, b);
    axes a;
    if (load_axes(r, &a)) {
        UINT32 i;
        for (i = 0; i < a.n; i++) {
            IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)8;
            CHECK_HR(IDWriteFontResource_GetAxisNames(r, i, &s), S_OK);
            CHECK(s != NULL && s != (IDWriteLocalizedStrings *)(ULONG_PTR)8);
            if (s && s != (IDWriteLocalizedStrings *)(ULONG_PTR)8) {
                UINT32 cnt = IDWriteLocalizedStrings_GetCount(s); char en[128] = "";
                t_note("axis %u (%08x) has %u name(s)", i, a.def[i].axisTag, cnt);
                if (cnt > 0 && SUCCEEDED(dw_ls_en(s, en, sizeof en))) {
                    t_note("  en name: \"%s\"", en);
                    if (a.def[i].axisTag == TAG_XTRA) EXPECT_STR(en, "Extra Axis");
                }
                if (a.def[i].axisTag == TAG_XTRA) CHECK(cnt >= 1);
            } else s = NULL;
            DW_RELEASE(s);
        }
    }
    CLOSE_VAR(x, r, b);
}
TEST(var_axis_value_names_for_every_listed_value) {
    OPEN_VAR(x, r, b);
    UINT32 na = IDWriteFontResource_GetFontAxisCount(r), i;
    for (i = 0; i < na && i < MAXAX; i++) {
        UINT32 c = IDWriteFontResource_GetAxisValueNameCount(r, i);
        t_note("axis %u value-name count = %u", i, c);
        if (c < 64) {
            UINT32 j;
            for (j = 0; j < c; j++) {
                DWRITE_FONT_AXIS_RANGE rg; IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)8;
                memset(&rg, 0xCC, sizeof rg);
                CHECK_HR(IDWriteFontResource_GetAxisValueNames(r, i, j, &rg, &s), S_OK);
                CHECK(s != NULL && s != (IDWriteLocalizedStrings *)(ULONG_PTR)8);
                if (s == (IDWriteLocalizedStrings *)(ULONG_PTR)8) s = NULL;
                DW_RELEASE(s);
            }
        }
    }
    CLOSE_VAR(x, r, b);
}
TEST(var_create_font_face_applies_axis_values) {
    OPEN_VAR(x, r, b);
    DWRITE_FONT_AXIS_VALUE av[2] = { { TAG_W, 250 }, { TAG_XTRA, 70 } };
    IDWriteFontFace5 *f5 = (IDWriteFontFace5 *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFontResource_CreateFontFace(r, SIM_NONE, av, 2, &f5), S_OK);
    CHECK(f5 != NULL && f5 != (IDWriteFontFace5 *)(ULONG_PTR)8);
    if (f5 && f5 != (IDWriteFontFace5 *)(ULONG_PTR)8) {
        EXPECT_EQ(IDWriteFontFace5_HasVariations(f5), TRUE);
        UINT32 c = IDWriteFontFace5_GetFontAxisValueCount(f5);
        CHECK(c >= 2);
        DWRITE_FONT_AXIS_VALUE got[MAXAX];
        if (c > 0 && c <= MAXAX && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(f5, got, c))) {
            int w = find_tag(got, c, TAG_W), xt = find_tag(got, c, TAG_XTRA);
            CHECK(w >= 0 && xt >= 0);
            if (w >= 0) EXPECT_EQ(got[w].value, 250.0f);
            if (xt >= 0) EXPECT_EQ(got[xt].value, 70.0f);
        }
    } else f5 = NULL;
    DW_RELEASE(f5);
    CLOSE_VAR(x, r, b);
}
TEST(var_create_font_face_without_values_uses_defaults) {
    OPEN_VAR(x, r, b);
    IDWriteFontFace5 *f5 = NULL;
    CHECK_HR(IDWriteFontResource_CreateFontFace(r, SIM_NONE, NULL, 0, &f5), S_OK);
    if (f5) {
        UINT32 c = IDWriteFontFace5_GetFontAxisValueCount(f5);
        t_note("face without explicit axis values reports %u axis values", c);
        DWRITE_FONT_AXIS_VALUE got[4];
        if (c > 0 && c <= 4 && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(f5, got, c))) {
            int w = find_tag(got, c, TAG_W);
            if (w >= 0) EXPECT_EQ(got[w].value, 400.0f);
        }
    }
    DW_RELEASE(f5);
    CLOSE_VAR(x, r, b);
}
TEST(var_create_font_face_reference_carries_values) {
    OPEN_VAR(x, r, b);
    DWRITE_FONT_AXIS_VALUE av[2] = { { TAG_W, 640 }, { TAG_XTRA, 20 } };
    IDWriteFontFaceReference1 *ref = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
    CHECK_HR(IDWriteFontResource_CreateFontFaceReference(r, SIM_BOLD, av, 2, &ref), S_OK);
    CHECK(ref != NULL && ref != (IDWriteFontFaceReference1 *)(ULONG_PTR)8);
    if (ref && ref != (IDWriteFontFaceReference1 *)(ULONG_PTR)8) {
        EXPECT_EQ(IDWriteFontFaceReference1_GetSimulations(ref), SIM_BOLD);
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontFaceIndex(ref), 0);
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(ref), 2);
        DWRITE_FONT_AXIS_VALUE got[2];
        if (SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(ref, got, 2))) {
            int w = find_tag(got, 2, TAG_W), xt = find_tag(got, 2, TAG_XTRA);
            CHECK(w >= 0 && xt >= 0);
            if (w >= 0) EXPECT_EQ(got[w].value, 640.0f);
            if (xt >= 0) EXPECT_EQ(got[xt].value, 20.0f);
        }
        /* The same reference built by the factory: identical key, so Equals is TRUE. */
        IDWriteFontFaceReference1 *viaf = NULL;
        if (SUCCEEDED(IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, SIM_BOLD, av, 2, &viaf)) && viaf)
            t_note("resource-made vs factory-made reference Equals: %d", (int)IDWriteFontFaceReference1_Equals(ref, (IDWriteFontFaceReference *)viaf));
        DW_RELEASE(viaf);
        IDWriteFontFace5 *f5 = NULL;
        if (SUCCEEDED(IDWriteFontFaceReference1_CreateFontFace(ref, &f5)) && f5) { EXPECT_EQ(IDWriteFontFace5_GetSimulations(f5), SIM_BOLD); EXPECT_EQ(IDWriteFontFace5_HasVariations(f5), TRUE); }
        DW_RELEASE(f5);
    } else ref = NULL;
    DW_RELEASE(ref);
    CLOSE_VAR(x, r, b);
}
TEST(var_face_via_get_font_resource_matches) {
    OPEN_VAR(x, r, b);
    DWRITE_FONT_AXIS_VALUE av = { TAG_W, 300 };
    IDWriteFontFace5 *f5 = NULL; IDWriteFontResource *back = NULL;
    if (SUCCEEDED(IDWriteFontResource_CreateFontFace(r, SIM_NONE, &av, 1, &f5)) && f5 && SUCCEEDED(IDWriteFontFace5_GetFontResource(f5, &back)) && back) {
        EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(back), IDWriteFontResource_GetFontAxisCount(r));
        EXPECT_EQ(IDWriteFontResource_HasVariations(back), TRUE);
        EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(back), 0);
        axes a1, a2;
        if (load_axes(r, &a1) && load_axes(back, &a2) && a1.n == a2.n) CHECK(!memcmp(a1.rg, a2.rg, a1.n * sizeof a1.rg[0]));
    }
    DW_RELEASE(back); DW_RELEASE(f5);
    CLOSE_VAR(x, r, b);
}
TEST(var_axis_flags_hidden_and_fixed) {
    size_t n; frr_axis ax[3] = { { "wght", 100, 400, 900, 0, 256, "Weight" }, { "HIDN", 0, 50, 100, 1, 257, "Hidden Axis" }, { "FIXD", 50, 50, 50, 0, 258, "Fixed Axis" } };
    uint8_t *b = frr_build_var(ax, 3, &n); frr_fix x;
    if (b && frr_open(&x, b, n) && x.f6) {
        IDWriteFontResource *r = mkres(&x, 0);
        if (r) {
            UINT32 na = IDWriteFontResource_GetFontAxisCount(r);
            t_note("three fvar axes (one hidden, one with min==max) -> GetFontAxisCount %u", na);
            DWRITE_FONT_AXIS_VALUE v[MAXAX]; DWRITE_FONT_AXIS_RANGE rg[MAXAX];
            if (na > 0 && na <= MAXAX && SUCCEEDED(IDWriteFontResource_GetDefaultFontAxisValues(r, v, na)) && SUCCEEDED(IDWriteFontResource_GetFontAxisRanges(r, rg, na))) {
                UINT32 i;
                for (i = 0; i < na; i++) {
                    DWRITE_FONT_AXIS_ATTRIBUTES a = IDWriteFontResource_GetFontAxisAttributes(r, i);
                    t_note("  axis %u tag %08x default %g range [%g, %g] attributes %d", i, v[i].axisTag, v[i].value, rg[i].minValue, rg[i].maxValue, (int)a);
                    if (v[i].axisTag == TAG_W) { CHECK(a & DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE); CHECK(!(a & DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN)); }
                    if (v[i].axisTag == TAG_HIDN) CHECK(a & DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN);
                    if (v[i].axisTag == TAG_FIXD) { EXPECT_EQ(rg[i].minValue, 50.0f); EXPECT_EQ(rg[i].maxValue, 50.0f); }
                }
            }
            DW_RELEASE(r);
        }
    } else if (!b) t_skip("frr_build_var failed");
    else if (x.f3 && !x.f6) t_skip("IDWriteFactory6 unsupported");
    frr_close(&x); free(b);
}
TEST(var_many_axes_buffer_exact) {
    enum { N = 12 };
    frr_axis ax[N]; char tags[N][5]; char names[N][16]; int i; size_t n;
    for (i = 0; i < N; i++) {
        snprintf(tags[i], 5, "A%02d%c", i, 'x'); snprintf(names[i], 16, "Axis %d", i);
        memcpy(ax[i].tag, tags[i], 5); ax[i].mn = (float)i; ax[i].df = (float)(i + 5); ax[i].mx = (float)(i + 20); ax[i].flags = 0; ax[i].nameid = 300 + i; ax[i].name = names[i];
    }
    uint8_t *b = frr_build_var(ax, N, &n); frr_fix x;
    if (b && frr_open(&x, b, n) && x.f6) {
        IDWriteFontResource *r = mkres(&x, 0);
        if (r) {
            axes a;
            UINT32 na = IDWriteFontResource_GetFontAxisCount(r);
            CHECK(na >= N);
            if (load_axes(r, &a)) {
                for (i = 0; i < N; i++) {
                    int k = find_tag(a.def, a.n, DW_TAG(tags[i][0], tags[i][1], tags[i][2], tags[i][3]));
                    t_check(k >= 0, __FILE__, __LINE__, "axis present", "%s", tags[i]);
                    if (k >= 0) { t_check(a.def[k].value == (float)(i + 5), __FILE__, __LINE__, "default", "%s: %g", tags[i], a.def[k].value); t_check(a.rg[k].minValue == (float)i && a.rg[k].maxValue == (float)(i + 20), __FILE__, __LINE__, "range", "%s", tags[i]); }
                }
            }
        }
        DW_RELEASE(r);
    } else if (!b) t_skip("frr_build_var failed");
    else if (x.f3 && !x.f6) t_skip("IDWriteFactory6 unsupported");
    frr_close(&x); free(b);
}

/* ---- CreateFontFace / CreateFontFaceReference validation ---------------------------------------------------------------------------------- */
TEST(resource_createfontface_validation) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    if (r) {
        DWRITE_FONT_AXIS_VALUE av[1] = { { TAG_W, 400 } };
        IDWriteFontFace5 *f = (IDWriteFontFace5 *)(ULONG_PTR)8;
        static const UINT32 bad[] = { 4, 8, 0x100, 0x80000000, 0xFFFFFFFF };
        size_t i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            f = (IDWriteFontFace5 *)(ULONG_PTR)8;
            HRESULT hr = IDWriteFontResource_CreateFontFace(r, (DWRITE_FONT_SIMULATIONS)bad[i], av, 1, &f);
            t_check(hr == FRR_EINVALIDARG, __FILE__, __LINE__, "bad sims", "0x%x -> %s", bad[i], dw_hr_name(hr));
            t_check(f == NULL, __FILE__, __LINE__, "out zeroed", "0x%x", bad[i]); f = NULL;
        }
        f = (IDWriteFontFace5 *)(ULONG_PTR)8;
        CHECK_HR(IDWriteFontResource_CreateFontFace(r, SIM_NONE, NULL, 3, &f), FRR_EINVALIDARG);          /* values missing */
        FRR_ZERO(f);
        f = (IDWriteFontFace5 *)(ULONG_PTR)8;
        CHECK_HR(IDWriteFontResource_CreateFontFace(r, SIM_NONE, av, 0x10000, &f), FRR_EINVALIDARG);      /* rejected before the array is read */
        FRR_ZERO(f);
        f = (IDWriteFontFace5 *)(ULONG_PTR)8;
        CHECK_HR(IDWriteFontResource_CreateFontFace(r, SIM_NONE, av, 0xFFFFFFFF, &f), FRR_EINVALIDARG);
        FRR_ZERO(f);
        CHECK_HR(IDWriteFontResource_CreateFontFace(r, SIM_NONE, NULL, 0, &f), S_OK);
        CHECK(f != NULL); DW_RELEASE(f);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(resource_createfontfacereference_validation) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    if (r) {
        DWRITE_FONT_AXIS_VALUE av[1] = { { TAG_W, 400 } };
        IDWriteFontFaceReference1 *f = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
        static const UINT32 bad[] = { 4, 8, 0x100, 0x80000000, 0xFFFFFFFF };
        size_t i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            f = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
            HRESULT hr = IDWriteFontResource_CreateFontFaceReference(r, (DWRITE_FONT_SIMULATIONS)bad[i], av, 1, &f);
            t_check(hr == FRR_EINVALIDARG, __FILE__, __LINE__, "bad sims", "0x%x -> %s", bad[i], dw_hr_name(hr));
            t_check(f == NULL, __FILE__, __LINE__, "out zeroed", "0x%x", bad[i]); f = NULL;
        }
        f = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
        CHECK_HR(IDWriteFontResource_CreateFontFaceReference(r, SIM_NONE, NULL, 1, &f), FRR_EINVALIDARG);
        FRR_ZERO(f);
        f = (IDWriteFontFaceReference1 *)(ULONG_PTR)8;
        CHECK_HR(IDWriteFontResource_CreateFontFaceReference(r, SIM_NONE, av, 0x10000, &f), FRR_EINVALIDARG);
        FRR_ZERO(f);
        CHECK_HR(IDWriteFontResource_CreateFontFaceReference(r, SIM_NONE, NULL, 0, &f), S_OK);
        CHECK(f != NULL);
        if (f) { EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(f), 0); EXPECT_EQ(IDWriteFontFaceReference1_GetFontFaceIndex(f), 0); }
        DW_RELEASE(f);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(resource_createfontface_simulations_matrix) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    UINT32 s;
    for (s = 0; r && s <= 3; s++) {
        IDWriteFontFace5 *f5 = NULL; IDWriteFontFaceReference1 *ref = NULL;
        CHECK_HR(IDWriteFontResource_CreateFontFace(r, (DWRITE_FONT_SIMULATIONS)s, NULL, 0, &f5), S_OK);
        CHECK_HR(IDWriteFontResource_CreateFontFaceReference(r, (DWRITE_FONT_SIMULATIONS)s, NULL, 0, &ref), S_OK);
        if (f5) t_check(IDWriteFontFace5_GetSimulations(f5) == (DWRITE_FONT_SIMULATIONS)s, __FILE__, __LINE__, "face sims", "%u", s);
        if (ref) t_check(IDWriteFontFaceReference1_GetSimulations(ref) == (DWRITE_FONT_SIMULATIONS)s, __FILE__, __LINE__, "ref sims", "%u", s);
        DW_RELEASE(f5); DW_RELEASE(ref);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(resource_createfontface_equivalent_to_reference_createfontface) {
    FRR_OPEN_NEED6(x);
    IDWriteFontResource *r = mkres(&x, 0);
    IDWriteFontFaceReference1 *ref = NULL; IDWriteFontFace5 *a = NULL, *b = NULL;
    if (r && SUCCEEDED(IDWriteFontResource_CreateFontFaceReference(r, SIM_BOLD, NULL, 0, &ref)) && ref &&
        SUCCEEDED(IDWriteFontResource_CreateFontFace(r, SIM_BOLD, NULL, 0, &a)) && SUCCEEDED(IDWriteFontFaceReference1_CreateFontFace(ref, &b)) && a && b) {
        EXPECT_EQ(IDWriteFontFace5_GetSimulations(a), IDWriteFontFace5_GetSimulations(b));
        EXPECT_EQ(IDWriteFontFace5_GetGlyphCount(a), IDWriteFontFace5_GetGlyphCount(b));
        t_note("resource face vs reference face: Equals %d", frr_face_equals((IUnknown *)a, (IUnknown *)b));
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(ref); DW_RELEASE(r);
    frr_close(&x);
}

/* ---- Bahnschrift (system variable font) ------------------------------------------------------------------------------------------------- */
static int fvar_axis(const uint8_t *fv, UINT32 len, UINT32 i, UINT32 *tag, float *mn, float *df, float *mx) {
    if (len < 16) return 0;
    unsigned off = (fv[4] << 8) | fv[5], na = (fv[8] << 8) | fv[9], sz = (fv[10] << 8) | fv[11];
    if (i >= na || off + (i + 1) * sz > len) return 0;
    const uint8_t *a = fv + off + i * sz;
    *tag = (UINT32)a[0] | (UINT32)a[1] << 8 | (UINT32)a[2] << 16 | (UINT32)a[3] << 24;
    *mn = (int32_t)((UINT32)a[4] << 24 | a[5] << 16 | a[6] << 8 | a[7]) / 65536.0f;
    *df = (int32_t)((UINT32)a[8] << 24 | a[9] << 16 | a[10] << 8 | a[11]) / 65536.0f;
    *mx = (int32_t)((UINT32)a[12] << 24 | a[13] << 16 | a[14] << 8 | a[15]) / 65536.0f;
    return 1;
}
static IDWriteFontResource *bahn(IDWriteFontFace **faceOut) {
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = dw_sysface(f, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace5 *f5 = NULL; IDWriteFontResource *r = NULL;
    *faceOut = NULL;
    if (!face) { t_skip("Bahnschrift not installed"); return NULL; }
    if (FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace5, (void **)&f5)) || !f5 || FAILED(IDWriteFontFace5_GetFontResource(f5, &r)) || !r) { DW_RELEASE(f5); DW_RELEASE(face); t_skip("no IDWriteFontFace5"); return NULL; }
    DW_RELEASE(f5);
    *faceOut = face;
    return r;
}
TEST(bahnschrift_axes_match_its_fvar_table) {
    IDWriteFontFace *face; IDWriteFontResource *r = bahn(&face); if (!r) return;
    UINT32 fl = 0; uint8_t *fv = dw_face_table(face, DW_TAG('f', 'v', 'a', 'r'), &fl);
    if (fv && fl >= 16) {
        UINT32 nf = (fv[8] << 8) | fv[9];
        EXPECT_EQ(IDWriteFontResource_HasVariations(r), TRUE);
        axes a;
        CHECK(IDWriteFontResource_GetFontAxisCount(r) >= nf);
        if (load_axes(r, &a)) {
            UINT32 i;
            for (i = 0; i < nf; i++) {
                UINT32 tag; float mn, df, mx;
                if (!fvar_axis(fv, fl, i, &tag, &mn, &df, &mx)) continue;
                int k = find_tag(a.def, a.n, tag);
                t_check(k >= 0, __FILE__, __LINE__, "fvar axis exposed", "tag %08x", tag);
                if (k < 0) continue;
                t_note("Bahnschrift axis %c%c%c%c: default %g range [%g, %g] attrs %d", tag & 255, (tag >> 8) & 255, (tag >> 16) & 255, tag >> 24, a.def[k].value, a.rg[k].minValue, a.rg[k].maxValue, (int)IDWriteFontResource_GetFontAxisAttributes(r, k));
                t_check(a.def[k].value == df, __FILE__, __LINE__, "default", "%g vs fvar %g", a.def[k].value, df);
                t_check(a.rg[k].axisTag == tag && a.rg[k].minValue == mn && a.rg[k].maxValue == mx, __FILE__, __LINE__, "range", "[%g,%g] vs fvar [%g,%g]", a.rg[k].minValue, a.rg[k].maxValue, mn, mx);
                t_check((IDWriteFontResource_GetFontAxisAttributes(r, k) & DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE) != 0, __FILE__, __LINE__, "VARIABLE", "tag %08x", tag);
            }
        }
    } else t_skip("no fvar table");
    free(fv); DW_RELEASE(r); DW_RELEASE(face);
}
TEST(bahnschrift_axis_names_and_value_names) {
    IDWriteFontFace *face; IDWriteFontResource *r = bahn(&face); if (!r) return;
    UINT32 na = IDWriteFontResource_GetFontAxisCount(r), i;
    for (i = 0; i < na && i < 8; i++) {
        IDWriteLocalizedStrings *s = NULL; char en[128] = "";
        CHECK_HR(IDWriteFontResource_GetAxisNames(r, i, &s), S_OK);
        CHECK(s != NULL);
        if (s && IDWriteLocalizedStrings_GetCount(s) > 0 && SUCCEEDED(dw_ls_en(s, en, sizeof en))) t_note("axis %u name \"%s\"", i, en);
        DW_RELEASE(s);
        UINT32 c = IDWriteFontResource_GetAxisValueNameCount(r, i), j;
        t_note("axis %u has %u named value(s)", i, c);
        for (j = 0; j < c && j < 32; j++) {
            DWRITE_FONT_AXIS_RANGE rg; IDWriteLocalizedStrings *vs = NULL; char nm[128] = "";
            memset(&rg, 0xCC, sizeof rg);
            CHECK_HR(IDWriteFontResource_GetAxisValueNames(r, i, j, &rg, &vs), S_OK);
            CHECK(vs != NULL);
            if (vs && IDWriteLocalizedStrings_GetCount(vs) > 0 && SUCCEEDED(dw_ls_en(vs, nm, sizeof nm))) t_note("  value %u \"%s\" tag %08x [%g, %g]", j, nm, rg.axisTag, rg.minValue, rg.maxValue);
            DW_RELEASE(vs);
        }
    }
    DW_RELEASE(r); DW_RELEASE(face);
}
TEST(bahnschrift_create_face_from_resource_matches_axis_values) {
    IDWriteFontFace *face; IDWriteFontResource *r = bahn(&face); if (!r) return;
    UINT32 na = IDWriteFontResource_GetFontAxisCount(r);
    DWRITE_FONT_AXIS_VALUE v[MAXAX];
    if (na > 0 && na <= MAXAX && SUCCEEDED(IDWriteFontResource_GetDefaultFontAxisValues(r, v, na))) {
        IDWriteFontFace5 *f5 = NULL;
        CHECK_HR(IDWriteFontResource_CreateFontFace(r, SIM_NONE, v, na, &f5), S_OK);
        if (f5) {
            EXPECT_EQ(IDWriteFontFace5_HasVariations(f5), TRUE);
            t_note("face created with all %u default axis values reports %u axis values", na, IDWriteFontFace5_GetFontAxisValueCount(f5));
        }
        DW_RELEASE(f5);
    }
    DW_RELEASE(r); DW_RELEASE(face);
}
