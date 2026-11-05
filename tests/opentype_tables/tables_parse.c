/* Byte-level parsing of tables fetched through TryGetFontTable from synthetic fonts: the test parses the returned bytes and compares
 * with the values the font was generated from. Because the generator is a pure function of the spec, every expected number is exact. */
#include "ot.h"

static uint8_t *fetch_copy(synth *s, const char *tag, UINT32 *n) {
    uint8_t *c = dw_face_table(s->h.face, TG(tag), n);
    return c;
}
static long long rd(const uint8_t *t, int off, int sz) {
    switch (sz) {
    case 1: return t[off];
    case 2: return BE16(t + off);
    case -2: return BE16S(t + off);
    case 4: return (long long)(uint32_t)BE32(t + off);
    case -4: return (long long)(int32_t)BE32(t + off);
    }
    return 0;
}
static void fld_check(const ttf_spec *sp, const char *tag, int off, int sz, long long want, const char *what) {
    synth s; UINT32 n = 0; uint8_t *t;
    memset(&s, 0, sizeof s);
    if (!synth_open(&s, sp)) { t_skip("open failed %s", dw_hr_name(s.hr)); synth_close(&s); return; }
    t = fetch_copy(&s, tag, &n);
    if (!t) { CHECKF(0, "table %s missing", tag); synth_close(&s); return; }
    if ((UINT32)(off + (sz < 0 ? -sz : sz)) > n) CHECKF(0, "%s: table %s too short (%u) for offset %d", what, tag, n, off);
    else { long long got = rd(t, off, sz); CHECKF(got == want, "%s: %s+%d got %lld want %lld", what, tag, off, got, want); }
    free(t); synth_close(&s);
}
#define FLD(id, setup, tag, off, sz, expr) TEST(tp_##id) { ttf_spec sp; ttf_default_spec(&sp); setup; fld_check(&sp, tag, off, sz, (long long)(expr), #id); }
#define NOP (void)0

/* ---- head ---- */
FLD(head_version, NOP, "head", 0, 4, 0x00010000)
FLD(head_revision, NOP, "head", 4, 4, sp.revision_fixed)
FLD(head_revision_custom, sp.revision_fixed = 0x00028000, "head", 4, 4, 0x00028000)
FLD(head_magic, NOP, "head", 12, 4, 0x5F0F3CF5)
FLD(head_flags_default, NOP, "head", 16, 2, 3)
FLD(head_flags_custom, sp.head_flags = 0x000B, "head", 16, 2, 0x000B)
FLD(head_upm_2048, NOP, "head", 18, 2, 2048)
FLD(head_upm_1000, sp.upm = 1000, "head", 18, 2, 1000)
FLD(head_upm_1024, sp.upm = 1024, "head", 18, 2, 1024)
FLD(head_upm_4096, sp.upm = 4096, "head", 18, 2, 4096)
FLD(head_upm_16384, sp.upm = 16384, "head", 18, 2, 16384)
FLD(head_macstyle_regular, NOP, "head", 44, 2, 0)
FLD(head_macstyle_bold, sp.mac_style = 1, "head", 44, 2, 1)
FLD(head_macstyle_italic, sp.mac_style = 2, "head", 44, 2, 2)
FLD(head_macstyle_bolditalic, sp.mac_style = 3, "head", 44, 2, 3)
FLD(head_lowest_rec_ppem, NOP, "head", 46, 2, 8)
FLD(head_direction_hint, NOP, "head", 48, 2, 2)
FLD(head_loca_format_short, NOP, "head", 50, -2, 0)
FLD(head_loca_format_long, sp.long_loca = 1, "head", 50, -2, 1)
FLD(head_glyph_data_format, NOP, "head", 52, -2, 0)
TEST(tp_head_length_54) {
    NEED_SYNTH(s); tbl t = tget(s.h.face, TG("head")); EXPECT_EQ(t.n, 54); trel(s.h.face, &t); synth_close(&s);
}
TEST(tp_head_checksum_adjustment_whole_file) {
    NEED_SYNTH(s);
    /* head.checkSumAdjustment = 0xB1B0AFBA - checksum(file) with the field itself counted as the stored value, so the sum of the
     * whole file is the constant 0xB1B0AFBA. */
    EXPECT_EQ(ot_checksum(s.blob, (uint32_t)s.n), 0xB1B0AFBAu);
    synth_close(&s);
}
TEST(tp_head_bbox_is_union_of_glyph_boxes) {
    NEED_SYNTH(s);
    ot_font f; ot_glyph g; int gid, x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768; UINT32 n;
    uint8_t *h = fetch_copy(&s, "head", &n);
    CHECK(h && ot_font_init(&f, s.blob, s.n, &s.dir) == 0);
    if (h) {
        for (gid = 0; gid < f.nglyphs; gid++) if (ot_glyph_get(&f, gid, &g) == 0 && g.has_outline) {
            if (g.x0 < x0) x0 = g.x0; if (g.y0 < y0) y0 = g.y0; if (g.x1 > x1) x1 = g.x1; if (g.y1 > y1) y1 = g.y1;
        }
        EXPECT_EQ(rd(h, 36, -2), x0); EXPECT_EQ(rd(h, 38, -2), y0); EXPECT_EQ(rd(h, 40, -2), x1); EXPECT_EQ(rd(h, 42, -2), y1);
    }
    free(h); synth_close(&s);
}
TEST(tp_head_created_modified_fixed) {
    NEED_SYNTH(s); UINT32 n; uint8_t *h = fetch_copy(&s, "head", &n);
    if (h) { EXPECT_EQ(rd(h, 20, 4), 0); EXPECT_EQ((uint32_t)rd(h, 24, 4), 0xD0000000u); EXPECT_EQ(rd(h, 28, 4), 0); EXPECT_EQ((uint32_t)rd(h, 32, 4), 0xD0000000u); }
    free(h); synth_close(&s);
}

