#include "bx.h"

/* ---- synthetic fonts ----------------------------------------------------------------------------------------- */
static ttf_map g_map[3000]; static int g_nmap;
static void add(unsigned cp, unsigned gid) { g_map[g_nmap].cp = cp; g_map[g_nmap].gid = gid; g_nmap++; }
static int cmp_map(const void *a, const void *b) { unsigned x = ((const ttf_map *)a)->cp, y = ((const ttf_map *)b)->cp; return x < y ? -1 : x > y; }

static void build_map(void)
{
    unsigned c, i;
    g_nmap = 0;
    add(0x20, 1); add(0x09, 1); add(0xa0, 1);
    for (c = 0x21; c < 0x7f; c++) {
        if (c >= '0' && c <= '9') add(c, 54 + (c - '0'));
        else if (c >= 'A' && c <= 'Z') add(c, 2 + (c - 'A'));
        else if (c >= 'a' && c <= 'z') add(c, 28 + (c - 'a'));
        else add(c, 2);
    }
    for (c = 0x300; c <= 0x36f; c++) add(c, 69);                /* combining marks: zero advance */
    for (c = 0x591; c <= 0x5bd; c++) add(c, 69);                /* Hebrew points */
    for (i = 0; i < 27; i++) add(0x5d0 + i, 2 + i);             /* aleph = gid 2 ('A'), bet = gid 3 ('B') ... */
    for (c = 0x621; c <= 0x63a; c++) add(c, 2 + (c - 0x621));
    for (c = 0x641; c <= 0x64a; c++) add(c, 2 + (c - 0x641));
    for (c = 0x64b; c <= 0x652; c++) add(c, 69);                /* Arabic marks */
    for (i = 0; i < 10; i++) { add(0x660 + i, 54 + i); add(0x6f0 + i, 54 + i); }
    add(0x200c, 69); add(0x200d, 69); add(0x200e, 69); add(0x200f, 69); add(0x2060, 69);
    add(0x2013, 2); add(0x2014, 2); add(0x2026, 2);
    for (i = 0; i < 16; i++) { add(0x4e00 + i, 2 + i); add(0x3040 + i, 2 + i); }
    for (i = 0; i < 16; i++) { add(0x10000 + i, 2 + i); add(0x10800 + i, 2 + i); }
    for (c = 0x1f600; c < 0x1f650; c++) add(c, 64);
    for (c = 0x1f3fb; c <= 0x1f3ff; c++) add(c, 64);
    add(0x1f466, 64); add(0x1f467, 64); add(0x1f468, 64); add(0x1f469, 64);
    qsort(g_map, g_nmap, sizeof g_map[0], cmp_map);
}

static uint8_t *build_family(const char *family, int kern, size_t *sz)
{
    static ttf_glyph gl[TTF_DEF_NGLYPHS + 1];
    static ttf_cmap cm[2];
    static const ttf_kernpair kp[] = { { 2, 2, -64 }, { 2, 3, -128 }, { 2, 23, -256 }, { 23, 2, -192 } };
    ttf_spec s; char ps[96]; int n = 0, bmp; const ttf_glyph *g = ttf_default_glyphs(&n);
    ttf_default_spec(&s);
    snprintf(ps, sizeof ps, "%s-Regular", family);
    ttf_set_names(&s, family, "Regular", family, ps);
    memcpy(gl, g, sizeof(ttf_glyph) * n);
    gl[n].adv = 0; n++;                                              /* gid 69: zero-advance empty glyph */
    build_map();
    bmp = 0; while (bmp < g_nmap && g_map[bmp].cp <= 0xFFFF) bmp++;
    cm[0].format = 4;  cm[0].plat = 3; cm[0].enc = 1;  cm[0].map = g_map; cm[0].n = bmp;
    cm[1].format = 12; cm[1].plat = 3; cm[1].enc = 10; cm[1].map = g_map; cm[1].n = g_nmap;
    s.glyphs = gl; s.nglyphs = n; s.cmaps = cm; s.ncmaps = 2;
    if (kern) { s.kern = kp; s.nkern = 4; }
    return ttf_build(&s, sz);
}

int bx_open(bx_fx *x)
{
    size_t sz; uint8_t *b; int i;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    for (i = 0; i < 2; i++) {
        b = build_family(i ? "DwBidiKern" : "DwBidi", i, &sz);
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

void bx_close(bx_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}

IDWriteTextFormat *bx_format(bx_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, BX_LOC, &t))) return NULL;
    return t;
}

