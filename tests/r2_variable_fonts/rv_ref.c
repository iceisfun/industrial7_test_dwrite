/* IDWriteFontFaceReference1 with axis values: IDWriteFontResource::CreateFontFaceReference, IDWriteFactory6::CreateFontFaceReference (file, index,
 * simulations, axis values), IDWriteFontFaceReference1::{GetFontAxisValueCount, GetFontAxisValues, CreateFontFace, Equals}.
 *
 * Win10 dwrite.dll (IDWriteFontResource::CreateFontFaceReference, IDWriteFactory::CreateFontFaceReference(file, ..., axis values) forwarding to
 *, IDWriteFontFaceReference::GetAxisValueCount / GetFontAxisValues / Equals):
 *   - both creators zero the output first; they return E_INVALIDARG for simulations & ~3, a count > 0xFFFF, or NULL values with a non-zero count
 *     (the file overload also for a NULL file);
 *   - a reference stores the values AS SUPPLIED (converted to 16.16): no clamping, no canonicalisation, duplicates and unknown tags kept, order kept;
 *     GetFontAxisValueCount returns the supplied count;
 *   - GetFontAxisValues zeroes the buffer over the passed count and fails with E_NOT_SUFFICIENT_BUFFER (0x8007007A) when it is smaller than the count;
 *   - Equals is FALSE for NULL and otherwise compares the raw keys.
 */
#include "rv.h"

static IDWriteFontFaceReference1 *mkref(rv *x, DWRITE_FONT_SIMULATIONS sim, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n)
{
    IDWriteFontFaceReference1 *r = NULL;
    HRESULT hr = IDWriteFontResource_CreateFontFaceReference(x->res, sim, v, n, &r);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? r : NULL;
}
static IDWriteFontFaceReference1 *mkref_f(rv *x, DWRITE_FONT_SIMULATIONS sim, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n)
{
    IDWriteFontFaceReference1 *r = NULL;
    HRESULT hr = IDWriteFactory6_CreateFontFaceReference(x->f6, x->file, 0, sim, v, n, &r);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? r : NULL;
}
static int req(IDWriteFontFaceReference1 *a, IDWriteFontFaceReference1 *b) { return IDWriteFontFaceReference1_Equals(a, (IDWriteFontFaceReference *)b) ? 1 : 0; }

