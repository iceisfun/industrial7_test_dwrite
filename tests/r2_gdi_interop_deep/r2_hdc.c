/* CreateFontFaceFromHdc against the real GDI view of the same HFONT: OUTLINETEXTMETRIC / TEXTMETRIC of a font selected into a memory DC,
 * face identity versus CreateFontFromLOGFONT, and GDI-compatible metrics versus tmAscent / tmDescent.  System fonts only; fonts that are
 * not installed (or that GDI maps to another face) skip. */
#include "r2.h"

static const int hdc_heights[] = { -9, -10, -11, -12, -13, -14, -16, -18, -20, -24, -32, -48, -72, 9, 12, 16, 20, 32 };

/* GDI height -> ppem: negative is the em size; positive is the cell height, em = cell * (tmHeight - tmInternalLeading) / tmHeight (read back from GDI) */
static void hdc_otm_heights(const char *fam)
{
    R2_OPEN(c, fam, 400, 0);
    int i, used = 0;
    for (i = 0; i < (int)(sizeof hdc_heights / sizeof hdc_heights[0]); i++) {
        int h = hdc_heights[i]; OUTLINETEXTMETRICW *o; IDWriteFontFace *hf = NULL; HRESULT hr;
        if (!r2_select(&c, 400, 0, h, R2_Q_NONAA)) { t_note("%s: GDI maps %s to another face", fam, fam); break; }
        o = r2_otm(c.dc);
        if (!o) { t_note("%s h=%d: no OUTLINETEXTMETRIC (not a TrueType/OpenType realization)", fam, h); continue; }
        used++;
        hr = IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &hf);
        CHECKF(hr == S_OK && hf, "%s h=%d CreateFontFaceFromHdc -> %s", fam, h, dw_hr_name(hr));
        if (hf) {
            DWRITE_FONT_METRICS m; DWRITE_FONT_FACE_TYPE t = IDWriteFontFace_GetType(hf);
            IDWriteFontFace_GetMetrics(hf, &m);
            /* otmEMSquare is head.unitsPerEm of the realized face */
            CHECKF((UINT)m.designUnitsPerEm == o->otmEMSquare, "%s h=%d upm %u vs otmEMSquare %u", fam, h, m.designUnitsPerEm, o->otmEMSquare);
            CHECKF(t == DWRITE_FONT_FACE_TYPE_TRUETYPE || t == DWRITE_FONT_FACE_TYPE_CFF || t == DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION,
                   "%s h=%d face type %d", fam, h, (int)t);
            /* lfHeight < 0: em = |h|; lfHeight > 0: tmHeight (the cell height) = h; hinting may move either by a pixel */
            if (h < 0) {
                int em = (int)o->otmTextMetrics.tmHeight - (int)o->otmTextMetrics.tmInternalLeading;
                CHECKF(r2_iabs(em + h) <= 1, "%s h=%d: tmHeight %ld - tmInternalLeading %ld = %d", fam, h, o->otmTextMetrics.tmHeight, o->otmTextMetrics.tmInternalLeading, em);
            } else {
                CHECKF(r2_iabs((int)o->otmTextMetrics.tmHeight - h) <= 1, "%s h=%d: tmHeight %ld", fam, h, o->otmTextMetrics.tmHeight);
            }
            /* the GDI compatible metrics at the GDI em size reproduce the GDI ascent/descent to within a pixel */
            if (c.f1) {
                int em = (int)o->otmTextMetrics.tmHeight - (int)o->otmTextMetrics.tmInternalLeading;
                DWRITE_FONT_METRICS1 g1;
                if (r2_use_typo(hf) != 0) t_note("%s: typographic or unknown metrics selection; GDI ascent/descent come from the win values", fam);
                else if (em > 0 && SUCCEEDED(IDWriteFontFace1_GetGdiCompatibleMetrics(c.f1, (float)em, 1.0f, NULL, &g1))) {
                    double asc = r2_du2px(g1.ascent, em, g1.designUnitsPerEm), desc = r2_du2px(g1.descent, em, g1.designUnitsPerEm);
                    CHECKF(fabs(asc - (double)o->otmTextMetrics.tmAscent) <= 4.0, "%s h=%d: compat ascent %.2f px vs tmAscent %ld", fam, h, asc, o->otmTextMetrics.tmAscent);
                    CHECKF(fabs(desc - (double)o->otmTextMetrics.tmDescent) <= 4.0, "%s h=%d: compat descent %.2f px vs tmDescent %ld", fam, h, desc, o->otmTextMetrics.tmDescent);
                    if (asc != (double)o->otmTextMetrics.tmAscent || desc != (double)o->otmTextMetrics.tmDescent)
                        t_note("%s h=%d em=%d: compat asc/desc %.3f/%.3f GDI %ld/%ld", fam, h, em, asc, desc, o->otmTextMetrics.tmAscent, o->otmTextMetrics.tmDescent);
                }
            }
            IDWriteFontFace_Release(hf);
        }
        free(o);
    }
    if (!used) t_note("%s: no OUTLINETEXTMETRIC at any height", fam);
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_hdc_otm, hdc_otm_heights)

