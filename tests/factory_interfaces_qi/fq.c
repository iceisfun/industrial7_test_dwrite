/* factory_interfaces_qi: class table, creators and the shared check bodies. See fq.h. */
#include "fq.h"
#include <objidl.h>

/* ---------------------------------------------------------------------------------------------- ctx */
void fq_ctx_init(fq_ctx *c) { memset(c, 0, sizeof *c); }
void fq_hold(fq_ctx *c, void *u) { if (u && c->nhold < 10) c->hold[c->nhold++] = (IUnknown *)u; }
void fq_ctx_free(fq_ctx *c) {
    while (c->nhold > 0) { IUnknown *u = c->hold[--c->nhold]; if (u) IUnknown_Release(u); }
    if (c->font_open) { dwx_font_close(&c->font); c->font_open = 0; }
    DW_RELEASE(c->own_factory);
}
/* A private factory for creators that register loaders (so the shared factory stays untouched). */
static IDWriteFactory *own_factory(fq_ctx *c) {
    if (!c->own_factory) c->own_factory = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    return c->own_factory;
}
static void *own_fv(fq_ctx *c, int ver) {
    IDWriteFactory *f = own_factory(c); void *q = NULL;
    if (!f) return NULL;
    if (ver == 0) return f;
    if (FAILED(IDWriteFactory_QueryInterface(f, dw_factory_iid(ver), &q))) return NULL;
    fq_hold(c, q);
    return q;
}
static IDWriteFactory *SF(void) { return dw_shared(); }
static void *SFV(int v) { return dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v); }

/* ---------------------------------------------------------------------------------------------- interface sets */
#define F0 "IDWriteFactory"
#define FACTORIES F0, "IDWriteFactory1", "IDWriteFactory2", "IDWriteFactory3", "IDWriteFactory4", "IDWriteFactory5", "IDWriteFactory6", "IDWriteFactory7"
#define TFORMATS "IDWriteTextFormat", "IDWriteTextFormat1", "IDWriteTextFormat2", "IDWriteTextFormat3"
#define TLAYOUTS "IDWriteTextLayout", "IDWriteTextLayout1", "IDWriteTextLayout2", "IDWriteTextLayout3", "IDWriteTextLayout4"
#define COLLS "IDWriteFontCollection", "IDWriteFontCollection1", "IDWriteFontCollection2", "IDWriteFontCollection3"
#define FLISTS "IDWriteFontList", "IDWriteFontList1", "IDWriteFontList2"
#define FFAMS "IDWriteFontFamily", "IDWriteFontFamily1", "IDWriteFontFamily2"
#define FONTS "IDWriteFont", "IDWriteFont1", "IDWriteFont2", "IDWriteFont3"
#define FACES "IDWriteFontFace", "IDWriteFontFace1", "IDWriteFontFace2", "IDWriteFontFace3", "IDWriteFontFace4", "IDWriteFontFace5"
#define RPS "IDWriteRenderingParams", "IDWriteRenderingParams1", "IDWriteRenderingParams2", "IDWriteRenderingParams3"
#define FSETS "IDWriteFontSet", "IDWriteFontSet1", "IDWriteFontSet2", "IDWriteFontSet3"

static const char *const l_factory[]   = { FACTORIES, NULL };
static const char *const l_tformat[]   = { TFORMATS, NULL };
static const char *const l_tlayout[]   = { TLAYOUTS, TFORMATS, NULL };
static const char *const l_coll[]      = { COLLS, NULL };

static const char *const l_coll_q[]    = { "IDWriteFontCollection", "?IDWriteFontCollection1", "?IDWriteFontCollection2", "?IDWriteFontCollection3", NULL };
static const char *const l_family[]    = { FFAMS, FLISTS, NULL };
static const char *const l_flist[]     = { FLISTS, "?IDWriteFontFamily", "?IDWriteFontFamily1", "?IDWriteFontFamily2", NULL };
static const char *const l_font[]      = { FONTS, NULL };
/* DWriteFontFace::QueryInterface (decomp 0x180075620) also answers IDWriteFontFaceReference from the same object. */
static const char *const l_face[]      = { FACES, "IDWriteFontFaceReference", NULL };
static const char *const l_file[]      = { "IDWriteFontFile", NULL };
static const char *const l_faceref[]   = { "IDWriteFontFaceReference", "IDWriteFontFaceReference1", NULL };
static const char *const l_resource[]  = { "IDWriteFontResource", NULL };
static const char *const l_strings[]   = { "IDWriteLocalizedStrings", "?IDWriteStringList", NULL };
static const char *const l_rp[]        = { RPS, NULL };
static const char *const l_typo[]      = { "IDWriteTypography", NULL };
static const char *const l_numsub[]    = { "IDWriteNumberSubstitution", NULL };
static const char *const l_gdi[]       = { "IDWriteGdiInterop", "IDWriteGdiInterop1", NULL };
static const char *const l_sbuilder[]  = { "IDWriteFontSetBuilder", "IDWriteFontSetBuilder1", "IDWriteFontSetBuilder2", NULL };
static const char *const l_fset[]      = { FSETS, NULL };
static const char *const l_gra[]       = { "IDWriteGlyphRunAnalysis", NULL };
static const char *const l_rt[]        = { "IDWriteBitmapRenderTarget", "IDWriteBitmapRenderTarget1", NULL };
static const char *const l_analyzer[]  = { "IDWriteTextAnalyzer", "IDWriteTextAnalyzer1", "IDWriteTextAnalyzer2", NULL };
static const char *const l_fallback[]  = { "IDWriteFontFallback", "IDWriteFontFallback1", NULL };
static const char *const l_fbbuilder[] = { "IDWriteFontFallbackBuilder", NULL };
static const char *const l_inline[]    = { "IDWriteInlineObject", NULL };
static const char *const l_dlqueue[]   = { "IDWriteFontDownloadQueue", NULL };
static const char *const l_memloader[] = { "IDWriteInMemoryFontFileLoader", "IDWriteFontFileLoader", NULL };
static const char *const l_colorenum[] = { "IDWriteColorGlyphRunEnumerator", "IDWriteColorGlyphRunEnumerator1", NULL };

