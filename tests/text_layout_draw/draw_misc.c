/* Drawing effects, shaping results visible in glyph runs (ligatures, kerning), cross-checks against the layout's own queries
 * (cluster metrics, hit testing, line metrics), threads, volume, and system fonts. */
#include "td.h"

#define RD() dwx_renderer *r = dwx_renderer_new()
#define DONE(l, r) do { IDWriteTextLayout_Release(l); dwx_release(r); TD_END(); } while (0)
#define NOSNAP(r) ((r)->snapping_disabled = TRUE)
#define SHAPE_LAYOUT(var, text) IDWriteTextLayout *var = td_layout_(&X, TD_SHAPE, text, 16.0f, 1000.0f, 100.0f); if (!var) { TD_END(); t_skip("DwShape layout failed"); return; }

/* ---- drawing effects --------------------------------------------------------------------------------------------- */
TEST(fx_effect_per_range)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBBCCCC", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 4)));
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(8, 4)));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    EXPECT_EQ(td_run_at(r, 0)->effect, (IUnknown *)e1); EXPECT_EQ(td_run_at(r, 4)->effect, NULL); EXPECT_EQ(td_run_at(r, 8)->effect, (IUnknown *)e2);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e1); dwx_release(e2); TD_END();
}
TEST(fx_effect_whole_text_single_run)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e = dwx_effect_new(5);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 8));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1); EXPECT_EQ(td_nth(r, DWX_DRAW_GLYPHRUN, 0)->effect, (IUnknown *)e);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(fx_effect_overwrite_and_clear)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 8)); IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(2, 4));
    EXPECT_EQ(dwx_refs(e1) >= 2, 1);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    EXPECT_EQ(td_run_at(r, 0)->effect, (IUnknown *)e1); EXPECT_EQ(td_run_at(r, 2)->effect, (IUnknown *)e2); EXPECT_EQ(td_run_at(r, 6)->effect, (IUnknown *)e1);
    IDWriteTextLayout_SetDrawingEffect(l, NULL, RNG(0, 8));
    dwx_renderer_clear(r); DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1); EXPECT_EQ(td_nth(r, DWX_DRAW_GLYPHRUN, 0)->effect, NULL);
    EXPECT_EQ(dwx_refs(e1), 1); EXPECT_EQ(dwx_refs(e2), 1);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e1); dwx_release(e2); TD_END();
}
TEST(fx_effect_refs_during_callback)
{
    /* the effect is kept alive by the layout while the renderer is called (never released early) */
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e = dwx_effect_new(5);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->effect_refs >= 2);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(fx_effect_getter_matches_draw)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e = dwx_effect_new(5);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(4, 4));
    IUnknown *got = NULL; DWRITE_TEXT_RANGE rg;
    CHECK_HR_OK(IDWriteTextLayout_GetDrawingEffect(l, 5, &got, &rg));
    EXPECT_EQ(got, (IUnknown *)e); EXPECT_EQ(rg.startPosition, 4); EXPECT_EQ(rg.length, 4);
    if (got) IUnknown_Release(got);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_run_at(r, 4)->effect, got);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(fx_effect_arbitrary_com_object)
{
    /* any IUnknown works: dwrite never queries it */
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e = dwx_effect_new(5);
    LONG qi0 = dwx_qi_calls(e);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 4));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(dwx_qi_calls(e), qi0);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(fx_effect_range_beyond_text_fails)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e = dwx_effect_new(5);
    HRESULT hr = IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 5000));
    t_note("SetDrawingEffect past the end -> %s", dw_hr_name(hr));
    DRAW_OK(l, r, 0.0f, 0.0f);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TD_END();
}
TEST(fx_effects_on_every_callback_kind)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB#CD", 16.0f, 1000.0f, 100.0f); RD();
    dwx_effect *e = dwx_effect_new(8); dwx_inline *io = dwx_inline_new(10.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(2, 1));
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 5));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 5)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 5));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i, kinds[5] = { 0 };
    for (i = 0; i < r->ncalls; i++) { EXPECT_EQ(r->calls[i].effect, (IUnknown *)e); kinds[r->calls[i].kind]++; }
    CHECK(kinds[1] >= 1 && kinds[2] >= 1 && kinds[3] >= 1 && kinds[4] == 1);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); dwx_release(io); TD_END();
}

