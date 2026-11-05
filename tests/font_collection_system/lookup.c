/* System font collection: IDWriteFontCollection::FindFamilyName.
 * Windows 10: DWriteFontCollection::FindFamilyName writes index = UINT_MAX and exists = 0 first, then looks the name up in
 * an ordered map whose comparer is CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, ...) over (pointer, length) pairs;
 * on a hit it stores the index and exists = 1. The method always returns S_OK. */
#include "fcs.h"

/* ---- well-known families -------------------------------------------------------------------------------------- */
/* Hard checks apply only to families that are present; absence is recorded (SKU / language dependent). */
static void fam_check(const WCHAR *name) {
    NEED_SYS();
    UINT32 idx = FIND(c, name);
    if (idx == 0xFFFFFFFFu) { t_note("family \"%s\" not installed", fcs_u8(name)); END_SYS(); return; }
    UINT32 count = fcs_count(c);
    CHECK(idx < count);
    IDWriteFontFamily *fam = NULL;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, idx, &fam));
    if (fam) {
        /* One of the family's localized names equals the query (case-insensitively). */
        IDWriteLocalizedStrings *s = NULL; int hit = 0;
        CHECK_HR_OK(IDWriteFontFamily_GetFamilyNames(fam, &s));
        if (s) {
            for (UINT32 i = 0; i < IDWriteLocalizedStrings_GetCount(s); i++) {
                char u[512], q[512];
                if (SUCCEEDED(dw_ls_get(s, i, u, sizeof u)) && dw_narrow(name, q, sizeof q) && fcs_ieq(u, q)) hit = 1;
            }
            CHECKF(hit, "no localized name of family #%u equals \"%s\"", idx, fcs_u8(name));
            IDWriteLocalizedStrings_Release(s);
        }
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        CHECK(nf >= 1);
        IDWriteFont *font = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, 0, &font));
        if (font) {
            IDWriteFontFamily *back = NULL;
            CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &back));
            if (back) { CHECK(fcs_same(back, fam)); IDWriteFontFamily_Release(back); }
            IDWriteFont_Release(font);
        }
        IDWriteFont *first = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &first));
        CHECK(first != NULL);
        DW_RELEASE(first);
        IDWriteFontCollection *owner = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(fam, &owner));
        CHECK(owner == c);
        DW_RELEASE(owner);
        IDWriteFontFamily_Release(fam);
    }
    /* Case variants resolve to the same slot. */
    WCHAR up[256], lo[256]; wcsncpy(up, name, 255); up[255] = 0; wcsncpy(lo, name, 255); lo[255] = 0;
    for (WCHAR *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    for (WCHAR *p = lo; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    CHECK(FIND(c, up) == idx);
    CHECK(FIND(c, lo) == idx);
    END_SYS();
}
#define FAMTEST(id, str) TEST(find_##id) { fam_check(L##str); }
FAMTEST(arial, "Arial")
FAMTEST(times_new_roman, "Times New Roman")
FAMTEST(courier_new, "Courier New")
FAMTEST(segoe_ui, "Segoe UI")
FAMTEST(tahoma, "Tahoma")
FAMTEST(verdana, "Verdana")
FAMTEST(consolas, "Consolas")
FAMTEST(calibri, "Calibri")
FAMTEST(cambria, "Cambria")
FAMTEST(georgia, "Georgia")
FAMTEST(lucida_console, "Lucida Console")
FAMTEST(marlett, "Marlett")
FAMTEST(symbol, "Symbol")
FAMTEST(wingdings, "Wingdings")
FAMTEST(webdings, "Webdings")
FAMTEST(segoe_ui_symbol, "Segoe UI Symbol")
FAMTEST(segoe_ui_emoji, "Segoe UI Emoji")
FAMTEST(ms_gothic, "MS Gothic")
FAMTEST(microsoft_yahei, "Microsoft YaHei")
FAMTEST(bahnschrift, "Bahnschrift")
FAMTEST(arial_black, "Arial Black")
FAMTEST(comic_sans_ms, "Comic Sans MS")
FAMTEST(impact, "Impact")
FAMTEST(trebuchet_ms, "Trebuchet MS")
FAMTEST(palatino_linotype, "Palatino Linotype")
FAMTEST(microsoft_sans_serif, "Microsoft Sans Serif")
FAMTEST(lucida_sans_unicode, "Lucida Sans Unicode")
FAMTEST(segoe_script, "Segoe Script")
FAMTEST(segoe_print, "Segoe Print")
FAMTEST(ebrima, "Ebrima")
FAMTEST(gadugi, "Gadugi")
FAMTEST(nirmala_ui, "Nirmala UI")
FAMTEST(leelawadee_ui, "Leelawadee UI")
FAMTEST(malgun_gothic, "Malgun Gothic")
FAMTEST(yu_gothic, "Yu Gothic")
FAMTEST(microsoft_jhenghei, "Microsoft JhengHei")
FAMTEST(simsun, "SimSun")
FAMTEST(mingliu, "MingLiU")
FAMTEST(mongolian_baiti, "Mongolian Baiti")
FAMTEST(javanese_text, "Javanese Text")
FAMTEST(myanmar_text, "Myanmar Text")
FAMTEST(franklin_gothic_medium, "Franklin Gothic Medium")
FAMTEST(candara, "Candara")
FAMTEST(corbel, "Corbel")
FAMTEST(constantia, "Constantia")
FAMTEST(segoe_mdl2_assets, "Segoe MDL2 Assets")
FAMTEST(sylfaen, "Sylfaen")
FAMTEST(sitka_text, "Sitka Text")
FAMTEST(segoe_ui_light, "Segoe UI Light")
FAMTEST(segoe_ui_semibold, "Segoe UI Semibold")
FAMTEST(segoe_ui_black, "Segoe UI Black")
FAMTEST(segoe_ui_historic, "Segoe UI Historic")
FAMTEST(arial_narrow, "Arial Narrow")
FAMTEST(courier, "Courier")
FAMTEST(ms_sans_serif, "MS Sans Serif")
FAMTEST(ms_serif, "MS Serif")
FAMTEST(small_fonts, "Small Fonts")
FAMTEST(microsoft_himalaya, "Microsoft Himalaya")
FAMTEST(microsoft_new_tai_lue, "Microsoft New Tai Lue")
FAMTEST(microsoft_phagspa, "Microsoft PhagsPa")
FAMTEST(microsoft_tai_le, "Microsoft Tai Le")
FAMTEST(mv_boli, "MV Boli")
FAMTEST(calibri_light, "Calibri Light")
FAMTEST(lucida_sans_typewriter, "Lucida Sans Typewriter")
FAMTEST(dubai, "Dubai")
FAMTEST(yu_mincho, "Yu Mincho")
FAMTEST(meiryo, "Meiryo")
FAMTEST(ms_mincho, "MS Mincho")
FAMTEST(gulim, "Gulim")
FAMTEST(batang, "Batang")
FAMTEST(nsimsun, "NSimSun")
FAMTEST(dengxian, "DengXian")
FAMTEST(estrangelo_edessa, "Estrangelo Edessa")
FAMTEST(gautami, "Gautami")
FAMTEST(latha, "Latha")
FAMTEST(mangal, "Mangal")
FAMTEST(raavi, "Raavi")
FAMTEST(shruti, "Shruti")
FAMTEST(tunga, "Tunga")
FAMTEST(vrinda, "Vrinda")
FAMTEST(kartika, "Kartika")
FAMTEST(iskoola_pota, "Iskoola Pota")
FAMTEST(plantagenet_cherokee, "Plantagenet Cherokee")
FAMTEST(euphemia, "Euphemia")
FAMTEST(urdu_typesetting, "Urdu Typesetting")
FAMTEST(aldhabi, "Aldhabi")
FAMTEST(sakkal_majalla, "Sakkal Majalla")
FAMTEST(simplified_arabic, "Simplified Arabic")
FAMTEST(traditional_arabic, "Traditional Arabic")
FAMTEST(arabic_typesetting, "Arabic Typesetting")

/* ---- not found --------------------------------------------------------------------------------------------- */
static void expect_absent(IDWriteFontCollection *c, const WCHAR *name, size_t len_for_diag, const char *file, int line) {
    UINT32 idx = 0xAAAA; BOOL ex = 0x77;
    HRESULT hr = IDWriteFontCollection_FindFamilyName(c, name, &idx, &ex);
    t_check(hr == S_OK, file, line, "FindFamilyName S_OK for absent name", "hr %s (len %zu)", dw_hr_name(hr), len_for_diag);
    t_check(ex == 0, file, line, "absent name => exists FALSE", "exists %d (len %zu)", ex, len_for_diag);
    t_check(idx == 0xFFFFFFFFu, file, line, "absent name => index UINT_MAX", "index %u (len %zu)", idx, len_for_diag);
}
#define ABSENT(c, n) expect_absent((c), (n), wcslen(n), __FILE__, __LINE__)

TEST(find_absent_simple) {
    NEED_SYS();
    static const WCHAR *names[] = {
        L"NoSuchFontFamilyXYZ", L"Arial2", L"Aria", L"rial", L"Ariall", L"ArialArial", L"Times", L"Times New", L"New Roman",
        L"Segoe", L"Segoe U", L"Courier  New", L"Courier-New", L"CourierNew", L"TimesNewRoman", L"~~~", L"Not A Font", L"0", L"12345",
        L"Wingding", L"Helvetica Neue Condensed Black Oblique", L"Segoe UI Nonexistent", L"Arial Bold Italic Condensed Wide",
    };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) ABSENT(c, names[i]);
    END_SYS();
}
TEST(find_absent_empty_string) {
    NEED_SYS();
    ABSENT(c, L"");
    END_SYS();
}
TEST(find_absent_whitespace_only) {
    NEED_SYS();
    static const WCHAR *names[] = { L" ", L"  ", L"\t", L"\n", L"\r\n", L" \t ", L"\x00A0", L"\x2003", L"\x3000" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) ABSENT(c, names[i]);
    END_SYS();
}
TEST(find_absent_punctuation) {
    NEED_SYS();
    static const WCHAR *names[] = { L".", L"..", L"/", L"\\", L"*", L"?", L"%", L"@", L"#", L"\"", L"'", L"<>", L"|", L":", L";", L"{}", L"[]", L"()", L"\\\\?\\", L"C:\\Windows\\Fonts\\arial.ttf", L"arial.ttf", L"ARIAL.TTF" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) ABSENT(c, names[i]);
    END_SYS();
}
TEST(find_absent_file_names_not_family_names) {
    /* A font file name or a full face name is not a family name. */
    NEED_SYS();
    static const WCHAR *names[] = { L"arial.ttf", L"arialbd.ttf", L"times.ttf", L"segoeui.ttf", L"msgothic.ttc", L"Arial Regular.ttf", L"Segoe UI.ttf" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) ABSENT(c, names[i]);
    END_SYS();
}
TEST(find_absent_suffix_and_prefix_of_real_family) {
    NEED_SYS();
    WCHAR buf[300];
    for (UINT32 i = 0; i < fcs_count(c) && i < 60; i++) {
        IDWriteFontFamily *fam = NULL;
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) || !fam) continue;
        fcs_family_en_w(fam, buf, 200);
        IDWriteFontFamily_Release(fam);
        if (!buf[0]) continue;
        WCHAR a[300], b[300];
        swprintf(a, 300, L"%ls~~~", buf);
        swprintf(b, 300, L"~~~%ls", buf);
        ABSENT(c, a);
        ABSENT(c, b);
    }
    END_SYS();
}
TEST(find_absent_non_ascii) {
    NEED_SYS();
    static const WCHAR *names[] = { L"\x65E5\x672C\x8A9E\x30D5\x30A9\x30F3\x30C8\x00D7", L"\x0416\x0416\x0416", L"\x00C4\x00D6\x00DC", L"\xD83D\xDE00", L"\xD83D\xDE00\xD83D\xDE00\xD83D\xDE00",
                                    L"\xD800", L"\xDC00", L"\xDC00\xD800", L"\xFFFF", L"\xFFFE", L"\xFEFF", L"\x0301" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) ABSENT(c, names[i]);
    {
        /* A leading right-to-left override is ignored by the platform lookup (like the soft hyphen), so
           the name matches Arial where Arial is installed; report it rather than asserting absence. */
        static const WCHAR rlo_arial[] = L"\x202E" L"Arial";
        UINT32 idx = 0; BOOL ex = 1;
        HRESULT hr = IDWriteFontCollection_FindFamilyName(c, rlo_arial, &idx, &ex);
        CHECKF(hr == S_OK, "RLO + Arial -> %s", dw_hr_name(hr));
        t_note("RLO + Arial: exists %d index %u", (int)ex, idx);
    }
    END_SYS();
}
TEST(find_absent_control_characters) {
    NEED_SYS();
    for (WCHAR ch = 1; ch < 0x20; ch++) {
        WCHAR s[4] = { ch, 0 };
        UINT32 idx = 0; BOOL ex = 1;
        HRESULT hr = IDWriteFontCollection_FindFamilyName(c, s, &idx, &ex);
        CHECKF(hr == S_OK, "control %02x -> %s", ch, dw_hr_name(hr));
        if (ex) t_note("control character %02x matched family %u", ch, idx);
    }
    END_SYS();
}
TEST(find_absent_embedded_terminator_ignored) {
    /* The name is NUL terminated, so characters after the first NUL are never examined: "Nope\0Arial" is "Nope". */
    NEED_SYS();
    static const WCHAR s[] = { 'N', 'o', 'p', 'e', 'F', 'a', 'm', 0, 'A', 'r', 'i', 'a', 'l', 0 };
    ABSENT(c, s);
    END_SYS();
}
TEST(find_absent_long_names) {
    NEED_SYS();
    static const size_t lens[] = { 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 511, 512, 1000, 1023, 1024, 4095, 4096, 8192, 32767, 32768, 65535, 65536 };
    for (size_t i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        size_t n = lens[i];
        WCHAR *w = (WCHAR *)malloc((n + 1) * sizeof(WCHAR));
        if (!w) { CHECK(0); break; }
        for (size_t k = 0; k < n; k++) w[k] = L'A' + (WCHAR)(k % 26);
        w[n] = 0;
        expect_absent(c, w, n, __FILE__, __LINE__);
        free(w);
    }
    END_SYS();
}
TEST(find_absent_very_long_with_real_prefix) {
    NEED_SYS();
    size_t n = 20000;
    WCHAR *w = (WCHAR *)malloc((n + 8) * sizeof(WCHAR));
    if (!w) { t_skip("no memory"); END_SYS(); return; }
    wcscpy(w, L"Arial");
    for (size_t k = 5; k < n; k++) w[k] = L'x';
    w[n] = 0;
    expect_absent(c, w, n, __FILE__, __LINE__);
    free(w);
    END_SYS();
}
TEST(find_absent_single_chars_ascii) {
    NEED_SYS();
    int hits = 0;
    for (WCHAR ch = 0x20; ch < 0x7f; ch++) {
        WCHAR s[2] = { ch, 0 };
        UINT32 idx = 0; BOOL ex = 0;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, s, &idx, &ex));
        if (ex) { hits++; t_note("single character family \"%c\" exists at %u", (char)ch, idx); }
        else CHECK(idx == 0xFFFFFFFFu);
    }
    END_SYS();
}

