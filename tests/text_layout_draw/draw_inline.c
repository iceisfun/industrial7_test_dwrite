/* Inline objects (IDWriteInlineObject set with SetInlineObject) as seen through Draw: the renderer receives DrawInlineObject with
 * the application's object and the origin of its top-left corner; the layout never calls the object's Draw itself, but it reads
 * GetMetrics for line sizes. DrawInlineObjects runs after all glyph runs and before underlines and strikethroughs. */
#include "td.h"

#define RD() dwx_renderer *r = dwx_renderer_new()
#define NOSNAP(r) ((r)->snapping_disabled = TRUE)
#define IL(r, n) td_nth((r), DWX_DRAW_INLINE, (n))
#define DONE_IO(l, r, io) do { IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(io); TD_END(); } while (0)

TEST(inline_single_call_fields)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)));
    DRAW_OK(l, r, 10.0f, 5.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 1);
    const dwx_draw *c = IL(r, 0);
    CHECK(c != NULL);
    if (c) {
        EXPECT_EQ(c->obj, (IDWriteInlineObject *)io);
        EXPECT_EQ(c->ctx, CTX_A);
        EXPECT_EQ(c->obj_sideways, FALSE);
        EXPECT_EQ(c->obj_rtl, FALSE);
        EXPECT_EQ(c->effect, NULL);
        CHECK_NEARF(c->x, 10.0f + 7.8125);
        CHECK_NEARF(c->y + 8.0f, 5.0f + td_line_baseline(l, 0));
    }
    DONE_IO(l, r, io);
}
TEST(inline_layout_never_calls_object_draw)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(io->n_draw, 0);
    CHECK(io->n_metrics >= 1);
    t_note("object calls: metrics %ld overhang %ld break %ld", io->n_metrics, io->n_overhang, io->n_break);
    DONE_IO(l, r, io);
}
TEST(inline_metrics_read_for_layout)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f);
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DWRITE_TEXT_METRICS m; CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK(io->n_metrics >= 1);
    CHECK_NEARF(m.width, 2 * 7.8125 + 20.0);
    IDWriteTextLayout_Release(l); dwx_release(io); TD_END();
}
TEST(inline_height_and_baseline_enlarge_line)
{
    /* an object taller than the font raises the baseline to its own baseline and the line height to fit it */
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(20.0f, 60.0f, 45.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    EXPECT_EQ(ln.n, 1);
    CHECK_NEARF(ln.lm[0].baseline, 45.0f);
    CHECK_NEARF(ln.lm[0].height, 60.0f);
    const dwx_draw *c = IL(r, 0), *g = td_run_at(r, 0);
    if (c && g) { CHECK_NEARF(c->y, 0.0f); CHECK_NEARF(g->y, 45.0f); }
    DONE_IO(l, r, io);
}
TEST(inline_small_object_aligned_on_baseline)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(5.0f, 6.0f, 4.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = IL(r, 0), *g = td_run_at(r, 0);
    if (c && g) { CHECK_NEARF(c->y + 4.0f, g->y); CHECK_NEARF(g->y, td_line_baseline(l, 0)); }
    DONE_IO(l, r, io);
}
TEST(inline_between_runs_positions)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA#BBB", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(30.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(3, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *a = td_run_at(r, 0), *b = td_run_at(r, 4), *c = IL(r, 0);
    CHECK(a && b && c);
    if (a && b && c) {
        CHECK_NEARF(c->x, 3 * 7.8125); CHECK_NEARF(b->x, 3 * 7.8125 + 30.0);
        EXPECT_EQ(a->text_len, 3); EXPECT_EQ(b->text_len, 3);
    }
    DONE_IO(l, r, io);
}
TEST(inline_no_glyph_run_for_object_text)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAA#BBB", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(30.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(3, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i; for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) { CHECK(r->calls[i].text_pos != 3); EXPECT_EQ(r->calls[i].text_len, 3); }
    EXPECT_EQ(td_total_glyphs(r), 6);
    DONE_IO(l, r, io);
}
TEST(inline_multiple_objects_in_text_order)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B#C#D", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *o1 = dwx_inline_new(10.0f, 10.0f, 8.0f, FALSE), *o2 = dwx_inline_new(20.0f, 10.0f, 8.0f, FALSE), *o3 = dwx_inline_new(30.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o1, RNG(1, 1)); IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o2, RNG(3, 1)); IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o3, RNG(5, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 3);
    const dwx_draw *a = IL(r, 0), *b = IL(r, 1), *c = IL(r, 2);
    if (a && b && c) {
        EXPECT_EQ(a->obj, (IDWriteInlineObject *)o1); EXPECT_EQ(b->obj, (IDWriteInlineObject *)o2); EXPECT_EQ(c->obj, (IDWriteInlineObject *)o3);
        CHECK_NEARF(a->x, 7.8125); CHECK_NEARF(b->x, 2 * 7.8125 + 10.0); CHECK_NEARF(c->x, 3 * 7.8125 + 30.0);
    }
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(o1); dwx_release(o2); dwx_release(o3); TD_END();
}
TEST(inline_same_object_two_ranges)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B#C", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(10.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)); IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(3, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 2);
    const dwx_draw *a = IL(r, 0), *b = IL(r, 1);
    if (a && b) { EXPECT_EQ(a->obj, (IDWriteInlineObject *)io); EXPECT_EQ(b->obj, (IDWriteInlineObject *)io); }
    DONE_IO(l, r, io);
}
TEST(inline_object_at_start_and_end)
{
    TD_BEGIN(); TD_LAYOUT(l, L"#AB#", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(10.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(0, 1)); IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(3, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 2);
    const dwx_draw *a = IL(r, 0), *b = IL(r, 1);
    if (a && b) { CHECK_NEARF(a->x, 0.0f); CHECK_NEARF(b->x, 10.0f + 2 * 7.8125); }
    DONE_IO(l, r, io);
}
TEST(inline_only_object)
{
    TD_BEGIN(); TD_LAYOUT(l, L"#", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(25.0f, 20.0f, 15.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(0, 1));
    DRAW_OK(l, r, 3.0f, 4.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_GLYPHRUN), 0);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 1);
    const dwx_draw *c = IL(r, 0);
    if (c) { CHECK_NEARF(c->x, 3.0f); CHECK_NEARF(c->y, 4.0f + td_line_baseline(l, 0) - 15.0f); }
    DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
    CHECK_NEARF(m.width, 25.0);
    DONE_IO(l, r, io);
}
TEST(inline_wraps_to_next_line)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA #", 16.0f, 40.0f, 200.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(30.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(5, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    td_lines_t ln; td_get_lines(l, &ln);
    EXPECT_EQ(ln.n, 2);
    const dwx_draw *c = IL(r, 0);
    if (c && ln.n == 2) { CHECK_NEARF(c->x, 0.0f); CHECK_NEARF(c->y, ln.top[1] + ln.lm[1].baseline - 8.0f); }
    DONE_IO(l, r, io);
}
TEST(inline_alignment_center)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 200.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(20.0f, 10.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER);
    DRAW_OK(l, r, 0.0f, 0.0f);
    DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
    const dwx_draw *c = IL(r, 0);
    if (c) CHECK_NEARF(c->x, m.left + 7.8125);
    CHECK_NEARF(m.left, (200.0f - (2 * 7.8125f + 20.0f)) / 2);
    DONE_IO(l, r, io);
}
TEST(inline_snapping_y)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DRAW_OK(l, r, 0.4f, 0.3f);
    const dwx_draw *c = IL(r, 0);
    if (c) {
        float want = td_snap(0.3f + td_line_baseline(l, 0), 1.0f, 0.0f) - 8.0f;
        t_note("inline y %g, snapped baseline - object baseline %g, unsnapped %g", c->y, want, 0.3f + td_line_baseline(l, 0) - 8.0f);
        CHECK_NEARF(c->y, want);
        CHECK_NEARF(c->x, 0.4f + 7.8125);
    }
    DONE_IO(l, r, io);
}
TEST(inline_effect_passthrough)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE); dwx_effect *e = dwx_effect_new(9);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(1, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = IL(r, 0);
    CHECK(c != NULL);
    if (c) { EXPECT_EQ(c->effect, e); EXPECT_EQ(c->effect_refs >= 2, 1); }
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(io); dwx_release(e); TD_END();
}
TEST(inline_rtl_paragraph_flags)
{
    /* a lone neutral character resolves to the paragraph level (UBA N2): odd in an RTL paragraph, so isRightToLeft is TRUE */
    TD_BEGIN(); TD_LAYOUT(l, L"#", 16.0f, 200.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(0, 1));
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = IL(r, 0);
    CHECK(c != NULL);
    if (c) {
        EXPECT_EQ(c->obj_rtl, TRUE); EXPECT_EQ(c->obj_sideways, FALSE);
        t_note("inline object in RTL paragraph: origin (%g, %g)", c->x, c->y);
        CHECK(c->x >= 200.0f - 20.0f - 0.01f && c->x <= 200.0f);
    }
    DONE_IO(l, r, io);
}
TEST(inline_after_glyph_runs_before_decorations)
{
    TD_BEGIN(); TD_LAYOUT(l, L"AA#BB", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(2, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 5)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 5));
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i, ii = -1, lastg = -1, firstdeco = 1000;
    for (i = 0; i < r->ncalls; i++) {
        int k = r->calls[i].kind;
        if (k == DWX_DRAW_GLYPHRUN) lastg = i; else if (k == DWX_DRAW_INLINE) ii = i; else if (i < firstdeco) firstdeco = i;
    }
    CHECK(ii > lastg); CHECK(ii < firstdeco);
    DONE_IO(l, r, io);
}
TEST(inline_failing_renderer_continues)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3));
    r->fail_draw = E_FAIL; r->fail_at = 0;
    CHECK_HR(td_draw(l, r, CTX_A, 0.0f, 0.0f), S_OK);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 1); CHECK(td_count(r, DWX_DRAW_UNDERLINE) >= 1);
    DONE_IO(l, r, io);
}

