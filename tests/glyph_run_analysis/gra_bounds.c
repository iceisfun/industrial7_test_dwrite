/* IDWriteGlyphRunAnalysis::GetAlphaTextureBounds and the geometry that determines it.
 *
 * Facts (Win10 19041 dwrite.dll, DWriteGlyphRunAnalysis::GetAlphaTextureBounds -> GlyphRunAnalysis::GetAlphaTextureBounds):
 *   textureType >= 2 -> E_INVALIDARG and *bounds = {0,0,0,0}.
 *   Otherwise S_OK; the rectangle is the union of the glyph bitmaps' boxes when textureType equals the analysis' own texture kind
 *   (ALIASED_1x1 for aliased / grayscale analyses, CLEARTYPE_3x1 for ClearType ones). For the other kind the rectangle is
 *   {0,0,0,0} (still S_OK). ClearType bounds are widened by the filter padding on both sides, never vertically.
 *   Glyphs without outline contribute nothing; a run without any visible glyph gives {0,0,0,0}.
 *   Baseline origin and glyph origins are multiplied by pixelsPerDip (and by m11 for pure scale matrices; dx/dy of such a matrix
 *   add to the origin before that scaling). Glyph offsets: positive ascenderOffset moves up, advanceOffset right for LTR.
 * Fixture: default synthetic font; letters are the box (100,0)-(900,1400) of 2048 units; em multiples of 512 give integer edges.
 */
#include "gra.h"

#define EM_SET_N 4
static const int em_set[EM_SET_N] = { 512, 1024, 1536, 2048 };

static HRESULT mkal(gra_fx *fx, IDWriteGlyphRunAnalysis **a, float ox, float oy) { return gra_make_al(fx, a, ox, oy); }

static void expect_box(gra_fx *fx, IDWriteGlyphRunAnalysis *a, int em, int dx, int dy, int extra_right, const char *tag) {
    RECT rc;
    HRESULT hr = gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc);
    CHECKF(hr == S_OK, "%s: GetAlphaTextureBounds %s", tag, dw_hr_name(hr));
    CHECKF(rc.left == GRA_X0(em) + dx && rc.top == -GRA_HT(em) + dy && rc.right == GRA_X1(em) + dx + extra_right && rc.bottom == dy,
           "%s: got (%ld,%ld,%ld,%ld) want (%d,%d,%d,%d)", tag, rc.left, rc.top, rc.right, rc.bottom,
           GRA_X0(em) + dx, -GRA_HT(em) + dy, GRA_X1(em) + dx + extra_right, dy);
}

