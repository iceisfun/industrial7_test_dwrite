#include "ntf.h"

/* CreateNumberSubstitution (DWriteFactory vtable method) on Windows 10:
 *   - *substitution is zeroed first;
 *   - localeName == NULL is E_INVALIDARG for every method except NONE (the only method that never reads the locale);
 *   - methods 0..4 are FROM_CULTURE, CONTEXTUAL, NONE, NATIONAL, TRADITIONAL; anything else throws ArgumentException
 *     which the COM boundary turns into E_INVALIDARG;
 *   - NATIONAL needs locale data (GetLocaleInfoEx digits/decimal/thousand); TRADITIONAL and CONTEXTUAL try the built-in
 *     traditional table first and fall back to NATIONAL, so an unknown locale also fails there;
 *   - the object implements IUnknown + IDWriteNumberSubstitution only (no methods of its own). */

#define M_CULTURE DWRITE_NUMBER_SUBSTITUTION_METHOD_FROM_CULTURE
#define M_CONTEXT DWRITE_NUMBER_SUBSTITUTION_METHOD_CONTEXTUAL
#define M_NONE    DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE
#define M_NATIONAL DWRITE_NUMBER_SUBSTITUTION_METHOD_NATIONAL
#define M_TRADIT  DWRITE_NUMBER_SUBSTITUTION_METHOD_TRADITIONAL

static const char *mname(int m)
{
    static const char *n[] = { "FROM_CULTURE", "CONTEXTUAL", "NONE", "NATIONAL", "TRADITIONAL" };
    return (m >= 0 && m < 5) ? n[m] : "invalid";
}

TEST(ns_enum_values)
{
    EXPECT_EQ(M_CULTURE, 0); EXPECT_EQ(M_CONTEXT, 1); EXPECT_EQ(M_NONE, 2); EXPECT_EQ(M_NATIONAL, 3); EXPECT_EQ(M_TRADIT, 4);
}

TEST(ns_create_each_method_en_us)
{
    IDWriteFactory *f = dw_shared(); int m, ign;
    for (m = 0; m < 5; m++) for (ign = 0; ign < 2; ign++) {
        IDWriteNumberSubstitution *ns = (IDWriteNumberSubstitution *)(void *)0;
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, L"en-US", ign, &ns);
        CHECKF(hr == S_OK, "%s ign=%d -> %s", mname(m), ign, dw_hr_name(hr));
        CHECK(ns != NULL);
        if (ns) EXPECT_REFS(ns, 1);
        DW_RELEASE(ns);
    }
}

TEST(ns_invalid_method_values)
{
    IDWriteFactory *f = dw_shared();
    static const int bad[] = { 5, 6, 7, 100, 255 };   /* larger values (256, 0x80000000) are masked/accepted on Win10 */
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteNumberSubstitution *ns = NULL;
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)bad[i], L"en-US", FALSE, &ns);
        CHECKF(hr == E_INVALIDARG, "method %d -> %s", bad[i], dw_hr_name(hr));   /* ArgumentException("localeName") from the constructor */
        CHECK_OUT_NULL(ns);
        DW_RELEASE(ns);
    }
}

TEST(ns_invalid_method_with_null_locale)
{
    /* NULL locale is rejected before the method is looked at, for any method but NONE; an invalid method with NULL is therefore E_INVALIDARG too. */
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL;
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)9, NULL, FALSE, &ns), E_INVALIDARG);
    CHECK_OUT_NULL(ns);
}

TEST(ns_null_locale_per_method)
{
    IDWriteFactory *f = dw_shared(); int m;
    for (m = 0; m < 5; m++) {
        IDWriteNumberSubstitution *ns = NULL;
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, NULL, FALSE, &ns);
        if (m == M_NONE) { CHECKF(hr == S_OK, "NONE with NULL locale -> %s", dw_hr_name(hr)); CHECK(ns != NULL); }
        else { CHECKF(hr == E_INVALIDARG, "%s with NULL locale -> %s", mname(m), dw_hr_name(hr)); CHECK_OUT_NULL(ns); }
        DW_RELEASE(ns);
    }
}

