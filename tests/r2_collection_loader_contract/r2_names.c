/* What the collection looks like once the loader contract has run: family order, duplicates, localized names taken from
 * the name table, collections as members of a TrueType collection (TTC).
 *
 * Win10 decomp / documented facts used as hard checks:
 *  - The family count is the number of distinct (en-us / WWS) family names; identical names merge, regardless of the order
 *    in which the enumerator delivered the files.
 *  - Every localized family name found in a font's name table (platform 3 records) is reachable through FindFamilyName and
 *    GetFamilyNames; IDWriteLocalizedStrings::FindLocaleName reports a miss as S_OK with exists = FALSE, index = UINT_MAX.
 *  - Font file entries that carry several faces (TTC) yield one font per face, each remembering its face index.
 * Order of families / fonts, and the handling of Macintosh / Unicode-platform-only names are version dependent: t_note only.
 */
#include "r2.h"

typedef struct { unsigned plat, enc, lang; const char *text; } nm_t;
static UINT32 add_named(r2_fx *x, const nm_t *n, int cnt, int weight)
{
    ttf_spec s; int i; size_t sz; uint8_t *b; UINT32 k;
    ttf_default_spec(&s);
    ttf_clear_names(&s);
    s.weight = weight;
    for (i = 0; i < cnt; i++) {
        ttf_add_name(&s, n[i].plat, n[i].enc, n[i].lang, 1, n[i].text);
        ttf_add_name(&s, n[i].plat, n[i].enc, n[i].lang, 2, "Regular");
        ttf_add_name(&s, n[i].plat, n[i].enc, n[i].lang, 4, n[i].text);
    }
    ttf_add_name(&s, n[0].plat, n[0].enc, n[0].lang, 6, "R2Name-Regular");
    b = ttf_build(&s, &sz);
    if (!b) return 0xFFFFFFFFu;
    k = r2_add_blob(x, b, sz); free(b);
    return k;
}
typedef struct { int n; char loc[8][40]; char str[8][160]; } fnames;
static int get_names(IDWriteFontCollection *c, UINT32 fam_index, fnames *out)
{
    IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL; UINT32 i, n; WCHAR w[256];
    memset(out, 0, sizeof *out);
    if (FAILED(IDWriteFontCollection_GetFontFamily(c, fam_index, &fam)) || !fam) return 0;
    if (FAILED(IDWriteFontFamily_GetFamilyNames(fam, &s)) || !s) { DW_RELEASE(fam); return 0; }
    n = IDWriteLocalizedStrings_GetCount(s);
    for (i = 0; i < n && i < 8; i++) {
        UINT32 len = 0;
        if (SUCCEEDED(IDWriteLocalizedStrings_GetLocaleNameLength(s, i, &len)) && len < 200 && SUCCEEDED(IDWriteLocalizedStrings_GetLocaleName(s, i, w, len + 1))) dw_narrow(w, out->loc[i], 40);
        if (SUCCEEDED(IDWriteLocalizedStrings_GetStringLength(s, i, &len)) && len < 200 && SUCCEEDED(IDWriteLocalizedStrings_GetString(s, i, w, len + 1))) dw_narrow(w, out->str[i], 160);
    }
    out->n = (int)n;
    IDWriteLocalizedStrings_Release(s); DW_RELEASE(fam);
    return 1;
}
static int has_pair(const fnames *f, const char *loc, const char *str)
{
    int i;
    for (i = 0; i < f->n && i < 8; i++) if (!_stricmp(f->loc[i], loc) && !strcmp(f->str[i], str)) return 1;
    return 0;
}
static void dump(const fnames *f, const char *what)
{
    int i; for (i = 0; i < f->n && i < 8; i++) t_note("%s name[%d] = \"%s\" (%s)", what, i, f->str[i], f->loc[i]);
}

