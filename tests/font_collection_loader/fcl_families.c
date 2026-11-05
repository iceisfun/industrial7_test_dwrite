/* Family structure of a custom collection: GetFontFamilyCount / GetFontFamily / FindFamilyName / family names and grouping.
 *
 * Win10 decomp facts:
 *  - A collection from CreateCustomFontCollection uses the weight-stretch-style family model (FontCollection ctor stores
 *    model 1): fonts are grouped by the WWS family name, i.e. name ID 21 if present, else the legacy family name (ID 1);
 *    typographic names (ID 16/17) do not merge e.g. "Fam Light" into "Fam".
 *  - Every localized family name of every font of a family is entered in the name map, so FindFamilyName succeeds for any
 *    locale's name; the en-us name decides which family a font joins.
 *  - FindFamilyName always returns S_OK; on a miss *index = 0xFFFFFFFF and *exists = FALSE.
 *  - GetFontFamily(i) with i >= count throws IndexOutOfRangeException, which the method maps to E_FAIL (not E_INVALIDARG);
 *    *ppFamily is NULL.
 */
#include "fcl.h"

static void std_names(ttf_spec *s, const char *id1, const char *id2)
{
    char full[160];
    snprintf(full, sizeof full, "%s %s", id1, id2);
    ttf_add_name(s, 3, 1, 0x409, 1, id1);
    ttf_add_name(s, 3, 1, 0x409, 2, id2);
    ttf_add_name(s, 3, 1, 0x409, 4, full);
    ttf_add_name(s, 3, 1, 0x409, 6, "PsName-Regular");
}
/* A synthetic font with only the given English legacy names. */
static UINT32 add_legacy(fcl_fx *x, const char *id1, const char *id2, int weight, int italic)
{
    ttf_spec s; ttf_style_spec(&s, "Placeholder", weight, 5, italic);
    ttf_clear_names(&s);
    std_names(&s, id1, id2);
    return fcl_add_spec(x, &s);
}
#define W(s) L##s

