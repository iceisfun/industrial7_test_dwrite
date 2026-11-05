/* Localized strings of installed fonts, font sets and font face references.
 *
 * System fonts are not under our control: hard checks are limited to relations (routes agree, protocol holds) and to
 * the English names of the long-standing inbox faces; everything else is a t_note.
 */
#include "lsfx.h"

static IDWriteFontCollection *sys_coll(IDWriteFactory *f) {
    IDWriteFontCollection *c = NULL;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE))) return NULL;
    return c;
}
/* Font for a family name (first match for the given weight/style) or NULL when not installed. */
static IDWriteFont *sys_font(IDWriteFactory *f, IDWriteFontCollection *c, const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st, IDWriteFontFamily **famout) {
    UINT32 idx = 0; BOOL ex = FALSE; IDWriteFontFamily *ff = NULL; IDWriteFont *font = NULL;
    if (FAILED(IDWriteFontCollection_FindFamilyName(c, fam, &idx, &ex)) || !ex) return NULL;
    if (FAILED(IDWriteFontCollection_GetFontFamily(c, idx, &ff))) return NULL;
    if (FAILED(IDWriteFontFamily_GetFirstMatchingFont(ff, w, DWRITE_FONT_STRETCH_NORMAL, st, &font))) { IDWriteFontFamily_Release(ff); return NULL; }
    if (famout) *famout = ff; else IDWriteFontFamily_Release(ff);
    return font;
}