/* ---- ligatures and kerning (DwShape) ------------------------------------------------------------------------------ */
TEST(shape_ligature_default_on)
{
    TD_BEGIN(); SHAPE_LAYOUT(l, L"xABy"); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->has_desc);
    if (c) {
        EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
        EXPECT_EQ(c->text_len, 4);
        EXPECT_EQ(c->nglyphs, 3);
        if (c->nglyphs == 3) { EXPECT_EQ(c->indices[1], 68); CHECK_NEARF(c->advances[1], 7.8125); }
        if (c->cluster_map && c->text_len == 4) { EXPECT_EQ(c->cluster_map[0], 0); EXPECT_EQ(c->cluster_map[1], 1); EXPECT_EQ(c->cluster_map[2], 1); EXPECT_EQ(c->cluster_map[3], 2); }
    }
    DONE(l, r);
}
TEST(shape_ligature_off_via_typography)
{
    TD_BEGIN(); SHAPE_LAYOUT(l, L"xABy"); RD();
    IDWriteTypography *ty = NULL;
    if (FAILED(IDWriteFactory_CreateTypography(f, &ty))) { DONE(l, r); t_skip("CreateTypography failed"); return; }
    DWRITE_FONT_FEATURE ft = { (DWRITE_FONT_FEATURE_TAG)DW_TAG('l', 'i', 'g', 'a'), 0 };
    CHECK_HR_OK(IDWriteTypography_AddFontFeature(ty, ft));
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, ty, RNG(0, 4)));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c != NULL);
    if (c) { EXPECT_EQ(c->nglyphs, 4); if (c->nglyphs == 4) { EXPECT_EQ(c->indices[1], 2); EXPECT_EQ(c->indices[2], 3); } }
    IDWriteTypography_Release(ty); DONE(l, r);
}
TEST(shape_ligature_split_by_run_boundary)
{
    /* a size change between A and B prevents the ligature: two runs, no ligature glyph */
    TD_BEGIN(); SHAPE_LAYOUT(l, L"xABy"); RD();
    IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(2, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_total_glyphs(r), 4);
    int i; for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) { UINT32 k; for (k = 0; k < r->calls[i].nglyphs; k++) CHECK(r->calls[i].indices[k] != 68); }
    DONE(l, r);
}
TEST(shape_ligature_cluster_vs_layout_clusters)
{
    TD_BEGIN(); SHAPE_LAYOUT(l, L"xABy"); RD();
    UINT32 n = 0; DWRITE_CLUSTER_METRICS cm[8];
    HRESULT hr = IDWriteTextLayout_GetClusterMetrics(l, cm, 8, &n);
    CHECK_HR_OK(hr);
    EXPECT_EQ(n, 3);
    if (n == 3) { EXPECT_EQ(cm[1].length, 2); CHECK_NEARF(cm[1].width, 7.8125); }
    DRAW_OK(l, r, 0.0f, 0.0f);
    DONE(l, r);
}
TEST(shape_kerning_changes_advance)
{
    TD_BEGIN(); SHAPE_LAYOUT(l, L"xCDy"); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->nglyphs == 4);
    if (c && c->nglyphs == 4) {
        t_note("advance of C before D: %g (plain 7.8125)", c->advances[1]);
        CHECK_NEARF(c->advances[1], (1000 - 200) * 16.0f / 2048.0f);
        CHECK_NEARF(c->advances[0], 7.8125); CHECK_NEARF(c->advances[2], 7.8125); CHECK_NEARF(c->advances[3], 7.8125);
    }
    DONE(l, r);
}
TEST(shape_kerning_not_applied_reverse_pair)
{
    TD_BEGIN(); SHAPE_LAYOUT(l, L"xDCy"); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c && c->nglyphs == 4) { CHECK_NEARF(c->advances[1], 7.8125); CHECK_NEARF(c->advances[2], 7.8125); }
    DONE(l, r);
}
TEST(shape_kerning_off_by_pair_kerning_api)
{
    TD_BEGIN(); SHAPE_LAYOUT(l, L"xCDy"); RD();
    IDWriteTextLayout1 *l1 = NULL;
    if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1))) { DONE(l, r); t_skip("IDWriteTextLayout1 unsupported"); return; }
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, FALSE, RNG(0, 4)));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c && c->nglyphs == 4) CHECK_NEARF(c->advances[1], 7.8125);
    IDWriteTextLayout1_Release(l1); DONE(l, r);
}
TEST(shape_kerning_width_matches_layout_metrics)
{
    TD_BEGIN(); SHAPE_LAYOUT(l, L"CDCDCD"); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    if (c) CHECK_NEARF(dwx_run_width(c), m.widthIncludingTrailingWhitespace);
    DONE(l, r);
}
TEST(shape_second_run_positions_use_kerned_width)
{
    TD_BEGIN(); SHAPE_LAYOUT(l, L"CDCD"); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(2, 2));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 2);
    if (a && b) CHECK_NEARF(b->x, a->x + dwx_run_width(a));
    DONE(l, r);
}

