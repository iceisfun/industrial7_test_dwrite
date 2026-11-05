#include "ot.h"

void ot_tagstr(UINT32 t, char out[5]) {
    int i;
    for (i = 0; i < 4; i++) { unsigned char c = (unsigned char)(t >> (8 * i)); out[i] = (c >= 0x20 && c < 0x7f) ? (char)c : '?'; }
    out[4] = 0;
}
uint32_t ot_checksum(const uint8_t *p, uint32_t len) {
    uint32_t s = 0, i;
    for (i = 0; i + 4 <= len; i += 4) s += BE32(p + i);
    if (i < len) { uint32_t v = 0; int k; for (k = 0; k < 4; k++) v = (v << 8) | (i + k < len ? p[i + k] : 0); s += v; }
    return s;
}
int ot_ttc_count(const uint8_t *b, size_t len) { return (len >= 16 && !memcmp(b, "ttcf", 4)) ? (int)BE32(b + 8) : -1; }
int ot_parse_dir(const uint8_t *b, size_t len, uint32_t index, ot_dir *d) {
    size_t base = 0; int n, i;
    memset(d, 0, sizeof *d);
    if (len < 12) return -1;
    if (!memcmp(b, "ttcf", 4)) {
        uint32_t nf;
        if (len < 16) return -1;
        nf = BE32(b + 8);
        if (index >= nf || 12 + 4 * (size_t)index + 4 > len) return -1;
        base = BE32(b + 12 + 4 * index);
    } else if (index != 0) return -1;
    if (base + 12 > len) return -1;
    d->sfnt_off = (uint32_t)base; d->version = BE32(b + base);
    n = BE16(b + base + 4);
    if (n > 96 || base + 12 + 16 * (size_t)n > len) return -1;
    for (i = 0; i < n; i++) {
        const uint8_t *e = b + base + 12 + 16 * i;
        d->r[i].tag = (UINT32)e[0] | (UINT32)e[1] << 8 | (UINT32)e[2] << 16 | (UINT32)e[3] << 24;
        d->r[i].sum = BE32(e + 4); d->r[i].off = BE32(e + 8); d->r[i].len = BE32(e + 12);
    }
    d->n = n;
    return 0;
}
const ot_rec *ot_find(const ot_dir *d, UINT32 tag) { int i; for (i = 0; i < d->n; i++) if (d->r[i].tag == tag) return &d->r[i]; return NULL; }

/* ---- cmap ---- */
static const uint8_t *cmap_sub(const uint8_t *c, size_t len, int plat, int enc, size_t *sublen) {
    int n, i;
    if (len < 4) return NULL;
    n = BE16(c + 2);
    for (i = 0; i < n && 4 + 8 * (size_t)i + 8 <= len; i++) {
        const uint8_t *r = c + 4 + 8 * i;
        if (BE16(r) == plat && BE16(r + 2) == enc) {
            uint32_t o = BE32(r + 4);
            if (o + 4 > len) return NULL;
            if (sublen) *sublen = len - o;
            return c + o;
        }
    }
    return NULL;
}
int ot_cmap_has(const uint8_t *c, size_t len, int plat, int enc, int *fmt) {
    const uint8_t *s = cmap_sub(c, len, plat, enc, NULL);
    if (!s) return 0;
    if (fmt) *fmt = BE16(s);
    return 1;
}
long ot_cmap_lookup(const uint8_t *c, size_t len, int plat, int enc, uint32_t cp) {
    size_t sl; const uint8_t *s = cmap_sub(c, len, plat, enc, &sl);
    int fmt;
    if (!s) return -2;
    fmt = BE16(s);
    if (fmt == 0) return cp < 256 && 6 + cp < sl ? s[6 + cp] : 0;
    if (fmt == 6) {
        unsigned first = BE16(s + 6), cnt = BE16(s + 8);
        if (cp < first || cp >= first + cnt) return 0;
        return BE16(s + 10 + 2 * (cp - first));
    }
    if (fmt == 4) {
        unsigned segx2 = BE16(s + 6), i;
        const uint8_t *endc = s + 14, *startc = endc + segx2 + 2, *delta = startc + segx2, *ro = delta + segx2;
        if (cp > 0xFFFF) return 0;
        for (i = 0; i < segx2 / 2; i++) {
            unsigned e = BE16(endc + 2 * i), st = BE16(startc + 2 * i);
            if (cp > e) continue;
            if (cp < st) return 0;
            {
                unsigned off = BE16(ro + 2 * i); int dl = BE16S(delta + 2 * i);
                if (off == 0) return (long)((cp + dl) & 0xFFFF);
                {
                    const uint8_t *g = ro + 2 * i + off + 2 * (cp - st);
                    unsigned gid;
                    if (g + 2 > s + sl) return 0;
                    gid = BE16(g);
                    return gid ? (long)((gid + dl) & 0xFFFF) : 0;
                }
            }
        }
        return 0;
    }
    if (fmt == 12 || fmt == 13) {
        uint32_t ng = BE32(s + 12), i;
        for (i = 0; i < ng && 16 + 12 * (size_t)i + 12 <= sl; i++) {
            const uint8_t *g = s + 16 + 12 * i; uint32_t st = BE32(g), en = BE32(g + 4), gid = BE32(g + 8);
            if (cp >= st && cp <= en) return fmt == 12 ? (long)(gid + (cp - st)) : (long)gid;
        }
        return 0;
    }
    return -3;
}
int ot_cmap_pick(const uint8_t *c, size_t len, int *plat, int *enc) {
    static const int pe[][2] = { {3, 10}, {0, 4}, {0, 6}, {3, 1}, {0, 3}, {0, 2}, {0, 1}, {0, 0} };
    unsigned i;
    for (i = 0; i < sizeof pe / sizeof pe[0]; i++) if (ot_cmap_has(c, len, pe[i][0], pe[i][1], NULL)) { *plat = pe[i][0]; *enc = pe[i][1]; return 1; }
    return 0;
}

