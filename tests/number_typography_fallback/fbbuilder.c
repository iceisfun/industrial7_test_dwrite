#include "ntf.h"

/* IDWriteFontFallbackBuilder on Windows 10 (DWriteFontFallbackBuilder):
 *   - CreateFontFallbackBuilder zeroes the out pointer and always succeeds; the object answers IUnknown + IDWriteFontFallbackBuilder;
 *   - AddMapping returns E_INVALIDARG for: ranges == NULL, rangesCount == 0, targetFamilyNames == NULL, targetFamilyNamesCount == 0,
 *     scale < 0, or any NULL entry inside targetFamilyNames. Nothing else is validated at this point: localeName, baseFamilyName and
 *     the collection may be NULL, scale 0 is accepted, and the Unicode ranges are copied verbatim (first > last is accepted here);
 *   - AddMappings(NULL) is E_INVALIDARG; otherwise every mapping of the given (DirectWrite-created) fallback is appended;
 *   - CreateFontFallback zeroes the out pointer, snapshots the builder (later AddMapping calls do not change fallbacks made earlier)
 *     and returns a new DWriteFontFallback (IDWriteFontFallback + IDWriteFontFallback1);
 *   - text format / layout SetFontFallback AddRefs (NULL allowed), GetFontFallback returns the stored pointer (AddRef'd) or NULL, always S_OK. */

static IDWriteFontFallbackBuilder *mkb(IDWriteFactory *f)
{
    IDWriteFactory2 *f2 = ntf_f2(f); IDWriteFontFallbackBuilder *b = NULL;
    if (!f2) return NULL;
    if (FAILED(IDWriteFactory2_CreateFontFallbackBuilder(f2, &b))) b = NULL;
    IDWriteFactory2_Release(f2);
    return b;
}
#define NEED_B(b, f) IDWriteFontFallbackBuilder *b = mkb(f); if (!b) { t_skip("IDWriteFactory2::CreateFontFallbackBuilder unavailable"); return; }

static const DWRITE_UNICODE_RANGE R_HIRA = { 0x3040, 0x309f }, R_KANA = { 0x30a0, 0x30ff }, R_SNOW = { 0x2603, 0x2603 }, R_ARAB = { 0x0600, 0x06ff }, R_EMOJI = { 0x1f600, 0x1f64f };

static HRESULT add1(IDWriteFontFallbackBuilder *b, DWRITE_UNICODE_RANGE r, const WCHAR *target, IDWriteFontCollection *coll, const WCHAR *loc, const WCHAR *base, float scale)
{
    const WCHAR *t[1]; t[0] = target;
    return IDWriteFontFallbackBuilder_AddMapping(b, &r, 1, t, 1, coll, loc, base, scale);
}

static IDWriteFontFallback *mkfb(IDWriteFontFallbackBuilder *b)
{
    IDWriteFontFallback *fb = NULL;
    if (FAILED(IDWriteFontFallbackBuilder_CreateFontFallback(b, &fb))) fb = NULL;
    return fb;
}

TEST(fbb_create_basic)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f);
    EXPECT_REFS(b, 1);
    DW_RELEASE(b);
}

TEST(fbb_create_distinct_and_all_factory_versions)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFallbackBuilder *a = mkb(f), *b = mkb(f); int v;
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); t_skip("unavailable"); return; }
    CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    for (v = 2; v <= 7; v++) {
        void *fv = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v); IDWriteFontFallbackBuilder *x = NULL;
        if (!fv) { t_note("IDWriteFactory%d unsupported", v); continue; }
        CHECK_HR_OK(IDWriteFactory2_CreateFontFallbackBuilder((IDWriteFactory2 *)fv, &x)); CHECK(x != NULL); DW_RELEASE(x);
    }
}

TEST(fbb_qi_matrix)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f);
    NTF_QI_ONLY(b, &IID_IDWriteFontFallbackBuilder);
    DW_RELEASE(b);
}

TEST(fbb_refcount_protocol)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f); ULONG r;
    r = IDWriteFontFallbackBuilder_AddRef(b); EXPECT_EQ(r, 2);
    r = IDWriteFontFallbackBuilder_Release(b); EXPECT_EQ(r, 1);
    r = IDWriteFontFallbackBuilder_Release(b); EXPECT_EQ(r, 0);
}

TEST(fbb_outlives_factory)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontFallbackBuilder *b; IDWriteFontFallback *fb;
    if (!f) { t_skip("no isolated factory"); return; }
    b = mkb(f); DW_RELEASE(f);
    if (!b) { t_skip("unavailable"); return; }
    CHECK_HR_OK(add1(b, R_HIRA, L"Arial", NULL, NULL, NULL, 1.0f));
    fb = mkfb(b); CHECK(fb != NULL);
    DW_RELEASE(fb); DW_RELEASE(b);
}

/* ---- AddMapping validation ------------------------------------------------------------------------------------------------------ */

