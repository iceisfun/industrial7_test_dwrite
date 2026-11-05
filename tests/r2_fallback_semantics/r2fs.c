#include "r2fs.h"

/* ---- synthetic fonts ----------------------------------------------------------------------------------------------------------------------- */
#define MAXM 160
typedef struct { ttf_map m[MAXM]; int n; ttf_map bmp[MAXM]; int nbmp; ttf_cmap cm[2]; int ncm; } fcm;

static void fcm_add(fcm *c, unsigned cp, unsigned gid)
{
    c->m[c->n].cp = cp; c->m[c->n].gid = gid; c->n++;
    if (cp <= 0xffff) { c->bmp[c->nbmp].cp = cp; c->bmp[c->nbmp].gid = gid; c->nbmp++; }
}
static void fcm_ascii(fcm *c) { unsigned cp; for (cp = 0x20; cp < 0x7f; cp++) { unsigned g = ttf_default_gid(cp); if (g) fcm_add(c, cp, g); } }
static void fcm_done(fcm *c, int fmt4)
{
    c->ncm = 0;
    if (fmt4) { c->cm[c->ncm].format = 4; c->cm[c->ncm].plat = 3; c->cm[c->ncm].enc = 1; c->cm[c->ncm].n = c->nbmp; c->cm[c->ncm].map = c->bmp; c->ncm++; }
    c->cm[c->ncm].format = 12; c->cm[c->ncm].plat = 3; c->cm[c->ncm].enc = 10; c->cm[c->ncm].n = c->n; c->cm[c->ncm].map = c->m; c->ncm++;
}

enum { F_MAIN, F_ALT, F_CJK, F_CJK2, F_SYM, F_EMOJI, F_MARK, F_PART4, F_PART7, F_DUPA, F_DUPB, F_LADDER0 };
static const struct { int weight, width, italic; } k_ladder[] = {
    { 300, 5, 0 }, { 400, 5, 0 }, { 400, 5, 1 }, { 700, 5, 0 }, { 700, 5, 1 }, { 900, 5, 0 }, { 400, 3, 0 }, { 400, 7, 0 },
};
#define NLADDER ((int)(sizeof k_ladder / sizeof k_ladder[0]))

static uint8_t *build_font(int which, size_t *sz)
{
    static fcm c; ttf_spec s;
    memset(&c, 0, sizeof c);
    switch (which) {
    case F_MAIN:  fcm_ascii(&c); fcm_add(&c, 0xe9, 30); ttf_style_spec(&s, "R2Main", 400, 5, 0); break;
    case F_ALT:   fcm_ascii(&c); ttf_style_spec(&s, "R2Alt", 400, 5, 0); break;
    case F_CJK:   fcm_add(&c, 0x20, 1); fcm_add(&c, 0x3042, 67); fcm_add(&c, 0x3044, 67); fcm_add(&c, 0x3046, 67);
                  fcm_add(&c, 0x4e00, 64); fcm_add(&c, 0x4e01, 65); fcm_add(&c, 0x4e02, 68); ttf_style_spec(&s, "R2Cjk", 400, 5, 0); break;
    case F_CJK2:  fcm_add(&c, 0x20, 1); fcm_add(&c, 0x3042, 64); fcm_add(&c, 0x3044, 64); fcm_add(&c, 0x3046, 64);
                  fcm_add(&c, 0x4e00, 65); fcm_add(&c, 0x4e01, 65); fcm_add(&c, 0x4e02, 68); ttf_style_spec(&s, "R2Cjk2", 400, 5, 0); break;
    case F_SYM:   fcm_add(&c, 0x20, 1); fcm_add(&c, 0x2600, 68); fcm_add(&c, 0x2603, 64); fcm_add(&c, 0x2605, 65); ttf_style_spec(&s, "R2Sym", 400, 5, 0); break;
    case F_EMOJI: fcm_add(&c, 0x20, 1); fcm_add(&c, 0x200d, 67); fcm_add(&c, 0x2764, 65); fcm_add(&c, 0xfe0f, 67);
                  fcm_add(&c, 0x1f468, 68); fcm_add(&c, 0x1f469, 68); fcm_add(&c, 0x1f600, 64); fcm_add(&c, 0x1f601, 65); ttf_style_spec(&s, "R2Emoji", 400, 5, 0); break;
    case F_MARK:  fcm_add(&c, 0x20, 1); fcm_add(&c, 0x300, 67); fcm_add(&c, 0x301, 67); fcm_add(&c, 0x308, 67); fcm_add(&c, 0x323, 67); ttf_style_spec(&s, "R2Mark", 400, 5, 0); break;
    case F_PART4: fcm_ascii(&c); ttf_style_spec(&s, "R2Part", 400, 5, 0); break;
    case F_PART7: fcm_ascii(&c); fcm_add(&c, 0x3042, 64); ttf_style_spec(&s, "R2Part", 700, 5, 0); break;
    case F_DUPA:  fcm_add(&c, 0x20, 1); fcm_add(&c, 0x3042, 64); ttf_style_spec(&s, "R2Dup", 400, 5, 0); break;
    case F_DUPB:  fcm_add(&c, 0x20, 1); fcm_add(&c, 0x3042, 65); ttf_style_spec(&s, "R2Dup", 400, 5, 0); break;
    default: {
        int i = which - F_LADDER0;
        fcm_ascii(&c); fcm_add(&c, 0x3042, 64); fcm_add(&c, 0x3044, 64);
        ttf_style_spec(&s, "R2Ladder", k_ladder[i].weight, k_ladder[i].width, k_ladder[i].italic);
        break; }
    }
    fcm_done(&c, 1);
    s.cmaps = c.cm; s.ncmaps = c.ncm;
    return ttf_build(&s, sz);
}

