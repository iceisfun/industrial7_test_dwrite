/* IDWriteLocalizedStrings call protocol, run against every entry point that hands out one.
 *
 * Verified in dwrite.dll (Win10 19041):
 *   GetString / GetLocaleName: size == 0 -> 0x8007007A before anything else (buffer untouched, NULL buffer allowed);
 *     otherwise buffer[0] = 0, then the index is range checked, then a bounded string copy: a too small buffer yields
 *     0x8007007A with the string truncated to size-1 characters and NUL terminated; size > 0x7FFFFFFF yields
 *     E_INVALIDARG (0x80070057) with buffer[0] = 0.
 *   GetStringLength / GetLocaleNameLength: *length = 0xFFFFFFFF first, index out of range fails.
 *   FindLocaleName: *index = UINT_MAX and *exists = FALSE first; NULL locale -> E_INVALIDARG; unknown -> S_OK.
 */
#include "lsfx.h"

#define PERSRC(name) \
    static void name##_run(ls_fx *x, int src, IDWriteLocalizedStrings *s); \
    static void name##_drive(int src) { \
        LS_NEED_FACTORY(); LS_NEED_MULTI(x); \
        IDWriteLocalizedStrings *s = NULL; HRESULT hr = ls_get_src(&x, src, &s); \
        if (hr == S_FALSE) { ls_close(&x); t_skip("%s unsupported", ls_src_name(src)); return; } \
        CHECKF(hr == S_OK && s, "%s -> %s", ls_src_name(src), dw_hr_name(hr)); \
        if (s) { name##_run(&x, src, s); IDWriteLocalizedStrings_Release(s); } \
        ls_close(&x); \
    } \
    TEST(name##_face_family)    { name##_drive(LS_S_FACE_FAMILY); } \
    TEST(name##_face_face)      { name##_drive(LS_S_FACE_FACE); } \
    TEST(name##_face_info)      { name##_drive(LS_S_FACE_INFO_WIN32FAM); } \
    TEST(name##_family_family)  { name##_drive(LS_S_FAMILY_FAMILY); } \
    TEST(name##_font_face)      { name##_drive(LS_S_FONT_FACE); } \
    TEST(name##_font_info)      { name##_drive(LS_S_FONT_INFO_WIN32FAM); } \
    TEST(name##_font_info_full) { name##_drive(LS_S_FONT_INFO_FULL); } \
    TEST(name##_face6_family)   { name##_drive(LS_S_FACE6_FAMILY); } \
    TEST(name##_face6_face)     { name##_drive(LS_S_FACE6_FACE); } \
    static void name##_run(ls_fx *x, int src, IDWriteLocalizedStrings *s)

#define E_BUF  DW_E_NOT_SUFFICIENT_BUFFER

/* ---- GetCount --------------------------------------------------------------------------------- */
PERSRC(lsp_count) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), want = ls_src_expect_count(src);
    if (want) EXPECT_EQ(n, want); else { CHECK(n >= 1); t_note("%s: count %u", ls_src_name(src), n); }
    EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), n);
    EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), n);
}

/* ---- FindLocaleName -------------------------------------------------------------------------------- */
PERSRC(lsp_find_each) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), i;
    for (i = 0; i < n; i++) {
        WCHAR *loc = ls_dup_locale(s, i); UINT32 idx = 77; BOOL ex = 77;
        CHECK(loc != NULL); if (!loc) continue;
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, loc, &idx, &ex));
        CHECK(ex == TRUE); EXPECT_EQ(idx, i);
        free(loc);
    }
}
PERSRC(lsp_find_case) {
    static const WCHAR *const v[] = { L"en-us", L"EN-US", L"En-Us", L"en-US", L"eN-uS" };
    int base = ls_index_of(s, L"en-us"), k;
    CHECK(base >= 0);
    for (k = 0; k < 5; k++) {
        UINT32 idx = 0; BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, v[k], &idx, &ex));
        CHECK(ex == TRUE); EXPECT_EQ(idx, (UINT32)base);
    }
}
PERSRC(lsp_find_unknown) {
    static const WCHAR *const v[] = { L"zz-zz", L"tlh", L"xx", L"qps-ploc", L"en-zz-zz-zz", L"x-private", L"1234", L"en us" };
    int k;
    for (k = 0; k < 8; k++) {
        UINT32 idx = 5; BOOL ex = TRUE;
        CHECK_HR(IDWriteLocalizedStrings_FindLocaleName(s, v[k], &idx, &ex), S_OK);
        CHECK(ex == FALSE); EXPECT_EQ(idx, 0xFFFFFFFFu);
    }
}
PERSRC(lsp_find_null) {
    UINT32 idx = 7; BOOL ex = TRUE;
    if (t_is_wine()) { t_skip("NULL locale name crashes the emulator"); return; }
    CHECK_HR(IDWriteLocalizedStrings_FindLocaleName(s, NULL, &idx, &ex), DW_E_INVALIDARG);
    EXPECT_EQ(idx, 0xFFFFFFFFu); CHECK(ex == FALSE);
}
PERSRC(lsp_find_resets_outputs) {
    /* a hit overwrites the prefilled garbage, a miss restores the documented defaults */
    UINT32 idx = 0x1234; BOOL ex = 0x55;
    CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"en-us", &idx, &ex));
    CHECK(ex == TRUE); CHECK(idx < IDWriteLocalizedStrings_GetCount(s));
    idx = 0x1234; ex = 0x55;
    CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"zz-zz", &idx, &ex));
    CHECK(ex == FALSE); EXPECT_EQ(idx, 0xFFFFFFFFu);
}
PERSRC(lsp_find_empty_and_long) {
    UINT32 idx = 3; BOOL ex = TRUE; WCHAR big[260]; int i;
    HRESULT hr = IDWriteLocalizedStrings_FindLocaleName(s, L"", &idx, &ex);
    t_note("FindLocaleName(\"\") -> %s exists=%d idx=%u", dw_hr_name(hr), ex, idx);
    if (SUCCEEDED(hr)) CHECK(ex == FALSE);
    for (i = 0; i < 259; i++) big[i] = L'a'; big[259] = 0;
    idx = 3; ex = TRUE;
    hr = IDWriteLocalizedStrings_FindLocaleName(s, big, &idx, &ex);
    t_note("FindLocaleName(259 chars) -> %s exists=%d", dw_hr_name(hr), ex);
    if (SUCCEEDED(hr)) { CHECK(ex == FALSE); EXPECT_EQ(idx, 0xFFFFFFFFu); }
}

