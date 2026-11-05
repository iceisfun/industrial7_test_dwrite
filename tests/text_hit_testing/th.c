#include "th.h"

static uint8_t *build_family(const char *family, int lig, size_t *sz)
{
    static ttf_glyph gl[TTF_DEF_NGLYPHS + 1];
    static ttf_map map[160];
    static ttf_cmap cm[2];
    static const ttf_sub4 li[] = { { { 33, 36 }, 2, 64 } };       /* 'f' (28+5) 'i' (28+8) -> triangle glyph */
    static const ttf_feature fe[] = { { "liga", TTF_LK_LIGATURE, 1, li } };
    ttf_spec s; char ps[96]; int n = 0, k = 0, i; const ttf_glyph *g = ttf_default_glyphs(&n);
    ttf_default_spec(&s);
    snprintf(ps, sizeof ps, "%s-Regular", family);
    ttf_set_names(&s, family, "Regular", family, ps);
    memcpy(gl, g, sizeof(ttf_glyph) * n);
    gl[n].adv = 0; n++;                                              /* gid 69: zero-advance, empty (combining mark) */
    map[k].cp = 0x20; map[k++].gid = 1;
    for (i = 0; i < 10; i++) { map[k].cp = '0' + i; map[k++].gid = 54 + i; }
    for (i = 0; i < 26; i++) { map[k].cp = 'A' + i; map[k++].gid = 2 + i; }
    for (i = 0; i < 26; i++) { map[k].cp = 'a' + i; map[k++].gid = 28 + i; }
    map[k].cp = 0x301; map[k++].gid = 69;
    for (i = 0; i < 27; i++) { map[k].cp = 0x5d0 + i; map[k++].gid = 2 + i; }
    map[k].cp = 0x627; map[k++].gid = 2; map[k].cp = 0x628; map[k++].gid = 3;
    map[k].cp = 0x2026; map[k++].gid = 2;
    for (i = 0; i < 16; i++) { map[k].cp = 0x10000 + i; map[k++].gid = 2 + i; }
    { int a, b; for (a = 0; a < k; a++) for (b = a + 1; b < k; b++) if (map[b].cp < map[a].cp) { ttf_map t = map[a]; map[a] = map[b]; map[b] = t; } }
    cm[0].format = 4;  cm[0].plat = 3; cm[0].enc = 1;  cm[0].map = map;
    cm[1].format = 12; cm[1].plat = 3; cm[1].enc = 10; cm[1].map = map;
    { int bmp = 0; while (bmp < k && map[bmp].cp <= 0xFFFF) bmp++; cm[0].n = bmp; cm[1].n = k; }
    s.glyphs = gl; s.nglyphs = n; s.cmaps = cm; s.ncmaps = 2;
    if (lig) { s.gsub = fe; s.ngsub = 1; }
    return ttf_build(&s, sz);
}

int th_open(th_fx *x)
{
    size_t sz; uint8_t *b; int i;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    for (i = 0; i < 2; i++) {
        b = build_family(i ? "DwHitLig" : "DwHit", i, &sz);
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

void th_close(th_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}

IDWriteTextFormat *th_format(th_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, TH_LOC, &t))) return NULL;
    return t;
}

