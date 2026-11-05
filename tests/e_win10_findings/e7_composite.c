/* TrueType composite glyphs on Windows 10: a component that is itself a composite is NOT resolved. The whole glyph
 * outlines as empty (only SetFillMode, no figure), including its plain sibling component. A one-level composite works.
 * Probed with synthetic glyphs: 69 = composite{ gid66 (itself box+triangle composite), box@(1000,0) },
 * 70 = composite{ box, box@(1000,0) } (control, one level), 71 = composite{ 70 } (nested, single component), 72 = composite{ 2 } (single plain). */
#include "e7.h"

enum { G_NEST = 69, G_FLAT2 = 70, G_NEST1 = 71, G_ONE = 72, G_COUNT = 73 };
static ttf_glyph glyphs[G_COUNT];
static const ttf_comp c_nest[2]  = { { TTF_GID_COMPOSITE, 0, 0, 0, 0 }, { 2, 1000, 0, 0, 0 } };
static const ttf_comp c_flat2[2] = { { 2, 0, 0, 0, 0 }, { 2, 1000, 0, 0, 0 } };
static const ttf_comp c_nest1[1] = { { G_FLAT2, 0, 0, 0, 0 } };
static const ttf_comp c_one[1]   = { { 2, 0, 0, 0, 0 } };

static int open_font(dwx_font *h)
{
    ttf_spec s; int n, i; const ttf_glyph *d = ttf_default_glyphs(&n); size_t sz; uint8_t *b; HRESULT hr;
    memset(h, 0, sizeof *h);
    if (!dw_shared()) return 0;
    ttf_default_spec(&s);
    memset(glyphs, 0, sizeof glyphs);
    for (i = 0; i < n && i < TTF_DEF_NGLYPHS; i++) glyphs[i] = d[i];
    glyphs[G_NEST].adv = 2000;  glyphs[G_NEST].ncomp = 2;  glyphs[G_NEST].comps = c_nest;
    glyphs[G_FLAT2].adv = 2000; glyphs[G_FLAT2].ncomp = 2; glyphs[G_FLAT2].comps = c_flat2;
    glyphs[G_NEST1].adv = 2000; glyphs[G_NEST1].ncomp = 1; glyphs[G_NEST1].comps = c_nest1;
    glyphs[G_ONE].adv = 1000;   glyphs[G_ONE].ncomp = 1;   glyphs[G_ONE].comps = c_one;
    s.glyphs = glyphs; s.nglyphs = G_COUNT;
    b = ttf_build(&s, &sz);
    if (!b) return 0;
    hr = dwx_font_open(h, dw_shared(), b, sz, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(b);
    return SUCCEEDED(hr) && h->face;
}
static dwx_geom *outline(IDWriteFontFace *face, UINT16 gid)
{
    dwx_geom *g = dwx_geom_new(); UINT16 gi = gid; FLOAT adv = 0;
    HRESULT hr = IDWriteFontFace_GetGlyphRunOutline(face, 2048.0f, &gi, &adv, NULL, 1, FALSE, FALSE, (IDWriteGeometrySink *)g);
    CHECK_HR(hr, S_OK);
    return g;
}

TEST(e7_composite_nested_outlines_empty)
{
    dwx_font h; dwx_geom *g;
    if (!open_font(&h)) { dwx_font_close(&h); t_skip("font"); return; }
    g = outline(h.face, G_NEST);
    EXPECT_EQ(g->nfigures_begun, 0); EXPECT_EQ(g->nev, 1); EXPECT_EQ(g->ev[0].kind, DWX_G_FILL);
    dwx_release(g);
    g = outline(h.face, G_NEST1);   /* composite whose only component is a composite */
    EXPECT_EQ(g->nfigures_begun, 0); EXPECT_EQ(g->nev, 1); EXPECT_EQ(g->ev[0].kind, DWX_G_FILL);
    dwx_release(g);
    dwx_font_close(&h);
}
TEST(e7_composite_nested_keeps_metrics)
{
    dwx_font h; DWRITE_GLYPH_METRICS gm[2]; UINT16 ids[2] = { G_NEST, G_FLAT2 };
    if (!open_font(&h)) { dwx_font_close(&h); t_skip("font"); return; }
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(h.face, ids, 2, gm, FALSE), S_OK);
    EXPECT_EQ(gm[0].advanceWidth, 2000); EXPECT_EQ(gm[1].advanceWidth, 2000);
    dwx_font_close(&h);
}
/* negative control: one level of composition outlines normally */
TEST(e7_composite_negative_control_flat_composites_draw)
{
    dwx_font h; dwx_geom *g;
    if (!open_font(&h)) { dwx_font_close(&h); t_skip("font"); return; }
    g = outline(h.face, G_FLAT2);
    EXPECT_EQ(g->nfigures_begun, 2); EXPECT_EQ(g->nfigures_ended, 2);
    dwx_release(g);
    g = outline(h.face, TTF_GID_COMPOSITE);   /* box + triangle */
    EXPECT_EQ(g->nfigures_begun, 2);
    dwx_release(g);
    g = outline(h.face, G_ONE);
    EXPECT_EQ(g->nfigures_begun, 1);
    dwx_release(g);
    dwx_font_close(&h);
}
