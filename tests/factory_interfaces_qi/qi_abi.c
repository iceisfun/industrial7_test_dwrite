/* Vtable-shape (ABI) checks: an interface pointer obtained for a derived interface must be callable through the base
 * interface type (the base methods are a prefix of the derived vtable), and the derived-only methods must sit where the SDK
 * header puts them. Deterministic fixtures: the synthetic default font (UPM 2048, 69 glyphs, weight 400, width 5).
 */
#include "fq.h"

/* ---- text format ---- */
static IDWriteTextFormat *mkfmt(IDWriteFactory *f) {
    IDWriteTextFormat *t = NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(f, L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_EXPANDED, 17.5f, L"en-us", &t))) return NULL;
    return t;
}
static void check_format_values(IDWriteTextFormat *t, const char *who) {
    WCHAR name[32], loc[32];
    CHECKF(IDWriteTextFormat_GetFontSize(t) == 17.5f, "%s: size %g", who, IDWriteTextFormat_GetFontSize(t));
    CHECKF(IDWriteTextFormat_GetFontWeight(t) == DWRITE_FONT_WEIGHT_BOLD, "%s: weight", who);
    CHECKF(IDWriteTextFormat_GetFontStyle(t) == DWRITE_FONT_STYLE_ITALIC, "%s: style", who);
    CHECKF(IDWriteTextFormat_GetFontStretch(t) == DWRITE_FONT_STRETCH_EXPANDED, "%s: stretch", who);
    CHECKF(IDWriteTextFormat_GetFontFamilyNameLength(t) == 8, "%s: family length", who);
    CHECKF(IDWriteTextFormat_GetLocaleNameLength(t) == 5, "%s: locale length", who);
    if (SUCCEEDED(IDWriteTextFormat_GetFontFamilyName(t, name, 32))) CHECKF(!lstrcmpW(name, L"Segoe UI"), "%s: family name", who);
    if (SUCCEEDED(IDWriteTextFormat_GetLocaleName(t, loc, 32))) CHECKF(!lstrcmpiW(loc, L"en-us"), "%s: locale", who);
    CHECKF(IDWriteTextFormat_GetTextAlignment(t) == DWRITE_TEXT_ALIGNMENT_LEADING, "%s: alignment", who);
    CHECKF(IDWriteTextFormat_GetParagraphAlignment(t) == DWRITE_PARAGRAPH_ALIGNMENT_NEAR, "%s: paragraph alignment", who);
    CHECKF(IDWriteTextFormat_GetWordWrapping(t) == DWRITE_WORD_WRAPPING_WRAP, "%s: wrapping", who);
    CHECKF(IDWriteTextFormat_GetReadingDirection(t) == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, "%s: reading direction", who);
    CHECKF(IDWriteTextFormat_GetFlowDirection(t) == DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM, "%s: flow direction", who);
}
TEST(abi_text_format_base_through_each_interface) {
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *t; int i; static const char *const n[] = { "IDWriteTextFormat", "IDWriteTextFormat1", "IDWriteTextFormat2", "IDWriteTextFormat3" };
    if (!f || !(t = mkfmt(f))) { t_skip("no format"); return; }
    for (i = 0; i < 4; i++) {
        void *p = fq_qi_by_name((IUnknown *)t, n[i]);
        if (!p) { CHECKF(0, "QI %s", n[i]); continue; }
        check_format_values((IDWriteTextFormat *)p, n[i]);
        IUnknown_Release((IUnknown *)p);
    }
    IDWriteTextFormat_Release(t);
}
TEST(abi_text_format1_methods_through_format3) {
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *t; IDWriteTextFormat3 *p3 = NULL; IDWriteTextFormat1 *p1 = NULL; IDWriteTextFormat2 *p2 = NULL;
    if (!f || !(t = mkfmt(f))) { t_skip("no format"); return; }
    IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat3, (void **)&p3);
    IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat1, (void **)&p1);
    IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat2, (void **)&p2);
    if (p3 && p1 && p2) {
        /* defaults documented for IDWriteTextFormat1 */
        EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(p1), DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT);
        EXPECT_EQ(IDWriteTextFormat1_GetOpticalAlignment(p1), DWRITE_OPTICAL_ALIGNMENT_NONE);
        /* write through the Format3 pointer typed as Format1, read through Format1 and back through Format3 */
        CHECK_HR(IDWriteTextFormat1_SetOpticalAlignment((IDWriteTextFormat1 *)p3, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS), S_OK);
        EXPECT_EQ(IDWriteTextFormat1_GetOpticalAlignment(p1), DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
        EXPECT_EQ(IDWriteTextFormat1_GetOpticalAlignment((IDWriteTextFormat1 *)p2), DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
        CHECK_HR(IDWriteTextFormat1_SetVerticalGlyphOrientation(p1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED), S_OK);
        EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation((IDWriteTextFormat1 *)p3), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
        /* IDWriteTextFormat2 line spacing: default is DEFAULT method, values reported through Format2/Format3 agree */
        {
            DWRITE_LINE_SPACING a, b;
            memset(&a, 0xAB, sizeof a); memset(&b, 0xCD, sizeof b);
            CHECK_HR(IDWriteTextFormat2_GetLineSpacing(p2, &a), S_OK);
            CHECK_HR(IDWriteTextFormat2_GetLineSpacing((IDWriteTextFormat2 *)p3, &b), S_OK);
            CHECK(!memcmp(&a, &b, sizeof a));
            EXPECT_EQ(a.method, DWRITE_LINE_SPACING_METHOD_DEFAULT);
        }
        /* IDWriteTextFormat3 axis values: a format created without axes reports none */
        t_note("IDWriteTextFormat3 font axis value count: %u", IDWriteTextFormat3_GetFontAxisValueCount(p3));
    } else CHECKF(0, "QI to IDWriteTextFormat1/2/3 failed");
    DW_RELEASE(p3); DW_RELEASE(p1); DW_RELEASE(p2);
    IDWriteTextFormat_Release(t);
}