/* ---- hhea ---- */
FLD(hhea_version, NOP, "hhea", 0, 4, 0x00010000)
FLD(hhea_ascender, NOP, "hhea", 4, -2, sp.hhea_asc)
FLD(hhea_descender, NOP, "hhea", 6, -2, sp.hhea_desc)
FLD(hhea_linegap, NOP, "hhea", 8, -2, sp.hhea_gap)
FLD(hhea_ascender_custom, sp.hhea_asc = 2222, "hhea", 4, -2, 2222)
FLD(hhea_descender_custom, sp.hhea_desc = -777, "hhea", 6, -2, -777)
FLD(hhea_linegap_custom, sp.hhea_gap = 513, "hhea", 8, -2, 513)
FLD(hhea_linegap_zero, sp.hhea_gap = 0, "hhea", 8, -2, 0)
FLD(hhea_advance_max, NOP, "hhea", 10, 2, TTF_NOTDEF_ADV)
FLD(hhea_caret_rise_upright, NOP, "hhea", 18, -2, 1)
FLD(hhea_caret_run_upright, NOP, "hhea", 20, -2, 0)
FLD(hhea_caret_offset, NOP, "hhea", 22, -2, 0)
FLD(hhea_caret_rise_italic, sp.italic_angle_fixed = -12 * 65536, "hhea", 18, -2, 1000)
FLD(hhea_caret_run_italic, sp.italic_angle_fixed = -12 * 65536, "hhea", 20, -2, 213)
FLD(hhea_metric_data_format, NOP, "hhea", 32, -2, 0)
FLD(hhea_num_hmetrics_default, NOP, "hhea", 34, 2, TTF_DEF_NGLYPHS)
TEST(tp_hhea_num_hmetrics_compressed) {
    ttf_spec sp; ttf_default_spec(&sp); sp.compress_hmtx = 1;
    int nlong = sp.nglyphs; while (nlong > 1 && sp.glyphs[nlong - 1].adv == sp.glyphs[nlong - 2].adv) nlong--;
    fld_check(&sp, "hhea", 34, 2, nlong, "numberOfHMetrics compressed");
}
TEST(tp_hhea_extremes_match_glyphs) {
    NEED_SYNTH(s); ot_font f; ot_glyph g; int gid, minl = 32767, minr = 32767, xext = -32768; UINT32 n; uint8_t *h = fetch_copy(&s, "hhea", &n);
    CHECK(h && ot_font_init(&f, s.blob, s.n, &s.dir) == 0);
    if (h) {
        for (gid = 0; gid < f.nglyphs; gid++) if (ot_glyph_get(&f, gid, &g) == 0 && g.has_outline) {
            int rsb = g.adv - g.lsb - (g.x1 - g.x0), ext = g.lsb + (g.x1 - g.x0);
            if (g.lsb < minl) minl = g.lsb; if (rsb < minr) minr = rsb; if (ext > xext) xext = ext;
        }
        EXPECT_EQ(rd(h, 12, -2), minl); EXPECT_EQ(rd(h, 14, -2), minr); EXPECT_EQ(rd(h, 16, -2), xext);
    }
    free(h); synth_close(&s);
}
TEST(tp_hhea_length_36) { NEED_SYNTH(s); tbl t = tget(s.h.face, TG("hhea")); EXPECT_EQ(t.n, 36); trel(s.h.face, &t); synth_close(&s); }

/* ---- maxp ---- */
FLD(maxp_version_1, NOP, "maxp", 0, 4, 0x00010000)
FLD(maxp_num_glyphs, NOP, "maxp", 4, 2, TTF_DEF_NGLYPHS)
FLD(maxp_max_zones, NOP, "maxp", 14, 2, 2)
FLD(maxp_max_component_elements, NOP, "maxp", 28, 2, 2)
TEST(tp_maxp_length_32) { NEED_SYNTH(s); tbl t = tget(s.h.face, TG("maxp")); EXPECT_EQ(t.n, 32); trel(s.h.face, &t); synth_close(&s); }
TEST(tp_maxp_point_and_contour_maxima) {
    ttf_spec sp; int i, k, mp = 0, mc = 0, mcp = 0, mcc = 0, mcomp = 0;
    ttf_default_spec(&sp);
    for (i = 0; i < sp.nglyphs; i++) {
        const ttf_glyph *g = &sp.glyphs[i];
        if (g->ncomp > 0) {
            int cp = 0, cc = 0;
            for (k = 0; k < g->ncomp; k++) { cp += sp.glyphs[g->comps[k].gid].npts; cc += sp.glyphs[g->comps[k].gid].ncont; }
            if (cp > mcp) mcp = cp; if (cc > mcc) mcc = cc; if (g->ncomp > mcomp) mcomp = g->ncomp;
        } else { if (g->npts > mp) mp = g->npts; if (g->ncont > mc) mc = g->ncont; }
    }
    fld_check(&sp, "maxp", 6, 2, mp, "maxPoints");
    fld_check(&sp, "maxp", 8, 2, mc, "maxContours");
    fld_check(&sp, "maxp", 10, 2, mcp, "maxCompositePoints");
    fld_check(&sp, "maxp", 12, 2, mcc, "maxCompositeContours");
    fld_check(&sp, "maxp", 28, 2, mcomp, "maxComponentElements");
}
TEST(tp_maxp_numglyphs_matches_glyph_count_api) {
    NEED_SYNTH(s); UINT32 n; uint8_t *m = fetch_copy(&s, "maxp", &n);
    /* GetGlyphCount reports maxp.numGlyphs. */
    if (m) EXPECT_EQ(IDWriteFontFace_GetGlyphCount(s.h.face), rd(m, 4, 2));
    EXPECT_EQ(IDWriteFontFace_GetGlyphCount(s.h.face), TTF_DEF_NGLYPHS);
    free(m); synth_close(&s);
}

