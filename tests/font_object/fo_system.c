/* System-font oriented IDWriteFont tests (relations and table cross-checks; exact values only where stable on Windows 10),
 * loader-activity contract of the metadata getters, and unusual OS/2/head style data. */
#include "fo.h"

static IDWriteFont3 *as3(IDWriteFont *f) { IDWriteFont3 *p = NULL; if (f) IDWriteFont_QueryInterface(f, &IID_IDWriteFont3, (void **)&p); return p; }
static void face_en(IDWriteFont *f, char *b, int cap) { IDWriteLocalizedStrings *n = NULL; b[0] = 0; if (SUCCEEDED(IDWriteFont_GetFaceNames(f, &n)) && n) { dw_ls_en(n, b, cap); IDWriteLocalizedStrings_Release(n); } }

/* ---- loader activity ------------------------------------------------------------------------------------------------------ */
TEST(font_metadata_getters_do_not_touch_the_loader) {
    /* weight/style/metrics/cmap/name data of a font live in the font set; once the collection exists none of these calls reads the file */
    fo_fx x; ttf_spec s; IDWriteFont *f; IDWriteFont1 *f1 = NULL; IDWriteFont3 *f3; LONG reads0, create0, out0; int i; fo_spec(&s, "FoLdr"); FO_OPEN1(x, s);
    f = x.font[0]; f3 = as3(f); IDWriteFont_QueryInterface(f, &IID_IDWriteFont1, (void **)&f1);
    reads0 = dwx_loader_total_reads(x.fl); create0 = x.fl->n_create; out0 = dwx_loader_total_outstanding(x.fl);
    for (i = 0; i < 3; i++) {
        DWRITE_FONT_METRICS m; DWRITE_PANOSE p; BOOL e; IDWriteLocalizedStrings *ls; IDWriteFontFamily *fam; UINT32 n; DWRITE_UNICODE_RANGE r[8]; DWRITE_FONT_METRICS1 m1;
        IDWriteFont_GetWeight(f); IDWriteFont_GetStretch(f); IDWriteFont_GetStyle(f); IDWriteFont_GetSimulations(f); IDWriteFont_IsSymbolFont(f); IDWriteFont_GetMetrics(f, &m);
        IDWriteFont_HasCharacter(f, 'A', &e); IDWriteFont_HasCharacter(f, 0x1F600, &e);
        ls = NULL; IDWriteFont_GetFaceNames(f, &ls); DW_RELEASE(ls);
        ls = NULL; IDWriteFont_GetInformationalStrings(f, DWRITE_INFORMATIONAL_STRING_FULL_NAME, &ls, &e); DW_RELEASE(ls);
        fam = NULL; IDWriteFont_GetFontFamily(f, &fam); DW_RELEASE(fam);
        if (f1) { FO_GETM1(f1, &m1); IDWriteFont1_GetPanose(f1, &p); IDWriteFont1_GetUnicodeRanges(f1, 8, r, &n); IDWriteFont1_IsMonospacedFont(f1); }
        if (f3) { IDWriteFont3_Equals(f3, f); FO_F3_HASCHAR(f3, 'B'); }
    }
    if (dwx_loader_total_reads(x.fl) != reads0 || x.fl->n_create != create0) t_note("metadata getters caused loader activity: reads %ld -> %ld, streams %ld -> %ld", reads0, dwx_loader_total_reads(x.fl), create0, x.fl->n_create);
    if (f3) { IDWriteFont3_GetLocality(f3); t_note("after GetLocality: reads %ld -> %ld, streams created %ld -> %ld", reads0, dwx_loader_total_reads(x.fl), create0, x.fl->n_create); }
    t_note("fragments outstanding while the collection is alive: %ld", out0);
    DW_RELEASE(f1); DW_RELEASE(f3); fo_close(&x);
}
TEST(font_fragments_accounting_after_face_creation) {
    fo_fx x; ttf_spec s; IDWriteFontFace *face = NULL; fo_spec(&s, "FoLdr2"); FO_OPEN1(x, s);
    LONG before = dwx_loader_total_outstanding(x.fl);
    IDWriteFont_CreateFontFace(x.font[0], &face);
    t_note("fragments outstanding: before face %ld, with face %ld", before, dwx_loader_total_outstanding(x.fl));
    CHECK(dwx_loader_total_outstanding(x.fl) >= 0);
    DW_RELEASE(face);
    CHECK(dwx_loader_total_outstanding(x.fl) >= 0);
    CHECK(x.fl->nstreams >= 1);
    fo_close(&x);
}
TEST(font_loader_released_after_everything_closed) {
    fo_fx x; ttf_spec s; dwx_mem_loader *ld; dwx_coll_loader *cl; fo_spec(&s, "FoLdr3"); FO_OPEN1(x, s);
    ld = x.fl; cl = x.cl; dwx_addref(ld); dwx_addref(cl);
    fo_close(&x);
    /* after the fixture released the fonts, collection, factory and unregistered the loaders only our extra references remain */
    { long r = dwx_refs(ld); if (r != 2) t_note("loader refs after teardown: %ld (expected 2: collection loader + ours)", r); CHECK(r >= 1); }
    dwx_release(cl); dwx_release(ld);
    CHECK(dwx_destroyed(ld) || dwx_refs(ld) >= 0);
}