int r2_open(r2_fx *x)
{
    int i; HRESULT hr; UINT32 key[F_LADDER0 + 8];
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    if (dwx_loader_register(x->fl, x->f) != S_OK) return 0;
    for (i = 0; i < F_LADDER0 + NLADDER; i++) {
        size_t n; uint8_t *b = build_font(i, &n);
        if (!b) return 0;
        key[i] = dwx_loader_add(x->fl, b, n); free(b);
    }
    x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < F_LADDER0 + NLADDER; i++) if (i != F_DUPB) dwx_coll_add_key(x->cl, key[i]);
    x->cl2 = dwx_coll_new(x->fl);
    dwx_coll_add_key(x->cl2, key[F_DUPB]);
    if (dwx_coll_register(x->cl, x->f) != S_OK) return 0;
    if (dwx_coll_register(x->cl2, x->f) != S_OK) return 0;
    hr = dwx_coll_create(x->cl, x->f, &x->coll);
    if (FAILED(hr) || !x->coll) return 0;
    hr = dwx_coll_create(x->cl2, x->f, &x->coll2);
    return SUCCEEDED(hr) && x->coll2;
}

void r2_close(r2_fx *x)
{
    DW_RELEASE(x->coll); DW_RELEASE(x->coll2);
    if (x->cl) dwx_coll_unregister(x->cl);
    if (x->cl2) dwx_coll_unregister(x->cl2);
    if (x->fl) dwx_loader_unregister(x->fl);
    if (x->cl) dwx_release(x->cl);
    if (x->cl2) dwx_release(x->cl2);
    if (x->fl) dwx_release(x->fl);
    DW_RELEASE(x->f);
    memset(x, 0, sizeof *x);
}

/* ---- builder helpers ----------------------------------------------------------------------------------------------------------------------- */
IDWriteFactory2 *r2_f2(IDWriteFactory *f)
{
    IDWriteFactory2 *p = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&p))) return NULL;
    return p;
}

IDWriteFontFallbackBuilder *r2_builder(IDWriteFactory *f)
{
    IDWriteFactory2 *f2 = r2_f2(f); IDWriteFontFallbackBuilder *b = NULL;
    if (!f2) return NULL;
    if (FAILED(IDWriteFactory2_CreateFontFallbackBuilder(f2, &b))) b = NULL;
    IDWriteFactory2_Release(f2);
    return b;
}

HRESULT r2_add(IDWriteFontFallbackBuilder *b, UINT32 lo, UINT32 hi, const WCHAR *target, IDWriteFontCollection *coll, const WCHAR *loc, const WCHAR *base, float scale)
{
    DWRITE_UNICODE_RANGE r; const WCHAR *t[1];
    r.first = lo; r.last = hi; t[0] = target;
    return IDWriteFontFallbackBuilder_AddMapping(b, &r, 1, t, 1, coll, loc, base, scale);
}

IDWriteFontFallback *r2_fb(IDWriteFontFallbackBuilder *b)
{
    IDWriteFontFallback *fb = NULL;
    if (FAILED(IDWriteFontFallbackBuilder_CreateFontFallback(b, &fb))) fb = NULL;
    return fb;
}

