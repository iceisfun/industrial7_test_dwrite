#include "td.h"

/* ---- fixture fonts ---------------------------------------------------------------------------------------- */
static ttf_map g_map[256]; static int g_nmap;
static ttf_cmap g_cm[2];
static ttf_glyph g_bold[TTF_DEF_NGLYPHS];

static void build_map(void)
{
    unsigned cp; int n = 0;
    static const struct { unsigned cp, gid; } extra[] = {
        { 0x03b1, 28 }, { 0x03b2, 29 }, { 0x03b3, 30 },            /* Greek small alpha beta gamma */
        { 0x05d0, 2 }, { 0x05d1, 3 }, { 0x05d2, 4 },               /* Hebrew alef bet gimel */
        { 0x4e00, 2 }, { 0x4e01, 3 }, { 0x4e02, 4 },               /* CJK ideographs */
        { 0x2026, TTF_GID_TRIANGLE },                              /* horizontal ellipsis */
    };
    if (g_nmap) return;
    for (cp = 0x20; cp < 0x7f; cp++) { unsigned g = ttf_default_gid(cp); if (g) { g_map[n].cp = cp; g_map[n].gid = g; n++; } }
    for (cp = 0; cp < sizeof extra / sizeof extra[0]; cp++) { g_map[n].cp = extra[cp].cp; g_map[n].gid = extra[cp].gid; n++; }
    g_nmap = n;
    g_cm[0].format = 4;  g_cm[0].plat = 3; g_cm[0].enc = 1;  g_cm[0].n = n; g_cm[0].map = g_map;
    g_cm[1].format = 12; g_cm[1].plat = 3; g_cm[1].enc = 10; g_cm[1].n = n; g_cm[1].map = g_map;
}

static uint8_t *build_font(int which, size_t *sz)
{
    ttf_spec s;
    build_map();
    if (which == 1) ttf_style_spec(&s, "DwDraw", 700, 5, 0);
    else {
        ttf_default_spec(&s);
        if (which == 0) ttf_set_names(&s, "DwDraw", "Regular", "DwDraw", "DwDraw-Regular");
        else {
            ttf_set_names(&s, "DwTall", "Regular", "DwTall", "DwTall-Regular");
            s.hhea_asc = 3400; s.hhea_desc = -800; s.hhea_gap = 0;
            s.typo_asc = 3200; s.typo_desc = -760; s.typo_gap = 0;
            s.win_asc = 3600; s.win_desc = 900;
        }
    }
    if (which == 1) {
        int n = 0, i; const ttf_glyph *g = ttf_default_glyphs(&n);
        if (n > TTF_DEF_NGLYPHS) n = TTF_DEF_NGLYPHS;
        memcpy(g_bold, g, sizeof(ttf_glyph) * n);
        for (i = 0; i < n; i++) if (g_bold[i].adv == TTF_DEF_ADV) g_bold[i].adv = 1200;
        g_bold[TTF_GID_SPACE].adv = 600;
        s.glyphs = g_bold; s.nglyphs = n;
    }
    s.cmaps = g_cm; s.ncmaps = 2;
    return ttf_build(&s, sz);
}

int td_open(td_fx *x)
{
    size_t sz; uint8_t *b; int i;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    for (i = 0; i < 3; i++) {
        b = build_font(i, &sz);
        if (!b) return 0;
        x->key[i] = dwx_loader_add(x->fl, b, sz); free(b);
    }
    if (FAILED(dwx_loader_register(x->fl, x->f))) return 0;
    x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < 3; i++) dwx_coll_add_key(x->cl, x->key[i]);
    if (FAILED(dwx_coll_register(x->cl, x->f))) return 0;
    if (FAILED(dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) return 0;
    return 1;
}
void td_close(td_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}
IDWriteTextFormat *td_format(td_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, TD_LOC, &t))) return NULL;
    return t;
}
IDWriteTextLayout *td_layout_(td_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h)
{
    IDWriteTextFormat *t = td_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}
