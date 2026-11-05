#include "ntf.h"

/* IDWriteFontFallback (system object from IDWriteFactory2::GetSystemFontFallback) on Windows 10:
 *   - GetSystemFontFallback zeroes the out pointer and builds a NEW IDWriteFontFallback object per call (refcount 1, never cached/shared);
 *   - the object answers IDWriteFontFallback and IDWriteFontFallback1 (and a private interface), nothing else public;
 *   - MapCharacters (v0) first stores mappedLength = 0, *mappedFont = NULL, *scale = 1.0 and only then validates:
 *       source == NULL, textPosition + textLength overflowing 32 bits, weight outside 1..999, style > 2, stretch outside 1..9  =>  E_INVALIDARG
 *     the outputs stay zeroed in that case; every other call returns S_OK;
 *   - IDWriteFontFallback1::MapCharacters additionally rejects fontAxisValueCount > 0xFFFF and a NULL axis array with a non-zero count,
 *     and hands back an IDWriteFontFace5 (QueryInterface on the mapped face) instead of an IDWriteFont;
 *   - the base family is tried first (when baseFontCollection is set and the name is non-empty), then custom/system mappings; a NULL name counts as "". */

static IDWriteFontFallback *sysfb(IDWriteFactory *f)
{
    IDWriteFactory2 *f2 = ntf_f2(f); IDWriteFontFallback *fb = NULL;
    if (!f2) return NULL;
    if (FAILED(IDWriteFactory2_GetSystemFontFallback(f2, &fb))) fb = NULL;
    IDWriteFactory2_Release(f2);
    return fb;
}
#define NEED_FB(fb, f) IDWriteFontFallback *fb = sysfb(f); if (!fb) { t_skip("IDWriteFactory2::GetSystemFontFallback unavailable"); return; }

TEST(fb_system_create_basic)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f);
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb);
}

TEST(fb_system_create_outparam_and_distinct)
{
    IDWriteFactory *f = dw_shared(); IDWriteFactory2 *f2 = ntf_f2(f); IDWriteFontFallback *a = NULL, *b = NULL;
    if (!f2) { t_skip("no IDWriteFactory2"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &a));
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &b));
    CHECK(a && b && a != b);                 /* a new object per call */
    DW_RELEASE(a); DW_RELEASE(b); IDWriteFactory2_Release(f2);
}

TEST(fb_system_qi_matrix)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f);
    NTF_QI_ONLY(fb, &IID_IDWriteFontFallback, &IID_IDWriteFontFallback1);
    DW_RELEASE(fb);
}

TEST(fb_system_qi_identity)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); IUnknown *u = NULL; IDWriteFontFallback1 *f1 = NULL; IUnknown *u1 = NULL;
    CHECK_HR_OK(IDWriteFontFallback_QueryInterface(fb, &IID_IUnknown, (void **)&u));
    CHECK_HR_OK(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&f1));
    if (f1) { CHECK_HR_OK(IDWriteFontFallback1_QueryInterface(f1, &IID_IUnknown, (void **)&u1)); CHECK(u1 == u); }
    EXPECT_REFS(fb, 4);
    DW_RELEASE(u); DW_RELEASE(f1); DW_RELEASE(u1);
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb);
}

TEST(fb_system_refcount_protocol)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); ULONG r;
    r = IDWriteFontFallback_AddRef(fb); EXPECT_EQ(r, 2);
    r = IDWriteFontFallback_AddRef(fb); EXPECT_EQ(r, 3);
    r = IDWriteFontFallback_Release(fb); EXPECT_EQ(r, 2);
    r = IDWriteFontFallback_Release(fb); EXPECT_EQ(r, 1);
    r = IDWriteFontFallback_Release(fb); EXPECT_EQ(r, 0);
}

TEST(fb_system_outlives_factory)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontFallback *fb = NULL; IDWriteFactory2 *f2; UINT32 ml; IDWriteFont *font; float sc;
    if (!f) { t_skip("no isolated factory"); return; }
    f2 = ntf_f2(f);
    if (!f2) { DW_RELEASE(f); t_skip("no IDWriteFactory2"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &fb));
    IDWriteFactory2_Release(f2); DW_RELEASE(f);
    if (fb) {
        CHECK_HR(ntf_map(fb, L"abc", 3, 0, 3, NULL, L"Arial", &ml, &font, &sc, NULL), S_OK);      /* the fallback keeps its factory alive */
        DW_RELEASE(font);
        DW_RELEASE(fb);
    }
}

