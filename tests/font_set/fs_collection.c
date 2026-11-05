/* Font sets and font collections: IDWriteFactory3::CreateFontCollectionFromFontSet, IDWriteFactory6 model variant,
 * IDWriteFontCollection1::GetFontSet round trips, family grouping, and the faces / resources reachable from a set.
 *
 * Win10 decompilation:
 *  - DWriteFactory::CreateFontCollectionFromFontSet(set, model, out): out zeroed, NULL set -> E_INVALIDARG, otherwise a new
 *    DWriteFontCollection over a copy of the set's FontSet; the Factory3 flavour passes model 1 (WEIGHT_STRETCH_STYLE).
 *  - DWriteFontCollection::GetFontSet builds a DWriteFontSet over the collection's own FontSet (same items as the source set).
 *  - GetFontFamily(index >= count) throws IndexOutOfRangeException (HRESULT 0x80004005); out zeroed first.
 *  - FindFamilyName writes index = 0xFFFFFFFF / exists = FALSE first and always returns S_OK.
 *  - In the WWS model every family font list gets simulated faces added for completeness (AddSimulationsForWwsCompleteness);
 *    in the typographic model it does not.
 */
#include "fs.h"

static IDWriteFontCollection1 *coll_from(fs_fx *x, IDWriteFontSet *set, HRESULT *phr)
{
    IDWriteFontCollection1 *c = NULL;
    HRESULT hr = IDWriteFactory3_CreateFontCollectionFromFontSet(x->f3, set, &c);
    if (phr) *phr = hr;
    return c;
}
static HRESULT fam_name(IDWriteFontCollection *c, UINT32 i, char *buf, int cap)
{
    IDWriteFontFamily *f = NULL; IDWriteLocalizedStrings *n = NULL; HRESULT hr;
    buf[0] = 0;
    hr = IDWriteFontCollection_GetFontFamily(c, i, &f);
    if (FAILED(hr) || !f) return FAILED(hr) ? hr : E_FAIL;
    hr = IDWriteFontFamily_GetFamilyNames(f, &n);
    if (SUCCEEDED(hr) && n) hr = dw_ls_en(n, buf, cap);
    DW_RELEASE(n); DW_RELEASE(f);
    return hr;
}
static UINT32 find_fam(IDWriteFontCollection *c, const WCHAR *name, BOOL *pex)
{
    UINT32 idx = 0x1234; BOOL ex = 2;
    HRESULT hr = IDWriteFontCollection_FindFamilyName(c, name, &idx, &ex);
    if (pex) *pex = SUCCEEDED(hr) ? ex : 2;
    return idx;
}

TEST(fset_c_from_set_basic)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    CHECK_HR_OK(hr);
    CHECK(c != NULL);
    if (c) {
        EXPECT_REFS(c, 1);
        dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontCollection, 1), DW_QI_ROW(IDWriteFontCollection1, 1),
                             DW_QI_ROW(IDWriteFontCollection2, -1), DW_QI_ROW(IDWriteFontCollection3, -1),
                             DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFontList, 0), DW_QI_ROW(IDWriteFactory, 0),
                             DW_QI_ROW(IDWriteFontFamily, 0) };
        DW_CHECK_QI(c, rows);
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_from_set_null)
{
    FS_OPEN(x);
    if (t_is_wine()) { t_skip("Wine dereferences the NULL set"); fs_close(&x); return; }
    IDWriteFontCollection1 *c = (IDWriteFontCollection1 *)FS_SENT;
    HRESULT hr = IDWriteFactory3_CreateFontCollectionFromFontSet(x.f3, NULL, &c);
    CHECK_HR(hr, E_INVALIDARG);
    CHECK_OUT_NULL(c);
    FS_DROP(c, hr);
    fs_close(&x);
}

