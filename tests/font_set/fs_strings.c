/* IDWriteFontSet::GetPropertyValues in all three flavours and the IDWriteStringList / IDWriteLocalizedStrings objects it returns.
 *
 * Fixture (fs_std): SynthA {Regular, Bold, Italic, Bold Italic} and SynthB Regular, all with Windows (3,1,0x409) names only.
 *
 * Win10 behaviour:
 *  - GetPropertyValues gives E_INVALIDARG for ids > 14; ids 0..14 are valid.
 *  - GetPropertyValues(id, NULL locales, ..) is E_INVALIDARG; the list out parameter is zeroed first.
 *  - GetPropertyValues(index, id, &exists, &strings): both outputs zeroed first, exists only when the font has a value.
 *  - String list methods: size 0 -> HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) (checked before the index), StringCchCopy
 *    truncation -> same code with a terminated truncated string; length queries store 0xFFFFFFFF before an out-of-range
 *    index fails.
 */
#include "fs.h"

/* Expected value sets for the fixture. */
static const char *const v_family[]  = { "SynthA", "SynthB" };
static const char *const v_full[]    = { "SynthA", "SynthA Bold", "SynthA Italic", "SynthA Bold Italic", "SynthB" };
static const char *const v_ps[]      = { "SynthA-Regular", "SynthA-Bold", "SynthA-Italic", "SynthA-BoldItalic", "SynthB-Regular" };
static const char *const v_face[]    = { "Regular", "Bold", "Italic", "Bold Italic" };
static const char *const v_weight[]  = { "400", "700" };
static const char *const v_stretch[] = { "5" };
static const char *const v_style[]   = { "0", "2" };

static void check_values(DWRITE_FONT_PROPERTY_ID id, const char *const *want, int n, int exact_count)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, id);
    CHECK(l != NULL);
    if (l) {
        int i; UINT32 cnt = IDWriteStringList_GetCount(l);
        for (i = 0; i < n; i++) CHECKF(fs_sl_has(l, want[i]), "id %d lacks \"%s\"", (int)id, want[i]);
        if (exact_count) CHECKF(cnt == (UINT32)n, "id %d count %u want %d", (int)id, cnt, n);
        else FS_SOFT(cnt == (UINT32)n);
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_wws_family_values)      { check_values(DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, v_family, 2, 1); }
TEST(fset_str_typographic_family)     { check_values(DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FAMILY_NAME, v_family, 2, 1); }
TEST(fset_str_win32_family)           { check_values(DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, v_family, 2, 1); }
TEST(fset_str_full_names)             { check_values(DWRITE_FONT_PROPERTY_ID_FULL_NAME, v_full, 5, 1); }
TEST(fset_str_postscript_names)       { check_values(DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME, v_ps, 5, 1); }
TEST(fset_str_wws_face_names)         { check_values(DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FACE_NAME, v_face, 4, 0); }
TEST(fset_str_typographic_face_names) { check_values(DWRITE_FONT_PROPERTY_ID_TYPOGRAPHIC_FACE_NAME, v_face, 4, 0); }
TEST(fset_str_weight_values)          { check_values(DWRITE_FONT_PROPERTY_ID_WEIGHT, v_weight, 2, 1); }
TEST(fset_str_stretch_values)         { check_values(DWRITE_FONT_PROPERTY_ID_STRETCH, v_stretch, 1, 1); }
TEST(fset_str_style_values)           { check_values(DWRITE_FONT_PROPERTY_ID_STYLE, v_style, 2, 1); }

TEST(fset_str_script_tags_note)
{
    FS_STD(s);
    DWRITE_FONT_PROPERTY_ID ids[] = { DWRITE_FONT_PROPERTY_ID_DESIGN_SCRIPT_LANGUAGE_TAG, DWRITE_FONT_PROPERTY_ID_SUPPORTED_SCRIPT_LANGUAGE_TAG,
                                      DWRITE_FONT_PROPERTY_ID_SEMANTIC_TAG };
    int i;
    for (i = 0; i < 3; i++) {
        IDWriteStringList *l = NULL;
        HRESULT hr = IDWriteFontSet_GetPropertyValues__(s.set, ids[i], &l);
        CHECK_HR_OK(hr);
        CHECK(l != NULL);
        if (l) {
            UINT32 n = IDWriteStringList_GetCount(l), k; char b[128];
            t_note("property id %d: %u values", (int)ids[i], n);
            for (k = 0; k < n && k < 6; k++) { fs_sl_get(l, k, b, sizeof b); t_note("  [%u] %s", k, b); }
            DW_RELEASE(l);
        }
    }
    fs_std_close(&s);
}

