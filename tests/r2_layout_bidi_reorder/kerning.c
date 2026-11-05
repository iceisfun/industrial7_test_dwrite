/* IDWriteTextLayout1::SetPairKerning with a font that carries a legacy 'kern' table.
 *
 * Facts (GetGlyphs -> ApplyKerning, OpenTypeKerningPairs):
 *  - kerning is applied only for runs whose pair-kerning flag is set (default off), when the face has kerning pairs;
 *  - the adjustment of a pair (left, right) is added to the advance of the LEFT glyph of the pair, scaled by size/unitsPerEm,
 *    and only while that advance still equals the unkerned advance; the result never goes below zero;
 *  - pairs are matched in glyph order of the run and never across run (format) boundaries.
 * Fixture "DwBidiKern": (A,A) -64, (A,B) -128, (A,V) -256, (V,A) -192 units; at size 16 these are -0.5, -1, -2, -1.5 DIPs.
 * U+05D0/U+05D1 map to the glyphs of A and B.
 */
#include "bx.h"

static float width_of(const WCHAR *text, int kern, int base)
{
    BX_BEGIN(); BX_LAYOUT_F(l, BX_KERN, text, 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    float w; DWRITE_TEXT_METRICS tm;
    if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    if (kern) CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, (UINT32)wcslen(text))));
    bx_metrics(l, &tm); w = tm.widthIncludingTrailingWhitespace;
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
    return w;
}

typedef struct { const char *nm; const WCHAR *text; float adj; } kr_row;
static void run_kern(const kr_row *r)
{
    float off = width_of(r->text, 0, 0), on = width_of(r->text, 1, 0);
    float n = (float)wcslen(r->text);
    CHK_NEAR(off, n * CW);                                    /* flag off: plain advances (default) */
    CHK_NEAR(on, n * CW + r->adj);
}
#define KR(nm, text, adj) static const kr_row kr_##nm = { #nm, text, adj }; TEST(r2_layout_bidi_reorder_kerning_ltr_##nm) { run_kern(&kr_##nm); }
KR(ab,        L"AB",    -1.0f)
KR(ba_none,   L"BA",     0.0f)
KR(av,        L"AV",    -2.0f)
KR(va,        L"VA",    -1.5f)
KR(aa,        L"AA",    -0.5f)
KR(aaa,       L"AAA",   -1.0f)
KR(ava,       L"AVA",   -3.5f)
KR(abab,      L"ABAB",  -2.0f - 0.0f)
KR(no_pairs,  L"XYZ",    0.0f)
KR(single,    L"A",      0.0f)
KR(avab,      L"AVAB",  -2.0f - 1.5f - 1.0f)
KR(space_between, L"A V", 0.0f)
KR(lower_case_no_pair, L"ab", 0.0f)
KR(abc,       L"ABC",   -1.0f)
KR(cab,       L"CAB",   -1.0f)

/* RTL paragraph with Latin text: the same adjustments (the run is left-to-right) */
TEST(r2_layout_bidi_reorder_kerning_latin_in_rtl_paragraph)
{
    float off = width_of(L"AB", 0, 1), on = width_of(L"AB", 1, 1);
    CHK_NEAR(off, 2 * CW); CHK_NEAR(on, 2 * CW - 1.0f);
}

/* Hebrew letters share the glyphs of A and B; only ONE of the two orders is kerned, whichever way the engine pairs RTL glyphs. */
TEST(r2_layout_bidi_reorder_kerning_hebrew_pair_order)
{
    float ab = width_of(L"\u05d0\u05d1", 1, 0), ba = width_of(L"\u05d1\u05d0", 1, 0);
    float ab_off = width_of(L"\u05d0\u05d1", 0, 0), ba_off = width_of(L"\u05d1\u05d0", 0, 0);
    CHK_NEAR(ab_off, 2 * CW); CHK_NEAR(ba_off, 2 * CW);
    CHECKF((bx_feq(ab, 2 * CW - 1.0f) && bx_feq(ba, 2 * CW)) || (bx_feq(ab, 2 * CW) && bx_feq(ba, 2 * CW - 1.0f)),
           "exactly one of aleph-bet (%g) / bet-aleph (%g) must be kerned by 1.0", ab, ba);
    t_note("RTL pair kerning applies to the %s order of the text", bx_feq(ab, 2 * CW - 1.0f) ? "logical (aleph, bet)" : "visual (bet, aleph)");
}
TEST(r2_layout_bidi_reorder_kerning_hebrew_in_rtl_paragraph_matches_ltr_paragraph)
{
    CHK_NEAR(width_of(L"\u05d0\u05d1", 1, 1), width_of(L"\u05d0\u05d1", 1, 0));
    CHK_NEAR(width_of(L"\u05d1\u05d0", 1, 1), width_of(L"\u05d1\u05d0", 1, 0));
}

/* kerning flag on a subrange */
TEST(r2_layout_bidi_reorder_kerning_range_only_covers_its_runs)
{
    BX_BEGIN(); BX_LAYOUT_F(l, BX_KERN, L"ABAB", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l); DWRITE_TEXT_METRICS tm;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 2)));              /* "AB" only */
    bx_metrics(l, &tm); CHK_NEAR(tm.width, 4 * CW - 1.0f);                             /* A,B kerned; B|A and the second A,B are not */
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(2, 2)));
    bx_metrics(l, &tm); CHK_NEAR(tm.width, 4 * CW - 2.0f);
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, FALSE, RNG(0, 4)));
    bx_metrics(l, &tm); CHK_NEAR(tm.width, 4 * CW);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* a pair split by a run boundary (another font size) is not kerned */
