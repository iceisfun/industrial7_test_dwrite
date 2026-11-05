/* Per-range character formatting on a layout: SetFontWeight/Style/Stretch/Size/FamilyName/LocaleName/Underline/Strikethrough and the
 * matching positional getters.
 *
 * Model established by the implementation: a layout keeps one run list that covers [0, 0xFFFFFFFF) regardless of the text length, so
 *   - an untouched layout reports the range {0, 0xFFFFFFFF} for every position (also positions beyond the text);
 *   - setters validate only start+length for unsigned wrap-around (E_INVALIDARG) and the value itself; ranges beyond the text are
 *     stored as given and later getters see them; a zero length range is a successful no-op;
 *   - adjacent runs that end up with the same value are merged, so a getter reports the maximal range sharing the value;
 *   - the layout-wide getters (the IDWriteTextFormat part of the interface) report the creation-time defaults and are not touched by range setters. */
#include "tl.h"

typedef struct tl_prop {
    const char *name; int nvals;
    HRESULT (*set)(IDWriteTextLayout *, int, DWRITE_TEXT_RANGE);
    int (*get)(IDWriteTextLayout *, UINT32, DWRITE_TEXT_RANGE *);       /* value index, -1 unknown value, -2 call failed */
} tl_prop;

#define SCALAR_PROP(nm, T, SETF, GETF, ...) \
    static const T nm##_vals[] = { __VA_ARGS__ }; \
    static HRESULT nm##_set(IDWriteTextLayout *l, int i, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_##SETF(l, nm##_vals[i], r); } \
    static int nm##_get(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r) { \
        T v; unsigned i; if (IDWriteTextLayout_##GETF(l, pos, &v, r) != S_OK) return -2; \
        for (i = 0; i < sizeof nm##_vals / sizeof nm##_vals[0]; i++) if (nm##_vals[i] == v) return (int)i; return -1; }

SCALAR_PROP(weight,  DWRITE_FONT_WEIGHT,  SetFontWeight,  GetFontWeight,  DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_WEIGHT_THIN, DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_WEIGHT_SEMI_LIGHT)
SCALAR_PROP(style,   DWRITE_FONT_STYLE,   SetFontStyle,   GetFontStyle,   DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STYLE_OBLIQUE)
SCALAR_PROP(stretch, DWRITE_FONT_STRETCH, SetFontStretch, GetFontStretch, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STRETCH_ULTRA_CONDENSED, DWRITE_FONT_STRETCH_ULTRA_EXPANDED, DWRITE_FONT_STRETCH_CONDENSED)
SCALAR_PROP(fsize,   FLOAT,               SetFontSize,    GetFontSize,    16.0f, 24.0f, 8.5f, 2048.0f, 0.5f)

static const BOOL line_vals[] = { FALSE, TRUE };
static HRESULT ul_set(IDWriteTextLayout *l, int i, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetUnderline(l, line_vals[i], r); }
static int ul_get(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r) { BOOL v = 0xcc; if (IDWriteTextLayout_GetUnderline(l, pos, &v, r) != S_OK) return -2; return v ? 1 : 0; }
static HRESULT st_set(IDWriteTextLayout *l, int i, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetStrikethrough(l, line_vals[i], r); }
static int st_get(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r) { BOOL v = 0xcc; if (IDWriteTextLayout_GetStrikethrough(l, pos, &v, r) != S_OK) return -2; return v ? 1 : 0; }

static const WCHAR *const fam_vals[] = { TL_FAM, TL_NARROW, L"Other Family", L"X" };
static HRESULT fam_set(IDWriteTextLayout *l, int i, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontFamilyName(l, fam_vals[i], r); }
static int fam_get(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r)
{
    WCHAR buf[64]; unsigned i;
    if (IDWriteTextLayout_GetFontFamilyName(l, pos, buf, 64, r) != S_OK) return -2;
    for (i = 0; i < 4; i++) if (!wcscmp(buf, fam_vals[i])) return (int)i;
    return -1;
}
static const WCHAR *const loc_vals[] = { L"en-us", L"fr-fr", L"de-de", L"ja-jp" };
static HRESULT loc_set(IDWriteTextLayout *l, int i, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetLocaleName(l, loc_vals[i], r); }
static int loc_get(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r)
{
    WCHAR buf[64]; unsigned i;
    if (IDWriteTextLayout_GetLocaleName(l, pos, buf, 64, r) != S_OK) return -2;
    for (i = 0; i < 4; i++) if (!_wcsicmp(buf, loc_vals[i])) return (int)i;
    return -1;
}

static const tl_prop PROPS[] = {
    { "weight", 5, weight_set, weight_get }, { "style", 3, style_set, style_get }, { "stretch", 4, stretch_set, stretch_get },
    { "size", 5, fsize_set, fsize_get }, { "underline", 2, ul_set, ul_get }, { "strikethrough", 2, st_set, st_get },
    { "family", 4, fam_set, fam_get }, { "locale", 4, loc_set, loc_get },
};
#define NPROPS ((int)(sizeof PROPS / sizeof PROPS[0]))
#define MAXR 0xFFFFFFFFu

#define CHK_RANGE(p, r, s, n) CHECKF((r).startPosition == (s) && (r).length == (n), "%s: range {%u,%u} want {%u,%u}", (p)->name, (r).startPosition, (r).length, (UINT32)(s), (UINT32)(n))
#define EACH_PROP_BEGIN for (int i_ = 0; i_ < NPROPS; i_++) { const tl_prop *p = &PROPS[i_]; DWRITE_TEXT_RANGE r; \
    IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, L"ABCDEFGH", 16.0f, 1000.0f, 1000.0f); if (!l) { t_check(0, __FILE__, __LINE__, "layout", "creation failed"); continue; }
#define EACH_PROP_END DW_RELEASE(l); }
#define NEED3 if (p->nvals < 3) { DW_RELEASE(l); continue; }
#define LAYOUT8(var) TL_LAYOUT(var, L"ABCDEFGH", 16.0f, 1000.0f, 1000.0f)

TEST(prop_default_values)
{
    TL_BEGIN(); LAYOUT8(l);
    int i;
    for (i = 0; i < NPROPS; i++) {
        DWRITE_TEXT_RANGE r = { 7, 7 };
        CHECKF(PROPS[i].get(l, 0, &r) == 0, "%s: default is value 0", PROPS[i].name);
    }
    DW_RELEASE(l); TL_END();
}

TEST(prop_default_range_unbounded)
{
    TL_BEGIN(); LAYOUT8(l);
    static const UINT32 pos[] = { 0, 1, 7, 8, 9, 100, 4096, 100000 };
    int i; size_t k;
    for (i = 0; i < NPROPS; i++)
        for (k = 0; k < sizeof pos / sizeof pos[0]; k++) {
            DWRITE_TEXT_RANGE r = { 7, 7 };
            int v = PROPS[i].get(l, pos[k], &r);
            CHECKF(v == 0, "%s@%u value %d", PROPS[i].name, pos[k], v);
            CHK_RANGE(&PROPS[i], r, 0, MAXR);
        }
    DW_RELEASE(l); TL_END();
}

TEST(prop_default_range_empty_text)
{
    TL_BEGIN(); TL_LAYOUT(l, L"", 16.0f, 100.0f, 100.0f);
    int i;
    for (i = 0; i < NPROPS; i++) {
        DWRITE_TEXT_RANGE r = { 7, 7 };
        CHECKF(PROPS[i].get(l, 0, &r) == 0, "%s", PROPS[i].name);
        CHK_RANGE(&PROPS[i], r, 0, MAXR);
    }
    DW_RELEASE(l); TL_END();
}

TEST(prop_set_middle_range_splits_runs)
{
    TL_BEGIN(); 
    EACH_PROP_BEGIN
    CHECK_HR_OK(p->set(l, 1, RNG(2, 3)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 0, &r) == 0, "%s@0", p->name); CHK_RANGE(p, r, 0, 2);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 1, &r) == 0, "%s@1", p->name); CHK_RANGE(p, r, 0, 2);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 2, &r) == 1, "%s@2", p->name); CHK_RANGE(p, r, 2, 3);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 3, &r) == 1, "%s@3", p->name); CHK_RANGE(p, r, 2, 3);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 4, &r) == 1, "%s@4", p->name); CHK_RANGE(p, r, 2, 3);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 5, &r) == 0, "%s@5", p->name); CHK_RANGE(p, r, 5, MAXR - 5);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 7, &r) == 0, "%s@7", p->name); CHK_RANGE(p, r, 5, MAXR - 5);
    CHECK_HR_OK(p->set(l, 0, RNG(2, 3)));      /* restore */
    EACH_PROP_END
    TL_END();
}

