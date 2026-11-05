/* IDWriteInlineObject placement as seen by DrawInlineObject: bidi flag, ordering of objects and neighbouring runs in right-to-left text,
 * partially overlapping SetInlineObject ranges, objects that interrupt a ligature cluster.
 * DrawInlineObjects passes isRightToLeft = (bidi level of the object's span) & 1 and isSideways = the span's sideways flag; originX/Y is the
 * top-left corner of the object box (x grows to the right in a horizontal layout), y = line baseline - object baseline.
 */
#include "r2.h"

#define HEB1 L"\x05d0"
#define HEB2 L"\x05d1"

static const rh_ev *ev_nth(const rh *r, int type, int n) { return rh_nth(r, type, n); }

RT(inline_between_hebrew_letters_is_right_to_left)
{
    R2_BEGIN(); R2_LAYOUT(l, HEB1 L"#" HEB2, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_INLINE], 1); EXPECT_EQ(r->count[RH_GLYPH], 2);
    { const rh_ev *in = ev_nth(r, RH_INLINE, 0);
      if (in) { EXPECT_EQ(in->obj, (IDWriteInlineObject *)io); EXPECT_EQ(in->ortl, TRUE); EXPECT_EQ(in->osw, FALSE); } else CHECK(0); }
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_in_rtl_text_visual_placement)
{
    /* logical alef # bet: bet is leftmost, the object in the middle, alef rightmost; glyph runs of odd level have their origin on the right */
    R2_BEGIN(); R2_LAYOUT(l, HEB1 L"#" HEB2, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *alef = ev_nth(r, RH_GLYPH, 0), *bet = ev_nth(r, RH_GLYPH, 1), *in = ev_nth(r, RH_INLINE, 0);
      if (alef && bet && in) {
        EXPECT_EQ(alef->text[0], 0x05d0); EXPECT_EQ(bet->text[0], 0x05d1);
        CHECK_NEARF(bet->x, 7.8125f);                         /* bet occupies [0, 7.8125], origin on its right edge */
        CHECK_NEARF(in->x, 7.8125f);
        CHECK_NEARF(alef->x, 7.8125f + 20.0f + 7.8125f);
        CHECK_NEARF(in->y + 8.0f, alef->y);                   /* baselines coincide */
      } else CHECK(0); }
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_between_latin_letters_in_rtl_paragraph_is_not_flagged)
{
    R2_BEGIN(); R2_LAYOUT(l, L"A#B", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *in = ev_nth(r, RH_INLINE, 0), *a = ev_nth(r, RH_GLYPH, 0);
      if (in && a) { EXPECT_EQ(in->ortl, FALSE); EXPECT_EQ(a->bidi, 2); } else CHECK(0); }
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
static void lone_object(DWRITE_READING_DIRECTION dir, BOOL want_rtl)
{
    R2_BEGIN(); R2_LAYOUT(l, L"#", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetReadingDirection(l, dir);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(0, 1));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 0); EXPECT_EQ(r->count[RH_INLINE], 1);
    { const rh_ev *in = ev_nth(r, RH_INLINE, 0);
      if (in) { EXPECT_EQ(in->ortl, want_rtl); EXPECT_EQ(in->osw, FALSE); } else CHECK(0); }
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_lone_object_ltr_paragraph_not_rtl) { lone_object(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, FALSE); }
RT(inline_lone_object_rtl_paragraph_is_rtl) { lone_object(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT, TRUE); }
RT(inline_lone_object_rtl_paragraph_origin_at_right)
{
    R2_BEGIN(); R2_LAYOUT(l, L"#", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(0, 1));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *in = ev_nth(r, RH_INLINE, 0); if (in) CHECK_NEARF(in->x, 180.0f); else CHECK(0); }   /* box right edge minus the object width */
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_two_objects_in_rtl_text_delivered_in_logical_order)
{
    R2_BEGIN(); R2_LAYOUT(l, HEB1 L"#" HEB2 L"#" HEB1, 16.0f, 300.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *i1 = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE), *i2 = dwx_inline_new(30.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i1, RNG(1, 1));
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i2, RNG(3, 1));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_INLINE], 2); EXPECT_EQ(r->count[RH_GLYPH], 3);
    { const rh_ev *a = ev_nth(r, RH_INLINE, 0), *b = ev_nth(r, RH_INLINE, 1);
      if (a && b) {
        EXPECT_EQ(a->obj, (IDWriteInlineObject *)i1); EXPECT_EQ(b->obj, (IDWriteInlineObject *)i2);
        CHECKF(b->x < a->x, "second object (x %g) lies left of the first (x %g) in right-to-left text", b->x, a->x);
        EXPECT_EQ(a->ortl, TRUE); EXPECT_EQ(b->ortl, TRUE);
      } else CHECK(0); }
    rh_free(r); dwx_release(i1); dwx_release(i2); IDWriteTextLayout_Release(l); R2_END();
}

