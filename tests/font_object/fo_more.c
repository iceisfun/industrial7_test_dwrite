/* IDWriteFont objects reached through other routes (family matching, GetFontFromFontFace, GDI interop), whole-collection invariants,
 * and concurrent use.  Observed references: IDWriteFontCollection::GetFontFromFontFace (zeroes *font, DWRITE_E_NOFONT when the face's physical or
 * canonical key is not in the collection, simulations taken from the face), DWriteGdiInterop::CreateFontFromLOGFONT /
 * ConvertFontToLOGFONT (E_INVALIDARG for NULL, DWRITE_E_NOFONT for unknown names, lfCharSet SYMBOL for symbol fonts, lfOutPrecision 8,
 * isSystemFont iff the font's collection loader id is 0). */
#include "fo.h"

static IDWriteFont3 *as3(IDWriteFont *f) { IDWriteFont3 *p = NULL; if (f) IDWriteFont_QueryInterface(f, &IID_IDWriteFont3, (void **)&p); return p; }
static int same_font(IDWriteFont *a, IDWriteFont *b) { IDWriteFont3 *a3 = as3(a); int r = a3 ? IDWriteFont3_Equals(a3, b) : -1; DW_RELEASE(a3); return r; }

/* ---- families with several synthetic faces -------------------------------------------------------------------------------- */
TEST(font_family_weight_ladder_one_family) {
    ttf_spec s[9]; fo_fx x; int i; IDWriteFontFamily *f0 = NULL;
    for (i = 0; i < 9; i++) ttf_style_spec(&s[i], "FoLadW", (i + 1) * 100, 5, 0);
    FO_OPEN(x, s, 9);
    IDWriteFont_GetFontFamily(x.font[0], &f0);
    for (i = 0; i < 9; i++) { IDWriteFontFamily *fi = NULL; IDWriteFont_GetFontFamily(x.font[i], &fi); CHECKF(fi == f0, "font %d belongs to a different family object", i); DW_RELEASE(fi); }
    if (f0) { CHECK(IDWriteFontFamily_GetFontCount(f0) >= 9); DW_RELEASE(f0); }
    fo_close(&x);
}
TEST(font_family_weight_ladder_exact_matches) {
    ttf_spec s[9]; fo_fx x; int i; IDWriteFontFamily *fam = NULL;
    for (i = 0; i < 9; i++) ttf_style_spec(&s[i], "FoLadM", (i + 1) * 100, 5, 0);
    FO_OPEN(x, s, 9);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        for (i = 0; i < 9; i++) {
            IDWriteFont *f = NULL; CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, (DWRITE_FONT_WEIGHT)((i + 1) * 100), DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &f));
            if (f) { EXPECT_EQ(IDWriteFont_GetWeight(f), (i + 1) * 100); EXPECT_EQ(IDWriteFont_GetSimulations(f), 0); CHECKF(same_font(f, x.font[i]) == 1, "weight %d", (i + 1) * 100); DW_RELEASE(f); }
        }
        DW_RELEASE(fam);
    }
    fo_close(&x);
}
TEST(font_family_stretch_ladder_exact_matches) {
    ttf_spec s[9]; fo_fx x; int i; IDWriteFontFamily *fam = NULL;
    for (i = 0; i < 9; i++) ttf_style_spec(&s[i], "FoLadS", 400, i + 1, 0);
    FO_OPEN(x, s, 9);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        for (i = 0; i < 9; i++) {
            IDWriteFont *f = NULL; IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, (DWRITE_FONT_STRETCH)(i + 1), DWRITE_FONT_STYLE_NORMAL, &f);
            if (f) { EXPECT_EQ(IDWriteFont_GetStretch(f), i + 1); CHECKF(same_font(f, x.font[i]) == 1, "stretch %d", i + 1); DW_RELEASE(f); } else CHECK(0);
        }
        DW_RELEASE(fam);
    }
    fo_close(&x);
}
TEST(font_family_ribbi_four_fonts_roundtrip) {
    ttf_spec s[4]; fo_fx x; int i; IDWriteFontFamily *fam = NULL; static const int w[4] = { 400, 700, 400, 700 }, st[4] = { 0, 0, 2, 2 };
    ttf_style_spec(&s[0], "FoRibbi", 400, 5, 0); ttf_style_spec(&s[1], "FoRibbi", 700, 5, 0); ttf_style_spec(&s[2], "FoRibbi", 400, 5, 1); ttf_style_spec(&s[3], "FoRibbi", 700, 5, 1);
    FO_OPEN(x, s, 4);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        for (i = 0; i < 4; i++) {
            IDWriteFont *f = NULL; IDWriteFontFamily_GetFirstMatchingFont(fam, (DWRITE_FONT_WEIGHT)w[i], DWRITE_FONT_STRETCH_NORMAL, (DWRITE_FONT_STYLE)st[i], &f);
            if (f) { CHECKF(same_font(f, x.font[i]) == 1, "ribbi %d", i); EXPECT_EQ(IDWriteFont_GetSimulations(f), 0); DW_RELEASE(f); } else CHECK(0);
        }
        DW_RELEASE(fam);
    }
    fo_close(&x);
}
TEST(font_family_matching_fonts_first_equals_first_matching) {
    ttf_spec s[3]; fo_fx x; IDWriteFontFamily *fam = NULL; IDWriteFontList *list = NULL; IDWriteFont *a = NULL, *b = NULL;
    ttf_style_spec(&s[0], "FoMatch", 400, 5, 0); ttf_style_spec(&s[1], "FoMatch", 700, 5, 0); ttf_style_spec(&s[2], "FoMatch", 400, 5, 1);
    FO_OPEN(x, s, 3);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &list));
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &a));
        if (list && a) { CHECK(IDWriteFontList_GetFontCount(list) >= 1); CHECK_HR_OK(IDWriteFontList_GetFont(list, 0, &b)); if (b) CHECK(same_font(a, b) == 1); }
        DW_RELEASE(b); DW_RELEASE(a); DW_RELEASE(list); DW_RELEASE(fam);
    }
    fo_close(&x);
}
TEST(font_family_get_font_by_index_covers_all_members) {
    ttf_spec s[3]; fo_fx x; IDWriteFontFamily *fam = NULL; int i; UINT32 j, n; int found[3] = { 0 };
    ttf_style_spec(&s[0], "FoIdx", 400, 5, 0); ttf_style_spec(&s[1], "FoIdx", 700, 5, 0); ttf_style_spec(&s[2], "FoIdx", 400, 5, 1);
    FO_OPEN(x, s, 3);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        n = IDWriteFontFamily_GetFontCount(fam);
        for (j = 0; j < n; j++) { IDWriteFont *f = NULL; if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, j, &f)) && f) { for (i = 0; i < 3; i++) if (same_font(f, x.font[i]) == 1) found[i] = 1; DW_RELEASE(f); } }
        for (i = 0; i < 3; i++) CHECKF(found[i], "face %d not reachable by index", i);
        DW_RELEASE(fam);
    }
    fo_close(&x);
}
TEST(font_family_get_font_index_out_of_range) {
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL; IDWriteFont *f = (IDWriteFont *)(ULONG_PTR)1; fo_spec(&s, "FoIdx2"); FO_OPEN1(x, s);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        UINT32 n = IDWriteFontFamily_GetFontCount(fam);
        HRESULT hr = IDWriteFontFamily_GetFont(fam, n, &f); CHECKF(FAILED(hr), "GetFont(count) -> %s", dw_hr_name(hr)); t_note("GetFont(index == count) -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(f);
        f = (IDWriteFont *)(ULONG_PTR)1; hr = IDWriteFontFamily_GetFont(fam, 0xFFFFFFFF, &f); CHECKF(FAILED(hr), "GetFont(-1) -> %s", dw_hr_name(hr)); CHECK_OUT_NULL(f);
        DW_RELEASE(fam);
    }
    fo_close(&x);
}

