/* Defaults of every per-range property before any Set*: the single run {0, 0xFFFFFFFF} carries the values of the IDWriteTextFormat the layout
 * was created from, and no Get* validates its position (the run table is searched with the raw position; positions past the text, even
 * 0xFFFFFFFF, report the same default run). Layout-wide IDWriteTextFormat getters report the same defaults and are never changed by range setters. */
#include "tlr.h"

static const UINT32 g_pos[] = { 0, 1, 4, 5, 6, 100, 65536, 0x7FFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFFu };
#define NPOS ((int)(sizeof g_pos / sizeof g_pos[0]))

TEST(def_weight_all_positions)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r;
        CHECK(tlr_weight(l, g_pos[i], &r) == DWRITE_FONT_WEIGHT_NORMAL);
        CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_style_all_positions)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r;
        CHECK(tlr_style(l, g_pos[i], &r) == DWRITE_FONT_STYLE_NORMAL);
        CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_stretch_all_positions)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r;
        CHECK(tlr_stretch(l, g_pos[i], &r) == DWRITE_FONT_STRETCH_NORMAL);
        CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_size_all_positions)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r;
        CHECK_FBITS(tlr_size(l, g_pos[i], &r), 16.0f);
        CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_underline_all_positions)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r;
        CHECK(tlr_underline(l, g_pos[i], &r) == FALSE);
        CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_strikethrough_all_positions)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r;
        CHECK(tlr_strike(l, g_pos[i], &r) == FALSE);
        CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_family_all_positions)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r;
        CHECK_WEQ(tlr_family(l, g_pos[i], &r), TLR_FAM);
        CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_family_length_is_chars_without_nul)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f);
    UINT32 len = 99; DWRITE_TEXT_RANGE r = POISON_RANGE;
    CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &len, &r), S_OK);
    EXPECT_EQ(len, 6u); CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_locale_matches_format)
{
    TLR_BEGIN(); IDWriteTextFormat *t = tlr_format(&X, TLR_FAM, 16.0f); IDWriteTextLayout *l = NULL; int i;
    if (!t) { TLR_END(); t_skip("format"); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"Hello", 5, t, 1000, 1000, &l), S_OK);
    if (l) {
        WCHAR fl[LOCALE_NAME_MAX_LENGTH];
        CHECK_HR(IDWriteTextFormat_GetLocaleName(t, fl, LOCALE_NAME_MAX_LENGTH), S_OK);
        for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
            DWRITE_TEXT_RANGE r;
            CHECK_WEQ(tlr_locale(l, g_pos[i], &r), fl);
            CHECK_RANGE(r, 0, TLR_ALL);
        }
        CHECK_WEQ(fl, TLR_LOC);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(t); TLR_END();
}
TEST(def_drawing_effect_null)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r; IUnknown *u = tlr_effect(l, g_pos[i], &r);
        CHECK(u == NULL); CHECK_RANGE(r, 0, TLR_ALL);
        DW_RELEASE(u);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_inline_object_null)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r; IDWriteInlineObject *u = tlr_inline(l, g_pos[i], &r);
        CHECK(u == NULL); CHECK_RANGE(r, 0, TLR_ALL);
        DW_RELEASE(u);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_typography_null)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        DWRITE_TEXT_RANGE r; IDWriteTypography *u = tlr_typo(l, g_pos[i], &r);
        CHECK(u == NULL); CHECK_RANGE(r, 0, TLR_ALL);
        DW_RELEASE(u);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_collection_equals_format_collection)
{
    TLR_BEGIN(); IDWriteTextFormat *t = tlr_format(&X, TLR_FAM, 16.0f); IDWriteTextLayout *l = NULL; int i;
    if (!t) { TLR_END(); t_skip("format"); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"Hello", 5, t, 1000, 1000, &l), S_OK);
    if (l) {
        IDWriteFontCollection *fc = NULL;
        CHECK_HR(IDWriteTextFormat_GetFontCollection(t, &fc), S_OK);
        CHECK(fc != NULL);
        for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
            DWRITE_TEXT_RANGE r; IDWriteFontCollection *c = tlr_coll(l, g_pos[i], &r);
            CHECK(c == fc); CHECK_RANGE(r, 0, TLR_ALL);
            DW_RELEASE(c);
        }
        DW_RELEASE(fc); IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(t); TLR_END();
}
TEST(def_collection_system_when_format_has_none)
{
    IDWriteFactory *fa = dw_shared(); IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    if (!fa) { t_skip("factory"); return; }
    DW_REQUIRE_OK(IDWriteFactory_CreateTextFormat(fa, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &t));
    DW_REQUIRE_OK(IDWriteFactory_CreateTextLayout(fa, L"abc", 3, t, 100, 100, &l));
    {
        IDWriteFontCollection *fc = NULL, *lc = NULL; DWRITE_TEXT_RANGE r = POISON_RANGE;
        CHECK_HR(IDWriteTextFormat_GetFontCollection(t, &fc), S_OK);
        CHECK_HR(IDWriteTextLayout_GetFontCollection(l, 0, &lc, &r), S_OK);
        CHECK(lc == fc);
        CHECK_RANGE(r, 0, TLR_ALL);
        t_note("format collection %p layout collection %p (NULL collection at CreateTextFormat)", (void *)fc, (void *)lc);
        DW_RELEASE(fc); DW_RELEASE(lc);
    }
    IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t);
}
TEST(def_nondefault_format_values_carry_over)
{
    TLR_BEGIN(); IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(f, TLR_WIDE, X.coll, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 33.5f, L"fr-FR", &t);
    if (FAILED(hr)) { TLR_END(); t_skip("format %s", dw_hr_name(hr)); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 1000, 1000, &l), S_OK);
    if (l) {
        DWRITE_TEXT_RANGE r;
        CHECK(tlr_weight(l, 0, &r) == DWRITE_FONT_WEIGHT_SEMI_BOLD);  CHECK_RANGE(r, 0, TLR_ALL);
        CHECK(tlr_style(l, 1, &r) == DWRITE_FONT_STYLE_ITALIC);       CHECK_RANGE(r, 0, TLR_ALL);
        CHECK(tlr_stretch(l, 2, &r) == DWRITE_FONT_STRETCH_CONDENSED); CHECK_RANGE(r, 0, TLR_ALL);
        CHECK_FBITS(tlr_size(l, 3, &r), 33.5f);                       CHECK_RANGE(r, 0, TLR_ALL);
        CHECK_WEQ(tlr_family(l, 0, &r), TLR_WIDE);
        CHECK_WEQI(tlr_locale(l, 0, &r), L"fr-FR");
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(t); TLR_END();
}
TEST(def_layout_wide_getters_match_format)
{
    TLR_BEGIN(); IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(f, TLR_NARROW, X.coll, (DWRITE_FONT_WEIGHT)650, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STRETCH_EXPANDED, 21.25f, L"de-DE", &t);
    if (FAILED(hr)) { TLR_END(); t_skip("format %s", dw_hr_name(hr)); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 1000, 1000, &l), S_OK);
    if (l) {
        WCHAR b[64]; IDWriteTextFormat *lf = (IDWriteTextFormat *)l;
        CHECK(IDWriteTextFormat_GetFontWeight(lf) == (DWRITE_FONT_WEIGHT)650);
        CHECK(IDWriteTextFormat_GetFontStyle(lf) == DWRITE_FONT_STYLE_OBLIQUE);
        CHECK(IDWriteTextFormat_GetFontStretch(lf) == DWRITE_FONT_STRETCH_EXPANDED);
        CHECK_FBITS(IDWriteTextFormat_GetFontSize(lf), 21.25f);
        EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(lf), 8u);
        CHECK_HR(IDWriteTextFormat_GetFontFamilyName(lf, b, 64), S_OK); CHECK_WEQ(b, TLR_NARROW);
        CHECK_HR(IDWriteTextFormat_GetLocaleName(lf, b, 64), S_OK); CHECK_WEQI(b, L"de-DE");
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(t); TLR_END();
}
TEST(def_layout_wide_getters_ignore_range_setters)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello world", 16.0f);
    IDWriteTextFormat *lf = (IDWriteTextFormat *)l; WCHAR b[64];
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 11)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, RNG(0, 11)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontStretch(l, DWRITE_FONT_STRETCH_CONDENSED, RNG(0, 11)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 40.0f, RNG(0, 11)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, TLR_WIDE, RNG(0, 11)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"ja-JP", RNG(0, 11)), S_OK);
    /* the layout-wide getters read the layout's default format, which only the format-level setters (none for fonts) can change */
    CHECK(IDWriteTextFormat_GetFontWeight(lf) == DWRITE_FONT_WEIGHT_NORMAL);
    CHECK(IDWriteTextFormat_GetFontStyle(lf) == DWRITE_FONT_STYLE_NORMAL);
    CHECK(IDWriteTextFormat_GetFontStretch(lf) == DWRITE_FONT_STRETCH_NORMAL);
    CHECK_FBITS(IDWriteTextFormat_GetFontSize(lf), 16.0f);
    CHECK_HR(IDWriteTextFormat_GetFontFamilyName(lf, b, 64), S_OK); CHECK_WEQ(b, TLR_FAM);
    CHECK_HR(IDWriteTextFormat_GetLocaleName(lf, b, 64), S_OK); CHECK_WEQ(b, TLR_LOC);
    /* while the per-range getters report the new values */
    CHECK(tlr_weight(l, 3, NULL) == DWRITE_FONT_WEIGHT_BOLD);
    CHECK_FBITS(tlr_size(l, 3, NULL), 40.0f);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_char_spacing_zero)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); TLR_QI(l1, IDWriteTextLayout1, l); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        float a = 1, b = 2, c = 3; DWRITE_TEXT_RANGE r = POISON_RANGE;
        CHECK_HR(IDWriteTextLayout1_GetCharacterSpacing(l1, g_pos[i], &a, &b, &c, &r), S_OK);
        CHECK_FLOAT_EQ(a, 0.0f); CHECK_FLOAT_EQ(b, 0.0f); CHECK_FLOAT_EQ(c, 0.0f);
        CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_pair_kerning_false)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); TLR_QI(l1, IDWriteTextLayout1, l); int i;
    for (i = 0; i < NPOS; i++) {
        TLR_SKIP_MAXPOS(g_pos[i])
        BOOL k = 0x5a5a; DWRITE_TEXT_RANGE r = POISON_RANGE;
        CHECK_HR(IDWriteTextLayout1_GetPairKerning(l1, g_pos[i], &k, &r), S_OK);
        CHECK(k == FALSE); CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_empty_text_layout)
{
    TLR_BEGIN(); IDWriteTextFormat *t = tlr_format(&X, TLR_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t) { TLR_END(); t_skip("format"); return; }
    HRESULT hr = IDWriteFactory_CreateTextLayout(f, L"", 0, t, 1000, 1000, &l);
    if (FAILED(hr) || !l) { IDWriteTextFormat_Release(t); TLR_END(); t_skip("empty text layout: %s", dw_hr_name(hr)); return; }
    {
        DWRITE_TEXT_RANGE r;
        CHECK(tlr_weight(l, 0, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
        CHECK_FBITS(tlr_size(l, 0, &r), 16.0f);                   CHECK_RANGE(r, 0, TLR_ALL);
        CHECK_WEQ(tlr_family(l, 0, &r), TLR_FAM);
        CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 3)), S_OK);
        CHECK(tlr_weight(l, 1, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 0, 3);
        CHECK(tlr_weight(l, 3, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 3, TLR_ALL - 3);
    }
    IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t); TLR_END();
}
TEST(def_getters_do_not_modify_state)
{
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); int i; DWRITE_TEXT_RANGE r;
    for (i = 0; i < 50; i++) { tlr_weight(l, (UINT32)(i * 7), NULL); tlr_family(l, (UINT32)i, NULL); }
    CHECK(tlr_weight(l, 2, &r) == DWRITE_FONT_WEIGHT_NORMAL);
    CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(def_getter_null_range_pointer_accepted)
{
    /* GetRunIndexerCurrentTextRange tests the range pointer for NULL: every Get* accepts a NULL textRange */
    TLR_BEGIN(); TLR_LAYOUT(l, L"Hello", 16.0f); WCHAR b[32]; UINT32 len = 0;
    DWRITE_FONT_WEIGHT w = 0; DWRITE_FONT_STYLE s = 9; DWRITE_FONT_STRETCH st = 0; float sz = 0; BOOL u = 7, k = 7;
    IUnknown *e = TLR_SENT(IUnknown *); IDWriteInlineObject *io = TLR_SENT(IDWriteInlineObject *); IDWriteTypography *ty = TLR_SENT(IDWriteTypography *);
    IDWriteFontCollection *fc = TLR_SENT(IDWriteFontCollection *);
    CHECK_HR(IDWriteTextLayout_GetFontWeight(l, 0, &w, NULL), S_OK);   CHECK(w == DWRITE_FONT_WEIGHT_NORMAL);
    CHECK_HR(IDWriteTextLayout_GetFontStyle(l, 0, &s, NULL), S_OK);    CHECK(s == DWRITE_FONT_STYLE_NORMAL);
    CHECK_HR(IDWriteTextLayout_GetFontStretch(l, 0, &st, NULL), S_OK); CHECK(st == DWRITE_FONT_STRETCH_NORMAL);
    CHECK_HR(IDWriteTextLayout_GetFontSize(l, 0, &sz, NULL), S_OK);    CHECK_FBITS(sz, 16.0f);
    CHECK_HR(IDWriteTextLayout_GetUnderline(l, 0, &u, NULL), S_OK);    CHECK(u == FALSE);
    CHECK_HR(IDWriteTextLayout_GetStrikethrough(l, 0, &k, NULL), S_OK); CHECK(k == FALSE);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &len, NULL), S_OK); EXPECT_EQ(len, 6u);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, b, 32, NULL), S_OK); CHECK_WEQ(b, TLR_FAM);
    CHECK_HR(IDWriteTextLayout_GetLocaleNameLength(l, 0, &len, NULL), S_OK); EXPECT_EQ(len, 5u);
    CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 0, b, 32, NULL), S_OK); CHECK_WEQ(b, TLR_LOC);
    CHECK_HR(IDWriteTextLayout_GetDrawingEffect(l, 0, &e, NULL), S_OK);   CHECK(e == NULL);
    CHECK_HR(IDWriteTextLayout_GetInlineObject(l, 0, &io, NULL), S_OK);   CHECK(io == NULL);
    CHECK_HR(IDWriteTextLayout_GetTypography(l, 0, &ty, NULL), S_OK);     CHECK(ty == NULL);
    CHECK_HR(IDWriteTextLayout_GetFontCollection(l, 0, &fc, NULL), S_OK); CHECK(fc != NULL && fc != TLR_SENT(IDWriteFontCollection *));
    DW_RELEASE(fc);
    IDWriteTextLayout_Release(l); TLR_END();
}
