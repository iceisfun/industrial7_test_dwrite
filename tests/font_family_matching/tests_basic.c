/* IDWriteFontFamily / IDWriteFontList basics: counts, GetFont bounds, names, collection identity, QueryInterface, lifetime. */
#include "fm.h"
#include "fm_tables.h"

/* n distinct faces: weights 100..900 normal, then italic, then condensed, then expanded */
static int fm_seq(fm_face *f, int n) {
    int k;
    for (k = 0; k < n; k++) {
        if (k < 9)       f[k] = (fm_face){ 100 * (1 + k), 5, 0 };
        else if (k < 18) f[k] = (fm_face){ 100 * (1 + k - 9), 5, 2 };
        else if (k < 27) f[k] = (fm_face){ 100 * (1 + k - 18), 3, 0 };
        else             f[k] = (fm_face){ 100 * (1 + k - 27), 7, 0 };
    }
    return n;
}

/* ---- GetFontCount ----------------------------------------------------------------------------- */
static void count_case(int n) {
    fm_face f[FM_MAXF]; fm m; int i;
    fm_seq(f, n);
    if (!fm_open(&m, "FmCount", f, n, NULL)) return;
    /* the family lists exactly the faces that carry the typographic family name; no simulated faces are added */
    EXPECT_EQ(m.count, n);
    EXPECT_EQ(IDWriteFontFamily_GetFontCount(m.fam), n);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(m.coll), 1);
    for (i = 0; i < n; i++) CHECKF(fm_find(&m, f[i].w, f[i].wd, f[i].st) >= 0, "face %d (w=%d wd=%d st=%d) missing", i, f[i].w, f[i].wd, f[i].st);
    for (i = 0; i < (int)m.count; i++) { CHECKF(m.src[i] >= 0, "family font %d has no matching spec", i); CHECKF(m.obs_sims[i] == 0, "family font %d simulations %d", i, m.obs_sims[i]); }
    fm_close(&m);
}
#define COUNT_TEST(n) TEST(fam_count_##n) { count_case(n); }
COUNT_TEST(1) COUNT_TEST(2) COUNT_TEST(3) COUNT_TEST(4) COUNT_TEST(5) COUNT_TEST(9) COUNT_TEST(10) COUNT_TEST(18) COUNT_TEST(27) COUNT_TEST(32)

TEST(fam_count_stable_across_calls) {
    FM_OPEN(m, "FmStable", F_RIBBI);
    int i;
    for (i = 0; i < 10; i++) EXPECT_EQ(IDWriteFontFamily_GetFontCount(m.fam), 4);
    fm_close(&m);
}

TEST(fam_ribbi_triples_are_the_spec) {
    FM_OPEN(m, "FmRibbi", F_RIBBI);
    int i, k, seen[4] = { 0 };
    for (i = 0; i < (int)m.count; i++) if (m.src[i] >= 0) seen[m.src[i]]++;
    for (k = 0; k < 4; k++) EXPECT_EQ(seen[k], 1);   /* every spec face appears exactly once */
    fm_close(&m);
}

/* ---- GetFont(index) --------------------------------------------------------------------------- */
TEST(fam_getfont_every_index_succeeds) {
    FM_OPEN(m, "FmGetFont", F_W9);
    int i;
    for (i = 0; i < (int)m.count; i++) {
        IDWriteFont *font = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, i, &font));
        CHECKF(font != NULL, "GetFont(%d) returned NULL", i);
        DW_RELEASE(font);
    }
    fm_close(&m);
}

TEST(fam_getfont_index_out_of_range) {
    FM_OPEN(m, "FmOob", F_RIBBI);
    /* DWriteFontFamily::GetFont(index): the face key lookup throws IndexOutOfRangeException (HRESULT E_FAIL 0x80004005);
     * *font is zeroed first. */
    const UINT32 bad[] = { m.count, m.count + 1, 0xFF, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)&i;
        HRESULT hr = IDWriteFontFamily_GetFont(m.fam, bad[i], &font);
        CHECKF(hr == E_FAIL, "GetFont(%u) -> %s", bad[i], dw_hr_name(hr));
        CHECKF(font == NULL, "GetFont(%u) left out parameter %p", bad[i], (void *)font);
        DW_RELEASE(font);
    }
    fm_close(&m);
}

