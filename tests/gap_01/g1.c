#include "g1.h"

/* ---- fixture --------------------------------------------------------------------------------------------------------- */
static uint8_t *build_font(size_t *sz)
{
    ttf_spec s;
    ttf_default_spec(&s);
    ttf_set_names(&s, "G1Fam", "Regular", "G1Fam", "G1Fam-Regular");
    return ttf_build(&s, sz);
}

int g1_open(g1_fx *x)
{
    size_t sz; uint8_t *b;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    b = build_font(&sz); if (!b) return 0;
    x->key = dwx_loader_add(x->fl, b, sz); free(b);
    if (FAILED(dwx_loader_register(x->fl, x->f))) return 0;
    x->cl = dwx_coll_new(x->fl);
    dwx_coll_add_key(x->cl, x->key);
    if (FAILED(dwx_coll_register(x->cl, x->f))) return 0;
    if (FAILED(dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) return 0;
    return 1;
}

void g1_close(g1_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}

IDWriteTextFormat *g1_format(g1_fx *x, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, G1_FAM, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &t))) return NULL;
    return t;
}

IDWriteTextLayout *g1_layout(g1_fx *x, const WCHAR *text, float width, float height, float size)
{
    IDWriteTextFormat *t = g1_format(x, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, width, height, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

HRESULT g1_get_lines(IDWriteTextLayout *l, g1_lines *o)
{
    UINT32 n = 0; HRESULT hr;
    memset(o, 0, sizeof *o);
    hr = IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n);
    if (hr != DW_E_NOT_SUFFICIENT_BUFFER && hr != S_OK) return hr;
    if (n > 64) n = 64;
    return IDWriteTextLayout_GetLineMetrics(l, o->lm, n, &o->n);
}

const char *g1_lens_str(IDWriteTextLayout *l)
{
    static char buf[256]; g1_lines o; UINT32 i; char t[24];
    buf[0] = 0;
    if (FAILED(g1_get_lines(l, &o))) return "?";
    for (i = 0; i < o.n; i++) { snprintf(t, sizeof t, "%s%u", i ? "," : "", o.lm[i].length); if (strlen(buf) + strlen(t) < sizeof buf) strcat(buf, t); }
    return buf;
}

void g1_check_lens_(IDWriteTextLayout *l, const UINT32 *want, int n, const char *file, int line)
{
    g1_lines o; char w[256] = "", t[24]; int i, ok;
    HRESULT hr = g1_get_lines(l, &o);
    if (FAILED(hr)) { t_check(0, file, line, "line lengths", "GetLineMetrics failed %s", dw_hr_name(hr)); return; }
    ok = (int)o.n == n;
    for (i = 0; ok && i < n; i++) if (o.lm[i].length != want[i]) ok = 0;
    if (!ok) {
        for (i = 0; i < n; i++) { snprintf(t, sizeof t, "%s%u", i ? "," : "", want[i]); if (strlen(w) + strlen(t) < sizeof w) strcat(w, t); }
        t_check(0, file, line, "line lengths", "got {%s} want {%s}", g1_lens_str(l), w);
    } else t_check(1, file, line, "line lengths", NULL);
}

int g1_analyze_breaks(IDWriteFactory *f, const WCHAR *text, UINT32 n, DWRITE_LINE_BREAKPOINT *out)
{
    IDWriteTextAnalyzer *an = NULL; dwx_ana_src *src; dwx_ana_sink *snk; UINT32 i; int ok;
    if (n > DWX_ANA_MAX || FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) return 0;
    src = dwx_ana_src_new(text, n); snk = dwx_ana_sink_new();
    ok = SUCCEEDED(IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)src, 0, n, (IDWriteTextAnalysisSink *)snk));
    if (ok) for (i = 0; i < n; i++) out[i] = snk->brk[i];
    IDWriteTextAnalyzer_Release(an); dwx_release(src); dwx_release(snk);
    return ok;
}

