/* Names across several fonts of a custom collection, IDWriteStringList from font sets, concurrent readers. */
#include "lsfx.h"
#include <process.h>

typedef struct {
    dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFactory *f; IDWriteFontCollection *coll;
} ls_coll;
static HRESULT coll_open(ls_coll *c, IDWriteFactory *f, uint8_t *const *blobs, const size_t *sizes, int n) {
    int i; HRESULT hr;
    memset(c, 0, sizeof *c); c->f = f;
    c->fl = dwx_loader_new();
    c->cl = dwx_coll_new(c->fl);
    for (i = 0; i < n; i++) dwx_coll_add_key(c->cl, dwx_loader_add(c->fl, blobs[i], sizes[i]));
    hr = dwx_loader_register(c->fl, f); if (FAILED(hr)) return hr;
    hr = dwx_coll_register(c->cl, f); if (FAILED(hr)) return hr;
    return dwx_coll_create(c->cl, f, &c->coll);
}
static void coll_close(ls_coll *c) {
    DW_RELEASE(c->coll);
    if (c->cl) { dwx_coll_unregister(c->cl); dwx_release(c->cl); c->cl = NULL; }
    if (c->fl) { dwx_loader_unregister(c->fl); dwx_release(c->fl); c->fl = NULL; }
}

/* ---- RIBBI family ------------------------------------------------------------------------------------------ */
typedef struct { int weight, italic; const char *face; } ribbi_row;
static const ribbi_row ribbi[4] = { { 400, 0, "Regular" }, { 700, 0, "Bold" }, { 400, 1, "Italic" }, { 700, 1, "Bold Italic" } };