TEST(fset_str_semantic_tag_empty)
{
    FS_STD(s);
    /* the synthetic fonts carry no semantic tags (no 'meta' table) */
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_SEMANTIC_TAG);
    CHECK(l != NULL);
    if (l) { FS_SOFT(IDWriteStringList_GetCount(l) == 0); DW_RELEASE(l); }
    fs_std_close(&s);
}

TEST(fset_str_id_none_and_14)
{
    FS_STD(s);
    int ids[] = { 0, 14 };
    int i;
    for (i = 0; i < 2; i++) {
        IDWriteStringList *l = (IDWriteStringList *)FS_SENT;
        HRESULT hr = IDWriteFontSet_GetPropertyValues__(s.set, (DWRITE_FONT_PROPERTY_ID)ids[i], &l);
        /* ids 0..14 are accepted by FontSet::GetPropertyValues */
        CHECKF(hr == S_OK, "id %d -> %s", ids[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && l) t_note("id %d: %u values", ids[i], IDWriteStringList_GetCount(l));
        FS_DROP(l, hr);
    }
    fs_std_close(&s);
}

TEST(fset_str_invalid_ids_all_forms)
{
    FS_STD(s);
    int bad[] = { 15, 16, 31, 100, 0x7FFFFFFF, -1, (int)0x80000000 };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteStringList *l = (IDWriteStringList *)FS_SENT; HRESULT hr;
        hr = IDWriteFontSet_GetPropertyValues__(s.set, (DWRITE_FONT_PROPERTY_ID)bad[i], &l);
        CHECKF(hr == E_INVALIDARG, "id %d -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(l);
        FS_DROP(l, hr);
        l = (IDWriteStringList *)FS_SENT;
        hr = IDWriteFontSet_GetPropertyValues_(s.set, (DWRITE_FONT_PROPERTY_ID)bad[i], L"en-us", &l);
        CHECKF(hr == E_INVALIDARG, "id %d (locale form) -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(l);
        FS_DROP(l, hr);
        IDWriteLocalizedStrings *ls = (IDWriteLocalizedStrings *)FS_SENT; BOOL ex = 77;
        hr = IDWriteFontSet_GetPropertyValues(s.set, 0, (DWRITE_FONT_PROPERTY_ID)bad[i], &ex, &ls);
        CHECKF(FAILED(hr), "id %d (index form) -> %s", bad[i], dw_hr_name(hr));
        CHECKF(ex == FALSE, "exists %d", ex);
        CHECK_OUT_NULL(ls);
        FS_DROP(ls, hr);
    }
    fs_std_close(&s);
}

TEST(fset_str_locale_null)
{
    FS_STD(s);
    IDWriteStringList *l = (IDWriteStringList *)FS_SENT;
    HRESULT hr = IDWriteFontSet_GetPropertyValues_(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, NULL, &l);
    CHECK_HR(hr, E_INVALIDARG);
    CHECK_OUT_NULL(l);
    FS_DROP(l, hr);
    fs_std_close(&s);
}

TEST(fset_str_locale_variants)
{
    FS_STD(s);
    const char *loc[] = { "en-us", "en-US", "EN-US", "en", "fr-fr", "de-DE", "ja-JP", "", "zz-ZZ", "fr-fr;en-us", "xx;yy;en-us" };
    unsigned i;
    for (i = 0; i < sizeof loc / sizeof loc[0]; i++) {
        WCHAR w[64]; IDWriteStringList *l = NULL; HRESULT hr;
        dw_wide(loc[i], w, 64);
        hr = IDWriteFontSet_GetPropertyValues_(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, w, &l);
        CHECKF(hr == S_OK, "locale \"%s\" -> %s", loc[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && l) {
            /* one nearest-language string per distinct value: SynthA and SynthB, whatever the requested locale */
            UINT32 n = IDWriteStringList_GetCount(l);
            if (i < 7) {
                CHECKF(n == 2, "locale \"%s\" count %u", loc[i], n);
                CHECKF(fs_sl_has(l, "SynthA") && fs_sl_has(l, "SynthB"), "locale \"%s\" values", loc[i]);
            } else FS_SOFT(n == 2);
        }
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_locale_form_all_ids)
{
    FS_STD(s);
    int id;
    for (id = 0; id <= 13; id++) {
        IDWriteStringList *a = NULL, *b = NULL;
        HRESULT ha = IDWriteFontSet_GetPropertyValues__(s.set, (DWRITE_FONT_PROPERTY_ID)id, &a);
        HRESULT hb = IDWriteFontSet_GetPropertyValues_(s.set, (DWRITE_FONT_PROPERTY_ID)id, L"en-us", &b);
        CHECKF(ha == S_OK && hb == S_OK, "id %d -> %s / %s", id, dw_hr_name(ha), dw_hr_name(hb));
        if (a && b) {
            /* all fixture names are en-us, so the plain and the en-us flavours agree */
            UINT32 na = IDWriteStringList_GetCount(a), nb = IDWriteStringList_GetCount(b), k;
            CHECKF(na == nb, "id %d counts %u vs %u", id, na, nb);
            for (k = 0; k < na; k++) { char sb[256]; if (SUCCEEDED(fs_sl_get(a, k, sb, sizeof sb))) CHECKF(fs_sl_has(b, sb), "id %d \"%s\"", id, sb); }
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_locale_names)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
    CHECK(l != NULL);
    if (l) {
        UINT32 i, n = IDWriteStringList_GetCount(l);
        for (i = 0; i < n; i++) {
            char loc[64]; UINT32 len = 99;
            CHECK_HR_OK(fs_sl_locale(l, i, loc, sizeof loc));
            CHECK_HR_OK(IDWriteStringList_GetLocaleNameLength(l, i, &len));
            CHECKF(len == strlen(loc), "locale length %u vs \"%s\"", len, loc);
            CHECKF(!_stricmp(loc, "en-us"), "locale \"%s\"", loc);
        }
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_string_lengths)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
    CHECK(l != NULL);
    if (l) {
        UINT32 i, n = IDWriteStringList_GetCount(l);
        for (i = 0; i < n; i++) {
            UINT32 len = 0xCCCC; WCHAR w[256];
            CHECK_HR_OK(IDWriteStringList_GetStringLength(l, i, &len));
            CHECK_HR_OK(IDWriteStringList_GetString(l, i, w, 256));
            CHECKF(len == wcslen(w), "length %u vs %u", len, (unsigned)wcslen(w));
        }
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_getstring_buffers)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_POSTSCRIPT_NAME);
    CHECK(l != NULL);
    if (l) {
        UINT32 i, n = IDWriteStringList_GetCount(l);
        for (i = 0; i < n; i++) {
            UINT32 len = 0; WCHAR full[256], buf[256]; HRESULT hr;
            IDWriteStringList_GetStringLength(l, i, &len);
            IDWriteStringList_GetString(l, i, full, 256);
            /* exact fit: length + 1 */
            wmemset(buf, 0x2222, 256);
            hr = IDWriteStringList_GetString(l, i, buf, len + 1);
            CHECK_HR_OK(hr);
            CHECK(!wcscmp(buf, full));
            /* one too short: truncated and terminated */
            wmemset(buf, 0x2222, 256);
            hr = IDWriteStringList_GetString(l, i, buf, len);
            CHECK_HR(hr, FS_E_NOTSUFF);
            CHECK(buf[len - 1] == 0);
            CHECK(!wcsncmp(buf, full, len - 1));
            /* size 1: only the terminator */
            wmemset(buf, 0x2222, 256);
            hr = IDWriteStringList_GetString(l, i, buf, 1);
            CHECK_HR(hr, FS_E_NOTSUFF);
            CHECK(buf[0] == 0);
            /* size 0: rejected before the buffer is touched */
            wmemset(buf, 0x2222, 256);
            hr = IDWriteStringList_GetString(l, i, buf, 0);
            CHECK_HR(hr, FS_E_NOTSUFF);
            CHECK(buf[0] == 0x2222);
            /* larger than needed */
            hr = IDWriteStringList_GetString(l, i, buf, 256);
            CHECK_HR_OK(hr);
            CHECK(!wcscmp(buf, full));
        }
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_getlocalename_buffers)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
    CHECK(l != NULL);
    if (l && IDWriteStringList_GetCount(l) > 0) {
        UINT32 len = 0; WCHAR buf[64], full[64]; HRESULT hr;
        IDWriteStringList_GetLocaleNameLength(l, 0, &len);
        IDWriteStringList_GetLocaleName(l, 0, full, 64);
        wmemset(buf, 0x2222, 64);
        CHECK_HR_OK(IDWriteStringList_GetLocaleName(l, 0, buf, len + 1));
        CHECK(!wcscmp(buf, full));
        wmemset(buf, 0x2222, 64);
        hr = IDWriteStringList_GetLocaleName(l, 0, buf, len);
        CHECK_HR(hr, FS_E_NOTSUFF);
        CHECK(buf[len - 1] == 0);
        wmemset(buf, 0x2222, 64);
        hr = IDWriteStringList_GetLocaleName(l, 0, buf, 0);
        CHECK_HR(hr, FS_E_NOTSUFF);
        CHECK(buf[0] == 0x2222);
    }
    DW_RELEASE(l);
    fs_std_close(&s);
}

TEST(fset_str_list_bad_index_lengths)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT);
    CHECK(l != NULL);
    if (l) {
        UINT32 n = IDWriteStringList_GetCount(l), len; HRESULT hr;
        UINT32 bad[] = { n, n + 1, 0x7FFFFFFFu, 0xFFFFFFFFu };
        unsigned i;
        for (i = 0; i < 4; i++) {
            len = 0x55; hr = IDWriteStringList_GetStringLength(l, bad[i], &len);
            CHECKF(hr == E_FAIL, "GetStringLength(%u) -> %s", bad[i], dw_hr_name(hr));   /* an out-of-range index gives E_FAIL */
            CHECKF(len == 0xFFFFFFFFu, "length %u", len);
            len = 0x55; hr = IDWriteStringList_GetLocaleNameLength(l, bad[i], &len);
            CHECKF(hr == E_FAIL, "GetLocaleNameLength(%u) -> %s", bad[i], dw_hr_name(hr));   /* an out-of-range index gives E_FAIL */
            CHECKF(len == 0xFFFFFFFFu, "length %u", len);
        }
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_bad_index_strings)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT);
    CHECK(l != NULL);
    if (l) {
        UINT32 n = IDWriteStringList_GetCount(l); WCHAR buf[16]; HRESULT hr;
        UINT32 bad[] = { n, n + 5, 0xFFFFFFFFu };
        unsigned i;
        for (i = 0; i < 3; i++) {
            wmemset(buf, 0x2222, 16);
            hr = IDWriteStringList_GetString(l, bad[i], buf, 16);
            CHECKF(hr == E_FAIL, "GetString(%u) -> %s", bad[i], dw_hr_name(hr));   /* an out-of-range index gives E_FAIL */
            CHECKF(buf[0] == 0, "buffer cleared before the throw: 0x%x", buf[0]);
            wmemset(buf, 0x2222, 16);
            hr = IDWriteStringList_GetLocaleName(l, bad[i], buf, 16);
            CHECKF(hr == E_FAIL, "GetLocaleName(%u) -> %s", bad[i], dw_hr_name(hr));   /* an out-of-range index gives E_FAIL */
            CHECKF(buf[0] == 0, "buffer cleared before the throw: 0x%x", buf[0]);
            /* size 0 wins over the index check */
            wmemset(buf, 0x2222, 16);
            hr = IDWriteStringList_GetString(l, bad[i], buf, 0);
            CHECK_HR(hr, FS_E_NOTSUFF);
            CHECK(buf[0] == 0x2222);
            hr = IDWriteStringList_GetLocaleName(l, bad[i], buf, 0);
            CHECK_HR(hr, FS_E_NOTSUFF);
        }
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_qi_matrix)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
    CHECK(l != NULL);
    if (l) {
        dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteStringList, 1), DW_QI_ROW(IDWriteLocalizedStrings, 1),
                             DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFactory, 0) };
        DW_CHECK_QI(l, rows);
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_localized_view)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
    CHECK(l != NULL);
    if (l) {
        IDWriteLocalizedStrings *ls = NULL;
        if (SUCCEEDED(IDWriteStringList_QueryInterface(l, &IID_IDWriteLocalizedStrings, (void **)&ls)) && ls) {
            UINT32 i, n = IDWriteStringList_GetCount(l);
            CHECK(IDWriteLocalizedStrings_GetCount(ls) == n);
            for (i = 0; i < n; i++) {
                WCHAR a[64], b[64];
                IDWriteStringList_GetString(l, i, a, 64);
                IDWriteLocalizedStrings_GetString(ls, i, b, 64);
                CHECK(!wcscmp(a, b));
            }
            UINT32 idx = 99; BOOL ex = FALSE;
            CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(ls, L"en-us", &idx, &ex));
            CHECK(ex == TRUE);
            DW_RELEASE(ls);
        } else t_note("string list does not expose IDWriteLocalizedStrings");
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_refcounts)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT);
    CHECK(l != NULL);
    if (l) {
        EXPECT_REFS(l, 1);
        IDWriteStringList_AddRef(l);
        EXPECT_REFS(l, 2);
        IDWriteStringList_Release(l);
        EXPECT_REFS(l, 1);
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_distinct_per_call)
{
    FS_STD(s);
    IDWriteStringList *a = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT), *b = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_WEIGHT);
    CHECK(a != NULL && b != NULL);
    if (a && b) {
        CHECK(a != b);
        CHECK(IDWriteStringList_GetCount(a) == IDWriteStringList_GetCount(b));
        EXPECT_REFS(a, 1);
        EXPECT_REFS(b, 1);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    fs_std_close(&s);
}

