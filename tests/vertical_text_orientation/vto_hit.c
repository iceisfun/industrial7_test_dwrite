/* Hit testing in vertical layouts. Hit-test coordinates are world coordinates: x runs across the lines, y along them.
 * For Latin text (horizontal advances on the vertical inline axis) a vertical layout answers HitTestPoint / HitTestTextPosition / HitTestTextRange
 * exactly like the horizontal layout of the same text and wrapping length, with the roles of the axes exchanged:
 *   vertical (x, y)  <->  horizontal (inline = y, flow = x mirrored for flow RIGHT_TO_LEFT).
 * The line box of line k in a vertical layout with flow RIGHT_TO_LEFT spans x in [W - top_k - h_k, W - top_k] (top_k = sum of the heights before it),
 * with flow LEFT_TO_RIGHT x in [top_k, top_k + h_k] (paragraph alignment NEAR).
 */
#include "vto.h"

#define A16 7.8125f

typedef struct hp { UINT32 pos; BOOL trailing, inside; DWRITE_HIT_TEST_METRICS m; HRESULT hr; } hp;
static hp hit(IDWriteTextLayout *l, float x, float y)
{
    hp h; memset(&h, 0, sizeof h);
    h.hr = IDWriteTextLayout_HitTestPoint(l, x, y, &h.trailing, &h.inside, &h.m); h.pos = h.m.textPosition;
    return h;
}
/* x (vertical layout, flow RTL) of the middle of line k */
static float vmid_x(IDWriteTextLayout *l, float W, UINT32 k, int flow_ltr)
{
    vt_lines ls; UINT32 i; float top = 0;
    vt_get_lines(l, &ls);
    for (i = 0; i < k && i < ls.n; i++) top += ls.lm[i].height;
    return flow_ltr ? top + ls.lm[k].height / 2 : W - (top + ls.lm[k].height / 2);
}
static float hmid_y(IDWriteTextLayout *l, UINT32 k)
{
    vt_lines ls; UINT32 i; float top = 0;
    vt_get_lines(l, &ls);
    for (i = 0; i < k && i < ls.n; i++) top += ls.lm[i].height;
    return top + ls.lm[k].height / 2;
}

