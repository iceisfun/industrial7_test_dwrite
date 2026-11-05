/* GetRecommendedRenderingMode and other DirectWrite services as a function of the system smoothing state.
 *
 * Behaviour (see cts.h): with DWRITE_MEASURING_MODE_GDI_CLASSIC and a rendering-params object whose mode is DEFAULT (or a NULL
 * object), the result is
 *     smoothing off (SPI 0x4A == 0)           -> ALIASED
 *     smoothing on, type STANDARD (1)         -> ALIASED or GDI_CLASSIC (depends on the font size/gasp)
 *     smoothing on, any other type (ClearType) -> GDI_CLASSIC
 * where a non-NULL object answers with the state captured when the object was created and a NULL object asks the system at call
 * time.  NATURAL / GDI_NATURAL measuring modes do not look at the smoothing state; an explicit mode stored in the params object
 * wins; font sizes of 100 ppem and more select OUTLINE for the default outline threshold.
 */
#include "cts.h"

typedef struct { dwx_font h; IDWriteFontFace *face; } fx;
static int fx_open(fx *x) {
    memset(x, 0, sizeof *x);
    if (!dw_shared() || FAILED(dwx_font_open_default(&x->h, dw_shared())) || !x->h.face) { dwx_font_close(&x->h); return 0; }
    x->face = x->h.face; return 1;
}
#define FX_OR_SKIP(x) do { if (!fx_open(&(x))) { cts_end(); t_skip("synthetic font unavailable"); return; } } while (0)

enum { ST_OFF, ST_CT, ST_STD, ST_N };
static const char *st_name[] = { "smoothing-off", "cleartype", "standard" };
static int st_apply(int st) {
    if (st == ST_OFF) return cts_set_smoothing(FALSE);
    if (!cts_set_smoothing(TRUE)) return 0;
    return cts_set_type(st == ST_CT ? CTS_FE_CLEARTYPE : CTS_FE_STANDARD);
}
static int st_ok_modes(int st, int mode) {
    if (st == ST_OFF) return mode == CTS_MODE_ALIASED;
    if (st == ST_CT) return mode == CTS_MODE_GDI_CLASSIC;
    return mode == CTS_MODE_ALIASED || mode == CTS_MODE_GDI_CLASSIC;
}
static int rec_mode(fx *x, DWRITE_MEASURING_MODE mm, IDWriteRenderingParams *p, float em) {
    int m; HRESULT hr = cts_recommend(x->face, em, mm, p, &m);
    CHECKF(SUCCEEDED(hr), "GetRecommendedRenderingMode -> %s", dw_hr_name(hr));
    return m;
}

