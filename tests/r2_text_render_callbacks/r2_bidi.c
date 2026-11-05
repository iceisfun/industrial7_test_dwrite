/* Right-to-left runs as delivered to DrawGlyphRun: for an odd bidiLevel the glyph array is in reading (logical) order and the baseline origin is
 * the RIGHT edge of the run (the glyphs extend to the left of it), so clusterMap is the identity for simple text; for an even level the origin
 * is the left edge.  DWRITE_GLYPH_RUN_DESCRIPTION text is in logical order.  Runs are delivered in logical order.
 * Fixture: Hebrew alef/bet/gimel are glyphs 2/3/4 (advance 1000 = 7.8125 DIPs at size 16), digits '1' '2' are glyphs 55 56.
 */
#include "r2.h"

#define HEB3 L"\x05d0\x05d1\x05d2"

static const rh_ev *glyph_at(const rh *r, int n) { return rh_nth(r, RH_GLYPH, n); }

RT(bidi_hebrew_glyphs_in_reading_order_origin_on_right)
{
    R2_BEGIN(); R2_LAYOUT(l, HEB3, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 1);
    { const rh_ev *g = glyph_at(r, 0);
      if (g) {
        EXPECT_EQ(g->n, 3); EXPECT_EQ(g->bidi, 1); EXPECT_EQ(g->sideways, FALSE);
        EXPECT_EQ(g->idx[0], 2); EXPECT_EQ(g->idx[1], 3); EXPECT_EQ(g->idx[2], 4);
        CHECK_NEARF(g->adv[0], 7.8125f); CHECK_NEARF(g->adv[1], 7.8125f); CHECK_NEARF(g->adv[2], 7.8125f);
        CHECK_NEARF(g->x, 3 * 7.8125f);
      } }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_hebrew_description_text_is_logical)
{
    R2_BEGIN(); R2_LAYOUT(l, HEB3, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *g = glyph_at(r, 0);
      if (g) {
        EXPECT_EQ(g->has_desc, 1); EXPECT_EQ(g->tlen, 3);
        EXPECT_EQ(g->text[0], 0x05d0); EXPECT_EQ(g->text[1], 0x05d1); EXPECT_EQ(g->text[2], 0x05d2);
      } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_hebrew_cluster_map_is_identity)
{
    R2_BEGIN(); R2_LAYOUT(l, HEB3, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *g = glyph_at(r, 0);
      if (g) { EXPECT_EQ(g->cmap[0], 0); EXPECT_EQ(g->cmap[1], 1); EXPECT_EQ(g->cmap[2], 2); } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_hebrew_cluster_map_agrees_with_glyph_indices)
{
    /* for every text position the glyph at clusterMap[i] is the glyph of that character */
    R2_BEGIN(); R2_LAYOUT(l, HEB3, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    static const UINT16 gid[3] = { 2, 3, 4 };
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *g = glyph_at(r, 0); int i;
      if (g) for (i = 0; i < 3; i++) CHECKF(g->cmap[i] < g->n && g->idx[g->cmap[i]] == gid[i], "text %d -> glyph slot %u", i, g->cmap[i]);
      else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_hebrew_in_rtl_paragraph_origin_at_box_right_edge)
{
    R2_BEGIN(); R2_LAYOUT(l, HEB3, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *g = glyph_at(r, 0);
      if (g) {
        EXPECT_EQ(g->bidi, 1); EXPECT_EQ(g->idx[0], 2); EXPECT_EQ(g->idx[2], 4); EXPECT_EQ(g->cmap[0], 0);
        CHECK_NEARF(g->x, 200.0f);                             /* the run's right edge sits on the right edge of the box (leading edge of a RTL paragraph) */
      } }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_latin_in_rtl_paragraph_is_level_two_logical_glyph_order)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABC", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *g = glyph_at(r, 0);
      if (g) {
        EXPECT_EQ(g->bidi, 2); EXPECT_EQ(g->n, 3);
        EXPECT_EQ(g->idx[0], 2); EXPECT_EQ(g->idx[1], 3); EXPECT_EQ(g->idx[2], 4);
        EXPECT_EQ(g->cmap[0], 0); EXPECT_EQ(g->cmap[1], 1); EXPECT_EQ(g->cmap[2], 2);
      } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_hebrew_then_digits_levels_and_tiling)
{
    /* levels 1 1 2 2: two runs (Hebrew odd, digits even) that tile the line without overlap */
    R2_BEGIN(); R2_LAYOUT(l, L"\x05d0\x05d1" L"12", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 2);
    { const rh_ev *h = glyph_at(r, 0), *d = glyph_at(r, 1);
      if (h && d) {
        EXPECT_EQ(h->tlen, 2); EXPECT_EQ(h->text[0], 0x05d0); EXPECT_EQ(h->bidi, 1);
        EXPECT_EQ(h->idx[0], 2); EXPECT_EQ(h->idx[1], 3); EXPECT_EQ(h->cmap[0], 0); EXPECT_EQ(h->cmap[1], 1);
        EXPECT_EQ(d->tlen, 2); EXPECT_EQ(d->text[0], '1'); EXPECT_EQ(d->bidi, 2);
        EXPECT_EQ(d->idx[0], 55); EXPECT_EQ(d->idx[1], 56); EXPECT_EQ(d->cmap[0], 0); EXPECT_EQ(d->cmap[1], 1);
        {   /* Hebrew run occupies [h->x - 15.625, h->x], digit run [d->x, d->x + 15.625] */
            float hl = h->x - 2 * 7.8125f, hr = h->x, dl = d->x, dr = d->x + 2 * 7.8125f;
            CHECKF(fabsf(hr - dl) < 0.001f || fabsf(dr - hl) < 0.001f, "runs must abut: Hebrew [%g,%g] digits [%g,%g]", hl, hr, dl, dr);
            CHECKF(fminf(hl, dl) < 0.001f, "leftmost run starts at 0");
            t_note("Hebrew then digits: digits are drawn %s of the Hebrew letters", dr <= hl + 0.001f ? "left" : "right");
        }
      } }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_latin_hebrew_latin_run_origins)
{
    /* the Hebrew run sits in its logical slot (it is the only right-to-left run); each run's origin is its left edge, or its right edge when odd */
    R2_BEGIN(); R2_LAYOUT(l, L"AB " HEB3 L" CD", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    float cum = 0; int i, nr = r->count[RH_GLYPH];
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(rh_total_glyphs(r), 9);
    for (i = 0; i < r->nev; i++) {
        const rh_ev *g = &r->ev[i]; float w; UINT32 k;
        if (g->type != RH_GLYPH) continue;
        w = rh_run_width(g);
        CHECKF(fabsf(g->x - ((g->bidi & 1) ? cum + w : cum)) < 0.001f, "run at text %c: x %g, expected %g (level %u)", (char)g->text[0], g->x, (g->bidi & 1) ? cum + w : cum, g->bidi);
        for (k = 0; k < g->tlen; k++) {
            int heb = g->text[k] >= 0x05d0 && g->text[k] <= 0x05d2;
            CHECKF(heb ? (g->bidi == 1) : (g->bidi == 0), "char U+%04x in a level %u run", g->text[k], g->bidi);
        }
        cum += w;
    }
    CHECK_NEARF(cum, 9 * 7.8125f);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_rtl_paragraph_logical_run_order_is_delivery_order)
{
    /* runs are reported in text order even when the paragraph is laid out right to left: the first text is the rightmost */
    R2_BEGIN(); R2_LAYOUT(l, L"AB " HEB3, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *a = glyph_at(r, 0), *h = NULL; int i;
      for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH && r->ev[i].text[0] == 0x05d0) h = &r->ev[i];
      CHECK(a && h);
      if (a && h) {
        EXPECT_EQ(a->text[0], 'A');
        CHECKF(rh_index(r, a) < rh_index(r, h), "Latin run delivered before the Hebrew run");
        CHECKF(h->x < a->x, "Hebrew right edge %g must be left of the Latin run's left edge %g", h->x, a->x);
        CHECK_NEARF(a->x + rh_run_width(a), 200.0f);        /* the first text takes the right end of the box */
      } }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(bidi_rtl_advances_positive_offsets_neutral)
{
    R2_BEGIN(); R2_LAYOUT(l, HEB3, 16.0f, 200.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *g = glyph_at(r, 0); UINT32 i;
      if (g) {
        for (i = 0; i < g->n; i++) CHECK(g->adv[i] > 0.0f);
        if (g->offsets_ptr) for (i = 0; i < g->n; i++) { EXPECT_EQ(g->off[i].advanceOffset, 0); EXPECT_EQ(g->off[i].ascenderOffset, 0); }
      } }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}

/* per-character embedding level (what the runs report, however the layout splits them) */
static const struct { const WCHAR *text; int rtl_para; int n; int lv[4]; } k_lv[] = {
    { L"A", 0, 1, { 0 } }, { L"\x05d0", 0, 1, { 1 } }, { L"1", 0, 1, { 0 } }, { L"\x05d0" L"1", 0, 2, { 1, 2 } }, { L"A1", 0, 2, { 0, 0 } },
    { L"\x05d0" L"A", 0, 2, { 1, 0 } }, { L"A\x05d0", 0, 2, { 0, 1 } }, { L"\x05d0 A", 0, 3, { 1, 0, 0 } }, { L"\x05d0\x05d1\x05d2", 0, 3, { 1, 1, 1 } },
    { L"A", 1, 1, { 2 } }, { L"\x05d0", 1, 1, { 1 } }, { L"1", 1, 1, { 2 } }, { L"\x05d0" L"1", 1, 2, { 1, 2 } }, { L"A1", 1, 2, { 2, 2 } },
    { L"\x05d0" L"A", 1, 2, { 1, 2 } }, { L"A\x05d0", 1, 2, { 2, 1 } },
};
RT(bidi_embedding_level_table)
{
    unsigned i;
    R2_BEGIN();
    for (i = 0; i < sizeof k_lv / sizeof k_lv[0]; i++) {
        IDWriteTextLayout *l = r2_layout_w(&X, k_lv[i].text, 16.0f, 300.0f, 50.0f, DWRITE_FONT_WEIGHT_NORMAL); rh *r = rh_new();
        int got[8], k, e;
        if (!l) { rh_free(r); t_check(0, __FILE__, __LINE__, "layout", "row %u", i); continue; }
        r->snap_val = TRUE;
        for (k = 0; k < 8; k++) got[k] = -1;
        if (k_lv[i].rtl_para) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
        { int pos = 0; for (e = 0; e < r->nev; e++) if (r->ev[e].type == RH_GLYPH) { for (k = 0; k < (int)r->ev[e].tlen && pos < 8; k++) got[pos++] = r->ev[e].bidi; } }
        for (k = 0; k < k_lv[i].n; k++) CHECKF(got[k] == k_lv[i].lv[k], "row %u (rtl paragraph %d) char %d: level %d want %d", i, k_lv[i].rtl_para, k, got[k], k_lv[i].lv[k]);
        rh_free(r); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
RT(bidi_rtl_line_wrap_keeps_run_levels)
{
    /* a Hebrew word wrapped onto its own line is still level 1; with its right-edge origin it sits at x = its width on a left aligned line */
    R2_BEGIN(); R2_LAYOUT(l, L"AAAA " HEB3, 16.0f, 4 * 7.8125f + 4.0f, 80.0f); rh *r = rh_new(); r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *a = glyph_at(r, 0), *b = glyph_at(r, 1);
      CHECK(a && b);
      if (a && b) { EXPECT_EQ(b->bidi, 1); CHECK(b->y > a->y); CHECK_NEARF(b->x, 3 * 7.8125f); } }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
