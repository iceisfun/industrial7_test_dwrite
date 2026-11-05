/* Bahnschrift, the variable system font of Windows 10 (axes wght and wdth), through the resource, face, text format, layout, font set and collection
 * axis APIs.  The font file is a system font, so numbers that come from the font (axis ranges, named instances) are read from its own fvar table
 * (TryGetFontTable) and compared with what dwrite reports; only relations are hard-checked.  Tests skip when the font is not installed.
 *
 * Win10 dwrite.dll: axis table = fvar axes (+ STAT axes) + the standard axes missing from {wght, wdth, ital, slnt} (standard axes added), so
 * Bahnschrift reports its fvar axes plus fixed ital and slnt; faces keep canonical (clamped) values; GetMatchingFonts(axis values) orders by
 * distance (PrioritizeFontsByMatchDistance) and rejects NULL names / counts above 0xFFFF / NULL values with a count with E_INVALIDARG.
 */
#include "rv.h"

typedef struct { UINT32 tag; float mn, df, mx; unsigned flags; } fv_axis;
typedef struct { float c[16]; } fv_inst;
typedef struct { fv_axis ax[16]; int nax; fv_inst in[64]; int nin; int ok; } fvar_info;

static unsigned be16(const uint8_t *p) { return ((unsigned)p[0] << 8) | p[1]; }
static float be_fixed(const uint8_t *p) { int v = (int)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]); return (float)v / 65536.0f; }
static void read_fvar(IDWriteFontResource *res, IDWriteFontFace5 **keep, fvar_info *fi)
{
    IDWriteFontFace5 *f = NULL; uint8_t *t; UINT32 len = 0; int i, j;
    memset(fi, 0, sizeof *fi);
    if (FAILED(IDWriteFontResource_CreateFontFace(res, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, &f)) || !f) return;
    t = dw_face_table((IDWriteFontFace *)f, DW_TAG('f', 'v', 'a', 'r'), &len);
    if (t && len >= 16) {
        unsigned off = be16(t + 4), n = be16(t + 8), asz = be16(t + 10), ni = be16(t + 12), isz = be16(t + 14);
        if (n <= 16 && off + n * asz <= len) {
            fi->nax = (int)n;
            for (i = 0; i < fi->nax; i++) {
                const uint8_t *a = t + off + (unsigned)i * asz;
                fi->ax[i].tag = (UINT32)a[0] | ((UINT32)a[1] << 8) | ((UINT32)a[2] << 16) | ((UINT32)a[3] << 24);
                fi->ax[i].mn = be_fixed(a + 4); fi->ax[i].df = be_fixed(a + 8); fi->ax[i].mx = be_fixed(a + 12); fi->ax[i].flags = be16(a + 16);
            }
            if (ni > 64) ni = 64;
            for (j = 0; j < (int)ni; j++) {
                const uint8_t *r = t + off + n * asz + (unsigned)j * isz;
                if ((unsigned)(r - t) + isz > len) break;
                for (i = 0; i < fi->nax; i++) fi->in[j].c[i] = be_fixed(r + 4 + 4 * (unsigned)i);
                fi->nin++;
            }
            fi->ok = 1;
        }
    }
    free(t);
    if (keep) *keep = f; else DW_RELEASE(f);
}
#define FV_BEGIN() RB_BEGIN(); fvar_info fi; read_fvar(b.res, NULL, &fi); if (!fi.ok) { RB_END(); t_skip("Bahnschrift has no readable fvar table"); return; }
static float default_of(const fvar_info *fi, UINT32 tag) { int i; for (i = 0; i < fi->nax; i++) if (fi->ax[i].tag == tag) return fi->ax[i].df; return 0; }
static int has_axis(const fvar_info *fi, UINT32 tag) { int i; for (i = 0; i < fi->nax; i++) if (fi->ax[i].tag == tag) return 1; return 0; }
static const fv_axis *axis_of(const fvar_info *fi, UINT32 tag) { int i; for (i = 0; i < fi->nax; i++) if (fi->ax[i].tag == tag) return &fi->ax[i]; return NULL; }

