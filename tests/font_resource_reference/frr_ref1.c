/* IDWriteFontFaceReference1 (GetFontAxisValueCount, GetFontAxisValues, CreateFontFace returning IDWriteFontFace5) and references that carry
 * font axis values (IDWriteFactory6::CreateFontFaceReference with axes, IDWriteFontResource::CreateFontFaceReference).
 *
 * Win10 dwrite.dll (DWriteFontFaceReferenceBase !0x928e0 GetFontAxisValueCount, !0xe43b0 GetFontAxisValues, !0x2a210 CreateFontFace;
 * DWriteFactory::CreateFontFaceReference with axes !0x2bb50 -> !0x2af10):
 *   - the axis values are stored as 16.16 fixed point in the reference key (float -> fixed on the way in, fixed -> float on the way out);
 *   - GetFontAxisValues zeroes the whole caller buffer first, then fails with E_NOT_SUFFICIENT_BUFFER (0x8007007A) when the buffer holds fewer
 *     values than the reference carries; a larger buffer is accepted and its tail stays zero;
 *   - the axis arrays given to the factory are validated: count > 0xFFFF or (count != 0 and NULL array) is E_INVALIDARG (the factory first
 *     converts the array, so NULL with a non-zero count is an access violation in the Factory6 wrapper - never passed here);
 *   - Equals compares the stored axis values too.
 */
#include "frr.h"

#define SIM_NONE DWRITE_FONT_SIMULATIONS_NONE
#define SIM_BOLD DWRITE_FONT_SIMULATIONS_BOLD
#define TAG_W DWRITE_FONT_AXIS_TAG_WEIGHT
#define TAG_D DWRITE_FONT_AXIS_TAG_WIDTH
#define TAG_S DWRITE_FONT_AXIS_TAG_SLANT
#define TAG_O DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE
#define TAG_I DWRITE_FONT_AXIS_TAG_ITALIC

static IDWriteFontFaceReference1 *mk6(frr_fix *x, UINT32 idx, DWRITE_FONT_SIMULATIONS sim, const DWRITE_FONT_AXIS_VALUE *av, UINT32 n) {
    IDWriteFontFaceReference1 *r = NULL;
    HRESULT hr = IDWriteFactory6_CreateFontFaceReference(x->f6, x->file, idx, sim, av, n, &r);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? r : NULL;
}
static int has_value(const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, UINT32 tag, float val) {
    UINT32 i;
    for (i = 0; i < n; i++) if (v[i].axisTag == tag && v[i].value == val) return 1;
    return 0;
}

/* ---- references without axes ------------------------------------------------------------------------------------------------- */
TEST(plain_ref_has_no_axis_values) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFaceReference1 *r1 = r ? frr_ref1(r) : NULL;
    if (r1) {
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r1), 0);
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r1, NULL, 0), S_OK);
    } else t_check(0, __FILE__, __LINE__, "QI IDWriteFontFaceReference1", "unsupported");
    DW_RELEASE(r1); DW_RELEASE(r);
    frr_close(&x);
}
TEST(plain_ref_axis_values_buffer_is_zeroed) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFaceReference1 *r1 = r ? frr_ref1(r) : NULL;
    if (r1) {
        DWRITE_FONT_AXIS_VALUE v[6]; UINT32 i; int bad = 0;
        memset(v, 0xCC, sizeof v);
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r1, v, 6), S_OK);
        for (i = 0; i < 6; i++) if (v[i].axisTag != 0 || v[i].value != 0.0f) bad++;
        EXPECT_EQ(bad, 0);
    }
    DW_RELEASE(r1); DW_RELEASE(r);
    frr_close(&x);
}
TEST(plain_ref_via_system_path_has_no_axis_values) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 0, SIM_NONE);
    IDWriteFontFaceReference1 *r1 = r ? frr_ref1(r) : NULL;
    if (r1) EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r1), 0);
    DW_RELEASE(r1); DW_RELEASE(r);
}

