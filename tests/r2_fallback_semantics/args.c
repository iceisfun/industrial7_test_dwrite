#include "r2fs.h"

/* Argument validation of the fallback family on Windows 10 (dwrite.dll):
 *   IDWriteFontFallback::MapCharacters (v0): writes *mappedLength = 0, *mappedFont = NULL, *scale = 1.0 BEFORE validating, then fails with
 *     E_INVALIDARG when the source is NULL, textPosition + textLength wraps past 2^32, (weight - 1) > 998 (so 1..999 are valid),
 *     style > 2 or (stretch - 1) > 8 (1..9 valid). Nothing else is checked (no range check against the text length).
 *   IDWriteFontFallback::MapCharacters (v1, IDWriteFontFallback1): same zeroing (face instead of font), E_INVALIDARG when the source is NULL,
 *     the position + length wraps, fontAxisValueCount > 0xFFFF, or fontAxisValueCount != 0 with a NULL value array.
 *   IDWriteFontFallbackBuilder::AddMapping: E_INVALIDARG when ranges == NULL, rangesCount == 0, targetFamilyNames == NULL,
 *     targetFamilyNamesCount == 0, scale < 0, or any of the first targetFamilyNamesCount names is NULL. Everything else is stored verbatim.
 *   IDWriteFontFallbackBuilder::AddMappings(NULL): E_INVALIDARG.   CreateFontFallback: out pointer zeroed, always S_OK.
 * Only objects created by DirectWrite may be passed to AddMappings (the implementation reinterprets the pointer), so these tests never do. */

typedef struct { const char *what; int null_src; UINT32 pos, len; UINT32 w, st, sx; } bad_row;

static void run_bad_rows(const bad_row *rows, int n, IDWriteFontFallback *fb)
{
    int i;
    for (i = 0; i < n; i++) {
        dwx_ana_src *src = dwx_ana_src_new(L"abc", 3); UINT32 ml = 0xABCD; IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1; float sc = 7.0f; HRESULT hr;
        hr = IDWriteFontFallback_MapCharacters(fb, rows[i].null_src ? NULL : (IDWriteTextAnalysisSource *)src, rows[i].pos, rows[i].len, NULL, NULL,
                                               (DWRITE_FONT_WEIGHT)rows[i].w, (DWRITE_FONT_STYLE)rows[i].st, (DWRITE_FONT_STRETCH)rows[i].sx, &ml, &font, &sc);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, rows[i].what, "hr %s", dw_hr_name(hr));
        t_check(ml == 0 && font == NULL && sc == 1.0f, __FILE__, __LINE__, rows[i].what, "outs not reset: ml %u font %p scale %g", ml, (void *)font, sc);
        dwx_release(src);
    }
}

#define OK_W DWRITE_FONT_WEIGHT_NORMAL
#define OK_S DWRITE_FONT_STYLE_NORMAL
#define OK_X DWRITE_FONT_STRETCH_NORMAL

RT(map_null_source_resets_outs)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const bad_row rows[] = {
        { "NULL source, pos 0 len 0", 1, 0, 0, OK_W, OK_S, OK_X }, { "NULL source, pos 0 len 3", 1, 0, 3, OK_W, OK_S, OK_X },
        { "NULL source, pos 5 len 1", 1, 5, 1, OK_W, OK_S, OK_X }, { "NULL source, pos 0xffffffff len 0", 1, 0xffffffff, 0, OK_W, OK_S, OK_X },
    };
    if (fb) { run_bad_rows(rows, 4, fb); DW_RELEASE(fb); }
    R2_END();
}

RT(map_null_source_resets_outs_system)
{
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2); IDWriteFontFallback *fb = NULL;
    static const bad_row rows[] = { { "NULL source (system)", 1, 0, 1, OK_W, OK_S, OK_X }, { "NULL source (system), huge", 1, 0, 0xffffffff, OK_W, OK_S, OK_X } };
    if (!f2) { t_skip("IDWriteFactory2 unavailable"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &fb));
    if (fb) { run_bad_rows(rows, 2, fb); DW_RELEASE(fb); }
}

