/* Font sets: ABI constants, custom collection loader interplay, strings in supplied properties, set composition, lifetime across
 * loader unregistration, local-file references, concurrency, and per-font sweeps of the system set. */
#include "fs.h"

/* ===== ABI / constants =========================================================================================== */
TEST(fset_abi_property_id_values)
{
    /* values are part of the documented ABI and index the per-font name dictionaries (ids 0..14 accepted by the implementation) */
    CHECK(DWRITE_FONT_PROPERTY_ID_NONE == 0);
    CHECK(DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME == 1);
    CHECK(DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME == 2);
    CHECK(DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME == 3);
    CHECK(DWRITE_FONT_PROPERTY_ID_FULL_NAME == 4);
    CHECK(DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME == 5);
    CHECK(DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME == 6);
    CHECK(DWRITE_FONT_PROPERTY_ID_DESIGN_SCRIPT_LANGUAGE_TAG == 7);
    CHECK(DWRITE_FONT_PROPERTY_ID_SUPPORTED_SCRIPT_LANGUAGE_TAG == 8);
    CHECK(DWRITE_FONT_PROPERTY_ID_SEMANTIC_TAG == 9);
    CHECK(DWRITE_FONT_PROPERTY_ID_WEIGHT == 10);
    CHECK(DWRITE_FONT_PROPERTY_ID_STRETCH == 11);
    CHECK(DWRITE_FONT_PROPERTY_ID_STYLE == 12);
    CHECK(DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME == 13);
    CHECK(DWRITE_FONT_PROPERTY_ID_FAMILY_NAME == DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME);
    CHECK(DWRITE_FONT_PROPERTY_ID_PREFERRED_FAMILY_NAME == DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
    CHECK(DWRITE_FONT_PROPERTY_ID_FACE_NAME == DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME);
    CHECK(DWRITE_FONT_PROPERTY_ID_TOTAL == 13);
    CHECK(DWRITE_FONT_PROPERTY_ID_TOTAL_RS3 == 14);
}

TEST(fset_abi_struct_layout)
{
    /* the implementation strides arrays of these by 0x18, 8 and 0xC bytes */
    CHECK(sizeof(DWRITE_FONT_PROPERTY) == 24);
    CHECK(offsetof(DWRITE_FONT_PROPERTY, propertyId) == 0);
    CHECK(offsetof(DWRITE_FONT_PROPERTY, propertyValue) == 8);
    CHECK(offsetof(DWRITE_FONT_PROPERTY, localeName) == 16);
    CHECK(sizeof(DWRITE_FONT_AXIS_VALUE) == 8);
    CHECK(offsetof(DWRITE_FONT_AXIS_VALUE, value) == 4);
    CHECK(sizeof(DWRITE_FONT_AXIS_RANGE) == 12);
    CHECK(offsetof(DWRITE_FONT_AXIS_RANGE, minValue) == 4);
    CHECK(offsetof(DWRITE_FONT_AXIS_RANGE, maxValue) == 8);
}

TEST(fset_abi_axis_tags_and_source_types)
{
    CHECK((UINT32)DWRITE_FONT_AXIS_TAG_WEIGHT == 0x74686777u);        /* 'wght' */
    CHECK((UINT32)DWRITE_FONT_AXIS_TAG_WIDTH == 0x68746477u);         /* 'wdth' */
    CHECK((UINT32)DWRITE_FONT_AXIS_TAG_SLANT == 0x746e6c73u);         /* 'slnt' */
    CHECK((UINT32)DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE == 0x7a73706fu);  /* 'opsz' */
    CHECK((UINT32)DWRITE_FONT_AXIS_TAG_ITALIC == 0x6c617469u);        /* 'ital' */
    CHECK(DWRITE_FONT_SOURCE_TYPE_UNKNOWN == 0 && DWRITE_FONT_SOURCE_TYPE_PER_MACHINE == 1 && DWRITE_FONT_SOURCE_TYPE_PER_USER == 2);
    CHECK(DWRITE_FONT_SOURCE_TYPE_APPX_PACKAGE == 3 && DWRITE_FONT_SOURCE_TYPE_REMOTE_FONT_PROVIDER == 4);
    CHECK(DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC == 0 && DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE == 1);
    CHECK(DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE == 1 && DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN == 2);
}

TEST(fset_abi_vtable_slot_order)
{
    /* method order of IDWriteFontSet as the implementation lays it out: calling by name through the header's vtable must hit the right slot */
    FS_STD(s);
    UINT32 n = IDWriteFontSet_GetFontCount(s.set);
    CHECK(n == 5);
    IDWriteFontFaceReference *r = NULL; UINT32 idx = 99; BOOL ex = FALSE;
    CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(s.set, 0, &r));
    if (r) {
        CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(s.set, r, &idx, &ex));
        CHECK(ex && idx == 0);
        DW_RELEASE(r);
    }
    IDWriteStringList *l = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues__(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT, &l));       /* (id, list) */
    if (l) { CHECK(fs_sl_has(l, "400")); DW_RELEASE(l); }
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues_(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT, L"en-us", &l));   /* (id, locales, list) */
    if (l) { CHECK(fs_sl_has(l, "700")); DW_RELEASE(l); }
    IDWriteLocalizedStrings *ls = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(s.set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT, &ex, &ls));   /* (index, id, exists, strings) */
    CHECK(ex && ls);
    DW_RELEASE(ls);
    fs_std_close(&s);
}