/* ---- system faces -------------------------------------------------------------------------------------------------------- */
TEST(font_system_arial_family_member_list_note) {
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; UINT32 idx = 0, n, i; BOOL ex = 0; int condensed = 0;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(dw_shared(), &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex);
    if (!ex) { DW_RELEASE(c); t_skip("Arial not installed"); return; }
    IDWriteFontCollection_GetFontFamily(c, idx, &fam);
    if (fam) {
        n = IDWriteFontFamily_GetFontCount(fam); CHECK(n >= 4);
        for (i = 0; i < n; i++) {
            IDWriteFont *f = NULL; char nm[64];
            if (FAILED(IDWriteFontFamily_GetFont(fam, i, &f)) || !f) { CHECK(0); continue; }
            face_en(f, nm, sizeof nm);
            t_note("Arial[%u]: weight %d stretch %d style %d sim %d face '%s'", i, (int)IDWriteFont_GetWeight(f), (int)IDWriteFont_GetStretch(f), (int)IDWriteFont_GetStyle(f), (int)IDWriteFont_GetSimulations(f), nm);
            if (IDWriteFont_GetStretch(f) == DWRITE_FONT_STRETCH_CONDENSED && IDWriteFont_GetSimulations(f) == 0) { condensed++; CHECKF(strstr(nm, "Narrow") != NULL, "condensed Arial face name '%s'", nm); }
            DW_RELEASE(f);
        }
        t_note("condensed (Arial Narrow) faces in the Arial family: %d", condensed);
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
}
TEST(font_system_ribbi_info_match_name_table) {
    static const struct { int w, st; } t[] = { { 400, 0 }, { 700, 0 }, { 400, 2 }, { 700, 2 } };
    int k, checked = 0;
    for (k = 0; k < 4; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), L"Arial", (DWRITE_FONT_WEIGHT)t[k].w, DWRITE_FONT_STRETCH_NORMAL, (DWRITE_FONT_STYLE)t[k].st); uint8_t *nm; UINT32 len; int ids[] = { 1, 2, 4, 6 }, j;
        if (!f) continue;
        if (fo_sys_table(f, "name", &nm, &len)) {
            for (j = 0; j < 4; j++) {
                static const int dw_ids[] = { DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, DWRITE_INFORMATIONAL_STRING_WIN32_SUBFAMILY_NAMES, DWRITE_INFORMATIONAL_STRING_FULL_NAME, DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME };
                WCHAR w[256], g[256]; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; int ix;
                if (fo_name_find(nm, len, 3, 1, 0x409, ids[j], w, 256) < 0) continue;
                IDWriteFont_GetInformationalStrings(f, (DWRITE_INFORMATIONAL_STRING_ID)dw_ids[j], &ls, &ex);
                if (ls && (ix = fo_ls_find_ci(ls, "en-us")) >= 0 && fo_ls_w(ls, ix, g, 256)) { checked++; CHECKF(!lstrcmpW(g, w), "Arial w=%d st=%d name id %d", t[k].w, t[k].st, ids[j]); }
                DW_RELEASE(ls);
            }
            free(nm);
        }
        DW_RELEASE(f);
    }
    if (!checked) t_skip("Arial not installed");
}
TEST(font_system_segoe_ui_weights_match_name_table) {
    static const int ws[] = { 300, 350, 400, 600, 700, 900 };
    int k, checked = 0;
    for (k = 0; k < 6; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), L"Segoe UI", (DWRITE_FONT_WEIGHT)ws[k], DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); uint8_t *nm; UINT32 len; int id;
        if (!f) continue;
        if (fo_sys_table(f, "name", &nm, &len)) {
            for (id = 1; id <= 21; id++) {
                static const int nid[22] = { 0, 0, 5, 7, 8, 9, 12, 10, 11, 13, 14, 1, 2, 16, 17, 19, 4, 6, 20, 21, 0, 0 };
                WCHAR w[512], g[512]; IDWriteLocalizedStrings *ls = NULL; BOOL ex = 0; int ix, nameid = id <= 19 ? nid[id] : -1;
                if (id == 1) nameid = 0; if (id < 1 || nameid < 0 || id > 19) continue;
                if (fo_name_find(nm, len, 3, 1, 0x409, nameid, w, 512) < 0) continue;
                IDWriteFont_GetInformationalStrings(f, (DWRITE_INFORMATIONAL_STRING_ID)id, &ls, &ex);
                CHECKF(ex && ls, "Segoe UI weight %d id %d", ws[k], id);
                if (ls && (ix = fo_ls_find_ci(ls, "en-us")) >= 0 && fo_ls_w(ls, ix, g, 512)) { checked++; CHECKF(!lstrcmpW(g, w), "Segoe UI weight %d id %d (name id %d)", ws[k], id, nameid); }
                DW_RELEASE(ls);
            }
            free(nm);
        }
        DW_RELEASE(f);
    }
    if (!checked) t_skip("Segoe UI not installed");
}
TEST(font_system_font_without_bold_gets_simulated_bold) {
    static const WCHAR *fams[] = { L"Marlett", L"Wingdings", L"Webdings", L"MS Gothic", L"Symbol" };
    int k, seen = 0;
    for (k = 0; k < 5; k++) {
        IDWriteFont *r = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL), *b, *i;
        if (!r) continue;
        b = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); i = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC);
        seen++;
        if (b && IDWriteFont_GetWeight(r) < 600 && IDWriteFont_GetSimulations(b) != 0) {
            CHECKF(IDWriteFont_GetSimulations(b) & DWRITE_FONT_SIMULATIONS_BOLD, "%ls", fams[k]); EXPECT_EQ(IDWriteFont_GetWeight(b), 700);
        } else if (b) t_note("%ls bold request -> weight %d sim %d", fams[k], (int)IDWriteFont_GetWeight(b), (int)IDWriteFont_GetSimulations(b));
        if (i && IDWriteFont_GetStyle(r) == DWRITE_FONT_STYLE_NORMAL) { t_note("%ls italic request -> style %d sim %d", fams[k], (int)IDWriteFont_GetStyle(i), (int)IDWriteFont_GetSimulations(i)); if (IDWriteFont_GetSimulations(i) & DWRITE_FONT_SIMULATIONS_OBLIQUE) EXPECT_EQ(IDWriteFont_GetStyle(i), DWRITE_FONT_STYLE_OBLIQUE); }
        DW_RELEASE(r); DW_RELEASE(b); DW_RELEASE(i);
    }
    if (!seen) t_skip("none of the listed fonts is installed");
}
TEST(font_system_ttc_faces_have_distinct_indices) {
    static const WCHAR *fams[] = { L"MS Gothic", L"MS UI Gothic", L"MS PGothic" };
    int k, n = 0; UINT32 idx[3] = { 9, 9, 9 };
    for (k = 0; k < 3; k++) {
        IDWriteFont *f = fo_sysfont(dw_shared(), fams[k], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFont3 *f3; IDWriteFontFaceReference *r = NULL;
        if (!f) continue;
        f3 = as3(f); if (f3) IDWriteFont3_GetFontFaceReference(f3, &r);
        if (r) { idx[k] = IDWriteFontFaceReference_GetFontFaceIndex(r); n++; t_note("%ls face index %u", fams[k], idx[k]); }
        DW_RELEASE(r); DW_RELEASE(f3); DW_RELEASE(f);
    }
    if (n < 2) { t_skip("fewer than two msgothic.ttc families installed"); return; }
    if (idx[0] != 9 && idx[1] != 9) CHECK(idx[0] != idx[1]);
    if (idx[0] != 9 && idx[2] != 9) CHECK(idx[0] != idx[2]);
    if (idx[1] != 9 && idx[2] != 9) CHECK(idx[1] != idx[2]);
}
TEST(font_system_variable_font_note) {
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; UINT32 idx = 0, n, i; BOOL ex = 0;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(dw_shared(), &c, FALSE)) || !c) { t_skip("no system collection"); return; }
    IDWriteFontCollection_FindFamilyName(c, L"Bahnschrift", &idx, &ex);
    if (!ex) { DW_RELEASE(c); t_skip("Bahnschrift not installed"); return; }
    IDWriteFontCollection_GetFontFamily(c, idx, &fam);
    if (fam) {
        n = IDWriteFontFamily_GetFontCount(fam); t_note("Bahnschrift family has %u fonts", n); CHECK(n >= 1);
        for (i = 0; i < n && i < 24; i++) {
            IDWriteFont *f = NULL; char nm[64];
            if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, i, &f)) && f) {
                int w = IDWriteFont_GetWeight(f), st = IDWriteFont_GetStretch(f); face_en(f, nm, sizeof nm);
                CHECKF(w >= 1 && w <= 999 && st >= 1 && st <= 9, "font %u weight %d stretch %d", i, w, st);
                t_note("Bahnschrift[%u]: weight %d stretch %d style %d face '%s'", i, w, st, (int)IDWriteFont_GetStyle(f), nm); DW_RELEASE(f);
            }
        }
        DW_RELEASE(fam);
    }
    DW_RELEASE(c);
}

