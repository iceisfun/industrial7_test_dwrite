/* Argument validation of the GDI interop entry points; none of these need an installed font. */
#include "r2.h"

static int neg_open(IDWriteFactory **f, IDWriteGdiInterop **g)
{
    *g = NULL; *f = dw_shared();
    if (!*f) { t_skip("no factory"); return 0; }
    if (FAILED(IDWriteFactory_GetGdiInterop(*f, g)) || !*g) { t_skip("no interop"); return 0; }
    return 1;
}

/* DWriteGdiInterop::CreateFontFaceFromHdc: NULL out or NULL hdc -> E_INVALIDARG, *out zeroed once the pointer is known to be valid */
TEST(r2_gdi_interop_deep_neg_facefromhdc_null_hdc)
{
    IDWriteFactory *f; IDWriteGdiInterop *g; IDWriteFontFace *face = (IDWriteFontFace *)(ULONG_PTR)0x1234;
    if (!neg_open(&f, &g)) return;
    CHECK_HR(IDWriteGdiInterop_CreateFontFaceFromHdc(g, NULL, &face), DW_E_INVALIDARG);
    CHECK_OUT_NULL(face);
    IDWriteGdiInterop_Release(g);
}
TEST(r2_gdi_interop_deep_neg_facefromhdc_null_out)
{
    IDWriteFactory *f; IDWriteGdiInterop *g; HDC dc;
    if (!neg_open(&f, &g)) return;
    dc = CreateCompatibleDC(NULL);
    /* the out pointer is tested before anything else on Windows; the other implementation dereferences it */
    if (t_is_wine()) { t_skip("NULL out pointer crashes under Wine"); if (dc) DeleteDC(dc); IDWriteGdiInterop_Release(g); return; }
    if (dc) { CHECK_HR(IDWriteGdiInterop_CreateFontFaceFromHdc(g, dc, NULL), DW_E_INVALIDARG); DeleteDC(dc); }
    CHECK_HR(IDWriteGdiInterop_CreateFontFaceFromHdc(g, NULL, NULL), DW_E_INVALIDARG);
    IDWriteGdiInterop_Release(g);
}

/* ConvertFontToLOGFONT clears the LOGFONT (0x5c bytes) and *isSystemFont before it looks at the font; a NULL font is E_INVALIDARG */
TEST(r2_gdi_interop_deep_neg_convertfont_null_font)
{
    IDWriteFactory *f; IDWriteGdiInterop *g; LOGFONTW lf; BOOL sys = 7; static const LOGFONTW zero;
    if (!neg_open(&f, &g)) return;
    memset(&lf, 0xA5, sizeof lf);
    CHECK_HR(IDWriteGdiInterop_ConvertFontToLOGFONT(g, NULL, &lf, &sys), DW_E_INVALIDARG);
    CHECK(!memcmp(&lf, &zero, sizeof lf));
    CHECK(sys == 0);
    IDWriteGdiInterop_Release(g);
}

/* ConvertFontFaceToLOGFONT clears the LOGFONT first; a NULL face is E_INVALIDARG */
TEST(r2_gdi_interop_deep_neg_convertface_null_face)
{
    IDWriteFactory *f; IDWriteGdiInterop *g; LOGFONTW lf; static const LOGFONTW zero;
    if (!neg_open(&f, &g)) return;
    memset(&lf, 0xA5, sizeof lf);
    CHECK_HR(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, NULL, &lf), DW_E_INVALIDARG);
    CHECK(!memcmp(&lf, &zero, sizeof lf));
    IDWriteGdiInterop_Release(g);
}

