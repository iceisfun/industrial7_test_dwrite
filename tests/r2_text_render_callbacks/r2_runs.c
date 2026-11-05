/* Shape of the data handed to DrawGlyphRun, the category order of Draw, and the renderer reference protocol.
 *
 * DrawGlyphRuns passes pointers into layout-wide arrays: the text pointer is the layout's own copy of the string plus the run start (so
 * consecutive runs of one line are contiguous in memory), glyph indices / advances / cluster map come from layout-wide arrays offset by the
 * run's first glyph / character, glyphOffsets is NULL when the layout holds no offsets at all.  TextLayout::Draw calls QueryInterface on the
 * renderer exactly once (IDWriteTextRenderer1) and never AddRefs it; a successful QI reference is released before Draw returns.
 */
#include "r2.h"

RT(runs_pointers_valid_inside_callback)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABC DEF", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); int i;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    CHECK(r->count[RH_GLYPH] >= 1);
    for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH) {
        const rh_ev *g = &r->ev[i];
        CHECK(g->run_ptr != NULL); CHECK(g->face != NULL); CHECK(g->indices_ptr != NULL); CHECK(g->advances_ptr != NULL);
        CHECK(g->text_ptr != NULL); CHECK(g->cmap_ptr != NULL); CHECK(g->locale_ptr != NULL);
        CHECK(g->n > 0); CHECK(g->tlen > 0);
    }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(runs_glyph_offsets_absent_or_zero_for_plain_text)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABC DEF", 16.0f, 300.0f, 50.0f); rh *r = rh_new(); int i, nul = 0, nonnul = 0;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH) {
        const rh_ev *g = &r->ev[i]; UINT32 k;
        if (!g->offsets_ptr) { nul++; continue; }
        nonnul++;
        for (k = 0; k < g->n && k < RH_MAXG; k++) { CHECK_NEARF(g->off[k].advanceOffset, 0.0f); CHECK_NEARF(g->off[k].ascenderOffset, 0.0f); }
    }
    t_note("glyphOffsets: %d runs with NULL, %d runs with an array of zero offsets", nul, nonnul);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(runs_consecutive_text_pointers_are_contiguous)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AAAABBBBCCCC", 16.0f, 400.0f, 50.0f); rh *r = rh_new();
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(4, 4)); IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(8, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 3);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1), *c = rh_nth(r, RH_GLYPH, 2);
      if (a && b && c) {
        CHECK((const WCHAR *)b->text_ptr == (const WCHAR *)a->text_ptr + a->tlen);
        CHECK((const WCHAR *)c->text_ptr == (const WCHAR *)b->text_ptr + b->tlen);
      } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(runs_cluster_map_pointer_follows_text_offset)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AAAABBBB", 16.0f, 400.0f, 50.0f); rh *r = rh_new();
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(4, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1);
      if (a && b) CHECK((const UINT16 *)b->cmap_ptr == (const UINT16 *)a->cmap_ptr + a->tlen);   /* one layout-wide cluster map, indexed by text position */
      else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(runs_glyph_arrays_are_layout_wide)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AAAABBBB", 16.0f, 400.0f, 50.0f); rh *r = rh_new();
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(4, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1);
      if (a && b) {
        /* both runs index the same layout-wide arrays, the later run starting at or after the earlier one */
        CHECK((const UINT16 *)b->indices_ptr >= (const UINT16 *)a->indices_ptr);
        CHECK((const float *)b->advances_ptr >= (const float *)a->advances_ptr);
        t_note("glyph index offset between consecutive runs: %d elements (first run has %u glyphs)", (int)((const UINT16 *)b->indices_ptr - (const UINT16 *)a->indices_ptr), a->n);
      } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(runs_cluster_map_values_stay_within_glyph_count)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AB CD EF GH", 16.0f, 40.0f, 200.0f); rh *r = rh_new(); int i; RH_DRAW_OK(l, r, 0.0f, 0.0f);
    for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH) {
        const rh_ev *g = &r->ev[i]; UINT32 k; UINT16 prev = 0;
        for (k = 0; k < g->tlen && k < RH_MAXG; k++) { CHECKF(g->cmap[k] < g->n, "run %d text %u maps to glyph %u of %u", i, k, g->cmap[k], g->n); CHECK(g->cmap[k] >= prev); prev = g->cmap[k]; }
    }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(runs_text_and_pointer_stable_across_repeated_draws_noted)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABCDEFGHIJ", 16.0f, 400.0f, 50.0f); rh *a = rh_new(), *b = rh_new();
    RH_DRAW_OK(l, a, 0.0f, 0.0f); RH_DRAW_OK(l, b, 0.0f, 0.0f);
    { const rh_ev *x = rh_nth(a, RH_GLYPH, 0), *y = rh_nth(b, RH_GLYPH, 0);
      if (x && y) { t_note("string pointer %s between two Draw calls, glyph array pointer %s", x->text_ptr == y->text_ptr ? "stable" : "changes", x->indices_ptr == y->indices_ptr ? "stable" : "changes");
                    EXPECT_EQ(x->tlen, y->tlen); } }
    rh_free(a); rh_free(b); IDWriteTextLayout_Release(l); R2_END();
}
RT(runs_locale_names_round_trip)
{
    static const WCHAR *const locs[] = { L"en-us", L"en-US", L"de-DE", L"fr-fr", L"ja-JP", L"he-IL", L"en-GB" };
    unsigned i;
    R2_BEGIN();
    for (i = 0; i < sizeof locs / sizeof locs[0]; i++) {
        IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL; rh *r = rh_new();
        if (FAILED(IDWriteFactory_CreateTextFormat(f, R2_FAM, X.coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, locs[i], &t))) { rh_free(r); continue; }
        if (SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"ABC", 3, t, 100.0f, 50.0f, &l))) {
            const rh_ev *g;
            CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
            g = rh_nth(r, RH_GLYPH, 0);
            if (g) {
                CHECKF(!_wcsicmp(g->locale, locs[i]), "format locale \"%ls\" reported as \"%ls\"", locs[i], g->locale);
                if (wcscmp(g->locale, locs[i])) t_note("locale \"%ls\" is reported as \"%ls\"", locs[i], g->locale);
            } else CHECK(0);
            IDWriteTextLayout_Release(l);
        }
        IDWriteTextFormat_Release(t); rh_free(r);
    }
    R2_END();
}
RT(runs_set_locale_name_range_splits_run_and_reports_new_name)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AAAABBBB", 16.0f, 400.0f, 50.0f); rh *r = rh_new();
    CHECK_HR_OK(IDWriteTextLayout_SetLocaleName(l, L"de-DE", RNG(4, 4)));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 2);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1);
      if (a && b) { CHECK(!_wcsicmp(a->locale, R2_LOC)); CHECK(!_wcsicmp(b->locale, L"de-DE")); } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}

