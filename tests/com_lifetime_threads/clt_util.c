/* com_lifetime_threads: shared fixtures, generic COM checks, makers and the thread runner. */
#include "clt.h"

/* ---- context ------------------------------------------------------------------------------------------- */
int clt_ctx_init(clt_ctx *c) {
    memset(c, 0, sizeof *c);
    c->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    return c->f != NULL;
}
IDWriteFontFace *clt_ctx_face(clt_ctx *c) {
    if (!c->font_open) { c->font_open = 1; dwx_font_open_default(&c->font, c->f); }
    return c->font.face;
}
IDWriteFontFile *clt_ctx_file(clt_ctx *c) { clt_ctx_face(c); return c->font.file; }
IDWriteFontCollection *clt_ctx_coll(clt_ctx *c) {
    if (c->coll) return c->coll;
    {
        size_t n; uint8_t *b = ttf_build_default(&n); UINT32 key;
        c->ld = dwx_loader_new();
        key = dwx_loader_add(c->ld, b, n); free(b);
        if (FAILED(dwx_loader_register(c->ld, c->f))) return NULL;
        c->cl = dwx_coll_new(c->ld);
        dwx_coll_add_key(c->cl, key);
        if (FAILED(dwx_coll_register(c->cl, c->f))) return NULL;
        if (FAILED(dwx_coll_create(c->cl, c->f, &c->coll))) c->coll = NULL;
    }
    return c->coll;
}
/* Drop our own references WITHOUT calling into the factory (used once the factory may already be gone). */
void clt_ctx_abandon(clt_ctx *c) {
    DW_RELEASE(c->coll);
    if (c->cl) { c->cl->registered = 0; dwx_release(c->cl); c->cl = NULL; }
    if (c->ld) { c->ld->registered = 0; dwx_release(c->ld); c->ld = NULL; }
    if (c->font_open) { DW_RELEASE(c->font.face); DW_RELEASE(c->font.file); if (c->font.ld) { c->font.ld->registered = 0; dwx_release(c->font.ld); c->font.ld = NULL; } c->font_open = 0; }
    if (c->dc_open) { dwx_dc_destroy(&c->dc); c->dc_open = 0; }
    c->f = NULL;
}
void clt_ctx_done(clt_ctx *c) {
    DW_RELEASE(c->coll);
    if (c->cl) { dwx_coll_unregister(c->cl); dwx_release(c->cl); c->cl = NULL; }
    if (c->ld) { dwx_loader_unregister(c->ld); dwx_release(c->ld); c->ld = NULL; }
    if (c->font_open) { dwx_font_close(&c->font); c->font_open = 0; }
    if (c->dc_open) { dwx_dc_destroy(&c->dc); c->dc_open = 0; }
    DW_RELEASE(c->f);
}

