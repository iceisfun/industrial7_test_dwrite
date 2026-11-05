#include "r2fs.h"

/* IDWriteFontFallback1::MapCharacters (DWriteFontFallback::MapCharacters with axis values on Windows 10): same lookup as the v0 method
 * (the base family first, then the mappings) but takes variable-font axis values instead of weight/style/stretch and returns an
 * IDWriteFontFace5 (the font object QueryInterface'd for IID_IDWriteFontFace5; a failing QI is returned as the HRESULT).
 * Outputs are zeroed first; validation is described in args.c. */

static IDWriteFontFallback1 *v1_of(IDWriteFontFallback *fb)
{
    IDWriteFontFallback1 *p = NULL;
    if (!fb || FAILED(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&p))) return NULL;
    return p;
}

typedef struct { HRESULT hr; UINT32 ml; float scale; IDWriteFontFace5 *face; r2_finfo fi; } r1;

static HRESULT map1(IDWriteFontFallback1 *fb1, const WCHAR *text, UINT32 pos, UINT32 len, IDWriteFontCollection *coll, const WCHAR *family,
                    const DWRITE_FONT_AXIS_VALUE *ax, UINT32 nax, const WCHAR *locale, r1 *out, dwx_ana_src **src_out)
{
    dwx_ana_src *s = dwx_ana_src_new(text, (UINT32)wcslen(text));
    memset(out, 0, sizeof *out); out->ml = 777; out->scale = 7.0f;
    if (locale) lstrcpynW(s->locale, locale, LOCALE_NAME_MAX_LENGTH);
    out->hr = IDWriteFontFallback1_MapCharacters(fb1, (IDWriteTextAnalysisSource *)s, pos, len, coll, family, ax, nax, &out->ml, &out->scale, &out->face);
    if (out->face) r2_face_info(out->face, &out->fi);
    if (src_out) *src_out = s; else dwx_release(s);
    return out->hr;
}

#define V1_BEGIN() R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); IDWriteFontFallback1 *fb1 = v1_of(fb); \
    if (!fb1) { DW_RELEASE(fb); R2_END(); t_skip("IDWriteFontFallback1 unavailable"); return; }
#define V1_END() do { DW_RELEASE(fb1); DW_RELEASE(fb); R2_END(); } while (0)

