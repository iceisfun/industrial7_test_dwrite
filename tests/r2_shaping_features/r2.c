#include "r2.h"

/* ---- glyph construction -------------------------------------------------------------------------------------------------------- */
static const ttf_pt mark_pts[4] = { {100,0,1}, {100,200,1}, {300,200,1}, {300,0,1} };
static const unsigned short end4[1] = { 3 };
#define MAXG 128
static ttf_glyph g_gl[MAXG];
static ttf_map g_map[256], g_map2[256];
static ttf_cmap g_cm[2];

static void gbox(int gid, unsigned adv) {
    const ttf_glyph *d = ttf_default_glyphs(NULL);
    g_gl[gid] = d[2]; g_gl[gid].adv = (unsigned short)adv;
}
static void gmark(int gid) {
    memset(&g_gl[gid], 0, sizeof g_gl[gid]); g_gl[gid].adv = 0; g_gl[gid].npts = 4; g_gl[gid].ncont = 1; g_gl[gid].pts = mark_pts; g_gl[gid].ends = end4;
}
static void gempty(int gid, unsigned adv) { memset(&g_gl[gid], 0, sizeof g_gl[gid]); g_gl[gid].adv = (unsigned short)adv; }
static int g_nm;
static void mapc(unsigned cp, unsigned gid) { g_map[g_nm].cp = cp; g_map[g_nm].gid = gid; g_nm++; }
static void start(ttf_spec *s, const char *fam) {
    const ttf_glyph *d = ttf_default_glyphs(NULL); int nc = 0; const ttf_cmap *dc = ttf_default_cmaps(&nc);
    ttf_default_spec(s);
    memset(g_gl, 0, sizeof g_gl); memcpy(g_gl, d, TTF_DEF_NGLYPHS * sizeof *d);
    g_nm = 0; { int i; for (i = 0; i < dc[0].n; i++) mapc(dc[0].map[i].cp, dc[0].map[i].gid); }
    ttf_set_names(s, fam, "Regular", fam, fam);
}
static void finish(ttf_spec *s, int nglyphs) {
    memcpy(g_map2, g_map, g_nm * sizeof *g_map);
    g_cm[0].format = 4; g_cm[0].plat = 3; g_cm[0].enc = 1; g_cm[0].n = g_nm; g_cm[0].map = g_map;
    g_cm[1].format = 12; g_cm[1].plat = 3; g_cm[1].enc = 10; g_cm[1].n = g_nm; g_cm[1].map = g_map2;
    s->cmaps = g_cm; s->ncmaps = 2; s->glyphs = g_gl; s->nglyphs = nglyphs;
}
static void rawtab(ttf_rawtable *r, const char *tag, const otb_blob *b) { memcpy(r->tag, tag, 5); r->data = b->p; r->len = (unsigned)b->n; }