TEST(gra_bounds_box_exact_per_em) {
    GRA_FIX(fx);
    int i;
    for (i = 0; i < EM_SET_N; i++) {
        gra_run(&fx, "A", (float)em_set[i]);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, 0, 0));
        char tag[32]; sprintf(tag, "em %d", em_set[i]);
        expect_box(&fx, a, em_set[i], 0, 0, 0, tag);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_all_box_glyphs_identical) {
    /* 'A'..'Z', 'a'..'z', '0'..'9' share one outline. */
    GRA_FIX(fx);
    const char *set = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    size_t i;
    for (i = 0; set[i]; i++) {
        char s[2] = { set[i], 0 };
        gra_run(&fx, s, 512.0f);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, 0, 0));
        expect_box(&fx, a, 512, 0, 0, 0, s);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_integer_origin_translation) {
    GRA_FIX(fx);
    static const int pts[][2] = { {0,0}, {1,0}, {0,1}, {-1,0}, {0,-1}, {7,13}, {-7,-13}, {100,-200}, {-500,500}, {1000,1000}, {-1000,-1000},
                                  {4096,0}, {0,4096}, {-4096,-4096}, {100000,-100000}, {-123456,654321} };
    size_t i;
    gra_run(&fx, "A", 512.0f);
    for (i = 0; i < sizeof pts / sizeof pts[0]; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, (float)pts[i][0], (float)pts[i][1]));
        char tag[48]; sprintf(tag, "origin (%d,%d)", pts[i][0], pts[i][1]);
        expect_box(&fx, a, 512, pts[i][0], pts[i][1], 0, tag);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_fractional_origin_rounds_like_sampling) {
    /* Box edges sit on integers, so an origin offset of 0.25 does not move any pixel centre across an edge, 0.75 moves exactly one
     * pixel column/row into the box on the far side. Holds for centre-sampling and for round-to-nearest origins alike. */
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    static const struct { float f; int shift; } c[] = { {0.0f, 0}, {0.1f, 0}, {0.25f, 0}, {0.4f, 0}, {0.6f, 1}, {0.75f, 1}, {0.9f, 1}, {-0.25f, 0}, {-0.75f, -1}, {-0.9f, -1}, {1.25f, 1}, {1.75f, 2}, {-1.25f, -1} };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, c[i].f, c[i].f));
        char tag[48]; sprintf(tag, "origin frac %g", c[i].f);
        expect_box(&fx, a, 512, c[i].shift, c[i].shift, 0, tag);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_half_pixel_origin_note) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0.5f, 0.5f));
    RECT rc;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    t_note("aliased box at origin (0.5,0.5): (%ld,%ld,%ld,%ld) (integer origin would be (%d,%d,%d,0))", rc.left, rc.top, rc.right, rc.bottom, GRA_X0(512), -GRA_HT(512), GRA_X1(512));
    CHECK(rc.left >= GRA_X0(512) && rc.left <= GRA_X0(512) + 1 && rc.right >= GRA_X1(512) && rc.right <= GRA_X1(512) + 1);
    CHECK(rc.top >= -GRA_HT(512) && rc.top <= -GRA_HT(512) + 1 && rc.bottom >= 0 && rc.bottom <= 1);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_scale_with_em) {
    GRA_FIX(fx);
    int i;
    RECT first = {0};
    for (i = 0; i < EM_SET_N; i++) {
        gra_run(&fx, "A", (float)em_set[i]);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, 0, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        if (i == 0) first = rc;
        else {
            /* x and y bounds scale exactly with the em size (integer edges) */
            CHECKF((rc.right - rc.left) * 512 == (first.right - first.left) * em_set[i], "em %d width %ld", em_set[i], rc.right - rc.left);
            CHECKF((rc.bottom - rc.top) * 512 == (first.bottom - first.top) * em_set[i], "em %d height %ld", em_set[i], rc.bottom - rc.top);
        }
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_idempotent) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 3, 4));
    RECT r0, r1;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
    int i;
    for (i = 0; i < 20; i++) { CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r1)); CHECK_RECT_EQ(r0, r1); }
    /* interleaved other texture type does not disturb it */
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1));
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r1));
    CHECK_RECT_EQ(r0, r1);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_invalid_texture_type) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    static const int bad[] = { 2, 3, 4, 255, 256, 0x7fffffff, -1, (int)0x80000000 };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        RECT rc; memset(&rc, 0x55, sizeof rc);
        HRESULT hr = IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, (DWRITE_TEXTURE_TYPE)bad[i], &rc);
        CHECKF(hr == GRA_E_INVALIDARG, "texture type %d -> %s", bad[i], dw_hr_name(hr));
        CHECKF(gra_rect_zero(&rc), "texture type %d: rect (%ld,%ld,%ld,%ld) not zeroed", bad[i], rc.left, rc.top, rc.right, rc.bottom);
    }
    EXPECT_REFS(a, 1);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_other_texture_kind_is_zero_rect) {
    /* Exactly one texture type matches an analysis; the other one reports S_OK with an all-zero rect (stale output overwritten). */
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 9, 9));
    RECT r1;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1));
    CHECK_RECT_ZERO(r1);
    IDWriteGlyphRunAnalysis_Release(a);
    int mode;
    for (mode = DWRITE_RENDERING_MODE_GDI_CLASSIC; mode <= DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC; mode++) {
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, (DWRITE_RENDERING_MODE)mode, DWRITE_MEASURING_MODE_NATURAL, 9, 9));
        RECT r0, rc;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc));
        CHECKF(gra_rect_zero(&r0) != gra_rect_zero(&rc), "mode %s: type0 (%ld,%ld,%ld,%ld) type1 (%ld,%ld,%ld,%ld): exactly one must be zero",
               gra_mode_name(mode), r0.left, r0.top, r0.right, r0.bottom, rc.left, rc.top, rc.right, rc.bottom);
        t_note("mode %s legacy overload: %s texture", gra_mode_name(mode), gra_rect_zero(&r0) ? "CLEARTYPE_3x1" : "ALIASED_1x1");
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_natural_modes_are_cleartype_textures) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    static const DWRITE_RENDERING_MODE modes[] = { DWRITE_RENDERING_MODE_NATURAL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC };
    size_t i;
    for (i = 0; i < 2; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, modes[i], DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        RECT r0, r1;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1));
        CHECKF(gra_rect_zero(&r0), "mode %s: ALIASED_1x1 bounds not zero", gra_mode_name(modes[i]));
        CHECKF(!gra_rect_empty(&r1), "mode %s: CLEARTYPE_3x1 bounds empty", gra_mode_name(modes[i]));
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_cleartype_padding_horizontal_only) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    static const DWRITE_RENDERING_MODE modes[] = { DWRITE_RENDERING_MODE_NATURAL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC };
    size_t i;
    for (i = 0; i < 2; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, modes[i], DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        RECT rc;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &rc));
        t_note("%s ClearType bounds (%ld,%ld,%ld,%ld); tight box is (%d,%d,%d,0)", gra_mode_name(modes[i]), rc.left, rc.top, rc.right, rc.bottom, GRA_X0(512), -GRA_HT(512), GRA_X1(512));
        CHECK(rc.top == -GRA_HT(512) && rc.bottom == 0);                                     /* never padded vertically */
        long padl = GRA_X0(512) - rc.left, padr = rc.right - GRA_X1(512);
        CHECKF(padl == padr && padl >= 0 && padl <= 2, "padding left %ld right %ld", padl, padr);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_gdi_modes_note) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    int mode, mm;
    for (mode = DWRITE_RENDERING_MODE_GDI_CLASSIC; mode <= DWRITE_RENDERING_MODE_GDI_NATURAL; mode++) for (mm = 0; mm < 3; mm++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, (DWRITE_RENDERING_MODE)mode, (DWRITE_MEASURING_MODE)mm, 0, 0));
        RECT r0, r1;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r0));
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r1));
        t_note("%s measuring %d: aliased (%ld,%ld,%ld,%ld) cleartype (%ld,%ld,%ld,%ld)", gra_mode_name(mode), mm, r0.left, r0.top, r0.right, r0.bottom, r1.left, r1.top, r1.right, r1.bottom);
        RECT *m = gra_rect_zero(&r0) ? &r1 : &r0;
        CHECK(m->left <= GRA_X0(512) && m->right >= GRA_X1(512) && m->top <= -GRA_HT(512) && m->bottom >= 0);
        CHECK(m->left >= GRA_X0(512) - 3 && m->right <= GRA_X1(512) + 3 && m->top >= -GRA_HT(512) - 1 && m->bottom <= 1);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

