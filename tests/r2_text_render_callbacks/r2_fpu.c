/* Floating point state around IDWriteTextLayout::Draw.
 *
 * IDWriteTextLayout::Draw constructs an FPUStateSSE: when (MXCSR & 0xff80) != 0x1f80 it loads 0x1f80 (exceptions masked, round to nearest,
 * no flush-to-zero) and restores the caller's control bits when it returns.  Draw and the DrawGlyphRuns / DrawInlineObjects /
 * DrawUnderlines / DrawStrikethroughs helpers wrap every client callback in an FPUStateSandboxSSE which puts back the MXCSR value that was
 * current before the callback if the callback changed it.  Hence: a renderer always runs with the default FPU mode, whatever the caller
 * used, and neither the caller's mode nor a callback's own change leaks into later callbacks.
 */
#include "r2.h"

#define CSR_NEAREST   0x1f80u
#define CSR_DOWN      0x3f80u
#define CSR_UP        0x5f80u
#define CSR_TRUNCATE  0x7f80u
#define CSR_FTZ       0x9f80u

/* "AB#CD": underline over AB, strikethrough over CD, an inline object over '#': every callback kind occurs. */
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
static void draw_with_csr(IDWriteTextLayout *l, rh *r, unsigned csr, HRESULT *hr, unsigned *after)
{
    unsigned saved = r2_getcsr();
    r2_setcsr(csr);
    *hr = rh_draw(l, r, CTX_A, 3.3f, 4.7f);
    *after = r2_getcsr();
    r2_setcsr(saved);
}
static void all_events_default(const rh *r)
{
    int i;
    for (i = 0; i < r->nev; i++) t_check((r->ev[i].mxcsr & R2_CSR_MASK) == R2_CSR_DEFAULT, __FILE__, __LINE__, "callback runs with default MXCSR", "event %d type %d MXCSR 0x%x", i, r->ev[i].type, r->ev[i].mxcsr);
}

static void caller_mode_seen_as_default(unsigned csr)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new();
    draw_with_csr(l, r, csr, &hr, &after);
    CHECK_HR_OK(hr);
    CHECK(r->count[RH_GLYPH] >= 2); CHECK(r->count[RH_INLINE] == 1); CHECK(r->count[RH_UNDER] == 1); CHECK(r->count[RH_STRIKE] == 1);
    CHECK(r->count[RH_SNAP] == 1);
    all_events_default(r);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}
static void caller_mode_restored(unsigned csr)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new();
    draw_with_csr(l, r, csr, &hr, &after);
    CHECK_HR_OK(hr);
    CHECKF((after & R2_CSR_MASK) == (csr & R2_CSR_MASK), "MXCSR after Draw 0x%x, caller had 0x%x", after, csr);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}
RT(fpu_nearest_caller_sees_default) { caller_mode_seen_as_default(CSR_NEAREST); }
RT(fpu_round_down_caller_callbacks_see_default) { caller_mode_seen_as_default(CSR_DOWN); }
RT(fpu_round_up_caller_callbacks_see_default) { caller_mode_seen_as_default(CSR_UP); }
RT(fpu_truncate_caller_callbacks_see_default) { caller_mode_seen_as_default(CSR_TRUNCATE); }
RT(fpu_flush_to_zero_caller_callbacks_see_default) { caller_mode_seen_as_default(CSR_FTZ); }
RT(fpu_nearest_caller_restored) { caller_mode_restored(CSR_NEAREST); }
RT(fpu_round_down_caller_restored) { caller_mode_restored(CSR_DOWN); }
RT(fpu_round_up_caller_restored) { caller_mode_restored(CSR_UP); }
RT(fpu_truncate_caller_restored) { caller_mode_restored(CSR_TRUNCATE); }
RT(fpu_flush_to_zero_caller_restored) { caller_mode_restored(CSR_FTZ); }