TEST(fset_c_from_set_distinct_objects)
{
    FS_STD(s);
    HRESULT h1, h2; IDWriteFontCollection1 *a = coll_from(&s.x, s.set, &h1), *b = coll_from(&s.x, s.set, &h2);
    CHECK_HR_OK(h1); CHECK_HR_OK(h2);
    CHECK(a != NULL && b != NULL && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    fs_std_close(&s);
}

TEST(fset_c_from_empty_set)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSet *e = NULL;
    if (b && SUCCEEDED(IDWriteFontSetBuilder_CreateFontSet(b, &e)) && e) {
        HRESULT hr; IDWriteFontCollection1 *c = coll_from(&x, e, &hr);
        CHECK_HR_OK(hr);
        if (c) {
            IDWriteFontCollection *base = (IDWriteFontCollection *)c;
            CHECK(IDWriteFontCollection_GetFontFamilyCount(base) == 0);
            BOOL ex = 5; UINT32 idx = find_fam(base, L"Anything", &ex);
            CHECK(ex == FALSE && idx == 0xFFFFFFFFu);
            IDWriteFontFamily *f = (IDWriteFontFamily *)FS_SENT;
            hr = IDWriteFontCollection_GetFontFamily(base, 0, &f);
            CHECKF(hr == E_FAIL, "GetFontFamily(0) of an empty collection -> %s", dw_hr_name(hr));
            CHECK_OUT_NULL(f);
            FS_DROP(f, hr);
            DW_RELEASE(c);
        }
    }
    DW_RELEASE(e); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_c_family_count_and_names)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        UINT32 n = IDWriteFontCollection_GetFontFamilyCount(base), i; int a = 0, b = 0;
        CHECKF(n == 2, "family count %u", n);
        for (i = 0; i < n; i++) {
            char nm[64];
            CHECK_HR_OK(fam_name(base, i, nm, sizeof nm));
            if (!strcmp(nm, "SynthA")) a++; else if (!strcmp(nm, "SynthB")) b++; else CHECKF(0, "unexpected family \"%s\"", nm);
        }
        CHECK(a == 1 && b == 1);
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_find_family_name)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        const WCHAR *hit[] = { L"SynthA", L"SynthB", L"synthA", L"SYNTHB" };
        const WCHAR *miss[] = { L"SynthC", L"", L"Synth", L"SynthA ", L"SynthA Bold" };
        unsigned i;
        for (i = 0; i < 4; i++) {
            BOOL ex = 2; UINT32 idx = find_fam(base, hit[i], &ex);
            CHECKF(ex == TRUE && idx < 2, "%ls: exists=%d idx=%u", hit[i], ex, idx);
        }
        for (i = 0; i < 5; i++) {
            BOOL ex = 2; UINT32 idx = find_fam(base, miss[i], &ex);
            CHECKF(ex == FALSE && idx == 0xFFFFFFFFu, "%ls: exists=%d idx=%u", miss[i], ex, idx);
        }
        /* the index found names the same family */
        BOOL ex; UINT32 ia = find_fam(base, L"SynthA", &ex); char nm[64];
        if (ia < 2) { CHECK_HR_OK(fam_name(base, ia, nm, sizeof nm)); CHECK(!strcmp(nm, "SynthA")); }
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_family_index_out_of_range)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        UINT32 bad[] = { 2, 3, 100, 0x7FFFFFFFu, 0xFFFFFFFFu }; unsigned i;
        for (i = 0; i < 5; i++) {
            IDWriteFontFamily *f = (IDWriteFontFamily *)FS_SENT;
            hr = IDWriteFontCollection_GetFontFamily(base, bad[i], &f);
            CHECKF(hr == E_FAIL, "index %u -> %s", bad[i], dw_hr_name(hr));   /* IndexOutOfRangeException */
            CHECK_OUT_NULL(f);
            FS_DROP(f, hr);
        }
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_family_fonts_synth_a)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        BOOL ex; UINT32 ia = find_fam(base, L"SynthA", &ex); IDWriteFontFamily *fam = NULL;
        if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(base, ia, &fam)) && fam) {
            UINT32 n = IDWriteFontFamily_GetFontCount(fam), i; int seen_bold = 0, seen_regular = 0, seen_italic = 0, seen_bi = 0;
            CHECKF(n >= 4, "font count %u", n);
            for (i = 0; i < n; i++) {
                IDWriteFont *f = NULL;
                CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, i, &f));
                if (!f) continue;
                int bold = IDWriteFont_GetWeight(f) >= 700, ital = IDWriteFont_GetStyle(f) == DWRITE_FONT_STYLE_ITALIC;
                DWRITE_FONT_SIMULATIONS sim = IDWriteFont_GetSimulations(f);
                if (sim == 0) { if (bold && ital) seen_bi++; else if (bold) seen_bold++; else if (ital) seen_italic++; else seen_regular++; }
                CHECK(IDWriteFont_GetStretch(f) == DWRITE_FONT_STRETCH_NORMAL);
                DW_RELEASE(f);
            }
            CHECKF(seen_regular >= 1 && seen_bold >= 1 && seen_italic >= 1 && seen_bi >= 1, "real faces: R=%d B=%d I=%d BI=%d", seen_regular, seen_bold, seen_italic, seen_bi);
            t_note("SynthA family font list: %u fonts", n);
            DW_RELEASE(fam);
        }
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_family_regular_only_gets_simulations)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        BOOL ex; UINT32 ib = find_fam(base, L"SynthB", &ex); IDWriteFontFamily *fam = NULL;
        if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(base, ib, &fam)) && fam) {
            /* WWS model: AddSimulationsForWwsCompleteness may add bold / oblique simulated entries to a one-face family */
            UINT32 n = IDWriteFontFamily_GetFontCount(fam), i; int real = 0;
            CHECKF(n >= 1 && n <= 4, "font count %u", n);
            for (i = 0; i < n; i++) {
                IDWriteFont *f = NULL;
                if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, i, &f)) && f) {
                    DWRITE_FONT_SIMULATIONS sim = IDWriteFont_GetSimulations(f);
                    CHECK((sim & ~3u) == 0);
                    if (sim == 0) real++;
                    t_note("SynthB font %u: weight %d style %d simulations %d", i, (int)IDWriteFont_GetWeight(f), (int)IDWriteFont_GetStyle(f), (int)sim);
                    DW_RELEASE(f);
                }
            }
            CHECKF(real == 1, "exactly one non-simulated face, got %d", real);
            DW_RELEASE(fam);
        }
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_family_first_matching_font)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        BOOL ex; UINT32 ia = find_fam(base, L"SynthA", &ex); IDWriteFontFamily *fam = NULL;
        if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(base, ia, &fam)) && fam) {
            IDWriteFont *f = NULL;
            CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &f));
            if (f) { CHECK(IDWriteFont_GetWeight(f) == DWRITE_FONT_WEIGHT_NORMAL && IDWriteFont_GetStyle(f) == DWRITE_FONT_STYLE_NORMAL && IDWriteFont_GetSimulations(f) == 0); DW_RELEASE(f); }
            CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &f));
            if (f) { CHECK(IDWriteFont_GetWeight(f) == DWRITE_FONT_WEIGHT_NORMAL && IDWriteFont_GetStyle(f) == DWRITE_FONT_STYLE_ITALIC && IDWriteFont_GetSimulations(f) == 0); DW_RELEASE(f); }
            CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &f));
            if (f) { FS_SOFT(IDWriteFont_GetWeight(f) == DWRITE_FONT_WEIGHT_BOLD && IDWriteFont_GetStyle(f) == DWRITE_FONT_STYLE_ITALIC && IDWriteFont_GetSimulations(f) == 0); DW_RELEASE(f); }
            DW_RELEASE(fam);
        }
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_font_face_found_in_source_set)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        BOOL ex; UINT32 ia = find_fam(base, L"SynthA", &ex); IDWriteFontFamily *fam = NULL;
        if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(base, ia, &fam)) && fam) {
            IDWriteFont *f = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &f)) && f) {
                IDWriteFontFace *face = NULL;
                if (SUCCEEDED(IDWriteFont_CreateFontFace(f, &face)) && face && IDWriteFont_GetSimulations(f) == 0) {
                    UINT32 idx = 0; BOOL fnd = FALSE; char full[64];
                    CHECK_HR_OK(IDWriteFontSet_FindFontFace(s.set, face, &idx, &fnd));
                    CHECK(fnd == TRUE);
                    if (fnd) { fs_font_prop(s.set, idx, DWRITE_FONT_PROPERTY_ID_FULL_NAME, full, sizeof full); CHECKF(!strcmp(full, "SynthA Bold"), "found \"%s\"", full); }
                }
                DW_RELEASE(face); DW_RELEASE(f);
            }
            DW_RELEASE(fam);
        }
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

