/* IDWriteFontFace::GetGlyphIndices over synthetic fonts with specific cmap subtables (format 4 / 12 / 0 / 6 / 13 / 14) and symbol
 * encodings.  Observed (IDWriteFontFace::GetGlyphIndicesW): the output array is zeroed first; codePoints == NULL with count != 0 returns
 * E_INVALIDARG (outputs stay zero); count 0 is S_OK; code points >= 0x110000 map to glyph 0; the code point is looked up in a
 * two-level (block of 64) table built from the font's Unicode cmap. */
#include "r2.h"

static const ttf_map m_bmp[] = { { 0x41, 10 }, { 0x42, 11 }, { 0xE9, 12 }, { 0xFFFD, 13 }, { 0x4E00, 14 }, { 0x20, 1 }, { 0x3042, 20 } };
static const ttf_map m_all[] = { { 0x41, 10 }, { 0x42, 11 }, { 0xE9, 12 }, { 0xFFFD, 13 }, { 0x4E00, 14 }, { 0x1F600, 15 }, { 0x10400, 16 }, { 0x10FFFF, 17 }, { 0x20, 1 }, { 0x3042, 20 } };
#define NM(a) ((int)(sizeof(a) / sizeof((a)[0])))

static uint8_t *font_with(const ttf_cmap *cm, int ncm, size_t *n)
{
    ttf_spec s; ttf_default_spec(&s); s.cmaps = cm; s.ncmaps = ncm; return ttf_build(&s, n);
}
typedef struct { fx x; uint8_t *blob; } cf;
static int cf_open(cf *c, const ttf_cmap *cm, int ncm)
{
    size_t n = 0; memset(c, 0, sizeof *c);
    c->blob = font_with(cm, ncm, &n);
    return c->blob && fx_open_blob(&c->x, c->blob, n);
}
static void cf_close(cf *c) { fx_close(&c->x); free(c->blob); }
#define CF_BEGIN(name, plat, enc, fmt, map) \
    ttf_cmap cm_[1] = { { fmt, plat, enc, NM(map), map, 0, NULL } }; cf name; \
    if (!cf_open(&name, cm_, 1)) { cf_close(&name); t_skip("cmap fixture unavailable"); return; }
static HRESULT gi(cf *c, const UINT32 *cp, UINT32 n, UINT16 *out) { return IDWriteFontFace_GetGlyphIndices(c->x.face, cp, n, out); }
#define GI_EXPECT(c, cps, want) do { UINT16 o_[NM(cps)]; UINT32 i_; memset(o_, 0xAA, sizeof o_); \
    CHECK_HR_OK(gi(c, cps, NM(cps), o_)); \
    for (i_ = 0; i_ < (UINT32)NM(cps); i_++) CHECKF(o_[i_] == (want)[i_], "U+%04X -> %u want %u", cps[i_], o_[i_], (want)[i_]); } while (0)

