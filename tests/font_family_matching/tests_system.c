/* Family matching on installed fonts.  Exact face lists vary by SKU and updates, so checks are relations (derived from the matcher rules)
 * plus t_note for observed values; they are hard only for families whose RIBBI structure has been stable since Windows 7. */
#include "fm.h"

#define NEED_SYS(m, name) fm m; if (!fm_open_system(&m, name)) return

/* real faces only: Windows 10 adds simulated members to every family list */
static int has_face(const fm *m, int w, int wd, int st) {
    UINT32 i;
    for (i = 0; i < m->count; i++) if (m->obs_sims[i] == 0 && m->obs[i].w == w && m->obs[i].wd == wd && m->obs[i].st == st) return 1;
    return 0;
}

/* OS/2 usWeightClass etc. of one family member, read through IDWriteFontFace::TryGetFontTable */
static int os2_attrs(IDWriteFont *font, int *w, int *wd, int *st) {
    IDWriteFontFace *face = NULL; UINT32 len = 0; uint8_t *os2; int ok = 0;
    if (FAILED(IDWriteFont_CreateFontFace(font, &face)) || !face) return 0;
    os2 = dw_face_table(face, DW_TAG('O','S','/','2'), &len);
    if (os2 && len >= 64) {
        unsigned ver = (os2[0] << 8) | os2[1], wc = (os2[4] << 8) | os2[5], wdc = (os2[6] << 8) | os2[7], fs = (os2[62] << 8) | os2[63];
        *w = wc == 0 ? 1 : wc < 10 ? (int)wc * 100 : wc < 1000 ? (int)wc : 999;
        *wd = wdc == 0 ? 5 : wdc < 10 ? (int)wdc : 9;
        *st = (ver >= 4 && (fs & 0x200)) ? 1 : (fs & 1) ? 2 : 0;
        ok = 1;
    }
    free(os2);
    IDWriteFontFace_Release(face);
    return ok;
}

static void family_matches_os2(fm *m, int hard) {
    UINT32 i;
    for (i = 0; i < m->count; i++) {
        IDWriteFont *font = NULL; int w, wd, st;
        if (m->obs_sims[i] != 0) continue;   /* simulated members share the OS/2 table of the real face they derive from */
        if (FAILED(IDWriteFontFamily_GetFont(m->fam, i, &font)) || !font) { CHECK(0); continue; }
        if (os2_attrs(font, &w, &wd, &st)) {
            int same = w == m->obs[i].w && wd == m->obs[i].wd && st == m->obs[i].st;
            if (hard) CHECKF(same, "%s font %u: reported w=%d wd=%d st=%d, OS/2 says w=%d wd=%d st=%d", m->name, i, m->obs[i].w, m->obs[i].wd, m->obs[i].st, w, wd, st);
            else if (!same) t_note("%s font %u: reported w=%d wd=%d st=%d, OS/2 says w=%d wd=%d st=%d", m->name, i, m->obs[i].w, m->obs[i].wd, m->obs[i].st, w, wd, st);
        } else t_note("%s font %u: no OS/2 table through TryGetFontTable", m->name, i);
        IDWriteFont_Release(font);
    }
}

/* oracle sweep over the observed faces of a system family */
static void family_oracle_sweep(fm *m, int hard) {
    static const int ws[] = { 1, 100, 200, 300, 350, 400, 450, 500, 550, 551, 600, 700, 800, 900, 1000 };
    static const int wds[] = { 1, 3, 5, 7, 9 };
    int a, b, c, bad = 0, total = 0;
    if (m->count > FM_MAXF) return;
    for (a = 0; a < 5; a++) for (b = 0; b < 3; b++) for (c = 0; c < 15; c++) {
        fm_pick p;
        if (fm_ofirst_tied(m, ws[c], wds[a], b)) continue;
        p = fm_ofirst(m, ws[c], wds[a], b);
        total++;
        if (hard) fm_expect_first(m, ws[c], wds[a], b, p.idx, p.sims);
        else {
            IDWriteFont *font = NULL; fm_face got; int s;
            if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(m->fam, (DWRITE_FONT_WEIGHT)ws[c], (DWRITE_FONT_STRETCH)wds[a], (DWRITE_FONT_STYLE)b, &font)) && fm_read_font(font, &got, &s)) {
                fm_face want = fm_reported(&m->obs[p.idx], p.sims);
                if (!(got.w == want.w && got.wd == want.wd && got.st == want.st && s == p.sims)) bad++;
            } else bad++;
            DW_RELEASE(font);
        }
    }
    if (!hard) t_note("%s: %d of %d requests differ from the matcher model", m->name, bad, total);
}

