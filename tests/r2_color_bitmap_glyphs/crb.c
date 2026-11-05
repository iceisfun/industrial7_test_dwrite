#include "crb.h"

/* ------------------------------------------------------------------------------------------ buffer */
void crb_put(crb_buf *b, const void *d, size_t n) {
    if (b->n + n > b->cap) { size_t c = b->cap ? b->cap : 256; while (c < b->n + n) c *= 2; b->p = (uint8_t *)realloc(b->p, c); b->cap = c; }
    if (n) { if (d) memcpy(b->p + b->n, d, n); else memset(b->p + b->n, 0, n); }
    b->n += n;
}
void crb_u8(crb_buf *b, unsigned v) { uint8_t c = (uint8_t)v; crb_put(b, &c, 1); }
void crb_u16(crb_buf *b, unsigned v) { crb_u8(b, v >> 8); crb_u8(b, v); }
void crb_u32(crb_buf *b, unsigned v) { crb_u16(b, v >> 16); crb_u16(b, v & 0xFFFF); }
void crb_zero(crb_buf *b, size_t n) { crb_put(b, NULL, n); }
void crb_set16(crb_buf *b, size_t at, unsigned v) { b->p[at] = (uint8_t)(v >> 8); b->p[at + 1] = (uint8_t)v; }
void crb_set32(crb_buf *b, size_t at, unsigned v) { crb_set16(b, at, v >> 16); crb_set16(b, at + 2, v & 0xFFFF); }

static uint32_t crc32_(const uint8_t *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu; size_t i; int k;
    for (i = 0; i < n; i++) { c ^= p[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1)); }
    return ~c;
}
static void png_chunk(crb_buf *b, const char *type, const uint8_t *d, size_t n) {
    size_t at; crb_u32(b, (unsigned)n); at = b->n; crb_put(b, type, 4); crb_put(b, d, n); crb_u32(b, crc32_(b->p + at, 4 + n));
}
uint8_t *crb_png(unsigned w, unsigned h, unsigned tag, size_t *n) {
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    crb_buf b = { 0 }, ih = { 0 }; uint8_t t[4];
    crb_put(&b, sig, 8);
    crb_u32(&ih, w); crb_u32(&ih, h); crb_u8(&ih, 8); crb_u8(&ih, 6); crb_u8(&ih, 0); crb_u8(&ih, 0); crb_u8(&ih, 0);
    png_chunk(&b, "IHDR", ih.p, ih.n); free(ih.p);
    t[0] = (uint8_t)(tag >> 24); t[1] = (uint8_t)(tag >> 16); t[2] = (uint8_t)(tag >> 8); t[3] = (uint8_t)tag;
    png_chunk(&b, "prVt", t, 4);
    png_chunk(&b, "IEND", NULL, 0);
    *n = b.n; return b.p;
}
uint8_t *crb_jpeg(unsigned w, unsigned h, unsigned tag, int style, size_t *n) {
    crb_buf b = { 0 }; int i;
    crb_u16(&b, 0xFFD8);
    if (style == 0) {
        crb_u16(&b, 0xFFE0); crb_u16(&b, 16); crb_put(&b, "JFIF", 5); crb_u8(&b, 1); crb_u8(&b, 1); crb_u8(&b, 0); crb_u16(&b, 1); crb_u16(&b, 1); crb_u8(&b, 0); crb_u8(&b, 0);
    } else if (style == 1) {
        crb_u16(&b, 0xFFDB); crb_u16(&b, 67); crb_u8(&b, 0); for (i = 0; i < 64; i++) crb_u8(&b, 1 + (unsigned)i);
        crb_u16(&b, 0xFFC4); crb_u16(&b, 19); crb_u8(&b, 0); for (i = 0; i < 16; i++) crb_u8(&b, 0);
    } else if (style == 2) {
        crb_u8(&b, 0x12); crb_u8(&b, 0x34); crb_u8(&b, 0xFF); crb_u8(&b, 0xFF); crb_u8(&b, 0xFF);   /* junk then fill bytes before the marker */
    }
    if (style == 3) {
        crb_u16(&b, 0xFFDA); crb_u16(&b, 8); for (i = 0; i < 6; i++) crb_u8(&b, 0);
    }
    crb_u16(&b, style == 1 ? 0xFFC2 : style == 2 ? 0xFFC1 : 0xFFC0);
    crb_u16(&b, 17); crb_u8(&b, 8); crb_u16(&b, h); crb_u16(&b, w); crb_u8(&b, 3);
    for (i = 0; i < 3; i++) { crb_u8(&b, 1 + (unsigned)i); crb_u8(&b, 0x11); crb_u8(&b, 0); }
    if (style != 3) { crb_u16(&b, 0xFFDA); crb_u16(&b, 8); for (i = 0; i < 6; i++) crb_u8(&b, 0); }
    crb_u32(&b, tag);
    crb_u16(&b, 0xFFD9);
    *n = b.n; return b.p;
}
static void tf16(crb_buf *b, int be, unsigned v) { if (be) crb_u16(b, v); else { crb_u8(b, v); crb_u8(b, v >> 8); } }
static void tf32(crb_buf *b, int be, unsigned v) { if (be) crb_u32(b, v); else { tf16(b, 0, v & 0xFFFF); tf16(b, 0, v >> 16); } }
static void tf_entry(crb_buf *b, int be, unsigned tag, int type, unsigned v) {
    tf16(b, be, tag); tf16(b, be, (unsigned)type); tf32(b, be, 1);
    if (type == 1 || type == 6) { crb_u8(b, v); crb_zero(b, 3); }
    else if (type == 3 || type == 8) { tf16(b, be, v); crb_zero(b, 2); }
    else tf32(b, be, v);
}
uint8_t *crb_tiff(unsigned w, unsigned h, int be, int wtype, int htype, int extra_first, unsigned tag, size_t *n) {
    crb_buf b = { 0 }; unsigned cnt = extra_first ? 3 : 2;
    crb_u8(&b, be ? 'M' : 'I'); crb_u8(&b, be ? 'M' : 'I');
    if (be) { crb_u16(&b, 42); } else { crb_u8(&b, 42); crb_u8(&b, 0); }
    tf32(&b, be, 8);
    tf16(&b, be, cnt);
    if (extra_first) tf_entry(&b, be, 0xFE, 4, 0);
    tf_entry(&b, be, 0x100, wtype, w);
    tf_entry(&b, be, 0x101, htype, h);
    tf32(&b, be, 0);
    crb_u32(&b, tag);
    *n = b.n; return b.p;
}

