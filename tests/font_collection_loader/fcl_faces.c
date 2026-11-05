/* Fonts and faces of custom collections: GetFontFromFontFace, interface surface, font sets, family model, text layout selection,
 * leak balance.
 *
 * Win10 decomp: GetFontFromFontFace looks the face's physical key (file loader id + reference key + face index) up in the
 * collection's font set (then its canonical key); a hit builds an IDWriteFont with the FACE's simulations, a miss gives
 * DWRITE_E_NOFONT (0x88985002) with *ppFont = NULL.
 */
#include "fcl.h"

static void ref_key_of(IDWriteFontFace *face, UINT8 *buf, UINT32 *size, int *loader_matches, IDWriteFontFileLoader *expect)
{
    IDWriteFontFile *file = NULL; UINT32 n = 1; const void *key = NULL; UINT32 ks = 0;
    *size = 0; *loader_matches = 0;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, &file)) || !file) return;
    if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(file, &key, &ks)) && key) { memcpy(buf, key, ks < 32 ? ks : 32); *size = ks; }
    {
        IDWriteFontFileLoader *ld = NULL;
        if (SUCCEEDED(IDWriteFontFile_GetLoader(file, &ld)) && ld) {
            *loader_matches = dw_same_identity((IUnknown *)ld, (IUnknown *)expect);
            IDWriteFontFileLoader_Release(ld);
        }
    }
    IDWriteFontFile_Release(file);
}