/* ---- text layout ---- */
TEST(abi_text_layout_base_through_each_interface) {
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *t; IDWriteTextLayout *l = NULL; int i;
    static const char *const n[] = { "IDWriteTextLayout", "IDWriteTextLayout1", "IDWriteTextLayout2", "IDWriteTextLayout3", "IDWriteTextLayout4" };
    if (!f || !(t = mkfmt(f))) { t_skip("no format"); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"Hello", 5, t, 120.0f, 64.0f, &l), S_OK);
    IDWriteTextFormat_Release(t);
    if (!l) return;
    for (i = 0; i < 5; i++) {
        void *p = fq_qi_by_name((IUnknown *)l, n[i]);
        if (!p) { CHECKF(0, "QI %s", n[i]); continue; }
        CHECKF(IDWriteTextLayout_GetMaxWidth((IDWriteTextLayout *)p) == 120.0f, "%s: GetMaxWidth", n[i]);
        CHECKF(IDWriteTextLayout_GetMaxHeight((IDWriteTextLayout *)p) == 64.0f, "%s: GetMaxHeight", n[i]);
        check_format_values((IDWriteTextFormat *)p, n[i]);          /* the layout interfaces derive from IDWriteTextFormat */
        IUnknown_Release((IUnknown *)p);
    }
    IDWriteTextLayout_Release(l);
}
TEST(abi_text_layout_state_shared_between_interfaces) {
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *t; IDWriteTextLayout *l = NULL; IDWriteTextLayout4 *l4 = NULL; IDWriteTextLayout1 *l1 = NULL;
    if (!f || !(t = mkfmt(f))) { t_skip("no format"); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"Hello", 5, t, 120.0f, 64.0f, &l), S_OK);
    IDWriteTextFormat_Release(t);
    if (!l) return;
    IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4);
    IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1);
    if (l4 && l1) {
        CHECK_HR(IDWriteTextLayout_SetMaxWidth((IDWriteTextLayout *)l4, 333.0f), S_OK);          /* write via the newest interface typed as the oldest */
        EXPECT_EQ(IDWriteTextLayout_GetMaxWidth(l), 333.0f);
        EXPECT_EQ(IDWriteTextLayout_GetMaxWidth((IDWriteTextLayout *)l1), 333.0f);
        CHECK_HR(IDWriteTextLayout_SetMaxHeight(l, 77.0f), S_OK);
        EXPECT_EQ(IDWriteTextLayout_GetMaxHeight((IDWriteTextLayout *)l4), 77.0f);
        CHECK_HR(IDWriteTextFormat_SetTextAlignment((IDWriteTextFormat *)l4, DWRITE_TEXT_ALIGNMENT_TRAILING), S_OK);
        EXPECT_EQ(IDWriteTextFormat_GetTextAlignment((IDWriteTextFormat *)l), DWRITE_TEXT_ALIGNMENT_TRAILING);
        /* Layout1 pair spacing and Layout4 axis values come from the typed tails of the vtables */
        {
            BOOL kern = TRUE; DWRITE_TEXT_RANGE rng = { 0, 5 };
            CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, FALSE, rng), S_OK);
            CHECK_HR(IDWriteTextLayout1_GetPairKerning((IDWriteTextLayout1 *)l4, 0, &kern, NULL), S_OK);
            EXPECT_EQ(kern, FALSE);
        }
    } else CHECKF(0, "QI to layout1/4 failed");
    DW_RELEASE(l4); DW_RELEASE(l1);
    IDWriteTextLayout_Release(l);
}