/* ---- hmtx ---- */
static void hmtx_check(ttf_spec *sp, const char *what) {
    synth s; UINT32 n = 0; uint8_t *h; int i, nlong;
    memset(&s, 0, sizeof s);
    if (!synth_open(&s, sp)) { t_skip("open failed"); synth_close(&s); return; }
    h = fetch_copy(&s, "hmtx", &n);
    nlong = sp->nglyphs; if (sp->compress_hmtx) while (nlong > 1 && sp->glyphs[nlong - 1].adv == sp->glyphs[nlong - 2].adv) nlong--;
    EXPECT_EQ(n, 4 * nlong + 2 * (sp->nglyphs - nlong));
    if (h && n == (UINT32)(4 * nlong + 2 * (sp->nglyphs - nlong))) {
        ot_font f; ot_glyph g;
        CHECK(ot_font_init(&f, s.blob, s.n, &s.dir) == 0);
        for (i = 0; i < sp->nglyphs; i++) {
            ot_glyph_get(&f, i, &g);
            CHECKF(g.adv == sp->glyphs[i].adv, "%s: gid %d advance %d want %d", what, i, g.adv, sp->glyphs[i].adv);
        }
    }
    free(h); synth_close(&s);
}
TEST(tp_hmtx_all_glyphs_uncompressed) { ttf_spec sp; ttf_default_spec(&sp); hmtx_check(&sp, "default"); }
TEST(tp_hmtx_all_glyphs_compressed) { ttf_spec sp; ttf_default_spec(&sp); sp.compress_hmtx = 1; hmtx_check(&sp, "compressed"); }
FLD(hmtx_notdef_advance, NOP, "hmtx", 0, 2, TTF_NOTDEF_ADV)
FLD(hmtx_space_advance, NOP, "hmtx", 4, 2, TTF_DEF_ADV)
FLD(hmtx_space_lsb, NOP, "hmtx", 6, -2, 0)
FLD(hmtx_A_advance, NOP, "hmtx", 4 * 2, 2, TTF_DEF_ADV)
FLD(hmtx_A_lsb, NOP, "hmtx", 4 * 2 + 2, -2, TTF_BOX_X0)
FLD(hmtx_empty500_advance, NOP, "hmtx", 4 * TTF_GID_EMPTY500, 2, 500)
FLD(hmtx_ring_advance, NOP, "hmtx", 4 * TTF_GID_RING, 2, TTF_DEF_ADV)

/* ---- cmap ---- */
TEST(tp_cmap_header_and_subtables) {
    NEED_SYNTH(s); UINT32 n; uint8_t *c = fetch_copy(&s, "cmap", &n); int fmt = -1;
    if (c) {
        EXPECT_EQ(rd(c, 0, 2), 0); EXPECT_EQ(rd(c, 2, 2), 2);
        CHECK(ot_cmap_has(c, n, 3, 1, &fmt)); EXPECT_EQ(fmt, 4);
        CHECK(ot_cmap_has(c, n, 3, 10, &fmt)); EXPECT_EQ(fmt, 12);
        CHECK(!ot_cmap_has(c, n, 1, 0, NULL)); CHECK(!ot_cmap_has(c, n, 0, 3, NULL));
    } else CHECK(0);
    free(c); synth_close(&s);
}
TEST(tp_cmap_encoding_records_sorted) {
    NEED_SYNTH(s); UINT32 n; uint8_t *c = fetch_copy(&s, "cmap", &n);
    if (c && n >= 20) {
        CHECK(rd(c, 4, 2) == 3 && rd(c, 6, 2) == 1);
        CHECK(rd(c, 12, 2) == 3 && rd(c, 14, 2) == 10);
        CHECK(rd(c, 8, 4) < n && rd(c, 16, 4) < n);
    } else CHECK(0);
    free(c); synth_close(&s);
}
static void cmap_scan(int plat, int enc, unsigned maxcp) {
    NEED_SYNTH(s); UINT32 n; uint8_t *c = fetch_copy(&s, "cmap", &n); unsigned cp, bad = 0;
    if (!c) { CHECK(0); synth_close(&s); return; }
    for (cp = 0; cp <= maxcp; cp++) {
        long want = ttf_default_gid(cp), got = ot_cmap_lookup(c, n, plat, enc, cp);
        if (got != want) { if (bad++ < 5) CHECKF(0, "cp U+%04X (%d,%d) got %ld want %ld", cp, plat, enc, got, want); }
    }
    CHECK(bad == 0);
    free(c); synth_close(&s);
}
TEST(tp_cmap_format4_matches_default_map_bmp) { cmap_scan(3, 1, 0x2FFF); }
TEST(tp_cmap_format12_matches_default_map) { cmap_scan(3, 10, 0x2FFF); }
TEST(tp_cmap_format12_beyond_bmp_unmapped) {
    NEED_SYNTH(s); UINT32 n; uint8_t *c = fetch_copy(&s, "cmap", &n);
    if (c) { CHECK(ot_cmap_lookup(c, n, 3, 10, 0x10000) == 0); CHECK(ot_cmap_lookup(c, n, 3, 10, 0x1F600) == 0); CHECK(ot_cmap_lookup(c, n, 3, 10, 0x10FFFF) == 0); }
    free(c); synth_close(&s);
}
TEST(tp_cmap_missing_subtable_lookup) {
    NEED_SYNTH(s); UINT32 n; uint8_t *c = fetch_copy(&s, "cmap", &n);
    if (c) EXPECT_EQ(ot_cmap_lookup(c, n, 1, 0, 'A'), -2);
    free(c); synth_close(&s);
}
TEST(tp_cmap_alternate_formats) {
    /* a font with a format 6 (0,3) subtable, a format 0 (1,0) subtable and a format 13 (3,10) subtable */
    static const ttf_map m[] = { { 'A', 2 }, { 'B', 3 }, { 'C', 4 } };
    static const ttf_cmap cm[] = { { 6, 0, 3, 3, m, 0, NULL }, { 0, 1, 0, 3, m, 0, NULL } };
    ttf_spec sp; synth s; UINT32 n; uint8_t *c; int fmt = 0;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.cmaps = cm; sp.ncmaps = 2;
    if (!synth_open(&s, &sp)) { t_note("face over format 6/0 cmap not created: %s", dw_hr_name(s.hr)); synth_close(&s); return; }
    c = fetch_copy(&s, "cmap", &n);
    if (c) {
        CHECK(ot_cmap_has(c, n, 0, 3, &fmt)); EXPECT_EQ(fmt, 6);
        CHECK(ot_cmap_has(c, n, 1, 0, &fmt)); EXPECT_EQ(fmt, 0);
        EXPECT_EQ(ot_cmap_lookup(c, n, 0, 3, 'B'), 3); EXPECT_EQ(ot_cmap_lookup(c, n, 1, 0, 'C'), 4); EXPECT_EQ(ot_cmap_lookup(c, n, 0, 3, 'D'), 0);
    }
    free(c); synth_close(&s);
}

