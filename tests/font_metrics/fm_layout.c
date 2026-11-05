/* Relation between the font metrics and the line height produced by IDWriteTextLayout (natural vs GDI compatible measuring).
 * Natural measuring: height = ascent + lineGap + descent scaled by size / designUnitsPerEm, baseline = (ascent + lineGap) * scale.
 * GDI compatible measuring: the GDI-compatible (pixel rounded) ascent and descent are used, the line gap is dropped and the result is a whole
 * number of pixels. */
#include "fm.h"
#include <math.h>

typedef struct { dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; IDWriteTextFormat *fmt; } fml_env;
static int env_open(fml_env *E, const ttf_spec *s, float size) {
    IDWriteFactory *f = dw_shared(); size_t n; uint8_t *b; UINT32 k; HRESULT hr;
    memset(E, 0, sizeof *E);
    if (!f) { t_skip("no factory"); return 0; }
    b = ttf_build(s, &n);
    if (!b) { t_skip("ttf_build"); return 0; }
    E->fl = dwx_loader_new(); k = dwx_loader_add(E->fl, b, n); free(b);
    dwx_loader_register(E->fl, f);
    E->cl = dwx_coll_new(E->fl); dwx_coll_add_key(E->cl, k); dwx_coll_register(E->cl, f);
    hr = dwx_coll_create(E->cl, f, &E->coll);
    if (FAILED(hr) || !E->coll) { t_skip("CreateCustomFontCollection: %s", dw_hr_name(hr)); return 1; }
    hr = IDWriteFactory_CreateTextFormat(f, L"DwMetric", E->coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &E->fmt);
    if (FAILED(hr) || !E->fmt) { t_skip("CreateTextFormat: %s", dw_hr_name(hr)); return 1; }
    return 1;
}
static void env_close(fml_env *E) {
    DW_RELEASE(E->fmt); DW_RELEASE(E->coll);
    if (E->cl) { dwx_coll_unregister(E->cl); dwx_release(E->cl); }
    if (E->fl) { dwx_loader_unregister(E->fl); dwx_release(E->fl); }
}
static void spec_named(ttf_spec *s) { ttf_default_spec(s); ttf_set_names(s, "DwMetric", "Regular", "DwMetric", "DwMetric-Regular"); }
#define ENV(E, spec, size) fml_env E; if (!env_open(&E, &(spec), (size))) return; if (!E.fmt) { env_close(&E); return; }