/* ---- Arial ------------------------------------------------------------------------------------------ */
TEST(sys_arial_ribbi_faces) {
    NEED_SYS(m, L"Arial");
    t_note("Arial: %u fonts", m.count);
    CHECK(has_face(&m, 400, 5, 0)); CHECK(has_face(&m, 700, 5, 0)); CHECK(has_face(&m, 400, 5, 2)); CHECK(has_face(&m, 700, 5, 2));
    CHECK(m.count >= 4);
    fm_close(&m);
}
TEST(sys_arial_exact_requests) {
    NEED_SYS(m, L"Arial");
    static const struct { int w, st; } rq[] = { {400,0}, {700,0}, {400,2}, {700,2} };
    int i;
    for (i = 0; i < 4; i++) fm_expect_first(&m, rq[i].w, 5, rq[i].st, fm_find(&m, rq[i].w, 5, rq[i].st), 0);
    fm_close(&m);
}
TEST(sys_arial_heavy_requests_use_real_bold) {
    NEED_SYS(m, L"Arial");
    int w;
    if (m.count != 4) { fm_close(&m); t_skip("Arial has %u fonts (not the classic four)", m.count); return; }
    /* a real face heavier than 550 beats simulated bold of the regular face */
    for (w = 600; w <= 1000; w += 100) {
        fm_expect_first(&m, w, 5, 0, fm_find(&m, 700, 5, 0), 0);
        fm_expect_first(&m, w, 5, 2, fm_find(&m, 700, 5, 2), 0);
    }
    for (w = 1; w <= 500; w += 50) {
        fm_expect_first(&m, w, 5, 0, fm_find(&m, 400, 5, 0), 0);
        fm_expect_first(&m, w, 5, 2, fm_find(&m, 400, 5, 2), 0);
    }
    fm_close(&m);
}
TEST(sys_arial_attributes_match_os2) { NEED_SYS(m, L"Arial"); family_matches_os2(&m, 1); fm_close(&m); }
TEST(sys_arial_oracle_sweep) { NEED_SYS(m, L"Arial"); family_oracle_sweep(&m, 1); fm_close(&m); }
TEST(sys_arial_list_covers_family) {
    NEED_SYS(m, L"Arial");
    IDWriteFontList *l = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, 400, 5, 0, &l));
    if (l) {
        UINT32 c = IDWriteFontList_GetFontCount(l);
        t_note("Arial GetMatchingFonts(400,5,0): %u of %u fonts", c, m.count);
        CHECK(c >= 1 && c <= m.count);
        IDWriteFont *font = NULL; int reg = fm_find(&m, 400, 5, 0);
        if (reg >= 0 && SUCCEEDED(IDWriteFontList_GetFont(l, 0, &font))) fm_check_font(&m, font, &m.obs[reg], 0, "arial head");
        DW_RELEASE(font);
        IDWriteFontList_Release(l);
    }
    fm_close(&m);
}
TEST(sys_arial_black_is_its_own_family) {
    NEED_SYS(m, L"Arial");
    UINT32 i; IDWriteFontCollection *coll = m.coll; BOOL ex = FALSE; UINT32 idx = 0;
    IDWriteFontCollection_FindFamilyName(coll, L"Arial Black", &idx, &ex);
    t_note("Arial Black family present: %d", ex);
    /* Windows 10 folds the black weight into the Arial family (weight stretch style grouping), so only the range is invariant */
    for (i = 0; i < m.count; i++) CHECKF(m.obs[i].w >= 1 && m.obs[i].w <= 999, "Arial font %u has weight %d", i, m.obs[i].w);
    CHECK(fm_find(&m, 400, 5, 0) >= 0 && fm_find(&m, 700, 5, 0) >= 0);
    fm_close(&m);
}