TEST(fbb_add_mapping_invalid_arguments)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f);
    DWRITE_UNICODE_RANGE rg[2] = { R_HIRA, R_KANA }; const WCHAR *t1[1] = { L"Arial" }; const WCHAR *t2[2] = { L"Arial", NULL }; const WCHAR *t3[2] = { NULL, L"Arial" };
    const WCHAR *t4[1] = { NULL };
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, NULL, 1, t1, 1, NULL, NULL, NULL, 1.0f), E_INVALIDARG);          /* NULL ranges */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 0, t1, 1, NULL, NULL, NULL, 1.0f), E_INVALIDARG);            /* zero ranges */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, NULL, 1, NULL, NULL, NULL, 1.0f), E_INVALIDARG);          /* NULL targets */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, t1, 0, NULL, NULL, NULL, 1.0f), E_INVALIDARG);            /* zero targets */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, t2, 2, NULL, NULL, NULL, 1.0f), E_INVALIDARG);            /* NULL second target */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, t3, 2, NULL, NULL, NULL, 1.0f), E_INVALIDARG);            /* NULL first target */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, t4, 1, NULL, NULL, NULL, 1.0f), E_INVALIDARG);
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, t1, 1, NULL, NULL, NULL, -0.5f), E_INVALIDARG);           /* negative scale */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, t1, 1, NULL, NULL, NULL, -1.0f), E_INVALIDARG);
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, t1, 1, NULL, NULL, NULL, -1e30f), E_INVALIDARG);
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, t1, 1, NULL, NULL, NULL, -INFINITY), E_INVALIDARG);
    DW_RELEASE(b);
}

TEST(fbb_add_mapping_null_target_after_valid_ones)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f); DWRITE_UNICODE_RANGE rg = R_HIRA; const WCHAR *t[4] = { L"A", L"B", L"C", NULL };
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, t, 4, NULL, NULL, NULL, 1.0f), E_INVALIDARG);      /* every entry is checked */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, t, 3, NULL, NULL, NULL, 1.0f), S_OK);              /* count 3 never reaches the NULL */
    DW_RELEASE(b);
}

TEST(fbb_add_mapping_optional_arguments_may_be_null)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f);
    CHECK_HR_OK(add1(b, R_HIRA, L"Arial", NULL, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, R_HIRA, L"Arial", NULL, L"ja-JP", NULL, 1.0f));
    CHECK_HR_OK(add1(b, R_HIRA, L"Arial", NULL, NULL, L"Segoe UI", 1.0f));
    CHECK_HR_OK(add1(b, R_HIRA, L"Arial", NULL, L"", L"", 1.0f));
    DW_RELEASE(b);
}

TEST(fbb_add_mapping_scale_values)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f); unsigned i;
    static const float sc[] = { 0.0f, 0.001f, 0.5f, 1.0f, 1.5f, 2.0f, 100.0f, 1e30f, 3.4e38f };
    for (i = 0; i < sizeof sc / sizeof sc[0]; i++) CHECKF(add1(b, R_HIRA, L"Arial", NULL, NULL, NULL, sc[i]) == S_OK, "scale %g", sc[i]);      /* only negative scales are rejected */
    DW_RELEASE(b);
}

TEST(fbb_add_mapping_unvalidated_ranges)
{
    /* AddMapping copies the ranges without looking at them. */
    IDWriteFactory *f = dw_shared(); NEED_B(b, f);
    DWRITE_UNICODE_RANGE inv = { 0x3099, 0x3040 }, one = { 0x41, 0x41 }, beyond = { 0x110000, 0xffffffffu }, zero = { 0, 0 }, full = { 0, 0x10ffff };
    CHECK_HR_OK(add1(b, inv, L"Arial", NULL, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, one, L"Arial", NULL, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, beyond, L"Arial", NULL, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, zero, L"Arial", NULL, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, full, L"Arial", NULL, NULL, NULL, 1.0f));
    DW_RELEASE(b);
}

TEST(fbb_add_mapping_many_ranges_and_targets)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f); DWRITE_UNICODE_RANGE rg[64]; const WCHAR *tg[16]; int i;
    static const WCHAR *names[16] = { L"A0", L"A1", L"A2", L"A3", L"A4", L"A5", L"A6", L"A7", L"A8", L"A9", L"B0", L"B1", L"B2", L"B3", L"B4", L"B5" };
    for (i = 0; i < 64; i++) { rg[i].first = 0x4e00 + i * 0x10; rg[i].last = 0x4e00 + i * 0x10 + 0xf; }
    for (i = 0; i < 16; i++) tg[i] = names[i];
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, rg, 64, tg, 16, NULL, L"ja", L"Arial", 1.0f), S_OK);
    for (i = 0; i < 100; i++) CHECK_HR(add1(b, R_HIRA, L"Arial", NULL, NULL, NULL, 1.0f), S_OK);
    DW_RELEASE(b);
}

