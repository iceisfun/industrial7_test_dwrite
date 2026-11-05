/* IDWriteFontSet1::GetFontAxisRanges (both overloads), GetFilteredFonts__ (index selection), GetFontLocality of font sets, families and lists
 * (IDWriteFontFamily1 / IDWriteFontList1), IDWriteFontList2 / IDWriteFontFamily2::GetFontSet and IDWriteFontCollection3::GetExpirationEvent.
 *
 * Win10 dwrite.dll:
 *   DWriteFontSet::GetFontAxisRanges (!0x8b6e0 per font, !0xe6840 whole set): *actual = 0 and the output array is zero-filled first, then *actual is set to
 *     the number of ranges (whole set: the union over all fonts, one entry per axis tag in order of first appearance, minimum of the minima and maximum of
 *     the maxima); the result is E_NOT_SUFFICIENT_BUFFER (0x8007007A) when the capacity is below *actual. Failures of the per-font lookup are caught and mapped.
 *   DWriteFontSet::GetFilteredFonts (!0xe62c0, index selection): *out = NULL first; indices == NULL with a non-zero count -> E_INVALIDARG; an index >= the
 *     font count raises ArgumentException("indices") which the catch handler maps to E_INVALIDARG; the result holds the selected fonts in index order.
 *   DWriteFontSet::GetFontLocality (!0xe6920), DWriteFontFamily::GetFontLocality (!0xe2700), DWriteMatchingFontList::GetFontLocality (!0x4ec20): a new
 *     FontFileReference is built from the font's key and FontFileReference::GetLocality is asked, which calls IDWriteRemoteFontFileLoader::GetLocalityFromKey
 *     of the font's loader exactly once per call when the loader supports it, and answers LOCAL otherwise. Nothing is cached.
 *   DWriteFontFamily::GetFontSet (!0xe2970) and DWriteMatchingFontList::GetFontSet (!0xe2a50) always create a NEW DWriteFontSet over the family's / list's fonts.
 *   DWriteFontCollection::GetExpirationEvent (!0xd8b50): NULL without a cache reference, otherwise the event handle stored in it (not duplicated).
 */
#include "g1.h"

typedef struct sx { IDWriteFactory *f; IDWriteFactory3 *f3; dwx_mem_loader *fl; g1_remote *rm; int ok; } sx;
static int sx_open(sx *x)
{
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f || FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory3, (void **)&x->f3)) || !x->f3) { t_skip("IDWriteFactory3 unavailable"); return 0; }
    x->fl = dwx_loader_new();
    if (FAILED(dwx_loader_register(x->fl, x->f))) { t_skip("loader registration failed"); return 0; }
    x->ok = 1;
    return 1;
}
static void sx_close(sx *x)
{
    if (x->rm) { g1_remote_close(x->rm); x->rm = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f3); DW_RELEASE(x->f);
}
#define SX_BEGIN() sx x; if (!sx_open(&x)) { sx_close(&x); return; }
#define SX_END() sx_close(&x)

static UINT32 add_style(sx *x, const char *family, int weight, int italic)
{
    ttf_spec s; size_t n; uint8_t *b; UINT32 k;
    ttf_style_spec(&s, family, weight, 5, italic);
    b = ttf_build(&s, &n); if (!b) return 0xFFFFFFFFu;
    k = dwx_loader_add(x->fl, b, n); free(b);
    return k;
}
static IDWriteFontFile *file_for(sx *x, UINT32 key, int remote)
{
    IDWriteFontFile *file = NULL;
    if (remote) { if (FAILED(IDWriteFactory_CreateCustomFontFileReference(x->f, &key, sizeof key, (IDWriteFontFileLoader *)x->rm, &file))) return NULL; return file; }
    if (FAILED(dwx_loader_file(x->fl, x->f, key, &file))) return NULL;
    return file;
}
static HRESULT build_set(sx *x, const UINT32 *keys, int n, int remote, IDWriteFontSet **out)
{
    IDWriteFontSetBuilder *b = NULL; IDWriteFontSetBuilder1 *b1 = NULL; int i; HRESULT hr;
    *out = NULL;
    if (FAILED(hr = IDWriteFactory3_CreateFontSetBuilder(x->f3, &b)) || !b) return hr ? hr : E_FAIL;
    if (!(b1 = g1_qi((IUnknown *)b, &IID_IDWriteFontSetBuilder1))) { IDWriteFontSetBuilder_Release(b); return E_NOINTERFACE; }
    for (i = 0; i < n; i++) {
        IDWriteFontFile *file = file_for(x, keys[i], remote);
        if (!file) { IDWriteFontSetBuilder1_Release(b1); IDWriteFontSetBuilder_Release(b); return E_FAIL; }
        hr = IDWriteFontSetBuilder1_AddFontFile(b1, file); IDWriteFontFile_Release(file);
        if (FAILED(hr)) t_note("AddFontFile failed: %s", dw_hr_name(hr));
    }
    hr = IDWriteFontSetBuilder_CreateFontSet(b, out);
    IDWriteFontSetBuilder1_Release(b1); IDWriteFontSetBuilder_Release(b);
    return hr;
}
/* the standard static set: SynthA regular, bold, italic, bold italic, SynthB regular */
static int std_set(sx *x, IDWriteFontSet **set, UINT32 keys[5])
{
    HRESULT hr;
    keys[0] = add_style(x, "SynthA", 400, 0); keys[1] = add_style(x, "SynthA", 700, 0); keys[2] = add_style(x, "SynthA", 400, 1);
    keys[3] = add_style(x, "SynthA", 700, 1); keys[4] = add_style(x, "SynthB", 400, 0);
    hr = build_set(x, keys, 5, 0, set);
    if (FAILED(hr) || !*set) { t_skip("font set build failed: %s", dw_hr_name(hr)); return 0; }
    return 1;
}
static IDWriteFontSet1 *as1(IDWriteFontSet *s) { return (IDWriteFontSet1 *)g1_qi((IUnknown *)s, &IID_IDWriteFontSet1); }

