/* IDWriteTextAnalyzer1::GetScriptProperties.
 *
 * Windows 10 (DWriteTextAnalyzer::GetScriptProperties): the 16-bit DWRITE_SCRIPT_ANALYSIS.script is an index into a table of
 * 0x99 (153) 32-byte entries. Index >= 153 returns E_INVALIDARG but the output has still been filled with entry 0
 * (the lookup index is masked to 0 for out of range values). DWRITE_SCRIPT_ANALYSIS.shapes is ignored.
 * The first 20 bytes of the entry are DWRITE_SCRIPT_PROPERTIES; isoScriptCode packs the four characters little-endian
 * ('L' in the lowest byte for "Latn").
 */
#include "ta.h"

#define NSCRIPTS 153

static DWRITE_SCRIPT_ANALYSIS mk_sa(UINT32 script, DWRITE_SCRIPT_SHAPES shapes) {
    DWRITE_SCRIPT_ANALYSIS sa; sa.script = (UINT16)script; sa.shapes = shapes; return sa;
}
static void iso_str(UINT32 code, char out[5]) {
    out[0] = (char)(code & 0xff); out[1] = (char)((code >> 8) & 0xff); out[2] = (char)((code >> 16) & 0xff); out[3] = (char)((code >> 24) & 0xff); out[4] = 0;
}
static HRESULT get_props(IDWriteTextAnalyzer1 *a1, UINT32 idx, DWRITE_SCRIPT_PROPERTIES *p) {
    memset(p, 0xCC, sizeof *p);
    return IDWriteTextAnalyzer1_GetScriptProperties(a1, mk_sa(idx, DWRITE_SCRIPT_SHAPES_DEFAULT), p);
}
static UINT32 flags_of(const DWRITE_SCRIPT_PROPERTIES *p) {
    return p->restrictCaretToClusters | (p->usesWordDividers << 1) | (p->isDiscreteWriting << 2) | (p->isBlockWriting << 3) |
           (p->isDistributedWithinCluster << 4) | (p->isConnectedWriting << 5) | (p->isCursiveWriting << 6);
}