TEST(fset_str_list_outlives_set_and_factory)
{
    fs_std s;
    if (!fs_std_open(&s)) return;
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
    CHECK(l != NULL);
    fs_std_close(&s);   /* set, loader and factory are gone */
    /* the list owns its string storage */
    if (l) {
        UINT32 n = IDWriteStringList_GetCount(l), i;
        CHECK(n == 5);
        for (i = 0; i < n; i++) { char b[128]; CHECK_HR_OK(fs_sl_get(l, i, b, sizeof b)); CHECK(b[0] != 0); }
        CHECK(fs_sl_has(l, "SynthA Bold Italic"));
        DW_RELEASE(l);
    }
}

TEST(fset_str_list_no_duplicates)
{
    FS_STD(s);
    int id;
    for (id = 1; id <= 13; id++) {
        IDWriteStringList *l = fs_props(s.set, (DWRITE_FONT_PROPERTY_ID)id);
        if (!l) continue;
        UINT32 i, j, n = IDWriteStringList_GetCount(l);
        for (i = 0; i < n; i++)
            for (j = i + 1; j < n; j++) {
                char a[256], b[256], la[64], lb[64];
                fs_sl_get(l, i, a, sizeof a); fs_sl_get(l, j, b, sizeof b);
                fs_sl_locale(l, i, la, sizeof la); fs_sl_locale(l, j, lb, sizeof lb);
                CHECKF(strcmp(a, b) || _stricmp(la, lb), "id %d duplicate (%s, %s)", id, a, la);
            }
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_list_order_note)
{
    FS_STD(s);
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
    if (l) {
        UINT32 i, n = IDWriteStringList_GetCount(l); char b[128];
        for (i = 0; i < n; i++) { fs_sl_get(l, i, b, sizeof b); t_note("full name [%u] = %s", i, b); }
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

/* ---- per-font form: GetPropertyValues(index, id, &exists, &strings) ---------------------------------- */
TEST(fset_str_font_form_family_every_font)
{
    FS_STD(s);
    UINT32 i, n = IDWriteFontSet_GetFontCount(s.set);
    CHECKF(n == 5, "count %u", n);
    for (i = 0; i < n; i++) {
        BOOL ex = 2; IDWriteLocalizedStrings *ls = NULL; char b[64];
        CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(s.set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, &ex, &ls));
        CHECKF(ex == TRUE, "font %u has a family", i);
        CHECK(ls != NULL);
        if (ls) {
            CHECK(IDWriteLocalizedStrings_GetCount(ls) == 1);
            dw_ls_en(ls, b, sizeof b);
            CHECKF(!strcmp(b, "SynthA") || !strcmp(b, "SynthB"), "family \"%s\"", b);
            DW_RELEASE(ls);
        }
    }
    fs_std_close(&s);
}

TEST(fset_str_font_form_counts_per_family)
{
    FS_STD(s);
    UINT32 i, n = IDWriteFontSet_GetFontCount(s.set), a = 0, b = 0;
    char buf[64];
    for (i = 0; i < n; i++) {
        fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, buf, sizeof buf);
        if (!strcmp(buf, "SynthA")) a++; else if (!strcmp(buf, "SynthB")) b++;
    }
    CHECKF(a == 4 && b == 1, "SynthA %u SynthB %u", a, b);
    fs_std_close(&s);
}

TEST(fset_str_font_form_weight_style_pairs)
{
    FS_STD(s);
    UINT32 i, n = IDWriteFontSet_GetFontCount(s.set);
    int seen[4] = { 0, 0, 0, 0 };   /* regular, bold, italic, bold italic of SynthA */
    for (i = 0; i < n; i++) {
        char fam[64], w[16], st[16], full[64];
        fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, fam, sizeof fam);
        fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_WEIGHT, w, sizeof w);
        fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_STYLE, st, sizeof st);
        fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_FULL_NAME, full, sizeof full);
        if (strcmp(fam, "SynthA")) continue;
        int bold = fs_atoi(w) == 700, ital = fs_atoi(st) == 2;
        seen[(bold ? 1 : 0) + (ital ? 2 : 0)]++;
        CHECKF(fs_atoi(w) == 400 || fs_atoi(w) == 700, "weight \"%s\"", w);
        CHECKF(!strcmp(full, bold ? (ital ? "SynthA Bold Italic" : "SynthA Bold") : (ital ? "SynthA Italic" : "SynthA")), "weight %s style %s full \"%s\"", w, st, full);
    }
    CHECKF(seen[0] == 1 && seen[1] == 1 && seen[2] == 1 && seen[3] == 1, "%d %d %d %d", seen[0], seen[1], seen[2], seen[3]);
    fs_std_close(&s);
}

