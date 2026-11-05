/* Font collections, families, font sets, set builders, fallback builders and rendering-side objects: index-out-of-range (E_FAIL),
 * unknown family (NOFONT), NULL / count combinations (E_INVALIDARG) and the buffer protocols of the set queries. */
#include "eh.h"

/* one-family custom collection ("DwTest", a single regular face) ----------------------------------------------------------------- */
typedef struct { IDWriteFactory *f; dwx_mem_loader *files; dwx_coll_loader *cl; IDWriteFontCollection *c; uint8_t *blob; int ok; } cfix;
static int cfix_open(cfix *x, IDWriteFactory *f) {
    size_t n; memset(x, 0, sizeof *x); x->f = f; x->blob = ttf_build_default(&n);
    x->files = dwx_loader_new(); UINT32 key = dwx_loader_add(x->files, x->blob, n);
    x->cl = dwx_coll_new(x->files); dwx_coll_add_key(x->cl, key);
    x->ok = SUCCEEDED(dwx_loader_register(x->files, f)) && SUCCEEDED(dwx_coll_register(x->cl, f)) && SUCCEEDED(dwx_coll_create(x->cl, f, &x->c)) && x->c;
    return x->ok;
}
static void cfix_close(cfix *x) {
    DW_RELEASE(x->c);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); }
    if (x->files) { dwx_loader_unregister(x->files); dwx_release(x->files); }
    free(x->blob);
}
#define NEED_COLL(x) NEED_FACTORY(); cfix x; if (!cfix_open(&x, f)) { cfix_close(&x); DONE_FACTORY(); t_skip("custom collection unavailable"); return; }
#define END_COLL(x) cfix_close(&x); DONE_FACTORY()

