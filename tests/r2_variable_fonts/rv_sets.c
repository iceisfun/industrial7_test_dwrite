/* Font sets built from the synthetic variable fixture: IDWriteFontSetBuilder2::AddFont (axis values and ranges), IDWriteFontSetBuilder1::AddFontFile
 * (named instances), IDWriteFontSet1::{GetFilteredFonts(axis ranges), GetFirstFontResources, GetFontAxisRanges, CreateFontResource, CreateFontFace,
 * GetFontFaceReference}.
 *
 * Win10 dwrite.dll (DWriteFontSetBuilder::AddFont !0xe36d0): E_INVALIDARG for a NULL file, simulations & ~3, more than 0xFFFF axis values, NULL
 * axis values / ranges / properties together with a non-zero count; otherwise S_OK (the font is not read at this point).  The documented set filters
 * ("endpoint-inclusive", "the subset is always equal to or less than the original set", an empty subset is not an error) are checked as relations.
 */
#include "rv.h"

typedef struct sb { rv x; IDWriteFontSetBuilder2 *b2; IDWriteFontSetBuilder1 *b1; IDWriteFontSetBuilder *b0; int ok; } sb;
static int sb_open(sb *s, unsigned flags, uint8_t **keep)
{
    size_t n; HRESULT hr;
    memset(s, 0, sizeof *s);
    *keep = rv_build(flags, &n);
    if (!*keep) { t_skip("font build failed"); return 0; }
    if (!rv_open(&s->x, *keep, n)) return 0;
    hr = IDWriteFactory6_CreateFontSetBuilder(s->x.f6, &s->b2);
    if (FAILED(hr) || !s->b2) { t_skip("CreateFontSetBuilder -> %s", dw_hr_name(hr)); return 0; }
    IDWriteFontSetBuilder2_QueryInterface(s->b2, &IID_IDWriteFontSetBuilder1, (void **)&s->b1);
    IDWriteFontSetBuilder2_QueryInterface(s->b2, &IID_IDWriteFontSetBuilder, (void **)&s->b0);
    s->ok = 1;
    return 1;
}
static void sb_close(sb *s, uint8_t *keep) { DW_RELEASE(s->b0); DW_RELEASE(s->b1); DW_RELEASE(s->b2); rv_close(&s->x); free(keep); }
#define SB_BEGIN() sb S; uint8_t *keep_ = NULL; if (!sb_open(&S, RVF_HVAR, &keep_)) { sb_close(&S, keep_); return; }
#define SB_END() sb_close(&S, keep_)

static IDWriteFontSet1 *make_set(sb *s)
{
    IDWriteFontSet *a = NULL; IDWriteFontSet1 *b = NULL;
    if (SUCCEEDED(IDWriteFontSetBuilder2_CreateFontSet(s->b2, &a)) && a) IDWriteFontSet_QueryInterface(a, &IID_IDWriteFontSet1, (void **)&b);
    DW_RELEASE(a);
    return b;
}
static IDWriteFontSet1 *file_set(sb *s)
{
    HRESULT hr;
    if (!s->b1) return NULL;
    hr = IDWriteFontSetBuilder1_AddFontFile(s->b1, s->x.file);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? make_set(s) : NULL;
}

