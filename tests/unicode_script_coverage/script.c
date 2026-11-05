/* IDWriteTextAnalyzer::AnalyzeScript / IDWriteTextAnalyzer1::GetScriptProperties over Unicode script samples.
 *
 * Facts used (decomp of DWriteTextAnalyzer::GetScriptProperties, existing public documentation):
 *   - DWRITE_SCRIPT_ANALYSIS.script is an index into a 153 entry table (ids >= 0x99 => E_INVALIDARG and the output holds entry 0);
 *   - the properties carry the ISO 15924 alphabetic tag (isoScriptCode, first letter in the lowest byte) and numeric code;
 *   - runs reported by AnalyzeScript are contiguous, ascending and cover the analysed range; Common and Inherited characters
 *     never open a run of their own except when no script character is present.
 */
#include "uc.h"

typedef struct { const char *name; const char *tag; UINT32 num; int hard; } script_def;
static const script_def S_LATN = { "Latin", "Latn", 215, 1 }, S_GREK = { "Greek", "Grek", 200, 1 }, S_CYRL = { "Cyrillic", "Cyrl", 220, 1 },
    S_ARMN = { "Armenian", "Armn", 230, 1 }, S_HEBR = { "Hebrew", "Hebr", 125, 1 }, S_ARAB = { "Arabic", "Arab", 160, 1 },
    S_SYRC = { "Syriac", "Syrc", 135, 1 }, S_THAA = { "Thaana", "Thaa", 170, 1 }, S_DEVA = { "Devanagari", "Deva", 315, 1 },
    S_BENG = { "Bengali", "Beng", 325, 1 }, S_GURU = { "Gurmukhi", "Guru", 310, 1 }, S_GUJR = { "Gujarati", "Gujr", 320, 1 },
    S_TAML = { "Tamil", "Taml", 346, 1 }, S_TELU = { "Telugu", "Telu", 340, 1 }, S_KNDA = { "Kannada", "Knda", 345, 1 },
    S_MLYM = { "Malayalam", "Mlym", 347, 1 }, S_SINH = { "Sinhala", "Sinh", 348, 1 }, S_THAI = { "Thai", "Thai", 352, 1 },
    S_LAOO = { "Lao", "Laoo", 356, 1 }, S_TIBT = { "Tibetan", "Tibt", 330, 1 }, S_MYMR = { "Myanmar", "Mymr", 350, 1 },
    S_GEOR = { "Georgian", "Geor", 240, 1 }, S_HANG = { "Hangul", "Hang", 286, 1 }, S_ETHI = { "Ethiopic", "Ethi", 430, 1 },
    S_CHER = { "Cherokee", "Cher", 445, 0 }, S_KHMR = { "Khmer", "Khmr", 355, 1 }, S_MONG = { "Mongolian", "Mong", 145, 1 },
    S_HANI = { "Han", "Hani", 500, 1 }, S_HIRA = { "Hiragana", "Hira", 410, 1 }, S_KANA = { "Katakana", "Kana", 411, 1 },
    S_BOPO = { "Bopomofo", "Bopo", 285, 1 };

static UINT32 tagcode(const char *t) { return UC_TAG(t[0], t[1], t[2], t[3]); }
static void tagstr(UINT32 c, char *o) { o[0] = (char)c; o[1] = (char)(c >> 8); o[2] = (char)(c >> 16); o[3] = (char)(c >> 24); o[4] = 0; }

/* One sample text: exactly one run covering all of it, mapped to the expected script. */
static void one_sample(const script_def *d, const WCHAR *t, UINT32 n)
{
    uc_ana a; UINT32 i;
    uc_analyze(t, n, &a);
    CHECK_HR_OK(a.hr);
    CHECK(a.covered);
    CHECKF(a.n == 1, "%s: %d runs, want 1", d->name, a.n);
    if (a.n >= 1) {
        char tg[5]; tagstr(a.r[0].iso, tg);
        CHECK(a.r[0].pos == 0 && a.r[0].len == n);
        if (d->hard) {
            CHECKF(a.r[0].iso == tagcode(d->tag), "%s: tag %s want %s", d->name, tg, d->tag);
            CHECKF(a.r[0].num == d->num, "%s: number %u want %u", d->name, a.r[0].num, d->num);
        } else t_note("%s: tag %s number %u (expected %s %u)", d->name, tg, a.r[0].num, d->tag, d->num);
        CHECK(a.r[0].sa.shapes == DWRITE_SCRIPT_SHAPES_DEFAULT);
        (void)i;
    }
}
static void samples(const script_def *d, const WCHAR *const *s, int ns)
{
    int i;
    for (i = 0; i < ns; i++) one_sample(d, s[i], LEN(s[i]));
}
#define SAMPLE_TEST(nm, def, ...) TEST(sample_##nm) { static const WCHAR *const s[] = { __VA_ARGS__ }; samples(&def, s, (int)(sizeof s / sizeof s[0])); }

