/* IDWriteFontFace3: GetFontFaceReference, GetPanose, GetWeight/GetStretch/GetStyle, GetFamilyNames, GetFaceNames, GetInformationalStrings,
 * HasCharacter, IsCharacterLocal, IsGlyphLocal, AreCharactersLocal, AreGlyphsLocal.
 *   GetFontFaceReference: *ref = the face's own IDWriteFontFaceReference; AddRef(this); S_OK.
 *   GetFamilyNames / GetFaceNames: *names zeroed, always S_OK.
 *   GetInformationalStrings: *strings = NULL, *exists = FALSE first; exists TRUE only when the name table has the id.
 *   HasCharacter: cmap lookup, non-zero glyph.
 *   AreCharactersLocal / AreGlyphsLocal: *are_local = 0; NULL array with count != 0 -> E_INVALIDARG. */
#include "ffx.h"

#define EINVAL ((HRESULT)0x80070057)

static int ls_en(IDWriteLocalizedStrings *s, char *buf, int cap) { buf[0] = 0; return s ? SUCCEEDED(dw_ls_en(s, buf, cap)) : 0; }
static void upper(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }
static int contains_ci(const char *h, const char *n) { char a[256], b[256]; snprintf(a, sizeof a, "%s", h); snprintf(b, sizeof b, "%s", n); upper(a); upper(b); return strstr(a, b) != NULL; }

/* ---- GetFontFaceReference ---------------------------------------------------------------------------------------------- */
TEST(ref_returns_subobject_of_the_face) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontFaceReference *r = (IDWriteFontFaceReference *)(ULONG_PTR)1; ULONG before = dw_refcount((IUnknown *)x.f0);
    CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(x.f3, &r));
    CHECK(r != NULL && r != (IDWriteFontFaceReference *)1);
    if (r && r != (IDWriteFontFaceReference *)1) {
        EXPECT_REFS(x.f0, before + 1);
        CHECK(dw_same_identity((IUnknown *)r, (IUnknown *)x.f0));
        CHECK(!FFX_SAME(r, x.f0));
        IDWriteFontFaceReference_Release(r);
        EXPECT_REFS(x.f0, before);
    }
    ffx_close(&x);
}
TEST(ref_same_object_returned_each_time) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontFaceReference *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(x.f3, &a)); CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(x.f3, &b));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
    ffx_close(&x);
}
TEST(ref_via_qi_matches_get_font_face_reference) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontFaceReference *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(x.f3, &a));
    CHECK_HR_OK(IDWriteFontFace_QueryInterface(x.f0, &IID_IDWriteFontFaceReference, (void **)&b));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
    ffx_close(&x);
}
TEST(ref_simple_properties) {
    ttf_spec s; ttf_default_spec(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_BOLD)) { ffx_close(&x); return; }
    IDWriteFontFaceReference *r = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(x.f3, &r));
    if (r) {
        EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 0);
        EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), DWRITE_FONT_SIMULATIONS_BOLD);
        IDWriteFontFile *ff = NULL; CHECK_HR_OK(IDWriteFontFaceReference_GetFontFile(r, &ff)); CHECK(ff != NULL); DW_RELEASE(ff);
        IDWriteFontFaceReference_Release(r);
    }
    ffx_close(&x);
}
TEST(ref_equals_itself_and_distinguishes_faces) {
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_BOLD) : 0;
    if (oa && ob) {
        IDWriteFontFaceReference *ra = NULL, *rb = NULL;
        CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(a.f3, &ra)); CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(b.f3, &rb));
        if (ra && rb) {
            EXPECT_EQ(IDWriteFontFaceReference_Equals(ra, ra), TRUE);
            EXPECT_EQ(IDWriteFontFaceReference_Equals(rb, rb), TRUE);
            EXPECT_EQ(IDWriteFontFaceReference_Equals(ra, rb), FALSE);      /* different simulations (and different files) */
            EXPECT_EQ(IDWriteFontFaceReference_Equals(rb, ra), FALSE);
        }
        DW_RELEASE(ra); DW_RELEASE(rb);
    }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(ref_create_font_face_round_trip) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontFaceReference *r = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(x.f3, &r));
    if (r) {
        IDWriteFontFace3 *nf = NULL; CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFace(r, &nf));
        if (nf) {
            IDWriteFontFace5 *n5 = NULL;
            CHECK_HR_OK(IDWriteFontFace3_QueryInterface(nf, &IID_IDWriteFontFace5, (void **)&n5));
            if (n5) { EXPECT_EQ(IDWriteFontFace5_Equals(x.f5, (IDWriteFontFace *)n5), TRUE); IDWriteFontFace5_Release(n5); }
            t_note("CreateFontFace from the reference returned %s face object", FFX_SAME(nf, x.f0) ? "the same" : "a different");
            DWRITE_FONT_METRICS1 m1, m2; ffx_c_metrics1(x.f1, &m1); ffx_c_metrics1((IDWriteFontFace1 *)nf, &m2);
            CHECK(!memcmp(&m1, &m2, sizeof m1));
            IDWriteFontFace3_Release(nf);
        }
        IDWriteFontFaceReference_Release(r);
    }
    ffx_close(&x);
}
TEST(ref_locality_local_for_system_font) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    IDWriteFontFaceReference *r = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(x.f3, &r));
    if (r) {
        EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_LOCAL);
        UINT64 ls = IDWriteFontFaceReference_GetLocalFileSize(r), fs = IDWriteFontFaceReference_GetFileSize(r);
        CHECK(ls > 0); EXPECT_EQ(ls, fs);
        FILETIME ft = { 0, 0 }; CHECK_HR_OK(IDWriteFontFaceReference_GetFileTime(r, &ft)); CHECK(ft.dwHighDateTime || ft.dwLowDateTime);
        IDWriteFontFaceReference_Release(r);
    }
    ffx_close(&x);
}
TEST(ref_locality_memory_font_note) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontFaceReference *r = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFontFaceReference(x.f3, &r));
    if (r) {
        t_note("custom-loader font: locality %d, file size %llu, local size %llu", (int)IDWriteFontFaceReference_GetLocality(r), (unsigned long long)IDWriteFontFaceReference_GetFileSize(r), (unsigned long long)IDWriteFontFaceReference_GetLocalFileSize(r));
        IDWriteFontFaceReference_Release(r);
    }
    ffx_close(&x);
}