/* ---- LATN ---------------------------------------------------------------------------------------------------------------------- */
static uint8_t *build_latn(int ignm, size_t *n) {
    ttf_spec s; start(&s, "R2Latn");
    gbox(G_FI, 1500); gbox(G_FF, 1400); gbox(G_FFI, 2000); gbox(G_SCA, 800); gbox(G_SCB, 700); gbox(G_P1, 600); gbox(G_T1, 1000); gbox(G_SS01A, 900);
    gbox(G_CALTB, 950); gmark(G_ACUTE); gmark(G_CIRC); gbox(G_EACUTE, 1000); gbox(G_DOTLESSI, 500); gbox(G_XY, 1800); gbox(G_CK, 1600);
    gbox(G_ALT1, 1100); gbox(G_ALT2, 1200); gbox(G_ZERO, 1000); gempty(G_ZWSP, 0);
    mapc(0x301, G_ACUTE); mapc(0x302, G_CIRC); mapc(0xE9, G_EACUTE); mapc(0x200B, G_ZWSP);
    finish(&s, 88);
    unsigned flag = ignm ? 0x0008 : 0;
    /* ---- GSUB ---- */
    static const otb_seq ccmp[] = { { G_EACUTE, 2, { 32, G_ACUTE } } };
    static const otb_s1 locl[] = { { 36, G_DOTLESSI } };
    static const otb_lig liga[] = { { { 33, 33, 36 }, 3, G_FFI }, { { 33, 33 }, 2, G_FF }, { { 33, 36 }, 2, G_FI } };
    static const otb_lig clig[] = { { { 30, 38 }, 2, G_CK } };
    static const otb_lig rlig[] = { { { 51, 52 }, 2, G_XY } };
    static const unsigned short gA[] = { 28 }, gB[] = { 29 };
    static const otb_glyphset back[] = { { gA, 1 } }, inp[] = { { gB, 1 } };
    static const otb_seqrec rec[] = { { 0, 6 } };
    static const otb_s1 aux[] = { { 29, G_CALTB } }, smcp[] = { { 28, G_SCA }, { 29, G_SCB } }, c2sc[] = { { 2, G_SCA }, { 3, G_SCB } };
    static const otb_s1 ss01[] = { { 28, G_SS01A } }, pnum[] = { { 55, G_P1 } }, tnum[] = { { 55, G_T1 } };
    static const otb_seq salt[] = { { 2, 2, { G_ALT1, G_ALT2 } } };
    static const unsigned short zeroin[] = { 54 };
    otb_lookup L[14]; memset(L, 0, sizeof L);
    L[0].type = 2; L[0].nsub = 1; L[0].sub[0] = otb_gsub2(ccmp, 1);
    L[1].type = 1; L[1].nsub = 1; L[1].sub[0] = otb_gsub1_list(locl, 1);
    L[2].type = 4; L[2].flag = flag; L[2].nsub = 1; L[2].sub[0] = otb_gsub4(liga, 3);
    L[3].type = 4; L[3].nsub = 1; L[3].sub[0] = otb_gsub4(clig, 1);
    L[4].type = 4; L[4].nsub = 1; L[4].sub[0] = otb_gsub4(rlig, 1);
    L[5].type = 6; L[5].nsub = 1; L[5].sub[0] = otb_gsub6(back, 1, inp, 1, NULL, 0, rec, 1);
    L[6].type = 1; L[6].nsub = 1; L[6].sub[0] = otb_gsub1_list(aux, 1);
    L[7].type = 1; L[7].nsub = 1; L[7].sub[0] = otb_gsub1_list(smcp, 2);
    L[8].type = 1; L[8].nsub = 1; L[8].sub[0] = otb_gsub1_list(c2sc, 2);
    L[9].type = 1; L[9].nsub = 1; L[9].sub[0] = otb_gsub1_list(ss01, 1);
    L[10].type = 1; L[10].nsub = 1; L[10].sub[0] = otb_gsub1_list(pnum, 1);
    L[11].type = 1; L[11].nsub = 1; L[11].sub[0] = otb_gsub1_list(tnum, 1);
    L[12].type = 3; L[12].nsub = 1; L[12].sub[0] = otb_gsub3(salt, 1);
    L[13].type = 1; L[13].nsub = 1; L[13].sub[0] = otb_gsub1_delta(zeroin, 1, 32);
    static const otb_feature F[] = {
        { "ccmp", 1, { 0 } }, { "locl", 1, { 1 } }, { "liga", 1, { 2 } }, { "clig", 1, { 3 } }, { "rlig", 1, { 4 } }, { "calt", 1, { 5 } },
        { "smcp", 1, { 7 } }, { "c2sc", 1, { 8 } }, { "ss01", 1, { 9 } }, { "pnum", 1, { 10 } }, { "tnum", 1, { 11 } }, { "salt", 1, { 12 } }, { "zero", 1, { 13 } },
    };
    static const char *common[] = { "ccmp", "liga", "clig", "rlig", "calt", "smcp", "c2sc", "ss01", "pnum", "tnum", "salt", "zero" };
    otb_script sc[2]; memset(sc, 0, sizeof sc);
    int i;
    for (i = 0; i < 2; i++) {
        memcpy(sc[i].tag, i ? "latn" : "DFLT", 5); sc[i].nf = 12; for (int k = 0; k < 12; k++) sc[i].f[k] = common[k];
    }
    /* latn adds the 'TRK ' language system with locl */
    sc[1].nlang = 1; memcpy(sc[1].lang[0].tag, "TRK ", 5); sc[1].lang[0].nf = 13;
    for (int k = 0; k < 12; k++) sc[1].lang[0].f[k] = common[k];
    sc[1].lang[0].f[12] = "locl";
    otb_blob gsub = otb_layout(sc, 2, F, 13, L, 14);
    /* ---- GPOS ---- */
    static const otb_pair pr[] = { { 2, 23, -100, 0 }, { 23, 2, -100, 0 }, { 21, 42, -150, 0 }, { 2, 26, -80, 0 } };
    static const otb_cls c1[] = { { 24, 1 } }, c2[] = { { 2, 1 }, { 42, 1 } };
    static const short mx[] = { 0, 0, 0, -60 };
    static const otb_mark mk[] = { { G_ACUTE, 0, { 100, 0 } }, { G_CIRC, 0, { 100, 0 } } };
    static otb_base bs[52]; static unsigned short cap[26]; static otb_val cv[26]; static const unsigned short zg[] = { 53 };
    for (i = 0; i < 52; i++) { bs[i].gid = (unsigned short)(2 + i); bs[i].a[0].x = 500; bs[i].a[0].y = 1400; }
    for (i = 0; i < 26; i++) { cap[i] = (unsigned short)(2 + i); cv[i].v[0] = cv[i].v[1] = cv[i].v[3] = 0; cv[i].v[2] = 100; }
    static const otb_mark mk2[] = { { G_CIRC, 0, { 100, 0 } } };
    static const otb_base bs2[] = { { G_ACUTE, { { 100, 600 } } } };
    otb_lookup P[6]; memset(P, 0, sizeof P);
    P[0].type = 2; P[0].flag = flag; P[0].nsub = 2; P[0].sub[0] = otb_gpos2_pairs(pr, 4); P[0].sub[1] = otb_gpos2_class(c1, 1, c2, 2, 2, 2, mx);
    P[1].type = 4; P[1].nsub = 1; P[1].sub[0] = otb_gpos_mark(mk, 2, bs, 52, 1);
    P[2].type = 6; P[2].nsub = 1; P[2].sub[0] = otb_gpos_mark(mk2, 1, bs2, 1, 1);
    P[3].type = 1; P[3].nsub = 1; P[3].sub[0] = otb_gpos1_each(cap, 26, 4, cv);
    P[4].type = 1; P[4].nsub = 1; { otb_val v = { { 50, 70, 0, 0 } }; P[4].sub[0] = otb_gpos1_one(zg, 1, 3, v); }
    static const otb_feature PF[] = { { "kern", 1, { 0 } }, { "mark", 1, { 1 } }, { "mkmk", 1, { 2 } }, { "cpsp", 1, { 3 } }, { "ss02", 1, { 4 } } };
    static const char *pcommon[] = { "kern", "mark", "mkmk", "cpsp", "ss02" };
    otb_script psc[2]; memset(psc, 0, sizeof psc);
    for (i = 0; i < 2; i++) { memcpy(psc[i].tag, i ? "latn" : "DFLT", 5); psc[i].nf = 5; for (int k = 0; k < 5; k++) psc[i].f[k] = pcommon[k]; }
    otb_blob gpos = otb_layout(psc, 2, PF, 5, P, 5);
    unsigned char cls[88]; memset(cls, 0, sizeof cls);
    for (i = 1; i < 88; i++) cls[i] = 1;
    cls[G_FI] = cls[G_FF] = cls[G_FFI] = 2; cls[G_ACUTE] = cls[G_CIRC] = 3;
    otb_blob gdef = otb_gdef(cls, 88);
    ttf_rawtable raw[3]; rawtab(&raw[0], "GSUB", &gsub); rawtab(&raw[1], "GPOS", &gpos); rawtab(&raw[2], "GDEF", &gdef);
    s.raw = raw; s.nraw = 3;
    uint8_t *b = ttf_build(&s, n);
    for (i = 0; i < 14; i++) { int k; for (k = 0; k < L[i].nsub; k++) otb_free(&L[i].sub[k]); }
    for (i = 0; i < 5; i++) { int k; for (k = 0; k < P[i].nsub; k++) otb_free(&P[i].sub[k]); }
    otb_free(&gsub); otb_free(&gpos); otb_free(&gdef);
    return b;
}

