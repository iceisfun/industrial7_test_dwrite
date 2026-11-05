/* The extension interfaces must be available on a face however it was created: factory + file, IDWriteFont, GDI interop (LOGFONT, HDC),
 * a font face reference, a font resource and a custom font collection. All of them are IDWriteFontFace objects, so they answer
 * IDWriteFontFace .. IDWriteFontFace5 with one pointer and agree with each other on identity (IDWriteFontFace5::Equals). */
#include "ffx.h"

static void chain_ok(IDWriteFontFace *f, const char *what) {
    IDWriteFontFace1 *a1 = NULL; IDWriteFontFace2 *a2 = NULL; IDWriteFontFace3 *a3 = NULL; IDWriteFontFace4 *a4 = NULL; IDWriteFontFace5 *a5 = NULL;
    HRESULT h1 = IDWriteFontFace_QueryInterface(f, &IID_IDWriteFontFace1, (void **)&a1), h2 = IDWriteFontFace_QueryInterface(f, &IID_IDWriteFontFace2, (void **)&a2),
            h3 = IDWriteFontFace_QueryInterface(f, &IID_IDWriteFontFace3, (void **)&a3), h4 = IDWriteFontFace_QueryInterface(f, &IID_IDWriteFontFace4, (void **)&a4),
            h5 = IDWriteFontFace_QueryInterface(f, &IID_IDWriteFontFace5, (void **)&a5);
    t_check(h1 == S_OK && h2 == S_OK && h3 == S_OK && h4 == S_OK && h5 == S_OK, __FILE__, __LINE__, what, "QI results %s %s %s %s %s", dw_hr_name(h1), dw_hr_name(h2), dw_hr_name(h3), dw_hr_name(h4), dw_hr_name(h5));
    t_check(FFX_SAME(a1, f) && FFX_SAME(a2, f) && FFX_SAME(a3, f) && FFX_SAME(a4, f) && FFX_SAME(a5, f), __FILE__, __LINE__, what, "interface pointers differ");
    if (a1) {
        DWRITE_FONT_METRICS1 m; DWRITE_FONT_METRICS b; ffx_c_metrics1(a1, &m); IDWriteFontFace_GetMetrics(f, &b);
        t_check(!memcmp(&b, &m, sizeof b) && m.designUnitsPerEm > 0, __FILE__, __LINE__, what, "metrics1 disagree with metrics");
    }
    DW_RELEASE(a1); DW_RELEASE(a2); DW_RELEASE(a3); DW_RELEASE(a4); DW_RELEASE(a5);
}
static IDWriteFont *sys_font(const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontFamily *ff = NULL; IDWriteFont *font = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (!f || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return NULL;
    IDWriteFontCollection_FindFamilyName(c, fam, &idx, &ex);
    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &ff)) && ff) { IDWriteFontFamily_GetFirstMatchingFont(ff, w, DWRITE_FONT_STRETCH_NORMAL, st, &font); IDWriteFontFamily_Release(ff); }
    IDWriteFontCollection_Release(c);
    return font;
}

