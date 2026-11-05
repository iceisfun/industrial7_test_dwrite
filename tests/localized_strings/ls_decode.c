/* Name record decoding: how 'name' table bytes become IDWriteLocalizedStrings entries.
 *
 * dwrite.dll (OpenTypeName::TryGetName / TryGetLCID, ParseMacRomanName, FontNameList::AppendItems):
 *  - (3,*) records: language = LCID of the record, converted with LCIDToLocaleName(LOCALE_ALLOW_NEUTRAL_NAMES);
 *    text is UTF-16BE copied unit by unit.
 *  - (1,0) records: only Macintosh language 0 (English) is understood and maps to en-us; bytes < 0x80 map to
 *    themselves, bytes >= 0x80 through the Mac OS Roman table.
 *  - one entry per language, entries sorted by language tag.
 * All strings are read through IDWriteFontFace3::GetInformationalStrings(DESCRIPTION) (name id 10) so the font's
 * family grouping is not involved.
 */
#include "lsfx.h"

static HRESULT get_info(ls_fx *x, DWRITE_INFORMATIONAL_STRING_ID id, IDWriteLocalizedStrings **s, BOOL *ex) {
    return IDWriteFontFace3_GetInformationalStrings(x->face3, id, s, ex);
}
#define DESC DWRITE_INFORMATIONAL_STRING_DESCRIPTION

/* Open a face with the given records (plus the base family names) and fetch DESCRIPTION. Returns 0 on prerequisite failure. */
static IDWriteLocalizedStrings *open_desc(ls_fx *x, IDWriteFactory *f, const ls_rec *r, int n, BOOL *exists) {
    IDWriteLocalizedStrings *s = NULL; HRESULT hr;
    *exists = 77;
    hr = ls_open_face(x, f, 1, r, n);
    if (FAILED(hr) || !x->face3) { t_skip("face fixture: %s", dw_hr_name(hr)); return NULL; }
    hr = get_info(x, DESC, &s, exists);
    CHECK_HR_OK(hr);
    if (FAILED(hr)) return NULL;
    return s;
}

/* The single en-us entry must read back exactly `want` (len units, may hold NULs). */
static void expect_en_desc(const ls_rec *r, int n, const WCHAR *want, UINT32 len) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex;
    s = open_desc(&x, f, r, n, &ex);
    if (s) {
        UINT32 sl = 0xAAAA; int i; WCHAR *got;
        CHECK(ex == TRUE); CHECK(s != NULL);
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 1);
        i = ls_index_of(s, L"en-us"); CHECK(i == 0);
        CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, 0, &sl));
        EXPECT_EQ(sl, len);
        got = (WCHAR *)calloc(sl + 4, sizeof(WCHAR));
        CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, got, sl + 1));
        CHECKF(!memcmp(got, want, len * sizeof(WCHAR)), "string content differs (%u units)", len);
        free(got);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
#define DEC_W(name, ws) TEST(lsd_utf16_##name) { static const WCHAR e[] = ws; const ls_rec r[] = { RW(3, 1, 0x409, 10, ws) }; \
    expect_en_desc(r, 1, e, (UINT32)(sizeof e / sizeof(WCHAR)) - 1); }

DEC_W(ascii, L"Hello World")
DEC_W(single_char, L"x")
DEC_W(latin1, L"café üß ñ")
DEC_W(cjk, L"日本語のテスト")
DEC_W(cyrillic, L"Привет")
DEC_W(greek, L"Γειά σου")
DEC_W(arabic, L"مرحبا")
DEC_W(hebrew, L"שלום")
DEC_W(surrogate_pair_emoji, L"\xD83D\xDE00")
DEC_W(surrogate_pair_first_astral, L"\xD800\xDC00")
DEC_W(surrogate_pair_last_astral, L"\xDBFF\xDFFF")
DEC_W(surrogate_pair_embedded, L"a\xD83D\xDE00" L"b")
DEC_W(surrogate_pairs_adjacent, L"\xD83D\xDE00\xD83D\xDE01\xD834\xDD1E")
DEC_W(bom_prefix, L"\xFEFF" L"text")
DEC_W(replacement_char, L"\xFFFD")
DEC_W(noncharacter_ffff, L"\xFFFF")
DEC_W(private_use, L"\xE000\xF8FF")
DEC_W(combining_mark, L"é")
DEC_W(leading_trailing_spaces, L"  padded  ")
DEC_W(control_chars, L"a\tb\nc\rd")
DEC_W(bmp_high_range, L"\xFB01\xFF21")
DEC_W(punctuation, L"–—“”…©®™")

/* Lone surrogates have no valid UTF-16 reading; the decoder copies 16-bit units, so the unit values are expected to survive. */
static void lone_surrogate_note(const WCHAR *ws, UINT32 n, const char *what) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex; ls_rec r;
    r.plat = 3; r.enc = 1; r.lang = 0x409; r.id = 10; r.kind = 1; r.p = ws; r.len = n * sizeof(WCHAR);
    s = open_desc(&x, f, &r, 1, &ex);
    if (s) {
        WCHAR *got = ls_dup_string(s, 0);
        CHECK(IDWriteLocalizedStrings_GetCount(s) == 1 || ex == FALSE);
        if (got) {
            t_note("%s: length %d, units %04x %04x", what, lstrlenW(got), got[0], got[0] ? got[1] : 0);
            CHECK(lstrlenW(got) > 0);
        }
        free(got);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_utf16_lone_high_surrogate) { static const WCHAR w[] = { 'a', 0xD800, 'b' }; lone_surrogate_note(w, 3, "lone high"); }
TEST(lsd_utf16_lone_low_surrogate)  { static const WCHAR w[] = { 'a', 0xDC00, 'b' }; lone_surrogate_note(w, 3, "lone low"); }
TEST(lsd_utf16_reversed_pair)       { static const WCHAR w[] = { 0xDE00, 0xD83D }; lone_surrogate_note(w, 2, "reversed pair"); }