/* ---------------------------------------------------------------------------------------------- creators */
static IUnknown *mk_factory_shared(fq_ctx *c) { void *f = NULL; dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &f); return (IUnknown *)f; }
static IUnknown *mk_factory_isolated(fq_ctx *c) { void *f = NULL; dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &f); return (IUnknown *)f; }

static IDWriteTextFormat *new_format(IDWriteFactory *f) {
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(f, L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &t))) return NULL;
    return t;
}
static IUnknown *mk_text_format(fq_ctx *c) { return (IUnknown *)new_format(SF()); }
static IUnknown *mk_text_format_axes(fq_ctx *c) {
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)SFV(6); IDWriteTextFormat3 *t = NULL; DWRITE_FONT_AXIS_VALUE ax = { DWRITE_FONT_AXIS_TAG_WEIGHT, 400.0f };
    if (!f6) return NULL;
    if (FAILED(IDWriteFactory6_CreateTextFormat(f6, L"Segoe UI", NULL, &ax, 1, 12.0f, L"en-us", &t))) return NULL;
    return (IUnknown *)t;
}
static IUnknown *mk_text_layout(fq_ctx *c) {
    IDWriteTextFormat *t = new_format(SF()); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(SF(), L"Hello", 5, t, 200.0f, 100.0f, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return (IUnknown *)l;
}
static IUnknown *mk_text_layout_gdi(fq_ctx *c) {
    IDWriteTextFormat *t = new_format(SF()); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(SF(), L"Hello", 5, t, 200.0f, 100.0f, 1.0f, NULL, FALSE, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return (IUnknown *)l;
}
static IDWriteFontCollection *sys_coll(void) {
    IDWriteFontCollection *cl = NULL;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(SF(), &cl, FALSE))) return NULL;
    return cl;
}
static IUnknown *mk_font_collection_system(fq_ctx *c) { return (IUnknown *)sys_coll(); }
static IUnknown *mk_font_collection_custom(fq_ctx *c) {
    IDWriteFactory *f = own_factory(c); dwx_mem_loader *ld; dwx_coll_loader *cl; uint8_t *blob; size_t n; UINT32 key; IDWriteFontCollection *out = NULL;
    if (!f) return NULL;
    blob = ttf_build_default(&n);
    ld = dwx_loader_new(); key = dwx_loader_add(ld, blob, n); free(blob);
    if (FAILED(dwx_loader_register(ld, f))) { dwx_release(ld); return NULL; }
    cl = dwx_coll_new(ld); dwx_coll_add_key(cl, key);
    if (SUCCEEDED(dwx_coll_register(cl, f))) {
        if (FAILED(dwx_coll_create(cl, f, &out))) out = NULL;
        /* The collection keeps what it needs; the loaders can be unregistered at once (the helper test doubles stay valid). */
        dwx_coll_unregister(cl);
    }
    dwx_loader_unregister(ld);
    dwx_release(cl); dwx_release(ld);
    return (IUnknown *)out;
}
static IUnknown *mk_font_collection_fontset(fq_ctx *c) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)SFV(3); IDWriteFontSet *fs = NULL; IDWriteFontCollection1 *cl = NULL;
    if (!f3 || FAILED(IDWriteFactory3_GetSystemFontSet(f3, &fs))) return NULL;
    if (FAILED(IDWriteFactory3_CreateFontCollectionFromFontSet(f3, fs, &cl))) cl = NULL;
    IDWriteFontSet_Release(fs);
    return (IUnknown *)cl;
}
static IUnknown *mk_font_collection_eudc(fq_ctx *c) {
    IDWriteFontCollection *cl = NULL;
    if (FAILED(IDWriteFactory1_GetEudcFontCollection((IDWriteFactory1 *)SFV(1), &cl, FALSE))) return NULL;
    return (IUnknown *)cl;
}
static IDWriteFontFamily *pick_family(void) {
    static const WCHAR *const names[] = { L"Arial", L"Segoe UI", L"Tahoma", L"Times New Roman", NULL };
    IDWriteFontCollection *cl = sys_coll(); IDWriteFontFamily *fam = NULL; UINT32 idx = 0; BOOL ex = FALSE; int i;
    if (!cl) return NULL;
    for (i = 0; names[i] && !ex; i++) IDWriteFontCollection_FindFamilyName(cl, names[i], &idx, &ex);
    if (!ex) idx = 0;
    if (FAILED(IDWriteFontCollection_GetFontFamily(cl, idx, &fam))) fam = NULL;
    IDWriteFontCollection_Release(cl);
    return fam;
}
static IUnknown *mk_font_family(fq_ctx *c) { return (IUnknown *)pick_family(); }
static IUnknown *mk_font_list(fq_ctx *c) {
    IDWriteFontFamily *fam = pick_family(); IDWriteFontList *l = NULL;
    if (!fam) return NULL;
    if (FAILED(IDWriteFontFamily_GetMatchingFonts(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &l))) l = NULL;
    IDWriteFontFamily_Release(fam);
    return (IUnknown *)l;
}
static IDWriteFont *pick_font(void) {
    IDWriteFontFamily *fam = pick_family(); IDWriteFont *f = NULL;
    if (!fam) return NULL;
    if (FAILED(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &f))) f = NULL;
    IDWriteFontFamily_Release(fam);
    return f;
}
static IUnknown *mk_font(fq_ctx *c) { return (IUnknown *)pick_font(); }
static IUnknown *mk_font_face(fq_ctx *c) {
    IDWriteFont *f = pick_font(); IDWriteFontFace *face = NULL;
    if (!f) return NULL;
    if (FAILED(IDWriteFont_CreateFontFace(f, &face))) face = NULL;
    IDWriteFont_Release(f);
    return (IUnknown *)face;
}
static int open_custom(fq_ctx *c) {
    IDWriteFactory *f = own_factory(c);
    if (!f || c->font_open) return f != NULL;
    if (FAILED(dwx_font_open_default(&c->font, f))) { dwx_font_close(&c->font); return 0; }
    c->font_open = 1;
    return 1;
}
static IUnknown *mk_font_face_custom(fq_ctx *c) {
    if (!open_custom(c)) return NULL;
    IDWriteFontFace_AddRef(c->font.face);
    return (IUnknown *)c->font.face;
}
static IUnknown *mk_font_file(fq_ctx *c) {
    if (!open_custom(c)) return NULL;
    IDWriteFontFile_AddRef(c->font.file);
    return (IUnknown *)c->font.file;
}
static IUnknown *mk_font_face_reference(fq_ctx *c) {
    IDWriteFactory3 *f3; IDWriteFontFaceReference *r = NULL;
    if (!open_custom(c)) return NULL;
    f3 = (IDWriteFactory3 *)own_fv(c, 3);
    if (!f3 || FAILED(IDWriteFactory3_CreateFontFaceReference_(f3, c->font.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &r))) return NULL;
    return (IUnknown *)r;
}
static IUnknown *mk_font_resource(fq_ctx *c) {
    IDWriteFactory6 *f6; IDWriteFontResource *r = NULL;
    if (!open_custom(c)) return NULL;
    f6 = (IDWriteFactory6 *)own_fv(c, 6);
    if (!f6 || FAILED(IDWriteFactory6_CreateFontResource(f6, c->font.file, 0, &r))) return NULL;
    return (IUnknown *)r;
}
static IUnknown *mk_localized_strings(fq_ctx *c) {
    IDWriteFontFamily *fam = pick_family(); IDWriteLocalizedStrings *s = NULL;
    if (!fam) return NULL;
    if (FAILED(IDWriteFontFamily_GetFamilyNames(fam, &s))) s = NULL;
    IDWriteFontFamily_Release(fam);
    return (IUnknown *)s;
}
static IUnknown *mk_rendering_params(fq_ctx *c) {
    IDWriteRenderingParams *p = NULL;
    if (FAILED(IDWriteFactory_CreateRenderingParams(SF(), &p))) return NULL;
    return (IUnknown *)p;
}
static IUnknown *mk_rendering_params_custom(fq_ctx *c) {
    IDWriteRenderingParams *p = NULL;
    if (FAILED(IDWriteFactory_CreateCustomRenderingParams(SF(), 2.2f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p))) return NULL;
    return (IUnknown *)p;
}
static IUnknown *mk_rendering_params_monitor(fq_ctx *c) {
    IDWriteRenderingParams *p = NULL; HMONITOR m = MonitorFromPoint((POINT){ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (!m || FAILED(IDWriteFactory_CreateMonitorRenderingParams(SF(), m, &p))) return NULL;
    return (IUnknown *)p;
}
static IUnknown *mk_typography(fq_ctx *c) { IDWriteTypography *t = NULL; return SUCCEEDED(IDWriteFactory_CreateTypography(SF(), &t)) ? (IUnknown *)t : NULL; }
static IUnknown *mk_number_substitution(fq_ctx *c) {
    IDWriteNumberSubstitution *n = NULL;
    return SUCCEEDED(IDWriteFactory_CreateNumberSubstitution(SF(), DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-US", FALSE, &n)) ? (IUnknown *)n : NULL;
}
static IUnknown *mk_gdi_interop(fq_ctx *c) { IDWriteGdiInterop *g = NULL; return SUCCEEDED(IDWriteFactory_GetGdiInterop(SF(), &g)) ? (IUnknown *)g : NULL; }
static IUnknown *mk_font_set_builder(fq_ctx *c) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)SFV(3); IDWriteFontSetBuilder *b = NULL;
    return f3 && SUCCEEDED(IDWriteFactory3_CreateFontSetBuilder(f3, &b)) ? (IUnknown *)b : NULL;
}
static IUnknown *mk_font_set_builder1(fq_ctx *c) {
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)SFV(5); IDWriteFontSetBuilder1 *b = NULL;
    return f5 && SUCCEEDED(IDWriteFactory5_CreateFontSetBuilder(f5, &b)) ? (IUnknown *)b : NULL;
}
static IUnknown *mk_font_set_system(fq_ctx *c) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)SFV(3); IDWriteFontSet *s = NULL;
    return f3 && SUCCEEDED(IDWriteFactory3_GetSystemFontSet(f3, &s)) ? (IUnknown *)s : NULL;
}
static IUnknown *mk_font_set_built(fq_ctx *c) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)SFV(3); IDWriteFontSetBuilder *b = NULL; IDWriteFontSet *s = NULL;
    if (!f3 || FAILED(IDWriteFactory3_CreateFontSetBuilder(f3, &b))) return NULL;
    if (FAILED(IDWriteFontSetBuilder_CreateFontSet(b, &s))) s = NULL;   /* empty builder -> empty set */
    IDWriteFontSetBuilder_Release(b);
    return (IUnknown *)s;
}
static IUnknown *mk_glyph_run_analysis(fq_ctx *c) {
    DWRITE_GLYPH_RUN run; UINT16 gid = 0; FLOAT adv = 10.0f; UINT32 cp = 'A'; IDWriteGlyphRunAnalysis *a = NULL;
    if (!open_custom(c)) return NULL;
    IDWriteFontFace_GetGlyphIndices(c->font.face, &cp, 1, &gid);
    memset(&run, 0, sizeof run);
    run.fontFace = c->font.face; run.fontEmSize = 20.0f; run.glyphCount = 1; run.glyphIndices = &gid; run.glyphAdvances = &adv;
    if (FAILED(IDWriteFactory_CreateGlyphRunAnalysis(c->own_factory, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f, &a))) return NULL;
    return (IUnknown *)a;
}
static IUnknown *mk_bitmap_render_target(fq_ctx *c) {
    IDWriteGdiInterop *g = NULL; IDWriteBitmapRenderTarget *t = NULL;
    if (FAILED(IDWriteFactory_GetGdiInterop(SF(), &g))) return NULL;
    if (FAILED(IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 16, 16, &t))) t = NULL;
    IDWriteGdiInterop_Release(g);
    return (IUnknown *)t;
}
static IUnknown *mk_text_analyzer(fq_ctx *c) { IDWriteTextAnalyzer *a = NULL; return SUCCEEDED(IDWriteFactory_CreateTextAnalyzer(SF(), &a)) ? (IUnknown *)a : NULL; }
static IUnknown *mk_font_fallback_system(fq_ctx *c) {
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)SFV(2); IDWriteFontFallback *fb = NULL;
    return f2 && SUCCEEDED(IDWriteFactory2_GetSystemFontFallback(f2, &fb)) ? (IUnknown *)fb : NULL;
}
static IUnknown *mk_font_fallback_custom(fq_ctx *c) {
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)SFV(2); IDWriteFontFallbackBuilder *b = NULL; IDWriteFontFallback *fb = NULL;
    if (!f2 || FAILED(IDWriteFactory2_CreateFontFallbackBuilder(f2, &b))) return NULL;
    if (FAILED(IDWriteFontFallbackBuilder_CreateFontFallback(b, &fb))) fb = NULL;
    IDWriteFontFallbackBuilder_Release(b);
    return (IUnknown *)fb;
}
static IUnknown *mk_font_fallback_builder(fq_ctx *c) {
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)SFV(2); IDWriteFontFallbackBuilder *b = NULL;
    return f2 && SUCCEEDED(IDWriteFactory2_CreateFontFallbackBuilder(f2, &b)) ? (IUnknown *)b : NULL;
}
static IUnknown *mk_inline_object(fq_ctx *c) {
    IDWriteTextFormat *t = new_format(SF()); IDWriteInlineObject *o = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(SF(), t, &o))) o = NULL;
    IDWriteTextFormat_Release(t);
    return (IUnknown *)o;
}
static IUnknown *mk_font_download_queue(fq_ctx *c) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)SFV(3); IDWriteFontDownloadQueue *q = NULL;
    return f3 && SUCCEEDED(IDWriteFactory3_GetFontDownloadQueue(f3, &q)) ? (IUnknown *)q : NULL;
}
static IUnknown *mk_in_memory_loader(fq_ctx *c) {
    IDWriteFactory5 *f5 = (IDWriteFactory5 *)SFV(5); IDWriteInMemoryFontFileLoader *l = NULL;
    return f5 && SUCCEEDED(IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &l)) ? (IUnknown *)l : NULL;
}
static IUnknown *mk_color_glyph_run_enumerator(fq_ctx *c) {
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)SFV(2); IDWriteFontFace *face; DWRITE_GLYPH_RUN run; UINT16 gid = 0; FLOAT adv = 20.0f; UINT32 cp = 0x1F600;
    IDWriteColorGlyphRunEnumerator *e = NULL; HRESULT hr;
    if (!f2) return NULL;
    face = dw_sysface(SF(), L"Segoe UI Emoji", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) return NULL;
    IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gid);
    memset(&run, 0, sizeof run);
    run.fontFace = face; run.fontEmSize = 32.0f; run.glyphCount = 1; run.glyphIndices = &gid; run.glyphAdvances = &adv;
    hr = IDWriteFactory2_TranslateColorGlyphRun(f2, 0.0f, 0.0f, &run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
    IDWriteFontFace_Release(face);
    return hr == S_OK ? (IUnknown *)e : NULL;
}