/* ------------------------------------------------------------------------------------------ CBLC / CBDT */
static void put_met(crb_buf *b, const crb_met *m, int big) {
    crb_u8(b, m->h); crb_u8(b, m->w); crb_u8(b, (uint8_t)m->bx); crb_u8(b, (uint8_t)m->by); crb_u8(b, m->adv);
    if (big) { crb_u8(b, (uint8_t)m->vbx); crb_u8(b, (uint8_t)m->vby); crb_u8(b, m->vadv); }
}
static void put_linemetrics(crb_buf *b, unsigned ppem) {
    crb_u8(b, (unsigned)(ppem * 3 / 4)); crb_u8(b, (uint8_t)-(int)(ppem / 4)); crb_u8(b, ppem); crb_u8(b, 1); crb_u8(b, 0); crb_u8(b, 0);
    crb_u8(b, 0); crb_u8(b, 0); crb_u8(b, 0); crb_u8(b, 0); crb_u8(b, 0); crb_u8(b, 0);
}
static size_t rec_hdr(int imgfmt) { return imgfmt == 17 ? 9 : imgfmt == 18 ? 12 : imgfmt == 19 ? 4 : 5; }
void crb_cbtables(const crb_cbstrike *s, int ns, uint8_t **cblc, size_t *nlc, uint8_t **cbdt, size_t *ndt) {
    crb_buf lc = { 0 }, dt = { 0 }, sub = { 0 }; int i, g; size_t *arr = (size_t *)calloc((size_t)ns, sizeof(size_t));
    unsigned *imgoff = (unsigned *)calloc((size_t)ns, sizeof(unsigned)); size_t *subsz = (size_t *)calloc((size_t)ns, sizeof(size_t));
    uint8_t **subbuf = (uint8_t **)calloc((size_t)ns, sizeof(uint8_t *));
    crb_u16(&dt, 3); crb_u16(&dt, 0);
    for (i = 0; i < ns; i++) {
        const crb_cbstrike *k = &s[i]; size_t rec_at;
        unsigned off = 0;
        memset(&sub, 0, sizeof sub);
        imgoff[i] = (unsigned)dt.n;
        crb_u16(&sub, (unsigned)k->idxfmt); crb_u16(&sub, (unsigned)k->imgfmt); crb_u32(&sub, imgoff[i]);
        /* image records */
        rec_at = dt.n;
        for (g = 0; g < k->ng; g++) {
            const crb_cbglyph *gl = &k->g[g];
            if (k->imgfmt == 17 || k->imgfmt == 1) put_met(&dt, &gl->m, 0);
            else if (k->imgfmt == 18) put_met(&dt, &gl->m, 1);
            if (k->imgfmt != 1) crb_u32(&dt, (unsigned)gl->imglen);
            crb_put(&dt, gl->img, gl->imglen);
        }
        (void)rec_at;
        if (k->idxfmt == 1) {
            off = 0;
            for (g = 0; g < k->ng; g++) {
                crb_u32(&sub, off);
                off += (unsigned)rec_hdr(k->imgfmt) + (unsigned)k->g[g].imglen;
            }
            crb_u32(&sub, off);
        } else if (k->idxfmt == 3) {
            off = 0;
            for (g = 0; g < k->ng; g++) {
                crb_u16(&sub, off);
                off += (unsigned)rec_hdr(k->imgfmt) + (unsigned)k->g[g].imglen;
            }
            crb_u16(&sub, off);
            if (sub.n & 3) crb_zero(&sub, 4 - (sub.n & 3));
        } else {   /* idxfmt 2 */
            crb_u32(&sub, (unsigned)rec_hdr(k->imgfmt) + (unsigned)k->g[0].imglen);
            put_met(&sub, &k->sub, 1);
        }
        subbuf[i] = sub.p; subsz[i] = sub.n;
    }
    crb_u16(&lc, 3); crb_u16(&lc, 0); crb_u32(&lc, (unsigned)ns);
    {   /* BitmapSize records first, then per strike: one array entry + the subtable */
        size_t pos = 8 + 48 * (size_t)ns;
        for (i = 0; i < ns; i++) { arr[i] = pos; pos += 8 + subsz[i]; }
    }
    for (i = 0; i < ns; i++) {
        const crb_cbstrike *k = &s[i];
        crb_u32(&lc, (unsigned)arr[i]); crb_u32(&lc, (unsigned)(8 + subsz[i])); crb_u32(&lc, 1); crb_u32(&lc, 0);
        put_linemetrics(&lc, k->ppem_x); put_linemetrics(&lc, k->ppem_x);
        crb_u16(&lc, k->g[0].gid); crb_u16(&lc, k->g[0].gid + (unsigned)k->ng - 1);
        crb_u8(&lc, k->ppem_x); crb_u8(&lc, k->ppem_y); crb_u8(&lc, k->depth); crb_u8(&lc, 1);
    }
    for (i = 0; i < ns; i++) {
        const crb_cbstrike *k = &s[i];
        crb_u16(&lc, k->g[0].gid); crb_u16(&lc, k->g[0].gid + (unsigned)k->ng - 1); crb_u32(&lc, 8);
        crb_put(&lc, subbuf[i], subsz[i]); free(subbuf[i]);
    }
    free(arr); free(imgoff); free(subsz); free(subbuf);
    *cblc = lc.p; *nlc = lc.n; *cbdt = dt.p; *ndt = dt.n;
}