TEST(fbb_add_mapping_caller_memory_is_copied)
{
    NTF_BEGIN(); NEED_B(b, f); WCHAR name[16]; DWRITE_UNICODE_RANGE rg = R_HIRA; const WCHAR *tg[1]; IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64];
    wcscpy(name, NTF_JA); tg[0] = name;
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, tg, 1, X.coll, NULL, NULL, 1.0f), S_OK);
    wcscpy(name, L"Overwritten"); rg.first = 0; rg.last = 0; tg[0] = NULL;        /* builder must have copied everything */
    fb = mkfb(b);
    if (fb) {
        CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        EXPECT_EQ(ml, 1);
        if (font) { ntf_font_family(font, fam, sizeof fam); EXPECT_STR(fam, "NtfJa"); } else CHECK(font != NULL);
        DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

/* ---- AddMappings -------------------------------------------------------------------------------------------------------------------- */

TEST(fbb_add_mappings_null)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f);
    CHECK_HR(IDWriteFontFallbackBuilder_AddMappings(b, NULL), E_INVALIDARG);
    DW_RELEASE(b);
}

TEST(fbb_add_mappings_system_fallback)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f); IDWriteFactory2 *f2 = ntf_f2(f); IDWriteFontFallback *sys = NULL;
    if (!f2) { DW_RELEASE(b); t_skip("no factory2"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &sys));
    if (sys) {
        CHECK_HR(IDWriteFontFallbackBuilder_AddMappings(b, sys), S_OK);
        EXPECT_REFS(sys, 1);                       /* the builder copies the mapping data, it does not keep the object */
    }
    DW_RELEASE(sys); IDWriteFactory2_Release(f2); DW_RELEASE(b);
}