/* Windows 10 completes deferred releases when dwrite is next entered on the thread: do some harmless work. */
static void clt_nudge(void) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteTextFormat *fm = NULL;
    if (!f) return;
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fm);
    DW_RELEASE(fm); IDWriteFactory_Release(f);
}
void clt_quiesce(void) {
    int last = dwx_live(), stable = 0, i;
    for (i = 0; i < 100 && stable < 5; i++) { Sleep(10); if (i % 10 == 5) clt_nudge(); { int n = dwx_live(); if (n == last) stable++; else { stable = 0; last = n; } } }
}
LONG clt_settle_refs(void *obj, LONG want) {
    LONG r = dwx_refs(obj); int i;
    for (i = 0; i < 100 && r != want; i++) { Sleep(10); if (i % 10 == 5) clt_nudge(); r = dwx_refs(obj); }
    return r;
}
int clt_in_dwrite(const void *addr) {
    MEMORY_BASIC_INFORMATION mbi; HMODULE m = t_module("dwrite.dll");
    if (!m || !addr) return 0;
    if (!VirtualQuery(addr, &mbi, sizeof mbi)) return 0;
    return mbi.AllocationBase == (void *)m;
}
/* ---- generic COM checks --------------------------------------------------------------------------------- */
int clt_qi_balance(IUnknown *o, const char *file, int line) {
    int n = 0, i, ok = 0; const dw_qi_row *rows = dw_iid_table(&n);
    ULONG base = dw_refcount(o);
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)1;
        HRESULT hr = IUnknown_QueryInterface(o, rows[i].iid, &p);
        if (SUCCEEDED(hr)) {
            ULONG now = dw_refcount(o);
            ok++;
            t_check(hr == S_OK, file, line, rows[i].name, "QI success code %s", dw_hr_name(hr));
            t_check(p != NULL && p != (void *)1, file, line, rows[i].name, "success with bad out pointer");
            t_check(now == base + 1, file, line, rows[i].name, "QI success must add exactly one reference: %lu -> %lu", base, now);
            if (p && p != (void *)1) {
                t_check(clt_in_dwrite(*(void **)p), file, line, rows[i].name, "interface vtable %p is not inside dwrite.dll (no proxy expected)", *(void **)p);
                t_check(dw_same_identity((IUnknown *)p, o), file, line, rows[i].name, "different COM identity");
                IUnknown_Release((IUnknown *)p);
            }
        } else {
            /* Windows 10 answers E_NOTIMPL for some well-known IIDs the object does not implement */
            t_check(hr == E_NOINTERFACE || hr == E_NOTIMPL, file, line, rows[i].name, "QI failure code %s want E_NOINTERFACE or E_NOTIMPL", dw_hr_name(hr));
            t_check(p == NULL, file, line, rows[i].name, "out pointer not zeroed on failure (%p)", p);
        }
    }
    t_check(dw_refcount(o) == base, file, line, "QI matrix balance", "before %lu after %lu", base, dw_refcount(o));
    return ok;
}
void clt_refs_arith(IUnknown *o, const char *file, int line) {
    ULONG b = dw_refcount(o), r;
    r = IUnknown_AddRef(o);  t_check(r == b + 1, file, line, "AddRef #1", "got %lu want %lu", r, b + 1);
    r = IUnknown_AddRef(o);  t_check(r == b + 2, file, line, "AddRef #2", "got %lu want %lu", r, b + 2);
    r = IUnknown_AddRef(o);  t_check(r == b + 3, file, line, "AddRef #3", "got %lu want %lu", r, b + 3);
    r = IUnknown_Release(o); t_check(r == b + 2, file, line, "Release #3", "got %lu want %lu", r, b + 2);
    r = IUnknown_Release(o); t_check(r == b + 1, file, line, "Release #2", "got %lu want %lu", r, b + 1);
    r = IUnknown_Release(o); t_check(r == b, file, line, "Release #1", "got %lu want %lu", r, b);
    {
        void *p = NULL;
        HRESULT hr = IUnknown_QueryInterface(o, &IID_IUnknown, &p);
        t_check(hr == S_OK && p != NULL, file, line, "QI(IUnknown)", "hr %s", dw_hr_name(hr));
        r = dw_refcount(o);
        t_check(r == b + 1, file, line, "QI(IUnknown) adds a reference to the same count", "got %lu want %lu", r, b + 1);
        if (p) IUnknown_Release((IUnknown *)p);
        r = dw_refcount(o);
        t_check(r == b, file, line, "Release of QI result", "got %lu want %lu", r, b);
    }
}

