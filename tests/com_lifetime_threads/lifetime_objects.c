/* Object lifetime relations: dependents keep their factory, a layout keeps the format's state after the format is gone,
 * collection/family/font/face keep each other usable in any release order, and objects handed to dwrite (inline objects,
 * drawing effects, typography, trimming signs, analysis sources/sinks) are retained and released with exact balance. */
#include "clt.h"

static DWRITE_TEXT_RANGE RNG(UINT32 s, UINT32 l) { DWRITE_TEXT_RANGE r; r.startPosition = s; r.length = l; return r; }
static const clt_maker *mk_at(int idx) { int n; const clt_maker *m = clt_makers(&n); return idx < n ? &m[idx] : NULL; }

/* ---- every creatable object survives the release of its factory ---------------------------------------------------- */
static void body_factory_first(int idx) {
    clt_ctx c; IUnknown *o; const clt_maker *mk = mk_at(idx); dwx_mem_loader *probe; LONG at_factory_drop;
    NEED_PROC();
    if (!clt_ctx_init(&c)) { t_skip("no factory"); return; }
    CLT_LEAK_BEGIN(ff);
    probe = dwx_loader_new();
    CHECK_HR_OK(dwx_loader_register(probe, c.f));
    EXPECT_EQ(dwx_refs(probe), 2);
    o = mk->make(&c);
    if (!o) {
        t_skip("%s: not creatable here", mk->name);
        clt_ctx_done(&c); dwx_loader_unregister(probe); dwx_release(probe);
        return;
    }
    probe->registered = 0;                        /* no Unregister once the factory may be gone */
    DW_RELEASE(c.f);                              /* the caller's reference to the factory; fixtures were created via c.f */
    at_factory_drop = dwx_refs(probe);
    t_note("%s: factory %s after the creator's reference is dropped", mk->name, at_factory_drop == 2 ? "stays alive (object keeps it)" : "is destroyed");
    if (mk->touch) mk->touch(o);                  /* the object keeps working with or without the factory */
    CLT_REFS_ARITH(o);
    CLT_QI_BALANCE(o);
    IUnknown_Release(o);
    clt_ctx_abandon(&c);                          /* our own fixture references (no calls into the factory) */
    CHECKF(clt_settle_refs(probe, 1) == 1, "factory registry still holds the probe loader (%ld)", (long)dwx_refs(probe));                /* now nothing of dwrite is left holding the factory or its registry */
    dwx_release(probe);
    CLT_LEAK_END(ff);
}
TEST(factory_first_textformat) { body_factory_first(0); }
TEST(factory_first_textlayout) { body_factory_first(1); }
TEST(factory_first_textformat_view_of_layout) { body_factory_first(2); }
TEST(factory_first_typography) { body_factory_first(3); }
TEST(factory_first_numbersubstitution) { body_factory_first(4); }
TEST(factory_first_renderingparams) { body_factory_first(5); }
TEST(factory_first_customrenderingparams) { body_factory_first(6); }
TEST(factory_first_textanalyzer) { body_factory_first(7); }
TEST(factory_first_gdiinterop) { body_factory_first(8); }
TEST(factory_first_ellipsissign) { body_factory_first(9); }
TEST(factory_first_fontfile) { body_factory_first(10); }
TEST(factory_first_fontface) { body_factory_first(11); }
TEST(factory_first_customcollection) { body_factory_first(12); }
TEST(factory_first_systemcollection) { body_factory_first(13); }
TEST(factory_first_fontfamily) { body_factory_first(14); }
TEST(factory_first_font) { body_factory_first(15); }
TEST(factory_first_familynames) { body_factory_first(16); }
TEST(factory_first_facenames) { body_factory_first(17); }
TEST(factory_first_fontface_from_font) { body_factory_first(18); }
TEST(factory_first_bitmaprendertarget) { body_factory_first(19); }
TEST(factory_first_glyphrunanalysis) { body_factory_first(20); }
TEST(factory_first_isolatedfactory) { body_factory_first(21); }
TEST(factory_first_sharedfactory) { body_factory_first(22); }
TEST(factory_first_fontsetbuilder) { body_factory_first(23); }
TEST(factory_first_systemfontset) { body_factory_first(24); }
TEST(factory_first_fontfacereference) { body_factory_first(25); }
TEST(factory_first_inmemoryloader) { body_factory_first(26); }
TEST(factory_first_downloadqueue) { body_factory_first(27); }
TEST(factory_first_collection_from_fontset) { body_factory_first(28); }
TEST(factory_first_systemcollection1) { body_factory_first(29); }