/* ---- multi-glyph runs ---------------------------------------------------------------------------------------------- */
TEST(gra_bounds_three_glyphs_explicit_advances) {
    GRA_FIX(fx);
    gra_run(&fx, "ABC", 512.0f);
    float adv[3] = { 300, 700, 50 };
    gra_advances(&fx, adv);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    /* glyph i sits at the sum of the preceding advances; the bounds are the union */
    expect_box(&fx, a, 512, 0, 0, 300 + 700, "ABC advances 300,700,50");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_advance_table) {
    GRA_FIX(fx);
    static const float a2[][2] = { {0, 0}, {10, 0}, {250, 0}, {1000, 0}, {-100, 0}, {-250, 0}, {-37, 0}, {5000, 0} };
    size_t i;
    for (i = 0; i < sizeof a2 / sizeof a2[0]; i++) {
        gra_run(&fx, "AB", 512.0f);
        gra_advances(&fx, a2[i]);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, 0, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        /* second glyph at x = advance[0]; first glyph at 0 */
        int adv0 = (int)a2[i][0];
        int l = adv0 < 0 ? GRA_X0(512) + adv0 : GRA_X0(512);
        int r = adv0 < 0 ? GRA_X1(512) : GRA_X1(512) + adv0;
        CHECKF(rc.left == l && rc.right == r && rc.top == -GRA_HT(512) && rc.bottom == 0, "advance %d: got (%ld,%ld,%ld,%ld) want (%d,%d,%d,0)", adv0, rc.left, rc.top, rc.right, rc.bottom, l, -GRA_HT(512), r);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_null_advances_use_font_advances) {
    /* em 1024: glyph advance 1000/2048*1024 = 500 exactly; NULL glyphAdvances means the font's design advances. */
    GRA_FIX(fx);
    int mm;
    for (mm = 0; mm < 3; mm++) {
        gra_run(&fx, "ABC", 1024.0f);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, (DWRITE_MEASURING_MODE)mm, 0, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        CHECKF(rc.left == GRA_X0(1024) && rc.right == GRA_X1(1024) + 1000 && rc.top == -GRA_HT(1024) && rc.bottom == 0,
               "measuring %d: got (%ld,%ld,%ld,%ld)", mm, rc.left, rc.top, rc.right, rc.bottom);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_null_advances_equal_explicit_font_advances) {
    GRA_FIX(fx);
    gra_run(&fx, "AAAA", 2048.0f);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    float adv[4] = { 1000, 1000, 1000, 1000 };
    gra_advances(&fx, adv);
    GRA_REQ(mkal(&fx, &b, 0, 0));
    RECT ra, rb;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &ra));
    CHECK_HR_OK(gra_bounds(b, DWRITE_TEXTURE_ALIASED_1x1, &rb));
    CHECK_RECT_EQ(ra, rb);
    CHECK_RECT(ra, 100, -1400, 3900, 0);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

TEST(gra_bounds_glyph_offsets) {
    GRA_FIX(fx);
    static const float adv_off[][2] = { {0, 0}, {10, 0}, {-10, 0}, {0, 10}, {0, -10}, {30, 40}, {-30, -40}, {100, 100} };
    size_t i;
    for (i = 0; i < sizeof adv_off / sizeof adv_off[0]; i++) {
        gra_run(&fx, "A", 512.0f);
        DWRITE_GLYPH_OFFSET o = { adv_off[i][0], adv_off[i][1] };
        gra_offsets(&fx, &o);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, 0, 0));
        /* positive advanceOffset moves right (LTR run), positive ascenderOffset moves up (towards negative y) */
        char tag[64]; sprintf(tag, "offset (%g,%g)", adv_off[i][0], adv_off[i][1]);
        expect_box(&fx, a, 512, (int)adv_off[i][0], -(int)adv_off[i][1], 0, tag);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_offsets_do_not_move_pen) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 512.0f);
    float adv[2] = { 300, 300 };
    gra_advances(&fx, adv);
    DWRITE_GLYPH_OFFSET off[2] = { { 50, 20 }, { 0, 0 } };
    gra_offsets(&fx, off);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    /* glyph 0 at (50,-20), glyph 1 still at x = 300 (offset of glyph 0 does not shift it) */
    CHECK_RECT(rc, GRA_X0(512) + 50, -GRA_HT(512) - 20, GRA_X1(512) + 300, 0);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_negative_advance_second_glyph_left) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 512.0f);
    float adv[2] = { -400, 0 };
    gra_advances(&fx, adv);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 1000, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    CHECK_RECT(rc, 1000 - 400 + GRA_X0(512), -GRA_HT(512), 1000 + GRA_X1(512), 0);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

