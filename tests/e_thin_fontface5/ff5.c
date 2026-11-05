/* IDWriteFontFace4 / IDWriteFontFace5 on the deterministic outline-only synthetic font (no variations, no colour, no bitmaps). */
#include "dwx.h"
#define CHECK_FBITS_(a, b) t_check((float)(a) == (float)(b), __FILE__, __LINE__, #a " == " #b, "got %g want %g", (double)(a), (double)(b))

typedef struct { dwx_font h; IDWriteFontFace4 *f4; IDWriteFontFace5 *f5; IDWriteFactory *f; int ok; } fx;
static int open_fx(fx *x)
{
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("no factory"); return 0; }
    if (FAILED(dwx_font_open_default(&x->h, x->f)) || !x->h.face) { dwx_font_close(&x->h); DW_RELEASE(x->f); t_skip("font open failed"); return 0; }
    if (FAILED(IDWriteFontFace_QueryInterface(x->h.face, &IID_IDWriteFontFace4, (void **)&x->f4))) x->f4 = NULL;
    if (FAILED(IDWriteFontFace_QueryInterface(x->h.face, &IID_IDWriteFontFace5, (void **)&x->f5))) x->f5 = NULL;
    if (!x->f4 || !x->f5) { DW_RELEASE(x->f4); DW_RELEASE(x->f5); dwx_font_close(&x->h); DW_RELEASE(x->f); t_skip("FontFace4/5 unsupported"); return 0; }
    return 1;
}
static void close_fx(fx *x) { DW_RELEASE(x->f4); DW_RELEASE(x->f5); dwx_font_close(&x->h); DW_RELEASE(x->f); }
#define FX() fx X; if (!open_fx(&X)) return

#define TAG_WGHT 0x74686777u
#define TAG_WDTH 0x68746477u
#define TAG_ITAL 0x6c617469u
#define TAG_SLNT 0x746e6c73u

TEST(ff5_axes_synthesized_on_static_font)
{
    FX();
    DWRITE_FONT_AXIS_VALUE v[8]; UINT32 i;
    EXPECT_EQ(IDWriteFontFace5_HasVariations(X.f5), FALSE);
    EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(X.f5), 4);          /* wght, wdth, ital, slnt even though the font has no fvar */
    memset(v, 0xCC, sizeof v);
    CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(X.f5, v, 4));
    EXPECT_EQ(v[0].axisTag, TAG_WGHT); CHECK_FBITS_(v[0].value, 400.0f);
    EXPECT_EQ(v[1].axisTag, TAG_WDTH); CHECK_FBITS_(v[1].value, 100.0f);
    EXPECT_EQ(v[2].axisTag, TAG_ITAL); CHECK_FBITS_(v[2].value, 0.0f);
    EXPECT_EQ(v[3].axisTag, TAG_SLNT); CHECK_FBITS_(v[3].value, 0.0f);
    /* a buffer larger than needed succeeds and the surplus is zeroed */
    memset(v, 0xCC, sizeof v);
    CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(X.f5, v, 8));
    EXPECT_EQ(v[0].axisTag, TAG_WGHT);
    for (i = 4; i < 8; i++) { EXPECT_EQ(v[i].axisTag, 0); CHECK_FBITS_(v[i].value, 0.0f); }
    /* negative control: a buffer that is too small fails with the insufficient-buffer code, for 0 and for 3 alike */
    CHECK_HR(IDWriteFontFace5_GetFontAxisValues(X.f5, v, 0), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR(IDWriteFontFace5_GetFontAxisValues(X.f5, v, 3), DW_E_NOT_SUFFICIENT_BUFFER);
    close_fx(&X);
}

TEST(ff5_axes_follow_weight_width_and_style_of_the_font)
{
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); size_t n; uint8_t *b;
    IDWriteFontFace5 *f5 = NULL; DWRITE_FONT_AXIS_VALUE v[4];
    if (!f) { t_skip("no factory"); return; }
    b = ttf_build_styled("EnrichAx", 700, 3, 1, &n);
    if (SUCCEEDED(dwx_font_open(&h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE)) && h.face &&
        SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace5, (void **)&f5))) {
        memset(v, 0xCC, sizeof v);
        EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f5), 4);
        CHECK_HR_OK(IDWriteFontFace5_GetFontAxisValues(f5, v, 4));
        EXPECT_EQ(v[0].axisTag, TAG_WGHT); CHECK_FBITS_(v[0].value, 700.0f);
        EXPECT_EQ(v[1].axisTag, TAG_WDTH); CHECK_FBITS_(v[1].value, 75.0f);       /* usWidthClass 3 = condensed class 75 */
        EXPECT_EQ(v[2].axisTag, TAG_ITAL); CHECK_FBITS_(v[2].value, 1.0f);
        EXPECT_EQ(v[3].axisTag, TAG_SLNT); CHECK_FBITS_(v[3].value, -12.0f);
    } else t_note("font open failed");
    DW_RELEASE(f5); dwx_font_close(&h); free(b); DW_RELEASE(f);
}