TEST(fam_getfont_last_valid_index) {
    FM_OPEN(m, "FmLast", F_W9);
    IDWriteFont *font = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, m.count - 1, &font));
    CHECK(font != NULL);
    DW_RELEASE(font);
    CHECK_HR(IDWriteFontFamily_GetFont(m.fam, m.count, &font), E_FAIL);
    CHECK_OUT_NULL(font);
    fm_close(&m);
}

TEST(fam_getfont_creates_new_object_each_call) {
    FM_OPEN(m, "FmFresh", F_RIBBI);
    IDWriteFont *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, 0, &a));
    CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, 0, &b));
    /* DWriteFont::Create allocates a new COM object per call */
    CHECK(a && b && a != b);
    if (a && b) CHECK(!dw_same_identity((IUnknown *)a, (IUnknown *)b));
    DW_RELEASE(a); DW_RELEASE(b);
    fm_close(&m);
}

TEST(fam_getfont_same_index_same_attributes) {
    FM_OPEN(m, "FmSame", F_RIBBI);
    int i, r;
    for (i = 0; i < (int)m.count; i++) for (r = 0; r < 3; r++) {
        IDWriteFont *font = NULL; fm_face got; int s;
        CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, i, &font));
        fm_read_font(font, &got, &s);
        CHECKF(got.w == m.obs[i].w && got.wd == m.obs[i].wd && got.st == m.obs[i].st && s == m.obs_sims[i], "index %d repeat %d differs", i, r);
        DW_RELEASE(font);
    }
    fm_close(&m);
}

TEST(fam_getfont_indices_are_distinct_faces) {
    FM_OPEN(m, "FmDistinct", F_W9);
    int i, j;
    for (i = 0; i < (int)m.count; i++) for (j = i + 1; j < (int)m.count; j++)
        CHECKF(!(m.obs[i].w == m.obs[j].w && m.obs[i].wd == m.obs[j].wd && m.obs[i].st == m.obs[j].st), "indices %d and %d are the same face", i, j);
    fm_close(&m);
}

TEST(fam_font_refcounts_balance) {
    FM_OPEN(m, "FmRef", F_RIBBI);
    IDWriteFont *font = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, 0, &font));
    if (font) {
        ULONG base = dw_refcount((IUnknown *)font), r1, r2, r3;
        r1 = IDWriteFont_AddRef(font); r2 = IDWriteFont_AddRef(font);
        EXPECT_EQ(r1, base + 1); EXPECT_EQ(r2, base + 2);
        r3 = IDWriteFont_Release(font); EXPECT_EQ(r3, base + 1);
        r3 = IDWriteFont_Release(font); EXPECT_EQ(r3, base);
        CHECK(base >= 1);
        IDWriteFont_Release(font);
    }
    fm_close(&m);
}

TEST(fam_font_keeps_family_alive) {
    FM_OPEN(m, "FmKeep", F_RIBBI);
    IDWriteFont *font = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, 1, &font));
    fm_face want = m.obs[1];
    DW_RELEASE(m.fam);   /* the font holds its family (DWriteFont stores a counted family reference) */
    if (font) {
        fm_face got; int s;
        fm_read_font(font, &got, &s);
        CHECK(got.w == want.w && got.wd == want.wd && got.st == want.st);
        IDWriteFontFamily *ff = NULL;
        CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &ff));
        CHECK(ff != NULL);
        if (ff) {
            UINT32 c = IDWriteFontFamily_GetFontCount(ff);
            EXPECT_EQ(c, 4);
            IDWriteFontFamily_Release(ff);
        }
        IDWriteFont_Release(font);
    }
    fm_close(&m);
}