RT(map_position_length_wrap)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const bad_row rows[] = {
        { "pos 0xffffffff len 1", 0, 0xffffffff, 1, OK_W, OK_S, OK_X }, { "pos 1 len 0xffffffff", 0, 1, 0xffffffff, OK_W, OK_S, OK_X },
        { "pos 0x80000000 len 0x80000000", 0, 0x80000000u, 0x80000000u, OK_W, OK_S, OK_X }, { "pos 0xfffffffe len 3", 0, 0xfffffffe, 3, OK_W, OK_S, OK_X },
        { "pos 3 len 0xfffffffe", 0, 3, 0xfffffffe, OK_W, OK_S, OK_X }, { "pos 0xffffffff len 0xffffffff", 0, 0xffffffff, 0xffffffff, OK_W, OK_S, OK_X },
        { "pos 0x7fffffff len 0x80000001", 0, 0x7fffffff, 0x80000001u, OK_W, OK_S, OK_X },
    };
    if (fb) { run_bad_rows(rows, 7, fb); DW_RELEASE(fb); }
    R2_END();
}

RT(map_weight_out_of_range)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const bad_row rows[] = {
        { "weight 0", 0, 0, 1, 0, OK_S, OK_X }, { "weight 1000", 0, 0, 1, 1000, OK_S, OK_X }, { "weight 1001", 0, 0, 1, 1001, OK_S, OK_X },
        { "weight 0xffff", 0, 0, 1, 0xffff, OK_S, OK_X }, { "weight 0x80000000", 0, 0, 1, 0x80000000u, OK_S, OK_X }, { "weight 0xffffffff", 0, 0, 1, 0xffffffffu, OK_S, OK_X },
        { "weight 5000", 0, 0, 1, 5000, OK_S, OK_X },
    };
    if (fb) { run_bad_rows(rows, 7, fb); DW_RELEASE(fb); }
    R2_END();
}

RT(map_style_out_of_range)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const bad_row rows[] = {
        { "style 3", 0, 0, 1, OK_W, 3, OK_X }, { "style 4", 0, 0, 1, OK_W, 4, OK_X }, { "style 0x100", 0, 0, 1, OK_W, 0x100, OK_X },
        { "style 0xffffffff", 0, 0, 1, OK_W, 0xffffffffu, OK_X },
    };
    if (fb) { run_bad_rows(rows, 4, fb); DW_RELEASE(fb); }
    R2_END();
}

RT(map_stretch_out_of_range)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const bad_row rows[] = {
        { "stretch 0 (undefined)", 0, 0, 1, OK_W, OK_S, 0 }, { "stretch 10", 0, 0, 1, OK_W, OK_S, 10 }, { "stretch 11", 0, 0, 1, OK_W, OK_S, 11 },
        { "stretch 0xffff", 0, 0, 1, OK_W, OK_S, 0xffff }, { "stretch 0xffffffff", 0, 0, 1, OK_W, OK_S, 0xffffffffu },
    };
    if (fb) { run_bad_rows(rows, 5, fb); DW_RELEASE(fb); }
    R2_END();
}

RT(map_weight_style_stretch_in_range_accepted)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const UINT32 w[] = { 1, 2, 100, 350, 400, 500, 650, 900, 950, 998, 999 };
    unsigned i;
    for (i = 0; fb && i < sizeof w / sizeof w[0]; i++) {
        r2_args a; r2_res r; r2_defaults(&a, &X); a.w = (DWRITE_FONT_WEIGHT)w[i];
        CHECKF(r2_map(fb, L"abc", 3, &a, &r, NULL) == S_OK, "weight %u must be accepted", w[i]);
        DW_RELEASE(r.font);
    }
    for (i = 0; fb && i <= 2; i++) {
        r2_args a; r2_res r; r2_defaults(&a, &X); a.st = (DWRITE_FONT_STYLE)i;
        CHECKF(r2_map(fb, L"abc", 3, &a, &r, NULL) == S_OK, "style %u must be accepted", i);
        DW_RELEASE(r.font);
    }
    for (i = 1; fb && i <= 9; i++) {
        r2_args a; r2_res r; r2_defaults(&a, &X); a.sx = (DWRITE_FONT_STRETCH)i;
        CHECKF(r2_map(fb, L"abc", 3, &a, &r, NULL) == S_OK, "stretch %u must be accepted", i);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(map_unmapped_text_scale_is_one_and_font_null)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r;
    r2_defaults(&a, &X);
    if (fb) {
        /* U+0905 is covered by neither the base family nor any mapping of the standard fallback */
        CHECK_HR_OK(r2_map(fb, L"\x0905", 1, &a, &r, NULL));
        CHECK(r.font == NULL); EXPECT_EQ(r.ml, 1); CHECK_FLOAT_EQ(r.scale, 1.0f);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(map_base_covered_text_scale_is_one)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x41, 0x7a, R2_ALT, X.coll, NULL, NULL, 3.0f));      /* never consulted: the base family covers ASCII */
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, NULL));
        EXPECT_EQ(r.ml, 3); CHECK(r.font != NULL); CHECK_FLOAT_EQ(r.scale, 1.0f); EXPECT_STR(r.fi.fam, "R2Main");
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(map_zero_length_range)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r;
    r2_defaults(&a, &X); a.len = 0;
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, NULL));
        t_note("zero length range: mappedLength %u font %s scale %g", r.ml, r.font ? r.fi.fam : "NULL", r.scale);
        DW_RELEASE(r.font);
        a.pos = 3;                                          /* at the end of the text */
        CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, NULL));
        t_note("zero length range at the end: mappedLength %u font %s", r.ml, r.font ? r.fi.fam : "NULL");
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(map_range_inside_text_does_not_touch_prefix)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r; dwx_ana_src *s = NULL;
    r2_defaults(&a, &X); a.pos = 2; a.len = 2;
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3042\x3044" L"ab", 4, &a, &r, &s));
        EXPECT_EQ(r.ml, 2); EXPECT_STR(r.fi.fam, "R2Main");     /* text at positions 0 and 1 is outside the requested range */
        DW_RELEASE(r.font); dwx_release(s);
    }
    DW_RELEASE(fb); R2_END();
}

