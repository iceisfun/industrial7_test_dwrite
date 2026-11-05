/* Named instances of the synthetic fixture reached through a private custom font collection (IDWriteFontCollection / IDWriteFontFamily / IDWriteFont)
 * - the "existing APIs project named instances into the weight/stretch/style model" workflow of the OpenType variable fonts documentation: a font
 * picked by weight is the named instance, its face carries the instance coordinates, layouts using the family pick them up.
 * Face names and the exact family projection of the stretched instance are only noted.
 */
#include "rv.h"

static IDWriteFontFamily *family(rc *c)
{
    UINT32 idx = 0; BOOL ex = FALSE; IDWriteFontFamily *f = NULL;
    if (SUCCEEDED(IDWriteFontCollection_FindFamilyName(c->coll, RV_FAM, &idx, &ex)) && ex) IDWriteFontCollection_GetFontFamily(c->coll, idx, &f);
    return f;
}
static IDWriteFontFace5 *face5_of(IDWriteFont *font)
{
    IDWriteFontFace *f = NULL; IDWriteFontFace5 *f5 = NULL;
    if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &f)) && f) IDWriteFontFace_QueryInterface(f, &IID_IDWriteFontFace5, (void **)&f5);
    DW_RELEASE(f);
    return f5;
}
static float axis_of_face(IDWriteFontFace5 *f, UINT32 tag)
{
    DWRITE_FONT_AXIS_VALUE v[16]; UINT32 n = IDWriteFontFace5_GetFontAxisValueCount(f); int k;
    if (n == 0 || n > 16 || FAILED(IDWriteFontFace5_GetFontAxisValues(f, v, n))) return -12345.0f;
    k = rv_find_value(v, n, tag);
    return k >= 0 ? v[k].value : -12345.0f;
}