TEST(fam_font_getfontfamily_is_same_object) {
    FM_OPEN(m, "FmIdent", F_RIBBI);
    IDWriteFont *font = NULL; IDWriteFontFamily *ff = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, 2, &font));
    if (font) {
        ULONG before = dw_refcount((IUnknown *)m.fam);
        CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &ff));
        /* DWriteFont::GetFontFamily AddRefs the stored family pointer: the very object the font was created from */
        CHECK(ff != NULL);
        if (ff) {
            CHECK(dw_same_identity((IUnknown *)ff, (IUnknown *)m.fam));
            EXPECT_REFS(m.fam, before + 1);
            IDWriteFontFamily_Release(ff);
            EXPECT_REFS(m.fam, before);
        }
        IDWriteFont_Release(font);
    }
    fm_close(&m);
}

/* ---- GetFamilyNames --------------------------------------------------------------------------- */
TEST(fam_names_en_us_string) {
    FM_OPEN(m, "FmNames", F_RIBBI);
    IDWriteLocalizedStrings *s = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(m.fam, &s));
    CHECK(s != NULL);
    if (s) {
        UINT32 idx = 99; BOOL ex = FALSE; char buf[128];
        CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1);
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"en-us", &idx, &ex));
        CHECK(ex);
        if (ex) { CHECK_HR_OK(dw_ls_get(s, idx, buf, sizeof buf)); EXPECT_STR(buf, "FmNames"); }
        IDWriteLocalizedStrings_Release(s);
    }
    fm_close(&m);
}

TEST(fam_names_locale_lookup_negative) {
    FM_OPEN(m, "FmNamesNeg", F_RIBBI);
    IDWriteLocalizedStrings *s = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(m.fam, &s));
    if (s) {
        UINT32 idx = 77; BOOL ex = TRUE;
        /* FindLocaleName on a locale the name table does not carry: S_OK, exists FALSE, index UINT_MAX */
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"xx-yy", &idx, &ex));
        CHECK(!ex);
        EXPECT_EQ(idx, 0xFFFFFFFFu);
        IDWriteLocalizedStrings_Release(s);
    }
    fm_close(&m);
}

TEST(fam_names_returns_new_object_each_time) {
    FM_OPEN(m, "FmNamesTwo", F_RIBBI);
    IDWriteLocalizedStrings *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(m.fam, &a));
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(m.fam, &b));
    CHECK(a && b && a != b);
    if (a && b) {
        char x[64], y[64];
        dw_ls_en(a, x, sizeof x); dw_ls_en(b, y, sizeof y);
        EXPECT_STR(x, y);
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(a), IDWriteLocalizedStrings_GetCount(b));
    }
    DW_RELEASE(a); DW_RELEASE(b);
    fm_close(&m);
}

TEST(fam_names_independent_of_member_weight) {
    /* every member of the family reports the typographic family name, whatever its own weight/style */
    FM_OPEN(m, "FmNamesAll", F_W9);
    int i;
    for (i = 0; i < (int)m.count; i++) {
        IDWriteFont *font = NULL; IDWriteFontFamily *ff = NULL; IDWriteLocalizedStrings *s = NULL; char buf[96] = "";
        if (FAILED(IDWriteFontFamily_GetFont(m.fam, i, &font))) { CHECK(0); continue; }
        if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &ff)) && ff && SUCCEEDED(IDWriteFontFamily_GetFamilyNames(ff, &s)) && s) {
            dw_ls_en(s, buf, sizeof buf); EXPECT_STR(buf, "FmNamesAll");
        } else CHECK(0);
        DW_RELEASE(s); DW_RELEASE(ff); DW_RELEASE(font);
    }
    fm_close(&m);
}