/* ---- IDWriteFontFallback1 ----------------------------------------------------------------------------------------------------------------- */
static IDWriteFontFallback1 *to_v1(IDWriteFontFallback *fb)
{
    IDWriteFontFallback1 *p = NULL;
    if (!fb || FAILED(IDWriteFontFallback_QueryInterface(fb, &IID_IDWriteFontFallback1, (void **)&p))) return NULL;
    return p;
}

typedef struct { const char *what; int null_src, null_axes; UINT32 pos, len, nvals; } bad1;

RT(map1_invalid_args_reset_outs)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); IDWriteFontFallback1 *fb1 = to_v1(fb);
    static const bad1 rows[] = {
        { "NULL source", 1, 0, 0, 1, 0 }, { "NULL source with axes", 1, 0, 0, 1, 1 }, { "pos 0xffffffff len 1", 0, 0, 0xffffffff, 1, 0 },
        { "pos 1 len 0xffffffff", 0, 0, 1, 0xffffffff, 0 }, { "pos 0x80000000 len 0x80000000", 0, 0, 0x80000000u, 0x80000000u, 0 },
        { "axis count 0x10000", 0, 0, 0, 1, 0x10000 }, { "axis count 0xffffffff", 0, 0, 0, 1, 0xffffffffu }, { "axis count 1, NULL axes", 0, 1, 0, 1, 1 },
        { "axis count 0xffff, NULL axes", 0, 1, 0, 1, 0xffff }, { "axis count 100, NULL axes", 0, 1, 0, 1, 100 },
    };
    DWRITE_FONT_AXIS_VALUE ax[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
    int i;
    if (!fb1) { DW_RELEASE(fb); R2_END(); t_skip("IDWriteFontFallback1 unavailable"); return; }
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        dwx_ana_src *src = dwx_ana_src_new(L"abc", 3); UINT32 ml = 0xABCD; IDWriteFontFace5 *face = (IDWriteFontFace5 *)(ULONG_PTR)1; float sc = 7.0f; HRESULT hr;
        hr = IDWriteFontFallback1_MapCharacters(fb1, rows[i].null_src ? NULL : (IDWriteTextAnalysisSource *)src, rows[i].pos, rows[i].len, X.coll, R2_MAIN,
                                                rows[i].null_axes ? NULL : ax, rows[i].nvals, &ml, &sc, &face);
        t_check(hr == E_INVALIDARG, __FILE__, __LINE__, rows[i].what, "hr %s", dw_hr_name(hr));
        t_check(ml == 0 && face == NULL && sc == 1.0f, __FILE__, __LINE__, rows[i].what, "outs not reset: ml %u face %p scale %g", ml, (void *)face, sc);
        dwx_release(src);
    }
    DW_RELEASE(fb1); DW_RELEASE(fb); R2_END();
}

RT(map1_zero_axes_with_null_pointer_ok)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); IDWriteFontFallback1 *fb1 = to_v1(fb);
    dwx_ana_src *src = dwx_ana_src_new(L"abc", 3); UINT32 ml = 0; IDWriteFontFace5 *face = NULL; float sc = 0;
    if (!fb1) { dwx_release(src); DW_RELEASE(fb); R2_END(); t_skip("IDWriteFontFallback1 unavailable"); return; }
    CHECK_HR_OK(IDWriteFontFallback1_MapCharacters(fb1, (IDWriteTextAnalysisSource *)src, 0, 3, X.coll, R2_MAIN, NULL, 0, &ml, &sc, &face));
    EXPECT_EQ(ml, 3); CHECK(face != NULL); CHECK_FLOAT_EQ(sc, 1.0f);
    DW_RELEASE(face); dwx_release(src); DW_RELEASE(fb1); DW_RELEASE(fb); R2_END();
}