/* ===== custom collection loader -> font set ======================================================================= */
typedef struct cc_fx { fs_fx x; dwx_coll_loader *cl; IDWriteFontCollection *coll; int ok; } cc_fx;
static int cc_open(cc_fx *c, const char *const *fams, int nfam)
{
    int i; HRESULT hr;
    memset(c, 0, sizeof *c);
    if (!fs_open(&c->x)) return 0;
    c->cl = dwx_coll_new(c->x.fl);
    for (i = 0; i < nfam; i++) { UINT32 k = fs_add(&c->x, fams[i], 400, 5, 0); dwx_coll_add_key(c->cl, k); }
    hr = dwx_coll_register(c->cl, c->x.f);
    if (FAILED(hr)) { t_skip("RegisterFontCollectionLoader failed: %s", dw_hr_name(hr)); dwx_release(c->cl); fs_close(&c->x); return 0; }
    hr = dwx_coll_create(c->cl, c->x.f, &c->coll);
    if (FAILED(hr) || !c->coll) { t_skip("CreateCustomFontCollection failed: %s", dw_hr_name(hr)); dwx_coll_unregister(c->cl); dwx_release(c->cl); fs_close(&c->x); return 0; }
    c->ok = 1;
    return 1;
}
static void cc_close(cc_fx *c)
{
    if (!c->ok) return;
    DW_RELEASE(c->coll);
    dwx_coll_unregister(c->cl);
    dwx_release(c->cl);
    fs_close(&c->x);
    c->ok = 0;
}

TEST(fset_cc_collection_get_font_set)
{
    static const char *fams[] = { "CustA", "CustB", "CustC" };
    cc_fx c;
    if (!cc_open(&c, fams, 3)) return;
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *set = (IDWriteFontSet *)FS_SENT;
    if (SUCCEEDED(IDWriteFontCollection_QueryInterface(c.coll, &IID_IDWriteFontCollection1, (void **)&c1)) && c1) {
        HRESULT hr = IDWriteFontCollection1_GetFontSet(c1, &set);
        CHECK_HR_OK(hr);
        if (SUCCEEDED(hr) && set) {
            CHECKF(IDWriteFontSet_GetFontCount(set) == 3, "count %u", IDWriteFontSet_GetFontCount(set));
            IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
            if (l) { CHECK(IDWriteStringList_GetCount(l) == 3); CHECK(fs_sl_has(l, "CustA") && fs_sl_has(l, "CustB") && fs_sl_has(l, "CustC")); DW_RELEASE(l); }
        }
        FS_DROP(set, hr);
        DW_RELEASE(c1);
    } else t_skip("custom collection does not expose IDWriteFontCollection1");
    cc_close(&c);
}

TEST(fset_cc_set_to_collection_roundtrip)
{
    static const char *fams[] = { "RtA", "RtB" };
    cc_fx c;
    if (!cc_open(&c, fams, 2)) return;
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *set = NULL;
    if (SUCCEEDED(IDWriteFontCollection_QueryInterface(c.coll, &IID_IDWriteFontCollection1, (void **)&c1)) && c1 && SUCCEEDED(IDWriteFontCollection1_GetFontSet(c1, &set)) && set) {
        IDWriteFontCollection1 *again = NULL;
        CHECK_HR_OK(IDWriteFactory3_CreateFontCollectionFromFontSet(c.x.f3, set, &again));
        if (again) {
            IDWriteFontCollection *a = (IDWriteFontCollection *)again;
            CHECKF(IDWriteFontCollection_GetFontFamilyCount(a) == IDWriteFontCollection_GetFontFamilyCount(c.coll), "families %u vs %u",
                   IDWriteFontCollection_GetFontFamilyCount(a), IDWriteFontCollection_GetFontFamilyCount(c.coll));
            UINT32 i;
            for (i = 0; i < IDWriteFontCollection_GetFontFamilyCount(c.coll); i++) {
                IDWriteFontFamily *f = NULL; IDWriteLocalizedStrings *n = NULL; WCHAR nm[64] = { 0 };
                if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c.coll, i, &f)) && f && SUCCEEDED(IDWriteFontFamily_GetFamilyNames(f, &n)) && n) {
                    UINT32 idx = 0; BOOL ex = FALSE;
                    IDWriteLocalizedStrings_GetString(n, 0, nm, 64);
                    IDWriteFontCollection_FindFamilyName(a, nm, &idx, &ex);
                    CHECKF(ex == TRUE, "family %ls present after the round trip", nm);
                }
                DW_RELEASE(n); DW_RELEASE(f);
            }
            DW_RELEASE(again);
        }
    } else t_skip("setup");
    DW_RELEASE(set); DW_RELEASE(c1);
    cc_close(&c);
}

