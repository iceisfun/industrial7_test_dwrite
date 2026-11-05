/* IDWriteFontResource axis surface on the synthetic variable fixture (rv.h): axis count, default values, ranges, attributes, names, HasVariations.
 *
 * Win10 dwrite.dll (IDWriteFontResource::GetFontAxisCount, GetDefaultFontAxisValues, GetFontAxisRanges
 *, GetFontAxisAttributes, built from the fvar table by reading the fvar table):
 *   - one axis record per fvar axis, in table order: tag, minimum, default, maximum (16.16 fixed converted to float);
 *   - the attribute byte is HIDDEN (2) when the fvar axis flag bit 0 (HIDDEN_AXIS) is set, plus VARIABLE (1) when maximum > minimum; an axis with
 *     minimum == maximum is therefore reported as DWRITE_FONT_AXIS_ATTRIBUTES_NONE;
 *   - the value buffers are zeroed over the passed count first and then filled over the same count, so only the exact axis count is passed here.
 */
#include "rv.h"

static void fill(void *p, size_t n) { memset(p, 0xCC, n); }

TEST(r2_variable_fonts_res_axis_count)
{
    RV_BEGIN(0);
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(x.res), RV_NALL);       /* HIDN and FIXD are fvar axes too */
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(x.res), RV_NALL);       /* stable */
    RV_END();
}
TEST(r2_variable_fonts_res_has_variations)
{
    RV_BEGIN(0);
    CHECK(IDWriteFontResource_HasVariations(x.res) == TRUE);
    RV_END();
}
TEST(r2_variable_fonts_res_has_variations_without_gvar)
{
    RV_BEGIN(RVF_NOGVAR);     /* fvar alone decides: bit 0 of the face flags is "fvar present" */
    CHECK(IDWriteFontResource_HasVariations(x.res) == TRUE);
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(x.res), RV_NALL);
    RV_END();
}
TEST(r2_variable_fonts_res_static_control_has_no_variations)
{
    RV_BEGIN(RVF_NOFVAR);
    CHECK(IDWriteFontResource_HasVariations(x.res) == FALSE);
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(x.res), 4);          /* only the standard wght, wdth, ital, slnt axes */
    RV_END();
}
TEST(r2_variable_fonts_res_default_values_per_axis)
{
    int i; DWRITE_FONT_AXIS_VALUE v[RV_NALL];
    RV_BEGIN(0);
    fill(v, sizeof v);
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, RV_NALL));
    for (i = 0; i < RV_NAXES; i++) {
        int k = rv_find_value(v, RV_NALL, RV_TAG(rv_axes[i].tag[0], rv_axes[i].tag[1], rv_axes[i].tag[2], rv_axes[i].tag[3]));
        CHECKF(k >= 0, "axis %s missing", rv_axes[i].tag);
        if (k >= 0) CHECKF(v[k].value == rv_axes[i].df, "axis %s default %g want %g", rv_axes[i].tag, v[k].value, rv_axes[i].df);
    }
    RV_END();
}
TEST(r2_variable_fonts_res_ranges_per_axis)
{
    int i; DWRITE_FONT_AXIS_RANGE r[RV_NALL];
    RV_BEGIN(0);
    fill(r, sizeof r);
    CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(x.res, r, RV_NALL));
    for (i = 0; i < RV_NAXES; i++) {
        UINT32 tag = RV_TAG(rv_axes[i].tag[0], rv_axes[i].tag[1], rv_axes[i].tag[2], rv_axes[i].tag[3]); int k;
        for (k = 0; k < RV_NALL && (UINT32)r[k].axisTag != tag; k++) ;
        CHECKF(k < RV_NALL, "axis %s missing", rv_axes[i].tag);
        if (k < RV_NALL) {
            CHECKF(r[k].minValue == rv_axes[i].mn, "axis %s min %g want %g", rv_axes[i].tag, r[k].minValue, rv_axes[i].mn);
            CHECKF(r[k].maxValue == rv_axes[i].mx, "axis %s max %g want %g", rv_axes[i].tag, r[k].maxValue, rv_axes[i].mx);
        }
    }
    RV_END();
}
TEST(r2_variable_fonts_res_defaults_and_ranges_same_order)
{
    DWRITE_FONT_AXIS_VALUE v[RV_NALL]; DWRITE_FONT_AXIS_RANGE r[RV_NALL]; int i;
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, RV_NALL));
    CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(x.res, r, RV_NALL));
    for (i = 0; i < RV_NALL; i++) {
        CHECKF(v[i].axisTag == r[i].axisTag, "index %d: value tag %08x range tag %08x", i, v[i].axisTag, r[i].axisTag);
        CHECKF(v[i].value >= r[i].minValue && v[i].value <= r[i].maxValue, "index %d default %g outside [%g, %g]", i, v[i].value, r[i].minValue, r[i].maxValue);
    }
    RV_END();
}
TEST(r2_variable_fonts_res_axis_order_is_fvar_order)
{
    DWRITE_FONT_AXIS_VALUE v[RV_NALL]; int i, same = 1;
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, RV_NALL));
    for (i = 0; i < RV_NAXES; i++) if ((UINT32)v[i].axisTag != RV_TAG(rv_axes[i].tag[0], rv_axes[i].tag[1], rv_axes[i].tag[2], rv_axes[i].tag[3])) same = 0;
    t_note("resource axis order %s the fvar order (wght wdth opsz HIDN FIXD)", same ? "matches" : "differs from");
    RV_END();
}
TEST(r2_variable_fonts_res_axis_tags_are_unique)
{
    DWRITE_FONT_AXIS_VALUE v[RV_NALL]; int i, j;
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, RV_NALL));
    for (i = 0; i < RV_NALL; i++) for (j = i + 1; j < RV_NALL; j++) CHECK(v[i].axisTag != v[j].axisTag);
    RV_END();
}
TEST(r2_variable_fonts_res_attributes_per_axis)
{
    /* VARIABLE when max > min; HIDDEN from fvar flag bit 0 */
    static const struct { UINT32 tag; DWRITE_FONT_AXIS_ATTRIBUTES want; } t[] = {
        { TAG_WGHT, DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE }, { TAG_WDTH, DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE }, { TAG_OPSZ, DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE },
        { TAG_HIDN, (DWRITE_FONT_AXIS_ATTRIBUTES)(DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE | DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN) },
        { TAG_FIXD, DWRITE_FONT_AXIS_ATTRIBUTES_NONE },
        { TAG_ITAL, DWRITE_FONT_AXIS_ATTRIBUTES_NONE }, { TAG_SLNT, DWRITE_FONT_AXIS_ATTRIBUTES_NONE } };     /* standard axes are fixed */
    unsigned i;
    RV_BEGIN(0);
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        int k = rv_axis_index(x.res, t[i].tag);
        CHECKF(k >= 0, "axis %08x missing", t[i].tag);
        if (k >= 0) { DWRITE_FONT_AXIS_ATTRIBUTES a = IDWriteFontResource_GetFontAxisAttributes(x.res, (UINT32)k); CHECKF(a == t[i].want, "axis %08x attributes %u want %u", t[i].tag, a, t[i].want); }
    }
    RV_END();
}
TEST(r2_variable_fonts_res_attribute_enum_values)
{
    EXPECT_EQ(DWRITE_FONT_AXIS_ATTRIBUTES_NONE, 0); EXPECT_EQ(DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE, 1); EXPECT_EQ(DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN, 2);
    EXPECT_EQ(DWRITE_AUTOMATIC_FONT_AXES_NONE, 0); EXPECT_EQ(DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE, 1);
    EXPECT_EQ(TAG_WGHT, RV_TAG('w', 'g', 'h', 't')); EXPECT_EQ(TAG_WDTH, RV_TAG('w', 'd', 't', 'h')); EXPECT_EQ(TAG_SLNT, RV_TAG('s', 'l', 'n', 't'));
    EXPECT_EQ(TAG_OPSZ, RV_TAG('o', 'p', 's', 'z')); EXPECT_EQ(TAG_ITAL, RV_TAG('i', 't', 'a', 'l'));
}
TEST(r2_variable_fonts_res_default_values_overwrite_whole_buffer)
{
    DWRITE_FONT_AXIS_VALUE v[RV_NALL]; int i, bad = 0;
    RV_BEGIN(0);
    fill(v, sizeof v);
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, RV_NALL));
    for (i = 0; i < RV_NALL; i++) if (v[i].axisTag == (DWRITE_FONT_AXIS_TAG)0xCCCCCCCCu) bad++;
    EXPECT_EQ(bad, 0);
    RV_END();
}
TEST(r2_variable_fonts_res_default_values_repeatable)
{
    DWRITE_FONT_AXIS_VALUE a[RV_NALL], b[RV_NALL];
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, a, RV_NALL));
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, b, RV_NALL));
    CHECK(!memcmp(a, b, sizeof a));
    RV_END();
}
TEST(r2_variable_fonts_res_get_font_file_identity)
{
    IDWriteFontFile *a = NULL, *b = NULL;
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_GetFontFile(x.res, &a));
    CHECK_HR_OK(IDWriteFontResource_GetFontFile(x.res, &b));
    CHECK(a == b);
    CHECK(a == x.file);        /* the resource keeps the file it was created from */
    EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(x.res), 0);
    DW_RELEASE(a); DW_RELEASE(b);
    RV_END();
}
TEST(r2_variable_fonts_res_axis_names_text)
{
    int i;
    RV_BEGIN(0);
    for (i = 0; i < RV_NAXES; i++) {
        IDWriteLocalizedStrings *s = NULL; char en[64] = "";
        int k = rv_axis_index(x.res, RV_TAG(rv_axes[i].tag[0], rv_axes[i].tag[1], rv_axes[i].tag[2], rv_axes[i].tag[3]));
        if (k < 0) { CHECKF(0, "axis %s missing", rv_axes[i].tag); continue; }
        CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, (UINT32)k, &s));
        CHECK(s != NULL);
        if (s && IDWriteLocalizedStrings_GetCount(s) > 0) dw_ls_en(s, en, sizeof en);
        CHECKF(!strcmp(en, rv_axes[i].name), "axis %s name \"%s\" want \"%s\"", rv_axes[i].tag, en, rv_axes[i].name);
        DW_RELEASE(s);
    }
    RV_END();
}
TEST(r2_variable_fonts_res_axis_names_out_of_range)
{
    IDWriteLocalizedStrings *s = RV_SENT(IDWriteLocalizedStrings *); HRESULT hr;
    RV_BEGIN(0);
    if (t_is_wine()) { RV_END(); t_skip("emulator does not validate the axis index"); return; }
    /* ArgumentException inside the method is mapped to E_INVALIDARG with the output zeroed first */
    hr = IDWriteFontResource_GetAxisNames(x.res, RV_NALL, &s);
    CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(s);
    s = RV_SENT(IDWriteLocalizedStrings *);
    hr = IDWriteFontResource_GetAxisNames(x.res, 0xFFFFFFFFu, &s);
    CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(s);
    RV_END();
}
TEST(r2_variable_fonts_res_named_instances_not_axis_values)
{
    /* STAT axis value names come from the STAT table; the fixture has none, so every axis reports zero value names */
    int i;
    RV_BEGIN(0);
    for (i = 0; i < RV_NALL; i++) {
        UINT32 n = IDWriteFontResource_GetAxisValueNameCount(x.res, (UINT32)i);
        CHECKF(n == 0, "axis %d value-name count %u", i, n);
    }
    RV_END();
}

