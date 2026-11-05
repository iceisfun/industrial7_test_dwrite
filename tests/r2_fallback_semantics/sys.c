#include "r2fs.h"

/* IDWriteFactory2::GetSystemFontFallback on Windows 10 (IDWriteFactory::GetSystemFontFallback):
 *   - *fallback is set to NULL first, then a NEW IDWriteFontFallback object is created on every call (distinct pointers, refcount 1);
 *   - the object implements IUnknown, IDWriteFontFallback and IDWriteFontFallback1 (the same primary vtable, so the pointers are equal)
 *     and an internal private interface that is not part of the public interface list; every other public IID gives E_NOINTERFACE.
 * The mapping data comes from the OS font fallback resource; a stock Windows 10 installs fonts for all major scripts, so for those the
 * returned font must exist and must contain the character. Which family is picked is font-set and locale dependent: notes only. */

static IDWriteFactory2 *shared2(void) { return (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2); }

static IDWriteFontFallback *sysfb(void)
{
    IDWriteFactory2 *f2 = shared2(); IDWriteFontFallback *fb = NULL;
    if (!f2 || FAILED(IDWriteFactory2_GetSystemFontFallback(f2, &fb))) return NULL;
    return fb;
}
#define NEED_SYS(fb) IDWriteFontFallback *fb = sysfb(); if (!fb) { t_skip("IDWriteFactory2::GetSystemFontFallback unavailable"); return; }

static IDWriteFontCollection *syscoll(IDWriteFactory *f)
{
    IDWriteFontCollection *c = NULL;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE))) return NULL;
    return c;
}

RT(sys_get_returns_new_object_with_one_reference)
{
    IDWriteFactory2 *f2 = shared2(); IDWriteFontFallback *fb = (IDWriteFontFallback *)(ULONG_PTR)1;
    if (!f2) { t_skip("IDWriteFactory2 unavailable"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &fb));
    CHECK(fb != NULL && fb != (IDWriteFontFallback *)(ULONG_PTR)1);
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb);
}

RT(sys_get_gives_a_distinct_object_each_time)
{
    IDWriteFactory2 *f2 = shared2(); IDWriteFontFallback *a = NULL, *b = NULL, *c = NULL;
    if (!f2) { t_skip("IDWriteFactory2 unavailable"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &a));
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &b));
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &c));
    CHECK(a != b && b != c && a != c);
    EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); EXPECT_REFS(c, 1);
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
}

RT(sys_get_from_every_factory_version_and_type)
{
    int v, t;
    for (t = 0; t < 2; t++) {
        for (v = 2; v <= 7; v++) {
            void *fv = dw_factory_v(t ? DWRITE_FACTORY_TYPE_ISOLATED : DWRITE_FACTORY_TYPE_SHARED, v); IDWriteFontFallback *fb = NULL; IDWriteFontFallback1 *fb1 = NULL;
            if (!fv) { t_note("IDWriteFactory%d (%s) unsupported", v, t ? "isolated" : "shared"); continue; }
            CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback((IDWriteFactory2 *)fv, &fb));
            CHECK(fb != NULL);
            if (fb) { CHECK_HR_OK(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&fb1)); CHECK(fb1 != NULL); }
            DW_RELEASE(fb1); DW_RELEASE(fb);
        }
    }
}

RT(sys_qi_supports_exactly_the_documented_interfaces)
{
    NEED_SYS(fb);
    int n, i; const dw_qi_row *rows = dw_iid_table(&n);
    for (i = 0; i < n; i++) {
        int ok = IsEqualIID(rows[i].iid, &IID_IUnknown) || IsEqualIID(rows[i].iid, &IID_IDWriteFontFallback) || IsEqualIID(rows[i].iid, &IID_IDWriteFontFallback1);
        void *pv = (void *)(ULONG_PTR)1; HRESULT hr = IDWriteFontFallback_QueryInterface(fb, rows[i].iid, &pv);
        if (ok) { t_check(hr == S_OK && pv != NULL, __FILE__, __LINE__, rows[i].name, "QI of a supported interface: %s", dw_hr_name(hr)); if (SUCCEEDED(hr) && pv) IUnknown_Release((IUnknown *)pv); }
        else { t_check(hr == E_NOINTERFACE && pv == NULL, __FILE__, __LINE__, rows[i].name, "QI of an unsupported interface: %s ptr %p", dw_hr_name(hr), pv); if (SUCCEEDED(hr) && pv) IUnknown_Release((IUnknown *)pv); }
    }
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb);
}

