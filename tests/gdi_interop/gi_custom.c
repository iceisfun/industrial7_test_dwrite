/* IDWriteGdiInterop with deterministic synthetic fonts served from a custom font collection (IDWriteGdiInterop1 overloads).
 *
 * The synthetic family names, weights and styles are known exactly, so the penalty function behind the LOGFONT lookup can be checked
 * numerically (gi_score() in gi.h): lookup by Win32 family name (name ID 1), then by full name (ID 4); lowest penalty wins;
 * lfWeight - fontWeight >= 151 asks for synthetic bold, an italic request on an upright face asks for synthetic oblique, and
 * ConvertFontToLOGFONT reports weight + 151 for a bold-simulated face.  Fonts that come from a custom collection are not
 * "system" fonts (the collection loader id is non-zero), but conversion still succeeds. */
#include "gi.h"

static const gi_cf gi_set_ribbi[] = {
    { "GiRibbi", 400, 0, 5 }, { "GiRibbi", 700, 0, 5 }, { "GiRibbi", 400, 1, 5 }, { "GiRibbi", 700, 1, 5 },
};
static const gi_ent gi_ents_ribbi[] = {
    { 400, DWRITE_FONT_STYLE_NORMAL, 1, 5 }, { 700, DWRITE_FONT_STYLE_NORMAL, 1, 5 },
    { 400, DWRITE_FONT_STYLE_ITALIC, 1, 5 }, { 700, DWRITE_FONT_STYLE_ITALIC, 1, 5 },
};
static const gi_ent gi_ents_solo[] = { { 400, DWRITE_FONT_STYLE_NORMAL, 1, 5 } };
static const gi_ent gi_ents_light[] = { { 300, DWRITE_FONT_STYLE_NORMAL, 1, 5 } };
static const int gi_wt[] = { 0, 1, 100, 200, 250, 300, 350, 399, 400, 401, 450, 500, 549, 550, 551, 552, 600, 650, 699, 700, 701, 750, 800, 850, 900, 950, 1000, 1001, 5000, -1, -400 };

static HRESULT gi_cf_font(gi_fix *x, const char *face, int w, int it, int cs, IDWriteFont **out)
{
    *out = (IDWriteFont *)(ULONG_PTR)0x1234;
    return gi_font_lf(x, face, w, it, cs, out);
}
static HRESULT gi_cf_conv(gi_fix *x, IDWriteFont *font, LOGFONTW *lf, BOOL *sys)
{
    GI_POISON_LF(*lf); *sys = (BOOL)0x5a5a;
    return IDWriteGdiInterop_ConvertFontToLOGFONT(x->gdi, font, lf, sys);
}
static void gi_cf_model(gi_fix *x, const char *face, const gi_ent *ents, int n, unsigned csmask, int cs)
{
    size_t wi; int it, checked = 0, ties = 0;
    for (it = 0; it < 2; it++)
        for (wi = 0; wi < sizeof gi_wt / sizeof gi_wt[0]; wi++) {
            int w = gi_wt[wi], sim = 0, best = gi_best(ents, n, csmask, w, it, &sim, NULL); IDWriteFont *font; LOGFONTW lf; BOOL sys; HRESULT hr;
            if (best < 0) { ties++; continue; }
            hr = gi_cf_font(x, face, w, it, cs, &font);
            CHECKF(hr == S_OK, "%s w=%d it=%d -> %s", face, w, it, dw_hr_name(hr));
            if (hr != S_OK || !font) continue;
            CHECK_HR_OK(gi_cf_conv(x, font, &lf, &sys));
            CHECKF(lf.lfWeight == ents[best].weight + ((sim & DWRITE_FONT_SIMULATIONS_BOLD) ? GI_BOLD_SIM_DELTA : 0),
                   "%s req w=%d it=%d: lfWeight %ld want entry %d (%d) sim %d", face, w, it, lf.lfWeight, best, ents[best].weight, sim);
            CHECKF(lf.lfItalic == ((ents[best].style || (sim & DWRITE_FONT_SIMULATIONS_OBLIQUE)) ? 1 : 0), "%s req w=%d it=%d: lfItalic %d", face, w, it, lf.lfItalic);
            CHECKF((int)IDWriteFont_GetSimulations(font) == sim, "%s req w=%d it=%d: simulations %d want %d", face, w, it, (int)IDWriteFont_GetSimulations(font), sim);
            EXPECT_EQ(sys, FALSE);
            checked++;
            DW_RELEASE(font);
        }
    CHECK(checked > 0);
    t_note("%s: %d requests checked, %d ties skipped", face, checked, ties);
}

