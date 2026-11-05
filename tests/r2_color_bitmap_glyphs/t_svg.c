/* OpenType 'SVG ' documents seen through GetGlyphImageFormats / GetGlyphImageData.
 *
 * SvgTableReader::EnumerateGlyphData (dwrite.dll!0x11f744) walks the document index and, for every glyph start..end of a record, hands the sink
 * an entry {format SVG, flags 0, ppem 0, offset = svgTableOffset + documentListOffset + docOffset, size = docLength}.  Entries of glyphs >= numGlyphs are
 * dropped (SetEntry, !0x10ffe0) and a record with start > end enumerates nothing.
 * FontFace::GetGlyphImageData (!0xfb340) completes SVG entries (no stored metrics) from the glyph's design metrics: pixelsPerEm = designUnitsPerEm,
 * pixelSize = 0x0, left origin (0,0), right origin (advanceWidth,0), top origin (advanceWidth/2, -verticalOriginY), bottom origin
 * (advanceWidth/2, advanceHeight - verticalOriginY).
 * FindGlyphGraphicDataEntry (!0x10fa68) returns the first entry whose ppem is >= the requested ppem, and an SVG entry has ppem 0, so an SVG document is
 * only returned for a request of ppem 0; GetGlyphImageFormats(glyph, first, last) on the other hand counts ppem 0 entries for every range. */
#include "crb.h"

