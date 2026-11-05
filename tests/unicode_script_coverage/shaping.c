/* IDWriteTextAnalyzer::GetGlyphs / GetGlyphPlacements on Unicode edge text: surrogate pairs, mirrored characters on RTL runs,
 * zero-width and format characters, private use, noncharacters, tag characters and variation selectors.
 * The synthetic UcTest font gives every code point of interest its own glyph id (gid = uc_gid), so the output is exact.
 */
#include "uc.h"

#define G(cp) uc_gid(UC_V_BOTH, cp)

/* Shape `t` against the UC_V_BOTH font; returns the result via *s. */
static int shape_ltr_rtl(const WCHAR *t, UINT32 n, BOOL rtl, uc_shaped *s, uc_face *u)
{
    if (!uc_analyzer()) { t_skip("no analyzer"); return 0; }
    if (FAILED(uc_shape(u->face, t, n, rtl, s))) { CHECK_HR_OK(s->hr); return 0; }
    return 1;
}

/* ---- surrogate pairs ------------------------------------------------------------------------------------------------------------------- */
TEST(glyphs_surrogate_pair_single_glyph)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B"; uc_shaped s;
    if (shape_ltr_rtl(t, 4, FALSE, &s, &u)) {
        CHECK_HR_OK(s.hr);
        CHECK(s.actual == 3);
        CHECK(s.gid[0] == G('A') && s.gid[1] == G(0x1F600) && s.gid[2] == G('B'));
        CHECK(s.cluster[0] == 0 && s.cluster[1] == 1 && s.cluster[2] == 1 && s.cluster[3] == 2);
    }
    uc_face_close(&u);
}
TEST(glyphs_surrogate_pairs_back_to_back)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"\xD800\xDC00\xDBFF\xDFFF\xD83D\xDE00"; uc_shaped s;     /* U+10000 U+10FFFF U+1F600 */
    if (shape_ltr_rtl(t, 6, FALSE, &s, &u)) {
        CHECK(s.actual == 3);
        CHECK(s.gid[0] == G(0x10000) && s.gid[1] == G(0x10FFFF) && s.gid[2] == G(0x1F600));
        CHECK(s.cluster[0] == 0 && s.cluster[1] == 0 && s.cluster[2] == 1 && s.cluster[3] == 1 && s.cluster[4] == 2 && s.cluster[5] == 2);
    }
    uc_face_close(&u);
}
TEST(glyphs_surrogate_pair_rtl)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B"; uc_shaped s;
    if (shape_ltr_rtl(t, 4, TRUE, &s, &u)) {
        /* glyphs stay in logical order for RTL runs (the run origin is on the right, glyph 0 is the first logical character) */
        CHECK(s.actual == 3);
        CHECK(s.gid[0] == G('A') && s.gid[1] == G(0x1F600) && s.gid[2] == G('B'));
        CHECK(s.cluster[0] == 0 && s.cluster[1] == 1 && s.cluster[2] == 1 && s.cluster[3] == 2);
    }
    uc_face_close(&u);
}
TEST(glyphs_rtl_keeps_logical_order)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"ABC"; uc_shaped s;
    if (shape_ltr_rtl(t, 3, TRUE, &s, &u)) {
        CHECK(s.actual == 3);
        CHECK(s.gid[0] == G('A') && s.gid[1] == G('B') && s.gid[2] == G('C'));
        CHECK(s.cluster[0] == 0 && s.cluster[1] == 1 && s.cluster[2] == 2);
    }
    uc_face_close(&u);
}
TEST(glyphs_ltr_identity_cluster_map)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"ABC abc 012"; uc_shaped s; UINT32 i;
    if (shape_ltr_rtl(t, 11, FALSE, &s, &u)) {
        CHECK(s.actual == 11);
        for (i = 0; i < 11; i++) { CHECK(s.cluster[i] == i); CHECK(s.gid[i] == G(t[i])); }
    }
    uc_face_close(&u);
}
TEST(glyphs_unmapped_is_notdef)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"A\x4E8C\xD840\xDC00"; uc_shaped s;                    /* U+4E8C and U+20000+? (U+20000 is mapped) */
    if (shape_ltr_rtl(t, 4, FALSE, &s, &u)) {
        CHECK(s.actual == 3);
        CHECK(s.gid[0] == G('A')); CHECK(s.gid[1] == 0);
        CHECK(s.gid[2] == G(0x20000));
    }
    uc_face_close(&u);
}
TEST(glyphs_unmapped_supplementary_is_one_notdef)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"A\xD83C\xDE00" L"B"; uc_shaped s;                    /* U+1F200: not in the cmap */
    if (shape_ltr_rtl(t, 4, FALSE, &s, &u)) {
        CHECK(s.actual == 3);
        CHECK(s.gid[1] == 0);
        CHECK(s.cluster[1] == 1 && s.cluster[2] == 1);
    }
    uc_face_close(&u);
}
TEST(glyphs_insufficient_buffer_for_pair)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B"; uc_shaped s; UINT32 act = 0xdeadbeef; UINT16 cl[4]; DWRITE_SHAPING_TEXT_PROPERTIES tp[4]; UINT16 gi[4]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[4];
    DWRITE_SCRIPT_ANALYSIS sa = uc_script_at(t, 4, 0, NULL);
    if (!uc_analyzer()) { uc_face_close(&u); t_skip("no analyzer"); return; }
    (void)s;
    CHECK_HR(IDWriteTextAnalyzer_GetGlyphs(uc_analyzer(), t, 4, u.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 2, cl, tp, gi, gp, &act), DW_E_NOT_SUFFICIENT_BUFFER);
    uc_face_close(&u);
}
TEST(placements_surrogate_pair_advance)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"A\xD83D\xDE00" L"B"; uc_shaped s; float adv[8]; DWRITE_GLYPH_OFFSET off[8];
    if (shape_ltr_rtl(t, 4, FALSE, &s, &u)) {
        DWRITE_SCRIPT_ANALYSIS sa = uc_script_at(t, 4, 0, NULL);
        memset(adv, 0, sizeof adv);
        CHECK_HR_OK(IDWriteTextAnalyzer_GetGlyphPlacements(uc_analyzer(), t, s.cluster, s.tp, 4, s.gid, s.gp, s.actual, u.face, 2048.0f, FALSE, FALSE, &sa, L"en-us",
                                                           NULL, NULL, 0, adv, off));
        CHECK(adv[0] == 1000.0f && adv[1] == 1000.0f && adv[2] == 1000.0f);
    }
    uc_face_close(&u);
}