TEST(fam_names_string_buffer_protocol) {
    FM_OPEN(m, "FmNamesBuf", F_ONE);
    IDWriteLocalizedStrings *s = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(m.fam, &s));
    if (s) {
        UINT32 len = 0, idx = 0; BOOL ex = FALSE; WCHAR buf[32];
        IDWriteLocalizedStrings_FindLocaleName(s, L"en-us", &idx, &ex);
        CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, idx, &len));
        EXPECT_EQ(len, 10);   /* "FmNamesBuf" */
        CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, idx, buf, len + 1));
        CHECK(buf[len] == 0);
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, idx, buf, len), DW_E_NOT_SUFFICIENT_BUFFER);
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, idx, buf, 0), DW_E_NOT_SUFFICIENT_BUFFER);
        CHECK_HR(IDWriteLocalizedStrings_GetStringLength(s, IDWriteLocalizedStrings_GetCount(s), &len), E_FAIL);
        IDWriteLocalizedStrings_Release(s);
    }
    fm_close(&m);
}

/* ---- GetFontCollection / identity ----------------------------------------------------------------- */
TEST(fam_collection_is_the_creating_collection) {
    FM_OPEN(m, "FmColl", F_RIBBI);
    IDWriteFontCollection *c = NULL;
    ULONG before = dw_refcount((IUnknown *)m.coll);
    CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(m.fam, &c));
    CHECK(c != NULL);
    if (c) {
        /* DWriteFontFamily::GetFontCollection AddRefs the stored collection pointer */
        CHECK(dw_same_identity((IUnknown *)c, (IUnknown *)m.coll));
        EXPECT_REFS(m.coll, before + 1);
        IDWriteFontCollection_Release(c);
        EXPECT_REFS(m.coll, before);
    }
    fm_close(&m);
}

TEST(fam_collection_find_family_name_roundtrip) {
    FM_OPEN(m, "FmFind", F_RIBBI);
    UINT32 idx = 5; BOOL ex = FALSE;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(m.coll, L"FmFind", &idx, &ex));
    CHECK(ex); EXPECT_EQ(idx, 0);
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(m.coll, L"fmfind", &idx, &ex));   /* case-insensitive */
    CHECK(ex);
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(m.coll, L"FmFindX", &idx, &ex));
    CHECK(!ex); EXPECT_EQ(idx, 0xFFFFFFFFu);
    fm_close(&m);
}

TEST(fam_family_from_collection_matches_count) {
    FM_OPEN(m, "FmFromColl", F_W9);
    IDWriteFontFamily *ff = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(m.coll, 0, &ff));
    if (ff) { EXPECT_EQ(IDWriteFontFamily_GetFontCount(ff), 9); IDWriteFontFamily_Release(ff); }
    fm_close(&m);
}

/* ---- QueryInterface ------------------------------------------------------------------------------- */
TEST(fam_qi_matrix) {
    FM_OPEN(m, "FmQi", F_RIBBI);
    /* ComObject<DWriteFontFamily>::QueryInterface: IDWriteFontFamily2 (the 3ed49e77 comparison), IDWriteFontFamily1, IDWriteFontFamily,
     * IDWriteFontList2, IDWriteFontList1, IDWriteFontList and IUnknown; anything else is E_NOINTERFACE with *ppv NULL. */
    dw_qi_row rows[] = {
        DW_QI_ROW(IDWriteFontFamily, 1), DW_QI_ROW(IDWriteFontFamily1, 1), DW_QI_ROW(IDWriteFontFamily2, 1),
        DW_QI_ROW(IDWriteFontList, 1), DW_QI_ROW(IDWriteFontList1, 1), DW_QI_ROW(IDWriteFontList2, 1),
        DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFont1, 0), DW_QI_ROW(IDWriteFont2, 0), DW_QI_ROW(IDWriteFont3, 0),
        DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontCollection1, 0), DW_QI_ROW(IDWriteFontCollection2, 0),
        DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFontSet1, 0),
        DW_QI_ROW(IDWriteLocalizedStrings, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteTextFormat, 0),
    };
    DW_CHECK_QI(m.fam, rows);
    fm_close(&m);
}

