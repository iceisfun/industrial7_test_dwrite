/* Additional invariants: V0 vs V1 prefix, idempotence, em-size scaling monotonicity. */
#include "fm.h"

TEST(fme_metrics_v0_is_prefix_of_v1) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS m0; DWRITE_FONT_METRICS1 m1;
    memset(&m0, 0xCD, sizeof m0); memset(&m1, 0xCD, sizeof m1);
    IDWriteFontFace_GetMetrics(F.face, &m0);
    if (F.f1) {
        IDWriteFontFace1_GetMetrics(F.f1, &m1);
        /* DWRITE_FONT_METRICS1 begins with the ten DWRITE_FONT_METRICS fields in the same layout */
        CHECKF(!memcmp(&m0, &m1, sizeof m0), "V0 differs from V1 prefix");
    }
    EXPECT_EQ(sizeof(DWRITE_FONT_METRICS), 20);
    EXPECT_EQ(sizeof(DWRITE_FONT_METRICS1), 48);
    fm_close(&F);
}
TEST(fme_get_metrics_is_idempotent) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS a, b;
    IDWriteFontFace_GetMetrics(F.face, &a);
    IDWriteFontFace_GetMetrics(F.face, &b);
    CHECKF(!memcmp(&a, &b, sizeof a), "second GetMetrics differs");
    fm_close(&F);
}
TEST(fme_gdi_metrics_idempotent_and_does_not_change_face_metrics) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS before, after, g1, g2;
    IDWriteFontFace_GetMetrics(F.face, &before);
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleMetrics(F.face, 13.0f, 1.25f, NULL, &g1), S_OK);
    CHECK_HR(IDWriteFontFace_GetGdiCompatibleMetrics(F.face, 13.0f, 1.25f, NULL, &g2), S_OK);
    IDWriteFontFace_GetMetrics(F.face, &after);
    CHECKF(!memcmp(&g1, &g2, sizeof g1), "GDI metrics not repeatable");
    CHECKF(!memcmp(&before, &after, sizeof before), "face metrics changed by GDI call");
    EXPECT_EQ(g1.designUnitsPerEm, before.designUnitsPerEm);
    fm_close(&F);
}
TEST(fme_gdi_metrics_ascent_monotonic_in_size) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    int px; unsigned prev = 0;
    for (px = 6; px <= 96; px++) {
        DWRITE_FONT_METRICS g;
        if (FAILED(IDWriteFontFace_GetGdiCompatibleMetrics(F.face, (float)px, 1.0f, NULL, &g))) { CHECKF(0, "em %d failed", px); break; }
        /* ascent in design units on a pixel grid is round(asc*px/upm)*upm/px: within one pixel of the unrounded value */
        {
            DWRITE_FONT_METRICS b; double lim; IDWriteFontFace_GetMetrics(F.face, &b);
            lim = (double)b.designUnitsPerEm / px;
            CHECKF(fabs((double)g.ascent - b.ascent) <= lim, "em %d: ascent %u vs %u", px, g.ascent, b.ascent);
            CHECKF(fabs((double)g.descent - b.descent) <= lim, "em %d: descent %u vs %u", px, g.descent, b.descent);
        }
        (void)prev;
    }
    fm_close(&F);
}
TEST(fme_design_glyph_metrics_zero_count_ok) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    UINT16 id = 2; DWRITE_GLYPH_METRICS g;
    /* a zero count with non-NULL pointers walks no glyph */
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, &id, 0, &g, FALSE), S_OK);
    fm_close(&F);
}
TEST(fme_oblique_and_bold_do_not_change_upm) {
    ttf_spec s; ttf_default_spec(&s);
    DWRITE_FONT_SIMULATIONS sims[] = { 0, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE };
    unsigned i; int base = -1;
    for (i = 0; i < 4; i++) {
        fm_font F; DWRITE_FONT_METRICS m; memset(&F, 0, sizeof F);
        if (!fm_open(&F, &s, sims[i])) continue;
        IDWriteFontFace_GetMetrics(F.face, &m);
        EXPECT_EQ(IDWriteFontFace_GetSimulations(F.face), sims[i]);
        if (base < 0) base = m.designUnitsPerEm;
        EXPECT_EQ(m.designUnitsPerEm, base);
        fm_close(&F);
    }
}
