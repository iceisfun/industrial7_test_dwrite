/* Querying and filtering font sets: GetPropertyOccurrenceCount, both GetMatchingFonts overloads of IDWriteFontSet,
 * IDWriteFontSet1::GetMatchingFonts(property, axis values), GetFilteredFonts / GetFilteredFontIndices (properties, indices,
 * axis ranges), GetFirstFontResources and GetFontAxisRanges.
 *
 * Win10 decompilation (DWriteFontSet / FontSet):
 *  - property values are compared with CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE); the locale of the DWRITE_FONT_PROPERTY is
 *    ignored; an id above 14 matches nothing (no error); GetPropertyOccurrenceCount(NULL) throws E_INVALIDARG after zeroing *count.
 *  - GetMatchingFonts(props, n) = GetFilteredFonts(props, n, FALSE): a font is kept when it matches every property; results keep the
 *    order of the source set. With n == 0 the result is a copy of the whole set. NULL props with n > 0 -> E_INVALIDARG.
 *  - GetFilteredFontIndices zero-fills the buffer, reports the full match count in *actual and returns
 *    HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) when the buffer is smaller than that.
 *  - GetFilteredFonts(indices): any index >= count -> ArgumentException("indices") = E_INVALIDARG; results follow the order of the
 *    index array.
 *  - GetMatchingFonts(family, weight, stretch, style) validates 1 <= weight <= 999, 1 <= stretch <= 9, style <= 2 (E_INVALIDARG),
 *    selects by WWS family name and orders the family by closeness to the request.
 *  - ranges filtering with zero ranges: all-ranges mode keeps every font, any-range mode keeps none.
 */
#include "fs.h"

static UINT32 cnt(IDWriteFontSet *s) { return IDWriteFontSet_GetFontCount(s); }
static int has_full(IDWriteFontSet *s, const char *full) { return fs_find_by(s, DWRITE_FONT_PROPERTY_ID_FULL_NAME, full) != FS_NOT_FOUND; }
static void full_of(IDWriteFontSet *s, UINT32 i, char *buf, int cap) { if (!fs_font_prop(s, i, DWRITE_FONT_PROPERTY_ID_FULL_NAME, buf, cap)) strcpy(buf, "?"); }
/* count with a (possibly NULL) locale; 0xDEAD0000 on failure */
static UINT32 occ(IDWriteFontSet *s, DWRITE_FONT_PROPERTY_ID id, const char *value, const char *loc)
{
    fs_prop p; UINT32 c = 0xDEAD0000u; HRESULT hr;
    fs_prop_set(&p, id, value, loc);
    hr = IDWriteFontSet_GetPropertyOccurrenceCount(s, &p.p, &c);
    return SUCCEEDED(hr) ? c : 0xDEAD0000u;
}
/* All names (full names) of a set joined for diagnostics. */
static const char *names(IDWriteFontSet *s)
{
    static char b[512]; UINT32 i, n = cnt(s); b[0] = 0;
    for (i = 0; i < n && strlen(b) < 400; i++) { char f[64]; full_of(s, i, f, sizeof f); strcat(b, f); strcat(b, i + 1 < n ? "|" : ""); }
    return b;
}
/* Index of the item of `sub` inside `parent` (via reference lookup); FS_NOT_FOUND if absent. */
static UINT32 parent_index(IDWriteFontSet *parent, IDWriteFontSet *sub, UINT32 i)
{
    IDWriteFontFaceReference *r = NULL; UINT32 idx = FS_NOT_FOUND; BOOL ex = FALSE;
    if (FAILED(IDWriteFontSet_GetFontFaceReference(sub, i, &r)) || !r) return FS_NOT_FOUND;
    IDWriteFontSet_FindFontFaceReference(parent, r, &idx, &ex);
    DW_RELEASE(r);
    return ex ? idx : FS_NOT_FOUND;
}
/* sub's items map to strictly ascending indices of parent (order of the source set preserved). */
static int ascending_in_parent(IDWriteFontSet *parent, IDWriteFontSet *sub)
{
    UINT32 i, last = 0; int first = 1;
    for (i = 0; i < cnt(sub); i++) {
        UINT32 p = parent_index(parent, sub, i);
        if (p == FS_NOT_FOUND) return 0;
        if (!first && p <= last) return 0;
        last = p; first = 0;
    }
    return 1;
}

/* ===== GetPropertyOccurrenceCount ====================================================================== */
TEST(fset_q_occ_family)
{
    FS_STD(s);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", "en-us") == 4);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB", "en-us") == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthC", "en-us") == 0);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "", "en-us") == 0);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA ", "en-us") == 0);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Synth", "en-us") == 0);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthAA", "en-us") == 0);
    fs_std_close(&s);
}

TEST(fset_q_occ_case_insensitive)
{
    FS_STD(s);
    const char *v[] = { "synthA", "SYNTHA", "syntha", "sYnThA" };
    unsigned i;
    for (i = 0; i < 4; i++) {
        UINT32 c = occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, v[i], "en-us");
        CHECKF(c == 4, "\"%s\" -> %u", v[i], c);
    }
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "synthA bold", "en-us") == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, "SYNTHA-REGULAR", "en-us") == 1);
    fs_std_close(&s);
}

TEST(fset_q_occ_locale_ignored)
{
    FS_STD(s);
    const char *loc[] = { NULL, "en-us", "fr-fr", "zz-ZZ", "", "not a locale at all" };
    unsigned i;
    for (i = 0; i < 6; i++) {
        UINT32 c = occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", loc[i]);
        CHECKF(c == 4, "locale %s -> %u", loc[i] ? loc[i] : "(null)", c);
    }
    fs_std_close(&s);
}

TEST(fset_q_occ_numeric_properties)
{
    FS_STD(s);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT, "400", NULL) == 3);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL) == 2);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT, "300", NULL) == 0);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_STRETCH, "5", NULL) == 5);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_STRETCH, "3", NULL) == 0);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_STYLE, "0", NULL) == 3);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_STYLE, "2", NULL) == 2);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_STYLE, "1", NULL) == 0);
    /* values are strings: a different spelling of the same number does not match */
    FS_SOFT(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT, "0400", NULL) == 0);
    FS_SOFT(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT, " 400", NULL) == 0);
    fs_std_close(&s);
}

TEST(fset_q_occ_name_ids)
{
    FS_STD(s);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, "SynthA", NULL) == 4);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, "SynthA", NULL) == 4);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, "SynthB", NULL) == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "SynthA Bold", NULL) == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "SynthA", NULL) == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "SynthB", NULL) == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, "SynthA-Bold", NULL) == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, "SynthA-BoldItalic", NULL) == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, "Bold", NULL) == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, "Regular", NULL) == 2);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, "Bold Italic", NULL) == 1);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME, "Italic", NULL) == 1);
    /* a value of one id never matches under another id */
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA Bold", NULL) == 0);
    CHECK(occ(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "700", NULL) == 0);
    fs_std_close(&s);
}

