/* Reference ownership of the interface-valued per-range properties: drawing effect (IUnknown), inline object, typography and font
 * collection. A Set* with a non-empty range stores one reference per run that holds the object; Get* hands out an AddRef'd pointer to the
 * very same object; replacing or reverting a range releases; destroying the layout releases everything. Empty-range and
 * invalid-range calls retain nothing. dwrite never calls QueryInterface on a drawing effect.
 */
#include "tlr.h"

#define T10 L"abcdefghij"
#define REFS(o) dwx_refs(o)

/* ---- drawing effect ------------------------------------------------------------------------------------------------------ */
TEST(refs_effect_set_retains_one_reference)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1);
    EXPECT_EQ(REFS(e), 1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 3)), S_OK);
    EXPECT_EQ(REFS(e), 2);
    IDWriteTextLayout_Release(l);
    EXPECT_EQ(REFS(e), 1); CHECK(!dwx_destroyed(e));
    dwx_release(e); CHECK(dwx_destroyed(e)); TLR_END();
}
TEST(refs_effect_two_separate_ranges_retain_two)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(1, 2)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(6, 2)), S_OK);
    EXPECT_EQ(REFS(e), 3);
    IDWriteTextLayout_Release(l);
    EXPECT_EQ(REFS(e), 1); dwx_release(e); TLR_END();
}
TEST(refs_effect_replaced_by_null_releases)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1); IUnknown *g; DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 3)), S_OK);
    EXPECT_EQ(REFS(e), 2);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, NULL, RNG(2, 3)), S_OK);
    EXPECT_EQ(REFS(e), 1);
    g = tlr_effect(l, 3, &r); CHECK(g == NULL); CHECK_RANGE(r, 0, TLR_ALL);
    IDWriteTextLayout_Release(l); dwx_release(e); TLR_END();
}
TEST(refs_effect_replaced_by_other_object)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *a = dwx_effect_new(1), *b = dwx_effect_new(2);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)a, RNG(2, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)b, RNG(2, 3)), S_OK);
    EXPECT_EQ(REFS(a), 1); EXPECT_EQ(REFS(b), 2);
    IDWriteTextLayout_Release(l);
    EXPECT_EQ(REFS(b), 1); dwx_release(a); dwx_release(b); TLR_END();
}
TEST(refs_effect_same_object_same_range_twice)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 3)), S_OK);
    EXPECT_EQ(REFS(e), 2);
    IDWriteTextLayout_Release(l); EXPECT_EQ(REFS(e), 1); dwx_release(e); TLR_END();
}
TEST(refs_effect_get_returns_same_pointer_with_addref)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1); IUnknown *g1, *g2; DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 3)), S_OK);
    g1 = tlr_effect(l, 2, &r); CHECK(g1 == (IUnknown *)e); CHECK_RANGE(r, 2, 3); EXPECT_EQ(REFS(e), 3);
    g2 = tlr_effect(l, 4, &r); CHECK(g2 == (IUnknown *)e); CHECK_RANGE(r, 2, 3); EXPECT_EQ(REFS(e), 4);
    IUnknown_Release(g1); IUnknown_Release(g2);
    EXPECT_EQ(REFS(e), 2);
    IDWriteTextLayout_Release(l); EXPECT_EQ(REFS(e), 1); dwx_release(e); TLR_END();
}
TEST(refs_effect_get_outside_range_does_not_addref)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1); IUnknown *g;
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 3)), S_OK);
    g = tlr_effect(l, 0, NULL); CHECK(g == NULL); EXPECT_EQ(REFS(e), 2);
    g = tlr_effect(l, 9, NULL); CHECK(g == NULL); EXPECT_EQ(REFS(e), 2);
    IDWriteTextLayout_Release(l); dwx_release(e); TLR_END();
}
TEST(refs_effect_creator_release_keeps_object_alive)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(7); IUnknown *g;
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 10)), S_OK);
    dwx_release(e);                                     /* the layout keeps the only reference */
    CHECK(!dwx_destroyed(e)); EXPECT_EQ(REFS(e), 1);
    g = tlr_effect(l, 5, NULL); CHECK(g == (IUnknown *)e); EXPECT_EQ(REFS(e), 2);
    IUnknown_Release(g); CHECK(!dwx_destroyed(e));
    IDWriteTextLayout_Release(l);
    CHECK(dwx_destroyed(e)); TLR_END();
}
TEST(refs_effect_split_by_other_property_retains_per_run)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 10)), S_OK);
    EXPECT_EQ(REFS(e), 2);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(4, 2)), S_OK);      /* [0,4) [4,6) [6,10) all carry the effect */
    EXPECT_EQ(REFS(e), 4);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_NORMAL, RNG(4, 2)), S_OK);    /* runs merge again */
    t_note("refs after merging the split runs again: %ld", (long)REFS(e));
    IDWriteTextLayout_Release(l); EXPECT_EQ(REFS(e), 1); dwx_release(e); TLR_END();
}
TEST(refs_effect_empty_and_invalid_ranges_retain_nothing)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(3, 0)), S_OK); EXPECT_EQ(REFS(e), 1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0xFFFFFFFFu, 0)), S_OK); EXPECT_EQ(REFS(e), 1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(3, 0xFFFFFFFFu)), TLR_EINVALIDARG); EXPECT_EQ(REFS(e), 1);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0xFFFFFFFFu, 1)), TLR_EINVALIDARG); EXPECT_EQ(REFS(e), 1);
    IDWriteTextLayout_Release(l); dwx_release(e); TLR_END();
}
TEST(refs_effect_range_beyond_text_is_retained)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1); IUnknown *g;
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(50, 5)), S_OK);
    EXPECT_EQ(REFS(e), 2);
    g = tlr_effect(l, 52, NULL); CHECK(g == (IUnknown *)e); DW_RELEASE(g);
    IDWriteTextLayout_Release(l); EXPECT_EQ(REFS(e), 1); dwx_release(e); TLR_END();
}
TEST(refs_effect_never_queried)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1); IUnknown *g; DWRITE_TEXT_METRICS m;
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 3)), S_OK);
    g = tlr_effect(l, 3, NULL); DW_RELEASE(g);
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK);
    EXPECT_EQ(dwx_qi_calls(e), 0);
    IDWriteTextLayout_Release(l); dwx_release(e); TLR_END();
}
TEST(refs_effect_shared_by_two_layouts)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); IDWriteTextLayout *l2 = tlr_layout_(&X, TLR_FAM, T10, 16.0f); dwx_effect *e = dwx_effect_new(1);
    if (!l2) { IDWriteTextLayout_Release(l); dwx_release(e); TLR_END(); t_skip("second layout"); return; }
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 3)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l2, (IUnknown *)e, RNG(0, 3)), S_OK);
    EXPECT_EQ(REFS(e), 3);
    IDWriteTextLayout_Release(l); EXPECT_EQ(REFS(e), 2);
    IDWriteTextLayout_Release(l2); EXPECT_EQ(REFS(e), 1);
    dwx_release(e); TLR_END();
}
TEST(refs_effect_stress_layout_cycles_do_not_leak)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_effect *e = dwx_effect_new(1); int i; DWRITE_TEXT_METRICS m;
    TLR_QI(l3, IDWriteTextLayout3, l);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(2, 5)), S_OK);
    for (i = 0; i < 20; i++) {
        CHECK_HR(IDWriteTextLayout3_InvalidateLayout(l3), S_OK);
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK);
        CHECK_HR(IDWriteTextLayout_SetFontSize(l, 16.0f + (float)(i & 1), RNG(3, 2)), S_OK);
    }
    IDWriteTextLayout3_Release(l3); IDWriteTextLayout_Release(l);
    EXPECT_EQ(REFS(e), 1); CHECK(!dwx_destroyed(e)); dwx_release(e); TLR_END();
}
TEST(refs_effect_100_disjoint_ranges)
{
    WCHAR text[300]; int i;
    for (i = 0; i < 299; i++) text[i] = L'a' + (i % 26); text[299] = 0;
    TLR_BEGIN(); TLR_LAYOUT(l, text, 16.0f); dwx_effect *e = dwx_effect_new(1);
    for (i = 0; i < 100; i++) CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG((UINT32)(i * 3), 1)), S_OK);
    t_note("refs with 100 disjoint single-character ranges: %ld (expected 101)", (long)REFS(e));
    EXPECT_EQ(REFS(e), 101);
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, NULL, RNG(0, 300)), S_OK);
    EXPECT_EQ(REFS(e), 1);
    IDWriteTextLayout_Release(l); dwx_release(e); TLR_END();
}

