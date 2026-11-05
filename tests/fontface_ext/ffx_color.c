/* IDWriteFontFace2: IsColorFont, GetColorPaletteCount, GetPaletteEntryCount, GetPaletteEntries.
 *   IsColorFont, GetColorPaletteCount, GetPaletteEntryCount read cached face data.
 *   GetPaletteEntries: first the palette is looked up (unknown index or no CPAL -> DWRITE_E_NOCOLOR 0x8898500C), then
 *   firstEntry > count or entryCount > count - firstEntry -> E_INVALIDARG; the output is not touched on failure.
 *   Colors are converted as byte / 255 into DWRITE_COLOR_F (r, g, b, a) from the CPAL BGRA records. */
#include "ffx.h"
#include <math.h>

#define EINVAL ((HRESULT)0x80070057)
static HRESULT pal(ffx *x, UINT32 p, UINT32 first, UINT32 n, DWRITE_COLOR_F *c) { return IDWriteFontFace2_GetPaletteEntries(x->f2, p, first, n, c); }

TEST(color_default_font_is_not_color) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace2_IsColorFont(x.f2), FALSE);
    EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), 0);
    EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 0);
    ffx_close(&x);
}
TEST(color_colr_cpal_font_is_color) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace2_IsColorFont(x.f2), TRUE);
    EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), 1);
    EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 4);
    ffx_close(&x);
}
TEST(color_flags_are_clean_bools) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    BOOL r = IDWriteFontFace2_IsColorFont(x.f2); CHECK(r == 1);
    ffx_close(&x);
    if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    r = IDWriteFontFace2_IsColorFont(x.f2); CHECK(r == 0);
    ffx_close(&x);
}
TEST(color_all_entries_converted_from_bgra) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_COLOR_F c[4]; memset(c, 0xCC, sizeof c);
    CHECK_HR_OK(pal(&x, 0, 0, 4, c));
    int i;
    for (i = 0; i < 4; i++) {
        uint32_t v = ffx_palette[i];
        float r = ((v >> 16) & 255) / 255.0f, g = ((v >> 8) & 255) / 255.0f, b = (v & 255) / 255.0f, a = (v >> 24) / 255.0f;
        t_check(fabsf(c[i].r - r) < 1e-4f && fabsf(c[i].g - g) < 1e-4f && fabsf(c[i].b - b) < 1e-4f && fabsf(c[i].a - a) < 1e-4f, __FILE__, __LINE__, "color", "entry %d: %g %g %g %g want %g %g %g %g", i, c[i].r, c[i].g, c[i].b, c[i].a, r, g, b, a);
    }
    ffx_close(&x);
}
TEST(color_sub_ranges_select_entries) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_COLOR_F all[4], part[4]; CHECK_HR_OK(pal(&x, 0, 0, 4, all));
    static const struct { UINT32 first, n; } c[] = { { 1, 2 }, { 3, 1 }, { 0, 1 }, { 2, 2 }, { 1, 3 } };
    int i, k;
    for (i = 0; i < 5; i++) {
        memset(part, 0, sizeof part);
        CHECK_HR_OK(pal(&x, 0, c[i].first, c[i].n, part));
        for (k = 0; k < (int)c[i].n; k++) t_check(!memcmp(&part[k], &all[c[i].first + k], sizeof part[0]), __FILE__, __LINE__, "subrange", "first %u n %u entry %d", c[i].first, c[i].n, k);
    }
    ffx_close(&x);
}
TEST(color_zero_count_edge_cases) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    CHECK_HR_OK(pal(&x, 0, 0, 0, NULL));
    CHECK_HR_OK(pal(&x, 0, 4, 0, NULL));       /* first == count with nothing requested is in range */
    CHECK_HR_OK(pal(&x, 0, 2, 0, NULL));
    CHECK_HR(pal(&x, 0, 5, 0, NULL), EINVAL);  /* first > count */
    CHECK_HR(pal(&x, 0, 0xFFFFFFFFu, 0, NULL), EINVAL);
    ffx_close(&x);
}
TEST(color_out_of_range_entries_invalid_and_untouched) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    static const struct { UINT32 first, n; } bad[] = { { 0, 5 }, { 1, 4 }, { 2, 3 }, { 3, 2 }, { 4, 1 }, { 5, 1 }, { 0, 0xFFFFFFFFu }, { 0xFFFFFFFFu, 1 }, { 0xFFFFFFFFu, 0xFFFFFFFFu }, { 3, 0xFFFFFFFEu } };
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        if (t_is_wine() && (bad[i].n > 4 || bad[i].first > 4)) continue;   /* Wine reads/writes before validating */
        DWRITE_COLOR_F c[8]; memset(c, 0x5A, sizeof c);
        HRESULT hr = pal(&x, 0, bad[i].first, bad[i].n > 8 ? 8 : bad[i].n, c);
        if (bad[i].n > 8) hr = pal(&x, 0, bad[i].first, bad[i].n, c);
        t_check(hr == EINVAL, __FILE__, __LINE__, "range", "first %u n %u -> %s", bad[i].first, bad[i].n, dw_hr_name(hr));
        unsigned char *p = (unsigned char *)c; int k, same = 1; for (k = 0; k < (int)sizeof c; k++) if (p[k] != 0x5A) same = 0;
        t_check(same, __FILE__, __LINE__, "untouched", "buffer written on failure (first %u n %u)", bad[i].first, bad[i].n);
    }
    ffx_close(&x);
}
TEST(color_unknown_palette_index_is_nocolor) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    static const UINT32 idx[] = { 1, 2, 7, 0x8000, 0xFFFF, 0x10000, 0xFFFFFFFFu };
    int i;
    for (i = 0; i < 7; i++) {
        DWRITE_COLOR_F c[4]; memset(c, 0x5A, sizeof c);
        HRESULT hr = pal(&x, idx[i], 0, 4, c);
        t_check(hr == FFX_E_NOCOLOR, __FILE__, __LINE__, "palette", "index %u -> %s", idx[i], dw_hr_name(hr));
    }
    /* the palette is resolved before the entry range is checked */
    CHECK_HR(pal(&x, 1, 99, 99, (DWRITE_COLOR_F *)&idx), FFX_E_NOCOLOR);
    ffx_close(&x);
}
TEST(color_non_color_font_palette_is_nocolor) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_COLOR_F c[4]; memset(c, 0x5A, sizeof c);
    CHECK_HR(pal(&x, 0, 0, 4, c), FFX_E_NOCOLOR);
    CHECK_HR(pal(&x, 0, 0, 0, NULL), FFX_E_NOCOLOR);
    CHECK_HR(pal(&x, 3, 9, 9, c), FFX_E_NOCOLOR);
    unsigned char *p = (unsigned char *)c; int k, same = 1; for (k = 0; k < (int)sizeof c; k++) if (p[k] != 0x5A) same = 0;
    CHECK(same);
    ffx_close(&x);
}
TEST(color_repeat_reads_identical) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_COLOR_F a[4], b[4]; CHECK_HR_OK(pal(&x, 0, 0, 4, a)); CHECK_HR_OK(pal(&x, 0, 0, 4, b));
    CHECK(!memcmp(a, b, sizeof a));
    ffx_close(&x);
}
TEST(color_same_through_interfaces) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_COLOR_F a[4], b[4];
    CHECK_HR_OK(IDWriteFontFace2_GetPaletteEntries(x.f2, 0, 0, 4, a)); CHECK_HR_OK(IDWriteFontFace5_GetPaletteEntries(x.f5, 0, 0, 4, b));
    CHECK(!memcmp(a, b, sizeof a));
    EXPECT_EQ(IDWriteFontFace5_IsColorFont(x.f5), TRUE); EXPECT_EQ(IDWriteFontFace3_GetColorPaletteCount(x.f3), 1);
    ffx_close(&x);
}
TEST(color_palette_with_many_entries) {
    uint32_t pal64[64]; int i; for (i = 0; i < 64; i++) pal64[i] = 0xFF000000u | (uint32_t)(i * 4) << 16 | (uint32_t)(255 - i * 4) << 8 | (uint32_t)i;
    ttf_spec s; ffx_spec_color(&s); s.palette = pal64; s.npalette = 64;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 64);
    DWRITE_COLOR_F c[64]; CHECK_HR_OK(pal(&x, 0, 0, 64, c));
    int bad = 0; for (i = 0; i < 64; i++) if (fabsf(c[i].r - (i * 4) / 255.0f) > 1e-4f || fabsf(c[i].g - (255 - i * 4) / 255.0f) > 1e-4f || fabsf(c[i].b - i / 255.0f) > 1e-4f || fabsf(c[i].a - 1.0f) > 1e-4f) bad++;
    EXPECT_EQ(bad, 0);
    CHECK_HR(pal(&x, 0, 1, 64, c), EINVAL);
    ffx_close(&x);
}
TEST(color_colr_without_cpal_note) {
    /* COLR alone: the color flag/palette state is whatever DirectWrite derives; record it. */
    static const ttf_colr_base cb[] = { { 2, 0, 1 } }; static const ttf_colr_layer cl[] = { { 64, 0 } }; static const uint32_t p1[] = { 0xFF00FF00u };
    ttf_spec s; ttf_default_spec(&s); s.colr_base = cb; s.ncolr_base = 1; s.colr_layers = cl; s.ncolr_layers = 1; s.palette = p1; s.npalette = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_COLOR_F c; HRESULT hr = pal(&x, 0, 0, 1, &c);
    EXPECT_EQ(IDWriteFontFace2_IsColorFont(x.f2), TRUE);
    CHECK_HR_OK(hr);
    CHECK_NEAR(c.g, 1.0f, 1e-4); CHECK_NEAR(c.r, 0.0f, 1e-4); CHECK_NEAR(c.a, 1.0f, 1e-4);
    ffx_close(&x);
}
static void check_sys_color(const WCHAR *fam, int want_color) {
    ffx x; if (!ffx_open_sys(&x, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    BOOL color = IDWriteFontFace2_IsColorFont(x.f2);
    UINT32 pc = IDWriteFontFace2_GetColorPaletteCount(x.f2), ec = IDWriteFontFace2_GetPaletteEntryCount(x.f2), clen = 0, plen = 0;
    uint8_t *colr = dw_face_table(x.f0, DW_TAG('C','O','L','R'), &clen), *cpal = dw_face_table(x.f0, DW_TAG('C','P','A','L'), &plen);
    t_note("%ls: IsColorFont=%d palettes=%u entries=%u COLR=%u CPAL=%u", fam, color, pc, ec, clen, plen);
    if (want_color >= 0) EXPECT_EQ(color, want_color);
    if (colr && cpal && plen >= 12) {
        EXPECT_EQ(color, TRUE);
        EXPECT_EQ(pc, ffx_getu16(cpal, 4));                                                                 /* CPAL numPalettes at +4 */
        EXPECT_EQ(ec, ffx_getu16(cpal, 2));                                                                 /* numPaletteEntries at +2 */
        if (pc > 0 && ec > 0) {
            DWRITE_COLOR_F *c = (DWRITE_COLOR_F *)calloc(ec, sizeof *c); UINT32 i; int bad = 0;
            CHECK_HR_OK(pal(&x, 0, 0, ec, c));
            for (i = 0; i < ec; i++) if (c[i].r < 0 || c[i].r > 1 || c[i].g < 0 || c[i].g > 1 || c[i].b < 0 || c[i].b > 1 || c[i].a < 0 || c[i].a > 1) bad++;
            EXPECT_EQ(bad, 0);
            if (plen >= 14 + 4) {
                /* first color record: CPAL v0 header, colorRecordsArrayOffset at +8, palette 0 index at +12 */
                unsigned off = (ffx_getu16(cpal, 8) << 16) | ffx_getu16(cpal, 10), first = ffx_getu16(cpal, 12);
                if (off + 4 * (first + 1) <= plen) {
                    const uint8_t *rec = cpal + off + 4 * first;
                    CHECK_NEAR(c[0].b, rec[0] / 255.0f, 1e-4); CHECK_NEAR(c[0].g, rec[1] / 255.0f, 1e-4);
                    CHECK_NEAR(c[0].r, rec[2] / 255.0f, 1e-4); CHECK_NEAR(c[0].a, rec[3] / 255.0f, 1e-4);
                }
            }
            free(c);
        }
        CHECK_HR(pal(&x, pc, 0, 1, (DWRITE_COLOR_F *)&pc), FFX_E_NOCOLOR);
    }
    free(colr); free(cpal); ffx_close(&x);
}
TEST(color_segoe_ui_emoji) { check_sys_color(L"Segoe UI Emoji", TRUE); }
TEST(color_segoe_ui_symbol) { check_sys_color(L"Segoe UI Symbol", -1); }
TEST(color_arial_not_color) { check_sys_color(L"Arial", FALSE); }
TEST(color_times_not_color) { check_sys_color(L"Times New Roman", FALSE); }
TEST(color_consolas_not_color) { check_sys_color(L"Consolas", FALSE); }
TEST(color_segoe_ui_not_color) { check_sys_color(L"Segoe UI", FALSE); }
TEST(color_bahnschrift_not_color) { check_sys_color(L"Bahnschrift", FALSE); }
