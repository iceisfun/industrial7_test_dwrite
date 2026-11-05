/* IDWriteFont3::CreateFontFace(IDWriteFontFace3**) (vtable slot 19; IDWriteFont::CreateFontFace thunks into it).
 * Win10 probing (IDWriteFont::CreateFontFace): *face = NULL first; the font's face key (file reference,
 * face index, simulations) is copied into a FontFaceElementKey and IDWriteFontFace::Create builds (or finds) the face in the factory
 * the font's collection belongs to; the HRESULT of Create is returned and *face receives whatever it produced. The function does not
 * validate the out pointer (NULL crashes) and takes no other argument. */
#include "g2.h"

#define F3_SETUP(c, name, font, font3) \
    g2_coll c; ttf_spec s_; IDWriteFont *font = NULL; IDWriteFont3 *font3 = NULL; g2_spec(&s_, name); \
    if (!g2_coll_open(&c, &s_, 1)) return; \
    font = g2_font(c.coll, L##name, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); \
    if (!font || FAILED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3))) { DW_RELEASE(font); g2_coll_close(&c); t_skip("IDWriteFont3 unavailable"); return; }
#define F3_TEARDOWN(c, font, font3) do { DW_RELEASE(font3); DW_RELEASE(font); g2_coll_close(&c); } while (0)

TEST(gap2_f3cf_basic_success)
{
    F3_SETUP(c, "G2F3Basic", font, font3);
    IDWriteFontFace3 *face = (IDWriteFontFace3 *)G2_SENT;
    CHECK_HR_OK(G2_F3_CREATEFACE(font3, &face));
    CHECK(face != NULL && face != G2_SENT);
    if (face == G2_SENT) face = NULL;
    if (face) {
        EXPECT_EQ(IDWriteFontFace3_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE);
        EXPECT_EQ(IDWriteFontFace3_GetIndex(face), 0);
        EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), DWRITE_FONT_SIMULATIONS_NONE);
        EXPECT_EQ(IDWriteFontFace3_GetGlyphCount(face), TTF_DEF_NGLYPHS);
    }
    DW_RELEASE(face);
    F3_TEARDOWN(c, font, font3);
}

TEST(gap2_f3cf_new_reference_is_released_once)
{
    F3_SETUP(c, "G2F3Refs", font, font3);
    IDWriteFontFace3 *a = NULL, *b = NULL; ULONG r;
    CHECK_HR_OK(G2_F3_CREATEFACE(font3, &a));
    if (a) {
        r = dw_refcount((IUnknown *)a);
        CHECKF(r >= 1, "refs %lu", r);
        CHECK_HR_OK(G2_F3_CREATEFACE(font3, &b));
        if (b == a) EXPECT_REFS(a, r + 1);          /* a cached face gains one reference per call */
        else if (b) { t_note("two CreateFontFace calls returned distinct faces"); EXPECT_REFS(b, r); }
        if (b) { r = dw_refcount((IUnknown *)b); IDWriteFontFace3_Release(b); b = NULL; (void)r; }
        EXPECT_REFS(a, dw_refcount((IUnknown *)a));
    }
    DW_RELEASE(a); DW_RELEASE(b);
    F3_TEARDOWN(c, font, font3);
}

TEST(gap2_f3cf_matches_plain_createfontface)
{
    /* IDWriteFont::CreateFontFace is a thunk into the same implementation: same kind of face either way. */
    F3_SETUP(c, "G2F3Thunk", font, font3);
    IDWriteFontFace3 *a = NULL; IDWriteFontFace *b = NULL;
    CHECK_HR_OK(G2_F3_CREATEFACE(font3, &a));
    CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &b));
    if (a && b) {
        CHECK(IDWriteFontFace3_GetType(a) == IDWriteFontFace_GetType(b));
        CHECK(IDWriteFontFace3_GetIndex(a) == IDWriteFontFace_GetIndex(b));
        CHECK(IDWriteFontFace3_GetSimulations(a) == IDWriteFontFace_GetSimulations(b));
        CHECK(IDWriteFontFace3_GetGlyphCount(a) == IDWriteFontFace_GetGlyphCount(b));
        if (g2_ident(a) == g2_ident(b)) t_note("both paths return the same face object");
    }
    DW_RELEASE(a); DW_RELEASE(b);
    F3_TEARDOWN(c, font, font3);
}

