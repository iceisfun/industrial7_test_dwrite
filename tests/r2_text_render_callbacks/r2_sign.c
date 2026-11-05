/* The object returned by IDWriteFactory::CreateEllipsisTrimmingSign, exercised directly through its IDWriteInlineObject vtable.
 *
 * On Windows 10 it is an InlineLayout (a ComObject) wrapped around a private one-character IDWriteTextLayout ("U+2026" for a horizontal
 * format, U+22EE for a vertical one) created from the format with maxWidth = maxHeight = 0, natural measuring mode, ppd 1 and an identity
 * transform.  For a horizontal format its metrics are fixed at construction:
 *     width = widthIncludingTrailingWhitespace of that layout, height = 0, baseline = 0, supportsSideways = FALSE;
 *     break conditions NEUTRAL / NEUTRAL (S_OK);
 *     Draw(ctx, renderer, x, y, isSideways, isRightToLeft, effect) = layout->Draw(ctx, renderer, x - left, y - baseline): the layout's glyph
 *       baseline origin lands exactly on (x, y); isSideways, isRightToLeft and the effect are not looked at; the HRESULT is the layout's.
 *     overhang: left = ov.left + left, top = ov.top + top + baseline, right = layoutWidth + ov.right - left - width, bottom = layoutHeight + ov.bottom - y0.
 * QueryInterface answers IUnknown and IDWriteInlineObject only (E_NOINTERFACE, *ppv = NULL otherwise); the reference count starts at 1.
 */
#include "r2.h"

static IDWriteInlineObject *mk_sign(r2_fx *x, IDWriteTextFormat *t)
{
    IDWriteInlineObject *s = (IDWriteInlineObject *)(ULONG_PTR)1;      /* sentinel: must be overwritten */
    if (FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(x->f, t, &s))) return NULL;
    return s;
}
#define SIGN_BEGIN(size) R2_BEGIN(); IDWriteTextFormat *fmt = r2_format(&X, (size), DWRITE_FONT_WEIGHT_NORMAL); \
    IDWriteInlineObject *sign = fmt ? mk_sign(&X, fmt) : NULL; \
    if (!sign) { DW_RELEASE(fmt); R2_END(); t_skip("CreateEllipsisTrimmingSign prerequisite failed"); return; }
#define SIGN_END() do { DW_RELEASE(sign); DW_RELEASE(fmt); R2_END(); } while (0)

