#include "r2fs.h"

/* MapCharacters semantics of a builder-made fallback (DWriteFontFallback / FontFallback::MapCharacters on Windows 10):
 *   1. When a base collection and a non-empty base family name are given, the base family is matched first (its GetMatchingFonts order, the
 *      first font that covers the leading text wins). If it covers anything, the source's GetLocaleName is never called.
 *   2. Otherwise the mappings whose base family / locale filters match are tried in order; for each, the target families are tried in order
 *      until one of them has a font covering the text; a target that lacks the character does not stop the search.
 *   3. mappedLength never exceeds the length of the source's locale run, the mapped font is a font of the target family (matched for the
 *      requested weight/style/stretch) and scale is the mapping's scale (1.0 for the base family and for "no font").
 *   4. When nothing covers the leading character mappedFont is NULL, scale 1.0 and mappedLength > 0. */

RT(walk_table_standard_mappings)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a;
    static const struct { const WCHAR *text; const char *want; } rows[] = {
        { L"abc", "3:R2Main" }, { L"a", "1:R2Main" }, { L"Hello World", "11:R2Main" }, { L"\x3042", "1:R2Cjk" }, { L"\x3042\x3044\x3046", "3:R2Cjk" },
        { L"\x4e00\x4e01\x4e02", "3:R2Cjk" }, { L"\x3042\x4e00", "2:R2Cjk" }, { L"a\x3042" L"b", "1:R2Main|1:R2Cjk|1:R2Main" },
        { L"\x3042" L"a\x3044", "1:R2Cjk|1:R2Main|1:R2Cjk" }, { L"ab\x3042\x3044" L"cd", "2:R2Main|2:R2Cjk|2:R2Main" },
        { L"a\x2603" L"b", "1:R2Main|1:R2Sym|1:R2Main" }, { L"\x2603\x2605", "2:R2Sym" }, { L"\x2600\x2603\x2605", "3:R2Sym" },
        { L"\x3042\x2603", "1:R2Cjk|1:R2Sym" }, { L"\x2603\x3042", "1:R2Sym|1:R2Cjk" }, { L"\x3042\x2603\x4e00", "1:R2Cjk|1:R2Sym|1:R2Cjk" },
        { L"\xD83D\xDE00", "2:R2Emoji" }, { L"\xD83D\xDE00\xD83D\xDE01", "4:R2Emoji" }, { L"a\xD83D\xDE00", "1:R2Main|2:R2Emoji" },
        { L"\xD83D\xDE00" L"a", "2:R2Emoji|1:R2Main" }, { L"a\xD83D\xDE00" L"b", "1:R2Main|2:R2Emoji|1:R2Main" },
        { L"\xD83D\xDE00\x3042", "2:R2Emoji|1:R2Cjk" }, { L"\x3042\xD83D\xDE01", "1:R2Cjk|2:R2Emoji" },
        { L"a\x0905", "1:R2Main|1:-" }, { L"\x0905", "1:-" },
        { L"\xe9", "1:R2Main" }, { L"caf\xe9", "4:R2Main" },
    };
    unsigned i;
    r2_defaults(&a, &X);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) R2_WALK_EQ(fb, rows[i].text, &a, rows[i].want);
    if (fb) {
        /* an unmapped character followed by text some mapping or the base family covers: how far the unmapped run extends is implementation detail */
        char buf[256];
        r2_walk(fb, L"a\x0905" L"b", &a, buf, sizeof buf, NULL); t_note("a U+0905 b: %s", buf);
        r2_walk(fb, L"\x0905" L"ab", &a, buf, sizeof buf, NULL); t_note("U+0905 a b: %s", buf);
        r2_walk(fb, L"\x0905\x0906\x3042", &a, buf, sizeof buf, NULL); t_note("U+0905 U+0906 U+3042: %s", buf);
        r2_walk_invariants(fb, L"a\x0905" L"b\x0906\x0907" L"c", &a, __FILE__, __LINE__);
    }
    DW_RELEASE(fb); R2_END();
}

RT(walk_starts_at_position)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a;
    static const struct { const WCHAR *text; UINT32 pos; const char *want; } rows[] = {
        { L"\x3042" L"ab\x3044", 1, "2:R2Main|1:R2Cjk" }, { L"\x3042\x3044" L"ab", 2, "2:R2Main" }, { L"ab\x3042\x3044", 2, "2:R2Cjk" },
        { L"a\xD83D\xDE00" L"b", 1, "2:R2Emoji|1:R2Main" }, { L"abc", 3, "" }, { L"abc", 2, "1:R2Main" },
    };
    unsigned i;
    r2_defaults(&a, &X);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) {
        char buf[256]; int n;
        a.pos = rows[i].pos; a.len = (UINT32)-1;
        n = r2_walk(fb, rows[i].text, &a, buf, sizeof buf, NULL);
        t_check(n >= 0 && !strcmp(buf, rows[i].want), __FILE__, __LINE__, "walk from position", "row %u: got \"%s\" want \"%s\"", i, buf, rows[i].want);
    }
    DW_RELEASE(fb); R2_END();
}