/* ---- other axis lists ---------------------------------------------------------------------------------------------------------------- */
/* number of standard axes (wght wdth ital slnt) the fixture does not define itself */
static int std_missing(const rv_axis *ax, int n)
{
    static const UINT32 std[4] = { TAG_WGHT, TAG_WDTH, TAG_ITAL, TAG_SLNT }; int i, j, miss = 0;
    for (i = 0; i < 4; i++) { int have = 0; for (j = 0; j < n; j++) if (RV_TAG(ax[j].tag[0], ax[j].tag[1], ax[j].tag[2], ax[j].tag[3]) == std[i]) have = 1; if (!have) miss++; }
    return miss;
}
static void axis_list(const rv_axis *ax, int n)
{
    size_t sz; uint8_t *b = rv_build_axes(ax, n, &sz); rv x; DWRITE_FONT_AXIS_VALUE v[48]; DWRITE_FONT_AXIS_RANGE r[48]; int i;
    if (!b) { t_skip("font build failed"); return; }
    if (!rv_open(&x, b, sz)) { rv_close(&x); free(b); return; }
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(x.res), n + std_missing(ax, n));
    if (IDWriteFontResource_GetFontAxisCount(x.res) == (UINT32)(n + std_missing(ax, n))) {
        int tot = n + std_missing(ax, n);
        CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, tot));
        CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(x.res, r, tot));
        for (i = 0; i < n; i++) {
            int k = rv_find_value(v, tot, RV_TAG(ax[i].tag[0], ax[i].tag[1], ax[i].tag[2], ax[i].tag[3]));
            CHECKF(k >= 0, "axis %s missing", ax[i].tag);
            if (k >= 0) { CHECKF(v[k].value == ax[i].df, "axis %s default %g", ax[i].tag, v[k].value); CHECKF(r[k].minValue == ax[i].mn && r[k].maxValue == ax[i].mx, "axis %s range %g..%g", ax[i].tag, r[k].minValue, r[k].maxValue); }
        }
    }
    rv_close(&x); free(b);
}
TEST(r2_variable_fonts_res_single_axis)
{
    rv_axis a[1] = { { "wght", 1, 500, 1000, 0, 256, "Weight" } };
    axis_list(a, 1);
}
TEST(r2_variable_fonts_res_fractional_axis_values)
{
    rv_axis a[2] = { { "wght", 0.25f, 0.5f, 0.75f, 0, 256, "W" }, { "slnt", -15.5f, 0, 0.125f, 0, 257, "S" } };
    axis_list(a, 2);        /* 16.16 values survive the float round trip exactly */
}
TEST(r2_variable_fonts_res_negative_axis_range)
{
    rv_axis a[1] = { { "slnt", -90, -10, 0, 0, 256, "Slant" } };
    axis_list(a, 1);
}
TEST(r2_variable_fonts_res_custom_tags)
{
    rv_axis a[4] = { { "XOPQ", 27, 96, 175, 0, 256, "a" }, { "YTLC", 416, 500, 570, 0, 257, "b" }, { "GRAD", -1, 0, 1, 0, 258, "c" }, { "SHRP", 0, 0, 100, 0, 259, "d" } };
    axis_list(a, 4);
}
TEST(r2_variable_fonts_res_registered_five)
{
    rv_axis a[5] = { { "wght", 100, 400, 900, 0, 256, "a" }, { "wdth", 50, 100, 200, 0, 257, "b" }, { "slnt", -20, 0, 0, 0, 258, "c" }, { "ital", 0, 0, 1, 0, 259, "d" }, { "opsz", 6, 12, 72, 0, 260, "e" } };
    axis_list(a, 5);
}
TEST(r2_variable_fonts_res_twenty_axes)
{
    rv_axis a[20]; int i;
    for (i = 0; i < 20; i++) { snprintf((char *)a[i].tag, 5, "A%03d", i); a[i].mn = 0; a[i].df = 50; a[i].mx = 100; a[i].flags = 0; a[i].nameid = 300 + i; a[i].name = "x"; }
    axis_list(a, 20);
}
TEST(r2_variable_fonts_res_twenty_one_axes)
{
    /* the converter keeps 20 axis values inline; one more moves to the heap path */
    rv_axis a[21]; int i;
    for (i = 0; i < 21; i++) { snprintf((char *)a[i].tag, 5, "B%03d", i); a[i].mn = 0; a[i].df = 50; a[i].mx = 100; a[i].flags = 0; a[i].nameid = 300 + i; a[i].name = "x"; }
    axis_list(a, 21);
}
TEST(r2_variable_fonts_res_thirty_two_axes)
{
    rv_axis a[32]; int i;
    for (i = 0; i < 32; i++) { snprintf((char *)a[i].tag, 5, "C%03d", i); a[i].mn = (float)i; a[i].df = (float)i + 1; a[i].mx = (float)i + 2; a[i].flags = 0; a[i].nameid = 300 + i; a[i].name = "x"; }
    axis_list(a, 32);
}
TEST(r2_variable_fonts_res_all_axes_fixed)
{
    rv_axis a[2] = { { "wght", 400, 400, 400, 0, 256, "W" }, { "wdth", 100, 100, 100, 0, 257, "D" } };
    size_t sz; uint8_t *b = rv_build_axes(a, 2, &sz); rv x; int i;
    if (!b) { t_skip("font build failed"); return; }
    if (!rv_open(&x, b, sz)) { rv_close(&x); free(b); return; }
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(x.res), 4);
    for (i = 0; (UINT32)i < IDWriteFontResource_GetFontAxisCount(x.res); i++) EXPECT_EQ(IDWriteFontResource_GetFontAxisAttributes(x.res, (UINT32)i), DWRITE_FONT_AXIS_ATTRIBUTES_NONE);
    CHECK(IDWriteFontResource_HasVariations(x.res) == TRUE);      /* variations are about fvar presence, not about movable axes */
    rv_close(&x); free(b);
}
TEST(r2_variable_fonts_res_hidden_flag_only_bit0)
{
    /* fvar axis flags: only bit 0 (HIDDEN_AXIS) matters; other bits are reserved */
    rv_axis a[3] = { { "AAAA", 0, 1, 2, 0x0000, 256, "a" }, { "BBBB", 0, 1, 2, 0x0001, 257, "b" }, { "CCCC", 0, 1, 2, 0x0002, 258, "c" } };
    size_t sz; uint8_t *b = rv_build_axes(a, 3, &sz); rv x;
    if (!b) { t_skip("font build failed"); return; }
    if (!rv_open(&x, b, sz)) { rv_close(&x); free(b); return; }
    if (IDWriteFontResource_GetFontAxisCount(x.res) == 7) {
        int ia = rv_axis_index(x.res, RV_TAG('A', 'A', 'A', 'A')), ib = rv_axis_index(x.res, RV_TAG('B', 'B', 'B', 'B')), ic = rv_axis_index(x.res, RV_TAG('C', 'C', 'C', 'C'));
        if (ia >= 0) EXPECT_EQ(IDWriteFontResource_GetFontAxisAttributes(x.res, (UINT32)ia), 1);
        if (ib >= 0) EXPECT_EQ(IDWriteFontResource_GetFontAxisAttributes(x.res, (UINT32)ib), 3);
        if (ic >= 0) EXPECT_EQ(IDWriteFontResource_GetFontAxisAttributes(x.res, (UINT32)ic), 1);
    } else CHECK(0);
    rv_close(&x); free(b);
}
TEST(r2_variable_fonts_res_inverted_range_not_variable)
{
    /* fvar min > max: VARIABLE requires max > min, so the axis is NONE (the font itself is not validated for this) */
    rv_axis a[1] = { { "wght", 900, 900, 100, 0, 256, "W" } };
    size_t sz; uint8_t *b = rv_build_axes(a, 1, &sz); rv x;
    if (!b) { t_skip("font build failed"); return; }
    if (!rv_open(&x, b, sz)) { rv_close(&x); free(b); return; }
    if (IDWriteFontResource_GetFontAxisCount(x.res) == 4) {
        int k = rv_axis_index(x.res, TAG_WGHT);
        DWRITE_FONT_AXIS_ATTRIBUTES at = IDWriteFontResource_GetFontAxisAttributes(x.res, k < 0 ? 0 : (UINT32)k);
        t_note("inverted fvar range: attributes %u", at);
        CHECK((at & DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE) == 0);
    }
    rv_close(&x); free(b);
}

