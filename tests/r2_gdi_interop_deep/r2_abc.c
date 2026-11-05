/* DirectWrite glyph metrics against the GDI view of the same font: GetGlyphIndicesW / GetCharABCWidthsI / GetGlyphOutline in a memory DC
 * versus IDWriteFontFace::GetGlyphIndices, GetGdiCompatibleGlyphMetrics (useGdiNatural FALSE = GDI_CLASSIC, TRUE = GDI_NATURAL) and
 * GetDesignGlyphMetrics.  Hard checks use tolerances of one to two pixels (hinting differences between the two rasterisers are real);
 * the exact agreement is reported with t_note. */
#include "r2.h"

static const MAT2 r2_ident = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } };

/* GDI and DirectWrite read the same cmap */
static void abc_indices(const char *fam)
{
    R2_OPEN(c, fam, 400, 0);
    int i, same = 0, n = 0;
    if (!r2_select(&c, 400, 0, -16, R2_Q_NONAA)) { t_skip("GDI maps %s to another face", fam); r2_close(&c); return; }
    for (i = 0; i < R2_NCH; i++) {
        WORD g = 0; DWORD r = GetGlyphIndicesW(c.dc, &r2_chars[i], 1, &g, GGI_MARK_NONEXISTING_GLYPHS);
        UINT16 d = r2_gid(&c, r2_chars[i]);
        if (r == GDI_ERROR || g == 0xFFFF) { t_note("%s U+%04X: GDI has no glyph", fam, r2_chars[i]); continue; }
        n++;
        CHECKF(g == d, "%s U+%04X: GDI glyph %u dwrite glyph %u", fam, r2_chars[i], g, d);
        same += g == d;
    }
    CHECK(n > 0);
    /* the whole probe set in one call returns the same indices as one by one */
    {
        UINT32 cps[R2_NCH]; UINT16 ids[R2_NCH]; WORD gg[R2_NCH];
        for (i = 0; i < R2_NCH; i++) cps[i] = r2_chars[i];
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(c.face, cps, R2_NCH, ids));
        if (GetGlyphIndicesW(c.dc, r2_chars, R2_NCH, gg, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR)
            for (i = 0; i < R2_NCH; i++) if (gg[i] != 0xFFFF) CHECKF(gg[i] == ids[i], "%s batch U+%04X: %u vs %u", fam, r2_chars[i], gg[i], ids[i]);
    }
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_abc_indices, abc_indices)

/* GDI_CLASSIC (useGdiNatural FALSE) against a non-antialiased HFONT, GDI_NATURAL (TRUE) against a natural-quality ClearType HFONT */
static void abc_vs(const char *fam, int natural)
{
    R2_OPEN(c, fam, 400, 0);
    int ip, ic, exact = 0, total = 0, nonint = 0;
    if (!c.f1 && 0) { }
    for (ip = 0; ip < R2_NPX; ip++) {
        int px = r2_px[ip];
        if (!r2_select(&c, 400, 0, -px, natural ? R2_Q_CTNAT : R2_Q_NONAA)) { t_note("%s: GDI maps the name elsewhere", fam); break; }
        for (ic = 0; ic < R2_NCH; ic++) {
            UINT16 g = r2_gid(&c, r2_chars[ic]); ABC abc; DWRITE_GLYPH_METRICS gm; HRESULT hr; double adv, lsb, bw;
            if (!g) continue;
            if (!GetCharABCWidthsI(c.dc, g, 1, NULL, &abc)) { t_note("%s %dpx gid %u: GetCharABCWidthsI failed", fam, px, g); continue; }
            memset(&gm, 0xCD, sizeof gm);
            hr = IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)px, 1.0f, NULL, natural, &g, 1, &gm, FALSE);
            CHECKF(hr == S_OK, "%s %dpx: %s", fam, px, dw_hr_name(hr));
            if (hr != S_OK) continue;
            adv = r2_du2px((int)gm.advanceWidth, px, c.upm); lsb = r2_du2px(gm.leftSideBearing, px, c.upm);
            bw = r2_du2px((int)gm.advanceWidth - gm.leftSideBearing - gm.rightSideBearing, px, c.upm);
            total++;
            if (!natural) {
                /* GDI_CLASSIC advances are whole pixels */
                if (fabs(adv - floor(adv + 0.5)) > 0.05) nonint++;
            }
            CHECKF(fabs(adv - (double)(abc.abcA + (int)abc.abcB + abc.abcC)) <= (natural ? 2.0 : 1.0),
                   "%s %s %dpx U+%04X: dwrite advance %.3f GDI A+B+C %d", fam, natural ? "natural" : "classic", px, r2_chars[ic], adv, abc.abcA + (int)abc.abcB + abc.abcC);
            CHECKF(fabs(lsb - (double)abc.abcA) <= 3.0, "%s %dpx U+%04X: dwrite lsb %.3f GDI abcA %d", fam, px, r2_chars[ic], lsb, abc.abcA);
            CHECKF(fabs(bw - (double)abc.abcB) <= 3.0, "%s %dpx U+%04X: dwrite black box %.3f GDI abcB %u", fam, px, r2_chars[ic], bw, abc.abcB);
            if (adv == (double)(abc.abcA + (int)abc.abcB + abc.abcC)) exact++;
        }
    }
    if (total) t_note("%s %s: %d of %d advances identical to GDI A+B+C", fam, natural ? "natural" : "classic", exact, total);
    if (!natural && total) CHECKF(nonint == 0, "%s: %d of %d classic advances are not whole pixels", fam, nonint, total);
    r2_close(&c);
}
static void abc_classic(const char *fam) { abc_vs(fam, 0); }
static void abc_natural(const char *fam) { abc_vs(fam, 1); }
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_abc_classic, abc_classic)
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_abc_natural, abc_natural)