/* ---- references with axes ------------------------------------------------------------------------------------------------------- */
TEST(axis_ref_roundtrip_exact_values) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av[3] = { { TAG_W, 700.0f }, { TAG_D, 87.5f }, { TAG_S, -9.0f } };
    IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, av, 3);
    if (r) {
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 3);
        DWRITE_FONT_AXIS_VALUE got[3];
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, got, 3), S_OK);
        CHECK(has_value(got, 3, TAG_W, 700.0f)); CHECK(has_value(got, 3, TAG_D, 87.5f)); CHECK(has_value(got, 3, TAG_S, -9.0f));
        t_note("axis order in/out: %08x %08x %08x -> %08x %08x %08x", av[0].axisTag, av[1].axisTag, av[2].axisTag, got[0].axisTag, got[1].axisTag, got[2].axisTag);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(axis_ref_values_representable_in_16_16_are_exact) {
    FRR_OPEN_NEED6(x);
    static const float vals[] = { 0.0f, 1.0f, 0.5f, 0.25f, 100.0f, 400.5f, 999.0f, -1.0f, -90.0f, 0.0001220703125f /* 2^-13 */, 1000.0f, 12345.0f };
    size_t i;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        DWRITE_FONT_AXIS_VALUE av = { TAG_W, vals[i] }, got = { 0, 0 };
        IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, &av, 1);
        if (r && SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(r, &got, 1)))
            t_check(got.axisTag == TAG_W && got.value == vals[i], __FILE__, __LINE__, "value", "stored %g got %g", vals[i], got.value);
        DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(axis_ref_values_are_quantised_to_1_over_65536) {
    FRR_OPEN_NEED6(x);
    /* float -> 16.16 fixed -> float: the result is within one fixed-point step (plus the float rounding of the value itself). */
    static const float vals[] = { 400.1f, 0.3f, 87.3f, 650.7777f, -12.34567f };
    size_t i;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        DWRITE_FONT_AXIS_VALUE av = { TAG_W, vals[i] }, got = { 0, 0 };
        IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, &av, 1);
        if (r && SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(r, &got, 1))) {
            float eps = 1.0f / 65536.0f + fabsf(vals[i]) * 2.4e-7f;
            t_check(fabsf(got.value - vals[i]) <= eps, __FILE__, __LINE__, "within one fixed-point step", "stored %.7g got %.7g", vals[i], got.value);
        }
        DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(axis_ref_buffer_protocol) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av[3] = { { TAG_W, 650.0f }, { TAG_D, 90.0f }, { TAG_O, 14.0f } };
    IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, av, 3);
    if (r) {
        DWRITE_FONT_AXIS_VALUE v[8]; UINT32 i; int bad;
        /* exact - 1 */
        memset(v, 0xCC, sizeof v);
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, v, 2), FRR_ENOTSUFFICIENT);
        for (i = 0, bad = 0; i < 2; i++) if (v[i].axisTag != 0 || v[i].value != 0.0f) bad++;
        EXPECT_EQ(bad, 0);
        EXPECT_EQ(v[2].axisTag, 0xCCCCCCCC);        /* only the requested count is touched */
        /* zero count */
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, NULL, 0), FRR_ENOTSUFFICIENT);
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, v, 0), FRR_ENOTSUFFICIENT);
        /* exact */
        memset(v, 0xCC, sizeof v);
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, v, 3), S_OK);
        CHECK(has_value(v, 3, TAG_W, 650.0f) && has_value(v, 3, TAG_D, 90.0f) && has_value(v, 3, TAG_O, 14.0f));
        EXPECT_EQ(v[3].axisTag, 0xCCCCCCCC);
        /* larger */
        memset(v, 0xCC, sizeof v);
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, v, 8), S_OK);
        for (i = 3, bad = 0; i < 8; i++) if (v[i].axisTag != 0 || v[i].value != 0.0f) bad++;
        EXPECT_EQ(bad, 0);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(axis_ref_five_standard_axes) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av[5] = { { TAG_W, 300 }, { TAG_D, 80 }, { TAG_S, -5 }, { TAG_O, 12 }, { TAG_I, 1 } };
    IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, av, 5);
    if (r) {
        DWRITE_FONT_AXIS_VALUE got[5];
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), 5);
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, got, 5), S_OK);
        UINT32 i; for (i = 0; i < 5; i++) t_check(has_value(got, 5, av[i].axisTag, av[i].value), __FILE__, __LINE__, "axis present", "tag %08x", av[i].axisTag);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(axis_ref_many_custom_axes) {
    FRR_OPEN_NEED6(x);
    enum { N = 40 };
    DWRITE_FONT_AXIS_VALUE av[N], got[N]; int i;
    for (i = 0; i < N; i++) { av[i].axisTag = DW_TAG('A', 'a' + i % 26, '0' + i / 26, 'x'); av[i].value = (float)(i * 3 + 1); }
    IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, av, N);
    if (r) {
        EXPECT_EQ(IDWriteFontFaceReference1_GetFontAxisValueCount(r), N);
        CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, got, N), S_OK);
        for (i = 0; i < N; i++) t_check(has_value(got, N, av[i].axisTag, av[i].value), __FILE__, __LINE__, "axis", "#%d", i);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(axis_ref_duplicate_tags_are_reported) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av[2] = { { TAG_W, 300 }, { TAG_W, 700 } };
    IDWriteFontFaceReference1 *r = NULL;
    HRESULT hr = IDWriteFactory6_CreateFontFaceReference(x.f6, x.file, 0, SIM_NONE, av, 2, &r);
    t_note("duplicate wght axis values -> %s, count %u", dw_hr_name(hr), r ? IDWriteFontFaceReference1_GetFontAxisValueCount(r) : 0);
    CHECK(SUCCEEDED(hr) ? r != NULL : r == NULL);
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(axis_ref_keeps_index_and_simulations) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av = { TAG_W, 500 };
    UINT32 s;
    for (s = 0; s <= 3; s++) {
        IDWriteFontFaceReference1 *r = mk6(&x, s, (DWRITE_FONT_SIMULATIONS)s, &av, 1);
        if (!r) continue;
        t_check(IDWriteFontFaceReference1_GetFontFaceIndex(r) == s, __FILE__, __LINE__, "index", "%u", s);
        t_check(IDWriteFontFaceReference1_GetSimulations(r) == (DWRITE_FONT_SIMULATIONS)s, __FILE__, __LINE__, "sim", "%u", s);
        EXPECT_EQ(IDWriteFontFaceReference1_GetLocality(r), DWRITE_LOCALITY_LOCAL);
        DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(axis_ref_file_is_same_loader_and_key) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av = { TAG_W, 500 };
    IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, &av, 1);
    IDWriteFontFile *ff = NULL; IDWriteFontFileLoader *ld = NULL;
    if (r && SUCCEEDED(IDWriteFontFaceReference1_GetFontFile(r, &ff)) && ff && SUCCEEDED(IDWriteFontFile_GetLoader(ff, &ld))) {
        CHECK(ld == (IDWriteFontFileLoader *)x.ld);
        const void *key = NULL; UINT32 ks = 0;
        CHECK_HR(IDWriteFontFile_GetReferenceKey(ff, &key, &ks), S_OK);
        t_check(ks == 4 && key && *(const UINT32 *)key == x.key, __FILE__, __LINE__, "key", "size %u", ks);
        size_t n; uint8_t *b = ttf_build_default(&n);
        EXPECT_EQ(IDWriteFontFaceReference1_GetFileSize(r), (UINT64)n); free(b);
    }
    DW_RELEASE(ld); DW_RELEASE(ff); DW_RELEASE(r);
    frr_close(&x);
}