/* ---- mirrored characters on RTL runs ----------------------------------------------------------------------------------------------------- */
static void mirror_pair(unsigned a, unsigned b)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    WCHAR ta[2] = { (WCHAR)a, 0 }, tb[2] = { (WCHAR)b, 0 }; uc_shaped s;
    DWRITE_SCRIPT_ANALYSIS lat = uc_script_at(L"a", 1, 0, NULL);
    if (!uc_analyzer()) { uc_face_close(&u); t_skip("no analyzer"); return; }
    /* left-to-right: the glyph of the character itself */
    CHECK_HR_OK(uc_shape_sa(u.face, ta, 1, FALSE, lat, &s)); CHECK(s.actual == 1 && s.gid[0] == G(a));
    CHECK_HR_OK(uc_shape_sa(u.face, tb, 1, FALSE, lat, &s)); CHECK(s.actual == 1 && s.gid[0] == G(b));
    /* right-to-left: the glyph of its mirror image */
    CHECK_HR_OK(uc_shape_sa(u.face, ta, 1, TRUE, lat, &s));
    CHECKF(s.actual == 1 && s.gid[0] == G(b), "U+%04X in RTL: glyph %u want %u (U+%04X)", a, s.gid[0], G(b), b);
    CHECK_HR_OK(uc_shape_sa(u.face, tb, 1, TRUE, lat, &s));
    CHECKF(s.actual == 1 && s.gid[0] == G(a), "U+%04X in RTL: glyph %u want %u (U+%04X)", b, s.gid[0], G(a), a);
    /* script analysis of the character itself (Common) must give the same answer */
    CHECK_HR_OK(uc_shape(u.face, ta, 1, TRUE, &s)); CHECK(s.actual == 1 && s.gid[0] == G(b));
    uc_face_close(&u);
}
#define MIRROR_TEST(nm, a, b) TEST(mirror_##nm) { mirror_pair(a, b); }
MIRROR_TEST(parens, '(', ')') MIRROR_TEST(brackets, '[', ']') MIRROR_TEST(angles, '<', '>') MIRROR_TEST(braces, '{', '}') MIRROR_TEST(guillemets, 0xAB, 0xBB)