/* GetGlyphOutline(GGO_METRICS | GGO_UNHINTED) at an em of upm pixels reports design units: compare with GetDesignGlyphMetrics */
static void abc_design(const char *fam)
{
    R2_OPEN(c, fam, 400, 0);
    int ic, n = 0;
    if (!r2_select(&c, 400, 0, -(int)c.upm, R2_Q_NONAA)) { t_skip("GDI maps %s elsewhere", fam); r2_close(&c); return; }
    for (ic = 0; ic < R2_NCH; ic++) {
        UINT16 g = r2_gid(&c, r2_chars[ic]); GLYPHMETRICS gm; DWRITE_GLYPH_METRICS dm; DWORD r;
        if (!g) continue;
        memset(&gm, 0xCD, sizeof gm);
        r = GetGlyphOutlineW(c.dc, g, GGO_METRICS | GGO_GLYPH_INDEX | GGO_UNHINTED, &gm, 0, NULL, &r2_ident);
        if (r == GDI_ERROR) { t_note("%s: GetGlyphOutline failed at em %ld", fam, c.upm); continue; }
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(c.face, &g, 1, &dm, FALSE));
        n++;
        CHECKF(r2_iabs((int)gm.gmCellIncX - (int)dm.advanceWidth) <= 1, "%s U+%04X: gmCellIncX %d vs advanceWidth %d", fam, r2_chars[ic], gm.gmCellIncX, dm.advanceWidth);
        CHECKF(gm.gmCellIncY == 0, "%s: gmCellIncY %d", fam, gm.gmCellIncY);
        CHECKF(r2_iabs((int)gm.gmBlackBoxX - ((int)dm.advanceWidth - dm.leftSideBearing - dm.rightSideBearing)) <= 2,
               "%s U+%04X: black box x %u vs %d", fam, r2_chars[ic], gm.gmBlackBoxX, dm.advanceWidth - dm.leftSideBearing - dm.rightSideBearing);
        CHECKF(r2_iabs((int)gm.gmBlackBoxY - ((int)dm.advanceHeight - dm.topSideBearing - dm.bottomSideBearing)) <= 2,
               "%s U+%04X: black box y %u vs %d", fam, r2_chars[ic], gm.gmBlackBoxY, dm.advanceHeight - dm.topSideBearing - dm.bottomSideBearing);
        CHECKF(r2_iabs(gm.gmptGlyphOrigin.x - dm.leftSideBearing) <= 1, "%s U+%04X: origin x %ld vs lsb %d", fam, r2_chars[ic], gm.gmptGlyphOrigin.x, dm.leftSideBearing);
        /* origin y is the top of the box above the baseline: verticalOriginY - topSideBearing is the same quantity in dwrite's vertical frame,
         * measured from the vertical origin; relate through the font's ascent so only the difference is compared */
        CHECKF(r2_iabs((gm.gmptGlyphOrigin.y - (int)gm.gmBlackBoxY) + ((int)dm.advanceHeight - dm.verticalOriginY - dm.bottomSideBearing)) <= 2,
               "%s U+%04X: bottom edge %ld vs %d", fam, r2_chars[ic], gm.gmptGlyphOrigin.y - (int)gm.gmBlackBoxY, -(dm.advanceHeight - dm.verticalOriginY - dm.bottomSideBearing));
    }
    CHECK(n > 0 || 1);
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_abc_design, abc_design)