TEST(ns_none_ignores_locale_text)
{
    /* NONE initializes from the nominal table without reading the locale, so even garbage text is accepted. */
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL;
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"", FALSE, &ns), S_OK); DW_RELEASE(ns);
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"not a locale at all", TRUE, &ns), S_OK); DW_RELEASE(ns);
    CHECK_HR(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"zz-ZZ-qq", FALSE, &ns), S_OK); DW_RELEASE(ns);
}

TEST(ns_national_bogus_locale)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL;
    static const WCHAR *bad[] = { L"!!bad!!", L"not a locale at all", L"12345", L"--" };   /* "zz-ZZ-qq-xx" is accepted on Win10 (syntactic parse only), so it is not a rejection case */
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, bad[i], FALSE, &ns);
        CHECKF(hr == E_INVALIDARG, "NATIONAL \"%ls\" -> %s", bad[i], dw_hr_name(hr));   /* GetData returns false -> ArgumentException("localeName") */
        CHECK_OUT_NULL(ns);
        DW_RELEASE(ns);
    }
}

TEST(ns_traditional_bogus_locale)
{
    /* TRADITIONAL tries the internal table, then falls back to NATIONAL; both fail for an unknown locale. */
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL;
    static const WCHAR *bad[] = { L"!!bad!!", L"not a locale at all" };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, M_TRADIT, bad[i], TRUE, &ns);
        CHECKF(hr == E_INVALIDARG, "TRADITIONAL \"%ls\" -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(ns);
        DW_RELEASE(ns);
    }
}

TEST(ns_contextual_bogus_locale)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL;
    HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, M_CONTEXT, L"!!bad!!", FALSE, &ns);
    CHECKF(hr == E_INVALIDARG, "CONTEXTUAL bogus -> %s", dw_hr_name(hr));   /* same traditional-then-national chain as TRADITIONAL */
    CHECK_OUT_NULL(ns); DW_RELEASE(ns);
}

TEST(ns_from_culture_bogus_locale)
{
    /* FROM_CULTURE resolves the method from locale data first; what an unknown locale resolves to is not pinned down here. */
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL;
    HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, M_CULTURE, L"!!bad!!", FALSE, &ns);
    t_note("FROM_CULTURE with unknown locale -> %s ns=%p", dw_hr_name(hr), (void *)ns);
    CHECK(hr == S_OK || hr == E_INVALIDARG);
    if (FAILED(hr)) CHECK_OUT_NULL(ns);
    DW_RELEASE(ns);
}

TEST(ns_empty_locale_note)
{
    /* L"" is the invariant locale name for GetLocaleInfoEx, so it may well succeed; record it. */
    IDWriteFactory *f = dw_shared(); int m;
    for (m = 0; m < 5; m++) {
        IDWriteNumberSubstitution *ns = NULL;
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, L"", FALSE, &ns);
        t_note("empty locale %s -> %s", mname(m), dw_hr_name(hr));
        CHECK(hr == S_OK || hr == E_INVALIDARG);
        if (FAILED(hr)) CHECK_OUT_NULL(ns); else CHECK(ns != NULL);
        DW_RELEASE(ns);
    }
}

TEST(ns_valid_locale_table)
{
    IDWriteFactory *f = dw_shared();
    static const WCHAR *loc[] = { L"en-US", L"en-us", L"EN-us", L"ar-SA", L"fa-IR", L"hi-IN", L"th-TH", L"ja-JP", L"de-DE", L"ar-EG", L"bn-BD", L"ur-PK", L"mr-IN", L"my-MM" };
    unsigned i; int m, ign;
    for (i = 0; i < sizeof loc / sizeof loc[0]; i++)
        for (m = 0; m < 5; m++) for (ign = 0; ign < 2; ign++) {
            IDWriteNumberSubstitution *ns = NULL;
            HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)m, loc[i], ign, &ns);
            if (hr != S_OK) t_check(0, __FILE__, __LINE__, "create", "%s \"%ls\" ign=%d -> %s", mname(m), loc[i], ign, dw_hr_name(hr));
            else CHECK(ns != NULL);
            DW_RELEASE(ns);
        }
}

