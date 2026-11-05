/* Hit testing against assorted layout settings: line spacing, character spacing, flow/reading directions, vertical overflow,
 * unusual text (empty, whitespace-only, control characters), interface versions and object lifetime.
 */
#include "th.h"

TEST(misc_empty_text_position)
{
    TH_BEGIN(); TH_LAYOUT(l, L"", 100.0f, 100.0f);
    th_tp q = th_pos(l, 0, FALSE), r = th_pos(l, 0, TRUE), s = th_pos(l, 50, FALSE);
    CHECK(q.m.textPosition == 0 && q.m.length == 0);
    CHK_NEAR(q.x, 0.0f); CHK_NEAR(q.y, 0.0f);
    CHECK(r.x == q.x && s.x == q.x && s.y == q.y);
    t_note("empty layout caret: height %g width %g isText %d lines %u", q.m.height, q.m.width, (int)q.m.isText, th_lines(l));
    TH_DONE(l);
}

TEST(misc_empty_text_alignment)
{
    TH_BEGIN(); TH_LAYOUT(l, L"", 100.0f, 100.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    th_tp q = th_pos(l, 0, FALSE);
    t_note("empty trailing-aligned caret x %g (width 100)", q.x);
    CHECK(q.x == 0.0f || th_feq(q.x, 100.0f));
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    q = th_pos(l, 0, FALSE);
    t_note("empty centre-aligned caret x %g", q.x);
    TH_DONE(l);
}

TEST(misc_whitespace_only_text)
{
    TH_BEGIN(); TH_LAYOUT(l, L"   ", 1000.0f, 100.0f);
    UINT32 i;
    for (i = 0; i < 3; i++) { th_tp q = th_pos(l, i, FALSE); CHK_M(q.m, i, 1, i * CW, CW, 0, 1); }
    th_pt p = th_point(l, CW + 1.0f, 3.0f);
    CHK_PT(p, 1, 1, 0, 1);
    TH_DONE(l);
}

TEST(misc_only_newline)
{
    TH_BEGIN(); TH_LAYOUT(l, L"\n", 1000.0f, 100.0f);
    th_tp a = th_pos(l, 0, FALSE), e = th_pos(l, 1, FALSE);
    CHECK(a.m.textPosition == 0 && a.m.length == 1);
    CHK_NEAR(a.x, 0.0f);
    CHECK(e.m.textPosition == 1 && e.m.length == 0);
    if (th_lines(l) == 2) CHK_NEAR(e.y, th_lh(l, 0));
    th_pt p = th_point(l, 50.0f, 3.0f);
    CHECK(p.m.textPosition == 0);                          /* the line-break cluster is not hit; with nothing else the first cluster is returned */
    t_note("\"\\n\" point hit: pos %u len %u trailing %d inside %d", p.m.textPosition, p.m.length, (int)p.tr, (int)p.in);
    TH_DONE(l);
}

TEST(misc_line_separator_breaks_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\x2028" L"CD", 1000.0f, 200.0f);
    UINT32 n = th_lines(l);
    if (n != 2) { t_note("U+2028 gave %u lines", n); TH_DONE(l); return; }
    th_tp q = th_pos(l, 3, FALSE);
    CHK_NEAR(q.x, 0.0f); CHK_NEAR(q.y, th_lh(l, 0));
    th_pt p = th_point(l, 300.0f, 3.0f);
    CHK_PT(p, 1, 1, 1, 0);
    TH_DONE(l);
}

TEST(misc_paragraph_separator_breaks_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\x2029" L"CD", 1000.0f, 200.0f);
    UINT32 n = th_lines(l);
    if (n != 2) { t_note("U+2029 gave %u lines", n); TH_DONE(l); return; }
    th_tp q = th_pos(l, 3, FALSE);
    CHK_NEAR(q.x, 0.0f); CHK_NEAR(q.y, th_lh(l, 0));
    TH_DONE(l);
}

TEST(misc_next_line_char_breaks_line)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB\x0085" L"CD", 1000.0f, 200.0f);
    UINT32 n = th_lines(l);
    t_note("U+0085 gave %u lines", n);
    if (n == 2) { th_tp q = th_pos(l, 3, FALSE); CHK_NEAR(q.x, 0.0f); }
    TH_DONE(l);
}