RT(sys_qi_pointers_are_one_identity)
{
    NEED_SYS(fb);
    IDWriteFontFallback1 *fb1 = NULL; IUnknown *u = NULL, *u1 = NULL; IDWriteFontFallback *again = NULL;
    CHECK_HR_OK(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&fb1));
    CHECK_HR_OK(IDWriteFontFallback_QueryInterface(fb, &IID_IUnknown, (void **)&u));
    CHECK_HR_OK(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback, (void **)&again));
    if (fb1) CHECK_HR_OK(IDWriteFontFallback1_QueryInterface(fb1, &IID_IUnknown, (void **)&u1));
    CHECK((void *)fb1 == (void *)fb);                         /* IDWriteFontFallback1 extends IDWriteFontFallback: one vtable pointer */
    t_note("IUnknown pointer %s the IDWriteFontFallback pointer", u == (IUnknown *)fb ? "equals" : "differs from"); CHECK(u1 == u); CHECK(again == fb);
    EXPECT_REFS(fb, 5);
    DW_RELEASE(u1); DW_RELEASE(again); DW_RELEASE(u); DW_RELEASE(fb1);
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb);
}

RT(sys_qi_failure_zeroes_the_out_pointer)
{
    NEED_SYS(fb);
    void *pv = (void *)(ULONG_PTR)0x1234; HRESULT hr;
    hr = IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFactory, &pv);
    CHECK(hr == E_NOINTERFACE && pv == NULL);
    pv = (void *)(ULONG_PTR)0x1234;
    hr = IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallbackBuilder, &pv);
    CHECK(hr == E_NOINTERFACE && pv == NULL);
    DW_RELEASE(fb);
}

RT(sys_addref_release_return_values)
{
    NEED_SYS(fb);
    ULONG a1 = IDWriteFontFallback_AddRef(fb), a2 = IDWriteFontFallback_AddRef(fb), r1 = IDWriteFontFallback_Release(fb), r2 = IDWriteFontFallback_Release(fb), r3 = IDWriteFontFallback_Release(fb);
    t_note("AddRef %lu %lu, Release %lu %lu %lu", a1, a2, r1, r2, r3);   /* the system object's counts are shared state: only the direction is contractual */
    CHECK(a1 >= 2 && a2 > a1 && r1 < a2 && r2 < r1 + 1 && r3 <= r2);
}

RT(sys_object_independent_of_other_system_fallbacks)
{
    IDWriteFontFallback *a, *b; r2_args ar; r2_res r; r2_fx X;
    memset(&X, 0, sizeof X);
    a = sysfb(); b = sysfb();
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); t_skip("unavailable"); return; }
    r2_defaults(&ar, NULL); ar.family = NULL;
    DW_RELEASE(a);                                            /* the second keeps working after the first is gone */
    CHECK_HR_OK(r2_map(b, L"\x4e00", 1, &ar, &r, NULL));
    CHECK(r.font != NULL); DW_RELEASE(r.font);
    DW_RELEASE(b);
}

/* ---- scripts every Windows 10 installation covers ---------------------------------------------------------------------------------------- */
typedef struct { UINT32 cp; const char *what; } scr;

static void cp_to_text(UINT32 cp, WCHAR *t)
{
    if (cp > 0xffff) { UINT32 v = cp - 0x10000; t[0] = (WCHAR)(0xd800 + (v >> 10)); t[1] = (WCHAR)(0xdc00 + (v & 0x3ff)); t[2] = 0; } else { t[0] = (WCHAR)cp; t[1] = 0; }
}

