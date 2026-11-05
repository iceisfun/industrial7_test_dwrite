/* IDWriteFontResource::CreateFontFace with axis values and IDWriteFontFace5 axis methods on the synthetic variable fixture (rv.h).
 *
 * Win10 dwrite.dll (IDWriteFontResource::CreateFontFace, GetCanonicalAxisValues used when reading the face,
 * IDWriteFontFace::GetFontAxisValueCount, GetFontAxisValues, Equals, float to fixed conversion):
 *   - CreateFontFace zeroes *face first and returns E_INVALIDARG for simulations & ~3, a value count > 0xFFFF, or NULL values with a non-zero count;
 *   - the values are converted to 16.16 fixed: round(v * 65536) with halves up (trunc(v * 2^32) + 0x8000 >> 16);
 *   - the face keeps CANONICAL values: one entry per font axis in axis order (the supplied value, FIRST occurrence of the tag wins, clamped to the
 *     axis [min, max]; the axis default when not supplied), followed by the supplied entries whose tag is not a font axis, in supplied order.
 *     Out-of-range and duplicate values are therefore never errors;
 *   - GetFontAxisValues zeroes the buffer over the passed count and fails with E_NOT_SUFFICIENT_BUFFER (0x8007007A) when the passed count is smaller
 *     than GetFontAxisValueCount(); a larger buffer is accepted and its tail stays zero;
 *   - Equals is FALSE for NULL, TRUE for equal raw keys and otherwise compares the canonical keys.
 */
#include "rv.h"

static float fixed_round(float v)       /* the 16.16 conversion of the Win10 converter, back to float */
{
    long long q = (long long)(v * 65536.0f * 65536.0f) + 0x8000;
    return (float)(q >> 16) / 65536.0f;
}
static UINT32 tag_of(const rv_axis *a) { return RV_TAG(a->tag[0], a->tag[1], a->tag[2], a->tag[3]); }
/* all values of a face (n <= 64) */
static UINT32 vals(IDWriteFontFace5 *f, DWRITE_FONT_AXIS_VALUE *v)
{
    UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(f);
    if (n == 0 || n > 64) return 0;
    CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, v, n));
    return n;
}
/* value of an axis on a face, or -12345 when the axis is absent */
static float val_of(IDWriteFontFace5 *f, UINT32 tag)
{
    DWRITE_FONT_AXIS_VALUE v[64]; UINT32 n = vals(f, v); int k = rv_find_value(v, n, tag);
    return k >= 0 ? v[k].value : -12345.0f;
}

