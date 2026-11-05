#include "ntf.h"

static ttf_map g_main[160], g_ja[8], g_sym[4], g_emoji[4], g_ar[8];
static ttf_cmap g_cm_main[2], g_cm_ja[2], g_cm_sym[2], g_cm_emoji[1], g_cm_ar[2];
static const ttf_sub4 g_liga[] = { { { 28, 29 }, 2, 67 } };
static const ttf_sub4 g_dlig[] = { { { 30, 31 }, 2, 64 } };
static const ttf_sub1 g_smcp[] = { { 28, 65 }, { 29, 64 } };
static const ttf_sub1 g_c2sc[] = { { 2, 64 } };
static const ttf_sub1 g_ss01[] = { { 29, 68 } };
static const ttf_sub1 g_ss02[] = { { 29, 64 } };
static const ttf_feature g_gsub[] = {
    { "liga", TTF_LK_LIGATURE, 1, g_liga }, { "dlig", TTF_LK_LIGATURE, 1, g_dlig },
    { "smcp", TTF_LK_SINGLE, 2, g_smcp }, { "c2sc", TTF_LK_SINGLE, 1, g_c2sc },
    { "ss01", TTF_LK_SINGLE, 1, g_ss01 }, { "ss02", TTF_LK_SINGLE, 1, g_ss02 },
};
static const ttf_kernpair g_kern[] = { { 2, 23, -200 } };    /* 'A','V' */

static int add(ttf_map *m, int n, unsigned cp, unsigned gid) { m[n].cp = cp; m[n].gid = gid; return n + 1; }

static void build_maps(void)
{
    unsigned cp, i; int n = 0;
    if (g_main[0].gid) return;
    for (cp = 0x20; cp < 0x7f; cp++) { unsigned g = ttf_default_gid(cp); if (g) n = add(g_main, n, cp, g); }
    for (i = 0; i < 10; i++) n = add(g_main, n, 0x660 + i, 2 + i);
    n = add(g_main, n, 0x66a, 64); n = add(g_main, n, 0x66b, 65); n = add(g_main, n, 0x66c, 66);
    for (i = 0; i < 10; i++) n = add(g_main, n, 0x6f0 + i, 38 + i);
    g_cm_main[0].format = 4;  g_cm_main[0].plat = 3; g_cm_main[0].enc = 1;  g_cm_main[0].n = n; g_cm_main[0].map = g_main;
    g_cm_main[1].format = 12; g_cm_main[1].plat = 3; g_cm_main[1].enc = 10; g_cm_main[1].n = n; g_cm_main[1].map = g_main;
    n = 0; n = add(g_ja, n, 0x20, 1); n = add(g_ja, n, 0x3042, 2); n = add(g_ja, n, 0x3044, 3); n = add(g_ja, n, 0x3046, 4);
    n = add(g_ja, n, 0x4e00, 5); n = add(g_ja, n, 0x4e01, 6); n = add(g_ja, n, 0x4e02, 7);
    g_cm_ja[0].format = 4;  g_cm_ja[0].plat = 3; g_cm_ja[0].enc = 1;  g_cm_ja[0].n = n; g_cm_ja[0].map = g_ja;
    g_cm_ja[1].format = 12; g_cm_ja[1].plat = 3; g_cm_ja[1].enc = 10; g_cm_ja[1].n = n; g_cm_ja[1].map = g_ja;
    n = 0; n = add(g_sym, n, 0x20, 1); n = add(g_sym, n, 0x2603, 2); n = add(g_sym, n, 0x2605, 3);
    g_cm_sym[0].format = 4;  g_cm_sym[0].plat = 3; g_cm_sym[0].enc = 1;  g_cm_sym[0].n = n; g_cm_sym[0].map = g_sym;
    g_cm_sym[1].format = 12; g_cm_sym[1].plat = 3; g_cm_sym[1].enc = 10; g_cm_sym[1].n = n; g_cm_sym[1].map = g_sym;
    n = 0; n = add(g_emoji, n, 0x20, 1); n = add(g_emoji, n, 0x1f600, 2); n = add(g_emoji, n, 0x1f601, 3);
    g_cm_emoji[0].format = 12; g_cm_emoji[0].plat = 3; g_cm_emoji[0].enc = 10; g_cm_emoji[0].n = n; g_cm_emoji[0].map = g_emoji;
    n = 0; n = add(g_ar, n, 0x20, 1); for (i = 0; i < 4; i++) n = add(g_ar, n, 0x627 + i, 2 + i);
    g_cm_ar[0].format = 4;  g_cm_ar[0].plat = 3; g_cm_ar[0].enc = 1;  g_cm_ar[0].n = n; g_cm_ar[0].map = g_ar;
    g_cm_ar[1].format = 12; g_cm_ar[1].plat = 3; g_cm_ar[1].enc = 10; g_cm_ar[1].n = n; g_cm_ar[1].map = g_ar;
}