RT(v1_returns_the_face_of_the_target_family)
{
    V1_BEGIN(); r1 r;
    static const struct { const WCHAR *text; const char *fam; UINT32 ml; } rows[] = {
        { L"abc", "R2Main", 3 }, { L"\x3042", "R2Cjk", 1 }, { L"\x3042\x3044", "R2Cjk", 2 }, { L"\x2603", "R2Sym", 1 }, { L"\xD83D\xDE00", "R2Emoji", 2 }, { L"\x0301", "R2Mark", 1 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        CHECK_HR_OK(map1(fb1, rows[i].text, 0, (UINT32)wcslen(rows[i].text), X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
        CHECKF(r.face != NULL && !strcmp(r.fi.fam, rows[i].fam) && r.ml == rows[i].ml, "row %u: family \"%s\" mappedLength %u want \"%s\" %u", i, r.fi.fam, r.ml, rows[i].fam, rows[i].ml);
        CHECK_FLOAT_EQ(r.scale, 1.0f);
        DW_RELEASE(r.face);
    }
    V1_END();
}

RT(v1_matches_v0_for_default_properties)
{
    V1_BEGIN(); unsigned i;
    static const WCHAR *texts[] = { L"abc", L"\x3042\x3044", L"\x2603\x2605", L"\xD83D\xDE00", L"a\x3042", L"\x0905", L"\x4e00\x4e01\x4e02" };
    for (i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        r2_args a; r2_res r0; r1 r;
        UINT32 tl = (UINT32)wcslen(texts[i]);
        r2_defaults(&a, &X);
        CHECK_HR_OK(r2_map(fb, texts[i], tl, &a, &r0, NULL));
        CHECK_HR_OK(map1(fb1, texts[i], 0, tl, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
        CHECKF(r.ml == r0.ml, "text %u: v1 mappedLength %u v0 %u", i, r.ml, r0.ml);
        CHECKF(r.scale == r0.scale, "text %u: v1 scale %g v0 %g", i, r.scale, r0.scale);
        CHECKF((r.face != NULL) == (r0.font != NULL), "text %u: v1 face %p v0 font %p", i, (void *)r.face, (void *)r0.font);
        if (r.face && r0.font) CHECKF(!strcmp(r.fi.fam, r0.fi.fam), "text %u: v1 \"%s\" v0 \"%s\"", i, r.fi.fam, r0.fi.fam);
        DW_RELEASE(r.face); DW_RELEASE(r0.font);
    }
    V1_END();
}

RT(v1_unmapped_character_gives_null_face_and_scale_one)
{
    V1_BEGIN(); r1 r;
    CHECK_HR_OK(map1(fb1, L"\x0905", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
    CHECK(r.face == NULL); EXPECT_EQ(r.ml, 1); CHECK_FLOAT_EQ(r.scale, 1.0f);
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_scale_comes_from_the_mapping)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; IDWriteFontFallback1 *fb1; r1 r;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 0.75f));
    fb = r2_fb(b); fb1 = v1_of(fb);
    if (fb1) {
        CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
        CHECK(r.face != NULL); CHECK_FLOAT_EQ(r.scale, 0.75f); EXPECT_STR(r.fi.fam, "R2Cjk");
        DW_RELEASE(r.face);
    }
    DW_RELEASE(fb1); DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(v1_range_inside_the_text)
{
    V1_BEGIN(); r1 r;
    CHECK_HR_OK(map1(fb1, L"ab\x3042\x3044" L"cd", 2, 2, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
    EXPECT_EQ(r.ml, 2); CHECK(r.face != NULL); EXPECT_STR(r.fi.fam, "R2Cjk");
    DW_RELEASE(r.face);
    CHECK_HR_OK(map1(fb1, L"ab\x3042\x3044" L"cd", 0, 2, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
    EXPECT_EQ(r.ml, 2); EXPECT_STR(r.fi.fam, "R2Main");
    DW_RELEASE(r.face);
    CHECK_HR_OK(map1(fb1, L"ab\x3042\x3044" L"cd", 4, 2, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
    EXPECT_EQ(r.ml, 2); EXPECT_STR(r.fi.fam, "R2Main");
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_face_is_a_working_fontface5)
{
    V1_BEGIN(); r1 r;
    CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
    if (r.face) {
        UINT32 cp = 0x3042; UINT16 g = 0; IDWriteFontFace *base = NULL; IDWriteFontFace3 *f3 = NULL;
        CHECK_HR_OK(IDWriteFontFace5_QueryInterface(r.face, &IID_IDWriteFontFace, (void **)&base));
        if (base) { CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(base, &cp, 1, &g)); EXPECT_EQ(g, 67); DW_RELEASE(base); }
        CHECK_HR_OK(IDWriteFontFace5_QueryInterface(r.face, &IID_IDWriteFontFace3, (void **)&f3));
        if (f3) { CHECK(IDWriteFontFace3_GetWeight(f3) == DWRITE_FONT_WEIGHT_NORMAL); CHECK(IDWriteFontFace3_GetStyle(f3) == DWRITE_FONT_STYLE_NORMAL); DW_RELEASE(f3); }
        CHECK(!IDWriteFontFace5_HasVariations(r.face));                                    /* synthetic static font */
        t_note("static mapped face: GetFontAxisValueCount %u", (unsigned)IDWriteFontFace5_GetFontAxisValueCount(r.face));
    } else CHECK(r.face != NULL);
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_face_survives_the_fallback_and_collection)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; IDWriteFontFallback1 *fb1; r1 r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b); fb1 = v1_of(fb);
    memset(&r, 0, sizeof r);
    if (fb1) CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));
    DW_RELEASE(fb1); DW_RELEASE(fb); DW_RELEASE(b);
    if (r.face) {
        UINT32 cp = 0x3044; UINT16 g = 0; IDWriteFontFace *base = NULL;
        CHECK_HR_OK(IDWriteFontFace5_QueryInterface(r.face, &IID_IDWriteFontFace, (void **)&base));
        if (base) { CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(base, &cp, 1, &g)); EXPECT_EQ(g, 67); DW_RELEASE(base); }
    } else CHECK(r.face != NULL);
    DW_RELEASE(r.face); R2_END();
}

RT(v1_axis_values_with_a_static_ladder_family_note)
{
    V1_BEGIN(); r1 r; unsigned i;
    static const float w[] = { 300, 400, 700, 900 };
    for (i = 0; i < 4; i++) {
        DWRITE_FONT_AXIS_VALUE ax[1];
        ax[0].axisTag = DWRITE_FONT_AXIS_TAG_WEIGHT; ax[0].value = w[i];
        CHECK_HR_OK(map1(fb1, L"abc", 0, 3, X.coll, R2_LADDER, ax, 1, NULL, &r, NULL));
        CHECK(r.face != NULL); EXPECT_EQ(r.ml, 3);
        t_note("R2Ladder with wght=%g -> weight %d style %d stretch %d", w[i], r.fi.w, r.fi.st, r.fi.sx);
        DW_RELEASE(r.face);
    }
    CHECK_HR_OK(map1(fb1, L"abc", 0, 3, X.coll, R2_LADDER, NULL, 0, NULL, &r, NULL));
    CHECK(r.face != NULL); t_note("R2Ladder without axis values -> weight %d style %d stretch %d", r.fi.w, r.fi.st, r.fi.sx);
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_axis_values_for_italic_and_width_note)
{
    V1_BEGIN(); r1 r;
    DWRITE_FONT_AXIS_VALUE ax[3];
    ax[0].axisTag = DWRITE_FONT_AXIS_TAG_WEIGHT; ax[0].value = 700;
    ax[1].axisTag = DWRITE_FONT_AXIS_TAG_ITALIC; ax[1].value = 1;
    ax[2].axisTag = DWRITE_FONT_AXIS_TAG_WIDTH;  ax[2].value = 100;
    CHECK_HR_OK(map1(fb1, L"abc", 0, 3, X.coll, R2_LADDER, ax, 3, NULL, &r, NULL));
    CHECK(r.face != NULL); t_note("wght 700 ital 1 wdth 100 -> weight %d style %d stretch %d", r.fi.w, r.fi.st, r.fi.sx);
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_axis_values_do_not_change_what_a_static_target_covers)
{
    V1_BEGIN(); r1 r; DWRITE_FONT_AXIS_VALUE ax[1];
    ax[0].axisTag = DWRITE_FONT_AXIS_TAG_WEIGHT; ax[0].value = 700;
    CHECK_HR_OK(map1(fb1, L"a\x3042", 0, 2, X.coll, R2_MAIN, ax, 1, NULL, &r, NULL));
    EXPECT_EQ(r.ml, 1); CHECK(r.face != NULL); EXPECT_STR(r.fi.fam, "R2Main");
    DW_RELEASE(r.face);
    CHECK_HR_OK(map1(fb1, L"a\x3042", 1, 1, X.coll, R2_MAIN, ax, 1, NULL, &r, NULL));
    EXPECT_EQ(r.ml, 1); CHECK(r.face != NULL); EXPECT_STR(r.fi.fam, "R2Cjk");
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_unknown_axis_tag_is_accepted)
{
    V1_BEGIN(); r1 r; DWRITE_FONT_AXIS_VALUE ax[2];
    ax[0].axisTag = (DWRITE_FONT_AXIS_TAG)0x20202020; ax[0].value = 5;
    ax[1].axisTag = DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE; ax[1].value = 12;
    CHECK_HR_OK(map1(fb1, L"abc", 0, 3, X.coll, R2_MAIN, ax, 2, NULL, &r, NULL));
    EXPECT_EQ(r.ml, 3); CHECK(r.face != NULL);
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_source_locale_not_queried_when_base_family_covers)
{
    V1_BEGIN(); r1 r; dwx_ana_src *s = NULL;
    CHECK_HR_OK(map1(fb1, L"abc", 0, 3, X.coll, R2_MAIN, NULL, 0, NULL, &r, &s));
    EXPECT_EQ(s->n_locale, 0); EXPECT_EQ(r.ml, 3);
    dwx_release(s); DW_RELEASE(r.face);
    CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &r, &s));
    CHECK(s->n_locale >= 1);
    dwx_release(s); DW_RELEASE(r.face);
    V1_END();
}

RT(v1_locale_filter_applies)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; IDWriteFontFallback1 *fb1; r1 r;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, L"ja-JP", NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK2, X.coll, L"zh-CN", NULL, 1.0f));
    fb = r2_fb(b); fb1 = v1_of(fb);
    if (fb1) {
        CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, L"ja-JP", &r, NULL)); CHECK(r.face != NULL); EXPECT_STR(r.fi.fam, "R2Cjk"); DW_RELEASE(r.face);
        CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, L"zh-CN", &r, NULL)); CHECK(r.face != NULL); EXPECT_STR(r.fi.fam, "R2Cjk2"); DW_RELEASE(r.face);
    }
    DW_RELEASE(fb1); DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(v1_base_family_filter_applies)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; IDWriteFontFallback1 *fb1; r1 r;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, R2_ALT, 1.0f));
    fb = r2_fb(b); fb1 = v1_of(fb);
    if (fb1) {
        CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_ALT, NULL, 0, NULL, &r, NULL)); CHECK(r.face != NULL); DW_RELEASE(r.face);
        if (!t_is_wine()) { CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL)); CHECK(r.face == NULL); DW_RELEASE(r.face); }
    }
    DW_RELEASE(fb1); DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(v1_null_collection_and_family)
{
    V1_BEGIN(); r1 r;
    CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, NULL, NULL, NULL, 0, NULL, &r, NULL));
    EXPECT_EQ(r.ml, 1); CHECK(r.face != NULL);
    if (r.face) CHECK(r.fi.fam[0] != 0);
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_system_fallback_variable_font_note)
{
    IDWriteFactory *f = dw_shared(); IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2); IDWriteFontFallback *fb = NULL; IDWriteFontFallback1 *fb1;
    IDWriteFontCollection *sc = NULL; r1 r; DWRITE_FONT_AXIS_VALUE ax[2];
    if (!f2 || FAILED(IDWriteFactory2_GetSystemFontFallback(f2, &fb)) || !fb) { t_skip("system fallback unavailable"); return; }
    fb1 = v1_of(fb);
    if (!fb1 || FAILED(IDWriteFactory_GetSystemFontCollection(f, &sc, FALSE)) || !sc || !dw_have_font(f, L"Bahnschrift")) { DW_RELEASE(fb1); DW_RELEASE(sc); DW_RELEASE(fb); t_skip("Bahnschrift not installed"); return; }
    ax[0].axisTag = DWRITE_FONT_AXIS_TAG_WEIGHT; ax[0].value = 700;
    ax[1].axisTag = DWRITE_FONT_AXIS_TAG_WIDTH;  ax[1].value = 75;
    CHECK_HR_OK(map1(fb1, L"Hello", 0, 5, sc, L"Bahnschrift", ax, 2, NULL, &r, NULL));
    CHECK(r.face != NULL); EXPECT_EQ(r.ml, 5);
    if (r.face) {
        UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(r.face);
        t_note("Bahnschrift: family \"%s\" weight %d, HasVariations %d, %u axis values", r.fi.fam, r.fi.w, IDWriteFontFace5_HasVariations(r.face), n);
        CHECK(!strcmp(r.fi.fam, "Bahnschrift"));
        if (n && n <= 16) {
            DWRITE_FONT_AXIS_VALUE got[16]; UINT32 k;
            if (SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(r.face, got, n))) for (k = 0; k < n; k++) t_note("  axis %08x = %g", (unsigned)got[k].axisTag, got[k].value);
        }
    }
    DW_RELEASE(r.face); DW_RELEASE(fb1); DW_RELEASE(sc); DW_RELEASE(fb);
}

