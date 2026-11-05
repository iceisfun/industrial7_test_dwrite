/* IDWriteFontFace::GetGlyphIndices / IDWriteFontFace1::GetGlyphIndices / IDWriteFont::HasCharacter over Unicode boundaries.
 *
 * Facts (observed behaviour of IDWriteFontFace::GetGlyphIndices, IDWriteFont::HasCharacter, IDWriteFontFace::GetUnicodeRanges):
 *   - the glyph array is zero-filled for `count` entries before anything is looked up;
 *   - codePoints == NULL with count != 0 => E_INVALIDARG (0x80070057), and the outputs stay zero; count == 0 => S_OK;
 *   - code points are UTF-32; values whose cmap block index (cp >> 6) is >= 0x4400, i.e. cp >= 0x110000, map to glyph 0;
 *   - HasCharacter always succeeds and reports whether the cmap maps the code point to a non-zero glyph.
 */
#include "uc.h"

static HRESULT gi(IDWriteFontFace *face, const UINT32 *cp, UINT32 n, UINT16 *out)
{
    return IDWriteFontFace_GetGlyphIndices(face, cp, n, out);
}
static UINT16 gi1(IDWriteFontFace *face, UINT32 cp)
{
    UINT16 g = 0xABCD; HRESULT hr = gi(face, &cp, 1, &g);
    if (hr != S_OK) return 0xFFFF;
    return g;
}

/* ---- one test per boundary code point, checked against every cmap variant ---------------------------------------------------- */
static void cp_all_variants(unsigned cp)
{
    int v;
    for (v = 0; v < UC_V_NVARIANT; v++) {
        UC_FACE_BEGIN(u, v);
        UINT16 g = gi1(u.face, cp);
        CHECKF(g == uc_gid(v, cp), "variant %d U+%04X -> glyph %u want %u", v, cp, g, uc_gid(v, cp));
        if (u.face1) {
            UINT16 g1 = 0xABCD; UINT32 c = cp;
            CHECK_HR_OK(IDWriteFontFace1_GetGlyphIndices(u.face1, &c, 1, &g1));
            CHECKF(g1 == g, "Face1 vtable result %u differs from Face %u", g1, g);
        }
        uc_face_close(&u);
    }
}
#define CP_TEST(nm, cp) TEST(gi_##nm) { cp_all_variants(cp); }
CP_TEST(u0000, 0x0000) CP_TEST(u0020, 0x0020) CP_TEST(u007f, 0x007F) CP_TEST(u00a0, 0x00A0) CP_TEST(u00ad, 0x00AD)
CP_TEST(u00e9, 0x00E9) CP_TEST(u0301, 0x0301) CP_TEST(u200b, 0x200B) CP_TEST(u200c, 0x200C) CP_TEST(u200d, 0x200D)
CP_TEST(u200e, 0x200E) CP_TEST(u200f, 0x200F) CP_TEST(u2028, 0x2028) CP_TEST(u2029, 0x2029) CP_TEST(u2060, 0x2060)
CP_TEST(ufeff, 0xFEFF) CP_TEST(ufffd, 0xFFFD) CP_TEST(uffff, 0xFFFF) CP_TEST(ufffe, 0xFFFE) CP_TEST(ufdd0, 0xFDD0)
CP_TEST(ufdef, 0xFDEF) CP_TEST(ud7ff, 0xD7FF) CP_TEST(ue000, 0xE000) CP_TEST(uf8ff, 0xF8FF) CP_TEST(ufe00, 0xFE00)
CP_TEST(ufe0f, 0xFE0F) CP_TEST(u0009, 0x0009) CP_TEST(u000a, 0x000A) CP_TEST(u000d, 0x000D) CP_TEST(u0085, 0x0085)
CP_TEST(u2000, 0x2000) CP_TEST(u202f, 0x202F) CP_TEST(u3000, 0x3000) CP_TEST(u10000, 0x10000) CP_TEST(u1f600, 0x1F600)
CP_TEST(u10ffff, 0x10FFFF) CP_TEST(u10fffd, 0x10FFFD) CP_TEST(u1fffe, 0x1FFFE) CP_TEST(u20000, 0x20000) CP_TEST(ue0001, 0xE0001)
CP_TEST(ue0020, 0xE0020) CP_TEST(ue007f, 0xE007F) CP_TEST(ue0100, 0xE0100) CP_TEST(uf0000, 0xF0000) CP_TEST(u1d400, 0x1D400)
CP_TEST(u1f1e6, 0x1F1E6) CP_TEST(u0627, 0x0627) CP_TEST(u05d0, 0x05D0) CP_TEST(u4e2d, 0x4E2D) CP_TEST(u0041, 0x0041)
CP_TEST(u007a, 0x007A) CP_TEST(u0030, 0x0030) CP_TEST(u0039, 0x0039)
/* unmapped neighbours of mapped values */
CP_TEST(u0001, 0x0001) CP_TEST(u001f, 0x001F) CP_TEST(u0021, 0x0021) CP_TEST(u007e, 0x007E) CP_TEST(u0080, 0x0080)
CP_TEST(u00a1, 0x00A1) CP_TEST(u00ac, 0x00AC) CP_TEST(u00ae, 0x00AE) CP_TEST(u2027, 0x2027) CP_TEST(u202a, 0x202A)
CP_TEST(ufeff_1, 0xFEFE) CP_TEST(uff00, 0xFF00) CP_TEST(u10001, 0x10001) CP_TEST(u1f601, 0x1F601) CP_TEST(u10fffe, 0x10FFFE)

