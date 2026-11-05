/* IDWriteGdiInterop::CreateFontFaceFromHdc and CreateBitmapRenderTarget.
 *
 * CreateFontFaceFromHdc: ppFontFace == NULL or hdc == NULL -> E_INVALIDARG (checked before anything is written; *ppFontFace is zeroed
 * when it is non-NULL).  Otherwise the font currently selected into the DC is identified through GDI's realization info; only
 * TrueType/OpenType realizations are supported (a raster font raises a file-format exception inside dwrite).  The result carries the
 * simulations GDI applied (bit 0 bold, bit 1 oblique) and the variation axis values of the realization. */
#include "gi.h"

static int gi_hdc_face(gi_hdc *h, IDWriteGdiInterop *g, IDWriteFontFace **face, HRESULT *hr)
{
    *face = (IDWriteFontFace *)(ULONG_PTR)0x1234;
    *hr = IDWriteGdiInterop_CreateFontFaceFromHdc(g, h->dc, face);
    return SUCCEEDED(*hr) && *face != NULL;
}

TEST(gih_null_hdc)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteFontFace *face = (IDWriteFontFace *)(ULONG_PTR)0x1234;
    if (!g) { t_skip("no interop"); return; }
    CHECK_HR(IDWriteGdiInterop_CreateFontFaceFromHdc(g, NULL, &face), DW_E_INVALIDARG);
    CHECK_OUT_NULL(face);
    DW_RELEASE(g);
}
TEST(gih_null_out)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); HDC dc = CreateCompatibleDC(NULL);
    if (!g || !dc) { t_skip("no interop or DC"); DW_RELEASE(g); if (dc) DeleteDC(dc); return; }
    if (t_is_wine()) { t_skip("the emulator dereferences the NULL out pointer"); DeleteDC(dc); DW_RELEASE(g); return; }
    CHECK_HR(IDWriteGdiInterop_CreateFontFaceFromHdc(g, dc, NULL), DW_E_INVALIDARG);   /* the NULL out pointer is tested first and nothing is written */
    DeleteDC(dc); DW_RELEASE(g);
}
TEST(gih_null_both)
{
    IDWriteGdiInterop *g = gi_get(dw_shared());
    if (!g) { t_skip("no interop"); return; }
    if (t_is_wine()) { t_skip("the emulator dereferences the NULL out pointer"); DW_RELEASE(g); return; }
    CHECK_HR(IDWriteGdiInterop_CreateFontFaceFromHdc(g, NULL, NULL), DW_E_INVALIDARG);
    DW_RELEASE(g);
}
TEST(gih_arial_20)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); gi_hdc h; IDWriteFontFace *face; HRESULT hr; char tf[64], file[64];
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial") || !gi_hdc_open(&h, "Arial", 400, 0, -20, DEFAULT_CHARSET, 0)) { t_skip("Arial / DC unavailable"); DW_RELEASE(g); return; }
    if (!gi_hdc_textface(&h, tf, sizeof tf) || _stricmp(tf, "Arial")) { t_skip("GDI did not map Arial (%s)", tf); gi_hdc_close(&h); DW_RELEASE(g); return; }
    CHECK(gi_hdc_face(&h, g, &face, &hr));
    CHECK_HR_OK(hr);
    if (face != NULL && face != (IDWriteFontFace *)(ULONG_PTR)0x1234) {
        EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
        EXPECT_EQ(IDWriteFontFace_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE);
        EXPECT_EQ(IDWriteFontFace_GetIndex(face), 0);
        EXPECT_EQ(IDWriteFontFace_IsSymbolFont(face), FALSE);
        if (gi_face_file(face, file, sizeof file, NULL, 0)) EXPECT_STR(file, "arial.ttf");
        EXPECT_REFS(face, 1);
        IDWriteFontFace_Release(face);
    }
    gi_hdc_close(&h); DW_RELEASE(g);
}
TEST(gih_arial_styles)
{
    static const struct { int w, it; const char *file; } rows[] = { { 400, 0, "arial.ttf" }, { 700, 0, "arialbd.ttf" }, { 400, 1, "ariali.ttf" }, { 700, 1, "arialbi.ttf" } };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < 4; i++) {
        gi_hdc h; IDWriteFontFace *face; HRESULT hr; char file[64];
        if (!gi_hdc_open(&h, "Arial", rows[i].w, rows[i].it, -16, DEFAULT_CHARSET, 0)) continue;
        CHECK(gi_hdc_face(&h, g, &face, &hr));
        CHECK_HR_OK(hr);
        if (face != NULL && face != (IDWriteFontFace *)(ULONG_PTR)0x1234) {
            if (gi_face_file(face, file, sizeof file, NULL, 0)) CHECKF(!strcmp(file, rows[i].file), "w=%d it=%d -> %s want %s", rows[i].w, rows[i].it, file, rows[i].file);
            EXPECT_EQ(IDWriteFontFace_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE);
            IDWriteFontFace_Release(face);
        }
        gi_hdc_close(&h);
    }
    DW_RELEASE(g);
}
TEST(gih_height_does_not_change_face)
{
    static const int heights[] = { 8, -8, 20, -20, -100, 0, -1, 200 };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < sizeof heights / sizeof heights[0]; i++) {
        gi_hdc h; IDWriteFontFace *face; HRESULT hr; char file[64];
        if (!gi_hdc_open(&h, "Arial", 400, 0, heights[i], DEFAULT_CHARSET, 0)) continue;
        CHECK(gi_hdc_face(&h, g, &face, &hr));
        if (face != NULL && face != (IDWriteFontFace *)(ULONG_PTR)0x1234) {
            if (gi_face_file(face, file, sizeof file, NULL, 0)) CHECKF(!strcmp(file, "arial.ttf"), "height %d -> %s", heights[i], file);
            IDWriteFontFace_Release(face);
        }
        gi_hdc_close(&h);
    }
    DW_RELEASE(g);
}
TEST(gih_quality_flags_do_not_change_face)
{
    static const int q[] = { DEFAULT_QUALITY, DRAFT_QUALITY, PROOF_QUALITY, NONANTIALIASED_QUALITY, ANTIALIASED_QUALITY, CLEARTYPE_QUALITY };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Times New Roman")) { t_skip("Times New Roman not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < sizeof q / sizeof q[0]; i++) {
        gi_hdc h; IDWriteFontFace *face; HRESULT hr; char file[64];
        if (!gi_hdc_open(&h, "Times New Roman", 400, 0, -24, DEFAULT_CHARSET, q[i])) continue;
        CHECK(gi_hdc_face(&h, g, &face, &hr));
        if (face != NULL && face != (IDWriteFontFace *)(ULONG_PTR)0x1234) {
            if (gi_face_file(face, file, sizeof file, NULL, 0)) CHECKF(!strcmp(file, "times.ttf"), "quality %d -> %s", q[i], file);
            IDWriteFontFace_Release(face);
        }
        gi_hdc_close(&h);
    }
    DW_RELEASE(g);
}
static void gi_hdc_file_case(const char *gdi_face, int w, int it, const char *file, int charset)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); gi_hdc h; IDWriteFontFace *face; HRESULT hr; char tf[64], got[64];
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, gdi_face) || !gi_hdc_open(&h, gdi_face, w, it, -20, charset, 0)) { t_skip("%s not installed", gdi_face); DW_RELEASE(g); return; }
    if (!gi_hdc_textface(&h, tf, sizeof tf) || _stricmp(tf, gdi_face)) { t_skip("GDI mapped %s to %s", gdi_face, tf); gi_hdc_close(&h); DW_RELEASE(g); return; }
    if (gi_hdc_face(&h, g, &face, &hr)) {
        if (gi_face_file(face, got, sizeof got, NULL, 0)) CHECKF(!strcmp(got, file), "%s -> %s want %s", gdi_face, got, file);
        IDWriteFontFace_Release(face);
    } else CHECKF(0, "%s: CreateFontFaceFromHdc -> %s", gdi_face, dw_hr_name(hr));
    gi_hdc_close(&h); DW_RELEASE(g);
}
#define HDCFILE(id, face, w, it, file, cs) TEST(gih_file_##id) { gi_hdc_file_case(face, w, it, file, cs); }
HDCFILE(times, "Times New Roman", 400, 0, "times.ttf", 1)
HDCFILE(times_bold, "Times New Roman", 700, 0, "timesbd.ttf", 1)
HDCFILE(courier, "Courier New", 400, 0, "cour.ttf", 1)
HDCFILE(courier_italic, "Courier New", 400, 1, "couri.ttf", 1)
HDCFILE(tahoma, "Tahoma", 400, 0, "tahoma.ttf", 1)
HDCFILE(tahoma_bold, "Tahoma", 700, 0, "tahomabd.ttf", 1)
HDCFILE(segoeui, "Segoe UI", 400, 0, "segoeui.ttf", 1)
HDCFILE(segoeui_bold, "Segoe UI", 700, 0, "segoeuib.ttf", 1)
HDCFILE(segoe_semibold, "Segoe UI Semibold", 600, 0, "seguisb.ttf", 1)
HDCFILE(segoe_light, "Segoe UI Light", 300, 0, "segoeuil.ttf", 1)
HDCFILE(arial_black, "Arial Black", 900, 0, "ariblk.ttf", 1)
HDCFILE(consolas, "Consolas", 400, 0, "consola.ttf", 1)
HDCFILE(verdana, "Verdana", 400, 0, "verdana.ttf", 1)
HDCFILE(marlett, "Marlett", 400, 0, "marlett.ttf", SYMBOL_CHARSET)
HDCFILE(lucida_console, "Lucida Console", 400, 0, "lucon.ttf", 1)
HDCFILE(calibri, "Calibri", 400, 0, "calibri.ttf", 1)

