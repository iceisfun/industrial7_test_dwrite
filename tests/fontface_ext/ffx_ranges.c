/* IDWriteFontFace1::GetUnicodeRanges.  DWriteFontFace::GetUnicodeRanges (dwrite.dll!0xdd0b0):
 *   *count = 0; if (ranges) memset(ranges, 0, max_count * 8);
 *   raw CFF -> DWRITE_E_UNSUPPORTEDOPERATION; if (max_count != 0 && ranges == NULL) -> E_INVALIDARG;
 *   else enumerate maximal runs of code points with a non-zero glyph in the cmap (CmapBase::GetUnicodeRanges, !0x10d66c),
 *   store the first max_count of them, *count = total; total > max_count -> HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER). */
#include "ffx.h"

static HRESULT ranges(ffx *x, UINT32 max, DWRITE_UNICODE_RANGE *r, UINT32 *cnt) { return IDWriteFontFace1_GetUnicodeRanges(x->f1, max, r, cnt); }
#define NSB ((HRESULT)0x8007007A)
typedef struct { UINT32 a, b; } rg;

static void expect_ranges(ffx *x, const rg *want, int n, const char *what) {
    DWRITE_UNICODE_RANGE buf[64]; UINT32 cnt = 0xDEAD; int i;
    memset(buf, 0xAB, sizeof buf);
    HRESULT hr = ranges(x, 64, buf, &cnt);
    t_check(hr == S_OK, __FILE__, __LINE__, what, "hr %s", dw_hr_name(hr));
    t_check(cnt == (UINT32)n, __FILE__, __LINE__, what, "count %u want %d", cnt, n);
    for (i = 0; i < n && i < 64; i++) t_check(buf[i].first == want[i].a && buf[i].last == want[i].b, __FILE__, __LINE__, what, "range %d: %04X-%04X want %04X-%04X", i, buf[i].first, buf[i].last, want[i].a, want[i].b);
    for (i = n; i < 64; i++) if (buf[i].first != 0 || buf[i].last != 0) { t_check(0, __FILE__, __LINE__, what, "entry %d beyond the count not zeroed", i); break; }
}
static const rg def_ranges[] = { { 0x20, 0x20 }, { 0x30, 0x39 }, { 0x41, 0x5A }, { 0x61, 0x7A } };
static const rg sparse_ranges[] = { { 0x41, 0x43 }, { 0x45, 0x45 }, { 0x61, 0x61 }, { 0x3041, 0x3043 }, { 0x4E00, 0x4E00 }, { 0x1F600, 0x1F601 } };

