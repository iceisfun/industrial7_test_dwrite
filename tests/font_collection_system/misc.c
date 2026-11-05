/* System font collection: structure of well-known families, symbol fonts, design metrics, name objects and GDI interop. */
#include "fcs.h"

/* Family handle for a well-known name, or skip. */
#define NEED_FAMILY(name) \
    NEED_SYS(); IDWriteFontFamily *fam = NULL; \
    { UINT32 idx_ = FIND(c, name); \
      if (idx_ == 0xFFFFFFFFu || FAILED(IDWriteFontCollection_GetFontFamily(c, idx_, &fam)) || !fam) { t_skip("%s not installed", fcs_u8(name)); END_SYS(); return; } }
#define END_FAMILY_T() IDWriteFontFamily_Release(fam); END_SYS()

static int family_has(IDWriteFontFamily *fam, int weight, int style) {
    UINT32 n = IDWriteFontFamily_GetFontCount(fam);
    for (UINT32 i = 0; i < n; i++) {
        IDWriteFont *f = NULL; int hit = 0;
        if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, i, &f)) && f) {
            hit = (int)IDWriteFont_GetWeight(f) == weight && (int)IDWriteFont_GetStyle(f) == style && IDWriteFont_GetStretch(f) == DWRITE_FONT_STRETCH_NORMAL;
            IDWriteFont_Release(f);
        }
        if (hit) return 1;
    }
    return 0;
}

/* ---- symbol fonts -------------------------------------------------------------------------------------------- */
static void symbol_check(const WCHAR *name, int want_symbol) {
    NEED_FAMILY(name);
    IDWriteFont *font = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font));
    if (font) {
        BOOL s = IDWriteFont_IsSymbolFont(font);
        CHECKF(s == want_symbol, "%s: IsSymbolFont %d want %d", fcs_u8(name), s, want_symbol);
        IDWriteFont_Release(font);
    }
    END_FAMILY_T();
}
#define SYMTEST(id, str, want) TEST(sym_##id) { symbol_check(L##str, want); }
SYMTEST(marlett, "Marlett", 1)
SYMTEST(wingdings, "Wingdings", 1)
SYMTEST(webdings, "Webdings", 1)
SYMTEST(symbol, "Symbol", 1)
SYMTEST(arial, "Arial", 0)
SYMTEST(times, "Times New Roman", 0)
SYMTEST(segoe_ui, "Segoe UI", 0)
SYMTEST(courier_new, "Courier New", 0)
SYMTEST(consolas, "Consolas", 0)
SYMTEST(segoe_ui_symbol, "Segoe UI Symbol", 0)

/* ---- design units per em of fonts whose tables are stable on every Windows 10 build ------------------------ */
static void upem_check(const WCHAR *name, unsigned want) {
    NEED_FAMILY(name);
    IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
    DW_REQUIRE_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font));
    DW_REQUIRE_OK(IDWriteFont_CreateFontFace(font, &face));
    DWRITE_FONT_METRICS m; memset(&m, 0, sizeof m);
    IDWriteFontFace_GetMetrics(face, &m);
    CHECKF(m.designUnitsPerEm == want, "%s: designUnitsPerEm %u want %u", fcs_u8(name), m.designUnitsPerEm, want);
    UINT32 len = 0; uint8_t *head = dw_face_table(face, DW_TAG('h', 'e', 'a', 'd'), &len);
    if (head && len >= 20) CHECKF(((head[18] << 8) | head[19]) == want, "%s: head.unitsPerEm %u", fcs_u8(name), (head[18] << 8) | head[19]);
    free(head);
    CHECK(m.ascent > 0 && m.descent > 0);
    IDWriteFontFace_Release(face); IDWriteFont_Release(font);
    END_FAMILY_T();
}
#define UPEMTEST(id, str, want) TEST(upem_##id) { upem_check(L##str, want); }
UPEMTEST(arial, "Arial", 2048)
UPEMTEST(times, "Times New Roman", 2048)
UPEMTEST(courier_new, "Courier New", 2048)
UPEMTEST(segoe_ui, "Segoe UI", 2048)
UPEMTEST(verdana, "Verdana", 2048)
UPEMTEST(tahoma, "Tahoma", 2048)
UPEMTEST(consolas, "Consolas", 2048)
UPEMTEST(calibri, "Calibri", 2048)
UPEMTEST(georgia, "Georgia", 2048)