/* ---- invalid code points ------------------------------------------------------------------------------------------------------- */
static void invalid_zero(unsigned cp)
{
    int v;
    for (v = 0; v < UC_V_NVARIANT; v++) {
        UC_FACE_BEGIN(u, v);
        UINT32 c = cp; UINT16 g = 0x7777;
        CHECK_HR_OK(gi(u.face, &c, 1, &g));
        CHECKF(g == 0, "variant %d code point 0x%X -> glyph %u, want 0", v, cp, g);
        uc_face_close(&u);
    }
    {
        IDWriteFactory *f = dw_shared(); IDWriteFontFace *a = f ? dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
        if (a) { UINT32 c = cp; UINT16 g = 0x7777; CHECK_HR_OK(gi(a, &c, 1, &g)); CHECKF(g == 0, "Arial 0x%X -> %u", cp, g); IDWriteFontFace_Release(a); }
    }
}
#define INV_TEST(nm, cp) TEST(gi_invalid_##nm) { invalid_zero(cp); }
INV_TEST(110000, 0x110000) INV_TEST(110001, 0x110001) INV_TEST(11ffff, 0x11FFFF) INV_TEST(1fffff, 0x1FFFFF) INV_TEST(200000, 0x200000)
INV_TEST(7fffffff, 0x7FFFFFFF) INV_TEST(80000000, 0x80000000u) INV_TEST(ffffffff, 0xFFFFFFFFu) INV_TEST(ffff0000, 0xFFFF0000u)
INV_TEST(0xdeadbeef, 0xDEADBEEFu) INV_TEST(1000000, 0x1000000) INV_TEST(10ffff_plus_64, 0x110000 + 64)
/* UTF-32 surrogate values are not characters: no cmap of the fixtures maps them */
INV_TEST(d800, 0xD800) INV_TEST(d801, 0xD801) INV_TEST(dbff, 0xDBFF) INV_TEST(dc00, 0xDC00) INV_TEST(dfff, 0xDFFF)
INV_TEST(db80, 0xDB80) INV_TEST(dc80, 0xDC80)

TEST(gi_invalid_surrogate_range_all_zero)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT32 cp[0x800]; UINT16 g[0x800]; int i, bad = 0;
    for (i = 0; i < 0x800; i++) { cp[i] = 0xD800 + i; g[i] = 0x5555; }
    CHECK_HR_OK(gi(u.face, cp, 0x800, g));
    for (i = 0; i < 0x800; i++) if (g[i]) bad++;
    CHECKF(bad == 0, "%d surrogate code points mapped to a glyph", bad);
    uc_face_close(&u);
}
TEST(gi_invalid_beyond_unicode_run)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT32 cp[256]; UINT16 g[256]; int i, bad = 0;
    for (i = 0; i < 256; i++) { cp[i] = 0x10FFF0 + i; g[i] = 0x5555; }
    CHECK_HR_OK(gi(u.face, cp, 256, g));
    for (i = 0; i < 256; i++) {
        unsigned want = cp[i] <= 0x10FFFF ? uc_gid(UC_V_BOTH, cp[i]) : 0;
        if (g[i] != want) bad++;
    }
    CHECKF(bad == 0, "%d mismatches around U+10FFFF", bad);
    uc_face_close(&u);
}