TEST(prop_set_prefix_range)
{
    TL_BEGIN(); 
    EACH_PROP_BEGIN
    CHECK_HR_OK(p->set(l, 1, RNG(0, 4)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 0, &r) == 1, "%s@0", p->name); CHK_RANGE(p, r, 0, 4);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 3, &r) == 1, "%s@3", p->name); CHK_RANGE(p, r, 0, 4);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 4, &r) == 0, "%s@4", p->name); CHK_RANGE(p, r, 4, MAXR - 4);
    CHECK_HR_OK(p->set(l, 0, RNG(0, 4)));
    EACH_PROP_END
    TL_END();
}

TEST(prop_set_whole_text)
{
    TL_BEGIN(); 
    EACH_PROP_BEGIN
    NEED3
    CHECK_HR_OK(p->set(l, 2, RNG(0, 8)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 0, &r) == 2, "%s@0", p->name); CHK_RANGE(p, r, 0, 8);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 7, &r) == 2, "%s@7", p->name); CHK_RANGE(p, r, 0, 8);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 8, &r) == 0, "%s@8", p->name); CHK_RANGE(p, r, 8, MAXR - 8);
    CHECK_HR_OK(p->set(l, 0, RNG(0, 8)));
    EACH_PROP_END
    TL_END();
}

TEST(prop_set_everything_max_range)
{
    TL_BEGIN(); 
    EACH_PROP_BEGIN
    CHECK_HR_OK(p->set(l, 1, RNG(0, MAXR)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 0, &r) == 1, "%s@0", p->name); CHK_RANGE(p, r, 0, MAXR);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 5000, &r) == 1, "%s@5000", p->name); CHK_RANGE(p, r, 0, MAXR);
    CHECK_HR_OK(p->set(l, 0, RNG(0, MAXR)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 0, &r) == 0, "%s restored", p->name); CHK_RANGE(p, r, 0, MAXR);
    EACH_PROP_END
    TL_END();
}

TEST(prop_set_zero_length_is_noop)
{
    TL_BEGIN(); LAYOUT8(l);
    static const UINT32 starts[] = { 0, 3, 8, 100, 0xFFFFFFFFu };
    int i; size_t k;
    for (i = 0; i < NPROPS; i++)
        for (k = 0; k < sizeof starts / sizeof starts[0]; k++) {
            const tl_prop *p = &PROPS[i]; DWRITE_TEXT_RANGE r = { 7, 7 };
            HRESULT hr = p->set(l, 1, RNG(starts[k], 0));
            CHECKF(hr == S_OK, "%s start %u -> %s", p->name, starts[k], dw_hr_name(hr));
            CHECKF(p->get(l, 3, &r) == 0, "%s still default", p->name); CHK_RANGE(p, r, 0, MAXR);
        }
    DW_RELEASE(l); TL_END();
}

