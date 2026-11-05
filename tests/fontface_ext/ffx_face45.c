/* IDWriteFontFace4 (glyph image formats / data) and IDWriteFontFace5 (variations, font resource, Equals).
 *   GetGlyphImageFormats(glyph, ppemFirst, ppemLast, *formats) (dwrite.dll!0xdc920): *formats = 0, S_OK; ORs the formats of the glyph's entries whose
 *       pixelsPerEm lies in [first, last] or is 0.
 *   GetGlyphImageFormats() (!0x924e0): face-wide byte.
 *   GetGlyphImageData (!0xdc780): *context = NULL and *data zeroed first; format must be exactly one of SVG(8), PNG(0x10), JPEG(0x20), TIFF(0x40),
 *       PREMULTIPLIED_B8G8R8A8(0x80), else E_INVALIDARG; otherwise S_OK (data stays zeroed when the glyph has no such image).
 *   GetFontAxisValueCount (!0xdc180) / GetFontAxisValues (!0xdc1e0): values zeroed first; valueCount < count -> E_NOT_SUFFICIENT_BUFFER (0x8007007A).
 *   HasVariations (!0x2a520), GetFontResource (!0xd190), Equals (!0xdbd40: NULL -> FALSE). */
#include "ffx.h"

#define EINVAL ((HRESULT)0x80070057)
#define NSB ((HRESULT)0x8007007A)
#define FMT_TT DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE
#define FMT_CFF DWRITE_GLYPH_IMAGE_FORMATS_CFF
#define FMT_COLR DWRITE_GLYPH_IMAGE_FORMATS_COLR
#define FMT_SVG DWRITE_GLYPH_IMAGE_FORMATS_SVG

static HRESULT gif(ffx *x, UINT16 g, UINT32 a, UINT32 b, DWRITE_GLYPH_IMAGE_FORMATS *f) { return x->f4->lpVtbl->GetGlyphImageFormats_(x->f4, g, a, b, f); }
static DWRITE_GLYPH_IMAGE_FORMATS gif_face(ffx *x) { return x->f4->lpVtbl->GetGlyphImageFormats(x->f4); }
static HRESULT gid(ffx *x, UINT16 g, UINT32 ppem, UINT32 fmt, DWRITE_GLYPH_IMAGE_DATA *d, void **ctx) { return IDWriteFontFace4_GetGlyphImageData(x->f4, g, ppem, (DWRITE_GLYPH_IMAGE_FORMATS)fmt, d, ctx); }

