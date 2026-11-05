/* Shared fixtures for the gdi_interop area (IDWriteGdiInterop / IDWriteGdiInterop1).
 *
 * Facts from the Windows 10 probing that the expectations rest on:
 *  - every LOGFONT-producing method memsets the whole 0x5c-byte LOGFONTW first, then fills lfWeight, lfItalic, lfCharSet
 *    (DEFAULT_CHARSET=1, SYMBOL_CHARSET=2 for symbol fonts), lfOutPrecision (OUT_OUTLINE_PRECIS=8) and lfFaceName (<= 31 chars).
 *  - a LOGFONT is resolved by Win32 family name (informational string WIN32_FAMILY_NAMES, falling back to the full name) with a
 *    penalty function over (weight, italic, code page mask, stretch); see gi_score().
 *  - synthetic bold is requested when lfWeight - fontWeight >= 151 and ConvertFontToLOGFONT then reports weight + 151.
 */
#ifndef GI_H
#define GI_H
#include "dwx.h"

#define GI_BOLD_SIM_DELTA 151        /* 0x97 */
#define GI_FACE_MAX 32

static __inline IDWriteGdiInterop *gi_get(IDWriteFactory *f)
{
    IDWriteGdiInterop *g = NULL;
    HRESULT hr = IDWriteFactory_GetGdiInterop(f, &g);
    if (FAILED(hr) || !g) return NULL;
    return g;
}
static __inline IDWriteGdiInterop1 *gi_get1(IDWriteGdiInterop *g)
{
    IDWriteGdiInterop1 *g1 = NULL;
    if (FAILED(IDWriteGdiInterop_QueryInterface(g, &IID_IDWriteGdiInterop1, (void **)&g1))) return NULL;
    return g1;
}
static __inline void gi_lf(LOGFONTW *lf, const char *face, int weight, int italic, int charset)
{
    memset(lf, 0, sizeof *lf);
    if (face) dw_wide(face, lf->lfFaceName, LF_FACESIZE);
    lf->lfWeight = weight; lf->lfItalic = (BYTE)italic; lf->lfCharSet = (BYTE)charset;
}
static __inline void gi_lfw(LOGFONTW *lf, const WCHAR *face, int weight, int italic, int charset)
{
    memset(lf, 0, sizeof *lf);
    if (face) lstrcpynW(lf->lfFaceName, face, LF_FACESIZE);
    lf->lfWeight = weight; lf->lfItalic = (BYTE)italic; lf->lfCharSet = (BYTE)charset;
}
#define GI_POISON_LF(lf) memset(&(lf), 0xCC, sizeof(lf))

/* Nonzero when the LOGFONT looks like the output of a convert call (every field except weight/italic/charset/facename fixed). */
static __inline void gi_check_lf_shape(const LOGFONTW *lf, const char *file, int line)
{
    t_check(lf->lfHeight == 0 && lf->lfWidth == 0 && lf->lfEscapement == 0 && lf->lfOrientation == 0, file, line,
            "lfHeight/lfWidth/lfEscapement/lfOrientation == 0", "%ld %ld %ld %ld", lf->lfHeight, lf->lfWidth, lf->lfEscapement, lf->lfOrientation);
    t_check(lf->lfUnderline == 0 && lf->lfStrikeOut == 0, file, line, "lfUnderline/lfStrikeOut == 0", "%d %d", lf->lfUnderline, lf->lfStrikeOut);
    t_check(lf->lfOutPrecision == OUT_OUTLINE_PRECIS, file, line, "lfOutPrecision == OUT_OUTLINE_PRECIS", "got %d", lf->lfOutPrecision);
    t_check(lf->lfClipPrecision == 0 && lf->lfQuality == 0 && lf->lfPitchAndFamily == 0, file, line, "clip/quality/pitch == 0", "%d %d %d",
            lf->lfClipPrecision, lf->lfQuality, lf->lfPitchAndFamily);
    t_check(lf->lfCharSet == DEFAULT_CHARSET || lf->lfCharSet == SYMBOL_CHARSET, file, line, "lfCharSet is DEFAULT or SYMBOL", "got %d", lf->lfCharSet);
    /* the face name is NUL-terminated within 31 chars and everything after the terminator was zeroed by the memset */
    {
        int n = (int)lstrlenW(lf->lfFaceName), i, ok = 1;
        for (i = n; i < LF_FACESIZE; i++) if (lf->lfFaceName[i]) ok = 0;
        t_check(n <= 31 && ok, file, line, "lfFaceName terminated, tail zero", "len %d", n);
    }
}
#define GI_LF_SHAPE(lf) gi_check_lf_shape(&(lf), __FILE__, __LINE__)

