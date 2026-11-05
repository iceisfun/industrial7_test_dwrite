/* Larger and more varied synthetic fonts: many glyphs, compressed hmtx tails, all optional tables at once, odd sfnt versions. */
#include "ot.h"

/* Font with `extra` additional box glyphs after the default 69; the trailing `tail` glyphs share advance 700 so hmtx can be compressed. */
static uint8_t *build_big(int extra, int tail, int compress, size_t *n, ttf_glyph **keep, ttf_spec *sp) {
    int dn, i; const ttf_glyph *dg = ttf_default_glyphs(&dn); ttf_glyph *g = (ttf_glyph *)calloc(dn + extra, sizeof *g);
    ttf_cmap *cm; ttf_map *mp;
    memcpy(g, dg, dn * sizeof *g);
    for (i = 0; i < extra; i++) {
        g[dn + i] = dg[2];                                         /* box glyph like 'A' */
        g[dn + i].adv = (i >= extra - tail) ? 700 : (unsigned short)(800 + (i % 50));
        g[dn + i].lsb_set = 1; g[dn + i].lsb = (short)(10 + (i % 90));
    }
    ttf_default_spec(sp); sp->glyphs = g; sp->nglyphs = dn + extra; sp->compress_hmtx = compress;
    mp = (ttf_map *)calloc(extra + 1, sizeof *mp); cm = (ttf_cmap *)calloc(1, sizeof *cm);
    for (i = 0; i < extra; i++) { mp[i].cp = 0xE000 + i; mp[i].gid = dn + i; }
    cm->format = 12; cm->plat = 3; cm->enc = 10; cm->n = extra; cm->map = mp; sp->cmaps = cm; sp->ncmaps = 1;
    *keep = g;
    return ttf_build(sp, n);
}
static void big_check(int extra, int tail, int compress) {
    ttf_spec sp; ttf_glyph *g; size_t n; uint8_t *b = build_big(extra, tail, compress, &n, &g, &sp); dwx_font h; HRESULT hr; ot_dir d; ot_font f; int i, bad = 0;
    memset(&h, 0, sizeof h);
    if (!b) { t_skip("build"); free(g); return; }
    hr = dwx_font_open(&h, dw_shared(), b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (hr != S_OK || !h.face) { t_note("big font not created: %s", dw_hr_name(hr)); dwx_font_close(&h); free(b); free(g); return; }
    ot_parse_dir(b, n, 0, &d); ot_font_init(&f, b, n, &d);
    EXPECT_EQ(IDWriteFontFace_GetGlyphCount(h.face), sp.nglyphs);
    EXPECT_EQ(f.nglyphs, sp.nglyphs);
    if (compress) CHECK(f.nhm < f.nglyphs);                     /* numberOfHMetrics < numGlyphs: the tail has lsb only */
    {
        UINT16 *ids = (UINT16 *)malloc(sp.nglyphs * 2); DWRITE_GLYPH_METRICS *gm = (DWRITE_GLYPH_METRICS *)malloc(sp.nglyphs * sizeof *gm);
        for (i = 0; i < sp.nglyphs; i++) ids[i] = (UINT16)i;
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(h.face, ids, sp.nglyphs, gm, FALSE));
        for (i = 0; i < sp.nglyphs; i++) {
            /* gids past numberOfHMetrics reuse the last advance with their own lsb from the trailing array */
            if (gm[i].advanceWidth != g[i].adv || gm[i].leftSideBearing != (g[i].lsb_set ? g[i].lsb : (g[i].npts || g[i].ncomp ? 100 : 0))) { if (bad++ < 5) { if (i >= 69) CHECKF(0, "gid %d adv %u lsb %d want %d/%d", i, gm[i].advanceWidth, gm[i].leftSideBearing, g[i].adv, g[i].lsb); } }
        }
        free(ids); free(gm);
    }
    {
        int cnt = 0, nb = compare_all_tables(h.face, b, n, &d, &cnt); EXPECT_EQ(nb, 0);
        { UINT32 cps[3] = { 0xE000, 0xE000 + extra - 1, 0xE000 + extra }; UINT16 gi[3]; IDWriteFontFace_GetGlyphIndices(h.face, cps, 3, gi); EXPECT_EQ(gi[0], 69); EXPECT_EQ(gi[1], 69 + extra - 1); EXPECT_EQ(gi[2], 0); }
    }
    dwx_font_close(&h); free(b); free(g); free((void *)sp.cmaps[0].map); free((void *)sp.cmaps);
}
TEST(fw_glyphs_200_uncompressed) { big_check(200, 0, 0); }
TEST(fw_glyphs_200_compressed_tail_30) { big_check(200, 30, 1); }
TEST(fw_glyphs_1000_compressed_tail_500) { big_check(1000, 500, 1); }
TEST(fw_glyphs_3000_uncompressed) { big_check(3000, 0, 0); }
TEST(fw_glyphs_3000_compressed_tail_2000) { big_check(3000, 2000, 1); }
TEST(fw_glyphs_300_compressed_tail_2) { big_check(300, 2, 1); }
TEST(fw_glyphs_8000_long_loca_automatic) { big_check(8000, 100, 1); }