/* ---- builder plumbing --------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_sets_builder2_interfaces)
{
    SB_BEGIN();
    CHECK(S.b1 != NULL); CHECK(S.b0 != NULL);
    if (S.b0) CHECK(dw_same_identity((IUnknown *)S.b0, (IUnknown *)S.b2));
    SB_END();
}
TEST(r2_variable_fonts_sets_empty_builder_gives_empty_set)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = make_set(&S);
    if (s) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(s), 0); DW_RELEASE(s); } else CHECK(0);
    SB_END();
}

/* ---- AddFont ----------------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_sets_add_font_invalid_arguments)
{
    SB_BEGIN();
    DWRITE_FONT_AXIS_VALUE v = rv_av(TAG_WGHT, 500); DWRITE_FONT_AXIS_RANGE r = { TAG_WGHT, 100, 900 }; DWRITE_FONT_PROPERTY p = { DWRITE_FONT_PROPERTY_ID_FULL_NAME, L"x", L"en-us" }; HRESULT hr;
    if (t_is_wine()) { SB_END(); t_skip("emulator does not validate"); return; }
#define AF(file, sim, vals, nv, rngs, nr, props, np, what) hr = IDWriteFontSetBuilder2_AddFont(S.b2, file, 0, sim, vals, nv, rngs, nr, props, np); CHECKF(hr == E_INVALIDARG, what " -> %s", dw_hr_name(hr))
    AF(NULL, DWRITE_FONT_SIMULATIONS_NONE, &v, 1, &r, 1, &p, 1, "NULL file");
    AF(S.x.file, (DWRITE_FONT_SIMULATIONS)4, &v, 1, &r, 1, &p, 1, "simulations 4");
    AF(S.x.file, (DWRITE_FONT_SIMULATIONS)0x80000000u, NULL, 0, NULL, 0, NULL, 0, "simulations 0x80000000");
    AF(S.x.file, DWRITE_FONT_SIMULATIONS_NONE, NULL, 1, &r, 1, &p, 1, "NULL axis values with count");
    AF(S.x.file, DWRITE_FONT_SIMULATIONS_NONE, &v, 0x10000, &r, 1, &p, 1, "axis value count 0x10000");
    AF(S.x.file, DWRITE_FONT_SIMULATIONS_NONE, &v, 1, NULL, 1, &p, 1, "NULL ranges with count");
    AF(S.x.file, DWRITE_FONT_SIMULATIONS_NONE, &v, 1, &r, 1, NULL, 1, "NULL properties with count");
#undef AF
    { IDWriteFontSet1 *s = make_set(&S); if (s) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(s), 0); DW_RELEASE(s); } }     /* nothing was added */
    SB_END();
}
TEST(r2_variable_fonts_sets_add_font_plain)
{
    SB_BEGIN();
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(S.b2, S.x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 0));
    { IDWriteFontSet1 *s = make_set(&S); if (s) { t_note("set after one plain AddFont: %u fonts", IDWriteFontSet1_GetFontCount(s)); CHECK(IDWriteFontSet1_GetFontCount(s) >= 1); DW_RELEASE(s); } }
    SB_END();
}
TEST(r2_variable_fonts_sets_add_font_zero_counts_ignore_pointers)
{
    SB_BEGIN();
    DWRITE_FONT_AXIS_VALUE v = rv_av(TAG_WGHT, 500); DWRITE_FONT_AXIS_RANGE r = { TAG_WGHT, 100, 900 }; DWRITE_FONT_PROPERTY p = { DWRITE_FONT_PROPERTY_ID_FULL_NAME, L"x", L"en-us" };
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(S.b2, S.x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &v, 0, &r, 0, &p, 0));
    SB_END();
}
TEST(r2_variable_fonts_sets_add_font_with_axis_values_and_ranges)
{
    SB_BEGIN();
    DWRITE_FONT_AXIS_VALUE v[2]; DWRITE_FONT_AXIS_RANGE r[2]; IDWriteFontSet1 *s;
    v[0] = rv_av(TAG_WGHT, 650); v[1] = rv_av(TAG_WDTH, 90);
    r[0].axisTag = TAG_WGHT; r[0].minValue = 500; r[0].maxValue = 800; r[1].axisTag = TAG_WDTH; r[1].minValue = 80; r[1].maxValue = 100;
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(S.b2, S.x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, v, 2, r, 2, NULL, 0));
    s = make_set(&S);
    if (s) {
        UINT32 n = IDWriteFontSet1_GetFontCount(s); t_note("set after AddFont with values and ranges: %u fonts", n);
        if (n >= 1) {
            IDWriteFontFaceReference1 *ref = NULL;
            if (SUCCEEDED(IDWriteFontSet1_GetFontFaceReference(s, 0, &ref)) && ref) {
                DWRITE_FONT_AXIS_VALUE g[8]; UINT32 c = IDWriteFontFaceReference1_GetFontAxisValueCount(ref);
                t_note("entry 0 reference carries %u axis values", c);
                if (c == 2 && SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(ref, g, 2))) CHECK(g[0].axisTag == TAG_WGHT && g[0].value == 650.0f && g[1].axisTag == TAG_WDTH && g[1].value == 90.0f);
                DW_RELEASE(ref);
            }
        }
        DW_RELEASE(s);
    }
    SB_END();
}
TEST(r2_variable_fonts_sets_add_font_ranges_reported)
{
    SB_BEGIN();
    DWRITE_FONT_AXIS_RANGE r; IDWriteFontSet1 *s;
    r.axisTag = TAG_WGHT; r.minValue = 500; r.maxValue = 800;
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(S.b2, S.x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, &r, 1, NULL, 0));
    s = make_set(&S);
    if (s && IDWriteFontSet1_GetFontCount(s) >= 1) {
        DWRITE_FONT_AXIS_RANGE g[16]; UINT32 actual = 0; HRESULT hr = IDWriteFontSet1_GetFontAxisRanges(s, g, 16, &actual);
        CHECK_HR(hr, S_OK);
        if (SUCCEEDED(hr) && actual <= 16) {
            UINT32 i; int k = -1;
            for (i = 0; i < actual; i++) if (g[i].axisTag == TAG_WGHT) k = (int)i;
            CHECK(k >= 0);
            if (k >= 0) t_note("wght range reported for an entry added with [500, 800]: [%g, %g]", g[k].minValue, g[k].maxValue);
        }
    }
    DW_RELEASE(s);
    SB_END();
}
TEST(r2_variable_fonts_sets_add_font_simulations_valid)
{
    SB_BEGIN();
    unsigned sim;
    for (sim = 0; sim < 4; sim++) CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(S.b2, S.x.file, 0, (DWRITE_FONT_SIMULATIONS)sim, NULL, 0, NULL, 0, NULL, 0));
    SB_END();
}
TEST(r2_variable_fonts_sets_add_font_many_axis_values)
{
    SB_BEGIN();
    DWRITE_FONT_AXIS_VALUE v[40]; int i;
    for (i = 0; i < 40; i++) v[i] = rv_av(RV_TAG('s', 'b', '0' + i / 10, '0' + i % 10), (float)i);
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(S.b2, S.x.file, 0, DWRITE_FONT_SIMULATIONS_NONE, v, 40, NULL, 0, NULL, 0));
    SB_END();
}