static __inline int gi_lf_all_zero(const LOGFONTW *lf)
{
    const BYTE *b = (const BYTE *)lf; size_t i;
    for (i = 0; i < sizeof *lf; i++) if (b[i]) return 0;
    return 1;
}

/* lowercase base name of the file backing a face ("arial.ttf"); returns 0 when it is not a local file */
static __inline int gi_face_file(IDWriteFontFace *face, char *out, size_t cap, char *full, size_t fcap)
{
    IDWriteFontFile *files[4]; UINT32 n = 4; int ok = 0; WCHAR path[MAX_PATH * 2]; char u8[MAX_PATH * 3];
    out[0] = 0; if (full) full[0] = 0;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, NULL)) || n == 0 || n > 4) return 0;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, files))) return 0;
    {
        IDWriteFontFileLoader *ld = NULL; const void *key = NULL; UINT32 ks = 0; IDWriteLocalFontFileLoader *ll = NULL;
        if (SUCCEEDED(IDWriteFontFile_GetLoader(files[0], &ld)) && ld &&
            SUCCEEDED(IDWriteFontFileLoader_QueryInterface(ld, &IID_IDWriteLocalFontFileLoader, (void **)&ll)) && ll &&
            SUCCEEDED(IDWriteFontFile_GetReferenceKey(files[0], &key, &ks)) &&
            SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, path, MAX_PATH * 2))) {
            char *p, *base;
            dw_narrow(path, u8, sizeof u8);
            for (p = u8; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
            base = strrchr(u8, '\\'); base = base ? base + 1 : u8;
            snprintf(out, cap, "%s", base);
            if (full) snprintf(full, fcap, "%s", u8);
            ok = 1;
        }
        if (ll) IDWriteLocalFontFileLoader_Release(ll);
        if (ld) IDWriteFontFileLoader_Release(ld);
    }
    { UINT32 i; for (i = 0; i < n; i++) IDWriteFontFile_Release(files[i]); }
    return ok;
}

