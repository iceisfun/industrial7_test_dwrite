/* Per-property setter and getter value and argument validation.
 *
 * Validation visible in the decompilation of the public entry points:
 *   weight    1..999 accepted (anything else E_INVALIDARG, before the range is looked at)
 *   style     0..2 (NORMAL, OBLIQUE, ITALIC)
 *   stretch   1..9
 *   size      > 0 and not NaN (+INF passes, -0.0 does not)
 *   family / locale  NULL string => E_INVALIDARG
 *   every setter: start + length must not wrap in 32 bits (E_INVALIDARG), length 0 is a successful no-op
 * Getters never fail, zero/overwrite their outputs, and accept a NULL range pointer.
 */
#include "tlr.h"

#define T10 L"abcdefghij"

/* ---- weight -------------------------------------------------------------------------------------------------------------- */
TEST(set_weight_valid_values_roundtrip)
{
    static const int v[] = { 1, 2, 50, 100, 150, 200, 300, 350, 400, 450, 500, 550, 600, 700, 800, 850, 900, 950, 998, 999 }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        DWRITE_TEXT_RANGE r;
        HRESULT hr = IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)v[i], RNG(2, 3));
        t_check(hr == S_OK, __FILE__, __LINE__, "SetFontWeight valid", "weight %d -> %s", v[i], dw_hr_name(hr));
        t_check(tlr_weight(l, 3, &r) == (DWRITE_FONT_WEIGHT)v[i], __FILE__, __LINE__, "weight roundtrip", "weight %d", v[i]);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_weight_invalid_values_rejected)
{
    static const int v[] = { 0, -1, -100, 1000, 1001, 1500, 0x7FFFFFFF, (int)0x80000000, (int)0xFFFFFFFF }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        DWRITE_TEXT_RANGE r;
        HRESULT hr = IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)v[i], RNG(2, 3));
        t_check(hr == TLR_EINVALIDARG, __FILE__, __LINE__, "SetFontWeight invalid", "weight %d -> %s", v[i], dw_hr_name(hr));
        CHECK(tlr_weight(l, 3, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_weight_invalid_checked_before_empty_range)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)0, RNG(0, 0)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)1000, RNG(5, 0)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)400, RNG(5, 0)), S_OK);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_weight_named_constants)
{
    static const DWRITE_FONT_WEIGHT v[] = { DWRITE_FONT_WEIGHT_THIN, DWRITE_FONT_WEIGHT_EXTRA_LIGHT, DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_WEIGHT_SEMI_LIGHT,
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_WEIGHT_DEMI_BOLD, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_WEIGHT_EXTRA_BOLD,
        DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_WEIGHT_EXTRA_BLACK }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        CHECK_HR(IDWriteTextLayout_SetFontWeight(l, v[i], RNG(0, 10)), S_OK);
        CHECK(tlr_weight(l, 9, NULL) == v[i]);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- style --------------------------------------------------------------------------------------------------------------- */
TEST(set_style_valid_values)
{
    static const DWRITE_FONT_STYLE v[] = { DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STYLE_ITALIC }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < 3; i++) {
        DWRITE_TEXT_RANGE r;
        CHECK_HR(IDWriteTextLayout_SetFontStyle(l, v[i], RNG(1, 4)), S_OK);
        CHECK(tlr_style(l, 2, &r) == v[i]);
        CHECK_HR(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_NORMAL, RNG(1, 4)), S_OK);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_style_invalid_values_rejected)
{
    static const int v[] = { 3, 4, 100, -1, 0x7FFFFFFF, (int)0x80000000 }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        DWRITE_TEXT_RANGE r;
        HRESULT hr = IDWriteTextLayout_SetFontStyle(l, (DWRITE_FONT_STYLE)v[i], RNG(1, 4));
        t_check(hr == TLR_EINVALIDARG, __FILE__, __LINE__, "SetFontStyle invalid", "style %d -> %s", v[i], dw_hr_name(hr));
        CHECK(tlr_style(l, 2, &r) == DWRITE_FONT_STYLE_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
    }
    CHECK_HR(IDWriteTextLayout_SetFontStyle(l, (DWRITE_FONT_STYLE)3, RNG(0, 0)), TLR_EINVALIDARG);
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- stretch ------------------------------------------------------------------------------------------------------------- */
TEST(set_stretch_valid_values)
{
    int v;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (v = 1; v <= 9; v++) {
        DWRITE_TEXT_RANGE r;
        HRESULT hr = IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)v, RNG(0, 10));
        t_check(hr == S_OK, __FILE__, __LINE__, "SetFontStretch valid", "stretch %d -> %s", v, dw_hr_name(hr));
        t_check(tlr_stretch(l, 5, &r) == (DWRITE_FONT_STRETCH)v, __FILE__, __LINE__, "stretch roundtrip", "stretch %d", v);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_stretch_invalid_values_rejected)
{
    static const int v[] = { 0, 10, 11, 100, -1, 0x7FFFFFFF, (int)0x80000000 }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        DWRITE_TEXT_RANGE r;
        HRESULT hr = IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)v[i], RNG(1, 4));
        t_check(hr == TLR_EINVALIDARG, __FILE__, __LINE__, "SetFontStretch invalid", "stretch %d -> %s", v[i], dw_hr_name(hr));
        CHECK(tlr_stretch(l, 2, &r) == DWRITE_FONT_STRETCH_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
    }
    CHECK_HR(IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)0, RNG(3, 0)), TLR_EINVALIDARG);
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- size ---------------------------------------------------------------------------------------------------------------- */
TEST(set_size_valid_values_roundtrip)
{
    static const float v[] = { 0.001f, 0.5f, 1.0f, 8.5f, 12.0f, 16.0f, 96.25f, 1000.0f, 65536.0f, 1e7f };
    int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        DWRITE_TEXT_RANGE r;
        HRESULT hr = IDWriteTextLayout_SetFontSize(l, v[i], RNG(2, 3));
        t_check(hr == S_OK, __FILE__, __LINE__, "SetFontSize valid", "size %g -> %s", v[i], dw_hr_name(hr));
        CHECK_FBITS(tlr_size(l, 3, &r), v[i]);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_size_extreme_positive_values_are_accepted)
{
    /* only <= 0 and NaN are rejected; no upper bound is applied by the setter (do not lay these out) */
    static const float v[] = { FLT_MIN, 1e-30f, FLT_MAX, INFINITY }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < 4; i++) {
        DWRITE_TEXT_RANGE r;
        HRESULT hr = IDWriteTextLayout_SetFontSize(l, v[i], RNG(2, 3));
        t_check(hr == S_OK, __FILE__, __LINE__, "SetFontSize extreme", "size %g -> %s", v[i], dw_hr_name(hr));
        CHECK_FBITS(tlr_size(l, 3, &r), v[i]);
        CHECK_HR(IDWriteTextLayout_SetFontSize(l, 16.0f, RNG(2, 3)), S_OK);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_size_invalid_values_rejected)
{
    union { float f; UINT32 u; } nan1 = { .u = 0x7FC00000 }, nan2 = { .u = 0xFFC00001 }, nan3 = { .u = 0x7F800001 }, nz = { .u = 0x80000000 };
    float v[] = { 0.0f, nz.f, -1.0f, -0.001f, -FLT_MIN, -FLT_MAX, -INFINITY, nan1.f, nan2.f, nan3.f }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        DWRITE_TEXT_RANGE r;
        HRESULT hr = IDWriteTextLayout_SetFontSize(l, v[i], RNG(2, 3));
        t_check(hr == TLR_EINVALIDARG, __FILE__, __LINE__, "SetFontSize invalid", "case %d (%g) -> %s", i, (double)v[i], dw_hr_name(hr));
        CHECK_FBITS(tlr_size(l, 3, &r), 16.0f); CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_size_invalid_checked_before_empty_range)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 0.0f, RNG(0, 0)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, -5.0f, RNG(3, 0)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 5.0f, RNG(3, 0)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 5.0f, RNG(1, TLR_ALL)), TLR_EINVALIDARG);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_size_equal_to_default_does_not_split)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 16.0f, RNG(3, 3)), S_OK);
    CHECK_FBITS(tlr_size(l, 0, &r), 16.0f); CHECK_RANGE(r, 0, TLR_ALL);
    CHECK_FBITS(tlr_size(l, 4, &r), 16.0f); CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- underline / strikethrough --------------------------------------------------------------------------------------------- */
