/* IDWriteFactory3/6/7 GetSystemFontSet and GetSystemFontCollection (plus IDWriteFactory::GetSystemFontCollection) - the cache slots.
 * Win10 probing: every entry point writes *out = NULL first and returns S_OK; they funnel into
 *   IDWriteFactory::GetSystemFontCollection(model, includeDownloadable, forceUpdate): one cached collection object
 *   per slot (model == TYPOGRAPHIC ? slot A : slot B) x (includeDownloadable != 0), AddRef'd for each caller;
 *   IDWriteFactory::GetSystemFontCollection(c, check) == IDWriteFactory3 variant with includeDownloadable 0 (vtable call with edx = 0);
 *   IDWriteFactory3::GetSystemFontCollection(dl, c, check) uses model WEIGHT_STRETCH_STYLE (1);
 *   IDWriteFactory6/7::GetSystemFontCollection(dl, model, c) pass the model through (any non-zero value picks the WSS slot);
 *   IDWriteFactory::GetSystemFontSet(includeDownloadable): one cached IDWriteFontSet per flag;
 *   IDWriteFactory3::GetSystemFontSet(set) forwards to the IDWriteFactory6 vtable slot with includeDownloadable = 1. */
#include "g2.h"

#define FV(var, ver) IDWriteFactory##ver *var = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, ver); if (!var) { t_skip("IDWriteFactory" #ver " unsupported"); return; }

static HRESULT col_a(IDWriteFactory *f, BOOL chk, IDWriteFontCollection **c) { return IDWriteFactory_GetSystemFontCollection(f, c, chk); }

TEST(gap2_sys_collection_slot_matrix)
{
    FV(f7, 7);
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFactory6 *f6 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    IDWriteFactory *f = dw_shared();
    IDWriteFontCollection *a = NULL; IDWriteFontCollection1 *b[2] = { 0 }; IDWriteFontCollection2 *c[2][2] = { { 0 } }; IDWriteFontCollection3 *d[2][2] = { { 0 } };
    int dl, m;
    CHECK_HR_OK(col_a(f, FALSE, &a));
    for (dl = 0; dl < 2; dl++) {
        CHECK_HR_OK(IDWriteFactory3_GetSystemFontCollection(f3, dl, &b[dl], FALSE));
        for (m = 0; m < 2; m++) {
            CHECK_HR_OK(IDWriteFactory6_GetSystemFontCollection(f6, dl, (DWRITE_FONT_FAMILY_MODEL)m, &c[dl][m]));
            CHECK_HR_OK(IDWriteFactory7_GetSystemFontCollection(f7, dl, (DWRITE_FONT_FAMILY_MODEL)m, &d[dl][m]));
        }
    }
    for (dl = 0; dl < 2; dl++) for (m = 0; m < 2; m++) CHECKF(g2_ident(c[dl][m]) == g2_ident(d[dl][m]), "Factory6 and Factory7 differ for dl %d model %d", dl, m);
    for (dl = 0; dl < 2; dl++) CHECKF(g2_ident(b[dl]) == g2_ident(c[dl][1]), "Factory3 dl %d is not the WSS slot", dl);
    CHECK(g2_ident(a) == g2_ident(c[0][1]));
    /* four distinct slots */
    CHECK(g2_ident(c[0][0]) != g2_ident(c[0][1]) && g2_ident(c[0][0]) != g2_ident(c[1][0]) && g2_ident(c[0][0]) != g2_ident(c[1][1]));
    CHECK(g2_ident(c[0][1]) != g2_ident(c[1][0]) && g2_ident(c[0][1]) != g2_ident(c[1][1]) && g2_ident(c[1][0]) != g2_ident(c[1][1]));
    DW_RELEASE(a);
    for (dl = 0; dl < 2; dl++) { DW_RELEASE(b[dl]); for (m = 0; m < 2; m++) { DW_RELEASE(c[dl][m]); DW_RELEASE(d[dl][m]); } }
}

TEST(gap2_sys_collection_nonzero_bool_is_true)
{
    FV(f6, 6);
    IDWriteFontCollection2 *t = NULL, *x = NULL; static const BOOL v[] = { 2, 0x100, -1, 0x7fffffff, (BOOL)0x80000000 }; size_t i;
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontCollection(f6, TRUE, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &t));
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        x = NULL;
        CHECK_HR_OK(IDWriteFactory6_GetSystemFontCollection(f6, v[i], DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &x));
        CHECKF(g2_ident(x) == g2_ident(t), "includeDownloadable %d picked another slot", (int)v[i]);
        DW_RELEASE(x);
    }
    DW_RELEASE(t);
}