/* ---- image formats -------------------------------------------------------------------------------------------------- */
TEST(gif_face_wide_truetype_outline_font) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    DWRITE_GLYPH_IMAGE_FORMATS f = gif_face(&x);
    CHECK(f & FMT_TT); CHECK(!(f & FMT_CFF)); CHECK(!(f & FMT_COLR)); CHECK(!(f & FMT_SVG));
    EXPECT_EQ(f & ~(UINT32)FMT_TT, 0);
    ffx_close(&x);
}
TEST(gif_face_wide_color_font_has_colr) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_GLYPH_IMAGE_FORMATS f = gif_face(&x);
    CHECK(f & FMT_TT); CHECK(f & FMT_COLR); CHECK(!(f & FMT_CFF));
    t_note("COLR font face-wide formats 0x%X", f);
    ffx_close(&x);
}
TEST(gif_face_wide_repeatable_and_same_for_simulations) {
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE) : 0;
    if (oa && ob) { EXPECT_EQ(gif_face(&a), gif_face(&a)); EXPECT_EQ(gif_face(&a), gif_face(&b)); }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(gif_glyph_zeroes_output_and_succeeds) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT16 g[] = { 0, 1, 2, 28, 64, 65, 66, 67, 68 }; int i;
    for (i = 0; i < 9; i++) {
        DWRITE_GLYPH_IMAGE_FORMATS f = (DWRITE_GLYPH_IMAGE_FORMATS)0xCCCCCCCC;
        HRESULT hr = gif(&x, g[i], 0, 0xFFFFFFFFu, &f);
        t_check(hr == S_OK && (f & ~gif_face(&x)) == 0, __FILE__, __LINE__, "glyph formats", "glyph %u: %s 0x%X (face 0x%X)", g[i], dw_hr_name(hr), f, gif_face(&x));
        t_note("glyph %u image formats 0x%X", g[i], f);
    }
    ffx_close(&x);
}
TEST(gif_glyph_ppem_range_variants) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const UINT32 ranges[][2] = { { 0, 0 }, { 1, 1 }, { 16, 16 }, { 0, 0xFFFFFFFFu }, { 100, 50 }, { 0xFFFFFFFFu, 0xFFFFFFFFu }, { 8, 72 } };
    int i;
    for (i = 0; i < 7; i++) {
        DWRITE_GLYPH_IMAGE_FORMATS f = (DWRITE_GLYPH_IMAGE_FORMATS)0xCCCCCCCC;
        HRESULT hr = gif(&x, 2, ranges[i][0], ranges[i][1], &f);
        t_check(hr == S_OK && (f & ~0xFFu) == 0, __FILE__, __LINE__, "ppem range", "[%u,%u]: %s 0x%X", ranges[i][0], ranges[i][1], dw_hr_name(hr), f);
    }
    ffx_close(&x);
}
TEST(gif_glyph_color_font_subset_of_face) {
    ttf_spec s; ffx_spec_color(&s);
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    int g;
    for (g = 0; g < 10; g++) {
        DWRITE_GLYPH_IMAGE_FORMATS f = 0; HRESULT hr = gif(&x, (UINT16)g, 0, 0xFFFFFFFFu, &f);
        t_check(hr == S_OK && (f & ~gif_face(&x)) == 0, __FILE__, __LINE__, "subset", "glyph %d: 0x%X", g, f);
        t_note("COLR font glyph %d formats 0x%X", g, f);
    }
    ffx_close(&x);
}
TEST(gif_system_arial) {
    ffx x; if (!ffx_open_sys(&x, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    DWRITE_GLYPH_IMAGE_FORMATS f = gif_face(&x);
    CHECK(f & FMT_TT); CHECK(!(f & (FMT_CFF | FMT_COLR | FMT_SVG)));
    UINT32 cp = 'A'; UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(x.f0, &cp, 1, &g);
    DWRITE_GLYPH_IMAGE_FORMATS gf = 0; CHECK_HR_OK(gif(&x, g, 0, 0xFFFFFFFFu, &gf));
    t_note("Arial face-wide 0x%X, glyph 'A' 0x%X", f, gf);
    CHECK((gf & ~f) == 0);
    ffx_close(&x);
}
TEST(gif_system_segoe_emoji) {
    ffx x; if (!ffx_open_sys(&x, L"Segoe UI Emoji", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    DWRITE_GLYPH_IMAGE_FORMATS f = gif_face(&x);
    t_note("Segoe UI Emoji face-wide formats 0x%X", f);
    CHECK(f & FMT_TT);
    if (IDWriteFontFace2_IsColorFont(x.f2)) CHECK(f & FMT_COLR);
    UINT32 cp = 0x1F600; UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(x.f0, &cp, 1, &g);
    if (g) { DWRITE_GLYPH_IMAGE_FORMATS gf = 0; CHECK_HR_OK(gif(&x, g, 0, 0xFFFFFFFFu, &gf)); t_note("U+1F600 glyph %u formats 0x%X", g, gf); CHECK((gf & ~f) == 0); }
    ffx_close(&x);
}

/* ---- GetGlyphImageData ---------------------------------------------------------------------------------------------- */
TEST(gid_invalid_formats_zero_everything) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const UINT32 bad[] = { 0, 1, 2, 4, 3, 5, 0x18, 0x30, 0x88, 0xF8, 0x100, 0x1FF, 0x200, 0x8000, 0x80000000u, 0xFFFFFFFFu, 12, 24, 0x41 };
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)0x1234; memset(&d, 0xCC, sizeof d);
        HRESULT hr = gid(&x, 2, 16, bad[i], &d, &ctx);
        static const char z[sizeof d] = { 0 };
        t_check(hr == EINVAL, __FILE__, __LINE__, "bad format", "format 0x%X -> %s", bad[i], dw_hr_name(hr));
        t_check(ctx == NULL && !memcmp(&d, z, sizeof d), __FILE__, __LINE__, "bad format zero", "format 0x%X: ctx %p data not zeroed", bad[i], ctx);
    }
    ffx_close(&x);
}
TEST(gid_valid_formats_on_outline_font) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const UINT32 ok[] = { DWRITE_GLYPH_IMAGE_FORMATS_SVG, DWRITE_GLYPH_IMAGE_FORMATS_PNG, DWRITE_GLYPH_IMAGE_FORMATS_JPEG, DWRITE_GLYPH_IMAGE_FORMATS_TIFF, DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8 };
    int i;
    for (i = 0; i < 5; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)0x1234; memset(&d, 0xCC, sizeof d);
        HRESULT hr = gid(&x, 2, 16, ok[i], &d, &ctx);
        t_check(hr == S_OK, __FILE__, __LINE__, "valid format", "format 0x%X -> %s", ok[i], dw_hr_name(hr));
        if (hr == S_OK) {
            t_check((d.imageData == NULL) == (d.imageDataSize == 0), __FILE__, __LINE__, "empty consistency", "format 0x%X: data %p size %u", ok[i], d.imageData, d.imageDataSize);
            t_check(d.imageData != NULL || ctx == NULL, __FILE__, __LINE__, "ctx", "format 0x%X: ctx %p without data", ok[i], ctx);
            t_note("format 0x%X glyph 2: data %p size %u ctx %p ppem %u", ok[i], d.imageData, d.imageDataSize, ctx, d.pixelsPerEm);
            if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        }
    }
    ffx_close(&x);
}
TEST(gid_data_struct_layout) {
    EXPECT_EQ(sizeof(DWRITE_GLYPH_IMAGE_DATA), 64);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, imageDataSize), 8);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, uniqueDataId), 12);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, pixelsPerEm), 16);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, pixelSize), 20);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, horizontalLeftOrigin), 28);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, verticalBottomOrigin), 52);
}