TEST(fam_qi_other_iids_fail_cleanly) {
    FM_OPEN(m, "FmQi2", F_ONE);
    int n = 0, i; const dw_qi_row *t = dw_iid_table(&n);
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)&n; HRESULT hr;
        if (t[i].iid == &IID_IDWriteFontFamily || t[i].iid == &IID_IDWriteFontFamily1 || t[i].iid == &IID_IDWriteFontFamily2 ||
            t[i].iid == &IID_IDWriteFontList || t[i].iid == &IID_IDWriteFontList1 || t[i].iid == &IID_IDWriteFontList2) continue;
        hr = IDWriteFontFamily_QueryInterface(m.fam, t[i].iid, &p);
        if (hr == S_OK) { CHECKF(0, "family answers %s", t[i].name); IUnknown_Release((IUnknown *)p); }
        else { CHECKF(hr == E_NOINTERFACE, "%s -> %s", t[i].name, dw_hr_name(hr)); CHECKF(p == NULL, "%s left %p", t[i].name, p); }
    }
    fm_close(&m);
}

TEST(fam_qi_interfaces_share_identity) {
    FM_OPEN(m, "FmQi3", F_RIBBI);
    IDWriteFontFamily1 *f1 = NULL; IDWriteFontFamily2 *f2 = NULL; IDWriteFontList *l = NULL; IDWriteFontList2 *l2 = NULL;
    IUnknown *u1 = NULL, *u2 = NULL;
    CHECK_HR_OK(IDWriteFontFamily_QueryInterface(m.fam, &IID_IDWriteFontFamily1, (void **)&f1));
    CHECK_HR_OK(IDWriteFontFamily_QueryInterface(m.fam, &IID_IDWriteFontFamily2, (void **)&f2));
    CHECK_HR_OK(IDWriteFontFamily_QueryInterface(m.fam, &IID_IDWriteFontList, (void **)&l));
    CHECK_HR_OK(IDWriteFontFamily_QueryInterface(m.fam, &IID_IDWriteFontList2, (void **)&l2));
    if (f1 && f2 && l && l2) {
        IDWriteFontFamily1_QueryInterface(f1, &IID_IUnknown, (void **)&u1);
        IDWriteFontFamily2_QueryInterface(f2, &IID_IUnknown, (void **)&u2);
        CHECK(u1 && u1 == u2);
        CHECK(dw_same_identity((IUnknown *)f1, (IUnknown *)m.fam));
        CHECK(dw_same_identity((IUnknown *)l, (IUnknown *)l2));
        CHECK(dw_same_identity((IUnknown *)l, (IUnknown *)m.fam));
        EXPECT_EQ(IDWriteFontList_GetFontCount(l), 4);
        EXPECT_EQ(IDWriteFontList2_GetFontCount(l2), 4);
        EXPECT_EQ(IDWriteFontFamily2_GetFontCount(f2), 4);
    }
    DW_RELEASE(u1); DW_RELEASE(u2); DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(l); DW_RELEASE(l2);
    fm_close(&m);
}

TEST(fam_refcount_balance_through_qi) {
    FM_OPEN(m, "FmQiRef", F_RIBBI);
    ULONG base = dw_refcount((IUnknown *)m.fam);
    IDWriteFontFamily2 *f2 = NULL;
    CHECK_HR_OK(IDWriteFontFamily_QueryInterface(m.fam, &IID_IDWriteFontFamily2, (void **)&f2));
    EXPECT_REFS(m.fam, base + 1);
    IDWriteFontFamily2_Release(f2);
    EXPECT_REFS(m.fam, base);
    fm_close(&m);
}

/* ---- lifetime ------------------------------------------------------------------------------------- */
TEST(fam_family_outlives_collection_reference) {
    FM_OPEN(m, "FmLife", F_RIBBI);
    DW_RELEASE(m.coll);   /* GetFontCollection hands out a counted reference, so the family keeps the collection alive */
    IDWriteFont *font = NULL; IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFont(m.fam, 0, &font));
    DW_RELEASE(font);
    CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(m.fam, &c));
    CHECK(c != NULL);
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); IDWriteFontCollection_Release(c); }
    IDWriteFont *f2 = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 700, 5, 0, &f2));
    DW_RELEASE(f2);
    fm_close(&m);
}

