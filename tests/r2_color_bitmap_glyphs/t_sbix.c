/* sbix strikes seen through GetGlyphImageFormats / GetGlyphImageData.
 *
 * SbixTableReader::EnumerateGlyphData (dwrite.dll!0x11fa00): a version 0 table yields nothing; per strike and glyph the record
 * [originOffsetX, originOffsetY, graphicType, data] is accepted when its length is >= 8 and the type is 'png ' (PNG), 'jpg ' (JPEG),
 * 'tiff' (TIFF), or 'dupe' (the data of the glyph named by the next u16 when that glyph is < numGlyphs and itself one of the three image
 * types; a dupe of a dupe is dropped).  The dimensions come from the image header (GetImageDimensions, !0x10fbb0, clamped to 65535); the entry
 * origin is (-originOffsetX, originOffsetY + height); the remaining metrics are completed by GlyphGraphicDataBuilder::SetEntry. */
#include "crb.h"

typedef struct { unsigned gid, ppem; uint32_t fmt; int16_t ox, oy; const uint8_t *data; size_t len; unsigned w, h; } sbexp;
static sbexp g_exp[32]; static int g_nexp; static uint8_t *g_cache; static size_t g_cachen;
#define TY_PNG CRB_TAG('p','n','g',' ')
#define TY_JPG CRB_TAG('j','p','g',' ')
#define TY_TIF CRB_TAG('t','i','f','f')
#define TY_DUPE CRB_TAG('d','u','p','e')