TEST(fset_str_font_form_out_of_range_index)
{
    FS_STD(s);
    UINT32 n = IDWriteFontSet_GetFontCount(s.set);
    UINT32 bad[] = { n, n + 1, 0x7FFFFFFFu, 0xFFFFFFFFu };
    unsigned i;
    for (i = 0; i < 4; i++) {
        BOOL ex = 99; IDWriteLocalizedStrings *ls = (IDWriteLocalizedStrings *)FS_SENT;
        HRESULT hr = IDWriteFontSet_GetPropertyValues(s.set, bad[i], DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, &ex, &ls);
        CHECKF(FAILED(hr), "index %u -> %s", bad[i], dw_hr_name(hr));
        CHECKF(ex == FALSE, "exists %d", ex);
        CHECK_OUT_NULL(ls);
        FS_DROP(ls, hr);
        t_note("index %u -> %s", bad[i], dw_hr_name(hr));
    }
    fs_std_close(&s);
}

TEST(fset_str_font_form_id_none)
{
    FS_STD(s);
    BOOL ex = 99; IDWriteLocalizedStrings *ls = NULL;
    HRESULT hr = IDWriteFontSet_GetPropertyValues(s.set, 0, DWRITE_FONT_PROPERTY_ID_NONE, &ex, &ls);
    CHECK_HR_OK(hr);
    FS_SOFT(ex == FALSE);
    FS_SOFT(ls == NULL);
    CHECK((ex == TRUE) == (ls != NULL));
    DW_RELEASE(ls);
    fs_std_close(&s);
}

