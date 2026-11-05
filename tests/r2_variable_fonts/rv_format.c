/* Font axis values on text formats and layouts: IDWriteFactory6::CreateTextFormat (axis overload), IDWriteTextFormat3::{SetFontAxisValues,
 * GetFontAxisValueCount, GetFontAxisValues, GetAutomaticFontAxes, SetAutomaticFontAxes}, IDWriteTextLayout4 ranged versions.
 *
 * Win10 dwrite.dll (DWriteFactory::CreateTextFormat(axis) !0xd84f0, DWriteTextFormat ctor !0xe4ba4, DWriteTextFormat::SetFontAxisValues !0xe4db0,
 * GetFontAxisValues !0xe4ce0, GetFontAxisValueCount !0x929a0, Set/GetAutomaticFontAxes !0x92ad0/!0x92980; DWriteTextLayout::SetFontAxisValues
 * !0xdf140 (whole) / !0xdf1b0 (range), GetFontAxisValues !0xde100, GetFontAxisValueCount !0xde0a0 / !0x92600, SetAutomaticFontAxes !0xdf030):
 *   - the axis CreateTextFormat zeroes *format first and returns E_INVALIDARG for a NULL family, a NULL locale, a count > 0xFFFF, NULL values with a
 *     non-zero count, and a size that is <= 0 or NaN; the format it makes has weight 400, style normal, stretch normal;
 *   - the axis values of a format are stored exactly as supplied (no clamping, no canonicalisation, duplicates and unknown tags kept); setting replaces the
 *     previous list, count 0 clears it; invalid arguments (count > 0xFFFF, NULL with count) are E_INVALIDARG and leave the list unchanged;
 *   - GetFontAxisValues zeroes `count` entries first and then copies the stored values;
 *   - the format stores the automatic axes value without validation, default NONE; the layout rejects values with bits other than bit 0 with E_INVALIDARG;
 *   - the ranged layout SetFontAxisValues rejects count > 0xFFFF, NULL with count, and a range whose start + length overflows 32 bits.
 */
#include "rv.h"

static IDWriteFactory6 *f6(void) { return (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6); }
#define NEED6() IDWriteFactory6 *F = f6(); if (!F) { t_skip("IDWriteFactory6 unsupported"); return; }

static IDWriteTextFormat3 *mkfmt(IDWriteFactory6 *F, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, float size)
{
    IDWriteTextFormat3 *t = NULL;
    HRESULT hr = IDWriteFactory6_CreateTextFormat(F, L"Segoe UI", NULL, v, n, size, L"en-us", &t);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? t : NULL;
}
static IDWriteTextLayout4 *mklayout(IDWriteFactory6 *F, IDWriteTextFormat3 *t, const WCHAR *text)
{
    IDWriteTextLayout *l = NULL; IDWriteTextLayout4 *l4 = NULL;
    HRESULT hr = IDWriteFactory6_CreateTextLayout(F, text, (UINT32)wcslen(text), (IDWriteTextFormat *)t, 1000.0f, 1000.0f, &l);
    CHECK_HR(hr, S_OK);
    if (l) { CHECK_HR_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4)); DW_RELEASE(l); }
    return l4;
}

