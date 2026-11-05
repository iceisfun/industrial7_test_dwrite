/* CPAL palette layouts through IDWriteFontFace2::GetColorPaletteCount / GetPaletteEntryCount / GetPaletteEntries / IsColorFont.
 *
 * IDWriteFontFace::GetPaletteEntries: the palette is resolved first (ColorGlyphMap::GetPalette: DWRITE_E_NOCOLOR 0x8898500C when
 * paletteIndex >= numPalettes), then firstEntryIndex > entryCount or entryCount > paletteEntries - firstEntryIndex gives E_INVALIDARG; nothing is written on failure.
 * Colour record n of palette p is colorRecords[colorRecordIndices[p] + n]; each channel is byte / 255 (ByteToFloat), CPAL stores B, G, R, A. */
#include "crb.h"

static uint8_t *font_cpal(unsigned ver, unsigned nent, unsigned npal, const unsigned *first, unsigned nrec, const uint32_t *argb, unsigned colr_ver, size_t *n) {
    static const crb_cbase b[1] = { { 2, 0, 2 } }; static const crb_clayer l[2] = { { 64, 0 }, { 65, 0 } };
    size_t nc, np; uint8_t *colr = crb_colr(colr_ver, b, 1, l, 2, &nc), *cpal = crb_cpal(ver, nent, npal, first, nrec, argb, &np), *out; ttf_rawtable raw[2];
    memcpy(raw[0].tag, "COLR", 5); raw[0].data = colr; raw[0].len = (unsigned)nc; memcpy(raw[1].tag, "CPAL", 5); raw[1].data = cpal; raw[1].len = (unsigned)np;
    out = crb_blob(raw, 2, 0, n); free(colr); free(cpal); return out;
}
static HRESULT gpe(crb *x, UINT32 p, UINT32 first, UINT32 cnt, DWRITE_COLOR_F *c) { return IDWriteFontFace2_GetPaletteEntries(x->f2, p, first, cnt, c); }
static float b2f(unsigned v) { return (float)v / 255.0f; }
/* Win10 converts with a reciprocal multiply, which can differ from byte / 255 in the last bit */
static int feq(float a, float b) { return fabsf(a - b) <= 1e-6f; }
#define NEED_FACE2() if (!x.f2) { t_skip("IDWriteFontFace2 unsupported"); crb_close(&x); return; }

/* expected result of GetPaletteEntries for a layout */
static HRESULT expect_hr(UINT32 p, UINT32 first, UINT32 cnt, UINT32 npal, UINT32 nent) {
    if (p >= npal) return E_NOCOLOR_;
    if (first > nent || nent - first < cnt) return E_INVALIDARG_;
    return S_OK;
}
static void check_layout(crb *x, const unsigned *start, unsigned npal, unsigned nent, const uint32_t *rec, unsigned nrec, const char *what) {
    UINT32 p, first, cnt;
    EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x->f2), npal); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x->f2), nent);
    for (p = 0; p < npal + 3; p++) for (first = 0; first <= nent + 2; first++) for (cnt = 0; cnt <= nent + 2; cnt++) {
        DWRITE_COLOR_F c[16]; unsigned i; HRESULT hr, want = expect_hr(p, first, cnt, npal, nent);
        for (i = 0; i < 16; i++) { c[i].r = c[i].g = c[i].b = c[i].a = -7.0f; }
        hr = gpe(x, p, first, cnt, c);
        CHECKF(hr == want, "%s: palette %u first %u count %u: %s want %s", what, p, first, cnt, dw_hr_name(hr), dw_hr_name(want));
        if (hr == S_OK) {
            for (i = 0; i < cnt; i++) {
                uint32_t v = rec[start[p] + first + i];
                CHECKF(feq(c[i].r, b2f((v >> 16) & 255)) && feq(c[i].g, b2f((v >> 8) & 255)) && feq(c[i].b, b2f(v & 255)) && feq(c[i].a, b2f(v >> 24)), "%s: palette %u entry %u: (%g %g %g %g)", what, p, first + i, c[i].r, c[i].g, c[i].b, c[i].a);
            }
            CHECKF(c[cnt].r == -7.0f && c[cnt].a == -7.0f, "%s: wrote beyond entryCount (palette %u first %u count %u)", what, p, first, cnt);
        } else {
            CHECKF(c[0].r == -7.0f && c[0].g == -7.0f && c[0].b == -7.0f && c[0].a == -7.0f, "%s: output written on failure (%s)", what, dw_hr_name(hr));
        }
    }
}

