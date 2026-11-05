#include "r2fs.h"

/* Character sequences: surrogate pairs, combining marks, variation selectors, joiners, controls and ill-formed UTF-16.
 * How a run is cut around characters that no font covers (marks, joiners, selectors) is implementation detail of the fallback tables,
 * so most rows only pin the invariants every caller depends on (the segments tile the text, every mappedLength is positive and within
 * the range, a valid surrogate pair is never split, a returned font contains the first code point of its segment) and record the
 * observed segmentation with t_note. Rows with a fixed segmentation are the ones where every character is either in the base family
 * or in exactly one mapping range. */

typedef struct { const char *what; const WCHAR *text; } srow;

#define S(w, t) { w, t }

static void run_rows(r2_fx *X, IDWriteFontFallback *fb, const srow *rows, int n, const WCHAR *family, int line)
{
    int i; r2_args a;
    for (i = 0; i < n; i++) {
        r2_defaults(&a, X); a.family = family;
        t_note("%s", rows[i].what);
        r2_walk_invariants(fb, rows[i].text, &a, __FILE__, line);
    }
}

RT(seq_combining_marks_invariants)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const srow rows[] = {
        S("a + U+0301 (mark only in R2Mark)", L"a\x0301"), S("a U+0301 b", L"a\x0301" L"b"), S("a U+0301 U+0323 b", L"a\x0301\x0323" L"b"),
        S("U+3042 U+0301", L"\x3042\x0301"), S("U+0301 a (leading mark)", L"\x0301" L"a"), S("U+0301 alone", L"\x0301"), S("U+0301 U+0308", L"\x0301\x0308"),
        S("a U+0301 U+3042", L"a\x0301\x3042"), S("U+3042 U+0301 a", L"\x3042\x0301" L"a"), S("U+2603 U+0308", L"\x2603\x0308"), S("e-acute precomposed + mark", L"\xe9\x0301"),
        S("mark outside every range: U+20D0", L"a\x20d0" L"b"), S("U+0345 (Greek ypogegrammeni)", L"a\x0345"), S("Devanagari virama U+094D", L"\x0915\x094d\x0937"),
    };
    if (fb) run_rows(&X, fb, rows, (int)(sizeof rows / sizeof rows[0]), R2_MAIN, __LINE__);
    DW_RELEASE(fb); R2_END();
}

RT(seq_variation_selectors_invariants)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const srow rows[] = {
        S("snowman + VS16", L"\x2603\xfe0f"), S("snowman + VS15", L"\x2603\xfe0e"), S("heart + VS16", L"\x2764\xfe0f"), S("a + VS16", L"a\xfe0f"),
        S("emoji + VS16", L"\xD83D\xDE00\xfe0f"), S("emoji + VS15", L"\xD83D\xDE00\xfe0e"), S("ideograph + VS1 (U+FE00)", L"\x4e00\xfe00"),
        S("ideograph + IVS (U+E0100)", L"\x4e00\xDB40\xDD00"), S("hiragana + VS1", L"\x3042\xfe00"), S("keycap 1 VS16 U+20E3", L"1\xfe0f\x20e3"),
        S("lone VS16", L"\xfe0f"), S("lone IVS selector", L"\xDB40\xDD00"), S("VS16 VS16", L"\x2603\xfe0f\xfe0f"), S("a + VS16 + b", L"a\xfe0f" L"b"),
    };
    if (fb) run_rows(&X, fb, rows, (int)(sizeof rows / sizeof rows[0]), R2_MAIN, __LINE__);
    DW_RELEASE(fb); R2_END();
}