TEST(fset_str_font_form_no_value_means_exists_false)
{
    FS_STD(s);
    /* fixtures have no semantic tags: exists FALSE and no object */
    BOOL ex = 99; IDWriteLocalizedStrings *ls = (IDWriteLocalizedStrings *)FS_SENT;
    HRESULT hr = IDWriteFontSet_GetPropertyValues(s.set, 0, DWRITE_FONT_PROPERTY_ID_SEMANTIC_TAG, &ex, &ls);
    CHECK_HR_OK(hr);
    CHECK((ex == TRUE) == (ls != NULL));
    FS_SOFT(ex == FALSE);
    FS_DROP(ls, hr);
    fs_std_close(&s);
}

TEST(fset_str_font_form_localized_protocol)
{
    FS_STD(s);
    BOOL ex = FALSE; IDWriteLocalizedStrings *ls = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(s.set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &ex, &ls));
    if (ls) {
        UINT32 len = 0; WCHAR b[128]; HRESULT hr;
        CHECK(IDWriteLocalizedStrings_GetCount(ls) == 1);
        CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(ls, 0, &len));
        hr = IDWriteLocalizedStrings_GetString(ls, 0, b, 128);
        CHECK_HR_OK(hr);
        CHECK(wcslen(b) == len);
        hr = IDWriteLocalizedStrings_GetString(ls, 0, b, len);
        CHECK_HR(hr, FS_E_NOTSUFF);
        hr = IDWriteLocalizedStrings_GetString(ls, 0, b, 0);
        CHECK_HR(hr, FS_E_NOTSUFF);
        UINT32 idx = 5; BOOL fnd = TRUE;
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(ls, L"en-us", &idx, &fnd));
        CHECK(fnd == TRUE && idx == 0);
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(ls, L"fr-fr", &idx, &fnd));
        CHECK(fnd == FALSE);
        CHECKF(idx == 0xFFFFFFFFu, "idx %u", idx);
        DW_RELEASE(ls);
    }
    fs_std_close(&s);
}

