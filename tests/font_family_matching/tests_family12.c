/* IDWriteFontFamily1 / IDWriteFontFamily2 / IDWriteFontList1 / IDWriteFontList2 / IDWriteFont3 / IDWriteFontFaceReference as reached from a family. */
#include "fm.h"
#include "fm_tables.h"

static IDWriteFontFamily1 *as_f1(fm *m) { IDWriteFontFamily1 *p = NULL; IDWriteFontFamily_QueryInterface(m->fam, &IID_IDWriteFontFamily1, (void **)&p); return p; }
static IDWriteFontFamily2 *as_f2(fm *m) { IDWriteFontFamily2 *p = NULL; IDWriteFontFamily_QueryInterface(m->fam, &IID_IDWriteFontFamily2, (void **)&p); return p; }

#define NEED_F1(m, p) IDWriteFontFamily1 *p = as_f1(&m); if (!p) { fm_close(&m); t_skip("IDWriteFontFamily1 unsupported"); return; }
#define NEED_F2(m, p) IDWriteFontFamily2 *p = as_f2(&m); if (!p) { fm_close(&m); t_skip("IDWriteFontFamily2 unsupported"); return; }

/* ---- IDWriteFontFamily1::GetFont -> IDWriteFont3 ---------------------------------------------------- */
TEST(f12_getfont_font3_matches_font) {
    FM_OPEN(m, "FmF3a", F_RIBBI);
    NEED_F1(m, f1);
    UINT32 i;
    for (i = 0; i < m.count; i++) {
        IDWriteFont3 *f3 = NULL; IDWriteFont *f = NULL; fm_face a, b; int sa, sb;
        CHECK_HR_OK(IDWriteFontFamily1_GetFont(f1, i, &f3));
        CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, i, &f));
        CHECK(f3 != NULL);
        if (f3 && f) {
            fm_read_font((IDWriteFont *)f3, &a, &sa); fm_read_font(f, &b, &sb);
            CHECKF(a.w == b.w && a.wd == b.wd && a.st == b.st && sa == sb, "index %u: font3 and font differ", i);
            CHECK(IDWriteFont3_Equals(f3, f));
        }
        DW_RELEASE(f3); DW_RELEASE(f);
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_getfont_font3_out_of_range) {
    FM_OPEN(m, "FmF3b", F_RIBBI);
    NEED_F1(m, f1);
    const UINT32 bad[] = { m.count, m.count + 1, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFFu };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteFont3 *f3 = (IDWriteFont3 *)(ULONG_PTR)&i;
        CHECK_HR(IDWriteFontFamily1_GetFont(f1, bad[i], &f3), E_FAIL);
        CHECKF(f3 == NULL, "GetFont(%u) left %p", bad[i], (void *)f3);

    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_font3_family_is_the_family) {
    FM_OPEN(m, "FmF3c", F_RIBBI);
    NEED_F1(m, f1);
    IDWriteFont3 *f3 = NULL; IDWriteFontFamily *ff = NULL;
    CHECK_HR_OK(IDWriteFontFamily1_GetFont(f1, 0, &f3));
    if (f3) {
        CHECK_HR_OK(IDWriteFont3_GetFontFamily(f3, &ff));
        if (ff) CHECK(dw_same_identity((IUnknown *)ff, (IUnknown *)m.fam));
        DW_RELEASE(ff); IDWriteFont3_Release(f3);
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_font3_locality_local) {
    FM_OPEN(m, "FmF3d", F_RIBBI);
    NEED_F1(m, f1);
    UINT32 i;
    for (i = 0; i < m.count; i++) {
        IDWriteFont3 *f3 = NULL;
        if (SUCCEEDED(IDWriteFontFamily1_GetFont(f1, i, &f3)) && f3) {
            /* FontFileReference::GetLocality: loaders that do not implement IDWriteRemoteFontFileLoader are LOCAL (2) */
            EXPECT_EQ(IDWriteFont3_GetLocality(f3), DWRITE_LOCALITY_LOCAL);
        } else CHECK(0);
        DW_RELEASE(f3);
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_font3_has_character) {
    FM_OPEN(m, "FmF3e", F_ONE);
    NEED_F1(m, f1);
    IDWriteFont3 *f3 = NULL;
    CHECK_HR_OK(IDWriteFontFamily1_GetFont(f1, 0, &f3));
    if (f3) {
        /* default synthetic cmap: U+0020, A-Z, a-z, 0-9 */
        CHECK(IDWriteFont3_HasCharacter(f3, 'A'));
        CHECK(IDWriteFont3_HasCharacter(f3, 'z'));
        CHECK(IDWriteFont3_HasCharacter(f3, '5'));
        CHECK(IDWriteFont3_HasCharacter(f3, ' '));
        CHECK(!IDWriteFont3_HasCharacter(f3, '!'));
        CHECK(!IDWriteFont3_HasCharacter(f3, 0x4E00));
        CHECK(!IDWriteFont3_HasCharacter(f3, 0x10FFFF));
        IDWriteFont3_Release(f3);
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_font3_equals) {
    FM_OPEN(m, "FmF3f", F_RIBBI);
    NEED_F1(m, f1);
    IDWriteFont3 *a = NULL, *b = NULL, *c = NULL;
    CHECK_HR_OK(IDWriteFontFamily1_GetFont(f1, 0, &a));
    CHECK_HR_OK(IDWriteFontFamily1_GetFont(f1, 0, &b));
    CHECK_HR_OK(IDWriteFontFamily1_GetFont(f1, 1, &c));
    if (a && b && c) {
        CHECK(a != b);
        CHECK(IDWriteFont3_Equals(a, (IDWriteFont *)b));
        CHECK(IDWriteFont3_Equals(a, (IDWriteFont *)a));
        CHECK(!IDWriteFont3_Equals(a, (IDWriteFont *)c));
        CHECK(!IDWriteFont3_Equals(c, (IDWriteFont *)a));
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_font_qi_to_font3) {
    FM_OPEN(m, "FmF3g", F_ONE);
    IDWriteFont *f = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, 0, &f));
    if (f) {
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteFont, 1), DW_QI_ROW(IDWriteFont3, 1), DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFontList, 0), DW_QI_ROW(IDWriteFontFace, 0) };
        DW_CHECK_QI(f, rows);
        IDWriteFont_Release(f);
    }
    fm_close(&m);
}

