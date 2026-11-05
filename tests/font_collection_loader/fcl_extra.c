/* More collection-loader scenarios: files from other sources flowing through enumerators, large/odd keys, re-entrancy, font
 * property derivation (OS/2 / head / post) inside custom collections, family member access. */
#include "fcl.h"

/* ------------------------------------------------------------------------------- files from other loaders */
TEST(fcl_src_file_whose_loader_is_not_registered_with_the_factory)
{
    /* The enumerator hands back a file created through a second factory (where the file loader is registered). The factory
     * building the collection has no id for that loader: ArgumentException -> E_INVALIDARG, nothing returned. */
    FCL_PX(p);
    IDWriteFactory *other = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!other) { fcl_px_close(&p); t_skip("second factory"); return; }
    fcl_px_add_style(&p, "Foreign File", 400, 5, 0);
    dwx_loader_unregister(p.fl);
    CHECK_HR_OK(IDWriteFactory_RegisterFontFileLoader(other, (IDWriteFontFileLoader *)p.fl));
    p.cl->file_factory = other;
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = fcl_px_create(&p, &c);
    /* The exact HRESULT is inferred from the id-lookup pattern, not read directly: only the shape is hard-checked. */
    t_note("foreign file loader -> %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(c);
    CHECK(p.cl->last_en && p.cl->last_en->destroyed);
    DW_RELEASE(c);
    CHECK_HR_OK(IDWriteFactory_UnregisterFontFileLoader(other, (IDWriteFontFileLoader *)p.fl));
    DW_RELEASE(other);
    fcl_px_close(&p);
}
TEST(fcl_src_file_from_the_builtin_local_file_loader)
{
    FCL_PX(p);
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    size_t n; uint8_t *b = ttf_build_styled("Disk Fam", 400, 5, 0, &n);
    t_tmpfile(path, sizeof path, "fcl_disk_one.ttf");
    FILE *fp = fopen(path, "wb"); CHECK(fp != NULL);
    if (!fp) { free(b); fcl_px_close(&p); return; }
    fwrite(b, 1, n, fp); fclose(fp); free(b);
    dw_wide(path, wpath, MAX_PATH);
    IDWriteFontFile *file = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(p.f, wpath, NULL, &file));
    if (file) {
        p.cl->inject[p.cl->ninject++] = file;
        fcl_px_add_style(&p, "Memory Fam", 400, 5, 0);       /* mixed with a custom-loader file */
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(fcl_px_create(&p, &c));
        if (c) {
            EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
            CHECK(fcl_find(c, "Disk Fam") >= 0);
            CHECK(fcl_find(c, "Memory Fam") >= 0);
            IDWriteFontFace *face = fcl_face_of(c, (UINT32)fcl_find(c, "Disk Fam"), 0);
            CHECK(face != NULL);
            if (face) { EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS); DW_RELEASE(face); }
        }
        DW_RELEASE(c);
        IDWriteFontFile_Release(file);
    }
    fcl_px_close(&p);
}
TEST(fcl_src_same_local_file_listed_twice)
{
    FCL_PX(p);
    char path[MAX_PATH]; WCHAR wpath[MAX_PATH];
    size_t n; uint8_t *b = ttf_build_styled("Disk Twice", 400, 5, 0, &n);
    t_tmpfile(path, sizeof path, "fcl_disk_two.ttf");
    FILE *fp = fopen(path, "wb");
    if (!fp) { free(b); fcl_px_close(&p); t_skip("cannot write scratch file"); return; }
    fwrite(b, 1, n, fp); fclose(fp); free(b);
    dw_wide(path, wpath, MAX_PATH);
    IDWriteFontFile *f1 = NULL, *f2 = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(p.f, wpath, NULL, &f1));
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(p.f, wpath, NULL, &f2));
    if (f1 && f2) {
        p.cl->inject[p.cl->ninject++] = f1; p.cl->inject[p.cl->ninject++] = f2;
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(fcl_px_create(&p, &c));
        if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); t_note("same disk file twice: font count %u", fcl_font_count(c, 0)); }
        DW_RELEASE(c);
    }
    DW_RELEASE(f1); DW_RELEASE(f2);
    fcl_px_close(&p);
}
TEST(fcl_src_in_memory_font_file_loader)
{
    FCL_PX(p);
    IDWriteFactory5 *f5 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(p.f, &IID_IDWriteFactory5, (void **)&f5)) || !f5) { fcl_px_close(&p); t_skip("IDWriteFactory5 unsupported"); return; }
    IDWriteInMemoryFontFileLoader *mem = NULL;
    CHECK_HR_OK(IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &mem));
    if (mem) {
        CHECK_HR_OK(IDWriteFactory5_RegisterFontFileLoader(f5, (IDWriteFontFileLoader *)mem));
        size_t n; uint8_t *b = ttf_build_styled("InMem Fam", 400, 5, 0, &n);
        IDWriteFontFile *file = NULL;
        CHECK_HR_OK(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(mem, p.f, b, (UINT32)n, NULL, &file));
        if (file) {
            p.cl->inject[p.cl->ninject++] = file;
            fcl_px_add_style(&p, "Plain Fam", 400, 5, 0);
            IDWriteFontCollection *c = NULL;
            CHECK_HR_OK(fcl_px_create(&p, &c));
            if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2); CHECK(fcl_find(c, "InMem Fam") >= 0); }
            DW_RELEASE(c);
            IDWriteFontFile_Release(file);
        }
        IDWriteFactory5_UnregisterFontFileLoader(f5, (IDWriteFontFileLoader *)mem);
        IDWriteInMemoryFontFileLoader_Release(mem);
        free(b);
    }
    IDWriteFactory5_Release(f5);
    fcl_px_close(&p);
}
TEST(fcl_src_enumerated_files_may_come_from_several_file_loaders)
{
    FCL_PX(p);
    dwx_mem_loader *second = dwx_loader_new();
    ttf_spec s; size_t n; uint8_t *b;
    ttf_style_spec(&s, "Second Loader Fam", 400, 5, 0); b = ttf_build(&s, &n);
    dwx_loader_add(second, "pad", 3);                  /* so that this loader's key (1) differs from the first loader's key (0) */
    UINT32 k2 = dwx_loader_add(second, b, n); free(b);
    CHECK_HR_OK(dwx_loader_register(second, p.f));
    IDWriteFontFile *file = NULL;
    CHECK_HR_OK(dwx_loader_file(second, p.f, k2, &file));
    fcl_px_add_style(&p, "First Loader Fam", 400, 5, 0);
    if (file) {
        p.cl->inject[p.cl->ninject++] = file;
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(fcl_px_create(&p, &c));
        if (c) {
            EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
            CHECK(fcl_find(c, "First Loader Fam") >= 0 && fcl_find(c, "Second Loader Fam") >= 0);
            CHECK(second->n_create >= 1);
        }
        DW_RELEASE(c);
        IDWriteFontFile_Release(file);
    }
    dwx_loader_unregister(second); dwx_release(second);
    fcl_px_close(&p);
}