#define CLS(n, ifaces, nif, fresh, distinct) const fq_class fq_cls_##n = { #n, mk_##n, ifaces, nif, fresh, distinct };
CLS(factory_shared,           l_factory,  E_NOINTERFACE, 0, 0)
CLS(factory_isolated,         l_factory,  E_NOINTERFACE, 1, 1)
CLS(text_format,              l_tformat,  E_NOINTERFACE, 1, 1)
CLS(text_format_axes,         l_tformat,  E_NOINTERFACE, 1, 1)
CLS(text_layout,              l_tlayout,  E_NOINTERFACE, 0, 1)
CLS(text_layout_gdi,          l_tlayout,  E_NOINTERFACE, 0, 1)
CLS(font_collection_system,   l_coll,     E_NOINTERFACE, 0, 0)
CLS(font_collection_custom,   l_coll,     E_NOINTERFACE, 0, 0)
CLS(font_collection_fontset,  l_coll,     E_NOINTERFACE, 0, 0)
CLS(font_collection_eudc,     l_coll_q,   E_NOINTERFACE, 0, 0)
CLS(font_family,              l_family,   E_NOINTERFACE, 0, 0)
CLS(font_list,                l_flist,    E_NOINTERFACE, 0, 0)
CLS(font,                     l_font,     E_NOINTERFACE, 0, 0)
CLS(font_face,                l_face,     E_NOTIMPL,     0, 0)
CLS(font_face_custom,         l_face,     E_NOTIMPL,     0, 0)
CLS(font_file,                l_file,     E_NOINTERFACE, 0, 0)
CLS(font_face_reference,      l_faceref,  E_NOINTERFACE, 0, 0)
CLS(font_resource,            l_resource, E_NOINTERFACE, 0, 0)
CLS(localized_strings,        l_strings,  E_NOINTERFACE, 0, 0)
CLS(rendering_params,         l_rp,       E_NOINTERFACE, 0, 0)
CLS(rendering_params_custom,  l_rp,       E_NOINTERFACE, 0, 0)
CLS(rendering_params_monitor, l_rp,       E_NOINTERFACE, 0, 0)
CLS(typography,               l_typo,     E_NOINTERFACE, 0, 1)
CLS(number_substitution,      l_numsub,   E_NOINTERFACE, 0, 0)
CLS(gdi_interop,              l_gdi,      E_NOINTERFACE, 0, 0)
CLS(font_set_builder,         l_sbuilder, E_NOINTERFACE, 0, 1)
CLS(font_set_builder1,        l_sbuilder, E_NOINTERFACE, 0, 1)
CLS(font_set_system,          l_fset,     E_NOINTERFACE, 0, 0)
CLS(font_set_built,           l_fset,     E_NOINTERFACE, 0, 0)
CLS(glyph_run_analysis,       l_gra,      E_NOINTERFACE, 0, 1)
CLS(bitmap_render_target,     l_rt,       E_NOINTERFACE, 0, 1)
CLS(text_analyzer,            l_analyzer, E_NOINTERFACE, 0, 0)
CLS(font_fallback_system,     l_fallback, E_NOINTERFACE, 0, 0)
CLS(font_fallback_custom,     l_fallback, E_NOINTERFACE, 0, 0)
CLS(font_fallback_builder,    l_fbbuilder,E_NOINTERFACE, 0, 1)
CLS(inline_object,            l_inline,   E_NOINTERFACE, 0, 0)
CLS(font_download_queue,      l_dlqueue,  E_NOINTERFACE, 0, 0)
CLS(in_memory_loader,         l_memloader,E_NOINTERFACE, 0, 1)
CLS(color_glyph_run_enumerator, l_colorenum, E_NOINTERFACE, 0, 1)

