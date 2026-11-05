/* IDWriteFactory::CreateTextFormat: argument validation, defaults of the new object, family/locale/weight/style/stretch/size
 * round trips and the string buffer protocol of the getters.
 *
 * Validation is performed by the factory before any object exists: family and locale non-NULL, weight 1..999, style 0..2,
 * stretch 1..9, size > 0 and not NaN. *format is zeroed first, so a failing call leaves NULL behind. The family name is stored
 * verbatim; the locale name is syntax-checked and lower-cased (language tag normalisation) when the object is built. */
#include "tf.h"

/* ---- argument validation --------------------------------------------------------------------------- */
#define SENTINEL ((IDWriteTextFormat *)(ULONG_PTR)0x1)

static HRESULT mk(IDWriteFactory *f, const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE s, DWRITE_FONT_STRETCH st, FLOAT sz, const WCHAR *loc, IDWriteTextFormat **out)
{
    *out = SENTINEL;
    return IDWriteFactory_CreateTextFormat(f, fam, NULL, w, s, st, sz, loc, out);
}

TEST(create_basic_ok)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = SENTINEL;
    CHECK_HR_OK(mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &t));
    CHECK(t != NULL && t != SENTINEL);
    if (t && t != SENTINEL) { EXPECT_REFS(t, 1); DW_RELEASE(t); }
}

TEST(create_weight_valid_every_value)
{
    TF_NEED_FACTORY();
    UINT32 w, bad = 0;
    for (w = 1; w <= 999; w++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", (DWRITE_FONT_WEIGHT)w, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t);
        if (hr != S_OK || !t || t == SENTINEL) { bad++; if (bad < 5) t_check(0, __FILE__, __LINE__, "weight accepted", "weight %u -> %s", w, dw_hr_name(hr)); }
        else { if (IDWriteTextFormat_GetFontWeight(t) != (DWRITE_FONT_WEIGHT)w) { bad++; t_check(0, __FILE__, __LINE__, "weight round trip", "weight %u", w); } IDWriteTextFormat_Release(t); }
    }
    CHECK(bad == 0);
}

TEST(create_weight_invalid)
{
    TF_NEED_FACTORY();
    static const UINT32 bad[] = { 0, 1000, 1001, 1500, 4096, 65535, 65536, 0x7fffffff, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", (DWRITE_FONT_WEIGHT)bad[i], DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t);
        CHECKF(hr == E_INVALIDARG, "weight %u -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(t);
        if (t && t != SENTINEL) IDWriteTextFormat_Release(t);
    }
}

TEST(create_weight_boundaries)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = SENTINEL;
    /* 1 is the lowest and 999 the highest accepted weight (valid range is the unsigned test weight-1 <= 998). */
    CHECK_HR_OK(mk(f, L"Arial", (DWRITE_FONT_WEIGHT)1, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t)); DW_RELEASE(t);
    CHECK_HR_OK(mk(f, L"Arial", (DWRITE_FONT_WEIGHT)999, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t)); DW_RELEASE(t);
    CHECK_HR(mk(f, L"Arial", (DWRITE_FONT_WEIGHT)0, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
    CHECK_HR(mk(f, L"Arial", (DWRITE_FONT_WEIGHT)1000, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
}

TEST(create_named_weights_roundtrip)
{
    TF_NEED_FACTORY();
    static const UINT32 num[] = { 100, 200, 300, 350, 400, 500, 600, 700, 800, 900, 950 };
    size_t i;
    CHECK(DWRITE_FONT_WEIGHT_THIN == 100 && DWRITE_FONT_WEIGHT_SEMI_LIGHT == 350 && DWRITE_FONT_WEIGHT_BOLD == 700 && DWRITE_FONT_WEIGHT_EXTRA_BLACK == 950);
    for (i = 0; i < sizeof num / sizeof num[0]; i++) {
        IDWriteTextFormat *t = tf_make(f, L"Arial", (DWRITE_FONT_WEIGHT)num[i], DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
        EXPECT_EQ(IDWriteTextFormat_GetFontWeight(t), num[i]);
        DW_RELEASE(t);
    }
}

TEST(create_style_valid)
{
    TF_NEED_FACTORY();
    int s;
    for (s = 0; s <= 2; s++) {
        IDWriteTextFormat *t = SENTINEL;
        CHECK_HR_OK(mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, (DWRITE_FONT_STYLE)s, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t));
        if (t && t != SENTINEL) { EXPECT_EQ(IDWriteTextFormat_GetFontStyle(t), s); IDWriteTextFormat_Release(t); }
    }
}

TEST(create_style_invalid)
{
    TF_NEED_FACTORY();
    static const UINT32 bad[] = { 3, 4, 5, 255, 256, 0x7fffffff, 0x80000000u, 0xffffffffu };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, (DWRITE_FONT_STYLE)bad[i], DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t);
        CHECKF(hr == E_INVALIDARG, "style %u -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(t);
        if (t && t != SENTINEL) IDWriteTextFormat_Release(t);
    }
}

TEST(create_stretch_valid)
{
    TF_NEED_FACTORY();
    int s;
    for (s = 1; s <= 9; s++) {
        IDWriteTextFormat *t = SENTINEL;
        CHECK_HR_OK(mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, (DWRITE_FONT_STRETCH)s, 10.0f, L"en-us", &t));
        if (t && t != SENTINEL) { EXPECT_EQ(IDWriteTextFormat_GetFontStretch(t), s); IDWriteTextFormat_Release(t); }
    }
}

TEST(create_stretch_invalid)
{
    TF_NEED_FACTORY();
    /* DWRITE_FONT_STRETCH_UNDEFINED (0) is rejected as well as everything above ULTRA_EXPANDED (9). */
    static const UINT32 bad[] = { 0, 10, 11, 100, 255, 0x7fffffff, 0xffffffffu };
    size_t i;
    CHECK(DWRITE_FONT_STRETCH_UNDEFINED == 0 && DWRITE_FONT_STRETCH_ULTRA_EXPANDED == 9 && DWRITE_FONT_STRETCH_NORMAL == 5);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, (DWRITE_FONT_STRETCH)bad[i], 10.0f, L"en-us", &t);
        CHECKF(hr == E_INVALIDARG, "stretch %u -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(t);
        if (t && t != SENTINEL) IDWriteTextFormat_Release(t);
    }
}

TEST(create_style_stretch_weight_all_combinations)
{
    TF_NEED_FACTORY();
    int s, st, bad = 0;
    for (s = 0; s <= 2; s++) for (st = 1; st <= 9; st++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", (DWRITE_FONT_WEIGHT)(100 * (st + 0)), (DWRITE_FONT_STYLE)s, (DWRITE_FONT_STRETCH)st, 8.0f + st, L"en-us", &t);
        if (hr != S_OK || !t || t == SENTINEL) { bad++; continue; }
        if (IDWriteTextFormat_GetFontStyle(t) != (DWRITE_FONT_STYLE)s || IDWriteTextFormat_GetFontStretch(t) != (DWRITE_FONT_STRETCH)st ||
            IDWriteTextFormat_GetFontWeight(t) != (DWRITE_FONT_WEIGHT)(100 * st) || IDWriteTextFormat_GetFontSize(t) != 8.0f + st) bad++;
        IDWriteTextFormat_Release(t);
    }
    CHECK(bad == 0);
}

TEST(create_size_valid)
{
    TF_NEED_FACTORY();
    static const float sz[] = { 0.001f, 0.5f, 1.0f, 8.0f, 11.25f, 12.0f, 16.0f, 72.0f, 100.5f, 1000.0f, 32767.0f, 1e6f };
    size_t i;
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz[i], L"en-us", &t);
        CHECKF(hr == S_OK, "size %g -> %s", sz[i], dw_hr_name(hr));
        if (hr == S_OK && t && t != SENTINEL) { CHECK_FBITS(IDWriteTextFormat_GetFontSize(t), sz[i]); IDWriteTextFormat_Release(t); }
    }
}