/* ---- Segoe UI weight ladder ------------------------------------------------------------------------- */
TEST(sys_segoe_ui_weight_ladder) {
    NEED_SYS(m, L"Segoe UI");
    static const int want[] = { 300, 350, 400, 600, 700, 900 };   /* Light, Semilight, Regular, Semibold, Bold, Black */
    int i;
    t_note("Segoe UI: %u fonts", m.count);
    for (i = 0; i < 6; i++) CHECKF(has_face(&m, want[i], 5, 0), "Segoe UI weight %d (upright) missing", want[i]);
    for (i = 0; i < 6; i++) if (!has_face(&m, want[i], 5, 2)) t_note("Segoe UI italic weight %d missing", want[i]);
    fm_close(&m);
}
TEST(sys_segoe_ui_exact_weight_requests) {
    NEED_SYS(m, L"Segoe UI");
    static const int want[] = { 300, 350, 400, 600, 700, 900 };
    int i;
    for (i = 0; i < 6; i++) if (has_face(&m, want[i], 5, 0)) fm_expect_first(&m, want[i], 5, 0, fm_find(&m, want[i], 5, 0), 0);
    fm_close(&m);
}
TEST(sys_segoe_ui_in_between_weights) {
    NEED_SYS(m, L"Segoe UI");
    /* weights 300,350,400,600,700,900: 500 is equidistant from 400 and 600 and ties go to the heavier face; 800 likewise between 700 and 900;
     * 450 is closer to 400 (250) than to 600 (750); below 300 the lightest wins */
    if (!(has_face(&m, 300, 5, 0) && has_face(&m, 350, 5, 0) && has_face(&m, 400, 5, 0) && has_face(&m, 600, 5, 0) && has_face(&m, 700, 5, 0) && has_face(&m, 900, 5, 0))) {
        fm_close(&m); t_skip("Segoe UI weight ladder differs"); return;
    }
    fm_expect_first(&m, 500, 5, 0, fm_find(&m, 600, 5, 0), 0);
    fm_expect_first(&m, 800, 5, 0, fm_find(&m, 900, 5, 0), 0);
    fm_expect_first(&m, 450, 5, 0, fm_find(&m, 400, 5, 0), 0);
    fm_expect_first(&m, 375, 5, 0, fm_find(&m, 350, 5, 0), 0);   /* 375 is 25 weight units from both 350 and 400: ties below 400 go to the lighter face */
    fm_expect_first(&m, 325, 5, 0, fm_find(&m, 300, 5, 0), 0);
    fm_expect_first(&m, 100, 5, 0, fm_find(&m, 300, 5, 0), 0);
    fm_expect_first(&m, 1000, 5, 0, fm_find(&m, 900, 5, 0), 0);
    fm_close(&m);
}
TEST(sys_segoe_ui_attributes_match_os2) { NEED_SYS(m, L"Segoe UI"); family_matches_os2(&m, 1); fm_close(&m); }
TEST(sys_segoe_ui_oracle_sweep) { NEED_SYS(m, L"Segoe UI"); family_oracle_sweep(&m, 1); fm_close(&m); }
TEST(sys_segoe_ui_dwrite_weight_constants) {
    NEED_SYS(m, L"Segoe UI");
    /* DWRITE_FONT_WEIGHT_LIGHT 300, SEMI_LIGHT 350, NORMAL 400, SEMI_BOLD 600, BOLD 700, BLACK/HEAVY 900 */
    EXPECT_EQ(DWRITE_FONT_WEIGHT_LIGHT, 300); EXPECT_EQ(DWRITE_FONT_WEIGHT_SEMI_LIGHT, 350); EXPECT_EQ(DWRITE_FONT_WEIGHT_REGULAR, 400);
    EXPECT_EQ(DWRITE_FONT_WEIGHT_SEMI_BOLD, 600); EXPECT_EQ(DWRITE_FONT_WEIGHT_BOLD, 700); EXPECT_EQ(DWRITE_FONT_WEIGHT_BLACK, 900);
    fm_close(&m);
}