TEST(origin_font_create_face) {
    IDWriteFont *font = sys_font(L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    if (!font) { t_skip("Arial missing"); return; }
    IDWriteFontFace *face = NULL; CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &face));
    if (face) {
        chain_ok(face, "IDWriteFont::CreateFontFace");
        IDWriteFontFace3 *f3 = NULL; IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3);
        if (f3) { EXPECT_EQ(IDWriteFontFace3_GetWeight(f3), IDWriteFont_GetWeight(font)); EXPECT_EQ(IDWriteFontFace3_GetStyle(f3), IDWriteFont_GetStyle(font)); EXPECT_EQ(IDWriteFontFace3_GetStretch(f3), IDWriteFont_GetStretch(font)); IDWriteFontFace3_Release(f3); }
        IDWriteFontFace_Release(face);
    }
    IDWriteFont_Release(font);
}
TEST(origin_font3_create_face) {
    IDWriteFont *font = sys_font(L"Segoe UI", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    if (!font) { t_skip("Segoe UI missing"); return; }
    IDWriteFont3 *f3 = NULL; IDWriteFontFace3 *face = NULL;
    if (SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&f3)) && f3) {
        CHECK_HR_OK(IDWriteFont3_CreateFontFace(f3, &face));
        if (face) { chain_ok((IDWriteFontFace *)face, "IDWriteFont3::CreateFontFace"); IDWriteFontFace3_Release(face); }
        IDWriteFont3_Release(f3);
    } else t_skip("IDWriteFont3 unavailable");
    IDWriteFont_Release(font);
}
TEST(origin_font_face_and_font3_face_equal) {
    IDWriteFont *font = sys_font(L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    if (!font) { t_skip("Arial missing"); return; }
    IDWriteFontFace *a = NULL; IDWriteFont3 *f3 = NULL; IDWriteFontFace3 *b = NULL;
    IDWriteFont_CreateFontFace(font, &a);
    if (SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&f3)) && f3) IDWriteFont3_CreateFontFace(f3, &b);
    if (a && b) {
        IDWriteFontFace5 *a5 = NULL; IDWriteFontFace_QueryInterface(a, &IID_IDWriteFontFace5, (void **)&a5);
        if (a5) { EXPECT_EQ(IDWriteFontFace5_Equals(a5, (IDWriteFontFace *)b), TRUE); IDWriteFontFace5_Release(a5); }
        t_note("IDWriteFont::CreateFontFace and IDWriteFont3::CreateFontFace return %s object", FFX_SAME(a, b) ? "the same" : "different");
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f3); IDWriteFont_Release(font);
}
TEST(origin_gdi_logfont_face) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL;
    if (!f || FAILED(IDWriteFactory_GetGdiInterop(f, &gi)) || !gi) { t_skip("no GDI interop"); return; }
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lf.lfFaceName, L"Arial");
    IDWriteFont *font = NULL; HRESULT hr = IDWriteGdiInterop_CreateFontFromLOGFONT(gi, &lf, &font);
    if (SUCCEEDED(hr) && font) {
        IDWriteFontFace *face = NULL; CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &face));
        if (face) {
            chain_ok(face, "GDI LOGFONT face");
            IDWriteFont *direct = sys_font(L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
            if (direct) {
                IDWriteFontFace *face2 = NULL; IDWriteFont_CreateFontFace(direct, &face2);
                IDWriteFontFace5 *f5 = NULL; if (face2) IDWriteFontFace_QueryInterface(face2, &IID_IDWriteFontFace5, (void **)&f5);
                if (f5) { EXPECT_EQ(IDWriteFontFace5_Equals(f5, face), TRUE); IDWriteFontFace5_Release(f5); }
                DW_RELEASE(face2); IDWriteFont_Release(direct);
            }
            IDWriteFontFace_Release(face);
        }
        IDWriteFont_Release(font);
    } else t_skip("CreateFontFromLOGFONT: %s", dw_hr_name(hr));
    IDWriteGdiInterop_Release(gi);
}
TEST(origin_gdi_hdc_face) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL;
    if (!f || FAILED(IDWriteFactory_GetGdiInterop(f, &gi)) || !gi) { t_skip("no GDI interop"); return; }
    HDC dc = CreateCompatibleDC(NULL);
    LOGFONTW lf; memset(&lf, 0, sizeof lf); lf.lfHeight = -20; lf.lfWeight = FW_BOLD; lf.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lf.lfFaceName, L"Arial");
    HFONT hf = CreateFontIndirectW(&lf); HGDIOBJ old = dc ? SelectObject(dc, hf) : NULL;
    IDWriteFontFace *face = NULL; HRESULT hr = dc ? IDWriteGdiInterop_CreateFontFaceFromHdc(gi, dc, &face) : E_FAIL;
    if (SUCCEEDED(hr) && face) {
        chain_ok(face, "CreateFontFaceFromHdc");
        IDWriteFontFace3 *f3 = NULL; IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3);
        if (f3) { t_note("HDC face (Arial bold request): weight %d style %d", (int)IDWriteFontFace3_GetWeight(f3), (int)IDWriteFontFace3_GetStyle(f3)); CHECK(IDWriteFontFace3_GetWeight(f3) >= 400); IDWriteFontFace3_Release(f3); }
        IDWriteFontFace_Release(face);
    } else t_skip("CreateFontFaceFromHdc: %s", dw_hr_name(hr));
    if (dc) { SelectObject(dc, old); DeleteObject(hf); DeleteDC(dc); }
    IDWriteGdiInterop_Release(gi);
}
TEST(origin_factory_file_face) {
    IDWriteFactory *f = dw_shared(); static const char *const cands[] = { "arial.ttf", "segoeui.ttf", "tahoma.ttf", "times.ttf", NULL };
    char path[MAX_PATH]; WCHAR wp[MAX_PATH];
    if (!f || !dw_find_system_font(cands, path, sizeof path)) { t_skip("no system font file"); return; }
    dw_wide(path, wp, MAX_PATH);
    IDWriteFontFile *file = NULL; HRESULT hr = IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file);
    if (FAILED(hr) || !file) { t_skip("CreateFontFileReference: %s", dw_hr_name(hr)); return; }
    BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf = 0;
    CHECK_HR_OK(IDWriteFontFile_Analyze(file, &sup, &ft, &fct, &nf));
    IDWriteFontFace *face = NULL;
    if (sup && nf >= 1) CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, fct, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    if (face) {
        chain_ok(face, "file-based face");
        IDWriteFontFace3 *f3 = NULL; IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3);
        IDWriteFontFaceReference *r = NULL;
        if (f3 && SUCCEEDED(IDWriteFontFace3_GetFontFaceReference(f3, &r)) && r) {
            EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_LOCAL);
            IDWriteFontFile *rf = NULL; if (SUCCEEDED(IDWriteFontFaceReference_GetFontFile(r, &rf)) && rf) { CHECK(rf != NULL); IDWriteFontFile_Release(rf); }
            IDWriteFontFaceReference_Release(r);
        }
        DW_RELEASE(f3);
        IDWriteFontFace_Release(face);
    }
    IDWriteFontFile_Release(file);
}
TEST(origin_factory3_reference_face) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    static const char *const cands[] = { "arial.ttf", "segoeui.ttf", "tahoma.ttf", NULL };
    char path[MAX_PATH]; WCHAR wp[MAX_PATH];
    if (!f3 || !dw_find_system_font(cands, path, sizeof path)) { t_skip("IDWriteFactory3 or a font file is unavailable"); return; }
    dw_wide(path, wp, MAX_PATH);
    IDWriteFontFaceReference *ref = NULL; HRESULT hr = IDWriteFactory3_CreateFontFaceReference(f3, wp, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    CHECK_HR_OK(hr);
    if (ref) {
        IDWriteFontFace3 *face = NULL; CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFace(ref, &face));
        if (face) { chain_ok((IDWriteFontFace *)face, "reference face"); IDWriteFontFace3_Release(face); }
        IDWriteFontFaceReference_Release(ref);
    }
}
TEST(origin_custom_collection_face) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no factory"); return; }
    ttf_spec s; ttf_style_spec(&s, "CollFam", 700, 5, 1); size_t n; uint8_t *b = ttf_build(&s, &n);
    dwx_mem_loader *fl = dwx_loader_new(); UINT32 k = dwx_loader_add(fl, b, n); free(b);
    dwx_loader_register(fl, f);
    dwx_coll_loader *cl = dwx_coll_new(fl); dwx_coll_add_key(cl, k); dwx_coll_register(cl, f);
    IDWriteFontCollection *coll = NULL; HRESULT hr = dwx_coll_create(cl, f, &coll);
    if (SUCCEEDED(hr) && coll) {
        IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(coll, 0, &fam));
        if (fam) CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, 0, &font));
        if (font) CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &face));
        if (face) {
            chain_ok(face, "custom collection face");
            IDWriteFontFace3 *f3 = NULL; IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3);
            if (f3) {
                EXPECT_EQ(IDWriteFontFace3_GetWeight(f3), DWRITE_FONT_WEIGHT_BOLD); EXPECT_EQ(IDWriteFontFace3_GetStyle(f3), DWRITE_FONT_STYLE_ITALIC);
                IDWriteLocalizedStrings *nm = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(f3, &nm));
                if (nm) { char bb[64]; dw_ls_en(nm, bb, sizeof bb); EXPECT_STR(bb, "CollFam"); IDWriteLocalizedStrings_Release(nm); }
                IDWriteFontFace3_Release(f3);
            }
            IDWriteFontFace_Release(face);
        }
        DW_RELEASE(font); DW_RELEASE(fam);
    } else t_note("custom collection: %s", dw_hr_name(hr));
    DW_RELEASE(coll);
    dwx_coll_unregister(cl); dwx_loader_unregister(fl); dwx_release(cl); dwx_release(fl);
}
TEST(origin_font_face_lifetime_after_font_release) {
    IDWriteFont *font = sys_font(L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    if (!font) { t_skip("Arial missing"); return; }
    IDWriteFontFace *face = NULL; IDWriteFont_CreateFontFace(font, &face);
    IDWriteFont_Release(font);
    if (face) {
        IDWriteFontFace1 *f1 = NULL; CHECK_HR_OK(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace1, (void **)&f1));
        IDWriteFontFace_Release(face);
        if (f1) { DWRITE_FONT_METRICS1 m; ffx_c_metrics1(f1, &m); EXPECT_EQ(m.designUnitsPerEm, 2048); IDWriteFontFace1_Release(f1); }
    }
}
TEST(origin_isolated_factory_face) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); if (!f) { t_skip("no isolated factory"); return; }
    dwx_font h; size_t n; uint8_t *b = ttf_build_default(&n);
    HRESULT hr = dwx_font_open(&h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE); free(b);
    if (SUCCEEDED(hr) && h.face) {
        chain_ok(h.face, "isolated factory face");
        IDWriteFontFace1 *f1 = NULL; IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace1, (void **)&f1);
        if (f1) { DWRITE_FONT_METRICS1 m; ffx_c_metrics1(f1, &m); EXPECT_EQ(m.glyphBoxTop, 2800); IDWriteFontFace1_Release(f1); }
    }
    dwx_font_close(&h);
    IDWriteFactory_Release(f);
}
TEST(origin_two_factories_same_bytes_not_equal) {
    IDWriteFactory *f1 = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *f2 = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!f1 || !f2) { DW_RELEASE(f1); DW_RELEASE(f2); t_skip("no isolated factories"); return; }
    dwx_font a, b; size_t n; uint8_t *blob = ttf_build_default(&n);
    HRESULT ha = dwx_font_open(&a, f1, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE), hb = dwx_font_open(&b, f2, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE); free(blob);
    if (SUCCEEDED(ha) && SUCCEEDED(hb) && a.face && b.face) {
        IDWriteFontFace5 *a5 = NULL; IDWriteFontFace_QueryInterface(a.face, &IID_IDWriteFontFace5, (void **)&a5);
        if (a5) { BOOL e = IDWriteFontFace5_Equals(a5, b.face); t_note("same bytes in two isolated factories: Equals=%d", e); CHECK(e == 0 || e == 1); IDWriteFontFace5_Release(a5); }
    }
    dwx_font_close(&a); dwx_font_close(&b);
    IDWriteFactory_Release(f1); IDWriteFactory_Release(f2);
}