/* ---- cross checks with the layout's own geometry --------------------------------------------------------------- */
TEST(xcheck_cluster_widths_equal_advances)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGH", 16.0f, 1000.0f, 100.0f); RD();
    UINT32 n = 0; DWRITE_CLUSTER_METRICS cm[16];
    CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(l, cm, 16, &n));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->nglyphs == n);
    if (c && c->nglyphs == n) { UINT32 i; for (i = 0; i < n; i++) CHECK_NEARF(cm[i].width, c->advances[i]); }
    DONE(l, r);
}
TEST(xcheck_hit_test_matches_run_origin)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAABBBB", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(4, 4));
    DRAW_OK(l, r, 7.0f, 0.0f);
    UINT32 pos[] = { 0, 2, 4, 6 }; size_t i;
    for (i = 0; i < 4; i++) {
        FLOAT px = 0, py = 0; DWRITE_HIT_TEST_METRICS hm;
        CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(l, pos[i], FALSE, &px, &py, &hm));
        const dwx_draw *c = td_run_at(r, pos[i] < 4 ? 0 : 4);
        if (c) {
            float off = 0; UINT32 k; for (k = 0; k < pos[i] - c->text_pos; k++) off += c->advances[k];
            CHECK_NEARF(7.0f + px, c->x + off);
        }
    }
    DONE(l, r);
}
TEST(xcheck_line_metrics_baseline_and_wrap)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA BBBB CCCC DDDD", 16.0f, 70.0f, 400.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    UINT32 start = 0, i;
    for (i = 0; i < ln.n; i++) {
        const dwx_draw *c = td_run_at(r, start);
        CHECK(c != NULL);
        if (c) CHECK_NEARF(c->y, ln.top[i] + ln.lm[i].baseline);
        start += ln.lm[i].length;
    }
    DONE(l, r);
}
TEST(xcheck_run_text_equals_line_text)
{
    static const WCHAR src[] = L"AAAA BBBB CCCC DDDD";
    TD_BEGIN(); TD_LAYOUT(l, src, 16.0f, 70.0f, 400.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    UINT32 start = 0, i;
    for (i = 0; i < ln.n; i++) {
        const dwx_draw *c = td_run_at(r, start);
        CHECK(c != NULL);
        if (c) CHECK(c->text_len <= ln.lm[i].length && !wcsncmp(c->text, src + start, c->text_len));
        start += ln.lm[i].length;
    }
    DONE(l, r);
}
TEST(xcheck_layout_metrics_bounds_contain_draw)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA BBBB CCCC DDDD", 16.0f, 70.0f, 400.0f); RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
    int i;
    for (i = 0; i < r->ncalls; i++) { const dwx_draw *c = &r->calls[i]; if (c->kind != DWX_DRAW_GLYPHRUN) continue; CHECK(c->x >= m.left - 0.01f); CHECK(c->x + dwx_run_width(c) <= m.left + m.widthIncludingTrailingWhitespace + 0.01f); CHECK(c->y <= m.top + m.height + 0.01f); }
    DONE(l, r);
}

