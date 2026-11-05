/* IDWriteTextAnalyzer1::GetScriptProperties: every table index, ISO 15924 consistency, and the script ids AnalyzeScript hands out for
 * sample characters of many scripts.
 * Windows 10 (IDWriteTextAnalyzer::GetScriptProperties): index < 0x99 copies 20 bytes (DWRITE_SCRIPT_PROPERTIES) of a 32 byte table entry and
 * returns S_OK; any larger index fills entry 0 and returns E_INVALIDARG (0x80070057); DWRITE_SCRIPT_ANALYSIS.shapes plays no part. */
#include "r2.h"

typedef struct { const char *code; unsigned num; } iso_row;
/* ISO 15924 numeric codes of the scripts a Windows 10 table can plausibly carry */
static const iso_row k_iso[] = {
    { "Adlm", 166 }, { "Aghb", 239 }, { "Ahom", 338 }, { "Arab", 160 }, { "Armi", 124 }, { "Armn", 230 }, { "Avst", 134 }, { "Bali", 360 },
    { "Bamu", 435 }, { "Bass", 259 }, { "Batk", 365 }, { "Beng", 325 }, { "Bhks", 334 }, { "Bopo", 285 }, { "Brah", 300 }, { "Brai", 570 },
    { "Bugi", 367 }, { "Buhd", 372 }, { "Cakm", 349 }, { "Cans", 440 }, { "Cari", 201 }, { "Cham", 358 }, { "Cher", 445 }, { "Copt", 204 },
    { "Cprt", 403 }, { "Cyrl", 220 }, { "Deva", 315 }, { "Dsrt", 250 }, { "Dupl", 755 }, { "Egyp", 50 }, { "Elba", 226 }, { "Ethi", 430 },
    { "Geor", 240 }, { "Glag", 225 }, { "Goth", 206 }, { "Gran", 343 }, { "Grek", 200 }, { "Gujr", 320 }, { "Guru", 310 }, { "Hang", 286 },
    { "Hani", 500 }, { "Hano", 371 }, { "Hebr", 125 }, { "Hira", 410 }, { "Hluw", 80 }, { "Hmng", 450 }, { "Hung", 176 }, { "Ital", 210 },
    { "Java", 361 }, { "Kali", 357 }, { "Kana", 411 }, { "Khar", 305 }, { "Khmr", 355 }, { "Khoj", 322 }, { "Knda", 345 }, { "Kthi", 317 },
    { "Lana", 351 }, { "Laoo", 356 }, { "Latn", 215 }, { "Lepc", 335 }, { "Limb", 336 }, { "Lina", 400 }, { "Linb", 401 }, { "Lisu", 399 },
    { "Lyci", 202 }, { "Lydi", 116 }, { "Mahj", 314 }, { "Mand", 140 }, { "Mani", 139 }, { "Mend", 438 }, { "Merc", 101 }, { "Mero", 100 },
    { "Mlym", 347 }, { "Modi", 324 }, { "Mong", 145 }, { "Mroo", 199 }, { "Mtei", 337 }, { "Mult", 323 }, { "Mymr", 350 }, { "Narb", 106 },
    { "Nbat", 159 }, { "Newa", 333 }, { "Nkoo", 165 }, { "Ogam", 212 }, { "Olck", 261 }, { "Orkh", 175 }, { "Orya", 327 }, { "Osge", 219 },
    { "Osma", 260 }, { "Palm", 126 }, { "Pauc", 263 }, { "Perm", 227 }, { "Phag", 331 }, { "Phli", 131 }, { "Phlp", 132 }, { "Phnx", 115 },
    { "Plrd", 282 }, { "Prti", 130 }, { "Rjng", 363 }, { "Runr", 211 }, { "Samr", 123 }, { "Sarb", 105 }, { "Saur", 344 }, { "Shaw", 281 },
    { "Shrd", 319 }, { "Sidd", 302 }, { "Sind", 318 }, { "Sinh", 348 }, { "Sora", 398 }, { "Sund", 362 }, { "Sylo", 316 }, { "Syrc", 135 },
    { "Tagb", 373 }, { "Takr", 321 }, { "Tale", 353 }, { "Talu", 354 }, { "Taml", 346 }, { "Tavt", 359 }, { "Telu", 340 }, { "Tfng", 120 },
    { "Tglg", 370 }, { "Thaa", 170 }, { "Thai", 352 }, { "Tibt", 330 }, { "Tirh", 326 }, { "Ugar", 40 }, { "Vaii", 470 }, { "Wara", 262 },
    { "Xpeo", 30 }, { "Xsux", 20 }, { "Yiii", 460 }, { "Zinh", 994 }, { "Zmth", 995 }, { "Zsym", 996 }, { "Zxxx", 997 }, { "Zyyy", 998 }, { "Zzzz", 999 },
};
static const iso_row *iso_find(const char *code) { unsigned i; for (i = 0; i < sizeof k_iso / sizeof *k_iso; i++) if (!strcmp(k_iso[i].code, code)) return &k_iso[i]; return NULL; }
static void code_str(UINT32 c, char o[5]) { o[0] = (char)c; o[1] = (char)(c >> 8); o[2] = (char)(c >> 16); o[3] = (char)(c >> 24); o[4] = 0; }
static UINT32 flags_of(const DWRITE_SCRIPT_PROPERTIES *p) {
    return p->restrictCaretToClusters | (p->usesWordDividers << 1) | (p->isDiscreteWriting << 2) | (p->isBlockWriting << 3) |
           (p->isDistributedWithinCluster << 4) | (p->isConnectedWriting << 5) | (p->isCursiveWriting << 6);
}
static HRESULT props(IDWriteTextAnalyzer1 *a1, UINT32 idx, DWRITE_SCRIPT_PROPERTIES *p) {
    DWRITE_SCRIPT_ANALYSIS sa; sa.script = (UINT16)idx; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    memset(p, 0xCC, sizeof *p);
    return IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, p);
}

