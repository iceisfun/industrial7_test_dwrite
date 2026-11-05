/* GetGlyphRunOutline on faces obtained in different ways and on fonts with other unitsPerEm / hmtx layouts. */
#include "fo.h"

static dwx_geom *run1(IDWriteFontFace *face, float em, UINT16 gid) {
    dwx_geom *s = dwx_geom_new();
    CHECK_HR(fo_call(face, em, &gid, NULL, NULL, 1, FALSE, FALSE, s), S_OK);
    return s;
}

/* ---- unitsPerEm variations --------------------------------------------------------------- */
static void upm_case(int upm, float em) {
    dwx_font h; UINT16 g[3] = { 2, TTF_GID_ROUND, FO_G_ALLOFF4 }; fo_exp e; dwx_geom *s; char tag[64];
    fo_lab_upm = upm; HRESULT hr = fo_open_lab(&h, 0); fo_lab_upm = 0;
    if (FAILED(hr) || !h.face) { dwx_font_close(&h); fo_upm = TTF_DEF_UPM; t_skip("font with upm %d rejected: %s", upm, dw_hr_name(hr)); return; }
    DWRITE_FONT_METRICS fm; IDWriteFontFace_GetMetrics(h.face, &fm);
    CHECK(fm.designUnitsPerEm == upm);
    s = dwx_geom_new();
    CHECK_HR(fo_call(h.face, em, g, NULL, NULL, 3, FALSE, FALSE, s), S_OK);
    fo_model_run(&e, fo_glyphs, FO_G_COUNT, h.face, em, g, NULL, NULL, 3, 0, 0, 0);
    snprintf(tag, sizeof tag, "upm %d em %g", upm, em);
    FO_COMPARE(s, &e, 1e-3 + em * 4e-6 * (2048.0 / upm > 1 ? 2048.0 / upm : 1), tag);
    FO_INVARIANTS(s, 1);
    dwx_release(s); dwx_font_close(&h); fo_upm = TTF_DEF_UPM;
}
TEST(upm_1000) { upm_case(1000, 1000.0f); upm_case(1000, 24.0f); }
TEST(upm_512) { upm_case(512, 512.0f); upm_case(512, 100.0f); }
TEST(upm_4096) { upm_case(4096, 4096.0f); upm_case(4096, 13.0f); }
TEST(upm_16384) { upm_case(16384, 16384.0f); upm_case(16384, 200.0f); }
TEST(upm_64) { upm_case(64, 64.0f); upm_case(64, 1000.0f); }
TEST(upm_1024_exact_unit_scale) {
    dwx_font h; fo_lab_upm = 1024; HRESULT hr = fo_open_lab(&h, 0); fo_lab_upm = 0;
    if (FAILED(hr) || !h.face) { dwx_font_close(&h); fo_upm = TTF_DEF_UPM; t_skip("font rejected"); return; }
    dwx_geom *s = run1(h.face, 1024.0f, 2);      /* em == upm: coordinates are design units */
    static const float want[8] = { 100, 0, 100, -1400, 900, -1400, 900, 0 }; int i;
    CHECK(s->npts == 4);
    for (i = 0; i < 4 && s->npts == 4; i++) { CHECK_FLOAT_EQ(s->pts[i].x, want[2 * i]); CHECK_FLOAT_EQ(s->pts[i].y, want[2 * i + 1]); }
    dwx_release(s); dwx_font_close(&h); fo_upm = TTF_DEF_UPM;
}