/* ---- argument protocol ------------------------------------------------------------------------------------------------------------- */
TEST(gi_count_zero_ok_and_untouched)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT32 c = 0x41; UINT16 g = 0x1234;
    CHECK_HR_OK(gi(u.face, &c, 0, &g));
    CHECK(g == 0x1234);
    uc_face_close(&u);
}
TEST(gi_null_codepoints_with_count_invalidarg)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT16 g[4] = { 0x1111, 0x2222, 0x3333, 0x4444 }; int i;
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(u.face, NULL, 4, g), E_INVALIDARG);
    /* the glyph array is cleared before the argument is validated */
    for (i = 0; i < 4; i++) CHECKF(g[i] == 0, "g[%d] = 0x%04x after failed call", i, g[i]);
    uc_face_close(&u);
}
TEST(gi_null_codepoints_count_zero_ok)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT16 g = 0x1234;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(u.face, NULL, 0, &g));
    CHECK(g == 0x1234);
    uc_face_close(&u);
}
TEST(gi_face1_null_codepoints_invalidarg)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT16 g[2] = { 0x1111, 0x2222 };
    if (!u.face1) { uc_face_close(&u); t_skip("no IDWriteFontFace1"); return; }
    CHECK_HR(IDWriteFontFace1_GetGlyphIndices(u.face1, NULL, 2, g), E_INVALIDARG);
    CHECK(g[0] == 0 && g[1] == 0);
    uc_face_close(&u);
}
TEST(gi_output_prefilled_is_overwritten)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT32 cp[5] = { 'A', 0x110000, 'z', 0xD800, '0' }; UINT16 g[5] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
    CHECK_HR_OK(gi(u.face, cp, 5, g));
    CHECK(g[0] == 2 && g[1] == 0 && g[2] == 53 && g[3] == 0 && g[4] == 54);
    uc_face_close(&u);
}
TEST(gi_duplicates_and_order)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT32 cp[8] = { 'C', 'A', 'C', 'B', 'A', 'C', 'B', 'A' }; UINT16 g[8]; int i;
    CHECK_HR_OK(gi(u.face, cp, 8, g));
    for (i = 0; i < 8; i++) CHECKF(g[i] == 2 + (cp[i] - 'A'), "g[%d]=%u", i, g[i]);
    uc_face_close(&u);
}
TEST(gi_batch_equals_single_calls)
{
    int v;
    for (v = 0; v < UC_V_NVARIANT; v++) {
        UC_FACE_BEGIN(u, v);
        UINT32 cp[256]; UINT16 g[256]; int i, bad = 0, n = 0;
        for (i = 0; i < uc_nextra; i++) cp[n++] = uc_extra[i];
        for (i = 0; i < 128 && n < 256; i++) cp[n++] = i;
        cp[n++] = 0x110000; cp[n++] = 0xFFFFFFFFu;
        CHECK_HR_OK(gi(u.face, cp, n, g));
        for (i = 0; i < n; i++) if (g[i] != gi1(u.face, cp[i])) bad++;
        CHECKF(bad == 0, "variant %d: %d batch/single mismatches", v, bad);
        uc_face_close(&u);
    }
}
TEST(gi_whole_table_per_variant)
{
    int v;
    for (v = 0; v < UC_V_NVARIANT; v++) {
        UC_FACE_BEGIN(u, v);
        UINT32 cp[256]; UINT16 g[256]; int i, bad = 0, n = 0;
        for (i = 0; i < uc_nextra; i++) cp[n++] = uc_extra[i];
        CHECK_HR_OK(gi(u.face, cp, n, g));
        for (i = 0; i < n; i++) if (g[i] != uc_gid(v, cp[i])) { bad++; t_note("variant %d U+%04X got %u want %u", v, cp[i], g[i], uc_gid(v, cp[i])); }
        CHECKF(bad == 0, "variant %d: %d mismatches", v, bad);
        uc_face_close(&u);
    }
}
TEST(gi_full_bmp_sweep_matches_default_cmap)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    static UINT32 cp[0x10000]; static UINT16 g[0x10000]; unsigned i, bad = 0;
    for (i = 0; i < 0x10000; i++) cp[i] = i;
    CHECK_HR_OK(gi(u.face, cp, 0x10000, g));
    for (i = 0; i < 0x10000; i++) if (g[i] != uc_gid(UC_V_BOTH, i)) bad++;
    CHECKF(bad == 0, "%u BMP code points differ from the cmap", bad);
    uc_face_close(&u);
}
TEST(gi_bmp_sweep_bmp_only_cmap)
{
    UC_FACE_BEGIN(u, UC_V_BMP);
    static UINT32 cp[0x10000]; static UINT16 g[0x10000]; unsigned i, bad = 0;
    for (i = 0; i < 0x10000; i++) cp[i] = i;
    CHECK_HR_OK(gi(u.face, cp, 0x10000, g));
    for (i = 0; i < 0x10000; i++) if (g[i] != uc_gid(UC_V_BMP, i)) bad++;
    CHECKF(bad == 0, "%u BMP code points differ from the cmap", bad);
    uc_face_close(&u);
}
TEST(gi_glyphs_below_glyph_count)
{
    int v;
    for (v = 0; v < UC_V_NVARIANT; v++) {
        UC_FACE_BEGIN(u, v);
        UINT32 cp[256]; UINT16 g[256]; int i, bad = 0;
        UINT16 cnt = IDWriteFontFace_GetGlyphCount(u.face);
        for (i = 0; i < uc_nextra; i++) cp[i] = uc_extra[i];
        CHECK_HR_OK(gi(u.face, cp, uc_nextra, g));
        for (i = 0; i < uc_nextra; i++) if (g[i] >= cnt) bad++;
        CHECKF(bad == 0, "variant %d: %d glyph ids >= glyph count %u", v, bad, cnt);
        CHECK(cnt == UC_FIRST_EXTRA_GID + 64);
        uc_face_close(&u);
    }
}
TEST(gi_variant_ucs4_vs_bmp_differences)
{
    UC_FACE_BEGIN(u4, UC_V_UCS4);
    UC_FACE_BEGIN(ub, UC_V_BMP);
    /* a format 12 table serves supplementary planes and U+FFFF; a format 4 only table cannot hold them */
    CHECK(gi1(u4.face, 0x1F600) == uc_gid(UC_V_UCS4, 0x1F600) && gi1(u4.face, 0x1F600) != 0);
    CHECK(gi1(ub.face, 0x1F600) == 0);
    CHECK(gi1(u4.face, 0xFFFF) != 0);
    CHECK(gi1(ub.face, 0xFFFF) == 0);
    CHECK(gi1(u4.face, 'A') == gi1(ub.face, 'A'));
    uc_face_close(&ub); uc_face_close(&u4);
}
TEST(gi_distinct_glyphs_for_distinct_code_points)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT32 cp[128]; UINT16 g[128]; int i, j, dup = 0;
    for (i = 0; i < uc_nextra; i++) cp[i] = uc_extra[i];
    CHECK_HR_OK(gi(u.face, cp, uc_nextra, g));
    for (i = 0; i < uc_nextra; i++) for (j = i + 1; j < uc_nextra; j++) if (g[i] == g[j]) dup++;
    CHECK(dup == 0);
    uc_face_close(&u);
}
TEST(gi_repeat_stable)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    int i;
    for (i = 0; i < 200; i++) { UINT16 g = gi1(u.face, 0x1F600); if (g != UC_FIRST_EXTRA_GID + 48) { CHECKF(0, "iteration %d: %u", i, g); break; } }
    uc_face_close(&u);
}
TEST(gi_face_survives_after_loader_use_and_factory_calls)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    IDWriteFontFace *again = NULL;
    CHECK(gi1(u.face, 'A') == 2);
    CHECK_HR_OK(IDWriteFontFace_QueryInterface(u.face, &IID_IDWriteFontFace, (void **)&again));
    CHECK(again == u.face);
    CHECK(gi1(again, 'a') == 28);
    DW_RELEASE(again);
    uc_face_close(&u);
}