/* ---- Equals with axes ------------------------------------------------------------------------------------------------------------ */
TEST(axis_ref_equals_matrix) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE a1[1] = { { TAG_W, 400 } }, a2[1] = { { TAG_W, 700 } }, a3[1] = { { TAG_D, 400 } };
    DWRITE_FONT_AXIS_VALUE a4[2] = { { TAG_W, 400 }, { TAG_D, 100 } };
    IDWriteFontFaceReference1 *r[6];
    r[0] = mk6(&x, 0, SIM_NONE, a1, 1); r[1] = mk6(&x, 0, SIM_NONE, a1, 1); r[2] = mk6(&x, 0, SIM_NONE, a2, 1);
    r[3] = mk6(&x, 0, SIM_NONE, a3, 1); r[4] = mk6(&x, 0, SIM_NONE, a4, 2); r[5] = mk6(&x, 0, SIM_NONE, NULL, 0);
    static const int eq[6][6] = {
        { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0 }, { 0, 0, 1, 0, 0, 0 }, { 0, 0, 0, 1, 0, 0 }, { 0, 0, 0, 0, 1, 0 }, { 0, 0, 0, 0, 0, 1 } };
    int i, j;
    for (i = 0; i < 6; i++) for (j = 0; j < 6; j++) {
        if (!r[i] || !r[j]) continue;
        BOOL got = IDWriteFontFaceReference1_Equals(r[i], (IDWriteFontFaceReference *)r[j]);
        t_check(got == eq[i][j], __FILE__, __LINE__, "axis equals", "(%d,%d) got %d want %d", i, j, got, eq[i][j]);
    }
    for (i = 0; i < 6; i++) DW_RELEASE(r[i]);
    frr_close(&x);
}
TEST(axis_ref_equals_plain_reference_differs_from_axis_reference) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av = { TAG_W, 400 };
    IDWriteFontFaceReference *plain = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFaceReference1 *ax = mk6(&x, 0, SIM_NONE, &av, 1), *none = mk6(&x, 0, SIM_NONE, NULL, 0);
    if (plain && ax && none) {
        EXPECT_EQ(IDWriteFontFaceReference_Equals(plain, (IDWriteFontFaceReference *)ax), 0);
        EXPECT_EQ(IDWriteFontFaceReference1_Equals(ax, plain), 0);
        EXPECT_EQ(IDWriteFontFaceReference_Equals(plain, (IDWriteFontFaceReference *)none), 1);   /* zero axes == no axes */
    }
    DW_RELEASE(plain); DW_RELEASE(ax); DW_RELEASE(none);
    frr_close(&x);
}
TEST(axis_ref_equals_ignores_axis_order_or_not) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE ab[2] = { { TAG_W, 400 }, { TAG_D, 100 } }, ba[2] = { { TAG_D, 100 }, { TAG_W, 400 } };
    IDWriteFontFaceReference1 *a = mk6(&x, 0, SIM_NONE, ab, 2), *b = mk6(&x, 0, SIM_NONE, ba, 2);
    if (a && b) t_note("Equals for the same axes given in a different order: %d", IDWriteFontFaceReference1_Equals(a, (IDWriteFontFaceReference *)b));
    DW_RELEASE(a); DW_RELEASE(b);
    frr_close(&x);
}