/* ---- GetFontLocality / GetFontFaceReference ----------------------------------------------------------- */
TEST(f12_locality_every_index_is_local) {
    FM_OPEN(m, "FmLoc", F_W9);
    NEED_F1(m, f1);
    UINT32 i;
    for (i = 0; i < m.count; i++) CHECKF(IDWriteFontFamily1_GetFontLocality(f1, i) == DWRITE_LOCALITY_LOCAL, "index %u", i);
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_locality_queries_loader_for_remote_interface) {
    FM_OPEN(m, "FmLoc2", F_ONE);
    NEED_F1(m, f1);
    LONG before = dwx_qi_calls(m.fl);
    DWRITE_LOCALITY l = IDWriteFontFamily1_GetFontLocality(f1, 0);
    EXPECT_EQ(l, DWRITE_LOCALITY_LOCAL);
    /* FontFileReference::GetLocality asks the loader for IDWriteRemoteFontFileLoader (68648c83-...), which our loader refuses */
    t_note("loader QueryInterface calls during GetFontLocality: %ld", (long)(dwx_qi_calls(m.fl) - before));
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_face_reference_basics) {
    FM_OPEN(m, "FmRef1", F_RIBBI);
    NEED_F1(m, f1);
    UINT32 i;
    for (i = 0; i < m.count; i++) {
        IDWriteFontFaceReference *r = NULL;
        CHECK_HR_OK(IDWriteFontFamily1_GetFontFaceReference(f1, i, &r));
        CHECK(r != NULL);
        if (r) {
            EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 0);   /* single-face files */
            EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), DWRITE_FONT_SIMULATIONS_NONE);
            IDWriteFontFaceReference_Release(r);
        }
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_face_reference_out_of_range) {
    FM_OPEN(m, "FmRef2", F_RIBBI);
    NEED_F1(m, f1);
    const UINT32 bad[] = { m.count, m.count + 1, 0xFFFF, 0x10000, 0xFFFFFFFFu };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)&i;
        HRESULT hr = IDWriteFontFamily1_GetFontFaceReference(f1, bad[i], &r);
        CHECKF(hr == E_FAIL, "GetFontFaceReference(%u) -> %s", bad[i], dw_hr_name(hr));
        CHECKF(r == NULL, "GetFontFaceReference(%u) left %p", bad[i], (void *)r);

    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_face_reference_file_key_identifies_blob) {
    FM_OPEN(m, "FmRef3", F_RIBBI);
    NEED_F1(m, f1);
    UINT32 i;
    for (i = 0; i < m.count; i++) {
        IDWriteFontFaceReference *r = NULL; IDWriteFontFile *file = NULL;
        if (SUCCEEDED(IDWriteFontFamily1_GetFontFaceReference(f1, i, &r)) && r && SUCCEEDED(IDWriteFontFaceReference_GetFontFile(r, &file)) && file) {
            const void *key = NULL; UINT32 ks = 0;
            CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(file, &key, &ks));
            EXPECT_EQ(ks, 4);
            /* our loader's keys are the blob numbers; the family's i-th font is the face built from blob src[i] */
            if (ks == 4 && key && m.src[i] >= 0) EXPECT_EQ(*(const UINT32 *)key, m.src[i]);
            IDWriteFontFileLoader *ld = NULL;
            if (SUCCEEDED(IDWriteFontFile_GetLoader(file, &ld)) && ld) { CHECK(ld == (IDWriteFontFileLoader *)m.fl); IDWriteFontFileLoader_Release(ld); }
        } else CHECKF(0, "index %u: no file", i);
        DW_RELEASE(file); DW_RELEASE(r);
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_face_reference_equals) {
    FM_OPEN(m, "FmRef4", F_RIBBI);
    NEED_F1(m, f1);
    IDWriteFontFaceReference *a = NULL, *b = NULL, *c = NULL;
    CHECK_HR_OK(IDWriteFontFamily1_GetFontFaceReference(f1, 0, &a));
    CHECK_HR_OK(IDWriteFontFamily1_GetFontFaceReference(f1, 0, &b));
    CHECK_HR_OK(IDWriteFontFamily1_GetFontFaceReference(f1, 1, &c));
    if (a && b && c) {
        CHECK(a != b);
        CHECK(IDWriteFontFaceReference_Equals(a, b));
        CHECK(IDWriteFontFaceReference_Equals(a, a));
        CHECK(!IDWriteFontFaceReference_Equals(a, c));
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_face_reference_creates_face_with_simulations) {
    FM_OPEN(m, "FmRef5", F_ONE);
    NEED_F1(m, f1);
    IDWriteFontFaceReference *r = NULL;
    CHECK_HR_OK(IDWriteFontFamily1_GetFontFaceReference(f1, 0, &r));
    if (r) {
        static const DWRITE_FONT_SIMULATIONS sims[] = { DWRITE_FONT_SIMULATIONS_NONE, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE };
        size_t i; IDWriteFontFace3 *face = NULL;
        CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFace(r, &face));
        if (face) { EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE); IDWriteFontFace3_Release(face); }
        for (i = 0; i < sizeof sims / sizeof sims[0]; i++) {
            face = NULL;
            CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFaceWithSimulations(r, sims[i], &face));
            if (face) { EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), sims[i]); IDWriteFontFace3_Release(face); }
        }
        IDWriteFontFaceReference_Release(r);
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}
TEST(f12_face_reference_for_matched_simulated_font) {
    FM_OPEN(m, "FmRef6", F_ONE);
    IDWriteFontList *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 700, 5, 2, &l));
    if (l) {
        IDWriteFontList1 *l1 = NULL;
        if (SUCCEEDED(IDWriteFontList_QueryInterface(l, &IID_IDWriteFontList1, (void **)&l1)) && l1) {
            IDWriteFontFaceReference *r = NULL;
            CHECK_HR_OK(IDWriteFontList1_GetFontFaceReference(l1, 0, &r));
            if (r) {
                /* the reference carries the simulations chosen by the matcher (FontFaceKey simulations field) */
                EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE);
                IDWriteFontFace3 *face = NULL;
                CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFace(r, &face));
                if (face) { EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE); IDWriteFontFace3_Release(face); }
                IDWriteFontFaceReference_Release(r);
            }
            IDWriteFontList1_Release(l1);
        }
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}
TEST(f12_font3_face_reference_and_face) {
    FM_OPEN(m, "FmRef7", F_ONE);
    NEED_F1(m, f1);
    IDWriteFont3 *f3 = NULL;
    CHECK_HR_OK(IDWriteFontFamily1_GetFont(f1, 0, &f3));
    if (f3) {
        IDWriteFontFaceReference *r = NULL; IDWriteFontFace3 *face = NULL;
        CHECK_HR_OK(IDWriteFont3_GetFontFaceReference(f3, &r));
        if (r) { EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), DWRITE_FONT_SIMULATIONS_NONE); IDWriteFontFaceReference_Release(r); }
        CHECK_HR_OK(IDWriteFont3_CreateFontFace(f3, &face));
        if (face) {
            EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE);
            EXPECT_EQ(IDWriteFontFace3_GetWeight(face), 400);
            EXPECT_EQ(IDWriteFontFace3_GetStretch(face), 5);
            EXPECT_EQ(IDWriteFontFace3_GetStyle(face), DWRITE_FONT_STYLE_NORMAL);
            IDWriteFontFace3_Release(face);
        }
        IDWriteFont3_Release(f3);
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}

