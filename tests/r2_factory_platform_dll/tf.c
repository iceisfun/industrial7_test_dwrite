/* Factory object creation arguments: CreateTextFormat / CreateTextLayout / CreateNumberSubstitution validation and locale handling.
 * Decompilation: DWriteFactory::CreateTextFormat zeroes *out, then returns E_INVALIDARG when family or locale is NULL, weight is
 * outside 1..999, style > 2 (unsigned), stretch outside 1..9, or size is <= 0 or NaN; nothing else is validated there.
 * CreateTextLayout rejects a NULL string or format and negative / NaN maxWidth / maxHeight.
 * CreateNumberSubstitution accepts a NULL locale only for method NONE (2). */
#include "rfp.h"

static HRESULT mk(IDWriteFactory *f, const WCHAR *fam, int w, int st, int str, float sz, const WCHAR *loc, IDWriteTextFormat **o) {
    return IDWriteFactory_CreateTextFormat(f, fam, NULL, (DWRITE_FONT_WEIGHT)w, (DWRITE_FONT_STYLE)st, (DWRITE_FONT_STRETCH)str, sz, loc, o);
}

/* ---- weights --------------------------------------------------------------------------------------------- */
TEST(tfv_weight_valid_range) {
    static const int w[] = { 1, 2, 100, 200, 300, 350, 400, 500, 600, 700, 800, 900, 950, 998, 999 }; int i;
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    for (i = 0; i < (int)(sizeof w / sizeof w[0]); i++) {
        IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; HRESULT hr = mk(f, L"Arial", w[i], 0, 5, 12.0f, L"en-us", &tf);
        CHECKF(hr == S_OK && tf && tf != RFP_SENTINEL, "weight %d -> %s", w[i], dw_hr_name(hr));
        if (hr == S_OK) { EXPECT_EQ(IDWriteTextFormat_GetFontWeight(tf), w[i]); DW_RELEASE(tf); }
    }
}
TEST(tfv_weight_invalid_values) {
    static const int w[] = { 0, -1, -400, 1000, 1001, 1500, 65536, 0x7fffffff, (int)0x80000000 }; int i;
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    for (i = 0; i < (int)(sizeof w / sizeof w[0]); i++) {
        IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; HRESULT hr = mk(f, L"Arial", w[i], 0, 5, 12.0f, L"en-us", &tf);
        CHECKF(hr == E_INVALIDARG && tf == NULL, "weight %d -> %s %p", w[i], dw_hr_name(hr), tf);
    }
}
TEST(tfv_style_valid_and_invalid) {
    static const int ok[] = { 0, 1, 2 }, bad[] = { 3, 4, 255, -1, 0x7fffffff }; int i;
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    for (i = 0; i < 3; i++) { IDWriteTextFormat *tf = NULL; CHECK_HR_OK(mk(f, L"Arial", 400, ok[i], 5, 12.0f, L"en-us", &tf)); if (tf) EXPECT_EQ(IDWriteTextFormat_GetFontStyle(tf), ok[i]); DW_RELEASE(tf); }
    for (i = 0; i < 5; i++) { IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; CHECK_HR(mk(f, L"Arial", 400, bad[i], 5, 12.0f, L"en-us", &tf), E_INVALIDARG); CHECK(tf == NULL); }
}
TEST(tfv_stretch_valid_and_invalid) {
    static const int bad[] = { 0, 10, 11, 255, -1, 0x7fffffff }; int i;
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    for (i = 1; i <= 9; i++) { IDWriteTextFormat *tf = NULL; CHECK_HR_OK(mk(f, L"Arial", 400, 0, i, 12.0f, L"en-us", &tf)); if (tf) EXPECT_EQ(IDWriteTextFormat_GetFontStretch(tf), i); DW_RELEASE(tf); }
    for (i = 0; i < 6; i++) { IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; CHECK_HR(mk(f, L"Arial", 400, 0, bad[i], 12.0f, L"en-us", &tf), E_INVALIDARG); CHECK(tf == NULL); }
}
TEST(tfv_size_valid_values) {
    static const float v[] = { 0.001f, 0.5f, 1.0f, 8.0f, 12.0f, 96.0f, 1000.0f, 100000.0f }; int i;
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    for (i = 0; i < 8; i++) {
        IDWriteTextFormat *tf = NULL; CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, v[i], L"en-us", &tf));
        if (tf) { CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(tf), v[i]); DW_RELEASE(tf); }
    }
}
TEST(tfv_size_invalid_values) {
    union { float f; UINT32 u; } nanv, negzero, neginf; float v[6]; int i;
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    nanv.u = 0x7fc00000; negzero.u = 0x80000000; neginf.u = 0xff800000;
    v[0] = 0.0f; v[1] = negzero.f; v[2] = -1.0f; v[3] = -12.0f; v[4] = nanv.f; v[5] = neginf.f;
    for (i = 0; i < 6; i++) {
        IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; HRESULT hr = mk(f, L"Arial", 400, 0, 5, v[i], L"en-us", &tf);
        CHECKF(hr == E_INVALIDARG && tf == NULL, "size #%d (%g) -> %s", i, (double)v[i], dw_hr_name(hr));
    }
}
TEST(tfv_size_positive_infinity_note) {
    union { float f; UINT32 u; } pinf; IDWriteTextFormat *tf = NULL; HRESULT hr; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    pinf.u = 0x7f800000;
    hr = mk(f, L"Arial", 400, 0, 5, pinf.f, L"en-us", &tf);
    t_note("CreateTextFormat with +infinity size: %s", dw_hr_name(hr));
    DW_RELEASE(tf);
}
TEST(tfv_null_family_invalid) {
    NO_WINE();
    IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    CHECK_HR(mk(f, NULL, 400, 0, 5, 12.0f, L"en-us", &tf), E_INVALIDARG); CHECK(tf == NULL);
}
TEST(tfv_null_locale_invalid) {
    NO_WINE();
    IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    CHECK_HR(mk(f, L"Arial", 400, 0, 5, 12.0f, NULL, &tf), E_INVALIDARG); CHECK(tf == NULL);
}
TEST(tfv_null_family_and_locale_invalid) {
    NO_WINE();
    IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    CHECK_HR(mk(f, NULL, 400, 0, 5, 12.0f, NULL, &tf), E_INVALIDARG); CHECK(tf == NULL);
}
TEST(tfv_failure_does_not_touch_factory_refcount) {
    NO_WINE();
    IDWriteFactory *f = dw_new_factory(FT_ISOLATED); IDWriteTextFormat *tf; int i; REQUIRE(f);
    for (i = 0; i < 10; i++) { tf = NULL; mk(f, NULL, 400, 0, 5, 12.0f, L"en-us", &tf); mk(f, L"Arial", 0, 0, 5, 12.0f, L"en-us", &tf); }
    EXPECT_REFS(f, 1);
    DW_RELEASE(f);
}
TEST(tfv_failure_with_every_other_arg_valid_per_arg_independence) {
    /* each invalid argument alone is enough; the others being valid does not matter */
    struct { int w, st, str; float sz; } t[] = { { 0, 0, 5, 12 }, { 400, 3, 5, 12 }, { 400, 0, 0, 12 }, { 400, 0, 5, 0 }, { 1000, 0, 5, 12 }, { 400, 0, 10, 12 }, { 400, 0, 5, -3 } };
    int i; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    for (i = 0; i < 7; i++) { IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; HRESULT hr = mk(f, L"Arial", t[i].w, t[i].st, t[i].str, t[i].sz, L"en-us", &tf); CHECKF(hr == E_INVALIDARG && !tf, "row %d -> %s", i, dw_hr_name(hr)); }
}
TEST(tfv_empty_family_name_accepted) {
    IDWriteTextFormat *tf = NULL; IDWriteFactory *f = dw_shared(); UINT32 len = 99; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"", 400, 0, 5, 12.0f, L"en-us", &tf)); REQUIRE(tf);
    len = IDWriteTextFormat_GetFontFamilyNameLength(tf);
    EXPECT_EQ(len, 0);
    DW_RELEASE(tf);
}
TEST(tfv_unknown_family_name_accepted_and_kept) {
    IDWriteTextFormat *tf = NULL; IDWriteFactory *f = dw_shared(); WCHAR buf[64]; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"No Such Family 12345", 400, 0, 5, 12.0f, L"en-us", &tf)); REQUIRE(tf);   /* resolved lazily at layout time */
    EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(tf), 20);
    CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(tf, buf, 64));
    EXPECT_WSTR(buf, L"No Such Family 12345");
    DW_RELEASE(tf);
}
TEST(tfv_family_name_buffer_protocol) {
    IDWriteTextFormat *tf = NULL; IDWriteFactory *f = dw_shared(); WCHAR buf[16]; HRESULT hr; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"en-us", &tf)); REQUIRE(tf);
    EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(tf), 5);
    CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(tf, buf, 6)); EXPECT_WSTR(buf, L"Arial");
    hr = IDWriteTextFormat_GetFontFamilyName(tf, buf, 5); CHECK_HR_FAILED(hr);
    hr = IDWriteTextFormat_GetFontFamilyName(tf, buf, 0); CHECK_HR_FAILED(hr);
    t_note("family name buffer too small: %s", dw_hr_name(hr));
    DW_RELEASE(tf);
}
TEST(tfv_null_collection_means_system_collection) {
    IDWriteTextFormat *tf = NULL; IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL, *sys = NULL; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"en-us", &tf)); REQUIRE(tf);
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(tf, &c));
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE));
    CHECK(c != NULL && c == sys);
    DW_RELEASE(c); DW_RELEASE(sys); DW_RELEASE(tf);
}
TEST(tfv_defaults_of_new_format) {
    IDWriteTextFormat *tf = NULL; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"en-us", &tf)); REQUIRE(tf);
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(tf), DWRITE_TEXT_ALIGNMENT_LEADING);
    EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(tf), DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(tf), DWRITE_WORD_WRAPPING_WRAP);
    EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(tf), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(tf), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    t_note("default incremental tab stop %g", (double)IDWriteTextFormat_GetIncrementalTabStop(tf));
    DW_RELEASE(tf);
}
TEST(tfv_every_factory_version_pointer_creates_format) {
    int v;
    for (v = 0; v <= 7; v++) {
        IDWriteFactory *f = (IDWriteFactory *)dw_factory_v(FT_SHARED, v); IDWriteTextFormat *tf = NULL;
        if (!f) continue;
        CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"en-us", &tf));
        CHECK(tf != NULL); DW_RELEASE(tf);
    }
}