/* ---- locale names ------------------------------------------------------------------------------------ */
PERSRC(lsp_locale_len) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), i;
    for (i = 0; i < n; i++) {
        UINT32 len = 0xABCD; WCHAR *loc;
        CHECK_HR_OK(IDWriteLocalizedStrings_GetLocaleNameLength(s, i, &len));
        loc = ls_dup_locale(s, i);
        CHECK(loc != NULL);
        if (loc) { EXPECT_EQ((UINT32)lstrlenW(loc), len); CHECK(len > 0); free(loc); }
    }
}
PERSRC(lsp_locale_buffers) {
    UINT32 len = 0, i, n = IDWriteLocalizedStrings_GetCount(s);
    for (i = 0; i < n; i++) {
        WCHAR b[300]; int k;
        IDWriteLocalizedStrings_GetLocaleNameLength(s, i, &len);
        /* exactly len+1: ok */
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR_OK(IDWriteLocalizedStrings_GetLocaleName(s, i, b, len + 1));
        EXPECT_EQ((UINT32)lstrlenW(b), len);
        /* more room: ok, nothing past the terminator touched */
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR_OK(IDWriteLocalizedStrings_GetLocaleName(s, i, b, len + 20));
        EXPECT_EQ(b[len + 1], 0x5A5A);
        /* size == len: one short, truncated and terminated */
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, i, b, len), E_BUF);
        EXPECT_EQ((UINT32)lstrlenW(b), len - 1);
        EXPECT_EQ(b[len], 0x5A5A);
        /* size == 1: only the terminator fits */
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, i, b, 1), E_BUF);
        EXPECT_EQ(b[0], 0); EXPECT_EQ(b[1], 0x5A5A);
        /* size == 0: rejected before the buffer is touched; NULL buffer is fine */
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, i, b, 0), E_BUF);
        EXPECT_EQ(b[0], 0x5A5A);
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, i, NULL, 0), E_BUF);
        /* sizes the bounded copy refuses outright */
        b[0] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, i, b, 0x80000000u), DW_E_INVALIDARG);
        EXPECT_EQ(b[0], 0);
        b[0] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, i, b, 0xFFFFFFFFu), DW_E_INVALIDARG);
        EXPECT_EQ(b[0], 0);
    }
}

