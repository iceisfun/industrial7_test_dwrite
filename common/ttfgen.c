/* Synthetic TrueType builder; see ttfgen.h. Pure C99, no Windows dependencies. */
#include "ttfgen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- growable big-endian buffer ---------------------------------------- */
typedef struct { uint8_t *p; size_t n, cap; } buf;
static void bgrow(buf *b, size_t add) {
    if (b->n + add <= b->cap) return;
    size_t c = b->cap ? b->cap : 256;
    while (c < b->n + add) c *= 2;
    b->p = (uint8_t *)realloc(b->p, c); b->cap = c;
}
static void bput(buf *b, const void *d, size_t n) { bgrow(b, n); if (n) memcpy(b->p + b->n, d, n); b->n += n; }
static void b8(buf *b, unsigned v)  { uint8_t c = (uint8_t)v; bput(b, &c, 1); }
static void b16(buf *b, unsigned v) { uint8_t c[2] = { (uint8_t)(v >> 8), (uint8_t)v }; bput(b, c, 2); }
static void b24(buf *b, unsigned v) { b8(b, v >> 16); b16(b, v & 0xFFFF); }
static void b32(buf *b, uint32_t v) { b16(b, v >> 16); b16(b, v & 0xFFFF); }
static void bpad4(buf *b) { while (b->n & 3) b8(b, 0); }
static void bset16(buf *b, size_t at, unsigned v) { b->p[at] = (uint8_t)(v >> 8); b->p[at + 1] = (uint8_t)v; }
static void bset32(buf *b, size_t at, uint32_t v) { bset16(b, at, v >> 16); bset16(b, at + 2, v & 0xFFFF); }
static uint32_t rd16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
static uint32_t rd32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static void wr32(uint8_t *p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static uint32_t checksum(const uint8_t *p, size_t n) {
    uint32_t s = 0; size_t i;
    for (i = 0; i + 4 <= n; i += 4) s += rd32(p + i);
    if (i < n) { uint8_t t[4] = { 0, 0, 0, 0 }; memcpy(t, p + i, n - i); s += rd32(t); }
    return s;
}
static unsigned flog2(unsigned v) { unsigned r = 0; while (v > 1) { v >>= 1; r++; } return r; }

/* ---- default font data -------------------------------------------------- */
static const ttf_pt box_pts[4]      = { {100,0,1}, {100,1400,1}, {900,1400,1}, {900,0,1} };          /* clockwise */
static const ttf_pt notdef_pts[4]   = { {100,0,1}, {100,1400,1}, {1100,1400,1}, {1100,0,1} };
static const ttf_pt tri_pts[3]      = { {100,0,1}, {500,1400,1}, {900,0,1} };
static const ttf_pt round_pts[8]    = { {500,0,1}, {100,0,0}, {100,700,1}, {100,1400,0}, {500,1400,1}, {900,1400,0}, {900,700,1}, {900,0,0} };
static const ttf_pt ring_pts[8]     = { {100,0,1}, {100,1400,1}, {900,1400,1}, {900,0,1}, {300,300,1}, {700,300,1}, {700,1100,1}, {300,1100,1} };
static const unsigned short end4[1] = { 3 }, end3[1] = { 2 }, end8[1] = { 7 }, end_ring[2] = { 3, 7 };
static const ttf_comp comp66[2]     = { { 2, 0, 0, TTF_COMP_USE_MY_METRICS, 0 }, { TTF_GID_TRIANGLE, 0, 1400, 0, 0 } };
static ttf_glyph g_def_glyphs[TTF_DEF_NGLYPHS];
static ttf_map g_def_map[63];
static ttf_cmap g_def_cmaps[2];
static int g_def_init;
static void def_init(void) {
    if (g_def_init) return;
    int i, k = 0;
    memset(g_def_glyphs, 0, sizeof g_def_glyphs);
    g_def_glyphs[0].adv = TTF_NOTDEF_ADV; g_def_glyphs[0].npts = 4; g_def_glyphs[0].ncont = 1; g_def_glyphs[0].pts = notdef_pts; g_def_glyphs[0].ends = end4;
    g_def_glyphs[1].adv = TTF_DEF_ADV;
    for (i = 2; i < 64; i++) { g_def_glyphs[i].adv = TTF_DEF_ADV; g_def_glyphs[i].npts = 4; g_def_glyphs[i].ncont = 1; g_def_glyphs[i].pts = box_pts; g_def_glyphs[i].ends = end4; }
    g_def_glyphs[64].adv = TTF_DEF_ADV; g_def_glyphs[64].npts = 3; g_def_glyphs[64].ncont = 1; g_def_glyphs[64].pts = tri_pts; g_def_glyphs[64].ends = end3;
    g_def_glyphs[65].adv = TTF_DEF_ADV; g_def_glyphs[65].npts = 8; g_def_glyphs[65].ncont = 1; g_def_glyphs[65].pts = round_pts; g_def_glyphs[65].ends = end8;
    g_def_glyphs[66].adv = TTF_DEF_ADV; g_def_glyphs[66].ncomp = 2; g_def_glyphs[66].comps = comp66;
    g_def_glyphs[67].adv = 500;
    g_def_glyphs[68].adv = TTF_DEF_ADV; g_def_glyphs[68].npts = 8; g_def_glyphs[68].ncont = 2; g_def_glyphs[68].pts = ring_pts; g_def_glyphs[68].ends = end_ring;
    g_def_map[k].cp = 0x20; g_def_map[k++].gid = 1;
    for (i = 0; i < 26; i++) { g_def_map[k].cp = 'A' + i; g_def_map[k++].gid = 2 + i; }
    for (i = 0; i < 26; i++) { g_def_map[k].cp = 'a' + i; g_def_map[k++].gid = 28 + i; }
    for (i = 0; i < 10; i++) { g_def_map[k].cp = '0' + i; g_def_map[k++].gid = 54 + i; }
    /* sorted by code point: 0x20, 0x30.., 0x41.., 0x61..  */
    { int a, b; for (a = 0; a < k; a++) for (b = a + 1; b < k; b++) if (g_def_map[b].cp < g_def_map[a].cp) { ttf_map t = g_def_map[a]; g_def_map[a] = g_def_map[b]; g_def_map[b] = t; } }
    g_def_cmaps[0].format = 4;  g_def_cmaps[0].plat = 3; g_def_cmaps[0].enc = 1;  g_def_cmaps[0].n = k; g_def_cmaps[0].map = g_def_map;
    g_def_cmaps[1].format = 12; g_def_cmaps[1].plat = 3; g_def_cmaps[1].enc = 10; g_def_cmaps[1].n = k; g_def_cmaps[1].map = g_def_map;
    g_def_init = 1;
}
const ttf_glyph *ttf_default_glyphs(int *n) { def_init(); if (n) *n = TTF_DEF_NGLYPHS; return g_def_glyphs; }
const ttf_cmap *ttf_default_cmaps(int *n) { def_init(); if (n) *n = 2; return g_def_cmaps; }
unsigned ttf_default_gid(unsigned cp) {
    if (cp == 0x20) return 1;
    if (cp >= 'A' && cp <= 'Z') return 2 + cp - 'A';
    if (cp >= 'a' && cp <= 'z') return 28 + cp - 'a';
    if (cp >= '0' && cp <= '9') return 54 + cp - '0';
    return 0;
}

void ttf_clear_names(ttf_spec *s) { s->nnames = 0; }
void ttf_add_name(ttf_spec *s, unsigned plat, unsigned enc, unsigned lang, unsigned id, const char *utf8) {
    if (s->nnames >= (int)(sizeof s->names / sizeof s->names[0])) return;
    ttf_name *n = &s->names[s->nnames++];
    n->plat = plat; n->enc = enc; n->lang = lang; n->id = id;
    snprintf(n->text, sizeof n->text, "%s", utf8);
}
void ttf_set_names(ttf_spec *s, const char *family, const char *sub, const char *full, const char *ps) {
    s->nnames = 0;
    ttf_add_name(s, 3, 1, 0x409, 1, family); ttf_add_name(s, 3, 1, 0x409, 2, sub);
    ttf_add_name(s, 3, 1, 0x409, 4, full);   ttf_add_name(s, 3, 1, 0x409, 6, ps);
    ttf_add_name(s, 1, 0, 0, 1, family); ttf_add_name(s, 1, 0, 0, 2, sub);
    ttf_add_name(s, 1, 0, 0, 4, full);   ttf_add_name(s, 1, 0, 0, 6, ps);
}
void ttf_default_spec(ttf_spec *s) {
    def_init();
    memset(s, 0, sizeof *s);
    s->sfnt_version = 0x00010000; s->upm = TTF_DEF_UPM;
    s->hhea_asc = 1700; s->hhea_desc = -400; s->hhea_gap = 90;
    s->typo_asc = 1600; s->typo_desc = -420; s->typo_gap = 80;
    s->win_asc = 1800; s->win_desc = 450;
    s->cap_height = 1400; s->x_height = 1000; s->x_avg_width = 1000;
    s->underline_pos = -200; s->underline_thick = 100; s->strike_pos = 600; s->strike_size = 100;
    s->fs_selection = 0x40; s->weight = 400; s->width = 5; s->os2_version = 4; s->head_flags = 3;
    s->revision_fixed = 0x00010000;
    s->glyphs = g_def_glyphs; s->nglyphs = TTF_DEF_NGLYPHS;
    s->cmaps = g_def_cmaps; s->ncmaps = 2;
    s->unicode_range[0] = 1; /* Basic Latin */
    s->codepage_range[0] = 1;
    ttf_set_names(s, "DwTest", "Regular", "DwTest", "DwTest-Regular");
}
const char *ttf_weight_word(int w) {
    switch (w) {
    case 100: return "Thin"; case 200: return "ExtraLight"; case 300: return "Light"; case 350: return "SemiLight";
    case 400: return "Regular"; case 500: return "Medium"; case 600: return "SemiBold"; case 700: return "Bold";
    case 800: return "ExtraBold"; case 900: return "Black"; case 950: return "ExtraBlack"; default: return "Custom";
    }
}
static const char *width_word(int w) {
    static const char *t[] = { "", "UltraCondensed", "ExtraCondensed", "Condensed", "SemiCondensed", "", "SemiExpanded", "Expanded", "ExtraExpanded", "UltraExpanded" };
    return (w >= 1 && w <= 9) ? t[w] : "";
}
void ttf_style_spec(ttf_spec *s, const char *family, int weight, int width, int italic) {
    char fam1[96], sub1[96], full[96], ps[96], style[96], tmp[200], base[96];
    size_t i, j;
    ttf_default_spec(s);
    s->weight = weight; s->width = width;
    s->fs_selection = 0;
    if (italic) { s->fs_selection |= 1; s->mac_style |= 2; s->italic_angle_fixed = -12 * 65536; }
    if (weight >= 700) s->mac_style |= 1;
    if (weight == 700 && width == 5) s->fs_selection |= 0x20;
    if (!italic && !(weight == 700 && width == 5)) s->fs_selection |= 0x40;
    /* style = non-italic part ("Regular", "Bold", "Condensed Light" ...) [+ " Italic"] */
    if (width == 5) snprintf(base, sizeof base, "%s", ttf_weight_word(weight));
    else snprintf(base, sizeof base, "%s%s%s", width_word(width), weight != 400 ? " " : "", weight != 400 ? ttf_weight_word(weight) : "");
    if (italic && width == 5 && weight == 400) snprintf(style, sizeof style, "Italic");
    else snprintf(style, sizeof style, "%s%s", base, italic ? " Italic" : "");
    snprintf(full, sizeof full, "%s%s%s", family, strcmp(style, "Regular") ? " " : "", strcmp(style, "Regular") ? style : "");
    snprintf(tmp, sizeof tmp, "%s-%s", family, style);
    for (i = j = 0; tmp[i] && j < sizeof ps - 1; i++) if (tmp[i] != ' ') ps[j++] = tmp[i];
    ps[j] = 0;
    s->nnames = 0;
    if (width == 5 && (weight == 400 || weight == 700)) {
        snprintf(fam1, sizeof fam1, "%s", family); snprintf(sub1, sizeof sub1, "%s", style);
    } else {
        /* legacy family carries every style word except Italic; legacy subfamily is Regular or Italic */
        const char *w = (italic && width == 5 && weight == 400) ? "" : base;
        snprintf(fam1, sizeof fam1, "%s%s%s", family, w[0] ? " " : "", w);
        snprintf(sub1, sizeof sub1, "%s", italic ? "Italic" : "Regular");
    }
    ttf_add_name(s, 3, 1, 0x409, 1, fam1); ttf_add_name(s, 3, 1, 0x409, 2, sub1);
    ttf_add_name(s, 3, 1, 0x409, 3, ps);
    ttf_add_name(s, 3, 1, 0x409, 4, full); ttf_add_name(s, 3, 1, 0x409, 6, ps);
    ttf_add_name(s, 1, 0, 0, 1, fam1); ttf_add_name(s, 1, 0, 0, 2, sub1);
    ttf_add_name(s, 1, 0, 0, 4, full); ttf_add_name(s, 1, 0, 0, 6, ps);
    if (!(width == 5 && (weight == 400 || weight == 700))) {
        ttf_add_name(s, 3, 1, 0x409, 16, family); ttf_add_name(s, 3, 1, 0x409, 17, style);
    }
}

/* ---- glyph outline bbox ------------------------------------------------- */
typedef struct { int ok, x0, y0, x1, y1; } bbox;
static void bb_add(bbox *b, int x, int y) {
    if (!b->ok) { b->ok = 1; b->x0 = b->x1 = x; b->y0 = b->y1 = y; return; }
    if (x < b->x0) b->x0 = x; if (x > b->x1) b->x1 = x; if (y < b->y0) b->y0 = y; if (y > b->y1) b->y1 = y;
}
static bbox glyph_bbox(const ttf_spec *s, int gid, int depth) {
    bbox b = { 0, 0, 0, 0, 0 };
    const ttf_glyph *g = &s->glyphs[gid];
    int i;
    if (g->ncomp > 0) {
        if (depth > 4) return b;
        for (i = 0; i < g->ncomp; i++) {
            const ttf_comp *c = &g->comps[i];
            if (c->gid >= s->nglyphs) continue;
            bbox cb = glyph_bbox(s, c->gid, depth + 1);
            if (!cb.ok) continue;
            double sc = c->scale ? c->scale / 16384.0 : 1.0;
            bb_add(&b, (int)(cb.x0 * sc) + c->dx, (int)(cb.y0 * sc) + c->dy);
            bb_add(&b, (int)(cb.x1 * sc) + c->dx, (int)(cb.y1 * sc) + c->dy);
        }
        return b;
    }
    for (i = 0; i < g->npts; i++) bb_add(&b, g->pts[i].x, g->pts[i].y);
    return b;
}

/* ---- tables ------------------------------------------------------------- */
typedef struct { char tag[5]; buf d; } tbl;
typedef struct { tbl t[40]; int n; } tbllist;
static buf *add_tbl(tbllist *L, const char *tag) {
    int i;
    for (i = 0; i < L->n; i++) if (!memcmp(L->t[i].tag, tag, 4)) { free(L->t[i].d.p); memset(&L->t[i].d, 0, sizeof(buf)); return &L->t[i].d; }
    if (L->n >= 40) return &L->t[39].d;
    memcpy(L->t[L->n].tag, tag, 4); L->t[L->n].tag[4] = 0;
    memset(&L->t[L->n].d, 0, sizeof(buf));
    return &L->t[L->n++].d;
}
static int cmp_u(const void *a, const void *b) { unsigned x = *(const unsigned *)a, y = *(const unsigned *)b; return x < y ? -1 : x > y; }
static int cmp_map(const void *a, const void *b) { const ttf_map *x = a, *y = b; return x->cp < y->cp ? -1 : x->cp > y->cp; }

static void utf8_to_u16(const char *s, buf *out, int utf16) {
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned cp; int extra;
        if (*p < 0x80) { cp = *p; extra = 0; } else if (*p < 0xE0) { cp = *p & 0x1F; extra = 1; }
        else if (*p < 0xF0) { cp = *p & 0x0F; extra = 2; } else { cp = *p & 0x07; extra = 3; }
        p++; while (extra-- && *p) cp = (cp << 6) | (*p++ & 0x3F);
        if (!utf16) { b8(out, cp < 0x100 ? cp : '?'); continue; }
        if (cp >= 0x10000) { cp -= 0x10000; b16(out, 0xD800 | (cp >> 10)); b16(out, 0xDC00 | (cp & 0x3FF)); } else b16(out, cp);
    }
}
static int cmp_name(const void *a, const void *b) {
    const ttf_name *x = a, *y = b;
    if (x->plat != y->plat) return x->plat - y->plat; if (x->enc != y->enc) return x->enc - y->enc;
    if (x->lang != y->lang) return x->lang - y->lang; return x->id - y->id;
}
static void build_name(const ttf_spec *s, buf *o) {
    ttf_name sorted[40]; int n = s->nnames, i; buf str = { 0, 0, 0 };
    memcpy(sorted, s->names, n * sizeof(ttf_name));
    qsort(sorted, n, sizeof(ttf_name), cmp_name);
    b16(o, 0); b16(o, n); b16(o, 6 + 12 * n);
    for (i = 0; i < n; i++) {
        size_t off = str.n;
        utf8_to_u16(sorted[i].text, &str, sorted[i].plat != 1);
        b16(o, sorted[i].plat); b16(o, sorted[i].enc); b16(o, sorted[i].lang); b16(o, sorted[i].id);
        b16(o, str.n - off); b16(o, off);
    }
    bput(o, str.p, str.n); free(str.p);
}