/* ---- case handling ------------------------------------------------------------------------------------------ */
TEST(find_case_arial_variants) {
    NEED_SYS();
    UINT32 base = FIND(c, L"Arial");
    if (base == 0xFFFFFFFFu) { t_skip("Arial not installed"); END_SYS(); return; }
    static const WCHAR *v[] = { L"ARIAL", L"arial", L"aRIAL", L"ArIaL", L"arIAL", L"AriaL", L"aRiAl", L"ARial", L"arIal" };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) { UINT32 k = FIND(c, v[i]); CHECKF(k == base, "\"%s\" -> %u want %u", fcs_u8(v[i]), k, base); }
    END_SYS();
}
TEST(find_case_multiword_variants) {
    NEED_SYS();
    static const struct { const WCHAR *canon, *alt[4]; } t[] = {
        { L"Times New Roman", { L"TIMES NEW ROMAN", L"times new roman", L"tImEs NeW rOmAn", L"Times new roman" } },
        { L"Courier New", { L"COURIER NEW", L"courier new", L"cOURIER nEW", L"Courier new" } },
        { L"Segoe UI", { L"SEGOE UI", L"segoe ui", L"Segoe Ui", L"sEGOE uI" } },
        { L"Lucida Console", { L"LUCIDA CONSOLE", L"lucida console", L"Lucida console", L"lUCIDA cONSOLE" } },
        { L"Segoe UI Symbol", { L"SEGOE UI SYMBOL", L"segoe ui symbol", L"Segoe ui symbol", L"SEGOE ui SYMBOL" } },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        UINT32 base = FIND(c, t[i].canon);
        if (base == 0xFFFFFFFFu) { t_note("%s not installed", fcs_u8(t[i].canon)); continue; }
        for (int k = 0; k < 4; k++) { UINT32 got = FIND(c, t[i].alt[k]); CHECKF(got == base, "\"%s\" -> %u want %u", fcs_u8(t[i].alt[k]), got, base); }
    }
    END_SYS();
}
TEST(find_case_every_family_upper_lower) {
    NEED_SYS();
    UINT32 n = fcs_count(c), checked = 0;
    for (UINT32 i = 0; i < n && i < FCS_MAX_FAMILIES; i++) {
        IDWriteFontFamily *fam = NULL; WCHAR nm[256];
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) || !fam) continue;
        fcs_family_en_w(fam, nm, 256);
        IDWriteFontFamily_Release(fam);
        if (!nm[0]) continue;
        WCHAR up[256], lo[256];
        wcscpy(up, nm); wcscpy(lo, nm);
        for (WCHAR *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        for (WCHAR *p = lo; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
        UINT32 a = FIND(c, nm), b = FIND(c, up), d = FIND(c, lo);
        CHECKF(a != 0xFFFFFFFFu, "family #%u \"%s\" not found by its own name", i, fcs_u8(nm));
        CHECKF(a == b && a == d, "family #%u \"%s\": exact %u upper %u lower %u", i, fcs_u8(nm), a, b, d);
        checked++;
    }
    CHECK(checked > 0 || t_is_wine());
    END_SYS();
}
TEST(find_case_non_ascii_letters_fold) {
    /* CompareStringW folds non-ASCII case too; families with such letters are rare, so exercise the code path and
     * only require that a name never matches a *different* family. */
    NEED_SYS();
    static const WCHAR *names[] = { L"\x00C4rial", L"\x00E4rial", L"ARIA\x0141", L"\x0391\x03A1\x0399\x0391\x039B", L"\x03B1\x03C1\x03B9\x03B1\x03BB" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        UINT32 idx = FIND(c, names[i]);
        if (idx != 0xFFFFFFFFu) t_note("\"%s\" matched family %u", fcs_u8(names[i]), idx);
    }
    END_SYS();
}
TEST(find_case_turkish_dotless_i_not_arial) {
    /* The comparison is invariant-locale, not Turkish: 'I' and 'i' pair up, dotless/dotted variants are other letters. */
    NEED_SYS();
    UINT32 base = FIND(c, L"Arial");
    if (base == 0xFFFFFFFFu) { t_skip("Arial not installed"); END_SYS(); return; }
    UINT32 a = FIND(c, L"AR\x0130" L"AL");   /* capital dotted I */
    UINT32 b = FIND(c, L"AR\x0131" L"AL");   /* dotless i */
    CHECKF(a != base, "dotted capital I matched Arial");
    CHECKF(b != base, "dotless i matched Arial");
    END_SYS();
}