static void script_table(IDWriteFontFallback *fb, IDWriteFontCollection *basecoll, const WCHAR *basefam, const scr *rows, int n, int line)
{
    int i;
    for (i = 0; i < n; i++) {
        WCHAR t[3]; r2_args a; r2_res r; UINT32 tl;
        cp_to_text(rows[i].cp, t); tl = (UINT32)wcslen(t);
        memset(&a, 0, sizeof a); a.len = (UINT32)-1; a.coll = basecoll; a.family = basefam; a.w = DWRITE_FONT_WEIGHT_NORMAL; a.st = DWRITE_FONT_STYLE_NORMAL; a.sx = DWRITE_FONT_STRETCH_NORMAL;
        t_check(r2_map(fb, t, tl, &a, &r, NULL) == S_OK, __FILE__, line, "MapCharacters", "U+%04X", rows[i].cp);
        t_check(r.font != NULL, __FILE__, line, rows[i].what, "U+%04X (%s): no font", rows[i].cp, rows[i].what);
        if (r.font) {
            t_check(r.ml == tl, __FILE__, line, rows[i].what, "U+%04X: mappedLength %u want %u", rows[i].cp, r.ml, tl);
            t_check(r2_font_has(r.font, rows[i].cp), __FILE__, line, rows[i].what, "U+%04X: \"%s\" lacks the character", rows[i].cp, r.fi.fam);
            t_check(r.scale > 0.0f && r.scale <= 4.0f, __FILE__, line, rows[i].what, "U+%04X: scale %g", rows[i].cp, r.scale);
            t_note("U+%04X (%s) -> \"%s\" scale %g", rows[i].cp, rows[i].what, r.fi.fam, r.scale);
        }
        DW_RELEASE(r.font);
    }
}

RT(sys_common_scripts_have_a_font)
{
    NEED_SYS(fb);
    static const scr rows[] = {
        { 0x0041, "Latin" }, { 0x00e9, "Latin-1" }, { 0x0141, "Latin Extended-A" }, { 0x0391, "Greek" }, { 0x0416, "Cyrillic" }, { 0x05d0, "Hebrew" }, { 0x0627, "Arabic" },
        { 0x0905, "Devanagari" }, { 0x0e01, "Thai" }, { 0x3042, "Hiragana" }, { 0x30a2, "Katakana" }, { 0x4e00, "CJK ideograph" }, { 0xac00, "Hangul" },
        { 0x2190, "Arrows" }, { 0x2200, "Mathematical operators" }, { 0x2603, "Snowman" }, { 0x2605, "Black star" }, { 0x20ac, "Euro" },
    };
    script_table(fb, NULL, NULL, rows, (int)(sizeof rows / sizeof rows[0]), __LINE__);
    DW_RELEASE(fb);
}

RT(sys_supplementary_plane_characters_have_a_font)
{
    NEED_SYS(fb);
    static const scr rows[] = { { 0x1f600, "grinning face emoji" }, { 0x1f468, "man emoji" }, { 0x1f4a9, "pile of poo" } };
    script_table(fb, NULL, NULL, rows, 3, __LINE__);
    DW_RELEASE(fb);
}

RT(sys_less_common_scripts_have_a_font_note)
{
    NEED_SYS(fb); int i; r2_args a; r2_res r;
    static const scr rows[] = {
        { 0x0985, "Bengali" }, { 0x0b85, "Tamil" }, { 0x0c05, "Telugu" }, { 0x0d05, "Malayalam" }, { 0x0a05, "Gurmukhi" }, { 0x0a85, "Gujarati" }, { 0x0c85, "Kannada" },
        { 0x10a0, "Georgian" }, { 0x0531, "Armenian" }, { 0x1200, "Ethiopic" }, { 0x1000, "Myanmar" }, { 0x1780, "Khmer" }, { 0x0e81, "Lao" }, { 0x0f40, "Tibetan" },
        { 0x1800, "Mongolian" }, { 0xa000, "Yi" }, { 0x13a0, "Cherokee" }, { 0x0710, "Syriac" }, { 0x0780, "Thaana" }, { 0x0d85, "Sinhala" }, { 0x1401, "Canadian Syllabics" },
        { 0x20000, "CJK Extension B" }, { 0x1d11e, "musical symbols" }, { 0x1f0a1, "playing cards" }, { 0x1d7d8, "mathematical double-struck digit" },
    };
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        WCHAR t[3]; UINT32 tl; cp_to_text(rows[i].cp, t); tl = (UINT32)wcslen(t);
        r2_defaults(&a, NULL); a.family = NULL;
        CHECK_HR_OK(r2_map(fb, t, tl, &a, &r, NULL));
        CHECK(r.ml == tl);
        if (r.font) { CHECKF(r2_font_has(r.font, rows[i].cp), "U+%04X: \"%s\" lacks the character", rows[i].cp, r.fi.fam); t_note("%s U+%04X -> \"%s\"", rows[i].what, rows[i].cp, r.fi.fam); }
        else t_note("%s U+%04X -> no font", rows[i].what, rows[i].cp);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb);
}