/* ---- makers --------------------------------------------------------------------------------------------- */
static IDWriteTextFormat *mkfmt(clt_ctx *c, float sz) {
    IDWriteTextFormat *fm = NULL;
    IDWriteFactory_CreateTextFormat(c->f, L"Arial", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz, L"en-us", &fm);
    return fm;
}
static IUnknown *m_fmt(clt_ctx *c) { return (IUnknown *)mkfmt(c, 14.0f); }
static void t_fmt(IUnknown *o) { float s = IDWriteTextFormat_GetFontSize((IDWriteTextFormat *)o); CHECK(s == 14.0f); }
static IUnknown *m_layout(clt_ctx *c) {
    IDWriteTextFormat *fm = mkfmt(c, 14.0f); IDWriteTextLayout *l = NULL;
    if (fm) IDWriteFactory_CreateTextLayout(c->f, L"Hello", 5, fm, 200.0f, 100.0f, &l);
    DW_RELEASE(fm);
    return (IUnknown *)l;
}
static void t_layout(IUnknown *o) { CHECK(IDWriteTextLayout_GetMaxWidth((IDWriteTextLayout *)o) == 200.0f); }
static IUnknown *m_typo(clt_ctx *c) { IDWriteTypography *t = NULL; IDWriteFactory_CreateTypography(c->f, &t); return (IUnknown *)t; }
static void t_typo(IUnknown *o) { CHECK(IDWriteTypography_GetFontFeatureCount((IDWriteTypography *)o) == 0); }
static IUnknown *m_numsubst(clt_ctx *c) { IDWriteNumberSubstitution *n = NULL; IDWriteFactory_CreateNumberSubstitution(c->f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-US", FALSE, &n); return (IUnknown *)n; }
static IUnknown *m_rp(clt_ctx *c) { IDWriteRenderingParams *p = NULL; IDWriteFactory_CreateRenderingParams(c->f, &p); return (IUnknown *)p; }
static void t_rp(IUnknown *o) { float g = IDWriteRenderingParams_GetGamma((IDWriteRenderingParams *)o); CHECK(g > 0.0f); }
static IUnknown *m_crp(clt_ctx *c) {
    IDWriteRenderingParams *p = NULL;
    IDWriteFactory_CreateCustomRenderingParams(c->f, 1.8f, 0.5f, 0.25f, DWRITE_PIXEL_GEOMETRY_FLAT, DWRITE_RENDERING_MODE_NATURAL, &p);
    return (IUnknown *)p;
}
static void t_crp(IUnknown *o) { CHECK(IDWriteRenderingParams_GetGamma((IDWriteRenderingParams *)o) == 1.8f); }
static IUnknown *m_analyzer(clt_ctx *c) { IDWriteTextAnalyzer *a = NULL; IDWriteFactory_CreateTextAnalyzer(c->f, &a); return (IUnknown *)a; }
static IUnknown *m_gdi(clt_ctx *c) { IDWriteGdiInterop *g = NULL; IDWriteFactory_GetGdiInterop(c->f, &g); return (IUnknown *)g; }
static IUnknown *m_ellipsis(clt_ctx *c) {
    IDWriteTextFormat *fm = mkfmt(c, 14.0f); IDWriteInlineObject *io = NULL;
    if (fm) IDWriteFactory_CreateEllipsisTrimmingSign(c->f, fm, &io);
    DW_RELEASE(fm);
    return (IUnknown *)io;
}
static void t_ellipsis(IUnknown *o) { DWRITE_INLINE_OBJECT_METRICS m; HRESULT hr = IDWriteInlineObject_GetMetrics((IDWriteInlineObject *)o, &m); if (hr != S_OK) t_note("ellipsis sign GetMetrics -> %s", dw_hr_name(hr)); }
static IUnknown *m_file(clt_ctx *c) {
    IDWriteFontFile *ff = NULL;
    clt_ctx_face(c);
    if (c->font.ld) dwx_loader_file(c->font.ld, c->f, c->font.key, &ff);
    return (IUnknown *)ff;
}
static void t_file(IUnknown *o) { const void *k = NULL; UINT32 n = 0; CHECK_HR_OK(IDWriteFontFile_GetReferenceKey((IDWriteFontFile *)o, &k, &n)); CHECK(n == 4); }
static IUnknown *m_face(clt_ctx *c) {
    IDWriteFontFile *ff = (IDWriteFontFile *)m_file(c); IDWriteFontFace *fc = NULL;
    if (ff) IDWriteFactory_CreateFontFace(c->f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &fc);
    DW_RELEASE(ff);
    return (IUnknown *)fc;
}
static void t_face(IUnknown *o) { UINT32 cp = 'A'; UINT16 g = 0; CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices((IDWriteFontFace *)o, &cp, 1, &g)); CHECK(g == 2); }
static IUnknown *m_coll(clt_ctx *c) {
    IDWriteFontCollection *cc = NULL;
    clt_ctx_coll(c);
    if (c->cl) dwx_coll_create(c->cl, c->f, &cc);
    return (IUnknown *)cc;
}
static void t_coll(IUnknown *o) { CHECK(IDWriteFontCollection_GetFontFamilyCount((IDWriteFontCollection *)o) == 1); }
static IUnknown *m_syscoll(clt_ctx *c) { IDWriteFontCollection *cc = NULL; IDWriteFactory_GetSystemFontCollection(c->f, &cc, FALSE); return (IUnknown *)cc; }
static void t_syscoll(IUnknown *o) { CHECK(IDWriteFontCollection_GetFontFamilyCount((IDWriteFontCollection *)o) >= 1); }
static IUnknown *m_family(clt_ctx *c) {
    IDWriteFontCollection *cc = NULL; IDWriteFontFamily *fam = NULL;
    cc = (IDWriteFontCollection *)m_coll(c);
    if (cc) IDWriteFontCollection_GetFontFamily(cc, 0, &fam);
    DW_RELEASE(cc);
    return (IUnknown *)fam;
}
static void t_family(IUnknown *o) { CHECK(IDWriteFontFamily_GetFontCount((IDWriteFontFamily *)o) >= 1); }
static IUnknown *m_font(clt_ctx *c) {
    IDWriteFontFamily *fam = (IDWriteFontFamily *)m_family(c); IDWriteFont *fn = NULL;
    if (fam) IDWriteFontFamily_GetFont(fam, 0, &fn);
    DW_RELEASE(fam);
    return (IUnknown *)fn;
}
static void t_font(IUnknown *o) { CHECK(IDWriteFont_GetWeight((IDWriteFont *)o) == DWRITE_FONT_WEIGHT_REGULAR); }
static IUnknown *m_famnames(clt_ctx *c) {
    IDWriteFontFamily *fam = (IDWriteFontFamily *)m_family(c); IDWriteLocalizedStrings *s = NULL;
    if (fam) IDWriteFontFamily_GetFamilyNames(fam, &s);
    DW_RELEASE(fam);
    return (IUnknown *)s;
}
static void t_loc(IUnknown *o) { CHECK(IDWriteLocalizedStrings_GetCount((IDWriteLocalizedStrings *)o) >= 0); }
static IUnknown *m_facenames(clt_ctx *c) {
    IDWriteFont *fn = (IDWriteFont *)m_font(c); IDWriteLocalizedStrings *s = NULL;
    if (fn) IDWriteFont_GetFaceNames(fn, &s);
    DW_RELEASE(fn);
    return (IUnknown *)s;
}
static IUnknown *m_fontface_from_font(clt_ctx *c) {
    IDWriteFont *fn = (IDWriteFont *)m_font(c); IDWriteFontFace *fc = NULL;
    if (fn) IDWriteFont_CreateFontFace(fn, &fc);
    DW_RELEASE(fn);
    return (IUnknown *)fc;
}
static IUnknown *m_brt(clt_ctx *c) {
    IDWriteGdiInterop *g = NULL; IDWriteBitmapRenderTarget *rt = NULL;
    if (!c->dc_open) c->dc_open = dwx_dc_create(&c->dc, 32, 32, 0xFFFFFF);
    if (!c->dc_open) return NULL;
    IDWriteFactory_GetGdiInterop(c->f, &g);
    if (g) IDWriteGdiInterop_CreateBitmapRenderTarget(g, c->dc.dc, 64, 32, &rt);
    DW_RELEASE(g);
    return (IUnknown *)rt;
}
static void t_brt(IUnknown *o) { SIZE sz = { 0, 0 }; CHECK_HR_OK(IDWriteBitmapRenderTarget_GetSize((IDWriteBitmapRenderTarget *)o, &sz)); CHECK(sz.cx == 64 && sz.cy == 32); }
static IUnknown *m_gra(clt_ctx *c) {
    IDWriteFontFace *fc = clt_ctx_face(c); IDWriteGlyphRunAnalysis *a = NULL; DWRITE_GLYPH_RUN run; UINT16 g = 2; float adv = 12.0f;
    if (!fc) return NULL;
    memset(&run, 0, sizeof run);
    run.fontFace = fc; run.fontEmSize = 16.0f; run.glyphCount = 1; run.glyphIndices = &g; run.glyphAdvances = &adv;
    IDWriteFactory_CreateGlyphRunAnalysis(c->f, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL, DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f, &a);
    return (IUnknown *)a;
}
static void t_gra(IUnknown *o) { RECT rc; CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds((IDWriteGlyphRunAnalysis *)o, DWRITE_TEXTURE_ALIASED_1x1, &rc)); }
static IUnknown *m_isofactory(clt_ctx *c) { return (IUnknown *)dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); }
static void t_factory(IUnknown *o) { IDWriteTypography *t = NULL; CHECK_HR_OK(IDWriteFactory_CreateTypography((IDWriteFactory *)o, &t)); DW_RELEASE(t); }
static IUnknown *m_sharedfactory(clt_ctx *c) { return (IUnknown *)dw_new_factory(DWRITE_FACTORY_TYPE_SHARED); }
static void *qif(clt_ctx *c, const IID *iid) { void *p = NULL; if (FAILED(IDWriteFactory_QueryInterface(c->f, iid, &p))) return NULL; return p; }
static IUnknown *m_fsb(clt_ctx *c) {
    IDWriteFactory3 *f3 = qif(c, &IID_IDWriteFactory3); IDWriteFontSetBuilder *b = NULL;
    if (!f3) return NULL;
    IDWriteFactory3_CreateFontSetBuilder(f3, &b);
    IDWriteFactory3_Release(f3);
    return (IUnknown *)b;
}
static IUnknown *m_sysset(clt_ctx *c) {
    IDWriteFactory3 *f3 = qif(c, &IID_IDWriteFactory3); IDWriteFontSet *s = NULL;
    if (!f3) return NULL;
    IDWriteFactory3_GetSystemFontSet(f3, &s);
    IDWriteFactory3_Release(f3);
    return (IUnknown *)s;
}
static void t_set(IUnknown *o) { CHECK(IDWriteFontSet_GetFontCount((IDWriteFontSet *)o) >= 1); }
static IUnknown *m_faceref(clt_ctx *c) {
    IDWriteFactory3 *f3 = qif(c, &IID_IDWriteFactory3); IDWriteFontFaceReference *r = NULL; IDWriteFontFile *ff = (IDWriteFontFile *)m_file(c);
    if (f3 && ff) IDWriteFactory3_CreateFontFaceReference_(f3, ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &r);
    DW_RELEASE(ff);
    if (f3) IDWriteFactory3_Release(f3);
    return (IUnknown *)r;
}
static void t_faceref(IUnknown *o) { CHECK(IDWriteFontFaceReference_GetFontFaceIndex((IDWriteFontFaceReference *)o) == 0); }
static IUnknown *m_inmem(clt_ctx *c) {
    IDWriteFactory5 *f5 = qif(c, &IID_IDWriteFactory5); IDWriteInMemoryFontFileLoader *l = NULL;
    if (!f5) return NULL;
    IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &l);
    IDWriteFactory5_Release(f5);
    return (IUnknown *)l;
}
static IUnknown *m_dlq(clt_ctx *c) {
    IDWriteFactory3 *f3 = qif(c, &IID_IDWriteFactory3); IDWriteFontDownloadQueue *q = NULL;
    if (!f3) return NULL;
    IDWriteFactory3_GetFontDownloadQueue(f3, &q);
    IDWriteFactory3_Release(f3);
    return (IUnknown *)q;
}
static void t_dlq(IUnknown *o) { BOOL e = IDWriteFontDownloadQueue_IsEmpty((IDWriteFontDownloadQueue *)o); (void)e; }
static IUnknown *m_fmt_from_layout_qi(clt_ctx *c) {   /* IDWriteTextFormat view of a layout: same object, same count */
    IUnknown *l = m_layout(c); IUnknown *fm = NULL;
    if (!l) return NULL;
    IUnknown_QueryInterface(l, &IID_IDWriteTextFormat, (void **)&fm);
    IUnknown_Release(l);
    return fm;
}
static IUnknown *m_collfromset(clt_ctx *c) {
    IDWriteFactory3 *f3 = qif(c, &IID_IDWriteFactory3); IDWriteFontSet *s = NULL; IDWriteFontCollection1 *cc = NULL;
    if (!f3) return NULL;
    IDWriteFactory3_GetSystemFontSet(f3, &s);
    if (s) IDWriteFactory3_CreateFontCollectionFromFontSet(f3, s, &cc);
    DW_RELEASE(s);
    IDWriteFactory3_Release(f3);
    return (IUnknown *)cc;
}
static IUnknown *m_sysfontcoll1(clt_ctx *c) {
    IDWriteFactory3 *f3 = qif(c, &IID_IDWriteFactory3); IDWriteFontCollection1 *cc = NULL;
    if (!f3) return NULL;
    IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &cc, FALSE);
    IDWriteFactory3_Release(f3);
    return (IUnknown *)cc;
}