TEST(mirror_not_applied_to_letters_in_rtl)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"AZaz09 "; uc_shaped s; int i;
    for (i = 0; i < 7; i++) { WCHAR c[2] = { t[i], 0 }; if (!uc_analyzer()) break; CHECK_HR_OK(uc_shape(u.face, c, 1, TRUE, &s)); CHECK(s.gid[0] == G(t[i])); }
    uc_face_close(&u);
}
TEST(mirror_sequence_rtl)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"(A)"; uc_shaped s;
    DWRITE_SCRIPT_ANALYSIS lat = uc_script_at(t, 3, 0, NULL);
    if (!uc_analyzer()) { uc_face_close(&u); t_skip("no analyzer"); return; }
    CHECK_HR_OK(uc_shape_sa(u.face, t, 3, TRUE, lat, &s));
    CHECK(s.actual == 3);
    /* logical order, each bracket replaced by its mirror image */
    CHECK(s.gid[0] == G(')') && s.gid[1] == G('A') && s.gid[2] == G('('));
    CHECK_HR_OK(uc_shape_sa(u.face, t, 3, FALSE, lat, &s));
    CHECK(s.gid[0] == G('(') && s.gid[1] == G('A') && s.gid[2] == G(')'));
    uc_face_close(&u);
}
TEST(mirror_all_pairs_one_run_rtl)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"()[]<>{}\x00AB\x00BB"; uc_shaped s; int i;
    DWRITE_SCRIPT_ANALYSIS lat = uc_script_at(L"a", 1, 0, NULL);
    if (!uc_analyzer()) { uc_face_close(&u); t_skip("no analyzer"); return; }
    CHECK_HR_OK(uc_shape_sa(u.face, t, 10, TRUE, lat, &s));
    CHECK(s.actual == 10);
    for (i = 0; i < 10; i++) {                         /* glyph i is text char i, mirrored */
        WCHAR src = t[i]; unsigned want = G(src);
        switch (src) { case '(': want = G(')'); break; case ')': want = G('('); break; case '[': want = G(']'); break; case ']': want = G('['); break;
            case '<': want = G('>'); break; case '>': want = G('<'); break; case '{': want = G('}'); break; case '}': want = G('{'); break;
            case 0xAB: want = G(0xBB); break; case 0xBB: want = G(0xAB); break; }
        CHECKF(s.gid[i] == want, "glyph %d is %u want %u", i, s.gid[i], want);
    }
    uc_face_close(&u);
}
TEST(mirror_system_arial_relation)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = f ? dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    uc_shaped s; UINT32 cp[2] = { '(', ')' }; UINT16 g[2];
    DWRITE_SCRIPT_ANALYSIS lat = uc_script_at(L"a", 1, 0, NULL);
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 2, g));
    CHECK_HR_OK(uc_shape_sa(face, L"(", 1, TRUE, lat, &s)); CHECK(s.actual == 1 && s.gid[0] == g[1]);
    CHECK_HR_OK(uc_shape_sa(face, L")", 1, TRUE, lat, &s)); CHECK(s.actual == 1 && s.gid[0] == g[0]);
    CHECK_HR_OK(uc_shape_sa(face, L"(", 1, FALSE, lat, &s)); CHECK(s.actual == 1 && s.gid[0] == g[0]);
    t_note("Arial '(' glyph %u ')' glyph %u", g[0], g[1]);
    DW_RELEASE(face);
}