/* ===== GetFilteredFonts__ ======================================================================================== */
TEST(gap1_fs_filtered_by_index_empty_selection)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set), *out = G1_SENT(IDWriteFontSet1 *); UINT32 none[1] = { 0 };
        if (s1) {
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, NULL, 0, &out));
            CHECK(out != NULL && out != G1_SENT(IDWriteFontSet1 *));
            if (out && out != G1_SENT(IDWriteFontSet1 *)) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(out), 0); DW_RELEASE(out); }
            out = G1_SENT(IDWriteFontSet1 *);
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, none, 0, &out));
            if (out && out != G1_SENT(IDWriteFontSet1 *)) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(out), 0); DW_RELEASE(out); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_filtered_by_index_null_array_with_count_invalid)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 counts[] = { 1, 2, 5, 100, 0xFFFFFFFFu }; unsigned i;
        for (i = 0; s1 && i < sizeof counts / sizeof counts[0]; i++) {
            IDWriteFontSet1 *out = G1_SENT(IDWriteFontSet1 *); HRESULT hr = IDWriteFontSet1_GetFilteredFonts__(s1, NULL, counts[i], &out);
            CHECKF(hr == E_INVALIDARG, "count %u -> %s", counts[i], dw_hr_name(hr));
            CHECKF(out == NULL, "count %u: out parameter not zeroed", counts[i]);
            if (out == G1_SENT(IDWriteFontSet1 *)) out = NULL;
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_filtered_by_index_out_of_range_invalid)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); UINT32 n = s1 ? IDWriteFontSet1_GetFontCount(s1) : 0;
        struct { UINT32 idx[3]; UINT32 cnt; const char *what; } rows[] = {
            { { 5, 0, 0 }, 1, "index == count" }, { { 6, 0, 0 }, 1, "index == count + 1" }, { { 0xFFFFFFFFu, 0, 0 }, 1, "index 0xFFFFFFFF" }, { { 0x80000000u, 0, 0 }, 1, "index 0x80000000" },
            { { 0, 1, 5 }, 3, "valid then invalid" }, { { 5, 0, 1 }, 3, "invalid then valid" }, { { 0, 4, 100 }, 3, "last invalid" },
        };
        unsigned i;
        if (n != 5 || t_is_wine()) { DW_RELEASE(s1); DW_RELEASE(set); SX_END(); t_skip("fixture mismatch or emulator"); return; }
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
            IDWriteFontSet1 *out = G1_SENT(IDWriteFontSet1 *); HRESULT hr = IDWriteFontSet1_GetFilteredFonts__(s1, rows[i].idx, rows[i].cnt, &out);
            CHECKF(hr == E_INVALIDARG, "%s -> %s", rows[i].what, dw_hr_name(hr));
            CHECKF(out == NULL, "%s: out parameter not zeroed", rows[i].what);
            if (out == G1_SENT(IDWriteFontSet1 *)) out = NULL;
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
static int index_in_parent(IDWriteFontSet *parent, IDWriteFontSet *sub, UINT32 i)
{
    IDWriteFontFaceReference *r = NULL; UINT32 idx = 0xFFFFFFFFu; BOOL ex = FALSE;
    if (FAILED(IDWriteFontSet_GetFontFaceReference(sub, i, &r)) || !r) return -1;
    IDWriteFontSet_FindFontFaceReference(parent, r, &idx, &ex);
    DW_RELEASE(r);
    return ex ? (int)idx : -1;
}
TEST(gap1_fs_filtered_by_index_selection_order)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 sel[] = { 4, 0, 2 }; IDWriteFontSet1 *out = NULL;
        if (s1) {
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, sel, 3, &out));
            if (out) {
                IDWriteFontSet *os = (IDWriteFontSet *)out; UINT32 i;
                EXPECT_EQ(IDWriteFontSet_GetFontCount(os), 3);
                for (i = 0; i < 3 && i < IDWriteFontSet_GetFontCount(os); i++) { int p = index_in_parent(set, os, i); CHECKF(p == (int)sel[i], "result %u maps to parent %d, want %u", i, p, sel[i]); }
                DW_RELEASE(out);
            }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_filtered_by_index_every_single_index)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); UINT32 i, n = s1 ? IDWriteFontSet1_GetFontCount(s1) : 0;
        for (i = 0; i < n; i++) {
            IDWriteFontSet1 *out = NULL; HRESULT hr = IDWriteFontSet1_GetFilteredFonts__(s1, &i, 1, &out);
            CHECK_HR(hr, S_OK);
            if (out) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(out), 1); CHECKF(index_in_parent(set, (IDWriteFontSet *)out, 0) == (int)i, "single index %u", i); DW_RELEASE(out); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_filtered_by_index_whole_set_in_order)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 all[] = { 0, 1, 2, 3, 4 }; IDWriteFontSet1 *out = NULL;
        if (s1) {
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, all, 5, &out));
            if (out) { UINT32 i; EXPECT_EQ(IDWriteFontSet1_GetFontCount(out), 5); for (i = 0; i < 5; i++) CHECKF(index_in_parent(set, (IDWriteFontSet *)out, i) == (int)i, "item %u", i); DW_RELEASE(out); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_filtered_by_index_duplicates_note)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 dup[] = { 1, 1, 1, 3 }; IDWriteFontSet1 *out = NULL;
        if (s1) {
            HRESULT hr = IDWriteFontSet1_GetFilteredFonts__(s1, dup, 4, &out);
            CHECK_HR(hr, S_OK);
            if (out) { t_note("indices {1,1,1,3} select %u fonts", IDWriteFontSet1_GetFontCount(out)); DW_RELEASE(out); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_filtered_by_index_result_independent_of_parent)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 sel[] = { 3, 1 }; IDWriteFontSet1 *out = NULL;
        if (s1 && SUCCEEDED(IDWriteFontSet1_GetFilteredFonts__(s1, sel, 2, &out)) && out) {
            DW_RELEASE(s1); DW_RELEASE(set);
            EXPECT_EQ(IDWriteFontSet1_GetFontCount(out), 2);
            { IDWriteFontFaceReference *r = NULL; CHECK_HR_OK(IDWriteFontSet1_GetFontFaceReference(out, 0, &r)); DW_RELEASE(r); }
            DW_RELEASE(out);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_filtered_by_index_new_object_each_call_and_chains)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 sel[] = { 0, 2, 4 }, sel2[] = { 2, 0 }; IDWriteFontSet1 *a = NULL, *b = NULL, *c = NULL;
        if (s1) {
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, sel, 3, &a)); CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(s1, sel, 3, &b));
            CHECK(a && b && a != b);
            if (a) { EXPECT_REFS(a, 1); CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(a, sel2, 2, &c)); }
            if (c) {
                EXPECT_EQ(IDWriteFontSet1_GetFontCount(c), 2);
                /* indices refer to the filtered set: item 0 of c is item 2 of a (parent index 4), item 1 is parent index 0 */
                CHECK(index_in_parent(set, (IDWriteFontSet *)c, 0) == 4 && index_in_parent(set, (IDWriteFontSet *)c, 1) == 0);
            }
            DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_filtered_by_index_on_a_filtered_set_range_check)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 sel[] = { 0, 2 }, bad[] = { 2 }; IDWriteFontSet1 *a = NULL, *out = G1_SENT(IDWriteFontSet1 *);
        if (s1 && !t_is_wine() && SUCCEEDED(IDWriteFontSet1_GetFilteredFonts__(s1, sel, 2, &a)) && a) {
            HRESULT hr = IDWriteFontSet1_GetFilteredFonts__(a, bad, 1, &out);
            CHECKF(hr == E_INVALIDARG, "index 2 of a 2 font set -> %s", dw_hr_name(hr));       /* valid in the parent, not in the filtered set */
            CHECK_OUT_NULL(out); if (out == G1_SENT(IDWriteFontSet1 *)) out = NULL;
            DW_RELEASE(out);
        }
        DW_RELEASE(a); DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}

