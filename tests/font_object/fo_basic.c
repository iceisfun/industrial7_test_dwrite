/* IDWriteFont object basics: interface set, identity, lifetime, family back-pointer, weight/stretch/style/simulations.
 * IDWriteFont (dwrite.dll) is one COM class answering IUnknown, IDWriteFont, IDWriteFont1, IDWriteFont2 and IDWriteFont3
 * (QueryInterfaceImpl<..., TypeList<IDWriteFont1, TypeList<IDWriteFont2, TypeList<IDWriteFont3>>>>). */
#include "fo.h"

static int iid_eq(const IID *a, const IID *b) { return IsEqualGUID(a, b); }

/* ---- interface set -------------------------------------------------------------------------------------- */
static const dw_qi_row k_font_qi[] = {
    DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFont, 1), DW_QI_ROW(IDWriteFont1, 1), DW_QI_ROW(IDWriteFont2, 1), DW_QI_ROW(IDWriteFont3, 1),
    DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontFace1, 0), DW_QI_ROW(IDWriteFontFace3, 0), DW_QI_ROW(IDWriteFontFamily, 0),
    DW_QI_ROW(IDWriteFontFamily1, 0), DW_QI_ROW(IDWriteFontList, 0), DW_QI_ROW(IDWriteFontList1, 0), DW_QI_ROW(IDWriteFontCollection, 0),
    DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFontFaceReference, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0),
    DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteTextLayout, 0), DW_QI_ROW(IDWriteFontResource, 0),
};

TEST(font_qi_matrix_synthetic) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoQiSyn"); FO_OPEN1(x, s);
    DW_CHECK_QI(x.font[0], k_font_qi);
    fo_close(&x);
}
TEST(font_qi_matrix_system_arial) {
    FO_SYS_REG(font, L"Arial");
    DW_CHECK_QI(font, k_font_qi);
    DW_RELEASE(font);
}
TEST(font_qi_every_public_iid) {
    /* The class answers exactly IUnknown + IDWriteFont..IDWriteFont3; every other public IID is E_NOINTERFACE with *ppv == NULL. */
    fo_fx x; ttf_spec s; int n, i; const dw_qi_row *t = dw_iid_table(&n);
    fo_spec(&s, "FoQiAll"); FO_OPEN1(x, s);
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)1; HRESULT hr = IDWriteFont_QueryInterface(x.font[0], t[i].iid, &p);
        int allowed = iid_eq(t[i].iid, &IID_IUnknown) || iid_eq(t[i].iid, &IID_IDWriteFont) || iid_eq(t[i].iid, &IID_IDWriteFont1) ||
                      iid_eq(t[i].iid, &IID_IDWriteFont2) || iid_eq(t[i].iid, &IID_IDWriteFont3);
        if (allowed) { CHECKF(hr == S_OK, "%s -> %s", t[i].name, dw_hr_name(hr)); if (p && p != (void *)1) IUnknown_Release((IUnknown *)p); }
        else { CHECKF(hr == E_NOINTERFACE, "%s -> %s", t[i].name, dw_hr_name(hr)); CHECKF(p == NULL, "%s: out not zeroed", t[i].name); }
    }
    fo_close(&x);
}
TEST(font_qi_unknown_iid_fails) {
    static const IID zero_iid = { 0 }; static const IID bogus = { 0x12345678, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    fo_fx x; ttf_spec s; void *p = (void *)(ULONG_PTR)1; fo_spec(&s, "FoQiBogus"); FO_OPEN1(x, s);
    CHECK_HR(IDWriteFont_QueryInterface(x.font[0], &bogus, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
    p = (void *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFont_QueryInterface(x.font[0], &zero_iid, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
    fo_close(&x);
}
TEST(font_qi_versions_share_identity) {
    fo_fx x; ttf_spec s; IDWriteFont1 *f1 = NULL; IDWriteFont2 *f2 = NULL; IDWriteFont3 *f3 = NULL; IUnknown *u0 = NULL, *u1 = NULL, *u3 = NULL;
    fo_spec(&s, "FoQiId"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont1, (void **)&f1));
    CHECK_HR_OK(IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont2, (void **)&f2));
    CHECK_HR_OK(IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont3, (void **)&f3));
    CHECK(f1 && f2 && f3);
    if (f1 && f2 && f3) {
        /* all interfaces are the primary base of one object: single vtable pointer value, identical pointers */
        CHECK((void *)f1 == (void *)x.font[0]); CHECK((void *)f2 == (void *)x.font[0]); CHECK((void *)f3 == (void *)x.font[0]);
        IDWriteFont_QueryInterface(x.font[0], &IID_IUnknown, (void **)&u0);
        IDWriteFont1_QueryInterface(f1, &IID_IUnknown, (void **)&u1);
        IDWriteFont3_QueryInterface(f3, &IID_IUnknown, (void **)&u3);
        CHECK(u0 && u0 == u1 && u1 == u3);
    }
    DW_RELEASE(u0); DW_RELEASE(u1); DW_RELEASE(u3); DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(f3);
    fo_close(&x);
}
TEST(font_qi_from_each_version_reaches_all) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3 = NULL; IDWriteFont *f0 = NULL; IDWriteFont2 *f2 = NULL;
    fo_spec(&s, "FoQiAny"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont3, (void **)&f3));
    if (f3) {
        CHECK_HR_OK(IDWriteFont3_QueryInterface(f3, &IID_IDWriteFont, (void **)&f0));
        CHECK_HR_OK(IDWriteFont3_QueryInterface(f3, &IID_IDWriteFont2, (void **)&f2));
        CHECK(f0 == x.font[0]);
    }
    DW_RELEASE(f0); DW_RELEASE(f2); DW_RELEASE(f3);
    fo_close(&x);
}