RT(map1_valid_axis_array_accepted)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); IDWriteFontFallback1 *fb1 = to_v1(fb);
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 100.0f } };
    dwx_ana_src *src = dwx_ana_src_new(L"abc", 3); UINT32 ml = 0; IDWriteFontFace5 *face = NULL; float sc = 0;
    if (!fb1) { dwx_release(src); DW_RELEASE(fb); R2_END(); t_skip("IDWriteFontFallback1 unavailable"); return; }
    CHECK_HR_OK(IDWriteFontFallback1_MapCharacters(fb1, (IDWriteTextAnalysisSource *)src, 0, 3, X.coll, R2_MAIN, ax, 2, &ml, &sc, &face));
    EXPECT_EQ(ml, 3); CHECK(face != NULL);
    DW_RELEASE(face); dwx_release(src); DW_RELEASE(fb1); DW_RELEASE(fb); R2_END();
}

RT(map1_zero_length_range)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); IDWriteFontFallback1 *fb1 = to_v1(fb);
    dwx_ana_src *src = dwx_ana_src_new(L"abc", 3); UINT32 ml = 99; IDWriteFontFace5 *face = NULL; float sc = 0;
    if (!fb1) { dwx_release(src); DW_RELEASE(fb); R2_END(); t_skip("IDWriteFontFallback1 unavailable"); return; }
    CHECK_HR_OK(IDWriteFontFallback1_MapCharacters(fb1, (IDWriteTextAnalysisSource *)src, 3, 0, X.coll, R2_MAIN, NULL, 0, &ml, &sc, &face));
    t_note("v1 zero length range: mappedLength %u face %s", ml, face ? "set" : "NULL");
    DW_RELEASE(face); dwx_release(src); DW_RELEASE(fb1); DW_RELEASE(fb); R2_END();
}

/* ---- AddMapping ------------------------------------------------------------------------------------------------------------------------------ */
typedef struct {
    const char *what; int null_ranges; UINT32 nranges; int null_names; UINT32 nnames; int null_at; /* index of a NULL entry in names, -1 none */
    float scale; HRESULT want;
} am_row;

RT(add_mapping_argument_table)
{
    R2_BEGIN(); R2_NEED_B(b);
    static const am_row rows[] = {
        { "ranges NULL", 1, 1, 0, 1, -1, 1.0f, E_INVALIDARG }, { "rangesCount 0", 0, 0, 0, 1, -1, 1.0f, E_INVALIDARG },
        { "names NULL", 0, 1, 1, 1, -1, 1.0f, E_INVALIDARG }, { "namesCount 0", 0, 1, 0, 0, -1, 1.0f, E_INVALIDARG },
        { "scale -1", 0, 1, 0, 1, -1, -1.0f, E_INVALIDARG }, { "scale -FLT_MIN", 0, 1, 0, 1, -1, -FLT_MIN, E_INVALIDARG },
        { "scale -FLT_MAX", 0, 1, 0, 1, -1, -FLT_MAX, E_INVALIDARG }, { "scale -INF", 0, 1, 0, 1, -1, -INFINITY, E_INVALIDARG },
        { "NULL name at 0 of 1", 0, 1, 0, 1, 0, 1.0f, E_INVALIDARG }, { "NULL name at 0 of 3", 0, 1, 0, 3, 0, 1.0f, E_INVALIDARG },
        { "NULL name at 1 of 3", 0, 1, 0, 3, 1, 1.0f, E_INVALIDARG }, { "NULL name at 2 of 3", 0, 1, 0, 3, 2, 1.0f, E_INVALIDARG },
        { "valid", 0, 1, 0, 1, -1, 1.0f, S_OK }, { "scale 0", 0, 1, 0, 1, -1, 0.0f, S_OK }, { "scale -0.0", 0, 1, 0, 1, -1, -0.0f, S_OK },
        { "scale FLT_MIN", 0, 1, 0, 1, -1, FLT_MIN, S_OK }, { "scale 0.5", 0, 1, 0, 1, -1, 0.5f, S_OK }, { "scale 2", 0, 1, 0, 1, -1, 2.0f, S_OK },
        { "scale 1000", 0, 1, 0, 1, -1, 1000.0f, S_OK }, { "scale FLT_MAX", 0, 1, 0, 1, -1, FLT_MAX, S_OK }, { "scale +INF", 0, 1, 0, 1, -1, INFINITY, S_OK },
        { "3 names", 0, 1, 0, 3, -1, 1.0f, S_OK }, { "4 ranges", 0, 4, 0, 1, -1, 1.0f, S_OK },
    };
    DWRITE_UNICODE_RANGE rg[4] = { { 0x3040, 0x30ff }, { 0x4e00, 0x4e0f }, { 0x2600, 0x2600 }, { 0x1f600, 0x1f600 } };
    const WCHAR *names[3] = { R2_CJK, R2_CJK2, R2_SYM };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        const WCHAR *nm[3]; HRESULT hr; unsigned k;
        for (k = 0; k < 3; k++) nm[k] = names[k];
        if (rows[i].null_at >= 0) nm[rows[i].null_at] = NULL;
        hr = IDWriteFontFallbackBuilder_AddMapping(b, rows[i].null_ranges ? NULL : rg, rows[i].nranges, rows[i].null_names ? NULL : nm, rows[i].nnames, X.coll, NULL, NULL, rows[i].scale);
        t_check(hr == rows[i].want, __FILE__, __LINE__, rows[i].what, "hr %s want %s", dw_hr_name(hr), dw_hr_name(rows[i].want));
    }
    DW_RELEASE(b); R2_END();
}