/* ---- long names ------------------------------------------------------------------------------------------- */
static void long_name(UINT32 chars) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex; WCHAR *w = (WCHAR *)malloc((chars + 1) * sizeof(WCHAR)); UINT32 i; ls_rec r;
    for (i = 0; i < chars; i++) w[i] = (WCHAR)(L'A' + (i * 7) % 26 + ((i / 26) % 2) * 32);
    w[chars] = 0;
    r.plat = 3; r.enc = 1; r.lang = 0x409; r.id = 10; r.kind = 1; r.p = w; r.len = chars * sizeof(WCHAR);
    s = open_desc(&x, f, &r, 1, &ex);
    if (s) {
        UINT32 len = 0; WCHAR *got;
        CHECK(ex == TRUE);
        CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, 0, &len));
        EXPECT_EQ(len, chars);
        got = (WCHAR *)calloc(len + 4, sizeof(WCHAR));
        CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, got, len + 1));
        CHECK(!memcmp(got, w, chars * sizeof(WCHAR)));
        /* the truncation protocol holds at any length */
        CHECK_HR(IDWriteLocalizedStrings_GetString(s, 0, got, len), DW_E_NOT_SUFFICIENT_BUFFER);
        EXPECT_EQ((UINT32)lstrlenW(got), len - 1);
        free(got);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x); free(w);
}
TEST(lsd_long_254) { long_name(254); }
TEST(lsd_long_255) { long_name(255); }
TEST(lsd_long_256) { long_name(256); }
TEST(lsd_long_300) { long_name(300); }
TEST(lsd_long_511) { long_name(511); }
TEST(lsd_long_1000) { long_name(1000); }
TEST(lsd_long_4096) { long_name(4096); }
TEST(lsd_long_20000) { long_name(20000); }
TEST(lsd_long_32000) { long_name(32000); }

/* ---- Mac Roman -------------------------------------------------------------------------------------------- */
static const unsigned short mac_hi[128] = {
    0x00C4,0x00C5,0x00C7,0x00C9,0x00D1,0x00D6,0x00DC,0x00E1,0x00E0,0x00E2,0x00E4,0x00E3,0x00E5,0x00E7,0x00E9,0x00E8,
    0x00EA,0x00EB,0x00ED,0x00EC,0x00EE,0x00EF,0x00F1,0x00F3,0x00F2,0x00F4,0x00F6,0x00F5,0x00FA,0x00F9,0x00FB,0x00FC,
    0x2020,0x00B0,0x00A2,0x00A3,0x00A7,0x2022,0x00B6,0x00DF,0x00AE,0x00A9,0x2122,0x00B4,0x00A8,0x2260,0x00C6,0x00D8,
    0x221E,0x00B1,0x2264,0x2265,0x00A5,0x00B5,0x2202,0x2211,0x220F,0x03C0,0x222B,0x00AA,0x00BA,0x03A9,0x00E6,0x00F8,
    0x00BF,0x00A1,0x00AC,0x221A,0x0192,0x2248,0x2206,0x00AB,0x00BB,0x2026,0x00A0,0x00C0,0x00C3,0x00D5,0x0152,0x0153,
    0x2013,0x2014,0x201C,0x201D,0x2018,0x2019,0x00F7,0x25CA,0x00FF,0x0178,0x2044,0x20AC,0x2039,0x203A,0xFB01,0xFB02,
    0x2021,0x00B7,0x201A,0x201E,0x2030,0x00C2,0x00CA,0x00C1,0x00CB,0x00C8,0x00CD,0x00CE,0x00CF,0x00CC,0x00D3,0x00D4,
    0xF8FF,0x00D2,0x00DA,0x00DB,0x00D9,0x0131,0x02C6,0x02DC,0x00AF,0x02D8,0x02D9,0x02DA,0x00B8,0x02DD,0x02DB,0x02C7,
};
/* 0xDB is the currency sign / euro sign and 0xF0 the Apple logo depending on the table revision: not hard checked. */
static void mac_range(int lo, int hi) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex; unsigned char bytes[130]; ls_rec r; int n = 0, b;
    for (b = lo; b <= hi; b++) bytes[n++] = (unsigned char)b;
    r.plat = 1; r.enc = 0; r.lang = 0; r.id = 10; r.kind = 0; r.p = bytes; r.len = n;
    s = open_desc(&x, f, &r, 1, &ex);
    if (s) {
        WCHAR *got; int i;
        CHECK(ex == TRUE);
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 1);
        CHECK(ls_index_of(s, L"en-us") == 0);       /* Macintosh language 0 is English */
        got = ls_dup_string(s, 0);
        CHECK(got && lstrlenW(got) == n);
        if (got && lstrlenW(got) == n)
            for (i = 0; i < n; i++) {
                int bb = lo + i; unsigned want = bb < 0x80 ? (unsigned)bb : mac_hi[bb - 0x80];
                if (bb == 0xDB || bb == 0xF0) { t_note("mac byte 0x%02X -> U+%04X", bb, got[i]); continue; }
                /* characters with two accepted Unicode equivalents across table revisions */
                if ((bb == 0xBD && got[i] == 0x2126) || (bb == 0xC6 && got[i] == 0x0394) || (bb == 0xB5 && got[i] == 0x03BC)) { t_note("mac byte 0x%02X -> alternate U+%04X", bb, got[i]); continue; }
                CHECKF(got[i] == want, "mac byte 0x%02X -> U+%04X want U+%04X", bb, got[i], want);
            }
        free(got);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_mac_ascii_printable) { mac_range(0x20, 0x7E); }