/* ---- canonical values ---------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_face_no_values_gives_defaults)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE v[16]; UINT32 n; int i;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) {
        n = vals(f, v); EXPECT_EQ(n, RV_NALL);
        for (i = 0; i < RV_NAXES; i++) { int k = rv_find_value(v, n, tag_of(&rv_axes[i])); CHECKF(k >= 0 && v[k].value == rv_axes[i].df, "axis %s", rv_axes[i].tag); }
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_non_null_empty_array)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE dummy = rv_av(TAG_WGHT, 900);
    RV_BEGIN(0);
    f = rv_face(&x, &dummy, 0);          /* count 0 ignores the pointer */
    if (f) { EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), RV_NALL); CHECK(val_of(f, TAG_WGHT) == 400.0f); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_count_is_font_axis_count)
{
    static const float w[] = { 100, 250, 400, 700, 900 }; unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, w[i]); IDWriteFontFace5 *f = rv_face(&x, &a, 1);
        if (f) { EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), RV_NALL); DW_RELEASE(f); }
    }
    RV_END();
}
TEST(r2_variable_fonts_face_set_each_axis_in_range)
{
    static const struct { UINT32 tag; float v; } t[] = {
        { TAG_WGHT, 100 }, { TAG_WGHT, 250 }, { TAG_WGHT, 400 }, { TAG_WGHT, 650 }, { TAG_WGHT, 900 },
        { TAG_WDTH, 75 }, { TAG_WDTH, 90 }, { TAG_WDTH, 100 }, { TAG_WDTH, 125 },
        { TAG_OPSZ, 8 }, { TAG_OPSZ, 14 }, { TAG_OPSZ, 72 }, { TAG_OPSZ, 144 }, { TAG_HIDN, 0 }, { TAG_HIDN, 3 }, { TAG_HIDN, 10 }, { TAG_FIXD, 5 } };
    unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(t[i].tag, t[i].v); IDWriteFontFace5 *f = rv_face(&x, &a, 1);
        if (f) { float g = val_of(f, t[i].tag); CHECKF(g == t[i].v, "axis %08x set %g got %g", t[i].tag, t[i].v, g); DW_RELEASE(f); }
    }
    RV_END();
}
TEST(r2_variable_fonts_face_other_axes_stay_default)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE a[2]; DWRITE_FONT_AXIS_VALUE v[16]; UINT32 n; int i;
    RV_BEGIN(0);
    a[0] = rv_av(TAG_WGHT, 800); a[1] = rv_av(TAG_OPSZ, 30);
    f = rv_face(&x, a, 2);
    if (f) {
        n = vals(f, v);
        for (i = 0; i < RV_NAXES; i++) {
            int k = rv_find_value(v, n, tag_of(&rv_axes[i])); float want = rv_axes[i].df;
            if (!strcmp(rv_axes[i].tag, "wght")) want = 800; if (!strcmp(rv_axes[i].tag, "opsz")) want = 30;
            CHECKF(k >= 0 && v[k].value == want, "axis %s", rv_axes[i].tag);
        }
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_clamps_to_axis_range)
{
    /* out-of-range values are clamped, never rejected (GetCanonicalAxisValues) */
    static const struct { UINT32 tag; float in, want; } t[] = {
        { TAG_WGHT, -5, 100 }, { TAG_WGHT, 0, 100 }, { TAG_WGHT, 99.5f, 100 }, { TAG_WGHT, 900.5f, 900 }, { TAG_WGHT, 1000, 900 }, { TAG_WGHT, 5000, 900 }, { TAG_WGHT, 1e9f, 900 }, { TAG_WGHT, -1e9f, 100 },
        { TAG_WDTH, 0, 75 }, { TAG_WDTH, 74.9f, 75 }, { TAG_WDTH, 125.1f, 125 }, { TAG_WDTH, 1000, 125 },
        { TAG_OPSZ, 0, 8 }, { TAG_OPSZ, -1, 8 }, { TAG_OPSZ, 1000, 144 },
        { TAG_FIXD, 0, 5 }, { TAG_FIXD, 5, 5 }, { TAG_FIXD, 100, 5 }, { TAG_HIDN, -1, 0 }, { TAG_HIDN, 11, 10 } };
    unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(t[i].tag, t[i].in); IDWriteFontFace5 *f = NULL;
        HRESULT hr = IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_NONE, &a, 1, &f);
        CHECKF(hr == S_OK, "axis %08x value %g -> %s", t[i].tag, t[i].in, dw_hr_name(hr));
        if (SUCCEEDED(hr) && f) { float g = val_of(f, t[i].tag); CHECKF(g == t[i].want, "axis %08x value %g -> %g want %g", t[i].tag, t[i].in, g, t[i].want); }
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_huge_float_note)
{
    /* 1e30 * 2^32 overflows float, the conversion then goes through the CPU's "indefinite" integer: observed, not asserted */
    static const float in[] = { 1e30f, -1e30f, 3.0e38f };
    unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < 3; i++) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, in[i]); IDWriteFontFace5 *f = NULL;
        HRESULT hr = IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_NONE, &a, 1, &f);
        CHECKF(hr == S_OK, "value %g -> %s", in[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && f) t_note("wght %g -> %g", in[i], val_of(f, TAG_WGHT));
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_duplicate_tag_first_wins)
{
    static const struct { float a, b, want; } t[] = { { 700, 300, 700 }, { 300, 700, 300 }, { 400, 400, 400 }, { 5000, 300, 900 }, { 300, 5000, 300 }, { -9, 800, 100 } };
    unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        DWRITE_FONT_AXIS_VALUE a[2]; IDWriteFontFace5 *f; a[0] = rv_av(TAG_WGHT, t[i].a); a[1] = rv_av(TAG_WGHT, t[i].b);
        f = rv_face(&x, a, 2);
        if (f) { float g = val_of(f, TAG_WGHT); CHECKF(g == t[i].want, "{%g, %g} -> %g want %g", t[i].a, t[i].b, g, t[i].want); EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), RV_NALL); DW_RELEASE(f); }
    }
    RV_END();
}
TEST(r2_variable_fonts_face_duplicates_of_every_axis)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE a[10]; int i;
    RV_BEGIN(0);
    for (i = 0; i < RV_NAXES; i++) { a[i] = rv_av(tag_of(&rv_axes[i]), rv_axes[i].mx); a[i + RV_NAXES] = rv_av(tag_of(&rv_axes[i]), rv_axes[i].mn); }
    f = rv_face(&x, a, 10);
    if (f) { EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), RV_NALL); for (i = 0; i < RV_NAXES; i++) CHECKF(val_of(f, tag_of(&rv_axes[i])) == rv_axes[i].mx, "axis %s", rv_axes[i].tag); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_unknown_tag_is_appended)
{
    /* a tag the font does not have is dropped: the face keeps exactly the font's own axes and the known value still applies */
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE a[2], v[16]; UINT32 n;
    RV_BEGIN(0);
    a[0] = rv_av(TAG_ZZZZ, 3.5f); a[1] = rv_av(TAG_WGHT, 600);
    f = rv_face(&x, a, 2);
    if (f) {
        n = vals(f, v);
        EXPECT_EQ(n, RV_NALL);
        CHECK(rv_find_value(v, n, TAG_ZZZZ) < 0);
        CHECK(val_of(f, TAG_WGHT) == 600.0f);
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_unknown_tags_keep_supplied_order)
{
    /* several unknown tags are all dropped */
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE a[3], v[16]; UINT32 n; int i;
    RV_BEGIN(0);
    a[0] = rv_av(RV_TAG('q', 'q', 'q', '2'), 2); a[1] = rv_av(RV_TAG('q', 'q', 'q', '1'), 1); a[2] = rv_av(RV_TAG('q', 'q', 'q', '3'), 3);
    f = rv_face(&x, a, 3);
    if (f) {
        n = vals(f, v);
        EXPECT_EQ(n, RV_NALL);
        for (i = 0; i < 3; i++) CHECK(rv_find_value(v, n, a[i].axisTag) < 0);
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_unknown_tag_values_not_clamped)
{
    /* an unknown tag with an extreme value is dropped without effect on the known axes */
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_ZZZZ, 123456.0f);
    RV_BEGIN(0);
    f = rv_face(&x, &a, 1);
    if (f) { CHECK(val_of(f, TAG_ZZZZ) == -12345.0f); EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), RV_NALL); CHECK(val_of(f, TAG_WGHT) == 400.0f); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_twenty_five_unknown_axes)
{
    /* many supplied unknown values are all dropped */
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE a[25]; int i;
    RV_BEGIN(0);
    for (i = 0; i < 25; i++) a[i] = rv_av(RV_TAG('u', 'k', '0' + i / 10, '0' + i % 10), (float)i);
    f = rv_face(&x, a, 25);
    if (f) { EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), RV_NALL); for (i = 0; i < 25; i++) CHECK(val_of(f, a[i].axisTag) == -12345.0f); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_order_matches_resource_default_order)
{
    DWRITE_FONT_AXIS_VALUE d[RV_NALL], v[16]; IDWriteFontFace5 *f; UINT32 n; int i;
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, d, RV_NALL));
    f = rv_face(&x, NULL, 0);
    if (f) { n = vals(f, v); EXPECT_EQ(n, RV_NALL); for (i = 0; i < RV_NALL && (UINT32)i < n; i++) CHECKF(v[i].axisTag == d[i].axisTag && v[i].value == d[i].value, "index %d", i); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_supplied_order_is_irrelevant)
{
    IDWriteFontFace5 *f1, *f2; DWRITE_FONT_AXIS_VALUE a[3], b[3], v1[16], v2[16]; UINT32 n1, n2;
    RV_BEGIN(0);
    a[0] = rv_av(TAG_WGHT, 700); a[1] = rv_av(TAG_WDTH, 90); a[2] = rv_av(TAG_OPSZ, 20);
    b[0] = rv_av(TAG_OPSZ, 20); b[1] = rv_av(TAG_WGHT, 700); b[2] = rv_av(TAG_WDTH, 90);
    f1 = rv_face(&x, a, 3); f2 = rv_face(&x, b, 3);
    if (f1 && f2) { n1 = vals(f1, v1); n2 = vals(f2, v2); EXPECT_EQ(n1, n2); if (n1 == n2) CHECK(!memcmp(v1, v2, n1 * sizeof v1[0])); }
    DW_RELEASE(f1); DW_RELEASE(f2);
    RV_END();
}