/* ---- refcount ----------------------------------------------------------------------------------------- */
TEST(font_refcount_addref_release_pairs) {
    fo_fx x; ttf_spec s; ULONG base, a, b, c, d; fo_spec(&s, "FoRef1"); FO_OPEN1(x, s);
    base = dw_refcount((IUnknown *)x.font[0]);
    a = IDWriteFont_AddRef(x.font[0]); EXPECT_EQ(a, base + 1);
    b = IDWriteFont_AddRef(x.font[0]); EXPECT_EQ(b, base + 2);
    c = IDWriteFont_Release(x.font[0]); EXPECT_EQ(c, base + 1);
    d = IDWriteFont_Release(x.font[0]); EXPECT_EQ(d, base);
    EXPECT_REFS(x.font[0], base);
    fo_close(&x);
}
TEST(font_refcount_qi_adds_exactly_one) {
    fo_fx x; ttf_spec s; ULONG base; void *p = NULL; const IID *ids[] = { &IID_IUnknown, &IID_IDWriteFont, &IID_IDWriteFont1, &IID_IDWriteFont2, &IID_IDWriteFont3 };
    int i; fo_spec(&s, "FoRef2"); FO_OPEN1(x, s);
    base = dw_refcount((IUnknown *)x.font[0]);
    for (i = 0; i < 5; i++) {
        p = NULL; CHECK_HR_OK(IDWriteFont_QueryInterface(x.font[0], ids[i], &p));
        EXPECT_REFS(x.font[0], base + 1);
        if (p) IUnknown_Release((IUnknown *)p);
        EXPECT_REFS(x.font[0], base);
    }
    fo_close(&x);
}
TEST(font_refcount_failed_qi_leaves_count) {
    fo_fx x; ttf_spec s; ULONG base; void *p = NULL; fo_spec(&s, "FoRef3"); FO_OPEN1(x, s);
    base = dw_refcount((IUnknown *)x.font[0]);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFontFace, &p);
    EXPECT_REFS(x.font[0], base);
    fo_close(&x);
}
TEST(font_refcount_each_getter_returns_new_object_ref) {
    /* GetFontFamily AddRefs the stored family pointer; CreateFontFace/GetFontFaceReference return new references. */
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL, *fam2 = NULL; ULONG b0, b1; fo_spec(&s, "FoRef4"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetFontFamily(x.font[0], &fam)); CHECK(fam != NULL);
    if (fam) {
        b0 = dw_refcount((IUnknown *)fam);
        CHECK_HR_OK(IDWriteFont_GetFontFamily(x.font[0], &fam2));
        b1 = dw_refcount((IUnknown *)fam);
        EXPECT_EQ(b1, b0 + 1);
        CHECK(fam == fam2);
    }
    DW_RELEASE(fam2); DW_RELEASE(fam);
    fo_close(&x);
}
TEST(font_destroyed_after_last_release_font_object_independent) {
    /* Two separately obtained IDWriteFont for the same face are independent COM objects (distinct lifetimes). */
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL; IDWriteFont *a = NULL, *b = NULL; fo_spec(&s, "FoRef5"); FO_OPEN1(x, s);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, 0, &a)); CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, 0, &b));
        if (a && b) {
            ULONG ra = dw_refcount((IUnknown *)a), rb = dw_refcount((IUnknown *)b);
            if (a != b) { EXPECT_EQ(ra, 1); EXPECT_EQ(rb, 1); } else t_note("GetFont returned a shared object (refs %lu)", ra);
            IDWriteFont_Release(a); a = NULL;
            CHECK(IDWriteFont_GetWeight(b) == DWRITE_FONT_WEIGHT_NORMAL);
        }
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(fam);
    }
    fo_close(&x);
}

/* ---- lifetime ----------------------------------------------------------------------------------------- */
TEST(font_survives_collection_and_family_release) {
    fo_fx x; ttf_spec s; IDWriteFont *font; fo_spec(&s, "FoLife1"); FO_OPEN1(x, s);
    font = x.font[0]; IDWriteFont_AddRef(font);
    DW_RELEASE(x.font[0]); DW_RELEASE(x.coll);
    EXPECT_EQ(IDWriteFont_GetWeight(font), DWRITE_FONT_WEIGHT_NORMAL);
    EXPECT_EQ(IDWriteFont_GetStretch(font), DWRITE_FONT_STRETCH_NORMAL);
    {   IDWriteFontFamily *fam = NULL; CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &fam)); CHECK(fam != NULL);
        if (fam) { CHECK(IDWriteFontFamily_GetFontCount(fam) >= 1); DW_RELEASE(fam); } }
    {   IDWriteLocalizedStrings *n = NULL; CHECK_HR_OK(IDWriteFont_GetFaceNames(font, &n)); CHECK(n && IDWriteLocalizedStrings_GetCount(n) >= 1); DW_RELEASE(n); }
    IDWriteFont_Release(font);
    fo_close(&x);
}
TEST(font_survives_factory_release) {
    /* The font keeps its owning factory alive: metadata stays valid after the application released the factory. */
    fo_fx x; ttf_spec s; IDWriteFont *font; DWRITE_FONT_METRICS m; fo_spec(&s, "FoLife2"); FO_OPEN1(x, s);
    font = x.font[0]; IDWriteFont_AddRef(font);
    fo_close(&x);          /* releases coll, loaders (unregistered) and our factory reference */
    memset(&m, 0xAB, sizeof m); IDWriteFont_GetMetrics(font, &m);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    EXPECT_EQ(IDWriteFont_GetWeight(font), DWRITE_FONT_WEIGHT_NORMAL);
    CHECK(IDWriteFont_GetSimulations(font) == DWRITE_FONT_SIMULATIONS_NONE);
    {   IDWriteLocalizedStrings *n = NULL; BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(font, DWRITE_INFORMATIONAL_STRING_FULL_NAME, &n, &ex)); CHECK(ex && n); DW_RELEASE(n); }
    IDWriteFont_Release(font);
}
TEST(font_survives_collection_loader_unregistered) {
    /* metadata is cached in the font; no loader call is needed to answer it */
    fo_fx x; ttf_spec s; fo_spec(&s, "FoLife3"); FO_OPEN1(x, s);
    dwx_coll_unregister(x.cl); dwx_loader_unregister(x.fl);
    EXPECT_EQ(IDWriteFont_GetWeight(x.font[0]), DWRITE_FONT_WEIGHT_NORMAL);
    EXPECT_EQ(IDWriteFont_GetStyle(x.font[0]), DWRITE_FONT_STYLE_NORMAL);
    fo_close(&x);
}