/* ---- penalty model (Win32ComputePenalty / FindNearestMatchingWin32Font) ------------------------------------------- */
typedef struct { int weight; int style; /* DWRITE_FONT_STYLE */ unsigned cp; int stretch; } gi_ent;
static __inline unsigned gi_charset_mask(unsigned cs)
{
    switch (cs) {
    case 0: return 1; case 1: return 0xffffffffu; case 2: return 0x80000000u; case 0x4d: return 0x20000000u;
    case 0x80: return 0x20000; case 0x81: return 0x80000; case 0x82: return 0x200000; case 0x86: return 0x40000;
    case 0x88: return 0x100000; case 0xa1: return 8; case 0xa2: return 0x10; case 0xa3: return 0x100; case 0xb1: return 0x20;
    case 0xb2: return 0x40; case 0xba: return 0x80; case 0xcc: return 4; case 0xde: return 0x10000; case 0xee: return 2;
    case 0xff: return 0x40000000u; default: return 0;
    }
}
/* score of one entry for a request; *sim receives the DWRITE_FONT_SIMULATIONS the match would carry */
static __inline unsigned gi_score(const gi_ent *e, unsigned csmask, int reqw, int reqit, int *sim)
{
    unsigned pen = 0; int s = 0, d, absd; unsigned total;
    if (!reqit) { pen = e->style != 0 ? 4 : 0; }
    else {
        if (e->style != 2) s = e->style != 1 ? DWRITE_FONT_SIMULATIONS_OBLIQUE : 0;
        pen = e->style != 2 ? 1 : 0;
    }
    d = reqw - e->weight;
    if (d != 0) {
        if (d < 0) absd = -d;
        else if (d < GI_BOLD_SIM_DELTA) absd = d;
        else { s |= DWRITE_FONT_SIMULATIONS_BOLD; absd = d - 120; }
        pen += (unsigned)((absd * 0x49) / 0x100);
    }
    *sim = s;
    total = pen + ((csmask & e->cp) ? 0 : 65000);
    return (unsigned)(e->stretch > 5 ? e->stretch - 5 : 5 - e->stretch) + total * 10;
}
/* index of the unique best entry or -1 on a tie; ties are not asserted because the order of equal scores is not specified */
static __inline int gi_best(const gi_ent *ents, int n, unsigned csmask, int reqw, int reqit, int *sim_out, unsigned *scores)
{
    int i, best = 0, sim, ties = 0; unsigned bs = 0xffffffffu;
    for (i = 0; i < n; i++) {
        unsigned sc = gi_score(&ents[i], csmask, reqw, reqit, &sim);
        if (scores) scores[i] = sc;
        if (sc < bs) { bs = sc; best = i; ties = 0; if (sim_out) *sim_out = sim; }
        else if (sc == bs) ties++;
    }
    return ties ? -1 : best;
}

/* ---- custom collection fixture: isolated factory, synthetic fonts, interop objects ----------------------------------- */
typedef struct gi_cf {
    const char *family; int weight; int italic; int width;
    unsigned cp0;            /* OS/2 ulCodePageRange1 (0 = default 1) */
    unsigned cp1;
    unsigned uni[4];         /* OS/2 ulUnicodeRange (all zero = default) */
    int symbol;              /* (3,0) symbol cmap instead of (3,1) */
    int upm;
} gi_cf;
typedef struct gi_fix {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl;
    IDWriteFontCollection *coll; IDWriteGdiInterop *gdi; IDWriteGdiInterop1 *gdi1;
    int n; UINT32 keys[16];
} gi_fix;
static const ttf_map gi_sym_map[] = { { 0xF020, 1 }, { 0xF041, 2 }, { 0xF042, 3 }, { 0xF061, 28 } };
static __inline void gi_fix_close(gi_fix *x)
{
    DW_RELEASE(x->gdi1); DW_RELEASE(x->gdi); DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}