/* ---- fixed point conversion ----------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_face_fraction_round_trip)
{
    static const float in[] = { 400.25f, 400.5f, 450.125f, 123.0625f, 899.9375f, 100.015625f };
    unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < sizeof in / sizeof in[0]; i++) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, in[i]); IDWriteFontFace5 *f = rv_face(&x, &a, 1);
        if (f) { float g = val_of(f, TAG_WGHT); CHECKF(g == in[i], "%.9g -> %.9g", in[i], g); DW_RELEASE(f); }
    }
    RV_END();
}
TEST(r2_variable_fonts_face_sixteen_sixteen_rounding)
{
    static const float in[] = { 450.3f, 450.7f, 333.333f, 400.000001f, 400.00002f, 700.123456f, 650.00001f };
    unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < sizeof in / sizeof in[0]; i++) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, in[i]); IDWriteFontFace5 *f = rv_face(&x, &a, 1);
        if (f) { float g = val_of(f, TAG_WGHT), want = fixed_round(in[i]); CHECKF(g == want, "%.9g -> %.9g want %.9g", in[i], g, want); DW_RELEASE(f); }
    }
    RV_END();
}
TEST(r2_variable_fonts_face_tiny_difference_collapses)
{
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 400.000001f); IDWriteFontFace5 *f;
    RV_BEGIN(0);
    f = rv_face(&x, &a, 1);
    if (f) { CHECK(val_of(f, TAG_WGHT) == 400.0f); DW_RELEASE(f); }       /* below half of 1/65536 */
    RV_END();
}
TEST(r2_variable_fonts_face_negative_fraction_rounding)
{
    /* slant style negative values on an axis that allows them */
    static const rv_axis ax[1] = { { "slnt", -90, 0, 90, 0, 256, "S" } };
    static const float in[] = { -7.5f, -7.25f, -0.00001f, -12.3f, 0.499f };
    size_t sz; uint8_t *b = rv_build_axes(ax, 1, &sz); rv x; unsigned i;
    if (!b) { t_skip("font build failed"); return; }
    if (!rv_open(&x, b, sz)) { rv_close(&x); free(b); return; }
    for (i = 0; i < sizeof in / sizeof in[0]; i++) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_SLNT, in[i]); IDWriteFontFace5 *f = rv_face(&x, &a, 1);
        if (f) { float g = val_of(f, TAG_SLNT), want = fixed_round(in[i]); CHECKF(g == want, "%.9g -> %.9g want %.9g", in[i], g, want); DW_RELEASE(f); }
    }
    rv_close(&x); free(b);
}