/* ---- argument validation ---------------------------------------------------------------------------------------------------- */

static HRESULT map_raw(IDWriteFontFallback *fb, IDWriteTextAnalysisSource *src, UINT32 pos, UINT32 len, int w, int st, int sr, UINT32 *ml, IDWriteFont **font, float *sc)
{
    *ml = 0x1234; *font = NULL; *sc = 7.0f;
    return IDWriteFontFallback_MapCharacters(fb, src, pos, len, NULL, L"Arial", (DWRITE_FONT_WEIGHT)w, (DWRITE_FONT_STYLE)st, (DWRITE_FONT_STRETCH)sr, ml, font, sc);
}

TEST(fb_map_null_source)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font; float sc;
    CHECK_HR(map_raw(fb, NULL, 0, 1, 400, 0, 5, &ml, &font, &sc), E_INVALIDARG);
    EXPECT_EQ(ml, 0); CHECK_OUT_NULL(font); CHECK_FLOAT_EQ(sc, 1.0f);      /* outputs are initialised before validation */
    DW_RELEASE(fb);
}

TEST(fb_map_position_overflow)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font; float sc; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3);
    static const UINT32 pl[][2] = { { 0xffffffffu, 1 }, { 1, 0xffffffffu }, { 0xfffffffeu, 2 }, { 0x80000000u, 0x80000000u }, { 3, 0xfffffffeu } };
    unsigned i;
    for (i = 0; i < sizeof pl / sizeof pl[0]; i++) {
        CHECKF(map_raw(fb, (IDWriteTextAnalysisSource *)s, pl[i][0], pl[i][1], 400, 0, 5, &ml, &font, &sc) == E_INVALIDARG, "pos %u len %u", pl[i][0], pl[i][1]);
        EXPECT_EQ(ml, 0); CHECK_OUT_NULL(font); CHECK_FLOAT_EQ(sc, 1.0f);
    }
    EXPECT_EQ(s->n_at, 0);                    /* rejected before the source is touched */
    dwx_release(s); DW_RELEASE(fb);
}

TEST(fb_map_invalid_weight)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font; float sc; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3);
    static const int bad[] = { 0, 1000, 1001, 2000, 0x7fffffff, -1, (int)0x80000000 };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECKF(map_raw(fb, (IDWriteTextAnalysisSource *)s, 0, 3, bad[i], 0, 5, &ml, &font, &sc) == E_INVALIDARG, "weight %d", bad[i]);
        EXPECT_EQ(ml, 0); CHECK_OUT_NULL(font); CHECK_FLOAT_EQ(sc, 1.0f);
    }
    dwx_release(s); DW_RELEASE(fb);
}

TEST(fb_map_invalid_style)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font; float sc; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3);
    static const int bad[] = { 3, 4, 100, 0xffff, -1 };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECKF(map_raw(fb, (IDWriteTextAnalysisSource *)s, 0, 3, 400, bad[i], 5, &ml, &font, &sc) == E_INVALIDARG, "style %d", bad[i]);
        EXPECT_EQ(ml, 0); CHECK_OUT_NULL(font); CHECK_FLOAT_EQ(sc, 1.0f);
    }
    dwx_release(s); DW_RELEASE(fb);
}

TEST(fb_map_invalid_stretch)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font; float sc; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3);
    static const int bad[] = { 0, 10, 11, 100, -1 };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECKF(map_raw(fb, (IDWriteTextAnalysisSource *)s, 0, 3, 400, 0, bad[i], &ml, &font, &sc) == E_INVALIDARG, "stretch %d", bad[i]);
        EXPECT_EQ(ml, 0); CHECK_OUT_NULL(font); CHECK_FLOAT_EQ(sc, 1.0f);
    }
    dwx_release(s); DW_RELEASE(fb);
}