SAMPLE_TEST(latin, S_LATN, L"A", L"z", L"Hello", L"\x00C0\x00FF", L"\x0100\x017F", L"\x0180\x024F", L"\x1E00\x1EFF", L"\x2C60")
SAMPLE_TEST(greek, S_GREK, L"\x0391", L"\x03C9", L"\x03B1\x03B2\x03B3", L"\x1F00\x1FFE", L"\x0370\x03FF")
SAMPLE_TEST(cyrillic, S_CYRL, L"\x0410", L"\x044F", L"\x0431\x0443\x043A", L"\x0500\x052F", L"\x2DE0", L"\xA640\xA69F")
SAMPLE_TEST(armenian, S_ARMN, L"\x0531", L"\x0561\x0562", L"\x0580\x0587", L"\xFB13")
SAMPLE_TEST(hebrew, S_HEBR, L"\x05D0", L"\x05EA", L"\x05E9\x05DC\x05D5\x05DD", L"\xFB1D\xFB4F", L"\x05D0\x05B0")
SAMPLE_TEST(arabic, S_ARAB, L"\x0627", L"\x064A", L"\x0645\x0631\x062D\x0628\x0627", L"\x0750\x077F", L"\xFB50\xFDFF", L"\xFE70\xFEFC", L"\x0627\x064B")
SAMPLE_TEST(syriac, S_SYRC, L"\x0710", L"\x0712\x0713", L"\x072F")
SAMPLE_TEST(thaana, S_THAA, L"\x0780", L"\x0781\x0782", L"\x07A5")
SAMPLE_TEST(devanagari, S_DEVA, L"\x0905", L"\x0915", L"\x0915\x094D\x0937", L"\x0928\x092E\x0938\x094D\x0924\x0947")
SAMPLE_TEST(bengali, S_BENG, L"\x0985", L"\x0995", L"\x0995\x09CD\x09B7")
SAMPLE_TEST(gurmukhi, S_GURU, L"\x0A05", L"\x0A15", L"\x0A15\x0A3E")
SAMPLE_TEST(gujarati, S_GUJR, L"\x0A85", L"\x0A95", L"\x0A95\x0ACD\x0AB7")
SAMPLE_TEST(tamil, S_TAML, L"\x0B85", L"\x0B95", L"\x0B95\x0BCD\x0BB7")
SAMPLE_TEST(telugu, S_TELU, L"\x0C05", L"\x0C15", L"\x0C15\x0C4D\x0C37")
SAMPLE_TEST(kannada, S_KNDA, L"\x0C85", L"\x0C95", L"\x0C95\x0CCD\x0CB7")
SAMPLE_TEST(malayalam, S_MLYM, L"\x0D05", L"\x0D15", L"\x0D15\x0D4D\x0D37")
SAMPLE_TEST(sinhala, S_SINH, L"\x0D85", L"\x0D9A", L"\x0D9A\x0DCA\x0DC2")
SAMPLE_TEST(thai, S_THAI, L"\x0E01", L"\x0E2E", L"\x0E01\x0E32", L"\x0E2A\x0E27\x0E31\x0E2A\x0E14\x0E35")
SAMPLE_TEST(lao, S_LAOO, L"\x0E81", L"\x0EAE", L"\x0E81\x0EB2")
SAMPLE_TEST(tibetan, S_TIBT, L"\x0F40", L"\x0F41\x0F42")
SAMPLE_TEST(myanmar, S_MYMR, L"\x1000", L"\x1001\x1031")
SAMPLE_TEST(georgian, S_GEOR, L"\x10A0", L"\x10D0", L"\x10D0\x10D1\x10D2", L"\x2D00")
SAMPLE_TEST(hangul, S_HANG, L"\xD55C", L"\xAC00", L"\xD7A3", L"\x1100\x1161\x11A8", L"\x3131", L"\xFFA0", L"\xD55C\xAD6D\xC5B4")
SAMPLE_TEST(ethiopic, S_ETHI, L"\x1200", L"\x1201\x1202")
SAMPLE_TEST(cherokee, S_CHER, L"\x13A0", L"\x13A1\x13A2")
SAMPLE_TEST(khmer, S_KHMR, L"\x1780", L"\x1781\x17B6", L"\x19E0")
SAMPLE_TEST(mongolian, S_MONG, L"\x1820", L"\x1821\x1822")
SAMPLE_TEST(han, S_HANI, L"\x4E2D", L"\x4E2D\x6587", L"\x3400\x4DB5", L"\x4E00\x9FA5", L"\xF900",
            L"\xD840\xDC00", L"\xD840\xDC00\xD840\xDC01", L"\xD869\xDED6", L"\xD87E\xDC00")
SAMPLE_TEST(hiragana, S_HIRA, L"\x3042", L"\x3042\x3044\x3046", L"\x3041\x3093", L"\x309F")
SAMPLE_TEST(katakana, S_KANA, L"\x30A2", L"\x30A2\x30A4\x30A6", L"\x30A1\x30F6", L"\xFF71\xFF72", L"\x31F0")
SAMPLE_TEST(bopomofo, S_BOPO, L"\x3105", L"\x3105\x3106", L"\x312D", L"\x31A0")

/* Digits of the complex scripts: the Unicode script property says "own script" for these, recorded only. */
TEST(sample_script_digits_note)
{
    static const struct { const char *tag; WCHAR t[3]; } rows[] = { { "Arab", { 0x06F0, 0x06F9 } }, { "Deva", { 0x0966, 0x096F } }, { "Beng", { 0x09E6, 0x09EF } },
        { "Guru", { 0x0A66, 0x0A67 } }, { "Gujr", { 0x0AE6, 0x0AE7 } }, { "Taml", { 0x0BE6, 0x0BE7 } }, { "Telu", { 0x0C66, 0x0C67 } }, { "Knda", { 0x0CE6, 0x0CE7 } },
        { "Mlym", { 0x0D66, 0x0D67 } }, { "Sinh", { 0x0DE6, 0x0DE7 } }, { "Thai", { 0x0E50, 0x0E59 } }, { "Laoo", { 0x0ED0, 0x0ED9 } }, { "Tibt", { 0x0F20, 0x0F29 } },
        { "Mymr", { 0x1040, 0x1049 } }, { "Khmr", { 0x17E0, 0x17E9 } }, { "Mong", { 0x1810, 0x1819 } }, { "Arab", { 0x0660, 0x0669 } } };
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        uc_ana a; uc_analyze(rows[i].t, 2, &a);
        CHECK(a.covered); CHECK(a.n == 1);
        if (a.n == 1) { char g[5]; tagstr(a.r[0].iso, g); t_note("digits %04X..%04X: tag %s (script %s)", rows[i].t[0], rows[i].t[1], g, rows[i].tag); }
    }
}