/* Called by the generated sp_idx_NNN tests. */
int r2_sp_check_index(IDWriteTextAnalyzer1 *a1, UINT32 idx) {
    DWRITE_SCRIPT_PROPERTIES p, q; char s[5]; DWRITE_SCRIPT_ANALYSIS sa; const iso_row *r;
    HRESULT hr = props(a1, idx, &p);
    CHECK_HR(hr, S_OK);
    code_str(p.isoScriptCode, s);
    CHECKF(s[0] >= 'A' && s[0] <= 'Z' && s[1] >= 'a' && s[1] <= 'z' && s[2] >= 'a' && s[2] <= 'z' && s[3] >= 'a' && s[3] <= 'z', "index %u code \"%s\"", idx, s);
    CHECKF(p.isoScriptNumber >= 1 && p.isoScriptNumber <= 999, "index %u number %u", idx, p.isoScriptNumber);
    CHECKF(p.clusterLookahead <= 16, "index %u lookahead %u", idx, p.clusterLookahead);
    CHECKF(p.justificationCharacter <= 0x10FFFF, "index %u justification U+%X", idx, p.justificationCharacter);
    CHECKF(p.reserved == 0, "index %u reserved bits %x", idx, p.reserved);
    /* the shapes field is never read, the entry is a pure function of the index */
    sa.script = (UINT16)idx; sa.shapes = DWRITE_SCRIPT_SHAPES_NO_VISUAL;
    memset(&q, 0, sizeof q);
    CHECK_HR(IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, &q), S_OK);
    CHECK(!memcmp(&p, &q, sizeof p));
    r = iso_find(s);
    if (r) { if (r->num != p.isoScriptNumber) t_note("index %u: %s has number %u, ISO 15924 lists %u", idx, s, p.isoScriptNumber, r->num); }
    else t_note("index %u: code %s is not in the reference list", idx, s);
    t_note("index %u: %s/%u lookahead=%u justify=U+%04X flags=0x%02x", idx, s, p.isoScriptNumber, p.clusterLookahead, p.justificationCharacter, flags_of(&p));
    return 1;
}