RT(sys_nothing_maps_noncharacters_note)
{
    NEED_SYS(fb); int i; r2_args a; r2_res r;
    static const UINT32 cps[] = { 0xfdd0, 0xffff, 0xe000, 0xf8ff, 0x378, 0xd7ff };
    for (i = 0; i < (int)(sizeof cps / sizeof cps[0]); i++) {
        WCHAR t[3]; cp_to_text(cps[i], t);
        r2_defaults(&a, NULL); a.family = NULL;
        CHECK_HR_OK(r2_map(fb, t, 1, &a, &r, NULL));
        CHECKF(r.ml == 1, "U+%04X: mappedLength %u", cps[i], r.ml);
        t_note("U+%04X -> %s", cps[i], r.font ? r.fi.fam : "no font");
        if (r.font) CHECK(r2_font_has(r.font, cps[i]));
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb);
}

/* ---- base family from the system collection ----------------------------------------------------------------------------------------------- */
RT(sys_base_family_covers_latin_text_in_full)
{
    NEED_SYS(fb);
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *sc = syscoll(f);
    static const WCHAR *fam[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Verdana", L"Calibri", L"Consolas", L"Georgia", L"Trebuchet MS" };
    unsigned i; int tested = 0;
    if (!sc) { DW_RELEASE(fb); t_skip("system collection unavailable"); return; }
    for (i = 0; i < sizeof fam / sizeof fam[0]; i++) {
        r2_args a; r2_res r; char want[64];
        if (!dw_have_font(f, fam[i])) { t_note("%ls not installed", fam[i]); continue; }
        r2_defaults(&a, NULL); a.coll = sc; a.family = fam[i];
        CHECK_HR_OK(r2_map(fb, L"Hello World 123", 15, &a, &r, NULL));
        dw_narrow(fam[i], want, sizeof want);
        CHECKF(r.ml == 15 && r.font != NULL && !strcmp(r.fi.fam, want), "%ls: mappedLength %u family \"%s\"", fam[i], r.ml, r.fi.fam);
        CHECK_FLOAT_EQ(r.scale, 1.0f);
        DW_RELEASE(r.font); tested++;
    }
    if (!tested) t_skip("none of the probe fonts installed");
    DW_RELEASE(sc); DW_RELEASE(fb);
}

RT(sys_base_family_weight_style_propagate)
{
    NEED_SYS(fb);
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *sc = syscoll(f);
    static const struct { DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; } rows[] = {
        { 400, DWRITE_FONT_STYLE_NORMAL }, { 700, DWRITE_FONT_STYLE_NORMAL }, { 400, DWRITE_FONT_STYLE_ITALIC }, { 700, DWRITE_FONT_STYLE_ITALIC },
    };
    unsigned i;
    if (!sc || !dw_have_font(f, L"Arial")) { DW_RELEASE(sc); DW_RELEASE(fb); t_skip("Arial not installed"); return; }
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        r2_args a; r2_res r;
        r2_defaults(&a, NULL); a.coll = sc; a.family = L"Arial"; a.w = rows[i].w; a.st = rows[i].st;
        CHECK_HR_OK(r2_map(fb, L"Abc", 3, &a, &r, NULL));
        CHECK(r.font != NULL); EXPECT_EQ(r.ml, 3);
        CHECKF(r.fi.w == rows[i].w && r.fi.st == rows[i].st, "Arial %d/%d: got %d/%d", rows[i].w, rows[i].st, r.fi.w, r.fi.st);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(sc); DW_RELEASE(fb);
}

RT(sys_base_family_other_weights_note)
{
    NEED_SYS(fb);
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *sc = syscoll(f);
    static const DWRITE_FONT_WEIGHT w[] = { 100, 300, 350, 500, 600, 800, 900 };
    unsigned i;
    if (!sc || !dw_have_font(f, L"Segoe UI")) { DW_RELEASE(sc); DW_RELEASE(fb); t_skip("Segoe UI not installed"); return; }
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        r2_args a; r2_res r;
        r2_defaults(&a, NULL); a.coll = sc; a.family = L"Segoe UI"; a.w = w[i];
        CHECK_HR_OK(r2_map(fb, L"Abc", 3, &a, &r, NULL));
        CHECK(r.font != NULL); t_note("Segoe UI weight %d -> %d", w[i], r.fi.w);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(sc); DW_RELEASE(fb);
}

RT(sys_base_family_missing_still_maps_latin)
{
    NEED_SYS(fb);
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *sc = syscoll(f); r2_args a; r2_res r;
    if (!sc) { DW_RELEASE(fb); t_skip("system collection unavailable"); return; }
    r2_defaults(&a, NULL); a.coll = sc; a.family = L"NoSuchFamily-r2-zz";
    CHECK_HR_OK(r2_map(fb, L"Hello", 5, &a, &r, NULL));
    CHECK(r.font != NULL);
    if (r.font) { CHECK(r.ml >= 1 && r.ml <= 5); CHECK(r2_font_has(r.font, 'H')); t_note("missing base family, Latin text -> \"%s\"", r.fi.fam); }
    DW_RELEASE(r.font); DW_RELEASE(sc); DW_RELEASE(fb);
}

RT(sys_private_base_collection_latin_stays_private)
{
    NEED_SYS(fb); R2_BEGIN(); r2_args a; r2_res r;
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_map(fb, L"abc\x4e00", 4, &a, &r, NULL));
    EXPECT_EQ(r.ml, 3); EXPECT_STR(r.fi.fam, "R2Main");       /* the base family is looked up in the collection that was passed */
    DW_RELEASE(r.font);
    a.pos = 3; a.len = 1;
    CHECK_HR_OK(r2_map(fb, L"abc\x4e00", 4, &a, &r, NULL));
    CHECK(r.font != NULL); CHECK(r.font == NULL || r2_font_has(r.font, 0x4e00)); EXPECT_EQ(r.ml, 1);
    if (r.font) t_note("CJK ideograph with a private base collection -> \"%s\"", r.fi.fam);
    DW_RELEASE(r.font); DW_RELEASE(fb); R2_END();
}