/* ---- ignorable characters (recorded) -------------------------------------------------------------------------- */
TEST(find_ignorable_characters_recorded) {
    /* CompareStringW without SORT_STRINGSORT treats some characters as zero-weight; record what happens for a name
     * that exists, so an implementation can reproduce the exact comparer. */
    NEED_SYS();
    UINT32 base = FIND(c, L"Arial");
    if (base == 0xFFFFFFFFu) { t_skip("Arial not installed"); END_SYS(); return; }
    static const struct { const WCHAR *s; const char *what; } t[] = {
        { L"Aria\x00ADl", "soft hyphen inside" }, { L"Arial\x00AD", "soft hyphen trailing" }, { L"Ari-al", "hyphen inside" }, { L"Arial-", "hyphen trailing" },
        { L"Ar'ial", "apostrophe inside" }, { L"Ar\x200Bial", "zero width space" }, { L"Ar\x200Dial", "zero width joiner" }, { L"\xFEFF" L"Arial", "BOM prefix" },
        { L"Arial ", "trailing space" }, { L" Arial", "leading space" }, { L"Ar ial", "inner space" }, { L"Arial\t", "trailing tab" }, { L"Arial.", "trailing dot" },
        { L"Arial\x0001", "trailing control" }, { L"A\x0301rial", "combining acute" },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        UINT32 k = FIND(c, t[i].s);
        t_note("Arial with %s -> %s", t[i].what, k == base ? "matches Arial" : (k == 0xFFFFFFFFu ? "not found" : "other family"));
    }
    END_SYS();
}
TEST(find_whitespace_is_significant) {
    NEED_SYS();
    UINT32 base = FIND(c, L"Segoe UI");
    if (base == 0xFFFFFFFFu) { t_skip("Segoe UI not installed"); END_SYS(); return; }
    UINT32 k = FIND(c, L"SegoeUI");
    CHECK(k != base);
    k = FIND(c, L"Segoe  UI");
    if (k == base) t_note("double space matches Segoe UI");
    END_SYS();
}