/* ---- stored values ------------------------------------------------------------------------------------------------------------------ */
TEST(r2_variable_fonts_ref_no_values)
{
    IDWriteFontFaceReference1 *r;
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0);
    if (r) { EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 0); CHECK_HR_OK(IDWriteFontFaceReference1_GetFontAxisValues(r, NULL, 0)); DW_RELEASE(r); }
    RV_END();
}
TEST(r2_variable_fonts_ref_count_is_supplied_count)
{
    IDWriteFontFaceReference1 *r; DWRITE_FONT_AXIS_VALUE a[3];
    RV_BEGIN(0);
    a[0] = rv_av(TAG_WGHT, 700); a[1] = rv_av(TAG_WDTH, 90); a[2] = rv_av(TAG_ZZZZ, 4);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, a, 3);
    if (r) { EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 3); DW_RELEASE(r); }       /* not the font's axis count */
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, a, 1);
    if (r) { EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 1); DW_RELEASE(r); }
    RV_END();
}
TEST(r2_variable_fonts_ref_values_kept_verbatim)
{
    IDWriteFontFaceReference1 *r; DWRITE_FONT_AXIS_VALUE a[4], g[4];
    RV_BEGIN(0);
    a[0] = rv_av(TAG_WGHT, 5000); a[1] = rv_av(TAG_WGHT, -5); a[2] = rv_av(TAG_ZZZZ, 1.5f); a[3] = rv_av(TAG_OPSZ, 0.25f);     /* out of range, duplicate, unknown */
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, a, 4);
    if (r) {
        memset(g, 0xCC, sizeof g);
        CHECK_HR_OK(IDWriteFontFaceReference1_GetFontAxisValues(r, g, 4));
        CHECK(!memcmp(a, g, sizeof a));
        DW_RELEASE(r);
    }
    RV_END();
}
TEST(r2_variable_fonts_ref_values_fixed_point_rounding)
{
    IDWriteFontFaceReference1 *r; DWRITE_FONT_AXIS_VALUE a[3], g[3];
    RV_BEGIN(0);
    a[0] = rv_av(TAG_WGHT, 400.000001f); a[1] = rv_av(TAG_WDTH, 90.5f); a[2] = rv_av(TAG_OPSZ, 12.3f);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, a, 3);
    if (r) {
        CHECK_HR_OK(IDWriteFontFaceReference1_GetFontAxisValues(r, g, 3));
        CHECK(g[0].value == 400.0f); CHECK(g[1].value == 90.5f);
        { long long q = (long long)(12.3f * 65536.0f * 65536.0f) + 0x8000; CHECKF(g[2].value == (float)(q >> 16) / 65536.0f, "opsz %.9g", g[2].value); }
        DW_RELEASE(r);
    }
    RV_END();
}
TEST(r2_variable_fonts_ref_values_buffer_protocol)
{
    IDWriteFontFaceReference1 *r; DWRITE_FONT_AXIS_VALUE a[3], g[6]; HRESULT hr; UINT32 i; int j;
    RV_BEGIN(0);
    a[0] = rv_av(TAG_WGHT, 700); a[1] = rv_av(TAG_WDTH, 90); a[2] = rv_av(TAG_OPSZ, 20);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, a, 3);
    if (r) {
        for (i = 0; i < 3; i++) {
            memset(g, 0xCC, sizeof g);
            hr = IDWriteFontFaceReference1_GetFontAxisValues(r, g, i);
            CHECKF(hr == DW_E_NOT_SUFFICIENT_BUFFER, "buffer %u -> %s", i, dw_hr_name(hr));
            { unsigned char *p = (unsigned char *)g; size_t k, bad = 0; for (k = 0; k < i * sizeof g[0]; k++) if (p[k]) bad++; CHECKF(bad == 0, "buffer %u: passed entries not zeroed", i); }
        }
        memset(g, 0xCC, sizeof g);
        CHECK_HR_OK(IDWriteFontFaceReference1_GetFontAxisValues(r, g, 6));
        for (j = 3; j < 6; j++) CHECKF(g[j].axisTag == 0 && g[j].value == 0.0f, "tail entry %d not zero", j);
        DW_RELEASE(r);
    }
    RV_END();
}
TEST(r2_variable_fonts_ref_zero_count_null_buffer_with_values)
{
    IDWriteFontFaceReference1 *r; DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 700); HRESULT hr;
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &a, 1);
    if (r) { hr = IDWriteFontFaceReference1_GetFontAxisValues(r, NULL, 0); CHECKF(hr == DW_E_NOT_SUFFICIENT_BUFFER, "NULL, 0 -> %s", dw_hr_name(hr)); DW_RELEASE(r); }
    RV_END();
}
TEST(r2_variable_fonts_ref_many_values)
{
    /* 30 values, past the converter's inline capacity of 20 */
    IDWriteFontFaceReference1 *r; DWRITE_FONT_AXIS_VALUE a[30], g[30]; int i;
    RV_BEGIN(0);
    for (i = 0; i < 30; i++) a[i] = rv_av(RV_TAG('r', 'f', '0' + i / 10, '0' + i % 10), (float)i * 2.5f);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, a, 30);
    if (r) { EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 30); CHECK_HR_OK(IDWriteFontFaceReference1_GetFontAxisValues(r, g, 30)); CHECK(!memcmp(a, g, sizeof a)); DW_RELEASE(r); }
    RV_END();
}