/* ---------------------------------------------------------------------------------------------- tables */
const IID *fq_iid_by_name(const char *name) {
    int n, i; const dw_qi_row *r = dw_iid_table(&n);
    for (i = 0; i < n; i++) if (!strcmp(r[i].name, name)) return r[i].iid;
    return NULL;
}
int fq_expect(const fq_class *cl, const char *name) {
    int i;
    for (i = 0; cl->ifaces[i]; i++) {
        const char *s = cl->ifaces[i];
        if (s[0] == '?') { if (!strcmp(s + 1, name)) return -1; }
        else if (!strcmp(s, name)) return 1;
    }
    return 0;
}
const char *fq_first_iface(const fq_class *cl) {
    int i;
    for (i = 0; cl->ifaces[i]; i++) if (cl->ifaces[i][0] != '?') return cl->ifaces[i];
    return NULL;
}
void *fq_qi_by_name(IUnknown *obj, const char *name) {
    void *p = NULL; const IID *iid = fq_iid_by_name(name);
    if (!iid || FAILED(IUnknown_QueryInterface(obj, iid, &p))) return NULL;
    return p;
}
static const struct { const char *n; int slots; } g_slots[] = {
#define S(x) { #x, (int)(sizeof(x##Vtbl) / sizeof(void *)) },
    S(IDWriteFactory) S(IDWriteFactory1) S(IDWriteFactory2) S(IDWriteFactory3) S(IDWriteFactory4) S(IDWriteFactory5) S(IDWriteFactory6) S(IDWriteFactory7)
    S(IDWriteTextFormat) S(IDWriteTextFormat1) S(IDWriteTextFormat2) S(IDWriteTextFormat3)
    S(IDWriteTextLayout) S(IDWriteTextLayout1) S(IDWriteTextLayout2) S(IDWriteTextLayout3) S(IDWriteTextLayout4)
    S(IDWriteFontCollection) S(IDWriteFontCollection1) S(IDWriteFontCollection2) S(IDWriteFontCollection3)
    S(IDWriteFontList) S(IDWriteFontList1) S(IDWriteFontList2) S(IDWriteFontFamily) S(IDWriteFontFamily1) S(IDWriteFontFamily2)
    S(IDWriteFont) S(IDWriteFont1) S(IDWriteFont2) S(IDWriteFont3)
    S(IDWriteFontFace) S(IDWriteFontFace1) S(IDWriteFontFace2) S(IDWriteFontFace3) S(IDWriteFontFace4) S(IDWriteFontFace5)
    S(IDWriteFontFaceReference) S(IDWriteFontFaceReference1) S(IDWriteFontFile) S(IDWriteFontResource)
    S(IDWriteLocalizedStrings) S(IDWriteStringList)
    S(IDWriteRenderingParams) S(IDWriteRenderingParams1) S(IDWriteRenderingParams2) S(IDWriteRenderingParams3)
    S(IDWriteTypography) S(IDWriteNumberSubstitution) S(IDWriteGdiInterop) S(IDWriteGdiInterop1)
    S(IDWriteFontSetBuilder) S(IDWriteFontSetBuilder1) S(IDWriteFontSetBuilder2)
    S(IDWriteFontSet) S(IDWriteFontSet1) S(IDWriteFontSet2) S(IDWriteFontSet3)
    S(IDWriteGlyphRunAnalysis) S(IDWriteBitmapRenderTarget) S(IDWriteBitmapRenderTarget1)
    S(IDWriteTextAnalyzer) S(IDWriteTextAnalyzer1) S(IDWriteTextAnalyzer2)
    S(IDWriteFontFallback) S(IDWriteFontFallback1) S(IDWriteFontFallbackBuilder) S(IDWriteInlineObject)
    S(IDWriteFontDownloadQueue) S(IDWriteInMemoryFontFileLoader) S(IDWriteFontFileLoader)
    S(IDWriteColorGlyphRunEnumerator) S(IDWriteColorGlyphRunEnumerator1)
#undef S
};
int fq_slots_of(const char *name) {
    size_t i;
    for (i = 0; i < sizeof g_slots / sizeof g_slots[0]; i++) if (!strcmp(g_slots[i].n, name)) return g_slots[i].slots;
    return 0;
}