RT(sys_null_collection_and_family_are_accepted)
{
    NEED_SYS(fb); r2_args a; r2_res r;
    r2_defaults(&a, NULL); a.coll = NULL; a.family = NULL;
    CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, NULL));
    CHECK(r.font != NULL && r.ml >= 1 && r.ml <= 3); DW_RELEASE(r.font);
    a.family = L"Arial";
    CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, NULL));          /* family without a collection: the family is ignored */
    CHECK(r.font != NULL && r.ml >= 1 && r.ml <= 3); DW_RELEASE(r.font);
    DW_RELEASE(fb);
}

/* ---- locale dependent choice for ideographs ------------------------------------------------------------------------------------------------- */
RT(sys_cjk_ideograph_per_locale_note)
{
    NEED_SYS(fb);
    static const WCHAR *loc[] = { L"en-us", L"ja-JP", L"zh-CN", L"zh-TW", L"zh-HK", L"ko-KR", L"zh-Hans", L"zh-Hant", L"ja", L"ko", L"" };
    unsigned i;
    for (i = 0; i < sizeof loc / sizeof loc[0]; i++) {
        r2_args a; r2_res r;
        r2_defaults(&a, NULL); a.family = NULL; a.locale = loc[i];
        CHECK_HR_OK(r2_map(fb, L"\x4e00", 1, &a, &r, NULL));
        CHECKF(r.font != NULL, "no font for U+4E00 with source locale \"%ls\"", loc[i]);
        if (r.font) { CHECK(r2_font_has(r.font, 0x4e00)); t_note("U+4E00 with locale \"%ls\" -> \"%s\" (scale %g)", loc[i], r.fi.fam, r.scale); }
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb);
}

RT(sys_han_unification_candidates_per_locale_note)
{
    NEED_SYS(fb);
    static const WCHAR *loc[] = { L"ja-JP", L"zh-CN", L"zh-TW", L"ko-KR" };
    static const UINT32 cps[] = { 0x76f4, 0x9aa8, 0x6d77, 0x9f8d };
    unsigned i, k;
    for (i = 0; i < 4; i++) for (k = 0; k < 4; k++) {
        r2_args a; r2_res r; WCHAR t[2];
        t[0] = (WCHAR)cps[k]; t[1] = 0;
        r2_defaults(&a, NULL); a.family = NULL; a.locale = loc[i];
        CHECK_HR_OK(r2_map(fb, t, 1, &a, &r, NULL));
        CHECKF(r.font != NULL, "U+%04X (%ls)", cps[k], loc[i]);
        if (r.font) t_note("U+%04X with locale %ls -> \"%s\"", cps[k], loc[i], r.fi.fam);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb);
}

RT(sys_locale_run_clamps_mapped_length)
{
    NEED_SYS(fb); r2_args a; r2_res r; dwx_ana_src *s;
    r2_defaults(&a, NULL); a.family = NULL; a.locale_run = 2;
    CHECK_HR_OK(r2_map(fb, L"\x4e00\x4e8c\x4e09\x56db", 4, &a, &r, &s));
    CHECKF(r.ml >= 1 && r.ml <= 2, "mappedLength %u exceeds the locale run (2)", r.ml);
    CHECK(s->n_locale >= 1);
    dwx_release(s); DW_RELEASE(r.font); DW_RELEASE(fb);
}