TEST(fset_q_occ_invalid_and_none_ids)
{
    FS_STD(s);
    int ids[] = { 0, 15, 16, 100, 0x7FFFFFFF, -1 };
    unsigned i;
    for (i = 0; i < 6; i++) {
        fs_prop p; UINT32 c = 77; fs_prop_set(&p, (DWRITE_FONT_PROPERTY_ID)ids[i], "SynthA", "en-us");
        HRESULT hr = IDWriteFontSet_GetPropertyOccurrenceCount(s.set, &p.p, &c);
        CHECKF(hr == S_OK && c == 0, "id %d -> %s count %u", ids[i], dw_hr_name(hr), c);
    }
    fs_std_close(&s);
}

TEST(fset_q_occ_null_property)
{
    FS_STD(s);
    UINT32 c = 55;
    HRESULT hr = IDWriteFontSet_GetPropertyOccurrenceCount(s.set, NULL, &c);
    CHECK_HR(hr, E_INVALIDARG);
    CHECKF(c == 0, "count %u (zeroed before the check)", c);
    fs_std_close(&s);
}

TEST(fset_q_occ_on_subset)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *sub = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &sub));
    if (sub) {
        CHECK(cnt(sub) == 1);
        CHECK(occ(sub, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB", NULL) == 1);
        CHECKF(occ(sub, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", NULL) == 0, "subset must not count fonts outside it");
        CHECKF(occ(sub, DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL) == 0, "bold fonts are not in the subset");
        CHECK(occ(sub, DWRITE_FONT_PROPERTY_ID_WEIGHT, "400", NULL) == 1);
        DW_RELEASE(sub);
    }
    fs_std_close(&s);
}

TEST(fset_q_occ_system_arial)
{
    NEED_SYSSET(set);
    if (!dw_have_font(dw_shared(), L"Arial")) { t_skip("Arial not installed"); DW_RELEASE(set); return; }
    UINT32 c = occ(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Arial", "en-us");
    CHECKF(c >= 4 && c < 40, "Arial faces: %u", c);
    t_note("Arial WWS family occurrences: %u", c);
    CHECK(occ(set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "Arial", NULL) >= 1);
    CHECK(occ(set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "Arial Bold", NULL) >= 1);
    CHECK(occ(set, DWRITE_FONT_PROPERTY_ID_WEIGHT, "400", NULL) > 20);
    CHECK(occ(set, DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL) > 5);
    DW_RELEASE(set);
}

/* ===== GetMatchingFonts(props, count) ===================================================================== */
TEST(fset_q_mf_zero_props_is_whole_set)
{
    FS_STD(s);
    IDWriteFontSet *out = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, NULL, 0, &out));
    if (out) {
        CHECK(out != s.set);
        CHECKF(cnt(out) == 5, "count %u", cnt(out));
        CHECK(ascending_in_parent(s.set, out));
        DW_RELEASE(out);
    }
    fs_std_close(&s);
}

TEST(fset_q_mf_null_props_nonzero_count)
{
    FS_STD(s);
    IDWriteFontSet *out = (IDWriteFontSet *)FS_SENT;
    HRESULT hr = IDWriteFontSet_GetMatchingFonts(s.set, NULL, 1, &out);
    CHECK_HR(hr, E_INVALIDARG);
    CHECK_OUT_NULL(out);
    FS_DROP(out, hr);
    out = (IDWriteFontSet *)FS_SENT;
    hr = IDWriteFontSet_GetMatchingFonts(s.set, NULL, 0xFFFFFFFFu, &out);
    CHECK_HR(hr, E_INVALIDARG);
    CHECK_OUT_NULL(out);
    FS_DROP(out, hr);
    fs_std_close(&s);
}

TEST(fset_q_mf_single_family)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *out = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", "en-us");
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out));
    if (out) {
        UINT32 i;
        CHECKF(cnt(out) == 4, "count %u: %s", cnt(out), names(out));
        for (i = 0; i < cnt(out); i++) {
            char b[64];
            fs_font_prop(out, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, b, sizeof b);
            CHECK(!strcmp(b, "SynthA"));
        }
        CHECK(ascending_in_parent(s.set, out));
        DW_RELEASE(out);
    }
    fs_std_close(&s);
}

TEST(fset_q_mf_single_weight)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *out = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out));
    if (out) {
        CHECKF(cnt(out) == 2, "count %u: %s", cnt(out), names(out));
        CHECK(has_full(out, "SynthA Bold") && has_full(out, "SynthA Bold Italic"));
        DW_RELEASE(out);
    }
    fs_std_close(&s);
}

TEST(fset_q_mf_two_properties_are_anded)
{
    FS_STD(s);
    fs_prop p[3]; DWRITE_FONT_PROPERTY arr[3]; IDWriteFontSet *out = NULL;
    fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", "en-us");
    fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL);
    fs_prop_set(&p[2], DWRITE_FONT_PROPERTY_ID_STYLE, "2", NULL);
    arr[0] = p[0].p; arr[1] = p[1].p; arr[2] = p[2].p;
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, arr, 2, &out));
    if (out) { CHECKF(cnt(out) == 2, "family+weight: %s", names(out)); DW_RELEASE(out); }
    out = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, arr, 3, &out));
    if (out) { CHECKF(cnt(out) == 1 && has_full(out, "SynthA Bold Italic"), "family+weight+style: %s", names(out)); DW_RELEASE(out); }
    fs_std_close(&s);
}

TEST(fset_q_mf_conflicting_values_match_nothing)
{
    FS_STD(s);
    fs_prop p[2]; DWRITE_FONT_PROPERTY arr[2]; IDWriteFontSet *out = NULL;
    fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", NULL);
    fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB", NULL);
    arr[0] = p[0].p; arr[1] = p[1].p;
    /* every property has to match, and no font has two WWS family names */
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, arr, 2, &out));
    if (out) { CHECKF(cnt(out) == 0, "count %u", cnt(out)); DW_RELEASE(out); }
    fs_std_close(&s);
}

TEST(fset_q_mf_no_match_is_empty_set)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *out = (IDWriteFontSet *)FS_SENT;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "NoSuchFamily", "en-us");
    HRESULT hr = IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out);
    CHECK_HR_OK(hr);
    CHECK(out != NULL && out != FS_SENT);
    if (SUCCEEDED(hr) && out) { CHECK(cnt(out) == 0); DW_RELEASE(out); }
    fs_std_close(&s);
}

TEST(fset_q_mf_invalid_property_id_matches_nothing)
{
    FS_STD(s);
    int ids[] = { 15, 99, 0x7FFFFFFF };
    unsigned i;
    for (i = 0; i < 3; i++) {
        fs_prop p; IDWriteFontSet *out = NULL;
        fs_prop_set(&p, (DWRITE_FONT_PROPERTY_ID)ids[i], "SynthA", NULL);
        HRESULT hr = IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out);
        CHECKF(hr == S_OK, "id %d -> %s", ids[i], dw_hr_name(hr));
        if (out) { CHECK(cnt(out) == 0); DW_RELEASE(out); }
    }
    fs_std_close(&s);
}