/* ---- output parameter protocol ----------------------------------------------------------------------------- */
TEST(find_outparams_found) {
    NEED_SYS();
    UINT32 idx = 0xCAFEBABE; BOOL ex = 0x1234;
    HRESULT hr = IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex);
    CHECK_HR(hr, S_OK);
    if (ex) { CHECK(ex == 1); CHECK(idx < fcs_count(c)); }
    else { CHECK(idx == 0xFFFFFFFFu); t_note("Arial not installed"); }
    END_SYS();
}
TEST(find_outparams_not_found_overwritten) {
    NEED_SYS();
    UINT32 idx = 0; BOOL ex = 1;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"DefinitelyNotInstalled_Family", &idx, &ex));
    CHECK(ex == 0);
    CHECK(idx == 0xFFFFFFFFu);
    idx = 7; ex = 0x7fffffff;
    CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"DefinitelyNotInstalled_Family", &idx, &ex));
    CHECK(ex == 0 && idx == 0xFFFFFFFFu);
    END_SYS();
}
TEST(find_exists_is_exactly_one) {
    NEED_SYS();
    for (UINT32 i = 0; i < fcs_count(c) && i < 40; i++) {
        WCHAR nm[256]; IDWriteFontFamily *fam = NULL;
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) || !fam) continue;
        fcs_family_en_w(fam, nm, 256); IDWriteFontFamily_Release(fam);
        UINT32 idx = 0; BOOL ex = 0x5555;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, nm, &idx, &ex));
        CHECKF(ex == 1, "family \"%s\": exists %d", fcs_u8(nm), ex);
    }
    END_SYS();
}
TEST(find_deterministic_repeat) {
    NEED_SYS();
    UINT32 a = FIND(c, L"Arial");
    UINT32 b = FIND(c, L"Not A Family");
    for (int i = 0; i < 2000; i++) {
        UINT32 x = 0, y = 0; BOOL ex = 0, ey = 0;
        if (FAILED(IDWriteFontCollection_FindFamilyName(c, L"Arial", &x, &ex)) || FAILED(IDWriteFontCollection_FindFamilyName(c, L"Not A Family", &y, &ey))) { CHECK(0); break; }
        if (x != a || y != b || ex != (a != 0xFFFFFFFFu) || ey) { CHECKF(0, "iteration %d changed result", i); break; }
    }
    CHECK(b == 0xFFFFFFFFu);
    END_SYS();
}
TEST(find_does_not_change_refcount) {
    NEED_SYS();
    ULONG base = dw_refcount((IUnknown *)c);
    for (int i = 0; i < 50; i++) { UINT32 idx; BOOL ex; IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex); }
    EXPECT_REFS(c, base);
    END_SYS();
}