RT(mapped_length_stops_at_requested_range_end)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r;
    static const struct { const WCHAR *text; UINT32 len; UINT32 want; const char *fam; } rows[] = {
        { L"abcdef", 3, 3, "R2Main" }, { L"abcdef", 1, 1, "R2Main" }, { L"\x3042\x3044\x3046", 2, 2, "R2Cjk" }, { L"\x3042\x3044\x3046", 1, 1, "R2Cjk" },
        { L"\xD83D\xDE00\xD83D\xDE01", 2, 2, "R2Emoji" }, { L"\x2603\x2605\x2600", 2, 2, "R2Sym" },
    };
    unsigned i;
    r2_defaults(&a, &X);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) {
        a.len = rows[i].len;
        CHECK_HR_OK(r2_map(fb, rows[i].text, (UINT32)wcslen(rows[i].text), &a, &r, NULL));
        CHECKF(r.ml == rows[i].want && !strcmp(r.fi.fam, rows[i].fam), "row %u: mappedLength %u family \"%s\" want %u \"%s\"", i, r.ml, r.fi.fam, rows[i].want, rows[i].fam);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(mapped_font_is_the_target_family_and_covers_the_text)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r;
    static const struct { UINT32 cp; const char *fam; UINT16 gid; } rows[] = {
        { 0x3042, "R2Cjk", 67 }, { 0x3044, "R2Cjk", 67 }, { 0x4e00, "R2Cjk", 64 }, { 0x4e01, "R2Cjk", 65 }, { 0x4e02, "R2Cjk", 68 },
        { 0x2603, "R2Sym", 64 }, { 0x2605, "R2Sym", 65 }, { 0x2600, "R2Sym", 68 }, { 0x1f600, "R2Emoji", 64 }, { 0x1f601, "R2Emoji", 65 },
        { 0x1f468, "R2Emoji", 68 }, { 0x301, "R2Mark", 67 }, { 0x41, "R2Main", 2 }, { 0x7a, "R2Main", 0 },
    };
    unsigned i;
    r2_defaults(&a, &X);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) {
        WCHAR t[3]; UINT32 n = 0; UINT16 want = rows[i].gid;
        if (rows[i].cp == 0x7a) want = (UINT16)ttf_default_gid(0x7a);
        if (rows[i].cp > 0xffff) { UINT32 v = rows[i].cp - 0x10000; t[n++] = (WCHAR)(0xd800 + (v >> 10)); t[n++] = (WCHAR)(0xdc00 + (v & 0x3ff)); } else t[n++] = (WCHAR)rows[i].cp;
        t[n] = 0;
        CHECK_HR_OK(r2_map(fb, t, n, &a, &r, NULL));
        if (!r.font) { t_check(0, __FILE__, __LINE__, "font found", "U+%04X: no font", rows[i].cp); continue; }
        CHECKF(!strcmp(r.fi.fam, rows[i].fam), "U+%04X: family \"%s\" want \"%s\"", rows[i].cp, r.fi.fam, rows[i].fam);
        CHECKF(r2_font_has(r.font, rows[i].cp), "U+%04X not covered by the mapped font", rows[i].cp);
        CHECKF(r2_font_gid(r.font, rows[i].cp) == want, "U+%04X: glyph %u want %u", rows[i].cp, r2_font_gid(r.font, rows[i].cp), want);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

/* ---- range semantics ---------------------------------------------------------------------------------------------------------------------- */
typedef struct { const WCHAR *text; const char *want; } wrow;

static void range_case(r2_fx *X, UINT32 lo, UINT32 hi, const wrow *rows, int n, int line)
{
    IDWriteFontFallbackBuilder *b = r2_builder(X->f); IDWriteFontFallback *fb; r2_args a; int i;
    if (!b) return;
    r2_defaults(&a, X);
    t_check(r2_add(b, lo, hi, R2_CJK, X->coll, NULL, NULL, 1.0f) == S_OK, __FILE__, line, "AddMapping", "range [%x,%x]", lo, hi);
    fb = r2_fb(b);
    for (i = 0; fb && i < n; i++) r2_walk_check_(fb, rows[i].text, &a, rows[i].want, __FILE__, line);
    DW_RELEASE(fb); DW_RELEASE(b);
}

RT(range_ends_are_inclusive)
{
    R2_BEGIN();
    static const wrow rows[] = { { L"\x3042", "1:R2Cjk" }, { L"\x3044", "1:R2Cjk" }, { L"\x3046", "1:-" }, { L"\x3042\x3044", "2:R2Cjk" }, { L"\x3042\x3044\x3046", "2:R2Cjk|1:-" } };
    range_case(&X, 0x3042, 0x3044, rows, 5, __LINE__);
    R2_END();
}

RT(range_one_below_and_above)
{
    R2_BEGIN();
    static const wrow rows[] = { { L"\x3042", "1:-" }, { L"\x3044", "1:R2Cjk" }, { L"\x3046", "1:-" }, { L"\x3044\x3046", "1:R2Cjk|1:-" } };
    range_case(&X, 0x3043, 0x3045, rows, 4, __LINE__);
    R2_END();
}

RT(range_single_code_point)
{
    R2_BEGIN();
    static const wrow rows[] = { { L"\x4e00", "1:-" }, { L"\x4e01", "1:R2Cjk" }, { L"\x4e02", "1:-" }, { L"\x4e01\x4e01", "2:R2Cjk" } };
    range_case(&X, 0x4e01, 0x4e01, rows, 4, __LINE__);
    R2_END();
}

RT(range_covering_all_code_points)
{
    R2_BEGIN();
    /* base R2Main still wins for ASCII; the target is consulted for everything else and unmapped when it lacks the character */
    static const wrow rows[] = { { L"abc", "3:R2Main" }, { L"\x3042", "1:R2Cjk" }, { L"\x4e02", "1:R2Cjk" }, { L"\x0905", "1:-" }, { L"a\x3042" L"b", "1:R2Main|1:R2Cjk|1:R2Main" } };
    range_case(&X, 0, 0x10ffff, rows, 5, __LINE__);
    R2_END();
}

RT(range_supplementary_plane)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b; IDWriteFontFallback *fb; r2_args a;
    static const wrow rows[] = { { L"\xD83D\xDE00", "2:R2Emoji" }, { L"\xD83D\xDE01", "2:-" }, { L"a\xD83D\xDE00", "1:R2Main|2:R2Emoji" } };
    b = r2_builder(f); if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x1f600, 0x1f600, R2_EMOJI, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    { int i; for (i = 0; fb && i < 3; i++) r2_walk_check_(fb, rows[i].text, &a, rows[i].want, __FILE__, __LINE__); }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(range_several_in_one_mapping)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    DWRITE_UNICODE_RANGE rg[3] = { { 0x3042, 0x3042 }, { 0x4e00, 0x4e01 }, { 0x2603, 0x2603 } }; const WCHAR *nm[1] = { R2_CJK };
    static const wrow rows[] = { { L"\x3042", "1:R2Cjk" }, { L"\x3044", "1:-" }, { L"\x4e00\x4e01", "2:R2Cjk" }, { L"\x4e02", "1:-" }, { L"\x2603", "1:-" } };
    int i;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMapping(b, rg, 3, nm, 1, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);                                         /* U+2603 lies in a range of this mapping but R2Cjk lacks it: unmapped */
    for (i = 0; fb && i < 5; i++) r2_walk_check_(fb, rows[i].text, &a, rows[i].want, __FILE__, __LINE__);
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(range_unsorted_and_overlapping_within_one_mapping)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    DWRITE_UNICODE_RANGE rg[3] = { { 0x4e01, 0x4e02 }, { 0x3042, 0x3046 }, { 0x3044, 0x4e00 } }; const WCHAR *nm[1] = { R2_CJK };
    static const wrow rows[] = { { L"\x3042\x3044\x3046", "3:R2Cjk" }, { L"\x4e01\x4e02", "2:R2Cjk" } };
    int i; char buf[256];
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMapping(b, rg, 3, nm, 1, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    for (i = 0; fb && i < 2; i++) r2_walk_check_(fb, rows[i].text, &a, rows[i].want, __FILE__, __LINE__);
    if (fb) { r2_walk(fb, L"\x4e00\x4e01\x4e02", &a, buf, sizeof buf, NULL); t_note("U+4E00 U+4E01 U+4E02 (three different ranges of one mapping): %s", buf); }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(range_reversed_first_greater_than_last)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x3046, 0x3042, R2_CJK, X.coll, NULL, NULL, 1.0f));      /* accepted by AddMapping; the range contains nothing */
    fb = r2_fb(b);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3044", 1, &a, &r, NULL));
        t_note("reversed range [3046,3042] and U+3044: %s", r.font ? "mapped" : "unmapped");
        CHECK(r.ml == 1);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