/* ---- structure of well-known families ---------------------------------------------------------------------- */
TEST(arial_has_riBBi) {
    NEED_FAMILY(L"Arial");
    CHECK(family_has(fam, 400, DWRITE_FONT_STYLE_NORMAL));
    CHECK(family_has(fam, 700, DWRITE_FONT_STYLE_NORMAL));
    CHECK(family_has(fam, 400, DWRITE_FONT_STYLE_ITALIC));
    CHECK(family_has(fam, 700, DWRITE_FONT_STYLE_ITALIC));
    CHECK(IDWriteFontFamily_GetFontCount(fam) >= 4);
    t_note("Arial: %u fonts", IDWriteFontFamily_GetFontCount(fam));
    END_FAMILY_T();
}
TEST(times_new_roman_has_riBBi) {
    NEED_FAMILY(L"Times New Roman");
    CHECK(family_has(fam, 400, DWRITE_FONT_STYLE_NORMAL));
    CHECK(family_has(fam, 700, DWRITE_FONT_STYLE_NORMAL));
    CHECK(family_has(fam, 400, DWRITE_FONT_STYLE_ITALIC));
    CHECK(family_has(fam, 700, DWRITE_FONT_STYLE_ITALIC));
    END_FAMILY_T();
}
TEST(courier_new_has_riBBi) {
    NEED_FAMILY(L"Courier New");
    CHECK(family_has(fam, 400, DWRITE_FONT_STYLE_NORMAL));
    CHECK(family_has(fam, 700, DWRITE_FONT_STYLE_NORMAL));
    CHECK(family_has(fam, 400, DWRITE_FONT_STYLE_ITALIC));
    CHECK(family_has(fam, 700, DWRITE_FONT_STYLE_ITALIC));
    END_FAMILY_T();
}
TEST(segoe_ui_has_regular_and_bold) {
    NEED_FAMILY(L"Segoe UI");
    CHECK(family_has(fam, 400, DWRITE_FONT_STYLE_NORMAL));
    CHECK(family_has(fam, 700, DWRITE_FONT_STYLE_NORMAL));
    t_note("Segoe UI (weight/stretch/style model): %u fonts", IDWriteFontFamily_GetFontCount(fam));
    END_FAMILY_T();
}
TEST(weight_boundaries_map_to_extremes) {
    /* A request for the lightest/heaviest weight is answered with some font of the family (nearest weight). */
    NEED_FAMILY(L"Arial");
    IDWriteFont *light = NULL, *heavy = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, (DWRITE_FONT_WEIGHT)1, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &light));
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, (DWRITE_FONT_WEIGHT)999, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &heavy));
    if (light && heavy) CHECK(IDWriteFont_GetWeight(light) <= IDWriteFont_GetWeight(heavy));
    DW_RELEASE(light); DW_RELEASE(heavy);
    END_FAMILY_T();
}
TEST(arial_bold_request_returns_bold) {
    NEED_FAMILY(L"Arial");
    IDWriteFont *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &b));
    if (b) { CHECK(IDWriteFont_GetWeight(b) == DWRITE_FONT_WEIGHT_BOLD); CHECK(IDWriteFont_GetStyle(b) == DWRITE_FONT_STYLE_NORMAL); CHECK(IDWriteFont_GetSimulations(b) == DWRITE_FONT_SIMULATIONS_NONE); IDWriteFont_Release(b); }
    END_FAMILY_T();
}
TEST(arial_italic_request_returns_italic) {
    NEED_FAMILY(L"Arial");
    IDWriteFont *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &b));
    if (b) { CHECK(IDWriteFont_GetStyle(b) == DWRITE_FONT_STYLE_ITALIC); CHECK(IDWriteFont_GetSimulations(b) == DWRITE_FONT_SIMULATIONS_NONE); IDWriteFont_Release(b); }
    END_FAMILY_T();
}
TEST(arial_family_names_en_us) {
    NEED_FAMILY(L"Arial");
    IDWriteLocalizedStrings *s = NULL; UINT32 idx = 0; BOOL ex = 0;
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &s));
    if (s) {
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"en-us", &idx, &ex));
        CHECK(ex == 1);
        if (ex) { char nm[64]; CHECK_HR_OK(dw_ls_get(s, idx, nm, sizeof nm)); EXPECT_STR(nm, "Arial"); }
        IDWriteLocalizedStrings_Release(s);
    }
    END_FAMILY_T();
}
TEST(family_names_two_calls_independent_objects) {
    NEED_FAMILY(L"Arial");
    IDWriteLocalizedStrings *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &a));
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &b));
    CHECK(a != NULL && b != NULL && a != b);
    if (a && b) {
        CHECK(IDWriteLocalizedStrings_GetCount(a) == IDWriteLocalizedStrings_GetCount(b));
        char x[128], y[128]; dw_ls_en(a, x, sizeof x); dw_ls_en(b, y, sizeof y);
        EXPECT_STR(x, y);
        ULONG ra = dw_refcount((IUnknown *)a);
        IDWriteLocalizedStrings_Release(b); b = NULL;
        EXPECT_REFS(a, ra);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    END_FAMILY_T();
}
TEST(family_names_outlive_family_and_collection) {
    NEED_FAMILY(L"Arial");
    IDWriteLocalizedStrings *s = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &s));
    IDWriteFontFamily_Release(fam); fam = NULL;
    END_SYS(); c = NULL;
    if (s) { char nm[64]; CHECK_HR_OK(dw_ls_en(s, nm, sizeof nm)); CHECK(nm[0] != 0); IDWriteLocalizedStrings_Release(s); }
}
TEST(family_survives_second_collection_get) {
    NEED_FAMILY(L"Arial");
    IDWriteFontCollection *c2 = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &c2, TRUE));
    CHECK(c2 == c);
    IDWriteFontCollection *owner = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(fam, &owner));
    CHECK(owner == c2);
    DW_RELEASE(owner); DW_RELEASE(c2);
    END_FAMILY_T();
}
TEST(font_faces_equal_for_same_font) {
    NEED_FAMILY(L"Arial");
    IDWriteFont *a = NULL, *b = NULL; IDWriteFontFace *fa = NULL, *fb = NULL;
    DW_REQUIRE_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, 400, 5, DWRITE_FONT_STYLE_NORMAL, &a));
    DW_REQUIRE_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, 400, 5, DWRITE_FONT_STYLE_NORMAL, &b));
    CHECK(a != b);
    DW_REQUIRE_OK(IDWriteFont_CreateFontFace(a, &fa));
    DW_REQUIRE_OK(IDWriteFont_CreateFontFace(b, &fb));
    CHECK(fa == fb);   /* faces are cached per factory by file key and index */
    DW_RELEASE(fa); DW_RELEASE(fb); DW_RELEASE(a); DW_RELEASE(b);
    END_FAMILY_T();
}
TEST(get_font_from_face_returns_equal_properties_arial_bold) {
    NEED_FAMILY(L"Arial");
    IDWriteFont *b = NULL; IDWriteFontFace *face = NULL; IDWriteFont *back = NULL;
    DW_REQUIRE_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, 700, 5, DWRITE_FONT_STYLE_NORMAL, &b));
    DW_REQUIRE_OK(IDWriteFont_CreateFontFace(b, &face));
    CHECK_HR_OK(IDWriteFontCollection_GetFontFromFontFace(c, face, &back));
    if (back) {
        CHECK(IDWriteFont_GetWeight(back) == 700);
        CHECK(IDWriteFont_GetStyle(back) == DWRITE_FONT_STYLE_NORMAL);
        CHECK(IDWriteFont_GetStretch(back) == DWRITE_FONT_STRETCH_NORMAL);
        IDWriteFontFamily *bf = NULL;
        if (SUCCEEDED(IDWriteFont_GetFontFamily(back, &bf)) && bf) { char n[64]; fcs_family_en(bf, n, sizeof n); EXPECT_STR(n, "Arial"); IDWriteFontFamily_Release(bf); }
        IDWriteFont_Release(back);
    }
    DW_RELEASE(face); DW_RELEASE(b);
    END_FAMILY_T();
}
TEST(get_font_from_face_out_cleared_on_failure) {
    NEED_SYS();
    dwx_font h; memset(&h, 0, sizeof h);
    if (FAILED(dwx_font_open_default(&h, f)) || !h.face) { dwx_font_close(&h); t_skip("synthetic face unavailable"); END_SYS(); return; }
    for (int i = 0; i < 3; i++) {
        IDWriteFont *got = (IDWriteFont *)(ULONG_PTR)(0x10 * (i + 1));
        CHECK_HR(IDWriteFontCollection_GetFontFromFontFace(c, h.face, &got), FCS_NOFONT);
        CHECK_OUT_NULL(got);
    }
    dwx_font_close(&h);
    END_SYS();
}