TEST(prop_set_overlapping_ranges)
{
    TL_BEGIN(); 
    EACH_PROP_BEGIN
    NEED3
    CHECK_HR_OK(p->set(l, 1, RNG(1, 4)));        /* [1,5) */
    CHECK_HR_OK(p->set(l, 2, RNG(3, 4)));        /* [3,7) overwrites the tail of the first */
    r.startPosition = r.length = 77; CHECKF(p->get(l, 0, &r) == 0, "%s@0", p->name); CHK_RANGE(p, r, 0, 1);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 1, &r) == 1, "%s@1", p->name); CHK_RANGE(p, r, 1, 2);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 2, &r) == 1, "%s@2", p->name); CHK_RANGE(p, r, 1, 2);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 3, &r) == 2, "%s@3", p->name); CHK_RANGE(p, r, 3, 4);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 6, &r) == 2, "%s@6", p->name); CHK_RANGE(p, r, 3, 4);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 7, &r) == 0, "%s@7", p->name); CHK_RANGE(p, r, 7, MAXR - 7);
    CHECK_HR_OK(p->set(l, 0, RNG(0, 8)));
    EACH_PROP_END
    TL_END();
}

TEST(prop_set_inside_existing_run)
{
    /* a nested range splits the surrounding run in three */
    TL_BEGIN();
    EACH_PROP_BEGIN
    NEED3
    CHECK_HR_OK(p->set(l, 1, RNG(0, 8)));
    CHECK_HR_OK(p->set(l, 2, RNG(3, 2)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 0, &r) == 1, "%s@0", p->name); CHK_RANGE(p, r, 0, 3);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 4, &r) == 2, "%s@4", p->name); CHK_RANGE(p, r, 3, 2);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 5, &r) == 1, "%s@5", p->name); CHK_RANGE(p, r, 5, 3);
    EACH_PROP_END
    TL_END();
}

TEST(prop_equal_neighbours_merge)
{
    /* two adjacent ranges given the same value form one reported range */
    TL_BEGIN();
    EACH_PROP_BEGIN
    CHECK_HR_OK(p->set(l, 1, RNG(1, 2)));
    CHECK_HR_OK(p->set(l, 1, RNG(3, 2)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 1, &r) == 1, "%s@1", p->name); CHK_RANGE(p, r, 1, 4);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 4, &r) == 1, "%s@4", p->name); CHK_RANGE(p, r, 1, 4);
    CHECK_HR_OK(p->set(l, 0, RNG(1, 4)));
    EACH_PROP_END
    TL_END();
}

TEST(prop_restoring_default_merges_back)
{
    TL_BEGIN(); 
    EACH_PROP_BEGIN
    CHECK_HR_OK(p->set(l, 1, RNG(2, 3)));
    CHECK_HR_OK(p->set(l, 0, RNG(2, 3)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 2, &r) == 0, "%s@2", p->name); CHK_RANGE(p, r, 0, MAXR);
    EACH_PROP_END
    TL_END();
}

TEST(prop_alternating_characters_do_not_merge)
{
    TL_BEGIN();
    EACH_PROP_BEGIN
    int c;
    NEED3
    for (c = 0; c < 8; c++) CHECK_HR_OK(p->set(l, 1 + (c & 1), RNG(c, 1)));
    for (c = 0; c < 8; c++) {
        r.startPosition = r.length = 77;
        CHECKF(p->get(l, c, &r) == 1 + (c & 1), "%s@%d", p->name, c); CHK_RANGE(p, r, c, 1);
    }
    EACH_PROP_END
    TL_END();
}

TEST(prop_set_beyond_text_length)
{
    /* the text is 8 characters; ranges outside [0,8) are accepted and visible through the getters */
    TL_BEGIN();
    EACH_PROP_BEGIN
    CHECK_HR_OK(p->set(l, 1, RNG(100, 4)));
    r.startPosition = r.length = 77; CHECKF(p->get(l, 100, &r) == 1, "%s@100", p->name); CHK_RANGE(p, r, 100, 4);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 99, &r) == 0, "%s@99", p->name); CHK_RANGE(p, r, 0, 100);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 104, &r) == 0, "%s@104", p->name); CHK_RANGE(p, r, 104, MAXR - 104);
    CHECK_HR_OK(p->set(l, 0, RNG(100, 4)));
    EACH_PROP_END
    TL_END();
}

TEST(prop_set_range_straddling_text_end)
{
    TL_BEGIN(); 
    EACH_PROP_BEGIN
    CHECK_HR_OK(p->set(l, 1, RNG(6, 10)));       /* [6,16): two characters inside, eight beyond */
    r.startPosition = r.length = 77; CHECKF(p->get(l, 7, &r) == 1, "%s@7", p->name); CHK_RANGE(p, r, 6, 10);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 15, &r) == 1, "%s@15", p->name); CHK_RANGE(p, r, 6, 10);
    r.startPosition = r.length = 77; CHECKF(p->get(l, 16, &r) == 0, "%s@16", p->name); CHK_RANGE(p, r, 16, MAXR - 16);
    CHECK_HR_OK(p->set(l, 0, RNG(6, 10)));
    EACH_PROP_END
    TL_END();
}

TEST(prop_range_overflow_rejected)
{
    TL_BEGIN(); LAYOUT8(l);
    static const DWRITE_TEXT_RANGE bad[] = {
        { 1, 0xFFFFFFFFu }, { 10, 0xFFFFFFFFu }, { 10, 0xFFFFFFFFu - 9 }, { 0xFFFFFFFFu, 1 }, { 0x80000000u, 0x80000000u },
        { 0xFFFFFFFFu, 0xFFFFFFFFu }, { 2, 0xFFFFFFFEu }, { 0xFFFFFFF0u, 0x20 },
    };
    int i; size_t k;
    for (i = 0; i < NPROPS; i++)
        for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
            const tl_prop *p = &PROPS[i]; DWRITE_TEXT_RANGE r = { 7, 7 };
            HRESULT hr = p->set(l, 1, bad[k]);
            CHECKF(hr == E_INVALIDARG, "%s {%u,%u} -> %s", p->name, bad[k].startPosition, bad[k].length, dw_hr_name(hr));
            CHECKF(p->get(l, 0, &r) == 0, "%s unchanged after failure", p->name); CHK_RANGE(p, r, 0, MAXR);
        }
    DW_RELEASE(l); TL_END();
}