/* ---- creation / COM -------------------------------------------------------------------------------------------- */
RT(sign_null_format_invalidarg)
{
    R2_BEGIN(); IDWriteInlineObject *s = (IDWriteInlineObject *)(ULONG_PTR)1;
    /* DWriteFactory::CreateEllipsisTrimmingSign clears *sign first, then rejects a NULL format with E_INVALIDARG */
    CHECK_HR(IDWriteFactory_CreateEllipsisTrimmingSign(f, NULL, &s), E_INVALIDARG);
    CHECK_OUT_NULL(s);
    R2_END();
}
RT(sign_created_with_one_reference)
{
    SIGN_BEGIN(16.0f);
    EXPECT_REFS(sign, 1);
    SIGN_END();
}
RT(sign_does_not_keep_format_alive)
{
    SIGN_BEGIN(16.0f);
    ULONG before = dw_refcount((IUnknown *)fmt);
    /* the sign is built from a private layout; the format's own count is unchanged by releasing the sign */
    DW_RELEASE(sign);
    EXPECT_EQ(dw_refcount((IUnknown *)fmt), before);
    DW_RELEASE(fmt); R2_END();
}
RT(sign_qi_iunknown_identity)
{
    SIGN_BEGIN(16.0f); IUnknown *u = NULL, *u2 = NULL;
    CHECK_HR_OK(IDWriteInlineObject_QueryInterface(sign, &IID_IUnknown, (void **)&u));
    EXPECT_EQ(u, (IUnknown *)sign);
    EXPECT_REFS(sign, 2);
    CHECK_HR_OK(IUnknown_QueryInterface(u, &IID_IDWriteInlineObject, (void **)&u2));
    EXPECT_EQ(u2, (IUnknown *)sign);
    EXPECT_REFS(sign, 3);
    IUnknown_Release(u2); IUnknown_Release(u);
    EXPECT_REFS(sign, 1);
    SIGN_END();
}
RT(sign_qi_inline_object_interface)
{
    SIGN_BEGIN(16.0f); IDWriteInlineObject *o = NULL;
    CHECK_HR_OK(IDWriteInlineObject_QueryInterface(sign, &IID_IDWriteInlineObject, (void **)&o));
    EXPECT_EQ(o, sign);
    IDWriteInlineObject_Release(o);
    SIGN_END();
}
RT(sign_qi_every_other_public_interface_fails)
{
    int n = 0, i, tested = 0; const dw_qi_row *rows = dw_iid_table(&n);
    SIGN_BEGIN(16.0f);
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)1; HRESULT hr;
        if (IsEqualIID(rows[i].iid, &IID_IUnknown) || IsEqualIID(rows[i].iid, &IID_IDWriteInlineObject)) continue;
        hr = IDWriteInlineObject_QueryInterface(sign, rows[i].iid, &p);
        tested++;
        CHECKF(hr == E_NOINTERFACE, "QI(%s) -> %s", rows[i].name, dw_hr_name(hr));
        CHECKF(p == NULL, "QI(%s) left *ppv = %p", rows[i].name, p);
    }
    CHECK(tested > 20);
    EXPECT_REFS(sign, 1);
    SIGN_END();
}
RT(sign_qi_random_iid_fails)
{
    static const GUID bogus = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    SIGN_BEGIN(16.0f); void *p = (void *)(ULONG_PTR)1;
    CHECK_HR(IDWriteInlineObject_QueryInterface(sign, &bogus, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    SIGN_END();
}
RT(sign_addref_release_arithmetic)
{
    SIGN_BEGIN(16.0f);
    EXPECT_EQ(IDWriteInlineObject_AddRef(sign), 2); EXPECT_EQ(IDWriteInlineObject_AddRef(sign), 3);
    EXPECT_EQ(IDWriteInlineObject_Release(sign), 2); EXPECT_EQ(IDWriteInlineObject_Release(sign), 1);
    SIGN_END();
}
RT(sign_two_signs_are_distinct_objects)
{
    SIGN_BEGIN(16.0f);
    IDWriteInlineObject *s2 = mk_sign(&X, fmt);
    CHECK(s2 != NULL);
    if (s2) { CHECK(s2 != sign); EXPECT_REFS(s2, 1); EXPECT_REFS(sign, 1); IDWriteInlineObject_Release(s2); }
    SIGN_END();
}
RT(sign_outlives_its_format)
{
    SIGN_BEGIN(16.0f); DWRITE_INLINE_OBJECT_METRICS m;
    DW_RELEASE(fmt);
    memset(&m, 0, sizeof m);
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(sign, &m));
    CHECK_NEARF(m.width, R2_ADV(1000, 16));
    SIGN_END();
}

/* ---- GetMetrics / GetBreakConditions ------------------------------------------------------------------------- */
RT(sign_metrics_width_follows_font_size)
{
    static const float sizes[] = { 1.0f, 8.0f, 12.0f, 16.0f, 20.0f, 24.0f, 32.0f, 48.0f, 64.0f, 100.0f };
    unsigned i;
    R2_BEGIN();
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextFormat *t = r2_format(&X, sizes[i], DWRITE_FONT_WEIGHT_NORMAL); IDWriteInlineObject *s = t ? mk_sign(&X, t) : NULL;
        DWRITE_INLINE_OBJECT_METRICS m; memset(&m, 0xff, sizeof m);
        if (!s) { DW_RELEASE(t); t_check(0, __FILE__, __LINE__, "sign created", "size %g", sizes[i]); continue; }
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &m));
        /* the ellipsis maps to the 1000 unit triangle glyph of the fixture font */
        CHECKF(fabsf(m.width - R2_ADV(1000, sizes[i])) < 0.0001f, "size %g: width %g want %g", sizes[i], m.width, R2_ADV(1000, sizes[i]));
        CHECKF(m.height == 0.0f && m.baseline == 0.0f, "size %g: height %g baseline %g", sizes[i], m.height, m.baseline);
        EXPECT_EQ(m.supportsSideways, FALSE);
        IDWriteInlineObject_Release(s); IDWriteTextFormat_Release(t);
    }
    R2_END();
}
RT(sign_metrics_width_bold_face)
{
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_BOLD); IDWriteInlineObject *s = t ? mk_sign(&X, t) : NULL;
    DWRITE_INLINE_OBJECT_METRICS m;
    if (!s) { DW_RELEASE(t); R2_END(); t_skip("bold format unavailable"); return; }
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &m));
    CHECK_NEARF(m.width, R2_ADV(1200, 16));                     /* the bold face advance of the triangle glyph */
    IDWriteInlineObject_Release(s); IDWriteTextFormat_Release(t); R2_END();
}
RT(sign_metrics_all_fields_written)
{
    SIGN_BEGIN(16.0f); DWRITE_INLINE_OBJECT_METRICS m; memset(&m, 0xcd, sizeof m);
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(sign, &m));
    CHECK(m.width == m.width && m.height == m.height && m.baseline == m.baseline);   /* not NaN garbage */
    EXPECT_EQ(m.height, 0); EXPECT_EQ(m.baseline, 0);
    EXPECT_EQ((unsigned)m.supportsSideways, 0);                                    /* a full BOOL, not just a byte */
    SIGN_END();
}
RT(sign_metrics_repeatable_bitwise)
{
    SIGN_BEGIN(16.0f); DWRITE_INLINE_OBJECT_METRICS a, b; int i;
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(sign, &a));
    for (i = 0; i < 5; i++) { CHECK_HR_OK(IDWriteInlineObject_GetMetrics(sign, &b)); CHECK(!memcmp(&a, &b, sizeof a)); }
    SIGN_END();
}
RT(sign_metrics_fixed_at_creation)
{
    SIGN_BEGIN(16.0f); DWRITE_INLINE_OBJECT_METRICS a, b;
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(sign, &a));
    IDWriteTextFormat_SetTextAlignment(fmt, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat_SetReadingDirection(fmt, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextFormat_SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_NO_WRAP);
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(sign, &b));
    CHECK(!memcmp(&a, &b, sizeof a));
    SIGN_END();
}
RT(sign_metrics_identical_for_rtl_format)
{
    SIGN_BEGIN(16.0f); DWRITE_INLINE_OBJECT_METRICS a, b; IDWriteInlineObject *s2;
    IDWriteTextFormat_SetReadingDirection(fmt, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    s2 = mk_sign(&X, fmt);
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(sign, &a));
    if (s2) {
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s2, &b));
        CHECK_FBITS(b.width, a.width); EXPECT_EQ(b.height, 0); EXPECT_EQ(b.baseline, 0); EXPECT_EQ(b.supportsSideways, FALSE);
        IDWriteInlineObject_Release(s2);
    }
    SIGN_END();
}
RT(sign_break_conditions_neutral)
{
    SIGN_BEGIN(16.0f); DWRITE_BREAK_CONDITION b = (DWRITE_BREAK_CONDITION)0x7f, a = (DWRITE_BREAK_CONDITION)0x7f;
    CHECK_HR_OK(IDWriteInlineObject_GetBreakConditions(sign, &b, &a));
    EXPECT_EQ(b, DWRITE_BREAK_CONDITION_NEUTRAL); EXPECT_EQ(a, DWRITE_BREAK_CONDITION_NEUTRAL);
    SIGN_END();
}
RT(sign_break_conditions_independent_of_format)
{
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 24.0f, DWRITE_FONT_WEIGHT_BOLD); IDWriteInlineObject *s = t ? mk_sign(&X, t) : NULL;
    DWRITE_BREAK_CONDITION b = DWRITE_BREAK_CONDITION_MUST_BREAK, a = DWRITE_BREAK_CONDITION_MUST_BREAK;
    if (!s) { DW_RELEASE(t); R2_END(); t_skip("bold format unavailable"); return; }
    IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_NO_WRAP);
    CHECK_HR_OK(IDWriteInlineObject_GetBreakConditions(s, &b, &a));
    EXPECT_EQ(b, 0); EXPECT_EQ(a, 0);
    IDWriteInlineObject_Release(s); IDWriteTextFormat_Release(t); R2_END();
}

