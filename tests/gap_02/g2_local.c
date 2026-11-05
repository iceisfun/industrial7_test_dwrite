/* IDWriteFontFace3::IsGlyphLocal.
 * Win10 probing: IDWriteFontFace::IsGlyphLocal fetches the factory's font download queue WITHOUT creating
 * it (an internal accessor with create = false) and asks the face: when the face has no remote file
 * stream (its remote stream list is empty) the answer is TRUE for every glyph index, valid or not; only faces over remote
 * (downloadable) streams consult the glyph locality. Returns a C++ bool widened to BOOL (0 or 1). */
#include "g2.h"

#define NEED_FACE3(x) g2_face x; if (!g2_face_open(&x, NULL)) return; \
    if (!x.face3) { g2_face_close(&x); t_skip("IDWriteFontFace3 unsupported"); return; }

TEST(gap2_igl_every_defined_glyph_true)
{
    NEED_FACE3(x);
    UINT16 g; int bad = 0;
    for (g = 0; g < TTF_DEF_NGLYPHS; g++) if (IDWriteFontFace3_IsGlyphLocal(x.face3, g) != TRUE) bad++;
    CHECKF(bad == 0, "%d defined glyphs reported non-local", bad);
    g2_face_close(&x);
}

TEST(gap2_igl_out_of_range_glyph_ids_true)
{
    /* No range validation on a local face. */
    NEED_FACE3(x);
    static const UINT16 ids[] = { 69, 70, 100, 255, 256, 1000, 4096, 0x7FFE, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF };
    size_t i;
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++) { BOOL r = IDWriteFontFace3_IsGlyphLocal(x.face3, ids[i]); CHECKF(r == TRUE, "glyph 0x%x -> %d", ids[i], (int)r); }
    g2_face_close(&x);
}

TEST(gap2_igl_result_is_exactly_boolean)
{
    NEED_FACE3(x);
    UINT16 ids[] = { 0, 2, 0xFFFF }; int i;
    for (i = 0; i < 3; i++) { BOOL r = IDWriteFontFace3_IsGlyphLocal(x.face3, ids[i]); CHECKF(r == 0 || r == 1, "raw value %d", (int)r); EXPECT_EQ(r, 1); }
    g2_face_close(&x);
}

TEST(gap2_igl_repeat_stable_and_cheap)
{
    NEED_FACE3(x);
    int i, bad = 0;
    for (i = 0; i < 5000; i++) if (!IDWriteFontFace3_IsGlyphLocal(x.face3, (UINT16)(i % 69))) bad++;
    CHECKF(bad == 0, "%d failures", bad);
    g2_face_close(&x);
}

TEST(gap2_igl_does_not_create_download_queue_entries)
{
    /* The queue is fetched without being created and nothing is enqueued: it stays empty. */
    NEED_FACE3(x);
    IDWriteFactory3 *f3 = NULL; IDWriteFontDownloadQueue *q = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(x.f, &IID_IDWriteFactory3, (void **)&f3))) {
        UINT16 g;
        for (g = 0; g < TTF_DEF_NGLYPHS; g++) IDWriteFontFace3_IsGlyphLocal(x.face3, g);
        CHECK_HR_OK(IDWriteFactory3_GetFontDownloadQueue(f3, &q));
        if (q) { CHECK(IDWriteFontDownloadQueue_IsEmpty(q) == TRUE); DW_RELEASE(q); }
        DW_RELEASE(f3);
    }
    g2_face_close(&x);
}

TEST(gap2_igl_refcounts_balanced)
{
    NEED_FACE3(x);
    ULONG a = dw_refcount((IUnknown *)x.face3), b, fa = dw_refcount((IUnknown *)x.f), fb;
    int i;
    for (i = 0; i < 20; i++) IDWriteFontFace3_IsGlyphLocal(x.face3, 2);
    b = dw_refcount((IUnknown *)x.face3); fb = dw_refcount((IUnknown *)x.f);
    CHECKF(a == b, "face %lu -> %lu", a, b);
    CHECKF(fa == fb, "factory %lu -> %lu", fa, fb);
    g2_face_close(&x);
}