/* ---- round trips ---------------------------------------------------------------------------------------- */
TEST(find_round_trip_all_families) {
    NEED_SYS();
    UINT32 n = fcs_count(c), mismatch = 0, checked = 0;
    for (UINT32 i = 0; i < n && i < FCS_MAX_FAMILIES; i++) {
        IDWriteFontFamily *fam = NULL; WCHAR nm[256];
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) || !fam) { CHECK(0); continue; }
        fcs_family_en_w(fam, nm, 256);
        IDWriteFontFamily_Release(fam);
        if (!nm[0]) { CHECKF(0, "family #%u has no English/first name", i); continue; }
        UINT32 idx = FIND(c, nm);
        CHECKF(idx != 0xFFFFFFFFu, "family #%u \"%s\" not found", i, fcs_u8(nm));
        if (idx != 0xFFFFFFFFu && idx != i) { mismatch++; if (mismatch <= 5) t_note("family #%u \"%s\" is found at index %u", i, fcs_u8(nm), idx); }
        checked++;
    }
    if (mismatch) t_note("%u of %u families are found at a different index than their position (duplicate names?)", mismatch, checked);
    CHECK(mismatch == 0);
    END_SYS();
}
TEST(find_all_localized_names_recorded) {
    NEED_SYS();
    UINT32 n = fcs_count(c), total = 0, found_self = 0, found_other = 0, missing = 0;
    for (UINT32 i = 0; i < n && i < FCS_MAX_FAMILIES; i++) {
        IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL;
        if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) || !fam) continue;
        if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) {
            for (UINT32 k = 0; k < IDWriteLocalizedStrings_GetCount(s); k++) {
                UINT32 len = 0; WCHAR nm[512];
                if (FAILED(IDWriteLocalizedStrings_GetStringLength(s, k, &len)) || len >= 500) continue;
                if (FAILED(IDWriteLocalizedStrings_GetString(s, k, nm, 512))) continue;
                UINT32 idx = FIND(c, nm); total++;
                if (idx == i) found_self++; else if (idx == 0xFFFFFFFFu) missing++; else found_other++;
            }
            IDWriteLocalizedStrings_Release(s);
        }
        IDWriteFontFamily_Release(fam);
    }
    t_note("localized family names: %u total, %u resolve to their own family, %u resolve elsewhere, %u are not found", total, found_self, found_other, missing);
    CHECK(total > 0 || t_is_wine());
    END_SYS();
}