TEST(fset_cc_set_item_refs_match_collection_faces)
{
    static const char *fams[] = { "FaceA", "FaceB" };
    cc_fx c;
    if (!cc_open(&c, fams, 2)) return;
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *set = NULL;
    if (SUCCEEDED(IDWriteFontCollection_QueryInterface(c.coll, &IID_IDWriteFontCollection1, (void **)&c1)) && c1 && SUCCEEDED(IDWriteFontCollection1_GetFontSet(c1, &set)) && set) {
        UINT32 fi;
        for (fi = 0; fi < IDWriteFontCollection_GetFontFamilyCount(c.coll); fi++) {
            IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c.coll, fi, &fam)) && fam && SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) && font
                && IDWriteFont_GetSimulations(font) == 0 && SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
                UINT32 idx = 0; BOOL ex = FALSE;
                CHECK_HR_OK(IDWriteFontSet_FindFontFace(set, face, &idx, &ex));
                CHECKF(ex == TRUE, "face of family %u is an item of the collection's font set", fi);
            }
            DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam);
        }
    } else t_skip("setup");
    DW_RELEASE(set); DW_RELEASE(c1);
    cc_close(&c);
}

TEST(fset_cc_loader_call_counts_note)
{
    static const char *fams[] = { "CntA", "CntB" };
    cc_fx c;
    if (!cc_open(&c, fams, 2)) return;
    LONG creates = c.cl->n_create;
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *set = NULL;
    if (SUCCEEDED(IDWriteFontCollection_QueryInterface(c.coll, &IID_IDWriteFontCollection1, (void **)&c1)) && c1) {
        CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c1, &set));
        /* the set is derived from data already loaded for the collection: the collection loader is not asked again */
        CHECKF(c.cl->n_create == creates, "CreateEnumeratorFromKey calls %ld -> %ld", (long)creates, (long)c.cl->n_create);
        DW_RELEASE(set);
        DW_RELEASE(c1);
    }
    FS_SOFT(creates == 1);
    cc_close(&c);
}

/* ===== strings carried in supplied properties ======================================================================== */
static void prop_roundtrip(const WCHAR *value, const char *what)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "StrFam", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        DWRITE_FONT_PROPERTY p; IDWriteFontSet *set = NULL;
        p.propertyId = DWRITE_FONT_PROPERTY_ID_FULL_NAME; p.propertyValue = value; p.localeName = L"en-us";
        HRESULT hr = IDWriteFontSetBuilder_AddFontFaceReference_(b, r, &p, 1);
        CHECKF(hr == S_OK, "%s: add -> %s", what, dw_hr_name(hr));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            BOOL ex = FALSE; IDWriteLocalizedStrings *ls = NULL;
            CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &ex, &ls));
            CHECKF(ex && ls, "%s: value present", what);
            if (ls) {
                WCHAR got[2048]; UINT32 len = 0;
                CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(ls, 0, &len));
                CHECKF(len == wcslen(value), "%s: length %u vs %u", what, len, (unsigned)wcslen(value));
                if (len < 2047) {
                    CHECK_HR_OK(IDWriteLocalizedStrings_GetString(ls, 0, got, 2048));
                    CHECKF(!wcscmp(got, value), "%s: value mismatch", what);
                }
                DW_RELEASE(ls);
            }
            fs_prop q; (void)q;
            DWRITE_FONT_PROPERTY probe = p; UINT32 c = 0xDEAD;
            CHECK_HR_OK(IDWriteFontSet_GetPropertyOccurrenceCount(set, &probe, &c));
            CHECKF(c == 1, "%s: occurrence count %u", what, c);
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_str_prop_unicode_value)
{
    static const WCHAR v[] = { 0x00DC, 0x006E, 0x00EF, 0x0020, 0x0046, 0x00E1, 0x006D, 0x0020, 0x65E5, 0x672C, 0x8A9E, 0x0020, 0xD83D, 0xDE00, 0 };
    prop_roundtrip(v, "unicode");
}
TEST(fset_str_prop_long_value)
{
    static WCHAR v[1025]; int i;
    for (i = 0; i < 1024; i++) v[i] = (WCHAR)('a' + i % 26);
    v[1024] = 0;
    prop_roundtrip(v, "1024 characters");
}
TEST(fset_str_prop_single_char_value) { prop_roundtrip(L"x", "one character"); }
TEST(fset_str_prop_spaces_and_punctuation) { prop_roundtrip(L"  lead; trail ,: \"quoted\" \\ /  ", "spaces and punctuation"); }