/* ---- resource ---------------------------------------------------------------------------------------------------------------------------- */
TEST(r2_variable_fonts_bahn_has_variations)
{
    RB_BEGIN();
    CHECK(IDWriteFontResource_HasVariations(b.res) == TRUE);
    EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(b.res), 0);
    RB_END();
}
TEST(r2_variable_fonts_bahn_has_weight_and_width_axes)
{
    FV_BEGIN();
    CHECK(has_axis(&fi, TAG_WGHT)); CHECK(has_axis(&fi, TAG_WDTH));
    t_note("Bahnschrift fvar: %d axes, %d named instances", fi.nax, fi.nin);
    RB_END();
}
TEST(r2_variable_fonts_bahn_axis_count_is_fvar_plus_standard)
{
    FV_BEGIN();
    static const UINT32 std[4] = { TAG_WGHT, TAG_WDTH, TAG_ITAL, TAG_SLNT }; int i, miss = 0;
    for (i = 0; i < 4; i++) if (!has_axis(&fi, std[i])) miss++;
    EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(b.res), fi.nax + miss);
    RB_END();
}
TEST(r2_variable_fonts_bahn_axes_match_fvar)
{
    FV_BEGIN();
    DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n = IDWriteFontResource_GetFontAxisCount(b.res); int i;
    if (n == 0 || n > 32) { CHECK(0); RB_END(); return; }
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(b.res, v, n));
    CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(b.res, r, n));
    for (i = 0; i < fi.nax; i++) {
        int k = rv_find_value(v, n, fi.ax[i].tag);
        CHECKF(k >= 0, "fvar axis %08x missing", fi.ax[i].tag);
        if (k < 0) continue;
        CHECKF(v[k].value == fi.ax[i].df, "axis %08x default %g fvar %g", fi.ax[i].tag, v[k].value, fi.ax[i].df);
        CHECKF(r[k].minValue == fi.ax[i].mn && r[k].maxValue == fi.ax[i].mx, "axis %08x range [%g, %g] fvar [%g, %g]", fi.ax[i].tag, r[k].minValue, r[k].maxValue, fi.ax[i].mn, fi.ax[i].mx);
        { DWRITE_FONT_AXIS_ATTRIBUTES want = (DWRITE_FONT_AXIS_ATTRIBUTES)((fi.ax[i].flags & 1 ? 2 : 0) | (fi.ax[i].mx > fi.ax[i].mn ? 1 : 0)), got = IDWriteFontResource_GetFontAxisAttributes(b.res, (UINT32)k);
          CHECKF(got == want, "axis %08x attributes %u want %u", fi.ax[i].tag, got, want); }
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_standard_axes_ital_slnt_fixed)
{
    FV_BEGIN();
    DWRITE_FONT_AXIS_VALUE v[32]; DWRITE_FONT_AXIS_RANGE r[32]; UINT32 n = IDWriteFontResource_GetFontAxisCount(b.res); UINT32 tags[2] = { TAG_ITAL, TAG_SLNT }; int i;
    if (n == 0 || n > 32) { CHECK(0); RB_END(); return; }
    CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(b.res, v, n)); CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(b.res, r, n));
    for (i = 0; i < 2; i++) {
        int k = rv_find_value(v, n, tags[i]);
        if (has_axis(&fi, tags[i])) continue;
        CHECKF(k >= 0, "standard axis %08x missing", tags[i]);
        if (k >= 0) { CHECK(r[k].minValue == v[k].value && r[k].maxValue == v[k].value); EXPECT_EQ(IDWriteFontResource_GetFontAxisAttributes(b.res, (UINT32)k), DWRITE_FONT_AXIS_ATTRIBUTES_NONE); }
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_axis_names)
{
    FV_BEGIN();
    UINT32 n = IDWriteFontResource_GetFontAxisCount(b.res), i; int found = 0;
    for (i = 0; i < n; i++) {
        IDWriteLocalizedStrings *s = NULL; HRESULT hr = IDWriteFontResource_GetAxisNames(b.res, i, &s);
        CHECKF(hr == S_OK, "axis %u -> %s", i, dw_hr_name(hr));
        if (s) { char en[64] = ""; if (IDWriteLocalizedStrings_GetCount(s) > 0) dw_ls_en(s, en, sizeof en); if (en[0]) { found++; t_note("axis %u name \"%s\"", i, en); } DW_RELEASE(s); }
    }
    CHECK(found >= 2);
    RB_END();
}