static uint32_t fmt_of(uint32_t ty) { return ty == TY_PNG ? F_PNG : ty == TY_JPG ? F_JPEG : F_TIFF; }
static void add_exp(unsigned gid, unsigned ppem, uint32_t ty, int ox, int oy, const uint8_t *d, size_t l, unsigned w, unsigned h) {
    sbexp *e = &g_exp[g_nexp++]; e->gid = gid; e->ppem = ppem; e->fmt = fmt_of(ty); e->ox = (int16_t)ox; e->oy = (int16_t)oy; e->data = d; e->len = l; e->w = w; e->h = h;
}
static const uint8_t dupe2[2] = { 0, 2 }, dupe3[2] = { 0, 3 }, dupe6[2] = { 0, 6 }, dupe9[2] = { 0, 9 }, dupe12[2] = { 0, 12 }, dupe17[2] = { 0, 17 }, dupe500[2] = { 0x01, 0xF4 };
static const uint8_t junk8[3] = { 9, 9, 9 };
/* Builds the fixture (once) and the oracle. */
static void build(void) {
    static crb_sbrec A[24], B[8], C[4], D[4]; static crb_sbstrike st[4]; size_t l[16]; int i = 0; ttf_rawtable raw[1];
    uint8_t *pA2, *jA3, *tA4, *pA5, *pB2, *jB3, *tB4, *pB20, *tC2, *pD21; size_t n;
    if (g_cache) return;
    pA2 = crb_png(11, 13, 1, &l[0]); jA3 = crb_jpeg(17, 9, 2, 0, &l[1]); tA4 = crb_tiff(12, 7, 0, 3, 3, 0, 3, &l[2]); pA5 = crb_png(30, 20, 4, &l[3]);
    pB2 = crb_png(22, 26, 5, &l[4]); jB3 = crb_jpeg(640, 480, 6, 1, &l[5]); tB4 = crb_tiff(100, 50, 1, 4, 4, 1, 7, &l[6]); pB20 = crb_png(5, 5, 8, &l[7]);
    tC2 = crb_tiff(3, 4, 0, 4, 4, 0, 9, &l[8]); pD21 = crb_png(2, 2, 10, &l[9]);
    memset(A, 0, sizeof A); memset(B, 0, sizeof B); memset(C, 0, sizeof C); memset(D, 0, sizeof D);
    A[0] = (crb_sbrec){ 2, TY_PNG, 3, -2, pA2, l[0], 0 };   A[1] = (crb_sbrec){ 3, TY_JPG, 0, 5, jA3, l[1], 0 };
    A[2] = (crb_sbrec){ 4, TY_TIF, -4, 1, tA4, l[2], 0 };   A[3] = (crb_sbrec){ 5, TY_PNG, 1, 1, pA5, l[3], 0 };
    A[4] = (crb_sbrec){ 6, TY_DUPE, 9, 9, dupe2, 2, 0 };    A[5] = (crb_sbrec){ 7, TY_DUPE, -9, 4, dupe3, 2, 0 };
    A[6] = (crb_sbrec){ 8, TY_DUPE, 0, 0, dupe6, 2, 0 };    /* dupe of a dupe */
    A[7] = (crb_sbrec){ 9, CRB_TAG('p','d','f',' '), 0, 0, junk8, 3, 0 };
    A[8] = (crb_sbrec){ 10, CRB_TAG('m','a','s','k'), 0, 0, junk8, 3, 0 };
    A[9] = (crb_sbrec){ 11, CRB_TAG('f','l','i','p'), 0, 0, junk8, 3, 0 };
    A[10] = (crb_sbrec){ 12, TY_PNG, 0, 0, junk8, 3, 4 };    /* record cut to 4 bytes: shorter than the 8 byte header */
    A[11] = (crb_sbrec){ 14, TY_DUPE, 0, 0, dupe500, 2, 0 };  /* target >= numGlyphs */
    A[12] = (crb_sbrec){ 15, TY_DUPE, 0, 0, dupe9, 2, 0 };    /* target has an unknown type */
    A[13] = (crb_sbrec){ 16, TY_DUPE, 0, 0, dupe12, 2, 0 };   /* target record too short */
    A[14] = (crb_sbrec){ 17, TY_DUPE, 0, 0, dupe17, 2, 0 };   /* dupe of itself */
    B[0] = (crb_sbrec){ 2, TY_PNG, 6, -4, pB2, l[4], 0 };   B[1] = (crb_sbrec){ 3, TY_JPG, 2, 3, jB3, l[5], 0 };
    B[2] = (crb_sbrec){ 4, TY_TIF, 0, 0, tB4, l[6], 0 };    B[3] = (crb_sbrec){ 20, TY_PNG, 0, 0, pB20, l[7], 0 };
    C[0] = (crb_sbrec){ 2, TY_TIF, 1, 2, tC2, l[8], 0 };
    D[0] = (crb_sbrec){ 21, TY_PNG, 0, 0, pD21, l[9], 0 };
    st[0] = (crb_sbstrike){ 20, 72, A, 15 }; st[1] = (crb_sbstrike){ 40, 72, B, 4 }; st[2] = (crb_sbstrike){ 80, 72, C, 1 }; st[3] = (crb_sbstrike){ 0, 72, D, 1 };
    (void)i;
    add_exp(2, 20, TY_PNG, 3, -2, pA2, l[0], 11, 13); add_exp(3, 20, TY_JPG, 0, 5, jA3, l[1], 17, 9); add_exp(4, 20, TY_TIF, -4, 1, tA4, l[2], 12, 7); add_exp(5, 20, TY_PNG, 1, 1, pA5, l[3], 30, 20);
    add_exp(6, 20, TY_PNG, 3, -2, pA2, l[0], 11, 13); add_exp(7, 20, TY_JPG, 0, 5, jA3, l[1], 17, 9);
    add_exp(2, 40, TY_PNG, 6, -4, pB2, l[4], 22, 26); add_exp(3, 40, TY_JPG, 2, 3, jB3, l[5], 640, 480); add_exp(4, 40, TY_TIF, 0, 0, tB4, l[6], 100, 50); add_exp(20, 40, TY_PNG, 0, 0, pB20, l[7], 5, 5);
    add_exp(2, 80, TY_TIF, 1, 2, tC2, l[8], 3, 4);
    add_exp(21, 0, TY_PNG, 0, 0, pD21, l[9], 2, 2);
    raw[0].data = crb_sbix(st, 4, TTF_DEF_NGLYPHS, 1, &n); raw[0].len = (unsigned)n; memcpy(raw[0].tag, "sbix", 5);
    g_cache = crb_blob(raw, 1, 0, &g_cachen);
}
static uint8_t *font(size_t *n) { uint8_t *p; build(); p = (uint8_t *)malloc(g_cachen); memcpy(p, g_cache, g_cachen); *n = g_cachen; return p; }
/* oracle: entry for (gid, fmt) with smallest ppem >= req */
static const sbexp *pick(unsigned gid, uint32_t fmt, unsigned req) {
    int i; const sbexp *best = NULL; build();
    for (i = 0; i < g_nexp; i++) if (g_exp[i].gid == gid && (g_exp[i].fmt & fmt) && g_exp[i].ppem >= req && (!best || g_exp[i].ppem < best->ppem)) best = &g_exp[i];
    return best;
}
static UINT32 range_formats(unsigned gid, unsigned a, unsigned b) {
    int i; UINT32 f = 0; build();
    for (i = 0; i < g_nexp; i++) if (g_exp[i].gid == gid && ((g_exp[i].ppem >= a && g_exp[i].ppem <= b) || g_exp[i].ppem == 0)) f |= g_exp[i].fmt;
    return f;
}
/* nearest-integer check with either neighbour accepted when the exact value is a tie */
static int round_ok(int got, double v) {
    double fl = floor(v), fr = v - fl;
    if (fabs(fr - 0.5) < 1e-3) return got == (int)fl || got == (int)fl + 1 || got == (int)fl + 2;
    return got == (int)floor(v + 0.5);
}

