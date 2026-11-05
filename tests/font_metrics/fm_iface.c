/* The same metrics through every face / font interface version (IDWriteFontFace1..5, IDWriteFont1..3) and across factories. */
#include "fm.h"

static const IID *face_iids[] = { &IID_IDWriteFontFace1, &IID_IDWriteFontFace2, &IID_IDWriteFontFace3, &IID_IDWriteFontFace4, &IID_IDWriteFontFace5 };
static const char *face_names[] = { "IDWriteFontFace1", "IDWriteFontFace2", "IDWriteFontFace3", "IDWriteFontFace4", "IDWriteFontFace5" };

TEST(fmi_face_versions_same_metrics1) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 ref; unsigned i;
    memset(&ref, 0xCD, sizeof ref);
    if (F.f1) IDWriteFontFace1_GetMetrics(F.f1, &ref);
    for (i = 0; i < 5; i++) {
        void *p = NULL; HRESULT hr = IDWriteFontFace_QueryInterface(F.face, face_iids[i], &p);
        if (FAILED(hr)) { t_note("%s: %s", face_names[i], dw_hr_name(hr)); CHECKF(i >= 1 || SUCCEEDED(hr), "IDWriteFontFace1 must exist"); continue; }
        {
            DWRITE_FONT_METRICS1 m; memset(&m, 0xAB, sizeof m);
            /* every later interface inherits the IDWriteFontFace1 slots, so the same vtable entry answers */
            IDWriteFontFace1_GetMetrics((IDWriteFontFace1 *)p, &m);
            CHECKF(!memcmp(&m, &ref, sizeof m), "%s metrics differ", face_names[i]);
        }
        IUnknown_Release((IUnknown *)p);
    }
    fm_close(&F);
}
TEST(fmi_face_versions_same_gdi_metrics_and_caret) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, DWRITE_FONT_SIMULATIONS_OBLIQUE);
    DWRITE_FONT_METRICS1 ref; DWRITE_CARET_METRICS cref; unsigned i;
    if (!F.f1) { t_skip("IDWriteFontFace1 unavailable"); fm_close(&F); return; }
    CHECK_HR(IDWriteFontFace1_GetGdiCompatibleMetrics(F.f1, 15.0f, 1.0f, NULL, &ref), S_OK);
    IDWriteFontFace1_GetCaretMetrics(F.f1, &cref);
    for (i = 1; i < 5; i++) {
        void *p = NULL;
        if (FAILED(IDWriteFontFace_QueryInterface(F.face, face_iids[i], &p))) continue;
        {
            IDWriteFontFace1 *q = (IDWriteFontFace1 *)p; DWRITE_FONT_METRICS1 m; DWRITE_CARET_METRICS c;
            CHECK_HR(IDWriteFontFace1_GetGdiCompatibleMetrics(q, 15.0f, 1.0f, NULL, &m), S_OK);
            CHECKF(!memcmp(&m, &ref, sizeof m), "%s GDI metrics differ", face_names[i]);
            q->lpVtbl->GetCaretMetrics(q, &c);
            CHECKF(!memcmp(&c, &cref, sizeof c), "%s caret differs", face_names[i]);
        }
        IUnknown_Release((IUnknown *)p);
    }
    fm_close(&F);
}
TEST(fmi_face_versions_same_design_metrics) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, DWRITE_FONT_SIMULATIONS_BOLD);
    UINT16 ids[3] = { 2, 1, 66 }; DWRITE_GLYPH_METRICS ref[3]; unsigned i;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(F.face, ids, 3, ref, FALSE), S_OK);
    for (i = 1; i < 5; i++) {
        void *p = NULL; DWRITE_GLYPH_METRICS m[3];
        if (FAILED(IDWriteFontFace_QueryInterface(F.face, face_iids[i], &p))) continue;
        memset(m, 0xAB, sizeof m);
        CHECK_HR(((IDWriteFontFace1 *)p)->lpVtbl->GetDesignGlyphMetrics((IDWriteFontFace1 *)p, ids, 3, m, FALSE), S_OK);
        CHECKF(!memcmp(m, ref, sizeof m), "%s design metrics differ", face_names[i]);
        IUnknown_Release((IUnknown *)p);
    }
    fm_close(&F);
}
TEST(fmi_font_versions_same_metrics_arial) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; UINT32 idx; BOOL ex = FALSE;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex);
    if (!ex || FAILED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) || !fam) { IDWriteFontCollection_Release(c); t_skip("Arial missing"); return; }
    IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
    if (font) {
        static const IID *iids[] = { &IID_IDWriteFont1, &IID_IDWriteFont2, &IID_IDWriteFont3 };
        DWRITE_FONT_METRICS v0; DWRITE_FONT_METRICS1 ref; unsigned i; IDWriteFont1 *f1 = NULL;
        IDWriteFont_GetMetrics(font, &v0);
        if (SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont1, (void **)&f1)) && f1) {
            IDWriteFont1_GetMetrics(f1, &ref);
            EXPECT_EQ(ref.designUnitsPerEm, v0.designUnitsPerEm); EXPECT_EQ(ref.ascent, v0.ascent); EXPECT_EQ(ref.descent, v0.descent); EXPECT_EQ(ref.lineGap, v0.lineGap);
            EXPECT_EQ(ref.capHeight, v0.capHeight); EXPECT_EQ(ref.xHeight, v0.xHeight);
            for (i = 1; i < 3; i++) {
                void *p = NULL; DWRITE_FONT_METRICS1 m;
                if (FAILED(IDWriteFont_QueryInterface(font, iids[i], &p))) continue;
                memset(&m, 0xAB, sizeof m);
                IDWriteFont1_GetMetrics((IDWriteFont1 *)p, &m);
                CHECK(!memcmp(&m, &ref, sizeof m));
                IUnknown_Release((IUnknown *)p);
            }
            DW_RELEASE(f1);
        }
        DW_RELEASE(font);
    }
    DW_RELEASE(fam); IDWriteFontCollection_Release(c);
}
TEST(fmi_metrics_survive_file_release) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    DWRITE_FONT_METRICS1 a, b;
    if (F.f1) {
        IDWriteFontFace1_GetMetrics(F.f1, &a);
        DW_RELEASE(F.h.file);     /* the face keeps what it needs */
        IDWriteFontFace1_GetMetrics(F.f1, &b);
        CHECK(!memcmp(&a, &b, sizeof a));
        {   /* the shared metrics cache does not depend on the file object either */
            DWRITE_FONT_METRICS1 g; CHECK_HR(IDWriteFontFace1_GetGdiCompatibleMetrics(F.f1, 11.0f, 1.0f, NULL, &g), S_OK);
        }
    }
    fm_close(&F);
}
TEST(fmi_shared_and_isolated_factory_same_metrics) {
    ttf_spec s; ttf_default_spec(&s); size_t n; uint8_t *blob = ttf_build(&s, &n); IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    dwx_font A, B; DWRITE_FONT_METRICS1 ma, mb;
    if (!blob || !iso || !dw_shared()) { free(blob); DW_RELEASE(iso); t_skip("factory unavailable"); return; }
    memset(&A, 0, sizeof A); memset(&B, 0, sizeof B);
    if (SUCCEEDED(dwx_font_open(&A, dw_shared(), blob, n, 0, 0)) && SUCCEEDED(dwx_font_open(&B, iso, blob, n, 0, 0)) && A.face && B.face) {
        IDWriteFontFace1 *a1 = fm_f1(A.face), *b1 = fm_f1(B.face);
        if (a1 && b1) { IDWriteFontFace1_GetMetrics(a1, &ma); IDWriteFontFace1_GetMetrics(b1, &mb); CHECK(!memcmp(&ma, &mb, sizeof ma)); EXPECT_EQ(ma.ascent, 1800); }
        DW_RELEASE(a1); DW_RELEASE(b1);
    } else t_skip("cannot open");
    dwx_font_close(&A); dwx_font_close(&B); DW_RELEASE(iso); free(blob);
}
TEST(fmi_face_identity_and_metrics_after_requery) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    IDWriteFontFace *again = NULL; DWRITE_FONT_METRICS a, b;
    IDWriteFontFace_GetMetrics(F.face, &a);
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(F.face, &IID_IDWriteFontFace, (void **)&again)) && again) {
        CHECK(again == F.face);
        IDWriteFontFace_GetMetrics(again, &b); CHECK(!memcmp(&a, &b, sizeof a));
        DW_RELEASE(again);
    }
    fm_close(&F);
}