/* ---- strings ------------------------------------------------------------------------------------------- */
PERSRC(lsp_string_len) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), i;
    for (i = 0; i < n; i++) {
        UINT32 len = 0xABCD; WCHAR *str;
        CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, i, &len));
        str = ls_dup_string(s, i);
        CHECK(str != NULL);
        if (str) { EXPECT_EQ((UINT32)lstrlenW(str), len); free(str); }
    }
}
PERSRC(lsp_string_buffers) {
    UINT32 len = 0, i, n = IDWriteLocalizedStrings_GetCount(s);
    for (i = 0; i < n; i++) {
        WCHAR b[300], *full; int k;
        IDWriteLocalizedStrings_GetStringLength(s, i, &len);
        if (len > 250) continue;
        full = ls_dup_string(s, i);
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, i, b, len + 1));
        EXPECT_EQ((UINT32)lstrlenW(b), len);
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, i, b, len + 20));
        EXPECT_EQ(b[len + 1], 0x5A5A);
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, i, b, len), E_BUF);
        CHECK(len >= 1 && (UINT32)lstrlenW(b) == len - 1);
        if (full && len >= 1) CHECK(!memcmp(b, full, (len - 1) * sizeof(WCHAR)));   /* truncation keeps the prefix */
        EXPECT_EQ(b[len], 0x5A5A);
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, i, b, 1), E_BUF);
        EXPECT_EQ(b[0], 0); EXPECT_EQ(b[1], 0x5A5A);
        for (k = 0; k < 300; k++) b[k] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, i, b, 0), E_BUF);
        EXPECT_EQ(b[0], 0x5A5A);
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, i, NULL, 0), E_BUF);
        b[0] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, i, b, 0x80000000u), DW_E_INVALIDARG);
        EXPECT_EQ(b[0], 0);
        b[0] = 0x5A5A;
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, i, b, 0xFFFFFFFFu), DW_E_INVALIDARG);
        EXPECT_EQ(b[0], 0);
        free(full);
    }
}
PERSRC(lsp_large_size_ok) {
    /* 0x7FFFFFFF is the largest accepted size; the copy stops at the terminator so the huge claim is harmless */
    WCHAR b[260]; UINT32 len = 0;
    IDWriteLocalizedStrings_GetStringLength(s, 0, &len);
    if (len > 200) return;
    memset(b, 0x5A, sizeof b);
    CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, b, 0x7FFFFFFFu));
    EXPECT_EQ((UINT32)lstrlenW(b), len);
    IDWriteLocalizedStrings_GetLocaleNameLength(s, 0, &len);
    memset(b, 0x5A, sizeof b);
    CHECK_HR_OK(IDWriteLocalizedStrings_GetLocaleName(s, 0, b, 0x7FFFFFFFu));
    EXPECT_EQ((UINT32)lstrlenW(b), len);
}

/* ---- index out of range ------------------------------------------------------------------------------------ */
PERSRC(lsp_oor_index) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), k;
    UINT32 bad[4]; bad[0] = n; bad[1] = n + 1; bad[2] = 1000; bad[3] = 0xFFFFFFFFu;
    for (k = 0; k < 4; k++) {
        UINT32 len = 100; WCHAR b[8]; HRESULT hr;
        hr = IDWriteLocalizedStrings_GetStringLength(s, bad[k], &len);
        CHECK(FAILED(hr)); if (hr != E_FAIL) t_note("GetStringLength(oor) -> %s", dw_hr_name(hr));
        EXPECT_EQ(len, 0xFFFFFFFFu);
        len = 100;
        hr = IDWriteLocalizedStrings_GetLocaleNameLength(s, bad[k], &len);
        CHECK(FAILED(hr)); if (hr != E_FAIL) t_note("GetLocaleNameLength(oor) -> %s", dw_hr_name(hr));
        EXPECT_EQ(len, 0xFFFFFFFFu);
        b[0] = 1;
        hr = IDWriteLocalizedStrings_GetString(s, bad[k], b, 8);
        CHECK(FAILED(hr)); if (hr != E_FAIL) t_note("GetString(oor) -> %s", dw_hr_name(hr));
        EXPECT_EQ(b[0], 0);
        b[0] = 1;
        hr = IDWriteLocalizedStrings_GetLocaleName(s, bad[k], b, 8);
        CHECK(FAILED(hr)); if (hr != E_FAIL) t_note("GetLocaleName(oor) -> %s", dw_hr_name(hr));
        EXPECT_EQ(b[0], 0);
        /* the size check precedes the index check */
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, bad[k], NULL, 0), E_BUF);
        CHECK_HR(IDWriteLocalizedStrings_GetLocaleName(s, bad[k], NULL, 0), E_BUF);
    }
}
PERSRC(lsp_valid_after_oor) {
    /* a failed call leaves the object usable */
    UINT32 len = 0, n = IDWriteLocalizedStrings_GetCount(s);
    IDWriteLocalizedStrings_GetStringLength(s, n, &len);
    EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), n);
    CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, n - 1, &len));
    CHECK(len < 1100);
}

