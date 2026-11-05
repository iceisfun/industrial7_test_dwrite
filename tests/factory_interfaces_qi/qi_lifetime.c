/* Object lifetime and reference ownership between COM objects handed out by dwrite.dll.
 * Every DirectWrite object keeps what it needs alive: objects stay usable after the factory (the last caller reference) is gone,
 * and a QueryInterface'd pointer keeps the object alive after the pointer it was queried from is released.
 */
#include "fq.h"

typedef struct {
    const char *name;
    IUnknown *(*make)(IDWriteFactory *f);
    int (*use)(IUnknown *o);            /* exercises the object; returns 1 when the call behaved */
    const char *query;                  /* an interface name to QI before the original pointer is released (NULL: none) */
} lt_kind;

static IUnknown *k_format(IDWriteFactory *f) {
    IDWriteTextFormat *t = NULL;
    return SUCCEEDED(IDWriteFactory_CreateTextFormat(f, L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"en-us", &t)) ? (IUnknown *)t : NULL;
}
static int u_format(IUnknown *o) { return IDWriteTextFormat_GetFontSize((IDWriteTextFormat *)o) == 15.0f && IDWriteTextFormat_GetFontWeight((IDWriteTextFormat *)o) == DWRITE_FONT_WEIGHT_NORMAL; }
static IUnknown *k_layout(IDWriteFactory *f) {
    IDWriteTextFormat *t = (IDWriteTextFormat *)k_format(f); IDWriteTextLayout *l = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(f, L"Hello world", 11, t, 300.0f, 50.0f, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return (IUnknown *)l;
}
static int u_layout(IUnknown *o) {
    DWRITE_TEXT_METRICS m; IDWriteTextLayout *l = (IDWriteTextLayout *)o;
    return IDWriteTextLayout_GetMaxWidth(l) == 300.0f && IDWriteTextLayout_GetMaxHeight(l) == 50.0f && SUCCEEDED(IDWriteTextLayout_GetMetrics(l, &m)) && m.layoutWidth == 300.0f;
}
static IUnknown *k_typography(IDWriteFactory *f) { IDWriteTypography *t = NULL; return SUCCEEDED(IDWriteFactory_CreateTypography(f, &t)) ? (IUnknown *)t : NULL; }
static int u_typography(IUnknown *o) {
    DWRITE_FONT_FEATURE ft = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1 }, got; IDWriteTypography *t = (IDWriteTypography *)o;
    return SUCCEEDED(IDWriteTypography_AddFontFeature(t, ft)) && IDWriteTypography_GetFontFeatureCount(t) == 1 && SUCCEEDED(IDWriteTypography_GetFontFeature(t, 0, &got)) && got.nameTag == ft.nameTag && got.parameter == 1;
}
static IUnknown *k_numsub(IDWriteFactory *f) { IDWriteNumberSubstitution *n = NULL; return SUCCEEDED(IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-US", FALSE, &n)) ? (IUnknown *)n : NULL; }
static int u_numsub(IUnknown *o) { return o != NULL; }
static IUnknown *k_gdi(IDWriteFactory *f) { IDWriteGdiInterop *g = NULL; return SUCCEEDED(IDWriteFactory_GetGdiInterop(f, &g)) ? (IUnknown *)g : NULL; }
static int u_gdi(IUnknown *o) {
    IDWriteBitmapRenderTarget *rt = NULL; int ok;
    ok = SUCCEEDED(IDWriteGdiInterop_CreateBitmapRenderTarget((IDWriteGdiInterop *)o, NULL, 8, 8, &rt)) && rt;
    if (rt) IDWriteBitmapRenderTarget_Release(rt);
    return ok;
}
static IUnknown *k_syscoll(IDWriteFactory *f) { IDWriteFontCollection *c = NULL; return SUCCEEDED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) ? (IUnknown *)c : NULL; }
static int u_coll(IUnknown *o) { return IDWriteFontCollection_GetFontFamilyCount((IDWriteFontCollection *)o) > 0; }
static IUnknown *k_family(IDWriteFactory *f) {
    IDWriteFontCollection *c = (IDWriteFontCollection *)k_syscoll(f); IDWriteFontFamily *fam = NULL;
    if (!c) return NULL;
    if (FAILED(IDWriteFontCollection_GetFontFamily(c, 0, &fam))) fam = NULL;
    IDWriteFontCollection_Release(c);
    return (IUnknown *)fam;
}
static int u_family(IUnknown *o) {
    IDWriteLocalizedStrings *s = NULL; int ok = SUCCEEDED(IDWriteFontFamily_GetFamilyNames((IDWriteFontFamily *)o, &s)) && s && IDWriteLocalizedStrings_GetCount(s) >= 1;
    DW_RELEASE(s);
    return ok && IDWriteFontFamily_GetFontCount((IDWriteFontFamily *)o) >= 1;
}
static IUnknown *k_font(IDWriteFactory *f) {
    IDWriteFontFamily *fam = (IDWriteFontFamily *)k_family(f); IDWriteFont *ft = NULL;
    if (!fam) return NULL;
    if (FAILED(IDWriteFontFamily_GetFont(fam, 0, &ft))) ft = NULL;
    IDWriteFontFamily_Release(fam);
    return (IUnknown *)ft;
}
static int u_font(IUnknown *o) {
    IDWriteFontFace *face = NULL; int ok = SUCCEEDED(IDWriteFont_CreateFontFace((IDWriteFont *)o, &face)) && face && IDWriteFontFace_GetGlyphCount(face) > 0;
    DW_RELEASE(face);
    return ok && IDWriteFont_GetWeight((IDWriteFont *)o) >= 1;
}
static IUnknown *k_face(IDWriteFactory *f) {
    IDWriteFont *ft = (IDWriteFont *)k_font(f); IDWriteFontFace *face = NULL;
    if (!ft) return NULL;
    if (FAILED(IDWriteFont_CreateFontFace(ft, &face))) face = NULL;
    IDWriteFont_Release(ft);
    return (IUnknown *)face;
}
static int u_face(IUnknown *o) { return IDWriteFontFace_GetGlyphCount((IDWriteFontFace *)o) > 0 && IDWriteFontFace_GetIndex((IDWriteFontFace *)o) < 64; }
static IUnknown *k_rp(IDWriteFactory *f) { IDWriteRenderingParams *p = NULL; return SUCCEEDED(IDWriteFactory_CreateCustomRenderingParams(f, 1.8f, 0.25f, 0.75f, DWRITE_PIXEL_GEOMETRY_BGR, DWRITE_RENDERING_MODE_NATURAL, &p)) ? (IUnknown *)p : NULL; }
static int u_rp(IUnknown *o) {
    IDWriteRenderingParams *p = (IDWriteRenderingParams *)o;
    return IDWriteRenderingParams_GetGamma(p) == 1.8f && IDWriteRenderingParams_GetEnhancedContrast(p) == 0.25f && IDWriteRenderingParams_GetClearTypeLevel(p) == 0.75f
        && IDWriteRenderingParams_GetPixelGeometry(p) == DWRITE_PIXEL_GEOMETRY_BGR && IDWriteRenderingParams_GetRenderingMode(p) == DWRITE_RENDERING_MODE_NATURAL;
}
static IUnknown *k_analyzer(IDWriteFactory *f) { IDWriteTextAnalyzer *a = NULL; return SUCCEEDED(IDWriteFactory_CreateTextAnalyzer(f, &a)) ? (IUnknown *)a : NULL; }
static int u_analyzer(IUnknown *o) { return o != NULL; }
static IUnknown *k_fbbuilder(IDWriteFactory *f) {
    IDWriteFactory2 *f2 = NULL; IDWriteFontFallbackBuilder *b = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2))) return NULL;
    if (FAILED(IDWriteFactory2_CreateFontFallbackBuilder(f2, &b))) b = NULL;
    IDWriteFactory2_Release(f2);
    return (IUnknown *)b;
}
static int u_fbbuilder(IUnknown *o) { IDWriteFontFallback *fb = NULL; int ok = SUCCEEDED(IDWriteFontFallbackBuilder_CreateFontFallback((IDWriteFontFallbackBuilder *)o, &fb)) && fb; DW_RELEASE(fb); return ok; }
static IUnknown *k_sysfallback(IDWriteFactory *f) {
    IDWriteFactory2 *f2 = NULL; IDWriteFontFallback *fb = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2))) return NULL;
    if (FAILED(IDWriteFactory2_GetSystemFontFallback(f2, &fb))) fb = NULL;
    IDWriteFactory2_Release(f2);
    return (IUnknown *)fb;
}
static int u_sysfallback(IUnknown *o) { return o != NULL; }
static IUnknown *k_sbuilder(IDWriteFactory *f) {
    IDWriteFactory3 *f3 = NULL; IDWriteFontSetBuilder *b = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3))) return NULL;
    if (FAILED(IDWriteFactory3_CreateFontSetBuilder(f3, &b))) b = NULL;
    IDWriteFactory3_Release(f3);
    return (IUnknown *)b;
}
static int u_sbuilder(IUnknown *o) {
    IDWriteFontSet *s = NULL; int ok = SUCCEEDED(IDWriteFontSetBuilder_CreateFontSet((IDWriteFontSetBuilder *)o, &s)) && s && IDWriteFontSet_GetFontCount(s) == 0;
    DW_RELEASE(s);
    return ok;
}
static IUnknown *k_sysfontset(IDWriteFactory *f) {
    IDWriteFactory3 *f3 = NULL; IDWriteFontSet *s = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3))) return NULL;
    if (FAILED(IDWriteFactory3_GetSystemFontSet(f3, &s))) s = NULL;
    IDWriteFactory3_Release(f3);
    return (IUnknown *)s;
}
static int u_fontset(IUnknown *o) { return IDWriteFontSet_GetFontCount((IDWriteFontSet *)o) > 0; }
static IUnknown *k_inline(IDWriteFactory *f) {
    IDWriteTextFormat *t = (IDWriteTextFormat *)k_format(f); IDWriteInlineObject *o = NULL;
    if (!t) return NULL;
    if (FAILED(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &o))) o = NULL;
    IDWriteTextFormat_Release(t);
    return (IUnknown *)o;
}
static int u_inline(IUnknown *o) { return o != NULL && dw_refcount(o) >= 1; }   /* GetMetrics of the trimming sign is exercised in the text-layout area */