TEST(set_underline_any_nonzero_is_true)
{
    static const int v[] = { 1, 2, -1, 0x100, 0x10000, 0x7FFFFFFF, (int)0x80000000 }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        DWRITE_TEXT_RANGE r; BOOL u;
        CHECK_HR(IDWriteTextLayout_SetUnderline(l, v[i], RNG(2, 3)), S_OK);
        u = tlr_underline(l, 3, &r);
        t_check(u == TRUE, __FILE__, __LINE__, "GetUnderline", "set %d: got %d (the value is normalised to 0/1)", v[i], (int)u);
        CHECK_RANGE(r, 2, 3);
        CHECK_HR(IDWriteTextLayout_SetUnderline(l, FALSE, RNG(2, 3)), S_OK);
        CHECK(tlr_underline(l, 3, &r) == FALSE); CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_strikethrough_any_nonzero_is_true)
{
    static const int v[] = { 1, 2, -1, 0x100, 0x10000, 0x7FFFFFFF, (int)0x80000000 }; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        DWRITE_TEXT_RANGE r; BOOL u;
        CHECK_HR(IDWriteTextLayout_SetStrikethrough(l, v[i], RNG(2, 3)), S_OK);
        u = tlr_strike(l, 3, &r);
        t_check(u == TRUE, __FILE__, __LINE__, "GetStrikethrough", "set %d: got %d (the value is normalised to 0/1)", v[i], (int)u);
        CHECK_RANGE(r, 2, 3);
        CHECK_HR(IDWriteTextLayout_SetStrikethrough(l, FALSE, RNG(2, 3)), S_OK);
        CHECK(tlr_strike(l, 3, &r) == FALSE); CHECK_RANGE(r, 0, TLR_ALL);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_underline_and_strikethrough_are_independent)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 6)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(3, 6)), S_OK);
    CHECK(tlr_underline(l, 1, &r) == TRUE);  CHECK_RANGE(r, 0, 3);
    CHECK(tlr_strike(l, 1, &r) == FALSE);    CHECK_RANGE(r, 0, 3);
    CHECK(tlr_underline(l, 4, &r) == TRUE);  CHECK_RANGE(r, 3, 3);
    CHECK(tlr_strike(l, 4, &r) == TRUE);     CHECK_RANGE(r, 3, 3);
    CHECK(tlr_underline(l, 7, &r) == FALSE); CHECK_RANGE(r, 6, 3);
    CHECK(tlr_strike(l, 7, &r) == TRUE);     CHECK_RANGE(r, 6, 3);
    CHECK(tlr_underline(l, 9, &r) == FALSE); CHECK_RANGE(r, 9, TLR_ALL - 9);
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- family name ---------------------------------------------------------------------------------------------------------- */
TEST(set_family_null_rejected)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, RNG(2, 3)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, RNG(0, 0)), TLR_EINVALIDARG);    /* the NULL check comes before the empty-range shortcut */
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, RNG(1, TLR_ALL)), TLR_EINVALIDARG);
    CHECK_WEQ(tlr_family(l, 3, &r), TLR_FAM); CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_family_string_stored_verbatim)
{
    static const WCHAR *const names[] = { L"DwNarrow", L"dwnarrow", L"DWNARROW", L"Arial", L"Segoe UI Emoji", L"A", L"Name With  Two Spaces", L"NoSuchFamily-0123456789",
                                           L"\x65E5\x672C\x8A9E", L"Family\xD83D\xDE00" };
    int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof names / sizeof names[0]); i++) {
        DWRITE_TEXT_RANGE r;
        CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, names[i], RNG(2, 3)), S_OK);
        CHECK_WEQ(tlr_family(l, 3, &r), names[i]);
        CHECK_RANGE(r, 2, 3);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_family_empty_string)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r; HRESULT hr = IDWriteTextLayout_SetFontFamilyName(l, L"", RNG(2, 3));
    t_note("SetFontFamilyName(L\"\") -> %s", dw_hr_name(hr));
    if (hr == S_OK) { CHECK_WEQ(tlr_family(l, 3, &r), L""); CHECK_RANGE(r, 2, 3); }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_family_long_name)
{
    WCHAR big[300]; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r; HRESULT hr;
    for (i = 0; i < 299; i++) big[i] = (WCHAR)(L'a' + (i % 26));
    big[299] = 0;
    hr = IDWriteTextLayout_SetFontFamilyName(l, big, RNG(2, 3));
    t_note("SetFontFamilyName(299 chars) -> %s", dw_hr_name(hr));
    if (hr == S_OK) { UINT32 len = 0; CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 3, &len, &r), S_OK); EXPECT_EQ(len, 299u); }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(get_family_buffer_protocol)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    WCHAR b[64]; DWRITE_TEXT_RANGE r; UINT32 len = 0;
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, L"DwNarrow", RNG(2, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 3, &len, &r), S_OK); EXPECT_EQ(len, 8u); CHECK_RANGE(r, 2, 3);
    /* exact: length + 1 */
    r = POISON_RANGE; wmemset(b, L'X', 64);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 3, b, 9, &r), S_OK); CHECK_WEQ(b, L"DwNarrow"); CHECK_RANGE(r, 2, 3);
    EXPECT_EQ(b[9], L'X');                                  /* nothing written past the buffer */
    /* larger */
    r = POISON_RANGE;
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 3, b, 64, &r), S_OK); CHECK_WEQ(b, L"DwNarrow"); CHECK_RANGE(r, 2, 3);
    /* exact - 1: no room for the terminator => E_NOT_SUFFICIENT_BUFFER, range untouched */
    r = POISON_RANGE; wmemset(b, L'X', 64);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 3, b, 8, &r), TLR_ENOBUF);
    CHECK_RANGE(r, 0xdeadbeefu, 0xcafef00du);
    EXPECT_EQ(b[8], L'X');                                  /* never writes beyond `size` characters */
    t_note("buffer after size==len failure: \"%.8ls\"", b);
    /* size 1: only the terminator fits */
    r = POISON_RANGE; wmemset(b, L'X', 64);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 3, b, 1, &r), TLR_ENOBUF);
    EXPECT_EQ(b[0], 0u); EXPECT_EQ(b[1], L'X');
    /* size 0 with a real buffer: nothing is written */
    r = POISON_RANGE; wmemset(b, L'X', 64);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 3, b, 0, &r), TLR_ENOBUF);
    EXPECT_EQ(b[0], L'X'); CHECK_RANGE(r, 0xdeadbeefu, 0xcafef00du);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(get_family_buffer_protocol_per_position)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    WCHAR b[64]; DWRITE_TEXT_RANGE r; UINT32 len;
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, L"DwNarrow", RNG(2, 3)), S_OK);       /* 8 chars inside, 6 ("DwTest") outside */
    len = 0; CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &len, &r), S_OK); EXPECT_EQ(len, 6u); CHECK_RANGE(r, 0, 2);
    len = 0; CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 4, &len, &r), S_OK); EXPECT_EQ(len, 8u); CHECK_RANGE(r, 2, 3);
    len = 0; CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 5, &len, &r), S_OK); EXPECT_EQ(len, 6u); CHECK_RANGE(r, 5, TLR_ALL - 5);
    /* a buffer that fits the short name at position 0 does not fit the longer one at position 3 */
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, b, 7, &r), S_OK); CHECK_WEQ(b, TLR_FAM);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 3, b, 7, &r), TLR_ENOBUF);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 3, b, 9, &r), S_OK); CHECK_WEQ(b, TLR_NARROW);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 50, b, 7, &r), S_OK); CHECK_WEQ(b, TLR_FAM);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(get_family_length_does_not_touch_range_when_null)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); UINT32 len = 0xdead;
    CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 7, &len, NULL), S_OK); EXPECT_EQ(len, 6u);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(get_family_empty_name_buffer_protocol)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); WCHAR b[4]; DWRITE_TEXT_RANGE r; UINT32 len = 77;
    if (FAILED(IDWriteTextLayout_SetFontFamilyName(l, L"", RNG(0, 10)))) { IDWriteTextLayout_Release(l); TLR_END(); t_skip("empty family name rejected"); return; }
    CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &len, &r), S_OK); EXPECT_EQ(len, 0u);
    wmemset(b, L'X', 4);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, b, 1, &r), S_OK); EXPECT_EQ(b[0], 0u);
    wmemset(b, L'X', 4);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, b, 0, &r), TLR_ENOBUF); EXPECT_EQ(b[0], L'X');
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- locale name ---------------------------------------------------------------------------------------------------------- */
TEST(set_locale_null_rejected)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, NULL, RNG(2, 3)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, NULL, RNG(0, 0)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, NULL, RNG(5, TLR_ALL)), TLR_EINVALIDARG);
    CHECK_WEQI(tlr_locale(l, 3, &r), TLR_LOC); CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_locale_valid_tags_roundtrip)
{
    static const WCHAR *const tags[] = { L"en-us", L"en-US", L"fr-FR", L"de-DE", L"ja-JP", L"zh-Hans", L"zh-CN", L"ar-SA", L"he-IL", L"en", L"pt-BR", L"sr-Latn-RS" };
    int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof tags / sizeof tags[0]); i++) {
        DWRITE_TEXT_RANGE r; HRESULT hr = IDWriteTextLayout_SetLocaleName(l, tags[i], RNG(2, 3));
        t_check(hr == S_OK, __FILE__, __LINE__, "SetLocaleName", "tag %ls -> %s", tags[i], dw_hr_name(hr));
        if (hr != S_OK) continue;
        CHECK_WEQI(tlr_locale(l, 3, &r), tags[i]);
        if (_wcsicmp(tags[i], TLR_LOC)) CHECK_RANGE(r, 2, 3);       /* the default tag itself may merge with the untouched text */
        else t_note("range for tag equal to the default: {%u,%u}", r.startPosition, r.length);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_locale_unusual_strings_are_noted)
{
    static const WCHAR *const odd[] = { L"", L"x", L"not a locale", L"en_US", L"EN-us", L"123" };
    WCHAR big[200]; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    for (i = 0; i < (int)(sizeof odd / sizeof odd[0]); i++) {
        DWRITE_TEXT_RANGE r; HRESULT hr = IDWriteTextLayout_SetLocaleName(l, odd[i], RNG(2, 3));
        t_note("SetLocaleName(\"%ls\") -> %s", odd[i], dw_hr_name(hr));
        if (hr == S_OK) { t_note("   reads back \"%ls\"", tlr_locale(l, 3, &r)); IDWriteTextLayout_SetLocaleName(l, L"en-us", RNG(2, 3)); }
    }
    for (i = 0; i < 199; i++) big[i] = L'a';
    big[199] = 0;
    t_note("SetLocaleName(199 chars) -> %s", dw_hr_name(IDWriteTextLayout_SetLocaleName(l, big, RNG(2, 3))));
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(get_locale_buffer_protocol)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f);
    WCHAR b[64]; DWRITE_TEXT_RANGE r; UINT32 len = 0;
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", RNG(2, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout_GetLocaleNameLength(l, 3, &len, &r), S_OK); EXPECT_EQ(len, 5u); CHECK_RANGE(r, 2, 3);
    r = POISON_RANGE; wmemset(b, L'X', 64);
    CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 3, b, 6, &r), S_OK); CHECK_WEQI(b, L"fr-fr"); CHECK_RANGE(r, 2, 3); EXPECT_EQ(b[6], L'X');
    r = POISON_RANGE; wmemset(b, L'X', 64);
    CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 3, b, 5, &r), TLR_ENOBUF); CHECK_RANGE(r, 0xdeadbeefu, 0xcafef00du); EXPECT_EQ(b[5], L'X');
    r = POISON_RANGE; wmemset(b, L'X', 64);
    CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 3, b, 1, &r), TLR_ENOBUF); EXPECT_EQ(b[0], 0u); EXPECT_EQ(b[1], L'X');
    r = POISON_RANGE; wmemset(b, L'X', 64);
    CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 3, b, 0, &r), TLR_ENOBUF); EXPECT_EQ(b[0], L'X');
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(get_locale_per_position)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", RNG(0, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"de-de", RNG(3, 3)), S_OK);
    CHECK_WEQI(tlr_locale(l, 0, &r), L"fr-fr"); CHECK_RANGE(r, 0, 3);
    CHECK_WEQI(tlr_locale(l, 2, &r), L"fr-fr"); CHECK_RANGE(r, 0, 3);
    CHECK_WEQI(tlr_locale(l, 3, &r), L"de-de"); CHECK_RANGE(r, 3, 3);
    CHECK_WEQI(tlr_locale(l, 5, &r), L"de-de"); CHECK_RANGE(r, 3, 3);
    CHECK_WEQI(tlr_locale(l, 6, &r), TLR_LOC);  CHECK_RANGE(r, 6, TLR_ALL - 6);
    CHECK_WEQI(tlr_locale(l, 10, &r), TLR_LOC); CHECK_RANGE(r, 6, TLR_ALL - 6);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(get_locale_length_differs_per_range)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r; UINT32 len;
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"zh-Hans-CN", RNG(4, 2)), S_OK);
    len = 0; CHECK_HR(IDWriteTextLayout_GetLocaleNameLength(l, 4, &len, &r), S_OK); EXPECT_EQ(len, 10u); CHECK_RANGE(r, 4, 2);
    len = 0; CHECK_HR(IDWriteTextLayout_GetLocaleNameLength(l, 0, &len, &r), S_OK); EXPECT_EQ(len, 5u);  CHECK_RANGE(r, 0, 4);
    len = 0; CHECK_HR(IDWriteTextLayout_GetLocaleNameLength(l, 6, &len, &r), S_OK); EXPECT_EQ(len, 5u);  CHECK_RANGE(r, 6, TLR_ALL - 6);
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- font collection ------------------------------------------------------------------------------------------------------ */
TEST(set_collection_roundtrip_pointer_identity)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r; IDWriteFontCollection *sys = NULL, *g;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE));
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, sys, RNG(2, 3)), S_OK);
    g = tlr_coll(l, 3, &r); CHECK(g == sys); CHECK_RANGE(r, 2, 3); DW_RELEASE(g);
    g = tlr_coll(l, 0, &r); CHECK(g == X.coll || g != sys); CHECK_RANGE(r, 0, 2); DW_RELEASE(g);
    DW_RELEASE(sys); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_collection_null_is_accepted)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE r; IDWriteFontCollection *g;
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, NULL, RNG(2, 3)), S_OK);
    g = tlr_coll(l, 3, &r);
    t_note("collection after SetFontCollection(NULL): %p range {%u,%u}", (void *)g, r.startPosition, r.length);
    CHECK_RANGE(r, 2, 3);
    DW_RELEASE(g);
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- every setter: argument errors ------------------------------------------------------------------------------------------ */
TEST(set_all_setters_wrapping_range_rejected)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE bad = RNG(0x80000000u, 0x80000000u), bad2 = RNG(1, TLR_ALL);
    dwx_effect *e = dwx_effect_new(1); dwx_inline *io = dwx_inline_new(10, 10, 5, FALSE); IDWriteTypography *ty = NULL;
    IDWriteFactory_CreateTypography(f, &ty);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, L"x", bad), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, bad), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, bad), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontStretch(l, DWRITE_FONT_STRETCH_CONDENSED, bad), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 20.0f, bad), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, TRUE, bad2), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetStrikethrough(l, TRUE, bad2), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, bad2), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, bad2), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, ty, bad2), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", bad2), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, X.coll, bad2), TLR_EINVALIDARG);
    CHECK(tlr_weight(l, 3, NULL) == DWRITE_FONT_WEIGHT_NORMAL);
    EXPECT_EQ(dwx_refs(e), 1); EXPECT_EQ(dwx_refs(io), 1);
    EXPECT_REFS(ty, 1);
    DW_RELEASE(ty); IDWriteTextLayout_Release(l); dwx_release(e); dwx_release(io); TLR_END();
}
TEST(set_all_setters_empty_range_succeeds)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE z = RNG(4, 0);
    dwx_effect *e = dwx_effect_new(1); dwx_inline *io = dwx_inline_new(10, 10, 5, FALSE); IDWriteTypography *ty = NULL; DWRITE_TEXT_RANGE r;
    IDWriteFactory_CreateTypography(f, &ty);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, L"x", z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontStretch(l, DWRITE_FONT_STRETCH_CONDENSED, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 20.0f, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, TRUE, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetStrikethrough(l, TRUE, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, ty, z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", z), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, X.coll, z), S_OK);
    CHECK(tlr_weight(l, 4, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
    EXPECT_EQ(dwx_refs(e), 1); EXPECT_EQ(dwx_refs(io), 1);          /* nothing was retained */
    EXPECT_REFS(ty, 1);
    DW_RELEASE(ty); IDWriteTextLayout_Release(l); dwx_release(e); dwx_release(io); TLR_END();
}
TEST(set_returns_s_ok_for_every_setter_on_valid_input)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); DWRITE_TEXT_RANGE g = RNG(2, 3);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, L"DwNarrow", g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontStretch(l, DWRITE_FONT_STRETCH_CONDENSED, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 20.0f, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, TRUE, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetStrikethrough(l, TRUE, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, X.coll, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, NULL, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, NULL, g), S_OK);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, NULL, g), S_OK);
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- character spacing / pair kerning (IDWriteTextLayout1 per-range properties) ---------------------------------------------- */
TEST(set_character_spacing_roundtrip)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); TLR_QI(l1, IDWriteTextLayout1, l);
    float a, b, c; DWRITE_TEXT_RANGE r = POISON_RANGE;
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 1.5f, -2.25f, 3.0f, RNG(2, 3)), S_OK);
    a = b = c = 99; CHECK_HR(IDWriteTextLayout1_GetCharacterSpacing(l1, 3, &a, &b, &c, &r), S_OK);
    CHECK_FLOAT_EQ(a, 1.5f); CHECK_FLOAT_EQ(b, -2.25f); CHECK_FLOAT_EQ(c, 3.0f); CHECK_RANGE(r, 2, 3);
    a = b = c = 99; CHECK_HR(IDWriteTextLayout1_GetCharacterSpacing(l1, 8, &a, &b, &c, &r), S_OK);
    CHECK_FLOAT_EQ(a, 0.0f); CHECK_FLOAT_EQ(b, 0.0f); CHECK_FLOAT_EQ(c, 0.0f); CHECK_RANGE(r, 5, TLR_ALL - 5);
    IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_character_spacing_validation)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); TLR_QI(l1, IDWriteTextLayout1, l);
    union { float f; UINT32 u; } nan1 = { .u = 0x7FC00000 };
    float a, b, c; DWRITE_TEXT_RANGE r;
    /* NaN leading / trailing and a negative minimum advance are rejected; negative leading/trailing are fine */
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, nan1.f, 0, 0, RNG(2, 3)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, nan1.f, 0, RNG(2, 3)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, -0.5f, RNG(2, 3)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, -INFINITY, RNG(2, 3)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, nan1.f, RNG(2, 3)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 1, RNG(1, TLR_ALL)), TLR_EINVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, nan1.f, 0, 0, RNG(2, 0)), TLR_EINVALIDARG);      /* validation precedes the empty-range shortcut */
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, -3.0f, -4.0f, 0, RNG(2, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0, 0, 0, RNG(2, 0)), S_OK);
    a = b = c = 9; CHECK_HR(IDWriteTextLayout1_GetCharacterSpacing(l1, 3, &a, &b, &c, &r), S_OK);
    CHECK_FLOAT_EQ(a, -3.0f); CHECK_FLOAT_EQ(b, -4.0f); CHECK_FLOAT_EQ(c, 0.0f);
    IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
TEST(set_pair_kerning_normalises_bool)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); TLR_QI(l1, IDWriteTextLayout1, l);
    BOOL k = 0; DWRITE_TEXT_RANGE r = POISON_RANGE;
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, 7, RNG(2, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout1_GetPairKerning(l1, 3, &k, &r), S_OK); EXPECT_EQ(k, 1u); CHECK_RANGE(r, 2, 3);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, 0, RNG(2, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout1_GetPairKerning(l1, 3, &k, &r), S_OK); EXPECT_EQ(k, 0u); CHECK_RANGE(r, 0, TLR_ALL);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(1, TLR_ALL)), TLR_EINVALIDARG);
    IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