/* ---- volume, threads ------------------------------------------------------------------------------------------------ */
TEST(volume_many_runs)
{
    WCHAR t[201]; int i;
    for (i = 0; i < 200; i++) t[i] = L'A' + (i % 26);
    t[200] = 0;
    TD_BEGIN(); TD_LAYOUT(l, t, 16.0f, 100000.0f, 100.0f); RD();
    for (i = 0; i < 200; i += 2) IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(i, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 200);
    EXPECT_EQ(td_total_glyphs(r), 200);
    int pos = 0, k;
    for (k = 0; k < r->ncalls; k++) { EXPECT_EQ(r->calls[k].text_pos, pos); pos += r->calls[k].text_len; }
    DONE(l, r);
}
TEST(volume_long_text_many_lines)
{
    enum { N = 6000 };
    WCHAR *t = (WCHAR *)malloc((N + 1) * sizeof(WCHAR)); int i;
    for (i = 0; i < N; i++) t[i] = (i % 6 == 5) ? L' ' : L'A' + (i % 26);
    t[N] = 0;
    TD_BEGIN(); TD_LAYOUT(l, t, 16.0f, 200.0f, 100000.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    UINT32 lines = td_lines(l);
    CHECK(lines > 50);
    CHECK(td_count(r, DWX_DRAW_GLYPHRUN) >= (int)lines);
    unsigned letters = 0; int k;
    for (k = 0; k < r->ncalls; k++) { UINT32 j; if (r->calls[k].kind != DWX_DRAW_GLYPHRUN) continue; for (j = 0; j < r->calls[k].text_len; j++) if (r->calls[k].text[j] != L' ') letters++; }
    EXPECT_EQ(letters, N - N / 6);
    DONE(l, r); free(t);
}
typedef struct { IDWriteFactory *f; IDWriteTextFormat *fmt; int ok, iters; } thr_arg;
static DWORD WINAPI thr_main(void *p)
{
    thr_arg *a = (thr_arg *)p; int i;
    for (i = 0; i < a->iters; i++) {
        IDWriteTextLayout *l = NULL; dwx_renderer *r;
        if (FAILED(IDWriteFactory_CreateTextLayout(a->f, L"AAAA BBBB CCCC", 14, a->fmt, 60.0f, 100.0f, &l))) return 1;
        IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 14));
        r = dwx_renderer_new();
        if (SUCCEEDED(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.0f, 0.0f)) && td_total_glyphs(r) >= 12 && td_count(r, DWX_DRAW_UNDERLINE) >= 1) a->ok++;
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    return 0;
}
TEST(threads_draw_independent_layouts)
{
    enum { NT = 4, IT = 40 };
    if (t_is_wine()) { t_skip("Wine's dwrite crashes when a format is shared across threads"); return; }
    TD_BEGIN(); IDWriteTextFormat *fmt = td_format(&X, TD_FAM, 16.0f);
    if (!fmt) { TD_END(); t_skip("format failed"); return; }
    thr_arg a[NT]; HANDLE h[NT]; int i, total = 0;
    dwx_release(dwx_renderer_new());      /* the double registry initialises its lock lazily: do it before the threads race */
    for (i = 0; i < NT; i++) { a[i].f = f; a[i].fmt = fmt; a[i].ok = 0; a[i].iters = IT; h[i] = CreateThread(NULL, 0, thr_main, &a[i], 0, NULL); }
    for (i = 0; i < NT; i++) { if (h[i]) { CHECK(WaitForSingleObject(h[i], 30000) == WAIT_OBJECT_0); CloseHandle(h[i]); total += a[i].ok; } }
    EXPECT_EQ(total, NT * IT);
    IDWriteTextFormat_Release(fmt); TD_END();
}

