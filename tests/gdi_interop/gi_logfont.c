/* IDWriteGdiInterop::CreateFontFromLOGFONT / ConvertFontToLOGFONT against the system font collection.
 *
 * Lookup (DWriteGdiInterop::CreateFontFromLOGFONT -> FindNearestMatchingWin32Font): candidates are the fonts whose
 * WIN32_FAMILY_NAME property equals lfFaceName; when none do, the fonts whose FULL_NAME equals it; when none, DWRITE_E_NOFONT.
 * Among the candidates the lowest Win32ComputePenalty wins (see gi_score() in gi.h).  Only lfFaceName, lfWeight, lfItalic and lfCharSet
 * are read; every other LOGFONT field is ignored.  There is no GDI font substitution and no bitmap-font support. */
#include "gi.h"

static const int gi_weights[] = { 0, 1, 100, 200, 300, 399, 400, 401, 450, 500, 549, 550, 551, 600, 650, 699, 700, 701, 800, 900, 1000, 1001, 2000 };

static HRESULT gi_create(IDWriteGdiInterop *g, const char *face, int w, int it, int cs, IDWriteFont **font)
{
    LOGFONTW lf; gi_lf(&lf, face, w, it, cs);
    *font = (IDWriteFont *)(ULONG_PTR)0x1234;      /* out parameter: must be overwritten */
    return IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, font);
}
static HRESULT gi_conv(IDWriteGdiInterop *g, IDWriteFont *font, LOGFONTW *lf, BOOL *sys)
{
    GI_POISON_LF(*lf); *sys = (BOOL)0x5a5a;
    return IDWriteGdiInterop_ConvertFontToLOGFONT(g, font, lf, sys);
}
static int gi_font_file(IDWriteFont *font, char *base, size_t cap)
{
    IDWriteFontFace *face = NULL; int ok = 0;
    base[0] = 0;
    if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) { ok = gi_face_file(face, base, cap, NULL, 0); IDWriteFontFace_Release(face); }
    return ok;
}
static UINT32 gi_family_count(IDWriteFactory *f, const char *name)
{
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; UINT32 idx = 0, n = 0; BOOL ex = FALSE; WCHAR w[64];
    dw_wide(name, w, 64);
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return 0;
    IDWriteFontCollection_FindFamilyName(c, w, &idx, &ex);
    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam) { n = IDWriteFontFamily_GetFontCount(fam); IDWriteFontFamily_Release(fam); }
    IDWriteFontCollection_Release(c);
    return n;
}

/* ---- one TEST per well-known face: regular request resolves to an outline font whose Win32 family name round-trips ---------------- */
static void gi_sysface(const char *face, int want_weight, const char *want_file, int want_symbol)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font; LOGFONTW lf; BOOL sys; HRESULT hr; char n[64];
    if (!g) { t_skip("no interop"); return; }
    hr = gi_create(g, face, FW_REGULAR, 0, DEFAULT_CHARSET, &font);
    if (hr == DWRITE_E_NOFONT) {
        CHECK_OUT_NULL(font);
        if (gi_sys_has(f, face)) CHECKF(0, "family %s is in the system collection but lookup says NOFONT", face);
        t_skip("%s not installed", face); DW_RELEASE(g); return;
    }
    CHECK_HR_OK(hr);
    if (!font) { DW_RELEASE(g); return; }
    CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
    gi_name_u8(&lf, n, sizeof n);
    CHECKF(!_stricmp(n, face), "lfFaceName \"%s\" vs requested \"%s\"", n, face);
    EXPECT_EQ(sys, TRUE);
    GI_LF_SHAPE(lf);
    if (want_weight) EXPECT_EQ(lf.lfWeight, want_weight);
    EXPECT_EQ(lf.lfItalic, 0);
    if (want_symbol) EXPECT_EQ(lf.lfCharSet, SYMBOL_CHARSET);
    {
        IDWriteFontFace *ff = NULL;
        if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &ff)) && ff) {
            BOOL isym = IDWriteFontFace_IsSymbolFont(ff);
            EXPECT_EQ(lf.lfCharSet, isym ? SYMBOL_CHARSET : DEFAULT_CHARSET);
            if (want_file) { char b[64]; if (gi_face_file(ff, b, sizeof b, NULL, 0)) CHECKF(!strcmp(b, want_file), "file %s want %s", b, want_file); }
            IDWriteFontFace_Release(ff);
        }
    }
    t_note("%s: lfWeight=%ld lfCharSet=%d", face, lf.lfWeight, lf.lfCharSet);
    DW_RELEASE(font); DW_RELEASE(g);
}
#define SYSFACE(id, name, weight, file, sym) TEST(gi_sys_##id) { gi_sysface(name, weight, file, sym); }
SYSFACE(arial, "Arial", 400, "arial.ttf", 0)
SYSFACE(times, "Times New Roman", 400, "times.ttf", 0)
SYSFACE(courier, "Courier New", 400, "cour.ttf", 0)
SYSFACE(tahoma, "Tahoma", 400, "tahoma.ttf", 0)
SYSFACE(segoeui, "Segoe UI", 400, "segoeui.ttf", 0)
SYSFACE(marlett, "Marlett", 0, "marlett.ttf", 1)
SYSFACE(consolas, "Consolas", 400, "consola.ttf", 0)
SYSFACE(verdana, "Verdana", 400, "verdana.ttf", 0)
SYSFACE(georgia, "Georgia", 400, "georgia.ttf", 0)
SYSFACE(calibri, "Calibri", 400, "calibri.ttf", 0)
SYSFACE(cambria, "Cambria", 0, NULL, 0)
SYSFACE(lucida_console, "Lucida Console", 400, "lucon.ttf", 0)
SYSFACE(trebuchet, "Trebuchet MS", 400, "trebuc.ttf", 0)
SYSFACE(comic, "Comic Sans MS", 400, "comic.ttf", 0)
SYSFACE(impact, "Impact", 400, "impact.ttf", 0)
SYSFACE(arial_black, "Arial Black", 900, "ariblk.ttf", 0)
SYSFACE(segoe_semibold, "Segoe UI Semibold", 600, "seguisb.ttf", 0)
SYSFACE(segoe_light, "Segoe UI Light", 300, "segoeuil.ttf", 0)
SYSFACE(segoe_black, "Segoe UI Black", 900, "seguibl.ttf", 0)
SYSFACE(segoe_symbol, "Segoe UI Symbol", 400, "seguisym.ttf", 0)
SYSFACE(segoe_emoji, "Segoe UI Emoji", 400, "seguiemj.ttf", 0)
SYSFACE(symbol, "Symbol", 400, "symbol.ttf", 1)
SYSFACE(wingdings, "Wingdings", 400, "wingding.ttf", 1)
SYSFACE(webdings, "Webdings", 400, "webdings.ttf", 1)
SYSFACE(ms_sans_serif_ttf, "Microsoft Sans Serif", 400, "micross.ttf", 0)
SYSFACE(palatino, "Palatino Linotype", 400, "pala.ttf", 0)
SYSFACE(sylfaen, "Sylfaen", 400, "sylfaen.ttf", 0)
SYSFACE(franklin, "Franklin Gothic Medium", 400, "framd.ttf", 0)
SYSFACE(ms_gothic, "MS Gothic", 400, NULL, 0)
SYSFACE(yu_gothic, "Yu Gothic", 0, NULL, 0)
SYSFACE(bahnschrift, "Bahnschrift", 0, NULL, 0)
SYSFACE(arial_narrow, "Arial Narrow", 400, "arialn.ttf", 0)
SYSFACE(arial_unicode, "Arial Unicode MS", 400, NULL, 0)