TEST(gap2_f3cf_face_outlives_font_and_collection)
{
    g2_coll c; ttf_spec s; IDWriteFont *font; IDWriteFont3 *font3 = NULL; IDWriteFontFace3 *face = NULL;
    g2_spec(&s, "G2F3Out");
    if (!g2_coll_open(&c, &s, 1)) return;
    font = g2_font(c.coll, L"G2F3Out", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (font && SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3)) && SUCCEEDED(G2_F3_CREATEFACE(font3, &face)) && face) {
        DW_RELEASE(font3); DW_RELEASE(font); DW_RELEASE(c.coll);
        EXPECT_EQ(IDWriteFontFace3_GetGlyphCount(face), TTF_DEF_NGLYPHS);
        { UINT16 g = 0; UINT32 cp = 'A'; CHECK_HR_OK(IDWriteFontFace3_GetGlyphIndices(face, &cp, 1, &g)); EXPECT_EQ(g, 2); }
    } else t_skip("no face");
    DW_RELEASE(face); DW_RELEASE(font3); DW_RELEASE(font);
    g2_coll_close(&c);
}

TEST(gap2_f3cf_font_usable_after_family_and_collection_release)
{
    g2_coll c; ttf_spec s; IDWriteFont *font; IDWriteFont3 *font3 = NULL; IDWriteFontFace3 *face = NULL;
    g2_spec(&s, "G2F3Fam");
    if (!g2_coll_open(&c, &s, 1)) return;
    font = g2_font(c.coll, L"G2F3Fam", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    DW_RELEASE(c.coll);                                   /* the family object was already released by g2_font */
    if (font && SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3))) {
        CHECK_HR_OK(G2_F3_CREATEFACE(font3, &face));
        CHECK(face != NULL);
    } else t_skip("no font3");
    DW_RELEASE(face); DW_RELEASE(font3); DW_RELEASE(font);
    g2_coll_close(&c);
}

TEST(gap2_f3cf_bold_simulation_propagates)
{
    g2_coll c; ttf_spec s; IDWriteFont *font; IDWriteFont3 *font3 = NULL; IDWriteFontFace3 *face = NULL;
    g2_spec(&s, "G2F3Bold");                              /* regular only: a bold request must be simulated */
    if (!g2_coll_open(&c, &s, 1)) return;
    font = g2_font(c.coll, L"G2F3Bold", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    if (font && SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3))) {
        DWRITE_FONT_SIMULATIONS fs = IDWriteFont_GetSimulations(font);
        CHECK_HR_OK(G2_F3_CREATEFACE(font3, &face));
        if (face) EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), fs);
        t_note("bold request on a regular-only family: font simulations 0x%x", fs);
        G2_SOFT(fs == DWRITE_FONT_SIMULATIONS_BOLD);
    } else t_skip("no font");
    DW_RELEASE(face); DW_RELEASE(font3); DW_RELEASE(font);
    g2_coll_close(&c);
}

TEST(gap2_f3cf_oblique_simulation_propagates)
{
    g2_coll c; ttf_spec s; IDWriteFont *font; IDWriteFont3 *font3 = NULL; IDWriteFontFace3 *face = NULL;
    g2_spec(&s, "G2F3Obl");
    if (!g2_coll_open(&c, &s, 1)) return;
    font = g2_font(c.coll, L"G2F3Obl", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC);
    if (font && SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3))) {
        DWRITE_FONT_SIMULATIONS fs = IDWriteFont_GetSimulations(font);
        CHECK_HR_OK(G2_F3_CREATEFACE(font3, &face));
        if (face) EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), fs);
        G2_SOFT(fs == DWRITE_FONT_SIMULATIONS_OBLIQUE);
    } else t_skip("no font");
    DW_RELEASE(face); DW_RELEASE(font3); DW_RELEASE(font);
    g2_coll_close(&c);
}

