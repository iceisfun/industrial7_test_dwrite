/* Colour bitmap strikes (CBLC / CBDT) seen through IDWriteFontFace4::GetGlyphImageFormats and GetGlyphImageData.
 *
 * EmbeddedBitmapTableReader::EnumerateGlyphData (dwrite.dll!0x11ce48) only accepts BitmapSize strikes with ppemX == ppemY and bitDepth 32;
 * FillGlyphGraphicDataEntry (!0x11d66c) takes the image dimensions and bearings from the SmallGlyphMetrics / BigGlyphMetrics, the entry
 * ppem from the strike and the data offset from the (absolute) file position of the PNG bytes.  FontFace::GetGlyphImageData (!0xfb340)
 * picks the entry with the smallest ppem >= the requested ppem. */
#include "crb.h"

static const crb_cbexp *oracle(unsigned gid, unsigned req, int *any) {
    int n, i; const crb_cbexp *e = crb_cb_entries(&n), *best = NULL;
    for (i = 0; i < n; i++) if (e[i].gid == gid && e[i].ppem >= req && (!best || e[i].ppem < best->ppem)) best = &e[i];
    if (any) *any = best != NULL;
    return best;
}
static int has_png(unsigned gid, unsigned a, unsigned b) {
    int n, i; const crb_cbexp *e = crb_cb_entries(&n);
    for (i = 0; i < n; i++) if (e[i].gid == gid && e[i].ppem >= a && e[i].ppem <= b) return 1;
    return 0;
}
static uint8_t *font(size_t *n) { return crb_cbdt_font(n); }