/* ---- locale handling ----------------------------------------------------------------------------------------- */
static void locale_of(IDWriteTextFormat *tf, WCHAR *out, int cap, UINT32 *len) {
    *len = IDWriteTextFormat_GetLocaleNameLength(tf);
    out[0] = 0;
    if ((int)*len + 1 <= cap) IDWriteTextFormat_GetLocaleName(tf, out, *len + 1);
}
TEST(loc_empty_locale_accepted) {
    IDWriteTextFormat *tf = (IDWriteTextFormat *)RFP_SENTINEL; IDWriteFactory *f = dw_shared(); WCHAR b[100]; UINT32 len; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"", &tf)); REQUIRE(tf && tf != RFP_SENTINEL);
    locale_of(tf, b, 100, &len);
    t_note("format created with L\"\" locale reports locale '%ls' (length %u)", b, len);
    CHECK(lstrlenW(b) == (int)len);
    DW_RELEASE(tf);
}
TEST(loc_empty_locale_name_round_trip_consistent) {
    IDWriteTextFormat *tf = NULL; IDWriteFactory *f = dw_shared(); WCHAR b[100]; UINT32 len; HRESULT hr; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"", &tf)); REQUIRE(tf);
    len = IDWriteTextFormat_GetLocaleNameLength(tf);
    hr = IDWriteTextFormat_GetLocaleName(tf, b, len + 1);
    CHECK_HR(hr, S_OK);
    EXPECT_EQ(lstrlenW(b), len);
    DW_RELEASE(tf);
}
TEST(loc_empty_locale_layout_inherits_format_locale) {
    IDWriteTextFormat *tf = NULL; IDWriteTextLayout *l = NULL; IDWriteFactory *f = dw_shared(); WCHAR fb[100], lb[100]; UINT32 flen; DWRITE_TEXT_RANGE r = { 7, 7 }; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"", &tf)); REQUIRE(tf);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc def", 7, tf, 100.0f, 100.0f, &l)); REQUIRE(l);
    locale_of(tf, fb, 100, &flen);
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 0, lb, 100, &r));
    CHECK(!lstrcmpW(fb, lb));
    EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 7);
    DW_RELEASE(l); DW_RELEASE(tf);
}
TEST(loc_explicit_locale_round_trip) {
    static const WCHAR *const names[] = { L"en-us", L"en-US", L"de-DE", L"ja-JP", L"ar-SA", L"fr", L"zh-Hans-CN", L"x-unknown-locale" }; int i;
    IDWriteFactory *f = dw_shared(); REQUIRE(f);
    for (i = 0; i < 8; i++) {
        IDWriteTextFormat *tf = NULL; WCHAR b[100]; UINT32 len;
        CHECKF(mk(f, L"Arial", 400, 0, 5, 12.0f, names[i], &tf) == S_OK, "locale #%d", i); if (!tf) continue;
        locale_of(tf, b, 100, &len);
        CHECKF(len == (UINT32)lstrlenW(names[i]), "locale %ls length %u", names[i], len);
        CHECKF(!lstrcmpiW(b, names[i]), "locale %ls reported as %ls", names[i], b);
        if (lstrcmpW(b, names[i])) t_note("locale '%ls' reported as '%ls' (case differs)", names[i], b);
        DW_RELEASE(tf);
    }
}
TEST(loc_locale_name_buffer_protocol) {
    IDWriteTextFormat *tf = NULL; IDWriteFactory *f = dw_shared(); WCHAR b[16]; HRESULT hr; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"en-us", &tf)); REQUIRE(tf);
    EXPECT_EQ(IDWriteTextFormat_GetLocaleNameLength(tf), 5);
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(tf, b, 6));
    hr = IDWriteTextFormat_GetLocaleName(tf, b, 0);
    CHECK_HR(hr, HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));   /* GetLocaleNameCopy: size 0 -> 0x8007007A */
    hr = IDWriteTextFormat_GetLocaleName(tf, b, 5);
    CHECK_HR(hr, HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));   /* StringCchCopyW: exactly length (no room for NUL) */
    hr = IDWriteTextFormat_GetLocaleName(tf, b, 1);
    CHECK_HR(hr, HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER));
    DW_RELEASE(tf);
}
TEST(loc_locale_name_exact_size_ok_for_sizes_up_to_16) {
    IDWriteTextFormat *tf = NULL; IDWriteFactory *f = dw_shared(); WCHAR b[40]; int n; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"zh-Hans-CN", &tf)); REQUIRE(tf);
    for (n = 0; n <= 20; n++) {
        HRESULT hr = IDWriteTextFormat_GetLocaleName(tf, b, n);
        if (n >= 11) { CHECKF(hr == S_OK, "size %d -> %s", n, dw_hr_name(hr)); } else { CHECKF(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "size %d -> %s", n, dw_hr_name(hr)); }
    }
    DW_RELEASE(tf);
}
TEST(loc_set_locale_not_part_of_text_format_api) {
    /* a format's locale is fixed at creation: two formats from one factory with different locales keep their own */
    IDWriteTextFormat *a = NULL, *b = NULL; IDWriteFactory *f = dw_shared(); WCHAR ba[40], bb[40]; UINT32 la, lb; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"en-us", &a)); CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"de-de", &b));
    REQUIRE(a && b);
    locale_of(a, ba, 40, &la); locale_of(b, bb, 40, &lb);
    CHECK(lstrcmpiW(ba, bb) != 0);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(loc_layout_range_locale_override_keeps_format_locale) {
    IDWriteTextFormat *tf = NULL; IDWriteTextLayout *l = NULL; IDWriteFactory *f = dw_shared(); WCHAR b[40]; DWRITE_TEXT_RANGE r, set = { 2, 3 }; REQUIRE(f);
    CHECK_HR_OK(mk(f, L"Arial", 400, 0, 5, 12.0f, L"en-us", &tf)); REQUIRE(tf);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abcdefg", 7, tf, 200.0f, 50.0f, &l)); REQUIRE(l);
    CHECK_HR_OK(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", set));
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 0, b, 40, &r)); CHECK(!lstrcmpiW(b, L"en-us")); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 2);
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 2, b, 40, &r)); CHECK(!lstrcmpiW(b, L"fr-fr")); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 3);
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 5, b, 40, &r)); CHECK(!lstrcmpiW(b, L"en-us")); EXPECT_EQ(r.startPosition, 5); EXPECT_EQ(r.length, 2);
    DW_RELEASE(l); DW_RELEASE(tf);
}
TEST(loc_user_default_locale_name_is_non_empty) {
    WCHAR b[LOCALE_NAME_MAX_LENGTH];
    int n = GetUserDefaultLocaleName(b, LOCALE_NAME_MAX_LENGTH);
    CHECK(n > 1);
    t_note("user default locale %ls", b);
}