/* ---- OS/2 ---- */
FLD(os2_version_default, NOP, "OS/2", 0, 2, 4)
FLD(os2_xavgcharwidth, NOP, "OS/2", 2, -2, sp.x_avg_width)
FLD(os2_weight_class, NOP, "OS/2", 4, 2, 400)
FLD(os2_weight_class_700, sp.weight = 700, "OS/2", 4, 2, 700)
FLD(os2_weight_class_100, sp.weight = 100, "OS/2", 4, 2, 100)
FLD(os2_weight_class_950, sp.weight = 950, "OS/2", 4, 2, 950)
FLD(os2_width_class, NOP, "OS/2", 6, 2, 5)
FLD(os2_width_class_3, sp.width = 3, "OS/2", 6, 2, 3)
FLD(os2_fstype_default, NOP, "OS/2", 8, 2, 0)
FLD(os2_fstype_installable_mask, sp.fs_type = 0x0008, "OS/2", 8, 2, 0x0008)
FLD(os2_subscript_xsize, NOP, "OS/2", 10, -2, TTF_DEF_UPM / 2)
FLD(os2_subscript_ysize, NOP, "OS/2", 12, -2, TTF_DEF_UPM / 2)
FLD(os2_subscript_xoffset, NOP, "OS/2", 14, -2, 0)
FLD(os2_subscript_yoffset, NOP, "OS/2", 16, -2, TTF_DEF_UPM / 7)
FLD(os2_superscript_xsize, NOP, "OS/2", 18, -2, TTF_DEF_UPM / 2)
FLD(os2_superscript_ysize, NOP, "OS/2", 20, -2, TTF_DEF_UPM / 2)
FLD(os2_superscript_xoffset, NOP, "OS/2", 22, -2, 0)
FLD(os2_superscript_yoffset, NOP, "OS/2", 24, -2, TTF_DEF_UPM / 3)
FLD(os2_strikeout_size, NOP, "OS/2", 26, -2, sp.strike_size)
FLD(os2_strikeout_pos, NOP, "OS/2", 28, -2, sp.strike_pos)
FLD(os2_strikeout_pos_custom, sp.strike_pos = 777, "OS/2", 28, -2, 777)
FLD(os2_unicode_range1, NOP, "OS/2", 42, 4, 1)
FLD(os2_unicode_range_custom, sp.unicode_range[0] = 0x80000003, "OS/2", 42, 4, 0x80000003)
FLD(os2_unicode_range2_custom, sp.unicode_range[1] = 0x20000000, "OS/2", 46, 4, 0x20000000)
FLD(os2_unicode_range3_custom, sp.unicode_range[2] = 0x00010001, "OS/2", 50, 4, 0x00010001)
FLD(os2_unicode_range4_custom, sp.unicode_range[3] = 0x80000000, "OS/2", 54, 4, 0x80000000)
FLD(os2_vendor_id, NOP, "OS/2", 58, 4, 0x44575453)
FLD(os2_fsselection_regular, NOP, "OS/2", 62, 2, 0x40)
FLD(os2_fsselection_use_typo, sp.fs_selection = 0xC0, "OS/2", 62, 2, 0xC0)
FLD(os2_first_char, NOP, "OS/2", 64, 2, 0x20)
FLD(os2_last_char, NOP, "OS/2", 66, 2, 'z')
FLD(os2_typo_ascender, NOP, "OS/2", 68, -2, sp.typo_asc)
FLD(os2_typo_descender, NOP, "OS/2", 70, -2, sp.typo_desc)
FLD(os2_typo_linegap, NOP, "OS/2", 72, -2, sp.typo_gap)
FLD(os2_win_ascent, NOP, "OS/2", 74, 2, sp.win_asc)
FLD(os2_win_descent, NOP, "OS/2", 76, 2, sp.win_desc)
FLD(os2_codepage1, NOP, "OS/2", 78, 4, 1)
FLD(os2_codepage_custom, sp.codepage_range[0] = 0x00020001; sp.codepage_range[1] = 0x80000000u, "OS/2", 78, 4, 0x00020001)
FLD(os2_xheight, NOP, "OS/2", 86, -2, sp.x_height)
FLD(os2_capheight, NOP, "OS/2", 88, -2, sp.cap_height)
FLD(os2_default_char, NOP, "OS/2", 90, 2, 0)
FLD(os2_break_char, NOP, "OS/2", 92, 2, 0x20)
FLD(os2_max_context, NOP, "OS/2", 94, 2, 1)
TEST(tp_os2_panose_zero) {
    NEED_SYNTH(s); UINT32 n; uint8_t *o = fetch_copy(&s, "OS/2", &n); int i, nz = 0;
    if (o) for (i = 32; i < 42; i++) nz += o[i] != 0;
    EXPECT_EQ(nz, 0); free(o); synth_close(&s);
}
static void os2_len_check(int v, UINT32 want) {
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *o;
    ttf_default_spec(&sp); sp.os2_version = v;
    memset(&s, 0, sizeof s);
    if (!synth_open(&s, &sp)) { t_note("face over OS/2 v%d not created: %s", v, dw_hr_name(s.hr)); synth_close(&s); return; }
    o = fetch_copy(&s, "OS/2", &n);
    /* TryGetFontTable returns the directory length verbatim: the table is exactly as large as the font says. */
    EXPECT_EQ(n, want);
    if (o) EXPECT_EQ(rd(o, 0, 2), v);
    free(o); synth_close(&s);
}
TEST(tp_os2_version0_length_78) { os2_len_check(0, 78); }
TEST(tp_os2_version1_length_86) { os2_len_check(1, 86); }
TEST(tp_os2_version2_length_96) { os2_len_check(2, 96); }
TEST(tp_os2_version3_length_96) { os2_len_check(3, 96); }
TEST(tp_os2_version4_length_96) { os2_len_check(4, 96); }
TEST(tp_os2_version5_length_100) { os2_len_check(5, 100); }