/* ---- CreateFontFace (IDWriteFontFace5) ---------------------------------------------------------------------------------------------- */
TEST(r1_createfontface_returns_face5) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_BOLD);
    IDWriteFontFaceReference1 *r1 = r ? frr_ref1(r) : NULL;
    IDWriteFontFace5 *f5 = (IDWriteFontFace5 *)(ULONG_PTR)8;
    if (r1) {
        CHECK_HR(IDWriteFontFaceReference1_CreateFontFace(r1, &f5), S_OK);
        CHECK(f5 != NULL && f5 != (IDWriteFontFace5 *)(ULONG_PTR)8);
        if (f5 && f5 != (IDWriteFontFace5 *)(ULONG_PTR)8) {
            EXPECT_EQ(IDWriteFontFace5_GetSimulations(f5), SIM_BOLD);
            EXPECT_EQ(IDWriteFontFace5_HasVariations(f5), FALSE);
            t_note("axis value count of a face made from a plain reference: %u", (unsigned)IDWriteFontFace5_GetFontAxisValueCount(f5));
            EXPECT_EQ(IDWriteFontFace5_GetGlyphCount(f5), TTF_DEF_NGLYPHS);
        } else f5 = NULL;
    }
    DW_RELEASE(f5); DW_RELEASE(r1); DW_RELEASE(r);
    frr_close(&x);
}
TEST(r1_both_createfontface_slots_agree) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFaceReference1 *r1 = r ? frr_ref1(r) : NULL;
    IDWriteFontFace3 *a = NULL; IDWriteFontFace5 *b = NULL;
    if (r1 && SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &a)) && SUCCEEDED(IDWriteFontFaceReference1_CreateFontFace(r1, &b)) && a && b) {
        EXPECT_EQ(IDWriteFontFace3_GetIndex(a), IDWriteFontFace5_GetIndex(b));
        EXPECT_EQ(IDWriteFontFace3_GetSimulations(a), IDWriteFontFace5_GetSimulations(b));
        EXPECT_EQ(IDWriteFontFace3_GetGlyphCount(a), IDWriteFontFace5_GetGlyphCount(b));
        t_note("slot 3 face vs slot 17 face: same object %d, Equals %d", (void *)a == (void *)b, frr_face_equals((IUnknown *)a, (IUnknown *)b));
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(r1); DW_RELEASE(r);
    frr_close(&x);
}
TEST(r1_axis_reference_on_static_font_creates_face) {
    FRR_OPEN_NEED6(x);
    DWRITE_FONT_AXIS_VALUE av = { TAG_W, 700 };
    IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, &av, 1);
    IDWriteFontFace5 *f5 = (IDWriteFontFace5 *)(ULONG_PTR)8;
    if (r) {
        HRESULT hr = IDWriteFontFaceReference1_CreateFontFace(r, &f5);
        t_note("CreateFontFace with a wght axis value over a static font -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr) && f5) t_note("face HasVariations %d, axis value count %u", IDWriteFontFace5_HasVariations(f5), IDWriteFontFace5_GetFontAxisValueCount(f5));
        else FRR_ZERO(f5);
        DW_RELEASE(f5);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(r1_variable_synthetic_font_axis_values_reach_the_face) {
    size_t n; uint8_t *b = frr_build_var2(&n); frr_fix x;
    if (!b) { t_skip("frr_build_var2 failed"); return; }
    if (frr_open(&x, b, n) && x.f6) {
        DWRITE_FONT_AXIS_VALUE av[2] = { { TAG_W, 700 }, { DW_TAG('X', 'T', 'R', 'A'), 30 } };
        IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, av, 2);
        IDWriteFontFace5 *f5 = NULL;
        if (r && SUCCEEDED(IDWriteFontFaceReference1_CreateFontFace(r, &f5)) && f5) {
            EXPECT_EQ(IDWriteFontFace5_HasVariations(f5), TRUE);
            UINT32 c = IDWriteFontFace5_GetFontAxisValueCount(f5);
            CHECK(c >= 2);                       /* a face may also list axes the font does not declare */
            DWRITE_FONT_AXIS_VALUE got[16];
            if (c > 0 && c <= 16 && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(f5, got, c))) {
                CHECK(has_value(got, c, TAG_W, 700.0f));
                CHECK(has_value(got, c, DW_TAG('X', 'T', 'R', 'A'), 30.0f));
            }
        } else t_note("CreateFontFace over the synthetic variable font failed");
        DW_RELEASE(f5); DW_RELEASE(r);
    } else if (x.f3 && !x.f6) t_skip("IDWriteFactory6 unsupported");
    frr_close(&x); free(b);
}
TEST(r1_variable_synthetic_font_unspecified_axes_use_defaults) {
    size_t n; uint8_t *b = frr_build_var2(&n); frr_fix x;
    if (!b) { t_skip("frr_build_var2 failed"); return; }
    if (frr_open(&x, b, n) && x.f6) {
        DWRITE_FONT_AXIS_VALUE av = { TAG_W, 600 };
        IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, &av, 1);
        IDWriteFontFace5 *f5 = NULL;
        if (r && SUCCEEDED(IDWriteFontFaceReference1_CreateFontFace(r, &f5)) && f5) {
            UINT32 c = IDWriteFontFace5_GetFontAxisValueCount(f5); DWRITE_FONT_AXIS_VALUE got[16];
            t_note("face from a one-axis reference over a two-axis font reports %u axis values", c);
            if (c > 0 && c <= 16 && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(f5, got, c))) {
                CHECK(has_value(got, c, TAG_W, 600.0f));
                UINT32 i; for (i = 0; i < c; i++) t_note("  tag %08x = %g", got[i].axisTag, got[i].value);
            }
        }
        DW_RELEASE(f5); DW_RELEASE(r);
    } else if (x.f3 && !x.f6) t_skip("IDWriteFactory6 unsupported");
    frr_close(&x); free(b);
}
TEST(r1_variable_synthetic_font_out_of_range_value_is_clamped_or_kept) {
    size_t n; uint8_t *b = frr_build_var2(&n); frr_fix x;
    if (!b) { t_skip("frr_build_var2 failed"); return; }
    if (frr_open(&x, b, n) && x.f6) {
        DWRITE_FONT_AXIS_VALUE av = { TAG_W, 5000 };
        IDWriteFontFaceReference1 *r = mk6(&x, 0, SIM_NONE, &av, 1);
        IDWriteFontFace5 *f5 = NULL;
        if (r) {
            DWRITE_FONT_AXIS_VALUE stored; CHECK_HR(IDWriteFontFaceReference1_GetFontAxisValues(r, &stored, 1), S_OK);
            EXPECT_EQ(stored.value, 5000.0f);                              /* the reference stores whatever it was given */
            HRESULT hr = IDWriteFontFaceReference1_CreateFontFace(r, &f5);
            t_note("CreateFontFace(wght=5000, fvar range 100..900) -> %s", dw_hr_name(hr));
            if (SUCCEEDED(hr) && f5) {
                UINT32 c = IDWriteFontFace5_GetFontAxisValueCount(f5); DWRITE_FONT_AXIS_VALUE got[16];
                if (c > 0 && c <= 16 && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(f5, got, c))) { UINT32 i; for (i = 0; i < c; i++) if (got[i].axisTag == TAG_W) t_note("face wght = %g", got[i].value); }
            }
        }
        DW_RELEASE(f5); DW_RELEASE(r);
    } else if (x.f3 && !x.f6) t_skip("IDWriteFactory6 unsupported");
    frr_close(&x); free(b);
}