/* ---- empty glyphs ---------------------------------------------------------------------------------------------------- */
TEST(gra_bounds_space_is_empty) {
    GRA_FIX(fx);
    gra_run(&fx, " ", 512.0f);
    int mode;
    for (mode = DWRITE_RENDERING_MODE_ALIASED; mode <= DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC; mode++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, (DWRITE_RENDERING_MODE)mode, DWRITE_MEASURING_MODE_NATURAL, 5, 6));
        RECT r;
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r)); CHECK_RECT_ZERO(r);
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_CLEARTYPE_3x1, &r)); CHECK_RECT_ZERO(r);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_empty500_is_empty) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_EMPTY500;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 3, 3));
    RECT r; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r)); CHECK_RECT_ZERO(r);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_space_then_glyph_shifted_by_advance) {
    GRA_FIX(fx);
    gra_run(&fx, " A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    /* the space advances the pen by 1000/2048*512 = 250 and draws nothing */
    expect_box(&fx, a, 512, GRA_ADV(512), 0, 0, "space then A");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_glyph_then_space_unchanged) {
    GRA_FIX(fx);
    gra_run(&fx, "A    ", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    expect_box(&fx, a, 512, 0, 0, 0, "A + spaces");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_mixed_empty_and_visible_union) {
    GRA_FIX(fx);
    gra_run(&fx, "A A", 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    expect_box(&fx, a, 512, 0, 0, 2 * GRA_ADV(512), "A, space, A");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_zero_glyph_count_zero_rect) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    fx.run.glyphCount = 0;
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 100, 100));
    RECT r;
    memset(&r, 0x77, sizeof r);
    CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r));
    CHECK_RECT_ZERO(r);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

