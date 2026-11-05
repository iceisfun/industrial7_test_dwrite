/* IDWriteTextRenderer1 entry points and vertical layouts.
 *
 * TextLayout::Draw asks the renderer for IDWriteTextRenderer1; when it is available every drawing callback goes to the oriented overload
 * (also for horizontal layouts).  The angle of DrawUnderline / DrawStrikethrough is the layout's vertical flag (0 for horizontal text,
 * DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES for a vertical layout); DrawGlyphRun / DrawInlineObject carry the span's own orientation.
 * A vertical layout drawn with a renderer that does not implement IDWriteTextRenderer1 fails with DWRITE_E_TEXTRENDERERINCOMPATIBLE
 * (0x8898500A) before any other callback.
 */
#include "r2.h"

#define E_INCOMPAT ((HRESULT)0x8898500A)

static IDWriteTextLayout *vlayout(r2_fx *x, const WCHAR *text, DWRITE_READING_DIRECTION rd, DWRITE_FLOW_DIRECTION fd)
{
    IDWriteTextLayout *l = r2_layout_w(x, text, 16.0f, 100.0f, 200.0f, DWRITE_FONT_WEIGHT_NORMAL);
    if (!l) return NULL;
    if (FAILED(IDWriteTextLayout_SetReadingDirection(l, rd)) || FAILED(IDWriteTextLayout_SetFlowDirection(l, fd))) { IDWriteTextLayout_Release(l); return NULL; }
    return l;
}
static const struct { DWRITE_READING_DIRECTION rd; DWRITE_FLOW_DIRECTION fd; const char *name; } k_vert[] = {
    { DWRITE_READING_DIRECTION_TOP_TO_BOTTOM, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT, "top-to-bottom / flow right-to-left" },
    { DWRITE_READING_DIRECTION_TOP_TO_BOTTOM, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT, "top-to-bottom / flow left-to-right" },
    { DWRITE_READING_DIRECTION_BOTTOM_TO_TOP, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT, "bottom-to-top / flow right-to-left" },
    { DWRITE_READING_DIRECTION_BOTTOM_TO_TOP, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT, "bottom-to-top / flow left-to-right" },
};
#define NVERT 4