/* ----------------------------------------------------------------------------------- GetFontFromFontFace */
TEST(fcl_face_roundtrip_every_font)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Round A", 400, 5, 0); fcl_add_style(&x, "Round A", 700, 5, 0); fcl_add_style(&x, "Round A", 400, 5, 1);
    fcl_add_style(&x, "Round B", 400, 5, 0);
    FCL_COLL(x, c);
    UINT32 nf = IDWriteFontCollection_GetFontFamilyCount(c), i, checked = 0;
    for (i = 0; i < nf; i++) {
        IDWriteFontFamily *fam = NULL;
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) || !fam) continue;
        UINT32 n = IDWriteFontFamily_GetFontCount(fam), k;
        for (k = 0; k < n; k++) {
            IDWriteFont *f = NULL, *back = (IDWriteFont *)(ULONG_PTR)1; IDWriteFontFace *face = NULL;
            if (FAILED(IDWriteFontFamily_GetFont(fam, k, &f)) || !f) continue;
            if (FAILED(IDWriteFont_CreateFontFace(f, &face)) || !face) { DW_RELEASE(f); continue; }
            HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(c, face, &back);
            CHECKF(hr == S_OK, "family %u font %u: %s", i, k, dw_hr_name(hr));
            if (hr == S_OK && back) {
                EXPECT_EQ(IDWriteFont_GetWeight(back), IDWriteFont_GetWeight(f));
                EXPECT_EQ(IDWriteFont_GetStyle(back), IDWriteFont_GetStyle(f));
                EXPECT_EQ(IDWriteFont_GetStretch(back), IDWriteFont_GetStretch(f));
                EXPECT_EQ(IDWriteFont_GetSimulations(back), IDWriteFont_GetSimulations(f));
                IDWriteFontFamily *bf = NULL;
                if (SUCCEEDED(IDWriteFont_GetFontFamily(back, &bf)) && bf) {
                    char a[64], b[64]; fcl_fam_name_of(fam, a, sizeof a); fcl_fam_name_of(bf, b, sizeof b);
                    CHECKF(!strcmp(a, b), "family %s vs %s", a, b);
                    IDWriteFontFamily_Release(bf);
                }
                checked++;
            } else CHECK_OUT_NULL(back);
            DW_RELEASE(back); DW_RELEASE(face); DW_RELEASE(f);
        }
        DW_RELEASE(fam);
    }
    CHECK(checked >= 4);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_face_not_in_collection)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Inside");
    UINT32 outside = fcl_hidden_style(&x, "Outside", 400, 5, 0);       /* loadable, but never enumerated */
    FCL_COLL(x, c);
    IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR_OK(dwx_loader_file(x.fl, x.f, outside, &file));
    if (file) CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    if (face) {
        IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1;
        CHECK_HR(IDWriteFontCollection_GetFontFromFontFace(c, face, &font), DWRITE_E_NOFONT);
        CHECK_OUT_NULL(font);
    }
    DW_RELEASE(face); DW_RELEASE(file); DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_face_same_bytes_other_loader_is_not_found)
{
    /* The physical key contains the file loader's id: identical bytes behind a different loader are a different font. */
    FCL_OPEN(x);
    ttf_spec s; ttf_style_spec(&s, "Twin Bytes", 400, 5, 0);
    size_t n; uint8_t *b = ttf_build(&s, &n);
    fcl_add(&x, b, n);
    dwx_mem_loader *other = dwx_loader_new();
    UINT32 ok = dwx_loader_add(other, b, n);
    free(b);
    CHECK_HR_OK(dwx_loader_register(other, x.f));
    FCL_COLL(x, c);
    IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR_OK(dwx_loader_file(other, x.f, ok, &file));
    if (file) CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    if (face) {
        IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1;
        CHECK_HR(IDWriteFontCollection_GetFontFromFontFace(c, face, &font), DWRITE_E_NOFONT);
        CHECK_OUT_NULL(font);
    }
    DW_RELEASE(face); DW_RELEASE(file); DW_RELEASE(c);
    dwx_loader_unregister(other); dwx_release(other);
    fcl_close(&x);
}
TEST(fcl_face_created_independently_from_same_reference_is_found)
{
    FCL_OPEN(x);
    UINT32 key = fcl_add_family(&x, "Independent");
    FCL_COLL(x, c);
    IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR_OK(dwx_loader_file(x.fl, x.f, key, &file));
    if (file) CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    if (face) {
        IDWriteFont *font = NULL;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(c, face, &font));
        CHECK(font != NULL);
        if (font) {
            EXPECT_EQ(IDWriteFont_GetWeight(font), 400);
            EXPECT_REFS(font, 1);
            DW_RELEASE(font);
        }
    }
    DW_RELEASE(face); DW_RELEASE(file); DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_face_simulations_come_from_the_face)
{
    FCL_OPEN(x);
    UINT32 key = fcl_add_family(&x, "SimFace");
    FCL_COLL(x, c);
    static const DWRITE_FONT_SIMULATIONS sims[] = { DWRITE_FONT_SIMULATIONS_NONE, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE };
    unsigned i;
    for (i = 0; i < 4; i++) {
        IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL; IDWriteFont *font = NULL;
        CHECK_HR_OK(dwx_loader_file(x.fl, x.f, key, &file));
        if (file) CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, sims[i], &face));
        if (face) {
            HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(c, face, &font);
            CHECKF(hr == S_OK, "simulations %u: %s", (unsigned)sims[i], dw_hr_name(hr));
            if (font) {
                EXPECT_EQ(IDWriteFont_GetSimulations(font), sims[i]);
                if (sims[i] & DWRITE_FONT_SIMULATIONS_BOLD) EXPECT_EQ(IDWriteFont_GetWeight(font), 700); else EXPECT_EQ(IDWriteFont_GetWeight(font), 400);
            }
        }
        DW_RELEASE(font); DW_RELEASE(face); DW_RELEASE(file);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_face_found_in_a_second_collection_over_the_same_files)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Shared Files");
    IDWriteFontCollection *c1 = NULL, *c2 = NULL;
    CHECK_HR_OK(fcl_create(&x, &c1));
    CHECK_HR_OK(fcl_create(&x, &c2));
    IDWriteFontFace *face = fcl_face_of(c1, 0, 0);
    CHECK(face != NULL);
    if (face && c2) {
        IDWriteFont *font = NULL;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(c2, face, &font));
        CHECK(font != NULL);
        DW_RELEASE(font);
    }
    DW_RELEASE(face); DW_RELEASE(c1); DW_RELEASE(c2);
    fcl_close(&x);
}
TEST(fcl_face_from_ttc_index)
{
    FCL_OPEN(x);
    size_t n[2], tn; uint8_t *f[2]; const uint8_t *cf[2];
    f[0] = ttf_build_styled("Ttc First", 400, 5, 0, &n[0]);
    f[1] = ttf_build_styled("Ttc Second", 400, 5, 0, &n[1]);
    cf[0] = f[0]; cf[1] = f[1];
    uint8_t *ttc = ttf_build_ttc(cf, n, 2, &tn);
    UINT32 key = fcl_add(&x, ttc, tn);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    unsigned idx;
    for (idx = 0; idx < 2; idx++) {
        IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL; IDWriteFont *font = NULL;
        CHECK_HR_OK(dwx_loader_file(x.fl, x.f, key, &file));
        if (file) CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &file, idx, DWRITE_FONT_SIMULATIONS_NONE, &face));
        if (face) {
            CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(c, face, &font));
            if (font) {
                IDWriteFontFamily *fam = NULL; char nm[64];
                if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &fam)) && fam) { fcl_fam_name_of(fam, nm, sizeof nm); EXPECT_STR(nm, idx ? "Ttc Second" : "Ttc First"); DW_RELEASE(fam); }
            }
        }
        DW_RELEASE(font); DW_RELEASE(face); DW_RELEASE(file);
    }
    DW_RELEASE(c);
    free(ttc); free(f[0]); free(f[1]);
    fcl_close(&x);
}
TEST(fcl_face_system_face_is_not_in_a_custom_collection)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Custom Only");
    FCL_COLL(x, c);
    IDWriteFontFace *face = dw_sysface(x.f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) face = dw_sysface(x.f, L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { DW_RELEASE(c); fcl_close(&x); t_skip("no Arial/Segoe UI installed"); return; }
    IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection_GetFontFromFontFace(c, face, &font), DWRITE_E_NOFONT);
    CHECK_OUT_NULL(font);
    DW_RELEASE(face); DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_face_result_is_a_new_object_each_time)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Objects");
    FCL_COLL(x, c);
    IDWriteFontFace *face = fcl_face_of(c, 0, 0);
    if (!face) { DW_RELEASE(c); fcl_close(&x); t_skip("no face"); return; }
    IDWriteFont *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(c, face, &a));
    CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(c, face, &b));
    CHECK(a != NULL && b != NULL && a != b);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(face); DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_face_font_keeps_collection_alive)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Keeper");
    FCL_COLL(x, c);
    IDWriteFontFace *face = fcl_face_of(c, 0, 0);
    IDWriteFont *font = NULL;
    if (face) CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(c, face, &font));
    DW_RELEASE(face);
    DW_RELEASE(c);
    if (font) {
        IDWriteFontFamily *fam = NULL; char nm[64];
        CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &fam));
        if (fam) { CHECK(fcl_fam_name_of(fam, nm, sizeof nm)); EXPECT_STR(nm, "Keeper"); DW_RELEASE(fam); }
        DW_RELEASE(font);
    }
    fcl_close(&x);
}