/* ---- other glyph shapes ------------------------------------------------------------------------------------------------ */
TEST(gra_bounds_notdef_wider_box) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_NOTDEF;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    RECT r; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r));
    CHECK_RECT(r, 25, -350, 275, 0);                    /* notdef box (100,0)-(1100,1400) */
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_ring_equals_outer_box) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_RING;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    expect_box(&fx, a, 512, 0, 0, 0, "ring");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_triangle_inside_box) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_TRIANGLE;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    RECT r; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r));
    /* base row is full width; the apex is a point so the top row(s) may stay uncovered */
    CHECK(r.left == GRA_X0(512) && r.right == GRA_X1(512) && r.bottom == 0);
    CHECKF(r.top >= -GRA_HT(512) && r.top <= -GRA_HT(512) + 6, "triangle top %ld", r.top);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_round_glyph_inside_box) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_ROUND;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    RECT r; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r));
    t_note("round glyph bounds (%ld,%ld,%ld,%ld)", r.left, r.top, r.right, r.bottom);
    CHECK(!gra_rect_empty(&r));
    CHECK(r.left >= GRA_X0(512) - 1 && r.right <= GRA_X1(512) + 1 && r.top >= -GRA_HT(512) - 1 && r.bottom <= 1);
    CHECK(r.right - r.left >= (GRA_X1(512) - GRA_X0(512)) - 8 && r.bottom - r.top >= GRA_HT(512) - 8);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_composite_glyph_stacked) {
    GRA_FIX(fx);
    UINT16 g = TTF_GID_COMPOSITE;
    gra_run_gids(&fx, &g, 1, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    RECT r; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &r));
    /* box 'A' (0..1400) with the triangle stacked above it (1400..2800) */
    CHECK(r.left == GRA_X0(512) && r.right == GRA_X1(512) && r.bottom == 0);
    CHECKF(r.top >= -2 * GRA_HT(512) && r.top <= -2 * GRA_HT(512) + 6, "composite top %ld (box top would be %d)", r.top, -2 * GRA_HT(512));
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

/* ---- pixelsPerDip --------------------------------------------------------------------------------------------------------- */
TEST(gra_bounds_ppd_scales_size_and_origin) {
    GRA_FIX(fx);
    static const struct { float ppd; int em; float ox, oy; } c[] = {
        { 1.0f, 1024, 0, 0 }, { 2.0f, 512, 0, 0 }, { 0.5f, 2048, 0, 0 }, { 2.0f, 512, 4, -2 }, { 4.0f, 256, 1, 1 }, { 1.0f, 1024, 8, 8 }, { 0.25f, 4096, 0, 0 },
        { 2.0f, 1024, 3, 5 }, { 0.5f, 2048, 8, -6 },
    };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        gra_run(&fx, "A", (float)c[i].em);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, c[i].ppd, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, c[i].ox, c[i].oy));
        int ppem = (int)(c[i].em * c[i].ppd);                       /* integral by construction */
        int dx = (int)(c[i].ox * c[i].ppd), dy = (int)(c[i].oy * c[i].ppd);
        char tag[64]; sprintf(tag, "ppd %g em %d origin (%g,%g)", c[i].ppd, c[i].em, c[i].ox, c[i].oy);
        expect_box(&fx, a, ppem, dx, dy, 0, tag);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_ppd_one_and_a_half) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 1024.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.5f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 2, 2));
    expect_box(&fx, a, 1536, 3, 3, 0, "ppd 1.5 em 1024");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_ppd_scales_advances) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 512.0f);
    float adv[2] = { 100, 0 };
    gra_advances(&fx, adv);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 2.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    /* advances are in DIPs: 100 DIP = 200 pixels at ppd 2; em 512 DIP = 1024 px */
    expect_box(&fx, a, 1024, 0, 0, 200, "ppd 2 advance 100");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_ppd_scales_glyph_offsets) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    DWRITE_GLYPH_OFFSET o = { 10, 5 };
    gra_offsets(&fx, &o);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 2.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    expect_box(&fx, a, 1024, 20, -10, 0, "ppd 2 offsets (10,5)");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