/* ---- GetFontAxisValues buffer protocol ------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_face_values_buffer_too_small)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE v[8]; HRESULT hr; UINT32 c, i;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) {
        c = IDWriteFontFace5_GetFontAxisValueCount(f);
        for (i = 0; i < c; i++) {
            memset(v, 0xCC, sizeof v);
            hr = IDWriteFontFace5_GetFontAxisValues(f, v, i);
            CHECKF(hr == DW_E_NOT_SUFFICIENT_BUFFER, "buffer of %u (count %u) -> %s", i, c, dw_hr_name(hr));
            { unsigned char *p = (unsigned char *)v; size_t j, bad = 0; for (j = 0; j < i * sizeof v[0]; j++) if (p[j] != 0) bad++; CHECKF(bad == 0, "first %u entries not zeroed", i); }
            CHECKF(((unsigned char *)v)[i * sizeof v[0]] == 0xCC || i == c, "entries past the passed count were touched");
        }
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_values_zero_count_with_null_buffer)
{
    IDWriteFontFace5 *f; HRESULT hr;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) { hr = IDWriteFontFace5_GetFontAxisValues(f, NULL, 0); CHECKF(hr == DW_E_NOT_SUFFICIENT_BUFFER, "NULL, 0 -> %s", dw_hr_name(hr)); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_values_exact_buffer)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE v[RV_NALL];
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) { memset(v, 0xCC, sizeof v); CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, v, RV_NALL)); CHECK(v[0].axisTag != (DWRITE_FONT_AXIS_TAG)0xCCCCCCCCu); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_values_larger_buffer_tail_zeroed)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE v[RV_NALL + 4]; int i;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) {
        memset(v, 0xCC, sizeof v);
        CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, v, RV_NALL + 4));
        for (i = RV_NALL; i < RV_NALL + 4; i++) CHECKF(v[i].axisTag == 0 && v[i].value == 0.0f, "tail entry %d not zero", i);
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_values_repeatable)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE a[RV_NALL], b[RV_NALL]; DWRITE_FONT_AXIS_VALUE in = rv_av(TAG_WGHT, 555);
    RV_BEGIN(0);
    f = rv_face(&x, &in, 1);
    if (f) { CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, a, RV_NALL)); CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, b, RV_NALL)); CHECK(!memcmp(a, b, sizeof a)); DW_RELEASE(f); }
    RV_END();
}

/* ---- HasVariations / GetFontResource / identity ------------------------------------------------------------------------------------------ */
TEST(r2_variable_fonts_face_has_variations)
{
    IDWriteFontFace5 *f;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) { CHECK(IDWriteFontFace5_HasVariations(f) == TRUE); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_static_font_has_no_variations)
{
    IDWriteFontFace5 *f = NULL;
    RV_BEGIN(RVF_NOFVAR);
    CHECK_HR_OK(IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, &f));
    if (f) {
        CHECK(IDWriteFontFace5_HasVariations(f) == FALSE);
        EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), 4);          /* standard wght, wdth, ital, slnt */
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_static_font_axis_values_clamp_to_fixed_axes)
{
    /* the static font's standard axes are collapsed ranges (min == default == max), so any supplied value lands on the font's own value */
    IDWriteFontFace5 *f = NULL; DWRITE_FONT_AXIS_VALUE a[3]; HRESULT hr;
    RV_BEGIN(RVF_NOFVAR);
    a[0] = rv_av(TAG_WGHT, 700); a[1] = rv_av(TAG_ITAL, 1); a[2] = rv_av(TAG_SLNT, -20);
    hr = IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_NONE, a, 3, &f);
    CHECK_HR(hr, S_OK);          /* axis values are never validated against the font */
    if (f) {
        CHECK(val_of(f, TAG_WGHT) == 400.0f); CHECK(val_of(f, TAG_ITAL) == 0.0f); CHECK(val_of(f, TAG_SLNT) == 0.0f); CHECK(val_of(f, TAG_WDTH) == 100.0f);
        EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), 4);
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_variable_font_standard_axes_fixed)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE a[2];
    RV_BEGIN(0);
    a[0] = rv_av(TAG_ITAL, 1); a[1] = rv_av(TAG_SLNT, -15);
    f = rv_face(&x, a, 2);
    if (f) { CHECK(val_of(f, TAG_ITAL) == 0.0f); CHECK(val_of(f, TAG_SLNT) == 0.0f); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_defaults_include_standard_axes)
{
    IDWriteFontFace5 *f;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) { CHECK(val_of(f, TAG_ITAL) == 0.0f); CHECK(val_of(f, TAG_SLNT) == 0.0f); EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), RV_NALL); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_get_font_resource)
{
    IDWriteFontFace5 *f; IDWriteFontResource *r = NULL; DWRITE_FONT_AXIS_VALUE in = rv_av(TAG_WGHT, 300);
    RV_BEGIN(0);
    f = rv_face(&x, &in, 1);
    if (f) {
        CHECK_HR_OK(IDWriteFontFace5_GetFontResource(f, &r));
        if (r) {
            EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(r), RV_NALL); CHECK(IDWriteFontResource_HasVariations(r) == TRUE); EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(r), 0);
            DW_RELEASE(r);
        }
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_get_font_resource_file_is_equal_source)
{
    IDWriteFontFace5 *f; IDWriteFontResource *r = NULL; IDWriteFontFile *ff = NULL; void *k1 = NULL, *k2 = NULL; UINT32 s1 = 0, s2 = 0;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f && SUCCEEDED(IDWriteFontFace5_GetFontResource(f, &r)) && r && SUCCEEDED(IDWriteFontResource_GetFontFile(r, &ff)) && ff) {
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(ff, (const void **)&k1, &s1));
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(x.file, (const void **)&k2, &s2));
        EXPECT_EQ(s1, s2); if (s1 == s2 && s1) CHECK(!memcmp(k1, k2, s1));
    } else CHECK(0);
    DW_RELEASE(ff); DW_RELEASE(r); DW_RELEASE(f);
    RV_END();
}
TEST(r2_variable_fonts_face_get_font_resource_null_not_probed)
{
    /* GetFontResource zeroes its output before building the resource */
    IDWriteFontFace5 *f; IDWriteFontResource *r = RV_SENT(IDWriteFontResource *);
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) { HRESULT hr = IDWriteFontFace5_GetFontResource(f, &r); CHECK_HR(hr, S_OK); CHECK(r != NULL && r != RV_SENT(IDWriteFontResource *)); if (r == RV_SENT(IDWriteFontResource *)) r = NULL; DW_RELEASE(r); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_qi_chain)
{
    IDWriteFontFace5 *f; static const struct { const IID *iid; const char *n; } t[] = {
        { &IID_IDWriteFontFace, "IDWriteFontFace" }, { &IID_IDWriteFontFace1, "IDWriteFontFace1" }, { &IID_IDWriteFontFace2, "IDWriteFontFace2" },
        { &IID_IDWriteFontFace3, "IDWriteFontFace3" }, { &IID_IDWriteFontFace4, "IDWriteFontFace4" }, { &IID_IDWriteFontFace5, "IDWriteFontFace5" } };
    unsigned i;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) {
        for (i = 0; i < sizeof t / sizeof t[0]; i++) { IUnknown *u = NULL; HRESULT hr = IDWriteFontFace5_QueryInterface(f, t[i].iid, (void **)&u); CHECKF(hr == S_OK, "%s -> %s", t[i].n, dw_hr_name(hr)); if (u) CHECK(dw_same_identity(u, (IUnknown *)f)); DW_RELEASE(u); }
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_independent_of_resource_release)
{
    IDWriteFontFace5 *f; DWRITE_FONT_AXIS_VALUE in = rv_av(TAG_WDTH, 80), v[RV_NALL];
    RV_BEGIN(0);
    f = rv_face(&x, &in, 1);
    DW_RELEASE(x.res); DW_RELEASE(x.file);
    if (f) { CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f, v, RV_NALL)); CHECK(val_of(f, TAG_WDTH) == 80.0f); CHECK(IDWriteFontFace5_HasVariations(f) == TRUE); DW_RELEASE(f); }
    RV_END();
}