TEST(lsd_mac_80_9F) { mac_range(0x80, 0x9F); }
TEST(lsd_mac_A0_BF) { mac_range(0xA0, 0xBF); }
TEST(lsd_mac_C0_DF) { mac_range(0xC0, 0xDF); }
TEST(lsd_mac_E0_FF) { mac_range(0xE0, 0xFF); }
TEST(lsd_mac_control_bytes) { mac_range(0x01, 0x1F); }
TEST(lsd_mac_full_high_half) { mac_range(0x80, 0xFF); }
TEST(lsd_mac_single_chars) {
    /* each high byte alone */
    int b;
    for (b = 0x80; b <= 0xFF; b += 0x11) mac_range(b, b);
}
TEST(lsd_mac_other_language_ids) {
    /* Mac language 1 (French), 2 (German), 11 (Japanese): the converter only understands language 0 as en-us; whatever
     * the other ids do, they must not disturb the English entry nor produce a failure. */
    LS_NEED_FACTORY(); ls_fx x; static const unsigned short langs[] = { 1, 2, 3, 11, 12, 19, 33 }; size_t k;
    for (k = 0; k < sizeof langs / sizeof langs[0]; k++) {
        ls_rec r[2] = { RB(1, 0, 0, 10, "english text"), RB(1, 0, 0, 10, "other") };
        IDWriteLocalizedStrings *s; BOOL ex; WCHAR *en;
        r[1].lang = langs[k];
        s = open_desc(&x, f, r, 2, &ex);
        if (s) {
            CHECK(ex == TRUE);
            en = ls_en(s);
            CHECK(en && !lstrcmpW(en, L"english text"));
            t_note("mac language %u: %u entries", langs[k], IDWriteLocalizedStrings_GetCount(s));
            free(en);
            IDWriteLocalizedStrings_Release(s);
        }
        ls_close(&x);
    }
}

/* ---- platform 3 LCIDs ------------------------------------------------------------------------------------------ */
/* The language of a (3,*) record is LCIDToLocaleName(lcid, ..., LOCALE_ALLOW_NEUTRAL_NAMES); this reads the same OS table. */
static void lcid_case(unsigned lcid) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex; WCHAR exp[LOCALE_NAME_MAX_LENGTH];
    ls_rec r[2] = { RW(3, 1, 0x409, 10, L"english"), RW(3, 1, 0, 10, L"localized") };
    r[1].lang = (unsigned short)lcid;
    if (!LCIDToLocaleName(lcid, exp, LOCALE_NAME_MAX_LENGTH, LOCALE_ALLOW_NEUTRAL_NAMES)) { t_skip("LCID %#x unknown to the OS", lcid); return; }
    s = open_desc(&x, f, r, 2, &ex);
    if (s) {
        int i = ls_index_of(s, exp);
        CHECK(ex == TRUE);
        CHECKF(i >= 0, "locale %ls (lcid %#x) not found", exp, lcid);
        if (i >= 0) {
            WCHAR *loc = ls_dup_locale(s, (UINT32)i), *str = ls_dup_string(s, (UINT32)i);
            CHECK(loc && ls_wcsieq(loc, exp));
            if (loc && lstrcmpW(loc, exp)) t_note("lcid %#x: OS spelling %ls, reported %ls", lcid, exp, loc);
            if (lcid != 0x409) { CHECK(str && !lstrcmpW(str, L"localized")); EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 2); }
            free(loc); free(str);
        }
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
#define LCID_T(n, v) TEST(lsd_lcid_##n) { lcid_case(v); }
LCID_T(0409_en_us, 0x409) LCID_T(0809_en_gb, 0x809) LCID_T(0c09_en_au, 0xC09) LCID_T(1009_en_ca, 0x1009)
LCID_T(0009_en_neutral, 0x9) LCID_T(0407_de_de, 0x407) LCID_T(0807_de_ch, 0x807) LCID_T(040c_fr_fr, 0x40C)
LCID_T(0c0c_fr_ca, 0xC0C) LCID_T(0410_it_it, 0x410) LCID_T(0416_pt_br, 0x416) LCID_T(0816_pt_pt, 0x816)
LCID_T(0411_ja_jp, 0x411) LCID_T(0412_ko_kr, 0x412) LCID_T(0804_zh_cn, 0x804) LCID_T(0404_zh_tw, 0x404)
LCID_T(0419_ru_ru, 0x419) LCID_T(0415_pl_pl, 0x415) LCID_T(040d_he_il, 0x40D) LCID_T(0401_ar_sa, 0x401)
LCID_T(041e_th_th, 0x41E) LCID_T(042a_vi_vn, 0x42A) LCID_T(041d_sv_se, 0x41D) LCID_T(0413_nl_nl, 0x413)
LCID_T(0408_el_gr, 0x408) LCID_T(041f_tr_tr, 0x41F) LCID_T(0405_cs_cz, 0x405) LCID_T(040e_hu_hu, 0x40E)
LCID_T(0439_hi_in, 0x439) LCID_T(0c0a_es_es, 0xC0A) LCID_T(080a_es_mx, 0x80A)

TEST(lsd_lcid_unknown_values) {
    /* LCIDs the OS cannot name: the record is ignored or kept under some tag; the English entry survives either way. */
    LS_NEED_FACTORY(); ls_fx x; static const unsigned short l[] = { 0x0000, 0x1234, 0x7FFF, 0xFFFF, 0x0400, 0x0800, 0x8000 }; size_t k;
    for (k = 0; k < sizeof l / sizeof l[0]; k++) {
        ls_rec r[2] = { RW(3, 1, 0x409, 10, L"english"), RW(3, 1, 0, 10, L"odd") }; IDWriteLocalizedStrings *s; BOOL ex; WCHAR *en;
        r[1].lang = l[k];
        s = open_desc(&x, f, r, 2, &ex);
        if (s) {
            en = ls_en(s);   /* an LCID the OS maps to en-US (e.g. 0x400 user default) may legitimately replace the entry */
            CHECK(en && (!lstrcmpW(en, L"english") || !lstrcmpW(en, L"odd")));
            t_note("lcid %#x: %u entries", l[k], IDWriteLocalizedStrings_GetCount(s));
            CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1);
            CHECK(ls_sweep(s, "odd lcid") == 0);
            free(en); IDWriteLocalizedStrings_Release(s);
        }
        ls_close(&x);
    }
}