TEST(gap2_sys_collection_nonzero_model_is_wss_slot)
{
    FV(f6, 6);
    IDWriteFontCollection2 *w = NULL; static const UINT32 models[] = { 1, 2, 3, 7, 255, 0x10000, 0x7fffffff, 0xFFFFFFFFu }; size_t i;
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &w));
    for (i = 0; i < sizeof models / sizeof models[0]; i++) {
        IDWriteFontCollection2 *x = (IDWriteFontCollection2 *)G2_SENT;
        HRESULT hr = IDWriteFactory6_GetSystemFontCollection(f6, FALSE, (DWRITE_FONT_FAMILY_MODEL)models[i], &x);
        if (SUCCEEDED(hr)) { CHECKF(x != NULL && x != G2_SENT, "model %u", models[i]); if (x && x != G2_SENT) { G2_SOFT(g2_ident(x) == g2_ident(w)); IDWriteFontCollection2_Release(x); } }
        else { CHECKF(x == NULL, "model %u: failure left %p", models[i], (void *)x); t_note("model %u rejected: %s", models[i], dw_hr_name(hr)); }
    }
    DW_RELEASE(w);
}

TEST(gap2_sys_collection_family_model_reported)
{
    FV(f6, 6);
    int m;
    for (m = 0; m < 2; m++) {
        IDWriteFontCollection2 *c = NULL;
        CHECK_HR_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, (DWRITE_FONT_FAMILY_MODEL)m, &c));
        if (c) { CHECKF(IDWriteFontCollection2_GetFontFamilyModel(c) == (DWRITE_FONT_FAMILY_MODEL)m, "model %d reports %d", m, (int)IDWriteFontCollection2_GetFontFamilyModel(c)); IDWriteFontCollection2_Release(c); }
    }
}

TEST(gap2_sys_collection_factory3_reports_wss_model)
{
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontCollection1 *c = NULL; IDWriteFontCollection2 *c2 = NULL;
    if (!f3) { t_skip("Factory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &c, FALSE));
    if (c && SUCCEEDED(IDWriteFontCollection1_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2)))
        CHECK(IDWriteFontCollection2_GetFontFamilyModel(c2) == DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE);
    else t_note("collection from Factory3 lacks IDWriteFontCollection2");
    DW_RELEASE(c2); DW_RELEASE(c);
}

TEST(gap2_sys_collection_acquire_adds_exactly_one_reference)
{
    FV(f7, 7);
    IDWriteFontCollection3 *a = NULL, *b = NULL; ULONG r0, r1, r2;
    CHECK_HR_OK(IDWriteFactory7_GetSystemFontCollection(f7, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &a));
    if (!a) return;
    r0 = dw_refcount((IUnknown *)a);
    CHECK_HR_OK(IDWriteFactory7_GetSystemFontCollection(f7, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &b));
    r1 = dw_refcount((IUnknown *)a);
    DW_RELEASE(b);
    r2 = dw_refcount((IUnknown *)a);
    CHECKF(r1 == r0 + 1, "second acquisition: %lu -> %lu", r0, r1);
    CHECKF(r2 == r0, "after release: %lu", r2);
    DW_RELEASE(a);
}

TEST(gap2_sys_collection_check_for_updates_variants)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *base = NULL; static const BOOL v[] = { FALSE, TRUE, 2, -1 }; size_t i; UINT32 n0;
    CHECK_HR_OK(col_a(f, FALSE, &base));
    if (!base) return;
    n0 = IDWriteFontCollection_GetFontFamilyCount(base);
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteFontCollection *c = (IDWriteFontCollection *)G2_SENT;
        HRESULT hr = col_a(f, v[i], &c);
        CHECK_HR(hr, S_OK);
        if (SUCCEEDED(hr) && c != G2_SENT) { CHECKF(IDWriteFontCollection_GetFontFamilyCount(c) == n0, "check %d: family count %u vs %u", (int)v[i], IDWriteFontCollection_GetFontFamilyCount(c), n0); DW_RELEASE(c); }
    }
    DW_RELEASE(base);
}

TEST(gap2_sys_collection_factory3_check_flag_variants)
{
    FV(f3, 3);
    static const BOOL v[] = { FALSE, TRUE, 3 }; int dl; size_t i;
    for (dl = 0; dl < 2; dl++) for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteFontCollection1 *c = (IDWriteFontCollection1 *)G2_SENT;
        HRESULT hr = IDWriteFactory3_GetSystemFontCollection(f3, dl, &c, v[i]);
        CHECKF(hr == S_OK && c != NULL && c != G2_SENT, "dl %d check %d: %s", dl, (int)v[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && c != G2_SENT) DW_RELEASE(c);
    }
}