/* ---- family back pointer --------------------------------------------------------------------------------- */
TEST(font_family_same_pointer_each_call) {
    /* IDWriteFont::GetFontFamily returns the stored family pointer (AddRef'd) every time. */
    fo_fx x; ttf_spec s; IDWriteFontFamily *a = NULL, *b = NULL; fo_spec(&s, "FoFamId"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetFontFamily(x.font[0], &a)); CHECK_HR_OK(IDWriteFont_GetFontFamily(x.font[0], &b));
    CHECK(a != NULL && a == b);
    DW_RELEASE(a); DW_RELEASE(b); fo_close(&x);
}
TEST(font_family_name_matches_source) {
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *n = NULL; char buf[128]; fo_spec(&s, "FoFamName"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetFontFamily(x.font[0], &fam));
    if (fam) {
        CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &n));
        if (n) { dw_ls_en(n, buf, sizeof buf); EXPECT_STR(buf, "FoFamName"); }
        DW_RELEASE(n); DW_RELEASE(fam);
    }
    fo_close(&x);
}
TEST(font_family_is_in_collection) {
    fo_fx x; ttf_spec s; IDWriteFontFamily *fam = NULL, *cf = NULL; UINT32 idx = 99; BOOL ex = FALSE; fo_spec(&s, "FoFamColl"); FO_OPEN1(x, s);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(x.coll, L"FoFamColl", &idx, &ex)); CHECK(ex);
    if (ex) { CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(x.coll, idx, &cf)); if (fam && cf) CHECK(fam == cf || IDWriteFontFamily_GetFontCount(fam) == IDWriteFontFamily_GetFontCount(cf)); }
    DW_RELEASE(cf); DW_RELEASE(fam); fo_close(&x);
}
TEST(font_family_font_count_matches_collection_members) {
    ttf_spec s[3]; fo_fx x; int i; IDWriteFontFamily *fam = NULL;
    ttf_style_spec(&s[0], "FoFamCnt", 400, 5, 0); ttf_style_spec(&s[1], "FoFamCnt", 700, 5, 0); ttf_style_spec(&s[2], "FoFamCnt", 400, 5, 1);
    FO_OPEN(x, s, 3);
    for (i = 0; i < 3; i++) {
        IDWriteFont_GetFontFamily(x.font[i], &fam);
        if (fam) { UINT32 c = IDWriteFontFamily_GetFontCount(fam); CHECKF(c >= 3, "family font count %u", c); if (c != 3) t_note("family of 3 real faces reports %u fonts", c); DW_RELEASE(fam); }
    }
    {   IDWriteFontFamily *f0 = NULL, *f1 = NULL; IDWriteFont_GetFontFamily(x.font[0], &f0); IDWriteFont_GetFontFamily(x.font[1], &f1); CHECK(f0 == f1); DW_RELEASE(f0); DW_RELEASE(f1); }
    fo_close(&x);
}
TEST(font_family_system_arial_name) {
    FO_SYS_REG(font, L"Arial");
    IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *n = NULL; char buf[64];
    CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &fam));
    if (fam) { IDWriteFontFamily_GetFamilyNames(fam, &n); if (n) { dw_ls_en(n, buf, sizeof buf); EXPECT_STR(buf, "Arial"); } DW_RELEASE(n); DW_RELEASE(fam); }
    DW_RELEASE(font);
}
TEST(font_family_system_roundtrip_equals) {
    FO_SYS_REG(font, L"Segoe UI");
    IDWriteFontFamily *fam = NULL; IDWriteFont *again = NULL; IDWriteFont3 *f3 = NULL, *a3 = NULL;
    IDWriteFont_GetFontFamily(font, &fam);
    if (fam) {
        IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &again);
        IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&f3);
        if (again) IDWriteFont_QueryInterface(again, &IID_IDWriteFont3, (void **)&a3);
        if (f3 && again) CHECK(IDWriteFont3_Equals(f3, again));
    }
    DW_RELEASE(a3); DW_RELEASE(f3); DW_RELEASE(again); DW_RELEASE(fam); DW_RELEASE(font);
}

