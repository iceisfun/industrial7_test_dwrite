/* IDWriteFontFace4::GetGlyphImageFormats(glyph, ppemFirst, ppemLast, *formats) (the range overload, vtable slot 49) and the
 * face-wide GetGlyphImageFormats() (slot 50).
 * Win10 probing: IDWriteFontFace::GetGlyphImageFormats writes *formats = 0, walks the glyph's linked list of
 * graphic-data entries and ORs the format byte of every entry whose ppem satisfies  ppemFirst <= ppem <= ppemLast  OR  ppem == 0
 * (size independent data such as COLR layers and SVG); there is no argument validation and the result is always S_OK. The face-wide
 * overload returns one cached byte. So, for any glyph:
 *   result(first, last) always contains the size-independent formats, which are exactly result(first, last) for first > last;
 *   result is monotonic in the range; every result is a subset of the face-wide formats. */
#include "g2.h"

#undef IDWriteFontFace4_GetGlyphImageFormats
#undef IDWriteFontFace4_GetGlyphImageFormats_
#define RANGE(f4, g, a, b, out) ((f4)->lpVtbl->GetGlyphImageFormats_((f4), (g), (a), (b), (out)))
#define FACEWIDE(f4) ((UINT32)(f4)->lpVtbl->GetGlyphImageFormats((f4)))

typedef struct { IDWriteFactory *f; dwx_font h; IDWriteFontFace4 *f4; int ok; } imf;
static void imf_close(imf *x)
{
    if (!x->ok) return;
    DW_RELEASE(x->f4); dwx_font_close(&x->h); DW_RELEASE(x->f); x->ok = 0;
}
static int imf_open(imf *x, int with_colr)
{
    ttf_spec s; uint8_t *b; size_t n; HRESULT hr;
    static const ttf_colr_base base[] = { { 2, 0, 2 }, { 5, 2, 1 } };
    static const ttf_colr_layer lay[] = { { 3, 0 }, { 4, 1 }, { 6, 0 } };
    static const uint32_t pal[] = { 0xFFFF0000u, 0xFF00FF00u };
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("factory"); return 0; }
    ttf_default_spec(&s);
    if (with_colr) { s.colr_base = base; s.ncolr_base = 2; s.colr_layers = lay; s.ncolr_layers = 3; s.palette = pal; s.npalette = 2; }
    b = ttf_build(&s, &n);
    hr = b ? dwx_font_open(&x->h, x->f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE) : E_FAIL;
    free(b);
    if (FAILED(hr) || !x->h.face) { dwx_font_close(&x->h); DW_RELEASE(x->f); t_skip("font did not open"); return 0; }
    if (FAILED(IDWriteFontFace_QueryInterface(x->h.face, &IID_IDWriteFontFace4, (void **)&x->f4))) { dwx_font_close(&x->h); DW_RELEASE(x->f); t_skip("IDWriteFontFace4 unsupported"); return 0; }
    x->ok = 1;
    return 1;
}
static UINT32 fmt(imf *x, UINT16 g, UINT32 a, UINT32 b)
{
    DWRITE_GLYPH_IMAGE_FORMATS f = (DWRITE_GLYPH_IMAGE_FORMATS)0xDEADBEEF;
    HRESULT hr = RANGE(x->f4, g, a, b, &f);
    CHECKF(hr == S_OK, "glyph %u %u..%u: %s", g, a, b, dw_hr_name(hr));
    return (UINT32)f;
}