TEST(create_size_extremes_accepted)
{
    TF_NEED_FACTORY();
    /* Only "size <= 0" and NaN are rejected; huge finite values and +infinity pass the check. */
    static const float sz[] = { FLT_MIN, FLT_MAX, INFINITY };
    size_t i;
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz[i], L"en-us", &t);
        CHECKF(hr == S_OK, "size %g -> %s", sz[i], dw_hr_name(hr));
        if (hr == S_OK && t && t != SENTINEL) { CHECK_FBITS(IDWriteTextFormat_GetFontSize(t), sz[i]); t_note("size %g: default tab stop %g", sz[i], IDWriteTextFormat_GetIncrementalTabStop(t)); IDWriteTextFormat_Release(t); }
        else DW_RELEASE(t);
    }
}

TEST(create_size_invalid)
{
    TF_NEED_FACTORY();
    static const float sz[] = { 0.0f, -0.0f, -1.0f, -0.001f, -16.0f, -FLT_MIN, -FLT_MAX, -INFINITY, NAN, -NAN };
    size_t i;
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz[i], L"en-us", &t);
        CHECKF(hr == E_INVALIDARG, "size #%u (%g) -> %s", (unsigned)i, sz[i], dw_hr_name(hr));
        CHECK_OUT_NULL(t);
        if (t && t != SENTINEL) IDWriteTextFormat_Release(t);
    }
}

TEST(create_null_family)
{
    TF_NEED_FACTORY();
    if (t_is_wine()) { t_skip("NULL string argument faults in the emulated implementation"); return; }
    IDWriteTextFormat *t = SENTINEL;
    CHECK_HR(mk(f, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &t), E_INVALIDARG);
    CHECK_OUT_NULL(t);
}

TEST(create_null_locale)
{
    TF_NEED_FACTORY();
    if (t_is_wine()) { t_skip("NULL string argument faults in the emulated implementation"); return; }
    IDWriteTextFormat *t = SENTINEL;
    CHECK_HR(mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, NULL, &t), E_INVALIDARG);
    CHECK_OUT_NULL(t);
}

TEST(create_null_family_and_locale)
{
    TF_NEED_FACTORY();
    if (t_is_wine()) { t_skip("NULL string argument faults in the emulated implementation"); return; }
    IDWriteTextFormat *t = SENTINEL;
    CHECK_HR(mk(f, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, NULL, &t), E_INVALIDARG);
    CHECK_OUT_NULL(t);
}

TEST(create_every_failure_zeroes_out_and_leaks_nothing)
{
    TF_NEED_FACTORY();
    struct { const WCHAR *fam; UINT32 w, s, st; float sz; const WCHAR *loc; const char *why; } c[] = {
        { NULL, 400, 0, 5, 10, L"en-us", "family" }, { L"A", 400, 0, 5, 10, NULL, "locale" }, { L"A", 0, 0, 5, 10, L"en-us", "weight0" },
        { L"A", 1000, 0, 5, 10, L"en-us", "weight1000" }, { L"A", 400, 3, 5, 10, L"en-us", "style" }, { L"A", 400, 0, 0, 10, L"en-us", "stretch0" },
        { L"A", 400, 0, 10, 10, L"en-us", "stretch10" }, { L"A", 400, 0, 5, 0, L"en-us", "size0" }, { L"A", 400, 0, 5, -3, L"en-us", "sizeneg" },
    };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        IDWriteTextFormat *t = (IDWriteTextFormat *)(ULONG_PTR)0xdeadbeef;
        if (t_is_wine() && (!c[i].fam || !c[i].loc)) continue;     /* NULL strings fault in the emulated implementation */
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, c[i].fam, NULL, (DWRITE_FONT_WEIGHT)c[i].w, (DWRITE_FONT_STYLE)c[i].s, (DWRITE_FONT_STRETCH)c[i].st, c[i].sz, c[i].loc, &t);
        CHECKF(FAILED(hr), "%s: %s", c[i].why, dw_hr_name(hr));
        CHECKF(t == NULL, "%s: out parameter not zeroed (%p)", c[i].why, (void *)t);
    }
}