/* ---- overlapping ranges ------------------------------------------------------------------------------------ */
RT(inline_partial_replacement_splits_first_object)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABCDE", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *i1 = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE), *i2 = dwx_inline_new(14.0f, 12.0f, 8.0f, FALSE);
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i1, RNG(0, 5)));
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i2, RNG(2, 1)));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 0);
    EXPECT_EQ(r->count[RH_INLINE], 3);
    { const rh_ev *a = ev_nth(r, RH_INLINE, 0), *b = ev_nth(r, RH_INLINE, 1), *c = ev_nth(r, RH_INLINE, 2);
      if (a && b && c) {
        EXPECT_EQ(a->obj, (IDWriteInlineObject *)i1); EXPECT_EQ(b->obj, (IDWriteInlineObject *)i2); EXPECT_EQ(c->obj, (IDWriteInlineObject *)i1);
        CHECK_NEARF(a->x, 0.0f); CHECK_NEARF(b->x, 10.0f); CHECK_NEARF(c->x, 10.0f + 14.0f);     /* one box per call, however many characters it covers */
      } }
    { DWRITE_TEXT_METRICS m; CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m)); CHECK_NEARF(m.width, 10.0f + 14.0f + 10.0f); }
    rh_free(r); dwx_release(i1); dwx_release(i2); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_replaced_object_fully_covered_is_dropped)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABC", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *i1 = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE), *i2 = dwx_inline_new(14.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i1, RNG(1, 1));
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i2, RNG(0, 3));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_INLINE], 1);
    { const rh_ev *a = ev_nth(r, RH_INLINE, 0); if (a) EXPECT_EQ(a->obj, (IDWriteInlineObject *)i2); }
    EXPECT_REFS(i1, 1);                                          /* the layout let go of the covered object */
    rh_free(r); dwx_release(i1); dwx_release(i2); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_removal_in_the_middle_restores_text_runs)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABCDE", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *i1 = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)i1, RNG(0, 5));
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, NULL, RNG(1, 3)));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_INLINE], 2);
    EXPECT_EQ(r->count[RH_GLYPH], 1);
    { const rh_ev *g = ev_nth(r, RH_GLYPH, 0); if (g) { EXPECT_EQ(g->tlen, 3); EXPECT_EQ(g->text[0], 'B'); CHECK_NEARF(g->x, 10.0f); } else CHECK(0); }
    rh_free(r); dwx_release(i1); IDWriteTextLayout_Release(l); R2_END();
}

/* ---- interrupting a ligature cluster ------------------------------------------------------------------------ */
RT(inline_ligature_without_object_is_one_glyph_per_pair)
{
    R2_BEGIN(); IDWriteTextLayout *l = r2_layout_fam(&X, R2_SHAPE, L"ABAB", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    if (!l) { rh_free(r); R2_END(); t_skip("shape family unavailable"); return; }
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 1);
    { const rh_ev *g = ev_nth(r, RH_GLYPH, 0);
      if (g) { EXPECT_EQ(g->tlen, 4); EXPECT_EQ(g->n, 2); EXPECT_EQ(g->idx[0], 68); EXPECT_EQ(g->idx[1], 68);
               EXPECT_EQ(g->cmap[0], 0); EXPECT_EQ(g->cmap[1], 0); EXPECT_EQ(g->cmap[2], 1); EXPECT_EQ(g->cmap[3], 1); } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_object_inside_ligature_pair_prevents_ligature)
{
    R2_BEGIN(); IDWriteTextLayout *l = r2_layout_fam(&X, R2_SHAPE, L"ABAB", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    if (!l) { rh_free(r); dwx_release(io); R2_END(); t_skip("shape family unavailable"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 2); EXPECT_EQ(r->count[RH_INLINE], 1);
    { const rh_ev *a = ev_nth(r, RH_GLYPH, 0), *b = ev_nth(r, RH_GLYPH, 1), *in = ev_nth(r, RH_INLINE, 0);
      if (a && b && in) {
        EXPECT_EQ(a->tlen, 1); EXPECT_EQ(a->text[0], 'A'); EXPECT_EQ(a->n, 1); EXPECT_EQ(a->idx[0], 2);
        EXPECT_EQ(b->tlen, 2); EXPECT_EQ(b->n, 1); EXPECT_EQ(b->idx[0], 68); EXPECT_EQ(b->cmap[0], 0); EXPECT_EQ(b->cmap[1], 0);
        CHECK_NEARF(in->x, 7.8125f); CHECK_NEARF(b->x, 7.8125f + 20.0f);
      } else CHECK(0); }
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_object_before_ligature_second_letter_leaves_single_glyph_run)
{
    R2_BEGIN(); IDWriteTextLayout *l = r2_layout_fam(&X, R2_SHAPE, L"ABAB", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    if (!l) { rh_free(r); dwx_release(io); R2_END(); t_skip("shape family unavailable"); return; }
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(0, 1));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 1); EXPECT_EQ(r->count[RH_INLINE], 1);
    { const rh_ev *g = ev_nth(r, RH_GLYPH, 0);
      if (g) { EXPECT_EQ(g->tlen, 3); EXPECT_EQ(g->text[0], 'B'); EXPECT_EQ(g->n, 2); EXPECT_EQ(g->idx[0], 3); EXPECT_EQ(g->idx[1], 68);
               EXPECT_EQ(g->cmap[0], 0); EXPECT_EQ(g->cmap[1], 1); EXPECT_EQ(g->cmap[2], 1); } else CHECK(0); }
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
RT(inline_cluster_metrics_of_object_range)
{
    R2_BEGIN(); R2_LAYOUT(l, L"A###B", 16.0f, 300.0f, 50.0f);
    dwx_inline *io = dwx_inline_new(20.0f, 12.0f, 8.0f, FALSE);
    DWRITE_CLUSTER_METRICS cm[8]; UINT32 n = 0, i, total = 0; float w = 0;
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 3));
    CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(l, cm, 8, &n));
    for (i = 0; i < n; i++) { total += cm[i].length; w += cm[i].width; }
    EXPECT_EQ(total, 5);
    CHECK_NEARF(w, 2 * 7.8125f + 20.0f);                         /* one 20 wide object for the whole 3 character range */
    t_note("clusters for 'A' + 3 object characters + 'B': %u, lengths %u %u %u %u", n, n > 0 ? cm[0].length : 0, n > 1 ? cm[1].length : 0, n > 2 ? cm[2].length : 0, n > 3 ? cm[3].length : 0);
    dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}