/* ---- inline object ------------------------------------------------------------------------------------------------------- */
TEST(refs_inline_set_retains_one_reference)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_inline *o = dwx_inline_new(30, 20, 15, FALSE);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 1)), S_OK);
    EXPECT_EQ(REFS(o), 2);
    EXPECT_EQ(o->n_draw, 0); EXPECT_EQ(o->n_metrics, 0); EXPECT_EQ(o->n_overhang, 0); EXPECT_EQ(o->n_break, 0);      /* Set only stores the pointer */
    IDWriteTextLayout_Release(l);
    EXPECT_EQ(REFS(o), 1); dwx_release(o); TLR_END();
}
TEST(refs_inline_get_returns_same_pointer_with_addref)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_inline *o = dwx_inline_new(30, 20, 15, FALSE); IDWriteInlineObject *g; DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 3)), S_OK);
    g = tlr_inline(l, 3, &r); CHECK(g == (IDWriteInlineObject *)o); CHECK_RANGE(r, 2, 3); EXPECT_EQ(REFS(o), 3);
    DW_RELEASE(g); EXPECT_EQ(REFS(o), 2);
    g = tlr_inline(l, 5, &r); CHECK(g == NULL); CHECK_RANGE(r, 5, TLR_ALL - 5); EXPECT_EQ(REFS(o), 2);
    IDWriteTextLayout_Release(l); dwx_release(o); TLR_END();
}
TEST(refs_inline_replace_and_null)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_inline *a = dwx_inline_new(30, 20, 15, FALSE), *b = dwx_inline_new(40, 20, 15, FALSE);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)a, RNG(2, 1)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)b, RNG(2, 1)), S_OK);
    EXPECT_EQ(REFS(a), 1); EXPECT_EQ(REFS(b), 2);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, NULL, RNG(2, 1)), S_OK);
    EXPECT_EQ(REFS(b), 1);
    IDWriteTextLayout_Release(l); dwx_release(a); dwx_release(b); TLR_END();
}
TEST(refs_inline_creator_release_keeps_alive)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_inline *o = dwx_inline_new(30, 20, 15, FALSE);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 1)), S_OK);
    dwx_release(o); CHECK(!dwx_destroyed(o));
    IDWriteTextLayout_Release(l); CHECK(dwx_destroyed(o)); TLR_END();
}
TEST(refs_inline_layout_calls_object_metrics)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_inline *o = dwx_inline_new(30, 20, 15, FALSE); DWRITE_TEXT_METRICS m;
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 1)), S_OK);
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK);
    CHECK(o->n_metrics >= 1);                    /* the layout asks the object for its size */
    EXPECT_EQ(o->n_draw, 0);                     /* and does not draw it while measuring */
    IDWriteTextLayout_Release(l); dwx_release(o); TLR_END();
}
TEST(refs_inline_invalid_range_retains_nothing)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); dwx_inline *o = dwx_inline_new(30, 20, 15, FALSE);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 0)), S_OK); EXPECT_EQ(REFS(o), 1);
    CHECK_HR(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 0xFFFFFFFFu)), TLR_EINVALIDARG); EXPECT_EQ(REFS(o), 1);
    IDWriteTextLayout_Release(l); dwx_release(o); TLR_END();
}