TEST(fset_str_font_form_matches_list_union)
{
    FS_STD(s);
    /* every per-font full name appears in the set-wide list and vice versa */
    IDWriteStringList *l = fs_props(s.set, DWRITE_FONT_PROPERTY_ID_FULL_NAME);
    UINT32 i, n = IDWriteFontSet_GetFontCount(s.set);
    CHECK(l != NULL);
    if (l) {
        int found = 0;
        for (i = 0; i < n; i++) {
            char b[128];
            fs_font_prop(s.set, i, DWRITE_FONT_PROPERTY_ID_FULL_NAME, b, sizeof b);
            CHECKF(fs_sl_has(l, b), "\"%s\"", b);
            found++;
        }
        CHECK(found == (int)n);
        DW_RELEASE(l);
    }
    fs_std_close(&s);
}

TEST(fset_str_font_form_distinct_objects)
{
    FS_STD(s);
    BOOL e1, e2; IDWriteLocalizedStrings *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(s.set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &e1, &a));
    CHECK_HR_OK(IDWriteFontSet_GetPropertyValues(s.set, 0, DWRITE_FONT_PROPERTY_ID_FULL_NAME, &e2, &b));
    CHECK(a != NULL && b != NULL && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    fs_std_close(&s);
}

TEST(fset_str_all_ids_every_font_no_throw)
{
    FS_STD(s);
    UINT32 i, n = IDWriteFontSet_GetFontCount(s.set);
    int id, nonempty[15] = { 0 };
    for (i = 0; i < n; i++)
        for (id = 0; id <= 14; id++) {
            BOOL ex = 99; IDWriteLocalizedStrings *ls = NULL;
            HRESULT hr = IDWriteFontSet_GetPropertyValues(s.set, i, (DWRITE_FONT_PROPERTY_ID)id, &ex, &ls);
            CHECKF(hr == S_OK, "font %u id %d -> %s", i, id, dw_hr_name(hr));
            if (ex) nonempty[id]++;
            CHECK((ex == TRUE) == (ls != NULL));
            DW_RELEASE(ls);
        }
    for (id = 0; id <= 14; id++) t_note("id %2d: %d of %u fonts have values", id, nonempty[id], n);
    fs_std_close(&s);
}

