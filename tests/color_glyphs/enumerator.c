/* IDWriteColorGlyphRunEnumerator / IDWriteColorGlyphRunEnumerator1 behaviour on the COLR fixture.
 *   MoveNext (dwrite.dll!0x1daf0): *hasRun = 0 first, S_OK always; state 0 -> base run, states 1/2 -> next colour layer, then FALSE forever.
 *   GetCurrentRun (v1: 0x2c2f0; v0 0xe1120 forwards to it): S_OK + pointer to the enumerator's record while a run is current, otherwise
 *       *run = NULL and HRESULT 0x8007139F (HRESULT_FROM_WIN32(ERROR_INVALID_STATE)).
 *   QueryInterface (0xe1140): IUnknown, IDWriteColorGlyphRunEnumerator and IDWriteColorGlyphRunEnumerator1 return the same pointer. */
#include "cg.h"

typedef struct snap {
    UINT32 n; UINT16 gl[64]; float adv[64]; float x, y, c[4]; UINT16 pal; UINT32 fmt; int mm; BOOL sideways; UINT32 bidi; void *face; float em; int has_off; int has_adv;
    const void *desc; const void *rec;
} snap;
static int collect(IDWriteColorGlyphRunEnumerator *e, snap *s, int max) {
    int k = 0; BOOL has;
    IDWriteColorGlyphRunEnumerator1 *e1 = NULL;
    IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1);
    while (k < max && SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) {
        const DWRITE_COLOR_GLYPH_RUN *c = NULL; const DWRITE_COLOR_GLYPH_RUN1 *c1 = NULL; UINT32 i;
        if (FAILED(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c)) || !c) break;
        memset(&s[k], 0, sizeof s[k]);
        s[k].n = c->glyphRun.glyphCount; s[k].x = c->baselineOriginX; s[k].y = c->baselineOriginY; s[k].pal = c->paletteIndex;
        s[k].c[0] = c->runColor.r; s[k].c[1] = c->runColor.g; s[k].c[2] = c->runColor.b; s[k].c[3] = c->runColor.a;
        s[k].sideways = c->glyphRun.isSideways; s[k].bidi = c->glyphRun.bidiLevel; s[k].face = c->glyphRun.fontFace; s[k].em = c->glyphRun.fontEmSize;
        s[k].has_off = c->glyphRun.glyphOffsets != NULL; s[k].has_adv = c->glyphRun.glyphAdvances != NULL; s[k].desc = c->glyphRunDescription; s[k].rec = c;
        for (i = 0; i < c->glyphRun.glyphCount && i < 64; i++) { s[k].gl[i] = c->glyphRun.glyphIndices[i]; if (c->glyphRun.glyphAdvances) s[k].adv[i] = c->glyphRun.glyphAdvances[i]; }
        if (e1 && SUCCEEDED(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e1, &c1)) && c1) { s[k].fmt = c1->glyphImageFormat; s[k].mm = c1->measuringMode; }
        k++;
    }
    if (e1) IDWriteColorGlyphRunEnumerator1_Release(e1);
    return k;
}
static IDWriteColorGlyphRunEnumerator *mk(cgfont *x, const UINT16 *g, int n, UINT32 pal, DWRITE_MEASURING_MODE mm, float ox, float oy, int adv, BOOL sw, UINT32 bidi, HRESULT *hr_out) {
    static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; HRESULT hr;
    cg_mkrun(&r, x->face, 20.0f, g, n, adv, 0, sw, bidi);
    hr = IDWriteFactory2_TranslateColorGlyphRun(cg_f2(), ox, oy, &r.run, NULL, mm, NULL, pal, &e);
    if (hr_out) *hr_out = hr;
    return e;
}
#define NEED_E(var, x, g, n, pal) IDWriteColorGlyphRunEnumerator *var = mk(&x, g, n, pal, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL); if (!var) { CHECKF(0, "translate failed"); cg_close(&x); return; }