/* ---- the factory overload ------------------------------------------------------------------------------------------------------------------ */
TEST(r2_variable_fonts_ref_factory_overload_matches_resource_overload)
{
    IDWriteFontFaceReference1 *a, *b; DWRITE_FONT_AXIS_VALUE v[2], g1[2], g2[2];
    RV_BEGIN(0);
    v[0] = rv_av(TAG_WGHT, 620); v[1] = rv_av(TAG_WDTH, 80);
    a = mkref(&x, DWRITE_FONT_SIMULATIONS_BOLD, v, 2); b = mkref_f(&x, DWRITE_FONT_SIMULATIONS_BOLD, v, 2);
    if (a && b) {
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(b), 2);
        CHECK_HR_OK(IDWriteFontFaceReference1_GetFontAxisValues(a, g1, 2)); CHECK_HR_OK(IDWriteFontFaceReference1_GetFontAxisValues(b, g2, 2)); CHECK(!memcmp(g1, g2, sizeof g1));
        CHECK(IDWriteFontFaceReference1_GetSimulations(b) == DWRITE_FONT_SIMULATIONS_BOLD);
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontFaceIndex(b), 0);
        CHECK(req(a, b) == 1);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_ref_factory_null_file)
{
    IDWriteFontFaceReference1 *r = RV_SENT(IDWriteFontFaceReference1 *); HRESULT hr;
    RV_BEGIN(0);
    if (t_is_wine()) { RV_END(); t_skip("emulator faults on a NULL font file"); return; }
    hr = IDWriteFactory6_CreateFontFaceReference(x.f6, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, &r);
    CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(r);
    RV_END();
}
TEST(r2_variable_fonts_ref_factory_invalid_arguments)
{
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 500), *big = (DWRITE_FONT_AXIS_VALUE *)calloc(0x10000, sizeof a);   /* the count is read before any validation: back it with real memory */
    UINT32 k;
    RV_BEGIN(0);
    if (t_is_wine()) { RV_END(); free(big); t_skip("emulator does not validate the arguments"); return; }
    if (!big) { RV_END(); t_skip("allocation failed"); return; }
    for (k = 0; k < 0x10000; k++) big[k] = a;
    { IDWriteFontFaceReference1 *r = RV_SENT(IDWriteFontFaceReference1 *); HRESULT hr = IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, (DWRITE_FONT_SIMULATIONS)4, &a, 1, &r); CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(r); }
    /* NULL values with a non-zero count is not exercised: the factory overload dereferences the pointer (access violation) */
    { IDWriteFontFaceReference1 *r = RV_SENT(IDWriteFontFaceReference1 *); HRESULT hr = IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, big, 0x10000, &r); CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(r); }
    free(big);
    RV_END();
}
TEST(r2_variable_fonts_ref_resource_invalid_arguments)
{
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 500);
    RV_BEGIN(0);
    if (t_is_wine()) { RV_END(); t_skip("emulator does not validate the arguments"); return; }
    { IDWriteFontFaceReference1 *r = RV_SENT(IDWriteFontFaceReference1 *); HRESULT hr = IDWriteFontResource_CreateFontFaceReference(x.res, (DWRITE_FONT_SIMULATIONS)8, &a, 1, &r); CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(r); }
    { IDWriteFontFaceReference1 *r = RV_SENT(IDWriteFontFaceReference1 *); HRESULT hr = IDWriteFontResource_CreateFontFaceReference(x.res, DWRITE_FONT_SIMULATIONS_NONE, NULL, 1, &r); CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(r); }
    { IDWriteFontFaceReference1 *r = RV_SENT(IDWriteFontFaceReference1 *); HRESULT hr = IDWriteFontResource_CreateFontFaceReference(x.res, DWRITE_FONT_SIMULATIONS_NONE, &a, 0xFFFF + 1, &r); CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(r); }
    RV_END();
}
TEST(r2_variable_fonts_ref_zero_count_ignores_pointer)
{
    IDWriteFontFaceReference1 *r; DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 500);
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &a, 0);
    if (r) { EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 0); DW_RELEASE(r); }
    r = mkref_f(&x, DWRITE_FONT_SIMULATIONS_NONE, &a, 0);
    if (r) { EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 0); DW_RELEASE(r); }
    RV_END();
}
TEST(r2_variable_fonts_ref_simulations_all_valid)
{
    unsigned s;
    RV_BEGIN(0);
    for (s = 0; s < 4; s++) {
        IDWriteFontFaceReference1 *r = mkref(&x, (DWRITE_FONT_SIMULATIONS)s, NULL, 0);
        if (r) { CHECKF(IDWriteFontFaceReference1_GetSimulations(r) == (DWRITE_FONT_SIMULATIONS)s, "simulations %u", s); DW_RELEASE(r); }
    }
    RV_END();
}
TEST(r2_variable_fonts_ref_non_axis_factory_overload_has_no_values)
{
    IDWriteFactory3 *f3 = NULL; IDWriteFontFaceReference *r = NULL; IDWriteFontFaceReference1 *r1 = NULL;
    RV_BEGIN(0);
    if (SUCCEEDED(IDWriteFactory6_QueryInterface(x.f6, &IID_IDWriteFactory3, (void **)&f3)) && f3) {
        CHECK_HR_OK(IDWriteFactory3_CreateFontFaceReference_(f3, x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &r));
        if (r && SUCCEEDED(IDWriteFontFaceReference_QueryInterface(r, &IID_IDWriteFontFaceReference1, (void **)&r1)) && r1) EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r1), 0);
        else CHECK(0);
    } else CHECK(0);
    DW_RELEASE(r1); DW_RELEASE(r); DW_RELEASE(f3);
    RV_END();
}