/* ---- AddFontFile expands named instances ---------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_sets_file_set_has_fonts)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s) { UINT32 n = IDWriteFontSet1_GetFontCount(s); t_note("AddFontFile of the fixture (%d named instances): %u fonts", RV_NINST, n); CHECK(n >= 1); DW_RELEASE(s); }
    else CHECK(0);
    SB_END();
}
TEST(r2_variable_fonts_sets_file_set_entries_use_instance_weights)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s) {
        UINT32 n = IDWriteFontSet1_GetFontCount(s), i;
        for (i = 0; i < n && i < 16; i++) {
            IDWriteFontFaceReference1 *ref = NULL;
            if (SUCCEEDED(IDWriteFontSet1_GetFontFaceReference(s, i, &ref)) && ref) {
                DWRITE_FONT_AXIS_VALUE g[16]; UINT32 c = IDWriteFontFaceReference1_GetFontAxisValueCount(ref);
                if (c > 0 && c <= 16 && SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(ref, g, c))) {
                    int k = rv_find_value(g, c, TAG_WGHT);
                    if (k >= 0) CHECKF(g[k].value == 300.0f || g[k].value == 400.0f || g[k].value == 700.0f, "entry %u: wght %g is not a named instance weight", i, g[k].value);
                    t_note("entry %u: %u axis values, wght %g", i, c, k >= 0 ? g[k].value : -1.0f);
                }
                DW_RELEASE(ref);
            }
        }
        DW_RELEASE(s);
    }
    SB_END();
}
TEST(r2_variable_fonts_sets_file_set_axis_ranges_within_fvar)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s) {
        DWRITE_FONT_AXIS_RANGE g[16]; UINT32 actual = 0, i;
        CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges(s, g, 16, &actual));
        if (actual <= 16) for (i = 0; i < actual; i++) {
            if (g[i].axisTag == TAG_WGHT) CHECKF(g[i].minValue >= 100.0f && g[i].maxValue <= 900.0f && g[i].minValue <= g[i].maxValue, "wght [%g, %g]", g[i].minValue, g[i].maxValue);
            if (g[i].axisTag == TAG_WDTH) CHECKF(g[i].minValue >= 75.0f && g[i].maxValue <= 125.0f && g[i].minValue <= g[i].maxValue, "wdth [%g, %g]", g[i].minValue, g[i].maxValue);
            t_note("set axis range %c%c%c%c [%g, %g]", g[i].axisTag & 0xFF, (g[i].axisTag >> 8) & 0xFF, (g[i].axisTag >> 16) & 0xFF, (g[i].axisTag >> 24) & 0xFF, g[i].minValue, g[i].maxValue);
        }
        DW_RELEASE(s);
    }
    SB_END();
}
TEST(r2_variable_fonts_sets_file_set_resource_and_face)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s && IDWriteFontSet1_GetFontCount(s) >= 1) {
        IDWriteFontResource *r = NULL; IDWriteFontFace5 *f = NULL;
        CHECK_HR_OK(IDWriteFontSet1_CreateFontResource(s, 0, &r));
        if (r) { EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(r), RV_NALL); CHECK(IDWriteFontResource_HasVariations(r) == TRUE); DW_RELEASE(r); }
        CHECK_HR_OK(IDWriteFontSet1_CreateFontFace(s, 0, &f));
        if (f) { CHECK(IDWriteFontFace5_HasVariations(f) == TRUE); EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), RV_NALL); DW_RELEASE(f); }
    }
    DW_RELEASE(s);
    SB_END();
}
TEST(r2_variable_fonts_sets_file_set_face_matches_reference_values)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s) {
        UINT32 n = IDWriteFontSet1_GetFontCount(s), i;
        for (i = 0; i < n && i < 8; i++) {
            IDWriteFontFaceReference1 *ref = NULL; IDWriteFontFace5 *f = NULL;
            if (SUCCEEDED(IDWriteFontSet1_GetFontFaceReference(s, i, &ref)) && ref && SUCCEEDED(IDWriteFontSet1_CreateFontFace(s, i, &f)) && f) {
                DWRITE_FONT_AXIS_VALUE rv[16], fv[16]; UINT32 rc = IDWriteFontFaceReference1_GetFontAxisValueCount(ref), fc = IDWriteFontFace5_GetFontAxisValueCount(f), j;
                if (rc > 0 && rc <= 16 && fc <= 16 && SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(ref, rv, rc)) && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(f, fv, fc)))
                    for (j = 0; j < rc; j++) { int k = rv_find_value(fv, fc, rv[j].axisTag); CHECKF(k >= 0, "entry %u: axis %08x of the reference is missing on the face", i, rv[j].axisTag); }
            }
            DW_RELEASE(f); DW_RELEASE(ref);
        }
        DW_RELEASE(s);
    }
    SB_END();
}
TEST(r2_variable_fonts_sets_first_font_resources_one_per_file)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s && IDWriteFontSet1_GetFontCount(s) >= 1) {
        IDWriteFontSet1 *f = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetFirstFontResources(s, &f));
        if (f) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(f), 1); DW_RELEASE(f); }       /* one entry for the single font resource */
    }
    DW_RELEASE(s);
    SB_END();
}