TEST(gap2_sys_collection_isolated_factory_has_own_slots)
{
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED), *sh = dw_shared(); IDWriteFontCollection *a = NULL, *b = NULL, *a2 = NULL;
    if (!iso) { t_skip("isolated factory unavailable"); return; }
    CHECK_HR_OK(col_a(iso, FALSE, &a)); CHECK_HR_OK(col_a(iso, FALSE, &a2)); CHECK_HR_OK(col_a(sh, FALSE, &b));
    CHECK(a && a2 && a == a2);
    CHECK(g2_ident(a) != g2_ident(b));               /* the cache lives inside each factory */
    if (a && b) CHECK(IDWriteFontCollection_GetFontFamilyCount(a) == IDWriteFontCollection_GetFontFamilyCount(b));
    DW_RELEASE(a); DW_RELEASE(a2); DW_RELEASE(b); DW_RELEASE(iso);
}

TEST(gap2_sys_collection_outlives_factory_release)
{
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontCollection *c = NULL;
    if (!iso) { t_skip("isolated factory unavailable"); return; }
    CHECK_HR_OK(col_a(iso, FALSE, &c));
    DW_RELEASE(iso);
    if (c) { CHECK(IDWriteFontCollection_GetFontFamilyCount(c) > 0); DW_RELEASE(c); }
}

/* ---- system font sets ---------------------------------------------------------------------------------------------- */
TEST(gap2_sys_set_counts_relations)
{
    FV(f6, 6);
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    IDWriteFontSet1 *loc = NULL, *all = NULL; IDWriteFontSet *s3 = NULL;
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, FALSE, &loc));
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, TRUE, &all));
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s3));
    if (loc && all && s3) {
        UINT32 nl = IDWriteFontSet1_GetFontCount(loc), na = IDWriteFontSet1_GetFontCount(all), n3 = IDWriteFontSet_GetFontCount(s3);
        t_note("system set: local %u, with downloadable %u, Factory3 %u", nl, na, n3);
        CHECK(nl > 0);
        CHECK(na >= nl);
        CHECKF(n3 == na, "Factory3 forwards with includeDownloadable = 1: %u vs %u", n3, na);
    }
    DW_RELEASE(loc); DW_RELEASE(all); DW_RELEASE(s3);
}

TEST(gap2_sys_set_cached_per_flag)
{
    FV(f6, 6);
    int dl;
    for (dl = 0; dl < 2; dl++) {
        IDWriteFontSet1 *a = NULL, *b = NULL;
        CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, dl, &a)); CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, dl, &b));
        CHECKF(a != NULL && g2_ident(a) == g2_ident(b), "includeDownloadable %d returned %p and %p", dl, (void *)a, (void *)b);
        DW_RELEASE(a); DW_RELEASE(b);
    }
}

TEST(gap2_sys_set_nonzero_bool_is_true)
{
    FV(f6, 6);
    IDWriteFontSet1 *t = NULL; static const BOOL v[] = { 2, -1, 0x100 }; size_t i;
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, TRUE, &t));
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        IDWriteFontSet1 *x = NULL;
        CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, v[i], &x));
        CHECKF(g2_ident(x) == g2_ident(t), "BOOL %d chose a different set", (int)v[i]);
        DW_RELEASE(x);
    }
    DW_RELEASE(t);
}

TEST(gap2_sys_set_factory7_matches_factory6)
{
    FV(f7, 7);
    IDWriteFactory6 *f6 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6); int dl;
    for (dl = 0; dl < 2; dl++) {
        IDWriteFontSet1 *a = NULL; IDWriteFontSet2 *b = NULL;
        CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, dl, &a)); CHECK_HR_OK(IDWriteFactory7_GetSystemFontSet(f7, dl, &b));
        if (a && b) {
            CHECKF(g2_ident(a) == g2_ident(b), "dl %d: Factory6 and Factory7 hand out different set objects", dl);
            CHECK(IDWriteFontSet1_GetFontCount(a) == IDWriteFontSet2_GetFontCount(b));
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
}

TEST(gap2_sys_set_qi_matrix)
{
    FV(f7, 7);
    IDWriteFontSet2 *s = NULL;
    CHECK_HR_OK(IDWriteFactory7_GetSystemFontSet(f7, FALSE, &s));
    if (s) {
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontSet, 1), DW_QI_ROW(IDWriteFontSet1, 1), DW_QI_ROW(IDWriteFontSet2, 1), DW_QI_ROW(IDWriteFontSet3, -1), DW_QI_ROW(IDWriteFontSet4, -1),
                             DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontSetBuilder, 0) };
        DW_CHECK_QI(s, rows);
        DW_RELEASE(s);
    }
}