static void cmap_sub(const ttf_cmap *c, buf *o) {
    ttf_map *m = (ttf_map *)malloc((c->n + 1) * sizeof(ttf_map));
    int n = c->n, i;
    if (n) memcpy(m, c->map, n * sizeof(ttf_map));
    qsort(m, n, sizeof(ttf_map), cmp_map);
    switch (c->format) {
    case 0: {
        uint8_t t[256]; memset(t, 0, 256);
        for (i = 0; i < n; i++) if (m[i].cp < 256) t[m[i].cp] = (uint8_t)m[i].gid;
        b16(o, 0); b16(o, 262); b16(o, 0); bput(o, t, 256); break; }
    case 4: {
        unsigned st[512], en[512], gs[512]; int seg = 0;
        for (i = 0; i < n; i++) {
            if (m[i].cp >= 0xFFFF) continue;
            if (seg && m[i].cp == en[seg - 1] + 1 && m[i].gid == gs[seg - 1] + (m[i].cp - st[seg - 1])) { en[seg - 1] = m[i].cp; continue; }
            if (seg >= 510) break;
            st[seg] = en[seg] = m[i].cp; gs[seg] = m[i].gid; seg++;
        }
        st[seg] = en[seg] = 0xFFFF; gs[seg] = 0; seg++;
        unsigned sr = 2 * (1u << flog2(seg));
        b16(o, 4); b16(o, 16 + 8 * seg); b16(o, 0); b16(o, 2 * seg); b16(o, sr); b16(o, flog2(seg)); b16(o, 2 * seg - sr);
        for (i = 0; i < seg; i++) b16(o, en[i]);
        b16(o, 0);
        for (i = 0; i < seg; i++) b16(o, st[i]);
        for (i = 0; i < seg; i++) b16(o, i == seg - 1 ? 1 : (gs[i] - st[i]) & 0xFFFF);
        for (i = 0; i < seg; i++) b16(o, 0);
        break; }
    case 6: {
        unsigned lo = n ? m[0].cp : 0, hi = n ? m[n - 1].cp : 0, k;
        if (hi > 0xFFFF) hi = 0xFFFF;
        unsigned cnt = n ? hi - lo + 1 : 0;
        b16(o, 6); b16(o, 10 + 2 * cnt); b16(o, 0); b16(o, lo); b16(o, cnt);
        for (k = 0; k < cnt; k++) { unsigned g = 0; for (i = 0; i < n; i++) if (m[i].cp == lo + k) g = m[i].gid; b16(o, g); }
        break; }
    case 12: case 13: {
        unsigned *grp = (unsigned *)malloc(3 * (n + 1) * sizeof(unsigned)); int ng = 0;
        for (i = 0; i < n; i++) {
            if (ng) {
                unsigned *g = grp + 3 * (ng - 1);
                int cont = m[i].cp == g[1] + 1;
                if (cont && ((c->format == 12 && m[i].gid == g[2] + (m[i].cp - g[0])) || (c->format == 13 && m[i].gid == g[2]))) { g[1] = m[i].cp; continue; }
            }
            grp[3 * ng] = grp[3 * ng + 1] = m[i].cp; grp[3 * ng + 2] = m[i].gid; ng++;
        }
        b16(o, c->format); b16(o, 0); b32(o, 16 + 12 * ng); b32(o, 0); b32(o, ng);
        for (i = 0; i < ng; i++) { b32(o, grp[3 * i]); b32(o, grp[3 * i + 1]); b32(o, grp[3 * i + 2]); }
        free(grp); break; }
    case 14: {
        unsigned vs[64]; int nvs = 0, j, k;
        for (i = 0; i < c->nuvs; i++) { for (j = 0; j < nvs; j++) if (vs[j] == c->uvs[i].vs) break; if (j == nvs && nvs < 64) vs[nvs++] = c->uvs[i].vs; }
        qsort(vs, nvs, sizeof(unsigned), cmp_u);
        buf body = { 0, 0, 0 }; size_t hdr = 10 + 11 * nvs;
        b16(o, 14); b32(o, 0); b32(o, nvs);
        for (j = 0; j < nvs; j++) {
            int nd = 0, nn = 0; size_t doff = 0, noff = 0;
            for (i = 0; i < c->nuvs; i++) if (c->uvs[i].vs == vs[j]) { if (c->uvs[i].is_default) nd++; else nn++; }
            if (nd) { doff = hdr + body.n; b32(&body, nd); for (i = 0; i < c->nuvs; i++) if (c->uvs[i].vs == vs[j] && c->uvs[i].is_default) { b24(&body, c->uvs[i].cp); b8(&body, 0); } }
            if (nn) { noff = hdr + body.n; b32(&body, nn); for (i = 0; i < c->nuvs; i++) if (c->uvs[i].vs == vs[j] && !c->uvs[i].is_default) { b24(&body, c->uvs[i].cp); b16(&body, c->uvs[i].gid); } }
            b24(o, vs[j]); b32(o, (uint32_t)doff); b32(o, (uint32_t)noff);
            (void)k;
        }
        bput(o, body.p, body.n); free(body.p);
        bset32(o, 2, (uint32_t)o->n); break; }
    }
    free(m);
}
static int cmp_cmap(const void *a, const void *b) {
    const ttf_cmap *x = *(const ttf_cmap *const *)a, *y = *(const ttf_cmap *const *)b;
    if (x->plat != y->plat) return x->plat - y->plat; return x->enc - y->enc;
}
static void build_cmap(const ttf_spec *s, buf *o) {
    int n = s->ncmaps, i; const ttf_cmap *ord[16]; buf sub[16];
    if (n > 16) n = 16;
    for (i = 0; i < n; i++) ord[i] = &s->cmaps[i];
    qsort(ord, n, sizeof ord[0], cmp_cmap);
    b16(o, 0); b16(o, n);
    size_t off = 4 + 8 * n;
    for (i = 0; i < n; i++) { memset(&sub[i], 0, sizeof(buf)); cmap_sub(ord[i], &sub[i]); b16(o, ord[i]->plat); b16(o, ord[i]->enc); b32(o, (uint32_t)off); off += sub[i].n; }
    for (i = 0; i < n; i++) { bput(o, sub[i].p, sub[i].n); free(sub[i].p); }
}