/* ---- IDWriteFontCollection1::GetFontSet ------------------------------------------------------------------ */
TEST(fset_c_get_font_set_roundtrip)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontSet *back = (IDWriteFontSet *)FS_SENT;
        hr = IDWriteFontCollection1_GetFontSet(c, &back);
        CHECK_HR_OK(hr);
        CHECK(back != NULL && back != FS_SENT && back != s.set);
        if (SUCCEEDED(hr) && back) {
            UINT32 i, n = IDWriteFontSet_GetFontCount(s.set);
            CHECKF(IDWriteFontSet_GetFontCount(back) == n, "count %u vs %u", IDWriteFontSet_GetFontCount(back), n);
            for (i = 0; i < n; i++) {
                IDWriteFontFaceReference *r = NULL; UINT32 idx = 0; BOOL ex = FALSE;
                if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(s.set, i, &r)) && r) {
                    CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(back, r, &idx, &ex));
                    CHECKF(ex == TRUE, "item %u survives the round trip", i);
                    DW_RELEASE(r);
                }
            }
            for (i = 1; i <= 13; i++) {
                IDWriteStringList *a = fs_props(s.set, (DWRITE_FONT_PROPERTY_ID)i), *b = fs_props(back, (DWRITE_FONT_PROPERTY_ID)i);
                if (a && b) CHECKF(IDWriteStringList_GetCount(a) == IDWriteStringList_GetCount(b), "property %u value counts", i);
                DW_RELEASE(a); DW_RELEASE(b);
            }
        }
        FS_DROP(back, hr);
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_get_font_set_distinct_each_call)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontSet *a = NULL, *b = NULL;
        CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c, &a));
        CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c, &b));
        CHECK(a != NULL && b != NULL && a != b);
        if (a) EXPECT_REFS(a, 1);
        DW_RELEASE(a); DW_RELEASE(b);
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_get_font_set_qi)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontSet *back = NULL;
        CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c, &back));
        if (back) {
            dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontSet, 1), DW_QI_ROW(IDWriteFontSet1, 1), DW_QI_ROW(IDWriteFontSet2, 1),
                                 DW_QI_ROW(IDWriteFontCollection, 0) };
            DW_CHECK_QI(back, rows);
            DW_RELEASE(back);
        }
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_get_font_set_outlives_collection)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    IDWriteFontSet *back = NULL;
    if (c) {
        CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c, &back));
        DW_RELEASE(c);
        if (back) {
            CHECK(IDWriteFontSet_GetFontCount(back) == 5);
            IDWriteStringList *l = fs_props(back, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
            if (l) { CHECK(IDWriteStringList_GetCount(l) == 5); DW_RELEASE(l); }
            DW_RELEASE(back);
        }
    }
    fs_std_close(&s);
}