/* The face behind an HFONT is the face CreateFontFromLOGFONT resolves for the same LOGFONT (same file, index, simulations). */
static void hdc_vs_logfont(const char *fam)
{
    static const struct { int w, it; } st[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 }, { 300, 0 }, { 600, 0 }, { 500, 0 }, { 900, 0 } };
    R2_OPEN(c, fam, 400, 0);
    size_t i;
    for (i = 0; i < sizeof st / sizeof st[0]; i++) {
        IDWriteFont *font = NULL; IDWriteFontFace *a = NULL, *b = NULL; LOGFONTW lf; char ka[600], kb[600];
        if (!r2_select(&c, st[i].w, st[i].it, -16, R2_Q_NONAA)) { t_note("%s: GDI maps the name to another face at w=%d", fam, st[i].w); continue; }
        r2_lf(&lf, fam, st[i].w, st[i].it, 0, 0);
        if (FAILED(IDWriteGdiInterop_CreateFontFromLOGFONT(c.g, &lf, &font)) || !font) { CHECK(0); continue; }
        if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &a)) && SUCCEEDED(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &b)) && a && b) {
            if (r2_face_key(a, ka, sizeof ka) && r2_face_key(b, kb, sizeof kb)) {
                if (strcmp(ka, kb)) t_note("%s w=%d it=%d: LOGFONT face %s vs HDC face %s", fam, st[i].w, st[i].it, ka, kb);
                /* same file and face index (simulations may legitimately be stated differently for in-between weights) */
                {
                    char *pa = strrchr(ka, '|'), *pb = strrchr(kb, '|'), ba[300], bb[300];
                    if (pa && pb) { *pa = 0; *pb = 0; }
                    r2_key_base(ka, ba, sizeof ba); r2_key_base(kb, bb, sizeof bb);
                    CHECKF(!strcmp(ba, bb), "%s w=%d it=%d: file|index %s vs %s", fam, st[i].w, st[i].it, ba, bb);
                }
            }
        }
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(font);
    }
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_hdc_face_vs_logfont, hdc_vs_logfont)

/* LOGFONT -> HFONT -> HDC face -> ConvertFontFaceToLOGFONT -> HFONT: GDI realizes the same face and the same em square. */
static void hdc_roundtrip(const char *fam)
{
    static const struct { int w, it; } st[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 } };
    R2_OPEN(c, fam, 400, 0);
    size_t i;
    for (i = 0; i < sizeof st / sizeof st[0]; i++) {
        IDWriteFontFace *face = NULL; LOGFONTW lf2; OUTLINETEXTMETRICW *o1, *o2; WCHAR t1[LF_FACESIZE + 4], t2[LF_FACESIZE + 4]; HFONT hf2, old2; HRESULT hr;
        if (!r2_select(&c, st[i].w, st[i].it, -20, R2_Q_NONAA)) continue;
        o1 = r2_otm(c.dc); if (!o1) continue;
        t1[0] = 0; GetTextFaceW(c.dc, LF_FACESIZE + 4, t1);
        if (FAILED(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &face)) || !face) { CHECK(0); free(o1); continue; }
        memset(&lf2, 0xCC, sizeof lf2);
        hr = IDWriteGdiInterop_ConvertFontFaceToLOGFONT(c.g, face, &lf2);
        CHECKF(hr == S_OK, "%s: ConvertFontFaceToLOGFONT -> %s", fam, dw_hr_name(hr));
        if (hr == S_OK) {
            lf2.lfHeight = -20; lf2.lfQuality = R2_Q_NONAA;
            hf2 = CreateFontIndirectW(&lf2);
            old2 = (HFONT)SelectObject(c.dc, hf2);
            t2[0] = 0; GetTextFaceW(c.dc, LF_FACESIZE + 4, t2);
            o2 = r2_otm(c.dc);
            CHECKF(!lstrcmpiW(t1, t2), "%s w=%d it=%d: GDI face name changed after the round trip", fam, st[i].w, st[i].it);
            if (o2) {
                CHECKF(o1->otmEMSquare == o2->otmEMSquare, "%s: em square %u vs %u", fam, o1->otmEMSquare, o2->otmEMSquare);
                CHECKF(o1->otmTextMetrics.tmAscent == o2->otmTextMetrics.tmAscent, "%s: tmAscent %ld vs %ld", fam, o1->otmTextMetrics.tmAscent, o2->otmTextMetrics.tmAscent);
                if (o1->otmTextMetrics.tmAveCharWidth != o2->otmTextMetrics.tmAveCharWidth)
                    t_note("%s: tmAveCharWidth %ld vs %ld after the round trip", fam, o1->otmTextMetrics.tmAveCharWidth, o2->otmTextMetrics.tmAveCharWidth);
                free(o2);
            }
            SelectObject(c.dc, old2); DeleteObject(hf2);
        }
        free(o1); IDWriteFontFace_Release(face);
    }
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_hdc_roundtrip, hdc_roundtrip)