/* ---- names that GDI knows as raster/alias faces: DirectWrite does no substitution and has no bitmap fonts ------------------------------------- */
static void gi_alias(const char *face)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font; HRESULT hr; LOGFONTW lf; BOOL sys;
    if (!g) { t_skip("no interop"); return; }
    hr = gi_create(g, face, FW_REGULAR, 0, DEFAULT_CHARSET, &font);
    CHECKF(hr == S_OK || hr == DWRITE_E_NOFONT, "%s -> %s", face, dw_hr_name(hr));   /* the only failure CreateFontFromLOGFONT produces */
    if (hr == S_OK) {
        CHECK(font != NULL);
        if (font) { CHECK_HR_OK(gi_conv(g, font, &lf, &sys)); char n[64]; gi_name_u8(&lf, n, sizeof n); t_note("%s resolved to \"%s\"", face, n); }
    } else CHECK_OUT_NULL(font);
    t_note("%s -> %s (bitmap/alias face)", face, dw_hr_name(hr));
    DW_RELEASE(font); DW_RELEASE(g);
}
#define ALIAS(id, name) TEST(gi_alias_##id) { gi_alias(name); }
ALIAS(ms_shell_dlg, "MS Shell Dlg")
ALIAS(ms_shell_dlg2, "MS Shell Dlg 2")
ALIAS(ms_sans_serif, "MS Sans Serif")
ALIAS(ms_serif, "MS Serif")
ALIAS(system, "System")
ALIAS(terminal, "Terminal")
ALIAS(fixedsys, "Fixedsys")
ALIAS(small_fonts, "Small Fonts")
ALIAS(modern, "Modern")
ALIAS(roman, "Roman")
ALIAS(script, "Script")
ALIAS(helv, "Helv")
ALIAS(tms_rmn, "Tms Rmn")
ALIAS(default_gui, "Default")
ALIAS(sans, "sans-serif")