static uint8_t *build_font(int which, size_t *sz)
{
    ttf_spec s; static const char *fam[] = { "NtfMain", "NtfJa", "NtfSym", "NtfEmoji", "NtfAr" };
    build_maps();
    ttf_style_spec(&s, fam[which], 400, 5, 0);
    switch (which) {
    case 0: s.cmaps = g_cm_main; s.ncmaps = 2; s.gsub = g_gsub; s.ngsub = (int)(sizeof g_gsub / sizeof g_gsub[0]);
            s.gpos_kern = g_kern; s.ngpos_kern = 1; break;
    case 1: s.cmaps = g_cm_ja; s.ncmaps = 2; break;
    case 2: s.cmaps = g_cm_sym; s.ncmaps = 2; break;
    case 3: s.cmaps = g_cm_emoji; s.ncmaps = 1; break;
    default: s.cmaps = g_cm_ar; s.ncmaps = 2; break;
    }
    return ttf_build(&s, sz);
}

int ntf_open(ntf_fx *x)
{
    int i; HRESULT hr;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    x->fl = dwx_loader_new();
    if (dwx_loader_register(x->fl, x->f) != S_OK) return 0;
    x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < 5; i++) {
        size_t n; uint8_t *b = build_font(i, &n); UINT32 k;
        if (!b) return 0;
        k = dwx_loader_add(x->fl, b, n); free(b);
        dwx_coll_add_key(x->cl, k);
    }
    if (dwx_coll_register(x->cl, x->f) != S_OK) return 0;
    hr = dwx_coll_create(x->cl, x->f, &x->coll);
    return SUCCEEDED(hr) && x->coll;
}

void ntf_close(ntf_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) dwx_coll_unregister(x->cl);
    if (x->fl) dwx_loader_unregister(x->fl);
    if (x->cl) dwx_release(x->cl);
    if (x->fl) dwx_release(x->fl);
    DW_RELEASE(x->f);
    memset(x, 0, sizeof *x);
}

IDWriteFactory2 *ntf_f2(IDWriteFactory *f)
{
    IDWriteFactory2 *p = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&p))) return NULL;
    return p;
}

void ntf_font_family(IDWriteFont *font, char *buf, int cap)
{
    IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL;
    buf[0] = 0;
    if (!font) return;
    if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &fam)) && fam) {
        if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) { dw_ls_en(s, buf, cap); IDWriteLocalizedStrings_Release(s); }
        IDWriteFontFamily_Release(fam);
    }
}

void ntf_face_family(IDWriteFontFace5 *face, char *buf, int cap)
{
    IDWriteLocalizedStrings *s = NULL;
    buf[0] = 0;
    if (!face) return;
    if (SUCCEEDED(IDWriteFontFace3_GetFamilyNames((IDWriteFontFace3 *)face, &s)) && s) { dw_ls_en(s, buf, cap); IDWriteLocalizedStrings_Release(s); }
}

HRESULT ntf_map(IDWriteFontFallback *fb, const WCHAR *text, UINT32 textlen, UINT32 pos, UINT32 len, IDWriteFontCollection *base,
                const WCHAR *family, UINT32 *mapped_len, IDWriteFont **font, float *scale, dwx_ana_src **src_out)
{
    dwx_ana_src *src = dwx_ana_src_new(text, textlen);
    HRESULT hr;
    *mapped_len = 777; *font = NULL; *scale = 7.0f;
    hr = IDWriteFontFallback_MapCharacters(fb, (IDWriteTextAnalysisSource *)src, pos, len, base, family,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, mapped_len, font, scale);
    if (src_out) *src_out = src; else dwx_release(src);
    return hr;
}

IDWriteTextFormat *ntf_format(ntf_fx *x, const WCHAR *family, float size)
{
    IDWriteTextFormat *fmt = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, family, x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt))) return NULL;
    return fmt;
}

IDWriteTextLayout *ntf_layout(ntf_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h)
{
    IDWriteTextFormat *fmt = ntf_format(x, family, size); IDWriteTextLayout *l = NULL;
    if (!fmt) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, (UINT32)wcslen(text), fmt, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(fmt);
    return l;
}