/* ---- CreateFontFace ------------------------------------------------------------------------------------------------------------------------ */
TEST(r2_variable_fonts_ref_create_face_canonicalises)
{
    IDWriteFontFaceReference1 *r; IDWriteFontFace5 *f = NULL; DWRITE_FONT_AXIS_VALUE a[2], v[16]; UINT32 n; int k;
    RV_BEGIN(0);
    a[0] = rv_av(TAG_WGHT, 5000); a[1] = rv_av(TAG_WGHT, 300);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, a, 2);
    if (r) {
        CHECK_HR_OK(IDWriteFontFaceReference1_CreateFontFace(r, &f));
        if (f) {
            n = IDWriteFontFace5_GetFontAxisValueCount(f); EXPECT_EQ(n, RV_NALL);
            if (n == RV_NALL) { CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, v, n)); k = rv_find_value(v, n, TAG_WGHT); CHECK(k >= 0 && v[k].value == 900.0f); }
            DW_RELEASE(f);
        }
        DW_RELEASE(r);
    }
    RV_END();
}
TEST(r2_variable_fonts_ref_create_face_equals_resource_face)
{
    IDWriteFontFaceReference1 *r; IDWriteFontFace5 *f1 = NULL, *f2; DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 650);
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &a, 1);
    f2 = rv_face(&x, &a, 1);
    if (r && f2) { CHECK_HR_OK(IDWriteFontFaceReference1_CreateFontFace(r, &f1)); if (f1) CHECK(IDWriteFontFace5_Equals(f1, (IDWriteFontFace *)f2) == TRUE); }
    DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(r);
    RV_END();
}
TEST(r2_variable_fonts_ref_create_face_keeps_simulations)
{
    IDWriteFontFaceReference1 *r; IDWriteFontFace5 *f = NULL;
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_BOLD, NULL, 0);
    if (r) { CHECK_HR_OK(IDWriteFontFaceReference1_CreateFontFace(r, &f)); if (f) CHECK(IDWriteFontFace5_GetSimulations(f) == DWRITE_FONT_SIMULATIONS_BOLD); }
    DW_RELEASE(f); DW_RELEASE(r);
    RV_END();
}
TEST(r2_variable_fonts_ref_create_face_via_base_interface)
{
    IDWriteFontFaceReference1 *r; IDWriteFontFace3 *f = NULL; IDWriteFontFace *f0 = NULL; DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 500);
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &a, 1);
    if (r) {
        CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFace((IDWriteFontFaceReference *)r, &f));     /* IDWriteFontFaceReference::CreateFontFace -> IDWriteFontFace3 */
        if (f) { IDWriteFontFace5 *f5 = NULL; if (SUCCEEDED(IDWriteFontFace3_QueryInterface(f, &IID_IDWriteFontFace5, (void **)&f5)) && f5) { CHECK(IDWriteFontFace5_HasVariations(f5) == TRUE); DW_RELEASE(f5); } else CHECK(0); }
    }
    (void)f0;
    DW_RELEASE(f); DW_RELEASE(r);
    RV_END();
}
TEST(r2_variable_fonts_ref_create_face_independent_of_reference)
{
    IDWriteFontFaceReference1 *r; IDWriteFontFace5 *f = NULL; DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WDTH, 85), v[16];
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &a, 1);
    if (r) CHECK_HR_OK(IDWriteFontFaceReference1_CreateFontFace(r, &f));
    DW_RELEASE(r); DW_RELEASE(x.res); DW_RELEASE(x.file);
    if (f) { UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(f); if (n && n <= 16) { CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, v, n)); CHECK(v[rv_find_value(v, n, TAG_WDTH) < 0 ? 0 : rv_find_value(v, n, TAG_WDTH)].value == 85.0f); } DW_RELEASE(f); }
    RV_END();
}