TEST(fset_c_collection_outlives_set)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    DW_RELEASE(s.set);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        CHECK(IDWriteFontCollection_GetFontFamilyCount(base) == 2);
        BOOL ex; find_fam(base, L"SynthA", &ex);
        CHECK(ex == TRUE);
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_collection_family_names_after_loader_unregistered)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        /* family names live in the set's own string storage, no font file has to be reopened */
        dwx_loader_unregister(s.x.fl);
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        char nm[64];
        CHECK(IDWriteFontCollection_GetFontFamilyCount(base) == 2);
        CHECK_HR_OK(fam_name(base, 0, nm, sizeof nm));
        CHECK(nm[0] != 0);
        BOOL ex; find_fam(base, L"SynthB", &ex);
        CHECK(ex == TRUE);
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

/* ---- IDWriteFactory6::CreateFontCollectionFromFontSet(set, model) ------------------------------------------ */
#define NEED_F6(f6) IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6); \
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }

TEST(fset_c_f6_models)
{
    NEED_F6(f6);
    /* a set built on the isolated fixture factory cannot be passed to a different factory's builder safely: use the shared factory's set */
    NEED_SYSSET(set);
    int model;
    for (model = 0; model <= 1; model++) {
        IDWriteFontCollection2 *c = (IDWriteFontCollection2 *)FS_SENT;
        HRESULT hr = IDWriteFactory6_CreateFontCollectionFromFontSet(f6, set, (DWRITE_FONT_FAMILY_MODEL)model, &c);
        CHECKF(hr == S_OK && c, "model %d -> %s", model, dw_hr_name(hr));
        if (SUCCEEDED(hr) && c) {
            CHECK(c->lpVtbl->GetFontFamilyModel(c) == (DWRITE_FONT_FAMILY_MODEL)model);
            CHECK(IDWriteFontCollection2_GetFontFamilyCount(c) > 10);
            EXPECT_REFS(c, 1);
        }
        FS_DROP(c, hr);
    }
    DW_RELEASE(set);
}

