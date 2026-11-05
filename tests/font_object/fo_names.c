/* IDWriteFont::GetFaceNames and GetInformationalStrings.
 * Decomp: DWriteFont::GetInformationalStrings zeroes *strings and *exists first, maps ids 1..21 (0x15) onto name dictionaries and
 * reports exists=TRUE only for a non-empty dictionary; any id outside [1,21] yields exists=FALSE with S_OK. */
#include "fo.h"

/* DWRITE_INFORMATIONAL_STRING_ID -> OpenType name id (docs table), -1 for ids that come from the 'meta' table */
static const struct { int id; const char *label; int nameid; } k_ids[] = {
    { 1, "COPYRIGHT_NOTICE", 0 }, { 2, "VERSION_STRINGS", 5 }, { 3, "TRADEMARK", 7 }, { 4, "MANUFACTURER", 8 }, { 5, "DESIGNER", 9 },
    { 6, "DESIGNER_URL", 12 }, { 7, "DESCRIPTION", 10 }, { 8, "FONT_VENDOR_URL", 11 }, { 9, "LICENSE_DESCRIPTION", 13 }, { 10, "LICENSE_INFO_URL", 14 },
    { 11, "WIN32_FAMILY_NAMES", 1 }, { 12, "WIN32_SUBFAMILY_NAMES", 2 }, { 13, "TYPOGRAPHIC_FAMILY_NAMES", 16 }, { 14, "TYPOGRAPHIC_SUBFAMILY_NAMES", 17 },
    { 15, "SAMPLE_TEXT", 19 }, { 16, "FULL_NAME", 4 }, { 17, "POSTSCRIPT_NAME", 6 }, { 18, "POSTSCRIPT_CID_NAME", 20 },
    { 19, "WWS_FAMILY_NAME", 21 }, { 20, "DESIGN_SCRIPT_LANGUAGE_TAG", -1 }, { 21, "SUPPORTED_SCRIPT_LANGUAGE_TAG", -1 },
};
#define NIDS 21

/* font whose name table carries a record "Text<nameid>" for every name id 0..22 except 15, 18 (platform 3, lang 0x409 only) */
static void info_spec(ttf_spec *s, const char *fam) {
    int id; char t[64];
    fo_spec(s, fam); ttf_clear_names(s);
    for (id = 0; id <= 22; id++) {
        if (id == 15 || id == 18) continue;
        if (id == 1) snprintf(t, sizeof t, "%s", fam);
        else if (id == 2) snprintf(t, sizeof t, "Regular");
        else if (id == 4) snprintf(t, sizeof t, "%s Full", fam);
        else if (id == 6) snprintf(t, sizeof t, "%s-PS", fam);
        else snprintf(t, sizeof t, "Text%d", id);
        ttf_add_name(s, 3, 1, 0x409, id, t);
    }
}
static void expect_text(int nameid, const char *fam, char *out) {
    if (nameid == 1) snprintf(out, 64, "%s", fam);
    else if (nameid == 2) snprintf(out, 64, "Regular");
    else if (nameid == 4) snprintf(out, 64, "%s Full", fam);
    else if (nameid == 6) snprintf(out, 64, "%s-PS", fam);
    else snprintf(out, 64, "Text%d", nameid);
}

static void check_one_info(int k) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 99; char fam[32], want[64], got[128]; int ix;
    snprintf(fam, sizeof fam, "FoInf%d", k_ids[k].id);
    info_spec(&s, fam); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], (DWRITE_INFORMATIONAL_STRING_ID)k_ids[k].id, &ls, &ex));
    if (k_ids[k].nameid < 0) { t_note("%s: exists=%d", k_ids[k].label, ex); DW_RELEASE(ls); fo_close(&x); return; }
    CHECKF(ex == TRUE, "%s exists", k_ids[k].label);
    CHECK(ls != NULL && ls != (void *)1);
    if (ex && ls && ls != (void *)1) {
        CHECK(IDWriteLocalizedStrings_GetCount(ls) >= 1);
        ix = fo_ls_find_ci(ls, "en-us"); CHECKF(ix >= 0, "%s has an en-us string", k_ids[k].label);
        if (ix >= 0) { expect_text(k_ids[k].nameid, fam, want); fo_ls(ls, ix, got, sizeof got); CHECKF(!strcmp(got, want), "%s: '%s' want '%s'", k_ids[k].label, got, want); }
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(ls), 1);
    }
    DW_RELEASE(ls); fo_close(&x);
}
#define INFO_TEST(i, nm) TEST(font_info_##nm) { check_one_info(i); }
INFO_TEST(0, copyright) INFO_TEST(1, version) INFO_TEST(2, trademark) INFO_TEST(3, manufacturer) INFO_TEST(4, designer) INFO_TEST(5, designer_url)
INFO_TEST(6, description) INFO_TEST(7, vendor_url) INFO_TEST(8, license_description) INFO_TEST(9, license_url) INFO_TEST(10, win32_family)
INFO_TEST(11, win32_subfamily) INFO_TEST(12, typographic_family) INFO_TEST(13, typographic_subfamily) INFO_TEST(14, sample_text) INFO_TEST(15, full_name)
INFO_TEST(16, postscript_name) INFO_TEST(17, postscript_cid_name) INFO_TEST(18, wws_family) INFO_TEST(19, design_script_tag) INFO_TEST(20, supported_script_tag)

