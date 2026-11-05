/* IDWriteFontResource::GetAxisNames, GetAxisValueNameCount and GetAxisValueNames on synthetic variable fonts.
 *
 * Win10 dwrite.dll (IDWriteFontResource::GetAxisNames, GetAxisValueNameCount, GetAxisValueNames over the style attribute data
 *):
 *   - GetAxisNames zeroes *names first, GetAxisValueNames zeroes *names and the range first; both return a NEW IDWriteLocalizedStrings object on every call
 *     (possibly with zero strings) and S_OK.
 *   - an axis index >= the axis count (and for GetAxisValueNames a value index >= the axis' value-name count) raises ArgumentException inside the
 *     call; the method's catch handler (catch$ funclets in IDWriteFontResource::GetAxisNames / GetAxisValueNames) maps it to E_INVALIDARG, so the call fails
 *     cleanly with the outputs already zeroed. GetAxisValueNameCount has no HRESULT, its handler swallows the failure.
 *   - when a value record carries a range, *range receives { axis tag, minimum, maximum } of the record.
 */
#include "g1.h"

typedef struct vx { IDWriteFactory *f; IDWriteFactory6 *f6; dwx_mem_loader *ld; IDWriteFontFile *file; IDWriteFontResource *res; int ok; } vx;
static int vx_open(vx *x, const uint8_t *blob, size_t n)
{
    UINT32 key; HRESULT hr;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("DWriteCreateFactory failed"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory6, (void **)&x->f6)) || !x->f6) { t_skip("IDWriteFactory6 unsupported"); return 0; }
    x->ld = dwx_loader_new();
    key = dwx_loader_add(x->ld, blob, n);
    if (FAILED(dwx_loader_register(x->ld, x->f))) { t_skip("loader registration failed"); return 0; }
    if (FAILED(dwx_loader_file(x->ld, x->f, key, &x->file)) || !x->file) { t_skip("font file creation failed"); return 0; }
    hr = IDWriteFactory6_CreateFontResource(x->f6, x->file, 0, &x->res);
    if (FAILED(hr) || !x->res) { t_skip("CreateFontResource -> %s", dw_hr_name(hr)); return 0; }
    x->ok = 1;
    return 1;
}
static void vx_close(vx *x)
{
    DW_RELEASE(x->res); DW_RELEASE(x->file);
    if (x->ld) { dwx_loader_unregister(x->ld); dwx_release(x->ld); }
    DW_RELEASE(x->f6); DW_RELEASE(x->f);
}
#define VX_BEGIN(blob_expr) size_t n_; uint8_t *b_ = (blob_expr); vx x; if (!b_) { t_skip("font build failed"); return; } \
    if (!vx_open(&x, b_, n_)) { vx_close(&x); free(b_); return; }
#define VX_END() do { vx_close(&x); free(b_); } while (0)
#define VAR2() VX_BEGIN(g1_build_var2("G1Var2", &n_))

/* Index of the axis with the given tag (-1 when absent). */
static int axis_index(IDWriteFontResource *res, UINT32 tag)
{
    DWRITE_FONT_AXIS_VALUE v[32]; UINT32 n = IDWriteFontResource_GetFontAxisCount(res), i;
    if (n == 0 || n > 32 || FAILED(IDWriteFontResource_GetDefaultFontAxisValues(res, v, n))) return -1;
    for (i = 0; i < n; i++) if (v[i].axisTag == tag) return (int)i;
    return -1;
}
#define TAG_WGHT G1_TAG('w', 'g', 'h', 't')
#define TAG_XTRA G1_TAG('X', 'T', 'R', 'A')
static void en_name(IDWriteLocalizedStrings *s, char *out, int cap)
{
    out[0] = 0;
    if (s && IDWriteLocalizedStrings_GetCount(s) > 0) dw_ls_en(s, out, cap);
}