/* ---- targets ------------------------------------------------------------------------------------------------------------------------------- */
static void target_case(r2_fx *X, const WCHAR **names, int nnames, IDWriteFontCollection *coll, const wrow *rows, int n, int line)
{
    IDWriteFontFallbackBuilder *b = r2_builder(X->f); IDWriteFontFallback *fb; r2_args a; DWRITE_UNICODE_RANGE rg = { 0x3040, 0x30ff }; int i;
    if (!b) return;
    r2_defaults(&a, X);
    t_check(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, names, nnames, coll, NULL, NULL, 1.0f) == S_OK, __FILE__, line, "AddMapping", "targets");
    fb = r2_fb(b);
    for (i = 0; fb && i < n; i++) r2_walk_check_(fb, rows[i].text, &a, rows[i].want, __FILE__, line);
    DW_RELEASE(fb); DW_RELEASE(b);
}

RT(targets_first_covering_family_wins)
{
    R2_BEGIN();
    const WCHAR *n1[2] = { R2_CJK, R2_CJK2 }, *n2[2] = { R2_CJK2, R2_CJK }, *n3[2] = { R2_SYM, R2_CJK }, *n4[2] = { R2_SYM, R2_MARK }, *n5[3] = { R2_SYM, R2_EMOJI, R2_CJK2 };
    static const wrow a1[] = { { L"\x3042", "1:R2Cjk" } }, a2[] = { { L"\x3042", "1:R2Cjk2" } }, a3[] = { { L"\x3042\x3044", "2:R2Cjk" } }, a4[] = { { L"\x3042", "1:-" } }, a5[] = { { L"\x3042", "1:R2Cjk2" } };
    target_case(&X, n1, 2, X.coll, a1, 1, __LINE__);
    target_case(&X, n2, 2, X.coll, a2, 1, __LINE__);
    target_case(&X, n3, 2, X.coll, a3, 1, __LINE__);      /* the first target lacks the character: the search goes on */
    target_case(&X, n4, 2, X.coll, a4, 1, __LINE__);      /* no target covers it */
    target_case(&X, n5, 3, X.coll, a5, 1, __LINE__);
    R2_END();
}

RT(targets_missing_families_are_skipped)
{
    R2_BEGIN();
    const WCHAR *n1[1] = { L"NoSuchFamily-r2" }, *n2[2] = { L"NoSuchFamily-r2", R2_CJK }, *n3[3] = { L"", L"Another-Missing", R2_CJK2 };
    static const wrow a1[] = { { L"\x3042", "1:-" } }, a2[] = { { L"\x3042", "1:R2Cjk" } }, a3[] = { { L"\x3042", "1:R2Cjk2" } };
    target_case(&X, n1, 1, X.coll, a1, 1, __LINE__);
    target_case(&X, n2, 2, X.coll, a2, 1, __LINE__);
    target_case(&X, n3, 3, X.coll, a3, 1, __LINE__);
    R2_END();
}

