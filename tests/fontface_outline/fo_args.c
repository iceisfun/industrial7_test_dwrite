/* IDWriteFontFace::GetGlyphRunOutline: argument validation, call protocol, object lifetime.
 * Windows: DWriteFontFace::GetGlyphRunOutline returns E_INVALIDARG (0x80070057) when (indices == NULL && count != 0),
 * sink == NULL, or !(emSize > 0) (NaN included); otherwise it returns S_OK after streaming the outline.  With count == 0
 * nothing at all is sent to the sink (no SetFillMode, no Close). */
#include "fo.h"
#include <xmmintrin.h>

static const UINT16 G_A = 2, G_SPACE = 1;

/* ---- invalid arguments ------------------------------------------------------------------ */
TEST(arg_null_sink) {
    FO_NEED_FACE();
    UINT16 g = G_A;
    CHECK_HR(IDWriteFontFace_GetGlyphRunOutline(h.face, 100.0f, &g, NULL, NULL, 1, FALSE, FALSE, NULL), E_INVALIDARG);
    dwx_font_close(&h);
}
TEST(arg_null_sink_count_zero) {
    FO_NEED_FACE();
    UINT16 g = G_A;
    /* the sink test is part of the same condition as the others: it applies even for an empty run */
    CHECK_HR(IDWriteFontFace_GetGlyphRunOutline(h.face, 100.0f, &g, NULL, NULL, 0, FALSE, FALSE, NULL), E_INVALIDARG);
    CHECK_HR(IDWriteFontFace_GetGlyphRunOutline(h.face, 100.0f, NULL, NULL, NULL, 0, FALSE, FALSE, NULL), E_INVALIDARG);
    dwx_font_close(&h);
}
TEST(arg_null_indices_count_nonzero) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new();
    DWRITE_GLYPH_OFFSET off[2] = { { 1, 1 }, { 2, 2 } }; float adv[2] = { 10, 10 };
    CHECK_HR(fo_call(h.face, 100.0f, NULL, NULL, NULL, 1, FALSE, FALSE, s), E_INVALIDARG);
    CHECK_HR(fo_call(h.face, 100.0f, NULL, adv, off, 2, FALSE, FALSE, s), E_INVALIDARG);
    CHECK_HR(fo_call(h.face, 100.0f, NULL, NULL, NULL, 0xFFFFFFFFu, FALSE, TRUE, s), E_INVALIDARG);
    CHECK(s->nev == 0);       /* validation happens before any sink call */
    EXPECT_REFS(s, 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(arg_null_indices_count_zero_ok) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new();
    CHECK_HR(fo_call(h.face, 100.0f, NULL, NULL, NULL, 0, FALSE, FALSE, s), S_OK);
    CHECK_HR(fo_call(h.face, 100.0f, NULL, NULL, NULL, 0, TRUE, TRUE, s), S_OK);
    CHECK(s->nev == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(arg_count_zero_sends_nothing) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new();
    UINT16 g[2] = { G_A, G_A };
    CHECK_HR(fo_call(h.face, 100.0f, g, NULL, NULL, 0, FALSE, FALSE, s), S_OK);
    /* ComputeGlyphRunOutline returns immediately for an empty run: not even SetFillMode */
    CHECK(s->nev == 0);
    CHECK(s->fill_calls == 0);
    CHECK(s->close_calls == 0);
    CHECK(s->fill_mode == -1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(arg_emsize_invalid_table) {
    FO_NEED_FACE();
    static const float ems[] = { 0.0f, -0.0f, -1.0f, -100.0f, -1e-30f, -3.4e38f };
    UINT16 g = G_A; size_t i;
    for (i = 0; i < sizeof ems / sizeof ems[0]; i++) {
        dwx_geom *s = dwx_geom_new();
        HRESULT hr = fo_call(h.face, ems[i], &g, NULL, NULL, 1, FALSE, FALSE, s);
        CHECKF(hr == E_INVALIDARG, "em=%g -> %s", ems[i], dw_hr_name(hr));
        CHECKF(s->nev == 0, "em=%g sink touched (%d events)", ems[i], s->nev);
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(arg_emsize_nan) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g = G_A; union { UINT32 u; float f; } n;
    n.u = 0x7FC00000;   /* quiet NaN: the comparison !(em > 0) rejects it */
    CHECK_HR(fo_call(h.face, n.f, &g, NULL, NULL, 1, FALSE, FALSE, s), E_INVALIDARG);
    n.u = 0xFFC00000;
    CHECK_HR(fo_call(h.face, n.f, &g, NULL, NULL, 1, FALSE, FALSE, s), E_INVALIDARG);
    n.u = 0x7F800001;   /* signalling NaN */
    CHECK_HR(fo_call(h.face, n.f, &g, NULL, NULL, 1, FALSE, FALSE, s), E_INVALIDARG);
    CHECK(s->nev == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(arg_emsize_invalid_even_with_empty_run) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g = G_A;
    CHECK_HR(fo_call(h.face, 0.0f, &g, NULL, NULL, 0, FALSE, FALSE, s), E_INVALIDARG);
    CHECK_HR(fo_call(h.face, -5.0f, NULL, NULL, NULL, 0, FALSE, FALSE, s), E_INVALIDARG);
    dwx_release(s); dwx_font_close(&h);
}
TEST(arg_emsize_tiny_positive_ok) {
    FO_NEED_FACE();
    static const float ems[] = { 1e-6f, 0.001f, 0.5f, 1.0f };
    UINT16 g = G_A; size_t i;
    for (i = 0; i < sizeof ems / sizeof ems[0]; i++) {
        dwx_geom *s = dwx_geom_new(); fo_exp e;
        CHECK_HR(fo_call(h.face, ems[i], &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
        fo_model_run(&e, ttf_default_glyphs(&(int){0}), TTF_DEF_NGLYPHS, h.face, ems[i], &g, NULL, NULL, 1, 0, 0, 0);
        FO_COMPARE(s, &e, 1e-5, "tiny em");
        FO_INVARIANTS(s, 1);
        dwx_release(s);
    }
    dwx_font_close(&h);
}
TEST(arg_emsize_huge_ok) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g = G_A; fo_exp e; int n;
    CHECK_HR(fo_call(h.face, 20000.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    fo_model_run(&e, ttf_default_glyphs(&n), TTF_DEF_NGLYPHS, h.face, 20000.0, &g, NULL, NULL, 1, 0, 0, 0);
    FO_COMPARE(s, &e, 0.01, "em 20000");
    dwx_release(s); dwx_font_close(&h);
}
TEST(arg_flags_any_nonzero_is_true) {
    FO_NEED_FACE();
    /* isSideways / isRightToLeft are BOOL: any non-zero value behaves as TRUE (param_6 != 0, param_7 != 0) */
    UINT16 g[2] = { G_A, G_A }; dwx_geom *a = dwx_geom_new(), *b = dwx_geom_new(), *c = dwx_geom_new(), *d = dwx_geom_new();
    int i;
    CHECK_HR(fo_call(h.face, 200.0f, g, NULL, NULL, 2, FALSE, TRUE, a), S_OK);
    CHECK_HR(fo_call(h.face, 200.0f, g, NULL, NULL, 2, FALSE, (BOOL)7, b), S_OK);
    CHECK_HR(fo_call(h.face, 200.0f, g, NULL, NULL, 2, TRUE, FALSE, c), S_OK);
    CHECK_HR(fo_call(h.face, 200.0f, g, NULL, NULL, 2, (BOOL)-1, FALSE, d), S_OK);
    CHECK(a->npts == b->npts && a->npts > 0);
    for (i = 0; i < a->npts && i < b->npts; i++) CHECK(a->pts[i].x == b->pts[i].x && a->pts[i].y == b->pts[i].y);
    CHECK(c->npts == d->npts && c->npts > 0);
    for (i = 0; i < c->npts && i < d->npts; i++) CHECK(c->pts[i].x == d->pts[i].x && c->pts[i].y == d->pts[i].y);
    dwx_release(a); dwx_release(b); dwx_release(c); dwx_release(d); dwx_font_close(&h);
}
TEST(arg_failure_does_not_modify_inputs) {
    FO_NEED_FACE();
    UINT16 g[3] = { 2, 3, 4 }; float adv[3] = { 5, 6, 7 }; DWRITE_GLYPH_OFFSET off[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
    UINT16 g0[3]; float adv0[3]; DWRITE_GLYPH_OFFSET off0[3]; dwx_geom *s = dwx_geom_new();
    memcpy(g0, g, sizeof g); memcpy(adv0, adv, sizeof adv); memcpy(off0, off, sizeof off);
    CHECK_HR(fo_call(h.face, -1.0f, g, adv, off, 3, FALSE, FALSE, s), E_INVALIDARG);
    CHECK(!memcmp(g, g0, sizeof g) && !memcmp(adv, adv0, sizeof adv) && !memcmp(off, off0, sizeof off));
    CHECK_HR(fo_call(h.face, 50.0f, g, adv, off, 3, TRUE, TRUE, s), S_OK);
    CHECK(!memcmp(g, g0, sizeof g) && !memcmp(adv, adv0, sizeof adv) && !memcmp(off, off0, sizeof off));
    dwx_release(s); dwx_font_close(&h);
}

/* ---- call protocol ------------------------------------------------------------------------ */
TEST(proto_first_call_is_setfillmode_winding) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g = G_A;
    CHECK_HR(fo_call(h.face, 64.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    CHECK(s->nev >= 1);
    if (s->nev >= 1) { CHECK(s->ev[0].kind == DWX_G_FILL); CHECK(s->ev[0].arg == D2D1_FILL_MODE_WINDING); }
    CHECK(s->fill_calls == 1);
    CHECK(s->fill_mode == D2D1_FILL_MODE_WINDING);   /* never ALTERNATE, even for the two-contour ring glyph below */
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_fill_mode_winding_for_holes) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g = TTF_GID_RING;
    CHECK_HR(fo_call(h.face, 64.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    CHECK(s->fill_mode == D2D1_FILL_MODE_WINDING);
    CHECK(s->fill_calls == 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_single_fill_for_whole_run) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g[6] = { 2, 3, 64, 65, 68, 66 };
    CHECK_HR(fo_call(h.face, 64.0f, g, NULL, NULL, 6, FALSE, FALSE, s), S_OK);
    CHECK(s->fill_calls == 1);
    CHECK(s->ev[0].kind == DWX_G_FILL);
    FO_INVARIANTS(s, 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_fill_per_call_not_per_sink) {
    FO_NEED_FACE();
    /* a sink reused for two calls sees SetFillMode twice, one at the start of each call; no Close in between */
    dwx_geom *s = dwx_geom_new(); UINT16 g = G_A;
    CHECK_HR(fo_call(h.face, 64.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    int n1 = s->nev;
    CHECK_HR(fo_call(h.face, 64.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    CHECK(s->fill_calls == 2);
    CHECK(s->nev == 2 * n1);
    CHECK(s->ev[n1].kind == DWX_G_FILL);
    CHECK(s->close_calls == 0);
    CHECK(s->order_violations == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_close_never_called) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g[3] = { 2, 64, 65 };
    s->fail_close = E_FAIL;     /* would be visible if dwrite called Close and propagated the result */
    CHECK_HR(fo_call(h.face, 64.0f, g, NULL, NULL, 3, FALSE, FALSE, s), S_OK);
    CHECK(s->close_calls == 0);
    /* the caller owns Close(): it is still callable afterwards */
    CHECK_HR(ID2D1SimplifiedGeometrySink_Close((ID2D1SimplifiedGeometrySink *)s), E_FAIL);
    CHECK(s->close_calls == 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_no_segment_flags) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g[4] = { 2, 64, 65, 68 };
    CHECK_HR(fo_call(h.face, 64.0f, g, NULL, NULL, 4, FALSE, FALSE, s), S_OK);
    int i, n = 0; for (i = 0; i < s->nev; i++) if (s->ev[i].kind == DWX_G_SEGFLAGS) n++;
    CHECK(n == 0);
    FO_INVARIANTS(s, 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_sink_refcount_untouched) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g[3] = { 2, 64, 65 };
    CHECK_HR(fo_call(h.face, 64.0f, g, NULL, NULL, 3, FALSE, FALSE, s), S_OK);
    EXPECT_REFS(s, 1);                      /* not retained after the call returns */
    CHECK(dwx_qi_calls(s) == 0);            /* the sink pointer is used as is, never queried */
    CHECK(!dwx_destroyed(s));
    dwx_release(s);
    CHECK(dwx_destroyed(s));
    dwx_font_close(&h);
}
TEST(proto_no_sink_use_after_return) {
    FO_NEED_FACE();
    /* nothing is deferred: the number of events does not grow after the call has returned */
    dwx_geom *s = dwx_geom_new(); UINT16 g = 64; int n;
    CHECK_HR(fo_call(h.face, 64.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    n = s->nev; Sleep(30);
    CHECK(s->nev == n);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_events_order_per_figure) {
    FO_NEED_FACE();
    /* ring: two figures, each Begin, 3 lines (4 points, closing edge implicit), End */
    dwx_geom *s = dwx_geom_new(); UINT16 g = TTF_GID_RING; int i, seq[16], k = 0;
    CHECK_HR(fo_call(h.face, 2048.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    for (i = 0; i < s->nev && k < 16; i++) seq[k++] = s->ev[i].kind;
    int want[] = { DWX_G_FILL, DWX_G_BEGIN, DWX_G_LINES, DWX_G_LINES, DWX_G_LINES, DWX_G_END, DWX_G_BEGIN, DWX_G_LINES, DWX_G_LINES, DWX_G_LINES, DWX_G_END };
    CHECK(s->nev == 11);
    for (i = 0; i < 11 && i < k; i++) CHECKF(seq[i] == want[i], "event %d kind %d want %d", i, seq[i], want[i]);
    FO_INVARIANTS(s, 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_empty_glyph_only_fill) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g = G_SPACE;
    CHECK_HR(fo_call(h.face, 64.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    CHECK(s->nev == 1);
    CHECK(s->fill_calls == 1);
    CHECK(s->nfigures_begun == 0 && s->npts == 0);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_empty_glyphs_run_only_fill) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g[5] = { G_SPACE, TTF_GID_EMPTY500, G_SPACE, TTF_GID_EMPTY500, G_SPACE };
    CHECK_HR(fo_call(h.face, 64.0f, g, NULL, NULL, 5, FALSE, TRUE, s), S_OK);
    CHECK(s->nev == 1 && s->fill_calls == 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_empty_glyph_between_outlines) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(), *r = dwx_geom_new(); UINT16 g[3] = { 2, G_SPACE, 2 }, ref[2] = { 2, 2 }; int n;
    float adv3[3] = { 100, 50, 100 }, adv2[2] = { 150, 100 };
    CHECK_HR(fo_call(h.face, 2048.0f, g, adv3, NULL, 3, FALSE, FALSE, s), S_OK);
    CHECK_HR(fo_call(h.face, 2048.0f, ref, adv2, NULL, 2, FALSE, FALSE, r), S_OK);
    CHECK(s->nev == r->nev);
    CHECK(s->nfigures_begun == 2);
    for (n = 0; n < s->npts && n < r->npts; n++) CHECK(s->pts[n].x == r->pts[n].x && s->pts[n].y == r->pts[n].y);
    dwx_release(s); dwx_release(r); dwx_font_close(&h);
}
TEST(proto_long_run_200_glyphs) {
    FO_NEED_FACE();
    dwx_geom *s = dwx_geom_new(); UINT16 g[200]; int i;
    for (i = 0; i < 200; i++) g[i] = 2 + (i % 26);
    CHECK_HR(fo_call(h.face, 16.0f, g, NULL, NULL, 200, FALSE, FALSE, s), S_OK);
    CHECK(s->nfigures_begun == 200);
    CHECK(s->nfigures_ended == 200);
    CHECK(s->nlines == 600);      /* three AddLines per box */
    CHECK(s->nbeziers == 0);
    CHECK(s->fill_calls == 1);
    FO_INVARIANTS(s, 1);
    dwx_release(s); dwx_font_close(&h);
}
TEST(proto_notdef_outline) {
    FO_NEED_FACE();
    /* glyph 0 is a regular glyph: .notdef box (100,0)-(1100,1400) */
    dwx_geom *s = dwx_geom_new(); UINT16 g = 0; fo_exp e; int n;
    CHECK_HR(fo_call(h.face, 2048.0f, &g, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    fo_model_run(&e, ttf_default_glyphs(&n), TTF_DEF_NGLYPHS, h.face, 2048, &g, NULL, NULL, 1, 0, 0, 0);
    FO_COMPARE(s, &e, 0, "notdef");
    CHECK(s->nfigures_begun == 1);
    float x0, y0, x1, y1; fo_bounds(s, &x0, &y0, &x1, &y1);
    CHECK_FLOAT_EQ(x0, 100); CHECK_FLOAT_EQ(x1, 1100); CHECK_FLOAT_EQ(y0, -1400); CHECK_FLOAT_EQ(y1, 0);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- determinism / reuse -------------------------------------------------------------------- */
TEST(reuse_repeat_identical) {
    FO_LAB(h, DWRITE_FONT_SIMULATIONS_NONE);
    UINT16 g[6] = { FO_G_OFFSTART, FO_G_ALLOFF4, TTF_GID_ROUND, TTF_GID_RING, FO_G_MULTI, FO_G_COMP_SCALE };
    dwx_geom *a = dwx_geom_new(), *b = dwx_geom_new(), *c = dwx_geom_new();
    CHECK_HR(fo_call(h.face, 37.5f, g, NULL, NULL, 6, FALSE, FALSE, a), S_OK);
    CHECK_HR(fo_call(h.face, 37.5f, g, NULL, NULL, 6, FALSE, FALSE, b), S_OK);
    CHECK_HR(fo_call(h.face, 37.5f, g, NULL, NULL, 6, FALSE, FALSE, c), S_OK);
    CHECK(a->nev == b->nev && b->nev == c->nev && a->nev > 20);
    CHECK(a->npts == b->npts && b->npts == c->npts);
    CHECK(!memcmp(a->pts, b->pts, sizeof(D2D1_POINT_2F) * a->npts));
    CHECK(!memcmp(b->pts, c->pts, sizeof(D2D1_POINT_2F) * b->npts));
    dwx_release(a); dwx_release(b); dwx_release(c); dwx_font_close(&h);
}
TEST(reuse_sizes_interleaved) {
    FO_LAB(h, DWRITE_FONT_SIMULATIONS_NONE);
    /* cached glyph data must not leak between sizes */
    UINT16 g[2] = { FO_G_OFFSTART, TTF_GID_ROUND }; static const float ems[] = { 10, 2048, 33.3f, 2048, 10, 500 };
    dwx_geom *first[6]; int i;
    for (i = 0; i < 6; i++) { first[i] = dwx_geom_new(); CHECK_HR(fo_call(h.face, ems[i], g, NULL, NULL, 2, FALSE, FALSE, first[i]), S_OK); }
    CHECK(first[0]->npts == first[4]->npts && !memcmp(first[0]->pts, first[4]->pts, sizeof(D2D1_POINT_2F) * first[0]->npts));
    CHECK(first[1]->npts == first[3]->npts && !memcmp(first[1]->pts, first[3]->pts, sizeof(D2D1_POINT_2F) * first[1]->npts));
    CHECK(memcmp(first[0]->pts, first[1]->pts, sizeof(D2D1_POINT_2F) * 4) != 0);
    for (i = 0; i < 6; i++) dwx_release(first[i]);
    dwx_font_close(&h);
}
TEST(reuse_two_faces_same_blob_equal) {
    if (!dw_shared()) { t_skip("no factory"); return; }
    dwx_font a, b; UINT16 g[2] = { 2, 65 };
    if (FAILED(fo_open_lab(&a, 0)) || FAILED(fo_open_lab(&b, 0))) { dwx_font_close(&a); dwx_font_close(&b); t_skip("lab font unavailable"); return; }
    dwx_geom *sa = dwx_geom_new(), *sb = dwx_geom_new();
    CHECK_HR(fo_call(a.face, 40.0f, g, NULL, NULL, 2, FALSE, FALSE, sa), S_OK);
    CHECK_HR(fo_call(b.face, 40.0f, g, NULL, NULL, 2, FALSE, FALSE, sb), S_OK);
    CHECK(sa->npts == sb->npts && sa->npts > 0 && !memcmp(sa->pts, sb->pts, sizeof(D2D1_POINT_2F) * sa->npts));
    dwx_release(sa); dwx_release(sb); dwx_font_close(&a); dwx_font_close(&b);
}
TEST(reuse_works_after_font_file_release) {
    FO_NEED_FACE();
    /* the face keeps the font data alive: dropping the application's IDWriteFontFile reference is harmless */
    dwx_geom *a = dwx_geom_new(), *b = dwx_geom_new(); UINT16 g = TTF_GID_ROUND;
    CHECK_HR(fo_call(h.face, 48.0f, &g, NULL, NULL, 1, FALSE, FALSE, a), S_OK);
    DW_RELEASE(h.file);
    CHECK_HR(fo_call(h.face, 48.0f, &g, NULL, NULL, 1, FALSE, FALSE, b), S_OK);
    CHECK(a->npts == b->npts && a->npts > 0 && !memcmp(a->pts, b->pts, sizeof(D2D1_POINT_2F) * a->npts));
    dwx_release(a); dwx_release(b); dwx_font_close(&h);
}
TEST(reuse_stream_balanced) {
    FO_NEED_FACE();
    dwx_geom *a = dwx_geom_new(); UINT16 g[3] = { 2, 64, 65 };
    CHECK_HR(fo_call(h.face, 48.0f, g, NULL, NULL, 3, FALSE, FALSE, a), S_OK);
    /* the face may keep the font data mapped for its lifetime, so only record what happened */
    t_note("ReadFileFragment calls so far: %ld, outstanding %ld", (long)dwx_loader_total_reads(h.ld), (long)dwx_loader_total_outstanding(h.ld));
    dwx_release(a); dwx_font_close(&h);
}

/* ---- FPU state ------------------------------------------------------------------------------ */
TEST(fpu_mxcsr_restored) {
    FO_NEED_FACE();
    /* FPUStateSSE saves and restores the caller's MXCSR around the outline computation */
    unsigned saved = _mm_getcsr(), odd = (saved & ~0x6000u) | 0x6000u;   /* round toward zero */
    dwx_geom *s = dwx_geom_new(), *ref = dwx_geom_new(); UINT16 g[3] = { 65, 64, 69 }; HRESULT hr; unsigned after;
    g[2] = TTF_GID_RING;
    CHECK_HR(fo_call(h.face, 33.3f, g, NULL, NULL, 3, FALSE, FALSE, ref), S_OK);
    _mm_setcsr(odd);
    hr = fo_call(h.face, 33.3f, g, NULL, NULL, 3, FALSE, FALSE, s);
    after = _mm_getcsr();
    _mm_setcsr(saved);
    CHECK_HR(hr, S_OK);
    CHECKF((after & 0x6000u) == 0x6000u, "rounding control changed: %08x -> %08x", odd, after);
    CHECK(s->npts == ref->npts && s->npts > 0);
    if (s->npts == ref->npts) {   /* results do not depend on the caller's rounding mode (0.001 DIP) */
        int i, bad = 0; for (i = 0; i < s->npts; i++) if (fabs(s->pts[i].x - ref->pts[i].x) > 0.001 || fabs(s->pts[i].y - ref->pts[i].y) > 0.001) bad++;
        CHECKF(bad == 0, "%d points depend on MXCSR rounding", bad);
    }
    dwx_release(s); dwx_release(ref); dwx_font_close(&h);
}

/* ---- derived interfaces ---------------------------------------------------------------------- */
TEST(iface_fontface1_same_stream) {
    FO_NEED_FACE();
    IDWriteFontFace1 *f1 = NULL; UINT16 g[3] = { 2, 65, 68 };
    if (FAILED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace1, (void **)&f1)) || !f1) { dwx_font_close(&h); t_skip("IDWriteFontFace1 unsupported"); return; }
    dwx_geom *a = dwx_geom_new(), *b = dwx_geom_new();
    CHECK_HR(fo_call(h.face, 55.0f, g, NULL, NULL, 3, FALSE, FALSE, a), S_OK);
    CHECK_HR(IDWriteFontFace1_GetGlyphRunOutline(f1, 55.0f, g, NULL, NULL, 3, FALSE, FALSE, (IDWriteGeometrySink *)b), S_OK);
    CHECK(a->nev == b->nev && a->nev > 0);
    CHECK(a->npts == b->npts && !memcmp(a->pts, b->pts, sizeof(D2D1_POINT_2F) * a->npts));
    /* argument validation is shared */
    CHECK_HR(IDWriteFontFace1_GetGlyphRunOutline(f1, 55.0f, g, NULL, NULL, 3, FALSE, FALSE, NULL), E_INVALIDARG);
    CHECK_HR(IDWriteFontFace1_GetGlyphRunOutline(f1, 0.0f, g, NULL, NULL, 3, FALSE, FALSE, (IDWriteGeometrySink *)b), E_INVALIDARG);
    IDWriteFontFace1_Release(f1);
    dwx_release(a); dwx_release(b); dwx_font_close(&h);
}
TEST(iface_all_fontface_versions_same_vtable_slot) {
    FO_NEED_FACE();
    const IID *iids[] = { &IID_IDWriteFontFace1, &IID_IDWriteFontFace2, &IID_IDWriteFontFace3, &IID_IDWriteFontFace4, &IID_IDWriteFontFace5 };
    UINT16 g[2] = { 2, 65 }; dwx_geom *ref = dwx_geom_new(); int i, tested = 0;
    CHECK_HR(fo_call(h.face, 55.0f, g, NULL, NULL, 2, FALSE, FALSE, ref), S_OK);
    for (i = 0; i < 5; i++) {
        void *p = NULL;
        if (FAILED(IDWriteFontFace_QueryInterface(h.face, iids[i], &p)) || !p) { t_note("IDWriteFontFace%d not supported by this face", i + 1); continue; }
        dwx_geom *s = dwx_geom_new();
        /* every IDWriteFontFaceN inherits the IDWriteFontFace vtable prefix */
        CHECK_HR(IDWriteFontFace_GetGlyphRunOutline((IDWriteFontFace *)p, 55.0f, g, NULL, NULL, 2, FALSE, FALSE, (IDWriteGeometrySink *)s), S_OK);
        CHECKF(s->npts == ref->npts && !memcmp(s->pts, ref->pts, sizeof(D2D1_POINT_2F) * ref->npts), "IDWriteFontFace%d stream differs", i + 1);
        dwx_release(s); IUnknown_Release((IUnknown *)p); tested++;
    }
    CHECK(tested >= 1);
    dwx_release(ref); dwx_font_close(&h);
}

/* ---- concurrency ------------------------------------------------------------------------------ */
typedef struct { IDWriteFontFace *face; dwx_geom *sink; float em; UINT16 g[4]; HRESULT hr; int rounds; } fo_thr;
static DWORD WINAPI fo_thread(LPVOID p) {
    fo_thr *t = (fo_thr *)p; int i; t->hr = S_OK;
    for (i = 0; i < t->rounds; i++) {
        dwx_geom *s = dwx_geom_new();
        HRESULT hr = fo_call(t->face, t->em, t->g, NULL, NULL, 4, FALSE, FALSE, s);
        if (FAILED(hr)) t->hr = hr;
        if (i == t->rounds - 1) { t->sink = s; } else dwx_release(s);
    }
    return 0;
}
TEST(thread_concurrent_same_face) {
    FO_NEED_FACE();
    fo_thr th[8]; HANDLE hd[8]; int i; dwx_geom *ref = dwx_geom_new(); UINT16 g[4] = { 2, 64, 65, 68 };
    CHECK_HR(fo_call(h.face, 29.0f, g, NULL, NULL, 4, FALSE, FALSE, ref), S_OK);
    for (i = 0; i < 8; i++) { memset(&th[i], 0, sizeof th[i]); th[i].face = h.face; th[i].em = 29.0f; memcpy(th[i].g, g, sizeof g); th[i].rounds = 50; hd[i] = CreateThread(NULL, 0, fo_thread, &th[i], 0, NULL); }
    for (i = 0; i < 8; i++) {
        if (hd[i]) { CHECK(WaitForSingleObject(hd[i], 20000) == WAIT_OBJECT_0); CloseHandle(hd[i]); }
        CHECK_HR(th[i].hr, S_OK);
        if (th[i].sink) { CHECK(th[i].sink->npts == ref->npts && !memcmp(th[i].sink->pts, ref->pts, sizeof(D2D1_POINT_2F) * ref->npts)); dwx_release(th[i].sink); }
    }
    dwx_release(ref); dwx_font_close(&h);
}
TEST(thread_concurrent_different_sizes) {
    FO_NEED_FACE();
    fo_thr th[4]; HANDLE hd[4]; int i; UINT16 g[4] = { 2, 64, 65, 68 };
    for (i = 0; i < 4; i++) { memset(&th[i], 0, sizeof th[i]); th[i].face = h.face; th[i].em = 10.0f + 17.0f * i; memcpy(th[i].g, g, sizeof g); th[i].rounds = 30; hd[i] = CreateThread(NULL, 0, fo_thread, &th[i], 0, NULL); }
    for (i = 0; i < 4; i++) {
        dwx_geom *ref = dwx_geom_new();
        if (hd[i]) { WaitForSingleObject(hd[i], 20000); CloseHandle(hd[i]); }
        CHECK_HR(th[i].hr, S_OK);
        CHECK_HR(fo_call(h.face, th[i].em, g, NULL, NULL, 4, FALSE, FALSE, ref), S_OK);
        if (th[i].sink) { CHECK(th[i].sink->npts == ref->npts && !memcmp(th[i].sink->pts, ref->pts, sizeof(D2D1_POINT_2F) * ref->npts)); dwx_release(th[i].sink); }
        dwx_release(ref);
    }
    dwx_font_close(&h);
}