/* ---- HasCharacter parity -------------------------------------------------------------------------------------------------------------- */
static IDWriteFont *fx_font(uc_fx *x)
{
    IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL;
    if (FAILED(IDWriteFontCollection_GetFontFamily(x->coll, 0, &fam)) || !fam) return NULL;
    IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
    DW_RELEASE(fam);
    return font;
}
static BOOL hc(IDWriteFont *font, UINT32 cp)
{
    BOOL b = 0x55; HRESULT hr = IDWriteFont_HasCharacter(font, cp, &b);
    t_check(hr == S_OK, __FILE__, __LINE__, "HasCharacter", "cp 0x%X -> %s", cp, dw_hr_name(hr));
    return b;
}
TEST(hc_parity_with_glyph_indices_table)
{
    UC_FX_BEGIN();
    IDWriteFont *font = fx_font(&X); IDWriteFontFace *face = NULL;
    if (!font || FAILED(IDWriteFont_CreateFontFace(font, &face))) { DW_RELEASE(font); UC_FX_END(); t_skip("no font"); return; }
    int i, bad = 0;
    for (i = 0; i < uc_nextra; i++) {
        BOOL b = hc(font, uc_extra[i]); UINT16 g = gi1(face, uc_extra[i]);
        if ((b != 0) != (g != 0)) bad++;
        CHECKF(b == (uc_gid(UC_V_BOTH, uc_extra[i]) != 0), "HasCharacter(U+%04X) = %d", uc_extra[i], b);
    }
    CHECK(bad == 0);
    DW_RELEASE(face); DW_RELEASE(font); UC_FX_END();
}
TEST(hc_parity_ascii_sweep)
{
    UC_FX_BEGIN();
    IDWriteFont *font = fx_font(&X); IDWriteFontFace *face = NULL; unsigned c, bad = 0;
    if (!font || FAILED(IDWriteFont_CreateFontFace(font, &face))) { DW_RELEASE(font); UC_FX_END(); t_skip("no font"); return; }
    for (c = 0; c < 0x300; c++) if ((hc(font, c) != 0) != (gi1(face, c) != 0)) bad++;
    CHECKF(bad == 0, "%u parity violations", bad);
    DW_RELEASE(face); DW_RELEASE(font); UC_FX_END();
}
TEST(hc_invalid_code_points_false)
{
    UC_FX_BEGIN();
    IDWriteFont *font = fx_font(&X);
    static const UINT32 bad[] = { 0x110000, 0x110001, 0x1FFFFF, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFFu, 0xD800, 0xDBFF, 0xDC00, 0xDFFF };
    int i;
    if (!font) { UC_FX_END(); t_skip("no font"); return; }
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) CHECKF(!hc(font, bad[i]), "HasCharacter(0x%X) true", bad[i]);
    DW_RELEASE(font); UC_FX_END();
}
TEST(hc_supplementary_true)
{
    UC_FX_BEGIN();
    IDWriteFont *font = fx_font(&X);
    if (!font) { UC_FX_END(); t_skip("no font"); return; }
    CHECK(hc(font, 0x10000)); CHECK(hc(font, 0x1F600)); CHECK(hc(font, 0x10FFFF)); CHECK(hc(font, 0xE0001));
    CHECK(!hc(font, 0x10001));
    DW_RELEASE(font); UC_FX_END();
}