TEST(fbb_add_mappings_from_custom_fallback)
{
    NTF_BEGIN(); NEED_B(b1, f); IDWriteFontFallbackBuilder *b2 = mkb(f); IDWriteFontFallback *fb1, *fb2; UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64];
    CHECK_HR_OK(add1(b1, R_HIRA, NTF_JA, X.coll, NULL, NULL, 1.0f));
    fb1 = mkfb(b1);
    if (b2 && fb1) {
        CHECK_HR(IDWriteFontFallbackBuilder_AddMappings(b2, fb1), S_OK);
        CHECK_HR_OK(add1(b2, R_SNOW, NTF_SYM, X.coll, NULL, NULL, 1.0f));
        fb2 = mkfb(b2);
        if (fb2) {
            CHECK_HR(ntf_map(fb2, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
            if (font) { ntf_font_family(font, fam, sizeof fam); EXPECT_STR(fam, "NtfJa"); } else CHECK(font != NULL);
            DW_RELEASE(font);
            CHECK_HR(ntf_map(fb2, L"\x2603", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
            if (font) { ntf_font_family(font, fam, sizeof fam); EXPECT_STR(fam, "NtfSym"); } else CHECK(font != NULL);
            DW_RELEASE(font);
            DW_RELEASE(fb2);
        }
    }
    DW_RELEASE(fb1); DW_RELEASE(b1); DW_RELEASE(b2); NTF_END();
}

TEST(fbb_add_mappings_self_copy)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc;
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b);
    if (fb) {
        CHECK_HR(IDWriteFontFallbackBuilder_AddMappings(b, fb), S_OK);     /* a fallback built from this builder can be fed back */
        DW_RELEASE(fb); fb = mkfb(b);
        if (fb) { CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK(font != NULL); DW_RELEASE(font); }
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

/* ---- CreateFontFallback + behaviour ------------------------------------------------------------------------------------------------- */

TEST(fbb_create_fallback_from_empty_builder)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb = (IDWriteFontFallback *)(void *)0; UINT32 ml; IDWriteFont *font = NULL; float sc;
    fb = NULL; CHECK_HR_OK(IDWriteFontFallbackBuilder_CreateFontFallback(b, &fb)); CHECK(fb != NULL);
    if (fb) {
        EXPECT_REFS(fb, 1);
        if (!t_is_wine()) {                            /* Wine dereferences a NULL mapping table here */
            CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
            CHECK(font == NULL);                       /* no mapping: nothing can render U+3042 (the base font does not cover it) */
            CHECK(ml >= 1 && ml <= 1); CHECK_FLOAT_EQ(sc, 1.0f);
            DW_RELEASE(font);
        }
        CHECK_HR(ntf_map(fb, L"abc", 3, 0, 3, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        EXPECT_EQ(ml, 3);                              /* the base family still handles what it covers */
        DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_create_fallback_qi_and_refs)
{
    IDWriteFactory *f = dw_shared(); NEED_B(b, f); IDWriteFontFallback *fb; IDWriteFontFallback *fb2;
    CHECK_HR_OK(add1(b, R_HIRA, L"Arial", NULL, NULL, NULL, 1.0f));
    fb = mkfb(b); fb2 = mkfb(b);
    CHECK(fb && fb2 && fb != fb2);
    if (fb) NTF_QI_ONLY(fb, &IID_IDWriteFontFallback, &IID_IDWriteFontFallback1);
    EXPECT_REFS(b, 1);
    DW_RELEASE(fb); DW_RELEASE(fb2); DW_RELEASE(b);
}

TEST(fbb_fallback_survives_builder_release)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc;
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b); DW_RELEASE(b);
    if (fb) { CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK(font != NULL); DW_RELEASE(font); }
    DW_RELEASE(fb); NTF_END();
}

TEST(fbb_fallback_is_a_snapshot)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb1, *fb2; UINT32 ml; IDWriteFont *font = NULL; float sc;
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 1.0f));
    fb1 = mkfb(b);
    CHECK_HR_OK(add1(b, R_SNOW, NTF_SYM, X.coll, NULL, NULL, 1.0f));
    fb2 = mkfb(b);
    if (fb1 && fb2) {
        CHECK_HR(ntf_map(fb1, L"\x2603", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK(font == NULL); DW_RELEASE(font);       /* fb1 predates the second mapping */
        CHECK_HR(ntf_map(fb2, L"\x2603", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK(font != NULL); DW_RELEASE(font);
        CHECK_HR(ntf_map(fb2, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK(font != NULL); DW_RELEASE(font);
    }
    DW_RELEASE(fb1); DW_RELEASE(fb2); DW_RELEASE(b); NTF_END();
}

typedef struct { const WCHAR *text; UINT32 first_len; const char *fam; } map_row;

TEST(fbb_custom_mapping_selects_target_family)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; unsigned i;
    static const map_row rows[] = {
        { L"\x3042", 1, "NtfJa" }, { L"\x3044\x3046", 2, "NtfJa" }, { L"\x4e00", 1, "NtfJa" }, { L"\x2603", 1, "NtfSym" }, { L"\x2605", 1, "NtfSym" },
        { L"\x0627\x0628", 2, "NtfAr" }, { L"\xD83D\xDE00", 2, "NtfEmoji" }, { L"\xD83D\xDE01", 2, "NtfEmoji" },
    };
    DWRITE_UNICODE_RANGE ja[2] = { { 0x3040, 0x30ff }, { 0x4e00, 0x9fff } }; const WCHAR *jt[1] = { NTF_JA };
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, ja, 2, jt, 1, X.coll, NULL, NULL, 1.0f), S_OK);
    CHECK_HR_OK(add1(b, R_SNOW, NTF_SYM, X.coll, NULL, NULL, 1.0f));
    { DWRITE_UNICODE_RANGE s = { 0x2605, 0x2605 }; CHECK_HR_OK(add1(b, s, NTF_SYM, X.coll, NULL, NULL, 1.0f)); }
    CHECK_HR_OK(add1(b, R_ARAB, NTF_AR, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, R_EMOJI, NTF_EMOJI, X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) {
        UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64] = "";
        CHECK_HR(ntf_map(fb, rows[i].text, (UINT32)wcslen(rows[i].text), 0, (UINT32)wcslen(rows[i].text), X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        if (font) ntf_font_family(font, fam, sizeof fam);
        CHECKF(ml == rows[i].first_len && !strcmp(fam, rows[i].fam), "row %u: mappedLength %u family \"%s\" want %u \"%s\"", i, ml, fam, rows[i].first_len, rows[i].fam);
        CHECK_FLOAT_EQ(sc, 1.0f);
        DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_mapping_range_boundaries)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; DWRITE_UNICODE_RANGE one = { 0x3042, 0x3044 };
    static const struct { UINT32 cp; int mapped; } rows[] = { { 0x3042, 1 }, { 0x3044, 1 }, { 0x3046, 0 } };
    unsigned i;
    CHECK_HR_OK(add1(b, one, NTF_JA, X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b);
    for (i = 0; fb && i < 3; i++) {
        WCHAR t[2] = { (WCHAR)rows[i].cp, 0 }; UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64] = "";
        CHECK_HR(ntf_map(fb, t, 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        if (font) ntf_font_family(font, fam, sizeof fam);
        CHECKF((font != NULL) == rows[i].mapped, "U+%04X: mapped=%d (range ends are inclusive)", rows[i].cp, font != NULL);
        if (rows[i].mapped) EXPECT_STR(fam, "NtfJa");
        DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_mapping_run_splits_between_ranges)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64];
    static const WCHAR t[] = L"\x3042\x2603\x2603\x3044";
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, R_SNOW, NTF_SYM, X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b);
    if (fb) {
        static const UINT32 want[3] = { 1, 2, 1 }; static const char *wf[3] = { "NtfJa", "NtfSym", "NtfJa" }; UINT32 pos = 0; int k;
        for (k = 0; k < 3; k++) {
            CHECK_HR(ntf_map(fb, t, 4, pos, 4 - pos, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
            fam[0] = 0; if (font) ntf_font_family(font, fam, sizeof fam);
            CHECKF(ml == want[k] && !strcmp(fam, wf[k]), "step %d: mappedLength %u family \"%s\"", k, ml, fam);
            DW_RELEASE(font); pos += ml;
        }
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_mapping_scale_is_returned)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc = 0;
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 0.5f));
    CHECK_HR_OK(add1(b, R_SNOW, NTF_SYM, X.coll, NULL, NULL, 2.0f));
    fb = mkfb(b);
    if (fb) {
        CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK_FLOAT_EQ(sc, 0.5f); DW_RELEASE(font);
        CHECK_HR(ntf_map(fb, L"\x2603", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK_FLOAT_EQ(sc, 2.0f); DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_first_available_target_wins)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64] = "";
    DWRITE_UNICODE_RANGE rg = R_HIRA; const WCHAR *tg[3] = { L"No Such Family One", L"No Such Family Two", NTF_JA };
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, tg, 3, X.coll, NULL, NULL, 1.0f), S_OK);
    fb = mkfb(b);
    if (fb) {
        CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        if (font) ntf_font_family(font, fam, sizeof fam);
        EXPECT_STR(fam, "NtfJa");                          /* names are tried in order until one exists and covers the text */
        DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_target_order_prefers_first_covering_family)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64] = "";
    DWRITE_UNICODE_RANGE rg = { 0x2603, 0x3046 }; const WCHAR *tg[2] = { NTF_SYM, NTF_JA };
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, tg, 2, X.coll, NULL, NULL, 1.0f), S_OK);
    fb = mkfb(b);
    if (fb) {
        CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);      /* NtfSym is first but lacks U+3042 */
        if (font) ntf_font_family(font, fam, sizeof fam);
        EXPECT_STR(fam, "NtfJa"); DW_RELEASE(font);
        CHECK_HR(ntf_map(fb, L"\x2603", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        fam[0] = 0; if (font) ntf_font_family(font, fam, sizeof fam);
        EXPECT_STR(fam, "NtfSym"); DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_unknown_target_family_maps_nothing)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc;
    CHECK_HR_OK(add1(b, R_HIRA, L"No Such Family Anywhere", X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b);
    if (fb) { CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK(font == NULL); CHECK(ml >= 1); DW_RELEASE(font); }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_target_that_lacks_the_character)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc;
    CHECK_HR_OK(add1(b, R_HIRA, NTF_SYM, X.coll, NULL, NULL, 1.0f));      /* NtfSym covers no hiragana */
    fb = mkfb(b);
    if (fb) { CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK(font == NULL); DW_RELEASE(font); }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_base_family_filter)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc;
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NTF_AR, 1.0f));      /* only for text whose base family is NtfAr */
    fb = mkfb(b);
    if (fb) {
        CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_AR, &ml, &font, &sc, NULL), S_OK); CHECK(font != NULL); DW_RELEASE(font);
        CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK); CHECK(font == NULL); DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_locale_filter_matches_source_locale)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml = 0; IDWriteFont *font = NULL; float sc; dwx_ana_src *s;
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, L"ja-JP", NULL, 1.0f));
    fb = mkfb(b);
    if (fb) {
        s = dwx_ana_src_new(L"\x3042", 1); wcscpy(s->locale, L"ja-JP");
        CHECK_HR(IDWriteFontFallback_MapCharacters(fb, (IDWriteTextAnalysisSource *)s, 0, 1, X.coll, NTF_MAIN, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &ml, &font, &sc), S_OK);
        CHECK(font != NULL); DW_RELEASE(font); EXPECT_EQ(ml, 1);
        dwx_release(s);
        if (!t_is_wine()) {                            /* Wine crashes when no mapping applies */
            s = dwx_ana_src_new(L"\x3042", 1); wcscpy(s->locale, L"de-DE");
            CHECK_HR(IDWriteFontFallback_MapCharacters(fb, (IDWriteTextAnalysisSource *)s, 0, 1, X.coll, NTF_MAIN, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &ml, &font, &sc), S_OK);
            t_note("mapping tagged ja-JP with de-DE text: font %s", font ? "found" : "NULL");
            DW_RELEASE(font); dwx_release(s);
        }
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_duplicate_range_priority_note)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64] = "";
    CHECK_HR_OK(add1(b, R_HIRA, NTF_SYM, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b);
    if (fb) {
        CHECK_HR(ntf_map(fb, L"\x3042", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        if (font) ntf_font_family(font, fam, sizeof fam);
        t_note("two mappings for one range, first target lacks the character: family \"%s\"", fam);     /* priority between overlapping mappings is not pinned down */
        CHECK(fam[0] == 0 || !strcmp(fam, "NtfJa"));
        DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_null_collection_means_system_collection)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64] = "";
    if (!dw_have_font(dw_shared(), L"Tahoma")) { DW_RELEASE(b); NTF_END(); t_skip("Tahoma not installed"); return; }
    CHECK_HR_OK(add1(b, R_ARAB, L"Tahoma", NULL, NULL, NULL, 1.0f));
    fb = mkfb(b);
    if (fb) {
        CHECK_HR(ntf_map(fb, L"\x0627", 1, 0, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        if (font) ntf_font_family(font, fam, sizeof fam);
        EXPECT_STR(fam, "Tahoma");
        DW_RELEASE(font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_fallback1_returns_face_of_target)
{
    NTF_BEGIN(); NEED_B(b, f); IDWriteFontFallback *fb; IDWriteFontFallback1 *fb1 = NULL; IDWriteFontFace5 *face = NULL; UINT32 ml; float sc; char fam[64] = "";
    dwx_ana_src *s = dwx_ana_src_new(L"\x3042\x3044", 2);
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 0.75f));
    fb = mkfb(b);
    if (fb && SUCCEEDED(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&fb1)) && fb1) {
        CHECK_HR(IDWriteFontFallback1_MapCharacters(fb1, (IDWriteTextAnalysisSource *)s, 0, 2, X.coll, NTF_MAIN, NULL, 0, &ml, &sc, &face), S_OK);
        EXPECT_EQ(ml, 2); CHECK_FLOAT_EQ(sc, 0.75f);
        if (face) { ntf_face_family(face, fam, sizeof fam); EXPECT_STR(fam, "NtfJa"); IDWriteFontFace5_Release(face); } else CHECK(face != NULL);
    }
    DW_RELEASE(fb1); dwx_release(s); DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

/* ---- text format / text layout integration ------------------------------------------------------------------------------------------ */

static IDWriteTextFormat1 *fmt1(ntf_fx *X, IDWriteTextFormat **base)
{
    IDWriteTextFormat1 *f1 = NULL;
    *base = ntf_format(X, NTF_MAIN, 20);
    if (!*base) return NULL;
    if (FAILED(IDWriteTextFormat_QueryInterface(*base, &IID_IDWriteTextFormat1, (void **)&f1))) f1 = NULL;
    return f1;
}

TEST(fbb_format_get_unset_is_null)
{
    NTF_BEGIN(); IDWriteTextFormat *fm; IDWriteTextFormat1 *f1 = fmt1(&X, &fm); IDWriteFontFallback *fb = (IDWriteFontFallback *)(void *)1;
    if (!f1) { DW_RELEASE(fm); NTF_END(); t_skip("IDWriteTextFormat1 unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(f1, &fb));
    CHECK(fb == NULL);                                  /* the format stores nothing until SetFontFallback is called */
    DW_RELEASE(f1); DW_RELEASE(fm); NTF_END();
}

TEST(fbb_format_set_get_refcounts)
{
    NTF_BEGIN(); IDWriteTextFormat *fm; IDWriteTextFormat1 *f1 = fmt1(&X, &fm); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb, *g = NULL, *fb2;
    if (!f1 || !b) { DW_RELEASE(f1); DW_RELEASE(fm); DW_RELEASE(b); NTF_END(); t_skip("unavailable"); return; }
    fb = mkfb(b); fb2 = mkfb(b);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(f1, fb));
    EXPECT_REFS(fb, 2);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(f1, &g)); CHECK(g == fb); EXPECT_REFS(fb, 3); DW_RELEASE(g); EXPECT_REFS(fb, 2);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(f1, fb2));
    EXPECT_REFS(fb, 1); EXPECT_REFS(fb2, 2);            /* the previous fallback is released */
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(f1, NULL));
    EXPECT_REFS(fb2, 1);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(f1, &g)); CHECK(g == NULL);
    DW_RELEASE(fb); DW_RELEASE(fb2); DW_RELEASE(b); DW_RELEASE(f1); DW_RELEASE(fm); NTF_END();
}