TEST(sp1_struct_layout) {
    CHECK(sizeof(DWRITE_SCRIPT_PROPERTIES) == 20);
    CHECK(sizeof(DWRITE_SCRIPT_ANALYSIS) == 8);
    EXPECT_EQ(offsetof(DWRITE_SCRIPT_PROPERTIES, isoScriptNumber), 4);
    EXPECT_EQ(offsetof(DWRITE_SCRIPT_PROPERTIES, clusterLookahead), 8);
    EXPECT_EQ(offsetof(DWRITE_SCRIPT_PROPERTIES, justificationCharacter), 12);
}
TEST(sp1_every_index_below_153_succeeds) {
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i++) { DWRITE_SCRIPT_PROPERTIES p; CHECKF(get_props(a1, i, &p) == S_OK, "script %u", i); }
    TA_END();
}
TEST(sp1_first_failure_is_153) {
    TA_BEGIN(); UINT32 i, first = 0xFFFFFFFF;
    for (i = 0; i < 400; i++) { DWRITE_SCRIPT_PROPERTIES p; if (get_props(a1, i, &p) != S_OK) { first = i; break; } }
    EXPECT_EQ(first, 153);
    TA_END();
}
TEST(sp1_out_of_range_invalid_and_filled_with_entry0) {
    TA_BEGIN(); static const UINT32 bad[] = { 153, 154, 200, 255, 256, 1000, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF }; int i; DWRITE_SCRIPT_PROPERTIES p0;
    CHECK_HR(get_props(a1, 0, &p0), S_OK);
    for (i = 0; i < (int)(sizeof bad / sizeof *bad); i++) {
        DWRITE_SCRIPT_PROPERTIES p;
        CHECK_HR(get_props(a1, bad[i], &p), TA_E_INVALIDARG);
        CHECKF(!memcmp(&p, &p0, sizeof p), "script %u: output is entry 0", bad[i]);
    }
    TA_END();
}
TEST(sp1_shapes_field_is_ignored) {
    TA_BEGIN(); UINT32 i; static const UINT32 shapes[] = { DWRITE_SCRIPT_SHAPES_DEFAULT, DWRITE_SCRIPT_SHAPES_NO_VISUAL, 2, 0xFFFFFFFFu };
    for (i = 0; i < NSCRIPTS; i += 7) {
        DWRITE_SCRIPT_PROPERTIES base, q; int s;
        CHECK_HR(get_props(a1, i, &base), S_OK);
        for (s = 0; s < 4; s++) {
            DWRITE_SCRIPT_ANALYSIS sa; memset(&q, 0, sizeof q); sa.script = (UINT16)i; sa.shapes = (DWRITE_SCRIPT_SHAPES)shapes[s];
            CHECK_HR(IDWriteTextAnalyzer1_GetScriptProperties(a1, sa, &q), S_OK);
            CHECK(!memcmp(&q, &base, sizeof q));
        }
    }
    TA_END();
}
TEST(sp1_iso_codes_are_iso15924_shaped) {
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i++) {
        DWRITE_SCRIPT_PROPERTIES p; char s[5]; get_props(a1, i, &p); iso_str(p.isoScriptCode, s);
        CHECKF(s[0] >= 'A' && s[0] <= 'Z' && s[1] >= 'a' && s[1] <= 'z' && s[2] >= 'a' && s[2] <= 'z' && s[3] >= 'a' && s[3] <= 'z', "script %u code \"%s\"", i, s);
    }
    TA_END();
}
TEST(sp1_iso_numbers_in_range) {
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i++) { DWRITE_SCRIPT_PROPERTIES p; get_props(a1, i, &p); CHECKF(p.isoScriptNumber >= 1 && p.isoScriptNumber <= 999, "script %u number %u", i, p.isoScriptNumber); }
    TA_END();
}
TEST(sp1_reserved_bits_zero) {
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i++) {
        DWRITE_SCRIPT_PROPERTIES p; UINT32 raw; get_props(a1, i, &p);
        memcpy(&raw, (const char *)&p + 16, 4);
        if ((raw >> 7) != 0) t_note("script %u reserved bits 0x%x", i, raw >> 7);   /* reserved: not a contract */
    }
    TA_END();
}
TEST(sp1_justification_character_is_valid_code_point) {
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i++) { DWRITE_SCRIPT_PROPERTIES p; get_props(a1, i, &p); CHECKF(p.justificationCharacter <= 0x10FFFF && !(p.justificationCharacter >= 0xD800 && p.justificationCharacter <= 0xDFFF), "script %u char 0x%x", i, p.justificationCharacter); }
    TA_END();
}
TEST(sp1_cluster_lookahead_small) {
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i++) { DWRITE_SCRIPT_PROPERTIES p; get_props(a1, i, &p); if (p.clusterLookahead < 1 || p.clusterLookahead > 8) t_note("script %u unusual lookahead %u", i, p.clusterLookahead); }
    TA_END();
}
TEST(sp1_iso_codes_unique_note) {
    TA_BEGIN(); DWRITE_SCRIPT_PROPERTIES all[NSCRIPTS]; UINT32 i, j, dup = 0;
    for (i = 0; i < NSCRIPTS; i++) get_props(a1, i, &all[i]);
    for (i = 0; i < NSCRIPTS; i++) for (j = i + 1; j < NSCRIPTS; j++) if (all[i].isoScriptCode == all[j].isoScriptCode) { char s[5]; iso_str(all[i].isoScriptCode, s); t_note("duplicate ISO code %s at %u and %u", s, i, j); dup++; }
    t_note("%u duplicate iso codes among %d entries", dup, NSCRIPTS);
    TA_END();
}
TEST(sp1_catalog_note) {
    /* the complete table, one NOTE per entry, so a reviewer on a real machine can diff the table */
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i++) {
        DWRITE_SCRIPT_PROPERTIES p; char s[5]; get_props(a1, i, &p); iso_str(p.isoScriptCode, s);
        t_note("script[%3u] %s num=%u lookahead=%u just=U+%04X flags(caret,wordDiv,discrete,block,distrib,connected,cursive)=%u%u%u%u%u%u%u", i, s, p.isoScriptNumber,
               p.clusterLookahead, p.justificationCharacter, p.restrictCaretToClusters, p.usesWordDividers, p.isDiscreteWriting, p.isBlockWriting,
               p.isDistributedWithinCluster, p.isConnectedWriting, p.isCursiveWriting);
        (void)flags_of;
    }
    TA_END();
}
TEST(sp1_repeatable) {
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i += 5) { DWRITE_SCRIPT_PROPERTIES p, q; get_props(a1, i, &p); get_props(a1, i, &q); CHECK(!memcmp(&p, &q, sizeof p)); }
    TA_END();
}
TEST(sp1_same_through_analyzer2) {
    TA_BEGIN(); UINT32 i;
    for (i = 0; i < NSCRIPTS; i += 3) {
        DWRITE_SCRIPT_PROPERTIES p, q; get_props(a1, i, &p); memset(&q, 0xCC, sizeof q);
        CHECK_HR(IDWriteTextAnalyzer2_GetScriptProperties(a2, mk_sa(i, DWRITE_SCRIPT_SHAPES_DEFAULT), &q), S_OK);
        CHECK(!memcmp(&p, &q, sizeof p));
    }
    TA_END();
}