/* ---- per installed font: names, routes, informational strings --------------------------------------------- */
static void sys_names(const WCHAR *fam) {
    LS_NEED_FACTORY(); IDWriteFontCollection *c = sys_coll(f); IDWriteFontFamily *ff = NULL; IDWriteFont *font; IDWriteLocalizedStrings *s = NULL; WCHAR *e;
    if (!c) { t_skip("no system collection"); return; }
    font = sys_font(f, c, fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, &ff);
    if (!font) { IDWriteFontCollection_Release(c); t_skip("%ls not installed", fam); return; }
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(ff, &s));
    CHECK(s != NULL);
    if (s) {
        CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1);
        EXPECT_REFS(s, 1);
        e = ls_en(s);
        CHECK(e && !lstrcmpW(e, fam));
        if (e && lstrcmpW(e, fam)) t_note("family en-us name is %ls", e);
        free(e);
        EXPECT_EQ(ls_sweep(s, "system family names"), 0);
        DW_RELEASE(s);
    }
    CHECK_HR_OK(IDWriteFont_GetFaceNames(font, &s));
    CHECK(s != NULL);
    if (s) {
        CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1);
        e = ls_en(s); CHECK(e != NULL);
        t_note("%ls first face: %ls", fam, e ? e : L"?");
        free(e);
        EXPECT_EQ(ls_sweep(s, "system face names"), 0);
        DW_RELEASE(s);
    }
    IDWriteFont_Release(font); IDWriteFontFamily_Release(ff); IDWriteFontCollection_Release(c);
}
static void sys_routes(const WCHAR *fam) {
    LS_NEED_FACTORY(); IDWriteFontCollection *c = sys_coll(f); IDWriteFont *font; IDWriteFontFace *face = NULL; IDWriteFontFace3 *f3 = NULL;
    IDWriteLocalizedStrings *a = NULL, *b = NULL; UINT32 i, n;
    if (!c) { t_skip("no system collection"); return; }
    font = sys_font(f, c, fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, NULL);
    if (!font) { IDWriteFontCollection_Release(c); t_skip("%ls not installed", fam); return; }
    CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &face));
    if (face && SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3))) {
        /* IDWriteFont::GetFaceNames and IDWriteFontFace3::GetFaceNames describe the same face */
        CHECK_HR_OK(IDWriteFont_GetFaceNames(font, &a));
        CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(f3, &b));
        if (a && b) {
            n = IDWriteLocalizedStrings_GetCount(a);
            EXPECT_EQ(IDWriteLocalizedStrings_GetCount(b), n);
            CHECK(a != b);
            for (i = 0; i < n && i < 64; i++) {
                WCHAR *l1 = ls_dup_locale(a, i), *l2 = ls_dup_locale(b, i), *s1 = ls_dup_string(a, i), *s2 = ls_dup_string(b, i);
                CHECK(l1 && l2 && !lstrcmpW(l1, l2)); CHECK(s1 && s2 && !lstrcmpW(s1, s2));
                free(l1); free(l2); free(s1); free(s2);
            }
        }
        DW_RELEASE(a); DW_RELEASE(b);
        /* WIN32 family names from both interfaces */
        { BOOL ea = FALSE, eb = FALSE;
          CHECK_HR_OK(IDWriteFont_GetInformationalStrings(font, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &a, &ea));
          CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(f3, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &b, &eb));
          EXPECT_EQ(ea, eb); CHECK(ea == TRUE);
          if (a && b) { WCHAR *x = ls_en(a), *y = ls_en(b); CHECK(x && y && !lstrcmpW(x, y)); t_note("%ls win32 family: %ls", fam, x ? x : L"?"); free(x); free(y); }
          DW_RELEASE(a); DW_RELEASE(b); }
        IDWriteFontFace3_Release(f3);
    } else t_skip("IDWriteFontFace3 unsupported");
    DW_RELEASE(face); IDWriteFont_Release(font); IDWriteFontCollection_Release(c);
}
static void sys_info(const WCHAR *fam) {
    LS_NEED_FACTORY(); IDWriteFontCollection *c = sys_coll(f); IDWriteFont *font; int id;
    if (!c) { t_skip("no system collection"); return; }
    font = sys_font(f, c, fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, NULL);
    if (!font) { IDWriteFontCollection_Release(c); t_skip("%ls not installed", fam); return; }
    for (id = 0; id <= 22; id++) {
        IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 77;
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(font, (DWRITE_INFORMATIONAL_STRING_ID)id, &s, &ex));
        if (id == 0 || id > 21) { CHECK(ex == FALSE); CHECK_OUT_NULL(s); continue; }
        if (ex) {
            CHECK(s != NULL);
            if (s) { CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1); EXPECT_EQ(ls_sweep(s, "system info"), 0); }
        } else CHECK(s == NULL);
        /* the inbox fonts always carry these */
        if (id == DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES || id == DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES ||
            id == DWRITE_INFORMATIONAL_STRING_FULL_NAME || id == DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME)
            CHECKF(ex, "info id %d missing", id);
        else if (!ex) t_note("%ls: info id %d absent", fam, id);
        DW_RELEASE(s);
    }
    IDWriteFont_Release(font); IDWriteFontCollection_Release(c);
}
#define SYS_T(n, fam) \
    TEST(lss_##n##_names)  { sys_names(fam); } \
    TEST(lss_##n##_routes) { sys_routes(fam); } \
    TEST(lss_##n##_info)   { sys_info(fam); }
SYS_T(arial, L"Arial") SYS_T(times, L"Times New Roman") SYS_T(courier, L"Courier New") SYS_T(segoe_ui, L"Segoe UI")
SYS_T(tahoma, L"Tahoma") SYS_T(consolas, L"Consolas") SYS_T(marlett, L"Marlett") SYS_T(segoe_symbol, L"Segoe UI Symbol")
SYS_T(segoe_emoji, L"Segoe UI Emoji") SYS_T(bahnschrift, L"Bahnschrift") SYS_T(ms_gothic, L"MS Gothic") SYS_T(verdana, L"Verdana")
SYS_T(georgia, L"Georgia") SYS_T(calibri, L"Calibri") SYS_T(cambria, L"Cambria") SYS_T(lucida_console, L"Lucida Console")
SYS_T(trebuchet, L"Trebuchet MS") SYS_T(comic_sans, L"Comic Sans MS") SYS_T(ms_sans_serif, L"Microsoft Sans Serif") SYS_T(symbol, L"Symbol")

/* ---- face names of the style variants ---------------------------------------------------------------------- */
static void face_name_is(const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st, const WCHAR *want) {
    LS_NEED_FACTORY(); IDWriteFontCollection *c = sys_coll(f); IDWriteFont *font; IDWriteLocalizedStrings *s = NULL; WCHAR *e;
    if (!c) { t_skip("no system collection"); return; }
    font = sys_font(f, c, fam, w, st, NULL);
    if (!font) { IDWriteFontCollection_Release(c); t_skip("%ls not installed", fam); return; }
    if (IDWriteFont_GetWeight(font) != w || IDWriteFont_GetStyle(font) != st) { t_skip("no exact %ls face for weight %d style %d", fam, (int)w, (int)st); }
    else {
        CHECK_HR_OK(IDWriteFont_GetFaceNames(font, &s));
        e = s ? ls_en(s) : NULL;
        CHECKF(e && !lstrcmpW(e, want), "%ls face name \"%ls\" want \"%ls\"", fam, e ? e : L"(none)", want);
        free(e); DW_RELEASE(s);
    }
    IDWriteFont_Release(font); IDWriteFontCollection_Release(c);
}
#define FACE_T(n, fam, w, st, want) TEST(lsf_##n) { face_name_is(fam, w, st, want); }
#define NORM DWRITE_FONT_STYLE_NORMAL
#define ITAL DWRITE_FONT_STYLE_ITALIC
FACE_T(arial_regular, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, NORM, L"Regular")
FACE_T(arial_bold, L"Arial", DWRITE_FONT_WEIGHT_BOLD, NORM, L"Bold")
FACE_T(arial_italic, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, ITAL, L"Italic")
FACE_T(arial_bold_italic, L"Arial", DWRITE_FONT_WEIGHT_BOLD, ITAL, L"Bold Italic")
FACE_T(times_regular, L"Times New Roman", DWRITE_FONT_WEIGHT_NORMAL, NORM, L"Regular")
FACE_T(times_bold, L"Times New Roman", DWRITE_FONT_WEIGHT_BOLD, NORM, L"Bold")
FACE_T(times_italic, L"Times New Roman", DWRITE_FONT_WEIGHT_NORMAL, ITAL, L"Italic")
FACE_T(times_bold_italic, L"Times New Roman", DWRITE_FONT_WEIGHT_BOLD, ITAL, L"Bold Italic")
FACE_T(courier_regular, L"Courier New", DWRITE_FONT_WEIGHT_NORMAL, NORM, L"Regular")
FACE_T(courier_bold, L"Courier New", DWRITE_FONT_WEIGHT_BOLD, NORM, L"Bold")
FACE_T(courier_italic, L"Courier New", DWRITE_FONT_WEIGHT_NORMAL, ITAL, L"Italic")
FACE_T(courier_bold_italic, L"Courier New", DWRITE_FONT_WEIGHT_BOLD, ITAL, L"Bold Italic")
FACE_T(tahoma_regular, L"Tahoma", DWRITE_FONT_WEIGHT_NORMAL, NORM, L"Regular")
FACE_T(tahoma_bold, L"Tahoma", DWRITE_FONT_WEIGHT_BOLD, NORM, L"Bold")
FACE_T(verdana_regular, L"Verdana", DWRITE_FONT_WEIGHT_NORMAL, NORM, L"Regular")
FACE_T(verdana_bold, L"Verdana", DWRITE_FONT_WEIGHT_BOLD, NORM, L"Bold")
FACE_T(verdana_italic, L"Verdana", DWRITE_FONT_WEIGHT_NORMAL, ITAL, L"Italic")
FACE_T(segoe_regular, L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, NORM, L"Regular")
FACE_T(segoe_bold, L"Segoe UI", DWRITE_FONT_WEIGHT_BOLD, NORM, L"Bold")
FACE_T(segoe_italic, L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, ITAL, L"Italic")
FACE_T(segoe_bold_italic, L"Segoe UI", DWRITE_FONT_WEIGHT_BOLD, ITAL, L"Bold Italic")
FACE_T(consolas_regular, L"Consolas", DWRITE_FONT_WEIGHT_NORMAL, NORM, L"Regular")
FACE_T(consolas_bold, L"Consolas", DWRITE_FONT_WEIGHT_BOLD, NORM, L"Bold")
FACE_T(marlett_regular, L"Marlett", DWRITE_FONT_WEIGHT_NORMAL, NORM, L"Regular")

/* ---- whole collection sweep ---------------------------------------------------------------------------------------- */
TEST(lss_collection_sweep) {
    LS_NEED_FACTORY(); IDWriteFontCollection *c = sys_coll(f); UINT32 n, i; int noen = 0, bad = 0;
    if (!c) { t_skip("no system collection"); return; }
    n = IDWriteFontCollection_GetFontFamilyCount(c);
    CHECK(n > 0);
    for (i = 0; i < n && i < 400; i++) {
        IDWriteFontFamily *ff = NULL; IDWriteLocalizedStrings *s = NULL;
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &ff))) { bad++; continue; }
        if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(ff, &s)) && s) {
            CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1);       /* "there is always at least one locale" */
            bad += ls_sweep(s, "family");
            if (ls_index_of(s, L"en-us") < 0) noen++;
        } else bad++;
        DW_RELEASE(s); IDWriteFontFamily_Release(ff);
    }
    EXPECT_EQ(bad, 0);
    t_note("%u families, %d without an en-us name", n, noen);
}
TEST(lss_collection_face_names_sweep) {
    LS_NEED_FACTORY(); IDWriteFontCollection *c = sys_coll(f); UINT32 n, i; int bad = 0, faces = 0;
    if (!c) { t_skip("no system collection"); return; }
    n = IDWriteFontCollection_GetFontFamilyCount(c);
    for (i = 0; i < n && i < 120; i++) {
        IDWriteFontFamily *ff = NULL; UINT32 fc, j;
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &ff))) { bad++; continue; }
        fc = IDWriteFontFamily_GetFontCount(ff);
        for (j = 0; j < fc && j < 12; j++) {
            IDWriteFont *font = NULL; IDWriteLocalizedStrings *s = NULL;
            if (FAILED(IDWriteFontFamily_GetFont(ff, j, &font))) { bad++; continue; }
            if (SUCCEEDED(IDWriteFont_GetFaceNames(font, &s)) && s) { CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1); bad += ls_sweep(s, "face"); faces++; } else bad++;
            DW_RELEASE(s); IDWriteFont_Release(font);
        }
        IDWriteFontFamily_Release(ff);
    }
    EXPECT_EQ(bad, 0);
    t_note("%d faces checked", faces);
}
TEST(lss_find_family_name_each_locale) {
    /* every family name of Arial (all locales) resolves back to the same family */
    LS_NEED_FACTORY(); IDWriteFontCollection *c = sys_coll(f); IDWriteFontFamily *ff = NULL; IDWriteFont *font; IDWriteLocalizedStrings *s = NULL; UINT32 i, n, idx0 = 0; BOOL ex;
    if (!c) { t_skip("no system collection"); return; }
    font = sys_font(f, c, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, NORM, &ff);
    if (!font) { IDWriteFontCollection_Release(c); t_skip("Arial not installed"); return; }
    IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx0, &ex);
    IDWriteFontFamily_GetFamilyNames(ff, &s);
    if (s) {
        n = IDWriteLocalizedStrings_GetCount(s);
        for (i = 0; i < n; i++) {
            WCHAR *nm = ls_dup_string(s, i); UINT32 idx = 0xFFFF; BOOL e2 = FALSE;
            if (nm) { CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, nm, &idx, &e2)); CHECK(e2); EXPECT_EQ(idx, idx0); }
            free(nm);
        }
        DW_RELEASE(s);
    }
    IDWriteFont_Release(font); IDWriteFontFamily_Release(ff); IDWriteFontCollection_Release(c);
}