TEST(rec_live_null_params_each_state) {
    fx x; int st;
    CTS_BEGIN(); FX_OR_SKIP(x);
    for (st = 0; st < ST_N; st++) {
        int m;
        CTS_SET_OR_SKIP(st_apply(st));
        m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 12.0f);
        CHECKF(st_ok_modes(st, m), "%s: GDI_CLASSIC recommended mode %d", st_name[st], m);
    }
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_smoothing_off_is_aliased_exactly) {
    fx x; int m;
    CTS_BEGIN(); FX_OR_SKIP(x);
    CTS_SET_OR_SKIP(st_apply(ST_OFF));
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 12.0f);
    EXPECT_EQ(m, CTS_MODE_ALIASED);
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 20.0f);
    EXPECT_EQ(m, CTS_MODE_ALIASED);
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_cleartype_is_gdi_classic_exactly) {
    fx x; int m;
    CTS_BEGIN(); FX_OR_SKIP(x);
    CTS_SET_OR_SKIP(st_apply(ST_CT));
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 12.0f);
    EXPECT_EQ(m, CTS_MODE_GDI_CLASSIC);
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 30.0f);
    EXPECT_EQ(m, CTS_MODE_GDI_CLASSIC);
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_standard_smoothing_is_aliased_or_classic) {
    fx x; int m, em;
    CTS_BEGIN(); FX_OR_SKIP(x);
    CTS_SET_OR_SKIP(st_apply(ST_STD));
    for (em = 6; em <= 40; em += 2) {
        m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, (float)em);
        CHECKF(m == CTS_MODE_ALIASED || m == CTS_MODE_GDI_CLASSIC, "em %d mode %d", em, m);
    }
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_system_params_snapshot_matrix) {
    /* params created in state A answer as A even after the system moved to B; a NULL object answers as B */
    fx x; int a, b;
    CTS_BEGIN(); FX_OR_SKIP(x);
    for (a = 0; a < ST_N; a++) for (b = 0; b < ST_N; b++) {
        IDWriteRenderingParams *p = NULL; int ms, ml;
        CTS_SET_OR_SKIP(st_apply(a));
        if (FAILED(IDWriteFactory_CreateRenderingParams(dw_shared(), &p)) || !p) continue;
        CTS_SET_OR_SKIP(st_apply(b));
        ms = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, 12.0f);
        ml = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 12.0f);
        CHECKF(st_ok_modes(a, ms), "params created in %s, system now %s: snapshot answer %d", st_name[a], st_name[b], ms);
        CHECKF(st_ok_modes(b, ml), "system now %s: live answer %d", st_name[b], ml);
        IDWriteRenderingParams_Release(p);
    }
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_off_snapshot_stays_aliased_after_enable) {
    fx x; IDWriteRenderingParams *p = NULL; int m, live;
    CTS_BEGIN(); FX_OR_SKIP(x);
    CTS_SET_OR_SKIP(st_apply(ST_OFF));
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(dw_shared(), &p));
    CTS_SET_OR_SKIP(st_apply(ST_CT));
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, 12.0f);
    live = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 12.0f);
    EXPECT_EQ(m, CTS_MODE_ALIASED);
    EXPECT_EQ(live, CTS_MODE_GDI_CLASSIC);
    IDWriteRenderingParams_Release(p); dwx_font_close(&x.h); cts_end();
}
TEST(rec_cleartype_snapshot_stays_classic_after_disable) {
    fx x; IDWriteRenderingParams *p = NULL; int m, live;
    CTS_BEGIN(); FX_OR_SKIP(x);
    CTS_SET_OR_SKIP(st_apply(ST_CT));
    DW_REQUIRE_OK(IDWriteFactory_CreateRenderingParams(dw_shared(), &p));
    CTS_SET_OR_SKIP(st_apply(ST_OFF));
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, 12.0f);
    live = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 12.0f);
    EXPECT_EQ(m, CTS_MODE_GDI_CLASSIC);
    EXPECT_EQ(live, CTS_MODE_ALIASED);
    IDWriteRenderingParams_Release(p); dwx_font_close(&x.h); cts_end();
}
TEST(rec_monitor_params_snapshot_too) {
    fx x; IDWriteRenderingParams *p = NULL; HMONITOR mon = cts_primary_monitor(); int m;
    CTS_BEGIN(); FX_OR_SKIP(x);
    if (!mon) { dwx_font_close(&x.h); cts_end(); t_skip("no monitor"); return; }
    CTS_SET_OR_SKIP(st_apply(ST_OFF));
    DW_REQUIRE_OK(IDWriteFactory_CreateMonitorRenderingParams(dw_shared(), mon, &p));
    CTS_SET_OR_SKIP(st_apply(ST_CT));
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, 12.0f);
    EXPECT_EQ(m, CTS_MODE_ALIASED);
    IDWriteRenderingParams_Release(p); dwx_font_close(&x.h); cts_end();
}
TEST(rec_custom_default_mode_params_snapshot_too) {
    /* every constructor path captures the smoothing state, including CreateCustomRenderingParams with mode DEFAULT */
    fx x; IDWriteRenderingParams *p = NULL; int m;
    CTS_BEGIN(); FX_OR_SKIP(x);
    CTS_SET_OR_SKIP(st_apply(ST_OFF));
    DW_REQUIRE_OK(IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 2.2f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p));
    CTS_SET_OR_SKIP(st_apply(ST_CT));
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, 12.0f);
    EXPECT_EQ(m, CTS_MODE_ALIASED);
    IDWriteRenderingParams_Release(p);
    CTS_SET_OR_SKIP(st_apply(ST_CT));
    DW_REQUIRE_OK(IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 2.2f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_DEFAULT, &p));
    CTS_SET_OR_SKIP(st_apply(ST_OFF));
    m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, 12.0f);
    EXPECT_EQ(m, CTS_MODE_GDI_CLASSIC);
    IDWriteRenderingParams_Release(p); dwx_font_close(&x.h); cts_end();
}
TEST(rec_explicit_mode_params_ignore_smoothing) {
    static const int modes[] = { DWRITE_RENDERING_MODE_ALIASED, DWRITE_RENDERING_MODE_GDI_CLASSIC, DWRITE_RENDERING_MODE_GDI_NATURAL };
    fx x; int i, st;
    CTS_BEGIN(); FX_OR_SKIP(x);
    for (i = 0; i < 3; i++) for (st = 0; st < ST_N; st++) {
        IDWriteRenderingParams *p = NULL; int m;
        CTS_SET_OR_SKIP(st_apply(st));
        if (FAILED(IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 2.2f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, (DWRITE_RENDERING_MODE)modes[i], &p)) || !p) continue;
        m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, 12.0f);
        /* a mode stored in the params object is returned as is */
        CHECKF(m == modes[i], "explicit mode %d in state %s -> %d", modes[i], st_name[st], m);
        IDWriteRenderingParams_Release(p);
    }
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_gdi_natural_measuring_always_gdi_natural) {
    fx x; int st;
    CTS_BEGIN(); FX_OR_SKIP(x);
    for (st = 0; st < ST_N; st++) {
        int m;
        CTS_SET_OR_SKIP(st_apply(st));
        m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_NATURAL, NULL, 12.0f);
        CHECKF(m == CTS_MODE_GDI_NATURAL, "%s: GDI_NATURAL measuring -> %d", st_name[st], m);
    }
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_natural_measuring_independent_of_smoothing) {
    fx x; int st, first = -2;
    CTS_BEGIN(); FX_OR_SKIP(x);
    for (st = 0; st < ST_N; st++) {
        int m;
        CTS_SET_OR_SKIP(st_apply(st));
        m = rec_mode(&x, DWRITE_MEASURING_MODE_NATURAL, NULL, 14.0f);
        if (first == -2) first = m;
        CHECKF(m == first, "%s: NATURAL measuring -> %d, first state gave %d", st_name[st], m, first);
        CHECKF(m != CTS_MODE_GDI_CLASSIC && m != CTS_MODE_GDI_NATURAL, "natural measuring gave a GDI mode %d", m);
    }
    t_note("NATURAL measuring at 14 em -> mode %d", first);
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_large_size_selects_outline_in_every_state) {
    fx x; int st;
    CTS_BEGIN(); FX_OR_SKIP(x);
    for (st = 0; st < ST_N; st++) {
        int m;
        CTS_SET_OR_SKIP(st_apply(st));
        m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 200.0f);
        CHECKF(m == CTS_MODE_OUTLINE, "%s: 200 em -> %d", st_name[st], m);
        m = rec_mode(&x, DWRITE_MEASURING_MODE_NATURAL, NULL, 200.0f);
        CHECKF(m == CTS_MODE_OUTLINE, "%s: 200 em natural -> %d", st_name[st], m);
    }
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_orientation_and_contrast_do_not_matter) {
    fx x; int base, m, i;
    CTS_BEGIN(); FX_OR_SKIP(x);
    CTS_SET_OR_SKIP(st_apply(ST_CT));
    base = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 12.0f);
    for (i = 0; i < 4; i++) {
        CTS_SET_OR_SKIP(cts_set_orient(i & 1)); CTS_SET_OR_SKIP(cts_set_contrast(1000 + 400 * (UINT)i));
        m = rec_mode(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, 12.0f);
        CHECKF(m == base, "step %d: %d vs %d", i, m, base);
    }
    dwx_font_close(&x.h); cts_end();
}
TEST(rec_face_versions_agree_in_every_state) {
    fx x; int st; IDWriteFontFace3 *f3 = NULL;
    CTS_BEGIN(); FX_OR_SKIP(x);
    if (FAILED(IDWriteFontFace_QueryInterface(x.face, &IID_IDWriteFontFace3, (void **)&f3)) || !f3) { dwx_font_close(&x.h); cts_end(); t_skip("IDWriteFontFace3 unsupported"); return; }
    for (st = 0; st < ST_N; st++) {
        DWRITE_RENDERING_MODE m0 = (DWRITE_RENDERING_MODE)-1; DWRITE_RENDERING_MODE1 m1 = (DWRITE_RENDERING_MODE1)-1; DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)-1;
        CTS_SET_OR_SKIP(st_apply(st));
        IDWriteRenderingParams *rp = NULL;
        CHECK_HR_OK(IDWriteFactory_CreateRenderingParams(dw_shared(), &rp));
        /* the original interface rejects a NULL params object; the newer one accepts it */
        CHECK_HR(IDWriteFontFace_GetRecommendedRenderingMode(x.face, 12.0f, 1.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, &m0), E_INVALIDARG);
        m0 = (DWRITE_RENDERING_MODE)-1;
        if (rp) CHECK_HR_OK(IDWriteFontFace_GetRecommendedRenderingMode(x.face, 12.0f, 1.0f, DWRITE_MEASURING_MODE_GDI_CLASSIC, rp, &m0));
        CHECK_HR_OK(IDWriteFontFace3_GetRecommendedRenderingMode(f3, 12.0f, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, DWRITE_MEASURING_MODE_GDI_CLASSIC, rp, &m1, &gf));
        if (rp) IDWriteRenderingParams_Release(rp);
        CHECKF((int)m0 == (int)m1, "%s: v0 %d vs v3 %d", st_name[st], (int)m0, (int)m1);
        CHECKF((unsigned)gf <= 2, "grid fit %d", (int)gf);
    }
    IDWriteFontFace3_Release(f3); dwx_font_close(&x.h); cts_end();
}
TEST(rec_out_params_zeroed_on_invalid_measuring_mode) {
    fx x; IDWriteFontFace3 *f3 = NULL; DWRITE_RENDERING_MODE1 m = (DWRITE_RENDERING_MODE1)0x55; DWRITE_GRID_FIT_MODE gf = (DWRITE_GRID_FIT_MODE)0x55;
    CTS_BEGIN(); FX_OR_SKIP(x);
    if (FAILED(IDWriteFontFace_QueryInterface(x.face, &IID_IDWriteFontFace3, (void **)&f3)) || !f3) { dwx_font_close(&x.h); cts_end(); t_skip("IDWriteFontFace3 unsupported"); return; }
    /* both outputs are cleared first, then a measuring mode outside 0..2 is rejected with E_INVALIDARG */
    CHECK_HR(IDWriteFontFace3_GetRecommendedRenderingMode(f3, 12.0f, 96.0f, 96.0f, NULL, FALSE, DWRITE_OUTLINE_THRESHOLD_ANTIALIASED, (DWRITE_MEASURING_MODE)3, NULL, &m, &gf), E_INVALIDARG);
    EXPECT_EQ(m, 0); EXPECT_EQ(gf, 0);
    m = (DWRITE_RENDERING_MODE1)0x55; gf = (DWRITE_GRID_FIT_MODE)0x55;
    CHECK_HR(IDWriteFontFace3_GetRecommendedRenderingMode(f3, 12.0f, 96.0f, 96.0f, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)2, DWRITE_MEASURING_MODE_NATURAL, NULL, &m, &gf), E_INVALIDARG);
    EXPECT_EQ(m, 0); EXPECT_EQ(gf, 0);
    IDWriteFontFace3_Release(f3); dwx_font_close(&x.h); cts_end();
}

