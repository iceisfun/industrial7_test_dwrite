/* IDWriteTextLayout::Draw: argument validation, the renderer protocol (QueryInterface / pixel snapping callbacks), callback
 * categories and their order, context passthrough, error propagation and reference-count hygiene. */
#include "td.h"

#define RD() dwx_renderer *r = dwx_renderer_new()
#define DONE(l, r) do { IDWriteTextLayout_Release(l); dwx_release(r); TD_END(); } while (0)

/* ---- argument validation ----------------------------------------------------------------------------------- */
TEST(draw_null_renderer_invalidarg)
{
    if (t_is_wine()) { t_skip("Wine dereferences a NULL renderer"); return; }
    /* DWriteTextLayout::Draw rejects a NULL renderer with E_INVALIDARG before touching the layout. */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    CHECK_HR(IDWriteTextLayout_Draw(l, CTX_A, NULL, 0.0f, 0.0f), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_Draw(l, NULL, NULL, 10.0f, 10.0f), E_INVALIDARG);
    EXPECT_REFS(l, 1);
    IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_null_renderer_empty_layout_invalidarg)
{
    if (t_is_wine()) { t_skip("Wine dereferences a NULL renderer"); return; }
    TD_BEGIN(); IDWriteTextLayout *l = td_layout_(&X, TD_FAM, L"", 16.0f, 100.0f, 50.0f);
    if (!l) { TD_END(); t_skip("empty layout not creatable"); return; }
    CHECK_HR(IDWriteTextLayout_Draw(l, NULL, NULL, 0.0f, 0.0f), E_INVALIDARG);
    IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_null_renderer_leaves_layout_usable)
{
    if (t_is_wine()) { t_skip("Wine dereferences a NULL renderer"); return; }
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    CHECK_HR(IDWriteTextLayout_Draw(l, CTX_A, NULL, 0.0f, 0.0f), E_INVALIDARG);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    DONE(l, r);
}
TEST(draw_basic_returns_ok)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), S_OK);
    CHECK(r->ncalls >= 1);
    DONE(l, r);
}
TEST(draw_through_derived_interfaces)
{
    /* Draw is inherited unchanged by IDWriteTextLayout1..4: same callbacks through any of them. */
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 100.0f, 50.0f);
    const IID *iids[] = { &IID_IDWriteTextLayout1, &IID_IDWriteTextLayout2, &IID_IDWriteTextLayout3, &IID_IDWriteTextLayout4 };
    int i, base;
    dwx_renderer *r0 = dwx_renderer_new();
    DRAW_OK(l, r0, 5.0f, 6.0f); base = r0->ncalls;
    CHECK(base >= 1);
    for (i = 0; i < 4; i++) {
        IDWriteTextLayout *q = NULL; dwx_renderer *r;
        HRESULT hr = IDWriteTextLayout_QueryInterface(l, iids[i], (void **)&q);
        if (FAILED(hr)) { t_note("IDWriteTextLayout%d not supported: %s", i + 1, dw_hr_name(hr)); continue; }
        r = dwx_renderer_new();
        CHECK_HR_OK(IDWriteTextLayout_Draw(q, CTX_A, (IDWriteTextRenderer *)r, 5.0f, 6.0f));
        EXPECT_EQ(r->ncalls, base);
        if (r->ncalls == base && base) { CHECK_FBITS(r->calls[0].x, r0->calls[0].x); CHECK_FBITS(r->calls[0].y, r0->calls[0].y); }
        dwx_release(r); IDWriteTextLayout_Release(q);
    }
    dwx_release(r0); IDWriteTextLayout_Release(l); TD_END();
}

