#include "tlm.h"
#include <stdlib.h>

static ttf_glyph g_uglyph[96];
static ttf_map g_umap[512]; static int g_umapn;
static ttf_cmap g_ucmap[2];
static ttf_map g_bmp[512]; static int g_bmpn;

static void umap(unsigned cp, unsigned gid) { g_umap[g_umapn].cp = cp; g_umap[g_umapn].gid = gid; g_umapn++; }
static int cmp_map(const void *a, const void *b) { unsigned x = ((const ttf_map *)a)->cp, y = ((const ttf_map *)b)->cp; return x < y ? -1 : x > y; }

/* extra glyph ids of DwUni */
enum { G_MARK = 69, G_WIDE, G_SHY, G_ZWJ, G_ASTRAL, G_HYPHEN, G_PUNCT, G_NGLYPHS };

static void build_uni(ttf_spec *s)
{
    int n = 0, i; const ttf_glyph *g = ttf_default_glyphs(&n);
    unsigned cp;
    memcpy(g_uglyph, g, sizeof(ttf_glyph) * n);
    g_uglyph[G_MARK] = g[2];   g_uglyph[G_MARK].adv = 0;
    g_uglyph[G_WIDE] = g[2];   g_uglyph[G_WIDE].adv = 2000;
    g_uglyph[G_SHY] = g[2];    g_uglyph[G_SHY].adv = 500;
    g_uglyph[G_ZWJ] = g[67];   g_uglyph[G_ZWJ].adv = 0;
    g_uglyph[G_ASTRAL] = g[2]; g_uglyph[G_ASTRAL].adv = 1500;
    g_uglyph[G_HYPHEN] = g[2]; g_uglyph[G_HYPHEN].adv = 600;
    g_uglyph[G_PUNCT] = g[2];  g_uglyph[G_PUNCT].adv = 500;
    g_umapn = 0;
    umap(0x20, 1);
    for (cp = '0'; cp <= '9'; cp++) umap(cp, ttf_default_gid(cp));
    for (cp = 'A'; cp <= 'Z'; cp++) umap(cp, ttf_default_gid(cp));
    for (cp = 'a'; cp <= 'z'; cp++) umap(cp, ttf_default_gid(cp));
    { static const unsigned punct[] = { '.', ',', '!', '?', ';', ':', ')', '(', '\'', '"', '/', '_' };
      for (i = 0; i < (int)(sizeof punct / sizeof punct[0]); i++) umap(punct[i], G_PUNCT); }
    umap('-', G_HYPHEN); umap(0x2010, G_HYPHEN); umap(0x2011, G_HYPHEN);
    umap(0xA0, 1);                                   /* no-break space shares the space glyph */
    umap(0xAD, G_SHY);                               /* soft hyphen */
    umap(0x200D, G_ZWJ); umap(0x200B, G_ZWJ);        /* ZWJ, ZWSP */
    for (cp = 0x300; cp <= 0x30F; cp++) umap(cp, G_MARK);      /* combining diacritics */
    for (cp = 0x4E00; cp <= 0x4E0F; cp++) umap(cp, G_WIDE);   /* CJK ideographs */
    for (cp = 0x3041; cp <= 0x3050; cp++) umap(cp, G_WIDE);   /* hiragana */
    umap(0xAC00, G_WIDE); umap(0xAC01, G_WIDE); umap(0x3000, G_WIDE);
    for (cp = 0x5D0; cp <= 0x5DA; cp++) umap(cp, 2);          /* Hebrew letters (RTL) */
    umap(0x1100, 2); umap(0x1112, 2); umap(0x1161, 2); umap(0x1175, 2); umap(0x11A8, 2); umap(0x11AB, 2);   /* Hangul jamo */
    for (cp = 0x1D400; cp <= 0x1D40F; cp++) umap(cp, G_ASTRAL);  /* math bold capitals (surrogate pairs) */
    umap(0x20000, G_WIDE); umap(0x20001, G_WIDE);                  /* CJK extension B */
    umap(0x1F600, G_ASTRAL);                                       /* emoji code point, plain glyph */
    qsort(g_umap, g_umapn, sizeof g_umap[0], cmp_map);
    g_bmpn = 0;
    for (i = 0; i < g_umapn; i++) if (g_umap[i].cp < 0x10000) g_bmp[g_bmpn++] = g_umap[i];
    g_ucmap[0].format = 4;  g_ucmap[0].plat = 3; g_ucmap[0].enc = 1;  g_ucmap[0].n = g_bmpn;  g_ucmap[0].map = g_bmp;
    g_ucmap[1].format = 12; g_ucmap[1].plat = 3; g_ucmap[1].enc = 10; g_ucmap[1].n = g_umapn; g_ucmap[1].map = g_umap;
    s->glyphs = g_uglyph; s->nglyphs = G_NGLYPHS; s->cmaps = g_ucmap; s->ncmaps = 2;
}