TEST(fset_str_prop_empty_value)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "EmptyVal", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        DWRITE_FONT_PROPERTY p = { DWRITE_FONT_PROPERTY_ID_FULL_NAME, L"", L"en-us" };
        HRESULT hr = IDWriteFontSetBuilder_AddFontFaceReference_(b, r, &p, 1);
        t_note("empty propertyValue -> %s", dw_hr_name(hr));
        CHECKF(SUCCEEDED(hr), "empty value is a valid (non-NULL) string");
        if (SUCCEEDED(hr)) {
            IDWriteFontSet *set = NULL;
            CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
            if (set) { CHECK(IDWriteFontSet_GetFontCount(set) == 1); DW_RELEASE(set); }
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_str_prop_value_buffer_not_retained)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "CopyFam", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        WCHAR val[32], loc[16]; DWRITE_FONT_PROPERTY p; IDWriteFontSet *set = NULL;
        wcscpy(val, L"Original Value"); wcscpy(loc, L"en-us");
        p.propertyId = DWRITE_FONT_PROPERTY_ID_FULL_NAME; p.propertyValue = val; p.localeName = loc;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, &p, 1));
        /* the builder copied the strings: clobbering the caller's buffers changes nothing */
        wcscpy(val, L"XXXXXXXXXXXXXX"); wcscpy(loc, L"zz");
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            char buf[64];
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, buf, sizeof buf) && !strcmp(buf, "Original Value"));
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_str_prop_script_tags_note)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "TagFam", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        DWRITE_FONT_PROPERTY p[3]; IDWriteFontSet *set = NULL;
        p[0].propertyId = DWRITE_FONT_PROPERTY_ID_DESIGN_SCRIPT_LANGUAGE_TAG; p[0].propertyValue = L"Latn,Grek"; p[0].localeName = NULL;
        p[1].propertyId = DWRITE_FONT_PROPERTY_ID_SUPPORTED_SCRIPT_LANGUAGE_TAG; p[1].propertyValue = L"Latn"; p[1].localeName = NULL;
        p[2].propertyId = DWRITE_FONT_PROPERTY_ID_SEMANTIC_TAG; p[2].propertyValue = L"sans-serif"; p[2].localeName = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, p, 3));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            int id;
            for (id = 7; id <= 9; id++) {
                IDWriteStringList *l = fs_props(set, (DWRITE_FONT_PROPERTY_ID)id);
                if (l) {
                    UINT32 i, n = IDWriteStringList_GetCount(l); char s[128];
                    t_note("id %d: %u values", id, n);
                    for (i = 0; i < n; i++) { fs_sl_get(l, i, s, sizeof s); t_note("  \"%s\"", s); }
                    DW_RELEASE(l);
                }
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_str_prop_many_properties)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "ManyProps", 400, 5, 0);
    IDWriteFontFaceReference *r = fs_ref(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x);
    if (r && b) {
        enum { N = 500 };
        fs_prop *p = (fs_prop *)calloc(N, sizeof *p); DWRITE_FONT_PROPERTY *arr = (DWRITE_FONT_PROPERTY *)calloc(N, sizeof *arr); int i;
        if (p && arr) {
            IDWriteFontSet *set = NULL;
            for (i = 0; i < N; i++) {
                char v[32], l[16];
                snprintf(v, sizeof v, "Name%03d", i); snprintf(l, sizeof l, "x-t%03d", i);
                fs_prop_set(&p[i], DWRITE_FONT_PROPERTY_ID_FULL_NAME, v, l);
                arr[i] = p[i].p;
            }
            CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference_(b, r, arr, N));
            CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
            if (set) {
                IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
                if (l) { CHECKF(IDWriteStringList_GetCount(l) == N, "count %u", IDWriteStringList_GetCount(l)); CHECK(fs_sl_has(l, "Name000") && fs_sl_has(l, "Name499")); DW_RELEASE(l); }
                CHECK(IDWriteFontSet_GetFontCount(set) == 1);
                DW_RELEASE(set);
            }
        }
        free(p); free(arr);
    } else t_skip("setup");
    DW_RELEASE(r); DW_RELEASE(b);
    fs_close(&x);
}