static const clt_maker g_makers[] = {
    { "textformat", m_fmt, t_fmt, 1 },
    { "textlayout", m_layout, t_layout, 1 },
    { "textformat_view_of_layout", m_fmt_from_layout_qi, t_fmt, 0 },
    { "typography", m_typo, t_typo, 1 },
    { "numbersubstitution", m_numsubst, NULL, 0 },
    { "renderingparams", m_rp, t_rp, 0 },
    { "customrenderingparams", m_crp, t_crp, 0 },
    { "textanalyzer", m_analyzer, NULL, 0 },
    { "gdiinterop", m_gdi, NULL, 0 },
    { "ellipsissign", m_ellipsis, t_ellipsis, 0 },
    { "fontfile", m_file, t_file, 0 },
    { "fontface", m_face, t_face, 0 },
    { "customcollection", m_coll, t_coll, 0 },
    { "systemcollection", m_syscoll, t_syscoll, 0 },
    { "fontfamily", m_family, t_family, 0 },
    { "font", m_font, t_font, 0 },
    { "familynames", m_famnames, t_loc, 0 },
    { "facenames", m_facenames, t_loc, 0 },
    { "fontface_from_font", m_fontface_from_font, t_face, 0 },
    { "bitmaprendertarget", m_brt, t_brt, 0 },
    { "glyphrunanalysis", m_gra, t_gra, 0 },
    { "isolatedfactory", m_isofactory, t_factory, 0 },
    { "sharedfactory", m_sharedfactory, t_factory, 0 },
    { "fontsetbuilder", m_fsb, NULL, 0 },
    { "systemfontset", m_sysset, t_set, 0 },
    { "fontfacereference", m_faceref, t_faceref, 0 },
    { "inmemoryloader", m_inmem, NULL, 0 },
    { "downloadqueue", m_dlq, t_dlq, 0 },
    { "collection_from_fontset", m_collfromset, t_syscoll, 0 },
    { "systemcollection1", m_sysfontcoll1, t_syscoll, 0 },
};
const clt_maker *clt_makers(int *n) { *n = (int)(sizeof g_makers / sizeof g_makers[0]); return g_makers; }