dwx_renderer *ntf_draw(IDWriteTextLayout *l)
{
    dwx_renderer *r = dwx_renderer_new();
    HRESULT hr = IDWriteTextLayout_Draw(l, (void *)(ULONG_PTR)0x77, (IDWriteTextRenderer *)r, 0, 0);
    if (FAILED(hr)) t_note("Draw failed: %s", dw_hr_name(hr));
    return r;
}

int ntf_glyphs(dwx_renderer *r, unsigned short *out, int cap)
{
    int n = 0, cur = 0; const dwx_draw *d;
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
        UINT32 i; for (i = 0; i < d->nglyphs && n < cap; i++) out[n++] = d->indices[i];
    }
    return n;
}

void ntf_run_family(dwx_renderer *r, int n, char *buf, int cap)
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

IDWriteFontFace *ntf_face(IDWriteFontCollection *coll, const WCHAR *family)
{
    UINT32 idx; BOOL ex = FALSE; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
    if (FAILED(IDWriteFontCollection_FindFamilyName(coll, family, &idx, &ex)) || !ex) return NULL;
    if (FAILED(IDWriteFontCollection_GetFontFamily(coll, idx, &fam)) || !fam) return NULL;
    if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font)) && font) {
        if (FAILED(IDWriteFont_CreateFontFace(font, &face))) face = NULL;
        IDWriteFont_Release(font);
    }
    IDWriteFontFamily_Release(fam);
    return face;
}

HRESULT ntf_shape(IDWriteFactory *f, IDWriteFontFace *face, const WCHAR *text, const WCHAR *locale, IDWriteNumberSubstitution *ns,
                  IDWriteTypography *typo, unsigned short *out, unsigned short *cluster, UINT32 *nglyphs)
{
    IDWriteTextAnalyzer *an = NULL; HRESULT hr; UINT32 len = (UINT32)wcslen(text);
    dwx_ana_src *src; dwx_ana_sink *sink; DWRITE_SCRIPT_ANALYSIS sa;
    DWRITE_SHAPING_TEXT_PROPERTIES tp[64]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[64];
    DWRITE_FONT_FEATURE ff[32]; DWRITE_TYPOGRAPHIC_FEATURES tf; const DWRITE_TYPOGRAPHIC_FEATURES *tfp = &tf; UINT32 rlen = len;
    *nglyphs = 0;
    hr = IDWriteFactory_CreateTextAnalyzer(f, &an);
    if (FAILED(hr)) return hr;
    src = dwx_ana_src_new(text, len); sink = dwx_ana_sink_new();
    hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sink);
    sa = sink->script[0];
    dwx_release(src); dwx_release(sink);
    if (FAILED(hr)) { IDWriteTextAnalyzer_Release(an); return hr; }
    memset(&tf, 0, sizeof tf);
    if (typo) {
        UINT32 i, n = IDWriteTypography_GetFontFeatureCount(typo);
        if (n > 32) n = 32;
        for (i = 0; i < n; i++) IDWriteTypography_GetFontFeature(typo, i, &ff[i]);
        tf.features = ff; tf.featureCount = n;
    }
    hr = IDWriteTextAnalyzer_GetGlyphs(an, text, len, face, FALSE, FALSE, &sa, locale, ns, typo ? &tfp : NULL, typo ? &rlen : NULL,
            typo ? 1 : 0, 64, cluster, tp, out, gp, nglyphs);
    IDWriteTextAnalyzer_Release(an);
    return hr;
}

void ntf_qi_only(IUnknown *obj, const IID *const *allowed, int n, const char *file, int line)
{
    int cnt, i, j; const dw_qi_row *rows = dw_iid_table(&cnt);
    for (i = 0; i < cnt; i++) {
        int ok = 0; void *pv = (void *)(ULONG_PTR)1; HRESULT hr;
        for (j = 0; j < n; j++) if (IsEqualIID(rows[i].iid, allowed[j])) ok = 1;
        hr = IUnknown_QueryInterface(obj, rows[i].iid, &pv);
        if (ok) {
            t_check(hr == S_OK && pv != NULL, file, line, rows[i].name, "QI of a supported interface: %s", dw_hr_name(hr));
            if (SUCCEEDED(hr) && pv) IUnknown_Release((IUnknown *)pv);
        } else {
            t_check(hr == E_NOINTERFACE && pv == NULL, file, line, rows[i].name, "QI of an unsupported interface: %s ptr %p", dw_hr_name(hr), pv);
            if (SUCCEEDED(hr) && pv) IUnknown_Release((IUnknown *)pv);
        }
    }
}
