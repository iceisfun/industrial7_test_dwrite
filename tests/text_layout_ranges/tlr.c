#include "tlr.h"

/* ---- font blobs (built once, shared by every fixture) ---------------------------------------------------------------------- */
static ttf_glyph g_gl[8][TTF_DEF_NGLYPHS];
static uint8_t *g_blob[8]; static size_t g_size[8];
static int g_built;

static uint8_t *build_face(int slot, const char *family, int weight, int width, int italic, int adv, size_t *sz)
{
    ttf_spec s; int n = 0, i; const ttf_glyph *g = ttf_default_glyphs(&n);
    if (n > TTF_DEF_NGLYPHS) n = TTF_DEF_NGLYPHS;
    memcpy(g_gl[slot], g, sizeof(ttf_glyph) * n);
    for (i = 0; i < n; i++)
        if (g_gl[slot][i].adv == TTF_DEF_ADV) g_gl[slot][i].adv = (unsigned short)adv;     /* letters, digits, space, triangle, ... */
    ttf_style_spec(&s, family, weight, width, italic);
    s.glyphs = g_gl[slot]; s.nglyphs = n;
    return ttf_build(&s, sz);
}

static int build_all(void)
{
    if (g_built) return 1;
    g_blob[0] = build_face(0, "DwTest",   400, 5, 0, 1000, &g_size[0]);
    g_blob[1] = build_face(1, "DwTest",   700, 5, 0, 1500, &g_size[1]);
    g_blob[2] = build_face(2, "DwTest",   400, 5, 1, 800,  &g_size[2]);
    g_blob[3] = build_face(3, "DwTest",   700, 5, 1, 1200, &g_size[3]);
    g_blob[4] = build_face(4, "DwTest",   400, 3, 0, 600,  &g_size[4]);
    g_blob[5] = build_face(5, "DwNarrow", 400, 5, 0, 500,  &g_size[5]);
    g_blob[6] = build_face(6, "DwWide",   400, 5, 0, 2000, &g_size[6]);
    { int i; for (i = 0; i < 7; i++) if (!g_blob[i]) return 0; }
    g_built = 1; return 1;
}

int tlr_open(tlr_fx *x)
{
    int i;
    memset(x, 0, sizeof *x);
    if (!build_all()) return 0;
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < 7; i++) { UINT32 k = dwx_loader_add(x->fl, g_blob[i], g_size[i]); dwx_coll_add_key(x->cl, k); }
    if (FAILED(dwx_loader_register(x->fl, x->f))) return 0;
    if (FAILED(dwx_coll_register(x->cl, x->f))) return 0;
    if (FAILED(dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) return 0;
    return 1;
}

void tlr_close(tlr_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}

IDWriteTextFormat *tlr_format(tlr_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, TLR_LOC, &t))) return NULL;
    return t;
}

IDWriteTextLayout *tlr_layout_(tlr_fx *x, const WCHAR *family, const WCHAR *text, float size)
{
    IDWriteTextFormat *t = tlr_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), t, 100000.0f, 100000.0f, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

void *tlr_qi(IUnknown *u, const IID *iid, const char *name)
{
    void *p = NULL;
    HRESULT hr = IUnknown_QueryInterface(u, iid, &p);
    if (FAILED(hr) || !p) t_skip("QueryInterface(%s) -> %s", name, dw_hr_name(hr));
    return p;
}