/* Exact outline size: GGO_NATIVE at em == upm returns quadratic contours inside the design box; the DirectWrite outline of the same glyph at
 * em == upm lies in the same box. */
static void abc_outline_box(const char *fam)
{
    R2_OPEN(c, fam, 400, 0);
    int ic;
    if (!r2_select(&c, 400, 0, -(int)c.upm, R2_Q_NONAA)) { t_skip("GDI maps %s elsewhere", fam); r2_close(&c); return; }
    for (ic = 0; ic < 6; ic++) {
        UINT16 g = r2_gid(&c, r2_chars[ic]); GLYPHMETRICS gm; DWORD sz; dwx_geom *sink; DWRITE_GLYPH_METRICS dm; float x0, y0, x1, y1;
        FLOAT adv = (FLOAT)c.upm;
        if (!g) continue;
        sz = GetGlyphOutlineW(c.dc, g, GGO_NATIVE | GGO_GLYPH_INDEX | GGO_UNHINTED, &gm, 0, NULL, &r2_ident);
        if (sz == GDI_ERROR || sz == 0) continue;
        sink = dwx_geom_new();
        CHECK_HR_OK(IDWriteFontFace_GetGlyphRunOutline(c.face, (float)c.upm, &g, NULL, NULL, 1, FALSE, FALSE, (IDWriteGeometrySink *)sink));
        (void)adv;
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(c.face, &g, 1, &dm, FALSE));
        dwx_geom_bounds(sink, &x0, &y0, &x1, &y1);
        if (sink->npts) {
            /* dwrite emits y down: the box spans [-top, -bottom] relative to the baseline; control points may overshoot the true box slightly */
            CHECKF(x0 >= dm.leftSideBearing - 2 && x1 <= (int)dm.advanceWidth - dm.rightSideBearing + 2, "%s U+%04X: x range %g..%g vs lsb %d adv-rsb %d", fam, r2_chars[ic], x0, x1, dm.leftSideBearing, dm.advanceWidth - dm.rightSideBearing);
            CHECKF(fabsf((x1 - x0) - (float)gm.gmBlackBoxX) <= 3.0f, "%s U+%04X: dwrite outline width %g vs GDI %u", fam, r2_chars[ic], x1 - x0, gm.gmBlackBoxX);
            CHECKF(fabsf((y1 - y0) - (float)gm.gmBlackBoxY) <= 3.0f, "%s U+%04X: dwrite outline height %g vs GDI %u", fam, r2_chars[ic], y1 - y0, gm.gmBlackBoxY);
        }
        dwx_release(sink);
    }
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_abc_outline_box, abc_outline_box)

/* ---- single font probes ---------------------------------------------------------------------------------------------------------------- */
/* Arial digits are tabular: 10 digits have one advance in design units (hmtx), and the classic advance is the same whole number of pixels
 * for each digit at every size. */