/* ---- object refcounts ---------------------------------------------------------------------------------------------- */
TEST(inline_refs_held_by_layout)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    EXPECT_EQ(dwx_refs(io), 1);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    LONG held = dwx_refs(io);
    CHECK(held >= 2);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(dwx_refs(io), held);
    IDWriteTextLayout_Release(l);
    EXPECT_EQ(dwx_refs(io), 1);
    dwx_release(r); dwx_release(io); TD_END();
}
TEST(inline_replaced_object_released)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *a = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE), *b = dwx_inline_new(30.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)a, RNG(1, 1));
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)b, RNG(1, 1));
    EXPECT_EQ(dwx_refs(a), 1);
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = IL(r, 0);
    if (c) EXPECT_EQ(c->obj, (IDWriteInlineObject *)b);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(a); dwx_release(b); TD_END();
}
TEST(inline_removed_by_null)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, NULL, RNG(1, 1)));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 0);
    EXPECT_EQ(dwx_refs(io), 1);
    EXPECT_EQ(td_total_glyphs(r), 3);
    DONE_IO(l, r, io);
}
TEST(inline_getinlineobject_identity)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f);
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    IDWriteInlineObject *got = NULL; DWRITE_TEXT_RANGE rg;
    CHECK_HR_OK(IDWriteTextLayout_GetInlineObject(l, 1, &got, &rg));
    EXPECT_EQ(got, (IDWriteInlineObject *)io);
    if (got) IDWriteInlineObject_Release(got);
    IDWriteTextLayout_Release(l); dwx_release(io); TD_END();
}