/* ---- overhang metrics ------------------------------------------------------------------------------------------ */
RT(sign_overhang_matches_private_layout_formula)
{
    SIGN_BEGIN(16.0f);
    IDWriteTextLayout *tw = NULL; IDWriteTextLayout2 *tw2 = NULL;
    DWRITE_OVERHANG_METRICS sov, lov; DWRITE_TEXT_METRICS tm; DWRITE_LINE_METRICS lm[4]; UINT32 nl = 0;
    DWRITE_INLINE_OBJECT_METRICS im;
    if (FAILED(IDWriteFactory_CreateTextLayout(f, L"\x2026", 1, fmt, 0.0f, 0.0f, &tw)) || !tw) { SIGN_END(); t_skip("twin layout unavailable"); return; }
    if (SUCCEEDED(IDWriteTextLayout_QueryInterface(tw, &IID_IDWriteTextLayout2, (void **)&tw2))) { IDWriteTextLayout2_SetVerticalGlyphOrientation(tw2, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED); IDWriteTextLayout2_Release(tw2); }
    memset(&sov, 0, sizeof sov);
    CHECK_HR_OK(IDWriteInlineObject_GetOverhangMetrics(sign, &sov));
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(tw, &lov));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(tw, &tm));
    CHECK_HR_OK(IDWriteTextLayout_GetLineMetrics(tw, lm, 4, &nl));
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(sign, &im));
    EXPECT_EQ(nl, 1);
    if (nl == 1) {
        float x0 = tm.left, y0 = tm.top + lm[0].baseline;
        CHECK_NEAR(sov.left, lov.left + x0, 0.0001);
        CHECK_NEAR(sov.top, lov.top + y0, 0.0001);
        CHECK_NEAR(sov.right, tm.layoutWidth + lov.right - x0 - im.width, 0.0001);
        CHECK_NEAR(sov.bottom, tm.layoutHeight + lov.bottom - y0 - im.height, 0.0001);
        t_note("sign overhang l %g t %g r %g b %g; twin layout overhang l %g t %g r %g b %g", sov.left, sov.top, sov.right, sov.bottom, lov.left, lov.top, lov.right, lov.bottom);
    }
    IDWriteTextLayout_Release(tw);
    SIGN_END();
}
RT(sign_overhang_horizontal_ink_bearings)
{
    SIGN_BEGIN(16.0f); DWRITE_OVERHANG_METRICS ov; memset(&ov, 0, sizeof ov);
    CHECK_HR_OK(IDWriteInlineObject_GetOverhangMetrics(sign, &ov));
    /* how DirectWrite derives layout overhangs (ink box vs advance box) is font dependent: record only */
    t_note("sign overhang left %g right %g top %g bottom %g (fixture glyph side bearings: 0.78125 each)", ov.left, ov.right, ov.top, ov.bottom);
    SIGN_END();
}
RT(sign_overhang_scales_with_size)
{
    static const float sizes[] = { 8.0f, 16.0f, 32.0f };
    unsigned i;
    R2_BEGIN();
    for (i = 0; i < 3; i++) {
        IDWriteTextFormat *t = r2_format(&X, sizes[i], DWRITE_FONT_WEIGHT_NORMAL); IDWriteInlineObject *s = t ? mk_sign(&X, t) : NULL;
        DWRITE_OVERHANG_METRICS ov; memset(&ov, 0, sizeof ov);
        if (!s) { DW_RELEASE(t); continue; }
        CHECK_HR_OK(IDWriteInlineObject_GetOverhangMetrics(s, &ov));
        t_note("size %g: sign overhang left %g right %g top %g bottom %g", sizes[i], ov.left, ov.right, ov.top, ov.bottom);
        IDWriteInlineObject_Release(s); IDWriteTextFormat_Release(t);
    }
    R2_END();
}