/* ===== GetFontAxisRanges ======================================================================================== */
static int find_tag(const DWRITE_FONT_AXIS_RANGE *r, UINT32 n, UINT32 tag) { UINT32 i; for (i = 0; i < n; i++) if (r[i].axisTag == tag) return (int)i; return -1; }
#define TAG_WGHT G1_TAG('w', 'g', 'h', 't')
#define TAG_XTRA G1_TAG('X', 'T', 'R', 'A')
#define TAG_FIXD G1_TAG('F', 'I', 'X', 'D')
static UINT32 add_var(sx *x, const char *family, float wmn, float wdf, float wmx, float emn, float edf, float emx, int third)
{
    g1_axis ax[3] = { { "wght", 0, 0, 0, 256, "Weight" }, { "XTRA", 0, 0, 0, 257, "Extra Axis" }, { "FIXD", 5, 5, 5, 258, "Fixed" } };
    size_t n; uint8_t *b; UINT32 k;
    ax[0].mn = wmn; ax[0].df = wdf; ax[0].mx = wmx; ax[1].mn = emn; ax[1].df = edf; ax[1].mx = emx;
    b = g1_build_var(family, ax, third ? 3 : 2, NULL, 0, NULL, 0, &n); if (!b) return 0xFFFFFFFFu;
    k = dwx_loader_add(x->fl, b, n); free(b);
    return k;
}
TEST(gap1_fs_axis_ranges_union_of_variable_fonts)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2];
    keys[0] = add_var(&x, "VarOne", 100, 400, 900, 10, 50, 90, 0); keys[1] = add_var(&x, "VarTwo", 300, 400, 500, 40, 50, 60, 0);
    if (FAILED(build_set(&x, keys, 2, 0, &set)) || !set) { SX_END(); t_skip("set build failed"); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); DWRITE_FONT_AXIS_RANGE r[32]; UINT32 actual = 0xDEAD; int w, e;
        if (s1) {
            memset(r, 0xCC, sizeof r);
            CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges(s1, r, 32, &actual));
            CHECK(actual >= 2 && actual <= 32);
            w = find_tag(r, actual, TAG_WGHT); e = find_tag(r, actual, TAG_XTRA);
            CHECK(w >= 0 && e >= 0);
            if (w >= 0) { CHECK_FLOAT_EQ(r[w].minValue, 100.0f); CHECK_FLOAT_EQ(r[w].maxValue, 900.0f); }
            if (e >= 0) { CHECK_FLOAT_EQ(r[e].minValue, 10.0f); CHECK_FLOAT_EQ(r[e].maxValue, 90.0f); }
            { UINT32 i, j; for (i = 0; i < actual; i++) for (j = i + 1; j < actual; j++) CHECKF(r[i].axisTag != r[j].axisTag, "tag %08x reported twice", (unsigned)r[i].axisTag); }
            { UINT32 i; for (i = actual; i < 32; i++) CHECKF(r[i].axisTag == 0 && r[i].minValue == 0.0f && r[i].maxValue == 0.0f, "entry %u beyond the count is not zero", i); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_axis_ranges_union_takes_extremes_from_both_fonts)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[3];
    keys[0] = add_var(&x, "VarA", 300, 400, 500, 40, 50, 60, 0); keys[1] = add_var(&x, "VarB", 100, 400, 450, 55, 58, 95, 1); keys[2] = add_var(&x, "VarC", 350, 400, 900, 5, 50, 70, 0);
    if (FAILED(build_set(&x, keys, 3, 0, &set)) || !set) { SX_END(); t_skip("set build failed"); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); DWRITE_FONT_AXIS_RANGE r[32]; UINT32 actual = 0; int w, e, f;
        if (s1) {
            CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges(s1, r, 32, &actual));
            w = find_tag(r, actual, TAG_WGHT); e = find_tag(r, actual, TAG_XTRA); f = find_tag(r, actual, TAG_FIXD);
            CHECK(w >= 0 && e >= 0 && f >= 0);
            if (w >= 0) { CHECK_FLOAT_EQ(r[w].minValue, 100.0f); CHECK_FLOAT_EQ(r[w].maxValue, 900.0f); }
            if (e >= 0) { CHECK_FLOAT_EQ(r[e].minValue, 5.0f); CHECK_FLOAT_EQ(r[e].maxValue, 95.0f); }
            if (f >= 0) { CHECK_FLOAT_EQ(r[f].minValue, 5.0f); CHECK_FLOAT_EQ(r[f].maxValue, 5.0f); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_axis_ranges_buffer_protocol)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2];
    keys[0] = add_var(&x, "VarProtoA", 100, 400, 900, 10, 50, 90, 1); keys[1] = add_var(&x, "VarProtoB", 100, 400, 900, 10, 50, 90, 0);
    if (FAILED(build_set(&x, keys, 2, 0, &set)) || !set) { SX_END(); t_skip("set build failed"); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); DWRITE_FONT_AXIS_RANGE r[32]; UINT32 actual = 0xDEAD, total = 0, cap;
        if (s1) {
            CHECK_HR(IDWriteFontSet1_GetFontAxisRanges(s1, NULL, 0, &actual), 0x8007007A);       /* size query */
            total = actual;
            CHECK(total >= 3);
            memset(r, 0xCC, sizeof r);
            CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges(s1, r, total, &actual)); EXPECT_EQ(actual, total);
            for (cap = 0; cap < total; cap++) {
                memset(r, 0xCC, sizeof r); actual = 0xDEAD;
                CHECK_HR(IDWriteFontSet1_GetFontAxisRanges(s1, r, cap, &actual), 0x8007007A);
                EXPECT_EQ(actual, total);
                { UINT32 i; for (i = cap; i < 32; i++) { uint8_t *p = (uint8_t *)&r[i]; unsigned k; int same = 1; for (k = 0; k < sizeof r[i]; k++) if (p[k] != 0xCC) same = 0; CHECKF(same, "entry %u beyond the capacity %u was touched", i, cap); } }
            }
            memset(r, 0xCC, sizeof r); actual = 0xDEAD;
            CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges(s1, r, 32, &actual)); EXPECT_EQ(actual, total);
            { UINT32 i; for (i = total; i < 32; i++) CHECKF(r[i].axisTag == 0 && r[i].minValue == 0 && r[i].maxValue == 0, "entry %u not zero-filled", i); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_axis_ranges_actual_and_buffer_zeroed_first)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[1];
    keys[0] = add_var(&x, "VarZero", 100, 400, 900, 10, 50, 90, 0);
    if (FAILED(build_set(&x, keys, 1, 0, &set)) || !set) { SX_END(); t_skip("set build failed"); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); DWRITE_FONT_AXIS_RANGE r[16]; UINT32 actual = 0x7777;
        if (s1) {
            memset(r, 0x5A, sizeof r);
            CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges(s1, r, 16, &actual));
            CHECK(actual < 16);
            { UINT32 i; for (i = actual; i < 16; i++) CHECK(r[i].axisTag == 0 && r[i].minValue == 0 && r[i].maxValue == 0); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_axis_ranges_per_font_overload_matches_the_font)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2];
    keys[0] = add_var(&x, "VarPerA", 100, 400, 900, 10, 50, 90, 1); keys[1] = add_var(&x, "VarPerB", 300, 400, 500, 40, 50, 60, 0);
    if (FAILED(build_set(&x, keys, 2, 0, &set)) || !set) { SX_END(); t_skip("set build failed"); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); UINT32 n = s1 ? IDWriteFontSet1_GetFontCount(s1) : 0, i;
        for (i = 0; i < n; i++) {
            DWRITE_FONT_AXIS_RANGE r[16]; UINT32 actual = 0xDEAD; IDWriteFontResource *res = NULL; DWRITE_FONT_AXIS_RANGE rr[16]; UINT32 na;
            memset(r, 0xCC, sizeof r);
            CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges_(s1, i, r, 16, &actual));
            CHECK(actual >= 2 && actual <= 16);
            { UINT32 k; for (k = actual; k < 16; k++) CHECK(r[k].axisTag == 0 && r[k].maxValue == 0); }
            { UINT32 a, b; for (a = 0; a < actual; a++) for (b = a + 1; b < actual; b++) CHECK(r[a].axisTag != r[b].axisTag); }
            if (SUCCEEDED(IDWriteFontSet1_CreateFontResource(s1, i, &res)) && res) {
                na = IDWriteFontResource_GetFontAxisCount(res);
                if (na == actual && na <= 16 && SUCCEEDED(IDWriteFontResource_GetFontAxisRanges(res, rr, na))) {
                    UINT32 k;
                    for (k = 0; k < na; k++) { int j = find_tag(r, actual, rr[k].axisTag); CHECKF(j >= 0 && r[j].minValue == rr[k].minValue && r[j].maxValue == rr[k].maxValue, "font %u axis %08x differs from its resource", i, (unsigned)rr[k].axisTag); }
                } else t_note("font %u: set reports %u ranges, resource %u axes", i, actual, na);
                DW_RELEASE(res);
            }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_axis_ranges_per_font_buffer_protocol)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[1];
    keys[0] = add_var(&x, "VarPerProto", 100, 400, 900, 10, 50, 90, 1);
    if (FAILED(build_set(&x, keys, 1, 0, &set)) || !set) { SX_END(); t_skip("set build failed"); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); DWRITE_FONT_AXIS_RANGE r[16]; UINT32 actual = 0xDEAD, total, cap;
        if (s1) {
            CHECK_HR(IDWriteFontSet1_GetFontAxisRanges_(s1, 0, NULL, 0, &actual), 0x8007007A); total = actual; CHECK(total >= 3);
            for (cap = 0; cap < total && cap < 16; cap++) {
                memset(r, 0xCC, sizeof r); actual = 0xDEAD;
                CHECK_HR(IDWriteFontSet1_GetFontAxisRanges_(s1, 0, r, cap, &actual), 0x8007007A);
                EXPECT_EQ(actual, total);
            }
            actual = 0xDEAD;
            CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges_(s1, 0, r, 16, &actual)); EXPECT_EQ(actual, total);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_axis_ranges_per_font_index_out_of_range)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (t_is_wine()) { DW_RELEASE(set); SX_END(); t_skip("emulator does not validate the index"); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 bad[] = { 5, 6, 1000, 0xFFFFFFFFu }; unsigned i;
        for (i = 0; s1 && i < sizeof bad / sizeof bad[0]; i++) {
            DWRITE_FONT_AXIS_RANGE r[4]; UINT32 actual = 0xDEAD; HRESULT hr; int k;
            memset(r, 0xCC, sizeof r);
            hr = IDWriteFontSet1_GetFontAxisRanges_(s1, bad[i], r, 4, &actual);
            CHECKF(FAILED(hr), "index %u -> %s", bad[i], dw_hr_name(hr));
            t_note("GetFontAxisRanges(index %u) -> %s", bad[i], dw_hr_name(hr));
            EXPECT_EQ(actual, 0);
            for (k = 0; k < 4; k++) CHECKF(r[k].axisTag == 0 && r[k].minValue == 0 && r[k].maxValue == 0, "entry %d not zeroed", k);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fs_axis_ranges_static_fonts_note)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); DWRITE_FONT_AXIS_RANGE r[32]; UINT32 actual = 0, i;
        if (s1) {
            CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges(s1, r, 32, &actual));
            for (i = 0; i < actual; i++) { t_note("static set axis %08x [%g, %g]", (unsigned)r[i].axisTag, r[i].minValue, r[i].maxValue); CHECK(r[i].minValue <= r[i].maxValue); }
            for (i = 0; i < actual; i++) { UINT32 j; for (j = i + 1; j < actual; j++) CHECK(r[i].axisTag != r[j].axisTag); }
            for (i = 0; i < 5; i++) { UINT32 a = 0; CHECK_HR_OK(IDWriteFontSet1_GetFontAxisRanges_(s1, i, r, 32, &a)); t_note("static font %u reports %u ranges", i, a); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}