RT(r1_vertical_plain_renderer_fails_before_any_other_callback)
{
    int i;
    R2_BEGIN();
    for (i = 0; i < NVERT; i++) {
        IDWriteTextLayout *l = vlayout(&X, L"ABCD", k_vert[i].rd, k_vert[i].fd); rh *r = rh_new();
        if (!l) { rh_free(r); t_check(0, __FILE__, __LINE__, "vertical layout", "%s", k_vert[i].name); continue; }
        CHECKF(rh_draw(l, r, CTX_A, 0.0f, 0.0f) == E_INCOMPAT, "%s: Draw returned %s", k_vert[i].name, dw_hr_name(rh_draw(l, r, CTX_A, 0.0f, 0.0f)));
        rh_clear(r);
        CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_INCOMPAT);
        EXPECT_EQ(r->nev, 1);                                    /* only the QueryInterface for IDWriteTextRenderer1 */
        EXPECT_EQ(r->count[RH_QI], 1); EXPECT_EQ(rh_qi_asked(r, &IID_IDWriteTextRenderer1), 1);
        rh_free(r); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
RT(r1_vertical_incompatible_failure_leaves_renderer_unreferenced)
{
    R2_BEGIN(); IDWriteTextLayout *l = vlayout(&X, L"ABCD", DWRITE_READING_DIRECTION_TOP_TO_BOTTOM, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT); rh *r = rh_new();
    if (!l) { rh_free(r); R2_END(); t_skip("vertical layout unavailable"); return; }
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_INCOMPAT);
    EXPECT_EQ(r->ref, 1); EXPECT_EQ(r->addref, 0);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(r1_vertical_with_renderer1_draws)
{
    int i;
    R2_BEGIN();
    for (i = 0; i < NVERT; i++) {
        IDWriteTextLayout *l = vlayout(&X, L"ABCD", k_vert[i].rd, k_vert[i].fd); rh *r = rh_new();
        if (!l) { rh_free(r); continue; }
        r->allow_r1 = 1;
        CHECKF(rh_draw(l, r, CTX_A, 0.0f, 0.0f) == S_OK, "%s", k_vert[i].name);
        CHECKF(r->count[RH_GLYPH] >= 1, "%s: no glyph run", k_vert[i].name);
        EXPECT_EQ(r->count[RH_SNAP], 1); EXPECT_EQ(r->count[RH_PPD], 1); EXPECT_EQ(r->count[RH_XFORM], 1);
        { int k; for (k = 0; k < r->nev; k++) if (r->ev[k].type >= RH_GLYPH) { CHECK(r->ev[k].r1); CHECK((unsigned)r->ev[k].angle < 4); } }
        rh_free(r); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
RT(r1_vertical_underline_angle_is_90)
{
    int i;
    R2_BEGIN();
    for (i = 0; i < NVERT; i++) {
        IDWriteTextLayout *l = vlayout(&X, L"ABCD", k_vert[i].rd, k_vert[i].fd); rh *r = rh_new(); const rh_ev *u;
        if (!l) { rh_free(r); continue; }
        r->allow_r1 = 1; IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4));
        CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
        u = rh_nth(r, RH_UNDER, 0);
        CHECKF(u != NULL, "%s: no underline", k_vert[i].name);
        if (u) {
            CHECKF(u->r1 && u->angle == DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES, "%s: angle %d", k_vert[i].name, u->angle);
            EXPECT_EQ(u->ul.readingDirection, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);   /* both vertical reading directions are reported as top-to-bottom */ EXPECT_EQ(u->ul.flowDirection, k_vert[i].fd);
            CHECK_NEARF(u->ul.width, 4 * 7.8125f);
        }
        rh_free(r); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
RT(r1_vertical_strikethrough_angle_is_90)
{
    int i;
    R2_BEGIN();
    for (i = 0; i < NVERT; i++) {
        IDWriteTextLayout *l = vlayout(&X, L"ABCD", k_vert[i].rd, k_vert[i].fd); rh *r = rh_new(); const rh_ev *s;
        if (!l) { rh_free(r); continue; }
        r->allow_r1 = 1; IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
        CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
        s = rh_nth(r, RH_STRIKE, 0);
        CHECKF(s != NULL, "%s: no strikethrough", k_vert[i].name);
        if (s) {
            CHECKF(s->r1 && s->angle == DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES, "%s: angle %d", k_vert[i].name, s->angle);
            EXPECT_EQ(s->st.readingDirection, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM); EXPECT_EQ(s->st.flowDirection, k_vert[i].fd);
        }
        rh_free(r); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
RT(r1_horizontal_decoration_angles_are_zero_even_in_rtl_paragraph)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABCD", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->allow_r1 = 1;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *u = rh_nth(r, RH_UNDER, 0), *s = rh_nth(r, RH_STRIKE, 0), *g = rh_nth(r, RH_GLYPH, 0);
      if (u && s && g) {
        EXPECT_EQ(u->angle, 0); EXPECT_EQ(s->angle, 0); EXPECT_EQ(g->angle, 0);
        EXPECT_EQ(u->r1, 1); EXPECT_EQ(s->r1, 1); EXPECT_EQ(g->r1, 1);
        /* Win10 reports the decoration reading direction of the text run (its bidi level), not the paragraph's: Latin text stays left-to-right here */
        EXPECT_EQ(u->ul.readingDirection, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        EXPECT_EQ(s->st.readingDirection, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        EXPECT_EQ(u->ul.flowDirection, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM); EXPECT_EQ(s->st.flowDirection, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
      } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(r1_decoration_reading_direction_follows_run_level_not_paragraph)
{
    static const wchar_t *tx[2] = { L"ABCD", L"\x05d0\x05d1\x05d2\x05d3" }; int a, d, r1;
    R2_BEGIN();
    for (a = 0; a < 2; a++) for (d = 0; d < 2; d++) for (r1 = 0; r1 < 2; r1++) {
        IDWriteTextLayout *l = r2_layout_w(&X, tx[a], 16.0f, 200.0f, 50.0f, DWRITE_FONT_WEIGHT_NORMAL); rh *r; if (!l) { CHECK(0); continue; }
        r = rh_new(); r->allow_r1 = r1;
        IDWriteTextLayout_SetReadingDirection(l, d ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
        RH_DRAW_OK(l, r, 0.0f, 0.0f);
        { const rh_ev *u = rh_nth(r, RH_UNDER, 0), *s = rh_nth(r, RH_STRIKE, 0);
          if (u && s) {   /* Hebrew runs are level 1 in either paragraph direction; Latin runs level 0 */
            DWRITE_READING_DIRECTION want = a ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
            CHECKF(u->ul.readingDirection == want && s->st.readingDirection == want, "text %d para-rtl %d r1 %d: underline %d strikethrough %d want %d", a, d, r1, (int)u->ul.readingDirection, (int)s->st.readingDirection, (int)want);
            EXPECT_EQ(u->ul.flowDirection, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM); EXPECT_EQ(s->st.flowDirection, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
          } else CHECK(0); }
        rh_free(r); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
RT(r1_horizontal_inline_object_angle_zero)
{
    R2_BEGIN(); R2_LAYOUT(l, L"A#B", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->allow_r1 = 1;
    dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *in = rh_nth(r, RH_INLINE, 0);
      if (in) { EXPECT_EQ(in->r1, 1); EXPECT_EQ(in->angle, 0); EXPECT_EQ(in->obj, (IDWriteInlineObject *)io); } else CHECK(0); }
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(r1_legacy_callbacks_not_used_when_renderer1_present)
{
    R2_BEGIN(); R2_LAYOUT(l, L"A#B", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->allow_r1 = 1; int k;
    dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 3));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    for (k = 0; k < r->nev; k++) if (r->ev[k].type >= RH_GLYPH) CHECKF(r->ev[k].r1, "event %d (type %d) used the legacy entry point", k, r->ev[k].type);
    CHECK(r->count[RH_GLYPH] >= 2); EXPECT_EQ(r->count[RH_INLINE], 1); CHECK(r->count[RH_UNDER] >= 1); CHECK(r->count[RH_STRIKE] >= 1);
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(r1_legacy_renderer_gets_legacy_entry_points)
{
    R2_BEGIN(); R2_LAYOUT(l, L"A#B", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); int k;
    dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 3));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    for (k = 0; k < r->nev; k++) if (r->ev[k].type >= RH_GLYPH) CHECKF(!r->ev[k].r1, "event %d (type %d) used the oriented entry point", k, r->ev[k].type);
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(r1_same_arguments_through_both_entry_points)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AB#CD", 16.0f, 200.0f, 50.0f); rh *a = rh_new(), *b = rh_new(); int i, n;
    dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE); dwx_effect *fx = dwx_effect_new(3);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(2, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 2)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(3, 2));
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)fx, RNG(0, 2));
    b->allow_r1 = 1;
    CHECK_HR_OK(rh_draw(l, a, CTX_A, 5.5f, 6.5f)); CHECK_HR_OK(rh_draw(l, b, CTX_A, 5.5f, 6.5f));
    EXPECT_EQ(a->nev - a->count[RH_QI], b->nev - b->count[RH_QI]);
    n = a->nev < b->nev ? a->nev : b->nev;
    { int ia = 0, ib = 0;
      for (;;) {
        while (ia < a->nev && a->ev[ia].type == RH_QI) ia++;
        while (ib < b->nev && b->ev[ib].type == RH_QI) ib++;
        if (ia >= a->nev || ib >= b->nev) break;
        CHECK(a->ev[ia].type == b->ev[ib].type);
        CHECK_FBITS(b->ev[ib].x, a->ev[ia].x); CHECK_FBITS(b->ev[ib].y, a->ev[ia].y);
        EXPECT_EQ(b->ev[ib].ctx, a->ev[ia].ctx); EXPECT_EQ(b->ev[ib].effect, a->ev[ia].effect);
        EXPECT_EQ(b->ev[ib].n, a->ev[ia].n); EXPECT_EQ(b->ev[ib].mm, a->ev[ia].mm);
        ia++; ib++;
      } }
    rh_free(a); rh_free(b); dwx_release(io); dwx_release(fx); IDWriteTextLayout_Release(l); R2_END();
}
RT(r1_oriented_callback_failures_ignored)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AB#CD", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); int t;
    dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(2, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 2)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(3, 2));
    r->allow_r1 = 1;
    for (t = RH_GLYPH; t <= RH_INLINE; t++) { r->ret_set[t] = 1; r->ret[t] = E_NOTIMPL; }
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    CHECK(r->count[RH_GLYPH] >= 2); EXPECT_EQ(r->count[RH_INLINE], 1); EXPECT_EQ(r->count[RH_UNDER], 1); EXPECT_EQ(r->count[RH_STRIKE], 1);
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(r1_vertical_glyph_run_angles_within_enum_and_consistent_per_script)
{
    R2_BEGIN(); IDWriteTextLayout *l = vlayout(&X, L"ABCD EFGH", DWRITE_READING_DIRECTION_TOP_TO_BOTTOM, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT); rh *r = rh_new();
    if (!l) { rh_free(r); R2_END(); t_skip("vertical layout unavailable"); return; }
    r->allow_r1 = 1;
    CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
    { int i, first = -1;
      for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH) {
        CHECK((unsigned)r->ev[i].angle < 4);
        if (first < 0) first = r->ev[i].angle; else CHECK(r->ev[i].angle == first);       /* all Latin runs rotate the same way */
      }
      t_note("Latin glyph runs in a top-to-bottom layout: angle %d (0=0, 1=90, 2=180, 3=270), sideways %d", first, r->count[RH_GLYPH] ? rh_nth(r, RH_GLYPH, 0)->sideways : -1); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(r1_vertical_snapping_protocol_once)
{
    R2_BEGIN(); IDWriteTextLayout *l = vlayout(&X, L"ABCD", DWRITE_READING_DIRECTION_TOP_TO_BOTTOM, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT); rh *r = rh_new();
    if (!l) { rh_free(r); R2_END(); t_skip("vertical layout unavailable"); return; }
    r->allow_r1 = 1;
    CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
    EXPECT_EQ(r->count[RH_SNAP], 1); EXPECT_EQ(r->count[RH_PPD], 1); EXPECT_EQ(r->count[RH_XFORM], 1);
    r->ret_set[RH_PPD] = 1; r->ret[RH_PPD] = E_NOTIMPL; rh_clear(r);
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_NOTIMPL);
    EXPECT_EQ(r->count[RH_GLYPH], 0);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
