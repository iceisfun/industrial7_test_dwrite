/* Per-range formatting as seen by a client text renderer: glyph runs are split at every change of the complete run format, each run
 * carries the font size, locale and client drawing effect of its range, decorations are emitted per decorated run, and inline objects
 * are handed to DrawInlineObject with the effect set for their range. Fixture advances: 1000 per character at size 2048.
 */
#include "tlr.h"

#define T8 L"abcdefgh"
#define BIG 2048.0f
#define CTX ((void *)(ULONG_PTR)0x5150)

static HRESULT draw(IDWriteTextLayout *l, dwx_renderer *r, float x, float y)
{
    dwx_renderer_clear(r);
    return IDWriteTextLayout_Draw(l, CTX, (IDWriteTextRenderer *)r, x, y);
}
static const dwx_draw *nth_run(dwx_renderer *r, int n)
{
    int cur = 0, i; const dwx_draw *d = NULL;
    for (i = 0; i <= n; i++) { d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur); if (!d) return NULL; }
    return d;
}

TEST(draw_unformatted_text_is_one_glyph_run)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(draw(l, r, 10.0f, 20.0f), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 1); EXPECT_EQ(r->count[DWX_DRAW_UNDERLINE], 0); EXPECT_EQ(r->count[DWX_DRAW_STRIKETHROUGH], 0); EXPECT_EQ(r->count[DWX_DRAW_INLINE], 0);
    { const dwx_draw *d = nth_run(r, 0);
      if (d) { EXPECT_EQ(d->nglyphs, 8u); CHECK_FLOAT_EQ(d->x, 10.0f); CHECK_FLOAT_EQ(d->em, BIG); CHECK(d->ctx == CTX); CHECK(d->effect == NULL);
               CHECK_FLOAT_EQ(dwx_run_width(d), 8000.0f); } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_size_range_splits_glyph_runs)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(3, 2)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 3);
    { const dwx_draw *a = nth_run(r, 0), *b = nth_run(r, 1), *c = nth_run(r, 2);
      if (a && b && c) {
          EXPECT_EQ(a->nglyphs, 3u); EXPECT_EQ(b->nglyphs, 2u); EXPECT_EQ(c->nglyphs, 3u);
          CHECK_FLOAT_EQ(a->em, BIG); CHECK_FLOAT_EQ(b->em, 1024.0f); CHECK_FLOAT_EQ(c->em, BIG);
          CHECK_FLOAT_EQ(a->x, 0.0f); CHECK_FLOAT_EQ(b->x, 3000.0f); CHECK_FLOAT_EQ(c->x, 4000.0f);
          EXPECT_EQ(a->text_pos, 0u); EXPECT_EQ(b->text_pos, 3u); EXPECT_EQ(c->text_pos, 5u);
          EXPECT_EQ(a->text_len, 3u); EXPECT_EQ(b->text_len, 2u); EXPECT_EQ(c->text_len, 3u);
      } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_range_beyond_text_end_does_not_add_runs)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(5, 1000)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(100, 5)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 2);
    { const dwx_draw *b = nth_run(r, 1); if (b) { EXPECT_EQ(b->text_pos, 5u); EXPECT_EQ(b->text_len, 3u); } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_weight_range_uses_other_face)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(2, 2)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 3);
    { const dwx_draw *a = nth_run(r, 0), *b = nth_run(r, 1), *c = nth_run(r, 2);
      if (a && b && c) {
          CHECK(a->face != NULL && b->face != NULL); CHECK(a->face != b->face);               /* distinct font faces */
          CHECK(!dw_same_identity((IUnknown *)a->face, (IUnknown *)b->face));
          CHECK_FLOAT_EQ(dwx_run_width(a), 2000.0f); CHECK_FLOAT_EQ(dwx_run_width(b), 3000.0f); CHECK_FLOAT_EQ(dwx_run_width(c), 4000.0f);
          CHECK_FLOAT_EQ(c->x, 5000.0f);
      } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_family_range_uses_other_face_and_size_kept)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, TLR_NARROW, RNG(4, 4)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 2);
    { const dwx_draw *a = nth_run(r, 0), *b = nth_run(r, 1);
      if (a && b) { CHECK_FLOAT_EQ(a->em, BIG); CHECK_FLOAT_EQ(b->em, BIG); CHECK_FLOAT_EQ(dwx_run_width(b), 2000.0f); CHECK(a->face != b->face); } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_effect_is_passed_per_run)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new(); dwx_effect *e = dwx_effect_new(9);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 4)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 3);
    { const dwx_draw *a = nth_run(r, 0), *b = nth_run(r, 1), *c = nth_run(r, 2);
      if (a && b && c) {
          CHECK(a->effect == NULL); CHECK(b->effect == (IUnknown *)e); CHECK(c->effect == NULL);
          EXPECT_EQ(b->text_pos, 2u); EXPECT_EQ(b->text_len, 4u); EXPECT_EQ(b->nglyphs, 4u);
          CHECK(b->effect_refs >= 2);                        /* the layout still holds its reference during the callback */
      } }
    IDWriteTextLayout_Release(l); dwx_release(e); TLR_END();
}
TEST(draw_two_effects_two_runs)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new(); dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(4, 4)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 2);
    { const dwx_draw *a = nth_run(r, 0), *b = nth_run(r, 1);
      if (a && b) { CHECK(a->effect == (IUnknown *)e1); CHECK(b->effect == (IUnknown *)e2); } }
    IDWriteTextLayout_Release(l); dwx_release(e1); dwx_release(e2); TLR_END();
}
TEST(draw_same_effect_adjacent_ranges_make_one_run)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new(); dwx_effect *e = dwx_effect_new(1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 4)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(4, 4)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 1);
    { const dwx_draw *a = nth_run(r, 0); if (a) { CHECK(a->effect == (IUnknown *)e); EXPECT_EQ(a->nglyphs, 8u); } }
    IDWriteTextLayout_Release(l); dwx_release(e); TLR_END();
}
TEST(draw_locale_is_reported_per_run)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetLocaleName(l, L"fr-fr", RNG(2, 3)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 3);
    { const dwx_draw *a = nth_run(r, 0), *b = nth_run(r, 1), *c = nth_run(r, 2);
      if (a && b && c) {
          CHECK(a->has_desc && b->has_desc && c->has_desc);
          if (a->has_desc && b->has_desc && c->has_desc) {
              CHECK(!_wcsicmp(a->locale, TLR_LOC)); CHECK(!_wcsicmp(b->locale, L"fr-fr")); CHECK(!_wcsicmp(c->locale, TLR_LOC));
              EXPECT_EQ(b->text_pos, 2u); EXPECT_EQ(b->text_len, 3u);
          }
      } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_run_description_text_matches_layout_text)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(3, 2)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    { const dwx_draw *b = nth_run(r, 1);
      if (b && b->has_desc && b->text) { CHECK(!wcsncmp(b->text, L"de", 2)); EXPECT_EQ(b->text_len, 2u); EXPECT_EQ(b->text_pos, 3u); } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_underline_emitted_for_underlined_run)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, TRUE, RNG(2, 3)), S_OK);
    CHECK_HR(draw(l, r, 5.0f, 7.0f), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 3); EXPECT_EQ(r->count[DWX_DRAW_UNDERLINE], 1); EXPECT_EQ(r->count[DWX_DRAW_STRIKETHROUGH], 0);
    { int cur = 0; const dwx_draw *u = dwx_renderer_next(r, DWX_DRAW_UNDERLINE, &cur);
      if (u) { CHECK_FLOAT_EQ(u->ul.width, 3000.0f); CHECK_FLOAT_EQ(u->x, 5.0f + 2000.0f); CHECK(u->ul.thickness > 0.0f); CHECK(u->ul.runHeight > 0.0f);
               CHECK(u->ctx == CTX); } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_strikethrough_emitted_for_struck_run)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(5, 3)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_UNDERLINE], 0); EXPECT_EQ(r->count[DWX_DRAW_STRIKETHROUGH], 1);
    { int cur = 0; const dwx_draw *s = dwx_renderer_next(r, DWX_DRAW_STRIKETHROUGH, &cur);
      if (s) { CHECK_FLOAT_EQ(s->st.width, 3000.0f); CHECK_FLOAT_EQ(s->x, 5000.0f); CHECK(s->st.thickness > 0.0f); } }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_underline_removed_again_draws_none)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK); EXPECT_EQ(r->count[DWX_DRAW_UNDERLINE], 1);
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, FALSE, RNG(0, 8)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK); EXPECT_EQ(r->count[DWX_DRAW_UNDERLINE], 0); EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 1);
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_underline_range_beyond_text_is_clipped)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR(IDWriteTextLayout_SetUnderline(l, TRUE, RNG(6, 1000)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK); EXPECT_EQ(r->count[DWX_DRAW_UNDERLINE], 1);
    { int cur = 0; const dwx_draw *u = dwx_renderer_next(r, DWX_DRAW_UNDERLINE, &cur); if (u) CHECK_FLOAT_EQ(u->ul.width, 2000.0f); }
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_inline_object_callback)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new(); dwx_inline *o = dwx_inline_new(300.0f, 100.0f, 80.0f, FALSE); dwx_effect *e = dwx_effect_new(3);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 1)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 1)), S_OK);
    CHECK_HR(draw(l, r, 4.0f, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_INLINE], 1);
    { int cur = 0; const dwx_draw *d = dwx_renderer_next(r, DWX_DRAW_INLINE, &cur);
      if (d) { CHECK(d->obj == (IDWriteInlineObject *)o); CHECK_FLOAT_EQ(d->x, 4.0f + 2000.0f); CHECK(d->obj_sideways == FALSE); CHECK(d->obj_rtl == FALSE);
               CHECK(d->effect == (IUnknown *)e); CHECK(d->ctx == CTX); } }
    EXPECT_EQ(o->n_draw, 0);                      /* the renderer, not the layout, decides whether to call IDWriteInlineObject::Draw */
    IDWriteTextLayout_Release(l); dwx_release(o); dwx_release(e); TLR_END();
}
TEST(draw_inline_object_splits_glyph_runs)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new(); dwx_inline *o = dwx_inline_new(300.0f, 100.0f, 80.0f, FALSE);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(3, 1)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 2); EXPECT_EQ(r->count[DWX_DRAW_INLINE], 1);
    { const dwx_draw *a = nth_run(r, 0), *b = nth_run(r, 1);
      if (a && b) { EXPECT_EQ(a->nglyphs, 3u); EXPECT_EQ(b->nglyphs, 4u); CHECK_FLOAT_EQ(b->x, 3300.0f); } }
    IDWriteTextLayout_Release(l); dwx_release(o); TLR_END();
}
TEST(draw_inline_object_removed_draws_text_again)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new(); dwx_inline *o = dwx_inline_new(300.0f, 100.0f, 80.0f, FALSE);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(3, 1)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, NULL, RNG(3, 1)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    EXPECT_EQ(r->count[DWX_DRAW_INLINE], 0); EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 1);
    IDWriteTextLayout_Release(l); dwx_release(o); TLR_END();
}
TEST(draw_two_layouts_with_different_formatting_are_independent)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); IDWriteTextLayout *l2 = tlr_layout_(&X, TLR_FAM, T8, BIG); dwx_renderer *r = dwx_renderer_new();
    if (!l2) { IDWriteTextLayout_Release(l); dwx_release(r); TLR_END(); t_skip("second layout"); return; }
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(0, 4)), S_OK);
    CHECK_HR(draw(l2, r, 0, 0), S_OK); EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 1);
    CHECK_HR(draw(l, r, 0, 0), S_OK);  EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 2);
    IDWriteTextLayout_Release(l2); IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