/* ---- GDI interop sees the same system fonts ---------------------------------------------------------------- */
static void gdi_family(const WCHAR *face) {
    NEED_SYS();
    IDWriteGdiInterop *gi = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    lf.lfHeight = -16; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
    UINT32 idx = FIND(c, face);
    IDWriteFont *font = NULL;
    HRESULT hr = IDWriteGdiInterop_CreateFontFromLOGFONT(gi, &lf, &font);
    if (idx != 0xFFFFFFFFu) {
        CHECKF(hr == S_OK && font, "%s: CreateFontFromLOGFONT %s", fcs_u8(face), dw_hr_name(hr));
        if (font) {
            IDWriteFontFamily *fam = NULL;
            CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &fam));
            if (fam) {
                char got[200], want[200];
                fcs_family_en(fam, got, sizeof got);
                dw_narrow(face, want, sizeof want);
                CHECKF(fcs_ieq(got, want), "GDI font for \"%s\" belongs to family \"%s\"", want, got);
                IDWriteFontCollection *owner = NULL;
                if (SUCCEEDED(IDWriteFontFamily_GetFontCollection(fam, &owner)) && owner) {
                    if (owner != c) t_note("GDI-interop font lives in another collection object than GetSystemFontCollection");
                    DW_RELEASE(owner);
                }
                IDWriteFontFamily_Release(fam);
            }
        }
    } else t_note("\"%s\" not installed (CreateFontFromLOGFONT -> %s)", fcs_u8(face), dw_hr_name(hr));
    DW_RELEASE(font); IDWriteGdiInterop_Release(gi);
    END_SYS();
}
#define GDITEST(id, str) TEST(gdi_##id) { gdi_family(L##str); }
GDITEST(arial, "Arial")
GDITEST(times, "Times New Roman")
GDITEST(courier_new, "Courier New")
GDITEST(tahoma, "Tahoma")
GDITEST(verdana, "Verdana")
GDITEST(segoe_ui, "Segoe UI")
TEST(gdi_unknown_face_name_is_not_in_collection) {
    NEED_SYS();
    IDWriteGdiInterop *gi = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    LOGFONTW lf; memset(&lf, 0, sizeof lf);
    wcscpy(lf.lfFaceName, L"NoSuchFamilyXYZ"); lf.lfHeight = -12; lf.lfWeight = FW_NORMAL;
    IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1;
    HRESULT hr = IDWriteGdiInterop_CreateFontFromLOGFONT(gi, &lf, &font);
    t_note("CreateFontFromLOGFONT(unknown face) -> %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(font); else { CHECK(font != NULL); DW_RELEASE(font); }
    IDWriteGdiInterop_Release(gi);
    END_SYS();
}

/* ---- text format / layout resolve family names through the collection ------------------------------------- */
TEST(text_format_unknown_family_creates_ok) {
    /* A missing family is not an error at creation time; the format keeps the name. */
    NEED_SYS();
    IDWriteTextFormat *tf = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"NoSuchFamilyXYZ", c, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf));
    if (tf) {
        UINT32 len = IDWriteTextFormat_GetFontFamilyNameLength(tf);
        CHECK(len == wcslen(L"NoSuchFamilyXYZ"));
        WCHAR nm[64]; CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(tf, nm, 64)); CHECK(!wcscmp(nm, L"NoSuchFamilyXYZ"));
        IDWriteTextFormat_Release(tf);
    }
    END_SYS();
}
TEST(text_layout_metrics_use_collection_font) {
    NEED_FAMILY(L"Arial");
    IDWriteTextFormat *tf = NULL; IDWriteTextLayout *tl = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", c, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &tf));
    DW_REQUIRE_OK(IDWriteFactory_CreateTextLayout(f, L"Hello", 5, tf, 1000.0f, 1000.0f, &tl));
    DWRITE_TEXT_METRICS tm; memset(&tm, 0, sizeof tm);
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(tl, &tm));
    CHECK(tm.width > 0.0f && tm.height > 0.0f && tm.lineCount == 1);
    IDWriteFontCollection *lc = NULL;
    CHECK_HR_OK(IDWriteTextLayout_GetFontCollection(tl, 0, &lc, NULL));
    CHECK(lc == c);
    DW_RELEASE(lc); IDWriteTextLayout_Release(tl); IDWriteTextFormat_Release(tf);
    END_FAMILY_T();
}
TEST(text_layout_missing_family_still_lays_out) {
    NEED_SYS();
    IDWriteTextFormat *tf = NULL; IDWriteTextLayout *tl = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateTextFormat(f, L"NoSuchFamilyXYZ", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &tf));
    DW_REQUIRE_OK(IDWriteFactory_CreateTextLayout(f, L"Hello", 5, tf, 1000.0f, 1000.0f, &tl));
    DWRITE_TEXT_METRICS tm; memset(&tm, 0, sizeof tm);
    HRESULT hr = IDWriteTextLayout_GetMetrics(tl, &tm);
    t_note("layout metrics with an unknown family: %s, width %g", dw_hr_name(hr), tm.width);   /* fallback behaviour is another area's contract */
    IDWriteTextLayout_Release(tl); IDWriteTextFormat_Release(tf);
    END_SYS();
}