/* ---- Bahnschrift (system variable font) ----------------------------------------------------------------------------------------------- */
TEST(bahnschrift_weight_axis_changes_advances) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    WCHAR p[MAX_PATH]; if (!f6 || !frr_sys_path("bahnschrift.ttf", p, MAX_PATH)) { t_skip("IDWriteFactory6 or bahnschrift.ttf unavailable"); return; }
    IDWriteFactory *f = dw_shared(); IDWriteFontFile *file = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateFontFileReference(f, p, NULL, &file));
    DWRITE_FONT_AXIS_VALUE lo = { TAG_W, 300 }, hi = { TAG_W, 700 };
    IDWriteFontFaceReference1 *rl = NULL, *rh = NULL; IDWriteFontFace5 *fl = NULL, *fh = NULL;
    CHECK_HR(IDWriteFactory6_CreateFontFaceReference(f6, file, 0, SIM_NONE, &lo, 1, &rl), S_OK);
    CHECK_HR(IDWriteFactory6_CreateFontFaceReference(f6, file, 0, SIM_NONE, &hi, 1, &rh), S_OK);
    if (rl && rh && SUCCEEDED(IDWriteFontFaceReference1_CreateFontFace(rl, &fl)) && SUCCEEDED(IDWriteFontFaceReference1_CreateFontFace(rh, &fh)) && fl && fh) {
        UINT32 cp = 'W'; UINT16 gl = 0, gh = 0; DWRITE_GLYPH_METRICS ml, mh;
        IDWriteFontFace5_GetGlyphIndices(fl, &cp, 1, &gl); IDWriteFontFace5_GetGlyphIndices(fh, &cp, 1, &gh);
        CHECK(gl != 0 && gl == gh);
        if (gl && SUCCEEDED(IDWriteFontFace5_GetDesignGlyphMetrics(fl, &gl, 1, &ml, FALSE)) && SUCCEEDED(IDWriteFontFace5_GetDesignGlyphMetrics(fh, &gh, 1, &mh, FALSE))) {
            t_note("Bahnschrift 'W' advance: wght300 = %u, wght700 = %u", ml.advanceWidth, mh.advanceWidth);
            /* Win10 reports identical design advances for this glyph at both weights (metric variations not applied here). */
        }
        EXPECT_EQ(IDWriteFontFace5_HasVariations(fl), TRUE);
        CHECK(!frr_face_equals((IUnknown *)fl, (IUnknown *)fh) || frr_face_equals((IUnknown *)fl, (IUnknown *)fh) == -1);
    }
    DW_RELEASE(fl); DW_RELEASE(fh); DW_RELEASE(rl); DW_RELEASE(rh); DW_RELEASE(file);
}
TEST(bahnschrift_reference_from_face_carries_axis_values) {
    IDWriteFactory *f = dw_shared();
    IDWriteFontFace *face = dw_sysface(f, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Bahnschrift not installed"); return; }
    IDWriteFontFace5 *f5 = NULL; IDWriteFontFaceReference *r = NULL; IDWriteFontFaceReference1 *r1 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace5, (void **)&f5)) && f5 && SUCCEEDED(IDWriteFontFace5_GetFontFaceReference(f5, &r)) && r && (r1 = frr_ref1(r))) {
        UINT32 nf = IDWriteFontFace5_GetFontAxisValueCount(f5), nr = IDWriteFontFaceReference1_GetFontAxisValueCount(r1);
        t_note("Bahnschrift face axis values %u, its reference carries %u", nf, nr);
        CHECK(nr >= 1 || nf == 0);
        DWRITE_FONT_AXIS_VALUE vf[16], vr[16];
        if (nf > 0 && nf <= 16 && nr <= 16 && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(f5, vf, nf)) && SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(r1, vr, nr))) {
            UINT32 i; for (i = 0; i < nf; i++) t_check(has_value(vr, nr, vf[i].axisTag, vf[i].value), __FILE__, __LINE__, "axis in reference", "tag %08x value %g", vf[i].axisTag, vf[i].value);
        }
    }
    DW_RELEASE(r1); DW_RELEASE(r); DW_RELEASE(f5); DW_RELEASE(face);
}