/* GSUB / GPOS */
static int cmp_feat(const void *a, const void *b) { return memcmp(((const ttf_feature *)a)->tag, ((const ttf_feature *)b)->tag, 4); }
static int cmp_s1(const void *a, const void *b) { return (int)((const ttf_sub1 *)a)->from - (int)((const ttf_sub1 *)b)->from; }
static int cmp_s3(const void *a, const void *b) { return (int)((const ttf_sub3 *)a)->from - (int)((const ttf_sub3 *)b)->from; }
static int cmp_s4(const void *a, const void *b) { return (int)((const ttf_sub4 *)a)->comps[0] - (int)((const ttf_sub4 *)b)->comps[0]; }
static void coverage(buf *o, const unsigned short *g, int n) { int i; b16(o, 1); b16(o, n); for (i = 0; i < n; i++) b16(o, g[i]); }
static void lookup_sub(const ttf_feature *f, buf *o) {   /* o receives the subtable (offsets relative to its start) */
    int i, n = f->n; unsigned short cov[256];
    if (n > 250) n = 250;
    if (f->type == TTF_LK_SINGLE) {
        ttf_sub1 *a = (ttf_sub1 *)malloc(n * sizeof *a + 4); memcpy(a, f->data, n * sizeof *a); qsort(a, n, sizeof *a, cmp_s1);
        for (i = 0; i < n; i++) cov[i] = a[i].from;
        b16(o, 2); b16(o, 6 + 2 * n); b16(o, n); for (i = 0; i < n; i++) b16(o, a[i].to);
        coverage(o, cov, n); free(a);
    } else if (f->type == TTF_LK_ALTERNATE) {
        ttf_sub3 *a = (ttf_sub3 *)malloc(n * sizeof *a + 4); memcpy(a, f->data, n * sizeof *a); qsort(a, n, sizeof *a, cmp_s3);
        for (i = 0; i < n; i++) cov[i] = a[i].from;
        size_t cov_off = 6 + 2 * n, sets = cov_off + 4 + 2 * n, pos = sets;
        b16(o, 1); b16(o, cov_off); b16(o, n);
        for (i = 0; i < n; i++) { b16(o, pos); pos += 2 + 2 * a[i].nalt; }
        coverage(o, cov, n);
        for (i = 0; i < n; i++) { int k; b16(o, a[i].nalt); for (k = 0; k < a[i].nalt; k++) b16(o, a[i].alt[k]); }
        free(a);
    } else if (f->type == TTF_LK_LIGATURE) {
        ttf_sub4 *a = (ttf_sub4 *)malloc(n * sizeof *a + 4); memcpy(a, f->data, n * sizeof *a);
        { int x, y; for (x = 1; x < n; x++) for (y = x; y > 0 && cmp_s4(&a[y - 1], &a[y]) > 0; y--) { ttf_sub4 t = a[y]; a[y] = a[y - 1]; a[y - 1] = t; } } /* stable */
        int nsets = 0, first[256], cnt[256];
        for (i = 0; i < n; i++) { if (nsets && cov[nsets - 1] == a[i].comps[0]) cnt[nsets - 1]++; else { cov[nsets] = a[i].comps[0]; first[nsets] = i; cnt[nsets] = 1; nsets++; } }
        size_t cov_off = 6 + 2 * nsets, pos = cov_off + 4 + 2 * nsets; int st;
        b16(o, 1); b16(o, cov_off); b16(o, nsets);
        for (st = 0; st < nsets; st++) { b16(o, pos); size_t sz = 2 + 2 * cnt[st]; int k; for (k = 0; k < cnt[st]; k++) sz += 4 + 2 * (a[first[st] + k].ncomp - 1); pos += sz; }
        coverage(o, cov, nsets);
        for (st = 0; st < nsets; st++) {
            int k; size_t lp = 2 + 2 * cnt[st];
            b16(o, cnt[st]);
            for (k = 0; k < cnt[st]; k++) { b16(o, lp); lp += 4 + 2 * (a[first[st] + k].ncomp - 1); }
            for (k = 0; k < cnt[st]; k++) { const ttf_sub4 *l = &a[first[st] + k]; int c; b16(o, l->lig); b16(o, l->ncomp); for (c = 1; c < l->ncomp; c++) b16(o, l->comps[c]); }
        }
        free(a);
    }
}
static void layout_common(buf *o, int nfeat, const char (*tags)[4], buf *lookups, int *ltype) {
    /* header v1.0 (10 bytes) : script list, feature list, lookup list */
    buf sl = { 0, 0, 0 }, fl = { 0, 0, 0 }, ll = { 0, 0, 0 }; int i;
    /* ScriptList: DFLT, latn each with defaultLangSys listing all features */
    b16(&sl, 2);
    { size_t scr_sz = 4 + (6 + 2 * nfeat); static const char *tg[2] = { "DFLT", "latn" }; int sc;
      for (sc = 0; sc < 2; sc++) { bput(&sl, tg[sc], 4); b16(&sl, 2 + 6 * 2 + sc * (int)scr_sz); }
      for (sc = 0; sc < 2; sc++) {
          b16(&sl, 4); b16(&sl, 0);                                   /* Script: defaultLangSys at +4, 0 langsys */
          b16(&sl, 0); b16(&sl, 0xFFFF); b16(&sl, nfeat); for (i = 0; i < nfeat; i++) b16(&sl, i);
      } }
    /* FeatureList */
    b16(&fl, nfeat);
    for (i = 0; i < nfeat; i++) { bput(&fl, tags[i], 4); b16(&fl, 2 + 6 * nfeat + 6 * i); }
    for (i = 0; i < nfeat; i++) { b16(&fl, 0); b16(&fl, 1); b16(&fl, i); }
    /* LookupList */
    b16(&ll, nfeat);
    { size_t pos = 2 + 2 * nfeat; for (i = 0; i < nfeat; i++) { b16(&ll, pos); pos += 8 + lookups[i].n; } }
    for (i = 0; i < nfeat; i++) { b16(&ll, ltype[i]); b16(&ll, 0); b16(&ll, 1); b16(&ll, 8); bput(&ll, lookups[i].p, lookups[i].n); }
    b32(o, 0x00010000); b16(o, 10); b16(o, 10 + sl.n); b16(o, 10 + sl.n + fl.n);
    bput(o, sl.p, sl.n); bput(o, fl.p, fl.n); bput(o, ll.p, ll.n);
    free(sl.p); free(fl.p); free(ll.p);
}
static void build_gsub(const ttf_spec *s, buf *o) {
    int n = s->ngsub, i; ttf_feature f[32]; char tags[32][4]; buf lk[32]; int lt[32];
    if (n > 32) n = 32;
    memcpy(f, s->gsub, n * sizeof f[0]);
    qsort(f, n, sizeof f[0], cmp_feat);
    for (i = 0; i < n; i++) { memcpy(tags[i], f[i].tag, 4); memset(&lk[i], 0, sizeof(buf)); lookup_sub(&f[i], &lk[i]); lt[i] = f[i].type; }
    layout_common(o, n, (const char (*)[4])tags, lk, lt);
    for (i = 0; i < n; i++) free(lk[i].p);
}
static int cmp_kp(const void *a, const void *b) {
    const ttf_kernpair *x = a, *y = b;
    if (x->l != y->l) return (int)x->l - (int)y->l; return (int)x->r - (int)y->r;
}
static void build_gpos(const ttf_spec *s, buf *o) {
    int n = s->ngpos_kern, i; ttf_kernpair *k = (ttf_kernpair *)malloc(n * sizeof *k + 4);
    memcpy(k, s->gpos_kern, n * sizeof *k); qsort(k, n, sizeof *k, cmp_kp);
    unsigned short cov[256]; int first[256], cnt[256], ns = 0;
    for (i = 0; i < n; i++) { if (ns && cov[ns - 1] == k[i].l) cnt[ns - 1]++; else { cov[ns] = k[i].l; first[ns] = i; cnt[ns] = 1; ns++; } }
    buf sub = { 0, 0, 0 }; size_t cov_off = 10 + 2 * ns, pos = cov_off + 4 + 2 * ns; int j;
    b16(&sub, 1); b16(&sub, cov_off); b16(&sub, 4); b16(&sub, 0); b16(&sub, ns);
    for (j = 0; j < ns; j++) { b16(&sub, pos); pos += 2 + 4 * cnt[j]; }
    coverage(&sub, cov, ns);
    for (j = 0; j < ns; j++) { int c; b16(&sub, cnt[j]); for (c = 0; c < cnt[j]; c++) { b16(&sub, k[first[j] + c].r); b16(&sub, (unsigned short)k[first[j] + c].v); } }
    char tags[1][4] = { { 'k', 'e', 'r', 'n' } }; int lt[1] = { 2 };
    layout_common(o, 1, (const char (*)[4])tags, &sub, lt);
    free(sub.p); free(k);
}