/* ---- lifetime ---------------------------------------------------------------------------------------------------------- */
TEST(lss_strings_outlive_everything) {
    /* The strings object keeps its data alive: release font, family, collection and the factory itself, then read. */
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontCollection *c = NULL; IDWriteFont *font = NULL; IDWriteFontFamily *ff = NULL;
    IDWriteLocalizedStrings *fam = NULL, *face = NULL, *info = NULL; BOOL ex = FALSE; ULONG r;
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return; }
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE));
    font = sys_font(f, c, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, NORM, &ff);
    if (!font) { DW_RELEASE(c); IDWriteFactory_Release(f); t_skip("Arial not installed"); return; }
    IDWriteFontFamily_GetFamilyNames(ff, &fam);
    IDWriteFont_GetFaceNames(font, &face);
    IDWriteFont_GetInformationalStrings(font, DWRITE_INFORMATIONAL_STRING_FULL_NAME, &info, &ex);
    IDWriteFont_Release(font); IDWriteFontFamily_Release(ff); IDWriteFontCollection_Release(c);
    r = IDWriteFactory_Release(f);
    EXPECT_EQ(r, 0);
    if (fam && face && info) {
        WCHAR *a = ls_en(fam), *b = ls_en(face), *d = ls_en(info);
        CHECK(a && !lstrcmpW(a, L"Arial")); CHECK(b && !lstrcmpW(b, L"Regular")); CHECK(d && !lstrcmpW(d, L"Arial"));
        EXPECT_EQ(ls_sweep(fam, "fam"), 0); EXPECT_EQ(ls_sweep(face, "face"), 0); EXPECT_EQ(ls_sweep(info, "info"), 0);
        free(a); free(b); free(d);
    } else CHECK(0);
    DW_RELEASE(fam); DW_RELEASE(face); DW_RELEASE(info);
}
TEST(lss_custom_strings_outlive_loader) {
    /* after the font file loader is unregistered and released the strings are still readable */
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s = NULL; HRESULT hr;
    memset(&x, 0, sizeof x);
    hr = ls_open_multi_face(&x, f);
    if (FAILED(hr) || !x.face3) { ls_close(&x); t_skip("fixture"); return; }
    CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.face3, &s));
    ls_close(&x);                       /* face, file, loader gone */
    if (s) {
        WCHAR *e = ls_en(s);
        CHECK(e && !lstrcmpW(e, L"LsFam")); free(e);
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 5);
        EXPECT_EQ(ls_sweep(s, "after loader"), 0);
        DW_RELEASE(s);
    }
}
TEST(lss_many_objects_independent) {
    /* 50 live strings objects from one face; releasing in a scrambled order keeps the others valid */
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s[50]; int i;
    HRESULT hr = ls_open_multi_face(&x, f);
    if (FAILED(hr) || !x.face3) { ls_close(&x); t_skip("fixture"); return; }
    for (i = 0; i < 50; i++) { s[i] = NULL; IDWriteFontFace3_GetFaceNames(x.face3, &s[i]); CHECK(s[i] != NULL); }
    for (i = 0; i < 50; i++) for (int j = i + 1; j < 50; j++) CHECK(s[i] != s[j]);
    for (i = 0; i < 50; i += 2) DW_RELEASE(s[i]);
    for (i = 1; i < 50; i += 2) { WCHAR *e = ls_en(s[i]); CHECK(e && !lstrcmpW(e, L"Regular")); free(e); }
    for (i = 1; i < 50; i += 2) DW_RELEASE(s[i]);
    ls_close(&x);
}