T(sbix_face_formats_union) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    EXPECT_EQ(crb_face_formats(&x), F_TT | F_PNG | F_JPEG | F_TIFF);
    crb_close(&x);
}
T(sbix_per_glyph_formats_grid) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const unsigned r[][2] = { {0,0},{0,19},{0,20},{20,20},{21,39},{20,40},{40,40},{41,79},{41,80},{80,80},{81,1000},{1,1},{0,0xFFFFFFFFu},{1,0xFFFFFFFFu},{0xFFFFFFFFu,0xFFFFFFFFu},{80,20},{100,0} };
    unsigned g, i;
    for (g = 0; g < TTF_DEF_NGLYPHS + 1; g++) for (i = 0; i < sizeof r / sizeof r[0]; i++) {
        UINT32 f; HRESULT hr = crb_gif(&x, (UINT16)g, r[i][0], r[i][1], &f);
        CHECKF(hr == S_OK && f == range_formats(g, r[i][0], r[i][1]), "glyph %u [%u,%u]: %s 0x%x want 0x%x", g, r[i][0], r[i][1], dw_hr_name(hr), f, range_formats(g, r[i][0], r[i][1]));
    }
    crb_close(&x);
}
T(sbix_ppem_zero_strike_matches_every_range) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n); UINT32 f;
    /* a strike with ppem 0 is "size independent": GetGlyphImageFormats includes it even for an empty (first > last) range */
    CHECK_HR_OK(crb_gif(&x, 21, 500, 600, &f)); EXPECT_EQ(f, F_PNG);
    CHECK_HR_OK(crb_gif(&x, 21, 600, 500, &f)); EXPECT_EQ(f, F_PNG);
    /* but GetGlyphImageData(ppem >= 1) cannot select it: it needs entry ppem >= requested */
    { DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; CHECK_HR_OK(crb_gid(&x, 21, 1, F_PNG, &d, &ctx)); CHECK(d.imageData == NULL && ctx == NULL);
      CHECK_HR_OK(crb_gid(&x, 21, 0, F_PNG, &d, &ctx)); CHECK(d.imageData != NULL && d.pixelsPerEm == 0); if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx); }
    crb_close(&x);
}
T(sbix_data_selection_grid) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const UINT32 fm[] = { F_PNG, F_JPEG, F_TIFF, F_SVG, F_PREM }; static const unsigned rq[] = { 0, 1, 10, 19, 20, 21, 39, 40, 41, 79, 80, 81, 200, 65535 };
    unsigned g, i, j;
    for (g = 0; g < 24; g++) for (i = 0; i < 5; i++) for (j = 0; j < sizeof rq / sizeof rq[0]; j++) {
        const sbexp *e = pick(g, fm[i], rq[j]); DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)5; memset(&d, 0xAB, sizeof d);
        HRESULT hr = crb_gid(&x, (UINT16)g, rq[j], fm[i], &d, &ctx);
        if (hr != S_OK) { CHECKF(0, "glyph %u fmt 0x%x req %u: %s", g, fm[i], rq[j], dw_hr_name(hr)); continue; }
        if (e) {
            CHECKF(d.pixelsPerEm == e->ppem && d.imageData && d.imageDataSize == e->len - 0 && !memcmp(d.imageData, e->data, e->len), "glyph %u fmt 0x%x req %u: ppem %u (want %u) size %u (want %u)", g, fm[i], rq[j], d.pixelsPerEm, e->ppem, d.imageDataSize, (unsigned)e->len);
            if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        } else CHECKF(crb_is_zero(&d, sizeof d) && ctx == NULL, "glyph %u fmt 0x%x req %u: expected empty", g, fm[i], rq[j]);
    }
    crb_close(&x);
}
T(sbix_data_fields_for_every_entry) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int i;
    for (i = 0; i < g_nexp; i++) {
        const sbexp *e = &g_exp[i]; DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; const uint8_t *at = crb_find(&x, e->data, e->len);
        CHECK_HR_OK(crb_gid(&x, (UINT16)e->gid, e->ppem, e->fmt, &d, &ctx));
        CHECKF(d.imageData && d.imageDataSize == e->len && !memcmp(d.imageData, e->data, e->len), "glyph %u ppem %u: bytes", e->gid, e->ppem);
        CHECKF(at && d.uniqueDataId == (UINT32)(at - x.blob), "glyph %u ppem %u: uniqueDataId %u want %u", e->gid, e->ppem, d.uniqueDataId, at ? (unsigned)(at - x.blob) : 0);
        CHECKF(d.pixelSize.width == e->w && d.pixelSize.height == e->h, "glyph %u ppem %u: pixelSize %ux%u want %ux%u", e->gid, e->ppem, d.pixelSize.width, d.pixelSize.height, e->w, e->h);
        /* left origin = (-originOffsetX, originOffsetY + height), right origin shares y */
        CHECKF(d.horizontalLeftOrigin.x == -e->ox && d.horizontalLeftOrigin.y == e->oy + (int)e->h, "glyph %u ppem %u: left (%d,%d) want (%d,%d)", e->gid, e->ppem, d.horizontalLeftOrigin.x, d.horizontalLeftOrigin.y, -e->ox, e->oy + (int)e->h);
        CHECKF(d.horizontalRightOrigin.y == d.horizontalLeftOrigin.y, "glyph %u ppem %u: right y %d", e->gid, e->ppem, d.horizontalRightOrigin.y);
        { DWRITE_FONT_METRICS fm; DWRITE_GLYPH_METRICS gm; UINT16 gi = (UINT16)e->gid; IDWriteFontFace_GetMetrics(x.h.face, &fm);
          CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(x.h.face, &gi, 1, &gm, FALSE));
          /* no horizontal advance in the strike: right origin x = left origin x + advanceWidth scaled to the strike ppem */
          CHECKF(round_ok(d.horizontalRightOrigin.x - d.horizontalLeftOrigin.x, (double)gm.advanceWidth * e->ppem / fm.designUnitsPerEm), "glyph %u ppem %u: right x %d left x %d advance %u upm %u", e->gid, e->ppem, d.horizontalRightOrigin.x, d.horizontalLeftOrigin.x, gm.advanceWidth, fm.designUnitsPerEm);
          CHECKF(d.verticalTopOrigin.x == d.verticalBottomOrigin.x, "glyph %u: vertical x differs", e->gid);
          CHECKF(round_ok(d.verticalTopOrigin.x, (double)(gm.advanceWidth / 2) * e->ppem / fm.designUnitsPerEm), "glyph %u ppem %u: vertical x %d", e->gid, e->ppem, d.verticalTopOrigin.x);
          if (fabs((d.verticalBottomOrigin.y - d.verticalTopOrigin.y) - (double)gm.advanceHeight * e->ppem / fm.designUnitsPerEm) > 1.0) t_note("glyph %u ppem %u: vertical advance %d differs from advanceHeight*scale", e->gid, e->ppem, d.verticalBottomOrigin.y - d.verticalTopOrigin.y); }
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    }
    crb_close(&x);
}
T(sbix_dupe_shares_target_data_and_origin) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    static const unsigned pairs[][2] = { { 6, 2 }, { 7, 3 } }; int i;
    for (i = 0; i < 2; i++) {
        DWRITE_GLYPH_IMAGE_DATA a, t; void *ca = NULL, *ct = NULL; UINT32 fm = pairs[i][0] == 6 ? F_PNG : F_JPEG;
        CHECK_HR_OK(crb_gid(&x, (UINT16)pairs[i][0], 20, fm, &a, &ca)); CHECK_HR_OK(crb_gid(&x, (UINT16)pairs[i][1], 20, fm, &t, &ct));
        /* the dupe record's own origin (9,9 / -9,4) is ignored: data, id, size and origin all come from the target record */
        CHECKF(a.uniqueDataId == t.uniqueDataId && a.imageDataSize == t.imageDataSize && !memcmp(&a.pixelSize, &t.pixelSize, sizeof a.pixelSize), "pair %d: dupe differs from target", i);
        CHECKF(a.horizontalLeftOrigin.x == t.horizontalLeftOrigin.x && a.horizontalLeftOrigin.y == t.horizontalLeftOrigin.y, "pair %d: origin (%d,%d) vs (%d,%d)", i, a.horizontalLeftOrigin.x, a.horizontalLeftOrigin.y, t.horizontalLeftOrigin.x, t.horizontalLeftOrigin.y);
        if (ca) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ca); if (ct) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ct);
    }
    crb_close(&x);
}
T(sbix_rejected_records_yield_nothing) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    /* unknown graphic types (9 'pdf ', 10 'mask', 11 'flip'), a 4 byte record (12), dupe of a dupe (8), dupe beyond numGlyphs (14), dupe of an unknown type (15),
     * dupe of a too short record (16) and a dupe of itself (17) */
    static const unsigned bad[] = { 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 }; unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        UINT32 f = 0xFF; DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)1;
        CHECK_HR_OK(crb_gif(&x, (UINT16)bad[i], 0, 0xFFFFFFFFu, &f)); CHECKF(f == 0, "glyph %u formats 0x%x", bad[i], f);
        CHECK_HR_OK(crb_gid(&x, (UINT16)bad[i], 0, F_PNG, &d, &ctx)); CHECKF(d.imageData == NULL && ctx == NULL, "glyph %u", bad[i]);
    }
    crb_close(&x);
}
T(sbix_one_glyph_several_formats_by_strike) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n); UINT32 f;
    /* glyph 2: PNG at 20 and 40, TIFF at 80 */
    CHECK_HR_OK(crb_gif(&x, 2, 0, 100, &f)); EXPECT_EQ(f, F_PNG | F_TIFF);
    CHECK_HR_OK(crb_gif(&x, 2, 50, 100, &f)); EXPECT_EQ(f, F_TIFF);
    CHECK_HR_OK(crb_gif(&x, 2, 0, 30, &f)); EXPECT_EQ(f, F_PNG);
    CHECK_HR_OK(crb_gif(&x, 2, 41, 79, &f)); EXPECT_EQ(f, 0);
    { DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL;
      CHECK_HR_OK(crb_gid(&x, 2, 41, F_PNG, &d, &ctx)); CHECK(d.imageData == NULL);   /* the PNG strikes are 20 and 40: nothing >= 41 */
      CHECK_HR_OK(crb_gid(&x, 2, 41, F_TIFF, &d, &ctx)); CHECK(d.imageData != NULL && d.pixelsPerEm == 80); if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
      CHECK_HR_OK(crb_gid(&x, 2, 21, F_PNG, &d, &ctx)); CHECK(d.imageData != NULL && d.pixelsPerEm == 40); if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx); }
    crb_close(&x);
}
T(sbix_version_zero_table_is_ignored) {
    crb_sbstrike st; crb_sbrec r; size_t l, n; ttf_rawtable raw[1]; uint8_t *png = crb_png(4, 4, 1, &l), *out;
    r = (crb_sbrec){ 2, TY_PNG, 0, 0, png, l, 0 }; st = (crb_sbstrike){ 20, 72, &r, 1 };
    raw[0].data = crb_sbix(&st, 1, TTF_DEF_NGLYPHS, 0, &n); raw[0].len = (unsigned)n; memcpy(raw[0].tag, "sbix", 5);
    out = crb_blob(raw, 1, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        UINT32 f; EXPECT_EQ(crb_face_formats(&x), F_TT);
        CHECK_HR_OK(crb_gif(&x, 2, 0, 0xFFFFFFFFu, &f)); EXPECT_EQ(f, 0);
        crb_close(&x);
    } else crb_close(&x); }
    free((void *)raw[0].data); free(png);
}
T(sbix_version_one_table_same_content_is_accepted) {
    crb_sbstrike st; crb_sbrec r; size_t l, n; ttf_rawtable raw[1]; uint8_t *png = crb_png(4, 4, 1, &l), *out;
    r = (crb_sbrec){ 2, TY_PNG, 0, 0, png, l, 0 }; st = (crb_sbstrike){ 20, 72, &r, 1 };
    raw[0].data = crb_sbix(&st, 1, TTF_DEF_NGLYPHS, 1, &n); raw[0].len = (unsigned)n; memcpy(raw[0].tag, "sbix", 5);
    out = crb_blob(raw, 1, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        UINT32 f; EXPECT_EQ(crb_face_formats(&x), F_TT | F_PNG);
        CHECK_HR_OK(crb_gif(&x, 2, 0, 0xFFFFFFFFu, &f)); EXPECT_EQ(f, F_PNG);
        crb_close(&x);
    } else crb_close(&x); }
    free((void *)raw[0].data); free(png);
}