/* ---- services that must not care ---------------------------------------------------------------------------------------- */
static int analyze_sig(IDWriteFactory *f, const WCHAR *text, UINT32 len, UINT32 *sig) {
    IDWriteTextAnalyzer *an = NULL; dwx_ana_src *src; dwx_ana_sink *snk; UINT32 i, h = 2166136261u; HRESULT hr;
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) return 0;
    src = dwx_ana_src_new(text, len); snk = dwx_ana_sink_new();
    hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)snk);
    if (SUCCEEDED(hr)) hr = IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)snk);
    for (i = 0; i < len; i++) {
        h = (h ^ snk->script[i].script) * 16777619u; h = (h ^ snk->script[i].shapes) * 16777619u;
        h = (h ^ *(UINT8 *)&snk->brk[i]) * 16777619u;
    }
    *sig = h;
    dwx_release(src); dwx_release(snk); IDWriteTextAnalyzer_Release(an);
    return SUCCEEDED(hr);
}
TEST(ana_script_and_breaks_unaffected_by_smoothing_state) {
    static const WCHAR text[] = { 'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', ' ', 0x0627, 0x0644, 0x0639, ' ', 0x4E2D, 0x6587, '.', 0 };
    UINT32 base = 0, sig; int st;
    CTS_BEGIN();
    if (!dw_shared() || !analyze_sig(dw_shared(), text, 19, &base)) { cts_end(); t_skip("analyzer unavailable"); return; }
    for (st = 0; st < ST_N; st++) {
        CTS_SET_OR_SKIP(st_apply(st));
        CHECK(analyze_sig(dw_shared(), text, 19, &sig));
        CHECKF(sig == base, "%s: analysis signature %08x vs %08x", st_name[st], sig, base);
    }
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    CHECK(analyze_sig(dw_shared(), text, 19, &sig)); CHECK(sig == base);
    cts_end();
}
TEST(layout_natural_metrics_unaffected_by_smoothing_state) {
    IDWriteFactory *f = dw_shared(); const WCHAR fam[] = L"Arial"; int st; float base_w = -1, base_h = -1;
    static const WCHAR txt[] = L"The quick brown fox";
    CTS_BEGIN();
    if (!f || !dw_have_font(f, fam)) { cts_end(); t_skip("Arial not installed"); return; }
    for (st = 0; st < ST_N; st++) {
        IDWriteTextFormat *tf = NULL; IDWriteTextLayout *tl = NULL; DWRITE_TEXT_METRICS tm;
        CTS_SET_OR_SKIP(st_apply(st));
        if (FAILED(IDWriteFactory_CreateTextFormat(f, fam, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &tf))) continue;
        if (SUCCEEDED(IDWriteFactory_CreateTextLayout(f, txt, 19, tf, 1000.0f, 100.0f, &tl)) && SUCCEEDED(IDWriteTextLayout_GetMetrics(tl, &tm))) {
            if (base_w < 0) { base_w = tm.width; base_h = tm.height; }
            CHECKF(tm.width == base_w && tm.height == base_h, "%s: %g x %g vs %g x %g", st_name[st], (double)tm.width, (double)tm.height, (double)base_w, (double)base_h);
        }
        DW_RELEASE(tl); DW_RELEASE(tf);
    }
    cts_end();
}
TEST(gdi_compatible_layout_still_creatable_in_every_state) {
    IDWriteFactory *f = dw_shared(); const WCHAR fam[] = L"Arial"; int st;
    static const WCHAR txt[] = L"Hello";
    CTS_BEGIN();
    if (!f || !dw_have_font(f, fam)) { cts_end(); t_skip("Arial not installed"); return; }
    for (st = 0; st < ST_N; st++) {
        IDWriteTextFormat *tf = NULL; IDWriteTextLayout *tl = NULL; DWRITE_TEXT_METRICS tm;
        CTS_SET_OR_SKIP(st_apply(st));
        DW_REQUIRE_OK(IDWriteFactory_CreateTextFormat(f, fam, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf));
        CHECK_HR_OK(IDWriteFactory_CreateGdiCompatibleTextLayout(f, txt, 5, tf, 500.0f, 100.0f, 1.0f, NULL, TRUE, &tl));
        if (tl) { CHECK_HR_OK(IDWriteTextLayout_GetMetrics(tl, &tm)); CHECK(tm.width > 0.0f); t_note("%s: GDI-compatible layout width %g", st_name[st], (double)tm.width); }
        DW_RELEASE(tl); DW_RELEASE(tf);
    }
    cts_end();
}