TEST(collection_family_index_out_of_range) {
    NEED_COLL(x); UINT32 n = IDWriteFontCollection_GetFontFamilyCount(x.c); static const UINT32 off[] = { 0, 1, 2, 1000, 0x7fffffff };
    unsigned i; EXPECT_EQ(n, 1);
    for (i = 0; i < sizeof off / sizeof off[0]; i++) {
        IDWriteFontFamily *fam = SENT(IDWriteFontFamily *); UINT32 idx = n + off[i];
        HRESULT hr = IDWriteFontCollection_GetFontFamily(x.c, idx, &fam);
        CHECKF(hr == E_FAIL && fam == NULL, "GetFontFamily(%u) -> %s out=%p", idx, dw_hr_name(hr), fam);
        if (fam && fam != SENT(IDWriteFontFamily *)) DW_RELEASE(fam);
    }
    { IDWriteFontFamily *fam = SENT(IDWriteFontFamily *); CHECK_HR(IDWriteFontCollection_GetFontFamily(x.c, 0xFFFFFFFFu, &fam), E_FAIL); CHECK_OUT_NULL(fam); }
    { IDWriteFontFamily *fam = NULL; CHECK_HR(IDWriteFontCollection_GetFontFamily(x.c, 0, &fam), S_OK); DW_RELEASE(fam); }
    END_COLL(x);
}
TEST(collection_findfamilyname_miss_is_not_an_error) {
    NEED_COLL(x); UINT32 idx = 55; BOOL ex = 7;
    CHECK_HR(IDWriteFontCollection_FindFamilyName(x.c, L"No Such Family Zq", &idx, &ex), S_OK); EXPECT_EQ(idx, 0xFFFFFFFFu); EXPECT_EQ(ex, 0);
    idx = 55; ex = 7; CHECK_HR(IDWriteFontCollection_FindFamilyName(x.c, L"DwTest", &idx, &ex), S_OK); EXPECT_EQ(idx, 0); EXPECT_EQ(ex, 1);
    idx = 55; ex = 7; CHECK_HR(IDWriteFontCollection_FindFamilyName(x.c, L"", &idx, &ex), S_OK); EXPECT_EQ(ex, 0);
    END_COLL(x);
}
TEST(family_font_index_out_of_range) {
    NEED_COLL(x); IDWriteFontFamily *fam = NULL;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(x.c, 0, &fam)) && fam) {
        UINT32 n = IDWriteFontFamily_GetFontCount(fam); unsigned i; static const UINT32 off[] = { 0, 1, 5, 0x7fffffff };
        EXPECT_EQ(n, 1);
        for (i = 0; i < sizeof off / sizeof off[0]; i++) {
            IDWriteFont *font = SENT(IDWriteFont *); HRESULT hr = IDWriteFontFamily_GetFont(fam, n + off[i], &font);
            CHECKF(hr == E_FAIL && font == NULL, "GetFont(%u) -> %s", n + off[i], dw_hr_name(hr));
            if (font && font != SENT(IDWriteFont *)) DW_RELEASE(font);
        }
        { IDWriteFont *font = SENT(IDWriteFont *); CHECK_HR(IDWriteFontFamily_GetFont(fam, 0xFFFFFFFFu, &font), E_FAIL); CHECK_OUT_NULL(font); }
        { IDWriteFont *font = NULL; CHECK_HR(IDWriteFontFamily_GetFont(fam, 0, &font), S_OK); DW_RELEASE(font); }
        DW_RELEASE(fam);
    }
    END_COLL(x);
}
TEST(family_getmatchingfonts_axis_arguments) {
    NEED_COLL(x); IDWriteFontFamily *fam = NULL; IDWriteFontFamily2 *f2 = NULL;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(x.c, 0, &fam)) && fam && SUCCEEDED(IDWriteFontFamily_QueryInterface(fam, &IID_IDWriteFontFamily2, (void **)&f2)) && f2) {
        IDWriteFontList2 *l = SENT(IDWriteFontList2 *); DWRITE_FONT_AXIS_VALUE v[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.f } };
        CHECK_HR(IDWriteFontFamily2_GetMatchingFonts(f2, NULL, 1, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
        l = SENT(IDWriteFontList2 *); CHECK_HR(IDWriteFontFamily2_GetMatchingFonts(f2, v, 0x10000, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
        l = SENT(IDWriteFontList2 *); CHECK_HR(IDWriteFontFamily2_GetMatchingFonts(f2, v, 0xFFFFFFFFu, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
        l = NULL; CHECK_HR(IDWriteFontFamily2_GetMatchingFonts(f2, NULL, 0, &l), S_OK); DW_RELEASE(l);
        l = NULL; CHECK_HR(IDWriteFontFamily2_GetMatchingFonts(f2, v, 1, &l), S_OK); DW_RELEASE(l);
        DW_RELEASE(f2);
    } else t_skip("IDWriteFontFamily2 not available");
    DW_RELEASE(fam); END_COLL(x);
}
TEST(collection2_getmatchingfonts_nofont_and_arguments) {
    NEED_COLL(x); IDWriteFontCollection2 *c2 = NULL;
    if (SUCCEEDED(IDWriteFontCollection_QueryInterface(x.c, &IID_IDWriteFontCollection2, (void **)&c2)) && c2) {
        IDWriteFontList2 *l = SENT(IDWriteFontList2 *); DWRITE_FONT_AXIS_VALUE v[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.f } };
        if (!t_is_wine()) { CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, NULL, v, 1, &l), E_INVALIDARG); CHECK_OUT_NULL(l); }
        l = SENT(IDWriteFontList2 *); CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, L"DwTest", v, 0x10000, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
        l = SENT(IDWriteFontList2 *); CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, L"DwTest", NULL, 1, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
        /* argument validation precedes the lookup; an unknown family name then yields DWRITE_E_NOFONT */
        l = SENT(IDWriteFontList2 *); CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, L"No Such Family Zq", v, 1, &l), EH_NOFONT); CHECK_OUT_NULL(l);
        l = SENT(IDWriteFontList2 *); CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, L"", NULL, 0, &l), EH_NOFONT); CHECK_OUT_NULL(l);
        l = NULL; CHECK_HR(IDWriteFontCollection2_GetMatchingFonts(c2, L"DwTest", v, 1, &l), S_OK); DW_RELEASE(l);
        DW_RELEASE(c2);
    } else t_skip("IDWriteFontCollection2 not available");
    END_COLL(x);
}