/* ---- GetFontFromFontFace ------------------------------------------------------------------------------------------------------- */
TEST(font_from_face_roundtrip_synthetic) {
    ttf_spec s[2]; fo_fx x; IDWriteFontFace *face = NULL; IDWriteFont *back = (IDWriteFont *)(ULONG_PTR)1; int i;
    ttf_style_spec(&s[0], "FoGff", 400, 5, 0); ttf_style_spec(&s[1], "FoGff", 700, 5, 1); FO_OPEN(x, s, 2);
    for (i = 0; i < 2; i++) {
        back = (IDWriteFont *)(ULONG_PTR)1; IDWriteFont_CreateFontFace(x.font[i], &face);
        if (face) {
            CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(x.coll, face, &back)); CHECK(back != NULL && back != (void *)1);
            if (back && back != (void *)1) { CHECK(same_font(back, x.font[i]) == 1); EXPECT_EQ(IDWriteFont_GetWeight(back), IDWriteFont_GetWeight(x.font[i])); EXPECT_EQ(IDWriteFont_GetStyle(back), IDWriteFont_GetStyle(x.font[i])); DW_RELEASE(back); }
            DW_RELEASE(face);
        }
    }
    fo_close(&x);
}
TEST(font_from_face_stranger_face_is_nofont) {
    /* a face from the same loader whose file is not part of the collection */
    fo_fx x; ttf_spec s, s2; IDWriteFontFace *face = NULL; IDWriteFont *back = (IDWriteFont *)(ULONG_PTR)1; IDWriteFontFile *file = NULL; size_t n; uint8_t *b; UINT32 key;
    fo_spec(&s, "FoGffIn"); FO_OPEN1(x, s);
    fo_spec(&s2, "FoGffOut"); b = ttf_build(&s2, &n); key = dwx_loader_add(x.fl, b, n); free(b);
    if (SUCCEEDED(dwx_loader_file(x.fl, x.f, key, &file))) {
        IDWriteFontFile *files[1]; files[0] = file;
        if (SUCCEEDED(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, files, 0, 0, &face)) && face) {
            HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(x.coll, face, &back);
            CHECK_HR(hr, DWRITE_E_NOFONT); CHECK_OUT_NULL(back);
        } else CHECK(0);
    } else CHECK(0);
    DW_RELEASE(face); DW_RELEASE(file); fo_close(&x);
}
TEST(font_from_face_simulations_come_from_face) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL; IDWriteFontFace3 *face = NULL; IDWriteFont *back = NULL; fo_spec(&s, "FoGffSim"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); if (f3) IDWriteFont3_GetFontFaceReference(f3, &r);
    if (r) {
        IDWriteFontFaceReference_CreateFontFaceWithSimulations(r, DWRITE_FONT_SIMULATIONS_BOLD, &face);
        if (face) {
            HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(x.coll, (IDWriteFontFace *)face, &back);
            if (SUCCEEDED(hr) && back) { EXPECT_EQ(IDWriteFont_GetSimulations(back), DWRITE_FONT_SIMULATIONS_BOLD); EXPECT_EQ(IDWriteFont_GetWeight(back), DWRITE_FONT_WEIGHT_BOLD); }
            else t_note("GetFontFromFontFace for a simulated face: %s", dw_hr_name(hr));
        }
    }
    DW_RELEASE(back); DW_RELEASE(face); DW_RELEASE(r); DW_RELEASE(f3); fo_close(&x);
}
TEST(font_from_face_system_arial) {
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontCollection *c = NULL; IDWriteFont *font = NULL;
    if (!face) { t_skip("Arial not installed"); return; }
    IDWriteFactory_GetSystemFontCollection(f, &c, FALSE);
    if (c) { CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(c, face, &font)); if (font) { EXPECT_EQ(IDWriteFont_GetWeight(font), 700); EXPECT_EQ(IDWriteFont_GetSimulations(font), 0); DW_RELEASE(font); } }
    DW_RELEASE(c); DW_RELEASE(face);
}
TEST(font_from_face_synthetic_face_not_in_system_collection) {
    dwx_font h; IDWriteFontCollection *c = NULL; IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1; IDWriteFactory *f = dw_shared();
    memset(&h, 0, sizeof h);
    if (FAILED(dwx_font_open_default(&h, f)) || !h.face) { dwx_font_close(&h); t_skip("synthetic face unavailable"); return; }
    IDWriteFactory_GetSystemFontCollection(f, &c, FALSE);
    if (c) { CHECK_HR(IDWriteFontCollection_GetFontFromFontFace(c, h.face, &font), DWRITE_E_NOFONT); CHECK_OUT_NULL(font); }
    DW_RELEASE(c); dwx_font_close(&h);
}
TEST(font_from_face_returns_new_object_each_time) {
    fo_fx x; ttf_spec s; IDWriteFontFace *face = NULL; IDWriteFont *a = NULL, *b = NULL; fo_spec(&s, "FoGff2"); FO_OPEN1(x, s);
    IDWriteFont_CreateFontFace(x.font[0], &face);
    if (face) {
        IDWriteFontCollection_GetFontFromFontFace(x.coll, face, &a); IDWriteFontCollection_GetFontFromFontFace(x.coll, face, &b);
        CHECK(a && b);
        if (a && b) { if (a != b) { EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); } CHECK(same_font(a, b) == 1); }
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(face); fo_close(&x);
}