RT(targets_family_name_case_insensitive_note)
{
    R2_BEGIN(); const WCHAR *n1[1] = { L"r2cjk" }; IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r; DWRITE_UNICODE_RANGE rg = { 0x3040, 0x30ff };
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, n1, 1, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) { CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); t_note("lower-case target family name resolves: %s", r.font ? "yes" : "no"); DW_RELEASE(r.font); }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

/* ---- priority between mappings ------------------------------------------------------------------------------------------------------------- */
RT(mappings_fall_through_when_earlier_target_lacks_the_character)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_SYM, X.coll, NULL, NULL, 1.0f));      /* R2Sym has no hiragana */
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) { R2_WALK_EQ(fb, L"\x3042", &a, "1:R2Cjk"); R2_WALK_EQ(fb, L"\x3042\x3044", &a, "2:R2Cjk"); }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(mappings_fall_through_in_reverse_order)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_SYM, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) R2_WALK_EQ(fb, L"\x3042", &a, "1:R2Cjk");
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(mappings_with_disjoint_ranges_are_independent)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x4e00, 0x4e01, R2_CJK2, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x2600, 0x2603, R2_SYM, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        R2_WALK_EQ(fb, L"\x4e00\x3042\x2603", &a, "1:R2Cjk2|1:R2Cjk|1:R2Sym");
        R2_WALK_EQ(fb, L"\x4e01\x4e02", &a, "1:R2Cjk2|1:-");
        R2_WALK_EQ(fb, L"\x2605", &a, "1:-");
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(mappings_overlapping_priority_note)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));      /* both cover U+3042 */
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK2, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        t_note("two mappings of one range, both targets cover U+3042: \"%s\" wins (first added: R2Cjk)", r.fi.fam);
        CHECK(!strcmp(r.fi.fam, "R2Cjk") || !strcmp(r.fi.fam, "R2Cjk2"));
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(mappings_narrow_after_wide_priority_note)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x3000, 0x9fff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x3042, 0x3042, R2_CJK2, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        t_note("wide mapping first, single-character mapping second: \"%s\" wins for U+3042", r.fi.fam);
        CHECK(r.font != NULL); DW_RELEASE(r.font);
        CHECK_HR_OK(r2_map(fb, L"\x3044", 1, &a, &r, NULL));
        CHECK(r.font != NULL && !strcmp(r.fi.fam, "R2Cjk"));
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

/* ---- scale --------------------------------------------------------------------------------------------------------------------------------- */
RT(scale_returned_per_mapping)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    static const struct { UINT32 cp; float scale; } rows[] = { { 0x3042, 0.5f }, { 0x2603, 2.0f }, { 0x4e00, 1.25f }, { 0x301, 0.75f }, { 0x1f600, 100.0f } };
    unsigned i;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 0.5f));
    CHECK_HR_OK(r2_add(b, 0x2600, 0x26ff, R2_SYM, X.coll, NULL, NULL, 2.0f));
    CHECK_HR_OK(r2_add(b, 0x4e00, 0x9fff, R2_CJK, X.coll, NULL, NULL, 1.25f));
    CHECK_HR_OK(r2_add(b, 0x0300, 0x036f, R2_MARK, X.coll, NULL, NULL, 0.75f));
    CHECK_HR_OK(r2_add(b, 0x1f600, 0x1f64f, R2_EMOJI, X.coll, NULL, NULL, 100.0f));
    fb = r2_fb(b);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) {
        WCHAR t[3] = { 0 }; UINT32 n = 0;
        if (rows[i].cp > 0xffff) { UINT32 v = rows[i].cp - 0x10000; t[n++] = (WCHAR)(0xd800 + (v >> 10)); t[n++] = (WCHAR)(0xdc00 + (v & 0x3ff)); } else t[n++] = (WCHAR)rows[i].cp;
        CHECK_HR_OK(r2_map(fb, t, n, &a, &r, NULL));
        CHECKF(r.font != NULL && r.scale == rows[i].scale, "U+%04X: scale %g want %g", rows[i].cp, r.scale, rows[i].scale);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(scale_each_run_has_its_own_scale)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r; const WCHAR *text = L"a\x3042\x2603" L"b";
    static const float want[4] = { 1.0f, 0.5f, 2.0f, 1.0f }; UINT32 pos = 0; int k = 0;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 0.5f));
    CHECK_HR_OK(r2_add(b, 0x2600, 0x26ff, R2_SYM, X.coll, NULL, NULL, 2.0f));
    fb = r2_fb(b);
    while (fb && pos < 4 && k < 4) {
        a.pos = pos; a.len = 4 - pos;
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, NULL));
        CHECKF(r.ml == 1 && r.scale == want[k], "run %d: mappedLength %u scale %g want 1 / %g", k, r.ml, r.scale, want[k]);
        pos += r.ml ? r.ml : 1; k++; DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(scale_zero_is_returned_note)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 0.0f));
    fb = r2_fb(b);
    if (fb) { CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); t_note("mapping scale 0: font %s scale %g", r.font ? "found" : "NULL", r.scale); DW_RELEASE(r.font); }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