TEST(create_validation_order_all_invalid)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = SENTINEL;
    /* Several arguments invalid at once: still a single E_INVALIDARG and a NULL out parameter. */
    CHECK_HR(mk(f, L"x", (DWRITE_FONT_WEIGHT)0, (DWRITE_FONT_STYLE)9, (DWRITE_FONT_STRETCH)0, -1.0f, L"en-us", &t), E_INVALIDARG);
    CHECK_OUT_NULL(t);
    if (!t_is_wine()) { t = SENTINEL; CHECK_HR(mk(f, NULL, (DWRITE_FONT_WEIGHT)0, (DWRITE_FONT_STYLE)9, (DWRITE_FONT_STRETCH)0, -1.0f, NULL, &t), E_INVALIDARG); CHECK_OUT_NULL(t); }
}

/* ---- family name --------------------------------------------------------------------------------------- */
TEST(create_family_roundtrip)
{
    TF_NEED_FACTORY();
    static const WCHAR *names[] = { L"Arial", L"Times New Roman", L"Segoe UI", L"a", L"No Such Family 12345", L"  padded  ", L"with.dots-and_underscores", L"Case SenSitive" };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        IDWriteTextFormat *t = tf_make(f, names[i], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
        WCHAR buf[128];
        EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(t), wcslen(names[i]));
        CHECK_HR_OK(tf_family(t, buf, 128));
        CHECK_WEQ(buf, names[i]);
        DW_RELEASE(t);
    }
}

TEST(create_family_empty)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_make(f, L"", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
    WCHAR buf[4] = { 0x1111, 0x2222, 0x3333, 0x4444 };
    EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(t), 0);
    CHECK_HR_OK(tf_family(t, buf, 1)); EXPECT_EQ(buf[0], 0); EXPECT_EQ(buf[1], 0x2222);
    buf[0] = 0x1111;
    CHECK_HR(tf_family(t, buf, 0), DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(buf[0], 0x1111);
    DW_RELEASE(t);
}

TEST(create_family_long_names)
{
    TF_NEED_FACTORY();
    /* No length limit is applied to the family name (it is not looked up at creation time). */
    static const int lens[] = { 31, 32, 33, 64, 255, 256, 257, 1000, 4000 };
    size_t i;
    for (i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        int n = lens[i], k;
        WCHAR *nm = (WCHAR *)malloc((n + 1) * sizeof(WCHAR)), *out = (WCHAR *)malloc((n + 2) * sizeof(WCHAR));
        IDWriteTextFormat *t;
        for (k = 0; k < n; k++) nm[k] = (WCHAR)(L'A' + (k % 26));
        nm[n] = 0;
        t = tf_make(f, nm, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
        EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(t), n);
        CHECK_HR_OK(tf_family(t, out, n + 1)); CHECK_WEQ(out, nm);
        CHECK_HR(tf_family(t, out, n), DW_E_NOT_SUFFICIENT_BUFFER);
        DW_RELEASE(t); free(nm); free(out);
    }
}

TEST(create_family_unicode_preserved)
{
    TF_NEED_FACTORY();
    static const WCHAR n1[] = { 0x65E5, 0x672C, 0x8A9E, 0 };                 /* CJK */
    static const WCHAR n2[] = { 0x00C4, 0x00D6, 0x00DC, 0x00DF, 0 };         /* Latin-1 supplement */
    static const WCHAR n3[] = { 0xD83D, 0xDE00, L'x', 0 };                   /* surrogate pair (U+1F600) */
    static const WCHAR n4[] = { 0x0627, 0x0644, 0x0639, 0x0631, 0x0628, 0x064A, 0x0629, 0 };   /* Arabic */
    static const WCHAR n5[] = { L'a', 0x200B, 0x00A0, L'b', 0 };             /* zero-width space, NBSP */
    const WCHAR *names[] = { n1, n2, n3, n4, n5 };
    size_t i;
    for (i = 0; i < 5; i++) {
        IDWriteTextFormat *t = tf_make(f, names[i], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
        WCHAR buf[16];
        EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(t), wcslen(names[i]));
        CHECK_HR_OK(tf_family(t, buf, 16)); CHECK_WEQ(buf, names[i]);
        DW_RELEASE(t);
    }
}

TEST(create_family_case_and_spaces_not_normalised)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_make(f, L"aRiAl  BLACK ", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
    WCHAR buf[32];
    CHECK_HR_OK(tf_family(t, buf, 32)); CHECK_WEQ(buf, L"aRiAl  BLACK ");
    DW_RELEASE(t);
}

TEST(create_family_copied_not_referenced)
{
    TF_NEED_FACTORY();
    WCHAR src[16] = L"Original"; WCHAR loc[16] = L"en-us"; WCHAR buf[16];
    IDWriteTextFormat *t = tf_make(f, src, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, loc);
    wcscpy(src, L"Clobbered!"); wcscpy(loc, L"xx-yy");
    CHECK_HR_OK(tf_family(t, buf, 16)); CHECK_WEQ(buf, L"Original");
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(t, buf, 16)); CHECK_WEQ(buf, L"en-us");
    DW_RELEASE(t);
}