TEST(fset_q_mf_case_insensitive)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *out = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_FULL_NAME, "SYNTHA BOLD ITALIC", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out));
    if (out) { CHECK(cnt(out) == 1); DW_RELEASE(out); }
    fs_std_close(&s);
}

TEST(fset_q_mf_result_object)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *out = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out));
    if (out) {
        EXPECT_REFS(out, 1);
        dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontSet, 1), DW_QI_ROW(IDWriteFontSet1, 1), DW_QI_ROW(IDWriteFontSet2, 1),
                             DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontSetBuilder, 0) };
        DW_CHECK_QI(out, rows);
        DW_RELEASE(out);
    }
    fs_std_close(&s);
}

TEST(fset_q_mf_result_outlives_source)
{
    fs_std s;
    if (!fs_std_open(&s)) return;
    fs_prop p; IDWriteFontSet *out = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out));
    DW_RELEASE(s.set);   /* the subset keeps what it needs */
    if (out) {
        char b[64];
        CHECK(cnt(out) == 4);
        CHECK(fs_font_prop(out, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, b, sizeof b));
        IDWriteStringList *l = fs_props(out, DWRITE_FONT_PROPERTY_ID_WEIGHT);
        if (l) { CHECK(IDWriteStringList_GetCount(l) == 2); DW_RELEASE(l); }
        DW_RELEASE(out);
    }
    fs_std_close(&s);
}

TEST(fset_q_mf_subset_properties_are_subset_only)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *out = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out));
    if (out) {
        IDWriteStringList *l = fs_props(out, DWRITE_FONT_PROPERTY_ID_WEIGHT);
        if (l) { CHECKF(IDWriteStringList_GetCount(l) == 1 && fs_sl_has(l, "700"), "weights: %u", IDWriteStringList_GetCount(l)); DW_RELEASE(l); }
        l = fs_props(out, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
        if (l) { CHECKF(IDWriteStringList_GetCount(l) == 1 && fs_sl_has(l, "SynthA"), "families: %u", IDWriteStringList_GetCount(l)); DW_RELEASE(l); }
        l = fs_props(out, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
        if (l) { CHECKF(IDWriteStringList_GetCount(l) == 2, "full names: %u", IDWriteStringList_GetCount(l)); DW_RELEASE(l); }
        DW_RELEASE(out);
    }
    fs_std_close(&s);
}

TEST(fset_q_mf_subset_index_space)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *out = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out));
    if (out && cnt(out) == 1) {
        /* SynthB is item 4 of the parent but item 0 of the subset */
        IDWriteFontFaceReference *r = NULL; UINT32 idx = 99, pidx; BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(out, 0, &r));
        if (r) {
            CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(out, r, &idx, &ex));
            CHECK(ex == TRUE && idx == 0);
            pidx = parent_index(s.set, out, 0);
            CHECK(pidx < 5);
            char b[64]; full_of(s.set, pidx, b, sizeof b);
            CHECK(!strcmp(b, "SynthB"));
            IDWriteFontFaceReference *r2 = (IDWriteFontFaceReference *)FS_SENT;
            HRESULT hr = IDWriteFontSet_GetFontFaceReference(out, 1, &r2);
            CHECK_HR(hr, E_INVALIDARG);
            CHECK_OUT_NULL(r2);
            FS_DROP(r2, hr);
            DW_RELEASE(r);
        }
    }
    DW_RELEASE(out);
    fs_std_close(&s);
}

TEST(fset_q_mf_nested_filter)
{
    FS_STD(s);
    fs_prop p1, p2; IDWriteFontSet *a = NULL, *b = NULL;
    fs_prop_set(&p1, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", NULL);
    fs_prop_set(&p2, DWRITE_FONT_PROPERTY_ID_STYLE, "2", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p1.p, 1, &a));
    if (a) {
        CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(a, &p2.p, 1, &b));
        if (b) { CHECKF(cnt(b) == 2, "%s", names(b)); CHECK(has_full(b, "SynthA Italic") && has_full(b, "SynthA Bold Italic")); DW_RELEASE(b); }
        DW_RELEASE(a);
    }
    fs_std_close(&s);
}

TEST(fset_q_mf_result_items_equal_source_items)
{
    FS_STD(s);
    fs_prop p; IDWriteFontSet *out = NULL;
    fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_STRETCH, "5", NULL);
    CHECK_HR_OK(IDWriteFontSet_GetMatchingFonts(s.set, &p.p, 1, &out));
    if (out) {
        UINT32 i;
        CHECK(cnt(out) == 5);
        for (i = 0; i < cnt(out); i++) {
            IDWriteFontFaceReference *a = NULL, *b = NULL; UINT32 pi = parent_index(s.set, out, i);
            CHECKF(pi < 5, "item %u maps into the parent", i);
            if (pi < 5 && SUCCEEDED(IDWriteFontSet_GetFontFaceReference(out, i, &a)) && SUCCEEDED(IDWriteFontSet_GetFontFaceReference(s.set, pi, &b)) && a && b)
                CHECK(IDWriteFontFaceReference_Equals(a, b));
            DW_RELEASE(a); DW_RELEASE(b);
        }
        DW_RELEASE(out);
    }
    fs_std_close(&s);
}

/* ===== GetMatchingFonts(family, weight, stretch, style) ========================================================== */
static IDWriteFontSet *mf_family(IDWriteFontSet *set, const WCHAR *fam, int w, int st, int style, HRESULT *phr)
{
    IDWriteFontSet *out = NULL;
    HRESULT hr = IDWriteFontSet_GetMatchingFonts_(set, fam, (DWRITE_FONT_WEIGHT)w, (DWRITE_FONT_STRETCH)st, (DWRITE_FONT_STYLE)style, &out);
    if (phr) *phr = hr;
    return out;
}

TEST(fset_q_mff_exact_regular_first)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontSet *out = mf_family(s.set, L"SynthA", 400, 5, 0, &hr);
    CHECK_HR_OK(hr);
    if (out) {
        char b[64];
        CHECKF(cnt(out) >= 1 && cnt(out) <= 4, "count %u: %s", cnt(out), names(out));
        full_of(out, 0, b, sizeof b);
        CHECKF(!strcmp(b, "SynthA"), "first = \"%s\"", b);
        FS_SOFT(cnt(out) == 4);
        DW_RELEASE(out);
    }
    fs_std_close(&s);
}

TEST(fset_q_mff_exact_italic_first)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontSet *out = mf_family(s.set, L"SynthA", 400, 5, DWRITE_FONT_STYLE_ITALIC, &hr);
    CHECK_HR_OK(hr);
    if (out && cnt(out) > 0) {
        char b[64]; full_of(out, 0, b, sizeof b);
        CHECKF(!strcmp(b, "SynthA Italic"), "first = \"%s\" (%s)", b, names(out));
    }
    DW_RELEASE(out);
    fs_std_close(&s);
}