/* ------------------------------------------------------------------------------------------ SVG */
uint8_t *crb_svg(const crb_svgdoc *d, int n, size_t *len) {
    crb_buf b = { 0 }, docs = { 0 }; int i, j; unsigned *doff = (unsigned *)calloc((size_t)n, sizeof(unsigned));
    for (i = 0; i < n; i++) {
        int dup = -1;
        for (j = 0; j < i; j++) if (d[j].doc == d[i].doc && d[j].len == d[i].len) { dup = j; break; }
        if (dup >= 0) doff[i] = doff[dup]; else { doff[i] = (unsigned)docs.n; crb_put(&docs, d[i].doc, d[i].len); }
    }
    crb_u16(&b, 0); crb_u32(&b, 10); crb_u32(&b, 0);
    crb_u16(&b, (unsigned)n);
    for (i = 0; i < n; i++) { crb_u16(&b, d[i].first); crb_u16(&b, d[i].last); crb_u32(&b, 2 + 12 * (unsigned)n + doff[i]); crb_u32(&b, (unsigned)d[i].len); }
    crb_put(&b, docs.p, docs.n); free(docs.p); free(doff);
    *len = b.n; return b.p;
}

/* ------------------------------------------------------------------------------------------ sbix */
uint8_t *crb_sbix(const crb_sbstrike *s, int ns, unsigned nglyphs, unsigned version, size_t *n) {
    crb_buf b = { 0 }; int i, r; unsigned g; size_t *so = (size_t *)calloc((size_t)ns, sizeof(size_t));
    crb_u16(&b, version); crb_u16(&b, 1); crb_u32(&b, (unsigned)ns);
    for (i = 0; i < ns; i++) crb_u32(&b, 0);
    for (i = 0; i < ns; i++) {
        const crb_sbstrike *k = &s[i]; size_t start = b.n, tab; unsigned cur;
        so[i] = start; crb_set32(&b, 8 + 4 * (size_t)i, (unsigned)start);
        crb_u16(&b, k->ppem); crb_u16(&b, k->ppi);
        tab = b.n; crb_zero(&b, 4 * ((size_t)nglyphs + 1));
        cur = (unsigned)(b.n - start); r = 0;
        for (g = 0; g < nglyphs; g++) {
            crb_set32(&b, tab + 4 * (size_t)g, cur);
            if (r < k->nr && k->r[r].gid == g) {
                const crb_sbrec *e = &k->r[r++]; crb_buf rec = { 0 };
                crb_u16(&rec, (unsigned)(uint16_t)e->ox); crb_u16(&rec, (unsigned)(uint16_t)e->oy); crb_u32(&rec, e->type); crb_put(&rec, e->data, e->len);
                if (e->trunc > 0) rec.n = (size_t)e->trunc;
                crb_put(&b, rec.p, rec.n); cur += (unsigned)rec.n; free(rec.p);
            }
        }
        crb_set32(&b, tab + 4 * (size_t)nglyphs, cur);
    }
    free(so);
    *n = b.n; return b.p;
}