TEST(ur_default_font_ranges) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    expect_ranges(&x, def_ranges, 4, "default");
    ffx_close(&x);
}
TEST(ur_count_query_with_zero_max_and_null) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT32 cnt = 0xDEAD;
    CHECK_HR(ranges(&x, 0, NULL, &cnt), NSB);
    EXPECT_EQ(cnt, 4);
    ffx_close(&x);
}
TEST(ur_zero_max_nonnull_buffer_untouched) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_UNICODE_RANGE b[4]; UINT32 cnt = 0xDEAD; memset(b, 0x5A, sizeof b);
    CHECK_HR(ranges(&x, 0, b, &cnt), NSB);
    EXPECT_EQ(cnt, 4);
    unsigned char *p = (unsigned char *)b; int i, same = 1; for (i = 0; i < (int)sizeof b; i++) if (p[i] != 0x5A) same = 0;
    CHECK(same);              /* memset(ranges, 0, 0 * 8) writes nothing */
    ffx_close(&x);
}
TEST(ur_exact_fit_succeeds) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_UNICODE_RANGE b[4]; UINT32 cnt = 0;
    CHECK_HR_OK(ranges(&x, 4, b, &cnt));
    EXPECT_EQ(cnt, 4);
    int i; for (i = 0; i < 4; i++) { EXPECT_EQ(b[i].first, def_ranges[i].a); EXPECT_EQ(b[i].last, def_ranges[i].b); }
    ffx_close(&x);
}
TEST(ur_one_short_reports_insufficient_buffer) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_UNICODE_RANGE b[4]; UINT32 cnt = 0; memset(b, 0xAB, sizeof b);
    CHECK_HR(ranges(&x, 3, b, &cnt), NSB);
    EXPECT_EQ(cnt, 4);
    int i; for (i = 0; i < 3; i++) { EXPECT_EQ(b[i].first, def_ranges[i].a); EXPECT_EQ(b[i].last, def_ranges[i].b); }
    EXPECT_EQ(b[3].first, 0xABABABAB);    /* entries beyond max_count are not touched */
    ffx_close(&x);
}
TEST(ur_every_short_size_fills_prefix) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT32 m;
    for (m = 1; m < 4; m++) {
        DWRITE_UNICODE_RANGE b[8]; UINT32 cnt = 0; UINT32 i; memset(b, 0xAB, sizeof b);
        HRESULT hr = ranges(&x, m, b, &cnt);
        t_check(hr == NSB && cnt == 4, __FILE__, __LINE__, "short", "max %u: %s count %u", m, dw_hr_name(hr), cnt);
        for (i = 0; i < m; i++) t_check(b[i].first == def_ranges[i].a && b[i].last == def_ranges[i].b, __FILE__, __LINE__, "short", "max %u range %u", m, i);
        for (i = m; i < 8; i++) t_check(b[i].first == 0xABABABAB, __FILE__, __LINE__, "short", "max %u: entry %u overwritten", m, i);
    }
    ffx_close(&x);
}
TEST(ur_oversized_buffer_is_zero_filled) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_UNICODE_RANGE b[1000]; UINT32 cnt = 0; memset(b, 0xCD, sizeof b);
    CHECK_HR_OK(ranges(&x, 1000, b, &cnt));
    EXPECT_EQ(cnt, 4);
    int i, bad = 0; for (i = 4; i < 1000; i++) if (b[i].first || b[i].last) bad++;
    EXPECT_EQ(bad, 0);       /* the whole max_count * 8 bytes are zeroed first */
    ffx_close(&x);
}
TEST(ur_null_ranges_with_nonzero_max_is_invalid) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const UINT32 maxes[] = { 1, 2, 4, 5, 100, 0xFFFFFFFFu };
    int i;
    for (i = 0; i < 6; i++) {
        UINT32 cnt = 0xDEAD; HRESULT hr = ranges(&x, maxes[i], NULL, &cnt);
        t_check(hr == (HRESULT)0x80070057 && cnt == 0, __FILE__, __LINE__, "null ranges", "max %u: %s count %u", maxes[i], dw_hr_name(hr), cnt);
    }
    ffx_close(&x);
}
TEST(ur_count_is_zeroed_before_anything) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT32 cnt = 0x12345678; DWRITE_UNICODE_RANGE b[1];
    ranges(&x, 1, b, &cnt);                         /* NSB, count 4 */
    EXPECT_EQ(cnt, 4);
    cnt = 0x12345678; ranges(&x, 1, NULL, &cnt);    /* invalid -> 0 */
    EXPECT_EQ(cnt, 0);
    ffx_close(&x);
}
TEST(ur_ranges_are_repeatable) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_UNICODE_RANGE a[8], b[8]; UINT32 ca = 0, cb = 0;
    CHECK_HR_OK(ranges(&x, 8, a, &ca)); CHECK_HR_OK(ranges(&x, 8, b, &cb));
    EXPECT_EQ(ca, cb); CHECK(!memcmp(a, b, sizeof a));
    ffx_close(&x);
}
TEST(ur_sparse_cmap_with_supplementary_plane) {
    ttf_spec s; ffx_spec_ranges(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    expect_ranges(&x, sparse_ranges, 6, "sparse");
    ffx_close(&x);
}
TEST(ur_sparse_cmap_union_matches_glyph_lookup) {
    ttf_spec s; ffx_spec_ranges(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_UNICODE_RANGE b[16]; UINT32 cnt = 0; CHECK_HR_OK(ranges(&x, 16, b, &cnt));
    UINT32 cp, bad = 0;
    for (cp = 0; cp < 0x5000; cp++) {
        UINT16 g = 0; UINT32 c = cp; IDWriteFontFace_GetGlyphIndices(x.f0, &c, 1, &g);
        int in = 0; UINT32 i; for (i = 0; i < cnt; i++) if (cp >= b[i].first && cp <= b[i].last) in = 1;
        if ((g != 0) != in) bad++;
    }
    EXPECT_EQ(bad, 0);
    UINT32 hi[] = { 0x1F5FF, 0x1F600, 0x1F601, 0x1F602, 0x10FFFF };
    int k; for (k = 0; k < 5; k++) { UINT16 g = 0; UINT32 c = hi[k]; IDWriteFontFace_GetGlyphIndices(x.f0, &c, 1, &g); int in = 0; UINT32 i; for (i = 0; i < cnt; i++) if (hi[k] >= b[i].first && hi[k] <= b[i].last) in = 1; t_check((g != 0) == in, __FILE__, __LINE__, "union", "U+%X glyph %u in-range %d", hi[k], g, in); }
    ffx_close(&x);
}
TEST(ur_bmp_only_cmap) {
    ttf_spec s; ffx_spec_ranges(&s);
    ttf_cmap one[1]; { int n; const ttf_cmap *c = ttf_default_cmaps(&n); (void)c; }
    /* keep only the (3,1) format 4 subtable: the second entry of the fixture */
    one[0] = s.cmaps[1]; s.cmaps = one; s.ncmaps = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    static const rg want[] = { { 0x41, 0x43 }, { 0x45, 0x45 }, { 0x61, 0x61 }, { 0x3041, 0x3043 }, { 0x4E00, 0x4E00 } };
    expect_ranges(&x, want, 5, "bmp only");
    ffx_close(&x);
}
TEST(ur_ucs4_only_cmap) {
    ttf_spec s; ffx_spec_ranges(&s);
    ttf_cmap one[1]; one[0] = s.cmaps[0]; s.cmaps = one; s.ncmaps = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    expect_ranges(&x, sparse_ranges, 6, "ucs4 only");
    ffx_close(&x);
}
TEST(ur_adjacent_codepoints_merge) {
    static const ttf_map m[] = { { 0x100, 2 }, { 0x101, 3 }, { 0x102, 4 }, { 0x103, 5 }, { 0x105, 6 }, { 0x106, 7 }, { 0x200, 8 }, { 0xFFFD, 9 } };
    static const ttf_cmap cm[] = { { 4, 3, 1, 8, m, 0, NULL } };
    ttf_spec s; ttf_default_spec(&s); s.cmaps = cm; s.ncmaps = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    static const rg want[] = { { 0x100, 0x103 }, { 0x105, 0x106 }, { 0x200, 0x200 }, { 0xFFFD, 0xFFFD } };
    expect_ranges(&x, want, 4, "merge");
    ffx_close(&x);
}
TEST(ur_ranges_ascending_and_disjoint_synthetic) {
    ttf_spec s; ffx_spec_ranges(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_UNICODE_RANGE b[16]; UINT32 cnt = 0, i; CHECK_HR_OK(ranges(&x, 16, b, &cnt));
    for (i = 0; i < cnt; i++) {
        CHECK(b[i].first <= b[i].last);
        if (i) CHECK(b[i].first > b[i - 1].last + 1);       /* maximal runs: never touching */
    }
    ffx_close(&x);
}
TEST(ur_same_through_interfaces) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_UNICODE_RANGE a[4], b[4]; UINT32 ca = 0, cb = 0;
    CHECK_HR_OK(IDWriteFontFace1_GetUnicodeRanges(x.f1, 4, a, &ca));
    CHECK_HR_OK(IDWriteFontFace5_GetUnicodeRanges(x.f5, 4, b, &cb));
    CHECK(ca == cb && !memcmp(a, b, sizeof a));
    ffx_close(&x);
}

static void check_system_ranges(const WCHAR *fam, int mono_check) {
    ffx x; if (!ffx_open_sys(&x, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cnt = 0; HRESULT hr = ranges(&x, 0, NULL, &cnt);
    t_check(hr == NSB || (hr == S_OK && cnt == 0), __FILE__, __LINE__, "count", "hr %s count %u", dw_hr_name(hr), cnt);
    if (cnt > 0 && cnt < 20000) {
        DWRITE_UNICODE_RANGE *b = (DWRITE_UNICODE_RANGE *)calloc(cnt, sizeof *b); UINT32 c2 = 0, i;
        CHECK_HR_OK(ranges(&x, cnt, b, &c2)); EXPECT_EQ(c2, cnt);
        int order_bad = 0, edge_bad = 0;
        for (i = 0; i < cnt; i++) {
            if (b[i].first > b[i].last) order_bad++;
            if (i && b[i].first <= b[i - 1].last + 1) order_bad++;
            if (!IDWriteFontFace3_HasCharacter(x.f3, b[i].first) || !IDWriteFontFace3_HasCharacter(x.f3, b[i].last)) edge_bad++;
            if (b[i].first > 0 && IDWriteFontFace3_HasCharacter(x.f3, b[i].first - 1)) edge_bad++;
            if (b[i].last < 0x10FFFF && IDWriteFontFace3_HasCharacter(x.f3, b[i].last + 1)) edge_bad++;
        }
        EXPECT_EQ(order_bad, 0); EXPECT_EQ(edge_bad, 0);
        t_note("%d ranges, first U+%04X last U+%04X", cnt, b[0].first, b[cnt - 1].last);
        free(b);
    }
    ffx_close(&x);
}
#define SYS_RANGES(NAME, FAM) TEST(NAME) { check_system_ranges(FAM, 0); }
SYS_RANGES(ur_system_arial, L"Arial")
SYS_RANGES(ur_system_times, L"Times New Roman")
SYS_RANGES(ur_system_courier, L"Courier New")
SYS_RANGES(ur_system_segoe_ui, L"Segoe UI")
SYS_RANGES(ur_system_tahoma, L"Tahoma")
SYS_RANGES(ur_system_consolas, L"Consolas")
SYS_RANGES(ur_system_marlett, L"Marlett")
SYS_RANGES(ur_system_segoe_symbol, L"Segoe UI Symbol")
SYS_RANGES(ur_system_segoe_emoji, L"Segoe UI Emoji")
SYS_RANGES(ur_system_ms_gothic, L"MS Gothic")
SYS_RANGES(ur_system_bahnschrift, L"Bahnschrift")

TEST(ur_arial_covers_basic_latin) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cnt = 0; ranges(&x, 0, NULL, &cnt);
    DWRITE_UNICODE_RANGE *b = (DWRITE_UNICODE_RANGE *)calloc(cnt + 1, sizeof *b); UINT32 c2 = 0, i; int found = 0;
    ranges(&x, cnt, b, &c2);
    for (i = 0; i < c2; i++) if (b[i].first <= 0x41 && b[i].last >= 0x5A) found = 1;
    CHECK(found);
    found = 0; for (i = 0; i < c2; i++) if (b[i].first <= 0x30 && b[i].last >= 0x39) found = 1;
    CHECK(found);
    free(b); ffx_close(&x);
}
TEST(ur_marlett_symbol_font_ranges_note) {
    ffx x; if (!ffx_open_sys(&x, L"Marlett", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cnt = 0; HRESULT hr = ranges(&x, 0, NULL, &cnt);
    t_note("Marlett (symbol cmap) range query: %s count %u, IsSymbolFont=%d", dw_hr_name(hr), cnt, IDWriteFontFace_IsSymbolFont(x.f0));
    CHECK(hr == NSB || hr == S_OK);
    ffx_close(&x);
}
