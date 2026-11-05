/* IDWriteFont::CreateFontFace, IDWriteFont3: CreateFontFace(IDWriteFontFace3**), Equals, GetFontFaceReference, HasCharacter (BOOL), GetLocality.
 * Decomp: DWriteFont::CreateFontFace(IDWriteFontFace**) is a thunk into the IDWriteFont3 variant (vtable slot 19), which zeroes *out, builds a
 * FontFaceElementKey from the font's FontFaceKey and calls DWriteFontFace::Create.  DWriteFont::Equals compares FontFaceKey (file reference,
 * face index, simulations) and falls back to the canonical keys; Equals(NULL) is FALSE.  GetLocality is LOCAL (2) unless the file loader
 * answers QueryInterface(IDWriteRemoteFontFileLoader) (FontFileReference::GetLocality). */
#include "fo.h"

static IDWriteFont3 *as3(IDWriteFont *f) { IDWriteFont3 *p = NULL; IDWriteFont_QueryInterface(f, &IID_IDWriteFont3, (void **)&p); return p; }
static IDWriteFontFace5 *face5(IDWriteFontFace *f) { IDWriteFontFace5 *p = NULL; if (f) IDWriteFontFace_QueryInterface(f, &IID_IDWriteFontFace5, (void **)&p); return p; }