/* ---- font sets ----------------------------------------------------------------------------------------------------------------------- */
static IDWriteFontSet *set_of(cfix *x) {
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *s = NULL;
    if (SUCCEEDED(IDWriteFontCollection_QueryInterface(x->c, &IID_IDWriteFontCollection1, (void **)&c1)) && c1) { IDWriteFontCollection1_GetFontSet(c1, &s); IDWriteFontCollection1_Release(c1); }
    return s;
}
#define NEED_SET(x, s) NEED_COLL(x); IDWriteFontSet *s = set_of(&x); if (!s) { cfix_close(&x); DONE_FACTORY(); t_skip("no font set"); return; }
#define END_SET(x, s) DW_RELEASE(s); END_COLL(x)
TEST(fontset_findfontface_null_face) {
    NEED_SET(x, s); UINT32 idx = 55; BOOL ex = 7;
    CHECK_HR(IDWriteFontSet_FindFontFace(s, NULL, &idx, &ex), E_INVALIDARG); EXPECT_EQ(idx, 0xFFFFFFFFu); EXPECT_EQ(ex, 0);
    END_SET(x, s);
}
TEST(fontset_findfontface_foreign_face_is_not_an_error) {
    NEED_SET(x, s); dwx_font h; memset(&h, 0, sizeof h); size_t n; uint8_t *b = ttf_build_styled("Other", 400, 5, 0, &n); UINT32 idx = 55; BOOL ex = 7;
    if (b && SUCCEEDED(dwx_font_open(&h, x.f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE)) && h.face) {
        CHECK_HR(IDWriteFontSet_FindFontFace(s, h.face, &idx, &ex), S_OK); EXPECT_EQ(idx, 0xFFFFFFFFu); EXPECT_EQ(ex, 0);
    }
    dwx_font_close(&h); free(b); END_SET(x, s);
}
TEST(fontset_getpropertyvalues_null_locale) {
    NEED_SET(x, s); IDWriteStringList *l = SENT(IDWriteStringList *);
    CHECK_HR(IDWriteFontSet_GetPropertyValues_(s, DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, NULL, &l), E_INVALIDARG); CHECK_OUT_NULL(l);
    l = NULL; CHECK_HR(IDWriteFontSet_GetPropertyValues_(s, DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, L"en-us", &l), S_OK); DW_RELEASE(l);
    l = NULL; CHECK_HR(IDWriteFontSet_GetPropertyValues__(s, DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, &l), S_OK); DW_RELEASE(l);
    END_SET(x, s);
}
TEST(fontset_getpropertyvalues_index_form_is_lenient) {
    /* the (index, id) form reports "no such property" through *exists instead of failing; out parameters are cleared first */
    NEED_SET(x, s); BOOL ex = 7; IDWriteLocalizedStrings *v = SENT(IDWriteLocalizedStrings *);
    CHECK_HR(IDWriteFontSet_GetPropertyValues(s, 0, DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, &ex, &v), S_OK); CHECK(ex); DW_RELEASE(v);
    ex = 7; v = SENT(IDWriteLocalizedStrings *);
    CHECK_HR(IDWriteFontSet_GetPropertyValues(s, 0, DWRITE_FONT_PROPERTY_ID_DESIGN_SCRIPT_LANGUAGE_TAG, &ex, &v), S_OK);
    t_note("design script language tag exists=%d", ex);
    DW_RELEASE(v);
    END_SET(x, s);
}
TEST(fontset1_filtered_fonts_null_arrays) {
    NEED_SET(x, s); IDWriteFontSet1 *s1 = NULL;
    if (SUCCEEDED(IDWriteFontSet_QueryInterface(s, &IID_IDWriteFontSet1, (void **)&s1)) && s1) {
        IDWriteFontSet1 *out = SENT(IDWriteFontSet1 *);
        CHECK_HR(IDWriteFontSet1_GetFilteredFonts__(s1, NULL, 1, &out), E_INVALIDARG); CHECK_OUT_NULL(out);
        out = SENT(IDWriteFontSet1 *); CHECK_HR(IDWriteFontSet1_GetFilteredFonts_(s1, NULL, 1, TRUE, &out), E_INVALIDARG); CHECK_OUT_NULL(out);
        out = SENT(IDWriteFontSet1 *); CHECK_HR(IDWriteFontSet1_GetFilteredFonts(s1, NULL, 1, TRUE, &out), E_INVALIDARG); CHECK_OUT_NULL(out);
        out = NULL; CHECK_HR(IDWriteFontSet1_GetFilteredFonts__(s1, NULL, 0, &out), S_OK); if (out) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(out), 0); DW_RELEASE(out); }
        out = NULL; { UINT32 idx[1] = { 0 }; CHECK_HR(IDWriteFontSet1_GetFilteredFonts__(s1, idx, 1, &out), S_OK); if (out) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(out), 1); DW_RELEASE(out); } }
        DW_RELEASE(s1);
    } else t_skip("IDWriteFontSet1 not available");
    END_SET(x, s);
}
TEST(fontset1_filtered_font_indices_protocol) {
    NEED_SET(x, s); IDWriteFontSet1 *s1 = NULL;
    if (SUCCEEDED(IDWriteFontSet_QueryInterface(s, &IID_IDWriteFontSet1, (void **)&s1)) && s1) {
        UINT32 idx[4], actual = 55; memset(idx, 0xAA, sizeof idx);
        CHECK_HR(IDWriteFontSet1_GetFilteredFontIndices_(s1, NULL, 1, TRUE, idx, 4, &actual), E_INVALIDARG); EXPECT_EQ(actual, 0);
        EXPECT_EQ(idx[0], 0);                                                  /* destination zeroed before validation */
        actual = 55; memset(idx, 0xAA, sizeof idx);
        CHECK_HR(IDWriteFontSet1_GetFilteredFontIndices_(s1, NULL, 0, TRUE, idx, 4, &actual), S_OK);
        t_note("no ranges selects %u fonts", actual);
        UINT32 total = actual;
        if (total > 0) { UINT32 small[1]; actual = 55; CHECK_HR(IDWriteFontSet1_GetFilteredFontIndices_(s1, NULL, 0, TRUE, small, 0, &actual), EH_NSB); EXPECT_EQ(actual, total); }
        DW_RELEASE(s1);
    } else t_skip("IDWriteFontSet1 not available");
    END_SET(x, s);
}
TEST(fontset_getmatchingfonts_null_properties) {
    NEED_SET(x, s); IDWriteFontSet *out = SENT(IDWriteFontSet *);
    CHECK_HR(IDWriteFontSet_GetMatchingFonts(s, NULL, 2, &out), E_INVALIDARG); CHECK_OUT_NULL(out);
    out = NULL; CHECK_HR(IDWriteFontSet_GetMatchingFonts(s, NULL, 0, &out), S_OK); DW_RELEASE(out);
    END_SET(x, s);
}
TEST(fontset_matching_family_unknown_gives_empty_set) {
    NEED_SET(x, s); IDWriteFontSet *out = SENT(IDWriteFontSet *);
    CHECK_HR(IDWriteFontSet_GetMatchingFonts_(s, L"No Such Family Zq", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &out), S_OK);
    if (out && out != SENT(IDWriteFontSet *)) { EXPECT_EQ(IDWriteFontSet_GetFontCount(out), 0); DW_RELEASE(out); }
    END_SET(x, s);
}
TEST(fontset3_source_name_protocol) {
    NEED_SET(x, s); IDWriteFontSet3 *s3 = NULL;
    if (SUCCEEDED(IDWriteFontSet_QueryInterface(s, &IID_IDWriteFontSet3, (void **)&s3)) && s3) {
        WCHAR buf[64]; unsigned i;
        for (i = 0; i < 64; i++) buf[i] = 0x7777;
        CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, 0, buf, 0), EH_NSB); EXPECT_EQ(buf[0], 0x7777);                  /* size 0: untouched */
        CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, 99, buf, 0), EH_NSB);                                               /* size is checked before the index */
        buf[0] = 0x7777; CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, 99, buf, 64), E_INVALIDARG); EXPECT_EQ(buf[0], 0);   /* ArgumentException("listIndex") */
        CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, 0xFFFFFFFFu, buf, 64), E_INVALIDARG);
        UINT32 len = IDWriteFontSet3_GetFontSourceNameLength(s3, 0);
        CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, 0, buf, len + 1), S_OK); EXPECT_EQ(lstrlenW(buf), len);
        { UINT32 k; for (k = 1; k <= len; k++) { for (i = 0; i < 64; i++) buf[i] = 0x7777; CHECKF(IDWriteFontSet3_GetFontSourceName(s3, 0, buf, k) == EH_NSB, "capacity %u of %u", k, len + 1); CHECK(buf[k - 1] == 0); } }
        DW_RELEASE(s3);
    } else t_skip("IDWriteFontSet3 not available");
    END_SET(x, s);
}