/* ===== composition ========================================================================================================= */
TEST(fset_comp_overlapping_sets)
{
    FS_OPEN(x);
    UINT32 k[3]; IDWriteFontSet *a = NULL, *b = NULL, *u = NULL;
    k[0] = fs_add(&x, "OvA", 400, 5, 0); k[1] = fs_add(&x, "OvB", 400, 5, 0); k[2] = fs_add(&x, "OvC", 400, 5, 0);
    UINT32 ka[2] = { k[0], k[1] }, kb[2] = { k[1], k[2] };
    CHECK_HR_OK(fs_build_keys(&x, ka, 2, &a));
    CHECK_HR_OK(fs_build_keys(&x, kb, 2, &b));
    if (a && b) {
        IDWriteFontSetBuilder *bd = fs_builder(&x);
        if (bd) {
            CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(bd, a));
            CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(bd, b));
            CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(bd, &u));
            if (u) {
                UINT32 n = IDWriteFontSet_GetFontCount(u);
                CHECKF(n == 3 || n == 4, "union count %u", n);
                t_note("{A,B} + {B,C} -> %u items", n);
                CHECK(fs_find_by(u, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "OvA") != FS_NOT_FOUND);
                CHECK(fs_find_by(u, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "OvC") != FS_NOT_FOUND);
                DW_RELEASE(u);
            }
            DW_RELEASE(bd);
        }
    }
    DW_RELEASE(a); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_comp_filter_then_rebuild)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *italics = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, "Italic", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &italics));
    if (italics) {
        IDWriteFontSetBuilder *b = fs_builder(&s.x); IDWriteFontSet *out = NULL;
        if (b) {
            CHECK_HR_OK(IDWriteFontSetBuilder_AddFontSet(b, italics));
            CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &out));
            if (out) {
                CHECK(IDWriteFontSet_GetFontCount(out) == 1);
                CHECK(fs_find_by(out, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "SynthA Italic") != FS_NOT_FOUND);
                CHECK(fs_find_by(out, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "SynthA") == FS_NOT_FOUND);
                DW_RELEASE(out);
            }
            DW_RELEASE(b);
        }
        DW_RELEASE(italics);
    }
    fs_std_close(&s);
}

TEST(fset_comp_builder_reuse_after_set)
{
    FS_OPEN(x);
    UINT32 k1 = fs_add(&x, "ReuseA", 400, 5, 0), k2 = fs_add(&x, "ReuseB", 400, 5, 0);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    IDWriteFontFaceReference *r1 = fs_ref(&x, k1), *r2 = fs_ref(&x, k2);
    if (b && r1 && r2) {
        IDWriteFontSet *s1 = NULL, *s2 = NULL, *s3 = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, r1));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s1));
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, r2));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s2));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s3));
        if (s1 && s2 && s3) {
            CHECK(IDWriteFontSet_GetFontCount(s1) == 1);
            CHECK(IDWriteFontSet_GetFontCount(s2) == 2);
            CHECK(IDWriteFontSet_GetFontCount(s3) == 2);
            CHECK(s2 != s3);
        }
        DW_RELEASE(s1); DW_RELEASE(s2); DW_RELEASE(s3);
    } else t_skip("setup");
    DW_RELEASE(r1); DW_RELEASE(r2); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_comp_first_resources_collapses_simulations)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "SimRes", 400, 5, 0);
    IDWriteFontFaceReference *plain = fs_ref(&x, k), *bold = fs_ref_ex(&x, k, 0, DWRITE_FONT_SIMULATIONS_BOLD), *obl = fs_ref_ex(&x, k, 0, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (plain && bold && obl && b) {
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, plain));
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, bold));
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, obl));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            UINT32 n = IDWriteFontSet_GetFontCount(set);
            t_note("plain + bold-simulated + oblique-simulated references of one face -> %u items", n);
            CHECK(n >= 1 && n <= 3);
            IDWriteFontSet1 *s1 = fs_set1(set), *res = NULL;
            if (s1) {
                CHECK_HR_OK(IDWriteFontSet1_GetFirstFontResources(s1, &res));
                if (res) { { UINT32 m = IDWriteFontSet1_GetFontCount(res); CHECKF(m >= 1 && m <= 3, "resources %u", m); t_note("first font resources of %u items: %u", n, m); } DW_RELEASE(res); }
                DW_RELEASE(s1);
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(plain); DW_RELEASE(bold); DW_RELEASE(obl); DW_RELEASE(b);
    fs_close(&x);
}

/* ===== lifetime across loader unregistration ========================================================================== */
TEST(fset_life_set_readable_after_unregister)
{
    FS_STD(s);
    dwx_loader_unregister(s.x.fl);
    /* names and properties are stored in the set itself */
    CHECK(IDWriteFontSet_GetFontCount(s.set) == 5);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
    CHECK(l != NULL);
    if (l) { CHECK(IDWriteStringList_GetCount(l) == 5); DW_RELEASE(l); }
    IDWriteFontFaceReference *r = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(s.set, 0, &r));
    if (r) {
        IDWriteFontFace3 *face = (IDWriteFontFace3 *)FS_SENT;
        HRESULT hr = IDWriteFontFaceReference_CreateFontFace(r, &face);
        /* the file loader is gone: opening the font fails and the out parameter is cleared */
        CHECKF(FAILED(hr), "CreateFontFace without a registered loader -> %s", dw_hr_name(hr));
        t_note("CreateFontFace after unregistering the file loader -> %s", dw_hr_name(hr));
        if (FAILED(hr)) CHECK_OUT_NULL(face);
        FS_DROP(face, hr);
        DW_RELEASE(r);
    }
    fs_std_close(&s);
}