TEST(fset_str_system_set_all_ids)
{
    NEED_SYSSET(set);
    int id;
    for (id = 0; id <= 14; id++) {
        IDWriteStringList *l = NULL;
        HRESULT hr = IDWriteFontSet_GetPropertyValues__(set, (DWRITE_FONT_PROPERTY_ID)id, &l);
        CHECKF(hr == S_OK, "id %d -> %s", id, dw_hr_name(hr));
        if (l) { t_note("system set id %2d: %u values", id, IDWriteStringList_GetCount(l)); DW_RELEASE(l); }
    }
    DW_RELEASE(set);
}

TEST(fset_str_system_numeric_ranges)
{
    NEED_SYSSET(set);
    struct { DWRITE_FONT_PROPERTY_ID id; int lo, hi; } t[] = {
        { DWRITE_FONT_PROPERTY_ID_WEIGHT, 1, 999 }, { DWRITE_FONT_PROPERTY_ID_STRETCH, 1, 9 }, { DWRITE_FONT_PROPERTY_ID_STYLE, 0, 2 } };
    int k;
    for (k = 0; k < 3; k++) {
        IDWriteStringList *l = fs_props(set, t[k].id);
        CHECK(l != NULL);
        if (!l) continue;
        UINT32 i, n = IDWriteStringList_GetCount(l);
        CHECK(n > 0);
        for (i = 0; i < n; i++) {
            char b[32]; char *end;
            CHECK_HR_OK(fs_sl_get(l, i, b, sizeof b));
            long v = strtol(b, &end, 10);
            CHECKF(*end == 0 && end != b && v >= t[k].lo && v <= t[k].hi, "id %d value \"%s\"", (int)t[k].id, b);
        }
        DW_RELEASE(l);
    }
    DW_RELEASE(set);
}