TEST(ns_neutral_locale_note)
{
    IDWriteFactory *f = dw_shared();
    static const WCHAR *loc[] = { L"ar", L"fa", L"en", L"hi", L"zh" };
    unsigned i;
    for (i = 0; i < sizeof loc / sizeof loc[0]; i++) {
        IDWriteNumberSubstitution *ns = NULL;
        HRESULT hr = IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, loc[i], TRUE, &ns);
        t_note("NATIONAL neutral \"%ls\" -> %s", loc[i], dw_hr_name(hr));
        CHECK(hr == S_OK || hr == E_INVALIDARG);
        DW_RELEASE(ns);
    }
}

TEST(ns_not_equal_objects)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"en-US", FALSE, &a));
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"en-US", FALSE, &b));
    CHECK(a != NULL && b != NULL && a != b);      /* a new object per call (operator new(0x48)) */
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(ns_refcount_protocol)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; ULONG r;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    if (!ns) return;
    EXPECT_REFS(ns, 1);
    r = IDWriteNumberSubstitution_AddRef(ns); EXPECT_EQ(r, 2);
    r = IDWriteNumberSubstitution_AddRef(ns); EXPECT_EQ(r, 3);
    r = IDWriteNumberSubstitution_Release(ns); EXPECT_EQ(r, 2);
    r = IDWriteNumberSubstitution_Release(ns); EXPECT_EQ(r, 1);
    r = IDWriteNumberSubstitution_Release(ns); EXPECT_EQ(r, 0);
}

TEST(ns_qi_matrix)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; IUnknown *u = NULL, *u2 = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"en-US", FALSE, &ns));
    if (!ns) return;
    NTF_QI_ONLY(ns, &IID_IDWriteNumberSubstitution);
    CHECK_HR_OK(IDWriteNumberSubstitution_QueryInterface(ns, &IID_IUnknown, (void **)&u));
    CHECK_HR_OK(IDWriteNumberSubstitution_QueryInterface(ns, &IID_IDWriteNumberSubstitution, (void **)&u2));
    CHECK(u == (IUnknown *)ns); CHECK(u2 == (IUnknown *)ns);       /* single-inheritance object: identity is the interface pointer */
    DW_RELEASE(u); DW_RELEASE(u2);
    EXPECT_REFS(ns, 1);
    DW_RELEASE(ns);
}

TEST(ns_qi_null_out_iid_zeroes_out)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; void *pv = (void *)(ULONG_PTR)0x10;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"en-US", FALSE, &ns));
    if (!ns) return;
    CHECK_HR(IDWriteNumberSubstitution_QueryInterface(ns, &IID_IDWriteTextFormat, &pv), E_NOINTERFACE);
    CHECK(pv == NULL);
    EXPECT_REFS(ns, 1);
    DW_RELEASE(ns);
}