TEST(fam_two_family_objects_agree) {
    FM_OPEN(m, "FmTwo", F_W9);
    IDWriteFontFamily *other = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(m.coll, 0, &other));
    if (other) {
        UINT32 i;
        EXPECT_EQ(IDWriteFontFamily_GetFontCount(other), m.count);
        for (i = 0; i < m.count; i++) {
            IDWriteFont *a = NULL, *b = NULL; fm_face fa, fb; int sa, sb;
            IDWriteFontFamily_GetFont(m.fam, i, &a); IDWriteFontFamily_GetFont(other, i, &b);
            if (fm_read_font(a, &fa, &sa) && fm_read_font(b, &fb, &sb)) CHECKF(fa.w == fb.w && fa.wd == fb.wd && fa.st == fb.st, "index %u differs between family objects", i);
            DW_RELEASE(a); DW_RELEASE(b);
        }
        t_note("GetFontFamily(0) twice: %s object", dw_same_identity((IUnknown *)other, (IUnknown *)m.fam) ? "same" : "different");
        IDWriteFontFamily_Release(other);
    }
    fm_close(&m);
}

/* ---- GetFirstMatchingFont plumbing ---------------------------------------------------------------- */
TEST(fam_first_matching_returns_new_object_each_call) {
    FM_OPEN(m, "FmFirstNew", F_RIBBI);
    IDWriteFont *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 400, 5, 0, &a));
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 400, 5, 0, &b));
    CHECK(a && b && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    fm_close(&m);
}

TEST(fam_first_matching_writes_out_param) {
    FM_OPEN(m, "FmFirstOut", F_RIBBI);
    IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)&m;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 400, 5, 0, &font));
    CHECK(font != NULL && font != (IDWriteFont *)(ULONG_PTR)&m);
    DW_RELEASE(font);
    fm_close(&m);
}

TEST(fam_first_matching_font_belongs_to_family) {
    FM_OPEN(m, "FmFirstFam", F_RIBBI);
    IDWriteFont *font = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 700, 5, 2, &font));
    if (font) fm_check_font(&m, font, &m.spec[3], 0, "bold italic");
    DW_RELEASE(font);
    fm_close(&m);
}

TEST(fam_out_of_range_request_values_are_accepted) {
    FM_OPEN(m, "FmEnumOob", F_RIBBI);
    /* the matcher is plain integer arithmetic: no enum validation, always S_OK and a font */
    const int w[] = { 0, 1, 999, 1000, 1001, 5000, 0x7FFFFFFF, -1, (int)0x80000000 };
    const int s[] = { 0, 1, 9, 10, 11, 100, 0x7FFFFFFF, -1 };
    const int t[] = { 0, 1, 2, 3, 4, 10, 255, 0x7FFFFFFF, -1 };
    size_t i;
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        IDWriteFont *font = NULL; IDWriteFontList *list = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, (DWRITE_FONT_WEIGHT)w[i], DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font));
        CHECKF(font != NULL, "weight %d", w[i]); DW_RELEASE(font);
        CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, (DWRITE_FONT_WEIGHT)w[i], DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &list));
        CHECKF(list != NULL && IDWriteFontList_GetFontCount(list) >= 1, "weight %d list", w[i]); DW_RELEASE(list);
    }
    for (i = 0; i < sizeof s / sizeof s[0]; i++) {
        IDWriteFont *font = NULL; IDWriteFontList *list = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, DWRITE_FONT_WEIGHT_NORMAL, (DWRITE_FONT_STRETCH)s[i], DWRITE_FONT_STYLE_NORMAL, &font));
        CHECKF(font != NULL, "stretch %d", s[i]); DW_RELEASE(font);
        CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, DWRITE_FONT_WEIGHT_NORMAL, (DWRITE_FONT_STRETCH)s[i], DWRITE_FONT_STYLE_NORMAL, &list));
        CHECKF(list != NULL, "stretch %d list", s[i]); DW_RELEASE(list);
    }
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDWriteFont *font = NULL; IDWriteFontList *list = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (DWRITE_FONT_STYLE)t[i], &font));
        CHECKF(font != NULL, "style %d", t[i]); DW_RELEASE(font);
        CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, (DWRITE_FONT_STYLE)t[i], &list));
        CHECKF(list != NULL, "style %d list", t[i]); DW_RELEASE(list);
    }
    fm_close(&m);
}

