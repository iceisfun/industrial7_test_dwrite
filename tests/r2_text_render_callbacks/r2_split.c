/* Which text attributes split glyph runs.  A run boundary appears wherever two neighbouring spans differ in font family, weight, style,
 * stretch, size, locale, drawing effect, underline, strikethrough or inline object; each boundary produces separate DrawGlyphRun calls
 * whose text ranges tile the string without gaps.  Attributes that merely re-state the current value are recorded with t_note.
 */
#include "r2.h"

#define TXT L"AAAABBBBCCCC"

typedef HRESULT (*setprop_fn)(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux);
static HRESULT p_size(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetFontSize(l, 20.0f, rg); }
static HRESULT p_weight(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, rg); }
static HRESULT p_style(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, rg); }
static HRESULT p_stretch(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetFontStretch(l, DWRITE_FONT_STRETCH_CONDENSED, rg); }
static HRESULT p_family(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetFontFamilyName(l, R2_SHAPE, rg); }
static HRESULT p_locale(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetLocaleName(l, L"fr-FR", rg); }
static HRESULT p_effect(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)aux, rg); }
static HRESULT p_under(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetUnderline(l, TRUE, rg); }
static HRESULT p_strike(IDWriteTextLayout *l, DWRITE_TEXT_RANGE rg, void *aux) { return IDWriteTextLayout_SetStrikethrough(l, TRUE, rg); }
static const struct { const char *name; setprop_fn fn; } k_props[] = {
    { "font size", p_size }, { "font weight", p_weight }, { "font style", p_style }, { "font stretch", p_stretch }, { "font family", p_family },
    { "locale", p_locale }, { "drawing effect", p_effect }, { "underline", p_under }, { "strikethrough", p_strike },
};
#define NPROPS ((int)(sizeof k_props / sizeof k_props[0]))

static void check_tiling(const rh *r, UINT32 total)
{
    int i; UINT32 covered = 0; const WCHAR *prev_end = NULL;
    for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH) {
        const rh_ev *g = &r->ev[i];
        if (prev_end) CHECK((const WCHAR *)g->text_ptr == prev_end);
        prev_end = (const WCHAR *)g->text_ptr + g->tlen; covered += g->tlen;
    }
    EXPECT_EQ(covered, total);
}