/* ---- weight / stretch / style ------------------------------------------------------------------------------------------- */
TEST(wss_weight_ladder) {
    static const int w[] = { 100, 200, 300, 350, 400, 500, 600, 700, 800, 900, 950 };
    int i;
    for (i = 0; i < 11; i++) {
        ttf_spec s; ttf_style_spec(&s, "LadFam", w[i], 5, 0);
        ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
        DWRITE_FONT_WEIGHT got = IDWriteFontFace3_GetWeight(x.f3);
        t_check((int)got == w[i], __FILE__, __LINE__, "weight", "usWeightClass %d -> %d", w[i], (int)got);
        ffx_close(&x);
    }
}
TEST(wss_stretch_ladder) {
    int wd;
    for (wd = 1; wd <= 9; wd++) {
        ttf_spec s; ttf_style_spec(&s, "WidFam", 400, wd, 0);
        ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
        DWRITE_FONT_STRETCH got = IDWriteFontFace3_GetStretch(x.f3);
        t_check((int)got == wd, __FILE__, __LINE__, "stretch", "usWidthClass %d -> %d", wd, (int)got);
        ffx_close(&x);
    }
}
TEST(wss_style_normal_and_italic) {
    ttf_spec s; ttf_style_spec(&s, "StyFam", 400, 5, 0);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace3_GetStyle(x.f3), DWRITE_FONT_STYLE_NORMAL);
    ffx_close(&x);
    ttf_style_spec(&s, "StyFam", 400, 5, 1);
    if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace3_GetStyle(x.f3), DWRITE_FONT_STYLE_ITALIC);
    ffx_close(&x);
}
TEST(wss_default_font_is_regular) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace3_GetWeight(x.f3), DWRITE_FONT_WEIGHT_REGULAR);
    EXPECT_EQ(IDWriteFontFace3_GetStretch(x.f3), DWRITE_FONT_STRETCH_NORMAL);
    EXPECT_EQ(IDWriteFontFace3_GetStyle(x.f3), DWRITE_FONT_STYLE_NORMAL);
    ffx_close(&x);
}
TEST(wss_simulations_note) {
    ttf_spec s; ttf_default_spec(&s);
    DWRITE_FONT_SIMULATIONS sims[] = { DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE };
    int i;
    for (i = 0; i < 2; i++) {
        ffx x; if (!ffx_open_spec(&x, &s, sims[i])) { ffx_close(&x); return; }
        t_note("simulations %d: weight %d stretch %d style %d", (int)sims[i], (int)IDWriteFontFace3_GetWeight(x.f3), (int)IDWriteFontFace3_GetStretch(x.f3), (int)IDWriteFontFace3_GetStyle(x.f3));
        ffx_close(&x);
    }
}
TEST(wss_arial_family_members) {
    static const struct { DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; } c[] = { { DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL }, { DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL }, { DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_ITALIC }, { DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC } };
    int i;
    for (i = 0; i < 4; i++) {
        ffx x; if (!ffx_open_sys(&x, L"Arial", c[i].w, DWRITE_FONT_STRETCH_NORMAL, c[i].st)) { ffx_close(&x); return; }
        EXPECT_EQ(IDWriteFontFace3_GetWeight(x.f3), c[i].w); EXPECT_EQ(IDWriteFontFace3_GetStyle(x.f3), c[i].st); EXPECT_EQ(IDWriteFontFace3_GetStretch(x.f3), DWRITE_FONT_STRETCH_NORMAL);
        ffx_close(&x);
    }
}
TEST(wss_system_weights_match_os2) {
    static const WCHAR *fam[] = { L"Arial", L"Segoe UI", L"Times New Roman", L"Consolas", L"Tahoma", L"Courier New" };
    int i;
    for (i = 0; i < 6; i++) {
        ffx x; if (!ffx_open_sys(&x, fam[i], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); continue; }
        UINT32 ol = 0; uint8_t *os2 = dw_face_table(x.f0, DW_TAG('O','S','/','2'), &ol);
        if (os2 && ol >= 8) {
            EXPECT_EQ((int)IDWriteFontFace3_GetWeight(x.f3), (int)ffx_getu16(os2, 4));
            EXPECT_EQ((int)IDWriteFontFace3_GetStretch(x.f3), (int)ffx_getu16(os2, 6));
        }
        free(os2); ffx_close(&x);
    }
}
TEST(wss_same_through_interfaces) {
    ttf_spec s; ttf_style_spec(&s, "SameFam", 300, 3, 1);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace3_GetWeight(x.f3), IDWriteFontFace5_GetWeight(x.f5));
    EXPECT_EQ(IDWriteFontFace3_GetStretch(x.f3), IDWriteFontFace5_GetStretch(x.f5));
    EXPECT_EQ(IDWriteFontFace3_GetStyle(x.f3), IDWriteFontFace4_GetStyle(x.f4));
    ffx_close(&x);
}