/* ---- ARAB ---------------------------------------------------------------------------------------------------------------------- */
static uint8_t *build_arab(size_t *n) {
    ttf_spec s; start(&s, "R2Arab"); int i;
    unsigned adv[] = { 700, 650, 1200, 800, 600, 900, 800, 500, 400, 700, 1100, 1000, 500, 0, 710, 1210, 810, 0, 0 };
    for (i = 0; i < 19; i++) { if (i == 13) gmark(69 + i); else if (i >= 17) gempty(69 + i, 0); else gbox(69 + i, adv[i]); }
    mapc(0x627, A_ALEF); mapc(0x628, A_BEH); mapc(0x644, A_LAM); mapc(0x621, A_HAMZA); mapc(0x64E, A_FATHA); mapc(0x200C, A_ZWNJ); mapc(0x200D, A_ZWJ);
    finish(&s, 88);
    static const otb_s1 isol[] = { { A_ALEF, A_ALEF_ISOL }, { A_BEH, A_BEH_ISOL }, { A_LAM, A_LAM_ISOL } };
    static const otb_s1 fina[] = { { A_ALEF, A_ALEF_FINA }, { A_BEH, A_BEH_FINA }, { A_LAM, A_LAM_FINA } };
    static const otb_s1 medi[] = { { A_BEH, A_BEH_MEDI }, { A_LAM, A_LAM_MEDI } };
    static const otb_s1 init[] = { { A_BEH, A_BEH_INIT }, { A_LAM, A_LAM_INIT } };
    static const otb_lig rlig[] = { { { A_LAM_INIT, A_ALEF_FINA }, 2, A_LAMALEF }, { { A_LAM_MEDI, A_ALEF_FINA }, 2, A_LAMALEF_FINA } };
    otb_lookup L[5]; memset(L, 0, sizeof L);
    L[0].type = 1; L[0].nsub = 1; L[0].sub[0] = otb_gsub1_list(isol, 3);
    L[1].type = 1; L[1].nsub = 1; L[1].sub[0] = otb_gsub1_list(fina, 3);
    L[2].type = 1; L[2].nsub = 1; L[2].sub[0] = otb_gsub1_list(medi, 2);
    L[3].type = 1; L[3].nsub = 1; L[3].sub[0] = otb_gsub1_list(init, 2);
    L[4].type = 4; L[4].nsub = 1; L[4].sub[0] = otb_gsub4(rlig, 2);
    static const otb_feature F[] = { { "isol", 1, { 0 } }, { "fina", 1, { 1 } }, { "medi", 1, { 2 } }, { "init", 1, { 3 } }, { "rlig", 1, { 4 } } };
    otb_script sc[1]; memset(sc, 0, sizeof sc); memcpy(sc[0].tag, "arab", 5); sc[0].nf = 5;
    sc[0].f[0] = "isol"; sc[0].f[1] = "fina"; sc[0].f[2] = "medi"; sc[0].f[3] = "init"; sc[0].f[4] = "rlig";
    otb_blob gsub = otb_layout(sc, 1, F, 5, L, 5);
    static const otb_mark mk[] = { { A_FATHA, 0, { 100, 0 } } };
    static otb_base bs[5]; static const unsigned short bg[5] = { A_BEH, A_BEH_INIT, A_BEH_MEDI, A_BEH_FINA, A_BEH_ISOL };
    for (i = 0; i < 5; i++) { bs[i].gid = bg[i]; bs[i].a[0].x = 400; bs[i].a[0].y = 1000; }
    otb_lookup P[1]; memset(P, 0, sizeof P);
    P[0].type = 4; P[0].nsub = 1; P[0].sub[0] = otb_gpos_mark(mk, 1, bs, 5, 1);
    static const otb_feature PF[] = { { "mark", 1, { 0 } } };
    otb_script psc[1]; memset(psc, 0, sizeof psc); memcpy(psc[0].tag, "arab", 5); psc[0].nf = 1; psc[0].f[0] = "mark";
    otb_blob gpos = otb_layout(psc, 1, PF, 1, P, 1);
    unsigned char cls[88]; memset(cls, 0, sizeof cls); for (i = 1; i < 88; i++) cls[i] = 1; cls[A_FATHA] = 3;
    otb_blob gdef = otb_gdef(cls, 88);
    ttf_rawtable raw[3]; rawtab(&raw[0], "GSUB", &gsub); rawtab(&raw[1], "GPOS", &gpos); rawtab(&raw[2], "GDEF", &gdef);
    s.raw = raw; s.nraw = 3;
    uint8_t *b = ttf_build(&s, n);
    for (i = 0; i < 5; i++) { int k; for (k = 0; k < L[i].nsub; k++) otb_free(&L[i].sub[k]); }
    otb_free(&P[0].sub[0]); otb_free(&gsub); otb_free(&gpos); otb_free(&gdef);
    return b;
}

