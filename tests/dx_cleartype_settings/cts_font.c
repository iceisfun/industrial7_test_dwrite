/* AddFontResourceEx(FR_PRIVATE / FR_NOT_ENUM) of a generated font file and the DirectWrite system collection.
 *
 * FR_PRIVATE fonts are visible to the calling process' GDI only; the DirectWrite system collection is built from the installed
 * fonts, so a private font never shows up in it, also not after GetSystemFontCollection(checkForUpdates TRUE).  Nothing is
 * installed system-wide: the file lives in the scratch directory and every resource added is removed before the test ends. */
#include "cts.h"

typedef struct { char path[MAX_PATH]; WCHAR wpath[MAX_PATH]; char fam[64]; WCHAR wfam[64]; int ok; } pf;
static int pf_seq;
static int pf_make(pf *p, const char *tag) {
    uint8_t *blob; size_t n = 0; FILE *fp; char leaf[96];
    memset(p, 0, sizeof *p);
    snprintf(p->fam, sizeof p->fam, "CtsPriv%s%lu_%d", tag, (unsigned long)GetCurrentProcessId(), ++pf_seq);
    dw_wide(p->fam, p->wfam, 64);
    blob = ttf_build_styled(p->fam, 400, 5, 0, &n);
    if (!blob) return 0;
    snprintf(leaf, sizeof leaf, "%s.ttf", p->fam);
    t_tmpfile(p->path, sizeof p->path, leaf);
    fp = fopen(p->path, "wb");
    if (!fp) { free(blob); return 0; }
    fwrite(blob, 1, n, fp); fclose(fp); free(blob);
    MultiByteToWideChar(CP_ACP, 0, p->path, -1, p->wpath, MAX_PATH);
    p->ok = 1;
    return 1;
}
#define PF_OR_SKIP(p, tag) do { if (!pf_make(&(p), tag)) { t_skip("cannot write the scratch font"); return; } } while (0)

static BOOL family_in_system(IDWriteFactory *f, BOOL check, const WCHAR *fam) {
    IDWriteFontCollection *c = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, check)) || !c) return -1;
    IDWriteFontCollection_FindFamilyName(c, fam, &idx, &ex);
    IDWriteFontCollection_Release(c);
    return ex;
}
static UINT32 system_family_count(IDWriteFactory *f, BOOL check) {
    IDWriteFontCollection *c = NULL; UINT32 n = 0xFFFFFFFF;
    if (SUCCEEDED(IDWriteFactory_GetSystemFontCollection(f, &c, check)) && c) { n = IDWriteFontCollection_GetFontFamilyCount(c); IDWriteFontCollection_Release(c); }
    return n;
}
static int gdi_face_is(const char *fam, char *got, size_t cap) {
    HDC dc = CreateCompatibleDC(NULL); HFONT hf, old; int ok = 0;
    got[0] = 0;
    if (!dc) return 0;
    hf = CreateFontA(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, fam);
    if (hf) {
        old = SelectObject(dc, hf);
        ok = GetTextFaceA(dc, (int)cap, got) > 0;
        SelectObject(dc, old); DeleteObject(hf);
    }
    DeleteDC(dc);
    return ok;
}
static int CALLBACK enum_cb(const LOGFONTA *lf, const TEXTMETRICA *tm, DWORD type, LPARAM l) { (*(int *)l)++; return 1; }
static int gdi_enum_count(const char *fam) {
    HDC dc = GetDC(NULL); LOGFONTA lf; int n = 0;
    memset(&lf, 0, sizeof lf); lf.lfCharSet = DEFAULT_CHARSET; snprintf(lf.lfFaceName, LF_FACESIZE, "%s", fam);
    EnumFontFamiliesExA(dc, &lf, enum_cb, (LPARAM)&n, 0);
    ReleaseDC(NULL, dc);
    return n;
}