/* A callback that switches the FPU mode is undone before the next callback runs and before Draw returns. */
static void callback_change_contained(int type, unsigned newcsr)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new();
    r->set_csr[type] = 1; r->csr_val[type] = newcsr;
    draw_with_csr(l, r, CSR_NEAREST, &hr, &after);
    CHECK_HR_OK(hr);
    CHECK(r->count[type] >= 1);
    all_events_default(r);
    CHECKF((after & R2_CSR_MASK) == CSR_NEAREST, "MXCSR after Draw 0x%x", after);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}
RT(fpu_snap_callback_change_contained) { callback_change_contained(RH_SNAP, CSR_TRUNCATE); }
RT(fpu_transform_callback_change_contained) { callback_change_contained(RH_XFORM, CSR_TRUNCATE); }
RT(fpu_ppd_callback_change_contained) { callback_change_contained(RH_PPD, CSR_DOWN); }
RT(fpu_glyph_callback_change_contained) { callback_change_contained(RH_GLYPH, CSR_TRUNCATE); }
RT(fpu_underline_callback_change_contained) { callback_change_contained(RH_UNDER, CSR_UP); }
RT(fpu_strike_callback_change_contained) { callback_change_contained(RH_STRIKE, CSR_DOWN); }
RT(fpu_inline_callback_change_contained) { callback_change_contained(RH_INLINE, CSR_TRUNCATE); }
RT(fpu_glyph_callback_sets_flush_to_zero) { callback_change_contained(RH_GLYPH, CSR_FTZ); }
RT(fpu_callback_change_contained_when_caller_truncates)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new();
    r->set_csr[RH_GLYPH] = 1; r->csr_val[RH_GLYPH] = CSR_UP;
    draw_with_csr(l, r, CSR_TRUNCATE, &hr, &after);
    CHECK_HR_OK(hr);
    all_events_default(r);
    CHECKF((after & R2_CSR_MASK) == CSR_TRUNCATE, "MXCSR after Draw 0x%x, caller had truncation", after);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}

/* The drawing coordinates do not depend on the caller's rounding mode. */
static void results_independent(unsigned csr)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after; int i;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *a = rh_new(), *b = rh_new();
    a->ppd_val = 1.5f; b->ppd_val = 1.5f;
    draw_with_csr(l, a, CSR_NEAREST, &hr, &after); CHECK_HR_OK(hr);
    draw_with_csr(l, b, csr, &hr, &after); CHECK_HR_OK(hr);
    EXPECT_EQ(a->nev, b->nev);
    for (i = 0; i < a->nev && i < b->nev; i++) {
        if (a->ev[i].type != b->ev[i].type) { t_check(0, __FILE__, __LINE__, "same callback order", "event %d differs", i); break; }
        if (a->ev[i].type == RH_QI || a->ev[i].type == RH_SNAP || a->ev[i].type == RH_XFORM || a->ev[i].type == RH_PPD) continue;
        CHECK_FBITS(b->ev[i].x, a->ev[i].x); CHECK_FBITS(b->ev[i].y, a->ev[i].y);
        if (a->ev[i].type == RH_GLYPH) { UINT32 k; for (k = 0; k < a->ev[i].n && k < RH_MAXG; k++) CHECK_FBITS(b->ev[i].adv[k], a->ev[i].adv[k]); }
        if (a->ev[i].type == RH_UNDER) { CHECK_FBITS(b->ev[i].ul.width, a->ev[i].ul.width); CHECK_FBITS(b->ev[i].ul.offset, a->ev[i].ul.offset); CHECK_FBITS(b->ev[i].ul.thickness, a->ev[i].ul.thickness); }
    }
    rh_free(a); rh_free(b); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}
RT(fpu_results_identical_with_round_down) { results_independent(CSR_DOWN); }
RT(fpu_results_identical_with_round_up) { results_independent(CSR_UP); }
RT(fpu_results_identical_with_truncation) { results_independent(CSR_TRUNCATE); }
RT(fpu_results_identical_with_flush_to_zero) { results_independent(CSR_FTZ); }