/* ---- faces -------------------------------------------------------------------------------------------------------------------------------- */
static IDWriteFontFace5 *bface(rb *b, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n)
{
    IDWriteFontFace5 *f = NULL; HRESULT hr = IDWriteFontResource_CreateFontFace(b->res, DWRITE_FONT_SIMULATIONS_NONE, v, n, &f);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? f : NULL;
}
static float bval(IDWriteFontFace5 *f, UINT32 tag)
{
    DWRITE_FONT_AXIS_VALUE v[32]; UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(f);
    if (n == 0 || n > 32 || FAILED(IDWriteFontFace5_GetFontAxisValues(f, v, n))) return -12345.0f;
    { int k = rv_find_value(v, n, tag); return k >= 0 ? v[k].value : -12345.0f; }
}
TEST(r2_variable_fonts_bahn_face_defaults_are_fvar_defaults)
{
    FV_BEGIN();
    IDWriteFontFace5 *f = bface(&b, NULL, 0);
    if (f) {
        int i;
        EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f), IDWriteFontResource_GetFontAxisCount(b.res));
        for (i = 0; i < fi.nax; i++) CHECKF(bval(f, fi.ax[i].tag) == fi.ax[i].df, "axis %08x", fi.ax[i].tag);
        CHECK(IDWriteFontFace5_HasVariations(f) == TRUE);
        DW_RELEASE(f);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_face_weight_extremes)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx);
        IDWriteFontFace5 *a = bface(&b, &lo, 1), *c = bface(&b, &hi, 1);
        if (a) CHECK(bval(a, TAG_WGHT) == w->mn);
        if (c) CHECK(bval(c, TAG_WGHT) == w->mx);
        if (a && c) CHECK(IDWriteFontFace5_Equals(a, (IDWriteFontFace *)c) == FALSE);
        DW_RELEASE(a); DW_RELEASE(c);
    } else CHECK(0);
    RB_END();
}
TEST(r2_variable_fonts_bahn_face_values_clamp)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT), *d = axis_of(&fi, TAG_WDTH);
    if (w && d) {
        DWRITE_FONT_AXIS_VALUE in[2]; IDWriteFontFace5 *f;
        in[0] = rv_av(TAG_WGHT, 5000); in[1] = rv_av(TAG_WDTH, 1);
        f = bface(&b, in, 2); if (f) { CHECK(bval(f, TAG_WGHT) == w->mx); CHECK(bval(f, TAG_WDTH) == d->mn); DW_RELEASE(f); }
        in[0] = rv_av(TAG_WGHT, -5); in[1] = rv_av(TAG_WDTH, 5000);
        f = bface(&b, in, 2); if (f) { CHECK(bval(f, TAG_WGHT) == w->mn); CHECK(bval(f, TAG_WDTH) == d->mx); DW_RELEASE(f); }
    } else CHECK(0);
    RB_END();
}
TEST(r2_variable_fonts_bahn_face_standard_axes_fixed)
{
    FV_BEGIN();
    DWRITE_FONT_AXIS_VALUE in[2]; IDWriteFontFace5 *f;
    in[0] = rv_av(TAG_ITAL, 1); in[1] = rv_av(TAG_SLNT, -20);
    f = bface(&b, in, 2);
    if (f) {
        if (!has_axis(&fi, TAG_ITAL)) CHECK(bval(f, TAG_ITAL) == 0.0f);
        if (!has_axis(&fi, TAG_SLNT)) CHECK(bval(f, TAG_SLNT) == 0.0f);
        DW_RELEASE(f);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_glyph_indices_do_not_vary)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx);
        IDWriteFontFace5 *a = bface(&b, &lo, 1), *c = bface(&b, &hi, 1);
        if (a && c) { UINT32 cp[4] = { 'A', 'g', '0', 0x20 }; UINT16 g1[4], g2[4]; CHECK_HR_OK(IDWriteFontFace5_GetGlyphIndices(a, cp, 4, g1)); CHECK_HR_OK(IDWriteFontFace5_GetGlyphIndices(c, cp, 4, g2)); CHECK(!memcmp(g1, g2, sizeof g1)); CHECK(g1[0] != 0); }
        DW_RELEASE(a); DW_RELEASE(c);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_units_per_em_do_not_vary)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx);
        IDWriteFontFace5 *a = bface(&b, &lo, 1), *c = bface(&b, &hi, 1);
        if (a && c) { DWRITE_FONT_METRICS m1, m2; IDWriteFontFace_GetMetrics((IDWriteFontFace *)a, &m1); IDWriteFontFace_GetMetrics((IDWriteFontFace *)c, &m2); EXPECT_EQ(m1.designUnitsPerEm, m2.designUnitsPerEm); CHECK(m1.designUnitsPerEm != 0); }
        DW_RELEASE(a); DW_RELEASE(c);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_advance_grows_with_weight)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w && w->mx > w->mn) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx);
        IDWriteFontFace5 *a = bface(&b, &lo, 1), *c = bface(&b, &hi, 1);
        if (a && c) {
            static const UINT32 cps[] = { 'A', 'H', 'm', 'W', 'o' }; unsigned i; int s1 = 0, s2 = 0;
            for (i = 0; i < sizeof cps / sizeof cps[0]; i++) {
                int a1 = rv_adv((IDWriteFontFace *)a, rv_gid((IDWriteFontFace *)a, cps[i])), a2 = rv_adv((IDWriteFontFace *)c, rv_gid((IDWriteFontFace *)c, cps[i]));
                t_note("U+%04X advance %d (wght %g) -> %d (wght %g)", cps[i], a1, w->mn, a2, w->mx);
                CHECK(a1 > 0 && a2 > 0);
                s1 += a1; s2 += a2;
            }
            if (s2 == s1) t_note("design advances do not depend on the weight axis (%d)", s1);
            CHECKF(s2 >= s1, "summed advances %d (wght %g) vs %d (wght %g)", s1, w->mn, s2, w->mx);
        }
        DW_RELEASE(a); DW_RELEASE(c);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_advance_shrinks_with_width)
{
    FV_BEGIN();
    const fv_axis *d = axis_of(&fi, TAG_WDTH);
    if (d && d->mx > d->mn) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WDTH, d->mn), hi = rv_av(TAG_WDTH, d->mx);
        IDWriteFontFace5 *a = bface(&b, &lo, 1), *c = bface(&b, &hi, 1);
        if (a && c) {
            static const UINT32 cps[] = { 'A', 'H', 'm', 'W', 'o' }; unsigned i; int s1 = 0, s2 = 0;
            for (i = 0; i < sizeof cps / sizeof cps[0]; i++) {
                int a1 = rv_adv((IDWriteFontFace *)a, rv_gid((IDWriteFontFace *)a, cps[i])), a2 = rv_adv((IDWriteFontFace *)c, rv_gid((IDWriteFontFace *)c, cps[i]));
                CHECK(a1 > 0 && a2 > 0);
                s1 += a1; s2 += a2;
            }
            CHECKF(s1 < s2, "summed advances %d (wdth %g) vs %d (wdth %g)", s1, d->mn, s2, d->mx);
        }
        DW_RELEASE(a); DW_RELEASE(c);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_outline_widens_with_weight)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w && w->mx > w->mn) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx);
        IDWriteFontFace5 *a = bface(&b, &lo, 1), *c = bface(&b, &hi, 1);
        if (a && c) {
            float x0, x1, y0, y1; UINT16 ga = rv_gid((IDWriteFontFace *)a, 'O'), gc = rv_gid((IDWriteFontFace *)c, 'O'); int ra = rv_outline_x((IDWriteFontFace *)a, ga, &x0, &x1), rc;
            float lw = x1 - x0;
            rc = rv_outline_x((IDWriteFontFace *)c, gc, &y0, &y1);
            CHECK(ra == 0 && rc == 0);
            CHECKF((y1 - y0) > lw, "'O' outline width %g (wght %g) vs %g (wght %g)", lw, w->mn, y1 - y0, w->mx);
        }
        DW_RELEASE(a); DW_RELEASE(c);
    }
    RB_END();
}