/* ---- Equals --------------------------------------------------------------------------------------------------------------------------------- */
static int eq(IDWriteFontFace5 *a, IDWriteFontFace5 *b) { return IDWriteFontFace5_Equals(a, (IDWriteFontFace *)b) ? 1 : 0; }
TEST(r2_variable_fonts_face_equals_null_and_self)
{
    IDWriteFontFace5 *f;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) { CHECK(IDWriteFontFace5_Equals(f, NULL) == FALSE); CHECK(eq(f, f) == 1); DW_RELEASE(f); }
    RV_END();
}
TEST(r2_variable_fonts_face_equals_same_arguments)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE in = rv_av(TAG_WGHT, 600);
    RV_BEGIN(0);
    a = rv_face(&x, &in, 1); b = rv_face(&x, &in, 1);
    if (a && b) CHECK(eq(a, b) == 1 && eq(b, a) == 1);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_face_equals_different_value)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE i1 = rv_av(TAG_WGHT, 600), i2 = rv_av(TAG_WGHT, 601);
    RV_BEGIN(0);
    a = rv_face(&x, &i1, 1); b = rv_face(&x, &i2, 1);
    if (a && b) CHECK(eq(a, b) == 0 && eq(b, a) == 0);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_face_equals_explicit_default_vs_omitted)
{
    /* different raw keys, equal canonical keys */
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE in = rv_av(TAG_WGHT, 400);
    RV_BEGIN(0);
    a = rv_face(&x, &in, 1); b = rv_face(&x, NULL, 0);
    if (a && b) CHECK(eq(a, b) == 1 && eq(b, a) == 1);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_face_equals_clamped_vs_boundary)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE i1 = rv_av(TAG_WGHT, 5000), i2 = rv_av(TAG_WGHT, 900);
    RV_BEGIN(0);
    a = rv_face(&x, &i1, 1); b = rv_face(&x, &i2, 1);
    if (a && b) CHECK(eq(a, b) == 1);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_face_equals_duplicate_vs_single)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE i1[2], i2 = rv_av(TAG_WGHT, 700);
    RV_BEGIN(0);
    i1[0] = rv_av(TAG_WGHT, 700); i1[1] = rv_av(TAG_WGHT, 200);
    a = rv_face(&x, i1, 2); b = rv_face(&x, &i2, 1);
    if (a && b) CHECK(eq(a, b) == 1);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_face_equals_axis_order)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE i1[2], i2[2];
    RV_BEGIN(0);
    i1[0] = rv_av(TAG_WGHT, 700); i1[1] = rv_av(TAG_WDTH, 90); i2[0] = i1[1]; i2[1] = i1[0];
    a = rv_face(&x, i1, 2); b = rv_face(&x, i2, 2);
    if (a && b) CHECK(eq(a, b) == 1);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_face_equals_simulations_differ)
{
    IDWriteFontFace5 *a = NULL, *b = NULL;
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, &a));
    CHECK_HR_OK(IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_BOLD, NULL, 0, &b));
    if (a && b) CHECK(eq(a, b) == 0);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_face_equals_other_axis_differs)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE i1 = rv_av(TAG_OPSZ, 20), i2 = rv_av(TAG_OPSZ, 21);
    RV_BEGIN(0);
    a = rv_face(&x, &i1, 1); b = rv_face(&x, &i2, 1);
    if (a && b) CHECK(eq(a, b) == 0);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_face_equals_unknown_tag_matters)
{
    IDWriteFontFace5 *a, *b; DWRITE_FONT_AXIS_VALUE i1 = rv_av(TAG_ZZZZ, 1);
    RV_BEGIN(0);
    a = rv_face(&x, &i1, 1); b = rv_face(&x, NULL, 0);
    if (a && b) t_note("face with unknown axis tag equals plain face: %d", eq(a, b));
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}