TEST(ns_outlives_factory)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteNumberSubstitution *ns = NULL; ULONG r;
    if (!f) { t_skip("no isolated factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"fa-IR", TRUE, &ns));
    DW_RELEASE(f);
    if (!ns) return;
    r = IDWriteNumberSubstitution_AddRef(ns); EXPECT_EQ(r, 2);
    IDWriteNumberSubstitution_Release(ns);
    DW_RELEASE(ns);
}

TEST(ns_isolated_and_shared_factories)
{
    IDWriteFactory *iso = dw_isolated(); IDWriteNumberSubstitution *ns = NULL;
    if (!iso) { t_skip("no isolated factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(iso, M_TRADIT, L"ar-SA", TRUE, &ns)); CHECK(ns != NULL); DW_RELEASE(ns);
}

TEST(ns_via_factory_versions)
{
    /* The method is inherited by every IDWriteFactoryN through the same vtable slot. */
    int v;
    for (v = 0; v <= 7; v++) {
        IDWriteFactory *fv = (v == 0) ? dw_shared() : (IDWriteFactory *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v);
        IDWriteNumberSubstitution *ns = NULL;
        if (!fv) { t_note("IDWriteFactory%d unsupported", v); continue; }
        CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(fv, M_NATIONAL, L"en-US", FALSE, &ns));
        CHECK(ns != NULL); DW_RELEASE(ns);
        CHECK_HR(IDWriteFactory_CreateNumberSubstitution(fv, (DWRITE_NUMBER_SUBSTITUTION_METHOD)5, L"en-US", FALSE, &ns), E_INVALIDARG);
        CHECK_OUT_NULL(ns);
    }
}

TEST(ns_many_objects_release_in_any_order)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *a[40]; int i;
    for (i = 0; i < 40; i++) { a[i] = NULL; CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)(i % 5), L"ar-SA", i & 1, &a[i])); }
    for (i = 39; i >= 0; i -= 2) DW_RELEASE(a[i]);
    for (i = 0; i < 40; i++) DW_RELEASE(a[i]);
}

/* ---- effect through the analyzer ------------------------------------------------------------------------------------------ */

typedef struct { const char *what; int method; const WCHAR *loc; int ign; unsigned base; } ns_row;

/* Expected glyph of digit i in NtfMain: Latin '0'+i -> 54+i, U+0660+i -> 2+i, U+06F0+i -> 38+i. */
static void shape_digits(ntf_fx *X, IDWriteFontFace *face, int method, const WCHAR *loc, int ign, int use_ns, unsigned short *g, UINT32 *n, HRESULT *hr_out)
{
    IDWriteNumberSubstitution *ns = NULL; unsigned short cl[64];
    if (use_ns) CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(X->f, (DWRITE_NUMBER_SUBSTITUTION_METHOD)method, loc, ign, &ns));
    *hr_out = ntf_shape(X->f, face, L"0123456789", loc, ns, NULL, g, cl, n);
    DW_RELEASE(ns);
}