/* ---- Draw ---------------------------------------------------------------------------------------------------- */
RT(sign_draw_baseline_lands_on_origin)
{
    static const float pts[][2] = { { 0, 0 }, { 50, 70 }, { -12.5f, 3.25f }, { 1000, 2000 }, { 0.1f, 0.9f }, { 33.333f, -44.444f } };
    unsigned i;
    SIGN_BEGIN(16.0f);
    for (i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        rh *r = rh_new(); const rh_ev *g; r->snap_val = TRUE;
        CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)r, pts[i][0], pts[i][1], FALSE, FALSE, NULL));
        EXPECT_EQ(r->count[RH_GLYPH], 1);
        EXPECT_EQ(r->count[RH_INLINE] + r->count[RH_UNDER] + r->count[RH_STRIKE], 0);
        g = rh_nth(r, RH_GLYPH, 0);
        if (g) { CHECKF(fabsf(g->x - pts[i][0]) < 0.001f && fabsf(g->y - pts[i][1]) < 0.001f, "origin (%g,%g) drew at (%g,%g)", pts[i][0], pts[i][1], g->x, g->y); }
        rh_free(r);
    }
    SIGN_END();
}
RT(sign_draw_glyph_run_fields)
{
    SIGN_BEGIN(16.0f); rh *r = rh_new(); const rh_ev *g; r->snap_val = TRUE;
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)r, 5.0f, 9.0f, FALSE, FALSE, NULL));
    g = rh_nth(r, RH_GLYPH, 0);
    CHECK(g != NULL);
    if (g) {
        EXPECT_EQ(g->n, 1); CHECK_NEARF(g->em, 16.0f); EXPECT_EQ(g->idx[0], TTF_GID_TRIANGLE); CHECK_NEARF(g->adv[0], R2_ADV(1000, 16));
        EXPECT_EQ(g->sideways, FALSE); EXPECT_EQ(g->bidi, 0);
        EXPECT_EQ(g->mm, DWRITE_MEASURING_MODE_NATURAL);
        EXPECT_EQ(g->has_desc, 1); EXPECT_EQ(g->tlen, 1); EXPECT_EQ(g->text[0], 0x2026); EXPECT_EQ(g->cmap[0], 0);
        CHECK(!_wcsicmp(g->locale, R2_LOC));
        EXPECT_EQ(g->ctx, CTX_A); EXPECT_EQ(g->effect, NULL);
        CHECK(g->face != NULL);
        CHECK(g->offsets_ptr == NULL || g->off[0].advanceOffset == 0);
    }
    rh_free(r); SIGN_END();
}
RT(sign_draw_snapping_protocol_runs_inside_draw)
{
    SIGN_BEGIN(16.0f); rh *r = rh_new();
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_B, (IDWriteTextRenderer *)r, 0.0f, 0.0f, FALSE, FALSE, NULL));
    EXPECT_EQ(r->count[RH_SNAP], 1); EXPECT_EQ(r->count[RH_PPD], 1); EXPECT_EQ(r->count[RH_XFORM], 1);
    { int i; for (i = 0; i < r->nev; i++) if (r->ev[i].type != RH_QI) EXPECT_EQ(r->ev[i].ctx, CTX_B); }
    rh_free(r); SIGN_END();
}
RT(sign_draw_ignores_sideways_and_rtl_arguments)
{
    SIGN_BEGIN(16.0f); rh *a = rh_new(), *b = rh_new(), *c = rh_new(); const rh_ev *ga, *gb, *gc;
    a->snap_val = b->snap_val = c->snap_val = TRUE;
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)a, 20.0f, 30.0f, FALSE, FALSE, NULL));
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)b, 20.0f, 30.0f, TRUE, FALSE, NULL));
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)c, 20.0f, 30.0f, TRUE, TRUE, NULL));
    ga = rh_nth(a, RH_GLYPH, 0); gb = rh_nth(b, RH_GLYPH, 0); gc = rh_nth(c, RH_GLYPH, 0);
    if (ga && gb && gc) {
        CHECK_FBITS(gb->x, ga->x); CHECK_FBITS(gb->y, ga->y); CHECK_FBITS(gc->x, ga->x); CHECK_FBITS(gc->y, ga->y);
        EXPECT_EQ(gb->sideways, FALSE); EXPECT_EQ(gc->sideways, FALSE); EXPECT_EQ(gc->bidi, ga->bidi);
    } else CHECK(0);
    rh_free(a); rh_free(b); rh_free(c); SIGN_END();
}
RT(sign_draw_ignores_effect_argument)
{
    SIGN_BEGIN(16.0f); rh *r = rh_new(); dwx_effect *fx = dwx_effect_new(7); const rh_ev *g;
    r->snap_val = TRUE;
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)r, 1.0f, 2.0f, FALSE, FALSE, (IUnknown *)fx));
    g = rh_nth(r, RH_GLYPH, 0);
    CHECK(g != NULL); if (g) EXPECT_EQ(g->effect, NULL);
    EXPECT_EQ(dwx_refs(fx), 1);
    dwx_release(fx); rh_free(r); SIGN_END();
}
RT(sign_draw_null_renderer_invalidarg)
{
    if (t_is_wine()) { t_skip("Wine dereferences a NULL renderer"); return; }
    SIGN_BEGIN(16.0f);
    CHECK_HR(IDWriteInlineObject_Draw(sign, CTX_A, NULL, 0.0f, 0.0f, FALSE, FALSE, NULL), E_INVALIDARG);
    EXPECT_REFS(sign, 1);
    SIGN_END();
}
RT(sign_draw_returns_snapping_failures)
{
    static const HRESULT codes[] = { E_FAIL, E_NOTIMPL, E_ABORT, (HRESULT)0x8898500A };
    unsigned i; int t;
    SIGN_BEGIN(16.0f);
    for (t = RH_SNAP; t <= RH_XFORM; t++) for (i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        rh *r = rh_new(); r->ret_set[t] = 1; r->ret[t] = codes[i];
        HRESULT hr = IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)r, 0.0f, 0.0f, FALSE, FALSE, NULL);
        CHECKF(hr == codes[i], "callback %d returned %s: sign Draw returned %s", t, dw_hr_name(codes[i]), dw_hr_name(hr));
        EXPECT_EQ(r->count[RH_GLYPH], 0);
        rh_free(r);
    }
    SIGN_END();
}
RT(sign_draw_glyph_callback_failure_ignored)
{
    SIGN_BEGIN(16.0f); rh *r = rh_new(); r->ret_set[RH_GLYPH] = 1; r->ret[RH_GLYPH] = E_FAIL;
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)r, 0.0f, 0.0f, FALSE, FALSE, NULL));
    EXPECT_EQ(r->count[RH_GLYPH], 1);
    rh_free(r); SIGN_END();
}
RT(sign_draw_repeatable)
{
    SIGN_BEGIN(16.0f); rh *r = rh_new(); int i; r->snap_val = TRUE;
    for (i = 0; i < 4; i++) CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)r, 8.0f, 9.0f, FALSE, FALSE, NULL));
    EXPECT_EQ(r->count[RH_GLYPH], 4); EXPECT_EQ(r->count[RH_SNAP], 4);
    for (i = 1; i < 4; i++) { const rh_ev *a = rh_nth(r, RH_GLYPH, 0), *b = rh_nth(r, RH_GLYPH, i); if (a && b) { CHECK_FBITS(b->x, a->x); CHECK_FBITS(b->y, a->y); } }
    EXPECT_REFS(sign, 1);
    rh_free(r); SIGN_END();
}
RT(sign_draw_with_renderer1_gets_angle_zero)
{
    SIGN_BEGIN(16.0f); rh *r = rh_new(); const rh_ev *g; r->allow_r1 = 1; r->snap_val = TRUE;
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)r, 4.0f, 6.0f, FALSE, FALSE, NULL));
    g = rh_nth(r, RH_GLYPH, 0);
    CHECK(g != NULL);
    if (g) { EXPECT_EQ(g->r1, 1); EXPECT_EQ(g->angle, DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES); CHECK_NEARF(g->x, 4.0f); CHECK_NEARF(g->y, 6.0f); }
    rh_free(r); SIGN_END();
}
RT(sign_draw_snapping_enabled_rounds_baseline_y)
{
    /* with pixel snapping on (ppd 1, identity) the layout snaps the baseline y to the pixel grid: 9.4 -> 9, x is left alone */
    SIGN_BEGIN(16.0f); rh *r = rh_new(); const rh_ev *g;
    CHECK_HR_OK(IDWriteInlineObject_Draw(sign, CTX_A, (IDWriteTextRenderer *)r, 5.3f, 9.4f, FALSE, FALSE, NULL));
    g = rh_nth(r, RH_GLYPH, 0);
    CHECK(g != NULL);
    if (g) { CHECK_NEAR(g->y, 9.0, 0.01); CHECK_NEAR(g->x, 5.3, 0.01); }
    rh_free(r); SIGN_END();
}