/* ---- CreateFontFace and simulations -------------------------------------------------------------------- */
TEST(f12_font_create_face_carries_simulations) {
    FM_OPEN(m, "FmSimF", F_ONE);
    static const struct { int rw, st, sims; } rq[] = { {400,0,0}, {700,0,1}, {400,1,2}, {400,2,2}, {700,2,3}, {900,1,3}, {551,0,1}, {550,0,0} };
    size_t i;
    for (i = 0; i < sizeof rq / sizeof rq[0]; i++) {
        IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, (DWRITE_FONT_WEIGHT)rq[i].rw, 5, (DWRITE_FONT_STYLE)rq[i].st, &font));
        if (font) {
            CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &face));
            if (face) {
                /* IDWriteFont::CreateFontFace creates the face with the simulations of the font */
                CHECKF((int)IDWriteFontFace_GetSimulations(face) == rq[i].sims, "request %zu: face simulations %d want %d", i, (int)IDWriteFontFace_GetSimulations(face), rq[i].sims);
                IDWriteFontFace_Release(face);
            }
        }
        DW_RELEASE(font);
    }
    fm_close(&m);
}
TEST(f12_simulated_face_reports_simulated_attributes_via_font3) {
    FM_OPEN(m, "FmSimG", F_ONE);
    NEED_F1(m, f1);
    IDWriteFont *font = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 700, 5, 2, &font));
    if (font) {
        IDWriteFont3 *f3 = NULL;
        if (SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&f3)) && f3) {
            EXPECT_EQ(IDWriteFont3_GetWeight(f3), 700);
            EXPECT_EQ(IDWriteFont3_GetStyle(f3), DWRITE_FONT_STYLE_OBLIQUE);
            EXPECT_EQ(IDWriteFont3_GetSimulations(f3), 3);
            IDWriteFont3_Release(f3);
        }
        IDWriteFont_Release(font);
    }
    IDWriteFontFamily1_Release(f1);
    fm_close(&m);
}