/* ---- empty layout ---------------------------------------------------------------------------------------- */
TEST(draw_empty_layout_no_draw_callbacks)
{
    /* nothing to draw: the snapping protocol still runs, but no glyph run / decoration / inline call is made. */
    TD_BEGIN(); IDWriteTextLayout *l = td_layout_(&X, TD_FAM, L"", 16.0f, 100.0f, 50.0f);
    if (!l) { TD_END(); t_skip("empty layout not creatable"); return; }
    RD();
    DRAW_OK(l, r, 3.0f, 4.0f);
    EXPECT_EQ(r->ncalls, 0);
    EXPECT_EQ(r->n_snapping, 1);
    DONE(l, r);
}
TEST(draw_empty_layout_with_underline_no_calls)
{
    TD_BEGIN(); IDWriteTextLayout *l = td_layout_(&X, TD_FAM, L"", 16.0f, 100.0f, 50.0f);
    if (!l) { TD_END(); t_skip("empty layout not creatable"); return; }
    RD();
    HRESULT hr = IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 0));
    t_note("SetUnderline on empty range -> %s", dw_hr_name(hr));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->ncalls, 0);
    DONE(l, r);
}
TEST(draw_only_spaces_succeeds)
{
    TD_BEGIN(); TD_LAYOUT(l, L"   ", 16.0f, 100.0f, 50.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    t_note("whitespace-only text: %d glyph runs, %u glyphs", td_count(r, DWX_DRAW_GLYPHRUN), td_total_glyphs(r));
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 0);
    DONE(l, r);
}

/* ---- client drawing context passthrough ------------------------------------------------------------------- */
TEST(draw_context_passthrough_table)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB CD", 16.0f, 100.0f, 50.0f);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 2)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(3, 2));
    void *ctxs[] = { NULL, (void *)(ULONG_PTR)1, (void *)(ULONG_PTR)0xdeadbeef00ULL, (void *)l, (void *)&X };
    int i, k;
    for (i = 0; i < (int)(sizeof ctxs / sizeof ctxs[0]); i++) {
        RD();
        CHECK_HR_OK(td_draw(l, r, ctxs[i], 0.0f, 0.0f));
        CHECK(r->ncalls >= 3);
        for (k = 0; k < r->ncalls; k++) EXPECT_EQ(r->calls[k].ctx, ctxs[i]);
        EXPECT_EQ(r->last_snap_ctx, ctxs[i]);
        dwx_release(r);
    }
    IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_context_reaches_every_snapping_callback)
{
    /* each of the three pixel snapping callbacks receives the client context unchanged */
    TD_BEGIN(); TD_LAYOUT(l, L"AB", 16.0f, 100.0f, 50.0f);
    td_rd *q = td_rd_new(); int i, seen = 0;
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    for (i = 0; i < q->nlog; i++) if (q->log[i].type != TE_QI) { EXPECT_EQ(q->log[i].ctx, CTX_A); seen++; }
    CHECK(seen >= 4);
    td_rd_free(q); IDWriteTextLayout_Release(l); TD_END();
}

