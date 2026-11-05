/* Standard axes (wght, wdth, ital, slnt) of fonts that do not define them in fvar.
 *
 * Win10 dwrite.dll (standard axes are added after the axes are read): for each tag of
 * {wght, wdth, ital, slnt} that the font (fvar or STAT) does not define an axis is added with minimum == default == maximum:
 *   wght = the font's weight (OS/2 usWeightClass), wdth = FontStretchToWidthAxisValue(stretch) (100 for normal),
 *   ital = 1 for DWRITE_FONT_STYLE_ITALIC else 0,
 *   slnt = the 'post' italicAngle (16.16) when the font has a post table, otherwise FontStyleToSlantFixedAxisValue: oblique -20, italic -12, normal 0.
 * A non-variable font therefore reports exactly these four axes, every one with attributes NONE.
 */
#include "rv.h"

static int open_spec(rv *x, const ttf_spec *s, uint8_t **keep)
{
    size_t n; uint8_t *b = ttf_build(s, &n);
    *keep = b;
    if (!b) { t_skip("font build failed"); return 0; }
    return rv_open(x, b, n);
}
#define STATIC_BEGIN(spec) ttf_spec s_ = (spec); rv x; uint8_t *keep_ = NULL; if (!open_spec(&x, &s_, &keep_)) { rv_close(&x); free(keep_); return; }
#define STATIC_END() do { rv_close(&x); free(keep_); } while (0)

static void get_axes(rv *x, DWRITE_FONT_AXIS_VALUE *v, DWRITE_FONT_AXIS_RANGE *r, UINT32 *n)
{
    *n = IDWriteFontResource_GetFontAxisCount(x->res);
    if (*n == 0 || *n > 32) { *n = 0; return; }
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(x->res, v, *n));
    CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(x->res, r, *n));
}
static ttf_spec styled(int weight, int width, int italic) { ttf_spec s; ttf_style_spec(&s, "RVStd", weight, width, italic); return s; }