TEST(family_name_buffer_protocol)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
    WCHAR buf[16]; UINT32 cap, i;
    EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(t), 5);
    /* capacity 0: nothing is written; 1..5: truncated copy, always NUL terminated, ERROR_INSUFFICIENT_BUFFER; >= 6: success. */
    for (i = 0; i < 16; i++) buf[i] = 0x7777;
    CHECK_HR(tf_family(t, buf, 0), DW_E_NOT_SUFFICIENT_BUFFER);
    for (i = 0; i < 16; i++) CHECK(buf[i] == 0x7777);
    for (cap = 1; cap <= 5; cap++) {
        for (i = 0; i < 16; i++) buf[i] = 0x7777;
        CHECK_HR(tf_family(t, buf, cap), DW_E_NOT_SUFFICIENT_BUFFER);
        CHECKF(buf[cap - 1] == 0, "capacity %u: not NUL terminated", cap);
        for (i = 0; i + 1 < cap; i++) CHECKF(buf[i] == L"Arial"[i], "capacity %u: char %u = %04x", cap, i, buf[i]);
        CHECKF(buf[cap] == 0x7777, "capacity %u: wrote past the buffer (%04x)", cap, buf[cap]);
    }
    for (cap = 6; cap <= 10; cap++) {
        for (i = 0; i < 16; i++) buf[i] = 0x7777;
        CHECK_HR_OK(tf_family(t, buf, cap));
        CHECK_WEQ(buf, L"Arial");
        CHECKF(buf[6] == 0x7777, "capacity %u: wrote past the string terminator", cap);
    }
    DW_RELEASE(t);
}

TEST(family_name_length_one_name)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_make(f, L"Q", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
    WCHAR b[3] = { 0x5555, 0x5555, 0x5555 };
    EXPECT_EQ(IDWriteTextFormat_GetFontFamilyNameLength(t), 1);
    CHECK_HR(tf_family(t, b, 1), DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(b[0], 0); EXPECT_EQ(b[1], 0x5555);
    b[0] = b[1] = 0x5555;
    CHECK_HR_OK(tf_family(t, b, 2)); EXPECT_EQ(b[0], L'Q'); EXPECT_EQ(b[1], 0); EXPECT_EQ(b[2], 0x5555);
    DW_RELEASE(t);
}

/* ---- locale name ---------------------------------------------------------------------------------------- */
static const struct { const WCHAR *in, *out; } loc_ok[] = {
    { L"en-us", L"en-us" }, { L"en-US", L"en-us" }, { L"EN-US", L"en-us" }, { L"En-Us", L"en-us" }, { L"en", L"en" }, { L"EN", L"en" },
    { L"fr-FR", L"fr-fr" }, { L"de-DE-1996", L"de-de-1996" }, { L"zh-Hans-CN", L"zh-hans-cn" }, { L"zh-Hant", L"zh-hant" }, { L"es-419", L"es-419" },
    { L"sr-Latn-RS", L"sr-latn-rs" }, { L"x-private", L"x-private" }, { L"a", L"a" }, { L"abcdefgh", L"abcdefgh" }, { L"en-abcdefgh", L"en-abcdefgh" },
    { L"ja-JP", L"ja-jp" }, { L"ar-SA", L"ar-sa" }, { L"he-IL", L"he-il" }, { L"en-us-u-co-phonebk", L"en-us-u-co-phonebk" }, { L"en-GB-oed", L"en-gb-oed" },
    { L"qps-ploc", L"qps-ploc" }, { L"zz", L"zz" }, { L"en_US", L"en_us" }, { L"EN_us_X", L"en_us_x" }, { L"i-klingon", L"i-klingon" },
};

TEST(create_locale_normalised_roundtrip)
{
    TF_NEED_FACTORY();
    size_t i;
    for (i = 0; i < sizeof loc_ok / sizeof loc_ok[0]; i++) {
        IDWriteTextFormat *t = SENTINEL; WCHAR buf[64];
        HRESULT hr = mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, loc_ok[i].in, &t);
        CHECKF(hr == S_OK, "locale \"%ls\" -> %s", loc_ok[i].in, dw_hr_name(hr));
        if (hr != S_OK || !t || t == SENTINEL) { DW_RELEASE(t); continue; }
        EXPECT_EQ(IDWriteTextFormat_GetLocaleNameLength(t), wcslen(loc_ok[i].out));
        CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(t, buf, 64));
        CHECKF(!wcscmp(buf, loc_ok[i].out), "locale \"%ls\" read back as \"%ls\" want \"%ls\"", loc_ok[i].in, buf, loc_ok[i].out);
        IDWriteTextFormat_Release(t);
    }
}

TEST(create_locale_empty_allowed)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"");
    WCHAR buf[4] = { 0x4141, 0x4141, 0x4141, 0x4141 };
    /* An empty tag is not validated; it is kept as the empty string. */
    EXPECT_EQ(IDWriteTextFormat_GetLocaleNameLength(t), 0);
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(t, buf, 1)); EXPECT_EQ(buf[0], 0); EXPECT_EQ(buf[1], 0x4141);
    CHECK_HR(IDWriteTextFormat_GetLocaleName(t, buf, 0), DW_E_NOT_SUFFICIENT_BUFFER);
    DW_RELEASE(t);
}

TEST(create_locale_invalid_syntax_rejected)
{
    TF_NEED_FACTORY();
    /* The language tag grammar is enforced while the object is built: primary subtag 1-8 letters, further subtags 1-8
     * letters/digits separated by '-' or '_', nothing else. The failure code is recorded, the failure itself and the zeroed
     * out parameter are required. */
    static const WCHAR u1[] = { 0x00E9, 0 }, u2[] = { L'e', L'n', L'-', 0x00FC, 0 };
    static const WCHAR *bad[] = { L"e1", L"123", L"en-", L"-en", L"en--us", L"abcdefghi", L"en-abcdefghi", L"en us", L"en-us ", L" en-us", L"en.us", L"en-us!", u1, u2,
                                  L"en-u$", L"1en", L"-", L"_", L"en_", L"a-b-" };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteTextFormat *t = SENTINEL;
        HRESULT hr = mk(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, bad[i], &t);
        CHECKF(FAILED(hr), "locale \"%ls\" accepted (%s)", bad[i], dw_hr_name(hr));
        CHECKF(t == NULL, "locale \"%ls\": out not zeroed", bad[i]);
        t_note("invalid locale \"%ls\" -> %s", bad[i], dw_hr_name(hr));
        if (t && t != SENTINEL) IDWriteTextFormat_Release(t);
    }
}