TEST(fset_str_system_family_names)
{
    NEED_SYSSET(set);
    if (!dw_have_font(dw_shared(), L"Arial")) { t_skip("Arial not installed"); DW_RELEASE(set); return; }
    IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
    CHECK(l != NULL);
    if (l) {
        CHECK(fs_sl_has(l, "Arial"));
        CHECK(IDWriteStringList_GetCount(l) > 50);
        DW_RELEASE(l);
    }
    l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME);
    if (l) { CHECK(fs_sl_has(l, "Arial")); DW_RELEASE(l); }
    DW_RELEASE(set);
}

TEST(fset_str_system_locale_forms)
{
    NEED_SYSSET(set);
    const char *loc[] = { "en-us", "fr-fr", "ja-jp", "zz" };
    unsigned i;
    UINT32 base = 0;
    {
        IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
        if (l) { base = IDWriteStringList_GetCount(l); DW_RELEASE(l); }
    }
    for (i = 0; i < 4; i++) {
        WCHAR w[32]; IDWriteStringList *l = NULL;
        dw_wide(loc[i], w, 32);
        HRESULT hr = IDWriteFontSet_GetPropertyValues_(set, DWRITE_FONT_PROPERTY_ID_WEIGHT_STRETCH_STYLE_FAMILY_NAME, w, &l);
        CHECKF(hr == S_OK && l, "locale %s -> %s", loc[i], dw_hr_name(hr));
        if (l) {
            UINT32 n = IDWriteStringList_GetCount(l);
            /* the single-locale flavour has at most one string per distinct value, the full flavour one per (locale, value) */
            CHECKF(n > 50, "locale %s: %u strings", loc[i], n);
            FS_SOFT(n <= base);
            DW_RELEASE(l);
        }
    }
    DW_RELEASE(set);
}