static HRESULT open_ribbi(ls_coll *c, IDWriteFactory *f, const char *family) {
    uint8_t *blobs[4]; size_t sizes[4]; int i; HRESULT hr;
    for (i = 0; i < 4; i++) blobs[i] = ttf_build_styled(family, ribbi[i].weight, 5, ribbi[i].italic, &sizes[i]);
    hr = coll_open(c, f, blobs, sizes, 4);
    for (i = 0; i < 4; i++) free(blobs[i]);
    return hr;
}
TEST(lsc_ribbi_family_layout) {
    LS_NEED_FACTORY(); ls_coll c; IDWriteFontFamily *ff = NULL; IDWriteLocalizedStrings *s = NULL; WCHAR *e;
    HRESULT hr = open_ribbi(&c, f, "LsRibbi");
    if (FAILED(hr)) { coll_close(&c); t_skip("collection: %s", dw_hr_name(hr)); return; }
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c.coll), 1);
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c.coll, 0, &ff));
    if (ff) {
        CHECK(IDWriteFontFamily_GetFontCount(ff) >= 4);          /* simulated faces may be listed in addition */
        t_note("font count %u", IDWriteFontFamily_GetFontCount(ff));
        CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(ff, &s));
        e = s ? ls_en(s) : NULL; CHECK(e && !lstrcmpW(e, L"LsRibbi")); free(e);
        if (s) { EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 1); EXPECT_EQ(ls_sweep(s, "ribbi family"), 0); }
        DW_RELEASE(s); IDWriteFontFamily_Release(ff);
    }
    coll_close(&c);
}
TEST(lsc_ribbi_face_names) {
    LS_NEED_FACTORY(); ls_coll c; IDWriteFontFamily *ff = NULL; UINT32 i;
    HRESULT hr = open_ribbi(&c, f, "LsRibbi");
    if (FAILED(hr)) { coll_close(&c); t_skip("collection: %s", dw_hr_name(hr)); return; }
    IDWriteFontCollection_GetFontFamily(c.coll, 0, &ff);
    if (ff) {
        int seen[4] = { 0, 0, 0, 0 };
        for (i = 0; i < IDWriteFontFamily_GetFontCount(ff) && i < 16; i++) {
            IDWriteFont *font = NULL; IDWriteLocalizedStrings *s = NULL; WCHAR *e; int k, matched = -1; WCHAR want[32];
            CHECK_HR_OK(IDWriteFontFamily_GetFont(ff, i, &font));
            if (!font) continue;
            if (IDWriteFont_GetSimulations(font) != DWRITE_FONT_SIMULATIONS_NONE) { IDWriteFont_Release(font); continue; }
            CHECK_HR_OK(IDWriteFont_GetFaceNames(font, &s));
            e = s ? ls_en(s) : NULL;
            for (k = 0; k < 4; k++) {
                dw_wide(ribbi[k].face, want, 32);
                if (e && !lstrcmpW(e, want)) {
                    matched = k;
                    /* the face name must agree with the weight / style the font object reports */
                    EXPECT_EQ(IDWriteFont_GetWeight(font), ribbi[k].weight);
                    EXPECT_EQ(IDWriteFont_GetStyle(font), ribbi[k].italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL);
                }
            }
            CHECKF(matched >= 0, "font %u face name \"%ls\"", i, e ? e : L"(none)");
            if (matched >= 0) seen[matched]++;
            free(e); DW_RELEASE(s); IDWriteFont_Release(font);
        }
        for (i = 0; i < 4; i++) EXPECT_EQ(seen[i], 1);
        IDWriteFontFamily_Release(ff);
    }
    coll_close(&c);
}
TEST(lsc_ribbi_matching_font_face_names) {
    LS_NEED_FACTORY(); ls_coll c; IDWriteFontFamily *ff = NULL; int k;
    HRESULT hr = open_ribbi(&c, f, "LsRibbi");
    if (FAILED(hr)) { coll_close(&c); t_skip("collection: %s", dw_hr_name(hr)); return; }
    IDWriteFontCollection_GetFontFamily(c.coll, 0, &ff);
    if (ff) {
        for (k = 0; k < 4; k++) {
            IDWriteFont *font = NULL; IDWriteLocalizedStrings *s = NULL; WCHAR *e, want[32];
            CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(ff, (DWRITE_FONT_WEIGHT)ribbi[k].weight, DWRITE_FONT_STRETCH_NORMAL,
                ribbi[k].italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL, &font));
            if (!font) continue;
            IDWriteFont_GetFaceNames(font, &s);
            e = s ? ls_en(s) : NULL; dw_wide(ribbi[k].face, want, 32);
            CHECKF(e && !lstrcmpW(e, want), "weight %d italic %d -> \"%ls\"", ribbi[k].weight, ribbi[k].italic, e ? e : L"(none)");
            free(e); DW_RELEASE(s); IDWriteFont_Release(font);
        }
        IDWriteFontFamily_Release(ff);
    }
    coll_close(&c);
}
TEST(lsc_ribbi_face3_names_match_font_names) {
    LS_NEED_FACTORY(); ls_coll c; IDWriteFontFamily *ff = NULL; UINT32 i;
    HRESULT hr = open_ribbi(&c, f, "LsRibbi");
    if (FAILED(hr)) { coll_close(&c); t_skip("collection: %s", dw_hr_name(hr)); return; }
    IDWriteFontCollection_GetFontFamily(c.coll, 0, &ff);
    if (ff) {
        for (i = 0; i < IDWriteFontFamily_GetFontCount(ff) && i < 16; i++) {
            IDWriteFont *font = NULL; IDWriteFontFace *face = NULL; IDWriteFontFace3 *f3 = NULL; IDWriteLocalizedStrings *a = NULL, *b = NULL, *fa = NULL, *fb = NULL;
            IDWriteFontFamily_GetFont(ff, i, &font);
            if (!font) continue;
            if (IDWriteFont_GetSimulations(font) != DWRITE_FONT_SIMULATIONS_NONE) { IDWriteFont_Release(font); continue; }
            IDWriteFont_CreateFontFace(font, &face);
            if (face && SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3))) {
                WCHAR *x, *y;
                IDWriteFont_GetFaceNames(font, &a); IDWriteFontFace3_GetFaceNames(f3, &b);
                x = a ? ls_en(a) : NULL; y = b ? ls_en(b) : NULL; CHECK(x && y && !lstrcmpW(x, y)); free(x); free(y);
                IDWriteFont_GetFaceNames(font, &fa); IDWriteFontFace3_GetFamilyNames(f3, &fb);
                x = fa ? ls_en(fa) : NULL; y = fb ? ls_en(fb) : NULL;
                CHECK(y && !lstrcmpW(y, L"LsRibbi"));
                free(x); free(y);
                DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(fa); DW_RELEASE(fb);
                IDWriteFontFace3_Release(f3);
            }
            DW_RELEASE(face); IDWriteFont_Release(font);
        }
        IDWriteFontFamily_Release(ff);
    }
    coll_close(&c);
}
TEST(lsc_find_family_name_variants) {
    LS_NEED_FACTORY(); ls_coll c; UINT32 idx; BOOL ex; size_t k;
    static const WCHAR *const hit[] = { L"LsRibbi", L"lsribbi", L"LSRIBBI", L"lSrIbBi" };
    static const WCHAR *const miss[] = { L"LsRibb", L"LsRibbi ", L" LsRibbi", L"LsRibbi2", L"", L"Ls", L"LsRibbiLsRibbi" };
    HRESULT hr = open_ribbi(&c, f, "LsRibbi");
    if (FAILED(hr)) { coll_close(&c); t_skip("collection: %s", dw_hr_name(hr)); return; }
    for (k = 0; k < 4; k++) { idx = 9; ex = FALSE; CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c.coll, hit[k], &idx, &ex)); CHECK(ex == TRUE); EXPECT_EQ(idx, 0); }
    for (k = 0; k < 7; k++) { idx = 9; ex = TRUE; CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c.coll, miss[k], &idx, &ex)); CHECK(ex == FALSE); EXPECT_EQ(idx, 0xFFFFFFFFu); }
    coll_close(&c);
}
TEST(lsc_localized_family_name_resolves) {
    /* every name in GetFamilyNames (all locales) finds the family */
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s = NULL; UINT32 i, n;
    HRESULT hr = ls_open_multi_coll(&x, f);
    if (FAILED(hr)) { ls_close(&x); t_skip("fixture"); return; }
    CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(x.fam, &s));
    if (s) {
        n = IDWriteLocalizedStrings_GetCount(s);
        for (i = 0; i < n; i++) {
            WCHAR *nm = ls_dup_string(s, i); UINT32 idx = 5; BOOL ex = FALSE;
            if (nm) {
                CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(x.coll, nm, &idx, &ex));
                CHECKF(ex == TRUE, "family name \"%ls\" (entry %u) not found", nm, i);
                if (ex) EXPECT_EQ(idx, 0);
            }
            free(nm);
        }
        DW_RELEASE(s);
    }
    ls_close(&x);
}
TEST(lsc_two_families_independent_names) {
    LS_NEED_FACTORY(); ls_coll c; uint8_t *b[2]; size_t sz[2]; HRESULT hr; UINT32 i; int seenA = 0, seenB = 0;
    b[0] = ttf_build_styled("LsAlpha", 400, 5, 0, &sz[0]); b[1] = ttf_build_styled("LsBeta", 400, 5, 0, &sz[1]);
    hr = coll_open(&c, f, b, sz, 2); free(b[0]); free(b[1]);
    if (FAILED(hr)) { coll_close(&c); t_skip("collection: %s", dw_hr_name(hr)); return; }
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c.coll), 2);
    for (i = 0; i < 2; i++) {
        IDWriteFontFamily *ff = NULL; IDWriteLocalizedStrings *s = NULL; WCHAR *e;
        IDWriteFontCollection_GetFontFamily(c.coll, i, &ff);
        if (!ff) continue;
        IDWriteFontFamily_GetFamilyNames(ff, &s);
        e = s ? ls_en(s) : NULL;
        if (e && !lstrcmpW(e, L"LsAlpha")) { seenA++; t_note("LsAlpha at family index %u", i); }
        else if (e && !lstrcmpW(e, L"LsBeta")) { seenB++; t_note("LsBeta at family index %u", i); }
        else CHECK(0);
        free(e); DW_RELEASE(s); IDWriteFontFamily_Release(ff);
    }
    EXPECT_EQ(seenA, 1); EXPECT_EQ(seenB, 1);
    coll_close(&c);
}
TEST(lsc_weight_ladder_names) {
    /* non-RIBBI weights: legacy family "Fam Light" / typographic family "Fam" and style "Light" */
    LS_NEED_FACTORY(); ls_coll c; uint8_t *b[5]; size_t sz[5]; static const int w[5] = { 300, 400, 500, 600, 900 }; int i; HRESULT hr;
    for (i = 0; i < 5; i++) b[i] = ttf_build_styled("LsLadder", w[i], 5, 0, &sz[i]);
    hr = coll_open(&c, f, b, sz, 5);
    for (i = 0; i < 5; i++) free(b[i]);
    if (FAILED(hr)) { coll_close(&c); t_skip("collection: %s", dw_hr_name(hr)); return; }
    t_note("weight ladder collection: %u families", IDWriteFontCollection_GetFontFamilyCount(c.coll));
    for (UINT32 fi = 0; fi < IDWriteFontCollection_GetFontFamilyCount(c.coll) && fi < 8; fi++) {
        IDWriteFontFamily *ff = NULL; IDWriteLocalizedStrings *s = NULL; WCHAR *e;
        IDWriteFontCollection_GetFontFamily(c.coll, fi, &ff);
        if (!ff) continue;
        IDWriteFontFamily_GetFamilyNames(ff, &s); e = s ? ls_en(s) : NULL;
        t_note("family %u: %ls (%u fonts)", fi, e ? e : L"?", IDWriteFontFamily_GetFontCount(ff));
        CHECK(e && wcsstr(e, L"LsLadder"));
        free(e); DW_RELEASE(s);
        for (UINT32 j = 0; j < IDWriteFontFamily_GetFontCount(ff) && j < 8; j++) {
            IDWriteFont *font = NULL;
            IDWriteFontFamily_GetFont(ff, j, &font);
            if (font) { IDWriteFont_GetFaceNames(font, &s); e = s ? ls_en(s) : NULL; t_note("  font %u weight %d face \"%ls\"", j, (int)IDWriteFont_GetWeight(font), e ? e : L"?"); CHECK(e && e[0]); free(e); DW_RELEASE(s); IDWriteFont_Release(font); }
        }
        IDWriteFontFamily_Release(ff);
    }
    coll_close(&c);
}