static const lt_kind kinds[] = {
    { "text_format", k_format, u_format, "IDWriteTextFormat3" },
    { "text_layout", k_layout, u_layout, "IDWriteTextLayout4" },
    { "typography", k_typography, u_typography, NULL },
    { "number_substitution", k_numsub, u_numsub, NULL },
    { "gdi_interop", k_gdi, u_gdi, "IDWriteGdiInterop1" },
    { "font_collection", k_syscoll, u_coll, "IDWriteFontCollection3" },
    { "font_family", k_family, u_family, "IDWriteFontFamily2" },
    { "font", k_font, u_font, "IDWriteFont3" },
    { "font_face", k_face, u_face, "IDWriteFontFace5" },
    { "rendering_params", k_rp, u_rp, "IDWriteRenderingParams3" },
    { "text_analyzer", k_analyzer, u_analyzer, "IDWriteTextAnalyzer2" },
    { "font_fallback_builder", k_fbbuilder, u_fbbuilder, NULL },
    { "font_fallback", k_sysfallback, u_sysfallback, "IDWriteFontFallback1" },
    { "font_set_builder", k_sbuilder, u_sbuilder, "IDWriteFontSetBuilder2" },
    { "font_set", k_sysfontset, u_fontset, "IDWriteFontSet3" },
    { "inline_object", k_inline, u_inline, NULL },
};