/* ---- system fonts: relations, values noted ----------------------------------------------------------------------------------------- */
static void sys_relations(const WCHAR *family)
{
    IDWriteFactory *f = dw_shared(); IDWriteFont *font = f ? uc_sysfont(f, family) : NULL; IDWriteFontFace *face = NULL;
    static UINT32 cp[0x3000]; static UINT16 g[0x3000]; unsigned i, bad = 0, nonzero = 0;
    if (!font) { t_skip("font %ls not installed", family); return; }
    DW_REQUIRE_OK(IDWriteFont_CreateFontFace(font, &face));
    for (i = 0; i < 0x3000; i++) cp[i] = i * 5 + (i & 3);          /* spread over U+0000..U+EFFC */
    CHECK_HR_OK(gi(face, cp, 0x3000, g));
    for (i = 0; i < 0x3000; i++) {
        BOOL b = hc(font, cp[i]);
        if ((b != 0) != (g[i] != 0)) bad++;
        if (g[i]) nonzero++;
    }
    CHECKF(bad == 0, "%ls: %u HasCharacter/GetGlyphIndices disagreements", family, bad);
    CHECK(nonzero > 0);
    CHECK(gi1(face, 0x110000) == 0);
    CHECK(gi1(face, 0xD800) == 0);
    t_note("%ls: %u of 12288 sampled code points mapped; U+0041 glyph %u U+0020 glyph %u", family, nonzero, gi1(face, 0x41), gi1(face, 0x20));
    DW_RELEASE(face); DW_RELEASE(font);
}
#define SYS_TEST(nm, fam) TEST(sys_relations_##nm) { sys_relations(fam); }
SYS_TEST(arial, L"Arial") SYS_TEST(times, L"Times New Roman") SYS_TEST(courier, L"Courier New") SYS_TEST(segoe_ui, L"Segoe UI")
SYS_TEST(tahoma, L"Tahoma") SYS_TEST(consolas, L"Consolas") SYS_TEST(segoe_symbol, L"Segoe UI Symbol") SYS_TEST(segoe_emoji, L"Segoe UI Emoji")
SYS_TEST(marlett, L"Marlett") SYS_TEST(calibri, L"Calibri") SYS_TEST(verdana, L"Verdana") SYS_TEST(georgia, L"Georgia")
SYS_TEST(cambria_math, L"Cambria Math") SYS_TEST(ms_gothic, L"MS Gothic") SYS_TEST(bahnschrift, L"Bahnschrift")

