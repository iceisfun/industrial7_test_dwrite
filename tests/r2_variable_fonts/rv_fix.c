/* Fixture generator and open helpers for the r2_variable_fonts area, see rv.h. */
#include "rv.h"

const rv_axis rv_axes[RV_NAXES] = {
    { "wght", 100, 400, 900, 0, 256, "Weight" },
    { "wdth", 75, 100, 125, 0, 257, "Width" },
    { "opsz", 8, 14, 144, 0, 258, "Optical Size" },
    { "HIDN", 0, 0, 10, 1, 259, "Hidden Axis" },
    { "FIXD", 5, 5, 5, 0, 260, "Fixed Axis" },
};
static const struct { unsigned id; const char *name; float c[RV_NAXES]; } inst[RV_NINST] = {
    { 261, "Light",           { 300, 100, 14, 0, 5 } },
    { 262, "Regular",         { 400, 100, 14, 0, 5 } },
    { 263, "Bold",            { 700, 100, 14, 0, 5 } },
    { 264, "Condensed Bold",  { 700,  75, 14, 0, 5 } },
};

static void p16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void p32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static uint32_t fx16(double v) { return (uint32_t)(int32_t)floor(v * 65536.0 + 0.5); }
static unsigned f2d14(double v) { return (unsigned)((int)floor(v * 16384.0 + 0.5)) & 0xFFFF; }