TEST(fbb_format_releases_fallback_on_destruction)
{
    NTF_BEGIN(); IDWriteTextFormat *fm; IDWriteTextFormat1 *f1 = fmt1(&X, &fm); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb;
    if (!f1 || !b) { DW_RELEASE(f1); DW_RELEASE(fm); DW_RELEASE(b); NTF_END(); t_skip("unavailable"); return; }
    fb = mkfb(b);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(f1, fb)); EXPECT_REFS(fb, 2);
    DW_RELEASE(f1); DW_RELEASE(fm);
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb); DW_RELEASE(b); NTF_END();
}

TEST(fbb_system_fallback_object_is_accepted_by_format)
{
    NTF_BEGIN(); IDWriteTextFormat *fm; IDWriteTextFormat1 *f1 = fmt1(&X, &fm); IDWriteFactory2 *f2 = ntf_f2(f); IDWriteFontFallback *sys = NULL, *g = NULL;
    if (!f1 || !f2) { DW_RELEASE(f1); DW_RELEASE(fm); if (f2) IDWriteFactory2_Release(f2); NTF_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &sys));
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(f1, sys));
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(f1, &g)); CHECK(g == sys);
    DW_RELEASE(g); DW_RELEASE(sys); IDWriteFactory2_Release(f2); DW_RELEASE(f1); DW_RELEASE(fm); NTF_END();
}