static void build_base(const ttf_spec *s, buf *o) {
    /* version 1.0, horizontal axis only: tags [ideo, romn]; script latn, default baseline romn (index 1) */
    buf ax = { 0, 0, 0 };
    b16(&ax, 4); b16(&ax, 14);                              /* axis: BaseTagList @4, BaseScriptList @14 */
    b16(&ax, 2); bput(&ax, "ideo", 4); bput(&ax, "romn", 4);
    b16(&ax, 1); bput(&ax, "latn", 4); b16(&ax, 8);         /* BaseScriptList @14: one record, script @+8 */
    b16(&ax, 6); b16(&ax, 0); b16(&ax, 0);                  /* BaseScript @22: baseValues @+6, no minmax, no langsys */
    b16(&ax, 1); b16(&ax, 2); b16(&ax, 8); b16(&ax, 12);    /* BaseValues @28: default index 1, two coords */
    b16(&ax, 1); b16(&ax, s->base_ideo); b16(&ax, 1); b16(&ax, s->base_romn);
    b32(o, 0x00010000); b16(o, 8); b16(o, 0);
    bput(o, ax.p, ax.n); free(ax.p);
}

static void build_colr_cpal(const ttf_spec *s, buf *colr, buf *cpal) {
    int i;
    b16(colr, 0); b16(colr, s->ncolr_base); b32(colr, 14); b32(colr, 14 + 6 * s->ncolr_base); b16(colr, s->ncolr_layers);
    for (i = 0; i < s->ncolr_base; i++) { b16(colr, s->colr_base[i].gid); b16(colr, s->colr_base[i].first); b16(colr, s->colr_base[i].n); }
    for (i = 0; i < s->ncolr_layers; i++) { b16(colr, s->colr_layers[i].gid); b16(colr, s->colr_layers[i].pal); }
    b16(cpal, 0); b16(cpal, s->npalette); b16(cpal, 1); b16(cpal, s->npalette); b32(cpal, 14); b16(cpal, 0);
    for (i = 0; i < s->npalette; i++) { uint32_t c = s->palette[i]; b8(cpal, c & 0xFF); b8(cpal, (c >> 8) & 0xFF); b8(cpal, (c >> 16) & 0xFF); b8(cpal, c >> 24); }
}