/* ---- panose ------------------------------------------------------------------------------------------------------------- */
typedef struct { uint8_t p[10]; } pan;
static void patch_panose(uint8_t *b, size_t n, void *ctx) { uint8_t *t = ffx_tbl(b, n, "OS/2", NULL); if (t) memcpy(t + 32, ((pan *)ctx)->p, 10); }
TEST(panose_default_is_all_zero) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_PANOSE p; memset(&p, 0xCC, sizeof p);
    IDWriteFontFace3_GetPanose(x.f3, &p);
    int i; for (i = 0; i < 10; i++) EXPECT_EQ(p.values[i], 0);
    ffx_close(&x);
}
TEST(panose_follows_os2_table) {
    static const pan c[] = { { { 2, 11, 6, 4, 2, 2, 2, 2, 2, 4 } }, { { 2, 2, 6, 3, 5, 4, 5, 2, 3, 4 } }, { { 5, 1, 2, 3, 4, 5, 6, 7, 8, 9 } }, { { 255, 254, 253, 252, 251, 250, 249, 248, 247, 246 } }, { { 0 } } };
    int i;
    for (i = 0; i < 5; i++) {
        ttf_spec s; ttf_default_spec(&s); pan p = c[i];
        ffx x; if (!ffx_open_patched(&x, &s, DWRITE_FONT_SIMULATIONS_NONE, patch_panose, &p)) { ffx_close(&x); return; }
        DWRITE_PANOSE got; memset(&got, 0xCC, sizeof got); IDWriteFontFace3_GetPanose(x.f3, &got);
        t_check(!memcmp(got.values, p.p, 10), __FILE__, __LINE__, "panose", "case %d: %d %d %d %d %d %d %d %d %d %d", i, got.values[0], got.values[1], got.values[2], got.values[3], got.values[4], got.values[5], got.values[6], got.values[7], got.values[8], got.values[9]);
        ffx_close(&x);
    }
}
TEST(panose_without_os2_is_zero) {
    ttf_spec s; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_PANOSE p; memset(&p, 0xCC, sizeof p); IDWriteFontFace3_GetPanose(x.f3, &p);
    int i; for (i = 0; i < 10; i++) EXPECT_EQ(p.values[i], 0);
    ffx_close(&x);
}
TEST(panose_system_fonts_match_os2) {
    static const WCHAR *fam[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Consolas" };
    int i;
    for (i = 0; i < 5; i++) {
        ffx x; if (!ffx_open_sys(&x, fam[i], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); continue; }
        UINT32 ol = 0; uint8_t *os2 = dw_face_table(x.f0, DW_TAG('O','S','/','2'), &ol);
        DWRITE_PANOSE p; memset(&p, 0xCC, sizeof p); IDWriteFontFace3_GetPanose(x.f3, &p);
        if (os2 && ol >= 42) t_check(!memcmp(p.values, os2 + 32, 10), __FILE__, __LINE__, "panose", "%ls differs from the OS/2 panose", fam[i]);
        t_note("%ls panose %d %d %d %d %d %d %d %d %d %d", fam[i], p.values[0], p.values[1], p.values[2], p.values[3], p.values[4], p.values[5], p.values[6], p.values[7], p.values[8], p.values[9]);
        free(os2); ffx_close(&x);
    }
}

/* ---- names -------------------------------------------------------------------------------------------------------------- */
static void expect_names(IDWriteLocalizedStrings *s, const char *want, const char *what) {
    char b[256]; t_check(s != NULL, __FILE__, __LINE__, what, "NULL strings"); if (!s) return;
    t_check(ls_en(s, b, sizeof b) && !strcmp(b, want), __FILE__, __LINE__, what, "got \"%s\" want \"%s\"", b, want);
}
static void names_case(const char *family, int w, int wd, int it, const char *face) {
    ttf_spec s; ttf_style_spec(&s, family, w, wd, it);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *fam = (IDWriteLocalizedStrings *)(ULONG_PTR)1, *fc = (IDWriteLocalizedStrings *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.f3, &fam)); CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(x.f3, &fc));
    expect_names(fam, family, "family name"); expect_names(fc, face, "face name");
    CHECK(fam != fc);
    DW_RELEASE(fam); DW_RELEASE(fc);
    ffx_close(&x);
}
TEST(names_ribbi_regular) { names_case("NamFam", 400, 5, 0, "Regular"); }
TEST(names_ribbi_bold) { names_case("NamFam", 700, 5, 0, "Bold"); }
TEST(names_ribbi_italic) { names_case("NamFam", 400, 5, 1, "Italic"); }
TEST(names_ribbi_bold_italic) { names_case("NamFam", 700, 5, 1, "Bold Italic"); }
TEST(names_family_with_spaces) { names_case("My Test Family", 400, 5, 0, "Regular"); }
TEST(names_family_unicode) {
    ttf_spec s; ttf_style_spec(&s, "Fam\xC3\xA9\xE6\x97\xA5", 400, 5, 0);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *fam = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.f3, &fam));
    expect_names(fam, "Fam\xC3\xA9\xE6\x97\xA5", "unicode family");
    DW_RELEASE(fam);
    ffx_close(&x);
}
static void ladder_names(int w, int wd) {
    ttf_spec s; ttf_style_spec(&s, "LadNam", w, wd, 0);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *fam = NULL, *fc = NULL;
    CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.f3, &fam)); CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(x.f3, &fc));
    char fb[256], cb[256]; ls_en(fam, fb, sizeof fb); ls_en(fc, cb, sizeof cb);
    t_note("weight %d width %d: family \"%s\" face \"%s\"", w, wd, fb, cb);
    /* the family name starts with the typographic family; the face name carries the style word */
    CHECK(!strncmp(fb, "LadNam", 6));
    if (w != 400) CHECK(contains_ci(cb, ttf_weight_word(w)) || contains_ci(cb, "Semi") || contains_ci(cb, "Extra"));
    DW_RELEASE(fam); DW_RELEASE(fc);
    ffx_close(&x);
}
TEST(names_weight_thin) { ladder_names(100, 5); }
TEST(names_weight_light) { ladder_names(300, 5); }
TEST(names_weight_medium) { ladder_names(500, 5); }
TEST(names_weight_semibold) { ladder_names(600, 5); }
TEST(names_weight_black) { ladder_names(900, 5); }
TEST(names_width_condensed) { ladder_names(400, 3); }
TEST(names_width_expanded) { ladder_names(400, 7); }
TEST(names_condensed_light) { ladder_names(300, 3); }
TEST(names_released_strings_independent_of_face) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *fam = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.f3, &fam));
    ffx_close(&x);                          /* the face and its file loader are gone, the string collection stays valid */
    if (fam) { UINT32 n = IDWriteLocalizedStrings_GetCount(fam); CHECK(n >= 1); char b[128]; CHECK(SUCCEEDED(dw_ls_get(fam, 0, b, sizeof b))); EXPECT_STR(b, "DwTest"); IDWriteLocalizedStrings_Release(fam); }
}
TEST(names_outputs_zeroed_then_set) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *a = (IDWriteLocalizedStrings *)(ULONG_PTR)0x1234, *b = (IDWriteLocalizedStrings *)(ULONG_PTR)0x1234;
    CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.f3, &a)); CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(x.f3, &b));
    CHECK(a != (IDWriteLocalizedStrings *)0x1234 && b != (IDWriteLocalizedStrings *)0x1234);
    DW_RELEASE(a); DW_RELEASE(b);
    ffx_close(&x);
}
TEST(names_second_language_adds_entry) {
    ttf_spec s; ttf_style_spec(&s, "TwoLang", 400, 5, 0);
    ttf_add_name(&s, 3, 1, 0x407, 1, "ZweiSprachen");
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *fam = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.f3, &fam));
    if (fam) {
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(fam), 2);
        UINT32 i = 0; BOOL ex = FALSE; CHECK_HR_OK(dw_ls_find(fam, L"de-de", &i, &ex)); EXPECT_EQ(ex, TRUE);
        if (ex) { char b[128]; dw_ls_get(fam, i, b, sizeof b); EXPECT_STR(b, "ZweiSprachen"); }
        CHECK_HR_OK(dw_ls_find(fam, L"en-us", &i, &ex)); EXPECT_EQ(ex, TRUE);
        if (ex) { char b[128]; dw_ls_get(fam, i, b, sizeof b); EXPECT_STR(b, "TwoLang"); }
        IDWriteLocalizedStrings_Release(fam);
    }
    ffx_close(&x);
}
TEST(names_system_arial) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *fam = NULL, *fc = NULL; char b[256];
    CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.f3, &fam)); CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(x.f3, &fc));
    CHECK(ls_en(fam, b, sizeof b)); EXPECT_STR(b, "Arial");
    CHECK(ls_en(fc, b, sizeof b)); EXPECT_STR(b, "Bold");
    DW_RELEASE(fam); DW_RELEASE(fc);
    ffx_close(&x);
}