/* ---- zero-width, format, private use, noncharacters, tags: structural invariants hard, glyph choice recorded ------------------------------- */
static void special_char(unsigned cp, int hard_glyph)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    WCHAR t[6]; int n = 0, k; uc_shaped s; UINT32 i, tl;
    t[n++] = 'A'; n += uc_utf16(cp, t + n); t[n++] = 'B';
    tl = (UINT32)n;
    if (!uc_analyzer()) { uc_face_close(&u); t_skip("no analyzer"); return; }
    CHECK_HR_OK(uc_shape(u.face, t, tl, FALSE, &s));
    CHECK(s.actual >= 2 && s.actual <= tl);
    CHECK(s.gid[0] == G('A'));
    CHECK(s.gid[s.actual - 1] == G('B'));
    for (i = 0; i < tl; i++) CHECKF(s.cluster[i] < s.actual, "cluster[%u]=%u >= %u glyphs", i, s.cluster[i], s.actual);
    for (i = 1; i < tl; i++) CHECK(s.cluster[i] >= s.cluster[i - 1]);              /* LTR maps ascend */
    CHECK(s.cluster[0] == 0);
    CHECK(s.cluster[tl - 1] == s.actual - 1);
    k = n - 2 - 1;                                                                /* UTF-16 units of the special character - 1 */
    if (k == 1) CHECK(s.cluster[1] == s.cluster[2]);                              /* both halves of a pair share a cluster */
    if (hard_glyph) {
        CHECKF(s.actual == tl - k, "U+%04X: %u glyphs", cp, s.actual);
        CHECKF(s.gid[1] == G(cp), "U+%04X: glyph %u want %u", cp, s.gid[1], G(cp));
    } else t_note("U+%04X between A and B: %u glyphs, middle glyph %u (font maps it to %u), isZeroWidthSpace=%u isClusterStart=%u", cp, s.actual,
                  s.actual > 2 ? s.gid[1] : 0, G(cp), s.actual > 1 ? s.gp[1].isZeroWidthSpace : 0, s.actual > 1 ? s.gp[1].isClusterStart : 0);
    uc_face_close(&u);
}
#define SPECIAL_TEST(nm, cp, hard) TEST(special_##nm) { special_char(cp, hard); }
SPECIAL_TEST(zwsp, 0x200B, 0) SPECIAL_TEST(zwnj, 0x200C, 0) SPECIAL_TEST(zwj, 0x200D, 0) SPECIAL_TEST(lrm, 0x200E, 0) SPECIAL_TEST(rlm, 0x200F, 0)
SPECIAL_TEST(word_joiner, 0x2060, 0) SPECIAL_TEST(bom, 0xFEFF, 0) SPECIAL_TEST(soft_hyphen, 0xAD, 0) SPECIAL_TEST(nbsp, 0xA0, 1)
SPECIAL_TEST(line_sep, 0x2028, 0) SPECIAL_TEST(para_sep, 0x2029, 0) SPECIAL_TEST(replacement, 0xFFFD, 1) SPECIAL_TEST(pua_bmp, 0xE000, 1)
SPECIAL_TEST(pua_bmp_end, 0xF8FF, 1) SPECIAL_TEST(pua_plane15, 0xF0000, 1) SPECIAL_TEST(pua_plane16, 0x10FFFD, 1)
SPECIAL_TEST(nonchar_fdd0, 0xFDD0, 0) SPECIAL_TEST(nonchar_fdef, 0xFDEF, 0) SPECIAL_TEST(nonchar_fffe, 0xFFFE, 0) SPECIAL_TEST(nonchar_ffff, 0xFFFF, 0)
SPECIAL_TEST(nonchar_1fffe, 0x1FFFE, 0) SPECIAL_TEST(nonchar_10ffff, 0x10FFFF, 0)
SPECIAL_TEST(tag_begin, 0xE0001, 0) SPECIAL_TEST(tag_space, 0xE0020, 0) SPECIAL_TEST(tag_cancel, 0xE007F, 0)
SPECIAL_TEST(vs17, 0xE0100, 0) SPECIAL_TEST(vs1, 0xFE00, 0) SPECIAL_TEST(vs16, 0xFE0F, 0) SPECIAL_TEST(del, 0x7F, 0) SPECIAL_TEST(nul, 0x0000, 0)
SPECIAL_TEST(tab, 0x09, 0) SPECIAL_TEST(nel, 0x85, 0) SPECIAL_TEST(ideographic_space, 0x3000, 0) SPECIAL_TEST(en_quad, 0x2000, 0) SPECIAL_TEST(nnbsp, 0x202F, 0)
SPECIAL_TEST(math_bold_a, 0x1D400, 1) SPECIAL_TEST(regional_a, 0x1F1E6, 1) SPECIAL_TEST(emoji, 0x1F600, 1) SPECIAL_TEST(plane2, 0x20000, 1)
SPECIAL_TEST(unassigned_bmp_mapped, 0xD7FF, 1)