/* ---- other platforms / encodings ------------------------------------------------------------------------------------ */
static void other_enc(unsigned plat, unsigned enc, unsigned lang, const void *bytes, size_t len, const WCHAR *want, const char *what) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex;
    ls_rec r; r.plat = plat; r.enc = enc; r.lang = lang; r.id = 10; r.kind = 0; r.p = bytes; r.len = len;
    s = open_desc(&x, f, &r, 1, &ex);
    if (s) {
        UINT32 n = IDWriteLocalizedStrings_GetCount(s), i; int found = 0;
        for (i = 0; i < n; i++) {
            WCHAR *str = ls_dup_string(s, i), *loc = ls_dup_locale(s, i);
            if (str && !lstrcmpW(str, want)) { found = 1; t_note("%s: decoded under locale %ls", what, loc); }
            free(str); free(loc);
        }
        if (!found) t_note("%s: not exposed (entries=%u, exists=%d)", what, n, ex);
        CHECK(ls_sweep(s, what) == 0);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_enc_platform0_unicode) { static const unsigned char b[] = { 0, 'U', 0, 'n', 0, 'i' }; other_enc(0, 3, 0, b, 6, L"Uni", "platform 0 enc 3"); }
TEST(lsd_enc_platform0_enc4) { static const unsigned char b[] = { 0, 'U', 0, '4' }; other_enc(0, 4, 0, b, 4, L"U4", "platform 0 enc 4"); }
TEST(lsd_enc_platform3_symbol) { static const unsigned char b[] = { 0, 'S', 0, 'y' }; other_enc(3, 0, 0x409, b, 4, L"Sy", "platform 3 enc 0 symbol"); }
TEST(lsd_enc_platform3_ucs4) { static const unsigned char b[] = { 0xD8, 0x3D, 0xDE, 0x00 }; other_enc(3, 10, 0x409, b, 4, L"\xD83D\xDE00", "platform 3 enc 10"); }
TEST(lsd_enc_platform3_gbk) { static const unsigned char b[] = { 0xD6, 0xD0, 0xCE, 0xC4 }; other_enc(3, 3, 0x804, b, 4, L"中文", "platform 3 enc 3 (PRC)"); }
TEST(lsd_enc_platform3_big5) { static const unsigned char b[] = { 0xA4, 0xA4, 0xA4, 0xE5 }; other_enc(3, 4, 0x404, b, 4, L"中文", "platform 3 enc 4 (Big5)"); }
TEST(lsd_enc_platform3_wansung) { static const unsigned char b[] = { 0xC7, 0xD1, 0xB1, 0xDB }; other_enc(3, 5, 0x412, b, 4, L"한글", "platform 3 enc 5 (Wansung)"); }
TEST(lsd_enc_platform3_shiftjis) { static const unsigned char b[] = { 0x93, 0xFA, 0x96, 0x7B }; other_enc(3, 2, 0x411, b, 4, L"日本", "platform 3 enc 2 (ShiftJIS)"); }
TEST(lsd_enc_platform2_iso) { static const unsigned char b[] = { 'I', 'S', 'O' }; other_enc(2, 0, 0, b, 3, L"ISO", "platform 2 enc 0"); }
TEST(lsd_enc_platform4_custom) { static const unsigned char b[] = { 'C', 'U' }; other_enc(4, 0, 0, b, 2, L"CU", "platform 4"); }
TEST(lsd_enc_mac_nonroman_script) { static const unsigned char b[] = { 0x93, 0xFA }; other_enc(1, 1, 11, b, 2, L"日", "platform 1 enc 1 (Japanese)"); }