/* ---- GDI interop ------------------------------------------------------------------------------------------------------------------ */
static LOGFONTW mklf(const WCHAR *name, int weight, int italic) {
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfWeight = weight; lf.lfItalic = (BYTE)italic; lf.lfCharSet = DEFAULT_CHARSET; lstrcpynW(lf.lfFaceName, name, LF_FACESIZE); return lf;
}
static IDWriteGdiInterop *gdi(void) { IDWriteGdiInterop *g = NULL; IDWriteFactory_GetGdiInterop(dw_shared(), &g); return g; }
TEST(font_gdi_from_logfont_arial_variants) {
    IDWriteGdiInterop *g = gdi(); static const struct { int w, it; int ww, ws; } t[] = { { 400, 0, 400, 0 }, { 700, 0, 700, 0 }, { 400, 1, 400, 2 }, { 700, 1, 700, 2 } };
    int k;
    if (!g) { t_skip("no GDI interop"); return; }
    for (k = 0; k < 4; k++) {
        LOGFONTW lf = mklf(L"Arial", t[k].w, t[k].it); IDWriteFont *f = (IDWriteFont *)(ULONG_PTR)1; HRESULT hr = IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &f);
        if (hr == DWRITE_E_NOFONT) { t_skip("Arial not installed"); break; }
        CHECK_HR_OK(hr);
        if (SUCCEEDED(hr) && f) { EXPECT_EQ(IDWriteFont_GetWeight(f), t[k].ww); EXPECT_EQ(IDWriteFont_GetStyle(f), t[k].ws); EXPECT_EQ(IDWriteFont_GetSimulations(f), 0); DW_RELEASE(f); }
    }
    DW_RELEASE(g);
}
TEST(font_gdi_from_logfont_null_and_unknown) {
    IDWriteGdiInterop *g = gdi(); IDWriteFont *f = (IDWriteFont *)(ULONG_PTR)1; LOGFONTW lf;
    if (!g) { t_skip("no GDI interop"); return; }
    CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, NULL, &f), E_INVALIDARG); CHECK_OUT_NULL(f);
    lf = mklf(L"NoSuchFontFamilyXyzzy", 400, 0); f = (IDWriteFont *)(ULONG_PTR)1;
    CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &f), DWRITE_E_NOFONT); CHECK_OUT_NULL(f);
    lf = mklf(L"", 400, 0); f = (IDWriteFont *)(ULONG_PTR)1;
    { HRESULT hr = IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &f); t_note("empty face name -> %s", dw_hr_name(hr)); DW_RELEASE(f); }
    DW_RELEASE(g);
}
TEST(font_gdi_from_logfont_case_insensitive_name_note) {
    IDWriteGdiInterop *g = gdi(); LOGFONTW lf = mklf(L"ARIAL", 400, 0); IDWriteFont *f = NULL; HRESULT hr;
    if (!g) { t_skip("no GDI interop"); return; }
    hr = IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &f); t_note("face name 'ARIAL' -> %s", dw_hr_name(hr)); DW_RELEASE(f); DW_RELEASE(g);
}
TEST(font_gdi_convert_null_font) {
    IDWriteGdiInterop *g = gdi(); LOGFONTW lf; BOOL sys = 77; unsigned char *p = (unsigned char *)&lf; size_t i;
    if (!g) { t_skip("no GDI interop"); return; }
    memset(&lf, 0xCD, sizeof lf);
    CHECK_HR(IDWriteGdiInterop_ConvertFontToLOGFONT(g, NULL, &lf, &sys), E_INVALIDARG);
    EXPECT_EQ(sys, FALSE);
    for (i = 0; i < 0x5c; i++) if (p[i] != 0) { CHECKF(0, "LOGFONT byte %zu not zeroed (0x%02x)", i, p[i]); break; }       /* ConvertFontToLOGFONT clears 0x5c bytes first */
    DW_RELEASE(g);
}
TEST(font_gdi_convert_arial_fields) {
    FO_SYS_REG(f, L"Arial"); IDWriteGdiInterop *g = gdi(); LOGFONTW lf; BOOL sys = 77;
    if (!g) { DW_RELEASE(f); t_skip("no GDI interop"); return; }
    memset(&lf, 0xCD, sizeof lf);
    CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(g, f, &lf, &sys));
    EXPECT_EQ(sys, TRUE); CHECK(!lstrcmpW(lf.lfFaceName, L"Arial")); EXPECT_EQ(lf.lfWeight, 400); EXPECT_EQ(lf.lfItalic, 0);
    EXPECT_EQ(lf.lfHeight, 0); EXPECT_EQ(lf.lfWidth, 0); EXPECT_EQ(lf.lfEscapement, 0); EXPECT_EQ(lf.lfOrientation, 0); EXPECT_EQ(lf.lfUnderline, 0); EXPECT_EQ(lf.lfStrikeOut, 0);
    EXPECT_EQ(lf.lfCharSet, DEFAULT_CHARSET); EXPECT_EQ(lf.lfOutPrecision, 8); EXPECT_EQ(lf.lfClipPrecision, 0); EXPECT_EQ(lf.lfQuality, 0); EXPECT_EQ(lf.lfPitchAndFamily, 0);
    DW_RELEASE(g); DW_RELEASE(f);
}
TEST(font_gdi_convert_bold_italic) {
    FO_SYS(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC); IDWriteGdiInterop *g = gdi(); LOGFONTW lf; BOOL sys = 0;
    if (!g) { DW_RELEASE(f); t_skip("no GDI interop"); return; }
    CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(g, f, &lf, &sys));
    EXPECT_EQ(lf.lfWeight, 700); EXPECT_EQ(lf.lfItalic, 1); CHECK(!lstrcmpW(lf.lfFaceName, L"Arial")); EXPECT_EQ(sys, TRUE);
    DW_RELEASE(g); DW_RELEASE(f);
}
TEST(font_gdi_convert_roundtrip_system_fonts) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Verdana" };
    IDWriteGdiInterop *g = gdi(); int k, seen = 0;
    if (!g) { t_skip("no GDI interop"); return; }
    for (k = 0; k < 6; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL), *back = NULL; LOGFONTW lf; BOOL sys;
        if (!f) continue;
        seen++;
        if (SUCCEEDED(IDWriteGdiInterop_ConvertFontToLOGFONT(g, f, &lf, &sys)) && SUCCEEDED(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &back)) && back) CHECKF(same_font(f, back) == 1, "%ls bold roundtrip", fams[k]);
        else CHECKF(0, "%ls roundtrip failed", fams[k]);
        DW_RELEASE(back); DW_RELEASE(f);
    }
    if (!seen) t_skip("no system font installed");
    DW_RELEASE(g);
}
TEST(font_gdi_convert_custom_font_is_not_system) {
    fo_fx x; ttf_spec s; IDWriteGdiInterop *g = NULL; LOGFONTW lf; BOOL sys = 77; fo_spec(&s, "FoGdiCust"); FO_OPEN1(x, s);
    IDWriteFactory_GetGdiInterop(x.f, &g);
    if (g) {
        CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(g, x.font[0], &lf, &sys)); EXPECT_EQ(sys, FALSE);
        CHECK(!lstrcmpW(lf.lfFaceName, L"FoGdiCust")); EXPECT_EQ(lf.lfWeight, 400); EXPECT_EQ(lf.lfItalic, 0); EXPECT_EQ(lf.lfCharSet, DEFAULT_CHARSET); EXPECT_EQ(lf.lfOutPrecision, 8);
        DW_RELEASE(g);
    }
    fo_close(&x);
}
TEST(font_gdi_convert_symbol_font_charset) {
    fo_fx x; ttf_spec s; IDWriteGdiInterop *g = NULL; LOGFONTW lf; BOOL sys; fo_symbol_spec(&s, "FoGdiSym"); FO_OPEN1(x, s);
    IDWriteFactory_GetGdiInterop(x.f, &g);
    if (g) { CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(g, x.font[0], &lf, &sys)); EXPECT_EQ(lf.lfCharSet, SYMBOL_CHARSET); DW_RELEASE(g); }
    fo_close(&x);
}
TEST(font_gdi_convert_long_family_name_truncated) {
    fo_fx x; ttf_spec s; IDWriteGdiInterop *g = NULL; LOGFONTW lf; BOOL sys; char name[64]; int i;
    for (i = 0; i < 45; i++) name[i] = (char)('A' + i % 26);
    name[45] = 0; fo_spec(&s, name); FO_OPEN1(x, s);
    IDWriteFactory_GetGdiInterop(x.f, &g);
    if (g) {
        CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(g, x.font[0], &lf, &sys));
        CHECKF(lstrlenW(lf.lfFaceName) == 31, "face name length %d (LF_FACESIZE 32, copied at most 31 characters)", lstrlenW(lf.lfFaceName));
        EXPECT_EQ(lf.lfFaceName[31], 0); EXPECT_EQ(lf.lfFaceName[0], 'A'); EXPECT_EQ(lf.lfFaceName[30], 'A' + 30 % 26);
        DW_RELEASE(g);
    }
    fo_close(&x);
}
TEST(font_gdi_convert_ribbi_weights) {
    ttf_spec s[4]; fo_fx x; IDWriteGdiInterop *g = NULL; int i; static const int w[4] = { 400, 700, 400, 700 }, it[4] = { 0, 0, 1, 1 };
    ttf_style_spec(&s[0], "FoGdiR", 400, 5, 0); ttf_style_spec(&s[1], "FoGdiR", 700, 5, 0); ttf_style_spec(&s[2], "FoGdiR", 400, 5, 1); ttf_style_spec(&s[3], "FoGdiR", 700, 5, 1); FO_OPEN(x, s, 4);
    IDWriteFactory_GetGdiInterop(x.f, &g);
    if (g) {
        for (i = 0; i < 4; i++) { LOGFONTW lf; BOOL sys; CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(g, x.font[i], &lf, &sys)); EXPECT_EQ(lf.lfWeight, w[i]); EXPECT_EQ(lf.lfItalic, it[i]); CHECK(!lstrcmpW(lf.lfFaceName, L"FoGdiR")); }
        DW_RELEASE(g);
    }
    fo_close(&x);
}
TEST(font_gdi_convert_non_ribbi_note) {
    ttf_spec s[2]; fo_fx x; IDWriteGdiInterop *g = NULL; int i; ttf_style_spec(&s[0], "FoGdiN", 300, 5, 0); ttf_style_spec(&s[1], "FoGdiN", 400, 3, 0); FO_OPEN(x, s, 2);
    IDWriteFactory_GetGdiInterop(x.f, &g);
    if (g) { for (i = 0; i < 2; i++) { LOGFONTW lf; BOOL sys; if (SUCCEEDED(IDWriteGdiInterop_ConvertFontToLOGFONT(g, x.font[i], &lf, &sys))) t_note("font %d: face '%ls' weight %ld italic %d", i, lf.lfFaceName, lf.lfWeight, lf.lfItalic); } DW_RELEASE(g); }
    fo_close(&x);
}
TEST(font_gdi_symbol_font_from_logfont_note) {
    static const WCHAR *fams[] = { L"Wingdings", L"Marlett", L"Symbol", L"Webdings" };
    IDWriteGdiInterop *g = gdi(); int k;
    if (!g) { t_skip("no GDI interop"); return; }
    for (k = 0; k < 4; k++) {
        LOGFONTW lf = mklf(fams[k], 400, 0); IDWriteFont *f = NULL; lf.lfCharSet = SYMBOL_CHARSET;
        if (SUCCEEDED(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &f)) && f) { t_note("%ls: IsSymbolFont=%d", fams[k], IDWriteFont_IsSymbolFont(f)); CHECKF(IDWriteFont_IsSymbolFont(f) == TRUE, "%ls", fams[k]); DW_RELEASE(f); }
        else t_note("%ls not available", fams[k]);
    }
    DW_RELEASE(g);
}
TEST(font_gdi_weight_selection_segoe_note) {
    IDWriteGdiInterop *g = gdi(); static const int w[] = { 100, 300, 350, 400, 500, 600, 700, 900 }; int k;
    if (!g) { t_skip("no GDI interop"); return; }
    for (k = 0; k < 8; k++) { LOGFONTW lf = mklf(L"Segoe UI", w[k], 0); IDWriteFont *f = NULL; if (SUCCEEDED(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &f)) && f) { t_note("Segoe UI lfWeight %d -> font weight %d sim %d", w[k], (int)IDWriteFont_GetWeight(f), (int)IDWriteFont_GetSimulations(f)); DW_RELEASE(f); } }
    DW_RELEASE(g);
}