TEST(fset_c_f6_null_set)
{
    NEED_F6(f6);
    if (t_is_wine()) { t_skip("Wine dereferences the NULL set"); return; }
    IDWriteFontCollection2 *c = (IDWriteFontCollection2 *)FS_SENT;
    HRESULT hr = IDWriteFactory6_CreateFontCollectionFromFontSet(f6, NULL, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &c);
    CHECK_HR(hr, E_INVALIDARG);
    CHECK_OUT_NULL(c);
    FS_DROP(c, hr);
}

TEST(fset_c_f3_flavour_uses_wws_model)
{
    NEED_SYSSET(set);
    NEED_F3(f3);
    IDWriteFontCollection1 *c = NULL;
    CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, set, &c));
    if (c) {
        IDWriteFontCollection2 *c2 = NULL;
        if (SUCCEEDED(IDWriteFontCollection1_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2)) && c2) {
            CHECK(c2->lpVtbl->GetFontFamilyModel(c2) == DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
            DW_RELEASE(c2);
        } else t_note("IDWriteFontCollection2 not available on this collection");
        DW_RELEASE(c);
    }
    DW_RELEASE(set);
}

TEST(fset_c_f6_models_family_count_relation)
{
    NEED_F6(f6);
    NEED_SYSSET(set);
    IDWriteFontCollection2 *typo = NULL, *wws = NULL;
    CHECK_HR_OK(IDWriteFactory6_CreateFontCollectionFromFontSet(f6, set, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &typo));
    CHECK_HR_OK(IDWriteFactory6_CreateFontCollectionFromFontSet(f6, set, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &wws));
    if (typo && wws) {
        UINT32 nt = IDWriteFontCollection2_GetFontFamilyCount(typo), nw = IDWriteFontCollection2_GetFontFamilyCount(wws);
        t_note("system set: %u typographic families, %u WWS families", nt, nw);
        CHECK(nt > 10 && nw > 10);
        BOOL ex = FALSE; UINT32 idx = 0;
        if (dw_have_font(dw_shared(), L"Arial")) {
            CHECK_HR_OK(IDWriteFontCollection2_FindFamilyName(typo, L"Arial", &idx, &ex)); CHECK(ex == TRUE);
            CHECK_HR_OK(IDWriteFontCollection2_FindFamilyName(wws, L"Arial", &idx, &ex)); CHECK(ex == TRUE);
        }
    }
    DW_RELEASE(typo); DW_RELEASE(wws);
    DW_RELEASE(set);
}

