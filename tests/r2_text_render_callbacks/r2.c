#include "r2.h"

/* ---- fixture fonts ------------------------------------------------------------------------------------------ */
static ttf_map g_map[256]; static int g_nmap;
static ttf_cmap g_cm[2];
static ttf_glyph g_bold[TTF_DEF_NGLYPHS];

static void build_map(void)
{
    unsigned cp; int n = 0;
    static const struct { unsigned cp, gid; } extra[] = {
        { 0x05d0, 2 }, { 0x05d1, 3 }, { 0x05d2, 4 },               /* Hebrew alef bet gimel */
        { 0x03b1, 28 }, { 0x03b2, 29 },                            /* Greek alpha beta */
        { 0x2026, TTF_GID_TRIANGLE },                              /* horizontal ellipsis */
    };
    if (g_nmap) return;
    for (cp = 0x20; cp < 0x7f; cp++) { unsigned g = ttf_default_gid(cp); if (g) { g_map[n].cp = cp; g_map[n].gid = g; n++; } }
    for (cp = 0; cp < sizeof extra / sizeof extra[0]; cp++) { g_map[n].cp = extra[cp].cp; g_map[n].gid = extra[cp].gid; n++; }
    g_nmap = n;
    g_cm[0].format = 4;  g_cm[0].plat = 3; g_cm[0].enc = 1;  g_cm[0].n = n; g_cm[0].map = g_map;
    g_cm[1].format = 12; g_cm[1].plat = 3; g_cm[1].enc = 10; g_cm[1].n = n; g_cm[1].map = g_map;
}
static uint8_t *build_font(int bold, size_t *sz)
{
    ttf_spec s;
    build_map();
    if (bold) {
        int n = 0, i; const ttf_glyph *g = ttf_default_glyphs(&n);
        ttf_style_spec(&s, "R2Font", 700, 5, 0);
        if (n > TTF_DEF_NGLYPHS) n = TTF_DEF_NGLYPHS;
        memcpy(g_bold, g, sizeof(ttf_glyph) * n);
        for (i = 0; i < n; i++) if (g_bold[i].adv == TTF_DEF_ADV) g_bold[i].adv = 1200;
        g_bold[TTF_GID_SPACE].adv = 600;
        s.glyphs = g_bold; s.nglyphs = n;
    } else {
        ttf_default_spec(&s);
        ttf_set_names(&s, "R2Font", "Regular", "R2Font", "R2Font-Regular");
    }
    s.cmaps = g_cm; s.ncmaps = 2;
    return ttf_build(&s, sz);
}
int r2_open(r2_fx *x)
{
    size_t sz; uint8_t *b; int i; UINT32 key[2];
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    for (i = 0; i < 2; i++) {
        b = build_font(i, &sz);
        if (!b) return 0;
        key[i] = dwx_loader_add(x->fl, b, sz); free(b);
    }
    if (FAILED(dwx_loader_register(x->fl, x->f))) return 0;
    x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < 2; i++) dwx_coll_add_key(x->cl, key[i]);
    if (FAILED(dwx_coll_register(x->cl, x->f))) return 0;
    if (FAILED(dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) return 0;
    return 1;
}
void r2_close(r2_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}
IDWriteTextFormat *r2_format(r2_fx *x, float size, DWRITE_FONT_WEIGHT weight)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, R2_FAM, x->coll, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, R2_LOC, &t))) return NULL;
    return t;
}
IDWriteTextLayout *r2_layout_w(r2_fx *x, const WCHAR *text, float size, float w, float h, DWRITE_FONT_WEIGHT weight)
{
    IDWriteTextFormat *t = r2_format(x, size, weight); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

/* ---- hook renderer -------------------------------------------------------------------------------------------- */
static rh_ev *newev(rh *r, int type)
{
    rh_ev *e;
    if (r->nev >= RH_MAXEV) { static rh_ev junk; memset(&junk, 0, sizeof junk); r->count[type]++; return &junk; }
    e = &r->ev[r->nev++]; memset(e, 0, sizeof *e);
    e->type = type; e->effect_refs = -1; e->mxcsr = r2_getcsr();
    r->count[type]++;
    return e;
}
static void probe_effect(rh_ev *e, IUnknown *fx)
{
    e->effect = fx;
    if (fx) { ULONG n = IUnknown_AddRef(fx); e->effect_refs = (LONG)n - 1; IUnknown_Release(fx); }
}
/* common tail of every non-QI callback: hook, MXCSR load, forced result */
static HRESULT finish(rh *r, rh_ev *e, HRESULT natural)
{
    int t = e->type, idx = r->count[t];
    HRESULT hr = natural;
    if (r->hook) r->hook(r, e, r->hook_arg);
    if (r->set_csr[t]) r2_setcsr(r->csr_val[t]);
    if (r->ret_set[t] && (!r->ret_nth[t] || r->ret_nth[t] == idx)) hr = r->ret[t];
    e->hr = hr;
    return hr;
}
#define ENTER(r) do { (r)->in_cb++; if ((r)->in_cb > (r)->max_depth) (r)->max_depth = (r)->in_cb; } while (0)
#define LEAVE(r) ((r)->in_cb--)

static HRESULT WINAPI h_QI(IDWriteTextRenderer1 *This, REFIID riid, void **ppv)
{
    rh *r = (rh *)This; rh_ev *e = newev(r, RH_QI); int ok = 0;
    e->iid = *riid;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWritePixelSnapping) || IsEqualIID(riid, &IID_IDWriteTextRenderer)) ok = 1;
    else if (IsEqualIID(riid, &IID_IDWriteTextRenderer1)) ok = r->allow_r1;
    if (!ok) { *ppv = NULL; e->hr = E_NOINTERFACE; return E_NOINTERFACE; }
    *ppv = This; InterlockedIncrement(&r->ref); e->hr = S_OK; return S_OK;
}
static ULONG WINAPI h_AR(IDWriteTextRenderer1 *This) { rh *r = (rh *)This; InterlockedIncrement(&r->addref); return InterlockedIncrement(&r->ref); }
static ULONG WINAPI h_RL(IDWriteTextRenderer1 *This) { rh *r = (rh *)This; InterlockedIncrement(&r->release); return InterlockedDecrement(&r->ref); }
static HRESULT WINAPI h_Snap(IDWriteTextRenderer1 *This, void *c, BOOL *v)
{
    rh *r = (rh *)This; rh_ev *e = newev(r, RH_SNAP); HRESULT hr; ENTER(r);
    e->ctx = c;
    if (r->snap_write) *v = r->snap_val;
    hr = finish(r, e, S_OK); LEAVE(r); return hr;
}
static HRESULT WINAPI h_Xf(IDWriteTextRenderer1 *This, void *c, DWRITE_MATRIX *m)
{
    rh *r = (rh *)This; rh_ev *e = newev(r, RH_XFORM); HRESULT hr; ENTER(r);
    e->ctx = c;
    if (r->xf_write) *m = r->xf;
    hr = finish(r, e, S_OK); LEAVE(r); return hr;
}
static HRESULT WINAPI h_Ppd(IDWriteTextRenderer1 *This, void *c, FLOAT *v)
{
    rh *r = (rh *)This; rh_ev *e = newev(r, RH_PPD); HRESULT hr; ENTER(r);
    e->ctx = c;
    if (r->ppd_write) *v = r->ppd_val;
    hr = finish(r, e, S_OK); LEAVE(r); return hr;
}
static void rec_glyph(rh_ev *e, void *c, float x, float y, DWRITE_MEASURING_MODE mm, const DWRITE_GLYPH_RUN *g, const DWRITE_GLYPH_RUN_DESCRIPTION *d, IUnknown *fx)
{
    UINT32 i, n;
    e->ctx = c; e->x = x; e->y = y; e->mm = mm; probe_effect(e, fx);
    e->run_ptr = g;
    if (g) {
        e->face = g->fontFace; e->em = g->fontEmSize; e->n = g->glyphCount; e->sideways = g->isSideways; e->bidi = (UINT8)g->bidiLevel;
        e->offsets_ptr = g->glyphOffsets; e->advances_ptr = g->glyphAdvances; e->indices_ptr = g->glyphIndices;
        n = g->glyphCount > RH_MAXG ? RH_MAXG : g->glyphCount;
        for (i = 0; i < n; i++) {
            e->idx[i] = g->glyphIndices ? g->glyphIndices[i] : 0xffff;
            e->adv[i] = g->glyphAdvances ? g->glyphAdvances[i] : -1.0f;
            if (g->glyphOffsets) e->off[i] = g->glyphOffsets[i];
        }
    }
    if (d) {
        e->has_desc = 1; e->tlen = d->stringLength; e->text_ptr = d->string; e->cmap_ptr = d->clusterMap; e->locale_ptr = d->localeName;
        if (d->localeName) { wcsncpy(e->locale, d->localeName, 23); e->locale[23] = 0; }
        n = d->stringLength > RH_MAXG ? RH_MAXG : d->stringLength;
        for (i = 0; i < n; i++) { e->text[i] = d->string[i]; e->cmap[i] = d->clusterMap ? d->clusterMap[i] : 0xffff; }
    }
}
static HRESULT WINAPI h_Glyph(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mm, const DWRITE_GLYPH_RUN *g, const DWRITE_GLYPH_RUN_DESCRIPTION *d, IUnknown *fx)
{
    rh *r = (rh *)This; rh_ev *e = newev(r, RH_GLYPH); HRESULT hr; ENTER(r);
    rec_glyph(e, c, x, y, mm, g, d, fx); hr = finish(r, e, S_OK); LEAVE(r); return hr;
}
static void rec_ul(rh_ev *e, void *c, float x, float y, const DWRITE_UNDERLINE *u, IUnknown *fx)
{
    e->ctx = c; e->x = x; e->y = y; probe_effect(e, fx);
    if (u) { e->ul = *u; if (u->localeName) { wcsncpy(e->dloc, u->localeName, 23); e->dloc[23] = 0; } }
}
static void rec_st(rh_ev *e, void *c, float x, float y, const DWRITE_STRIKETHROUGH *s, IUnknown *fx)
{
    e->ctx = c; e->x = x; e->y = y; probe_effect(e, fx);
    if (s) { e->st = *s; if (s->localeName) { wcsncpy(e->dloc, s->localeName, 23); e->dloc[23] = 0; } }
}
static HRESULT WINAPI h_Under(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, const DWRITE_UNDERLINE *u, IUnknown *fx)
{ rh *r = (rh *)This; rh_ev *e = newev(r, RH_UNDER); HRESULT hr; ENTER(r); rec_ul(e, c, x, y, u, fx); hr = finish(r, e, S_OK); LEAVE(r); return hr; }
static HRESULT WINAPI h_Strike(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH *s, IUnknown *fx)
{ rh *r = (rh *)This; rh_ev *e = newev(r, RH_STRIKE); HRESULT hr; ENTER(r); rec_st(e, c, x, y, s, fx); hr = finish(r, e, S_OK); LEAVE(r); return hr; }
static void rec_in(rh_ev *e, void *c, float x, float y, IDWriteInlineObject *o, BOOL sw, BOOL rtl, IUnknown *fx)
{ e->ctx = c; e->x = x; e->y = y; e->obj = o; e->osw = sw; e->ortl = rtl; probe_effect(e, fx); }
static HRESULT WINAPI h_Inline(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, IDWriteInlineObject *o, BOOL sw, BOOL rtl, IUnknown *fx)
{ rh *r = (rh *)This; rh_ev *e = newev(r, RH_INLINE); HRESULT hr; ENTER(r); rec_in(e, c, x, y, o, sw, rtl, fx); hr = finish(r, e, S_OK); LEAVE(r); return hr; }
static HRESULT WINAPI h1_Glyph(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, DWRITE_MEASURING_MODE mm, const DWRITE_GLYPH_RUN *g, const DWRITE_GLYPH_RUN_DESCRIPTION *d, IUnknown *fx)
{ rh *r = (rh *)This; rh_ev *e = newev(r, RH_GLYPH); HRESULT hr; ENTER(r); e->r1 = 1; e->angle = a; rec_glyph(e, c, x, y, mm, g, d, fx); hr = finish(r, e, S_OK); LEAVE(r); return hr; }
static HRESULT WINAPI h1_Under(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, const DWRITE_UNDERLINE *u, IUnknown *fx)
{ rh *r = (rh *)This; rh_ev *e = newev(r, RH_UNDER); HRESULT hr; ENTER(r); e->r1 = 1; e->angle = a; rec_ul(e, c, x, y, u, fx); hr = finish(r, e, S_OK); LEAVE(r); return hr; }
static HRESULT WINAPI h1_Strike(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, const DWRITE_STRIKETHROUGH *s, IUnknown *fx)
{ rh *r = (rh *)This; rh_ev *e = newev(r, RH_STRIKE); HRESULT hr; ENTER(r); e->r1 = 1; e->angle = a; rec_st(e, c, x, y, s, fx); hr = finish(r, e, S_OK); LEAVE(r); return hr; }
static HRESULT WINAPI h1_Inline(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, IDWriteInlineObject *o, BOOL sw, BOOL rtl, IUnknown *fx)
{ rh *r = (rh *)This; rh_ev *e = newev(r, RH_INLINE); HRESULT hr; ENTER(r); e->r1 = 1; e->angle = a; rec_in(e, c, x, y, o, sw, rtl, fx); hr = finish(r, e, S_OK); LEAVE(r); return hr; }