TEST(prop_range_just_fitting_accepted)
{
    /* start + length == 2^32 - 1 does not wrap: accepted */
    TL_BEGIN(); LAYOUT8(l);
    static const DWRITE_TEXT_RANGE ok[] = { { 0, 0xFFFFFFFFu }, { 10, 0xFFFFFFFFu - 10 }, { 0xFFFFFFFEu, 1 }, { 0x7FFFFFFFu, 0x80000000u }, { 5, 0xFFFFFFFFu - 5 } };
    int i; size_t k;
    for (i = 0; i < NPROPS; i++)
        for (k = 0; k < sizeof ok / sizeof ok[0]; k++) {
            const tl_prop *p = &PROPS[i];
            HRESULT hr = p->set(l, 1, ok[k]);
            CHECKF(hr == S_OK, "%s {%u,%u} -> %s", p->name, ok[k].startPosition, ok[k].length, dw_hr_name(hr));
            p->set(l, 0, RNG(0, MAXR));
        }
    DW_RELEASE(l); TL_END();
}

TEST(prop_failed_set_changes_nothing_in_metrics)
{
    TL_BEGIN(); TL_LAYOUT(l, L"ABCD", TL_UPM, 100000.0f, 1000.0f);
    DWRITE_TEXT_METRICS a, b;
    tl_metrics(l, &a);
    IDWriteTextLayout_SetFontSize(l, 64.0f, RNG(1, 0xFFFFFFFFu));
    IDWriteTextLayout_SetFontSize(l, -1.0f, RNG(0, 4));
    IDWriteTextLayout_SetFontFamilyName(l, TL_NARROW, RNG(0xFFFFFFFFu, 2));
    tl_metrics(l, &b);
    CHECK_FLOAT_EQ(a.width, b.width); CHECK_FLOAT_EQ(a.height, b.height);
    DW_RELEASE(l); TL_END();
}

TEST(prop_setters_dont_touch_layout_wide_getters)
{
    /* SetFont*(range) rewrites runs only: the creation-time defaults reported by the IDWriteTextFormat part stay */
    TL_BEGIN(); LAYOUT8(l);
    WCHAR buf[64]; UINT32 n;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 99.0f, RNG(0, 8)));
    CHECK_HR_OK(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BLACK, RNG(0, 8)));
    CHECK_HR_OK(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, RNG(0, 8)));
    CHECK_HR_OK(IDWriteTextLayout_SetFontStretch(l, DWRITE_FONT_STRETCH_CONDENSED, RNG(0, 8)));
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, TL_NARROW, RNG(0, 8)));
    CHECK_HR_OK(IDWriteTextLayout_SetLocaleName(l, L"de-de", RNG(0, 8)));
    CHECK_FLOAT_EQ(l->lpVtbl->GetFontSize(l), 16.0f);
    EXPECT_EQ(l->lpVtbl->GetFontWeight(l), DWRITE_FONT_WEIGHT_NORMAL);
    EXPECT_EQ(l->lpVtbl->GetFontStyle(l), DWRITE_FONT_STYLE_NORMAL);
    EXPECT_EQ(l->lpVtbl->GetFontStretch(l), DWRITE_FONT_STRETCH_NORMAL);
    n = l->lpVtbl->GetFontFamilyNameLength(l); EXPECT_EQ(n, wcslen(TL_FAM));
    CHECK_HR_OK(l->lpVtbl->GetFontFamilyName(l, buf, 64)); CHECK_WEQ(buf, TL_FAM);
    CHECK_HR_OK(l->lpVtbl->GetLocaleName(l, buf, 64)); CHECK(!_wcsicmp(buf, L"en-us"));
    EXPECT_EQ(l->lpVtbl->GetLocaleNameLength(l), 5);
    DW_RELEASE(l); TL_END();
}

TEST(prop_positional_matches_layout_wide_at_creation)
{
    TL_BEGIN();
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l;
    DWRITE_TEXT_RANGE r; float sz; DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE s; DWRITE_FONT_STRETCH st; WCHAR b[64], b2[64]; UINT32 n;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, TL_NARROW, X.coll, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STRETCH_EXPANDED, 33.5f, L"fr-FR", &t));
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCDEF", 6, t, 100.0f, 100.0f, &l));
    DW_RELEASE(t);
    if (!l) { TL_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 3, &sz, &r)); CHECK_FLOAT_EQ(sz, 33.5f); CHECK_FLOAT_EQ(l->lpVtbl->GetFontSize(l), 33.5f);
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 3, &w, &r)); EXPECT_EQ(w, DWRITE_FONT_WEIGHT_SEMI_BOLD); EXPECT_EQ(l->lpVtbl->GetFontWeight(l), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    CHECK_HR_OK(IDWriteTextLayout_GetFontStyle(l, 3, &s, &r)); EXPECT_EQ(s, DWRITE_FONT_STYLE_OBLIQUE); EXPECT_EQ(l->lpVtbl->GetFontStyle(l), DWRITE_FONT_STYLE_OBLIQUE);
    CHECK_HR_OK(IDWriteTextLayout_GetFontStretch(l, 3, &st, &r)); EXPECT_EQ(st, DWRITE_FONT_STRETCH_EXPANDED); EXPECT_EQ(l->lpVtbl->GetFontStretch(l), DWRITE_FONT_STRETCH_EXPANDED);
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyNameLength(l, 3, &n, &r)); EXPECT_EQ(n, wcslen(TL_NARROW));
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 3, b, 64, &r)); CHECK_WEQ(b, TL_NARROW);
    CHECK_HR_OK(l->lpVtbl->GetFontFamilyName(l, b2, 64)); CHECK_WEQ(b2, TL_NARROW);
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleNameLength(l, 3, &n, &r)); EXPECT_EQ(n, 5);
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 3, b, 64, &r)); CHECK(!_wcsicmp(b, L"fr-FR"));
    CHECK_HR_OK(l->lpVtbl->GetLocaleName(l, b2, 64)); CHECK(!wcscmp(b, b2));
    DW_RELEASE(l); TL_END();
}