RT(seq_zero_width_joiners_invariants)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const srow rows[] = {
        S("man ZWJ woman", L"\xD83D\xDC68\x200d\xD83D\xDC69"), S("grin ZWJ grin", L"\xD83D\xDE00\x200d\xD83D\xDE01"), S("a ZWJ b", L"a\x200d" L"b"),
        S("hiragana ZWJ hiragana", L"\x3042\x200d\x3044"), S("hiragana ZWNJ hiragana", L"\x3042\x200c\x3044"), S("ZWJ alone", L"\x200d"), S("ZWNJ alone", L"\x200c"),
        S("grin ZWJ", L"\xD83D\xDE00\x200d"), S("ZWJ grin", L"\x200d\xD83D\xDE00"), S("a ZWNJ b", L"a\x200c" L"b"), S("snowman ZWJ snowman", L"\x2603\x200d\x2603"),
        S("man ZWJ woman ZWJ grin", L"\xD83D\xDC68\x200d\xD83D\xDC69\x200d\xD83D\xDE00"), S("word joiner U+2060", L"a\x2060" L"b"),
    };
    if (fb) run_rows(&X, fb, rows, (int)(sizeof rows / sizeof rows[0]), R2_MAIN, __LINE__);
    DW_RELEASE(fb); R2_END();
}

RT(seq_joiner_inside_a_mapped_range_stays_with_the_run)
{
    R2_BEGIN(); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; char buf[256];
    DWRITE_UNICODE_RANGE rg[2] = { { 0x1f468, 0x1f469 }, { 0x200d, 0x200d } }; const WCHAR *nm[1] = { R2_EMOJI };
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteFontFallbackBuilder_AddMapping(b, rg, 2, nm, 1, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) {
        int n = r2_walk(fb, L"\xD83D\xDC68\x200d\xD83D\xDC69", &a, buf, sizeof buf, NULL);
        CHECK(n >= 1); t_note("man ZWJ woman with ZWJ in the mapping: %s", buf);
        r2_walk_invariants(fb, L"\xD83D\xDC68\x200d\xD83D\xDC69", &a, __FILE__, __LINE__);
        /* each character covered by R2Emoji, each in a range: the segmentation may merge or split but only ever with R2Emoji */
        CHECK(strstr(buf, "R2Main") == NULL);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(seq_surrogate_pair_is_atomic)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a;
    static const struct { const WCHAR *text; const char *want; } rows[] = {
        { L"\xD83D\xDE00", "2:R2Emoji" }, { L"\xD83D\xDE00\xD83D\xDE01", "4:R2Emoji" }, { L"a\xD83D\xDE00" L"b", "1:R2Main|2:R2Emoji|1:R2Main" },
        { L"\xD83D\xDE00\x3042", "2:R2Emoji|1:R2Cjk" }, { L"\x3042\xD83D\xDE00", "1:R2Cjk|2:R2Emoji" },
        { L"\xD83D\xDC68", "2:R2Emoji" }, { L"\xD83D\xDC69\xD83D\xDC68", "4:R2Emoji" },
    };
    unsigned i;
    r2_defaults(&a, &X);
    for (i = 0; fb && i < sizeof rows / sizeof rows[0]; i++) R2_WALK_EQ(fb, rows[i].text, &a, rows[i].want);
    DW_RELEASE(fb); R2_END();
}

RT(seq_range_end_inside_a_surrogate_pair)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r;
    const WCHAR *text = L"a\xD83D\xDE00" L"b";
    r2_defaults(&a, &X);
    if (fb) {
        a.pos = 0; a.len = 2;                                 /* the range ends between the high and the low surrogate */
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, NULL));
        CHECKF(r.ml >= 1 && r.ml <= 2, "mappedLength %u outside [1,2]", r.ml);
        DW_RELEASE(r.font);
        a.pos = 1; a.len = 1;                                 /* only the high surrogate */
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, NULL));
        CHECKF(r.ml >= 1, "mappedLength %u", r.ml);
        t_note("range of one high surrogate followed (outside the range) by its low surrogate: mappedLength %u", r.ml);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(seq_range_starts_at_a_low_surrogate)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r;
    const WCHAR *text = L"a\xD83D\xDE00" L"b";
    r2_defaults(&a, &X);
    if (fb) {
        a.pos = 2; a.len = 2;
        CHECK_HR_OK(r2_map(fb, text, 4, &a, &r, NULL));
        CHECKF(r.ml >= 1 && r.ml <= 2, "mappedLength %u outside [1,2]", r.ml);
        t_note("range starting at the low surrogate: mappedLength %u font %s", r.ml, r.font ? r.fi.fam : "NULL");
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(seq_ill_formed_utf16_invariants)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const srow rows[] = {
        S("lone high surrogate", L"\xD83D"), S("lone low surrogate", L"\xDE00"), S("a + lone high", L"a\xD83D"), S("lone low + a", L"\xDE00" L"a"),
        S("high high low", L"\xD83D\xD83D\xDE00"), S("high low low", L"\xD83D\xDE00\xDE00"), S("low high", L"\xDE00\xD83D"),
        S("a high b", L"a\xD83D" L"b"), S("a low b", L"a\xDE00" L"b"), S("high + hiragana", L"\xD83D\x3042"), S("hiragana + low", L"\x3042\xDE00"),
    };
    if (fb) run_rows(&X, fb, rows, (int)(sizeof rows / sizeof rows[0]), R2_MAIN, __LINE__);
    DW_RELEASE(fb); R2_END();
}