/* ---- GDI interop ---------------------------------------------------------------------------------------------------------------- */
TEST(lss_gdi_logfont_names) {
    LS_NEED_FACTORY(); IDWriteGdiInterop *gi = NULL; IDWriteFont *font = NULL; LOGFONTW lf; IDWriteLocalizedStrings *s = NULL; WCHAR *e;
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    memset(&lf, 0, sizeof lf); lf.lfHeight = 12; lf.lfWeight = FW_NORMAL; lstrcpyW(lf.lfFaceName, L"Arial");
    CHECK_HR_OK(IDWriteGdiInterop_CreateFontFromLOGFONT(gi, &lf, &font));
    if (font) {
        CHECK_HR_OK(IDWriteFont_GetFaceNames(font, &s));
        e = s ? ls_en(s) : NULL; CHECK(e && !lstrcmpW(e, L"Regular")); free(e); DW_RELEASE(s);
        { IDWriteFontFamily *ff = NULL; CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &ff));
          if (ff) { CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(ff, &s)); e = s ? ls_en(s) : NULL; CHECK(e && !lstrcmpW(e, L"Arial")); free(e); DW_RELEASE(s); IDWriteFontFamily_Release(ff); } }
        IDWriteFont_Release(font);
    }
    DW_RELEASE(gi);
}
TEST(lss_font_file_on_disk_names) {
    /* the same font opened from a file path: names equal the collection route */
    LS_NEED_FACTORY(); static const char *const cand[] = { "arial.ttf", NULL }; char path[MAX_PATH]; WCHAR wp[MAX_PATH]; IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL; IDWriteFontFace3 *f3 = NULL;
    BOOL sup; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf; IDWriteLocalizedStrings *s = NULL; WCHAR *e;
    if (!dw_find_system_font(cand, path, sizeof path)) { t_skip("arial.ttf not found"); return; }
    dw_wide(path, wp, MAX_PATH);
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file));
    if (!file) return;
    CHECK_HR_OK(IDWriteFontFile_Analyze(file, &sup, &ft, &fct, &nf));
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(f, fct, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face));
    if (face && SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3))) {
        CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(f3, &s)); e = s ? ls_en(s) : NULL; CHECK(e && !lstrcmpW(e, L"Arial")); free(e); DW_RELEASE(s);
        CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(f3, &s)); e = s ? ls_en(s) : NULL; CHECK(e && !lstrcmpW(e, L"Regular")); free(e); DW_RELEASE(s);
        IDWriteFontFace3_Release(f3);
    }
    DW_RELEASE(face); DW_RELEASE(file);
}