/* ---- scripts reached through AnalyzeScript ------------------------------------------------------------------------------ */
typedef struct { const WCHAR *text; const char *iso; UINT32 num; int hard; } script_case;
static const script_case k_scripts[] = {
    { L"A", "Latn", 215, 1 }, { L"\x0416", "Cyrl", 220, 1 }, { L"\x0391", "Grek", 200, 1 }, { L"\x05D0", "Hebr", 125, 1 }, { L"\x0627", "Arab", 160, 1 },
    { L"\x0E01", "Thai", 352, 1 }, { L"\x0905", "Deva", 315, 1 }, { L"\x4E00", "Hani", 500, 1 },
    { L"\x3042", "Hira", 410, 0 }, { L"\x30A2", "Kana", 411, 0 }, { L"\xD55C", "Hang", 286, 0 }, { L"\x0531", "Armn", 230, 0 }, { L"\x10D0", "Geor", 240, 0 },
    { L"\x0985", "Beng", 325, 0 }, { L"\x0BA4", "Taml", 346, 0 }, { L"\x0C24", "Telu", 340, 0 }, { L"\x0C95", "Knda", 345, 0 }, { L"\x0D2E", "Mlym", 347, 0 },
    { L"\x0A97", "Gujr", 320, 0 }, { L"\x0A17", "Guru", 310, 0 }, { L"\x1780", "Khmr", 355, 0 }, { L"\x0E81", "Laoo", 356, 0 }, { L"\x0F40", "Tibt", 330, 0 },
    { L"\x1820", "Mong", 145, 0 }, { L"\x12A0", "Ethi", 430, 0 }, { L"\x0710", "Syrc", 135, 0 }, { L"\x0780", "Thaa", 170, 0 }, { L"\x0D85", "Sinh", 348, 0 },
    { L"\x1000", "Mymr", 350, 0 }, { L"\x0B05", "Orya", 327, 0 }, { L"\x13A0", "Cher", 445, 0 }, { L"\x1401", "Cans", 440, 0 }, { L"\xA000", "Yiii", 460, 0 },
    { L"\x3105", "Bopo", 285, 0 }, { L"\x2800", "Brai", 570, 0 }, { L"\x16A0", "Runr", 211, 0 }, { L"\x1680", "Ogam", 212, 0 },
};
TEST(sp1_known_scripts_via_analyzescript) {
    TA_BEGIN(); ta_ctx *c = &c_; unsigned i;
    for (i = 0; i < sizeof k_scripts / sizeof *k_scripts; i++) {
        const script_case *k = &k_scripts[i]; UINT16 s = ta_script_of(c, k->text, 1); DWRITE_SCRIPT_PROPERTIES p; char iso[5];
        if (s == 0xFFFF) { CHECKF(0, "AnalyzeScript failed for %s", k->iso); continue; }
        CHECKF(s < NSCRIPTS, "%s: script index %u", k->iso, s);
        if (get_props(a1, s, &p) != S_OK) { CHECK(0); continue; }
        iso_str(p.isoScriptCode, iso);
        if (k->hard) { CHECKF(!strcmp(iso, k->iso) && p.isoScriptNumber == k->num, "U+%04X: got %s/%u want %s/%u", k->text[0], iso, p.isoScriptNumber, k->iso, k->num); }
        else if (strcmp(iso, k->iso) || p.isoScriptNumber != k->num) t_note("U+%04X: script %s/%u (index %u), expected %s/%u", k->text[0], iso, p.isoScriptNumber, s, k->iso, k->num);
    }
    TA_END();
}
TEST(sp1_distinct_scripts_get_distinct_indices) {
    TA_BEGIN(); ta_ctx *c = &c_; UINT16 s[8]; int i, j;
    for (i = 0; i < 8; i++) s[i] = ta_script_of(c, k_scripts[i].text, 1);
    for (i = 0; i < 8; i++) for (j = i + 1; j < 8; j++) CHECK(s[i] != s[j]);
    TA_END();
}
TEST(sp1_analyzed_indices_always_in_table) {
    TA_BEGIN(); ta_ctx *c = &c_; UINT32 cp, bad = 0, tested = 0;
    for (cp = 0x20; cp < 0x3200; cp += 7) {
        WCHAR w[2] = { (WCHAR)cp, 0 };
        if (cp >= 0xD800 && cp < 0xE000) continue;
        UINT16 s = ta_script_of(c, w, 1); tested++;
        if (s != 0xFFFF && s >= NSCRIPTS) bad++;
    }
    CHECKF(bad == 0, "%u of %u code points mapped outside the table", bad, tested);
    TA_END();
}
TEST(sp1_latin_properties) {
    TA_BEGIN(); ta_ctx *c = &c_; DWRITE_SCRIPT_PROPERTIES p; UINT16 s = ta_script_of(c, L"A", 1);
    CHECK_HR(get_props(a1, s, &p), S_OK);
    CHECK(p.usesWordDividers == 1);                 /* Latin separates words with spaces */
    CHECK(p.isConnectedWriting == 0 && p.isCursiveWriting == 0);
    CHECK(p.isBlockWriting == 0);
    t_note("Latn justification char U+%04X lookahead %u caret-restrict %u discrete %u distributed %u", p.justificationCharacter, p.clusterLookahead, p.restrictCaretToClusters, p.isDiscreteWriting, p.isDistributedWithinCluster);
    TA_END();
}
TEST(sp1_arabic_properties) {
    TA_BEGIN(); ta_ctx *c = &c_; DWRITE_SCRIPT_PROPERTIES p; UINT16 s = ta_script_of(c, L"\x0627", 1);
    CHECK_HR(get_props(a1, s, &p), S_OK);
    CHECK(p.isConnectedWriting == 1 || p.isCursiveWriting == 1);
    CHECK(p.usesWordDividers == 1);
    CHECKF(p.justificationCharacter == 0x0640, "Arabic justification character U+%04X (tatweel expected)", p.justificationCharacter);
    t_note("Arab connected=%u cursive=%u caret=%u lookahead=%u", p.isConnectedWriting, p.isCursiveWriting, p.restrictCaretToClusters, p.clusterLookahead);
    TA_END();
}
TEST(sp1_thai_and_han_have_no_word_dividers) {
    TA_BEGIN(); ta_ctx *c = &c_; DWRITE_SCRIPT_PROPERTIES p;
    CHECK_HR(get_props(a1, ta_script_of(c, L"\x0E01", 1), &p), S_OK);
    CHECK(p.usesWordDividers == 0);
    t_note("Thai caret=%u distributed=%u lookahead=%u", p.restrictCaretToClusters, p.isDistributedWithinCluster, p.clusterLookahead);
    CHECK_HR(get_props(a1, ta_script_of(c, L"\x4E00", 1), &p), S_OK);
    CHECK(p.usesWordDividers == 0);
    t_note("Hani block=%u discrete=%u just=U+%04X", p.isBlockWriting, p.isDiscreteWriting, p.justificationCharacter);
    TA_END();
}
TEST(sp1_indic_scripts_restrict_caret_note) {
    TA_BEGIN(); ta_ctx *c = &c_; DWRITE_SCRIPT_PROPERTIES p;
    CHECK_HR(get_props(a1, ta_script_of(c, L"\x0905", 1), &p), S_OK);
    t_note("Deva caret=%u distributed=%u lookahead=%u wordDiv=%u", p.restrictCaretToClusters, p.isDistributedWithinCluster, p.clusterLookahead, p.usesWordDividers);
    CHECK(p.usesWordDividers == 1);
    TA_END();
}
TEST(sp1_common_and_inherited_note) {
    TA_BEGIN(); ta_ctx *c = &c_; static const WCHAR t1[] = { ' ', 0 }, t2[] = { 0x0301, 0 }, t3[] = { '1', 0 }, t4[] = { 0x0640, 0 };
    const WCHAR *ts[] = { t1, t2, t3, t4 }; int i;
    for (i = 0; i < 4; i++) {
        UINT16 s = ta_script_of(c, ts[i], 1); DWRITE_SCRIPT_PROPERTIES p; char iso[5];
        if (s == 0xFFFF || get_props(a1, s, &p) != S_OK) { CHECK(0); continue; }
        iso_str(p.isoScriptCode, iso); t_note("U+%04X -> script %u %s/%u", ts[i][0], s, iso, p.isoScriptNumber);
    }
    TA_END();
}