/* ---- category order and counts ------------------------------------------------------------------------------- */
static void check_category_order(const rh *r)
{
    int lg = rh_last(r, RH_GLYPH), fi = rh_first(r, RH_INLINE), li = rh_last(r, RH_INLINE), fu = rh_first(r, RH_UNDER), lu = rh_last(r, RH_UNDER), fs = rh_first(r, RH_STRIKE);
    int fsnap = rh_last(r, RH_SNAP), fg = rh_first(r, RH_GLYPH);
    if (lg >= 0 && fi >= 0) CHECK(lg < fi);
    if (lg >= 0 && fu >= 0) CHECK(lg < fu);
    if (li >= 0 && fu >= 0) CHECK(li < fu);
    if (lu >= 0 && fs >= 0) CHECK(lu < fs);
    if (li >= 0 && fs >= 0) CHECK(li < fs);
    if (lg >= 0 && fs >= 0) CHECK(lg < fs);
    if (fg >= 0) CHECK(fsnap < fg);
}
typedef void (*setup_fn)(IDWriteTextLayout *l, dwx_inline *i1, dwx_inline *i2);
static void s_plain(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { }
static void s_sizes(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(3, 3)); }
static void s_ul2(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(3, 3)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8)); }
static void s_ul_strike(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 8)); }
static void s_ul_disjoint(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 2)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(5, 2)); }
static void s_inline1(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)a, RNG(3, 1)); }
static void s_inline2(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)a, RNG(2, 1)); IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)b, RNG(5, 1)); }
static void s_inline_ul_strike(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)a, RNG(3, 1)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 3)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(4, 3)); }
static void s_inline_all(IDWriteTextLayout *l, dwx_inline *a, dwx_inline *b) { IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)a, RNG(2, 1)); IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)b, RNG(5, 1)); IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 8)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 8)); }
static const struct { const char *name; setup_fn fn; int g, in, ul, st; } k_mat[] = {
    { "plain", s_plain, 1, 0, 0, 0 }, { "two sizes", s_sizes, 3, 0, 0, 0 }, { "underline over two sizes", s_ul2, 3, 0, 1, 0 },
    { "underline and strikethrough", s_ul_strike, 1, 0, 1, 1 }, { "two underline ranges", s_ul_disjoint, 4, 0, 2, 0 },
    { "one object", s_inline1, 2, 1, 0, 0 }, { "two objects", s_inline2, 3, 2, 0, 0 },
    { "object + underline + strike", s_inline_ul_strike, 3, 1, 1, 1 },
};
RT(runs_category_counts_and_order_matrix)
{
    unsigned i;
    R2_BEGIN();
    for (i = 0; i < sizeof k_mat / sizeof k_mat[0]; i++) {
        IDWriteTextLayout *l = r2_layout_w(&X, L"ABCDEFGH", 16.0f, 400.0f, 100.0f, DWRITE_FONT_WEIGHT_NORMAL); rh *r = rh_new();
        dwx_inline *a = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE), *b = dwx_inline_new(12.0f, 12.0f, 8.0f, FALSE);
        if (!l) { rh_free(r); dwx_release(a); dwx_release(b); continue; }
        k_mat[i].fn(l, a, b);
        CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
        CHECKF(r->count[RH_GLYPH] == k_mat[i].g, "%s: %d glyph runs, want %d", k_mat[i].name, r->count[RH_GLYPH], k_mat[i].g);
        CHECKF(r->count[RH_INLINE] == k_mat[i].in, "%s: %d inline objects, want %d", k_mat[i].name, r->count[RH_INLINE], k_mat[i].in);
        CHECKF(r->count[RH_UNDER] == k_mat[i].ul, "%s: %d underlines, want %d", k_mat[i].name, r->count[RH_UNDER], k_mat[i].ul);
        CHECKF(r->count[RH_STRIKE] == k_mat[i].st, "%s: %d strikethroughs, want %d", k_mat[i].name, r->count[RH_STRIKE], k_mat[i].st);
        check_category_order(r);
        rh_free(r); IDWriteTextLayout_Release(l); dwx_release(a); dwx_release(b);
    }
    R2_END();
}
RT(runs_everything_at_once_order)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABCDEFGH", 16.0f, 400.0f, 100.0f); rh *r = rh_new();
    dwx_inline *a = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE), *b = dwx_inline_new(12.0f, 12.0f, 8.0f, FALSE);
    s_inline_all(l, a, b);
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    check_category_order(r);
    EXPECT_EQ(r->count[RH_GLYPH], 3); EXPECT_EQ(r->count[RH_INLINE], 2);
    CHECK(r->count[RH_UNDER] >= 1); CHECK(r->count[RH_STRIKE] >= 1);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(a); dwx_release(b); R2_END();
}
RT(runs_multi_line_categories_are_global_not_per_line)
{
    /* glyph runs of ALL lines come before any underline: the categories are not interleaved per line */
    R2_BEGIN(); R2_LAYOUT(l, L"AAAA BBBB CCCC", 16.0f, 4 * 7.8125f + 2.0f, 200.0f); rh *r = rh_new();
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 14)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 14));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_UNDER], 3); EXPECT_EQ(r->count[RH_STRIKE], 3);
    check_category_order(r);
    { int i, prevu = -1; for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_UNDER) { if (prevu >= 0) CHECK(r->ev[i].y > r->ev[prevu].y); prevu = i; } }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}