/* ---- threads ---------------------------------------------------------------------------------------------- */
typedef struct { clt_thr *t; clt_thr_fn fn; } thr_start;
static DWORD WINAPI thr_main(void *p) {
    thr_start *s = (thr_start *)p; clt_thr *t = s->t; int entered = 0;
    if (t->com_mode == 1) entered = SUCCEEDED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED));
    else if (t->com_mode == 2) entered = SUCCEEDED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED));
    WaitForSingleObject(t->start, 20000);
    s->fn(t);
    if (entered) U(CoUninitialize)();
    return 0;
}
int clt_run(int n, int iters, clt_thr_fn fn, void *arg, clt_thr *t, DWORD com_mode) {
    HANDLE th[CLT_MAXTH]; thr_start st[CLT_MAXTH]; HANDLE start = CreateEventA(NULL, TRUE, FALSE, NULL); int i, ok = 1;
    for (i = 0; i < n; i++) {
        memset(&t[i], 0, sizeof t[i]);
        t[i].idx = i; t[i].nthreads = n; t[i].iters = iters; t[i].arg = arg; t[i].start = start; t[i].com_mode = com_mode; t[i].hash = CLT_H0;
        st[i].t = &t[i]; st[i].fn = fn;
        th[i] = CreateThread(NULL, 0, thr_main, &st[i], 0, NULL);
        if (!th[i]) { ok = 0; InterlockedIncrement(&t[i].fail); }
    }
    SetEvent(start);
    for (i = 0; i < n; i++) if (th[i]) {
        if (WaitForSingleObject(th[i], 90000) != WAIT_OBJECT_0) { TerminateThread(th[i], 1); InterlockedIncrement(&t[i].fail); ok = 0; }
        CloseHandle(th[i]);
    }
    CloseHandle(start);
    return ok;
}
void clt_verify(const clt_thr *t, int n, const char *what) {
    int i;
    for (i = 0; i < n; i++) {
        CHECKF(t[i].fail == 0, "%s: thread %d: %ld in-thread failures, first line %d: %s", what, i, (long)t[i].fail, t[i].fail_line, t[i].fail_expr);
        CHECKF(t[i].hash == t[0].hash, "%s: thread %d digest %016llx differs from thread 0 %016llx", what, i, (unsigned long long)t[i].hash, (unsigned long long)t[0].hash);
    }
}