HRESULT r2_add_standard(IDWriteFontFallbackBuilder *b, IDWriteFontCollection *coll)
{
    DWRITE_UNICODE_RANGE cjk[2] = { { 0x3040, 0x30ff }, { 0x4e00, 0x9fff } }, emo[2] = { { 0x1f600, 0x1f64f }, { 0x1f468, 0x1f469 } };
    const WCHAR *t[1]; HRESULT hr, h;
    t[0] = R2_CJK;   hr = IDWriteFontFallbackBuilder_AddMapping(b, cjk, 2, t, 1, coll, NULL, NULL, 1.0f);
    h = r2_add(b, 0x2600, 0x26ff, R2_SYM, coll, NULL, NULL, 1.0f); if (FAILED(h)) hr = h;
    t[0] = R2_EMOJI; h = IDWriteFontFallbackBuilder_AddMapping(b, emo, 2, t, 1, coll, NULL, NULL, 1.0f); if (FAILED(h)) hr = h;
    h = r2_add(b, 0x0300, 0x036f, R2_MARK, coll, NULL, NULL, 1.0f); if (FAILED(h)) hr = h;
    return hr;
}

IDWriteFontFallback *r2_std_fallback(r2_fx *x)
{
    IDWriteFontFallbackBuilder *b = r2_builder(x->f); IDWriteFontFallback *fb;
    if (!b) return NULL;
    if (FAILED(r2_add_standard(b, x->coll))) { IDWriteFontFallbackBuilder_Release(b); return NULL; }
    fb = r2_fb(b);
    IDWriteFontFallbackBuilder_Release(b);
    return fb;
}

/* ---- font inspection ----------------------------------------------------------------------------------------------------------------------- */
void r2_font_info(IDWriteFont *font, r2_finfo *out)
{
    IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL;
    memset(out, 0, sizeof *out);
    if (!font) return;
    out->w = IDWriteFont_GetWeight(font); out->st = IDWriteFont_GetStyle(font); out->sx = IDWriteFont_GetStretch(font);
    if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &fam)) && fam) {
        if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) { dw_ls_en(s, out->fam, sizeof out->fam); IDWriteLocalizedStrings_Release(s); }
        IDWriteFontFamily_Release(fam);
    }
}

void r2_face_info(IDWriteFontFace5 *face, r2_finfo *out)
{
    IDWriteLocalizedStrings *s = NULL;
    memset(out, 0, sizeof *out);
    if (!face) return;
    out->w = IDWriteFontFace3_GetWeight((IDWriteFontFace3 *)face); out->st = IDWriteFontFace3_GetStyle((IDWriteFontFace3 *)face);
    out->sx = IDWriteFontFace3_GetStretch((IDWriteFontFace3 *)face);
    if (SUCCEEDED(IDWriteFontFace3_GetFamilyNames((IDWriteFontFace3 *)face, &s)) && s) { dw_ls_en(s, out->fam, sizeof out->fam); IDWriteLocalizedStrings_Release(s); }
}

BOOL r2_font_has(IDWriteFont *font, UINT32 cp)
{
    BOOL ex = FALSE;
    if (!font || FAILED(IDWriteFont_HasCharacter(font, cp, &ex))) return FALSE;
    return ex;
}

UINT16 r2_font_gid(IDWriteFont *font, UINT32 cp)
{
    IDWriteFontFace *face = NULL; UINT16 g = 0xffff;
    if (!font || FAILED(IDWriteFont_CreateFontFace(font, &face)) || !face) return 0xffff;
    if (FAILED(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g))) g = 0xffff;
    IDWriteFontFace_Release(face);
    return g;
}

/* ---- MapCharacters driver ------------------------------------------------------------------------------------------------------------------ */
void r2_defaults(r2_args *a, const r2_fx *x)
{
    memset(a, 0, sizeof *a);
    a->pos = 0; a->len = (UINT32)-1; a->coll = x ? x->coll : NULL; a->family = R2_MAIN;
    a->w = DWRITE_FONT_WEIGHT_NORMAL; a->st = DWRITE_FONT_STYLE_NORMAL; a->sx = DWRITE_FONT_STRETCH_NORMAL;
}

