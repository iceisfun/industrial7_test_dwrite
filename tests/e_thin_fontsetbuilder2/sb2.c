/* IDWriteFontSetBuilder2::AddFont / AddFontFile on static synthetic fonts: caller-supplied properties, simulations, duplicates,
 * and what the produced IDWriteFontSet reports. */
#include "../font_set/fs.h"

typedef struct {
    fs_fx x; IDWriteFactory *f; IDWriteFactory6 *f6; IDWriteFontFile *file, *file2; IDWriteFontSetBuilder2 *b; int ok;
} sx;
static int sx_open(sx *x)
{
    memset(x, 0, sizeof *x);
    if (!fs_open(&x->x)) return 0;
    x->f = x->x.f;
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory6, (void **)&x->f6))) { fs_close(&x->x); t_skip("IDWriteFactory6 unavailable"); return 0; }
    x->file = fs_file(&x->x, fs_add(&x->x, "EnrichA", 400, 5, 0));
    x->file2 = fs_file(&x->x, fs_add(&x->x, "EnrichB", 700, 5, 0));
    if (!x->file || !x->file2 || FAILED(IDWriteFactory6_CreateFontSetBuilder(x->f6, &x->b))) { t_skip("fixture failed"); return 1; }
    x->ok = 1; return 1;
}
static void sx_close(sx *x)
{
    DW_RELEASE(x->b); DW_RELEASE(x->file); DW_RELEASE(x->file2); DW_RELEASE(x->f6);
    fs_close(&x->x);
}
#define SX() sx X; if (!sx_open(&X)) return; if (!X.ok) { sx_close(&X); return; }

static IDWriteFontSet *mk(sx *x) { IDWriteFontSet *s = NULL; CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(x->b, &s)); return s; }
static void prop_values(IDWriteFontSet *s, UINT32 idx, DWRITE_FONT_PROPERTY_ID id, char *out, int cap)
{
    BOOL exists = 0; IDWriteLocalizedStrings *ls = NULL; out[0] = 0;
    if (SUCCEEDED(IDWriteFontSet_GetPropertyValues(s, idx, id, &exists, &ls)) && exists && ls) dw_ls_en(ls, out, cap);
    DW_RELEASE(ls);
}

/* Property ids 1..14 as one comma-joined snapshot of the English values of entry 0 ("" for absent). */
static void snap(IDWriteFontSet *s, char out[14][64])
{
    UINT32 id;
    for (id = 1; id <= 14; id++) prop_values(s, 0, (DWRITE_FONT_PROPERTY_ID)id, out[id - 1], 64);
}
static int all_empty(char v[14][64], int skip_id)
{
    int i;
    for (i = 0; i < 14; i++) if (i + 1 != skip_id && v[i][0]) return 0;
    return 1;
}

TEST(sb2_add_font_without_properties_reads_nothing_from_file)
{
    SX(); IDWriteFontSet *s; char v[14][64];
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 0));
    s = mk(&X);
    if (s) { EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 1); snap(s, v); CHECK(all_empty(v, 0)); DW_RELEASE(s); }
    /* negative control: the file-reading overload on the same file fills the properties in */
    {   IDWriteFontSetBuilder1 *b1 = NULL; IDWriteFontSetBuilder2 *c = NULL;
        CHECK_HR_OK(IDWriteFactory6_CreateFontSetBuilder(X.f6, &c));
        if (c && SUCCEEDED(IDWriteFontSetBuilder2_QueryInterface(c, &IID_IDWriteFontSetBuilder1, (void **)&b1))) {
            CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(b1, X.file));
            s = NULL; CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(c, &s));
            if (s) { snap(s, v); EXPECT_STR(v[DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME - 1], "EnrichA"); EXPECT_STR(v[DWRITE_FONT_PROPERTY_ID_WEIGHT - 1], "400"); DW_RELEASE(s); }
        }
        DW_RELEASE(b1); DW_RELEASE(c); }
    sx_close(&X);
}

TEST(sb2_add_font_caller_properties_are_the_only_properties)
{
    SX(); IDWriteFontSet *s; char v[14][64];
    DWRITE_FONT_PROPERTY p = { DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, L"Overridden", L"en-us" };
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, &p, 1));
    s = mk(&X);
    if (s) {
        EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 1); snap(s, v);
        EXPECT_STR(v[DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME - 1], "Overridden");
        CHECK(all_empty(v, DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME));       /* nothing taken from the file's own name table */
        DW_RELEASE(s);
    }
    sx_close(&X);
}