/* ---- factory overload -------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_fmt_create_without_values)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 16.0f);
    if (t) { EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 0); CHECK(IDWriteTextFormat3_GetFontSize(t) == 16.0f); DW_RELEASE(t); }
}
TEST(r2_variable_fonts_fmt_create_stores_values_verbatim)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a[4], g[4];
    a[0] = rv_av(TAG_WGHT, 5000); a[1] = rv_av(TAG_WGHT, -3); a[2] = rv_av(TAG_ZZZZ, 1.25f); a[3] = rv_av(TAG_OPSZ, 0);        /* out of range, duplicate, unknown tag */
    IDWriteTextFormat3 *t = mkfmt(F, a, 4, 12.0f);
    if (t) {
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 4);
        memset(g, 0xCC, sizeof g);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, g, 4));
        CHECK(!memcmp(a, g, sizeof a));
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_create_copies_values)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a[2], g[2];
    a[0] = rv_av(TAG_WGHT, 700); a[1] = rv_av(TAG_WDTH, 80);
    IDWriteTextFormat3 *t = mkfmt(F, a, 2, 12.0f);
    a[0].value = 1; a[1].axisTag = TAG_ZZZZ;          /* the caller's array is not referenced afterwards */
    if (t) { CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, g, 2)); CHECK(g[0].axisTag == TAG_WGHT && g[0].value == 700.0f && g[1].axisTag == TAG_WDTH && g[1].value == 80.0f); DW_RELEASE(t); }
}
TEST(r2_variable_fonts_fmt_create_default_weight_stretch_style)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 700);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 1, 20.0f);
    if (t) {
        CHECK(IDWriteTextFormat3_GetFontWeight(t) == DWRITE_FONT_WEIGHT_NORMAL); CHECK(IDWriteTextFormat3_GetFontStretch(t) == DWRITE_FONT_STRETCH_NORMAL);
        CHECK(IDWriteTextFormat3_GetFontStyle(t) == DWRITE_FONT_STYLE_NORMAL);       /* the axis values do not feed the legacy properties */
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_create_family_locale_size)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 33.5f);
    if (t) {
        WCHAR b[64];
        CHECK(IDWriteTextFormat3_GetFontSize(t) == 33.5f);
        EXPECT_EQ(IDWriteTextFormat3_GetFontFamilyNameLength(t), 8);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontFamilyName(t, b, 64)); EXPECT_WSTR(b, L"Segoe UI");
        CHECK_HR_OK(IDWriteTextFormat3_GetLocaleName(t, b, 64)); EXPECT_WSTR(b, L"en-us");
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_create_invalid_arguments)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 500); IDWriteTextFormat3 *t; HRESULT hr;
    if (t_is_wine()) { t_skip("emulator does not validate"); return; }
#define BAD(expr, what) do { t = RV_SENT(IDWriteTextFormat3 *); hr = (expr); CHECKF(hr == E_INVALIDARG, what " -> %s", dw_hr_name(hr)); CHECKF(t == NULL, what ": not zeroed"); if (t == RV_SENT(IDWriteTextFormat3 *)) t = NULL; DW_RELEASE(t); } while (0)
    BAD(IDWriteFactory6_CreateTextFormat(F, NULL, NULL, &a, 1, 12.0f, L"en-us", &t), "NULL family");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, &a, 1, 12.0f, NULL, &t), "NULL locale");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, NULL, 1, 12.0f, L"en-us", &t), "NULL values with count 1");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, &a, 0x10000, 12.0f, L"en-us", &t), "count 0x10000");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, &a, 0xFFFFFFFFu, 12.0f, L"en-us", &t), "count 0xFFFFFFFF");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, &a, 1, 0.0f, L"en-us", &t), "size 0");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, &a, 1, -1.0f, L"en-us", &t), "size -1");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, &a, 1, -0.0f, L"en-us", &t), "size -0");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, &a, 1, NAN, L"en-us", &t), "size NaN");
    BAD(IDWriteFactory6_CreateTextFormat(F, L"Arial", NULL, &a, 1, -INFINITY, L"en-us", &t), "size -inf");
#undef BAD
}
TEST(r2_variable_fonts_fmt_create_zero_count_ignores_pointer)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 500);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 0, 12.0f);
    if (t) { EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 0); DW_RELEASE(t); }
}
TEST(r2_variable_fonts_fmt_create_many_values)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a[300], g[300]; int i;
    for (i = 0; i < 300; i++) a[i] = rv_av(RV_TAG('f', '0' + i / 100, '0' + (i / 10) % 10, '0' + i % 10), (float)i);
    IDWriteTextFormat3 *t = mkfmt(F, a, 300, 12.0f);
    if (t) { EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 300); CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, g, 300)); CHECK(!memcmp(a, g, sizeof a)); DW_RELEASE(t); }
}
TEST(r2_variable_fonts_fmt_create_max_count_accepted)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE *a = (DWRITE_FONT_AXIS_VALUE *)calloc(0xFFFF, sizeof *a); IDWriteTextFormat3 *t; UINT32 i;
    if (!a) { t_skip("allocation failed"); return; }
    for (i = 0; i < 0xFFFF; i++) { a[i].axisTag = (DWRITE_FONT_AXIS_TAG)(0x41000000u | i); a[i].value = (float)(i & 255); }
    t = mkfmt(F, a, 0xFFFF, 12.0f);
    if (t) { EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 0xFFFF); DW_RELEASE(t); }
    free(a);
}
TEST(r2_variable_fonts_fmt_legacy_create_has_no_values)
{
    NEED6();
    IDWriteTextFormat *t = NULL; IDWriteTextFormat3 *t3 = NULL;
    CHECK_HR_OK(F->lpVtbl->CreateTextFormat(F, L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &t));
    if (t && SUCCEEDED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat3, (void **)&t3)) && t3) {
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 0);
        CHECK(IDWriteTextFormat3_GetAutomaticFontAxes(t3) == DWRITE_AUTOMATIC_FONT_AXES_NONE);
    } else CHECK(0);
    DW_RELEASE(t3); DW_RELEASE(t);
}