/* ---- filtering by axis range ------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_sets_filter_wide_range_keeps_everything)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s && IDWriteFontSet1_GetFontCount(s) >= 1) {
        DWRITE_FONT_AXIS_RANGE r = { TAG_WGHT, 0, 1000 }; IDWriteFontSet1 *f = NULL; HRESULT hr = IDWriteFontSet1_GetFilteredFonts_(s, &r, 1, TRUE, &f);
        CHECKF(hr == S_OK, "-> %s", dw_hr_name(hr));
        if (f) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(f), IDWriteFontSet1_GetFontCount(s)); DW_RELEASE(f); }
    }
    DW_RELEASE(s);
    SB_END();
}
TEST(r2_variable_fonts_sets_filter_empty_result_is_not_an_error)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s && IDWriteFontSet1_GetFontCount(s) >= 1) {
        DWRITE_FONT_AXIS_RANGE r = { TAG_WGHT, 5000, 6000 }; IDWriteFontSet1 *f = RV_SENT(IDWriteFontSet1 *); HRESULT hr = IDWriteFontSet1_GetFilteredFonts_(s, &r, 1, TRUE, &f);
        CHECKF(hr == S_OK, "-> %s", dw_hr_name(hr));
        if (hr == S_OK && f && f != RV_SENT(IDWriteFontSet1 *)) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(f), 0); DW_RELEASE(f); }
    }
    DW_RELEASE(s);
    SB_END();
}
TEST(r2_variable_fonts_sets_filter_subset_never_larger)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s && IDWriteFontSet1_GetFontCount(s) >= 1) {
        static const struct { float lo, hi; } t[] = { { 100, 300 }, { 350, 450 }, { 600, 800 }, { 400, 400 }, { 700, 700 }, { 0, 50 }, { 899, 5000 } }; unsigned i;
        for (i = 0; i < sizeof t / sizeof t[0]; i++) {
            DWRITE_FONT_AXIS_RANGE r; IDWriteFontSet1 *f = NULL; r.axisTag = TAG_WGHT; r.minValue = t[i].lo; r.maxValue = t[i].hi;
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts_(s, &r, 1, TRUE, &f));
            if (f) { CHECKF(IDWriteFontSet1_GetFontCount(f) <= IDWriteFontSet1_GetFontCount(s), "range [%g, %g]", t[i].lo, t[i].hi); DW_RELEASE(f); }
        }
    }
    DW_RELEASE(s);
    SB_END();
}
TEST(r2_variable_fonts_sets_filter_zero_ranges)
{
    SB_BEGIN();
    IDWriteFontSet1 *s = file_set(&S);
    if (s && IDWriteFontSet1_GetFontCount(s) >= 1) {
        IDWriteFontSet1 *f = NULL; HRESULT hr = IDWriteFontSet1_GetFilteredFonts_(s, NULL, 0, TRUE, &f);
        t_note("GetFilteredFonts with no ranges (any = TRUE) -> %s, %u fonts", dw_hr_name(hr), f ? IDWriteFontSet1_GetFontCount(f) : 0);
        DW_RELEASE(f);
    }
    DW_RELEASE(s);
    SB_END();
}
TEST(r2_variable_fonts_sets_builder_reuse)
{
    SB_BEGIN();
    IDWriteFontSet1 *a = file_set(&S), *b;
    UINT32 n1 = a ? IDWriteFontSet1_GetFontCount(a) : 0;
    b = make_set(&S);
    if (a && b) CHECK(IDWriteFontSet1_GetFontCount(b) == n1);          /* creating a set does not consume the builder */
    DW_RELEASE(a); DW_RELEASE(b);
    SB_END();
}