TEST(sb2_counts_and_duplicates)
{
    SX(); IDWriteFontSet *s;
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 0));
    s = mk(&X); if (s) { EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 1); DW_RELEASE(s); }
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(X.b, X.file2, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 0));
    s = mk(&X); if (s) { EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 2); DW_RELEASE(s); }
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 0));
    s = mk(&X); if (s) { EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 3); DW_RELEASE(s); }     /* no de-duplication */
    sx_close(&X);
}

TEST(sb2_simulations_accepted_but_reference_reports_none)
{
    SX(); IDWriteFontSet *s; int k;
    static const DWRITE_FONT_SIMULATIONS sims[] = { DWRITE_FONT_SIMULATIONS_NONE, DWRITE_FONT_SIMULATIONS_BOLD, DWRITE_FONT_SIMULATIONS_OBLIQUE,
                                                    (DWRITE_FONT_SIMULATIONS)3 };
    for (k = 0; k < 4; k++) {
        IDWriteFontSetBuilder2 *c = NULL; IDWriteFontFaceReference *r = NULL;
        CHECK_HR_OK(IDWriteFactory6_CreateFontSetBuilder(X.f6, &c));
        CHECKF(c != NULL, "builder");
        if (!c) break;
        CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(c, X.file, 0, sims[k], NULL, 0, NULL, 0, NULL, 0));
        s = NULL; CHECK_HR_OK(IDWriteFontSetBuilder2_CreateFontSet(c, &s));
        if (s) {
            EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 1);
            CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(s, 0, &r));
            if (r) { EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), 0); EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 0); DW_RELEASE(r); }
            DW_RELEASE(s);
        }
        DW_RELEASE(c);
    }
    /* negative control: bits outside the two simulation flags are rejected */
    CHECK_HR(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, (DWRITE_FONT_SIMULATIONS)4, NULL, 0, NULL, 0, NULL, 0), E_INVALIDARG);
    sx_close(&X);
}

TEST(sb2_add_font_file_by_path)
{
    SX(); char path[MAX_PATH]; WCHAR w[MAX_PATH]; size_t n; uint8_t *b = ttf_build_styled("EnrichP", 700, 5, 0, &n); FILE *fp; IDWriteFontSet *s; char v[14][64];
    t_tmpfile(path, sizeof path, "enrich_p.ttf");
    fp = fopen(path, "wb"); CHECK(fp != NULL);
    if (fp) { fwrite(b, 1, n, fp); fclose(fp); }
    dw_wide(path, w, MAX_PATH);
    CHECK_HR(IDWriteFontSetBuilder2_AddFontFile(X.b, L"Z:\\no\\such\\dir\\file.ttf"), DWRITE_E_FILENOTFOUND);
    CHECK_HR(IDWriteFontSetBuilder2_AddFontFile(X.b, L""), DWRITE_E_FILENOTFOUND);
    CHECK_HR(IDWriteFontSetBuilder2_AddFontFile(X.b, NULL), E_INVALIDARG);
    s = mk(&X); if (s) { EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 0); DW_RELEASE(s); }          /* failures add nothing */
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFontFile(X.b, w));
    s = mk(&X);
    if (s) {
        EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 1); snap(s, v);
        EXPECT_STR(v[DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME - 1], "EnrichP");      /* the path overload reads the file, unlike AddFont */
        EXPECT_STR(v[DWRITE_FONT_PROPERTY_ID_WEIGHT - 1], "700");
        EXPECT_STR(v[DWRITE_FONT_PROPERTY_ID_STRETCH - 1], "5");
        DW_RELEASE(s);
    }
    free(b);
    sx_close(&X);
}

TEST(sb2_invalid_arguments_leave_builder_unchanged)
{
    SX(); IDWriteFontSet *s;
    CHECK_HR(IDWriteFontSetBuilder2_AddFont(X.b, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 0), E_INVALIDARG);
    CHECK_HR(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, (DWRITE_FONT_SIMULATIONS)0x10, NULL, 0, NULL, 0, NULL, 0), E_INVALIDARG);
    CHECK_HR(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 1, NULL, 0, NULL, 0), E_INVALIDARG);
    CHECK_HR(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 1, NULL, 0), E_INVALIDARG);
    CHECK_HR(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 1), E_INVALIDARG);
    s = mk(&X); if (s) { EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 0); DW_RELEASE(s); }
    /* negative control: the same call with valid arguments adds one entry */
    CHECK_HR_OK(IDWriteFontSetBuilder2_AddFont(X.b, X.file, 0, DWRITE_FONT_SIMULATIONS_NONE, NULL, 0, NULL, 0, NULL, 0));
    s = mk(&X); if (s) { EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 1); DW_RELEASE(s); }
    sx_close(&X);
}