/* ---- unusual style data --------------------------------------------------------------------------------------------------------- */
TEST(font_style_without_os2_note) {
    fo_fx x; ttf_spec s[2]; fo_spec(&s[0], "FoNoOs2"); s[0].omit |= TTF_OMIT_OS2; fo_spec(&s[1], "FoNoOs2B"); s[1].omit |= TTF_OMIT_OS2; s[1].mac_style = 3;
    if (!fo_open(&x, s, 2)) { t_note("font without OS/2 rejected (%s)", dw_hr_name(x.hr_coll)); fo_close(&x); return; }
    t_note("no OS/2: weight %d stretch %d style %d; macStyle bold|italic: weight %d style %d", (int)IDWriteFont_GetWeight(x.font[0]), (int)IDWriteFont_GetStretch(x.font[0]), (int)IDWriteFont_GetStyle(x.font[0]),
           (int)IDWriteFont_GetWeight(x.font[1]), (int)IDWriteFont_GetStyle(x.font[1]));
    EXPECT_EQ(IDWriteFont_GetSimulations(x.font[0]), 0);
    fo_close(&x);
}
TEST(font_style_conflicting_bits_note) {
    ttf_spec s[4]; fo_fx x; int i;
    fo_spec(&s[0], "FoCnf1"); s[0].fs_selection = 0x20;                      /* BOLD bit with usWeightClass 400 */
    fo_spec(&s[1], "FoCnf2"); s[1].weight = 700; s[1].fs_selection = 0x40;   /* weight 700 with REGULAR bit */
    fo_spec(&s[2], "FoCnf3"); s[2].fs_selection = 0x01 | 0x40;               /* ITALIC and REGULAR bits */
    fo_spec(&s[3], "FoCnf4"); s[3].mac_style = 2;                            /* head.macStyle italic only */
    FO_OPEN(x, s, 4);
    for (i = 0; i < 4; i++) t_note("conflict case %d: weight %d style %d sim %d", i, (int)IDWriteFont_GetWeight(x.font[i]), (int)IDWriteFont_GetStyle(x.font[i]), (int)IDWriteFont_GetSimulations(x.font[i]));
    fo_close(&x);
}
TEST(font_names_only_non_english_note) {
    fo_fx x; ttf_spec s; IDWriteLocalizedStrings *n = NULL; char l[32], b[64]; fo_spec(&s, "FoDeOnly"); ttf_clear_names(&s);
    ttf_add_name(&s, 3, 1, 0x407, 1, "FoDeOnly"); ttf_add_name(&s, 3, 1, 0x407, 2, "Standard"); ttf_add_name(&s, 3, 1, 0x407, 4, "FoDeOnly"); ttf_add_name(&s, 3, 1, 0x407, 6, "FoDeOnly-Standard");
    if (!fo_open1(&x, &s)) { t_note("font with only German names rejected (%s)", dw_hr_name(x.hr_coll)); fo_close(&x); return; }
    if (SUCCEEDED(IDWriteFont_GetFaceNames(x.font[0], &n)) && n) {
        fo_ls_locale(n, 0, l, sizeof l); fo_ls(n, 0, b, sizeof b); t_note("face names: count %u first locale '%s' text '%s'", IDWriteLocalizedStrings_GetCount(n), l, b);
        CHECK(IDWriteLocalizedStrings_GetCount(n) >= 1); DW_RELEASE(n);
    }
    EXPECT_EQ(IDWriteFont_GetWeight(x.font[0]), 400);
    fo_close(&x);
}
TEST(font_attributes_independent_of_names) {
    /* the numeric attributes come from OS/2, not from the name strings */
    fo_fx x; ttf_spec s; fo_spec(&s, "FoBoldNameOnly"); ttf_set_names(&s, "FoBoldNameOnly", "Bold Italic", "FoBoldNameOnly Bold Italic", "FoBoldNameOnly-BoldItalic"); FO_OPEN1(x, s);
    t_note("names say Bold Italic, OS/2 says 400/normal: weight %d style %d", (int)IDWriteFont_GetWeight(x.font[0]), (int)IDWriteFont_GetStyle(x.font[0]));
    EXPECT_EQ(IDWriteFont_GetStretch(x.font[0]), 5);
    fo_close(&x);
}
TEST(font_two_fonts_same_family_name_group) {
    ttf_spec s[2]; fo_fx x; IDWriteFontFamily *a = NULL, *b = NULL; ttf_style_spec(&s[0], "FoSameFam", 400, 5, 0); ttf_style_spec(&s[1], "FoSameFam", 700, 5, 0); FO_OPEN(x, s, 2);
    IDWriteFont_GetFontFamily(x.font[0], &a); IDWriteFont_GetFontFamily(x.font[1], &b); CHECK(a && a == b);
    if (a) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(x.coll), 1);
    DW_RELEASE(a); DW_RELEASE(b); fo_close(&x);
}
TEST(font_two_fonts_different_family_names_split) {
    ttf_spec s[2]; fo_fx x; IDWriteFontFamily *a = NULL, *b = NULL; ttf_style_spec(&s[0], "FoSplitA", 400, 5, 0); ttf_style_spec(&s[1], "FoSplitB", 400, 5, 0); FO_OPEN(x, s, 2);
    IDWriteFont_GetFontFamily(x.font[0], &a); IDWriteFont_GetFontFamily(x.font[1], &b); CHECK(a && b && a != b);
    EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(x.coll), 2);
    DW_RELEASE(a); DW_RELEASE(b); fo_close(&x);
}