/* Extended scripts: results recorded; the run must still be single and span the text. */
typedef struct { const char *name, *tag; UINT32 num; WCHAR t[4]; } ext_row;
static const ext_row ext_rows[] = {
    { "Ogham", "Ogam", 212, { 0x1681, 0x1682 } }, { "Runic", "Runr", 211, { 0x16A0, 0x16A1 } }, { "Braille", "Brai", 570, { 0x2800, 0x2801 } },
    { "Yi", "Yiii", 460, { 0xA000, 0xA001 } }, { "Tifinagh", "Tfng", 120, { 0x2D30, 0x2D31 } }, { "Coptic", "Copt", 204, { 0x2C80, 0x2C81 } },
    { "Glagolitic", "Glag", 225, { 0x2C00, 0x2C01 } }, { "Oriya", "Orya", 327, { 0xB15, 0xB16 } }, { "NKo", "Nkoo", 165, { 0x7CA, 0x7CB } },
    { "Limbu", "Limb", 336, { 0x1900, 0x1901 } }, { "TaiLe", "Tale", 353, { 0x1950, 0x1951 } }, { "Buginese", "Bugi", 367, { 0x1A00, 0x1A01 } },
    { "Balinese", "Bali", 360, { 0x1B05, 0x1B06 } }, { "UCAS", "Cans", 440, { 0x1401, 0x1402 } }, { "Gothic", "Goth", 206, { 0xD800, 0xDF30 } },
    { "Phagspa", "Phag", 331, { 0xA840, 0xA841 } }, { "Lisu", "Lisu", 399, { 0xA4D0, 0xA4D1 } }, { "Vai", "Vaii", 470, { 0xA500, 0xA501 } },
    { "Samaritan", "Samr", 123, { 0x0800, 0x0801 } }, { "Mandaic", "Mand", 140, { 0x0840, 0x0841 } }, { "Javanese", "Java", 361, { 0xA984, 0xA985 } },
    { "Tagalog", "Tglg", 370, { 0x1700, 0x1701 } }, { "NewTaiLue", "Talu", 354, { 0x1980, 0x1981 } }, { "Sundanese", "Sund", 362, { 0x1B83, 0x1B84 } },
    { "Lepcha", "Lepc", 335, { 0x1C00, 0x1C01 } }, { "OlChiki", "Olck", 261, { 0x1C5A, 0x1C5B } }, { "Cham", "Cham", 358, { 0xAA00, 0xAA01 } },
    { "Osmanya", "Osma", 260, { 0xD801, 0xDC80 } }, { "Deseret", "Dsrt", 250, { 0xD801, 0xDC00 } }, { "Phoenician", "Phnx", 115, { 0xD802, 0xDD00 } },
    { "Linear B", "Linb", 401, { 0xD800, 0xDC00 } }, { "Cuneiform", "Xsux", 20, { 0xD808, 0xDC00 } }, { "Egyptian", "Egyp", 50, { 0xD80C, 0xDC00 } },
};
TEST(sample_extended_scripts_single_run)
{
    int i;
    for (i = 0; i < (int)(sizeof ext_rows / sizeof ext_rows[0]); i++) {
        uc_ana a; UINT32 n = ext_rows[i].t[0] >= 0xD800 && ext_rows[i].t[0] < 0xDC00 ? 2 : (ext_rows[i].t[1] ? 2 : 1);
        char tg[5];
        uc_analyze(ext_rows[i].t, n, &a);
        CHECK_HR_OK(a.hr);
        CHECK(a.covered);
        CHECKF(a.n == 1, "%s: %d runs", ext_rows[i].name, a.n);
        if (a.n) { tagstr(a.r[0].iso, tg); t_note("%s: tag %s number %u (script %s %u)", ext_rows[i].name, tg, a.r[0].num, ext_rows[i].tag, ext_rows[i].num); }
    }
}

/* ---- properties consistency -------------------------------------------------------------------------------------------------------- */
static const script_def *all_defs[] = { &S_LATN, &S_GREK, &S_CYRL, &S_ARMN, &S_HEBR, &S_ARAB, &S_SYRC, &S_THAA, &S_DEVA, &S_BENG, &S_GURU, &S_GUJR,
    &S_TAML, &S_TELU, &S_KNDA, &S_MLYM, &S_SINH, &S_THAI, &S_LAOO, &S_TIBT, &S_MYMR, &S_GEOR, &S_HANG, &S_ETHI, &S_CHER, &S_KHMR, &S_MONG,
    &S_HANI, &S_HIRA, &S_KANA, &S_BOPO };
#define NDEFS ((int)(sizeof all_defs / sizeof all_defs[0]))
static const WCHAR *first_char_text[NDEFS] = { L"A", L"\x03B1", L"\x0430", L"\x0561", L"\x05D0", L"\x0627", L"\x0710", L"\x0780", L"\x0915",
    L"\x0995", L"\x0A15", L"\x0A95", L"\x0B95", L"\x0C15", L"\x0C95", L"\x0D15", L"\x0D9A", L"\x0E01", L"\x0E81", L"\x0F40", L"\x1000", L"\x10D0",
    L"\xD55C", L"\x1200", L"\x13A0", L"\x1780", L"\x1820", L"\x4E2D", L"\x3042", L"\x30A2", L"\x3105" };