TEST(fset_q_mff_bold_ordering)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontSet *out = mf_family(s.set, L"SynthA", 700, 5, 0, &hr);
    CHECK_HR_OK(hr);
    if (out && cnt(out) > 0) {
        char b[64]; full_of(out, 0, b, sizeof b);
        /* a real bold face and a bold-simulated regular face are equally close; the real one or its simulation leads */
        CHECKF(!strcmp(b, "SynthA Bold") || !strcmp(b, "SynthA"), "first = \"%s\" (%s)", b, names(out));
        FS_SOFT(!strcmp(b, "SynthA Bold"));
        t_note("bold request order: %s", names(out));
    }
    DW_RELEASE(out);
    out = mf_family(s.set, L"SynthA", 700, 5, DWRITE_FONT_STYLE_ITALIC, &hr);
    if (out && cnt(out) > 0) {
        char b[64]; full_of(out, 0, b, sizeof b);
        FS_SOFT(!strcmp(b, "SynthA Bold Italic"));
        t_note("bold italic request order: %s", names(out));
    }
    DW_RELEASE(out);
    fs_std_close(&s);
}

TEST(fset_q_mff_only_family_members)
{
    FS_STD(s);
    int w;
    for (w = 100; w <= 900; w += 200) {
        HRESULT hr; IDWriteFontSet *out = mf_family(s.set, L"SynthA", w, 5, 0, &hr);
        CHECK_HR_OK(hr);
        if (out) {
            UINT32 i;
            for (i = 0; i < cnt(out); i++) {
                char b[64]; fs_font_prop(out, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, b, sizeof b);
                CHECKF(!strcmp(b, "SynthA"), "weight %d item %u family \"%s\"", w, i, b);
            }
            DW_RELEASE(out);
        }
    }
    fs_std_close(&s);
}

TEST(fset_q_mff_unknown_family)
{
    FS_STD(s);
    HRESULT hr; IDWriteFontSet *out = mf_family(s.set, L"NoSuchFamily", 400, 5, 0, &hr);
    CHECK_HR_OK(hr);
    CHECK(out != NULL);
    if (out) { CHECK(cnt(out) == 0); DW_RELEASE(out); }
    out = mf_family(s.set, L"", 400, 5, 0, &hr);
    CHECK_HR_OK(hr);
    if (out) { CHECK(cnt(out) == 0); DW_RELEASE(out); }
    fs_std_close(&s);
}

TEST(fset_q_mff_case_insensitive_family)
{
    FS_STD(s);
    const WCHAR *names_[] = { L"synthA", L"SYNTHA", L"synthb" };
    unsigned i;
    for (i = 0; i < 3; i++) {
        HRESULT hr; IDWriteFontSet *out = mf_family(s.set, names_[i], 400, 5, 0, &hr);
        CHECK_HR_OK(hr);
        if (out) { CHECKF(cnt(out) >= 1, "%ls", names_[i]); DW_RELEASE(out); }
    }
    fs_std_close(&s);
}

TEST(fset_q_mff_only_wws_family_names)
{
    FS_STD(s);
    /* the lookup uses the WWS family name only: a full name or a PostScript name finds nothing */
    const WCHAR *other[] = { L"SynthA Bold", L"SynthA-Regular", L"Bold", L"SynthA Bold Italic" };
    unsigned i;
    for (i = 0; i < 4; i++) {
        HRESULT hr; IDWriteFontSet *out = mf_family(s.set, other[i], 400, 5, 0, &hr);
        CHECK_HR_OK(hr);
        if (out) { CHECKF(cnt(out) == 0, "%ls -> %u", other[i], cnt(out)); DW_RELEASE(out); }
    }
    fs_std_close(&s);
}

TEST(fset_q_mff_enum_validation)
{
    FS_STD(s);
    struct { int w, st, style; HRESULT want; const char *what; } t[] = {
        { 400, 5, 0, S_OK, "normal" },
        { 1, 5, 0, S_OK, "weight 1" }, { 999, 5, 0, S_OK, "weight 999" },
        { 0, 5, 0, E_INVALIDARG, "weight 0" }, { 1000, 5, 0, E_INVALIDARG, "weight 1000" }, { -1, 5, 0, E_INVALIDARG, "weight -1" },
        { 0x7FFFFFFF, 5, 0, E_INVALIDARG, "weight INT_MAX" },
        { 400, 1, 0, S_OK, "stretch 1" }, { 400, 9, 0, S_OK, "stretch 9" },
        { 400, 0, 0, E_INVALIDARG, "stretch 0" }, { 400, 10, 0, E_INVALIDARG, "stretch 10" }, { 400, -1, 0, E_INVALIDARG, "stretch -1" },
        { 400, 5, 1, S_OK, "oblique" }, { 400, 5, 2, S_OK, "italic" },
        { 400, 5, 3, E_INVALIDARG, "style 3" }, { 400, 5, -1, E_INVALIDARG, "style -1" }, { 400, 5, 0x7FFFFFFF, E_INVALIDARG, "style INT_MAX" },
    };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDWriteFontSet *out = (IDWriteFontSet *)FS_SENT;
        HRESULT hr = IDWriteFontSet_GetMatchingFonts_(s.set, L"SynthA", (DWRITE_FONT_WEIGHT)t[i].w, (DWRITE_FONT_STRETCH)t[i].st, (DWRITE_FONT_STYLE)t[i].style, &out);
        CHECKF(hr == t[i].want, "%s -> %s", t[i].what, dw_hr_name(hr));
        if (FAILED(hr)) CHECK_OUT_NULL(out);
        FS_DROP(out, hr);
    }
    fs_std_close(&s);
}

TEST(fset_q_mff_validation_before_family_lookup)
{
    FS_STD(s);
    /* an unknown family does not mask an invalid enum value */
    IDWriteFontSet *out = (IDWriteFontSet *)FS_SENT;
    HRESULT hr = IDWriteFontSet_GetMatchingFonts_(s.set, L"NoSuchFamily", (DWRITE_FONT_WEIGHT)0, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &out);
    CHECK_HR(hr, E_INVALIDARG);
    CHECK_OUT_NULL(out);
    FS_DROP(out, hr);
    fs_std_close(&s);
}

TEST(fset_q_mff_single_member_family)
{
    FS_STD(s);
    int w;
    for (w = 100; w <= 900; w += 100) {
        HRESULT hr; IDWriteFontSet *out = mf_family(s.set, L"SynthB", w, 5, 0, &hr);
        CHECK_HR_OK(hr);
        if (out) {
            char b[64];
            CHECKF(cnt(out) >= 1, "weight %d", w);
            if (cnt(out) > 0) { full_of(out, 0, b, sizeof b); CHECKF(!strcmp(b, "SynthB"), "weight %d first \"%s\"", w, b); }
            DW_RELEASE(out);
        }
    }
    fs_std_close(&s);
}