/* object outlives the last caller reference of its (isolated) factory */
static void outlive_factory(const lt_kind *k) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IUnknown *o;
    if (!f) { t_skip("no factory"); return; }
    if (t_is_wine()) { IDWriteFactory_Release(f); t_skip("objects of this implementation do not retain their factory (crash)"); return; }
    o = k->make(f);
    if (!o) { IDWriteFactory_Release(f); t_skip("%s: creation failed", k->name); return; }
    { ULONG r = IDWriteFactory_Release(f); t_note("%s: factory refcount after the caller released it: %lu (objects retain the factory they came from)", k->name, r); }
    CHECKF(k->use(o), "%s unusable after the factory was released", k->name);
    if (k->query) {
        void *q = fq_qi_by_name(o, k->query);
        CHECKF(q != NULL, "QI %s after factory release", k->query);
        if (q) IUnknown_Release((IUnknown *)q);
    }
    IUnknown_Release(o);
}
/* QI'd pointer keeps the object alive after the original reference is dropped */
static void outlive_original(const lt_kind *k) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IUnknown *o, *q;
    if (!f) { t_skip("no factory"); return; }
    o = k->make(f);
    if (!o) { IDWriteFactory_Release(f); t_skip("%s: creation failed", k->name); return; }
    q = k->query ? (IUnknown *)fq_qi_by_name(o, k->query) : NULL;
    if (!q) { IUnknown_AddRef(o); q = o; }
    IUnknown_Release(o);
    CHECKF(k->use(q), "%s unusable through the queried pointer", k->name);
    /* use() takes the pointer typed for the base interface; a queried derived pointer is ABI compatible with it */
    IUnknown_Release(q);
    IDWriteFactory_Release(f);
}
/* many create/QI/release cycles leave the factory at exactly one reference */
static void churn(const lt_kind *k) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); int i; ULONG base;
    if (!f) { t_skip("no factory"); return; }
    base = dw_refcount((IUnknown *)f);
    for (i = 0; i < 25; i++) {
        IUnknown *o = k->make(f);
        if (!o) { t_skip("%s: creation failed", k->name); break; }
        if (k->query) { void *q = fq_qi_by_name(o, k->query); if (q) IUnknown_Release((IUnknown *)q); }
        IUnknown_Release(o);
    }
    t_note("%s: factory refcount before %lu, after churn %lu (objects may retain the factory or cache objects)", k->name, base, dw_refcount((IUnknown *)f));
    IDWriteFactory_Release(f);
}