TEST(r2_variable_fonts_std_static_has_four_axes)
{
    DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n, i;
    STATIC_BEGIN(styled(400, 5, 0));
    get_axes(&x, v, r, &n);
    EXPECT_EQ(n, 4);
    CHECK(rv_find_value(v, n, TAG_WGHT) >= 0); CHECK(rv_find_value(v, n, TAG_WDTH) >= 0); CHECK(rv_find_value(v, n, TAG_ITAL) >= 0); CHECK(rv_find_value(v, n, TAG_SLNT) >= 0);
    for (i = 0; i < n; i++) {
        CHECKF(r[i].minValue == v[i].value && r[i].maxValue == v[i].value, "axis %u range [%g, %g] default %g", i, r[i].minValue, r[i].maxValue, v[i].value);
        EXPECT_EQ(IDWriteFontResource_GetFontAxisAttributes(x.res, i), DWRITE_FONT_AXIS_ATTRIBUTES_NONE);
    }
    CHECK(IDWriteFontResource_HasVariations(x.res) == FALSE);
    STATIC_END();
}
TEST(r2_variable_fonts_std_static_weight_axis)
{
    static const int w[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900 }; unsigned i;
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k;
        STATIC_BEGIN(styled(w[i], 5, 0));
        get_axes(&x, v, r, &n);
        k = rv_find_value(v, n, TAG_WGHT);
        CHECKF(k >= 0 && v[k].value == (float)w[i], "usWeightClass %d -> %g", w[i], k >= 0 ? v[k].value : -1.0f);
        STATIC_END();
    }
}
TEST(r2_variable_fonts_std_static_width_normal_is_100)
{
    DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k;
    STATIC_BEGIN(styled(400, 5, 0));
    get_axes(&x, v, r, &n);
    k = rv_find_value(v, n, TAG_WDTH);
    CHECK(k >= 0 && v[k].value == 100.0f);
    STATIC_END();
}
TEST(r2_variable_fonts_std_static_width_is_increasing_with_stretch)
{
    float prev = 0; int w;
    for (w = 1; w <= 9; w++) {
        DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k;
        STATIC_BEGIN(styled(400, w, 0));
        get_axes(&x, v, r, &n);
        k = rv_find_value(v, n, TAG_WDTH);
        if (k >= 0) { t_note("usWidthClass %d -> wdth %g", w, v[k].value); CHECKF(v[k].value > prev, "usWidthClass %d: %g not above %g", w, v[k].value, prev); prev = v[k].value; }
        else CHECK(0);
        STATIC_END();
    }
}
TEST(r2_variable_fonts_std_static_ital_axis)
{
    int it;
    for (it = 0; it <= 1; it++) {
        DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k;
        STATIC_BEGIN(styled(400, 5, it));
        get_axes(&x, v, r, &n);
        k = rv_find_value(v, n, TAG_ITAL);
        CHECKF(k >= 0 && v[k].value == (float)it, "italic=%d -> ital %g", it, k >= 0 ? v[k].value : -1.0f);
        STATIC_END();
    }
}
TEST(r2_variable_fonts_std_static_slnt_from_post_angle)
{
    static const int ang[] = { 0, -12 << 16, -20 << 16, 5 << 16, -(int)(20.5 * 65536), (int)(0.25 * 65536) }; unsigned i;
    for (i = 0; i < sizeof ang / sizeof ang[0]; i++) {
        DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k; ttf_spec sp = styled(400, 5, 0);
        sp.italic_angle_fixed = ang[i];
        STATIC_BEGIN(sp);
        get_axes(&x, v, r, &n);
        k = rv_find_value(v, n, TAG_SLNT);
        CHECKF(k >= 0 && v[k].value == (float)ang[i] / 65536.0f, "post italicAngle %g -> slnt %g", (float)ang[i] / 65536.0f, k >= 0 ? v[k].value : -999.0f);
        STATIC_END();
    }
}
TEST(r2_variable_fonts_std_static_slnt_without_post_follows_style)
{
    int it;
    for (it = 0; it <= 1; it++) {
        DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k; ttf_spec sp = styled(400, 5, it);
        sp.omit |= TTF_OMIT_POST;
        STATIC_BEGIN(sp);
        get_axes(&x, v, r, &n);
        k = rv_find_value(v, n, TAG_SLNT);
        CHECKF(k >= 0 && v[k].value == (it ? -12.0f : 0.0f), "italic=%d, no post -> slnt %g", it, k >= 0 ? v[k].value : -999.0f);
        STATIC_END();
    }
}
TEST(r2_variable_fonts_std_variable_font_adds_only_missing)
{
    DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k;
    RV_BEGIN(0);
    get_axes(&x, v, r, &n);
    EXPECT_EQ(n, RV_NALL);
    k = rv_find_value(v, n, TAG_ITAL); CHECK(k >= 0 && v[k].value == 0.0f && r[k].minValue == 0.0f && r[k].maxValue == 0.0f);
    k = rv_find_value(v, n, TAG_SLNT); CHECK(k >= 0 && v[k].value == 0.0f && r[k].minValue == 0.0f && r[k].maxValue == 0.0f);
    k = rv_find_value(v, n, TAG_WGHT); CHECK(k >= 0 && r[k].minValue == 100.0f && r[k].maxValue == 900.0f);       /* the fvar axis, not a standard one */
    RV_END();
}
TEST(r2_variable_fonts_std_fvar_slnt_replaces_standard)
{
    rv_axis a[1] = { { "slnt", -30, 0, 10, 0, 256, "Slant" } };
    size_t sz; uint8_t *b = rv_build_axes(a, 1, &sz); rv x; DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k;
    if (!b) { t_skip("font build failed"); return; }
    if (!rv_open(&x, b, sz)) { rv_close(&x); free(b); return; }
    get_axes(&x, v, r, &n);
    EXPECT_EQ(n, 4);
    k = rv_find_value(v, n, TAG_SLNT);
    CHECK(k >= 0 && r[k].minValue == -30.0f && r[k].maxValue == 10.0f && v[k].value == 0.0f);
    if (k >= 0) EXPECT_EQ(IDWriteFontResource_GetFontAxisAttributes(x.res, (UINT32)k), DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE);
    rv_close(&x); free(b);
}
TEST(r2_variable_fonts_std_italic_variable_font)
{
    /* a variable font that is an italic style: ital 1 as a fixed standard axis */
    rv_axis a[1] = { { "wght", 100, 400, 900, 0, 256, "Weight" } };
    size_t sz; uint8_t *b; rv x; DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n; int k;
    ttf_spec s; unsigned l = 16 + 20, len;
    uint8_t fv[16 + 20]; (void)l; (void)len;
    memset(fv, 0, sizeof fv);
    fv[1] = 1; fv[5] = 16; fv[7] = 2; fv[9] = 1; fv[11] = 20; fv[13] = 0; fv[15] = 4;
    memcpy(fv + 16, "wght", 4);
    { uint32_t mn = 100 << 16, df = 400 << 16, mx = 900 << 16; int j; for (j = 0; j < 4; j++) { fv[20 + j] = (uint8_t)(mn >> (24 - 8 * j)); fv[24 + j] = (uint8_t)(df >> (24 - 8 * j)); fv[28 + j] = (uint8_t)(mx >> (24 - 8 * j)); } }
    ttf_style_spec(&s, "RVStd", 400, 5, 1);
    { ttf_rawtable raw[1]; memset(raw, 0, sizeof raw); memcpy(raw[0].tag, "fvar", 5); raw[0].data = fv; raw[0].len = sizeof fv; s.raw = raw; s.nraw = 1; b = ttf_build(&s, &sz); }
    (void)a;
    if (!b) { t_skip("font build failed"); return; }
    if (!rv_open(&x, b, sz)) { rv_close(&x); free(b); return; }
    get_axes(&x, v, r, &n);
    k = rv_find_value(v, n, TAG_ITAL);
    CHECK(k >= 0 && v[k].value == 1.0f);
    rv_close(&x); free(b);
}