/* ---- typography ---------------------------------------------------------------------------------------------------------- */
TEST(refs_typography_set_get_release)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); IDWriteTypography *t = NULL, *g; DWRITE_TEXT_RANGE r;
    DW_REQUIRE_OK(IDWriteFactory_CreateTypography(f, &t));
    EXPECT_REFS(t, 1);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, RNG(2, 3)), S_OK);
    EXPECT_REFS(t, 2);
    g = tlr_typo(l, 3, &r); CHECK(g == t); CHECK_RANGE(r, 2, 3); EXPECT_REFS(t, 3);
    DW_RELEASE(g); EXPECT_REFS(t, 2);
    g = tlr_typo(l, 0, &r); CHECK(g == NULL); CHECK_RANGE(r, 0, 2); EXPECT_REFS(t, 2);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, NULL, RNG(2, 3)), S_OK); EXPECT_REFS(t, 1);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, RNG(0, 10)), S_OK); EXPECT_REFS(t, 2);
    IDWriteTextLayout_Release(l); EXPECT_REFS(t, 1);
    IDWriteTypography_Release(t); TLR_END();
}
TEST(refs_typography_same_object_two_ranges)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); IDWriteTypography *t = NULL;
    DW_REQUIRE_OK(IDWriteFactory_CreateTypography(f, &t));
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, RNG(0, 2)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, RNG(5, 2)), S_OK);
    EXPECT_REFS(t, 3);
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, RNG(2, 3)), S_OK);       /* fills the gap: one run */
    t_note("typography refs after the runs merged: %lu", dw_refcount((IUnknown *)t));
    IDWriteTextLayout_Release(l); EXPECT_REFS(t, 1);
    IDWriteTypography_Release(t); TLR_END();
}
TEST(refs_typography_content_edits_after_set_are_accepted)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); IDWriteTypography *t = NULL; DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1 };
    DW_REQUIRE_OK(IDWriteFactory_CreateTypography(f, &t));
    CHECK_HR(IDWriteTextLayout_SetTypography(l, t, RNG(0, 10)), S_OK);
    CHECK_HR(IDWriteTypography_AddFontFeature(t, ft), S_OK);                   /* the layout holds the object, not a copy of its features */
    EXPECT_EQ(IDWriteTypography_GetFontFeatureCount(t), 1u);
    { DWRITE_TEXT_METRICS m; CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK); }
    IDWriteTextLayout_Release(l); IDWriteTypography_Release(t); TLR_END();
}