/* ===== GetAxisNames ====================================================================================================== */
TEST(gap1_vr_axis_names_text)
{
    VAR2();
    IDWriteLocalizedStrings *s = NULL; char en[64]; int w = axis_index(x.res, TAG_WGHT), e = axis_index(x.res, TAG_XTRA);
    CHECK(IDWriteFontResource_GetFontAxisCount(x.res) >= 2); CHECK(w >= 0 && e >= 0);
    if (w >= 0) { CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, (UINT32)w, &s)); CHECK(s != NULL); en_name(s, en, sizeof en); EXPECT_STR(en, "Weight"); DW_RELEASE(s); }
    if (e >= 0) { CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, (UINT32)e, &s)); CHECK(s != NULL); en_name(s, en, sizeof en); EXPECT_STR(en, "Extra Axis"); DW_RELEASE(s); }
    VX_END();
}
TEST(gap1_vr_axis_names_new_object_each_call)
{
    VAR2();
    IDWriteLocalizedStrings *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, 0, &a)); CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, 0, &b));
    CHECK(a != NULL && b != NULL && a != b);
    if (a) EXPECT_REFS(a, 1);
    if (b) EXPECT_REFS(b, 1);
    DW_RELEASE(a); DW_RELEASE(b);
    VX_END();
}
TEST(gap1_vr_axis_names_outlive_resource_file_and_factory)
{
    VAR2();
    IDWriteLocalizedStrings *s = NULL; char en[64]; int e = axis_index(x.res, TAG_XTRA);
    if (e < 0) { VX_END(); t_skip("axis lookup failed"); return; }
    CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, (UINT32)e, &s));
    DW_RELEASE(x.res); DW_RELEASE(x.file);
    en_name(s, en, sizeof en); EXPECT_STR(en, "Extra Axis");
    DW_RELEASE(s);
    VX_END();
}
TEST(gap1_vr_axis_names_index_out_of_range)
{
    static const UINT32 bad[] = { 3, 4, 100, 0x7FFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFFu };
    unsigned i;
    VAR2();
    { UINT32 na = IDWriteFontResource_GetFontAxisCount(x.res); IDWriteLocalizedStrings *s = G1_SENT(IDWriteLocalizedStrings *);
      if (!t_is_wine()) { HRESULT hr = IDWriteFontResource_GetAxisNames(x.res, na, &s); CHECKF(hr == E_INVALIDARG, "axis == axisCount -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(s); if (s == G1_SENT(IDWriteLocalizedStrings *)) s = NULL; DW_RELEASE(s); } }
    if (t_is_wine()) { VX_END(); t_skip("emulator does not validate the axis index"); return; }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteLocalizedStrings *s = G1_SENT(IDWriteLocalizedStrings *);
        HRESULT hr = IDWriteFontResource_GetAxisNames(x.res, bad[i], &s);
        t_note("GetAxisNames(%u) -> %s, out %s", bad[i], dw_hr_name(hr), s == NULL ? "NULL" : s == G1_SENT(IDWriteLocalizedStrings *) ? "untouched" : "object");
        if (bad[i] >= 100) CHECKF(hr == E_INVALIDARG, "axis %u -> %s", bad[i], dw_hr_name(hr));
        if (s == G1_SENT(IDWriteLocalizedStrings *)) s = NULL;
        DW_RELEASE(s);
    }
    VX_END();
}
TEST(gap1_vr_axis_names_non_variable_font_has_no_axis_zero)
{
    size_t n; uint8_t *b = ttf_build_default(&n); vx x;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    if (!t_is_wine()) {
        IDWriteLocalizedStrings *s = G1_SENT(IDWriteLocalizedStrings *);
        HRESULT hr = IDWriteFontResource_GetAxisNames(x.res, 0, &s);
        t_note("axis count %u; GetAxisNames(0) -> %s", (unsigned)IDWriteFontResource_GetFontAxisCount(x.res), dw_hr_name(hr));
        if (s == G1_SENT(IDWriteLocalizedStrings *)) s = NULL;
        DW_RELEASE(s);
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_axis_without_name_id_succeeds_note)
{
    g1_axis ax[2] = { { "wght", 100, 400, 900, 0, NULL }, { "XTRA", 10, 50, 90, 999, NULL } };       /* no name record at all / id without a name record */
    size_t n; uint8_t *b = g1_build_var("G1NoNames", ax, 2, NULL, 0, NULL, 0, &n); vx x; int i;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    for (i = 0; i < 2; i++) {
        IDWriteLocalizedStrings *s = G1_SENT(IDWriteLocalizedStrings *); HRESULT hr = IDWriteFontResource_GetAxisNames(x.res, (UINT32)i, &s);
        CHECK_HR(hr, S_OK);
        if (SUCCEEDED(hr) && s != G1_SENT(IDWriteLocalizedStrings *)) { t_note("axis %d without a name: %s object, %u strings", i, s ? "an" : "no", s ? IDWriteLocalizedStrings_GetCount(s) : 0); DW_RELEASE(s); }
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_axis_names_all_axes_of_a_five_axis_font)
{
    static const char *tags[] = { "wght", "wdth", "slnt", "opsz", "XTRA" };
    static const char *names[] = { "Weight", "Width", "Slant", "Optical Size", "Extra" };
    g1_axis ax[5]; size_t n; uint8_t *b; vx x; int i;
    for (i = 0; i < 5; i++) { memcpy(ax[i].tag, tags[i], 5); ax[i].mn = 1; ax[i].df = 2; ax[i].mx = 3; ax[i].nameid = 300 + i; ax[i].name = names[i]; }
    b = g1_build_var("G1Five", ax, 5, NULL, 0, NULL, 0, &n);
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    CHECK(IDWriteFontResource_GetFontAxisCount(x.res) >= 5);
    for (i = 0; i < 5; i++) {
        IDWriteLocalizedStrings *s = NULL; char en[64]; int idx = axis_index(x.res, G1_TAG(tags[i][0], tags[i][1], tags[i][2], tags[i][3]));
        CHECKF(idx >= 0, "axis %s not found", tags[i]);
        if (idx < 0) continue;
        CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, (UINT32)idx, &s));
        en_name(s, en, sizeof en); EXPECT_STR(en, names[i]);
        DW_RELEASE(s);
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_axis_names_locale_lookup)
{
    VAR2();
    IDWriteLocalizedStrings *s = NULL; UINT32 idx = 99; BOOL ex = TRUE;
    CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, 0, &s));
    if (s) {
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"en-US", &idx, &ex));
        t_note("en-US present: %d index %u count %u", (int)ex, idx, IDWriteLocalizedStrings_GetCount(s));
        idx = 99; ex = TRUE;
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"xx-YY", &idx, &ex));
        EXPECT_EQ(ex, FALSE);
    }
    DW_RELEASE(s);
    VX_END();
}