/* -------------------------------------------------------------------------------- face / file plumbing */
TEST(fcl_faces_reference_the_enumerated_files)
{
    FCL_OPEN(x);
    UINT32 k0 = fcl_add_family(&x, "Plumb One"), k1 = fcl_add_family(&x, "Plumb Two");
    FCL_COLL(x, c);
    UINT32 i;
    for (i = 0; i < 2; i++) {
        IDWriteFontFace *face = fcl_face_of(c, i, 0);
        CHECK(face != NULL);
        if (!face) continue;
        UINT8 key[32]; UINT32 ks; int match;
        ref_key_of(face, key, &ks, &match, (IDWriteFontFileLoader *)x.fl);
        CHECK(match);                                       /* the face reads through our file loader */
        EXPECT_EQ(ks, 4);
        UINT32 kv = 0xFFFFFFFF; if (ks == 4) memcpy(&kv, key, 4);
        CHECK(kv == k0 || kv == k1);
        EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
        EXPECT_EQ(IDWriteFontFace_GetIndex(face), 0);
        DW_RELEASE(face);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_faces_data_comes_from_the_streams)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Streamed");
    FCL_COLL(x, c);
    IDWriteFontFace *face = fcl_face_of(c, 0, 0);
    CHECK(face != NULL);
    if (face) {
        UINT32 len = 0; uint8_t *head = dw_face_table(face, DW_TAG('h', 'e', 'a', 'd'), &len);
        CHECK(head != NULL && len == 54);
        free(head);
        uint8_t *name = dw_face_table(face, DW_TAG('n', 'a', 'm', 'e'), &len);
        CHECK(name != NULL);
        free(name);
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
        DW_RELEASE(face);
    }
    CHECK(dwx_loader_total_reads(x.fl) > 0);
    EXPECT_EQ(x.fl->streams[0]->bad_release_ctx, 0);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_faces_font_metrics_of_the_synthetic_font)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Metrics Fam");
    FCL_COLL(x, c);
    IDWriteFont *f = fcl_pick(c, 0, 400, 5, 0, 0);
    CHECK(f != NULL);
    if (f) {
        DWRITE_FONT_METRICS m;
        IDWriteFont_GetMetrics(f, &m);
        EXPECT_EQ(m.designUnitsPerEm, 2048);
        EXPECT_EQ(m.capHeight, 1400);
        EXPECT_EQ(m.xHeight, 1000);
        BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteFont_HasCharacter(f, L'A', &ex)); EXPECT_EQ(ex, 1);
        CHECK_HR_OK(IDWriteFont_HasCharacter(f, 0x4E2D, &ex)); EXPECT_EQ(ex, 0);
        DW_RELEASE(f);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}