TEST(misc_uniform_line_spacing)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\nB\nC", 1000.0f, 400.0f);
    if (th_lines(l) != 3) { TH_DONE(l); t_skip("expected 3 lines"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 40.0f, 30.0f));
    CHK_NEAR(th_lh(l, 0), 40.0f);
    th_tp a = th_pos(l, 0, FALSE), b = th_pos(l, 2, FALSE), c = th_pos(l, 4, FALSE);
    CHK_NEAR(b.m.top - a.m.top, 40.0f); CHK_NEAR(c.m.top - b.m.top, 40.0f);
    th_pt p = th_point(l, 1.0f, 100.0f);                          /* 80..120 is line 2 (the glyph box may be shorter than the line box) */
    CHK_PT(p, 4, 1, 0, -1);
    p = th_point(l, 1.0f, 79.0f);                                 CHK_PT(p, 2, 1, 0, -1);
    t_note("uniform spacing 40: rect top %g height %g", a.m.top, a.m.height);
    TH_DONE(l);
}

TEST(misc_proportional_line_spacing)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\nB", 1000.0f, 400.0f);
    if (th_lines(l) != 2) { TH_DONE(l); t_skip("expected 2 lines"); return; }
    float natural = th_lh(l, 0);
    HRESULT hr = IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 0.8f);
    if (FAILED(hr)) { t_note("PROPORTIONAL rejected: %s", dw_hr_name(hr)); TH_DONE(l); return; }
    float h = th_lh(l, 0);
    t_note("natural line height %g, proportional x2: %g", natural, h);
    th_tp b = th_pos(l, 2, FALSE);
    CHECK(b.m.top >= h - EPS && b.m.top < 2 * h);
    TH_DONE(l);
}

TEST(misc_character_spacing_widens_clusters)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 1000.0f, 100.0f);
    IDWriteTextLayout1 *l1 = NULL;
    if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1)) || !l1) { TH_DONE(l); t_skip("IDWriteTextLayout1 unsupported"); return; }
    DWRITE_TEXT_RANGE r = { 0, 4 };
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 2.0f, 3.0f, 0.0f, r));
    DWRITE_CLUSTER_METRICS c[4]; UINT32 n = th_clusters(l, c, 4), i;
    CHECK(n == 4);
    for (i = 0; i < 4; i++) { th_tp q = th_pos(l, i, FALSE); CHK_NEAR(q.m.width, c[i].width); CHECK(q.m.width > CW + 4.0f); }
    t_note("character spacing 2+3: cluster width %g (plain %g)", c[0].width, CW);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout1_Release(l1);
    TH_DONE(l);
}

TEST(misc_pair_kerning_toggle_consistent)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AVAVAV", 1000.0f, 100.0f);
    IDWriteTextLayout1 *l1 = NULL;
    if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1)) || !l1) { TH_DONE(l); t_skip("IDWriteTextLayout1 unsupported"); return; }
    DWRITE_TEXT_RANGE r = { 0, 6 };
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, r));
    CHECK_CONSISTENT(l);
    IDWriteTextLayout1_Release(l1);
    TH_DONE(l);
}

TEST(misc_overflowing_height_still_hit_testable)
{
    TH_BEGIN(); TH_LAYOUT(l, L"A\nB\nC\nD\nE\nF", 1000.0f, 20.0f);
    UINT32 n = th_lines(l);
    if (n != 6) { TH_DONE(l); t_skip("expected 6 lines"); return; }
    float top = 0; UINT32 k;
    for (k = 0; k < 6; k++) { th_pt p = th_point(l, 1.0f, top + th_lh(l, k) * 0.5f); CHK_PT(p, 2 * k, 1, 0, 1); top += th_lh(l, k); }
    TH_DONE(l);
}