static int cmp_tbl(const void *a, const void *b) { return memcmp(((const tbl *)a)->tag, ((const tbl *)b)->tag, 4); }

uint8_t *ttf_build(const ttf_spec *s, size_t *size) {
    tbllist L; memset(&L, 0, sizeof L);
    int ng = s->nglyphs, i, k;
    if (ng < 1 || !s->glyphs) return NULL;
    /* glyf + loca */
    buf glyf = { 0, 0, 0 }; size_t *goff = (size_t *)malloc((ng + 1) * sizeof(size_t));
    int maxpts = 0, maxcont = 0, maxcpts = 0, maxccont = 0, maxcomp = 0;
    bbox all = { 0, 0, 0, 0, 0 }; int minlsb = 32767, minrsb = 32767, xmaxext = -32768, advmax = 0;
    short *lsbs = (short *)malloc(ng * sizeof(short));
    for (i = 0; i < ng; i++) {
        const ttf_glyph *g = &s->glyphs[i];
        goff[i] = glyf.n;
        bbox b = glyph_bbox(s, i, 0);
        if (g->ncomp > 0) {
            int cp = 0, cc = 0;
            b16(&glyf, 0xFFFF); b16(&glyf, b.x0); b16(&glyf, b.y0); b16(&glyf, b.x1); b16(&glyf, b.y1);
            for (k = 0; k < g->ncomp; k++) {
                const ttf_comp *c = &g->comps[k];
                unsigned fl = 0x0001 | 0x0002 | c->flags;
                if (k + 1 < g->ncomp) fl |= 0x0020;
                if (c->scale) fl |= 0x0008;
                b16(&glyf, fl); b16(&glyf, c->gid); b16(&glyf, c->dx); b16(&glyf, c->dy);
                if (c->scale) b16(&glyf, (unsigned short)c->scale);
                if (c->gid < ng) { const ttf_glyph *cg = &s->glyphs[c->gid]; cp += cg->npts; cc += cg->ncont; }
            }
            if (cp > maxcpts) maxcpts = cp; if (cc > maxccont) maxccont = cc; if (g->ncomp > maxcomp) maxcomp = g->ncomp;
        } else if (g->ncont > 0 && g->npts > 0) {
            b16(&glyf, g->ncont); b16(&glyf, b.x0); b16(&glyf, b.y0); b16(&glyf, b.x1); b16(&glyf, b.y1);
            for (k = 0; k < g->ncont; k++) b16(&glyf, g->ends[k]);
            b16(&glyf, 0);
            for (k = 0; k < g->npts; k++) b8(&glyf, g->pts[k].on ? 1 : 0);
            int px = 0; for (k = 0; k < g->npts; k++) { b16(&glyf, g->pts[k].x - px); px = g->pts[k].x; }
            int py = 0; for (k = 0; k < g->npts; k++) { b16(&glyf, g->pts[k].y - py); py = g->pts[k].y; }
            if (g->npts > maxpts) maxpts = g->npts; if (g->ncont > maxcont) maxcont = g->ncont;
        }
        bpad4(&glyf);
        if (b.ok) { bb_add(&all, b.x0, b.y0); bb_add(&all, b.x1, b.y1); }
        lsbs[i] = g->lsb_set ? g->lsb : (b.ok ? b.x0 : 0);
        if (g->adv > advmax) advmax = g->adv;
        if (b.ok) {
            if (lsbs[i] < minlsb) minlsb = lsbs[i];
            int rsb = g->adv - lsbs[i] - (b.x1 - b.x0); if (rsb < minrsb) minrsb = rsb;
            int ext = lsbs[i] + (b.x1 - b.x0); if (ext > xmaxext) xmaxext = ext;
        }
    }
    goff[ng] = glyf.n;
    if (minlsb == 32767) { minlsb = 0; minrsb = 0; xmaxext = 0; }
    int longloca = s->long_loca || glyf.n / 2 > 0xFFFF;
    int upm = s->upm ? s->upm : 2048;

    if (!(s->omit & TTF_OMIT_GLYF)) {
        buf *g = add_tbl(&L, "glyf"); *g = glyf;
        buf *lo = add_tbl(&L, "loca");
        for (i = 0; i <= ng; i++) { if (longloca) b32(lo, (uint32_t)goff[i]); else b16(lo, (unsigned)(goff[i] / 2)); }
    } else free(glyf.p);
    /* hmtx */
    int nlong = ng;
    if (s->compress_hmtx) { while (nlong > 1 && s->glyphs[nlong - 1].adv == s->glyphs[nlong - 2].adv) nlong--; }
    if (!(s->omit & TTF_OMIT_HMTX)) {
        buf *h = add_tbl(&L, "hmtx");
        for (i = 0; i < ng; i++) { if (i < nlong) b16(h, s->glyphs[i].adv); b16(h, (unsigned short)lsbs[i]); }
    }
    /* head */
    if (!(s->omit & TTF_OMIT_HEAD)) {
        buf *h = add_tbl(&L, "head");
        b32(h, 0x00010000); b32(h, s->revision_fixed); b32(h, 0); b32(h, 0x5F0F3CF5); b16(h, s->head_flags ? s->head_flags : 3); b16(h, upm);
        b32(h, 0); b32(h, 0xD0000000u); b32(h, 0); b32(h, 0xD0000000u);     /* created / modified, fixed value */
        b16(h, all.x0); b16(h, all.y0); b16(h, all.x1); b16(h, all.y1);
        b16(h, s->mac_style); b16(h, 8); b16(h, 2); b16(h, longloca); b16(h, 0);
    }
    if (!(s->omit & TTF_OMIT_HHEA)) {
        buf *h = add_tbl(&L, "hhea");
        b32(h, 0x00010000); b16(h, s->hhea_asc); b16(h, s->hhea_desc); b16(h, s->hhea_gap); b16(h, advmax);
        b16(h, minlsb); b16(h, minrsb); b16(h, xmaxext);
        b16(h, s->italic_angle_fixed ? 1000 : 1); b16(h, s->italic_angle_fixed ? 213 : 0); b16(h, 0);
        for (i = 0; i < 4; i++) b16(h, 0);
        b16(h, 0); b16(h, nlong);
    }
    if (!(s->omit & TTF_OMIT_MAXP)) {
        buf *m = add_tbl(&L, "maxp");
        b32(m, 0x00010000); b16(m, ng); b16(m, maxpts); b16(m, maxcont); b16(m, maxcpts); b16(m, maxccont);
        b16(m, 2); b16(m, 0); b16(m, 0); b16(m, 0); b16(m, 0); b16(m, 0); b16(m, 0); b16(m, maxcomp); b16(m, maxcomp ? 1 : 0);
    }
    if (!(s->omit & TTF_OMIT_OS2)) {
        buf *o = add_tbl(&L, "OS/2"); int v = s->os2_version;
        unsigned first = s->first_char, last = s->last_char;
        if (!first && !last && s->ncmaps && s->cmaps[0].n) {
            first = 0xFFFF; last = 0;
            for (i = 0; i < s->cmaps[0].n; i++) { unsigned c = s->cmaps[0].map[i].cp; if (c > 0xFFFF) c = 0xFFFF; if (c < first) first = c; if (c > last) last = c; }
        }
        b16(o, v); b16(o, s->x_avg_width); b16(o, s->weight); b16(o, s->width); b16(o, s->fs_type);
        b16(o, upm / 2); b16(o, upm / 2); b16(o, 0); b16(o, upm / 7);            /* subscript size/offset */
        b16(o, upm / 2); b16(o, upm / 2); b16(o, 0); b16(o, upm / 3);            /* superscript */
        b16(o, s->strike_size); b16(o, s->strike_pos); b16(o, 0);
        for (i = 0; i < 10; i++) b8(o, 0);
        for (i = 0; i < 4; i++) b32(o, s->unicode_range[i]);
        bput(o, "DWTS", 4); b16(o, s->fs_selection); b16(o, first); b16(o, last);
        b16(o, s->typo_asc); b16(o, s->typo_desc); b16(o, s->typo_gap); b16(o, s->win_asc); b16(o, s->win_desc);
        if (v >= 1) { b32(o, s->codepage_range[0]); b32(o, s->codepage_range[1]); }
        if (v >= 2) { b16(o, s->x_height); b16(o, s->cap_height); b16(o, 0); b16(o, 0x20); b16(o, 1); }
        if (v >= 5) { b16(o, 0); b16(o, 0xFFFF); }
    }
    if (!(s->omit & TTF_OMIT_NAME) && s->nnames) build_name(s, add_tbl(&L, "name"));
    if (!(s->omit & TTF_OMIT_CMAP)) build_cmap(s, add_tbl(&L, "cmap"));
    if (!(s->omit & TTF_OMIT_POST)) {
        buf *p = add_tbl(&L, "post");
        b32(p, 0x00030000); b32(p, s->italic_angle_fixed); b16(p, s->underline_pos); b16(p, s->underline_thick); b32(p, s->is_fixed_pitch);
        b32(p, 0); b32(p, 0); b32(p, 0); b32(p, 0);
    }
    if (s->nkern) {
        buf *kt = add_tbl(&L, "kern"); ttf_kernpair *kp = (ttf_kernpair *)malloc(s->nkern * sizeof *kp);
        memcpy(kp, s->kern, s->nkern * sizeof *kp); qsort(kp, s->nkern, sizeof *kp, cmp_kp);
        unsigned n = s->nkern, sr = 6 * (1u << flog2(n));
        b16(kt, 0); b16(kt, 1); b16(kt, 0); b16(kt, 14 + 6 * n); b16(kt, 0x0001); b16(kt, n); b16(kt, sr); b16(kt, flog2(n)); b16(kt, 6 * n - sr);
        for (i = 0; i < (int)n; i++) { b16(kt, kp[i].l); b16(kt, kp[i].r); b16(kt, (unsigned short)kp[i].v); }
        free(kp);
    }
    if (s->ngpos_kern) build_gpos(s, add_tbl(&L, "GPOS"));
    if (s->ngsub) build_gsub(s, add_tbl(&L, "GSUB"));
    if (s->vertical) {
        buf *vh = add_tbl(&L, "vhea"), *vm;
        int va = s->v_ascent ? s->v_ascent : upm / 2, vd = s->v_descent ? s->v_descent : -upm / 2;
        b32(vh, 0x00010000); b16(vh, va); b16(vh, vd); b16(vh, 0); b16(vh, upm); b16(vh, 0); b16(vh, 0); b16(vh, 0);
        b16(vh, 0); b16(vh, 1); b16(vh, 0); for (i = 0; i < 4; i++) b16(vh, 0); b16(vh, 0); b16(vh, ng);
        vm = add_tbl(&L, "vmtx");
        for (i = 0; i < ng; i++) {
            const ttf_glyph *g = &s->glyphs[i]; bbox b = glyph_bbox(s, i, 0);
            b16(vm, g->vadv ? g->vadv : upm); b16(vm, (unsigned short)(g->tsb_set ? g->tsb : va - (b.ok ? b.y1 : 0)));
        }
    }
    if (s->has_base) build_base(s, add_tbl(&L, "BASE"));
    if (s->ncolr_base) { buf *c = add_tbl(&L, "COLR"); buf *p = add_tbl(&L, "CPAL"); build_colr_cpal(s, c, p); }
    if (s->ngasp) { buf *g = add_tbl(&L, "gasp"); b16(g, 1); b16(g, s->ngasp); for (i = 0; i < s->ngasp; i++) { b16(g, s->gasp[i].max_ppem); b16(g, s->gasp[i].behavior); } }
    for (i = 0; i < s->nraw; i++) { buf *r = add_tbl(&L, s->raw[i].tag); bput(r, s->raw[i].data, s->raw[i].len); }
    free(goff); free(lsbs);

    qsort(L.t, L.n, sizeof(tbl), cmp_tbl);
    buf out = { 0, 0, 0 }; int nt = L.n;
    unsigned sr = 16 * (1u << flog2(nt ? nt : 1));
    b32(&out, s->sfnt_version ? s->sfnt_version : 0x00010000); b16(&out, nt); b16(&out, sr); b16(&out, flog2(nt ? nt : 1)); b16(&out, nt * 16 - sr);
    size_t off = 12 + 16 * nt; size_t headoff = 0;
    for (i = 0; i < nt; i++) {
        bput(&out, L.t[i].tag, 4); b32(&out, checksum(L.t[i].d.p, L.t[i].d.n)); b32(&out, (uint32_t)off); b32(&out, (uint32_t)L.t[i].d.n);
        if (!memcmp(L.t[i].tag, "head", 4)) headoff = off;
        off += (L.t[i].d.n + 3) & ~(size_t)3;
    }
    for (i = 0; i < nt; i++) { bput(&out, L.t[i].d.p, L.t[i].d.n); bpad4(&out); free(L.t[i].d.p); }
    if (headoff && out.n >= headoff + 12) bset32(&out, headoff + 8, 0xB1B0AFBAu - checksum(out.p, out.n));
    *size = out.n;
    return out.p;
}
uint8_t *ttf_build_default(size_t *size) { ttf_spec s; ttf_default_spec(&s); return ttf_build(&s, size); }
uint8_t *ttf_build_styled(const char *family, int weight, int width, int italic, size_t *size) {
    ttf_spec s; ttf_style_spec(&s, family, weight, width, italic); return ttf_build(&s, size);
}