/* ---- base family filter ---------------------------------------------------------------------------------------------------------------------- */
RT(base_family_filter_exact_match)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    static const struct { const WCHAR *family; int mapped; } rows[] = { { R2_ALT, 1 }, { R2_MAIN, 0 }, { R2_SYM, 0 }, { L"NoSuchBase", 0 } };
    unsigned i;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, R2_ALT, 1.0f));      /* only for text whose base family is R2Alt */
    fb = r2_fb(b);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) {
        r2_defaults(&a, &X); a.family = rows[i].family;
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        CHECKF((r.font != NULL) == rows[i].mapped, "base family \"%ls\": mapped=%d want %d", rows[i].family, r.font != NULL, rows[i].mapped);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(base_family_filter_absent_matches_any_family)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    static const WCHAR *fam[] = { R2_MAIN, R2_ALT, R2_SYM, L"NoSuchBase", L"" };
    unsigned i;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    for (i = 0; fb && i < sizeof fam / sizeof fam[0]; i++) {
        r2_defaults(&a, &X); a.family = fam[i];
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        CHECKF(r.font != NULL && !strcmp(r.fi.fam, "R2Cjk"), "base family \"%ls\": %s", fam[i], r.font ? r.fi.fam : "unmapped");
        DW_RELEASE(r.font);
    }
    r2_defaults(&a, &X); a.family = NULL;
    if (fb) { CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); CHECK(r.font != NULL); DW_RELEASE(r.font); }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(base_family_filter_selects_between_two_mappings)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, R2_MAIN, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK2, X.coll, NULL, R2_ALT, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.family = R2_MAIN; CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); EXPECT_STR(r.fi.fam, "R2Cjk"); DW_RELEASE(r.font);
        r2_defaults(&a, &X); a.family = R2_ALT;  CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); EXPECT_STR(r.fi.fam, "R2Cjk2"); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(base_family_filter_case_and_null_note)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, R2_ALT, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.family = L"r2alt"; CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); t_note("base family compared case-insensitively: %s", r.font ? "yes" : "no"); DW_RELEASE(r.font);
        r2_defaults(&a, &X); a.family = NULL;     CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); t_note("NULL base family against a filtered mapping: %s", r.font ? "mapped" : "unmapped"); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

/* ---- base collection / family lookup ----------------------------------------------------------------------------------------------------------- */
RT(base_family_missing_from_collection_uses_mappings_for_ascii)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x20, 0x7e, R2_ALT, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.family = L"NoSuchBase";
        R2_WALK_EQ(fb, L"abc", &a, "3:R2Alt");
        a.family = R2_MAIN; R2_WALK_EQ(fb, L"abc", &a, "3:R2Main");      /* with the base family present the base wins */
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(null_base_collection_skips_the_base_family)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x20, 0x7e, R2_ALT, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.coll = NULL;                       /* the base family is only looked up in a given collection */
        R2_WALK_EQ(fb, L"abc", &a, "3:R2Alt");
        a.coll = X.coll; a.family = L""; R2_WALK_EQ(fb, L"abc", &a, "3:R2Alt");       /* an empty family name is no base family either */
        a.family = NULL; R2_WALK_EQ(fb, L"abc", &a, "3:R2Alt");
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(base_family_from_other_collection_is_not_found)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x20, 0x7e, R2_ALT, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.coll = X.coll2; a.family = R2_MAIN;       /* coll2 holds only R2Dup */
        R2_WALK_EQ(fb, L"abc", &a, "3:R2Alt");
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(base_family_covering_partially_splits_the_run)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a;
    r2_defaults(&a, &X); a.family = R2_ALT;
    if (fb) {
        R2_WALK_EQ(fb, L"ab\x3042" L"cd", &a, "2:R2Alt|1:R2Cjk|2:R2Alt");
        a.family = R2_CJK; R2_WALK_EQ(fb, L"\x3042\x3044", &a, "2:R2Cjk");                /* the base family covers all of it */
    }
    DW_RELEASE(fb); R2_END();
}