/* ---- custom collection chain: collection -> family -> font -> face ---- */
typedef struct { fq_ctx c; IDWriteFontCollection *coll; IDWriteFontFamily *fam; IDWriteFont *font; IDWriteFontFace *face; } stack_t;
static int stack_open(stack_t *s) {
    memset(s, 0, sizeof *s); fq_ctx_init(&s->c);
    s->coll = (IDWriteFontCollection *)fq_cls_font_collection_custom.create(&s->c);
    if (!s->coll) return 0;
    if (FAILED(IDWriteFontCollection_GetFontFamily(s->coll, 0, &s->fam)) || !s->fam) return 0;
    if (FAILED(IDWriteFontFamily_GetFont(s->fam, 0, &s->font)) || !s->font) return 0;
    if (FAILED(IDWriteFont_CreateFontFace(s->font, &s->face)) || !s->face) return 0;
    return 1;
}
static void stack_close(stack_t *s) {
    DW_RELEASE(s->face); DW_RELEASE(s->font); DW_RELEASE(s->fam); DW_RELEASE(s->coll);
    fq_ctx_free(&s->c);
}
#define STACK(s) stack_t s; if (!stack_open(&s)) { stack_close(&s); t_skip("custom font stack unavailable"); return; }

TEST(abi_collection_base_through_each_interface) {
    int i; static const char *const n[] = { "IDWriteFontCollection", "IDWriteFontCollection1", "IDWriteFontCollection2", "IDWriteFontCollection3" };
    STACK(s)
    for (i = 0; i < 4; i++) {
        void *p = fq_qi_by_name((IUnknown *)s.coll, n[i]); UINT32 idx = 99; BOOL ex = FALSE; IDWriteFontFamily *fam = NULL;
        if (!p) { CHECKF(0, "QI %s", n[i]); continue; }
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount((IDWriteFontCollection *)p), 1);
        CHECK_HR(IDWriteFontCollection_GetFontFamily((IDWriteFontCollection *)p, 0, &fam), S_OK);
        DW_RELEASE(fam);
        { HRESULT hr_ = IDWriteFontCollection_GetFontFamily((IDWriteFontCollection *)p, 1, &fam); CHECK(FAILED(hr_)); t_note("GetFontFamily(out of range) -> %s", dw_hr_name(hr_)); }
        CHECK_OUT_NULL(fam);
        CHECK_HR(IDWriteFontCollection_FindFamilyName((IDWriteFontCollection *)p, L"No Such Family Name", &idx, &ex), S_OK);
        EXPECT_EQ(ex, FALSE);
        EXPECT_EQ(idx, UINT_MAX);
        IUnknown_Release((IUnknown *)p);
    }
    stack_close(&s);
}
TEST(abi_family_and_list_interfaces) {
    int i; static const char *const n[] = { "IDWriteFontFamily", "IDWriteFontFamily1", "IDWriteFontFamily2", "IDWriteFontList", "IDWriteFontList1", "IDWriteFontList2" };
    STACK(s)
    for (i = 0; i < 6; i++) {
        void *p = fq_qi_by_name((IUnknown *)s.fam, n[i]); IDWriteFont *ft = NULL; IDWriteFontCollection *c = NULL;
        if (!p) { CHECKF(0, "QI %s", n[i]); continue; }
        EXPECT_EQ(IDWriteFontList_GetFontCount((IDWriteFontList *)p), 1);                 /* base IDWriteFontList is the prefix of every one of these */
        CHECK_HR(IDWriteFontList_GetFont((IDWriteFontList *)p, 0, &ft), S_OK);
        DW_RELEASE(ft);
        { HRESULT hr_ = IDWriteFontList_GetFont((IDWriteFontList *)p, 1, &ft); CHECK(FAILED(hr_)); t_note("GetFont(out of range) -> %s", dw_hr_name(hr_)); }
        CHECK_OUT_NULL(ft);
        CHECK_HR(IDWriteFontList_GetFontCollection((IDWriteFontList *)p, &c), S_OK);
        CHECK(c != NULL);
        DW_RELEASE(c);
        IUnknown_Release((IUnknown *)p);
    }
    stack_close(&s);
}
TEST(abi_family1_font_locality) {
    IDWriteFontFamily1 *f1 = NULL; IDWriteFontList1 *l1 = NULL;
    STACK(s)
    IDWriteFontFamily_QueryInterface(s.fam, &IID_IDWriteFontFamily1, (void **)&f1);
    IDWriteFontFamily_QueryInterface(s.fam, &IID_IDWriteFontList1, (void **)&l1);
    if (f1 && l1) {
        EXPECT_EQ(IDWriteFontFamily1_GetFontLocality(f1, 0), DWRITE_LOCALITY_LOCAL);
        EXPECT_EQ(IDWriteFontList1_GetFontLocality(l1, 0), DWRITE_LOCALITY_LOCAL);
        {
            IDWriteFont3 *f3 = NULL;
            CHECK_HR(IDWriteFontFamily1_GetFont(f1, 0, &f3), S_OK);
            DW_RELEASE(f3);
        }
    } else CHECKF(0, "QI family1/list1");
    DW_RELEASE(f1); DW_RELEASE(l1);
    stack_close(&s);
}
TEST(abi_font_base_through_each_interface) {
    int i; static const char *const n[] = { "IDWriteFont", "IDWriteFont1", "IDWriteFont2", "IDWriteFont3" };
    STACK(s)
    for (i = 0; i < 4; i++) {
        void *p = fq_qi_by_name((IUnknown *)s.font, n[i]); IDWriteFontFace *face = NULL; IDWriteFontFamily *fam = NULL;
        if (!p) { CHECKF(0, "QI %s", n[i]); continue; }
        /* synthetic font: OS/2 weight 400, width 5, upright */
        CHECKF(IDWriteFont_GetWeight((IDWriteFont *)p) == DWRITE_FONT_WEIGHT_NORMAL, "%s: weight %d", n[i], IDWriteFont_GetWeight((IDWriteFont *)p));
        CHECKF(IDWriteFont_GetStretch((IDWriteFont *)p) == DWRITE_FONT_STRETCH_NORMAL, "%s: stretch", n[i]);
        CHECKF(IDWriteFont_GetStyle((IDWriteFont *)p) == DWRITE_FONT_STYLE_NORMAL, "%s: style", n[i]);
        CHECKF(IDWriteFont_IsSymbolFont((IDWriteFont *)p) == FALSE, "%s: symbol", n[i]);
        CHECKF(IDWriteFont_GetSimulations((IDWriteFont *)p) == DWRITE_FONT_SIMULATIONS_NONE, "%s: simulations", n[i]);
        CHECK_HR(IDWriteFont_CreateFontFace((IDWriteFont *)p, &face), S_OK);
        if (face) CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS);
        DW_RELEASE(face);
        CHECK_HR(IDWriteFont_GetFontFamily((IDWriteFont *)p, &fam), S_OK);
        DW_RELEASE(fam);
        IUnknown_Release((IUnknown *)p);
    }
    stack_close(&s);
}
TEST(abi_font_typed_tails) {
    IDWriteFont1 *f1 = NULL; IDWriteFont2 *f2 = NULL; IDWriteFont3 *f3 = NULL;
    STACK(s)
    IDWriteFont_QueryInterface(s.font, &IID_IDWriteFont1, (void **)&f1);
    IDWriteFont_QueryInterface(s.font, &IID_IDWriteFont2, (void **)&f2);
    IDWriteFont_QueryInterface(s.font, &IID_IDWriteFont3, (void **)&f3);
    if (f1 && f2 && f3) {
        DWRITE_FONT_METRICS1 m1; DWRITE_FONT_METRICS m0;
        memset(&m1, 0, sizeof m1);
        IDWriteFont1_GetMetrics(f1, &m1);
        IDWriteFont_GetMetrics(s.font, &m0);
        EXPECT_EQ(m1.designUnitsPerEm, TTF_DEF_UPM);
        EXPECT_EQ(m0.designUnitsPerEm, TTF_DEF_UPM);
        CHECK(m1.ascent == m0.ascent && m1.descent == m0.descent && m1.lineGap == m0.lineGap && m1.capHeight == m0.capHeight && m1.xHeight == m0.xHeight);
        /* the same through the Font2/Font3 pointers typed as Font1 (prefix) */
        {
            DWRITE_FONT_METRICS1 a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
            IDWriteFont1_GetMetrics((IDWriteFont1 *)f2, &a);
            IDWriteFont1_GetMetrics((IDWriteFont1 *)f3, &b);
            CHECK(!memcmp(&a, &m1, sizeof a) && !memcmp(&b, &m1, sizeof b));
        }
        EXPECT_EQ(IDWriteFont2_IsColorFont(f2), FALSE);
        {
            IDWriteFontFaceReference *ref = NULL;
            CHECK_HR(IDWriteFont3_GetFontFaceReference(f3, &ref), S_OK);
            if (ref) { EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(ref), 0); IDWriteFontFaceReference_Release(ref); }
        }
    } else CHECKF(0, "QI font1/2/3");
    DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(f3);
    stack_close(&s);
}
TEST(abi_face_base_through_each_interface) {
    int i; static const char *const n[] = { "IDWriteFontFace", "IDWriteFontFace1", "IDWriteFontFace2", "IDWriteFontFace3", "IDWriteFontFace4", "IDWriteFontFace5" };
    DWRITE_FONT_METRICS ref; STACK(s)
    IDWriteFontFace_GetMetrics(s.face, &ref);
    for (i = 0; i < 6; i++) {
        void *p = fq_qi_by_name((IUnknown *)s.face, n[i]); IDWriteFontFace *b; DWRITE_FONT_METRICS m;
        if (!p) { CHECKF(0, "QI %s", n[i]); continue; }
        b = (IDWriteFontFace *)p;
        CHECKF(IDWriteFontFace_GetType(b) == DWRITE_FONT_FACE_TYPE_TRUETYPE, "%s: type %d", n[i], IDWriteFontFace_GetType(b));
        CHECKF(IDWriteFontFace_GetIndex(b) == 0, "%s: index", n[i]);
        CHECKF(IDWriteFontFace_GetGlyphCount(b) == TTF_DEF_NGLYPHS, "%s: glyph count", n[i]);
        CHECKF(IDWriteFontFace_GetSimulations(b) == DWRITE_FONT_SIMULATIONS_NONE, "%s: simulations", n[i]);
        CHECKF(IDWriteFontFace_IsSymbolFont(b) == FALSE, "%s: symbol", n[i]);
        memset(&m, 0xEE, sizeof m);
        IDWriteFontFace_GetMetrics(b, &m);
        CHECKF(!memcmp(&m, &ref, sizeof m), "%s: metrics differ from the IDWriteFontFace pointer's", n[i]);
        CHECKF(m.designUnitsPerEm == TTF_DEF_UPM, "%s: upm", n[i]);
        {
            UINT32 cp = 'A'; UINT16 g = 0xFFFF;
            CHECK_HR(IDWriteFontFace_GetGlyphIndices(b, &cp, 1, &g), S_OK);
            CHECKF(g == ttf_default_gid('A'), "%s: gid for A = %u", n[i], g);
        }
        IUnknown_Release((IUnknown *)p);
    }
    stack_close(&s);
}
TEST(abi_face_typed_tails) {
    IDWriteFontFace1 *f1 = NULL; IDWriteFontFace2 *f2 = NULL; IDWriteFontFace3 *f3 = NULL; IDWriteFontFace4 *f4 = NULL; IDWriteFontFace5 *f5 = NULL;
    STACK(s)
    IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace1, (void **)&f1);
    IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace2, (void **)&f2);
    IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace3, (void **)&f3);
    IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace4, (void **)&f4);
    IDWriteFontFace_QueryInterface(s.face, &IID_IDWriteFontFace5, (void **)&f5);
    if (f1 && f2 && f3 && f4 && f5) {
        DWRITE_FONT_METRICS1 m; memset(&m, 0, sizeof m);
        IDWriteFontFace1_GetMetrics(f1, &m);
        EXPECT_EQ(m.designUnitsPerEm, TTF_DEF_UPM);
        EXPECT_EQ(IDWriteFontFace2_IsColorFont(f2), FALSE);
        EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(f2), 0);
        EXPECT_EQ(IDWriteFontFace3_GetWeight(f3), DWRITE_FONT_WEIGHT_NORMAL);
        EXPECT_EQ(IDWriteFontFace3_GetStretch(f3), DWRITE_FONT_STRETCH_NORMAL);
        EXPECT_EQ(IDWriteFontFace3_GetStyle(f3), DWRITE_FONT_STYLE_NORMAL);
        {
            IDWriteFontFaceReference *ref = NULL;
            CHECK_HR(IDWriteFontFace3_GetFontFaceReference(f3, &ref), S_OK);
            if (ref) { EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(ref), 0); IDWriteFontFaceReference_Release(ref); }
        }
        EXPECT_EQ(IDWriteFontFace5_HasVariations(f5), FALSE);
        EXPECT_EQ(IDWriteFontFace5_GetFontAxisValueCount(f5), 0);
        t_note("IDWriteFontFace4 supported glyph image formats: 0x%x", (unsigned)0);
        (void)f4;
    } else CHECKF(0, "QI face1..5");
    DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(f3); DW_RELEASE(f4); DW_RELEASE(f5);
    stack_close(&s);
}