/* ---- DEVA ---------------------------------------------------------------------------------------------------------------------- */
static uint8_t *build_deva(size_t *n) {
    ttf_spec s; start(&s, "R2Deva"); int i;
    gbox(D_KA, 900); gbox(D_SSA, 900); gmark(D_VIRAMA); g_gl[D_VIRAMA].adv = 300; gbox(D_I, 400); gbox(D_AA, 500); gbox(D_HALF_KA, 600);
    gbox(D_RA, 700); gmark(D_REPH); gmark(D_NUKTA); gbox(D_KA_NUKTA, 900);
    mapc(0x915, D_KA); mapc(0x937, D_SSA); mapc(0x94D, D_VIRAMA); mapc(0x93F, D_I); mapc(0x93E, D_AA); mapc(0x930, D_RA); mapc(0x93C, D_NUKTA);
    finish(&s, 88);
    static const otb_lig half[] = { { { D_KA, D_VIRAMA }, 2, D_HALF_KA } };
    static const otb_lig rphf[] = { { { D_RA, D_VIRAMA }, 2, D_REPH } };
    static const otb_lig nukt[] = { { { D_KA, D_NUKTA }, 2, D_KA_NUKTA } };
    otb_lookup L[3]; memset(L, 0, sizeof L);
    L[0].type = 4; L[0].nsub = 1; L[0].sub[0] = otb_gsub4(nukt, 1);
    L[1].type = 4; L[1].nsub = 1; L[1].sub[0] = otb_gsub4(rphf, 1);
    L[2].type = 4; L[2].nsub = 1; L[2].sub[0] = otb_gsub4(half, 1);
    static const otb_feature F[] = { { "nukt", 1, { 0 } }, { "rphf", 1, { 1 } }, { "half", 1, { 2 } } };
    otb_script sc[2]; memset(sc, 0, sizeof sc);
    for (i = 0; i < 2; i++) { memcpy(sc[i].tag, i ? "dev2" : "deva", 5); sc[i].nf = 3; sc[i].f[0] = "nukt"; sc[i].f[1] = "rphf"; sc[i].f[2] = "half"; }
    otb_blob gsub = otb_layout(sc, 2, F, 3, L, 3);
    unsigned char cls[88]; memset(cls, 0, sizeof cls); for (i = 1; i < 88; i++) cls[i] = 1; cls[D_VIRAMA] = cls[D_REPH] = cls[D_NUKTA] = 3;
    otb_blob gdef = otb_gdef(cls, 88);
    ttf_rawtable raw[2]; rawtab(&raw[0], "GSUB", &gsub); rawtab(&raw[1], "GDEF", &gdef);
    s.raw = raw; s.nraw = 2;
    uint8_t *b = ttf_build(&s, n);
    for (i = 0; i < 3; i++) otb_free(&L[i].sub[0]);
    otb_free(&gsub); otb_free(&gdef);
    return b;
}