/* ---- font set builders ---------------------------------------------------------------------------------------------------------------- */
TEST(fontsetbuilder_null_arguments) {
    if (t_is_wine()) { t_skip("NULL arguments are dereferenced by the emulated implementation"); return; }
    NEED_FACTORY(); IDWriteFactory3 *f3 = NULL; IDWriteFontSetBuilder *b = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) && f3 && SUCCEEDED(IDWriteFactory3_CreateFontSetBuilder(f3, &b)) && b) {
        CHECK_HR(IDWriteFontSetBuilder_AddFontFaceReference(b, NULL), E_INVALIDARG);
        CHECK_HR(IDWriteFontSetBuilder_AddFontSet(b, NULL), E_INVALIDARG);
        IDWriteFontSetBuilder1 *b1 = NULL;
        if (SUCCEEDED(IDWriteFontSetBuilder_QueryInterface(b, &IID_IDWriteFontSetBuilder1, (void **)&b1)) && b1) { CHECK_HR(IDWriteFontSetBuilder1_AddFontFile(b1, NULL), E_INVALIDARG); DW_RELEASE(b1); }
        IDWriteFontSet *set = SENT(IDWriteFontSet *);
        CHECK_HR(IDWriteFontSetBuilder_CreateFontSet(b, &set), S_OK);                          /* nothing was added: an empty set */
        if (set && set != SENT(IDWriteFontSet *)) { EXPECT_EQ(IDWriteFontSet_GetFontCount(set), 0); DW_RELEASE(set); }
        DW_RELEASE(b);
    } else t_skip("IDWriteFactory3 not available");
    DW_RELEASE(f3); DONE_FACTORY();
}
TEST(fontsetbuilder2_addfont_argument_matrix) {
    NEED_FACTORY(); IDWriteFactory5 *f5 = NULL; IDWriteFontSetBuilder1 *b1 = NULL; cfix x;
    if (!cfix_open(&x, f)) { cfix_close(&x); DONE_FACTORY(); t_skip("custom collection unavailable"); return; }
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, (void **)&f5)) && f5 && SUCCEEDED(IDWriteFactory5_CreateFontSetBuilder(f5, &b1)) && b1) {
        IDWriteFontSetBuilder2 *b2 = NULL; IDWriteFontFile *file = NULL; DWRITE_FONT_AXIS_VALUE av[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.f } };
        DWRITE_FONT_AXIS_RANGE ar[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 100.f, 900.f } }; DWRITE_FONT_PROPERTY pr[1] = { { DWRITE_FONT_PROPERTY_ID_FAMILY_NAME, L"DwTest", L"en-us" } };
        UINT32 key = x.files->nblobs ? 0 : 0;
        if (SUCCEEDED(IDWriteFontSetBuilder1_QueryInterface(b1, &IID_IDWriteFontSetBuilder2, (void **)&b2)) && b2 && SUCCEEDED(dwx_loader_file(x.files, f, key, &file)) && file) {
            CHECK_HR(IDWriteFontSetBuilder2_AddFont(b2, NULL, 0, 0, NULL, 0, NULL, 0, NULL, 0), E_INVALIDARG);                   /* file */
            CHECK_HR(IDWriteFontSetBuilder2_AddFont(b2, file, 0, (DWRITE_FONT_SIMULATIONS)4, NULL, 0, NULL, 0, NULL, 0), E_INVALIDARG);
            CHECK_HR(IDWriteFontSetBuilder2_AddFont(b2, file, 0, (DWRITE_FONT_SIMULATIONS)0xFFFFFFFFu, NULL, 0, NULL, 0, NULL, 0), E_INVALIDARG);
            CHECK_HR(IDWriteFontSetBuilder2_AddFont(b2, file, 0, 0, NULL, 1, NULL, 0, NULL, 0), E_INVALIDARG);                    /* axis values */
            CHECK_HR(IDWriteFontSetBuilder2_AddFont(b2, file, 0, 0, av, 0x10000, NULL, 0, NULL, 0), E_INVALIDARG);
            CHECK_HR(IDWriteFontSetBuilder2_AddFont(b2, file, 0, 0, NULL, 0, NULL, 1, NULL, 0), E_INVALIDARG);                    /* axis ranges */
            CHECK_HR(IDWriteFontSetBuilder2_AddFont(b2, file, 0, 0, NULL, 0, NULL, 0, NULL, 1), E_INVALIDARG);                    /* properties */
            { HRESULT hr = IDWriteFontSetBuilder2_AddFont(b2, file, 0, 0, NULL, 0, ar, 1, pr, 1); t_note("AddFont with explicit ranges and properties = %s", dw_hr_name(hr)); CHECK(hr != E_INVALIDARG); }
            DW_RELEASE(file);
        } else t_skip("IDWriteFontSetBuilder2 not available");
        DW_RELEASE(b2); DW_RELEASE(b1);
    } else t_skip("IDWriteFactory5 not available");
    DW_RELEASE(f5); cfix_close(&x); DONE_FACTORY();
}