/* ---- renderer references ------------------------------------------------------------------------------------- */
RT(renderer_asked_once_for_textrenderer1_and_never_addrefed)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); rh *r = rh_new();
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_QI], 1);
    EXPECT_EQ(rh_qi_asked(r, &IID_IDWriteTextRenderer1), 1);
    EXPECT_EQ(r->addref, 0); EXPECT_EQ(r->release, 0); EXPECT_EQ(r->ref, 1);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(renderer_qi_reference_is_released_after_draw)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); rh *r = rh_new(); r->allow_r1 = 1;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_QI], 1);
    EXPECT_EQ(r->release, 1);
    EXPECT_EQ(r->ref, 1);
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->release, 2); EXPECT_EQ(r->ref, 1);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(renderer_not_retained_after_draw)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABC", 16.0f, 100.0f, 50.0f); rh *r = rh_new();
    dwx_effect *fx = dwx_effect_new(1);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)fx, RNG(0, 3));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->ref, 1);
    { const rh_ev *g = rh_nth(r, RH_GLYPH, 0); if (g) { EXPECT_EQ(g->effect, (IUnknown *)fx); CHECK(g->effect_refs >= 2); } else CHECK(0); }
    { ULONG held = dw_refcount((IUnknown *)fx);                  /* ours + whatever the layout keeps (the count it keeps is implementation detail) */
      CHECK(held >= 2); t_note("effect refcount after the first Draw: %lu", held);
      RH_DRAW_OK(l, r, 0.0f, 0.0f); EXPECT_REFS(fx, held); }       /* Draw added nothing permanent */
    IDWriteTextLayout_Release(l);
    dwx_release(fx); rh_free(r); R2_END();
}