/* ---- pixel snapping protocol ---------------------------------------------------------------------------- */
TEST(draw_snapping_enabled_calls_each_once)
{
    /* IsPixelSnappingDisabled == FALSE: GetPixelsPerDip and GetCurrentTransform are each queried exactly once per Draw */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF GHI", 16.0f, 40.0f, 200.0f); RD();
    r->snapping_disabled = FALSE;
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->n_snapping, 1); EXPECT_EQ(r->n_ppd, 1); EXPECT_EQ(r->n_transform, 1);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->n_snapping, 2); EXPECT_EQ(r->n_ppd, 2); EXPECT_EQ(r->n_transform, 2);
    DONE(l, r);
}
TEST(draw_snapping_disabled_skips_other_callbacks)
{
    /* TextLayout::Draw only reads the pixels-per-dip and the transform when pixel snapping is not disabled */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    r->snapping_disabled = TRUE;
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->n_snapping, 1); EXPECT_EQ(r->n_ppd, 0); EXPECT_EQ(r->n_transform, 0);
    DONE(l, r);
}
TEST(draw_snapping_queried_even_without_text)
{
    TD_BEGIN(); IDWriteTextLayout *l = td_layout_(&X, TD_FAM, L"", 16.0f, 100.0f, 50.0f);
    if (!l) { TD_END(); t_skip("empty layout not creatable"); return; }
    RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->n_snapping, 1); EXPECT_EQ(r->n_ppd, 1); EXPECT_EQ(r->n_transform, 1);
    DONE(l, r);
}
TEST(draw_snapping_protocol_precedes_drawing)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    td_rd *q = td_rd_new();
    int i, first_draw = -1, last_snap = -1, qi_after_draw = 0;
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    for (i = 0; i < q->nlog; i++) {
        int t = q->log[i].type;
        if (t == TE_GLYPH && first_draw < 0) first_draw = i;
        if (t == TE_SNAP || t == TE_XFORM || t == TE_PPD) last_snap = i;
        if (t == TE_QI && first_draw >= 0) qi_after_draw++;
    }
    CHECK(first_draw >= 0); CHECK(last_snap >= 0);
    CHECK(last_snap < first_draw);
    EXPECT_EQ(qi_after_draw, 0);
    EXPECT_EQ(td_rd_count(q, TE_SNAP), 1);
    EXPECT_EQ(td_rd_count(q, TE_XFORM), 1);
    EXPECT_EQ(td_rd_count(q, TE_PPD), 1);
    CHECK(td_rd_first(q, TE_SNAP) < td_rd_first(q, TE_PPD));
    CHECK(td_rd_first(q, TE_SNAP) < td_rd_first(q, TE_XFORM));
    t_note("snapping order: ppd at %d, transform at %d (snap at %d)", td_rd_first(q, TE_PPD), td_rd_first(q, TE_XFORM), td_rd_first(q, TE_SNAP));
    td_rd_free(q); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_snapping_disabled_event_order)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    td_rd *q = td_rd_new(); q->in->snapping_disabled = TRUE;
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    EXPECT_EQ(td_rd_count(q, TE_SNAP), 1);
    EXPECT_EQ(td_rd_count(q, TE_PPD), 0);
    EXPECT_EQ(td_rd_count(q, TE_XFORM), 0);
    CHECK(td_rd_count(q, TE_GLYPH) >= 1);
    CHECK(td_rd_first(q, TE_SNAP) < td_rd_first(q, TE_GLYPH));
    td_rd_free(q); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_snapping_counts_independent_of_run_count)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEFGHIJKLMNOP", 16.0f, 100.0f, 50.0f); RD();
    int i;
    for (i = 0; i < 16; i += 2) IDWriteTextLayout_SetFontSize(l, 20.0f + i, RNG(i, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    CHECK(td_count(r, DWX_DRAW_GLYPHRUN) >= 16);
    EXPECT_EQ(r->n_snapping, 1); EXPECT_EQ(r->n_ppd, 1); EXPECT_EQ(r->n_transform, 1);
    DONE(l, r);
}

/* ---- snapping failure aborts Draw before any drawing ---------------------------------------------------------- */
static const HRESULT k_hrs[] = { E_FAIL, E_OUTOFMEMORY, E_NOTIMPL, E_INVALIDARG, (HRESULT)0x88985003, (HRESULT)0x80070005 };
TEST(draw_snapping_failure_propagates_hresult)
{
    /* CallbackException::ThrowIfFailed turns a failing IsPixelSnappingDisabled into the Draw result; nothing is drawn */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    size_t i;
    for (i = 0; i < sizeof k_hrs / sizeof k_hrs[0]; i++) {
        RD(); r->fail_snapping = k_hrs[i];
        HRESULT hr = td_draw(l, r, CTX_A, 0.0f, 0.0f);
        CHECK_HR(hr, k_hrs[i]);
        EXPECT_EQ(r->ncalls, 0);
        EXPECT_EQ(r->n_snapping, 1); EXPECT_EQ(r->n_ppd, 0); EXPECT_EQ(r->n_transform, 0);
        dwx_release(r);
    }
    IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_ppd_failure_propagates_hresult)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    size_t i;
    for (i = 0; i < sizeof k_hrs / sizeof k_hrs[0]; i++) {
        RD(); r->fail_ppd = k_hrs[i];
        CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), k_hrs[i]);
        EXPECT_EQ(r->ncalls, 0);
        EXPECT_EQ(r->n_ppd, 1);
        dwx_release(r);
    }
    IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_transform_failure_propagates_hresult)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    size_t i;
    for (i = 0; i < sizeof k_hrs / sizeof k_hrs[0]; i++) {
        RD(); r->fail_transform = k_hrs[i];
        CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), k_hrs[i]);
        EXPECT_EQ(r->ncalls, 0);
        EXPECT_EQ(r->n_transform, 1);
        dwx_release(r);
    }
    IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_snapping_failure_order_of_ppd_and_transform)
{
    /* failing both: whichever the layout asks first is the only one called */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    r->fail_ppd = E_FAIL; r->fail_transform = E_FAIL;
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), E_FAIL);
    EXPECT_EQ(r->n_ppd + r->n_transform, 1);
    t_note("first snapping query after IsPixelSnappingDisabled: %s", r->n_ppd ? "GetPixelsPerDip" : "GetCurrentTransform");
    DONE(l, r);
}
TEST(draw_ppd_failure_ignored_when_snapping_disabled)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    r->snapping_disabled = TRUE; r->fail_ppd = E_FAIL; r->fail_transform = E_FAIL;
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->n_ppd, 0); EXPECT_EQ(r->n_transform, 0);
    CHECK(r->ncalls >= 1);
    DONE(l, r);
}
TEST(draw_snapping_failure_layout_still_usable)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    r->fail_snapping = E_FAIL;
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), E_FAIL);
    r->fail_snapping = 0;
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 1);
    EXPECT_REFS(l, 1);
    DONE(l, r);
}