/* ------------------------------------------------------------------------------------------ counts */
TEST(fcl_fam_one_family)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Single Fam");
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    char nm[64];
    CHECK(fcl_name_at(c, 0, nm, sizeof nm));
    EXPECT_STR(nm, "Single Fam");
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_count_by_number_of_distinct_families)
{
    int n;
    for (n = 1; n <= 12; n++) {
        FCL_OPEN(x);
        int i; char nm[32];
        for (i = 0; i < n; i++) { snprintf(nm, sizeof nm, "Count Fam %02d", i); fcl_add_family(&x, nm); }
        FCL_COLL(x, c);
        UINT32 got = IDWriteFontCollection_GetFontFamilyCount(c);
        CHECKF(got == (UINT32)n, "n=%d got %u", n, got);
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_fam_count_is_stable)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Stable A"); fcl_add_family(&x, "Stable B"); fcl_add_family(&x, "Stable C");
    FCL_COLL(x, c);
    UINT32 n0 = IDWriteFontCollection_GetFontFamilyCount(c), i;
    for (i = 0; i < 20; i++) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), n0);
    EXPECT_EQ(n0, 3);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_ribbi_is_one_family)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Ribbi", 400, 5, 0);
    fcl_add_style(&x, "Ribbi", 700, 5, 0);
    fcl_add_style(&x, "Ribbi", 400, 5, 1);
    fcl_add_style(&x, "Ribbi", 700, 5, 1);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    long fi = fcl_find(c, "Ribbi");
    CHECK(fi == 0);
    if (fi >= 0) {
        int w, it;
        for (w = 0; w < 2; w++) for (it = 0; it < 2; it++) {
            IDWriteFont *f = fcl_pick(c, (UINT32)fi, w ? 700 : 400, DWRITE_FONT_STRETCH_NORMAL, it ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_SIMULATIONS_NONE);
            CHECKF(f != NULL, "Ribbi weight %d italic %d missing", w ? 700 : 400, it);
            DW_RELEASE(f);
        }
        t_note("RIBBI family font count %u", fcl_font_count(c, (UINT32)fi));
        CHECK(fcl_font_count(c, (UINT32)fi) >= 4);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_regular_only_is_completed_with_simulations)
{
    /* AddSimulationsForWwsCompleteness: a Regular face gets a simulated Bold, and every non-oblique face a simulated Oblique. */
    FCL_OPEN(x);
    fcl_add_family(&x, "Sims Only");
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    if (fam) {
        UINT32 n = IDWriteFontFamily_GetFontCount(fam), k; int seen[4] = { 0 };
        EXPECT_EQ(n, 4);
        for (k = 0; k < n; k++) {
            IDWriteFont *f = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &f)) && f) {
                DWRITE_FONT_SIMULATIONS s = IDWriteFont_GetSimulations(f);
                CHECKF((unsigned)s < 4, "unexpected simulations %u", (unsigned)s);
                if ((unsigned)s < 4) seen[s]++;
                if (s & DWRITE_FONT_SIMULATIONS_BOLD) EXPECT_EQ(IDWriteFont_GetWeight(f), 700);
                else EXPECT_EQ(IDWriteFont_GetWeight(f), 400);
                IDWriteFont_Release(f);
            }
        }
        int s; for (s = 0; s < 4; s++) CHECKF(seen[s] == 1, "simulation set %d seen %d times", s, seen[s]);
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_two_families_found_by_name)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Twin One"); fcl_add_family(&x, "Twin Two");
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    long a = fcl_find(c, "Twin One"), b = fcl_find(c, "Twin Two");
    CHECK(a >= 0 && b >= 0 && a != b);
    CHECK(a < 2 && b < 2);
    char nm[64];
    if (a >= 0) { CHECK(fcl_name_at(c, (UINT32)a, nm, sizeof nm)); EXPECT_STR(nm, "Twin One"); }
    if (b >= 0) { CHECK(fcl_name_at(c, (UINT32)b, nm, sizeof nm)); EXPECT_STR(nm, "Twin Two"); }
    fcl_note_families(c, "enumeration order of Twin One/Two");
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_order_note)
{
    /* Families are added in a scrambled order; the position-to-name mapping must be a consistent bijection. The ordering rule itself is
     * noted, not asserted. */
    FCL_OPEN(x);
    static const char *names[] = { "Mango", "apple", "Zebra", "banana", "Cherry", "Apple Pie" };
    int i, n = 6;
    for (i = 0; i < n; i++) fcl_add_family(&x, names[i]);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), n);
    int seen[6] = { 0 }; char nm[64];
    for (i = 0; i < n; i++) {
        CHECK(fcl_name_at(c, (UINT32)i, nm, sizeof nm));
        int j; for (j = 0; j < n; j++) if (!strcmp(nm, names[j])) seen[j]++;
    }
    for (i = 0; i < n; i++) CHECKF(seen[i] == 1, "%s listed %d times", names[i], seen[i]);
    for (i = 0; i < n; i++) {
        long idx = fcl_find(c, names[i]);
        CHECKF(idx >= 0, "%s not found", names[i]);
        if (idx >= 0) { fcl_name_at(c, (UINT32)idx, nm, sizeof nm); EXPECT_STR(nm, names[i]); }
    }
    fcl_note_families(c, "families added as Mango, apple, Zebra, banana, Cherry, Apple Pie");
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_order_independent_of_unrelated_calls)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Ord Gamma"); fcl_add_family(&x, "Ord Alpha"); fcl_add_family(&x, "Ord Beta");
    FCL_COLL(x, c);
    char a[3][64]; int i, pass;
    for (pass = 0; pass < 3; pass++) {
        for (i = 0; i < 3; i++) {
            char nm[64]; fcl_name_at(c, (UINT32)i, nm, sizeof nm);
            if (pass == 0) strcpy(a[i], nm); else CHECK(!strcmp(nm, a[i]));
        }
        UINT32 idx; BOOL ex; IDWriteFontCollection_FindFamilyName(c, L"nothing", &idx, &ex);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_two_collections_same_fonts_same_order)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Same Z"); fcl_add_family(&x, "Same A"); fcl_add_family(&x, "Same M");
    IDWriteFontCollection *c1 = NULL, *c2 = NULL;
    CHECK_HR_OK(fcl_create(&x, &c1));
    CHECK_HR_OK(fcl_create(&x, &c2));
    if (c1 && c2) {
        UINT32 i;
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c1), IDWriteFontCollection_GetFontFamilyCount(c2));
        for (i = 0; i < 3; i++) { char a[64], b[64]; fcl_name_at(c1, i, a, sizeof a); fcl_name_at(c2, i, b, sizeof b); CHECK(!strcmp(a, b)); }
    }
    DW_RELEASE(c1); DW_RELEASE(c2);
    fcl_close(&x);
}