/* origin independence from the format's alignment properties */
RT(sign_draw_origin_independent_of_alignment_and_direction)
{
    static const DWRITE_TEXT_ALIGNMENT ta[] = { DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_TEXT_ALIGNMENT_JUSTIFIED };
    static const DWRITE_PARAGRAPH_ALIGNMENT pa[] = { DWRITE_PARAGRAPH_ALIGNMENT_NEAR, DWRITE_PARAGRAPH_ALIGNMENT_FAR, DWRITE_PARAGRAPH_ALIGNMENT_CENTER };
    static const DWRITE_READING_DIRECTION rd[] = { DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT };
    unsigned i, j, k;
    R2_BEGIN();
    for (i = 0; i < 4; i++) for (j = 0; j < 3; j++) for (k = 0; k < 2; k++) {
        IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_NORMAL); IDWriteInlineObject *s; rh *r; const rh_ev *g;
        if (!t) continue;
        IDWriteTextFormat_SetTextAlignment(t, ta[i]); IDWriteTextFormat_SetParagraphAlignment(t, pa[j]); IDWriteTextFormat_SetReadingDirection(t, rd[k]);
        s = mk_sign(&X, t);
        if (!s) { IDWriteTextFormat_Release(t); t_check(0, __FILE__, __LINE__, "sign created", "alignment %u/%u/%u", i, j, k); continue; }
        r = rh_new(); r->snap_val = TRUE;
        CHECK_HR_OK(IDWriteInlineObject_Draw(s, CTX_A, (IDWriteTextRenderer *)r, 40.0f, 60.0f, FALSE, FALSE, NULL));
        g = rh_nth(r, RH_GLYPH, 0);
        if (g) CHECKF(fabsf(g->x - 40.0f) < 0.001f && fabsf(g->y - 60.0f) < 0.001f, "text %u para %u dir %u: drawn at (%g,%g)", i, j, k, g->x, g->y);
        else CHECK(0);
        { DWRITE_INLINE_OBJECT_METRICS m; CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &m)); CHECKF(fabsf(m.width - R2_ADV(1000, 16)) < 0.0001f, "alignment %u/%u/%u: width %g", i, j, k, m.width); }
        rh_free(r); IDWriteInlineObject_Release(s); IDWriteTextFormat_Release(t);
    }
    R2_END();
}
RT(sign_draw_rtl_format_glyph_level)
{
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_NORMAL); IDWriteInlineObject *s; rh *r = rh_new(); const rh_ev *g;
    if (!t) { rh_free(r); R2_END(); t_skip("format unavailable"); return; }
    IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    s = mk_sign(&X, t); r->snap_val = TRUE;
    if (s) {
        CHECK_HR_OK(IDWriteInlineObject_Draw(s, CTX_A, (IDWriteTextRenderer *)r, 0.0f, 0.0f, FALSE, FALSE, NULL));
        g = rh_nth(r, RH_GLYPH, 0);
        if (g) { EXPECT_EQ(g->text[0], 0x2026); t_note("ellipsis in a right-to-left paragraph: bidiLevel %u", g->bidi); CHECK(g->bidi == 0 || g->bidi == 1); }
        IDWriteInlineObject_Release(s);
    }
    rh_free(r); IDWriteTextFormat_Release(t); R2_END();
}
RT(sign_draw_follows_format_locale)
{
    R2_BEGIN(); IDWriteTextFormat *t = NULL; IDWriteInlineObject *s; rh *r = rh_new(); const rh_ev *g;
    if (FAILED(IDWriteFactory_CreateTextFormat(f, R2_FAM, X.coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"de-DE", &t))) { rh_free(r); R2_END(); t_skip("format unavailable"); return; }
    s = mk_sign(&X, t); r->snap_val = TRUE;
    if (s) {
        CHECK_HR_OK(IDWriteInlineObject_Draw(s, CTX_A, (IDWriteTextRenderer *)r, 0.0f, 0.0f, FALSE, FALSE, NULL));
        g = rh_nth(r, RH_GLYPH, 0);
        CHECK(g != NULL); if (g) CHECKF(!_wcsicmp(g->locale, L"de-DE"), "locale \"%ls\"", g->locale);
        IDWriteInlineObject_Release(s);
    }
    rh_free(r); IDWriteTextFormat_Release(t); R2_END();
}
RT(sign_draw_follows_format_size_and_weight)
{
    static const struct { float size; DWRITE_FONT_WEIGHT w; int adv; } rows[] = {
        { 10.0f, DWRITE_FONT_WEIGHT_NORMAL, 1000 }, { 16.0f, DWRITE_FONT_WEIGHT_NORMAL, 1000 }, { 40.0f, DWRITE_FONT_WEIGHT_NORMAL, 1000 },
        { 10.0f, DWRITE_FONT_WEIGHT_BOLD, 1200 }, { 16.0f, DWRITE_FONT_WEIGHT_BOLD, 1200 }, { 40.0f, DWRITE_FONT_WEIGHT_BOLD, 1200 },
    };
    unsigned i;
    R2_BEGIN();
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        IDWriteTextFormat *t = r2_format(&X, rows[i].size, rows[i].w); IDWriteInlineObject *s = t ? mk_sign(&X, t) : NULL; rh *r = rh_new(); const rh_ev *g;
        r->snap_val = TRUE;
        if (s) {
            CHECK_HR_OK(IDWriteInlineObject_Draw(s, CTX_A, (IDWriteTextRenderer *)r, 0.0f, 0.0f, FALSE, FALSE, NULL));
            g = rh_nth(r, RH_GLYPH, 0);
            if (g) { CHECK_NEARF(g->em, rows[i].size); CHECK_NEAR(g->adv[0], R2_ADV(rows[i].adv, rows[i].size), 0.0001); } else CHECK(0);
            IDWriteInlineObject_Release(s);
        }
        rh_free(r); DW_RELEASE(t);
    }
    R2_END();
}