/* ---- CreateFontFace ----------------------------------------------------------------------------------------------- */
TEST(font_createface_basic_synthetic) {
    fo_fx x; ttf_spec s; IDWriteFontFace *face = (IDWriteFontFace *)(ULONG_PTR)1; fo_spec(&s, "FoCf1"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_CreateFontFace(x.font[0], &face));
    CHECK(face != NULL && face != (void *)1);
    if (face && face != (void *)1) {
        EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(IDWriteFontFace_GetIndex(face), 0);
        EXPECT_EQ(IDWriteFontFace_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE); EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
        DW_RELEASE(face);
    }
    fo_close(&x);
}
TEST(font_createface_files_are_the_collection_file) {
    fo_fx x; ttf_spec s; IDWriteFontFace *face = NULL; IDWriteFontFile *file = NULL; UINT32 n = 0; const void *key = NULL; UINT32 ks = 0; fo_spec(&s, "FoCf2"); FO_OPEN1(x, s);
    IDWriteFont_CreateFontFace(x.font[0], &face);
    if (face) {
        CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, NULL)); EXPECT_EQ(n, 1);
        n = 1; CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &n, &file));
        if (file) {
            CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(file, &key, &ks)); EXPECT_EQ(ks, 4); if (ks == 4) EXPECT_EQ(*(const UINT32 *)key, x.keys[0]);
            { IDWriteFontFileLoader *ld = NULL; CHECK_HR_OK(IDWriteFontFile_GetLoader(file, &ld)); CHECK((void *)ld == (void *)x.fl); DW_RELEASE(ld); }
            DW_RELEASE(file);
        }
        DW_RELEASE(face);
    }
    fo_close(&x);
}
TEST(font_createface_refcount_and_independence) {
    fo_fx x; ttf_spec s; IDWriteFontFace *a = NULL, *b = NULL; fo_spec(&s, "FoCf3"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_CreateFontFace(x.font[0], &a)); CHECK_HR_OK(IDWriteFont_CreateFontFace(x.font[0], &b));
    CHECK(a && b);
    if (a && b) {
        if (a == b) { t_note("CreateFontFace twice returned the same face object (refs %lu)", dw_refcount((IUnknown *)a)); EXPECT_REFS(a, 2); }
        else { t_note("CreateFontFace twice returned distinct face objects"); EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); }
        IDWriteFontFace_Release(a); a = NULL; EXPECT_EQ(IDWriteFontFace_GetIndex(b), 0);
    }
    DW_RELEASE(a); DW_RELEASE(b); fo_close(&x);
}
TEST(font_createface_equal_faces) {
    fo_fx x; ttf_spec s; IDWriteFontFace *a = NULL, *b = NULL; IDWriteFontFace5 *a5 = NULL; fo_spec(&s, "FoCf4"); FO_OPEN1(x, s);
    IDWriteFont_CreateFontFace(x.font[0], &a); IDWriteFont_CreateFontFace(x.font[0], &b); a5 = face5(a);
    if (a5 && b) { CHECK(IDWriteFontFace5_Equals(a5, b)); CHECK(IDWriteFontFace5_Equals(a5, a)); CHECK(!IDWriteFontFace5_Equals(a5, NULL)); }
    else t_skip("IDWriteFontFace5 unavailable");
    DW_RELEASE(a5); DW_RELEASE(a); DW_RELEASE(b); fo_close(&x);
}
TEST(font_createface_different_fonts_not_equal) {
    ttf_spec s[2]; fo_fx x; IDWriteFontFace *a = NULL, *b = NULL; IDWriteFontFace5 *a5; ttf_style_spec(&s[0], "FoCf5", 400, 5, 0); ttf_style_spec(&s[1], "FoCf5", 700, 5, 0); FO_OPEN(x, s, 2);
    IDWriteFont_CreateFontFace(x.font[0], &a); IDWriteFont_CreateFontFace(x.font[1], &b); a5 = face5(a);
    if (a5 && b) { CHECK(!IDWriteFontFace5_Equals(a5, b)); } else t_skip("IDWriteFontFace5 unavailable");
    DW_RELEASE(a5); DW_RELEASE(a); DW_RELEASE(b); fo_close(&x);
}
TEST(font_createface_face3_variant) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFace3 *face = (IDWriteFontFace3 *)(ULONG_PTR)1; IDWriteFontFace *plain = NULL; fo_spec(&s, "FoCf6"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); CHECK(f3 != NULL);
    if (f3) {
        CHECK_HR_OK(FO_F3_CREATEFACE(f3, &face)); CHECK(face != NULL && face != (void *)1);
        if (face && face != (void *)1) {
            IDWriteFont_CreateFontFace(x.font[0], &plain);
            if (plain) { CHECK(dw_same_identity((IUnknown *)plain, (IUnknown *)face)); }
            EXPECT_EQ(IDWriteFontFace3_GetIndex(face), 0);
            DW_RELEASE(face);
        }
        DW_RELEASE(plain); DW_RELEASE(f3);
    }
    fo_close(&x);
}
TEST(font_createface_face_interface_set) {
    fo_fx x; ttf_spec s; IDWriteFontFace *face = NULL; static const dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontFace, 1), DW_QI_ROW(IDWriteFontFace1, 1), DW_QI_ROW(IDWriteFontFace2, 1),
        DW_QI_ROW(IDWriteFontFace3, 1), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFontCollection, 0) };
    fo_spec(&s, "FoCf7"); FO_OPEN1(x, s);
    IDWriteFont_CreateFontFace(x.font[0], &face);
    if (face) DW_CHECK_QI(face, rows);
    DW_RELEASE(face); fo_close(&x);
}
TEST(font_createface_attributes_match_font) {
    ttf_spec s[3]; fo_fx x; int i; ttf_style_spec(&s[0], "FoCf8", 700, 5, 1); ttf_style_spec(&s[1], "FoCf8L", 300, 3, 0); ttf_style_spec(&s[2], "FoCf8R", 400, 5, 0);
    FO_OPEN(x, s, 3);
    for (i = 0; i < 3; i++) {
        IDWriteFontFace *face = NULL; IDWriteFontFace3 *f3 = NULL; IDWriteFont_CreateFontFace(x.font[i], &face);
        if (face) IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3);
        if (f3) {
            EXPECT_EQ(IDWriteFontFace3_GetWeight(f3), IDWriteFont_GetWeight(x.font[i])); EXPECT_EQ(IDWriteFontFace3_GetStretch(f3), IDWriteFont_GetStretch(x.font[i]));
            EXPECT_EQ(IDWriteFontFace3_GetStyle(f3), IDWriteFont_GetStyle(x.font[i])); EXPECT_EQ(IDWriteFontFace3_IsSymbolFont(f3), IDWriteFont_IsSymbolFont(x.font[i]));
            EXPECT_EQ(IDWriteFontFace3_GetSimulations(f3), IDWriteFont_GetSimulations(x.font[i]));
        } else CHECK(0);
        DW_RELEASE(f3); DW_RELEASE(face);
    }
    fo_close(&x);
}
TEST(font_createface_names_match_font) {
    ttf_spec s[2]; fo_fx x; int i; ttf_style_spec(&s[0], "FoCf9", 700, 5, 1); fo_spec(&s[1], "FoCf9B");
    FO_OPEN(x, s, 2);
    for (i = 0; i < 2; i++) {
        IDWriteFontFace *face = NULL; IDWriteFontFace3 *f3 = NULL; IDWriteLocalizedStrings *a = NULL, *b = NULL; char sa[64], sb[64];
        IDWriteFont_CreateFontFace(x.font[i], &face); if (face) IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3);
        if (f3) {
            IDWriteFont_GetFaceNames(x.font[i], &a); IDWriteFontFace3_GetFaceNames(f3, &b);
            if (a && b) { dw_ls_en(a, sa, sizeof sa); dw_ls_en(b, sb, sizeof sb); EXPECT_STR(sa, sb); }
            DW_RELEASE(a); DW_RELEASE(b);
            { BOOL e1, e2; IDWriteFont_GetInformationalStrings(x.font[i], DWRITE_INFORMATIONAL_STRING_FULL_NAME, &a, &e1); IDWriteFontFace3_GetInformationalStrings(f3, DWRITE_INFORMATIONAL_STRING_FULL_NAME, &b, &e2);
              EXPECT_EQ(e1, e2); if (a && b) { dw_ls_en(a, sa, sizeof sa); dw_ls_en(b, sb, sizeof sb); EXPECT_STR(sa, sb); } DW_RELEASE(a); DW_RELEASE(b); }
        }
        DW_RELEASE(f3); DW_RELEASE(face);
    }
    fo_close(&x);
}
TEST(font_createface_has_character_matches_font) {
    fo_fx x; ttf_spec s; IDWriteFontFace *face = NULL; IDWriteFontFace3 *f3 = NULL; UINT32 cp; fo_spec(&s, "FoCf10"); FO_OPEN1(x, s);
    IDWriteFont_CreateFontFace(x.font[0], &face); if (face) IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3);
    if (f3) for (cp = 0; cp < 0x200; cp++) { BOOL e = 5; IDWriteFont_HasCharacter(x.font[0], cp, &e); CHECKF(IDWriteFontFace3_HasCharacter(f3, cp) == e, "U+%04X", cp); }
    DW_RELEASE(f3); DW_RELEASE(face); fo_close(&x);
}
TEST(font_createface_system_arial) {
    FO_SYS_REG(f, L"Arial"); IDWriteFontFace *face = NULL; IDWriteFontFile *file = NULL; UINT32 n = 0;
    CHECK_HR_OK(IDWriteFont_CreateFontFace(f, &face));
    if (face) {
        EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(IDWriteFontFace_GetIndex(face), 0);
        IDWriteFontFace_GetFiles(face, &n, NULL); EXPECT_EQ(n, 1);
        n = 1; IDWriteFontFace_GetFiles(face, &n, &file);
        if (file) {
            IDWriteFontFileLoader *ld = NULL; IDWriteLocalFontFileLoader *lo = NULL; const void *k = NULL; UINT32 ks = 0;
            IDWriteFontFile_GetReferenceKey(file, &k, &ks); IDWriteFontFile_GetLoader(file, &ld);
            if (ld && SUCCEEDED(IDWriteFontFileLoader_QueryInterface(ld, &IID_IDWriteLocalFontFileLoader, (void **)&lo)) && lo) {
                UINT32 len = 0; WCHAR path[MAX_PATH] = L"";
                if (SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(lo, k, ks, &len)) && len < MAX_PATH && SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathFromKey(lo, k, ks, path, len + 1))) {
                    CHECKF(wcsstr(_wcslwr(path), L"arial.ttf") != NULL, "Arial regular file: %ls", path);
                }
            } else CHECK(0);
            DW_RELEASE(lo); DW_RELEASE(ld); DW_RELEASE(file);
        }
        DW_RELEASE(face);
    }
    DW_RELEASE(f);
}
TEST(font_createface_simulated_carries_simulations) {
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL; IDWriteFont *b = NULL; IDWriteFontFace *face = NULL, *base = NULL; IDWriteFontFace5 *b5 = NULL; fo_spec(&s, "FoCfSim"); FO_OPEN1(x, s);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &b);
    if (b) {
        IDWriteFont_CreateFontFace(b, &face); IDWriteFont_CreateFontFace(x.font[0], &base);
        if (face && base) {
            EXPECT_EQ(IDWriteFontFace_GetSimulations(face), DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE); EXPECT_EQ(IDWriteFontFace_GetSimulations(base), 0);
            b5 = face5(face); if (b5) CHECK(!IDWriteFontFace5_Equals(b5, base));
        }
    } else CHECK(0);
    DW_RELEASE(b5); DW_RELEASE(face); DW_RELEASE(base); DW_RELEASE(b); DW_RELEASE(fam); fo_close(&x);
}