TEST(gih_marlett_is_symbol)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); gi_hdc h; IDWriteFontFace *face; HRESULT hr; LOGFONTW lf;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Marlett") || !gi_hdc_open(&h, "Marlett", 400, 0, -20, SYMBOL_CHARSET, 0)) { t_skip("Marlett unavailable"); DW_RELEASE(g); return; }
    if (gi_hdc_face(&h, g, &face, &hr)) {
        EXPECT_EQ(IDWriteFontFace_IsSymbolFont(face), TRUE);
        GI_POISON_LF(lf);
        CHECK_HR_OK(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, face, &lf));
        EXPECT_EQ(lf.lfCharSet, SYMBOL_CHARSET);
        EXPECT_WSTR(lf.lfFaceName, L"Marlett");
        IDWriteFontFace_Release(face);
    }
    gi_hdc_close(&h); DW_RELEASE(g);
}
TEST(gih_face_to_logfont_roundtrip)
{
    static const char *faces[] = { "Arial", "Times New Roman", "Courier New", "Tahoma", "Segoe UI", "Verdana", "Consolas" };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i; int it;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < sizeof faces / sizeof faces[0]; i++)
        for (it = 0; it < 2; it++) {
            gi_hdc h; IDWriteFontFace *face; HRESULT hr; LOGFONTW lf; char tf[64], n[64];
            if (!gi_sys_has(f, faces[i]) || !gi_hdc_open(&h, faces[i], 400, it, -20, DEFAULT_CHARSET, 0)) continue;
            if (!gi_hdc_textface(&h, tf, sizeof tf) || _stricmp(tf, faces[i])) { gi_hdc_close(&h); continue; }
            if (gi_hdc_face(&h, g, &face, &hr)) {
                GI_POISON_LF(lf);
                CHECK_HR_OK(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, face, &lf));
                gi_name_u8(&lf, n, sizeof n);
                CHECKF(!_stricmp(n, tf), "face -> LOGFONT name \"%s\" vs GDI \"%s\"", n, tf);
                GI_LF_SHAPE(lf);
                EXPECT_EQ(lf.lfWeight, 400);
                IDWriteFontFace_Release(face);
            }
            gi_hdc_close(&h);
        }
    DW_RELEASE(g);
}
TEST(gih_bold_gdi_font_without_bold_face)
{
    /* GDI emboldens fonts that have no bold design; the simulations come from the realization, and a bold-simulated face reports lfWeight + 151 */
    static const char *faces[] = { "Segoe UI Light", "Arial Black", "Segoe UI Semibold", "Impact", "Comic Sans MS", "Marlett" };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < sizeof faces / sizeof faces[0]; i++) {
        gi_hdc h; IDWriteFontFace *face; HRESULT hr; LOGFONTW lf; char tf[64];
        if (!gi_sys_has(f, faces[i]) || !gi_hdc_open(&h, faces[i], 700, 0, -20, DEFAULT_CHARSET, 0)) continue;
        if (!gi_hdc_textface(&h, tf, sizeof tf) || _stricmp(tf, faces[i])) { gi_hdc_close(&h); continue; }
        if (gi_hdc_face(&h, g, &face, &hr)) {
            DWRITE_FONT_SIMULATIONS sim = IDWriteFontFace_GetSimulations(face); LOGFONTW plain; IDWriteFont *font = NULL; BOOL sys;
            CHECK_HR_OK(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, face, &lf));
            t_note("%s lfWeight 700 -> face simulations %d, ConvertFontFaceToLOGFONT lfWeight %ld", faces[i], (int)sim, lf.lfWeight);
            gi_lf(&plain, faces[i], 400, 0, 1);
            if (SUCCEEDED(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &plain, &font)) && font) {
                LOGFONTW base;
                if (SUCCEEDED(IDWriteGdiInterop_ConvertFontToLOGFONT(g, font, &base, &sys)))
                    EXPECT_EQ(lf.lfWeight, base.lfWeight + ((sim & DWRITE_FONT_SIMULATIONS_BOLD) ? GI_BOLD_SIM_DELTA : 0));
                IDWriteFont_Release(font);
            }
            IDWriteFontFace_Release(face);
        }
        gi_hdc_close(&h);
    }
    DW_RELEASE(g);
}
TEST(gih_italic_gdi_font_without_italic_face)
{
    /* Tahoma has no italic design: GDI slants it and the realization carries the oblique bit */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); gi_hdc h; IDWriteFontFace *face; HRESULT hr; TEXTMETRICW tm; char tf[64];
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Tahoma") || !gi_hdc_open(&h, "Tahoma", 400, 1, -20, DEFAULT_CHARSET, 0)) { t_skip("Tahoma unavailable"); DW_RELEASE(g); return; }
    if (!gi_hdc_textface(&h, tf, sizeof tf) || _stricmp(tf, "Tahoma") || !GetTextMetricsW(h.dc, &tm) || !tm.tmItalic) { t_skip("GDI did not synthesize italic Tahoma"); gi_hdc_close(&h); DW_RELEASE(g); return; }
    if (gi_hdc_face(&h, g, &face, &hr)) {
        LOGFONTW lf; char file[64];
        if (gi_face_file(face, file, sizeof file, NULL, 0)) CHECKF(!strcmp(file, "tahoma.ttf"), "%s", file);
        EXPECT_EQ(IDWriteFontFace_GetSimulations(face) & DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_OBLIQUE);
        CHECK_HR_OK(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, face, &lf));
        EXPECT_EQ(lf.lfItalic, 1);
        IDWriteFontFace_Release(face);
    } else CHECK_HR_OK(hr);
    gi_hdc_close(&h); DW_RELEASE(g);
}
TEST(gih_matches_logfont_resolution)
{
    /* the DirectWrite font chosen for a LOGFONT and the face GDI realizes for it are the same file for well-formed RIBBI families */
    static const char *faces[] = { "Arial", "Times New Roman", "Courier New", "Verdana", "Segoe UI" };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i; int it, w;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < sizeof faces / sizeof faces[0]; i++)
        for (it = 0; it < 2; it++)
            for (w = 400; w <= 700; w += 300) {
                gi_hdc h; IDWriteFontFace *face; HRESULT hr; IDWriteFont *font = NULL; LOGFONTW lf; char tf[64], a[64], b[64];
                if (!gi_sys_has(f, faces[i]) || !gi_hdc_open(&h, faces[i], w, it, -20, DEFAULT_CHARSET, 0)) continue;
                if (!gi_hdc_textface(&h, tf, sizeof tf) || _stricmp(tf, faces[i])) { gi_hdc_close(&h); continue; }
                gi_lf(&lf, faces[i], w, it, DEFAULT_CHARSET);
                if (SUCCEEDED(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font)) && font && gi_hdc_face(&h, g, &face, &hr)) {
                    IDWriteFontFace *ff = NULL;
                    if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &ff)) && ff && gi_face_file(ff, a, sizeof a, NULL, 0) && gi_face_file(face, b, sizeof b, NULL, 0))
                        CHECKF(!strcmp(a, b), "%s w=%d it=%d: DirectWrite %s, GDI %s", faces[i], w, it, a, b);
                    DW_RELEASE(ff); IDWriteFontFace_Release(face);
                }
                DW_RELEASE(font);
                gi_hdc_close(&h);
            }
    DW_RELEASE(g);
}
TEST(gih_face_survives_dc_and_hfont_deletion)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); gi_hdc h; IDWriteFontFace *face; HRESULT hr; DWRITE_FONT_METRICS m; char file[64];
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial") || !gi_hdc_open(&h, "Arial", 400, 0, -20, DEFAULT_CHARSET, 0)) { t_skip("Arial unavailable"); DW_RELEASE(g); return; }
    if (gi_hdc_face(&h, g, &face, &hr)) {
        gi_hdc_close(&h);
        IDWriteFontFace_GetMetrics(face, &m);
        CHECK(m.designUnitsPerEm == 2048);   /* Arial is a 2048 unit/em font on every Windows 10 */
        CHECK(gi_face_file(face, file, sizeof file, NULL, 0));
        IDWriteFontFace_Release(face);
    } else gi_hdc_close(&h);
    DW_RELEASE(g);
}
TEST(gih_two_requests_same_identity)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); gi_hdc h; IDWriteFontFace *a, *b; HRESULT hr;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial") || !gi_hdc_open(&h, "Arial", 400, 0, -20, DEFAULT_CHARSET, 0)) { t_skip("Arial unavailable"); DW_RELEASE(g); return; }
    if (gi_hdc_face(&h, g, &a, &hr) && gi_hdc_face(&h, g, &b, &hr)) {
        IDWriteFontFace *sysf = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        t_note("two faces from the same HDC are %s objects; GDI face vs collection face are %s objects", a == b ? "the same" : "different", sysf == a ? "the same" : "different");
        if (sysf) {
            EXPECT_EQ(IDWriteFontFace_GetGlyphCount(a), IDWriteFontFace_GetGlyphCount(sysf));
            EXPECT_EQ(IDWriteFontFace_GetGlyphCount(a), IDWriteFontFace_GetGlyphCount(b));
            IDWriteFontFace_Release(sysf);
        }
        IDWriteFontFace_Release(b); IDWriteFontFace_Release(a);
    }
    gi_hdc_close(&h); DW_RELEASE(g);
}
TEST(gih_dib_memory_dc)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); dwx_dc d; HFONT hf, old; LOGFONTW lf; IDWriteFontFace *face = NULL; HRESULT hr;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial") || !dwx_dc_create(&d, 16, 16, 0xffffff)) { t_skip("Arial or DC unavailable"); DW_RELEASE(g); return; }
    gi_lf(&lf, "Arial", 400, 0, 1); lf.lfHeight = -12;
    hf = CreateFontIndirectW(&lf); old = (HFONT)SelectObject(d.dc, hf);
    hr = IDWriteGdiInterop_CreateFontFaceFromHdc(g, d.dc, &face);
    CHECK_HR_OK(hr);
    DW_RELEASE(face);
    SelectObject(d.dc, old); DeleteObject(hf); dwx_dc_destroy(&d); DW_RELEASE(g);
}
TEST(gih_repeat_no_leak)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGdiInterop *g; int i; gi_hdc h;
    if (!f) { t_skip("no factory"); return; }
    g = gi_get(f);
    if (!g || !gi_sys_has(f, "Arial") || !gi_hdc_open(&h, "Arial", 400, 0, -20, DEFAULT_CHARSET, 0)) { t_skip("Arial / DC unavailable"); DW_RELEASE(g); DW_RELEASE(f); return; }
    for (i = 0; i < 300; i++) {
        IDWriteFontFace *face = NULL;
        if (FAILED(IDWriteGdiInterop_CreateFontFaceFromHdc(g, h.dc, &face)) || !face) { CHECK(0); break; }
        IDWriteFontFace_Release(face);
    }
    gi_hdc_close(&h);
    DW_RELEASE(g); DW_RELEASE(f);
}
TEST(gih_variable_font_axes_note)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); gi_hdc h; IDWriteFontFace *face; HRESULT hr;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Bahnschrift") || !gi_hdc_open(&h, "Bahnschrift", 700, 0, -20, DEFAULT_CHARSET, 0)) { t_skip("Bahnschrift unavailable"); DW_RELEASE(g); return; }
    if (gi_hdc_face(&h, g, &face, &hr)) {
        IDWriteFontFace5 *f5 = NULL; char file[64];
        gi_face_file(face, file, sizeof file, NULL, 0);
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace5, (void **)&f5)) && f5) {
            t_note("Bahnschrift via HDC: file %s, HasVariations=%d axes=%u", file, (int)IDWriteFontFace5_HasVariations(f5), (unsigned)IDWriteFontFace5_GetFontAxisValueCount(f5));
            IDWriteFontFace5_Release(f5);
        }
        IDWriteFontFace_Release(face);
    } else t_note("Bahnschrift CreateFontFaceFromHdc -> %s", dw_hr_name(hr));
    gi_hdc_close(&h); DW_RELEASE(g);
}
TEST(gih_ttc_member_index)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); static const char *faces[] = { "MS Gothic", "MS UI Gothic", "MS PGothic" }; size_t i;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < 3; i++) {
        gi_hdc h; IDWriteFontFace *face; HRESULT hr; char tf[64], file[64];
        if (!gi_sys_has(f, faces[i]) || !gi_hdc_open(&h, faces[i], 400, 0, -16, SHIFTJIS_CHARSET, 0)) continue;
        if (!gi_hdc_textface(&h, tf, sizeof tf) || _stricmp(tf, faces[i])) { gi_hdc_close(&h); continue; }
        if (gi_hdc_face(&h, g, &face, &hr)) {
            gi_face_file(face, file, sizeof file, NULL, 0);
            CHECKF(!strcmp(file, "msgothic.ttc"), "%s -> %s", faces[i], file);
            EXPECT_EQ(IDWriteFontFace_GetIndex(face), i);   /* position in the collection: MS Gothic, MS UI Gothic, MS PGothic */
            EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
            IDWriteFontFace_Release(face);
        }
        gi_hdc_close(&h);
    }
    DW_RELEASE(g);
}