TEST(fset_q_mff_result_independent_of_source)
{
    fs_std s;
    if (!fs_std_open(&s)) return;
    HRESULT hr; IDWriteFontSet *out = mf_family(s.set, L"SynthA", 400, 5, 0, &hr);
    DW_RELEASE(s.set);
    if (out) { CHECK(cnt(out) >= 1); IDWriteStringList *l = fs_props(out, DWRITE_FONT_PROPERTY_ID_FULL_NAME); CHECK(l != NULL); DW_RELEASE(l); DW_RELEASE(out); }
    fs_std_close(&s);
}

TEST(fset_q_mff_system_arial)
{
    NEED_SYSSET(set);
    if (!dw_have_font(dw_shared(), L"Arial")) { t_skip("Arial not installed"); DW_RELEASE(set); return; }
    HRESULT hr; IDWriteFontSet *out = mf_family(set, L"Arial", 700, 5, 0, &hr);
    CHECK_HR_OK(hr);
    if (out) {
        UINT32 i, n = cnt(out);
        CHECKF(n >= 1, "count %u", n);
        for (i = 0; i < n; i++) {
            char b[64]; fs_font_prop(out, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, b, sizeof b);
            CHECKF(!_stricmp(b, "Arial"), "item %u family \"%s\"", i, b);
        }
        if (n) {
            char w[16]; fs_font_prop(out, 0, DWRITE_FONT_PROPERTY_ID_WEIGHT, w, sizeof w);
            CHECKF(fs_atoi(w) == 700, "first item weight %s", w);
            char f[64]; full_of(out, 0, f, sizeof f);
            t_note("Arial bold request first item: %s", f);
        }
        DW_RELEASE(out);
    }
    DW_RELEASE(set);
}

/* ===== IDWriteFontSet1::GetMatchingFonts(property, axis values) =================================================== */
TEST(fset_q_s1mf_null_property_all_fonts)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set); IDWriteFontSet1 *out = NULL;
    if (s1) {
        DWRITE_FONT_AXIS_VALUE av[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
        CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s1, NULL, av, 1, &out));
        if (out) { CHECKF(IDWriteFontSet1_GetFontCount(out) == 5, "count %u", IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        out = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s1, NULL, NULL, 0, &out));
        if (out) { CHECKF(IDWriteFontSet1_GetFontCount(out) == 5, "count %u", IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_s1mf_invalid_axis_args)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        DWRITE_FONT_AXIS_VALUE av[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } };
        IDWriteFontSet1 *out = (IDWriteFontSet1 *)FS_SENT;
        HRESULT hr = IDWriteFontSet1_GetMatchingFonts(s1, NULL, NULL, 1, &out);
        CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(out); FS_DROP(out, hr);
        out = (IDWriteFontSet1 *)FS_SENT;
        hr = IDWriteFontSet1_GetMatchingFonts(s1, NULL, av, 0x10000, &out);
        CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(out); FS_DROP(out, hr);
        out = (IDWriteFontSet1 *)FS_SENT;
        hr = IDWriteFontSet1_GetMatchingFonts(s1, NULL, av, 0xFFFFFFFFu, &out);
        CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(out); FS_DROP(out, hr);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_s1mf_max_axis_count)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 n = 0xFFFF, i; IDWriteFontSet1 *out = NULL;
        DWRITE_FONT_AXIS_VALUE *av = (DWRITE_FONT_AXIS_VALUE *)calloc(n, sizeof *av);
        if (av) {
            for (i = 0; i < n; i++) { av[i].axisTag = DWRITE_FONT_AXIS_TAG_WEIGHT; av[i].value = 400.0f; }
            HRESULT hr = IDWriteFontSet1_GetMatchingFonts(s1, NULL, av, n, &out);
            CHECKF(hr == S_OK, "0xFFFF axis values -> %s", dw_hr_name(hr));
            DW_RELEASE(out);
            free(av);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_s1mf_weight_axis_orders_by_distance)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p; IDWriteFontSet1 *out = NULL; char w[16];
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", NULL);
        DWRITE_FONT_AXIS_VALUE v400[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f } }, v700[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f } };
        CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s1, &p.p, v400, 1, &out));
        if (out) {
            CHECKF(IDWriteFontSet1_GetFontCount(out) == 4, "count %u", IDWriteFontSet1_GetFontCount(out));
            fs_font_prop(FS_AS_SET(out), 0, DWRITE_FONT_PROPERTY_ID_WEIGHT, w, sizeof w);
            CHECKF(fs_atoi(w) == 400, "wght 400 -> first weight %s", w);
            DW_RELEASE(out);
        }
        out = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s1, &p.p, v700, 1, &out));
        if (out) {
            fs_font_prop(FS_AS_SET(out), 0, DWRITE_FONT_PROPERTY_ID_WEIGHT, w, sizeof w);
            CHECKF(fs_atoi(w) == 700, "wght 700 -> first weight %s", w);
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_s1mf_is_a_subset)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p; IDWriteFontSet1 *out = NULL;
        DWRITE_FONT_AXIS_VALUE v[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f } };
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB", NULL);
        CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s1, &p.p, v, 1, &out));
        if (out) {
            CHECK(IDWriteFontSet1_GetFontCount(out) == 1);
            CHECK(parent_index(s.set, FS_AS_SET(out), 0) == 4);
            DW_RELEASE(out);
        }
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Nobody", NULL);
        CHECK_HR_OK(IDWriteFontSet1_GetMatchingFonts(s1, &p.p, v, 1, &out));
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