/* ---- table builders ------------------------------------------------------------------------------------------------------------- */
static uint8_t *make_fvar(const rv_axis *ax, int n, int with_inst, unsigned *len)
{
    int ninst = with_inst ? RV_NINST : 0, i, j; unsigned isz = 4 + 4 * (unsigned)n, l = 16 + 20 * (unsigned)n + (unsigned)ninst * isz;
    uint8_t *t = (uint8_t *)calloc(1, l);
    p16(t, 1); p16(t + 2, 0); p16(t + 4, 16); p16(t + 6, 2); p16(t + 8, (unsigned)n); p16(t + 10, 20); p16(t + 12, (unsigned)ninst); p16(t + 14, isz);
    for (i = 0; i < n; i++) {
        uint8_t *a = t + 16 + 20 * i;
        memcpy(a, ax[i].tag, 4); p32(a + 4, fx16(ax[i].mn)); p32(a + 8, fx16(ax[i].df)); p32(a + 12, fx16(ax[i].mx)); p16(a + 16, ax[i].flags); p16(a + 18, ax[i].nameid);
    }
    for (i = 0; i < ninst; i++) {
        uint8_t *r = t + 16 + 20 * n + i * isz;
        p16(r, inst[i].id); p16(r + 2, 0);
        for (j = 0; j < n && j < RV_NAXES; j++) p32(r + 4 + 4 * j, fx16(inst[i].c[j]));
    }
    *len = l; return t;
}
static uint8_t *make_avar(unsigned *len)
{
    /* wght: 4 pairs, the other four axes: 3 pairs each */
    unsigned l = 8 + (2 + 4 * 4) + 4 * (2 + 3 * 4); uint8_t *t = (uint8_t *)calloc(1, l), *p = t + 8; int i;
    p16(t, 1); p16(t + 2, 0); p16(t + 4, 0); p16(t + 6, RV_NAXES);
    p16(p, 4); p += 2;
    p16(p, f2d14(-1)); p16(p + 2, f2d14(-1)); p += 4; p16(p, 0); p16(p + 2, 0); p += 4;
    p16(p, f2d14(0.5)); p16(p + 2, f2d14(0.25)); p += 4; p16(p, f2d14(1)); p16(p + 2, f2d14(1)); p += 4;
    for (i = 1; i < RV_NAXES; i++) {
        p16(p, 3); p += 2;
        p16(p, f2d14(-1)); p16(p + 2, f2d14(-1)); p += 4; p16(p, 0); p16(p + 2, 0); p += 4; p16(p, f2d14(1)); p16(p + 2, f2d14(1)); p += 4;
    }
    *len = l; return t;
}
static uint8_t *make_hvar(unsigned *len)
{
    enum { NR = 5, NG = TTF_DEF_NGLYPHS };
    unsigned hdr = 20, ivs = hdr, rl = ivs + 12;                    /* item variation store: format, regionListOffset, count, 1 offset */
    unsigned rl_len = 4 + NR * RV_NAXES * 6, ivd = rl + rl_len, ivd_len = 6 + 2 * NR + NG * 2 * NR, l = ivd + ivd_len;
    uint8_t *t = (uint8_t *)calloc(1, l), *r; int g, a, k;
    p16(t, 1); p16(t + 2, 0); p32(t + 4, hdr); p32(t + 8, 0); p32(t + 12, 0); p32(t + 16, 0);
    p16(t + ivs, 1); p32(t + ivs + 2, rl - ivs); p16(t + ivs + 6, 1); p32(t + ivs + 8, ivd - ivs);
    r = t + rl; p16(r, RV_NAXES); p16(r + 2, NR); r += 4;
    for (k = 0; k < NR; k++) for (a = 0; a < RV_NAXES; a++, r += 6) {
        double s = 0, pk = 0, e = 0;
        if (k == 0 && a == 0) { s = 0; pk = 1; e = 1; }
        if (k == 1 && a == 0) { s = -1; pk = -1; e = 0; }
        if (k == 2 && a == 1) { s = 0; pk = 1; e = 1; }
        if (k == 3 && a == 2) { s = 0; pk = 1; e = 1; }
        if (k == 4 && a == 2) { s = -1; pk = -1; e = 0; }
        p16(r, f2d14(s)); p16(r + 2, f2d14(pk)); p16(r + 4, f2d14(e));
    }
    r = t + ivd; p16(r, NG); p16(r + 2, NR); p16(r + 4, NR); for (k = 0; k < NR; k++) p16(r + 6 + 2 * k, (unsigned)k); r += 6 + 2 * NR;
    for (g = 0; g < NG; g++, r += 2 * NR) {
        int d[NR] = { 0, 0, 0, 0, 0 };
        if (g == 2) { d[0] = 100; d[1] = -50; d[2] = 60; }
        if (g == 3) { d[0] = 200; d[1] = -100; }
        if (g == 4) { d[3] = 40; d[4] = -30; }
        for (k = 0; k < NR; k++) p16(r + 2 * k, (unsigned)d[k] & 0xFFFF);
    }
    *len = l; return t;
}
static uint8_t *make_gvar(int naxes, unsigned *len)
{
    enum { NG = TTF_DEF_NGLYPHS };
    unsigned hdr = 20, offs = hdr, data = offs + 4 * (NG + 1), tuplehdr = 4 + 2 + 2 + 2 * (unsigned)naxes, ser = 1 + 1 + 16 + 1, g2 = tuplehdr + ser, g2p = (g2 + 3) & ~3u;
    unsigned l = data + g2p; uint8_t *t = (uint8_t *)calloc(1, l), *d; int g;
    p16(t, 1); p16(t + 2, 0); p16(t + 4, (unsigned)naxes); p16(t + 6, 0); p32(t + 8, data); p16(t + 12, NG); p16(t + 14, 1); p32(t + 16, data);
    for (g = 0; g <= NG; g++) p32(t + offs + 4 * g, g <= 2 ? 0 : g2p);       /* only glyph 2 has data */
    d = t + data;
    p16(d, 1); p16(d + 2, tuplehdr);                                       /* tupleVariationCount, offset to serialised data */
    p16(d + 4, ser); p16(d + 6, 0xA000);                                   /* variationDataSize, EMBEDDED_PEAK_TUPLE | PRIVATE_POINT_NUMBERS */
    p16(d + 8, f2d14(1));                                                 /* wght peak; remaining axes 0 */
    d += tuplehdr;
    d[0] = 0;                                                              /* point count 0 = all points (4 outline + 4 phantom) */
    d[1] = 0x40 | 7; for (g = 0; g < 4; g++) p16(d + 2 + 2 * g, 100);     /* x deltas: words, run of 8; first four points +100 */
    d[18] = 0x80 | 7;                                                      /* y deltas: run of 8 zeros */
    *len = l; return t;
}

static uint8_t *make_stat(unsigned *len)
{
    /* version 1.1, two design axes: 'ZSTA' (not in fvar, ordering 0) and 'wght' (ordering 1), no axis values */
    unsigned l = 20 + 2 * 8; uint8_t *t = (uint8_t *)calloc(1, l);
    p16(t, 1); p16(t + 2, 1); p16(t + 4, 8); p16(t + 6, 2); p32(t + 8, 20); p16(t + 12, 0); p32(t + 14, 0); p16(t + 18, 2);
    memcpy(t + 20, "ZSTA", 4); p16(t + 24, 270); p16(t + 26, 0);
    memcpy(t + 28, "wght", 4); p16(t + 32, 256); p16(t + 34, 1);
    *len = l; return t;
}