/* ----------------------------------------------------------------------------------------- interfaces */
TEST(fcl_iface_collection_qi_matrix)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Qi Fam");
    FCL_COLL(x, c);
    dw_qi_row rows[] = {
        DW_QI_ROW(IDWriteFontCollection, 1), DW_QI_ROW(IDWriteFontCollection1, 1), DW_QI_ROW(IDWriteFontCollection2, 1), DW_QI_ROW(IDWriteFontCollection3, 1),
        DW_QI_ROW(IDWriteFontList, 0), DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontFace, 0),
        DW_QI_ROW(IDWriteFontCollectionLoader, 0), DW_QI_ROW(IDWriteFontFileEnumerator, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0), DW_QI_ROW(IDWriteFontFileLoader, 0),
        DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteFontSet, -1), DW_QI_ROW(IDWriteFontList1, -1), DW_QI_ROW(IDWriteFontList2, -1),
    };
    DW_CHECK_QI(c, rows);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_iface_collection_refcounting)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Refs Fam");
    FCL_COLL(x, c);
    EXPECT_REFS(c, 1);
    ULONG r;
    r = IUnknown_AddRef((IUnknown *)c); EXPECT_EQ(r, 2);
    r = IUnknown_AddRef((IUnknown *)c); EXPECT_EQ(r, 3);
    r = IUnknown_Release((IUnknown *)c); EXPECT_EQ(r, 2);
    r = IUnknown_Release((IUnknown *)c); EXPECT_EQ(r, 1);
    IDWriteFontCollection1 *c1 = NULL;
    CHECK_HR_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    EXPECT_REFS(c, 2);
    DW_RELEASE(c1);
    EXPECT_REFS(c, 1);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_iface_collection1_family_and_set)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Set Fam", 400, 5, 0); fcl_add_style(&x, "Set Fam", 700, 5, 0); fcl_add_family(&x, "Set Other");
    FCL_COLL(x, c);
    IDWriteFontCollection1 *c1 = NULL;
    CHECK_HR_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    if (c1) {
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c1, &set));
        CHECK(set != NULL);
        if (set) {
            EXPECT_EQ(IDWriteFontSet_GetFontCount(set), 3);      /* real faces only: no simulations in the set */
            DW_RELEASE(set);
        }
        UINT32 n = IDWriteFontCollection1_GetFontFamilyCount(c1);
        EXPECT_EQ(n, 2);
        IDWriteFontFamily1 *f1 = (IDWriteFontFamily1 *)(ULONG_PTR)1;
        CHECK_HR(IDWriteFontCollection1_GetFontFamily(c1, n, &f1), FCL_E_FAIL);
        CHECK_OUT_NULL(f1);
        CHECK_HR_OK(IDWriteFontCollection1_GetFontFamily(c1, 0, &f1));
        DW_RELEASE(f1);
        DW_RELEASE(c1);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_iface_collection2_family_model)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Model Fam");
    FCL_COLL(x, c);
    IDWriteFontCollection2 *c2 = NULL;
    CHECK_HR_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2));
    if (c2) {
        EXPECT_EQ(IDWriteFontCollection2_GetFontFamilyModel(c2), DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
        IDWriteFontFamily2 *f2 = (IDWriteFontFamily2 *)(ULONG_PTR)1;
        CHECK_HR(IDWriteFontCollection2_GetFontFamily(c2, 5, &f2), FCL_E_FAIL);
        CHECK_OUT_NULL(f2);
        CHECK_HR_OK(IDWriteFontCollection2_GetFontFamily(c2, 0, &f2));
        DW_RELEASE(f2);
        DW_RELEASE(c2);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_iface_collection3_expiration_event)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Expiry Fam");
    FCL_COLL(x, c);
    IDWriteFontCollection3 *c3 = NULL;
    CHECK_HR_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection3, (void **)&c3));
    if (c3) {
        HANDLE ev = IDWriteFontCollection3_GetExpirationEvent(c3);
        t_note("custom collection expiration event: %p", ev);
        if (ev) { DWORD w = WaitForSingleObject(ev, 0); CHECK(w == WAIT_TIMEOUT); }
        EXPECT_EQ(IDWriteFontCollection3_GetFontFamilyModel(c3), DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
        DW_RELEASE(c3);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_iface_collection2_matching_fonts)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Match Fam", 400, 5, 0); fcl_add_style(&x, "Match Fam", 700, 5, 0);
    FCL_COLL(x, c);
    IDWriteFontCollection2 *c2 = NULL;
    CHECK_HR_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2));
    if (c2) {
        IDWriteFontList2 *list = (IDWriteFontList2 *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontCollection2_GetMatchingFonts(c2, L"Match Fam", NULL, 0, &list);
        t_note("GetMatchingFonts(\"Match Fam\") -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr) && list) { t_note("matching count %u", IDWriteFontList2_GetFontCount(list)); CHECK(IDWriteFontList2_GetFontCount(list) >= 2); DW_RELEASE(list); }
        list = (IDWriteFontList2 *)(ULONG_PTR)1;
        hr = IDWriteFontCollection2_GetMatchingFonts(c2, L"No Such Family", NULL, 0, &list);
        t_note("GetMatchingFonts(missing) -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr) && list) { EXPECT_EQ(IDWriteFontList2_GetFontCount(list), 0); DW_RELEASE(list); }
        DW_RELEASE(c2);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}

/* ------------------------------------------------------------------------------------- font set based */
static IDWriteFontSet *build_set(fcl_fx *x, IDWriteFactory3 *f3, const UINT32 *keys, int n)
{
    IDWriteFontSetBuilder *b = NULL; IDWriteFontSetBuilder1 *b1 = NULL; IDWriteFontSet *set = NULL; int i;
    if (FAILED(IDWriteFactory3_CreateFontSetBuilder(f3, &b)) || !b) return NULL;
    if (FAILED(IDWriteFontSetBuilder_QueryInterface(b, &IID_IDWriteFontSetBuilder1, (void **)&b1)) || !b1) { IDWriteFontSetBuilder_Release(b); t_note("IDWriteFontSetBuilder1 unsupported"); return NULL; }
    for (i = 0; i < n; i++) {
        IDWriteFontFile *file = NULL;
        if (SUCCEEDED(dwx_loader_file(x->fl, x->f, keys[i], &file)) && file) { IDWriteFontSetBuilder1_AddFontFile(b1, file); IDWriteFontFile_Release(file); }
    }
    IDWriteFontSetBuilder1_CreateFontSet(b1, &set);
    IDWriteFontSetBuilder1_Release(b1);
    IDWriteFontSetBuilder_Release(b);
    return set;
}
#define NEED_F3(x, f3) IDWriteFactory3 *f3 = NULL; \
    if (FAILED(IDWriteFactory_QueryInterface(x.f, &IID_IDWriteFactory3, (void **)&f3)) || !f3) { fcl_close(&x); t_skip("IDWriteFactory3 unsupported"); return; }

TEST(fcl_set_collection_from_font_set)
{
    FCL_OPEN(x); NEED_F3(x, f3);
    UINT32 k[3];
    k[0] = fcl_hidden_style(&x, "Set Coll", 400, 5, 0); k[1] = fcl_hidden_style(&x, "Set Coll", 700, 5, 0); k[2] = fcl_hidden_style(&x, "Set Other", 400, 5, 0);
    IDWriteFontSet *set = build_set(&x, f3, k, 3);
    CHECK(set != NULL);
    if (set) {
        IDWriteFontCollection1 *c = (IDWriteFontCollection1 *)(ULONG_PTR)1;
        CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, set, &c));
        CHECK(c != NULL);
        if (c) {
            EXPECT_EQ(IDWriteFontCollection1_GetFontFamilyCount(c), 2);
            CHECK(fcl_find((IDWriteFontCollection *)c, "Set Coll") >= 0);
            CHECK(fcl_find((IDWriteFontCollection *)c, "Set Other") >= 0);
            IDWriteFontCollection2 *c2 = NULL;
            if (SUCCEEDED(IDWriteFontCollection1_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2)) && c2) {
                EXPECT_EQ(IDWriteFontCollection2_GetFontFamilyModel(c2), DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
                DW_RELEASE(c2);
            }
            DW_RELEASE(c);
        }
        DW_RELEASE(set);
    }
    IDWriteFactory3_Release(f3);
    fcl_close(&x);
}
TEST(fcl_set_collection_null_set)
{
    FCL_OPEN(x); NEED_F3(x, f3);
    if (t_is_wine()) { IDWriteFactory3_Release(f3); fcl_close(&x); t_skip("Wine dereferences a NULL font set"); return; }
    IDWriteFontCollection1 *c = (IDWriteFontCollection1 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, NULL, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    IDWriteFactory3_Release(f3);
    fcl_close(&x);
}
TEST(fcl_set_empty_set_gives_empty_collection)
{
    FCL_OPEN(x); NEED_F3(x, f3);
    IDWriteFontSet *set = build_set(&x, f3, NULL, 0);
    CHECK(set != NULL);
    if (set) {
        IDWriteFontCollection1 *c = NULL;
        CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, set, &c));
        if (c) { EXPECT_EQ(IDWriteFontCollection1_GetFontFamilyCount(c), 0); DW_RELEASE(c); }
        DW_RELEASE(set);
    }
    IDWriteFactory3_Release(f3);
    fcl_close(&x);
}
TEST(fcl_set_collection_outlives_set_and_builder)
{
    FCL_OPEN(x); NEED_F3(x, f3);
    UINT32 k = fcl_hidden_style(&x, "Outlives Set", 400, 5, 0);
    IDWriteFontSet *set = build_set(&x, f3, &k, 1);
    IDWriteFontCollection1 *c = NULL;
    if (set) CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, set, &c));
    DW_RELEASE(set);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection1_GetFontFamilyCount(c), 1);
        CHECK(fcl_find((IDWriteFontCollection *)c, "Outlives Set") == 0);
        IDWriteFontFace *face = fcl_face_of((IDWriteFontCollection *)c, 0, 0);
        CHECK(face != NULL);
        DW_RELEASE(face);
        DW_RELEASE(c);
    }
    IDWriteFactory3_Release(f3);
    fcl_close(&x);
}
TEST(fcl_set_duplicate_file_is_not_deduplicated)
{
    FCL_OPEN(x); NEED_F3(x, f3);
    UINT32 k[2];
    k[0] = k[1] = fcl_hidden_style(&x, "Dup Set", 400, 5, 0);
    IDWriteFontSet *set = build_set(&x, f3, k, 2);
    CHECK(set != NULL);
    if (set) {
        EXPECT_EQ(IDWriteFontSet_GetFontCount(set), 2);
        IDWriteFontCollection1 *c = NULL;
        CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, set, &c));
        if (c) { EXPECT_EQ(IDWriteFontCollection1_GetFontFamilyCount(c), 1); DW_RELEASE(c); }
        DW_RELEASE(set);
    }
    IDWriteFactory3_Release(f3);
    fcl_close(&x);
}
TEST(fcl_set_roundtrip_collection_to_set_to_collection)
{
    FCL_OPEN(x); NEED_F3(x, f3);
    fcl_add_style(&x, "Loop Fam", 400, 5, 0); fcl_add_style(&x, "Loop Fam", 700, 5, 0); fcl_add_family(&x, "Loop Other");
    FCL_COLL(x, c);
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *set = NULL;
    CHECK_HR_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    if (c1) CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c1, &set));
    if (set) {
        IDWriteFontCollection1 *again = NULL;
        CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, set, &again));
        if (again) {
            EXPECT_EQ(IDWriteFontCollection1_GetFontFamilyCount(again), IDWriteFontCollection_GetFontFamilyCount(c));
            CHECK(fcl_find((IDWriteFontCollection *)again, "Loop Fam") >= 0);
            DW_RELEASE(again);
        }
        DW_RELEASE(set);
    }
    DW_RELEASE(c1); DW_RELEASE(c);
    IDWriteFactory3_Release(f3);
    fcl_close(&x);
}
TEST(fcl_set_typographic_model_groups_by_id16)
{
    FCL_OPEN(x);
    IDWriteFactory6 *f6 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(x.f, &IID_IDWriteFactory6, (void **)&f6)) || !f6) { fcl_close(&x); t_skip("IDWriteFactory6 unsupported"); return; }
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)f6;
    UINT32 k[2];
    k[0] = fcl_hidden_style(&x, "Typo Model", 400, 5, 0); k[1] = fcl_hidden_style(&x, "Typo Model", 300, 5, 0);
    IDWriteFontSet *set = build_set(&x, f3, k, 2);
    CHECK(set != NULL);
    if (set) {
        IDWriteFontCollection2 *typo = NULL, *wws = NULL;
        CHECK_HR_OK(IDWriteFactory6_CreateFontCollectionFromFontSet(f6, set, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &typo));
        CHECK_HR_OK(IDWriteFactory6_CreateFontCollectionFromFontSet(f6, set, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &wws));
        if (typo) {
            EXPECT_EQ(IDWriteFontCollection2_GetFontFamilyModel(typo), DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC);
            EXPECT_EQ(IDWriteFontCollection2_GetFontFamilyCount(typo), 1);          /* Regular and Light share the typographic family name */
            CHECK(fcl_find((IDWriteFontCollection *)typo, "Typo Model") == 0);
        }
        if (wws) {
            EXPECT_EQ(IDWriteFontCollection2_GetFontFamilyModel(wws), DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
            /* Win10 also yields a single family for the synthetic fonts here */
            EXPECT_EQ(IDWriteFontCollection2_GetFontFamilyCount(wws), 1);
            t_note("wws: \"Typo Model Light\" resolves to %ld", fcl_find((IDWriteFontCollection *)wws, "Typo Model Light"));
        }
        DW_RELEASE(typo); DW_RELEASE(wws); DW_RELEASE(set);
    }
    IDWriteFactory6_Release(f6);
    fcl_close(&x);
}
TEST(fcl_set_factory6_null_set_and_out_zeroed)
{
    FCL_OPEN(x);
    IDWriteFactory6 *f6 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(x.f, &IID_IDWriteFactory6, (void **)&f6)) || !f6) { fcl_close(&x); t_skip("IDWriteFactory6 unsupported"); return; }
    if (t_is_wine()) { IDWriteFactory6_Release(f6); fcl_close(&x); t_skip("Wine dereferences a NULL font set"); return; }
    IDWriteFontCollection2 *c = (IDWriteFontCollection2 *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory6_CreateFontCollectionFromFontSet(f6, NULL, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    IDWriteFactory6_Release(f6);
    fcl_close(&x);
}
TEST(fcl_set_unknown_family_model_value_note)
{
    FCL_OPEN(x);
    IDWriteFactory6 *f6 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(x.f, &IID_IDWriteFactory6, (void **)&f6)) || !f6) { fcl_close(&x); t_skip("IDWriteFactory6 unsupported"); return; }
    UINT32 k = fcl_hidden_style(&x, "Odd Model", 400, 5, 0);
    IDWriteFontSet *set = build_set(&x, (IDWriteFactory3 *)f6, &k, 1);
    if (set) {
        IDWriteFontCollection2 *c = (IDWriteFontCollection2 *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFactory6_CreateFontCollectionFromFontSet(f6, set, (DWRITE_FONT_FAMILY_MODEL)7, &c);
        t_note("family model 7 -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr) && c) { t_note("model reported %d, families %u", (int)IDWriteFontCollection2_GetFontFamilyModel(c), IDWriteFontCollection2_GetFontFamilyCount(c)); DW_RELEASE(c); }
        else CHECK_OUT_NULL(c);
        DW_RELEASE(set);
    }
    IDWriteFactory6_Release(f6);
    fcl_close(&x);
}

/* --------------------------------------------------------------------------- text layout font selection */
#define CHECK_ADV(call, want) do { float g_ = (call); CHECK_NEAR(g_, (want), 0.5); } while (0)
static uint8_t *adv_variant(const char *family, int weight, int italic, int advA, size_t *size)
{
    ttf_spec s; ttf_glyph *g; int n; const ttf_glyph *dg = ttf_default_glyphs(&n);
    ttf_style_spec(&s, family, weight, 5, italic);
    g = (ttf_glyph *)malloc(sizeof(ttf_glyph) * n);
    memcpy(g, dg, sizeof(ttf_glyph) * n);
    g[ttf_default_gid('A')].adv = (unsigned short)advA;
    s.glyphs = g; s.nglyphs = n;
    uint8_t *b = ttf_build(&s, size);
    free(g);
    return b;
}
static float run_advance_a(IDWriteFactory *f, IDWriteFontCollection *c, const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st, float em)
{
    IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *lay = NULL; float adv = -1.0f;
    HRESULT hr_ = IDWriteFactory_CreateTextFormat(f, fam, c, w, st, DWRITE_FONT_STRETCH_NORMAL, em, L"en-us", &fmt);
    if (FAILED(hr_) || !fmt) { t_note("CreateTextFormat %ls -> %s", fam, dw_hr_name(hr_)); return -1.0f; }
    if (SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"A", 1, fmt, 100000.0f, 100000.0f, &lay)) && lay) {
        dwx_renderer *rd = dwx_renderer_new();
        HRESULT dh = IDWriteTextLayout_Draw(lay, NULL, (IDWriteTextRenderer *)rd, 0, 0);
        if (FAILED(dh)) t_note("Draw %ls -> %s", fam, dw_hr_name(dh));
        if (SUCCEEDED(dh)) {
            int cur = 0; const dwx_draw *d = dwx_renderer_next(rd, DWX_DRAW_GLYPHRUN, &cur);
            if (d && d->nglyphs == 1 && d->advances) adv = d->advances[0];
            else t_note("%ls: glyph run %s n=%u", fam, d ? "found" : "missing", d ? d->nglyphs : 0);
        }
        dwx_release(rd);
        IDWriteTextLayout_Release(lay);
    }
    IDWriteTextFormat_Release(fmt);
    return adv;
}
TEST(fcl_layout_uses_the_right_face_of_the_family)
{
    FCL_OPEN(x);
    size_t n; uint8_t *b;
    b = adv_variant("Sel Fam", 400, 0, 1000, &n); fcl_add(&x, b, n); free(b);
    b = adv_variant("Sel Fam", 700, 0, 1400, &n); fcl_add(&x, b, n); free(b);
    b = adv_variant("Sel Fam", 400, 1, 1800, &n); fcl_add(&x, b, n); free(b);
    b = adv_variant("Sel Fam", 700, 1, 1900, &n); fcl_add(&x, b, n); free(b);
    FCL_COLL(x, c);
    const float em = 2048.0f;           /* em == units per em, so advance in DIPs equals design units */
    CHECK_ADV(run_advance_a(x.f, c, L"Sel Fam", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, em), 1000.0f);
    CHECK_ADV(run_advance_a(x.f, c, L"Sel Fam", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, em), 1400.0f);
    CHECK_ADV(run_advance_a(x.f, c, L"Sel Fam", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC, em), 1800.0f);
    CHECK_ADV(run_advance_a(x.f, c, L"Sel Fam", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, em), 1900.0f);
    /* nearest-weight rules: 600 prefers the heavier face, 300 has nothing lighter and takes Regular */
    CHECK_ADV(run_advance_a(x.f, c, L"Sel Fam", DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, em), 1400.0f);
    CHECK_ADV(run_advance_a(x.f, c, L"Sel Fam", DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL, em), 1000.0f);
    CHECK_ADV(run_advance_a(x.f, c, L"sel fam", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, em), 1400.0f);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_layout_second_family_is_independent)
{
    FCL_OPEN(x);
    size_t n; uint8_t *b;
    b = adv_variant("Narrow Fam", 400, 0, 600, &n); fcl_add(&x, b, n); free(b);
    b = adv_variant("Broad Fam", 400, 0, 1200, &n); fcl_add(&x, b, n); free(b);
    FCL_COLL(x, c);
    const float em = 2048.0f;
    CHECK_ADV(run_advance_a(x.f, c, L"Broad Fam", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, em), 1200.0f);
    CHECK_ADV(run_advance_a(x.f, c, L"Narrow Fam", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, em), 600.0f);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_layout_works_after_both_loaders_are_unregistered)
{
    FCL_OPEN(x);
    size_t n; uint8_t *b = adv_variant("Late Fam", 400, 0, 1234, &n); fcl_add(&x, b, n); free(b);
    FCL_COLL(x, c);
    const float em = 2048.0f;
    CHECK_ADV(run_advance_a(x.f, c, L"Late Fam", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, em), 1234.0f);
    dwx_coll_unregister(x.cl);                      /* only the collection loader: the file loader must stay for font data */
    CHECK_ADV(run_advance_a(x.f, c, L"Late Fam", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, em), 1234.0f);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_collection_listing_survives_file_loader_unregistration)
{
    /* The family/name data was captured while the collection was built; listing it does not need the file loader any more
     * (creating faces would, and is deliberately not exercised: the application is required to keep the loader registered). */
    FCL_OPEN(x);
    fcl_add_style(&x, "Orphan Fam", 400, 5, 0);
    fcl_add_family(&x, "Orphan Two");
    FCL_COLL(x, c);
    dwx_loader_unregister(x.fl);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    CHECK(fcl_find(c, "Orphan Fam") >= 0);
    CHECK(fcl_find(c, "orphan two") >= 0);
    char nm[64];
    CHECK(fcl_name_at(c, 0, nm, sizeof nm));
    DW_RELEASE(c);
    fcl_close(&x);
}