TEST(r2_shaping_features_cmap4_basic)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp);
    static const UINT32 cp[] = { 0x41, 0x42, 0xE9, 0xFFFD, 0x4E00, 0x43, 0x20, 0x3042 }; static const UINT16 w[] = { 10, 11, 12, 13, 14, 0, 1, 20 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_cmap4_unmapped_is_zero)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp);
    static const UINT32 cp[] = { 0, 1, 0x40, 0x43, 0x7F, 0xE8, 0xEA, 0x4DFF, 0x4E01 }; static const UINT16 w[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_cmap4_supplementary_not_mapped)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp);
    static const UINT32 cp[] = { 0x1F600, 0x10400, 0x10041, 0x10FFFF }; static const UINT16 w[] = { 0, 0, 0, 0 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_cmap4_surrogates_not_mapped)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp);
    static const UINT32 cp[] = { 0xD800, 0xDBFF, 0xDC00, 0xDFFF }; static const UINT16 w[] = { 0, 0, 0, 0 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_cmap12_basic)
{
    CF_BEGIN(c, 3, 10, 12, m_all);
    static const UINT32 cp[] = { 0x41, 0x1F600, 0x10400, 0x10FFFF, 0x4E00, 0x1F601, 0x10401 }; static const UINT16 w[] = { 10, 15, 16, 17, 14, 0, 0 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_cmap12_beyond_unicode_range)
{   /* observed: block index (cp >> 6) must be below 0x4400, i.e. cp < 0x110000 */
    CF_BEGIN(c, 3, 10, 12, m_all);
    static const UINT32 cp[] = { 0x110000, 0x110041, 0x7FFFFFFF, 0x80000041, 0xFFFFFFFF }; static const UINT16 w[] = { 0, 0, 0, 0, 0 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_cmap12_on_unicode_platform_0_4)
{
    CF_BEGIN(c, 0, 4, 12, m_all);
    static const UINT32 cp[] = { 0x41, 0x1F600, 0x10FFFF }; static const UINT16 w[] = { 10, 15, 17 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_cmap4_on_unicode_platform_0_3)
{
    CF_BEGIN(c, 0, 3, 4, m_bmp);
    static const UINT32 cp[] = { 0x41, 0xE9, 0x4E00 }; static const UINT16 w[] = { 10, 12, 14 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_cmap4_on_unicode_platform_0_0_note)
{
    CF_BEGIN(c, 0, 0, 4, m_bmp);
    UINT32 cp = 0x41; UINT16 o = 0xAAAA; CHECK_HR_OK(gi(&c, &cp, 1, &o)); t_note("platform 0 encoding 0 subtable: U+0041 -> %u", o); cf_close(&c);
}
TEST(r2_shaping_features_cmap4_on_unicode_platform_0_1_note)
{
    CF_BEGIN(c, 0, 1, 4, m_bmp);
    UINT32 cp = 0x41; UINT16 o = 0xAAAA; CHECK_HR_OK(gi(&c, &cp, 1, &o)); t_note("platform 0 encoding 1 subtable: U+0041 -> %u", o); cf_close(&c);
}
TEST(r2_shaping_features_cmap4_on_unicode_platform_0_2_note)
{
    CF_BEGIN(c, 0, 2, 4, m_bmp);
    UINT32 cp = 0x41; UINT16 o = 0xAAAA; CHECK_HR_OK(gi(&c, &cp, 1, &o)); t_note("platform 0 encoding 2 subtable: U+0041 -> %u", o); cf_close(&c);
}
TEST(r2_shaping_features_cmap4_and_12_prefers_ucs4_note)
{
    static const ttf_map a[] = { { 0x41, 10 }, { 0x42, 11 } }, b[] = { { 0x41, 30 }, { 0x42, 31 }, { 0x1F600, 32 } };
    ttf_cmap cm[2] = { { 4, 3, 1, 2, a, 0, NULL }, { 12, 3, 10, 3, b, 0, NULL } }; cf c; UINT32 cp[] = { 0x41, 0x42, 0x1F600 }; UINT16 o[3];
    if (!cf_open(&c, cm, 2)) { cf_close(&c); t_skip("cmap fixture unavailable"); return; }
    CHECK_HR_OK(gi(&c, cp, 3, o)); t_note("(3,1)->10/11 vs (3,10)->30/31/32: got %u %u %u", o[0], o[1], o[2]);
    CHECK(o[0] == 10 || o[0] == 30); CHECK(o[2] == 32); cf_close(&c);
}
TEST(r2_shaping_features_cmap_unicode_and_mac_same_font)
{
    static const ttf_map a[] = { { 0x41, 10 } }, mac[] = { { 0x41, 40 } };
    ttf_cmap cm[2] = { { 0, 1, 0, 1, mac, 0, NULL }, { 4, 3, 1, 1, a, 0, NULL } }; cf c; UINT32 cp = 0x41; UINT16 o;
    if (!cf_open(&c, cm, 2)) { cf_close(&c); t_skip("cmap fixture unavailable"); return; }
    CHECK_HR_OK(gi(&c, &cp, 1, &o)); EXPECT_EQ(o, 10); cf_close(&c);
}
TEST(r2_shaping_features_cmap_mac_only_note)
{
    static const ttf_map mac[] = { { 0x41, 40 }, { 0x42, 41 } };
    ttf_cmap cm[1] = { { 0, 1, 0, 2, mac, 0, NULL } }; cf c; UINT32 cp[2] = { 0x41, 0x42 }; UINT16 o[2];
    if (!cf_open(&c, cm, 1)) { cf_close(&c); t_skip("cmap fixture unavailable"); return; }
    CHECK_HR_OK(gi(&c, cp, 2, o)); t_note("Mac Roman-only cmap (1,0) format 0: A -> %u B -> %u", o[0], o[1]); cf_close(&c);
}
TEST(r2_shaping_features_cmap_format6_note)
{
    static const ttf_map m[] = { { 0x41, 10 }, { 0x42, 11 }, { 0x43, 12 } };
    ttf_cmap cm[1] = { { 6, 3, 1, 3, m, 0, NULL } }; cf c; UINT32 cp[3] = { 0x41, 0x42, 0x43 }; UINT16 o[3];
    if (!cf_open(&c, cm, 1)) { cf_close(&c); t_skip("cmap fixture unavailable"); return; }
    CHECK_HR_OK(gi(&c, cp, 3, o)); t_note("(3,1) format 6: %u %u %u", o[0], o[1], o[2]); cf_close(&c);
}
TEST(r2_shaping_features_cmap_format13_note)
{
    static const ttf_map m[] = { { 0x41, 10 }, { 0x1F600, 15 } };
    ttf_cmap cm[1] = { { 13, 3, 10, 2, m, 0, NULL } }; cf c; UINT32 cp[2] = { 0x41, 0x1F600 }; UINT16 o[2];
    if (!cf_open(&c, cm, 1)) { cf_close(&c); t_skip("cmap fixture unavailable"); return; }
    CHECK_HR_OK(gi(&c, cp, 2, o)); t_note("(3,10) format 13: %u %u", o[0], o[1]); cf_close(&c);
}
TEST(r2_shaping_features_cmap_symbol_encoding_note)
{
    static const ttf_map m[] = { { 0xF041, 10 }, { 0xF042, 11 }, { 0xF020, 1 } };
    ttf_cmap cm[1] = { { 4, 3, 0, 3, m, 0, NULL } }; cf c; UINT32 cp[4] = { 0x41, 0x42, 0xF041, 0x20 }; UINT16 o[4];
    if (!cf_open(&c, cm, 1)) { cf_close(&c); t_skip("cmap fixture unavailable"); return; }
    CHECK_HR_OK(gi(&c, cp, 4, o));
    t_note("symbol (3,0): U+0041 -> %u U+0042 -> %u U+F041 -> %u U+0020 -> %u; IsSymbolFont=%d", o[0], o[1], o[2], o[3], IDWriteFontFace_IsSymbolFont(c.x.face));
    EXPECT_EQ(IDWriteFontFace_IsSymbolFont(c.x.face), TRUE); CHECK(o[2] == 10); cf_close(&c);
}
TEST(r2_shaping_features_cmap_nonsymbol_is_not_symbol)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); EXPECT_EQ(IDWriteFontFace_IsSymbolFont(c.x.face), FALSE); cf_close(&c);
}

/* ---- protocol: argument validation (observed) ---- */
TEST(r2_shaping_features_gi_count_zero_ok)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); UINT16 o = 0xAAAA; UINT32 cp = 0x41;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(c.x.face, &cp, 0, &o)); EXPECT_EQ(o, 0xAAAA);
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(c.x.face, NULL, 0, &o)); EXPECT_EQ(o, 0xAAAA); cf_close(&c);
}
TEST(r2_shaping_features_gi_null_codepoints_invalidarg_outputs_zeroed)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); UINT16 o[4]; memset(o, 0xAA, sizeof o);
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(c.x.face, NULL, 4, o), E_INVALIDARG);
    CHECKF(o[0] == 0 && o[1] == 0 && o[2] == 0 && o[3] == 0, "outputs not zeroed: %u %u %u %u", o[0], o[1], o[2], o[3]); cf_close(&c);
}
TEST(r2_shaping_features_gi_output_beyond_count_untouched)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); UINT16 o[4]; UINT32 cp[2] = { 0x41, 0x42 }; memset(o, 0xAA, sizeof o);
    CHECK_HR_OK(gi(&c, cp, 2, o)); EXPECT_EQ(o[0], 10); EXPECT_EQ(o[1], 11); EXPECT_EQ(o[2], 0xAAAA); EXPECT_EQ(o[3], 0xAAAA); cf_close(&c);
}
TEST(r2_shaping_features_gi_same_codepoint_repeated)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); UINT32 cp[8]; UINT16 o[8]; int i; for (i = 0; i < 8; i++) cp[i] = 0x41; CHECK_HR_OK(gi(&c, cp, 8, o));
    for (i = 0; i < 8; i++) EXPECT_EQ(o[i], 10); cf_close(&c);
}
TEST(r2_shaping_features_gi_alternating_blocks)
{   /* the lookup caches the last block: alternating blocks must not return stale glyphs */
    CF_BEGIN(c, 3, 1, 4, m_bmp); UINT32 cp[12]; UINT16 o[12]; int i;
    for (i = 0; i < 12; i += 3) { cp[i] = 0x41; cp[i + 1] = 0x4E00; cp[i + 2] = 0x3042; }
    CHECK_HR_OK(gi(&c, cp, 12, o));
    for (i = 0; i < 12; i += 3) { EXPECT_EQ(o[i], 10); EXPECT_EQ(o[i + 1], 14); EXPECT_EQ(o[i + 2], 20); } cf_close(&c);
}
TEST(r2_shaping_features_gi_unsorted_input_order_preserved)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); static const UINT32 cp[] = { 0x4E00, 0x41, 0x3042, 0x20, 0x42, 0xE9 }; static const UINT16 w[] = { 14, 10, 20, 1, 11, 12 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_gi_large_count)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); enum { N = 20000 }; UINT32 *cp = (UINT32 *)malloc(N * 4); UINT16 *o = (UINT16 *)malloc(N * 2); int i, bad = 0;
    for (i = 0; i < N; i++) cp[i] = (i & 1) ? 0x41 : 0x4E00;
    CHECK_HR_OK(gi(&c, cp, N, o)); for (i = 0; i < N; i++) if (o[i] != ((i & 1) ? 10 : 14)) bad++;
    CHECKF(bad == 0, "%d wrong entries", bad); free(cp); free(o); cf_close(&c);
}
TEST(r2_shaping_features_gi_every_bmp_codepoint)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); UINT32 cp; UINT16 o; int bad = 0;
    for (cp = 0; cp < 0x10000; cp += 1) {
        UINT16 want = 0; int k; for (k = 0; k < NM(m_bmp); k++) if (m_bmp[k].cp == cp) want = (UINT16)m_bmp[k].gid;
        if (FAILED(gi(&c, &cp, 1, &o)) || o != want) { if (bad < 5) t_note("U+%04X -> %u want %u", cp, o, want); bad++; }
    }
    CHECKF(bad == 0, "%d code points differ", bad); cf_close(&c);
}
TEST(r2_shaping_features_gi_range_segments)
{   /* contiguous runs become a single format 4 segment with idDelta; each member must keep its own glyph */
    static ttf_map m[40]; int i; for (i = 0; i < 40; i++) { m[i].cp = 0x100 + i; m[i].gid = 5 + i; }
    ttf_cmap cm[1] = { { 4, 3, 1, 40, m, 0, NULL } }; cf c; UINT32 cp[44]; UINT16 o[44];
    if (!cf_open(&c, cm, 1)) { cf_close(&c); t_skip("cmap fixture unavailable"); return; }
    for (i = 0; i < 44; i++) cp[i] = 0x0FE + i;
    CHECK_HR_OK(gi(&c, cp, 44, o));
    for (i = 0; i < 44; i++) { UINT16 w = (cp[i] >= 0x100 && cp[i] < 0x128) ? (UINT16)(5 + cp[i] - 0x100) : 0; CHECKF(o[i] == w, "U+%04X -> %u want %u", cp[i], o[i], w); }
    cf_close(&c);
}
TEST(r2_shaping_features_gi_format12_group_boundaries)
{
    static const ttf_map m[] = { { 0x20000, 10 }, { 0x20001, 11 }, { 0x2FFFF, 12 }, { 0x10000, 13 }, { 0xFFFF, 14 } };
    ttf_cmap cm[1] = { { 12, 3, 10, 5, m, 0, NULL } }; cf c; static const UINT32 cp[] = { 0x1FFFF, 0x20000, 0x20001, 0x20002, 0x2FFFE, 0x2FFFF, 0x30000, 0x10000, 0xFFFF, 0xFFFE };
    static const UINT16 w[] = { 0, 10, 11, 0, 0, 12, 0, 13, 14, 0 };
    if (!cf_open(&c, cm, 1)) { cf_close(&c); t_skip("cmap fixture unavailable"); return; }
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_gi_matches_default_font_cmap)
{
    FX_BEGIN(x, FX_PLAIN); UINT32 cp[128]; UINT16 o[128]; int i;
    for (i = 0; i < 128; i++) cp[i] = (UINT32)i;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.face, cp, 128, o));
    for (i = 0; i < 128; i++) CHECKF(o[i] == ttf_default_gid((unsigned)i), "U+%04X -> %u want %u", i, o[i], ttf_default_gid((unsigned)i));
    fx_close(&x);
}
TEST(r2_shaping_features_gi_multiple_faces_independent)
{   /* two faces of different fonts do not share the lookup cache */
    CF_BEGIN(a, 3, 1, 4, m_bmp);
    static const ttf_map m2[] = { { 0x41, 30 } }; ttf_cmap cm2[1] = { { 4, 3, 1, 1, m2, 0, NULL } }; cf b; UINT32 cp = 0x41; UINT16 oa, ob;
    if (!cf_open(&b, cm2, 1)) { cf_close(&b); cf_close(&a); t_skip("cmap fixture unavailable"); return; }
    CHECK_HR_OK(gi(&a, &cp, 1, &oa)); CHECK_HR_OK(gi(&b, &cp, 1, &ob)); CHECK_HR_OK(gi(&a, &cp, 1, &oa)); EXPECT_EQ(oa, 10); EXPECT_EQ(ob, 30);
    cf_close(&b); cf_close(&a);
}

