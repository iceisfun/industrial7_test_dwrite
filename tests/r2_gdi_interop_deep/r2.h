/* Shared fixtures of the r2_gdi_interop_deep area: system fonts reached through the GDI interop, a GDI memory DC with the same
 * LOGFONT selected, and conversions between LOGFONT heights and pixel sizes.
 *
 * Facts the expectations rest on:
 *  - IDWriteGdiInterop::CreateFontFromLOGFONT resolves lfFaceName/lfWeight/lfItalic/lfCharSet only; lfHeight/lfWidth/... are ignored.
 *  - IDWriteFontFace::GetGdiCompatibleGlyphMetrics runs the glyph metrics path with DWRITE_MEASURING_MODE_GDI_CLASSIC (useGdiNatural
 *    FALSE) or GDI_NATURAL (TRUE) after validating (indices != NULL, emSize > 0, pixelsPerDip > 0); the glyph metrics array is zeroed
 *    first, even when validation then fails.
 *  - GDI: lfHeight < 0 is the em size in pixels; lfHeight > 0 is the cell height (ascent + descent) in pixels;
 *    TEXTMETRIC.tmHeight - tmInternalLeading is the em size, OUTLINETEXTMETRIC.otmEMSquare is head.unitsPerEm for TrueType.
 */
#ifndef R2_H
#define R2_H
#include "dwx.h"
#include <math.h>

#define R2_NAMES 12
static const char *const r2_fonts[R2_NAMES] = { "Arial", "Times New Roman", "Courier New", "Segoe UI", "Tahoma", "Verdana",
    "Consolas", "Georgia", "Trebuchet MS", "Calibri", "Cambria", "Lucida Console" };

static const int r2_px[] = { 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 21, 24, 28, 32, 36, 40, 48, 64, 72 };
#define R2_NPX ((int)(sizeof r2_px / sizeof r2_px[0]))
static const float r2_ppd[] = { 1.0f, 1.25f, 2.0f };

typedef struct r2_ctx {
    IDWriteFactory *f; IDWriteGdiInterop *g;
    IDWriteFont *font; IDWriteFontFace *face; IDWriteFontFace1 *f1;
    HDC dc; HFONT hf, old;
    char name[64]; WCHAR wname[LF_FACESIZE];
    LONG upm;
} r2_ctx;

static __inline void r2_lf(LOGFONTW *lf, const char *face, int weight, int italic, int height, int quality)
{
    memset(lf, 0, sizeof *lf);
    dw_wide(face, lf->lfFaceName, LF_FACESIZE);
    lf->lfWeight = weight; lf->lfItalic = (BYTE)italic; lf->lfCharSet = DEFAULT_CHARSET; lf->lfHeight = height; lf->lfQuality = (BYTE)quality;
}

static __inline void r2_close(r2_ctx *c)
{
    if (c->dc && c->old) SelectObject(c->dc, c->old);
    if (c->hf) DeleteObject(c->hf);
    if (c->dc) DeleteDC(c->dc);
    DW_RELEASE(c->f1); DW_RELEASE(c->face); DW_RELEASE(c->font); DW_RELEASE(c->g);
    memset(c, 0, sizeof *c);
}

/* Opens the face a LOGFONT resolves to (through the interop) and a memory DC; GDI font selection is separate (r2_select).
 * Returns 1 on success.  On failure everything is released and the test is skipped. */
static __inline int r2_open(r2_ctx *c, const char *face, int weight, int italic)
{
    LOGFONTW lf; HRESULT hr; DWRITE_FONT_METRICS m;
    memset(c, 0, sizeof *c);
    snprintf(c->name, sizeof c->name, "%s", face); dw_wide(face, c->wname, LF_FACESIZE);
    c->f = dw_shared();
    if (!c->f) { t_skip("no factory"); return 0; }
    if (!dw_have_font(c->f, c->wname)) { t_skip("%s not installed", face); return 0; }
    if (FAILED(IDWriteFactory_GetGdiInterop(c->f, &c->g)) || !c->g) { t_skip("no interop"); r2_close(c); return 0; }
    r2_lf(&lf, face, weight, italic, 0, 0);
    hr = IDWriteGdiInterop_CreateFontFromLOGFONT(c->g, &lf, &c->font);
    if (FAILED(hr) || !c->font) { t_skip("CreateFontFromLOGFONT(%s) -> %s", face, dw_hr_name(hr)); r2_close(c); return 0; }
    if (FAILED(IDWriteFont_CreateFontFace(c->font, &c->face)) || !c->face) { t_skip("CreateFontFace failed"); r2_close(c); return 0; }
    if (FAILED(IDWriteFontFace_QueryInterface(c->face, &IID_IDWriteFontFace1, (void **)&c->f1))) c->f1 = NULL;
    IDWriteFontFace_GetMetrics(c->face, &m); c->upm = m.designUnitsPerEm;
    c->dc = CreateCompatibleDC(NULL);
    if (!c->dc) { t_skip("no DC"); r2_close(c); return 0; }
    return 1;
}
#define R2_OPEN(c, face, w, it) r2_ctx c; do { if (!r2_open(&c, face, w, it)) return; } while (0)

/* Selects a fresh HFONT for the face with the given LOGFONT height/quality; 0 when GDI substituted another face. */
static __inline int r2_select(r2_ctx *c, int weight, int italic, int height, int quality)
{
    LOGFONTW lf; WCHAR got[LF_FACESIZE + 4];
    if (c->old) { SelectObject(c->dc, c->old); c->old = NULL; }
    if (c->hf) { DeleteObject(c->hf); c->hf = NULL; }
    r2_lf(&lf, c->name, weight, italic, height, quality);
    c->hf = CreateFontIndirectW(&lf);
    if (!c->hf) return 0;
    c->old = (HFONT)SelectObject(c->dc, c->hf);
    got[0] = 0; GetTextFaceW(c->dc, LF_FACESIZE + 4, got);
    if (lstrcmpiW(got, c->wname)) return 0;
    return 1;
}
/* GDI quality constants (wingdi.h): */
#define R2_Q_NONAA   NONANTIALIASED_QUALITY
#define R2_Q_AA      ANTIALIASED_QUALITY
#define R2_Q_CT      CLEARTYPE_QUALITY
#define R2_Q_CTNAT   CLEARTYPE_NATURAL_QUALITY