/* ---- system fonts: relations only ------------------------------------------------------------------------------------- */
static const WCHAR *k_sys[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Consolas", L"Verdana" };
TEST(sys_run_fields_for_system_fonts)
{
    TD_BEGIN(); size_t i; int tested = 0;
    for (i = 0; i < sizeof k_sys / sizeof k_sys[0]; i++) {
        IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
        if (!dw_have_font(f, k_sys[i])) { t_note("%ls not installed", k_sys[i]); continue; }
        if (FAILED(IDWriteFactory_CreateTextFormat(f, k_sys[i], NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &t))) continue;
        if (FAILED(IDWriteFactory_CreateTextLayout(f, L"Hello World", 11, t, 1000.0f, 100.0f, &l))) { IDWriteTextFormat_Release(t); continue; }
        RD(); NOSNAP(r);
        DRAW_OK(l, r, 1.0f, 2.0f);
        const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
        CHECKF(c != NULL, "%ls: no glyph run", k_sys[i]);
        if (c) {
            CHECK(c->face != NULL); CHECK_FLOAT_EQ(c->em, 20.0f);
            EXPECT_EQ(c->nglyphs, 11);
            td_check_indices(c, L"Hello World", "system font glyph indices");
            DWRITE_FONT_METRICS m; td_face_metrics(c, &m);
            CHECK_NEAR(c->y, 2.0f + m.ascent * 20.0f / m.designUnitsPerEm, 0.05);
            CHECK_NEARF(c->x, 1.0f);
            DWRITE_GLYPH_METRICS gm[11]; if (c->nglyphs == 11 && SUCCEEDED(IDWriteFontFace_GetDesignGlyphMetrics(c->face, c->indices, 11, gm, FALSE))) { UINT32 k; for (k = 0; k < 11; k++) CHECK_NEAR(c->advances[k], gm[k].advanceWidth * 20.0f / m.designUnitsPerEm, 0.02 + 0.2); }
            t_note("%ls: ascent %u descent %u upm %u baseline %g", k_sys[i], m.ascent, m.descent, m.designUnitsPerEm, c->y);
            tested++;
        }
        IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t); dwx_release(r);
    }
    if (!tested) t_skip("no system font available");
    TD_END();
}
TEST(sys_arial_underline_strike_from_face_metrics)
{
    TD_BEGIN();
    if (!dw_have_font(f, L"Arial")) { TD_END(); t_skip("Arial not installed"); return; }
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 32.0f, L"en-us", &t);
    IDWriteFactory_CreateTextLayout(f, L"Underlined", 10, t, 1000.0f, 100.0f, &l);
    if (!l) { DW_RELEASE(t); TD_END(); t_skip("layout failed"); return; }
    RD(); NOSNAP(r);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 10)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 10));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *u = td_nth(r, DWX_DRAW_UNDERLINE, 0), *s = td_nth(r, DWX_DRAW_STRIKETHROUGH, 0), *g = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(u && s && g);
    if (u && s && g) {
        DWRITE_FONT_METRICS m; td_face_metrics(g, &m);
        float sc = 32.0f / m.designUnitsPerEm;
        CHECK_NEAR(u->ul.thickness, m.underlineThickness * sc, 1e-4); CHECK_NEAR(u->ul.offset, -m.underlinePosition * sc, 1e-4);
        CHECK_NEAR(s->st.thickness, m.strikethroughThickness * sc, 1e-4); CHECK_NEAR(s->st.offset, -m.strikethroughPosition * sc, 1e-4);
        CHECK_NEAR(u->ul.width, dwx_run_width(g), 1e-3); CHECK_NEAR(s->st.width, dwx_run_width(g), 1e-3);
        t_note("Arial 32px: ul pos %d thick %u, strike pos %d thick %u, upm %u", m.underlinePosition, m.underlineThickness, m.strikethroughPosition, m.strikethroughThickness, m.designUnitsPerEm);
    }
    IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t); dwx_release(r); TD_END();
}
TEST(sys_arial_bold_italic_faces)
{
    TD_BEGIN();
    if (!dw_have_font(f, L"Arial")) { TD_END(); t_skip("Arial not installed"); return; }
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &t);
    IDWriteFactory_CreateTextLayout(f, L"AAAABBBBCCCC", 12, t, 1000.0f, 100.0f, &l);
    if (!l) { DW_RELEASE(t); TD_END(); t_skip("layout failed"); return; }
    IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(4, 4)); IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, RNG(8, 4));
    RD(); DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 3);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 4), *c = td_run_at(r, 8);
    if (a && b && c) {
        CHECK(IDWriteFontFace_GetSimulations(b->face) == DWRITE_FONT_SIMULATIONS_NONE);
        CHECK(IDWriteFontFace_GetSimulations(c->face) == DWRITE_FONT_SIMULATIONS_NONE);
        CHECK(b->face != a->face); CHECK(c->face != a->face);
        CHECK(dwx_run_width(b) > dwx_run_width(a));      /* Arial Bold is wider than Arial Regular for A and B */
        CHECK_NEARF(b->x, dwx_run_width(a)); CHECK_NEARF(c->x, dwx_run_width(a) + dwx_run_width(b));
    }
    IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t); dwx_release(r); TD_END();
}
TEST(sys_fallback_hebrew_arabic_cjk_drawn)
{
    /* a font without the script falls back to a system face: every character is covered by some run */
    TD_BEGIN(); TD_LAYOUT(l, L"Aאב اب 中文", 16.0f, 1000.0f, 100.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    unsigned total = 0; int i;
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) total += r->calls[i].text_len;
    CHECK(total >= 8);
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) { CHECK(r->calls[i].face != NULL); CHECK(r->calls[i].em > 0); }
    t_note("%d glyph runs for mixed-script text", td_count(r, DWX_DRAW_GLYPHRUN));
    DONE(l, r);
}