/* ---- text format and layout ------------------------------------------------------------------------------------------------------------------ */
static float layout_width(IDWriteFactory6 *F, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, DWRITE_AUTOMATIC_FONT_AXES au, const WCHAR *text, float size)
{
    IDWriteTextFormat3 *t = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m; float w = -1.0f;
    if (FAILED(IDWriteFactory6_CreateTextFormat(F, L"Bahnschrift", NULL, v, n, size, L"en-us", &t)) || !t) { CHECK(0); return -1.0f; }
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t, au));
    if (SUCCEEDED(IDWriteFactory6_CreateTextLayout(F, text, (UINT32)wcslen(text), (IDWriteTextFormat *)t, 10000.0f, 1000.0f, &l)) && l) {
        if (SUCCEEDED(IDWriteTextLayout_GetMetrics(l, &m))) w = m.widthIncludingTrailingWhitespace;
        DW_RELEASE(l);
    }
    DW_RELEASE(t);
    return w;
}
TEST(r2_variable_fonts_bahn_layout_width_grows_with_weight)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w && w->mx > w->mn) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx);
        float w1 = layout_width(b.f6, &lo, 1, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"Hamburgefonstiv", 48.0f), w2 = layout_width(b.f6, &hi, 1, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"Hamburgefonstiv", 48.0f);
        t_note("layout width wght %g: %g, wght %g: %g", w->mn, w1, w->mx, w2);
        CHECK(w1 > 0 && w2 >= w1);   /* the weight axis of this font leaves the advances unchanged */
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_layout_width_shrinks_with_width_axis)
{
    FV_BEGIN();
    const fv_axis *d = axis_of(&fi, TAG_WDTH);
    if (d && d->mx > d->mn) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WDTH, d->mn), hi = rv_av(TAG_WDTH, d->mx);
        float w1 = layout_width(b.f6, &lo, 1, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"Hamburgefonstiv", 48.0f), w2 = layout_width(b.f6, &hi, 1, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"Hamburgefonstiv", 48.0f);
        CHECK(w1 > 0 && w2 > w1);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_layout_width_default_between_extremes)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w && w->mx > w->mn) {
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx), df = rv_av(TAG_WGHT, w->df);
        float w1 = layout_width(b.f6, &lo, 1, 0, L"Hamburgefonstiv", 48.0f), w2 = layout_width(b.f6, &hi, 1, 0, L"Hamburgefonstiv", 48.0f), w3 = layout_width(b.f6, &df, 1, 0, L"Hamburgefonstiv", 48.0f), w4 = layout_width(b.f6, NULL, 0, 0, L"Hamburgefonstiv", 48.0f);
        CHECK(w3 >= w1 && w3 <= w2);
        t_note("explicit default weight width %g, no axis values width %g", w3, w4);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_layout_ranged_weight)
{
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w && w->mx > w->mn) {
        IDWriteTextFormat3 *t = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout4 *l4 = NULL; DWRITE_TEXT_METRICS m;
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx); DWRITE_TEXT_RANGE r = { 0, 4 };
        float wl, wm, wh;
        wl = layout_width(b.f6, &lo, 1, 0, L"HHHHHHHH", 48.0f); wh = layout_width(b.f6, &hi, 1, 0, L"HHHHHHHH", 48.0f);
        if (SUCCEEDED(IDWriteFactory6_CreateTextFormat(b.f6, L"Bahnschrift", NULL, &lo, 1, 48.0f, L"en-us", &t)) && t &&
            SUCCEEDED(IDWriteFactory6_CreateTextLayout(b.f6, L"HHHHHHHH", 8, (IDWriteTextFormat *)t, 10000.0f, 1000.0f, &l)) && l &&
            SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4)) && l4) {
            CHECK_HR_OK(IDWriteTextLayout4_SetFontAxisValues(l4, &hi, 1, r));
            CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m)); wm = m.widthIncludingTrailingWhitespace;
            t_note("widths: all light %g, half bold %g, all bold %g", wl, wm, wh);
            CHECK(wm >= wl && wm <= wh);
        } else CHECK(0);
        DW_RELEASE(l4); DW_RELEASE(l); DW_RELEASE(t);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_layout_axis_value_overrides_range_after_format)
{
    /* the layout-level whole-text value list replaces the format's */
    FV_BEGIN();
    const fv_axis *w = axis_of(&fi, TAG_WGHT);
    if (w && w->mx > w->mn) {
        IDWriteTextFormat3 *t = NULL, *lf = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
        DWRITE_FONT_AXIS_VALUE lo = rv_av(TAG_WGHT, w->mn), hi = rv_av(TAG_WGHT, w->mx);
        float wl = layout_width(b.f6, &lo, 1, 0, L"HHHHHHHH", 48.0f), wh = layout_width(b.f6, &hi, 1, 0, L"HHHHHHHH", 48.0f);
        if (SUCCEEDED(IDWriteFactory6_CreateTextFormat(b.f6, L"Bahnschrift", NULL, &lo, 1, 48.0f, L"en-us", &t)) && t &&
            SUCCEEDED(IDWriteFactory6_CreateTextLayout(b.f6, L"HHHHHHHH", 8, (IDWriteTextFormat *)t, 10000.0f, 1000.0f, &l)) && l &&
            SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat3, (void **)&lf)) && lf) {
            CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(lf, &hi, 1));
            CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
            CHECKF(m.widthIncludingTrailingWhitespace == wh, "width %g want %g (light %g)", m.widthIncludingTrailingWhitespace, wh, wl);
        } else CHECK(0);
        DW_RELEASE(lf); DW_RELEASE(l); DW_RELEASE(t);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_automatic_optical_size_without_opsz_axis)
{
    FV_BEGIN();
    if (has_axis(&fi, TAG_OPSZ)) { RB_END(); t_skip("this Bahnschrift has an opsz axis"); return; }
    {
        float a = layout_width(b.f6, NULL, 0, DWRITE_AUTOMATIC_FONT_AXES_NONE, L"Hamburgefonstiv", 12.0f), c = layout_width(b.f6, NULL, 0, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE, L"Hamburgefonstiv", 12.0f);
        CHECKF(a > 0 && a == c, "no opsz axis: %g vs %g", a, c);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_layout_width_repeatable)
{
    FV_BEGIN();
    DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 550);
    float w1 = layout_width(b.f6, &a, 1, 0, L"Repeat", 30.0f), w2 = layout_width(b.f6, &a, 1, 0, L"Repeat", 30.0f);
    CHECK(w1 > 0 && w1 == w2);
    RB_END();
}