TEST(r2_variable_fonts_inst_family_found)
{
    RC_BEGIN(0);
    UINT32 idx = 0; BOOL ex = FALSE;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c.coll, RV_FAM, &idx, &ex));
    CHECK(ex);
    t_note("custom collection: %u families", IDWriteFontCollection_GetFontFamilyCount(c.coll));
    RC_END();
}
TEST(r2_variable_fonts_inst_family_font_count_note)
{
    RC_BEGIN(0);
    IDWriteFontFamily *f = family(&c);
    if (f) { UINT32 n = IDWriteFontFamily_GetFontCount(f); t_note("family RVVar: %u fonts (fixture has %d named instances)", n, RV_NINST); CHECK(n >= 1); DW_RELEASE(f); } else CHECK(0);
    RC_END();
}
TEST(r2_variable_fonts_inst_matching_font_is_the_instance)
{
    static const struct { DWRITE_FONT_WEIGHT w; float wght; } t[] = { { DWRITE_FONT_WEIGHT_LIGHT, 300 }, { DWRITE_FONT_WEIGHT_NORMAL, 400 }, { DWRITE_FONT_WEIGHT_BOLD, 700 } }; unsigned i;
    RC_BEGIN(0);
    IDWriteFontFamily *f = family(&c);
    for (i = 0; f && i < 3; i++) {
        IDWriteFont *font = NULL; HRESULT hr = IDWriteFontFamily_GetFirstMatchingFont(f, t[i].w, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
        CHECKF(hr == S_OK, "weight %d -> %s", t[i].w, dw_hr_name(hr));
        if (font) {
            IDWriteFontFace5 *f5 = face5_of(font);
            CHECKF(IDWriteFont_GetWeight(font) == t[i].w, "weight %d: font reports %d", t[i].w, IDWriteFont_GetWeight(font));
            if (f5) {
                float w = axis_of_face(f5, TAG_WGHT);
                CHECKF(w == t[i].wght, "weight %d: face wght = %g, expected %g", t[i].w, w, t[i].wght);
                CHECK(IDWriteFontFace5_HasVariations(f5) == TRUE);
                DW_RELEASE(f5);
            } else CHECK(0);
            DW_RELEASE(font);
        }
    }
    DW_RELEASE(f);
    RC_END();
}
TEST(r2_variable_fonts_inst_normal_instance_has_default_other_axes)
{
    RC_BEGIN(0);
    IDWriteFontFamily *f = family(&c);
    if (f) {
        IDWriteFont *font = NULL;
        if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(f, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font)) && font) {
            IDWriteFontFace5 *f5 = face5_of(font);
            if (f5) { CHECK(axis_of_face(f5, TAG_WDTH) == 100.0f); CHECK(axis_of_face(f5, TAG_OPSZ) == 14.0f); CHECK(axis_of_face(f5, TAG_FIXD) == 5.0f); DW_RELEASE(f5); }
            DW_RELEASE(font);
        }
        DW_RELEASE(f);
    }
    RC_END();
}
TEST(r2_variable_fonts_inst_face_names_note)
{
    RC_BEGIN(0);
    IDWriteFontFamily *f = family(&c);
    if (f) {
        UINT32 n = IDWriteFontFamily_GetFontCount(f), i;
        for (i = 0; i < n && i < 8; i++) {
            IDWriteFont *font = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFont(f, i, &font)) && font) {
                IDWriteLocalizedStrings *s = NULL; char en[64] = "";
                if (SUCCEEDED(IDWriteFont_GetFaceNames(font, &s)) && s) { if (IDWriteLocalizedStrings_GetCount(s) > 0) dw_ls_en(s, en, sizeof en); DW_RELEASE(s); }
                t_note("font %u: face name \"%s\", weight %d, stretch %d, style %d", i, en, IDWriteFont_GetWeight(font), IDWriteFont_GetStretch(font), IDWriteFont_GetStyle(font));
                DW_RELEASE(font);
            }
        }
        DW_RELEASE(f);
    }
    RC_END();
}
TEST(r2_variable_fonts_inst_fonts_share_one_file)
{
    RC_BEGIN(0);
    IDWriteFontFamily *f = family(&c);
    if (f) {
        UINT32 n = IDWriteFontFamily_GetFontCount(f), i; const void *k0 = NULL; UINT32 s0 = 0;
        for (i = 0; i < n && i < 8; i++) {
            IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFont(f, i, &font)) && font && SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
                IDWriteFontFile *files[2] = { NULL, NULL }; UINT32 nf = 2;
                if (SUCCEEDED(IDWriteFontFace_GetFiles(face, &nf, files)) && nf == 1 && files[0]) {
                    const void *k = NULL; UINT32 s = 0;
                    CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(files[0], &k, &s));
                    if (i == 0) { k0 = k; s0 = s; } else { EXPECT_EQ(s, s0); if (s == s0 && s) CHECK(!memcmp(k, k0, s)); }
                }
                DW_RELEASE(files[0]);
            }
            DW_RELEASE(face); DW_RELEASE(font);
        }
        DW_RELEASE(f);
    }
    RC_END();
}
TEST(r2_variable_fonts_inst_layout_by_weight_picks_instance_width)
{
    /* with HVAR the bold instance (wght 700) has A advance 1000 + 100 * (700 - 400) / 500 = 1060; only observed when dwrite applies HVAR */
    IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m0, m1; float w0 = 0, w1 = 0;
    RC_BEGIN(RVF_HVAR);
    if (SUCCEEDED(IDWriteFactory_CreateTextFormat((IDWriteFactory *)c.f6, RV_FAM, c.coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 64.0f, L"en-us", &t)) && t &&
        SUCCEEDED(IDWriteFactory_CreateTextLayout((IDWriteFactory *)c.f6, L"AAAA", 4, t, 10000.0f, 1000.0f, &l)) && l) { CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m0)); w0 = m0.widthIncludingTrailingWhitespace; }
    DW_RELEASE(l); DW_RELEASE(t);
    if (SUCCEEDED(IDWriteFactory_CreateTextFormat((IDWriteFactory *)c.f6, RV_FAM, c.coll, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 64.0f, L"en-us", &t)) && t &&
        SUCCEEDED(IDWriteFactory_CreateTextLayout((IDWriteFactory *)c.f6, L"AAAA", 4, t, 10000.0f, 1000.0f, &l)) && l) { CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m1)); w1 = m1.widthIncludingTrailingWhitespace; }
    DW_RELEASE(l); DW_RELEASE(t);
    CHECKF(w0 == 125.0f, "regular width %g", w0);
    if (w1 == w0) t_note("the bold named instance is laid out with the default advances");
    else CHECKF(w1 == 4.0f * 1060.0f / 2048.0f * 64.0f, "bold width %g, expected %g", w1, 4.0f * 1060.0f / 2048.0f * 64.0f);
    RC_END();
}
TEST(r2_variable_fonts_inst_font_set_of_collection)
{
    RC_BEGIN(0);
    IDWriteFontCollection2 *c2 = NULL;
    if (SUCCEEDED(IDWriteFontCollection_QueryInterface(c.coll, &IID_IDWriteFontCollection2, (void **)&c2)) && c2) {
        IDWriteFontSet1 *s = NULL;
        CHECK_HR_OK(IDWriteFontCollection2_GetFontSet(c2, &s));
        if (s) { t_note("font set of the custom collection: %u fonts", IDWriteFontSet1_GetFontCount(s)); CHECK(IDWriteFontSet1_GetFontCount(s) >= 1); DW_RELEASE(s); }
        CHECK(IDWriteFontCollection2_GetFontFamilyModel(c2) == DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE || IDWriteFontCollection2_GetFontFamilyModel(c2) == DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC);
        DW_RELEASE(c2);
    } else t_note("custom collection does not implement IDWriteFontCollection2");
    RC_END();
}