static uint8_t *build(const rv_axis *ax, int n, int with_inst, unsigned flags, size_t *size)
{
    ttf_spec s; ttf_rawtable raw[5]; int nraw = 0, i; unsigned l[5]; uint8_t *t[5] = { 0 }, *blob;
    ttf_default_spec(&s);
    ttf_set_names(&s, "RVVar", "Regular", "RVVar", "RVVar-Regular");
    for (i = 0; i < n; i++) if (ax[i].name && ax[i].nameid) ttf_add_name(&s, 3, 1, 0x409, ax[i].nameid, ax[i].name);
    if (with_inst) for (i = 0; i < RV_NINST; i++) ttf_add_name(&s, 3, 1, 0x409, inst[i].id, inst[i].name);
    memset(raw, 0, sizeof raw);
    if (!(flags & RVF_NOFVAR)) {
        t[0] = make_fvar(ax, n, with_inst, &l[0]); memcpy(raw[nraw].tag, "fvar", 5); raw[nraw].data = t[0]; raw[nraw].len = l[0]; nraw++;
        if (!(flags & RVF_NOGVAR)) { t[1] = make_gvar(n, &l[1]); memcpy(raw[nraw].tag, "gvar", 5); raw[nraw].data = t[1]; raw[nraw].len = l[1]; nraw++; }
        if ((flags & RVF_AVAR) && n == RV_NAXES) { t[2] = make_avar(&l[2]); memcpy(raw[nraw].tag, "avar", 5); raw[nraw].data = t[2]; raw[nraw].len = l[2]; nraw++; }
        if ((flags & RVF_HVAR) && n == RV_NAXES) { t[3] = make_hvar(&l[3]); memcpy(raw[nraw].tag, "HVAR", 5); raw[nraw].data = t[3]; raw[nraw].len = l[3]; nraw++; }
    }
    if (flags & RVF_STAT) { ttf_add_name(&s, 3, 1, 0x409, 270, "Stat Axis"); t[4] = make_stat(&l[4]); memcpy(raw[nraw].tag, "STAT", 5); raw[nraw].data = t[4]; raw[nraw].len = l[4]; nraw++; }
    s.raw = raw; s.nraw = nraw;
    blob = ttf_build(&s, size);
    for (i = 0; i < 5; i++) free(t[i]);
    return blob;
}
uint8_t *rv_build(unsigned flags, size_t *size) { return build(rv_axes, RV_NAXES, 1, flags, size); }
uint8_t *rv_build_axes(const rv_axis *ax, int n, size_t *size) { return build(ax, n, 0, 0, size); }