/* ---- the application draws the object itself ----------------------------------------------------------------------- */
TEST(inline_recursion_passes_arguments)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r); r->recurse_inline = 1;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE); dwx_effect *e = dwx_effect_new(4);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)); IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(1, 1));
    DRAW_OK(l, r, 2.0f, 3.0f);
    EXPECT_EQ(io->n_draw, 1);
    const dwx_draw *c = IL(r, 0);
    if (c) { EXPECT_EQ(io->last_ctx, CTX_A); EXPECT_EQ(io->last_effect, (IUnknown *)e); CHECK_FBITS(io->last_x, c->x); CHECK_FBITS(io->last_y, c->y); EXPECT_EQ(io->last_sideways, c->obj_sideways); EXPECT_EQ(io->last_rtl, c->obj_rtl); EXPECT_EQ(io->last_renderer, (IDWriteTextRenderer *)r); }
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(io); dwx_release(e); TD_END();
}
TEST(inline_object_draw_failure_propagates_from_renderer_only)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD(); r->recurse_inline = 1;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE); io->fail_draw = E_FAIL;
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    HRESULT hr = td_draw(l, r, CTX_A, 0.0f, 0.0f);
    CHECK_HR(hr, S_OK);              /* the renderer's DrawInlineObject result is ignored by Draw */
    EXPECT_EQ(io->n_draw, 1);
    DONE_IO(l, r, io);
}