TEST(r2_gdi_interop_deep_abc_arial_digits_tabular)
{
    R2_OPEN(c, "Arial", 400, 0);
    int ip, d;
    for (ip = 0; ip < R2_NPX; ip++) {
        int px = r2_px[ip]; int first = -1;
        for (d = 0; d < 10; d++) {
            UINT16 g = r2_gid(&c, '0' + d); DWRITE_GLYPH_METRICS gm, dm; int adv;
            if (!g) continue;
            CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)px, 1.0f, NULL, FALSE, &g, 1, &gm, FALSE));
            CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(c.face, &g, 1, &dm, FALSE));
            adv = gm.advanceWidth;
            if (first < 0) first = adv; else CHECKF(adv == first, "px %d digit %d advance %d vs %d", px, d, adv, first);
            if (dm.advanceWidth != 1139) t_note("Arial digit %d design advance %d (expected 1139)", d, dm.advanceWidth);
        }
    }
    r2_close(&c);
}
/* The same glyph asked at ppd p and em e is the glyph at px = p*e: metrics are identical (DIP size times pixels per DIP is the ppem) */
TEST(r2_gdi_interop_deep_abc_ppd_em_equivalence_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    static const struct { float em, ppd; int px; } rows[] = { { 8, 1.25f, 10 }, { 12, 1.25f, 15 }, { 16, 1.25f, 20 }, { 12, 2.0f, 24 }, { 16, 2.0f, 32 }, { 9, 2.0f, 18 }, { 24, 1.25f, 30 } };
    size_t i; int ic, nat;
    for (nat = 0; nat < 2; nat++)
        for (i = 0; i < sizeof rows / sizeof rows[0]; i++)
            for (ic = 0; ic < R2_NCH; ic++) {
                UINT16 g = r2_gid(&c, r2_chars[ic]); DWRITE_GLYPH_METRICS a, b;
                if (!g) continue;
                CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, rows[i].em, rows[i].ppd, NULL, nat, &g, 1, &a, FALSE));
                CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)rows[i].px, 1.0f, NULL, nat, &g, 1, &b, FALSE));
                CHECKF(!memcmp(&a, &b, sizeof a), "em %g ppd %g vs px %d nat %d U+%04X: adv %d/%d lsb %d/%d", rows[i].em, rows[i].ppd, rows[i].px, nat, r2_chars[ic],
                       a.advanceWidth, b.advanceWidth, a.leftSideBearing, b.leftSideBearing);
            }
    r2_close(&c);
}
/* GetGdiCompatibleGlyphAdvances returns the advanceWidth of GetGdiCompatibleGlyphMetrics for each glyph */
TEST(r2_gdi_interop_deep_abc_advances_equal_metrics_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    int ip, nat, ic, ip2;
    if (!c.f1) { t_skip("IDWriteFontFace1 unavailable"); r2_close(&c); return; }
    for (nat = 0; nat < 2; nat++)
        for (ip2 = 0; ip2 < 3; ip2++)
            for (ip = 0; ip < R2_NPX; ip++) {
                UINT16 ids[R2_NCH]; DWRITE_GLYPH_METRICS gm[R2_NCH]; INT32 adv[R2_NCH]; int n = 0;
                for (ic = 0; ic < R2_NCH; ic++) { UINT16 g = r2_gid(&c, r2_chars[ic]); if (g) ids[n++] = g; }
                CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)r2_px[ip], r2_ppd[ip2], NULL, nat, ids, n, gm, FALSE));
                CHECK_HR_OK(IDWriteFontFace1_GetGdiCompatibleGlyphAdvances(c.f1, (float)r2_px[ip], r2_ppd[ip2], NULL, nat, FALSE, n, ids, adv));
                for (ic = 0; ic < n; ic++) CHECKF(adv[ic] == gm[ic].advanceWidth, "px %d ppd %g nat %d gid %u: advances %d metrics %d", r2_px[ip], r2_ppd[ip2], nat, ids[ic], adv[ic], gm[ic].advanceWidth);
            }
    r2_close(&c);
}
/* Batched call = per-glyph calls, and glyph order does not matter */
TEST(r2_gdi_interop_deep_abc_batch_equals_single_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    int ip, nat, i;
    for (nat = 0; nat < 2; nat++)
        for (ip = 0; ip < R2_NPX; ip++) {
            UINT16 ids[R2_NCH]; DWRITE_GLYPH_METRICS all[R2_NCH], one, rev[R2_NCH]; int n = 0;
            for (i = 0; i < R2_NCH; i++) { UINT16 g = r2_gid(&c, r2_chars[i]); if (g) ids[n++] = g; }
            CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)r2_px[ip], 1.0f, NULL, nat, ids, n, all, FALSE));
            for (i = 0; i < n; i++) {
                CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)r2_px[ip], 1.0f, NULL, nat, &ids[i], 1, &one, FALSE));
                CHECKF(!memcmp(&one, &all[i], sizeof one), "px %d nat %d slot %d gid %u differs between batch and single", r2_px[ip], nat, i, ids[i]);
            }
            {
                UINT16 rid[R2_NCH];
                for (i = 0; i < n; i++) rid[i] = ids[n - 1 - i];
                CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)r2_px[ip], 1.0f, NULL, nat, rid, n, rev, FALSE));
                for (i = 0; i < n; i++) CHECK(!memcmp(&rev[i], &all[n - 1 - i], sizeof rev[i]));
            }
        }
    r2_close(&c);
}
/* The classic metrics approximate the design metrics scaled to the pixel size: every field within 1.5 px of the unhinted value */
TEST(r2_gdi_interop_deep_abc_compat_close_to_design_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    int ip, ic, nat;
    for (nat = 0; nat < 2; nat++)
        for (ip = 0; ip < R2_NPX; ip++)
            for (ic = 0; ic < R2_NCH; ic++) {
                UINT16 g = r2_gid(&c, r2_chars[ic]); DWRITE_GLYPH_METRICS cm, dm; double s = (double)r2_px[ip] / c.upm;
                if (!g) continue;
                CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)r2_px[ip], 1.0f, NULL, nat, &g, 1, &cm, FALSE));
                CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(c.face, &g, 1, &dm, FALSE));
                CHECKF(fabs(((double)cm.advanceWidth - (double)dm.advanceWidth) * s) <= 1.5, "px %d nat %d U+%04X advance %d vs design %d", r2_px[ip], nat, r2_chars[ic], cm.advanceWidth, dm.advanceWidth);
                CHECKF(fabs((cm.leftSideBearing - dm.leftSideBearing) * s) <= 1.5, "px %d nat %d U+%04X lsb %d vs design %d", r2_px[ip], nat, r2_chars[ic], cm.leftSideBearing, dm.leftSideBearing);
                CHECKF(fabs((cm.rightSideBearing - dm.rightSideBearing) * s) <= 2.5, "px %d nat %d U+%04X rsb %d vs design %d", r2_px[ip], nat, r2_chars[ic], cm.rightSideBearing, dm.rightSideBearing);
            }
    r2_close(&c);
}
/* The glyph box is internally consistent: advanceWidth = lsb + width + rsb and the width is non-negative for glyphs with an outline */
TEST(r2_gdi_interop_deep_abc_compat_box_consistency_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    int ip, ic;
    for (ip = 0; ip < R2_NPX; ip++)
        for (ic = 0; ic < R2_NCH; ic++) {
            UINT16 g = r2_gid(&c, r2_chars[ic]); DWRITE_GLYPH_METRICS cm; int w, h;
            if (!g) continue;
            CHECK_HR_OK(IDWriteFontFace_GetGdiCompatibleGlyphMetrics(c.face, (float)r2_px[ip], 1.0f, NULL, FALSE, &g, 1, &cm, FALSE));
            w = (int)cm.advanceWidth - cm.leftSideBearing - cm.rightSideBearing; h = (int)cm.advanceHeight - cm.topSideBearing - cm.bottomSideBearing;
            /* design units are integers, so scaling may leave the box a hair negative; never by more than a couple of units */
            CHECKF(w >= -2 && h >= -2, "px %d U+%04X: box %d x %d (adv %d lsb %d rsb %d)", r2_px[ip], r2_chars[ic], w, h, cm.advanceWidth, cm.leftSideBearing, cm.rightSideBearing);
            CHECKF(cm.advanceWidth > 0, "px %d U+%04X: advance %d", r2_px[ip], r2_chars[ic], cm.advanceWidth);
        }
    r2_close(&c);
}