/* ---- empty / NUL / duplicates / precedence ----------------------------------------------------------------------------------- */
TEST(lsd_empty_name_record) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex; const ls_rec r[] = { RB(3, 1, 0x409, 10, "") };
    s = open_desc(&x, f, r, 1, &ex);
    if (s) {
        UINT32 n = IDWriteLocalizedStrings_GetCount(s);
        t_note("empty description: exists=%d entries=%u", ex, n);
        if (n >= 1) { UINT32 len = 9; WCHAR b[4] = { 7, 7, 7, 7 }; CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, 0, &len)); EXPECT_EQ(len, 0);
                      CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, b, 1)); EXPECT_EQ(b[0], 0);
                      CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, b, 4)); EXPECT_EQ(b[0], 0); }
        CHECK(ls_sweep(s, "empty") == 0);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_empty_beside_nonempty_locale) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex;
    const ls_rec r[] = { RB(3, 1, 0x409, 10, ""), RW(3, 1, 0x407, 10, L"Deutsch") };
    s = open_desc(&x, f, r, 2, &ex);
    if (s) {
        int de = ls_index_of(s, L"de-de");
        CHECK(de >= 0);
        if (de >= 0) { WCHAR *d = ls_dup_string(s, (UINT32)de); CHECK(d && !lstrcmpW(d, L"Deutsch")); free(d); }
        t_note("entries with an empty sibling: %u", IDWriteLocalizedStrings_GetCount(s));
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_embedded_nul_utf16) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex; static const WCHAR w[] = { 'A', 0, 'B', 'C' };
    ls_rec r; r.plat = 3; r.enc = 1; r.lang = 0x409; r.id = 10; r.kind = 1; r.p = w; r.len = sizeof w;
    s = open_desc(&x, f, &r, 1, &ex);
    if (s) {
        UINT32 len = 0, n = IDWriteLocalizedStrings_GetCount(s); WCHAR b[16] = { 9 };
        CHECK(n >= 1);
        if (n >= 1) {
            CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, 0, &len));
            t_note("string with embedded NUL: GetStringLength=%u (4 = counts past the NUL)", len);
            CHECK(len == 1 || len == 4);
            /* GetString copies up to the first terminator */
            CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, b, 16));
            EXPECT_EQ(b[0], 'A'); EXPECT_EQ(b[1], 0);
        }
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_embedded_nul_mac) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex; const ls_rec r[] = { RB(1, 0, 0, 10, "A\0BC") };
    s = open_desc(&x, f, r, 1, &ex);
    if (s) {
        UINT32 len = 0, n = IDWriteLocalizedStrings_GetCount(s); WCHAR b[16] = { 9 };
        CHECK(n >= 1);
        if (n >= 1) {
            CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, 0, &len));
            t_note("mac string with embedded NUL: GetStringLength=%u", len);
            CHECK(len == 1 || len == 4);
            CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, 0, b, 16));
            EXPECT_EQ(b[0], 'A'); EXPECT_EQ(b[1], 0);
        }
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_odd_byte_length_utf16) {
    /* a record whose byte length is odd: the trailing byte cannot form a unit */
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex; static const unsigned char b[] = { 0, 'A', 0, 'B', 0 };
    ls_rec r; r.plat = 3; r.enc = 1; r.lang = 0x409; r.id = 10; r.kind = 0; r.p = b; r.len = 5;
    s = open_desc(&x, f, &r, 1, &ex);
    if (s) {
        WCHAR *got = IDWriteLocalizedStrings_GetCount(s) ? ls_dup_string(s, 0) : NULL;
        t_note("odd byte length: exists=%d string=%ls", ex, got ? got : L"(none)");
        if (got) CHECK(got[0] == 'A');
        free(got);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_duplicate_same_key) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex;
    const ls_rec r[] = { RW(3, 1, 0x409, 10, L"first"), RW(3, 1, 0x409, 10, L"second") };
    s = open_desc(&x, f, r, 2, &ex);
    if (s) {
        WCHAR *e;
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 1);          /* one entry per language */
        e = ls_en(s);
        CHECK(e && (!lstrcmpW(e, L"first") || !lstrcmpW(e, L"second")));
        t_note("duplicate (3,1,0x409,10) records: kept \"%ls\"", e ? e : L"?");
        free(e); IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_duplicate_mac_and_windows_english) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex;
    const ls_rec r[] = { RB(1, 0, 0, 10, "mac text"), RW(3, 1, 0x409, 10, L"win text") };
    s = open_desc(&x, f, r, 2, &ex);
    if (s) {
        WCHAR *e;
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 1);          /* both are en-us */
        e = ls_en(s);
        CHECK(e && (!lstrcmpW(e, L"mac text") || !lstrcmpW(e, L"win text")));
        t_note("mac vs windows english: kept \"%ls\"", e ? e : L"?");
        free(e); IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_same_language_different_text_encoding_pair) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex;
    const ls_rec r[] = { RW(3, 1, 0x409, 10, L"same"), RB(1, 0, 0, 10, "same") };
    s = open_desc(&x, f, r, 2, &ex);
    if (s) {
        WCHAR *e;
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 1);
        e = ls_en(s); CHECK(e && !lstrcmpW(e, L"same")); free(e);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_unicode_platform0_vs_windows_english) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s; BOOL ex;
    const ls_rec r[] = { RW(0, 3, 0, 10, L"uni text"), RW(3, 1, 0x409, 10, L"win text") };
    s = open_desc(&x, f, r, 2, &ex);
    if (s) {
        WCHAR *e = ls_en(s);
        CHECK(e && !lstrcmpW(e, L"win text"));       /* platform 0 has no language: it never replaces a windows entry */
        t_note("platform 0 + windows english: %u entries", IDWriteLocalizedStrings_GetCount(s));
        free(e); IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}
TEST(lsd_record_order_in_table_irrelevant) {
    /* The builder sorts records as the table requires; the entry order the API exposes must not depend on how the
     * caller listed the records. */
    LS_NEED_FACTORY(); ls_fx a, b; IDWriteLocalizedStrings *sa, *sb; BOOL ea, eb;
    const ls_rec r1[] = { RW(3, 1, 0x409, 10, L"e"), RW(3, 1, 0x407, 10, L"d"), RW(3, 1, 0x411, 10, L"j") };
    const ls_rec r2[] = { RW(3, 1, 0x411, 10, L"j"), RW(3, 1, 0x409, 10, L"e"), RW(3, 1, 0x407, 10, L"d") };
    sa = open_desc(&a, f, r1, 3, &ea);
    if (!sa) { ls_close(&a); return; }
    sb = open_desc(&b, f, r2, 3, &eb);
    if (sb) {
        UINT32 n = IDWriteLocalizedStrings_GetCount(sa), i;
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(sb), n);
        for (i = 0; i < n; i++) {
            WCHAR *l1 = ls_dup_locale(sa, i), *l2 = ls_dup_locale(sb, i), *s1 = ls_dup_string(sa, i), *s2 = ls_dup_string(sb, i);
            CHECK(l1 && l2 && !lstrcmpW(l1, l2)); CHECK(s1 && s2 && !lstrcmpW(s1, s2));
            free(l1); free(l2); free(s1); free(s2);
        }
        IDWriteLocalizedStrings_Release(sb);
    }
    IDWriteLocalizedStrings_Release(sa);
    ls_close(&a); ls_close(&b);
}
TEST(lsd_many_locales) {
    /* 40 distinct languages in one name id */
    LS_NEED_FACTORY(); ls_fx x; static const unsigned short lc[] = {
        0x401,0x402,0x403,0x404,0x405,0x406,0x407,0x408,0x409,0x40B,0x40C,0x40D,0x40E,0x410,0x411,0x412,0x413,0x414,0x415,0x416,
        0x418,0x419,0x41A,0x41B,0x41D,0x41E,0x41F,0x420,0x422,0x423,0x424,0x425,0x426,0x427,0x42A,0x804,0x809,0x807,0x80A,0xC0A };
    ls_rec r[40]; WCHAR txt[40][8]; IDWriteLocalizedStrings *s; BOOL ex; int i, found = 0;
    for (i = 0; i < 40; i++) { wsprintfW(txt[i], L"t%02d", i); r[i].plat = 3; r[i].enc = 1; r[i].lang = lc[i]; r[i].id = 10; r[i].kind = 1; r[i].p = txt[i]; r[i].len = lstrlenW(txt[i]) * 2; }
    s = open_desc(&x, f, r, 40, &ex);
    if (s) {
        UINT32 n = IDWriteLocalizedStrings_GetCount(s);
        t_note("40 name records -> %u entries", n);
        CHECK(n >= 30 && n <= 40);
        for (i = 0; i < 40; i++) {
            WCHAR loc[LOCALE_NAME_MAX_LENGTH]; int k;
            if (!LCIDToLocaleName(lc[i], loc, LOCALE_NAME_MAX_LENGTH, LOCALE_ALLOW_NEUTRAL_NAMES)) continue;
            k = ls_index_of(s, loc);
            if (k >= 0) { WCHAR *t = ls_dup_string(s, (UINT32)k); found++; CHECKF(t && !lstrcmpW(t, txt[i]), "locale %ls content", loc); free(t); }
        }
        CHECK(found >= 30);
        CHECK(ls_sweep(s, "many locales") == 0);
        IDWriteLocalizedStrings_Release(s);
    }
    ls_close(&x);
}