TEST(gic_exact_ribbi)
{
    static const struct { int w, it; } rows[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 } };
    gi_fix x; size_t i;
    GI_FIX_OR_SKIP(x, gi_set_ribbi);
    for (i = 0; i < 4; i++) {
        IDWriteFont *font; LOGFONTW lf; BOOL sys; char n[64];
        CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", rows[i].w, rows[i].it, DEFAULT_CHARSET, &font));
        if (!font) continue;
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        gi_name_u8(&lf, n, sizeof n);
        EXPECT_STR(n, "GiRibbi");
        EXPECT_EQ(lf.lfWeight, rows[i].w);
        EXPECT_EQ(lf.lfItalic, rows[i].it);
        EXPECT_EQ(lf.lfCharSet, DEFAULT_CHARSET);
        EXPECT_EQ(sys, FALSE);
        GI_LF_SHAPE(lf);
        EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_NONE);
        EXPECT_EQ(IDWriteFont_GetWeight(font), rows[i].w);
        EXPECT_REFS(font, 1);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_model_ribbi_default_charset) { gi_fix x; GI_FIX_OR_SKIP(x, gi_set_ribbi); gi_cf_model(&x, "GiRibbi", gi_ents_ribbi, 4, 0xffffffffu, DEFAULT_CHARSET); gi_fix_close(&x); }
TEST(gic_model_ribbi_ansi_charset) { gi_fix x; GI_FIX_OR_SKIP(x, gi_set_ribbi); gi_cf_model(&x, "GiRibbi", gi_ents_ribbi, 4, 1, ANSI_CHARSET); gi_fix_close(&x); }
TEST(gic_model_ribbi_other_charset)
{
    /* SHIFTJIS: no font claims the Japanese code page, every candidate carries the same +65000 penalty, ordering is unchanged */
    gi_fix x; GI_FIX_OR_SKIP(x, gi_set_ribbi); gi_cf_model(&x, "GiRibbi", gi_ents_ribbi, 4, 0x20000, SHIFTJIS_CHARSET); gi_fix_close(&x);
}
static const gi_cf gi_set_solo[] = { { "GiSolo", 400, 0, 5 } };
TEST(gic_model_solo) { gi_fix x; GI_FIX_OR_SKIP(x, gi_set_solo); gi_cf_model(&x, "GiSolo", gi_ents_solo, 1, 0xffffffffu, DEFAULT_CHARSET); gi_fix_close(&x); }
static const gi_cf gi_set_light[] = { { "GiLight", 300, 0, 5 } };
TEST(gic_model_light)
{
    /* GiLight's only font has legacy name "GiLight Light" (weight 300 is not a RIBBI weight); query with that Win32 family name */
    gi_fix x; GI_FIX_OR_SKIP(x, gi_set_light); gi_cf_model(&x, "GiLight Light", gi_ents_light, 1, 0xffffffffu, DEFAULT_CHARSET); gi_fix_close(&x);
}
TEST(gic_solo_bold_boundary)
{
    /* d = request - 400: 550 -> d=150, no simulation; 551 -> d=151, synthetic bold; the converted weight becomes 400 + 151 */
    static const struct { int w; int sim; int lfw; } rows[] = { { 400, 0, 400 }, { 549, 0, 400 }, { 550, 0, 400 }, { 551, 1, 551 }, { 700, 1, 551 }, { 900, 1, 551 }, { 1000, 1, 551 } };
    gi_fix x; size_t i; GI_FIX_OR_SKIP(x, gi_set_solo);
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteFont *font; LOGFONTW lf; BOOL sys;
        CHECK_HR_OK(gi_cf_font(&x, "GiSolo", rows[i].w, 0, 1, &font));
        if (!font) continue;
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        CHECKF((int)IDWriteFont_GetSimulations(font) == rows[i].sim, "w=%d simulations %d", rows[i].w, (int)IDWriteFont_GetSimulations(font));
        CHECKF(lf.lfWeight == rows[i].lfw, "w=%d lfWeight %ld want %d", rows[i].w, lf.lfWeight, rows[i].lfw);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_solo_lighter_requests_never_simulate)
{
    static const int w[] = { 0, 1, 100, 399, 399, 200 };
    gi_fix x; size_t i; GI_FIX_OR_SKIP(x, gi_set_solo);
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        IDWriteFont *font;
        CHECK_HR_OK(gi_cf_font(&x, "GiSolo", w[i], 0, 1, &font));
        if (font) { EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_NONE); DW_RELEASE(font); }
    }
    gi_fix_close(&x);
}
TEST(gic_solo_italic_is_oblique_simulation)
{
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, gi_set_solo);
    CHECK_HR_OK(gi_cf_font(&x, "GiSolo", 400, 1, 1, &font));
    if (font) {
        EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_OBLIQUE);
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        EXPECT_EQ(lf.lfItalic, 1);
        EXPECT_EQ(lf.lfWeight, 400);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_solo_bold_italic_both_simulations)
{
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, gi_set_solo);
    CHECK_HR_OK(gi_cf_font(&x, "GiSolo", 700, 1, 1, &font));
    if (font) {
        EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE);
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        EXPECT_EQ(lf.lfItalic, 1);
        EXPECT_EQ(lf.lfWeight, 400 + GI_BOLD_SIM_DELTA);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_ribbi_bold_italic_request_exact)
{
    gi_fix x; IDWriteFont *font; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 700, 1, 1, &font));
    if (font) { EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_NONE); EXPECT_EQ(IDWriteFont_GetStyle(font), DWRITE_FONT_STYLE_ITALIC); EXPECT_EQ(IDWriteFont_GetWeight(font), 700); DW_RELEASE(font); }
    gi_fix_close(&x);
}
TEST(gic_ribbi_semibold_request_resolves_to_regular_with_simulation)
{
    /* lfWeight 600: regular (400) is d=200 -> bold simulation, penalty (200-120)*73/256 = 22; bold (700) is d=-100 -> penalty 28; regular+bold-sim wins */
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 600, 0, 1, &font));
    if (font) {
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_BOLD);
        EXPECT_EQ(lf.lfWeight, 551);
        DW_RELEASE(font);
    }
    CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 650, 0, 1, &font));
    if (font) { EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_NONE); EXPECT_EQ(IDWriteFont_GetWeight(font), 700); DW_RELEASE(font); }
    gi_fix_close(&x);
}
TEST(gic_ribbi_weight_zero_is_regular)
{
    gi_fix x; IDWriteFont *font; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 0, 0, 1, &font));
    if (font) { EXPECT_EQ(IDWriteFont_GetWeight(font), 400); DW_RELEASE(font); }
    gi_fix_close(&x);
}