TEST(point_inside_each_character_single_line)
{
    VT_BEGIN(); float W = 100; VT_VL(l, VT_FAM, L"ABCD", 16.0f, W, 200); float x = vmid_x(l, W, 0, 0); int i;
    for (i = 0; i < 4; i++) {
        hp lead = hit(l, x, i * A16 + A16 * 0.25f), trail = hit(l, x, i * A16 + A16 * 0.75f);
        CHECK_HR_OK(lead.hr); CHECK_HR_OK(trail.hr);
        CHECKF(lead.pos == (UINT32)i && lead.inside && !lead.trailing, "char %d lead: pos %u inside %d trailing %d", i, lead.pos, lead.inside, lead.trailing);
        CHECKF(trail.pos == (UINT32)i && trail.inside && trail.trailing, "char %d trail: pos %u inside %d trailing %d", i, trail.pos, trail.inside, trail.trailing);
        CHECK_NEARF(lead.m.top, i * A16); CHECK_NEARF(lead.m.height, A16); EXPECT_EQ(lead.m.length, 1);
    }
    DW_RELEASE(l); VT_END();
}
TEST(point_beyond_the_end_of_the_line)
{
    VT_BEGIN(); float W = 100; VT_VL(l, VT_FAM, L"ABCD", 16.0f, W, 200); float x = vmid_x(l, W, 0, 0); hp h = hit(l, x, 150.0f);
    CHECK_HR_OK(h.hr); CHECK(!h.inside); EXPECT_EQ(h.pos, 3); CHECK(h.trailing);
    DW_RELEASE(l); VT_END();
}
TEST(point_before_the_start_of_the_line)
{
    VT_BEGIN(); float W = 100; VT_VL(l, VT_FAM, L"ABCD", 16.0f, W, 200); float x = vmid_x(l, W, 0, 0); hp h = hit(l, x, -20.0f);
    CHECK_HR_OK(h.hr); CHECK(!h.inside); EXPECT_EQ(h.pos, 0); CHECK(!h.trailing);
    DW_RELEASE(l); VT_END();
}
TEST(point_outside_the_line_box_across_the_flow)
{
    VT_BEGIN(); float W = 100; VT_VL(l, VT_FAM, L"ABCD", 16.0f, W, 200); hp left = hit(l, -50.0f, 1.5f * A16), right = hit(l, W + 50.0f, 1.5f * A16);
    CHECK_HR_OK(left.hr); CHECK_HR_OK(right.hr);
    CHECK(!left.inside); CHECK(!right.inside); EXPECT_EQ(left.pos, 1); EXPECT_EQ(right.pos, 1);
    DW_RELEASE(l); VT_END();
}
TEST(point_matches_horizontal_hit_test_single_line)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCDEF", 16.0f, W, 300); VT_HL(h, VT_FAM, L"ABCDEF", 16.0f, 300, W); float vx = vmid_x(v, W, 0, 0), hy = hmid_y(h, 0); int i;
    for (i = 0; i < 40; i++) {
        float p = i * 1.3f - 3.0f; hp a = hit(v, vx, p), b = hit(h, p, hy);
        CHECKF(a.pos == b.pos && a.trailing == b.trailing && a.inside == b.inside, "inline %g: vertical (%u,%d,%d) horizontal (%u,%d,%d)", p, a.pos, a.trailing, a.inside, b.pos, b.trailing, b.inside);
    }
    DW_RELEASE(v); DW_RELEASE(h); VT_END();
}
TEST(point_matches_horizontal_hit_test_wrapped_lines_rtl_flow)
{
    VT_BEGIN(); float W = 200; VT_VL(v, VT_FAM, L"AB CD EF GH IJ", 16.0f, W, 40); VT_HL(h, VT_FAM, L"AB CD EF GH IJ", 16.0f, 40, W); vt_lines lv; UINT32 k; int i;
    vt_get_lines(v, &lv); CHECK(lv.n >= 3);
    for (k = 0; k < lv.n; k++) for (i = 0; i < 12; i++) {
        float p = i * 3.7f; hp a = hit(v, vmid_x(v, W, k, 0), p), b = hit(h, p, hmid_y(h, k));
        CHECKF(a.pos == b.pos && a.trailing == b.trailing, "line %u inline %g: vertical (%u,%d) horizontal (%u,%d)", k, p, a.pos, a.trailing, b.pos, b.trailing);
    }
    DW_RELEASE(v); DW_RELEASE(h); VT_END();
}
TEST(point_matches_horizontal_hit_test_wrapped_lines_ltr_flow)
{
    VT_BEGIN(); float W = 200; IDWriteTextLayout *v = vt_layout_(&X, VT_FAM, L"AB CD EF GH IJ", 16.0f, W, 40, RD_TTB, FD_LTR); VT_HL(h, VT_FAM, L"AB CD EF GH IJ", 16.0f, 40, W); vt_lines lv; UINT32 k; int i;
    if (!v) { DW_RELEASE(h); VT_END(); t_skip("layout"); return; }
    vt_get_lines(v, &lv); CHECK(lv.n >= 3);
    for (k = 0; k < lv.n; k++) for (i = 0; i < 12; i++) {
        float p = i * 3.7f; hp a = hit(v, vmid_x(v, W, k, 1), p), b = hit(h, p, hmid_y(h, k));
        CHECKF(a.pos == b.pos && a.trailing == b.trailing, "line %u inline %g: vertical (%u,%d) horizontal (%u,%d)", k, p, a.pos, a.trailing, b.pos, b.trailing);
    }
    DW_RELEASE(v); DW_RELEASE(h); VT_END();
}
TEST(text_position_box_matches_horizontal_with_swapped_axes)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCD", 16.0f, W, 200); VT_HL(h, VT_FAM, L"ABCD", 16.0f, 200, W); int i; vt_lines lv;
    vt_get_lines(v, &lv);
    for (i = 0; i < 4; i++) {
        float vx, vy, hx, hy; DWRITE_HIT_TEST_METRICS mv, mh;
        CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, i, FALSE, &vx, &vy, &mv)); CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(h, i, FALSE, &hx, &hy, &mh));
        CHECK_NEARF(mv.top, mh.left); CHECK_NEARF(mv.height, mh.width); EXPECT_EQ(mv.textPosition, mh.textPosition); EXPECT_EQ(mv.length, mh.length);
        CHECK_NEARF(mv.width, lv.lm[0].height); CHECK_NEARF(mv.left, W - lv.lm[0].height);
        t_note("char %d: vertical caret (%g,%g) box (%g,%g %gx%g); horizontal caret (%g,%g)", i, vx, vy, mv.left, mv.top, mv.width, mv.height, hx, hy);
    }
    DW_RELEASE(v); DW_RELEASE(h); VT_END();
}
TEST(text_position_trailing_edge_is_next_leading_edge)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCD", 16.0f, W, 200); int i;
    for (i = 0; i < 3; i++) {
        float x0, y0, x1, y1; DWRITE_HIT_TEST_METRICS m0, m1;
        CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, i, TRUE, &x0, &y0, &m0)); CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, i + 1, FALSE, &x1, &y1, &m1));
        CHECK_NEARF(y0, y1); CHECK_NEARF(y0, (i + 1) * A16); CHECK_NEARF(x0, x1);
    }
    DW_RELEASE(v); VT_END();
}
TEST(text_position_caret_runs_along_y)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCD", 16.0f, W, 200); int i; float x0 = 0;
    for (i = 0; i < 5; i++) {
        float x, y; DWRITE_HIT_TEST_METRICS m; CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, i, FALSE, &x, &y, &m));
        CHECK_NEARF(y, i * A16);
        if (i == 0) x0 = x; else CHECK_NEARF(x, x0);                 /* the caret stays on the same side of the line */
    }
    DW_RELEASE(v); VT_END();
}
TEST(text_position_end_of_text)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCD", 16.0f, W, 200); float x, y; DWRITE_HIT_TEST_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, 4, FALSE, &x, &y, &m));
    CHECK_NEARF(y, 4 * A16);
    DW_RELEASE(v); VT_END();
}
TEST(text_position_second_line_box)
{
    VT_BEGIN(); float W = 200; VT_VL(v, VT_FAM, L"AB CD EF", 16.0f, W, 25); vt_lines ls; float x, y; DWRITE_HIT_TEST_METRICS m;
    vt_get_lines(v, &ls); CHECK(ls.n >= 2);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, ls.lm[0].length, FALSE, &x, &y, &m));
    CHECK_NEARF(m.top, 0.0f); CHECK_NEARF(m.width, ls.lm[1].height); CHECK_NEARF(m.left, W - ls.lm[0].height - ls.lm[1].height);
    DW_RELEASE(v); VT_END();
}
TEST(text_range_single_box)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCDEF", 16.0f, W, 200); DWRITE_HIT_TEST_METRICS m[4]; UINT32 n = 0; vt_lines ls;
    vt_get_lines(v, &ls);
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(v, 1, 3, 0, 0, m, 4, &n));
    EXPECT_EQ(n, 1);
    if (n == 1) { EXPECT_EQ(m[0].textPosition, 1); EXPECT_EQ(m[0].length, 3); CHECK_NEARF(m[0].top, A16); CHECK_NEARF(m[0].height, 3 * A16); CHECK_NEARF(m[0].width, ls.lm[0].height); CHECK_NEARF(m[0].left, W - ls.lm[0].height); }
    DW_RELEASE(v); VT_END();
}
TEST(text_range_origin_offsets_both_axes)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCDEF", 16.0f, W, 200); DWRITE_HIT_TEST_METRICS a, b; UINT32 n;
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(v, 1, 3, 0, 0, &a, 1, &n)); CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(v, 1, 3, 10.0f, 20.0f, &b, 1, &n));
    CHECK_NEARF(b.left - a.left, 10.0f); CHECK_NEARF(b.top - a.top, 20.0f); CHECK_NEARF(b.width, a.width); CHECK_NEARF(b.height, a.height);
    DW_RELEASE(v); VT_END();
}
TEST(text_range_across_wrapped_lines_gives_one_box_per_line)
{
    VT_BEGIN(); float W = 200; VT_VL(v, VT_FAM, L"AB CD EF GH", 16.0f, W, 30); VT_HL(h, VT_FAM, L"AB CD EF GH", 16.0f, 30, W); DWRITE_HIT_TEST_METRICS mv[8], mh[8]; UINT32 nv = 0, nh = 0, i;
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(v, 0, 11, 0, 0, mv, 8, &nv)); CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(h, 0, 11, 0, 0, mh, 8, &nh));
    EXPECT_EQ(nv, nh); CHECK(nv >= 2);
    for (i = 0; i < nv && i < nh; i++) { EXPECT_EQ(mv[i].textPosition, mh[i].textPosition); EXPECT_EQ(mv[i].length, mh[i].length); CHECK_NEARF(mv[i].top, mh[i].left); CHECK_NEARF(mv[i].height, mh[i].width); CHECK_NEARF(mv[i].width, mh[i].height); }
    DW_RELEASE(v); DW_RELEASE(h); VT_END();
}
TEST(text_range_buffer_protocol_vertical)
{
    VT_BEGIN(); float W = 200; VT_VL(v, VT_FAM, L"AB CD EF GH", 16.0f, W, 30); DWRITE_HIT_TEST_METRICS m[8]; UINT32 n = 99, total;
    CHECK_HR(IDWriteTextLayout_HitTestTextRange(v, 0, 11, 0, 0, NULL, 0, &n), DW_E_NOT_SUFFICIENT_BUFFER); total = n; CHECK(total >= 2);
    n = 99; CHECK_HR(IDWriteTextLayout_HitTestTextRange(v, 0, 11, 0, 0, m, 1, &n), DW_E_NOT_SUFFICIENT_BUFFER); EXPECT_EQ(n, total);
    n = 0; CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(v, 0, 11, 0, 0, m, 8, &n)); EXPECT_EQ(n, total);
    DW_RELEASE(v); VT_END();
}
TEST(han_cluster_boxes_use_vertical_advance)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, HAN4, 16.0f, W, 200); int i; const float V = 11.71875f;
    for (i = 0; i < 4; i++) { float x, y; DWRITE_HIT_TEST_METRICS m; CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, i, FALSE, &x, &y, &m)); CHECK_NEARF(m.top, i * V); CHECK_NEARF(m.height, V); }
    { hp h = hit(v, W - 5.0f, 2 * V + 1.0f); CHECK_HR_OK(h.hr); EXPECT_EQ(h.pos, 2); CHECK(h.inside); }
    DW_RELEASE(v); VT_END();
}
TEST(hit_test_after_flow_change_uses_new_geometry)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCD", 16.0f, W, 200); float x0, y0, x1, y1; DWRITE_HIT_TEST_METRICS a, b;
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, 0, FALSE, &x0, &y0, &a));
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(v, FD_LTR));
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextPosition(v, 0, FALSE, &x1, &y1, &b));
    CHECK_NEARF(a.left, W - a.width); CHECK_NEARF(b.left, 0.0f); CHECK_NEARF(a.top, b.top);
    DW_RELEASE(v); VT_END();
}
TEST(hit_test_bad_arguments_vertical)
{
    VT_BEGIN(); float W = 100; VT_VL(v, VT_FAM, L"ABCD", 16.0f, W, 200); float x = 5, y = 5; DWRITE_HIT_TEST_METRICS m; BOOL t, in;
    { HRESULT hr = IDWriteTextLayout_HitTestTextPosition(v, 100, FALSE, &x, &y, &m); t_note("HitTestTextPosition(100): %s", dw_hr_name(hr)); }
    t_note("HitTestTextPosition(100) in a 4 character vertical layout: caret (%g,%g) box (%g,%g %gx%g) length %u", x, y, m.left, m.top, m.width, m.height, m.length);
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(v, 1e6f, 1e6f, &t, &in, &m)); CHECK(!in);
    CHECK_HR_OK(IDWriteTextLayout_HitTestPoint(v, -1e6f, -1e6f, &t, &in, &m)); CHECK(!in);
    DW_RELEASE(v); VT_END();
}