/* ---- GetInformationalStrings -------------------------------------------------------------------------------------------- */
static const struct { DWRITE_INFORMATIONAL_STRING_ID id; unsigned name_id; const char *what; } info_map[] = {
    { DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, 0, "copyright" }, { DWRITE_INFORMATIONAL_STRING_VERSION_STRINGS, 5, "version" },
    { DWRITE_INFORMATIONAL_STRING_TRADEMARK, 7, "trademark" }, { DWRITE_INFORMATIONAL_STRING_MANUFACTURER, 8, "manufacturer" },
    { DWRITE_INFORMATIONAL_STRING_DESIGNER, 9, "designer" }, { DWRITE_INFORMATIONAL_STRING_DESCRIPTION, 10, "description" },
    { DWRITE_INFORMATIONAL_STRING_FONT_VENDOR_URL, 11, "vendor url" }, { DWRITE_INFORMATIONAL_STRING_DESIGNER_URL, 12, "designer url" },
    { DWRITE_INFORMATIONAL_STRING_LICENSE_DESCRIPTION, 13, "license" }, { DWRITE_INFORMATIONAL_STRING_LICENSE_INFO_URL, 14, "license url" },
    { DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, 1, "win32 family" }, { DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, 2, "win32 subfamily" },
    { DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES, 16, "typographic family" }, { DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES, 17, "typographic subfamily" },
    { DWRITE_INFORMATIONAL_STRING_SAMPLE_TEXT, 19, "sample text" }, { DWRITE_INFORMATIONAL_STRING_FULL_NAME, 4, "full name" },
    { DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, 6, "postscript" }, { DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_CID_NAME, 20, "cid name" },
};
#define NINFO ((int)(sizeof info_map / sizeof info_map[0]))
static void fill_info(ttf_spec *s) {
    int i; char t[64];
    ttf_style_spec(s, "InfoFam", 400, 5, 0); ttf_clear_names(s);
    for (i = 0; i < NINFO; i++) { snprintf(t, sizeof t, "%s #%u", info_map[i].what, info_map[i].name_id); ttf_add_name(s, 3, 1, 0x409, info_map[i].name_id, t); }
}
TEST(info_each_id_maps_to_its_name_record) {
    ttf_spec s; fill_info(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    int i;
    for (i = 0; i < NINFO; i++) {
        IDWriteLocalizedStrings *str = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 0x77; char want[64], got[128];
        HRESULT hr = IDWriteFontFace3_GetInformationalStrings(x.f3, info_map[i].id, &str, &ex);
        snprintf(want, sizeof want, "%s #%u", info_map[i].what, info_map[i].name_id);
        t_check(hr == S_OK && ex == TRUE && str != NULL, __FILE__, __LINE__, info_map[i].what, "id %d: %s exists=%d", (int)info_map[i].id, dw_hr_name(hr), ex);
        if (str && str != (IDWriteLocalizedStrings *)1) { ls_en(str, got, sizeof got); t_check(!strcmp(got, want), __FILE__, __LINE__, info_map[i].what, "id %d -> \"%s\" want \"%s\"", (int)info_map[i].id, got, want); IDWriteLocalizedStrings_Release(str); }
    }
    ffx_close(&x);
}
TEST(info_missing_strings_report_not_exists) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const DWRITE_INFORMATIONAL_STRING_ID absent[] = { DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, DWRITE_INFORMATIONAL_STRING_VERSION_STRINGS, DWRITE_INFORMATIONAL_STRING_TRADEMARK,
        DWRITE_INFORMATIONAL_STRING_MANUFACTURER, DWRITE_INFORMATIONAL_STRING_DESIGNER, DWRITE_INFORMATIONAL_STRING_DESIGNER_URL, DWRITE_INFORMATIONAL_STRING_DESCRIPTION,
        DWRITE_INFORMATIONAL_STRING_FONT_VENDOR_URL, DWRITE_INFORMATIONAL_STRING_LICENSE_DESCRIPTION, DWRITE_INFORMATIONAL_STRING_LICENSE_INFO_URL,
        DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES, DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES, DWRITE_INFORMATIONAL_STRING_SAMPLE_TEXT };
    int i;
    for (i = 0; i < 13; i++) {
        IDWriteLocalizedStrings *str = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 0x77;
        HRESULT hr = IDWriteFontFace3_GetInformationalStrings(x.f3, absent[i], &str, &ex);
        t_check(hr == S_OK && ex == FALSE && str == NULL, __FILE__, __LINE__, "absent", "id %d: %s exists=%d str=%p", (int)absent[i], dw_hr_name(hr), ex, (void *)str);
    }
    ffx_close(&x);
}
TEST(info_present_in_default_font) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    struct { DWRITE_INFORMATIONAL_STRING_ID id; const char *want; } c[] = {
        { DWRITE_INFORMATIONAL_STRING_FULL_NAME, "DwTest" }, { DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, "DwTest-Regular" },
        { DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, "DwTest" }, { DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, "Regular" },
    };
    int i;
    for (i = 0; i < 4; i++) {
        IDWriteLocalizedStrings *str = NULL; BOOL ex = FALSE; char b[128];
        CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(x.f3, c[i].id, &str, &ex));
        t_check(ex == TRUE && str, __FILE__, __LINE__, "info", "id %d missing", (int)c[i].id);
        if (str) { ls_en(str, b, sizeof b); t_check(!strcmp(b, c[i].want), __FILE__, __LINE__, "info", "id %d \"%s\" want \"%s\"", (int)c[i].id, b, c[i].want); IDWriteLocalizedStrings_Release(str); }
    }
    ffx_close(&x);
}
TEST(info_none_id_note) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *str = NULL; BOOL ex = 0x77;
    HRESULT hr = IDWriteFontFace3_GetInformationalStrings(x.f3, DWRITE_INFORMATIONAL_STRING_NONE, &str, &ex);
    t_note("DWRITE_INFORMATIONAL_STRING_NONE: %s exists=%d", dw_hr_name(hr), ex);
    CHECK(ex == 0 || ex == 1);
    DW_RELEASE(str);
    ffx_close(&x);
}
TEST(info_multiple_languages) {
    ttf_spec s; fill_info(&s);
    ttf_add_name(&s, 3, 1, 0x40C, 8, "fabricant #8");
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *str = NULL; BOOL ex = FALSE;
    CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(x.f3, DWRITE_INFORMATIONAL_STRING_MANUFACTURER, &str, &ex));
    if (str) {
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(str), 2);
        UINT32 i = 0; BOOL f = FALSE; char b[64];
        CHECK_HR_OK(dw_ls_find(str, L"fr-fr", &i, &f)); EXPECT_EQ(f, TRUE); if (f) { dw_ls_get(str, i, b, sizeof b); EXPECT_STR(b, "fabricant #8"); }
        IDWriteLocalizedStrings_Release(str);
    }
    ffx_close(&x);
}
TEST(info_mac_roman_record_used) {
    ttf_spec s; ttf_style_spec(&s, "MacFam", 400, 5, 0);
    ttf_add_name(&s, 1, 0, 0, 8, "MacMaker");
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *str = NULL; BOOL ex = FALSE;
    CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(x.f3, DWRITE_INFORMATIONAL_STRING_MANUFACTURER, &str, &ex));
    t_note("Mac-only manufacturer name: exists=%d", ex);
    if (str) { char b[64]; dw_ls_en(str, b, sizeof b); EXPECT_STR(b, "MacMaker"); IDWriteLocalizedStrings_Release(str); }
    ffx_close(&x);
}
TEST(info_arial_values_note) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    int i;
    for (i = 0; i < NINFO; i++) {
        IDWriteLocalizedStrings *str = NULL; BOOL ex = FALSE; char b[200] = "";
        CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(x.f3, info_map[i].id, &str, &ex));
        if (ex && str) dw_ls_en(str, b, sizeof b);
        t_note("Arial %s: %s \"%.60s\"", info_map[i].what, ex ? "present" : "absent", b);
        CHECK((ex && str) || (!ex && !str));
        DW_RELEASE(str);
    }
    ffx_close(&x);
}
TEST(info_arial_full_and_postscript_names) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    IDWriteLocalizedStrings *s1 = NULL, *s2 = NULL; BOOL e1 = FALSE, e2 = FALSE; char b[128];
    CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(x.f3, DWRITE_INFORMATIONAL_STRING_FULL_NAME, &s1, &e1));
    CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(x.f3, DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, &s2, &e2));
    EXPECT_EQ(e1, TRUE); EXPECT_EQ(e2, TRUE);
    if (s1) { dw_ls_en(s1, b, sizeof b); EXPECT_STR(b, "Arial"); }
    if (s2) { dw_ls_en(s2, b, sizeof b); EXPECT_STR(b, "ArialMT"); }
    DW_RELEASE(s1); DW_RELEASE(s2);
    ffx_close(&x);
}