/* ===== GetFilteredFonts(properties) and GetFilteredFontIndices(properties) ============================================== */
TEST(fset_q_gff_any_vs_all)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p[2]; DWRITE_FONT_PROPERTY arr[2]; IDWriteFontSet1 *out = NULL;
        fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB", NULL);
        fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL);
        arr[0] = p[0].p; arr[1] = p[1].p;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts(s1, arr, 2, FALSE, &out));
        if (out) { CHECKF(IDWriteFontSet1_GetFontCount(out) == 0, "all: %u", IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        out = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts(s1, arr, 2, TRUE, &out));
        if (out) { CHECKF(IDWriteFontSet1_GetFontCount(out) == 3, "any: %u (%s)", IDWriteFontSet1_GetFontCount(out), names(FS_AS_SET(out))); DW_RELEASE(out); }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gff_selectany_nonzero_is_true)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p[2]; DWRITE_FONT_PROPERTY arr[2]; BOOL flags[] = { 1, 2, -1, 0x100 };
        unsigned i;
        fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthB", NULL);
        fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL);
        arr[0] = p[0].p; arr[1] = p[1].p;
        for (i = 0; i < 4; i++) {
            IDWriteFontSet1 *out = NULL;
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts(s1, arr, 2, flags[i], &out));
            if (out) { CHECKF(IDWriteFontSet1_GetFontCount(out) == 3, "flag %d: %u", flags[i], IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gff_zero_props)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        BOOL any;
        for (any = 0; any <= 1; any++) {
            IDWriteFontSet1 *out = NULL;
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts(s1, NULL, 0, any, &out));
            if (out) { CHECKF(IDWriteFontSet1_GetFontCount(out) == 5, "any=%d count %u", any, IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gff_null_props_nonzero_count)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        IDWriteFontSet1 *out = (IDWriteFontSet1 *)FS_SENT;
        HRESULT hr = IDWriteFontSet1_GetFilteredFonts(s1, NULL, 3, TRUE, &out);
        CHECK_HR(hr, E_INVALIDARG);
        CHECK_OUT_NULL(out);
        FS_DROP(out, hr);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gff_single_property_flag_irrelevant)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p; BOOL any;
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_STYLE, "2", NULL);
        for (any = 0; any <= 1; any++) {
            IDWriteFontSet1 *out = NULL;
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts(s1, &p.p, 1, any, &out));
            if (out) { CHECKF(IDWriteFontSet1_GetFontCount(out) == 2, "any=%d count %u", any, IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gff_order_and_distinct)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p[2]; DWRITE_FONT_PROPERTY arr[2]; IDWriteFontSet1 *a = NULL, *b = NULL;
        fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_WEIGHT, "400", NULL);
        fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL);
        arr[0] = p[0].p; arr[1] = p[1].p;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts(s1, arr, 2, TRUE, &a));
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts(s1, arr, 2, TRUE, &b));
        if (a && b) {
            CHECK(a != b);
            CHECK(IDWriteFontSet1_GetFontCount(a) == 5 && IDWriteFontSet1_GetFontCount(b) == 5);
            CHECK(ascending_in_parent(s.set, FS_AS_SET(a)));
            EXPECT_REFS(a, 1);
        }
        DW_RELEASE(a); DW_RELEASE(b);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfi_buffer_protocol)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p; UINT32 buf[16], actual, i;
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "SynthA", NULL);
        /* size 0 / NULL buffer */
        actual = 77;
        HRESULT hr = IDWriteFontSet1_GetFilteredFontIndices(s1, &p.p, 1, FALSE, NULL, 0, &actual);
        CHECK_HR(hr, FS_E_NOTSUFF);
        CHECKF(actual == 4, "actual %u", actual);
        /* exact - 1 */
        for (i = 0; i < 16; i++) buf[i] = 0xCCCCCCCCu;
        actual = 77;
        hr = IDWriteFontSet1_GetFilteredFontIndices(s1, &p.p, 1, FALSE, buf, 3, &actual);
        CHECK_HR(hr, FS_E_NOTSUFF);
        CHECKF(actual == 4, "actual %u", actual);
        CHECK(buf[0] == 0 && buf[1] == 1 && buf[2] == 2);
        CHECKF(buf[3] == 0xCCCCCCCCu, "element past the buffer untouched: 0x%x", buf[3]);
        /* exact */
        for (i = 0; i < 16; i++) buf[i] = 0xCCCCCCCCu;
        actual = 77;
        hr = IDWriteFontSet1_GetFilteredFontIndices(s1, &p.p, 1, FALSE, buf, 4, &actual);
        CHECK_HR_OK(hr);
        CHECK(actual == 4);
        CHECK(buf[0] == 0 && buf[1] == 1 && buf[2] == 2 && buf[3] == 3);
        CHECK(buf[4] == 0xCCCCCCCCu);
        /* larger: the unused part is zero-filled */
        for (i = 0; i < 16; i++) buf[i] = 0xCCCCCCCCu;
        actual = 77;
        hr = IDWriteFontSet1_GetFilteredFontIndices(s1, &p.p, 1, FALSE, buf, 10, &actual);
        CHECK_HR_OK(hr);
        CHECK(actual == 4);
        for (i = 4; i < 10; i++) CHECKF(buf[i] == 0, "buf[%u] = 0x%x", i, buf[i]);
        CHECK(buf[10] == 0xCCCCCCCCu);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfi_buffer_zero_filled_on_failure)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 buf[4] = { 9, 9, 9, 9 }, actual = 99;
        HRESULT hr = IDWriteFontSet1_GetFilteredFontIndices(s1, NULL, 2, FALSE, buf, 4, &actual);
        CHECK_HR(hr, E_INVALIDARG);
        CHECKF(actual == 0, "actual %u", actual);
        CHECKF(buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 0, "buffer cleared before validation");
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfi_no_match)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p; UINT32 buf[4] = { 5, 5, 5, 5 }, actual = 99;
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "Nobody", NULL);
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFontIndices(s1, &p.p, 1, FALSE, buf, 4, &actual));
        CHECK(actual == 0);
        CHECK(buf[0] == 0 && buf[3] == 0);
        actual = 99;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFontIndices(s1, &p.p, 1, FALSE, NULL, 0, &actual));
        CHECK(actual == 0);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfi_zero_props)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 buf[8] = { 0 }, actual = 99, i;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFontIndices(s1, NULL, 0, FALSE, buf, 8, &actual));
        CHECKF(actual == 5, "all-mode, no properties: %u", actual);
        for (i = 0; i < 5; i++) CHECK(buf[i] == i);
        /* any-mode with no properties asks for "at least one match" and so selects nothing */
        actual = 99;
        HRESULT hr = IDWriteFontSet1_GetFilteredFontIndices(s1, NULL, 0, TRUE, buf, 8, &actual);
        CHECK_HR_OK(hr);
        FS_SOFT(actual == 0);
        t_note("any-mode, no properties: %u indices", actual);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfi_matches_filtered_set)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        fs_prop p[2]; DWRITE_FONT_PROPERTY arr[2]; UINT32 buf[8], actual = 0, i; IDWriteFontSet1 *sub = NULL;
        fs_prop_set(&p[0], DWRITE_FONT_PROPERTY_ID_STYLE, "2", NULL);
        fs_prop_set(&p[1], DWRITE_FONT_PROPERTY_ID_WEIGHT, "700", NULL);
        arr[0] = p[0].p; arr[1] = p[1].p;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFontIndices(s1, arr, 2, TRUE, buf, 8, &actual));
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts(s1, arr, 2, TRUE, &sub));
        if (sub) {
            CHECKF(actual == IDWriteFontSet1_GetFontCount(sub), "indices %u vs set %u", actual, IDWriteFontSet1_GetFontCount(sub));
            for (i = 0; i < actual && i < IDWriteFontSet1_GetFontCount(sub); i++)
                CHECKF(parent_index(s.set, FS_AS_SET(sub), i) == buf[i], "item %u: index %u vs %u", i, buf[i], parent_index(s.set, FS_AS_SET(sub), i));
            DW_RELEASE(sub);
        }
        for (i = 1; i < actual; i++) CHECK(buf[i - 1] < buf[i]);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