/* ---- argument validation and failure state ---------------------------------------------------------------------------------------------------- */
TEST(gi_lf_null_logfont)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)0x1234;
    if (!g) { t_skip("no interop"); return; }
    CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, NULL, &font), DW_E_INVALIDARG);
    CHECK_OUT_NULL(font);
    DW_RELEASE(g);
}
TEST(gi_lf_null_logfont_interop1)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteGdiInterop1 *g1; IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)0x1234;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1) { t_skip("no interop1"); DW_RELEASE(g); return; }
    CHECK_HR(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, NULL, NULL, &font), DW_E_INVALIDARG);
    CHECK_OUT_NULL(font);
    DW_RELEASE(g1); DW_RELEASE(g);
}
TEST(gi_lf_nonexistent_names)
{
    static const char *names[] = { "NoSuchFontXyz", "Arial Does Not Exist", "x", "0", "Arialx", "ArialArial", "Times", "Courier", "Sans",
                                   "Segoe", "Tahomaa", "\x01\x02", "###", "Microsoft", "Segoe UI Semibold Italic X" };
    IDWriteGdiInterop *g = gi_get(dw_shared()); size_t i;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        IDWriteFont *font; HRESULT hr = gi_create(g, names[i], 400, 0, DEFAULT_CHARSET, &font);
        CHECKF(hr == DWRITE_E_NOFONT, "\"%s\": %s", names[i], dw_hr_name(hr));
        CHECK_OUT_NULL(font);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_lf_nonexistent_is_exact_hr_value)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteFont *font; HRESULT hr;
    if (!g) { t_skip("no interop"); return; }
    hr = gi_create(g, "NoSuchFontXyz", 400, 0, 1, &font);
    EXPECT_EQ((unsigned)hr, 0x88985002u);
    DW_RELEASE(g);
}
TEST(gi_lf_empty_face_name)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteFont *font; HRESULT hr;
    if (!g) { t_skip("no interop"); return; }
    hr = gi_create(g, "", 400, 0, DEFAULT_CHARSET, &font);
    CHECKF(hr == S_OK || hr == DWRITE_E_NOFONT, "empty name -> %s", dw_hr_name(hr));
    if (hr != S_OK) CHECK_OUT_NULL(font);
    t_note("empty lfFaceName -> %s (GDI would substitute its default face)", dw_hr_name(hr));
    DW_RELEASE(font); DW_RELEASE(g);
}
TEST(gi_lf_all_zero_logfont)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); IDWriteFont *font = NULL; LOGFONTW lf; HRESULT hr;
    if (!g) { t_skip("no interop"); return; }
    memset(&lf, 0, sizeof lf);
    hr = IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font);
    CHECKF(hr == S_OK || hr == DWRITE_E_NOFONT, "all-zero LOGFONT -> %s", dw_hr_name(hr));
    DW_RELEASE(font); DW_RELEASE(g);
}
TEST(gi_lf_case_insensitive)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    static const char *v[] = { "arial", "ARIAL", "aRiAl", "Arial" };
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < 4; i++) {
        IDWriteFont *font; HRESULT hr = gi_create(g, v[i], 400, 0, 1, &font);
        t_note("lfFaceName \"%s\" -> %s", v[i], dw_hr_name(hr));
        if (hr == S_OK) { char b[64]; CHECK(gi_font_file(font, b, sizeof b)); CHECKF(!strcmp(b, "arial.ttf"), "%s", b); }
        else CHECK_OUT_NULL(font);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_lf_whitespace_and_prefix_variants)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    static const char *v[] = { "Arial ", " Arial", "@Arial", "Arial\t", "Ar ial" };
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < 5; i++) {
        IDWriteFont *font; HRESULT hr = gi_create(g, v[i], 400, 0, 1, &font);
        CHECKF(hr == S_OK || hr == DWRITE_E_NOFONT, "%s", dw_hr_name(hr));
        t_note("lfFaceName \"%s\" -> %s", v[i], dw_hr_name(hr));
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_lf_full_name_fallback)
{
    /* no Win32 family called "Arial Bold": the FULL_NAME property is consulted next and finds the bold face */
    static const struct { const char *name; const char *file; int weight; int italic; } rows[] = {
        { "Arial Bold", "arialbd.ttf", 700, 0 }, { "Arial Italic", "ariali.ttf", 400, 1 }, { "Arial Bold Italic", "arialbi.ttf", 700, 1 },
        { "Times New Roman Bold", "timesbd.ttf", 700, 0 }, { "Courier New Bold", "courbd.ttf", 700, 0 }, { "Tahoma Bold", "tahomabd.ttf", 700, 0 },
        { "Segoe UI Bold", "segoeuib.ttf", 700, 0 }, { "Segoe UI Italic", "segoeuii.ttf", 400, 1 },
    };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteFont *font; HRESULT hr = gi_create(g, rows[i].name, 400, 0, DEFAULT_CHARSET, &font);
        if (hr == DWRITE_E_NOFONT) { t_note("%s: NOFONT (font not installed)", rows[i].name); continue; }
        CHECKF(hr == S_OK, "%s -> %s", rows[i].name, dw_hr_name(hr));
        if (font) {
            LOGFONTW lf; BOOL sys; char b[64];
            if (gi_font_file(font, b, sizeof b)) CHECKF(!strcmp(b, rows[i].file), "%s resolved to %s want %s", rows[i].name, b, rows[i].file);
            CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
            EXPECT_EQ(lf.lfItalic, rows[i].italic);
            t_note("%s -> weight %ld italic %d face %ls", rows[i].name, lf.lfWeight, lf.lfItalic, lf.lfFaceName);
        }
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}