/* ---- metrics validation: how failing objects behave --------------------------------------------------------------- */
TEST(inline_metrics_failure_visible_at_layout)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f);
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE); io->fail_metrics = E_FAIL;
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DWRITE_TEXT_METRICS m; HRESULT hr = IDWriteTextLayout_GetMetrics(l, &m);
    t_note("GetMetrics with failing inline object -> %s", dw_hr_name(hr));
    IDWriteTextLayout_Release(l); dwx_release(io); TD_END();
}
TEST(inline_zero_size_object)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(0.0f, 0.0f, 0.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 1);
    const dwx_draw *c = IL(r, 0);
    if (c) CHECK_NEARF(c->x, 7.8125);
    DONE_IO(l, r, io);
}
TEST(inline_wide_object_size_table)
{
    static const float w[] = { 1.0f, 7.5f, 50.0f, 333.25f, 900.0f };
    TD_BEGIN(); size_t i;
    for (i = 0; i < sizeof w / sizeof w[0]; i++) {
        TD_LAYOUT(l, L"A#B", 16.0f, 100000.0f, 100.0f); RD(); NOSNAP(r);
        dwx_inline *io = dwx_inline_new(w[i], 12.0f, 8.0f, FALSE);
        IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
        DRAW_OK(l, r, 0.0f, 0.0f);
        const dwx_draw *b = td_run_at(r, 2);
        if (b) CHECK_NEARF(b->x, 7.8125 + w[i]);
        DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m);
        CHECK_NEARF(m.width, 2 * 7.8125f + w[i]);
        IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(io);
    }
    TD_END();
}
TEST(inline_sideways_support_flag_horizontal)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, TRUE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = IL(r, 0);
    if (c) EXPECT_EQ(c->obj_sideways, FALSE);
    DONE_IO(l, r, io);
}
TEST(inline_break_conditions_requested)
{
    TD_BEGIN(); TD_LAYOUT(l, L"A#B", 16.0f, 1000.0f, 100.0f); RD();
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    io->before = DWRITE_BREAK_CONDITION_CAN_BREAK; io->after = DWRITE_BREAK_CONDITION_MAY_NOT_BREAK;
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    t_note("GetBreakConditions called %ld time(s)", io->n_break);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 1);
    DONE_IO(l, r, io);
}
TEST(inline_break_never_keeps_with_neighbors)
{
    /* MAY_NOT_BREAK on both sides forbids a wrap right before / after the object */
    TD_BEGIN(); TD_LAYOUT(l, L"AAAA#BBBB", 16.0f, 60.0f, 300.0f); RD(); NOSNAP(r);
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    io->before = DWRITE_BREAK_CONDITION_MAY_NOT_BREAK; io->after = DWRITE_BREAK_CONDITION_MAY_NOT_BREAK;
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(4, 1));
    DRAW_OK(l, r, 0.0f, 0.0f);
    const dwx_draw *c = IL(r, 0), *a = td_run_at(r, 0);
    if (c && a) { t_note("object at (%g,%g), first run at y=%g, lines %u", c->x, c->y, a->y, td_lines(l)); EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 1); }
    DONE_IO(l, r, io);
}