/* ---- font fallback ---------------------------------------------------------------------------------------------------------------------- */
TEST(fallback_builder_addmapping_arguments) {
    NEED_FACTORY(); IDWriteFactory2 *f2 = NULL; IDWriteFontFallbackBuilder *b = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2)) && f2 && SUCCEEDED(IDWriteFactory2_CreateFontFallbackBuilder(f2, &b)) && b) {
        DWRITE_UNICODE_RANGE rg[1] = { { 0x41, 0x5A } }; const WCHAR *fam[1] = { L"Arial" }; const WCHAR *famnull[2] = { L"Arial", NULL };
        CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, NULL, 1, fam, 1, NULL, NULL, NULL, 1.f), E_INVALIDARG);
        CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 0, fam, 1, NULL, NULL, NULL, 1.f), E_INVALIDARG);
        CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 1, NULL, 1, NULL, NULL, NULL, 1.f), E_INVALIDARG);
        CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 1, fam, 0, NULL, NULL, NULL, 1.f), E_INVALIDARG);
        CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 1, fam, 1, NULL, NULL, NULL, -1.f), E_INVALIDARG);
        CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 1, famnull, 2, NULL, NULL, NULL, 1.f), E_INVALIDARG);          /* a NULL family name inside the list */
        CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 1, fam, 1, NULL, NULL, NULL, 1.f), S_OK);
        CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 1, fam, 1, NULL, L"en-us", L"Segoe UI", 0.f), S_OK);          /* scale 0 is allowed */
        IDWriteFontFallback *fb = NULL; CHECK_HR(IDWriteFontFallbackBuilder_CreateFontFallback(b, &fb), S_OK); DW_RELEASE(fb);
        DW_RELEASE(b);
    } else t_skip("IDWriteFactory2 not available");
    DW_RELEASE(f2); DONE_FACTORY();
}
TEST(fallback_builder_addmappings_null) {
    NEED_FACTORY(); IDWriteFactory2 *f2 = NULL; IDWriteFontFallbackBuilder *b = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2)) && f2 && SUCCEEDED(IDWriteFactory2_CreateFontFallbackBuilder(f2, &b)) && b) {
        if (!t_is_wine()) CHECK_HR(IDWriteFontFallbackBuilder_AddMappings(b, NULL), E_INVALIDARG);
        DW_RELEASE(b);
    } else t_skip("IDWriteFactory2 not available");
    DW_RELEASE(f2); DONE_FACTORY();
}
TEST(fallback_mapcharacters_arguments) {
    NEED_FACTORY(); IDWriteFactory2 *f2 = NULL; IDWriteFontFallback *fb = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2)) && f2 && SUCCEEDED(IDWriteFactory2_GetSystemFontFallback(f2, &fb)) && fb) {
        static const WCHAR txt[] = L"Hello"; dwx_ana_src *src = dwx_ana_src_new(txt, 5); IDWriteTextAnalysisSource *s = (IDWriteTextAnalysisSource *)src;
        struct { int null_src; UINT32 pos, len; DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; DWRITE_FONT_STRETCH sx; const char *why; } rows[] = {
            { 1, 0, 5, 400, 0, 5, "source NULL" }, { 0, 0xFFFFFFFFu, 1, 400, 0, 5, "range wraps" }, { 0, 3, 0xFFFFFFFFu, 400, 0, 5, "length -1" },
            { 0, 0, 5, 0, 0, 5, "weight 0" }, { 0, 0, 5, 1000, 0, 5, "weight 1000" }, { 0, 0, 5, 400, 3, 5, "style 3" }, { 0, 0, 5, 400, 0, 0, "stretch 0" }, { 0, 0, 5, 400, 0, 10, "stretch 10" },
        };
        unsigned i;
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            if (t_is_wine() && rows[i].null_src) continue;
            UINT32 ml = 55; IDWriteFont *font = SENT(IDWriteFont *); FLOAT scale = 9.f;
            HRESULT hr = IDWriteFontFallback_MapCharacters(fb, rows[i].null_src ? NULL : s, rows[i].pos, rows[i].len, NULL, NULL, rows[i].w, rows[i].st, rows[i].sx, &ml, &font, &scale);
            CHECKF(hr == E_INVALIDARG && ml == 0 && font == NULL && scale == 1.f, "%s -> %s len=%u font=%p scale=%g", rows[i].why, dw_hr_name(hr), ml, font, scale);
            if (font && font != SENT(IDWriteFont *)) DW_RELEASE(font);
        }
        dwx_release(src); DW_RELEASE(fb);
    } else t_skip("IDWriteFactory2 not available");
    DW_RELEASE(f2); DONE_FACTORY();
}

