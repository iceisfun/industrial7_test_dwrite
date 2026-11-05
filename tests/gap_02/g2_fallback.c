/* IDWriteFontFallback1::MapCharacters (the overload with font axis values, vtable slot 4).
 * Win10 decompilation (dwrite.dll!DWriteFontFallback::MapCharacters 0xe5390): *mappedLength = 0, *mappedFontFace = NULL, *scale = 1.0
 * first; then E_INVALIDARG when the analysis source is NULL, when textPosition + textLength wraps past 2^32, when fontAxisValueCount
 * is above 0xFFFF, or when the count is non-zero and the axis array is NULL (the source is not consulted in those cases). Otherwise the
 * shared FontFallback::MapCharacters picks a face and the result is QueryInterface'd to IDWriteFontFace5. */
#include "g2.h"

typedef struct { IDWriteFontFallback1 *fb1; IDWriteFactory2 *f2; } fbx;
static IDWriteFontFallback1 *get_fb1(IDWriteFactory *f)
{
    IDWriteFactory2 *f2 = NULL; IDWriteFontFallback *fb = NULL; IDWriteFontFallback1 *fb1 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2)) || !f2) return NULL;
    if (SUCCEEDED(IDWriteFactory2_GetSystemFontFallback(f2, &fb)) && fb) {
        if (FAILED(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&fb1))) fb1 = NULL;
        IDWriteFontFallback_Release(fb);
    }
    IDWriteFactory2_Release(f2);
    return fb1;
}
#define NEED_FB1(fb1) IDWriteFontFallback1 *fb1 = get_fb1(dw_shared()); if (!fb1) { t_skip("IDWriteFontFallback1 unavailable"); return; }

static HRESULT map1(IDWriteFontFallback1 *fb, dwx_ana_src *src, UINT32 pos, UINT32 len, IDWriteFontCollection *coll, const WCHAR *fam,
                    const DWRITE_FONT_AXIS_VALUE *ax, UINT32 nax, UINT32 *ml, float *sc, IDWriteFontFace5 **face)
{
    *ml = 0x1234; *sc = 7.0f; *face = (IDWriteFontFace5 *)G2_SENT;
    return IDWriteFontFallback1_MapCharacters(fb, (IDWriteTextAnalysisSource *)src, pos, len, coll, fam, ax, nax, ml, sc, face);
}

TEST(gap2_fb1_invalid_arg_table)
{
    NEED_FB1(fb);
    static const WCHAR txt[] = L"abcdef";
    dwx_ana_src *s = dwx_ana_src_new(txt, 6);
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 80.0f } };
    static const struct { int use_src; UINT32 pos, len; int use_ax; UINT32 nax; const char *why; } c[] = {
        { 0, 0, 3, 0, 0, "NULL source" }, { 0, 0, 0, 0, 0, "NULL source, empty range" }, { 0, 5, 1, 1, 1, "NULL source with axes" },
        { 1, 0xFFFFFFFFu, 1, 0, 0, "pos 0xFFFFFFFF len 1 wraps" }, { 1, 0xFFFFFFFFu, 0xFFFFFFFFu, 0, 0, "both max wraps" },
        { 1, 0x80000000u, 0x80000000u, 0, 0, "2^31 + 2^31 wraps" }, { 1, 1, 0xFFFFFFFFu, 0, 0, "1 + max wraps" }, { 1, 0xFFFFFFF0u, 0x11, 0, 0, "near the top wraps" },
        { 1, 0, 6, 0, 1, "count 1 without array" }, { 1, 0, 6, 0, 2, "count 2 without array" }, { 1, 0, 6, 0, 0xFFFF, "count 0xFFFF without array" },
        { 1, 0, 6, 1, 0x10000, "count 0x10000" }, { 1, 0, 6, 1, 0x10001, "count 0x10001" }, { 1, 0, 6, 1, 0x7FFFFFFF, "count INT_MAX" },
        { 1, 0, 6, 1, 0xFFFFFFFFu, "count UINT_MAX" }, { 1, 0, 6, 0, 0xFFFFFFFFu, "count UINT_MAX without array" },
    };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr;
        hr = map1(fb, c[i].use_src ? s : NULL, c[i].pos, c[i].len, NULL, L"Arial", c[i].use_ax ? ax : NULL, c[i].nax, &ml, &sc, &face);
        CHECKF(hr == E_INVALIDARG, "%s: %s", c[i].why, dw_hr_name(hr));
        CHECKF(ml == 0 && sc == 1.0f && face == NULL, "%s: outputs ml %u scale %g face %p", c[i].why, ml, sc, (void *)face);
        CHECKF(s->n_at == 0 && s->n_before == 0 && s->n_locale == 0 && s->n_dir == 0, "%s: the source was consulted", c[i].why);
    }
    dwx_release(s); DW_RELEASE(fb);
}