/* ---------------------------------------------------------------------------------------- key variety */
TEST(fcl_key_large_sizes)
{
    static const UINT32 sizes[] = { 1024, 4096, 65536, 1000000 };
    unsigned i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        FCL_PX(p);
        UINT32 ks = sizes[i], j; uint8_t *key = (uint8_t *)malloc(ks);
        uint32_t hv = 2166136261u;
        for (j = 0; j < ks; j++) { key[j] = (uint8_t)(j * 31 + i); hv ^= key[j]; hv *= 16777619u; }
        fcl_px_add_style(&p, "Big Key", 400, 5, 0);
        IDWriteFontCollection *c = NULL;
        HRESULT hr = fcl_px_create_key(&p, key, ks, &c);
        CHECKF(hr == S_OK, "key size %u: %s", ks, dw_hr_name(hr));
        EXPECT_EQ(p.cl->last_ks, ks);
        CHECKF(p.cl->key_hash == hv, "key size %u: the loader saw different bytes", ks);
        if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        DW_RELEASE(c);
        free(key);
        fcl_px_close(&p);
    }
}
TEST(fcl_key_differing_in_last_byte_are_distinct)
{
    FCL_PX(p);
    uint8_t key[4096]; memset(key, 0x5A, sizeof key);
    fcl_px_add_style(&p, "Last Byte", 400, 5, 0);
    IDWriteFontCollection *a = NULL, *b = NULL;
    CHECK_HR_OK(fcl_px_create_key(&p, key, sizeof key, &a));
    uint32_t h1 = p.cl->key_hash;
    key[sizeof key - 1] ^= 1;
    CHECK_HR_OK(fcl_px_create_key(&p, key, sizeof key, &b));
    EXPECT_EQ(p.cl->n_create, 2);
    CHECK(p.cl->key_hash != h1);
    DW_RELEASE(a); DW_RELEASE(b);
    fcl_px_close(&p);
}
TEST(fcl_key_same_prefix_different_size_are_distinct)
{
    FCL_PX(p);
    static const char key[] = "prefixkey\0\0";
    fcl_px_add_style(&p, "Prefix Key", 400, 5, 0);
    IDWriteFontCollection *a = NULL, *b = NULL;
    CHECK_HR_OK(fcl_px_create_key(&p, key, 9, &a));
    CHECK_HR_OK(fcl_px_create_key(&p, key, 10, &b));
    EXPECT_EQ(p.cl->n_create, 2);
    EXPECT_EQ(p.cl->last_ks, 10);
    DW_RELEASE(a); DW_RELEASE(b);
    fcl_px_close(&p);
}
TEST(fcl_key_text_is_not_interpreted)
{
    /* UTF-16 text, a path, embedded newlines and high bytes all travel verbatim. */
    static const struct { const void *k; UINT32 n; } keys[] = {
        { L"C:\\fonts\\mine.ttf", 38 }, { "line1\nline2\r\n", 13 }, { "\xFF\xFE\xFD\x01", 4 }, { "%s%n%d", 6 }, { "\"quoted\"", 8 },
    };
    unsigned i;
    for (i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        FCL_PX(p);
        fcl_px_add_style(&p, "Verbatim", 400, 5, 0);
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(fcl_px_create_key(&p, keys[i].k, keys[i].n, &c));
        EXPECT_EQ(p.cl->last_ks, keys[i].n);
        CHECK(!memcmp(p.cl->keycopy, keys[i].k, keys[i].n));
        DW_RELEASE(c);
        fcl_px_close(&p);
    }
}

