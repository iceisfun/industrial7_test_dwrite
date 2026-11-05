/* IDWriteFactory4::ComputeGlyphOrigins, both overloads.
 * Win10 probing: the 3-argument form forwards to the 5-argument form with measuring mode NATURAL and no
 * matrix. The 5-argument form: run == NULL -> E_INVALIDARG; else origins[0..glyphCount) are zeroed and the internal
 * ComputeGlyphOrigins runs with pixelsPerDip 1, isSideways = run.isSideways != 0, rtl = bidiLevel & 1; the matrix is never
 * applied to the output points. Internal behaviour:
 *   - LTR, advances supplied, not sideways: pure accumulation, x_i = ox + sum(adv_j, j < i), y_i = oy; no font access at all
 *     (matrix and measuring mode are ignored).
 *   - otherwise font metrics are fetched through a metrics converter. RTL: x_i = cur - fontAdvance_i, then cur -= (supplied advance if
 *     any else the font advance). Sideways: x_i = cur + verticalOriginY_i, cur += (supplied advance else advanceHeight_i),
 *     y_i = oy + 0.5 * advanceWidth_i (client units).
 *   - glyph offsets (when present): LTR x += advanceOffset, RTL x -= advanceOffset; both y -= ascenderOffset.
 * glyphCount == 0 touches nothing (the origins pointer is not dereferenced). */
#include "g2.h"

typedef struct { IDWriteFactory4 *f4; } ogf;

static void run_init(DWRITE_GLYPH_RUN *r, IDWriteFontFace *face, float em, UINT32 n, const UINT16 *idx, const float *adv,
                     const DWRITE_GLYPH_OFFSET *off, BOOL side, UINT8 bidi)
{
    memset(r, 0, sizeof *r);
    r->fontFace = face; r->fontEmSize = em; r->glyphCount = n; r->glyphIndices = idx; r->glyphAdvances = adv;
    r->glyphOffsets = off; r->isSideways = side; r->bidiLevel = bidi;
}
static HRESULT co3(IDWriteFactory4 *f, const DWRITE_GLYPH_RUN *r, float ox, float oy, D2D1_POINT_2F *out)
{
    D2D1_POINT_2F o; o.x = ox; o.y = oy;
    return IDWriteFactory4_ComputeGlyphOrigins_(f, r, o, out);
}
static HRESULT co5(IDWriteFactory4 *f, const DWRITE_GLYPH_RUN *r, DWRITE_MEASURING_MODE mm, float ox, float oy, const DWRITE_MATRIX *m, D2D1_POINT_2F *out)
{
    D2D1_POINT_2F o; o.x = ox; o.y = oy;
    return IDWriteFactory4_ComputeGlyphOrigins(f, r, mm, o, m, out);
}
#define OG_OPEN(x) g2_face x; if (!g2_face_open(&x, NULL)) return; \
    if (!x.f4) { g2_face_close(&x); t_skip("IDWriteFactory4 unsupported"); return; }

/* design advance (default synthetic font) of a glyph */
static float adv_du(UINT16 g) { return g == 0 ? 1200.0f : g == TTF_GID_EMPTY500 ? 500.0f : 1000.0f; }