TEST(font_info_id_enum_values) {
    CHECK(DWRITE_INFORMATIONAL_STRING_NONE == 0 && DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE == 1 && DWRITE_INFORMATIONAL_STRING_VERSION_STRINGS == 2 &&
          DWRITE_INFORMATIONAL_STRING_TRADEMARK == 3 && DWRITE_INFORMATIONAL_STRING_MANUFACTURER == 4 && DWRITE_INFORMATIONAL_STRING_DESIGNER == 5 &&
          DWRITE_INFORMATIONAL_STRING_DESIGNER_URL == 6 && DWRITE_INFORMATIONAL_STRING_DESCRIPTION == 7 && DWRITE_INFORMATIONAL_STRING_FONT_VENDOR_URL == 8 &&
          DWRITE_INFORMATIONAL_STRING_LICENSE_DESCRIPTION == 9 && DWRITE_INFORMATIONAL_STRING_LICENSE_INFO_URL == 10 && DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES == 11 &&
          DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES == 12 && DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES == 13 &&
          DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES == 14 && DWRITE_INFORMATIONAL_STRING_SAMPLE_TEXT == 15 && DWRITE_INFORMATIONAL_STRING_FULL_NAME == 16 &&
          DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME == 17 && DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_CID_NAME == 18 &&
          DWRITE_INFORMATIONAL_STRING_WEIGHT_STRETCH_STYLE_FAMILY_NAME == 19 && DWRITE_INFORMATIONAL_STRING_DESIGN_SCRIPT_LANGUAGE_TAG == 20 &&
          DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG == 21);
    CHECK(DWRITE_INFORMATIONAL_STRING_PREFERRED_FAMILY_NAMES == DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES);
    CHECK(DWRITE_INFORMATIONAL_STRING_PREFERRED_SUBFAMILY_NAMES == DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES);
    CHECK(DWRITE_INFORMATIONAL_STRING_WWS_FAMILY_NAME == DWRITE_INFORMATIONAL_STRING_WEIGHT_STRETCH_STYLE_FAMILY_NAME);
}
TEST(font_info_none_id_reports_absent) {
    /* id 0 (NONE) is below the valid range [1,21]: S_OK, exists FALSE, *strings NULL */
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 99; info_spec(&s, "FoInfNone"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_NONE, &ls, &ex));
    EXPECT_EQ(ex, FALSE); CHECK_OUT_NULL(ls);
    fo_close(&x);
}
TEST(font_info_out_of_range_ids_absent) {
    static const int ids[] = { 22, 23, 24, 31, 32, 63, 64, 100, 255, 256, 1000, 65535, 0x10000, 0x7fffffff, -1, -2, -100, (int)0x80000000 };
    fo_fx x; ttf_spec s; int i; info_spec(&s, "FoInfRng"); FO_OPEN1(x, s);
    for (i = 0; i < (int)(sizeof ids / sizeof ids[0]); i++) {
        IDWriteLocalizedStrings *ls = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 99;
        HRESULT hr = IDWriteFont_GetInformationalStrings(x.font[0], (DWRITE_INFORMATIONAL_STRING_ID)ids[i], &ls, &ex);
        CHECKF(hr == S_OK, "id %d -> %s", ids[i], dw_hr_name(hr)); CHECKF(ex == FALSE, "id %d exists=%d", ids[i], ex); CHECKF(ls == NULL, "id %d strings=%p", ids[i], (void *)ls);
        DW_RELEASE(ls);
    }
    fo_close(&x);
}
TEST(font_info_absent_name_records) {
    /* ids whose name records are missing report exists FALSE (empty dictionary) */
    fo_fx x; ttf_spec s; int k; fo_spec(&s, "FoInfAbs"); FO_OPEN1(x, s);   /* only name ids 1,2,4,6 (+ mac copies) */
    for (k = 0; k < NIDS; k++) {
        IDWriteLocalizedStrings *ls = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 99; int nid = k_ids[k].nameid;
        HRESULT hr = IDWriteFont_GetInformationalStrings(x.font[0], (DWRITE_INFORMATIONAL_STRING_ID)k_ids[k].id, &ls, &ex);
        CHECKF(hr == S_OK, "%s -> %s", k_ids[k].label, dw_hr_name(hr));
        if (nid == 1 || nid == 2 || nid == 4 || nid == 6) { CHECKF(ex == TRUE && ls, "%s present", k_ids[k].label); }
        else if (nid == 0 || nid == 5 || nid == 7 || nid == 8 || nid == 9 || nid == 10 || nid == 11 || nid == 12 || nid == 13 || nid == 14 || nid == 19 || nid == 20) {
            CHECKF(ex == FALSE && ls == NULL, "%s absent (ex=%d)", k_ids[k].label, ex);
        } else t_note("%s without a record: exists=%d", k_ids[k].label, ex);
        DW_RELEASE(ls);
    }
    fo_close(&x);
}
TEST(font_info_typographic_fallback_note) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; fo_spec(&s, "FoInfTyp"); FO_OPEN1(x, s);
    IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES, &ls, &ex);
    t_note("font without name id 16: TYPOGRAPHIC_FAMILY_NAMES exists=%d", ex); DW_RELEASE(ls);
    IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES, &ls, &ex);
    t_note("font without name id 17: TYPOGRAPHIC_SUBFAMILY_NAMES exists=%d", ex); DW_RELEASE(ls);
    IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_WEIGHT_STRETCH_STYLE_FAMILY_NAME, &ls, &ex);
    t_note("font without name id 21: WWS_FAMILY_NAME exists=%d", ex); DW_RELEASE(ls);
    fo_close(&x);
}
TEST(font_info_distinct_objects_per_call) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *a = NULL, *b = NULL; BOOL ex; info_spec(&s, "FoInfObj"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_FULL_NAME, &a, &ex));
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_FULL_NAME, &b, &ex));
    CHECK(a && b);
    if (a && b) {
        EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); CHECK(a != b);
        { char s1[64], s2[64]; dw_ls_en(a, s1, sizeof s1); dw_ls_en(b, s2, sizeof s2); EXPECT_STR(s1, s2); }
    }
    DW_RELEASE(a); DW_RELEASE(b); fo_close(&x);
}
TEST(font_info_strings_outlive_font) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *a = NULL; BOOL ex; char buf[64]; info_spec(&s, "FoInfLife"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_FULL_NAME, &a, &ex));
    fo_close(&x);
    CHECK(a != NULL);
    if (a) { CHECK_HR_OK(dw_ls_en(a, buf, sizeof buf)); EXPECT_STR(buf, "FoInfLife Full"); DW_RELEASE(a); }
}
TEST(font_info_queries_do_not_disturb_each_other) {
    fo_fx x; ttf_spec s; int k, pass; info_spec(&s, "FoInfOrd"); FO_OPEN1(x, s);
    for (pass = 0; pass < 3; pass++)
        for (k = 20; k >= 0; k--) {
            IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; int nid = k_ids[k].nameid;
            if (nid < 0) continue;
            IDWriteFont_GetInformationalStrings(x.font[0], (DWRITE_INFORMATIONAL_STRING_ID)k_ids[k].id, &ls, &ex);
            CHECKF(ex && ls, "%s pass %d", k_ids[k].label, pass); DW_RELEASE(ls);
        }
    fo_close(&x);
}
TEST(font_info_multiple_languages) {
    /* name records in en-US and de-DE produce two strings in one IDWriteLocalizedStrings */
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; int en, de; char b[64];
    info_spec(&s, "FoInfLang"); ttf_add_name(&s, 3, 1, 0x407, 0, "Kopie 0"); ttf_add_name(&s, 3, 1, 0x40C, 0, "Copie 0");
    FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, &ls, &ex));
    CHECK(ex && ls);
    if (ls) {
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(ls), 3);
        en = fo_ls_find_ci(ls, "en-us"); de = fo_ls_find_ci(ls, "de-de");
        CHECK(en >= 0); CHECK(de >= 0); CHECK(fo_ls_find_ci(ls, "fr-fr") >= 0);
        if (en >= 0) { fo_ls(ls, en, b, sizeof b); EXPECT_STR(b, "Text0"); }
        if (de >= 0) { fo_ls(ls, de, b, sizeof b); EXPECT_STR(b, "Kopie 0"); }
    }
    DW_RELEASE(ls); fo_close(&x);
}
TEST(font_info_locale_names_lowercase_note) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char b[64]; info_spec(&s, "FoInfLoc"); FO_OPEN1(x, s);
    IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_FULL_NAME, &ls, &ex);
    if (ls) { fo_ls_locale(ls, 0, b, sizeof b); t_note("locale name of name record 0x409 is '%s'", b); CHECK(!_stricmp(b, "en-us")); }
    DW_RELEASE(ls); fo_close(&x);
}
TEST(font_info_non_ascii_strings) {
    /* BMP and non-BMP characters survive the UTF-16BE name record round trip */
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; WCHAR w[64]; int n;
    fo_spec(&s, "FoInfUni"); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "FoInfUni"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&s, 3, 1, 0x409, 4, "FoInfUni");
    ttf_add_name(&s, 3, 1, 0x409, 6, "FoInfUni-Regular");
    ttf_add_name(&s, 3, 1, 0x409, 0, "\xC3\xA9\xE4\xB8\xAD\xE6\x96\x87\xF0\x9F\x98\x80z");
    FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, &ls, &ex));
    if (ls && ex) {
        n = fo_ls_w(ls, 0, w, 64);
        EXPECT_EQ(n, 7); CHECK(!lstrcmpW(w, L"é中文\xD83D\xDE00z"));
        { UINT32 len = 0; CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(ls, 0, &len)); EXPECT_EQ(len, 6); }
    } else CHECK(0);
    DW_RELEASE(ls); fo_close(&x);
}
TEST(font_info_long_string) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char t[96]; WCHAR w[128]; int i; fo_spec(&s, "FoInfLong"); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "FoInfLong"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&s, 3, 1, 0x409, 4, "FoInfLong"); ttf_add_name(&s, 3, 1, 0x409, 6, "FoInfLong-R");
    for (i = 0; i < 90; i++) t[i] = (char)('a' + i % 26);
    t[90] = 0; ttf_add_name(&s, 3, 1, 0x409, 0, t);
    FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, &ls, &ex));
    if (ls && ex) { EXPECT_EQ(fo_ls_w(ls, 0, w, 128), 90); CHECK(w[0] == 'a' && w[89] == (WCHAR)('a' + 89 % 26)); } else CHECK(0);
    DW_RELEASE(ls); fo_close(&x);
}
TEST(font_info_mac_roman_only_note) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char b[64], l[32]; fo_spec(&s, "FoInfMac"); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "FoInfMac"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&s, 3, 1, 0x409, 4, "FoInfMac"); ttf_add_name(&s, 3, 1, 0x409, 6, "FoInfMac-R");
    ttf_add_name(&s, 1, 0, 0, 0, "MacCopyright");
    FO_OPEN1(x, s);
    IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, &ls, &ex);
    t_note("Mac Roman only copyright record: exists=%d", ex);
    if (ls) { fo_ls(ls, 0, b, sizeof b); fo_ls_locale(ls, 0, l, sizeof l); t_note("  count=%u first='%s' locale='%s'", IDWriteLocalizedStrings_GetCount(ls), b, l); }
    DW_RELEASE(ls); fo_close(&x);
}
TEST(font_info_same_text_both_platforms_note) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; fo_spec(&s, "FoInfDup"); FO_OPEN1(x, s);   /* ids 1,2,4,6 in (3,1,0x409) and (1,0,0) */
    IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, &ls, &ex);
    CHECK(ex && ls);
    if (ls) { UINT32 n = IDWriteLocalizedStrings_GetCount(ls); CHECK(n >= 1); t_note("family name records in (3,1) and (1,0): %u string(s)", n); }
    DW_RELEASE(ls); fo_close(&x);
}
TEST(font_info_unicode_platform_note) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; fo_spec(&s, "FoInfU0"); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "FoInfU0"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&s, 3, 1, 0x409, 4, "FoInfU0"); ttf_add_name(&s, 3, 1, 0x409, 6, "FoInfU0-R");
    ttf_add_name(&s, 0, 3, 0, 0, "UnicodePlatformCopyright");
    FO_OPEN1(x, s);
    IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, &ls, &ex);
    t_note("platform 0 (Unicode) only copyright record: exists=%d", ex); DW_RELEASE(ls); fo_close(&x);
}
TEST(font_info_empty_record_note) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; fo_spec(&s, "FoInfEmpty"); ttf_add_name(&s, 3, 1, 0x409, 0, "");
    FO_OPEN1(x, s);
    IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, &ls, &ex);
    t_note("empty copyright record: exists=%d ls=%p", ex, (void *)ls); DW_RELEASE(ls); fo_close(&x);
}
static uint8_t *meta_table(unsigned *len, const char *dlng, const char *slng) {
    unsigned nd = (unsigned)strlen(dlng), ns = (unsigned)strlen(slng), hdr = 16 + 12 * 2, i; uint8_t *m = (uint8_t *)calloc(1, hdr + nd + ns + 4);
    m[3] = 1;                                   /* version 1 */
    m[15] = 2;                                  /* dataMapsCount */
    memcpy(m + 16, "dlng", 4); m[16 + 4 + 3] = (uint8_t)hdr; m[16 + 8 + 3] = (uint8_t)nd;
    memcpy(m + 28, "slng", 4); m[28 + 4 + 3] = (uint8_t)(hdr + nd); m[28 + 8 + 3] = (uint8_t)ns;
    memcpy(m + hdr, dlng, nd); memcpy(m + hdr + nd, slng, ns);
    for (i = 0; i < 2; i++) (void)i;
    *len = hdr + nd + ns; return m;
}
TEST(font_info_meta_script_tags) {
    /* DESIGN_SCRIPT_LANGUAGE_TAG / SUPPORTED_SCRIPT_LANGUAGE_TAG come from the 'dlng' / 'slng' maps of the 'meta' table */
    fo_fx x; ttf_spec s; ttf_rawtable raw; unsigned ml; uint8_t *m = meta_table(&ml, "Latn", "Latn,Grek,Cyrl"); IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char b[64];
    fo_spec(&s, "FoInfMeta"); raw.data = m; raw.len = ml; memcpy(raw.tag, "meta", 5); s.raw = &raw; s.nraw = 1;
    FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_DESIGN_SCRIPT_LANGUAGE_TAG, &ls, &ex));
    CHECKF(ex == TRUE, "design script tag present"); if (ls) { fo_ls(ls, 0, b, sizeof b); t_note("dlng 'Latn' -> '%s' (count %u)", b, IDWriteLocalizedStrings_GetCount(ls)); } DW_RELEASE(ls);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG, &ls, &ex));
    CHECKF(ex == TRUE, "supported script tag present"); if (ls) { fo_ls(ls, 0, b, sizeof b); t_note("slng 'Latn,Grek,Cyrl' -> '%s' (count %u)", b, IDWriteLocalizedStrings_GetCount(ls)); } DW_RELEASE(ls);
    fo_close(&x); free(m);
}
TEST(font_info_no_meta_absent) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *ls = (IDWriteLocalizedStrings *)(ULONG_PTR)1; BOOL ex = 99; fo_spec(&s, "FoInfNoMeta"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_DESIGN_SCRIPT_LANGUAGE_TAG, &ls, &ex)); EXPECT_EQ(ex, FALSE); CHECK_OUT_NULL(ls);
    ls = (IDWriteLocalizedStrings *)(ULONG_PTR)1; ex = 99;
    CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG, &ls, &ex)); EXPECT_EQ(ex, FALSE); CHECK_OUT_NULL(ls);
    fo_close(&x);
}
TEST(font_info_through_all_font_interfaces) {
    fo_fx x; ttf_spec s; IDWriteFont3 *f3 = NULL; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char b[64]; info_spec(&s, "FoInfIf"); FO_OPEN1(x, s);
    IDWriteFont_QueryInterface(x.font[0], &IID_IDWriteFont3, (void **)&f3);
    if (f3) { CHECK_HR_OK(IDWriteFont3_GetInformationalStrings(f3, DWRITE_INFORMATIONAL_STRING_FULL_NAME, &ls, &ex)); CHECK(ex && ls); if (ls) { dw_ls_en(ls, b, sizeof b); EXPECT_STR(b, "FoInfIf Full"); } }
    DW_RELEASE(ls); DW_RELEASE(f3); fo_close(&x);
}