/* ---- containers ---------------------------------------------------------- */
const uint8_t *ttf_find_table(const uint8_t *blob, size_t len, const char *tag, unsigned *tlen) {
    size_t base = 0; int i, n;
    if (len < 12) return NULL;
    if (!memcmp(blob, "ttcf", 4)) { if (len < 16) return NULL; base = rd32(blob + 12); if (base + 12 > len) return NULL; }
    n = rd16(blob + base + 4);
    for (i = 0; i < n; i++) {
        size_t e = base + 12 + 16 * (size_t)i;
        if (e + 16 > len) return NULL;
        if (!memcmp(blob + e, tag, 4)) {
            size_t o = rd32(blob + e + 8), l = rd32(blob + e + 12);
            if (o > len || l > len - o) return NULL;
            if (tlen) *tlen = (unsigned)l;
            return blob + o;
        }
    }
    return NULL;
}
uint8_t *ttf_build_ttc(const uint8_t *const *blobs, const size_t *sizes, int n, size_t *size) {
    buf out = { 0, 0, 0 }; int i, t; size_t pos;
    size_t *dir = (size_t *)malloc(n * sizeof(size_t));
    b32(&out, 0x74746366); b32(&out, 0x00010000); b32(&out, n);
    pos = 12 + 4 * n;
    for (i = 0; i < n; i++) { dir[i] = pos; b32(&out, (uint32_t)pos); pos += 12 + 16 * rd16(blobs[i] + 4); }
    for (i = 0; i < n; i++) {
        int nt = rd16(blobs[i] + 4);
        bput(&out, blobs[i], 12);
        for (t = 0; t < nt; t++) { bput(&out, blobs[i] + 12 + 16 * t, 16); }
    }
    for (i = 0; i < n; i++) {
        int nt = rd16(blobs[i] + 4);
        for (t = 0; t < nt; t++) {
            size_t e = dir[i] + 12 + 16 * t, o = rd32(blobs[i] + 12 + 16 * t + 8), l = rd32(blobs[i] + 12 + 16 * t + 12);
            bpad4(&out);
            bset32(&out, e + 8, (uint32_t)out.n);
            bput(&out, blobs[i] + o, l);
        }
    }
    bpad4(&out); free(dir); (void)sizes;
    *size = out.n; return out.p;
}
uint8_t *ttf_wrap_woff(const uint8_t *sfnt, size_t len, size_t *size) {
    buf out = { 0, 0, 0 }; int nt = rd16(sfnt + 4), i;
    size_t off = 44 + 20 * nt;
    b32(&out, 0x774F4646); bput(&out, sfnt, 4); b32(&out, 0); b16(&out, nt); b16(&out, 0); b32(&out, (uint32_t)len);
    b16(&out, 1); b16(&out, 0); b32(&out, 0); b32(&out, 0); b32(&out, 0); b32(&out, 0); b32(&out, 0);
    for (i = 0; i < nt; i++) {
        const uint8_t *e = sfnt + 12 + 16 * i; uint32_t l = rd32(e + 12);
        bput(&out, e, 4); b32(&out, (uint32_t)off); b32(&out, l); b32(&out, l); b32(&out, rd32(e + 4));
        off += (l + 3) & ~3u;
    }
    for (i = 0; i < nt; i++) { const uint8_t *e = sfnt + 12 + 16 * i; bput(&out, sfnt + rd32(e + 8), rd32(e + 12)); bpad4(&out); }
    bset32(&out, 8, (uint32_t)out.n);
    *size = out.n; return out.p;
}
uint8_t *ttf_corrupt(const uint8_t *blob, size_t len, int kind, size_t param, size_t *size) {
    uint8_t *c = (uint8_t *)malloc(len + 16); memcpy(c, blob, len); *size = len;
    size_t e = 12 + 16 * param;
    switch (kind) {
    case TTF_CORRUPT_TRUNCATE: *size = param < len ? param : len; break;
    case TTF_CORRUPT_BAD_MAGIC: memcpy(c, "XXXX", 4); break;
    case TTF_CORRUPT_NUMTABLES: c[4] = param >> 8; c[5] = (uint8_t)param; break;
    case TTF_CORRUPT_TABLE_OFFSET: wr32(c + e + 8, (uint32_t)len + 0x1000); break;
    case TTF_CORRUPT_TABLE_LENGTH: wr32(c + e + 12, (uint32_t)len + 0x1000); break;
    case TTF_CORRUPT_TABLE_CHECKSUM: c[e + 4] ^= 0xFF; break;
    case TTF_CORRUPT_ZERO_TABLE: wr32(c + e + 12, 0); break;
    case TTF_CORRUPT_ZERO_FILL: { size_t o = rd32(c + e + 8), l = rd32(c + e + 12); if (o <= len && l <= len - o) memset(c + o, 0, l); break; }
    case TTF_CORRUPT_HEAD_MAGIC: { unsigned hl; const uint8_t *h = ttf_find_table(c, len, "head", &hl); if (h && hl >= 16) wr32((uint8_t *)h + 12, 0xDEADBEEF); break; }
    }
    return c;
}