/* ---- failing draw callbacks do not abort Draw ----------------------------------------------------------------- */
static void fill_all_kinds(IDWriteTextLayout *l)
{
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3));
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(4, 3));
}
TEST(draw_callback_failure_does_not_abort)
{
    /* DrawGlyphRuns / DrawUnderlines / DrawStrikethroughs / DrawInlineObjects never inspect the HRESULT returned by the renderer:
     * a failing DrawGlyphRun neither stops Draw nor changes its return value. */
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB CCC", 16.0f, 1000.0f, 50.0f); fill_all_kinds(l);
    dwx_renderer *base = dwx_renderer_new(); DRAW_OK(l, base, 0.0f, 0.0f);
    int n = base->ncalls, i; CHECK(n >= 4);
    size_t h;
    for (h = 0; h < sizeof k_hrs / sizeof k_hrs[0]; h++) {
        for (i = 1; i <= n; i++) {
            dwx_renderer *r = dwx_renderer_new(); r->fail_draw = k_hrs[h]; r->fail_at = i;
            HRESULT hr = td_draw(l, r, CTX_A, 0.0f, 0.0f);
            CHECK_HR(hr, S_OK);
            EXPECT_EQ(r->ncalls, n);
            dwx_release(r);
        }
    }
    dwx_release(base); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_all_callbacks_failing_still_complete)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB CCC", 16.0f, 1000.0f, 50.0f); fill_all_kinds(l);
    dwx_renderer *base = dwx_renderer_new(); DRAW_OK(l, base, 0.0f, 0.0f);
    RD(); r->fail_draw = E_FAIL; r->fail_at = 0;
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), S_OK);
    EXPECT_EQ(r->ncalls, base->ncalls);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), td_count(base, DWX_DRAW_GLYPHRUN));
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), td_count(base, DWX_DRAW_UNDERLINE));
    EXPECT_EQ(td_count(r, DWX_DRAW_STRIKETHROUGH), td_count(base, DWX_DRAW_STRIKETHROUGH));
    dwx_release(base); DONE(l, r);
}
TEST(draw_failing_callback_still_gets_balanced_refs)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA BBB", 16.0f, 1000.0f, 50.0f); RD();
    r->fail_draw = E_FAIL;
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_REFS(r, 1); EXPECT_REFS(l, 1);
    DONE(l, r);
}

/* ---- callback categories and their order ---------------------------------------------------------------------- */
TEST(draw_category_order_runs_inline_underline_strike)
{
    /* the four helpers run in this order, each over the whole layout (not per line, not per run) */
    TD_BEGIN(); TD_LAYOUT(l, L"AB CD EF GH IJ KL", 16.0f, 40.0f, 400.0f);
    dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(3, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 17)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 17));
    RD(); DRAW_OK(l, r, 0.0f, 0.0f);
    int i, phase = 0, bad = 0, seen[5] = { 0 };
    static const int rank[5] = { 0, 0, 2, 3, 1 };      /* glyph runs, inline objects, underlines, strikethroughs */
    for (i = 0; i < r->ncalls; i++) {
        int k = r->calls[i].kind;
        if (rank[k] < phase) bad++;
        if (rank[k] > phase) phase = rank[k];
        seen[k]++;
    }
    EXPECT_EQ(bad, 0);
    CHECK(seen[DWX_DRAW_GLYPHRUN] >= 2); CHECK(seen[DWX_DRAW_INLINE] == 1);
    CHECK(seen[DWX_DRAW_UNDERLINE] >= 1); CHECK(seen[DWX_DRAW_STRIKETHROUGH] >= 1);
    t_note("glyph runs %d, inline %d, underlines %d, strikethroughs %d", seen[1], seen[4], seen[2], seen[3]);
    EXPECT_EQ(dwx_refs(io) >= 1, 1);
    dwx_release(r); IDWriteTextLayout_Release(l); dwx_release(io); TD_END();
}
TEST(draw_sequence_numbers_dense)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB CD", 16.0f, 1000.0f, 50.0f); RD();
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 5));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i; for (i = 0; i < r->ncalls; i++) EXPECT_EQ(r->calls[i].seq, i + 1);
    DONE(l, r);
}
TEST(draw_no_decoration_without_underline_or_strike)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCDEF GHI", 16.0f, 1000.0f, 50.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_UNDERLINE), 0); EXPECT_EQ(td_count(r, DWX_DRAW_STRIKETHROUGH), 0); EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 0);
    DONE(l, r);
}