/* ---- walks ---------------------------------------------------------------------------------------------------------------------------------- */
RT(sys_mixed_text_walk_tiles_and_fonts_cover)
{
    NEED_SYS(fb); r2_args a; char buf[1024]; int cov = 1, n;
    const WCHAR *text = L"Hello \x4e16\x754c \x3053\x3093\x306b\x3061\x306f \xD55C\xAE00 \x0645\x0631\x062d\x0628\x0627 \x05e9\x05dc\x05d5\x05dd \xD83D\xDE00 \x0391\x03b2 \x0416 \x2603";
    r2_defaults(&a, NULL); a.family = NULL;
    n = r2_walk(fb, text, &a, buf, sizeof buf, &cov);
    CHECK(n >= 1); CHECK(cov);
    t_note("mixed walk: %s", buf);
    r2_walk_invariants(fb, text, &a, __FILE__, __LINE__);
    DW_RELEASE(fb);
}

RT(sys_walk_is_deterministic)
{
    NEED_SYS(fb); IDWriteFontFallback *fb2 = sysfb(); r2_args a; char b1[1024], b2[1024];
    const WCHAR *text = L"abc \x4e00\x4e8c \x0627\x0628 \xD83D\xDE00 \x2603 xyz";
    r2_defaults(&a, NULL); a.family = NULL;
    r2_walk(fb, text, &a, b1, sizeof b1, NULL); r2_walk(fb2, text, &a, b2, sizeof b2, NULL);
    EXPECT_STR(b1, b2);
    r2_walk(fb, text, &a, b2, sizeof b2, NULL);
    EXPECT_STR(b1, b2);
    DW_RELEASE(fb2); DW_RELEASE(fb);
}

RT(sys_emoji_sequences_invariants)
{
    NEED_SYS(fb); r2_args a; unsigned i;
    static const WCHAR *seq[] = {
        L"\xD83D\xDE00", L"\xD83D\xDE00\xfe0f", L"\x2764\xfe0f", L"\x2764", L"\xD83D\xDC68\x200d\xD83D\xDC69\x200d\xD83D\xDC67", L"\xD83D\xDC68\xD83C\xDFFB",
        L"\xD83C\xDDEF\xD83C\xDDF5", L"1\xfe0f\x20e3", L"\x2603\xfe0e", L"\xD83D\xDC4D\xD83C\xDFFD", L"a\xD83D\xDE00" L"b", L"\x263a\xfe0f",
    };
    for (i = 0; i < sizeof seq / sizeof seq[0]; i++) {
        r2_defaults(&a, NULL); a.family = NULL;
        r2_walk_invariants(fb, seq[i], &a, __FILE__, __LINE__);
    }
    DW_RELEASE(fb);
}

RT(sys_combining_sequences_invariants)
{
    NEED_SYS(fb); r2_args a; unsigned i;
    static const WCHAR *seq[] = {
        L"e\x0301", L"a\x0308\x0323", L"\x3042\x3099", L"\x0915\x094d\x0937", L"\x0627\x064e\x0651", L"\x05d0\x05b0", L"\x0e01\x0e48", L"\xac00\x11a8", L"\x4e00\xfe00",
        L"\x0301", L"\x0345", L"\x20d0",
    };
    for (i = 0; i < sizeof seq / sizeof seq[0]; i++) {
        r2_defaults(&a, NULL); a.family = NULL;
        r2_walk_invariants(fb, seq[i], &a, __FILE__, __LINE__);
    }
    DW_RELEASE(fb);
}

RT(sys_controls_and_spaces_invariants)
{
    NEED_SYS(fb); r2_args a; unsigned i;
    static const WCHAR *seq[] = {
        L" ", L"   ", L"\t", L"\r\n", L"a b", L"\x3042 \x3044", L"\xa0", L"\x200b", L"\x200d", L"\xfeff", L"\x2028", L"\x2029", L"\xad", L"\x3000", L"\xfffc", L"\xfffd",
    };
    for (i = 0; i < sizeof seq / sizeof seq[0]; i++) {
        r2_defaults(&a, NULL); a.family = NULL;
        r2_walk_invariants(fb, seq[i], &a, __FILE__, __LINE__);
    }
    DW_RELEASE(fb);
}