/* ---------------------------------------------------------------------------------------------- leaks */
TEST(fcl_leak_collection_lifecycle)
{
    DWX_LEAK_BEGIN(lk);
    {
        FCL_OPEN(x);
        fcl_add_style(&x, "Leak Fam", 400, 5, 0); fcl_add_style(&x, "Leak Fam", 700, 5, 0); fcl_add_family(&x, "Leak Other");
        FCL_COLL(x, c);
        UINT32 i, n = IDWriteFontCollection_GetFontFamilyCount(c);
        for (i = 0; i < n; i++) {
            IDWriteFontFamily *fam = NULL;
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) && fam) {
                UINT32 k, fc = IDWriteFontFamily_GetFontCount(fam);
                for (k = 0; k < fc; k++) {
                    IDWriteFont *f = NULL; IDWriteFontFace *face = NULL; IDWriteFont *back = NULL;
                    if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &f)) && f) {
                        if (SUCCEEDED(IDWriteFont_CreateFontFace(f, &face)) && face) { IDWriteFontCollection_GetFontFromFontFace(c, face, &back); DW_RELEASE(back); DW_RELEASE(face); }
                        DW_RELEASE(f);
                    }
                }
                DW_RELEASE(fam);
            }
        }
        DW_RELEASE(c);
        fcl_close(&x);
    }
    int live = dwx_live();
    t_note("test doubles still live after the factory and every dwrite object were released: %d (before: %d)", live, lk_live0_);
    DWX_LEAK_END(lk);
}
TEST(fcl_leak_failed_creations)
{
    DWX_LEAK_BEGIN(lk);
    {
        FCL_OPEN(x);
        fcl_add_family(&x, "Leak Fail A"); fcl_add_family(&x, "Leak Fail B");
        IDWriteFontCollection *c = NULL;
        x.cl->enum_fail_move = E_FAIL;
        CHECK_HR(fcl_create(&x, &c), E_FAIL);
        x.cl->enum_fail_move = 0; x.cl->enum_fail_get = E_FAIL;
        CHECK_HR_OK(fcl_create(&x, &c));
        DW_RELEASE(c);
        x.cl->enum_fail_get = 0; x.cl->fail_create = E_ACCESSDENIED;
        CHECK_HR(fcl_create(&x, &c), E_ACCESSDENIED);
        fcl_close(&x);
    }
    DWX_LEAK_END(lk);
}