/* ---- CreateTextLayout ------------------------------------------------------------------------------------------ */
TEST(tl_null_string_invalid) {
    IDWriteTextFormat *tf; IDWriteTextLayout *l = (IDWriteTextLayout *)RFP_SENTINEL; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    tf = rfp_fmt(f, 12.0f, L"en-us"); REQUIRE(tf);
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, NULL, 0, tf, 100.0f, 100.0f, &l), E_INVALIDARG); CHECK(l == NULL);
    l = (IDWriteTextLayout *)RFP_SENTINEL;
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, NULL, 5, tf, 100.0f, 100.0f, &l), E_INVALIDARG); CHECK(l == NULL);
    DW_RELEASE(tf);
}
TEST(tl_null_format_invalid) {
    IDWriteTextLayout *l = (IDWriteTextLayout *)RFP_SENTINEL; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, NULL, 100.0f, 100.0f, &l), E_INVALIDARG); CHECK(l == NULL);
}
TEST(tl_negative_and_nan_extents_invalid) {
    union { float f; UINT32 u; } nanv; float bad[4]; int i, j; IDWriteTextFormat *tf; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    nanv.u = 0x7fc00000; bad[0] = -1.0f; bad[1] = -0.001f; bad[2] = nanv.f; bad[3] = -1e30f;
    tf = rfp_fmt(f, 12.0f, L"en-us"); REQUIRE(tf);
    for (i = 0; i < 4; i++) for (j = 0; j < 2; j++) {
        IDWriteTextLayout *l = (IDWriteTextLayout *)RFP_SENTINEL;
        HRESULT hr = IDWriteFactory_CreateTextLayout(f, L"abc", 3, tf, j ? 100.0f : bad[i], j ? bad[i] : 100.0f, &l);
        CHECKF(hr == E_INVALIDARG && l == NULL, "extent #%d (%s) -> %s", i, j ? "height" : "width", dw_hr_name(hr));
    }
    DW_RELEASE(tf);
}
TEST(tl_zero_and_large_extents_valid) {
    static const float v[] = { 0.0f, 0.5f, 1.0f, 100.0f, 1e6f }; int i; IDWriteTextFormat *tf; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    tf = rfp_fmt(f, 12.0f, L"en-us"); REQUIRE(tf);
    for (i = 0; i < 5; i++) {
        IDWriteTextLayout *l = NULL; CHECKF(IDWriteFactory_CreateTextLayout(f, L"abc", 3, tf, v[i], v[i], &l) == S_OK && l, "extent %g", (double)v[i]);
        if (l) { CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), v[i]); CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), v[i]); }
        DW_RELEASE(l);
    }
    DW_RELEASE(tf);
}
TEST(tl_zero_length_string_valid) {
    IDWriteTextFormat *tf; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    tf = rfp_fmt(f, 12.0f, L"en-us"); REQUIRE(tf);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"", 0, tf, 100.0f, 100.0f, &l)); REQUIRE(l);
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    EXPECT_EQ(m.lineCount, 1);   /* an empty layout still has one (empty) line */
    CHECK_FLOAT_EQ(m.width, 0.0f);
    DW_RELEASE(l); DW_RELEASE(tf);
}
TEST(tl_length_shorter_than_buffer_uses_prefix) {
    IDWriteTextFormat *tf; IDWriteTextLayout *l = NULL; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    tf = rfp_fmt(f, 12.0f, L"en-us"); REQUIRE(tf);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abcdefgh", 3, tf, 100.0f, 100.0f, &l)); REQUIRE(l);
    {   DWRITE_TEXT_RANGE r = { 0, 8 }; WCHAR b[16]; HRESULT hr = IDWriteTextLayout_GetLocaleName(l, 2, b, 16, &r); CHECK_HR_OK(hr); EXPECT_EQ(r.length, 3); }
    DW_RELEASE(l); DW_RELEASE(tf);
}
TEST(tl_layout_keeps_format_from_other_factory_working) {
    /* a layout is created from the receiving factory; the format may come from another factory of the same type */
    IDWriteFactory *a = dw_new_factory(FT_ISOLATED), *b = dw_new_factory(FT_ISOLATED); IDWriteTextFormat *tf; IDWriteTextLayout *l = NULL; HRESULT hr;
    if (!a || !b) { t_skip("no factory"); DW_RELEASE(a); DW_RELEASE(b); return; }
    tf = rfp_fmt(a, 12.0f, L"en-us"); REQUIRE(tf);
    hr = IDWriteFactory_CreateTextLayout(b, L"abc", 3, tf, 100.0f, 100.0f, &l);
    t_note("CreateTextLayout with a format from another factory: %s", dw_hr_name(hr));
    DW_RELEASE(l); DW_RELEASE(tf); DW_RELEASE(a); DW_RELEASE(b);
}
TEST(tl_layout_text_with_embedded_nul) {
    static const WCHAR t[] = { 'a', 0, 'b' }; IDWriteTextFormat *tf; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m; IDWriteFactory *f = dw_shared(); REQUIRE(f);
    tf = rfp_fmt(f, 12.0f, L"en-us"); REQUIRE(tf);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, t, 3, tf, 1000.0f, 100.0f, &l)); REQUIRE(l);
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    EXPECT_EQ(m.lineCount, 1);
    EXPECT_EQ(m.lineCount, 1);   /* the length argument decides, not a terminator */
    DW_RELEASE(l); DW_RELEASE(tf);
}