/* ---- invalid values ---------------------------------------------------------------------------------------------- */
TEST(prop_weight_invalid_values)
{
    TL_BEGIN(); LAYOUT8(l);
    static const UINT32 bad[] = { 0, 1000, 1001, 4096, 65535, 0x7fffffff, 0x80000000u, 0xffffffffu };
    static const UINT32 good[] = { 1, 100, 350, 400, 999 };
    size_t k; DWRITE_FONT_WEIGHT w; DWRITE_TEXT_RANGE r;
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)bad[k], RNG(0, 4));
        CHECKF(hr == E_INVALIDARG, "weight %u -> %s", bad[k], dw_hr_name(hr));
    }
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 0, &w, &r)); EXPECT_EQ(w, DWRITE_FONT_WEIGHT_NORMAL);
    for (k = 0; k < sizeof good / sizeof good[0]; k++) {
        CHECK_HR_OK(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)good[k], RNG(0, 4)));
        CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 0, &w, &r)); EXPECT_EQ(w, good[k]);
    }
    /* invalid value with a zero-length range is still rejected: the value is checked first */
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)0, RNG(0, 0)), E_INVALIDARG);
    DW_RELEASE(l); TL_END();
}

TEST(prop_style_invalid_values)
{
    TL_BEGIN(); LAYOUT8(l);
    static const UINT32 bad[] = { 3, 4, 100, 0x7fffffff, 0xffffffffu };
    size_t k; DWRITE_FONT_STYLE s; DWRITE_TEXT_RANGE r;
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetFontStyle(l, (DWRITE_FONT_STYLE)bad[k], RNG(0, 4));
        CHECKF(hr == E_INVALIDARG, "style %u -> %s", bad[k], dw_hr_name(hr));
    }
    CHECK_HR(IDWriteTextLayout_SetFontStyle(l, (DWRITE_FONT_STYLE)3, RNG(0, 0)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextLayout_GetFontStyle(l, 0, &s, &r)); EXPECT_EQ(s, DWRITE_FONT_STYLE_NORMAL);
    DW_RELEASE(l); TL_END();
}

TEST(prop_stretch_invalid_values)
{
    TL_BEGIN(); LAYOUT8(l);
    static const UINT32 bad[] = { 0, 10, 11, 100, 0x7fffffff, 0xffffffffu };
    size_t k; DWRITE_FONT_STRETCH s; DWRITE_TEXT_RANGE r; UINT32 v;
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)bad[k], RNG(0, 4));
        CHECKF(hr == E_INVALIDARG, "stretch %u -> %s", bad[k], dw_hr_name(hr));
    }
    for (v = 1; v <= 9; v++) {
        CHECK_HR_OK(IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)v, RNG(0, 4)));
        CHECK_HR_OK(IDWriteTextLayout_GetFontStretch(l, 0, &s, &r)); EXPECT_EQ(s, v);
    }
    DW_RELEASE(l); TL_END();
}

TEST(prop_size_invalid_values)
{
    TL_BEGIN(); LAYOUT8(l);
    static const float bad[] = { 0.0f, -0.0f, -1.0f, -FLT_MIN, -FLT_MAX, -INFINITY, NAN, -NAN };
    static const float good[] = { FLT_MIN, 0.001f, 1.0f, 16.0f, 1e6f, FLT_MAX };
    size_t k; float v; DWRITE_TEXT_RANGE r;
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetFontSize(l, bad[k], RNG(0, 4));
        CHECKF(hr == E_INVALIDARG, "size %g -> %s", bad[k], dw_hr_name(hr));
    }
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 0.0f, RNG(0, 0)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 0, &v, &r)); CHECK_FLOAT_EQ(v, 16.0f);
    for (k = 0; k < sizeof good / sizeof good[0]; k++) {
        CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, good[k], RNG(0, 4)));
        CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 0, &v, &r)); CHECK_FBITS(v, good[k]);
    }
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, INFINITY, RNG(0, 4)));    /* only <= 0 and NaN are rejected */
    DW_RELEASE(l); TL_END();
}

TEST(prop_family_null_rejected)
{
    TL_BEGIN(); LAYOUT8(l);
    DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, RNG(0, 4)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, RNG(0, 0)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, NULL, RNG(0, 4)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, NULL, RNG(0, 0)), E_INVALIDARG);
    CHECK(fam_get(l, 0, &r) == 0); CHECK(loc_get(l, 0, &r) == 0);
    DW_RELEASE(l); TL_END();
}

TEST(prop_underline_strikethrough_any_int)
{
    /* the flag is a BOOL: any non-zero value switches it on */
    TL_BEGIN(); LAYOUT8(l);
    static const BOOL v[] = { 1, 2, -1, 0x100, INT_MIN, INT_MAX };
    size_t k; BOOL u; DWRITE_TEXT_RANGE r;
    for (k = 0; k < sizeof v / sizeof v[0]; k++) {
        CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, v[k], RNG(0, 3)));
        u = 0xcc; CHECK_HR_OK(IDWriteTextLayout_GetUnderline(l, 0, &u, &r)); CHECKF(u != 0, "underline %d", v[k]);
        t_note("GetUnderline after Set(%d) = %d", v[k], u);
        CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, 0, RNG(0, 3)));
        u = 0xcc; CHECK_HR_OK(IDWriteTextLayout_GetUnderline(l, 0, &u, &r)); EXPECT_EQ(u, 0);
        CHECK_HR_OK(IDWriteTextLayout_SetStrikethrough(l, v[k], RNG(0, 3)));
        u = 0xcc; CHECK_HR_OK(IDWriteTextLayout_GetStrikethrough(l, 0, &u, &r)); CHECKF(u != 0, "strikethrough %d", v[k]);
        CHECK_HR_OK(IDWriteTextLayout_SetStrikethrough(l, 0, RNG(0, 3)));
    }
    DW_RELEASE(l); TL_END();
}