/* ---- repeated drawing --------------------------------------------------------------------------------------- */
static int same_call(const dwx_draw *a, const dwx_draw *b)
{
    UINT32 i;
    if (a->kind != b->kind || a->ctx != b->ctx || !td_fbits(a->x, b->x) || !td_fbits(a->y, b->y) || a->mm != b->mm) return 0;
    if (a->kind == DWX_DRAW_GLYPHRUN) {
        if (a->em != b->em || a->nglyphs != b->nglyphs || a->sideways != b->sideways || a->bidi != b->bidi || a->face != b->face) return 0;
        for (i = 0; i < a->nglyphs; i++) { if (a->indices[i] != b->indices[i] || a->advances[i] != b->advances[i]) return 0; }
        if (a->has_desc != b->has_desc || a->text_pos != b->text_pos || a->text_len != b->text_len) return 0;
    } else if (a->kind == DWX_DRAW_UNDERLINE) {
        if (a->ul.width != b->ul.width || a->ul.thickness != b->ul.thickness || a->ul.offset != b->ul.offset || a->ul.runHeight != b->ul.runHeight) return 0;
    } else if (a->kind == DWX_DRAW_STRIKETHROUGH) {
        if (a->st.width != b->st.width || a->st.thickness != b->st.thickness || a->st.offset != b->st.offset) return 0;
    }
    return 1;
}
TEST(draw_twice_identical)
{
    TD_BEGIN(); TD_LAYOUT(l, L"Hello World, wrap me please", 16.0f, 90.0f, 200.0f);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(2, 8)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(10, 6));
    IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(0, 3));
    dwx_renderer *a = dwx_renderer_new(), *b = dwx_renderer_new(); int i;
    DRAW_OK(l, a, 4.5f, 7.25f); DRAW_OK(l, b, 4.5f, 7.25f);
    EXPECT_EQ(a->ncalls, b->ncalls);
    for (i = 0; i < a->ncalls && i < b->ncalls; i++) CHECKF(same_call(&a->calls[i], &b->calls[i]), "call %d differs between two Draws", i);
    dwx_release(a); dwx_release(b); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_many_times_stable)
{
    TD_BEGIN(); TD_LAYOUT(l, L"stable output", 16.0f, 1000.0f, 50.0f);
    dwx_renderer *a = dwx_renderer_new(); int i;
    DRAW_OK(l, a, 1.0f, 1.0f);
    for (i = 0; i < 50; i++) {
        dwx_renderer *b = dwx_renderer_new();
        DRAW_OK(l, b, 1.0f, 1.0f);
        EXPECT_EQ(a->ncalls, b->ncalls);
        if (a->ncalls == b->ncalls && a->ncalls) CHECK(same_call(&a->calls[0], &b->calls[0]));
        dwx_release(b);
    }
    dwx_release(a); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_same_renderer_accumulates)
{
    /* the same renderer drawn twice sees the callbacks twice; clear() forgets them */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f); int n = r->ncalls;
    DRAW_OK(l, r, 0.0f, 0.0f); EXPECT_EQ(r->ncalls, 2 * n);
    dwx_renderer_clear(r); EXPECT_EQ(r->ncalls, 0);
    DRAW_OK(l, r, 0.0f, 0.0f); EXPECT_EQ(r->ncalls, n);
    DONE(l, r);
}
TEST(draw_origin_changes_translate_all_calls)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB CD EF", 16.0f, 30.0f, 400.0f); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8));
    dwx_renderer *a = dwx_renderer_new(), *b = dwx_renderer_new(); int i;
    a->snapping_disabled = TRUE; b->snapping_disabled = TRUE;
    DRAW_OK(l, a, 0.0f, 0.0f); DRAW_OK(l, b, 100.0f, 50.0f);
    EXPECT_EQ(a->ncalls, b->ncalls);
    for (i = 0; i < a->ncalls && i < b->ncalls; i++) {
        CHECK_NEAR(b->calls[i].x - a->calls[i].x, 100.0f, 0.001);
        CHECK_NEAR(b->calls[i].y - a->calls[i].y, 50.0f, 0.001);
    }
    dwx_release(a); dwx_release(b); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_reflects_layout_modification)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 50.0f);
    dwx_renderer *a = dwx_renderer_new(), *b = dwx_renderer_new();
    DRAW_OK(l, a, 0.0f, 0.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(0, 4)));
    DRAW_OK(l, b, 0.0f, 0.0f);
    const dwx_draw *c0 = td_nth(a, DWX_DRAW_GLYPHRUN, 0), *c1 = td_nth(b, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c0 && c1);
    if (c0 && c1) { CHECK_FLOAT_EQ(c0->em, 16.0f); CHECK_FLOAT_EQ(c1->em, 32.0f); CHECK_FLOAT_EQ(c1->advances[0], 15.625f); }
    dwx_release(a); dwx_release(b); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_does_not_change_metrics)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AB CD EF GH", 16.0f, 40.0f, 400.0f); RD();
    DWRITE_TEXT_METRICS m1, m2; memset(&m1, 1, sizeof m1); memset(&m2, 2, sizeof m2);
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m1));
    DRAW_OK(l, r, 12.0f, 9.0f);
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m2));
    CHECK(memcmp(&m1, &m2, sizeof m1) == 0);
    DONE(l, r);
}
TEST(draw_two_layouts_one_renderer)
{
    TD_BEGIN(); TD_LAYOUT(a, L"AAAA", 16.0f, 1000.0f, 50.0f); TD_LAYOUT(b, L"BB", 20.0f, 1000.0f, 50.0f); RD();
    DRAW_OK(a, r, 0.0f, 0.0f); DRAW_OK(b, r, 0.0f, 100.0f);
    const dwx_draw *c0 = td_nth(r, DWX_DRAW_GLYPHRUN, 0), *c1 = td_nth(r, DWX_DRAW_GLYPHRUN, 1);
    CHECK(c0 && c1);
    if (c0 && c1) { EXPECT_EQ(c0->nglyphs, 4); EXPECT_EQ(c1->nglyphs, 2); CHECK_FLOAT_EQ(c0->em, 16.0f); CHECK_FLOAT_EQ(c1->em, 20.0f); }
    IDWriteTextLayout_Release(a); IDWriteTextLayout_Release(b); dwx_release(r); TD_END();
}
TEST(draw_layout_released_before_renderer_data_use)
{
    /* recorded deep copies stay valid after the layout is gone */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    DRAW_OK(l, r, 0.0f, 0.0f);
    IDWriteTextLayout_Release(l);
    const dwx_draw *c = td_nth(r, DWX_DRAW_GLYPHRUN, 0);
    CHECK(c && c->has_desc);
    if (c && c->has_desc) { CHECK_WEQ(c->text, L"ABC"); }
    dwx_release(r); TD_END();
}