TEST(ns_effect_none_and_null_keep_latin_digits)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); unsigned short g[64]; UINT32 n, i; HRESULT hr;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    shape_digits(&X, face, M_NONE, L"ar-SA", TRUE, 1, g, &n, &hr);
    CHECK_HR(hr, S_OK); EXPECT_EQ(n, 10);
    for (i = 0; i < n && i < 10; i++) EXPECT_EQ(g[i], 54 + i);
    shape_digits(&X, face, 0, L"ar-SA", TRUE, 0, g, &n, &hr);       /* no substitution object at all */
    CHECK_HR(hr, S_OK); EXPECT_EQ(n, 10);
    for (i = 0; i < n && i < 10; i++) EXPECT_EQ(g[i], 54 + i);
    DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_national_arabic)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); unsigned short g[64]; UINT32 n, i; HRESULT hr;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    shape_digits(&X, face, M_NATIONAL, L"ar-SA", TRUE, 1, g, &n, &hr);
    CHECK_HR(hr, S_OK); EXPECT_EQ(n, 10);
    /* ar-SA native digits (LOCALE_SNATIVEDIGITS) are the Arabic-Indic U+0660..0669 */
    for (i = 0; i < n && i < 10; i++) CHECKF(g[i] == 2 + i, "digit %u -> glyph %u want %u", i, g[i], 2 + i);
    DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_national_persian)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); unsigned short g[64]; UINT32 n, i; HRESULT hr;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    shape_digits(&X, face, M_NATIONAL, L"fa-IR", TRUE, 1, g, &n, &hr);
    CHECK_HR(hr, S_OK); EXPECT_EQ(n, 10);
    for (i = 0; i < n && i < 10; i++) CHECKF(g[i] == 38 + i, "digit %u -> glyph %u want %u", i, g[i], 38 + i);   /* U+06F0.. */
    DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_national_latin_locales)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); unsigned short g[64]; UINT32 n, i; HRESULT hr;
    static const WCHAR *loc[] = { L"en-US", L"de-DE", L"ja-JP", L"fr-FR" };
    unsigned k;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    for (k = 0; k < sizeof loc / sizeof loc[0]; k++) {
        shape_digits(&X, face, M_NATIONAL, loc[k], TRUE, 1, g, &n, &hr);
        CHECK_HR(hr, S_OK); EXPECT_EQ(n, 10);
        for (i = 0; i < n && i < 10; i++) CHECKF(g[i] == 54 + i, "%ls digit %u -> glyph %u", loc[k], i, g[i]);
    }
    DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_traditional_notes)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); unsigned short g[64]; UINT32 n; HRESULT hr; int m;
    static const WCHAR *loc[] = { L"ar-SA", L"fa-IR", L"en-US" };
    unsigned k;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    for (k = 0; k < 3; k++) for (m = 0; m < 5; m++) {
        shape_digits(&X, face, m, loc[k], TRUE, 1, g, &n, &hr);
        CHECK_HR(hr, S_OK);
        CHECK(n == 10);
        t_note("%s %ls digit1 glyph %u digit9 glyph %u", mname(m), loc[k], n > 1 ? g[1] : 0, n > 9 ? g[9] : 0);
    }
    DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_non_digits_unchanged)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); IDWriteNumberSubstitution *ns = NULL; unsigned short g[64], cl[64]; UINT32 n;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(X.f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(ntf_shape(X.f, face, L"abAB 7", L"ar-SA", ns, NULL, g, cl, &n), S_OK);
    EXPECT_EQ(n, 5);
    if (n == 5) {   /* letters/space keep their cmap glyphs; with the default liga the pair "ab" forms glyph 67; '7' -> U+0667 -> 2+7 */
        EXPECT_EQ(g[0], 67); EXPECT_EQ(g[1], 2); EXPECT_EQ(g[2], 3); EXPECT_EQ(g[3], 1); EXPECT_EQ(g[4], 9);
    }
    DW_RELEASE(ns); DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_mixed_text_digits_only)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); IDWriteNumberSubstitution *ns = NULL; unsigned short g[64], cl[64]; UINT32 n;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(X.f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(ntf_shape(X.f, face, L"A1B2", L"ar-SA", ns, NULL, g, cl, &n), S_OK);
    EXPECT_EQ(n, 4);
    if (n == 4) { EXPECT_EQ(g[0], 2); EXPECT_EQ(g[1], 3); EXPECT_EQ(g[2], 3); EXPECT_EQ(g[3], 4); }   /* 'A'=2, U+0661=3, 'B'=3, U+0662=4 */
    DW_RELEASE(ns); DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_separators_note)
{
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); IDWriteNumberSubstitution *ns = NULL; unsigned short g[64], cl[64]; UINT32 n;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(X.f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(ntf_shape(X.f, face, L"1.5,2%", L"ar-SA", ns, NULL, g, cl, &n), S_OK);
    EXPECT_EQ(n, 6);
    if (n == 6) {
        t_note("ar-SA NATIONAL '.'->glyph %u (65 = U+066B) ','->%u (66 = U+066C) '%%'->%u (64 = U+066A)", g[1], g[3], g[5]);
        EXPECT_EQ(g[0], 3); EXPECT_EQ(g[2], 7);       /* digits '1' and '5' -> U+0661 U+0665 */
    }
    DW_RELEASE(ns); DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_ignore_user_override_flag)
{
    /* With ignoreUserOverride the registry values are not consulted: both flag values must still produce a complete digit string. */
    NTF_BEGIN(); IDWriteFontFace *face = ntf_face(X.coll, NTF_MAIN); unsigned short g[64]; UINT32 n; HRESULT hr; int ign;
    if (!face) { NTF_END(); t_skip("no face"); return; }
    for (ign = 0; ign < 2; ign++) {
        shape_digits(&X, face, M_NATIONAL, L"ar-SA", ign, 1, g, &n, &hr);
        CHECK_HR(hr, S_OK); EXPECT_EQ(n, 10);
        if (ign) CHECK(n == 10 && g[0] == 2);
        else t_note("ar-SA NATIONAL ignoreUserOverride=FALSE digit0 glyph %u", n ? g[0] : 0);
    }
    DW_RELEASE(face); NTF_END();
}