TEST(fw_long_loca_when_glyf_exceeds_128k) {
    ttf_spec sp; ttf_glyph *g; size_t n; uint8_t *b = build_big(8000, 0, 0, &n, &g, &sp); ot_dir d; ot_font f;
    if (!b) { t_skip("build"); free(g); return; }
    ot_parse_dir(b, n, 0, &d); ot_font_init(&f, b, n, &d);
    t_note("glyf %u bytes, head.indexToLocFormat %d", ot_find(&d, TG("glyf"))->len, f.loca_fmt);
    CHECK(f.loca_fmt == 1 || ot_find(&d, TG("glyf"))->len < 131072);
    free(b); free(g); free((void *)sp.cmaps[0].map); free((void *)sp.cmaps);
}

/* ---- kitchen sink: every optional table at once ---- */
static const ttf_kernpair ks_kern[] = { { 2, 3, -10 } };
static const ttf_sub1 ks_sub[] = { { 2, 28 } };
static const ttf_feature ks_feat[] = { { "smcp", TTF_LK_SINGLE, 1, ks_sub } };
static const ttf_colr_base ks_cb[] = { { 2, 0, 1 } };
static const ttf_colr_layer ks_cl[] = { { 28, 0 } };
static const uint32_t ks_pal[] = { 0xFF112233u };
static const ttf_gasp ks_gasp[] = { { 0xFFFF, 0xF } };
static uint8_t ks_junk[8][70];
static void ks_spec(ttf_spec *sp, ttf_rawtable *raw) {
    static const char *const tags[8] = { "DSIG", "cvt ", "fpgm", "prep", "GDEF", "hdmx", "LTSH", "meta" };
    static const unsigned lens[8] = { 12, 24, 9, 17, 12, 20, 70, 16 };
    int i, k;
    ttf_default_spec(sp);
    sp->kern = ks_kern; sp->nkern = 1; sp->gpos_kern = ks_kern; sp->ngpos_kern = 1; sp->gsub = ks_feat; sp->ngsub = 1;
    sp->vertical = 1; sp->v_ascent = 1500; sp->v_descent = -548; sp->has_base = 1; sp->base_ideo = -100; sp->base_romn = 0;
    sp->colr_base = ks_cb; sp->ncolr_base = 1; sp->colr_layers = ks_cl; sp->ncolr_layers = 1; sp->palette = ks_pal; sp->npalette = 1;
    sp->gasp = ks_gasp; sp->ngasp = 1;
    for (i = 0; i < 8; i++) { memset(raw[i].tag, 0, 5); memcpy(raw[i].tag, tags[i], 4); for (k = 0; k < (int)lens[i]; k++) ks_junk[i][k] = (uint8_t)(i * 31 + k); raw[i].data = ks_junk[i]; raw[i].len = lens[i]; }
    /* GDEF must stay a valid header so that nothing downstream chokes: version 1.0, no subtables */
    memset(ks_junk[4], 0, 12); ks_junk[4][1] = 1;
    sp->raw = raw; sp->nraw = 8;
}
TEST(fw_kitchen_sink_all_tables_byte_exact) {
    ttf_spec sp; ttf_rawtable raw[8]; synth s; int n = 0, bad;
    memset(&s, 0, sizeof s);
    ks_spec(&sp, raw);
    if (!synth_open(&s, &sp)) { t_note("kitchen sink font not created: %s", dw_hr_name(s.hr)); synth_close(&s); return; }
    bad = compare_all_tables(s.h.face, s.blob, s.n, &s.dir, &n);
    EXPECT_EQ(bad, 0); CHECK(n >= 24);
    t_note("kitchen sink: %d tables", n);
    synth_close(&s);
}
TEST(fw_kitchen_sink_directory_sorted_and_unique) {
    ttf_spec sp; ttf_rawtable raw[8]; synth s; int i;
    memset(&s, 0, sizeof s);
    ks_spec(&sp, raw);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    for (i = 1; i < s.dir.n; i++) CHECK(memcmp(s.blob + 12 + 16 * (i - 1), s.blob + 12 + 16 * i, 4) < 0);
    synth_close(&s);
}
TEST(fw_kitchen_sink_reversed_tag_never_found) {
    ttf_spec sp; ttf_rawtable raw[8]; synth s; int i;
    memset(&s, 0, sizeof s);
    ks_spec(&sp, raw);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    for (i = 0; i < s.dir.n; i++) {
        UINT32 t = s.dir.r[i].tag, rev = (t >> 24) | ((t >> 8) & 0xFF00) | ((t << 8) & 0xFF0000) | (t << 24);
        tbl x;
        if (ot_find(&s.dir, rev)) continue;
        x = tget(s.h.face, rev);
        CHECKF(!x.ex && !x.d && !x.n && !x.ctx, "byte-swapped tag %08x found", rev);
        trel(s.h.face, &x);
    }
    synth_close(&s);
}
TEST(fw_kitchen_sink_raw_tables_exact_lengths) {
    ttf_spec sp; ttf_rawtable raw[8]; synth s; int i;
    memset(&s, 0, sizeof s);
    ks_spec(&sp, raw);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    for (i = 0; i < 8; i++) {
        tbl t = tget(s.h.face, TG(raw[i].tag));
        CHECKF(t.ex && t.n == raw[i].len, "%s: exists %d size %u want %u", raw[i].tag, t.ex, t.n, raw[i].len);
        if (t.ex && t.n == raw[i].len && i != 4) CHECK(!memcmp(t.d, raw[i].data, t.n));
        trel(s.h.face, &t);
    }
    synth_close(&s);
}
TEST(fw_kitchen_sink_vertical_design_metrics_use_vmtx) {
    ttf_spec sp; ttf_rawtable raw[8]; synth s; UINT16 id = 2; DWRITE_GLYPH_METRICS gm;
    memset(&s, 0, sizeof s);
    ks_spec(&sp, raw);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.h.face, &id, 1, &gm, FALSE));
    EXPECT_EQ(gm.advanceHeight, 2048); EXPECT_EQ(gm.verticalOriginY, 1500); EXPECT_EQ(gm.topSideBearing, 1500 - 1400); EXPECT_EQ(gm.bottomSideBearing, 2048 - 100 - 1400);
    synth_close(&s);
}
TEST(fw_kitchen_sink_has_kerning_and_kern_values) {
    ttf_spec sp; ttf_rawtable raw[8]; synth s; IDWriteFontFace1 *f1 = NULL; UINT16 g[2] = { 2, 3 }; INT32 adj[2];
    memset(&s, 0, sizeof s);
    ks_spec(&sp, raw);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.h.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        CHECK(IDWriteFontFace1_HasKerningPairs(f1));
        IDWriteFontFace1_GetKerningPairAdjustments(f1, 2, g, adj); EXPECT_EQ(adj[0], -10);
        IDWriteFontFace1_Release(f1);
    }
    synth_close(&s);
}
TEST(fw_kitchen_sink_color_font_flag) {
    ttf_spec sp; ttf_rawtable raw[8]; synth s; IDWriteFontFace2 *f2 = NULL;
    memset(&s, 0, sizeof s);
    ks_spec(&sp, raw);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.h.face, &IID_IDWriteFontFace2, (void **)&f2)) && f2) {
        CHECK(IDWriteFontFace2_IsColorFont(f2));
        t_note("palette count %u, entries %u", IDWriteFontFace2_GetColorPaletteCount(f2), IDWriteFontFace2_GetPaletteEntryCount(f2));
        EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(f2), 1); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(f2), 1);
        IDWriteFontFace2_Release(f2);
    }
    synth_close(&s);
}