TEST(fb_map_valid_enum_boundaries)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font; float sc; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3);
    static const int w[] = { 1, 100, 400, 700, 900, 950, 999 }; unsigned i; int st, sr;
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        CHECKF(map_raw(fb, (IDWriteTextAnalysisSource *)s, 0, 3, w[i], 0, 5, &ml, &font, &sc) == S_OK, "weight %d", w[i]);
        DW_RELEASE(font);
    }
    for (st = 0; st <= 2; st++) for (sr = 1; sr <= 9; sr++) {
        CHECKF(map_raw(fb, (IDWriteTextAnalysisSource *)s, 0, 3, 400, st, sr, &ml, &font, &sc) == S_OK, "style %d stretch %d", st, sr);
        DW_RELEASE(font);
    }
    dwx_release(s); DW_RELEASE(fb);
}

TEST(fb_map_null_family_and_collection_ok)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc;
    CHECK_HR(ntf_map(fb, L"abc", 3, 0, 3, NULL, NULL, &ml, &font, &sc, NULL), S_OK);      /* NULL collection and NULL family are legal */
    CHECK(ml >= 1 && ml <= 3); DW_RELEASE(font);
    CHECK_HR(ntf_map(fb, L"abc", 3, 0, 3, NULL, L"", &ml, &font, &sc, NULL), S_OK);
    CHECK(ml >= 1 && ml <= 3); DW_RELEASE(font);
    DW_RELEASE(fb);
}

TEST(fb_map_unknown_base_family_ok)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc;
    IDWriteFontCollection *sys = NULL;
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE));
    CHECK_HR(ntf_map(fb, L"abc", 3, 0, 3, sys, L"No Such Family Qq", &ml, &font, &sc, NULL), S_OK);
    CHECK(ml >= 1 && ml <= 3); CHECK(sc > 0.0f);
    DW_RELEASE(font); DW_RELEASE(sys); DW_RELEASE(fb);
}

TEST(fb_map_source_is_queried)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc; dwx_ana_src *s = NULL; int i, bad = 0;
    CHECK_HR(ntf_map(fb, L"abcdef", 6, 2, 3, NULL, L"Arial", &ml, &font, &sc, &s), S_OK);
    CHECK(s->n_at >= 1);                                    /* the text is fetched with GetTextAtPosition */
    for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 1 && s->log[i].pos < 2) bad++;
    t_note("source calls: at=%ld before=%ld locale=%ld dir=%ld, GetTextAtPosition below textPosition: %d", s->n_at, s->n_before, s->n_locale, s->n_dir, bad);
    CHECK(ml <= 3);                                         /* never maps beyond the requested range */
    DW_RELEASE(font); dwx_release(s); DW_RELEASE(fb);
}

TEST(fb_map_result_never_exceeds_range)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc; unsigned p, l;
    static const WCHAR t[] = L"abc\x3042\x3044xyz";
    for (p = 0; p < 8; p++) for (l = 1; p + l <= 8; l++) {
        CHECK_HR(ntf_map(fb, t, 8, p, l, NULL, L"Arial", &ml, &font, &sc, NULL), S_OK);
        CHECKF(ml >= 1 && ml <= l, "range %u+%u -> mappedLength %u", p, l, ml);
        CHECK(sc > 0.0f);
        DW_RELEASE(font);
    }
    DW_RELEASE(fb);
}

TEST(fb_map_zero_length_note)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc; HRESULT hr;
    hr = ntf_map(fb, L"abc", 3, 1, 0, NULL, L"Arial", &ml, &font, &sc, NULL);
    t_note("textLength 0: %s mappedLength=%u font=%p scale=%g", dw_hr_name(hr), ml, (void *)font, sc);
    CHECK(hr == S_OK);
    hr = ntf_map(fb, L"abc", 3, 3, 0, NULL, L"Arial", &ml, &font, &sc, NULL);
    t_note("textPosition == text length, length 0: %s mappedLength=%u", dw_hr_name(hr), ml);
    DW_RELEASE(font); DW_RELEASE(fb);
}

/* ---- synthetic base collection: deterministic fallback decisions -------------------------------------------------------------- */