RT(seq_control_and_format_characters_invariants)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const srow rows[] = {
        S("a CR LF b", L"a\r\n" L"b"), S("a LF b", L"a\n" L"b"), S("tab", L"a\tb"), S("NBSP", L"a\xa0" L"b"), S("space between ideographs", L"\x3042 \x3044"),
        S("ideographic space", L"\x3042\x3000\x3044"), S("soft hyphen", L"a\xad" L"b"), S("BOM", L"\xfeff" L"a"), S("object replacement", L"a\xfffc" L"b"),
        S("LRM / RLM", L"a\x200e\x200f" L"b"), S("bidi isolates", L"a\x2066" L"b\x2069"), S("line separator U+2028", L"a\x2028" L"b"),
        S("NUL inside (as data)", L"a\x0001" L"b"), S("U+0085 NEL", L"a\x0085" L"b"), S("replacement U+FFFD", L"a\xfffd" L"b"), S("space only", L" "),
        S("spaces", L"   "), S("PUA U+E000", L"a\xe000" L"b"), S("PUA U+F8FF", L"\xf8ff"), S("noncharacter U+FFFF", L"a\xffff" L"b"),
        S("plane 16 max", L"\xDBFF\xDFFF"), S("plane 2 ideograph", L"\xD840\xDC00"), S("noncharacter U+FDD0", L"\xfdd0"),
    };
    if (fb) run_rows(&X, fb, rows, (int)(sizeof rows / sizeof rows[0]), R2_MAIN, __LINE__);
    DW_RELEASE(fb); R2_END();
}

RT(seq_regional_indicators_and_tags_invariants)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X);
    static const srow rows[] = {
        S("flag JP", L"\xD83C\xDDEF\xD83C\xDDF5"), S("flag GB with tags", L"\xD83C\xDFF4\xDB40\xDC67\xDB40\xDC62\xDB40\xDC65\xDB40\xDC6E\xDB40\xDC67\xDB40\xDC7F"),
        S("a flag b", L"a\xD83C\xDDEF\xD83C\xDDF5" L"b"), S("keycap", L"#\xfe0f\x20e3"), S("skin tone modifier", L"\xD83D\xDC68\xD83C\xDFFB"),
        S("man tone ZWJ woman", L"\xD83D\xDC68\xD83C\xDFFB\x200d\xD83D\xDC69"),
    };
    if (fb) run_rows(&X, fb, rows, (int)(sizeof rows / sizeof rows[0]), R2_MAIN, __LINE__);
    DW_RELEASE(fb); R2_END();
}