HRESULT td_draw(IDWriteTextLayout *l, dwx_renderer *r, void *ctx, float x, float y)
{
    return IDWriteTextLayout_Draw(l, ctx, (IDWriteTextRenderer *)r, x, y);
}
int td_count(dwx_renderer *r, int kind) { return (int)r->count[kind]; }
const dwx_draw *td_nth(dwx_renderer *r, int kind, int n)
{
    int i, k = 0;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == kind) { if (k == n) return &r->calls[i]; k++; }
    return NULL;
}
void td_face_metrics(const dwx_draw *d, DWRITE_FONT_METRICS *m)
{
    memset(m, 0, sizeof *m);
    if (d && d->face) IDWriteFontFace_GetMetrics(d->face, m);
}
float td_line_baseline(IDWriteTextLayout *l, UINT32 line)
{
    DWRITE_LINE_METRICS lm[16]; UINT32 n = 0; memset(lm, 0, sizeof lm);
    IDWriteTextLayout_GetLineMetrics(l, lm, 16, &n);
    return line < n && line < 16 ? lm[line].baseline : -1.0f;
}
float td_line_height(IDWriteTextLayout *l, UINT32 line)
{
    DWRITE_LINE_METRICS lm[16]; UINT32 n = 0; memset(lm, 0, sizeof lm);
    IDWriteTextLayout_GetLineMetrics(l, lm, 16, &n);
    return line < n && line < 16 ? lm[line].height : -1.0f;
}
UINT32 td_lines(IDWriteTextLayout *l)
{
    DWRITE_TEXT_METRICS m; memset(&m, 0, sizeof m); IDWriteTextLayout_GetMetrics(l, &m); return m.lineCount;
}
int td_face_a_advance(const dwx_draw *d)
{
    UINT32 cp = 'A'; UINT16 g = 0; DWRITE_GLYPH_METRICS gm;
    if (!d || !d->face) return -1;
    if (FAILED(IDWriteFontFace_GetGlyphIndices(d->face, &cp, 1, &g))) return -1;
    if (FAILED(IDWriteFontFace_GetDesignGlyphMetrics(d->face, &g, 1, &gm, FALSE))) return -1;
    return (int)gm.advanceWidth;
}
UINT32 td_total_glyphs(dwx_renderer *r)
{
    int i; UINT32 n = 0;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) n += r->calls[i].nglyphs;
    return n;
}

void td_get_lines(IDWriteTextLayout *l, td_lines_t *o)
{
    UINT32 i; memset(o, 0, sizeof *o);
    IDWriteTextLayout_GetLineMetrics(l, o->lm, 32, &o->n);
    if (o->n > 32) o->n = 32;
    for (i = 0; i < o->n; i++) o->top[i + 1] = o->top[i] + o->lm[i].height;
}
int td_line_of(const td_lines_t *o, UINT32 pos)
{
    UINT32 i, s = 0;
    for (i = 0; i < o->n; i++) { if (pos >= s && pos < s + o->lm[i].length) return (int)i; s += o->lm[i].length; }
    return -1;
}
float td_snap(float v, float scale, float offset)
{
    return (floorf(scale * v + offset + 0.5f) - offset) / scale;
}
const dwx_draw *td_run_at(dwx_renderer *r, UINT32 pos)
{
    int i;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && r->calls[i].has_desc && r->calls[i].text_pos == pos) return &r->calls[i];
    return NULL;
}
void td_check_indices(const dwx_draw *d, const WCHAR *src, const char *what)
{
    UINT32 cp[256], i; UINT16 want[256];
    if (!d || !d->face || d->nglyphs > 256 || d->nglyphs != d->text_len) { t_check(0, __FILE__, __LINE__, what, "run shape unsuitable for index check"); return; }
    for (i = 0; i < d->nglyphs; i++) cp[i] = src[d->text_pos + i];
    if (FAILED(IDWriteFontFace_GetGlyphIndices(d->face, cp, d->nglyphs, want))) { t_check(0, __FILE__, __LINE__, what, "GetGlyphIndices failed"); return; }
    for (i = 0; i < d->nglyphs; i++) t_check(d->indices[i] == want[i], __FILE__, __LINE__, what, "glyph %u: run has %u, GetGlyphIndices says %u", i, d->indices[i], want[i]);
}