/* ---- variation selectors (format 14) through GetGlyphs ---- */
static const ttf_uvs uvs[] = { { 0x4E00, 0xE0100, 0, 16 }, { 0x4E02, 0xFE01, 0, 17 }, { 0x4E01, 0xFE00, 1, 0 }, { 0x4E00, 0xFE00, 0, 18 } };
static const ttf_map m_uvs[] = { { 0x4E00, 14 }, { 0x4E01, 15 }, { 0x4E02, 19 }, { 0x20, 1 } };
static int open_uvs(cf *c)
{
    ttf_cmap cm[2] = { { 4, 3, 1, NM(m_uvs), m_uvs, 0, NULL }, { 14, 0, 5, 0, NULL, NM(uvs), uvs } };
    return cf_open(c, cm, 2);
}
TEST(r2_shaping_features_uvs_selector_codepoints_not_in_gi)
{
    cf c; if (!open_uvs(&c)) { cf_close(&c); t_skip("uvs fixture unavailable"); return; }
    static const UINT32 cp[] = { 0xFE00, 0xFE01, 0xE0100, 0xE01EF, 0x4E00 }; static const UINT16 w[] = { 0, 0, 0, 0, 14 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_uvs_base_codepoints_map_normally)
{
    cf c; if (!open_uvs(&c)) { cf_close(&c); t_skip("uvs fixture unavailable"); return; }
    static const UINT32 cp[] = { 0x4E00, 0x4E01, 0x4E02 }; static const UINT16 w[] = { 14, 15, 19 };
    GI_EXPECT(&c, cp, w); cf_close(&c);
}
TEST(r2_shaping_features_uvs_getglyphs_nondefault_note)
{
    cf c; shaped s; if (!open_uvs(&c)) { cf_close(&c); t_skip("uvs fixture unavailable"); return; }
    CHECK_HR_OK(sh_run(&c.x, &s, L"\x4e00\xdb40\xdd00", FALSE, NULL, 0));
    t_note("U+4E00 U+E0100 (non-default UVS -> gid 16): %s cmap %u %u %u", sh_dump(&s), s.cmap[0], s.cmap[1], s.cmap[2]);
    CHECK(s.actual >= 1); cf_close(&c);
}
TEST(r2_shaping_features_uvs_getglyphs_bmp_selector_note)
{
    cf c; shaped s; if (!open_uvs(&c)) { cf_close(&c); t_skip("uvs fixture unavailable"); return; }
    CHECK_HR_OK(sh_run(&c.x, &s, L"\x4e02\xfe01", FALSE, NULL, 0));
    t_note("U+4E02 U+FE01 (non-default UVS -> gid 17): %s cmap %u %u", sh_dump(&s), s.cmap[0], s.cmap[1]); CHECK(s.actual >= 1); cf_close(&c);
}
TEST(r2_shaping_features_uvs_getglyphs_default_selector_note)
{
    cf c; shaped s; if (!open_uvs(&c)) { cf_close(&c); t_skip("uvs fixture unavailable"); return; }
    CHECK_HR_OK(sh_run(&c.x, &s, L"\x4e01\xfe00", FALSE, NULL, 0));
    t_note("U+4E01 U+FE00 (default UVS): %s cmap %u %u", sh_dump(&s), s.cmap[0], s.cmap[1]); CHECK(s.actual >= 1); CHECK(s.gid[0] == 15); cf_close(&c);
}
TEST(r2_shaping_features_uvs_getglyphs_selector_without_entry_note)
{
    cf c; shaped s; if (!open_uvs(&c)) { cf_close(&c); t_skip("uvs fixture unavailable"); return; }
    CHECK_HR_OK(sh_run(&c.x, &s, L"\x4e02\xfe05", FALSE, NULL, 0));
    t_note("U+4E02 U+FE05 (no entry): %s cmap %u %u", sh_dump(&s), s.cmap[0], s.cmap[1]); CHECK(s.actual >= 1); CHECK(s.gid[0] == 19); cf_close(&c);
}

/* ---- GetGlyphs consumes the same cmap ---- */
TEST(r2_shaping_features_getglyphs_uses_cmap4_gids)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); shaped s; CHECK_HR_OK(sh_run(&c.x, &s, L"AB C", FALSE, NULL, 0));
    CHECK_GIDS(&s, 10, 11, 1, 0); CHECK_ADV(&s, 1000, 1000, 1000, 1200); cf_close(&c);
}
TEST(r2_shaping_features_getglyphs_surrogate_pair_cluster)
{
    CF_BEGIN(c, 3, 10, 12, m_all); shaped s; CHECK_HR_OK(sh_run(&c.x, &s, L"A\xd83d\xde00" L"B", FALSE, NULL, 0));
    CHECK_GIDS(&s, 10, 15, 11); CHECK_CMAP(&s, 0, 1, 1, 2); cf_close(&c);
}
TEST(r2_shaping_features_getglyphs_unpaired_surrogate)
{
    CF_BEGIN(c, 3, 10, 12, m_all); shaped s; CHECK_HR_OK(sh_run(&c.x, &s, L"A\xd83d" L"B", FALSE, NULL, 0));
    CHECK(s.actual == 3); CHECK(s.gid[0] == 10 && s.gid[1] == 0 && s.gid[2] == 11); cf_close(&c);
}
TEST(r2_shaping_features_getglyphs_cmap4_font_supplementary_is_notdef)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); shaped s; CHECK_HR_OK(sh_run(&c.x, &s, L"A\xd83d\xde00" L"B", FALSE, NULL, 0));
    CHECK_GIDS(&s, 10, 0, 11); CHECK_CMAP(&s, 0, 1, 1, 2); cf_close(&c);
}
TEST(r2_shaping_features_glyph_count_matches_maxp)
{
    CF_BEGIN(c, 3, 1, 4, m_bmp); EXPECT_EQ(IDWriteFontFace_GetGlyphCount(c.x.face), TTF_DEF_NGLYPHS); cf_close(&c);
}