/* ===== locality ================================================================================================= */
static const DWRITE_LOCALITY LOCS[] = { DWRITE_LOCALITY_REMOTE, DWRITE_LOCALITY_PARTIAL, DWRITE_LOCALITY_LOCAL, DWRITE_LOCALITY_PARTIAL, DWRITE_LOCALITY_REMOTE, DWRITE_LOCALITY_LOCAL };
/* Set of two fonts through a remote-flavoured loader (answers LOCAL while the set is built). */
static int remote_set(sx *x, IDWriteFontSet **set, UINT32 keys[2])
{
    HRESULT hr;
    x->rm = g1_remote_new(x->fl, DWRITE_LOCALITY_LOCAL);
    if (FAILED(hr = g1_remote_register(x->rm, x->f))) { t_skip("remote loader registration -> %s", dw_hr_name(hr)); return 0; }
    keys[0] = add_style(x, "RemA", 400, 0); keys[1] = add_style(x, "RemA", 700, 0);
    hr = build_set(x, keys, 2, 1, set);
    if (FAILED(hr) || !*set) { t_skip("set over the remote loader -> %s", dw_hr_name(hr)); return 0; }
    return 1;
}
TEST(gap1_fl_set_locality_follows_the_loader_each_call)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2];
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); unsigned i; UINT32 n = s1 ? IDWriteFontSet1_GetFontCount(s1) : 0, f;
        EXPECT_EQ(n, 2);
        for (i = 0; s1 && i < sizeof LOCS / sizeof LOCS[0]; i++) for (f = 0; f < n; f++) {
            LONG before; DWRITE_LOCALITY got;
            x.rm->locality = LOCS[i]; before = x.rm->n_locality;
            got = IDWriteFontSet1_GetFontLocality(s1, f);
            CHECKF(got == LOCS[i], "step %u font %u: want %d got %d", i, f, (int)LOCS[i], (int)got);
            CHECKF(x.rm->n_locality == before + 1, "step %u font %u: loader asked %ld times", i, f, x.rm->n_locality - before);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_set_locality_passes_key_to_loader)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2];
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set);
        if (s1 && IDWriteFontSet1_GetFontCount(s1) == 2) {
            UINT32 a, b;
            IDWriteFontSet1_GetFontLocality(s1, 0); a = x.rm->last_key; EXPECT_EQ(x.rm->last_key_size, 4);
            IDWriteFontSet1_GetFontLocality(s1, 1); b = x.rm->last_key;
            CHECK(a != b);
            CHECK((a == keys[0] && b == keys[1]) || (a == keys[1] && b == keys[0]));
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_set_locality_value_not_validated)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2];
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set);
        if (s1) { x.rm->locality = (DWRITE_LOCALITY)7; EXPECT_EQ((int)IDWriteFontSet1_GetFontLocality(s1, 0), 7); x.rm->locality = (DWRITE_LOCALITY)-1; EXPECT_EQ((int)IDWriteFontSet1_GetFontLocality(s1, 0), -1); }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_set_locality_custom_loader_is_local)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); UINT32 i, n = s1 ? IDWriteFontSet1_GetFontCount(s1) : 0;
        for (i = 0; i < n; i++) EXPECT_EQ((int)IDWriteFontSet1_GetFontLocality(s1, i), (int)DWRITE_LOCALITY_LOCAL);
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_set_locality_matches_reference_locality)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2];
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); unsigned i; UINT32 f;
        for (i = 0; s1 && i < sizeof LOCS / sizeof LOCS[0]; i++) for (f = 0; f < 2; f++) {
            IDWriteFontFaceReference *r = NULL;
            x.rm->locality = LOCS[i];
            if (SUCCEEDED(IDWriteFontSet1_GetFontFaceReference(s1, f, &r)) && r) { EXPECT_EQ((int)IDWriteFontFaceReference_GetLocality(r), (int)IDWriteFontSet1_GetFontLocality(s1, f)); DW_RELEASE(r); }
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_set_locality_unchanged_after_set_copy_and_filter)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2];
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    {
        IDWriteFontSet1 *s1 = as1(set); static const UINT32 sel[] = { 1 }; IDWriteFontSet1 *sub = NULL;
        if (s1 && SUCCEEDED(IDWriteFontSet1_GetFilteredFonts__(s1, sel, 1, &sub)) && sub) {
            x.rm->locality = DWRITE_LOCALITY_PARTIAL; EXPECT_EQ((int)IDWriteFontSet1_GetFontLocality(sub, 0), (int)DWRITE_LOCALITY_PARTIAL);
            x.rm->locality = DWRITE_LOCALITY_REMOTE;  EXPECT_EQ((int)IDWriteFontSet1_GetFontLocality(sub, 0), (int)DWRITE_LOCALITY_REMOTE);
            DW_RELEASE(sub);
        }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_set_locality_out_of_range_index_does_not_crash_note)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (t_is_wine()) { DW_RELEASE(set); SX_END(); t_skip("emulator does not validate the index"); return; }
    {
        IDWriteFontSet1 *s1 = as1(set);
        if (s1) { int v = (int)IDWriteFontSet1_GetFontLocality(s1, 5); t_note("GetFontLocality(index == count) -> %d", v); v = (int)IDWriteFontSet1_GetFontLocality(s1, 0xFFFFFFFFu); t_note("GetFontLocality(0xFFFFFFFF) -> %d", v); }
        DW_RELEASE(s1);
    }
    DW_RELEASE(set); SX_END();
}