TEST(fb_fixture_base_covers_text)
{
    NTF_BEGIN(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc; char fam[64];
    CHECK_HR(ntf_map(fb, L"abc xyz", 7, 0, 7, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
    EXPECT_EQ(ml, 7);                                       /* NtfMain covers every character of the run */
    CHECK(sc > 0.0f); CHECK_FLOAT_EQ(sc, 1.0f);
    if (font) { ntf_font_family(font, fam, sizeof fam); EXPECT_STR(fam, "NtfMain"); }
    else t_note("base font covers text: mappedFont is NULL");
    DW_RELEASE(font); DW_RELEASE(fb); NTF_END();
}

TEST(fb_fixture_run_splits_at_uncovered_char)
{
    NTF_BEGIN(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc;
    static const WCHAR t[] = L"ab\x3042";
    CHECK_HR(ntf_map(fb, t, 3, 0, 3, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
    EXPECT_EQ(ml, 2);                                       /* "ab" is covered by the base font, U+3042 is not: the run ends there */
    DW_RELEASE(font);
    CHECK_HR(ntf_map(fb, t, 3, 2, 1, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
    EXPECT_EQ(ml, 1);
    if (font) { CHECK_HR_OK_OR_FALSE(S_OK); { BOOL has = FALSE; CHECK_HR_OK(IDWriteFont_HasCharacter(font, 0x3042, &has)); CHECK(has); } }
    else t_note("no system font found for U+3042");
    DW_RELEASE(font); DW_RELEASE(fb); NTF_END();
}

TEST(fb_fixture_surrogate_pair_not_split)
{
    NTF_BEGIN(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc;
    static const WCHAR t[] = L"\xD83D\xDE00";
    CHECK_HR(ntf_map(fb, t, 2, 0, 2, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
    EXPECT_EQ(ml, 2);                                       /* one code point U+1F600: never reported as length 1 */
    if (font) { BOOL has = FALSE; CHECK_HR_OK(IDWriteFont_HasCharacter(font, 0x1f600, &has)); CHECK(has); }
    else t_note("no system font for U+1F600");
    DW_RELEASE(font); DW_RELEASE(fb); NTF_END();
}

TEST(fb_fixture_partition_covers_all)
{
    NTF_BEGIN(); NEED_FB(fb, f);
    static const WCHAR t[] = L"ab\x3042\x3044" L"cd\x2603" L"e\xD83D\xDE00" L"f";
    UINT32 total = (UINT32)(sizeof t / sizeof t[0]) - 1, pos = 0; int calls = 0;
    while (pos < total && calls < 20) {
        UINT32 ml; IDWriteFont *font = NULL; float sc;
        CHECK_HR(ntf_map(fb, t, total, pos, total - pos, X.coll, NTF_MAIN, &ml, &font, &sc, NULL), S_OK);
        CHECKF(ml >= 1 && ml <= total - pos, "pos %u -> mappedLength %u", pos, ml);
        if (font) { BOOL has = FALSE; UINT32 cp = t[pos]; if (cp >= 0xD800 && cp < 0xDC00 && pos + 1 < total) cp = 0x10000 + ((cp - 0xD800) << 10) + (t[pos + 1] - 0xDC00);
                    CHECK_HR_OK(IDWriteFont_HasCharacter(font, cp, &has)); CHECKF(has, "mapped font lacks U+%X at %u", cp, pos); }
        DW_RELEASE(font);
        if (ml == 0 || ml > total - pos) break;
        pos += ml; calls++;
    }
    EXPECT_EQ(pos, total);
    t_note("partition took %d calls", calls);
    DW_RELEASE(fb); NTF_END();
}

TEST(fb_fixture_locale_passed_through_source)
{
    NTF_BEGIN(); NEED_FB(fb, f); dwx_ana_src *s = dwx_ana_src_new(L"ab\x3042", 3); UINT32 ml = 0; IDWriteFont *font = NULL; float sc = 0;
    wcscpy(s->locale, L"ja-JP");
    CHECK_HR(IDWriteFontFallback_MapCharacters(fb, (IDWriteTextAnalysisSource *)s, 2, 1, X.coll, NTF_MAIN, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &ml, &font, &sc), S_OK);
    t_note("ja-JP locale: source GetLocaleName calls %ld", s->n_locale);
    CHECK(ml == 1);
    DW_RELEASE(font); dwx_release(s); DW_RELEASE(fb); NTF_END();
}

TEST(fb_fixture_source_chunked_text)
{
    /* The source may return the text in small chunks; the answer must not depend on the chunking. */
    NTF_BEGIN(); NEED_FB(fb, f); static const WCHAR t[] = L"abcdef\x3042"; UINT32 ml1, ml2; IDWriteFont *f1 = NULL, *f2 = NULL; float sc; dwx_ana_src *s;
    CHECK_HR(ntf_map(fb, t, 7, 0, 7, X.coll, NTF_MAIN, &ml1, &f1, &sc, NULL), S_OK);
    s = dwx_ana_src_new(t, 7); s->chunk = 2;
    CHECK_HR(IDWriteFontFallback_MapCharacters(fb, (IDWriteTextAnalysisSource *)s, 0, 7, X.coll, NTF_MAIN, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &ml2, &f2, &sc), S_OK);
    EXPECT_EQ(ml1, 6); EXPECT_EQ(ml2, ml1);
    DW_RELEASE(f1); DW_RELEASE(f2); dwx_release(s); DW_RELEASE(fb); NTF_END();
}

TEST(fb_fixture_source_failure_note)
{
    NTF_BEGIN(); NEED_FB(fb, f); dwx_ana_src *s = dwx_ana_src_new(L"abc\x3042", 4); UINT32 ml = 0; IDWriteFont *font = NULL; float sc = 0; HRESULT hr;
    s->fail_locale = E_FAIL;
    hr = IDWriteFontFallback_MapCharacters(fb, (IDWriteTextAnalysisSource *)s, 0, 4, X.coll, NTF_MAIN, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &ml, &font, &sc);
    t_note("GetLocaleName failing: %s (locale calls %ld)", dw_hr_name(hr), s->n_locale);
    DW_RELEASE(font); dwx_release(s); DW_RELEASE(fb); NTF_END();
}

/* ---- real system fonts: relations and notes ------------------------------------------------------------------------------------- */

static void sys_map(const WCHAR *text, const WCHAR *base, UINT32 pos, UINT32 len, UINT32 *ml, char *fam, float *sc, BOOL *have_first_cp, UINT32 first_cp)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFallback *fb = sysfb(f); IDWriteFont *font = NULL; IDWriteFontCollection *sys = NULL;
    fam[0] = 0; *ml = 0; if (have_first_cp) *have_first_cp = FALSE;
    if (!fb) return;
    IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE);
    CHECK_HR(ntf_map(fb, text, (UINT32)wcslen(text), pos, len, sys, base, ml, &font, sc, NULL), S_OK);
    if (font) { ntf_font_family(font, fam, 64); if (have_first_cp) IDWriteFont_HasCharacter(font, first_cp, have_first_cp); }
    DW_RELEASE(font); DW_RELEASE(sys); DW_RELEASE(fb);
}

TEST(fb_sys_ascii_with_arial_full_length)
{
    IDWriteFactory *f = dw_shared(); UINT32 ml; char fam[64]; float sc = 0; BOOL has;
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial not installed"); return; }
    sys_map(L"Hello, world 123", L"Arial", 0, 16, &ml, fam, &sc, &has, 'H');
    EXPECT_EQ(ml, 16);                                  /* Arial covers all of it */
    if (fam[0]) { t_note("mappedFont for covered ASCII: %s", fam); CHECK(has); } else t_note("mappedFont is NULL when the base font covers the text");
    t_note("scale %g", sc); CHECK(sc > 0.0f);
}

TEST(fb_sys_ascii_with_other_bases)
{
    IDWriteFactory *f = dw_shared(); static const WCHAR *fams[] = { L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Consolas", L"Calibri", L"Verdana" }; unsigned i;
    for (i = 0; i < sizeof fams / sizeof fams[0]; i++) {
        UINT32 ml; char fam[64]; float sc = 0;
        if (!dw_have_font(f, fams[i])) { t_note("%ls not installed", fams[i]); continue; }
        sys_map(L"The quick brown fox 0123456789", fams[i], 0, 30, &ml, fam, &sc, NULL, 0);
        CHECKF(ml == 30, "%ls: mappedLength %u", fams[i], ml);
    }
}

TEST(fb_sys_arabic_with_tahoma)
{
    IDWriteFactory *f = dw_shared(); UINT32 ml; char fam[64]; float sc = 0; BOOL has;
    if (!dw_have_font(f, L"Tahoma")) { t_skip("Tahoma not installed"); return; }
    sys_map(L"\x0627\x0644\x0639\x0631\x0628\x064a\x0629", L"Tahoma", 0, 7, &ml, fam, &sc, &has, 0x0627);
    EXPECT_EQ(ml, 7);
    if (fam[0]) { t_note("Arabic with Tahoma base maps to %s", fam); CHECK(has); }
}

TEST(fb_sys_cjk_after_latin_with_arial)
{
    IDWriteFactory *f = dw_shared(); UINT32 ml; char fam[64]; float sc = 0; BOOL has;
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial not installed"); return; }
    sys_map(L"a\x3042\x3044", L"Arial", 0, 3, &ml, fam, &sc, &has, 'a');
    EXPECT_EQ(ml, 1);                                   /* Arial has 'a' and no hiragana: the run is just the first character */
    sys_map(L"a\x3042\x3044", L"Arial", 1, 2, &ml, fam, &sc, &has, 0x3042);
    CHECK(ml >= 1 && ml <= 2);
    if (fam[0]) { t_note("hiragana fallback family: %s (mappedLength %u scale %g)", fam, ml, sc); CHECK(has); }
    else t_note("no system font found for hiragana");
    sys_map(L"\x4e00\x4e01", L"Arial", 0, 2, &ml, fam, &sc, &has, 0x4e00);
    if (fam[0]) { t_note("ideograph fallback family: %s", fam); CHECK(has); }
    EXPECT_EQ(ml, 2);
}

TEST(fb_sys_emoji_pair)
{
    IDWriteFactory *f = dw_shared(); UINT32 ml; char fam[64]; float sc = 0; BOOL has;
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial not installed"); return; }
    sys_map(L"\xD83D\xDE00", L"Arial", 0, 2, &ml, fam, &sc, &has, 0x1f600);
    EXPECT_EQ(ml, 2);
    if (fam[0]) { t_note("U+1F600 maps to %s scale %g", fam, sc); CHECK(has); } else t_note("no font for U+1F600");
    sys_map(L"a\xD83D\xDE00" L"b", L"Arial", 0, 4, &ml, fam, &sc, &has, 'a');
    EXPECT_EQ(ml, 1);
    sys_map(L"a\xD83D\xDE00" L"b", L"Arial", 1, 3, &ml, fam, &sc, &has, 0x1f600);
    EXPECT_EQ(ml, 2);
}

TEST(fb_sys_symbol_snowman)
{
    IDWriteFactory *f = dw_shared(); UINT32 ml; char fam[64]; float sc = 0; BOOL has;
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial not installed"); return; }
    sys_map(L"\x2603", L"Arial", 0, 1, &ml, fam, &sc, &has, 0x2603);
    EXPECT_EQ(ml, 1);
    if (fam[0]) { t_note("U+2603 maps to %s", fam); CHECK(has); } else t_note("no font for U+2603");
}

TEST(fb_sys_scale_note_and_positive)
{
    IDWriteFactory *f = dw_shared(); UINT32 ml; char fam[64]; float sc = 0; unsigned i;
    static const WCHAR *txt[] = { L"a", L"\x3042", L"\x4e00", L"\xD83D\xDE00", L"\x2603", L"\x0627", L"\x05d0", L"\x0e01", L"\x0905" };
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial not installed"); return; }
    for (i = 0; i < sizeof txt / sizeof txt[0]; i++) {
        sys_map(txt[i], L"Arial", 0, (UINT32)wcslen(txt[i]), &ml, fam, &sc, NULL, 0);
        t_note("U+%04X base Arial -> %s mappedLength=%u scale=%g", txt[i][0], fam[0] ? fam : "(null)", ml, sc);
        CHECK(ml >= 1); CHECK(sc > 0.0f && sc <= 4.0f);
    }
}

TEST(fb_sys_weight_style_inputs_accepted)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); IDWriteFontCollection *sys = NULL; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3); UINT32 ml; IDWriteFont *font; float sc;
    if (!dw_have_font(f, L"Arial")) { dwx_release(s); DW_RELEASE(fb); t_skip("Arial not installed"); return; }
    IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE);
    font = NULL;
    CHECK_HR(IDWriteFontFallback_MapCharacters(fb, (IDWriteTextAnalysisSource *)s, 0, 3, sys, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, &ml, &font, &sc), S_OK);
    EXPECT_EQ(ml, 3);
    if (font) { DWRITE_FONT_WEIGHT w = IDWriteFont_GetWeight(font); t_note("bold italic condensed request -> weight %d style %d stretch %d", w, IDWriteFont_GetStyle(font), IDWriteFont_GetStretch(font)); }
    DW_RELEASE(font); DW_RELEASE(sys); dwx_release(s); DW_RELEASE(fb);
}

TEST(fb_sys_mapped_font_refcount)
{
    IDWriteFactory *f = dw_shared(); NEED_FB(fb, f); UINT32 ml; IDWriteFont *font = NULL; float sc;
    if (!dw_have_font(f, L"Arial")) { DW_RELEASE(fb); t_skip("Arial not installed"); return; }
    CHECK_HR(ntf_map(fb, L"a\x3042", 2, 1, 1, NULL, L"Arial", &ml, &font, &sc, NULL), S_OK);
    if (font) { ULONG r = IDWriteFont_AddRef(font); EXPECT_EQ(r, 2); IDWriteFont_Release(font); DW_RELEASE(font); }
    DW_RELEASE(fb);
}

/* ---- IDWriteFontFallback1::MapCharacters ----------------------------------------------------------------------------------------- */

static IDWriteFontFallback1 *sysfb1(IDWriteFactory *f)
{
    IDWriteFontFallback *fb = sysfb(f); IDWriteFontFallback1 *f1 = NULL;
    if (!fb) return NULL;
    if (FAILED(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&f1))) f1 = NULL;
    IDWriteFontFallback_Release(fb);
    return f1;
}

static HRESULT map1(IDWriteFontFallback1 *fb, IDWriteTextAnalysisSource *src, UINT32 pos, UINT32 len, IDWriteFontCollection *coll, const WCHAR *fam,
                    const DWRITE_FONT_AXIS_VALUE *ax, UINT32 nax, UINT32 *ml, float *sc, IDWriteFontFace5 **face)
{
    *ml = 0x1234; *sc = 7.0f; *face = NULL;
    return IDWriteFontFallback1_MapCharacters(fb, src, pos, len, coll, fam, ax, nax, ml, sc, face);
}

TEST(fb1_validation)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFallback1 *fb = sysfb1(f); UINT32 ml; float sc; IDWriteFontFace5 *face; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3);
    DWRITE_FONT_AXIS_VALUE ax = { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f };
    if (!fb) { dwx_release(s); t_skip("IDWriteFontFallback1 unavailable"); return; }
    CHECK_HR(map1(fb, NULL, 0, 3, NULL, L"Arial", NULL, 0, &ml, &sc, &face), E_INVALIDARG);
    EXPECT_EQ(ml, 0); CHECK_OUT_NULL(face); CHECK_FLOAT_EQ(sc, 1.0f);
    CHECK_HR(map1(fb, (IDWriteTextAnalysisSource *)s, 0xffffffffu, 2, NULL, L"Arial", NULL, 0, &ml, &sc, &face), E_INVALIDARG);
    EXPECT_EQ(ml, 0); CHECK_OUT_NULL(face); CHECK_FLOAT_EQ(sc, 1.0f);
    CHECK_HR(map1(fb, (IDWriteTextAnalysisSource *)s, 0, 3, NULL, L"Arial", NULL, 1, &ml, &sc, &face), E_INVALIDARG);       /* count without array */
    EXPECT_EQ(ml, 0); CHECK_OUT_NULL(face); CHECK_FLOAT_EQ(sc, 1.0f);
    CHECK_HR(map1(fb, (IDWriteTextAnalysisSource *)s, 0, 3, NULL, L"Arial", &ax, 0x10000, &ml, &sc, &face), E_INVALIDARG);   /* more than 0xFFFF axes */
    EXPECT_EQ(ml, 0); CHECK_OUT_NULL(face); CHECK_FLOAT_EQ(sc, 1.0f);
    CHECK_HR(map1(fb, (IDWriteTextAnalysisSource *)s, 0, 3, NULL, L"Arial", NULL, 0xffffffffu, &ml, &sc, &face), E_INVALIDARG);
    EXPECT_EQ(s->n_at, 0);
    dwx_release(s); DW_RELEASE(fb);
}

TEST(fb1_valid_without_axes)
{
    NTF_BEGIN(); IDWriteFontFallback1 *fb = sysfb1(f); UINT32 ml; float sc; IDWriteFontFace5 *face; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3); char fam[64];
    if (!fb) { dwx_release(s); NTF_END(); t_skip("IDWriteFontFallback1 unavailable"); return; }
    CHECK_HR(map1(fb, (IDWriteTextAnalysisSource *)s, 0, 3, X.coll, NTF_MAIN, NULL, 0, &ml, &sc, &face), S_OK);
    EXPECT_EQ(ml, 3); CHECK(sc > 0.0f);
    if (face) { ntf_face_family(face, fam, sizeof fam); EXPECT_STR(fam, "NtfMain"); IDWriteFontFace5_Release(face); } else t_note("face NULL for covered text");
    dwx_release(s); DW_RELEASE(fb); NTF_END();
}

TEST(fb1_axis_values_on_static_font)
{
    NTF_BEGIN(); IDWriteFontFallback1 *fb = sysfb1(f); UINT32 ml; float sc; IDWriteFontFace5 *face; dwx_ana_src *s = dwx_ana_src_new(L"abc", 3);
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 75.0f } };
    if (!fb) { dwx_release(s); NTF_END(); t_skip("IDWriteFontFallback1 unavailable"); return; }
    CHECK_HR(map1(fb, (IDWriteTextAnalysisSource *)s, 0, 3, X.coll, NTF_MAIN, ax, 2, &ml, &sc, &face), S_OK);
    EXPECT_EQ(ml, 3);
    DW_RELEASE(face);
    CHECK_HR(map1(fb, (IDWriteTextAnalysisSource *)s, 0, 3, X.coll, NTF_MAIN, ax, 1, &ml, &sc, &face), S_OK);
    DW_RELEASE(face);
    dwx_release(s); DW_RELEASE(fb); NTF_END();
}

