#include "uc.h"

/* Code points the fixture font maps in addition to the default letters/digits/space. Order defines the glyph ids. */
const unsigned uc_extra[] = {
    0x0000, 0x007F, 0x00A0, 0x00AD, 0x00E9, 0x0301,                 /* 0..5   */
    0x0028, 0x0029, 0x005B, 0x005D, 0x003C, 0x003E, 0x007B, 0x007D, /* 6..13  ( ) [ ] < > { } */
    0x00AB, 0x00BB,                                                 /* 14,15  guillemets */
    0x200B, 0x200C, 0x200D, 0x200E, 0x200F,                         /* 16..20 ZWSP ZWNJ ZWJ LRM RLM */
    0x2028, 0x2029, 0x2060, 0xFEFF, 0xFFFD,                         /* 21..25 */
    0xFFFF, 0xFFFE, 0xFDD0, 0xFDEF, 0xD7FF,                         /* 26..30 noncharacters, last before surrogates */
    0xE000, 0xF8FF, 0xFE00, 0xFE0F,                                 /* 31..34 PUA, variation selectors */
    0x0009, 0x000A, 0x000D, 0x0085, 0x1FFF, 0x2000, 0x202F, 0x3000,  /* 35..42 controls, spaces */
    0x0041 + 0x1000, 0x00C0, 0x00FF, 0x0100,                        /* 43..46 */
    0x10000, 0x1F600, 0x10FFFF, 0x10FFFD, 0x1FFFE, 0x20000,         /* 47..52 supplementary */
    0xE0001, 0xE0020, 0xE007F, 0xE0100, 0xF0000, 0x1D400, 0x1F1E6, /* 53..59 tag chars, UVS supplement, SPUA, math, regional indicator */
    0x0627, 0x05D0, 0x4E2D,                                         /* 60..62 */
};
const int uc_nextra = (int)(sizeof uc_extra / sizeof uc_extra[0]);

static int in_variant(int v, unsigned cp)
{
    if (v == UC_V_BMP) return cp < 0xFFFF;
    return 1;
}
unsigned uc_gid(int v, unsigned cp)
{
    int i; unsigned g = ttf_default_gid(cp);
    if (g) return g;
    for (i = 0; i < uc_nextra; i++) if (uc_extra[i] == cp) return in_variant(v, cp) ? UC_FIRST_EXTRA_GID + i : 0;
    return 0;
}

static const ttf_uvs g_uvs[] = { { 0x41, 0xFE00, 1, 0 }, { 0x42, 0xFE00, 0, UC_UVS_NONDEFAULT_GID }, { 0x43, 0xE0100, 0, 101 } };

uint8_t *uc_build(int variant, size_t *n)
{
    static ttf_glyph glyphs[UC_FIRST_EXTRA_GID + 64];
    static ttf_map maps[256]; static ttf_cmap cm[4];
    ttf_spec s; int ng = 0, nm = 0, i, nc = 0; const ttf_glyph *d = ttf_default_glyphs(&ng);
    ttf_default_spec(&s);
    memcpy(glyphs, d, sizeof(ttf_glyph) * ng);
    for (i = 0; i < 64; i++) glyphs[ng + i] = glyphs[2];          /* box glyphs, advance 1000 */
    s.glyphs = glyphs; s.nglyphs = ng + 64;
    for (i = 0; i < 128; i++) { unsigned g = ttf_default_gid(i); if (g) { maps[nm].cp = i; maps[nm].gid = g; nm++; } }
    for (i = 0; i < uc_nextra; i++) { maps[nm].cp = uc_extra[i]; maps[nm].gid = UC_FIRST_EXTRA_GID + i; nm++; }
    memset(cm, 0, sizeof cm);
    if (variant == UC_V_BOTH || variant == UC_V_BMP || variant == UC_V_UVS) { cm[nc].format = 4; cm[nc].plat = 3; cm[nc].enc = 1; cm[nc].n = nm; cm[nc].map = maps; nc++; }
    if (variant == UC_V_BOTH || variant == UC_V_UCS4 || variant == UC_V_UVS) { cm[nc].format = 12; cm[nc].plat = 3; cm[nc].enc = 10; cm[nc].n = nm; cm[nc].map = maps; nc++; }
    if (variant == UC_V_UVS) { cm[nc].format = 14; cm[nc].plat = 0; cm[nc].enc = 5; cm[nc].nuvs = 3; cm[nc].uvs = g_uvs; nc++; }
    s.cmaps = cm; s.ncmaps = nc;
    ttf_set_names(&s, "UcTest", "Regular", "UcTest", "UcTest-Regular");
    return ttf_build(&s, n);
}

