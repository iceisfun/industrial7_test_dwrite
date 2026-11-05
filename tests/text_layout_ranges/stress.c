/* Volume and robustness: many ranges, many calls, long text. Loop bounds stay small and the time limits are generous: a slow machine
 * must not fail these, a quadratic-or-worse blow-up or a hang must. */
#include "tlr.h"

static WCHAR *make_text(int n)
{
    WCHAR *t = (WCHAR *)malloc(sizeof(WCHAR) * (size_t)(n + 1)); int i;
    for (i = 0; i < n; i++) t[i] = (i % 17 == 16) ? L' ' : (WCHAR)(L'a' + (i % 26));
    t[n] = 0; return t;
}

TEST(stress_1000_alternating_weight_ranges)
{
    WCHAR *text = make_text(2000); DWORD t0; int i;
    TLR_BEGIN(); TLR_LAYOUT(l, text, 16.0f); DWRITE_TEXT_RANGE r;
    t0 = GetTickCount();
    for (i = 0; i < 1000; i++) {
        HRESULT hr = IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG((UINT32)(2 * i), 1));
        if (hr != S_OK) { CHECK_HR(hr, S_OK); break; }
    }
    t_check(GetTickCount() - t0 < 20000, __FILE__, __LINE__, "1000 SetFontWeight calls", "took %lu ms", GetTickCount() - t0);
    for (i = 0; i < 999; i += 37) {
        CHECK(tlr_weight(l, (UINT32)(2 * i), &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 2 * i, 1);
        CHECK(tlr_weight(l, (UINT32)(2 * i + 1), &r) == DWRITE_FONT_WEIGHT_NORMAL);
        CHECK_RANGE(r, 2 * i + 1, 1);
    }
    CHECK(tlr_weight(l, 1999, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 1999, TLR_ALL - 1999);
    t0 = GetTickCount();
    { DWRITE_TEXT_METRICS m; HRESULT hr = IDWriteTextLayout_GetMetrics(l, &m); CHECK_HR(hr, S_OK); if (hr == S_OK) { EXPECT_EQ(m.lineCount >= 1, 1u); } }
    t_check(GetTickCount() - t0 < 30000, __FILE__, __LINE__, "layout with 1000 runs", "took %lu ms", GetTickCount() - t0);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_NORMAL, RNG(0, 2000)), S_OK);
    CHECK(tlr_weight(l, 100, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); TLR_END(); free(text);
}
TEST(stress_1000_ranges_mixed_properties)
{
    WCHAR *text = make_text(1500); DWORD t0; int i; UINT32 seed = 12345;
    TLR_BEGIN(); TLR_LAYOUT(l, text, 16.0f);
    t0 = GetTickCount();
    for (i = 0; i < 1000; i++) {
        UINT32 s = tlr_rand(&seed) % 1500, n = 1 + tlr_rand(&seed) % 20; HRESULT hr;
        switch (i % 5) {
        case 0: hr = IDWriteTextLayout_SetFontWeight(l, (i & 8) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, RNG(s, n)); break;
        case 1: hr = IDWriteTextLayout_SetFontSize(l, 10.0f + (float)(i % 7), RNG(s, n)); break;
        case 2: hr = IDWriteTextLayout_SetUnderline(l, i & 1, RNG(s, n)); break;
        case 3: hr = IDWriteTextLayout_SetFontStyle(l, (i & 4) ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, RNG(s, n)); break;
        default: hr = IDWriteTextLayout_SetStrikethrough(l, i & 2, RNG(s, n)); break;
        }
        if (hr != S_OK) { CHECK_HR(hr, S_OK); break; }
    }
    t_check(GetTickCount() - t0 < 30000, __FILE__, __LINE__, "1000 mixed Set calls", "took %lu ms", GetTickCount() - t0);
    { DWRITE_TEXT_METRICS m; CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK); }
    IDWriteTextLayout_Release(l); TLR_END(); free(text);
}
TEST(stress_many_getter_calls)
{
    int i; DWORD t0; DWRITE_TEXT_RANGE r;
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefghij", 16.0f);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(3, 3)), S_OK);
    t0 = GetTickCount();
    for (i = 0; i < 20000; i++) {
        DWRITE_FONT_WEIGHT w; UINT32 pos = (UINT32)(i % 12);
        HRESULT hr = IDWriteTextLayout_GetFontWeight(l, pos, &w, &r);
        if (hr != S_OK || w != ((pos >= 3 && pos < 6) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL)) { t_check(0, __FILE__, __LINE__, "GetFontWeight loop", "pos %u -> %s %d", pos, dw_hr_name(hr), (int)w); break; }
    }
    t_check(GetTickCount() - t0 < 20000, __FILE__, __LINE__, "20000 getter calls", "took %lu ms", GetTickCount() - t0);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(stress_set_revert_cycles)
{
    int i; DWRITE_TEXT_RANGE r;
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefghij", 16.0f);
    for (i = 0; i < 500; i++) {
        UINT32 s = (UINT32)(i % 10);
        CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(s, 3)), S_OK);
        CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_NORMAL, RNG(s, 3)), S_OK);
    }
    CHECK(tlr_weight(l, 4, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); TLR_END();
}
TEST(stress_long_text_few_ranges)
{
    WCHAR *text = make_text(20000); DWORD t0; DWRITE_TEXT_RANGE r; DWRITE_TEXT_METRICS m;
    TLR_BEGIN(); TLR_LAYOUT(l, text, 16.0f);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(10000, 5000)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(19990, 100)), S_OK);
    CHECK(tlr_weight(l, 12000, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 10000, 5000);
    CHECK_FBITS(tlr_size(l, 19999, &r), 32.0f); CHECK_RANGE(r, 19990, 100);
    t0 = GetTickCount();
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK);
    t_check(GetTickCount() - t0 < 30000, __FILE__, __LINE__, "layout of 20000 characters", "took %lu ms", GetTickCount() - t0);
    IDWriteTextLayout_Release(l); TLR_END(); free(text);
}
TEST(stress_many_layouts_independent_ranges)
{
    IDWriteTextLayout *ls[16]; int i; DWRITE_TEXT_RANGE r;
    TLR_BEGIN();
    memset(ls, 0, sizeof ls);
    for (i = 0; i < 16; i++) {
        ls[i] = tlr_layout_(&X, TLR_FAM, L"abcdefghij", 16.0f);
        if (!ls[i]) break;
        CHECK_HR(IDWriteTextLayout_SetFontWeight(ls[i], (DWRITE_FONT_WEIGHT)(101 + 50 * i), RNG((UINT32)i % 8, 2)), S_OK);
    }
    for (i = 0; i < 16; i++) {
        if (!ls[i]) continue;
        CHECK(tlr_weight(ls[i], (UINT32)i % 8, &r) == (DWRITE_FONT_WEIGHT)(101 + 50 * i)); CHECK_RANGE(r, (UINT32)i % 8, 2);
        IDWriteTextLayout_Release(ls[i]);
    }
    TLR_END();
}
TEST(stress_family_names_many_distinct)
{
    int i; WCHAR name[40]; DWRITE_TEXT_RANGE r;
    TLR_BEGIN(); TLR_LAYOUT(l, L"abcdefghijklmnopqrstuvwxyz0123456789", 16.0f);
    for (i = 0; i < 36; i++) {
        swprintf(name, 40, L"Family%d", i);
        CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, name, RNG((UINT32)i, 1)), S_OK);
    }
    for (i = 0; i < 36; i++) {
        swprintf(name, 40, L"Family%d", i);
        CHECK_WEQ(tlr_family(l, (UINT32)i, &r), name); CHECK_RANGE(r, i, 1);
    }
    IDWriteTextLayout_Release(l); TLR_END();
}