/* ---- grouping by supplied properties ---------------------------------------------------------------------------- */
TEST(fset_c_grouping_follows_supplied_family_property)
{
    FS_OPEN(x);
    UINT32 k1 = fs_add(&x, "FileNameOne", 400, 5, 0), k2 = fs_add(&x, "FileNameTwo", 400, 5, 0);
    IDWriteFontFaceReference *r1 = fs_ref(&x, k1), *r2 = fs_ref(&x, k2); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r1 && r2 && b) {
        fs_prop p1[2], p2[2]; DWRITE_FONT_PROPERTY a1[2], a2[2]; IDWriteFontSet *set = NULL;
        fs_prop_set(&p1[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Joined Family", "en-us");
        fs_prop_set(&p1[1], DWRITE_FONT_PROPERTY_ID_WEIGHT, "300", NULL);
        fs_prop_set(&p2[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Joined Family", "en-us");
        fs_prop_set(&p2[1], DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL);
        a1[0] = p1[0].p; a1[1] = p1[1].p; a2[0] = p2[0].p; a2[1] = p2[1].p;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r1, a1, 2));
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r2, a2, 2));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            CHECK(IDWriteFontSet_GetFontCount(set) == 2);
            HRESULT hr; IDWriteFontCollection1 *c = coll_from(&x, set, &hr);
            CHECK_HR_OK(hr);
            if (c) {
                IDWriteFontCollection *base = (IDWriteFontCollection *)c; BOOL ex;
                CHECKF(IDWriteFontCollection_GetFontFamilyCount(base) == 1, "families %u", IDWriteFontCollection_GetFontFamilyCount(base));
                find_fam(base, L"Joined Family", &ex); CHECK(ex == TRUE);
                find_fam(base, L"FileNameOne", &ex); CHECK(ex == FALSE);
                IDWriteFontFamily *fam = NULL;
                if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(base, 0, &fam)) && fam) {
                    UINT32 n = IDWriteFontFamily_GetFontCount(fam), i; int w300 = 0, w700 = 0;
                    CHECKF(n >= 2, "fonts %u", n);
                    for (i = 0; i < n; i++) {
                        IDWriteFont *f = NULL;
                        if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, i, &f)) && f) {
                            if (IDWriteFont_GetWeight(f) == 300) w300++;
                            if (IDWriteFont_GetWeight(f) == 700) w700++;
                            DW_RELEASE(f);
                        }
                    }
                    FS_SOFT(w300 >= 1 && w700 >= 1);
                    DW_RELEASE(fam);
                }
                DW_RELEASE(c);
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r1); DW_RELEASE(r2); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_c_font_without_family_property)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "HasNoProps", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, NULL, 0));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            HRESULT hr; IDWriteFontCollection1 *c = coll_from(&x, set, &hr);
            CHECKF(SUCCEEDED(hr), "collection from a property-less set -> %s", dw_hr_name(hr));
            if (c) {
                t_note("property-less font: %u families", IDWriteFontCollection1_GetFontFamilyCount(c));
                DW_RELEASE(c);
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_c_system_subset_to_collection)
{
    NEED_SYSSET(sys);
    if (!dw_have_font(dw_shared(), L"Arial")) { t_skip("Arial not installed"); DW_RELEASE(sys); return; }
    IDWriteFontSet *arial = NULL;
    fs_prop p; fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Arial", "en-us");
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(sys, &p.p, 1, &arial));
    if (arial && IDWriteFontSet_GetFontCount(arial) > 0) {
        NEED_F3(f3);
        IDWriteFontCollection1 *c = NULL;
        CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, arial, &c));
        if (c) {
            IDWriteFontCollection *base = (IDWriteFontCollection *)c; BOOL ex;
            CHECKF(IDWriteFontCollection_GetFontFamilyCount(base) == 1, "families %u", IDWriteFontCollection_GetFontFamilyCount(base));
            find_fam(base, L"Arial", &ex); CHECK(ex == TRUE);
            find_fam(base, L"Times New Roman", &ex); CHECK(ex == FALSE);
            DW_RELEASE(c);
        }
    }
    DW_RELEASE(arial); DW_RELEASE(sys);
}