/* ---- IDWriteStringList from font sets ---------------------------------------------------------------------------------------------- */
TEST(lsl_system_set_property_list) {
    LS_NEED_FACTORY(); IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *set = NULL; IDWriteStringList *l = NULL; UINT32 n, i;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &set));
    if (!set) return;
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues__(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, &l));
    if (l) {
        n = IDWriteStringList_GetCount(l);
        CHECK(n > 0);
        for (i = 0; i < n && i < 300; i++) {
            UINT32 sl = 0xAAAA, ll = 0xAAAA; WCHAR b[300], lb[100];
            CHECK_HR_OK(IDWriteStringList_GetStringLength(l, i, &sl));
            CHECK_HR_OK(IDWriteStringList_GetLocaleNameLength(l, i, &ll));
            if (sl < 250) { CHECK_HR_OK(IDWriteStringList_GetString(l, i, b, 300)); EXPECT_EQ((UINT32)lstrlenW(b), sl);
                            if (sl) CHECK_HR(IDWriteStringList_GetString(l, i, b, sl), DW_E_NOT_SUFFICIENT_BUFFER); }
            if (ll < 90) { CHECK_HR_OK(IDWriteStringList_GetLocaleName(l, i, lb, 100)); EXPECT_EQ((UINT32)lstrlenW(lb), ll); }
            CHECK_HR(IDWriteStringList_GetString(l, i, NULL, 0), DW_E_NOT_SUFFICIENT_BUFFER);
        }
        { UINT32 len = 5; HRESULT hr = IDWriteStringList_GetStringLength(l, n, &len); CHECK(FAILED(hr)); EXPECT_EQ(len, 0xFFFFFFFFu); }
        { UINT32 len = 5; HRESULT hr = IDWriteStringList_GetLocaleNameLength(l, n, &len); CHECK(FAILED(hr)); EXPECT_EQ(len, 0xFFFFFFFFu); }
        EXPECT_REFS(l, 1);
        DW_RELEASE(l);
    }
    IDWriteFontSet_Release(set);
}
TEST(lsl_system_set_property_list_with_locales) {
    LS_NEED_FACTORY(); IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *set = NULL; IDWriteStringList *l = NULL, *l2 = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &set));
    if (!set) return;
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues__(set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &l));
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues_(set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, L"en-us", &l2));
    if (l && l2) {
        t_note("full names: %u without locale filter, %u with en-us", IDWriteStringList_GetCount(l), IDWriteStringList_GetCount(l2));
        CHECK(IDWriteStringList_GetCount(l2) > 0);
        CHECK(IDWriteStringList_GetCount(l2) <= IDWriteStringList_GetCount(l));
        CHECK(l != l2);
    }
    DW_RELEASE(l); DW_RELEASE(l2); IDWriteFontSet_Release(set);
}
TEST(lsl_list_is_not_a_localized_strings) {
    /* a string list produced by a font set answers IDWriteStringList; localized strings objects are a different interface */
    LS_NEED_FACTORY(); IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *set = NULL; IDWriteStringList *l = NULL; void *p = (void *)1;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &set));
    if (!set) return;
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues__(set, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &l));
    if (l) {
        CHECK_HR(IDWriteStringList_QueryInterface(l, &IID_IDWriteFont, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
        p = (void *)1;
        HRESULT hr = IDWriteStringList_QueryInterface(l, &IID_IDWriteLocalizedStrings, &p);
        t_note("string list QI(IDWriteLocalizedStrings) -> %s", dw_hr_name(hr));
        if (SUCCEEDED(hr)) IUnknown_Release((IUnknown *)p); else CHECK_OUT_NULL(p);
        DW_RELEASE(l);
    }
    IDWriteFontSet_Release(set);
}

/* ---- concurrent readers ---------------------------------------------------------------------------------------------------------------------- */
typedef struct { IDWriteLocalizedStrings *s; volatile LONG bad; int iters; } thr_ctx;
static unsigned __stdcall reader(void *arg) {
    thr_ctx *c = (thr_ctx *)arg; int it;
    for (it = 0; it < c->iters; it++) {
        UINT32 n = IDWriteLocalizedStrings_GetCount(c->s), i;
        if (n != 5) InterlockedIncrement(&c->bad);
        for (i = 0; i < n && i < 5; i++) {
            UINT32 len = 0, idx = 0; BOOL ex = FALSE; WCHAR b[64], l[32];
            if (IDWriteLocalizedStrings_GetStringLength(c->s, i, &len) != S_OK || len >= 60) { InterlockedIncrement(&c->bad); continue; }
            if (IDWriteLocalizedStrings_GetString(c->s, i, b, 64) != S_OK || (UINT32)lstrlenW(b) != len) InterlockedIncrement(&c->bad);
            if (IDWriteLocalizedStrings_GetLocaleName(c->s, i, l, 32) != S_OK) InterlockedIncrement(&c->bad);
            else if (IDWriteLocalizedStrings_FindLocaleName(c->s, l, &idx, &ex) != S_OK || !ex || idx != i) InterlockedIncrement(&c->bad);
        }
    }
    return 0;
}
TEST(lsx_concurrent_readers_one_object) {
    LS_NEED_FACTORY(); LS_NEED_MULTI(x); IDWriteLocalizedStrings *s = NULL; HANDLE th[8]; thr_ctx c; int i;
    CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.cface3, &s));
    if (!s) { ls_close(&x); return; }
    c.s = s; c.bad = 0; c.iters = 300;
    for (i = 0; i < 8; i++) th[i] = (HANDLE)_beginthreadex(NULL, 0, reader, &c, 0, NULL);
    for (i = 0; i < 8; i++) if (th[i]) { CHECK(WaitForSingleObject(th[i], 20000) == WAIT_OBJECT_0); CloseHandle(th[i]); }
    EXPECT_EQ(c.bad, 0);
    EXPECT_REFS(s, 1);
    DW_RELEASE(s); ls_close(&x);
}
typedef struct { ls_fx *x; volatile LONG bad; int iters; } thr_ctx2;
static unsigned __stdcall fetcher(void *arg) {
    thr_ctx2 *c = (thr_ctx2 *)arg; int it;
    for (it = 0; it < c->iters; it++) {
        IDWriteLocalizedStrings *s = NULL; WCHAR *e;
        if (IDWriteFontFace3_GetFamilyNames(c->x->cface3, &s) != S_OK || !s) { InterlockedIncrement(&c->bad); continue; }
        e = ls_en(s);
        if (!e || lstrcmpW(e, L"LsFam")) InterlockedIncrement(&c->bad);
        free(e); IDWriteLocalizedStrings_Release(s);
        s = NULL;
        if (IDWriteFont_GetFaceNames(c->x->font, &s) != S_OK || !s) { InterlockedIncrement(&c->bad); continue; }
        if (IDWriteLocalizedStrings_GetCount(s) != 5) InterlockedIncrement(&c->bad);
        IDWriteLocalizedStrings_Release(s);
    }
    return 0;
}
TEST(lsx_concurrent_fetchers_one_face) {
    LS_NEED_FACTORY(); LS_NEED_MULTI(x); HANDLE th[8]; thr_ctx2 c; int i;
    c.x = &x; c.bad = 0; c.iters = 200;
    for (i = 0; i < 8; i++) th[i] = (HANDLE)_beginthreadex(NULL, 0, fetcher, &c, 0, NULL);
    for (i = 0; i < 8; i++) if (th[i]) { CHECK(WaitForSingleObject(th[i], 20000) == WAIT_OBJECT_0); CloseHandle(th[i]); }
    EXPECT_EQ(c.bad, 0);
    ls_close(&x);
}