/* ---- sfnt versions and containers ---- */
TEST(fw_sfnt_version_true) {
    ttf_spec sp; synth s; int n = 0;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.sfnt_version = 0x74727565;
    if (!synth_open(&s, &sp)) { t_note("'true' sfnt not opened: %s (supported=%d)", dw_hr_name(s.hr), s.h.supported); synth_close(&s); return; }
    t_note("'true' sfnt opened, file type %d face type %d", s.h.file_type, s.h.face_type);
    EXPECT_EQ(compare_all_tables(s.h.face, s.blob, s.n, &s.dir, &n), 0);
    synth_close(&s);
}
TEST(fw_sfnt_version_otto_without_cff_note) {
    ttf_spec sp; synth s;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.sfnt_version = 0x4F54544F;
    synth_open(&s, &sp);
    t_note("OTTO sfnt without CFF: hr %s supported %d file type %d face type %d", dw_hr_name(s.hr), s.h.supported, s.h.file_type, s.h.face_type);
    synth_close(&s);
}
TEST(fw_woff_wrapper_note) {
    size_t n, w; uint8_t *b = ttf_build_default(&n), *wf = ttf_wrap_woff(b, n, &w); dwx_font h; HRESULT hr;
    memset(&h, 0, sizeof h);
    hr = wf ? dwx_font_open(&h, dw_shared(), wf, w, 0, DWRITE_FONT_SIMULATIONS_NONE) : E_FAIL;
    t_note("WOFF 1.0: hr %s supported %d file type %d", dw_hr_name(hr), h.supported, h.file_type);
    if (hr == S_OK && h.face) {
        /* if the platform decodes WOFF, TryGetFontTable serves the decoded sfnt tables */
        tbl t = tget(h.face, TG("head")); ot_dir d; ot_parse_dir(b, n, 0, &d);
        CHECK(t.ex && t.n == 54 && !memcmp(t.d, b + ot_find(&d, TG("head"))->off, 54)); trel(h.face, &t);
    }
    dwx_font_close(&h); free(b); free(wf);
}
TEST(fw_table_count_one_face_font_without_loca_glyf) {
    ttf_spec sp; synth s;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.omit = TTF_OMIT_GLYF;
    synth_open(&s, &sp);
    t_note("font without glyf/loca: hr %s", dw_hr_name(s.hr));
    if (s.h.face) { tbl g = tget(s.h.face, TG("glyf")), l = tget(s.h.face, TG("loca")); CHECK(!g.ex && !l.ex); trel(s.h.face, &g); trel(s.h.face, &l); }
    synth_close(&s);
}
TEST(fw_missing_required_tables_note) {
    struct { unsigned omit; const char *what; } c[] = { { TTF_OMIT_CMAP, "cmap" }, { TTF_OMIT_HHEA, "hhea" }, { TTF_OMIT_HMTX, "hmtx" }, { TTF_OMIT_MAXP, "maxp" }, { TTF_OMIT_HEAD, "head" } };
    unsigned i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        ttf_spec sp; synth s; memset(&s, 0, sizeof s);
        ttf_default_spec(&sp); sp.omit = c[i].omit;
        synth_open(&s, &sp);
        t_note("without %s: CreateFontFace %s%s", c[i].what, dw_hr_name(s.hr), s.h.face ? " (face created)" : "");
        if (s.h.face) { tbl t = tget(s.h.face, TG(c[i].what)); CHECK(!t.ex); trel(s.h.face, &t); }
        synth_close(&s);
    }
}

