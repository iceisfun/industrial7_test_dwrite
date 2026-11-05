/* Colour related IDWriteFontFace2/IDWriteFontFace4 behaviour on the COLR/CPAL (+SVG, +CBDT) fixtures and malformed variants.
 *   IsColorFont / GetColorPaletteCount / GetPaletteEntryCount (dwrite.dll!0x208b0 / 0x924c0 / 0x92510): cached face data.
 *   GetPaletteEntries (0xdcbe0): unknown palette -> DWRITE_E_NOCOLOR; first > count or entryCount > count - first -> E_INVALIDARG.
 *   GetGlyphImageFormats() (0x924e0) face-wide byte; GetGlyphImageFormats(glyph, ppemFirst, ppemLast, *fmt) (0xdc920): *fmt = 0 first, ORs
 *   the formats of the glyph's linked entries whose ppem is inside [first, last] (or 0 = any size).
 *   GetGlyphImageData (0xdc780): *context = NULL and the data struct zeroed; format must be one of SVG/PNG/JPEG/TIFF/PREMULTIPLIED, else E_INVALIDARG. */
#include "cg.h"

static int open_tables(cgfont *x, const uint8_t *colr, size_t nc, const uint8_t *cpal, size_t np, unsigned extra) {
    ttf_spec s; ttf_rawtable raw[4]; int nraw = 0; size_t n = 0; uint8_t *b; int ok;
    ttf_default_spec(&s);
    if (colr) { memcpy(raw[nraw].tag, "COLR", 5); raw[nraw].data = colr; raw[nraw].len = (unsigned)nc; nraw++; }
    if (cpal) { memcpy(raw[nraw].tag, "CPAL", 5); raw[nraw].data = cpal; raw[nraw].len = (unsigned)np; nraw++; }
    (void)extra; s.raw = raw; s.nraw = nraw;
    b = ttf_build(&s, &n); if (!b) { memset(x, 0, sizeof *x); t_skip("ttf_build failed"); return 0; }
    ok = cg_open_blob(x, b, n); free(b); return ok;
}
static HRESULT pal(cgfont *x, UINT32 p, UINT32 first, UINT32 n, DWRITE_COLOR_F *c) { return IDWriteFontFace2_GetPaletteEntries(x->f2, p, first, n, c); }