/* ---- informational string ids ------------------------------------------------------------------------------------------------ */
/* DWRITE_INFORMATIONAL_STRING_ID -> name table id; strings of the multi fixture */
static void info_case(DWRITE_INFORMATIONAL_STRING_ID id, const WCHAR *want_en, unsigned want_count) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s = NULL; BOOL ex = 77; HRESULT hr;
    HRESULT ho = ls_open_multi_face(&x, f);
    if (FAILED(ho) || !x.face3) { ls_close(&x); t_skip("fixture %s", dw_hr_name(ho)); return; }
    hr = get_info(&x, id, &s, &ex);
    CHECK_HR_OK(hr);
    if (want_en) {
        CHECK(ex == TRUE); CHECK(s != NULL);
        if (s) {
            WCHAR *e = ls_en(s);
            CHECK(e && !lstrcmpW(e, want_en));
            if (want_count) EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), want_count);
            CHECK(ls_sweep(s, "info") == 0);
            free(e);
        }
    } else {
        CHECK(ex == FALSE); CHECK(s == NULL);
    }
    DW_RELEASE(s);
    /* the IDWriteFont flavour (same strings) */
    ls_close(&x);
}
#define INFO_T(n, id, en, cnt) TEST(lsi_##n) { info_case(id, en, cnt); }
INFO_T(copyright, DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, L"Copyright (c) LsFam", 2)
INFO_T(version, DWRITE_INFORMATIONAL_STRING_VERSION_STRINGS, L"Version 1.000", 1)
INFO_T(trademark, DWRITE_INFORMATIONAL_STRING_TRADEMARK, L"LsFam is a trademark", 1)
INFO_T(manufacturer, DWRITE_INFORMATIONAL_STRING_MANUFACTURER, L"LsFam Foundry", 1)
INFO_T(designer, DWRITE_INFORMATIONAL_STRING_DESIGNER, L"Ls Designer", 1)
INFO_T(designer_url, DWRITE_INFORMATIONAL_STRING_DESIGNER_URL, L"http://example.com/designer", 1)
INFO_T(description, DWRITE_INFORMATIONAL_STRING_DESCRIPTION, L"A synthetic test font", 2)
INFO_T(vendor_url, DWRITE_INFORMATIONAL_STRING_FONT_VENDOR_URL, L"http://example.com/vendor", 1)
INFO_T(license_desc, DWRITE_INFORMATIONAL_STRING_LICENSE_DESCRIPTION, L"Free to use", 1)
INFO_T(license_url, DWRITE_INFORMATIONAL_STRING_LICENSE_INFO_URL, L"http://example.com/license", 1)
INFO_T(win32_family, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, L"LsFam", 5)
INFO_T(win32_subfamily, DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, L"Regular", 5)
INFO_T(sample_text, DWRITE_INFORMATIONAL_STRING_SAMPLE_TEXT, L"Sphinx of black quartz", 1)
INFO_T(full_name, DWRITE_INFORMATIONAL_STRING_FULL_NAME, L"LsFam", 2)
INFO_T(postscript_name, DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, L"LsFam", 1)

static void info_absent(DWRITE_INFORMATIONAL_STRING_ID id, const char *what) {
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = TRUE; HRESULT hr;
    HRESULT ho = ls_open_multi_face(&x, f);
    if (FAILED(ho) || !x.face3) { ls_close(&x); t_skip("fixture %s", dw_hr_name(ho)); return; }
    hr = get_info(&x, id, &s, &ex);
    CHECK_HR_OK(hr);
    t_note("%s (id %d): exists=%d", what, (int)id, ex);
    if (ex) { CHECK(s != NULL); if (s) { CHECK(IDWriteLocalizedStrings_GetCount(s) >= 1); CHECK(ls_sweep(s, what) == 0); } DW_RELEASE(s); }
    else CHECK(s == NULL);
    ls_close(&x);
}
TEST(lsi_typographic_family_absent_or_derived) { info_absent(DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES, "typographic family"); }
TEST(lsi_typographic_subfamily_absent_or_derived) { info_absent(DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES, "typographic subfamily"); }
TEST(lsi_postscript_cid_absent_or_derived) { info_absent(DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_CID_NAME, "postscript cid"); }
TEST(lsi_wws_family_absent_or_derived) { info_absent(DWRITE_INFORMATIONAL_STRING_WEIGHT_STRETCH_STYLE_FAMILY_NAME, "wws family"); }
TEST(lsi_design_script_language_absent_or_derived) { info_absent(DWRITE_INFORMATIONAL_STRING_DESIGN_SCRIPT_LANGUAGE_TAG, "design script language"); }
TEST(lsi_supported_script_language_absent_or_derived) { info_absent(DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG, "supported script language"); }