RT(sp_out_of_range_boundary_fills_entry_zero) {
    R2_BEGIN(); DWRITE_SCRIPT_PROPERTIES p0, p; unsigned i;
    static const UINT32 bad[] = { 153, 154, 155, 160, 191, 192, 255, 256, 257, 511, 512, 1023, 4096, 0x7FFE, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF };
    CHECK_HR(props(a1, 0, &p0), S_OK);
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        CHECK_HR(props(a1, bad[i], &p), R2_E_INVALIDARG);
        CHECKF(!memcmp(&p, &p0, sizeof p), "index %u: output is not entry 0", bad[i]);
    }
    CHECK_HR(props(a1, 152, &p), S_OK);
    R2_END();
}
RT(sp_indices_differ_in_at_least_one_field_per_pair_note) {
    R2_BEGIN(); DWRITE_SCRIPT_PROPERTIES p[153]; unsigned i, j, dups = 0;
    for (i = 0; i < 153; i++) CHECK_HR(props(a1, i, &p[i]), S_OK);
    for (i = 0; i < 153; i++) for (j = i + 1; j < 153; j++) if (p[i].isoScriptCode == p[j].isoScriptCode) { dups++; t_note("indices %u and %u share ISO code", i, j); }
    CHECKF(dups == 0, "%u duplicate ISO codes", dups);
    R2_END();
}
RT(sp_entry_zero_and_one_are_the_catch_alls_note) {
    R2_BEGIN(); DWRITE_SCRIPT_PROPERTIES p; char s[5]; unsigned i;
    for (i = 0; i < 4; i++) { CHECK_HR(props(a1, i, &p), S_OK); code_str(p.isoScriptCode, s); t_note("index %u = %s/%u", i, s, p.isoScriptNumber); }
    R2_END();
}
RT(sp_iso_number_agrees_with_reference_list) {
    R2_BEGIN(); DWRITE_SCRIPT_PROPERTIES p; char s[5]; unsigned i, known = 0, mismatch = 0;
    for (i = 0; i < 153; i++) {
        const iso_row *r;
        CHECK_HR(props(a1, i, &p), S_OK); code_str(p.isoScriptCode, s); r = iso_find(s);
        if (r) { known++; if (r->num != p.isoScriptNumber) { mismatch++; t_note("%s: reports %u, reference %u", s, p.isoScriptNumber, r->num); } }
    }
    t_note("%u of 153 codes found in the reference list, %u numbers differ", known, mismatch);
    CHECKF(known > 100, "only %u codes recognised", known);
    R2_END();
}
RT(sp_flag_combinations_present_note) {
    R2_BEGIN(); DWRITE_SCRIPT_PROPERTIES p; unsigned i, seen[128], n = 0, k, j;
    for (i = 0; i < 153; i++) {
        UINT32 fl; int found = 0;
        CHECK_HR(props(a1, i, &p), S_OK); fl = flags_of(&p);
        for (k = 0; k < n; k++) if (seen[k] == fl) found = 1;
        if (!found && n < 128) seen[n++] = fl;
    }
    for (j = 0; j < n; j++) {
        char names[200]; int o = 0; unsigned cnt = 0;
        for (i = 0; i < 153; i++) { props(a1, i, &p); if (flags_of(&p) == seen[j]) cnt++; }
        o += sprintf(names + o, "%s%s%s%s%s%s%s", seen[j] & 1 ? "caret " : "", seen[j] & 2 ? "wordDiv " : "", seen[j] & 4 ? "discrete " : "", seen[j] & 8 ? "block " : "",
                     seen[j] & 16 ? "distributed " : "", seen[j] & 32 ? "connected " : "", seen[j] & 64 ? "cursive" : "");
        t_note("flag set 0x%02x (%s): %u scripts", seen[j], names, cnt);
    }
    CHECK(n >= 2);
    R2_END();
}
RT(sp_clusterlookahead_distribution_note) {
    R2_BEGIN(); DWRITE_SCRIPT_PROPERTIES p; unsigned i, hist[17] = { 0 };
    for (i = 0; i < 153; i++) { CHECK_HR(props(a1, i, &p), S_OK); if (p.clusterLookahead <= 16) hist[p.clusterLookahead]++; }
    for (i = 0; i <= 16; i++) if (hist[i]) t_note("clusterLookahead %u: %u scripts", i, hist[i]);
    R2_END();
}
RT(sp_justification_characters_note) {
    R2_BEGIN(); DWRITE_SCRIPT_PROPERTIES p; unsigned i; UINT32 seen[32]; unsigned n = 0, k;
    for (i = 0; i < 153; i++) {
        int found = 0;
        CHECK_HR(props(a1, i, &p), S_OK);
        for (k = 0; k < n; k++) if (seen[k] == p.justificationCharacter) found = 1;
        if (!found && n < 32) seen[n++] = p.justificationCharacter;
    }
    for (k = 0; k < n; k++) t_note("justification character U+%04X is used", seen[k]);
    R2_END();
}
RT(sp_analyzer_object_independent_results) {
    R2_BEGIN(); IDWriteTextAnalyzer *o = NULL; IDWriteTextAnalyzer1 *o1 = NULL; unsigned i;
    if (SUCCEEDED(IDWriteFactory_CreateTextAnalyzer(f, &o)) && o && SUCCEEDED(IDWriteTextAnalyzer_QueryInterface(o, &IID_IDWriteTextAnalyzer1, (void **)&o1))) {
        for (i = 0; i < 153; i += 3) { DWRITE_SCRIPT_PROPERTIES p, q; props(a1, i, &p); props(o1, i, &q); CHECKF(!memcmp(&p, &q, sizeof p), "index %u", i); }
    }
    DW_RELEASE(o1); DW_RELEASE(o);
    R2_END();
}