/* ---- font sets / face references --------------------------------------------------------------------------------------------------- */
TEST(lsr_system_fontset_property_values) {
    LS_NEED_FACTORY(); IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *set = NULL; UINT32 n, i; int bad = 0;
    static const DWRITE_FONT_PROPERTY_ID ids[] = { DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME,
        DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, DWRITE_FONT_PROPERTY_ID_FULL_NAME, DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME };
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &set));
    if (!set) return;
    n = IDWriteFontSet_GetFontCount(set);
    CHECK(n > 0);
    for (i = 0; i < n && i < 60; i++) {
        size_t k;
        for (k = 0; k < sizeof ids / sizeof ids[0]; k++) {
            IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 77; HRESULT hr = IDWriteFontSet_GetPropertyValues(set, i, ids[k], &ex, &s);
            CHECK_HR_OK(hr);
            if (ex) { CHECK(s != NULL); if (s) { CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1); bad += ls_sweep(s, "fontset"); } }
            else { CHECK(s == NULL); if (ids[k] == DWRITE_FONT_PROPERTY_ID_FULL_NAME || ids[k] == DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME) bad++; }
            DW_RELEASE(s);
        }
    }
    EXPECT_EQ(bad, 0);
    IDWriteFontSet_Release(set);
}
TEST(lsr_fontset_property_none_and_invalid) {
    LS_NEED_FACTORY(); IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *set = NULL; size_t k;
    static const int ids[] = { 0, 14, 15, 100, 0x7FFFFFFF };
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &set));
    if (!set || IDWriteFontSet_GetFontCount(set) == 0) { DW_RELEASE(set); t_skip("empty set"); return; }
    for (k = 0; k < sizeof ids / sizeof ids[0]; k++) {
        IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 77;
        HRESULT hr = IDWriteFontSet_GetPropertyValues(set, 0, (DWRITE_FONT_PROPERTY_ID)ids[k], &ex, &s);
        t_note("GetPropertyValues(id %d) -> %s exists=%d strings=%p", ids[k], dw_hr_name(hr), ex, (void *)s);
        if (SUCCEEDED(hr)) { CHECK(ex == (s != NULL)); } else { CHECK(ex == FALSE); CHECK_OUT_NULL(s); }
        DW_RELEASE(s);
    }
    IDWriteFontSet_Release(set);
}
/* In-memory font -> face reference -> font set */
static HRESULT build_set(ls_fx *x, IDWriteFactory *f, IDWriteFontFaceReference **ref, IDWriteFontSet **set) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSetBuilder *bld = NULL; HRESULT hr;
    *ref = NULL; *set = NULL;
    if (!f3) return S_FALSE;
    hr = ls_open_multi_face(x, f); if (FAILED(hr)) return hr;
    hr = IDWriteFactory3_CreateFontFaceReference_(f3, x->h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, ref); if (FAILED(hr)) return hr;
    hr = IDWriteFactory3_CreateFontSetBuilder(f3, &bld); if (FAILED(hr)) return hr;
    hr = IDWriteFontSetBuilder_AddFontFaceReference(bld, *ref); if (FAILED(hr)) { IDWriteFontSetBuilder_Release(bld); return hr; }
    hr = IDWriteFontSetBuilder_CreateFontSet(bld, set);
    IDWriteFontSetBuilder_Release(bld);
    return hr;
}
static void set_prop(DWRITE_FONT_PROPERTY_ID id, const WCHAR *want_en, unsigned want_count) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteFontFaceReference *ref; IDWriteFontSet *set; HRESULT hr = build_set(&x, f, &ref, &set);
    if (hr == S_FALSE || FAILED(hr)) { DW_RELEASE(ref); DW_RELEASE(set); ls_close(&x); t_skip("font set fixture: %s", dw_hr_name(hr)); return; }
    EXPECT_EQ(IDWriteFontSet_GetFontCount(set), 1);
    { IDWriteLocalizedStrings *s = NULL; BOOL ex = FALSE;
      CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(set, 0, id, &ex, &s));
      if (want_en) {
          CHECK(ex == TRUE); CHECK(s != NULL);
          if (s) { WCHAR *e = ls_en(s); CHECKF(e && !lstrcmpW(e, want_en), "en-us \"%ls\" want \"%ls\"", e ? e : L"(none)", want_en);
                   if (want_count) EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), want_count);
                   EXPECT_EQ(ls_sweep(s, "set prop"), 0); free(e); }
      } else {
          t_note("property %d: exists=%d count=%u", (int)id, ex, s ? IDWriteLocalizedStrings_GetCount(s) : 0);
          if (ex && s) EXPECT_EQ(ls_sweep(s, "set prop"), 0);
      }
      DW_RELEASE(s); }
    IDWriteFontSet_Release(set); IDWriteFontFaceReference_Release(ref); ls_close(&x);
}
TEST(lsr_set_full_name) { set_prop(DWRITE_FONT_PROPERTY_ID_FULL_NAME, L"LsFam", 2); }
TEST(lsr_set_postscript_name) { set_prop(DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, L"LsFam", 1); }
TEST(lsr_set_win32_family) { set_prop(DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, L"LsFam", 5); }
TEST(lsr_set_wss_family) { set_prop(DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, NULL, 0); }
TEST(lsr_set_typographic_family) { set_prop(DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, NULL, 0); }
TEST(lsr_set_face_name) { set_prop(DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, NULL, 0); }
TEST(lsr_set_typographic_face_name) { set_prop(DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME, NULL, 0); }
TEST(lsr_set_numeric_properties) {
    set_prop(DWRITE_FONT_PROPERTY_ID_WEIGHT, NULL, 0); set_prop(DWRITE_FONT_PROPERTY_ID_STRETCH, NULL, 0); set_prop(DWRITE_FONT_PROPERTY_ID_STYLE, NULL, 0);
    set_prop(DWRITE_FONT_PROPERTY_ID_DESIGN_SCRIPT_LANGUAGE_TAG, NULL, 0); set_prop(DWRITE_FONT_PROPERTY_ID_SUPPORTED_SCRIPT_LANGUAGE_TAG, NULL, 0);
}
TEST(lsr_set_strings_independent_of_set) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteFontFaceReference *ref; IDWriteFontSet *set; HRESULT hr = build_set(&x, f, &ref, &set);
    IDWriteLocalizedStrings *s = NULL; BOOL ex;
    if (hr == S_FALSE || FAILED(hr)) { DW_RELEASE(ref); DW_RELEASE(set); ls_close(&x); t_skip("font set fixture: %s", dw_hr_name(hr)); return; }
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &ex, &s));
    IDWriteFontSet_Release(set); IDWriteFontFaceReference_Release(ref); ls_close(&x);
    if (s) { WCHAR *e = ls_en(s); CHECK(e && !lstrcmpW(e, L"LsFam")); free(e); DW_RELEASE(s); }
}
TEST(lsr_reference_face_names_match_direct_face) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteFontFaceReference *ref; IDWriteFontSet *set; HRESULT hr = build_set(&x, f, &ref, &set);
    IDWriteFontFace3 *rf = NULL;
    if (hr == S_FALSE || FAILED(hr)) { DW_RELEASE(ref); DW_RELEASE(set); ls_close(&x); t_skip("font set fixture: %s", dw_hr_name(hr)); return; }
    CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFace(ref, &rf));
    if (rf) {
        IDWriteLocalizedStrings *a = NULL, *b = NULL; UINT32 i, n;
        CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(rf, &a)); CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.face3, &b));
        if (a && b) {
            n = IDWriteLocalizedStrings_GetCount(a); EXPECT_EQ(IDWriteLocalizedStrings_GetCount(b), n);
            for (i = 0; i < n; i++) { WCHAR *s1 = ls_dup_string(a, i), *s2 = ls_dup_string(b, i), *l1 = ls_dup_locale(a, i), *l2 = ls_dup_locale(b, i);
                CHECK(s1 && s2 && !lstrcmpW(s1, s2) && l1 && l2 && !lstrcmpW(l1, l2)); free(s1); free(s2); free(l1); free(l2); }
        }
        DW_RELEASE(a); DW_RELEASE(b);
        CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(rf, &a)); CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(x.face3, &b));
        if (a && b) { WCHAR *s1 = ls_en(a), *s2 = ls_en(b); CHECK(s1 && s2 && !lstrcmpW(s1, s2)); free(s1); free(s2); }
        DW_RELEASE(a); DW_RELEASE(b);
        IDWriteFontFace3_Release(rf);
    }
    IDWriteFontSet_Release(set); IDWriteFontFaceReference_Release(ref); ls_close(&x);
}
TEST(lsr_reference_simulated_face_names) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteFontFaceReference *ref; IDWriteFontSet *set; HRESULT hr = build_set(&x, f, &ref, &set);
    IDWriteFontFace3 *rf = NULL;
    if (hr == S_FALSE || FAILED(hr)) { DW_RELEASE(ref); DW_RELEASE(set); ls_close(&x); t_skip("font set fixture: %s", dw_hr_name(hr)); return; }
    CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFaceWithSimulations(ref, DWRITE_FONT_SIMULATIONS_BOLD, &rf));
    if (rf) {
        IDWriteLocalizedStrings *a = NULL; WCHAR *e;
        CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(rf, &a));
        e = a ? ls_en(a) : NULL; CHECK(e && !lstrcmpW(e, L"LsFam")); free(e); DW_RELEASE(a);
        CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(rf, &a));
        e = a ? ls_en(a) : NULL; t_note("bold-simulated face name: %ls", e ? e : L"?"); CHECK(e != NULL); free(e);
        if (a) EXPECT_EQ(ls_sweep(a, "simulated"), 0);
        DW_RELEASE(a);
        IDWriteFontFace3_Release(rf);
    }
    IDWriteFontSet_Release(set); IDWriteFontFaceReference_Release(ref); ls_close(&x);
}