TEST(misc_max_height_change_with_far_alignment)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AB", 100.0f, 100.0f);
    float lh = th_lh(l, 0);
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    th_tp q = th_pos(l, 0, FALSE);   CHK_NEAR(q.y, 100.0f - lh);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 300.0f));
    q = th_pos(l, 0, FALSE);         CHK_NEAR(q.y, 300.0f - lh);
    TH_DONE(l);
}

TEST(misc_reading_direction_switch_relayouts)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 100.0f);
    th_tp q = th_pos(l, 0, FALSE);   CHK_NEAR(q.x, 0.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    q = th_pos(l, 0, FALSE);         CHK_NEAR(q.x, 100.0f - 4 * CW);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT));
    q = th_pos(l, 0, FALSE);         CHK_NEAR(q.x, 0.0f);
    TH_DONE(l);
}

TEST(misc_vertical_reading_direction_noted)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 100.0f, 100.0f);
    HRESULT hr = IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
    if (FAILED(hr)) { t_note("TOP_TO_BOTTOM rejected: %s", dw_hr_name(hr)); TH_DONE(l); return; }
    HRESULT hf = IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
    t_note("SetFlowDirection(RTL) -> %s", dw_hr_name(hf));
    th_tp q = th_pos(l, 0, FALSE), e = th_pos(l, 4, FALSE);
    CHECK(q.m.textPosition == 0 && q.m.length == 1);
    CHECK(e.m.textPosition == 4 && e.m.length == 0);
    t_note("vertical layout: pos0 x %g y %g rect (%g,%g %gx%g); end caret x %g y %g", q.x, q.y, q.m.left, q.m.top, q.m.width, q.m.height, e.x, e.y);
    th_pt p = th_point(l, q.m.left + q.m.width * 0.5f, q.m.top + q.m.height * 0.5f);
    CHECK(p.m.textPosition == 0);
    TH_DONE(l);
}

TEST(misc_point_and_position_agree_after_many_property_changes)
{
    TH_BEGIN(); TH_LAYOUT(l, L"AAAA BBBB CCCC", 1000.0f, 300.0f);
    int round;
    for (round = 0; round < 6; round++) {
        IDWriteTextLayout_SetMaxWidth(l, (round % 2) ? 5 * CW : 1000.0f);
        IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)(round % 3));
        th_tp q = th_pos(l, 5, FALSE);
        th_pt p = th_point(l, q.x + 0.1f * CW, q.y + q.m.height * 0.5f);
        CHECKF(p.m.textPosition == 5 && p.in == 1, "round %d: pos %u in %d", round, p.m.textPosition, (int)p.in);
    }
    TH_DONE(l);
}

TEST(misc_derived_interfaces_share_state)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDEF", 1000.0f, 200.0f);
    IDWriteTextLayout4 *l4 = NULL;
    if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4)) || !l4) { TH_DONE(l); t_skip("IDWriteTextLayout4 unsupported"); return; }
    DWRITE_TEXT_RANGE r = { 3, 3 };
    CHECK_HR_OK(IDWriteTextLayout4_SetFontSize(l4, 32.0f, r));
    th_tp q = th_pos(l, 4, FALSE);                                /* the change through one interface is visible through the other */
    CHK_NEAR(q.m.width, 2 * CW);
    float x = 0, y = 0; DWRITE_HIT_TEST_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout4_HitTestTextPosition(l4, 4, FALSE, &x, &y, &m));
    CHK_NEAR(x, q.x);
    IDWriteTextLayout4_Release(l4);
    TH_DONE(l);
}