TEST(fam_out_of_range_request_still_yields_face_of_family) {
    FM_OPEN(m, "FmEnumOob2", F_W9);
    const int w[] = { 0, 1000, 1500, 100000 };
    size_t i;
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        IDWriteFont *font = NULL;
        if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(m.fam, (DWRITE_FONT_WEIGHT)w[i], 5, 0, &font)) && font) {
            fm_face f; int s; fm_read_font(font, &f, &s);
            CHECKF(fm_find(&m, f.w, f.wd, f.st) >= 0 || s != 0, "weight %d returned face w=%d", w[i], f.w);
        }
        DW_RELEASE(font);
    }
    /* 0 is below every face and 1000+ above every face: lightest / heaviest face respectively */
    fm_expect_first(&m, 0, 5, 0, fm_find(&m, 100, 5, 0), 0);
    fm_expect_first(&m, 1000, 5, 0, fm_find(&m, 900, 5, 0), 0);
    fm_close(&m);
}

/* ---- GetMatchingFonts list plumbing --------------------------------------------------------------- */
TEST(fam_list_count_and_get_font) {
    FM_OPEN(m, "FmList", F_RIBBI);
    IDWriteFontList *l = NULL; UINT32 c, i;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    CHECK(l != NULL);
    if (l) {
        c = IDWriteFontList_GetFontCount(l);
        CHECK(c >= 1 && c <= 4);
        for (i = 0; i < c; i++) {
            IDWriteFont *font = NULL;
            CHECK_HR_OK(IDWriteFontList_GetFont(l, i, &font));
            CHECK(font != NULL);
            DW_RELEASE(font);
        }
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}

TEST(fam_list_get_font_out_of_range) {
    FM_OPEN(m, "FmListOob", F_RIBBI);
    IDWriteFontList *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l) {
        UINT32 c = IDWriteFontList_GetFontCount(l);
        const UINT32 bad[] = { c, c + 1, 0x100, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFFu };
        size_t i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)&i;
            HRESULT hr = IDWriteFontList_GetFont(l, bad[i], &font);
            CHECKF(hr == E_FAIL, "list GetFont(%u) -> %s", bad[i], dw_hr_name(hr));
            CHECKF(font == NULL, "list GetFont(%u) left %p", bad[i], (void *)font);
            DW_RELEASE(font);
        }
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}

TEST(fam_list_collection_is_family_collection) {
    FM_OPEN(m, "FmListColl", F_RIBBI);
    IDWriteFontList *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l) {
        IDWriteFontCollection *c = NULL; ULONG before = dw_refcount((IUnknown *)m.coll);
        CHECK_HR_OK(IDWriteFontList_GetFontCollection(l, &c));
        CHECK(c != NULL);
        if (c) {
            CHECK(dw_same_identity((IUnknown *)c, (IUnknown *)m.coll));
            EXPECT_REFS(m.coll, before + 1);
            IDWriteFontCollection_Release(c);
        }
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}

TEST(fam_list_fonts_report_the_source_family) {
    FM_OPEN(m, "FmListFam", F_RIBBI);
    IDWriteFontList *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l) {
        UINT32 c = IDWriteFontList_GetFontCount(l), i;
        for (i = 0; i < c; i++) {
            IDWriteFont *font = NULL; IDWriteFontFamily *ff = NULL;
            if (SUCCEEDED(IDWriteFontList_GetFont(l, i, &font)) && font && SUCCEEDED(IDWriteFont_GetFontFamily(font, &ff)) && ff) {
                /* DWriteMatchingFontList::GetFont creates fonts with the family it was created from */
                CHECKF(dw_same_identity((IUnknown *)ff, (IUnknown *)m.fam), "list font %u family differs", i);
            } else CHECK(0);
            DW_RELEASE(ff); DW_RELEASE(font);
        }
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}

