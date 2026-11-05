/* HRESULT / out-parameter protocol between IDWriteTextLayout::Draw and the renderer's pixel snapping and drawing callbacks.
 *
 * TextLayout::Draw: IsPixelSnappingDisabled gets *isDisabled preset to FALSE; every non-zero answer counts as disabled and skips
 * GetPixelsPerDip / GetCurrentTransform; each of the three HRESULTs goes through CallbackException::ThrowIfFailed, i.e. negative
 * values abort Draw and are returned by DWriteTextLayout::Draw, non-negative values (S_FALSE, ...) are accepted and Draw returns S_OK.
 * The four Draw* helpers never look at the HRESULT of DrawGlyphRun / DrawInlineObject / DrawUnderline / DrawStrikethrough.
 */
#include "r2.h"

static IDWriteTextLayout *mk_full(r2_fx *x, dwx_inline **io)
{
    IDWriteTextLayout *l = r2_layout_w(x, L"AB#CD", 16.0f, 200.0f, 60.0f, DWRITE_FONT_WEIGHT_NORMAL);
    if (!l) return NULL;
    *io = dwx_inline_new(10.0f, 14.0f, 10.0f, FALSE);
    IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)*io, RNG(2, 1));
    IDWriteTextLayout_SetUnderline(l, TRUE, RNG(0, 2));
    IDWriteTextLayout_SetStrikethrough(l, TRUE, RNG(3, 2));
    return l;
}
#define FULL_BEGIN() R2_BEGIN(); dwx_inline *io = NULL; IDWriteTextLayout *l = mk_full(&X, &io); \
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; } rh *r = rh_new()
#define FULL_END() do { rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END(); } while (0)

static const struct { HRESULT hr; const char *name; } k_fail[] = {
    { E_FAIL, "E_FAIL" }, { E_NOTIMPL, "E_NOTIMPL" }, { E_ABORT, "E_ABORT" }, { E_OUTOFMEMORY, "E_OUTOFMEMORY" },
    { E_INVALIDARG, "E_INVALIDARG" }, { E_UNEXPECTED, "E_UNEXPECTED" }, { E_ACCESSDENIED, "E_ACCESSDENIED" }, { E_POINTER, "E_POINTER" },
    { (HRESULT)0x88985002, "DWRITE_E_FILENOTFOUND" }, { (HRESULT)0x8898500A, "DWRITE_E_TEXTRENDERERINCOMPATIBLE" },
    { (HRESULT)0x80000000, "0x80000000" }, { (HRESULT)0xFFFFFFFF, "0xFFFFFFFF" }, { (HRESULT)0x8007007A, "ERROR_INSUFFICIENT_BUFFER" },
    { (HRESULT)0xC0000005, "0xC0000005" },
};
#define NFAIL ((int)(sizeof k_fail / sizeof k_fail[0]))
static const HRESULT k_ok[] = { S_FALSE, 2, 0x00040000, 0x7fffffff };

/* ---- out parameter preset ------------------------------------------------------------------------------------ */
RT(snap_untouched_out_param_means_enabled)
{
    FULL_BEGIN();
    r->snap_write = 0;                 /* S_OK without writing *isDisabled: the preset FALSE stands */
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_SNAP], 1); EXPECT_EQ(r->count[RH_PPD], 1); EXPECT_EQ(r->count[RH_XFORM], 1);
    CHECK(r->count[RH_GLYPH] >= 1);
    FULL_END();
}
RT(snap_untouched_out_param_with_s_false_means_enabled)
{
    FULL_BEGIN();
    r->snap_write = 0; r->ret_set[RH_SNAP] = 1; r->ret[RH_SNAP] = S_FALSE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_PPD], 1); EXPECT_EQ(r->count[RH_XFORM], 1);
    FULL_END();
}
RT(snap_nonzero_values_all_disable)
{
    static const BOOL vals[] = { 1, 2, 3, 0x100, 0x7fffffff, -1, (BOOL)0x80000000 };
    unsigned i;
    FULL_BEGIN();
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        rh_clear(r); r->snap_val = vals[i];
        CHECK_HR_OK(rh_draw(l, r, CTX_A, 0.0f, 0.0f));
        CHECKF(r->count[RH_PPD] == 0 && r->count[RH_XFORM] == 0, "BOOL 0x%x: ppd %d xform %d", vals[i], r->count[RH_PPD], r->count[RH_XFORM]);
        CHECKF(r->count[RH_SNAP] == 1 && r->count[RH_GLYPH] >= 1, "BOOL 0x%x still draws", vals[i]);
    }
    FULL_END();
}
RT(snap_zero_value_enables)
{
    FULL_BEGIN();
    r->snap_val = 0;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_PPD], 1); EXPECT_EQ(r->count[RH_XFORM], 1);
    FULL_END();
}
RT(snap_disabled_value_returned_with_s_false_still_disables)
{
    FULL_BEGIN();
    r->snap_val = TRUE; r->ret_set[RH_SNAP] = 1; r->ret[RH_SNAP] = S_FALSE;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_PPD], 0); EXPECT_EQ(r->count[RH_XFORM], 0);
    FULL_END();
}
RT(snap_disabled_origin_unrounded)
{
    FULL_BEGIN();
    r->snap_val = TRUE;
    RH_DRAW_OK(l, r, 10.3f, 20.7f);
    { const rh_ev *g = rh_nth(r, RH_GLYPH, 0); if (g) CHECK_NEARF(g->x, 10.3f); }
    { const rh_ev *u = rh_nth(r, RH_UNDER, 0); if (u) CHECK_NEARF(u->x, 10.3f); }
    FULL_END();
}