/* ------------------------------------------------------------------------------------- GetFontFamily */
TEST(fcl_fam_get_index_out_of_range)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Bounds A"); fcl_add_family(&x, "Bounds B");
    FCL_COLL(x, c);
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c);
    static const UINT32 deltas[] = { 0, 1, 2, 100 };
    unsigned i;
    for (i = 0; i < sizeof deltas / sizeof deltas[0]; i++) {
        IDWriteFontFamily *fam = (IDWriteFontFamily *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontCollection_GetFontFamily(c, n + deltas[i], &fam);
        CHECKF(hr == FCL_E_FAIL, "index count+%u -> %s", deltas[i], dw_hr_name(hr));
        CHECK_OUT_NULL(fam);
    }
    static const UINT32 big[] = { 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (i = 0; i < sizeof big / sizeof big[0]; i++) {
        IDWriteFontFamily *fam = (IDWriteFontFamily *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontCollection_GetFontFamily(c, big[i], &fam);
        CHECKF(hr == FCL_E_FAIL, "index 0x%x -> %s", big[i], dw_hr_name(hr));
        CHECK_OUT_NULL(fam);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_get_last_valid_index)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Last A"); fcl_add_family(&x, "Last B"); fcl_add_family(&x, "Last C");
    FCL_COLL(x, c);
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c), i;
    for (i = 0; i < n; i++) {
        IDWriteFontFamily *fam = NULL;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, i, &fam));
        CHECK(fam != NULL);
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_get_returns_fresh_object_each_time)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Fresh");
    FCL_COLL(x, c);
    IDWriteFontFamily *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &a));
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &b));
    CHECK(a != NULL && b != NULL);
    CHECK(a != b);
    if (a) EXPECT_REFS(a, 1);
    if (b) EXPECT_REFS(b, 1);
    DW_RELEASE(a); DW_RELEASE(b);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_family_holds_the_collection)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Holder");
    FCL_COLL(x, c);
    ULONG base = dw_refcount((IUnknown *)c);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    CHECK(dw_refcount((IUnknown *)c) > base);
    DW_RELEASE(fam);
    EXPECT_REFS(c, base);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_family_collection_identity)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Identity");
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    if (fam) {
        IDWriteFontCollection *back = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(fam, &back));
        CHECK(back != NULL);
        if (back) { CHECK(dw_same_identity((IUnknown *)back, (IUnknown *)c)); DW_RELEASE(back); }
    }
    DW_RELEASE(fam);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_fam_family_outlives_collection_release)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Outlive", 400, 5, 0);
    fcl_add_style(&x, "Outlive", 700, 5, 0);
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    DW_RELEASE(c);                                          /* the family keeps the collection alive */
    if (fam) {
        char nm[64];
        CHECK(fcl_fam_name_of(fam, nm, sizeof nm));
        EXPECT_STR(nm, "Outlive");
        CHECK(IDWriteFontFamily_GetFontCount(fam) >= 2);
        IDWriteFont *f = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, 0, &f));
        DW_RELEASE(f);
    }
    DW_RELEASE(fam);
    fcl_close(&x);
}