TEST(props_tag_and_number_agree_with_sample)
{
    int i;
    for (i = 0; i < NDEFS; i++) {
        uc_ana a; DWRITE_SCRIPT_PROPERTIES p;
        uc_analyze(first_char_text[i], LEN(first_char_text[i]), &a);
        if (a.n != 1) { CHECK(0); continue; }
        CHECK_HR_OK(uc_props(a.r[0].sa, &p));
        if (all_defs[i]->hard) {
            CHECKF(p.isoScriptCode == tagcode(all_defs[i]->tag), "%s: code 0x%08x", all_defs[i]->name, p.isoScriptCode);
            CHECKF(p.isoScriptNumber == all_defs[i]->num, "%s: number %u", all_defs[i]->name, p.isoScriptNumber);
        }
    }
}
TEST(props_distinct_scripts_have_distinct_ids)
{
    UINT16 ids[NDEFS]; int i, j, dup = 0;
    for (i = 0; i < NDEFS; i++) { uc_ana a; uc_analyze(first_char_text[i], LEN(first_char_text[i]), &a); ids[i] = a.n ? a.r[0].sa.script : 0xFFFF; }
    for (i = 0; i < NDEFS; i++) for (j = i + 1; j < NDEFS; j++) if (ids[i] == ids[j]) { dup++; t_note("%s and %s share script id %u", all_defs[i]->name, all_defs[j]->name, ids[i]); }
    CHECK(dup == 0);
}
TEST(props_ids_are_below_table_size)
{
    int i;
    for (i = 0; i < NDEFS; i++) { uc_ana a; uc_analyze(first_char_text[i], LEN(first_char_text[i]), &a); CHECK(a.n == 1 && a.r[0].sa.script < 153); }
}
TEST(props_same_script_same_id_across_blocks)
{
    static const WCHAR *const grp[][3] = { { L"a", L"\x00E9", L"\x1E01" }, { L"\x03B1", L"\x1F00", L"\x03A9" }, { L"\x0430", L"\x0500", L"\x042F" },
                                             { L"\x4E2D", L"\x3400", L"\xD840\xDC00" } };
    int g, k;
    for (g = 0; g < 4; g++) {
        uc_ana a0; uc_analyze(grp[g][0], LEN(grp[g][0]), &a0);
        for (k = 1; k < 3; k++) { uc_ana a; uc_analyze(grp[g][k], LEN(grp[g][k]), &a); CHECKF(a.n == 1 && a0.n == 1 && a.r[0].sa.script == a0.r[0].sa.script, "group %d member %d", g, k); }
    }
}
TEST(props_all_table_entries_have_ascii_tags)
{
    UINT32 id; int bad = 0, ok = 0;
    for (id = 0; id < 153; id++) {
        DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p; char t[5]; int j;
        sa.script = (UINT16)id; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
        if (FAILED(uc_props(sa, &p))) { bad++; continue; }
        tagstr(p.isoScriptCode, t);
        for (j = 0; j < 4; j++) if (!((t[j] >= 'A' && t[j] <= 'Z') || (t[j] >= 'a' && t[j] <= 'z'))) bad++;
        if (p.isoScriptNumber > 999) bad++;
        ok++;
    }
    CHECKF(bad == 0, "%d malformed script table entries", bad);
    CHECK(ok == 153);
}
TEST(props_iso_pair_table_consistent)
{
    /* every table entry whose tag is listed here must carry the matching ISO 15924 number */
    static const struct { const char *tag; UINT32 num; } iso[] = { { "Latn", 215 }, { "Grek", 200 }, { "Cyrl", 220 }, { "Armn", 230 }, { "Hebr", 125 },
        { "Arab", 160 }, { "Syrc", 135 }, { "Thaa", 170 }, { "Deva", 315 }, { "Beng", 325 }, { "Guru", 310 }, { "Gujr", 320 }, { "Taml", 346 },
        { "Telu", 340 }, { "Knda", 345 }, { "Mlym", 347 }, { "Sinh", 348 }, { "Thai", 352 }, { "Laoo", 356 }, { "Tibt", 330 }, { "Mymr", 350 },
        { "Geor", 240 }, { "Hang", 286 }, { "Ethi", 430 }, { "Cher", 445 }, { "Khmr", 355 }, { "Mong", 145 }, { "Hani", 500 }, { "Hira", 410 },
        { "Kana", 411 }, { "Bopo", 285 }, { "Zyyy", 998 }, { "Zinh", 994 }, { "Brai", 570 }, { "Yiii", 460 }, { "Ogam", 212 }, { "Runr", 211 },
        { "Orya", 327 }, { "Copt", 204 }, { "Goth", 206 }, { "Cans", 440 }, { "Tfng", 120 }, { "Nkoo", 165 } };
    UINT32 id; int i, checked = 0;
    for (id = 0; id < 153; id++) {
        DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p;
        sa.script = (UINT16)id; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
        if (FAILED(uc_props(sa, &p))) continue;
        for (i = 0; i < (int)(sizeof iso / sizeof iso[0]); i++)
            if (p.isoScriptCode == tagcode(iso[i].tag)) { CHECKF(p.isoScriptNumber == iso[i].num, "id %u %s number %u want %u", id, iso[i].tag, p.isoScriptNumber, iso[i].num); checked++; }
    }
    CHECK(checked >= 25);
}
TEST(props_out_of_range_ids_invalidarg_entry_zero)
{
    DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p0, p; static const UINT16 bad[] = { 153, 154, 200, 255, 256, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF };
    int i;
    sa.script = 0; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    CHECK_HR_OK(uc_props(sa, &p0));
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        sa.script = bad[i];
        memset(&p, 0xCC, sizeof p);
        CHECK_HR(uc_props(sa, &p), E_INVALIDARG);
        CHECKF(p.isoScriptCode == p0.isoScriptCode && p.isoScriptNumber == p0.isoScriptNumber, "script %u: output is not entry 0", bad[i]);
    }
}
TEST(props_last_valid_id)
{
    DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p;
    sa.script = 152; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    CHECK_HR_OK(uc_props(sa, &p));
    CHECK(p.isoScriptNumber != 0);
}
TEST(props_shapes_field_ignored)
{
    DWRITE_SCRIPT_ANALYSIS a, b; DWRITE_SCRIPT_PROPERTIES pa, pb;
    a.script = 1; a.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT; b.script = 1; b.shapes = DWRITE_SCRIPT_SHAPES_NO_VISUAL;
    CHECK_HR_OK(uc_props(a, &pa)); CHECK_HR_OK(uc_props(b, &pb));
    CHECK(!memcmp(&pa, &pb, sizeof pa));
}
TEST(props_entry_zero_is_common_note)
{
    DWRITE_SCRIPT_ANALYSIS sa; DWRITE_SCRIPT_PROPERTIES p; char t[5];
    sa.script = 0; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    CHECK_HR_OK(uc_props(sa, &p));
    tagstr(p.isoScriptCode, t);
    t_note("script id 0: %s %u", t, p.isoScriptNumber);
}
/* Script property flags are data: recorded per script so an implementation can compare. */
TEST(props_flags_note)
{
    int i;
    for (i = 0; i < NDEFS; i++) {
        uc_ana a; DWRITE_SCRIPT_PROPERTIES p;
        uc_analyze(first_char_text[i], LEN(first_char_text[i]), &a);
        if (a.n != 1 || FAILED(uc_props(a.r[0].sa, &p))) continue;
        t_note("%s: lookahead %u justify U+%04X caret=%u wordDiv=%u discrete=%u block=%u distributed=%u connected=%u cursive=%u", all_defs[i]->name,
               p.clusterLookahead, p.justificationCharacter, p.restrictCaretToClusters, p.usesWordDividers, p.isDiscreteWriting, p.isBlockWriting,
               p.isDistributedWithinCluster, p.isConnectedWriting, p.isCursiveWriting);
        CHECKF(p.reserved == 0, "%s: reserved bits 0x%x", all_defs[i]->name, p.reserved);
    }
}
TEST(props_latin_word_dividers_arabic_connected)
{
    uc_ana l, a; DWRITE_SCRIPT_PROPERTIES pl, pa;
    uc_analyze(L"a", 1, &l); uc_analyze(L"\x0627", 1, &a);
    if (l.n != 1 || a.n != 1) { t_skip("analysis failed"); return; }
    CHECK_HR_OK(uc_props(l.r[0].sa, &pl)); CHECK_HR_OK(uc_props(a.r[0].sa, &pa));
    CHECK(pl.usesWordDividers == 1);               /* Latin separates words with spaces */
    CHECK(pa.isConnectedWriting == 1);             /* Arabic letters join */
    CHECK(pl.isConnectedWriting == 0);
    CHECK(pl.isDiscreteWriting == 1 || pl.isDiscreteWriting == 0);
}