HRESULT r2_map(IDWriteFontFallback *fb, const WCHAR *text, UINT32 textlen, const r2_args *a, r2_res *out, dwx_ana_src **src_out)
{
    dwx_ana_src *src = dwx_ana_src_new(text, textlen);
    UINT32 len = a->len == (UINT32)-1 ? textlen - a->pos : a->len;
    memset(out, 0, sizeof *out);
    out->ml = 777; out->scale = 7.0f;
    if (a->locale) lstrcpynW(src->locale, a->locale, LOCALE_NAME_MAX_LENGTH);
    src->locale_run = a->locale_run; src->chunk = a->chunk;
    out->hr = IDWriteFontFallback_MapCharacters(fb, (IDWriteTextAnalysisSource *)src, a->pos, len, a->coll, a->family, a->w, a->st, a->sx, &out->ml, &out->font, &out->scale);
    if (out->font) r2_font_info(out->font, &out->fi);
    if (src_out) *src_out = src; else dwx_release(src);
    return out->hr;
}

int r2_walk(IDWriteFontFallback *fb, const WCHAR *text, const r2_args *a0, char *buf, int cap, int *covered_first_ok)
{
    UINT32 tl = (UINT32)wcslen(text), pos = a0->pos; int nseg = 0, used = 0;
    r2_args a = *a0;
    buf[0] = 0;
    if (covered_first_ok) *covered_first_ok = 1;
    while (pos < tl) {
        r2_res r; UINT32 cp = text[pos]; int n;
        a.pos = pos; a.len = tl - pos;
        if (FAILED(r2_map(fb, text, tl, &a, &r, NULL))) { DW_RELEASE(r.font); return -1; }
        if (cp >= 0xd800 && cp < 0xdc00 && pos + 1 < tl && text[pos + 1] >= 0xdc00 && text[pos + 1] < 0xe000) cp = 0x10000 + ((cp - 0xd800) << 10) + (text[pos + 1] - 0xdc00);
        if (r.font && covered_first_ok && !r2_font_has(r.font, cp)) *covered_first_ok = 0;
        n = snprintf(buf + used, cap - used, "%s%u:%s", nseg ? "|" : "", r.ml, r.font ? r.fi.fam : "-");
        if (n > 0 && used + n < cap) used += n;
        DW_RELEASE(r.font);
        if (r.ml == 0 || r.ml > tl - pos) return -1;
        pos += r.ml; nseg++;
    }
    return nseg;
}

/* merge adjacent segments naming the same family: where an implementation cuts a run inside one family is not part of the contract */
static void walk_coalesce(const char *in, char *out, int cap)
{
    char tmp[512]; char *tok, *save = NULL; char lastfam[128] = ""; unsigned total = 0; int have = 0, used = 0;
    snprintf(tmp, sizeof tmp, "%s", in);
    out[0] = 0;
    for (tok = strtok_s(tmp, "|", &save); ; tok = strtok_s(NULL, "|", &save)) {
        unsigned n = 0; char fam[128] = "";
        if (tok) { char *c = strchr(tok, ':'); if (c) { n = (unsigned)atoi(tok); snprintf(fam, sizeof fam, "%s", c + 1); } }
        if (have && (!tok || strcmp(fam, lastfam))) {
            int w = snprintf(out + used, cap - used, "%s%u:%s", used ? "|" : "", total, lastfam);
            if (w > 0 && used + w < cap) used += w;
            have = 0; total = 0;
        }
        if (!tok) break;
        snprintf(lastfam, sizeof lastfam, "%s", fam); total += n; have = 1;
    }
}

void r2_walk_check_(IDWriteFontFallback *fb, const WCHAR *text, const r2_args *a, const char *expected, const char *file, int line)
{
    char buf[512], cb[512], ce[512]; int cov = 1, n = r2_walk(fb, text, a, buf, sizeof buf, &cov);
    char name[600];
    walk_coalesce(buf, cb, sizeof cb); walk_coalesce(expected, ce, sizeof ce);
    snprintf(name, sizeof name, "walk == \"%s\"", expected);
    t_check(n >= 0 && !strcmp(cb, ce), file, line, name, "got \"%s\" (segments %d)", buf, n);
    (void)cov;
}