/* ---- IDWriteFontList1 / IDWriteFontList2 on matching lists --------------------------------------------- */
TEST(f12_list1_matches_list) {
    FM_OPEN(m, "FmL1a", F_W9);
    IDWriteFontList *l = NULL; IDWriteFontList1 *l1 = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l && SUCCEEDED(IDWriteFontList_QueryInterface(l, &IID_IDWriteFontList1, (void **)&l1)) && l1) {
        UINT32 c = IDWriteFontList_GetFontCount(l), i;
        EXPECT_EQ(IDWriteFontList1_GetFontCount(l1), c);
        for (i = 0; i < c; i++) {
            IDWriteFont3 *f3 = NULL; IDWriteFont *f = NULL;
            CHECK_HR_OK(IDWriteFontList1_GetFont(l1, i, &f3));
            CHECK_HR_OK(IDWriteFontList_GetFont(l, i, &f));
            if (f3 && f) CHECK(IDWriteFont3_Equals(f3, f));
            CHECK(IDWriteFontList1_GetFontLocality(l1, i) == DWRITE_LOCALITY_LOCAL);
            DW_RELEASE(f3); DW_RELEASE(f);
        }
        IDWriteFontList1_Release(l1);
    }
    DW_RELEASE(l);
    fm_close(&m);
}
TEST(f12_list1_out_of_range) {
    FM_OPEN(m, "FmL1b", F_W9);
    IDWriteFontList *l = NULL; IDWriteFontList1 *l1 = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l && SUCCEEDED(IDWriteFontList_QueryInterface(l, &IID_IDWriteFontList1, (void **)&l1)) && l1) {
        UINT32 c = IDWriteFontList1_GetFontCount(l1);
        const UINT32 bad[] = { c, c + 1, 0xFFFF, 0xFFFFFFFFu }; size_t i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            IDWriteFont3 *f3 = (IDWriteFont3 *)(ULONG_PTR)&i; IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)&i;
            CHECK_HR(IDWriteFontList1_GetFont(l1, bad[i], &f3), E_FAIL); CHECK_OUT_NULL(f3);
            CHECK_HR(IDWriteFontList1_GetFontFaceReference(l1, bad[i], &r), E_FAIL); CHECK_OUT_NULL(r);
            DW_RELEASE(f3); DW_RELEASE(r);
        }
        IDWriteFontList1_Release(l1);
    }
    DW_RELEASE(l);
    fm_close(&m);
}
TEST(f12_list1_face_references_follow_list_order) {
    FM_OPEN(m, "FmL1c", F_W9);
    IDWriteFontList *l = NULL; IDWriteFontList1 *l1 = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 450, 5, 0, &l));
    if (l && SUCCEEDED(IDWriteFontList_QueryInterface(l, &IID_IDWriteFontList1, (void **)&l1)) && l1) {
        UINT32 c = IDWriteFontList1_GetFontCount(l1), i;
        for (i = 0; i < c; i++) {
            IDWriteFontFaceReference *r = NULL; IDWriteFont3 *f3 = NULL; IDWriteFontFaceReference *r2 = NULL;
            CHECK_HR_OK(IDWriteFontList1_GetFontFaceReference(l1, i, &r));
            CHECK_HR_OK(IDWriteFontList1_GetFont(l1, i, &f3));
            if (f3) IDWriteFont3_GetFontFaceReference(f3, &r2);
            if (r && r2) CHECKF(IDWriteFontFaceReference_Equals(r, r2), "entry %u", i);
            DW_RELEASE(r); DW_RELEASE(r2); DW_RELEASE(f3);
        }
        IDWriteFontList1_Release(l1);
    }
    DW_RELEASE(l);
    fm_close(&m);
}
TEST(f12_list2_fontset) {
    FM_OPEN(m, "FmL2", F_W9);
    IDWriteFontList *l = NULL; IDWriteFontList2 *l2 = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l && SUCCEEDED(IDWriteFontList_QueryInterface(l, &IID_IDWriteFontList2, (void **)&l2)) && l2) {
        IDWriteFontSet1 *s = NULL;
        CHECK_HR_OK(IDWriteFontList2_GetFontSet(l2, &s));
        CHECK(s != NULL);
        if (s) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(s), IDWriteFontList_GetFontCount(l)); IDWriteFontSet1_Release(s); }
        IDWriteFontList2_Release(l2);
    }
    DW_RELEASE(l);
    fm_close(&m);
}