/* ---- pixels: note-level (the exact anti-aliasing is renderer detail) ------------------------------------------------------ */
static int draw_ink(fx *x, DWRITE_MEASURING_MODE mm, IDWriteRenderingParams *p, int *distinct, int *ink) {
    IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL; dwx_dc d; UINT16 gid = 2; FLOAT adv = 12.0f; DWRITE_GLYPH_RUN run;
    int ok = 0, i; uint32_t seen[64]; int ns = 0; HDC mdc;
    *distinct = 0; *ink = 0;
    if (FAILED(IDWriteFactory_GetGdiInterop(dw_shared(), &gi)) || !gi) return 0;
    if (!dwx_dc_create(&d, 64, 32, 0xFFFFFF)) { IDWriteGdiInterop_Release(gi); return 0; }
    if (SUCCEEDED(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, d.dc, 64, 32, &rt)) && rt) {
        memset(&run, 0, sizeof run);
        run.fontFace = x->face; run.fontEmSize = 24.0f; run.glyphCount = 1; run.glyphIndices = &gid; run.glyphAdvances = &adv;
        mdc = IDWriteBitmapRenderTarget_GetMemoryDC(rt);
        if (mdc) {
            BitBlt(mdc, 0, 0, 64, 32, NULL, 0, 0, WHITENESS);
            if (SUCCEEDED(IDWriteBitmapRenderTarget_DrawGlyphRun(rt, 4.0f, 26.0f, mm, &run, p, 0x000000, NULL))) {
                int px, py;
                BitBlt(d.dc, 0, 0, 64, 32, mdc, 0, 0, SRCCOPY);
                for (py = 0; py < 32; py++) for (px = 0; px < 64; px++) {
                    uint32_t c = dwx_dc_pixel(&d, px, py);
                    if (c == 0xFFFFFF) continue;
                    (*ink)++;
                    for (i = 0; i < ns; i++) if (seen[i] == c) break;
                    if (i == ns && ns < 64) seen[ns++] = c;
                }
                *distinct = ns; ok = 1;
            }
        }
        IDWriteBitmapRenderTarget_Release(rt);
    }
    dwx_dc_destroy(&d); IDWriteGdiInterop_Release(gi);
    return ok;
}
TEST(pix_bitmap_target_gdi_classic_by_smoothing_state_note) {
    fx x; int st;
    CTS_BEGIN(); FX_OR_SKIP(x);
    for (st = 0; st < ST_N; st++) {
        int distinct, ink;
        CTS_SET_OR_SKIP(st_apply(st));
        if (!draw_ink(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, NULL, &distinct, &ink)) { t_note("%s: draw failed", st_name[st]); continue; }
        t_note("%s: GDI_CLASSIC glyph 'A' ink pixels %d, distinct colours %d", st_name[st], ink, distinct);
        CHECKF(ink > 0, "%s: nothing drawn", st_name[st]);
    }
    dwx_font_close(&x.h); cts_end();
}
TEST(pix_bitmap_target_explicit_aliased_params_two_tone) {
    fx x; IDWriteRenderingParams *p = NULL; int distinct, ink, st;
    CTS_BEGIN(); FX_OR_SKIP(x);
    DW_REQUIRE_OK(IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 1.8f, 0.5f, 1.0f, DWRITE_PIXEL_GEOMETRY_RGB, DWRITE_RENDERING_MODE_ALIASED, &p));
    for (st = 0; st < ST_N; st++) {
        CTS_SET_OR_SKIP(st_apply(st));
        if (!draw_ink(&x, DWRITE_MEASURING_MODE_GDI_CLASSIC, p, &distinct, &ink)) continue;
        t_note("%s: explicit ALIASED params -> ink %d colours %d", st_name[st], ink, distinct);
        CHECKF(ink > 0, "%s: nothing drawn", st_name[st]);
        /* aliased output of black text on white has no intermediate colours */
        CHECKF(distinct <= 1, "%s: aliased rendering produced %d distinct colours", st_name[st], distinct);
    }
    IDWriteRenderingParams_Release(p); dwx_font_close(&x.h); cts_end();
}