/* ------------------------------------------------------------------------- name table selection */
RT(names_en_us_only)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x409, "Loc One" } };
    add_named(&x, n, 1, 400);
    IDWriteFontCollection *c = NULL; fnames f;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        CHECK(get_names(c, 0, &f));
        EXPECT_EQ(f.n, 1);
        CHECK(has_pair(&f, "en-us", "Loc One"));
        CHECK(r2_find_family(c, "Loc One") == 0);
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_en_us_and_de_de)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x409, "Loc Both" }, { 3, 1, 0x407, "Gebiet Beide" } };
    add_named(&x, n, 2, 400);
    IDWriteFontCollection *c = NULL; fnames f;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        CHECK(get_names(c, 0, &f));
        dump(&f, "en+de");
        EXPECT_EQ(f.n, 2);
        CHECK(has_pair(&f, "en-us", "Loc Both"));
        CHECK(has_pair(&f, "de-de", "Gebiet Beide"));
        CHECK(r2_find_family(c, "Loc Both") == 0);
        CHECK(r2_find_family(c, "Gebiet Beide") == 0);          /* any locale's name finds the family */
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_de_de_only)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x407, "Nur Deutsch" } };
    add_named(&x, n, 1, 400);
    IDWriteFontCollection *c = NULL; fnames f;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        UINT32 cnt = IDWriteFontCollection_GetFontFamilyCount(c);
        t_note("font with only a de-de family name -> %u families", cnt);
        if (cnt == 1) {
            CHECK(get_names(c, 0, &f));
            dump(&f, "de only");
            CHECK(has_pair(&f, "de-de", "Nur Deutsch"));
            CHECK(r2_find_family(c, "Nur Deutsch") == 0);
        }
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_en_us_and_en_gb)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x409, "Colour Pal" }, { 3, 1, 0x809, "Colour Palette" } };
    add_named(&x, n, 2, 400);
    IDWriteFontCollection *c = NULL; fnames f;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        CHECK(get_names(c, 0, &f));
        EXPECT_EQ(f.n, 2);
        CHECK(has_pair(&f, "en-us", "Colour Pal"));
        CHECK(has_pair(&f, "en-gb", "Colour Palette"));
        CHECK(r2_find_family(c, "Colour Palette") == 0);
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_three_locales)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x409, "Tri Name" }, { 3, 1, 0x40c, "Nom Tri" }, { 3, 1, 0x410, "Nome Tri" } };
    add_named(&x, n, 3, 400);
    IDWriteFontCollection *c = NULL; fnames f;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        CHECK(get_names(c, 0, &f));
        EXPECT_EQ(f.n, 3);
        CHECK(has_pair(&f, "en-us", "Tri Name"));
        CHECK(has_pair(&f, "fr-fr", "Nom Tri"));
        CHECK(has_pair(&f, "it-it", "Nome Tri"));
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_non_latin_locale_name_finds_the_family)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x409, "Jp Fam" }, { 3, 1, 0x411, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" } };
    add_named(&x, n, 2, 400);
    IDWriteFontCollection *c = NULL; UINT32 idx = 77; BOOL ex = FALSE;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"日本語", &idx, &ex));
        CHECK(ex && idx == 0);
        fnames f; CHECK(get_names(c, 0, &f));
        CHECK(has_pair(&f, "ja-jp", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"));
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_find_locale_in_the_family_names)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x409, "Find Loc" }, { 3, 1, 0x407, "Finde Loc" } };
    add_named(&x, n, 2, 400);
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) && SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) {
        UINT32 idx = 5; BOOL ex = TRUE; char buf[96];
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"de-de", &idx, &ex));
        CHECK(ex);
        if (ex) { CHECK_HR_OK(dw_ls_get(s, idx, buf, sizeof buf)); EXPECT_STR(buf, "Finde Loc"); }
        idx = 5; ex = TRUE;
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"fr-fr", &idx, &ex));      /* a miss is still S_OK */
        CHECK(!ex);
        EXPECT_EQ(idx, 0xFFFFFFFFu);
        CHECK_HR_OK(dw_ls_en(s, buf, sizeof buf));
        EXPECT_STR(buf, "Find Loc");
    }
    DW_RELEASE(s); DW_RELEASE(fam); DW_RELEASE(c);
    r2_close(&x);
}
RT(names_macintosh_record_only_note)
{
    FX(x);
    static const nm_t n[] = { { 1, 0, 0, "Mac Only Fam" } };
    add_named(&x, n, 1, 400);
    IDWriteFontCollection *c = NULL; fnames f;
    HRESULT hr = r2_create(&x, &c);
    CHECK_HR(hr, S_OK);
    if (c) {
        UINT32 cnt = IDWriteFontCollection_GetFontFamilyCount(c);
        t_note("font with only a Macintosh name record -> %u families", cnt);
        CHECK(cnt <= 1);
        if (cnt == 1 && get_names(c, 0, &f)) dump(&f, "mac only");
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_unicode_platform_record_only_note)
{
    FX(x);
    static const nm_t n[] = { { 0, 3, 0, "Uni Only Fam" } };
    add_named(&x, n, 1, 400);
    IDWriteFontCollection *c = NULL; fnames f;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        UINT32 cnt = IDWriteFontCollection_GetFontFamilyCount(c);
        t_note("font with only a platform-0 name record -> %u families", cnt);
        CHECK(cnt <= 1);
        if (cnt == 1 && get_names(c, 0, &f)) dump(&f, "platform 0 only");
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_windows_and_macintosh_record_with_the_same_text)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x409, "Both Plat" }, { 1, 0, 0, "Both Plat" } };
    add_named(&x, n, 2, 400);
    IDWriteFontCollection *c = NULL; fnames f;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        CHECK(get_names(c, 0, &f));
        t_note("windows + mac record with equal text -> %d family name(s)", f.n);
        CHECK(f.n >= 1);
        CHECK(has_pair(&f, "en-us", "Both Plat"));
        CHECK(r2_find_family(c, "Both Plat") == 0);
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_second_font_adds_its_locale_to_the_family)
{
    FX(x);
    static const nm_t a[] = { { 3, 1, 0x409, "Grow Fam" } };
    static const nm_t b[] = { { 3, 1, 0x409, "Grow Fam" }, { 3, 1, 0x407, "Wachs Fam" } };
    add_named(&x, a, 1, 400);
    add_named(&x, b, 2, 700);
    IDWriteFontCollection *c = NULL; fnames f;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        CHECK(r2_find_family(c, "Wachs Fam") == 0);
        CHECK(get_names(c, 0, &f));
        dump(&f, "merged");
        CHECK(has_pair(&f, "en-us", "Grow Fam"));
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_en_us_name_decides_the_family)
{
    FX(x);
    static const nm_t a[] = { { 3, 1, 0x409, "Split One" }, { 3, 1, 0x407, "Gleich" } };
    static const nm_t b[] = { { 3, 1, 0x409, "Split Two" }, { 3, 1, 0x407, "Gleich" } };
    add_named(&x, a, 2, 400);
    add_named(&x, b, 2, 400);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
        CHECK(r2_find_family(c, "Split One") >= 0);
        CHECK(r2_find_family(c, "Split Two") >= 0);
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(names_lookup_is_case_insensitive)
{
    FX(x);
    static const nm_t n[] = { { 3, 1, 0x409, "Mixed Case Fam" } };
    add_named(&x, n, 1, 400);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        CHECK(r2_find_family(c, "mixed case fam") == 0);
        CHECK(r2_find_family(c, "MIXED CASE FAM") == 0);
        CHECK(r2_find_family(c, "Mixed Case Fa") < 0);
        CHECK(r2_find_family(c, "Mixed Case Fam ") < 0);
    }
    DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- order and duplicates */
static const char *const perm_names[3] = { "Zulu Fam", "Mike Fam", "Alpha Fam" };
static const int perms[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
static void order_case(int p, char out[3][40])
{
    FX(x);
    int i; IDWriteFontCollection *c = NULL;
    for (i = 0; i < 3; i++) r2_add_family(&x, perm_names[perms[p][i]]);
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 3);
        for (i = 0; i < 3; i++) { out[i][0] = 0; r2_family_name(c, i, out[i], 40); }
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(order_family_set_is_independent_of_enumeration_order)
{
    char first[3][40], cur[3][40]; int p, i, same_order = 1;
    memset(first, 0, sizeof first);
    order_case(0, first);
    for (p = 1; p < 6; p++) {
        memset(cur, 0, sizeof cur);
        order_case(p, cur);
        for (i = 0; i < 3; i++) {
            int found = 0, k; for (k = 0; k < 3; k++) if (!strcmp(cur[i], first[k])) found = 1;
            CHECKF(found, "perm %d: family \"%s\" not in the first run's set", p, cur[i]);
            if (strcmp(cur[i], first[i])) same_order = 0;
        }
    }
    t_note("family order for 3 families: %s, %s, %s; identical for all 6 enumeration orders: %s", first[0], first[1], first[2], same_order ? "yes" : "no");
}
RT(order_is_repeatable_for_one_enumeration)
{
    char a[3][40], b[3][40]; int i;
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    order_case(3, a); order_case(3, b);
    for (i = 0; i < 3; i++) EXPECT_STR(a[i], b[i]);
}
RT(order_ascending_note)
{
    FX(x);
    static const char *const nm[] = { "delta", "Charlie", "alpha", "Echo", "bravo", "Foxtrot" };
    int i, sorted = 1; IDWriteFontCollection *c = NULL; char prev[40] = "", cur[40];
    for (i = 0; i < 6; i++) r2_add_family(&x, nm[i]);
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 6);
        for (i = 0; i < 6; i++) { r2_family_name(c, i, cur, 40); if (i && _stricmp(prev, cur) > 0) sorted = 0; strcpy(prev, cur); t_note("family[%d] = %s", i, cur); }
        t_note("families in case-insensitive ascending order: %s", sorted ? "yes" : "no");
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(duplicate_same_file_five_times_is_one_family)
{
    FX(x);
    UINT32 k = r2_add_family(&x, "Quint");
    int i; for (i = 0; i < 4; i++) r2_cl_step_file(x.cl, k);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        IDWriteFontFamily *fam = NULL;
        if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) && fam) {
            UINT32 fc = IDWriteFontFamily_GetFontCount(fam);
            t_note("one file listed 5 times -> %u fonts in the family", fc);
            CHECK(fc >= 1);
            DW_RELEASE(fam);
        }
    }
    EXPECT_EQ(x.cl->last_en->n_get, 5);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(same_family_different_styles_are_fonts_of_one_family)
{
    FX(x);
    r2_add_style(&x, "Styled", 400, 5, 0); r2_add_style(&x, "Styled", 700, 5, 0); r2_add_style(&x, "Styled", 400, 5, 1); r2_add_style(&x, "Styled", 700, 5, 1);
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) && fam) {
            UINT32 nfonts = IDWriteFontFamily_GetFontCount(fam);
            CHECK(nfonts >= 4);
            UINT32 i; unsigned seen = 0;
            for (i = 0; i < nfonts; i++) {
                IDWriteFont *f = NULL;
                if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, i, &f)) && f) {
                    unsigned code = (IDWriteFont_GetWeight(f) == 700 ? 1u : 0u) | (IDWriteFont_GetStyle(f) == DWRITE_FONT_STYLE_ITALIC ? 2u : 0u);
                    seen |= 1u << code; t_note("font[%u] weight %d style %d", i, IDWriteFont_GetWeight(f), IDWriteFont_GetStyle(f)); DW_RELEASE(f);
                }
            }
            EXPECT_EQ(seen, 0xF);
            DW_RELEASE(fam);
        }
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(order_of_fonts_within_a_family_does_not_depend_on_enumeration_order_note)
{
    int rev; char desc[2][128];
    for (rev = 0; rev < 2; rev++) {
        FX(x);
        static const int w[3] = { 300, 400, 700 };
        int i; IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL;
        for (i = 0; i < 3; i++) r2_add_style(&x, "Ladder", w[rev ? 2 - i : i], 5, 0);
        desc[rev][0] = 0;
        CHECK_HR(r2_create(&x, &c), S_OK);
        if (c && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) && fam) {
            UINT32 n = IDWriteFontFamily_GetFontCount(fam), k;
            for (k = 0; k < n; k++) { IDWriteFont *f = NULL; if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &f)) && f) { char t[16]; snprintf(t, sizeof t, "%d ", IDWriteFont_GetWeight(f)); strcat(desc[rev], t); DW_RELEASE(f); } }
            DW_RELEASE(fam);
        }
        DW_RELEASE(c);
        r2_close(&x);
    }
    t_note("font weights in family order, forward enumeration: %s | reverse enumeration: %s", desc[0], desc[1]);
}