TEST(fset_life_builder_after_unregister)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "LateAdd", 400, 5, 0);
    IDWriteFontFile *file = fs_file(&x, k); IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSetBuilder1 *b1 = b ? fs_builder1(b) : NULL;
    if (file && b1) {
        dwx_loader_unregister(x.fl);
        HRESULT hr = IDWriteFontSetBuilder1_AddFontFile(b1, file);
        /* the file object holds its loader; the stream is opened through it even though the loader is no longer registered, or it fails */
        t_note("AddFontFile after UnregisterFontFileLoader -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr)) {
            IDWriteFontSet *set = NULL;
            CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
            if (set) { CHECK(IDWriteFontSet_GetFontCount(set) == 1); DW_RELEASE(set); }
        }
    } else t_skip("setup");
    DW_RELEASE(file); DW_RELEASE(b1); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_life_loader_released_by_dwrite)
{
    FS_OPEN(x);
    UINT32 k = fs_add(&x, "LoaderRef", 400, 5, 0);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(fs_build_keys(&x, &k, 1, &set));
    LONG during = dwx_refs(x.fl);
    DW_RELEASE(set);
    LONG after = dwx_refs(x.fl);
    t_note("loader refcount with set %ld, after releasing the set %ld", (long)during, (long)after);
    CHECK(after <= during);
    fs_close(&x);
    FS_SOFT(dwx_destroyed(x.fl));   /* dwrite drops its loader references once the factory is gone */
    t_note("loader refs after fs_close: %ld", (long)dwx_refs(x.fl));
}

/* ===== local files ======================================================================================================== */
static int write_font(const char *leaf, const char *family, char *path, size_t cap)
{
    ttf_spec s; size_t n; uint8_t *blob; FILE *fp;
    fs_style(&s, family, 400, 5, 0);
    blob = ttf_build(&s, &n);
    t_tmpfile(path, cap, leaf);
    fp = fopen(path, "wb");
    if (!fp || !blob) { free(blob); return 0; }
    fwrite(blob, 1, n, fp); fclose(fp); free(blob);
    return 1;
}

TEST(fset_file_reference_from_path)
{
    FS_OPEN(x);
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    if (!write_font("setref.ttf", "PathRef", path, sizeof path)) { t_skip("cannot write scratch font"); fs_close(&x); return; }
    dw_wide(path, wpath, MAX_PATH);
    IDWriteFontFaceReference *ref = NULL;
    HRESULT hr = IDWriteFactory3_CreateFontFaceReference(x.f3, wpath, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    CHECK_HR_OK(hr);
    IDWriteFontSetBuilder *b = fs_builder(&x);
    if (ref && b) {
        IDWriteFontSet *set = NULL;
        CHECK_HR_OK(IDWriteFontSetBuilder_AddFontFaceReference(b, ref));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &set));
        if (set) {
            char buf[64];
            CHECK(IDWriteFontSet_GetFontCount(set) == 1);
            CHECK(fs_font_prop(set, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf) && !strcmp(buf, "PathRef"));
            UINT32 idx = 9; BOOL ex = TRUE;
            /* Win10: the reference that was added is not an item of the set (items carry more state than path and face index) */
            CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, ref, &idx, &ex));
            CHECK(!ex && idx == 0xFFFFFFFFu);
            IDWriteFontFaceReference *r2 = NULL;
            if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(set, 0, &r2)) && r2) {
                CHECK(!IDWriteFontFaceReference_Equals(ref, r2));
                ex = FALSE; CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, r2, &idx, &ex)); CHECK(ex && idx == 0);
                CHECKF(IDWriteFontFaceReference_GetLocality(r2) == DWRITE_LOCALITY_LOCAL, "locality");
                CHECK(IDWriteFontFaceReference_GetFileSize(r2) > 0);
                DW_RELEASE(r2);
            }
            DW_RELEASE(set);
        }
    } else t_skip("setup");
    DW_RELEASE(ref); DW_RELEASE(b);
    DeleteFileA(path);
    fs_close(&x);
}

TEST(fset_file_path_equals_file_object_reference)
{
    FS_OPEN(x);
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    if (!write_font("eqref.ttf", "EqRef", path, sizeof path)) { t_skip("cannot write scratch font"); fs_close(&x); return; }
    dw_wide(path, wpath, MAX_PATH);
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *a = NULL, *b2 = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(x.f, wpath, NULL, &file));
    CHECK_HR_OK(IDWriteFactory3_CreateFontFaceReference(x.f3, wpath, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &a));
    if (file) CHECK_HR_OK(IDWriteFactory3_CreateFontFaceReference_(x.f3, file, 0, DWRITE_FONT_SIMULATIONS_NONE, &b2));
    if (a && b2) CHECK(IDWriteFontFaceReference_Equals(a, b2));
    DW_RELEASE(a); DW_RELEASE(b2); DW_RELEASE(file);
    DeleteFileA(path);
    fs_close(&x);
}

