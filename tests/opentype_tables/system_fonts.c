/* TryGetFontTable and derived facts on installed Windows fonts. Absent fonts skip. The oracle is the font file itself, read through the
 * font file's own loader/stream and parsed here; exact per-font numbers that depend on the installed font version are only noted. */
#include "ot.h"

typedef struct sysfont {
    IDWriteFontFace *face; uint8_t *file; size_t len; ot_dir dir; ot_font f; UINT32 index; int is_ttc; int has_glyf, has_cff;
} sysfont;
static int sys_open(sysfont *s, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFactory *f = dw_shared();
    memset(s, 0, sizeof *s);
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    s->face = dw_sysface(f, family, w, DWRITE_FONT_STRETCH_NORMAL, st);
    if (!s->face) { t_skip("font not installed"); return 0; }
    s->file = face_file_bytes(s->face, &s->len);
    if (!s->file) { t_skip("font file bytes not readable"); return 0; }
    s->index = IDWriteFontFace_GetIndex(s->face);
    s->is_ttc = ot_ttc_count(s->file, s->len) > 0;
    if (ot_parse_dir(s->file, s->len, s->index, &s->dir)) { t_skip("font directory not parseable"); return 0; }
    ot_font_init(&s->f, s->file, s->len, &s->dir);
    s->has_glyf = ot_find(&s->dir, TG("glyf")) != NULL; s->has_cff = ot_find(&s->dir, TG("CFF ")) != NULL || ot_find(&s->dir, TG("CFF2")) != NULL;
    return 1;
}
static void sys_close(sysfont *s) { free(s->file); DW_RELEASE(s->face); }
#define OPEN_SYS(s, fam, w, st) sysfont s; if (!sys_open(&s, fam, w, st)) { sys_close(&s); return; }
static const uint8_t *stab(const sysfont *s, const char *tag, uint32_t *len) {
    const ot_rec *r = ot_find(&s->dir, TG(tag));
    if (!r || (size_t)r->off + r->len > s->len) return NULL;
    if (len) *len = r->len;
    return s->file + r->off;
}