/* ---- face names ------------------------------------------------------------------------------------------ */
static void face_name_en(IDWriteFont *f, char *buf, int cap) {
    IDWriteLocalizedStrings *n = NULL; buf[0] = 0;
    if (SUCCEEDED(IDWriteFont_GetFaceNames(f, &n)) && n) { dw_ls_en(n, buf, cap); IDWriteLocalizedStrings_Release(n); }
}
TEST(font_face_names_ribbi) {
    ttf_spec s[4]; fo_fx x; char b[64]; int i; static const char *want[4] = { "Regular", "Bold", "Italic", "Bold Italic" };
    ttf_style_spec(&s[0], "FoFnR", 400, 5, 0); ttf_style_spec(&s[1], "FoFnR", 700, 5, 0); ttf_style_spec(&s[2], "FoFnR", 400, 5, 1); ttf_style_spec(&s[3], "FoFnR", 700, 5, 1);
    FO_OPEN(x, s, 4);
    for (i = 0; i < 4; i++) { face_name_en(x.font[i], b, sizeof b); CHECKF(!strcmp(b, want[i]), "font %d face name '%s' want '%s'", i, b, want[i]); }
    fo_close(&x);
}
TEST(font_face_names_ladder_use_typographic_subfamily) {
    /* weights other than 400/700 carry name id 17 ("Thin", "Light", ...): that is the face name */
    ttf_spec s[7]; fo_fx x; char b[64]; int i; static const int w[7] = { 100, 200, 300, 500, 600, 800, 900 };
    for (i = 0; i < 7; i++) { char f[32]; snprintf(f, sizeof f, "FoFnL%d", w[i]); ttf_style_spec(&s[i], f, w[i], 5, 0); }
    FO_OPEN(x, s, 7);
    for (i = 0; i < 7; i++) { face_name_en(x.font[i], b, sizeof b); CHECKF(!strcmp(b, ttf_weight_word(w[i])), "weight %d face name '%s' want '%s'", w[i], b, ttf_weight_word(w[i])); }
    fo_close(&x);
}
TEST(font_face_names_condensed_and_expanded) {
    ttf_spec s[4]; fo_fx x; char b[64]; int i; static const int wd[4] = { 3, 4, 6, 7 }; static const char *want[4] = { "Condensed", "SemiCondensed", "SemiExpanded", "Expanded" };
    for (i = 0; i < 4; i++) { char f[32]; snprintf(f, sizeof f, "FoFnW%d", wd[i]); ttf_style_spec(&s[i], f, 400, wd[i], 0); }
    FO_OPEN(x, s, 4);
    for (i = 0; i < 4; i++) { face_name_en(x.font[i], b, sizeof b); CHECKF(!strcmp(b, want[i]), "width %d face name '%s' want '%s'", wd[i], b, want[i]); }
    fo_close(&x);
}
TEST(font_face_names_typographic_beats_legacy_note) {
    fo_fx x; ttf_spec s; char b[64]; fo_spec(&s, "FoFnTyp"); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "FoFnTyp Legacy"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&s, 3, 1, 0x409, 4, "FoFnTyp Full"); ttf_add_name(&s, 3, 1, 0x409, 6, "FoFnTyp-R");
    ttf_add_name(&s, 3, 1, 0x409, 16, "FoFnTyp"); ttf_add_name(&s, 3, 1, 0x409, 17, "Display");
    FO_OPEN1(x, s);
    face_name_en(x.font[0], b, sizeof b); t_note("name id 2 'Regular' + id 17 'Display' -> face name '%s'", b);
    fo_close(&x);
}
TEST(font_face_names_localized_two_languages) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *n = NULL; int en, de; char b[64]; fo_spec(&s, "FoFnLoc"); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x409, 1, "FoFnLoc"); ttf_add_name(&s, 3, 1, 0x409, 2, "Regular"); ttf_add_name(&s, 3, 1, 0x409, 4, "FoFnLoc"); ttf_add_name(&s, 3, 1, 0x409, 6, "FoFnLoc-R");
    ttf_add_name(&s, 3, 1, 0x407, 1, "FoFnLoc"); ttf_add_name(&s, 3, 1, 0x407, 2, "Standard");
    FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetFaceNames(x.font[0], &n));
    if (n) {
        en = fo_ls_find_ci(n, "en-us"); de = fo_ls_find_ci(n, "de-de"); CHECK(en >= 0); CHECK(de >= 0);
        if (en >= 0) { fo_ls(n, en, b, sizeof b); EXPECT_STR(b, "Regular"); }
        if (de >= 0) { fo_ls(n, de, b, sizeof b); EXPECT_STR(b, "Standard"); }
        EXPECT_EQ(IDWriteLocalizedStrings_GetCount(n), 2);
    }
    DW_RELEASE(n); fo_close(&x);
}
TEST(font_face_names_new_object_each_call) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *a = NULL, *b = NULL; fo_spec(&s, "FoFnObj"); FO_OPEN1(x, s);
    CHECK_HR_OK(IDWriteFont_GetFaceNames(x.font[0], &a)); CHECK_HR_OK(IDWriteFont_GetFaceNames(x.font[0], &b));
    CHECK(a && b);
    if (a && b) { EXPECT_REFS(a, 1); EXPECT_REFS(b, 1); CHECK(a != b); EXPECT_EQ(IDWriteLocalizedStrings_GetCount(a), IDWriteLocalizedStrings_GetCount(b)); }
    DW_RELEASE(a); DW_RELEASE(b); fo_close(&x);
}
TEST(font_face_names_outlive_font) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *a = NULL; char b[32]; fo_spec(&s, "FoFnLife"); FO_OPEN1(x, s);
    IDWriteFont_GetFaceNames(x.font[0], &a); fo_close(&x);
    CHECK(a != NULL); if (a) { dw_ls_en(a, b, sizeof b); EXPECT_STR(b, "Regular"); DW_RELEASE(a); }
}
TEST(font_face_names_locale_string_apis) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *n = NULL; UINT32 idx = 77; BOOL ex = 77; fo_spec(&s, "FoFnApi"); FO_OPEN1(x, s);
    IDWriteFont_GetFaceNames(x.font[0], &n);
    if (n) {
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(n, L"en-us", &idx, &ex)); CHECK(ex);
        CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(n, L"xx-nonexistent", &idx, &ex)); EXPECT_EQ(ex, FALSE); EXPECT_EQ(idx, (UINT32)-1);
        { UINT32 len = 0; WCHAR w[16]; CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(n, 0, &len)); EXPECT_EQ(len, 7);
          CHECK_HR(IDWriteLocalizedStrings_GetString(n, 0, w, len), DW_E_NOT_SUFFICIENT_BUFFER);
          CHECK_HR_OK(IDWriteLocalizedStrings_GetString(n, 0, w, len + 1)); CHECK(!lstrcmpW(w, L"Regular"));
          CHECK_HR_FAILED(IDWriteLocalizedStrings_GetStringLength(n, IDWriteLocalizedStrings_GetCount(n), &len)); }
    }
    DW_RELEASE(n); fo_close(&x);
}
TEST(font_face_names_system_arial_ribbi) {
    static const struct { int w, st; const char *want; } t[] = { { 400, 0, "Regular" }, { 700, 0, "Bold" }, { 400, 2, "Italic" }, { 700, 2, "Bold Italic" } };
    int k;
    for (k = 0; k < 4; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), L"Arial", (DWRITE_FONT_WEIGHT)t[k].w, DWRITE_FONT_STRETCH_NORMAL, (DWRITE_FONT_STYLE)t[k].st); char b[64];
        if (!f) { t_skip("Arial not installed"); return; }
        face_name_en(f, b, sizeof b); CHECKF(!strcmp(b, t[k].want), "Arial w=%d st=%d: '%s' want '%s'", t[k].w, t[k].st, b, t[k].want);
        DW_RELEASE(f);
    }
}
TEST(font_face_names_system_condensed_note) {
    IDWriteFont *f = fo_sysfont(dw_shared(), L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, DWRITE_FONT_STYLE_NORMAL); char b[64];
    if (!f) { t_skip("Arial not installed"); return; }
    face_name_en(f, b, sizeof b);
    t_note("Arial condensed request: stretch %d weight %d simulations %d face name '%s'", (int)IDWriteFont_GetStretch(f), (int)IDWriteFont_GetWeight(f), (int)IDWriteFont_GetSimulations(f), b);
    DW_RELEASE(f);
}
TEST(font_face_names_system_segoe_ui_light) {
    IDWriteFont *f = fo_sysfont(dw_shared(), L"Segoe UI", DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); char b[64];
    if (!f) { t_skip("Segoe UI not installed"); return; }
    face_name_en(f, b, sizeof b); EXPECT_STR(b, "Light"); DW_RELEASE(f);
}
TEST(font_face_names_system_match_name_table) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Courier New", L"Tahoma", L"Segoe UI" };
    int k, checked = 0;
    for (k = 0; k < 5; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        uint8_t *nm; UINT32 len; WCHAR w[64], g[64]; IDWriteLocalizedStrings *n = NULL;
        if (!f) continue;
        if (fo_sys_table(f, "name", &nm, &len) && fo_name_find(nm, len, 3, 1, 0x409, 2, w, 64) >= 0 && SUCCEEDED(IDWriteFont_GetFaceNames(f, &n)) && n) {
            int ix = fo_ls_find_ci(n, "en-us");
            if (ix >= 0 && fo_ls_w(n, ix, g, 64)) { checked++; CHECKF(!lstrcmpW(g, w), "%ls face name vs name id 2", fams[k]); }
        }
        free(nm); DW_RELEASE(n); DW_RELEASE(f);
    }
    if (!checked) t_skip("no system font comparable");
}