/* ---- name ---- */
static void utf16be_to_utf8(const uint8_t *p, int len, char *out, int cap) {
    WCHAR w[200]; int n = len / 2, i; if (n > 199) n = 199;
    for (i = 0; i < n; i++) w[i] = (WCHAR)BE16(p + 2 * i);
    w[n] = 0; dw_narrow(w, out, cap);
}
TEST(tp_name_records_default) {
    NEED_SYNTH(s); UINT32 n; uint8_t *t = fetch_copy(&s, "name", &n); uint8_t buf[256]; char u8[256]; int l;
    if (!t) { CHECK(0); synth_close(&s); return; }
    CHECK(rd(t, 0, 2) == 0); CHECK(ot_name_count(t, n) == 8);
    l = ot_name_get(t, n, 3, 1, 0x409, 1, buf, sizeof buf); CHECK(l > 0); if (l > 0) { utf16be_to_utf8(buf, l, u8, sizeof u8); EXPECT_STR(u8, "DwTest"); }
    l = ot_name_get(t, n, 3, 1, 0x409, 2, buf, sizeof buf); CHECK(l > 0); if (l > 0) { utf16be_to_utf8(buf, l, u8, sizeof u8); EXPECT_STR(u8, "Regular"); }
    l = ot_name_get(t, n, 3, 1, 0x409, 4, buf, sizeof buf); CHECK(l > 0); if (l > 0) { utf16be_to_utf8(buf, l, u8, sizeof u8); EXPECT_STR(u8, "DwTest"); }
    l = ot_name_get(t, n, 3, 1, 0x409, 6, buf, sizeof buf); CHECK(l > 0); if (l > 0) { utf16be_to_utf8(buf, l, u8, sizeof u8); EXPECT_STR(u8, "DwTest-Regular"); }
    l = ot_name_get(t, n, 1, 0, 0, 1, buf, sizeof buf); CHECK(l == 6); if (l == 6) CHECK(!memcmp(buf, "DwTest", 6));
    l = ot_name_get(t, n, 3, 1, 0x409, 3, buf, sizeof buf); CHECK(l < 0);
    free(t); synth_close(&s);
}
TEST(tp_name_non_ascii_utf16) {
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *t; uint8_t buf[256]; char u8[256]; int l;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); ttf_clear_names(&sp);
    ttf_add_name(&sp, 3, 1, 0x409, 1, "T\xC3\xA9st \xE6\x97\xA5\xE6\x9C\xAC"); ttf_add_name(&sp, 3, 1, 0x409, 2, "Regular");
    ttf_add_name(&sp, 3, 1, 0x409, 4, "T\xC3\xA9st \xE6\x97\xA5\xE6\x9C\xAC"); ttf_add_name(&sp, 3, 1, 0x409, 6, "Test-Regular");
    if (!synth_open(&s, &sp)) { t_note("not created: %s", dw_hr_name(s.hr)); synth_close(&s); return; }
    t = fetch_copy(&s, "name", &n);
    if (t) {
        l = ot_name_get(t, n, 3, 1, 0x409, 1, buf, sizeof buf);
        CHECK(l == 14);   /* 7 UTF-16 code units */
        if (l > 0) { utf16be_to_utf8(buf, l, u8, sizeof u8); EXPECT_STR(u8, "T\xC3\xA9st \xE6\x97\xA5\xE6\x9C\xAC"); }
    } else CHECK(0);
    free(t); synth_close(&s);
}
TEST(tp_name_table_string_storage_inside_table) {
    NEED_SYNTH(s); UINT32 n; uint8_t *t = fetch_copy(&s, "name", &n); int i, cnt;
    if (t) {
        unsigned so = BE16(t + 4); cnt = ot_name_count(t, n);
        CHECK(so >= 6u + 12u * cnt); CHECK(so <= n);
        for (i = 0; i < cnt; i++) { const uint8_t *r = t + 6 + 12 * i; CHECK(so + BE16(r + 10) + BE16(r + 8) <= n); }
    } else CHECK(0);
    free(t); synth_close(&s);
}
TEST(tp_name_styled_family_ladder_ids) {
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *t; uint8_t buf[256]; char u8[256]; int l;
    memset(&s, 0, sizeof s);
    ttf_style_spec(&sp, "Ladder", 300, 5, 0);
    if (!synth_open(&s, &sp)) { t_note("not created"); synth_close(&s); return; }
    t = fetch_copy(&s, "name", &n);
    if (t) {
        /* weight 300 uses the typographic grouping: id1 "Ladder Light", id16 "Ladder", id17 "Light" */
        l = ot_name_get(t, n, 3, 1, 0x409, 16, buf, sizeof buf); CHECK(l > 0); if (l > 0) { utf16be_to_utf8(buf, l, u8, sizeof u8); EXPECT_STR(u8, "Ladder"); }
        l = ot_name_get(t, n, 3, 1, 0x409, 17, buf, sizeof buf); CHECK(l > 0); if (l > 0) { utf16be_to_utf8(buf, l, u8, sizeof u8); EXPECT_STR(u8, "Light"); }
        l = ot_name_get(t, n, 3, 1, 0x409, 1, buf, sizeof buf); CHECK(l > 0); if (l > 0) { utf16be_to_utf8(buf, l, u8, sizeof u8); EXPECT_STR(u8, "Ladder Light"); }
    } else CHECK(0);
    free(t); synth_close(&s);
}