TEST(cf_plain_font_reports_no_color) {
    cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    EXPECT_EQ(IDWriteFontFace2_IsColorFont(x.f2), FALSE); EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), 0); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), 0);
    DWRITE_COLOR_F c[2]; CHECK_HR(pal(&x, 0, 0, 1, c), CG_E_NOCOLOR);
    cg_close(&x);
}
TEST(cf_fixture_counts) {
    CG_OPEN(x, CGFX_COLR);
    EXPECT_EQ(IDWriteFontFace2_IsColorFont(x.f2), TRUE); EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), CG_NPAL); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), CG_NENT);
    cg_close(&x);
}
TEST(cf_fixture_palette_entries_exact) {
    CG_OPEN(x, CGFX_COLR); int p, i;
    for (p = 0; p < CG_NPAL; p++) {
        DWRITE_COLOR_F c[CG_NENT]; memset(c, 0xCC, sizeof c);
        CHECK_HR_OK(pal(&x, p, 0, CG_NENT, c));
        for (i = 0; i < CG_NENT; i++) {
            uint32_t v = cg_pal[p][i];
            CHECKF(DW_APPROX(c[i].r, ((v >> 16) & 255) / 255.0f, 1e-6) && DW_APPROX(c[i].g, ((v >> 8) & 255) / 255.0f, 1e-6) && DW_APPROX(c[i].b, (v & 255) / 255.0f, 1e-6) && DW_APPROX(c[i].a, (v >> 24) / 255.0f, 1e-6),
                   "palette %d entry %d: %g %g %g %g", p, i, c[i].r, c[i].g, c[i].b, c[i].a);
        }
    }
    cg_close(&x);
}
TEST(cf_fixture_palette_ranges) {
    CG_OPEN(x, CGFX_COLR); DWRITE_COLOR_F all[CG_NENT], part[CG_NENT]; UINT32 first, n; CHECK_HR_OK(pal(&x, 1, 0, CG_NENT, all));
    for (first = 0; first <= CG_NENT; first++) for (n = 0; first + n <= CG_NENT; n++) {
        UINT32 k; memset(part, 0, sizeof part);
        HRESULT hr = pal(&x, 1, first, n, n ? part : NULL); CHECKF(hr == S_OK, "first %u n %u: %s", first, n, dw_hr_name(hr));
        for (k = 0; k < n; k++) CHECKF(!memcmp(&part[k], &all[first + k], sizeof part[0]), "first %u n %u entry %u", first, n, k);
    }
    for (first = 0; first <= CG_NENT + 2; first++) for (n = 0; n <= CG_NENT + 3; n++) {
        if (first <= CG_NENT && n <= CG_NENT - first) continue;
        DWRITE_COLOR_F c[16]; memset(c, 0x5A, sizeof c);
        HRESULT hr = pal(&x, 0, first, n > 16 ? 16 : n, c); if (n > 16) continue;
        CHECKF(hr == CG_E_INVALIDARG, "first %u n %u: %s", first, n, dw_hr_name(hr));
        unsigned char *q = (unsigned char *)c; int k, same = 1; for (k = 0; k < (int)sizeof c; k++) if (q[k] != 0x5A) same = 0; CHECKF(same, "buffer written on failure first %u n %u", first, n);
    }
    cg_close(&x);
}
TEST(cf_fixture_palette_index_out_of_range) {
    CG_OPEN(x, CGFX_COLR); static const UINT32 idx[] = { 2, 3, 255, 0xFFFF, 0x10000, 0xFFFFFFFFu }; int i; DWRITE_COLOR_F c[CG_NENT];
    for (i = 0; i < 6; i++) { memset(c, 0x5A, sizeof c); CHECKF(pal(&x, idx[i], 0, CG_NENT, c) == CG_E_NOCOLOR, "palette %u", idx[i]); CHECK(((unsigned char *)c)[0] == 0x5A); }
    CHECK_HR(pal(&x, 2, 99, 99, c), CG_E_NOCOLOR);   /* the palette is validated before the range */
    cg_close(&x);
}
TEST(cf_tables_roundtrip_through_try_get_font_table) {
    CG_OPEN(x, CGFX_COLR | CGFX_SVG | CGFX_PNG); size_t n = 0; uint8_t *blob = cg_build_fx(CGFX_COLR | CGFX_SVG | CGFX_PNG, &n); static const char *const tags[] = { "COLR", "CPAL", "SVG ", "CBLC", "CBDT" }; int i;
    for (i = 0; i < 5; i++) {
        unsigned want_len = 0; const uint8_t *want = ttf_find_table(blob, n, tags[i], &want_len); UINT32 got_len = 0; uint8_t *got = dw_face_table(x.face, DW_TAG(tags[i][0], tags[i][1], tags[i][2], tags[i][3]), &got_len);
        CHECKF(want && got && got_len == want_len && !memcmp(got, want, want_len), "%s table: got %p len %u want len %u", tags[i], (void *)got, got_len, want_len);
        free(got);
    }
    free(blob); cg_close(&x);
}
TEST(cf_face_wide_image_formats) {
    cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    EXPECT_EQ(x.f4->lpVtbl->GetGlyphImageFormats(x.f4), CGF_TT); cg_close(&x);
    if (!cg_open_fx(&x, CGFX_COLR)) { cg_close(&x); return; }
    EXPECT_EQ(x.f4->lpVtbl->GetGlyphImageFormats(x.f4), CGF_TT | CGF_COLR); cg_close(&x);
    if (!cg_open_fx(&x, CGFX_COLR | CGFX_SVG)) { cg_close(&x); return; }
    UINT32 f = x.f4->lpVtbl->GetGlyphImageFormats(x.f4); t_note("COLR+SVG face formats 0x%x", f); CHECK((f & (CGF_TT | CGF_COLR)) == (CGF_TT | CGF_COLR)); cg_close(&x);
    if (!cg_open_fx(&x, CGFX_COLR | CGFX_PNG)) { cg_close(&x); return; }
    f = x.f4->lpVtbl->GetGlyphImageFormats(x.f4); t_note("COLR+CBDT face formats 0x%x", f); CHECK((f & (CGF_TT | CGF_COLR)) == (CGF_TT | CGF_COLR)); cg_close(&x);
}
TEST(cf_per_glyph_formats_are_subset_of_face_formats) {
    CG_OPEN(x, CGFX_COLR | CGFX_SVG | CGFX_PNG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    UINT32 face = x.f4->lpVtbl->GetGlyphImageFormats(x.f4); int g; static const UINT32 pp[][2] = { { 0, 0 }, { 0, 0xFFFFFFFFu }, { 16, 16 }, { 17, 31 }, { 32, 32 }, { 100, 50 }, { 1, 1 } }; int i;
    for (g = 0; g < TTF_DEF_NGLYPHS + 3; g++) for (i = 0; i < 7; i++) {
        DWRITE_GLYPH_IMAGE_FORMATS f = (DWRITE_GLYPH_IMAGE_FORMATS)0xDEADBEEF; HRESULT hr = x.f4->lpVtbl->GetGlyphImageFormats_(x.f4, (UINT16)g, pp[i][0], pp[i][1], &f);
        CHECKF(hr == S_OK && (f & ~face) == 0, "glyph %d ppem %u..%u: %s 0x%x (face 0x%x)", g, pp[i][0], pp[i][1], dw_hr_name(hr), f, face);
    }
    cg_close(&x);
}
TEST(cf_per_glyph_formats_of_plain_and_color_glyphs) {
    CG_OPEN(x, CGFX_COLR); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    DWRITE_GLYPH_IMAGE_FORMATS f = (DWRITE_GLYPH_IMAGE_FORMATS)0xFFFFFFFF; int g;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        f = (DWRITE_GLYPH_IMAGE_FORMATS)0xFFFFFFFF; CHECK_HR_OK(x.f4->lpVtbl->GetGlyphImageFormats_(x.f4, (UINT16)g, 0, 0xFFFFFFFFu, &f));
        CHECKF(!(f & (CGF_SVG | CGF_PNG | CGF_JPEG | CGF_TIFF | CGF_PREM)), "glyph %d reports a bitmap format 0x%x", g, f);
        if (g < 4) t_note("glyph %d formats 0x%x", g, f);
    }
    cg_close(&x);
}
TEST(cf_svg_glyph_formats) {
    CG_OPEN(x, CGFX_COLR | CGFX_SVG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    if (!(x.f4->lpVtbl->GetGlyphImageFormats(x.f4) & CGF_SVG)) { t_skip("synthetic SVG table not recognised"); cg_close(&x); return; }
    static const UINT32 pp[][2] = { { 0, 0 }, { 0, 0xFFFFFFFFu }, { 8, 8 }, { 500, 20 }, { 0xFFFFFFFFu, 0xFFFFFFFFu } }; int g, i;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) for (i = 0; i < 5; i++) {
        DWRITE_GLYPH_IMAGE_FORMATS f = 0; CHECK_HR_OK(x.f4->lpVtbl->GetGlyphImageFormats_(x.f4, (UINT16)g, pp[i][0], pp[i][1], &f));
        /* SVG documents are size independent (ppem 0 entries): present for any range */
        CHECKF(((f & CGF_SVG) != 0) == (cg_svg_doc_of(g) >= 0), "glyph %d ppem %u..%u: 0x%x", g, pp[i][0], pp[i][1], f);
    }
    cg_close(&x);
}
TEST(cf_glyph_formats_output_cleared_first) {
    CG_OPEN(x, CGFX_COLR); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    DWRITE_GLYPH_IMAGE_FORMATS f = (DWRITE_GLYPH_IMAGE_FORMATS)0xFFFFFFFF;
    CHECK_HR_OK(x.f4->lpVtbl->GetGlyphImageFormats_(x.f4, 10000, 0, 100, &f));
    CHECKF((f & 0xFFFFFF00u) == 0, "unexpected high bits 0x%x for a glyph outside the font", f);
    cg_close(&x);
}
TEST(cf_svg_image_data_documents) {
    CG_OPEN(x, CGFX_COLR | CGFX_SVG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    if (!(x.f4->lpVtbl->GetGlyphImageFormats(x.f4) & CGF_SVG)) { t_skip("synthetic SVG table not recognised"); cg_close(&x); return; }
    int g;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)0x77; int doc = cg_svg_doc_of(g);
        memset(&d, 0xCC, sizeof d);
        CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(x.f4, (UINT16)g, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &d, &ctx));
        if (doc >= 0) {
            size_t l = strlen(cg_svg_docs[doc]);
            CHECKF(d.imageData != NULL && d.imageDataSize == l && !memcmp(d.imageData, cg_svg_docs[doc], l), "glyph %d: data %p size %u want %u", g, d.imageData, d.imageDataSize, (unsigned)l);
            CHECKF(ctx != NULL, "glyph %d: no context", g);
            if (g == 7) t_note("SVG glyph 7: unique id %u ppem %u pixelSize %ux%u LeftOrigin (%d,%d) RightOrigin (%d,%d) TopOrigin (%d,%d) BottomOrigin (%d,%d)", d.uniqueDataId, d.pixelsPerEm, d.pixelSize.width, d.pixelSize.height,
                                d.horizontalLeftOrigin.x, d.horizontalLeftOrigin.y, d.horizontalRightOrigin.x, d.horizontalRightOrigin.y, d.verticalTopOrigin.x, d.verticalTopOrigin.y, d.verticalBottomOrigin.x, d.verticalBottomOrigin.y);
            if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        } else {
            unsigned char *q = (unsigned char *)&d; int k, zero = 1; for (k = 0; k < (int)sizeof d; k++) if (q[k]) zero = 0;
            CHECKF(zero && ctx == NULL, "glyph %d without a document: data not zeroed (ctx %p)", g, ctx);
        }
    }
    cg_close(&x);
}
TEST(cf_svg_same_document_shares_unique_id) {
    CG_OPEN(x, CGFX_COLR | CGFX_SVG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    DWRITE_GLYPH_IMAGE_DATA a, b, c; void *ca = NULL, *cb = NULL, *cc = NULL;
    CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(x.f4, 29, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &a, &ca)); CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(x.f4, 30, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &b, &cb));
    CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(x.f4, 7, 100, DWRITE_GLYPH_IMAGE_FORMATS_SVG, &c, &cc));
    if (a.imageData && b.imageData && c.imageData) { CHECK(a.uniqueDataId == b.uniqueDataId); CHECK(a.uniqueDataId != c.uniqueDataId); CHECK(a.imageDataSize == b.imageDataSize); }
    if (ca) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ca); if (cb) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, cb); if (cc) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, cc);
    cg_close(&x);
}
TEST(cf_svg_data_ppem_independent) {
    CG_OPEN(x, CGFX_COLR | CGFX_SVG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    static const UINT32 pp[] = { 0, 1, 16, 100, 65535, 0xFFFFFFFFu }; int i; size_t l = strlen(cg_svg_docs[1]);
    for (i = 0; i < 6; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(x.f4, 7, pp[i], DWRITE_GLYPH_IMAGE_FORMATS_SVG, &d, &ctx));
        CHECKF(d.imageData && d.imageDataSize == l && !memcmp(d.imageData, cg_svg_docs[1], l), "ppem %u", pp[i]);
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    cg_close(&x);
}
TEST(cf_image_data_format_validation) {
    CG_OPEN(x, CGFX_COLR | CGFX_SVG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    UINT32 fm; static const UINT32 extra[] = { 0x100, 0x1000, 0x80000000u, 0xFFFFFFFFu, 0x10008 };
    for (fm = 0; fm < 256 + 5; fm++) {
        UINT32 v = fm < 256 ? fm : extra[fm - 256]; DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)0x77; int valid = v == 8 || v == 0x10 || v == 0x20 || v == 0x40 || v == 0x80;
        memset(&d, 0xCC, sizeof d);
        HRESULT hr = IDWriteFontFace4_GetGlyphImageData(x.f4, 7, 100, (DWRITE_GLYPH_IMAGE_FORMATS)v, &d, &ctx);
        CHECKF(hr == (valid ? S_OK : CG_E_INVALIDARG), "format 0x%x: %s", v, dw_hr_name(hr));
        if (!valid) { unsigned char *q = (unsigned char *)&d; int k, zero = 1; for (k = 0; k < (int)sizeof d; k++) if (q[k]) zero = 0; CHECKF(zero && ctx == NULL, "format 0x%x: outputs not cleared", v); }
        else if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    cg_close(&x);
}
TEST(cf_png_strikes) {
    CG_OPEN(x, CGFX_COLR | CGFX_PNG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    if (!(x.f4->lpVtbl->GetGlyphImageFormats(x.f4) & CGF_PNG)) { t_note("face formats 0x%x", x.f4->lpVtbl->GetGlyphImageFormats(x.f4)); t_skip("synthetic CBDT/CBLC not recognised"); cg_close(&x); return; }
    static const struct { UINT32 a, b; int has; } r[] = { { 16, 16, 1 }, { 32, 32, 1 }, { 0, 1000, 1 }, { 17, 31, 0 }, { 33, 1000, 0 }, { 0, 15, 0 }, { 16, 32, 1 } }; int i, g;
    for (g = 8; g <= 9; g++) for (i = 0; i < 7; i++) {
        DWRITE_GLYPH_IMAGE_FORMATS f = 0; CHECK_HR_OK(x.f4->lpVtbl->GetGlyphImageFormats_(x.f4, (UINT16)g, r[i].a, r[i].b, &f));
        CHECKF(((f & CGF_PNG) != 0) == r[i].has, "glyph %d ppem %u..%u: 0x%x", g, r[i].a, r[i].b, f);
    }
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) { if (g == 8 || g == 9) continue; DWRITE_GLYPH_IMAGE_FORMATS f = 0; CHECK_HR_OK(x.f4->lpVtbl->GetGlyphImageFormats_(x.f4, (UINT16)g, 0, 1000, &f)); CHECKF(!(f & CGF_PNG), "glyph %d has PNG", g); }
    cg_close(&x);
}
TEST(cf_png_image_data) {
    CG_OPEN(x, CGFX_COLR | CGFX_PNG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    if (!(x.f4->lpVtbl->GetGlyphImageFormats(x.f4) & CGF_PNG)) { t_skip("synthetic CBDT/CBLC not recognised"); cg_close(&x); return; }
    static const UINT32 req[] = { 16, 32, 20, 8, 100 }; int i;
    for (i = 0; i < 5; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(x.f4, 8, req[i], DWRITE_GLYPH_IMAGE_FORMATS_PNG, &d, &ctx));
        t_note("PNG glyph 8 request %u ppem: data %p size %u ppem %u pixelSize %ux%u unique %u", req[i], d.imageData, d.imageDataSize, d.pixelsPerEm, d.pixelSize.width, d.pixelSize.height, d.uniqueDataId);
        if (d.imageData) {
            CHECKF(d.pixelsPerEm == 16 || d.pixelsPerEm == 32, "ppem %u", d.pixelsPerEm);
            CHECK(d.imageDataSize == cg_png_len && !memcmp(d.imageData, cg_png_blob, cg_png_len));
            CHECK(ctx != NULL);
            if (req[i] == 16) EXPECT_EQ(d.pixelsPerEm, 16);
            if (req[i] == 32) EXPECT_EQ(d.pixelsPerEm, 32);
        }
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    cg_close(&x);
}
TEST(cf_png_wrong_format_requests) {
    CG_OPEN(x, CGFX_COLR | CGFX_PNG); if (!x.f4) { t_skip("no IDWriteFontFace4"); cg_close(&x); return; }
    static const UINT32 fm[] = { CGF_SVG, CGF_JPEG, CGF_TIFF, CGF_PREM }; int i;
    for (i = 0; i < 4; i++) { DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)1; CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(x.f4, 8, 16, (DWRITE_GLYPH_IMAGE_FORMATS)fm[i], &d, &ctx)); CHECKF(d.imageData == NULL && d.imageDataSize == 0 && ctx == NULL, "format 0x%x returned data", fm[i]); }
    cg_close(&x);
}
TEST(cf_translate_with_png_format_and_bitmap_glyph) {
    CG_NEED_F4(f); CG_OPEN(x, CGFX_COLR | CGFX_PNG); UINT32 ff = x.f4 ? x.f4->lpVtbl->GetGlyphImageFormats(x.f4) : 0;
    if (!(ff & CGF_PNG)) { t_skip("synthetic CBDT/CBLC not recognised"); cg_close(&x); return; }
    UINT16 g[] = { GID_G, GID_A, GID_H }; static cgrun r; D2D1_POINT_2F o = { 10, 20 }; IDWriteColorGlyphRunEnumerator1 *e = NULL; BOOL has; int k = 0;
    cg_mkrun(&r, x.face, 16.0f, g, 3, 2, 0, FALSE, 0);
    HRESULT hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(CGF_PNG | CGF_COLR), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
    CHECK_HR_OK(hr);
    while (e && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e, &has)) && has && k < 20) {
        const DWRITE_COLOR_GLYPH_RUN1 *c = NULL; IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e, &c);
        if (c) t_note("run %d: glyphs %u first %u fmt 0x%x pal 0x%x x %g", k, c->glyphRun.glyphCount, c->glyphRun.glyphIndices[0], c->glyphImageFormat, c->paletteIndex, c->baselineOriginX);
        if (c && k == 0) { EXPECT_EQ(c->glyphRun.glyphCount, 3); }
        k++;
    }
    /* base run + one bitmap run per glyph group G | A's two COLR layers | H */
    CHECK(k >= 4);
    DW_RELEASE(e); cg_close(&x);
}
TEST(cf_colr_without_cpal) {
    size_t nc; uint8_t *colr = cg_build_colr((const ttf_colr_base[]){ { 2, 0, 1 } }, 1, (const ttf_colr_layer[]){ { 64, 0 } }, 1, &nc); cgfont x;
    if (!open_tables(&x, colr, nc, NULL, 0, 0)) { cg_close(&x); free(colr); return; }
    t_note("COLR without CPAL: IsColorFont=%d palettes=%u entries=%u", IDWriteFontFace2_IsColorFont(x.f2), IDWriteFontFace2_GetColorPaletteCount(x.f2), IDWriteFontFace2_GetPaletteEntryCount(x.f2));
    EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), 0);
    IDWriteFactory2 *f = cg_f2(); UINT16 g = 2; cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    if (f) { CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR); CHECK_OUT_NULL(e); }
    cg_close(&x); free(colr);
}
TEST(cf_cpal_without_colr) {
    uint32_t c[2] = { 0xFF112233u, 0xFF445566u }; size_t np; uint8_t *cpal = cg_build_cpal(1, 2, c, &np); cgfont x;
    if (!open_tables(&x, NULL, 0, cpal, np, 0)) { cg_close(&x); free(cpal); return; }
    t_note("CPAL without COLR: IsColorFont=%d palettes=%u entries=%u", IDWriteFontFace2_IsColorFont(x.f2), IDWriteFontFace2_GetColorPaletteCount(x.f2), IDWriteFontFace2_GetPaletteEntryCount(x.f2));
    IDWriteFactory2 *f = cg_f2(); UINT16 g = 2; cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    if (f) { CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), CG_E_NOCOLOR); CHECK_OUT_NULL(e); }
    cg_close(&x); free(cpal);
}
TEST(cf_many_palettes) {
    enum { NP = 300, NE = 3 }; static uint32_t col[NP * NE]; int i; size_t nc, np;
    for (i = 0; i < NP * NE; i++) col[i] = 0xFF000000u | (uint32_t)((i * 7) & 255) << 16 | (uint32_t)((i / 3) & 255) << 8 | (uint32_t)(i & 255);
    uint8_t *colr = cg_build_colr((const ttf_colr_base[]){ { 2, 0, 2 } }, 1, (const ttf_colr_layer[]){ { 64, 0 }, { 65, 2 } }, 2, &nc), *cpal = cg_build_cpal(NP, NE, col, &np); cgfont x;
    if (!open_tables(&x, colr, nc, cpal, np, 0)) { cg_close(&x); free(colr); free(cpal); return; }
    EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(x.f2), NP); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(x.f2), NE);
    DWRITE_COLOR_F c[NE]; CHECK_HR_OK(pal(&x, 299, 0, NE, c)); CHECK_NEAR(c[1].b, (299 * 3 + 1) % 256 / 255.0f, 1e-5);
    CHECK_HR(pal(&x, 300, 0, NE, c), CG_E_NOCOLOR); CHECK_HR(pal(&x, 256, 0, NE, c), S_OK);
    IDWriteFactory2 *f = cg_f2(); UINT16 g = 2; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    if (f) {
        CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 299, &e)); DW_RELEASE(e);
        CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 256, &e)); DW_RELEASE(e);
        e = (void *)(ULONG_PTR)1; CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 300, &e), CG_E_NOCOLOR);
    }
    cg_close(&x); free(colr); free(cpal);
}
TEST(cf_layer_glyph_outside_font_is_enumerated_verbatim) {
    size_t nc, np; uint32_t c[1] = { 0xFF010203u }; uint8_t *colr = cg_build_colr((const ttf_colr_base[]){ { 2, 0, 2 } }, 1, (const ttf_colr_layer[]){ { 500, 0 }, { 64, 0 } }, 2, &nc), *cpal = cg_build_cpal(1, 1, c, &np); cgfont x;
    if (!open_tables(&x, colr, nc, cpal, np, 0)) { cg_close(&x); free(colr); free(cpal); return; }
    IDWriteFactory2 *f = cg_f2(); UINT16 g = 2; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
    if (f && SUCCEEDED(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e)) && e) {
        BOOL has; int k = 0; const DWRITE_COLOR_GLYPH_RUN *cr = NULL;
        while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) { IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &cr); if (cr) t_note("run %d glyph %u", k, cr->glyphRun.glyphIndices[0]); k++; }
        EXPECT_EQ(k, 3); DW_RELEASE(e);
    } else t_note("font with an out of range layer glyph was rejected or Translate failed");
    cg_close(&x); free(colr); free(cpal);
}
TEST(cf_colr_header_variants_note) {
    /* version 1 header, truncated base records, unsorted base records: dwrite either rejects the colour data or ignores the table; record the outcome */
    size_t nc, np; uint32_t c[1] = { 0xFF010203u }; uint8_t *cpal = cg_build_cpal(1, 1, c, &np);
    static const ttf_colr_base unsorted[] = { { 3, 0, 1 }, { 2, 1, 1 } }; static const ttf_colr_layer lay[] = { { 64, 0 }, { 65, 0 } };
    uint8_t *colr = cg_build_colr(unsorted, 2, lay, 2, &nc); cgfont x;
    if (open_tables(&x, colr, nc, cpal, np, 0)) {
        IDWriteFactory2 *f = cg_f2(); int gg;
        for (gg = 2; gg <= 3; gg++) { UINT16 g = (UINT16)gg; cgrun r; IDWriteColorGlyphRunEnumerator *e = (void *)(ULONG_PTR)1; cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
            HRESULT hr = f ? IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e) : E_FAIL;
            t_note("unsorted COLR base records: glyph %d -> %s", gg, dw_hr_name(hr)); if (SUCCEEDED(hr) && e) { DW_RELEASE(e); } }
    }
    cg_close(&x); free(colr);
    colr = cg_build_colr(unsorted, 2, lay, 2, &nc); colr[1] = 1;   /* version 1 */
    if (open_tables(&x, colr, nc, cpal, np, 0)) t_note("COLR version 1 header: IsColorFont=%d", IDWriteFontFace2_IsColorFont(x.f2));
    cg_close(&x); free(colr); free(cpal);
}
TEST(cf_simulated_faces_still_translate) {
    CG_NEED_F2(f); size_t n = 0; uint8_t *b = cg_build_fx(CGFX_COLR, &n); dwx_font h; static const DWRITE_FONT_SIMULATIONS sim[] = { DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE }; int i;
    for (i = 0; i < 3; i++) {
        UINT16 g[] = { GID_A, GID_F }; static cgrun r;
        if (FAILED(dwx_font_open(&h, dw_shared(), b, n, 0, sim[i])) || !h.face) { t_note("simulation %d not available", sim[i]); dwx_font_close(&h); continue; }
        cg_mkrun(&r, h.face, 20.0f, g, 2, 2, 0, FALSE, 0);
        cg_check_f2(f, "simulated", &r.run, 1, 2, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, CGF_TT | CGF_COLR);
        dwx_font_close(&h);
    }
    free(b);
}
TEST(cf_model_parser_reads_the_fixture_tables) {
    CG_OPEN(x, CGFX_COLR); cg_model m; int i;
    if (!cg_model_from_face(x.face, &m)) { CHECKF(0, "COLR/CPAL v0 of the fixture not parseable"); cg_close(&x); return; }
    EXPECT_EQ(m.nbase, cg_fixture_model.nbase); EXPECT_EQ(m.nlay, cg_fixture_model.nlay); EXPECT_EQ(m.npal, CG_NPAL); EXPECT_EQ(m.nent, CG_NENT);
    CHECK(!memcmp(m.argb, cg_pal, sizeof cg_pal));
    for (i = 0; i < m.nbase && i < cg_fixture_model.nbase; i++) CHECK(m.base[i].gid == cg_fixture_model.base[i].gid && m.base[i].first == cg_fixture_model.base[i].first && m.base[i].n == cg_fixture_model.base[i].n);
    for (i = 0; i < m.nlay && i < cg_fixture_model.nlay; i++) CHECK(m.lay[i].gid == cg_fixture_model.lay[i].gid && m.lay[i].pal == cg_fixture_model.lay[i].pal);
    cg_model_free(&m); cg_close(&x);
}