/* ---- success codes ------------------------------------------------------------------------------------------ */
static void ok_codes_for(int type)
{
    unsigned i;
    FULL_BEGIN();
    for (i = 0; i < sizeof k_ok / sizeof k_ok[0]; i++) {
        rh_clear(r); r->ret_set[type] = 1; r->ret[type] = k_ok[i];
        HRESULT hr = rh_draw(l, r, CTX_A, 0.0f, 0.0f);
        /* DWriteTextLayout::Draw returns 0 on completion, never the callback's positive code */
        CHECKF(hr == S_OK, "callback returned 0x%lx: Draw returned %s", (unsigned long)k_ok[i], dw_hr_name(hr));
        CHECKF(r->count[RH_GLYPH] >= 2 && r->count[RH_UNDER] == 1, "callback code 0x%lx still draws", (unsigned long)k_ok[i]);
    }
    FULL_END();
}
RT(snap_success_codes_from_snapping_callback) { ok_codes_for(RH_SNAP); }
RT(snap_success_codes_from_ppd_callback) { ok_codes_for(RH_PPD); }
RT(snap_success_codes_from_transform_callback) { ok_codes_for(RH_XFORM); }

/* ---- failure codes ------------------------------------------------------------------------------------------- */
static void fail_codes_for(int type)
{
    int i;
    FULL_BEGIN();
    for (i = 0; i < NFAIL; i++) {
        rh_clear(r); r->ret_set[type] = 1; r->ret[type] = k_fail[i].hr;
        HRESULT hr = rh_draw(l, r, CTX_A, 0.0f, 0.0f);
        CHECKF(hr == k_fail[i].hr, "%s from callback %d: Draw returned %s", k_fail[i].name, type, dw_hr_name(hr));
        EXPECT_EQ(r->count[RH_GLYPH] + r->count[RH_INLINE] + r->count[RH_UNDER] + r->count[RH_STRIKE], 0);
        EXPECT_EQ(r->count[RH_SNAP], 1);
        EXPECT_EQ(r->count[RH_PPD], type == RH_SNAP ? 0 : 1);
        EXPECT_EQ(r->count[RH_XFORM], type == RH_XFORM ? 1 : 0);
    }
    FULL_END();
}
RT(snap_failure_codes_from_snapping_callback) { fail_codes_for(RH_SNAP); }
RT(snap_failure_codes_from_ppd_callback) { fail_codes_for(RH_PPD); }
RT(snap_failure_codes_from_transform_callback) { fail_codes_for(RH_XFORM); }
RT(snap_notimpl_from_snapping_callback_aborts)
{
    FULL_BEGIN();
    r->ret_set[RH_SNAP] = 1; r->ret[RH_SNAP] = E_NOTIMPL;
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_NOTIMPL);
    EXPECT_EQ(r->nev - r->count[RH_QI], 1);                  /* only IsPixelSnappingDisabled was called */
    FULL_END();
}
RT(snap_notimpl_wins_over_written_disabled_flag)
{
    FULL_BEGIN();
    r->snap_val = TRUE; r->ret_set[RH_SNAP] = 1; r->ret[RH_SNAP] = E_NOTIMPL;
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_NOTIMPL);
    EXPECT_EQ(r->count[RH_GLYPH], 0);
    FULL_END();
}
RT(snap_notimpl_from_ppd_callback_aborts_before_transform)
{
    FULL_BEGIN();
    r->ret_set[RH_PPD] = 1; r->ret[RH_PPD] = E_NOTIMPL;
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_NOTIMPL);
    EXPECT_EQ(r->count[RH_XFORM], 0);
    FULL_END();
}
RT(snap_failure_then_recovery_on_next_draw)
{
    FULL_BEGIN();
    r->ret_set[RH_SNAP] = 1; r->ret[RH_SNAP] = E_FAIL;
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_FAIL);
    rh_clear(r); r->ret_set[RH_SNAP] = 0;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    CHECK(r->count[RH_GLYPH] >= 2); EXPECT_EQ(r->count[RH_UNDER], 1); EXPECT_EQ(r->count[RH_STRIKE], 1); EXPECT_EQ(r->count[RH_INLINE], 1);
    FULL_END();
}
RT(snap_failure_second_call_only)
{
    /* the failure is taken from the first Draw's snapping callback only; the second Draw has its own call */
    FULL_BEGIN();
    r->ret_set[RH_SNAP] = 1; r->ret[RH_SNAP] = E_ABORT; r->ret_nth[RH_SNAP] = 2;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    CHECK(r->count[RH_GLYPH] >= 1);
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_ABORT);
    FULL_END();
}
RT(snap_failure_hr_not_cached_in_layout)
{
    FULL_BEGIN();
    rh *r2 = rh_new();
    r->ret_set[RH_XFORM] = 1; r->ret[RH_XFORM] = E_FAIL;
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_FAIL);
    RH_DRAW_OK(l, r2, 0.0f, 0.0f);                           /* another renderer on the same layout is unaffected */
    CHECK(r2->count[RH_GLYPH] >= 2);
    rh_free(r2);
    FULL_END();
}
RT(snap_failure_with_renderer1_still_aborts)
{
    FULL_BEGIN();
    r->allow_r1 = 1; r->ret_set[RH_PPD] = 1; r->ret[RH_PPD] = E_UNEXPECTED;
    CHECK_HR(rh_draw(l, r, CTX_A, 0.0f, 0.0f), E_UNEXPECTED);
    EXPECT_EQ(r->count[RH_GLYPH], 0);
    t_note("renderer refcount after an aborted Draw with IDWriteTextRenderer1 available: %ld (addref %ld release %ld)", r->ref, r->addref, r->release);
    FULL_END();
}