TEST(r2_layout_bidi_reorder_kerning_not_across_format_boundary)
{
    BX_BEGIN(); BX_LAYOUT_F(l, BX_KERN, L"AB", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l); DWRITE_TEXT_METRICS tm;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 2)));
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(1, 1)));
    bx_metrics(l, &tm); CHK_NEAR(tm.width, CW + 2 * CW);                               /* A (16) + B (32): no adjustment */
    BX_DONE(l);
}
TEST(r2_layout_bidi_reorder_kerning_flag_boundary_inside_pair)
{
    BX_BEGIN(); BX_LAYOUT_F(l, BX_KERN, L"AB", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l); DWRITE_TEXT_METRICS tm;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 1)));               /* flag on A only: A and B are in different runs */
    bx_metrics(l, &tm); CHK_NEAR(tm.width, 2 * CW);
    BX_DONE(l);
}

/* scaling with font size: -128 units at 2048 upem */
TEST(r2_layout_bidi_reorder_kerning_scales_with_size)
{
    BX_BEGIN(); static const float sizes[] = { 8.0f, 16.0f, 32.0f, 20.5f, 64.0f }; unsigned i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextLayout *l = bx_layout_(&X, BX_KERN, L"AB", sizes[i], 1000.0f, 200.0f); IDWriteTextLayout1 *l1 = NULL; DWRITE_TEXT_METRICS tm;
        if (!l) continue;
        if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1)) && l1) {
            IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 2));
            bx_metrics(l, &tm);
            CHK_NEAR(tm.width, 2 * 1000.0f * sizes[i] / 2048.0f - 128.0f * sizes[i] / 2048.0f);
            IDWriteTextLayout1_Release(l1);
        }
        IDWriteTextLayout_Release(l);
    }
    BX_END();
}

/* the system-independent family without a kern table is unaffected by the flag */
TEST(r2_layout_bidi_reorder_kerning_font_without_pairs_unchanged)
{
    BX_BEGIN(); BX_LAYOUT(l, L"AVAB", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l); DWRITE_TEXT_METRICS tm;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 4)));
    bx_metrics(l, &tm); CHK_NEAR(tm.width, 4 * CW);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* drawn advances carry the adjustment on the left glyph of the pair */
TEST(r2_layout_bidi_reorder_kerning_draw_advances)
{
    BX_BEGIN(); BX_LAYOUT_F(l, BX_KERN, L"AVA", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l); dwx_renderer *r;
    IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 3));
    r = bx_draw(l, 0, 0);
    if (r) {
        const dwx_draw *d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &(int){ 0 });
        CHECK(d && d->nglyphs == 3);
        if (d && d->nglyphs == 3) { CHK_NEAR(d->advances[0], CW - 2.0f); CHK_NEAR(d->advances[1], CW - 1.5f); CHK_NEAR(d->advances[2], CW); }
        dwx_release(r);
    }
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* kerning and character spacing together: kerning first, spacing added on top */
TEST(r2_layout_bidi_reorder_kerning_with_character_spacing)
{
    BX_BEGIN(); BX_LAYOUT_F(l, BX_KERN, L"AB", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l); DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 2));
    IDWriteTextLayout1_SetCharacterSpacing(l1, 1.0f, 1.0f, 0.0f, RNG(0, 2));
    bx_metrics(l, &tm);
    t_note("kerned+spaced width %g (kern only %g, spacing only %g)", tm.width, 2 * CW - 1.0f, 2 * CW + 4.0f);
    CHECK(tm.width <= 2 * CW + 4.0f + EPS && tm.width >= 2 * CW + 4.0f - 1.0f - EPS);
    BX_DONE(l);
}

/* kerning toggled after layout: widths follow immediately */
TEST(r2_layout_bidi_reorder_kerning_toggle_relayouts)
{
    BX_BEGIN(); BX_LAYOUT_F(l, BX_KERN, L"AV", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l); DWRITE_TEXT_METRICS tm;
    bx_metrics(l, &tm); CHK_NEAR(tm.width, 2 * CW);
    IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0, 2)); bx_metrics(l, &tm); CHK_NEAR(tm.width, 2 * CW - 2.0f);
    IDWriteTextLayout1_SetPairKerning(l1, FALSE, RNG(0, 2)); bx_metrics(l, &tm); CHK_NEAR(tm.width, 2 * CW);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* IDWriteFontFace1::GetKerningPairAdjustments of the fixture agrees with the numbers above */
TEST(r2_layout_bidi_reorder_kerning_fontface_adjustments)
{
    BX_BEGIN(); IDWriteFontCollection *c = X.coll; UINT32 idx = 0; BOOL ex = FALSE;
    IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL; IDWriteFontFace1 *f1 = NULL;
    if (FAILED(IDWriteFontCollection_FindFamilyName(c, BX_KERN, &idx, &ex)) || !ex) { BX_END(); t_skip("family"); return; }
    IDWriteFontCollection_GetFontFamily(c, idx, &fam);
    if (fam) IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
    if (font) IDWriteFont_CreateFontFace(font, &face);
    if (face) IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace1, (void **)&f1);
    if (f1) {
        UINT16 g[4] = { 2, 3, 23, 2 }; INT32 adj[4] = { 7, 7, 7, 7 };
        CHECK(IDWriteFontFace1_HasKerningPairs(f1));
        CHECK_HR_OK(IDWriteFontFace1_GetKerningPairAdjustments(f1, 4, g, adj));
        CHECK(adj[0] == -128 && adj[1] == 0 && adj[2] == -192 && adj[3] == 0);       /* (A,B) (B,V) (V,A) and the last glyph */
        IDWriteFontFace1_Release(f1);
    } else t_skip("IDWriteFontFace1 unavailable");
    DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam);
    BX_END();
}