static IDWriteTextLayout2 *lay2(ntf_fx *X, const WCHAR *text, float size)
{
    IDWriteTextLayout *l = ntf_layout(X, NTF_MAIN, text, size, 1000, 200); IDWriteTextLayout2 *l2 = NULL;
    if (!l) return NULL;
    if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2))) l2 = NULL;
    IDWriteTextLayout_Release(l);
    return l2;
}

TEST(fbb_layout_get_unset_is_null)
{
    NTF_BEGIN(); IDWriteTextLayout2 *l = lay2(&X, L"abc", 20); IDWriteFontFallback *fb = (IDWriteFontFallback *)(void *)1;
    if (!l) { NTF_END(); t_skip("IDWriteTextLayout2 unavailable"); return; }
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l, &fb));
    CHECK(fb == NULL);
    IDWriteTextLayout2_Release(l); NTF_END();
}

TEST(fbb_layout_set_get_refcounts)
{
    NTF_BEGIN(); IDWriteTextLayout2 *l = lay2(&X, L"abc", 20); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb, *g = NULL;
    if (!l || !b) { if (l) IDWriteTextLayout2_Release(l); DW_RELEASE(b); NTF_END(); t_skip("unavailable"); return; }
    fb = mkfb(b);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l, fb)); EXPECT_REFS(fb, 2);
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l, &g)); CHECK(g == fb); EXPECT_REFS(fb, 3); DW_RELEASE(g);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l, NULL)); EXPECT_REFS(fb, 1);
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l, &g)); CHECK(g == NULL);
    DW_RELEASE(fb); DW_RELEASE(b); IDWriteTextLayout2_Release(l); NTF_END();
}