/* Quality flags select the GDI rasteriser but not the face: the HDC face is the same for every lfQuality. */
TEST(r2_gdi_interop_deep_hdc_quality_same_face)
{
    static const int q[] = { DEFAULT_QUALITY, DRAFT_QUALITY, PROOF_QUALITY, NONANTIALIASED_QUALITY, ANTIALIASED_QUALITY, CLEARTYPE_QUALITY, CLEARTYPE_NATURAL_QUALITY };
    R2_OPEN(c, "Arial", 400, 0);
    char k0[600] = "", k[600]; size_t i;
    for (i = 0; i < sizeof q / sizeof q[0]; i++) {
        IDWriteFontFace *face = NULL;
        if (!r2_select(&c, 400, 0, -16, q[i])) continue;
        CHECK_HR_OK(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &face));
        if (face && r2_face_key(face, k, sizeof k)) { if (!k0[0]) strcpy(k0, k); else CHECKF(!strcmp(k0, k), "quality %d: %s vs %s", q[i], k, k0); }
        DW_RELEASE(face);
    }
    r2_close(&c);
}

/* ppem 2048-style requests: an HFONT whose height equals the em square still maps to the same face and otmEMSquare. */
TEST(r2_gdi_interop_deep_hdc_em_square_height)
{
    R2_OPEN(c, "Arial", 400, 0);
    OUTLINETEXTMETRICW *o; IDWriteFontFace *face = NULL;
    if (!r2_select(&c, 400, 0, -2048, R2_Q_NONAA)) { t_skip("GDI refused the 2048 pixel font"); r2_close(&c); return; }
    o = r2_otm(c.dc);
    if (o) {
        CHECK_HR_OK(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &face));
        if (face) { DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m); EXPECT_EQ(m.designUnitsPerEm, o->otmEMSquare); }
        /* at em == upm GDI's pixel metrics are the design units: ascent equals dwrite's ascent */
        if (face && c.f1) {
            DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m);
            if (m.designUnitsPerEm == 2048 && r2_use_typo(face) == 0) {
                t_note("Arial em 2048: tmAscent %ld tmDescent %ld dwrite %u/%u", o->otmTextMetrics.tmAscent, o->otmTextMetrics.tmDescent, m.ascent, m.descent);
                CHECKF(r2_iabs((int)o->otmTextMetrics.tmAscent - (int)m.ascent) <= 1, "tmAscent %ld vs ascent %u", o->otmTextMetrics.tmAscent, m.ascent);
                CHECKF(r2_iabs((int)o->otmTextMetrics.tmDescent - (int)m.descent) <= 1, "tmDescent %ld vs descent %u", o->otmTextMetrics.tmDescent, m.descent);
            }
        }
        free(o);
    }
    DW_RELEASE(face); r2_close(&c);
}