/* ---- IDWriteFont3::Equals ------------------------------------------------------------------------------------------- */
TEST(font3_equals_self) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; fo_spec(&s, "FoEq1"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); if (f3) { CHECK(IDWriteFont3_Equals(f3, x.font[0]) == TRUE); DW_RELEASE(f3); } else CHECK(0);
    fo_close(&x);
}
TEST(font3_equals_null_is_false) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; fo_spec(&s, "FoEq2"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); if (f3) { BOOL r = IDWriteFont3_Equals(f3, NULL); EXPECT_EQ(r, FALSE); DW_RELEASE(f3); } else CHECK(0);
    fo_close(&x);
}
TEST(font3_equals_same_face_from_two_lookups) {
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL; IDWriteFont *a = NULL, *b = NULL, *c = NULL; IDWriteFont3 *f3 = NULL; fo_spec(&s, "FoEq3"); FO_OPEN1(x, s);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        IDWriteFontFamily_GetFont(fam, 0, &a); IDWriteFontFamily_GetFont(fam, 0, &b);
        IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &c);
        f3 = as3(a);
        if (f3 && b && c) { CHECK(IDWriteFont3_Equals(f3, b)); CHECK(IDWriteFont3_Equals(f3, c)); CHECK(IDWriteFont3_Equals(f3, x.font[0])); }
        else CHECK(0);
    }
    DW_RELEASE(f3); DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c); DW_RELEASE(fam); fo_close(&x);
}
TEST(font3_equals_symmetric_and_distinct) {
    ttf_spec s[3]; fo_fx x; IDWriteFont3 *a, *b; int i, j;
    ttf_style_spec(&s[0], "FoEq4", 400, 5, 0); ttf_style_spec(&s[1], "FoEq4", 700, 5, 0); ttf_style_spec(&s[2], "FoEq4", 400, 5, 1); FO_OPEN(x, s, 3);
    for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) {
        a = as3(x.font[i]); b = as3(x.font[j]);
        if (a && b) CHECKF((IDWriteFont3_Equals(a, x.font[j]) == TRUE) == (i == j) && (IDWriteFont3_Equals(b, x.font[i]) == TRUE) == (i == j), "fonts %d,%d", i, j);
        DW_RELEASE(a); DW_RELEASE(b);
    }
    fo_close(&x);
}
TEST(font3_equals_simulated_vs_base) {
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL; IDWriteFont *bo = NULL, *b = NULL; IDWriteFont3 *f3 = NULL, *g3 = NULL; fo_spec(&s, "FoEq5"); FO_OPEN1(x, s);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &b);
        IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &bo);
        if (b && bo) { f3 = as3(b); g3 = as3(bo); CHECK(!IDWriteFont3_Equals(f3, bo)); CHECK(!IDWriteFont3_Equals(g3, b)); CHECK(!IDWriteFont3_Equals(f3, x.font[0])); CHECK(IDWriteFont3_Equals(g3, bo)); }
        else CHECK(0);
    }
    DW_RELEASE(f3); DW_RELEASE(g3); DW_RELEASE(b); DW_RELEASE(bo); DW_RELEASE(fam); fo_close(&x);
}
TEST(font3_equals_same_bytes_two_keys_note) {
    /* identical font data registered under two different file keys: keys differ, canonical keys may or may not unify them */
    fo_fx x; ttf_spec s; size_t n; uint8_t *b; UINT32 k1, k2; IDWriteFontFile *f1 = NULL, *f2 = NULL; IDWriteFontFace *a = NULL, *c = NULL; IDWriteFontFace5 *a5;
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dwx_mem_loader *ld = dwx_loader_new(); fo_spec(&s, "FoEq6"); b = ttf_build(&s, &n); (void)x;
    k1 = dwx_loader_add(ld, b, n); k2 = dwx_loader_add(ld, b, n); free(b);
    if (f && SUCCEEDED(dwx_loader_register(ld, f)) && SUCCEEDED(dwx_loader_file(ld, f, k1, &f1)) && SUCCEEDED(dwx_loader_file(ld, f, k2, &f2))) {
        IDWriteFontFile *a1[1], *a2[1]; a1[0] = f1; a2[0] = f2;
        IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, a1, 0, 0, &a); IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, a2, 0, 0, &c);
        a5 = face5(a); if (a5 && c) t_note("same bytes under two keys: IDWriteFontFace5::Equals = %d", IDWriteFontFace5_Equals(a5, c));
        DW_RELEASE(a5);
    }
    DW_RELEASE(a); DW_RELEASE(c); DW_RELEASE(f1); DW_RELEASE(f2); dwx_loader_unregister(ld); dwx_release(ld); DW_RELEASE(f);
}
TEST(font3_equals_other_factory_note) {
    fo_fx x, y; ttf_spec s; IDWriteFont3 *f3; fo_spec(&s, "FoEq7"); FO_OPEN1(x, s);
    if (!fo_open1(&y, &s)) { fo_close(&y); fo_close(&x); t_skip("second fixture unavailable"); return; }
    f3 = as3(x.font[0]);
    if (f3) { t_note("same font data from two private factories: Equals = %d", IDWriteFont3_Equals(f3, y.font[0])); DW_RELEASE(f3); }
    fo_close(&y); fo_close(&x);
}
TEST(font3_equals_system_fonts) {
    FO_SYS_REG(a, L"Arial"); FO_SYS(b, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL); IDWriteFont *a2 = fo_sysfont(dw_shared(), L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFont3 *a3 = as3(a);
    if (a3) { CHECK(IDWriteFont3_Equals(a3, a2)); CHECK(!IDWriteFont3_Equals(a3, b)); }
    DW_RELEASE(a3); DW_RELEASE(a2); DW_RELEASE(a); DW_RELEASE(b);
}
TEST(font3_equals_system_across_factories_note) {
    FO_SYS_REG(a, L"Arial"); IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFont *b; IDWriteFont3 *a3 = as3(a);
    if (!iso) { DW_RELEASE(a3); DW_RELEASE(a); t_skip("no isolated factory"); return; }
    b = fo_sysfont(iso, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (b && a3) t_note("Arial regular from shared vs isolated factory: Equals = %d", IDWriteFont3_Equals(a3, b));
    DW_RELEASE(b); DW_RELEASE(a3); DW_RELEASE(a); DW_RELEASE(iso);
}
TEST(font3_equals_ttc_faces_differ) {
    /* two faces of one .ttc share the file but differ in face index */
    ttf_spec sa, sb; size_t na, nb, nt; uint8_t *ba, *bb, *tt; const uint8_t *bl[2]; size_t sz[2]; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    dwx_mem_loader *ld = dwx_loader_new(); dwx_coll_loader *cl; IDWriteFontCollection *coll = NULL; UINT32 key, ia = 9, ib = 9; BOOL ea = 0, eb = 0;
    fo_spec(&sa, "FoTtcA"); fo_spec(&sb, "FoTtcB"); ba = ttf_build(&sa, &na); bb = ttf_build(&sb, &nb); bl[0] = ba; bl[1] = bb; sz[0] = na; sz[1] = nb; tt = ttf_build_ttc(bl, sz, 2, &nt);
    key = dwx_loader_add(ld, tt, nt); free(ba); free(bb); free(tt);
    cl = dwx_coll_new(ld); dwx_coll_add_key(cl, key);
    if (f && SUCCEEDED(dwx_loader_register(ld, f)) && SUCCEEDED(dwx_coll_register(cl, f)) && SUCCEEDED(dwx_coll_create(cl, f, &coll)) && coll) {
        IDWriteFontFamily *fa = NULL, *fb = NULL; IDWriteFont *a = NULL, *b = NULL;
        IDWriteFontCollection_FindFamilyName(coll, L"FoTtcA", &ia, &ea); IDWriteFontCollection_FindFamilyName(coll, L"FoTtcB", &ib, &eb);
        CHECK(ea && eb);
        if (ea && eb) {
            IDWriteFontCollection_GetFontFamily(coll, ia, &fa); IDWriteFontCollection_GetFontFamily(coll, ib, &fb);
            if (fa && fb) { IDWriteFontFamily_GetFont(fa, 0, &a); IDWriteFontFamily_GetFont(fb, 0, &b); }
            if (a && b) {
                IDWriteFont3 *a3 = as3(a), *b3 = as3(b); IDWriteFontFaceReference *ra = NULL, *rb = NULL;
                CHECK(!IDWriteFont3_Equals(a3, b)); CHECK(IDWriteFont3_Equals(a3, a));
                CHECK_HR_OK(IDWriteFont3_GetFontFaceReference(a3, &ra)); CHECK_HR_OK(IDWriteFont3_GetFontFaceReference(b3, &rb));
                if (ra && rb) {
                    UINT32 i1 = IDWriteFontFaceReference_GetFontFaceIndex(ra), i2 = IDWriteFontFaceReference_GetFontFaceIndex(rb);
                    CHECK((i1 == 0 && i2 == 1) || (i1 == 1 && i2 == 0)); CHECK(!IDWriteFontFaceReference_Equals(ra, rb));
                }
                DW_RELEASE(ra); DW_RELEASE(rb); DW_RELEASE(a3); DW_RELEASE(b3);
            } else CHECK(0);
        }
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(fa); DW_RELEASE(fb);
    } else t_note("ttc collection unavailable");
    DW_RELEASE(coll); dwx_coll_unregister(cl); dwx_loader_unregister(ld); dwx_release(cl); dwx_release(ld); DW_RELEASE(f);
}

/* ---- GetFontFaceReference --------------------------------------------------------------------------------------------- */
TEST(font3_face_reference_basic) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)1; fo_spec(&s, "FoRef1"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]);
    if (f3) {
        CHECK_HR_OK(IDWriteFont3_GetFontFaceReference(f3, &r)); CHECK(r != NULL && r != (void *)1);
        if (r && r != (void *)1) {
            EXPECT_REFS(r, 1); EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 0); EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), DWRITE_FONT_SIMULATIONS_NONE);
            EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_LOCAL);
            DW_RELEASE(r);
        }
        DW_RELEASE(f3);
    }
    fo_close(&x);
}
TEST(font3_face_reference_new_object_each_call) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *a = NULL, *b = NULL; fo_spec(&s, "FoRef2"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]);
    if (f3) {
        IDWriteFont3_GetFontFaceReference(f3, &a); IDWriteFont3_GetFontFaceReference(f3, &b);
        CHECK(a && b && a != b);
        if (a && b) { EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); CHECK(IDWriteFontFaceReference_Equals(a, b)); CHECK(IDWriteFontFaceReference_Equals(b, a)); CHECK(IDWriteFontFaceReference_Equals(a, a)); if (!t_is_wine()) CHECK(!IDWriteFontFaceReference_Equals(a, NULL)); }
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f3);
    }
    fo_close(&x);
}
TEST(font3_face_reference_interface_set) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL; static const dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontFaceReference, 1), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontFile, 0) };
    fo_spec(&s, "FoRef3"); FO_OPEN1(x, s); f3 = as3(x.font[0]);
    if (f3) { IDWriteFont3_GetFontFaceReference(f3, &r); if (r) DW_CHECK_QI(r, rows); DW_RELEASE(r); DW_RELEASE(f3); }
    fo_close(&x);
}
TEST(font3_face_reference_file_and_key) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL; IDWriteFontFile *file = NULL; const void *k = NULL; UINT32 ks = 0; fo_spec(&s, "FoRef4"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); IDWriteFont3_GetFontFaceReference(f3, &r);
    if (r) {
        CHECK_HR_OK(IDWriteFontFaceReference_GetFontFile(r, &file)); CHECK(file != NULL);
        if (file) { CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(file, &k, &ks)); EXPECT_EQ(ks, 4); if (ks == 4) EXPECT_EQ(*(const UINT32 *)k, x.keys[0]); DW_RELEASE(file); }
    }
    DW_RELEASE(r); DW_RELEASE(f3); fo_close(&x);
}
TEST(font3_face_reference_creates_equal_face) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL; IDWriteFontFace3 *face = NULL; IDWriteFontFace *plain = NULL; IDWriteFontFace5 *p5; fo_spec(&s, "FoRef5"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); IDWriteFont3_GetFontFaceReference(f3, &r);
    if (r) {
        CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFace(r, &face));
        IDWriteFont_CreateFontFace(x.font[0], &plain); p5 = face5(plain);
        if (face && p5) { CHECK(IDWriteFontFace5_Equals(p5, (IDWriteFontFace *)face)); EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), 0); }
        DW_RELEASE(p5); DW_RELEASE(plain); DW_RELEASE(face);
    }
    DW_RELEASE(r); DW_RELEASE(f3); fo_close(&x);
}
TEST(font3_face_reference_with_simulations) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL; IDWriteFontFace3 *face = NULL; fo_spec(&s, "FoRef6"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); IDWriteFont3_GetFontFaceReference(f3, &r);
    if (r) {
        CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFaceWithSimulations(r, DWRITE_FONT_SIMULATIONS_BOLD, &face));
        if (face) { EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), DWRITE_FONT_SIMULATIONS_BOLD); DW_RELEASE(face); }
        CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFaceWithSimulations(r, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE, &face));
        if (face) { EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), 3); DW_RELEASE(face); }
    }
    DW_RELEASE(r); DW_RELEASE(f3); fo_close(&x);
}
TEST(font3_face_reference_of_simulated_font) {
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL; IDWriteFont *b = NULL; IDWriteFont3 *b3 = NULL, *f3 = NULL; IDWriteFontFaceReference *rb = NULL, *r0 = NULL; fo_spec(&s, "FoRef7"); FO_OPEN1(x, s);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &b);
    if (b) {
        b3 = as3(b); f3 = as3(x.font[0]); IDWriteFont3_GetFontFaceReference(b3, &rb); IDWriteFont3_GetFontFaceReference(f3, &r0);
        if (rb && r0) { EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(rb), 3); EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r0), 0); CHECK(!IDWriteFontFaceReference_Equals(rb, r0)); }
    } else CHECK(0);
    DW_RELEASE(rb); DW_RELEASE(r0); DW_RELEASE(b3); DW_RELEASE(f3); DW_RELEASE(b); DW_RELEASE(fam); fo_close(&x);
}
TEST(font3_face_reference_outlives_font) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL; fo_spec(&s, "FoRef8"); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); IDWriteFont3_GetFontFaceReference(f3, &r); DW_RELEASE(f3);
    DW_RELEASE(x.font[0]);
    if (r) { EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 0); EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_LOCAL); DW_RELEASE(r); }
    fo_close(&x);
}
TEST(font3_face_reference_file_size_and_time) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL; size_t n; uint8_t *b; fo_spec(&s, "FoRef9"); b = ttf_build(&s, &n); FO_OPEN1(x, s);
    f3 = as3(x.font[0]); IDWriteFont3_GetFontFaceReference(f3, &r);
    if (r) {
        FILETIME ft; HRESULT hr = IDWriteFontFaceReference_GetFileTime(r, &ft);
        EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r), n);
        t_note("GetLocalFileSize = %llu (file %zu bytes), GetFileTime hr=%s", (unsigned long long)IDWriteFontFaceReference_GetLocalFileSize(r), n, dw_hr_name(hr));
        if (SUCCEEDED(hr)) { ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime; EXPECT_EQ(u.QuadPart, x.fl->last_write); }
    }
    free(b); DW_RELEASE(r); DW_RELEASE(f3); fo_close(&x);
}
TEST(font3_face_reference_system_arial) {
    FO_SYS_REG(f, L"Arial"); IDWriteFont3 *f3 = as3(f); IDWriteFontFaceReference *r = NULL;
    if (f3) IDWriteFont3_GetFontFaceReference(f3, &r);
    if (r) {
        EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 0); EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), 0); EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_LOCAL);
        CHECK(IDWriteFontFaceReference_GetFileSize(r) > 100000); EXPECT_EQ(IDWriteFontFaceReference_GetFileSize(r), IDWriteFontFaceReference_GetLocalFileSize(r));
        { FILETIME ft; CHECK_HR_OK(IDWriteFontFaceReference_GetFileTime(r, &ft)); CHECK(ft.dwHighDateTime != 0); }
    } else CHECK(0);
    DW_RELEASE(r); DW_RELEASE(f3); DW_RELEASE(f);
}