/* ---- locale ------------------------------------------------------------------------------------------------------------------------------- */
RT(locale_filter_table)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    static const struct { const WCHAR *src; int hard; int mapped; const char *what; } rows[] = {
        { L"ja-JP", 1, 1, "exact" }, { L"ja-jp", 0, 1, "lower case" }, { L"ja", 0, 1, "language only" }, { L"zh-CN", 0, 0, "other language" },
        { L"en-us", 0, 0, "english" }, { L"", 0, 0, "empty" },
    };
    unsigned i;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, L"ja-JP", NULL, 1.0f));
    fb = r2_fb(b);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) {
        if (t_is_wine() && !rows[i].mapped) continue;           /* Wine crashes in MapCharacters when no mapping applies */
        r2_defaults(&a, &X); a.locale = rows[i].src;
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        if (rows[i].hard) CHECKF((r.font != NULL) == rows[i].mapped, "source locale \"%ls\": mapped=%d", rows[i].src, r.font != NULL);
        else t_note("mapping tagged ja-JP, source locale \"%ls\" (%s): %s", rows[i].src, rows[i].what, r.font ? "mapped" : "unmapped");
        CHECK(r.ml == 1);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(locale_absent_in_mapping_matches_any_source_locale)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    static const WCHAR *loc[] = { L"en-us", L"ja-JP", L"zh-CN", L"ko-KR", L"de-DE", L"ar-SA", L"x-unknown" };
    unsigned i;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    for (i = 0; fb && i < sizeof loc / sizeof loc[0]; i++) {
        r2_defaults(&a, &X); a.locale = loc[i];
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        CHECKF(r.font != NULL && !strcmp(r.fi.fam, "R2Cjk"), "source locale \"%ls\": %s", loc[i], r.font ? r.fi.fam : "unmapped");
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(locale_selects_between_two_mappings)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, L"ja-JP", NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK2, X.coll, L"zh-CN", NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.locale = L"ja-JP"; CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); EXPECT_STR(r.fi.fam, "R2Cjk"); DW_RELEASE(r.font);
        r2_defaults(&a, &X); a.locale = L"zh-CN"; CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); EXPECT_STR(r.fi.fam, "R2Cjk2"); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(locale_run_length_clamps_mapped_length)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r; dwx_ana_src *s;
    const WCHAR *text = L"\x3042\x3044\x3046\x4e00";
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x9fff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.locale_run = 2;                 /* the source reports a locale for two characters at a time */
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, &s));
        CHECKF(r.ml >= 1 && r.ml <= 2, "mappedLength %u must not exceed the locale run (2)", r.ml);
        CHECK(s->n_locale >= 1); dwx_release(s); DW_RELEASE(r.font);
        a.locale_run = 1;
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, &s));
        EXPECT_EQ(r.ml, 1); dwx_release(s); DW_RELEASE(r.font);
        a.locale_run = 3; a.pos = 1; a.len = 3;
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, &s));
        CHECKF(r.ml >= 1 && r.ml <= 3, "mappedLength %u must not exceed the locale run (3)", r.ml);
        dwx_release(s); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(locale_per_run_selects_per_run_font)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r; dwx_ana_src *s;
    const WCHAR *text = L"\x3042\x3044\x3046\x4e00";
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x9fff, R2_CJK, X.coll, L"ja-JP", NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x3040, 0x9fff, R2_CJK2, X.coll, L"zh-CN", NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        /* The source says "ja-JP" for the first run; the second MapCharacters call starts in a zh-CN run: the locale is queried at the call's position */
        r2_defaults(&a, &X); a.locale = L"ja-JP"; a.locale_run = 2;
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, &s));
        CHECK(r.font != NULL && !strcmp(r.fi.fam, "R2Cjk") && r.ml <= 2); dwx_release(s); DW_RELEASE(r.font);
        a.locale = L"zh-CN"; a.pos = 2;
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, &s));
        CHECK(r.font != NULL && !strcmp(r.fi.fam, "R2Cjk2")); dwx_release(s); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

/* ---- the text source ----------------------------------------------------------------------------------------------------------------------- */
RT(source_locale_not_queried_when_base_family_covers)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r; dwx_ana_src *s = NULL;
    r2_defaults(&a, &X);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, &s));
        EXPECT_EQ(r.ml, 3); EXPECT_EQ(s->n_locale, 0);          /* the base family settled the run before the locale lookup */
        CHECK(s->n_at >= 1);
        dwx_release(s); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(source_locale_queried_at_the_call_position_when_falling_back)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r; dwx_ana_src *s = NULL; int i, found = 0;
    r2_defaults(&a, &X); a.pos = 2; a.len = 2;
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"ab\x3042\x3044", 4, &a, &r, &s));
        EXPECT_EQ(r.ml, 2); CHECK(s->n_locale >= 1);
        for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 4) { found = 1; EXPECT_EQ(s->log[i].pos, 2); break; }   /* GetLocaleName(textPosition = the range start) */
        CHECK(found);
        dwx_release(s); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(source_text_is_read_inside_the_requested_range)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r; dwx_ana_src *s = NULL; int i;
    r2_defaults(&a, &X); a.pos = 2; a.len = 2;
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"ab\x3042\x3044" L"cd", 6, &a, &r, &s));
        for (i = 0; i < s->log_n; i++) if (s->log[i].fn == 1) { CHECKF(s->log[i].pos >= 2 && s->log[i].pos <= 4, "GetTextAtPosition(%u) lies outside [2,4]", s->log[i].pos); }
        t_note("GetTextBeforePosition calls during MapCharacters: %ld", (long)s->n_before);
        dwx_release(s); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(source_chunking_does_not_change_the_result)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a;
    static const WCHAR *texts[] = { L"abcdefgh", L"ab\x3042\x3044" L"cd", L"\x3042\x3044\x3046\x4e00\x4e01", L"a\x2603" L"b\x2605" L"c", L"\xe9" L"a\x0905" L"b" };
    unsigned i, ck;
    if (!fb) { R2_END(); t_skip("unavailable"); return; }
    for (i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        char want[256], got[256];
        r2_defaults(&a, &X);
        r2_walk(fb, texts[i], &a, want, sizeof want, NULL);
        for (ck = 1; ck <= 3; ck++) {
            a.chunk = ck; r2_walk(fb, texts[i], &a, got, sizeof got, NULL);
            CHECKF(!strcmp(got, want), "text %u chunk %u: \"%s\" want \"%s\"", i, ck, got, want);
        }
    }
    DW_RELEASE(fb); R2_END();
}

RT(source_not_retained_after_the_call)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r; dwx_ana_src *s = NULL;
    r2_defaults(&a, &X);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"a\x3042", 2, &a, &r, &s));
        EXPECT_REFS(s, 1);                                      /* dwrite did not keep a reference to the source */
        dwx_release(s); DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

/* ---- weight / style / stretch ---------------------------------------------------------------------------------------------------------------------- */
typedef struct { DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE st; DWRITE_FONT_STRETCH sx; } wsx;

