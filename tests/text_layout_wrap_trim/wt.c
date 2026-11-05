#include "wt.h"

static ttf_glyph g_glyphs[TTF_DEF_NGLYPHS + 1];
static ttf_map g_map[512];
static int g_nmap;
static ttf_cmap g_cmaps[2];

static void add_map(unsigned cp, unsigned gid) { if (g_nmap < 512) { g_map[g_nmap].cp = cp; g_map[g_nmap].gid = gid; g_nmap++; } }
static int cmp_map(const void *a, const void *b) { return (int)((const ttf_map *)a)->cp - (int)((const ttf_map *)b)->cp; }

static uint8_t *build_font(size_t *sz)
{
    ttf_spec s; int n = 0, i; const ttf_glyph *g = ttf_default_glyphs(&n);
    ttf_default_spec(&s);
    ttf_set_names(&s, "DwWrap", "Regular", "DwWrap", "DwWrap-Regular");
    memcpy(g_glyphs, g, sizeof(ttf_glyph) * TTF_DEF_NGLYPHS);
    memset(&g_glyphs[TTF_DEF_NGLYPHS], 0, sizeof(ttf_glyph));      /* gid 69: empty, advance 0 */
    s.glyphs = g_glyphs; s.nglyphs = TTF_DEF_NGLYPHS + 1;
    g_nmap = 0;
    add_map(0x20, 1);
    for (i = 0; i < 26; i++) { add_map('A' + i, 2 + i); add_map('a' + i, 28 + i); }
    for (i = 0; i < 10; i++) add_map('0' + i, 54 + i);
    for (i = 0x21; i <= 0x2f; i++) add_map(i, 2);
    for (i = 0x3a; i <= 0x40; i++) add_map(i, 2);
    for (i = 0x5b; i <= 0x60; i++) add_map(i, 2);
    for (i = 0x7b; i <= 0x7e; i++) add_map(i, 2);
    add_map(0x09, 69); add_map(0x0a, 69); add_map(0x0b, 69); add_map(0x0c, 69); add_map(0x0d, 69); add_map(0x85, 69);
    add_map(0xa0, 1); add_map(0xa1, 2); add_map(0xab, 2); add_map(0xbb, 2);
    add_map(0x200b, 69); add_map(0x200c, 69); add_map(0x200d, 69); add_map(0x2060, 69); add_map(0x2028, 69); add_map(0x2029, 69);
    for (i = 0x2010; i <= 0x2015; i++) add_map(i, 2);
    add_map(0x2018, 2); add_map(0x2019, 2); add_map(0x201c, 2); add_map(0x201d, 2); add_map(0x2026, 2); add_map(0x2022, 2);
    add_map(0x3000, 1); add_map(0x3001, 2); add_map(0x3002, 2); add_map(0x300c, 2); add_map(0x300d, 2);
    add_map(0x3042, 2); add_map(0x3044, 2); add_map(0x30a2, 2); add_map(0x30fc, 2);
    add_map(0xff01, 2); add_map(0xff08, 2); add_map(0xff09, 2); add_map(0xff0c, 2); add_map(0xff0e, 2);
    for (i = 0x4e00; i <= 0x4e1f; i++) add_map(i, 2);
    for (i = 0x05d0; i <= 0x05ea; i++) add_map(i, 2);
    qsort(g_map, g_nmap, sizeof g_map[0], cmp_map);
    g_cmaps[0].format = 4;  g_cmaps[0].plat = 3; g_cmaps[0].enc = 1;  g_cmaps[0].n = g_nmap; g_cmaps[0].map = g_map;
    g_cmaps[1].format = 12; g_cmaps[1].plat = 3; g_cmaps[1].enc = 10; g_cmaps[1].n = g_nmap; g_cmaps[1].map = g_map;
    s.cmaps = g_cmaps; s.ncmaps = 2;
    return ttf_build(&s, sz);
}

int wt_open(wt_fx *x)
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

void wt_close(wt_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}

IDWriteTextFormat *wt_format(wt_fx *x, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, WT_FAM, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, WT_LOC, &t))) return NULL;
    return t;
}