RT(v1_system_fallback_cjk_with_locale_note)
{
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2); IDWriteFontFallback *fb = NULL; IDWriteFontFallback1 *fb1; unsigned i;
    static const WCHAR *loc[] = { L"ja-JP", L"zh-CN", L"ko-KR", L"en-us" };
    r1 r;
    if (!f2 || FAILED(IDWriteFactory2_GetSystemFontFallback(f2, &fb)) || !fb) { t_skip("system fallback unavailable"); return; }
    fb1 = v1_of(fb);
    for (i = 0; fb1 && i < 4; i++) {
        CHECK_HR_OK(map1(fb1, L"\x76f4", 0, 1, NULL, NULL, NULL, 0, loc[i], &r, NULL));
        CHECKF(r.face != NULL, "no face for U+76F4 with locale %ls", loc[i]);
        if (r.face) t_note("U+76F4 locale %ls -> \"%s\"", loc[i], r.fi.fam);
        DW_RELEASE(r.face);
    }
    DW_RELEASE(fb1); DW_RELEASE(fb);
}

RT(v1_outs_are_overwritten_not_accumulated)
{
    V1_BEGIN(); r1 r;
    CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &r, NULL));        /* sentinel ml 777 / scale 7 were replaced */
    CHECK(r.ml == 1 && r.scale == 1.0f);
    DW_RELEASE(r.face);
    V1_END();
}

RT(v1_two_calls_give_independent_faces)
{
    V1_BEGIN(); r1 a, b;
    CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &a, NULL));
    CHECK_HR_OK(map1(fb1, L"\x3042", 0, 1, X.coll, R2_MAIN, NULL, 0, NULL, &b, NULL));
    CHECK(a.face != NULL && b.face != NULL);
    if (a.face && b.face) {
        ULONG ra = dw_refcount((IUnknown *)a.face);
        t_note("faces from two calls: %s object, refcount %lu", a.face == b.face ? "same" : "different", ra);
        DW_RELEASE(a.face);
        CHECK(dw_refcount((IUnknown *)b.face) >= 1);
        DW_RELEASE(b.face);
    }
    V1_END();
}