TEST(fbb_layout_inherits_or_not_note)
{
    NTF_BEGIN(); IDWriteTextFormat *fm; IDWriteTextFormat1 *f1 = fmt1(&X, &fm); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb, *g = NULL; IDWriteTextLayout *l = NULL; IDWriteTextLayout2 *l2 = NULL;
    if (!f1 || !b) { DW_RELEASE(f1); DW_RELEASE(fm); DW_RELEASE(b); NTF_END(); t_skip("unavailable"); return; }
    fb = mkfb(b);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(f1, fb));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fm, 100, 100, &l));
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2)) && l2) {
        CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g));
        t_note("layout created after TextFormat1::SetFontFallback: GetFontFallback %s the format's object", g == fb ? "returns" : (g ? "returns another than" : "returns NULL, not"));
        DW_RELEASE(g); IDWriteTextLayout2_Release(l2);
    }
    DW_RELEASE(l); DW_RELEASE(fb); DW_RELEASE(b); DW_RELEASE(f1); DW_RELEASE(fm); NTF_END();
}

/* Draw: which font each glyph run uses. NtfMain letters, then hiragana/snowman through the fallback. */
static int draw_runs(IDWriteTextLayout2 *l, dwx_renderer **rr)
{
    dwx_renderer *r = dwx_renderer_new(); HRESULT hr = IDWriteTextLayout2_Draw(l, (void *)(ULONG_PTR)1, (IDWriteTextRenderer *)r, 0, 0);
    if (FAILED(hr)) t_note("Draw: %s", dw_hr_name(hr));
    *rr = r; return (int)r->count[DWX_DRAW_GLYPHRUN];
}

TEST(fbb_draw_uses_custom_fallback_font)
{
    NTF_BEGIN(); IDWriteTextLayout2 *l = lay2(&X, L"ac\x3042\x2603" L"c", 20); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb; dwx_renderer *r; int n; char fam[64];
    if (!l || !b) { if (l) IDWriteTextLayout2_Release(l); DW_RELEASE(b); NTF_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(add1(b, R_SNOW, NTF_SYM, X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l, fb));
    n = draw_runs(l, &r);
    EXPECT_EQ(n, 4);
    if (n == 4) {
        const dwx_draw *d; int cur = 0; static const char *want[4] = { "NtfMain", "NtfJa", "NtfSym", "NtfMain" }; static const unsigned short g[4] = { 0, 2, 2, 0 }; int k = 0;
        while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
            ntf_run_family(r, k, fam, sizeof fam);
            CHECKF(!strcmp(fam, want[k]), "run %d font family \"%s\" want %s", k, fam, want[k]);
            if (k == 0) { EXPECT_EQ(d->nglyphs, 2); if (d->nglyphs == 2) { EXPECT_EQ(d->indices[0], 28); EXPECT_EQ(d->indices[1], 30); } }
            else if (k == 1 || k == 2) { EXPECT_EQ(d->nglyphs, 1); if (d->nglyphs == 1) EXPECT_EQ(d->indices[0], g[k]); }
            else { EXPECT_EQ(d->nglyphs, 1); if (d->nglyphs == 1) EXPECT_EQ(d->indices[0], 30); }
            CHECK_NEAR(d->em, 20.0f, 0.001);
            k++;
        }
    }
    dwx_release(r); DW_RELEASE(fb); DW_RELEASE(b); IDWriteTextLayout2_Release(l); NTF_END();
}

