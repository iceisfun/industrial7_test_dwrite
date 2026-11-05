#include "tl.h"

static ttf_glyph g_narrow[TTF_DEF_NGLYPHS];

static uint8_t *build_family(const char *family, int narrow, size_t *sz)
{
    ttf_spec s; char ps[96];
    ttf_default_spec(&s);
    snprintf(ps, sizeof ps, "%s-Regular", family);
    ttf_set_names(&s, family, "Regular", family, ps);
    if (narrow) {
        int n = 0, i; const ttf_glyph *g = ttf_default_glyphs(&n);
        if (n > TTF_DEF_NGLYPHS) n = TTF_DEF_NGLYPHS;
        memcpy(g_narrow, g, sizeof(ttf_glyph) * n);
        for (i = 0; i < n; i++) {
            if (g_narrow[i].adv == TTF_DEF_ADV) g_narrow[i].adv = 500;
            else if (g_narrow[i].adv == 500) g_narrow[i].adv = 250;
        }
        g_narrow[TTF_GID_SPACE].adv = 250;
        s.glyphs = g_narrow; s.nglyphs = n;
    }
    return ttf_build(&s, sz);
}

int tl_open(tl_fx *x)
{
    size_t sz; uint8_t *b; int i;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    for (i = 0; i < 2; i++) {
        b = build_family(i ? "DwNarrow" : "DwTest", i, &sz);
        if (!b) return 0;
        x->key[i] = dwx_loader_add(x->fl, b, sz); free(b);
    }
    if (FAILED(dwx_loader_register(x->fl, x->f))) return 0;
    x->cl = dwx_coll_new(x->fl);
    dwx_coll_add_key(x->cl, x->key[0]); dwx_coll_add_key(x->cl, x->key[1]);
    if (FAILED(dwx_coll_register(x->cl, x->f))) return 0;
    if (FAILED(dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) return 0;
    return 1;
}

void tl_close(tl_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}

IDWriteTextFormat *tl_format(tl_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, TL_LOC, &t))) return NULL;
    return t;
}

IDWriteTextLayout *tl_layout_(tl_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h)
{
    IDWriteTextFormat *t = tl_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);        /* the layout keeps its own copy: the format is not needed afterwards */
    return l;
}

void tl_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m)
{
    HRESULT hr;
    memset(m, 0xcc, sizeof *m);
    hr = IDWriteTextLayout_GetMetrics(l, m);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetMetrics", "got %s", dw_hr_name(hr));
}
UINT32 tl_lines(IDWriteTextLayout *l) { DWRITE_TEXT_METRICS m; tl_metrics(l, &m); return m.lineCount; }

void *tl_qi(IUnknown *u, const IID *iid, const char *name)
{
    void *p = NULL;
    HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    if (FAILED(hr) || !p) { t_skip("QueryInterface(%s) -> %s", name, dw_hr_name(hr)); }
    return p;
}

void tl_snap(IDWriteTextLayout *l, tl_state *o)
{
    memset(o, 0, sizeof *o);
    o->ta = IDWriteTextLayout_GetTextAlignment(l); o->pa = IDWriteTextLayout_GetParagraphAlignment(l);
    o->ww = IDWriteTextLayout_GetWordWrapping(l); o->rd = IDWriteTextLayout_GetReadingDirection(l); o->fd = IDWriteTextLayout_GetFlowDirection(l);
    o->tab = IDWriteTextLayout_GetIncrementalTabStop(l);
    IDWriteTextLayout_GetTrimming(l, &o->trim, &o->sign);
    if (o->sign) IDWriteInlineObject_Release(o->sign);
    IDWriteTextLayout_GetLineSpacing(l, &o->lsm, &o->ls, &o->bl);
    o->maxw = IDWriteTextLayout_GetMaxWidth(l); o->maxh = IDWriteTextLayout_GetMaxHeight(l);
}
int tl_state_eq(const tl_state *a, const tl_state *b)
{
    return a->ta == b->ta && a->pa == b->pa && a->ww == b->ww && a->rd == b->rd && a->fd == b->fd && tl_fbits(a->tab, b->tab) &&
           a->trim.granularity == b->trim.granularity && a->trim.delimiter == b->trim.delimiter && a->trim.delimiterCount == b->trim.delimiterCount &&
           a->sign == b->sign && a->lsm == b->lsm && tl_fbits(a->ls, b->ls) && tl_fbits(a->bl, b->bl) && tl_fbits(a->maxw, b->maxw) && tl_fbits(a->maxh, b->maxh);
}

static HRESULT WINAPI fb_QI(IDWriteFontFallback *This, REFIID riid, void **ppv)
{
    tl_fb *o = (tl_fb *)This; InterlockedIncrement(&o->qi);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFallback)) { *ppv = This; InterlockedIncrement(&o->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI fb_AddRef(IDWriteFontFallback *This) { tl_fb *o = (tl_fb *)This; InterlockedIncrement(&o->addref); return InterlockedIncrement(&o->ref); }
static ULONG WINAPI fb_Release(IDWriteFontFallback *This) { tl_fb *o = (tl_fb *)This; InterlockedIncrement(&o->release); return InterlockedDecrement(&o->ref); }
static HRESULT WINAPI fb_Map(IDWriteFontFallback *This, IDWriteTextAnalysisSource *src, UINT32 pos, UINT32 len, IDWriteFontCollection *base,
                             const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE s, DWRITE_FONT_STRETCH st, UINT32 *mapped,
                             IDWriteFont **font, FLOAT *scale)
{
    tl_fb *o = (tl_fb *)This; InterlockedIncrement(&o->map);
    if (mapped) *mapped = 0; if (font) *font = NULL; if (scale) *scale = 1.0f; return S_OK;
}
static const IDWriteFontFallbackVtbl fb_vtbl = { fb_QI, fb_AddRef, fb_Release, fb_Map };
void tl_fb_init(tl_fb *o) { memset(o, 0, sizeof *o); o->lpVtbl = &fb_vtbl; o->ref = 1; }