/* ---------------------------------------------------------------------------------------------- image helpers */
HMODULE fq_dwrite(void) { return GetModuleHandleA("dwrite.dll"); }
static int region(const void *p, MEMORY_BASIC_INFORMATION *mbi) {
    HMODULE m = fq_dwrite();
    if (!m || !p) return 0;
    if (!VirtualQuery(p, mbi, sizeof *mbi)) return 0;
    return mbi->AllocationBase == (void *)m && mbi->State == MEM_COMMIT;
}
int fq_in_image(const void *p) { MEMORY_BASIC_INFORMATION m; return region(p, &m); }
int fq_is_exec(const void *p) {
    MEMORY_BASIC_INFORMATION m;
    return region(p, &m) && (m.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}
int fq_is_readonly_data(const void *p) {
    MEMORY_BASIC_INFORMATION m;
    return region(p, &m) && (m.Protect & (PAGE_READONLY | PAGE_WRITECOPY * 0)) && !(m.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY | PAGE_READWRITE | PAGE_WRITECOPY));
}
const char *fq_section_of(const void *p) {
    static char name[16]; HMODULE m = fq_dwrite(); IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)m; IMAGE_NT_HEADERS *nt; IMAGE_SECTION_HEADER *sec; int i;
    strcpy(name, "?");
    if (!m || dos->e_magic != IMAGE_DOS_SIGNATURE) return name;
    nt = (IMAGE_NT_HEADERS *)((char *)m + dos->e_lfanew);
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        ULONG_PTR a = (ULONG_PTR)m + sec[i].VirtualAddress;
        if ((ULONG_PTR)p >= a && (ULONG_PTR)p < a + sec[i].Misc.VirtualSize) { memcpy(name, sec[i].Name, 8); name[8] = 0; return name; }
    }
    return name;
}