/* ---- whole-collection invariants ------------------------------------------------------------------------------------------------------ */
TEST(font_sweep_system_collection_invariants) {
    IDWriteFontCollection *c = NULL; UINT32 nfam, fi, k, checked = 0, nofn = 0;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(dw_shared(), &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    nfam = IDWriteFontCollection_GetFontFamilyCount(c);
    for (fi = 0; fi < nfam && checked < 250; fi += 1 + nfam / 120) {
        IDWriteFontFamily *fam = NULL; UINT32 nf;
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, fi, &fam)) || !fam) { CHECK(0); continue; }
        nf = IDWriteFontFamily_GetFontCount(fam);
        for (k = 0; k < nf && k < 3; k++) {
            IDWriteFont *f = NULL; DWRITE_FONT_METRICS m; IDWriteLocalizedStrings *fn = NULL; IDWriteFontFamily *back = NULL; IDWriteLocalizedStrings *info = NULL; BOOL ex = 0;
            UINT32 nr = 0; IDWriteFont1 *f1 = NULL; HRESULT hr; int w, st, sty, sim;
            if (FAILED(IDWriteFontFamily_GetFont(fam, k, &f)) || !f) { CHECK(0); continue; }
            checked++;
            w = IDWriteFont_GetWeight(f); st = IDWriteFont_GetStretch(f); sty = IDWriteFont_GetStyle(f); sim = IDWriteFont_GetSimulations(f);
            CHECKF(w >= 1 && w <= 999, "family %u font %u weight %d", fi, k, w); CHECKF(st >= 1 && st <= 9, "family %u font %u stretch %d", fi, k, st);
            CHECKF(sty >= 0 && sty <= 2, "style %d", sty); CHECKF((sim & ~3) == 0, "simulations %d", sim);
            IDWriteFont_GetMetrics(f, &m); CHECKF(m.designUnitsPerEm >= 16 && m.designUnitsPerEm <= 16384, "upm %u", m.designUnitsPerEm); CHECK(m.ascent + m.descent > 0);
            if (SUCCEEDED(IDWriteFont_GetFaceNames(f, &fn)) && fn) { CHECK(IDWriteLocalizedStrings_GetCount(fn) >= 1); { char b[128]; dw_ls_get(fn, 0, b, sizeof b); CHECK(b[0] != 0); } DW_RELEASE(fn); } else CHECK(0);
            if (SUCCEEDED(IDWriteFont_GetFontFamily(f, &back)) && back) { CHECK(back == fam); DW_RELEASE(back); } else CHECK(0);
            CHECK(same_font(f, f) == 1);
            CHECK_HR_OK(IDWriteFont_GetInformationalStrings(f, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &info, &ex)); if (!ex) nofn++; CHECK((ex != 0) == (info != NULL)); DW_RELEASE(info);
            { int v = IDWriteFont_IsSymbolFont(f); CHECK(v == 0 || v == 1); }
            if (SUCCEEDED(IDWriteFont_QueryInterface(f, &IID_IDWriteFont1, (void **)&f1)) && f1) {
                hr = IDWriteFont1_GetUnicodeRanges(f1, 0, NULL, &nr);
                CHECKF((hr == S_OK && nr == 0) || (hr == DW_E_NOT_SUFFICIENT_BUFFER && nr > 0), "family %u font %u ranges hr=%s n=%u", fi, k, dw_hr_name(hr), nr);
                { int v = IDWriteFont1_IsMonospacedFont(f1); CHECK(v == 0 || v == 1); }
                IDWriteFont1_Release(f1);
            }
            DW_RELEASE(f);
        }
        DW_RELEASE(fam);
    }
    t_note("checked %u fonts of %u families; %u without a Win32 family name", checked, nfam, nofn);
    CHECK(checked > 0);
    DW_RELEASE(c);
}
TEST(font_sweep_system_faces_agree_with_fonts) {
    IDWriteFontCollection *c = NULL; UINT32 nfam, fi, checked = 0;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(dw_shared(), &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    nfam = IDWriteFontCollection_GetFontFamilyCount(c);
    for (fi = 0; fi < nfam && checked < 120; fi += 1 + nfam / 80) {
        IDWriteFontFamily *fam = NULL; IDWriteFont *f = NULL; IDWriteFontFace *face = NULL, *face2 = NULL; IDWriteFont *back = NULL; DWRITE_FONT_METRICS a, b;
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, fi, &fam)) || !fam) continue;
        if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &f)) && f && SUCCEEDED(IDWriteFont_CreateFontFace(f, &face)) && face) {
            checked++;
            IDWriteFont_GetMetrics(f, &a); IDWriteFontFace_GetMetrics(face, &b); CHECK(!memcmp(&a, &b, sizeof a));
            EXPECT_EQ(IDWriteFontFace_IsSymbolFont(face), IDWriteFont_IsSymbolFont(f)); EXPECT_EQ(IDWriteFontFace_GetSimulations(face), IDWriteFont_GetSimulations(f));
            if (SUCCEEDED(IDWriteFontCollection_GetFontFromFontFace(c, face, &back)) && back) { CHECK(same_font(f, back) == 1); DW_RELEASE(back); } else CHECK(0);
            IDWriteFont_CreateFontFace(f, &face2); if (face2) { EXPECT_EQ(IDWriteFontFace_GetIndex(face2), IDWriteFontFace_GetIndex(face)); DW_RELEASE(face2); }
        }
        DW_RELEASE(face); DW_RELEASE(f); DW_RELEASE(fam);
    }
    t_note("compared %u faces", checked); DW_RELEASE(c);
}