static __inline UINT16 r2_gid(r2_ctx *c, UINT32 cp)
{
    UINT16 g = 0;
    if (FAILED(IDWriteFontFace_GetGlyphIndices(c->face, &cp, 1, &g))) return 0;
    return g;
}
static __inline int r2_iabs(int v) { return v < 0 ? -v : v; }
/* round-half-up scale of design units to pixels */
static __inline double r2_du2px(int du, double ppem, int upm) { return (double)du * ppem / (double)upm; }

/* Probe characters present in every Latin text font */
static const WCHAR r2_chars[] = { 'A', 'H', 'W', 'g', 'i', 'm', 'j', 'y', '0', 'O', '.', ',', 'l', 'x', 'T', 'e', 'o', 'f', 'Q', '1' };
#define R2_NCH ((int)(sizeof r2_chars / sizeof r2_chars[0]))


/* Identity of the file/face index/simulations behind a face: "path|index|sim" (path lower-cased); 0 when not a local file */
static __inline int r2_face_key(IDWriteFontFace *face, char *out, size_t cap)
{
    IDWriteFontFile *files[4]; UINT32 n = 4, i; int ok = 0;
    out[0] = 0;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, NULL)) || n == 0 || n > 4) return 0;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, files))) return 0;
    {
        IDWriteFontFileLoader *ld = NULL; const void *key = NULL; UINT32 ks = 0; IDWriteLocalFontFileLoader *ll = NULL; WCHAR path[MAX_PATH * 2];
        if (SUCCEEDED(IDWriteFontFile_GetLoader(files[0], &ld)) && ld &&
            SUCCEEDED(IDWriteFontFileLoader_QueryInterface(ld, &IID_IDWriteLocalFontFileLoader, (void **)&ll)) && ll &&
            SUCCEEDED(IDWriteFontFile_GetReferenceKey(files[0], &key, &ks)) &&
            SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, path, MAX_PATH * 2))) {
            char u8[MAX_PATH * 3], *p; dw_narrow(path, u8, sizeof u8);
            for (p = u8; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
            snprintf(out, cap, "%s|%u|%d", u8, (unsigned)IDWriteFontFace_GetIndex(face), (int)IDWriteFontFace_GetSimulations(face));
            ok = 1;
        }
        if (ll) IDWriteLocalFontFileLoader_Release(ll);
        if (ld) IDWriteFontFileLoader_Release(ld);
    }
    for (i = 0; i < n; i++) IDWriteFontFile_Release(files[i]);
    return ok;
}
/* OS/2 fsSelection bit 7 (USE_TYPO_METRICS): 1/0, or -1 when the OS/2 table is unavailable */
static __inline int r2_use_typo(IDWriteFontFace *face)
{
    UINT32 len = 0; uint8_t *t = dw_face_table(face, DW_TAG('O', 'S', '/', '2'), &len); int r = -1;
    if (t && len >= 64) r = (t[62 + 1] & 0x80) ? 1 : 0;       /* fsSelection is big endian at offset 62: bit 7 is in the low byte */
    free(t);
    return r;
}
/* base name (after the last backslash) of a key made by r2_face_key plus "|index" */
static __inline void r2_key_base(const char *key, char *out, size_t cap)
{
    const char *bar = strrchr(key, '|'), *sl = key, *p;
    for (p = key; bar && p < bar; p++) if (*p == '\\' || *p == '/') sl = p + 1;
    snprintf(out, cap, "%.*s", bar ? (int)(bar - sl) : (int)strlen(sl), sl);
    if (bar && strlen(out) + strlen(bar) < cap) strcat(out, bar);
}
/* OUTLINETEXTMETRICW for the DC's font (malloc'd, caller frees) or NULL */
static __inline OUTLINETEXTMETRICW *r2_otm(HDC dc)
{
    UINT n = GetOutlineTextMetricsW(dc, 0, NULL); OUTLINETEXTMETRICW *o;
    if (!n) return NULL;
    o = (OUTLINETEXTMETRICW *)calloc(1, n);
    if (!o) return NULL;
    if (!GetOutlineTextMetricsW(dc, n, o)) { free(o); return NULL; }
    return o;
}

/* Whole-test helper: iterate over the fonts that exist */
#define R2_FOR_EACH_FONT_TEST(prefix, body_fn) \
    TEST(prefix##_arial) { body_fn("Arial"); } \
    TEST(prefix##_times) { body_fn("Times New Roman"); } \
    TEST(prefix##_courier) { body_fn("Courier New"); } \
    TEST(prefix##_segoeui) { body_fn("Segoe UI"); } \
    TEST(prefix##_tahoma) { body_fn("Tahoma"); } \
    TEST(prefix##_verdana) { body_fn("Verdana"); } \
    TEST(prefix##_consolas) { body_fn("Consolas"); } \
    TEST(prefix##_georgia) { body_fn("Georgia"); } \
    TEST(prefix##_trebuchet) { body_fn("Trebuchet MS"); } \
    TEST(prefix##_calibri) { body_fn("Calibri"); } \
    TEST(prefix##_cambria) { body_fn("Cambria"); } \
    TEST(prefix##_lucida) { body_fn("Lucida Console"); }
#endif