TEST(gap2_fb1_zero_axis_count_ignores_array)
{
    NEED_FB1(fb);
    dwx_ana_src *s = dwx_ana_src_new(L"abc", 3); UINT32 ml, ml2; float sc, sc2; IDWriteFontFace5 *face, *face2; HRESULT h1, h2;
    DWRITE_FONT_AXIS_VALUE ax = { DWRITE_FONT_AXIS_TAG_WEIGHT, 900.0f };
    h1 = map1(fb, s, 0, 3, NULL, L"Arial", NULL, 0, &ml, &sc, &face);
    h2 = map1(fb, s, 0, 3, NULL, L"Arial", &ax, 0, &ml2, &sc2, &face2);
    CHECK(h1 == h2 && ml == ml2 && sc == sc2);
    if (SUCCEEDED(h1) && face && face != G2_SENT) IDWriteFontFace5_Release(face);
    if (SUCCEEDED(h2) && face2 && face2 != G2_SENT) IDWriteFontFace5_Release(face2);
    dwx_release(s); DW_RELEASE(fb);
}

/* Walk the whole text: every call must make progress and the runs must tile [0,len). */
static void walk(IDWriteFontFallback1 *fb, const WCHAR *text, UINT32 len, UINT32 chunk, IDWriteFontCollection *coll, const WCHAR *fam, UINT32 *bounds, int *nb)
{
    dwx_ana_src *s = dwx_ana_src_new(text, len); UINT32 pos = 0; int guard = 0;
    s->chunk = chunk; *nb = 0;
    while (pos < len && guard++ < 200) {
        UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr = map1(fb, s, pos, len - pos, coll, fam, NULL, 0, &ml, &sc, &face);
        if (hr != S_OK) { CHECKF(0, "pos %u: %s", pos, dw_hr_name(hr)); break; }
        CHECKF(ml >= 1 && ml <= len - pos, "pos %u: mapped length %u of %u remaining", pos, ml, len - pos);
        if (face && face != G2_SENT) { CHECKF(sc > 0.0f, "scale %g", sc); IDWriteFontFace5_Release(face); }
        if (ml == 0 || ml > len - pos) break;
        pos += ml; bounds[(*nb)++] = pos;
    }
    CHECKF(pos == len, "walk ended at %u of %u", pos, len);
    dwx_release(s);
}
TEST(gap2_fb1_walk_tiles_mixed_text)
{
    NEED_FB1(fb);
    static const WCHAR txt[] = L"Hello \x4e16\x754c \x0645\x0631\x062d\x0628\x0627 123 \x20ac \x2603 end";
    UINT32 b[64]; int nb; UINT32 chunk;
    for (chunk = 0; chunk <= 2; chunk++) { walk(fb, txt, (UINT32)wcslen(txt), chunk, NULL, L"Arial", b, &nb); t_note("chunk %u: %d runs", chunk, nb); }
    DW_RELEASE(fb);
}
TEST(gap2_fb1_walk_never_splits_surrogate_pair)
{
    NEED_FB1(fb);
    static const WCHAR txt[] = L"a\xD83D\xDE00" L"b\xD83D\xDE80" L"c\xD83C\xDF55";
    UINT32 b[32]; int nb, i; UINT32 len = (UINT32)wcslen(txt);
    walk(fb, txt, len, 0, NULL, L"Arial", b, &nb);
    for (i = 0; i < nb; i++) {
        UINT32 e = b[i];
        if (e < len) CHECKF(!(txt[e - 1] >= 0xD800 && txt[e - 1] < 0xDC00 && txt[e] >= 0xDC00 && txt[e] < 0xE000), "run boundary %u splits a surrogate pair", e);
    }
    DW_RELEASE(fb);
}
TEST(gap2_fb1_walk_with_chunked_source_same_boundaries)
{
    NEED_FB1(fb);
    static const WCHAR txt[] = L"abc \x4e16\x754c def \x0645\x0631";
    UINT32 b0[64], b1[64]; int n0, n1, i;
    walk(fb, txt, (UINT32)wcslen(txt), 0, NULL, L"Arial", b0, &n0);
    walk(fb, txt, (UINT32)wcslen(txt), 1, NULL, L"Arial", b1, &n1);
    CHECKF(n0 == n1, "run counts %d vs %d", n0, n1);
    for (i = 0; i < n0 && i < n1; i++) CHECK(b0[i] == b1[i]);
    DW_RELEASE(fb);
}
TEST(gap2_fb1_latin_text_is_one_run_with_glyphs)
{
    NEED_FB1(fb);
    dwx_ana_src *s = dwx_ana_src_new(L"abc", 3); UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr;
    if (!dw_have_font(dw_shared(), L"Arial")) { dwx_release(s); DW_RELEASE(fb); t_skip("Arial not installed"); return; }
    hr = map1(fb, s, 0, 3, NULL, L"Arial", NULL, 0, &ml, &sc, &face);
    CHECK_HR(hr, S_OK);
    CHECK(ml == 3 && face != NULL && face != G2_SENT);
    if (face && face != G2_SENT) {
        UINT32 cps[3] = { 'a', 'b', 'c' }; UINT16 g[3] = { 0, 0, 0 };
        CHECK_HR_OK(IDWriteFontFace5_GetGlyphIndices(face, cps, 3, g));
        CHECK(g[0] != 0 && g[1] != 0 && g[2] != 0);
        G2_SOFT(sc == 1.0f);
        IDWriteFontFace5_Release(face);
    }
    CHECK(s->n_at > 0);
    dwx_release(s); DW_RELEASE(fb);
}
TEST(gap2_fb1_null_family_and_collection)
{
    NEED_FB1(fb);
    dwx_ana_src *s = dwx_ana_src_new(L"abc", 3); UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr;
    hr = map1(fb, s, 0, 3, NULL, NULL, NULL, 0, &ml, &sc, &face);
    CHECK_HR(hr, S_OK);
    CHECK(ml >= 1 && ml <= 3);
    if (SUCCEEDED(hr) && face && face != G2_SENT) IDWriteFontFace5_Release(face);
    dwx_release(s); DW_RELEASE(fb);
}
TEST(gap2_fb1_unknown_family_still_maps)
{
    NEED_FB1(fb);
    dwx_ana_src *s = dwx_ana_src_new(L"abc", 3); UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr;
    hr = map1(fb, s, 0, 3, NULL, L"No Such Family G2", NULL, 0, &ml, &sc, &face);
    CHECK_HR(hr, S_OK);
    CHECK(ml >= 1);
    if (SUCCEEDED(hr) && face && face != G2_SENT) IDWriteFontFace5_Release(face);
    dwx_release(s); DW_RELEASE(fb);
}
TEST(gap2_fb1_unassigned_code_point)
{
    NEED_FB1(fb);
    dwx_ana_src *s = dwx_ana_src_new(L"\x0378", 1); UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr;
    hr = map1(fb, s, 0, 1, NULL, L"Arial", NULL, 0, &ml, &sc, &face);
    CHECK_HR(hr, S_OK);
    CHECKF(ml == 1, "mapped length %u", ml);
    t_note("U+0378: face %s scale %g", (face && face != G2_SENT) ? "found" : "none", sc);
    if (SUCCEEDED(hr) && face && face != G2_SENT) IDWriteFontFace5_Release(face);
    else if (face == NULL) CHECK(sc >= 0.0f);
    dwx_release(s); DW_RELEASE(fb);
}
TEST(gap2_fb1_empty_range)
{
    NEED_FB1(fb);
    dwx_ana_src *s = dwx_ana_src_new(L"abc", 3); UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr;
    hr = map1(fb, s, 1, 0, NULL, L"Arial", NULL, 0, &ml, &sc, &face);
    t_note("empty range -> %s ml %u", dw_hr_name(hr), ml);
    if (SUCCEEDED(hr)) CHECK(ml == 0);
    if (SUCCEEDED(hr) && face && face != G2_SENT) IDWriteFontFace5_Release(face);
    dwx_release(s); DW_RELEASE(fb);
}
TEST(gap2_fb1_custom_collection_stops_where_coverage_ends)
{
    NEED_FB1(fb);
    g2_coll c; ttf_spec sp;
    g2_spec(&sp, "G2Fb");
    if (!g2_coll_open(&c, &sp, 1)) { DW_RELEASE(fb); return; }
    {
        static const WCHAR txt[] = L"ABC\x4e00";
        dwx_ana_src *s = dwx_ana_src_new(txt, 4); UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr;
        hr = map1(fb, s, 0, 4, c.coll, L"G2Fb", NULL, 0, &ml, &sc, &face);
        CHECK_HR(hr, S_OK);
        CHECKF(ml == 3, "ABC is covered by the base family, the CJK ideograph is not: mapped %u", ml);
        if (face && face != G2_SENT) {
            IDWriteLocalizedStrings *names = NULL; char nm[64] = "";
            if (SUCCEEDED(IDWriteFontFace5_GetFamilyNames(face, &names)) && names) { dw_ls_en(names, nm, sizeof nm); DW_RELEASE(names); }
            EXPECT_STR(nm, "G2Fb");
            CHECK(sc == 1.0f);
            IDWriteFontFace5_Release(face);
        } else CHECK(0);
        dwx_release(s);
    }
    g2_coll_close(&c);
    DW_RELEASE(fb);
}
TEST(gap2_fb1_custom_collection_second_run_leaves_base_family)
{
    NEED_FB1(fb);
    g2_coll c; ttf_spec sp;
    g2_spec(&sp, "G2Fb2");
    if (!g2_coll_open(&c, &sp, 1)) { DW_RELEASE(fb); return; }
    {
        static const WCHAR txt[] = L"ABC\x4e00";
        dwx_ana_src *s = dwx_ana_src_new(txt, 4); UINT32 ml; float sc; IDWriteFontFace5 *face; HRESULT hr;
        hr = map1(fb, s, 3, 1, c.coll, L"G2Fb2", NULL, 0, &ml, &sc, &face);
        CHECK_HR(hr, S_OK);
        CHECKF(ml == 1, "mapped %u", ml);
        if (face && face != G2_SENT) {
            IDWriteLocalizedStrings *names = NULL; char nm[64] = "";
            if (SUCCEEDED(IDWriteFontFace5_GetFamilyNames(face, &names)) && names) { dw_ls_en(names, nm, sizeof nm); DW_RELEASE(names); }
            CHECKF(strcmp(nm, "G2Fb2") != 0, "the base family cannot supply U+4E00 but was returned");
            IDWriteFontFace5_Release(face);
        } else t_note("no font covers U+4E00 on this system");
        dwx_release(s);
    }
    g2_coll_close(&c);
    DW_RELEASE(fb);
}
TEST(gap2_fb1_axis_values_do_not_change_static_mapping)
{
    NEED_FB1(fb);
    g2_coll c; ttf_spec sp;
    g2_spec(&sp, "G2Fb3");
    if (!g2_coll_open(&c, &sp, 1)) { DW_RELEASE(fb); return; }
    {
        dwx_ana_src *s = dwx_ana_src_new(L"ABC", 3); UINT32 ml0, ml1; float sc0, sc1; IDWriteFontFace5 *f0, *f1;
        DWRITE_FONT_AXIS_VALUE ax[3] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 100.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 50.0f }, { DWRITE_FONT_AXIS_TAG_ITALIC, 1.0f } };
        CHECK_HR(map1(fb, s, 0, 3, c.coll, L"G2Fb3", NULL, 0, &ml0, &sc0, &f0), S_OK);
        CHECK_HR(map1(fb, s, 0, 3, c.coll, L"G2Fb3", ax, 3, &ml1, &sc1, &f1), S_OK);
        CHECK(ml0 == ml1 && sc0 == sc1);
        if (f0 && f0 != G2_SENT) IDWriteFontFace5_Release(f0);
        if (f1 && f1 != G2_SENT) IDWriteFontFace5_Release(f1);
        dwx_release(s);
    }
    g2_coll_close(&c);
    DW_RELEASE(fb);
}
TEST(gap2_fb1_fallback_object_identity_and_qi)
{
    IDWriteFactory *f = dw_shared(); IDWriteFactory2 *f2 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2); IDWriteFontFallback *a = NULL, *b = NULL; IDWriteFontFallback1 *a1 = NULL;
    (void)f;
    if (!f2) { t_skip("Factory2 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &a)); CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &b));
    CHECK(a && b && a != b);                                  /* a new object per call */
    if (a) {
        HRESULT hr = IDWriteFontFallback_QueryInterface(a, &IID_IDWriteFontFallback1, (void **)&a1);
        if (SUCCEEDED(hr)) CHECK(g2_ident(a) == g2_ident(a1)); else t_note("Fallback1 not exposed: %s", dw_hr_name(hr));
        EXPECT_REFS(a, 1 + (a1 ? 1 : 0));
    }
    DW_RELEASE(a1); DW_RELEASE(a); DW_RELEASE(b);
}