static const char D0[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"><circle id=\"glyph3\" cx=\"10\" cy=\"10\" r=\"9\"/></svg>";
static const char D1[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"><g id=\"glyph7\"><rect width=\"50\" height=\"60\"/></g><g id=\"glyph8\"><rect width=\"5\" height=\"6\"/></g></svg>";
static const char D2[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"><path id=\"glyph20\" d=\"M0 0L100 0L100 100z\"/></svg>";
static const char D4[] = { 0x1F, (char)0x8B, 8, 0, 0, 0, 0, 0, 0, 3, 'g', 'z', 'i', 'p', '-', 'l', 'i', 'k', 'e', ' ', 'b', 'y', 't', 'e', 's', 0, 1, 2 };
static const char D5[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"><ellipse id=\"glyph66\" rx=\"4\" ry=\"3\"/></svg>";
static const char D6[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"><line id=\"glyph0\" x2=\"5\"/></svg>";
static const char D7[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"><text id=\"glyph5\">never</text></svg>";
#define NDOC 8
static const crb_svgdoc docs[NDOC] = {
    { 3, 3, D0, sizeof D0 - 1 }, { 7, 9, D1, sizeof D1 - 1 }, { 20, 20, D2, sizeof D2 - 1 }, { 30, 31, D2, sizeof D2 - 1 },
    { 50, 50, D4, sizeof D4 }, { 66, 70, D5, sizeof D5 - 1 }, { 5, 4, D7, sizeof D7 - 1 }, { 0, 0, D6, sizeof D6 - 1 } };
static const crb_svgdoc *doc_of(unsigned g) {
    int i; for (i = 0; i < NDOC; i++) if (g >= docs[i].first && g <= docs[i].last && g < TTF_DEF_NGLYPHS) return &docs[i];
    return NULL;
}
static uint8_t *font_with(const crb_svgdoc *d, int n, size_t *len, int nglyphs) {
    size_t sl; uint8_t *svg = crb_svg(d, n, &sl); ttf_rawtable raw[1]; uint8_t *out;
    memcpy(raw[0].tag, "SVG ", 5); raw[0].data = svg; raw[0].len = (unsigned)sl;
    out = crb_blob(raw, 1, nglyphs, len); free(svg); return out;
}
static uint8_t *font(size_t *n) { return font_with(docs, NDOC, n, 0); }

T(svg_face_formats) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    EXPECT_EQ(crb_face_formats(&x), F_TT | F_SVG);
    crb_close(&x);
}
T(svg_formats_are_size_independent_in_every_range) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const unsigned r[][2] = { {0,0},{1,1},{0,100},{16,16},{16,8},{100,0},{2048,2048},{0xFFFFFFFFu,0xFFFFFFFFu},{0,0xFFFFFFFFu},{500,20} }; unsigned g, i;
    for (g = 0; g < TTF_DEF_NGLYPHS + 4; g++) for (i = 0; i < sizeof r / sizeof r[0]; i++) {
        UINT32 f; HRESULT hr = crb_gif(&x, (UINT16)g, r[i][0], r[i][1], &f);
        /* SVG entries have ppem 0, accepted by the "|| ppem == 0" clause of the range test */
        CHECKF(hr == S_OK && f == (doc_of(g) ? F_SVG : 0u), "glyph %u [%u,%u]: %s 0x%x", g, r[i][0], r[i][1], dw_hr_name(hr), f);
    }
    crb_close(&x);
}
T(svg_documents_at_ppem_zero) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    unsigned g;
    for (g = 0; g < TTF_DEF_NGLYPHS + 4; g++) {
        const crb_svgdoc *d = doc_of(g); DWRITE_GLYPH_IMAGE_DATA im; void *ctx = (void *)(ULONG_PTR)7; memset(&im, 0xCC, sizeof im);
        CHECK_HR_OK(crb_gid(&x, (UINT16)g, 0, F_SVG, &im, &ctx));
        if (d) {
            const uint8_t *at = crb_find(&x, d->doc, d->len);
            CHECKF(im.imageData && im.imageDataSize == d->len && !memcmp(im.imageData, d->doc, d->len), "glyph %u: document bytes", g);
            CHECKF(at && im.uniqueDataId == (UINT32)(at - x.blob), "glyph %u: uniqueDataId %u want absolute file offset %u", g, im.uniqueDataId, at ? (unsigned)(at - x.blob) : 0);
            CHECKF(ctx != NULL, "glyph %u: no context for non-empty data", g);
            if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        } else CHECKF(crb_is_zero(&im, sizeof im) && ctx == NULL, "glyph %u: expected empty result", g);
    }
    crb_close(&x);
}
T(svg_documents_need_a_zero_ppem_request) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const UINT32 pp[] = { 1, 2, 12, 16, 100, 2048, 65535, 65536, 0x7FFFFFFFu, 0xFFFFFFFFu }; unsigned i, g;
    for (g = 3; g <= 9; g++) for (i = 0; i < sizeof pp / sizeof pp[0]; i++) {
        DWRITE_GLYPH_IMAGE_DATA im; void *ctx = (void *)(ULONG_PTR)7; memset(&im, 0xCC, sizeof im);
        CHECK_HR_OK(crb_gid(&x, (UINT16)g, pp[i], F_SVG, &im, &ctx));
        /* the entry ppem (0) is compared with ">= requested": nothing is found for a request above 0 although GetGlyphImageFormats reports SVG */
        CHECKF(crb_is_zero(&im, sizeof im) && ctx == NULL, "glyph %u ppem %u returned data", g, pp[i]);
    }
    crb_close(&x);
}
T(svg_data_fields_from_design_metrics) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    unsigned g;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        DWRITE_GLYPH_IMAGE_DATA im; void *ctx = NULL; DWRITE_FONT_METRICS fm; DWRITE_GLYPH_METRICS gm; UINT16 gi = (UINT16)g;
        if (!doc_of(g)) continue;
        IDWriteFontFace_GetMetrics(x.h.face, &fm);
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(x.h.face, &gi, 1, &gm, FALSE));
        CHECK_HR_OK(crb_gid(&x, gi, 0, F_SVG, &im, &ctx));
        EXPECT_EQ(im.pixelsPerEm, fm.designUnitsPerEm);
        CHECKF(im.pixelSize.width == 0 && im.pixelSize.height == 0, "glyph %u: pixelSize %ux%u", g, im.pixelSize.width, im.pixelSize.height);
        CHECKF(im.horizontalLeftOrigin.x == 0 && im.horizontalLeftOrigin.y == 0, "glyph %u: left origin (%d,%d)", g, im.horizontalLeftOrigin.x, im.horizontalLeftOrigin.y);
        CHECKF(im.horizontalRightOrigin.x == (int)gm.advanceWidth && im.horizontalRightOrigin.y == 0, "glyph %u: right origin (%d,%d) advance %u", g, im.horizontalRightOrigin.x, im.horizontalRightOrigin.y, gm.advanceWidth);
        CHECKF(im.verticalTopOrigin.x == (int)(gm.advanceWidth >> 1) && im.verticalTopOrigin.y == -gm.verticalOriginY, "glyph %u: top origin (%d,%d) vorigin %d", g, im.verticalTopOrigin.x, im.verticalTopOrigin.y, gm.verticalOriginY);
        CHECKF(im.verticalBottomOrigin.x == (int)(gm.advanceWidth >> 1) && im.verticalBottomOrigin.y == (int)gm.advanceHeight - gm.verticalOriginY, "glyph %u: bottom origin (%d,%d) advanceHeight %u", g, im.verticalBottomOrigin.x, im.verticalBottomOrigin.y, gm.advanceHeight);
        if (g == 3) t_note("SVG glyph 3: ppem %u advance %u vertical origin Y %d advanceHeight %u", im.pixelsPerEm, gm.advanceWidth, gm.verticalOriginY, gm.advanceHeight);
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    crb_close(&x);
}
T(svg_glyph_ranges_share_one_document) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const unsigned grp[][3] = { { 7, 8, 9 }, { 20, 30, 31 } }; int i;
    for (i = 0; i < 2; i++) {
        DWRITE_GLYPH_IMAGE_DATA a, c, d; void *ca = NULL, *cc = NULL, *cd = NULL;
        CHECK_HR_OK(crb_gid(&x, (UINT16)grp[i][0], 0, F_SVG, &a, &ca)); CHECK_HR_OK(crb_gid(&x, (UINT16)grp[i][1], 0, F_SVG, &c, &cc)); CHECK_HR_OK(crb_gid(&x, (UINT16)grp[i][2], 0, F_SVG, &d, &cd));
        CHECKF(a.uniqueDataId == c.uniqueDataId && c.uniqueDataId == d.uniqueDataId, "group %d: ids %u %u %u", i, a.uniqueDataId, c.uniqueDataId, d.uniqueDataId);
        CHECKF(a.imageDataSize == c.imageDataSize && a.imageDataSize == d.imageDataSize && !memcmp(a.imageData, c.imageData, a.imageDataSize) && !memcmp(a.imageData, d.imageData, a.imageDataSize), "group %d: documents differ", i);
        if (ca) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ca); if (cc) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, cc); if (cd) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, cd);
    }
    { DWRITE_GLYPH_IMAGE_DATA a, c; void *ca = NULL, *cc = NULL; CHECK_HR_OK(crb_gid(&x, 3, 0, F_SVG, &a, &ca)); CHECK_HR_OK(crb_gid(&x, 7, 0, F_SVG, &c, &cc)); CHECK(a.uniqueDataId != c.uniqueDataId);
      if (ca) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ca); if (cc) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, cc); }
    crb_close(&x);
}
T(svg_two_records_pointing_at_one_document_share_the_offset) {
    /* glyphs 20 (record 3) and 30..31 (record 4) refer to one stored document: identical id, bytes taken once from the file */
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    DWRITE_GLYPH_IMAGE_DATA a, c; void *ca = NULL, *cc = NULL; int count = 0; size_t i;
    CHECK_HR_OK(crb_gid(&x, 20, 0, F_SVG, &a, &ca)); CHECK_HR_OK(crb_gid(&x, 31, 0, F_SVG, &c, &cc));
    EXPECT_EQ(a.uniqueDataId, c.uniqueDataId);
    for (i = 0; i + sizeof D2 - 1 <= x.n; i++) if (x.blob[i] == '<' && !memcmp(x.blob + i, D2, sizeof D2 - 1)) count++;
    EXPECT_EQ(count, 1);
    if (ca) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ca); if (cc) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, cc);
    crb_close(&x);
}
T(svg_compressed_documents_are_returned_verbatim) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    DWRITE_GLYPH_IMAGE_DATA im; void *ctx = NULL;
    CHECK_HR_OK(crb_gid(&x, 50, 0, F_SVG, &im, &ctx));
    CHECKF(im.imageData && im.imageDataSize == sizeof D4 && !memcmp(im.imageData, D4, sizeof D4), "size %u", im.imageDataSize);
    CHECKF(im.imageData && ((const uint8_t *)im.imageData)[0] == 0x1F && ((const uint8_t *)im.imageData)[1] == 0x8B, "gzip magic");
    if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    crb_close(&x);
}
T(svg_reversed_record_enumerates_nothing) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    UINT32 f; DWRITE_GLYPH_IMAGE_DATA im; void *ctx = NULL; unsigned g;
    for (g = 4; g <= 5; g++) {   /* record {5,4}: first > last */
        CHECK_HR_OK(crb_gif(&x, (UINT16)g, 0, 100, &f)); EXPECT_EQ(f, 0);
        CHECK_HR_OK(crb_gid(&x, (UINT16)g, 0, F_SVG, &im, &ctx)); CHECKF(im.imageData == NULL && ctx == NULL, "glyph %u", g);
    }
    crb_close(&x);
}
T(svg_glyphs_beyond_maxp_are_dropped) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    UINT32 f; int g;
    for (g = 64; g <= 72; g++) {
        CHECK_HR_OK(crb_gif(&x, (UINT16)g, 0, 0, &f));
        CHECKF(((f & F_SVG) != 0) == (g >= 66 && g <= 68), "glyph %d (record 66..70, font has 69 glyphs): 0x%x", g, f);
    }
    crb_close(&x);
    /* the same table in a 76 glyph font keeps 69 and 70 */
    b = font_with(docs, NDOC, &n, 76);
    { crb y; if (crb_open(&y, b, n)) {
        for (g = 64; g <= 72; g++) { CHECK_HR_OK(crb_gif(&y, (UINT16)g, 0, 0, &f)); CHECKF(((f & F_SVG) != 0) == (g >= 66 && g <= 70), "glyph %d (76 glyph font): 0x%x", g, f); }
        crb_close(&y);
    } else crb_close(&y); }
}
T(svg_only_dropped_records_leave_no_svg_bit_on_the_face) {
    static const crb_svgdoc d[1] = { { 100, 110, D0, sizeof D0 - 1 } }; size_t n; uint8_t *b = font_with(d, 1, &n, 0);
    CRB_NEED(x, b, n);
    EXPECT_EQ(crb_face_formats(&x), F_TT);
    crb_close(&x);
}
T(svg_and_png_strikes_in_one_font) {
    size_t sl, nlc, ndt, n, pl; uint8_t *svg = crb_svg(docs, NDOC, &sl), *lc, *dt, *out, *png = crb_png(3, 3, 0x11, &pl); crb_cbstrike cs; crb_cbglyph cg[2]; ttf_rawtable raw[3];
    memset(&cs, 0, sizeof cs); memset(cg, 0, sizeof cg); cs.ppem_x = cs.ppem_y = 16; cs.depth = 32; cs.imgfmt = 17; cs.idxfmt = 1; cs.g = cg; cs.ng = 2;
    cg[0].gid = 3; cg[1].gid = 4; cg[0].img = cg[1].img = png; cg[0].imglen = cg[1].imglen = pl; cg[0].m.h = cg[0].m.w = 3; cg[1].m = cg[0].m;
    crb_cbtables(&cs, 1, &lc, &nlc, &dt, &ndt);
    memcpy(raw[0].tag, "SVG ", 5); raw[0].data = svg; raw[0].len = (unsigned)sl; memcpy(raw[1].tag, "CBLC", 5); raw[1].data = lc; raw[1].len = (unsigned)nlc; memcpy(raw[2].tag, "CBDT", 5); raw[2].data = dt; raw[2].len = (unsigned)ndt;
    out = crb_blob(raw, 3, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        UINT32 f; DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL;
        EXPECT_EQ(crb_face_formats(&x), F_TT | F_SVG | F_PNG);
        CHECK_HR_OK(crb_gif(&x, 3, 0, 100, &f)); EXPECT_EQ(f, F_SVG | F_PNG);
        CHECK_HR_OK(crb_gif(&x, 3, 17, 100, &f)); EXPECT_EQ(f, F_SVG);        /* the PNG strike is 16 */
        CHECK_HR_OK(crb_gif(&x, 4, 0, 100, &f)); EXPECT_EQ(f, F_PNG);
        CHECK_HR_OK(crb_gid(&x, 3, 16, F_PNG, &d, &ctx)); CHECK(d.imageData != NULL && d.imageDataSize == pl); if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        CHECK_HR_OK(crb_gid(&x, 3, 0, F_SVG, &d, &ctx)); CHECK(d.imageData != NULL && d.imageDataSize == sizeof D0 - 1); if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        crb_close(&x);
    } else crb_close(&x); }
    free(svg); free(lc); free(dt); free(png);
}
T(svg_many_documents_every_glyph) {
    /* one document per glyph for all 69 glyphs: ids all distinct, every document found by its own bytes */
    static char txt[69][80]; crb_svgdoc d[69]; size_t n; uint8_t *b; int g;
    for (g = 0; g < 69; g++) { snprintf(txt[g], sizeof txt[g], "<svg xmlns=\"http://www.w3.org/2000/svg\"><use id=\"glyph%d\" x=\"%d\"/></svg>", g, g * 3); d[g].first = d[g].last = (unsigned)g; d[g].doc = txt[g]; d[g].len = strlen(txt[g]); }
    b = font_with(d, 69, &n, 0);
    { crb x; if (crb_open(&x, b, n)) {
        UINT32 ids[69];
        for (g = 0; g < 69; g++) {
            DWRITE_GLYPH_IMAGE_DATA im; void *ctx = NULL; CHECK_HR_OK(crb_gid(&x, (UINT16)g, 0, F_SVG, &im, &ctx));
            CHECKF(im.imageData && im.imageDataSize == d[g].len && !memcmp(im.imageData, d[g].doc, d[g].len), "glyph %d", g);
            ids[g] = im.uniqueDataId; if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        }
        { int i, j; for (i = 0; i < 69; i++) for (j = i + 1; j < 69; j++) if (ids[i] == ids[j]) CHECKF(0, "glyphs %d and %d share id %u", i, j, ids[i]); }
        crb_close(&x);
    } else crb_close(&x); }
}
T(svg_empty_document_note) {
    static const crb_svgdoc d[2] = { { 3, 3, D0, 0 }, { 4, 4, D1, sizeof D1 - 1 } }; size_t n; uint8_t *b = font_with(d, 2, &n, 0);
    CRB_NEED(x, b, n);
    DWRITE_GLYPH_IMAGE_DATA im; void *ctx = (void *)(ULONG_PTR)1;
    CHECK_HR_OK(crb_gid(&x, 3, 0, F_SVG, &im, &ctx));
    /* size 0 skips ReadFragment (FontFace::GetGlyphImageData tests entry.size != 0): no pointer, no context */
    CHECKF(im.imageData == NULL && im.imageDataSize == 0 && ctx == NULL, "empty document returned %p size %u ctx %p", im.imageData, im.imageDataSize, ctx);
    t_note("empty SVG document: uniqueDataId %u ppem %u face formats 0x%x", im.uniqueDataId, im.pixelsPerEm, crb_face_formats(&x));
    crb_close(&x);
}
T(svg_translate_color_glyph_run_ppem_rounding) {
    /* TranslateColorGlyphRun looks bitmap/SVG entries up with ppem = round(maxScale(transform) * fontEmSize); an SVG entry (ppem 0) is therefore only
     * found when that rounds to 0 */
    IDWriteFactory4 *f = crb_f4(); size_t n; uint8_t *b = font(&n);
    if (!f) { free(b); t_skip("IDWriteFactory4 unsupported"); return; }
    { crb x; if (crb_open(&x, b, n)) {
        static const float em[] = { 0.1f, 0.4f, 0.49f, 0.5f, 0.6f, 1.0f, 20.0f, 2048.0f }; unsigned i; UINT16 g = 3; float adv = 0;
        DWRITE_GLYPH_RUN run; memset(&run, 0, sizeof run); run.fontFace = x.h.face; run.glyphCount = 1; run.glyphIndices = &g; run.glyphAdvances = &adv;
        for (i = 0; i < sizeof em / sizeof em[0]; i++) {
            IDWriteColorGlyphRunEnumerator1 *e = (void *)(ULONG_PTR)1; D2D1_POINT_2F o = { 0, 0 }; HRESULT hr; double rounded = floor((double)em[i] + 0.5);
            run.fontEmSize = em[i];
            hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, &run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(F_TT | F_SVG), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
            if (rounded == 0.0 && em[i] != 0.5f) { CHECKF(hr == S_OK, "em %g: %s", em[i], dw_hr_name(hr)); }
            else if (rounded >= 1.0 && em[i] != 0.5f) { CHECKF(hr == E_NOCOLOR_, "em %g: %s", em[i], dw_hr_name(hr)); }
            else t_note("em %g (rounds to a tie): %s", em[i], dw_hr_name(hr));
            if (FAILED(hr)) CHECK_OUT_NULL(e);
            DW_RELEASE(e);
        }
        crb_close(&x);
    } else crb_close(&x); }
}