/* ---- order-recording renderer ------------------------------------------------------------------------------ */
static void lg(td_rd *r, int type, const IID *iid, HRESULT hr, void *ctx)
{
    if (r->nlog >= 2048) return;
    r->log[r->nlog].type = type; r->log[r->nlog].hr = hr; r->log[r->nlog].ctx = ctx;
    if (iid) r->log[r->nlog].iid = *iid; else memset(&r->log[r->nlog].iid, 0, sizeof(IID));
    r->nlog++;
}
#define IN(r) ((IDWriteTextRenderer1 *)(r)->in)
static HRESULT WINAPI q_QI(IDWriteTextRenderer1 *This, REFIID riid, void **ppv)
{
    td_rd *r = (td_rd *)This; int ok = 0;
    lg(r, TE_QI, riid, 0, NULL);
    if (IsEqualIID(riid, &IID_IUnknown)) ok = 1;
    else if (IsEqualIID(riid, &IID_IDWritePixelSnapping)) ok = r->allow_pixel_snapping_qi;
    else if (IsEqualIID(riid, &IID_IDWriteTextRenderer)) ok = r->allow_renderer;
    else if (IsEqualIID(riid, &IID_IDWriteTextRenderer1)) ok = r->allow_r1;
    if (!ok) { *ppv = NULL; return E_NOINTERFACE; }
    *ppv = This; InterlockedIncrement(&r->ref); return S_OK;
}
static ULONG WINAPI q_AR(IDWriteTextRenderer1 *This) { td_rd *r = (td_rd *)This; InterlockedIncrement(&r->addref); return InterlockedIncrement(&r->ref); }
static ULONG WINAPI q_RL(IDWriteTextRenderer1 *This) { td_rd *r = (td_rd *)This; InterlockedIncrement(&r->release); return InterlockedDecrement(&r->ref); }
static HRESULT WINAPI q_Snap(IDWriteTextRenderer1 *This, void *c, BOOL *v) { td_rd *r = (td_rd *)This; HRESULT hr = IN(r)->lpVtbl->IsPixelSnappingDisabled(IN(r), c, v); lg(r, TE_SNAP, NULL, hr, c); return hr; }
static HRESULT WINAPI q_Xf(IDWriteTextRenderer1 *This, void *c, DWRITE_MATRIX *m) { td_rd *r = (td_rd *)This; HRESULT hr = IN(r)->lpVtbl->GetCurrentTransform(IN(r), c, m); lg(r, TE_XFORM, NULL, hr, c); return hr; }
static HRESULT WINAPI q_Ppd(IDWriteTextRenderer1 *This, void *c, FLOAT *v) { td_rd *r = (td_rd *)This; HRESULT hr = IN(r)->lpVtbl->GetPixelsPerDip(IN(r), c, v); lg(r, TE_PPD, NULL, hr, c); return hr; }
static HRESULT WINAPI q_Glyph(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mm, const DWRITE_GLYPH_RUN *g, const DWRITE_GLYPH_RUN_DESCRIPTION *d, IUnknown *fx)
{ td_rd *r = (td_rd *)This; lg(r, TE_GLYPH, NULL, 0, c); return IN(r)->lpVtbl->DrawGlyphRun(IN(r), c, x, y, mm, g, d, fx); }
static HRESULT WINAPI q_Under(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, const DWRITE_UNDERLINE *u, IUnknown *fx)
{ td_rd *r = (td_rd *)This; lg(r, TE_UNDER, NULL, 0, c); return IN(r)->lpVtbl->DrawUnderline(IN(r), c, x, y, u, fx); }
static HRESULT WINAPI q_Strike(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH *s, IUnknown *fx)
{ td_rd *r = (td_rd *)This; lg(r, TE_STRIKE, NULL, 0, c); return IN(r)->lpVtbl->DrawStrikethrough(IN(r), c, x, y, s, fx); }
static HRESULT WINAPI q_Inline(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, IDWriteInlineObject *o, BOOL sw, BOOL rtl, IUnknown *fx)
{ td_rd *r = (td_rd *)This; lg(r, TE_INLINE, NULL, 0, c); return IN(r)->lpVtbl->DrawInlineObject(IN(r), c, x, y, o, sw, rtl, fx); }
static HRESULT WINAPI q1_Glyph(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, DWRITE_MEASURING_MODE mm, const DWRITE_GLYPH_RUN *g, const DWRITE_GLYPH_RUN_DESCRIPTION *d, IUnknown *fx)
{ td_rd *r = (td_rd *)This; lg(r, TE_GLYPH, NULL, 0, c); return (IN(r)->lpVtbl->IDWriteTextRenderer1_DrawGlyphRun)(IN(r), c, x, y, a, mm, g, d, fx); }
static HRESULT WINAPI q1_Under(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, const DWRITE_UNDERLINE *u, IUnknown *fx)
{ td_rd *r = (td_rd *)This; lg(r, TE_UNDER, NULL, 0, c); return (IN(r)->lpVtbl->IDWriteTextRenderer1_DrawUnderline)(IN(r), c, x, y, a, u, fx); }
static HRESULT WINAPI q1_Strike(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, const DWRITE_STRIKETHROUGH *s, IUnknown *fx)
{ td_rd *r = (td_rd *)This; lg(r, TE_STRIKE, NULL, 0, c); return (IN(r)->lpVtbl->IDWriteTextRenderer1_DrawStrikethrough)(IN(r), c, x, y, a, s, fx); }
static HRESULT WINAPI q1_Inline(IDWriteTextRenderer1 *This, void *c, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, IDWriteInlineObject *o, BOOL sw, BOOL rtl, IUnknown *fx)
{ td_rd *r = (td_rd *)This; lg(r, TE_INLINE, NULL, 0, c); return (IN(r)->lpVtbl->IDWriteTextRenderer1_DrawInlineObject)(IN(r), c, x, y, a, o, sw, rtl, fx); }
static const IDWriteTextRenderer1Vtbl q_vt = { q_QI, q_AR, q_RL, q_Snap, q_Xf, q_Ppd, q_Glyph, q_Under, q_Strike, q_Inline, q1_Glyph, q1_Under, q1_Strike, q1_Inline };

td_rd *td_rd_new(void)
{
    td_rd *r = (td_rd *)calloc(1, sizeof *r);
    r->lpVtbl = &q_vt; r->ref = 1; r->in = dwx_renderer_new();
    r->allow_pixel_snapping_qi = 1; r->allow_renderer = 1; r->allow_r1 = 0;
    return r;
}
void td_rd_free(td_rd *r) { if (!r) return; dwx_release(r->in); free(r); }
int td_rd_count(const td_rd *r, int type) { int i, n = 0; for (i = 0; i < r->nlog; i++) if (r->log[i].type == type) n++; return n; }
int td_rd_first(const td_rd *r, int type) { int i; for (i = 0; i < r->nlog; i++) if (r->log[i].type == type) return i; return -1; }
int td_rd_qi_asked(const td_rd *r, const IID *iid) { int i, n = 0; for (i = 0; i < r->nlog; i++) if (r->log[i].type == TE_QI && IsEqualIID(&r->log[i].iid, iid)) n++; return n; }