RT(sys_start_offsets_tile_the_rest)
{
    NEED_SYS(fb); r2_args a; const WCHAR *text = L"ab\x4e00\xD83D\xDE00\x2603\x0301" L"c\x0627" L"d"; UINT32 n = (UINT32)wcslen(text), p;
    for (p = 0; p < n; p++) {
        UINT32 pos = p, total = 0; int guard = 0;
        r2_defaults(&a, NULL); a.family = NULL;
        while (pos < n && guard++ < 64) {
            r2_res r;
            a.pos = pos; a.len = n - pos;
            if (FAILED(r2_map(fb, text, n, &a, &r, NULL))) break;
            if (r.ml == 0 || r.ml > n - pos) { DW_RELEASE(r.font); total = 0xffff; break; }
            pos += r.ml; total += r.ml; DW_RELEASE(r.font);
        }
        CHECKF(total == n - p, "start offset %u: covered %u of %u", p, total, n - p);
    }
    DW_RELEASE(fb);
}

RT(sys_mapped_font_refcount_and_lifetime)
{
    NEED_SYS(fb); r2_args a; r2_res r; IDWriteFontFace *face = NULL;
    r2_defaults(&a, NULL); a.family = NULL;
    CHECK_HR_OK(r2_map(fb, L"\x4e00", 1, &a, &r, NULL));
    DW_RELEASE(fb);                                            /* the font stays valid after the fallback is gone */
    if (r.font) {
        CHECK_HR_OK(IDWriteFont_CreateFontFace(r.font, &face));
        CHECK(face != NULL);
        DW_RELEASE(face);
    } else CHECK(r.font != NULL);
    DW_RELEASE(r.font);
}

RT(sys_fallback1_matches_fallback0_for_default_properties)
{
    NEED_SYS(fb); IDWriteFontFallback1 *fb1 = NULL; r2_args a; r2_res r; unsigned i;
    static const WCHAR *texts[] = { L"abc", L"\x4e00", L"\x0627", L"\x2603", L"\xD83D\xDE00" };
    CHECK_HR_OK(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&fb1));
    for (i = 0; fb1 && i < sizeof texts / sizeof texts[0]; i++) {
        dwx_ana_src *s = dwx_ana_src_new(texts[i], (UINT32)wcslen(texts[i])); UINT32 ml1 = 0; float sc1 = 0; IDWriteFontFace5 *face = NULL; r2_finfo fi1;
        r2_defaults(&a, NULL); a.family = NULL;
        CHECK_HR_OK(r2_map(fb, texts[i], (UINT32)wcslen(texts[i]), &a, &r, NULL));
        CHECK_HR_OK(IDWriteFontFallback1_MapCharacters(fb1, (IDWriteTextAnalysisSource *)s, 0, (UINT32)wcslen(texts[i]), NULL, NULL, NULL, 0, &ml1, &sc1, &face));
        r2_face_info(face, &fi1);
        CHECKF(ml1 == r.ml, "text %u: v1 mappedLength %u v0 %u", i, ml1, r.ml);
        CHECKF(sc1 == r.scale, "text %u: v1 scale %g v0 %g", i, sc1, r.scale);
        CHECKF((face != NULL) == (r.font != NULL), "text %u: v1 face %p v0 font %p", i, (void *)face, (void *)r.font);
        if (face && r.font) CHECKF(!strcmp(fi1.fam, r.fi.fam), "text %u: v1 family \"%s\" v0 \"%s\"", i, fi1.fam, r.fi.fam);
        DW_RELEASE(face); DW_RELEASE(r.font); dwx_release(s);
    }
    DW_RELEASE(fb1); DW_RELEASE(fb);
}

/* ---- AddMappings(system) ---------------------------------------------------------------------------------------------------------------------- */
RT(sys_add_mappings_from_system_fallback_maps_cjk)
{
    if (t_is_wine()) { t_skip("Wine does not implement IDWriteFontFallbackBuilder::AddMappings"); return; }
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *sys = sysfb(), *fb; r2_args a; r2_res r;
    if (!b || !sys) { DW_RELEASE(b); DW_RELEASE(sys); R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMappings(b, sys));
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x4e00", 1, &a, &r, NULL));
        CHECK(r.font != NULL); if (r.font) { CHECK(r2_font_has(r.font, 0x4e00)); t_note("builder with system mappings: U+4E00 -> \"%s\"", r.fi.fam); }
        DW_RELEASE(r.font);
        CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, NULL));       /* the private base family still comes first */
        EXPECT_EQ(r.ml, 3); EXPECT_STR(r.fi.fam, "R2Main"); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(sys); DW_RELEASE(b); R2_END();
}