/* ---- trimming sign (an inline object created by the factory) -------------------------------------------------------- */
static IDWriteTextLayout *trimmed_layout(td_fx *x, IDWriteInlineObject **sign)
{
    IDWriteTextFormat *t = td_format(x, TD_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    *sign = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(x->f, L"ABCDEFGHIJKLMNOP", 16, t, 60.0f, 100.0f, &l))) l = NULL;
    if (l) {
        IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP);
        if (FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(x->f, t, sign)) || !*sign || FAILED(IDWriteTextLayout_SetTrimming(l, &tr, *sign))) { DW_RELEASE(*sign); IDWriteTextLayout_Release(l); l = NULL; }
    }
    IDWriteTextFormat_Release(t);
    return l;
}
TEST(trim_ellipsis_sign_drawn_as_inline_object)
{
    TD_BEGIN(); IDWriteInlineObject *sign; IDWriteTextLayout *l = trimmed_layout(&X, &sign);
    if (!l) { TD_END(); t_skip("trimming setup failed"); return; }
    RD(); NOSNAP(r);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 1);
    const dwx_draw *c = IL(r, 0);
    if (c) {
        CHECK(c->obj != NULL);
        t_note("trimming sign passed to DrawInlineObject is %s the object given to SetTrimming", c->obj == sign ? "the same as" : "a different object from");
        DWRITE_INLINE_OBJECT_METRICS m; memset(&m, 0, sizeof m);
        if (c->obj) CHECK_HR_OK(IDWriteInlineObject_GetMetrics(c->obj, &m));
        CHECK(m.width > 0.0f);
        /* trimmed text plus ellipsis fits the layout width */
        float used = 0; int i;
        for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) used += dwx_run_width(&r->calls[i]);
        CHECK(used + m.width <= 60.0f + 0.01f);
        CHECK(used > 0);
        CHECK(c->x >= used - 0.01f);
        t_note("trimmed run width %g, sign width %g at x=%g", used, m.width, c->x);
    }
    DW_RELEASE(sign); do { IDWriteTextLayout_Release(l); dwx_release(r); TD_END(); } while (0);
}
TEST(trim_ellipsis_sign_draw_reaches_renderer)
{
    TD_BEGIN(); IDWriteInlineObject *sign; IDWriteTextLayout *l = trimmed_layout(&X, &sign);
    if (!l) { TD_END(); t_skip("trimming setup failed"); return; }
    RD(); NOSNAP(r); r->recurse_inline = 1;
    DRAW_OK(l, r, 0.0f, 0.0f);
    int i, before = 0, after = 0, seen_inline = 0;
    for (i = 0; i < r->ncalls; i++) { if (r->calls[i].kind == DWX_DRAW_INLINE) seen_inline = 1; else if (r->calls[i].kind == DWX_DRAW_GLYPHRUN) { if (seen_inline) after++; else before++; } }
    CHECK(before >= 1);
    CHECK(after >= 1);           /* the sign draws its own glyphs through the renderer */
    for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && r->calls[i].has_desc && r->calls[i].text_len == 1 && r->calls[i].text[0] == 0x2026) t_note("ellipsis glyph %u advance %g em %g", r->calls[i].indices[0], r->calls[i].advances[0], r->calls[i].em);
    DW_RELEASE(sign); do { IDWriteTextLayout_Release(l); dwx_release(r); TD_END(); } while (0);
}
TEST(trim_no_sign_drawn_when_fits)
{
    TD_BEGIN(); IDWriteInlineObject *sign; IDWriteTextLayout *l = trimmed_layout(&X, &sign);
    if (!l) { TD_END(); t_skip("trimming setup failed"); return; }
    RD(); IDWriteTextLayout_SetMaxWidth(l, 1000.0f);
    DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(td_count(r, DWX_DRAW_INLINE), 0);
    EXPECT_EQ(td_total_glyphs(r), 16);
    DW_RELEASE(sign); do { IDWriteTextLayout_Release(l); dwx_release(r); TD_END(); } while (0);
}
TEST(trim_sign_refcount_balanced)
{
    TD_BEGIN(); IDWriteInlineObject *sign; IDWriteTextLayout *l = trimmed_layout(&X, &sign);
    if (!l) { TD_END(); t_skip("trimming setup failed"); return; }
    RD();
    ULONG before = dw_refcount((IUnknown *)sign);
    DRAW_OK(l, r, 0.0f, 0.0f);
    ULONG after = dw_refcount((IUnknown *)sign);
    EXPECT_EQ(after, before);
    DW_RELEASE(sign); do { IDWriteTextLayout_Release(l); dwx_release(r); TD_END(); } while (0);
}