static uint8_t *build_family(int which, size_t *sz)
{
    ttf_spec s; char ps[96];
    static const char *names[4] = { "DwTest", "DwGap", "DwAsym", "DwUni" };
    ttf_default_spec(&s);
    snprintf(ps, sizeof ps, "%s-Regular", names[which]);
    ttf_set_names(&s, names[which], "Regular", names[which], ps);
    if (which == 1) s.hhea_gap = 1000;                            /* hhea sum 3100 > win sum 2250 => positive derived lineGap */
    if (which == 2) { s.win_asc = 1500; s.win_desc = 700; s.hhea_gap = 0; s.hhea_asc = 1500; s.hhea_desc = -700; }
    if (which == 3) build_uni(&s);
    return ttf_build(&s, sz);
}

int tm_open(tm_fx *x)
{
    size_t sz; uint8_t *b; int i;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    for (i = 0; i < 4; i++) {
        b = build_family(i, &sz);
        if (!b) return 0;
        x->key[i] = dwx_loader_add(x->fl, b, sz); free(b);
    }
    if (FAILED(dwx_loader_register(x->fl, x->f))) return 0;
    x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < 4; i++) dwx_coll_add_key(x->cl, x->key[i]);
    if (FAILED(dwx_coll_register(x->cl, x->f))) return 0;
    if (FAILED(dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) return 0;
    return 1;
}

void tm_close(tm_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}

IDWriteTextFormat *tm_format(tm_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, LOC, &t))) return NULL;
    return t;
}

IDWriteTextLayout *tm_layout_(tm_fx *x, const WCHAR *family, const WCHAR *text, UINT32 len, float size, float w, float h)
{
    IDWriteTextFormat *t = tm_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, len, t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

int tm_get_lines(IDWriteTextLayout *l, tm_lines *o)
{
    HRESULT hr; UINT32 n = 99;
    memset(o, 0xcc, sizeof *o); o->n = 0;
    hr = IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n);
    t_check(hr == DW_E_NOT_SUFFICIENT_BUFFER || hr == S_OK, __FILE__, __LINE__, "GetLineMetrics probe", "got %s", dw_hr_name(hr));
    if (n == 0 || n > MAXL) return 0;
    hr = IDWriteTextLayout_GetLineMetrics(l, o->v, n, &o->n);
    t_check(hr == S_OK && o->n == n, __FILE__, __LINE__, "GetLineMetrics exact", "got %s n=%u/%u", dw_hr_name(hr), o->n, n);
    return hr == S_OK;
}
int tm_get_clusters(IDWriteTextLayout *l, tm_clusters *o)
{
    HRESULT hr; UINT32 n = 99;
    memset(o, 0xcc, sizeof *o); o->n = 0;
    hr = IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &n);
    t_check(hr == DW_E_NOT_SUFFICIENT_BUFFER || hr == S_OK, __FILE__, __LINE__, "GetClusterMetrics probe", "got %s", dw_hr_name(hr));
    if (n == 0) { o->n = 0; return 1; }
    if (n > MAXL * 4) return 0;
    hr = IDWriteTextLayout_GetClusterMetrics(l, o->v, n, &o->n);
    t_check(hr == S_OK && o->n == n, __FILE__, __LINE__, "GetClusterMetrics exact", "got %s n=%u/%u", dw_hr_name(hr), o->n, n);
    return hr == S_OK;
}
void tm_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m)
{
    HRESULT hr;
    memset(m, 0xcc, sizeof *m);
    hr = IDWriteTextLayout_GetMetrics(l, m);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetMetrics", "got %s", dw_hr_name(hr));
}

int tm_font_metrics(tm_fx *x, const WCHAR *family, DWRITE_FONT_METRICS *m)
{
    UINT32 idx = 0; BOOL ex = FALSE; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; int ok = 0;
    memset(m, 0, sizeof *m);
    if (FAILED(IDWriteFontCollection_FindFamilyName(x->coll, family, &idx, &ex)) || !ex) return 0;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(x->coll, idx, &fam)) && fam &&
        SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font)) && font) {
        IDWriteFont_GetMetrics(font, m); ok = 1;
    }
    DW_RELEASE(font); DW_RELEASE(fam);
    return ok;
}

void *tm_qi(IUnknown *u, const IID *iid, const char *name)
{
    void *p = NULL;
    HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    if (FAILED(hr) || !p) { t_skip("QueryInterface(%s) -> %s", name, dw_hr_name(hr)); return NULL; }
    return p;
}

float tm_sum_cluster_width(const tm_clusters *c) { float s = 0; UINT32 i; for (i = 0; i < c->n; i++) s += c->v[i].width; return s; }
UINT32 tm_sum_cluster_len(const tm_clusters *c) { UINT32 s = 0, i; for (i = 0; i < c->n; i++) s += c->v[i].length; return s; }
UINT32 tm_sum_line_len(const tm_lines *l) { UINT32 s = 0, i; for (i = 0; i < l->n; i++) s += l->v[i].length; return s; }
float tm_sum_line_height(const tm_lines *l) { float s = 0; UINT32 i; for (i = 0; i < l->n; i++) s += l->v[i].height; return s; }