/* ---- SetFontAxisValues on a format -------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_fmt_set_replaces_list)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a[2], b[1], g[4];
    a[0] = rv_av(TAG_WGHT, 700); a[1] = rv_av(TAG_WDTH, 80); b[0] = rv_av(TAG_OPSZ, 20);
    IDWriteTextFormat3 *t = mkfmt(F, a, 2, 12.0f);
    if (t) {
        CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t, b, 1));
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 1);
        memset(g, 0, sizeof g); CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, g, 4));
        CHECK(g[0].axisTag == TAG_OPSZ && g[0].value == 20.0f && g[1].axisTag == 0);
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_set_zero_count_clears)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 700);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 1, 12.0f);
    if (t) {
        CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t, NULL, 0));
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 0);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, NULL, 0));
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_set_invalid_leaves_list)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 700), b = rv_av(TAG_WDTH, 50);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 1, 12.0f);
    if (t) {
        HRESULT hr; DWRITE_FONT_AXIS_VALUE g;
        if (t_is_wine()) { DW_RELEASE(t); t_skip("emulator does not validate"); return; }
        hr = IDWriteTextFormat3_SetFontAxisValues(t, NULL, 1); CHECKF(hr == E_INVALIDARG, "NULL, 1 -> %s", dw_hr_name(hr));
        hr = IDWriteTextFormat3_SetFontAxisValues(t, &b, 0x10000); CHECKF(hr == E_INVALIDARG, "0x10000 -> %s", dw_hr_name(hr));
        hr = IDWriteTextFormat3_SetFontAxisValues(t, &b, 0xFFFFFFFFu); CHECKF(hr == E_INVALIDARG, "0xFFFFFFFF -> %s", dw_hr_name(hr));
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 1);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, &g, 1)); CHECK(g.axisTag == TAG_WGHT && g.value == 700.0f);
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_set_values_stored_without_validation)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a[3], g[3]; IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 12.0f);
    a[0] = rv_av(0, 1); a[1] = rv_av(0xFFFFFFFFu, -1e30f); a[2] = rv_av(TAG_WGHT, NAN);
    if (t) {
        CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t, a, 3));
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 3);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, g, 3));
        CHECK(g[0].axisTag == 0 && g[0].value == 1.0f && g[1].axisTag == (DWRITE_FONT_AXIS_TAG)0xFFFFFFFFu && g[1].value == -1e30f && g[2].axisTag == TAG_WGHT && g[2].value != g[2].value);
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_get_values_zero_fills_buffer)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a[2], g[6]; int i;
    a[0] = rv_av(TAG_WGHT, 700); a[1] = rv_av(TAG_WDTH, 80);
    IDWriteTextFormat3 *t = mkfmt(F, a, 2, 12.0f);
    if (t) {
        memset(g, 0xCC, sizeof g);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, g, 6));
        CHECK(g[0].axisTag == TAG_WGHT && g[1].axisTag == TAG_WDTH);
        for (i = 2; i < 6; i++) CHECKF(g[i].axisTag == 0 && g[i].value == 0.0f, "tail entry %d not zero", i);
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_get_values_empty_list_zeroes_buffer)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE g[3]; int i;
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 12.0f);
    if (t) { memset(g, 0xCC, sizeof g); CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, g, 3)); for (i = 0; i < 3; i++) CHECK(g[i].axisTag == 0 && g[i].value == 0.0f); DW_RELEASE(t); }
}
TEST(r2_variable_fonts_fmt_values_independent_between_formats)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 700), b = rv_av(TAG_WGHT, 300);
    IDWriteTextFormat3 *t1 = mkfmt(F, &a, 1, 12.0f), *t2 = mkfmt(F, &a, 1, 12.0f);
    if (t1 && t2) {
        DWRITE_FONT_AXIS_VALUE g;
        CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t2, &b, 1));
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t1, &g, 1)); CHECK(g.value == 700.0f);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t2, &g, 1)); CHECK(g.value == 300.0f);
    }
    DW_RELEASE(t1); DW_RELEASE(t2);
}
TEST(r2_variable_fonts_fmt_legacy_setters_keep_values)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 700);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 1, 12.0f);
    if (t) {
        CHECK_HR_OK(IDWriteTextFormat3_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_CENTER));
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 1);
        DW_RELEASE(t);
    }
}

/* ---- automatic font axes on a format ------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_fmt_automatic_axes_default_none)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 12.0f);
    if (t) { CHECK(IDWriteTextFormat3_GetAutomaticFontAxes(t) == DWRITE_AUTOMATIC_FONT_AXES_NONE); DW_RELEASE(t); }
}
TEST(r2_variable_fonts_fmt_automatic_axes_round_trip)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 12.0f);
    if (t) {
        CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
        CHECK(IDWriteTextFormat3_GetAutomaticFontAxes(t) == DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
        CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t, DWRITE_AUTOMATIC_FONT_AXES_NONE));
        CHECK(IDWriteTextFormat3_GetAutomaticFontAxes(t) == DWRITE_AUTOMATIC_FONT_AXES_NONE);
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_automatic_axes_not_validated_on_format)
{
    NEED6();
    static const UINT32 v[] = { 2, 3, 0x10, 0x80000000u, 0xFFFFFFFFu }; unsigned i;
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 12.0f);
    if (t_is_wine()) { DW_RELEASE(t); t_skip("emulator validates the value"); return; }
    if (t) {
        for (i = 0; i < sizeof v / sizeof v[0]; i++) {
            HRESULT hr = IDWriteTextFormat3_SetAutomaticFontAxes(t, (DWRITE_AUTOMATIC_FONT_AXES)v[i]);
            CHECKF(hr == S_OK, "0x%x -> %s", v[i], dw_hr_name(hr));         /* DWriteTextFormat::SetAutomaticFontAxes stores the value as is */
            CHECKF((UINT32)IDWriteTextFormat3_GetAutomaticFontAxes(t) == v[i], "0x%x read back as 0x%x", v[i], IDWriteTextFormat3_GetAutomaticFontAxes(t));
        }
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_automatic_axes_independent_of_values)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_OPSZ, 20);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 1, 12.0f);
    if (t) {
        CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 1);
        CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t, NULL, 0));
        CHECK(IDWriteTextFormat3_GetAutomaticFontAxes(t) == DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
        DW_RELEASE(t);
    }
}