TEST(draw_does_not_change_ranges)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new(); DWRITE_TEXT_RANGE rg; dwx_effect *e = dwx_effect_new(1);
    CHECK_HR(IDWriteTextLayout_SetFontSize(l, 1024.0f, RNG(3, 2)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(3, 2)), S_OK);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    CHECK_FLOAT_EQ(tlr_size(l, 3, &rg), 1024.0f); CHECK_RANGE(rg, 3, 2);
    CHECK_FLOAT_EQ(tlr_size(l, 5, &rg), BIG);     CHECK_RANGE(rg, 5, TLR_ALL - 5);
    IDWriteTextLayout_Release(l); dwx_release(r); dwx_release(e); TLR_END();
}
TEST(draw_renderer_failure_formatting_survives)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T8, BIG); dwx_renderer *r = dwx_renderer_new(); DWRITE_TEXT_RANGE rg;
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 3)), S_OK);
    r->fail_draw = E_ABORT;
    t_note("Draw with a failing renderer -> %s", dw_hr_name(draw(l, r, 0, 0)));
    r->fail_draw = 0;
    CHECK(tlr_weight(l, 1, &rg) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(rg, 0, 3);
    CHECK_HR(draw(l, r, 0, 0), S_OK);
    IDWriteTextLayout_Release(l); dwx_release(r); TLR_END();
}