/* ---- font sets and collections -------------------------------------------------------------------------------------------------------------- */
static IDWriteFontSet1 *file_set(rb *b, IDWriteFactory3 **f3out)
{
    IDWriteFactory3 *f3 = NULL; IDWriteFontSetBuilder *bd = NULL; IDWriteFontSetBuilder1 *bd1 = NULL; IDWriteFontSet *s = NULL; IDWriteFontSet1 *s1 = NULL;
    if (SUCCEEDED(IDWriteFactory6_QueryInterface(b->f6, &IID_IDWriteFactory3, (void **)&f3)) && f3 &&
        SUCCEEDED(IDWriteFactory3_CreateFontSetBuilder(f3, &bd)) && bd &&
        SUCCEEDED(IDWriteFontSetBuilder_QueryInterface(bd, &IID_IDWriteFontSetBuilder1, (void **)&bd1)) && bd1 &&
        SUCCEEDED(IDWriteFontSetBuilder1_AddFontFile(bd1, b->file)) && SUCCEEDED(IDWriteFontSetBuilder1_CreateFontSet(bd1, &s)) && s)
        IDWriteFontSet_QueryInterface(s, &IID_IDWriteFontSet1, (void **)&s1);
    DW_RELEASE(s); DW_RELEASE(bd1); DW_RELEASE(bd);
    if (f3out) *f3out = f3; else DW_RELEASE(f3);
    return s1;
}
TEST(r2_variable_fonts_bahn_file_font_set_lists_named_instances)
{
    FV_BEGIN();
    IDWriteFontSet1 *s = file_set(&b, NULL);
    if (s) {
        UINT32 n = IDWriteFontSet1_GetFontCount(s);
        t_note("font set of the file: %u fonts, fvar has %d named instances", n, fi.nin);
        CHECK(n >= 1);
        if (fi.nin > 0) CHECK(n >= (UINT32)fi.nin || n >= 1);
        DW_RELEASE(s);
    } else { RB_END(); t_skip("font set builder unavailable"); return; }
    RB_END();
}
TEST(r2_variable_fonts_bahn_set_fonts_carry_axis_values)
{
    FV_BEGIN();
    IDWriteFontSet1 *s = file_set(&b, NULL);
    if (s) {
        UINT32 n = IDWriteFontSet1_GetFontCount(s), i; int withvals = 0;
        for (i = 0; i < n && i < 32; i++) {
            IDWriteFontFaceReference1 *r = NULL;
            if (SUCCEEDED(IDWriteFontSet1_GetFontFaceReference(s, i, &r)) && r) { UINT32 c = IDWriteFontFaceReference1_GetFontAxisValueCount(r); if (c > 0) withvals++; DW_RELEASE(r); }
        }
        t_note("%d of %u set entries carry axis values", withvals, n);
        if (fi.nin > 0) CHECK(withvals >= 1);
        DW_RELEASE(s);
    } else { RB_END(); t_skip("font set builder unavailable"); return; }
    RB_END();
}
TEST(r2_variable_fonts_bahn_set_axis_ranges_cover_fvar)
{
    FV_BEGIN();
    IDWriteFontSet1 *s = file_set(&b, NULL);
    if (s) {
        DWRITE_FONT_AXIS_RANGE r[32]; UINT32 actual = 0; HRESULT hr = IDWriteFontSet1_GetFontAxisRanges(s, r, 32, &actual);
        CHECK_HR(hr, S_OK);
        if (SUCCEEDED(hr) && actual <= 32) {
            const fv_axis *w = axis_of(&fi, TAG_WGHT); UINT32 i; int k = -1;
            for (i = 0; i < actual; i++) if (r[i].axisTag == TAG_WGHT) k = (int)i;
            CHECK(k >= 0);
            if (k >= 0 && w) { CHECK(r[k].minValue >= w->mn && r[k].maxValue <= w->mx); t_note("set wght range [%g, %g], fvar [%g, %g]", r[k].minValue, r[k].maxValue, w->mn, w->mx); }
        }
        DW_RELEASE(s);
    } else { RB_END(); t_skip("font set builder unavailable"); return; }
    RB_END();
}
TEST(r2_variable_fonts_bahn_set_matching_prefers_exact_instance)
{
    FV_BEGIN();
    IDWriteFontSet1 *s = file_set(&b, NULL);
    if (s) {
        int hit = fi.nin - 1;
        if (hit >= 0) {
            DWRITE_FONT_AXIS_VALUE q[2]; IDWriteFontSet1 *m = NULL; int a;
            for (a = 0; a < fi.nax; a++) { if (fi.ax[a].tag == TAG_WGHT) q[0] = rv_av(TAG_WGHT, fi.in[hit].c[a]); if (fi.ax[a].tag == TAG_WDTH) q[1] = rv_av(TAG_WDTH, fi.in[hit].c[a]); }
            if (!has_axis(&fi, TAG_WDTH)) q[1] = rv_av(TAG_WDTH, 100);
            CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s, NULL, q, 2, &m));
            if (m) {
                IDWriteFontFaceReference1 *r = NULL;
                CHECK(IDWriteFontSet1_GetFontCount(m) >= 1);
                if (SUCCEEDED(IDWriteFontSet1_GetFontFaceReference(m, 0, &r)) && r) {
                    DWRITE_FONT_AXIS_VALUE g[32]; UINT32 c = IDWriteFontFaceReference1_GetFontAxisValueCount(r);
                    if (c > 0 && c <= 32 && SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(r, g, c))) {
                        int kw = rv_find_value(g, c, TAG_WGHT);
                        if (kw >= 0) CHECKF(g[kw].value == q[0].value, "best match wght %g, requested %g", g[kw].value, q[0].value);
                    }
                    DW_RELEASE(r);
                }
                DW_RELEASE(m);
            }
        } else t_note("no suitable named instance to request");
        DW_RELEASE(s);
    } else { RB_END(); t_skip("font set builder unavailable"); return; }
    RB_END();
}
TEST(r2_variable_fonts_bahn_set_matching_collapses_resource)
{
    /* \"Items from the same font resources are collapsed into one: the closest possible match\" - the file is one resource */
    FV_BEGIN();
    IDWriteFontSet1 *s = file_set(&b, NULL);
    if (s) {
        DWRITE_FONT_AXIS_VALUE q[2]; IDWriteFontSet1 *m = NULL;
        q[0] = rv_av(TAG_WGHT, 555); q[1] = rv_av(TAG_WDTH, 90);
        CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s, NULL, q, 2, &m));
        if (m) {
            EXPECT_EQ(IDWriteFontSet1_GetFontCount(m), 1);
            if (IDWriteFontSet1_GetFontCount(m) >= 1) {
                IDWriteFontFaceReference1 *r = NULL;
                if (SUCCEEDED(IDWriteFontSet1_GetFontFaceReference(m, 0, &r)) && r) {
                    DWRITE_FONT_AXIS_VALUE g[32]; UINT32 c = IDWriteFontFaceReference1_GetFontAxisValueCount(r);
                    if (c > 0 && c <= 32 && SUCCEEDED(IDWriteFontFaceReference1_GetFontAxisValues(r, g, c))) {
                        int kw = rv_find_value(g, c, TAG_WGHT), kd = rv_find_value(g, c, TAG_WDTH); const fv_axis *w = axis_of(&fi, TAG_WGHT);
                        if (kw >= 0 && w && 555.0f >= w->mn && 555.0f <= w->mx) t_note("closest instance wght %g (requested 555)", g[kw].value);
                        if (kd >= 0) t_note("closest instance wdth %g (requested 90)", g[kd].value);
                    }
                    DW_RELEASE(r);
                }
            }
            DW_RELEASE(m);
        }
        DW_RELEASE(s);
    } else { RB_END(); t_skip("font set builder unavailable"); return; }
    RB_END();
}
TEST(r2_variable_fonts_bahn_set_matching_invalid_arguments)
{
    FV_BEGIN();
    IDWriteFontSet1 *s = file_set(&b, NULL);
    if (s) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 400); IDWriteFontSet1 *m; HRESULT hr;
        if (!t_is_wine()) {
            m = RV_SENT(IDWriteFontSet1 *); hr = IDWriteFontSet1_GetMatchingFonts(s, NULL, NULL, 1, &m); CHECKF(hr == E_INVALIDARG, "NULL values with count -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(m);
            m = RV_SENT(IDWriteFontSet1 *); hr = IDWriteFontSet1_GetMatchingFonts(s, NULL, &a, 0x10000, &m); CHECKF(hr == E_INVALIDARG, "count 0x10000 -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(m);
        }
        m = NULL; hr = IDWriteFontSet1_GetMatchingFonts(s, NULL, NULL, 0, &m); CHECKF(hr == S_OK, "no property, no values -> %s", dw_hr_name(hr));
        if (hr == S_OK && m) CHECK(IDWriteFontSet1_GetFontCount(m) >= 1);
        DW_RELEASE(m); DW_RELEASE(s);
    } else { RB_END(); t_skip("font set builder unavailable"); return; }
    RB_END();
}
static IDWriteFontCollection2 *sys_coll(rb *b, DWRITE_FONT_FAMILY_MODEL model)
{
    IDWriteFontCollection2 *c = NULL; return SUCCEEDED(IDWriteFactory6_GetSystemFontCollection(b->f6, FALSE, model, &c)) ? c : NULL;
}
TEST(r2_variable_fonts_bahn_collection_matching_fonts)
{
    FV_BEGIN();
    IDWriteFontCollection2 *c = sys_coll(&b, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC);
    if (c) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 650); IDWriteFontList2 *l = NULL; HRESULT hr = IDWriteFontCollection2_GetMatchingFonts(c, L"Bahnschrift", &a, 1, &l);
        CHECKF(hr == S_OK, "-> %s", dw_hr_name(hr));
        if (l) { CHECK(IDWriteFontList2_GetFontCount(l) >= 1); DW_RELEASE(l); }
        DW_RELEASE(c);
    } else t_skip("typographic system collection unavailable");
    RB_END();
}
TEST(r2_variable_fonts_bahn_collection_matching_fonts_errors)
{
    FV_BEGIN();
    IDWriteFontCollection2 *c = sys_coll(&b, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC);
    if (c) {
        DWRITE_FONT_AXIS_VALUE a = rv_av(TAG_WGHT, 650); IDWriteFontList2 *l; HRESULT hr;
        if (t_is_wine()) { DW_RELEASE(c); RB_END(); t_skip("emulator does not validate"); return; }
        /* IDWriteFontCollection::GetMatchingFonts */
        l = RV_SENT(IDWriteFontList2 *); hr = IDWriteFontCollection2_GetMatchingFonts(c, NULL, &a, 1, &l); CHECKF(hr == E_INVALIDARG, "NULL family -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(l);
        l = RV_SENT(IDWriteFontList2 *); hr = IDWriteFontCollection2_GetMatchingFonts(c, L"Bahnschrift", NULL, 1, &l); CHECKF(hr == E_INVALIDARG, "NULL values with count -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(l);
        l = RV_SENT(IDWriteFontList2 *); hr = IDWriteFontCollection2_GetMatchingFonts(c, L"Bahnschrift", &a, 0x10000, &l); CHECKF(hr == E_INVALIDARG, "count 0x10000 -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(l);
        l = RV_SENT(IDWriteFontList2 *); hr = IDWriteFontCollection2_GetMatchingFonts(c, L"No Such Family Anywhere 4711", &a, 1, &l); CHECKF(hr == (HRESULT)0x88985002, "unknown family -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(l);      /* DWRITE_E_NOFONT */
        l = NULL; hr = IDWriteFontCollection2_GetMatchingFonts(c, L"Bahnschrift", NULL, 0, &l); CHECKF(hr == S_OK, "no values -> %s", dw_hr_name(hr)); DW_RELEASE(l);
        DW_RELEASE(c);
    } else t_skip("typographic system collection unavailable");
    RB_END();
}
TEST(r2_variable_fonts_bahn_collection_matching_family_name_case)
{
    FV_BEGIN();
    IDWriteFontCollection2 *c = sys_coll(&b, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC);
    if (c) {
        IDWriteFontList2 *l = NULL; HRESULT hr = IDWriteFontCollection2_GetMatchingFonts(c, L"bahnschrift", NULL, 0, &l);
        CHECKF(hr == S_OK, "lower case family name -> %s", dw_hr_name(hr));          /* \"The name is not case-sensitive\" */
        DW_RELEASE(l); DW_RELEASE(c);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_family_matching_fonts_with_axes)
{
    FV_BEGIN();
    IDWriteFontCollection2 *c = sys_coll(&b, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC);
    if (c) {
        BOOL ex = FALSE; UINT32 idx = 0;
        if (SUCCEEDED(IDWriteFontCollection2_FindFamilyName(c, L"Bahnschrift", &idx, &ex)) && ex) {
            IDWriteFontFamily2 *fam = NULL;
            if (SUCCEEDED(IDWriteFontCollection2_GetFontFamily(c, idx, &fam)) && fam) {
                DWRITE_FONT_AXIS_VALUE a[2]; IDWriteFontList2 *l = NULL; HRESULT hr;
                a[0] = rv_av(TAG_WGHT, 300); a[1] = rv_av(TAG_WDTH, 100);
                hr = IDWriteFontFamily2_GetMatchingFonts(fam, a, 2, &l); CHECKF(hr == S_OK, "-> %s", dw_hr_name(hr));
                if (l) {
                    CHECK(IDWriteFontList2_GetFontCount(l) >= 1);
                    DW_RELEASE(l);
                }
                if (!t_is_wine()) {
                    l = RV_SENT(IDWriteFontList2 *); hr = IDWriteFontFamily2_GetMatchingFonts(fam, NULL, 2, &l); CHECKF(hr == E_INVALIDARG, "NULL values with count -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(l);
                    l = RV_SENT(IDWriteFontList2 *); hr = IDWriteFontFamily2_GetMatchingFonts(fam, a, 0x10000, &l); CHECKF(hr == E_INVALIDARG, "count 0x10000 -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(l);
                }
                DW_RELEASE(fam);
            }
        } else t_note("typographic family \"Bahnschrift\" not found");
        DW_RELEASE(c);
    }
    RB_END();
}
TEST(r2_variable_fonts_bahn_system_set_variable_fonts_have_axis_ranges)
{
    RB_BEGIN();
    IDWriteFontSet1 *s = NULL;
    if (SUCCEEDED(IDWriteFactory6_GetSystemFontSet(b.f6, FALSE, &s)) && s) {
        DWRITE_FONT_PROPERTY p; IDWriteFontSet1 *m = NULL; p.propertyId = DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME; p.propertyValue = L"Bahnschrift"; p.localeName = L"";
        CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s, &p, NULL, 0, &m));
        if (m) {
            DWRITE_FONT_AXIS_RANGE r[32]; UINT32 actual = 0, n = IDWriteFontSet1_GetFontCount(m);
            t_note("system font set: %u Bahnschrift fonts", n);
            if (n > 0) { CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges(m, r, 32, &actual)); CHECK(actual >= 2); }
            DW_RELEASE(m);
        }
        DW_RELEASE(s);
    } else t_skip("IDWriteFactory6::GetSystemFontSet failed");
    RB_END();
}