/* ===== value names ========================================================================================================= */
static const g1_stat_val STAT_W[] = {
    { 1, 0, 400, "Thin", 100, 0, 0, 0 },
    { 1, 0, 401, "Regular", 400, 0, 0, 0 },
    { 1, 0, 402, "Bold", 700, 0, 0, 0 },
    { 2, 0, 403, "Heavyish", 850, 800, 900, 0 },
    { 3, 0, 404, "Linked", 500, 0, 0, 700 },
    { 1, 1, 405, "Narrow", 10, 0, 0, 0 },
};
static uint8_t *stat_font(const char *family, size_t *n)
{
    g1_axis ax[2] = { { "wght", 100, 400, 900, 256, "Weight" }, { "XTRA", 10, 50, 90, 257, "Extra Axis" } };
    return g1_build_var(family, ax, 2, STAT_W, (int)(sizeof STAT_W / sizeof STAT_W[0]), NULL, 0, n);
}
static void dump_values(IDWriteFontResource *res)
{
    UINT32 na = IDWriteFontResource_GetFontAxisCount(res), a;
    for (a = 0; a < na; a++) {
        UINT32 c = IDWriteFontResource_GetAxisValueNameCount(res, a), v;
        t_note("axis %u: %u named values", a, c);
        for (v = 0; v < c && v < 16; v++) {
            DWRITE_FONT_AXIS_RANGE rg; IDWriteLocalizedStrings *s = NULL; char en[64];
            memset(&rg, 0xCC, sizeof rg);
            if (SUCCEEDED(IDWriteFontResource_GetAxisValueNames(res, a, v, &rg, &s))) {
                en_name(s, en, sizeof en);
                t_note("  value %u \"%s\" tag %08x [%g, %g]", v, en, (unsigned)rg.axisTag, rg.minValue, rg.maxValue);
            }
            DW_RELEASE(s);
        }
    }
}
TEST(gap1_vr_value_names_stat_table_note)
{
    size_t n; uint8_t *b = stat_font("G1Stat", &n); vx x;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    dump_values(x.res);
    vx_close(&x); free(b);
}
TEST(gap1_vr_value_names_every_listed_value_retrievable)
{
    size_t n; uint8_t *b = stat_font("G1StatAll", &n); vx x; UINT32 a;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    for (a = 0; a < IDWriteFontResource_GetFontAxisCount(x.res); a++) {
        UINT32 c = IDWriteFontResource_GetAxisValueNameCount(x.res, a), v;
        CHECKF(c <= 64, "axis %u: implausible value count %u", a, c);
        for (v = 0; v < c && v < 64; v++) {
            DWRITE_FONT_AXIS_RANGE rg; IDWriteLocalizedStrings *s = G1_SENT(IDWriteLocalizedStrings *); HRESULT hr;
            memset(&rg, 0xCC, sizeof rg);
            hr = IDWriteFontResource_GetAxisValueNames(x.res, a, v, &rg, &s);
            CHECK_HR(hr, S_OK);
            if (SUCCEEDED(hr)) {
                CHECKF(s != NULL && s != G1_SENT(IDWriteLocalizedStrings *), "axis %u value %u: no strings object", a, v);
                CHECKF(rg.minValue <= rg.maxValue || rg.axisTag == 0, "axis %u value %u: range [%g, %g]", a, v, rg.minValue, rg.maxValue);
                CHECKF(rg.axisTag == 0 || rg.axisTag == TAG_WGHT || rg.axisTag == TAG_XTRA, "axis %u value %u: tag %08x", a, v, (unsigned)rg.axisTag);
                if (s == G1_SENT(IDWriteLocalizedStrings *)) s = NULL;
                DW_RELEASE(s);
            }
        }
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_value_count_stable_and_per_axis)
{
    size_t n; uint8_t *b = stat_font("G1StatCnt", &n); vx x; int i;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    {
        UINT32 c0 = IDWriteFontResource_GetAxisValueNameCount(x.res, 0), c1 = IDWriteFontResource_GetAxisValueNameCount(x.res, 1);
        for (i = 0; i < 10; i++) { EXPECT_EQ(IDWriteFontResource_GetAxisValueNameCount(x.res, 0), c0); EXPECT_EQ(IDWriteFontResource_GetAxisValueNameCount(x.res, 1), c1); }
        t_note("value-name counts: wght %u, XTRA %u (STAT lists 5 and 1 values)", c0, c1);
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_value_names_index_out_of_range)
{
    size_t n; uint8_t *b = stat_font("G1StatOOR", &n); vx x;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    if (!t_is_wine()) {
        UINT32 c0 = IDWriteFontResource_GetAxisValueNameCount(x.res, 0), c1 = IDWriteFontResource_GetAxisValueNameCount(x.res, 1);
        struct { UINT32 axis, val; const char *what; } rows[] = {
            { 0, c0, "value == count" }, { 0, c0 + 1, "value == count + 1" }, { 0, 0xFFFFFFFFu, "value 0xFFFFFFFF" }, { 1, c1, "axis 1 value == count" },
            { IDWriteFontResource_GetFontAxisCount(x.res), 0, "axis == axisCount" }, { IDWriteFontResource_GetFontAxisCount(x.res) + 1, 0, "axis == axisCount + 1" }, { 0xFFFFFFFFu, 0, "axis 0xFFFFFFFF" }, { 0xFFFFFFFFu, 0xFFFFFFFFu, "both max" }, { 100, 100, "axis 100 value 100" },
        };
        unsigned i;
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            DWRITE_FONT_AXIS_RANGE rg; IDWriteLocalizedStrings *s = G1_SENT(IDWriteLocalizedStrings *); HRESULT hr;
            memset(&rg, 0xCC, sizeof rg);
            hr = IDWriteFontResource_GetAxisValueNames(x.res, rows[i].axis, rows[i].val, &rg, &s);
            CHECKF(hr == E_INVALIDARG, "%s -> %s", rows[i].what, dw_hr_name(hr));
            CHECKF(s == NULL, "%s: names not zeroed", rows[i].what);
            CHECKF(rg.axisTag == 0 && rg.minValue == 0.0f && rg.maxValue == 0.0f, "%s: range not zeroed (%08x %g %g)", rows[i].what, (unsigned)rg.axisTag, rg.minValue, rg.maxValue);
            if (s == G1_SENT(IDWriteLocalizedStrings *)) s = NULL;
            DW_RELEASE(s);
        }
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_value_names_failure_leaves_resource_usable)
{
    size_t n; uint8_t *b = stat_font("G1StatUse", &n); vx x;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    if (!t_is_wine()) {
        DWRITE_FONT_AXIS_RANGE rg; IDWriteLocalizedStrings *s = NULL;
        UINT32 c0 = IDWriteFontResource_GetAxisValueNameCount(x.res, 0);
        IDWriteFontResource_GetAxisValueNames(x.res, 9, 9, &rg, &s);
        EXPECT_EQ(IDWriteFontResource_GetAxisValueNameCount(x.res, 0), c0);
        CHECK(IDWriteFontResource_GetFontAxisCount(x.res) >= 2);
        CHECK_HR_OK(IDWriteFontResource_GetAxisNames(x.res, 0, &s)); DW_RELEASE(s);
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_value_names_outlive_resource)
{
    size_t n; uint8_t *b = stat_font("G1StatLife", &n); vx x;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    {
        UINT32 c = IDWriteFontResource_GetAxisValueNameCount(x.res, 0);
        if (c > 0) {
            DWRITE_FONT_AXIS_RANGE rg; IDWriteLocalizedStrings *s = NULL; char en1[64], en2[64];
            CHECK_HR_OK(IDWriteFontResource_GetAxisValueNames(x.res, 0, 0, &rg, &s));
            en_name(s, en1, sizeof en1);
            DW_RELEASE(x.res); DW_RELEASE(x.file);
            en_name(s, en2, sizeof en2);
            EXPECT_STR(en1, en2);
            DW_RELEASE(s);
        } else t_note("no named values reported");
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_value_names_new_object_each_call)
{
    size_t n; uint8_t *b = stat_font("G1StatNew", &n); vx x;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    if (IDWriteFontResource_GetAxisValueNameCount(x.res, 0) > 0) {
        DWRITE_FONT_AXIS_RANGE r1, r2; IDWriteLocalizedStrings *a = NULL, *c = NULL;
        CHECK_HR_OK(IDWriteFontResource_GetAxisValueNames(x.res, 0, 0, &r1, &a)); CHECK_HR_OK(IDWriteFontResource_GetAxisValueNames(x.res, 0, 0, &r2, &c));
        CHECK(a && c && a != c); CHECK(!memcmp(&r1, &r2, sizeof r1));
        DW_RELEASE(a); DW_RELEASE(c);
    }
    vx_close(&x); free(b);
}
TEST(gap1_vr_font_without_stat_value_names_note)
{
    VAR2();
    UINT32 a;
    for (a = 0; a < 2; a++) t_note("axis %u of a font without STAT: %u named values", a, IDWriteFontResource_GetAxisValueNameCount(x.res, a));
    dump_values(x.res);
    VX_END();
}
TEST(gap1_vr_named_instances_in_fvar_note)
{
    g1_axis ax[1] = { { "wght", 100, 400, 900, 256, "Weight" } };
    g1_inst in[3] = { { 270, "Hairline", { 100 } }, { 271, "Normal Instance", { 400 } }, { 272, "Black Instance", { 900 } } };
    size_t n; uint8_t *b = g1_build_var("G1Inst", ax, 1, NULL, 0, in, 3, &n); vx x;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    t_note("font with 3 fvar named instances and no STAT: axis 0 has %u named values", IDWriteFontResource_GetAxisValueNameCount(x.res, 0));
    dump_values(x.res);
    vx_close(&x); free(b);
}
TEST(gap1_vr_stat_axis_index_beyond_axes_note)
{
    g1_axis ax[1] = { { "wght", 100, 400, 900, 256, "Weight" } };
    g1_stat_val sv[2] = { { 1, 0, 400, "Ok", 400, 0, 0, 0 }, { 1, 7, 401, "Stray", 1, 0, 0, 0 } };
    size_t n; uint8_t *b = g1_build_var("G1StatBad", ax, 1, sv, 2, NULL, 0, &n); vx x;
    if (!b || !vx_open(&x, b, n)) { vx_close(&x); free(b); return; }
    t_note("STAT value for a non-existent axis: axis 0 value count %u", IDWriteFontResource_GetAxisValueNameCount(x.res, 0));
    vx_close(&x); free(b);
}
TEST(gap1_vr_axis_value_name_count_out_of_range_note)
{
    VAR2();
    if (t_is_wine()) { VX_END(); t_skip("emulator does not validate the axis index"); return; }
    t_note("GetAxisValueNameCount(axis == axisCount) -> %u", IDWriteFontResource_GetAxisValueNameCount(x.res, IDWriteFontResource_GetFontAxisCount(x.res)));
    t_note("GetAxisValueNameCount(0xFFFFFFFF) -> %u", IDWriteFontResource_GetAxisValueNameCount(x.res, 0xFFFFFFFFu));
    CHECK(IDWriteFontResource_GetFontAxisCount(x.res) >= 2);        /* the resource keeps working */
    VX_END();
}

/* ===== axis ranges of the resource against the font set =================================================================== */
static int find_range_tag(const DWRITE_FONT_AXIS_RANGE *r, UINT32 n, UINT32 tag) { UINT32 i; for (i = 0; i < n; i++) if (r[i].axisTag == tag) return (int)i; return -1; }
TEST(gap1_vr_resource_axis_ranges_match_fvar)
{
    VAR2();
    DWRITE_FONT_AXIS_RANGE r[32]; UINT32 na = IDWriteFontResource_GetFontAxisCount(x.res); int w, e;
    memset(r, 0xCC, sizeof r);
    if (na == 0 || na > 32) { VX_END(); t_skip("axis count %u", na); return; }
    CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(x.res, r, na));
    w = find_range_tag(r, na, TAG_WGHT); e = find_range_tag(r, na, TAG_XTRA);
    CHECK(w >= 0 && e >= 0);
    if (w >= 0) { CHECK_FLOAT_EQ(r[w].minValue, 100.0f); CHECK_FLOAT_EQ(r[w].maxValue, 900.0f); }
    if (e >= 0) { CHECK_FLOAT_EQ(r[e].minValue, 10.0f); CHECK_FLOAT_EQ(r[e].maxValue, 90.0f); }
    VX_END();
}
TEST(gap1_vr_default_axis_values_match_fvar)
{
    VAR2();
    DWRITE_FONT_AXIS_VALUE v[32]; UINT32 na = IDWriteFontResource_GetFontAxisCount(x.res), i; int w = -1, e = -1;
    memset(v, 0xCC, sizeof v);
    if (na == 0 || na > 32) { VX_END(); t_skip("axis count %u", na); return; }
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x.res, v, na));
    for (i = 0; i < na; i++) { if (v[i].axisTag == TAG_WGHT) w = (int)i; if (v[i].axisTag == TAG_XTRA) e = (int)i; }
    CHECK(w >= 0 && e >= 0);
    if (w >= 0) CHECK_FLOAT_EQ(v[w].value, 400.0f);
    if (e >= 0) CHECK_FLOAT_EQ(v[e].value, 50.0f);
    VX_END();
}