RT(add_mapping_null_name_beyond_count_is_not_read)
{
    R2_BEGIN(); R2_NEED_B(b);
    const WCHAR *nm[3] = { R2_CJK, NULL, NULL }; DWRITE_UNICODE_RANGE rg = { 0x3040, 0x30ff };
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, nm, 1, X.coll, NULL, NULL, 1.0f), S_OK);      /* only the first targetFamilyNamesCount entries count */
    DW_RELEASE(b); R2_END();
}

RT(add_mapping_empty_strings_accepted)
{
    R2_BEGIN(); R2_NEED_B(b);
    CHECK_HR(r2_add(b, 0x3040, 0x30ff, L"", X.coll, L"", L"", 1.0f), S_OK);                   /* only NULL is rejected */
    CHECK_HR(r2_add(b, 0x3040, 0x30ff, R2_CJK, NULL, L"", NULL, 1.0f), S_OK);
    DW_RELEASE(b); R2_END();
}

RT(add_mapping_odd_ranges_accepted)
{
    R2_BEGIN(); R2_NEED_B(b);
    static const DWRITE_UNICODE_RANGE odd[] = { { 0x3042, 0x3041 }, { 0, 0 }, { 0, 0x10ffff }, { 0x10ffff, 0x10ffff }, { 0x110000, 0x110000 }, { 0xffffffff, 0xffffffff }, { 0, 0xffffffff }, { 0xd800, 0xdfff } };
    unsigned i;
    for (i = 0; i < sizeof odd / sizeof odd[0]; i++) {
        const WCHAR *nm[1] = { R2_CJK };
        t_check(IDWriteFontFallbackBuilder_AddMapping(b, &odd[i], 1, nm, 1, X.coll, NULL, NULL, 1.0f) == S_OK, __FILE__, __LINE__, "odd range accepted", "range %u: [%x,%x]", i, odd[i].first, odd[i].last);
    }
    DW_RELEASE(b); R2_END();
}

RT(add_mapping_invalid_adds_nothing)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; r2_args a; r2_res r;
    DWRITE_UNICODE_RANGE rg = { 0x3040, 0x30ff }; const WCHAR *nm[2] = { R2_CJK, NULL };
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, nm, 2, X.coll, NULL, NULL, 1.0f), E_INVALIDARG);      /* NULL second name */
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, nm, 1, X.coll, NULL, NULL, -1.0f), E_INVALIDARG);
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        CHECK(r.font == NULL);                              /* a rejected call must not leave a half-added mapping behind */
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(add_mappings_null_is_invalidarg)
{
    R2_BEGIN(); R2_NEED_B(b);
    if (!t_is_wine()) CHECK_HR(IDWriteFontFallbackBuilder_AddMappings(b, NULL), E_INVALIDARG);
    DW_RELEASE(b); R2_END();
}

RT(create_fallback_resets_out_and_succeeds)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb = (IDWriteFontFallback *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteFontFallbackBuilder_CreateFontFallback(b, &fb));
    CHECK(fb != NULL && fb != (IDWriteFontFallback *)(ULONG_PTR)1);
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}