/* ---- post ---- */
FLD(post_version_3, NOP, "post", 0, 4, 0x00030000)
FLD(post_italic_angle_upright, NOP, "post", 4, 4, 0)
FLD(post_italic_angle_italic, sp.italic_angle_fixed = -12 * 65536, "post", 4, -4, -12 * 65536)
FLD(post_underline_position, NOP, "post", 8, -2, sp.underline_pos)
FLD(post_underline_thickness, NOP, "post", 10, -2, sp.underline_thick)
FLD(post_underline_custom_pos, sp.underline_pos = -333, "post", 8, -2, -333)
FLD(post_underline_custom_thick, sp.underline_thick = 77, "post", 10, -2, 77)
FLD(post_is_fixed_pitch_zero, NOP, "post", 12, 4, 0)
FLD(post_is_fixed_pitch_one, sp.is_fixed_pitch = 1, "post", 12, 4, 1)
TEST(tp_post_length_32) { NEED_SYNTH(s); tbl t = tget(s.h.face, TG("post")); EXPECT_EQ(t.n, 32); trel(s.h.face, &t); synth_close(&s); }

/* ---- glyf / loca ---- */
static void loca_check(int longfmt) {
    ttf_spec sp; synth s; UINT32 nl = 0, ng = 0; uint8_t *lo, *gl; int i, mono = 1;
    ttf_default_spec(&sp); sp.long_loca = longfmt;
    memset(&s, 0, sizeof s);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    lo = fetch_copy(&s, "loca", &nl); gl = fetch_copy(&s, "glyf", &ng);
    EXPECT_EQ(nl, (longfmt ? 4u : 2u) * (sp.nglyphs + 1));
    if (lo && gl && nl == (longfmt ? 4u : 2u) * (sp.nglyphs + 1)) {
        uint32_t prev = 0;
        for (i = 0; i <= sp.nglyphs; i++) {
            uint32_t o = longfmt ? BE32(lo + 4 * i) : 2u * BE16(lo + 2 * i);
            if (o < prev) mono = 0;
            if (o > ng) mono = 0;
            if (!longfmt && (o & 1)) mono = 0;
            prev = o;
        }
        CHECK(mono);
        EXPECT_EQ(prev <= ng, 1);
        /* empty glyphs (space, empty500) occupy no bytes */
        {
            uint32_t a, b;
            a = longfmt ? BE32(lo + 4 * TTF_GID_SPACE) : 2u * BE16(lo + 2 * TTF_GID_SPACE); b = longfmt ? BE32(lo + 4 * (TTF_GID_SPACE + 1)) : 2u * BE16(lo + 2 * (TTF_GID_SPACE + 1));
            EXPECT_EQ(a, b);
            a = longfmt ? BE32(lo + 4 * TTF_GID_EMPTY500) : 2u * BE16(lo + 2 * TTF_GID_EMPTY500); b = longfmt ? BE32(lo + 4 * (TTF_GID_EMPTY500 + 1)) : 2u * BE16(lo + 2 * (TTF_GID_EMPTY500 + 1));
            EXPECT_EQ(a, b);
        }
    }
    free(lo); free(gl); synth_close(&s);
}
TEST(tp_loca_short_format_consistent) { loca_check(0); }
TEST(tp_loca_long_format_consistent) { loca_check(1); }
TEST(tp_glyf_headers_match_spec_boxes) {
    NEED_SYNTH(s); ot_font f; ot_glyph g; ttf_spec sp; int i, k;
    ttf_default_spec(&sp);
    CHECK(ot_font_init(&f, s.blob, s.n, &s.dir) == 0);
    for (i = 0; i < sp.nglyphs; i++) {
        const ttf_glyph *sg = &sp.glyphs[i];
        ot_glyph_get(&f, i, &g);
        if (sg->ncomp == 0 && sg->npts == 0) { CHECKF(!g.has_outline, "gid %d should be empty", i); continue; }
        CHECKF(g.has_outline, "gid %d should have an outline", i);
        if (sg->ncomp == 0) {
            int x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768;
            for (k = 0; k < sg->npts; k++) { if (sg->pts[k].x < x0) x0 = sg->pts[k].x; if (sg->pts[k].x > x1) x1 = sg->pts[k].x; if (sg->pts[k].y < y0) y0 = sg->pts[k].y; if (sg->pts[k].y > y1) y1 = sg->pts[k].y; }
            CHECKF(g.x0 == x0 && g.y0 == y0 && g.x1 == x1 && g.y1 == y1, "gid %d box (%d,%d,%d,%d) want (%d,%d,%d,%d)", i, g.x0, g.y0, g.x1, g.y1, x0, y0, x1, y1);
            EXPECT_EQ(g.ncont, sg->ncont);
        } else EXPECT_EQ(g.ncont, -1);   /* composite */
    }
    synth_close(&s);
}
TEST(tp_glyf_ring_two_contours) { NEED_SYNTH(s); ot_font f; ot_glyph g; ot_font_init(&f, s.blob, s.n, &s.dir); ot_glyph_get(&f, TTF_GID_RING, &g); EXPECT_EQ(g.ncont, 2); synth_close(&s); }
TEST(tp_glyf_composite_marker) { NEED_SYNTH(s); ot_font f; ot_glyph g; ot_font_init(&f, s.blob, s.n, &s.dir); ot_glyph_get(&f, TTF_GID_COMPOSITE, &g); EXPECT_EQ(g.ncont, -1); synth_close(&s); }
TEST(tp_glyf_through_dwrite_equals_file) {
    NEED_SYNTH(s); tbl g = tget(s.h.face, TG("glyf")), l = tget(s.h.face, TG("loca"));
    const ot_rec *rg = ot_find(&s.dir, TG("glyf")), *rl = ot_find(&s.dir, TG("loca"));
    CHECK(g.ex && l.ex);
    CHECK(g.n == rg->len && !memcmp(g.d, s.blob + rg->off, g.n));
    CHECK(l.n == rl->len && !memcmp(l.d, s.blob + rl->off, l.n));
    trel(s.h.face, &g); trel(s.h.face, &l); synth_close(&s);
}