/* ---- single font resource -------------------------------------------------------------------------------------------------------- */
int rv_open(rv *x, const uint8_t *blob, size_t n)
{
    HRESULT hr;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("DWriteCreateFactory failed"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory6, (void **)&x->f6)) || !x->f6) { t_skip("IDWriteFactory6 unsupported"); return 0; }
    x->ld = dwx_loader_new();
    x->key = dwx_loader_add(x->ld, blob, n);
    if (FAILED(dwx_loader_register(x->ld, x->f))) { t_skip("loader registration failed"); return 0; }
    if (FAILED(dwx_loader_file(x->ld, x->f, x->key, &x->file)) || !x->file) { t_skip("font file creation failed"); return 0; }
    hr = IDWriteFactory6_CreateFontResource(x->f6, x->file, 0, &x->res);
    if (FAILED(hr) || !x->res) { t_skip("CreateFontResource -> %s", dw_hr_name(hr)); return 0; }
    x->ok = 1;
    return 1;
}
void rv_close(rv *x)
{
    DW_RELEASE(x->res); DW_RELEASE(x->file);
    if (x->ld) { dwx_loader_unregister(x->ld); dwx_release(x->ld); x->ld = NULL; }
    DW_RELEASE(x->f6); DW_RELEASE(x->f);
}
IDWriteFontFace5 *rv_face(rv *x, const DWRITE_FONT_AXIS_VALUE *v, UINT32 n)
{
    IDWriteFontFace5 *f = NULL;
    HRESULT hr = IDWriteFontResource_CreateFontFace(x->res, DWRITE_FONT_SIMULATIONS_NONE, v, n, &f);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? f : NULL;
}
int rv_find_value(const DWRITE_FONT_AXIS_VALUE *v, UINT32 n, UINT32 tag)
{
    UINT32 i; for (i = 0; i < n; i++) if ((UINT32)v[i].axisTag == tag) return (int)i; return -1;
}
int rv_axis_index(IDWriteFontResource *res, UINT32 tag)
{
    DWRITE_FONT_AXIS_VALUE v[32]; UINT32 n = IDWriteFontResource_GetFontAxisCount(res);
    if (n == 0 || n > 32 || FAILED(IDWriteFontResource_GetDefaultFontAxisValues(res, v, n))) return -1;
    return rv_find_value(v, n, tag);
}
int rv_adv(IDWriteFontFace *face, UINT16 gid)
{
    DWRITE_GLYPH_METRICS m; memset(&m, 0, sizeof m);
    if (FAILED(IDWriteFontFace_GetDesignGlyphMetrics(face, &gid, 1, &m, FALSE))) return -1;
    return (int)m.advanceWidth;
}
UINT16 rv_gid(IDWriteFontFace *face, UINT32 cp)
{
    UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g); return g;
}
int rv_outline_x(IDWriteFontFace *face, UINT16 gid, float *xmin, float *xmax)
{
    dwx_geom *g = dwx_geom_new(); float y0, y1; HRESULT hr;
    hr = IDWriteFontFace_GetGlyphRunOutline(face, 2048.0f, &gid, NULL, NULL, 1, FALSE, FALSE, (IDWriteGeometrySink *)g);
    if (FAILED(hr)) { dwx_release(g); return -1; }
    dwx_geom_bounds(g, xmin, &y0, xmax, &y1);
    dwx_release(g);
    return 0;
}

/* ---- Bahnschrift ----------------------------------------------------------------------------------------------------------------- */
int rb_open(rb *b)
{
    static const char *const names[] = { "bahnschrift.ttf", NULL };
    WCHAR w[MAX_PATH];
    memset(b, 0, sizeof *b);
    b->f = dw_shared();
    if (!b->f) return 0;
    if (FAILED(IDWriteFactory_QueryInterface(b->f, &IID_IDWriteFactory6, (void **)&b->f6)) || !b->f6) return 0;
    if (!dw_find_system_font(names, b->path, sizeof b->path)) return 0;
    if (!dw_wide(b->path, w, MAX_PATH)) return 0;
    if (FAILED(IDWriteFactory6_CreateFontFileReference(b->f6, w, NULL, &b->file)) || !b->file) return 0;
    if (FAILED(IDWriteFactory6_CreateFontResource(b->f6, b->file, 0, &b->res)) || !b->res) return 0;
    b->ok = 1;
    return 1;
}
void rb_close(rb *b)
{
    DW_RELEASE(b->res); DW_RELEASE(b->file); DW_RELEASE(b->f6);       /* the shared factory is owned by the helper */
}

/* ---- private collection ------------------------------------------------------------------------------------------------------------ */
int rc_open(rc *c, const uint8_t *blob, size_t n)
{
    UINT32 key;
    memset(c, 0, sizeof *c);
    c->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!c->f) { t_skip("DWriteCreateFactory failed"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(c->f, &IID_IDWriteFactory6, (void **)&c->f6)) || !c->f6) { t_skip("IDWriteFactory6 unsupported"); return 0; }
    c->fl = dwx_loader_new();
    key = dwx_loader_add(c->fl, blob, n);
    if (FAILED(dwx_loader_register(c->fl, c->f))) { t_skip("loader registration failed"); return 0; }
    c->cl = dwx_coll_new(c->fl);
    dwx_coll_add_key(c->cl, key);
    if (FAILED(dwx_coll_register(c->cl, c->f))) { t_skip("collection loader registration failed"); return 0; }
    if (FAILED(dwx_coll_create(c->cl, c->f, &c->coll)) || !c->coll) { t_skip("custom collection creation failed"); return 0; }
    c->ok = 1;
    return 1;
}
void rc_close(rc *c)
{
    DW_RELEASE(c->coll);
    if (c->cl) { dwx_coll_unregister(c->cl); dwx_release(c->cl); c->cl = NULL; }
    if (c->fl) { dwx_loader_unregister(c->fl); dwx_release(c->fl); c->fl = NULL; }
    DW_RELEASE(c->f6); DW_RELEASE(c->f);
}