/* ---- weight / italic selection ------------------------------------------------------------------------------------------------------------ */
static void gi_model(const char *face, const gi_ent *ents, int nents)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t wi; int it, ties = 0, done = 0;
    if (!g) { t_skip("no interop"); return; }
    if (gi_family_count(f, face) != (UINT32)nents) { t_skip("%s is not a %d-font family on this machine", face, nents); DW_RELEASE(g); return; }
    for (it = 0; it < 2; it++)
        for (wi = 0; wi < sizeof gi_weights / sizeof gi_weights[0]; wi++) {
            int w = gi_weights[wi], sim = 0, best = gi_best(ents, nents, 0xffffffffu, w, it, &sim, NULL);
            IDWriteFont *font; LOGFONTW lf; BOOL sys; HRESULT hr;
            if (best < 0) { ties++; continue; }
            hr = gi_create(g, face, w, it, DEFAULT_CHARSET, &font);
            CHECKF(hr == S_OK, "%s w=%d it=%d -> %s", face, w, it, dw_hr_name(hr));
            if (hr != S_OK || !font) continue;
            CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
            CHECKF(lf.lfWeight == ents[best].weight + ((sim & DWRITE_FONT_SIMULATIONS_BOLD) ? GI_BOLD_SIM_DELTA : 0),
                   "%s req w=%d it=%d: lfWeight %ld, model entry %d weight %d sim %d", face, w, it, lf.lfWeight, best, ents[best].weight, sim);
            CHECKF(lf.lfItalic == ((ents[best].style != 0 || (sim & DWRITE_FONT_SIMULATIONS_OBLIQUE)) ? 1 : 0),
                   "%s req w=%d it=%d: lfItalic %d, model sim %d style %d", face, w, it, lf.lfItalic, sim, ents[best].style);
            CHECKF(IDWriteFont_GetSimulations(font) == (DWRITE_FONT_SIMULATIONS)sim, "%s req w=%d it=%d: simulations %d want %d", face, w, it,
                   (int)IDWriteFont_GetSimulations(font), sim);
            done++;
            DW_RELEASE(font);
        }
    t_note("%s: %d rows checked against the penalty model, %d ambiguous rows skipped", face, done, ties);
    DW_RELEASE(g);
}
static const gi_ent gi_ribbi[] = { { 400, DWRITE_FONT_STYLE_NORMAL, 1, 5 }, { 700, DWRITE_FONT_STYLE_NORMAL, 1, 5 },
                                   { 400, DWRITE_FONT_STYLE_ITALIC, 1, 5 }, { 700, DWRITE_FONT_STYLE_ITALIC, 1, 5 } };