int uc_face_open(uc_face *u, int variant)
{
    size_t n = 0; uint8_t *b; HRESULT hr; IDWriteFactory *f = dw_shared();
    memset(u, 0, sizeof *u); u->variant = variant;
    if (!f) return 0;
    b = uc_build(variant, &n); if (!b) return 0;
    hr = dwx_font_open(&u->h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(b);
    if (hr != S_OK || !u->h.face) return 0;
    u->face = u->h.face;
    IDWriteFontFace_QueryInterface(u->face, &IID_IDWriteFontFace1, (void **)&u->face1);
    return 1;
}
void uc_face_close(uc_face *u) { DW_RELEASE(u->face1); dwx_font_close(&u->h); u->face = NULL; }

int uc_fx_open(uc_fx *x)
{
    size_t n; uint8_t *b; UINT32 key;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) return 0;
    b = uc_build(UC_V_BOTH, &n); if (!b) return 0;
    x->fl = dwx_loader_new(); key = dwx_loader_add(x->fl, b, n); free(b);
    if (FAILED(dwx_loader_register(x->fl, x->f))) return 0;
    x->cl = dwx_coll_new(x->fl); dwx_coll_add_key(x->cl, key);
    if (FAILED(dwx_coll_register(x->cl, x->f))) return 0;
    if (FAILED(dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) return 0;
    return 1;
}
void uc_fx_close(uc_fx *x)
{
    DW_RELEASE(x->coll);
    if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}
IDWriteTextLayout *uc_layout(uc_fx *x, const WCHAR *text, UINT32 len, float size, float w, float h)
{
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(x->f, L"UcTest", x->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &t)) || !t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, text, len, t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}

IDWriteFont *uc_sysfont(IDWriteFactory *f, const WCHAR *family)
{
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (!f || FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return NULL;
    if (SUCCEEDED(IDWriteFontCollection_FindFamilyName(c, family, &idx, &ex)) && ex &&
        SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam)
        IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
    DW_RELEASE(fam); DW_RELEASE(c);
    return font;
}
IDWriteFontFace1 *uc_sysface1(IDWriteFactory *f, const WCHAR *family)
{
    IDWriteFontFace *face = dw_sysface(f, family, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace1 *f1 = NULL;
    if (!face) return NULL;
    IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace1, (void **)&f1);
    IDWriteFontFace_Release(face);
    return f1;
}

IDWriteTextAnalyzer *uc_analyzer(void)
{
    static IDWriteTextAnalyzer *an;
    if (!an && dw_shared()) IDWriteFactory_CreateTextAnalyzer(dw_shared(), &an);
    return an;
}
HRESULT uc_props(DWRITE_SCRIPT_ANALYSIS sa, DWRITE_SCRIPT_PROPERTIES *p)
{
    IDWriteTextAnalyzer1 *a1 = NULL; HRESULT hr; IDWriteTextAnalyzer *an = uc_analyzer();
    memset(p, 0, sizeof *p);
    if (!an || FAILED(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer1, (void **)&a1)) || !a1) return E_NOINTERFACE;
    hr = IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, p);
    IDWriteTextAnalyzer1_Release(a1);
    return hr;
}
void uc_analyze(const WCHAR *t, UINT32 len, uc_ana *o)
{
    dwx_ana_src *s; dwx_ana_sink *k; IDWriteTextAnalyzer *an = uc_analyzer(); int i;
    memset(o, 0, sizeof *o); o->hr = E_FAIL;
    if (!an) return;
    s = dwx_ana_src_new(t, len); k = dwx_ana_sink_new();
    o->hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)s, 0, len, (IDWriteTextAnalysisSink *)k);
    o->covered = dwx_ana_covered(k, 's', len);
    for (i = 0; i < k->nrun_script && i < 64; i++) {
        DWRITE_SCRIPT_PROPERTIES p; uc_run *r = &o->r[i];
        r->pos = k->script_runs[i].pos; r->len = k->script_runs[i].len; r->sa = k->script[r->pos];
        if (SUCCEEDED(uc_props(r->sa, &p))) { r->iso = p.isoScriptCode; r->num = p.isoScriptNumber; }
    }
    o->n = k->nrun_script > 64 ? 64 : k->nrun_script;
    dwx_release(s); dwx_release(k);
}
DWRITE_SCRIPT_ANALYSIS uc_script_at(const WCHAR *t, UINT32 len, UINT32 pos, int *ok)
{
    uc_ana a; int i; DWRITE_SCRIPT_ANALYSIS z = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT };
    uc_analyze(t, len, &a);
    if (ok) *ok = 0;
    for (i = 0; i < a.n; i++) if (pos >= a.r[i].pos && pos < a.r[i].pos + a.r[i].len) { if (ok) *ok = 1; return a.r[i].sa; }
    return z;
}

HRESULT uc_shape_sa(IDWriteFontFace *face, const WCHAR *t, UINT32 len, BOOL rtl, DWRITE_SCRIPT_ANALYSIS sa, uc_shaped *o)
{
    memset(o, 0, sizeof *o); o->actual = 0xdeadbeef; o->nglyphs = UC_MAXG;
    o->hr = IDWriteTextAnalyzer_GetGlyphs(uc_analyzer(), t, len, face, FALSE, rtl, &sa, L"en-us", NULL, NULL, NULL, 0, UC_MAXG,
                                          o->cluster, o->tp, o->gid, o->gp, &o->actual);
    return o->hr;
}
HRESULT uc_shape(IDWriteFontFace *face, const WCHAR *t, UINT32 len, BOOL rtl, uc_shaped *o)
{
    return uc_shape_sa(face, t, len, rtl, uc_script_at(t, len, 0, NULL), o);
}

int uc_utf16(unsigned cp, WCHAR *out)
{
    if (cp < 0x10000) { out[0] = (WCHAR)cp; return 1; }
    cp -= 0x10000; out[0] = (WCHAR)(0xD800 + (cp >> 10)); out[1] = (WCHAR)(0xDC00 + (cp & 0x3FF)); return 2;
}