/* ------------------------------------------------------------------------------------------ fonts */
uint8_t *crb_blob(const ttf_rawtable *raw, int nraw, int nglyphs, size_t *n) {
    ttf_spec s; static ttf_glyph gl[600]; int dn = 0, i; const ttf_glyph *dg = ttf_default_glyphs(&dn);
    ttf_default_spec(&s);
    if (nglyphs > dn) {
        for (i = 0; i < dn; i++) gl[i] = dg[i];
        for (i = dn; i < nglyphs && i < 600; i++) { gl[i] = dg[TTF_GID_EMPTY500]; }
        s.glyphs = gl; s.nglyphs = nglyphs;
    }
    s.raw = raw; s.nraw = nraw;
    return ttf_build(&s, n);
}
IDWriteFactory4 *crb_f4(void) { return (IDWriteFactory4 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 4); }
IDWriteFactory2 *crb_f2(void) { return (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2); }
int crb_open(crb *x, uint8_t *blob, size_t n) {
    IDWriteFactory *f = dw_shared(); HRESULT hr;
    memset(x, 0, sizeof *x); x->blob = blob; x->n = n;
    if (!blob) { t_skip("ttf_build failed"); return 0; }
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    {   /* developer aid: CRB_DUMP_DIR=<dir> writes every synthetic font to <dir>\fNNN.ttf so the fixtures can be parsed offline */
        const char *dir = getenv("CRB_DUMP_DIR"); static int seq;
        if (dir && *dir) { char p[512]; FILE *fp; snprintf(p, sizeof p, "%s\\f%03d.ttf", dir, ++seq); fp = fopen(p, "wb"); if (fp) { fwrite(blob, 1, n, fp); fclose(fp); } }
    }
    hr = dwx_font_open(&x->h, f, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (FAILED(hr) || !x->h.face) { t_skip("cannot open synthetic face: %s", dw_hr_name(hr)); return 0; }
    IDWriteFontFace_QueryInterface(x->h.face, &IID_IDWriteFontFace4, (void **)&x->f4);
    IDWriteFontFace_QueryInterface(x->h.face, &IID_IDWriteFontFace2, (void **)&x->f2);
    if (!x->f4) { t_skip("IDWriteFontFace4 unsupported"); return 0; }
    x->ok = 1; return 1;
}
void crb_close(crb *x) {
    DW_RELEASE(x->f4); DW_RELEASE(x->f2); dwx_font_close(&x->h); free(x->blob); memset(x, 0, sizeof *x);
}
const uint8_t *crb_find(const crb *x, const void *needle, size_t len) {
    size_t i; if (!len || len > x->n) return NULL;
    for (i = 0; i + len <= x->n; i++) if (x->blob[i] == ((const uint8_t *)needle)[0] && !memcmp(x->blob + i, needle, len)) return x->blob + i;
    return NULL;
}
unsigned crb_table_offset(const crb *x, const char *tag, unsigned *len) {
    unsigned l = 0; const uint8_t *p = ttf_find_table(x->blob, x->n, tag, &l);
    if (len) *len = l; return p ? (unsigned)(p - x->blob) : 0;
}
UINT32 crb_face_formats(crb *x) { return (UINT32)x->f4->lpVtbl->GetGlyphImageFormats(x->f4); }
HRESULT crb_gif(crb *x, UINT16 g, UINT32 a, UINT32 b, UINT32 *out) {
    DWRITE_GLYPH_IMAGE_FORMATS f = (DWRITE_GLYPH_IMAGE_FORMATS)0xDEADBEEF; HRESULT hr = x->f4->lpVtbl->GetGlyphImageFormats_(x->f4, g, a, b, &f);
    *out = (UINT32)f; return hr;
}
HRESULT crb_gid(crb *x, UINT16 g, UINT32 ppem, UINT32 fmt, DWRITE_GLYPH_IMAGE_DATA *d, void **ctx) {
    return IDWriteFontFace4_GetGlyphImageData(x->f4, g, ppem, (DWRITE_GLYPH_IMAGE_FORMATS)fmt, d, ctx);
}
int crb_is_zero(const void *p, size_t n) { const unsigned char *q = (const unsigned char *)p; size_t i; for (i = 0; i < n; i++) if (q[i]) return 0; return 1; }

/* ------------------------------------------------------------------------------------------ the shared CBDT font
 * strikes in FILE order (deliberately not sorted by ppem):
 *   32x32 depth 32 fmt 17 idx 1  glyphs 2..4        16x16 depth 32 fmt 17 idx 3  glyphs 2..3
 *   24x24 depth 32 fmt 18 idx 1  glyphs 3..5        64x64 depth 32 fmt 19 idx 2  glyphs 6..7
 *   20x20 depth 8 (ignored)      glyphs 8..9        12x14 depth 32 (ignored)     glyphs 10..11       40x40 depth 1 (ignored) glyph 12
 * PNG dimensions differ from the metric sizes on purpose. */
static crb_cbexp g_cbexp[32]; static int g_ncbexp;
static uint8_t *g_cbpng[64]; static size_t g_cbpnglen[64]; static int g_ncbpng;
static const uint8_t *mk_png(unsigned tag, unsigned w, unsigned h, size_t *len) {
    /* allocated once per slot so pointers handed out earlier stay valid across rebuilds */
    if (!g_cbpng[g_ncbpng]) g_cbpng[g_ncbpng] = crb_png(w, h, tag, &g_cbpnglen[g_ncbpng]);
    *len = g_cbpnglen[g_ncbpng]; return g_cbpng[g_ncbpng++];
}
const crb_cbexp *crb_cb_entries(int *n) { size_t l; if (!g_ncbexp) free(crb_cbdt_font(&l)); if (n) *n = g_ncbexp; return g_cbexp; }
uint8_t *crb_cbdt_font(size_t *n) {
    static crb_cbglyph gl[7][4]; crb_cbstrike st[7]; ttf_rawtable raw[2]; uint8_t *lc, *dt, *out; size_t nlc, ndt;
    static const struct { unsigned ppem_x, ppem_y, depth; int imgfmt, idxfmt; unsigned g0; int ng; int accepted; } sp[7] = {
        { 32, 32, 32, 17, 1, 2, 3, 1 }, { 16, 16, 32, 17, 3, 2, 2, 1 }, { 24, 24, 32, 18, 1, 3, 3, 1 }, { 64, 64, 32, 19, 2, 6, 2, 1 },
        { 20, 20, 8, 17, 1, 8, 2, 0 }, { 12, 14, 32, 17, 1, 10, 2, 0 }, { 40, 40, 1, 17, 1, 12, 1, 0 } };
    int i, g;
    g_ncbpng = 0; g_ncbexp = 0;
    for (i = 0; i < 7; i++) {
        memset(&st[i], 0, sizeof st[i]);
        st[i].ppem_x = sp[i].ppem_x; st[i].ppem_y = sp[i].ppem_y; st[i].depth = sp[i].depth; st[i].imgfmt = sp[i].imgfmt; st[i].idxfmt = sp[i].idxfmt;
        st[i].g = gl[i]; st[i].ng = sp[i].ng;
        if (sp[i].idxfmt == 2) { st[i].sub.h = 50; st[i].sub.w = 48; st[i].sub.bx = 2; st[i].sub.by = 44; st[i].sub.adv = 52; st[i].sub.vbx = -24; st[i].sub.vby = 3; st[i].sub.vadv = 60; }
        for (g = 0; g < sp[i].ng; g++) {
            crb_cbglyph *c = &gl[i][g]; unsigned gid = sp[i].g0 + (unsigned)g;
            memset(c, 0, sizeof *c); c->gid = gid;
            c->m.h = (uint8_t)(sp[i].ppem_x - 2 - (unsigned)g); c->m.w = (uint8_t)(sp[i].ppem_x - 4 - (unsigned)g); c->m.bx = (int8_t)(1 + g); c->m.by = (int8_t)(sp[i].ppem_x - 6 + (unsigned)g);
            c->m.adv = (uint8_t)(sp[i].ppem_x + 1 + (unsigned)g); c->m.vbx = (int8_t)(-3 - g); c->m.vby = (int8_t)(2 + g); c->m.vadv = (uint8_t)(sp[i].ppem_x + 5);
            c->img = mk_png(0x70000000u | (unsigned)i << 8 | (unsigned)g, 7 + (unsigned)g, 9 + (unsigned)g, &c->imglen);
            if (sp[i].idxfmt == 2) c->m = st[i].sub;
            if (sp[i].accepted && g_ncbexp < 32) {
                crb_cbexp *e = &g_cbexp[g_ncbexp++]; e->gid = gid; e->ppem = sp[i].ppem_x; e->imgfmt = sp[i].imgfmt; e->m = c->m; e->png = c->img; e->pnglen = c->imglen;
            }
        }
    }
    crb_cbtables(st, 7, &lc, &nlc, &dt, &ndt);
    memcpy(raw[0].tag, "CBLC", 5); raw[0].data = lc; raw[0].len = (unsigned)nlc;
    memcpy(raw[1].tag, "CBDT", 5); raw[1].data = dt; raw[1].len = (unsigned)ndt;
    out = crb_blob(raw, 2, 0, n);
    free(lc); free(dt);
    return out;
}