TEST(gap2_og_ltr_explicit_table)
{
    OG_OPEN(x);
    static const UINT16 idx[8] = { 2, 3, 4, 5, 6, 7, 8, 9 };
    static const float advs[8] = { 10, 0, 7.5f, 3, 100, 0.25f, 12, 9 };
    static const DWRITE_MEASURING_MODE modes[] = { DWRITE_MEASURING_MODE_NATURAL, DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_MEASURING_MODE_GDI_NATURAL };
    static const DWRITE_MATRIX mats[] = { { 1, 0, 0, 1, 0, 0 }, { 3, 0, 0, 3, 5, 5 }, { 0, 1, -1, 0, 0, 0 } };
    static const UINT32 counts[] = { 1, 2, 3, 5, 8 };
    size_t ci, mi, ki;
    for (ci = 0; ci < sizeof counts / sizeof counts[0]; ci++) {
        UINT32 n = counts[ci], i;
        for (mi = 0; mi < sizeof modes / sizeof modes[0]; mi++) for (ki = 0; ki <= sizeof mats / sizeof mats[0]; ki++) {
            DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[8]; float cur = 20.0f; int bad = 0;
            run_init(&r, x.face, 16.0f, n, idx, advs, NULL, FALSE, 0);
            CHECK_HR_OK(co5(x.f4, &r, modes[mi], 20.0f, -3.5f, ki ? &mats[ki - 1] : NULL, out));
            for (i = 0; i < n; i++) { if (out[i].x != cur || out[i].y != -3.5f) bad++; cur += advs[i]; }
            CHECKF(bad == 0, "count %u mode %d matrix %zu: %d wrong origins", n, (int)modes[mi], ki, bad);
        }
        {
            DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[8]; float cur = 20.0f; int bad = 0;
            run_init(&r, x.face, 16.0f, n, idx, advs, NULL, FALSE, 0);
            CHECK_HR_OK(co3(x.f4, &r, 20.0f, -3.5f, out));
            for (i = 0; i < n; i++) { if (out[i].x != cur || out[i].y != -3.5f) bad++; cur += advs[i]; }
            CHECKF(bad == 0, "3-arg count %u: %d wrong origins", n, bad);
        }
    }
    g2_face_close(&x);
}

TEST(gap2_og_invalid_mode_ignored_on_simple_path)
{
    /* LTR + supplied advances never consults the measuring mode, so even out-of-range values succeed. */
    OG_OPEN(x);
    static const UINT16 idx[2] = { 2, 3 }; static const float adv[2] = { 4, 5 };
    static const UINT32 modes[] = { 3, 4, 7, 0xFF, 0x10000, 0x7fffffff, 0xFFFFFFFFu };
    size_t i;
    for (i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[2];
        run_init(&r, x.face, 16.0f, 2, idx, adv, NULL, FALSE, 0);
        CHECKF(co5(x.f4, &r, (DWRITE_MEASURING_MODE)modes[i], 1, 2, NULL, out) == S_OK, "mode %u", modes[i]);
        CHECK(out[0].x == 1.0f && out[1].x == 5.0f && out[1].y == 2.0f);
    }
    g2_face_close(&x);
}

TEST(gap2_og_null_run_both_overloads)
{
    OG_OPEN(x);
    D2D1_POINT_2F out[2]; int i;
    if (t_is_wine()) { g2_face_close(&x); t_skip("Wine dereferences a NULL run"); return; }
    for (i = 0; i < 2; i++) { out[i].x = 123.0f; out[i].y = 456.0f; }
    CHECK_HR(co3(x.f4, NULL, 0, 0, out), E_INVALIDARG);
    CHECK(out[0].x == 123.0f && out[0].y == 456.0f && out[1].x == 123.0f);
    CHECK_HR(co5(x.f4, NULL, DWRITE_MEASURING_MODE_NATURAL, 5, 5, NULL, out), E_INVALIDARG);
    CHECK_HR(co5(x.f4, NULL, DWRITE_MEASURING_MODE_GDI_CLASSIC, 5, 5, NULL, out), E_INVALIDARG);
    CHECK(out[0].x == 123.0f && out[1].y == 456.0f);
    g2_face_close(&x);
}

TEST(gap2_og_zero_count_touches_nothing)
{
    /* glyphCount 0: no memset, no font access; the origins pointer is not dereferenced. */
    OG_OPEN(x);
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[2];
    out[0].x = 9; out[0].y = 8; out[1].x = 7; out[1].y = 6;
    run_init(&r, x.face, 16.0f, 0, NULL, NULL, NULL, FALSE, 0);
    CHECK_HR_OK(co3(x.f4, &r, 1, 1, out));
    CHECK(out[0].x == 9.0f && out[0].y == 8.0f && out[1].x == 7.0f);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 1, 1, NULL, out));
    CHECK(out[0].x == 9.0f);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 1, 1, NULL, NULL));
    CHECK_HR_OK(co3(x.f4, &r, 1, 1, NULL));
    run_init(&r, x.face, 16.0f, 0, NULL, NULL, NULL, TRUE, 1);      /* sideways + rtl with no glyphs */
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_GDI_NATURAL, 1, 1, NULL, NULL));
    g2_face_close(&x);
}

