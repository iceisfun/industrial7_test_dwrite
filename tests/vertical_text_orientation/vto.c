#include "vto.h"

static ttf_map g_map[256]; static int g_nmap;
static ttf_cmap g_cm[2];
static ttf_glyph g_gl[TTF_DEF_NGLYPHS];
static const ttf_sub1 g_vert[] = { { 2, 64 }, { 3, 65 } };
static const ttf_feature g_gsub[] = { { "vert", TTF_LK_SINGLE, 2, g_vert } };

static void build_map(void)
{
    unsigned cp; int n = 0, i;
    static const struct { unsigned cp, gid; } extra[] = {
        { 0x4e00, 2 }, { 0x4e01, 3 }, { 0x4e02, 4 }, { 0x4e03, 5 }, { 0x3042, 28 }, { 0x3044, 29 }, { 0x3000, 1 },
        { 0x22ee, TTF_GID_TRIANGLE }, { 0x2026, TTF_GID_ROUND },
    };
    if (g_nmap) return;
    for (cp = 0x20; cp < 0x7f; cp++) { unsigned g = ttf_default_gid(cp); if (g) { g_map[n].cp = cp; g_map[n].gid = g; n++; } }
    for (i = 0; i < (int)(sizeof extra / sizeof extra[0]); i++) { g_map[n].cp = extra[i].cp; g_map[n].gid = extra[i].gid; n++; }
    g_nmap = n;
    g_cm[0].format = 4;  g_cm[0].plat = 3; g_cm[0].enc = 1;  g_cm[0].n = n; g_cm[0].map = g_map;
    g_cm[1].format = 12; g_cm[1].plat = 3; g_cm[1].enc = 10; g_cm[1].n = n; g_cm[1].map = g_map;
}

static uint8_t *build_font(int which, size_t *sz)
{
    ttf_spec s; int n = 0, i; const ttf_glyph *g = ttf_default_glyphs(&n);
    build_map();
    ttf_default_spec(&s);
    if (which == 0) ttf_set_names(&s, "DwVert", "Regular", "DwVert", "DwVert-Regular");
    else if (which == 1) ttf_set_names(&s, "DwVertSub", "Regular", "DwVertSub", "DwVertSub-Regular");
    else ttf_set_names(&s, "DwPlain", "Regular", "DwPlain", "DwPlain-Regular");
    s.cmaps = g_cm; s.ncmaps = 2;
    if (which != 2) {
        if (n > TTF_DEF_NGLYPHS) n = TTF_DEF_NGLYPHS;
        memcpy(g_gl, g, sizeof(ttf_glyph) * n);
        for (i = 0; i < n; i++) g_gl[i].vadv = (i == TTF_GID_SPACE) ? VT_VADV_SPACE : (i == 0 ? 1700 : 1500);
        s.glyphs = g_gl; s.nglyphs = n;
        s.vertical = 1; s.v_ascent = 1500; s.v_descent = -548;
    }
    if (which == 1) { s.gsub = g_gsub; s.ngsub = 1; }
    return ttf_build(&s, sz);
}

int vt_open(vt_fx *x)
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
void vt_close(vt_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}
IDWriteTextFormat *vt_format(vt_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, VT_LOC, &t))) return NULL;
    return t;
}
IDWriteTextLayout *vt_layout_(vt_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h, int reading, int flow)
{
    IDWriteTextFormat *t = vt_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    if (!l) return NULL;
    if (FAILED(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)reading)) ||
        FAILED(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)flow))) { IDWriteTextLayout_Release(l); return NULL; }
    return l;
}
dwx_renderer *vt_r1(void) { dwx_renderer *r = dwx_renderer_new(); r->support_r1 = 1; r->snapping_disabled = TRUE; return r; }
dwx_renderer *vt_r0(void) { dwx_renderer *r = dwx_renderer_new(); r->snapping_disabled = TRUE; return r; }
HRESULT vt_draw(IDWriteTextLayout *l, dwx_renderer *r, float ox, float oy)
{
    return IDWriteTextLayout_Draw(l, (void *)(ULONG_PTR)0x1234abcd, (IDWriteTextRenderer *)r, ox, oy);
}
const dwx_draw *vt_run(dwx_renderer *r, int nth)
{
    int i, k = 0;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) { if (k == nth) return &r->calls[i]; k++; }
    return NULL;
}
int vt_runs(dwx_renderer *r) { return (int)r->count[DWX_DRAW_GLYPHRUN]; }
void vt_get_lines(IDWriteTextLayout *l, vt_lines *o)
{
    memset(o, 0, sizeof *o);
    IDWriteTextLayout_GetLineMetrics(l, o->lm, 64, &o->n);
}
float vt_sum_heights(const vt_lines *o) { float s = 0; UINT32 i; for (i = 0; i < o->n && i < 64; i++) s += o->lm[i].height; return s; }
DWRITE_TEXT_METRICS vt_metrics(IDWriteTextLayout *l)
{
    DWRITE_TEXT_METRICS m; memset(&m, 0, sizeof m);
    IDWriteTextLayout_GetMetrics(l, &m);
    return m;
}
float vt_runw(const dwx_draw *d) { return d ? dwx_run_width(d) : -1.0f; }