/* ---- hmtx with numberOfHMetrics < numGlyphs -------------------------------------------------- */
TEST(hmtx_compressed_trailing_advances) {
    dwx_font h; fo_lab_compress = 1; HRESULT hr = fo_open_lab(&h, 0); fo_lab_compress = 0;
    if (FAILED(hr) || !h.face) { dwx_font_close(&h); t_skip("lab font unavailable"); return; }
    /* glyph 87 (FO_G_EMPTYCONT) shares the final long-metric advance: the default pen advance is still 1000 */
    UINT16 g[3] = { FO_G_EMPTYCONT, FO_G_EMPTYCONT, 2 }; dwx_geom *s = dwx_geom_new(); float x, y;
    CHECK_HR(fo_call(h.face, 2048.0f, g, NULL, NULL, 3, FALSE, FALSE, s), S_OK);
    CHECK(fo_begin_pt(s, 1, &x, &y) && x == 2100 && y == 0);
    DWRITE_GLYPH_METRICS m; UINT16 gi = FO_G_EMPTYCONT; memset(&m, 0, sizeof m);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(h.face, &gi, 1, &m, FALSE), S_OK);
    CHECK(m.advanceWidth == 1000);
    dwx_release(s); dwx_font_close(&h);
}

/* ---- face created through a custom collection -------------------------------------------------- */
TEST(face_from_custom_collection_equals_direct_face) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no factory"); return; }
    ttf_spec sp; size_t n; uint8_t *b; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll = NULL; UINT32 k;
    fo_lab_spec(&sp); ttf_set_names(&sp, "FoOutline", "Regular", "FoOutline Regular", "FoOutline-Regular");
    b = ttf_build(&sp, &n);
    fl = dwx_loader_new(); k = dwx_loader_add(fl, b, n);
    dwx_loader_register(fl, f);
    cl = dwx_coll_new(fl); dwx_coll_add_key(cl, k); dwx_coll_register(cl, f);
    HRESULT hr = dwx_coll_create(cl, f, &coll);
    if (SUCCEEDED(hr) && coll && IDWriteFontCollection_GetFontFamilyCount(coll) >= 1) {
        IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL, *direct = NULL;
        CHECK_HR(IDWriteFontCollection_GetFontFamily(coll, 0, &fam), S_OK);
        if (fam && SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) && font && SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
            dwx_font h; memset(&h, 0, sizeof h);
            if (SUCCEEDED(dwx_font_open(&h, f, b, n, 0, 0)) && h.face) {
                UINT16 g[5] = { 2, 65, 68, FO_G_OFFSTART, FO_G_MULTI };
                dwx_geom *a = dwx_geom_new(), *c = dwx_geom_new();
                CHECK_HR(fo_call(face, 61.0f, g, NULL, NULL, 5, FALSE, FALSE, a), S_OK);
                CHECK_HR(fo_call(h.face, 61.0f, g, NULL, NULL, 5, FALSE, FALSE, c), S_OK);
                CHECK(a->npts == c->npts && a->npts > 0 && !memcmp(a->pts, c->pts, sizeof(D2D1_POINT_2F) * a->npts));
                CHECK(a->nev == c->nev);
                dwx_release(a); dwx_release(c);
                (void)direct;
            }
            dwx_font_close(&h);
        } else t_skip("could not create a face from the custom collection");
        DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam);
    } else t_skip("custom collection unavailable: %s", dw_hr_name(hr));
    DW_RELEASE(coll);
    dwx_coll_unregister(cl); dwx_loader_unregister(fl);
    dwx_release(cl); dwx_release(fl); free(b);
}