TEST(gap2_og_ltr_font_advances_em2048)
{
    /* advances NULL: widths come from the font; at em == unitsPerEm one design unit is exactly one DIP. */
    OG_OPEN(x);
    static const UINT16 idx[6] = { 2, 0, 1, TTF_GID_EMPTY500, TTF_GID_TRIANGLE, 30 };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[6]; float cur = 50.0f; int i;
    run_init(&r, x.face, 2048.0f, 6, idx, NULL, NULL, FALSE, 0);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 50, 7, NULL, out));
    for (i = 0; i < 6; i++) {
        CHECKF(out[i].x == cur && out[i].y == 7.0f, "glyph %d: got (%g,%g) want (%g,7)", i, out[i].x, out[i].y, cur);
        cur += adv_du(idx[i]);
    }
    CHECK_HR_OK(co3(x.f4, &r, 50, 7, out));
    cur = 50.0f;
    for (i = 0; i < 6; i++) { CHECK(out[i].x == cur && out[i].y == 7.0f); cur += adv_du(idx[i]); }
    g2_face_close(&x);
}

TEST(gap2_og_ltr_font_advances_scaled)
{
    OG_OPEN(x);
    static const UINT16 idx[5] = { 2, 0, 1, TTF_GID_EMPTY500, 3 };
    static const float ems[] = { 1024.0f, 512.0f, 100.0f, 16.0f, 12.5f, 1.0f, 4096.0f };
    size_t e;
    for (e = 0; e < sizeof ems / sizeof ems[0]; e++) {
        DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[5]; double cur = 10.0; int i;
        run_init(&r, x.face, ems[e], 5, idx, NULL, NULL, FALSE, 0);
        CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 10, 0, NULL, out));
        for (i = 0; i < 5; i++) {
            CHECKF(DW_APPROX(out[i].x, cur, 1e-3 + 1e-6 * fabs(cur)), "em %g glyph %d: got %g want %g", ems[e], i, out[i].x, cur);
            cur += adv_du(idx[i]) * ems[e] / 2048.0;
        }
    }
    g2_face_close(&x);
}

TEST(gap2_og_three_arg_is_natural_no_matrix)
{
    OG_OPEN(x);
    static const UINT16 idx[4] = { 2, 0, 67, 5 };
    static const float adv[4] = { 11, 12, 13, 14 };
    static const UINT8 bidis[] = { 0, 1, 2, 3, 254, 255 };
    size_t b, a;
    for (b = 0; b < sizeof bidis; b++) for (a = 0; a < 2; a++) {
        DWRITE_GLYPH_RUN r; D2D1_POINT_2F o3[4], o5[4];
        run_init(&r, x.face, 20.0f, 4, idx, a ? adv : NULL, NULL, FALSE, bidis[b]);
        CHECK_HR_OK(co3(x.f4, &r, 30, 40, o3));
        CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 30, 40, NULL, o5));
        CHECKF(memcmp(o3, o5, sizeof o3) == 0, "bidi %u advances %d differ", bidis[b], (int)a);
    }
    g2_face_close(&x);
}

TEST(gap2_og_rtl_font_advances)
{
    /* RTL, no supplied advances: x_i = cur - fontAdvance_i; cur -= fontAdvance_i  =>  origins run leftwards from the start point. */
    OG_OPEN(x);
    static const UINT16 idx[4] = { 2, 0, 1, TTF_GID_EMPTY500 };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[4]; float cur = 5000.0f; int i;
    run_init(&r, x.face, 2048.0f, 4, idx, NULL, NULL, FALSE, 1);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 5000, 9, NULL, out));
    for (i = 0; i < 4; i++) {
        cur -= adv_du(idx[i]);
        CHECKF(out[i].x == cur && out[i].y == 9.0f, "glyph %d: got (%g,%g) want (%g,9)", i, out[i].x, out[i].y, cur);
    }
    g2_face_close(&x);
}