TEST(special_zwsp_zero_width_flag_note)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"A\x200B" L"B"; uc_shaped s;
    if (shape_ltr_rtl(t, 3, FALSE, &s, &u)) {
        UINT32 i;
        for (i = 0; i < s.actual; i++) t_note("glyph %u: id %u justification %u clusterStart %u diacritic %u zeroWidth %u", i, s.gid[i], s.gp[i].justification, s.gp[i].isClusterStart, s.gp[i].isDiacritic, s.gp[i].isZeroWidthSpace);
    }
    uc_face_close(&u);
}
TEST(special_first_glyph_is_cluster_start)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"AB\xD83D\xDE00" L"C"; uc_shaped s; UINT32 i;
    if (shape_ltr_rtl(t, 5, FALSE, &s, &u)) {
        for (i = 0; i < s.actual; i++) CHECKF(s.gp[i].isClusterStart == 1, "glyph %u not a cluster start", i);
        for (i = 0; i < s.actual; i++) CHECK(s.gp[i].isZeroWidthSpace == 0);
    }
    uc_face_close(&u);
}

/* ---- precomposed vs combining sequences ------------------------------------------------------------------------------------------------------ */
TEST(combining_sequence_cluster_note)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR pre[] = L"\x00E9", dec[] = L"e\x0301"; uc_shaped s;
    if (shape_ltr_rtl(pre, 1, FALSE, &s, &u)) { CHECK(s.actual == 1); CHECK(s.gid[0] == G(0xE9)); CHECK(s.cluster[0] == 0); }
    if (shape_ltr_rtl(dec, 2, FALSE, &s, &u)) {
        CHECK(s.actual >= 1 && s.actual <= 2);
        CHECK(s.cluster[0] == 0 && s.cluster[1] < s.actual);
        t_note("e + U+0301 in the synthetic font: %u glyph(s), glyphs %u %u, cluster map %u %u (U+00E9 maps to %u)", s.actual, s.gid[0], s.actual > 1 ? s.gid[1] : 0,
               s.cluster[0], s.cluster[1], G(0xE9));
    }
    uc_face_close(&u);
}
TEST(combining_mark_run_cluster_map_monotonic)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static const WCHAR t[] = L"e\x0301\x0301" L"A"; uc_shaped s; UINT32 i;
    if (shape_ltr_rtl(t, 4, FALSE, &s, &u)) {
        for (i = 1; i < 4; i++) CHECK(s.cluster[i] >= s.cluster[i - 1]);
        CHECK(s.gid[s.actual - 1] == G('A'));
        t_note("e + 2x U+0301 + A: %u glyphs, cluster map %u %u %u %u", s.actual, s.cluster[0], s.cluster[1], s.cluster[2], s.cluster[3]);
    }
    uc_face_close(&u);
}

/* ---- variation selectors with a format 14 subtable ------------------------------------------------------------------------------------------- */
static void uvs_run(const WCHAR *t, UINT32 n, const char *what)
{
    UC_FACE_BEGIN(u, UC_V_UVS);
    uc_shaped s; UINT32 i;
    if (!uc_analyzer()) { uc_face_close(&u); t_skip("no analyzer"); return; }
    CHECK_HR_OK(uc_shape(u.face, t, n, FALSE, &s));
    CHECK(s.actual >= 1 && s.actual <= n);
    for (i = 1; i < n; i++) CHECK(s.cluster[i] >= s.cluster[i - 1]);
    t_note("%s: %u glyph(s): %u %u %u, cluster map %u %u", what, s.actual, s.gid[0], s.actual > 1 ? s.gid[1] : 0, s.actual > 2 ? s.gid[2] : 0, s.cluster[0], n > 1 ? s.cluster[1] : 0);
    uc_face_close(&u);
}
TEST(uvs_default_sequence_note) { uvs_run(L"A\xFE00", 2, "A + VS1 (default UVS)"); }
TEST(uvs_nondefault_sequence_note) { uvs_run(L"B\xFE00", 2, "B + VS1 (non-default UVS, glyph 100)"); }
TEST(uvs_supplementary_selector_note) { uvs_run(L"C\xDB40\xDD00", 3, "C + VS17 (non-default UVS, glyph 101)"); }
TEST(uvs_unlisted_selector_note) { uvs_run(L"A\xFE0F", 2, "A + VS16 (not in format 14)"); }
TEST(uvs_get_glyph_indices_ignores_selector_tables)
{
    UC_FACE_BEGIN(u, UC_V_UVS);
    UINT32 cp[6] = { 'A', 'B', 'C', 0xFE00, 0xFE0F, 0xE0100 }; UINT16 g[6]; int i;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(u.face, cp, 6, g));
    for (i = 0; i < 6; i++) CHECKF(g[i] == uc_gid(UC_V_UVS, cp[i]), "U+%04X -> %u want %u", cp[i], g[i], uc_gid(UC_V_UVS, cp[i]));
    CHECK(g[1] != UC_UVS_NONDEFAULT_GID);
    uc_face_close(&u);
}
TEST(uvs_font_otherwise_behaves_like_plain_font)
{
    UC_FACE_BEGIN(u, UC_V_UVS);
    UC_FACE_BEGIN(p, UC_V_BOTH);
    UINT32 cp[256]; UINT16 a[256], b[256]; int i, bad = 0;
    for (i = 0; i < 256; i++) cp[i] = i;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(u.face, cp, 256, a));
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(p.face, cp, 256, b));
    for (i = 0; i < 256; i++) if (a[i] != b[i]) bad++;
    CHECK(bad == 0);
    CHECK(IDWriteFontFace_GetGlyphCount(u.face) == IDWriteFontFace_GetGlyphCount(p.face));
    uc_face_close(&p); uc_face_close(&u);
}