/* ---- FR_PRIVATE ----------------------------------------------------------------------------------------------------------- */
TEST(fr_private_add_and_remove_counts) {
    pf p; int n; BOOL r;
    PF_OR_SKIP(p, "A");
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font (error %lu)", GetLastError()); return; }
    EXPECT_EQ(n, 1);                              /* the file holds one font */
    r = RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    CHECK(r);
}
TEST(fr_private_not_in_system_collection_check_updates) {
    IDWriteFactory *f = dw_shared(); pf p; int n; BOOL in;
    PF_OR_SKIP(p, "B");
    REQUIRE(f);
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    in = family_in_system(f, TRUE, p.wfam);
    CHECKF(in == FALSE, "private family '%s' listed by GetSystemFontCollection(TRUE): %d", p.fam, (int)in);
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
}
TEST(fr_private_not_in_system_collection_no_update) {
    IDWriteFactory *f = dw_shared(); pf p; int n;
    PF_OR_SKIP(p, "C");
    REQUIRE(f);
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    CHECK(family_in_system(f, FALSE, p.wfam) == FALSE);
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
}
TEST(fr_private_family_count_unchanged) {
    IDWriteFactory *f = dw_shared(); pf p; UINT32 before, during, after; int n;
    PF_OR_SKIP(p, "D");
    REQUIRE(f);
    before = system_family_count(f, TRUE);
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    during = system_family_count(f, TRUE);
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    after = system_family_count(f, TRUE);
    CHECKF(before == during, "family count %u -> %u while a private font is loaded", before, during);
    CHECKF(before == after, "family count %u -> %u after removing it", before, after);
}
TEST(fr_private_not_in_isolated_or_new_factories) {
    pf p; int n; IDWriteFactory *nf; int i;
    PF_OR_SKIP(p, "E");
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    for (i = 0; i < 2; i++) {
        nf = dw_new_factory(i ? DWRITE_FACTORY_TYPE_SHARED : DWRITE_FACTORY_TYPE_ISOLATED);
        if (!nf) continue;
        CHECKF(family_in_system(nf, TRUE, p.wfam) == FALSE, "factory type %d lists the private font", i);
        IDWriteFactory_Release(nf);
    }
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
}
TEST(fr_private_not_in_system_font_set) {
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); pf p; int n; IDWriteFontSet *s0 = NULL, *s1 = NULL;
    PF_OR_SKIP(p, "F");
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontSet(f3, &s0));
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { DW_RELEASE(s0); t_skip("GDI rejected the generated font"); return; }
    if (SUCCEEDED(IDWriteFactory3_GetSystemFontSet(f3, &s1)) && s1) {
        UINT32 a = IDWriteFontSet_GetFontCount(s0), b = IDWriteFontSet_GetFontCount(s1);
        CHECKF(a == b, "system font set size %u -> %u", a, b);
    }
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    DW_RELEASE(s0); DW_RELEASE(s1);
}
TEST(fr_private_visible_to_gdi_in_this_process) {
    pf p; int n; char got[LF_FACESIZE + 1];
    PF_OR_SKIP(p, "G");
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    CHECK(gdi_face_is(p.fam, got, sizeof got));
    CHECKF(!lstrcmpiA(got, p.fam), "GDI selected face '%s' for '%s'", got, p.fam);
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    gdi_face_is(p.fam, got, sizeof got);
    CHECKF(lstrcmpiA(got, p.fam) != 0, "GDI still maps '%s' after RemoveFontResourceEx", p.fam);
}
TEST(fr_private_enumerable_in_gdi) {
    pf p; int n, cnt;
    PF_OR_SKIP(p, "H");
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    cnt = gdi_enum_count(p.fam);
    CHECKF(cnt >= 1, "EnumFontFamiliesEx found %d", cnt);
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    CHECK(gdi_enum_count(p.fam) == 0);
}
TEST(fr_private_gdi_interop_lookup_note) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; pf p; int n; LOGFONTW lf; IDWriteFont *font = NULL; HRESULT hr;
    PF_OR_SKIP(p, "I");
    REQUIRE(f);
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { IDWriteGdiInterop_Release(gi); t_skip("GDI rejected the generated font"); return; }
    memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET; lstrcpynW(lf.lfFaceName, p.wfam, LF_FACESIZE);
    hr = IDWriteGdiInterop_CreateFontFromLOGFONT(gi, &lf, &font);
    t_note("CreateFontFromLOGFONT(private face) -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && font) {
        IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *names = NULL; char nm[128] = "";
        if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &fam)) && fam && SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &names)) && names) dw_ls_en(names, nm, sizeof nm);
        t_note("  resolved to family '%s'", nm);
        DW_RELEASE(names); DW_RELEASE(fam);
    } else CHECK(font == NULL);       /* failure leaves the out pointer NULL */
    DW_RELEASE(font);
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    IDWriteGdiInterop_Release(gi);
}
TEST(fr_private_face_from_hdc_note) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; pf p; int n; HDC dc; HFONT hf, old; IDWriteFontFace *face = NULL; HRESULT hr;
    PF_OR_SKIP(p, "J");
    REQUIRE(f);
    DW_REQUIRE_OK(IDWriteFactory_GetGdiInterop(f, &gi));
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { IDWriteGdiInterop_Release(gi); t_skip("GDI rejected the generated font"); return; }
    dc = CreateCompatibleDC(NULL);
    hf = CreateFontA(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, p.fam);
    old = SelectObject(dc, hf);
    hr = IDWriteGdiInterop_CreateFontFaceFromHdc(gi, dc, &face);
    t_note("CreateFontFaceFromHdc(private font selected) -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr)) CHECK(face != NULL); else CHECK(face == NULL);
    DW_RELEASE(face);
    SelectObject(dc, old); DeleteObject(hf); DeleteDC(dc);
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    IDWriteGdiInterop_Release(gi);
}
TEST(fr_private_file_still_loadable_by_directwrite_reference) {
    IDWriteFactory *f = dw_shared(); pf p; int n; IDWriteFontFile *ff = NULL; BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf = 0;
    PF_OR_SKIP(p, "K");
    REQUIRE(f);
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    /* the GDI resource does not lock out a DirectWrite reference to the same file */
    CHECK_HR_OK(IDWriteFactory_CreateFontFileReference(f, p.wpath, NULL, &ff));
    if (ff) {
        CHECK_HR_OK(IDWriteFontFile_Analyze(ff, &sup, &ft, &fct, &nf));
        CHECK(sup); EXPECT_EQ(fct, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(nf, 1);
    }
    DW_RELEASE(ff);
    RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
}
TEST(fr_private_double_add_refcounted) {
    pf p; int n1, n2; BOOL r1, r2, r3;
    PF_OR_SKIP(p, "L");
    n1 = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n1 == 0) { t_skip("GDI rejected the generated font"); return; }
    n2 = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    EXPECT_EQ(n1, 1); EXPECT_EQ(n2, 1);
    r1 = RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    r2 = RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    CHECK(r1); CHECK(r2);
    SetLastError(0);
    r3 = RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    t_note("third RemoveFontResourceEx -> %d (error %lu)", r3, GetLastError());
}
TEST(fr_remove_without_add_fails) {
    pf p; BOOL r;
    PF_OR_SKIP(p, "M");
    r = RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    CHECK(!r);
}
TEST(fr_add_missing_file_returns_zero) {
    WCHAR path[MAX_PATH]; int n;
    MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, path, MAX_PATH);
    lstrcatW(path, L"no_such_font_file.ttf");
    n = AddFontResourceExW(path, FR_PRIVATE, NULL);
    EXPECT_EQ(n, 0);
}
TEST(fr_add_non_font_file_returns_zero) {
    pf p; FILE *fp; int n;
    PF_OR_SKIP(p, "N");
    fp = fopen(p.path, "wb"); if (!fp) { t_skip("cannot write"); return; }
    fwrite("this is not a font file at all.............", 1, 44, fp); fclose(fp);
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    EXPECT_EQ(n, 0);
    if (n) RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL);
}
TEST(fr_private_two_distinct_fonts_independent) {
    pf a, b; int na, nb; IDWriteFactory *f = dw_shared();
    PF_OR_SKIP(a, "O"); PF_OR_SKIP(b, "P");
    REQUIRE(f);
    na = AddFontResourceExW(a.wpath, FR_PRIVATE, NULL);
    nb = AddFontResourceExW(b.wpath, FR_PRIVATE, NULL);
    if (na == 0 || nb == 0) { if (na) RemoveFontResourceExW(a.wpath, FR_PRIVATE, NULL); if (nb) RemoveFontResourceExW(b.wpath, FR_PRIVATE, NULL); t_skip("GDI rejected a generated font"); return; }
    CHECK(family_in_system(f, TRUE, a.wfam) == FALSE);
    CHECK(family_in_system(f, TRUE, b.wfam) == FALSE);
    CHECK(RemoveFontResourceExW(a.wpath, FR_PRIVATE, NULL));
    {
        char got[LF_FACESIZE + 1];
        gdi_face_is(b.fam, got, sizeof got);
        CHECKF(!lstrcmpiA(got, b.fam), "second font lost when the first was removed: '%s'", got);
    }
    CHECK(RemoveFontResourceExW(b.wpath, FR_PRIVATE, NULL));
}