/* returns 1 on success; the caller must always gi_fix_close() */
static __inline int gi_fix_open(gi_fix *x, const gi_cf *specs, int n)
{
    int i; ttf_cmap symcm;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("no isolated factory"); return 0; }
    x->fl = dwx_loader_new(); x->cl = dwx_coll_new(x->fl);
    if (FAILED(dwx_loader_register(x->fl, x->f)) || FAILED(dwx_coll_register(x->cl, x->f))) { t_skip("loader registration failed"); return 0; }
    memset(&symcm, 0, sizeof symcm); symcm.format = 4; symcm.plat = 3; symcm.enc = 0; symcm.n = 4; symcm.map = gi_sym_map;
    for (i = 0; i < n && i < 16; i++) {
        ttf_spec s; size_t sz; uint8_t *b; UINT32 k;
        ttf_style_spec(&s, specs[i].family, specs[i].weight ? specs[i].weight : 400, specs[i].width ? specs[i].width : 5, specs[i].italic);
        if (specs[i].cp0 || specs[i].cp1) { s.codepage_range[0] = specs[i].cp0; s.codepage_range[1] = specs[i].cp1; }
        if (specs[i].uni[0] || specs[i].uni[1] || specs[i].uni[2] || specs[i].uni[3]) memcpy(s.unicode_range, specs[i].uni, sizeof s.unicode_range);
        if (specs[i].symbol) { s.cmaps = &symcm; s.ncmaps = 1; }
        b = ttf_build(&s, &sz);
        if (!b) { t_skip("ttf_build failed"); return 0; }
        k = dwx_loader_add(x->fl, b, sz); free(b);
        dwx_coll_add_key(x->cl, k);
        x->keys[x->n++] = k;
    }
    if (FAILED(dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) { t_skip("custom collection creation failed"); return 0; }
    x->gdi = gi_get(x->f);
    if (!x->gdi) { t_skip("no interop"); return 0; }
    x->gdi1 = gi_get1(x->gdi);
    return 1;
}
#define GI_FIX_OR_SKIP(x, specs) do { if (!gi_fix_open(&(x), specs, (int)(sizeof(specs) / sizeof((specs)[0])))) { gi_fix_close(&(x)); return; } } while (0)

/* LOGFONT -> font (custom collection) -> logfont */
static __inline HRESULT gi_font_lf(gi_fix *x, const char *face, int weight, int italic, int cs, IDWriteFont **font)
{
    LOGFONTW lf; gi_lf(&lf, face, weight, italic, cs);
    return IDWriteGdiInterop1_CreateFontFromLOGFONT(x->gdi1, &lf, x->coll, font);
}
/* name in the font's WIN32_FAMILY_NAMES with the en-us locale (or the first), UTF-8 */
static __inline int gi_win32_family(IDWriteFont *font, char *out, int cap)
{
    IDWriteLocalizedStrings *s = NULL; BOOL ex = FALSE; int ok = 0;
    out[0] = 0;
    if (SUCCEEDED(IDWriteFont_GetInformationalStrings(font, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &s, &ex)) && s && ex) {
        ok = SUCCEEDED(dw_ls_en(s, out, cap));
    }
    if (s) IDWriteLocalizedStrings_Release(s);
    return ok;
}
static __inline void gi_name_u8(const LOGFONTW *lf, char *out, int cap) { dw_narrow(lf->lfFaceName, out, cap); }
static __inline BOOL gi_sys_has(IDWriteFactory *f, const char *face)
{
    WCHAR w[LF_FACESIZE]; dw_wide(face, w, LF_FACESIZE);
    return dw_have_font(f, w);
}

/* A DC with a GDI font selected; Arial resolved to the real face when requested */
typedef struct gi_hdc { HDC dc; HFONT font, old; } gi_hdc;
static __inline int gi_hdc_open(gi_hdc *h, const char *face, int weight, int italic, int height, int charset, int quality)
{
    LOGFONTW lf; memset(h, 0, sizeof *h);
    gi_lf(&lf, face, weight, italic, charset); lf.lfHeight = height; lf.lfQuality = (BYTE)quality;
    h->dc = CreateCompatibleDC(NULL);
    if (!h->dc) return 0;
    h->font = CreateFontIndirectW(&lf);
    if (!h->font) return 0;
    h->old = (HFONT)SelectObject(h->dc, h->font);
    return 1;
}
static __inline void gi_hdc_close(gi_hdc *h)
{
    if (h->dc && h->old) SelectObject(h->dc, h->old);
    if (h->font) DeleteObject(h->font);
    if (h->dc) DeleteDC(h->dc);
    memset(h, 0, sizeof *h);
}
static __inline int gi_hdc_textface(gi_hdc *h, char *out, int cap)
{
    WCHAR w[LF_FACESIZE + 4]; w[0] = 0;
    if (!GetTextFaceW(h->dc, LF_FACESIZE + 4, w)) return 0;
    dw_narrow(w, out, cap);
    return 1;
}
#endif