/* ---- glyphs ---- */
int ot_font_init(ot_font *f, const uint8_t *b, size_t len, const ot_dir *d) {
    const ot_rec *r;
#define GET(field, tag, lenfield) r = ot_find(d, TG(tag)); f->field = NULL; if (r && (size_t)r->off + r->len <= len) { f->field = b + r->off; lenfield } 
    uint32_t dummy;
    memset(f, 0, sizeof *f);
    GET(head, "head", dummy = r->len;)
    GET(hhea, "hhea", dummy = r->len;)
    GET(maxp, "maxp", dummy = r->len;)
    GET(hmtx, "hmtx", f->hmtx_len = r->len;)
    GET(loca, "loca", f->loca_len = r->len;)
    GET(glyf, "glyf", f->glyf_len = r->len;)
#undef GET
    if (!f->head || !f->hhea || !f->maxp || !f->hmtx) return -1;
    f->upm = BE16(f->head + 18); f->loca_fmt = BE16S(f->head + 50);
    f->nglyphs = BE16(f->maxp + 4); f->nhm = BE16(f->hhea + 34);
    return 0;
}
int ot_glyph_get(const ot_font *f, int gid, ot_glyph *g) {
    memset(g, 0, sizeof *g);
    if (gid < 0 || gid >= f->nglyphs || f->nhm < 1) return -1;
    if (gid < f->nhm) { if ((size_t)4 * gid + 4 > f->hmtx_len) return -1; g->adv = BE16(f->hmtx + 4 * gid); g->lsb = BE16S(f->hmtx + 4 * gid + 2); }
    else {
        size_t o = 4 * (size_t)f->nhm + 2 * (size_t)(gid - f->nhm);
        g->adv = BE16(f->hmtx + 4 * (f->nhm - 1));
        if (o + 2 > f->hmtx_len) return -1;
        g->lsb = BE16S(f->hmtx + o);
    }
    g->ok = 1;
    if (f->loca && f->glyf) {
        uint32_t o0, o1;
        if (f->loca_fmt == 0) { if (2 * (size_t)gid + 4 > f->loca_len) return 0; o0 = 2u * BE16(f->loca + 2 * gid); o1 = 2u * BE16(f->loca + 2 * gid + 2); }
        else { if (4 * (size_t)gid + 8 > f->loca_len) return 0; o0 = BE32(f->loca + 4 * gid); o1 = BE32(f->loca + 4 * gid + 4); }
        if (o1 > o0 && o0 + 10 <= f->glyf_len) {
            const uint8_t *gp = f->glyf + o0;
            g->has_outline = 1; g->ncont = BE16S(gp);
            g->x0 = BE16S(gp + 2); g->y0 = BE16S(gp + 4); g->x1 = BE16S(gp + 6); g->y1 = BE16S(gp + 8);
        }
    }
    return 0;
}

/* ---- name ---- */
int ot_name_count(const uint8_t *n, size_t len) { return len >= 6 ? BE16(n + 2) : -1; }
int ot_name_get(const uint8_t *n, size_t len, int plat, int enc, int lang, int id, uint8_t *out, int cap) {
    int cnt, i; unsigned so;
    if (len < 6) return -1;
    cnt = BE16(n + 2); so = BE16(n + 4);
    for (i = 0; i < cnt && 6 + 12 * (size_t)i + 12 <= len; i++) {
        const uint8_t *r = n + 6 + 12 * i;
        if (BE16(r) == plat && BE16(r + 2) == enc && BE16(r + 4) == lang && BE16(r + 6) == id) {
            unsigned l = BE16(r + 8), o = so + BE16(r + 10);
            if (o + l > len || (int)l > cap) return -1;
            memcpy(out, n + o, l); return (int)l;
        }
    }
    return -1;
}