IDWriteTextLayout *bx_layout_(bx_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h)
{
    IDWriteTextFormat *t = bx_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

/* ---- call wrappers -------------------------------------------------------------------------------------------- */
bx_pt bx_point(IDWriteTextLayout *l, float x, float y)
{
    bx_pt r;
    memset(&r, 0xcc, sizeof r);
    r.hr = IDWriteTextLayout_HitTestPoint(l, x, y, &r.tr, &r.in, &r.m);
    t_check(r.hr == S_OK, __FILE__, __LINE__, "HitTestPoint", "got %s", dw_hr_name(r.hr));
    return r;
}
bx_tp bx_pos(IDWriteTextLayout *l, UINT32 pos, BOOL trailing)
{
    bx_tp r;
    memset(&r, 0xcc, sizeof r);
    r.x = 1e30f; r.y = 1e30f;
    r.hr = IDWriteTextLayout_HitTestTextPosition(l, pos, trailing, &r.x, &r.y, &r.m);
    t_check(r.hr == S_OK, __FILE__, __LINE__, "HitTestTextPosition", "pos %u got %s", pos, dw_hr_name(r.hr));
    return r;
}
bx_rg bx_range(IDWriteTextLayout *l, UINT32 pos, UINT32 len)
{
    bx_rg r;
    memset(&r, 0xcc, sizeof r);
    r.count = 0xdeadbeef;
    r.hr = IDWriteTextLayout_HitTestTextRange(l, pos, len, 0.0f, 0.0f, r.m, BX_MAXR, &r.count);
    t_check(r.hr == S_OK, __FILE__, __LINE__, "HitTestTextRange", "(%u,%u) got %s", pos, len, dw_hr_name(r.hr));
    if (r.hr != S_OK || r.count > BX_MAXR) r.count = 0;
    return r;
}
void bx_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m)
{
    HRESULT hr;
    memset(m, 0xcc, sizeof *m);
    hr = IDWriteTextLayout_GetMetrics(l, m);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetMetrics", "got %s", dw_hr_name(hr));
}
UINT32 bx_clusters(IDWriteTextLayout *l, DWRITE_CLUSTER_METRICS *c, UINT32 cap)
{
    UINT32 n = 0;
    HRESULT hr = IDWriteTextLayout_GetClusterMetrics(l, c, cap, &n);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetClusterMetrics", "got %s", dw_hr_name(hr));
    return hr == S_OK ? n : 0;
}
UINT32 bx_lines(IDWriteTextLayout *l, DWRITE_LINE_METRICS *lm, UINT32 cap)
{
    UINT32 n = 0;
    HRESULT hr = IDWriteTextLayout_GetLineMetrics(l, lm, cap, &n);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetLineMetrics", "got %s", dw_hr_name(hr));
    return hr == S_OK ? n : 0;
}

dwx_renderer *bx_draw(IDWriteTextLayout *l, float ox, float oy)
{
    dwx_renderer *r = dwx_renderer_new();
    HRESULT hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, ox, oy);
    t_check(hr == S_OK, __FILE__, __LINE__, "Draw", "got %s", dw_hr_name(hr));
    if (hr != S_OK) { dwx_release(r); return NULL; }
    return r;
}

int bx_draw_levels(dwx_renderer *r, UINT32 len, UINT8 *lev)
{
    int i, ok = 1; UINT32 k;
    for (k = 0; k < len; k++) lev[k] = 0xff;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *d = &r->calls[i];
        if (d->kind != DWX_DRAW_GLYPHRUN) continue;
        if (!d->has_desc) { ok = 0; continue; }
        for (k = d->text_pos; k < d->text_pos + d->text_len; k++) {
            if (k >= len) { ok = 0; continue; }
            if (lev[k] != 0xff) ok = 0;
            lev[k] = d->bidi;
        }
    }
    for (k = 0; k < len; k++) if (lev[k] == 0xff) ok = 0;
    return ok;
}

/* ---- reference bidi (fixture alphabet only) ------------------------------------------------------------------- */
enum { T_L, T_R, T_AL, T_EN, T_AN, T_ES, T_ET, T_CS, T_NSM, T_ON, T_WS };
static int bidi_class(WCHAR c)
{
    if (c == ' ' || c == '\t' || c == 0xa0) return T_WS;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return T_L;
    if (c >= '0' && c <= '9') return T_EN;
    if (c >= 0x5d0 && c <= 0x5ea) return T_R;
    if (c == 0x200f) return T_R;
    if (c == 0x200e) return T_L;
    if ((c >= 0x621 && c <= 0x64a)) return T_AL;
    if (c >= 0x660 && c <= 0x669) return T_AN;
    if (c >= 0x6f0 && c <= 0x6f9) return T_EN;
    if ((c >= 0x300 && c <= 0x36f) || (c >= 0x591 && c <= 0x5bd) || (c >= 0x64b && c <= 0x652)) return T_NSM;
    if (c == '+' || c == '-') return T_ES;
    if (c == '$' || c == '%' || c == '#') return T_ET;
    if (c == ',' || c == '.' || c == ':' || c == '/') return T_CS;
    if ((c >= 0x4e00 && c <= 0x4e0f) || (c >= 0x3040 && c <= 0x304f)) return T_L;
    return T_ON;
}