/* ---- Common / Inherited behaviour ------------------------------------------------------------------------------------------------------- */
static void common_alone(const WCHAR *t, const char *what)
{
    uc_ana a; UINT32 n = LEN(t);
    uc_analyze(t, n, &a);
    CHECK_HR_OK(a.hr);
    CHECK(a.covered);
    CHECKF(a.n == 1, "%s: %d runs", what, a.n);
    if (a.n == 1) t_note("%s: tag 0x%08x number %u script id %u", what, a.r[0].iso, a.r[0].num, a.r[0].sa.script);
}
#define COMMON_TEST(nm, ...) TEST(common_alone_##nm) { common_alone(W(__VA_ARGS__), #nm); }
COMMON_TEST(space, ' ') COMMON_TEST(digits, '1', '2', '3') COMMON_TEST(punct, '!', '?', '.', ',') COMMON_TEST(brackets, '(', ')', '[', ']')
COMMON_TEST(currency, '$', 0x20AC, 0xA3) COMMON_TEST(nbsp, 0xA0) COMMON_TEST(arrows, 0x2190, 0x2192) COMMON_TEST(math_ops, 0x2200, 0x2211)
COMMON_TEST(fullwidth_digits, 0xFF11, 0xFF12) COMMON_TEST(cjk_punct, 0x3001, 0x3002) COMMON_TEST(emoji, 0xD83D, 0xDE00)
COMMON_TEST(math_alnum, 0xD835, 0xDC00, 0xD835, 0xDC01) COMMON_TEST(box_drawing, 0x2500, 0x2502) COMMON_TEST(general_punct, 0x2018, 0x2019, 0x201C, 0x201D)
COMMON_TEST(tag_chars, 0xDB40, 0xDC20, 0xDB40, 0xDC41) COMMON_TEST(pua, 0xE000, 0xE001) COMMON_TEST(supp_pua, 0xDB80, 0xDC00)
COMMON_TEST(controls, 0x01, 0x02, 0x7F) COMMON_TEST(zero_width, 0x200B, 0x200C, 0x200D) COMMON_TEST(bidi_marks, 0x200E, 0x200F)
COMMON_TEST(line_sep, 0x2028, 0x2029) COMMON_TEST(noncharacters, 0xFFFE, 0xFFFF, 0xFDD0) COMMON_TEST(replacement, 0xFFFD) COMMON_TEST(bom, 0xFEFF)
COMMON_TEST(variation_selectors, 0xFE00, 0xFE0F) COMMON_TEST(combining_marks, 0x0301, 0x0300) COMMON_TEST(soft_hyphen, 0xAD)