/* ---- raster fonts: the failure happens inside dwrite as a thrown file-format exception; run in a child so a crash is observable ---------- */
CHILD(hdcraster)
{
    /* argv[1] = GDI face name, or "-" for a DC with its default font.  Exit codes: 0 failed cleanly with *out == NULL, 1 succeeded, 2 failed with non-NULL out,
     * 4 GDI selected an outline font (nothing to test), 5 setup failure. */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g; gi_hdc h; IDWriteFontFace *face = (IDWriteFontFace *)(ULONG_PTR)0x1234; HRESULT hr; TEXTMETRICW tm;
    int rc;
    if (argc < 2 || !f) return 5;
    g = gi_get(f);
    if (!g) return 5;
    if (strcmp(argv[1], "-")) { if (!gi_hdc_open(&h, argv[1], 400, 0, 12, DEFAULT_CHARSET, 0)) return 5; }
    else { memset(&h, 0, sizeof h); h.dc = CreateCompatibleDC(NULL); if (!h.dc) return 5; }
    if (!GetTextMetricsW(h.dc, &tm)) return 5;
    if (tm.tmPitchAndFamily & TMPF_TRUETYPE) return 4;
    hr = IDWriteGdiInterop_CreateFontFaceFromHdc(g, h.dc, &face);
    printf("raster font %s -> %s\n", argv[1], dw_hr_name(hr));
    if (SUCCEEDED(hr)) rc = 1;
    else rc = (face == NULL) ? 0 : 2;
    if (SUCCEEDED(hr) && face) IDWriteFontFace_Release(face);
    return rc;
}
static int gi_brt_touched;   /* a render target was created in this process */
static void gi_raster(const char *face)
{
    char arg[96]; DWORD rc;
    /* the emulator wedges child-process creation once this process has created a bitmap render target; Windows does not */
    if (t_is_wine() && gi_brt_touched) { t_skip("child process creation hangs after a render target was created (emulator only)"); return; }
    snprintf(arg, sizeof arg, "\"%s\"", face);   /* faces with spaces must survive command-line splitting */
    rc = t_run_child("hdcraster", arg, 20000);
    CHECKF(rc != 0xDEAD0000 && rc != 0xDEAD0001, "child did not run to completion (0x%lx)", rc);
    if (rc == 4) { t_note("%s: GDI mapped it to an outline font; nothing to test", face); return; }
    if (rc == 5) { t_skip("could not set up the %s DC", face); return; }
    t_note("%s: child exit code %lu (0 clean failure, 1 success, 2 failure with non-NULL out)", face, rc);
    CHECKF(rc == 0, "%s: CreateFontFaceFromHdc on a raster font must fail without crashing (exit 0x%lx)", face, rc);
}
TEST(gih_raster_system) { gi_raster("System"); }
TEST(gih_raster_terminal) { gi_raster("Terminal"); }
TEST(gih_raster_fixedsys) { gi_raster("Fixedsys"); }
TEST(gih_raster_ms_sans_serif) { gi_raster("MS Sans Serif"); }
TEST(gih_raster_small_fonts) { gi_raster("Small Fonts"); }
TEST(gih_raster_default_dc_font) { gi_raster("-"); }