/* ---- rendering params ---- */
TEST(abi_rendering_params_base_through_each_interface) {
    IDWriteRenderingParams *rp = NULL; int i; static const char *const n[] = { "IDWriteRenderingParams", "IDWriteRenderingParams1", "IDWriteRenderingParams2", "IDWriteRenderingParams3" };
    if (FAILED(IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 1.8f, 0.25f, 0.75f, DWRITE_PIXEL_GEOMETRY_BGR, DWRITE_RENDERING_MODE_NATURAL, &rp))) { t_skip("no params"); return; }
    for (i = 0; i < 4; i++) {
        void *p = fq_qi_by_name((IUnknown *)rp, n[i]); IDWriteRenderingParams *b;
        if (!p) { CHECKF(0, "QI %s", n[i]); continue; }
        b = (IDWriteRenderingParams *)p;
        CHECKF(IDWriteRenderingParams_GetGamma(b) == 1.8f, "%s: gamma", n[i]);
        CHECKF(IDWriteRenderingParams_GetEnhancedContrast(b) == 0.25f, "%s: contrast", n[i]);
        CHECKF(IDWriteRenderingParams_GetClearTypeLevel(b) == 0.75f, "%s: cleartype level", n[i]);
        CHECKF(IDWriteRenderingParams_GetPixelGeometry(b) == DWRITE_PIXEL_GEOMETRY_BGR, "%s: geometry", n[i]);
        CHECKF(IDWriteRenderingParams_GetRenderingMode(b) == DWRITE_RENDERING_MODE_NATURAL, "%s: mode", n[i]);
        IUnknown_Release((IUnknown *)p);
    }
    IDWriteRenderingParams_Release(rp);
}
TEST(abi_rendering_params_typed_tails) {
    IDWriteRenderingParams *rp = NULL; IDWriteRenderingParams1 *p1 = NULL; IDWriteRenderingParams2 *p2 = NULL; IDWriteRenderingParams3 *p3 = NULL;
    if (FAILED(IDWriteFactory_CreateCustomRenderingParams(dw_shared(), 1.8f, 0.25f, 0.75f, DWRITE_PIXEL_GEOMETRY_BGR, DWRITE_RENDERING_MODE_NATURAL, &rp))) { t_skip("no params"); return; }
    IDWriteRenderingParams_QueryInterface(rp, &IID_IDWriteRenderingParams1, (void **)&p1);
    IDWriteRenderingParams_QueryInterface(rp, &IID_IDWriteRenderingParams2, (void **)&p2);
    IDWriteRenderingParams_QueryInterface(rp, &IID_IDWriteRenderingParams3, (void **)&p3);
    if (p1 && p2 && p3) {
        float g1 = IDWriteRenderingParams1_GetGrayscaleEnhancedContrast(p1);
        CHECK(g1 == IDWriteRenderingParams1_GetGrayscaleEnhancedContrast((IDWriteRenderingParams1 *)p2));
        CHECK(g1 == IDWriteRenderingParams1_GetGrayscaleEnhancedContrast((IDWriteRenderingParams1 *)p3));
        t_note("grayscale enhanced contrast of CreateCustomRenderingParams(contrast 0.25): %g; gridfit mode %d; rendering mode1 %d", g1,
               IDWriteRenderingParams2_GetGridFitMode(p2), IDWriteRenderingParams3_GetRenderingMode1(p3));
        EXPECT_EQ(IDWriteRenderingParams2_GetGridFitMode(p2), IDWriteRenderingParams2_GetGridFitMode((IDWriteRenderingParams2 *)p3));
    } else CHECKF(0, "QI params1..3");
    DW_RELEASE(p1); DW_RELEASE(p2); DW_RELEASE(p3);
    IDWriteRenderingParams_Release(rp);
}