/* ---- faces and resources reached through a set ------------------------------------------------------------------- */
TEST(fset_c_set_create_font_face_glyphs)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 i, n = IDWriteFontSet1_GetFontCount(s1);
        for (i = 0; i < n; i++) {
            IDWriteFontFace5 *face = NULL;
            HRESULT hr = IDWriteFontSet1_CreateFontFace(s1, i, &face);
            CHECKF(hr == S_OK && face, "CreateFontFace(%u) -> %s", i, dw_hr_name(hr));
            if (!face) continue;
            UINT32 cp[3] = { 'A', 'z', '5' }; UINT16 g[3] = { 0xFFFF, 0xFFFF, 0xFFFF };
            CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices((IDWriteFontFace *)face, cp, 3, g));
            CHECKF(g[0] == 2 && g[1] == 27 + 26 && g[2] == 54 + 5, "glyphs %u %u %u", g[0], g[1], g[2]);
            CHECK(IDWriteFontFace_GetGlyphCount((IDWriteFontFace *)face) == TTF_DEF_NGLYPHS);
            UINT32 idx = 99; BOOL ex = FALSE;
            CHECK_HR_OK(IDWriteFontSet_FindFontFace(s.set, (IDWriteFontFace *)face, &idx, &ex));
            CHECKF(ex == TRUE && idx == i, "face %u found at %u (exists %d)", i, idx, ex);
            DW_RELEASE(face);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_c_set_face_weight_matches_property)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 i, n = IDWriteFontSet1_GetFontCount(s1);
        for (i = 0; i < n; i++) {
            IDWriteFontFace5 *face = NULL; char w[16], st[16];
            if (FAILED(IDWriteFontSet1_CreateFontFace(s1, i, &face)) || !face) continue;
            fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT, w, sizeof w);
            fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_STYLE, st, sizeof st);
            IDWriteFontFace3 *f3 = (IDWriteFontFace3 *)face;
            CHECKF((int)IDWriteFontFace3_GetWeight(f3) == fs_atoi(w), "face %u weight %d vs property %s", i, (int)IDWriteFontFace3_GetWeight(f3), w);
            CHECKF((int)IDWriteFontFace3_GetStyle(f3) == fs_atoi(st), "face %u style %d vs property %s", i, (int)IDWriteFontFace3_GetStyle(f3), st);
            CHECK(IDWriteFontFace3_GetStretch(f3) == DWRITE_FONT_STRETCH_NORMAL);
            DW_RELEASE(face);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_c_set_face_names_match_properties)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 i, n = IDWriteFontSet1_GetFontCount(s1);
        for (i = 0; i < n; i++) {
            IDWriteFontFace5 *face = NULL; IDWriteLocalizedStrings *fam = NULL, *fn = NULL; char a[64], pa[64], b[64], pb[64];
            if (FAILED(IDWriteFontSet1_CreateFontFace(s1, i, &face)) || !face) continue;
            IDWriteFontFace3 *f3 = (IDWriteFontFace3 *)face;
            if (SUCCEEDED(IDWriteFontFace3_GetFamilyNames(f3, &fam)) && fam) {
                dw_ls_en(fam, a, sizeof a);
                fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, pa, sizeof pa);
                CHECKF(!strcmp(a, pa), "face %u family \"%s\" vs property \"%s\"", i, a, pa);
            }
            if (SUCCEEDED(IDWriteFontFace3_GetFaceNames(f3, &fn)) && fn) {
                dw_ls_en(fn, b, sizeof b);
                fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, pb, sizeof pb);
                CHECKF(!strcmp(b, pb), "face %u face name \"%s\" vs property \"%s\"", i, b, pb);
            }
            DW_RELEASE(fam); DW_RELEASE(fn); DW_RELEASE(face);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_c_set_reference_face_roundtrip)
{
    FS_STD(s);
    UINT32 i, n = IDWriteFontSet_GetFontCount(s.set);
    for (i = 0; i < n; i++) {
        IDWriteFontFaceReference *r = NULL; IDWriteFontFace3 *face = NULL;
        if (FAILED(IDWriteFontSet_GetFontFaceReference(s.set, i, &r)) || !r) continue;
        HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
        CHECKF(hr == S_OK && face, "item %u CreateFontFace -> %s", i, dw_hr_name(hr));
        if (face && !t_is_wine()) {   /* Wine faces from custom file loaders crash in GetFontFaceReference */
            IDWriteFontFaceReference *r2 = NULL;
            CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(face, &r2));
            if (r2) { CHECKF(IDWriteFontFaceReference_Equals(r, r2), "item %u: reference of the face equals the set's reference", i); DW_RELEASE(r2); }
        }
        DW_RELEASE(face); DW_RELEASE(r);
    }
    fs_std_close(&s);
}