static const gi_ent gi_rb[] = { { 400, DWRITE_FONT_STYLE_NORMAL, 1, 5 }, { 700, DWRITE_FONT_STYLE_NORMAL, 1, 5 } };
TEST(gi_model_arial) { gi_model("Arial", gi_ribbi, 4); }
TEST(gi_model_times) { gi_model("Times New Roman", gi_ribbi, 4); }
TEST(gi_model_courier) { gi_model("Courier New", gi_ribbi, 4); }
TEST(gi_model_verdana) { gi_model("Verdana", gi_ribbi, 4); }
TEST(gi_model_georgia) { gi_model("Georgia", gi_ribbi, 4); }
TEST(gi_model_consolas) { gi_model("Consolas", gi_ribbi, 4); }
TEST(gi_model_tahoma) { gi_model("Tahoma", gi_rb, 2); }
TEST(gi_model_trebuchet) { gi_model("Trebuchet MS", gi_ribbi, 4); }
TEST(gi_model_segoeui_win32_family)
{
    /* the Win32 family "Segoe UI" holds Regular/Bold/Italic/Bold Italic only: Light/Semilight/Semibold/Black have their own Win32
     * family names, so lfWeight 600 on "Segoe UI" is regular + synthetic bold or true bold, never the Semibold font */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font; LOGFONTW lf; BOOL sys;
    int sim = 0, best = gi_best(gi_ribbi, 4, 0xffffffffu, 600, 0, &sim, NULL);
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Segoe UI")) { t_skip("Segoe UI not installed"); DW_RELEASE(g); return; }
    CHECK_HR_OK(gi_create(g, "Segoe UI", 600, 0, DEFAULT_CHARSET, &font));
    if (font) {
        char b[64];
        CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
        CHECKF(!_wcsicmp(lf.lfFaceName, L"Segoe UI"), "face %ls", lf.lfFaceName);
        CHECK(lf.lfWeight != 600);
        if (best >= 0) EXPECT_EQ(lf.lfWeight, gi_ribbi[best].weight + ((sim & 1) ? GI_BOLD_SIM_DELTA : 0));
        if (gi_font_file(font, b, sizeof b)) t_note("Segoe UI lfWeight 600 -> %s lfWeight %ld simulations %d", b, lf.lfWeight, (int)IDWriteFont_GetSimulations(font));
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_segoe_semibold_exact_600)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font; LOGFONTW lf; BOOL sys; int i;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Segoe UI Semibold")) { t_skip("Segoe UI Semibold not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < 3; i++) {
        static const int w[] = { 600, 400, 700 };
        CHECK_HR_OK(gi_create(g, "Segoe UI Semibold", w[i], 0, DEFAULT_CHARSET, &font));   /* a one-font Win32 family: the request weight only decides simulations */
        if (!font) continue;
        CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
        CHECKF(!_wcsicmp(lf.lfFaceName, L"Segoe UI Semibold"), "%ls", lf.lfFaceName);
        t_note("Segoe UI Semibold request %d -> lfWeight %ld simulations %d", w[i], lf.lfWeight, (int)IDWriteFont_GetSimulations(font));
        if (w[i] == 600 || w[i] == 400) EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_NONE);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_segoe_light_weight_ladder)
{
    /* single-entry Win32 families: d = request - 300; simulations switch on at d >= 151 */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (gi_family_count(f, "Segoe UI Light") != 2) { t_note("Segoe UI Light family font count %u", gi_family_count(f, "Segoe UI Light")); }
    if (!gi_sys_has(f, "Segoe UI Light")) { t_skip("Segoe UI Light not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < sizeof gi_weights / sizeof gi_weights[0]; i++) {
        IDWriteFont *font; int w = gi_weights[i];
        if (SUCCEEDED(gi_create(g, "Segoe UI Light", w, 0, DEFAULT_CHARSET, &font)) && font) {
            DWRITE_FONT_SIMULATIONS s = IDWriteFont_GetSimulations(font);
            int want = (w - 300 >= GI_BOLD_SIM_DELTA) ? DWRITE_FONT_SIMULATIONS_BOLD : 0;
            if (gi_family_count(f, "Segoe UI Light") == 2) CHECKF((int)s == want, "request %d: simulations %d want %d", w, (int)s, want);
            DW_RELEASE(font);
        } else CHECK(0);
    }
    DW_RELEASE(g);
}
TEST(gi_arial_black_single_font)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial Black")) { t_skip("Arial Black not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < sizeof gi_weights / sizeof gi_weights[0]; i++) {
        IDWriteFont *font; LOGFONTW lf; BOOL sys; char b[64];
        CHECK_HR_OK(gi_create(g, "Arial Black", gi_weights[i], 0, DEFAULT_CHARSET, &font));
        if (!font) continue;
        CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
        CHECK(gi_font_file(font, b, sizeof b) && !strcmp(b, "ariblk.ttf"));
        CHECK(lf.lfWeight >= 900);   /* 900 (+151 when the request would add a synthetic bold; never possible above weight 900) */
        EXPECT_EQ(lf.lfWeight, 900);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_italic_byte_is_boolean)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); static const int v[] = { 1, 2, 7, 0x80, 0xff };
    size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (gi_family_count(f, "Arial") != 4) { t_skip("Arial is not the four-font family"); DW_RELEASE(g); return; }
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteFont *font; LOGFONTW lf; BOOL sys;
        CHECK_HR_OK(gi_create(g, "Arial", 400, v[i], DEFAULT_CHARSET, &font));
        if (!font) continue;
        CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
        EXPECT_EQ(lf.lfItalic, 1);   /* any non-zero lfItalic is treated as "italic wanted" */
        EXPECT_EQ(IDWriteFont_GetStyle(font), DWRITE_FONT_STYLE_ITALIC);
        EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_NONE);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_arial_style_files)
{
    static const struct { int w, it; const char *file; } rows[] = { { 400, 0, "arial.ttf" }, { 700, 0, "arialbd.ttf" }, { 400, 1, "ariali.ttf" }, { 700, 1, "arialbi.ttf" } };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (gi_family_count(f, "Arial") != 4) { t_skip("Arial is not the four-font family"); DW_RELEASE(g); return; }
    for (i = 0; i < 4; i++) {
        IDWriteFont *font; char b[64];
        CHECK_HR_OK(gi_create(g, "Arial", rows[i].w, rows[i].it, DEFAULT_CHARSET, &font));
        if (!font) continue;
        if (gi_font_file(font, b, sizeof b)) CHECKF(!strcmp(b, rows[i].file), "w=%d it=%d -> %s want %s", rows[i].w, rows[i].it, b, rows[i].file);
        EXPECT_EQ(IDWriteFont_GetWeight(font), rows[i].w);
        EXPECT_EQ(IDWriteFont_GetStyle(font), rows[i].it ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_lf_weight_zero_picks_lightest)
{
    /* lfWeight 0 (FW_DONTCARE) is just a very light request: the penalty grows with the entry weight, so the lightest face wins */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font; LOGFONTW lf; BOOL sys;
    if (!g) { t_skip("no interop"); return; }
    if (gi_family_count(f, "Arial") != 4) { t_skip("Arial is not the four-font family"); DW_RELEASE(g); return; }
    CHECK_HR_OK(gi_create(g, "Arial", 0, 0, DEFAULT_CHARSET, &font));
    if (font) { CHECK_HR_OK(gi_conv(g, font, &lf, &sys)); EXPECT_EQ(lf.lfWeight, 400); DW_RELEASE(font); }
    DW_RELEASE(g);
}
TEST(gi_lf_bold_sim_boundary_regular_only_family)
{
    /* Segoe UI Light (weight 300): 450 is d=150 -> no simulation, 451 is d=151 -> synthetic bold */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *a, *b;
    if (!g) { t_skip("no interop"); return; }
    if (gi_family_count(f, "Segoe UI Light") != 2) { t_skip("Segoe UI Light is not the expected two-font family"); DW_RELEASE(g); return; }
    CHECK_HR_OK(gi_create(g, "Segoe UI Light", 450, 0, DEFAULT_CHARSET, &a));
    CHECK_HR_OK(gi_create(g, "Segoe UI Light", 451, 0, DEFAULT_CHARSET, &b));
    if (a) EXPECT_EQ(IDWriteFont_GetSimulations(a), DWRITE_FONT_SIMULATIONS_NONE);
    if (b) EXPECT_EQ(IDWriteFont_GetSimulations(b), DWRITE_FONT_SIMULATIONS_BOLD);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(g);
}

/* ---- fields that must be ignored --------------------------------------------------------------------------------------------------------- */
TEST(gi_lf_ignored_fields)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *base = NULL, *other = NULL; LOGFONTW lf, lf2; char b1[64], b2[64];
    BOOL sys; int k;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    gi_lf(&lf, "Arial", 700, 1, DEFAULT_CHARSET);
    CHECK_HR_OK(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &base));
    if (!base) { DW_RELEASE(g); return; }
    for (k = 0; k < 12; k++) {
        lf2 = lf;
        switch (k) {
        case 0: lf2.lfHeight = -200; break;
        case 1: lf2.lfHeight = 5000; break;
        case 2: lf2.lfWidth = 77; break;
        case 3: lf2.lfEscapement = 900; break;
        case 4: lf2.lfOrientation = 450; break;
        case 5: lf2.lfUnderline = 1; break;
        case 6: lf2.lfStrikeOut = 1; break;
        case 7: lf2.lfOutPrecision = OUT_RASTER_PRECIS; break;
        case 8: lf2.lfClipPrecision = CLIP_LH_ANGLES; break;
        case 9: lf2.lfQuality = NONANTIALIASED_QUALITY; break;
        case 10: lf2.lfPitchAndFamily = FIXED_PITCH | FF_MODERN; break;
        case 11: lf2.lfQuality = CLEARTYPE_QUALITY; lf2.lfOutPrecision = OUT_TT_ONLY_PRECIS; lf2.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN; break;
        }
        other = NULL;
        CHECK_HR_OK(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf2, &other));
        if (!other) continue;
        if (gi_font_file(base, b1, sizeof b1) && gi_font_file(other, b2, sizeof b2)) CHECKF(!strcmp(b1, b2), "variant %d changed the font: %s vs %s", k, b1, b2);
        EXPECT_EQ(IDWriteFont_GetSimulations(other), IDWriteFont_GetSimulations(base));
        DW_RELEASE(other);
    }
    (void)sys;
    DW_RELEASE(base); DW_RELEASE(g);
}
TEST(gi_lf_input_not_modified)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font = NULL; LOGFONTW lf, copy;
    if (!g) { t_skip("no interop"); return; }
    gi_lf(&lf, "Arial", 700, 1, 0); lf.lfHeight = -33; lf.lfQuality = 5; copy = lf;
    IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font);
    CHECK(!memcmp(&lf, &copy, sizeof lf));
    DW_RELEASE(font); DW_RELEASE(g);
}
TEST(gi_lf_charset_table)
{
    static const int cs[] = { ANSI_CHARSET, DEFAULT_CHARSET, SYMBOL_CHARSET, MAC_CHARSET, SHIFTJIS_CHARSET, HANGUL_CHARSET, GB2312_CHARSET,
                              CHINESEBIG5_CHARSET, JOHAB_CHARSET, GREEK_CHARSET, TURKISH_CHARSET, VIETNAMESE_CHARSET, HEBREW_CHARSET,
                              ARABIC_CHARSET, BALTIC_CHARSET, RUSSIAN_CHARSET, THAI_CHARSET, EASTEUROPE_CHARSET, OEM_CHARSET, 3, 0x55, 0xfe };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i; IDWriteFont *ref = NULL; char rb[64], b[64];
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    CHECK_HR_OK(gi_create(g, "Arial", 400, 0, DEFAULT_CHARSET, &ref));
    if (!ref || !gi_font_file(ref, rb, sizeof rb)) { DW_RELEASE(ref); DW_RELEASE(g); return; }
    for (i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        IDWriteFont *font; HRESULT hr = gi_create(g, "Arial", 400, 0, cs[i], &font);
        /* the code page mask only adds an equal penalty to every font of the family: a single-style request always resolves */
        CHECKF(hr == S_OK, "charset 0x%x -> %s", cs[i], dw_hr_name(hr));
        if (font) { if (gi_font_file(font, b, sizeof b)) CHECKF(!strcmp(b, rb), "charset 0x%x -> %s want %s", cs[i], b, rb); }
        DW_RELEASE(font);
    }
    DW_RELEASE(ref); DW_RELEASE(g);
}
TEST(gi_lf_symbol_charset_on_text_font)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font; HRESULT hr;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    hr = gi_create(g, "Arial", 400, 0, SYMBOL_CHARSET, &font);
    CHECK_HR_OK(hr);
    if (font) { LOGFONTW lf; BOOL sys; CHECK_HR_OK(gi_conv(g, font, &lf, &sys)); EXPECT_EQ(lf.lfCharSet, DEFAULT_CHARSET); }   /* the result's charset comes from the font, not the request */
    DW_RELEASE(font); DW_RELEASE(g);
}
TEST(gi_lf_out_param_overwritten_on_success)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    CHECK_HR_OK(gi_create(g, "Arial", 400, 0, 1, &font));
    CHECK(font != NULL && font != (IDWriteFont *)(ULONG_PTR)0x1234);
    if (font) EXPECT_REFS(font, 1);
    DW_RELEASE(font); DW_RELEASE(g);
}
TEST(gi_lf_each_call_new_object)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *a, *b;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    CHECK_HR_OK(gi_create(g, "Arial", 400, 0, 1, &a));
    CHECK_HR_OK(gi_create(g, "Arial", 400, 0, 1, &b));
    CHECK(a && b);
    if (a && b) { EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(g);
}
TEST(gi_lf_same_via_interop1_null_collection)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteGdiInterop1 *g1; IDWriteFont *a = NULL, *b = NULL; LOGFONTW lf;
    char fa[64], fb[64];
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1 || !gi_sys_has(f, "Arial")) { t_skip("interop1 or Arial missing"); DW_RELEASE(g1); DW_RELEASE(g); return; }
    gi_lf(&lf, "Arial", 700, 0, 1);
    CHECK_HR_OK(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &a));
    CHECK_HR_OK(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, &lf, NULL, &b));
    if (a && b && gi_font_file(a, fa, sizeof fa) && gi_font_file(b, fb, sizeof fb)) CHECK(!strcmp(fa, fb));
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(g1); DW_RELEASE(g);
}
TEST(gi_lf_repeated_mixed)
{
    /* mixed hits and misses in a loop: no state may leak from a failed lookup into the next call */
    IDWriteFactory *f = dw_shared();
    IDWriteGdiInterop *g = gi_get(f); int i; IDWriteFont *font;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < 200; i++) {
        HRESULT hr = gi_create(g, i & 1 ? "Arial" : "NoSuchFontXyz", 400 + (i % 5) * 100, i & 2, DEFAULT_CHARSET, &font);
        CHECKF(hr == S_OK || hr == DWRITE_E_NOFONT, "%s", dw_hr_name(hr));
        if (hr != S_OK) CHECK_OUT_NULL(font);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}