void r2_walk_invariants(IDWriteFontFallback *fb, const WCHAR *text, const r2_args *a0, const char *file, int line)
{
    UINT32 tl = (UINT32)wcslen(text), pos = a0->pos, total = 0; r2_args a = *a0; char walk[512] = ""; int used = 0, bad = 0;
    while (pos < tl && !bad) {
        r2_res r; int n;
        a.pos = pos; a.len = tl - pos;
        if (FAILED(r2_map(fb, text, tl, &a, &r, NULL))) { t_check(0, file, line, "MapCharacters succeeds", "text pos %u hr %s", pos, dw_hr_name(r.hr)); DW_RELEASE(r.font); return; }
        n = snprintf(walk + used, sizeof walk - used, "%s%u:%s", used ? "|" : "", r.ml, r.font ? r.fi.fam : "-");
        if (n > 0 && used + n < (int)sizeof walk) used += n;
        if (r.ml == 0 || r.ml > tl - pos) { t_check(0, file, line, "0 < mappedLength <= remaining", "pos %u mappedLength %u remaining %u", pos, r.ml, tl - pos); bad = 1; }
        else {
            UINT32 end = pos + r.ml;
            if (end < tl && text[end - 1] >= 0xd800 && text[end - 1] < 0xdc00 && text[end] >= 0xdc00 && text[end] < 0xe000)
                { t_check(0, file, line, "no surrogate pair split", "pos %u mappedLength %u splits a pair", pos, r.ml); bad = 1; }
            if (r.font) {
                UINT32 cp = text[pos];
                if (cp >= 0xd800 && cp < 0xdc00 && r.ml >= 2) cp = 0x10000 + ((cp - 0xd800) << 10) + (text[pos + 1] - 0xdc00);
                if (!(cp >= 0xd800 && cp < 0xe000)) t_check(r2_font_has(r.font, cp), file, line, "mapped font has the first character", "U+%04X not in \"%s\"", cp, r.fi.fam);
            }
            pos = end; total += r.ml;
        }
        DW_RELEASE(r.font);
    }
    if (!bad) t_check(total == tl - a0->pos, file, line, "segments tile the text", "covered %u of %u", total, tl - a0->pos);
    t_note("walk of %u chars: %s", tl, walk);
}

/* ---- application-implemented IDWriteFontFallback ------------------------------------------------------------------------------------------- */
static HRESULT WINAPI cf_QI(IDWriteFontFallback *This, REFIID riid, void **ppv)
{
    r2_cfb *c = (r2_cfb *)This; InterlockedIncrement(&c->n_qi);
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFallback)) { *ppv = This; InterlockedIncrement(&c->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_AR(IDWriteFontFallback *This) { r2_cfb *c = (r2_cfb *)This; InterlockedIncrement(&c->n_addref); return (ULONG)InterlockedIncrement(&c->ref); }
static ULONG WINAPI cf_RL(IDWriteFontFallback *This)
{
    r2_cfb *c = (r2_cfb *)This; LONG r; InterlockedIncrement(&c->n_release); r = InterlockedDecrement(&c->ref);
    if (r == 0) c->destroyed = 1;
    return (ULONG)r;
}

static const r2_rule *cf_rule(const r2_cfb *c, UINT32 cp)
{
    int i;
    for (i = 0; i < c->nrules; i++) if (cp >= c->rules[i].lo && cp <= c->rules[i].hi) return &c->rules[i];
    return NULL;
}

static HRESULT WINAPI cf_Map(IDWriteFontFallback *This, IDWriteTextAnalysisSource *src, UINT32 pos, UINT32 len, IDWriteFontCollection *basecoll, const WCHAR *basefam,
                             DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st, DWRITE_FONT_STRETCH sx, UINT32 *ml, IDWriteFont **font, FLOAT *scale)
{
    r2_cfb *c = (r2_cfb *)This; r2_cfb_call *lg = NULL; WCHAR buf[96]; UINT32 n = 0, i, mapped; const r2_rule *rule = NULL;
    LONG idx = InterlockedIncrement(&c->n_map) - 1;
    if (idx < R2_CFB_LOG) { lg = &c->log[idx]; c->nlog = idx + 1; memset(lg, 0, sizeof *lg); }
    if (lg) {
        const WCHAR *loc = NULL; UINT32 ll = 0;
        lg->pos = pos; lg->len = len; lg->coll = basecoll; lg->w = w; lg->st = st; lg->sx = sx;
        if (basefam) { lg->has_family = 1; lstrcpynW(lg->family, basefam, 64); }
        if (SUCCEEDED(IDWriteTextAnalysisSource_GetLocaleName(src, pos, &ll, &loc)) && loc) { lstrcpynW(lg->locale, loc, LOCALE_NAME_MAX_LENGTH); lg->locale_len = ll; }
    }
    *ml = 0; *font = NULL; *scale = 1.0f;
    if (c->fail_hr) return c->fail_hr;
    /* read up to 95 characters of [pos, pos+len) from the source */
    while (n < len && n < 95) {
        const WCHAR *t = NULL; UINT32 tl = 0;
        if (FAILED(IDWriteTextAnalysisSource_GetTextAtPosition(src, pos + n, &t, &tl)) || !t || !tl) break;
        for (i = 0; i < tl && n < len && n < 95; i++) buf[n++] = t[i];
    }
    if (lg) { memcpy(lg->text, buf, n * sizeof(WCHAR)); lg->text[n] = 0; lg->ntext = n; }
    if (n == 0) { if (lg) lg->ret_ml = 0; return S_OK; }
    /* the rule of the first code point, extended while the following code points share it */
    {
        UINT32 k = 0, cp;
        while (k < n) {
            UINT32 step = 1; cp = buf[k];
            if (cp >= 0xd800 && cp < 0xdc00 && k + 1 < n && buf[k + 1] >= 0xdc00 && buf[k + 1] < 0xe000) { cp = 0x10000 + ((cp - 0xd800) << 10) + (buf[k + 1] - 0xdc00); step = 2; }
            if (k == 0) rule = cf_rule(c, cp);
            else if (cf_rule(c, cp) != rule) break;
            k += step;
        }
        mapped = k;
    }
    if (n < len && mapped == n) mapped = n;
    if (c->max_ml && mapped > c->max_ml) mapped = c->max_ml;
    if (mapped > len) mapped = len;
    *ml = mapped;
    if (rule && c->font_coll) {
        UINT32 fi; BOOL ex = FALSE; IDWriteFontFamily *fam = NULL;
        if (SUCCEEDED(IDWriteFontCollection_FindFamilyName(c->font_coll, rule->family, &fi, &ex)) && ex &&
            SUCCEEDED(IDWriteFontCollection_GetFontFamily(c->font_coll, fi, &fam)) && fam) {
            IDWriteFontFamily_GetFirstMatchingFont(fam, w, sx, st, font);
            IDWriteFontFamily_Release(fam);
        }
        *scale = rule->scale;
    }
    if (lg) { lg->ret_ml = *ml; lg->ret_font = *font != NULL; }
    return S_OK;
}

static const IDWriteFontFallbackVtbl cf_vt = { cf_QI, cf_AR, cf_RL, cf_Map };

r2_cfb *r2_cfb_new(IDWriteFontCollection *font_coll)
{
    r2_cfb *c = (r2_cfb *)calloc(1, sizeof *c);
    c->lpVtbl = &cf_vt; c->ref = 1; c->font_coll = font_coll;
    return c;
}
void r2_cfb_rule(r2_cfb *c, UINT32 lo, UINT32 hi, const WCHAR *family, float scale)
{
    if (c->nrules >= 8) return;
    c->rules[c->nrules].lo = lo; c->rules[c->nrules].hi = hi; c->rules[c->nrules].family = family; c->rules[c->nrules].scale = scale; c->nrules++;
}
void r2_cfb_free(r2_cfb *c) { free(c); }

/* ---- layout helpers ------------------------------------------------------------------------------------------------------------------------- */
IDWriteTextFormat *r2_format(r2_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *fmt = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt))) return NULL;
    return fmt;
}