/* ---- Draw* callback results are ignored -------------------------------------------------------------------- */
static void draw_cb_codes(int type)
{
    int i, base_total; 
    FULL_BEGIN();
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    base_total = r->nev;
    for (i = 0; i < NFAIL; i++) {
        rh_clear(r); r->ret_set[type] = 1; r->ret[type] = k_fail[i].hr;
        HRESULT hr = rh_draw(l, r, CTX_A, 0.0f, 0.0f);
        CHECKF(hr == S_OK, "%s from drawing callback %d: Draw returned %s", k_fail[i].name, type, dw_hr_name(hr));
        CHECKF(r->nev == base_total, "%s: %d callbacks, %d without failure", k_fail[i].name, r->nev, base_total);
        EXPECT_EQ(r->count[RH_UNDER], 1); EXPECT_EQ(r->count[RH_STRIKE], 1); EXPECT_EQ(r->count[RH_INLINE], 1);
    }
    FULL_END();
}
RT(snap_glyph_callback_failures_ignored) { draw_cb_codes(RH_GLYPH); }
RT(snap_inline_callback_failures_ignored) { draw_cb_codes(RH_INLINE); }
RT(snap_underline_callback_failures_ignored) { draw_cb_codes(RH_UNDER); }
RT(snap_strike_callback_failures_ignored) { draw_cb_codes(RH_STRIKE); }
RT(snap_only_second_glyph_run_fails_later_runs_delivered)
{
    R2_BEGIN(); R2_LAYOUT(l, L"AAAABBBBCCCC", 16.0f, 300.0f, 50.0f); rh *r = rh_new();
    IDWriteTextLayout_SetFontSize(l, 20.0f, RNG(4, 4)); IDWriteTextLayout_SetFontSize(l, 24.0f, RNG(8, 4));
    r->ret_set[RH_GLYPH] = 1; r->ret[RH_GLYPH] = E_FAIL; r->ret_nth[RH_GLYPH] = 2;
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_GLYPH], 3);
    EXPECT_EQ(rh_total_glyphs(r), 12);
    rh_free(r); IDWriteTextLayout_Release(l); R2_END();
}
RT(snap_all_drawing_callbacks_fail_snapping_still_once)
{
    FULL_BEGIN();
    int t;
    for (t = RH_GLYPH; t <= RH_INLINE; t++) { r->ret_set[t] = 1; r->ret[t] = E_ABORT; }
    RH_DRAW_OK(l, r, 0.0f, 0.0f);
    EXPECT_EQ(r->count[RH_SNAP], 1); EXPECT_EQ(r->count[RH_PPD], 1); EXPECT_EQ(r->count[RH_XFORM], 1);
    CHECK(r->count[RH_GLYPH] >= 2); EXPECT_EQ(r->count[RH_INLINE], 1); EXPECT_EQ(r->count[RH_UNDER], 1); EXPECT_EQ(r->count[RH_STRIKE], 1);
    FULL_END();
}