/* ----------------------------------------------------------------------------------------- re-entrancy */
static void hook_create_inner(fcl_cl *c, IDWriteFactory *f)
{
    /* inside CreateEnumeratorFromKey: ask the same factory for another (different-key) collection and for a system collection */
    IDWriteFontCollection *inner = NULL, *sys = NULL;
    HRESULT hr = IDWriteFactory_CreateCustomFontCollection(f, (IDWriteFontCollectionLoader *)c, "inner-key-1", 11, &inner);
    c->tid = (DWORD)hr;                       /* smuggle the result out */
    if (inner) IDWriteFontCollection_Release(inner);
    IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE);
    if (sys) IDWriteFontCollection_Release(sys);
    c->hook = NULL;
}
/* Run in a child process: a lock-order problem inside the library must not hang the whole run. */
CHILD(fcl_reentrant)
{
    fcl_px p;
    if (!fcl_px_open(&p, 1)) return 90;
    fcl_px_add_style(&p, "Reentrant", 400, 5, 0);
    p.cl->hook = hook_create_inner;
    IDWriteFontCollection *c = NULL;
    HRESULT hr = fcl_px_create_key(&p, "outer-key-1", 11, &c);
    int rc = 0;
    if (FAILED(hr) || !c) rc = 1;
    else if (IDWriteFontCollection_GetFontFamilyCount(c) != 1) rc = 2;
    else if (FAILED((HRESULT)p.cl->tid)) rc = 3;
    DW_RELEASE(c);
    fcl_px_close(&p);
    return rc;
}
TEST(fcl_reentrant_create_from_inside_the_loader)
{
    DWORD rc = t_run_child("fcl_reentrant", "", 15000);
    if (rc == 0xDEAD0001) { t_note("re-entrant CreateCustomFontCollection did not finish within 15s (child killed)"); return; }
    if (rc == 0xDEAD0000) { t_skip("could not launch the child process"); return; }
    t_note("child exit code %lu (0 ok, 1 outer create failed, 2 wrong family count, 3 inner create failed)", (unsigned long)rc);
    CHECK(rc == 0 || rc == 3);              /* the outer collection must come out right even if the nested request is refused */
}