/* ------------------------------------------------------------------------------------- FindFamilyName */
TEST(fcl_find_exact_name)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Findable");
    FCL_COLL(x, c);
    UINT32 idx = 0x1234; BOOL ex = 0x55;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"Findable", &idx, &ex));
    EXPECT_EQ(ex, 1);
    EXPECT_EQ(idx, 0);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_find_case_insensitive)
{
    static const WCHAR *variants[] = { L"findable", L"FINDABLE", L"fInDaBlE", L"Findable", L"fINDABLE" };
    FCL_OPEN(x);
    fcl_add_family(&x, "Findable");
    fcl_add_family(&x, "Other Name");
    FCL_COLL(x, c);
    long want = fcl_find(c, "Findable");
    unsigned i;
    for (i = 0; i < sizeof variants / sizeof variants[0]; i++) {
        UINT32 idx = 0xAAAA; BOOL ex = 0;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, variants[i], &idx, &ex));
        CHECKF(ex == 1, "variant %u not found", i);
        CHECKF((long)idx == want, "variant %u index %u want %ld", i, idx, want);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_find_case_insensitive_latin1)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "\xC3\x9C" "ber F\xC3\xA4m");           /* "Über Fäm" */
    FCL_COLL(x, c);
    UINT32 idx = 9; BOOL ex = 0;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"Über Fäm", &idx, &ex)); EXPECT_EQ(ex, 1); EXPECT_EQ(idx, 0);
    ex = 0; idx = 9;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"über fäm", &idx, &ex)); EXPECT_EQ(ex, 1); EXPECT_EQ(idx, 0);
    ex = 0; idx = 9;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"ÜBER FÄM", &idx, &ex)); EXPECT_EQ(ex, 1); EXPECT_EQ(idx, 0);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_find_non_latin_case_note)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "\xCE\x91\xCE\xB2\xCE\xB3 Fam");     /* Greek capital alpha, beta, gamma */
    FCL_COLL(x, c);
    UINT32 idx = 9; BOOL ex = 0;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"Αβγ Fam", &idx, &ex)); EXPECT_EQ(ex, 1);
    ex = 0;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"αΒΓ fam", &idx, &ex));
    t_note("Greek mixed-case lookup exists=%d", ex);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_find_miss_outputs)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Present");
    FCL_COLL(x, c);
    UINT32 idx = 0x1234; BOOL ex = 0x77;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"Absent", &idx, &ex));
    EXPECT_EQ(ex, 0);
    EXPECT_EQ(idx, 0xFFFFFFFF);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_find_near_misses)
{
    static const WCHAR *misses[] = { L"", L" ", L"Presen", L"Presentt", L"Present ", L" Present", L"Pre sent", L"Present\t", L"Presenт", L"P", L"Present.", L"resent", L"Presen_t" };
    FCL_OPEN(x);
    fcl_add_family(&x, "Present");
    FCL_COLL(x, c);
    unsigned i;
    for (i = 0; i < sizeof misses / sizeof misses[0]; i++) {
        UINT32 idx = 5; BOOL ex = 1;
        HRESULT hr = IDWriteFontCollection_FindFamilyName(c, misses[i], &idx, &ex);
        CHECKF(hr == S_OK, "miss %u: %s", i, dw_hr_name(hr));
        CHECKF(ex == 0, "miss %u unexpectedly found", i);
        CHECKF(idx == 0xFFFFFFFF, "miss %u index 0x%x", i, idx);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_find_very_long_names_miss)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Short");
    FCL_COLL(x, c);
    WCHAR *w = (WCHAR *)malloc(5001 * sizeof(WCHAR));
    int i;
    for (i = 0; i < 5000; i++) w[i] = L'a' + (i % 26);
    w[5000] = 0;
    UINT32 idx = 3; BOOL ex = 1;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, w, &idx, &ex));
    EXPECT_EQ(ex, 0); EXPECT_EQ(idx, 0xFFFFFFFF);
    free(w);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_find_matches_family_enumeration)
{
    FCL_OPEN(x);
    char nm[32]; int i;
    for (i = 0; i < 8; i++) { snprintf(nm, sizeof nm, "Round Trip %c", 'A' + i); fcl_add_family(&x, nm); }
    FCL_COLL(x, c);
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c), k;
    EXPECT_EQ(n, 8);
    for (k = 0; k < n; k++) {
        char got[64]; WCHAR w[64];
        CHECK(fcl_name_at(c, k, got, sizeof got));
        dw_wide(got, w, 64);
        UINT32 idx = 99; BOOL ex = 0;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, w, &idx, &ex));
        CHECKF(ex && idx == k, "%s: found=%d idx=%u want %u", got, ex, idx, k);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_find_after_loader_unregistered)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Unreg Find");
    FCL_COLL(x, c);
    dwx_coll_unregister(x.cl);
    UINT32 idx = 5; BOOL ex = 0;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"unreg find", &idx, &ex));
    EXPECT_EQ(ex, 1); EXPECT_EQ(idx, 0);
    DW_RELEASE(c);
    fcl_close(&x);
}