T(palette_contiguous_layout_exhaustive) {
    static const uint32_t rec[9] = { 0xFF112233u, 0x80445566u, 0x00778899u, 0xFFAABBCCu, 0x01DDEEFFu, 0xFE010203u, 0x7F0A0B0Cu, 0xC0FF00FFu, 0x10203040u }; static const unsigned st[3] = { 0, 3, 6 };
    size_t n; uint8_t *b = font_cpal(0, 3, 3, st, 9, rec, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
    EXPECT_EQ(IDWriteFontFace2_IsColorFont(x.f2), TRUE);
    check_layout(&x, st, 3, 3, rec, 9, "contiguous");
    crb_close(&x);
}
T(palette_shared_and_overlapping_records) {
    /* colorRecordIndices need not be multiples of numPaletteEntries: palette 1 starts one record after palette 0, palette 2 repeats palette 0 */
    static const uint32_t rec[6] = { 0xFF000001u, 0xFF000102u, 0xFF010203u, 0xFF020304u, 0xFF030405u, 0xFF040506u }; static const unsigned st[4] = { 0, 1, 0, 3 };
    size_t n; uint8_t *b = font_cpal(0, 3, 4, st, 6, rec, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
    check_layout(&x, st, 4, 3, rec, 6, "overlapping");
    { DWRITE_COLOR_F a, c; CHECK_HR_OK(gpe(&x, 0, 1, 1, &a)); CHECK_HR_OK(gpe(&x, 1, 0, 1, &c)); CHECK(a.r == c.r && a.g == c.g && a.b == c.b && a.a == c.a); }
    crb_close(&x);
}
T(palette_single_palette_single_entry) {
    static const uint32_t rec[1] = { 0x80FF8040u }; static const unsigned st[1] = { 0 };
    size_t n; uint8_t *b = font_cpal(0, 1, 1, st, 1, rec, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
    check_layout(&x, st, 1, 1, rec, 1, "1x1");
    crb_close(&x);
}
T(palette_wide_palettes) {
    static uint32_t rec[2 * 40]; static unsigned st[2] = { 0, 40 }; int i;
    for (i = 0; i < 80; i++) rec[i] = 0xFF000000u ^ ((uint32_t)(i * 37) << 16) ^ ((uint32_t)(i * 11) << 8) ^ (uint32_t)(i * 5) ^ ((uint32_t)i << 24);
    { size_t n; uint8_t *b = font_cpal(0, 40, 2, st, 80, rec, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
      EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 40);
      { UINT32 first, cnt; for (first = 0; first <= 40; first += 3) for (cnt = 0; cnt <= 40; cnt += 5) {
            DWRITE_COLOR_F c[41]; HRESULT hr = gpe(&x, 1, first, cnt, c); HRESULT want = expect_hr(1, first, cnt, 2, 40);
            CHECKF(hr == want, "first %u count %u: %s", first, cnt, dw_hr_name(hr));
            if (hr == S_OK && cnt) { uint32_t v = rec[40 + first + cnt - 1]; CHECK(feq(c[cnt - 1].r, b2f((v >> 16) & 255)) && feq(c[cnt - 1].a, b2f(v >> 24))); } } }
      crb_close(&x); }
}
T(palette_channel_conversion_all_byte_values) {
    static uint32_t rec[256]; static unsigned st[1] = { 0 }; int i;
    for (i = 0; i < 256; i++) rec[i] = ((uint32_t)i << 24) | ((uint32_t)(255 - i) << 16) | ((uint32_t)((i * 7) & 255) << 8) | (uint32_t)(i ^ 0x5A);
    { size_t n; uint8_t *b = font_cpal(0, 256, 1, st, 256, rec, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
      EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 256);
      { static DWRITE_COLOR_F c[256]; CHECK_HR_OK(gpe(&x, 0, 0, 256, c));
        for (i = 0; i < 256; i++) {
            /* byte / 255 for every channel (within 1e-6) (CPAL stores BGRA, DWRITE_COLOR_F is RGBA) */
            CHECKF(feq(c[i].a, b2f((unsigned)i)) && feq(c[i].r, b2f((unsigned)(255 - i))) && feq(c[i].g, b2f((unsigned)((i * 7) & 255))) && feq(c[i].b, b2f((unsigned)(i ^ 0x5A))), "entry %d: (%.9g %.9g %.9g %.9g)", i, c[i].r, c[i].g, c[i].b, c[i].a);
        }
        CHECK(c[0].a == 0.0f && feq(c[255].a, 1.0f)); }
      crb_close(&x); }
}
T(palette_failed_calls_leave_the_buffer_alone_and_validation_order) {
    static const uint32_t rec[4] = { 1, 2, 3, 4 }; static const unsigned st[2] = { 0, 2 };
    size_t n; uint8_t *b = font_cpal(0, 2, 2, st, 4, rec, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
    DWRITE_COLOR_F c[4]; memset(c, 0x5A, sizeof c);
    /* an unknown palette is reported before the range is looked at */
    CHECK_HR(gpe(&x, 2, 0, 1, c), E_NOCOLOR_); CHECK_HR(gpe(&x, 2, 5, 100, c), E_NOCOLOR_); CHECK_HR(gpe(&x, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, c), E_NOCOLOR_);
    CHECK_HR(gpe(&x, 0, 3, 0, c), E_INVALIDARG_); CHECK_HR(gpe(&x, 1, 0, 3, c), E_INVALIDARG_); 
    if (!t_is_wine()) { CHECK_HR(gpe(&x, 0, 0xFFFFFFFFu, 1, c), E_INVALIDARG_); CHECK_HR(gpe(&x, 0, 1, 0xFFFFFFFFu, c), E_INVALIDARG_); }   /* the emulator's range check overflows and crashes */
    { unsigned char *q = (unsigned char *)c; size_t i; int same = 1; for (i = 0; i < sizeof c; i++) if (q[i] != 0x5A) same = 0; CHECK(same); }
    /* boundaries that are valid */
    CHECK_HR_OK(gpe(&x, 0, 2, 0, c)); CHECK_HR_OK(gpe(&x, 1, 0, 2, c)); CHECK_HR_OK(gpe(&x, 1, 1, 1, c)); CHECK_HR_OK(gpe(&x, 0, 0, 0, c));
    crb_close(&x);
}
T(palette_zero_entry_palettes) {
    /* Win10 rejects a CPAL with zero entries per palette when the font is opened */
    static const uint32_t rec[1] = { 0xFF0000FFu }; static const unsigned st[2] = { 0, 0 };
    size_t n; uint8_t *b = font_cpal(0, 0, 2, st, 0, rec, 0, &n); dwx_font h; HRESULT hr;
    if (!b) { t_skip("ttf_build failed"); return; }
    if (!dw_shared()) { free(b); t_skip("DWriteCreateFactory unavailable"); return; }
    hr = dwx_font_open(&h, dw_shared(), b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    t_note("CPAL with 0 entries and 2 palettes: font open %s", dw_hr_name(hr));
    CHECKF(FAILED(hr) && h.face == NULL, "font with an empty CPAL palette opened: %s", dw_hr_name(hr));
    if (SUCCEEDED(hr)) dwx_font_close(&h);
    free(b);
}
T(palette_cpal_version_one_header_note) {
    /* CPAL version 1 appends three offsets (palette types, labels, entry labels) after the record indices */
    static const uint32_t rec[6] = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu, 0xFF010101u, 0xFF020202u, 0xFF030303u }; static const unsigned st[2] = { 0, 3 };
    size_t n; uint8_t *b = font_cpal(1, 3, 2, st, 6, rec, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
    t_note("CPAL v1: IsColorFont=%d palettes=%u entries=%u", IDWriteFontFace2_IsColorFont(x.f2), IDWriteFontFace2_GetColorPaletteCount(x.f2), IDWriteFontFace2_GetPaletteEntryCount(x.f2));
    if (IDWriteFontFace2_GetColorPaletteCount(x.f2) == 2) check_layout(&x, st, 2, 3, rec, 6, "cpal v1");
    crb_close(&x);
}
T(palette_colr_version_one_header_note) {
    static const uint32_t rec[3] = { 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu }; static const unsigned st[1] = { 0 };
    size_t n; uint8_t *b = font_cpal(0, 3, 1, st, 3, rec, 1, &n); CRB_NEED(x, b, n); NEED_FACE2();
    IDWriteFactory4 *f4 = crb_f4();
    t_note("COLR v1 header with v0 records: IsColorFont=%d face formats 0x%x", IDWriteFontFace2_IsColorFont(x.f2), crb_face_formats(&x));
    if (f4 && (crb_face_formats(&x) & F_COLR)) {
        UINT16 g = 2; float adv = 10; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator1 *e = NULL; D2D1_POINT_2F o = { 0, 0 };
        memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 20; r.glyphCount = 1; r.glyphIndices = &g; r.glyphAdvances = &adv;
        { HRESULT hr = IDWriteFactory4_TranslateColorGlyphRun(f4, o, &r, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(F_TT | F_COLR), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e); t_note("translate on a COLR v1 header font: %s", dw_hr_name(hr)); DW_RELEASE(e); }
    }
    crb_close(&x);
}
T(palette_is_color_font_by_table_combination_note) {
    static const uint32_t rec[2] = { 0xFFFF0000u, 0xFF00FF00u }; static const unsigned st[1] = { 0 };
    size_t nc, np, n; static const crb_cbase b0[1] = { { 2, 0, 1 } }; static const crb_clayer l0[1] = { { 64, 0 } };
    uint8_t *colr = crb_colr(0, b0, 1, l0, 1, &nc), *cpal = crb_cpal(0, 2, 1, st, 2, rec, &np); ttf_rawtable raw[2]; int variant;
    for (variant = 0; variant < 4; variant++) {
        int nraw = 0; uint8_t *blob;
        if (variant == 0 || variant == 1) { memcpy(raw[nraw].tag, "COLR", 5); raw[nraw].data = colr; raw[nraw].len = (unsigned)nc; nraw++; }
        if (variant == 0 || variant == 2) { memcpy(raw[nraw].tag, "CPAL", 5); raw[nraw].data = cpal; raw[nraw].len = (unsigned)np; nraw++; }
        blob = crb_blob(raw, nraw, 0, &n);
        { crb x; if (crb_open(&x, blob, n)) {
            if (x.f2) {
                UINT32 ff = crb_face_formats(&x);
                t_note("tables %s%s: IsColorFont=%d palettes=%u entries=%u face formats 0x%x", variant == 0 || variant == 1 ? "COLR " : "", variant == 0 || variant == 2 ? "CPAL" : "", IDWriteFontFace2_IsColorFont(x.f2), IDWriteFontFace2_GetColorPaletteCount(x.f2), IDWriteFontFace2_GetPaletteEntryCount(x.f2), ff);
                /* the COLR face format bit needs both tables (OpenTypeFontFaceBuilder::ReadColorTables) */
                CHECKF(((ff & F_COLR) != 0) == (variant == 0), "variant %d: face formats 0x%x", variant, ff);
                if (variant == 0) { EXPECT_EQ(IDWriteFontFace2_IsColorFont(x.f2), TRUE); EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), 1); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 2); }
            }
            crb_close(&x);
        } else crb_close(&x); }
    }
    free(colr); free(cpal);
}
T(palette_sbix_png_font_is_color_note) {
    size_t sl, n, pl; uint8_t *png = crb_png(4, 4, 1, &pl), *sbx, *out; crb_sbrec r = { 2, CRB_TAG('p','n','g',' '), 0, 0, png, pl, 0 }; crb_sbstrike st = { 20, 72, &r, 1 }; ttf_rawtable raw[1];
    sbx = crb_sbix(&st, 1, TTF_DEF_NGLYPHS, 1, &sl); memcpy(raw[0].tag, "sbix", 5); raw[0].data = sbx; raw[0].len = (unsigned)sl; out = crb_blob(raw, 1, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        if (x.f2) {
            t_note("sbix-only font: IsColorFont=%d palettes=%u entries=%u", IDWriteFontFace2_IsColorFont(x.f2), IDWriteFontFace2_GetColorPaletteCount(x.f2), IDWriteFontFace2_GetPaletteEntryCount(x.f2));
            EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), 0); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 0);
            { DWRITE_COLOR_F c[1]; CHECK_HR(gpe(&x, 0, 0, 0, c), E_NOCOLOR_); }
        }
        crb_close(&x);
    } else crb_close(&x); }
    free(sbx); free(png);
}
T(palette_plain_font_has_no_palettes) {
    size_t n; uint8_t *b = crb_blob(NULL, 0, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
    DWRITE_COLOR_F c[1];
    EXPECT_EQ(IDWriteFontFace2_IsColorFont(x.f2), FALSE); EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), 0); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 0);
    CHECK_HR(gpe(&x, 0, 0, 0, c), E_NOCOLOR_); CHECK_HR(gpe(&x, 0, 0, 1, c), E_NOCOLOR_); CHECK_HR(gpe(&x, 5, 5, 5, c), E_NOCOLOR_);
    EXPECT_EQ(crb_face_formats(&x), F_TT);
    crb_close(&x);
}
T(palette_queries_agree_between_face_interfaces) {
    static const uint32_t rec[2] = { 0xFFFF0000u, 0xFF00FF00u }; static const unsigned st[1] = { 0 };
    size_t n; uint8_t *b = font_cpal(0, 2, 1, st, 2, rec, 0, &n); CRB_NEED(x, b, n); NEED_FACE2();
    IDWriteFontFace3 *f3 = NULL; IDWriteFontFace5 *f5 = NULL;
    IDWriteFontFace_QueryInterface(x.h.face, &IID_IDWriteFontFace3, (void **)&f3); IDWriteFontFace_QueryInterface(x.h.face, &IID_IDWriteFontFace5, (void **)&f5);
    EXPECT_EQ((void *)x.f2, (void *)x.h.face); if (f3) EXPECT_EQ((void *)f3, (void *)x.h.face); if (f5) EXPECT_EQ((void *)f5, (void *)x.h.face);
    if (f3) { DWRITE_COLOR_F c[2]; CHECK_HR_OK(IDWriteFontFace3_GetPaletteEntries(f3, 0, 0, 2, c)); CHECK(c[0].r == 1.0f && c[1].g == 1.0f); EXPECT_EQ(IDWriteFontFace3_GetColorPaletteCount(f3), 1); }
    DW_RELEASE(f3); DW_RELEASE(f5); crb_close(&x);
}