/* ---- weight / stretch / style ------------------------------------------------------------------------- */
TEST(font_weight_ladder_matches_os2) {
    /* usWeightClass 100..900 is reported unchanged. */
    ttf_spec s[9]; fo_fx x; int i;
    for (i = 0; i < 9; i++) { char fam[32]; snprintf(fam, sizeof fam, "FoWt%d", (i + 1) * 100); ttf_style_spec(&s[i], fam, (i + 1) * 100, 5, 0); }
    FO_OPEN(x, s, 9);
    for (i = 0; i < 9; i++) {
        int w = (int)IDWriteFont_GetWeight(x.font[i]);
        CHECKF(w == (i + 1) * 100, "font %d weight %d", i, w);
        EXPECT_EQ(IDWriteFont_GetStretch(x.font[i]), DWRITE_FONT_STRETCH_NORMAL);
        EXPECT_EQ(IDWriteFont_GetStyle(x.font[i]), DWRITE_FONT_STYLE_NORMAL);
    }
    fo_close(&x);
}
TEST(font_weight_enum_constants) {
    CHECK(DWRITE_FONT_WEIGHT_THIN == 100 && DWRITE_FONT_WEIGHT_EXTRA_LIGHT == 200 && DWRITE_FONT_WEIGHT_LIGHT == 300 && DWRITE_FONT_WEIGHT_SEMI_LIGHT == 350 &&
          DWRITE_FONT_WEIGHT_NORMAL == 400 && DWRITE_FONT_WEIGHT_MEDIUM == 500 && DWRITE_FONT_WEIGHT_DEMI_BOLD == 600 && DWRITE_FONT_WEIGHT_BOLD == 700 &&
          DWRITE_FONT_WEIGHT_EXTRA_BOLD == 800 && DWRITE_FONT_WEIGHT_BLACK == 900 && DWRITE_FONT_WEIGHT_EXTRA_BLACK == 950);
}
TEST(font_weight_intermediate_values_note) {
    static const int w[] = { 1, 50, 150, 250, 350, 450, 550, 650, 750, 850, 950, 999, 1000 };
    ttf_spec s[10]; fo_fx x; int i;
    for (i = 0; i < 10; i++) { char fam[32]; snprintf(fam, sizeof fam, "FoWtI%d", w[i]); ttf_style_spec(&s[i], fam, w[i], 5, 0); }
    FO_OPEN(x, s, 10);
    for (i = 0; i < 10; i++) t_note("OS/2 usWeightClass %d -> IDWriteFont::GetWeight %d", w[i], (int)IDWriteFont_GetWeight(x.font[i]));
    fo_close(&x);
}
TEST(font_stretch_ladder_matches_os2) {
    /* usWidthClass 1..9 maps to DWRITE_FONT_STRETCH 1..9. */
    ttf_spec s[9]; fo_fx x; int i;
    for (i = 0; i < 9; i++) { char fam[32]; snprintf(fam, sizeof fam, "FoSt%d", i + 1); ttf_style_spec(&s[i], fam, 400, i + 1, 0); }
    FO_OPEN(x, s, 9);
    for (i = 0; i < 9; i++) {
        int st = (int)IDWriteFont_GetStretch(x.font[i]);
        CHECKF(st == i + 1, "font %d stretch %d", i, st);
        EXPECT_EQ(IDWriteFont_GetWeight(x.font[i]), DWRITE_FONT_WEIGHT_NORMAL);
    }
    fo_close(&x);
}
TEST(font_stretch_enum_constants) {
    CHECK(DWRITE_FONT_STRETCH_UNDEFINED == 0 && DWRITE_FONT_STRETCH_ULTRA_CONDENSED == 1 && DWRITE_FONT_STRETCH_EXTRA_CONDENSED == 2 && DWRITE_FONT_STRETCH_CONDENSED == 3 &&
          DWRITE_FONT_STRETCH_SEMI_CONDENSED == 4 && DWRITE_FONT_STRETCH_NORMAL == 5 && DWRITE_FONT_STRETCH_MEDIUM == 5 && DWRITE_FONT_STRETCH_SEMI_EXPANDED == 6 &&
          DWRITE_FONT_STRETCH_EXPANDED == 7 && DWRITE_FONT_STRETCH_EXTRA_EXPANDED == 8 && DWRITE_FONT_STRETCH_ULTRA_EXPANDED == 9);
    CHECK(DWRITE_FONT_STYLE_NORMAL == 0 && DWRITE_FONT_STYLE_OBLIQUE == 1 && DWRITE_FONT_STYLE_ITALIC == 2);
    CHECK(DWRITE_FONT_SIMULATIONS_NONE == 0 && DWRITE_FONT_SIMULATIONS_BOLD == 1 && DWRITE_FONT_SIMULATIONS_OBLIQUE == 2);
}
TEST(font_stretch_out_of_range_width_class_note) {
    static const int w[] = { 0, 10, 11, 65535 };
    ttf_spec s[4]; fo_fx x; int i;
    for (i = 0; i < 4; i++) { char fam[32]; snprintf(fam, sizeof fam, "FoStO%d", w[i]); ttf_style_spec(&s[i], fam, 400, 5, 0); s[i].width = w[i]; }
    FO_OPEN(x, s, 4);
    for (i = 0; i < 4; i++) t_note("OS/2 usWidthClass %d -> GetStretch %d", w[i], (int)IDWriteFont_GetStretch(x.font[i]));
    fo_close(&x);
}
TEST(font_style_italic_bit_gives_italic) {
    /* fsSelection ITALIC (bit 0) + 'Italic' naming => DWRITE_FONT_STYLE_ITALIC; regular => NORMAL. */
    ttf_spec s[4]; fo_fx x;
    ttf_style_spec(&s[0], "FoSty", 400, 5, 0); ttf_style_spec(&s[1], "FoSty", 400, 5, 1);
    ttf_style_spec(&s[2], "FoSty", 700, 5, 0); ttf_style_spec(&s[3], "FoSty", 700, 5, 1);
    FO_OPEN(x, s, 4);
    EXPECT_EQ(IDWriteFont_GetStyle(x.font[0]), DWRITE_FONT_STYLE_NORMAL);
    EXPECT_EQ(IDWriteFont_GetStyle(x.font[1]), DWRITE_FONT_STYLE_ITALIC);
    EXPECT_EQ(IDWriteFont_GetStyle(x.font[2]), DWRITE_FONT_STYLE_NORMAL);
    EXPECT_EQ(IDWriteFont_GetStyle(x.font[3]), DWRITE_FONT_STYLE_ITALIC);
    EXPECT_EQ(IDWriteFont_GetWeight(x.font[0]), 400); EXPECT_EQ(IDWriteFont_GetWeight(x.font[1]), 400);
    EXPECT_EQ(IDWriteFont_GetWeight(x.font[2]), 700); EXPECT_EQ(IDWriteFont_GetWeight(x.font[3]), 700);
    fo_close(&x);
}
TEST(font_style_oblique_naming_note) {
    ttf_spec s[2]; fo_fx x;
    ttf_style_spec(&s[0], "FoObl", 400, 5, 1);
    ttf_style_spec(&s[1], "FoObl2", 400, 5, 1);
    ttf_clear_names(&s[1]); ttf_set_names(&s[1], "FoObl2", "Oblique", "FoObl2 Oblique", "FoObl2-Oblique");
    FO_OPEN(x, s, 2);
    t_note("italic-bit font -> style %d; 'Oblique'-named font with italic bit -> style %d", (int)IDWriteFont_GetStyle(x.font[0]), (int)IDWriteFont_GetStyle(x.font[1]));
    fo_close(&x);
}
TEST(font_all_getters_agree_across_versions) {
    ttf_spec s[2]; fo_fx x; int i;
    ttf_style_spec(&s[0], "FoAgree", 700, 3, 1); ttf_style_spec(&s[1], "FoAgree", 300, 7, 0);
    FO_OPEN(x, s, 2);
    for (i = 0; i < 2; i++) {
        IDWriteFont1 *f1 = NULL; IDWriteFont2 *f2 = NULL; IDWriteFont3 *f3 = NULL;
        IDWriteFont_QueryInterface(x.font[i], &IID_IDWriteFont1, (void **)&f1); IDWriteFont_QueryInterface(x.font[i], &IID_IDWriteFont2, (void **)&f2);
        IDWriteFont_QueryInterface(x.font[i], &IID_IDWriteFont3, (void **)&f3);
        if (f1 && f2 && f3) {
            EXPECT_EQ(IDWriteFont1_GetWeight(f1), IDWriteFont_GetWeight(x.font[i])); EXPECT_EQ(IDWriteFont2_GetWeight(f2), IDWriteFont_GetWeight(x.font[i]));
            EXPECT_EQ(IDWriteFont3_GetWeight(f3), IDWriteFont_GetWeight(x.font[i]));
            EXPECT_EQ(IDWriteFont3_GetStretch(f3), IDWriteFont_GetStretch(x.font[i])); EXPECT_EQ(IDWriteFont2_GetStyle(f2), IDWriteFont_GetStyle(x.font[i]));
            EXPECT_EQ(IDWriteFont1_GetSimulations(f1), IDWriteFont_GetSimulations(x.font[i]));
            EXPECT_EQ(IDWriteFont3_IsSymbolFont(f3), IDWriteFont_IsSymbolFont(x.font[i]));
        } else CHECK(0);
        DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(f3);
    }
    EXPECT_EQ(IDWriteFont_GetStretch(x.font[0]), 3); EXPECT_EQ(IDWriteFont_GetStretch(x.font[1]), 7);
    fo_close(&x);
}
TEST(font_default_synthetic_attributes) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoDefault"); FO_OPEN1(x, s);
    IDWriteFont *f = x.font[0];
    EXPECT_EQ(IDWriteFont_GetWeight(f), DWRITE_FONT_WEIGHT_NORMAL); EXPECT_EQ(IDWriteFont_GetStretch(f), DWRITE_FONT_STRETCH_NORMAL);
    EXPECT_EQ(IDWriteFont_GetStyle(f), DWRITE_FONT_STYLE_NORMAL); EXPECT_EQ(IDWriteFont_GetSimulations(f), DWRITE_FONT_SIMULATIONS_NONE);
    EXPECT_EQ(IDWriteFont_IsSymbolFont(f), FALSE);
    fo_close(&x);
}
TEST(font_getters_repeatable) {
    fo_fx x; ttf_spec s; int i; fo_spec(&s, "FoRepeat"); FO_OPEN1(x, s);
    for (i = 0; i < 50; i++) {
        EXPECT_EQ(IDWriteFont_GetWeight(x.font[0]), 400); EXPECT_EQ(IDWriteFont_GetStretch(x.font[0]), 5);
        EXPECT_EQ(IDWriteFont_GetStyle(x.font[0]), 0); EXPECT_EQ(IDWriteFont_GetSimulations(x.font[0]), 0);
    }
    fo_close(&x);
}