/* ---- kern ---- */
static const ttf_kernpair k_pairs[] = { { 2 + 'V' - 'A', 2, -100 }, { 2, 2 + 'V' - 'A', -120 }, { 3, 4, 40 }, { 5, 6, -1 }, { 28, 29, 300 }, { 2, 3, -32768 }, { 7, 8, 32767 } };
static void kern_spec(ttf_spec *sp) { ttf_default_spec(sp); sp->kern = k_pairs; sp->nkern = (int)(sizeof k_pairs / sizeof k_pairs[0]); }
TEST(tp_kern_header) {
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *k;
    memset(&s, 0, sizeof s);
    kern_spec(&sp);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    k = fetch_copy(&s, "kern", &n);
    if (k) {
        EXPECT_EQ(rd(k, 0, 2), 0); EXPECT_EQ(rd(k, 2, 2), 1);
        EXPECT_EQ(rd(k, 4, 2), 0); EXPECT_EQ(rd(k, 6, 2), 14 + 6 * sp.nkern); EXPECT_EQ(rd(k, 8, 2), 0x0001); EXPECT_EQ(rd(k, 10, 2), sp.nkern);
        EXPECT_EQ(n, 4u + 14 + 6u * sp.nkern);
    } else CHECK(0);
    free(k); synth_close(&s);
}
TEST(tp_kern_pairs_lookup) {
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *k; unsigned i; int v;
    memset(&s, 0, sizeof s);
    kern_spec(&sp);
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    k = fetch_copy(&s, "kern", &n);
    if (k) {
        for (i = 0; i < sizeof k_pairs / sizeof k_pairs[0]; i++) { v = 12345; CHECK(ot_kern_find(k, n, k_pairs[i].l, k_pairs[i].r, &v)); EXPECT_EQ(v, k_pairs[i].v); }
        CHECK(!ot_kern_find(k, n, 9, 9, &v)); CHECK(!ot_kern_find(k, n, 3, 2, &v));
    } else CHECK(0);
    free(k); synth_close(&s);
}
TEST(tp_kern_absent_in_default_font) { NEED_SYNTH(s); tbl t = tget(s.h.face, TG("kern")); CHECK(!t.ex); trel(s.h.face, &t); synth_close(&s); }