/* ---- THAI ---------------------------------------------------------------------------------------------------------------------- */
static uint8_t *build_thai(int with_am, size_t *n) {
    ttf_spec s; start(&s, "R2Thai"); int i;
    gbox(TH_KO, 900); gmark(TH_MAIEK); gbox(TH_SARAA, 500); gmark(TH_NIKHAHIT); gbox(TH_SARAAM, 600); gmark(TH_SARAI); gmark(TH_SARAU); gbox(TH_SARAE, 500); gmark(TH_MAITHO);
    mapc(0xE01, TH_KO); mapc(0xE48, TH_MAIEK); mapc(0xE32, TH_SARAA); mapc(0xE4D, TH_NIKHAHIT); if (with_am) mapc(0xE33, TH_SARAAM);
    mapc(0xE34, TH_SARAI); mapc(0xE38, TH_SARAU); mapc(0xE40, TH_SARAE); mapc(0xE49, TH_MAITHO);
    finish(&s, 88);
    unsigned char cls[88]; memset(cls, 0, sizeof cls); for (i = 1; i < 88; i++) cls[i] = 1; cls[TH_MAIEK] = cls[TH_NIKHAHIT] = cls[TH_SARAI] = cls[TH_SARAU] = cls[TH_MAITHO] = 3;
    otb_blob gdef = otb_gdef(cls, 88);
    ttf_rawtable raw[1]; rawtab(&raw[0], "GDEF", &gdef); s.raw = raw; s.nraw = 1;
    uint8_t *b = ttf_build(&s, n);
    otb_free(&gdef);
    return b;
}