/* ---- collection -> family -> font -> face: any release order ----------------------------------------------------------- */
static void chain_use(IDWriteFontCollection *c, IDWriteFontFamily *fam, IDWriteFont *fn, IDWriteFontFace *fc, const char *tag, const char *file, int line) {
    if (c) t_check(IDWriteFontCollection_GetFontFamilyCount(c) == 2, file, line, "collection alive", "%s", tag);
    if (fam) t_check(IDWriteFontFamily_GetFontCount(fam) >= 1, file, line, "family alive", "%s", tag);
    if (fn) t_check(IDWriteFont_GetWeight(fn) == DWRITE_FONT_WEIGHT_REGULAR, file, line, "font alive", "%s", tag);
    if (fc) { UINT32 cp = 'C'; UINT16 g = 0; HRESULT hr = IDWriteFontFace_GetGlyphIndices(fc, &cp, 1, &g); t_check(hr == S_OK && g == 4, file, line, "face alive", "%s hr=%s g=%u", tag, dw_hr_name(hr), g); }
}
static void permute(int *a, int k, int n, int out[][4], int *cnt) {
    int i;
    if (k == n) { memcpy(out[*cnt], a, sizeof(int) * 4); (*cnt)++; return; }
    for (i = k; i < n; i++) { int t = a[k]; a[k] = a[i]; a[i] = t; permute(a, k + 1, n, out, cnt); t = a[k]; a[k] = a[i]; a[i] = t; }
}
TEST(chain_any_release_order_keeps_remaining_alive) {
    NEED_PROC();
    int perms[24][4], cnt = 0, p, a0[4] = { 0, 1, 2, 3 };
    static const char *nm[4] = { "collection", "family", "font", "face" };
    permute(a0, 0, 4, perms, &cnt);
    EXPECT_EQ(cnt, 24);
    for (p = 0; p < cnt; p++) {
        clt_cfix x; IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *fn = NULL; IDWriteFontFace *fc = NULL;
        void *obj[4]; int i; char tag[64];
        CLT_LEAK_BEGIN(chain);
        if (!clt_cfix_open(&x, 2)) { t_skip("fixture"); clt_cfix_close(&x); return; }
        if (FAILED(dwx_coll_create(x.cl, x.f, &c)) || !c) { CHECKF(0, "perm %d: collection", p); clt_cfix_close(&x); return; }
        IDWriteFontCollection_GetFontFamily(c, 1, &fam);
        if (fam) IDWriteFontFamily_GetFont(fam, 0, &fn);
        if (fn) IDWriteFont_CreateFontFace(fn, &fc);
        if (!fam || !fn || !fc) { CHECKF(0, "perm %d: chain creation", p); DW_RELEASE(fc); DW_RELEASE(fn); DW_RELEASE(fam); DW_RELEASE(c); clt_cfix_close(&x); return; }
        obj[0] = c; obj[1] = fam; obj[2] = fn; obj[3] = fc;
        snprintf(tag, sizeof tag, "perm %d (%s,%s,%s,%s)", p, nm[perms[p][0]], nm[perms[p][1]], nm[perms[p][2]], nm[perms[p][3]]);
        chain_use(c, fam, fn, fc, tag, __FILE__, __LINE__);
        for (i = 0; i < 4; i++) {
            IUnknown_Release((IUnknown *)obj[perms[p][i]]);
            obj[perms[p][i]] = NULL;
            chain_use((IDWriteFontCollection *)obj[0], (IDWriteFontFamily *)obj[1], (IDWriteFont *)obj[2], (IDWriteFontFace *)obj[3], tag, __FILE__, __LINE__);
        }
        clt_cfix_close(&x);
        CLT_LEAK_END(chain);
    }
}
TEST(chain_face_outlives_everything_but_loader_registration) {
    NEED_PROC();
    clt_cfix x; IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *fn = NULL; IDWriteFontFace *fc = NULL;
    if (!clt_cfix_open(&x, 1)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    dwx_coll_create(x.cl, x.f, &c);
    if (c) IDWriteFontCollection_GetFontFamily(c, 0, &fam);
    if (fam) IDWriteFontFamily_GetFont(fam, 0, &fn);
    if (fn) IDWriteFont_CreateFontFace(fn, &fc);
    DW_RELEASE(fn); DW_RELEASE(fam); DW_RELEASE(c);
    CHECK(fc != NULL);
    if (fc) {
        DWRITE_FONT_METRICS m; UINT32 cp = 'B'; UINT16 g = 0; DWRITE_GLYPH_METRICS gm;
        IDWriteFontFace_GetMetrics(fc, &m); EXPECT_EQ(m.designUnitsPerEm, 2048);
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(fc, &cp, 1, &g)); EXPECT_EQ(g, 3);
        CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(fc, &g, 1, &gm, FALSE));
        EXPECT_EQ(gm.advanceWidth, 1000);
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(fc), TTF_DEF_NGLYPHS);
    }
    DW_RELEASE(fc);
    clt_cfix_close(&x);
}
TEST(chain_system_face_outlives_collection) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontFace *fc;
    if (!f) { t_skip("no factory"); return; }
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial not installed"); DW_RELEASE(f); return; }
    fc = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);   /* collection, family, font all released inside */
    CHECK(fc != NULL);
    if (fc) {
        DWRITE_FONT_METRICS m; UINT32 cp = 'A'; UINT16 g = 0;
        DW_RELEASE(f);                                      /* and the factory */
        IDWriteFontFace_GetMetrics(fc, &m);
        EXPECT_EQ(m.designUnitsPerEm, 2048);
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(fc, &cp, 1, &g)); CHECK(g != 0);
        DW_RELEASE(fc);
    }
    DW_RELEASE(f);
}
TEST(chain_system_font_outlives_family_and_collection) {
    NEED_PROC();
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *fn = NULL; UINT32 idx = 0; BOOL ex = 0;
    if (!f) { t_skip("no factory"); return; }
    IDWriteFactory_GetSystemFontCollection(f, &c, FALSE);
    if (c) IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex);
    if (!ex) { t_skip("Arial not installed"); DW_RELEASE(c); DW_RELEASE(f); return; }
    IDWriteFontCollection_GetFontFamily(c, idx, &fam);
    if (fam) IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &fn);
    DW_RELEASE(fam); DW_RELEASE(c);
    CHECK(fn != NULL);
    if (fn) {
        IDWriteFontFamily *back = NULL; IDWriteLocalizedStrings *names = NULL; UINT32 n = 0;
        EXPECT_EQ(IDWriteFont_GetWeight(fn), DWRITE_FONT_WEIGHT_BOLD);
        CHECK_HR_OK(IDWriteFont_GetFontFamily(fn, &back));          /* the font still knows its family */
        if (back) { CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(back, &names)); }
        if (names) { n = IDWriteLocalizedStrings_GetCount(names); CHECK(n >= 1); }
        DW_RELEASE(names); DW_RELEASE(back); DW_RELEASE(fn);
    }
    DW_RELEASE(f);
}