/* ---- informational strings of system fonts ------------------------------------------------------------------ */
TEST(font_info_system_strings_match_name_table) {
    /* every string whose en-US (3,1,0x409) record exists in the file's name table is reported identically */
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Verdana" };
    int fi, checked = 0;
    for (fi = 0; fi < 6; fi++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[fi], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        uint8_t *nm; UINT32 len; int k;
        if (!f) continue;
        if (fo_sys_table(f, "name", &nm, &len)) {
            for (k = 0; k < NIDS; k++) {
                WCHAR w[512], g[512]; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; int fl, ix;
                if (k_ids[k].nameid < 0) continue;
                fl = fo_name_find(nm, len, 3, 1, 0x409, k_ids[k].nameid, w, 512);
                if (FAILED(IDWriteFont_GetInformationalStrings(f, (DWRITE_INFORMATIONAL_STRING_ID)k_ids[k].id, &ls, &ex))) { CHECK(0); continue; }
                if (fl >= 0) {
                    CHECKF(ex && ls, "%ls %s exists", fams[fi], k_ids[k].label);
                    if (ls && (ix = fo_ls_find_ci(ls, "en-us")) >= 0 && fo_ls_w(ls, ix, g, 512)) { checked++; CHECKF(!lstrcmpW(g, w), "%ls %s differs from the name table", fams[fi], k_ids[k].label); }
                }
                DW_RELEASE(ls);
            }
            free(nm);
        }
        DW_RELEASE(f);
    }
    if (!checked) t_skip("no system font comparable");
}
TEST(font_info_system_arial_known_strings) {
    FO_SYS_REG(f, L"Arial");
    static const struct { int id; const char *want; } t[] = { { DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, "Arial" }, { DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, "Regular" },
        { DWRITE_INFORMATIONAL_STRING_FULL_NAME, "Arial" }, { DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, "ArialMT" } };
    int k;
    for (k = 0; k < 4; k++) {
        IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char b[64];
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(f, (DWRITE_INFORMATIONAL_STRING_ID)t[k].id, &ls, &ex));
        CHECKF(ex && ls, "id %d", t[k].id);
        if (ls) { dw_ls_en(ls, b, sizeof b); CHECKF(!strcmp(b, t[k].want), "id %d: '%s' want '%s'", t[k].id, b, t[k].want); }
        DW_RELEASE(ls);
    }
    DW_RELEASE(f);
}
TEST(font_info_system_arial_bold_strings) {
    FO_SYS(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    static const struct { int id; const char *want; } t[] = { { DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, "Arial" }, { DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, "Bold" },
        { DWRITE_INFORMATIONAL_STRING_FULL_NAME, "Arial Bold" }, { DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, "Arial-BoldMT" } };
    int k;
    for (k = 0; k < 4; k++) {
        IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char b[64];
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(f, (DWRITE_INFORMATIONAL_STRING_ID)t[k].id, &ls, &ex));
        if (ls) { dw_ls_en(ls, b, sizeof b); CHECKF(!strcmp(b, t[k].want), "id %d: '%s' want '%s'", t[k].id, b, t[k].want); } else CHECK(0);
        DW_RELEASE(ls);
    }
    DW_RELEASE(f);
}
TEST(font_info_system_arial_copyright_note) {
    FO_SYS_REG(f, L"Arial"); IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char b[256];
    IDWriteFont_GetInformationalStrings(f, DWRITE_INFORMATIONAL_STRING_COPYRIGHT_NOTICE, &ls, &ex);
    CHECK(ex && ls); if (ls) { fo_ls(ls, 0, b, sizeof b); CHECK(strstr(b, "Monotype") != NULL || strstr(b, "Microsoft") != NULL); t_note("Arial copyright: %s", b); }
    DW_RELEASE(ls);
    IDWriteFont_GetInformationalStrings(f, DWRITE_INFORMATIONAL_STRING_VERSION_STRINGS, &ls, &ex);
    CHECK(ex && ls); if (ls) { fo_ls(ls, 0, b, sizeof b); CHECK(strstr(b, "Version") != NULL); t_note("Arial version: %s", b); }
    DW_RELEASE(ls);
    DW_RELEASE(f);
}
TEST(font_info_system_all_ids_note) {
    FO_SYS_REG(f, L"Segoe UI"); int k;
    for (k = 0; k < NIDS; k++) {
        IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; char b[96] = "";
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(f, (DWRITE_INFORMATIONAL_STRING_ID)k_ids[k].id, &ls, &ex));
        CHECKF((ex != 0) == (ls != NULL), "%s: exists=%d but strings=%p", k_ids[k].label, ex, (void *)ls);
        if (ls) fo_ls(ls, 0, b, sizeof b);
        t_note("Segoe UI %s: exists=%d count=%u first='%.80s'", k_ids[k].label, ex, ls ? IDWriteLocalizedStrings_GetCount(ls) : 0, b);
        DW_RELEASE(ls);
    }
    DW_RELEASE(f);
}
TEST(font_info_exists_iff_strings_nonnull) {
    fo_fx x; ttf_spec s; int k; info_spec(&s, "FoInfIff"); FO_OPEN1(x, s);
    for (k = 0; k < NIDS; k++) {
        IDWriteLocalizedStrings *ls = NULL; BOOL ex = 5;
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(x.font[0], (DWRITE_INFORMATIONAL_STRING_ID)k_ids[k].id, &ls, &ex));
        CHECKF(ex == 0 || ex == 1, "%s exists=%d is not a canonical BOOL", k_ids[k].label, ex);
        CHECKF((ex == 1) == (ls != NULL), "%s", k_ids[k].label); DW_RELEASE(ls);
    }
    fo_close(&x);
}