/* ---- scoping ---- */
TEST(gic_custom_collection_hides_system_fonts)
{
    gi_fix x; IDWriteFont *font; GI_FIX_OR_SKIP(x, gi_set_solo);
    CHECK_HR(gi_cf_font(&x, "Arial", 400, 0, 1, &font), DWRITE_E_NOFONT);
    CHECK_OUT_NULL(font);
    CHECK_HR(gi_cf_font(&x, "Segoe UI", 400, 0, 1, &font), DWRITE_E_NOFONT);
    CHECK_OUT_NULL(font);
    gi_fix_close(&x);
}
TEST(gic_system_collection_hides_custom_fonts)
{
    gi_fix x; IDWriteFont *font = NULL; LOGFONTW lf; GI_FIX_OR_SKIP(x, gi_set_solo);
    gi_lf(&lf, "GiSolo", 400, 0, 1);
    font = (IDWriteFont *)(ULONG_PTR)0x1234;
    CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(x.gdi, &lf, &font), DWRITE_E_NOFONT);   /* NULL collection means the system collection */
    CHECK_OUT_NULL(font);
    gi_fix_close(&x);
}
TEST(gic_explicit_system_collection_same_as_null)
{
    gi_fix x; IDWriteFontCollection *sys = NULL; IDWriteFont *a = NULL, *b = NULL; LOGFONTW lf; GI_FIX_OR_SKIP(x, gi_set_solo);
    IDWriteFactory_GetSystemFontCollection(x.f, &sys, FALSE);
    gi_lf(&lf, "NoSuchFaceAnywhere", 400, 0, 1);
    if (x.gdi1 && sys) {
        CHECK_HR(IDWriteGdiInterop1_CreateFontFromLOGFONT(x.gdi1, &lf, sys, &a), DWRITE_E_NOFONT);
        CHECK_HR(IDWriteGdiInterop1_CreateFontFromLOGFONT(x.gdi1, &lf, NULL, &b), DWRITE_E_NOFONT);
        CHECK_OUT_NULL(a); CHECK_OUT_NULL(b);
    }
    DW_RELEASE(sys);
    gi_fix_close(&x);
}
TEST(gic_two_collections_independent)
{
    static const gi_cf s1[] = { { "GiOne", 400, 0, 5 } };
    gi_fix x; IDWriteFont *font; GI_FIX_OR_SKIP(x, s1);
    CHECK_HR_OK(gi_cf_font(&x, "GiOne", 400, 0, 1, &font));
    DW_RELEASE(font);
    CHECK_HR(gi_cf_font(&x, "GiRibbi", 400, 0, 1, &font), DWRITE_E_NOFONT);
    CHECK_OUT_NULL(font);
    gi_fix_close(&x);
}
TEST(gic_font_outlives_collection)
{
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 700, 0, 1, &font));
    DW_RELEASE(x.coll);
    if (font) {
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        EXPECT_EQ(lf.lfWeight, 700);
        EXPECT_EQ(sys, FALSE);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_loader_leak_balance)
{
    DWX_LEAK_BEGIN(l);
    {
        gi_fix x; IDWriteFont *font; GI_FIX_OR_SKIP(x, gi_set_ribbi);
        CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 700, 0, 1, &font));
        DW_RELEASE(font);
        gi_fix_close(&x);
    }
    DWX_LEAK_END(l);
}