/* ---- layout and format ------------------------------------------------------------------------------------------------ */
static IDWriteTextFormat *rich_format(IDWriteFactory *f, dwx_inline *sign) {
    IDWriteTextFormat *fm = NULL; DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    if (FAILED(IDWriteFactory_CreateTextFormat(f, L"Times New Roman", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 21.5f, L"fr-FR", &fm))) return NULL;
    IDWriteTextFormat_SetTextAlignment(fm, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat_SetParagraphAlignment(fm, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    IDWriteTextFormat_SetWordWrapping(fm, DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteTextFormat_SetLineSpacing(fm, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f);
    IDWriteTextFormat_SetIncrementalTabStop(fm, 33.0f);
    IDWriteTextFormat_SetTrimming(fm, &tr, sign ? (IDWriteInlineObject *)sign : NULL);
    return fm;
}
TEST(layout_keeps_format_state_after_format_release) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm; IDWriteTextLayout *l = NULL; dwx_inline *sign = dwx_inline_new(10, 10, 8, FALSE);
    fm = rich_format(f, sign);
    if (!fm) { t_skip("format"); dwx_release(sign); return; }
    EXPECT_EQ(dwx_refs(sign), 2);                   /* the format owns the trimming sign */
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"Some layout text", 16, fm, 300.0f, 50.0f, &l));
    DW_RELEASE(fm);                                  /* format gone: the layout must carry its own copy of the state */
    if (l) {
        IDWriteTextFormat *lf = NULL; WCHAR name[64] = { 0 }; DWRITE_TEXT_RANGE r; DWRITE_FONT_WEIGHT w = 0; DWRITE_FONT_STYLE st = 0; DWRITE_FONT_STRETCH sr = 0; float sz = 0;
        DWRITE_TRIMMING tr; IDWriteInlineObject *s2 = NULL; DWRITE_LINE_SPACING_METHOD lm; float ls = 0, bl = 0;
        CHECK_HR_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat, (void **)&lf));
        if (lf) {
            EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(lf), DWRITE_TEXT_ALIGNMENT_CENTER);
            EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(lf), DWRITE_PARAGRAPH_ALIGNMENT_FAR);
            EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(lf), DWRITE_WORD_WRAPPING_NO_WRAP);
            CHECK_FLOAT_EQ(IDWriteTextFormat_GetIncrementalTabStop(lf), 33.0f);
            CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(lf, &lm, &ls, &bl));
            EXPECT_EQ(lm, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(ls, 30.0f); CHECK_FLOAT_EQ(bl, 24.0f);
            EXPECT_EQ(IDWriteTextFormat_GetFontWeight(lf), DWRITE_FONT_WEIGHT_BOLD);
            EXPECT_EQ(IDWriteTextFormat_GetFontStyle(lf), DWRITE_FONT_STYLE_ITALIC);
            EXPECT_EQ(IDWriteTextFormat_GetFontStretch(lf), DWRITE_FONT_STRETCH_CONDENSED);
            CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(lf), 21.5f);
            CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(lf, name, 64)); EXPECT_WSTR(name, L"Times New Roman");
            CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(lf, name, 64)); CHECK(!lstrcmpiW(name, L"fr-FR"));
            memset(&tr, 0xAA, sizeof tr);
            CHECK_HR_OK(IDWriteTextFormat_GetTrimming(lf, &tr, &s2));
            EXPECT_EQ(tr.granularity, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
            CHECK(s2 == (IDWriteInlineObject *)sign);             /* the sign object itself, not a copy */
            DW_RELEASE(s2);
            DW_RELEASE(lf);
        }
        /* position based getters report the same defaults over the whole text */
        CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 3, &w, &r)); EXPECT_EQ(w, DWRITE_FONT_WEIGHT_BOLD); t_note("default-run range of an untouched layout: start %u length %u", r.startPosition, r.length);
        CHECK_HR_OK(IDWriteTextLayout_GetFontStyle(l, 3, &st, &r)); EXPECT_EQ(st, DWRITE_FONT_STYLE_ITALIC);
        CHECK_HR_OK(IDWriteTextLayout_GetFontStretch(l, 3, &sr, &r)); EXPECT_EQ(sr, DWRITE_FONT_STRETCH_CONDENSED);
        CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 3, &sz, &r)); CHECK_FLOAT_EQ(sz, 21.5f);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), 300.0f);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), 50.0f);
        DW_RELEASE(l);
    }
    EXPECT_EQ(dwx_refs(sign), 1);
    dwx_release(sign);
}
TEST(layout_unaffected_by_later_format_changes) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; IDWriteTextFormat *lf = NULL; DWRITE_TEXT_RANGE r; float sz = 0;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &fm));
    if (!fm) return;
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, fm, 100, 100, &l));
    IDWriteTextFormat_SetTextAlignment(fm, DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat_SetWordWrapping(fm, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat, (void **)&lf);
        if (lf) {
            EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(lf), DWRITE_TEXT_ALIGNMENT_LEADING);
            EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(lf), DWRITE_WORD_WRAPPING_WRAP);
            DW_RELEASE(lf);
        }
        CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 0, &sz, &r)); CHECK_FLOAT_EQ(sz, 12.0f);
        /* and the other way round: layout changes never reach the format */
        CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 40.0f, RNG(0, 3)));
        CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(fm), 12.0f);
        DW_RELEASE(l);
    }
    DW_RELEASE(fm);
}
TEST(layout_holds_and_releases_inline_object) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; dwx_inline *a, *b;
    DWRITE_TEXT_RANGE all = { 0, 5 };
    CLT_LEAK_BEGIN(inl);
    a = dwx_inline_new(20, 10, 8, FALSE); b = dwx_inline_new(30, 10, 8, FALSE);
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(f, L"01234", 5, fm, 200, 100, &l);
    DW_RELEASE(fm);
    if (!l) { t_skip("layout"); dwx_release(a); dwx_release(b); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)a, all));
    CHECKF(dwx_refs(a) >= 2, "layout must retain the inline object (refs %ld)", (long)dwx_refs(a));
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)b, all));    /* replaces over the whole range */
    EXPECT_EQ(dwx_refs(a), 1);
    CHECKF(dwx_refs(b) >= 2, "second inline object retained");
    {   /* Get adds a reference the caller must release */
        IDWriteInlineObject *got = NULL; DWRITE_TEXT_RANGE r; LONG before = dwx_refs(b);
        CHECK_HR_OK(IDWriteTextLayout_GetInlineObject(l, 2, &got, &r));
        CHECK(got == (IDWriteInlineObject *)b);
        EXPECT_EQ(dwx_refs(b), before + 1);
        DW_RELEASE(got);
        EXPECT_EQ(dwx_refs(b), before);
    }
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, NULL, all));                         /* clearing releases */
    EXPECT_EQ(dwx_refs(b), 1);
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)a, RNG(1, 2)));
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(a), 1);
    dwx_release(a); dwx_release(b);
    CLT_LEAK_END(inl);
}
TEST(layout_holds_and_releases_drawing_effect) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; dwx_effect *e1 = dwx_effect_new(1), *e2 = dwx_effect_new(2);
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(f, L"0123456789", 10, fm, 200, 100, &l);
    DW_RELEASE(fm);
    if (!l) { t_skip("layout"); dwx_release(e1); dwx_release(e2); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(0, 10)));
    CHECK(dwx_refs(e1) >= 2);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e2, RNG(0, 10)));
    EXPECT_EQ(dwx_refs(e1), 1);
    CHECK(dwx_refs(e2) >= 2);
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e1, RNG(3, 3)));      /* splits the range: both objects are held */
    CHECK(dwx_refs(e1) >= 2); CHECK(dwx_refs(e2) >= 2);
    {
        IUnknown *got = NULL; DWRITE_TEXT_RANGE r; LONG before = dwx_refs(e1);
        CHECK_HR_OK(IDWriteTextLayout_GetDrawingEffect(l, 4, &got, &r));
        CHECK(got == (IUnknown *)e1);
        EXPECT_EQ(dwx_refs(e1), before + 1);
        DW_RELEASE(got);
        EXPECT_EQ(dwx_refs(e1), before);
    }
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(e1), 1); EXPECT_EQ(dwx_refs(e2), 1);
    dwx_release(e1); dwx_release(e2);
}
TEST(layout_holds_and_releases_typography) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; IDWriteTypography *ty = NULL; ULONG b;
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(f, L"abcdef", 6, fm, 200, 100, &l);
    DW_RELEASE(fm);
    IDWriteFactory_CreateTypography(f, &ty);
    if (!l || !ty) { t_skip("objects"); DW_RELEASE(l); DW_RELEASE(ty); return; }
    b = dw_refcount((IUnknown *)ty);
    CHECK_HR_OK(IDWriteTextLayout_SetTypography(l, ty, RNG(0, 6)));
    CHECKF(dw_refcount((IUnknown *)ty) > b, "typography not retained by the layout");
    DW_RELEASE(l);
    EXPECT_REFS(ty, b);
    DW_RELEASE(ty);
}
TEST(layout_holds_and_releases_font_collection) {
    NEED_PROC();
    clt_cfix x; IDWriteFontCollection *c = NULL; IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; ULONG b;
    if (!clt_cfix_open(&x, 1)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    dwx_coll_create(x.cl, x.f, &c);
    if (!c) { clt_cfix_close(&x); return; }
    b = dw_refcount((IUnknown *)c);
    IDWriteFactory_CreateTextFormat(x.f, L"CltFam0", c, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &fm);
    CHECK(fm != NULL);
    CHECKF(dw_refcount((IUnknown *)c) > b, "the format must retain its collection");
    if (fm) IDWriteFactory_CreateTextLayout(x.f, L"ABC", 3, fm, 200, 100, &l);
    DW_RELEASE(fm);                          /* the layout keeps using the collection after the format is gone */
    CHECKF(dw_refcount((IUnknown *)c) > b, "the layout must retain the collection too");
    if (l) {
        DWRITE_TEXT_METRICS m; IDWriteFontCollection *got = NULL; DWRITE_TEXT_RANGE r;
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
        CHECK_NEAR(m.width, 3 * 1000.0f * 16.0f / 2048.0f, 0.01);     /* three 1000-unit boxes at 16 DIPs: proves the synthetic font was used */
        CHECK_HR_OK(IDWriteTextLayout_GetFontCollection(l, 0, &got, &r));
        CHECK(got == c);
        DW_RELEASE(got);
    }
    DW_RELEASE(l);
    EXPECT_REFS(c, b);
    DW_RELEASE(c);
    clt_cfix_close(&x);
}
TEST(layout_set_font_collection_replace_balances) {
    NEED_PROC();
    clt_cfix x; IDWriteFontCollection *c = NULL, *sys = NULL; IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; ULONG b;
    if (!clt_cfix_open(&x, 1)) { t_skip("fixture"); clt_cfix_close(&x); return; }
    dwx_coll_create(x.cl, x.f, &c);
    IDWriteFactory_GetSystemFontCollection(x.f, &sys, FALSE);
    IDWriteFactory_CreateTextFormat(x.f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(x.f, L"ABC", 3, fm, 200, 100, &l);
    DW_RELEASE(fm);
    if (!c || !l) { DW_RELEASE(c); DW_RELEASE(sys); DW_RELEASE(l); clt_cfix_close(&x); t_skip("objects"); return; }
    b = dw_refcount((IUnknown *)c);
    CHECK_HR_OK(IDWriteTextLayout_SetFontCollection(l, c, RNG(0, 3)));
    CHECK(dw_refcount((IUnknown *)c) > b);
    CHECK_HR_OK(IDWriteTextLayout_SetFontCollection(l, sys, RNG(0, 3)));     /* replace over the whole text */
    EXPECT_REFS(c, b);
    DW_RELEASE(l); DW_RELEASE(c); DW_RELEASE(sys);
    clt_cfix_close(&x);
}
TEST(layout_draw_balances_renderer_and_objects) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm = NULL; IDWriteTextLayout *l = NULL; dwx_renderer *r = dwx_renderer_new(); dwx_inline *io = dwx_inline_new(20, 12, 9, FALSE);
    dwx_effect *e = dwx_effect_new(7);
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &fm);
    if (fm) IDWriteFactory_CreateTextLayout(f, L"ab\xFFFC" L"cd", 5, fm, 300, 100, &l);
    DW_RELEASE(fm);
    if (!l) { t_skip("layout"); dwx_release(r); dwx_release(io); dwx_release(e); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(2, 1)));
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)e, RNG(0, 5)));
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, (void *)0x1234, (IDWriteTextRenderer *)r, 5.0f, 6.0f));   /* warm-up: Windows 10 keeps extra references after the first Draw */
    dwx_renderer_clear(r);
    {
        LONG io0 = dwx_refs(io), e0 = dwx_refs(e);
        CHECK_HR_OK(IDWriteTextLayout_Draw(l, (void *)0x1234, (IDWriteTextRenderer *)r, 5.0f, 6.0f));
        EXPECT_EQ(dwx_refs(r), 1);                     /* any temporary reference taken during Draw is gone */
        EXPECT_EQ(dwx_refs(io), io0); EXPECT_EQ(dwx_refs(e), e0);
        CHECK(r->count[DWX_DRAW_GLYPHRUN] >= 1);
        EXPECT_EQ(r->count[DWX_DRAW_INLINE], 1);
        {   /* the effect handed to the callbacks is the layout's own reference (at least ours + layout's) */
            int cur = 0; const dwx_draw *d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur);
            if (d) { CHECK(d->effect == (IUnknown *)e); CHECK(d->effect_refs >= 2); }
            cur = 0; d = dwx_renderer_next(r, DWX_DRAW_INLINE, &cur);
            if (d) { CHECK(d->obj == (IDWriteInlineObject *)io); CHECK(d->ctx == (void *)0x1234); }
        }
    }
    r->fail_draw = E_FAIL; r->fail_at = 1;             /* a failing callback must not leak whatever the layout passed out */
    {   LONG io0 = dwx_refs(io), e0 = dwx_refs(e);
        HRESULT hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0, 0);
        t_note("Draw with a failing renderer -> %s", dw_hr_name(hr));
        EXPECT_EQ(dwx_refs(r), 1); EXPECT_EQ(dwx_refs(io), io0); EXPECT_EQ(dwx_refs(e), e0); }
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(io), 1); EXPECT_EQ(dwx_refs(e), 1); EXPECT_EQ(dwx_refs(r), 1);
    dwx_release(r); dwx_release(io); dwx_release(e);
}
TEST(format_trimming_sign_ref_balance) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm = NULL; dwx_inline *s1 = dwx_inline_new(8, 8, 6, FALSE), *s2 = dwx_inline_new(9, 8, 6, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &fm);
    if (!fm) { t_skip("format"); dwx_release(s1); dwx_release(s2); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(fm, &tr, (IDWriteInlineObject *)s1));
    EXPECT_EQ(dwx_refs(s1), 2);
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(fm, &tr, (IDWriteInlineObject *)s2));
    EXPECT_EQ(dwx_refs(s1), 1); EXPECT_EQ(dwx_refs(s2), 2);
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(fm, &tr, NULL));
    EXPECT_EQ(dwx_refs(s2), 1);
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(fm, &tr, (IDWriteInlineObject *)s1));
    DW_RELEASE(fm);
    EXPECT_EQ(dwx_refs(s1), 1);
    dwx_release(s1); dwx_release(s2);
}