TEST(ns_effect_arial_ar_sa)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    IDWriteNumberSubstitution *ns = NULL; unsigned short a[64], b[64], c[64], cl[64]; UINT32 na, nb, nc, i;
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(ntf_shape(f, face, L"0123456789", L"ar-SA", ns, NULL, a, cl, &na), S_OK);
    CHECK_HR(ntf_shape(f, face, L"0123456789", L"ar-SA", NULL, NULL, b, cl, &nb), S_OK);
    EXPECT_EQ(na, 10); EXPECT_EQ(nb, 10);
    for (i = 0; i < 10 && i < na && i < nb; i++) CHECKF(a[i] != b[i], "digit %u: national glyph %u equals Latin glyph %u", i, a[i], b[i]);
    /* The substituted glyphs are exactly the glyphs of the Arabic-Indic code points. */
    {
        UINT32 cp[10]; UINT16 want[10];
        for (i = 0; i < 10; i++) cp[i] = 0x660 + i;
        if (SUCCEEDED(IDWriteFontFace_GetGlyphIndices(face, cp, 10, want))) {
            for (i = 0; i < 10 && i < na; i++) CHECKF(a[i] == want[i], "digit %u: glyph %u vs cmap(U+%04X) %u", i, a[i], 0x660 + i, want[i]);
        }
    }
    /* NONE in the same locale keeps Latin digits. */
    DW_RELEASE(ns);
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"ar-SA", TRUE, &ns));
    CHECK_HR(ntf_shape(f, face, L"0123456789", L"ar-SA", ns, NULL, c, cl, &nc), S_OK);
    for (i = 0; i < 10 && i < nc && i < nb; i++) EXPECT_EQ(c[i], b[i]);
    DW_RELEASE(ns); DW_RELEASE(face);
}

TEST(ns_effect_arial_en_us_national_is_latin)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    IDWriteNumberSubstitution *ns = NULL; unsigned short a[64], b[64], cl[64]; UINT32 na, nb, i;
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"en-US", TRUE, &ns));
    CHECK_HR(ntf_shape(f, face, L"0123456789", L"en-US", ns, NULL, a, cl, &na), S_OK);
    CHECK_HR(ntf_shape(f, face, L"0123456789", L"en-US", NULL, NULL, b, cl, &nb), S_OK);
    EXPECT_EQ(na, nb);
    for (i = 0; i < na && i < nb; i++) EXPECT_EQ(a[i], b[i]);
    DW_RELEASE(ns); DW_RELEASE(face);
}

/* ---- AnalyzeNumberSubstitution ---------------------------------------------------------------------------------------------
 * Win10: NULL source/sink or textPosition+textLength overflow => E_INVALIDARG. The analyzer asks the source for (substitution, run length)
 * at the start of every source run; a NULL object counts as NONE. For each maximal digit run (digits plus '.' ',' between digits and a trailing '%')
 * inside a run whose method is applicable, the sink gets ONE SetNumberSubstitution(position, length, object). NONE never reports; NATIONAL and
 * TRADITIONAL always report; CONTEXTUAL depends on the script of the surrounding text. */

static HRESULT analyze(IDWriteFactory *f, const WCHAR *text, UINT32 len, IDWriteNumberSubstitution *ns, UINT32 run, UINT32 pos, UINT32 alen, dwx_ana_src **sp, dwx_ana_sink **kp)
{
    IDWriteTextAnalyzer *an = NULL; HRESULT hr;
    dwx_ana_src *src = dwx_ana_src_new(text, len); dwx_ana_sink *sink = dwx_ana_sink_new();
    src->subst = ns; src->subst_run = run;
    hr = IDWriteFactory_CreateTextAnalyzer(f, &an);
    if (SUCCEEDED(hr)) hr = IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, pos, alen, (IDWriteTextAnalysisSink *)sink);
    DW_RELEASE(an);
    *sp = src; *kp = sink;
    return hr;
}
static int sink_set_range(dwx_ana_sink *k, IDWriteNumberSubstitution *ns, UINT32 a, UINT32 b, UINT32 total)
{
    UINT32 i;
    for (i = 0; i < total && i < DWX_ANA_MAX; i++) {
        int want = (i >= a && i < b);
        if (want ? !(k->subst_set[i] && k->subst[i] == ns) : k->subst_set[i]) return 0;
    }
    return 1;
}