/* ---- recording inline object -------------------------------------------------------------------------------------------- */
static HRESULT WINAPI io_QI(IDWriteInlineObject *This, REFIID r, void **o)
{
    g1_io *s = (g1_io *)This; InterlockedIncrement(&s->n_qi);
    *o = NULL;
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteInlineObject)) { *o = This; InterlockedIncrement(&s->ref); return S_OK; }
    return E_NOINTERFACE;
}
static ULONG WINAPI io_AR(IDWriteInlineObject *This) { return InterlockedIncrement(&((g1_io *)This)->ref); }
static ULONG WINAPI io_RL(IDWriteInlineObject *This) { g1_io *s = (g1_io *)This; LONG r = InterlockedDecrement(&s->ref); if (r == 0) s->destroyed = 1; return r < 0 ? 0 : r; }
static HRESULT WINAPI io_Draw(IDWriteInlineObject *This, void *ctx, IDWriteTextRenderer *rd, FLOAT x, FLOAT y, BOOL sw, BOOL rtl, IUnknown *fx)
{
    g1_io *s = (g1_io *)This; InterlockedIncrement(&s->n_draw);
    s->draw_ctx = ctx; s->draw_x = x; s->draw_y = y; s->draw_sideways = sw; s->draw_rtl = rtl;
    return s->hr_draw;
}
static HRESULT WINAPI io_Metrics(IDWriteInlineObject *This, DWRITE_INLINE_OBJECT_METRICS *m)
{
    g1_io *s = (g1_io *)This; InterlockedIncrement(&s->n_metrics);
    if (FAILED(s->hr_metrics) && !s->write_on_fail) return s->hr_metrics;
    *m = s->metrics; return s->hr_metrics;
}
static HRESULT WINAPI io_Overhang(IDWriteInlineObject *This, DWRITE_OVERHANG_METRICS *m)
{
    g1_io *s = (g1_io *)This; InterlockedIncrement(&s->n_overhang);
    if (FAILED(s->hr_overhang) && !s->write_on_fail) return s->hr_overhang;
    *m = s->overhang; return s->hr_overhang;
}
static HRESULT WINAPI io_Break(IDWriteInlineObject *This, DWRITE_BREAK_CONDITION *b, DWRITE_BREAK_CONDITION *a)
{
    g1_io *s = (g1_io *)This; int i = InterlockedIncrement(&s->n_break) - 1;
    if (i < G1_IO_LOG) { s->in_before[i] = *b; s->in_after[i] = *a; s->nlog = i + 1; }
    if (s->write_break && (SUCCEEDED(s->hr_break) || s->write_on_fail)) { *b = s->out_before; *a = s->out_after; }
    return s->hr_break;
}
static const IDWriteInlineObjectVtbl io_vt = { io_QI, io_AR, io_RL, io_Draw, io_Metrics, io_Overhang, io_Break };
g1_io *g1_io_new(float w, float h, float base)
{
    g1_io *o = (g1_io *)calloc(1, sizeof *o);
    o->lpVtbl = &io_vt; o->ref = 1;
    o->metrics.width = w; o->metrics.height = h; o->metrics.baseline = base;
    return o;
}

/* ---- remote loader ---------------------------------------------------------------------------------------------------------- */
static HRESULT WINAPI rm_QI(IDWriteRemoteFontFileLoader *This, REFIID r, void **o)
{
    g1_remote *l = (g1_remote *)This;
    *o = NULL;
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteFontFileLoader) || IsEqualIID(r, &IID_IDWriteRemoteFontFileLoader)) { *o = This; InterlockedIncrement(&l->ref); return S_OK; }
    return E_NOINTERFACE;
}
static ULONG WINAPI rm_AR(IDWriteRemoteFontFileLoader *This) { return InterlockedIncrement(&((g1_remote *)This)->ref); }
static ULONG WINAPI rm_RL(IDWriteRemoteFontFileLoader *This) { LONG r = InterlockedDecrement(&((g1_remote *)This)->ref); return r < 0 ? 0 : r; }
static HRESULT WINAPI rm_CreateStream(IDWriteRemoteFontFileLoader *This, const void *key, UINT32 ks, IDWriteFontFileStream **s)
{
    g1_remote *l = (g1_remote *)This; InterlockedIncrement(&l->n_create);
    return IDWriteFontFileLoader_CreateStreamFromKey((IDWriteFontFileLoader *)l->inner, key, ks, s);
}
static HRESULT WINAPI rm_CreateRemoteStream(IDWriteRemoteFontFileLoader *This, const void *key, UINT32 ks, IDWriteRemoteFontFileStream **s)
{
    g1_remote *l = (g1_remote *)This; InterlockedIncrement(&l->n_remote_create);
    if (s) *s = NULL;
    return E_NOTIMPL;
}
static HRESULT WINAPI rm_Locality(IDWriteRemoteFontFileLoader *This, const void *key, UINT32 ks, DWRITE_LOCALITY *loc)
{
    g1_remote *l = (g1_remote *)This; InterlockedIncrement(&l->n_locality);
    l->last_key_size = ks;
    if (FAILED(l->locality_hr)) return l->locality_hr;
    *loc = l->locality;
    return S_OK;
}
static HRESULT WINAPI rm_Url(IDWriteRemoteFontFileLoader *This, IDWriteFactory *f, const WCHAR *b, const WCHAR *u, IDWriteFontFile **out)
{
    if (out) *out = NULL;
    return E_NOTIMPL;
}
static const IDWriteRemoteFontFileLoaderVtbl rm_vt = { rm_QI, rm_AR, rm_RL, rm_CreateStream, rm_CreateRemoteStream, rm_Locality, rm_Url };
g1_remote *g1_remote_new(dwx_mem_loader *inner, DWRITE_LOCALITY loc)
{
    g1_remote *r = (g1_remote *)calloc(1, sizeof *r);
    r->lpVtbl = &rm_vt; r->ref = 1; r->inner = inner; dwx_addref(inner); r->locality = loc;
    return r;
}
HRESULT g1_remote_register(g1_remote *r, IDWriteFactory *f)
{
    HRESULT hr = IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)r);
    if (SUCCEEDED(hr)) { r->registered = 1; r->reg_factory = f; }
    return hr;
}
void g1_remote_close(g1_remote *r)
{
    if (r->registered) { IDWriteFactory_UnregisterFontFileLoader(r->reg_factory, (IDWriteFontFileLoader *)r); r->registered = 0; }
}