/* ---- rendering side objects ----------------------------------------------------------------------------------------------------------------- */
TEST(glyph_run_analysis_method_arguments) {
    NEED_FACTORY(); NEED_FACE(h, f); UINT16 idx[2] = { 2, 3 }; float adv[2] = { 12, 12 }; DWRITE_GLYPH_RUN run; IDWriteGlyphRunAnalysis *a = NULL; RECT rc; BYTE px[4096];
    memset(&run, 0, sizeof run); run.fontFace = h.face; run.fontEmSize = 16; run.glyphCount = 2; run.glyphIndices = idx; run.glyphAdvances = adv;
    if (SUCCEEDED(IDWriteFactory_CreateGlyphRunAnalysis(f, &run, 1.f, NULL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL, 0.f, 0.f, &a)) && a) {
        int t;
        for (t = 2; t < 6; t++) {
            memset(&rc, 0x55, sizeof rc);
            CHECKF(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, (DWRITE_TEXTURE_TYPE)t, &rc) == E_INVALIDARG, "texture type %d", t);
            CHECKF(rc.left == 0 && rc.top == 0 && rc.right == 0 && rc.bottom == 0, "texture type %d: rect not cleared", t);
        }
        memset(&rc, 0x55, sizeof rc); CHECK_HR(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, (DWRITE_TEXTURE_TYPE)-1, &rc), E_INVALIDARG);
        CHECK_HR(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc), S_OK);
        CHECK_HR(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc), S_OK);
        if (!t_is_wine()) {
            CHECK_HR(IDWriteGlyphRunAnalysis_CreateAlphaTexture(a, DWRITE_TEXTURE_ALIASED_1x1, NULL, px, sizeof px), E_INVALIDARG);
        }
        DW_RELEASE(a);
    }
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(bitmap_render_target_argument_validation) {
    NEED_FACTORY(); IDWriteGdiInterop *g = NULL; IDWriteBitmapRenderTarget *t = NULL; HDC dc = CreateCompatibleDC(NULL);
    if (dc && SUCCEEDED(IDWriteFactory_GetGdiInterop(f, &g)) && g && SUCCEEDED(IDWriteGdiInterop_CreateBitmapRenderTarget(g, dc, 64, 64, &t)) && t) {
        static const float bad[] = { 0.f, -1.f, -0.f, -INF_F }; unsigned i; float ppd = IDWriteBitmapRenderTarget_GetPixelsPerDip(t);
        for (i = 0; i < 4; i++) { CHECKF(IDWriteBitmapRenderTarget_SetPixelsPerDip(t, bad[i]) == E_INVALIDARG, "pixelsPerDip %g", bad[i]); CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(t), ppd); }
        CHECK_HR(IDWriteBitmapRenderTarget_SetPixelsPerDip(t, 2.f), S_OK); CHECK_FLOAT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(t), 2.f);
        DWRITE_MATRIX nm = { 1, 0, 0, 1, NAN_F, 0 }, ok = { 2, 0, 0, 2, 5, 5 }, got;
        CHECK_HR(IDWriteBitmapRenderTarget_SetCurrentTransform(t, &nm), E_INVALIDARG);
        nm.dx = 0; nm.m22 = NAN_F; CHECK_HR(IDWriteBitmapRenderTarget_SetCurrentTransform(t, &nm), E_INVALIDARG);
        CHECK_HR(IDWriteBitmapRenderTarget_SetCurrentTransform(t, &ok), S_OK);
        CHECK_HR(IDWriteBitmapRenderTarget_GetCurrentTransform(t, &got), S_OK); CHECK_FLOAT_EQ(got.m11, 2.f); CHECK_FLOAT_EQ(got.dx, 5.f);
        CHECK_HR(IDWriteBitmapRenderTarget_SetCurrentTransform(t, NULL), S_OK);                        /* NULL resets to identity */
        CHECK_HR(IDWriteBitmapRenderTarget_GetCurrentTransform(t, &got), S_OK); CHECK_FLOAT_EQ(got.m11, 1.f); CHECK_FLOAT_EQ(got.dx, 0.f);
        IDWriteBitmapRenderTarget1 *t1 = NULL;
        if (SUCCEEDED(IDWriteBitmapRenderTarget_QueryInterface(t, &IID_IDWriteBitmapRenderTarget1, (void **)&t1)) && t1) {
            CHECK_HR(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(t1, (DWRITE_TEXT_ANTIALIAS_MODE)2), E_INVALIDARG);
            CHECK_HR(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(t1, (DWRITE_TEXT_ANTIALIAS_MODE)-1), E_INVALIDARG);
            CHECK_HR(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(t1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE), S_OK);
            EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(t1), DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            CHECK_HR(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(t1, (DWRITE_TEXT_ANTIALIAS_MODE)7), E_INVALIDARG);
            EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(t1), DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            DW_RELEASE(t1);
        }
    } else t_skip("bitmap render target unavailable");
    DW_RELEASE(t); DW_RELEASE(g); if (dc) DeleteDC(dc); DONE_FACTORY();
}