/* ---- vertical placement against the line baseline --------------------------------------------------------------- */
static const struct { float h, b; } k_hb[] = { { 6, 0 }, { 6, 6 }, { 6, 4 }, { 20, 10 }, { 40, 30 }, { 60, 5 }, { 1, 1 }, { 100, 90 } };
RT(inline_top_left_y_is_baseline_minus_object_baseline_table)
{
    unsigned i;
    R2_BEGIN();
    for (i = 0; i < sizeof k_hb / sizeof k_hb[0]; i++) {
        IDWriteTextLayout *l = r2_layout_w(&X, L"A#B", 16.0f, 300.0f, 200.0f, DWRITE_FONT_WEIGHT_NORMAL); rh *r = rh_new(); dwx_inline *io;
        if (!l) { rh_free(r); continue; }
        r->snap_val = TRUE; io = dwx_inline_new(12.0f, k_hb[i].h, k_hb[i].b, FALSE);
        IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1));
        CHECK_HR_OK(rh_draw(l, r, CTX_A, 7.0f, 11.0f));
        { const rh_ev *in = ev_nth(r, RH_INLINE, 0), *g = ev_nth(r, RH_GLYPH, 0);
          if (in && g) {
            CHECKF(fabsf(in->y + k_hb[i].b - g->y) < 0.001f, "object h %g b %g: top %g, text baseline %g", k_hb[i].h, k_hb[i].b, in->y, g->y);
            CHECKF(fabsf(in->x - (7.0f + 7.8125f)) < 0.001f, "object x %g", in->x);
            /* the line must be tall enough for the object: its bottom edge (top + height) does not leave the line box */
            { DWRITE_LINE_METRICS lm[2]; UINT32 nl = 0; IDWriteTextLayout_GetLineMetrics(l, lm, 2, &nl);
              CHECKF(nl == 1 && in->y + k_hb[i].h <= 11.0f + lm[0].height + 0.01f, "object bottom %g exceeds line bottom %g", in->y + k_hb[i].h, 11.0f + lm[0].height);
              CHECKF(in->y >= 11.0f - 0.01f, "object top %g above line top", in->y); }
          } else CHECK(0); }
        rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
RT(inline_objects_do_not_change_glyph_run_baseline_between_lines)
{
    /* two lines, an object only in the second: the first line's baseline comes from the text, the second line is taller */
    R2_BEGIN(); R2_LAYOUT(l, L"AAAA BB#B", 16.0f, 4 * 7.8125f + 2.0f, 200.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_inline *io = dwx_inline_new(8.0f, 40.0f, 30.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(7, 1));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { DWRITE_LINE_METRICS lm[4]; UINT32 n = 0; IDWriteTextLayout_GetLineMetrics(l, lm, 4, &n);
      EXPECT_EQ(n, 2);
      if (n == 2) {
        const rh_ev *g0 = ev_nth(r, RH_GLYPH, 0);
        CHECK(lm[1].height > lm[0].height); CHECK_NEARF(lm[1].baseline, 30.0f);
        if (g0) CHECK_NEARF(g0->y, lm[0].baseline);
      } }
    rh_free(r); dwx_release(io); IDWriteTextLayout_Release(l); R2_END();
}