/* ---- generic checks ---- */
static void sf_tables_match_file(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    int n = 0, bad = compare_all_tables(s.face, s.file, s.len, &s.dir, &n);
    CHECKF(bad == 0, "%d of %d tables differ from the file bytes", bad, n);
    CHECK(n >= 6);
    sys_close(&s);
}
static void sf_structure(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    int i, outside = 0, unaligned = 0, unsorted = 0, badsum = 0, dup = 0;
    for (i = 0; i < s.dir.n; i++) {
        const ot_rec *r = &s.dir.r[i];
        if ((size_t)r->off + r->len > s.len) outside++;
        if (r->off & 3) unaligned++;
        if (i && BE32(s.file + s.dir.sfnt_off + 12 + 16 * i) <= BE32(s.file + s.dir.sfnt_off + 12 + 16 * (i - 1))) unsorted++;
        if (ot_find(&s.dir, r->tag) != r) dup++;
        if ((size_t)r->off + r->len <= s.len && r->tag != TG("head") && ot_checksum(s.file + r->off, r->len) != r->sum) badsum++;
    }
    CHECK(outside == 0);                 /* tables must lie inside the file for the face to be readable at all */
    CHECK(s.dir.version == 0x00010000 || s.dir.version == 0x74727565 || s.dir.version == 0x4F54544F);
    if (unaligned || unsorted || badsum || dup) t_note("directory: %d unaligned, %d unsorted/duplicate-order, %d checksum mismatches, %d duplicate tags", unaligned, unsorted, badsum, dup);
    sys_close(&s);
}
static void sf_required(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    static const char *const req[] = { "head", "hhea", "maxp", "hmtx", "cmap", "name", "post" };
    unsigned i;
    for (i = 0; i < sizeof req / sizeof req[0]; i++) {
        tbl t = tget(s.face, TG(req[i]));
        CHECKF(t.hr == S_OK && t.ex && t.n > 0 && t.d, "required table %s: hr %s ex %d n %u", req[i], dw_hr_name(t.hr), t.ex, t.n);
        trel(s.face, &t);
    }
    CHECKF(s.has_glyf ? ot_find(&s.dir, TG("loca")) != NULL : s.has_cff, "outline tables: glyf %d cff %d", s.has_glyf, s.has_cff);
    sys_close(&s);
}
static void sf_head_facts(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    uint32_t hl = 0, ml = 0; const uint8_t *h = stab(&s, "head", &hl), *m = stab(&s, "maxp", &ml);
    DWRITE_FONT_METRICS fm;
    CHECK(h && m);
    if (h && m) {
        EXPECT_EQ(BE32(h + 12), 0x5F0F3CF5u);                           /* head.magicNumber */
        CHECK(BE16(h + 18) >= 16 && BE16(h + 18) <= 16384);
        IDWriteFontFace_GetMetrics(s.face, &fm);
        EXPECT_EQ(fm.designUnitsPerEm, BE16(h + 18));
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(s.face), BE16(m + 4));       /* maxp.numGlyphs */
        CHECK(BE16S(h + 50) == 0 || BE16S(h + 50) == 1);
        CHECK(BE32(h) == 0x00010000);
    }
    sys_close(&s);
}
static void sf_metrics_from_tables(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    uint32_t hl, pl, ol, hh; const uint8_t *h = stab(&s, "head", &hl), *post = stab(&s, "post", &pl), *os2 = stab(&s, "OS/2", &ol), *hhea = stab(&s, "hhea", &hh);
    IDWriteFontFace1 *f1 = NULL; DWRITE_FONT_METRICS1 m;
    if (FAILED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1)) || !f1) { t_skip("no FontFace1"); sys_close(&s); return; }
    IDWriteFontFace1_GetMetrics(f1, &m);
    if (h) { EXPECT_EQ(m.glyphBoxLeft, BE16S(h + 36)); EXPECT_EQ(m.glyphBoxBottom, BE16S(h + 38)); EXPECT_EQ(m.glyphBoxRight, BE16S(h + 40)); EXPECT_EQ(m.glyphBoxTop, BE16S(h + 42)); }
    if (post && pl >= 12) { EXPECT_EQ(m.underlinePosition, BE16S(post + 8)); EXPECT_EQ(m.underlineThickness, BE16S(post + 10)); }
    if (os2 && ol >= 78) {
        EXPECT_EQ(m.strikethroughPosition, BE16S(os2 + 28));
        EXPECT_EQ(m.strikethroughThickness, BE16S(os2 + 26) > 0 ? BE16S(os2 + 26) : m.underlineThickness);
        EXPECT_EQ(m.subscriptSizeX, BE16S(os2 + 10)); EXPECT_EQ(m.subscriptSizeY, BE16S(os2 + 12));
        EXPECT_EQ(m.subscriptPositionX, BE16S(os2 + 14)); EXPECT_EQ(m.subscriptPositionY, -BE16S(os2 + 16));
        EXPECT_EQ(m.superscriptSizeX, BE16S(os2 + 18)); EXPECT_EQ(m.superscriptSizeY, BE16S(os2 + 20));
        EXPECT_EQ(m.superscriptPositionX, BE16S(os2 + 22)); EXPECT_EQ(m.superscriptPositionY, BE16S(os2 + 24));
        EXPECT_EQ(m.hasTypographicMetrics, (BE16(os2 + 62) & 0x80) != 0);
        if (BE16(os2) >= 2 && ol >= 90) {
            int xh = BE16S(os2 + 86), ch = BE16S(os2 + 88);
            if (xh > 0 && ch > 0 && ch < (int)m.designUnitsPerEm * 2) { if (m.xHeight != xh || m.capHeight != ch) t_note("OS/2 heights x %d cap %d vs reported x %u cap %u", xh, ch, m.xHeight, m.capHeight); }
        }
    }
    if (hhea) {
        int ha = BE16S(hhea + 4), hd = -BE16S(hhea + 6), hg = BE16S(hhea + 8);
        int wa = os2 && ol >= 78 ? BE16(os2 + 74) : 0, wd = os2 && ol >= 78 ? BE16(os2 + 76) : 0;
        int ta = os2 && ol >= 78 ? BE16S(os2 + 68) : 0, td = os2 && ol >= 78 ? -BE16S(os2 + 70) : 0, tg = os2 && ol >= 78 ? BE16S(os2 + 72) : 0;
        int typo = os2 && ol >= 78 && (BE16(os2 + 62) & 0x80);
        /* win metrics are used unless the font carries AAT morph tables (mort/morx) and no OpenType layout tables (GDEF/GPOS/GSUB) */
        int has_aat = ot_find(&s.dir, TG("morx")) != NULL || ot_find(&s.dir, TG("mort")) != NULL;
        int has_ot = ot_find(&s.dir, TG("GDEF")) != NULL || ot_find(&s.dir, TG("GPOS")) != NULL || ot_find(&s.dir, TG("GSUB")) != NULL;
        int ea, ed, eg;
        if (typo) { ea = ta; ed = td; eg = tg; }
        else if ((wa || wd) && (!has_aat || has_ot)) { int g = ha + hd + hg - wa - wd; ea = wa; ed = wd; eg = g > 0 ? g : 0; }
        else { ea = ha; ed = hd; eg = hg > 0 ? hg : 0; }
        if (m.ascent != ea || m.descent != ed || m.lineGap != eg) CHECKF(0, "ascent/descent/gap %u/%u/%d, rule says %d/%d/%d (hhea %d/%d/%d win %d/%d typo %d/%d/%d use_typo %d)", m.ascent, m.descent, m.lineGap, ea, ed, eg, ha, hd, hg, wa, wd, ta, td, tg, typo);
        else CHECK(1);
    }
    IDWriteFontFace1_Release(f1); sys_close(&s);
}
static void sf_glyph_indices_vs_cmap(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    uint32_t cl; const uint8_t *cmap = stab(&s, "cmap", &cl);
    UINT32 cps[400]; UINT16 g[400]; int n = 0, i, k, bad = 0;
    static const int pe[][2] = { {3, 10}, {0, 4}, {0, 6}, {3, 1}, {0, 3}, {0, 2}, {0, 1}, {0, 0} };
    if (IDWriteFontFace_IsSymbolFont(s.face)) { t_note("symbol font: Unicode cmap comparison skipped"); sys_close(&s); return; }
    CHECK(cmap != NULL);
    for (i = 0x20; i < 0x7F; i++) cps[n++] = i;                    /* ASCII */
    for (i = 0xA0; i < 0x100; i++) cps[n++] = i;                   /* Latin-1 */
    for (i = 0x391; i < 0x3AA; i++) cps[n++] = i;                  /* Greek capitals */
    for (i = 0x410; i < 0x430; i++) cps[n++] = i;                  /* Cyrillic */
    { static const UINT32 x[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC, 0x2122, 0x2190, 0x2191, 0x25A0, 0x4E00, 0x3042, 0xFFFD, 0xE000, 0xFB01, 0x1F600, 0x10FFFF, 0x20000, 0x0, 0x9, 0xD, 0xA0 };
      for (i = 0; i < (int)(sizeof x / sizeof x[0]) && n < 400; i++) cps[n++] = x[i]; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.face, cps, n, g));
    for (i = 0; i < n; i++) {
        int ok = 0, any = 0;
        for (k = 0; k < 8; k++) {
            long v;
            if (!ot_cmap_has(cmap, cl, pe[k][0], pe[k][1], NULL)) continue;
            v = ot_cmap_lookup(cmap, cl, pe[k][0], pe[k][1], cps[i]);
            if (v >= 0) { any = 1; if (v == g[i]) ok = 1; }
        }
        if (!any) { ok = 1; }                      /* no Unicode subtable: nothing to compare */
        if (!ok) { if (bad++ < 8) CHECKF(0, "U+%04X: GetGlyphIndices %u, cmap subtables disagree", cps[i], g[i]); }
    }
    CHECK(bad == 0);
    sys_close(&s);
}
static void sf_design_metrics(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    UINT16 ids[128]; DWRITE_GLYPH_METRICS gm[128]; int i, n = s.f.nglyphs < 128 ? s.f.nglyphs : 128, bad_adv = 0, bad_lsb = 0, bad_rsb = 0;
    IDWriteFontFace1 *f1 = NULL; INT32 adv[128];
    if (n < 2 || !s.f.hmtx) { t_skip("no glyph data"); sys_close(&s); return; }
    for (i = 0; i < n; i++) ids[i] = (UINT16)(i * (s.f.nglyphs / n));
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.face, ids, n, gm, FALSE));
    for (i = 0; i < n; i++) {
        ot_glyph g; ot_glyph_get(&s.f, ids[i], &g);
        if (gm[i].advanceWidth != (UINT32)g.adv) bad_adv++;
        if (gm[i].leftSideBearing != g.lsb) bad_lsb++;
        if (s.has_glyf && gm[i].rightSideBearing != g.adv - g.lsb - (g.has_outline ? g.x1 - g.x0 : 0)) bad_rsb++;
        CHECK((int)gm[i].advanceHeight == gm[i].topSideBearing + (g.has_outline ? g.y1 - g.y0 : 0) + gm[i].bottomSideBearing || !s.has_glyf);
    }
    CHECKF(bad_adv == 0, "%d advance mismatches", bad_adv);
    CHECKF(bad_lsb == 0, "%d lsb mismatches", bad_lsb);
    CHECKF(bad_rsb == 0, "%d rsb mismatches", bad_rsb);
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, n, ids, adv, FALSE));
        for (i = 0; i < n; i++) CHECKF(adv[i] == (int)gm[i].advanceWidth, "advances disagree at %d", i);
        IDWriteFontFace1_Release(f1);
    }
    sys_close(&s);
}
static void sf_unicode_ranges(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace1 *f1 = NULL; UINT32 n = 0, i; DWRITE_UNICODE_RANGE *r; uint32_t cl; const uint8_t *cmap = stab(&s, "cmap", &cl);
    int plat, enc; unsigned cp, bad = 0;
    if (FAILED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1)) || !f1) { t_skip("no FontFace1"); sys_close(&s); return; }
    HRESULT hr = IDWriteFontFace1_GetUnicodeRanges(f1, 0, NULL, &n);
    CHECK(hr == DW_E_NOT_SUFFICIENT_BUFFER || (hr == S_OK && n == 0));
    if (n > 0 && n < 100000 && cmap && ot_cmap_pick(cmap, cl, &plat, &enc) && !IDWriteFontFace_IsSymbolFont(s.face)) {
        r = (DWRITE_UNICODE_RANGE *)calloc(n, sizeof *r);
        CHECK_HR_OK(IDWriteFontFace1_GetUnicodeRanges(f1, n, r, &n));
        for (i = 0; i < n; i++) {
            CHECK(r[i].first <= r[i].last);
            if (i) CHECKF(r[i].first > r[i - 1].last, "ranges %u/%u overlap or are unsorted", i - 1, i);
            if (r[i].last >= 0xFFFF) continue;          /* the format 4 sentinel segment is not a mapped range */
            CHECKF(ot_cmap_lookup(cmap, cl, plat, enc, r[i].first) > 0 || ot_cmap_lookup(cmap, cl, 3, 1, r[i].first) > 0, "range %u first U+%04X not mapped", i, r[i].first);
            CHECKF(ot_cmap_lookup(cmap, cl, plat, enc, r[i].last) > 0 || ot_cmap_lookup(cmap, cl, 3, 1, r[i].last) > 0, "range %u last U+%04X not mapped", i, r[i].last);
        }
        /* every mapped BMP code point lies in a range and vice versa */
        for (cp = 0x20; cp < 0xFFFF; cp += 1) {
            int in = 0; UINT32 j;
            for (j = 0; j < n; j++) if (cp >= r[j].first && cp <= r[j].last) { in = 1; break; }
            if (in != (ot_cmap_lookup(cmap, cl, plat, enc, cp) > 0)) { if (bad++ < 5) t_note("U+%04X range membership %d disagrees with cmap (%d,%d)", cp, in, plat, enc); }
        }
        if (bad) CHECKF(0, "%u code points disagree between ranges and cmap", bad);
        free(r);
    }
    IDWriteFontFace1_Release(f1); sys_close(&s);
}
static void sf_panose(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace3 *f3 = NULL; uint32_t ol; const uint8_t *os2 = stab(&s, "OS/2", &ol); DWRITE_PANOSE p;
    if (FAILED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace3, (void **)&f3)) || !f3) { t_skip("no FontFace3"); sys_close(&s); return; }
    memset(&p, 0x77, sizeof p);
    IDWriteFontFace3_GetPanose(f3, &p);
    if (os2 && ol >= 42) CHECK(!memcmp(p.values, os2 + 32, 10));
    IDWriteFontFace3_Release(f3); sys_close(&s);
}
static void sf_kerning(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    uint32_t kl = 0; const uint8_t *kern = stab(&s, "kern", &kl);
    IDWriteFontFace1 *f1 = NULL;
    if (FAILED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1)) || !f1) { t_skip("no FontFace1"); sys_close(&s); return; }
    if (kern && kl >= 18 && BE16(kern) == 0 && BE16(kern + 2) >= 1) {
        unsigned np = 0, i, off = 4, t, nt = BE16(kern + 2); int tested = 0, bad = 0;
        const uint8_t *p = NULL;
        for (t = 0; t < nt && off + 14 <= kl; t++) { if (BE16(kern + off + 4) == 1) { np = BE16(kern + off + 6); p = kern + off + 14; break; } off += BE16(kern + off + 2); }
        if (p && off + 14 + 6u * np <= kl) {
            for (i = 0; i < np; i += (np > 200 ? np / 200 : 1)) {
                UINT16 g[2]; INT32 adj[2]; g[0] = BE16(p + 6 * i); g[1] = BE16(p + 6 * i + 2);
                if (g[0] >= s.f.nglyphs || g[1] >= s.f.nglyphs) continue;
                IDWriteFontFace1_GetKerningPairAdjustments(f1, 2, g, adj);
                if (adj[0] != BE16S(p + 6 * i + 4)) bad++;
                if (adj[1] != 0) bad++;
                tested++;
            }
            CHECKF(bad == 0, "%d kerning mismatches out of %d pairs", bad, tested);
            CHECK(IDWriteFontFace1_HasKerningPairs(f1));
            CHECK(tested > 0);
        } else t_note("kern table without a plain format-0 subtable");
    } else {
        UINT16 g[3] = { 1, 2, 3 }; INT32 adj[3];
        IDWriteFontFace1_GetKerningPairAdjustments(f1, 3, g, adj);
        t_note("no kern table: HasKerningPairs %d, adjustments %d %d %d", IDWriteFontFace1_HasKerningPairs(f1), adj[0], adj[1], adj[2]);
    }
    IDWriteFontFace1_Release(f1); sys_close(&s);
}
static const char *const probe_tags[] = { "DSIG", "GDEF", "GPOS", "GSUB", "kern", "cvt ", "fpgm", "prep", "gasp", "hdmx", "LTSH", "VDMX", "vhea", "vmtx", "COLR", "CPAL", "fvar", "avar", "STAT", "HVAR", "MVAR", "gvar", "CFF ", "CFF2", "BASE", "JSTF", "MATH", "meta", "SVG ", "sbix", "CBDT", "CBLC", "EBDT", "EBLC", "PCLT", "VORG", "trak", "morx" };
static void sf_presence_note(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    char list[600] = ""; unsigned i;
    for (i = 0; i < sizeof probe_tags / sizeof probe_tags[0]; i++) {
        tbl t = tget(s.face, TG(probe_tags[i]));
        int infile = ot_find(&s.dir, TG(probe_tags[i])) != NULL;
        CHECKF((t.ex != 0) == infile, "tag %s: TryGetFontTable exists=%d, directory says %d", probe_tags[i], t.ex, infile);
        if (t.ex) { strcat(list, probe_tags[i]); strcat(list, " "); }
        trel(s.face, &t);
    }
    t_note("optional tables present: %s", list);
    sys_close(&s);
}
static void sf_table_pointer_stable(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    tbl a = tget(s.face, TG("cmap")), b = tget(s.face, TG("cmap"));
    CHECK(a.ex && b.ex);
    EXPECT_EQ(a.n, b.n);
    if (a.d && b.d) CHECK(!memcmp(a.d, b.d, a.n));
    if (a.d != b.d) t_note("two fetches of cmap returned different pointers (%p, %p)", a.d, b.d); else t_note("two fetches of cmap returned the same pointer");
    trel(s.face, &a); trel(s.face, &b);
    sys_close(&s);
}
static void sf_face_released_table_balance(const WCHAR *fam) {
    OPEN_SYS(s, fam, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    int i;
    for (i = 0; i < 50; i++) { tbl t = tget(s.face, TG("hmtx")); CHECK(t.ex); trel(s.face, &t); }
    sys_close(&s);
}

#define SYSFONT(id, fam) \
    TEST(sf_##id##_tables_match_file) { sf_tables_match_file(fam); } \
    TEST(sf_##id##_structure) { sf_structure(fam); } \
    TEST(sf_##id##_required_tables) { sf_required(fam); } \
    TEST(sf_##id##_head_facts) { sf_head_facts(fam); } \
    TEST(sf_##id##_metrics_from_tables) { sf_metrics_from_tables(fam); } \
    TEST(sf_##id##_glyph_indices_vs_cmap) { sf_glyph_indices_vs_cmap(fam); } \
    TEST(sf_##id##_design_metrics) { sf_design_metrics(fam); } \
    TEST(sf_##id##_unicode_ranges) { sf_unicode_ranges(fam); } \
    TEST(sf_##id##_panose) { sf_panose(fam); } \
    TEST(sf_##id##_kerning) { sf_kerning(fam); } \
    TEST(sf_##id##_presence_note) { sf_presence_note(fam); } \
    TEST(sf_##id##_pointer_stable) { sf_table_pointer_stable(fam); } \
    TEST(sf_##id##_repeat_fetch) { sf_face_released_table_balance(fam); }

SYSFONT(arial, L"Arial")
SYSFONT(times, L"Times New Roman")
SYSFONT(courier, L"Courier New")
SYSFONT(segoeui, L"Segoe UI")
SYSFONT(tahoma, L"Tahoma")
SYSFONT(consolas, L"Consolas")
SYSFONT(marlett, L"Marlett")
SYSFONT(segoesym, L"Segoe UI Symbol")
SYSFONT(segoeemoji, L"Segoe UI Emoji")
SYSFONT(bahnschrift, L"Bahnschrift")
SYSFONT(msgothic, L"MS Gothic")
SYSFONT(cambria, L"Cambria")
SYSFONT(simsun, L"SimSun")

/* ---- font specific facts ---- */
static int has_tag(sysfont *s, const char *tag) { tbl t = tget(s->face, TG(tag)); int e = t.ex; trel(s->face, &t); return e; }
TEST(sf_arial_truetype_core_tables) {
    OPEN_SYS(s, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    static const char *const need[] = { "head", "hhea", "maxp", "hmtx", "cmap", "OS/2", "name", "post", "glyf", "loca", "cvt ", "fpgm", "prep", "gasp" };
    unsigned i; for (i = 0; i < sizeof need / sizeof need[0]; i++) CHECKF(has_tag(&s, need[i]), "Arial lacks %s", need[i]);
    CHECK(!has_tag(&s, "CFF "));
    sys_close(&s);
}
TEST(sf_arial_units_per_em_2048) {
    OPEN_SYS(s, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(s.face, &m);
    EXPECT_EQ(m.designUnitsPerEm, 2048);
    sys_close(&s);
}
TEST(sf_arial_not_monospaced) {
    OPEN_SYS(s, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace1 *f1 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) { CHECK(!IDWriteFontFace1_IsMonospacedFont(f1)); IDWriteFontFace1_Release(f1); }
    CHECK(!IDWriteFontFace_IsSymbolFont(s.face));
    sys_close(&s);
}
TEST(sf_arial_metrics_note) {
    OPEN_SYS(s, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    DWRITE_FONT_METRICS1 m; IDWriteFontFace1 *f1 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        IDWriteFontFace1_GetMetrics(f1, &m);
        t_note("Arial: upm %u asc %u desc %u gap %d cap %u x %u ul %d/%u st %d/%u", m.designUnitsPerEm, m.ascent, m.descent, m.lineGap, m.capHeight, m.xHeight, m.underlinePosition, m.underlineThickness, m.strikethroughPosition, m.strikethroughThickness);
        IDWriteFontFace1_Release(f1);
    }
    sys_close(&s);
}
TEST(sf_arial_AV_kerning_vs_kern_table) {
    OPEN_SYS(s, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    UINT32 cps[2] = { 'A', 'V' }; UINT16 g[2]; INT32 adj[2]; IDWriteFontFace1 *f1 = NULL; uint32_t kl; const uint8_t *k = stab(&s, "kern", &kl); int v = 0, found;
    IDWriteFontFace_GetGlyphIndices(s.face, cps, 2, g);
    if (!k) { t_skip("Arial has no kern table here"); sys_close(&s); return; }
    found = ot_kern_find(k, kl, g[0], g[1], &v);
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        IDWriteFontFace1_GetKerningPairAdjustments(f1, 2, g, adj);
        EXPECT_EQ(adj[0], found ? v : 0);
        t_note("Arial A,V (glyphs %u,%u): kern table %s %d, adjustment %d design units", g[0], g[1], found ? "value" : "no pair", v, adj[0]);
        CHECK(!found || adj[0] < 0);      /* kerning AV tightens the pair */
        IDWriteFontFace1_Release(f1);
    }
    sys_close(&s);
}
TEST(sf_arial_glyph_indices_200_codepoints) {
    OPEN_SYS(s, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    uint32_t cl; const uint8_t *cmap = stab(&s, "cmap", &cl); UINT32 cps[200]; UINT16 g[200]; int i, plat, enc, bad = 0, nz = 0;
    for (i = 0; i < 200; i++) cps[i] = (i < 95) ? 0x20 + i : (i < 160) ? 0xA0 + (i - 95) : 0x391 + (i - 160);
    CHECK(cmap && ot_cmap_pick(cmap, cl, &plat, &enc));
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.face, cps, 200, g));
    if (cmap) for (i = 0; i < 200; i++) { long v = ot_cmap_lookup(cmap, cl, plat, enc, cps[i]); if (v != g[i]) bad++; if (g[i]) nz++; }
    CHECKF(bad == 0, "%d of 200 Arial glyph indices differ from the (%d,%d) cmap", bad, plat, enc);
    CHECK(nz >= 150);        /* Arial covers Basic Latin, Latin-1 and Greek */
    sys_close(&s);
}
TEST(sf_arial_bold_is_a_different_face) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no factory"); return; }
    IDWriteFontFace *r = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace *b = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (r && b) {
        UINT32 nr, nb; uint8_t *tr = dw_face_table(r, TG("head"), &nr), *tb = dw_face_table(b, TG("head"), &nb);
        CHECK(tr && tb && nr == nb);
        if (tr && tb && nr == nb) { CHECK(BE16(tb + 44) & 1); CHECK(!(BE16(tr + 44) & 1)); }   /* head.macStyle bold bit */
        free(tr); free(tb);
    } else t_skip("Arial regular/bold unavailable");
    DW_RELEASE(r); DW_RELEASE(b);
}
TEST(sf_courier_new_is_monospaced) {
    OPEN_SYS(s, L"Courier New", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace1 *f1 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) { CHECK(IDWriteFontFace1_IsMonospacedFont(f1)); IDWriteFontFace1_Release(f1); }
    sys_close(&s);
}
TEST(sf_courier_new_fixed_advances) {
    OPEN_SYS(s, L"Courier New", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    UINT32 cps[26]; UINT16 g[26]; DWRITE_GLYPH_METRICS gm[26]; int i;
    for (i = 0; i < 26; i++) cps[i] = 'a' + i;
    IDWriteFontFace_GetGlyphIndices(s.face, cps, 26, g);
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(s.face, g, 26, gm, FALSE));
    for (i = 1; i < 26; i++) CHECK(gm[i].advanceWidth == gm[0].advanceWidth);
    sys_close(&s);
}
TEST(sf_consolas_is_monospaced) {
    OPEN_SYS(s, L"Consolas", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    UINT32 cps[2] = { 'i', 'W' }; UINT16 g[2]; DWRITE_GLYPH_METRICS gm[2];
    IDWriteFontFace_GetGlyphIndices(s.face, cps, 2, g);
    IDWriteFontFace_GetDesignGlyphMetrics(s.face, g, 2, gm, FALSE);
    EXPECT_EQ(gm[0].advanceWidth, gm[1].advanceWidth);
    sys_close(&s);
}
TEST(sf_marlett_is_symbol_font) {
    OPEN_SYS(s, L"Marlett", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    uint32_t cl; const uint8_t *cmap = stab(&s, "cmap", &cl);
    CHECK(IDWriteFontFace_IsSymbolFont(s.face));
    CHECK(cmap && ot_cmap_has(cmap, cl, 3, 0, NULL));
    sys_close(&s);
}
TEST(sf_marlett_glyph_for_symbol_codepoints) {
    OPEN_SYS(s, L"Marlett", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    uint32_t cl; const uint8_t *cmap = stab(&s, "cmap", &cl); UINT32 cps[] = { 0x30, 0x31, 0x72, 0xF030, 0xF072 }; UINT16 g[5]; int i;
    if (!cmap) { CHECK(0); sys_close(&s); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(s.face, cps, 5, g));
    for (i = 0; i < 5; i++) t_note("Marlett U+%04X -> glyph %u (symbol cmap: %ld, +F000: %ld)", cps[i], g[i], ot_cmap_lookup(cmap, cl, 3, 0, cps[i]), ot_cmap_lookup(cmap, cl, 3, 0, cps[i] | 0xF000));
    /* The (3,0) subtable is keyed by 0xF0xx; the same glyph must be reachable through the 0xF0xx code point. */
    CHECK(g[3] == ot_cmap_lookup(cmap, cl, 3, 0, 0xF030));
    sys_close(&s);
}
TEST(sf_segoe_emoji_color_tables) {
    OPEN_SYS(s, L"Segoe UI Emoji", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    tbl c = tget(s.face, TG("COLR")), p = tget(s.face, TG("CPAL"));
    CHECK(c.ex && p.ex);
    if (c.ex && c.n >= 14) {
        const uint8_t *d = (const uint8_t *)c.d;
        uint32_t nb = BE16(d + 2), bo = BE32(d + 4), lo = BE32(d + 8), nl = BE16(d + 12);
        CHECK(BE16(d) <= 1);
        CHECK(nb > 0 && bo + 6 * nb <= c.n && lo + 4 * nl <= c.n);
        t_note("COLR v%u: %u base glyph records, %u layer records", BE16(d), nb, nl);
    }
    if (p.ex && p.n >= 12) {
        const uint8_t *d = (const uint8_t *)p.d;
        CHECK(BE16(d + 2) > 0 && BE16(d + 4) > 0);
        t_note("CPAL v%u: %u entries, %u palettes", BE16(d), BE16(d + 2), BE16(d + 4));
    }
    trel(s.face, &c); trel(s.face, &p);
    sys_close(&s);
}
TEST(sf_segoe_emoji_is_color_font) {
    OPEN_SYS(s, L"Segoe UI Emoji", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace2 *f2 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace2, (void **)&f2)) && f2) { CHECK(IDWriteFontFace2_IsColorFont(f2)); IDWriteFontFace2_Release(f2); }
    else t_skip("no FontFace2");
    sys_close(&s);
}
TEST(sf_arial_is_not_color_font) {
    OPEN_SYS(s, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace2 *f2 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace2, (void **)&f2)) && f2) { CHECK(!IDWriteFontFace2_IsColorFont(f2)); IDWriteFontFace2_Release(f2); }
    sys_close(&s);
}
TEST(sf_bahnschrift_variable_tables) {
    OPEN_SYS(s, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    static const char *const var[] = { "fvar", "avar", "STAT", "HVAR", "MVAR", "gvar" };
    unsigned i; for (i = 0; i < sizeof var / sizeof var[0]; i++) CHECKF(has_tag(&s, var[i]), "Bahnschrift lacks %s", var[i]);
    sys_close(&s);
}
TEST(sf_bahnschrift_fvar_layout) {
    OPEN_SYS(s, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    tbl t = tget(s.face, TG("fvar"));
    if (t.ex && t.n >= 16) {
        const uint8_t *d = (const uint8_t *)t.d; unsigned axoff = BE16(d + 4), na = BE16(d + 8), asz = BE16(d + 10), ni = BE16(d + 12), isz = BE16(d + 14), i;
        EXPECT_EQ(BE32(d), 0x00010000u); EXPECT_EQ(asz, 20); CHECK(na >= 1 && na <= 64);
        CHECK(isz == 4 + 4 * na || isz == 6 + 4 * na);
        CHECK(axoff + na * asz + ni * isz <= t.n);
        for (i = 0; i < na && axoff + (i + 1) * asz <= t.n; i++) {
            const uint8_t *a = d + axoff + i * asz; int32_t mn = (int32_t)BE32(a + 4), df = (int32_t)BE32(a + 8), mx = (int32_t)BE32(a + 12);
            CHECK(mn <= df && df <= mx);
            t_note("axis %u: '%c%c%c%c' %g..%g default %g", i, a[0], a[1], a[2], a[3], mn / 65536.0, mx / 65536.0, df / 65536.0);
        }
        { IDWriteFontFace5 *f5 = NULL; if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace5, (void **)&f5)) && f5) { UINT32 c = IDWriteFontFace5_GetFontAxisValueCount(f5); t_note("IDWriteFontFace5 axis value count %u (fvar axes %u)", c, na); CHECK(c == na); IDWriteFontFace5_Release(f5); } }
    } else CHECK(0);
    trel(s.face, &t);
    sys_close(&s);
}
TEST(sf_bahnschrift_avar_stat_hvar_headers) {
    OPEN_SYS(s, L"Bahnschrift", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    tbl a = tget(s.face, TG("avar")), st = tget(s.face, TG("STAT")), h = tget(s.face, TG("HVAR"));
    if (a.ex && a.n >= 8) CHECK(BE32(a.d) == 0x00010000);
    if (st.ex && st.n >= 18) { CHECK(BE16(st.d) == 1); CHECK(BE16((const uint8_t *)st.d + 2) <= 2); }
    if (h.ex && h.n >= 20) { CHECK(BE16(h.d) == 1); CHECK(BE16((const uint8_t *)h.d + 2) == 0); }
    CHECK(a.ex && st.ex && h.ex);
    trel(s.face, &a); trel(s.face, &st); trel(s.face, &h);
    sys_close(&s);
}
TEST(sf_segoeui_gpos_gsub_gdef_note) {
    OPEN_SYS(s, L"Segoe UI", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    tbl a = tget(s.face, TG("GPOS")), b = tget(s.face, TG("GSUB")), c = tget(s.face, TG("GDEF"));
    t_note("Segoe UI: GPOS %d GSUB %d GDEF %d", a.ex, b.ex, c.ex);
    if (a.ex && a.n >= 10) { CHECK(BE32(a.d) == 0x00010000 || BE32(a.d) == 0x00010001); CHECK(BE16((const uint8_t *)a.d + 4) < a.n); }
    if (b.ex && b.n >= 10) { CHECK(BE32(b.d) == 0x00010000 || BE32(b.d) == 0x00010001); CHECK(BE16((const uint8_t *)b.d + 6) < b.n); }
    if (c.ex && c.n >= 12) { CHECK(BE32(c.d) == 0x00010000 || BE32(c.d) == 0x00010002 || BE32(c.d) == 0x00010003); }
    trel(s.face, &a); trel(s.face, &b); trel(s.face, &c);
    sys_close(&s);
}
TEST(sf_times_new_roman_units_per_em_2048) {
    OPEN_SYS(s, L"Times New Roman", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL);
    DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(s.face, &m); EXPECT_EQ(m.designUnitsPerEm, 2048);
    sys_close(&s);
}
TEST(sf_tables_same_across_two_face_objects) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no factory"); return; }
    IDWriteFontFace *a = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL), *b = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!a || !b) { t_skip("Arial unavailable"); DW_RELEASE(a); DW_RELEASE(b); return; }
    {
        tbl x = tget(a, TG("OS/2")), y = tget(b, TG("OS/2"));
        CHECK(x.ex && y.ex); EXPECT_EQ(x.n, y.n);
        if (x.d && y.d && x.n == y.n) CHECK(!memcmp(x.d, y.d, x.n));
        trel(a, &x); trel(b, &y);
    }
    t_note("two faces for Arial are %s objects", a == b ? "the same" : "distinct");
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(sf_table_survives_collection_release) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL; UINT32 idx; BOOL ex;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) { t_skip("no collection"); return; }
    IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex);
    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font)) && SUCCEEDED(IDWriteFont_CreateFontFace(font, &face))) {
        tbl t;
        DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c);       /* the face keeps working without the objects that created it */
        t = tget(face, TG("head")); CHECK(t.ex && t.n == 54); trel(face, &t);
    } else t_skip("Arial unavailable");
    DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c); DW_RELEASE(face);
}