/* ---- CreateNumberSubstitution ------------------------------------------------------------------------------------- */
TEST(ns_null_locale_only_valid_for_method_none) {
    IDWriteFactory *f = dw_shared(); int m; REQUIRE(f);
    for (m = 0; m <= 4; m++) {
        IDWriteNumberSubstitution *s = (IDWriteNumberSubstitution *)RFP_SENTINEL; HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, NULL, FALSE, &s);
        if (m == DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE) { CHECK_HR_OK(hr); CHECK(s != NULL && s != RFP_SENTINEL); DW_RELEASE(s); }
        else { CHECKF(hr == E_INVALIDARG && s == NULL, "method %d with NULL locale -> %s %p", m, dw_hr_name(hr), s); }
    }
}
TEST(ns_valid_methods_with_locale) {
    IDWriteFactory *f = dw_shared(); int m; REQUIRE(f);
    for (m = 0; m <= 4; m++) {
        IDWriteNumberSubstitution *s = NULL; CHECKF(IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, L"ar-SA", FALSE, &s) == S_OK && s, "method %d", m);
        if (s) EXPECT_REFS(s, 1);
        DW_RELEASE(s);
    }
}
TEST(ns_ignore_user_override_flag_accepted) {
    IDWriteFactory *f = dw_shared(); int u; REQUIRE(f);
    for (u = 0; u < 2; u++) { IDWriteNumberSubstitution *s = NULL; CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE, L"en-us", u, &s)); CHECK(s != NULL); DW_RELEASE(s); }
}
TEST(ns_empty_locale_string_not_rejected) {
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *s = NULL; HRESULT hr; REQUIRE(f);
    hr = IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL, L"", FALSE, &s);
    CHECK_HR_OK(hr);   /* only a NULL pointer is rejected */
    DW_RELEASE(s);
}
TEST(ns_out_of_range_method_note) {
    IDWriteFactory *f = dw_shared(); static const int m[] = { 5, 6, 100, -1, 0x7fffffff }; int i; REQUIRE(f);
    for (i = 0; i < 5; i++) {
        IDWriteNumberSubstitution *s = NULL; HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m[i], L"en-us", FALSE, &s);
        t_note("CreateNumberSubstitution(method %d): %s", m[i], dw_hr_name(hr));
        DW_RELEASE(s);
    }
}
TEST(ns_distinct_objects_each_call) {
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *a = NULL, *b = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, NULL, FALSE, &a));
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, NULL, FALSE, &b));
    CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(ns_qi_matrix) {
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *s = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, NULL, FALSE, &s)); REQUIRE(s);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteNumberSubstitution, 1), DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteTextFormat, 0) };
    DW_CHECK_QI(s, rows);
    DW_RELEASE(s);
}
TEST(ns_outlives_isolated_factory) {
    IDWriteFactory *f = dw_new_factory(FT_ISOLATED); IDWriteNumberSubstitution *s = NULL; REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL, L"ar-SA", TRUE, &s));
    DW_RELEASE(f);
    REQUIRE(s);
    EXPECT_REFS(s, 1);
    DW_RELEASE(s);
}