/* ---- sample characters of many scripts: AnalyzeScript id -> GetScriptProperties ----------------------------------------------------------------- */
typedef struct { unsigned cp; const char *code; unsigned num; int hard; } sample;
static int utf16(unsigned cp, WCHAR *w) { if (cp < 0x10000) { w[0] = (WCHAR)cp; return 1; } cp -= 0x10000; w[0] = (WCHAR)(0xD800 + (cp >> 10)); w[1] = (WCHAR)(0xDC00 + (cp & 0x3FF)); return 2; }
static void run_samples(const sample *rows, unsigned nrows, const char *group) {
    r2_ctx c_; unsigned i, hardfail = 0, softmiss = 0;
    if (!r2_open(&c_)) { r2_close(&c_); t_skip("analyzer unavailable"); return; }
    for (i = 0; i < nrows; i++) {
        WCHAR w[4]; int n = utf16(rows[i].cp, w); DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, w, (UINT32)n); DWRITE_SCRIPT_PROPERTIES p; char s[5] = "????";
        int ok = 0;
        if (sa.script != 0xFFFF && sa.script < 153 && props(c_.a1, sa.script, &p) == S_OK) {
            code_str(p.isoScriptCode, s);
            ok = !strcmp(s, rows[i].code) && p.isoScriptNumber == rows[i].num;
        }
        if (rows[i].hard) { CHECKF(ok, "%s U+%04X (%s/%u): got index %u code %s", group, rows[i].cp, rows[i].code, rows[i].num, sa.script, s); if (!ok) hardfail++; }
        else if (!ok) { softmiss++; t_note("%s U+%04X expected %s/%u, got index %u code %s", group, rows[i].cp, rows[i].code, rows[i].num, sa.script, s); }
    }
    t_note("%s: %u rows, %u hard failures, %u soft misses", group, nrows, hardfail, softmiss);
    r2_close(&c_);
}
RT(sp_samples_unicode_core_scripts) {
    static const sample k[] = {
        { 0x0041, "Latn", 215, 1 }, { 0x00E9, "Latn", 215, 1 }, { 0x0391, "Grek", 200, 1 }, { 0x0416, "Cyrl", 220, 1 }, { 0x0531, "Armn", 230, 1 },
        { 0x05D0, "Hebr", 125, 1 }, { 0x0627, "Arab", 160, 1 }, { 0x0710, "Syrc", 135, 1 }, { 0x0780, "Thaa", 170, 1 }, { 0x0905, "Deva", 315, 1 },
        { 0x0985, "Beng", 325, 1 }, { 0x0A05, "Guru", 310, 1 }, { 0x0A85, "Gujr", 320, 1 }, { 0x0B05, "Orya", 327, 1 }, { 0x0B85, "Taml", 346, 1 },
        { 0x0C05, "Telu", 340, 1 }, { 0x0C85, "Knda", 345, 1 }, { 0x0D05, "Mlym", 347, 1 }, { 0x0D85, "Sinh", 348, 1 }, { 0x0E01, "Thai", 352, 1 },
        { 0x0E81, "Laoo", 356, 1 }, { 0x0F40, "Tibt", 330, 1 }, { 0x1000, "Mymr", 350, 1 }, { 0x10D0, "Geor", 240, 1 }, { 0x1100, "Hang", 286, 1 },
        { 0x1200, "Ethi", 430, 1 }, { 0x1780, "Khmr", 355, 1 }, { 0x1820, "Mong", 145, 1 }, { 0x3042, "Hira", 410, 1 }, { 0x30A2, "Kana", 411, 1 },
        { 0x3105, "Bopo", 285, 1 }, { 0x4E00, "Hani", 500, 1 }, { 0xAC00, "Hang", 286, 1 },
    };
    run_samples(k, sizeof k / sizeof *k, "core");
}
RT(sp_samples_unicode_4_to_5_scripts) {
    static const sample k[] = {
        { 0x13A0, "Cher", 445, 1 }, { 0x1401, "Cans", 440, 1 }, { 0x1681, "Ogam", 212, 1 }, { 0x16A0, "Runr", 211, 1 }, { 0xA000, "Yiii", 460, 1 },
        { 0x10300, "Ital", 210, 1 }, { 0x10330, "Goth", 206, 1 }, { 0x10400, "Dsrt", 250, 1 }, { 0x2800, "Brai", 570, 1 }, { 0x1700, "Tglg", 370, 1 },
        { 0x1720, "Hano", 371, 1 }, { 0x1740, "Buhd", 372, 1 }, { 0x1760, "Tagb", 373, 1 }, { 0x1900, "Limb", 336, 1 }, { 0x1950, "Tale", 353, 1 },
        { 0x10000, "Linb", 401, 1 }, { 0x10380, "Ugar", 40, 1 }, { 0x103A0, "Xpeo", 30, 1 }, { 0x10800, "Cprt", 403, 1 }, { 0x10450, "Shaw", 281, 1 },
        { 0x10480, "Osma", 260, 1 }, { 0x12000, "Xsux", 20, 1 }, { 0x10900, "Phnx", 115, 1 }, { 0x2C80, "Copt", 204, 1 }, { 0x2C00, "Glag", 225, 1 },
        { 0x2D30, "Tfng", 120, 1 }, { 0xA840, "Phag", 331, 1 }, { 0xA800, "Sylo", 316, 1 }, { 0x07C0, "Nkoo", 165, 1 }, { 0x1B05, "Bali", 360, 1 },
        { 0x10A00, "Khar", 305, 1 }, { 0x1980, "Talu", 354, 1 },
    };
    run_samples(k, sizeof k / sizeof *k, "unicode 4-5");
}
RT(sp_samples_unicode_5_1_to_6_scripts) {
    static const sample k[] = {
        { 0x1B83, "Sund", 362, 0 }, { 0x1C00, "Lepc", 335, 0 }, { 0x1C5A, "Olck", 261, 0 }, { 0xA500, "Vaii", 470, 0 }, { 0xA882, "Saur", 344, 0 },
        { 0xA90A, "Kali", 357, 0 }, { 0xA93F, "Rjng", 363, 0 }, { 0xAA00, "Cham", 358, 0 }, { 0xA4D0, "Lisu", 399, 0 }, { 0xA984, "Java", 361, 0 },
        { 0x0800, "Samr", 123, 0 }, { 0x0840, "Mand", 140, 0 }, { 0x10840, "Armi", 124, 0 }, { 0x10B00, "Avst", 134, 0 }, { 0x13000, "Egyp", 50, 0 },
        { 0x10280, "Lyci", 202, 0 }, { 0x102A0, "Cari", 201, 0 }, { 0x10920, "Lydi", 116, 0 }, { 0xABC0, "Mtei", 337, 0 }, { 0x1BC0, "Batk", 365, 0 },
        { 0x1A00, "Bugi", 367, 0 }, { 0x1A20, "Lana", 351, 0 }, { 0xAA80, "Tavt", 359, 0 }, { 0x1BC0, "Batk", 365, 0 }, { 0xA6A0, "Bamu", 435, 0 },
        { 0x10B40, "Prti", 130, 0 }, { 0x10B60, "Phli", 131, 0 }, { 0x10A60, "Sarb", 105, 0 }, { 0x10C00, "Orkh", 175, 0 }, { 0x11000, "Brah", 300, 0 },
        { 0x11100, "Cakm", 349, 0 }, { 0x16F00, "Plrd", 282, 0 }, { 0x11180, "Shrd", 319, 0 }, { 0x110D0, "Sora", 398, 0 }, { 0x11680, "Takr", 321, 0 },
        { 0x10000, "Linb", 401, 0 },
    };
    run_samples(k, sizeof k / sizeof *k, "unicode 5.1-6");
}
RT(sp_samples_unicode_7_and_later_scripts) {
    static const sample k[] = {
        { 0x10500, "Elba", 226, 0 }, { 0x10530, "Aghb", 239, 0 }, { 0x11300, "Gran", 343, 0 }, { 0x11480, "Tirh", 326, 0 }, { 0x11580, "Sidd", 302, 0 },
        { 0x11600, "Modi", 324, 0 }, { 0x11700, "Ahom", 338, 0 }, { 0x14400, "Hluw", 80, 0 }, { 0x1E800, "Adlm", 166, 0 }, { 0x11400, "Newa", 333, 0 },
        { 0x16A40, "Mroo", 199, 0 }, { 0x10880, "Nbat", 159, 0 }, { 0x10860, "Palm", 126, 0 }, { 0x10A80, "Narb", 106, 0 }, { 0x10C80, "Hung", 176, 0 },
        { 0x10AC0, "Mani", 139, 0 }, { 0x1E000, "Glag", 225, 0 }, { 0x11200, "Khoj", 322, 0 }, { 0x11280, "Mult", 323, 0 }, { 0x112B0, "Sind", 318, 0 },
        { 0x11150, "Mahj", 314, 0 }, { 0x11480, "Tirh", 326, 0 }, { 0x16AD0, "Bass", 259, 0 }, { 0x16B00, "Hmng", 450, 0 }, { 0x1BC00, "Dupl", 755, 0 },
        { 0x118A0, "Wara", 262, 0 }, { 0x11AC0, "Pauc", 263, 0 }, { 0x11C00, "Bhks", 334, 0 }, { 0x1E900, "Adlm", 166, 0 }, { 0x104B0, "Osge", 219, 0 },
    };
    run_samples(k, sizeof k / sizeof *k, "unicode 7+");
}
RT(sp_common_and_inherited_samples_note) {
    R2_BEGIN(); static const unsigned cps[] = { 0x20, 0x21, 0x30, 0x39, 0x2013, 0x3000, 0x20AC, 0x300, 0x301, 0x200D, 0x200C, 0xFE0F, 0xFF10, 0x1F600, 0x2603 };
    unsigned i;
    for (i = 0; i < sizeof cps / sizeof *cps; i++) {
        WCHAR w[2]; int n = utf16(cps[i], w); DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, w, (UINT32)n); DWRITE_SCRIPT_PROPERTIES p; char s[5] = "????";
        if (sa.script < 153 && props(a1, sa.script, &p) == S_OK) code_str(p.isoScriptCode, s);
        t_note("U+%04X alone -> script index %u (%s)", cps[i], sa.script, s);
        CHECKF(sa.script < 153, "U+%04X: script id %u out of table", cps[i], sa.script);
    }
    R2_END();
}
RT(sp_text_script_ids_stay_inside_table) {
    /* every id handed out by AnalyzeScript for a broad sweep of code points resolves through GetScriptProperties with S_OK */
    R2_BEGIN(); unsigned cp, bad = 0, n = 0; DWRITE_SCRIPT_PROPERTIES p;
    for (cp = 0x20; cp < 0x20000; cp += (cp < 0x3000 ? 1 : 13)) {
        WCHAR w[2]; int len; DWRITE_SCRIPT_ANALYSIS sa;
        if (cp >= 0xD800 && cp < 0xE000) continue;
        len = utf16(cp, w); sa = r2_sa_of(&c_, w, (UINT32)len); n++;
        if (sa.script == 0xFFFF || props(a1, sa.script, &p) != S_OK) { if (bad++ < 10) CHECKF(0, "U+%04X: id %u", cp, sa.script); }
    }
    t_note("%u code points swept", n);
    CHECK(bad == 0);
    R2_END();
}