/* ---- COM identity / lifetime ---------------------------------------------------------------------------------- */
PERSRC(lsp_qi) {
    dw_qi_row rows[] = {
        DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteLocalizedStrings, 1), DW_QI_ROW(IDWriteStringList, 1),
        DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFactory, 0),
        DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontList, 0),
        DW_QI_ROW(IDWriteTextFormat, 0), DW_QI_ROW(IDWriteFontFile, 0), DW_QI_ROW(IDWriteFontSet, 0),
    };
    DW_CHECK_QI(s, rows);
}
PERSRC(lsp_refcount) {
    IDWriteLocalizedStrings *q = NULL; ULONG r;
    EXPECT_REFS(s, 1);
    r = IDWriteLocalizedStrings_AddRef(s); EXPECT_EQ(r, 2);
    r = IDWriteLocalizedStrings_AddRef(s); EXPECT_EQ(r, 3);
    EXPECT_REFS(s, 3);
    r = IDWriteLocalizedStrings_Release(s); EXPECT_EQ(r, 2);
    CHECK_HR_OK(IDWriteLocalizedStrings_QueryInterface(s, &IID_IDWriteLocalizedStrings, (void **)&q));
    CHECK(q == s);
    EXPECT_REFS(s, 3);
    IDWriteLocalizedStrings_Release(q);
    r = IDWriteLocalizedStrings_Release(s); EXPECT_EQ(r, 1);
    EXPECT_REFS(s, 1);
}
PERSRC(lsp_qi_failure_zeroes) {
    void *p = (void *)(ULONG_PTR)0x1;
    CHECK_HR(IDWriteLocalizedStrings_QueryInterface(s, &IID_IDWriteFont, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    p = (void *)(ULONG_PTR)0x1;
    CHECK_HR(IDWriteLocalizedStrings_QueryInterface(s, &IID_IDWriteFactory, &p), E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    EXPECT_REFS(s, 1);
}
PERSRC(lsp_distinct_objects) {
    IDWriteLocalizedStrings *s2 = NULL;
    HRESULT hr = ls_get_src(x, src, &s2);
    CHECK(hr == S_OK && s2);
    if (s2) {
        CHECK(s2 != s);
        EXPECT_REFS(s, 1); EXPECT_REFS(s2, 1);
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), IDWriteLocalizedStrings_GetCount(s2));
        /* releasing one does not affect the other */
        IDWriteLocalizedStrings_Release(s2);
        CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1);
    }
}
PERSRC(lsp_stringlist_view) {
    IDWriteStringList *l = NULL; UINT32 n = IDWriteLocalizedStrings_GetCount(s), i;
    if (FAILED(IDWriteLocalizedStrings_QueryInterface(s, &IID_IDWriteStringList, (void **)&l)) || !l) { CHECK(0); return; }
    EXPECT_EQ(IDWriteStringList_GetCount(l), n);
    for (i = 0; i < n; i++) {
        WCHAR *a = ls_dup_string(s, i), *loc = ls_dup_locale(s, i); UINT32 len = 0; WCHAR b[300];
        if (a && lstrlenW(a) < 250) {
            CHECK_HR_OK(IDWriteStringList_GetStringLength(l, i, &len)); EXPECT_EQ((UINT32)lstrlenW(a), len);
            CHECK_HR_OK(IDWriteStringList_GetString(l, i, b, 300)); EXPECT_WSTR(a, b);
        }
        if (loc) {
            CHECK_HR_OK(IDWriteStringList_GetLocaleNameLength(l, i, &len)); EXPECT_EQ((UINT32)lstrlenW(loc), len);
            CHECK_HR_OK(IDWriteStringList_GetLocaleName(l, i, b, 300)); EXPECT_WSTR(loc, b);
            CHECK_HR(IDWriteStringList_GetLocaleName(l, i, b, 0), E_BUF);
        }
        free(a); free(loc);
    }
    IDWriteStringList_Release(l);
}
PERSRC(lsp_sweep) {
    EXPECT_EQ(ls_sweep(s, ls_src_name(src)), 0);
}
PERSRC(lsp_sorted_unique) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), i; int sorted = 1, unique = 1; WCHAR *prev = NULL;
    for (i = 0; i < n; i++) {
        WCHAR *loc = ls_dup_locale(s, i);
        if (prev && loc) {
            int c = CompareStringOrdinal(prev, -1, loc, -1, TRUE);
            if (c == CSTR_EQUAL) unique = 0;
            if (c == CSTR_GREATER_THAN) sorted = 0;
        }
        free(prev); prev = loc;
    }
    free(prev);
    CHECK(unique);                       /* one entry per language */
    t_note("%s: locale order ascending ignoring case: %d", ls_src_name(src), sorted);
    if (n == LS_NMULTI) {
        int de = ls_index_of(s, L"de-de"), en = ls_index_of(s, L"en-us"), fr = ls_index_of(s, L"fr-fr"),
            ja = ls_index_of(s, L"ja-jp"), ru = ls_index_of(s, L"ru-ru");
        CHECK(de >= 0 && en >= 0 && fr >= 0 && ja >= 0 && ru >= 0);
        CHECK(sorted);
        CHECK(de < en && en < fr && fr < ja && ja < ru);
    }
}
PERSRC(lsp_content_en) {
    WCHAR *e = ls_en(s);
    CHECK(e != NULL);
    if (e) { EXPECT_WSTR(e, ls_src_expect_en(src)); free(e); }
}
PERSRC(lsp_content_locale_tags) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), i;
    for (i = 0; i < n; i++) {
        WCHAR *loc = ls_dup_locale(s, i); int k, known = 0;
        if (!loc) continue;
        for (k = 0; k < LS_NMULTI; k++) { WCHAR w[16]; dw_wide(ls_multi[k].tag, w, 16); if (ls_wcsieq(w, loc)) { known = 1; if (lstrcmpW(w, loc)) t_note("locale spelled \"%ls\" (expected tag \"%s\")", loc, ls_multi[k].tag); } }
        CHECKF(known, "unexpected locale name %ls", loc);
        free(loc);
    }
}
PERSRC(lsp_content_all_locales) {
    unsigned want = ls_src_expect_count(src); int k;
    if (want != LS_NMULTI) { t_skip("source has no pinned per-locale content"); return; }
    for (k = 0; k < LS_NMULTI; k++) {
        WCHAR w[16]; int i; WCHAR *str; const WCHAR *exp;
        dw_wide(ls_multi[k].tag, w, 16);
        i = ls_index_of(s, w);
        CHECKF(i >= 0, "locale %s missing", ls_multi[k].tag);
        if (i < 0) continue;
        str = ls_dup_string(s, (UINT32)i);
        exp = (src == LS_S_FACE_FACE || src == LS_S_FONT_FACE) ? ls_multi[k].sub : ls_multi[k].fam;
        CHECK(str != NULL);
        if (str) { CHECKF(!lstrcmpW(str, exp), "%s: got %ls want %ls", ls_multi[k].tag, str, exp); free(str); }
    }
}
PERSRC(lsp_repeatable) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), i; int pass;
    for (pass = 0; pass < 3; pass++)
        for (i = 0; i < n; i++) {
            WCHAR *a = ls_dup_string(s, i), *b = ls_dup_string(s, i);
            CHECK(a && b && !lstrcmpW(a, b)); free(a); free(b);
        }
}