TEST(gap2_og_rtl_explicit_advances_use_font_width_for_x)
{
    /* RTL with supplied advances: the glyph's own x is cur - FONT advance, but the cursor moves by the SUPPLIED advance. */
    OG_OPEN(x);
    static const UINT16 idx[4] = { 2, 0, 1, TTF_GID_EMPTY500 };
    static const float adv[4] = { 30, 40, 50, 60 };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[4]; float cur = 3000.0f; int i;
    run_init(&r, x.face, 2048.0f, 4, idx, adv, NULL, FALSE, 1);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 3000, 0, NULL, out));
    for (i = 0; i < 4; i++) {
        float want = cur - adv_du(idx[i]);
        CHECKF(out[i].x == want, "glyph %d: got %g want %g", i, out[i].x, want);
        cur -= adv[i];
    }
    g2_face_close(&x);
}

TEST(gap2_og_bidi_parity_table)
{
    /* Only bit 0 of bidiLevel selects right-to-left. */
    OG_OPEN(x);
    static const UINT16 idx[3] = { 2, 3, 4 };
    UINT32 lv;
    for (lv = 0; lv <= 255; lv++) {
        DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[3];
        run_init(&r, x.face, 2048.0f, 3, idx, NULL, NULL, FALSE, (UINT8)lv);
        CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 100, 0, NULL, out));
        if (lv & 1) CHECKF(out[0].x == -900.0f && out[1].x == -1900.0f && out[2].x == -2900.0f, "level %u: %g %g %g", lv, out[0].x, out[1].x, out[2].x);
        else CHECKF(out[0].x == 100.0f && out[1].x == 1100.0f && out[2].x == 2100.0f, "level %u: %g %g %g", lv, out[0].x, out[1].x, out[2].x);
    }
    g2_face_close(&x);
}

TEST(gap2_og_offsets_ltr_explicit)
{
    OG_OPEN(x);
    static const UINT16 idx[4] = { 2, 3, 4, 5 };
    static const float adv[4] = { 10, 20, 30, 40 };
    static const DWRITE_GLYPH_OFFSET off[4] = { { 0, 0 }, { 5, 0 }, { 0, 7 }, { -2.5f, -3 } };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[4]; int i;
    run_init(&r, x.face, 16.0f, 4, idx, adv, off, FALSE, 0);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 100, 50, NULL, out));
    {
        float basex[4] = { 100, 110, 130, 160 };
        for (i = 0; i < 4; i++) {
            /* x += advanceOffset, y -= ascenderOffset (positive ascender offset moves the glyph up) */
            CHECKF(out[i].x == basex[i] + off[i].advanceOffset && out[i].y == 50.0f - off[i].ascenderOffset, "glyph %d: (%g,%g)", i, out[i].x, out[i].y);
        }
    }
    CHECK_HR_OK(co3(x.f4, &r, 100, 50, out));
    CHECK(out[1].x == 115.0f && out[2].y == 43.0f && out[3].x == 157.5f && out[3].y == 53.0f);
    g2_face_close(&x);
}

TEST(gap2_og_offsets_rtl_explicit)
{
    OG_OPEN(x);
    static const UINT16 idx[3] = { 2, 3, 4 };
    static const float adv[3] = { 10, 20, 30 };
    static const DWRITE_GLYPH_OFFSET off[3] = { { 5, 2 }, { -5, -2 }, { 0, 0 } };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[3]; float cur = 5000.0f; int i;
    run_init(&r, x.face, 2048.0f, 3, idx, adv, off, FALSE, 1);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 5000, 100, NULL, out));
    for (i = 0; i < 3; i++) {
        /* RTL: x = cur - fontAdvance - advanceOffset; y = oy - ascenderOffset */
        float want = cur - 1000.0f - off[i].advanceOffset;
        CHECKF(out[i].x == want && out[i].y == 100.0f - off[i].ascenderOffset, "glyph %d: (%g,%g) want x %g", i, out[i].x, out[i].y, want);
        cur -= adv[i];
    }
    g2_face_close(&x);
}