/* ---- ConvertFontToLOGFONT -------------------------------------------------------------------------------------------------------------------- */
TEST(gi_conv_null_font)
{
    IDWriteGdiInterop *g = gi_get(dw_shared()); LOGFONTW lf; BOOL sys;
    if (!g) { t_skip("no interop"); return; }
    CHECK_HR(gi_conv(g, NULL, &lf, &sys), DW_E_INVALIDARG);
    CHECK(gi_lf_all_zero(&lf));       /* the structure is zeroed before the argument check */
    EXPECT_EQ(sys, FALSE);            /* so is *isSystemFont */
    DW_RELEASE(g);
}
TEST(gi_conv_poison_overwritten)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFont *font; LOGFONTW lf; BOOL sys;
    if (!g) { t_skip("no interop"); return; }
    if (!gi_sys_has(f, "Arial")) { t_skip("Arial not installed"); DW_RELEASE(g); return; }
    CHECK_HR_OK(gi_create(g, "Arial", 400, 0, 1, &font));
    if (font) {
        const BYTE *p; size_t i, bad = 0;
        CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
        p = (const BYTE *)&lf;
        for (i = 0; i < sizeof lf; i++) if (p[i] == 0xCC) bad++;
        CHECKF(bad == 0, "%zu bytes of the LOGFONT kept the poison pattern", bad);
        GI_LF_SHAPE(lf);
        EXPECT_EQ(sys, 1);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_conv_roundtrip_identity)
{
    static const char *faces[] = { "Arial", "Times New Roman", "Courier New", "Tahoma", "Segoe UI", "Verdana", "Consolas", "Georgia" };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i; int w, it;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < sizeof faces / sizeof faces[0]; i++) {
        if (!gi_sys_has(f, faces[i])) { t_note("%s not installed", faces[i]); continue; }
        for (it = 0; it < 2; it++)
            for (w = 400; w <= 700; w += 300) {
                IDWriteFont *a, *b; LOGFONTW lf; BOOL sys; char fa[64], fb[64];
                CHECK_HR_OK(gi_create(g, faces[i], w, it, 1, &a));
                if (!a) continue;
                CHECK_HR_OK(gi_conv(g, a, &lf, &sys));
                /* converting then re-resolving the produced LOGFONT must land on the same file with the same simulations */
                CHECK_HR_OK(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &b));
                if (b) {
                    if (gi_font_file(a, fa, sizeof fa) && gi_font_file(b, fb, sizeof fb)) CHECKF(!strcmp(fa, fb), "%s w=%d it=%d: %s vs %s", faces[i], w, it, fa, fb);
                    EXPECT_EQ(IDWriteFont_GetSimulations(b), IDWriteFont_GetSimulations(a));
                }
                DW_RELEASE(a); DW_RELEASE(b);
            }
    }
    DW_RELEASE(g);
}
TEST(gi_conv_facename_matches_win32_family_names)
{
    static const char *faces[] = { "Arial", "Times New Roman", "Courier New", "Tahoma", "Segoe UI", "Arial Black", "Segoe UI Semibold", "Segoe UI Light" };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < sizeof faces / sizeof faces[0]; i++) {
        IDWriteFont *font; LOGFONTW lf; BOOL sys; char fam[128], n[64];
        if (!gi_sys_has(f, faces[i])) continue;
        CHECK_HR_OK(gi_create(g, faces[i], 400, 0, 1, &font));
        if (!font) continue;
        CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
        gi_name_u8(&lf, n, sizeof n);
        if (gi_win32_family(font, fam, sizeof fam)) CHECKF(!strcmp(fam, n), "lfFaceName \"%s\" vs WIN32_FAMILY_NAMES \"%s\"", n, fam);
        DW_RELEASE(font);
    }
    DW_RELEASE(g);
}
TEST(gi_conv_fonts_from_family_enumeration)
{
    /* every font of a DirectWrite family converts; the LOGFONT carries the Win32 family/weight view of that font */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL;
    UINT32 idx = 0, n, i; BOOL ex = FALSE;
    if (!g) { t_skip("no interop"); return; }
    IDWriteFactory_GetSystemFontCollection(f, &c, FALSE);
    if (!c) { DW_RELEASE(g); return; }
    IDWriteFontCollection_FindFamilyName(c, L"Segoe UI", &idx, &ex);
    if (!ex) { t_skip("Segoe UI not installed"); DW_RELEASE(c); DW_RELEASE(g); return; }
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, idx, &fam));
    n = fam ? IDWriteFontFamily_GetFontCount(fam) : 0;
    for (i = 0; i < n; i++) {
        IDWriteFont *font = NULL; LOGFONTW lf; BOOL sys;
        CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, i, &font));
        if (!font) continue;
        CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
        EXPECT_EQ(sys, TRUE);
        GI_LF_SHAPE(lf);
        EXPECT_EQ(lf.lfItalic, IDWriteFont_GetStyle(font) != DWRITE_FONT_STYLE_NORMAL);
        t_note("Segoe UI font %u: dw weight %d style %d -> lfFaceName \"%ls\" lfWeight %ld lfItalic %d", i, (int)IDWriteFont_GetWeight(font),
               (int)IDWriteFont_GetStyle(font), lf.lfFaceName, lf.lfWeight, lf.lfItalic);
        DW_RELEASE(font);
    }
    DW_RELEASE(fam); DW_RELEASE(c); DW_RELEASE(g);
}
TEST(gi_conv_segoe_named_weights)
{
    static const struct { int w; const char *name; } rows[] = { { 300, "Segoe UI Light" }, { 600, "Segoe UI Semibold" }, { 900, "Segoe UI Black" }, { 400, "Segoe UI" }, { 700, "Segoe UI" } };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    if (!dw_have_font(f, L"Segoe UI")) { t_skip("Segoe UI not installed"); DW_RELEASE(g); return; }
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; UINT32 idx; BOOL ex;
        IDWriteFactory_GetSystemFontCollection(f, &c, FALSE);
        IDWriteFontCollection_FindFamilyName(c, L"Segoe UI", &idx, &ex);
        if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) &&
            SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, (DWRITE_FONT_WEIGHT)rows[i].w, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font)) && font) {
            LOGFONTW lf; BOOL sys; char n[64];
            if (IDWriteFont_GetWeight(font) != (DWRITE_FONT_WEIGHT)rows[i].w) { t_note("no exact Segoe UI weight %d", rows[i].w); }
            else {
                CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
                gi_name_u8(&lf, n, sizeof n);
                CHECKF(!strcmp(n, rows[i].name), "weight %d: lfFaceName \"%s\" want \"%s\"", rows[i].w, n, rows[i].name);
                EXPECT_EQ(lf.lfWeight, rows[i].w);
            }
        }
        DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c);
    }
    DW_RELEASE(g);
}
TEST(gi_conv_matches_gdi_textface)
{
    static const char *faces[] = { "Arial", "Times New Roman", "Courier New", "Tahoma", "Segoe UI", "Verdana" };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); size_t i;
    if (!g) { t_skip("no interop"); return; }
    for (i = 0; i < sizeof faces / sizeof faces[0]; i++) {
        IDWriteFont *font; LOGFONTW lf; BOOL sys; char gdi[64], dw[64];
        if (!gi_sys_has(f, faces[i])) continue;
        CHECK_HR_OK(gi_create(g, faces[i], 400, 0, 1, &font));
        if (!font) continue;
        CHECK_HR_OK(gi_conv(g, font, &lf, &sys));
        gi_name_u8(&lf, dw, sizeof dw);
        lf.lfHeight = -20;
        {
            HFONT hf = CreateFontIndirectW(&lf); HDC dc = CreateCompatibleDC(NULL); HFONT old = (HFONT)SelectObject(dc, hf); WCHAR w[LF_FACESIZE + 4];
            GetTextFaceW(dc, LF_FACESIZE + 4, w); dw_narrow(w, gdi, sizeof gdi);
            CHECKF(!_stricmp(gdi, dw), "GDI face for the converted LOGFONT \"%s\" is \"%s\"", dw, gdi);
            SelectObject(dc, old); DeleteObject(hf); DeleteDC(dc);
        }
        DW_RELEASE(font);
    }
    (void)0;
    DW_RELEASE(g);
}