/* ---- font resource, axis values, unicode ranges --------------------------------------------------------------------------------------------------- */
TEST(fontresource_createface_and_reference_arguments) {
    if (t_is_wine()) { t_skip("invalid arguments fault in the emulated implementation"); return; }
    NEED_FACTORY(); IDWriteFactory6 *f6 = NULL; cfix x;
    if (!cfix_open(&x, f)) { cfix_close(&x); DONE_FACTORY(); t_skip("custom collection unavailable"); return; }
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory6, (void **)&f6)) && f6) {
        IDWriteFontFile *file = NULL; IDWriteFontResource *res = NULL; DWRITE_FONT_AXIS_VALUE av[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.f } };
        if (SUCCEEDED(dwx_loader_file(x.files, f, 0, &file)) && file && SUCCEEDED(IDWriteFactory6_CreateFontResource(f6, file, 0, &res)) && res) {
            IDWriteFontFace5 *face = SENT(IDWriteFontFace5 *); IDWriteFontFaceReference1 *ref = SENT(IDWriteFontFaceReference1 *);
            CHECK_HR(IDWriteFontResource_CreateFontFace(res, (DWRITE_FONT_SIMULATIONS)4, NULL, 0, &face), E_INVALIDARG); CHECK_OUT_NULL(face);
            face = SENT(IDWriteFontFace5 *); CHECK_HR(IDWriteFontResource_CreateFontFace(res, (DWRITE_FONT_SIMULATIONS)0xFFFFFFFFu, NULL, 0, &face), E_INVALIDARG); CHECK_OUT_NULL(face);
            face = SENT(IDWriteFontFace5 *); CHECK_HR(IDWriteFontResource_CreateFontFace(res, 0, NULL, 1, &face), E_INVALIDARG); CHECK_OUT_NULL(face);
            face = SENT(IDWriteFontFace5 *); CHECK_HR(IDWriteFontResource_CreateFontFace(res, 0, av, 0x10000, &face), E_INVALIDARG); CHECK_OUT_NULL(face);
            CHECK_HR(IDWriteFontResource_CreateFontFaceReference(res, (DWRITE_FONT_SIMULATIONS)4, NULL, 0, &ref), E_INVALIDARG); CHECK_OUT_NULL(ref);
            ref = SENT(IDWriteFontFaceReference1 *); CHECK_HR(IDWriteFontResource_CreateFontFaceReference(res, 0, NULL, 1, &ref), E_INVALIDARG); CHECK_OUT_NULL(ref);
            ref = SENT(IDWriteFontFaceReference1 *); CHECK_HR(IDWriteFontResource_CreateFontFaceReference(res, 0, av, 0x10000, &ref), E_INVALIDARG); CHECK_OUT_NULL(ref);
            face = NULL; { HRESULT hr = IDWriteFontResource_CreateFontFace(res, 0, NULL, 0, &face); t_note("CreateFontFace(no axes) = %s", dw_hr_name(hr)); CHECK(hr != E_INVALIDARG); DW_RELEASE(face); }
            ref = NULL; { HRESULT hr = IDWriteFontResource_CreateFontFaceReference(res, 3, NULL, 0, &ref); CHECK_HR(hr, S_OK); DW_RELEASE(ref); }
            DW_RELEASE(res);
        } else t_skip("font resource unavailable");
        DW_RELEASE(file); DW_RELEASE(f6);
    } else t_skip("IDWriteFactory6 not available");
    cfix_close(&x); DONE_FACTORY();
}
TEST(face_axis_values_buffer_protocol_variable_font) {
    NEED_FACTORY(); IDWriteFontFace *face = dw_sysface(f, L"Bahnschrift", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace5 *f5 = NULL;
    if (face && SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace5, (void **)&f5)) && f5) {
        UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(f5); DWRITE_FONT_AXIS_VALUE v[16]; unsigned i;
        t_note("Bahnschrift axis value count = %u", n);
        if (n >= 1 && n <= 16) {
            CHECK_HR(IDWriteFontFace5_GetFontAxisValues(f5, v, n), S_OK);
            for (i = 0; i < n; i++) CHECKF(IDWriteFontFace5_GetFontAxisValues(f5, v, i) == EH_NSB, "capacity %u of %u", i, n);
            CHECK_HR(IDWriteFontFace5_GetFontAxisValues(f5, v, 16), S_OK);
        } else t_skip("not a variable font on this host");
        DW_RELEASE(f5);
    } else t_skip("Bahnschrift not installed or IDWriteFontFace5 unavailable");
    DW_RELEASE(face); DONE_FACTORY();
}
TEST(face_unicode_ranges_buffer_protocol) {
    NEED_FACTORY(); NEED_FACE(h, f); IDWriteFontFace1 *f1 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        DWRITE_UNICODE_RANGE r[8]; UINT32 actual = 55; size_t i;
        /* the synthetic cmap maps U+0020, U+0030-0039, U+0041-005A and U+0061-007A: four ranges */
        CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 0, NULL, &actual), EH_NSB); EXPECT_EQ(actual, 4);
        actual = 55; CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 3, NULL, &actual), E_INVALIDARG); EXPECT_EQ(actual, 0);
        memset(r, 0xAA, sizeof r); actual = 55;
        CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 2, r, &actual), EH_NSB); EXPECT_EQ(actual, 4);
        EXPECT_EQ(r[0].first, 0x20); EXPECT_EQ(r[0].last, 0x20); EXPECT_EQ(r[1].first, 0x30); EXPECT_EQ(r[1].last, 0x39);
        memset(r, 0xAA, sizeof r); actual = 55;
        CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 4, r, &actual), S_OK); EXPECT_EQ(actual, 4);
        EXPECT_EQ(r[2].first, 0x41); EXPECT_EQ(r[2].last, 0x5A); EXPECT_EQ(r[3].first, 0x61); EXPECT_EQ(r[3].last, 0x7A);
        memset(r, 0xAA, sizeof r); actual = 55;
        CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(f1, 8, r, &actual), S_OK); EXPECT_EQ(actual, 4);
        for (i = 4; i < 8; i++) EXPECT_EQ(r[i].first, 0);                  /* the whole destination is zeroed first, unused entries stay 0 */
        DW_RELEASE(f1);
    } else t_skip("IDWriteFontFace1 not available");
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(font_unicode_ranges_buffer_protocol) {
    NEED_COLL(x); IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFont1 *font1 = NULL;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(x.c, 0, &fam)) && fam && SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) && font &&
        SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont1, (void **)&font1)) && font1) {
        DWRITE_UNICODE_RANGE r[8]; UINT32 actual = 55;
        CHECK_HR(IDWriteFont1_GetUnicodeRanges(font1, 0, NULL, &actual), EH_NSB); EXPECT_EQ(actual, 4);
        actual = 55; CHECK_HR(IDWriteFont1_GetUnicodeRanges(font1, 2, NULL, &actual), E_INVALIDARG);
        actual = 55; memset(r, 0xAA, sizeof r); CHECK_HR(IDWriteFont1_GetUnicodeRanges(font1, 3, r, &actual), EH_NSB); EXPECT_EQ(actual, 4);
        actual = 55; CHECK_HR(IDWriteFont1_GetUnicodeRanges(font1, 8, r, &actual), S_OK); EXPECT_EQ(actual, 4);
    } else t_skip("IDWriteFont1 not available");
    DW_RELEASE(font1); DW_RELEASE(font); DW_RELEASE(fam); END_COLL(x);
}