/* ---- analysis source / sink ownership ----------------------------------------------------------------------------------------- */
static void analyze_balance(int which) {
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; static const WCHAR txt[] = L"Hello world 123 \x05E9\x05DC\x05D5\x05DD";
    UINT32 len = (UINT32)lstrlenW(txt); dwx_ana_src *src; dwx_ana_sink *sink; HRESULT hr = S_OK;
    IDWriteNumberSubstitution *ns = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &an));
    if (!an) { t_skip("analyzer"); return; }
    src = dwx_ana_src_new(txt, len); sink = dwx_ana_sink_new();
    if (which == 2) { IDWriteFactory_CreateNumberSubstitution(f, DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE, L"en-US", FALSE, &ns); src->subst = ns; }
    switch (which) {
    case 0: hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sink); break;
    case 1: hr = IDWriteTextAnalyzer_AnalyzeBidi(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sink); break;
    case 2: hr = IDWriteTextAnalyzer_AnalyzeNumberSubstitution(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sink); break;
    case 3: hr = IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)src, 0, len, (IDWriteTextAnalysisSink *)sink); break;
    }
    CHECK_HR_OK(hr);
    EXPECT_EQ(dwx_refs(src), 1);                      /* the analyzer holds neither the source nor the sink after returning */
    EXPECT_EQ(dwx_refs(sink), 1);
    if (which == 0) CHECK(dwx_ana_covered(sink, 's', len));
    if (which == 1) CHECK(dwx_ana_covered(sink, 'l', len));
    if (which == 3) CHECK(dwx_ana_covered(sink, 'b', len));
    if (which == 2) { t_note("number substitution refs after analysis: %lu (sink retains: %s)", dw_refcount((IUnknown *)ns), dw_refcount((IUnknown *)ns) > 1 ? "yes" : "no"); }
    /* the same analyzer works again with fresh doubles */
    dwx_release(src); dwx_release(sink);
    EXPECT_EQ(dwx_destroyed(src), 1); EXPECT_EQ(dwx_destroyed(sink), 1);
    DW_RELEASE(ns);
    DW_RELEASE(an);
}
TEST(analysis_script_leaves_source_sink_balanced) { NEED_PROC(); analyze_balance(0); }
TEST(analysis_bidi_leaves_source_sink_balanced) { NEED_PROC(); analyze_balance(1); }
TEST(analysis_numsubst_leaves_source_sink_balanced) { NEED_PROC(); analyze_balance(2); }
TEST(analysis_linebreak_leaves_source_sink_balanced) { NEED_PROC(); analyze_balance(3); }
TEST(analysis_failure_paths_leave_balance) {
    NEED_PROC();
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; static const WCHAR txt[] = L"abc def";
    dwx_ana_src *src; dwx_ana_sink *sink; HRESULT hr;
    IDWriteFactory_CreateTextAnalyzer(f, &an);
    if (!an) { t_skip("analyzer"); return; }
    src = dwx_ana_src_new(txt, 7); sink = dwx_ana_sink_new();
    sink->fail_script = E_FAIL; sink->fail_break = E_FAIL; sink->fail_bidi = E_FAIL; sink->fail_subst = E_FAIL;
    hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, 7, (IDWriteTextAnalysisSink *)sink);
    CHECK(FAILED(hr));
    hr = IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)src, 0, 7, (IDWriteTextAnalysisSink *)sink);
    CHECK(FAILED(hr));
    hr = IDWriteTextAnalyzer_AnalyzeBidi(an, (IDWriteTextAnalysisSource *)src, 0, 7, (IDWriteTextAnalysisSink *)sink);
    CHECK(FAILED(hr));
    EXPECT_EQ(dwx_refs(src), 1); EXPECT_EQ(dwx_refs(sink), 1);
    src->fail_locale = E_FAIL;
    hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, 7, (IDWriteTextAnalysisSink *)sink);
    t_note("AnalyzeScript with failing GetLocaleName -> %s", dw_hr_name(hr));
    EXPECT_EQ(dwx_refs(src), 1); EXPECT_EQ(dwx_refs(sink), 1);
    dwx_release(src); dwx_release(sink); DW_RELEASE(an);
}