TEST(ns_analyze_argument_validation)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; static const WCHAR t[] = L"123";
    dwx_ana_src *src = dwx_ana_src_new(t, 3); dwx_ana_sink *sink = dwx_ana_sink_new();
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &an));
    if (an) {
        CHECK_HR(IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, NULL, 0, 3, (IDWriteTextAnalysisSink *)sink), E_INVALIDARG);
        CHECK_HR(IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, 0, 3, NULL), E_INVALIDARG);
        CHECK_HR(IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, 0xffffffffu, 2, (IDWriteTextAnalysisSink *)sink), E_INVALIDARG);
        CHECK_HR(IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, 2, 0xffffffffu, (IDWriteTextAnalysisSink *)sink), E_INVALIDARG);
        EXPECT_EQ(src->n_subst, 0); EXPECT_EQ(sink->n_subst, 0);
    }
    dwx_release(src); dwx_release(sink); DW_RELEASE(an);
}

TEST(ns_analyze_zero_length_is_noop)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(analyze(f, L"123", 3, ns, 0, 0, 0, &src, &sink), S_OK);
    EXPECT_EQ(src->n_subst, 0); EXPECT_EQ(sink->n_subst, 0);      /* the loop body is skipped when textLength == 0 */
    dwx_release(src); dwx_release(sink); DW_RELEASE(ns);
}

TEST(ns_analyze_national_reports_digit_run)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(analyze(f, L"abc 123 def", 11, ns, 0, 0, 11, &src, &sink), S_OK);
    EXPECT_EQ(sink->n_subst, 1);
    CHECK(sink_set_range(sink, ns, 4, 7, 11));
    CHECK(src->n_subst >= 1);
    dwx_release(src); dwx_release(sink);
    EXPECT_REFS(ns, 1);                   /* every reference the source handed out was released again */
    DW_RELEASE(ns);
}

TEST(ns_analyze_traditional_reports_digit_run)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_TRADIT, L"ar-SA", TRUE, &ns));
    CHECK_HR(analyze(f, L"x 42 y", 6, ns, 0, 0, 6, &src, &sink), S_OK);
    EXPECT_EQ(sink->n_subst, 1);
    CHECK(sink_set_range(sink, ns, 2, 4, 6));
    dwx_release(src); dwx_release(sink); DW_RELEASE(ns);
}

TEST(ns_analyze_none_never_reports)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NONE, L"ar-SA", TRUE, &ns));
    CHECK_HR(analyze(f, L"abc 123 def", 11, ns, 0, 0, 11, &src, &sink), S_OK);
    EXPECT_EQ(sink->n_subst, 0);          /* IsSubstistutionApplicable(NONE) == false */
    CHECK(src->n_subst >= 1);
    dwx_release(src); dwx_release(sink); DW_RELEASE(ns);
}

TEST(ns_analyze_null_object_is_none)
{
    IDWriteFactory *f = dw_shared(); dwx_ana_src *src; dwx_ana_sink *sink;
    CHECK_HR(analyze(f, L"12 ab", 5, NULL, 0, 0, 5, &src, &sink), S_OK);
    EXPECT_EQ(sink->n_subst, 0);          /* a NULL substitution from the source is treated as method NONE */
    CHECK(src->n_subst >= 1);
    dwx_release(src); dwx_release(sink);
}