TEST(gap2_igl_same_through_all_interface_pointers)
{
    NEED_FACE3(x);
    IDWriteFontFace1 *f1 = NULL; IDWriteFontFace4 *f4 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(x.face, &IID_IDWriteFontFace4, (void **)&f4))) {
        IDWriteFontFace3 *via = NULL;
        CHECK_HR_OK(IDWriteFontFace4_QueryInterface(f4, &IID_IDWriteFontFace3, (void **)&via));
        CHECK(via == x.face3);
        CHECK(IDWriteFontFace3_IsGlyphLocal(via, 3) == TRUE);
        DW_RELEASE(via); DW_RELEASE(f4);
    }
    (void)f1;
    g2_face_close(&x);
}

TEST(gap2_igl_agrees_with_are_glyphs_local)
{
    NEED_FACE3(x);
    UINT16 ids[TTF_DEF_NGLYPHS]; int i, enq;
    for (i = 0; i < TTF_DEF_NGLYPHS; i++) ids[i] = (UINT16)i;
    for (enq = 0; enq < 2; enq++) {
        BOOL all = FALSE;
        CHECK_HR_OK(IDWriteFontFace3_AreGlyphsLocal(x.face3, ids, TTF_DEF_NGLYPHS, enq, &all));
        CHECKF(all == TRUE, "enqueue %d", enq);
    }
    g2_face_close(&x);
}

TEST(gap2_igl_simulated_faces_local)
{
    g2_face x; uint8_t *b; size_t n; dwx_font h; static const DWRITE_FONT_SIMULATIONS sims[] = { DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE, DWRITE_FONT_SIMULATIONS_BOLD | DWRITE_FONT_SIMULATIONS_OBLIQUE };
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); size_t i;
    (void)x;
    if (!f) { t_skip("factory"); return; }
    b = ttf_build_default(&n);
    for (i = 0; i < 3; i++) {
        IDWriteFontFace3 *f3 = NULL;
        if (FAILED(dwx_font_open(&h, f, b, n, 0, sims[i])) || !h.face) { dwx_font_close(&h); continue; }
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace3, (void **)&f3))) {
            CHECKF(IDWriteFontFace3_IsGlyphLocal(f3, 2) == TRUE && IDWriteFontFace3_IsGlyphLocal(f3, 0) == TRUE, "simulations 0x%x", sims[i]);
            DW_RELEASE(f3);
        }
        dwx_font_close(&h);
    }
    free(b); DW_RELEASE(f);
}

TEST(gap2_igl_ttc_second_face)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); ttf_spec s0, s1; size_t n0, n1, nt; uint8_t *b0, *b1, *ttc; dwx_font h; IDWriteFontFace3 *f3 = NULL;
    const uint8_t *bl[2]; size_t sz[2];
    if (!f) { t_skip("factory"); return; }
    g2_spec(&s0, "G2TtcA"); g2_spec(&s1, "G2TtcB");
    b0 = ttf_build(&s0, &n0); b1 = ttf_build(&s1, &n1); bl[0] = b0; bl[1] = b1; sz[0] = n0; sz[1] = n1;
    ttc = ttf_build_ttc(bl, sz, 2, &nt);
    if (ttc && SUCCEEDED(dwx_font_open(&h, f, ttc, nt, 1, DWRITE_FONT_SIMULATIONS_NONE)) && h.face) {
        EXPECT_EQ(IDWriteFontFace_GetIndex(h.face), 1);
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace3, (void **)&f3))) { CHECK(IDWriteFontFace3_IsGlyphLocal(f3, 5) == TRUE); DW_RELEASE(f3); }
        dwx_font_close(&h);
    } else { dwx_font_close(&h); t_skip("ttc face did not open"); }
    free(b0); free(b1); free(ttc); DW_RELEASE(f);
}

TEST(gap2_igl_from_font_object_and_reference)
{
    g2_coll c; ttf_spec s; IDWriteFont *font; IDWriteFontFace *face = NULL; IDWriteFontFace3 *f3 = NULL, *f3b = NULL; IDWriteFontFaceReference *ref = NULL;
    g2_spec(&s, "G2Local");
    if (!g2_coll_open(&c, &s, 1)) return;
    font = g2_font(c.coll, L"G2Local", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (font) {
        IDWriteFont3 *font3 = NULL;
        if (SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3))) {
            CHECK_HR_OK(G2_F3_CREATEFACE(font3, &f3));
            if (f3) { CHECK(IDWriteFontFace3_IsGlyphLocal(f3, 2) == TRUE); CHECK(IDWriteFontFace3_IsGlyphLocal(f3, 0xFFFF) == TRUE);
                      if (SUCCEEDED(IDWriteFontFace3_GetFontFaceReference(f3, &ref)) && ref) {
                          if (SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(ref, &f3b)) && f3b) CHECK(IDWriteFontFace3_IsGlyphLocal(f3b, 7) == TRUE);
                      } }
            DW_RELEASE(font3);
        } else { IDWriteFont_CreateFontFace(font, &face); t_note("IDWriteFont3 unavailable"); }
    }
    DW_RELEASE(f3b); DW_RELEASE(ref); DW_RELEASE(f3); DW_RELEASE(face); DW_RELEASE(font);
    g2_coll_close(&c);
}

static void sys_local(const WCHAR *fam)
{
    IDWriteFontFace *face = dw_sysface(dw_shared(), fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace3 *f3 = NULL;
    UINT16 n, g, picks[8]; int i, bad = 0;
    if (!face) { t_skip("%ls not installed", fam); return; }
    if (FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3))) { DW_RELEASE(face); t_skip("no FontFace3"); return; }
    n = IDWriteFontFace_GetGlyphCount(face);
    picks[0] = 0; picks[1] = 1; picks[2] = 2; picks[3] = n / 2; picks[4] = n > 0 ? n - 1 : 0; picks[5] = n; picks[6] = 0x7FFF; picks[7] = 0xFFFF;
    for (i = 0; i < 8; i++) { g = picks[i]; if (IDWriteFontFace3_IsGlyphLocal(f3, g) != TRUE) bad++; }
    CHECKF(bad == 0, "%ls: %d non-local picks (glyph count %u)", fam, bad, n);
    DW_RELEASE(f3); DW_RELEASE(face);
}
TEST(gap2_igl_system_arial)          { sys_local(L"Arial"); }
TEST(gap2_igl_system_times)          { sys_local(L"Times New Roman"); }
TEST(gap2_igl_system_courier)        { sys_local(L"Courier New"); }
TEST(gap2_igl_system_segoe_ui)       { sys_local(L"Segoe UI"); }
TEST(gap2_igl_system_tahoma)         { sys_local(L"Tahoma"); }
TEST(gap2_igl_system_consolas)       { sys_local(L"Consolas"); }
TEST(gap2_igl_system_marlett)        { sys_local(L"Marlett"); }
TEST(gap2_igl_system_segoe_symbol)   { sys_local(L"Segoe UI Symbol"); }
TEST(gap2_igl_system_segoe_emoji)    { sys_local(L"Segoe UI Emoji"); }
TEST(gap2_igl_system_ms_gothic)      { sys_local(L"MS Gothic"); }
TEST(gap2_igl_system_bahnschrift)    { sys_local(L"Bahnschrift"); }

TEST(gap2_igl_isolated_and_shared_factories)
{
    int t;
    for (t = 0; t < 2; t++) {
        IDWriteFactory *f = t ? dw_shared() : dw_isolated(); IDWriteFontFace *face;
        face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (face) {
            IDWriteFontFace3 *f3 = NULL;
            if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3))) { CHECK(IDWriteFontFace3_IsGlyphLocal(f3, 36) == TRUE); DW_RELEASE(f3); }
            DW_RELEASE(face);
        }
    }
}