RT(sys_add_mappings_custom_mapping_added_before_system_wins_for_its_range)
{
    if (t_is_wine()) { t_skip("Wine does not implement IDWriteFontFallbackBuilder::AddMappings"); return; }
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *sys = sysfb(), *fb; r2_args a; r2_res r;
    if (!b || !sys) { DW_RELEASE(b); DW_RELEASE(sys); R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMappings(b, sys));
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        CHECK(r.font != NULL);
        t_note("custom mapping first, then the system mappings: U+3042 -> \"%s\"", r.fi.fam);
        DW_RELEASE(r.font);
        CHECK_HR_OK(r2_map(fb, L"\x0627", 1, &a, &r, NULL));
        CHECK(r.font != NULL); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(sys); DW_RELEASE(b); R2_END();
}

RT(sys_add_mappings_is_additive_and_repeatable)
{
    if (t_is_wine()) { t_skip("Wine does not implement IDWriteFontFallbackBuilder::AddMappings"); return; }
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *sys = sysfb(), *fb; r2_args a; r2_res r;
    if (!b || !sys) { DW_RELEASE(b); DW_RELEASE(sys); R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMappings(b, sys));
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMappings(b, sys));
    EXPECT_REFS(sys, 1);                                        /* the builder copies the mappings, it does not keep the source */
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) { CHECK_HR_OK(r2_map(fb, L"\x4e00", 1, &a, &r, NULL)); CHECK(r.font != NULL); DW_RELEASE(r.font); }
    DW_RELEASE(fb); DW_RELEASE(sys); DW_RELEASE(b); R2_END();
}

RT(sys_add_mappings_from_a_builder_made_fallback)
{
    if (t_is_wine()) { t_skip("Wine does not implement IDWriteFontFallbackBuilder::AddMappings"); return; }
    R2_BEGIN(); IDWriteFontFallbackBuilder *b1 = r2_builder(f), *b2 = r2_builder(f); IDWriteFontFallback *fa, *fb; r2_args a;
    if (!b1 || !b2) { DW_RELEASE(b1); DW_RELEASE(b2); R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b1, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 0.5f));
    CHECK_HR_OK(r2_add(b1, 0x2600, 0x26ff, R2_SYM, X.coll, NULL, NULL, 1.0f));
    fa = r2_fb(b1);
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMappings(b2, fa));
    fb = r2_fb(b2); r2_defaults(&a, &X);
    if (fb) {
        r2_res r;
        R2_WALK_EQ(fb, L"a\x3042\x2603", &a, "1:R2Main|1:R2Cjk|1:R2Sym");
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); CHECK_FLOAT_EQ(r.scale, 0.5f); DW_RELEASE(r.font);       /* scale is copied with the mapping */
    }
    DW_RELEASE(fa); DW_RELEASE(fb); DW_RELEASE(b1); DW_RELEASE(b2); R2_END();
}

RT(sys_add_mappings_keeps_locale_and_base_filters)
{
    if (t_is_wine()) { t_skip("Wine does not implement IDWriteFontFallbackBuilder::AddMappings"); return; }
    R2_BEGIN(); IDWriteFontFallbackBuilder *b1 = r2_builder(f), *b2 = r2_builder(f); IDWriteFontFallback *fa, *fb; r2_args a; r2_res r;
    if (!b1 || !b2) { DW_RELEASE(b1); DW_RELEASE(b2); R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b1, 0x3040, 0x30ff, R2_CJK, X.coll, L"ja-JP", R2_ALT, 1.0f));
    fa = r2_fb(b1);
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMappings(b2, fa));
    fb = r2_fb(b2);
    if (fb) {
        r2_defaults(&a, &X); a.family = R2_ALT; a.locale = L"ja-JP";
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); CHECK(r.font != NULL); DW_RELEASE(r.font);
        if (!t_is_wine()) {
            a.family = R2_MAIN;
            CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); CHECK(r.font == NULL); DW_RELEASE(r.font);      /* the base family filter was copied */
        }
    }
    DW_RELEASE(fa); DW_RELEASE(fb); DW_RELEASE(b1); DW_RELEASE(b2); R2_END();
}