static const IDWriteTextRenderer1Vtbl h_vt = { h_QI, h_AR, h_RL, h_Snap, h_Xf, h_Ppd, h_Glyph, h_Under, h_Strike, h_Inline, h1_Glyph, h1_Under, h1_Strike, h1_Inline };

rh *rh_new(void)
{
    rh *r = (rh *)calloc(1, sizeof *r);
    r->lpVtbl = &h_vt; r->ref = 1;
    r->snap_write = 1; r->snap_val = FALSE; r->ppd_write = 1; r->ppd_val = 1.0f; r->xf_write = 1;
    r->xf.m11 = 1; r->xf.m22 = 1;
    return r;
}
void rh_free(rh *r) { free(r); }
void rh_clear(rh *r) { r->nev = 0; memset(r->count, 0, sizeof r->count); }
const rh_ev *rh_nth(const rh *r, int type, int n)
{
    int i, k = 0;
    for (i = 0; i < r->nev; i++) if (r->ev[i].type == type) { if (k == n) return &r->ev[i]; k++; }
    return NULL;
}
int rh_index(const rh *r, const rh_ev *e) { return (int)(e - r->ev); }
int rh_first(const rh *r, int type) { int i; for (i = 0; i < r->nev; i++) if (r->ev[i].type == type) return i; return -1; }
int rh_last(const rh *r, int type) { int i; for (i = r->nev - 1; i >= 0; i--) if (r->ev[i].type == type) return i; return -1; }
int rh_qi_asked(const rh *r, const IID *iid) { int i, n = 0; for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_QI && IsEqualIID(&r->ev[i].iid, iid)) n++; return n; }
UINT32 rh_total_glyphs(const rh *r) { int i; UINT32 n = 0; for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH) n += r->ev[i].n; return n; }
float rh_run_width(const rh_ev *e) { UINT32 i, n = e->n > RH_MAXG ? RH_MAXG : e->n; float w = 0; for (i = 0; i < n; i++) w += e->adv[i]; return w; }