TEST(locale_name_buffer_protocol)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
    WCHAR buf[16]; UINT32 cap, i;
    EXPECT_EQ(IDWriteTextFormat_GetLocaleNameLength(t), 5);
    for (i = 0; i < 16; i++) buf[i] = 0x7777;
    CHECK_HR(IDWriteTextFormat_GetLocaleName(t, buf, 0), DW_E_NOT_SUFFICIENT_BUFFER);
    for (i = 0; i < 16; i++) CHECK(buf[i] == 0x7777);
    for (cap = 1; cap <= 5; cap++) {
        for (i = 0; i < 16; i++) buf[i] = 0x7777;
        CHECK_HR(IDWriteTextFormat_GetLocaleName(t, buf, cap), DW_E_NOT_SUFFICIENT_BUFFER);
        CHECKF(buf[cap - 1] == 0, "capacity %u: not NUL terminated", cap);
        for (i = 0; i + 1 < cap; i++) CHECKF(buf[i] == L"en-us"[i], "capacity %u: char %u", cap, i);
        CHECKF(buf[cap] == 0x7777, "capacity %u: wrote past the buffer", cap);
    }
    for (cap = 6; cap <= 12; cap++) {
        for (i = 0; i < 16; i++) buf[i] = 0x7777;
        CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(t, buf, cap));
        CHECK_WEQ(buf, L"en-us");
        CHECKF(buf[6] == 0x7777, "capacity %u: padded the buffer", cap);
    }
    DW_RELEASE(t);
}

TEST(length_getters_do_not_count_the_terminator)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_make(f, L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"de-DE");
    WCHAR fam[32], loc[32];
    UINT32 fl = IDWriteTextFormat_GetFontFamilyNameLength(t), ll = IDWriteTextFormat_GetLocaleNameLength(t);
    EXPECT_EQ(fl, 8); EXPECT_EQ(ll, 5);
    CHECK_HR_OK(tf_family(t, fam, fl + 1)); CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(t, loc, ll + 1));
    EXPECT_EQ(wcslen(fam), fl); EXPECT_EQ(wcslen(loc), ll);
    CHECK_HR(tf_family(t, fam, fl), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR(IDWriteTextFormat_GetLocaleName(t, loc, ll), DW_E_NOT_SUFFICIENT_BUFFER);
    DW_RELEASE(t);
}

/* ---- size / tab stop ----------------------------------------------------------------------------------- */
TEST(create_size_roundtrip_table)
{
    TF_NEED_FACTORY();
    static const float sz[] = { 0.125f, 1.0f, 6.0f, 9.5f, 10.0f, 10.667f, 12.0f, 13.3333f, 14.0f, 18.0f, 24.0f, 36.0f, 48.0f, 96.0f, 123.456f, 500.0f, 2000.0f };
    size_t i;
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        IDWriteTextFormat *t = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz[i], L"en-us");
        CHECK_FBITS(IDWriteTextFormat_GetFontSize(t), sz[i]);
        DW_RELEASE(t);
    }
}

TEST(create_default_tab_stop_is_four_ems)
{
    TF_NEED_FACTORY();
    /* The constructor stores size * 4.0f as the incremental tab stop. */
    static const float sz[] = { 0.125f, 1.0f, 8.0f, 10.0f, 12.5f, 16.0f, 24.0f, 72.0f, 100.0f, 333.5f };
    size_t i;
    for (i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        IDWriteTextFormat *t = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz[i], L"en-us");
        CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(t), sz[i] * 4.0f);
        DW_RELEASE(t);
    }
}

/* ---- defaults of a fresh format ------------------------------------------------------------------------- */
TEST(default_alignment_wrapping_directions)
{
    TF_NEW(t);
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(t), DWRITE_TEXT_ALIGNMENT_LEADING);
    EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(t), DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(t), DWRITE_WORD_WRAPPING_WRAP);
    EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(t), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(t), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    DW_RELEASE(t);
}

TEST(default_trimming_none_and_no_sign)
{
    TF_NEW(t);
    DWRITE_TRIMMING tr; IDWriteInlineObject *sign = (IDWriteInlineObject *)(ULONG_PTR)1;
    memset(&tr, 0xAB, sizeof tr);
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &tr, &sign));
    EXPECT_EQ(tr.granularity, DWRITE_TRIMMING_GRANULARITY_NONE);
    EXPECT_EQ(tr.delimiter, 0);
    EXPECT_EQ(tr.delimiterCount, 0);
    CHECK(sign == NULL);
    DW_RELEASE(t);
}

TEST(default_line_spacing_is_default_method_zero)
{
    TF_NEW(t);
    DWRITE_LINE_SPACING_METHOD m = (DWRITE_LINE_SPACING_METHOD)77; float sp = -1.0f, bl = -1.0f;
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_DEFAULT);
    CHECK_FBITS(sp, 0.0f);
    CHECK_FBITS(bl, 0.0f);
    DW_RELEASE(t);
}

TEST(default_line_spacing_struct_form)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING ls; memset(&ls, 0x5A, sizeof ls);
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &ls));
    EXPECT_EQ(ls.method, DWRITE_LINE_SPACING_METHOD_DEFAULT);
    CHECK_FBITS(ls.height, 0.0f);
    CHECK_FBITS(ls.baseline, 0.0f);
    t_note("default leadingBefore=%g fontLineGapUsage=%d", ls.leadingBefore, (int)ls.fontLineGapUsage);
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(default_format1_properties)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    IDWriteFontFallback *fb = (IDWriteFontFallback *)(ULONG_PTR)1;
    EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(t1), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
    EXPECT_EQ(IDWriteTextFormat1_GetOpticalAlignment(t1), DWRITE_OPTICAL_ALIGNMENT_NONE);
    EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), TRUE);       /* last-line wrapping is on by default */
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(t1, &fb));
    CHECK(fb == NULL);                                                 /* no custom fallback: S_OK with NULL */
    DW_RELEASE(t1); DW_RELEASE(t);
}

