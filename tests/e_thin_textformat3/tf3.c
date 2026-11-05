/* IDWriteTextFormat3 state and how a layout inherits / decouples from the format it was created from (axes, automatic axes,
 * DWRITE_LINE_SPACING, vertical orientation, optical alignment, last-line wrapping). Deterministic DwTest family. */
#include "../text_layout_core/tl.h"

#define AX(tag, v) { (tag), (v) }

static IDWriteTextFormat3 *mkfmt3(tl_fx *x, IDWriteTextFormat **base)
{
    IDWriteTextFormat3 *t3 = NULL;
    *base = tl_format(x, TL_FAM, TL_UPM);
    if (!*base) return NULL;
    if (FAILED(IDWriteTextFormat_QueryInterface(*base, &IID_IDWriteTextFormat3, (void **)&t3))) t3 = NULL;
    return t3;
}
#define F3_BEGIN() TL_BEGIN(); IDWriteTextFormat *fb_ = NULL; IDWriteTextFormat3 *t3 = mkfmt3(&X, &fb_); \
    if (!t3) { DW_RELEASE(fb_); TL_END(); t_skip("text format 3 unavailable"); return; }
#define F3_END() do { IDWriteTextFormat3_Release(t3); DW_RELEASE(fb_); TL_END(); } while (0)

static IDWriteTextLayout3 *layout_from(IDWriteFactory *f, IDWriteTextFormat *fmt)
{
    IDWriteTextLayout *l = NULL; IDWriteTextLayout3 *l3 = NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(f, L"ABC", 3, fmt, 100000.0f, 100000.0f, &l)) || !l) return NULL;
    if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout3, (void **)&l3))) l3 = NULL;
    IDWriteTextLayout_Release(l);
    return l3;
}

static IDWriteTextFormat3 *as_fmt3(IDWriteTextLayout3 *l3)
{
    IDWriteTextFormat3 *t = NULL;
    if (FAILED(IDWriteTextLayout3_QueryInterface(l3, &IID_IDWriteTextFormat3, (void **)&t))) t = NULL;
    return t;
}

TEST(tf3_axes_inherited_then_decoupled)
{
    F3_BEGIN();
    DWRITE_FONT_AXIS_VALUE in[2] = { AX(DWRITE_FONT_AXIS_TAG_WEIGHT, 650.0f), AX(DWRITE_FONT_AXIS_TAG_WIDTH, 80.0f) }, out[4];
    IDWriteTextLayout3 *l3;
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, in, 2));
    l3 = layout_from(f, fb_);
    if (l3) {
        IDWriteTextFormat3 *lf = as_fmt3(l3);
        if (lf) {
            memset(out, 0, sizeof out);
            EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(lf), 2);
            CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(lf, out, 2));
            EXPECT_EQ(out[0].axisTag, DWRITE_FONT_AXIS_TAG_WEIGHT); CHECK_FBITS(out[0].value, 650.0f);
            EXPECT_EQ(out[1].axisTag, DWRITE_FONT_AXIS_TAG_WIDTH);  CHECK_FBITS(out[1].value, 80.0f);
            /* later changes to the format do not reach the layout */
            CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, in, 0));
            EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 0);
            EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(lf), 2);
            /* and the other way round */
            CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, in, 1));
            { DWRITE_FONT_AXIS_VALUE one = AX(DWRITE_FONT_AXIS_TAG_WEIGHT, 300.0f);
              CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(lf, &one, 1));
              CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, out, 1)); CHECK_FBITS(out[0].value, 650.0f); }
            IDWriteTextFormat3_Release(lf);
        }
        IDWriteTextLayout3_Release(l3);
    }
    F3_END();
}