/* ---- name lookup ---- */
TEST(gic_full_name_fallback)
{
    /* synthetic full names: "GiRibbi" (regular), "GiRibbi Bold", "GiRibbi Italic", "GiRibbi Bold Italic" */
    static const struct { const char *name; int w, it; } rows[] = { { "GiRibbi Bold", 700, 0 }, { "GiRibbi Italic", 400, 1 }, { "GiRibbi Bold Italic", 700, 1 } };
    gi_fix x; size_t i; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    for (i = 0; i < 3; i++) {
        IDWriteFont *font; LOGFONTW lf; BOOL sys;
        CHECK_HR_OK(gi_cf_font(&x, rows[i].name, 400, 0, 1, &font));   /* only one font carries that full name */
        if (!font) continue;
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        CHECKF(lf.lfWeight == rows[i].w + (IDWriteFont_GetSimulations(font) & 1 ? GI_BOLD_SIM_DELTA : 0), "%s: lfWeight %ld", rows[i].name, lf.lfWeight);
        EXPECT_WSTR(lf.lfFaceName, L"GiRibbi");
        EXPECT_EQ(lf.lfItalic, rows[i].it || (IDWriteFont_GetSimulations(font) & 2) ? 1 : 0);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_win32_family_wins_over_full_name)
{
    /* "GiRibbi" is both the Win32 family of all four fonts and the full name of the regular one: the family match is used, so the
     * request weight can still select another member */
    gi_fix x; IDWriteFont *font; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 700, 0, 1, &font));
    if (font) { EXPECT_EQ(IDWriteFont_GetWeight(font), 700); DW_RELEASE(font); }
    gi_fix_close(&x);
}
TEST(gic_name_case_variants)
{
    static const char *v[] = { "giribbi", "GIRIBBI", "gIrIbBi" };
    gi_fix x; size_t i; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    for (i = 0; i < 3; i++) {
        IDWriteFont *font; HRESULT hr = gi_cf_font(&x, v[i], 400, 0, 1, &font);
        t_note("\"%s\" -> %s", v[i], dw_hr_name(hr));
        if (hr != S_OK) CHECK_OUT_NULL(font);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_semibold_win32_family)
{
    static const gi_cf set[] = { { "GiW", 600, 0, 5 } };   /* legacy family "GiW SemiBold", typographic family "GiW" */
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, set);
    CHECK_HR_OK(gi_cf_font(&x, "GiW SemiBold", 600, 0, 1, &font));
    if (font) {
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        EXPECT_WSTR(lf.lfFaceName, L"GiW SemiBold");
        EXPECT_EQ(lf.lfWeight, 600);
        EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_NONE);
        DW_RELEASE(font);
    }
    CHECK_HR(gi_cf_font(&x, "GiW", 600, 0, 1, &font), DWRITE_E_NOFONT);   /* the typographic family name is not a Win32 family name */
    CHECK_OUT_NULL(font);
    gi_fix_close(&x);
}
TEST(gic_semibold_request_weights)
{
    static const gi_cf set[] = { { "GiW", 600, 0, 5 } };
    static const struct { int w; int sim; } rows[] = { { 0, 0 }, { 300, 0 }, { 600, 0 }, { 750, 0 }, { 751, 1 }, { 900, 1 } };
    gi_fix x; size_t i; GI_FIX_OR_SKIP(x, set);
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteFont *font;
        CHECK_HR_OK(gi_cf_font(&x, "GiW SemiBold", rows[i].w, 0, 1, &font));
        if (font) { CHECKF((int)IDWriteFont_GetSimulations(font) == rows[i].sim, "w=%d sim %d", rows[i].w, (int)IDWriteFont_GetSimulations(font)); DW_RELEASE(font); }
    }
    gi_fix_close(&x);
}
TEST(gic_light_weight_name)
{
    static const gi_cf set[] = { { "GiL", 300, 0, 5 } };
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, set);
    CHECK_HR_OK(gi_cf_font(&x, "GiL Light", 300, 0, 1, &font));
    if (font) { CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys)); EXPECT_WSTR(lf.lfFaceName, L"GiL Light"); EXPECT_EQ(lf.lfWeight, 300); DW_RELEASE(font); }
    gi_fix_close(&x);
}
TEST(gic_black_weight_name)
{
    static const gi_cf set[] = { { "GiB", 900, 0, 5 } };
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, set);
    CHECK_HR_OK(gi_cf_font(&x, "GiB Black", 400, 0, 1, &font));
    if (font) { CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys)); EXPECT_WSTR(lf.lfFaceName, L"GiB Black"); EXPECT_EQ(lf.lfWeight, 900); DW_RELEASE(font); }
    gi_fix_close(&x);
}
TEST(gic_weight_ladder_each_is_own_win32_family)
{
    /* weights other than 400/700 carry the style word in the legacy family name, so each rung is a one-font Win32 family */
    static const gi_cf set[] = { { "GiLad", 100, 0, 5 }, { "GiLad", 200, 0, 5 }, { "GiLad", 300, 0, 5 }, { "GiLad", 400, 0, 5 }, { "GiLad", 500, 0, 5 },
                                 { "GiLad", 600, 0, 5 }, { "GiLad", 700, 0, 5 }, { "GiLad", 800, 0, 5 }, { "GiLad", 900, 0, 5 } };
    gi_fix x; int w; GI_FIX_OR_SKIP(x, set);
    for (w = 100; w <= 900; w += 100) {
        IDWriteFont *font; LOGFONTW lf; BOOL sys; char name[64], want[64];
        snprintf(want, sizeof want, "GiLad%s%s", (w == 400 || w == 700) ? "" : " ", (w == 400 || w == 700) ? "" : ttf_weight_word(w));
        CHECK_HR_OK(gi_cf_font(&x, want, 400, 0, 1, &font));
        if (!font) continue;
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        gi_name_u8(&lf, name, sizeof name);
        CHECKF(!strcmp(name, want), "rung %d: \"%s\" want \"%s\"", w, name, want);
        if (w != 400 && w != 700) EXPECT_EQ(lf.lfWeight, w);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}

/* ---- code page selection ---- */
static const gi_cf gi_set_cp[] = {
    { "GiCp", 400, 0, 5, 0x00000001 },     /* Latin 1 */
    { "GiCp", 400, 0, 5, 0x00020000 },     /* Japanese */
    { "GiCp", 400, 0, 5, 0x00040000 },     /* simplified Chinese */
};
static UINT32 gi_fam_count(IDWriteFontCollection *c, const WCHAR *name)
{
    UINT32 idx = 0, n = 0; BOOL ex = FALSE; IDWriteFontFamily *fam = NULL;
    IDWriteFontCollection_FindFamilyName(c, name, &idx, &ex);
    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam) { n = IDWriteFontFamily_GetFontCount(fam); IDWriteFontFamily_Release(fam); }
    return n;
}
static int gi_cp_of(IDWriteGdiInterop1 *g1, IDWriteFont *font, unsigned *cp0)
{
    FONTSIGNATURE s;
    if (FAILED(IDWriteGdiInterop1_GetFontSignature(g1, font, &s))) return 0;
    *cp0 = s.fsCsb[0];
    return 1;
}
TEST(gic_charset_selects_code_page_member)
{
    static const struct { int cs; unsigned cp; } rows[] = { { ANSI_CHARSET, 1 }, { SHIFTJIS_CHARSET, 0x20000 }, { GB2312_CHARSET, 0x40000 } };
    gi_fix x; size_t i; GI_FIX_OR_SKIP(x, gi_set_cp);
    if (!x.gdi1) { t_skip("no interop1"); gi_fix_close(&x); return; }
    if (gi_fam_count(x.coll, L"GiCp") != 3) { t_skip("custom collection merged identical-name fonts (%u)", gi_fam_count(x.coll, L"GiCp")); gi_fix_close(&x); return; }
    for (i = 0; i < 3; i++) {
        IDWriteFont *font; unsigned cp = 0;
        CHECK_HR_OK(gi_cf_font(&x, "GiCp", 400, 0, rows[i].cs, &font));
        if (!font) continue;
        if (gi_cp_of(x.gdi1, font, &cp)) CHECKF(cp == rows[i].cp, "charset 0x%x picked code page mask 0x%x want 0x%x", rows[i].cs, cp, rows[i].cp);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_charset_no_match_still_resolves)
{
    gi_fix x; IDWriteFont *font; GI_FIX_OR_SKIP(x, gi_set_cp);
    CHECK_HR_OK(gi_cf_font(&x, "GiCp", 400, 0, HEBREW_CHARSET, &font));   /* nobody has the Hebrew bit: penalty 65000 for everyone, a member is still returned */
    CHECK(font != NULL);
    DW_RELEASE(font);
    gi_fix_close(&x);
}
TEST(gic_unknown_charset_mask_zero)
{
    gi_fix x; IDWriteFont *font; GI_FIX_OR_SKIP(x, gi_set_cp);
    CHECK_HR_OK(gi_cf_font(&x, "GiCp", 400, 0, 0x55, &font));   /* unmapped charset -> mask 0 -> no member matches, all penalised equally */
    CHECK(font != NULL);
    DW_RELEASE(font);
    gi_fix_close(&x);
}
TEST(gic_charset_mask_table_vs_selection)
{
    /* one font per Windows code page bit, charsets 0x80.. choose their own */
    static const struct { int cs; unsigned bit; } rows[] = {
        { ANSI_CHARSET, 0x1 }, { EASTEUROPE_CHARSET, 0x2 }, { RUSSIAN_CHARSET, 0x4 }, { GREEK_CHARSET, 0x8 }, { TURKISH_CHARSET, 0x10 },
        { HEBREW_CHARSET, 0x20 }, { ARABIC_CHARSET, 0x40 }, { BALTIC_CHARSET, 0x80 }, { VIETNAMESE_CHARSET, 0x100 }, { THAI_CHARSET, 0x10000 },
        { SHIFTJIS_CHARSET, 0x20000 }, { GB2312_CHARSET, 0x40000 }, { HANGUL_CHARSET, 0x80000 }, { CHINESEBIG5_CHARSET, 0x100000 },
        { JOHAB_CHARSET, 0x200000 }, { MAC_CHARSET, 0x20000000 }, { OEM_CHARSET, 0x40000000 }, { SYMBOL_CHARSET, 0x80000000u },
    };
    gi_cf set[18]; gi_fix x; size_t i;
    for (i = 0; i < 18; i++) { memset(&set[i], 0, sizeof set[i]); set[i].family = "GiMask"; set[i].weight = 400; set[i].width = 5; set[i].cp0 = rows[i].bit; }
    if (!gi_fix_open(&x, set, 16)) { gi_fix_close(&x); return; }   /* the fixture holds 16 fonts */
    if (!x.gdi1) { t_skip("no interop1"); gi_fix_close(&x); return; }
    if (gi_fam_count(x.coll, L"GiMask") != 16) { t_skip("custom collection merged identical-name fonts (%u)", gi_fam_count(x.coll, L"GiMask")); gi_fix_close(&x); return; }
    for (i = 0; i < 16; i++) {
        IDWriteFont *font; unsigned cp = 0;
        CHECK_HR_OK(gi_cf_font(&x, "GiMask", 400, 0, rows[i].cs, &font));
        if (!font) continue;
        if (gi_cp_of(x.gdi1, font, &cp)) CHECKF(cp == rows[i].bit, "charset 0x%x -> code page mask 0x%x want 0x%x", rows[i].cs, cp, rows[i].bit);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}

/* ---- ConvertFontToLOGFONT / ConvertFontFaceToLOGFONT output shape ---- */
TEST(gic_conv_poison_fully_overwritten)
{
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 400, 0, 1, &font));
    if (font) {
        const BYTE *p = (const BYTE *)&lf; size_t i, bad = 0;
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        for (i = 0; i < sizeof lf; i++) if (p[i] == 0xCC) bad++;
        CHECKF(bad == 0, "%zu LOGFONT bytes kept the poison", bad);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_conv_null_font_zeroes)
{
    gi_fix x; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    CHECK_HR(gi_cf_conv(&x, NULL, &lf, &sys), DW_E_INVALIDARG);
    CHECK(gi_lf_all_zero(&lf));
    EXPECT_EQ(sys, FALSE);
    gi_fix_close(&x);
}
TEST(gic_conv_sentinel_at_end_untouched)
{
    /* the structure written is exactly sizeof(LOGFONTW) = 0x5c bytes */
    struct { LOGFONTW lf; BYTE guard[16]; } s; gi_fix x; IDWriteFont *font; BOOL sys; int i, bad = 0; GI_FIX_OR_SKIP(x, gi_set_ribbi);
    CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", 400, 0, 1, &font));
    if (font) {
        memset(&s, 0xCC, sizeof s); sys = 5;
        CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(x.gdi, font, &s.lf, &sys));
        for (i = 0; i < 16; i++) if (s.guard[i] != 0xCC) bad++;
        CHECKF(bad == 0, "%d bytes past the LOGFONT were modified", bad);
        EXPECT_EQ(sizeof(LOGFONTW), 0x5c);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_conv_long_family_name_truncated)
{
    static const gi_cf set[] = { { "GiTruncFamilyNameThatIsLongerThan31Chars", 400, 0, 5 } };
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, set);
    /* a name this long cannot be requested (lfFaceName holds 31 chars + NUL), so reach the font through the collection */
    CHECK_HR(gi_cf_font(&x, "GiTruncFamilyNameThatIsLonger", 400, 0, 1, &font), DWRITE_E_NOFONT);
    DW_RELEASE(font);
    {
        IDWriteFontFamily *fam = NULL; IDWriteFont *f2 = NULL;
        if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(x.coll, 0, &fam)) && fam && SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &f2)) && f2) {
            CHECK_HR_OK(gi_cf_conv(&x, f2, &lf, &sys));
            EXPECT_EQ(lstrlenW(lf.lfFaceName), 31);               /* clipped to LF_FACESIZE - 1 and terminated */
            EXPECT_EQ(lf.lfFaceName[31], 0);
            CHECK(!wcsncmp(lf.lfFaceName, L"GiTruncFamilyNameThatIsLongerThan31Chars", 31));
            EXPECT_EQ(sys, FALSE);
            GI_LF_SHAPE(lf);
        }
        DW_RELEASE(f2); DW_RELEASE(fam);
    }
    gi_fix_close(&x);
}
TEST(gic_conv_exact_31_char_name_roundtrips)
{
    static const gi_cf set[] = { { "GiThirtyOneCharsFamilyNameXXXXX", 400, 0, 5 } };   /* exactly 31 characters */
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, set);
    EXPECT_EQ(strlen("GiThirtyOneCharsFamilyNameXXXXX"), 31);
    CHECK_HR_OK(gi_cf_font(&x, "GiThirtyOneCharsFamilyNameXXXXX", 400, 0, 1, &font));
    if (font) {
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        EXPECT_WSTR(lf.lfFaceName, L"GiThirtyOneCharsFamilyNameXXXXX");
        EXPECT_EQ(lstrlenW(lf.lfFaceName), 31);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_conv_unicode_family_name)
{
    static const gi_cf set[] = { { "Gi\xC3\xA9\xE6\x97\xA5\xE6\x9C\xAC", 400, 0, 5 } };   /* "Gi" + e-acute + two CJK ideographs */
    gi_fix x; IDWriteFont *font; LOGFONTW lf; BOOL sys; WCHAR w[LF_FACESIZE]; GI_FIX_OR_SKIP(x, set);
    dw_wide(set[0].family, w, LF_FACESIZE);
    gi_lfw(&lf, w, 400, 0, 1);
    font = NULL;
    CHECK_HR_OK(IDWriteGdiInterop1_CreateFontFromLOGFONT(x.gdi1, &lf, x.coll, &font));
    if (font) {
        CHECK_HR_OK(gi_cf_conv(&x, font, &lf, &sys));
        EXPECT_WSTR(lf.lfFaceName, w);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gic_conv_symbol_font_charset)
{
    static const gi_cf set[] = { { "GiSym", 400, 0, 5, 0, 0, { 0 }, 1 }, { "GiTxt", 400, 0, 5 } };
    gi_fix x; IDWriteFont *fs, *ft; LOGFONTW lf; BOOL sys; GI_FIX_OR_SKIP(x, set);
    CHECK_HR_OK(gi_cf_font(&x, "GiSym", 400, 0, 1, &fs));
    CHECK_HR_OK(gi_cf_font(&x, "GiTxt", 400, 0, 1, &ft));
    if (fs) {
        IDWriteFontFace *ff = NULL;
        CHECK_HR_OK(gi_cf_conv(&x, fs, &lf, &sys));
        EXPECT_EQ(lf.lfCharSet, SYMBOL_CHARSET);
        IDWriteFont_CreateFontFace(fs, &ff);
        if (ff) { CHECK(IDWriteFontFace_IsSymbolFont(ff)); DW_RELEASE(ff); }
    }
    if (ft) { CHECK_HR_OK(gi_cf_conv(&x, ft, &lf, &sys)); EXPECT_EQ(lf.lfCharSet, DEFAULT_CHARSET); }
    DW_RELEASE(fs); DW_RELEASE(ft);
    gi_fix_close(&x);
}
TEST(gic_conv_vs_face_conv_equal)
{
    gi_fix x; size_t i; static const struct { int w, it; } rows[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 }, { 600, 0 }, { 900, 1 } };
    GI_FIX_OR_SKIP(x, gi_set_ribbi);
    for (i = 0; i < 6; i++) {
        IDWriteFont *font; IDWriteFontFace *face = NULL; LOGFONTW a, b; BOOL sys;
        CHECK_HR_OK(gi_cf_font(&x, "GiRibbi", rows[i].w, rows[i].it, 1, &font));
        if (!font) continue;
        IDWriteFont_CreateFontFace(font, &face);
        CHECK_HR_OK(gi_cf_conv(&x, font, &a, &sys));
        if (face) {
            GI_POISON_LF(b);
            CHECK_HR_OK(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(x.gdi, face, &b));
            CHECKF(!memcmp(&a, &b, sizeof a), "w=%d it=%d: font and face conversions differ (weights %ld/%ld italic %d/%d)", rows[i].w, rows[i].it, a.lfWeight, b.lfWeight, a.lfItalic, b.lfItalic);
        }
        DW_RELEASE(face); DW_RELEASE(font);
    }
    gi_fix_close(&x);
}