T(cbdt_fixture_opens_and_reports_png) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    UINT32 f = crb_face_formats(&x);
    /* glyf (1) | PNG (0x10): the face wide byte is the OR of the table bits and every accepted entry */
    EXPECT_EQ(f, F_TT | F_PNG);
    crb_close(&x);
}
T(cbdt_entries_oracle_not_empty) {
    int n; const crb_cbexp *e = crb_cb_entries(&n); (void)e;
    EXPECT_EQ(n, 3 + 2 + 3 + 2);
}
T(cbdt_per_glyph_formats_only_contain_bitmap_bits) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    unsigned g;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        UINT32 f; HRESULT hr = crb_gif(&x, (UINT16)g, 0, 0xFFFFFFFFu, &f);
        CHECKF(hr == S_OK, "glyph %u: %s", g, dw_hr_name(hr));
        /* only SVG / bitmap readers feed the per glyph lists: TRUETYPE, CFF and COLR never appear there */
        CHECKF((f & ~(UINT32)F_BITMAPS) == 0, "glyph %u: 0x%x", g, f);
        CHECKF(((f & F_PNG) != 0) == has_png(g, 0, 0xFFFFFFFFu), "glyph %u: 0x%x", g, f);
    }
    crb_close(&x);
}
T(cbdt_glyph_format_ranges_grid) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const unsigned r[][2] = { {0,0},{0,15},{0,16},{16,16},{16,17},{17,23},{17,24},{24,24},{24,31},{25,31},{25,32},{32,32},{33,63},{33,64},{64,64},{65,100},
        {0,0xFFFFFFFFu},{1,0xFFFFFFFFu},{16,64},{17,63},{20,40},{1,1},{0xFFFFFFFFu,0xFFFFFFFFu},{64,16},{32,16},{100,0},{12,14},{20,20},{40,40} };
    unsigned g, i;
    for (g = 0; g < TTF_DEF_NGLYPHS + 2; g++) for (i = 0; i < sizeof r / sizeof r[0]; i++) {
        UINT32 f; HRESULT hr = crb_gif(&x, (UINT16)g, r[i][0], r[i][1], &f);
        CHECKF(hr == S_OK && (((f & F_PNG) != 0) == has_png(g, r[i][0], r[i][1])) && (f & ~(UINT32)F_PNG) == 0, "glyph %u [%u,%u]: %s 0x%x", g, r[i][0], r[i][1], dw_hr_name(hr), f);
    }
    crb_close(&x);
}
T(cbdt_glyph_format_range_bounds_are_inclusive) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int k, i; const crb_cbexp *e = crb_cb_entries(&k);
    for (i = 0; i < k; i++) {
        UINT32 f;
        CHECK_HR_OK(crb_gif(&x, (UINT16)e[i].gid, e[i].ppem, e[i].ppem, &f));  CHECKF(f & F_PNG, "glyph %u ppem %u exact", e[i].gid, e[i].ppem);
        CHECK_HR_OK(crb_gif(&x, (UINT16)e[i].gid, 0, e[i].ppem, &f));          CHECKF(f & F_PNG, "glyph %u upper bound", e[i].gid);
        CHECK_HR_OK(crb_gif(&x, (UINT16)e[i].gid, e[i].ppem, 0xFFFFFFFFu, &f)); CHECKF(f & F_PNG, "glyph %u lower bound", e[i].gid);
        CHECK_HR_OK(crb_gif(&x, (UINT16)e[i].gid, 0, e[i].ppem - 1, &f));
        CHECKF(((f & F_PNG) != 0) == has_png(e[i].gid, 0, e[i].ppem - 1), "glyph %u below", e[i].gid);
        CHECK_HR_OK(crb_gif(&x, (UINT16)e[i].gid, e[i].ppem + 1, 0xFFFFFFFFu, &f));
        CHECKF(((f & F_PNG) != 0) == has_png(e[i].gid, e[i].ppem + 1, 0xFFFFFFFFu), "glyph %u above", e[i].gid);
    }
    crb_close(&x);
}
T(cbdt_ignored_strikes_contribute_nothing) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    unsigned g;
    /* depth 8 (glyphs 8,9), ppemX != ppemY (10,11), depth 1 (12): the 32 bit filter in EnumerateGlyphData skips them */
    for (g = 8; g <= 12; g++) {
        UINT32 f = 0xFFFF; int ppem;
        CHECK_HR_OK(crb_gif(&x, (UINT16)g, 0, 0xFFFFFFFFu, &f)); CHECKF(f == 0, "glyph %u: 0x%x", g, f);
        for (ppem = 1; ppem <= 64; ppem += 7) {
            DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)1; memset(&d, 0x5A, sizeof d);
            CHECK_HR_OK(crb_gid(&x, (UINT16)g, (UINT32)ppem, F_PNG, &d, &ctx));
            CHECKF(crb_is_zero(&d, sizeof d) && ctx == NULL, "glyph %u ppem %d", g, ppem);
        }
    }
    crb_close(&x);
}
T(cbdt_only_ignored_strikes_face_has_no_png) {
    crb_cbstrike st[3]; crb_cbglyph gl[3][2]; uint8_t *lc, *dt, *out; size_t nlc, ndt, n, pl; ttf_rawtable raw[2]; int i;
    static const unsigned pp[3][3] = { {20,20,8}, {12,14,32}, {40,40,1} };
    for (i = 0; i < 3; i++) {
        memset(&st[i], 0, sizeof st[i]); st[i].ppem_x = pp[i][0]; st[i].ppem_y = pp[i][1]; st[i].depth = pp[i][2]; st[i].imgfmt = 17; st[i].idxfmt = 1; st[i].g = gl[i]; st[i].ng = 2;
        memset(gl[i], 0, sizeof gl[i]);
        gl[i][0].gid = 2; gl[i][1].gid = 3;
        gl[i][0].img = crb_png(4, 4, 0x100 + i, &pl); gl[i][0].imglen = pl; gl[i][1].img = gl[i][0].img; gl[i][1].imglen = pl;
        gl[i][0].m.h = gl[i][0].m.w = 4; gl[i][1].m = gl[i][0].m;
    }
    crb_cbtables(st, 3, &lc, &nlc, &dt, &ndt);
    memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc; memcpy(raw[1].tag, "CBDT", 5); raw[1].data = dt; raw[1].len = (unsigned)ndt;
    out = crb_blob(raw, 2, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        UINT32 f; EXPECT_EQ(crb_face_formats(&x), F_TT);
        CHECK_HR_OK(crb_gif(&x, 2, 0, 0xFFFFFFFFu, &f)); EXPECT_EQ(f, 0);
        crb_close(&x); } else crb_close(&x); }
    for (i = 0; i < 3; i++) free((void *)gl[i][0].img);
    free(lc); free(dt);
}
T(cbdt_data_selects_smallest_ppem_not_below_request) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    unsigned g, req;
    for (g = 0; g < 14; g++) for (req = 0; req <= 70; req++) {
        int any; const crb_cbexp *e = oracle(g, req, &any); DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)7; memset(&d, 0xCC, sizeof d);
        HRESULT hr = crb_gid(&x, (UINT16)g, req, F_PNG, &d, &ctx);
        if (hr != S_OK) { CHECKF(0, "glyph %u req %u: %s", g, req, dw_hr_name(hr)); continue; }
        if (any) {
            CHECKF(d.pixelsPerEm == e->ppem && d.imageData && d.imageDataSize == e->pnglen && ctx, "glyph %u req %u: ppem %u (want %u) data %p", g, req, d.pixelsPerEm, e->ppem, d.imageData);
            if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        } else {
            CHECKF(crb_is_zero(&d, sizeof d) && ctx == NULL, "glyph %u req %u should be empty", g, req);
        }
    }
    crb_close(&x);
}
T(cbdt_data_fields_for_every_entry) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int k, i; const crb_cbexp *e = crb_cb_entries(&k);
    for (i = 0; i < k; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; const uint8_t *at = crb_find(&x, e[i].png, e[i].pnglen);
        CHECK_HR_OK(crb_gid(&x, (UINT16)e[i].gid, e[i].ppem, F_PNG, &d, &ctx));
        CHECKF(at != NULL, "png of glyph %u not found in the font file", e[i].gid);
        CHECKF(d.imageData && d.imageDataSize == e[i].pnglen && !memcmp(d.imageData, e[i].png, e[i].pnglen), "glyph %u ppem %u: bytes", e[i].gid, e[i].ppem);
        CHECKF(at && d.uniqueDataId == (UINT32)(at - x.blob), "glyph %u ppem %u: uniqueDataId %u want file offset %u", e[i].gid, e[i].ppem, d.uniqueDataId, at ? (unsigned)(at - x.blob) : 0);
        CHECKF(d.pixelsPerEm == e[i].ppem, "glyph %u: ppem %u want %u", e[i].gid, d.pixelsPerEm, e[i].ppem);
        /* pixelSize comes from the glyph metrics (width, height), not from the PNG header */
        CHECKF(d.pixelSize.width == e[i].m.w && d.pixelSize.height == e[i].m.h, "glyph %u ppem %u: pixelSize %ux%u want %ux%u", e[i].gid, e[i].ppem, d.pixelSize.width, d.pixelSize.height, e[i].m.w, e[i].m.h);
        /* left origin = (-bearingX, bearingY); right origin x = advance - bearingX, same y */
        CHECKF(d.horizontalLeftOrigin.x == -e[i].m.bx && d.horizontalLeftOrigin.y == e[i].m.by, "glyph %u ppem %u: left origin (%d,%d) want (%d,%d)", e[i].gid, e[i].ppem, d.horizontalLeftOrigin.x, d.horizontalLeftOrigin.y, -e[i].m.bx, e[i].m.by);
        CHECKF(d.horizontalRightOrigin.x == e[i].m.adv - e[i].m.bx && d.horizontalRightOrigin.y == e[i].m.by, "glyph %u ppem %u: right origin (%d,%d) want (%d,%d)", e[i].gid, e[i].ppem, d.horizontalRightOrigin.x, d.horizontalRightOrigin.y, e[i].m.adv - e[i].m.bx, e[i].m.by);
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    crb_close(&x);
}
T(cbdt_big_metrics_give_exact_vertical_origins) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int k, i, seen = 0; const crb_cbexp *e = crb_cb_entries(&k);
    for (i = 0; i < k; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL;
        if (e[i].imgfmt != 18 && e[i].imgfmt != 19) continue;
        seen++;
        CHECK_HR_OK(crb_gid(&x, (UINT16)e[i].gid, e[i].ppem, F_PNG, &d, &ctx));
        /* vertical top origin = (-vertBearingX, vertBearingY); bottom origin = (-vertBearingX, vertAdvance + vertBearingY) */
        CHECKF(d.verticalTopOrigin.x == -e[i].m.vbx && d.verticalTopOrigin.y == e[i].m.vby, "glyph %u fmt %d: top (%d,%d) want (%d,%d)", e[i].gid, e[i].imgfmt, d.verticalTopOrigin.x, d.verticalTopOrigin.y, -e[i].m.vbx, e[i].m.vby);
        CHECKF(d.verticalBottomOrigin.x == -e[i].m.vbx && d.verticalBottomOrigin.y == e[i].m.vadv + e[i].m.vby, "glyph %u fmt %d: bottom (%d,%d) want (%d,%d)", e[i].gid, e[i].imgfmt, d.verticalBottomOrigin.x, d.verticalBottomOrigin.y, -e[i].m.vbx, e[i].m.vadv + e[i].m.vby);
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    EXPECT_EQ(seen, 5);
    crb_close(&x);
}
T(cbdt_small_metrics_vertical_origins_are_derived_from_hmtx) {
    /* SmallGlyphMetrics carry no vertical metrics: GlyphGraphicDataBuilder::SetEntry (!0x10ffe0) fills them from the font's design metrics
     * scaled by ppem / unitsPerEm.  Both vertical origins share their x coordinate (one field). */
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int k, i; const crb_cbexp *e = crb_cb_entries(&k);
    for (i = 0; i < k; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; DWRITE_FONT_METRICS fm; UINT16 gi = (UINT16)e[i].gid; DWRITE_GLYPH_METRICS gm;
        if (e[i].imgfmt != 17) continue;
        IDWriteFontFace_GetMetrics(x.h.face, &fm);
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(x.h.face, &gi, 1, &gm, FALSE));
        CHECK_HR_OK(crb_gid(&x, gi, e[i].ppem, F_PNG, &d, &ctx));
        CHECKF(d.verticalTopOrigin.x == d.verticalBottomOrigin.x, "glyph %u: x differs %d %d", gi, d.verticalTopOrigin.x, d.verticalBottomOrigin.x);
        {
            double s = (double)e[i].ppem / fm.designUnitsPerEm; double wantx = (gm.advanceWidth / 2) * s;
            CHECKF(fabs(d.verticalTopOrigin.x - wantx) <= 1.0, "glyph %u ppem %u: vertical x %d want ~%g", gi, e[i].ppem, d.verticalTopOrigin.x, wantx);
            CHECKF(fabs((d.verticalBottomOrigin.y - d.verticalTopOrigin.y) - gm.advanceHeight * s) <= 1.0, "glyph %u ppem %u: vertical advance %d want ~%g", gi, e[i].ppem, d.verticalBottomOrigin.y - d.verticalTopOrigin.y, gm.advanceHeight * s);
        }
        t_note("glyph %u ppem %u small metrics: vertical top (%d,%d) bottom (%d,%d)", gi, e[i].ppem, d.verticalTopOrigin.x, d.verticalTopOrigin.y, d.verticalBottomOrigin.x, d.verticalBottomOrigin.y);
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    crb_close(&x);
}
T(cbdt_unique_ids_are_distinct_per_image_and_stable) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int k, i, j; const crb_cbexp *e = crb_cb_entries(&k); UINT32 id[16]; void *ctxs[16];
    for (i = 0; i < k; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; ctxs[i] = NULL; CHECK_HR_OK(crb_gid(&x, (UINT16)e[i].gid, e[i].ppem, F_PNG, &d, &ctxs[i])); id[i] = d.uniqueDataId;
    }
    for (i = 0; i < k; i++) for (j = i + 1; j < k; j++) CHECKF(id[i] != id[j], "entries %d and %d share uniqueDataId %u", i, j, id[i]);
    for (i = 0; i < k; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *c2 = NULL; CHECK_HR_OK(crb_gid(&x, (UINT16)e[i].gid, e[i].ppem, F_PNG, &d, &c2));
        CHECKF(d.uniqueDataId == id[i], "entry %d: id changed %u -> %u", i, id[i], d.uniqueDataId);
        if (c2) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, c2);
        if (ctxs[i]) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctxs[i]);
    }
    crb_close(&x);
}
T(cbdt_other_formats_return_empty_success) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const UINT32 fm[] = { F_SVG, F_JPEG, F_TIFF, F_PREM }; int i; unsigned g;
    for (g = 2; g <= 7; g++) for (i = 0; i < 4; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)3; memset(&d, 0x77, sizeof d);
        CHECK_HR_OK(crb_gid(&x, (UINT16)g, 16, fm[i], &d, &ctx));
        CHECKF(crb_is_zero(&d, sizeof d) && ctx == NULL, "glyph %u format 0x%x", g, fm[i]);
    }
    crb_close(&x);
}
T(cbdt_invalid_format_values_are_rejected_and_zero_outputs) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    UINT32 v;
    for (v = 0; v < 512; v++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)9; int valid = v == 8 || v == 0x10 || v == 0x20 || v == 0x40 || v == 0x80; memset(&d, 0x33, sizeof d);
        HRESULT hr = crb_gid(&x, 2, 16, v, &d, &ctx);
        CHECKF(hr == (valid ? S_OK : E_INVALIDARG_), "format 0x%x: %s", v, dw_hr_name(hr));
        CHECKF(crb_is_zero(&d.pixelSize, sizeof d - offsetof(DWRITE_GLYPH_IMAGE_DATA, pixelSize)) || valid, "format 0x%x: struct not cleared", v);
        if (!valid) CHECKF(ctx == NULL && d.imageData == NULL && d.imageDataSize == 0, "format 0x%x: outputs not cleared", v);
        else if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    crb_close(&x);
}
T(cbdt_glyph_ids_beyond_the_font_are_empty) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const UINT16 ids[] = { 69, 70, 100, 255, 256, 1000, 32767, 32768, 65534, 65535 }; unsigned i;
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)1; UINT32 f = 1; memset(&d, 0x11, sizeof d);
        CHECK_HR_OK(crb_gid(&x, ids[i], 0, F_PNG, &d, &ctx)); CHECKF(crb_is_zero(&d, sizeof d) && ctx == NULL, "glyph %u", ids[i]);
        CHECK_HR_OK(crb_gif(&x, ids[i], 0, 0xFFFFFFFFu, &f)); CHECKF(f == 0, "glyph %u formats 0x%x", ids[i], f);
    }
    crb_close(&x);
}
T(cbdt_strike_glyphs_beyond_maxp_are_dropped) {
    /* GlyphGraphicDataBuilder::SetEntry ignores glyph ids >= the number of glyphs of the font */
    crb_cbstrike st; crb_cbglyph gl[10]; uint8_t *lc, *dt, *out; size_t nlc, ndt, nb, pl; ttf_rawtable raw[2]; int i, pass; uint8_t *png = crb_png(5, 6, 0x42, &pl);
    for (pass = 0; pass < 2; pass++) {
        int nglyphs = pass ? 76 : 0;   /* 0 = the default 69 glyphs */
        memset(&st, 0, sizeof st); memset(gl, 0, sizeof gl);
        st.ppem_x = st.ppem_y = 24; st.depth = 32; st.imgfmt = 17; st.idxfmt = 1; st.g = gl; st.ng = 10;
        for (i = 0; i < 10; i++) { gl[i].gid = 66 + (unsigned)i; gl[i].img = png; gl[i].imglen = pl; gl[i].m.h = 6; gl[i].m.w = 5; gl[i].m.adv = 6; }
        crb_cbtables(&st, 1, &lc, &nlc, &dt, &ndt);
        memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc; memcpy(raw[1].tag, "CBDT", 5); raw[1].data = dt; raw[1].len = (unsigned)ndt;
        out = crb_blob(raw, 2, nglyphs, &nb);
        { crb x; if (crb_open(&x, out, nb)) {
            int top = pass ? 76 : 69;
            for (i = 66; i < 76; i++) {
                UINT32 f = 0; CHECK_HR_OK(crb_gif(&x, (UINT16)i, 0, 100, &f));
                CHECKF(((f & F_PNG) != 0) == (i < top), "pass %d glyph %d: 0x%x (font has %d glyphs)", pass, i, f, top);
            }
            crb_close(&x);
        } else crb_close(&x); }
        free(lc); free(dt);
    }
    free(png);
}
T(cbdt_png_header_not_validated_for_cbdt) {
    /* the PNG bytes of a CBDT record are passed through untouched, even when they are not a PNG at all */
    crb_cbstrike st; crb_cbglyph gl[2]; uint8_t *lc, *dt, *out; size_t nlc, ndt, nb; ttf_rawtable raw[2];
    static const uint8_t junk[11] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    memset(&st, 0, sizeof st); memset(gl, 0, sizeof gl);
    st.ppem_x = st.ppem_y = 16; st.depth = 32; st.imgfmt = 17; st.idxfmt = 1; st.g = gl; st.ng = 2;
    gl[0].gid = 2; gl[1].gid = 3; gl[0].img = junk; gl[0].imglen = 11; gl[1].img = junk; gl[1].imglen = 11;
    gl[0].m.h = 3; gl[0].m.w = 4; gl[1].m = gl[0].m;
    crb_cbtables(&st, 1, &lc, &nlc, &dt, &ndt);
    memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc; memcpy(raw[1].tag, "CBDT", 5); raw[1].data = dt; raw[1].len = (unsigned)ndt;
    out = crb_blob(raw, 2, 0, &nb);
    { crb x; if (crb_open(&x, out, nb)) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL;
        CHECK_HR_OK(crb_gid(&x, 2, 16, F_PNG, &d, &ctx));
        CHECKF(d.imageData && d.imageDataSize == 11 && !memcmp(d.imageData, junk, 11), "junk bytes");
        CHECKF(d.pixelSize.width == 4 && d.pixelSize.height == 3, "size %ux%u", d.pixelSize.width, d.pixelSize.height);
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        crb_close(&x);
    } else crb_close(&x); }
    free(lc); free(dt);
}
T(cbdt_format1_depth32_bitmap_note) {
    /* image format 1 (small metrics + raw bitmap) at bit depth 32: FillGlyphGraphicDataEntry marks entries whose format class has no
     * flag as PREMULTIPLIED_B8G8R8A8 when the strike depth is 32.  Observed behaviour is recorded; consistency is checked when reported. */
    crb_cbstrike st; crb_cbglyph gl[2]; uint8_t *lc, *dt, *out; size_t nlc, ndt, nb; ttf_rawtable raw[2]; static uint8_t pix[4 * 4 * 4];
    int i; for (i = 0; i < 64; i++) pix[i] = (uint8_t)(i * 3 + 1);
    memset(&st, 0, sizeof st); memset(gl, 0, sizeof gl);
    st.ppem_x = st.ppem_y = 16; st.depth = 32; st.imgfmt = 1; st.idxfmt = 1; st.g = gl; st.ng = 2;
    for (i = 0; i < 2; i++) { gl[i].gid = 2 + (unsigned)i; gl[i].img = pix; gl[i].imglen = sizeof pix; gl[i].m.h = 4; gl[i].m.w = 4; gl[i].m.adv = 5; gl[i].m.by = 3; }
    crb_cbtables(&st, 1, &lc, &nlc, &dt, &ndt);
    memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc; memcpy(raw[1].tag, "CBDT", 5); raw[1].data = dt; raw[1].len = (unsigned)ndt;
    out = crb_blob(raw, 2, 0, &nb);
    { crb x; if (crb_open(&x, out, nb)) {
        UINT32 f = crb_face_formats(&x), gf = 0; DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL;
        CHECK_HR_OK(crb_gif(&x, 2, 0, 100, &gf));
        t_note("CBDT image format 1 depth 32: face formats 0x%x glyph 2 formats 0x%x", f, gf);
        CHECK((gf & ~f) == 0);
        CHECK_HR_OK(crb_gid(&x, 2, 16, F_PREM, &d, &ctx));
        if (gf & F_PREM) {
            CHECKF(d.imageData != NULL && d.imageDataSize == sizeof pix && !memcmp(d.imageData, pix, sizeof pix), "premultiplied bytes size %u", d.imageDataSize);
            CHECKF(d.pixelSize.width == 4 && d.pixelSize.height == 4, "size %ux%u", d.pixelSize.width, d.pixelSize.height);
        } else CHECK(d.imageData == NULL);
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        crb_close(&x);
    } else crb_close(&x); }
    free(lc); free(dt);
}
T(cbdt_index_subtable_formats_are_equivalent) {
    /* the same strike described with index subtable format 1 (u32 offsets) and 3 (u16 offsets) enumerates identically */
    size_t pl, nb; uint8_t *png[4]; int i, f, pass; ttf_rawtable raw[2]; uint8_t *lc, *dt, *out; unsigned ids[2][4];
    for (i = 0; i < 4; i++) png[i] = crb_png(3 + (unsigned)i, 4, 0x900 + (unsigned)i, &pl);
    for (pass = 0; pass < 2; pass++) {
        crb_cbstrike st; crb_cbglyph gl[4]; size_t nlc, ndt; memset(&st, 0, sizeof st); memset(gl, 0, sizeof gl);
        st.ppem_x = st.ppem_y = 20; st.depth = 32; st.imgfmt = 17; st.idxfmt = pass ? 3 : 1; st.g = gl; st.ng = 4;
        for (i = 0; i < 4; i++) { gl[i].gid = 10 + (unsigned)i; gl[i].img = png[i]; gl[i].imglen = pl; gl[i].m.h = (uint8_t)(4 + i); gl[i].m.w = (uint8_t)(3 + i); gl[i].m.adv = 9; }
        crb_cbtables(&st, 1, &lc, &nlc, &dt, &ndt);
        memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc; memcpy(raw[1].tag, "CBDT", 5); raw[1].data = dt; raw[1].len = (unsigned)ndt;
        out = crb_blob(raw, 2, 0, &nb);
        { crb x; if (crb_open(&x, out, nb)) {
            for (i = 0; i < 4; i++) {
                DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; CHECK_HR_OK(crb_gid(&x, (UINT16)(10 + i), 20, F_PNG, &d, &ctx));
                CHECKF(d.imageData && d.imageDataSize == pl && !memcmp(d.imageData, png[i], pl), "format %d glyph %d", st.idxfmt, 10 + i);
                CHECKF(d.pixelSize.width == (UINT32)(3 + i) && d.pixelSize.height == (UINT32)(4 + i), "format %d glyph %d size %ux%u", st.idxfmt, 10 + i, d.pixelSize.width, d.pixelSize.height);
                ids[pass][i] = d.uniqueDataId; if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
            }
            crb_close(&x);
        } else { crb_close(&x); for (f = 0; f < 4; f++) ids[pass][f] = 0; } }
        free(lc); free(dt);
    }
    for (i = 0; i < 4; i++) free(png[i]);
}
T(cbdt_cblc_without_cbdt_is_not_a_bitmap_font_note) {
    crb_cbstrike st; crb_cbglyph gl[1]; uint8_t *lc, *dt, *out; size_t nlc, ndt, nb, pl; ttf_rawtable raw[1]; uint8_t *png = crb_png(2, 2, 7, &pl);
    memset(&st, 0, sizeof st); memset(gl, 0, sizeof gl);
    st.ppem_x = st.ppem_y = 16; st.depth = 32; st.imgfmt = 17; st.idxfmt = 1; st.g = gl; st.ng = 1; gl[0].gid = 2; gl[0].img = png; gl[0].imglen = pl; gl[0].m.h = 2; gl[0].m.w = 2;
    crb_cbtables(&st, 1, &lc, &nlc, &dt, &ndt);
    memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc;
    out = crb_blob(raw, 1, 0, &nb);
    { crb x; if (crb_open(&x, out, nb)) {
        UINT32 f = crb_face_formats(&x); t_note("CBLC without CBDT: face formats 0x%x", f);
        CHECK((f & F_TT) != 0);
        crb_close(&x);
    } else crb_close(&x); }
    free(lc); free(dt); free(png);
}