/* ---- simulations ----------------------------------------------------------------------------------------- */
static IDWriteFont *sim_font(fo_fx *x, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFontFamily *fam = NULL; IDWriteFont *f = NULL;
    IDWriteFont_GetFontFamily(x->font[0], &fam);
    if (fam) { if (FAILED(IDWriteFontFamily_GetFirstMatchingFont(fam, w, DWRITE_FONT_STRETCH_NORMAL, st, &f))) f = NULL; IDWriteFontFamily_Release(fam); }
    return f;
}
TEST(font_simulated_bold_from_regular_only_family) {
    /* FindNearestMatchingWwsFont yields simulation flags: asking Bold from a Regular-only family simulates bold. */
    fo_fx x; ttf_spec s; IDWriteFont *b; fo_spec(&s, "FoSimB"); FO_OPEN1(x, s);
    b = sim_font(&x, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL); CHECK(b != NULL);
    if (b) {
        CHECK(IDWriteFont_GetSimulations(b) & DWRITE_FONT_SIMULATIONS_BOLD);
        EXPECT_EQ(IDWriteFont_GetWeight(b), DWRITE_FONT_WEIGHT_BOLD);   /* GetFontWeight: BOLD simulation => 700 */
        EXPECT_EQ(IDWriteFont_GetStyle(b), DWRITE_FONT_STYLE_NORMAL);
        DW_RELEASE(b);
    }
    fo_close(&x);
}
TEST(font_simulated_oblique_from_upright_family) {
    fo_fx x; ttf_spec s; IDWriteFont *o; fo_spec(&s, "FoSimO"); FO_OPEN1(x, s);
    o = sim_font(&x, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC); CHECK(o != NULL);
    if (o) {
        CHECK(IDWriteFont_GetSimulations(o) & DWRITE_FONT_SIMULATIONS_OBLIQUE);
        EXPECT_EQ(IDWriteFont_GetStyle(o), DWRITE_FONT_STYLE_OBLIQUE);  /* GetFontStyle: OBLIQUE simulation => 1 */
        EXPECT_EQ(IDWriteFont_GetWeight(o), DWRITE_FONT_WEIGHT_NORMAL);
        DW_RELEASE(o);
    }
    fo_close(&x);
}
TEST(font_simulated_bold_oblique) {
    fo_fx x; ttf_spec s; IDWriteFont *o; fo_spec(&s, "FoSimBO"); FO_OPEN1(x, s);
    o = sim_font(&x, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_OBLIQUE); CHECK(o != NULL);
    if (o) {
        DWRITE_FONT_SIMULATIONS sim = IDWriteFont_GetSimulations(o);
        CHECKF(sim == (DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE), "simulations %d", (int)sim);
        EXPECT_EQ(IDWriteFont_GetWeight(o), 700); EXPECT_EQ(IDWriteFont_GetStyle(o), DWRITE_FONT_STYLE_OBLIQUE);
        DW_RELEASE(o);
    }
    fo_close(&x);
}
TEST(font_simulation_flags_only_defined_bits) {
    fo_fx x; ttf_spec s; IDWriteFont *f; int i; fo_spec(&s, "FoSimBits"); FO_OPEN1(x, s);
    static const int ws[] = { 100, 400, 700, 900 }; static const int ss[] = { 0, 1, 2 };
    for (i = 0; i < 12; i++) {
        f = sim_font(&x, (DWRITE_FONT_WEIGHT)ws[i % 4], (DWRITE_FONT_STYLE)ss[i / 4]);
        if (f) { CHECKF((IDWriteFont_GetSimulations(f) & ~3) == 0, "undefined simulation bits %x", (unsigned)IDWriteFont_GetSimulations(f)); DW_RELEASE(f); }
    }
    fo_close(&x);
}
TEST(font_simulation_not_applied_when_real_face_exists) {
    ttf_spec s[2]; fo_fx x; IDWriteFontFamily *fam = NULL; IDWriteFont *f = NULL;
    ttf_style_spec(&s[0], "FoSimReal", 400, 5, 0); ttf_style_spec(&s[1], "FoSimReal", 700, 5, 0);
    FO_OPEN(x, s, 2);
    IDWriteFont_GetFontFamily(x.font[0], &fam);
    if (fam) {
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &f));
        if (f) { EXPECT_EQ(IDWriteFont_GetSimulations(f), DWRITE_FONT_SIMULATIONS_NONE); EXPECT_EQ(IDWriteFont_GetWeight(f), 700); EXPECT_EQ(IDWriteFont_GetWeight(x.font[1]), 700); DW_RELEASE(f); }
        DW_RELEASE(fam);
    }
    fo_close(&x);
}
TEST(font_simulated_font_same_family) {
    fo_fx x; ttf_spec s; IDWriteFont *b; IDWriteFontFamily *fa = NULL, *fb = NULL; fo_spec(&s, "FoSimFam"); FO_OPEN1(x, s);
    b = sim_font(&x, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    if (b) { IDWriteFont_GetFontFamily(b, &fb); IDWriteFont_GetFontFamily(x.font[0], &fa); CHECK(fa && fa == fb); DW_RELEASE(fa); DW_RELEASE(fb); DW_RELEASE(b); }
    else CHECK(0);
    fo_close(&x);
}
static void ls_first_en(IDWriteLocalizedStrings *n, char *buf, int cap) { buf[0] = 0; if (n) dw_ls_en(n, buf, cap); }
TEST(font_simulated_face_names) {
    /* GetFaceNames: CreateSimulatedBold/ObliqueFaceName remove 'Regular' and append ' Bold' / ' Oblique' (or yield the bare word). */
    fo_fx x; ttf_spec s; IDWriteFont *f; IDWriteLocalizedStrings *n; char buf[64]; fo_spec(&s, "FoSimNm"); FO_OPEN1(x, s);
    static const struct { int w, st; const char *want; } t[] = {
        { 400, 0, "Regular" }, { 700, 0, "Bold" }, { 400, 2, "Oblique" }, { 700, 1, "Bold Oblique" } };
    int i;
    for (i = 0; i < 4; i++) {
        f = sim_font(&x, (DWRITE_FONT_WEIGHT)t[i].w, (DWRITE_FONT_STYLE)t[i].st); n = NULL;
        if (f && SUCCEEDED(IDWriteFont_GetFaceNames(f, &n))) { ls_first_en(n, buf, sizeof buf); CHECKF(!strcmp(buf, t[i].want), "w=%d st=%d face name '%s' want '%s'", t[i].w, t[i].st, buf, t[i].want); }
        else CHECK(0);
        DW_RELEASE(n); DW_RELEASE(f);
    }
    fo_close(&x);
}
TEST(font_simulated_oblique_metrics_shift_glyph_box) {
    /* GetFontMetrics: for an OBLIQUE simulation glyphBoxLeft/Right and sub/superscript X offsets are shifted by (y * 0x5700) >> 16. */
    fo_fx x; ttf_spec s; IDWriteFont *o; IDWriteFont1 *b1 = NULL, *o1 = NULL; DWRITE_FONT_METRICS1 mb, mo; fo_spec(&s, "FoSimMet"); FO_OPEN1(x, s);
    o = sim_font(&x, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont1, (void **)&b1);
    if (o) IDWriteFont_QueryInterface(o, &IID_IDWriteFont1, (void **)&o1);
    if (b1 && o1) {
        memset(&mb, 0, sizeof mb); memset(&mo, 0, sizeof mo);
        FO_GETM1(b1, &mb); FO_GETM1(o1, &mo);
        EXPECT_EQ(mo.designUnitsPerEm, mb.designUnitsPerEm); EXPECT_EQ(mo.ascent, mb.ascent); EXPECT_EQ(mo.descent, mb.descent);
        EXPECT_EQ(mo.glyphBoxTop, mb.glyphBoxTop); EXPECT_EQ(mo.glyphBoxBottom, mb.glyphBoxBottom);
        EXPECT_EQ((UINT16)mo.glyphBoxRight, (UINT16)(mb.glyphBoxRight + ((mb.glyphBoxTop * 0x5700) >> 16)));
        EXPECT_EQ((UINT16)mo.glyphBoxLeft, (UINT16)(mb.glyphBoxLeft + ((mb.glyphBoxBottom * 0x5700) >> 16)));
        EXPECT_EQ((UINT16)mo.subscriptPositionX, (UINT16)(mb.subscriptPositionX + ((mb.subscriptPositionY * 0x5700) >> 16)));
        EXPECT_EQ((UINT16)mo.superscriptPositionX, (UINT16)(mb.superscriptPositionX + ((mb.superscriptPositionY * 0x5700) >> 16)));
        EXPECT_EQ(mo.superscriptPositionY, mb.superscriptPositionY); EXPECT_EQ(mo.subscriptPositionY, mb.subscriptPositionY);
        EXPECT_EQ(mo.underlinePosition, mb.underlinePosition); EXPECT_EQ(mo.strikethroughPosition, mb.strikethroughPosition);
    } else CHECK(0);
    DW_RELEASE(b1); DW_RELEASE(o1); DW_RELEASE(o); fo_close(&x);
}
TEST(font_simulated_bold_metrics_unchanged) {
    fo_fx x; ttf_spec s; IDWriteFont *b; DWRITE_FONT_METRICS ma, mb; fo_spec(&s, "FoSimMetB"); FO_OPEN1(x, s);
    b = sim_font(&x, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    if (b) { IDWriteFont_GetMetrics(x.font[0], &ma); IDWriteFont_GetMetrics(b, &mb); CHECK(!memcmp(&ma, &mb, sizeof ma)); DW_RELEASE(b); } else CHECK(0);
    fo_close(&x);
}
TEST(font_simulated_font_has_character_like_base) {
    fo_fx x; ttf_spec s; IDWriteFont *b; BOOL a = FALSE, c = FALSE; fo_spec(&s, "FoSimHas"); FO_OPEN1(x, s);
    b = sim_font(&x, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC);
    if (b) { CHECK_HR_OK(IDWriteFont_HasCharacter(b, 'A', &a)); CHECK_HR_OK(IDWriteFont_HasCharacter(b, 0x4E00, &c)); CHECK(a); CHECK(!c); DW_RELEASE(b); } else CHECK(0);
    fo_close(&x);
}
TEST(font_simulated_font_not_equal_to_base) {
    fo_fx x; ttf_spec s; IDWriteFont *b; IDWriteFont3 *f3 = NULL; fo_spec(&s, "FoSimEq"); FO_OPEN1(x, s);
    b = sim_font(&x, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont3, (void **)&f3);
    if (b && f3) {
        /* the font face key includes the simulation flags, so a simulated font is a different face */
        CHECK(!IDWriteFont3_Equals(f3, b));
        CHECK(IDWriteFont3_Equals(f3, x.font[0]));
    } else CHECK(0);
    DW_RELEASE(f3); DW_RELEASE(b); fo_close(&x);
}
TEST(font_simulated_bold_face_has_simulation) {
    fo_fx x; ttf_spec s; IDWriteFont *b; IDWriteFontFace *face = NULL; fo_spec(&s, "FoSimFace"); FO_OPEN1(x, s);
    b = sim_font(&x, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC);
    if (b) { CHECK_HR_OK(IDWriteFont_CreateFontFace(b, &face)); if (face) { EXPECT_EQ(IDWriteFontFace_GetSimulations(face), IDWriteFont_GetSimulations(b)); DW_RELEASE(face); } DW_RELEASE(b); } else CHECK(0);
    fo_close(&x);
}

/* ---- system fonts ------------------------------------------------------------------------------------- */
TEST(font_system_arial_regular_attributes) {
    FO_SYS_REG(f, L"Arial");
    EXPECT_EQ(IDWriteFont_GetWeight(f), 400); EXPECT_EQ(IDWriteFont_GetStretch(f), 5); EXPECT_EQ(IDWriteFont_GetStyle(f), 0);
    EXPECT_EQ(IDWriteFont_GetSimulations(f), 0);
    DW_RELEASE(f);
}
TEST(font_system_arial_bold_italic_attributes) {
    FO_SYS(b, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    FO_SYS(i, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC);
    FO_SYS(bi, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC);
    EXPECT_EQ(IDWriteFont_GetWeight(b), 700); EXPECT_EQ(IDWriteFont_GetStyle(b), 0); EXPECT_EQ(IDWriteFont_GetSimulations(b), 0);
    EXPECT_EQ(IDWriteFont_GetWeight(i), 400); EXPECT_EQ(IDWriteFont_GetStyle(i), 2); EXPECT_EQ(IDWriteFont_GetSimulations(i), 0);
    EXPECT_EQ(IDWriteFont_GetWeight(bi), 700); EXPECT_EQ(IDWriteFont_GetStyle(bi), 2); EXPECT_EQ(IDWriteFont_GetSimulations(bi), 0);
    DW_RELEASE(b); DW_RELEASE(i); DW_RELEASE(bi);
}
TEST(font_system_segoe_ui_weights) {
    /* Segoe UI ships Light, Semilight, Regular, Semibold, Bold, Black faces; Light/Semibold are real faces (no simulation). */
    static const struct { int req; int want; } t[] = { { 300, 300 }, { 400, 400 }, { 600, 600 }, { 700, 700 } };
    int k;
    for (k = 0; k < 4; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), L"Segoe UI", (DWRITE_FONT_WEIGHT)t[k].req, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!f) { t_skip("Segoe UI not installed"); return; }
        EXPECT_EQ(IDWriteFont_GetWeight(f), t[k].want); EXPECT_EQ(IDWriteFont_GetSimulations(f), 0);
        DW_RELEASE(f);
    }
    {   IDWriteFont *f = fo_sysfont(dw_shared(), L"Segoe UI", DWRITE_FONT_WEIGHT_SEMI_LIGHT, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (f) { t_note("Segoe UI semilight request -> weight %d", (int)IDWriteFont_GetWeight(f)); DW_RELEASE(f); } }
}
TEST(font_system_times_courier_tahoma_attributes) {
    static const WCHAR *fams[] = { L"Times New Roman", L"Courier New", L"Tahoma", L"Verdana", L"Calibri", L"Consolas" };
    int k;
    for (k = 0; k < 6; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!f) { t_note("%ls not installed", fams[k]); continue; }
        EXPECT_EQ(IDWriteFont_GetWeight(f), 400); EXPECT_EQ(IDWriteFont_GetStretch(f), 5); EXPECT_EQ(IDWriteFont_GetStyle(f), 0);
        DW_RELEASE(f);
    }
}
TEST(font_system_attributes_match_os2_table) {
    /* usWeightClass of the file's OS/2 table equals GetWeight for real (non-simulated) regular faces */
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Courier New", L"Tahoma" };
    int k;
    for (k = 0; k < 4; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        uint8_t *os2; UINT32 len;
        if (!f) continue;
        if (fo_sys_table(f, "OS/2", &os2, &len) && len >= 8) {
            int wt = (os2[4] << 8) | os2[5], wd = (os2[6] << 8) | os2[7];
            EXPECT_EQ(IDWriteFont_GetWeight(f), wt); EXPECT_EQ(IDWriteFont_GetStretch(f), wd);
        }
        free(os2); DW_RELEASE(f);
    }
}
TEST(font_system_bahnschrift_variable_note) {
    FO_SYS_REG(f, L"Bahnschrift");
    t_note("Bahnschrift regular request: weight %d stretch %d style %d simulations %d", (int)IDWriteFont_GetWeight(f), (int)IDWriteFont_GetStretch(f), (int)IDWriteFont_GetStyle(f), (int)IDWriteFont_GetSimulations(f));
    EXPECT_EQ(IDWriteFont_GetSimulations(f), 0);
    DW_RELEASE(f);
}
TEST(font_system_bold_wider_than_regular_design) {
    FO_SYS_REG(r, L"Arial"); FO_SYS(b, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    CHECK(IDWriteFont_GetWeight(b) > IDWriteFont_GetWeight(r));
    DW_RELEASE(r); DW_RELEASE(b);
}

/* ---- classification flags ----------------------------------------------------------------------------- */
TEST(font_is_symbol_from_cmap_3_0) {
    /* IsSymbolFont(OpenTypeCharacterMap) is true when the selected cmap subtable is the (3,0) symbol map. */
    fo_fx x; ttf_spec s; fo_symbol_spec(&s, "FoSym30"); FO_OPEN1(x, s);
    CHECK(IDWriteFont_IsSymbolFont(x.font[0]) == TRUE);
    fo_close(&x);
}
TEST(font_is_symbol_false_for_unicode_cmap) {
    fo_fx x; ttf_spec s; fo_spec(&s, "FoSymNo"); FO_OPEN1(x, s);
    EXPECT_EQ(IDWriteFont_IsSymbolFont(x.font[0]), FALSE);
    fo_close(&x);
}
TEST(font_is_symbol_from_panose_family_5) {
    /* ... or when the OS/2 PANOSE bFamilyType is 5 (Latin Symbol / pictorial), whatever the cmap. */
    fo_fx x; ttf_spec s; uint8_t p[10] = { 5, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; fo_spec(&s, "FoSymPan"); fo_set_panose(&s, p); FO_OPEN1(x, s);
    CHECK(IDWriteFont_IsSymbolFont(x.font[0]) == TRUE);
    fo_close(&x);
}
TEST(font_is_symbol_panose_other_families_false) {
    fo_fx x; ttf_spec s[5]; int i; uint8_t p[10] = { 0 };
    for (i = 0; i < 5; i++) { char fam[32]; snprintf(fam, sizeof fam, "FoSymPf%d", i); fo_spec(&s[i], fam); p[0] = (uint8_t)(i == 4 ? 6 : i); fo_set_panose(&s[i], p); }
    FO_OPEN(x, s, 5);
    for (i = 0; i < 5; i++) CHECKF(IDWriteFont_IsSymbolFont(x.font[i]) == FALSE, "panose family %d", i == 4 ? 6 : i);
    fo_close(&x);
}
TEST(font_is_symbol_returns_boolean) {
    fo_fx x; ttf_spec s[2]; fo_symbol_spec(&s[0], "FoSymB1"); fo_spec(&s[1], "FoSymB2"); FO_OPEN(x, s, 2);
    EXPECT_EQ(IDWriteFont_IsSymbolFont(x.font[0]), 1); EXPECT_EQ(IDWriteFont_IsSymbolFont(x.font[1]), 0);
    fo_close(&x);
}
TEST(font_is_symbol_system_symbol_fonts) {
    static const WCHAR *fams[] = { L"Marlett", L"Wingdings", L"Symbol", L"Webdings" };
    int k, seen = 0;
    for (k = 0; k < 4; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!f) { t_note("%ls not installed", fams[k]); continue; }
        seen++; CHECKF(IDWriteFont_IsSymbolFont(f) == TRUE, "%ls", fams[k]); DW_RELEASE(f);
    }
    if (!seen) t_skip("no symbol system font installed");
}
TEST(font_is_symbol_system_text_fonts_false) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Segoe UI", L"Courier New", L"Tahoma", L"Segoe UI Symbol", L"Segoe UI Emoji" };
    int k, seen = 0;
    for (k = 0; k < 7; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!f) continue;
        seen++; CHECKF(IDWriteFont_IsSymbolFont(f) == FALSE, "%ls", fams[k]); DW_RELEASE(f);
    }
    if (!seen) t_skip("no text system font installed");
}
TEST(font_is_symbol_matches_face) {
    fo_fx x; ttf_spec s[2]; IDWriteFontFace *face = NULL; int i; fo_symbol_spec(&s[0], "FoSymF1"); fo_spec(&s[1], "FoSymF2"); FO_OPEN(x, s, 2);
    for (i = 0; i < 2; i++) {
        IDWriteFont_CreateFontFace(x.font[i], &face);
        if (face) { EXPECT_EQ(IDWriteFontFace_IsSymbolFont(face), IDWriteFont_IsSymbolFont(x.font[i])); DW_RELEASE(face); } else CHECK(0);
    }
    fo_close(&x);
}