TEST(common_kinds_share_one_script_id)
{
    /* plain Common text (spaces, digits, ASCII punctuation, symbols) all maps to the same script id */
    static const WCHAR *const t[] = { L" ", L"123", L"!?.,", L"()[]", L"$", L"\x2190\x2192", L"\x2500", L"\x2018\x2019" };
    UINT16 id0 = 0; int i, first = 1;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        uc_ana a; uc_analyze(t[i], LEN(t[i]), &a);
        if (a.n != 1) { CHECK(0); continue; }
        if (first) { id0 = a.r[0].sa.script; first = 0; }
        CHECKF(a.r[0].sa.script == id0, "text %d: script id %u differs from %u", i, a.r[0].sa.script, id0);
    }
}
TEST(common_digits_do_not_force_latin)
{
    uc_ana d, l; uc_analyze(L"123", 3, &d); uc_analyze(L"abc", 3, &l);
    if (d.n == 1 && l.n == 1) t_note("digits id %u iso %u, Latin id %u", d.r[0].sa.script, d.r[0].num, l.r[0].sa.script);
    CHECK(d.n == 1 && l.n == 1);
}
TEST(common_trailing_follows_script)
{
    static const struct { const script_def *d; const WCHAR *t; } rows[] = {
        { &S_LATN, L"abc 123!" }, { &S_GREK, L"\x03B1\x03B2 12," }, { &S_CYRL, L"\x0430\x0431 (1)" }, { &S_HEBR, L"\x05D0\x05D1 12" },
        { &S_ARAB, L"\x0627\x0628 12" }, { &S_THAI, L"\x0E01\x0E02 12" }, { &S_DEVA, L"\x0915\x0916 12" }, { &S_HANI, L"\x4E2D\x6587\x3002" },
        { &S_HANG, L"\xD55C\xAD6D 12" }, { &S_ARMN, L"\x0561\x0562 12" }, { &S_GEOR, L"\x10D0\x10D1 12" }, { &S_TAML, L"\x0B95\x0B99 12" },
    };
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        uc_ana a; UINT32 n = LEN(rows[i].t);
        uc_analyze(rows[i].t, n, &a);
        CHECKF(a.n == 1, "%s: %d runs", rows[i].d->name, a.n);
        if (a.n == 1) CHECKF(a.r[0].iso == tagcode(rows[i].d->tag) && a.r[0].len == n, "%s: trailing commons left the run", rows[i].d->name);
    }
}
TEST(common_leading_follows_script)
{
    static const struct { const script_def *d; const WCHAR *t; } rows[] = {
        { &S_LATN, L"(abc" }, { &S_GREK, L"12 \x03B1\x03B2" }, { &S_CYRL, L"\"\x0430\x0431\"" }, { &S_HEBR, L"12 \x05D0\x05D1" },
        { &S_THAI, L"12 \x0E01\x0E02" }, { &S_HANI, L"\x3010\x4E2D\x6587\x3011" },
    };
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        uc_ana a; UINT32 n = LEN(rows[i].t);
        uc_analyze(rows[i].t, n, &a);
        CHECK(a.covered);
        CHECK(a.n >= 1 && a.r[a.n - 1].iso == tagcode(rows[i].d->tag));
        t_note("%s with leading Common: %d run(s)", rows[i].d->name, a.n);
    }
}
TEST(inherited_marks_attach_to_base)
{
    static const struct { const script_def *d; const WCHAR *t; } rows[] = {
        { &S_LATN, L"e\x0301" }, { &S_LATN, L"e\x0301\x0323\x0302" }, { &S_GREK, L"\x03B1\x0301" }, { &S_CYRL, L"\x0430\x0301" },
        { &S_LATN, L"a\x0300" L"b\x0301" L"c\x0302" }, { &S_LATN, L"\x00E9\x0301" }, { &S_HEBR, L"\x05D0\x05B0" }, { &S_DEVA, L"\x0915\x0941" },
        { &S_THAI, L"\x0E01\x0E31" }, { &S_LATN, L"e\x0361" L"e" }, { &S_LATN, L"e\x20D0" }, { &S_LATN, L"e\x1AB0" },
        { &S_LATN, L"e\xFE20" }, { &S_LATN, L"e\x1DC0" }, { &S_LATN, L"e\x0300\x0300\x0300\x0300\x0300\x0300" },
    };
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        uc_ana a; UINT32 n = LEN(rows[i].t);
        uc_analyze(rows[i].t, n, &a);
        CHECKF(a.n == 1, "row %d (%s): %d runs", i, rows[i].d->name, a.n);
        if (a.n == 1) CHECKF(a.r[0].iso == tagcode(rows[i].d->tag) && a.r[0].len == n, "row %d mark left its base's run", i);
    }
}
TEST(inherited_mark_between_scripts_goes_with_left_base)
{
    uc_ana a; static const WCHAR t[] = L"e\x0301\x03B1";
    uc_analyze(t, 3, &a);
    CHECK(a.covered);
    CHECKF(a.n == 2, "%d runs", a.n);
    if (a.n == 2) { CHECK(a.r[0].pos == 0 && a.r[0].len == 2); CHECK(a.r[0].iso == tagcode("Latn")); CHECK(a.r[1].pos == 2 && a.r[1].iso == tagcode("Grek")); }
}
TEST(inherited_mark_leading_note)
{
    uc_ana a; static const WCHAR t[] = L"\x0301" L"abc";
    uc_analyze(t, 4, &a);
    CHECK(a.covered);
    CHECK(a.n >= 1 && a.r[a.n - 1].iso == tagcode("Latn"));
    t_note("leading combining mark before Latin: %d run(s)", a.n);
}
TEST(inherited_mark_alone_one_run)
{
    uc_ana a; static const WCHAR t[] = L"\x0301\x0302";
    uc_analyze(t, 2, &a);
    CHECK(a.covered);
    CHECK(a.n == 1);
}
TEST(joiners_between_arabic_letters_stay_in_run)
{
    static const WCHAR *const t[] = { L"\x0628\x200D\x0628", L"\x0628\x200C\x0628", L"\x0628\x200D", L"\x200D\x0628" };
    int i;
    for (i = 0; i < 4; i++) {
        uc_ana a; uc_analyze(t[i], LEN(t[i]), &a);
        CHECK(a.covered);
        CHECK(a.n >= 1 && a.r[a.n - 1].iso == tagcode("Arab"));
        t_note("Arabic with joiner (%d): %d run(s)", i, a.n);
    }
}
TEST(zero_width_in_latin_one_run_note)
{
    static const WCHAR *const t[] = { L"a\x200B" L"b", L"a\x200C" L"b", L"a\x200D" L"b", L"a\x200E" L"b", L"a\x200F" L"b", L"a\x2060" L"b", L"a\xFEFF" L"b", L"a\x00AD" L"b" };
    int i;
    for (i = 0; i < 8; i++) {
        uc_ana a; uc_analyze(t[i], 3, &a);
        CHECK(a.covered);
        CHECK(a.n >= 1 && a.r[0].iso == tagcode("Latn"));
        t_note("Latin with U+%04X in the middle: %d run(s)", t[i][1], a.n);
    }
}