/* ---- synthetic faces / misc ------------------------------------------------------------------------------------------------- */
int g1_face_open(g1_face *g, const ttf_spec *s)
{
    size_t n; uint8_t *b = ttf_build(s, &n); HRESULT hr;
    memset(g, 0, sizeof *g);
    if (!b) return 0;
    g->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g->f) { free(b); return 0; }
    hr = dwx_font_open(&g->h, g->f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(b);
    if (FAILED(hr) || !g->h.face) return 0;
    g->ok = 1;
    return 1;
}
void g1_face_close(g1_face *g) { dwx_font_close(&g->h); DW_RELEASE(g->f); }

void *g1_qi(IUnknown *u, const IID *iid)
{
    void *p = NULL;
    if (!u || FAILED(IUnknown_QueryInterface(u, iid, &p))) return NULL;
    return p;
}

IDWriteFont *g1_sysfont(IDWriteFactory *f, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st)
{
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return NULL;
    if (SUCCEEDED(IDWriteFontCollection_FindFamilyName(c, family, &idx, &ex)) && ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam)
        IDWriteFontFamily_GetFirstMatchingFont(fam, w, DWRITE_FONT_STRETCH_NORMAL, st, &font);
    DW_RELEASE(fam); DW_RELEASE(c);
    return font;
}

/* ---- one synthetic font through a custom collection ------------------------------------------------------------------------- */
int g1_fnt_open(g1_fnt *g, const ttf_spec *s)
{
    size_t n; uint8_t *b = ttf_build(s, &n); UINT32 key;
    memset(g, 0, sizeof *g);
    if (!b) return 0;
    g->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!g->f) { free(b); return 0; }
    g->fl = dwx_loader_new();
    key = dwx_loader_add(g->fl, b, n); free(b);
    if (FAILED(dwx_loader_register(g->fl, g->f))) return 0;
    g->cl = dwx_coll_new(g->fl);
    dwx_coll_add_key(g->cl, key);
    if (FAILED(dwx_coll_register(g->cl, g->f))) return 0;
    if (FAILED(dwx_coll_create(g->cl, g->f, &g->coll)) || !g->coll) return 0;
    if (IDWriteFontCollection_GetFontFamilyCount(g->coll) < 1) return 0;
    if (FAILED(IDWriteFontCollection_GetFontFamily(g->coll, 0, &g->fam)) || !g->fam) return 0;
    if (FAILED(IDWriteFontFamily_GetFont(g->fam, 0, &g->font)) || !g->font) return 0;
    if (FAILED(IDWriteFont_CreateFontFace(g->font, &g->face)) || !g->face) return 0;
    g->ok = 1;
    return 1;
}
void g1_fnt_close(g1_fnt *g)
{
    DW_RELEASE(g->face); DW_RELEASE(g->font); DW_RELEASE(g->fam); DW_RELEASE(g->coll);
    if (g->cl) { dwx_coll_unregister(g->cl); dwx_release(g->cl); g->cl = NULL; }
    if (g->fl) { dwx_loader_unregister(g->fl); dwx_release(g->fl); g->fl = NULL; }
    DW_RELEASE(g->f);
}

int g1_patch_os2(ttf_spec *s, unsigned off, const void *bytes, unsigned len)
{
    static ttf_rawtable raws[256]; static int nr;
    size_t sz; uint8_t *blob = ttf_build(s, &sz), *copy; unsigned tl = 0; const uint8_t *t; int cnt = 0, i;
    if (!blob) return 0;
    t = ttf_find_table(blob, sz, "OS/2", &tl);
    if (!t || off + len > tl || nr > 250) { free(blob); return 0; }
    copy = (uint8_t *)malloc(tl); memcpy(copy, t, tl); memcpy(copy + off, bytes, len); free(blob);
    for (i = 0; i < s->nraw; i++) { if (!memcmp(s->raw[i].tag, "OS/2", 4)) continue; raws[nr + cnt++] = s->raw[i]; }
    memcpy(raws[nr + cnt].tag, "OS/2", 5); raws[nr + cnt].data = copy; raws[nr + cnt].len = tl; cnt++;
    s->raw = &raws[nr]; s->nraw = cnt; nr += cnt;
    return 1;
}