/* ===== GetFilteredFonts(indices) ============================================================================================ */
TEST(fset_q_gfidx_subset)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 idx[2] = { 1, 3 }; IDWriteFontSet1 *out = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, idx, 2, &out));
        if (out) {
            CHECK(IDWriteFontSet1_GetFontCount(out) == 2);
            CHECK(parent_index(s.set, FS_AS_SET(out), 0) == 1);
            CHECK(parent_index(s.set, FS_AS_SET(out), 1) == 3);
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfidx_order_as_given)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 idx[3] = { 4, 0, 2 }; IDWriteFontSet1 *out = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, idx, 3, &out));
        if (out) {
            CHECK(IDWriteFontSet1_GetFontCount(out) == 3);
            FS_SOFT(parent_index(s.set, FS_AS_SET(out), 0) == 4);
            FS_SOFT(parent_index(s.set, FS_AS_SET(out), 1) == 0);
            FS_SOFT(parent_index(s.set, FS_AS_SET(out), 2) == 2);
            t_note("indices {4,0,2} -> %s", names(FS_AS_SET(out)));
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfidx_out_of_range)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 cases[][2] = { { 5, 0 }, { 0, 5 }, { 0xFFFFFFFFu, 0 }, { 1, 0x7FFFFFFFu }, { 100, 100 } };
        unsigned i;
        for (i = 0; i < 5; i++) {
            IDWriteFontSet1 *out = (IDWriteFontSet1 *)FS_SENT;
            HRESULT hr = IDWriteFontSet1_GetFilteredFonts__(s1, cases[i], 2, &out);
            CHECKF(hr == E_INVALIDARG, "{%u,%u} -> %s", cases[i][0], cases[i][1], dw_hr_name(hr));
            CHECK_OUT_NULL(out);
            FS_DROP(out, hr);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfidx_empty_and_null)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        IDWriteFontSet1 *out = NULL; UINT32 one = 1;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, NULL, 0, &out));
        if (out) { CHECK(IDWriteFontSet1_GetFontCount(out) == 0); DW_RELEASE(out); }
        out = (IDWriteFontSet1 *)FS_SENT;
        HRESULT hr = IDWriteFontSet1_GetFilteredFonts__(s1, NULL, 1, &out);
        CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(out); FS_DROP(out, hr);
        out = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, &one, 0, &out));
        if (out) { CHECK(IDWriteFontSet1_GetFontCount(out) == 0); DW_RELEASE(out); }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfidx_duplicates_note)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 idx[3] = { 2, 2, 2 }; IDWriteFontSet1 *out = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, idx, 3, &out));
        if (out) {
            UINT32 n = IDWriteFontSet1_GetFontCount(out);
            CHECKF(n == 1 || n == 3, "count %u", n);
            t_note("index {2,2,2} -> %u items", n);
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_gfidx_all_indices_is_copy)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 idx[5] = { 0, 1, 2, 3, 4 }, i; IDWriteFontSet1 *out = NULL;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, idx, 5, &out));
        if (out) {
            CHECK(IDWriteFontSet1_GetFontCount(out) == 5);
            for (i = 0; i < 5; i++) CHECKF(parent_index(s.set, FS_AS_SET(out), i) == i, "item %u", i);
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

/* ===== axis range filters ===================================================================================================== */
TEST(fset_q_ranges_zero_ranges)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        IDWriteFontSet1 *all = NULL, *any = NULL;
        /* IsFontItemWithinRanges: no ranges -> "all ranges" is vacuously true, "any range" is false */
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts_(s1, NULL, 0, FALSE, &all));
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts_(s1, NULL, 0, TRUE, &any));
        if (all) { CHECKF(IDWriteFontSet1_GetFontCount(all) == 5, "all-ranges: %u", IDWriteFontSet1_GetFontCount(all)); DW_RELEASE(all); }
        if (any) { CHECKF(IDWriteFontSet1_GetFontCount(any) == 0, "any-range: %u", IDWriteFontSet1_GetFontCount(any)); DW_RELEASE(any); }
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_ranges_null_nonzero)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        IDWriteFontSet1 *out = (IDWriteFontSet1 *)FS_SENT;
        HRESULT hr = IDWriteFontSet1_GetFilteredFonts_(s1, NULL, 1, FALSE, &out);
        CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(out); FS_DROP(out, hr);
        UINT32 buf[4] = { 7, 7, 7, 7 }, actual = 9;
        hr = IDWriteFontSet1_GetFilteredFontIndices_(s1, NULL, 1, FALSE, buf, 4, &actual);
        CHECK_HR(hr, E_INVALIDARG);
        CHECK(actual == 0 && buf[0] == 0 && buf[3] == 0);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_ranges_static_fonts_note)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        DWRITE_FONT_AXIS_RANGE r[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 300.0f, 500.0f } };
        IDWriteFontSet1 *out = NULL; UINT32 buf[8], actual = 0;
        CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts_(s1, r, 1, FALSE, &out));
        if (out) { t_note("wght 300..500 matches %u of 5 static faces", IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        HRESULT hr = IDWriteFontSet1_GetFilteredFontIndices_(s1, r, 1, FALSE, buf, 8, &actual);
        CHECK(hr == S_OK || hr == FS_E_NOTSUFF);
        t_note("GetFilteredFontIndices(ranges) -> %s, %u", dw_hr_name(hr), actual);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_ranges_indices_zero_ranges_all)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        UINT32 buf[8], actual = 99, i;
        HRESULT hr = IDWriteFontSet1_GetFilteredFontIndices_(s1, NULL, 0, FALSE, buf, 8, &actual);
        CHECK_HR_OK(hr);
        CHECKF(actual == 5, "actual %u", actual);
        for (i = 0; i < 5; i++) CHECK(buf[i] == i);
        hr = IDWriteFontSet1_GetFilteredFontIndices_(s1, NULL, 0, FALSE, buf, 2, &actual);
        CHECK_HR(hr, FS_E_NOTSUFF);
        CHECK(actual == 5);
        hr = IDWriteFontSet1_GetFilteredFontIndices_(s1, NULL, 0, FALSE, NULL, 0, &actual);
        CHECK_HR(hr, FS_E_NOTSUFF);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_axis_ranges_whole_set)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        DWRITE_FONT_AXIS_RANGE r[16]; UINT32 actual = 99;
        memset(r, 0xCC, sizeof r);
        HRESULT hr = IDWriteFontSet1_GetFontAxisRanges(s1, r, 16, &actual);
        CHECKF(hr == S_OK, "%s", dw_hr_name(hr));
        CHECK(actual <= 16);
        t_note("static synthetic set reports %u axis ranges", actual);
        if (actual < 16) {
            /* the output array is zero-filled before it is written */
            CHECKF(r[actual].axisTag == 0 && r[15].axisTag == 0, "tail zero-filled");
        }
        actual = 99;
        hr = IDWriteFontSet1_GetFontAxisRanges(s1, NULL, 0, &actual);
        CHECKF(hr == S_OK || hr == FS_E_NOTSUFF, "size 0 -> %s", dw_hr_name(hr));
        CHECK((hr == S_OK) == (actual == 0));
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_axis_ranges_per_font)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set);
    if (s1) {
        DWRITE_FONT_AXIS_RANGE r[16]; UINT32 actual = 99;
        HRESULT hr = IDWriteFontSet1_GetFontAxisRanges_(s1, 0, r, 16, &actual);
        CHECK_HR_OK(hr);
        CHECK(actual <= 16);
        actual = 99;
        hr = IDWriteFontSet1_GetFontAxisRanges_(s1, 5, r, 16, &actual);
        CHECKF(FAILED(hr), "index == count -> %s", dw_hr_name(hr));
        CHECKF(actual == 0, "actual %u", actual);
        t_note("GetFontAxisRanges(index == count) -> %s", dw_hr_name(hr));
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

/* ===== GetFirstFontResources ================================================================================================= */
TEST(fset_q_first_resources_static_set)
{
    FS_STD(s);
    IDWriteFontSet1 *s1 = fs_set1(s.set), *out = (IDWriteFontSet1 *)FS_SENT;
    if (s1) {
        HRESULT hr = IDWriteFontSet1_GetFirstFontResources(s1, &out);
        CHECK_HR_OK(hr);
        CHECK(out != NULL && out != FS_SENT);
        if (SUCCEEDED(hr) && out) {
            /* five different font files: every one is its own font resource */
            CHECKF(IDWriteFontSet1_GetFontCount(out) == 5, "count %u", IDWriteFontSet1_GetFontCount(out));
            CHECK((void *)out != (void *)s1);
            EXPECT_REFS(out, 1);
        }
        FS_DROP(out, hr);
        DW_RELEASE(s1);
    }
    fs_std_close(&s);
}

TEST(fset_q_first_resources_empty_set)
{
    FS_OPEN(x);
    IDWriteFontSetBuilder *b = fs_builder(&x); IDWriteFontSet *e = NULL;
    if (b && SUCCEEDED(IDWriteFontSetBuilder_CreateFontSet(b, &e)) && e) {
        IDWriteFontSet1 *s1 = fs_set1(e), *out = NULL;
        if (s1) {
            CHECK_HR_OK(IDWriteFontSet1_GetFirstFontResources(s1, &out));
            if (out) { CHECK(IDWriteFontSet1_GetFontCount(out) == 0); DW_RELEASE(out); }
            DW_RELEASE(s1);
        }
    }
    DW_RELEASE(e); DW_RELEASE(b);
    fs_close(&x);
}

TEST(fset_q_first_resources_ttc_faces_are_resources)
{
    FS_OPEN(x);
    ttf_spec s1, s2; size_t n1, n2, nt;
    fs_style(&s1, "ResA", 400, 5, 0); fs_style(&s2, "ResB", 400, 5, 0);
    uint8_t *b1 = ttf_build(&s1, &n1), *b2 = ttf_build(&s2, &n2);
    const uint8_t *blobs[2] = { b1, b2 }; size_t sizes[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(blobs, sizes, 2, &nt);
    UINT32 k = fs_add_blob(&x, ttc, nt);
    IDWriteFontSet *set = NULL;
    CHECK_HR_OK(fs_build_keys(&x, &k, 1, &set));
    if (set) {
        IDWriteFontSet1 *a = fs_set1(set), *out = NULL;
        if (a) {
            CHECK_HR_OK(IDWriteFontSet1_GetFirstFontResources(a, &out));
            if (out) { CHECKF(IDWriteFontSet1_GetFontCount(out) == 2, "count %u", IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
            DW_RELEASE(a);
        }
        DW_RELEASE(set);
    }
    free(b1); free(b2); free(ttc);
    fs_close(&x);
}

TEST(fset_q_first_resources_system_set)
{
    NEED_SYSSET(set);
    IDWriteFontSet1 *s1 = fs_set1(set), *out = NULL;
    if (s1) {
        UINT32 n = IDWriteFontSet1_GetFontCount(s1);
        CHECK_HR_OK(IDWriteFontSet1_GetFirstFontResources(s1, &out));
        if (out) {
            UINT32 m = IDWriteFontSet1_GetFontCount(out);
            CHECKF(m > 0 && m <= n, "%u of %u", m, n);
            t_note("first font resources: %u of %u fonts", m, n);
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set);
}

/* ===== system set cross checks ======================================================================================================== */
TEST(fset_q_system_matching_by_weight_partitions)
{
    NEED_SYSSET(set);
    UINT32 total = cnt(set), sum = 0; int w;
    /* every font has exactly one weight value, so the per-weight counts partition the set */
    for (w = 1; w <= 999; w++) {
        char v[8]; snprintf(v, sizeof v, "%d", w);
        UINT32 c = occ(set, DWRITE_FONT_PROPERTY_ID_WEIGHT, v, NULL);
        if (c != 0xDEAD0000u) sum += c;
    }
    CHECKF(sum == total, "sum of per-weight counts %u vs %u fonts", sum, total);
    DW_RELEASE(set);
}

TEST(fset_q_system_style_stretch_partition)
{
    NEED_SYSSET(set);
    UINT32 total = cnt(set), sum = 0; int i;
    for (i = 0; i <= 2; i++) { char v[4]; snprintf(v, sizeof v, "%d", i); sum += occ(set, DWRITE_FONT_PROPERTY_ID_STYLE, v, NULL); }
    CHECKF(sum == total, "style counts %u vs %u", sum, total);
    sum = 0;
    for (i = 1; i <= 9; i++) { char v[4]; snprintf(v, sizeof v, "%d", i); sum += occ(set, DWRITE_FONT_PROPERTY_ID_STRETCH, v, NULL); }
    CHECKF(sum == total, "stretch counts %u vs %u", sum, total);
    DW_RELEASE(set);
}

TEST(fset_q_system_indices_match_count)
{
    NEED_SYSSET(set);
    IDWriteFontSet1 *s1 = fs_set1(set);
    if (s1) {
        fs_prop p; UINT32 actual = 0;
        fs_prop_set(&p, DWRITE_FONT_PROPERTY_ID_WEIGHT, "400", NULL);
        HRESULT hr = IDWriteFontSet1_GetFilteredFontIndices(s1, &p.p, 1, FALSE, NULL, 0, &actual);
        CHECK(hr == FS_E_NOTSUFF || (hr == S_OK && actual == 0));
        CHECKF(actual == occ(set, DWRITE_FONT_PROPERTY_ID_WEIGHT, "400", NULL), "actual %u vs occurrence count", actual);
        UINT32 *buf = (UINT32 *)calloc(actual + 1, sizeof *buf);
        if (buf) {
            UINT32 a2 = 0, i;
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFontIndices(s1, &p.p, 1, FALSE, buf, actual, &a2));
            CHECK(a2 == actual);
            for (i = 1; i < actual; i++) CHECK(buf[i - 1] < buf[i]);
            for (i = 0; i < actual && i < 8; i++) {
                char w[16]; fs_font_prop(set, buf[i], DWRITE_FONT_PROPERTY_ID_WEIGHT, w, sizeof w);
                CHECKF(!strcmp(w, "400"), "index %u weight %s", buf[i], w);
            }
            free(buf);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set);
}