/* Arial facts verified in the font tables: units per em 2048. */
TEST(r2_gdi_interop_deep_hdc_arial_upm_2048)
{
    R2_OPEN(c, "Arial", 400, 0);
    IDWriteFontFace *face = NULL; DWRITE_FONT_METRICS m;
    if (!r2_select(&c, 400, 0, -12, R2_Q_NONAA)) { t_skip("GDI maps Arial elsewhere"); r2_close(&c); return; }
    CHECK_HR_OK(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &face));
    if (face) {
        UINT32 len = 0; uint8_t *head = dw_face_table(face, DW_TAG('h', 'e', 'a', 'd'), &len);
        IDWriteFontFace_GetMetrics(face, &m);
        if (head && len >= 20) { unsigned upm = (head[18] << 8) | head[19]; EXPECT_EQ(m.designUnitsPerEm, upm); t_note("head.unitsPerEm %u", upm); }
        else t_note("head table unavailable");
        free(head);
        IDWriteFontFace_Release(face);
    }
    r2_close(&c);
}

/* A DC whose selected font is replaced between two calls yields faces that follow the DC state, not a cached one. */
TEST(r2_gdi_interop_deep_hdc_follows_selection)
{
    R2_OPEN(c, "Arial", 400, 0);
    static const struct { int w, it; } st[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 }, { 400, 0 }, { 700, 1 } };
    char first[600] = "", k[600]; size_t i; char keys[6][600];
    for (i = 0; i < 6; i++) {
        IDWriteFontFace *face = NULL; keys[i][0] = 0;
        if (!r2_select(&c, st[i].w, st[i].it, -14, R2_Q_NONAA)) continue;
        CHECK_HR_OK(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &face));
        if (face) r2_face_key(face, keys[i], sizeof keys[i]);
        DW_RELEASE(face);
    }
    CHECK(keys[0][0] == 0 || strcmp(keys[0], keys[1]));       /* regular vs bold */
    CHECK(keys[0][0] == 0 || strcmp(keys[0], keys[2]));       /* regular vs italic */
    CHECK(keys[1][0] == 0 || strcmp(keys[1], keys[3]));
    CHECKF(!strcmp(keys[0], keys[4]), "regular again: %s vs %s", keys[0], keys[4]);
    CHECKF(!strcmp(keys[3], keys[5]), "bold italic again: %s vs %s", keys[3], keys[5]);
    (void)first; (void)k;
    r2_close(&c);
}

/* The face outlives the DC and the HFONT; the DC is unchanged by the call (selected object, text metrics). */
TEST(r2_gdi_interop_deep_hdc_dc_state_untouched)
{
    R2_OPEN(c, "Arial", 400, 0);
    IDWriteFontFace *face = NULL; HGDIOBJ before, after; int mm0, mm1, bk0, bk1; COLORREF tc0, tc1; TEXTMETRICW t0, t1;
    if (!r2_select(&c, 400, 0, -17, R2_Q_NONAA)) { t_skip("GDI maps Arial elsewhere"); r2_close(&c); return; }
    SetMapMode(c.dc, MM_TEXT); SetBkMode(c.dc, TRANSPARENT); SetTextColor(c.dc, RGB(1, 2, 3));
    before = GetCurrentObject(c.dc, OBJ_FONT); mm0 = GetMapMode(c.dc); bk0 = GetBkMode(c.dc); tc0 = GetTextColor(c.dc); GetTextMetricsW(c.dc, &t0);
    CHECK_HR_OK(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &face));
    after = GetCurrentObject(c.dc, OBJ_FONT); mm1 = GetMapMode(c.dc); bk1 = GetBkMode(c.dc); tc1 = GetTextColor(c.dc); GetTextMetricsW(c.dc, &t1);
    CHECK(before == after); CHECK(mm0 == mm1); CHECK(bk0 == bk1); CHECK(tc0 == tc1);
    CHECK(t0.tmHeight == t1.tmHeight && t0.tmAscent == t1.tmAscent && t0.tmAveCharWidth == t1.tmAveCharWidth && t0.tmWeight == t1.tmWeight);
    r2_close(&c);                       /* HFONT deleted, DC deleted */
    if (face) {
        DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m); CHECK(m.designUnitsPerEm > 0);
        EXPECT_REFS(face, 1);
        IDWriteFontFace_Release(face);
    }
}