static void sys_points(const WCHAR *family, const unsigned *cps, int n, const char *what)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = f ? dw_sysface(f, family, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    int i;
    if (!face) { t_skip("font %ls not installed", family); return; }
    for (i = 0; i < n; i++) {
        UINT16 g = gi1(face, cps[i]);
        t_note("%ls %s U+%04X -> glyph %u", family, what, cps[i], g);
    }
    IDWriteFontFace_Release(face);
}
TEST(arial_boundary_glyphs)
{
    static const unsigned cps[] = { 0x0, 0x20, 0x7F, 0xA0, 0xAD, 0x2028, 0xFEFF, 0xFFFD, 0xFFFF, 0x10000, 0x1F600, 0x10FFFF };
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = f ? dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    if (!face) { t_skip("Arial not installed"); return; }
    CHECK(gi1(face, 0x20) != 0);
    CHECK(gi1(face, 0x41) != 0);
    CHECK(gi1(face, 0xA0) != 0);
    CHECK(gi1(face, 0x10FFFF) == 0);
    CHECK(gi1(face, 0xFFFF) == 0);              /* a noncharacter is never drawn by Arial */
    CHECK(gi1(face, 0x1F600) == 0);             /* Arial has no emoji */
    CHECK(gi1(face, 0x41) != gi1(face, 0x42));
    IDWriteFontFace_Release(face);
    sys_points(L"Arial", cps, (int)(sizeof cps / sizeof cps[0]), "boundary");
}
TEST(segoe_emoji_supplementary)
{
    IDWriteFactory *f = dw_shared(); IDWriteFont *font = f ? uc_sysfont(f, L"Segoe UI Emoji") : NULL; IDWriteFontFace *face = NULL;
    if (!font) { t_skip("Segoe UI Emoji not installed"); return; }
    DW_REQUIRE_OK(IDWriteFont_CreateFontFace(font, &face));
    CHECK(hc(font, 0x1F600));
    CHECK(gi1(face, 0x1F600) != 0);
    CHECK(hc(font, 0x1F600) == (gi1(face, 0x1F600) != 0));
    t_note("Segoe UI Emoji: U+1F600 glyph %u, U+1F1E6 glyph %u, U+200D glyph %u", gi1(face, 0x1F600), gi1(face, 0x1F1E6), gi1(face, 0x200D));
    DW_RELEASE(face); DW_RELEASE(font);
}
TEST(consistency_across_two_faces_of_same_font)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *a, *b; unsigned c, bad = 0;
    if (!f) { t_skip("no factory"); return; }
    a = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    b = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); t_skip("Arial missing"); return; }
    for (c = 0; c < 0x500; c++) if (gi1(a, c) != gi1(b, c)) bad++;
    CHECK(bad == 0);
    DW_RELEASE(a); DW_RELEASE(b);
}