/* ------------------------------------------------------------------------------------- family names */
TEST(fcl_names_single_english)
{
    FCL_OPEN(x);
    ttf_spec s; ttf_style_spec(&s, "Placeholder", 400, 5, 0); ttf_clear_names(&s);
    std_names(&s, "English Only", "Regular");
    fcl_add_spec(&x, &s);
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *ls = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &ls));
    if (ls) {
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(ls), 1);
        UINT32 idx = 9; BOOL ex = 0;
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(ls, L"en-us", &idx, &ex));
        EXPECT_EQ(ex, 1); EXPECT_EQ(idx, 0);
        UINT32 len = 0; CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(ls, 0, &len)); EXPECT_EQ(len, 12);
        WCHAR loc[32]; CHECK_HR_OK(IDWriteLocalizedStrings_GetLocaleName(ls, 0, loc, 32));
        CHECK(!lstrcmpiW(loc, L"en-us"));
        t_note("locale name as returned: %ls", loc);
        char nm[64]; CHECK(fcl_ls_en(ls, nm, sizeof nm)); EXPECT_STR(nm, "English Only");
        DW_RELEASE(ls);
    }
    DW_RELEASE(fam); DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_multiple_locales_findable_by_any)
{
    FCL_OPEN(x);
    ttf_spec s; ttf_style_spec(&s, "Placeholder", 400, 5, 0); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "Multi Locale"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular");
    ttf_add_name(&s, 3, 1, 0x407, 1, "Mehrsprachig");  ttf_add_name(&s, 3, 1, 0x407, 2, "Standard");
    ttf_add_name(&s, 3, 1, 0x40C, 1, "Multilingue");   ttf_add_name(&s, 3, 1, 0x40C, 2, "Normal");
    fcl_add_spec(&x, &s);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    static const char *all[] = { "Multi Locale", "Mehrsprachig", "Multilingue", "mehrsprachig", "MULTILINGUE" };
    unsigned i;
    for (i = 0; i < 5; i++) { long idx = fcl_find(c, all[i]); CHECKF(idx == 0, "%s -> %ld", all[i], idx); }
    IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *ls = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &ls));
    if (ls) {
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(ls), 3);
        static const struct { const WCHAR *loc; const char *name; } want[] = { { L"en-us", "Multi Locale" }, { L"de-de", "Mehrsprachig" }, { L"fr-fr", "Multilingue" } };
        for (i = 0; i < 3; i++) {
            UINT32 idx = 0; BOOL ex = 0; char got[64];
            CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(ls, want[i].loc, &idx, &ex));
            CHECKF(ex, "locale %ls missing", want[i].loc);
            if (ex) { CHECK_HR_OK(dw_ls_get(ls, idx, got, sizeof got)); CHECKF(!strcmp(got, want[i].name), "%ls -> %s", want[i].loc, got); }
        }
        char line[200] = ""; UINT32 k;
        for (k = 0; k < 3; k++) { WCHAR loc[32]; char l8[64]; IDWriteLocalizedStrings_GetLocaleName(ls, k, loc, 32); dw_narrow(loc, l8, 64); strcat(line, l8); strcat(line, " "); }
        t_note("locale order: %s", line);
        DW_RELEASE(ls);
    }
    DW_RELEASE(fam); DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_family_identity_is_the_english_name)
{
    /* Two fonts with the same non-English name but different en-us names are different families. */
    FCL_OPEN(x);
    ttf_spec a, b;
    ttf_style_spec(&a, "P", 400, 5, 0); ttf_clear_names(&a);
    ttf_add_name(&a, 3, 1, 0x409, 1, "Alpha Fam"); ttf_add_name(&a, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&a, 3, 1, 0x407, 1, "Gemeinsam"); ttf_add_name(&a, 3, 1, 0x407, 2, "Standard");
    ttf_style_spec(&b, "P", 400, 5, 0); ttf_clear_names(&b);
    ttf_add_name(&b, 3, 1, 0x409, 1, "Beta Fam");  ttf_add_name(&b, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&b, 3, 1, 0x407, 1, "Gemeinsam"); ttf_add_name(&b, 3, 1, 0x407, 2, "Standard");
    fcl_add_spec(&x, &a); fcl_add_spec(&x, &b);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    CHECK(fcl_find(c, "Alpha Fam") >= 0);
    CHECK(fcl_find(c, "Beta Fam") >= 0);
    long g = fcl_find(c, "Gemeinsam");
    t_note("shared German name resolves to index %ld", g);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_two_fonts_merge_by_english_name)
{
    FCL_OPEN(x);
    ttf_spec a, b;
    ttf_style_spec(&a, "P", 400, 5, 0); ttf_clear_names(&a);
    ttf_add_name(&a, 3, 1, 0x409, 1, "Merge Fam"); ttf_add_name(&a, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&a, 3, 1, 0x407, 1, "Verbund"); ttf_add_name(&a, 3, 1, 0x407, 2, "Standard");
    ttf_style_spec(&b, "P", 700, 5, 0); ttf_clear_names(&b);
    ttf_add_name(&b, 3, 1, 0x409, 1, "Merge Fam"); ttf_add_name(&b, 3, 1, 0x409, 2, "Bold");
    fcl_add_spec(&x, &a); fcl_add_spec(&x, &b);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    CHECK(fcl_find(c, "Verbund") == 0);                  /* the localized name of just one of the two fonts still resolves */
    IDWriteFont *reg = fcl_pick(c, 0, 400, 5, 0, 0), *bold = fcl_pick(c, 0, 700, 5, 0, 0);
    CHECK(reg != NULL && bold != NULL);
    DW_RELEASE(reg); DW_RELEASE(bold);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_only_non_english_name)
{
    FCL_OPEN(x);
    ttf_spec s; ttf_style_spec(&s, "P", 400, 5, 0); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x407, 1, "Nur Deutsch"); ttf_add_name(&s, 3, 1, 0x407, 2, "Standard");
    fcl_add_spec(&x, &s);
    FCL_COLL(x, c);
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c);
    t_note("font with only a de-de family name: %u families", n);
    if (n == 1) {
        CHECK(fcl_find(c, "Nur Deutsch") == 0);
        IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *ls = NULL;
        CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
        CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &ls));
        if (ls) { EXPECT_EQ(IDWriteLocalizedStrings_GetCount(ls), 1); DW_RELEASE(ls); }
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_mac_platform_only)
{
    FCL_OPEN(x);
    ttf_spec s; ttf_style_spec(&s, "P", 400, 5, 0); ttf_clear_names(&s);
    ttf_add_name(&s, 1, 0, 0, 1, "Mac Only Fam"); ttf_add_name(&s, 1, 0, 0, 2, "Regular");
    fcl_add_spec(&x, &s);
    FCL_COLL(x, c);
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c);
    t_note("Macintosh-platform-only names: %u families, find=%ld", n, fcl_find(c, "Mac Only Fam"));
    if (n == 1) { CHECK(fcl_find(c, "Mac Only Fam") == 0); char nm[64]; fcl_name_at(c, 0, nm, sizeof nm); t_note("name: %s", nm); }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_windows_and_mac_duplicates_collapse)
{
    FCL_OPEN(x);
    ttf_spec s; ttf_style_spec(&s, "P", 400, 5, 0); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "Dup Plat"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular");
    ttf_add_name(&s, 1, 0, 0, 1, "Dup Plat");     ttf_add_name(&s, 1, 0, 0, 2, "Regular");
    fcl_add_spec(&x, &s);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *ls = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &ls));
    if (ls) { t_note("windows+mac english names -> %u locale entries", IDWriteLocalizedStrings_GetCount(ls)); CHECK(IDWriteLocalizedStrings_GetCount(ls) >= 1); DW_RELEASE(ls); }
    DW_RELEASE(fam); DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_unicode_family_name)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E Font");           /* Japanese characters */
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    UINT32 idx = 5; BOOL ex = 0;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"日本語 Font", &idx, &ex));
    EXPECT_EQ(ex, 1); EXPECT_EQ(idx, 0);
    char nm[64]; CHECK(fcl_name_at(c, 0, nm, sizeof nm)); EXPECT_STR(nm, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E Font");
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_long_family_name)
{
    FCL_OPEN(x);
    char big[96]; int i;
    for (i = 0; i < 90; i++) big[i] = (char)('A' + (i % 26)); big[90] = 0;
    fcl_add_family(&x, big);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    CHECK(fcl_find(c, big) == 0);
    char nm[128]; CHECK(fcl_name_at(c, 0, nm, sizeof nm)); EXPECT_STR(nm, big);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_spaces_are_significant)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Spaced Name");
    fcl_add_family(&x, "SpacedName");
    fcl_add_family(&x, "Spaced  Name");
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 3);
    long a = fcl_find(c, "Spaced Name"), b = fcl_find(c, "SpacedName"), d = fcl_find(c, "Spaced  Name");
    CHECK(a >= 0 && b >= 0 && d >= 0);
    CHECK(a != b && b != d && a != d);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_case_variants_across_files)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Casey Fam", 400, 5, 0);
    fcl_add_style(&x, "CASEY FAM", 700, 5, 0);
    FCL_COLL(x, c);
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c);
    t_note("\"Casey Fam\" + \"CASEY FAM\" -> %u families", n);
    CHECK(n == 1 || n == 2);
    CHECK(fcl_find(c, "casey fam") >= 0);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_empty_family_name_no_crash)
{
    FCL_OPEN(x);
    add_legacy(&x, "", "Regular", 400, 0);
    fcl_add_family(&x, "Neighbour");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = fcl_create(&x, &c);
    t_note("empty family name -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && c) {
        t_note("families: %u", IDWriteFontCollection_GetFontFamilyCount(c));
        CHECK(fcl_find(c, "Neighbour") >= 0);
        UINT32 idx = 5; BOOL ex = 0;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"", &idx, &ex));
        t_note("FindFamilyName(\"\") exists=%d idx=%u", ex, idx);
    } else CHECK_OUT_NULL(c);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_names_missing_name_table_no_crash)
{
    FCL_OPEN(x);
    ttf_spec s; ttf_default_spec(&s); s.omit |= TTF_OMIT_NAME;
    fcl_add_spec(&x, &s);
    fcl_add_family(&x, "With Names");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = fcl_create(&x, &c);
    t_note("font without a name table -> %s, families=%u", dw_hr_name(hr), (SUCCEEDED(hr) && c) ? IDWriteFontCollection_GetFontFamilyCount(c) : 0);
    if (SUCCEEDED(hr) && c) CHECK(fcl_find(c, "With Names") >= 0); else CHECK_OUT_NULL(c);
    DW_RELEASE(c);
    fcl_close(&x);
}

