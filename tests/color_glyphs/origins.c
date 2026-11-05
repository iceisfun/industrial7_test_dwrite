/* IDWriteFactory4::ComputeGlyphOrigins (both overloads).
 *   5-argument form (dwrite.dll!DWriteFactory::ComputeGlyphOrigins 0xd6350): run == NULL -> E_INVALIDARG (nothing written); otherwise
 *   origins[0..glyphCount) are zeroed, then ComputeGlyphOrigins (0x8f4fc) fills them; the transform is NOT applied to the origins (only
 *   the InDIPs variant of IDWriteFactory5.. applies it) and only matters for GDI measuring modes.
 *   3-argument form (0xd6310) = the 5-argument form with DWRITE_MEASURING_MODE_NATURAL and a NULL transform.
 *   Left-to-right, not sideways, advances provided: origin[i] = baseline + (sum of advances[0..i)) with y = baseline.y.
 *   Odd bidiLevel: origin[i].x = cursor - fontAdvance[i] (design advance of the glyph, scaled), cursor -= advances[i] (or the font advance when
 *   the run has no advances).  Glyph offsets: x += advanceOffset (x -= for odd bidiLevel), y -= ascenderOffset. */
#include "cg.h"

static const D2D1_POINT_2F k_poison = { -7777.0f, -8888.0f };
static void poison(D2D1_POINT_2F *p, int n) { int i; for (i = 0; i < n; i++) p[i] = k_poison; }
static int is_poison(const D2D1_POINT_2F *p) { return p->x == k_poison.x && p->y == k_poison.y; }

static HRESULT co3(IDWriteFactory4 *f, const DWRITE_GLYPH_RUN *r, D2D1_POINT_2F o, D2D1_POINT_2F *out) { return IDWriteFactory4_ComputeGlyphOrigins_(f, r, o, out); }
static HRESULT co5(IDWriteFactory4 *f, const DWRITE_GLYPH_RUN *r, DWRITE_MEASURING_MODE mm, D2D1_POINT_2F o, const DWRITE_MATRIX *m, D2D1_POINT_2F *out) { return IDWriteFactory4_ComputeGlyphOrigins(f, r, mm, o, m, out); }

/* model for non-sideways runs */
static void model(const DWRITE_GLYPH_RUN *run, D2D1_POINT_2F o, D2D1_POINT_2F *out) {
    UINT32 i; float cur = o.x; int rtl = run->bidiLevel & 1;
    for (i = 0; i < run->glyphCount; i++) {
        float fa = (float)cg_design_adv(run->glyphIndices[i]) * run->fontEmSize / 2048.0f, a = run->glyphAdvances ? run->glyphAdvances[i] : fa;
        if (rtl) { out[i].x = cur - fa; cur -= a; } else { out[i].x = cur; cur += a; }
        out[i].y = o.y;
        if (run->glyphOffsets) { out[i].x += rtl ? -run->glyphOffsets[i].advanceOffset : run->glyphOffsets[i].advanceOffset; out[i].y -= run->glyphOffsets[i].ascenderOffset; }
    }
}
static int near2(D2D1_POINT_2F a, D2D1_POINT_2F b, float eps) { return fabsf(a.x - b.x) <= eps * (1 + fabsf(b.x)) && fabsf(a.y - b.y) <= eps * (1 + fabsf(b.y)); }
static void check_against_model(IDWriteFactory4 *f, const char *tag, const DWRITE_GLYPH_RUN *run, D2D1_POINT_2F o, float eps) {
    D2D1_POINT_2F got3[65], got5[65], want[65]; UINT32 i; HRESULT hr;
    poison(got3, 65); poison(got5, 65); model(run, o, want);
    hr = co3(f, run, o, got3); CHECKF(hr == S_OK, "%s: 3-arg %s", tag, dw_hr_name(hr));
    hr = co5(f, run, DWRITE_MEASURING_MODE_NATURAL, o, NULL, got5); CHECKF(hr == S_OK, "%s: 5-arg %s", tag, dw_hr_name(hr));
    for (i = 0; i < run->glyphCount; i++) {
        CHECKF(near2(got3[i], want[i], eps), "%s: 3-arg origin[%u] (%g,%g) want (%g,%g)", tag, i, got3[i].x, got3[i].y, want[i].x, want[i].y);
        CHECKF(near2(got5[i], want[i], eps), "%s: 5-arg origin[%u] (%g,%g) want (%g,%g)", tag, i, got5[i].x, got5[i].y, want[i].x, want[i].y);
    }
    CHECKF(is_poison(&got3[run->glyphCount]) && is_poison(&got5[run->glyphCount]), "%s: wrote past glyphCount", tag);
}
#define NEED(f) CG_NEED_F4(f)

