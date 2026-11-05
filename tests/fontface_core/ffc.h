/* Shared scaffolding for the fontface_core area (IDWriteFontFace and its v1/v2/v3 additions).
 *
 * Every test runs against its OWN isolated factory (FTEST) so that face / glyph / metric caches keyed by (loader, key)
 * can never leak from one synthetic font into the next, and every font file loader registered by a test is unregistered
 * (dwx_font_close) before the test ends. */
#ifndef FFC_H
#define FFC_H
#include "dwx.h"

/* A test with an isolated factory `f`. */
#define FTEST(n) \
    static void body_##n(IDWriteFactory *f); \
    TEST(n) { \
        IDWriteFactory *f_ = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); \
        if (!f_) { t_skip("DWriteCreateFactory unavailable"); return; } \
        body_##n(f_); \
        IDWriteFactory_Release(f_); \
    } \
    static void body_##n(IDWriteFactory *f)

static __inline unsigned ffc_be16(const uint8_t *p) { return ((unsigned)p[0] << 8) | p[1]; }
static __inline int ffc_s16(const uint8_t *p) { return (int)(short)ffc_be16(p); }
static __inline unsigned ffc_be32(const uint8_t *p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }

/* Builds `s`, opens it as a font face through the whole loader -> file -> Analyze -> CreateFontFace chain.
 * Returns the face (owned by h; call dwx_font_close(h) at the end) or NULL after t_skip. */
static IDWriteFontFace *ffc_open_idx(dwx_font *h, IDWriteFactory *f, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim, UINT32 idx) {
    size_t n = 0; uint8_t *b = ttf_build(s, &n); HRESULT hr;
    memset(h, 0, sizeof *h);
    if (!b) { t_skip("ttf_build failed"); return NULL; }
    hr = dwx_font_open(h, f, b, n, idx, sim);
    free(b);
    if (FAILED(hr) || !h->face) { t_skip("font open failed: %s", dw_hr_name(hr)); dwx_font_close(h); return NULL; }
    return h->face;
}
static IDWriteFontFace *ffc_open(dwx_font *h, IDWriteFactory *f, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim) { return ffc_open_idx(h, f, s, sim, 0); }
/* default synthetic font, no simulations */
static IDWriteFontFace *ffc_open_def(dwx_font *h, IDWriteFactory *f) { ttf_spec s; ttf_default_spec(&s); return ffc_open(h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); }

/* Face interface at version v (1..3) obtained through QueryInterface; NULL when unsupported. Caller releases. */
static void *ffc_face_v(IDWriteFontFace *face, int v) {
    static const IID *ids[4];
    void *p = NULL;
    ids[1] = &IID_IDWriteFontFace1; ids[2] = &IID_IDWriteFontFace2; ids[3] = &IID_IDWriteFontFace3;
    if (v < 1 || v > 3) return NULL;
    if (FAILED(IUnknown_QueryInterface((IUnknown *)face, ids[v], &p))) return NULL;
    return p;
}

/* Raw bytes of a table inside a freshly built blob (what TryGetFontTable must reproduce). */
static const uint8_t *ffc_blob_table(const uint8_t *blob, size_t n, const char *tag, unsigned *len) { return ttf_find_table(blob, n, tag, len); }

/* Fixed-point rounding used by the oblique simulation: (short)((value * 0x5700) >> 16), arithmetic shift */
static __inline int ffc_shear(int v) { return (v * 0x5700) >> 16; }

/* Default synthetic font facts (see common/ttfgen.h) */
#define FFC_UPM 2048

/* Two-glyph-class fonts used by several files: the default font with the metric inputs replaced. */
static void ffc_spec(ttf_spec *s) { ttf_default_spec(s); }

/* A glyph table that is the default font plus extra glyphs (the extras are appended after gid 68). */
#define FFC_MAXG 1100
static ttf_glyph ffc_glyph_store[FFC_MAXG];
static __inline ttf_glyph *ffc_glyphs_default(int *n) {
    int dn = 0; const ttf_glyph *d = ttf_default_glyphs(&dn);
    memcpy(ffc_glyph_store, d, (size_t)dn * sizeof *d);
    *n = dn;
    return ffc_glyph_store;
}
#endif