/* ---------------------------------------------------------------------------------------------- checks */
void fq_check_row(IUnknown *obj, const char *name, const IID *iid, int expect, HRESULT noiface, const char *file, int line) {
    void *p = (void *)(ULONG_PTR)1; ULONG base = dw_refcount(obj), after; HRESULT hr;
    hr = IUnknown_QueryInterface(obj, iid, &p);
    if (expect == 1) {
        t_check(hr == S_OK, file, line, name, "QI failed: %s", dw_hr_name(hr));
        if (hr == S_OK) {
            t_check(p != NULL && p != (void *)1, file, line, name, "success but out pointer is %p", p);
            after = dw_refcount(obj);
            t_check(after == base + 1, file, line, name, "refcount after QI %lu want %lu", after, base + 1);
            if (p && p != (void *)1) {
                t_check(dw_same_identity((IUnknown *)p, obj), file, line, name, "different COM identity");
                IUnknown_Release((IUnknown *)p);
            }
        } else t_check(p == NULL, file, line, name, "failed QI left out pointer %p", p);
    } else if (expect == 0) {
        t_check(hr == noiface, file, line, name, "unsupported IID: got %s want %s", dw_hr_name(hr), dw_hr_name(noiface));
        t_check(p == NULL, file, line, name, "out pointer not zeroed on failure (%p)", p);
        if (SUCCEEDED(hr) && p && p != (void *)1) IUnknown_Release((IUnknown *)p);
    } else {
        t_note("QI %s -> %s", name, dw_hr_name(hr));
        if (SUCCEEDED(hr) && p && p != (void *)1) IUnknown_Release((IUnknown *)p);
    }
    t_check(dw_refcount(obj) == base, file, line, name, "refcount not restored: %lu want %lu", dw_refcount(obj), base);
}

/* Extra IIDs that no dwrite object implements. */
typedef struct { const IID *iid; const char *name; } extra_row;
static int extra_rows(extra_row *r) {
    static const IID zero = { 0 }, ones = { 0xffffffff, 0xffff, 0xffff, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };
    int n = 0;
    r[n].iid = &IID_IStream; r[n++].name = "IStream";
    r[n].iid = &IID_IDispatch; r[n++].name = "IDispatch";
    r[n].iid = &IID_IMarshal; r[n++].name = "IMarshal";
    r[n].iid = &IID_IPersist; r[n++].name = "IPersist";
    r[n].iid = &IID_IClassFactory; r[n++].name = "IClassFactory";
    r[n].iid = &zero; r[n++].name = "GUID_NULL";
    r[n].iid = &ones; r[n++].name = "GUID_all_ones";
    r[n].iid = &IID_ID2D1Factory; r[n++].name = "ID2D1Factory";
    return n;
}