TEST(prop_underline_independent_of_strikethrough)
{
    TL_BEGIN(); LAYOUT8(l);
    DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4)));
    CHECK(st_get(l, 0, &r) == 0);
    CHECK_HR_OK(IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(2, 4)));
    CHECK(ul_get(l, 5, &r) == 0); CHK_RANGE(&PROPS[4], r, 4, 2);       /* runs are shared: bounded by the strikethrough run [2,6) */
    CHECK(st_get(l, 0, &r) == 0); CHK_RANGE(&PROPS[5], r, 0, 2);
    CHECK(ul_get(l, 3, &r) == 1); CHK_RANGE(&PROPS[4], r, 2, 2);
    DW_RELEASE(l); TL_END();
}

TEST(prop_runs_are_shared_between_properties)
{
    /* every character property lives in one run list (one record per run holds weight, size, underline, ...): the range a getter
     * reports is a run of that list, so a size change over [2,5) also bounds the range reported by the weight getter */
    TL_BEGIN(); LAYOUT8(l);
    DWRITE_TEXT_RANGE r = { 7, 7 }; DWRITE_FONT_WEIGHT w;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(2, 3)));
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 3, &w, &r)); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 3);
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 0, &w, &r)); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 2);
    CHECK_HR_OK(IDWriteTextLayout_GetUnderline(l, 6, (BOOL *)&w, &r)); EXPECT_EQ(r.startPosition, 5); EXPECT_EQ(r.length, MAXR - 5);
    DW_RELEASE(l); TL_END();
}

TEST(prop_bool_alternating_runs)
{
    TL_BEGIN(); LAYOUT8(l);
    int c; DWRITE_TEXT_RANGE r; BOOL u;
    for (c = 1; c < 8; c += 2) CHECK_HR_OK(IDWriteTextLayout_SetUnderline(l, TRUE, RNG(c, 1)));
    for (c = 0; c < 8; c++) {
        r.startPosition = r.length = 77; u = 0xcc;
        CHECK_HR_OK(IDWriteTextLayout_GetUnderline(l, c, &u, &r));
        CHECKF((u != 0) == (c & 1), "underline@%d = %d", c, u);
        if (c < 7) { EXPECT_EQ(r.startPosition, c); EXPECT_EQ(r.length, 1); }
    }
    DW_RELEASE(l); TL_END();
}

/* ---- family / locale getters: buffer protocol ------------------------------------------------------------------------- */
TEST(prop_family_name_length_and_buffers)
{
    TL_BEGIN(); LAYOUT8(l);
    DWRITE_TEXT_RANGE r; WCHAR buf[32]; UINT32 n = 0; size_t len = wcslen(TL_FAM);
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &n, &r)); EXPECT_EQ(n, len);
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, (UINT32)len + 1, &r)); CHECK_WEQ(buf, TL_FAM);
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, 32, &r)); CHECK_WEQ(buf, TL_FAM);
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, 32, NULL));
    wmemset(buf, 0x7777, 32);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, (UINT32)len, &r), DW_E_NOT_SUFFICIENT_BUFFER);
    wmemset(buf, 0x7777, 32);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, 1, &r), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECKF(buf[0] == 0, "first element is cleared before the copy: %x", buf[0]);
    wmemset(buf, 0x7777, 32);
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, 0, &r), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECKF(buf[0] == 0x7777, "size 0 leaves the buffer alone: %x", buf[0]);
    DW_RELEASE(l); TL_END();
}

TEST(prop_family_name_null_buffer)
{
    /* a NULL buffer is an argument error; the dereferencing variants (size > 0) are not exercised */
    TL_BEGIN(); LAYOUT8(l);
    DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, NULL, 0, &r), E_INVALIDARG);
    DW_RELEASE(l); TL_END();
}

TEST(prop_family_name_per_range)
{
    TL_BEGIN(); LAYOUT8(l);
    DWRITE_TEXT_RANGE r; WCHAR buf[32]; UINT32 n;
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, L"A Longer Family Name", RNG(2, 2)));
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyNameLength(l, 2, &n, &r)); EXPECT_EQ(n, 20); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 2);
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 3, buf, 32, &r)); CHECK_WEQ(buf, L"A Longer Family Name");
    CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 2, buf, 20, &r), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 2, buf, 21, &r));
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &n, &r)); EXPECT_EQ(n, wcslen(TL_FAM));
    DW_RELEASE(l); TL_END();
}

TEST(prop_family_name_stored_verbatim)
{
    /* no case folding or validation against installed fonts: whatever string is set is what comes back */
    TL_BEGIN(); LAYOUT8(l);
    static const WCHAR *const names[] = { L"dwtest", L"DWTEST", L"No Such Family 12345", L" ", L"a", L"Family;With;Semicolons", L"\x65E5\x672C\x8A9E" };
    size_t k; WCHAR buf[64]; DWRITE_TEXT_RANGE r; UINT32 n;
    for (k = 0; k < sizeof names / sizeof names[0]; k++) {
        CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, names[k], RNG(0, 8)));
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &n, &r)); EXPECT_EQ(n, wcslen(names[k]));
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, 64, &r)); CHECK_WEQ(buf, names[k]);
    }
    DW_RELEASE(l); TL_END();
}

TEST(prop_family_name_empty_string)
{
    TL_BEGIN(); LAYOUT8(l);
    WCHAR buf[8] = { 'x' }; DWRITE_TEXT_RANGE r; UINT32 n = 9; HRESULT hr;
    hr = IDWriteTextLayout_SetFontFamilyName(l, L"", RNG(0, 4));
    /* DWriteTextLayout::SetFontFamilyName only rejects a NULL name or an overflowing range; an empty string is stored as-is. */
    CHECK_HR(hr, S_OK);
    {
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &n, &r)); EXPECT_EQ(n, 0);
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, 8, &r)); CHECK(buf[0] == 0);
    }
    DW_RELEASE(l); TL_END();
}