/* ---- QueryInterface protocol of the renderer ------------------------------------------------------------------ */
TEST(draw_asks_renderer_for_textrenderer1)
{
    /* Draw queries the renderer for IDWriteTextRenderer1 (d3e0e934-22a0-427e-aae4-7d9574b59db1) exactly once per call */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    td_rd *q = td_rd_new(); int i;
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    EXPECT_EQ(td_rd_qi_asked(q, &IID_IDWriteTextRenderer1), 1);
    for (i = 0; i < q->nlog; i++) if (q->log[i].type == TE_QI) t_note("QI asked: %08lx-%04x-%04x", q->log[i].iid.Data1, q->log[i].iid.Data2, q->log[i].iid.Data3);
    CHECK(q->log[0].type == TE_QI);
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    EXPECT_EQ(td_rd_qi_asked(q, &IID_IDWriteTextRenderer1), 2);
    td_rd_free(q); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_qi_before_anything_else)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    td_rd *q = td_rd_new();
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    CHECK(q->nlog > 0 && q->log[0].type == TE_QI);
    if (q->nlog > 0) CHECK(IsEqualIID(&q->log[0].iid, &IID_IDWriteTextRenderer1));
    int snap = td_rd_first(q, TE_SNAP);
    CHECK(snap > 0);
    td_rd_free(q); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_plain_renderer_uses_legacy_callbacks)
{
    /* a renderer that rejects IDWriteTextRenderer1 receives the IDWriteTextRenderer callbacks */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF", 16.0f, 1000.0f, 50.0f); RD();
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(4, 3));
    r->support_r1 = 0;
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->r1_calls, 0);
    CHECK(r->ncalls >= 3);
    int i; for (i = 0; i < r->ncalls; i++) EXPECT_EQ(r->calls[i].has_orientation, 0);
    EXPECT_EQ(dwx_qi_calls(r) >= 1, 1);
    DONE(l, r);
}
TEST(draw_renderer1_gets_oriented_callbacks)
{
    /* with IDWriteTextRenderer1 available only the orientation-aware methods are used; horizontal text is angle 0 */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF", 16.0f, 1000.0f, 50.0f);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(4, 3));
    dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(3, 1));
    RD(); r->support_r1 = 1;
    DRAW_OK(l, r, 0.0f, 0.0f);
    CHECK(r->ncalls >= 4);
    EXPECT_EQ(r->r1_calls, r->ncalls);
    int i; for (i = 0; i < r->ncalls; i++) { EXPECT_EQ(r->calls[i].has_orientation, 1); EXPECT_EQ(r->calls[i].angle, DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES); }
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 1);
    dwx_release(r); IDWriteTextLayout_Release(l); dwx_release(io); TD_END();
}
TEST(draw_renderer1_same_args_as_legacy)
{
    /* the oriented callbacks carry the same origin/run data as the legacy ones */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF GHI", 16.0f, 50.0f, 300.0f);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 7));
    dwx_renderer *a = dwx_renderer_new(), *b = dwx_renderer_new(); int i;
    b->support_r1 = 1;
    DRAW_OK(l, a, 2.0f, 3.0f); DRAW_OK(l, b, 2.0f, 3.0f);
    EXPECT_EQ(a->ncalls, b->ncalls);
    for (i = 0; i < a->ncalls && i < b->ncalls; i++) CHECK(same_call(&a->calls[i], &b->calls[i]));
    dwx_release(a); dwx_release(b); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_renderer1_snapping_protocol_unchanged)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD(); r->support_r1 = 1;
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->n_snapping, 1); EXPECT_EQ(r->n_ppd, 1); EXPECT_EQ(r->n_transform, 1);
    DONE(l, r);
}
TEST(draw_renderer1_qi_reference_released)
{
    /* the reference obtained by the QueryInterface is released again before Draw returns */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    td_rd *q = td_rd_new(); q->allow_r1 = 1;
    EXPECT_EQ(q->ref, 1);
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    EXPECT_EQ(q->ref, 1);
    CHECK(td_rd_count(q, TE_GLYPH) >= 1);
    td_rd_free(q); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_renderer1_via_td_wrapper_fails_over)
{
    /* wrapper that refuses the R1 interface behaves like a plain renderer */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    td_rd *q = td_rd_new(); q->allow_r1 = 0;
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    EXPECT_EQ(q->in->r1_calls, 0);
    EXPECT_EQ(q->ref, 1);
    td_rd_free(q); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_renderer_no_pixelsnapping_qi_needed)
{
    /* the pixel snapping methods are reached through the renderer pointer itself, not via QueryInterface(IDWritePixelSnapping) */
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f);
    td_rd *q = td_rd_new(); q->allow_pixel_snapping_qi = 0;
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, CTX_A, (IDWriteTextRenderer *)q, 0.0f, 0.0f));
    CHECK(td_rd_count(q, TE_SNAP) == 1);
    CHECK(td_rd_count(q, TE_GLYPH) >= 1);
    t_note("IDWritePixelSnapping asked %d times, IDWriteTextRenderer asked %d times", td_rd_qi_asked(q, &IID_IDWritePixelSnapping), td_rd_qi_asked(q, &IID_IDWriteTextRenderer));
    td_rd_free(q); IDWriteTextLayout_Release(l); TD_END();
}
TEST(draw_renderer_refcount_balanced)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF", 16.0f, 30.0f, 200.0f); RD();
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 7));
    EXPECT_REFS(r, 1);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_REFS(r, 1);
    r->support_r1 = 1; DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_REFS(r, 1);
    DONE(l, r);
}
TEST(draw_layout_refcount_balanced)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF", 16.0f, 30.0f, 200.0f); RD();
    EXPECT_REFS(l, 1);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_REFS(l, 1);
    DONE(l, r);
}
TEST(draw_effect_refcount_balanced)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF", 16.0f, 300.0f, 200.0f); RD();
    dwx_effect *e = dwx_effect_new(1);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 7)));
    LONG held = dwx_refs(e);
    CHECK(held >= 2);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(dwx_refs(e), held);
    IDWriteTextLayout_Release(l);
    EXPECT_EQ(dwx_refs(e), 1);
    dwx_release(e); dwx_release(r); TD_END();
}
TEST(draw_leak_check)
{
    DWX_LEAK_BEGIN(a);
    {
        TD_BEGIN(); TD_LAYOUT(l, L"ABC DEF", 16.0f, 30.0f, 200.0f); RD();
        dwx_inline *io = dwx_inline_new(5.0f, 5.0f, 4.0f, FALSE); dwx_effect *e = dwx_effect_new(2);
        IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
        IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 7));
        DRAW_OK(l, r, 0.0f, 0.0f);
        IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(io); dwx_release(e); TD_END();
    }
    DWX_LEAK_END(a);
}