TEST(default_format3_properties)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), DWRITE_AUTOMATIC_FONT_AXES_NONE);
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 0);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(default_font_collection_is_system_collection)
{
    TF_NEW(t);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &c));
    CHECK(c != NULL);
    if (c) {
        EXPECT_NE(IDWriteFontCollection_GetFontFamilyCount(c), 0);
        UINT32 idx = 99; BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex));
        t_note("system collection FindFamilyName(Arial) exists=%d idx=%u", ex, idx);
        IDWriteFontCollection *sys = NULL;
        if (SUCCEEDED(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE)) && sys) {
            t_note("default format collection %s the factory system collection pointer", sys == c ? "equals" : "differs from");
            IDWriteFontCollection_Release(sys);
        }
        DW_RELEASE(c);
    }
    DW_RELEASE(t);
}

TEST(default_state_independent_of_properties_given)
{
    TF_NEED_FACTORY();
    /* The same default behaviour holds for every weight/style/stretch/size combination: only the size drives the tab stop. */
    IDWriteTextFormat *a = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_ULTRA_EXPANDED, 33.0f, L"ja-JP");
    DWRITE_TRIMMING tr; IDWriteInlineObject *sign = NULL;
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(a), DWRITE_TEXT_ALIGNMENT_LEADING);
    EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(a), DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(a), DWRITE_WORD_WRAPPING_WRAP);
    EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(a), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(a), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(a), 132.0f);
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(a, &tr, &sign));
    EXPECT_EQ(tr.granularity, DWRITE_TRIMMING_GRANULARITY_NONE); CHECK(sign == NULL);
    DW_RELEASE(a);
}

/* ---- factories --------------------------------------------------------------------------------------- */
TEST(create_through_every_factory_version)
{
    REQUIRE(dw_proc());
    int v;
    for (v = 0; v <= 7; v++) {
        IDWriteFactory *fv = (IDWriteFactory *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v);
        IDWriteTextFormat *t = SENTINEL; WCHAR buf[16];
        if (!fv) { t_note("IDWriteFactory%d unsupported", v); continue; }
        /* The base CreateTextFormat slot is inherited by every factory interface. */
        CHECK_HR_OK(IDWriteFactory_CreateTextFormat(fv, L"Arial", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 21.0f, L"fr-FR", &t));
        if (t && t != SENTINEL) {
            EXPECT_EQ(IDWriteTextFormat_GetFontWeight(t), DWRITE_FONT_WEIGHT_BOLD);
            EXPECT_EQ(IDWriteTextFormat_GetFontStyle(t), DWRITE_FONT_STYLE_ITALIC);
            EXPECT_EQ(IDWriteTextFormat_GetFontStretch(t), DWRITE_FONT_STRETCH_CONDENSED);
            CHECK_FBITS(IDWriteTextFormat_GetFontSize(t), 21.0f);
            CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(t, buf, 16)); CHECK_WEQ(buf, L"fr-fr");
            IDWriteTextFormat_Release(t);
        }
        t = SENTINEL;
        CHECK_HR(IDWriteFactory_CreateTextFormat(fv, L"Arial", NULL, (DWRITE_FONT_WEIGHT)0, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 21.0f, L"fr-FR", &t), E_INVALIDARG);
        CHECK_OUT_NULL(t);
    }
}

TEST(create_on_isolated_factory)
{
    REQUIRE(dw_proc());
    IDWriteFactory *iso = dw_isolated();
    if (!iso) { t_skip("no isolated factory"); return; }
    IDWriteTextFormat *t = SENTINEL;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(iso, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-us", &t));
    if (t && t != SENTINEL) {
        CHECK_FBITS(IDWriteTextFormat_GetFontSize(t), 15.0f);
        CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(t), 60.0f);
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &c));
        CHECK(c != NULL);
        DW_RELEASE(c);
        IDWriteTextFormat_Release(t);
    }
    if (t_is_wine()) return;
    t = SENTINEL;
    CHECK_HR(IDWriteFactory_CreateTextFormat(iso, NULL, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-us", &t), E_INVALIDARG);
    CHECK_OUT_NULL(t);
}

TEST(create_many_formats_then_release)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *fm[200]; int i, n = 0;
    for (i = 0; i < 200; i++) {
        fm[i] = NULL;
        if (SUCCEEDED(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 8.0f + i, L"en-us", &fm[i]))) n++;
    }
    EXPECT_EQ(n, 200);
    for (i = 0; i < 200; i++) if (fm[i]) { CHECK_FBITS(IDWriteTextFormat_GetFontSize(fm[i]), 8.0f + i); IDWriteTextFormat_Release(fm[i]); }
}

TEST(create_returns_distinct_objects)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    CHECK(a != b);
    EXPECT_REFS(a, 1); EXPECT_REFS(b, 1);
    DW_RELEASE(a); DW_RELEASE(b);
}

/* ---- font collection -------------------------------------------------------------------------------- */
TEST(create_with_explicit_system_collection_keeps_identity)
{
    TF_NEED_FACTORY();
    IDWriteFontCollection *sys = NULL, *got = NULL;
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE));
    if (!sys) { t_skip("no system collection"); return; }
    ULONG before = dw_refcount((IUnknown *)sys);
    IDWriteTextFormat *t = SENTINEL;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", sys, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &t));
    if (t && t != SENTINEL) {
        EXPECT_EQ(dw_refcount((IUnknown *)sys), before + 1);          /* the format keeps one reference */
        CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &got));
        CHECK(got == sys);                                             /* the very same object is handed back */
        EXPECT_EQ(dw_refcount((IUnknown *)sys), before + 2);
        DW_RELEASE(got);
        IDWriteTextFormat_Release(t);
        EXPECT_EQ(dw_refcount((IUnknown *)sys), before);
    }
    DW_RELEASE(sys);
}