/* ---- glyph run analysis and the face ------------------------------------------------------------------------------------------- */
TEST(glyph_run_analysis_keeps_face_alive) {
    NEED_PROC();
    dwx_font h; IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGlyphRunAnalysis *a = NULL; DWRITE_GLYPH_RUN run; UINT16 g = 2; float adv = 12.0f;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(dwx_font_open_default(&h, f));
    if (!h.face) { dwx_font_close(&h); DW_RELEASE(f); t_skip("font"); return; }
    memset(&run, 0, sizeof run);
    run.fontFace = h.face; run.fontEmSize = 20.0f; run.glyphCount = 1; run.glyphIndices = &g; run.glyphAdvances = &adv;
    CHECK_HR_OK(IDWriteFactory_CreateGlyphRunAnalysis(f, &run, 1.0f, NULL, DWRITE_RENDERING_MODE_ALIASED, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &a));
    if (a) {
        RECT rc = { 0, 0, 0, 0 }; ULONG fr;
        fr = dw_refcount((IUnknown *)h.face);
        t_note("face refs with analysis alive: %lu", fr);
        run.fontFace = NULL;
        DW_RELEASE(h.face);                        /* the analysis no longer needs our reference (it holds its own, or has copied what it needs) */
        CHECK_HR_OK(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(a, DWRITE_TEXTURE_ALIASED_1x1, &rc));
        CHECK(rc.right > rc.left && rc.bottom > rc.top);   /* the 'A' box has ink */
        DW_RELEASE(a);
    }
    DW_RELEASE(h.face);
    dwx_font_close(&h);
    DW_RELEASE(f);
}