IDWriteTextLayout *th_layout_(th_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h)
{
    IDWriteTextFormat *t = th_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

th_pt th_point(IDWriteTextLayout *l, float x, float y)
{
    th_pt r;
    memset(&r, 0xcc, sizeof r);
    r.hr = IDWriteTextLayout_HitTestPoint(l, x, y, &r.tr, &r.in, &r.m);
    t_check(r.hr == S_OK, __FILE__, __LINE__, "HitTestPoint", "got %s (never fails on a valid layout)", dw_hr_name(r.hr));
    /* the BOOL outputs are written as 0 or 1 (never left, never other non-zero values) */
    t_check(r.tr == 0 || r.tr == 1, __FILE__, __LINE__, "isTrailingHit normalised", "got 0x%x", (unsigned)r.tr);
    t_check(r.in == 0 || r.in == 1, __FILE__, __LINE__, "isInside normalised", "got 0x%x", (unsigned)r.in);
    t_check(r.m.isText == 0 || r.m.isText == 1, __FILE__, __LINE__, "isText normalised", "got 0x%x", (unsigned)r.m.isText);
    t_check(r.m.isTrimmed == 0 || r.m.isTrimmed == 1, __FILE__, __LINE__, "isTrimmed normalised", "got 0x%x", (unsigned)r.m.isTrimmed);
    return r;
}

th_tp th_pos(IDWriteTextLayout *l, UINT32 pos, BOOL trailing)
{
    th_tp r;
    memset(&r, 0xcc, sizeof r);
    r.x = 1e30f; r.y = 1e30f;
    r.hr = IDWriteTextLayout_HitTestTextPosition(l, pos, trailing, &r.x, &r.y, &r.m);
    t_check(r.hr == S_OK, __FILE__, __LINE__, "HitTestTextPosition", "pos %u got %s (never fails on a valid layout)", pos, dw_hr_name(r.hr));
    t_check(r.x != 1e30f && r.y != 1e30f, __FILE__, __LINE__, "x/y written", "pos %u", pos);
    t_check(r.m.isText == 0 || r.m.isText == 1, __FILE__, __LINE__, "isText normalised", "got 0x%x", (unsigned)r.m.isText);
    t_check(r.m.isTrimmed == 0 || r.m.isTrimmed == 1, __FILE__, __LINE__, "isTrimmed normalised", "got 0x%x", (unsigned)r.m.isTrimmed);
    return r;
}

th_rg th_range(IDWriteTextLayout *l, UINT32 pos, UINT32 len, float ox, float oy)
{
    th_rg r;
    memset(&r, 0xcc, sizeof r);
    r.count = 0xdeadbeef;
    r.hr = IDWriteTextLayout_HitTestTextRange(l, pos, len, ox, oy, r.m, TH_MAXR, &r.count);
    t_check(r.hr == S_OK, __FILE__, __LINE__, "HitTestTextRange", "(%u,%u) got %s", pos, len, dw_hr_name(r.hr));
    t_check(r.count <= TH_MAXR, __FILE__, __LINE__, "actualHitTestMetricsCount", "(%u,%u) count %u", pos, len, r.count);
    if (r.count > TH_MAXR) r.count = 0;
    return r;
}

void th_metrics(IDWriteTextLayout *l, DWRITE_TEXT_METRICS *m)
{
    HRESULT hr;
    memset(m, 0xcc, sizeof *m);
    hr = IDWriteTextLayout_GetMetrics(l, m);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetMetrics", "got %s", dw_hr_name(hr));
}
UINT32 th_lines(IDWriteTextLayout *l) { DWRITE_TEXT_METRICS m; th_metrics(l, &m); return m.lineCount; }

float th_lh(IDWriteTextLayout *l, UINT32 line)
{
    DWRITE_LINE_METRICS lm[16]; UINT32 n = 0;
    HRESULT hr = IDWriteTextLayout_GetLineMetrics(l, lm, 16, &n);
    t_check(hr == S_OK && line < n, __FILE__, __LINE__, "GetLineMetrics", "got %s count %u", dw_hr_name(hr), n);
    return (hr == S_OK && line < n) ? lm[line].height : 0.0f;
}

UINT32 th_clusters(IDWriteTextLayout *l, DWRITE_CLUSTER_METRICS *c, UINT32 cap)
{
    UINT32 n = 0;
    HRESULT hr = IDWriteTextLayout_GetClusterMetrics(l, c, cap, &n);
    if (hr != S_OK) return 0;
    return n;
}

void th_expect_pt(const th_pt *p, UINT32 pos, UINT32 len, int trailing, int inside, const char *file, int line)
{
    t_check(p->m.textPosition == pos, file, line, "hit textPosition", "got %u want %u", p->m.textPosition, pos);
    if (len != (UINT32)-1) t_check(p->m.length == len, file, line, "hit length", "got %u want %u", p->m.length, len);
    if (trailing >= 0) t_check((int)p->tr == trailing, file, line, "isTrailingHit", "got %d want %d", (int)p->tr, trailing);
    if (inside >= 0) t_check((int)p->in == inside, file, line, "isInside", "got %d want %d", (int)p->in, inside);
}

void th_expect_m(const DWRITE_HIT_TEST_METRICS *m, UINT32 pos, UINT32 len, float left, float width, UINT32 bidi, int isText, const char *file, int line)
{
    t_check(m->textPosition == pos, file, line, "metrics textPosition", "got %u want %u", m->textPosition, pos);
    t_check(m->length == len, file, line, "metrics length", "got %u want %u", m->length, len);
    if (!isnan(left)) t_check(th_feq(m->left, left), file, line, "metrics left", "got %g want %g", (double)m->left, (double)left);
    if (!isnan(width)) t_check(th_feq(m->width, width), file, line, "metrics width", "got %g want %g", (double)m->width, (double)width);
    if (bidi != (UINT32)-1) t_check(m->bidiLevel == bidi, file, line, "metrics bidiLevel", "got %u want %u", m->bidiLevel, bidi);
    if (isText >= 0) t_check((int)m->isText == isText, file, line, "metrics isText", "got %d want %d", (int)m->isText, isText);
}

float th_cw_sum(IDWriteTextLayout *l, UINT32 pos, UINT32 len)
{
    DWRITE_CLUSTER_METRICS c[64]; UINT32 n = th_clusters(l, c, 64), i, at = 0; float w = 0;
    for (i = 0; i < n; i++) {
        if (at >= pos && at < pos + len) w += c[i].width;
        at += c[i].length;
    }
    return w;
}

/* Cross-checks HitTestTextPosition / HitTestPoint against GetClusterMetrics + GetLineMetrics for single-direction left-to-right text. */
void th_check_consistency(IDWriteTextLayout *l, const char *file, int line)
{
    DWRITE_CLUSTER_METRICS c[256]; DWRITE_LINE_METRICS lm[64]; UINT32 nc = 0, nl = 0, ci, pos = 0, li = 0, lineStart = 0, k;
    float top = 0, prevRight = 0; int havePrev = 0; DWRITE_TEXT_METRICS tm;
    th_metrics(l, &tm); top = tm.top;
    HRESULT hr = IDWriteTextLayout_GetLineMetrics(l, lm, 64, &nl);
    t_check(hr == S_OK && nl > 0, file, line, "GetLineMetrics", "got %s", dw_hr_name(hr));
    if (hr != S_OK || !nl) return;
    nc = th_clusters(l, c, 256);
    t_check(nc > 0, file, line, "GetClusterMetrics", "no clusters");
    for (ci = 0; ci < nc; ci++) {
        while (li + 1 < nl && pos >= lineStart + lm[li].length) { lineStart += lm[li].length; top += lm[li].height; li++; havePrev = 0; }
        th_tp q = th_pos(l, pos, FALSE);
        t_check(q.m.textPosition == pos && q.m.length == c[ci].length, file, line, "cluster hit rect", "pos %u: got (%u,%u) want (%u,%u)", pos, q.m.textPosition, q.m.length, pos, c[ci].length);
        t_check(th_feq(q.m.width, c[ci].width), file, line, "cluster hit width", "pos %u got %g want %g", pos, (double)q.m.width, (double)c[ci].width);
        t_check(q.m.top >= top - EPS && q.m.top <= top + lm[li].height + EPS, file, line, "cluster hit top", "pos %u got %g want %g", pos, (double)q.m.top, (double)top);
        t_check(q.m.height > 0.0f && q.m.height <= lm[li].height + EPS, file, line, "cluster hit height", "pos %u got %g line %g", pos, (double)q.m.height, (double)lm[li].height);
        t_check(th_feq(q.x, q.m.left) && th_feq(q.y, q.m.top), file, line, "leading caret", "pos %u", pos);
        t_check(q.m.bidiLevel == 0 && q.m.isText == 1, file, line, "bidi/isText", "pos %u level %u text %d", pos, q.m.bidiLevel, (int)q.m.isText);
        if (havePrev) t_check(th_feq(q.m.left, prevRight), file, line, "adjacent clusters", "pos %u left %g prev right %g", pos, (double)q.m.left, (double)prevRight);
        for (k = 1; k < c[ci].length; k++) {
            th_tp q2 = th_pos(l, pos + k, FALSE);
            t_check(!memcmp(&q2.m, &q.m, sizeof q.m) && q2.x == q.x && q2.y == q.y, file, line, "positions inside a cluster", "pos %u+%u", pos, k);
        }
        th_tp t = th_pos(l, pos, TRUE);
        t_check(th_feq(t.x, q.m.left + q.m.width) && th_feq(t.y, q.y), file, line, "trailing caret", "pos %u", pos);
        /* hit the cluster with points: only clusters before the line's newline can be hit */
        if (c[ci].width > 1.0f && pos + c[ci].length <= lineStart + lm[li].length - lm[li].newlineLength) {
            float y = q.m.top + q.m.height * 0.5f;
            th_pt a = th_point(l, q.m.left + 0.25f * q.m.width, y), b = th_point(l, q.m.left + 0.75f * q.m.width, y);
            t_check(a.m.textPosition == pos && a.tr == 0 && a.in == 1, file, line, "point in leading quarter", "pos %u got pos %u tr %d in %d", pos, a.m.textPosition, (int)a.tr, (int)a.in);
            t_check(b.m.textPosition == pos && b.tr == 1 && b.in == 1, file, line, "point in trailing quarter", "pos %u got pos %u tr %d in %d", pos, b.m.textPosition, (int)b.tr, (int)b.in);
        }
        prevRight = q.m.left + q.m.width; havePrev = 1;
        pos += c[ci].length;
    }
}