/* ---- reference hygiene of the application objects ---------------------------------------------------------------- */
RT(leak_effect_and_inline_released_with_layout)
{
    DWX_LEAK_BEGIN(a); R2_BEGIN();
    {
        R2_LAYOUT(l, L"AB#CD", 16.0f, 200.0f, 50.0f); rh *r = rh_new();
        dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE); dwx_effect *fx = dwx_effect_new(1);
        IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(2, 1));
        IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)fx, RNG(0, 2));
        IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 2));
        RH_DRAW_OK(l, r, 0.0f, 0.0f); RH_DRAW_OK(l, r, 1.0f, 1.0f);
        { ULONG hi = dw_refcount((IUnknown *)io), hf = dw_refcount((IUnknown *)fx); CHECK(hi >= 2 && hf >= 2);
          dwx_release(io); dwx_release(fx);
          EXPECT_REFS(io, hi - 1); EXPECT_REFS(fx, hf - 1); }   /* the layout still holds them after our references are gone */
        IDWriteTextLayout_Release(l);
        rh_free(r);
    }
    R2_END();
    DWX_LEAK_END(a);
}
RT(leak_after_aborted_draw)
{
    DWX_LEAK_BEGIN(a); R2_BEGIN();
    {
        R2_LAYOUT(l, L"AB#CD", 16.0f, 200.0f, 50.0f); rh *r = rh_new();
        dwx_inline *io = dwx_inline_new(10.0f, 12.0f, 8.0f, FALSE); dwx_effect *fx = dwx_effect_new(1);
        IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(2, 1));
        IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)fx, RNG(0, 2));
        RH_DRAW_OK(l, r, 0.0f, 0.0f);
        r->ret_set[RH_XFORM] = 1; r->ret[RH_XFORM] = E_FAIL;
        { ULONG hi = dw_refcount((IUnknown *)io), hf = dw_refcount((IUnknown *)fx);
          CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_FAIL);
          EXPECT_REFS(io, hi); EXPECT_REFS(fx, hf);              /* the aborted Draw leaves no extra reference */ }
        IDWriteTextLayout_Release(l);
        EXPECT_REFS(io, 1); EXPECT_REFS(fx, 1);
        dwx_release(io); dwx_release(fx); rh_free(r);
    }
    R2_END();
    DWX_LEAK_END(a);
}
RT(leak_effect_refcount_unchanged_by_draws)
{
    R2_BEGIN(); R2_LAYOUT(l, L"ABCD", 16.0f, 200.0f, 50.0f); rh *r = rh_new(); dwx_effect *fx = dwx_effect_new(1); int i;
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)fx, RNG(0, 4));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 4)); IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(0, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);                                 /* the first Draw may take internal references that stay with the layout */
    { ULONG held = dw_refcount((IUnknown *)fx); CHECK(held >= 2);
      for (i = 0; i < 5; i++) { RH_DRAW_OK(l, r, 0.0f, 0.0f); EXPECT_REFS(fx, held); } }
    { int k; for (k = 0; k < r->nev; k++) if (r->ev[k].type >= RH_GLYPH) { EXPECT_EQ(r->ev[k].effect, (IUnknown *)fx); } }
    IDWriteTextLayout_Release(l);
    dwx_release(fx); rh_free(r); R2_END();
}