IDWriteTextLayout *wt_layout_(wt_fx *x, const WCHAR *text, float width, float height, float size)
{
    IDWriteTextFormat *t = wt_format(x, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, width, height, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

IDWriteTextLayout *wt_layout_wrap(wt_fx *x, const WCHAR *text, float width, DWRITE_WORD_WRAPPING w)
{
    IDWriteTextLayout *l = wt_layout_(x, text, width, WT_BIGH, WT_SIZE);
    if (l && FAILED(IDWriteTextLayout_SetWordWrapping(l, w))) { IDWriteTextLayout_Release(l); l = NULL; }
    return l;
}

HRESULT wt_get_lines(IDWriteTextLayout *l, wt_lines *o)
{
    UINT32 n = 0, i, pos = 0; HRESULT hr;
    memset(o, 0, sizeof *o);
    hr = IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n);
    if (hr != DW_E_NOT_SUFFICIENT_BUFFER && hr != S_OK) return hr;
    if (n > 128) n = 128;
    hr = IDWriteTextLayout_GetLineMetrics(l, o->lm, n, &o->n);
    for (i = 0; SUCCEEDED(hr) && i < o->n; i++) { o->start[i] = pos; pos += o->lm[i].length; }
    return hr;
}
UINT32 wt_nlines(IDWriteTextLayout *l) { wt_lines o; if (FAILED(wt_get_lines(l, &o))) return 0; return o.n; }

void wt_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m)
{
    HRESULT hr; memset(m, 0xcc, sizeof *m);
    hr = IDWriteTextLayout_GetMetrics(l, m);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetMetrics", "got %s", dw_hr_name(hr));
}

void wt_check_lens_(IDWriteTextLayout *l, const UINT32 *want, int n, const char *file, int line)
{
    wt_lines o; char got[256] = "", tmp[24]; UINT32 i; int ok;
    HRESULT hr = wt_get_lines(l, &o);
    if (FAILED(hr)) { t_check(0, file, line, "line lengths", "GetLineMetrics failed %s", dw_hr_name(hr)); return; }
    ok = (int)o.n == n;
    for (i = 0; i < o.n; i++) { snprintf(tmp, sizeof tmp, "%s%u", i ? "," : "", o.lm[i].length); if (strlen(got) + strlen(tmp) < sizeof got) strcat(got, tmp); }
    for (i = 0; ok && (int)i < n; i++) if (o.lm[i].length != want[i]) ok = 0;
    if (!ok) {
        char w[256] = ""; int k;
        for (k = 0; k < n; k++) { snprintf(tmp, sizeof tmp, "%s%u", k ? "," : "", want[k]); if (strlen(w) + strlen(tmp) < sizeof w) strcat(w, tmp); }
        t_check(0, file, line, "line lengths", "got {%s} want {%s}", got, w);
    } else t_check(1, file, line, "line lengths", NULL);
}

UINT32 wt_clusters(IDWriteTextLayout *l, DWRITE_CLUSTER_METRICS *cm, UINT32 cap)
{
    UINT32 n = 0; HRESULT hr = IDWriteTextLayout_GetClusterMetrics(l, cm, cap, &n);
    if (FAILED(hr)) return 0;
    return n;
}

static float hit(IDWriteTextLayout *l, UINT32 pos, BOOL trailing, int wanty)
{
    float x = -999, y = -999; DWRITE_HIT_TEST_METRICS m;
    HRESULT hr = IDWriteTextLayout_HitTestTextPosition(l, pos, trailing, &x, &y, &m);
    if (FAILED(hr)) { t_note("HitTestTextPosition(%u,%d) -> %s", pos, (int)trailing, dw_hr_name(hr)); return -12345.0f; }
    return wanty ? y : x;
}
float wt_x(IDWriteTextLayout *l, UINT32 pos) { return hit(l, pos, FALSE, 0); }
float wt_x_trailing(IDWriteTextLayout *l, UINT32 pos) { return hit(l, pos, TRUE, 0); }
float wt_y(IDWriteTextLayout *l, UINT32 pos) { return hit(l, pos, FALSE, 1); }

dwx_renderer *wt_draw(IDWriteTextLayout *l, float ox, float oy)
{
    dwx_renderer *r = dwx_renderer_new();
    HRESULT hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, ox, oy);
    t_check(hr == S_OK, __FILE__, __LINE__, "Draw", "got %s", dw_hr_name(hr));
    return r;
}

void wt_analyze(IDWriteFactory *f, const WCHAR *text, UINT32 len, wt_brk *b)
{
    IDWriteTextAnalyzer *an = NULL; dwx_ana_src *src; dwx_ana_sink *snk; UINT32 i;
    memset(b, 0, sizeof *b); b->len = len;
    if (len > 512 || FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) { b->ok = -1; return; }
    src = dwx_ana_src_new(text, len); snk = dwx_ana_sink_new();
    if (FAILED(IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)snk))) b->ok = -1;
    else { b->ok = 1; for (i = 0; i < len; i++) b->bp[i] = snk->brk[i]; }
    IDWriteTextAnalyzer_Release(an); dwx_release(src); dwx_release(snk);
}
/* A break between p-1 and p is legal when neither side forbids it. */
int wt_can_break(const wt_brk *b, UINT32 p)
{
    if (b->ok != 1 || p == 0 || p >= b->len) return b->ok == 1 ? (p == b->len) : -1;
    return b->bp[p - 1].breakConditionAfter != DWRITE_BREAK_CONDITION_MAY_NOT_BREAK && b->bp[p].breakConditionBefore != DWRITE_BREAK_CONDITION_MAY_NOT_BREAK;
}