TEST(ns_analyze_two_digit_runs)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"fa-IR", TRUE, &ns));
    CHECK_HR(analyze(f, L"1 2", 3, ns, 0, 0, 3, &src, &sink), S_OK);
    EXPECT_EQ(sink->n_subst, 2);
    CHECK(sink_set_range(sink, ns, 0, 1, 3) || (sink->subst_set[0] && sink->subst_set[2] && !sink->subst_set[1]));
    dwx_release(src); dwx_release(sink); DW_RELEASE(ns);
}

TEST(ns_analyze_separators_join_the_number)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(analyze(f, L"1.5,2%", 6, ns, 0, 0, 6, &src, &sink), S_OK);
    EXPECT_EQ(sink->n_subst, 1);          /* '.' and ',' between digits and a '%' after a digit stay inside one reported run */
    CHECK(sink_set_range(sink, ns, 0, 6, 6));
    dwx_release(src); dwx_release(sink); DW_RELEASE(ns);
}

TEST(ns_analyze_run_chunks_follow_source)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink; int i, at4 = 0, at8 = 0;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(analyze(f, L"0123456789", 10, ns, 4, 0, 10, &src, &sink), S_OK);
    EXPECT_EQ(sink->n_subst, 3);          /* source runs of 4, 4 and 2 characters: one report per run */
    CHECK(sink_set_range(sink, ns, 0, 10, 10));
    for (i = 0; i < src->log_n; i++) if (src->log[i].fn == 5) { if (src->log[i].pos == 4) at4 = 1; if (src->log[i].pos == 8) at8 = 1; }
    CHECK(at4 && at8);
    dwx_release(src); dwx_release(sink); DW_RELEASE(ns);
}

TEST(ns_analyze_subrange_of_text)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR(analyze(f, L"12 34 56", 8, ns, 0, 3, 2, &src, &sink), S_OK);      /* only "34" is analysed */
    EXPECT_EQ(sink->n_subst, 1);
    CHECK(sink_set_range(sink, ns, 3, 5, 8));
    dwx_release(src); dwx_release(sink); DW_RELEASE(ns);
}

TEST(ns_analyze_contextual_note)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink; int k;
    static const WCHAR *txt[] = { L"123", L"abc 123", L"\x0627\x0644 123", L"\x05d0 123" };
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_CONTEXT, L"ar-SA", TRUE, &ns));
    for (k = 0; k < 4; k++) {
        CHECK_HR(analyze(f, txt[k], (UINT32)wcslen(txt[k]), ns, 0, 0, (UINT32)wcslen(txt[k]), &src, &sink), S_OK);
        t_note("CONTEXTUAL ar-SA text %d: sink reports %ld", k, sink->n_subst);
        dwx_release(src); dwx_release(sink);
    }
    DW_RELEASE(ns);
}

TEST(ns_analyze_sink_failure_propagates)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink; IDWriteTextAnalyzer *an = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &an));
    src = dwx_ana_src_new(L"123", 3); src->subst = ns; sink = dwx_ana_sink_new(); sink->fail_subst = E_FAIL;
    if (an) CHECK_HR(IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, 0, 3, (IDWriteTextAnalysisSink *)sink), E_FAIL);
    dwx_release(src); dwx_release(sink); DW_RELEASE(an);
    EXPECT_REFS(ns, 1);
    DW_RELEASE(ns);
}

TEST(ns_analyze_source_failure_propagates)
{
    IDWriteFactory *f = dw_shared(); IDWriteNumberSubstitution *ns = NULL; dwx_ana_src *src; dwx_ana_sink *sink; IDWriteTextAnalyzer *an = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateNumberSubstitution(f, M_NATIONAL, L"ar-SA", TRUE, &ns));
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &an));
    src = dwx_ana_src_new(L"123", 3); src->subst = ns; src->fail_subst = E_ABORT; sink = dwx_ana_sink_new();
    if (an) CHECK_HR(IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, 0, 3, (IDWriteTextAnalysisSink *)sink), E_ABORT);
    EXPECT_EQ(sink->n_subst, 0);
    dwx_release(src); dwx_release(sink); DW_RELEASE(an); DW_RELEASE(ns);
}