void bx_ref_bidi(const WCHAR *t, UINT32 n, int base, UINT8 *lev, UINT32 *vis)
{
    int ty[256]; UINT32 i, j; int sos = base ? T_R : T_L, maxl = 0, minodd = 255, lv;
    if (n > 256) n = 256;
    for (i = 0; i < n; i++) ty[i] = bidi_class(t[i]);
    for (i = 0; i < n; i++) if (ty[i] == T_NSM) ty[i] = i ? ty[i - 1] : sos;                         /* W1 */
    for (i = 0; i < n; i++) if (ty[i] == T_EN) {                                                     /* W2 */
        int s = sos; for (j = i; j-- > 0;) if (ty[j] == T_L || ty[j] == T_R || ty[j] == T_AL) { s = ty[j]; break; }
        if (s == T_AL) ty[i] = T_AN;
    }
    for (i = 0; i < n; i++) if (ty[i] == T_AL) ty[i] = T_R;                                          /* W3 */
    for (i = 1; i + 1 < n; i++) {                                                                    /* W4 */
        if (ty[i] == T_ES && ty[i - 1] == T_EN && ty[i + 1] == T_EN) ty[i] = T_EN;
        else if (ty[i] == T_CS && ty[i - 1] == ty[i + 1] && (ty[i - 1] == T_EN || ty[i - 1] == T_AN)) ty[i] = ty[i - 1];
    }
    for (i = 0; i < n; i++) if (ty[i] == T_ET) {                                                     /* W5 */
        UINT32 a = i, b = i; int en = 0;
        while (b < n && ty[b] == T_ET) b++;
        if (a > 0 && ty[a - 1] == T_EN) en = 1;
        if (b < n && ty[b] == T_EN) en = 1;
        if (en) for (j = a; j < b; j++) ty[j] = T_EN;
        i = b - 1;
    }
    for (i = 0; i < n; i++) if (ty[i] == T_ES || ty[i] == T_ET || ty[i] == T_CS) ty[i] = T_ON;       /* W6 */
    for (i = 0; i < n; i++) if (ty[i] == T_EN) {                                                     /* W7 */
        int s = sos; for (j = i; j-- > 0;) if (ty[j] == T_L || ty[j] == T_R) { s = ty[j]; break; }
        if (s == T_L) ty[i] = T_L;
    }
    for (i = 0; i < n; i++) if (ty[i] == T_ON || ty[i] == T_WS) {                                    /* N1 / N2 */
        UINT32 a = i, b = i; int pre, post, e = base ? T_R : T_L;
        while (b < n && (ty[b] == T_ON || ty[b] == T_WS)) b++;
        pre = a ? ty[a - 1] : sos; post = b < n ? ty[b] : sos;
        if (pre == T_EN || pre == T_AN) pre = T_R;
        if (post == T_EN || post == T_AN) post = T_R;
        for (j = a; j < b; j++) ty[j] = (pre == post) ? pre : e;
        i = b - 1;
    }
    for (i = 0; i < n; i++) {                                                                        /* I1 / I2 */
        lv = base;
        if (!(base & 1)) { if (ty[i] == T_R) lv += 1; else if (ty[i] == T_AN || ty[i] == T_EN) lv += 2; }
        else { if (ty[i] == T_L || ty[i] == T_EN || ty[i] == T_AN) lv += 1; }
        lev[i] = (UINT8)lv;
        if (lv > maxl) maxl = lv;
        if ((lv & 1) && lv < minodd) minodd = lv;
        vis[i] = i;
    }
    for (lv = maxl; lv >= minodd && lv >= 1; lv--) {                                                 /* L2 */
        for (i = 0; i < n; i++) if (lev[vis[i]] >= lv) {
            UINT32 a = i, b = i;
            while (b < n && lev[vis[b]] >= lv) b++;
            for (j = 0; j < (b - a) / 2; j++) { UINT32 tmp = vis[a + j]; vis[a + j] = vis[b - 1 - j]; vis[b - 1 - j] = tmp; }
            i = b - 1;
        }
    }
}