TEST(fam_list_holds_family_reference) {
    FM_OPEN(m, "FmListHold", F_RIBBI);
    ULONG before = dw_refcount((IUnknown *)m.fam);
    IDWriteFontList *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    /* DWriteMatchingFontList's constructor AddRefs the family */
    EXPECT_REFS(m.fam, before + 1);
    if (l) IDWriteFontList_Release(l);
    EXPECT_REFS(m.fam, before);
    fm_close(&m);
}

TEST(fam_list_survives_family_and_collection_release) {
    FM_OPEN(m, "FmListLife", F_RIBBI);
    IDWriteFontList *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 700, 5, 2, &l));
    DW_RELEASE(m.fam); DW_RELEASE(m.coll);
    if (l) {
        UINT32 c = IDWriteFontList_GetFontCount(l);
        CHECK(c >= 1);
        IDWriteFont *font = NULL;
        CHECK_HR_OK(IDWriteFontList_GetFont(l, 0, &font));
        if (font) { EXPECT_EQ(IDWriteFont_GetWeight(font), 700); EXPECT_EQ(IDWriteFont_GetStyle(font), 2); IDWriteFont_Release(font); }
        IDWriteFontCollection *cc = NULL;
        CHECK_HR_OK(IDWriteFontList_GetFontCollection(l, &cc));
        DW_RELEASE(cc);
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}

TEST(fam_list_objects_are_distinct) {
    FM_OPEN(m, "FmListTwo", F_RIBBI);
    IDWriteFontList *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &a));
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &b));
    CHECK(a && b && a != b);
    if (a && b) CHECK(!dw_same_identity((IUnknown *)a, (IUnknown *)b));
    DW_RELEASE(a); DW_RELEASE(b);
    fm_close(&m);
}

TEST(fam_list_qi_matrix) {
    FM_OPEN(m, "FmListQi", F_RIBBI);
    IDWriteFontList *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l) {
        /* ComObject<DWriteMatchingFontList>::QueryInterface: IDWriteFontList2 (c0763a34), IDWriteFontList1 (da20d8ef-...-ade), IDWriteFontList, IUnknown */
        dw_qi_row rows[] = {
            DW_QI_ROW(IDWriteFontList, 1), DW_QI_ROW(IDWriteFontList1, 1), DW_QI_ROW(IDWriteFontList2, 1),
            DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFontFamily1, 0), DW_QI_ROW(IDWriteFontFamily2, 0),
            DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0),
        };
        DW_CHECK_QI(l, rows);
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}

TEST(fam_list_is_not_a_family) {
    FM_OPEN(m, "FmListNotFam", F_RIBBI);
    IDWriteFontList *l = NULL; IDWriteFontFamily *ff = (IDWriteFontFamily *)(ULONG_PTR)&m;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l) {
        CHECK_HR(IDWriteFontList_QueryInterface(l, &IID_IDWriteFontFamily, (void **)&ff), E_NOINTERFACE);
        CHECK_OUT_NULL(ff);
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}

TEST(fam_first_and_list_head_agree) {
    FM_OPEN(m, "FmHeadAgree", F_W9);
    int w;
    for (w = 50; w <= 1000; w += 25) {
        IDWriteFont *a = NULL, *b = NULL; IDWriteFontList *l = NULL; fm_face fa, fb; int sa, sb;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, (DWRITE_FONT_WEIGHT)w, 5, 0, &a));
        CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, (DWRITE_FONT_WEIGHT)w, 5, 0, &l));
        if (l) IDWriteFontList_GetFont(l, 0, &b);
        if (fm_read_font(a, &fa, &sa) && fm_read_font(b, &fb, &sb))
            CHECKF(fa.w == fb.w && fa.wd == fb.wd && fa.st == fb.st && sa == sb, "weight %d: first %d vs list head %d", w, fa.w, fb.w);
        else CHECK(0);
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(l);
    }
    fm_close(&m);
}