/* families and lists over a collection built from the remote set */
typedef struct cx { IDWriteFontCollection1 *coll; IDWriteFontFamily *fam; IDWriteFontFamily1 *fam1; IDWriteFontList *match; IDWriteFontList1 *list1; } cx;
static int cx_open(sx *x, IDWriteFontSet *set, cx *c)
{
    HRESULT hr;
    memset(c, 0, sizeof *c);
    hr = IDWriteFactory3_CreateFontCollectionFromFontSet(x->f3, set, &c->coll);
    if (FAILED(hr) || !c->coll) { t_skip("CreateFontCollectionFromFontSet -> %s", dw_hr_name(hr)); return 0; }
    if (IDWriteFontCollection1_GetFontFamilyCount(c->coll) < 1 || FAILED(IDWriteFontCollection1_GetFontFamily(c->coll, 0, &c->fam)) || !c->fam) { t_skip("no family"); return 0; }
    c->fam1 = g1_qi((IUnknown *)c->fam, &IID_IDWriteFontFamily1);
    IDWriteFontFamily_GetMatchingFonts(c->fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &c->match);
    if (c->match) c->list1 = g1_qi((IUnknown *)c->match, &IID_IDWriteFontList1);
    return 1;
}
static void cx_close(cx *c) { DW_RELEASE(c->list1); DW_RELEASE(c->match); DW_RELEASE(c->fam1); DW_RELEASE(c->fam); DW_RELEASE(c->coll); }
TEST(gap1_fl_family_locality_follows_the_loader_each_call)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2]; cx c;
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    if (cx_open(&x, set, &c) && c.fam1) {
        UINT32 n = IDWriteFontFamily_GetFontCount(c.fam), f; unsigned i;
        CHECK(n >= 2);          /* Win10 lists simulated faces too */
        for (i = 0; i < sizeof LOCS / sizeof LOCS[0]; i++) for (f = 0; f < n; f++) {
            LONG before; DWRITE_LOCALITY got;
            x.rm->locality = LOCS[i]; before = x.rm->n_locality;
            got = IDWriteFontFamily1_GetFontLocality(c.fam1, f);
            CHECKF(got == LOCS[i], "step %u font %u: want %d got %d", i, f, (int)LOCS[i], (int)got);
            CHECKF(x.rm->n_locality == before + 1, "step %u font %u: loader asked %ld times", i, f, x.rm->n_locality - before);
        }
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_list_locality_follows_the_loader_each_call)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2]; cx c;
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    if (cx_open(&x, set, &c) && c.list1) {
        UINT32 n = IDWriteFontList_GetFontCount(c.match), f; unsigned i;
        CHECK(n >= 1);
        for (i = 0; i < sizeof LOCS / sizeof LOCS[0]; i++) for (f = 0; f < n; f++) {
            LONG before; DWRITE_LOCALITY got;
            x.rm->locality = LOCS[i]; before = x.rm->n_locality;
            got = IDWriteFontList1_GetFontLocality(c.list1, f);
            CHECKF(got == LOCS[i], "step %u font %u: want %d got %d", i, f, (int)LOCS[i], (int)got);
            CHECKF(x.rm->n_locality == before + 1, "step %u font %u: loader asked %ld times", i, f, x.rm->n_locality - before);
        }
    } else if (c.coll) t_skip("matching list has no IDWriteFontList1");
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_family_and_list_and_set_agree)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2]; cx c;
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    if (cx_open(&x, set, &c) && c.fam1) {
        IDWriteFontSet1 *s1 = as1(set); unsigned i;
        for (i = 0; s1 && i < sizeof LOCS / sizeof LOCS[0]; i++) {
            UINT32 f; x.rm->locality = LOCS[i];
            for (f = 0; f < 2; f++) EXPECT_EQ((int)IDWriteFontFamily1_GetFontLocality(c.fam1, f), (int)IDWriteFontSet1_GetFontLocality(s1, 0));      /* one locality for all of them */
        }
        DW_RELEASE(s1);
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_font3_reference_locality_matches_family)
{
    SX_BEGIN(); IDWriteFontSet *set = NULL; UINT32 keys[2]; cx c;
    if (!remote_set(&x, &set, keys)) { DW_RELEASE(set); SX_END(); return; }
    if (cx_open(&x, set, &c) && c.fam1) {
        unsigned i;
        for (i = 0; i < sizeof LOCS / sizeof LOCS[0]; i++) {
            IDWriteFont *fo = NULL; IDWriteFont3 *f3 = NULL; x.rm->locality = LOCS[i];
            if (SUCCEEDED(IDWriteFontFamily1_GetFont(c.fam1, 0, &fo)) && fo && (f3 = g1_qi((IUnknown *)fo, &IID_IDWriteFont3)) != NULL) EXPECT_EQ((int)IDWriteFont3_GetLocality(f3), (int)IDWriteFontFamily1_GetFontLocality(c.fam1, 0));
            DW_RELEASE(f3); DW_RELEASE(fo);
        }
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_custom_loader_family_and_list_are_local)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5]; cx c;
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (cx_open(&x, set, &c)) {
        UINT32 f, n = IDWriteFontFamily_GetFontCount(c.fam);
        for (f = 0; c.fam1 && f < n; f++) EXPECT_EQ((int)IDWriteFontFamily1_GetFontLocality(c.fam1, f), (int)DWRITE_LOCALITY_LOCAL);
        for (f = 0; c.list1 && f < IDWriteFontList_GetFontCount(c.match); f++) EXPECT_EQ((int)IDWriteFontList1_GetFontLocality(c.list1, f), (int)DWRITE_LOCALITY_LOCAL);
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fl_system_fonts_are_local)
{
    IDWriteFactory *f = dw_shared(); static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Segoe UI", L"Courier New", L"Tahoma" }; unsigned i; int seen = 0;
    for (i = 0; f && i < sizeof fams / sizeof fams[0]; i++) {
        IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFontFamily1 *fam1; UINT32 idx = 0, n, k; BOOL ex = FALSE;
        if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) continue;
        IDWriteFontCollection_FindFamilyName(c, fams[i], &idx, &ex);
        if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam && (fam1 = g1_qi((IUnknown *)fam, &IID_IDWriteFontFamily1)) != NULL) {
            seen++;
            n = IDWriteFontFamily_GetFontCount(fam);
            for (k = 0; k < n; k++) CHECKF(IDWriteFontFamily1_GetFontLocality(fam1, k) == DWRITE_LOCALITY_LOCAL, "%ls font %u", fams[i], k);
            DW_RELEASE(fam1);
        }
        DW_RELEASE(fam); DW_RELEASE(c);
    }
    if (!seen) t_skip("none of the sample families is installed");
}
TEST(gap1_fl_system_set_localities_are_known_values)
{
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *set = NULL; IDWriteFontSet1 *s1;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    if (FAILED(IDWriteFactory3_GetSystemFontSet(f3, &set)) || !set) { t_skip("no system font set"); return; }
    s1 = as1(set);
    if (s1) {
        UINT32 n = IDWriteFontSet1_GetFontCount(s1), i, step = n > 200 ? n / 100 : 1, local = 0, other = 0;
        for (i = 0; i < n; i += step) { DWRITE_LOCALITY l = IDWriteFontSet1_GetFontLocality(s1, i); CHECKF(l == DWRITE_LOCALITY_REMOTE || l == DWRITE_LOCALITY_PARTIAL || l == DWRITE_LOCALITY_LOCAL, "font %u: locality %d", i, (int)l); if (l == DWRITE_LOCALITY_LOCAL) local++; else other++; }
        t_note("system font set: %u fonts sampled with step %u, %u local, %u not local", n, step, local, other);
        CHECK(local > 0);
        DW_RELEASE(s1);
    }
    DW_RELEASE(set);
}