TEST(gap2_og_offsets_with_font_advances)
{
    OG_OPEN(x);
    static const UINT16 idx[3] = { 2, 0, 3 };
    static const DWRITE_GLYPH_OFFSET off[3] = { { 1, 1 }, { 2, 2 }, { 3, 3 } };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[3];
    run_init(&r, x.face, 2048.0f, 3, idx, NULL, off, FALSE, 0);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, NULL, out));
    CHECK(out[0].x == 1.0f && out[0].y == -1.0f);
    CHECK(out[1].x == 1002.0f && out[1].y == -2.0f);
    CHECK(out[2].x == 2203.0f && out[2].y == -3.0f);
    g2_face_close(&x);
}

TEST(gap2_og_zero_offsets_equal_null_offsets)
{
    OG_OPEN(x);
    static const UINT16 idx[3] = { 2, 3, 4 };
    static const float adv[3] = { 4, 5, 6 };
    static const DWRITE_GLYPH_OFFSET off[3];
    UINT8 bidi;
    for (bidi = 0; bidi < 2; bidi++) {
        int a;
        for (a = 0; a < 2; a++) {
            DWRITE_GLYPH_RUN r1, r2; D2D1_POINT_2F o1[3], o2[3];
            run_init(&r1, x.face, 2048.0f, 3, idx, a ? adv : NULL, NULL, FALSE, bidi);
            run_init(&r2, x.face, 2048.0f, 3, idx, a ? adv : NULL, off, FALSE, bidi);
            CHECK_HR_OK(co5(x.f4, &r1, DWRITE_MEASURING_MODE_NATURAL, 7, 8, NULL, o1));
            CHECK_HR_OK(co5(x.f4, &r2, DWRITE_MEASURING_MODE_NATURAL, 7, 8, NULL, o2));
            CHECKF(memcmp(o1, o2, sizeof o1) == 0, "bidi %u adv %d", bidi, a);
        }
    }
    g2_face_close(&x);
}

TEST(gap2_og_y_passthrough)
{
    OG_OPEN(x);
    static const UINT16 idx[3] = { 2, 3, 4 };
    static const float ys[] = { 0, -0.0f, 1e-3f, -123.456f, 1e6f, -1e6f };
    size_t i;
    for (i = 0; i < sizeof ys / sizeof ys[0]; i++) {
        int a, b;
        for (a = 0; a < 2; a++) for (b = 0; b < 2; b++) {
            static const float adv[3] = { 4, 5, 6 };
            DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[3];
            run_init(&r, x.face, 2048.0f, 3, idx, a ? adv : NULL, NULL, FALSE, (UINT8)b);
            CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, ys[i], NULL, out));
            CHECKF(out[0].y == ys[i] && out[1].y == ys[i] && out[2].y == ys[i], "y %g adv %d rtl %d", ys[i], a, b);
        }
    }
    g2_face_close(&x);
}

TEST(gap2_og_origin_translation_property)
{
    /* Integer-valued data: origins(o) == origins(0) + o for every path that does not involve glyph offsets. */
    OG_OPEN(x);
    static const UINT16 idx[4] = { 2, 0, 1, 3 };
    static const float adv[4] = { 8, 16, 24, 32 };
    static const float pts[][2] = { { 0, 0 }, { 100, 0 }, { -100, 50 }, { 12345, -678 }, { 0, 1 } };
    int a, b; size_t p;
    for (a = 0; a < 2; a++) for (b = 0; b < 2; b++) for (p = 1; p < sizeof pts / sizeof pts[0]; p++) {
        DWRITE_GLYPH_RUN r; D2D1_POINT_2F z[4], o[4]; int i, bad = 0;
        run_init(&r, x.face, 2048.0f, 4, idx, a ? adv : NULL, NULL, FALSE, (UINT8)b);
        CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, NULL, z));
        CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, pts[p][0], pts[p][1], NULL, o));
        for (i = 0; i < 4; i++) if (o[i].x != z[i].x + pts[p][0] || o[i].y != z[i].y + pts[p][1]) bad++;
        CHECKF(bad == 0, "adv %d rtl %d origin %zu: %d differ", a, b, p, bad);
    }
    g2_face_close(&x);
}