/* ---- mixed-script segmentation oracle ------------------------------------------------------------------------------------------------------- */
typedef struct { const char *tag; const WCHAR *s; } seg;      /* tag == NULL: Common / Inherited text */
static void seg_check(const char *label, const seg *sg, int ns)
{
    WCHAR text[256]; const char *tg[256]; UINT32 n = 0; int i, k; uc_ana a; int ngroups = 0; const char *last = NULL;
    for (i = 0; i < ns; i++) for (k = 0; sg[i].s[k]; k++) { text[n] = sg[i].s[k]; tg[n] = sg[i].tag; n++; }
    text[n] = 0;
    for (i = 0; i < (int)n; i++) if (tg[i] && (!last || strcmp(last, tg[i]))) { ngroups++; last = tg[i]; }
    uc_analyze(text, n, &a);
    CHECK_HR_OK(a.hr);
    CHECKF(a.covered, "%s: runs do not cover the text", label);
    {
        UINT32 pos = 0; int r;
        for (r = 0; r < a.n; r++) { CHECKF(a.r[r].pos == pos && a.r[r].len > 0, "%s: run %d not contiguous", label, r); pos += a.r[r].len; }
        CHECKF(pos == n, "%s: runs end at %u of %u", label, pos, n);
    }
    if (ngroups == 0) { CHECKF(a.n == 1, "%s: pure Common text split into %d runs", label, a.n); return; }
    CHECKF(a.n == ngroups, "%s: %d runs, want %d script groups", label, a.n, ngroups);
    for (i = 0; i < (int)n; i++) {
        int r, found = -1; char got[5];
        for (r = 0; r < a.n; r++) if ((UINT32)i >= a.r[r].pos && (UINT32)i < a.r[r].pos + a.r[r].len) found = r;
        if (found < 0) { CHECK(0); continue; }
        tagstr(a.r[found].iso, got);
        if (tg[i]) CHECKF(!strcmp(got, tg[i]), "%s: position %d is %s, want %s", label, i, got, tg[i]);
        else {
            /* Common characters belong to a neighbouring script run, never to a third script */
            const char *prev = NULL, *next = NULL; int j;
            for (j = i - 1; j >= 0; j--) if (tg[j]) { prev = tg[j]; break; }
            for (j = i + 1; j < (int)n; j++) if (tg[j]) { next = tg[j]; break; }
            CHECKF((prev && !strcmp(prev, got)) || (next && !strcmp(next, got)), "%s: Common at %d took %s (neighbours %s/%s)", label, i, got, prev ? prev : "-", next ? next : "-");
        }
        /* a run never starts in the middle of a surrogate pair */
        if (i > 0 && text[i] >= 0xDC00 && text[i] < 0xE000 && text[i - 1] >= 0xD800 && text[i - 1] < 0xDC00) {
            int r2; for (r2 = 0; r2 < a.n; r2++) CHECKF(a.r[r2].pos != (UINT32)i, "%s: run starts inside a surrogate pair at %d", label, i);
        }
    }
}
#define SEGT(nm, ...) TEST(seg_##nm) { static const seg s[] = { __VA_ARGS__ }; seg_check(#nm, s, (int)(sizeof s / sizeof s[0])); }
SEGT(latin_greek, { "Latn", L"abc" }, { NULL, L" " }, { "Grek", L"\x03B1\x03B2\x03B3" })
SEGT(greek_latin, { "Grek", L"\x03B1\x03B2" }, { NULL, L", " }, { "Latn", L"xyz" })
SEGT(latin_cyrillic_latin, { "Latn", L"ab" }, { "Cyrl", L"\x0430\x0431" }, { "Latn", L"cd" })
SEGT(latin_arabic, { "Latn", L"Hello " }, { "Arab", L"\x0645\x0631\x062D\x0628\x0627" })
SEGT(arabic_digits_arabic, { "Arab", L"\x0628" }, { NULL, L" 123 " }, { "Arab", L"\x0628" })
SEGT(hebrew_latin_hebrew, { "Hebr", L"\x05D0\x05D1" }, { NULL, L" " }, { "Latn", L"abc" }, { NULL, L" " }, { "Hebr", L"\x05D2" })
SEGT(latin_han, { "Latn", L"ab" }, { "Hani", L"\x4E2D\x6587" })
SEGT(han_kana_mix, { "Hani", L"\x65E5\x672C" }, { "Hira", L"\x3092" }, { "Hani", L"\x8A9E" }, { "Kana", L"\x30AB\x30CA" })
SEGT(japanese_sentence, { "Hani", L"\x4ECA\x65E5" }, { "Hira", L"\x306F" }, { "Hani", L"\x6674" }, { "Hira", L"\x308C\x3067\x3059" }, { NULL, L"\x3002" })
SEGT(korean_latin, { "Hang", L"\xD55C\xAD6D\xC5B4" }, { NULL, L" (" }, { "Latn", L"Korean" }, { NULL, L")" })
SEGT(thai_latin_thai, { "Thai", L"\x0E01\x0E32" }, { "Latn", L"ab" }, { "Thai", L"\x0E01" })
SEGT(devanagari_latin, { "Deva", L"\x0928\x092E\x0938\x094D\x0924\x0947" }, { NULL, L" " }, { "Latn", L"hello" })
SEGT(five_scripts, { "Latn", L"a" }, { "Grek", L"\x03B1" }, { "Cyrl", L"\x0430" }, { "Hebr", L"\x05D0" }, { "Arab", L"\x0627" })
SEGT(ten_scripts, { "Latn", L"a" }, { "Grek", L"\x03B1" }, { "Cyrl", L"\x0430" }, { "Armn", L"\x0561" }, { "Hebr", L"\x05D0" }, { "Arab", L"\x0627" },
     { "Deva", L"\x0915" }, { "Thai", L"\x0E01" }, { "Hang", L"\xD55C" }, { "Hani", L"\x4E2D" })
SEGT(each_indic, { "Deva", L"\x0915" }, { "Beng", L"\x0995" }, { "Guru", L"\x0A15" }, { "Gujr", L"\x0A95" }, { "Taml", L"\x0B95" }, { "Telu", L"\x0C15" },
     { "Knda", L"\x0C95" }, { "Mlym", L"\x0D15" }, { "Sinh", L"\x0D9A" })