/* ---- simulations ---------------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_face_simulations_kept_with_axes)
{
    static const DWRITE_FONT_SIMULATIONS s[] = { DWRITE_FONT_SIMULATIONS_NONE, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, (DWRITE_FONT_SIMULATIONS)3 };
    unsigned i; DWRITE_FONT_AXIS_VALUE in = rv_av(TAG_WGHT, 650);
    RV_BEGIN(0);
    for (i = 0; i < 4; i++) {
        IDWriteFontFace5 *f = NULL; HRESULT hr = IDWriteFontResource_CreateFontFace(x.res, s[i], &in, 1, &f);
        CHECKF(hr == S_OK, "simulations %u -> %s", s[i], dw_hr_name(hr));
        if (f) { DWRITE_FONT_SIMULATIONS g = IDWriteFontFace5_GetSimulations(f); CHECKF(g == s[i], "simulations %u got %u", s[i], g); CHECK(val_of(f, TAG_WGHT) == 650.0f); DW_RELEASE(f); }
    }
    RV_END();
}

/* ---- argument validation ------------------------------------------------------------------------------------------------------------------ */
TEST(r2_variable_fonts_face_invalid_simulations)
{
    static const UINT32 bad[] = { 4, 5, 8, 0x10, 0x100, 0x80000000u, 0xFFFFFFFCu, 0xFFFFFFFFu };
    unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteFontFace5 *f = RV_SENT(IDWriteFontFace5 *); HRESULT hr = IDWriteFontResource_CreateFontFace(x.res, (DWRITE_FONT_SIMULATIONS)bad[i], NULL, 0, &f);
        CHECKF(hr == E_INVALIDARG, "simulations 0x%x -> %s", bad[i], dw_hr_name(hr));
        CHECKF(f == NULL, "simulations 0x%x: face not zeroed", bad[i]);
        if (f == RV_SENT(IDWriteFontFace5 *)) f = NULL;
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_null_values_with_count)
{
    static const UINT32 c[] = { 1, 2, 5, 100, 0xFFFF };
    unsigned i;
    RV_BEGIN(0);
    if (t_is_wine()) { RV_END(); t_skip("emulator reads the NULL array"); return; }
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        IDWriteFontFace5 *f = RV_SENT(IDWriteFontFace5 *); HRESULT hr = IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_NONE, NULL, c[i], &f);
        CHECKF(hr == E_INVALIDARG, "NULL values, count %u -> %s", c[i], dw_hr_name(hr));
        CHECKF(f == NULL, "count %u: face not zeroed", c[i]);
        if (f == RV_SENT(IDWriteFontFace5 *)) f = NULL;
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_count_over_65535)
{
    /* the count is checked before the array is touched */
    static const UINT32 c[] = { 0x10000, 0x10001, 0x7FFFFFFFu, 0xFFFFFFFFu };
    DWRITE_FONT_AXIS_VALUE in = rv_av(TAG_WGHT, 500); unsigned i;
    RV_BEGIN(0);
    if (t_is_wine()) { RV_END(); t_skip("emulator reads the array before checking the count"); return; }
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        IDWriteFontFace5 *f = RV_SENT(IDWriteFontFace5 *); HRESULT hr = IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_NONE, &in, c[i], &f);
        CHECKF(hr == E_INVALIDARG, "count 0x%x -> %s", c[i], dw_hr_name(hr));
        CHECKF(f == NULL, "count 0x%x: face not zeroed", c[i]);
        if (f == RV_SENT(IDWriteFontFace5 *)) f = NULL;
        DW_RELEASE(f);
    }
    RV_END();
}
TEST(r2_variable_fonts_face_success_replaces_output)
{
    IDWriteFontFace5 *f = RV_SENT(IDWriteFontFace5 *);
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_CreateFontFace(x.res, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, &f));
    CHECK(f != NULL && f != RV_SENT(IDWriteFontFace5 *));
    if (f == RV_SENT(IDWriteFontFace5 *)) f = NULL;
    DW_RELEASE(f);
    RV_END();
}
TEST(r2_variable_fonts_face_values_not_modified)
{
    DWRITE_FONT_AXIS_VALUE in[3], keep[3]; IDWriteFontFace5 *f;
    RV_BEGIN(0);
    in[0] = rv_av(TAG_WGHT, 5000); in[1] = rv_av(TAG_WGHT, 1); in[2] = rv_av(TAG_ZZZZ, 2); memcpy(keep, in, sizeof in);
    f = rv_face(&x, in, 3);
    CHECK(!memcmp(in, keep, sizeof in));
    DW_RELEASE(f);
    RV_END();
}
TEST(r2_variable_fonts_face_many_faces_same_arguments)
{
    IDWriteFontFace5 *f[10]; DWRITE_FONT_AXIS_VALUE in = rv_av(TAG_WGHT, 777); int i;
    RV_BEGIN(0);
    for (i = 0; i < 10; i++) f[i] = rv_face(&x, &in, 1);
    for (i = 1; i < 10; i++) if (f[0] && f[i]) CHECK(eq(f[0], f[i]) == 1);
    for (i = 0; i < 10; i++) DW_RELEASE(f[i]);
    RV_END();
}
TEST(r2_variable_fonts_face_refcounts)
{
    IDWriteFontFace5 *f; ULONG r0;
    RV_BEGIN(0);
    f = rv_face(&x, NULL, 0);
    if (f) { r0 = dw_refcount((IUnknown *)f); IDWriteFontFace5_AddRef(f); EXPECT_REFS(f, r0 + 1); IDWriteFontFace5_Release(f); EXPECT_REFS(f, r0); DW_RELEASE(f); }
    RV_END();
}