TEST(gap2_og_repeated_glyph_long_run)
{
    OG_OPEN(x);
    enum { N = 500 };
    UINT16 idx[N]; D2D1_POINT_2F out[N]; DWRITE_GLYPH_RUN r; int i, bad = 0;
    for (i = 0; i < N; i++) idx[i] = 2;
    run_init(&r, x.face, 2048.0f, N, idx, NULL, NULL, FALSE, 0);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, NULL, out));
    for (i = 0; i < N; i++) if (out[i].x != 1000.0f * i) bad++;
    CHECKF(bad == 0, "%d wrong origins", bad);
    run_init(&r, x.face, 2048.0f, N, idx, NULL, NULL, FALSE, 1);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, NULL, out));
    for (i = 0, bad = 0; i < N; i++) if (out[i].x != -1000.0f * (i + 1)) bad++;
    CHECKF(bad == 0, "rtl: %d wrong origins", bad);
    g2_face_close(&x);
}

TEST(gap2_og_run_not_modified)
{
    OG_OPEN(x);
    UINT16 idx[3] = { 2, 3, 4 }; float adv[3] = { 4, 5, 6 }; DWRITE_GLYPH_OFFSET off[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
    DWRITE_GLYPH_RUN r, r0; UINT16 idx0[3]; float adv0[3]; DWRITE_GLYPH_OFFSET off0[3]; D2D1_POINT_2F out[3];
    run_init(&r, x.face, 16.0f, 3, idx, adv, off, FALSE, 1);
    r0 = r; memcpy(idx0, idx, sizeof idx); memcpy(adv0, adv, sizeof adv); memcpy(off0, off, sizeof off);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 1, 1, NULL, out));
    CHECK(memcmp(&r, &r0, sizeof r) == 0 && !memcmp(idx, idx0, sizeof idx) && !memcmp(adv, adv0, sizeof adv) && !memcmp(off, off0, sizeof off));
    EXPECT_REFS(x.face, dw_refcount((IUnknown *)x.face));       /* no leaked reference on the face */
    {
        ULONG before = dw_refcount((IUnknown *)x.face), after;
        CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 1, 1, NULL, out));
        after = dw_refcount((IUnknown *)x.face);
        CHECKF(before == after, "face refcount %lu -> %lu", before, after);
    }
    g2_face_close(&x);
}

TEST(gap2_og_deterministic_repeat)
{
    OG_OPEN(x);
    static const UINT16 idx[4] = { 2, 0, 1, 67 };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F a[4], b[4]; int i;
    run_init(&r, x.face, 13.37f, 4, idx, NULL, NULL, FALSE, 0);
    for (i = 0; i < 10; i++) {
        CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 3.3f, 4.4f, NULL, i ? b : a));
        if (i) CHECK(memcmp(a, b, sizeof a) == 0);
    }
    g2_face_close(&x);
}

TEST(gap2_og_gdi_modes_integral_advances)
{
    /* GDI measuring modes snap glyph advances to whole pixels (pixelsPerDip 1). */
    OG_OPEN(x);
    static const UINT16 idx[5] = { 2, 3, 0, 1, 4 };
    static const DWRITE_MEASURING_MODE modes[] = { DWRITE_MEASURING_MODE_GDI_CLASSIC, DWRITE_MEASURING_MODE_GDI_NATURAL };
    static const float ems[] = { 9.0f, 12.0f, 13.0f, 16.0f, 20.0f };
    size_t m, e;
    for (m = 0; m < 2; m++) for (e = 0; e < sizeof ems / sizeof ems[0]; e++) {
        DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[5]; int i;
        run_init(&r, x.face, ems[e], 5, idx, NULL, NULL, FALSE, 0);
        CHECK_HR_OK(co5(x.f4, &r, modes[m], 0, 0, NULL, out));
        for (i = 1; i < 5; i++) {
            float d = out[i].x - out[i - 1].x;
            CHECKF(d == floorf(d) && d > 0, "mode %d em %g step %d: %g", (int)modes[m], ems[e], i, d);
        }
        t_note("GDI mode %d em %g steps: %g %g %g %g", (int)modes[m], ems[e], out[1].x - out[0].x, out[2].x - out[1].x, out[3].x - out[2].x, out[4].x - out[3].x);
    }
    g2_face_close(&x);
}