TEST(fb1_split_matches_v0)
{
    NTF_BEGIN(); IDWriteFontFallback1 *fb1 = sysfb1(f); IDWriteFontFallback *fb0 = sysfb(f); static const WCHAR t[] = L"ab\x3042"; UINT32 m0, m1; float s0, s1;
    IDWriteFont *font = NULL; IDWriteFontFace5 *face = NULL; dwx_ana_src *s = dwx_ana_src_new(t, 3);
    if (!fb1 || !fb0) { dwx_release(s); DW_RELEASE(fb1); DW_RELEASE(fb0); NTF_END(); t_skip("fallback unavailable"); return; }
    CHECK_HR(ntf_map(fb0, t, 3, 0, 3, X.coll, NTF_MAIN, &m0, &font, &s0, NULL), S_OK);
    CHECK_HR(map1(fb1, (IDWriteTextAnalysisSource *)s, 0, 3, X.coll, NTF_MAIN, NULL, 0, &m1, &s1, &face), S_OK);
    EXPECT_EQ(m0, m1); CHECK_FLOAT_EQ(s0, s1);
    DW_RELEASE(font); DW_RELEASE(face); dwx_release(s); DW_RELEASE(fb1); DW_RELEASE(fb0); NTF_END();
}

TEST(fb1_face_for_fallback_char_has_glyph)
{
    NTF_BEGIN(); IDWriteFontFallback1 *fb = sysfb1(f); UINT32 ml; float sc; IDWriteFontFace5 *face = NULL; dwx_ana_src *s = dwx_ana_src_new(L"ab\x3042", 3);
    if (!fb) { dwx_release(s); NTF_END(); t_skip("IDWriteFontFallback1 unavailable"); return; }
    CHECK_HR(map1(fb, (IDWriteTextAnalysisSource *)s, 2, 1, X.coll, NTF_MAIN, NULL, 0, &ml, &sc, &face), S_OK);
    EXPECT_EQ(ml, 1);
    if (face) { UINT32 cp = 0x3042; UINT16 gi = 0; CHECK_HR_OK(IDWriteFontFace5_GetGlyphIndices(face, &cp, 1, &gi)); CHECK(gi != 0); IDWriteFontFace5_Release(face); }
    else t_note("no face for U+3042");
    dwx_release(s); DW_RELEASE(fb); NTF_END();
}