/* ---- IDWriteFontFamily2 ---------------------------------------------------------------------------------- */
TEST(f12_family2_fontset_counts) {
    FM_OPEN(m, "FmF2a", F_W9);
    NEED_F2(m, f2);
    IDWriteFontSet1 *s = NULL;
    CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(f2, &s));
    CHECK(s != NULL);
    if (s) {
        UINT32 i, c = IDWriteFontSet1_GetFontCount(s);
        EXPECT_EQ(c, m.count);
        for (i = 0; i < c; i++) {
            IDWriteFontFaceReference1 *r1 = NULL; IDWriteFontFaceReference *r = NULL;
            CHECK_HR_OK(IDWriteFontSet1_GetFontFaceReference(s, i, &r1));
            (void)r;
            DW_RELEASE(r1);
        }
        IDWriteFontSet1_Release(s);
    }
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_fontset_is_new_object_each_call) {
    FM_OPEN(m, "FmF2b", F_RIBBI);
    NEED_F2(m, f2);
    IDWriteFontSet1 *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(f2, &a));
    CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(f2, &b));
    CHECK(a && b && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_fontset_family_name_property) {
    FM_OPEN(m, "FmF2c", F_RIBBI);
    NEED_F2(m, f2);
    IDWriteFontSet1 *s = NULL;
    CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(f2, &s));
    if (s) {
        UINT32 i;
        for (i = 0; i < m.count; i++) {
            IDWriteLocalizedStrings *names = NULL; BOOL ex = FALSE;
            HRESULT hr = IDWriteFontSet1_GetPropertyValues(s, i, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, &ex, &names);
            if (SUCCEEDED(hr) && ex && names) { char b[64]; dw_ls_en(names, b, sizeof b); EXPECT_STR(b, "FmF2c"); }
            else t_note("font %u typographic family name property: %s exists=%d", i, dw_hr_name(hr), ex);
            DW_RELEASE(names);
        }
        IDWriteFontSet1_Release(s);
    }
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_matching_argument_validation) {
    FM_OPEN(m, "FmF2d", F_RIBBI);
    NEED_F2(m, f2);
    DWRITE_FONT_AXIS_VALUE v = { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f };
    IDWriteFontList2 *l;
    /* DWriteFontFamily::GetMatchingFonts(axis): the count must be < 0x10000 and the array non-NULL unless the count is 0, else E_INVALIDARG;
     * the out parameter is zeroed first and nothing is read on the error paths. */
    struct { const DWRITE_FONT_AXIS_VALUE *p; UINT32 n; } bad[] = { { NULL, 1 }, { NULL, 2 }, { NULL, 100 }, { NULL, 0xFFFF }, { NULL, 0x10000 }, { &v, 0x10000 }, { &v, 0x10001 }, { &v, 0x7FFFFFFF }, { &v, 0xFFFFFFFFu }, { NULL, 0xFFFFFFFFu } };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        l = (IDWriteFontList2 *)(ULONG_PTR)&i;
        HRESULT hr = IDWriteFontFamily2_GetMatchingFonts(f2, bad[i].p, bad[i].n, &l);
        CHECKF(hr == E_INVALIDARG, "case %zu (n=%u ptr=%p) -> %s", i, bad[i].n, (const void *)bad[i].p, dw_hr_name(hr));
        CHECKF(l == NULL, "case %zu left %p", i, (void *)l);

    }
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_matching_zero_values) {
    FM_OPEN(m, "FmF2e", F_RIBBI);
    NEED_F2(m, f2);
    IDWriteFontList2 *l = NULL; DWRITE_FONT_AXIS_VALUE v = { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f };
    CHECK_HR_OK(IDWriteFontFamily2_GetMatchingFonts(f2, NULL, 0, &l));
    CHECK(l != NULL);
    if (l) { CHECK(IDWriteFontList2_GetFontCount(l) >= 1); IDWriteFontList2_Release(l); }
    l = NULL;
    CHECK_HR_OK(IDWriteFontFamily2_GetMatchingFonts(f2, &v, 0, &l));   /* count 0 with a valid pointer */
    CHECK(l != NULL);
    DW_RELEASE(l);
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
static void axis_case(fm *m, IDWriteFontFamily2 *f2, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, const char *what) {
    IDWriteFontList2 *l = NULL;
    HRESULT hr = IDWriteFontFamily2_GetMatchingFonts(f2, v, n, &l);
    CHECKF(hr == S_OK && l, "%s -> %s", what, dw_hr_name(hr));
    if (hr == S_OK && l) {
        UINT32 c = IDWriteFontList2_GetFontCount(l), i;
        CHECKF(c >= 1 && c <= m->count, "%s: %u fonts of %u", what, c, m->count);
        for (i = 0; i < c; i++) {
            IDWriteFont3 *f3 = NULL; IDWriteFontFaceReference *r = NULL;
            CHECK_HR_OK(IDWriteFontList1_GetFont((IDWriteFontList1 *)l, i, &f3));
            CHECK_HR_OK(IDWriteFontList2_GetFontFaceReference(l, i, &r));
            CHECK(IDWriteFontList2_GetFontLocality(l, i) == DWRITE_LOCALITY_LOCAL);
            DW_RELEASE(f3); DW_RELEASE(r);
        }
        { IDWriteFont3 *f3 = (IDWriteFont3 *)(ULONG_PTR)&c; CHECK_HR(IDWriteFontList1_GetFont((IDWriteFontList1 *)l, c, &f3), E_FAIL); CHECK_OUT_NULL(f3); }
        {
            IDWriteFontCollection *cc = NULL;
            CHECK_HR_OK(IDWriteFontList2_GetFontCollection(l, &cc));
            if (cc) { CHECK(dw_same_identity((IUnknown *)cc, (IUnknown *)m->coll)); IDWriteFontCollection_Release(cc); }
        }
        {
            IDWriteFont3 *first = NULL; fm_face a; int s;
            if (SUCCEEDED(IDWriteFontList1_GetFont((IDWriteFontList1 *)l, 0, &first)) && fm_read_font((IDWriteFont *)first, &a, &s))
                t_note("%s: first font w=%d wd=%d st=%d sims=%d (of %u)", what, a.w, a.wd, a.st, s, c);
            DW_RELEASE(first);
        }
        IDWriteFontList2_Release(l);
    }
}
TEST(f12_family2_matching_axis_values_structure) {
    FM_OPEN(m, "FmF2f", F_W9);
    NEED_F2(m, f2);
    DWRITE_FONT_AXIS_VALUE v1[] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f } };
    DWRITE_FONT_AXIS_VALUE v2[] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 300.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 100.0f } };
    DWRITE_FONT_AXIS_VALUE v3[] = { { DWRITE_FONT_AXIS_TAG_ITALIC, 1.0f } };
    DWRITE_FONT_AXIS_VALUE v4[] = { { DWRITE_FONT_AXIS_TAG_SLANT, -12.0f }, { DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE, 12.0f } };
    DWRITE_FONT_AXIS_VALUE v5[] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 1000.0f } };
    DWRITE_FONT_AXIS_VALUE v6[] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 1.0f } };
    axis_case(&m, f2, v1, 1, "wght=700"); axis_case(&m, f2, v2, 2, "wght=300,wdth=100"); axis_case(&m, f2, v3, 1, "ital=1");
    axis_case(&m, f2, v4, 2, "slnt,opsz"); axis_case(&m, f2, v5, 1, "wght=1000"); axis_case(&m, f2, v6, 1, "wght=1");
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_matching_unknown_axis_tag) {
    FM_OPEN(m, "FmF2g", F_RIBBI);
    NEED_F2(m, f2);
    DWRITE_FONT_AXIS_VALUE v[] = { { (DWRITE_FONT_AXIS_TAG)0x31323334, 5.0f }, { (DWRITE_FONT_AXIS_TAG)0, 0.0f }, { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
    axis_case(&m, f2, v, 3, "unknown tags");
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_matching_duplicate_axis_values) {
    FM_OPEN(m, "FmF2h", F_RIBBI);
    NEED_F2(m, f2);
    DWRITE_FONT_AXIS_VALUE v[] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f }, { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
    axis_case(&m, f2, v, 3, "duplicate wght");
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_matching_many_axis_values) {
    FM_OPEN(m, "FmF2i", F_RIBBI);
    NEED_F2(m, f2);
    DWRITE_FONT_AXIS_VALUE v[64]; int i;
    for (i = 0; i < 64; i++) v[i] = (DWRITE_FONT_AXIS_VALUE){ (DWRITE_FONT_AXIS_TAG)(0x41414141 + i), (float)i };
    axis_case(&m, f2, v, 64, "64 axis values");
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_matching_extreme_float_values) {
    FM_OPEN(m, "FmF2j", F_RIBBI);
    NEED_F2(m, f2);
    DWRITE_FONT_AXIS_VALUE v1[] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 1.0e30f } };
    DWRITE_FONT_AXIS_VALUE v2[] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, -1.0e30f } };
    DWRITE_FONT_AXIS_VALUE v3[] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 0.0f } };
    axis_case(&m, f2, v1, 1, "wght=+1e30"); axis_case(&m, f2, v2, 1, "wght=-1e30"); axis_case(&m, f2, v3, 1, "wght=0");
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_matching_returns_new_list_each_call) {
    FM_OPEN(m, "FmF2k", F_RIBBI);
    NEED_F2(m, f2);
    IDWriteFontList2 *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily2_GetMatchingFonts(f2, NULL, 0, &a));
    CHECK_HR_OK(IDWriteFontFamily2_GetMatchingFonts(f2, NULL, 0, &b));
    CHECK(a && b && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_matching_list_holds_family) {
    FM_OPEN(m, "FmF2l", F_RIBBI);
    NEED_F2(m, f2);
    ULONG before = dw_refcount((IUnknown *)m.fam);
    IDWriteFontList2 *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily2_GetMatchingFonts(f2, NULL, 0, &l));
    EXPECT_REFS(m.fam, before + 1);
    DW_RELEASE(l);
    EXPECT_REFS(m.fam, before);
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}
TEST(f12_family2_same_methods_as_family1_and_family) {
    FM_OPEN(m, "FmF2m", F_RIBBI);
    NEED_F2(m, f2);
    EXPECT_EQ(IDWriteFontFamily2_GetFontCount(f2), 4);
    IDWriteFont3 *f3 = NULL; IDWriteFont *f = NULL;
    CHECK_HR_OK(IDWriteFontFamily1_GetFont((IDWriteFontFamily1 *)f2, 3, &f3));
    CHECK_HR_OK(IDWriteFontFamily2_GetFirstMatchingFont(f2, 700, 5, 2, &f));
    if (f3 && f) CHECK(IDWriteFont3_Equals(f3, f));
    CHECK(IDWriteFontFamily2_GetFontLocality(f2, 0) == DWRITE_LOCALITY_LOCAL);
    DW_RELEASE(f3); DW_RELEASE(f);
    IDWriteFontFamily2_Release(f2);
    fm_close(&m);
}