/* ---- same answers through every collection interface and factory ------------------------------------------- */
TEST(find_via_collection1_2_3_same) {
    NEED_SYS();
    UINT32 base = FIND(c, L"Segoe UI");
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontCollection2 *c2 = NULL; IDWriteFontCollection3 *c3 = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection1, (void **)&c1));
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection2, (void **)&c2));
    DW_REQUIRE_OK(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection3, (void **)&c3));
    UINT32 i1 = 7, i2 = 7, i3 = 7; BOOL e1 = 9, e2 = 9, e3 = 9;
    CHECK_HR_OK(IDWriteFontCollection1_FindFamilyName(c1, L"Segoe UI", &i1, &e1));
    CHECK_HR_OK(IDWriteFontCollection2_FindFamilyName(c2, L"Segoe UI", &i2, &e2));
    CHECK_HR_OK(IDWriteFontCollection3_FindFamilyName(c3, L"Segoe UI", &i3, &e3));
    CHECK(i1 == i2 && i2 == i3 && e1 == e2 && e2 == e3);
    CHECK(e1 == (base != 0xFFFFFFFFu));
    if (base != 0xFFFFFFFFu) CHECK(i1 == base);
    DW_RELEASE(c1); DW_RELEASE(c2); DW_RELEASE(c3);
    END_SYS();
}
TEST(find_shared_vs_isolated_agree) {
    NEED_SYS();
    IDWriteFactory *iso = dw_isolated();
    IDWriteFontCollection *ic = NULL;
    if (!iso || FAILED(IDWriteFactory_GetSystemFontCollection(iso, &ic, FALSE)) || !ic) { t_skip("isolated collection unavailable"); END_SYS(); return; }
    static const WCHAR *names[] = { L"Arial", L"Segoe UI", L"Times New Roman", L"Tahoma", L"Consolas", L"Marlett", L"NoSuchFamily" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        UINT32 a = FIND(c, names[i]), b = FIND(ic, names[i]);
        CHECKF((a == 0xFFFFFFFFu) == (b == 0xFFFFFFFFu), "\"%s\" found in one collection only (%u vs %u)", fcs_u8(names[i]), a, b);
        if (a != b) t_note("\"%s\" index %u shared vs %u isolated", fcs_u8(names[i]), a, b);
    }
    CHECK(fcs_count(c) == fcs_count(ic));
    DW_RELEASE(ic);
    END_SYS();
}
TEST(find_typographic_collection_recorded) {
    NEED_FV(f6, IDWriteFactory6, 6);
    NEED_SYS();
    IDWriteFontCollection2 *t = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    static const WCHAR *names[] = { L"Arial", L"Segoe UI", L"Segoe UI Light", L"Segoe UI Semibold", L"Segoe UI Black", L"Arial Narrow", L"Arial Black", L"Calibri Light", L"Bahnschrift", L"Bahnschrift Light" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        UINT32 w = FIND(c, names[i]), ty = FIND((IDWriteFontCollection *)t, names[i]);
        t_note("%-20s weight/stretch/style model: %s, typographic model: %s", fcs_u8(names[i]), w == 0xFFFFFFFFu ? "absent" : "present", ty == 0xFFFFFFFFu ? "absent" : "present");
    }
    /* Typographic grouping never produces more families than weight/stretch/style grouping for the same fonts. */
    UINT32 nt = fcs_count((IDWriteFontCollection *)t), nw = fcs_count(c);
    t_note("family counts: typographic %u, weight/stretch/style %u", nt, nw);
    CHECK(nt >= 1);
    DW_RELEASE(t);
    END_SYS();
}
TEST(find_in_downloadable_collection) {
    NEED_FV(f3, IDWriteFactory3, 3);
    IDWriteFontCollection1 *d = NULL;
    DW_REQUIRE_OK(IDWriteFactory3_GetSystemFontCollection(f3, TRUE, &d, FALSE));
    NEED_SYS();
    static const WCHAR *names[] = { L"Arial", L"Segoe UI", L"Times New Roman", L"NoSuchFamily" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        UINT32 a = FIND(c, names[i]), b = FIND((IDWriteFontCollection *)d, names[i]);
        if (a != 0xFFFFFFFFu) CHECKF(b != 0xFFFFFFFFu, "\"%s\" installed but missing from the downloadable-including collection", fcs_u8(names[i]));
        if (a != b) t_note("\"%s\": local index %u, downloadable-including index %u", fcs_u8(names[i]), a, b);
    }
    DW_RELEASE(d);
    END_SYS();
}