RT(seq_long_mixed_text_tiles_exactly)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; WCHAR text[1024]; unsigned n = 0, i; char buf[4096]; int nseg, cov = 1;
    static const WCHAR *unit[] = { L"abc", L"\x3042\x3044", L"\x2603", L"\xD83D\xDE00", L"def", L"\x4e00", L"\x0905", L"gh", L"\x2605\x2600" };
    r2_defaults(&a, &X);
    for (i = 0; i < 90; i++) { const WCHAR *u = unit[i % 9]; size_t l = wcslen(u); memcpy(text + n, u, l * sizeof(WCHAR)); n += (unsigned)l; }
    text[n] = 0;
    if (fb) {
        nseg = r2_walk(fb, text, &a, buf, sizeof buf, &cov);
        CHECK(nseg >= 50); CHECK(cov);                         /* many segments, every returned font covers its first character */
        r2_walk_invariants(fb, text, &a, __FILE__, __LINE__);
    }
    DW_RELEASE(fb); R2_END();
}

RT(seq_every_start_offset_tiles_the_rest)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; const WCHAR *text = L"ab\x3042\xD83D\xDE00\x2603\x0301" L"c\x0905" L"d"; UINT32 n = (UINT32)wcslen(text), p;
    if (!fb) { R2_END(); t_skip("unavailable"); return; }
    for (p = 0; p < n; p++) {
        UINT32 pos = p, total = 0; int guard = 0;
        r2_defaults(&a, &X);
        while (pos < n && guard++ < 64) {
            r2_res r;
            a.pos = pos; a.len = n - pos;
            if (FAILED(r2_map(fb, text, n, &a, &r, NULL))) break;
            if (r.ml == 0 || r.ml > n - pos) { DW_RELEASE(r.font); total = 0xffff; break; }
            pos += r.ml; total += r.ml; DW_RELEASE(r.font);
        }
        CHECKF(total == n - p, "start offset %u: covered %u of %u", p, total, n - p);
    }
    DW_RELEASE(fb); R2_END();
}

RT(seq_decomposed_and_precomposed_accents_note)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; char b1[256], b2[256];
    r2_defaults(&a, &X);
    if (fb) {
        r2_walk(fb, L"\xe9", &a, b1, sizeof b1, NULL);
        r2_walk(fb, L"e\x0301", &a, b2, sizeof b2, NULL);
        EXPECT_STR(b1, "1:R2Main");                            /* the precomposed é is in R2Main */
        t_note("precomposed \\u00E9: %s ; decomposed e U+0301: %s", b1, b2);
    }
    DW_RELEASE(fb); R2_END();
}

RT(seq_walk_with_each_base_family_tiles)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; unsigned i;
    static const WCHAR *fam[] = { R2_MAIN, R2_ALT, R2_CJK, R2_SYM, R2_EMOJI, R2_MARK, R2_LADDER, R2_PART, R2_DUP, L"NoSuchBase" };
    const WCHAR *text = L"ab\x3042\xD83D\xDE00\x2603\x0301" L"c\x0905" L"d";
    if (!fb) { R2_END(); t_skip("unavailable"); return; }
    for (i = 0; i < sizeof fam / sizeof fam[0]; i++) {
        r2_defaults(&a, &X); a.family = fam[i];
        t_note("base family %ls", fam[i]);
        r2_walk_invariants(fb, text, &a, __FILE__, __LINE__);
    }
    DW_RELEASE(fb); R2_END();
}

RT(seq_base_covers_everything_in_one_run)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r;
    r2_defaults(&a, &X);
    if (fb) {
        a.family = R2_CJK;
        CHECK_HR_OK(r2_map(fb, L"\x3042\x3044\x3046\x4e00\x4e01\x4e02", 6, &a, &r, NULL));
        EXPECT_EQ(r.ml, 6); EXPECT_STR(r.fi.fam, "R2Cjk"); CHECK_FLOAT_EQ(r.scale, 1.0f);
        DW_RELEASE(r.font);
        a.family = R2_EMOJI;
        CHECK_HR_OK(r2_map(fb, L"\xD83D\xDE00\xD83D\xDE01\xD83D\xDC68", 6, &a, &r, NULL));
        EXPECT_EQ(r.ml, 6); EXPECT_STR(r.fi.fam, "R2Emoji");
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}