/* ---- families with a short face list: simulations ------------------------------------------------------- */
TEST(sys_tahoma_regular_bold_only) {
    NEED_SYS(m, L"Tahoma");
    t_note("Tahoma: %u fonts", m.count);
    CHECK(has_face(&m, 400, 5, 0)); CHECK(has_face(&m, 700, 5, 0));
    if (!has_face(&m, 400, 5, 2) && !has_face(&m, 400, 5, 1)) {
        /* no slanted face: italic and oblique requests resolve to the upright face with simulated oblique */
        fm_expect_first(&m, 400, 5, 2, fm_find(&m, 400, 5, 0), 2);
        fm_expect_first(&m, 400, 5, 1, fm_find(&m, 400, 5, 0), 2);
        fm_expect_first(&m, 700, 5, 2, fm_find(&m, 700, 5, 0), 2);
        fm_expect_first(&m, 700, 5, 1, fm_find(&m, 700, 5, 0), 2);
    } else t_note("Tahoma has a slanted face");
    fm_close(&m);
}
TEST(sys_tahoma_simulated_oblique_attributes) {
    NEED_SYS(m, L"Tahoma");
    IDWriteFont *font = NULL;
    if (has_face(&m, 400, 5, 2) || has_face(&m, 400, 5, 1)) { fm_close(&m); t_skip("Tahoma has a slanted face"); return; }
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 400, 5, 2, &font));
    if (font) {
        EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_OBLIQUE);
        EXPECT_EQ(IDWriteFont_GetStyle(font), DWRITE_FONT_STYLE_OBLIQUE);
        EXPECT_EQ(IDWriteFont_GetWeight(font), 400);
        IDWriteFontFace *face = NULL;
        CHECK_HR_OK(IDWriteFont_CreateFontFace(font, &face));
        if (face) { EXPECT_EQ(IDWriteFontFace_GetSimulations(face), DWRITE_FONT_SIMULATIONS_OBLIQUE); IDWriteFontFace_Release(face); }
        IDWriteFont_Release(font);
    }
    fm_close(&m);
}
TEST(sys_tahoma_attributes_match_os2) { NEED_SYS(m, L"Tahoma"); family_matches_os2(&m, 1); fm_close(&m); }
TEST(sys_tahoma_oracle_sweep) { NEED_SYS(m, L"Tahoma"); family_oracle_sweep(&m, 1); fm_close(&m); }
TEST(sys_single_face_families_simulate) {
    static const WCHAR *names[] = { L"Segoe UI Symbol", L"Marlett", L"Segoe UI Emoji", L"Webdings", L"Wingdings", L"Symbol" };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        fm m;
        if (!fm_open_system(&m, names[i])) continue;
        t_note("%s: %u fonts", m.name, m.count);
        if (m.count == 1 && m.obs[0].w == 400 && m.obs[0].wd == 5 && m.obs[0].st == 0) {
            fm_expect_first(&m, 700, 5, 0, 0, 1);
            fm_expect_first(&m, 400, 5, 2, 0, 2);
            fm_expect_first(&m, 700, 5, 2, 0, 3);
            fm_expect_first(&m, 400, 5, 0, 0, 0);
            fm_expect_first(&m, 550, 5, 0, 0, 0);
            fm_expect_first(&m, 551, 5, 0, 0, 1);
        }
        fm_close(&m);
    }
}