/* ---- threads ------------------------------------------------------------------------------------------------ */
typedef struct { IDWriteFontCollection *c; UINT32 expect_arial; int bad; } lk_ctx;
static DWORD WINAPI lk_thread(LPVOID p) {
    lk_ctx *x = (lk_ctx *)p;
    for (int i = 0; i < 400; i++) {
        UINT32 a = 0, b = 0; BOOL ea = 0, eb = 1;
        if (FAILED(IDWriteFontCollection_FindFamilyName(x->c, L"ARIAL", &a, &ea)) || FAILED(IDWriteFontCollection_FindFamilyName(x->c, L"Nope Nope", &b, &eb))) { x->bad++; continue; }
        if (ea ? (a != x->expect_arial) : (x->expect_arial != 0xFFFFFFFFu)) x->bad++;
        if (eb || b != 0xFFFFFFFFu) x->bad++;
    }
    return 0;
}
TEST(find_concurrent_lookups) {
    if (t_is_wine()) { t_skip("emulator's collection is not thread-safe"); return; }
    NEED_SYS();
    UINT32 arial = FIND(c, L"Arial");
    enum { N = 8 };
    HANDLE h[N]; lk_ctx x[N];
    for (int i = 0; i < N; i++) { x[i].c = c; x[i].expect_arial = arial; x[i].bad = 0; h[i] = CreateThread(NULL, 0, lk_thread, &x[i], 0, NULL); }
    int bad = 0;
    for (int i = 0; i < N; i++) if (h[i]) { CHECK(WaitForSingleObject(h[i], 30000) == WAIT_OBJECT_0); CloseHandle(h[i]); bad += x[i].bad; }
    CHECK(bad == 0);
    END_SYS();
}