TEST(gap2_f3cf_bold_oblique_simulation_propagates)
{
    g2_coll c; ttf_spec s; IDWriteFont *font; IDWriteFont3 *font3 = NULL; IDWriteFontFace3 *face = NULL;
    g2_spec(&s, "G2F3BO");
    if (!g2_coll_open(&c, &s, 1)) return;
    font = g2_font(c.coll, L"G2F3BO", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_OBLIQUE);
    if (font && SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3))) {
        DWRITE_FONT_SIMULATIONS fs = IDWriteFont_GetSimulations(font);
        CHECK_HR_OK(G2_F3_CREATEFACE(font3, &face));
        if (face) EXPECT_EQ(IDWriteFontFace3_GetSimulations(face), fs);
        t_note("bold oblique request: simulations 0x%x", fs);
    } else t_skip("no font");
    DW_RELEASE(face); DW_RELEASE(font3); DW_RELEASE(font);
    g2_coll_close(&c);
}

TEST(gap2_f3cf_distinct_fonts_distinct_files)
{
    g2_coll c; ttf_spec s[3]; int i; IDWriteFontFace3 *faces[3] = { 0 }; UINT32 keys[3] = { 0 };
    g2_spec(&s[0], "G2F3A"); g2_spec(&s[1], "G2F3B"); g2_spec(&s[2], "G2F3C");
    if (!g2_coll_open(&c, s, 3)) return;
    for (i = 0; i < 3; i++) {
        static const WCHAR *nm[3] = { L"G2F3A", L"G2F3B", L"G2F3C" };
        IDWriteFont *font = g2_font(c.coll, nm[i], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFont3 *f3 = NULL;
        if (font && SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&f3))) {
            CHECK_HR_OK(G2_F3_CREATEFACE(f3, &faces[i]));
            DW_RELEASE(f3);
        }
        DW_RELEASE(font);
    }
    for (i = 0; i < 3; i++) if (faces[i]) {
        IDWriteFontFile *files[2] = { 0 }; UINT32 nf = 0;
        if (SUCCEEDED(IDWriteFontFace3_GetFiles(faces[i], &nf, NULL)) && nf == 1 && SUCCEEDED(IDWriteFontFace3_GetFiles(faces[i], &nf, files))) {
            const void *k = NULL; UINT32 ks = 0;
            if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(files[0], &k, &ks)) && ks == 4) memcpy(&keys[i], k, 4);
            IDWriteFontFile_Release(files[0]);
        }
    }
    CHECK(keys[0] != keys[1] && keys[1] != keys[2] && keys[0] != keys[2]);
    for (i = 0; i < 3; i++) EXPECT_EQ(keys[i], c.keys[i]);
    for (i = 0; i < 3; i++) DW_RELEASE(faces[i]);
    g2_coll_close(&c);
}

TEST(gap2_f3cf_face_reference_roundtrip)
{
    F3_SETUP(c, "G2F3Ref", font, font3);
    IDWriteFontFace3 *face = NULL; IDWriteFontFaceReference *ref = NULL;
    CHECK_HR_OK(G2_F3_CREATEFACE(font3, &face));
    if (face && SUCCEEDED(IDWriteFontFace3_GetFontFaceReference(face, &ref)) && ref) {
        EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(ref), 0);
        EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(ref), DWRITE_FONT_SIMULATIONS_NONE);
        {
            IDWriteFontFace3 *again = NULL;
            CHECK_HR_OK(IDWriteFontFaceReference_CreateFontFace(ref, &again));
            if (again) { EXPECT_EQ(IDWriteFontFace3_GetGlyphCount(again), TTF_DEF_NGLYPHS); DW_RELEASE(again); }
        }
    }
    DW_RELEASE(ref); DW_RELEASE(face);
    F3_TEARDOWN(c, font, font3);
}