/* ---- CreateBitmapRenderTarget: creation contract only (rendering is covered by the bitmap_render_target area) -------------------------- */
TEST(gih_brt_null_hdc)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteBitmapRenderTarget *rt = (IDWriteBitmapRenderTarget *)(ULONG_PTR)0x1234;
    if (!g) { t_skip("no interop"); return; }
    gi_brt_touched = 1;
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 32, 24, &rt));   /* the hdc is optional; the call never fails on arguments */
    CHECK(rt != NULL && rt != (IDWriteBitmapRenderTarget *)(ULONG_PTR)0x1234);
    if (rt && rt != (IDWriteBitmapRenderTarget *)(ULONG_PTR)0x1234) {
        SIZE sz; EXPECT_REFS(rt, 1);
        if (SUCCEEDED(IDWriteBitmapRenderTarget_GetSize(rt, &sz))) { EXPECT_EQ(sz.cx, 32); EXPECT_EQ(sz.cy, 24); }
        IDWriteBitmapRenderTarget_Release(rt);
    }
    DW_RELEASE(g);
}
TEST(gih_brt_with_memory_dc)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteBitmapRenderTarget *rt = NULL; dwx_dc d;
    if (!g || !dwx_dc_create(&d, 8, 8, 0xffffff)) { t_skip("no interop or DC"); DW_RELEASE(g); return; }
    gi_brt_touched = 1;
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(g, d.dc, 40, 30, &rt));
    if (rt) {
        HDC mdc = IDWriteBitmapRenderTarget_GetMemoryDC(rt);
        CHECK(mdc != NULL);
        CHECK(mdc != d.dc);   /* the target owns its own memory DC */
        IDWriteBitmapRenderTarget_Release(rt);
    }
    dwx_dc_destroy(&d); DW_RELEASE(g);
}
TEST(gih_brt_distinct_objects)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteBitmapRenderTarget *a = NULL, *b = NULL;
    if (!g) { t_skip("no interop"); return; }
    gi_brt_touched = 1;
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 8, 8, &a));
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 8, 8, &b));
    CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(g);
}
TEST(gih_brt_qi_target1)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteBitmapRenderTarget *rt = NULL;
    if (!g) { t_skip("no interop"); return; }
    gi_brt_touched = 1;
    CHECK_HR_OK(IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 8, 8, &rt));
    if (rt) {
        IDWriteBitmapRenderTarget1 *t1 = NULL; IUnknown *u = NULL;
        CHECK_HR_OK(IDWriteBitmapRenderTarget_QueryInterface(rt, &IID_IDWriteBitmapRenderTarget1, (void **)&t1));
        CHECK_HR_OK(IDWriteBitmapRenderTarget_QueryInterface(rt, &IID_IUnknown, (void **)&u));
        DW_RELEASE(t1); DW_RELEASE(u);
        EXPECT_REFS(rt, 1);
        IDWriteBitmapRenderTarget_Release(rt);
    }
    DW_RELEASE(g);
}