static float range_width(const DWRITE_CLUSTER_METRICS *cm, UINT32 ncm, UINT32 a, UINT32 b)
{
    UINT32 i, p = 0; float w = 0;
    for (i = 0; i < ncm; i++) { if (p >= a && p < b) w += cm[i].width; p += cm[i].length; }
    return w;
}

void wt_check_greedy_(wt_fx *x, IDWriteTextLayout *l, const WCHAR *text, float width, int check_fit, const char *file, int line)
{
    wt_lines lines; wt_brk b; DWRITE_CLUSTER_METRICS cm[512]; UINT32 ncm, len = (UINT32)wcslen(text), i, sum = 0;
    int bad_sum = 0, bad_legal = 0, bad_fit = 0, bad_greedy = 0;
    if (FAILED(wt_get_lines(l, &lines))) { t_check(0, file, line, "greedy", "GetLineMetrics failed"); return; }
    ncm = wt_clusters(l, cm, 512);
    wt_analyze(x->f, text, len, &b);
    if (b.ok != 1 || !ncm) { t_note("%s:%d analyzer or clusters unavailable", file, line); return; }
    for (i = 0; i < lines.n; i++) sum += lines.lm[i].length;
    bad_sum = sum != len;
    for (i = 0; i < lines.n; i++) {
        UINT32 s = lines.start[i], e = s + lines.lm[i].length, vis = e - lines.lm[i].trailingWhitespaceLength - lines.lm[i].newlineLength, p;
        float w; int internal = 0;
        if (i + 1 < lines.n && !lines.lm[i].newlineLength && !wt_can_break(&b, e)) bad_legal++;
        w = range_width(cm, ncm, s, vis);
        for (p = s + 1; p < vis; p++) if (wt_can_break(&b, p) == 1) internal = 1;
        if (check_fit && w > width && internal) bad_fit++;
        if (check_fit && i + 1 < lines.n && !lines.lm[i].newlineLength) {
            /* the next legal segment, with its own trailing white space excluded, must not have fit on this line */
            UINT32 q = e, ne = lines.start[i + 1] + lines.lm[i + 1].length, segend;
            for (segend = q + 1; segend < ne && !wt_can_break(&b, segend); segend++) ;
            {
                UINT32 se = segend; DWRITE_CLUSTER_METRICS *c = cm; UINT32 k, pp = 0; int wsfree = 1;
                /* strip white space at the end of the segment */
                while (se > q) {
                    for (k = 0, pp = 0; k < ncm; k++) { if (pp == se - 1) break; pp += c[k].length; }
                    if (k < ncm && c[k].isWhitespace) se--; else break;
                }
                (void)wsfree;
                if (se > q && range_width(cm, ncm, s, se) <= width) bad_greedy++;
            }
        }
    }
    if (bad_greedy || bad_fit || bad_legal) {
        char dbg[400] = "", tmp[32]; UINT32 q;
        for (q = 0; q < lines.n && strlen(dbg) < 340; q++) { snprintf(tmp, sizeof tmp, "%s%u/%u", q ? "," : "", lines.lm[q].length, lines.lm[q].trailingWhitespaceLength); strcat(dbg, tmp); }
        t_note("greedy debug: lines (len/trailingws) %s", dbg);
    }
    t_check(!bad_sum, file, line, "line lengths sum to the text length", "sum %u len %u", sum, len);
    t_check(!bad_legal, file, line, "every break is a legal break position", "%d illegal breaks (text \"%ls\" width %g)", bad_legal, text, width);
    t_check(!bad_fit, file, line, "every breakable line fits the width", "%d lines overflow (text \"%ls\" width %g)", bad_fit, text, width);
    t_check(!bad_greedy, file, line, "greedy: next segment did not fit", "%d lines could take more (text \"%ls\" width %g)", bad_greedy, text, width);
}

void *wt_qi(IUnknown *u, const IID *iid, const char *name)
{
    void *p = NULL; HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    if (FAILED(hr) || !p) t_skip("QueryInterface(%s) -> %s", name, dw_hr_name(hr));
    return p;
}

dwx_inline *wt_sign(float w, float h, float base) { return dwx_inline_new(w, h, base, FALSE); }