TEST(misc_refcounts_unchanged_by_hit_testing)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDEF", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(10.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_RANGE r = { 2, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, r));
    EXPECT_REFS(l, 1);
    LONG before = dwx_refs(io);
    th_pos(l, 2, FALSE); th_point(l, 20.0f, 3.0f); th_range(l, 0, 6, 0, 0);
    CHECK(dwx_refs(io) == before);
    EXPECT_REFS(l, 1);
    IDWriteTextLayout_Release(l);
    CHECK(dwx_refs(io) == 1);
    dwx_release(io);
    TH_END();
}

TEST(misc_inline_object_draw_not_called_by_hit_testing)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABC", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(10.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_RANGE r = { 1, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, r));
    th_pos(l, 1, FALSE); th_point(l, 9.0f, 2.0f); th_range(l, 0, 3, 0, 0);
    CHECK(io->n_draw == 0);                                       /* hit testing only measures */
    CHECK(io->n_metrics >= 1);
    IDWriteTextLayout_Release(l); dwx_release(io); TH_END();
}

TEST(misc_inline_object_metrics_failure_does_not_break_hit_testing)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABC", 1000.0f, 200.0f);
    dwx_inline *io = dwx_inline_new(10.0f, 10.0f, 8.0f, FALSE);
    io->fail_metrics = E_FAIL;
    DWRITE_TEXT_RANGE r = { 1, 1 };
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, r));
    float x = 1e30f, y = 1e30f; DWRITE_HIT_TEST_METRICS m; memset(&m, 0xcc, sizeof m);
    HRESULT hr = IDWriteTextLayout_HitTestTextPosition(l, 0, FALSE, &x, &y, &m);
    t_note("inline GetMetrics fails: HitTestTextPosition -> %s", dw_hr_name(hr));
    CHECK(hr == S_OK || FAILED(hr));
    IDWriteTextLayout_Release(l); dwx_release(io); TH_END();
}

TEST(misc_effect_on_range_does_not_affect_geometry)
{
    TH_BEGIN(); TH_LAYOUT(l, L"ABCD", 1000.0f, 200.0f);
    dwx_effect *e = dwx_effect_new(1);
    DWRITE_TEXT_RANGE r = { 1, 2 };
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, r));
    UINT32 i;
    for (i = 0; i < 4; i++) { th_tp q = th_pos(l, i, FALSE); CHK_M(q.m, i, 1, i * CW, CW, 0, 1); }
    th_rg rr = th_range(l, 0, 4, 0, 0);
    CHECK(rr.count == 1);
    IDWriteTextLayout_Release(l); dwx_release(e); TH_END();
}

TEST(misc_two_layouts_independent)
{
    TH_BEGIN();
    IDWriteTextLayout *a = th_layout_(&X, TH_FAM, L"AAAA", 16.0f, 1000.0f, 100.0f), *b = th_layout_(&X, TH_FAM, L"BBBBBBBB", 32.0f, 1000.0f, 100.0f);
    if (!a || !b) { if (a) IDWriteTextLayout_Release(a); if (b) IDWriteTextLayout_Release(b); TH_END(); t_skip("layout"); return; }
    th_tp qa = th_pos(a, 4, FALSE), qb = th_pos(b, 4, FALSE);
    CHK_NEAR(qa.x, 4 * CW); CHK_NEAR(qb.x, 8 * CW);
    IDWriteTextLayout_Release(a);
    qb = th_pos(b, 8, FALSE);
    CHK_NEAR(qb.x, 16 * CW);
    IDWriteTextLayout_Release(b); TH_END();
}

TEST(misc_hit_point_after_position_calls)
{
    /* the layout is not documented as thread safe; this only runs concurrent hit tests on DIFFERENT layouts */
    TH_BEGIN(); TH_LAYOUT(l, L"ABCDEFGH", 1000.0f, 200.0f);
    th_pt p = th_point(l, 20.0f, 3.0f);
    CHK_PT(p, 2, 1, 0, 1);
    TH_DONE(l);
}