TEST(fset_c_set_font_resource)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 i, n = IDWriteFontSet1_GetFontCount(s1);
        for (i = 0; i < n; i++) {
            IDWriteFontResource *res = NULL;
            HRESULT hr = IDWriteFontSet1_CreateFontResource(s1, i, &res);
            CHECKF(hr == S_OK && res, "CreateFontResource(%u) -> %s", i, dw_hr_name(hr));
            if (!res) continue;
            IDWriteFontFile *file = NULL;
            CHECK_HR_OK(IDWriteFontResource_GetFontFile(res, &file));
            CHECK(file != NULL);
            DW_RELEASE(file);
            CHECK(IDWriteFontResource_GetFontFaceIndex(res) == 0);
            CHECK(IDWriteFontResource_GetFontAxisCount(res) == 0);
            CHECK(IDWriteFontResource_HasVariations(res) == FALSE);
            DW_RELEASE(res);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_c_set_font_resource_create_face_with_simulations)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        IDWriteFontResource *res = NULL;
        CHECK_HR_OK(IDWriteFontSet1_CreateFontResource(s1, 0, &res));
        if (res) {
            IDWriteFontFace5 *face = NULL;
            HRESULT hr = IDWriteFontResource_CreateFontFace(res, DWRITE_FONT_SIMULATIONS_BOLD, NULL, 0, &face);
            CHECKF(hr == S_OK && face, "-> %s", dw_hr_name(hr));
            if (face) { CHECK(IDWriteFontFace_GetSimulations((IDWriteFontFace *)face) == DWRITE_FONT_SIMULATIONS_BOLD); DW_RELEASE(face); }
            IDWriteFontFaceReference1 *ref = NULL;
            hr = IDWriteFontResource_CreateFontFaceReference(res, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, &ref);
            CHECKF(hr == S_OK && ref, "-> %s", dw_hr_name(hr));
            DW_RELEASE(ref);
            DW_RELEASE(res);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_c_set_ref_and_resource_agree)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        IDWriteFontFaceReference1 *r1 = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetFontFaceReference(s1, 2, &r1));
        if (r1) {
            IDWriteFontFaceReference *base = (IDWriteFontFaceReference *)r1, *r0 = NULL;
            CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(s.set, 2, &r0));
            if (r0) { CHECK(IDWriteFontFaceReference_Equals(base, r0)); DW_RELEASE(r0); }
            CHECK(IDWriteFontFaceReference_GetFontFaceIndex(base) == 0);
            CHECK(IDWriteFontFaceReference_GetSimulations(base) == 0);
            DW_RELEASE(r1);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_c_family_get_font_set)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontCollection1 *c = coll_from(&s.x, s.set, &hr);
    if (c) {
        IDWriteFontCollection *base = (IDWriteFontCollection *)c;
        BOOL ex; UINT32 ia = find_fam(base, L"SynthA", &ex); IDWriteFontFamily *fam = NULL;
        if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(base, ia, &fam)) && fam) {
            IDWriteFontFamily2 *f2 = NULL;
            if (SUCCEEDED(IDWriteFontFamily_QueryInterface(fam, &IID_IDWriteFontFamily2, (void **)&f2)) && f2) {
                IDWriteFontSet1 *fs = NULL;
                CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(f2, &fs));
                if (fs) {
                    UINT32 n = IDWriteFontSet1_GetFontCount(fs);
                    CHECKF(n >= 4, "family font set count %u", n);
                    IDWriteStringList *l = fs_props(FS_AS_SET(fs), DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
                    if (l) { CHECK(IDWriteStringList_GetCount(l) == 1 && fs_sl_has(l, "SynthA")); DW_RELEASE(l); }
                    DW_RELEASE(fs);
                }
                DW_RELEASE(f2);
            } else t_note("IDWriteFontFamily2 not available");
            DW_RELEASE(fam);
        }
        DW_RELEASE(c);
    }
    fs_std_close(&s);
}

TEST(fset_c_system_collection_font_set_agrees)
{
    NEED_F3(f3);
    IDWriteFontCollection1 *c = NULL; IDWriteFontSet *fs = NULL;
    HRESULT hr = IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &c, FALSE);
    if (FAILED(hr) || !c) { t_skip("system collection unavailable"); return; }
    CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c, &fs));
    if (fs && dw_have_font(dw_shared(), L"Arial")) {
        UINT32 occ_n = 0; fs_prop p; fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Arial", "en-us");
        CHECK_HR_OK(IDWriteFontSet_GetPropertyOccurrenceCount(fs, &p.p, &occ_n));
        CHECK(occ_n >= 4);
        BOOL ex; UINT32 idx = find_fam((IDWriteFontCollection *)c, L"Arial", &ex);
        CHECK(ex == TRUE);
        IDWriteFontFamily *fam = NULL;
        if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily((IDWriteFontCollection *)c, idx, &fam)) && fam) {
            t_note("Arial: %u set entries, %u family fonts", occ_n, IDWriteFontFamily_GetFontCount(fam));
            DW_RELEASE(fam);
        }
    }
    DW_RELEASE(fs); DW_RELEASE(c);
}