TEST(gap2_gif_output_always_written)
{
    imf x; static const UINT32 fill[] = { 0xFFFFFFFFu, 0xDEADBEEFu, 0x12345678u, 0u }; size_t i; UINT16 g;
    if (!imf_open(&x, 1)) return;
    for (g = 0; g < TTF_DEF_NGLYPHS; g += 7) for (i = 0; i < 4; i++) {
        DWRITE_GLYPH_IMAGE_FORMATS f = (DWRITE_GLYPH_IMAGE_FORMATS)fill[i];
        CHECK_HR_OK(RANGE(x.f4, g, 0, 0xFFFFFFFFu, &f));
        CHECKF((UINT32)f <= 0xFFu, "glyph %u fill 0x%x: result 0x%x has bits above the format byte", g, fill[i], (UINT32)f);
    }
    imf_close(&x);
}
TEST(gap2_gif_plain_font_all_glyphs)
{
    imf x; UINT16 g;
    if (!imf_open(&x, 0)) return;
    EXPECT_EQ(FACEWIDE(x.f4), DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE);
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        UINT32 f = fmt(&x, g, 0, 0xFFFFFFFFu);
        CHECKF((f & ~(UINT32)DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE) == 0, "glyph %u: 0x%x", g, f);
        if (g < 3) t_note("plain glyph %u: 0x%x", g, f);
    }
    imf_close(&x);
}
TEST(gap2_gif_out_of_range_glyph_ids)
{
    imf x; static const UINT16 ids[] = { 69, 100, 255, 1000, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF }; size_t i;
    if (!imf_open(&x, 1)) return;
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++) { UINT32 f = fmt(&x, ids[i], 0, 0xFFFFFFFFu); CHECKF(f == 0, "glyph %u -> 0x%x (the per-glyph offset table is bounds checked: no entries)", ids[i], f); }
    imf_close(&x);
}
TEST(gap2_gif_subset_of_face_formats)
{
    imf x; UINT16 g; static const UINT32 r[][2] = { { 0, 0 }, { 0, 0xFFFFFFFFu }, { 1, 1 }, { 8, 12 }, { 16, 16 }, { 20, 400 }, { 0xFFFFFFFFu, 0xFFFFFFFFu }, { 50, 10 } }; size_t i;
    if (!imf_open(&x, 1)) return;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) for (i = 0; i < sizeof r / sizeof r[0]; i++) { UINT32 f = fmt(&x, g, r[i][0], r[i][1]); CHECKF((f & ~FACEWIDE(x.f4)) == 0, "glyph %u %u..%u -> 0x%x (face 0x%x)", g, r[i][0], r[i][1], f, FACEWIDE(x.f4)); }
    imf_close(&x);
}
TEST(gap2_gif_reversed_range_equals_size_independent_set)
{
    imf x; UINT16 g; static const UINT32 rev[][2] = { { 1, 0 }, { 5, 2 }, { 100, 99 }, { 0xFFFFFFFFu, 0 }, { 0xFFFFFFFFu, 0xFFFFFFFEu }, { 17, 16 } }; size_t i;
    if (!imf_open(&x, 1)) return;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        UINT32 s0 = fmt(&x, g, 1, 0);
        for (i = 0; i < sizeof rev / sizeof rev[0]; i++) CHECKF(fmt(&x, g, rev[i][0], rev[i][1]) == s0, "glyph %u reversed range %u..%u differs from 1..0", g, rev[i][0], rev[i][1]);
    }
    imf_close(&x);
}
TEST(gap2_gif_every_range_contains_size_independent_set)
{
    imf x; UINT16 g; static const UINT32 r[][2] = { { 0, 0 }, { 8, 8 }, { 12, 24 }, { 0, 0xFFFFFFFFu }, { 1000, 2000 }, { 0xFFFFFFFFu, 0xFFFFFFFFu } }; size_t i;
    if (!imf_open(&x, 1)) return;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        UINT32 s0 = fmt(&x, g, 1, 0);
        for (i = 0; i < sizeof r / sizeof r[0]; i++) { UINT32 f = fmt(&x, g, r[i][0], r[i][1]); CHECKF((f & s0) == s0, "glyph %u %u..%u: 0x%x lacks 0x%x", g, r[i][0], r[i][1], f, s0); }
    }
    imf_close(&x);
}
TEST(gap2_gif_monotonic_in_range)
{
    imf x; UINT16 g; static const UINT32 nest[][4] = { { 8, 8, 0, 16 }, { 10, 20, 5, 30 }, { 16, 16, 0, 0xFFFFFFFFu }, { 100, 200, 100, 200 }, { 1, 1, 1, 1 } }; size_t i;
    if (!imf_open(&x, 1)) return;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) for (i = 0; i < sizeof nest / sizeof nest[0]; i++) {
        UINT32 in = fmt(&x, g, nest[i][0], nest[i][1]), out = fmt(&x, g, nest[i][2], nest[i][3]);
        CHECKF((in & ~out) == 0, "glyph %u [%u,%u] in [%u,%u]: 0x%x not in 0x%x", g, nest[i][0], nest[i][1], nest[i][2], nest[i][3], in, out);
    }
    imf_close(&x);
}
TEST(gap2_gif_colr_glyphs_report_color_format)
{
    imf x; UINT32 face, f2, f3, f9;
    if (!imf_open(&x, 1)) return;
    face = FACEWIDE(x.f4);
    EXPECT_EQ(face & DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE, DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE);
    EXPECT_EQ(face & DWRITE_GLYPH_IMAGE_FORMATS_COLR, DWRITE_GLYPH_IMAGE_FORMATS_COLR);
    f2 = fmt(&x, 2, 0, 0xFFFFFFFFu); f3 = fmt(&x, 3, 0, 0xFFFFFFFFu); f9 = fmt(&x, 9, 0, 0xFFFFFFFFu);
    t_note("COLR base glyph 2: 0x%x, layer glyph 3: 0x%x, plain glyph 9: 0x%x", f2, f3, f9);
    G2_SOFT(f2 & DWRITE_GLYPH_IMAGE_FORMATS_COLR);
    G2_SOFT(!(f9 & DWRITE_GLYPH_IMAGE_FORMATS_COLR));
    imf_close(&x);
}
TEST(gap2_gif_colr_is_size_independent)
{
    imf x; UINT16 g;
    if (!imf_open(&x, 1)) return;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) {
        UINT32 a = fmt(&x, g, 1, 0), b = fmt(&x, g, 0, 0xFFFFFFFFu);
        /* COLR layers carry no ppem: they appear even when the requested range is empty */
        CHECKF(((a & DWRITE_GLYPH_IMAGE_FORMATS_COLR) != 0) == ((b & DWRITE_GLYPH_IMAGE_FORMATS_COLR) != 0), "glyph %u: COLR bit differs (0x%x vs 0x%x)", g, a, b);
    }
    imf_close(&x);
}
TEST(gap2_gif_face_wide_stable_and_cached)
{
    imf x; int i; UINT32 f0;
    if (!imf_open(&x, 1)) return;
    f0 = FACEWIDE(x.f4);
    for (i = 0; i < 20; i++) CHECK(FACEWIDE(x.f4) == f0);
    imf_close(&x);
}
TEST(gap2_gif_face_wide_values_plain_vs_colr)
{
    imf a, b;
    if (!imf_open(&a, 0)) return;
    if (!imf_open(&b, 1)) { imf_close(&a); return; }
    CHECK((FACEWIDE(b.f4) & FACEWIDE(a.f4)) == FACEWIDE(a.f4));       /* colour adds bits, never removes TRUETYPE */
    CHECK(FACEWIDE(b.f4) != FACEWIDE(a.f4));
    imf_close(&a); imf_close(&b);
}
TEST(gap2_gif_through_fontface5_pointer)
{
    imf x; IDWriteFontFace5 *f5 = NULL;
    if (!imf_open(&x, 1)) return;
    if (SUCCEEDED(IDWriteFontFace4_QueryInterface(x.f4, &IID_IDWriteFontFace5, (void **)&f5))) {
        DWRITE_GLYPH_IMAGE_FORMATS f = 0, g = 0;
        CHECK_HR_OK(RANGE((IDWriteFontFace4 *)f5, 2, 0, 100, &f));
        CHECK_HR_OK(RANGE(x.f4, 2, 0, 100, &g));
        CHECK(f == g);
        CHECK(FACEWIDE((IDWriteFontFace4 *)f5) == FACEWIDE(x.f4));
        DW_RELEASE(f5);
    } else t_note("IDWriteFontFace5 not exposed");
    imf_close(&x);
}
TEST(gap2_gif_ppem_bounds_inclusive_noted)
{
    imf x;
    if (!imf_open(&x, 1)) return;
    t_note("glyph 2: ppem 0..0 -> 0x%x, 1..1 -> 0x%x, 0xFFFFFFFF..0xFFFFFFFF -> 0x%x", fmt(&x, 2, 0, 0), fmt(&x, 2, 1, 1), fmt(&x, 2, 0xFFFFFFFFu, 0xFFFFFFFFu));
    imf_close(&x);
}