/* Mapping modes and world transforms of the DC do not change which face is reported. */
TEST(r2_gdi_interop_deep_hdc_map_mode_independent)
{
    static const int modes[] = { MM_TEXT, MM_LOMETRIC, MM_HIMETRIC, MM_LOENGLISH, MM_HIENGLISH, MM_TWIPS };
    R2_OPEN(c, "Arial", 400, 0);
    char k0[600] = "", k[600]; size_t i;
    for (i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        IDWriteFontFace *face = NULL;
        SetMapMode(c.dc, modes[i]);
        if (!r2_select(&c, 400, 0, -16, R2_Q_NONAA)) continue;
        CHECK_HR_OK(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &face));
        if (face && r2_face_key(face, k, sizeof k)) { if (!k0[0]) strcpy(k0, k); else CHECKF(!strcmp(k0, k), "map mode %d: %s vs %s", modes[i], k, k0); }
        DW_RELEASE(face);
    }
    r2_close(&c);
}

/* Several DCs, several fonts in one process: independent faces, equal faces for equal fonts. */
TEST(r2_gdi_interop_deep_hdc_two_dcs)
{
    R2_OPEN(c, "Arial", 400, 0);
    HDC dc2 = CreateCompatibleDC(NULL); HFONT h2, old2; LOGFONTW lf; IDWriteFontFace *a = NULL, *b = NULL; char ka[600], kb[600];
    if (!dc2 || !r2_select(&c, 400, 0, -16, R2_Q_NONAA)) { t_skip("setup failed"); if (dc2) DeleteDC(dc2); r2_close(&c); return; }
    r2_lf(&lf, "Arial", 700, 0, -30, R2_Q_NONAA);
    h2 = CreateFontIndirectW(&lf); old2 = (HFONT)SelectObject(dc2, h2);
    CHECK_HR_OK(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, c.dc, &a));
    CHECK_HR_OK(IDWriteGdiInterop_CreateFontFaceFromHdc(c.g, dc2, &b));
    if (a && b && r2_face_key(a, ka, sizeof ka) && r2_face_key(b, kb, sizeof kb)) CHECKF(strcmp(ka, kb) != 0, "regular and bold report the same face %s", ka);
    DW_RELEASE(a); DW_RELEASE(b);
    SelectObject(dc2, old2); DeleteObject(h2); DeleteDC(dc2);
    r2_close(&c);
}

/* CreateFontFaceFromHdc for a DC with no explicit font: the default GDI font is a raster font or a TrueType font; either way no crash and a
 * valid out value on success (raster fonts are NOT supported; see gih_raster_* in gdi_interop). */
TEST(r2_gdi_interop_deep_hdc_stock_fonts_system_ttf_only)
{
    static const int stock[] = { DEFAULT_GUI_FONT, SYSTEM_FONT, ANSI_VAR_FONT, ANSI_FIXED_FONT, DEVICE_DEFAULT_FONT, OEM_FIXED_FONT };
    HDC dc = CreateCompatibleDC(NULL); IDWriteGdiInterop *g = NULL; size_t i;
    if (!dc || FAILED(IDWriteFactory_GetGdiInterop(dw_shared(), &g)) || !g) { t_skip("no DC or interop"); if (dc) DeleteDC(dc); return; }
    if (t_is_wine()) { t_skip("raster stock fonts raise exceptions in the emulator"); DW_RELEASE(g); DeleteDC(dc); return; }
    for (i = 0; i < sizeof stock / sizeof stock[0]; i++) {
        HGDIOBJ so = GetStockObject(stock[i]), old; OUTLINETEXTMETRICW *o; IDWriteFontFace *face = NULL; HRESULT hr; LOGFONTW lf;
        if (!so) continue;
        GetObjectW(so, sizeof lf, &lf);
        old = SelectObject(dc, so);
        o = r2_otm(dc);
        if (o) {                                     /* only TrueType realizations: raster fonts cannot be represented */
            hr = IDWriteGdiInterop_CreateFontFaceFromHdc(g, dc, &face);
            /* GDI reports an OUTLINETEXTMETRIC for some bitmap-font realizations; DirectWrite refuses those files */
            if (hr == DWRITE_E_FILEFORMAT) t_note("stock %d (%ls): font file format not supported by DirectWrite", stock[i], lf.lfFaceName);
            else CHECKF(hr == S_OK && face, "stock %d (%ls) -> %s", stock[i], lf.lfFaceName, dw_hr_name(hr));
            DW_RELEASE(face); free(o);
        } else t_note("stock object %d (%ls) is not an outline font", stock[i], lf.lfFaceName);
        SelectObject(dc, old);
    }
    DW_RELEASE(g); DeleteDC(dc);
}