TEST(co_vtable_slots_present) {
    NEED(f);
    CHECK(f->lpVtbl->ComputeGlyphOrigins_ != NULL); CHECK(f->lpVtbl->ComputeGlyphOrigins != NULL); CHECK(f->lpVtbl->ComputeGlyphOrigins_ != f->lpVtbl->ComputeGlyphOrigins);
}
TEST(co_null_run_is_invalid_arg_and_writes_nothing) {
    NEED(f); D2D1_POINT_2F o = { 1, 2 }, out[4];
    if (t_is_wine()) { t_skip("Wine dereferences the NULL run"); return; }
    poison(out, 4); CHECK_HR(co3(f, NULL, o, out), CG_E_INVALIDARG); CHECK(is_poison(&out[0]) && is_poison(&out[3]));
    poison(out, 4); CHECK_HR(co5(f, NULL, DWRITE_MEASURING_MODE_NATURAL, o, NULL, out), CG_E_INVALIDARG); CHECK(is_poison(&out[0]) && is_poison(&out[3]));
    poison(out, 4); CHECK_HR(co5(f, NULL, DWRITE_MEASURING_MODE_GDI_CLASSIC, o, NULL, out), CG_E_INVALIDARG); CHECK(is_poison(&out[0]));
}
TEST(co_zero_glyphs_writes_nothing) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g = 2; cgrun r; D2D1_POINT_2F o = { 1, 2 }, out[4]; cg_mkrun(&r, x.face, 20.0f, &g, 1, 1, 0, FALSE, 0); r.run.glyphCount = 0;
    poison(out, 4); CHECK_HR_OK(co3(f, &r.run, o, out)); CHECK(is_poison(&out[0]));
    poison(out, 4); CHECK_HR_OK(co5(f, &r.run, DWRITE_MEASURING_MODE_NATURAL, o, NULL, out)); CHECK(is_poison(&out[0]));
    r.run.glyphIndices = NULL; r.run.glyphAdvances = NULL;
    poison(out, 4); CHECK_HR_OK(co5(f, &r.run, DWRITE_MEASURING_MODE_GDI_NATURAL, o, NULL, out)); CHECK(is_poison(&out[0]));
    cg_close(&x);
}
TEST(co_single_glyph_origin_is_baseline) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    static const D2D1_POINT_2F o[] = { { 0, 0 }, { 123, 321 }, { -5.5f, 7.25f }, { 1e6f, -1e6f } }; int i;
    UINT16 g = 2; cgrun r; cg_mkrun(&r, x.face, 20.0f, &g, 1, 1, 0, FALSE, 0);
    for (i = 0; i < 4; i++) { D2D1_POINT_2F out[1]; poison(out, 1); CHECK_HR_OK(co3(f, &r.run, o[i], out)); CHECK(out[0].x == o[i].x && out[0].y == o[i].y); }
    cg_close(&x);
}
TEST(co_wine_style_two_glyph_example) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 0, 0 }; float adv[] = { 10.0f, 20.0f }; DWRITE_GLYPH_RUN run; D2D1_POINT_2F o = { 123.0f, 321.0f }, out[2];
    memset(&run, 0, sizeof run); run.fontFace = x.face; run.fontEmSize = 16.0f; run.glyphCount = 2; run.glyphIndices = g; run.glyphAdvances = adv;
    poison(out, 2); CHECK_HR_OK(co3(f, &run, o, out));
    CHECK(out[0].x == 123.0f && out[0].y == 321.0f); CHECK(out[1].x == 133.0f && out[1].y == 321.0f);
    poison(out, 2); CHECK_HR_OK(co5(f, &run, DWRITE_MEASURING_MODE_NATURAL, o, NULL, out));
    CHECK(out[0].x == 123.0f && out[1].x == 133.0f && out[1].y == 321.0f);
    cg_close(&x);
}
TEST(co_provided_advances_ltr_every_length) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[64]; static cgrun r; int n, i; D2D1_POINT_2F o = { 100.5f, -20.25f };
    for (i = 0; i < 64; i++) g[i] = (UINT16)(2 + i % 26);
    for (n = 1; n <= 64; n++) { char tag[24]; cg_mkrun(&r, x.face, 16.0f, g, n, 2, 0, FALSE, 0); snprintf(tag, sizeof tag, "n=%d", n); check_against_model(f, tag, &r.run, o, 0); }
    cg_close(&x);
}
TEST(co_provided_advances_exact_sum_order) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4, 5, 6 }; float adv[] = { 0.1f, 0.2f, 0.3f, 1e-3f, 1e3f }; DWRITE_GLYPH_RUN run; D2D1_POINT_2F o = { 1.0f, 0 }, out[5]; int i; float cur = o.x;
    memset(&run, 0, sizeof run); run.fontFace = x.face; run.fontEmSize = 12; run.glyphCount = 5; run.glyphIndices = g; run.glyphAdvances = adv;
    CHECK_HR_OK(co3(f, &run, o, out));
    for (i = 0; i < 5; i++) { CHECKF(out[i].x == cur, "origin[%d] %.9g vs running float sum %.9g", i, out[i].x, cur); cur += adv[i]; }
    cg_close(&x);
}
TEST(co_negative_and_zero_advances) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4, 5 }; float adv[] = { 0.0f, -8.0f, 4.0f, 0.0f }; DWRITE_GLYPH_RUN run; D2D1_POINT_2F o = { 50, 60 }, out[4];
    memset(&run, 0, sizeof run); run.fontFace = x.face; run.fontEmSize = 12; run.glyphCount = 4; run.glyphIndices = g; run.glyphAdvances = adv;
    CHECK_HR_OK(co3(f, &run, o, out));
    CHECK(out[0].x == 50 && out[1].x == 50 && out[2].x == 42 && out[3].x == 46);
    cg_close(&x);
}
TEST(co_computed_advances_natural_exact_at_design_size) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 0, 2, 67, 1, 3 }; static cgrun r; D2D1_POINT_2F o = { 10, 20 }, out[5];
    cg_mkrun(&r, x.face, 2048.0f, g, 5, 0, 0, FALSE, 0);
    poison(out, 5); CHECK_HR_OK(co3(f, &r.run, o, out));
    CHECK(out[0].x == 10 && out[1].x == 1210 && out[2].x == 2210 && out[3].x == 2710 && out[4].x == 3710);
    CHECK(out[0].y == 20 && out[4].y == 20);
    cg_close(&x);
}
TEST(co_computed_advances_various_em) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    static const float em[] = { 1, 8, 9.5f, 12, 16, 24, 48, 100, 333, 1024, 4096 }; int i; UINT16 g[] = { 0, 2, 67, 1, 3, 28 }; static cgrun r;
    for (i = 0; i < 11; i++) { char tag[24]; cg_mkrun(&r, x.face, em[i], g, 6, 0, 0, FALSE, 0); snprintf(tag, sizeof tag, "em %g", em[i]); check_against_model(f, tag, &r.run, (D2D1_POINT_2F){ 5, 6 }, 1e-5f); }
    cg_close(&x);
}
TEST(co_computed_advances_rtl) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 0, 2, 67, 1, 3 }; static cgrun r; D2D1_POINT_2F o = { 10000, 20 }, out[5];
    cg_mkrun(&r, x.face, 2048.0f, g, 5, 0, 0, FALSE, 1);
    poison(out, 5); CHECK_HR_OK(co3(f, &r.run, o, out));
    /* each glyph sits one font advance to the left of the pen: 10000-1200, then the pen moves left by the advance */
    /* glyph 0 sits one font advance (1200) left of the pen; the pen then moves left by 1200 (computed advances) */
    CHECK(out[0].x == 8800 && out[1].x == 10000 - 1200 - 1000 && out[2].x == 10000 - 1200 - 1000 - 500);
    check_against_model(f, "rtl computed", &r.run, o, 0);
    cg_close(&x);
}
TEST(co_rtl_provided_equal_to_font_advances) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4 }; static cgrun r; D2D1_POINT_2F o = { 5000, 0 }, out[3];
    cg_mkrun(&r, x.face, 2048.0f, g, 3, 1, 0, FALSE, 1);
    poison(out, 3); CHECK_HR_OK(co3(f, &r.run, o, out));
    CHECK(out[0].x == 4000 && out[1].x == 3000 && out[2].x == 2000);
    cg_close(&x);
}
TEST(co_rtl_with_advances_differing_from_font) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    /* the x of an RTL origin subtracts the font's own advance of that glyph, the pen moves by the supplied advance */
    UINT16 g[] = { 0, 2, 67 }; DWRITE_GLYPH_RUN run; float adv[] = { 1500.0f, 300.0f, 700.0f }; D2D1_POINT_2F o = { 9000, 0 }, out[3];
    memset(&run, 0, sizeof run); run.fontFace = x.face; run.fontEmSize = 2048.0f; run.glyphCount = 3; run.glyphIndices = g; run.glyphAdvances = adv; run.bidiLevel = 1;
    poison(out, 3); CHECK_HR_OK(co3(f, &run, o, out));
    CHECK(out[0].x == 9000 - 1200); CHECK(out[1].x == 9000 - 1500 - 1000); CHECK(out[2].x == 9000 - 1500 - 300 - 500);
    cg_close(&x);
}
TEST(co_glyph_offsets_ltr) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4 }; static cgrun r; D2D1_POINT_2F o = { 123, 321 }, out[3]; int i;
    cg_mkrun(&r, x.face, 32.0f, g, 3, 2, 1, FALSE, 0);
    poison(out, 3); CHECK_HR_OK(co3(f, &r.run, o, out)); float cur = 123;
    for (i = 0; i < 3; i++) { CHECKF(out[i].x == cur + r.off[i].advanceOffset && out[i].y == 321 - r.off[i].ascenderOffset, "origin[%d] (%g,%g)", i, out[i].x, out[i].y); cur += r.adv[i]; }
    cg_close(&x);
}
TEST(co_glyph_offsets_rtl) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4 }; static cgrun r; D2D1_POINT_2F o = { 4000, 321 }, out[3]; int i;
    cg_mkrun(&r, x.face, 2048.0f, g, 3, 1, 1, FALSE, 1);
    poison(out, 3); CHECK_HR_OK(co3(f, &r.run, o, out)); float cur = 4000;
    for (i = 0; i < 3; i++) { CHECKF(out[i].x == cur - 1000 - r.off[i].advanceOffset && out[i].y == 321 - r.off[i].ascenderOffset, "origin[%d] (%g,%g)", i, out[i].x, out[i].y); cur -= r.adv[i]; }
    cg_close(&x);
}
TEST(co_offsets_without_advances) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 0 }; static cgrun r; D2D1_POINT_2F o = { 0, 0 }, out[2];
    cg_mkrun(&r, x.face, 2048.0f, g, 2, 0, 1, FALSE, 0);
    poison(out, 2); CHECK_HR_OK(co3(f, &r.run, o, out));
    CHECK(out[0].x == 0.25f && out[0].y == 0.5f && out[1].x == 1000 + 0.5f && out[1].y == 1.0f);
    cg_close(&x);
}
TEST(co_bidi_level_parity) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3 }; static cgrun r; int lv; D2D1_POINT_2F o = { 3000, 0 }, out[2];
    for (lv = 0; lv < 8; lv++) {
        char tag[16]; cg_mkrun(&r, x.face, 2048.0f, g, 2, 1, 0, FALSE, lv); snprintf(tag, sizeof tag, "level %d", lv);
        poison(out, 2); CHECK_HR_OK(co3(f, &r.run, o, out));
        if (lv & 1) CHECKF(out[0].x == 2000 && out[1].x == 1000, "%s: %g %g", tag, out[0].x, out[1].x); else CHECKF(out[0].x == 3000 && out[1].x == 4000, "%s: %g %g", tag, out[0].x, out[1].x);
    }
    cg_mkrun(&r, x.face, 2048.0f, g, 2, 1, 0, FALSE, 255); poison(out, 2); CHECK_HR_OK(co3(f, &r.run, o, out)); CHECK(out[0].x == 2000);
    cg_mkrun(&r, x.face, 2048.0f, g, 2, 1, 0, FALSE, 254); poison(out, 2); CHECK_HR_OK(co3(f, &r.run, o, out)); CHECK(out[0].x == 3000);
    cg_close(&x);
}
TEST(co_transform_does_not_move_origins) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    static const DWRITE_MATRIX m[] = { { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 1, 0, 0 }, { 0, 1, -1, 0, 0, 0 }, { 1, 0, 0, 1, 50, 60 }, { 0.5f, 0, 0, 0.5f, 0, 0 }, { 0, 0, 0, 0, 0, 0 } };
    UINT16 g[] = { 2, 3, 4 }; static cgrun r; D2D1_POINT_2F o = { 123, 321 }, a[3], b[3]; int i, k, mm;
    cg_mkrun(&r, x.face, 16.0f, g, 3, 2, 0, FALSE, 0);
    poison(a, 3); CHECK_HR_OK(co5(f, &r.run, DWRITE_MEASURING_MODE_NATURAL, o, NULL, a));
    for (mm = 0; mm < 3; mm++) for (i = 0; i < 6; i++) { poison(b, 3); CHECK_HR_OK(co5(f, &r.run, (DWRITE_MEASURING_MODE)mm, o, &m[i], b)); for (k = 0; k < 3; k++) CHECKF(a[k].x == b[k].x && a[k].y == b[k].y, "mm %d matrix %d origin %d", mm, i, k); }
    cg_close(&x);
}
TEST(co_measuring_modes_with_provided_advances) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4, 5 }; static cgrun r; D2D1_POINT_2F o = { 10, 20 }, a[4], b[4]; int mm, k;
    cg_mkrun(&r, x.face, 14.0f, g, 4, 2, 0, FALSE, 0);
    poison(a, 4); CHECK_HR_OK(co5(f, &r.run, DWRITE_MEASURING_MODE_NATURAL, o, NULL, a));
    for (mm = 1; mm < 3; mm++) { poison(b, 4); CHECK_HR_OK(co5(f, &r.run, (DWRITE_MEASURING_MODE)mm, o, NULL, b)); for (k = 0; k < 4; k++) CHECKF(a[k].x == b[k].x && a[k].y == b[k].y, "mm %d origin %d", mm, k); }
    cg_close(&x);
}
TEST(co_gdi_modes_with_computed_advances_are_pixel_aligned) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4, 5, 6 }; static cgrun r; D2D1_POINT_2F o = { 0, 0 }, out[5]; int mm, k;
    for (mm = 1; mm < 3; mm++) {
        cg_mkrun(&r, x.face, 11.0f, g, 5, 0, 0, FALSE, 0); poison(out, 5);
        CHECK_HR_OK(co5(f, &r.run, (DWRITE_MEASURING_MODE)mm, o, NULL, out));
        t_note("mm %d at 11 ppem: x = %g %g %g %g %g", mm, out[0].x, out[1].x, out[2].x, out[3].x, out[4].x);
        for (k = 0; k < 5; k++) CHECKF(out[k].x == floorf(out[k].x) && out[k].y == 0, "mm %d origin %d (%g,%g)", mm, k, out[k].x, out[k].y);
        CHECK(out[1].x > 0 && out[4].x > out[3].x);
    }
    cg_close(&x);
}
TEST(co_gdi_modes_use_the_transform_for_ppem) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4 }; static cgrun r; D2D1_POINT_2F o = { 0, 0 }, a[3], b[3]; DWRITE_MATRIX m = { 2, 0, 0, 2, 0, 0 };
    cg_mkrun(&r, x.face, 10.0f, g, 3, 0, 0, FALSE, 0);
    poison(a, 3); poison(b, 3);
    CHECK_HR_OK(co5(f, &r.run, DWRITE_MEASURING_MODE_GDI_CLASSIC, o, NULL, a));
    CHECK_HR_OK(co5(f, &r.run, DWRITE_MEASURING_MODE_GDI_CLASSIC, o, &m, b));
    t_note("GDI_CLASSIC 10px: %g %g; with 2x transform: %g %g", a[1].x, a[2].x, b[1].x, b[2].x);
    CHECK(b[2].x >= a[2].x * 0.9f);   /* advances are measured at the scaled ppem (in DIPs: pixel advance / scale) */
    cg_close(&x);
}
TEST(co_invalid_measuring_mode_with_provided_advances_note) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3 }; static cgrun r; D2D1_POINT_2F o = { 1, 2 }, out[2];
    cg_mkrun(&r, x.face, 14.0f, g, 2, 2, 0, FALSE, 0); poison(out, 2);
    HRESULT hr = co5(f, &r.run, (DWRITE_MEASURING_MODE)9, o, NULL, out);
    t_note("measuring mode 9 (provided advances): %s, x1 = %g", dw_hr_name(hr), out[1].x);
    CHECK(hr == S_OK || hr == CG_E_INVALIDARG);
    cg_close(&x);
}
TEST(co_null_font_face_with_provided_advances) {
    NEED(f); UINT16 g[] = { 0, 0, 0 }; float adv[] = { 10, 20, 30 }; DWRITE_GLYPH_RUN run; D2D1_POINT_2F o = { 1, 2 }, out[3];
    /* with advances, no offsets, not RTL, not sideways the face is never consulted */
    if (t_is_wine()) { t_skip("not asserted under Wine"); return; }
    memset(&run, 0, sizeof run); run.fontEmSize = 16; run.glyphCount = 3; run.glyphIndices = g; run.glyphAdvances = adv;
    poison(out, 3); CHECK_HR_OK(co3(f, &run, o, out));
}
TEST(co_sideways_relations) {
    NEED(f); ttf_spec s; ttf_default_spec(&s); s.vertical = 1; s.v_ascent = 1024; s.v_descent = -1024;
    size_t n = 0; uint8_t *b = ttf_build(&s, &n); cgfont x; if (!b || !cg_open_blob(&x, b, n)) { free(b); cg_close(&x); return; } free(b);
    UINT16 g[] = { 2, 2, 2 }; static cgrun r; D2D1_POINT_2F o = { 100, 200 }, out[3];
    cg_mkrun(&r, x.face, 2048.0f, g, 3, 2, 0, TRUE, 0);
    poison(out, 3); CHECK_HR_OK(co3(f, &r.run, o, out));
    t_note("sideways origins: (%g,%g) (%g,%g) (%g,%g) for advances %g %g %g", out[0].x, out[0].y, out[1].x, out[1].y, out[2].x, out[2].y, r.adv[0], r.adv[1], r.adv[2]);
    /* identical glyphs: the per-glyph vertical-origin shift is the same, so the pen advance shows up unchanged and y never moves */
    CHECK_NEAR(out[1].x - out[0].x, r.adv[0], 1e-3); CHECK_NEAR(out[2].x - out[1].x, r.adv[1], 1e-3);
    CHECK(out[0].y == out[1].y && out[1].y == out[2].y);
    /* the same run, not sideways, starts at the baseline origin */
    r.run.isSideways = FALSE; D2D1_POINT_2F h[3]; poison(h, 3); CHECK_HR_OK(co3(f, &r.run, o, h)); CHECK(h[0].x == 100 && h[0].y == 200);
    cg_close(&x);
}
TEST(co_sideways_rtl_relations) {
    NEED(f); ttf_spec s; ttf_default_spec(&s); s.vertical = 1; s.v_ascent = 1024; s.v_descent = -1024;
    size_t n = 0; uint8_t *b = ttf_build(&s, &n); cgfont x; if (!b || !cg_open_blob(&x, b, n)) { free(b); cg_close(&x); return; } free(b);
    UINT16 g[] = { 2, 2, 2 }; static cgrun r; D2D1_POINT_2F o = { 5000, 200 }, out[3];
    cg_mkrun(&r, x.face, 2048.0f, g, 3, 2, 0, TRUE, 1);
    poison(out, 3); CHECK_HR_OK(co3(f, &r.run, o, out));
    CHECK_NEAR(out[0].x - out[1].x, r.adv[0], 1e-3); CHECK_NEAR(out[1].x - out[2].x, r.adv[1], 1e-3); CHECK(out[0].y == out[1].y);
    cg_close(&x);
}
TEST(co_sideways_computed_advances_use_vertical_advance) {
    NEED(f); ttf_spec s; ttf_default_spec(&s); s.vertical = 1; s.v_ascent = 1024; s.v_descent = -1024;
    size_t n = 0; uint8_t *b = ttf_build(&s, &n); cgfont x; if (!b || !cg_open_blob(&x, b, n)) { free(b); cg_close(&x); return; } free(b);
    UINT16 g[] = { 2, 2, 2 }; static cgrun r; D2D1_POINT_2F o = { 0, 0 }, out[3];
    cg_mkrun(&r, x.face, 100.0f, g, 3, 0, 0, TRUE, 0);
    poison(out, 3); CHECK_HR_OK(co3(f, &r.run, o, out));
    /* vmtx advance of every glyph = unitsPerEm (2048): one em at any size */
    CHECK_NEAR(out[1].x - out[0].x, 100.0f, 1e-3); CHECK_NEAR(out[2].x - out[1].x, 100.0f, 1e-3);
    cg_close(&x);
}
TEST(co_array_canary_after_count) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3, 4 }; static cgrun r; D2D1_POINT_2F buf[6], o = { 1, 2 }; int rtl;
    for (rtl = 0; rtl < 2; rtl++) {
        cg_mkrun(&r, x.face, 16.0f, g, 3, rtl ? 0 : 2, rtl, FALSE, rtl); poison(buf, 6);
        CHECK_HR_OK(co3(f, &r.run, o, buf)); CHECK(is_poison(&buf[3]) && is_poison(&buf[4]) && is_poison(&buf[5]) && !is_poison(&buf[2]));
        poison(buf, 6); CHECK_HR_OK(co5(f, &r.run, DWRITE_MEASURING_MODE_NATURAL, o, NULL, buf)); CHECK(is_poison(&buf[3]) && !is_poison(&buf[0]));
    }
    cg_close(&x);
}
TEST(co_overwrites_previous_content) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3 }; static cgrun r; D2D1_POINT_2F o = { 7, 8 }, a[2], b[2];
    cg_mkrun(&r, x.face, 16.0f, g, 2, 2, 0, FALSE, 0);
    memset(a, 0xAB, sizeof a); memset(b, 0x00, sizeof b);
    CHECK_HR_OK(co3(f, &r.run, o, a)); CHECK_HR_OK(co3(f, &r.run, o, b)); CHECK(!memcmp(a, b, sizeof a));
    cg_close(&x);
}
TEST(co_large_values) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3 }; static cgrun r; D2D1_POINT_2F o = { 1e7f, -1e7f };
    cg_mkrun(&r, x.face, 16.0f, g, 2, 2, 0, FALSE, 0); check_against_model(f, "large origin", &r.run, o, 0);
    cg_mkrun(&r, x.face, 16.0f, g, 2, 2, 0, FALSE, 1); check_against_model(f, "large origin rtl", &r.run, o, 1e-6f);
    cg_close(&x);
}
TEST(co_em_size_zero_with_computed_advances) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    UINT16 g[] = { 2, 3 }; static cgrun r; D2D1_POINT_2F o = { 1, 2 }, out[2];
    cg_mkrun(&r, x.face, 0.0f, g, 2, 0, 0, FALSE, 0); poison(out, 2);
    HRESULT hr = co3(f, &r.run, o, out);
    t_note("em 0: %s origins (%g,%g) (%g,%g)", dw_hr_name(hr), out[0].x, out[0].y, out[1].x, out[1].y);
    CHECK_HR_OK(hr); CHECK(out[0].x == 1 && out[1].x == 1);
    cg_close(&x);
}
TEST(co_color_fixture_runs) {
    NEED(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F, GID_B, GID_C, GID_a }; static cgrun r;
    cg_mkrun(&r, x.face, 20.0f, g, 5, 2, 1, FALSE, 0); check_against_model(f, "colour glyphs", &r.run, (D2D1_POINT_2F){ 10, 20 }, 0);
    cg_mkrun(&r, x.face, 2048.0f, g, 5, 0, 1, FALSE, 1); check_against_model(f, "colour glyphs rtl", &r.run, (D2D1_POINT_2F){ 10000, 20 }, 0);
    cg_close(&x);
}
TEST(co_origins_agree_with_translate_color_glyph_run) {
    NEED(f); CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_F, GID_A, GID_F, GID_B }; static cgrun r; D2D1_POINT_2F o = { 40, 50 }, out[4]; IDWriteColorGlyphRunEnumerator *e = NULL; int ltr;
    for (ltr = 0; ltr < 2; ltr++) {
        cg_mkrun(&r, x.face, 2048.0f, g, 4, 0, 0, FALSE, ltr ? 0 : 1);
        poison(out, 4); CHECK_HR_OK(co3(f, &r.run, o, out));
        CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(cg_f2(), o.x, o.y, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
        if (e) {
            BOOL has; int k = 0;
            while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) {
                const DWRITE_COLOR_GLYPH_RUN *c = NULL; IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c);
                /* every run of the enumeration lives on the baseline of the input origin */
                if (c) CHECK(c->baselineOriginY == o.y);
                k++;
            }
            DW_RELEASE(e);
        }
    }
    cg_close(&x);
}
static void sys_origin_check(const WCHAR *fam, const char *tag) {
    NEED(f); cgfont x; if (!cg_open_sys(&x, fam)) { cg_close(&x); return; }
    UINT32 cp[16], i; UINT16 gl[16]; DWRITE_GLYPH_METRICS gm[16]; DWRITE_FONT_METRICS fm; static cgrun r; D2D1_POINT_2F o = { 12.5f, 80 }, out[16];
    for (i = 0; i < 16; i++) cp[i] = "Hello, World! Ag"[i];
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(x.face, cp, 16, gl));
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(x.face, gl, 16, gm, FALSE));
    IDWriteFontFace_GetMetrics(x.face, &fm);
    cg_mkrun(&r, x.face, 36.0f, gl, 16, 0, 0, FALSE, 0);
    poison(out, 16); CHECK_HR_OK(co3(f, &r.run, o, out));
    float cur = o.x;
    for (i = 0; i < 16; i++) { float want = cur; CHECKF(fabsf(out[i].x - want) <= 1e-3f * (1 + fabsf(want)) && out[i].y == o.y, "%s origin[%u] (%g,%g) want (%g,%g)", tag, i, out[i].x, out[i].y, want, o.y); cur += gm[i].advanceWidth * 36.0f / fm.designUnitsPerEm; }
    cg_mkrun(&r, x.face, 36.0f, gl, 16, 0, 0, FALSE, 1);
    poison(out, 16); CHECK_HR_OK(co3(f, &r.run, o, out)); cur = o.x;
    for (i = 0; i < 16; i++) { float a = gm[i].advanceWidth * 36.0f / fm.designUnitsPerEm; CHECKF(fabsf(out[i].x - (cur - a)) <= 1e-3f * (1 + fabsf(cur)), "%s rtl origin[%u] %g want %g", tag, i, out[i].x, cur - a); cur -= a; }
    cg_close(&x);
}
TEST(co_system_arial) { sys_origin_check(L"Arial", "Arial"); }
TEST(co_system_times) { sys_origin_check(L"Times New Roman", "Times"); }
TEST(co_system_segoe_ui) { sys_origin_check(L"Segoe UI", "Segoe UI"); }
TEST(co_system_consolas) { sys_origin_check(L"Consolas", "Consolas"); }
TEST(co_system_segoe_ui_emoji) { sys_origin_check(L"Segoe UI Emoji", "Segoe UI Emoji"); }
TEST(co_three_arg_equals_five_arg_natural) {
    NEED(f); cgfont x; if (!cg_open_plain(&x)) { cg_close(&x); return; }
    int n, adv; static cgrun r; UINT16 g[64]; int i; for (i = 0; i < 64; i++) g[i] = (UINT16)(i % 69);
    for (adv = 0; adv < 3; adv++) for (n = 1; n <= 64; n += 7) {
        D2D1_POINT_2F a[64], b[64], o = { 9, 10 }; cg_mkrun(&r, x.face, 17.0f, g, n, adv, adv == 2, FALSE, n & 1);
        poison(a, 64); poison(b, 64); CHECK_HR_OK(co3(f, &r.run, o, a)); CHECK_HR_OK(co5(f, &r.run, DWRITE_MEASURING_MODE_NATURAL, o, NULL, b));
        CHECKF(!memcmp(a, b, sizeof(D2D1_POINT_2F) * n), "n=%d adv=%d differ", n, adv);
    }
    cg_close(&x);
}