/* ---- getters ------------------------------------------------------------------------------------------------------------- */
#define GETTER(name, T, call, init) \
    T name(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r) \
    { T v = init; DWRITE_TEXT_RANGE rr = POISON_RANGE; HRESULT hr = call; \
      t_check(hr == S_OK, __FILE__, __LINE__, #name, "got %s at position %u", dw_hr_name(hr), (unsigned)pos); \
      if (r) *r = rr; return v; }
GETTER(tlr_weight,  DWRITE_FONT_WEIGHT,  IDWriteTextLayout_GetFontWeight(l, pos, &v, &rr),  (DWRITE_FONT_WEIGHT)0x7777)
GETTER(tlr_style,   DWRITE_FONT_STYLE,   IDWriteTextLayout_GetFontStyle(l, pos, &v, &rr),   (DWRITE_FONT_STYLE)0x7777)
GETTER(tlr_stretch, DWRITE_FONT_STRETCH, IDWriteTextLayout_GetFontStretch(l, pos, &v, &rr), (DWRITE_FONT_STRETCH)0x7777)
GETTER(tlr_size,    float,               IDWriteTextLayout_GetFontSize(l, pos, &v, &rr),    -12345.0f)
GETTER(tlr_underline, BOOL,              IDWriteTextLayout_GetUnderline(l, pos, &v, &rr),   0x5a5a)
GETTER(tlr_strike,  BOOL,                IDWriteTextLayout_GetStrikethrough(l, pos, &v, &rr), 0x5a5a)

#define PGET(name, T, call) \
    T name(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r) \
    { T v = TLR_SENT(T); DWRITE_TEXT_RANGE rr = POISON_RANGE; HRESULT hr = call; \
      t_check(hr == S_OK, __FILE__, __LINE__, #name, "got %s at position %u", dw_hr_name(hr), (unsigned)pos); \
      if (r) *r = rr; return v; }
PGET(tlr_effect, IUnknown *, IDWriteTextLayout_GetDrawingEffect(l, pos, &v, &rr))
PGET(tlr_inline, IDWriteInlineObject *, IDWriteTextLayout_GetInlineObject(l, pos, &v, &rr))
PGET(tlr_typo, IDWriteTypography *, IDWriteTextLayout_GetTypography(l, pos, &v, &rr))
PGET(tlr_coll, IDWriteFontCollection *, IDWriteTextLayout_GetFontCollection(l, pos, &v, &rr))

static WCHAR g_ring[8][512]; static int g_ri;
const WCHAR *tlr_family(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r)
{
    WCHAR *b = g_ring[g_ri++ & 7]; UINT32 len = 0xdead; DWRITE_TEXT_RANGE r1 = POISON_RANGE, r2 = POISON_RANGE;
    HRESULT hr = IDWriteTextLayout_GetFontFamilyNameLength(l, pos, &len, &r1);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetFontFamilyNameLength", "got %s", dw_hr_name(hr));
    b[0] = 0;
    if (len + 1 > 511) { t_check(0, __FILE__, __LINE__, "family length sane", "len %u", len); return b; }
    hr = IDWriteTextLayout_GetFontFamilyName(l, pos, b, len + 1, &r2);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetFontFamilyName", "got %s", dw_hr_name(hr));
    t_check(wcslen(b) == len, __FILE__, __LINE__, "wcslen(family) == length", "len %u strlen %u", len, (unsigned)wcslen(b));
    t_check(r1.startPosition == r2.startPosition && r1.length == r2.length, __FILE__, __LINE__, "family length/name ranges agree", "{%u,%u} vs {%u,%u}",
            r1.startPosition, r1.length, r2.startPosition, r2.length);
    if (r) *r = r2;
    return b;
}
const WCHAR *tlr_locale(IDWriteTextLayout *l, UINT32 pos, DWRITE_TEXT_RANGE *r)
{
    WCHAR *b = g_ring[g_ri++ & 7]; UINT32 len = 0xdead; DWRITE_TEXT_RANGE r1 = POISON_RANGE, r2 = POISON_RANGE;
    HRESULT hr = IDWriteTextLayout_GetLocaleNameLength(l, pos, &len, &r1);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetLocaleNameLength", "got %s", dw_hr_name(hr));
    b[0] = 0;
    if (len + 1 > 511) { t_check(0, __FILE__, __LINE__, "locale length sane", "len %u", len); return b; }
    hr = IDWriteTextLayout_GetLocaleName(l, pos, b, len + 1, &r2);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetLocaleName", "got %s", dw_hr_name(hr));
    t_check(wcslen(b) == len, __FILE__, __LINE__, "wcslen(locale) == length", "len %u strlen %u", len, (unsigned)wcslen(b));
    t_check(r1.startPosition == r2.startPosition && r1.length == r2.length, __FILE__, __LINE__, "locale length/name ranges agree", "{%u,%u} vs {%u,%u}",
            r1.startPosition, r1.length, r2.startPosition, r2.length);
    if (r) *r = r2;
    return b;
}

float tlr_width(IDWriteTextLayout *l)
{
    DWRITE_TEXT_METRICS m; HRESULT hr; memset(&m, 0xcc, sizeof m);
    hr = IDWriteTextLayout_GetMetrics(l, &m);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetMetrics", "got %s", dw_hr_name(hr));
    return m.width;
}
float tlr_height(IDWriteTextLayout *l)
{
    DWRITE_TEXT_METRICS m; HRESULT hr; memset(&m, 0xcc, sizeof m);
    hr = IDWriteTextLayout_GetMetrics(l, &m);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetMetrics", "got %s", dw_hr_name(hr));
    return m.height;
}
UINT32 tlr_lines(IDWriteTextLayout *l)
{
    DWRITE_TEXT_METRICS m; HRESULT hr; memset(&m, 0xcc, sizeof m);
    hr = IDWriteTextLayout_GetMetrics(l, &m);
    t_check(hr == S_OK, __FILE__, __LINE__, "GetMetrics", "got %s", dw_hr_name(hr));
    return m.lineCount;
}

/* ---- reference model --------------------------------------------------------------------------------------------------- */
int tlr_cell_eq(const tlr_cell *a, const tlr_cell *b)
{
    return a->weight == b->weight && a->style == b->style && a->stretch == b->stretch && tlr_fbits(a->size, b->size) &&
           a->ul == b->ul && a->st == b->st && a->effect == b->effect;
}
void tlr_model_init(tlr_model *m, const tlr_cell *def)
{
    int i; m->def = *def; for (i = 0; i < TLR_MODEL_N; i++) m->c[i] = *def;
}
void tlr_model_run(const tlr_model *m, UINT32 pos, tlr_cell *value, DWRITE_TEXT_RANGE *r)
{
    const tlr_cell *cur = pos < TLR_MODEL_N ? &m->c[pos] : &m->def;
    UINT32 s = pos, e = pos;
    if (pos >= TLR_MODEL_N) {
        s = TLR_MODEL_N;
        while (s > 0 && tlr_cell_eq(&m->c[s - 1], cur)) s--;
        e = TLR_ALL;
    } else {
        while (s > 0 && tlr_cell_eq(&m->c[s - 1], cur)) s--;
        e = pos + 1;
        while (e < TLR_MODEL_N && tlr_cell_eq(&m->c[e], cur)) e++;
        if (e == TLR_MODEL_N && tlr_cell_eq(&m->def, cur)) e = TLR_ALL;
    }
    *value = *cur; r->startPosition = s; r->length = e - s;
}
UINT32 tlr_rand(UINT32 *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }
