/* Lifetime: who holds what after CreateCustomFontCollection has returned.
 *
 * Win10 decomp: the collection stores only the loader's numeric registration id (never the loader pointer) and the
 * enumeration is finished before the call returns; the registration map owns one reference on the loader until
 * Unregister or factory destruction.  Destroyed test objects here are replaced by a tripwire vtable (r2_uaf).
 */
#include "r2.h"

static void make_face(IDWriteFontCollection *c, UINT32 fam, IDWriteFontFace **face)
{
    IDWriteFontFamily *f = NULL; IDWriteFont *font = NULL;
    *face = NULL;
    if (c && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, fam, &f)) && f && SUCCEEDED(IDWriteFontFamily_GetFont(f, 0, &font)) && font) IDWriteFont_CreateFontFace(font, face);
    DW_RELEASE(font); DW_RELEASE(f);
}

RT(loader_reference_stages)
{
    FX(x);
    r2_add_family(&x, "Stages");
    EXPECT_EQ(x.cl->ref, 2);                                  /* ours + registration */
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(x.cl->ref, 2);                                  /* the collection does not retain the loader */
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl))); x.reg_cl = 0;
    EXPECT_EQ(x.cl->ref, 1);
    DW_RELEASE(c);
    EXPECT_EQ(x.cl->ref, 1);
    r2_close(&x);
    EXPECT_EQ(x.cl->destroyed, 1);
}
RT(loader_is_destroyed_exactly_once)
{
    FX(x);
    r2_add_family(&x, "Once");
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    DW_RELEASE(c);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl))); x.reg_cl = 0;
    EXPECT_EQ(x.cl->destroyed, 0);
    r2_obj_release(x.cl);
    EXPECT_EQ(x.cl->destroyed, 1);
    EXPECT_EQ(r2_count(EV_CL_DESTROY), 1);
    r2_fl_unregister(x.fl); r2_obj_release(x.fl); DW_RELEASE(x.f); x.ok = 0;
}
RT(collection_works_after_the_loader_is_destroyed)
{
    FX(x);
    r2_add_family(&x, "Orphan A"); r2_add_family(&x, "Orphan B");
    IDWriteFontCollection *c = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl))); x.reg_cl = 0;
    r2_obj_release(x.cl);                                     /* last reference: destroyed, vtable now a tripwire */
    CHECK(x.cl->destroyed);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
        CHECK(r2_find_family(c, "Orphan A") >= 0);
        CHECK(r2_find_family(c, "Orphan B") >= 0);
        make_face(c, 0, &face);
        CHECK(face != NULL);
        if (face) CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS);
    }
    EXPECT_EQ(r2_uaf, 0);                                     /* nothing called into the destroyed loader */
    DW_RELEASE(face); DW_RELEASE(c);
    r2_fl_unregister(x.fl); r2_obj_release(x.fl); DW_RELEASE(x.f); x.ok = 0;
}
RT(collection_works_after_the_file_loader_is_unregistered_listing_only)
{
    FX(x);
    r2_add_family(&x, "FileGone");
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    r2_fl_unregister(x.fl); x.reg_fl = 0;
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        char nm[40]; CHECK(r2_family_name(c, 0, nm, sizeof nm)); EXPECT_STR(nm, "FileGone");
        CHECK(r2_find_family(c, "FileGone") == 0);
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(loader_references_return_to_base_when_the_factory_dies_with_a_live_collection)
{
    FX(x);
    r2_add_family(&x, "Dies");
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    LONG with_reg = x.cl->ref;
    EXPECT_EQ(with_reg, 2);
    /* release the application's last reference to the factory without unregistering: the collection may keep the
     * factory alive, in which case the registration (and its loader reference) stays until the collection is released */
    IDWriteFactory *f = x.f; x.f = NULL;
    ULONG left = IDWriteFactory_Release(f);
    t_note("factory refs after the application released it with a live collection: %lu; loader refs %ld", left, (long)x.cl->ref);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    DW_RELEASE(c);
    t_note("loader refs after the collection was released too: %ld", (long)x.cl->ref);
    EXPECT_EQ(x.cl->ref, 1);
    x.reg_cl = 0; x.reg_fl = 0; x.fl->registered_on = NULL; x.cl->registered_on = NULL;
    r2_obj_release(x.cl); r2_obj_release(x.fl);
    x.ok = 0;
}
RT(fifty_create_release_cycles_do_not_accumulate_references)
{
    FX(x);
    int i;
    r2_add_family(&x, "Cycle A"); r2_add_family(&x, "Cycle B");
    for (i = 0; i < 50; i++) {
        IDWriteFontCollection *c = NULL;
        CHECK_HR(r2_create(&x, &c), S_OK);
        DW_RELEASE(c);
    }
    EXPECT_EQ(x.cl->ref, 2);
    EXPECT_EQ(x.cl->n_create, 50);
    EXPECT_EQ(x.cl->nens, 50);
    int destroyed = 0;
    for (i = 0; i < 50 && i < 64; i++) if (x.cl->ens[i] && x.cl->ens[i]->destroyed) destroyed++;
    EXPECT_EQ(destroyed, 50);
    r2_close(&x);
}
RT(fifty_cycles_leave_no_stream_behind_after_the_factory_is_released)
{
    FX(x);
    int i;
    r2_add_family(&x, "Leak");
    for (i = 0; i < 50; i++) { IDWriteFontCollection *c = NULL; CHECK_HR(r2_create(&x, &c), S_OK); DW_RELEASE(c); }
    t_note("%ld streams created, %d alive before the factory is released", (long)x.fl->nstreams, r2_fl_streams_alive(x.fl));
    r2_close(&x);
    EXPECT_EQ(r2_fl_streams_alive(x.fl), 0);
    EXPECT_EQ(r2_fl_outstanding(x.fl), 0);
}
RT(live_collections_hold_what_they_need_and_nothing_more)
{
    FX(x);
    r2_add_family(&x, "Hold");
    IDWriteFontCollection *c[5] = { 0 }; int i;
    for (i = 0; i < 5; i++) CHECK_HR(r2_create(&x, &c[i]), S_OK);
    EXPECT_EQ(x.cl->ref, 2);
    for (i = 0; i < 5; i++) { if (c[i]) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c[i]), 1); }
    for (i = 4; i >= 0; i--) DW_RELEASE(c[i]);
    EXPECT_EQ(x.cl->ref, 2);
    r2_close(&x);
}
RT(face_keeps_working_after_collection_family_and_font_are_released)
{
    FX(x);
    r2_add_family(&x, "FaceOnly");
    IDWriteFontCollection *c = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    make_face(c, 0, &face);
    DW_RELEASE(c);
    if (face) {
        UINT16 gi = 0; UINT32 cp = 'A';
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gi));
        EXPECT_EQ(gi, ttf_default_gid('A'));
        DWRITE_GLYPH_METRICS gm; memset(&gm, 0, sizeof gm);
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, &gi, 1, &gm, FALSE));
        EXPECT_EQ(gm.advanceWidth, TTF_DEF_ADV);
    }
    DW_RELEASE(face);
    r2_close(&x);
}
RT(layout_from_a_custom_family_measures_with_the_synthetic_advances)
{
    FX(x);
    r2_add_family(&x, "LayoutFam");
    IDWriteFontCollection *c = NULL; IDWriteTextFormat *tf = NULL; IDWriteTextLayout *tl = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c && SUCCEEDED(IDWriteFactory_CreateTextFormat(x.f, L"LayoutFam", c, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &tf)) && tf &&
        SUCCEEDED(IDWriteFactory_CreateTextLayout(x.f, L"AAAA", 4, tf, 1000.0f, 100.0f, &tl)) && tl) {
        DWRITE_TEXT_METRICS tm; memset(&tm, 0, sizeof tm);
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(tl, &tm));
        CHECK_NEAR(tm.widthIncludingTrailingWhitespace, 4 * DW_DU(TTF_DEF_ADV, TTF_DEF_UPM, 20.0f), 0.01);
    } else t_skip("no layout");
    DW_RELEASE(tl); DW_RELEASE(tf); DW_RELEASE(c);
    r2_close(&x);
}
RT(layout_keeps_the_family_after_both_loaders_are_unregistered)
{
    FX(x);
    r2_add_family(&x, "LayoutKeep");
    IDWriteFontCollection *c = NULL; IDWriteTextFormat *tf = NULL; IDWriteTextLayout *tl = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(x.cl))); x.reg_cl = 0;
    if (c && SUCCEEDED(IDWriteFactory_CreateTextFormat(x.f, L"LayoutKeep", c, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &tf)) && tf &&
        SUCCEEDED(IDWriteFactory_CreateTextLayout(x.f, L"AAAA", 4, tf, 1000.0f, 100.0f, &tl)) && tl) {
        DWRITE_TEXT_METRICS tm; memset(&tm, 0, sizeof tm);
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(tl, &tm));
        CHECK_NEAR(tm.widthIncludingTrailingWhitespace, 4 * DW_DU(TTF_DEF_ADV, TTF_DEF_UPM, 20.0f), 0.01);
    } else t_skip("no layout");
    DW_RELEASE(tl); DW_RELEASE(tf); DW_RELEASE(c);
    r2_close(&x);
}