TEST(lsi_invalid_ids) {
    /* ids outside 1..21 (GetInformationalStrings): success, exists FALSE, strings NULL, on both interfaces */
    LS_NEED_FACTORY(); LS_NEED_MULTI(x);
    static const int bad[] = { 0, 22, 23, 100, 0xDEAD, 0x7FFFFFFF, -1, 0x80000000 };
    size_t k;
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)0xDEADBEEF; BOOL ex = TRUE;
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font, (DWRITE_INFORMATIONAL_STRING_ID)bad[k], &s, &ex));
        CHECK(ex == FALSE); CHECK_OUT_NULL(s);
        s = (IDWriteLocalizedStrings *)(ULONG_PTR)0xDEADBEEF; ex = TRUE;
        CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(x.cface3, (DWRITE_INFORMATIONAL_STRING_ID)bad[k], &s, &ex));
        CHECK(ex == FALSE); CHECK_OUT_NULL(s);
    }
    ls_close(&x);
}
TEST(lsi_out_params_initialised_on_miss) {
    LS_NEED_FACTORY(); LS_NEED_MULTI(x);
    IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)0x1234; BOOL ex = 0x55;
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font, DWRITE_INFORMATIONAL_STRING_NONE, &s, &ex));
    CHECK(ex == FALSE); CHECK_OUT_NULL(s);
    s = (IDWriteLocalizedStrings *)(ULONG_PTR)0x1234; ex = 0x55;
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &s, &ex));
    CHECK(ex == TRUE); CHECK(s != NULL); DW_RELEASE(s);
    ls_close(&x);
}
TEST(lsi_font_and_face_agree_for_every_id) {
    LS_NEED_FACTORY(); LS_NEED_MULTI(x); int id;
    for (id = 0; id <= 22; id++) {
        IDWriteLocalizedStrings *a = NULL, *b = NULL; BOOL ea = 77, eb = 77;
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font, (DWRITE_INFORMATIONAL_STRING_ID)id, &a, &ea));
        CHECK_HR_OK(IDWriteFontFace3_GetInformationalStrings(x.cface3, (DWRITE_INFORMATIONAL_STRING_ID)id, &b, &eb));
        EXPECT_EQ(ea, eb);
        if (ea && eb && a && b) {
            UINT32 n = IDWriteLocalizedStrings_GetCount(a), i;
            EXPECT_EQ(IDWriteLocalizedStrings_GetCount(b), n);
            for (i = 0; i < n; i++) {
                WCHAR *l1 = ls_dup_locale(a, i), *l2 = ls_dup_locale(b, i), *s1 = ls_dup_string(a, i), *s2 = ls_dup_string(b, i);
                CHECKF(l1 && l2 && !lstrcmpW(l1, l2) && s1 && s2 && !lstrcmpW(s1, s2), "id %d entry %u differs", id, i);
                free(l1); free(l2); free(s1); free(s2);
            }
        }
        DW_RELEASE(a); DW_RELEASE(b);
    }
    ls_close(&x);
}
TEST(lsi_exists_false_when_name_missing) {
    /* a font without a description / copyright: exists FALSE, even though other ids exist */
    LS_NEED_FACTORY(); ls_fx x; IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = TRUE;
    HRESULT hr = ls_open_face(&x, f, 1, NULL, 0);
    if (FAILED(hr) || !x.face3) { ls_close(&x); t_skip("fixture"); return; }
    CHECK_HR_OK(get_info(&x, DWRITE_INFORMATIONAL_STRING_DESCRIPTION, &s, &ex)); CHECK(ex == FALSE); CHECK(s == NULL);
    s = (IDWriteLocalizedStrings *)(ULONG_PTR)1; ex = TRUE;
    CHECK_HR_OK(get_info(&x, DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, &s, &ex)); CHECK(ex == FALSE); CHECK(s == NULL);
    s = NULL; ex = FALSE;
    CHECK_HR_OK(get_info(&x, DWRITE_INFORMATIONAL_STRING_FULL_NAME, &s, &ex)); CHECK(ex == TRUE); CHECK(s != NULL); DW_RELEASE(s);
    ls_close(&x);
}