TEST(gap2_og_natural_matrix_effect_noted)
{
    OG_OPEN(x);
    static const UINT16 idx[3] = { 2, 3, 4 };
    static const DWRITE_MATRIX m2 = { 2, 0, 0, 2, 10, 10 };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F a[3], b[3];
    run_init(&r, x.face, 20.0f, 3, idx, NULL, NULL, FALSE, 0);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, NULL, a));
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &m2, b));
    /* the output is never multiplied by the matrix (its dx/dy never appear); a matrix may only influence font metric snapping */
    CHECK(b[0].x == 0.0f && b[0].y == 0.0f);
    if (memcmp(a, b, sizeof a)) t_note("natural mode: matrix changed advances (%g vs %g)", a[1].x, b[1].x);
    g2_face_close(&x);
}

static int sideways_run(g2_face *x, const float *adv, UINT8 bidi, D2D1_POINT_2F *out, UINT32 n, DWRITE_MEASURING_MODE mm)
{
    UINT16 idx[8]; DWRITE_GLYPH_RUN r; UINT32 i;
    for (i = 0; i < n; i++) idx[i] = 2;
    run_init(&r, x->face, 2048.0f, n, idx, adv, NULL, TRUE, bidi);
    return co5(x->f4, &r, mm, 100, 200, NULL, out) == S_OK;
}
TEST(gap2_og_sideways_explicit_advances)
{
    /* Sideways: cursor moves by the supplied advance; the x of each glyph adds its verticalOriginY (identical glyphs => identical bias). */
    OG_OPEN(x);
    static const float adv[4] = { 10, 20, 30, 40 };
    D2D1_POINT_2F out[4]; int i;
    CHECK(sideways_run(&x, adv, 0, out, 4, DWRITE_MEASURING_MODE_NATURAL));
    for (i = 1; i < 4; i++) CHECKF(DW_APPROX(out[i].x - out[i - 1].x, adv[i - 1], 1e-3), "step %d: %g", i, out[i].x - out[i - 1].x);
    for (i = 1; i < 4; i++) CHECK(out[i].y == out[0].y);
    G2_SOFT(out[0].y > 200.0f);       /* y = oy + 0.5 * advanceWidth of the sideways metrics */
    t_note("sideways first origin (%g,%g)", out[0].x, out[0].y);
    g2_face_close(&x);
}
TEST(gap2_og_sideways_font_advances)
{
    OG_OPEN(x);
    D2D1_POINT_2F out[4]; int i; float d0;
    CHECK(sideways_run(&x, NULL, 0, out, 4, DWRITE_MEASURING_MODE_NATURAL));
    d0 = out[1].x - out[0].x;
    CHECKF(d0 > 0, "step %g", d0);
    for (i = 2; i < 4; i++) CHECKF(DW_APPROX(out[i].x - out[i - 1].x, d0, 1e-3), "step %d: %g vs %g", i, out[i].x - out[i - 1].x, d0);
    for (i = 1; i < 4; i++) CHECK(out[i].y == out[0].y);
    t_note("sideways font advance step %g (default font has no vmtx)", d0);
    g2_face_close(&x);
}
TEST(gap2_og_sideways_rtl_runs_leftwards)
{
    OG_OPEN(x);
    static const float adv[3] = { 10, 20, 30 };
    D2D1_POINT_2F out[3];
    CHECK(sideways_run(&x, adv, 1, out, 3, DWRITE_MEASURING_MODE_NATURAL));
    CHECKF(out[1].x < out[0].x && out[2].x < out[1].x, "x %g %g %g", out[0].x, out[1].x, out[2].x);
    CHECKF(DW_APPROX(out[0].x - out[1].x, adv[0], 1e-3) && DW_APPROX(out[1].x - out[2].x, adv[1], 1e-3), "steps %g %g", out[0].x - out[1].x, out[1].x - out[2].x);
    g2_face_close(&x);
}
TEST(gap2_og_sideways_matrix_not_applied)
{
    OG_OPEN(x);
    static const UINT16 idx[2] = { 2, 3 }; static const float adv[2] = { 10, 10 };
    static const DWRITE_MATRIX m = { 100, 0, 0, 100, 1000, 1000 };
    DWRITE_GLYPH_RUN r; D2D1_POINT_2F a[2], b[2];
    run_init(&r, x.face, 2048.0f, 2, idx, adv, NULL, TRUE, 0);
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, NULL, a));
    CHECK_HR_OK(co5(x.f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &m, b));
    CHECKF(fabsf(b[0].x) < 5000.0f && fabsf(b[0].y) < 5000.0f, "matrix translation leaked: (%g,%g)", b[0].x, b[0].y);
    g2_face_close(&x);
}