RT(split_each_attribute_in_the_middle_makes_three_runs)
{
    int p;
    R2_BEGIN();
    for (p = 0; p < NPROPS; p++) {
        IDWriteTextLayout *l = r2_layout_w(&X, TXT, 16.0f, 500.0f, 50.0f, DWRITE_FONT_WEIGHT_NORMAL); rh *r = rh_new(); dwx_effect *fx = dwx_effect_new(1);
        if (!l) { rh_free(r); dwx_release(fx); continue; }
        r->snap_val = TRUE;
        CHECKF(SUCCEEDED(k_props[p].fn(l, RNG(4, 4), fx)), "set %s", k_props[p].name);
        CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
        CHECKF(r->count[RH_GLYPH] == 3, "%s: %d runs, want 3", k_props[p].name, r->count[RH_GLYPH]);
        if (r->count[RH_GLYPH] == 3) {
            const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1), *c = rh_nth(r, RH_GLYPH, 2);
            CHECKF(a->tlen == 4 && b->tlen == 4 && c->tlen == 4, "%s: run lengths %u %u %u", k_props[p].name, a->tlen, b->tlen, c->tlen);
            CHECKF(fabsf(b->x - (a->x + rh_run_width(a))) < 0.001f, "%s: second run starts at %g, first ends at %g", k_props[p].name, b->x, a->x + rh_run_width(a));
            CHECKF(fabsf(c->x - (b->x + rh_run_width(b))) < 0.001f, "%s: third run starts at %g", k_props[p].name, c->x);
        }
        check_tiling(r, 12);
        rh_free(r); dwx_release(fx); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
static const struct { UINT32 start, len; int nruns; UINT32 lens[3]; } k_rg[] = {
    { 0, 4, 2, { 4, 8 } }, { 4, 4, 3, { 4, 4, 4 } }, { 8, 4, 2, { 8, 4 } }, { 2, 6, 3, { 2, 6, 4 } }, { 1, 1, 3, { 1, 1, 10 } }, { 11, 1, 2, { 11, 1 } },
    { 0, 12, 1, { 12 } }, { 5, 1, 3, { 5, 1, 6 } },
};
RT(split_size_range_table_run_lengths)
{
    unsigned i;
    R2_BEGIN();
    for (i = 0; i < sizeof k_rg / sizeof k_rg[0]; i++) {
        IDWriteTextLayout *l = r2_layout_w(&X, TXT, 16.0f, 500.0f, 50.0f, DWRITE_FONT_WEIGHT_NORMAL); rh *r = rh_new(); int k, n = 0;
        if (!l) { rh_free(r); continue; }
        r->snap_val = TRUE;
        CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(k_rg[i].start, k_rg[i].len)));
        CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
        CHECKF(r->count[RH_GLYPH] == k_rg[i].nruns, "range (%u,%u): %d runs, want %d", k_rg[i].start, k_rg[i].len, r->count[RH_GLYPH], k_rg[i].nruns);
        for (k = 0; k < r->nev; k++) if (r->ev[k].type == RH_GLYPH && n < 3) {
            CHECKF(r->ev[k].tlen == k_rg[i].lens[n], "range (%u,%u) run %d: length %u want %u", k_rg[i].start, k_rg[i].len, n, r->ev[k].tlen, k_rg[i].lens[n]);
            n++;
        }
        check_tiling(r, 12);
        rh_free(r); IDWriteTextLayout_Release(l);
    }
    R2_END();
}
RT(split_run_em_size_follows_range)
{
    R2_BEGIN(); R2_LAYOUT(l, TXT, 16.0f, 500.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(4, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1), *c = rh_nth(r, RH_GLYPH, 2);
      if (a && b && c) {
        CHECK_NEARF(a->em, 16.0f); CHECK_NEARF(b->em, 20.0f); CHECK_NEARF(c->em, 16.0f);
        CHECK_NEARF(b->adv[0], R2_ADV(1000, 20)); CHECK_NEARF(rh_run_width(b), 4 * R2_ADV(1000, 20));
      } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_weight_run_uses_bold_advances)
{
    R2_BEGIN(); R2_LAYOUT(l, TXT, 16.0f, 500.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(4, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1), *c = rh_nth(r, RH_GLYPH, 2);
      if (a && b && c) {
        CHECK_NEARF(a->adv[0], R2_ADV(1000, 16)); CHECK_NEARF(b->adv[0], R2_ADV(1200, 16)); CHECK_NEARF(c->adv[0], R2_ADV(1000, 16));
        CHECK(a->face != b->face); CHECK(a->face == c->face);
        CHECK_NEARF(c->x, 4 * R2_ADV(1000, 16) + 4 * R2_ADV(1200, 16));
      } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_family_run_reports_other_face_with_ligature)
{
    R2_BEGIN(); IDWriteTextLayout *l = r2_layout_w(&X, L"ABABAB", 16.0f, 500.0f, 50.0f, DWRITE_FONT_WEIGHT_NORMAL); rh *r = rh_new();
    if (!l) { rh_free(r); R2_END(); t_skip("layout unavailable"); return; }
    r->snap_val = TRUE;
    IDWriteTextLayout_SetFontFamilyName(l, R2_SHAPE, RNG(2, 2));
    CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
    EXPECT_EQ(r->count[RH_GLYPH], 3);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1), *c = rh_nth(r, RH_GLYPH, 2);
      if (a && b && c) {
        EXPECT_EQ(a->n, 2); EXPECT_EQ(a->idx[0], 2); EXPECT_EQ(a->idx[1], 3);        /* plain family: no ligature */
        EXPECT_EQ(b->n, 1); EXPECT_EQ(b->idx[0], 68); EXPECT_EQ(b->cmap[0], 0); EXPECT_EQ(b->cmap[1], 0);   /* R2Shape: A B -> ring */
        EXPECT_EQ(c->n, 2);
        CHECK_NEARF(b->x, 2 * 7.8125f); CHECK_NEARF(c->x, 3 * 7.8125f);
      } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_run_locale_names_follow_ranges)
{
    R2_BEGIN(); R2_LAYOUT(l, TXT, 16.0f, 500.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTextLayout_SetLocaleName(l, L"fr-FR", RNG(4, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1), *c = rh_nth(r, RH_GLYPH, 2);
      if (a && b && c) { CHECK(!_wcsicmp(a->locale, R2_LOC)); CHECK(!_wcsicmp(b->locale, L"fr-FR")); CHECK(!_wcsicmp(c->locale, R2_LOC)); } else CHECK(0); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_effect_per_run)
{
    R2_BEGIN(); R2_LAYOUT(l, TXT, 16.0f, 500.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 4)); IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(8, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 3);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1), *c = rh_nth(r, RH_GLYPH, 2);
      if (a && b && c) { EXPECT_EQ(a->effect, (IUnknown *)e1); EXPECT_EQ(b->effect, NULL); EXPECT_EQ(c->effect, (IUnknown *)e2); } else CHECK(0); }
    rh_free(r); dwx_release(e1); dwx_release(e2); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_two_distinct_effect_objects_adjacent_make_two_runs)
{
    R2_BEGIN(); R2_LAYOUT(l, TXT, 16.0f, 500.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 6)); IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(6, 6));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 2);
    { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, 1);
      if (a && b) { EXPECT_EQ(a->tlen, 6); EXPECT_EQ(b->tlen, 6); EXPECT_EQ(a->effect, (IUnknown *)e1); EXPECT_EQ(b->effect, (IUnknown *)e2); } else CHECK(0); }
    rh_free(r); dwx_release(e1); dwx_release(e2); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_line_wrap_splits_runs_at_line_end)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AAAA BBBB CCCC", 16.0f, 4 * 7.8125f + 2.0f, 200.0f); rh *r = rh_new(); r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    /* trailing spaces may be runs of their own: look at the first run on each line */
    { int i, lines = 0; float prevy = -1, ly[4] = { 0 }, lx[4] = { 0 };
      for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH && r->ev[i].y > prevy + 0.01f) { if (lines < 4) { ly[lines] = r->ev[i].y; lx[lines] = r->ev[i].x; } lines++; prevy = r->ev[i].y; }
      EXPECT_EQ(lines, 3);
      if (lines == 3) { CHECK(ly[0] < ly[1]); CHECK(ly[1] < ly[2]); CHECK_NEARF(lx[0], 0.0f); CHECK_NEARF(lx[1], 0.0f); CHECK_NEARF(lx[2], 0.0f); } }
    CHECK(rh_total_glyphs(r) >= 12 && rh_total_glyphs(r) <= 14);     /* the separating spaces may or may not be drawn as glyphs */
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_hard_newline_splits_runs)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AAA\nBBB\r\nCCC", 16.0f, 500.0f, 200.0f); rh *r = rh_new(); r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    { int i, lines_seen = 0; float prevy = -1;
      for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH) { if (r->ev[i].y > prevy + 0.01f) lines_seen++; prevy = r->ev[i].y; }
      EXPECT_EQ(lines_seen, 3); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_restating_current_value_noted)
{
    R2_BEGIN(); R2_LAYOUT(l, TXT, 16.0f, 500.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTextLayout_SetFontSize(l, 16.0f, RNG(4, 4));
    IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_NORMAL, RNG(4, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    t_note("setting size and weight to their current values in the middle: %d glyph runs", r->count[RH_GLYPH]);
    CHECK(r->count[RH_GLYPH] >= 1 && r->count[RH_GLYPH] <= 3);
    check_tiling(r, 12);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_set_then_restore_value_noted)
{
    R2_BEGIN(); R2_LAYOUT(l, TXT, 16.0f, 500.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(4, 4));
    IDWriteTextLayout_SetFontSize(l, 16.0f, RNG(4, 4));
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    t_note("size changed and restored: %d glyph runs", r->count[RH_GLYPH]);
    CHECK(r->count[RH_GLYPH] >= 1 && r->count[RH_GLYPH] <= 3);
    EXPECT_EQ(rh_total_glyphs(r), 12);
    { const rh_ev *g = rh_nth(r, RH_GLYPH, r->count[RH_GLYPH] - 1); if (g) CHECK_NEARF(g->em, 16.0f); }
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(split_typography_spacing_numbers_noted)
{
    R2_BEGIN(); R2_LAYOUT(l, TXT, 16.0f, 500.0f, 50.0f); rh *r = rh_new(); r->snap_val = TRUE;
    IDWriteTypography *ty = NULL; IDWriteTextLayout1 *l1 = NULL;
    if (SUCCEEDED(IDWriteFactory_CreateTypography(f, &ty))) {
        DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0 };
        IDWriteTypography_AddFontFeature(ty, ft);
        IDWriteTextLayout_SetTypography(l, ty, RNG(4, 4));
        IDWriteTypography_Release(ty);
    }
    if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1))) {
        IDWriteTextLayout1_SetCharacterSpacing(l1, 1.0f, 1.0f, 0.0f, RNG(8, 4));
        IDWriteTextLayout1_Release(l1);
    }
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    t_note("typography on the middle and character spacing on the end: %d glyph runs", r->count[RH_GLYPH]);
    EXPECT_EQ(rh_total_glyphs(r), 12);
    check_tiling(r, 12);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