TEST(tf3_automatic_axes_default_and_copy)
{
    F3_BEGIN(); IDWriteTextLayout3 *l3;
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), DWRITE_AUTOMATIC_FONT_AXES_NONE);
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t3, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
    l3 = layout_from(f, fb_);
    if (l3) {
        IDWriteTextFormat3 *lf = as_fmt3(l3);
        if (lf) {
            EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(lf), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
            CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t3, DWRITE_AUTOMATIC_FONT_AXES_NONE));
            EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(lf), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);   /* decoupled */
            IDWriteTextFormat3_Release(lf);
        }
        IDWriteTextLayout3_Release(l3);
    }
    /* no validation: any bit pattern is stored and read back verbatim */
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t3, (DWRITE_AUTOMATIC_FONT_AXES)2));
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), 2);
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t3, (DWRITE_AUTOMATIC_FONT_AXES)0x80));
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), 0x80);
    F3_END();
}

TEST(tf3_line_spacing_struct_copy)
{
    F3_BEGIN(); IDWriteTextLayout3 *l3; DWRITE_LINE_SPACING ls = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 0.75f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, g;
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing((IDWriteTextFormat2 *)t3, &ls));
    memset(&g, 0xCC, sizeof g);
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing((IDWriteTextFormat2 *)t3, &g));
    EXPECT_EQ(g.method, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL); CHECK_FBITS(g.height, 1.5f); CHECK_FBITS(g.baseline, 0.75f);
    CHECK_FBITS(g.leadingBefore, 0.5f); EXPECT_EQ(g.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_ENABLED);
    l3 = layout_from(f, fb_);
    if (l3) {
        memset(&g, 0xCC, sizeof g);
        CHECK_HR_OK(IDWriteTextLayout3_GetLineSpacing(l3, &g));
        EXPECT_EQ(g.method, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL); CHECK_FBITS(g.height, 1.5f); CHECK_FBITS(g.leadingBefore, 0.5f);
        EXPECT_EQ(g.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_ENABLED);
        IDWriteTextLayout3_Release(l3);
    }
    /* negative control: an invalid struct is rejected and leaves the format unchanged */
    ls.leadingBefore = 2.0f;
    CHECK_HR(IDWriteTextFormat2_SetLineSpacing((IDWriteTextFormat2 *)t3, &ls), E_INVALIDARG);
    memset(&g, 0xCC, sizeof g); CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing((IDWriteTextFormat2 *)t3, &g)); CHECK_FBITS(g.leadingBefore, 0.5f);
    F3_END();
}

TEST(tf3_format1_properties_copy_to_layout)
{
    F3_BEGIN(); IDWriteTextLayout3 *l3;
    CHECK_HR_OK(IDWriteTextFormat3_SetVerticalGlyphOrientation(t3, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED));
    CHECK_HR_OK(IDWriteTextFormat3_SetOpticalAlignment(t3, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS));
    CHECK_HR_OK(IDWriteTextFormat3_SetLastLineWrapping(t3, FALSE));
    l3 = layout_from(f, fb_);
    if (l3) {
        EXPECT_EQ(IDWriteTextLayout3_GetVerticalGlyphOrientation(l3), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
        EXPECT_EQ(IDWriteTextLayout3_GetOpticalAlignment(l3), DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
        EXPECT_EQ(IDWriteTextLayout3_GetLastLineWrapping(l3), FALSE);
        IDWriteTextLayout3_Release(l3);
    }
    /* negative control: defaults on a fresh format */
    { IDWriteTextFormat *b2 = tl_format(&X, TL_FAM, TL_UPM); IDWriteTextFormat3 *u = NULL;
      if (b2 && SUCCEEDED(IDWriteTextFormat_QueryInterface(b2, &IID_IDWriteTextFormat3, (void **)&u))) {
          EXPECT_EQ(IDWriteTextFormat3_GetVerticalGlyphOrientation(u), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
          EXPECT_EQ(IDWriteTextFormat3_GetOpticalAlignment(u), DWRITE_OPTICAL_ALIGNMENT_NONE);
          EXPECT_EQ(IDWriteTextFormat3_GetLastLineWrapping(u), TRUE);
          IDWriteTextFormat3_Release(u);
      }
      DW_RELEASE(b2); }
    F3_END();
}