/* ---- the sign inside a trimmed layout -------------------------------------------------------------------------- */
typedef struct { int fired; } rec_arg;
static void recurse_inline(rh *r, rh_ev *e, void *arg)
{
    rec_arg *a = (rec_arg *)arg;
    if (e->type != RH_INLINE || a->fired || !e->obj) return;
    a->fired = 1;
    IDWriteInlineObject_Draw(e->obj, e->ctx, (IDWriteTextRenderer *)r, e->x, e->y, e->osw, e->ortl, e->effect);
}
RT(sign_in_trimmed_layout_draws_on_baseline_after_text)
{
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_NORMAL); IDWriteTextLayout *l = NULL; IDWriteInlineObject *s = NULL;
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    rh *r = rh_new(); rec_arg a = { 0 };
    if (!t || FAILED(IDWriteFactory_CreateTextLayout(f, L"ABCDEFGHIJKLMNOP", 16, t, 60.0f, 100.0f, &l)) ||
        FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)) || !s ||
        FAILED(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP)) || FAILED(IDWriteTextLayout_SetTrimming(l, &tr, s))) {
        DW_RELEASE(s); DW_RELEASE(l); DW_RELEASE(t); rh_free(r); R2_END(); t_skip("trimming setup failed"); return;
    }
    r->snap_val = TRUE; r->hook = recurse_inline; r->hook_arg = &a;
    RH_DRAW_OK(l, r, 10.0f, 20.0f);
    EXPECT_EQ(r->count[RH_INLINE], 1);
    {
        const rh_ev *in = rh_nth(r, RH_INLINE, 0), *g0 = rh_nth(r, RH_GLYPH, 0);
        float used = 0; int i, sign_glyphs = 0;
        for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH && !(r->ev[i].tlen == 1 && r->ev[i].text[0] == 0x2026)) used += rh_run_width(&r->ev[i]);
        if (in && g0) {
            EXPECT_EQ(in->osw, FALSE); EXPECT_EQ(in->ortl, FALSE); EXPECT_EQ(in->effect, NULL);
            CHECK_NEAR(in->y, g0->y, 0.01);                          /* the sign has baseline 0: its top-left is on the text baseline */
            CHECK_NEAR(in->x, 10.0f + used, 0.01);                   /* immediately after the kept text */
        }
        for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH && r->ev[i].tlen == 1 && r->ev[i].text[0] == 0x2026) {
            sign_glyphs++;
            if (in) { CHECK_NEAR(r->ev[i].x, in->x, 0.01); CHECK_NEAR(r->ev[i].y, in->y, 0.01); }
        }
        EXPECT_EQ(sign_glyphs, 1);
        CHECK(used > 0 && used + R2_ADV(1000, 16) <= 60.0f + 0.01f);
    }
    DW_RELEASE(s); IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t); rh_free(r); R2_END();
}
RT(sign_in_trimmed_layout_not_drawn_when_text_fits)
{
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_NORMAL); IDWriteTextLayout *l = NULL; IDWriteInlineObject *s = NULL;
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; rh *r = rh_new();
    if (!t || FAILED(IDWriteFactory_CreateTextLayout(f, L"ABC", 3, t, 200.0f, 100.0f, &l)) ||
        FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)) || !s || FAILED(IDWriteTextLayout_SetTrimming(l, &tr, s))) {
        DW_RELEASE(s); DW_RELEASE(l); DW_RELEASE(t); rh_free(r); R2_END(); t_skip("trimming setup failed"); return;
    }
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_INLINE], 0);
    EXPECT_EQ(rh_total_glyphs(r), 3);
    EXPECT_REFS(s, 2);                                          /* ours + the layout's */
    DW_RELEASE(s); IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t); rh_free(r); R2_END();
}