RT(base_family_face_follows_requested_properties)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r; unsigned i;
    static const wsx rows[] = {
        { 300, DWRITE_FONT_STYLE_NORMAL, 5 }, { 400, DWRITE_FONT_STYLE_NORMAL, 5 }, { 700, DWRITE_FONT_STYLE_NORMAL, 5 }, { 900, DWRITE_FONT_STYLE_NORMAL, 5 },
        { 400, DWRITE_FONT_STYLE_ITALIC, 5 }, { 700, DWRITE_FONT_STYLE_ITALIC, 5 }, { 400, DWRITE_FONT_STYLE_NORMAL, 3 }, { 400, DWRITE_FONT_STYLE_NORMAL, 7 },
    };
    IDWriteFontFamily *fam = NULL; UINT32 idx; BOOL ex = FALSE;
    if (!fb) { R2_END(); t_skip("unavailable"); return; }
    if (FAILED(IDWriteFontCollection_FindFamilyName(X.coll, R2_LADDER, &idx, &ex)) || !ex) { DW_RELEASE(fb); R2_END(); t_skip("R2Ladder family missing"); return; }
    IDWriteFontCollection_GetFontFamily(X.coll, idx, &fam);
    if (fam && IDWriteFontFamily_GetFontCount(fam) < 8) { t_note("R2Ladder has %u fonts (8 expected)", IDWriteFontFamily_GetFontCount(fam)); DW_RELEASE(fam); DW_RELEASE(fb); R2_END(); t_skip("ladder not grouped into one family"); return; }
    DW_RELEASE(fam);
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        r2_defaults(&a, &X); a.family = R2_LADDER; a.w = rows[i].w; a.st = rows[i].st; a.sx = rows[i].sx;
        CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, NULL));
        CHECK(r.font != NULL); EXPECT_EQ(r.ml, 3);
        CHECKF(r.fi.w == rows[i].w && r.fi.st == rows[i].st && r.fi.sx == rows[i].sx, "request %d/%d/%d: got %d/%d/%d (%s)", rows[i].w, rows[i].st, rows[i].sx, r.fi.w, r.fi.st, r.fi.sx, r.fi.fam);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(fallback_target_face_follows_requested_properties)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r; unsigned i;
    static const wsx rows[] = {
        { 300, DWRITE_FONT_STYLE_NORMAL, 5 }, { 400, DWRITE_FONT_STYLE_NORMAL, 5 }, { 700, DWRITE_FONT_STYLE_NORMAL, 5 }, { 900, DWRITE_FONT_STYLE_NORMAL, 5 },
        { 400, DWRITE_FONT_STYLE_ITALIC, 5 }, { 700, DWRITE_FONT_STYLE_ITALIC, 5 }, { 400, DWRITE_FONT_STYLE_NORMAL, 3 }, { 400, DWRITE_FONT_STYLE_NORMAL, 7 },
    };
    IDWriteFontFamily *fam = NULL; UINT32 idx; BOOL ex = FALSE;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    if (FAILED(IDWriteFontCollection_FindFamilyName(X.coll, R2_LADDER, &idx, &ex)) || !ex) { DW_RELEASE(b); R2_END(); t_skip("R2Ladder family missing"); return; }
    IDWriteFontCollection_GetFontFamily(X.coll, idx, &fam);
    if (fam && IDWriteFontFamily_GetFontCount(fam) < 8) { DW_RELEASE(fam); DW_RELEASE(b); R2_END(); t_skip("ladder not grouped into one family"); return; }
    DW_RELEASE(fam);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_LADDER, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) {
        r2_defaults(&a, &X); a.w = rows[i].w; a.st = rows[i].st; a.sx = rows[i].sx;
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        CHECK(r.font != NULL);
        CHECKF(r.fi.w == rows[i].w && r.fi.st == rows[i].st && r.fi.sx == rows[i].sx, "request %d/%d/%d: got %d/%d/%d (%s)", rows[i].w, rows[i].st, rows[i].sx, r.fi.w, r.fi.st, r.fi.sx, r.fi.fam);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(inexact_request_picks_nearest_face_note)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r; unsigned i;
    static const wsx rows[] = {
        { 350, DWRITE_FONT_STYLE_NORMAL, 5 }, { 500, DWRITE_FONT_STYLE_NORMAL, 5 }, { 600, DWRITE_FONT_STYLE_NORMAL, 5 }, { 800, DWRITE_FONT_STYLE_NORMAL, 5 },
        { 100, DWRITE_FONT_STYLE_NORMAL, 5 }, { 400, DWRITE_FONT_STYLE_OBLIQUE, 5 }, { 400, DWRITE_FONT_STYLE_NORMAL, 4 }, { 400, DWRITE_FONT_STYLE_NORMAL, 9 },
    };
    if (!fb) { R2_END(); t_skip("unavailable"); return; }
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        r2_defaults(&a, &X); a.family = R2_LADDER; a.w = rows[i].w; a.st = rows[i].st; a.sx = rows[i].sx;
        CHECK_HR_OK(r2_map(fb, L"abc", 3, &a, &r, NULL));
        CHECKF(r.font != NULL && r.ml == 3, "request %d/%d/%d must still map the text with the ladder family", rows[i].w, rows[i].st, rows[i].sx);
        t_note("request %d/%d/%d -> %d/%d/%d", rows[i].w, rows[i].st, rows[i].sx, r.fi.w, r.fi.st, r.fi.sx);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(coverage_decides_between_faces_of_one_family_note)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    /* R2Part: the regular face lacks U+3042, the bold face has it */
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_PART, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.w = DWRITE_FONT_WEIGHT_NORMAL;
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        if (r.font) { CHECK(r2_font_has(r.font, 0x3042)); t_note("regular requested, only the bold face covers U+3042: weight %d returned", r.fi.w); }
        else t_note("regular requested, only the bold face covers U+3042: unmapped");
        DW_RELEASE(r.font);
        a.w = DWRITE_FONT_WEIGHT_BOLD;
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        CHECK(r.font != NULL); CHECK(r.font == NULL || r.fi.w == DWRITE_FONT_WEIGHT_BOLD);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(requested_properties_do_not_change_the_mapped_length)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; unsigned i;
    static const wsx rows[] = { { 100, DWRITE_FONT_STYLE_NORMAL, 1 }, { 400, DWRITE_FONT_STYLE_ITALIC, 5 }, { 900, DWRITE_FONT_STYLE_OBLIQUE, 9 }, { 999, DWRITE_FONT_STYLE_NORMAL, 5 } };
    if (!fb) { R2_END(); t_skip("unavailable"); return; }
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        r2_defaults(&a, &X); a.w = rows[i].w; a.st = rows[i].st; a.sx = rows[i].sx;
        R2_WALK_EQ(fb, L"ab\x3042\x2603" L"c", &a, "2:R2Main|1:R2Cjk|1:R2Sym|1:R2Main");
    }
    DW_RELEASE(fb); R2_END();
}