/* ---- family / face name selection ------------------------------------------------------------------------------------------------ */
static const ls_rec wws_recs[] = {
    RW(3, 1, 0x409, 1, L"LsFam Bold"), RW(3, 1, 0x409, 2, L"Regular"), RW(3, 1, 0x409, 4, L"LsFam Bold"), RW(3, 1, 0x409, 6, L"LsFam-Bold"),
    RW(3, 1, 0x409, 16, L"LsFam"), RW(3, 1, 0x409, 17, L"Bold"),
    RW(3, 1, 0x407, 16, L"LsFam DE"), RW(3, 1, 0x407, 17, L"Fett"),
};
TEST(lsn_typographic_names_present_info_strings) {
    LS_NEED_FACTORY(); ls_fx x; HRESULT hr = ls_open_face(&x, f, 0, wws_recs, 8); IDWriteLocalizedStrings *s; BOOL ex; WCHAR *e;
    if (FAILED(hr) || !x.face3) { ls_close(&x); t_skip("fixture"); return; }
    s = NULL; CHECK_HR_OK(get_info(&x, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &s, &ex)); CHECK(ex);
    if (s) { e = ls_en(s); CHECK(e && !lstrcmpW(e, L"LsFam Bold")); free(e); EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 1); DW_RELEASE(s); }
    s = NULL; CHECK_HR_OK(get_info(&x, DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES, &s, &ex)); CHECK(ex);
    if (s) { e = ls_en(s); CHECK(e && !lstrcmpW(e, L"LsFam")); free(e); EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 2);
             e = NULL; { int i = ls_index_of(s, L"de-de"); if (i >= 0) e = ls_dup_string(s, i); } CHECK(e && !lstrcmpW(e, L"LsFam DE")); free(e); DW_RELEASE(s); }
    s = NULL; CHECK_HR_OK(get_info(&x, DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES, &s, &ex)); CHECK(ex);
    if (s) { e = ls_en(s); CHECK(e && !lstrcmpW(e, L"Bold")); free(e); DW_RELEASE(s); }
    ls_close(&x);
}
TEST(lsn_face6_typographic_model) {
    LS_NEED_FACTORY(); ls_fx x; HRESULT hr = ls_open_face(&x, f, 0, wws_recs, 8); IDWriteFontFace6 *f6 = NULL; IDWriteLocalizedStrings *s; WCHAR *e;
    if (FAILED(hr) || !x.face3) { ls_close(&x); t_skip("fixture"); return; }
    if (FAILED(IDWriteFontFace_QueryInterface(x.face, &IID_IDWriteFontFace6, (void **)&f6)) || !f6) { ls_close(&x); t_skip("IDWriteFontFace6 unsupported"); return; }
    s = NULL; CHECK_HR_OK(IDWriteFontFace6_GetFamilyNames(f6, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &s));
    if (s) { e = ls_en(s); CHECK(e && !lstrcmpW(e, L"LsFam")); free(e); DW_RELEASE(s); }
    s = NULL; CHECK_HR_OK(IDWriteFontFace6_GetFaceNames(f6, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &s));
    if (s) { e = ls_en(s); CHECK(e && !lstrcmpW(e, L"Bold")); free(e); DW_RELEASE(s); }
    s = NULL; CHECK_HR_OK(IDWriteFontFace6_GetFamilyNames(f6, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &s));
    if (s) { e = ls_en(s); t_note("WWS family name: %ls", e ? e : L"?"); CHECK(e != NULL); free(e); DW_RELEASE(s); }
    s = NULL; CHECK_HR_OK(IDWriteFontFace6_GetFaceNames(f6, DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &s));
    if (s) { e = ls_en(s); t_note("WWS face name: %ls", e ? e : L"?"); CHECK(e != NULL); free(e); DW_RELEASE(s); }
    IDWriteFontFace6_Release(f6);
    ls_close(&x);
}
TEST(lsn_face6_invalid_model) {
    LS_NEED_FACTORY(); LS_NEED_MULTI(x); IDWriteFontFace6 *f6 = NULL; size_t k; static const int bad[] = { 2, 3, 100, -1, 0x7FFFFFFF };
    if (FAILED(IDWriteFontFace_QueryInterface(x.cface, &IID_IDWriteFontFace6, (void **)&f6)) || !f6) { ls_close(&x); t_skip("IDWriteFontFace6 unsupported"); return; }
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)0x1;
        CHECK_HR(IDWriteFontFace6_GetFamilyNames(f6, (DWRITE_FONT_FAMILY_MODEL)bad[k], &s), DW_E_INVALIDARG); CHECK_OUT_NULL(s);
        s = (IDWriteLocalizedStrings *)(ULONG_PTR)0x1;
        CHECK_HR(IDWriteFontFace6_GetFaceNames(f6, (DWRITE_FONT_FAMILY_MODEL)bad[k], &s), DW_E_INVALIDARG); CHECK_OUT_NULL(s);
    }
    IDWriteFontFace6_Release(f6);
    ls_close(&x);
}
TEST(lsn_face3_family_is_one_of_known_names) {
    LS_NEED_FACTORY(); ls_fx x; HRESULT hr = ls_open_face(&x, f, 0, wws_recs, 8); IDWriteLocalizedStrings *s = NULL; WCHAR *e;
    if (FAILED(hr) || !x.face3) { ls_close(&x); t_skip("fixture"); return; }
    CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.face3, &s));
    e = s ? ls_en(s) : NULL;
    t_note("FontFace3::GetFamilyNames with ids 1+16: \"%ls\"", e ? e : L"?");
    CHECK(e && (!lstrcmpW(e, L"LsFam") || !lstrcmpW(e, L"LsFam Bold")));
    free(e); DW_RELEASE(s);
    s = NULL; CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(x.face3, &s));
    e = s ? ls_en(s) : NULL;
    t_note("FontFace3::GetFaceNames with ids 2+17: \"%ls\"", e ? e : L"?");
    CHECK(e && (!lstrcmpW(e, L"Bold") || !lstrcmpW(e, L"Regular")));
    free(e); DW_RELEASE(s);
    ls_close(&x);
}
TEST(lsn_family_only_id1_used_when_no_typographic) {
    LS_NEED_FACTORY(); LS_NEED_FACE(x, 1, ((const ls_rec[]){ RW(3, 1, 0x40C, 1, L"Famille") }));
    IDWriteLocalizedStrings *s = NULL; WCHAR *e;
    CHECK_HR_OK(IDWriteFontFace3_GetFamilyNames(x.face3, &s));
    if (s) {
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 2);
        e = ls_en(s); CHECK(e && !lstrcmpW(e, L"LsFam")); free(e);
        { int i = ls_index_of(s, L"fr-fr"); CHECK(i >= 0); if (i >= 0) { e = ls_dup_string(s, i); CHECK(e && !lstrcmpW(e, L"Famille")); free(e); } }
        DW_RELEASE(s);
    }
    ls_close(&x);
}
TEST(lsn_face_names_localized) {
    LS_NEED_FACTORY(); LS_NEED_FACE(x, 1, ((const ls_rec[]){ RW(3, 1, 0x407, 2, L"Standard"), RW(3, 1, 0x411, 2, L"標準") }));
    IDWriteLocalizedStrings *s = NULL; WCHAR *e;
    CHECK_HR_OK(IDWriteFontFace3_GetFaceNames(x.face3, &s));
    if (s) {
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(s), 3);
        e = ls_en(s); CHECK(e && !lstrcmpW(e, L"Regular")); free(e);
        { int i = ls_index_of(s, L"de-de"); CHECK(i >= 0); if (i >= 0) { e = ls_dup_string(s, i); CHECK(e && !lstrcmpW(e, L"Standard")); free(e); } }
        { int i = ls_index_of(s, L"ja-jp"); CHECK(i >= 0); if (i >= 0) { e = ls_dup_string(s, i); CHECK(e && !lstrcmpW(e, L"標準")); free(e); } }
        DW_RELEASE(s);
    }
    ls_close(&x);
}