TEST(prop_locale_name_buffers)
{
    TL_BEGIN(); LAYOUT8(l);
    DWRITE_TEXT_RANGE r; WCHAR buf[32]; UINT32 n = 0;
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleNameLength(l, 0, &n, &r)); EXPECT_EQ(n, 5);
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 0, buf, 6, &r)); CHECK(!_wcsicmp(buf, L"en-us"));
    wmemset(buf, 0x7777, 32);
    CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 0, buf, 5, &r), DW_E_NOT_SUFFICIENT_BUFFER);
    wmemset(buf, 0x7777, 32);
    CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 0, buf, 1, NULL), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECKF(buf[0] == 0, "first element cleared: %x", buf[0]);
    CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 0, NULL, 0, &r), E_INVALIDARG);
    DW_RELEASE(l); TL_END();
}

TEST(prop_locale_per_range_roundtrip)
{
    TL_BEGIN(); LAYOUT8(l);
    static const WCHAR *const loc[] = { L"fr-FR", L"de", L"ja-JP", L"zh-Hans-CN", L"en-GB", L"ar-SA", L"x-custom" };
    size_t k; WCHAR buf[64]; DWRITE_TEXT_RANGE r; UINT32 n;
    for (k = 0; k < sizeof loc / sizeof loc[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetLocaleName(l, loc[k], RNG(1, 3));
        CHECKF(hr == S_OK, "%ls -> %s", loc[k], dw_hr_name(hr));
        if (hr != S_OK) continue;
        CHECK_HR_OK(IDWriteTextLayout_GetLocaleNameLength(l, 2, &n, &r)); EXPECT_EQ(n, wcslen(loc[k]));
        CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 2, buf, 64, &r)); CHECKF(!_wcsicmp(buf, loc[k]), "got %ls want %ls", buf, loc[k]);
        EXPECT_EQ(r.startPosition, 1); EXPECT_EQ(r.length, 3);
    }
    DW_RELEASE(l); TL_END();
}

TEST(prop_layout_wide_name_getters_buffers)
{
    TL_BEGIN(); LAYOUT8(l);
    WCHAR buf[32]; size_t len = wcslen(TL_FAM);
    CHECK_HR_OK(l->lpVtbl->GetFontFamilyName(l, buf, (UINT32)len + 1)); CHECK_WEQ(buf, TL_FAM);
    wmemset(buf, 0x7777, 32);
    CHECK_HR(l->lpVtbl->GetFontFamilyName(l, buf, (UINT32)len), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR(l->lpVtbl->GetFontFamilyName(l, buf, 0), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR(l->lpVtbl->GetLocaleName(l, buf, 5), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR(l->lpVtbl->GetLocaleName(l, buf, 0), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK_HR_OK(l->lpVtbl->GetLocaleName(l, buf, 6));
    DW_RELEASE(l); TL_END();
}

/* ---- font collection per position -------------------------------------------------------------------------------------- */
TEST(prop_collection_default_is_creation_collection)
{
    TL_BEGIN(); LAYOUT8(l);
    IDWriteFontCollection *c = NULL, *c2 = NULL; DWRITE_TEXT_RANGE r = { 7, 7 };
    CHECK_HR_OK(IDWriteTextLayout_GetFontCollection(l, 0, &c, &r));
    CHECK(c != NULL);
    if (c) {
        CHECK(dw_same_identity((IUnknown *)c, (IUnknown *)X.coll));
        EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
        CHECK_HR_OK(l->lpVtbl->GetFontCollection(l, &c2));
        if (c2) { CHECK(dw_same_identity((IUnknown *)c2, (IUnknown *)X.coll)); IDWriteFontCollection_Release(c2); }
        IDWriteFontCollection_Release(c);
    }
    DW_RELEASE(l); TL_END();
}

TEST(prop_collection_getter_addrefs)
{
    TL_BEGIN(); LAYOUT8(l);
    IDWriteFontCollection *c = NULL; DWRITE_TEXT_RANGE r; ULONG base, after;
    base = dw_refcount((IUnknown *)X.coll);
    CHECK_HR_OK(IDWriteTextLayout_GetFontCollection(l, 0, &c, &r));
    after = dw_refcount((IUnknown *)X.coll);
    CHECKF(after == base + 1, "collection refcount %lu -> %lu", base, after);
    DW_RELEASE(c);
    EXPECT_REFS(X.coll, base);
    DW_RELEASE(l); TL_END();
}

TEST(prop_collection_per_range)
{
    TL_BEGIN(); LAYOUT8(l);
    IDWriteFontCollection *sys = NULL, *c = NULL; DWRITE_TEXT_RANGE r;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE));
    CHECK_HR_OK(IDWriteTextLayout_SetFontCollection(l, sys, RNG(2, 3)));
    CHECK_HR_OK(IDWriteTextLayout_GetFontCollection(l, 3, &c, &r));
    CHECK(c != NULL && dw_same_identity((IUnknown *)c, (IUnknown *)sys)); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 3);
    DW_RELEASE(c);
    CHECK_HR_OK(IDWriteTextLayout_GetFontCollection(l, 0, &c, &r));
    CHECK(c != NULL && dw_same_identity((IUnknown *)c, (IUnknown *)X.coll)); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 2);
    DW_RELEASE(c);
    CHECK_HR_OK(IDWriteTextLayout_GetFontCollection(l, 5, &c, &r));
    CHECK(c != NULL && dw_same_identity((IUnknown *)c, (IUnknown *)X.coll)); EXPECT_EQ(r.startPosition, 5);
    DW_RELEASE(c); DW_RELEASE(sys); DW_RELEASE(l); TL_END();
}