TEST(ff5_equals_semantics)
{
    FX();
    IDWriteFontFace *again = NULL; IDWriteFontFile *file = X.h.file;
    EXPECT_EQ(IDWriteFontFace5_Equals(X.f5, X.h.face), TRUE);
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(X.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &again));
    if (again) {
        EXPECT_EQ(IDWriteFontFace5_Equals(X.f5, again), TRUE);
        DW_RELEASE(again);
    }
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(X.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &again));
    if (again) {   /* negative control: a different simulation is a different face */
        EXPECT_EQ(IDWriteFontFace5_Equals(X.f5, again), FALSE);
        DW_RELEASE(again);
    }
    close_fx(&X);
}

TEST(ff5_font_resource)
{
    FX();
    IDWriteFontResource *r = (IDWriteFontResource *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteFontFace5_GetFontResource(X.f5, &r));
    if (r && r != (void *)1) {
        EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(r), 0);
        EXPECT_EQ(IDWriteFontResource_GetFontAxisCount(r), 4); EXPECT_EQ(IDWriteFontResource_HasVariations(r), FALSE);
        EXPECT_EQ(IDWriteFontResource_HasVariations(r), FALSE);
        DW_RELEASE(r);
    } else CHECK(0);
    close_fx(&X);
}

TEST(ff4_image_formats_outline_only)
{
    FX();
    DWRITE_GLYPH_IMAGE_FORMATS fm;
    EXPECT_EQ(IDWriteFontFace4_GetGlyphImageFormats(X.f4), DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE);       /* face-wide: outlines only */
    fm = (DWRITE_GLYPH_IMAGE_FORMATS)0xCCCC;
    CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageFormats_(X.f4, 2, 8, 64, &fm));
    EXPECT_EQ(fm, 0);                                                    /* per-glyph query lists bitmap/colour/SVG formats only */
    fm = (DWRITE_GLYPH_IMAGE_FORMATS)0xCCCC;
    CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageFormats_(X.f4, 2, 64, 8, &fm));   /* reversed ppem range is not validated */
    EXPECT_EQ(fm, 0);
    fm = (DWRITE_GLYPH_IMAGE_FORMATS)0xCCCC;
    CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageFormats_(X.f4, 500, 8, 64, &fm));  /* glyph beyond the font is not validated either */
    EXPECT_EQ(fm, 0);
    close_fx(&X);
}

TEST(ff4_image_data_unavailable)
{
    FX();
    DWRITE_GLYPH_IMAGE_DATA d; void *ctx; HRESULT hr;
    /* PNG request on a font without bitmaps: success with an empty result */
    memset(&d, 0xCC, sizeof d); ctx = (void *)1;
    CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(X.f4, 2, 16, DWRITE_GLYPH_IMAGE_FORMATS_PNG, &d, &ctx));
    CHECK(d.imageData == NULL); EXPECT_EQ(d.imageDataSize, 0); CHECK(ctx == NULL);
    IDWriteFontFace4_ReleaseGlyphImageData(X.f4, ctx);                  /* releasing the empty context is harmless */
    /* negative control: formats that are not a single bitmap/SVG format are rejected and the outputs are cleared */
    {   static const DWRITE_GLYPH_IMAGE_FORMATS bad[] = { DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE, (DWRITE_GLYPH_IMAGE_FORMATS)0,
            (DWRITE_GLYPH_IMAGE_FORMATS)(DWRITE_GLYPH_IMAGE_FORMATS_PNG | DWRITE_GLYPH_IMAGE_FORMATS_JPEG) };
        size_t i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            memset(&d, 0xCC, sizeof d); ctx = (void *)1;
            hr = IDWriteFontFace4_GetGlyphImageData(X.f4, 2, 16, bad[i], &d, &ctx);
            CHECKF(hr == E_INVALIDARG, "format 0x%x -> %s", bad[i], dw_hr_name(hr));
            CHECK(d.imageData == NULL); CHECK(ctx == NULL);
        }
    }
    close_fx(&X);
}