/* ===== GetFontSet of families and lists ========================================================================== */
static int ref_present(IDWriteFontSet *set, IDWriteFontFaceReference *r)
{
    UINT32 i, n = IDWriteFontSet_GetFontCount(set);
    for (i = 0; i < n; i++) { IDWriteFontFaceReference *o = NULL; int eq = 0; if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(set, i, &o)) && o) { eq = IDWriteFontFaceReference_Equals(o, r); DW_RELEASE(o); } if (eq) return 1; }
    return 0;
}
TEST(gap1_fg_family_font_set_has_the_family_fonts)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5]; cx c;
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (cx_open(&x, set, &c)) {
        IDWriteFontFamily2 *f2 = g1_qi((IUnknown *)c.fam, &IID_IDWriteFontFamily2);
        if (!f2) t_skip("IDWriteFontFamily2 unsupported");
        else {
            UINT32 nf = IDWriteFontCollection1_GetFontFamilyCount(c.coll), fi;
            for (fi = 0; fi < nf; fi++) {
                IDWriteFontFamily *fam = NULL; IDWriteFontFamily2 *g2 = NULL; IDWriteFontSet1 *fs = G1_SENT(IDWriteFontSet1 *);
                if (FAILED(IDWriteFontCollection1_GetFontFamily(c.coll, fi, &fam)) || !fam) continue;
                g2 = g1_qi((IUnknown *)fam, &IID_IDWriteFontFamily2);
                if (g2) {
                    UINT32 n = IDWriteFontFamily_GetFontCount(fam), k;
                    CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(g2, &fs));
                    if (fs && fs != G1_SENT(IDWriteFontSet1 *)) {
                        EXPECT_EQ(IDWriteFontSet1_GetFontCount(fs), n);
                        for (k = 0; k < n; k++) {
                            IDWriteFont *fo = NULL; IDWriteFont3 *f3 = NULL; IDWriteFontFaceReference *r = NULL;
                            if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &fo)) && fo && (f3 = g1_qi((IUnknown *)fo, &IID_IDWriteFont3)) != NULL) {
                                if (SUCCEEDED(IDWriteFont3_GetFontFaceReference(f3, &r)) && r) { CHECKF(ref_present((IDWriteFontSet *)fs, r), "family %u font %u missing in its font set", fi, k); DW_RELEASE(r); }
                            }
                            DW_RELEASE(f3); DW_RELEASE(fo);
                        }
                        DW_RELEASE(fs);
                    }
                }
                DW_RELEASE(g2); DW_RELEASE(fam);
            }
            DW_RELEASE(f2);
        }
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fg_family_font_set_new_object_each_call_and_outlives_family)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5]; cx c;
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (cx_open(&x, set, &c)) {
        IDWriteFontFamily2 *f2 = g1_qi((IUnknown *)c.fam, &IID_IDWriteFontFamily2); IDWriteFontSet1 *a = NULL, *b = NULL;
        if (f2) {
            UINT32 n = IDWriteFontFamily_GetFontCount(c.fam);
            CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(f2, &a)); CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(f2, &b));
            CHECK(a && b && a != b);
            if (a) EXPECT_REFS(a, 1);
            DW_RELEASE(f2); DW_RELEASE(c.fam1); DW_RELEASE(c.fam); DW_RELEASE(c.list1); DW_RELEASE(c.match); DW_RELEASE(c.coll);
            if (a) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(a), n); { IDWriteFontFaceReference *r = NULL; CHECK_HR_OK(IDWriteFontSet1_GetFontFaceReference(a, 0, &r)); DW_RELEASE(r); } }
            DW_RELEASE(a); DW_RELEASE(b);
        } else t_skip("IDWriteFontFamily2 unsupported");
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fg_collection_font_set_covers_all_families)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5]; cx c;
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (cx_open(&x, set, &c)) {
        IDWriteFontSet *cs = NULL; UINT32 total = 0, fi, nf = IDWriteFontCollection1_GetFontFamilyCount(c.coll);
        for (fi = 0; fi < nf; fi++) { IDWriteFontFamily *fam = NULL; if (SUCCEEDED(IDWriteFontCollection1_GetFontFamily(c.coll, fi, &fam)) && fam) { total += IDWriteFontFamily_GetFontCount(fam); DW_RELEASE(fam); } }
        CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c.coll, &cs));
        if (cs) { CHECK(total >= IDWriteFontSet_GetFontCount(cs)); EXPECT_EQ(IDWriteFontSet_GetFontCount(cs), 5); DW_RELEASE(cs); }
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fg_matching_list_font_set_matches_the_list)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5]; cx c;
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (cx_open(&x, set, &c)) {
        static const struct { DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE s; } req[] = { { DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL }, { DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL }, { DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC }, { DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_OBLIQUE }, { DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_STYLE_ITALIC } };
        unsigned i;
        for (i = 0; i < sizeof req / sizeof req[0]; i++) {
            IDWriteFontList *m = NULL; IDWriteFontList2 *l2; IDWriteFontSet1 *fs = G1_SENT(IDWriteFontSet1 *);
            if (FAILED(IDWriteFontFamily_GetMatchingFonts(c.fam, req[i].w, DWRITE_FONT_STRETCH_NORMAL, req[i].s, &m)) || !m) continue;
            l2 = g1_qi((IUnknown *)m, &IID_IDWriteFontList2);
            if (l2) {
                CHECK_HR_OK(IDWriteFontList2_GetFontSet(l2, &fs));
                if (fs && fs != G1_SENT(IDWriteFontSet1 *)) {
                    UINT32 n = IDWriteFontList_GetFontCount(m), k;
                    EXPECT_EQ(IDWriteFontSet1_GetFontCount(fs), n);
                    for (k = 0; k < n; k++) {
                        IDWriteFont *fo = NULL; IDWriteFont3 *f3 = NULL; IDWriteFontFaceReference *r = NULL;
                        if (SUCCEEDED(IDWriteFontList_GetFont(m, k, &fo)) && fo && (f3 = g1_qi((IUnknown *)fo, &IID_IDWriteFont3)) != NULL) { if (SUCCEEDED(IDWriteFont3_GetFontFaceReference(f3, &r)) && r) { CHECKF(ref_present((IDWriteFontSet *)fs, r), "request %u font %u missing", i, k); DW_RELEASE(r); } }
                        DW_RELEASE(f3); DW_RELEASE(fo);
                    }
                    DW_RELEASE(fs);
                }
                DW_RELEASE(l2);
            } else t_note("matching list has no IDWriteFontList2");
            DW_RELEASE(m);
        }
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fg_matching_list_font_set_new_object_each_call)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5]; cx c;
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (cx_open(&x, set, &c) && c.match) {
        IDWriteFontList2 *l2 = g1_qi((IUnknown *)c.match, &IID_IDWriteFontList2); IDWriteFontSet1 *a = NULL, *b = NULL;
        if (l2) {
            CHECK_HR_OK(IDWriteFontList2_GetFontSet(l2, &a)); CHECK_HR_OK(IDWriteFontList2_GetFontSet(l2, &b));
            CHECK(a && b && a != b);
            DW_RELEASE(l2); DW_RELEASE(c.list1); DW_RELEASE(c.match);
            if (a) CHECK(IDWriteFontSet1_GetFontCount(a) >= 1);
            DW_RELEASE(a); DW_RELEASE(b);
        } else t_note("matching list has no IDWriteFontList2");
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fg_font_set_from_family_is_itself_filterable)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5]; cx c;
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (cx_open(&x, set, &c)) {
        IDWriteFontFamily2 *f2 = g1_qi((IUnknown *)c.fam, &IID_IDWriteFontFamily2); IDWriteFontSet1 *fs = NULL, *one = NULL; UINT32 i0 = 0;
        if (f2 && SUCCEEDED(IDWriteFontFamily2_GetFontSet(f2, &fs)) && fs && IDWriteFontSet1_GetFontCount(fs) >= 1) {
            CHECK_HR_OK(IDWriteFontSet1_GetFilteredFonts__(fs, &i0, 1, &one));
            if (one) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(one), 1); DW_RELEASE(one); }
        }
        DW_RELEASE(fs); DW_RELEASE(f2);
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_fg_system_family_font_set)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFontFamily2 *f2; UINT32 idx = 0; BOOL ex = FALSE;
    if (!f || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex);
    if (!ex || FAILED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) || !fam) { DW_RELEASE(c); t_skip("Arial not installed"); return; }
    f2 = g1_qi((IUnknown *)fam, &IID_IDWriteFontFamily2);
    if (f2) {
        IDWriteFontSet1 *fs = NULL;
        CHECK_HR_OK(IDWriteFontFamily2_GetFontSet(f2, &fs));
        if (fs) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(fs), IDWriteFontFamily_GetFontCount(fam)); CHECK(IDWriteFontSet1_GetFontCount(fs) >= 4); DW_RELEASE(fs); }
        DW_RELEASE(f2);
    } else t_skip("IDWriteFontFamily2 unsupported");
    DW_RELEASE(fam); DW_RELEASE(c);
}