/* An aborted Draw (failing snapping callback) still restores the caller's mode (the FPU state object is destroyed during unwinding). */
static void aborted_restores(int type)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new();
    r->ret_set[type] = 1; r->ret[type] = E_FAIL;
    draw_with_csr(l, r, CSR_TRUNCATE, &hr, &after);
    CHECK_HR(hr, E_FAIL);
    EXPECT_EQ(r->count[RH_GLYPH], 0);
    all_events_default(r);
    CHECKF((after & R2_CSR_MASK) == CSR_TRUNCATE, "MXCSR after failed Draw 0x%x", after);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}
RT(fpu_aborted_by_snap_failure_restores) { aborted_restores(RH_SNAP); }
RT(fpu_aborted_by_ppd_failure_restores) { aborted_restores(RH_PPD); }
RT(fpu_aborted_by_transform_failure_restores) { aborted_restores(RH_XFORM); }

RT(fpu_null_renderer_leaves_mode_alone)
{
    if (t_is_wine()) { t_skip("Wine dereferences a NULL renderer"); return; }
    R2_BEGIN(); R2_LAYOUT(l, L"AB", 16.0f, 100.0f, 50.0f);
    unsigned saved = r2_getcsr(), after; HRESULT hr;
    r2_setcsr(CSR_TRUNCATE);
    hr = IDWriteTextLayout_Draw(l, CTX_A, NULL, 0.0f, 0.0f);
    after = r2_getcsr(); r2_setcsr(saved);
    CHECK_HR(hr, E_INVALIDARG);
    EXPECT_EQ(after & R2_CSR_MASK, CSR_TRUNCATE);
    IDWriteTextLayout_Release(l); R2_END();
}
RT(fpu_snapping_disabled_path_default_mode)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new(); r->snap_val = TRUE;
    draw_with_csr(l, r, CSR_UP, &hr, &after);
    CHECK_HR_OK(hr);
    EXPECT_EQ(r->count[RH_PPD], 0);
    all_events_default(r);
    CHECKF((after & R2_CSR_MASK) == CSR_UP, "MXCSR after Draw 0x%x", after);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}
RT(fpu_renderer1_callbacks_default_mode)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new(); r->allow_r1 = 1;
    draw_with_csr(l, r, CSR_TRUNCATE, &hr, &after);
    CHECK_HR_OK(hr);
    CHECK(r->count[RH_GLYPH] >= 1);
    { const rh_ev *g = rh_nth(r, RH_GLYPH, 0); if (g) CHECK(g->r1); }
    all_events_default(r);
    CHECKF((after & R2_CSR_MASK) == CSR_TRUNCATE, "MXCSR after Draw 0x%x", after);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}
RT(fpu_repeated_draws_stay_balanced)
{
    R2_BEGIN(); dwx_inline *io; IDWriteTextLayout *l = mk_full(&X, &io); HRESULT hr; unsigned after; int i;
    if (!l) { R2_END(); t_skip("layout prerequisite failed"); return; }
    rh *r = rh_new();
    for (i = 0; i < 6; i++) {
        unsigned csr = (i & 1) ? CSR_TRUNCATE : CSR_NEAREST;
        draw_with_csr(l, r, csr, &hr, &after);
        CHECK_HR_OK(hr);
        CHECK((after & R2_CSR_MASK) == csr);
    }
    all_events_default(r);
    rh_free(r); IDWriteTextLayout_Release(l); dwx_release(io); R2_END();
}