TEST(fset_file_missing_path_reference)
{
    FS_OPEN(x);
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    t_tmpfile(path, sizeof path, "never_created.ttf");
    dw_wide(path, wpath, MAX_PATH);
    IDWriteFontFaceReference *ref = (IDWriteFontFaceReference *)FS_SENT;
    HRESULT hr = IDWriteFactory3_CreateFontFaceReference(x.f3, wpath, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &ref);
    t_note("CreateFontFaceReference(missing path) -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && ref) {
        IDWriteFontSetBuilder *b = fs_builder(&x);
        if (b) {
            IDWriteFontSet *set = NULL;
            HRESULT h2 = IDWriteFontSetBuilder_AddFontFaceReference(b, ref);
            HRESULT h3 = IDWriteFontSetBuilder_CreateFontSet(b, &set);
            t_note("AddFontFaceReference -> %s, CreateFontSet -> %s, count %u", dw_hr_name(h2), dw_hr_name(h3), set ? IDWriteFontSet_GetFontCount(set) : 0);
            DW_RELEASE(set); DW_RELEASE(b);
        }
    }
    FS_DROP(ref, hr);
    fs_close(&x);
}

/* ===== threads =============================================================================================================== */
typedef struct { IDWriteFontSet *set; volatile LONG bad; int iters; } thr_ctx;
static DWORD WINAPI thr_proc(LPVOID p)
{
    thr_ctx *c = (thr_ctx *)p; int i;
    for (i = 0; i < c->iters; i++) {
        IDWriteStringList *l = NULL; IDWriteFontSet *sub = NULL; fs_prop q;
        if (IDWriteFontSet_GetFontCount(c->set) != 5) InterlockedIncrement(&c->bad);
        if (FAILED(IDWriteFontSet_GetPropertyValues__(c->set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &l)) || !l || IDWriteStringList_GetCount(l) != 5) InterlockedIncrement(&c->bad);
        if (l) IDWriteStringList_Release(l);
        fs_prop_set(&q, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", "en-us");
        if (FAILED(IDWriteFontSet_GetMatchingFonts(c->set, &q.p, 1, &sub)) || !sub || IDWriteFontSet_GetFontCount(sub) != 4) InterlockedIncrement(&c->bad);
        if (sub) IDWriteFontSet_Release(sub);
    }
    return 0;
}

TEST(fset_thread_concurrent_queries)
{
    FS_STD(s);
    enum { NT = 6 };
    HANDLE h[NT]; thr_ctx ctx; int i;
    ctx.set = s.set; ctx.bad = 0; ctx.iters = 200;
    for (i = 0; i < NT; i++) h[i] = CreateThread(NULL, 0, thr_proc, &ctx, 0, NULL);
    for (i = 0; i < NT; i++) if (h[i]) { DWORD w = WaitForSingleObject(h[i], 20000); CHECKF(w == WAIT_OBJECT_0, "thread %d finished", i); CloseHandle(h[i]); }
    CHECKF(ctx.bad == 0, "%ld inconsistent results", (long)ctx.bad);
    EXPECT_REFS(s.set, 1);
    fs_std_close(&s);
}

typedef struct { IDWriteFontSet *set; volatile LONG bad; } sys_ctx;
static DWORD WINAPI sys_proc(LPVOID p)
{
    sys_ctx *c = (sys_ctx *)p; int i; UINT32 n = IDWriteFontSet_GetFontCount(c->set);
    for (i = 0; i < 100; i++) {
        IDWriteFontFaceReference *r = NULL; UINT32 idx = 0; BOOL ex = FALSE; UINT32 at = (UINT32)(i * 7) % n;
        if (FAILED(IDWriteFontSet_GetFontFaceReference(c->set, at, &r)) || !r) { InterlockedIncrement(&c->bad); continue; }
        if (FAILED(IDWriteFontSet_FindFontFaceReference(c->set, r, &idx, &ex)) || !ex || idx >= n) InterlockedIncrement(&c->bad);
        IDWriteFontFaceReference_Release(r);
    }
    return 0;
}

TEST(fset_thread_system_set_lookups)
{
    NEED_SYSSET(set);
    enum { NT = 4 };
    HANDLE h[NT]; sys_ctx ctx; int i;
    ctx.set = set; ctx.bad = 0;
    for (i = 0; i < NT; i++) h[i] = CreateThread(NULL, 0, sys_proc, &ctx, 0, NULL);
    for (i = 0; i < NT; i++) if (h[i]) { DWORD w = WaitForSingleObject(h[i], 20000); CHECKF(w == WAIT_OBJECT_0, "thread %d finished", i); CloseHandle(h[i]); }
    CHECKF(ctx.bad == 0, "%ld failures", (long)ctx.bad);
    DW_RELEASE(set);
}

/* ===== system set sweeps ============================================================================================================= */
TEST(fset_sweep_every_font_has_numeric_properties)
{
    NEED_SYSSET(set);
    UINT32 i, n = IDWriteFontSet_GetFontCount(set), bad = 0, nofam = 0;
    if (n > 2000) n = 2000;
    for (i = 0; i < n; i++) {
        char w[16], st[16], sy[16], fam[128];
        BOOL ew = fs_font_prop(set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT, w, sizeof w);
        BOOL es = fs_font_prop(set, i, DWRITE_FONT_PROPERTY_ID_STRETCH, st, sizeof st);
        BOOL ey = fs_font_prop(set, i, DWRITE_FONT_PROPERTY_ID_STYLE, sy, sizeof sy);
        if (!ew || !es || !ey) bad++;
        else if (fs_atoi(w) < 1 || fs_atoi(w) > 999 || fs_atoi(st) < 1 || fs_atoi(st) > 9 || fs_atoi(sy) < 0 || fs_atoi(sy) > 2) bad++;
        if (!fs_font_prop(set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, fam, sizeof fam) || !fam[0]) nofam++;
    }
    CHECKF(bad == 0, "%u of %u fonts lack a valid weight/stretch/style", bad, n);
    FS_SOFT(nofam == 0);
    DW_RELEASE(set);
}

TEST(fset_sweep_ids_0_to_14_sampled_fonts)
{
    NEED_SYSSET(set);
    UINT32 n = IDWriteFontSet_GetFontCount(set), i, step = n / 25 + 1; int id;
    for (i = 0; i < n; i += step)
        for (id = 0; id <= 14; id++) {
            BOOL ex = 99; IDWriteLocalizedStrings *ls = NULL;
            HRESULT hr = IDWriteFontSet_GetPropertyValues(set, i, (DWRITE_FONT_PROPERTY_ID)id, &ex, &ls);
            CHECKF(hr == S_OK, "font %u id %d -> %s", i, id, dw_hr_name(hr));
            CHECK((ex == TRUE) == (ls != NULL));
            if (ls) CHECK(IDWriteLocalizedStrings_GetCount(ls) > 0);
            DW_RELEASE(ls);
        }
    DW_RELEASE(set);
}

TEST(fset_sweep_set_wide_values_cover_per_font_values)
{
    NEED_SYSSET(set);
    IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME);
    CHECK(l != NULL);
    if (l) {
        UINT32 n = IDWriteFontSet_GetFontCount(set), i, step = n / 40 + 1;
        for (i = 0; i < n; i += step) {
            BOOL ex = FALSE; IDWriteLocalizedStrings *ls = NULL;
            if (SUCCEEDED(IDWriteFontSet_GetPropertyValues(set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, &ex, &ls)) && ex && ls) {
                char v[128]; dw_ls_en(ls, v, sizeof v);
                CHECKF(fs_sl_has(l, v), "font %u face name \"%s\" appears in the set-wide list", i, v);
            }
            DW_RELEASE(ls);
        }
        DW_RELEASE(l);
    }
    DW_RELEASE(set);
}