/* CreateFontFromLOGFONT zeroes *out first, then NULL LOGFONT -> E_INVALIDARG, unknown family -> DWRITE_E_NOFONT (0x88985002) */
TEST(r2_gdi_interop_deep_neg_createfont_null_logfont)
{
    IDWriteFactory *f; IDWriteGdiInterop *g; IDWriteGdiInterop1 *g1 = NULL; IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)0x1234;
    if (!neg_open(&f, &g)) return;
    CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, NULL, &font), DW_E_INVALIDARG);
    CHECK_OUT_NULL(font);
    if (SUCCEEDED(IDWriteGdiInterop_QueryInterface(g, &IID_IDWriteGdiInterop1, (void **)&g1)) && g1) {
        font = (IDWriteFont *)(ULONG_PTR)0x1234;
        CHECK_HR(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, NULL, NULL, &font), DW_E_INVALIDARG);
        CHECK_OUT_NULL(font);
        IDWriteGdiInterop1_Release(g1);
    }
    IDWriteGdiInterop_Release(g);
}
TEST(r2_gdi_interop_deep_neg_createfont_unknown_family)
{
    IDWriteFactory *f; IDWriteGdiInterop *g; IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)0x1234; LOGFONTW lf; int cs, w;
    static const int sets[] = { DEFAULT_CHARSET, ANSI_CHARSET, SYMBOL_CHARSET, SHIFTJIS_CHARSET, OEM_CHARSET };
    if (!neg_open(&f, &g)) return;
    for (cs = 0; cs < 5; cs++)
        for (w = 100; w <= 900; w += 200) {
            r2_lf(&lf, "No Such Family r2gdi neg", w, w > 400, -12, 0); lf.lfCharSet = (BYTE)sets[cs];
            font = (IDWriteFont *)(ULONG_PTR)0x1234;
            CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font), 0x88985002);
            CHECK_OUT_NULL(font);
        }
    IDWriteGdiInterop_Release(g);
}

/* DrawGlyphRun: the blackBoxRect is cleared first, then NULL run / NULL rendering params / NULL font face / NaN origin -> E_INVALIDARG */
TEST(r2_gdi_interop_deep_neg_drawglyphrun_null_args)
{
    IDWriteFactory *f; IDWriteGdiInterop *g; IDWriteBitmapRenderTarget *rt = NULL; IDWriteRenderingParams *rp = NULL;
    DWRITE_GLYPH_RUN run; RECT bb; HRESULT hr; UINT16 gi = 1; FLOAT adv = 8; float nan = (float)NAN;
    if (t_is_wine()) { t_skip("NULL run/face arguments crash under Wine"); return; }
    if (!neg_open(&f, &g)) return;
    hr = IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 32, 32, &rt);
    if (FAILED(hr) || !rt) { t_skip("CreateBitmapRenderTarget -> %s", dw_hr_name(hr)); IDWriteGdiInterop_Release(g); return; }
    if (FAILED(IDWriteFactory_CreateRenderingParams(f, &rp))) rp = NULL;
    memset(&run, 0, sizeof run); run.fontEmSize = 12; run.glyphCount = 1; run.glyphIndices = &gi; run.glyphAdvances = &adv;
    if (!t_is_wine()) {
    memset(&bb, 0xCD, sizeof bb);
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(rt, 4, 20, DWRITE_MEASURING_MODE_NATURAL, NULL, rp, 0, &bb), DW_E_INVALIDARG);
    CHECK(bb.left == 0 && bb.top == 0 && bb.right == 0 && bb.bottom == 0);
    }
    memset(&bb, 0xCD, sizeof bb);
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(rt, 4, 20, DWRITE_MEASURING_MODE_NATURAL, &run, rp, 0, &bb), DW_E_INVALIDARG);   /* fontFace NULL */
    CHECK(bb.left == 0 && bb.top == 0 && bb.right == 0 && bb.bottom == 0);
    if (!t_is_wine()) CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(rt, 4, 20, DWRITE_MEASURING_MODE_NATURAL, &run, NULL, 0, NULL), DW_E_INVALIDARG);
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(rt, nan, 20, DWRITE_MEASURING_MODE_NATURAL, &run, rp, 0, NULL), DW_E_INVALIDARG);
    CHECK_HR(IDWriteBitmapRenderTarget_DrawGlyphRun(rt, 4, nan, DWRITE_MEASURING_MODE_NATURAL, &run, rp, 0, NULL), DW_E_INVALIDARG);
    DW_RELEASE(rp); DW_RELEASE(rt); IDWriteGdiInterop_Release(g);
}