static void matrix_on(IUnknown *obj, const fq_class *cl, const char *file, int line) {
    int n, i, ne; const dw_qi_row *rows = dw_iid_table(&n); extra_row ex[8];
    fq_check_row(obj, "IUnknown", &IID_IUnknown, 1, cl->noiface, file, line);
    for (i = 0; i < n; i++) fq_check_row(obj, rows[i].name, rows[i].iid, fq_expect(cl, rows[i].name), cl->noiface, file, line);
    ne = extra_rows(ex);
    for (i = 0; i < ne; i++) fq_check_row(obj, ex[i].name, ex[i].iid, 0, cl->noiface, file, line);
}

#define MAKE_OBJ(cl, ctx, obj) fq_ctx ctx; IUnknown *obj; fq_ctx_init(&ctx); obj = (cl)->create(&ctx); \
    if (!obj) { fq_ctx_free(&ctx); t_skip("%s: prerequisite missing (creator returned NULL)", (cl)->name); return; }

void fq_matrix(const fq_class *cl) {
    MAKE_OBJ(cl, c, obj)
    matrix_on(obj, cl, __FILE__, __LINE__);
    IUnknown_Release(obj);
    fq_ctx_free(&c);
}
void fq_matrix_from_each(const fq_class *cl) {
    int i;
    MAKE_OBJ(cl, c, obj)
    for (i = 0; cl->ifaces[i]; i++) {
        const char *nm = cl->ifaces[i]; void *p;
        if (nm[0] == '?') continue;
        p = fq_qi_by_name(obj, nm);
        t_check(p != NULL, __FILE__, __LINE__, nm, "QI to a listed interface failed");
        if (!p) continue;
        matrix_on((IUnknown *)p, cl, __FILE__, __LINE__);
        IUnknown_Release((IUnknown *)p);
    }
    IUnknown_Release(obj);
    fq_ctx_free(&c);
}
void fq_identity(const fq_class *cl) {
    int i, j, nd = 0, nt = 0; IUnknown *u0 = NULL; void *ptrs[16]; const char *nms[16]; int np = 0;
    MAKE_OBJ(cl, c, obj)
    CHECK_HR(IUnknown_QueryInterface(obj, &IID_IUnknown, (void **)&u0), S_OK);
    for (i = 0; cl->ifaces[i] && np < 16; i++) {
        const char *nm = cl->ifaces[i]; void *p, *p2, *self = NULL; IUnknown *u = NULL;
        if (nm[0] == '?') continue;
        p = fq_qi_by_name(obj, nm);
        if (!p) { CHECKF(0, "QI %s failed", nm); continue; }
        /* repeated QI of the same interface yields the same pointer */
        p2 = fq_qi_by_name(obj, nm);
        CHECKF(p == p2, "%s: QI twice gave %p and %p", nm, p, p2);
        if (p2) IUnknown_Release((IUnknown *)p2);
        /* QI(IUnknown) from every interface pointer is the object's identity */
        if (SUCCEEDED(IUnknown_QueryInterface((IUnknown *)p, &IID_IUnknown, (void **)&u))) { CHECKF(u == u0, "%s: IUnknown identity %p vs %p", nm, (void *)u, (void *)u0); IUnknown_Release(u); }
        else CHECKF(0, "%s: QI(IUnknown) failed", nm);
        /* reflexive: QI(I) on an I pointer returns that same pointer */
        if (SUCCEEDED(IUnknown_QueryInterface((IUnknown *)p, fq_iid_by_name(nm), &self))) { CHECKF(self == p, "%s: reflexive QI %p vs %p", nm, self, p); IUnknown_Release((IUnknown *)self); }
        else CHECKF(0, "%s: reflexive QI failed", nm);
        ptrs[np] = p; nms[np] = nm; np++;
    }
    /* transitive/symmetric: QI(T) from any interface pointer S equals QI(T) from the original pointer */
    for (i = 0; i < np; i++) for (j = 0; j < np; j++) {
        void *t = NULL; IUnknown_QueryInterface((IUnknown *)ptrs[i], fq_iid_by_name(nms[j]), &t);
        CHECKF(t == ptrs[j], "QI(%s) via %s gave %p, direct gave %p", nms[j], nms[i], t, ptrs[j]);
        if (t) IUnknown_Release((IUnknown *)t);
    }
    for (i = 0; i < np; i++) { int dup = 0; for (j = 0; j < i; j++) if (ptrs[j] == ptrs[i]) dup = 1; if (!dup) nd++; }
    t_note("%s: %d listed interfaces map to %d distinct interface pointers; IUnknown ptr %p, first iface ptr %p (delta %lld)", cl->name, np, nd,
           (void *)u0, np ? ptrs[0] : NULL, np ? (long long)((char *)u0 - (char *)ptrs[0]) : 0LL);
    (void)nt;
    for (i = 0; i < np; i++) IUnknown_Release((IUnknown *)ptrs[i]);
    if (u0) IUnknown_Release(u0);
    IUnknown_Release(obj);
    fq_ctx_free(&c);
}
void fq_refcount(const fq_class *cl) {
    int i; ULONG base, r;
    MAKE_OBJ(cl, c, obj)
    base = dw_refcount(obj);
    t_note("%s: initial refcount %lu", cl->name, base);
    CHECKF(base >= 1, "refcount of a live object must be >= 1, got %lu", base);
    if (cl->fresh) EXPECT_EQ(base, 1);
    /* AddRef/Release return the new count */
    r = IUnknown_AddRef(obj);   EXPECT_EQ(r, base + 1);
    r = IUnknown_AddRef(obj);   EXPECT_EQ(r, base + 2);
    r = IUnknown_Release(obj);  EXPECT_EQ(r, base + 1);
    r = IUnknown_Release(obj);  EXPECT_EQ(r, base);
    for (i = 0; cl->ifaces[i]; i++) {
        const char *nm = cl->ifaces[i]; void *p;
        if (nm[0] == '?') continue;
        p = fq_qi_by_name(obj, nm);
        if (!p) { CHECKF(0, "QI %s failed", nm); continue; }
        /* the count is one counter shared by every interface of the object */
        EXPECT_EQ(dw_refcount((IUnknown *)p), base + 1);
        r = IUnknown_AddRef((IUnknown *)p);   EXPECT_EQ(r, base + 2);
        EXPECT_EQ(dw_refcount(obj), base + 2);
        r = IUnknown_Release(obj);            EXPECT_EQ(r, base + 1);     /* released through a different interface than acquired */
        r = IUnknown_Release((IUnknown *)p);  EXPECT_EQ(r, base);
        EXPECT_EQ(dw_refcount(obj), base);
    }
    /* many interfaces held at once */
    {
        void *held[16]; int n = 0;
        for (i = 0; cl->ifaces[i] && n < 16; i++) if (cl->ifaces[i][0] != '?') { held[n] = fq_qi_by_name(obj, cl->ifaces[i]); if (held[n]) n++; }
        EXPECT_EQ(dw_refcount(obj), base + (ULONG)n);
        while (n > 0) { n--; r = IUnknown_Release((IUnknown *)held[n]); EXPECT_EQ(r, base + (ULONG)n); }
        EXPECT_EQ(dw_refcount(obj), base);
    }
    r = IUnknown_Release(obj);
    EXPECT_EQ(r, base - 1);       /* the creator's reference; 0 when it was the only one */
    fq_ctx_free(&c);
}
void fq_vtables(const fq_class *cl) {
    int i, nptr = 0, same012 = 0; void **first_vt = NULL; ULONG_PTR seen[16]; int nseen = 0;
    MAKE_OBJ(cl, c, obj)
    CHECK(fq_dwrite() != NULL);
    for (i = -1; i == -1 || cl->ifaces[i]; i++) {
        const char *nm = i < 0 ? "IUnknown" : cl->ifaces[i]; void *p; void **vt; int k, ns;
        if (i >= 0 && nm[0] == '?') continue;
        if (i < 0) { p = NULL; IUnknown_QueryInterface(obj, &IID_IUnknown, &p); } else p = fq_qi_by_name(obj, nm);
        if (!p) { CHECKF(0, "QI %s failed", nm); continue; }
        vt = *(void ***)p;
        CHECKF(fq_in_image(vt), "%s: vtable %p not inside dwrite.dll", nm, (void *)vt);
        CHECKF(fq_is_readonly_data(vt), "%s: vtable %p (section %s) is not read-only data", nm, (void *)vt, fq_section_of(vt));
        if (!first_vt) first_vt = vt;
        ns = i < 0 ? 3 : fq_slots_of(nm);
        CHECKF(ns >= 3, "%s: slot count unknown", nm);
        for (k = 0; k < ns; k++) {
            CHECKF(vt[k] != NULL, "%s: slot %d is NULL", nm, k);
            CHECKF(fq_is_exec(vt[k]), "%s: slot %d (%p) is not code inside dwrite.dll", nm, k, vt[k]);
        }
        if (ns >= 3) {
            nptr++;
            if (first_vt && vt[0] == first_vt[0] && vt[1] == first_vt[1] && vt[2] == first_vt[2]) same012++;
        }
        if (i >= 0) {
            int dup = 0, j;
            for (j = 0; j < nseen; j++) if (seen[j] == (ULONG_PTR)vt) dup = 1;
            if (!dup && nseen < 16) seen[nseen++] = (ULONG_PTR)vt;
            if (i == 0) t_note("%s: first interface %s vtable in section %s, %d slots", cl->name, nm, fq_section_of(vt), ns);
        }
        IUnknown_Release((IUnknown *)p);
    }
    t_note("%s: %d vtables checked, %d share IUnknown slots 0..2 with the IUnknown-identity vtable, %d distinct vtables among listed interfaces", cl->name, nptr, same012, nseen);
    IUnknown_Release(obj);
    fq_ctx_free(&c);
}
void fq_fresh(const fq_class *cl) {
    fq_ctx c1, c2; IUnknown *a, *b; IUnknown *ia = NULL, *ib = NULL;
    fq_ctx_init(&c1); fq_ctx_init(&c2);
    a = cl->create(&c1);
    if (!a) { fq_ctx_free(&c1); t_skip("%s: prerequisite missing", cl->name); return; }
    b = cl->create(&c2);
    if (!b) { IUnknown_Release(a); fq_ctx_free(&c1); fq_ctx_free(&c2); t_skip("%s: second creation failed", cl->name); return; }
    IUnknown_QueryInterface(a, &IID_IUnknown, (void **)&ia);
    IUnknown_QueryInterface(b, &IID_IUnknown, (void **)&ib);
    if (ia) IUnknown_Release(ia);   /* identity pointers only; drop the references so counts are the creators' */
    if (ib) IUnknown_Release(ib);
    if (cl->distinct) CHECKF(ia != ib, "two creations returned the same object %p", (void *)ia);
    else t_note("%s: two creations %s", cl->name, ia == ib ? "returned the same object (cached)" : "returned distinct objects");
    if (cl->fresh && ia != ib) { EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); }
    if (ia == ib) t_note("%s: shared object refcount after two creations: %lu", cl->name, dw_refcount(a));
    IUnknown_Release(b); IUnknown_Release(a);
    fq_ctx_free(&c1); fq_ctx_free(&c2);
}