/* ---- interface plumbing -------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_fmt_qi_chain)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 12.0f);
    if (t) {
        static const struct { const IID *iid; const char *n; } r[] = { { &IID_IDWriteTextFormat, "IDWriteTextFormat" }, { &IID_IDWriteTextFormat1, "IDWriteTextFormat1" }, { &IID_IDWriteTextFormat2, "IDWriteTextFormat2" }, { &IID_IDWriteTextFormat3, "IDWriteTextFormat3" } };
        unsigned i;
        for (i = 0; i < 4; i++) { IUnknown *u = NULL; HRESULT hr = IDWriteTextFormat3_QueryInterface(t, r[i].iid, (void **)&u); CHECKF(hr == S_OK, "%s -> %s", r[i].n, dw_hr_name(hr)); if (u) CHECK(dw_same_identity(u, (IUnknown *)t)); DW_RELEASE(u); }
        { void *p = RV_SENT(void *); CHECK_HR(IDWriteTextFormat3_QueryInterface(t, &IID_IDWriteTextLayout, &p), E_NOINTERFACE); CHECK_OUT_NULL(p); }
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_fmt_refcounts)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 12.0f);
    if (t) { ULONG r0 = dw_refcount((IUnknown *)t); IDWriteTextFormat3_AddRef(t); EXPECT_REFS(t, r0 + 1); IDWriteTextFormat3_Release(t); EXPECT_REFS(t, r0); DW_RELEASE(t); }
}

/* ---- layouts ---------------------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_layout_inherits_format_values)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a[2], g[2]; IDWriteTextLayout4 *l; IDWriteTextFormat3 *t;
    a[0] = rv_av(TAG_WGHT, 650); a[1] = rv_av(TAG_OPSZ, 18);
    t = mkfmt(F, a, 2, 14.0f);
    l = t ? mklayout(F, t, L"Hello axis world") : NULL;
    if (l) {
        DWRITE_TEXT_RANGE rg = { 99, 99 };
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 0), 2);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 15), 2);
        CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 3, g, 2, &rg));
        CHECK(!memcmp(a, g, sizeof a));
        EXPECT_EQ(rg.startPosition, 0); EXPECT_EQ(rg.length, 0xFFFFFFFFu);   /* a run that reaches the end of the text reports an open-ended length */
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_range_set_splits_runs)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"0123456789") : NULL;
    if (l) {
        DWRITE_FONT_AXIS_VALUE a[2], g[4]; DWRITE_TEXT_RANGE r = { 2, 3 }, rg;
        a[0] = rv_av(TAG_WGHT, 800); a[1] = rv_av(TAG_WDTH, 90);
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, a, 2, r));
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 0), 0);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 1), 0);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 2), 2);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 4), 2);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 5), 0);
        memset(&rg, 0xCC, sizeof rg); CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 3, g, 4, &rg));
        EXPECT_EQ(rg.startPosition, 2); EXPECT_EQ(rg.length, 3);
        CHECK(g[0].axisTag == TAG_WGHT && g[0].value == 800.0f && g[1].axisTag == TAG_WDTH && g[1].value == 90.0f && g[2].axisTag == 0 && g[3].axisTag == 0);
        CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 0, g, 4, &rg)); EXPECT_EQ(rg.startPosition, 0); EXPECT_EQ(rg.length, 2);
        CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 9, g, 4, &rg)); EXPECT_EQ(rg.startPosition, 5); EXPECT_EQ(rg.length, 0xFFFFFFFFu - 5u);   /* last run: open-ended length */
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_get_values_range_may_be_null)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"abc") : NULL;
    if (l) { DWRITE_FONT_AXIS_VALUE g[2]; CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 0, g, 2, NULL)); DW_RELEASE(l); }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_get_values_zeroes_buffer_and_range)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"abcdef") : NULL;
    if (l) {
        DWRITE_FONT_AXIS_VALUE g[3]; DWRITE_TEXT_RANGE rg = { 7, 7 }; int i;
        memset(g, 0xCC, sizeof g);
        CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 2, g, 3, &rg));
        for (i = 0; i < 3; i++) CHECK(g[i].axisTag == 0 && g[i].value == 0.0f);
        EXPECT_EQ(rg.startPosition, 0); EXPECT_EQ(rg.length, 0xFFFFFFFFu);   /* single run reaching the end: open-ended length */
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_range_set_replaces_overlapping)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"0123456789") : NULL;
    if (l) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 800), b = rv_av(TAG_WDTH, 90), g[2]; DWRITE_TEXT_RANGE r1 = { 0, 6 }, r2 = { 4, 6 }, rg;
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, r1));
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, &b, 1, r2));
        CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 1, g, 2, &rg)); CHECK(g[0].axisTag == TAG_WGHT); EXPECT_EQ(rg.startPosition, 0); EXPECT_EQ(rg.length, 4);
        CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 5, g, 2, &rg)); CHECK(g[0].axisTag == TAG_WDTH); EXPECT_EQ(rg.startPosition, 4); EXPECT_EQ(rg.length, 6);        /* the later set replaces the whole value list there */
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 5), 1);
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_range_set_zero_count_clears_range)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 800);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 1, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"0123456789") : NULL;
    if (l) {
        DWRITE_TEXT_RANGE r = { 3, 2 };
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, NULL, 0, r));
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 3), 0);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 2), 1);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 5), 1);
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_range_set_invalid_arguments)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"0123456789") : NULL;
    if (t_is_wine()) { DW_RELEASE(l); DW_RELEASE(t); t_skip("emulator does not validate"); return; }
    if (l) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 800); DWRITE_TEXT_RANGE r = { 0, 4 }, o1 = { 0xFFFFFFFFu, 1 }, o2 = { 0x80000000u, 0x80000000u }, o3 = { 5, 0xFFFFFFFFu }; HRESULT hr;
        hr = IDWriteTextLayout4_SetFontAxisValues(l, NULL, 1, r); CHECKF(hr == E_INVALIDARG, "NULL values with count 1 -> %s", dw_hr_name(hr));
        hr = IDWriteTextLayout4_SetFontAxisValues(l, &a, 0x10000, r); CHECKF(hr == E_INVALIDARG, "count 0x10000 -> %s", dw_hr_name(hr));
        hr = IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, o1); CHECKF(hr == E_INVALIDARG, "range 0xFFFFFFFF+1 -> %s", dw_hr_name(hr));
        hr = IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, o2); CHECKF(hr == E_INVALIDARG, "range 0x80000000+0x80000000 -> %s", dw_hr_name(hr));
        hr = IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, o3); CHECKF(hr == E_INVALIDARG, "range 5+0xFFFFFFFF -> %s", dw_hr_name(hr));
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 0), 0);          /* nothing was applied */
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_range_set_beyond_text_is_ok)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"0123456789") : NULL;
    if (l) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 800); DWRITE_TEXT_RANGE r1 = { 8, 100 }, r2 = { 50, 10 }, r3 = { 0, 0 }, r4 = { 4, 0 };
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, r1));
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, r2));
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, r3));
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, r4));          /* empty ranges change nothing */
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 9), 1);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 7), 0);
        EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 4), 0);
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_whole_set_through_format3)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"0123456789") : NULL;
    if (l) {
        IDWriteTextFormat3 *lf = NULL; DWRITE_FONT_AXIS_VALUE a[2], g[2];
        a[0] = rv_av(TAG_WGHT, 800); a[1] = rv_av(TAG_OPSZ, 30);
        if (SUCCEEDED(IDWriteTextLayout4_QueryInterface(l, &IID_IDWriteTextFormat3, (void **)&lf)) && lf) {
            CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(lf, a, 2));
            EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(lf), 2);
            CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(lf, g, 2)); CHECK(!memcmp(a, g, sizeof a));
            EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 0), 2); EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 9), 2);
            { HRESULT hr; if (!t_is_wine()) { hr = IDWriteTextFormat3_SetFontAxisValues(lf, NULL, 3); CHECKF(hr == E_INVALIDARG, "NULL, 3 -> %s", dw_hr_name(hr)); EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(lf), 2); } }
            DW_RELEASE(lf);
        } else CHECK(0);
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_automatic_axes_validation)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"abc") : NULL;
    if (l) {
        static const UINT32 bad[] = { 2, 3, 4, 0x10, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu }; unsigned i; HRESULT hr;
        CHECK(IDWriteTextLayout4_GetAutomaticFontAxes(l) == DWRITE_AUTOMATIC_FONT_AXES_NONE);
        CHECK_HR_OK(IDWriteTextLayout4_SetAutomaticFontAxes(l, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
        CHECK(IDWriteTextLayout4_GetAutomaticFontAxes(l) == DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
        if (!t_is_wine()) for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            hr = IDWriteTextLayout4_SetAutomaticFontAxes(l, (DWRITE_AUTOMATIC_FONT_AXES)bad[i]);
            CHECKF(hr == E_INVALIDARG, "0x%x -> %s", bad[i], dw_hr_name(hr));
            CHECK(IDWriteTextLayout4_GetAutomaticFontAxes(l) == DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);       /* unchanged by the rejected call */
        }
        CHECK_HR_OK(IDWriteTextLayout4_SetAutomaticFontAxes(l, DWRITE_AUTOMATIC_FONT_AXES_NONE));
        CHECK(IDWriteTextLayout4_GetAutomaticFontAxes(l) == DWRITE_AUTOMATIC_FONT_AXES_NONE);
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_automatic_axes_via_format3_view)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"abc") : NULL;
    if (l) {
        IDWriteTextFormat3 *lf = NULL;
        if (SUCCEEDED(IDWriteTextLayout4_QueryInterface(l, &IID_IDWriteTextFormat3, (void **)&lf)) && lf) {
            CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(lf, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
            CHECK(IDWriteTextLayout4_GetAutomaticFontAxes(l) == DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);      /* one state behind both views */
            if (!t_is_wine()) { HRESULT hr = IDWriteTextFormat3_SetAutomaticFontAxes(lf, (DWRITE_AUTOMATIC_FONT_AXES)2); CHECKF(hr == E_INVALIDARG, "2 -> %s", dw_hr_name(hr)); }
            DW_RELEASE(lf);
        } else CHECK(0);
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_automatic_axes_from_format_note)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f);
    if (t) {
        IDWriteTextLayout4 *l;
        CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
        l = mklayout(F, t, L"abc");
        if (l) { t_note("layout made from a format with automatic optical size: automatic axes = %u", (unsigned)IDWriteTextLayout4_GetAutomaticFontAxes(l)); DW_RELEASE(l); }
        DW_RELEASE(t);
    }
}
TEST(r2_variable_fonts_layout_values_survive_format_release)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 650);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 1, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"abc") : NULL;
    DW_RELEASE(t);
    if (l) { DWRITE_FONT_AXIS_VALUE g; EXPECT_EQ(IDWriteTextLayout4_GetFontAxisValueCount(l, 0), 1); CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, 0, &g, 1, NULL)); CHECK(g.axisTag == TAG_WGHT && g.value == 650.0f); DW_RELEASE(l); }
}
TEST(r2_variable_fonts_layout_set_does_not_touch_format)
{
    NEED6();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 650), b = rv_av(TAG_WGHT, 200);
    IDWriteTextFormat3 *t = mkfmt(F, &a, 1, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"abc") : NULL;
    if (l) {
        DWRITE_TEXT_RANGE r = { 0, 3 }; DWRITE_FONT_AXIS_VALUE g;
        CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, &b, 1, r));
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, &g, 1)); CHECK(g.value == 650.0f);
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_many_ranges)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"0123456789ABCDEFGHIJ") : NULL;
    if (l) {
        UINT32 i;
        for (i = 0; i < 20; i++) { DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 100.0f + 40.0f * i); DWRITE_TEXT_RANGE r = { i, 1 }; CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l, &a, 1, r)); }
        for (i = 0; i < 20; i++) { DWRITE_FONT_AXIS_VALUE g; DWRITE_TEXT_RANGE rg; CHECK_HR_OK(IDWriteTextLayout4_GetFontAxisValues(l, i, &g, 1, &rg)); CHECKF(g.value == 100.0f + 40.0f * i && rg.startPosition == i && rg.length == 1, "position %u", i); }
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
TEST(r2_variable_fonts_layout_qi_layout4)
{
    NEED6();
    IDWriteTextFormat3 *t = mkfmt(F, NULL, 0, 14.0f); IDWriteTextLayout4 *l = t ? mklayout(F, t, L"abc") : NULL;
    if (l) {
        static const struct { const IID *iid; const char *n; } r[] = { { &IID_IDWriteTextLayout, "IDWriteTextLayout" }, { &IID_IDWriteTextLayout1, "IDWriteTextLayout1" }, { &IID_IDWriteTextLayout2, "IDWriteTextLayout2" },
            { &IID_IDWriteTextLayout3, "IDWriteTextLayout3" }, { &IID_IDWriteTextLayout4, "IDWriteTextLayout4" }, { &IID_IDWriteTextFormat1, "IDWriteTextFormat1" }, { &IID_IDWriteTextFormat2, "IDWriteTextFormat2" }, { &IID_IDWriteTextFormat3, "IDWriteTextFormat3" } };
        unsigned i;
        for (i = 0; i < sizeof r / sizeof r[0]; i++) { IUnknown *u = NULL; HRESULT hr = IDWriteTextLayout4_QueryInterface(l, r[i].iid, (void **)&u); CHECKF(hr == S_OK, "%s -> %s", r[i].n, dw_hr_name(hr)); if (u) CHECK(dw_same_identity(u, (IUnknown *)l)); DW_RELEASE(u); }
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
}