/* ---- TrueType collection: face index selects the outlines ----------------------------------------- */
TEST(ttc_face_index_selects_font) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no factory"); return; }
    ttf_spec a, bsp; ttf_glyph g2[TTF_DEF_NGLYPHS]; int n, i; const ttf_glyph *d = ttf_default_glyphs(&n);
    for (i = 0; i < n; i++) g2[i] = d[i];
    g2[2] = g2[TTF_GID_TRIANGLE];                       /* font B draws 'A' as a triangle */
    ttf_style_spec(&a, "TtcOne", 400, 5, 0); ttf_style_spec(&bsp, "TtcTwo", 400, 5, 0); bsp.glyphs = g2; bsp.nglyphs = n;
    size_t na, nb, nc; uint8_t *ba = ttf_build(&a, &na), *bb = ttf_build(&bsp, &nb);
    const uint8_t *blobs[2] = { ba, bb }; size_t sizes[2] = { na, nb }; uint8_t *c = ttf_build_ttc(blobs, sizes, 2, &nc);
    dwx_font h0, h1; HRESULT hr0 = dwx_font_open(&h0, f, c, nc, 0, 0), hr1 = dwx_font_open(&h1, f, c, nc, 1, 0);
    if (FAILED(hr0) || FAILED(hr1) || !h0.face || !h1.face) t_skip("TTC faces unavailable (%s, %s)", dw_hr_name(hr0), dw_hr_name(hr1));
    else {
        CHECK(h0.nfaces == 2);
        dwx_geom *s0 = run1(h0.face, 2048.0f, 2), *s1 = run1(h1.face, 2048.0f, 2);
        static const struct { float x, y; } box[4] = { {100, 0}, {100, -1400}, {900, -1400}, {900, 0} }, tri[3] = { {100, 0}, {500, -1400}, {900, 0} };
        CHECK(s0->npts == 4 && s1->npts == 3);
        for (i = 0; i < s0->npts && s0->npts == 4; i++) { CHECK_FLOAT_EQ(s0->pts[i].x, box[i].x); CHECK_FLOAT_EQ(s0->pts[i].y, box[i].y); }
        for (i = 0; i < s1->npts && s1->npts == 3; i++) { CHECK_FLOAT_EQ(s1->pts[i].x, tri[i].x); CHECK_FLOAT_EQ(s1->pts[i].y, tri[i].y); }
        dwx_release(s0); dwx_release(s1);
    }
    dwx_font_close(&h0); dwx_font_close(&h1);
    free(ba); free(bb); free(c);
}

/* ---- two independent faces, independent caches ----------------------------------------------------- */
TEST(faces_with_same_glyph_ids_different_fonts_do_not_share_cache) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no factory"); return; }
    ttf_spec a, bsp; ttf_glyph g2[TTF_DEF_NGLYPHS]; int n, i; const ttf_glyph *d = ttf_default_glyphs(&n);
    for (i = 0; i < n; i++) g2[i] = d[i];
    g2[2] = g2[TTF_GID_RING];
    ttf_style_spec(&a, "CacheOne", 400, 5, 0); ttf_style_spec(&bsp, "CacheTwo", 400, 5, 0); bsp.glyphs = g2; bsp.nglyphs = n;
    size_t na, nb; uint8_t *ba = ttf_build(&a, &na), *bb = ttf_build(&bsp, &nb);
    dwx_font h0, h1; HRESULT hr0 = dwx_font_open(&h0, f, ba, na, 0, 0), hr1 = dwx_font_open(&h1, f, bb, nb, 0, 0);
    if (FAILED(hr0) || FAILED(hr1) || !h0.face || !h1.face) t_skip("faces unavailable");
    else {
        for (i = 0; i < 3; i++) {     /* alternate so a shared cache entry would show up */
            dwx_geom *s0 = run1(h0.face, 2048.0f, 2), *s1 = run1(h1.face, 2048.0f, 2);
            CHECK(s0->nfigures_begun == 1); CHECK(s1->nfigures_begun == 2);
            dwx_release(s0); dwx_release(s1);
        }
    }
    dwx_font_close(&h0); dwx_font_close(&h1); free(ba); free(bb);
}
TEST(face_released_and_recreated_same_result) {
    FO_LAB(h, 0);
    UINT16 g[2] = { 65, FO_G_MULTI }; dwx_geom *a = dwx_geom_new(), *b = dwx_geom_new();
    CHECK_HR(fo_call(h.face, 50.0f, g, NULL, NULL, 2, FALSE, FALSE, a), S_OK);
    dwx_font_close(&h);
    FO_LAB(h2, 0);
    CHECK_HR(fo_call(h2.face, 50.0f, g, NULL, NULL, 2, FALSE, FALSE, b), S_OK);
    CHECK(a->npts == b->npts && a->npts > 0 && !memcmp(a->pts, b->pts, sizeof(D2D1_POINT_2F) * a->npts));
    dwx_release(a); dwx_release(b); dwx_font_close(&h2);
}