int clt_lenient;
static void __attribute__((constructor)) clt_init_lenient(void) { clt_lenient = t_is_wine(); }
/* ---- shared workload ------------------------------------------------------------------------------------- */
static uint64_t hash_family_name(IDWriteFontFamily *fam, uint64_t h) {
    IDWriteLocalizedStrings *s = NULL; WCHAR buf[128];
    if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) {
        buf[0] = 0;
        if (SUCCEEDED(IDWriteLocalizedStrings_GetString(s, 0, buf, 128))) h = clt_hw(h, buf);
        IDWriteLocalizedStrings_Release(s);
    }
    return h;
}
uint64_t clt_op(clt_thr *t, IDWriteFactory *f, int op, int i, dwx_mem_loader *ld) {
    uint64_t h = CLT_H0; op %= CLT_NOPS;
    h = clt_hi(h, op);
    switch (op) {
    case 0: {   /* text format */
        IDWriteTextFormat *fm = NULL; float sz = 12.0f + (float)(i % 5); WCHAR nm[64] = { 0 };
        HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz, L"en-us", &fm);
        TCHK(t, hr == S_OK && fm);
        if (fm) {
            TCHK(t, IDWriteTextFormat_GetFontSize(fm) == sz);
            IDWriteTextFormat_GetFontFamilyName(fm, nm, 64);
            TCHK(t, !lstrcmpW(nm, L"Arial"));
            h = clt_hf(h, IDWriteTextFormat_GetFontSize(fm)); h = clt_hi(h, IDWriteTextFormat_GetFontWeight(fm));
            IDWriteTextFormat_Release(fm);
        }
        break; }
    case 1: {   /* layout metrics */
        IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m; HRESULT hr;
        WCHAR txt[] = L"Hello DirectWrite, concurrent layout of a moderately long line of text.";
        float sz = 12.0f + (float)(i % 5);
        IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sz, L"en-us", &fm);
        if (fm) IDWriteFactory_CreateTextLayout(f, txt, lstrlenW(txt), fm, 150.0f, 400.0f, &l);
        TCHK(t, l != NULL);
        if (l) {
            memset(&m, 0, sizeof m);
            hr = IDWriteTextLayout_GetMetrics(l, &m);
            TCHK(t, hr == S_OK || clt_lenient);        /* an emulator without fonts may fail layout; the result must still agree across threads */
            h = clt_hi(h, (uint64_t)(uint32_t)hr);
            TCHK(t, clt_lenient || (m.lineCount >= 1 && m.width > 0.0f));
            h = clt_hf(h, m.width); h = clt_hf(h, m.height); h = clt_hf(h, m.widthIncludingTrailingWhitespace); h = clt_hi(h, m.lineCount);
            IDWriteTextLayout_Release(l);
        }
        DW_RELEASE(fm);
        break; }
    case 2: {   /* system collection enumeration */
        IDWriteFontCollection *c = NULL; UINT32 n, k, lim;
        HRESULT hr = IDWriteFactory_GetSystemFontCollection(f, &c, FALSE);
        TCHK(t, hr == S_OK && c);
        if (c) {
            n = IDWriteFontCollection_GetFontFamilyCount(c);
            TCHK(t, n >= 1);
            h = clt_hi(h, n);
            lim = (i % 40 == 2) ? n : (n < 24 ? n : 24);
            for (k = 0; k < lim; k++) {
                IDWriteFontFamily *fam = NULL;
                if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, k, &fam)) && fam) { h = hash_family_name(fam, h); IDWriteFontFamily_Release(fam); }
                else TCHK(t, 0);
            }
            IDWriteFontCollection_Release(c);
        }
        break; }
    case 3: {   /* FindFamilyName */
        IDWriteFontCollection *c = NULL; UINT32 idx = 7, idx2 = 7; BOOL ex = 5, ex2 = 5;
        IDWriteFactory_GetSystemFontCollection(f, &c, FALSE);
        TCHK(t, c != NULL);
        if (c) {
            TCHK(t, IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex) == S_OK);
            h = clt_hi(h, ex); if (ex) h = clt_hi(h, idx);
            TCHK(t, IDWriteFontCollection_FindFamilyName(c, L"NoSuchFamily Zzyzx", &idx2, &ex2) == S_OK);
            TCHK(t, ex2 == FALSE && idx2 == 0xFFFFFFFFu);   /* documented: index is UINT_MAX when not found */
            IDWriteFontCollection_Release(c);
        }
        break; }
    case 4: {   /* system face: glyph indices and metrics */
        IDWriteFontFace *fc = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (fc) {
            UINT32 cps[5] = { 'H', 'e', 'l', 'l', 'o' }; UINT16 gi[5] = { 0 }; DWRITE_FONT_METRICS fm;
            TCHK(t, IDWriteFontFace_GetGlyphIndices(fc, cps, 5, gi) == S_OK);
            TCHK(t, gi[0] != 0 && gi[2] == gi[3]);
            h = clt_h(h, gi, sizeof gi);
            IDWriteFontFace_GetMetrics(fc, &fm);
            h = clt_hi(h, fm.designUnitsPerEm); h = clt_hi(h, fm.ascent); h = clt_hi(h, fm.descent);
            IDWriteFontFace_Release(fc);
        }
        break; }
    case 5: {   /* table access */
        IDWriteFontFace *fc = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (fc) {
            const void *d = NULL; UINT32 sz = 0; void *ctx = NULL; BOOL ex = FALSE;
            TCHK(t, IDWriteFontFace_TryGetFontTable(fc, DW_TAG('h', 'e', 'a', 'd'), &d, &sz, &ctx, &ex) == S_OK);
            TCHK(t, ex && sz == 54);
            if (ex && d) { h = clt_hi(h, sz); h = clt_h(h, d, sz); }
            if (ex) IDWriteFontFace_ReleaseFontTable(fc, ctx);
            IDWriteFontFace_Release(fc);
        }
        break; }
    case 6: {   /* synthetic font through the registered loader */
        IDWriteFontFile *ff = NULL; IDWriteFontFace *fc = NULL; UINT32 key = 0;
        HRESULT hr = IDWriteFactory_CreateCustomFontFileReference(f, &key, 4, (IDWriteFontFileLoader *)ld, &ff);
        TCHK(t, hr == S_OK && ff);
        if (ff) {
            hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &ff, 0, DWRITE_FONT_SIMULATIONS_NONE, &fc);
            TCHK(t, hr == S_OK && fc);
            if (fc) {
                UINT32 cps[3] = { 'A', 'B', 'C' }; UINT16 gi[3] = { 0 }; DWRITE_GLYPH_METRICS gm[3];
                IDWriteFontFace_GetGlyphIndices(fc, cps, 3, gi);
                TCHK(t, gi[0] == 2 && gi[1] == 3 && gi[2] == 4);
                memset(gm, 0, sizeof gm);
                TCHK(t, IDWriteFontFace_GetDesignGlyphMetrics(fc, gi, 3, gm, FALSE) == S_OK);
                TCHK(t, gm[0].advanceWidth == 1000 && gm[2].advanceWidth == 1000);
                h = clt_h(h, gi, sizeof gi); h = clt_hi(h, gm[1].advanceWidth);
                IDWriteFontFace_Release(fc);
            }
            IDWriteFontFile_Release(ff);
        }
        break; }
    case 7: {   /* typography + number substitution + analyzer */
        IDWriteTypography *ty = NULL; IDWriteNumberSubstitution *ns = NULL; IDWriteTextAnalyzer *an = NULL;
        DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1 };
        TCHK(t, IDWriteFactory_CreateTypography(f, &ty) == S_OK && ty);
        if (ty) {
            TCHK(t, IDWriteTypography_AddFontFeature(ty, ft) == S_OK);
            TCHK(t, IDWriteTypography_GetFontFeatureCount(ty) == 1);
            h = clt_hi(h, IDWriteTypography_GetFontFeatureCount(ty));
            IDWriteTypography_Release(ty);
        }
        TCHK(t, IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-US", FALSE, &ns) == S_OK && ns);
        DW_RELEASE(ns);
        TCHK(t, IDWriteFactory_CreateTextAnalyzer(f, &an) == S_OK && an);
        DW_RELEASE(an);
        break; }
    }
    return h;
}

/* ---- custom collection fixture ---- */
int clt_cfix_open(clt_cfix *x, int nfonts) {
    int i; memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    for (i = 0; i < nfonts; i++) {
        char fam[32]; size_t n; uint8_t *b; UINT32 key;
        sprintf(fam, "CltFam%d", i);
        b = ttf_build_styled(fam, 400, 5, 0, &n); key = dwx_loader_add(x->fl, b, n); free(b);
        if (i == 0) {}
        (void)key;
    }
    if (FAILED(dwx_loader_register(x->fl, x->f))) return 0;
    x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < nfonts; i++) dwx_coll_add_key(x->cl, (UINT32)i);
    if (FAILED(dwx_coll_register(x->cl, x->f))) return 0;
    return 1;
}
void clt_cfix_close(clt_cfix *x) {
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); }
    DW_RELEASE(x->f);
}