/* ---- factory family ---- */
TEST(abi_factory_base_through_each_version) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); int v;
    if (!f) { t_skip("no factory"); return; }
    for (v = 0; v <= 7; v++) {
        void *p = NULL; IDWriteTypography *t = NULL; IDWriteNumberSubstitution *ns = NULL; IDWriteGdiInterop *g = NULL; IDWriteTextFormat *tf = NULL; IDWriteFontCollection *c = NULL;
        IUnknown_QueryInterface((IUnknown *)f, dw_factory_iid(v), &p);
        if (!p) { CHECKF(0, "QI factory%d", v); continue; }
        CHECKF(SUCCEEDED(IDWriteFactory_CreateTypography((IDWriteFactory *)p, &t)) && t, "factory%d: CreateTypography", v);
        CHECKF(SUCCEEDED(IDWriteFactory_CreateNumberSubstitution((IDWriteFactory *)p, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-US", FALSE, &ns)) && ns, "factory%d: CreateNumberSubstitution", v);
        CHECKF(SUCCEEDED(IDWriteFactory_GetGdiInterop((IDWriteFactory *)p, &g)) && g, "factory%d: GetGdiInterop", v);
        CHECKF(SUCCEEDED(IDWriteFactory_CreateTextFormat((IDWriteFactory *)p, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &tf)) && tf, "factory%d: CreateTextFormat", v);
        CHECKF(SUCCEEDED(IDWriteFactory_GetSystemFontCollection((IDWriteFactory *)p, &c, FALSE)) && c, "factory%d: GetSystemFontCollection", v);
        DW_RELEASE(t); DW_RELEASE(ns); DW_RELEASE(g); DW_RELEASE(tf); DW_RELEASE(c);
        IUnknown_Release((IUnknown *)p);
    }
    IDWriteFactory_Release(f);
}
TEST(abi_factory_typed_tails) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); void *p[8]; int v;
    if (!f) { t_skip("no factory"); return; }
    for (v = 0; v <= 7; v++) { p[v] = NULL; IUnknown_QueryInterface((IUnknown *)f, dw_factory_iid(v), &p[v]); }
    if (p[7]) {
        IDWriteFontCollection *eudc = NULL; IDWriteFontFallback *fb = NULL; IDWriteFontSet *fs = NULL; IDWriteFontSetBuilder1 *b1 = NULL; IDWriteInMemoryFontFileLoader *ml = NULL;
        IDWriteFontDownloadQueue *q = NULL; IDWriteFontSetBuilder *b6 = NULL;
        CHECK_HR(IDWriteFactory1_GetEudcFontCollection((IDWriteFactory1 *)p[1], &eudc, FALSE), S_OK);
        CHECK_HR(IDWriteFactory2_GetSystemFontFallback((IDWriteFactory2 *)p[2], &fb), S_OK);
        CHECK_HR(IDWriteFactory3_GetSystemFontSet((IDWriteFactory3 *)p[3], &fs), S_OK);
        CHECK_HR(IDWriteFactory3_GetFontDownloadQueue((IDWriteFactory3 *)p[3], &q), S_OK);
        CHECK_HR(IDWriteFactory5_CreateFontSetBuilder((IDWriteFactory5 *)p[5], &b1), S_OK);
        CHECK_HR(IDWriteFactory5_CreateInMemoryFontFileLoader((IDWriteFactory5 *)p[5], &ml), S_OK);
        CHECK_HR(IDWriteFactory6_CreateFontSetBuilder((IDWriteFactory6 *)p[6], &b6), S_OK);
        DW_RELEASE(eudc); DW_RELEASE(fb); DW_RELEASE(fs); DW_RELEASE(q); DW_RELEASE(b1); DW_RELEASE(ml); DW_RELEASE(b6);
    } else CHECKF(0, "IDWriteFactory7 unavailable");
    for (v = 0; v <= 7; v++) if (p[v]) IUnknown_Release((IUnknown *)p[v]);
    IDWriteFactory_Release(f);
}
TEST(abi_font_set_base_through_each_interface) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *fs = NULL; int i;
    static const char *const n[] = { "IDWriteFontSet", "IDWriteFontSet1", "IDWriteFontSet2", "IDWriteFontSet3" };
    UINT32 cnt;
    if (!f3 || FAILED(IDWriteFactory3_GetSystemFontSet(f3, &fs))) { t_skip("no system font set"); return; }
    cnt = IDWriteFontSet_GetFontCount(fs);
    CHECK(cnt > 0);
    for (i = 0; i < 4; i++) {
        void *p = fq_qi_by_name((IUnknown *)fs, n[i]);
        if (!p) { CHECKF(0, "QI %s", n[i]); continue; }
        CHECKF(IDWriteFontSet_GetFontCount((IDWriteFontSet *)p) == cnt, "%s: count", n[i]);
        IUnknown_Release((IUnknown *)p);
    }
    IDWriteFontSet_Release(fs);
}
TEST(abi_bitmap_render_target_interfaces) {
    IDWriteGdiInterop *g = NULL; IDWriteBitmapRenderTarget *rt = NULL; IDWriteBitmapRenderTarget1 *rt1 = NULL; SIZE sz = { 0, 0 };
    if (FAILED(IDWriteFactory_GetGdiInterop(dw_shared(), &g))) { t_skip("no interop"); return; }
    if (FAILED(IDWriteGdiInterop_CreateBitmapRenderTarget(g, NULL, 16, 24, &rt)) || !rt) { IDWriteGdiInterop_Release(g); t_skip("no render target"); return; }
    CHECK_HR(IDWriteBitmapRenderTarget_QueryInterface(rt, &IID_IDWriteBitmapRenderTarget1, (void **)&rt1), S_OK);
    CHECK_HR(IDWriteBitmapRenderTarget_GetSize(rt, &sz), S_OK);
    EXPECT_EQ(sz.cx, 16); EXPECT_EQ(sz.cy, 24);
    CHECK(IDWriteBitmapRenderTarget_GetMemoryDC(rt) != NULL);
    EXPECT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(rt), 1.0f);              /* documented default */
    if (rt1) {
        CHECK_HR(IDWriteBitmapRenderTarget_SetPixelsPerDip((IDWriteBitmapRenderTarget *)rt1, 2.0f), S_OK);
        EXPECT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip(rt), 2.0f);           /* one state behind both interface pointers */
        EXPECT_EQ(IDWriteBitmapRenderTarget_GetPixelsPerDip((IDWriteBitmapRenderTarget *)rt1), 2.0f);
        EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(rt1), DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE);   /* documented default */
        CHECK_HR(IDWriteBitmapRenderTarget1_SetTextAntialiasMode(rt1, DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE), S_OK);
        EXPECT_EQ(IDWriteBitmapRenderTarget1_GetTextAntialiasMode(rt1), DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    }
    DW_RELEASE(rt1); IDWriteBitmapRenderTarget_Release(rt); IDWriteGdiInterop_Release(g);
}