/* ---- vertical layouts need IDWriteTextRenderer1 --------------------------------------------------------------- */
static IDWriteTextLayout *vertical_layout(td_fx *x)
{
    IDWriteTextLayout *l = td_layout_(x, TD_FAM, CJK, 16.0f, 200.0f, 200.0f);
    HRESULT hr;
    if (!l) return NULL;
    hr = IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
    if (SUCCEEDED(hr)) hr = IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
    if (FAILED(hr)) { t_note("vertical reading direction not accepted: %s", dw_hr_name(hr)); IDWriteTextLayout_Release(l); return NULL; }
    return l;
}
TEST(draw_vertical_plain_renderer_incompatible)
{
    /* TextLayout::Draw throws DWRITE_E_TEXTRENDERERINCOMPATIBLE when the layout is vertical and the renderer lacks IDWriteTextRenderer1 */
    TD_BEGIN(); IDWriteTextLayout *l = vertical_layout(&X);
    if (!l) { TD_END(); t_skip("vertical layout unavailable"); return; }
    RD(); r->support_r1 = 0;
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), DWRITE_E_TEXTRENDERERINCOMPATIBLE);
    EXPECT_EQ(r->ncalls, 0);
    DONE(l, r);
}
TEST(draw_vertical_renderer1_ok)
{
    TD_BEGIN(); IDWriteTextLayout *l = vertical_layout(&X);
    if (!l) { TD_END(); t_skip("vertical layout unavailable"); return; }
    RD(); r->support_r1 = 1;
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), S_OK);
    CHECK(td_count(r, DWX_DRAW_GLYPHRUN) >= 1);
    EXPECT_EQ(r->r1_calls, r->ncalls);
    DONE(l, r);
}
TEST(draw_vertical_incompatible_leaves_layout_usable)
{
    TD_BEGIN(); IDWriteTextLayout *l = vertical_layout(&X);
    if (!l) { TD_END(); t_skip("vertical layout unavailable"); return; }
    RD();
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), DWRITE_E_TEXTRENDERERINCOMPATIBLE);
    r->support_r1 = 1;
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), S_OK);
    EXPECT_REFS(l, 1);
    DONE(l, r);
}
TEST(draw_horizontal_plain_renderer_always_compatible)
{
    TD_BEGIN(); TD_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); RD();
    int k;
    DWRITE_READING_DIRECTION dirs[] = { DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT };
    for (k = 0; k < 2; k++) {
        IDWriteTextLayout_SetReadingDirection(l, dirs[k]);
        dwx_renderer_clear(r);
        CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), S_OK);
        CHECK(r->ncalls >= 1);
    }
    DONE(l, r);
}