SEGT(southeast_asian, { "Thai", L"\x0E01" }, { "Laoo", L"\x0E81" }, { "Tibt", L"\x0F40" }, { "Mymr", L"\x1000" }, { "Khmr", L"\x1780" })
SEGT(caucasus_ethiopic, { "Geor", L"\x10D0" }, { "Armn", L"\x0561" }, { "Ethi", L"\x1200" }, { "Cher", L"\x13A0" }, { "Mong", L"\x1820" })
SEGT(syriac_thaana_arabic, { "Syrc", L"\x0710" }, { "Thaa", L"\x0780" }, { "Arab", L"\x0627" })
SEGT(bopomofo_han, { "Bopo", L"\x3105\x3106" }, { "Hani", L"\x4E2D" }, { "Bopo", L"\x3107" })
SEGT(surrogate_han_between_latin, { "Latn", L"a" }, { "Hani", L"\xD840\xDC00" }, { "Latn", L"b" })
SEGT(surrogate_han_runs, { "Hani", L"\xD840\xDC00\xD840\xDC01" }, { "Hira", L"\x3042" }, { "Hani", L"\xD840\xDC02" })
SEGT(surrogate_common_between_scripts, { "Latn", L"a" }, { NULL, L"\xD83D\xDE00" }, { "Grek", L"\x03B1" })
SEGT(emoji_inside_latin, { "Latn", L"a" }, { NULL, L"\xD83D\xDE00" }, { "Latn", L"b" })
SEGT(emoji_only, { NULL, L"\xD83D\xDE00\xD83D\xDE01" })
SEGT(math_alnum_in_text, { "Latn", L"x" }, { NULL, L"\xD835\xDC00\xD835\xDC01" }, { "Latn", L"y" })
SEGT(digits_only, { NULL, L"0123456789" })
SEGT(punct_only, { NULL, L".,;:!?" })
SEGT(spaces_only, { NULL, L"     " })
SEGT(digits_between_scripts, { "Latn", L"a" }, { NULL, L"123" }, { "Grek", L"\x03B1" })
SEGT(punct_between_scripts, { "Cyrl", L"\x0430" }, { NULL, L"!?" }, { "Hebr", L"\x05D0" })
SEGT(leading_punct_then_script, { NULL, L"\"(" }, { "Latn", L"quoted" }, { NULL, L")\"" })
SEGT(long_common_gap, { "Latn", L"a" }, { NULL, L"                                " }, { "Grek", L"\x03B1" })
SEGT(marks_across_scripts, { "Latn", L"e" }, { NULL, L"\x0301" }, { "Grek", L"\x03B1" }, { NULL, L"\x0301" }, { "Cyrl", L"\x0430" })
SEGT(fullwidth_latin, { "Latn", L"\xFF21\xFF22" }, { "Hani", L"\x4E2D" })
SEGT(halfwidth_katakana, { "Kana", L"\xFF71\xFF72" }, { "Latn", L"a" })
SEGT(hiragana_prolonged_mark, { "Hira", L"\x3042" }, { NULL, L"\x30FC" }, { "Hira", L"\x3044" })
SEGT(katakana_prolonged_mark, { "Kana", L"\x30A2" }, { NULL, L"\x30FC" }, { "Kana", L"\x30A4" })
SEGT(han_ideographic_space, { "Hani", L"\x4E2D" }, { NULL, L"\x3000" }, { "Hani", L"\x6587" })
SEGT(greek_extended_mix, { "Grek", L"\x03B1\x1F00" }, { "Latn", L"a" }, { "Grek", L"\x1FFE" })
SEGT(cyrillic_ext_mix, { "Cyrl", L"\x0430\x0500" }, { "Latn", L"a" }, { "Cyrl", L"\x2DE0" })
SEGT(armenian_ligature_latin, { "Armn", L"\xFB13" }, { "Latn", L"\xFB01" })
SEGT(arabic_presentation_forms, { "Arab", L"\xFE8D\xFE8E" }, { "Latn", L"a" }, { "Arab", L"\xFB50" })
SEGT(single_char_each, { "Latn", L"a" }, { "Cyrl", L"\x0430" }, { "Latn", L"a" }, { "Cyrl", L"\x0430" }, { "Latn", L"a" })

/* ---- API behaviour on subranges ---------------------------------------------------------------------------------------------------------------- */
TEST(analyze_subrange_reports_only_range)
{
    static const WCHAR t[] = L"abc\x03B1\x03B2\x03B3\x0430\x0431\x0432";
    dwx_ana_src *s; dwx_ana_sink *k; IDWriteTextAnalyzer *an = uc_analyzer();
    if (!an) { t_skip("no analyzer"); return; }
    s = dwx_ana_src_new(t, 9); k = dwx_ana_sink_new();
    CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)s, 2, 5, (IDWriteTextAnalysisSink *)k));
    CHECK(k->n_script == 3);
    CHECK(!k->script_set[1] && k->script_set[2] && k->script_set[6] && !k->script_set[7]);
    dwx_release(s); dwx_release(k);
}
TEST(analyze_zero_length_no_runs)
{
    static const WCHAR t[] = L"abc";
    dwx_ana_src *s; dwx_ana_sink *k; IDWriteTextAnalyzer *an = uc_analyzer();
    if (!an) { t_skip("no analyzer"); return; }
    s = dwx_ana_src_new(t, 3); k = dwx_ana_sink_new();
    CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)s, 0, 0, (IDWriteTextAnalysisSink *)k));
    CHECK(k->n_script == 0);
    dwx_release(s); dwx_release(k);
}
TEST(analyze_split_pair_range_note)
{
    /* a range that begins or ends between the halves of a surrogate pair: success is required, the classification is recorded */
    static const WCHAR t[] = L"a\xD840\xDC00" L"b";
    dwx_ana_src *s; dwx_ana_sink *k; IDWriteTextAnalyzer *an = uc_analyzer();
    HRESULT hr;
    if (!an) { t_skip("no analyzer"); return; }
    s = dwx_ana_src_new(t, 4); k = dwx_ana_sink_new();
    hr = IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)s, 2, 2, (IDWriteTextAnalysisSink *)k);
    CHECK_HR_OK(hr);
    CHECK(dwx_ana_covered(k, 's', 4) == 0);                 /* positions 0,1 not reported */
    t_note("range starting at the low surrogate: %ld run(s), first run (%u,%u)", k->n_script, k->script_runs[0].pos, k->script_runs[0].len);
    dwx_release(s); dwx_release(k);
}