/* ---- the trimming sign ------------------------------------------------------------------------------------------ */
RT(fpu_sign_creation_restores_caller_mode)
{
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_NORMAL); IDWriteInlineObject *s = NULL; unsigned saved = r2_getcsr(), after; HRESULT hr;
    if (!t) { R2_END(); t_skip("format unavailable"); return; }
    r2_setcsr(CSR_TRUNCATE);
    hr = IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s);
    after = r2_getcsr(); r2_setcsr(saved);
    CHECK_HR_OK(hr);
    CHECKF((after & R2_CSR_MASK) == CSR_TRUNCATE, "MXCSR after CreateEllipsisTrimmingSign 0x%x", after);
    if (s) {
        DWRITE_INLINE_OBJECT_METRICS a, b;
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &a));
        CHECK_NEARF(a.width, R2_ADV(1000, 16));                /* built in the default FPU mode, not in truncation */
        r2_setcsr(CSR_TRUNCATE); hr = IDWriteInlineObject_GetMetrics(s, &b); after = r2_getcsr(); r2_setcsr(saved);
        CHECK_HR_OK(hr); CHECK(!memcmp(&a, &b, sizeof a));
        IDWriteInlineObject_Release(s);
    }
    IDWriteTextFormat_Release(t); R2_END();
}
RT(fpu_sign_draw_callbacks_see_default_mode)
{
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_NORMAL); IDWriteInlineObject *s = NULL; rh *r = rh_new();
    unsigned saved, after; HRESULT hr;
    if (!t || FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)) || !s) { DW_RELEASE(t); rh_free(r); R2_END(); t_skip("sign unavailable"); return; }
    saved = r2_getcsr();
    r2_setcsr(CSR_UP);
    hr = IDWriteInlineObject_Draw(s, CTX_A, (IDWriteTextRenderer *)r, 3.3f, 4.7f, FALSE, FALSE, NULL);
    after = r2_getcsr(); r2_setcsr(saved);
    CHECK_HR_OK(hr);
    CHECK(r->count[RH_GLYPH] == 1);
    all_events_default(r);
    CHECKF((after & R2_CSR_MASK) == CSR_UP, "MXCSR after sign Draw 0x%x", after);
    IDWriteInlineObject_Release(s); IDWriteTextFormat_Release(t); rh_free(r); R2_END();
}
RT(fpu_sign_draw_callback_change_contained)
{
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_NORMAL); IDWriteInlineObject *s = NULL; rh *r = rh_new();
    unsigned saved, after; HRESULT hr;
    if (!t || FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)) || !s) { DW_RELEASE(t); rh_free(r); R2_END(); t_skip("sign unavailable"); return; }
    r->set_csr[RH_GLYPH] = 1; r->csr_val[RH_GLYPH] = CSR_TRUNCATE;
    saved = r2_getcsr();
    hr = IDWriteInlineObject_Draw(s, CTX_A, (IDWriteTextRenderer *)r, 0.0f, 0.0f, FALSE, FALSE, NULL);
    after = r2_getcsr(); r2_setcsr(saved);
    CHECK_HR_OK(hr);
    CHECKF((after & R2_CSR_MASK) == (saved & R2_CSR_MASK), "MXCSR after sign Draw 0x%x, before 0x%x", after, saved);
    IDWriteInlineObject_Release(s); IDWriteTextFormat_Release(t); rh_free(r); R2_END();
}
RT(fpu_trimmed_layout_sign_recursion_default_mode)
{
    /* the application's DrawInlineObject calls the sign back: a nested Draw, again with the default mode in its callbacks */
    R2_BEGIN(); IDWriteTextFormat *t = r2_format(&X, 16.0f, DWRITE_FONT_WEIGHT_NORMAL); IDWriteTextLayout *l = NULL; IDWriteInlineObject *s = NULL; rh *r = rh_new();
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; unsigned saved, after; HRESULT hr;
    if (!t || FAILED(IDWriteFactory_CreateTextLayout(f, L"ABCDEFGHIJKLMNOP", 16, t, 60.0f, 100.0f, &l)) ||
        FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)) || !s ||
        FAILED(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP)) || FAILED(IDWriteTextLayout_SetTrimming(l, &tr, s))) {
        DW_RELEASE(s); DW_RELEASE(l); DW_RELEASE(t); rh_free(r); R2_END(); t_skip("trimming setup failed"); return;
    }
    saved = r2_getcsr(); r2_setcsr(CSR_TRUNCATE);
    hr = rh_draw(l, r, CTX_A, 0.0f, 0.0f);
    after = r2_getcsr(); r2_setcsr(saved);
    CHECK_HR_OK(hr);
    EXPECT_EQ(r->count[RH_INLINE], 1);
    all_events_default(r);
    CHECKF((after & R2_CSR_MASK) == CSR_TRUNCATE, "MXCSR after Draw 0x%x", after);
    DW_RELEASE(s); IDWriteTextLayout_Release(l); IDWriteTextFormat_Release(t); rh_free(r); R2_END();
}