/* ------------------------------------------------------------------------- legacy vs typographic naming */
TEST(fcl_group_light_weight_is_its_own_legacy_family)
{
    /* name ID 1 = "Wws Fam Light", ID 16 = "Wws Fam": the WWS model uses the legacy name. */
    FCL_OPEN(x);
    fcl_add_style(&x, "Wws Fam", 300, 5, 0);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    CHECK(fcl_find(c, "Wws Fam Light") == 0);
    t_note("typographic name \"Wws Fam\" resolves to %ld", fcl_find(c, "Wws Fam"));
    IDWriteFont *f = fcl_pick(c, 0, 300, 5, 0, 0);
    CHECK(f != NULL);
    DW_RELEASE(f);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_regular_and_light_are_two_families)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Split Fam", 400, 5, 0);
    fcl_add_style(&x, "Split Fam", 300, 5, 0);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    long r = fcl_find(c, "Split Fam"), l = fcl_find(c, "Split Fam Light");
    CHECK(r >= 0 && l >= 0 && r != l);
    if (r >= 0) { IDWriteFont *f = fcl_pick(c, (UINT32)r, 400, 5, 0, 0); CHECK(f != NULL); DW_RELEASE(f); f = fcl_pick(c, (UINT32)r, 300, 5, 0, -1); CHECK(f == NULL); DW_RELEASE(f); }
    if (l >= 0) { IDWriteFont *f = fcl_pick(c, (UINT32)l, 300, 5, 0, 0); CHECK(f != NULL); DW_RELEASE(f); }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_weight_ladder)
{
    static const int weights[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900 };
    FCL_OPEN(x);
    unsigned i;
    for (i = 0; i < sizeof weights / sizeof weights[0]; i++) fcl_add_style(&x, "Ladder", weights[i], 5, 0);
    FCL_COLL(x, c);
    /* "Ladder" holds 400+700 (legacy RIBBI), every other weight has its own legacy family name */
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 8);
    long base = fcl_find(c, "Ladder");
    CHECK(base >= 0);
    for (i = 0; i < sizeof weights / sizeof weights[0]; i++) {
        if (weights[i] == 400 || weights[i] == 700) continue;
        char nm[64]; snprintf(nm, sizeof nm, "Ladder %s", ttf_weight_word(weights[i]));
        long idx = fcl_find(c, nm);
        CHECKF(idx >= 0, "%s missing", nm);
        if (idx >= 0) { IDWriteFont *f = fcl_pick(c, (UINT32)idx, weights[i], 5, 0, 0); CHECKF(f != NULL, "%s: no face with weight %d", nm, weights[i]); DW_RELEASE(f); }
    }
    if (base >= 0) {
        IDWriteFont *r = fcl_pick(c, (UINT32)base, 400, 5, 0, 0), *b = fcl_pick(c, (UINT32)base, 700, 5, 0, 0);
        CHECK(r != NULL && b != NULL);
        DW_RELEASE(r); DW_RELEASE(b);
    }
    fcl_note_families(c, "weight ladder");
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_stretch_is_its_own_legacy_family)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Narrow Fam", 400, 5, 0);
    fcl_add_style(&x, "Narrow Fam", 400, 3, 0);      /* condensed */
    fcl_add_style(&x, "Narrow Fam", 400, 7, 0);      /* expanded */
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 3);
    long a = fcl_find(c, "Narrow Fam"), b = fcl_find(c, "Narrow Fam Condensed"), d = fcl_find(c, "Narrow Fam Expanded");
    CHECK(a >= 0 && b >= 0 && d >= 0);
    if (b >= 0) { IDWriteFont *f = fcl_pick(c, (UINT32)b, 400, DWRITE_FONT_STRETCH_CONDENSED, 0, 0); CHECK(f != NULL); DW_RELEASE(f); }
    if (d >= 0) { IDWriteFont *f = fcl_pick(c, (UINT32)d, 400, DWRITE_FONT_STRETCH_EXPANDED, 0, 0); CHECK(f != NULL); DW_RELEASE(f); }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_stretch_values_from_width_class)
{
    FCL_OPEN(x);
    int w;
    for (w = 1; w <= 9; w++) { char nm[32]; snprintf(nm, sizeof nm, "WidthFam%d", w); fcl_add_style(&x, nm, 400, w, 0); }
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 9);
    for (w = 1; w <= 9; w++) {
        char nm[48]; snprintf(nm, sizeof nm, "WidthFam%d", w);
        /* width 5 keeps the bare family name; the others have the stretch word appended, so look the family up by walking */
        UINT32 i, n = IDWriteFontCollection_GetFontFamilyCount(c); int found = 0;
        for (i = 0; i < n && !found; i++) {
            char got[64]; fcl_name_at(c, i, got, sizeof got);
            if (strncmp(got, nm, strlen(nm))) continue;
            IDWriteFont *f = fcl_pick(c, i, 400, w, 0, 0);
            if (f) found = 1;
            DW_RELEASE(f);
        }
        CHECKF(found, "no unsimulated face with usWidthClass %d (stretch %d)", w, w);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_italic_only_family)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Slanted", 400, 5, 1);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    CHECK(fcl_find(c, "Slanted") == 0);
    IDWriteFont *f = fcl_pick(c, 0, 400, 5, DWRITE_FONT_STYLE_ITALIC, 0);
    CHECK(f != NULL);
    DW_RELEASE(f);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_bold_only_family)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Heavy", 700, 5, 0);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    CHECK(fcl_find(c, "Heavy") == 0);
    IDWriteFont *f = fcl_pick(c, 0, 700, 5, 0, 0);
    CHECK(f != NULL);
    DW_RELEASE(f);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_name_id21_defines_the_wws_family)
{
    FCL_OPEN(x);
    ttf_spec s; ttf_style_spec(&s, "P", 300, 5, 0); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "Legacy Name Light"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular");
    ttf_add_name(&s, 3, 1, 0x409, 16, "Typo Name");        ttf_add_name(&s, 3, 1, 0x409, 17, "Light");
    ttf_add_name(&s, 3, 1, 0x409, 21, "Wws Name");          ttf_add_name(&s, 3, 1, 0x409, 22, "Light");
    fcl_add_spec(&x, &s);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    CHECK(fcl_find(c, "Wws Name") == 0);
    t_note("ID1 name resolves to %ld, ID16 name to %ld", fcl_find(c, "Legacy Name Light"), fcl_find(c, "Typo Name"));
    char nm[64]; CHECK(fcl_name_at(c, 0, nm, sizeof nm)); t_note("family name reported: %s", nm);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_typographic_names_do_not_merge_families)
{
    FCL_OPEN(x);
    ttf_spec a, b;
    ttf_style_spec(&a, "P", 400, 5, 0); ttf_clear_names(&a);
    ttf_add_name(&a, 3, 1, 0x409, 1, "Typo Group"); ttf_add_name(&a, 3, 1, 0x409, 2, "Regular");
    ttf_style_spec(&b, "P", 600, 5, 0); ttf_clear_names(&b);
    ttf_add_name(&b, 3, 1, 0x409, 1, "Typo Group SemiBold"); ttf_add_name(&b, 3, 1, 0x409, 2, "Regular");
    ttf_add_name(&b, 3, 1, 0x409, 16, "Typo Group");          ttf_add_name(&b, 3, 1, 0x409, 17, "SemiBold");
    fcl_add_spec(&x, &a); fcl_add_spec(&x, &b);
    FCL_COLL(x, c);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    CHECK(fcl_find(c, "Typo Group") >= 0);
    CHECK(fcl_find(c, "Typo Group SemiBold") >= 0);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_typographic_family_with_many_styles)
{
    /* A designer-style family: 16/17 names on every face; legacy names carry the weight. */
    FCL_OPEN(x);
    static const int ws[] = { 300, 400, 500, 700 };
    unsigned i;
    for (i = 0; i < 4; i++) fcl_add_style(&x, "Designer", ws[i], 5, 0);
    fcl_add_style(&x, "Designer", 400, 5, 1);
    FCL_COLL(x, c);
    UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c);
    t_note("Designer 300/400/500/700 + 400 italic -> %u families", n);
    fcl_note_families(c, "designer");
    CHECK(fcl_find(c, "Designer") >= 0);
    CHECK(fcl_find(c, "Designer Light") >= 0);
    CHECK(fcl_find(c, "Designer Medium") >= 0);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_group_font_in_family_has_matching_properties)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Props", 400, 5, 0); fcl_add_style(&x, "Props", 700, 5, 0); fcl_add_style(&x, "Props", 400, 5, 1);
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    if (fam) {
        UINT32 n = IDWriteFontFamily_GetFontCount(fam), k;
        for (k = 0; k < n; k++) {
            IDWriteFont *f = NULL;
            if (FAILED(IDWriteFontFamily_GetFont(fam, k, &f)) || !f) continue;
            DWRITE_FONT_WEIGHT w = IDWriteFont_GetWeight(f);
            DWRITE_FONT_STYLE st = IDWriteFont_GetStyle(f);
            DWRITE_FONT_SIMULATIONS sim = IDWriteFont_GetSimulations(f);
            CHECKF(w == 400 || w == 700, "font %u weight %d", k, w);
            CHECKF(st == 0 || st == 1 || st == 2, "font %u style %d", k, st);
            if (sim == 0) {                                          /* real faces keep the OS/2 weight and fsSelection style */
                if (w == 700) EXPECT_EQ(st, DWRITE_FONT_STYLE_NORMAL);
            }
            EXPECT_EQ(IDWriteFont_GetStretch(f), DWRITE_FONT_STRETCH_NORMAL);
            DW_RELEASE(f);
        }
    }
    DW_RELEASE(fam); DW_RELEASE(c);
    fcl_close(&x);
}