/* ---- resource lifetime ------------------------------------------------------------------------------------------------------------------ */
TEST(r2_variable_fonts_res_outlives_file_release)
{
    DWRITE_FONT_AXIS_VALUE v[RV_NALL];
    RV_BEGIN(0);
    DW_RELEASE(x.file);
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, RV_NALL));
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(x.res), RV_NALL);
    RV_END();
}
TEST(r2_variable_fonts_res_refcount_balance)
{
    ULONG r0;
    RV_BEGIN(0);
    r0 = dw_refcount((IUnknown *)x.res);
    IDWriteFontResource_AddRef(x.res); IDWriteFontResource_AddRef(x.res);
    EXPECT_REFS(x.res, r0 + 2);
    IDWriteFontResource_Release(x.res); IDWriteFontResource_Release(x.res);
    EXPECT_REFS(x.res, r0);
    RV_END();
}
TEST(r2_variable_fonts_res_qi_identity)
{
    IUnknown *u1 = NULL, *u2 = NULL;
    RV_BEGIN(0);
    CHECK_HR_OK(IDWriteFontResource_QueryInterface(x.res, &IID_IUnknown, (void **)&u1));
    CHECK_HR_OK(IDWriteFontResource_QueryInterface(x.res, &IID_IDWriteFontResource, (void **)&u2));
    CHECK(u1 == (IUnknown *)u2);
    CHECK(u1 == (IUnknown *)x.res);
    DW_RELEASE(u1); DW_RELEASE(u2);
    {
        void *p = RV_SENT(void *);
        CHECK_HR(IDWriteFontResource_QueryInterface(x.res, &IID_IDWriteFontFace, &p), E_NOINTERFACE);
        CHECK_OUT_NULL(p);
    }
    RV_END();
}
TEST(r2_variable_fonts_res_create_resource_null_file)
{
    IDWriteFontResource *r = RV_SENT(IDWriteFontResource *);
    RV_BEGIN(0);
    if (t_is_wine()) { RV_END(); t_skip("emulator faults on a NULL font file"); return; }
    CHECK_HR(IDWriteFactory6_CreateFontResource(x.f6, NULL, 0, &r), E_INVALIDARG);
    CHECK_OUT_NULL(r);
    RV_END();
}
TEST(r2_variable_fonts_res_many_resources_same_file)
{
    IDWriteFontResource *r[8]; int i;
    RV_BEGIN(0);
    for (i = 0; i < 8; i++) { r[i] = NULL; CHECK_HR_OK(IDWriteFactory6_CreateFontResource(x.f6, x.file, 0, &r[i])); }
    for (i = 0; i < 8; i++) if (r[i]) { EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(r[i]), RV_NALL); CHECK(IDWriteFontResource_HasVariations(r[i]) == TRUE); }
    for (i = 0; i < 8; i++) DW_RELEASE(r[i]);
    RV_END();
}