/* ---- GSUB / GPOS / COLR / CPAL / gasp / vertical / BASE ---- */
static const ttf_sub1 sub_single[] = { { 2, 28 }, { 3, 29 } };
static const ttf_sub4 sub_lig[] = { { { 2 + 5, 2 + 8 }, 2, 64 } };
static const ttf_feature feats[] = { { "smcp", TTF_LK_SINGLE, 2, sub_single }, { "liga", TTF_LK_LIGATURE, 1, sub_lig } };
TEST(tp_gsub_header_and_feature_tags) {
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *t; int i, found_smcp = 0, found_liga = 0;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.gsub = feats; sp.ngsub = 2;
    if (!synth_open(&s, &sp)) { t_note("GSUB font not created: %s", dw_hr_name(s.hr)); synth_close(&s); return; }
    t = fetch_copy(&s, "GSUB", &n);
    if (t) {
        unsigned so = BE16(t + 4), fo = BE16(t + 6), lo = BE16(t + 8);
        EXPECT_EQ(rd(t, 0, 4), 0x00010000);
        CHECK(so < n && fo < n && lo < n);
        {
            unsigned nf = BE16(t + fo);
            EXPECT_EQ(nf, 2);
            for (i = 0; i < (int)nf; i++) { if (!memcmp(t + fo + 2 + 6 * i, "smcp", 4)) found_smcp = 1; if (!memcmp(t + fo + 2 + 6 * i, "liga", 4)) found_liga = 1; }
            CHECK(found_smcp && found_liga);
            EXPECT_EQ(BE16(t + lo), 2);     /* one lookup per feature */
        }
    } else CHECK(0);
    free(t); synth_close(&s);
}
TEST(tp_gsub_absent_gpos_absent_gdef_absent_by_default) {
    NEED_SYNTH(s); tbl a = tget(s.h.face, TG("GSUB")), b = tget(s.h.face, TG("GPOS")), c = tget(s.h.face, TG("GDEF"));
    CHECK(!a.ex && !b.ex && !c.ex); synth_close(&s);
}
TEST(tp_gpos_kern_feature) {
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *t; int i, found = 0;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.gpos_kern = k_pairs; sp.ngpos_kern = (int)(sizeof k_pairs / sizeof k_pairs[0]);
    if (!synth_open(&s, &sp)) { t_note("GPOS font not created: %s", dw_hr_name(s.hr)); synth_close(&s); return; }
    t = fetch_copy(&s, "GPOS", &n);
    if (t) {
        unsigned fo = BE16(t + 6), nf;
        EXPECT_EQ(rd(t, 0, 4), 0x00010000);
        nf = BE16(t + fo); CHECK(nf >= 1);
        for (i = 0; i < (int)nf; i++) if (!memcmp(t + fo + 2 + 6 * i, "kern", 4)) found = 1;
        CHECK(found);
    } else CHECK(0);
    { tbl k = tget(s.h.face, TG("kern")); CHECK(!k.ex); trel(s.h.face, &k); }
    free(t); synth_close(&s);
}
TEST(tp_colr_cpal_v0) {
    static const uint32_t pal[] = { 0xFFFF0000u, 0xFF00FF00u, 0x800000FFu };
    static const ttf_colr_base base[] = { { 2, 0, 2 } };
    static const ttf_colr_layer lay[] = { { 28, 0 }, { 54, 1 } };
    ttf_spec sp; synth s; UINT32 n = 0, m = 0; uint8_t *c, *p;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.colr_base = base; sp.ncolr_base = 1; sp.colr_layers = lay; sp.ncolr_layers = 2; sp.palette = pal; sp.npalette = 3;
    if (!synth_open(&s, &sp)) { t_note("COLR font not created: %s", dw_hr_name(s.hr)); synth_close(&s); return; }
    c = fetch_copy(&s, "COLR", &n); p = fetch_copy(&s, "CPAL", &m);
    if (c && p) {
        EXPECT_EQ(rd(c, 0, 2), 0); EXPECT_EQ(rd(c, 2, 2), 1); EXPECT_EQ(rd(c, 12, 2), 2);
        EXPECT_EQ(rd(c, 14, 2), 2); EXPECT_EQ(rd(c, 16, 2), 0); EXPECT_EQ(rd(c, 18, 2), 2);
        EXPECT_EQ(rd(p, 0, 2), 0); EXPECT_EQ(rd(p, 2, 2), 3); EXPECT_EQ(rd(p, 4, 2), 1); EXPECT_EQ(rd(p, 6, 2), 3);
        {
            uint32_t off = BE32(p + 8);
            EXPECT_EQ(p[off + 0], 0x00); EXPECT_EQ(p[off + 2], 0xFF); EXPECT_EQ(p[off + 3], 0xFF);    /* BGRA of 0xFFFF0000: B=0 G=0 R=FF A=FF */
            EXPECT_EQ(p[off + 4 + 1], 0xFF);                                                          /* second colour green */
            EXPECT_EQ(p[off + 8 + 0], 0xFF); EXPECT_EQ(p[off + 8 + 3], 0x80);
        }
    } else CHECK(0);
    free(c); free(p); synth_close(&s);
}
TEST(tp_gasp_ranges) {
    static const ttf_gasp g[] = { { 8, 2 }, { 16, 1 }, { 0xFFFF, 3 } };
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *t; int i;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.gasp = g; sp.ngasp = 3;
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    t = fetch_copy(&s, "gasp", &n);
    if (t) {
        EXPECT_EQ(n, 4 + 4 * 3); EXPECT_EQ(rd(t, 0, 2), 1); EXPECT_EQ(rd(t, 2, 2), 3);
        for (i = 0; i < 3; i++) { EXPECT_EQ(rd(t, 4 + 4 * i, 2), g[i].max_ppem); EXPECT_EQ(rd(t, 6 + 4 * i, 2), g[i].behavior); }
    } else CHECK(0);
    free(t); synth_close(&s);
}
TEST(tp_vhea_vmtx_present_when_vertical) {
    ttf_spec sp; synth s; UINT32 n = 0, m = 0; uint8_t *vh, *vm;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.vertical = 1; sp.v_ascent = 1000; sp.v_descent = -1048;
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    vh = fetch_copy(&s, "vhea", &n); vm = fetch_copy(&s, "vmtx", &m);
    if (vh && vm) {
        EXPECT_EQ(rd(vh, 0, 4), 0x00010000); EXPECT_EQ(rd(vh, 4, -2), 1000); EXPECT_EQ(rd(vh, 6, -2), -1048); EXPECT_EQ(rd(vh, 34, 2), TTF_DEF_NGLYPHS);
        EXPECT_EQ(m, 4 * TTF_DEF_NGLYPHS);
        EXPECT_EQ(rd(vm, 0, 2), TTF_DEF_UPM);     /* default advance height = upm */
    } else CHECK(0);
    free(vh); free(vm); synth_close(&s);
}
TEST(tp_base_table_present) {
    ttf_spec sp; synth s; UINT32 n = 0; uint8_t *t;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.has_base = 1; sp.base_ideo = -120; sp.base_romn = 0;
    if (!synth_open(&s, &sp)) { t_skip("open"); synth_close(&s); return; }
    t = fetch_copy(&s, "BASE", &n);
    if (t) { EXPECT_EQ(rd(t, 0, 4), 0x00010000); CHECK(n > 8); } else CHECK(0);
    free(t); synth_close(&s);
}

/* ---- omitted tables ---- */
static void omit_check(unsigned omit, const char *tag) {
    ttf_spec sp; synth s;
    memset(&s, 0, sizeof s);
    ttf_default_spec(&sp); sp.omit = omit;
    if (!synth_open(&s, &sp)) { t_note("face without %s not created: %s", tag, dw_hr_name(s.hr)); synth_close(&s); return; }
    {
        tbl t = tget(s.h.face, TG(tag));
        CHECK_HR(t.hr, S_OK); CHECK(!t.ex); CHECK(t.d == NULL); EXPECT_EQ(t.n, 0); CHECK(t.ctx == NULL);
        trel(s.h.face, &t);
    }
    synth_close(&s);
}
TEST(tp_omitted_os2) { omit_check(TTF_OMIT_OS2, "OS/2"); }
TEST(tp_omitted_post) { omit_check(TTF_OMIT_POST, "post"); }
TEST(tp_omitted_name) { omit_check(TTF_OMIT_NAME, "name"); }