/* ===== GetExpirationEvent ======================================================================================== */
static void expiry_checks(const char *what, HANDLE h, HANDLE h2)
{
    CHECKF(h == h2, "%s: two calls returned different handles (%p, %p)", what, h, h2);      /* the stored handle is returned, never a duplicate */
    if (h) {
        DWORD flags = 0, w;
        CHECKF(GetHandleInformation(h, &flags), "%s: handle %p is not valid", what, h);
        w = WaitForSingleObject(h, 0);
        CHECKF(w == WAIT_TIMEOUT || w == WAIT_OBJECT_0, "%s: wait -> %lu", what, w);
        t_note("%s: event %p, wait(0) -> %lu", what, h, w);
    } else t_note("%s: NULL (no expiration event)", what);
}
TEST(gap1_ee_system_collection_event_is_stable)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontCollection3 *c3;
    if (!f || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    c3 = g1_qi((IUnknown *)c, &IID_IDWriteFontCollection3);
    if (c3) {
        HANDLE h = IDWriteFontCollection3_GetExpirationEvent(c3), h2 = IDWriteFontCollection3_GetExpirationEvent(c3), h3 = IDWriteFontCollection3_GetExpirationEvent(c3);
        expiry_checks("system collection", h, h2); CHECK(h3 == h);
        DW_RELEASE(c3);
    } else t_skip("IDWriteFontCollection3 unsupported");
    DW_RELEASE(c);
}
TEST(gap1_ee_system_collection_event_through_two_interfaces)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontCollection3 *a, *b;
    if (!f || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    a = g1_qi((IUnknown *)c, &IID_IDWriteFontCollection3); b = g1_qi((IUnknown *)c, &IID_IDWriteFontCollection3);
    if (a && b) { CHECK(IDWriteFontCollection3_GetExpirationEvent(a) == IDWriteFontCollection3_GetExpirationEvent(b)); }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
}
TEST(gap1_ee_two_system_collection_objects_note)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c1 = NULL, *c2 = NULL; IDWriteFontCollection3 *a, *b;
    if (!f || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c1, FALSE)) || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c2, FALSE)) || !c1 || !c2) { DW_RELEASE(c1); DW_RELEASE(c2); t_skip("no system collection"); return; }
    a = g1_qi((IUnknown *)c1, &IID_IDWriteFontCollection3); b = g1_qi((IUnknown *)c2, &IID_IDWriteFontCollection3);
    if (a && b) t_note("system collection objects %p / %p: events %p / %p", (void *)c1, (void *)c2, IDWriteFontCollection3_GetExpirationEvent(a), IDWriteFontCollection3_GetExpirationEvent(b));
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c1); DW_RELEASE(c2);
}
TEST(gap1_ee_system_collection_and_its_font_set_note)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontCollection3 *c3; IDWriteFontCollection1 *c1; IDWriteFontSet *s = NULL; IDWriteFontSet2 *s2;
    if (!f || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    c3 = g1_qi((IUnknown *)c, &IID_IDWriteFontCollection3); c1 = g1_qi((IUnknown *)c, &IID_IDWriteFontCollection1);
    if (c3 && c1 && SUCCEEDED(IDWriteFontCollection1_GetFontSet(c1, &s)) && s && (s2 = g1_qi((IUnknown *)s, &IID_IDWriteFontSet2)) != NULL) {
        HANDLE hc = IDWriteFontCollection3_GetExpirationEvent(c3), hs = IDWriteFontSet2_GetExpirationEvent(s2), hs2 = IDWriteFontSet2_GetExpirationEvent(s2);
        CHECK(hs == hs2);
        t_note("collection event %p, its font set event %p", hc, hs);
        DW_RELEASE(s2);
    }
    DW_RELEASE(s); DW_RELEASE(c1); DW_RELEASE(c3); DW_RELEASE(c);
}
TEST(gap1_ee_custom_collection_event_is_stable)
{
    G1_BEGIN();
    IDWriteFontCollection3 *c3 = g1_qi((IUnknown *)X.coll, &IID_IDWriteFontCollection3);
    if (c3) {
        HANDLE h = IDWriteFontCollection3_GetExpirationEvent(c3), h2 = IDWriteFontCollection3_GetExpirationEvent(c3);
        expiry_checks("custom collection", h, h2);
        DW_RELEASE(c3);
    } else t_skip("IDWriteFontCollection3 unsupported");
    G1_END();
}
TEST(gap1_ee_collection_from_font_set_event_is_stable)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5]; cx c;
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    if (cx_open(&x, set, &c)) {
        IDWriteFontCollection3 *c3 = g1_qi((IUnknown *)c.coll, &IID_IDWriteFontCollection3);
        if (c3) { HANDLE h = IDWriteFontCollection3_GetExpirationEvent(c3), h2 = IDWriteFontCollection3_GetExpirationEvent(c3); expiry_checks("collection from a font set", h, h2); DW_RELEASE(c3); }
    }
    cx_close(&c); DW_RELEASE(set); SX_END();
}
TEST(gap1_ee_font_set_expiration_event_is_stable)
{
    SX_BEGIN(); IDWriteFontSet *set; UINT32 keys[5];
    if (!std_set(&x, &set, keys)) { SX_END(); return; }
    {
        IDWriteFontSet2 *s2 = g1_qi((IUnknown *)set, &IID_IDWriteFontSet2);
        if (s2) { HANDLE h = IDWriteFontSet2_GetExpirationEvent(s2), h2 = IDWriteFontSet2_GetExpirationEvent(s2); expiry_checks("builder font set", h, h2); DW_RELEASE(s2); }
        else t_skip("IDWriteFontSet2 unsupported");
    }
    DW_RELEASE(set); SX_END();
}