/* ---- threads ------------------------------------------------------------------------------------------------------------------------------- */
typedef struct { IDWriteFont *font; LONG bad; LONG iters; } thr_arg;
static DWORD WINAPI thr_main(LPVOID p) {
    thr_arg *a = (thr_arg *)p; LONG i;
    for (i = 0; i < a->iters; i++) {
        BOOL e = 9; IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *ls = NULL; DWRITE_FONT_METRICS m; IDWriteFont1 *f1 = NULL;
        if (IDWriteFont_GetWeight(a->font) != 400) a->bad++;
        if (FAILED(IDWriteFont_HasCharacter(a->font, 'A', &e)) || e != TRUE) a->bad++;
        if (FAILED(IDWriteFont_HasCharacter(a->font, 0x4E00, &e)) || e != FALSE) a->bad++;
        IDWriteFont_GetMetrics(a->font, &m); if (m.designUnitsPerEm != 2048) a->bad++;
        if (SUCCEEDED(IDWriteFont_GetFontFamily(a->font, &fam)) && fam) IDWriteFontFamily_Release(fam); else a->bad++;
        if (SUCCEEDED(IDWriteFont_GetFaceNames(a->font, &ls)) && ls) IDWriteLocalizedStrings_Release(ls); else a->bad++;
        if (SUCCEEDED(IDWriteFont_QueryInterface(a->font, &IID_IDWriteFont1, (void **)&f1)) && f1) {
            UINT32 n = 0; if (IDWriteFont1_GetUnicodeRanges(f1, 0, NULL, &n) != DW_E_NOT_SUFFICIENT_BUFFER || n != 4) a->bad++;
            IDWriteFont1_Release(f1);
        } else a->bad++;
    }
    return 0;
}
TEST(font_concurrent_use_from_threads) {
    fo_fx x; ttf_spec s; thr_arg arg[4]; HANDLE h[4]; int i; ULONG base; fo_spec(&s, "FoThr"); FO_OPEN1(x, s);
    base = dw_refcount((IUnknown *)x.font[0]);
    for (i = 0; i < 4; i++) { arg[i].font = x.font[0]; arg[i].bad = 0; arg[i].iters = 400; h[i] = CreateThread(NULL, 0, thr_main, &arg[i], 0, NULL); }
    for (i = 0; i < 4; i++) {
        if (!h[i]) { CHECK(0); continue; }
        if (WaitForSingleObject(h[i], 30000) != WAIT_OBJECT_0) { CHECKF(0, "thread %d did not finish", i); continue; }
        CloseHandle(h[i]); CHECKF(arg[i].bad == 0, "thread %d saw %ld inconsistent results", i, arg[i].bad);
    }
    EXPECT_REFS(x.font[0], base);
    fo_close(&x);
}
static DWORD WINAPI addref_loop(LPVOID p) { IDWriteFont *f = *(IDWriteFont **)p; int k; for (k = 0; k < 5000; k++) { IDWriteFont_AddRef(f); IDWriteFont_Release(f); } return 0; }
TEST(font_concurrent_refcount_balance) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoThr2"); FO_OPEN1(x, s);
    {
        struct { IDWriteFont *f; } a = { x.font[0] }; HANDLE h[3]; int i; ULONG base = dw_refcount((IUnknown *)x.font[0]);
        for (i = 0; i < 3; i++) h[i] = CreateThread(NULL, 0, addref_loop, &a, 0, NULL);
        for (i = 0; i < 3; i++) if (h[i]) { WaitForSingleObject(h[i], 30000); CloseHandle(h[i]); }
        EXPECT_REFS(x.font[0], base);
    }
    fo_close(&x);
}