TEST(fset_sweep_known_system_families)
{
    NEED_SYSSET(set);
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Consolas", L"Verdana", L"Calibri" };
    static const char *u8[] = { "Arial", "Times New Roman", "Courier New", "Segoe UI", "Tahoma", "Consolas", "Verdana", "Calibri" };
    unsigned i;
    for (i = 0; i < sizeof fams / sizeof fams[0]; i++) {
        if (!dw_have_font(dw_shared(), fams[i])) { t_note("%s not installed", u8[i]); continue; }
        fs_prop p; UINT32 c = 0;
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, u8[i], "en-us");
        CHECK_HR_OK(IDWriteFontSet_GetPropertyOccurrenceCount(set, &p.p, &c));
        CHECKF(c >= 1, "%s in the font set (%u)", u8[i], c);
        IDWriteFontSet *sub = NULL;
        CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(set, &p.p, 1, &sub));
        if (sub) { CHECKF(IDWriteFontSet_GetFontCount(sub) == c, "%s: %u vs %u", u8[i], IDWriteFontSet_GetFontCount(sub), c); DW_RELEASE(sub); }
    }
    DW_RELEASE(set);
}

TEST(fset_sweep_variable_font_axes_note)
{
    NEED_SYSSET(set);
    IDWriteFontSet1 *s1 = fs_set1(set);
    if (s1) {
        DWRITE_FONT_AXIS_RANGE r[16]; UINT32 actual = 0;
        HRESULT hr = IDWriteFontSet1_GetFontAxisRanges(s1, r, 16, &actual);
        CHECK(hr == S_OK || hr == FS_E_NOTSUFF);
        t_note("system set axis ranges: %s, %u", dw_hr_name(hr), actual);
        UINT32 i;
        for (i = 0; i < actual && i < 16; i++) {
            CHECKF(r[i].minValue <= r[i].maxValue, "range %u: %g..%g", i, r[i].minValue, r[i].maxValue);
            t_note("  tag 0x%08x %g..%g", (unsigned)r[i].axisTag, r[i].minValue, r[i].maxValue);
        }
        if (dw_have_font(dw_shared(), L"Bahnschrift")) {
            DWRITE_FONT_AXIS_RANGE wr[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f, 700.0f } };
            IDWriteFontSet1 *out = NULL;
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts_(s1, wr, 1, FALSE, &out));
            if (out) { t_note("fonts with wght range containing 700: %u", IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set);
}