/* ---- IDWriteFontCollection1::GetFontFamily(IDWriteFontFamily1) ---------------------------------------------- */
TEST(f12_collection1_getfontfamily_family1) {
    FM_OPEN(m, "FmC1", F_RIBBI);
    IDWriteFontCollection1 *c1 = NULL;
    if (FAILED(IDWriteFontCollection_QueryInterface(m.coll, &IID_IDWriteFontCollection1, (void **)&c1)) || !c1) { fm_close(&m); t_skip("IDWriteFontCollection1 unsupported"); return; }
    IDWriteFontFamily2 *f2 = NULL; IDWriteFontFamily1 *f1 = NULL;
    CHECK_HR_OK(IDWriteFontCollection1_GetFontFamily(c1, 0, &f2));
    if (f2) {
        EXPECT_EQ(IDWriteFontFamily2_GetFontCount(f2), 4);
        CHECK(IDWriteFontFamily2_GetFontLocality(f2, 0) == DWRITE_LOCALITY_LOCAL);
        DW_RELEASE(f2);
    }
    CHECK_HR_OK(IDWriteFontCollection1_GetFontFamily(c1, 0, (IDWriteFontFamily2 **)&f1));
    DW_RELEASE(f1);
    f2 = (IDWriteFontFamily2 *)(ULONG_PTR)&m;
    CHECK_HR_FAILED(IDWriteFontCollection1_GetFontFamily(c1, 1, &f2));
    CHECK_OUT_NULL(f2);
    IDWriteFontCollection1_Release(c1);
    fm_close(&m);
}