/* ---- system fonts: Arial structural checks ------------------------------------------------------------------------------------------------------ */
TEST(arial_pair_and_combining_clusters)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = f ? dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    uc_shaped s;
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(uc_shape(face, L"e\x0301", 2, FALSE, &s));
    CHECK(s.cluster[0] == 0);
    CHECKF(s.cluster[1] == 0, "Arial: combining acute starts its own cluster (map %u %u)", s.cluster[0], s.cluster[1]);
    t_note("Arial e+U+0301: %u glyph(s)", s.actual);
    CHECK_HR_OK(uc_shape(face, L"\x00E9", 1, FALSE, &s));
    CHECK(s.actual == 1 && s.gid[0] != 0);
    DW_RELEASE(face);
}
TEST(arial_surrogate_pair_notdef_cluster)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = f ? dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    uc_shaped s; static const WCHAR t[] = L"A\xD83D\xDE00" L"B";
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK_HR_OK(uc_shape(face, t, 4, FALSE, &s));
    CHECK(s.actual == 3);
    CHECK(s.gid[1] == 0);                                     /* Arial has no emoji: one notdef for the pair */
    CHECK(s.cluster[1] == 1 && s.cluster[2] == 1 && s.cluster[3] == 2);
    DW_RELEASE(face);
}
TEST(segoe_emoji_pair_glyph_matches_cmap)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = f ? dw_sysface(f, L"Segoe UI Emoji", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    uc_shaped s; UINT32 cp = 0x1F600; UINT16 g;
    if (!face) { t_skip("Segoe UI Emoji not installed"); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g));
    CHECK_HR_OK(uc_shape(face, L"\xD83D\xDE00", 2, FALSE, &s));
    CHECK(s.actual == 1);
    CHECK(s.gid[0] == g);
    CHECK(s.cluster[0] == 0 && s.cluster[1] == 0);
    DW_RELEASE(face);
}
TEST(arial_mirror_rtl_all_pairs)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = f ? dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    static const UINT32 pr[][2] = { { '(', ')' }, { '[', ']' }, { '<', '>' }, { '{', '}' }, { 0xAB, 0xBB } }; int i;
    DWRITE_SCRIPT_ANALYSIS lat = uc_script_at(L"a", 1, 0, NULL);
    if (!face) { t_skip("Arial not installed"); return; }
    for (i = 0; i < 5; i++) {
        UINT16 g[2]; uc_shaped s; WCHAR c[2] = { (WCHAR)pr[i][0], 0 };
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, pr[i], 2, g));
        CHECK_HR_OK(uc_shape_sa(face, c, 1, TRUE, lat, &s));
        CHECKF(s.actual == 1 && s.gid[0] == g[1], "Arial U+%04X RTL glyph %u want %u", pr[i][0], s.gid[0], g[1]);
    }
    DW_RELEASE(face);
}