/* ---- HasCharacter ------------------------------------------------------------------------------------------------------- */
TEST(has_char_default_font) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const struct { UINT32 cp; BOOL want; } c[] = { { ' ', TRUE }, { 'A', TRUE }, { 'Z', TRUE }, { 'a', TRUE }, { 'z', TRUE }, { '0', TRUE }, { '9', TRUE },
        { '!', FALSE }, { '@', FALSE }, { '[', FALSE }, { '`', FALSE }, { '{', FALSE }, { '/', FALSE }, { ':', FALSE }, { 0, FALSE }, { 0xA0, FALSE }, { 0xFFFD, FALSE },
        { 0xFFFF, FALSE }, { 0x10000, FALSE }, { 0x1F600, FALSE }, { 0x10FFFF, FALSE }, { 0x110000, FALSE }, { 0xFFFFFFFFu, FALSE } };
    int i;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        BOOL got = IDWriteFontFace3_HasCharacter(x.f3, c[i].cp);
        t_check(got == c[i].want, __FILE__, __LINE__, "has char", "U+%X: %d want %d", c[i].cp, got, c[i].want);
    }
    ffx_close(&x);
}
TEST(has_char_agrees_with_glyph_indices) {
    ttf_spec s; ffx_spec_ranges(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    UINT32 cp, bad = 0;
    for (cp = 0; cp < 0x5000; cp++) { UINT16 g = 0; UINT32 c = cp; IDWriteFontFace_GetGlyphIndices(x.f0, &c, 1, &g); if ((g != 0) != (IDWriteFontFace3_HasCharacter(x.f3, cp) != 0)) bad++; }
    EXPECT_EQ(bad, 0);
    static const UINT32 hi[] = { 0x1F5FF, 0x1F600, 0x1F601, 0x1F602 };
    int i; for (i = 0; i < 4; i++) { UINT16 g = 0; UINT32 c = hi[i]; IDWriteFontFace_GetGlyphIndices(x.f0, &c, 1, &g); t_check((g != 0) == (IDWriteFontFace3_HasCharacter(x.f3, hi[i]) != 0), __FILE__, __LINE__, "agree", "U+%X", hi[i]); }
    EXPECT_EQ(IDWriteFontFace3_HasCharacter(x.f3, 0x1F600), TRUE);
    EXPECT_EQ(IDWriteFontFace3_HasCharacter(x.f3, 0x3042), TRUE);
    EXPECT_EQ(IDWriteFontFace3_HasCharacter(x.f3, 0x3044), FALSE);
    ffx_close(&x);
}
TEST(has_char_clean_bool) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    BOOL a = IDWriteFontFace3_HasCharacter(x.f3, 'A'), b = IDWriteFontFace3_HasCharacter(x.f3, '!');
    CHECK(a == 1); CHECK(b == 0);
    ffx_close(&x);
}
TEST(has_char_system_fonts) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace3_HasCharacter(x.f3, 'A'), TRUE); EXPECT_EQ(IDWriteFontFace3_HasCharacter(x.f3, 0x20AC), TRUE);
    EXPECT_EQ(IDWriteFontFace3_HasCharacter(x.f3, 0x3042), FALSE);      /* no hiragana in Arial */
    EXPECT_EQ(IDWriteFontFace3_HasCharacter(x.f3, 0x110000), FALSE);
    ffx_close(&x);
    if (!ffx_open_sys(&x, L"MS Gothic", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace3_HasCharacter(x.f3, 0x3042), TRUE);
    ffx_close(&x);
}
TEST(has_char_symbol_font_note) {
    ffx x; if (!ffx_open_sys(&x, L"Marlett", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    t_note("Marlett HasCharacter: U+0061 %d, U+F061 %d, U+0020 %d", IDWriteFontFace3_HasCharacter(x.f3, 0x61), IDWriteFontFace3_HasCharacter(x.f3, 0xF061), IDWriteFontFace3_HasCharacter(x.f3, 0x20));
    ffx_close(&x);
}

/* ---- locality ------------------------------------------------------------------------------------------------------------ */
TEST(local_single_character_and_glyph) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace3_IsCharacterLocal(x.f3, 'A'), TRUE);
    EXPECT_EQ(IDWriteFontFace3_IsCharacterLocal(x.f3, ' '), TRUE);
    EXPECT_EQ(IDWriteFontFace3_IsGlyphLocal(x.f3, 2), TRUE);
    EXPECT_EQ(IDWriteFontFace3_IsGlyphLocal(x.f3, 0), TRUE);
    t_note("IsCharacterLocal('!' unmapped)=%d IsCharacterLocal(0x10FFFF)=%d IsGlyphLocal(68)=%d", IDWriteFontFace3_IsCharacterLocal(x.f3, '!'), IDWriteFontFace3_IsCharacterLocal(x.f3, 0x10FFFF), IDWriteFontFace3_IsGlyphLocal(x.f3, 68));
    ffx_close(&x);
}
TEST(local_many_characters) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const WCHAR txt[] = L"Hello World 0123456789";
    int enq;
    for (enq = 0; enq < 2; enq++) {
        BOOL r = 0x77; CHECK_HR_OK(IDWriteFontFace3_AreCharactersLocal(x.f3, txt, 22, enq, &r));
        t_check(r == TRUE, __FILE__, __LINE__, "chars local", "enqueue %d -> %d", enq, r);
    }
    ffx_close(&x);
}
TEST(local_many_glyphs) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[10] = { 0, 1, 2, 3, 28, 54, 64, 65, 66, 67 };
    int enq;
    for (enq = 0; enq < 2; enq++) {
        BOOL r = 0x77; CHECK_HR_OK(IDWriteFontFace3_AreGlyphsLocal(x.f3, g, 10, enq, &r));
        t_check(r == TRUE, __FILE__, __LINE__, "glyphs local", "enqueue %d -> %d", enq, r);
    }
    ffx_close(&x);
}
TEST(local_null_arrays_invalid_and_out_zeroed) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    BOOL r = 0x77;
    CHECK_HR(IDWriteFontFace3_AreCharactersLocal(x.f3, NULL, 3, FALSE, &r), EINVAL); EXPECT_EQ(r, 0);
    r = 0x77; CHECK_HR(IDWriteFontFace3_AreCharactersLocal(x.f3, NULL, 1, TRUE, &r), EINVAL); EXPECT_EQ(r, 0);
    r = 0x77; CHECK_HR(IDWriteFontFace3_AreGlyphsLocal(x.f3, NULL, 3, FALSE, &r), EINVAL); EXPECT_EQ(r, 0);
    r = 0x77; CHECK_HR(IDWriteFontFace3_AreGlyphsLocal(x.f3, NULL, 0xFFFFFFFFu, TRUE, &r), EINVAL); EXPECT_EQ(r, 0);
    ffx_close(&x);
}
TEST(local_zero_count_succeeds) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    BOOL r = 0x77; static const WCHAR t[] = L"A"; static const UINT16 g[] = { 1 };
    CHECK_HR_OK(IDWriteFontFace3_AreCharactersLocal(x.f3, NULL, 0, FALSE, &r)); t_note("AreCharactersLocal(NULL, 0) -> %d", r); CHECK(r == 0 || r == 1);
    r = 0x77; CHECK_HR_OK(IDWriteFontFace3_AreCharactersLocal(x.f3, t, 0, TRUE, &r)); CHECK(r == 0 || r == 1);
    r = 0x77; CHECK_HR_OK(IDWriteFontFace3_AreGlyphsLocal(x.f3, NULL, 0, FALSE, &r)); t_note("AreGlyphsLocal(NULL, 0) -> %d", r); CHECK(r == 0 || r == 1);
    r = 0x77; CHECK_HR_OK(IDWriteFontFace3_AreGlyphsLocal(x.f3, g, 0, TRUE, &r)); CHECK(r == 0 || r == 1);
    ffx_close(&x);
}
TEST(local_system_font_is_local) {
    ffx x; if (!ffx_open_sys(&x, L"Segoe UI", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    static const WCHAR t[] = L"The quick brown fox"; BOOL r = 0x77;
    EXPECT_EQ(IDWriteFontFace3_IsCharacterLocal(x.f3, 'T'), TRUE);
    CHECK_HR_OK(IDWriteFontFace3_AreCharactersLocal(x.f3, t, 19, TRUE, &r)); EXPECT_EQ(r, TRUE);
    UINT16 g[3] = { 3, 4, 5 }; r = 0x77; CHECK_HR_OK(IDWriteFontFace3_AreGlyphsLocal(x.f3, g, 3, TRUE, &r)); EXPECT_EQ(r, TRUE);
    ffx_close(&x);
}
TEST(local_enqueue_on_local_font_does_not_notify_listener) {
    /* a local font needs no download: asking for the characters with enqueue = TRUE leaves the factory's download queue empty */
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    if (!f3) { t_skip("IDWriteFactory3 unavailable"); return; }
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontDownloadQueue *q = NULL; HRESULT hr = IDWriteFactory3_GetFontDownloadQueue(f3, &q);
    if (SUCCEEDED(hr) && q) {
        static const WCHAR t[] = L"ABC"; BOOL r = 0;
        CHECK_HR_OK(IDWriteFontFace3_AreCharactersLocal(x.f3, t, 3, TRUE, &r));
        EXPECT_EQ(IDWriteFontDownloadQueue_IsEmpty(q), TRUE);
        IDWriteFontDownloadQueue_Release(q);
    } else t_skip("no download queue: %s", dw_hr_name(hr));
    ffx_close(&x);
}