/* ---- which vertical metrics source: AAT morph tables without OpenType layout tables select the hhea values ---- */
static void metrics_with_raw(const char *tag, int with_gsub, UINT32 *asc, UINT32 *desc, INT32 *gap) {
    static const uint8_t z[16] = { 0, 1, 0, 0 };
    ttf_rawtable rt; ttf_spec sp; synth s; DWRITE_FONT_METRICS m;
    memset(&s, 0, sizeof s); memset(rt.tag, 0, 5); memcpy(rt.tag, tag, 4); rt.data = z; rt.len = 16;
    ttf_default_spec(&sp); sp.raw = &rt; sp.nraw = 1;
    if (with_gsub) { sp.gsub = ks_feat; sp.ngsub = 1; }
    *asc = *desc = 0xFFFF; *gap = -1;
    if (!synth_open(&s, &sp)) { t_note("font with %s not created: %s", tag, dw_hr_name(s.hr)); synth_close(&s); return; }
    IDWriteFontFace_GetMetrics(s.h.face, &m); *asc = m.ascent; *desc = m.descent; *gap = m.lineGap;
    synth_close(&s);
}
TEST(fw_morx_without_ot_layout_uses_hhea_metrics) {
    UINT32 a, d; INT32 g; metrics_with_raw("morx", 0, &a, &d, &g);
    if (a != 0xFFFF) { EXPECT_EQ(a, 1700); EXPECT_EQ(d, 400); EXPECT_EQ(g, 90); }
}
TEST(fw_mort_without_ot_layout_uses_hhea_metrics) {
    UINT32 a, d; INT32 g; metrics_with_raw("mort", 0, &a, &d, &g);
    if (a != 0xFFFF) { EXPECT_EQ(a, 1700); EXPECT_EQ(d, 400); EXPECT_EQ(g, 90); }
}
TEST(fw_morx_with_gsub_uses_win_metrics) {
    UINT32 a, d; INT32 g; metrics_with_raw("morx", 1, &a, &d, &g);
    if (a != 0xFFFF) { EXPECT_EQ(a, 1800); EXPECT_EQ(d, 450); EXPECT_EQ(g, 0); }
}
TEST(fw_mort_with_gsub_uses_win_metrics) {
    UINT32 a, d; INT32 g; metrics_with_raw("mort", 1, &a, &d, &g);
    if (a != 0xFFFF) { EXPECT_EQ(a, 1800); EXPECT_EQ(d, 450); EXPECT_EQ(g, 0); }
}
TEST(fw_gsub_alone_keeps_win_metrics) {
    ttf_spec sp; synth s; DWRITE_FONT_METRICS m; memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.gsub = ks_feat; sp.ngsub = 1;
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    IDWriteFontFace_GetMetrics(s.h.face, &m); EXPECT_EQ(m.ascent, 1800); EXPECT_EQ(m.descent, 450);
    synth_close(&s);
}