/* A font with an 'SVG ' table: glyph 2..3 -> one SVG document. */
static const char svg_doc[] = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 100\"><rect id=\"glyph2\" width=\"80\" height=\"80\"/></svg>";
static uint8_t g_svg_tbl[14 + 12 + 256];
static unsigned build_svg_table(void) {
    unsigned n = 0, dl = (unsigned)sizeof svg_doc - 1;
    memset(g_svg_tbl, 0, sizeof g_svg_tbl);
    ffx_put16(g_svg_tbl, 0, 0);                                   /* version */
    g_svg_tbl[4] = 0; g_svg_tbl[5] = 0; g_svg_tbl[6] = 0; g_svg_tbl[7] = 10;      /* svgDocumentListOffset = 10 */
    ffx_put16(g_svg_tbl, 10, 1);                                  /* numEntries */
    ffx_put16(g_svg_tbl, 12, 2); ffx_put16(g_svg_tbl, 14, 3);     /* start/end glyph */
    g_svg_tbl[19] = 14;                                           /* svgDocOffset (from the document list) = 2 + 12 */
    g_svg_tbl[22] = (uint8_t)(dl >> 8); g_svg_tbl[23] = (uint8_t)dl;
    memcpy(g_svg_tbl + 24, svg_doc, dl);
    n = 24 + dl;
    return n;
}
TEST(gid_svg_table_font) {
    unsigned n = build_svg_table();
    ttf_rawtable raw[1] = { { "SVG ", g_svg_tbl, n } };
    ttf_spec s; ttf_default_spec(&s); s.raw = raw; s.nraw = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_GLYPH_IMAGE_FORMATS face = gif_face(&x), g2 = 0, g4 = 0;
    CHECK_HR_OK(gif(&x, 2, 0, 0xFFFFFFFFu, &g2)); CHECK_HR_OK(gif(&x, 4, 0, 0xFFFFFFFFu, &g4));
    t_note("SVG table font: face-wide 0x%X, glyph 2 0x%X, glyph 4 0x%X", face, g2, g4);
    CHECK((g2 & ~face) == 0 && (g4 & ~face) == 0);
    DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL;
    CHECK_HR_OK(gid(&x, 2, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &d, &ctx));
    t_note("SVG glyph 2: data %p size %u unique %u ppem %u pixelSize %ux%u", d.imageData, d.imageDataSize, d.uniqueDataId, d.pixelsPerEm, d.pixelSize.width, d.pixelSize.height);
    if (d.imageData) {
        EXPECT_EQ(d.imageDataSize, sizeof svg_doc - 1);
        CHECK(d.imageDataSize == sizeof svg_doc - 1 && !memcmp(d.imageData, svg_doc, sizeof svg_doc - 1));
        CHECK(g2 & FMT_SVG);
        CHECK(face & FMT_SVG);
        CHECK(ctx != NULL);
    } else t_note("no SVG image data returned for glyph 2");
    if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    /* a glyph outside the SVG document range has no SVG image */
    void *ctx2 = (void *)(ULONG_PTR)7; DWRITE_GLYPH_IMAGE_DATA d2;
    CHECK_HR_OK(gid(&x, 10, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &d2, &ctx2));
    CHECK(d2.imageData == NULL && d2.imageDataSize == 0 && ctx2 == NULL);
    EXPECT_EQ(g4 & FMT_SVG, 0);
    ffx_close(&x);
}
TEST(gid_svg_range_covers_both_glyphs) {
    unsigned n = build_svg_table();
    ttf_rawtable raw[1] = { { "SVG ", g_svg_tbl, n } };
    ttf_spec s; ttf_default_spec(&s); s.raw = raw; s.nraw = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_GLYPH_IMAGE_DATA a, b; void *ca = NULL, *cb = NULL;
    CHECK_HR_OK(gid(&x, 2, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &a, &ca)); CHECK_HR_OK(gid(&x, 3, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &b, &cb));
    if (a.imageData && b.imageData) { EXPECT_EQ(a.imageDataSize, b.imageDataSize); CHECK(a.uniqueDataId == b.uniqueDataId); }
    if (ca) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ca);
    if (cb) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, cb);
    ffx_close(&x);
}
TEST(gid_release_after_face_stays_valid) {
    unsigned n = build_svg_table();
    ttf_rawtable raw[1] = { { "SVG ", g_svg_tbl, n } };
    ttf_spec s; ttf_default_spec(&s); s.raw = raw; s.nraw = 1;
    ffx x; if (!ffx_open_spec(&x, &s, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); return; }
    DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; CHECK_HR_OK(gid(&x, 2, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &d, &ctx));
    IDWriteFontFace4 *keep = x.f4; IDWriteFontFace4_AddRef(keep);
    ffx_close(&x);            /* the extra reference keeps face (and file stream) alive; the loader stays registered until the face goes */
    t_note("image data held across ffx_close: data %p", d.imageData);
    if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(keep, ctx);
    IDWriteFontFace4_Release(keep);
}
TEST(gid_system_emoji_note) {
    ffx x; if (!ffx_open_sys(&x, L"Segoe UI Emoji", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 cp = 0x1F600; UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(x.f0, &cp, 1, &g);
    if (g) {
        static const UINT32 fm[] = { DWRITE_GLYPH_IMAGE_FORMATS_SVG, DWRITE_GLYPH_IMAGE_FORMATS_PNG, DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8 };
        int i;
        for (i = 0; i < 3; i++) {
            DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; HRESULT hr = gid(&x, g, 96, fm[i], &d, &ctx);
            CHECK_HR_OK(hr);
            t_note("Segoe UI Emoji U+1F600 format 0x%X: data %p size %u", fm[i], d.imageData, d.imageDataSize);
            if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        }
    }
    ffx_close(&x);
}

/* ---- variations (FontFace5) -------------------------------------------------------------------------------------------- */
static HRESULT axv(ffx *x, DWRITE_FONT_AXIS_VALUE *v, UINT32 n) { return IDWriteFontFace5_GetFontAxisValues(x->f5, v, n); }
TEST(var_synthetic_font_has_no_variations) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace5_HasVariations(x.f5), FALSE);
    ffx_close(&x);
}
TEST(var_axis_count_and_values_consistent_static_font) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(x.f5);
    t_note("static synthetic font axis value count: %u", n);
    DWRITE_FONT_AXIS_VALUE v[16]; memset(v, 0xCC, sizeof v);
    if (n <= 16) {
        CHECK_HR_OK(axv(&x, v, n));
    }
    if (n > 0) {
        DWRITE_FONT_AXIS_VALUE one; memset(&one, 0xCC, sizeof one);
        CHECK_HR(axv(&x, &one, n - 1), NSB);
        CHECK_HR(axv(&x, NULL, 0), NSB);
        EXPECT_EQ(one.axisTag, 0); CHECK(one.value == 0.0f);          /* zeroed first */
    } else {
        CHECK_HR_OK(axv(&x, NULL, 0));
    }
    ffx_close(&x);
}
TEST(var_axis_values_zero_fill_when_buffer_larger) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(x.f5);
    DWRITE_FONT_AXIS_VALUE v[12]; memset(v, 0xCC, sizeof v);
    CHECK_HR_OK(axv(&x, v, 12));
    UINT32 i; int bad = 0; for (i = n; i < 12; i++) if (v[i].axisTag != 0 || v[i].value != 0.0f) bad++;
    EXPECT_EQ(bad, 0);       /* the whole buffer is zeroed before the real values are converted into it */
    ffx_close(&x);
}
static int fvar_find(const uint8_t *fv, UINT32 len, UINT32 tag_le, float *mn, float *df, float *mx, int *naxes) {
    if (len < 16) return 0;
    unsigned off = ffx_getu16(fv, 4), na = ffx_getu16(fv, 8), sz = ffx_getu16(fv, 10), i;
    *naxes = (int)na;
    for (i = 0; i < na; i++) {
        const uint8_t *a = fv + off + i * sz; if (off + (i + 1) * sz > len) return 0;
        UINT32 t = (UINT32)a[3] << 24 | (UINT32)a[2] << 16 | (UINT32)a[1] << 8 | a[0];     /* tag bytes 'wght' -> little-endian DWRITE tag */
        t = (UINT32)a[0] | (UINT32)a[1] << 8 | (UINT32)a[2] << 16 | (UINT32)a[3] << 24;
        if (t == tag_le) {
            int32_t m = (int32_t)((UINT32)a[4] << 24 | a[5] << 16 | a[6] << 8 | a[7]), d = (int32_t)((UINT32)a[8] << 24 | a[9] << 16 | a[10] << 8 | a[11]), M = (int32_t)((UINT32)a[12] << 24 | a[13] << 16 | a[14] << 8 | a[15]);
            *mn = m / 65536.0f; *df = d / 65536.0f; *mx = M / 65536.0f; return 1;
        }
    }
    return 0;
}
TEST(var_bahnschrift_has_variations_and_axes) {
    ffx x; if (!ffx_open_sys(&x, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace5_HasVariations(x.f5), TRUE);
    UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(x.f5);
    t_note("Bahnschrift axis value count %u", n);
    CHECK(n >= 2);
    DWRITE_FONT_AXIS_VALUE v[16]; UINT32 i; int hw = 0, hd = 0;
    if (n <= 16) {
        CHECK_HR_OK(axv(&x, v, n));
        for (i = 0; i < n; i++) {
            t_note("axis %u: tag %c%c%c%c value %g", i, (char)(v[i].axisTag & 255), (char)((v[i].axisTag >> 8) & 255), (char)((v[i].axisTag >> 16) & 255), (char)(v[i].axisTag >> 24), v[i].value);
            if (v[i].axisTag == DWRITE_FONT_AXIS_TAG_WEIGHT) hw = 1;
            if (v[i].axisTag == DWRITE_FONT_AXIS_TAG_WIDTH) hd = 1;
        }
        CHECK(hw); CHECK(hd);
        UINT32 fl = 0; uint8_t *fv = dw_face_table(x.f0, DW_TAG('f','v','a','r'), &fl);
        for (i = 0; i < n && fv; i++) {
            float mn, df, mx; int na;
            if (fvar_find(fv, fl, v[i].axisTag, &mn, &df, &mx, &na))
                t_check(v[i].value >= mn - 0.01f && v[i].value <= mx + 0.01f, __FILE__, __LINE__, "axis range", "axis %u value %g outside fvar [%g, %g]", i, v[i].value, mn, mx);
        }
        free(fv);
    }
    ffx_close(&x);
}
TEST(var_bahnschrift_buffer_protocol) {
    ffx x; if (!ffx_open_sys(&x, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(x.f5);
    if (n < 2 || n > 14) { t_skip("axis count %u", n); ffx_close(&x); return; }
    DWRITE_FONT_AXIS_VALUE v[16]; memset(v, 0xCC, sizeof v);
    CHECK_HR(axv(&x, v, n - 1), NSB);
    UINT32 i; int bad = 0; for (i = 0; i < n - 1; i++) if (v[i].axisTag != 0 || v[i].value != 0.0f) bad++;
    EXPECT_EQ(bad, 0);                    /* all requested slots are zeroed even though nothing is returned */
    memset(v, 0xCC, sizeof v); CHECK_HR(axv(&x, NULL, 0), NSB);
    memset(v, 0xCC, sizeof v); CHECK_HR(axv(&x, v, 0), NSB);
    memset(v, 0xCC, sizeof v);
    CHECK_HR_OK(axv(&x, v, n + 2));
    for (i = n, bad = 0; i < n + 2; i++) if (v[i].axisTag != 0 || v[i].value != 0.0f) bad++;
    EXPECT_EQ(bad, 0);
    CHECK(v[0].axisTag != 0);
    ffx_close(&x);
}
TEST(var_bahnschrift_weight_bold_instance) {
    ffx x; if (!ffx_open_sys(&x, L"Bahnschrift", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(x.f5); DWRITE_FONT_AXIS_VALUE v[16]; UINT32 i;
    if (n == 0 || n > 16) { t_skip("axis count %u", n); ffx_close(&x); return; }
    CHECK_HR_OK(axv(&x, v, n));
    for (i = 0; i < n; i++) if (v[i].axisTag == DWRITE_FONT_AXIS_TAG_WEIGHT) {
        t_note("Bahnschrift bold instance wght = %g, IDWriteFontFace3 weight %d", v[i].value, (int)IDWriteFontFace3_GetWeight(x.f3));
        CHECK(v[i].value >= 600.0f && v[i].value <= 800.0f);
    }
    ffx_close(&x);
}
TEST(var_static_system_fonts_have_no_variations) {
    static const WCHAR *fam[] = { L"Arial", L"Times New Roman", L"Courier New", L"Consolas", L"Tahoma", L"MS Gothic" };
    int i;
    for (i = 0; i < 6; i++) {
        ffx x; if (!ffx_open_sys(&x, fam[i], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); continue; }
        t_check(IDWriteFontFace5_HasVariations(x.f5) == FALSE, __FILE__, __LINE__, "static", "%ls reports variations", fam[i]);
        UINT32 fl = 0; uint8_t *fv = dw_face_table(x.f0, DW_TAG('f','v','a','r'), &fl);
        t_check(fv == NULL, __FILE__, __LINE__, "fvar", "%ls has an fvar table", fam[i]);
        t_note("%ls axis value count %u", fam[i], IDWriteFontFace5_GetFontAxisValueCount(x.f5));
        free(fv); ffx_close(&x);
    }
}
TEST(var_has_variations_clean_bool) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    BOOL r = IDWriteFontFace5_HasVariations(x.f5); CHECK(r == 0);
    ffx_close(&x);
    if (!ffx_open_sys(&x, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    r = IDWriteFontFace5_HasVariations(x.f5); CHECK(r == 1);
    ffx_close(&x);
}

/* ---- font resource -------------------------------------------------------------------------------------------------------- */
TEST(res_get_font_resource_basics) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontResource *r = (IDWriteFontResource *)(ULONG_PTR)1;
    HRESULT hr = IDWriteFontFace5_GetFontResource(x.f5, &r);
    CHECK_HR_OK(hr);
    if (hr == S_OK && r) {
        EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(r), 0);
        EXPECT_EQ(IDWriteFontResource_HasVariations(r), FALSE);
        IDWriteFontFile *ff = NULL; CHECK_HR_OK(IDWriteFontResource_GetFontFile(r, &ff)); CHECK(ff != NULL); DW_RELEASE(ff);
        t_note("static font resource: axis count %u", IDWriteFontResource_GetFontAxisCount(r));
        IDWriteFontResource_Release(r);
    }
    ffx_close(&x);
}
TEST(res_face_index_follows_ttc_member) {
    size_t n1, n2; uint8_t *b1 = ttf_build_styled("TtcA", 400, 5, 0, &n1), *b2 = ttf_build_styled("TtcB", 700, 5, 0, &n2);
    const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 }; size_t tn;
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &tn);
    free(b1); free(b2);
    if (!ttc) { t_skip("no ttc"); return; }
    ffx x;
    if (!ffx_open_blob(&x, ttc, tn, 1, DWRITE_FONT_SIMULATIONS_NONE)) { ffx_close(&x); free(ttc); return; }
    EXPECT_EQ(IDWriteFontFace5_GetIndex(x.f5), 1);
    IDWriteFontResource *r = NULL; CHECK_HR_OK(IDWriteFontFace5_GetFontResource(x.f5, &r));
    if (r) { EXPECT_EQ(IDWriteFontResource_GetFontFaceIndex(r), 1); IDWriteFontResource_Release(r); }
    IDWriteLocalizedStrings *nm = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.f3, &nm));
    if (nm) { char b[64]; dw_ls_en(nm, b, sizeof b); EXPECT_STR(b, "TtcB"); IDWriteLocalizedStrings_Release(nm); }
    EXPECT_EQ(IDWriteFontFace3_GetWeight(x.f3), DWRITE_FONT_WEIGHT_BOLD);
    ffx_close(&x); free(ttc);
}
TEST(res_bahnschrift_axes_match_fvar) {
    ffx x; if (!ffx_open_sys(&x, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    IDWriteFontResource *r = NULL; CHECK_HR_OK(IDWriteFontFace5_GetFontResource(x.f5, &r));
    UINT32 fl = 0; uint8_t *fv = dw_face_table(x.f0, DW_TAG('f','v','a','r'), &fl);
    if (r && fv && fl >= 16) {
        UINT32 na = IDWriteFontResource_GetFontAxisCount(r);
        /* DirectWrite also reports registered axes (ital/slnt) that fvar does not carry: fvar axes are a subset */
        t_check(na >= ffx_getu16(fv, 8), __FILE__, __LINE__, "axis count", "dwrite %u fvar %u", na, ffx_getu16(fv, 8));
        EXPECT_EQ(IDWriteFontResource_HasVariations(r), TRUE);
        if (na > 0 && na <= 8) {
            DWRITE_FONT_AXIS_VALUE def[8]; DWRITE_FONT_AXIS_RANGE rg[8]; UINT32 i;
            CHECK_HR_OK(IDWriteFontResource_GetDefaultFontAxisValues(r, def, na));
            CHECK_HR_OK(IDWriteFontResource_GetFontAxisRanges(r, rg, na));
            for (i = 0; i < na; i++) {
                float mn, df, mx; int n2;
                if (fvar_find(fv, fl, def[i].axisTag, &mn, &df, &mx, &n2)) {
                    t_check(fabsf(def[i].value - df) < 0.01f, __FILE__, __LINE__, "default", "axis %u default %g fvar %g", i, def[i].value, df);
                    t_check(rg[i].axisTag == def[i].axisTag && fabsf(rg[i].minValue - mn) < 0.01f && fabsf(rg[i].maxValue - mx) < 0.01f, __FILE__, __LINE__, "range", "axis %u [%g,%g] fvar [%g,%g]", i, rg[i].minValue, rg[i].maxValue, mn, mx);
                } else t_check(def[i].axisTag == DWRITE_FONT_AXIS_TAG_ITALIC || def[i].axisTag == DWRITE_FONT_AXIS_TAG_SLANT, __FILE__, __LINE__, "fvar", "axis tag %08X missing in fvar", def[i].axisTag);
                t_note("axis %u: tag 0x%08X default %g range [%g, %g]", i, def[i].axisTag, def[i].value, rg[i].minValue, rg[i].maxValue);
            }
        }
    }
    free(fv); DW_RELEASE(r); ffx_close(&x);
}
TEST(res_create_face_with_axis_values_round_trips) {
    ffx x; if (!ffx_open_sys(&x, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    IDWriteFontResource *r = NULL; CHECK_HR_OK(IDWriteFontFace5_GetFontResource(x.f5, &r));
    UINT32 fl = 0; uint8_t *fv = dw_face_table(x.f0, DW_TAG('f','v','a','r'), &fl);
    float mn = 0, df = 0, mx = 0; int na = 0;
    if (r && fv && fvar_find(fv, fl, DWRITE_FONT_AXIS_TAG_WEIGHT, &mn, &df, &mx, &na)) {
        float want = (mn + mx) / 2.0f;
        DWRITE_FONT_AXIS_VALUE av[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, want } };
        IDWriteFontFace5 *nf = NULL; HRESULT hr = IDWriteFontResource_CreateFontFace(r, DWRITE_FONT_SIMULATIONS_NONE, av, 1, &nf);
        CHECK_HR_OK(hr);
        if (nf) {
            EXPECT_EQ(IDWriteFontFace5_HasVariations(nf), TRUE);
            UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(nf); DWRITE_FONT_AXIS_VALUE v[16]; UINT32 i; int found = 0;
            if (n > 0 && n <= 16 && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(nf, v, n)))
                for (i = 0; i < n; i++) if (v[i].axisTag == DWRITE_FONT_AXIS_TAG_WEIGHT) { found = 1; t_check(fabsf(v[i].value - want) < 0.01f, __FILE__, __LINE__, "wght", "got %g want %g", v[i].value, want); }
            CHECK(found);
            EXPECT_EQ(IDWriteFontFace5_Equals(nf, (IDWriteFontFace *)x.f5), FALSE);       /* different instance */
            IDWriteFontFace5_Release(nf);
        }
    }
    free(fv); DW_RELEASE(r); ffx_close(&x);
}
TEST(res_create_face_default_axes_equals_original) {
    ffx x; if (!ffx_open_sys(&x, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL)) { ffx_close(&x); return; }
    UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(x.f5); DWRITE_FONT_AXIS_VALUE v[16];
    IDWriteFontResource *r = NULL; CHECK_HR_OK(IDWriteFontFace5_GetFontResource(x.f5, &r));
    if (r && n > 0 && n <= 16 && SUCCEEDED(IDWriteFontFace5_GetFontAxisValues(x.f5, v, n))) {
        IDWriteFontFace5 *nf = NULL;
        CHECK_HR_OK(IDWriteFontResource_CreateFontFace(r, DWRITE_FONT_SIMULATIONS_NONE, v, n, &nf));
        if (nf) { EXPECT_EQ(IDWriteFontFace5_Equals(nf, (IDWriteFontFace *)x.f5), TRUE); IDWriteFontFace5_Release(nf); }
    }
    DW_RELEASE(r); ffx_close(&x);
}

/* ---- Equals ---------------------------------------------------------------------------------------------------------------- */
TEST(eq_self_true_and_null_false) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace5_Equals(x.f5, x.f0), TRUE);
    EXPECT_EQ(IDWriteFontFace5_Equals(x.f5, (IDWriteFontFace *)x.f5), TRUE);
    EXPECT_EQ(IDWriteFontFace5_Equals(x.f5, NULL), FALSE);
    ffx_close(&x);
}
TEST(eq_same_file_same_parameters_equal) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontFace *again = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.h.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &x.h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &again));
    if (again) {
        EXPECT_EQ(IDWriteFontFace5_Equals(x.f5, again), TRUE);
        t_note("second CreateFontFace for the same file returned %s object", FFX_SAME(again, x.f0) ? "the same" : "a different");
        IDWriteFontFace_Release(again);
    }
    ffx_close(&x);
}
TEST(eq_different_simulations) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    IDWriteFontFace *bold = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateFontFace(x.h.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &x.h.file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &bold));
    if (bold) {
        BOOL e = IDWriteFontFace5_Equals(x.f5, bold);
        t_note("regular vs bold-simulated face of the same file: Equals=%d", e);
        CHECK(e == 0 || e == 1);
        IDWriteFontFace5 *b5 = NULL; IDWriteFontFace_QueryInterface(bold, &IID_IDWriteFontFace5, (void **)&b5);
        if (b5) { EXPECT_EQ(IDWriteFontFace5_Equals(b5, bold), TRUE); EXPECT_EQ(IDWriteFontFace5_Equals(b5, x.f0), e); IDWriteFontFace5_Release(b5); }
        IDWriteFontFace_Release(bold);
    }
    ffx_close(&x);
}
TEST(eq_different_fonts_not_equal) {
    ttf_spec s1, s2; ttf_style_spec(&s1, "EqOne", 400, 5, 0); ttf_style_spec(&s2, "EqTwo", 400, 5, 0);
    ffx a, b; int oa = ffx_open_spec(&a, &s1, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s2, DWRITE_FONT_SIMULATIONS_NONE) : 0;
    if (oa && ob) { EXPECT_EQ(IDWriteFontFace5_Equals(a.f5, b.f0), FALSE); EXPECT_EQ(IDWriteFontFace5_Equals(b.f5, a.f0), FALSE); }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(eq_same_bytes_different_loaders_note) {
    ttf_spec s; ttf_default_spec(&s);
    ffx a, b; int oa = ffx_open_spec(&a, &s, DWRITE_FONT_SIMULATIONS_NONE), ob = oa ? ffx_open_spec(&b, &s, DWRITE_FONT_SIMULATIONS_NONE) : 0;
    if (oa && ob) { BOOL e = IDWriteFontFace5_Equals(a.f5, b.f0); t_note("identical bytes through two loaders: Equals=%d", e); CHECK(e == 0 || e == 1); EXPECT_EQ(IDWriteFontFace5_Equals(b.f5, a.f0), e); }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
}
TEST(eq_system_faces) {
    ffx a, b, c; int oa = ffx_open_sys(&a, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    int ob = oa ? ffx_open_sys(&b, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : 0;
    int oc = ob ? ffx_open_sys(&c, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : 0;
    if (oa && ob && oc) {
        EXPECT_EQ(IDWriteFontFace5_Equals(a.f5, b.f0), TRUE);           /* two lookups of the same system font */
        EXPECT_EQ(IDWriteFontFace5_Equals(a.f5, c.f0), FALSE);           /* Arial vs Arial Bold */
        EXPECT_EQ(IDWriteFontFace5_Equals(c.f5, b.f0), FALSE);
    }
    if (oa) ffx_close(&a);
    if (ob) ffx_close(&b);
    if (oc) ffx_close(&c);
}
TEST(eq_does_not_change_refcount) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    ULONG r0 = dw_refcount((IUnknown *)x.f0);
    IDWriteFontFace5_Equals(x.f5, x.f0); IDWriteFontFace5_Equals(x.f5, NULL);
    EXPECT_REFS(x.f0, r0);
    BOOL e = IDWriteFontFace5_Equals(x.f5, x.f0); CHECK(e == 1);
    ffx_close(&x);
}