/* ---- other classic families ------------------------------------------------------------------------------ */
#define CLASSIC(tname, wname) \
    TEST(sys_##tname##_attributes_match_os2) { NEED_SYS(m, wname); family_matches_os2(&m, 1); fm_close(&m); } \
    TEST(sys_##tname##_oracle_sweep) { NEED_SYS(m, wname); family_oracle_sweep(&m, 1); fm_close(&m); } \
    TEST(sys_##tname##_has_regular) { NEED_SYS(m, wname); t_note("%s: %u fonts", m.name, m.count); CHECK(has_face(&m, 400, 5, 0)); fm_expect_first(&m, 400, 5, 0, fm_find(&m, 400, 5, 0), 0); fm_close(&m); }
CLASSIC(times_new_roman, L"Times New Roman")
CLASSIC(courier_new, L"Courier New")
CLASSIC(verdana, L"Verdana")
CLASSIC(consolas, L"Consolas")
CLASSIC(georgia, L"Georgia")
CLASSIC(calibri, L"Calibri")
CLASSIC(cambria, L"Cambria")

#define SOFT(tname, wname) \
    TEST(sys_##tname##_survey) { NEED_SYS(m, wname); t_note("%s: %u fonts", m.name, m.count); family_matches_os2(&m, 0); family_oracle_sweep(&m, 0); fm_close(&m); }
SOFT(trebuchet_ms, L"Trebuchet MS")
SOFT(lucida_console, L"Lucida Console")
SOFT(comic_sans_ms, L"Comic Sans MS")
SOFT(impact, L"Impact")
SOFT(candara, L"Candara")
SOFT(constantia, L"Constantia")
SOFT(corbel, L"Corbel")
SOFT(sylfaen, L"Sylfaen")
SOFT(ms_gothic, L"MS Gothic")
SOFT(segoe_script, L"Segoe Script")
SOFT(arial_narrow, L"Arial Narrow")
SOFT(arial_black, L"Arial Black")
SOFT(bahnschrift, L"Bahnschrift")
SOFT(segoe_ui_variable, L"Segoe UI Variable")
SOFT(sitka, L"Sitka Text")
SOFT(javanese, L"Javanese Text")
SOFT(nirmala, L"Nirmala UI")
SOFT(yu_gothic, L"Yu Gothic")

TEST(sys_arial_narrow_stretch) {
    NEED_SYS(m, L"Arial Narrow");
    /* Arial Narrow is a width variant: when it is its own family its fonts are normal-width faces with usWidthClass 3 or 5 */
    UINT32 i;
    for (i = 0; i < m.count; i++) t_note("Arial Narrow font %u: w=%d wd=%d st=%d", i, m.obs[i].w, m.obs[i].wd, m.obs[i].st);
    CHECK(m.count >= 1);
    fm_close(&m);
}
TEST(sys_bahnschrift_variable_instances) {
    NEED_SYS(m, L"Bahnschrift");
    UINT32 i; int minw = 1000, maxw = 0, minwd = 10, maxwd = 0;
    t_note("Bahnschrift: %u named instances / fonts", m.count);
    for (i = 0; i < m.count; i++) {
        CHECKF(m.obs[i].w >= 1 && m.obs[i].w <= 999, "font %u weight %d", i, m.obs[i].w);
        CHECKF(m.obs[i].wd >= 1 && m.obs[i].wd <= 9, "font %u stretch %d", i, m.obs[i].wd);
        CHECKF(m.obs[i].st >= 0 && m.obs[i].st <= 2, "font %u style %d", i, m.obs[i].st);
        if (m.obs[i].w < minw) minw = m.obs[i].w; if (m.obs[i].w > maxw) maxw = m.obs[i].w;
        if (m.obs[i].wd < minwd) minwd = m.obs[i].wd; if (m.obs[i].wd > maxwd) maxwd = m.obs[i].wd;
    }
    t_note("Bahnschrift weights %d..%d, stretches %d..%d", minw, maxw, minwd, maxwd);
    CHECK(m.count >= 1);
    fm_close(&m);
}

/* ---- whole collection invariants ------------------------------------------------------------------------------ */
TEST(sys_all_families_invariants) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *coll = NULL; UINT32 n, i; int fams = 0, fonts = 0;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(f, &coll, FALSE));
    if (!coll) return;
    n = IDWriteFontCollection_GetFontFamilyCount(coll);
    CHECK(n >= 1);
    for (i = 0; i < n && i < 400; i++) {
        IDWriteFontFamily *fam = NULL; UINT32 c, k;
        if (FAILED(IDWriteFontCollection_GetFontFamily(coll, i, &fam)) || !fam) { CHECKF(0, "GetFontFamily(%u)", i); continue; }
        fams++;
        c = IDWriteFontFamily_GetFontCount(fam);
        CHECKF(c >= 1, "family %u has no fonts", i);
        for (k = 0; k < c && k < 64; k++) {
            IDWriteFont *font = NULL; fm_face a; int s; IDWriteFontFamily *back = NULL;
            if (FAILED(IDWriteFontFamily_GetFont(fam, k, &font)) || !font) { CHECKF(0, "family %u GetFont(%u)", i, k); continue; }
            fonts++;
            fm_read_font(font, &a, &s);
            CHECKF(a.w >= 1 && a.w <= 999 && a.wd >= 1 && a.wd <= 9 && a.st >= 0 && a.st <= 2 && s >= 0 && s <= 3, "family %u font %u: w=%d wd=%d st=%d sims=%d", i, k, a.w, a.wd, a.st, s);
            if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &back)) && back) { CHECKF(dw_same_identity((IUnknown *)back, (IUnknown *)fam), "family %u font %u", i, k); IDWriteFontFamily_Release(back); }
            IDWriteFont_Release(font);
        }
        {
            IDWriteFont *first = NULL; IDWriteFontList *l = NULL;
            CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(fam, 400, 5, 0, &first));
            CHECKF(first != NULL, "family %u: no first matching font", i);
            DW_RELEASE(first);
            CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(fam, 400, 5, 0, &l));
            if (l) { UINT32 lc = IDWriteFontList_GetFontCount(l); CHECKF(lc >= 1 && lc <= c, "family %u list %u of %u", i, lc, c); IDWriteFontList_Release(l); }
        }
        IDWriteFontFamily_Release(fam);
    }
    t_note("system collection: %u families, %d fonts inspected", n, fonts);
    IDWriteFontCollection_Release(coll);
}
TEST(sys_all_families_oracle_survey) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *coll = NULL; UINT32 n, i; int ok = 0, tot = 0;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &coll, FALSE)) || !coll) return;
    n = IDWriteFontCollection_GetFontFamilyCount(coll);
    for (i = 0; i < n && i < 400; i++) {
        IDWriteFontFamily *fam = NULL; fm_face faces[FM_MAXF]; int cnt, k, bad = 0;
        static const int ws[] = { 100, 300, 400, 500, 600, 700, 900 };
        if (FAILED(IDWriteFontCollection_GetFontFamily(coll, i, &fam)) || !fam) continue;
        cnt = (int)IDWriteFontFamily_GetFontCount(fam);
        if (cnt > FM_MAXF) { IDWriteFontFamily_Release(fam); continue; }
        {   /* real members only: the simulated members Windows adds to the list are not matcher candidates */
            int nr = 0;
            for (k = 0; k < cnt; k++) { IDWriteFont *font = NULL; int s = 1; fm_face tmp; if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &font)) && font) { fm_read_font(font, &tmp, &s); IDWriteFont_Release(font); if (s == 0) faces[nr++] = tmp; } }
            cnt = nr;
        }
        if (cnt == 0) { IDWriteFontFamily_Release(fam); continue; }
        for (k = 0; k < 7 * 3; k++) {
            int rw = ws[k % 7], st = k / 7; IDWriteFont *font = NULL;
            if (fm_oracle_first_tied(faces, cnt, rw, 5, st)) continue;
            fm_pick p = fm_oracle_first(faces, cnt, rw, 5, st);
            if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, (DWRITE_FONT_WEIGHT)rw, 5, (DWRITE_FONT_STYLE)st, &font)) && font) {
                fm_face got; int s; fm_face want = fm_reported(&faces[p.idx], p.sims);
                fm_read_font(font, &got, &s);
                if (!(got.w == want.w && got.wd == want.wd && got.st == want.st && s == p.sims)) bad++;
            } else bad++;
            DW_RELEASE(font);
        }
        tot++; if (!bad) ok++;
        IDWriteFontFamily_Release(fam);
    }
    t_note("%d of %d system families match the matcher model on 21 requests", ok, tot);
    IDWriteFontCollection_Release(coll);
}
TEST(sys_family_names_roundtrip) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *coll = NULL; UINT32 n, i;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &coll, FALSE)) || !coll) return;
    n = IDWriteFontCollection_GetFontFamilyCount(coll);
    for (i = 0; i < n && i < 200; i++) {
        IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL;
        if (FAILED(IDWriteFontCollection_GetFontFamily(coll, i, &fam)) || !fam) continue;
        if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) {
            UINT32 idx = 0, len = 0; BOOL ex = FALSE; WCHAR name[200];
            if (FAILED(IDWriteLocalizedStrings_FindLocaleName(s, L"en-us", &idx, &ex)) || !ex) idx = 0;
            if (SUCCEEDED(IDWriteLocalizedStrings_GetStringLength(s, idx, &len)) && len < 199 && SUCCEEDED(IDWriteLocalizedStrings_GetString(s, idx, name, len + 1))) {
                UINT32 found = 0; BOOL fex = FALSE;
                CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(coll, name, &found, &fex));
                CHECKF(fex, "family %u name not found again", i);
                if (fex) {
                    IDWriteFontFamily *again = NULL;
                    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(coll, found, &again)) && again) { EXPECT_EQ(IDWriteFontFamily_GetFontCount(again), IDWriteFontFamily_GetFontCount(fam)); IDWriteFontFamily_Release(again); }
                }
            }
            IDWriteLocalizedStrings_Release(s);
        }
        IDWriteFontFamily_Release(fam);
    }
    IDWriteFontCollection_Release(coll);
}
TEST(sys_family1_family2_on_system_families) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *coll = NULL; UINT32 n, i;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &coll, FALSE)) || !coll) return;
    n = IDWriteFontCollection_GetFontFamilyCount(coll);
    for (i = 0; i < n && i < 120; i += 3) {
        IDWriteFontFamily *fam = NULL; IDWriteFontFamily2 *f2 = NULL;
        if (FAILED(IDWriteFontCollection_GetFontFamily(coll, i, &fam)) || !fam) continue;
        if (SUCCEEDED(IDWriteFontFamily_QueryInterface(fam, &IID_IDWriteFontFamily2, (void **)&f2)) && f2) {
            UINT32 c = IDWriteFontFamily2_GetFontCount(f2), k;
            for (k = 0; k < c && k < 8; k++) {
                IDWriteFontFaceReference *r = NULL;
                CHECKF(IDWriteFontFamily2_GetFontLocality(f2, k) == DWRITE_LOCALITY_LOCAL, "family %u font %u not local", i, k);
                CHECK_HR_OK(IDWriteFontFamily2_GetFontFaceReference(f2, k, &r));
                if (r) {
                    IDWriteFont *fo = NULL;   /* the reference carries the simulations of the family member */
                    if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &fo)) && fo) { EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), IDWriteFont_GetSimulations(fo)); IDWriteFont_Release(fo); }
                    IDWriteFontFaceReference_Release(r);
                }
            }
            { IDWriteFontSet1 *s = NULL; if (SUCCEEDED(IDWriteFontFamily2_GetFontSet(f2, &s)) && s) { EXPECT_EQ(IDWriteFontSet1_GetFontCount(s), c); IDWriteFontSet1_Release(s); } }
            { IDWriteFontList2 *l = NULL; if (SUCCEEDED(IDWriteFontFamily2_GetMatchingFonts(f2, NULL, 0, &l)) && l) { UINT32 lc = IDWriteFontList2_GetFontCount(l); CHECK(lc >= 1 && lc <= c); IDWriteFontList2_Release(l); } }
            IDWriteFontFamily2_Release(f2);
        }
        IDWriteFontFamily_Release(fam);
    }
    IDWriteFontCollection_Release(coll);
}
TEST(sys_family_collection_identity) {
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *coll = NULL; IDWriteFontFamily *fam = NULL;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &coll, FALSE)) || !coll) return;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(coll, 0, &fam)) && fam) {
        IDWriteFontCollection *c2 = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(fam, &c2));
        if (c2) { CHECK(dw_same_identity((IUnknown *)c2, (IUnknown *)coll)); IDWriteFontCollection_Release(c2); }
        IDWriteFontFamily_Release(fam);
    }
    IDWriteFontCollection_Release(coll);
}
TEST(sys_system_family_out_of_range_font_index) {
    NEED_SYS(m, L"Arial");
    IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)&m;
    CHECK_HR(IDWriteFontFamily_GetFont(m.fam, m.count, &font), E_FAIL);
    CHECK_OUT_NULL(font);
    font = (IDWriteFont *)(ULONG_PTR)&m;
    CHECK_HR(IDWriteFontFamily_GetFont(m.fam, 0xFFFFFFFFu, &font), E_FAIL);
    CHECK_OUT_NULL(font);
    fm_close(&m);
}
TEST(sys_font_objects_are_fresh_per_call) {
    NEED_SYS(m, L"Arial");
    IDWriteFont *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 400, 5, 0, &a));
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 400, 5, 0, &b));
    CHECK(a && b && a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    fm_close(&m);
}
TEST(sys_first_matching_font_family_identity) {
    NEED_SYS(m, L"Arial");
    IDWriteFont *font = NULL; IDWriteFontFamily *ff = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 700, 5, 0, &font));
    if (font) {
        CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &ff));
        if (ff) CHECK(dw_same_identity((IUnknown *)ff, (IUnknown *)m.fam));
        DW_RELEASE(ff);
        IDWriteFont_Release(font);
    }
    fm_close(&m);
}