TEST(fbb_draw_mapping_scale_scales_font_size)
{
    NTF_BEGIN(); IDWriteTextLayout2 *l = lay2(&X, L"a\x3042", 20); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb; dwx_renderer *r; int n;
    if (!l || !b) { if (l) IDWriteTextLayout2_Release(l); DW_RELEASE(b); NTF_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 0.5f));
    fb = mkfb(b);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l, fb));
    n = draw_runs(l, &r);
    EXPECT_EQ(n, 2);
    if (n == 2) {
        const dwx_draw *d0 = dwx_renderer_call(r, 0), *d1 = NULL; int cur = 0; (void)d0;
        d1 = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur); d1 = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
        CHECK(d1 != NULL);
        if (d1) { CHECK_NEAR(d1->em, 10.0f, 0.001); CHECK_NEAR(d1->advances[0], NTF_ADV(1000, 10), 0.01); }     /* font size times the mapping scale */
    }
    dwx_release(r); DW_RELEASE(fb); DW_RELEASE(b); IDWriteTextLayout2_Release(l); NTF_END();
}

TEST(fbb_draw_without_mapping_uses_base_notdef)
{
    NTF_BEGIN(); IDWriteTextLayout2 *l = lay2(&X, L"a\x3042", 20); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb; dwx_renderer *r; int n; char fam[64];
    if (!l || !b) { if (l) IDWriteTextLayout2_Release(l); DW_RELEASE(b); NTF_END(); t_skip("unavailable"); return; }
    if (t_is_wine()) { DW_RELEASE(b); IDWriteTextLayout2_Release(l); NTF_END(); t_skip("Wine crashes when no fallback mapping applies"); return; }
    fb = mkfb(b);                                       /* empty fallback */
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l, fb));
    n = draw_runs(l, &r);
    t_note("empty fallback: %d glyph runs", n);
    CHECK(n >= 1);
    if (n >= 1) {
        const dwx_draw *d; int cur = 0, k = 0;
        while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
            ntf_run_family(r, k++, fam, sizeof fam);
            CHECKF(!strcmp(fam, "NtfMain"), "run uses \"%s\"", fam);       /* nothing else is available: the base font draws .notdef */
            if (d->nglyphs == 1 && d->indices[0] != 28) EXPECT_EQ(d->indices[0], 0);
        }
    }
    dwx_release(r); DW_RELEASE(fb); DW_RELEASE(b); IDWriteTextLayout2_Release(l); NTF_END();
}

TEST(fbb_draw_changing_fallback_changes_runs)
{
    NTF_BEGIN(); IDWriteTextLayout2 *l = lay2(&X, L"\x3042", 20); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb; dwx_renderer *r; int n; char fam[64];
    if (!l || !b) { if (l) IDWriteTextLayout2_Release(l); DW_RELEASE(b); NTF_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(add1(b, R_HIRA, NTF_JA, X.coll, NULL, NULL, 1.0f));
    fb = mkfb(b);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l, fb));
    n = draw_runs(l, &r); EXPECT_EQ(n, 1); ntf_run_family(r, 0, fam, sizeof fam); EXPECT_STR(fam, "NtfJa"); dwx_release(r);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l, NULL));       /* back to the system fallback */
    n = draw_runs(l, &r); ntf_run_family(r, 0, fam, sizeof fam);
    t_note("after SetFontFallback(NULL) U+3042 draws with \"%s\"", fam);
    dwx_release(r); DW_RELEASE(fb); DW_RELEASE(b); IDWriteTextLayout2_Release(l); NTF_END();
}

TEST(fbb_draw_fallback_font_metrics_via_layout_width)
{
    /* NtfJa glyphs are 1000 units wide like NtfMain's, so a mapped run keeps the same width at scale 1 and halves at scale 0.5 */
    NTF_BEGIN(); IDWriteFontFallbackBuilder *b = mkb(f); IDWriteFontFallback *fb; IDWriteTextLayout2 *l; DWRITE_TEXT_METRICS m; int pass;
    if (!b) { NTF_END(); t_skip("unavailable"); return; }
    for (pass = 0; pass < 2; pass++) {
        IDWriteFontFallbackBuilder *bb = mkb(f);
        CHECK_HR_OK(add1(bb, R_HIRA, NTF_JA, X.coll, NULL, NULL, pass ? 0.5f : 1.0f));
        fb = mkfb(bb); l = lay2(&X, L"\x3042\x3044", 20);
        if (l && fb) {
            CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l, fb));
            CHECK_HR_OK(IDWriteTextLayout2_GetMetrics(l, &m));
            CHECK_NEAR(m.width, NTF_ADV(2000, pass ? 10 : 20), 0.02);
        }
        if (l) IDWriteTextLayout2_Release(l);
        DW_RELEASE(fb); DW_RELEASE(bb);
    }
    DW_RELEASE(b); NTF_END();
}

TEST(fbb_draw_system_fallback_default_has_cjk)
{
    NTF_BEGIN(); IDWriteTextLayout2 *l = lay2(&X, L"\x4e00", 20); dwx_renderer *r; int n; char fam[64];
    if (!l) { NTF_END(); t_skip("no layout"); return; }
    n = draw_runs(l, &r);
    ntf_run_family(r, 0, fam, sizeof fam);
    t_note("default system fallback draws U+4E00 with \"%s\" (%d runs)", fam, n);
    CHECK(n >= 1);
    dwx_release(r); IDWriteTextLayout2_Release(l); NTF_END();
}