static int line_metrics(IDWriteTextLayout *lay, DWRITE_LINE_METRICS *lm, int cap) {
    UINT32 n = 0; memset(lm, 0, cap * sizeof *lm);
    IDWriteTextLayout_GetLineMetrics(lay, lm, cap, &n);
    return (int)n;
}
static void natural_case(const ttf_spec *s, float size, int total, int baseline_lo, int baseline_hi, const char *label) {
    IDWriteFactory *f = dw_shared(); IDWriteTextLayout *lay = NULL; DWRITE_LINE_METRICS lm[4]; int n;
    ENV(E, *s, size);
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"A", 1, E.fmt, 10000.0f, 10000.0f, &lay), S_OK);
    if (lay) {
        n = line_metrics(lay, lm, 4);
        EXPECT_EQ(n, 1);
        if (n == 1) {
            float scale = size / (float)s->upm;
            t_check(fabsf(lm[0].height - total * scale) <= 0.002f * (1 + total * scale / 100), __FILE__, __LINE__, "line height", "%s: height %g want %g", label, lm[0].height, total * scale);
            t_check(lm[0].baseline >= baseline_lo * scale - 0.002f && lm[0].baseline <= baseline_hi * scale + 0.002f, __FILE__, __LINE__, "baseline", "%s: baseline %g not in [%g, %g]", label, lm[0].baseline, baseline_lo * scale, baseline_hi * scale);
            t_note("%s: height=%g baseline=%g (size %g)", label, lm[0].height, lm[0].baseline, size);
        }
        DW_RELEASE(lay);
    }
    env_close(&E);
}
TEST(fml_natural_line_height_default_font) {
    ttf_spec s; spec_named(&s);
    /* ascent 1800 (win), descent 450, gap 0 */
    natural_case(&s, 2048.0f, 2250, 1800, 1800, "default");
}
TEST(fml_natural_line_height_typo_font) {
    ttf_spec s; spec_named(&s); s.fs_selection |= 0x80;
    /* typo: 1600 + 420 + gap 80 */
    natural_case(&s, 2048.0f, 2100, 1600, 1680, "typo");
}
TEST(fml_natural_line_height_hhea_gap) {
    ttf_spec s; spec_named(&s); s.win_asc = 1700; s.win_desc = 400;
    /* win == hhea: line gap 90 survives */
    natural_case(&s, 2048.0f, 2190, 1700, 1790, "gap90");
}
TEST(fml_natural_line_height_no_gap_zero) {
    ttf_spec s; spec_named(&s); s.hhea_gap = 0; s.win_asc = 1700; s.win_desc = 400;
    natural_case(&s, 2048.0f, 2100, 1700, 1700, "gap0");
}
TEST(fml_natural_line_height_scales_with_size) {
    static const float sizes[] = { 8.0f, 10.5f, 12.0f, 14.0f, 16.0f, 20.0f, 33.3f, 48.0f, 100.0f, 1024.0f };
    unsigned i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        ttf_spec s; spec_named(&s); s.fs_selection |= 0x80; char l[32]; snprintf(l, sizeof l, "typo size %g", sizes[i]);
        natural_case(&s, sizes[i], 2100, 1600, 1680, l);
    }
}
TEST(fml_natural_line_height_upm_1000) {
    ttf_spec s; fm_spec_upm(&s, 1000); ttf_set_names(&s, "DwMetric", "Regular", "DwMetric", "DwMetric-Regular");
    /* scaled metrics: win 878 / 219, gap 0 after clamping */
    natural_case(&s, 1000.0f, 878 + 219, 878, 878, "upm1000");
}
TEST(fml_line_height_independent_of_text) {
    IDWriteFactory *f = dw_shared(); ttf_spec s; spec_named(&s);
    ENV(E, s, 2048.0f);
    static const WCHAR *texts[] = { L"A", L"AAAAAAAAAA", L" ", L"0123456789", L"a b c" };
    unsigned i;
    for (i = 0; i < 5; i++) {
        IDWriteTextLayout *lay = NULL; DWRITE_LINE_METRICS lm[2]; int n;
        CHECK_HR(IDWriteFactory_CreateTextLayout(f, texts[i], (UINT32)wcslen(texts[i]), E.fmt, 100000.0f, 100000.0f, &lay), S_OK);
        if (!lay) continue;
        n = line_metrics(lay, lm, 2);
        EXPECT_EQ(n, 1);
        if (n == 1) CHECKF(fabsf(lm[0].height - 2250.0f) < 0.01f, "text %u: height %g", i, lm[0].height);
        DW_RELEASE(lay);
    }
    env_close(&E);
}
TEST(fml_two_lines_stack) {
    IDWriteFactory *f = dw_shared(); ttf_spec s; spec_named(&s); IDWriteTextLayout *lay = NULL;
    ENV(E, s, 2048.0f);
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"A\nB", 3, E.fmt, 100000.0f, 100000.0f, &lay), S_OK);
    if (lay) {
        DWRITE_LINE_METRICS lm[4]; DWRITE_TEXT_METRICS tm; int n = line_metrics(lay, lm, 4);
        EXPECT_EQ(n, 2);
        if (n == 2) {
            CHECKF(fabsf(lm[0].height - 2250.0f) < 0.01f && fabsf(lm[1].height - 2250.0f) < 0.01f, "heights %g %g", lm[0].height, lm[1].height);
            EXPECT_EQ(lm[0].length, 2); EXPECT_EQ(lm[0].newlineLength, 1); EXPECT_EQ(lm[1].length, 1);
        }
        CHECK_HR(IDWriteTextLayout_GetMetrics(lay, &tm), S_OK);
        CHECKF(fabsf(tm.height - 4500.0f) < 0.02f, "layout height %g", tm.height);
        EXPECT_EQ(tm.lineCount, 2);
        DW_RELEASE(lay);
    }
    env_close(&E);
}
TEST(fml_layout_width_uses_design_advances) {
    IDWriteFactory *f = dw_shared(); ttf_spec s; spec_named(&s); IDWriteTextLayout *lay = NULL;
    ENV(E, s, 2048.0f);
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"ABC", 3, E.fmt, 100000.0f, 100000.0f, &lay), S_OK);
    if (lay) {
        DWRITE_TEXT_METRICS tm;
        CHECK_HR(IDWriteTextLayout_GetMetrics(lay, &tm), S_OK);
        /* at size == upm one design unit is one DIP: three boxes of advance 1000 */
        CHECKF(fabsf(tm.widthIncludingTrailingWhitespace - 3000.0f) < 0.01f, "width %g", tm.widthIncludingTrailingWhitespace);
        DW_RELEASE(lay);
    }
    env_close(&E);
}
TEST(fml_gdi_compatible_layout_line_height_is_whole_pixels) {
    IDWriteFactory *f = dw_shared(); ttf_spec s; spec_named(&s); s.fs_selection |= 0x80;
    static const float sizes[] = { 9.0f, 12.0f, 13.0f, 16.0f, 21.0f, 30.0f };
    unsigned i;
    ENV(E, s, 16.0f);
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *lay = NULL; DWRITE_LINE_METRICS lm[2]; int n;
        if (FAILED(IDWriteFactory_CreateTextFormat(f, L"DwMetric", E.coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, sizes[i], L"en-us", &fmt)) || !fmt) continue;
        CHECK_HR(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"A", 1, fmt, 10000.0f, 10000.0f, 1.0f, NULL, FALSE, &lay), S_OK);
        if (lay) {
            n = line_metrics(lay, lm, 2);
            EXPECT_EQ(n, 1);
            if (n == 1) {
                /* GDI measuring: whole pixel line height, built from the GDI-compatible ascent and descent (no line gap) */
                IDWriteFontFace *face = NULL; IDWriteFontFace1 *f1 = NULL; DWRITE_FONT_METRICS1 g;
                float h = lm[0].height;
                t_check(fabsf(h - floorf(h + 0.5f)) < 0.001f, __FILE__, __LINE__, "whole pixel height", "size %g: %g", sizes[i], h);
                {   /* expected from the face */
                    UINT32 idx = 0; BOOL ex = FALSE; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL;
                    IDWriteFontCollection_FindFamilyName(E.coll, L"DwMetric", &idx, &ex);
                    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(E.coll, idx, &fam)) && fam) {
                        IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
                        if (font) IDWriteFont_CreateFontFace(font, &face);
                        DW_RELEASE(font); DW_RELEASE(fam);
                    }
                }
                if (face && (f1 = fm_f1(face)) != NULL) {
                    if (SUCCEEDED(IDWriteFontFace1_GetGdiCompatibleMetrics(f1, sizes[i], 1.0f, NULL, &g))) {
                        float a = floorf((float)g.ascent * sizes[i] / g.designUnitsPerEm + 0.5f), d = floorf((float)g.descent * sizes[i] / g.designUnitsPerEm + 0.5f);
                        t_check(fabsf(h - (a + d)) < 0.001f, __FILE__, __LINE__, "GDI cell height", "size %g: layout %g vs %g + %g", sizes[i], h, a, d);
                    }
                    DW_RELEASE(f1);
                }
                DW_RELEASE(face);
            }
            DW_RELEASE(lay);
        }
        DW_RELEASE(fmt);
    }
    env_close(&E);
}
TEST(fml_gdi_natural_layout_has_same_height_as_classic_up_to_rounding) {
    IDWriteFactory *f = dw_shared(); ttf_spec s; spec_named(&s);
    ENV(E, s, 20.0f);
    {
        IDWriteTextLayout *a = NULL, *b = NULL; DWRITE_LINE_METRICS la[2], lb[2];
        CHECK_HR(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"A", 1, E.fmt, 10000.0f, 10000.0f, 1.0f, NULL, FALSE, &a), S_OK);
        CHECK_HR(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"A", 1, E.fmt, 10000.0f, 10000.0f, 1.0f, NULL, TRUE, &b), S_OK);
        if (a && b && line_metrics(a, la, 2) == 1 && line_metrics(b, lb, 2) == 1) {
            t_note("gdi classic height %g, natural %g", la[0].height, lb[0].height);
            CHECKF(fabsf(la[0].height - lb[0].height) < 0.001f, "classic %g vs natural %g", la[0].height, lb[0].height);
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    env_close(&E);
}
TEST(fml_arial_natural_line_height_matches_face_metrics) {
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face; IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *lay = NULL; DWRITE_FONT_METRICS m;
    if (!f) { t_skip("no factory"); return; }
    face = fm_sys(L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Arial missing"); return; }
    IDWriteFontFace_GetMetrics(face, &m);
    if (SUCCEEDED(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &fmt)) && fmt &&
        SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"Hxg", 3, fmt, 10000.0f, 10000.0f, &lay)) && lay) {
        DWRITE_LINE_METRICS lm[2];
        if (line_metrics(lay, lm, 2) == 1) {
            float want = (float)(m.ascent + m.descent + m.lineGap) * 20.0f / m.designUnitsPerEm;
            CHECKF(fabsf(lm[0].height - want) < 0.01f, "Arial 20px: height %g want %g", lm[0].height, want);
            CHECKF(fabsf(lm[0].baseline - (float)(m.ascent + m.lineGap) * 20.0f / m.designUnitsPerEm) < 0.01f || fabsf(lm[0].baseline - (float)m.ascent * 20.0f / m.designUnitsPerEm) < 0.01f,
                   "Arial baseline %g", lm[0].baseline);
            t_note("Arial 20px: height %g baseline %g (face: %d+%d+%d / %d)", lm[0].height, lm[0].baseline, m.ascent, m.descent, m.lineGap, m.designUnitsPerEm);
        }
    }
    DW_RELEASE(lay); DW_RELEASE(fmt); DW_RELEASE(face);
}