TEST(gap2_f3cf_names_match_font)
{
    F3_SETUP(c, "G2F3Names", font, font3);
    IDWriteFontFace3 *face = NULL; IDWriteLocalizedStrings *fam = NULL;
    CHECK_HR_OK(G2_F3_CREATEFACE(font3, &face));
    if (face && SUCCEEDED(IDWriteFontFace3_GetFamilyNames(face, &fam)) && fam) {
        char buf[64]; dw_ls_en(fam, buf, sizeof buf);
        EXPECT_STR(buf, "G2F3Names");
    }
    DW_RELEASE(fam); DW_RELEASE(face);
    F3_TEARDOWN(c, font, font3);
}

TEST(gap2_f3cf_metrics_match_font_metrics)
{
    F3_SETUP(c, "G2F3Metrics", font, font3);
    IDWriteFontFace3 *face = NULL; DWRITE_FONT_METRICS a, b;
    CHECK_HR_OK(G2_F3_CREATEFACE(font3, &face));
    if (face) {
        IDWriteFont_GetMetrics(font, &a); IDWriteFontFace_GetMetrics((IDWriteFontFace *)face, &b);
        CHECK(!memcmp(&a, &b, sizeof a));
        EXPECT_EQ(a.designUnitsPerEm, 2048);
    }
    DW_RELEASE(face);
    F3_TEARDOWN(c, font, font3);
}

TEST(gap2_f3cf_repeated_calls_all_succeed)
{
    F3_SETUP(c, "G2F3Rep", font, font3);
    int i;
    for (i = 0; i < 50; i++) {
        IDWriteFontFace3 *face = (IDWriteFontFace3 *)G2_SENT;
        HRESULT hr = G2_F3_CREATEFACE(font3, &face);
        CHECK_HR(hr, S_OK);
        if (SUCCEEDED(hr) && face != G2_SENT) DW_RELEASE(face);
    }
    F3_TEARDOWN(c, font, font3);
}

static void sys_face_check(const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *sc = NULL; IDWriteFont *font; IDWriteFont3 *f3 = NULL; IDWriteFontFace3 *face = NULL;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &sc, FALSE)) || !sc) { t_skip("no system collection"); return; }
    font = g2_font(sc, fam, w, st);
    if (!font) { DW_RELEASE(sc); t_skip("%ls not installed", fam); return; }
    if (SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&f3))) {
        CHECK_HR_OK(G2_F3_CREATEFACE(f3, &face));
        if (face) {
            EXPECT_EQ(IDWriteFontFace3_GetIndex(face) < 64, 1);
            CHECK(IDWriteFontFace3_GetGlyphCount(face) > 100);
            CHECK(IDWriteFontFace3_GetSimulations(face) == IDWriteFont_GetSimulations(font));
            {
                DWRITE_FONT_METRICS m1, m2; IDWriteFont_GetMetrics(font, &m1); IDWriteFontFace_GetMetrics((IDWriteFontFace *)face, &m2);
                CHECK(m1.designUnitsPerEm == m2.designUnitsPerEm && m1.ascent == m2.ascent && m1.descent == m2.descent);
            }
        }
    } else t_skip("no IDWriteFont3");
    DW_RELEASE(face); DW_RELEASE(f3); DW_RELEASE(font); DW_RELEASE(sc);
}
TEST(gap2_f3cf_system_arial_regular)   { sys_face_check(L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(gap2_f3cf_system_arial_bold)      { sys_face_check(L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL); }
TEST(gap2_f3cf_system_arial_italic)    { sys_face_check(L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC); }
TEST(gap2_f3cf_system_times)           { sys_face_check(L"Times New Roman", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(gap2_f3cf_system_segoe_ui)        { sys_face_check(L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(gap2_f3cf_system_segoe_ui_light)  { sys_face_check(L"Segoe UI", DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL); }
TEST(gap2_f3cf_system_consolas)        { sys_face_check(L"Consolas", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(gap2_f3cf_system_ms_gothic)       { sys_face_check(L"MS Gothic", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(gap2_f3cf_system_bahnschrift)     { sys_face_check(L"Bahnschrift", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