static uint8_t *build_plain(size_t *n) { ttf_spec s; start(&s, "R2Plain"); mapc(0x301, 64); finish(&s, TTF_DEF_NGLYPHS); return ttf_build(&s, n); }

uint8_t *fx_blob(int kind, size_t *n) {
    switch (kind) {
    case FX_LATN: return build_latn(0, n);
    case FX_LATN_IGNM: return build_latn(1, n);
    case FX_ARAB: return build_arab(n);
    case FX_DEVA: return build_deva(n);
    case FX_THAI: return build_thai(1, n);
    case FX_THAI_NOAM: return build_thai(0, n);
    default: return build_plain(n);
    }
}

/* ---- fixture ------------------------------------------------------------------------------------------------------------------- */
int fx_open_blob(fx *x, const void *blob, size_t n) {
    IDWriteFactory *f = dw_shared();
    memset(x, 0, sizeof *x);
    if (!f || !blob) return 0;
    if (FAILED(dwx_font_open(&x->h, f, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE)) || !x->h.face) return 0;
    x->face = x->h.face;
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(f, &x->an)) || !x->an) return 0;
    IDWriteTextAnalyzer_QueryInterface(x->an, &IID_IDWriteTextAnalyzer1, (void **)&x->an1);
    x->ok = 1; return 1;
}
int fx_open(fx *x, int kind) {
    size_t n = 0; uint8_t *b = fx_blob(kind, &n); int r;
    if (!b) { memset(x, 0, sizeof *x); return 0; }
    r = fx_open_blob(x, b, n);
    free(b);
    return r;
}
void fx_close(fx *x) {
    DW_RELEASE(x->an1); DW_RELEASE(x->an);
    dwx_font_close(&x->h); x->face = NULL;
}

DWRITE_SCRIPT_ANALYSIS fx_script(fx *x, const WCHAR *text, UINT32 n) {
    DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT };
    dwx_ana_src *s = dwx_ana_src_new(text, n); dwx_ana_sink *k = dwx_ana_sink_new();
    if (SUCCEEDED(IDWriteTextAnalyzer_AnalyzeScript(x->an, (IDWriteTextAnalysisSource *)s, 0, n, (IDWriteTextAnalysisSink *)k)) && k->script_set[0]) sa = k->script[0];
    dwx_release(s); dwx_release(k);
    return sa;
}