/* ---- re-entrancy ------------------------------------------------------------------------------------------- */
typedef struct { IDWriteTextLayout *inner; int fired; HRESULT hr; } nest_arg;
static void nest_hook(rh *r, rh_ev *e, void *arg)
{
    nest_arg *a = (nest_arg *)arg;
    if (e->type != RH_GLYPH || a->fired) return;
    a->fired = 1;
    a->hr = IDWriteTextLayout_Draw(a->inner, CTX_B, (IDWriteTextRenderer *)r, 1.0f, 2.0f);
}
RT(snap_nested_draw_of_other_layout_from_callback)
{
    R2_BEGIN(); R2_LAYOUT(outer, L"AB", 16.0f, 100.0f, 50.0f);
    IDWriteTextLayout *inner = r2_layout_w(&X, L"CDE", 16.0f, 100.0f, 50.0f, DWRITE_FONT_WEIGHT_NORMAL);
    if (!inner) { IDWriteTextLayout_Release(outer); R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new(); nest_arg a = { inner, 0, E_FAIL }; r->hook = nest_hook; r->hook_arg = &a;
    RH_DRAW_OK(outer, r, 0.0f, 0.0f);
    CHECK(a.fired); CHECK_HR(a.hr, S_OK);
    EXPECT_EQ(r->count[RH_SNAP], 2); EXPECT_EQ(r->count[RH_GLYPH], 2);
    EXPECT_EQ(r->max_depth, 2);
    /* the inner draw used its own client context and origin */
    { int i, n = 0; for (i = 0; i < r->nev; i++) if (r->ev[i].type == RH_GLYPH && r->ev[i].ctx == CTX_B) { n++; CHECK_NEARF(r->ev[i].x, 1.0f); } EXPECT_EQ(n, 1); }
    rh_free(r); IDWriteTextLayout_Release(inner); IDWriteTextLayout_Release(outer); R2_END();
}
RT(snap_nested_draw_failure_does_not_fail_outer)
{
    R2_BEGIN(); R2_LAYOUT(outer, L"AB", 16.0f, 100.0f, 50.0f);
    IDWriteTextLayout *inner = r2_layout_w(&X, L"CDE", 16.0f, 100.0f, 50.0f, DWRITE_FONT_WEIGHT_NORMAL);
    if (!inner) { IDWriteTextLayout_Release(outer); R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new(); nest_arg a = { inner, 0, S_OK }; r->hook = nest_hook; r->hook_arg = &a;
    r->ret_set[RH_SNAP] = 1; r->ret[RH_SNAP] = E_FAIL; r->ret_nth[RH_SNAP] = 2;      /* the inner Draw's snapping callback fails */
    RH_DRAW_OK(outer, r, 0.0f, 0.0f);
    CHECK_HR(a.hr, E_FAIL);
    EXPECT_EQ(r->count[RH_GLYPH], 1);                        /* inner delivered nothing, the outer run was delivered */
    rh_free(r); IDWriteTextLayout_Release(inner); IDWriteTextLayout_Release(outer); R2_END();
}