TEST(create_with_custom_collection_identity_and_lifetime)
{
    TF_NEED_FACTORY();
    ttf_spec s; size_t n; uint8_t *blob; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll = NULL; UINT32 key;
    ttf_style_spec(&s, "TfColl", 400, 5, 0); blob = ttf_build(&s, &n);
    fl = dwx_loader_new(); key = dwx_loader_add(fl, blob, n); free(blob);
    CHECK_HR_OK(dwx_loader_register(fl, f));
    cl = dwx_coll_new(fl); dwx_coll_add_key(cl, key);
    CHECK_HR_OK(dwx_coll_register(cl, f));
    if (SUCCEEDED(dwx_coll_create(cl, f, &coll)) && coll) {
        ULONG before = dw_refcount((IUnknown *)coll);
        IDWriteTextFormat *t = SENTINEL; IDWriteFontCollection *got = NULL;
        CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"TfColl", coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &t));
        if (t && t != SENTINEL) {
            EXPECT_EQ(dw_refcount((IUnknown *)coll), before + 1);
            CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &got));
            CHECK(got == coll);
            DW_RELEASE(got);
            DW_RELEASE(coll);                                           /* the format alone keeps the collection alive */
            CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &got));
            CHECK(got != NULL);
            if (got) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(got), 1); EXPECT_REFS(got, 2); }
            DW_RELEASE(got);
            IDWriteTextFormat_Release(t);
        }
        DW_RELEASE(coll);
    } else t_note("custom collection could not be created");
    dwx_coll_unregister(cl); dwx_loader_unregister(fl);
    dwx_release(cl); dwx_release(fl);
}

TEST(get_font_collection_adds_one_reference_per_call)
{
    TF_NEW(t);
    IDWriteFontCollection *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &a));
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &b));
    CHECK(a != NULL && a == b);
    if (a) { ULONG r = dw_refcount((IUnknown *)a); IDWriteFontCollection_Release(b); b = NULL; EXPECT_REFS(a, r - 1); }
    DW_RELEASE(b); DW_RELEASE(a); DW_RELEASE(t);
}

/* ---- IDWriteFactory6::CreateTextFormat (font axis values) ------------------------------------------------ */
#define F6_NEED() IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6); if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
#define SENT3 ((IDWriteTextFormat3 *)(ULONG_PTR)0x1)

TEST(f6_create_with_axes_roundtrip)
{
    F6_NEED();
    DWRITE_FONT_AXIS_VALUE ax[3] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 650.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 87.5f }, { DWRITE_FONT_AXIS_TAG_ITALIC, 1.0f } };
    DWRITE_FONT_AXIS_VALUE got[5]; IDWriteTextFormat3 *t = SENT3; int i;
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Bahnschrift", NULL, ax, 3, 14.0f, L"en-us", &t));
    if (!t || t == SENT3) { t_skip("axes format not created"); return; }
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 3);
    for (i = 0; i < 5; i++) { got[i].axisTag = 0x12345678; got[i].value = 9.0f; }
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, got, 5));
    for (i = 0; i < 3; i++) { EXPECT_EQ(got[i].axisTag, ax[i].axisTag); CHECK_FBITS(got[i].value, ax[i].value); }
    for (i = 3; i < 5; i++) { EXPECT_EQ(got[i].axisTag, 0); CHECK_FBITS(got[i].value, 0.0f); }   /* unused entries are zero filled */
    IDWriteTextFormat3_Release(t);
}

TEST(f6_create_axes_property_defaults)
{
    F6_NEED();
    /* Without weight/style/stretch parameters the object is created as normal/normal/normal (400, upright, 5). */
    DWRITE_FONT_AXIS_VALUE ax = { DWRITE_FONT_AXIS_TAG_WEIGHT, 900.0f };
    IDWriteTextFormat3 *t = SENT3; WCHAR buf[32];
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Bahnschrift", NULL, &ax, 1, 18.0f, L"EN-us", &t));
    if (!t || t == SENT3) { t_skip("axes format not created"); return; }
    IDWriteTextFormat *b = (IDWriteTextFormat *)tf_qi((IUnknown *)t, &IID_IDWriteTextFormat, "IDWriteTextFormat");
    EXPECT_EQ(IDWriteTextFormat_GetFontWeight(b), 400);
    EXPECT_EQ(IDWriteTextFormat_GetFontStyle(b), DWRITE_FONT_STYLE_NORMAL);
    EXPECT_EQ(IDWriteTextFormat_GetFontStretch(b), DWRITE_FONT_STRETCH_NORMAL);
    CHECK_FBITS(IDWriteTextFormat_GetFontSize(b), 18.0f);
    CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(b), 72.0f);
    CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(b, buf, 32)); CHECK_WEQ(buf, L"Bahnschrift");
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(b, buf, 32)); CHECK_WEQ(buf, L"en-us");
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(b), DWRITE_TEXT_ALIGNMENT_LEADING);
    DW_RELEASE(b); IDWriteTextFormat3_Release(t);
}

TEST(f6_create_zero_axes_allows_null_pointer)
{
    F6_NEED();
    IDWriteTextFormat3 *t = SENT3;
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 0, 12.0f, L"en-us", &t));
    if (t && t != SENT3) { EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 0); IDWriteTextFormat3_Release(t); }
}