/* ---- font collection ----------------------------------------------------------------------------------------------------- */
TEST(refs_collection_set_get_release)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); IDWriteFontCollection *c2 = NULL, *g; DWRITE_TEXT_RANGE r; ULONG base;
    DW_REQUIRE_OK(dwx_coll_create(X.cl, f, &c2));
    EXPECT_REFS(c2, 1);
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, c2, RNG(2, 3)), S_OK);
    EXPECT_REFS(c2, 2);
    g = tlr_coll(l, 3, &r); CHECK(g == c2); CHECK_RANGE(r, 2, 3); EXPECT_REFS(c2, 3);
    DW_RELEASE(g); EXPECT_REFS(c2, 2);
    base = dw_refcount((IUnknown *)X.coll);
    g = tlr_coll(l, 0, &r); CHECK(g != c2); CHECK_RANGE(r, 0, 2); EXPECT_REFS(c2, 2);
    CHECK(g == X.coll); EXPECT_REFS(X.coll, base + 1); DW_RELEASE(g);
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, X.coll, RNG(2, 3)), S_OK);       /* back to the default collection */
    EXPECT_REFS(c2, 1);
    g = tlr_coll(l, 3, &r); CHECK(g == X.coll); CHECK_RANGE(r, 0, TLR_ALL); DW_RELEASE(g);
    IDWriteTextLayout_Release(l); IDWriteFontCollection_Release(c2); TLR_END();
}
TEST(refs_collection_layout_keeps_collection_alive)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); IDWriteFontCollection *c2 = NULL, *g;
    DW_REQUIRE_OK(dwx_coll_create(X.cl, f, &c2));
    CHECK_HR(IDWriteTextLayout_SetFontCollection(l, c2, RNG(0, 10)), S_OK);
    IDWriteFontCollection_Release(c2);                                  /* the layout holds the last reference */
    g = tlr_coll(l, 4, NULL); CHECK(g == c2);
    if (g) { CHECK_HR(IDWriteFontCollection_GetFontFamilyCount(g) > 0 ? S_OK : E_FAIL, S_OK); }
    DW_RELEASE(g);
    { DWRITE_TEXT_METRICS m; CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), S_OK); }
    IDWriteTextLayout_Release(l); TLR_END();
}

/* ---- layout object lifetime ----------------------------------------------------------------------------------------------- */
TEST(refs_layout_survives_factory_release)
{
    tlr_fx x; memset(&x, 0, sizeof x);
    if (!tlr_open(&x)) { tlr_close(&x); t_skip("fixture"); return; }
    IDWriteTextLayout *l = tlr_layout_(&x, TLR_FAM, T10, 16.0f);
    if (!l) { tlr_close(&x); t_skip("layout"); return; }
    dwx_effect *e = dwx_effect_new(1); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 5)), S_OK);
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 5)), S_OK);
    tlr_close(&x);                                  /* factory, collection and loaders released: the layout keeps what it needs */
    CHECK(tlr_weight(l, 2, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 0, 5);
    { IUnknown *g = tlr_effect(l, 2, NULL); CHECK(g == (IUnknown *)e); DW_RELEASE(g); }
    IDWriteTextLayout_Release(l);
    EXPECT_EQ(REFS(e), 1); dwx_release(e);
}
TEST(refs_layout_qi_identity_shares_run_table)
{
    TLR_BEGIN(); TLR_LAYOUT(l, T10, 16.0f); TLR_QI(l1, IDWriteTextLayout1, l); TLR_QI(l2, IDWriteTextLayout2, l); DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(1, 2)), S_OK);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(2, 3)), S_OK);
    /* the same run table is seen through every interface of the object: a pair-kerning change splits the weight run */
    CHECK(tlr_weight((IDWriteTextLayout *)l2, 1, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 1, 1);
    CHECK(tlr_weight((IDWriteTextLayout *)l1, 2, &r) == DWRITE_FONT_WEIGHT_BOLD); CHECK_RANGE(r, 2, 1);
    CHECK(tlr_weight(l, 3, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 3, 2);
    IDWriteTextLayout2_Release(l2); IDWriteTextLayout1_Release(l1); IDWriteTextLayout_Release(l); TLR_END();
}