/* ---- transforms ------------------------------------------------------------------------------------------------------------- */
TEST(gra_bounds_transform_pure_scale) {
    GRA_FIX(fx);
    static const struct { float s; int em; } c[] = { {2.0f, 512}, {4.0f, 256}, {0.5f, 2048}, {1.0f, 1024}, {3.0f, 512}, {0.25f, 4096} };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        gra_run(&fx, "A", (float)c[i].em);
        DWRITE_MATRIX m = { c[i].s, 0, 0, c[i].s, 0, 0 };
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        char tag[48]; sprintf(tag, "scale %g em %d", c[i].s, c[i].em);
        expect_box(&fx, a, (int)(c[i].em * c[i].s), 0, 0, 0, tag);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_transform_scale_scales_origin) {
    /* For a pure scale the baseline origin is scaled together with the glyphs (origin is pre-transform). */
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    DWRITE_MATRIX m = { 2, 0, 0, 2, 0, 0 };
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 10, 20));
    expect_box(&fx, a, 1024, 20, 40, 0, "scale 2 origin (10,20)");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_transform_scale_advances) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 512.0f);
    float adv[2] = { 100, 0 };
    gra_advances(&fx, adv);
    DWRITE_MATRIX m = { 2, 0, 0, 2, 0, 0 };
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    expect_box(&fx, a, 1024, 0, 0, 200, "scale 2 advance 100");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_transform_translation_with_unit_scale) {
    GRA_FIX(fx);
    static const struct { float dx, dy; float ppd; } c[] = { {7, 3, 1.0f}, {-7, -3, 1.0f}, {100, 200, 1.0f}, {4, 6, 2.0f}, {0, 0, 1.0f}, {50, -50, 0.5f} };
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        gra_run(&fx, "A", 512.0f / c[i].ppd);
        DWRITE_MATRIX m = { 1, 0, 0, 1, c[i].dx, c[i].dy };
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, c[i].ppd, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        char tag[64]; sprintf(tag, "dx %g dy %g ppd %g", c[i].dx, c[i].dy, c[i].ppd);
        /* dx/dy are pre-ppd units: they scale by pixelsPerDip like the origin */
        expect_box(&fx, a, 512, (int)(c[i].dx * c[i].ppd), (int)(c[i].dy * c[i].ppd), 0, tag);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

static const struct { const char *name; DWRITE_MATRIX m; int l, t, r, b; } xf1024[] = {
    /* box at em 1024: x 50..450, y -700..0 (y down) */
    { "flip x",       { -1, 0, 0, 1, 0, 0 },   -450, -700,  -50,    0 },
    { "flip y",       {  1, 0, 0, -1, 0, 0 },    50,    0,  450,  700 },
    { "rot 90 cw",    {  0, 1, -1, 0, 0, 0 },     0,   50,  700,  450 },
    { "rot 180",      { -1, 0, 0, -1, 0, 0 },  -450,    0,  -50,  700 },
    { "rot 270 cw",   {  0, -1, 1, 0, 0, 0 },  -700, -450,    0,  -50 },
    { "scale x 2",    {  2, 0, 0, 1, 0, 0 },    100, -700,  900,    0 },
    { "scale y 2",    {  1, 0, 0, 2, 0, 0 },     50,-1400,  450,    0 },
    { "scale x .5",   { .5f, 0, 0, 1, 0, 0 },    25, -700,  225,    0 },
    { "swap xy",      {  0, 1, 1, 0, 0, 0 },   -700,   50,    0,  450 },
};
TEST(gra_bounds_transform_orthogonal_table) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 1024.0f);
    size_t i;
    for (i = 0; i < sizeof xf1024 / sizeof xf1024[0]; i++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, &xf1024[i].m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        CHECKF(rc.left == xf1024[i].l && rc.top == xf1024[i].t && rc.right == xf1024[i].r && rc.bottom == xf1024[i].b,
               "%s: got (%ld,%ld,%ld,%ld) want (%d,%d,%d,%d)", xf1024[i].name, rc.left, rc.top, rc.right, rc.bottom, xf1024[i].l, xf1024[i].t, xf1024[i].r, xf1024[i].b);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_transform_rotation_multi_glyph_advance_direction) {
    /* Advances follow the transformed baseline: after a 90 degree clockwise rotation the pen moves down the screen. */
    GRA_FIX(fx);
    gra_run(&fx, "AB", 1024.0f);
    float adv[2] = { 600, 0 };
    gra_advances(&fx, adv);
    DWRITE_MATRIX m = { 0, 1, -1, 0, 0, 0 };
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    CHECK_RECT(rc, 0, 50, 700, 450 + 600);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_transform_rotation_with_translation_note) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 1024.0f);
    DWRITE_MATRIX m = { 0, 1, -1, 0, 10, 20 };
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    t_note("rot90 with dx=10 dy=20: (%ld,%ld,%ld,%ld); untranslated would be (0,50,700,450)", rc.left, rc.top, rc.right, rc.bottom);
    CHECK(rc.right - rc.left == 700 && rc.bottom - rc.top == 400);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_transform_double_flip_equals_rotation) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 1024.0f);
    float adv[2] = { 600, 0 };
    gra_advances(&fx, adv);
    DWRITE_MATRIX r180 = { -1, 0, 0, -1, 0, 0 };
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &r180, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    /* origin of glyph 1 maps to x = -600 */
    CHECK_RECT(rc, -450 - 600, 0, -50, 700);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_transform_shear_contains_unsheared_base) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 1024.0f);
    DWRITE_MATRIX m = { 1, 0, 0.5f, 1, 0, 0 };            /* x' = x + 0.5*y : italic-like slant, top moves left (y is negative upwards) */
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    t_note("shear m21=0.5: (%ld,%ld,%ld,%ld)", rc.left, rc.top, rc.right, rc.bottom);
    /* corners map to x = 50..450 at the baseline and 50-350..450-350 at the top (y=-700) */
    CHECK(rc.left >= -301 && rc.left <= -299 && rc.right >= 449 && rc.right <= 451 && rc.top == -700 && rc.bottom == 0);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_transform_zero_matrix_no_crash) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    DWRITE_MATRIX m = { 0, 0, 0, 0, 0, 0 };
    IDWriteGlyphRunAnalysis *a = (IDWriteGlyphRunAnalysis *)GRA_SENTINEL;
    HRESULT hr = gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0);
    t_note("all-zero matrix -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && a) {
        RECT rc; HRESULT h2 = gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc);
        t_note("bounds hr %s (%ld,%ld,%ld,%ld)", dw_hr_name(h2), rc.left, rc.top, rc.right, rc.bottom);
        IDWriteGlyphRunAnalysis_Release(a);
    } else CHECK_OUT_NULL(a);
    gra_close(&fx);
}