/* ---- kern ---- */
int ot_kern_find(const uint8_t *k, size_t len, unsigned l, unsigned r, int *v) {
    unsigned nt, t; size_t off = 4;
    if (len < 4 || BE16(k) != 0) return 0;
    nt = BE16(k + 2);
    for (t = 0; t < nt && off + 14 <= len; t++) {
        unsigned sl = BE16(k + off + 2), cov = BE16(k + off + 4);
        if (cov == 0x0001) {
            unsigned np = BE16(k + off + 6), i; const uint8_t *p = k + off + 14;
            if (off + 14 + 6 * (size_t)np > len) np = (unsigned)((len - off - 14) / 6);
            for (i = 0; i < np; i++) if (BE16(p + 6 * i) == l && BE16(p + 6 * i + 2) == r) { *v = BE16S(p + 6 * i + 4); return 1; }
            return 0;
        }
        off += sl ? sl : 0xFFFFFFF;
    }
    return 0;
}

/* ---- fixtures ---- */
int synth_open(synth *s, const ttf_spec *spec) {
    IDWriteFactory *f = dw_shared();
    ttf_spec def;
    if (!f) { s->hr = E_FAIL; return 0; }
    if (!spec) { ttf_default_spec(&def); spec = &def; }
    s->blob = ttf_build(spec, &s->n);
    if (!s->blob) { s->hr = E_FAIL; return 0; }
    s->hr = dwx_font_open(&s->h, f, s->blob, s->n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (s->hr != S_OK || !s->h.face) return 0;
    if (ot_parse_dir(s->blob, s->n, 0, &s->dir)) { s->hr = E_FAIL; return 0; }
    { dwx_mem_stream *st = synth_stream(s); s->base_out = st ? st->outstanding : 0; }   /* fragments the face itself keeps */
    return 1;
}
void synth_close(synth *s) { dwx_font_close(&s->h); free(s->blob); s->blob = NULL; }
dwx_mem_stream *synth_stream(synth *s) { return s->h.ld ? s->h.ld->last_stream : NULL; }

tbl tget(IDWriteFontFace *face, UINT32 tag) {
    tbl t; t.d = (const void *)(ULONG_PTR)1; t.n = 0xDEADBEEF; t.ctx = (void *)(ULONG_PTR)1; t.ex = 0x55;
    t.hr = IDWriteFontFace_TryGetFontTable(face, tag, &t.d, &t.n, &t.ctx, &t.ex);
    return t;
}
void trel(IDWriteFontFace *face, tbl *t) { if (t->ctx) IDWriteFontFace_ReleaseFontTable(face, t->ctx); t->ctx = NULL; }

uint8_t *face_file_bytes(IDWriteFontFace *face, size_t *len) {
    UINT32 n = 0; IDWriteFontFile *file = NULL; IDWriteFontFileLoader *ld = NULL; const void *key = NULL; UINT32 ks = 0;
    IDWriteFontFileStream *st = NULL; UINT64 sz = 0; const void *p = NULL; void *ctx = NULL; uint8_t *out = NULL;
    if (len) *len = 0;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, NULL)) || n < 1) return NULL;
    n = 1;
    if (FAILED(IDWriteFontFace_GetFiles(face, &n, &file)) || !file) return NULL;
    if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(file, &key, &ks)) && SUCCEEDED(IDWriteFontFile_GetLoader(file, &ld)) && ld &&
        SUCCEEDED(IDWriteFontFileLoader_CreateStreamFromKey(ld, key, ks, &st)) && st &&
        SUCCEEDED(IDWriteFontFileStream_GetFileSize(st, &sz)) && sz > 0 && sz < (UINT64)512 * 1024 * 1024 &&
        SUCCEEDED(IDWriteFontFileStream_ReadFileFragment(st, &p, 0, sz, &ctx))) {
        out = (uint8_t *)malloc((size_t)sz);
        if (out) { memcpy(out, p, (size_t)sz); if (len) *len = (size_t)sz; }
        IDWriteFontFileStream_ReleaseFileFragment(st, ctx);
    }
    if (st) IDWriteFontFileStream_Release(st);
    if (ld) IDWriteFontFileLoader_Release(ld);
    IDWriteFontFile_Release(file);
    return out;
}

int compare_all_tables(IDWriteFontFace *face, const uint8_t *blob, size_t len, const ot_dir *d, int *nt) {
    int i, bad = 0, cnt = 0;
    for (i = 0; i < d->n; i++) {
        const ot_rec *r = &d->r[i];
        const ot_rec *first = ot_find(d, r->tag);
        tbl t;
        if (r->off > len || r->len > len - r->off) continue;
        t = tget(face, r->tag); cnt++;
        if (t.hr != S_OK || !t.ex || t.n != first->len || (first->len && (!t.d || memcmp(t.d, blob + first->off, first->len)))) bad++;
        trel(face, &t);
    }
    if (nt) *nt = cnt;
    return bad;
}