TEST(prop_collection_set_null_and_overflow)
{
    TL_BEGIN(); LAYOUT8(l);
    IDWriteFontCollection *c = NULL; DWRITE_TEXT_RANGE r; HRESULT hr;
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, X.coll, RNG(1, 0xFFFFFFFFu)), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, X.coll, RNG(0xFFFFFFFFu, 1)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextLayout_SetFontCollection(l, X.coll, RNG(1, 0)));
    hr = IDWriteTextLayout_SetFontCollection(l, NULL, RNG(0, 4));      /* NULL selects the system collection */
    CHECK_HR_OK(hr);
    hr = IDWriteTextLayout_GetFontCollection(l, 0, &c, &r);
    CHECK_HR_OK(hr);
    t_note("collection after Set(NULL) is %s (range %u,%u)", c ? "non-NULL" : "NULL", r.startPosition, r.length);
    DW_RELEASE(c); DW_RELEASE(l); TL_END();
}

/* ---- the extra per-range slots: drawing effect, inline object, typography ------------------------------------------------- */
TEST(prop_drawing_effect_roundtrip)
{
    TL_BEGIN(); LAYOUT8(l);
    dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    IUnknown *got = (IUnknown *)(ULONG_PTR)1; DWRITE_TEXT_RANGE r; LONG base;
    CHECK_HR_OK(IDWriteTextLayout_GetDrawingEffect(l, 0, &got, &r)); CHECK(got == NULL); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(1, 3)));
    CHECK(dwx_refs(e1) >= 2);                                   /* the layout retains it */
    base = dwx_refs(e1);
    CHECK_HR_OK(IDWriteTextLayout_GetDrawingEffect(l, 2, &got, &r)); CHECK(got == (IUnknown *)e1); EXPECT_EQ(r.startPosition, 1); EXPECT_EQ(r.length, 3);
    EXPECT_EQ(dwx_refs(e1), base + 1);                          /* the getter hands out a counted reference */
    IUnknown_Release(got);
    EXPECT_EQ(dwx_refs(e1), base);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(2, 1)));
    CHECK_HR_OK(IDWriteTextLayout_GetDrawingEffect(l, 2, &got, &r)); CHECK(got == (IUnknown *)e2); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 1); IUnknown_Release(got);
    CHECK_HR_OK(IDWriteTextLayout_GetDrawingEffect(l, 1, &got, &r)); CHECK(got == (IUnknown *)e1); EXPECT_EQ(r.length, 1); IUnknown_Release(got);
    CHECK_HR_OK(IDWriteTextLayout_GetDrawingEffect(l, 3, &got, &r)); CHECK(got == (IUnknown *)e1); EXPECT_EQ(r.startPosition, 3); IUnknown_Release(got);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, NULL, RNG(0, 8)));
    EXPECT_EQ(dwx_refs(e1), 1); EXPECT_EQ(dwx_refs(e2), 1);      /* everything released */
    CHECK_HR_OK(IDWriteTextLayout_GetDrawingEffect(l, 2, &got, &r)); CHECK(got == NULL);
    DW_RELEASE(l);
    dwx_release(e1); dwx_release(e2); TL_END();
}

TEST(prop_drawing_effect_released_with_layout)
{
    TL_BEGIN(); LAYOUT8(l);
    dwx_effect *e = dwx_effect_new(1);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 8)));
    CHECK(dwx_refs(e) >= 2);
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(e), 1);
    dwx_release(e); TL_END();
}

TEST(prop_drawing_effect_range_overflow)
{
    TL_BEGIN(); LAYOUT8(l);
    dwx_effect *e = dwx_effect_new(1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(5, 0xFFFFFFFFu)), E_INVALIDARG);
    EXPECT_EQ(dwx_refs(e), 1);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(5, 0)));
    EXPECT_EQ(dwx_refs(e), 1);                                   /* zero-length range retains nothing */
    DW_RELEASE(l); dwx_release(e); TL_END();
}

TEST(prop_inline_object_roundtrip)
{
    TL_BEGIN(); LAYOUT8(l);
    dwx_inline *o = dwx_inline_new(10.0f, 10.0f, 8.0f, FALSE);
    IDWriteInlineObject *got = (IDWriteInlineObject *)(ULONG_PTR)1; DWRITE_TEXT_RANGE r; LONG base;
    CHECK_HR_OK(IDWriteTextLayout_GetInlineObject(l, 0, &got, &r)); CHECK(got == NULL); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(3, 1)));
    CHECK(dwx_refs(o) >= 2); base = dwx_refs(o);
    CHECK_HR_OK(IDWriteTextLayout_GetInlineObject(l, 3, &got, &r)); CHECK(got == (IDWriteInlineObject *)o); EXPECT_EQ(r.startPosition, 3); EXPECT_EQ(r.length, 1);
    EXPECT_EQ(dwx_refs(o), base + 1); IDWriteInlineObject_Release(got);
    CHECK_HR_OK(IDWriteTextLayout_GetInlineObject(l, 0, &got, &r)); CHECK(got == NULL); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, 3);
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, NULL, RNG(3, 1)));
    EXPECT_EQ(dwx_refs(o), 1);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(4, 0xFFFFFFFFu)), E_INVALIDARG);
    DW_RELEASE(l); dwx_release(o); TL_END();
}

TEST(prop_typography_roundtrip)
{
    TL_BEGIN(); LAYOUT8(l);
    IDWriteTypography *ty = NULL, *got = (IDWriteTypography *)(ULONG_PTR)1; DWRITE_TEXT_RANGE r;
    DW_REQUIRE_OK(IDWriteFactory_CreateTypography(f, &ty));
    CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 0, &got, &r)); CHECK(got == NULL); EXPECT_EQ(r.startPosition, 0); EXPECT_EQ(r.length, MAXR);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, ty, RNG(1, 4)));
    CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 2, &got, &r)); CHECK(got == ty); EXPECT_EQ(r.startPosition, 1); EXPECT_EQ(r.length, 4);
    DW_RELEASE(got);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, ty, RNG(1, 0xFFFFFFFFu)), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, NULL, RNG(1, 4)));
    CHECK_HR_OK(IDWriteTextLayout_GetTypography(l, 2, &got, &r)); CHECK(got == NULL);
    DW_RELEASE(ty); DW_RELEASE(l); TL_END();
}