/* ---- STAT design axes feed the axis table (axis reading: STAT axes first, then fvar axes merged by tag, then the standard axes) ---------- */
TEST(r2_variable_fonts_res_stat_axis_not_in_fvar_is_listed)
{
    DWRITE_FONT_AXIS_VALUE v[16]; DWRITE_FONT_AXIS_RANGE r[16]; UINT32 n;
    RV_BEGIN(RVF_STAT);
    n = IDWriteFontResource_GetFontAxisCount(x.res);
    EXPECT_EQ(n, RV_NALL + 1);                 /* five fvar axes + STAT-only 'ZSTA' + standard ital and slnt */
    if (n == RV_NALL + 1) {
        int k;
        CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, n));
        CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(x.res, r, n));
        k = rv_find_value(v, n, TAG_ZSTA);
        CHECK(k >= 0);
        if (k >= 0) {
            /* no axis values: default 0, range collapsed to [0, 0] */
            CHECK(v[k].value == 0.0f); CHECK(r[k].minValue == 0.0f && r[k].maxValue == 0.0f);
            EXPECT_EQ(IDWriteFontResource_GetFontAxisAttributes(x.res, (UINT32)k), DWRITE_FONT_AXIS_ATTRIBUTES_NONE);
        }
    }
    RV_END();
}
TEST(r2_variable_fonts_res_stat_axis_does_not_disturb_fvar_axes)
{
    DWRITE_FONT_AXIS_VALUE v[16]; DWRITE_FONT_AXIS_RANGE r[16]; UINT32 n; int i;
    RV_BEGIN(RVF_STAT);
    n = IDWriteFontResource_GetFontAxisCount(x.res);
    if (n == RV_NALL + 1) {
        CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, n));
        CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(x.res, r, n));
        for (i = 0; i < RV_NAXES; i++) {
            UINT32 tag = RV_TAG(rv_axes[i].tag[0], rv_axes[i].tag[1], rv_axes[i].tag[2], rv_axes[i].tag[3]); int k = rv_find_value(v, n, tag);
            CHECKF(k >= 0, "axis %s missing", rv_axes[i].tag);
            if (k >= 0) CHECKF(v[k].value == rv_axes[i].df && r[k].minValue == rv_axes[i].mn && r[k].maxValue == rv_axes[i].mx, "axis %s changed by STAT merge", rv_axes[i].tag);
        }
    } else CHECK(0);
    RV_END();
}
TEST(r2_variable_fonts_res_axis_order_note)
{
    DWRITE_FONT_AXIS_VALUE v[16]; UINT32 n, i;
    RV_BEGIN(RVF_STAT);
    n = IDWriteFontResource_GetFontAxisCount(x.res);
    if (n > 0 && n <= 16 && SUCCEEDED(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, n)))
        for (i = 0; i < n; i++) t_note("axis[%u] = %c%c%c%c", i, v[i].axisTag & 0xFF, (v[i].axisTag >> 8) & 0xFF, (v[i].axisTag >> 16) & 0xFF, (v[i].axisTag >> 24) & 0xFF);
    RV_END();
}
TEST(r2_variable_fonts_res_stat_font_has_variations)
{
    RV_BEGIN(RVF_STAT);
    CHECK(IDWriteFontResource_HasVariations(x.res) == TRUE);
    RV_END();
}