TEST(en_run_counts_table) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR);
    /* base run + every layer whose palette entry is not fully transparent (palette 0 entry 4 is transparent, palette 1 entry 4 is not) */
    static const struct { UINT16 g[3]; int n; UINT32 pal; int runs; } t[] = {
        { { GID_A }, 1, 0, 3 }, { { GID_A }, 1, 1, 3 }, { { GID_B }, 1, 0, 2 }, { { GID_B }, 1, 1, 2 }, { { GID_C }, 1, 0, 4 }, { { GID_C }, 1, 1, 4 },
        { { GID_D }, 1, 0, 2 }, { { GID_D }, 1, 1, 3 }, { { GID_E }, 1, 0, 1 }, { { GID_E }, 1, 1, 2 }, { { GID_a }, 1, 0, 5 }, { { GID_a }, 1, 1, 5 },
        { { GID_0 }, 1, 0, 2 }, { { GID_0 + 2 }, 1, 0, 1 }, { { GID_0 + 2 }, 1, 1, 2 }, { { GID_0 + 8 }, 1, 0, 1 },
        { { GID_A, GID_F }, 2, 0, 3 }, { { GID_F, GID_A }, 2, 0, 3 }, { { GID_A, GID_A }, 2, 0, 5 }, { { GID_A, GID_B, GID_C }, 3, 0, 1 + 2 + 1 + 3 },
        { { GID_F, GID_A, GID_F }, 3, 1, 3 }, { { GID_E, GID_E, GID_E }, 3, 0, 1 }, { { GID_D, GID_D, GID_D }, 3, 0, 1 + 3 }, { { GID_D, GID_D, GID_D }, 3, 1, 1 + 9 },
    };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        IDWriteColorGlyphRunEnumerator *e = mk(&x, t[i].g, t[i].n, t[i].pal, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL); snap s[40];
        if (!e) { CHECKF(0, "row %d: no enumerator", i); continue; }
        int k = collect(e, s, 40); CHECKF(k == t[i].runs, "row %d: %d runs, want %d", i, k, t[i].runs); DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_movenext_returns_true_value_one) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; NEED_E(e, x, &g, 1, 0);
    BOOL has; int i;
    for (i = 0; i < 3; i++) { has = (BOOL)0x77777777; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)); EXPECT_EQ(has, 1); }
    has = (BOOL)0x77777777; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)); EXPECT_EQ(has, 0);
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_movenext_after_end_stays_false) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_B; NEED_E(e, x, &g, 1, 0); BOOL has; int i;
    for (i = 0; i < 2; i++) IDWriteColorGlyphRunEnumerator_MoveNext(e, &has);
    for (i = 0; i < 20; i++) { has = 0x55; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)); EXPECT_EQ(has, 0); }
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_getcurrentrun_after_end_is_invalid_state) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_B; NEED_E(e, x, &g, 1, 0); BOOL has; int i;
    IDWriteColorGlyphRunEnumerator1 *e1 = NULL; IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1);
    for (i = 0; i < 3; i++) IDWriteColorGlyphRunEnumerator_MoveNext(e, &has);
    EXPECT_EQ(has, 0);
    for (i = 0; i < 3; i++) {
        const DWRITE_COLOR_GLYPH_RUN *c = (const void *)(ULONG_PTR)0x1234; const DWRITE_COLOR_GLYPH_RUN1 *c1 = (const void *)(ULONG_PTR)0x1234;
        CHECK_HR(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c), CG_E_NOT_VALID_STATE); CHECK(c == NULL);
        if (e1) { CHECK_HR(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e1, &c1), CG_E_NOT_VALID_STATE); CHECK(c1 == NULL); }
        IDWriteColorGlyphRunEnumerator_MoveNext(e, &has);
    }
    if (e1) IDWriteColorGlyphRunEnumerator1_Release(e1);
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_getcurrentrun_before_first_movenext) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; NEED_E(e, x, &g, 1, 0);
    const DWRITE_COLOR_GLYPH_RUN *c = (const void *)(ULONG_PTR)0x1234; HRESULT hr = IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c);
    t_note("GetCurrentRun before MoveNext -> %s, run %p", dw_hr_name(hr), (const void *)c);
    CHECK(hr == S_OK ? c != NULL : (hr == CG_E_NOT_VALID_STATE && c == NULL));
    BOOL has; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)); EXPECT_EQ(has, 1);
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_getcurrentrun_is_idempotent) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_C; NEED_E(e, x, &g, 1, 0); BOOL has; int i;
    IDWriteColorGlyphRunEnumerator_MoveNext(e, &has); IDWriteColorGlyphRunEnumerator_MoveNext(e, &has);
    const DWRITE_COLOR_GLYPH_RUN *a = NULL, *b = NULL; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &a));
    DWRITE_COLOR_GLYPH_RUN copy; if (a) copy = *a;
    for (i = 0; i < 5; i++) { b = NULL; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &b)); CHECK(a == b); if (a && b) CHECK(b->glyphRun.glyphIndices[0] == copy.glyphRun.glyphIndices[0] && b->baselineOriginX == copy.baselineOriginX && b->paletteIndex == copy.paletteIndex); }
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_current_record_address_across_movenext) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_a; NEED_E(e, x, &g, 1, 0); BOOL has; const DWRITE_COLOR_GLYPH_RUN *prev = NULL, *c = NULL; int same = 0, n = 0;
    while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) { IDWriteColorGlyphRunEnumerator_GetCurrentRun(e, &c); if (prev && prev == c) same++; prev = c; n++; }
    t_note("%d of %d successive GetCurrentRun records share an address (the pointer is only valid until the next MoveNext)", same, n);
    EXPECT_EQ(n, 5);
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_base_run_comes_first) {
    CG_OPEN(x, CGFX_COLR); snap s[40];
    static const struct { UINT16 g[4]; int n; } t[] = { { { GID_A }, 1 }, { { GID_F, GID_A }, 2 }, { { GID_B, GID_F, GID_C }, 3 }, { { GID_a, GID_b, GID_F, GID_0 }, 4 } }; int i;
    for (i = 0; i < 4; i++) {
        IDWriteColorGlyphRunEnumerator *e = mk(&x, t[i].g, t[i].n, 0, DWRITE_MEASURING_MODE_NATURAL, 12.5f, 33.0f, 2, FALSE, 0, NULL);
        if (!e) { CHECKF(0, "row %d", i); continue; }
        int k = collect(e, s, 40), j; CHECK(k >= 1);
        if (k >= 1) {
            EXPECT_EQ(s[0].n, (UINT32)t[i].n); EXPECT_EQ(s[0].pal, 0xFFFF);
            CHECK(s[0].c[0] == 0 && s[0].c[1] == 0 && s[0].c[2] == 0 && s[0].c[3] == 0);
            CHECK_NEAR(s[0].x, 12.5f, 1e-5); CHECK_NEAR(s[0].y, 33.0f, 1e-5);
            for (j = 0; j < t[i].n; j++) {
                UINT16 gid = t[i].g[j]; int isc = cg_layers_of(gid, NULL) > 0;
                CHECKF(s[0].gl[j] == (isc ? 1 : gid), "row %d glyph %d: %u (input %u, colour %d)", i, j, s[0].gl[j], gid, isc);
            }
        }
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_base_run_format) {
    CG_OPEN(x, CGFX_COLR); snap s[40];
    UINT16 allc[] = { GID_A, GID_B }, mix[] = { GID_A, GID_F };
    IDWriteColorGlyphRunEnumerator *e = mk(&x, allc, 2, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL); int k;
    if (e) { k = collect(e, s, 40); CHECK(k > 0); if (k > 0) EXPECT_EQ(s[0].fmt, CGF_NONE); DW_RELEASE(e); }
    e = mk(&x, mix, 2, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL);
    if (e) { k = collect(e, s, 40); CHECK(k > 0); if (k > 0) EXPECT_EQ(s[0].fmt, CGF_TT); DW_RELEASE(e); }
    cg_close(&x);
}
TEST(en_layer_runs_have_one_glyph_and_same_face) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g[] = { GID_A, GID_F, GID_a, GID_C };
    IDWriteColorGlyphRunEnumerator *e = mk(&x, g, 4, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL); int k, i;
    if (e) {
        k = collect(e, s, 40); EXPECT_EQ(k, 1 + 2 + 5 + 3);
        for (i = 0; i < k; i++) { CHECKF(s[i].face == (void *)x.face, "run %d face", i); CHECKF(s[i].em == 20.0f, "run %d em", i); CHECKF(s[i].has_adv, "run %d advances", i); CHECKF(s[i].desc == NULL, "run %d desc", i); if (i) CHECKF(s[i].n == 1, "run %d glyphCount %u", i, s[i].n); }
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_layer_glyph_ids_follow_colr_order) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; int gid;
    for (gid = 0; gid < TTF_DEF_NGLYPHS; gid++) {
        const ttf_colr_layer *L; int nl = cg_layers_of(gid, &L), k, j, at = 0; UINT16 g = (UINT16)gid;
        if (!nl) continue;
        IDWriteColorGlyphRunEnumerator *e = mk(&x, &g, 1, 1, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 0, FALSE, 0, NULL); if (!e) { CHECKF(0, "glyph %d", gid); continue; }
        k = collect(e, s, 40);
        for (j = 0; j < nl; j++) {
            if (L[j].pal < CG_NENT && (cg_pal[1][L[j].pal] >> 24) == 0) continue;   /* skipped layers */
            at++;
            if (at < k) CHECKF(s[at].gl[0] == L[j].gid, "glyph %d layer %d: %u want %u", gid, j, s[at].gl[0], L[j].gid);
            if (at < k && s[at].gl[0] == gid) CHECKF(0, "layer of %d is the base glyph", gid);
        }
        EXPECT_EQ(k, 1 + at);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_layer_format_is_truetype_colr) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g[] = { GID_A, GID_F }; int k, i;
    IDWriteColorGlyphRunEnumerator *e = mk(&x, g, 2, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL);
    if (e) { k = collect(e, s, 40); EXPECT_EQ(s[0].fmt, CGF_TT); for (i = 1; i < k; i++) CHECKF(s[i].fmt == (CGF_TT | CGF_COLR), "layer %d format 0x%x", i, s[i].fmt); DW_RELEASE(e); }
    cg_close(&x);
}
TEST(en_foreground_and_out_of_range_layer_colors) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g = GID_C; int p;
    for (p = 0; p < 2; p++) {
        IDWriteColorGlyphRunEnumerator *e = mk(&x, &g, 1, p, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL); int k;
        if (!e) { CHECKF(0, "pal %d", p); continue; }
        k = collect(e, s, 40); EXPECT_EQ(k, 4);
        if (k == 4) {
            /* layer 0: palette index 0xFFFF = foreground; layer 1: index 9 >= 6 entries -> treated the same; layer 2: entry 3 */
            EXPECT_EQ(s[1].pal, 0xFFFF); CHECK(s[1].c[0] == 0 && s[1].c[1] == 0 && s[1].c[2] == 0 && s[1].c[3] == 0); EXPECT_EQ(s[1].gl[0], GID_TRI);
            EXPECT_EQ(s[2].pal, 0xFFFF); CHECK(s[2].c[0] == 0 && s[2].c[1] == 0 && s[2].c[2] == 0 && s[2].c[3] == 0); EXPECT_EQ(s[2].gl[0], GID_ROUND);
            EXPECT_EQ(s[3].pal, 3); EXPECT_EQ(s[3].gl[0], GID_RING);
            uint32_t v = cg_pal[p][3];
            CHECK_NEAR(s[3].c[0], ((v >> 16) & 255) / 255.0f, 1e-5); CHECK_NEAR(s[3].c[1], ((v >> 8) & 255) / 255.0f, 1e-5); CHECK_NEAR(s[3].c[2], (v & 255) / 255.0f, 1e-5); CHECK_NEAR(s[3].c[3], (v >> 24) / 255.0f, 1e-5);
        }
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_transparent_layers_are_skipped_per_palette) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g = GID_D; int k;
    IDWriteColorGlyphRunEnumerator *e = mk(&x, &g, 1, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL);
    if (e) { k = collect(e, s, 40); EXPECT_EQ(k, 2); if (k == 2) { EXPECT_EQ(s[1].gl[0], GID_ROUND); EXPECT_EQ(s[1].pal, 0); } DW_RELEASE(e); }
    e = mk(&x, &g, 1, 1, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL);
    if (e) { k = collect(e, s, 40); EXPECT_EQ(k, 3); if (k == 3) { EXPECT_EQ(s[1].gl[0], GID_TRI); EXPECT_EQ(s[1].pal, 4); EXPECT_EQ(s[2].gl[0], GID_ROUND); } DW_RELEASE(e); }
    cg_close(&x);
}
TEST(en_all_layers_transparent_leaves_only_base_run) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g = GID_E; int k;
    IDWriteColorGlyphRunEnumerator *e = mk(&x, &g, 1, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL);
    if (e) {
        k = collect(e, s, 40); EXPECT_EQ(k, 1);
        if (k == 1) { EXPECT_EQ(s[0].n, 1); EXPECT_EQ(s[0].gl[0], 1); EXPECT_EQ(s[0].fmt, CGF_NONE); }
        DW_RELEASE(e);
    } else CHECKF(0, "a glyph whose layers are all transparent still counts as a colour glyph");
    cg_close(&x);
}
TEST(en_color_conversion_is_byte_over_255) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; int p, i;
    for (p = 0; p < 2; p++) {
        UINT16 g[] = { GID_0, GID_0 + 1, GID_0 + 3, GID_0 + 4, GID_0 + 5 };   /* palette entries 0,5,3,2,1 at palette p */
        static const int ent[5] = { 0, 5, 3, 2, 1 };
        for (i = 0; i < 5; i++) {
            IDWriteColorGlyphRunEnumerator *e = mk(&x, &g[i], 1, p, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL); int k;
            if (!e) { CHECKF(0, "digit %d", i); continue; }
            k = collect(e, s, 40); EXPECT_EQ(k, 2);
            if (k == 2) {
                uint32_t v = cg_pal[p][ent[i]];
                CHECKF(DW_APPROX(s[1].c[0], ((v >> 16) & 255) / 255.0f, 1e-6) && DW_APPROX(s[1].c[1], ((v >> 8) & 255) / 255.0f, 1e-6) && DW_APPROX(s[1].c[2], (v & 255) / 255.0f, 1e-6) && DW_APPROX(s[1].c[3], (v >> 24) / 255.0f, 1e-6),
                       "pal %d entry %d: %g %g %g %g", p, ent[i], s[1].c[0], s[1].c[1], s[1].c[2], s[1].c[3]);
                CHECK(s[1].c[0] >= 0 && s[1].c[0] <= 1 && s[1].c[1] >= 0 && s[1].c[1] <= 1 && s[1].c[2] >= 0 && s[1].c[2] <= 1 && s[1].c[3] >= 0 && s[1].c[3] <= 1);
                EXPECT_EQ(s[1].pal, ent[i]);
            }
            DW_RELEASE(e);
        }
    }
    cg_close(&x);
}
TEST(en_input_origin_y_is_every_runs_y) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g[] = { GID_F, GID_A, GID_C }; IDWriteColorGlyphRunEnumerator *e = mk(&x, g, 3, 0, DWRITE_MEASURING_MODE_NATURAL, 5.0f, -77.5f, 2, FALSE, 0, NULL); int k, i;
    if (e) { k = collect(e, s, 40); CHECK(k > 3); for (i = 0; i < k; i++) CHECKF(s[i].y == -77.5f, "run %d y %g", i, s[i].y); DW_RELEASE(e); }
    cg_close(&x);
}
TEST(en_layer_x_accumulates_preceding_advances) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g[] = { GID_F, GID_F, GID_A, GID_F, GID_B };
    static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; int k;
    cg_mkrun(&r, x.face, 20.0f, g, 5, 2, 0, FALSE, 0);
    float cum2 = r.adv[0] + r.adv[1], cum4 = cum2 + r.adv[2] + r.adv[3];
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(cg_f2(), 100, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        k = collect(e, s, 40); EXPECT_EQ(k, 1 + 2 + 1);
        if (k == 4) { CHECK_NEAR(s[0].x, 100.0f, 1e-5); CHECK_NEAR(s[1].x, 100 + cum2, 1e-4); CHECK_NEAR(s[2].x, 100 + cum2, 1e-4); CHECK_NEAR(s[3].x, 100 + cum4, 1e-4); }
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_layer_x_with_rtl_run) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g[] = { GID_F, GID_F, GID_A }; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; int k;
    cg_mkrun(&r, x.face, 20.0f, g, 3, 2, 0, FALSE, 1);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(cg_f2(), 100, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) { k = collect(e, s, 40); EXPECT_EQ(k, 3); if (k == 3) { CHECK_NEAR(s[0].x, 100.0f, 1e-5); CHECK_NEAR(s[1].x, 100 - r.adv[0] - r.adv[1], 1e-4); CHECK_NEAR(s[2].x, 100 - r.adv[0] - r.adv[1], 1e-4); EXPECT_EQ(s[1].bidi, 1); } DW_RELEASE(e); }
    cg_close(&x);
}
TEST(en_flags_are_copied_to_every_run) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g[] = { GID_A, GID_B }; int sw, bd;
    for (sw = 0; sw < 2; sw++) for (bd = 0; bd < 4; bd += 3) {
        IDWriteColorGlyphRunEnumerator *e = mk(&x, g, 2, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 2, sw, bd, NULL); int k, i;
        if (!e) { CHECKF(0, "sw %d bd %d", sw, bd); continue; }
        k = collect(e, s, 40); CHECK(k >= 4);
        for (i = 0; i < k; i++) CHECKF(s[i].sideways == sw && s[i].bidi == (UINT32)bd, "run %d: sideways %d bidi %u", i, s[i].sideways, s[i].bidi);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_measuring_mode_reported_by_enumerator1) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g[] = { GID_A, GID_F }; int mm;
    for (mm = 0; mm < 3; mm++) {
        IDWriteColorGlyphRunEnumerator *e = mk(&x, g, 2, 0, (DWRITE_MEASURING_MODE)mm, 0, 0, 2, FALSE, 0, NULL); int k, i;
        if (!e) { CHECKF(0, "mm %d", mm); continue; }
        k = collect(e, s, 40); CHECK(k >= 3);
        for (i = 0; i < k; i++) CHECKF(s[i].mm == mm, "mm %d run %d reports %d", mm, i, s[i].mm);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_repeated_glyph_scales_run_count) {
    CG_OPEN(x, CGFX_COLR); snap s[200]; int n;
    for (n = 1; n <= 30; n++) {
        UINT16 g[64]; int i; for (i = 0; i < n; i++) g[i] = GID_A;
        IDWriteColorGlyphRunEnumerator *e = mk(&x, g, n, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL);
        if (!e) { CHECKF(0, "n=%d", n); continue; }
        int k = collect(e, s, 200); CHECKF(k == 1 + 2 * n, "n=%d: %d runs", n, k);
        for (i = 1; i < k; i++) if (s[i].x < s[i - 1].x) CHECKF(0, "n=%d: x decreases at run %d", n, i);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_layer_x_non_decreasing_in_ltr_non_increasing_in_rtl) {
    CG_OPEN(x, CGFX_COLR); snap s[100]; UINT16 g[] = { GID_A, GID_B, GID_C, GID_a, GID_D }; int rtl;
    for (rtl = 0; rtl < 2; rtl++) {
        IDWriteColorGlyphRunEnumerator *e = mk(&x, g, 5, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, rtl, NULL); int k, i;
        if (!e) { CHECKF(0, "rtl %d", rtl); continue; }
        k = collect(e, s, 100);
        for (i = 2; i < k; i++) CHECKF(rtl ? s[i].x <= s[i - 1].x : s[i].x >= s[i - 1].x, "rtl %d run %d: x %g after %g", rtl, i, s[i].x, s[i - 1].x);
        DW_RELEASE(e);
    }
    cg_close(&x);
}
TEST(en_iterate_through_enumerator1_only) {
    CG_OPEN(x, CGFX_COLR); UINT16 g[] = { GID_A, GID_F }; IDWriteColorGlyphRunEnumerator *e = mk(&x, g, 2, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL);
    if (!e) { CHECKF(0, "no enumerator"); cg_close(&x); return; }
    IDWriteColorGlyphRunEnumerator1 *e1 = NULL; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1));
    if (e1) {
        BOOL has = 0; int k = 0;
        while (SUCCEEDED(IDWriteColorGlyphRunEnumerator1_MoveNext(e1, &has)) && has && k < 10) {
            const DWRITE_COLOR_GLYPH_RUN1 *c1 = NULL; const DWRITE_COLOR_GLYPH_RUN *c = NULL;
            CHECK_HR_OK(IDWriteColorGlyphRunEnumerator1_GetCurrentRun(e1, &c1)); CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_GetCurrentRun((IDWriteColorGlyphRunEnumerator *)e1, &c));
            CHECK((const void *)c1 == (const void *)c && c1 != NULL);
            if (c1) { CHECK(c1->glyphRun.fontFace == x.face); CHECK(c1->measuringMode == DWRITE_MEASURING_MODE_NATURAL); }
            k++;
        }
        EXPECT_EQ(k, 1 + 2);
        IDWriteColorGlyphRunEnumerator1_Release(e1);
    }
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_movenext_through_either_interface_shares_state) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_C; IDWriteColorGlyphRunEnumerator *e = mk(&x, &g, 1, 0, DWRITE_MEASURING_MODE_NATURAL, 0, 0, 1, FALSE, 0, NULL);
    if (!e) { CHECKF(0, "no enumerator"); cg_close(&x); return; }
    IDWriteColorGlyphRunEnumerator1 *e1 = NULL; BOOL has; int k = 0;
    IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1);
    if (e1) {
        while (k < 10) {
            HRESULT hr = (k & 1) ? IDWriteColorGlyphRunEnumerator1_MoveNext(e1, &has) : IDWriteColorGlyphRunEnumerator_MoveNext(e, &has);
            CHECK_HR_OK(hr); if (!has) break; k++;
        }
        EXPECT_EQ(k, 4);
        IDWriteColorGlyphRunEnumerator1_Release(e1);
    }
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_query_interface_matrix) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; NEED_E(e, x, &g, 1, 0);
    dw_qi_row rows[] = {
        DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteColorGlyphRunEnumerator, 1), DW_QI_ROW(IDWriteColorGlyphRunEnumerator1, 1),
        DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFactory2, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteTextRenderer, 0),
        DW_QI_ROW(IDWritePixelSnapping, 0), DW_QI_ROW(IDWriteGlyphRunAnalysis, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0), DW_QI_ROW(IDWriteFontFile, 0),
    };
    DW_CHECK_QI(e, rows);
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_query_interface_failure_zeroes_out) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; NEED_E(e, x, &g, 1, 0);
    void *p = (void *)(ULONG_PTR)0x99; CHECK_HR(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteFactory, &p), CG_E_NOINTERFACE); CHECK_OUT_NULL(p);
    p = (void *)(ULONG_PTR)0x99; CHECK_HR(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteFontFace2, &p), CG_E_NOINTERFACE); CHECK_OUT_NULL(p);
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_enumerator1_is_same_object) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; NEED_E(e, x, &g, 1, 0);
    IDWriteColorGlyphRunEnumerator1 *e1 = NULL; IUnknown *u = NULL;
    CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1));
    CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IUnknown, (void **)&u));
    CHECK((void *)e1 == (void *)e); CHECK((void *)u == (void *)e);
    DW_RELEASE(e1); DW_RELEASE(u);
    DW_RELEASE(e); cg_close(&x);
}
TEST(en_addref_release_counting) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_A; NEED_E(e, x, &g, 1, 0);
    EXPECT_REFS(e, 1);
    ULONG a = IDWriteColorGlyphRunEnumerator_AddRef(e); EXPECT_EQ(a, 2);
    a = IDWriteColorGlyphRunEnumerator_AddRef(e); EXPECT_EQ(a, 3);
    IDWriteColorGlyphRunEnumerator1 *e1 = NULL; CHECK_HR_OK(IDWriteColorGlyphRunEnumerator_QueryInterface(e, &IID_IDWriteColorGlyphRunEnumerator1, (void **)&e1));
    EXPECT_REFS(e, 4);
    a = IDWriteColorGlyphRunEnumerator1_Release(e1); EXPECT_EQ(a, 3);
    a = IDWriteColorGlyphRunEnumerator_Release(e); EXPECT_EQ(a, 2);
    a = IDWriteColorGlyphRunEnumerator_Release(e); EXPECT_EQ(a, 1);
    a = IDWriteColorGlyphRunEnumerator_Release(e); EXPECT_EQ(a, 0);
    cg_close(&x);
}
TEST(en_enumerator_survives_extra_reference_after_release) {
    CG_OPEN(x, CGFX_COLR); UINT16 g = GID_a; NEED_E(e, x, &g, 1, 0);
    IDWriteColorGlyphRunEnumerator_AddRef(e); BOOL has; int k = 0;
    IDWriteColorGlyphRunEnumerator_Release(e);
    while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has) k++;
    EXPECT_EQ(k, 5);
    IDWriteColorGlyphRunEnumerator_Release(e); cg_close(&x);
}
TEST(en_struct_layout) {
    EXPECT_EQ(sizeof(DWRITE_GLYPH_RUN), 48); EXPECT_EQ(offsetof(DWRITE_GLYPH_RUN, glyphIndices), 16); EXPECT_EQ(offsetof(DWRITE_GLYPH_RUN, glyphOffsets), 32); EXPECT_EQ(offsetof(DWRITE_GLYPH_RUN, bidiLevel), 44);
    EXPECT_EQ(offsetof(DWRITE_COLOR_GLYPH_RUN, glyphRunDescription), 48); EXPECT_EQ(offsetof(DWRITE_COLOR_GLYPH_RUN, baselineOriginX), 56);
    EXPECT_EQ(offsetof(DWRITE_COLOR_GLYPH_RUN, runColor), 64); EXPECT_EQ(offsetof(DWRITE_COLOR_GLYPH_RUN, paletteIndex), 80);
    EXPECT_EQ(offsetof(DWRITE_COLOR_GLYPH_RUN1, glyphImageFormat), 88); EXPECT_EQ(offsetof(DWRITE_COLOR_GLYPH_RUN1, measuringMode), 92); EXPECT_EQ(sizeof(DWRITE_COLOR_GLYPH_RUN1), 96);
    EXPECT_EQ(sizeof(DWRITE_COLOR_F), 16);
}
typedef struct thr { int id, iters; volatile LONG bad; cgfont *x; } thr;
static DWORD WINAPI thread_main(LPVOID p) {
    thr *t = (thr *)p; int i; UINT16 g[] = { GID_A, GID_F, GID_C }; IDWriteFactory2 *f = cg_f2();
    for (i = 0; i < t->iters; i++) {
        static const int expected = 1 + 2 + 3; cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; BOOL has; int k = 0;
        cg_mkrun(&r, t->x->face, 20.0f, g, 3, 1, 0, FALSE, 0);
        if (FAILED(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, i & 1, &e)) || !e) { InterlockedIncrement(&t->bad); continue; }
        while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(e, &has)) && has && k < 20) k++;
        if (k != expected) InterlockedIncrement(&t->bad);
        IDWriteColorGlyphRunEnumerator_Release(e);
    }
    return 0;
}
TEST(en_concurrent_use_from_threads) {
    CG_NEED_F2(f); CG_OPEN(x, CGFX_COLR); thr t[4]; HANDLE h[4]; int i;
    for (i = 0; i < 4; i++) { t[i].id = i; t[i].iters = 150; t[i].bad = 0; t[i].x = &x; h[i] = CreateThread(NULL, 0, thread_main, &t[i], 0, NULL); }
    for (i = 0; i < 4; i++) if (h[i]) { WaitForSingleObject(h[i], 20000); CloseHandle(h[i]); }
    for (i = 0; i < 4; i++) CHECKF(t[i].bad == 0, "thread %d: %ld bad iterations", i, t[i].bad);
    cg_close(&x);
}

TEST(en_layer_run_advance_note) {
    CG_OPEN(x, CGFX_COLR); snap s[40]; UINT16 g[] = { GID_A, GID_B }; static cgrun r; IDWriteColorGlyphRunEnumerator *e = NULL; int k, i;
    cg_mkrun(&r, x.face, 2048.0f, g, 2, 2, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(cg_f2(), 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e));
    if (e) {
        k = collect(e, s, 40);
        for (i = 0; i < k; i++) { t_note("run %d: glyph %u advance[0] %g (input advances %g %g)", i, s[i].gl[0], s[i].adv[0], r.adv[0], r.adv[1]); CHECK(s[i].has_adv); }
        DW_RELEASE(e);
    }
    cg_close(&x);
}