/* ---- Equals --------------------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_ref_equals_null_self)
{
    IDWriteFontFaceReference1 *r;
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0);
    if (r && t_is_wine()) { DW_RELEASE(r); RV_END(); t_skip("emulator faults on a NULL comparison"); return; }
    if (r) { CHECK(IDWriteFontFaceReference1_Equals(r, NULL) == FALSE); CHECK(req(r, r) == 1); DW_RELEASE(r); }
    RV_END();
}
TEST(r2_variable_fonts_ref_equals_same_and_different_values)
{
    IDWriteFontFaceReference1 *a, *b, *c; DWRITE_FONT_AXIS_VALUE v1 = rv_av(TAG_WGHT, 600), v2 = rv_av(TAG_WGHT, 601);
    RV_BEGIN(0);
    a = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &v1, 1); b = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &v1, 1); c = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &v2, 1);
    if (a && b && c) { CHECK(req(a, b) == 1 && req(b, a) == 1); CHECK(req(a, c) == 0 && req(c, a) == 0); }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    RV_END();
}
TEST(r2_variable_fonts_ref_equals_simulations)
{
    IDWriteFontFaceReference1 *a, *b;
    RV_BEGIN(0);
    a = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0); b = mkref(&x, DWRITE_FONT_SIMULATIONS_OBLIQUE, NULL, 0);
    if (a && b) CHECK(req(a, b) == 0);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_ref_equals_across_creators)
{
    IDWriteFontFaceReference1 *a, *b; DWRITE_FONT_AXIS_VALUE v = rv_av(TAG_OPSZ, 30);
    RV_BEGIN(0);
    a = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &v, 1); b = mkref_f(&x, DWRITE_FONT_SIMULATIONS_NONE, &v, 1);
    if (a && b) CHECK(req(a, b) == 1);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_ref_equals_raw_vs_canonical_note)
{
    IDWriteFontFaceReference1 *a, *b; DWRITE_FONT_AXIS_VALUE v = rv_av(TAG_WGHT, 400);
    RV_BEGIN(0);
    a = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, &v, 1); b = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0);
    if (a && b) t_note("reference with explicit default wght equals reference without values: %d", req(a, b));
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_ref_equals_order_note)
{
    IDWriteFontFaceReference1 *a, *b; DWRITE_FONT_AXIS_VALUE v[2], w[2];
    RV_BEGIN(0);
    v[0] = rv_av(TAG_WGHT, 600); v[1] = rv_av(TAG_WDTH, 90); w[0] = v[1]; w[1] = v[0];
    a = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, v, 2); b = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, w, 2);
    if (a && b) t_note("reference equality depends on value order: equal = %d", req(a, b));
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}

/* ---- misc ----------------------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_ref_get_font_file)
{
    IDWriteFontFaceReference1 *r; IDWriteFontFile *f = NULL; const void *k1 = NULL, *k2 = NULL; UINT32 s1 = 0, s2 = 0;
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0);
    if (r) {
        CHECK_HR_OK(IDWriteFontFaceReference1_GetFontFile(r, &f));
        if (f) { CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(f, &k1, &s1)); CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(x.file, &k2, &s2)); EXPECT_EQ(s1, s2); if (s1 == s2 && s1) CHECK(!memcmp(k1, k2, s1)); }
        DW_RELEASE(f); DW_RELEASE(r);
    }
    RV_END();
}
TEST(r2_variable_fonts_ref_qi_chain)
{
    IDWriteFontFaceReference1 *r; IDWriteFontFaceReference *r0 = NULL;
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0);
    if (r) {
        CHECK_HR_OK(IDWriteFontFaceReference1_QueryInterface(r, &IID_IDWriteFontFaceReference, (void **)&r0));
        if (r0) CHECK(dw_same_identity((IUnknown *)r0, (IUnknown *)r));
        DW_RELEASE(r0); DW_RELEASE(r);
    }
    RV_END();
}
TEST(r2_variable_fonts_ref_refcounts)
{
    IDWriteFontFaceReference1 *r; ULONG r0;
    RV_BEGIN(0);
    r = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0);
    if (r) { r0 = dw_refcount((IUnknown *)r); IDWriteFontFaceReference1_AddRef(r); EXPECT_REFS(r, r0 + 1); IDWriteFontFaceReference1_Release(r); EXPECT_REFS(r, r0); DW_RELEASE(r); }
    RV_END();
}
TEST(r2_variable_fonts_ref_is_new_object_each_time)
{
    IDWriteFontFaceReference1 *a, *b;
    RV_BEGIN(0);
    a = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0); b = mkref(&x, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0);
    if (a && b) CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