/* DWRITE_TYPOGRAPHIC_FEATURES blocks for the ranges */
static void build_feat(const featrange *fr, UINT32 nfr, DWRITE_TYPOGRAPHIC_FEATURES *tf, const DWRITE_TYPOGRAPHIC_FEATURES **ptf, UINT32 *lens) {
    UINT32 i;
    for (i = 0; i < nfr; i++) { tf[i].features = (DWRITE_FONT_FEATURE *)fr[i].f; tf[i].featureCount = fr[i].nf; ptf[i] = &tf[i]; lens[i] = fr[i].len; }
}
HRESULT sh_glyphs(fx *x, shaped *s, const WCHAR *text, UINT32 n, BOOL rtl, const WCHAR *loc, const featrange *fr, UINT32 nfr) {
    DWRITE_TYPOGRAPHIC_FEATURES tf[8]; const DWRITE_TYPOGRAPHIC_FEATURES *ptf[8]; UINT32 lens[8];
    memset(s, 0, sizeof *s);
    s->len = n; s->text = text; s->loc = loc ? loc : L"en-us"; s->rtl = rtl; s->sa = fx_script(x, text, n); s->actual = 0xdeadbeef;
    if (nfr > 8) nfr = 8;
    build_feat(fr, nfr, tf, ptf, lens);
    s->hr = IDWriteTextAnalyzer_GetGlyphs(x->an, text, n, x->face, FALSE, rtl, &s->sa, s->loc, NULL, nfr ? ptf : NULL, nfr ? lens : NULL, nfr,
                                          SH_MAX, s->cmap, s->tp, s->gid, s->gp, &s->actual);
    return s->hr;
}
HRESULT sh_place(fx *x, shaped *s, float em, const featrange *fr, UINT32 nfr) {
    DWRITE_TYPOGRAPHIC_FEATURES tf[8]; const DWRITE_TYPOGRAPHIC_FEATURES *ptf[8]; UINT32 lens[8];
    if (nfr > 8) nfr = 8;
    build_feat(fr, nfr, tf, ptf, lens);
    s->hr_place = IDWriteTextAnalyzer_GetGlyphPlacements(x->an, s->text, s->cmap, s->tp, s->len, s->gid, s->gp, s->actual, x->face, em, FALSE, s->rtl, &s->sa,
                                                         s->loc, nfr ? ptf : NULL, nfr ? lens : NULL, nfr, s->adv, s->off);
    return s->hr_place;
}
HRESULT sh_run(fx *x, shaped *s, const WCHAR *text, BOOL rtl, const DWRITE_FONT_FEATURE *f, UINT32 nf) {
    featrange fr; HRESULT hr; UINT32 n = WLEN(text);
    fr.f = f; fr.nf = nf; fr.len = n;
    hr = sh_glyphs(x, s, text, n, rtl, NULL, nf ? &fr : NULL, nf ? 1 : 0);
    if (FAILED(hr)) return hr;
    return sh_place(x, s, EM2048, nf ? &fr : NULL, nf ? 1 : 0);
}

int sh_gids_are(const shaped *s, const UINT16 *want, UINT32 n) {
    UINT32 i; if (FAILED(s->hr) || s->actual != n) return 0;
    for (i = 0; i < n; i++) if (s->gid[i] != want[i]) return 0;
    return 1;
}
const char *sh_dump(const shaped *s) {
    static char b[512]; int k = 0; UINT32 i;
    k += snprintf(b + k, sizeof b - k, "[hr=%s n=%u:", dw_hr_name(s->hr), s->actual);
    for (i = 0; i < s->actual && i < 40 && k < 480; i++) k += snprintf(b + k, sizeof b - k, " %u", s->gid[i]);
    snprintf(b + k, sizeof b - k, "]"); return b;
}

int sh_order(const shaped *s) {
    UINT32 i; int up = 1, down = 1;
    for (i = 1; i < s->len; i++) { if (s->cmap[i] < s->cmap[i - 1]) up = 0; if (s->cmap[i] > s->cmap[i - 1]) down = 0; }
    if (up) return 0;
    return down ? 1 : -1;
}
int sh_index_of(const shaped *s, UINT16 gid) { UINT32 i; for (i = 0; i < s->actual; i++) if (s->gid[i] == gid) return (int)i; return -1; }
int sh_has(const shaped *s, UINT16 gid) { return sh_index_of(s, gid) >= 0; }