/* ---- mapped font object -------------------------------------------------------------------------------------------------------------------- */
RT(mapped_font_is_owned_by_the_caller)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r;
    r2_defaults(&a, &X);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        if (r.font) {
            ULONG r0 = dw_refcount((IUnknown *)r.font);
            CHECK(r0 >= 1); t_note("refcount of a freshly mapped font: %lu", r0);
            IDWriteFont_AddRef(r.font); EXPECT_REFS(r.font, r0 + 1); IDWriteFont_Release(r.font);
            { IDWriteFont *g = NULL; IDWriteFontFace *face = NULL; r2_res r2x;
              CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r2x, NULL)); g = r2x.font; CHECK(g != NULL);
              if (g) { CHECK_HR_OK(IDWriteFont_CreateFontFace(g, &face)); CHECK(face != NULL); DW_RELEASE(face); }
              DW_RELEASE(g); }
            DW_RELEASE(r.font);
        } else CHECK(r.font != NULL);
    }
    DW_RELEASE(fb); R2_END();
}

RT(mapped_font_outlives_fallback_and_builder)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r; IDWriteFontFace *face = NULL; UINT16 g = 0;
    UINT32 cp = 0x3042;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); else memset(&r, 0, sizeof r);
    DW_RELEASE(fb); DW_RELEASE(b);
    if (r.font) {
        CHECK_HR_OK(IDWriteFont_CreateFontFace(r.font, &face));
        if (face) { CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g)); EXPECT_EQ(g, 67); DW_RELEASE(face); }
    }
    DW_RELEASE(r.font); R2_END();
}

RT(fallback_usable_after_builder_released)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b); DW_RELEASE(b); r2_defaults(&a, &X);
    if (fb) { R2_WALK_EQ(fb, L"a\x3042", &a, "1:R2Main|1:R2Cjk"); }
    DW_RELEASE(fb); R2_END();
}

RT(same_fallback_gives_identical_results_repeatedly)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; int i;
    r2_defaults(&a, &X);
    for (i = 0; fb && i < 20; i++) R2_WALK_EQ(fb, L"ab\x3042\xD83D\xDE00\x2603" L"c", &a, "2:R2Main|1:R2Cjk|2:R2Emoji|1:R2Sym|1:R2Main");
    DW_RELEASE(fb); R2_END();
}

RT(fallbacks_from_one_builder_are_independent_objects)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *f1, *f2; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    f1 = r2_fb(b);
    CHECK_HR_OK(r2_add(b, 0x2600, 0x26ff, R2_SYM, X.coll, NULL, NULL, 1.0f));
    f2 = r2_fb(b); r2_defaults(&a, &X);
    CHECK(f1 != f2);
    if (f1 && f2) {
        R2_WALK_EQ(f1, L"\x3042\x2603", &a, "1:R2Cjk|1:-");      /* the first fallback is a snapshot */
        R2_WALK_EQ(f2, L"\x3042\x2603", &a, "1:R2Cjk|1:R2Sym");
    }
    DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(b); R2_END();
}

RT(empty_builder_fallback_maps_nothing)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb && t_is_wine()) { DW_RELEASE(fb); DW_RELEASE(b); R2_END(); t_skip("Wine crashes in MapCharacters when no mapping exists"); return; }
    if (fb) {
        R2_WALK_EQ(fb, L"abc", &a, "3:R2Main");                   /* the base family still works */
        R2_WALK_EQ(fb, L"a\x3042", &a, "1:R2Main|1:-");
        a.family = L"NoSuchBase"; R2_WALK_EQ(fb, L"a", &a, "1:-");
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(adjacent_characters_in_two_ranges_of_one_mapping_note)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; char buf[256];
    DWRITE_UNICODE_RANGE rg[2] = { { 0x3040, 0x30ff }, { 0x4e00, 0x9fff } }; const WCHAR *nm[1] = { R2_CJK };
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    r2_defaults(&a, &X);
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, nm, 1, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        int n = r2_walk(fb, L"\x3042\x4e00", &a, buf, sizeof buf, NULL);
        CHECK(n == 1 || n == 2);
        t_note("U+3042 U+4E00, two ranges of one mapping, one target: %s", buf);
        n = r2_walk(fb, L"\x3042\x3044\x4e00\x4e01", &a, buf, sizeof buf, NULL);
        t_note("U+3042 U+3044 U+4E00 U+4E01: %s", buf);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}