/* ---- sideways / bidi ----------------------------------------------------------------------------------------------------------- */
TEST(gra_bounds_sideways_equals_rotation_matrix) {
    GRA_FIX(fx);
    gra_run(&fx, "AB", 1024.0f);
    float adv[2] = { 600, 0 };
    gra_advances(&fx, adv);
    IDWriteGlyphRunAnalysis *a = NULL, *b = NULL;
    DWRITE_MATRIX m = { 0, 1, -1, 0, 0, 0 };
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    fx.run.isSideways = TRUE;
    GRA_REQ(gra_make(&fx, &b, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    RECT ra, rb;
    CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &ra));
    CHECK_HR_OK(gra_bounds(b, DWRITE_TEXTURE_ALIASED_1x1, &rb));
    t_note("sideways run: (%ld,%ld,%ld,%ld)", rb.left, rb.top, rb.right, rb.bottom);
    /* sideways rotates each glyph in place but the pen still advances along x: 700 wide glyph + 600 advance */
    CHECK_RECT(rb, 100, -200, 1400, 200);
    CHECK(ra.right - ra.left == 700 && ra.bottom - ra.top == 1000);
    DW_RELEASE(a); DW_RELEASE(b);
    gra_close(&fx);
}

TEST(gra_bounds_sideways_single_glyph) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 1024.0f);
    fx.run.isSideways = TRUE;
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    CHECK_RECT(rc, 100, -200, 800, 200);        /* glyph rotated 90 degrees, centred on the baseline */
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_sideways_with_scale_matrix) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 512.0f);
    fx.run.isSideways = TRUE;
    DWRITE_MATRIX m = { 2, 0, 0, 2, 0, 0 };
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(gra_make(&fx, &a, 1.0f, &m, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    CHECK_RECT(rc, 100, -200, 800, 200);
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_bidi_level_only_low_bit) {
    GRA_FIX(fx);
    int lv;
    RECT base[2];
    for (lv = 0; lv < 8; lv++) {
        gra_run(&fx, "AB", 512.0f);
        float adv[2] = { 300, 300 };
        gra_advances(&fx, adv);
        fx.run.bidiLevel = lv;
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, 1000, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        if (lv < 2) { base[lv] = rc; t_note("bidiLevel %d: (%ld,%ld,%ld,%ld)", lv, rc.left, rc.top, rc.right, rc.bottom); }
        else CHECKF(!memcmp(&rc, &base[lv & 1], sizeof rc), "bidiLevel %d differs from level %d", lv, lv & 1);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    CHECK(base[0].left == 1000 + GRA_X0(512));
    gra_close(&fx);
}

/* ---- size extremes ---------------------------------------------------------------------------------------------------------------- */
TEST(gra_bounds_huge_em_2000) {
    GRA_FIX(fx);
    gra_run(&fx, "A", 2000.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
    t_note("em 2000 bounds (%ld,%ld,%ld,%ld)", rc.left, rc.top, rc.right, rc.bottom);
    if (!gra_rect_empty(&rc)) {
        /* x 97.66..878.9, height 1367.2 */
        CHECK(rc.left >= 96 && rc.left <= 99 && rc.right >= 878 && rc.right <= 880);
        CHECK(rc.top >= -1369 && rc.top <= -1366 && rc.bottom >= 0 && rc.bottom <= 1);
    } else t_note("empty bounds at em 2000");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_tiny_em) {
    GRA_FIX(fx);
    static const float ems[] = { 0.01f, 0.5f, 1.0f, 2.0f, 4.0f };
    size_t i;
    for (i = 0; i < sizeof ems / sizeof ems[0]; i++) {
        gra_run(&fx, "A", ems[i]);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, 0, 0));
        RECT rc; HRESULT hr = gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc);
        CHECK_HR_OK(hr);
        t_note("em %g bounds (%ld,%ld,%ld,%ld)", ems[i], rc.left, rc.top, rc.right, rc.bottom);
        CHECK(rc.right >= rc.left && rc.bottom >= rc.top);
        CHECK(rc.right - rc.left <= 4 && rc.bottom - rc.top <= 4);
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_em_sweep_monotonic) {
    GRA_FIX(fx);
    long prev_w = -1, prev_h = -1;
    int em;
    for (em = 4; em <= 400; em += 6) {
        gra_run(&fx, "A", (float)em);
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(mkal(&fx, &a, 0, 0));
        RECT rc; CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        long w = rc.right - rc.left, h = rc.bottom - rc.top;
        double ew = em * 800.0 / 2048, eh = em * 1400.0 / 2048;
        CHECKF(w >= (long)ew - 1 && w <= (long)ew + 2 && h >= (long)eh - 1 && h <= (long)eh + 2, "em %d: %ldx%ld expected about %.1fx%.1f", em, w, h, ew, eh);
        CHECKF(w >= prev_w - 1 && h >= prev_h - 1, "em %d: size shrank", em);
        prev_w = w; prev_h = h;
        IDWriteGlyphRunAnalysis_Release(a);
    }
    gra_close(&fx);
}

TEST(gra_bounds_run_of_many_glyphs) {
    GRA_FIX(fx);
    char s[17]; int i;
    for (i = 0; i < 16; i++) s[i] = 'A' + i;
    s[16] = 0;
    gra_run(&fx, s, 512.0f);
    IDWriteGlyphRunAnalysis *a = NULL;
    GRA_REQ(mkal(&fx, &a, 0, 0));
    expect_box(&fx, a, 512, 0, 0, 15 * GRA_ADV(512), "16 glyphs");
    IDWriteGlyphRunAnalysis_Release(a);
    gra_close(&fx);
}

TEST(gra_bounds_gdi_measuring_rounds_advances) {
    /* GDI measuring modes use pixel-rounded advances when the run has none: at em 20 the design advance is 9.765625 px. */
    GRA_FIX(fx);
    gra_run(&fx, "AAAA", 20.0f);
    int mm;
    RECT rc[3];
    for (mm = 0; mm < 3; mm++) {
        IDWriteGlyphRunAnalysis *a = NULL;
        GRA_REQ(gra_make(&fx, &a, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, (DWRITE_MEASURING_MODE)mm, 0, 0));
        CHECK_HR_OK(gra_bounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc[mm]));
        t_note("measuring %d: (%ld,%ld,%ld,%ld)", mm, rc[mm].left, rc[mm].top, rc[mm].right, rc[mm].bottom);
        CHECK(!gra_rect_empty(&rc[mm]));
        IDWriteGlyphRunAnalysis_Release(a);
    }
    /* natural: 3 * 9.765625 = 29.3 plus the last glyph; gdi: advances rounded to whole pixels (10) */
    long wn = rc[DWRITE_MEASURING_MODE_NATURAL].right - rc[DWRITE_MEASURING_MODE_NATURAL].left;
    CHECK(wn >= 35 && wn <= 38);
    long wc = rc[DWRITE_MEASURING_MODE_GDI_CLASSIC].right - rc[DWRITE_MEASURING_MODE_GDI_CLASSIC].left;
    CHECK(wc >= 35 && wc <= 40);
    gra_close(&fx);
}