static void sys_origin_check(const WCHAR *fam)
{
    IDWriteFactory *f = dw_shared(); IDWriteFactory4 *f4 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 4);
    IDWriteFontFace *face; UINT32 cps[6] = { 'H', 'e', 'l', 'o', ' ', 'W' }, i; UINT16 g[6]; DWRITE_FONT_METRICS fm;
    DWRITE_GLYPH_METRICS gm[6]; DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[6];
    if (!f4) { t_skip("IDWriteFactory4 unsupported"); return; }
    face = dw_sysface(f, fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("%ls not installed", fam); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cps, 6, g));
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 6, gm, FALSE));
    IDWriteFontFace_GetMetrics(face, &fm);
    run_init(&r, face, 40.0f, 6, g, NULL, NULL, FALSE, 0);
    CHECK_HR_OK(co5(f4, &r, DWRITE_MEASURING_MODE_NATURAL, 10, 20, NULL, out));
    { double cur = 10; for (i = 0; i < 6; i++) { CHECKF(DW_APPROX(out[i].x, cur, 1e-3), "%ls glyph %u: %g vs %g", fam, i, out[i].x, cur); cur += gm[i].advanceWidth * 40.0 / fm.designUnitsPerEm; } }
    DW_RELEASE(face);
}
TEST(gap2_og_system_arial_matches_design_metrics)   { sys_origin_check(L"Arial"); }
TEST(gap2_og_system_times_matches_design_metrics)   { sys_origin_check(L"Times New Roman"); }
TEST(gap2_og_system_segoe_matches_design_metrics)   { sys_origin_check(L"Segoe UI"); }
TEST(gap2_og_system_consolas_matches_design_metrics) { sys_origin_check(L"Consolas"); }
TEST(gap2_og_system_courier_monospaced_steps)
{
    IDWriteFactory *f = dw_shared(); IDWriteFactory4 *f4 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 4);
    IDWriteFontFace *face; UINT32 cps[5] = { 'i', 'W', 'm', '.', ' ' }; UINT16 g[5]; DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[5]; int i;
    if (!f4) { t_skip("IDWriteFactory4 unsupported"); return; }
    face = dw_sysface(f, L"Courier New", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Courier New not installed"); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cps, 5, g));
    run_init(&r, face, 100.0f, 5, g, NULL, NULL, FALSE, 0);
    CHECK_HR_OK(co5(f4, &r, DWRITE_MEASURING_MODE_NATURAL, 0, 0, NULL, out));
    for (i = 2; i < 5; i++) CHECKF(DW_APPROX(out[i].x - out[i - 1].x, out[1].x - out[0].x, 1e-3), "step %d", i);
    CHECK(out[1].x - out[0].x > 40.0f && out[1].x - out[0].x < 80.0f);
    DW_RELEASE(face);
}

TEST(gap2_og_factory4_qi_and_refcount)
{
    OG_OPEN(x);
    static const UINT16 idx[1] = { 2 }; DWRITE_GLYPH_RUN r; D2D1_POINT_2F out[1]; ULONG a, b;
    run_init(&r, x.face, 16.0f, 1, idx, NULL, NULL, FALSE, 0);
    a = dw_refcount((IUnknown *)x.f4);
    CHECK_HR_OK(co3(x.f4, &r, 0, 0, out));
    b = dw_refcount((IUnknown *)x.f4);
    CHECK(a == b);
    g2_face_close(&x);
}