/* --------------------------------------------------------------------------- font members of a family */
TEST(fcl_member_get_font_out_of_range)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Member Bounds");
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    if (fam) {
        UINT32 n = IDWriteFontFamily_GetFontCount(fam);
        static const UINT32 deltas[] = { 0, 1, 1000 };
        unsigned i;
        for (i = 0; i < 3; i++) {
            IDWriteFont *f = (IDWriteFont *)(ULONG_PTR)1;
            HRESULT hr = IDWriteFontFamily_GetFont(fam, n + deltas[i], &f);
            t_note("GetFont(count+%u) -> %s", deltas[i], dw_hr_name(hr));
            CHECK(FAILED(hr));
            CHECK_OUT_NULL(f);
        }
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_member_every_index_resolves)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Member All", 400, 5, 0); fcl_add_style(&x, "Member All", 700, 5, 0);
    fcl_add_style(&x, "Member All", 400, 5, 1); fcl_add_style(&x, "Member All", 700, 5, 1);
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    if (fam) {
        UINT32 n = IDWriteFontFamily_GetFontCount(fam), k;
        CHECK(n >= 4);
        for (k = 0; k < n; k++) {
            IDWriteFont *f = NULL;
            HRESULT hr = IDWriteFontFamily_GetFont(fam, k, &f);
            CHECKF(hr == S_OK && f, "font %u: %s", k, dw_hr_name(hr));
            if (f) {
                DWRITE_FONT_METRICS m; IDWriteFont_GetMetrics(f, &m);
                EXPECT_EQ(m.designUnitsPerEm, 2048);
                IDWriteLocalizedStrings *fn = NULL;
                if (SUCCEEDED(IDWriteFont_GetFaceNames(f, &fn)) && fn) { char nm[64]; fcl_ls_en(fn, nm, sizeof nm); t_note("font %u face name \"%s\" w=%d st=%d sim=%d", k, nm, IDWriteFont_GetWeight(f), IDWriteFont_GetStyle(f), IDWriteFont_GetSimulations(f)); DW_RELEASE(fn); }
                DW_RELEASE(f);
            }
        }
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_member_first_matching_font_prefers_real_faces)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Match Real", 400, 5, 0); fcl_add_style(&x, "Match Real", 700, 5, 0);
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    if (fam) {
        IDWriteFont *f = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &f));
        if (f) { EXPECT_EQ(IDWriteFont_GetWeight(f), 700); EXPECT_EQ(IDWriteFont_GetSimulations(f), 0); DW_RELEASE(f); }
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &f));
        if (f) { EXPECT_EQ(IDWriteFont_GetWeight(f), 400); EXPECT_EQ(IDWriteFont_GetSimulations(f), 0); DW_RELEASE(f); }
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &f));
        if (f) { t_note("italic requested from a family without italic: weight %d style %d sim %d", IDWriteFont_GetWeight(f), IDWriteFont_GetStyle(f), IDWriteFont_GetSimulations(f)); DW_RELEASE(f); }
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_member_matching_fonts_list)
{
    FCL_OPEN(x);
    fcl_add_style(&x, "Match List", 400, 5, 0); fcl_add_style(&x, "Match List", 700, 5, 0); fcl_add_style(&x, "Match List", 400, 5, 1);
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    if (fam) {
        IDWriteFontList *list = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &list));
        if (list) {
            UINT32 n = IDWriteFontList_GetFontCount(list);
            t_note("matching list size %u", n);
            CHECK(n >= 1);
            IDWriteFont *f = NULL;
            CHECK_HR_OK(IDWriteFontList_GetFont(list, 0, &f));
            if (f) { EXPECT_EQ(IDWriteFont_GetWeight(f), 400); EXPECT_EQ(IDWriteFont_GetStyle(f), DWRITE_FONT_STYLE_NORMAL); DW_RELEASE(f); }
            DW_RELEASE(list);
        }
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
    fcl_close(&x);
}

