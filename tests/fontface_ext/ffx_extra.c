/* Outline thresholds of GetRecommendedRenderingMode and QueryInterface failure behaviour.
 * The recommended-mode resolver (dwrite.dll!0x71520) switches to OUTLINE when ppem >= 100 for DWRITE_OUTLINE_THRESHOLD_ANTIALIASED (0)
 * and when ppem >= 350 for DWRITE_OUTLINE_THRESHOLD_ALIASED (1), with ppem = emSize * dpi / 96 when no rendering parameters override the mode. */
#include "ffx.h"

static DWRITE_RENDERING_MODE rm1_at(ffx *x, float em, float dpi, int th, int mm) {
    DWRITE_RENDERING_MODE m = (DWRITE_RENDERING_MODE)0xCCCC;
    HRESULT hr = ffx_c_rm1(x->f1, em, dpi, dpi, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)th, (DWRITE_MEASURING_MODE)mm, &m);
    t_check(hr == S_OK, __FILE__, __LINE__, "rm1", "em %g dpi %g -> %s", em, dpi, dw_hr_name(hr));
    return m;
}
TEST(rmx_antialiased_threshold_is_100_ppem) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    int mm;
    for (mm = 0; mm < 3; mm++) {
        t_check(rm1_at(&x, 99.0f, 96, 0, mm) != DWRITE_RENDERING_MODE_OUTLINE, __FILE__, __LINE__, "99 ppem", "mm %d outline below 100", mm);
        t_check(rm1_at(&x, 100.0f, 96, 0, mm) == DWRITE_RENDERING_MODE_OUTLINE, __FILE__, __LINE__, "100 ppem", "mm %d not outline at 100", mm);
        t_check(rm1_at(&x, 101.0f, 96, 0, mm) == DWRITE_RENDERING_MODE_OUTLINE, __FILE__, __LINE__, "101 ppem", "mm %d not outline at 101", mm);
    }
    ffx_close(&x);
}
TEST(rmx_threshold_uses_dpi_scaled_ppem) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    /* 50 * 192 / 96 = 100 ppem, 49 * 192 / 96 = 98 ppem */
    CHECK(rm1_at(&x, 50.0f, 192, 0, 2) == DWRITE_RENDERING_MODE_OUTLINE);
    CHECK(rm1_at(&x, 49.0f, 192, 0, 2) != DWRITE_RENDERING_MODE_OUTLINE);
    /* 175 * 192 / 96 = 350 ppem for the aliased threshold */
    CHECK(rm1_at(&x, 175.0f, 192, 1, 2) == DWRITE_RENDERING_MODE_OUTLINE);
    CHECK(rm1_at(&x, 174.0f, 192, 1, 2) != DWRITE_RENDERING_MODE_OUTLINE);
    ffx_close(&x);
}
TEST(rmx_rm3_default_params_matches_thresholds) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    static const struct { float em; int th; int outline; } c[] = { { 99, 0, 0 }, { 100, 0, 1 }, { 349, 1, 0 }, { 350, 1, 1 }, { 100, 1, 0 }, { 400, 0, 1 } };
    int i;
    for (i = 0; i < (int)(sizeof c / sizeof c[0]); i++) {
        DWRITE_RENDERING_MODE1 m = (DWRITE_RENDERING_MODE1)0xCCCC; DWRITE_GRID_FIT_MODE g = (DWRITE_GRID_FIT_MODE)0xCCCC;
        HRESULT hr = ffx_c_rm3(x.f3, c[i].em, 96, 96, NULL, FALSE, (DWRITE_OUTLINE_THRESHOLD)c[i].th, DWRITE_MEASURING_MODE_NATURAL, NULL, &m, &g);
        t_check(hr == S_OK, __FILE__, __LINE__, "rm3", "em %g th %d -> %s", c[i].em, c[i].th, dw_hr_name(hr));
        t_check((m == DWRITE_RENDERING_MODE1_OUTLINE) == c[i].outline, __FILE__, __LINE__, "rm3 outline", "em %g th %d mode %d", c[i].em, c[i].th, (int)m);
    }
    ffx_close(&x);
}
TEST(rmx_qi_failure_keeps_refcount) {
    ffx x; if (!ffx_open_default(&x)) { ffx_close(&x); return; }
    /* a failed QueryInterface goes through the `*ppv = NULL` branch and never touches the reference count */
    ULONG before = dw_refcount((IUnknown *)x.f0), after; void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = IDWriteFontFace_QueryInterface(x.f0, &IID_IDWriteFactory, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    after = dw_refcount((IUnknown *)x.f0);
    EXPECT_EQ(after, before);
    ffx_close(&x);
}