T(cbdt_image_data_struct_layout_and_constants) {
    /* offsets used by FontFace::GetGlyphImageData (!0xfb340) and the 0x40 byte clear in DWriteFontFace::GetGlyphImageData (!0xdc780) */
    EXPECT_EQ(sizeof(DWRITE_GLYPH_IMAGE_DATA), 64);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, imageData), 0); EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, imageDataSize), 8); EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, uniqueDataId), 0xC);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, pixelsPerEm), 0x10); EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, pixelSize), 0x14); EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, horizontalLeftOrigin), 0x1C);
    EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, horizontalRightOrigin), 0x24); EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, verticalTopOrigin), 0x2C); EXPECT_EQ(offsetof(DWRITE_GLYPH_IMAGE_DATA, verticalBottomOrigin), 0x34);
    EXPECT_EQ(F_TT, 1); EXPECT_EQ(F_CFF, 2); EXPECT_EQ(F_COLR, 4); EXPECT_EQ(F_SVG, 8); EXPECT_EQ(F_PNG, 0x10); EXPECT_EQ(F_JPEG, 0x20); EXPECT_EQ(F_TIFF, 0x40); EXPECT_EQ(F_PREM, 0x80);
    EXPECT_EQ(DWRITE_GLYPH_IMAGE_FORMATS_NONE, 0);
}
T(cbdt_two_bitmapsize_records_with_one_ppem_cover_different_glyphs) {
    crb_cbstrike st[2]; crb_cbglyph ga[2], gb[3]; uint8_t *lc, *dt, *out; size_t nlc, ndt, nb, pl; ttf_rawtable raw[2]; uint8_t *png = crb_png(4, 4, 0x33, &pl); int i;
    memset(st, 0, sizeof st); memset(ga, 0, sizeof ga); memset(gb, 0, sizeof gb);
    for (i = 0; i < 2; i++) { ga[i].gid = 2 + (unsigned)i; ga[i].img = png; ga[i].imglen = pl; ga[i].m.h = ga[i].m.w = 4; }
    for (i = 0; i < 3; i++) { gb[i].gid = 10 + (unsigned)i; gb[i].img = png; gb[i].imglen = pl; gb[i].m.h = gb[i].m.w = 4; }
    st[0].ppem_x = st[0].ppem_y = 24; st[0].depth = 32; st[0].imgfmt = 17; st[0].idxfmt = 1; st[0].g = ga; st[0].ng = 2;
    st[1] = st[0]; st[1].g = gb; st[1].ng = 3;
    crb_cbtables(st, 2, &lc, &nlc, &dt, &ndt);
    memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc; memcpy(raw[1].tag, "CBDT", 5); raw[1].data = dt; raw[1].len = (unsigned)ndt;
    out = crb_blob(raw, 2, 0, &nb);
    { crb x; if (crb_open(&x, out, nb)) {
        unsigned g;
        for (g = 0; g < 16; g++) { UINT32 f; int want = (g == 2 || g == 3 || (g >= 10 && g <= 12)); CHECK_HR_OK(crb_gif(&x, (UINT16)g, 24, 24, &f)); CHECKF(((f & F_PNG) != 0) == want, "glyph %u: 0x%x", g, f); }
        crb_close(&x);
    } else crb_close(&x); }
    free(lc); free(dt); free(png);
}
T(cbdt_same_glyph_in_two_strikes_of_equal_ppem_note) {
    /* two entries with equal ppem and format for one glyph: which one GetGlyphImageData returns is decided by the unstable sort of the entries; only record it */
    crb_cbstrike st[2]; crb_cbglyph ga[1], gb[1]; uint8_t *lc, *dt, *out; size_t nlc, ndt, nb, p1, p2; ttf_rawtable raw[2]; uint8_t *pa = crb_png(4, 4, 0x41, &p1), *pb = crb_png(5, 5, 0x42, &p2);
    memset(st, 0, sizeof st); memset(ga, 0, sizeof ga); memset(gb, 0, sizeof gb);
    ga[0].gid = 2; ga[0].img = pa; ga[0].imglen = p1; ga[0].m.h = ga[0].m.w = 4; gb[0].gid = 2; gb[0].img = pb; gb[0].imglen = p2; gb[0].m.h = gb[0].m.w = 5;
    st[0].ppem_x = st[0].ppem_y = 24; st[0].depth = 32; st[0].imgfmt = 17; st[0].idxfmt = 1; st[0].g = ga; st[0].ng = 1; st[1] = st[0]; st[1].g = gb;
    crb_cbtables(st, 2, &lc, &nlc, &dt, &ndt);
    memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc; memcpy(raw[1].tag, "CBDT", 5); raw[1].data = dt; raw[1].len = (unsigned)ndt;
    out = crb_blob(raw, 2, 0, &nb);
    { crb x; if (crb_open(&x, out, nb)) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; UINT32 f;
        CHECK_HR_OK(crb_gif(&x, 2, 24, 24, &f)); EXPECT_EQ(f, F_PNG);
        CHECK_HR_OK(crb_gid(&x, 2, 24, F_PNG, &d, &ctx));
        CHECKF(d.imageData && (d.imageDataSize == p1 || d.imageDataSize == p2), "size %u", d.imageDataSize);
        t_note("duplicate strike entries: returned the %s", d.imageDataSize == p1 ? "first (4x4)" : "second (5x5)");
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        crb_close(&x);
    } else crb_close(&x); }
    free(lc); free(dt); free(pa); free(pb);
}