TEST(f6_create_validation)
{
    F6_NEED();
    DWRITE_FONT_AXIS_VALUE ax = { DWRITE_FONT_AXIS_TAG_WEIGHT, 500.0f };
    IDWriteTextFormat3 *t;
    if (t_is_wine()) { t_skip("argument validation faults in the emulated implementation"); return; }
    t = SENT3; CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, NULL, NULL, &ax, 1, 12.0f, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
    t = SENT3; CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, &ax, 1, 12.0f, NULL, &t), E_INVALIDARG); CHECK_OUT_NULL(t);
    t = SENT3; CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 1, 12.0f, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
    t = SENT3; CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, &ax, 0x10000, 12.0f, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
    t = SENT3; CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, &ax, 0xffffffffu, 12.0f, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
    t = SENT3; CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, &ax, 1, 0.0f, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
    t = SENT3; CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, &ax, 1, -4.0f, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
    t = SENT3; CHECK_HR(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, &ax, 1, NAN, L"en-us", &t), E_INVALIDARG); CHECK_OUT_NULL(t);
}

TEST(f6_create_axes_are_copied)
{
    F6_NEED();
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 300.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 75.0f } }, got[2];
    IDWriteTextFormat3 *t = SENT3;
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, ax, 2, 12.0f, L"en-us", &t));
    if (!t || t == SENT3) { t_skip("axes format not created"); return; }
    ax[0].value = 999.0f; ax[1].axisTag = 0;
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, got, 2));
    CHECK_FBITS(got[0].value, 300.0f); EXPECT_EQ(got[1].axisTag, DWRITE_FONT_AXIS_TAG_WIDTH);
    IDWriteTextFormat3_Release(t);
}

TEST(f6_create_max_axis_count)
{
    F6_NEED();
    /* 0xFFFF entries is the largest accepted count. */
    DWRITE_FONT_AXIS_VALUE *ax = (DWRITE_FONT_AXIS_VALUE *)calloc(0xFFFF, sizeof *ax), *got = (DWRITE_FONT_AXIS_VALUE *)calloc(0xFFFF, sizeof *ax);
    IDWriteTextFormat3 *t = SENT3; UINT32 i, bad = 0;
    for (i = 0; i < 0xFFFF; i++) { ax[i].axisTag = 0x20202020 + i; ax[i].value = (float)i; }
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, ax, 0xFFFF, 12.0f, L"en-us", &t));
    if (t && t != SENT3) {
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t), 0xFFFF);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t, got, 0xFFFF));
        for (i = 0; i < 0xFFFF; i++) if (got[i].axisTag != ax[i].axisTag || got[i].value != ax[i].value) bad++;
        CHECK(bad == 0);
        IDWriteTextFormat3_Release(t);
    }
    free(ax); free(got);
}

TEST(f6_default_collection_model_note)
{
    F6_NEED();
    IDWriteTextFormat3 *t = SENT3; IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 0, 12.0f, L"en-us", &t));
    if (!t || t == SENT3) return;
    CHECK_HR_OK(IDWriteTextFormat3_GetFontCollection(t, &c));
    CHECK(c != NULL);
    if (c) {
        IDWriteFontCollection2 *c2 = NULL;
        if (SUCCEEDED(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2)) && c2) {
            t_note("axes-format default collection family model = %d", (int)IDWriteFontCollection2_GetFontFamilyModel(c2));
            IDWriteFontCollection2_Release(c2);
        } else t_note("default collection does not expose IDWriteFontCollection2");
        DW_RELEASE(c);
    }
    IDWriteTextFormat3_Release(t);
}

TEST(f6_created_format_supports_full_interface_chain)
{
    F6_NEED();
    IDWriteTextFormat3 *t = SENT3; void *p;
    CHECK_HR_OK(IDWriteFactory6_CreateTextFormat(f6, L"Arial", NULL, NULL, 0, 12.0f, L"en-us", &t));
    if (!t || t == SENT3) return;
    p = NULL; CHECK_HR_OK(IDWriteTextFormat3_QueryInterface(t, &IID_IDWriteTextFormat, &p)); DW_RELEASE(p);
    p = NULL; CHECK_HR_OK(IDWriteTextFormat3_QueryInterface(t, &IID_IDWriteTextFormat1, &p)); DW_RELEASE(p);
    p = NULL; CHECK_HR_OK(IDWriteTextFormat3_QueryInterface(t, &IID_IDWriteTextFormat2, &p)); DW_RELEASE(p);
    IDWriteTextFormat3_Release(t);
}

/* ---- lifetime ------------------------------------------------------------------------------------------ */
TEST(format_outlives_its_factory)
{
    REQUIRE(dw_proc());
    IDWriteFactory *nf = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!nf) { t_skip("cannot create a private factory"); return; }
    IDWriteTextFormat *t = SENTINEL; IDWriteFontCollection *c = NULL; WCHAR buf[16];
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(nf, L"Arial", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STRETCH_EXPANDED, 19.0f, L"it-IT", &t));
    if (!t || t == SENTINEL) { IDWriteFactory_Release(nf); return; }
    IDWriteFactory_Release(nf);
    /* Properties are plain data; the collection reference taken at creation keeps working after the factory is gone. */
    EXPECT_EQ(IDWriteTextFormat_GetFontWeight(t), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    EXPECT_EQ(IDWriteTextFormat_GetFontStyle(t), DWRITE_FONT_STYLE_OBLIQUE);
    EXPECT_EQ(IDWriteTextFormat_GetFontStretch(t), DWRITE_FONT_STRETCH_EXPANDED);
    CHECK_FBITS(IDWriteTextFormat_GetFontSize(t), 19.0f);
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(t, buf, 16)); CHECK_WEQ(buf, L"it-it");
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_CENTER));
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(t), DWRITE_TEXT_ALIGNMENT_CENTER);
    CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &c));
    CHECK(c != NULL);
    if (c) { EXPECT_NE(IDWriteFontCollection_GetFontFamilyCount(c), 0); DW_RELEASE(c); }
    IDWriteTextFormat_Release(t);
}