IDWriteTextLayout *r2_layout(r2_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h)
{
    IDWriteTextFormat *fmt = r2_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!fmt) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), fmt, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(fmt);
    return l;
}

dwx_renderer *r2_draw(IDWriteTextLayout *l)
{
    dwx_renderer *r = dwx_renderer_new();
    HRESULT hr = IDWriteTextLayout_Draw(l, (void *)(ULONG_PTR)0x77, (IDWriteTextRenderer *)r, 0, 0);
    if (FAILED(hr)) t_note("Draw failed: %s", dw_hr_name(hr));
    return r;
}

void r2_run_family(dwx_renderer *r, int n, char *buf, int cap)
{
    int cur = 0, k = 0; const dwx_draw *d; IDWriteLocalizedStrings *s = NULL; IDWriteFontFace3 *f3 = NULL;
    buf[0] = 0;
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
        if (k++ != n) continue;
        if (d->face && SUCCEEDED(IDWriteFontFace_QueryInterface(d->face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
            if (SUCCEEDED(IDWriteFontFace3_GetFamilyNames(f3, &s)) && s) { dw_ls_en(s, buf, cap); IDWriteLocalizedStrings_Release(s); }
            IDWriteFontFace3_Release(f3);
        }
        return;
    }
}

int r2_glyphs(dwx_renderer *r, unsigned short *out, int cap)
{
    int n = 0, cur = 0; const dwx_draw *d;
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
        UINT32 i; for (i = 0; i < d->nglyphs && n < cap; i++) out[n++] = d->indices[i];
    }
    return n;
}