/* ---- image dimension parsing ---------------------------------------------------------------------------------------------------- */
typedef struct { uint8_t *p; size_t n; uint32_t ty; unsigned w, h; const char *what; } dimcase;
static void dims_run(dimcase *c, int nc) {
    crb_sbstrike st; crb_sbrec *r = (crb_sbrec *)calloc((size_t)nc, sizeof *r); size_t n; ttf_rawtable raw[1]; uint8_t *out; int i;
    for (i = 0; i < nc; i++) r[i] = (crb_sbrec){ 2 + (unsigned)i, c[i].ty, 0, 0, c[i].p, c[i].n, 0 };
    st = (crb_sbstrike){ 32, 72, r, nc };
    raw[0].data = crb_sbix(&st, 1, TTF_DEF_NGLYPHS, 1, &n); raw[0].len = (unsigned)n; memcpy(raw[0].tag, "sbix", 5);
    out = crb_blob(raw, 1, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        for (i = 0; i < nc; i++) {
            DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; UINT32 fm = c[i].ty == TY_PNG ? F_PNG : c[i].ty == TY_JPG ? F_JPEG : F_TIFF;
            CHECK_HR_OK(crb_gid(&x, (UINT16)(2 + i), 32, fm, &d, &ctx));
            CHECKF(d.imageData != NULL, "%s: not accepted", c[i].what);
            CHECKF(d.pixelSize.width == c[i].w && d.pixelSize.height == c[i].h, "%s: pixelSize %ux%u want %ux%u", c[i].what, d.pixelSize.width, d.pixelSize.height, c[i].w, c[i].h);
            if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        }
        crb_close(&x);
    } else crb_close(&x); }
    free((void *)raw[0].data); free(r);
}
T(sbix_png_dimensions) {
    static const unsigned d[][2] = { {1,1},{7,5},{300,200},{65535,1},{1,65535},{65535,65535},{256,256},{255,257},{70000,80000},{0,0},{16,16},{128,64} };
    static const unsigned ew[] = { 1,7,300,65535,1,65535,256,255,65535,0,16,128 }, eh[] = { 1,5,200,1,65535,65535,256,257,65535,0,16,64 };
    dimcase c[12]; int i;
    for (i = 0; i < 12; i++) { c[i].p = crb_png(d[i][0], d[i][1], 0x500 + (unsigned)i, &c[i].n); c[i].ty = TY_PNG; c[i].w = ew[i]; c[i].h = eh[i]; c[i].what = "png"; }
    dims_run(c, 12);
    for (i = 0; i < 12; i++) free(c[i].p);
}
T(sbix_jpeg_dimensions) {
    static const unsigned d[][2] = { {5,3},{640,480},{65535,1},{1,1},{4000,3000},{256,255} }; int st, i, k = 0; dimcase c[24];
    for (st = 0; st < 3; st++) for (i = 0; i < 6; i++, k++) { c[k].p = crb_jpeg(d[i][0], d[i][1], 0x600 + (unsigned)k, st, &c[k].n); c[k].ty = TY_JPG; c[k].w = d[i][0]; c[k].h = d[i][1]; c[k].what = "jpeg"; }
    dims_run(c, k);
    for (i = 0; i < k; i++) free(c[i].p);
}
T(sbix_jpeg_start_of_scan_before_frame_gives_zero_size) {
    dimcase c[2]; int i;
    for (i = 0; i < 2; i++) { c[i].p = crb_jpeg(10 + (unsigned)i, 20, 0x610 + (unsigned)i, 3, &c[i].n); c[i].ty = TY_JPG; c[i].w = 0; c[i].h = 0; c[i].what = "jpeg sos first"; }
    dims_run(c, 2);
    for (i = 0; i < 2; i++) free(c[i].p);
}
T(sbix_tiff_dimensions_by_byte_order_and_field_type) {
    static const struct { int be, wt, ht, extra; unsigned w, h; } t[] = {
        { 0, 3, 3, 0, 12, 7 }, { 0, 4, 4, 0, 1200, 700 }, { 1, 3, 3, 0, 33, 44 }, { 1, 4, 4, 0, 3300, 4400 }, { 0, 3, 4, 0, 99, 100000 }, { 1, 4, 3, 0, 100000, 99 },
        { 0, 1, 1, 0, 5, 6 }, { 1, 1, 1, 0, 7, 8 }, { 0, 6, 6, 0, 9, 10 }, { 0, 8, 8, 0, 300, 301 }, { 1, 8, 8, 0, 302, 303 }, { 0, 9, 9, 0, 70000, 80000 },
        { 0, 3, 3, 1, 21, 22 }, { 1, 4, 4, 1, 23, 24 } };
    dimcase c[14]; int i;
    for (i = 0; i < 14; i++) {
        c[i].p = crb_tiff(t[i].w, t[i].h, t[i].be, t[i].wt, t[i].ht, t[i].extra, 0x700 + (unsigned)i, &c[i].n); c[i].ty = TY_TIF; c[i].w = t[i].w; c[i].h = t[i].h; c[i].what = "tiff";
        if (t[i].wt == 1 || t[i].wt == 6 || t[i].ht == 1 || t[i].ht == 6) { if (t[i].w > 255 || t[i].h > 255) c[i].w = c[i].h = 0; }
    }
    dims_run(c, 14);
    for (i = 0; i < 14; i++) free(c[i].p);
}
T(sbix_bad_signatures_give_zero_size) {
    dimcase c[3]; size_t n;
    c[0].p = crb_png(9, 9, 1, &n); c[0].n = n; c[0].p[1] = 'X'; c[0].ty = TY_PNG; c[0].w = c[0].h = 0; c[0].what = "png bad signature";
    c[1].p = crb_jpeg(9, 9, 2, 0, &n); c[1].n = n; c[1].p[0] = 0xFE; c[1].ty = TY_JPG; c[1].w = c[1].h = 0; c[1].what = "jpeg without SOI";
    c[2].p = crb_tiff(9, 9, 0, 3, 3, 0, 3, &n); c[2].n = n; c[2].p[2] = 43; c[2].ty = TY_TIF; c[2].w = c[2].h = 0; c[2].what = "tiff with wrong magic";
    dims_run(c, 3);
    free(c[0].p); free(c[1].p); free(c[2].p);
}
T(sbix_data_is_not_decoded_or_validated_beyond_the_header) {
    /* trailing bytes after the header are returned verbatim, record length - 8 is the data size */
    crb_sbstrike st; crb_sbrec r; size_t l, n; ttf_rawtable raw[1]; uint8_t *png = crb_png(4, 4, 1, &l), *big = (uint8_t *)malloc(l + 100), *out; int i;
    memcpy(big, png, l); for (i = 0; i < 100; i++) big[l + i] = (uint8_t)(i * 7);
    r = (crb_sbrec){ 2, TY_PNG, 0, 0, big, l + 100, 0 }; st = (crb_sbstrike){ 20, 72, &r, 1 };
    raw[0].data = crb_sbix(&st, 1, TTF_DEF_NGLYPHS, 1, &n); raw[0].len = (unsigned)n; memcpy(raw[0].tag, "sbix", 5);
    out = crb_blob(raw, 1, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; CHECK_HR_OK(crb_gid(&x, 2, 20, F_PNG, &d, &ctx));
        CHECKF(d.imageDataSize == l + 100 && !memcmp(d.imageData, big, l + 100), "size %u want %u", d.imageDataSize, (unsigned)(l + 100));
        if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        crb_close(&x);
    } else crb_close(&x); }
    free((void *)raw[0].data); free(png); free(big);
}
T(sbix_and_cbdt_in_one_font_merge_by_ppem) {
    /* both readers feed the same per glyph list: the smallest ppem >= request wins regardless of the source table */
    size_t l, n, nc, nd; crb_sbstrike st; crb_sbrec r; crb_cbstrike cs; crb_cbglyph cg; ttf_rawtable raw[3]; uint8_t *lc, *dt, *out;
    uint8_t *spng = crb_png(10, 10, 0xA1, &l), *cpng = crb_png(5, 5, 0xA2, &nc);
    r = (crb_sbrec){ 2, TY_PNG, 0, 0, spng, l, 0 }; st = (crb_sbstrike){ 40, 72, &r, 1 };
    memset(&cs, 0, sizeof cs); memset(&cg, 0, sizeof cg); cs.ppem_x = cs.ppem_y = 24; cs.depth = 32; cs.imgfmt = 17; cs.idxfmt = 1; cs.g = &cg; cs.ng = 1;
    cg.gid = 2; cg.img = cpng; cg.imglen = nc; cg.m.h = 5; cg.m.w = 5; cg.m.adv = 6;
    crb_cbtables(&cs, 1, &lc, &n, &dt, &nd);
    raw[0].data = crb_sbix(&st, 1, TTF_DEF_NGLYPHS, 1, &nc); raw[0].len = (unsigned)nc; memcpy(raw[0].tag, "sbix", 5);
    raw[1].data = lc; raw[1].len = (unsigned)n; memcpy(raw[1].tag, "CBLC", 5); raw[2].data = dt; raw[2].len = (unsigned)nd; memcpy(raw[2].tag, "CBDT", 5);
    out = crb_blob(raw, 3, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; UINT32 f;
        CHECK_HR_OK(crb_gif(&x, 2, 0, 100, &f)); EXPECT_EQ(f, F_PNG);
        CHECK_HR_OK(crb_gid(&x, 2, 10, F_PNG, &d, &ctx)); EXPECT_EQ(d.pixelsPerEm, 24); if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        CHECK_HR_OK(crb_gid(&x, 2, 25, F_PNG, &d, &ctx)); EXPECT_EQ(d.pixelsPerEm, 40); if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        CHECK_HR_OK(crb_gid(&x, 2, 41, F_PNG, &d, &ctx)); CHECK(d.imageData == NULL);
        crb_close(&x);
    } else crb_close(&x); }
    free((void *)raw[0].data); free(lc); free(dt); free(spng); free(cpng);
}