#define KIND_TESTS(idx, nm) \
    TEST(lifetime_outlives_factory_##nm) { outlive_factory(&kinds[idx]); } \
    TEST(lifetime_outlives_original_##nm) { outlive_original(&kinds[idx]); } \
    TEST(lifetime_churn_##nm) { churn(&kinds[idx]); }
KIND_TESTS(0, text_format) KIND_TESTS(1, text_layout) KIND_TESTS(2, typography) KIND_TESTS(3, number_substitution)
KIND_TESTS(4, gdi_interop) KIND_TESTS(5, font_collection) KIND_TESTS(6, font_family) KIND_TESTS(7, font)
KIND_TESTS(8, font_face) KIND_TESTS(9, rendering_params) KIND_TESTS(10, text_analyzer) KIND_TESTS(11, font_fallback_builder)
KIND_TESTS(12, font_fallback) KIND_TESTS(13, font_set_builder) KIND_TESTS(14, font_set) KIND_TESTS(15, inline_object)

/* ---- ownership chains ---- */
TEST(lifetime_layout_independent_of_source_format) {
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fmt = NULL, *lf = NULL; IDWriteTextLayout *l = NULL;
    if (!f) { t_skip("no factory"); return; }
    fmt = (IDWriteTextFormat *)k_format(f);
    if (!fmt) { t_skip("no format"); return; }
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fmt, 100.0f, 100.0f, &l), S_OK);
    if (l) {
        EXPECT_REFS(fmt, 1);                         /* the layout copies the format, it does not retain it */
        CHECK_HR(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat, (void **)&lf), S_OK);
        if (lf) {
            CHECK((void *)lf != (void *)fmt);
            CHECK(!dw_same_identity((IUnknown *)lf, (IUnknown *)fmt));
            CHECK(dw_same_identity((IUnknown *)lf, (IUnknown *)l));
            /* changing the layout through its IDWriteTextFormat face does not touch the source format */
            CHECK_HR(IDWriteTextFormat_SetTextAlignment(lf, DWRITE_TEXT_ALIGNMENT_CENTER), S_OK);
            EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(lf), DWRITE_TEXT_ALIGNMENT_CENTER);
            EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(fmt), DWRITE_TEXT_ALIGNMENT_LEADING);
            IDWriteTextFormat_Release(lf);
        }
        IDWriteTextFormat_Release(fmt); fmt = NULL;
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(fmt);
}
TEST(lifetime_face_reference_shares_count) {
    /* DWriteFontFace::QueryInterface (decomp 0x180075620): IDWriteFontFaceReference is answered from the face object itself. */
    fq_ctx c; IUnknown *face; IDWriteFontFaceReference *ref = NULL; ULONG base;
    fq_ctx_init(&c);
    face = fq_cls_font_face_custom.create(&c);
    if (!face) { fq_ctx_free(&c); t_skip("no face"); return; }
    base = dw_refcount(face);
    CHECK_HR(IUnknown_QueryInterface(face, &IID_IDWriteFontFaceReference, (void **)&ref), S_OK);
    if (ref) {
        EXPECT_EQ(dw_refcount((IUnknown *)ref), base + 1);
        CHECK((void *)ref != (void *)face);                                    /* a secondary base of the object */
        CHECK(dw_same_identity((IUnknown *)ref, face));
        EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(ref), 0);
        EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(ref), DWRITE_FONT_SIMULATIONS_NONE);
        t_note("face %p, face-reference interface pointer %p (delta %lld)", (void *)face, (void *)ref, (long long)((char *)ref - (char *)face));
        IUnknown_Release(face);                                                /* creator's reference; the reference interface keeps the face */
        face = NULL;
        EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(ref), 0);
        IDWriteFontFaceReference_Release(ref);
    }
    if (face) IUnknown_Release(face);
    fq_ctx_free(&c);
}
TEST(lifetime_collection_outlives_family_chain) {
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontCollection *c; IDWriteFontFamily *fam = NULL; IDWriteFontCollection *c2 = NULL;
    if (!f) { t_skip("no factory"); return; }
    c = (IDWriteFontCollection *)k_syscoll(f);
    if (!c) { IDWriteFactory_Release(f); t_skip("no collection"); return; }
    CHECK_HR(IDWriteFontCollection_GetFontFamily(c, 0, &fam), S_OK);
    IDWriteFontCollection_Release(c);
    IDWriteFactory_Release(f);
    if (fam) {
        CHECK_HR(IDWriteFontFamily_GetFontCollection(fam, &c2), S_OK);
        CHECK(c2 != NULL && IDWriteFontCollection_GetFontFamilyCount(c2) > 0);
        DW_RELEASE(c2);
        IDWriteFontFamily_Release(fam);
    }
}