/* ---- HasCharacter (BOOL variant) / GetLocality ------------------------------------------------------------------------------ */
TEST(font3_has_character_bool_variant) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; UINT32 cp; fo_spec(&s, "FoH3"); FO_OPEN1(x, s); f3 = as3(x.font[0]);
    if (f3) {
        for (cp = 0; cp < 0x80; cp++) { BOOL b = FO_F3_HASCHAR(f3, cp); CHECKF(b == (ttf_default_gid(cp) != 0), "U+%04X: %d", cp, b); }
        CHECK(FO_F3_HASCHAR(f3, 'A') == TRUE); CHECK(FO_F3_HASCHAR(f3, 0x4E00) == FALSE); CHECK(FO_F3_HASCHAR(f3, 0x110000) == FALSE); CHECK(FO_F3_HASCHAR(f3, 0xFFFFFFFFu) == FALSE); CHECK(FO_F3_HASCHAR(f3, 0xD800) == FALSE);
        DW_RELEASE(f3);
    }
    fo_close(&x);
}
TEST(font3_has_character_variants_agree_system) {
    FO_SYS_REG(f, L"Arial"); IDWriteFont3 *f3 = as3(f); UINT32 cp;
    if (f3) { for (cp = 0; cp < 0x3000; cp += 7) { BOOL e = 5; IDWriteFont_HasCharacter(f, cp, &e); CHECKF(FO_F3_HASCHAR(f3, cp) == e, "U+%04X", cp); } DW_RELEASE(f3); }
    DW_RELEASE(f);
}
TEST(font3_locality_synthetic_is_local) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; LONG q0; fo_spec(&s, "FoLoc1"); FO_OPEN1(x, s); f3 = as3(x.font[0]);
    if (f3) {
        q0 = dwx_qi_calls(x.fl);
        EXPECT_EQ(IDWriteFont3_GetLocality(f3), DWRITE_LOCALITY_LOCAL);
        t_note("loader QueryInterface calls made by GetLocality: %ld", (long)(dwx_qi_calls(x.fl) - q0));
        DW_RELEASE(f3);
    }
    fo_close(&x);
}
TEST(font3_locality_enum_values) { CHECK(DWRITE_LOCALITY_REMOTE == 0 && DWRITE_LOCALITY_PARTIAL == 1 && DWRITE_LOCALITY_LOCAL == 2); }
TEST(font3_locality_system_fonts_local) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Segoe UI", L"Courier New", L"Segoe UI Emoji" };
    int k, seen = 0;
    for (k = 0; k < 5; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFont3 *f3;
        if (!f) continue;
        f3 = as3(f); seen++; if (f3) { CHECKF(IDWriteFont3_GetLocality(f3) == DWRITE_LOCALITY_LOCAL, "%ls", fams[k]); DW_RELEASE(f3); }
        DW_RELEASE(f);
    }
    if (!seen) t_skip("no system font installed");
}
TEST(font3_locality_matches_reference) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL; fo_spec(&s, "FoLoc2"); FO_OPEN1(x, s); f3 = as3(x.font[0]);
    if (f3) { IDWriteFont3_GetFontFaceReference(f3, &r); if (r) EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), IDWriteFont3_GetLocality(f3)); DW_RELEASE(r); DW_RELEASE(f3); }
    fo_close(&x);
}
TEST(font3_locality_remote_loader_is_consulted) {
    /* a file loader answering QueryInterface(IDWriteRemoteFontFileLoader) would be asked via GetLocalityFromKey; our plain loader is not one */
    fo_fx x; ttf_spec s; IDWriteFont3 *f3; void *p = (void *)1; fo_spec(&s, "FoLoc3"); FO_OPEN1(x, s); f3 = as3(x.font[0]);
    CHECK_HR(IDWriteFontFileLoader_QueryInterface((IDWriteFontFileLoader *)x.fl, &IID_IDWriteRemoteFontFileLoader, &p), E_NOINTERFACE);
    if (f3) { EXPECT_EQ(IDWriteFont3_GetLocality(f3), DWRITE_LOCALITY_LOCAL); DW_RELEASE(f3); }
    fo_close(&x);
}
TEST(font3_calls_through_older_pointers_work) {
    /* one vtable: an IDWriteFont3 call made through a pointer obtained for IDWriteFont2 or IDWriteFont1 behaves identically */
    fo_fx x; ttf_spec s; IDWriteFont2 *f2 = NULL; IDWriteFont3 *f3 = NULL; fo_spec(&s, "FoVt"); FO_OPEN1(x, s);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont2, (void **)&f2);
    if (f2) { f3 = (IDWriteFont3 *)f2; CHECK(IDWriteFont3_Equals(f3, x.font[0])); EXPECT_EQ(IDWriteFont3_GetLocality(f3), DWRITE_LOCALITY_LOCAL); DW_RELEASE(f2); }
    fo_close(&x);
}