/* ---- FR_NOT_ENUM -------------------------------------------------------------------------------------------------------- */
TEST(fr_not_enum_private_hidden_from_gdi_enum_and_directwrite) {
    pf p; int n; IDWriteFactory *f = dw_shared();
    PF_OR_SKIP(p, "Q");
    REQUIRE(f);
    n = AddFontResourceExW(p.wpath, FR_PRIVATE | FR_NOT_ENUM, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    /* documented: a FR_NOT_ENUM font cannot be enumerated by any process, including the caller */
    CHECKF(gdi_enum_count(p.fam) == 0, "FR_NOT_ENUM font enumerated");
    CHECK(family_in_system(f, TRUE, p.wfam) == FALSE);
    CHECK(RemoveFontResourceExW(p.wpath, FR_PRIVATE | FR_NOT_ENUM, NULL));
}
TEST(fr_not_enum_private_still_selectable_by_name) {
    pf p; int n; char got[LF_FACESIZE + 1];
    PF_OR_SKIP(p, "R");
    n = AddFontResourceExW(p.wpath, FR_PRIVATE | FR_NOT_ENUM, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    gdi_face_is(p.fam, got, sizeof got);
    t_note("FR_NOT_ENUM|FR_PRIVATE font selected by name -> face '%s'", got);
    CHECK(!lstrcmpiA(got, p.fam));
    RemoveFontResourceExW(p.wpath, FR_PRIVATE | FR_NOT_ENUM, NULL);
}
TEST(fr_not_enum_session_font_note) {
    /* not private: the font is added to the session font table (removed again below) */
    pf p; int n; IDWriteFactory *f = dw_shared();
    PF_OR_SKIP(p, "S");
    REQUIRE(f);
    n = AddFontResourceExW(p.wpath, FR_NOT_ENUM, NULL);
    if (n == 0) { t_skip("AddFontResourceEx(FR_NOT_ENUM) failed (error %lu)", GetLastError()); return; }
    t_note("FR_NOT_ENUM session font: gdi enum %d, DirectWrite system collection(TRUE) %d, collection(FALSE) %d",
           gdi_enum_count(p.fam), (int)family_in_system(f, TRUE, p.wfam), (int)family_in_system(f, FALSE, p.wfam));
    CHECK(gdi_enum_count(p.fam) == 0);
    RemoveFontResourceExW(p.wpath, FR_NOT_ENUM, NULL);
}
TEST(fr_session_font_note) {
    /* a plain AddFontResource makes the font available to every process of the session until removed */
    pf p; int n; IDWriteFactory *f = dw_shared();
    PF_OR_SKIP(p, "T");
    REQUIRE(f);
    n = AddFontResourceExW(p.wpath, 0, NULL);
    if (n == 0) { t_skip("AddFontResourceEx(0) failed (error %lu)", GetLastError()); return; }
    t_note("session font: gdi enum %d, DirectWrite system collection(TRUE) %d, collection(FALSE) %d",
           gdi_enum_count(p.fam), (int)family_in_system(f, TRUE, p.wfam), (int)family_in_system(f, FALSE, p.wfam));
    CHECK(gdi_enum_count(p.fam) >= 1);
    CHECK(RemoveFontResourceExW(p.wpath, 0, NULL));
    t_note("after removal: DirectWrite system collection(TRUE) %d", (int)family_in_system(f, TRUE, p.wfam));
    CHECK(gdi_enum_count(p.fam) == 0);
}
TEST(fr_flags_must_match_on_remove_note) {
    pf p; int n; BOOL r;
    PF_OR_SKIP(p, "U");
    n = AddFontResourceExW(p.wpath, FR_PRIVATE, NULL);
    if (n == 0) { t_skip("GDI rejected the generated font"); return; }
    SetLastError(0);
    r = RemoveFontResourceExW(p.wpath, 0, NULL);
    t_note("RemoveFontResourceEx(flags 0) of a private font -> %d (error %lu)", r, GetLastError());
    if (r) { /* removed anyway, nothing left to clean */ }
    else CHECK(RemoveFontResourceExW(p.wpath, FR_PRIVATE, NULL));
}