TEST(gap2_sys_set_local_set_fonts_are_local)
{
    FV(f6, 6);
    IDWriteFontSet1 *s = NULL; UINT32 n, i, step, bad = 0;
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, FALSE, &s));
    if (!s) return;
    n = IDWriteFontSet1_GetFontCount(s); step = n / 40 + 1;
    for (i = 0; i < n; i += step) if (IDWriteFontSet1_GetFontLocality(s, i) != DWRITE_LOCALITY_LOCAL) bad++;
    CHECKF(bad == 0, "%u non-local entries in the includeDownloadable = FALSE set", bad);
    DW_RELEASE(s);
}

TEST(gap2_sys_set_acquire_adds_exactly_one_reference)
{
    FV(f7, 7);
    IDWriteFontSet2 *a = NULL, *b = NULL; ULONG r0, r1, r2;
    CHECK_HR_OK(IDWriteFactory7_GetSystemFontSet(f7, TRUE, &a));
    if (!a) return;
    r0 = dw_refcount((IUnknown *)a);
    CHECK_HR_OK(IDWriteFactory7_GetSystemFontSet(f7, TRUE, &b));
    r1 = dw_refcount((IUnknown *)a);
    DW_RELEASE(b);
    r2 = dw_refcount((IUnknown *)a);
    CHECKF(r1 == r0 + 1 && r2 == r0, "%lu %lu %lu", r0, r1, r2);
    DW_RELEASE(a);
}

TEST(gap2_sys_set_matches_collection_font_set)
{
    FV(f6, 6);
    IDWriteFontCollection2 *c = NULL; IDWriteFontSet1 *cs = NULL, *ss = NULL;
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &c));
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, FALSE, &ss));
    if (c && ss && SUCCEEDED(IDWriteFontCollection2_GetFontSet(c, &cs)) && cs) {
        UINT32 a = IDWriteFontSet1_GetFontCount(cs), b = IDWriteFontSet1_GetFontCount(ss);
        t_note("collection font set %u, system set %u", a, b);
        G2_SOFT(a == b);
        CHECK(a > 0);
    }
    DW_RELEASE(cs); DW_RELEASE(ss); DW_RELEASE(c);
}

TEST(gap2_sys_set_isolated_factory_independent)
{
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory6 *f6 = NULL; IDWriteFontSet1 *a = NULL, *b = NULL;
    if (!iso) { t_skip("isolated factory unavailable"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(iso, &IID_IDWriteFactory6, (void **)&f6))) { DW_RELEASE(iso); t_skip("Factory6 unsupported"); return; }
    {
        IDWriteFactory6 *sh = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
        CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, FALSE, &a)); CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(sh, FALSE, &b));
        if (a && b) { CHECK(a != b); CHECK(IDWriteFontSet1_GetFontCount(a) == IDWriteFontSet1_GetFontCount(b)); }
    }
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(f6); DW_RELEASE(iso);
}

TEST(gap2_sys_set_outlives_factory_release)
{
    IDWriteFactory *iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory3 *f3 = NULL; IDWriteFontSet *s = NULL;
    if (!iso) { t_skip("isolated factory unavailable"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(iso, &IID_IDWriteFactory3, (void **)&f3))) { DW_RELEASE(iso); t_skip("Factory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &s));
    DW_RELEASE(f3); DW_RELEASE(iso);
    if (s) { CHECK(IDWriteFontSet_GetFontCount(s) > 0); DW_RELEASE(s); }
}

TEST(gap2_sys_set_collection_font_resolves_in_set)
{
    /* Round trip: a font from the system collection can be found in the system set through its face reference. */
    IDWriteFactory *f = dw_shared(); IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontCollection *sc = NULL; IDWriteFont *font; IDWriteFont3 *font3 = NULL;
    IDWriteFontSet *set = NULL; IDWriteFontFaceReference *ref = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (!f3) { t_skip("Factory3 unsupported"); return; }
    IDWriteFactory_GetSystemFontCollection(f, &sc, FALSE);
    font = sc ? g2_font(sc, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    if (!font) { DW_RELEASE(sc); t_skip("Arial not installed"); return; }
    if (SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont3, (void **)&font3)) && SUCCEEDED(IDWriteFont3_GetFontFaceReference(font3, &ref)) && ref &&
        SUCCEEDED(IDWriteFactory3_GetSystemFontSet(f3, &set)) && set) {
        CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, ref, &idx, &ex));
        CHECK(ex == TRUE);
        if (ex) CHECK(idx < IDWriteFontSet_GetFontCount(set));
    }
    DW_RELEASE(set); DW_RELEASE(ref); DW_RELEASE(font3); DW_RELEASE(font); DW_RELEASE(sc);
}