/* ---- ConvertFontFaceToLOGFONT (synthetic faces opened without a collection) ---- */
static void gi_face_conv(const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim, int want_w, int want_it, const char *want_name, int want_cs)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGdiInterop *g; dwx_font h; size_t sz; uint8_t *b; LOGFONTW lf;
    if (!f) { t_skip("no factory"); return; }
    g = gi_get(f);
    b = ttf_build(s, &sz);
    memset(&h, 0, sizeof h);
    if (!g || !b || FAILED(dwx_font_open(&h, f, b, sz, 0, sim)) || !h.face) { t_skip("could not open synthetic face"); }
    else {
        GI_POISON_LF(lf);
        CHECK_HR_OK(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, h.face, &lf));
        EXPECT_EQ(lf.lfWeight, want_w);
        EXPECT_EQ(lf.lfItalic, want_it);
        EXPECT_EQ(lf.lfCharSet, want_cs);
        if (want_name) { char n[64]; gi_name_u8(&lf, n, sizeof n); EXPECT_STR(n, want_name); }
        GI_LF_SHAPE(lf);
    }
    free(b);
    dwx_font_close(&h);
    DW_RELEASE(g); DW_RELEASE(f);
}
TEST(gic_face_regular) { ttf_spec s; ttf_style_spec(&s, "GiFace", 400, 5, 0); gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_NONE, 400, 0, "GiFace", DEFAULT_CHARSET); }
TEST(gic_face_bold) { ttf_spec s; ttf_style_spec(&s, "GiFace", 700, 5, 0); gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_NONE, 700, 0, "GiFace", DEFAULT_CHARSET); }
TEST(gic_face_italic) { ttf_spec s; ttf_style_spec(&s, "GiFace", 400, 5, 1); gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_NONE, 400, 1, "GiFace", DEFAULT_CHARSET); }
TEST(gic_face_bold_italic) { ttf_spec s; ttf_style_spec(&s, "GiFace", 700, 5, 1); gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_NONE, 700, 1, "GiFace", DEFAULT_CHARSET); }
TEST(gic_face_semibold) { ttf_spec s; ttf_style_spec(&s, "GiFace", 600, 5, 0); gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_NONE, 600, 0, "GiFace SemiBold", DEFAULT_CHARSET); }
TEST(gic_face_bold_simulation) { ttf_spec s; ttf_style_spec(&s, "GiFace", 400, 5, 0); gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_BOLD, 400 + GI_BOLD_SIM_DELTA, 0, "GiFace", DEFAULT_CHARSET); }
TEST(gic_face_oblique_simulation) { ttf_spec s; ttf_style_spec(&s, "GiFace", 400, 5, 0); gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_OBLIQUE, 400, 1, "GiFace", DEFAULT_CHARSET); }
TEST(gic_face_both_simulations)
{
    ttf_spec s; ttf_style_spec(&s, "GiFace", 400, 5, 0);
    gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE, 400 + GI_BOLD_SIM_DELTA, 1, "GiFace", DEFAULT_CHARSET);
}
TEST(gic_face_bold_simulation_on_bold_font)
{
    /* the offset is added to whatever the face weight is */
    ttf_spec s; ttf_style_spec(&s, "GiFace", 700, 5, 0); gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_BOLD, 700 + GI_BOLD_SIM_DELTA, 0, "GiFace", DEFAULT_CHARSET);
}
TEST(gic_face_symbol)
{
    ttf_spec s; ttf_cmap c; ttf_style_spec(&s, "GiFace", 400, 5, 0);
    memset(&c, 0, sizeof c); c.format = 4; c.plat = 3; c.enc = 0; c.n = 4; c.map = gi_sym_map; s.cmaps = &c; s.ncmaps = 1;
    gi_face_conv(&s, DWRITE_FONT_SIMULATIONS_NONE, 400, 0, "GiFace", SYMBOL_CHARSET);
}
TEST(gic_face_null)
{
    IDWriteGdiInterop *g = gi_get(dw_isolated()); LOGFONTW lf;
    if (!g) { t_skip("no interop"); return; }
    GI_POISON_LF(lf);
    CHECK_HR(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, NULL, &lf), DW_E_INVALIDARG);
    CHECK(gi_lf_all_zero(&lf));
    DW_RELEASE(g);
}
TEST(gic_face_conv_name_other_than_en_us)
{
    /* only a non-English Win32 family name present: the first dictionary entry is used when neither the user locale nor en-us exist */
    ttf_spec s; size_t sz; uint8_t *b; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGdiInterop *g; dwx_font h; LOGFONTW lf;
    if (!f) { t_skip("no factory"); return; }
    g = gi_get(f);
    ttf_style_spec(&s, "GiLoc", 400, 5, 0);
    ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x40c, 1, "GiLocFr"); ttf_add_name(&s, 3, 1, 0x40c, 2, "Regular"); ttf_add_name(&s, 3, 1, 0x40c, 4, "GiLocFr"); ttf_add_name(&s, 3, 1, 0x40c, 6, "GiLocFr");
    b = ttf_build(&s, &sz); memset(&h, 0, sizeof h);
    if (b && g && SUCCEEDED(dwx_font_open(&h, f, b, sz, 0, DWRITE_FONT_SIMULATIONS_NONE)) && h.face) {
        CHECK_HR_OK(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, h.face, &lf));
        t_note("only fr-FR Win32 family name -> lfFaceName \"%ls\"", lf.lfFaceName);
        CHECKF(lstrlenW(lf.lfFaceName) <= 31, "name length");
    } else t_skip("could not open synthetic face");
    free(b); dwx_font_close(&h); DW_RELEASE(g); DW_RELEASE(f);
}
TEST(gic_face_without_name_table)
{
    ttf_spec s; size_t sz; uint8_t *b; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGdiInterop *g; dwx_font h; LOGFONTW lf; HRESULT hr;
    if (!f) { t_skip("no factory"); return; }
    g = gi_get(f);
    ttf_default_spec(&s); s.omit |= TTF_OMIT_NAME;
    b = ttf_build(&s, &sz); memset(&h, 0, sizeof h);
    if (b && g && SUCCEEDED(dwx_font_open(&h, f, b, sz, 0, DWRITE_FONT_SIMULATIONS_NONE)) && h.face) {
        GI_POISON_LF(lf);
        hr = IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, h.face, &lf);
        CHECKF(hr == S_OK || hr == DWRITE_E_NOFONT, "%s", dw_hr_name(hr));
        t_note("font without name table -> %s lfFaceName \"%ls\"", dw_hr_name(hr), lf.lfFaceName);
    } else t_skip("a font without a name table cannot be opened here");
    free(b); dwx_font_close(&h); DW_RELEASE(g); DW_RELEASE(f);
}
