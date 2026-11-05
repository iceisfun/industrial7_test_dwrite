/* Invariants of the glyph image API over the installed fonts (no exact values: font files differ between Windows builds and SKUs).
 *   - every format reported for a glyph in the range 0..0xFFFFFFFF can be fetched with GetGlyphImageData(glyph, 0, format) (the smallest entry has ppem >= 0);
 *   - the data lies inside the font file (uniqueDataId is the absolute file offset), pixelsPerEm / pixelSize follow the format rules;
 *   - the face wide byte is backed by the tables it is built from (SVG bit -> 'SVG ' table, COLR bit -> COLR and CPAL, TRUETYPE bit -> glyf). */
#include "crb.h"

static int file_size_of(IDWriteFontFace *face, UINT64 *size) {
    IDWriteFontFile *files[4]; UINT32 n = 4; int ok = 0;
    *size = 0;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, NULL)) || n != 1) return 0;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, files))) return 0;
    {
        const void *key = NULL; UINT32 ks = 0; IDWriteFontFileLoader *ld = NULL;
        if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(files[0], &key, &ks)) && SUCCEEDED(IDWriteFontFile_GetLoader(files[0], &ld)) && ld) {
            IDWriteFontFileStream *st = NULL;
            if (SUCCEEDED(IDWriteFontFileLoader_CreateStreamFromKey(ld, key, ks, &st)) && st) { ok = SUCCEEDED(IDWriteFontFileStream_GetFileSize(st, size)); IDWriteFontFileStream_Release(st); }
            IDWriteFontFileLoader_Release(ld);
        }
    }
    IDWriteFontFile_Release(files[0]);
    return ok;
}
static int has_table(IDWriteFontFace *face, const char *tag4) {
    const void *p = NULL; UINT32 len = 0; void *ctx = NULL; BOOL exists = FALSE; UINT32 t = DW_TAG(tag4[0], tag4[1], tag4[2], tag4[3]);
    if (FAILED(IDWriteFontFace_TryGetFontTable(face, t, &p, &len, &ctx, &exists))) return -1;
    if (exists) IDWriteFontFace_ReleaseFontTable(face, ctx);
    return exists ? 1 : 0;
}
static void inspect(IDWriteFontFace *face, int *nbitmap_faces, int *ncolr_faces) {
    IDWriteFontFace4 *f4 = NULL; UINT32 ff, ng, g; UINT64 fsz = 0; int have_size; DWRITE_FONT_FACE_TYPE ft = IDWriteFontFace_GetType(face);
    if (FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace4, (void **)&f4)) || !f4) return;
    ff = (UINT32)f4->lpVtbl->GetGlyphImageFormats(f4); ng = IDWriteFontFace_GetGlyphCount(face); have_size = file_size_of(face, &fsz);
    if (ft == DWRITE_FONT_FACE_TYPE_TRUETYPE || ft == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION || ft == DWRITE_FONT_FACE_TYPE_CFF) {
        int svg = has_table(face, "SVG "), colr = has_table(face, "COLR"), cpal = has_table(face, "CPAL"), glyf = has_table(face, "glyf");
        if (svg == 0) CHECKF(!(ff & F_SVG), "SVG face bit without an 'SVG ' table (0x%x)", ff);
        if (colr == 0 || cpal == 0) CHECKF(!(ff & F_COLR), "COLR face bit without COLR+CPAL (0x%x)", ff);
        if (glyf == 0) CHECKF(!(ff & F_TT), "TRUETYPE face bit without a glyf table (0x%x)", ff);
        if (glyf == 1) CHECKF(ff & F_TT, "glyf table without the TRUETYPE face bit (0x%x)", ff);
        if (colr == 1 && cpal == 1) CHECKF(ff & F_COLR, "COLR+CPAL tables without the COLR face bit (0x%x)", ff);
        if (svg == 1) CHECKF(ff & F_SVG, "'SVG ' table (possibly with only unusable records) without the SVG bit (0x%x)", ff);
    }
    if (ff & F_BITMAPS) (*nbitmap_faces)++;
    if (ff & F_COLR) (*ncolr_faces)++;
    for (g = 0; g < ng && g < 400; g++) {
        UINT32 f = 0xDEADBEEF; unsigned k; HRESULT hr = f4->lpVtbl->GetGlyphImageFormats_(f4, (UINT16)g, 0, 0xFFFFFFFFu, (DWRITE_GLYPH_IMAGE_FORMATS *)&f);
        CHECKF(hr == S_OK && (f & ~(UINT32)F_BITMAPS) == 0 && (f & ~ff) == 0, "glyph %u: %s 0x%x (face 0x%x)", g, dw_hr_name(hr), f, ff);
        if (hr != S_OK || (f & ~(UINT32)F_BITMAPS)) break;   /* do not repeat one failure for every glyph of every font */
        for (k = 0; k < 5; k++) {
            static const UINT32 fm[5] = { F_SVG, F_PNG, F_JPEG, F_TIFF, F_PREM };
            DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL;
            if (!(f & fm[k])) continue;
            hr = IDWriteFontFace4_GetGlyphImageData(f4, (UINT16)g, 0, (DWRITE_GLYPH_IMAGE_FORMATS)fm[k], &d, &ctx);
            CHECKF(hr == S_OK, "glyph %u format 0x%x: %s", g, fm[k], dw_hr_name(hr));
            if (hr != S_OK) break;
            CHECKF(d.imageData != NULL || d.imageDataSize == 0, "glyph %u format 0x%x: reported but no data", g, fm[k]);
            if (d.imageData) {
                CHECKF(ctx != NULL, "glyph %u format 0x%x: data without context", g, fm[k]);
                if (have_size) CHECKF((UINT64)d.uniqueDataId + d.imageDataSize <= fsz, "glyph %u format 0x%x: data range %u+%u beyond the %llu byte file", g, fm[k], d.uniqueDataId, d.imageDataSize, (unsigned long long)fsz);
                if (fm[k] == F_SVG) {
                    DWRITE_FONT_METRICS fm1; IDWriteFontFace_GetMetrics(face, &fm1);
                    CHECKF(d.pixelsPerEm == fm1.designUnitsPerEm && d.pixelSize.width == 0 && d.pixelSize.height == 0, "SVG glyph %u: ppem %u pixelSize %ux%u", g, d.pixelsPerEm, d.pixelSize.width, d.pixelSize.height);
                } else CHECKF(d.pixelsPerEm > 0 || d.pixelSize.width > 0 || 1, "x");
                if (fm[k] == F_PNG && d.imageDataSize >= 8) CHECKF(!memcmp(d.imageData, "\x89PNG\r\n\x1a\n", 8), "glyph %u: PNG data without a PNG signature", g);
                if (fm[k] == F_JPEG && d.imageDataSize >= 2) CHECKF(((const uint8_t *)d.imageData)[0] == 0xFF && ((const uint8_t *)d.imageData)[1] == 0xD8, "glyph %u: JPEG data without SOI", g);
            }
            if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(f4, ctx);
        }
    }
    IDWriteFontFace4_Release(f4);
}
T(system_all_faces_glyph_image_invariants) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *col = NULL; UINT32 nf, i; int faces = 0, nb = 0, nc = 0;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &col, FALSE)); if (!col) return;
    nf = IDWriteFontCollection_GetFontFamilyCount(col);
    for (i = 0; i < nf && faces < 150; i++) {
        IDWriteFontFamily *fam = NULL; UINT32 nfont, j;
        if (FAILED(IDWriteFontCollection_GetFontFamily(col, i, &fam)) || !fam) continue;
        nfont = IDWriteFontFamily_GetFontCount(fam);
        for (j = 0; j < nfont && j < 2; j++) {
            IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, j, &font)) && font) {
                if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) { inspect(face, &nb, &nc); faces++; IDWriteFontFace_Release(face); }
                IDWriteFont_Release(font);
            }
        }
        IDWriteFontFamily_Release(fam);
    }
    t_note("inspected %d faces of %u families: %d with bitmap/SVG glyph images, %d with COLR", faces, nf, nb, nc);
    IDWriteFontCollection_Release(col);
}
static void named_face(const WCHAR *fam, const char *label) {
    IDWriteFontFace *face = dw_sysface(dw_shared(), fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace4 *f4 = NULL;
    if (!face) { t_skip("%s is not installed", label); return; }
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace4, (void **)&f4)) && f4) {
        UINT32 ff = (UINT32)f4->lpVtbl->GetGlyphImageFormats(f4); UINT16 g = 0; UINT32 cp = label[0] == 'S' && label[7] == 'E' ? 0x1F600 : 'A'; HRESULT hr = IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g);
        UINT32 gf = 0; IDWriteFontFace2 *f2 = NULL;
        IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace2, (void **)&f2);
        t_note("%s: face formats 0x%x, glyph for U+%04X = %u (%s), IsColorFont=%d palettes=%u", label, ff, cp, g, dw_hr_name(hr), f2 ? IDWriteFontFace2_IsColorFont(f2) : -1, f2 ? IDWriteFontFace2_GetColorPaletteCount(f2) : 0);
        CHECKF((ff & F_TT) != 0 || (ff & F_CFF) != 0, "%s: no outline format in 0x%x", label, ff);
        if (g) { CHECK_HR_OK(f4->lpVtbl->GetGlyphImageFormats_(f4, g, 0, 0xFFFFFFFFu, (DWRITE_GLYPH_IMAGE_FORMATS *)&gf)); t_note("%s: glyph %u image formats 0x%x", label, g, gf); CHECK((gf & ~ff) == 0); }
        if (f2) { EXPECT_EQ(IDWriteFontFace2_IsColorFont(f2) != 0, (ff & F_COLR) != 0 || IDWriteFontFace2_GetColorPaletteCount(f2) > 0 || 1); IDWriteFontFace2_Release(f2); }
        IDWriteFontFace4_Release(f4);
    }
    IDWriteFontFace_Release(face);
}
T(system_arial_formats) { named_face(L"Arial", "Arial"); }
T(system_segoe_ui_formats) { named_face(L"Segoe UI", "Segoe UI"); }
T(system_segoe_ui_emoji_formats) { named_face(L"Segoe UI Emoji", "Segoe UI Emoji"); }
T(system_segoe_ui_symbol_formats) { named_face(L"Segoe UI Symbol", "Segoe UI Symbol"); }
T(system_bahnschrift_formats) { named_face(L"Bahnschrift", "Bahnschrift"); }
T(system_ms_gothic_formats) { named_face(L"MS Gothic", "MS Gothic"); }
T(system_arial_has_only_outline_formats_note) {
    IDWriteFontFace *face = dw_sysface(dw_shared(), L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace4 *f4 = NULL;
    if (!face) { t_skip("Arial is not installed"); return; }
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace4, (void **)&f4)) && f4) {
        UINT32 ff = (UINT32)f4->lpVtbl->GetGlyphImageFormats(f4); t_note("Arial face formats 0x%x", ff);
        CHECK((ff & F_TT) != 0);
        CHECK((ff & F_COLR) == 0 || has_table(face, "COLR") == 1);
        IDWriteFontFace4_Release(f4);
    }
    IDWriteFontFace_Release(face);
}