static void sys_gif(const WCHAR *fam, UINT32 cp)
{
    IDWriteFontFace *face = dw_sysface(dw_shared(), fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace4 *f4 = NULL;
    UINT16 g = 0; UINT32 faceFmt, r1, r0, s0; DWRITE_GLYPH_IMAGE_FORMATS f;
    if (!face) { t_skip("%ls not installed", fam); return; }
    if (FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace4, (void **)&f4))) { DW_RELEASE(face); t_skip("no FontFace4"); return; }
    IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g);
    faceFmt = FACEWIDE(f4);
    CHECK_HR_OK(RANGE(f4, g, 0, 0xFFFFFFFFu, &f)); r1 = (UINT32)f;
    CHECK_HR_OK(RANGE(f4, g, 1, 0, &f)); s0 = (UINT32)f;
    CHECK_HR_OK(RANGE(f4, g, 0, 0, &f)); r0 = (UINT32)f;
    t_note("%ls U+%04X glyph %u: face 0x%x, all sizes 0x%x, size independent 0x%x, ppem 0 0x%x", fam, cp, g, faceFmt, r1, s0, r0);
    CHECK((r1 & ~faceFmt) == 0);
    CHECK((r1 & s0) == s0);
    CHECK(s0 == r0);                                    /* ppem 0..0 only matches size independent entries */
    CHECK((faceFmt & DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE) || (faceFmt & DWRITE_GLYPH_IMAGE_FORMATS_CFF));
    DW_RELEASE(f4); DW_RELEASE(face);
}
TEST(gap2_gif_system_arial)       { sys_gif(L"Arial", 'A'); }
TEST(gap2_gif_system_segoe_ui)    { sys_gif(L"Segoe UI", 'g'); }
TEST(gap2_gif_system_emoji)       { sys_gif(L"Segoe UI Emoji", 0x1F600); }
TEST(gap2_gif_system_symbol)      { sys_gif(L"Segoe UI Symbol", 0x2603); }
TEST(gap2_gif_system_bahnschrift) { sys_gif(L"Bahnschrift", 'A'); }
TEST(gap2_gif_system_ms_gothic)   { sys_gif(L"MS Gothic", 0x3042); }
TEST(gap2_gif_system_consolas)    { sys_gif(L"Consolas", 'x'); }
TEST(gap2_gif_system_emoji_has_color_glyph)
{
    IDWriteFontFace *face = dw_sysface(dw_shared(), L"Segoe UI Emoji", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace4 *f4 = NULL;
    UINT32 cp = 0x1F600; UINT16 g = 0; DWRITE_GLYPH_IMAGE_FORMATS f = 0;
    if (!face) { t_skip("Segoe UI Emoji not installed"); return; }
    if (FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace4, (void **)&f4))) { DW_RELEASE(face); t_skip("no FontFace4"); return; }
    IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g);
    CHECK(g != 0);
    CHECK_HR_OK(RANGE(f4, g, 0, 0xFFFFFFFFu, &f));
    G2_SOFT(f & (DWRITE_GLYPH_IMAGE_FORMATS_COLR | DWRITE_GLYPH_IMAGE_FORMATS_SVG | DWRITE_GLYPH_IMAGE_FORMATS_PNG));
    DW_RELEASE(f4); DW_RELEASE(face);
}