/* ------------------------------------------------------------------------- TTC members */
static uint8_t *make_ttc(const char *const *fam, const int *weight, const int *italic, int n, size_t *tn)
{
    uint8_t *blobs[8]; size_t sizes[8]; const uint8_t *cb[8]; int i; uint8_t *ttc;
    for (i = 0; i < n; i++) { blobs[i] = ttf_build_styled(fam[i], weight[i], 5, italic[i], &sizes[i]); cb[i] = blobs[i]; }
    ttc = ttf_build_ttc(cb, sizes, n, tn);
    for (i = 0; i < n; i++) free(blobs[i]);
    return ttc;
}
static int face_index_of_first_font(IDWriteFontCollection *c, UINT32 fam_i, UINT32 font_i)
{
    IDWriteFontFamily *fam = NULL; IDWriteFont *f = NULL; IDWriteFontFace *face = NULL; int idx = -1;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, fam_i, &fam)) && fam && SUCCEEDED(IDWriteFontFamily_GetFont(fam, font_i, &f)) && f && SUCCEEDED(IDWriteFont_CreateFontFace(f, &face)) && face)
        idx = (int)IDWriteFontFace_GetIndex(face);
    DW_RELEASE(face); DW_RELEASE(f); DW_RELEASE(fam);
    return idx;
}
RT(ttc_three_families_give_three_face_indices)
{
    FX(x);
    static const char *const fam[3] = { "Ttc Alpha", "Ttc Beta", "Ttc Gamma" }; static const int w[3] = { 400, 400, 400 }, it[3] = { 0, 0, 0 };
    size_t tn; uint8_t *ttc = make_ttc(fam, w, it, 3, &tn);
    r2_add_blob(&x, ttc, tn); free(ttc);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        unsigned seen = 0; int i;
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 3);
        for (i = 0; i < 3; i++) {
            int idx = face_index_of_first_font(c, i, 0);
            CHECKF(idx >= 0 && idx < 3, "family %d face index %d", i, idx);
            if (idx >= 0 && idx < 3) seen |= 1u << idx;
        }
        EXPECT_EQ(seen, 7);
        for (i = 0; i < 3; i++) CHECK(r2_find_family(c, fam[i]) >= 0);
        /* the member with face index i carries the name of member i */
        for (i = 0; i < 3; i++) { int fi = r2_find_family(c, fam[i]); if (fi >= 0) EXPECT_EQ(face_index_of_first_font(c, fi, 0), i); }
    }
    EXPECT_EQ(r2_fl_reads_beyond(x.fl), 0);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(ttc_four_styles_of_one_family)
{
    FX(x);
    static const char *const fam[4] = { "TtcRibbi", "TtcRibbi", "TtcRibbi", "TtcRibbi" }; static const int w[4] = { 400, 700, 400, 700 }, it[4] = { 0, 0, 1, 1 };
    size_t tn; uint8_t *ttc = make_ttc(fam, w, it, 4, &tn);
    r2_add_blob(&x, ttc, tn); free(ttc);
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *family = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, 0, &family)) && family) {
        UINT32 i;
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        UINT32 nf = IDWriteFontFamily_GetFontCount(family);
        CHECK(nf >= 4);
        for (i = 0; i < nf; i++) {
            IDWriteFont *f = NULL; IDWriteFontFace *face = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFont(family, i, &f)) && f && SUCCEEDED(IDWriteFont_CreateFontFace(f, &face)) && face) {
                UINT32 idx = IDWriteFontFace_GetIndex(face);
                CHECKF(idx < 4, "face index %u", idx);
                if (idx < 4 && IDWriteFont_GetSimulations(f) == DWRITE_FONT_SIMULATIONS_NONE) {
                    CHECKF((int)IDWriteFont_GetWeight(f) == w[idx], "face %u weight %d want %d", idx, IDWriteFont_GetWeight(f), w[idx]);
                    CHECKF((IDWriteFont_GetStyle(f) == DWRITE_FONT_STYLE_ITALIC) == (it[idx] != 0), "face %u style %d", idx, IDWriteFont_GetStyle(f));
                }
            }
            DW_RELEASE(face); DW_RELEASE(f);
        }
    }
    DW_RELEASE(family); DW_RELEASE(c);
    r2_close(&x);
}
RT(ttc_and_standalone_file_of_the_same_family)
{
    FX(x);
    static const char *const fam[2] = { "TtcMix", "TtcMix" }; static const int w[2] = { 400, 700 }, it[2] = { 0, 0 };
    size_t tn; uint8_t *ttc = make_ttc(fam, w, it, 2, &tn);
    r2_add_blob(&x, ttc, tn); free(ttc);
    r2_add_style(&x, "TtcMix", 400, 5, 1);
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *family = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, 0, &family)) && family) { UINT32 nf = IDWriteFontFamily_GetFontCount(family); t_note("TTC(regular,bold) + italic file -> %u fonts", nf); CHECK(nf >= 3); }
    }
    EXPECT_EQ(x.cl->last_en->n_get, 2);
    DW_RELEASE(family); DW_RELEASE(c);
    r2_close(&x);
}
RT(ttc_listed_twice_note)
{
    FX(x);
    static const char *const fam[2] = { "TtcTwice A", "TtcTwice B" }; static const int w[2] = { 400, 400 }, it[2] = { 0, 0 };
    size_t tn; uint8_t *ttc = make_ttc(fam, w, it, 2, &tn);
    UINT32 k = r2_add_blob(&x, ttc, tn); free(ttc);
    r2_cl_step_file(x.cl, k);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        UINT32 i, n = IDWriteFontCollection_GetFontFamilyCount(c);
        EXPECT_EQ(n, 2);
        for (i = 0; i < n; i++) { IDWriteFontFamily *fm = NULL; if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, i, &fm)) && fm) { t_note("family %u has %u fonts for a TTC listed twice", i, IDWriteFontFamily_GetFontCount(fm)); DW_RELEASE(fm); } }
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(ttc_stream_traffic_note)
{
    FX(x);
    static const char *const fam[3] = { "TtcIo A", "TtcIo B", "TtcIo C" }; static const int w[3] = { 400, 400, 400 }, it[3] = { 0, 0, 0 };
    size_t tn; uint8_t *ttc = make_ttc(fam, w, it, 3, &tn);
    r2_add_blob(&x, ttc, tn); free(ttc);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    t_note("3-face TTC: %ld CreateStreamFromKey, %ld reads, max fragment end %llu of %lu", (long)x.fl->n_create, (long)r2_fl_reads(x.fl), (unsigned long long)x.fl->streams[0]->max_end, (unsigned long)tn);
    CHECK(x.fl->n_create >= 1);
    EXPECT_EQ(r2_fl_bad_ctx(x.fl), 0);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(ttc_member_faces_resolve_back_to_fonts)
{
    FX(x);
    static const char *const fam[3] = { "TtcBack A", "TtcBack B", "TtcBack C" }; static const int w[3] = { 400, 400, 400 }, it[3] = { 0, 0, 0 };
    size_t tn; uint8_t *ttc = make_ttc(fam, w, it, 3, &tn);
    r2_add_blob(&x, ttc, tn); free(ttc);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) {
        UINT32 i;
        for (i = 0; i < 3; i++) {
            IDWriteFontFamily *fm = NULL; IDWriteFont *f = NULL, *back = NULL; IDWriteFontFace *face = NULL;
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, i, &fm)) && fm && SUCCEEDED(IDWriteFontFamily_GetFont(fm, 0, &f)) && f && SUCCEEDED(IDWriteFont_CreateFontFace(f, &face)) && face) {
                HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(c, face, &back);
                CHECKF(hr == S_OK, "family %u: GetFontFromFontFace %s", i, dw_hr_name(hr));
                if (back) { CHECK(IDWriteFont_GetWeight(back) == IDWriteFont_GetWeight(f)); DW_RELEASE(back); }
            }
            DW_RELEASE(face); DW_RELEASE(f); DW_RELEASE(fm);
        }
    }
    DW_RELEASE(c);
    r2_close(&x);
}