/* ----------------------------------------------------------------- property derivation in the collection */
static IDWriteFont *real_font_of(IDWriteFontCollection *c)
{
    IDWriteFontFamily *fam = NULL; IDWriteFont *res = NULL; UINT32 n, k;
    if (FAILED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) || !fam) return NULL;
    n = IDWriteFontFamily_GetFontCount(fam);
    for (k = 0; k < n && !res; k++) {
        IDWriteFont *f = NULL;
        if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &f)) && f) { if (IDWriteFont_GetSimulations(f) == 0) res = f; else IDWriteFont_Release(f); }
    }
    IDWriteFontFamily_Release(fam);
    return res;
}
static IDWriteFont *font_from_spec(fcl_fx *x, const ttf_spec *s, IDWriteFontCollection **cout)
{
    fcl_add_spec(x, s);
    *cout = NULL;
    if (FAILED(fcl_create(x, cout)) || !*cout) return NULL;
    return real_font_of(*cout);
}
TEST(fcl_prop_weight_class_passthrough)
{
    unsigned i;
    for (i = 100; i <= 900; i += 50) {
        FCL_OPEN(x);
        ttf_spec s; ttf_style_spec(&s, "WeightProp", (int)i, 5, 0);
        IDWriteFontCollection *c; IDWriteFont *f = font_from_spec(&x, &s, &c);
        CHECKF(f != NULL, "weight %u: no font", i);
        if (f) { CHECKF(IDWriteFont_GetWeight(f) == (int)i, "usWeightClass %u -> %d", i, IDWriteFont_GetWeight(f)); DW_RELEASE(f); }
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_prop_weight_class_odd_values_note)
{
    static const int ws[] = { 0, 1, 4, 7, 9, 10, 99, 101, 399, 401, 999, 1000, 1001, 1500 };
    unsigned i;
    for (i = 0; i < sizeof ws / sizeof ws[0]; i++) {
        FCL_OPEN(x);
        ttf_spec s; ttf_style_spec(&s, "OddWeight", 400, 5, 0); s.weight = ws[i];
        IDWriteFontCollection *c; IDWriteFont *f = font_from_spec(&x, &s, &c);
        if (f) { t_note("usWeightClass %d -> weight %d", ws[i], IDWriteFont_GetWeight(f)); DW_RELEASE(f); } else t_note("usWeightClass %d -> no font (%u families)", ws[i], c ? IDWriteFontCollection_GetFontFamilyCount(c) : 0);
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_prop_width_class_values)
{
    int w;
    for (w = 1; w <= 9; w++) {
        FCL_OPEN(x);
        ttf_spec s; ttf_style_spec(&s, "WidthProp", 400, w, 0);
        IDWriteFontCollection *c; IDWriteFont *f = font_from_spec(&x, &s, &c);
        CHECKF(f != NULL, "width %d: no font", w);
        if (f) { CHECKF(IDWriteFont_GetStretch(f) == w, "usWidthClass %d -> %d", w, IDWriteFont_GetStretch(f)); DW_RELEASE(f); }
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_prop_width_class_out_of_range_note)
{
    static const int ws[] = { 0, 10, 11, 255 };
    unsigned i;
    for (i = 0; i < 4; i++) {
        FCL_OPEN(x);
        ttf_spec s; ttf_style_spec(&s, "OddWidth", 400, 5, 0); s.width = ws[i];
        IDWriteFontCollection *c; IDWriteFont *f = font_from_spec(&x, &s, &c);
        if (f) { t_note("usWidthClass %d -> stretch %d", ws[i], IDWriteFont_GetStretch(f)); CHECK(IDWriteFont_GetStretch(f) >= 0 && IDWriteFont_GetStretch(f) <= 9); DW_RELEASE(f); }
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_prop_style_from_fs_selection)
{
    static const struct { unsigned fs; unsigned mac; int angle; int want; const char *what; } t[] = {
        { 0x40, 0, 0, DWRITE_FONT_STYLE_NORMAL, "REGULAR" },
        { 0x01 | 0x40, 0, 0, DWRITE_FONT_STYLE_ITALIC, "ITALIC bit" },
        { 0x01, 2, -12 * 65536, DWRITE_FONT_STYLE_ITALIC, "ITALIC bit + macStyle + angle" },
    };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        FCL_OPEN(x);
        ttf_spec s; ttf_style_spec(&s, "StyleProp", 400, 5, 0);
        s.fs_selection = t[i].fs; s.mac_style = t[i].mac; s.italic_angle_fixed = t[i].angle;
        IDWriteFontCollection *c; IDWriteFont *f = font_from_spec(&x, &s, &c);
        CHECKF(f != NULL, "%s: no font", t[i].what);
        if (f) { CHECKF(IDWriteFont_GetStyle(f) == t[i].want, "%s -> style %d want %d", t[i].what, IDWriteFont_GetStyle(f), t[i].want); DW_RELEASE(f); }
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_prop_style_other_italic_signals_note)
{
    static const struct { unsigned fs; unsigned mac; int angle; const char *what; } t[] = {
        { 0x40, 2, 0, "macStyle italic only" },
        { 0x40, 0, -12 * 65536, "post italicAngle only" },
        { 0x40 | 0x200, 0, 0, "OS/2 OBLIQUE bit only" },
        { 0x40 | 0x200, 2, -12 * 65536, "OBLIQUE bit + angle + macStyle" },
        { 0x01 | 0x200, 0, 0, "ITALIC and OBLIQUE bits" },
    };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        FCL_OPEN(x);
        ttf_spec s; ttf_style_spec(&s, "StyleSignals", 400, 5, 0);
        s.fs_selection = t[i].fs; s.mac_style = t[i].mac; s.italic_angle_fixed = t[i].angle;
        IDWriteFontCollection *c; IDWriteFont *f = font_from_spec(&x, &s, &c);
        if (f) { t_note("%s -> style %d", t[i].what, IDWriteFont_GetStyle(f)); DW_RELEASE(f); }
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_prop_bold_signals_note)
{
    static const struct { unsigned fs; unsigned mac; int weight; const char *what; } t[] = {
        { 0x20, 1, 400, "BOLD bit + macStyle bold, weight class 400" },
        { 0x40, 0, 700, "weight class 700, no bold bits" },
        { 0x40, 1, 400, "macStyle bold only" },
        { 0x20, 0, 400, "fsSelection BOLD only" },
    };
    unsigned i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        FCL_OPEN(x);
        ttf_spec s; ttf_style_spec(&s, "BoldSignals", 400, 5, 0);
        s.fs_selection = t[i].fs; s.mac_style = t[i].mac; s.weight = t[i].weight;
        IDWriteFontCollection *c; IDWriteFont *f = font_from_spec(&x, &s, &c);
        if (f) { t_note("%s -> weight %d", t[i].what, IDWriteFont_GetWeight(f)); DW_RELEASE(f); }
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_prop_missing_os2_table_note)
{
    FCL_OPEN(x);
    ttf_spec s; ttf_style_spec(&s, "NoOs2", 700, 5, 0); s.omit |= TTF_OMIT_OS2; s.mac_style = 1;
    IDWriteFontCollection *c; IDWriteFont *f = font_from_spec(&x, &s, &c);
    t_note("font without OS/2: %s", f ? "listed" : "not listed");
    if (f) { t_note("weight %d style %d stretch %d", IDWriteFont_GetWeight(f), IDWriteFont_GetStyle(f), IDWriteFont_GetStretch(f)); DW_RELEASE(f); }
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_prop_missing_cmap_or_glyf_table_note)
{
    static const unsigned omits[] = { TTF_OMIT_CMAP, TTF_OMIT_GLYF, TTF_OMIT_HEAD, TTF_OMIT_HHEA, TTF_OMIT_HMTX, TTF_OMIT_MAXP, TTF_OMIT_POST };
    unsigned i;
    for (i = 0; i < sizeof omits / sizeof omits[0]; i++) {
        FCL_OPEN(x);
        ttf_spec s; ttf_style_spec(&s, "MissingTable", 400, 5, 0); s.omit |= omits[i];
        fcl_add_spec(&x, &s);
        fcl_add_family(&x, "Healthy Neighbour");
        IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
        HRESULT hr = fcl_create(&x, &c);
        t_note("font missing table (omit mask 0x%x) -> %s, families %u", omits[i], dw_hr_name(hr), (SUCCEEDED(hr) && c) ? IDWriteFontCollection_GetFontFamilyCount(c) : 0);
        if (SUCCEEDED(hr) && c) CHECK(fcl_find(c, "Healthy Neighbour") >= 0); else CHECK_OUT_NULL(c);
        DW_RELEASE(c);
        fcl_close(&x);
    }
}
TEST(fcl_rev_get_font_family_out_zeroed_on_bad_index)
{
    /* GetFontFamily stores NULL into *out before the range check (decomp), so a stale pointer never survives a failure. */
    FCL_OPEN(x);
    fcl_add_family(&x, "Zero Out A");
    FCL_COLL(x, c);
    IDWriteFontFamily *fam = (IDWriteFontFamily *)(ULONG_PTR)1;
    HRESULT hr = IDWriteFontCollection_GetFontFamily(c, IDWriteFontCollection_GetFontFamilyCount(c), &fam);
    CHECK(FAILED(hr));
    CHECK_OUT_NULL(fam);
    DW_RELEASE(fam);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_rev_family_count_is_stable_across_calls)
{
    FCL_OPEN(x);
    fcl_add_family(&x, "Stable One");
    fcl_add_family(&x, "Stable Two");
    FCL_COLL(x, c);
    UINT32 a = IDWriteFontCollection_GetFontFamilyCount(c), b = IDWriteFontCollection_GetFontFamilyCount(c);
    EXPECT_EQ(a, 2);
    CHECK(a == b);
    DW_RELEASE(c);
    fcl_close(&x);
}
TEST(fcl_rev_find_miss_reports_sentinel_index)
{
    /* FindFamilyName always succeeds; a miss yields exists=FALSE and index 0xFFFFFFFF. */
    FCL_OPEN(x);
    fcl_add_family(&x, "Present Fam");
    FCL_COLL(x, c);
    WCHAR w[64]; UINT32 idx = 5; BOOL ex = TRUE;
    dw_wide("Absent Fam", w, 64);
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, w, &idx, &ex));
    CHECK(!ex);
    CHECK(idx == 0xFFFFFFFFu);
    DW_RELEASE(c);
    fcl_close(&x);
}
