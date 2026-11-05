/* DirectWrite family font matching (IDWriteFontFamily::GetFirstMatchingFont / GetMatchingFonts) on synthetic families.
 *
 * The Windows 10 matcher (FontSet::FindNearestMatchingWwsFont, FontSet::ApplySimulationsForWwsFallback, MatchingFont::MatchingFont,
 * MatchingStyle::IsBetterMatch) works like this, and the tables below are exact consequences of it:
 *   - every candidate is mapped to a point (stretch*1100, style*700, weight*5) relative to (normal, normal, 400);
 *     the request maps to (requestStretch-5)*1100, requestStyle*700, (requestWeight-400)*5; the candidate with the smallest squared
 *     Euclidean distance wins (stretch, style and weight are NOT strictly lexicographic, but a stretch step is 1100, a style step 700
 *     and a weight step of 100 only 500);
 *   - request style != NORMAL and a NORMAL candidate: the candidate is treated as simulated OBLIQUE (style distance 700) and the
 *     result carries DWRITE_FONT_SIMULATIONS_OBLIQUE;
 *   - request weight > 550 and a candidate lighter than 551 and lighter than (request - 100): simulated BOLD, its effective weight
 *     becomes max(700, weight) and the result carries DWRITE_FONT_SIMULATIONS_BOLD;
 *   - equal distance: larger dot product with the request wins (heavier above weight 400, lighter below it, wider/narrower likewise),
 *     then larger stretch, style, weight distance;
 *   - a real oblique candidate beats a simulated oblique one of equal effective weight, and a real face heavier than 550 beats a
 *     simulated bold one with the same effective style;
 *   - an exact (weight, stretch, style) match is returned immediately;
 *   - no request value is validated: any weight/stretch/style integer returns S_OK.
 */
#include "fm.h"
#include "fm_tables.h"

/* ---- table driver -------------------------------------------------------------------------------- */
static void run_table(const char *famname, const fm_face *fs, int nf, const mrow *rows, int nr) {
    fm m; int r;
    if (!fm_open(&m, famname, fs, nf, NULL)) return;
    EXPECT_EQ(m.count, nf);
    for (r = 0; r < nr; r++) {
        const mrow *w = &rows[r]; int idx = fm_find(&m, w->ew, w->ewd, w->est);
        CHECKF(idx >= 0, "row %d: expected face (%d,%d,%d) not in family", r, w->ew, w->ewd, w->est);
        if (idx < 0) continue;
        fm_expect_first(&m, w->rw, w->rwd, w->rst, idx, w->sims);
        {
            /* the head of GetMatchingFonts is the same face with the same simulations */
            IDWriteFontList *l = NULL; IDWriteFont *font = NULL; char tag[96];
            snprintf(tag, sizeof tag, "list head%s", fm_req_name(w->rw, w->rwd, w->rst));
            if (SUCCEEDED(IDWriteFontFamily_GetMatchingFonts(m.fam, (DWRITE_FONT_WEIGHT)w->rw, (DWRITE_FONT_STRETCH)w->rwd, (DWRITE_FONT_STYLE)w->rst, &l)) && l) {
                CHECKF(IDWriteFontList_GetFontCount(l) >= 1, "%s empty", tag);
                if (SUCCEEDED(IDWriteFontList_GetFont(l, 0, &font))) fm_check_font(&m, font, &m.obs[idx], w->sims, tag);
                DW_RELEASE(font);
            } else CHECKF(0, "%s: GetMatchingFonts failed", tag);
            DW_RELEASE(l);
        }
    }
    fm_close(&m);
}
#define TABLE_TEST(name) TEST(mt_table_##name) { run_table("FmT" #name, F_##name, FM_N(F_##name), T_##name, FM_N(T_##name)); }

/* weight ladder 100..900: nearest weight, ties go to the heavier face from 400 up and to the lighter face below 400 */
TABLE_TEST(W9)
/* regular/bold/italic/bold italic: oblique requests resolve to the normal face with simulated oblique (zero style distance) */
TABLE_TEST(RIBBI)
/* a single regular face: bold simulated above weight 550, oblique simulated for any non-normal style request */
TABLE_TEST(ONE)
/* gaps in the weight ladder */
TABLE_TEST(SPARSE)
/* condensed / normal / expanded: ties go to the narrower face below stretch 5 and to the wider one above it */
TABLE_TEST(STRETCH)
TABLE_TEST(WIDE9)
/* style families */
TABLE_TEST(NORM_OBL)
TABLE_TEST(NORM_ITAL)
TABLE_TEST(OBL_ITAL)
TABLE_TEST(NORM_OBL_ITAL)
TABLE_TEST(OBL_ONLY)
TABLE_TEST(ITAL_ONLY)
/* axes against each other */
TABLE_TEST(CROSS1)
TABLE_TEST(CROSS2)
TABLE_TEST(W5_ITAL)
/* a real black face beats a simulated bold regular */
TABLE_TEST(REG_BLACK)

/* ---- the model against the tables (host-independent) ------------------------------------------------ */
static void model_table(const fm_face *fs, int nf, const mrow *rows, int nr, const char *name) {
    int r;
    for (r = 0; r < nr; r++) {
        fm_pick p = fm_oracle_first(fs, nf, rows[r].rw, rows[r].rwd, rows[r].rst);
        CHECKF(p.idx >= 0 && fs[p.idx].w == rows[r].ew && fs[p.idx].wd == rows[r].ewd && fs[p.idx].st == rows[r].est && p.sims == rows[r].sims,
               "%s row %d %s: model gives face %d sims %d", name, r, fm_req_name(rows[r].rw, rows[r].rwd, rows[r].rst), p.idx, p.sims);
        CHECKF(!fm_oracle_first_tied(fs, nf, rows[r].rw, rows[r].rwd, rows[r].rst), "%s row %d tie", name, r);
    }
}
TEST(mt_model_agrees_with_tables) {
#define MT(N) model_table(F_##N, FM_N(F_##N), T_##N, FM_N(T_##N), #N);
    MT(W9) MT(RIBBI) MT(ONE) MT(SPARSE) MT(STRETCH) MT(WIDE9) MT(NORM_OBL) MT(NORM_ITAL) MT(OBL_ITAL) MT(NORM_OBL_ITAL)
    MT(OBL_ONLY) MT(ITAL_ONLY) MT(CROSS1) MT(CROSS2) MT(W5_ITAL) MT(REG_BLACK)
#undef MT
}

/* ---- exact matches -------------------------------------------------------------------------------- */
static int grid27(fm_face *f) {
    static const int w[] = { 300, 400, 700 }, wd[] = { 3, 5, 7 }, st[] = { 0, 1, 2 };
    int a, b, c, n = 0;
    for (a = 0; a < 3; a++) for (b = 0; b < 3; b++) for (c = 0; c < 3; c++) f[n++] = (fm_face){ w[a], wd[b], st[c] };
    return n;
}

TEST(mt_exact_match_returns_that_face_grid27) {
    fm_face f[FM_MAXF]; int n = grid27(f), i; fm m;
    if (!fm_open(&m, "FmGrid", f, n, NULL)) return;
    EXPECT_EQ(m.count, 27);
    for (i = 0; i < n; i++) fm_expect_first(&m, f[i].w, f[i].wd, f[i].st, fm_find(&m, f[i].w, f[i].wd, f[i].st), 0);
    fm_close(&m);
}

TEST(mt_exact_match_is_independent_of_family_order) {
    /* the same faces enumerated in two different orders resolve every exact request to the same face */
    fm_face a[] = { {300,5,0}, {400,5,0}, {700,5,0}, {400,5,2} }, b[] = { {400,5,2}, {700,5,0}, {400,5,0}, {300,5,0} };
    fm ma, mb; int i;
    if (!fm_open(&ma, "FmOrdA", a, 4, NULL)) return;
    if (!fm_open(&mb, "FmOrdB", b, 4, NULL)) { fm_close(&ma); return; }
    for (i = 0; i < 4; i++) {
        fm_expect_first(&ma, a[i].w, a[i].wd, a[i].st, fm_find(&ma, a[i].w, a[i].wd, a[i].st), 0);
        fm_expect_first(&mb, a[i].w, a[i].wd, a[i].st, fm_find(&mb, a[i].w, a[i].wd, a[i].st), 0);
    }
    fm_close(&mb); fm_close(&ma);
}

/* ---- weight rules --------------------------------------------------------------------------------- */
static void pair_case(int w1, int w2, int rw, int expect_w) {
    fm_face f[2]; fm m; char nm[40];
    f[0] = (fm_face){ w1, 5, 0 }; f[1] = (fm_face){ w2, 5, 0 };
    snprintf(nm, sizeof nm, "FmPair%d_%d", w1, w2);
    if (!fm_open(&m, nm, f, 2, NULL)) return;
    fm_expect_first(&m, rw, 5, 0, fm_find(&m, expect_w, 5, 0), 0);
    fm_close(&m);
}
TEST(mt_weight_tie_at_or_above_400_prefers_heavier) {
    /* request equidistant from both faces: the dot product with the request vector decides, so the heavier face wins for weights >= 400 */
    pair_case(400, 500, 450, 500);
    pair_case(400, 600, 500, 600);
    pair_case(200, 600, 400, 600);
    pair_case(300, 500, 400, 500);
    pair_case(500, 600, 550, 600);
    pair_case(100, 900, 500, 900);
}
TEST(mt_weight_tie_below_400_prefers_lighter) {
    pair_case(300, 400, 350, 300);
    pair_case(200, 400, 300, 200);
    pair_case(100, 300, 200, 100);
    pair_case(100, 500, 300, 100);
    pair_case(200, 300, 250, 200);
}
TEST(mt_weight_tie_straddling_400) {
    /* request exactly 400, faces 300 and 500: the dot product with the zero weight vector is 0 for both, so the larger weight distance wins: 500 */
    pair_case(300, 500, 400, 500);
    pair_case(200, 600, 400, 600);
    pair_case(100, 700, 400, 700);
}
TEST(mt_weight_just_off_the_midpoint) {
    pair_case(400, 500, 449, 400);
    pair_case(400, 500, 451, 500);
    pair_case(300, 400, 349, 300);
    pair_case(300, 400, 351, 400);
    pair_case(600, 800, 699, 600);
    pair_case(600, 800, 701, 800);
}

/* a lone face is the answer for every weight; the simulation flags follow the thresholds exactly */
static void bold_threshold(int fw) {
    fm_face f = { fw, 5, 0 }; fm m; char nm[40]; int rw;
    snprintf(nm, sizeof nm, "FmBoldThr%d", fw);
    if (!fm_open(&m, nm, &f, 1, NULL)) return;
    for (rw = 1; rw <= 1000; rw++) {
        /* MatchingFont::MatchingFont: bold is simulated when request > 550, face < 551 and face < request - 100 */
        int sim = (rw > 550 && fw < 551 && fw < rw - 100) ? 1 : 0;
        fm_expect_first(&m, rw, 5, 0, 0, sim);
    }
    fm_close(&m);
}
#define BOLD_TEST(w) TEST(mt_bold_simulation_thresholds_face_##w) { bold_threshold(w); }
BOLD_TEST(100) BOLD_TEST(300) BOLD_TEST(400) BOLD_TEST(450) BOLD_TEST(500) BOLD_TEST(540) BOLD_TEST(550) BOLD_TEST(551) BOLD_TEST(600) BOLD_TEST(700)

TEST(mt_bold_simulation_boundary_points) {
    /* font 400: simulated for requests 551..., never for 550 and below; font 450 becomes simulated first at 551 (450 < 451) */
    fm_face f = { 400, 5, 0 }; fm m;
    if (!fm_open(&m, "FmBoldPt", &f, 1, NULL)) return;
    fm_expect_first(&m, 550, 5, 0, 0, 0);
    fm_expect_first(&m, 551, 5, 0, 0, 1);
    fm_expect_first(&m, 500, 5, 0, 0, 0);
    fm_expect_first(&m, 700, 5, 0, 0, 1);
    fm_expect_first(&m, 400, 5, 0, 0, 0);
    fm_close(&m);
}

TEST(mt_bold_simulation_reports_weight_700) {
    fm_face f = { 400, 5, 0 }; fm m; int rw;
    if (!fm_open(&m, "FmBold700", &f, 1, NULL)) return;
    for (rw = 551; rw <= 999; rw += 56) {
        IDWriteFont *font = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, (DWRITE_FONT_WEIGHT)rw, 5, 0, &font));
        if (font) {
            /* FontFaceLight::GetFontWeight returns 700 once BOLD is simulated, whatever was requested */
            EXPECT_EQ(IDWriteFont_GetWeight(font), 700);
            EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_BOLD);
            EXPECT_EQ(IDWriteFont_GetStyle(font), DWRITE_FONT_STYLE_NORMAL);
        }
        DW_RELEASE(font);
    }
    fm_close(&m);
}

TEST(mt_bold_face_is_never_simulated_further) {
    fm_face f = { 700, 5, 0 }; fm m; int rw;
    if (!fm_open(&m, "FmBoldReal", &f, 1, NULL)) return;
    for (rw = 1; rw <= 1000; rw += 9) fm_expect_first(&m, rw, 5, 0, 0, 0);
    fm_close(&m);
}

/* ---- style rules ----------------------------------------------------------------------------------- */
TEST(mt_oblique_simulated_for_each_nonnormal_style_on_normal_face) {
    fm_face f = { 400, 5, 0 }; fm m; int st;
    if (!fm_open(&m, "FmObl", &f, 1, NULL)) return;
    fm_expect_first(&m, 400, 5, 0, 0, 0);
    for (st = 1; st <= 2; st++) fm_expect_first(&m, 400, 5, st, 0, 2);
    for (st = 3; st <= 6; st++) fm_expect_first(&m, 400, 5, st, 0, 2);   /* any non-zero style value */
    fm_close(&m);
}
TEST(mt_oblique_simulation_reports_oblique_style) {
    fm_face f = { 400, 5, 0 }; fm m; int st;
    if (!fm_open(&m, "FmOblRep", &f, 1, NULL)) return;
    for (st = 1; st <= 2; st++) {
        IDWriteFont *font = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m.fam, 400, 5, (DWRITE_FONT_STYLE)st, &font));
        if (font) {
            EXPECT_EQ(IDWriteFont_GetStyle(font), DWRITE_FONT_STYLE_OBLIQUE);   /* even for an ITALIC request */
            EXPECT_EQ(IDWriteFont_GetSimulations(font), DWRITE_FONT_SIMULATIONS_OBLIQUE);
            EXPECT_EQ(IDWriteFont_GetWeight(font), 400);
        }
        DW_RELEASE(font);
    }
    fm_close(&m);
}
TEST(mt_bold_and_oblique_simulated_together) {
    fm_face f = { 400, 5, 0 }; fm m;
    if (!fm_open(&m, "FmBoldObl", &f, 1, NULL)) return;
    fm_expect_first(&m, 700, 5, 2, 0, 3);
    fm_expect_first(&m, 700, 5, 1, 0, 3);
    fm_expect_first(&m, 900, 5, 2, 0, 3);
    fm_expect_first(&m, 551, 5, 1, 0, 3);
    fm_expect_first(&m, 550, 5, 1, 0, 2);
    fm_close(&m);
}
TEST(mt_italic_face_is_not_simulated_when_normal_requested) {
    fm_face f = { 400, 5, 2 }; fm m; int rw, st;
    if (!fm_open(&m, "FmItalOnly", &f, 1, NULL)) return;
    /* the only face is italic: normal requests return it unchanged (no oblique flag, no un-slanting); style distance is irrelevant for a lone face */
    for (st = 0; st <= 2; st++) fm_expect_first(&m, 400, 5, st, 0, 0);
    for (rw = 600; rw <= 900; rw += 100) fm_expect_first(&m, rw, 5, 0, 0, 1);
    fm_close(&m);
}
TEST(mt_oblique_face_only_is_not_resimulated) {
    fm_face f = { 400, 5, 1 }; fm m; int st;
    if (!fm_open(&m, "FmOblOnly", &f, 1, NULL)) return;
    for (st = 0; st <= 2; st++) fm_expect_first(&m, 400, 5, st, 0, 0);
    fm_expect_first(&m, 700, 5, 1, 0, 1);
    fm_close(&m);
}
TEST(mt_real_oblique_beats_simulated_oblique) {
    /* normal 400 and oblique 400: an oblique request returns the real oblique face (equal effective weight, flags differ) */
    fm_face f[] = { {400,5,0}, {400,5,1} }; fm m; int w;
    if (!fm_open(&m, "FmRealObl", f, 2, NULL)) return;
    for (w = 100; w <= 500; w += 100) fm_expect_first(&m, w, 5, 1, fm_find(&m, 400, 5, 1), 0);
    fm_close(&m);
}
TEST(mt_style_preference_for_italic_request) {
    /* italic request: italic > oblique > normal(simulated) */
    fm_face a[] = { {400,5,0}, {400,5,1}, {400,5,2} }; fm m;
    if (!fm_open(&m, "FmStyIt", a, 3, NULL)) return;
    fm_expect_first(&m, 400, 5, 2, fm_find(&m, 400, 5, 2), 0);
    fm_expect_first(&m, 400, 5, 1, fm_find(&m, 400, 5, 1), 0);
    fm_expect_first(&m, 400, 5, 0, fm_find(&m, 400, 5, 0), 0);
    fm_close(&m);
}
TEST(mt_normal_request_prefers_oblique_over_italic) {
    /* normal request, family without normal: oblique (style distance 700) beats italic (1400) */
    fm_face a[] = { {400,5,1}, {400,5,2} }; fm m;
    if (!fm_open(&m, "FmNoNorm", a, 2, NULL)) return;
    fm_expect_first(&m, 400, 5, 0, fm_find(&m, 400, 5, 1), 0);
    fm_close(&m);
}
TEST(mt_style_distance_competes_with_weight_distance) {
    /* a style step (700) is worth 140 weight units: an italic face 100 weight units away wins over a normal one 200 away, not the reverse */
    fm_face a[] = { {400,5,2}, {600,5,0} }; fm m;
    if (!fm_open(&m, "FmStyW", a, 2, NULL)) return;
    fm_expect_first(&m, 500, 5, 0, fm_find(&m, 600, 5, 0), 0);   /* 600: 500 away; italic 400: sqrt(1400^2 + 500^2) away */
    fm_expect_first(&m, 400, 5, 0, fm_find(&m, 600, 5, 0), 0);   /* 600: 1000 away; italic 400: 1400 away */
    fm_expect_first(&m, 400, 5, 2, fm_find(&m, 400, 5, 2), 0);
    fm_close(&m);
}

/* ---- stretch rules ----------------------------------------------------------------------------------- */
TEST(mt_stretch_beats_small_weight_difference) {
    /* one stretch step (1100) equals 220 weight units */
    fm_face a[] = { {400,5,0}, {500,4,0} }; fm m;
    if (!fm_open(&m, "FmStrW", a, 2, NULL)) return;
    fm_expect_first(&m, 500, 5, 0, fm_find(&m, 400, 5, 0), 0);   /* 500/4: stretch off by 1100; 400/5: weight off by 500 */
    fm_expect_first(&m, 500, 4, 0, fm_find(&m, 500, 4, 0), 0);
    fm_close(&m);
}
TEST(mt_large_weight_difference_beats_stretch) {
    fm_face a[] = { {400,5,0}, {900,4,0} }; fm m;
    if (!fm_open(&m, "FmWStr", a, 2, NULL)) return;
    /* 900/4 is 1100 off in stretch; 400/5 is simulated bold (effective weight 700), 1000 off in weight: the simulated face wins */
    fm_expect_first(&m, 900, 5, 0, fm_find(&m, 400, 5, 0), 1);
    fm_expect_first(&m, 900, 4, 0, fm_find(&m, 900, 4, 0), 0);
    fm_close(&m);
}
TEST(mt_stretch_extremes) {
    fm_face a[] = { {400,1,0}, {400,9,0} }; fm m; int wd;
    if (!fm_open(&m, "FmStrExt", a, 2, NULL)) return;
    for (wd = 0; wd <= 4; wd++) fm_expect_first(&m, 400, wd, 0, fm_find(&m, 400, 1, 0), 0);
    for (wd = 5; wd <= 10; wd++) fm_expect_first(&m, 400, wd, 0, fm_find(&m, 400, 9, 0), 0);
    fm_close(&m);
}

/* ---- oracle sweeps ---------------------------------------------------------------------------------- */
TEST(mt_sweep_grid27_first_all_requests) {
    fm_face f[FM_MAXF]; int n = grid27(f); fm m;
    static const int ws[] = { 0, 1, 100, 250, 299, 300, 301, 350, 399, 400, 401, 450, 500, 549, 550, 551, 600, 650, 700, 701, 800, 999, 1000 };
    int wd, st, i;
    if (!fm_open(&m, "FmSwGrid", f, n, NULL)) return;
    for (wd = 0; wd <= 10; wd++) for (st = 0; st <= 2; st++) for (i = 0; i < (int)(sizeof ws / sizeof ws[0]); i++) {
        fm_pick p = fm_oracle_first(m.obs, (int)m.count, ws[i], wd, st);
        if (fm_oracle_first_tied(m.obs, (int)m.count, ws[i], wd, st)) continue;
        fm_expect_first(&m, ws[i], wd, st, p.idx, p.sims);
    }
    fm_close(&m);
}
TEST(mt_sweep_grid27_lists) {
    fm_face f[FM_MAXF]; int n = grid27(f); fm m; int k = 0;
    static const int ws[] = { 100, 350, 400, 500, 550, 551, 700, 900 };
    int wd, st, i;
    if (!fm_open(&m, "FmSwGridL", f, n, NULL)) return;
    for (wd = 1; wd <= 9; wd += 2) for (st = 0; st <= 2; st++) for (i = 0; i < (int)(sizeof ws / sizeof ws[0]); i++)
        fm_check_request(&m, ws[i], wd, st, (k++ % 3) == 0);
    fm_close(&m);
}
TEST(mt_sweep_w9_every_weight) {
    fm_face f[9]; fm m; int i, w;
    for (i = 0; i < 9; i++) f[i] = (fm_face){ 100 * (i + 1), 5, 0 };
    if (!fm_open(&m, "FmSwW9", f, 9, NULL)) return;
    for (w = 0; w <= 1100; w++) {
        fm_pick p = fm_oracle_first(m.obs, (int)m.count, w, 5, 0);
        if (fm_oracle_first_tied(m.obs, (int)m.count, w, 5, 0)) continue;
        fm_expect_first(&m, w, 5, 0, p.idx, p.sims);
    }
    fm_close(&m);
}
TEST(mt_sweep_w9_every_weight_italic_request) {
    fm_face f[9]; fm m; int i, w;
    for (i = 0; i < 9; i++) f[i] = (fm_face){ 100 * (i + 1), 5, 0 };
    if (!fm_open(&m, "FmSwW9I", f, 9, NULL)) return;
    for (w = 0; w <= 1100; w += 3) {
        fm_pick p = fm_oracle_first(m.obs, (int)m.count, w, 5, 2);
        if (fm_oracle_first_tied(m.obs, (int)m.count, w, 5, 2)) continue;
        fm_expect_first(&m, w, 5, 2, p.idx, p.sims);
    }
    fm_close(&m);
}
TEST(mt_sweep_ribbi_weights_styles) {
    fm m; int w, st, wd;
    if (!fm_open(&m, "FmSwRibbi", F_RIBBI, 4, NULL)) return;
    for (wd = 3; wd <= 7; wd += 2) for (st = 0; st <= 2; st++) for (w = 1; w <= 1000; w += 7) fm_check_request(&m, w, wd, st, (w % 5) == 1);
    fm_close(&m);
}
TEST(mt_sweep_one_face_lists_have_one_entry) {
    fm_face f = { 400, 5, 0 }; fm m; int w, st;
    if (!fm_open(&m, "FmSwOne", &f, 1, NULL)) return;
    for (st = 0; st <= 2; st++) for (w = 1; w <= 1000; w += 11) {
        IDWriteFontList *l = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m.fam, (DWRITE_FONT_WEIGHT)w, 5, (DWRITE_FONT_STYLE)st, &l));
        if (l) {
            IDWriteFont *font = NULL;
            EXPECT_EQ(IDWriteFontList_GetFontCount(l), 1);
            if (SUCCEEDED(IDWriteFontList_GetFont(l, 0, &font))) {
                fm_pick p = fm_oracle_first(m.obs, 1, w, 5, st);
                fm_check_font(&m, font, &m.obs[0], p.sims, "one-face list");
            }
            DW_RELEASE(font); IDWriteFontList_Release(l);
        }
    }
    fm_close(&m);
}
TEST(mt_sweep_32_weights) {
    fm_face f[FM_MAXF]; fm m; int i, w;
    for (i = 0; i < 32; i++) f[i] = (fm_face){ 30 + i * 31, 5, 0 };
    if (!fm_open(&m, "FmSw32", f, 32, NULL)) return;
    EXPECT_EQ(m.count, 32);
    for (w = 0; w <= 1050; w += 5) fm_check_request(&m, w, 5, 0, (w % 50) == 0);
    fm_close(&m);
}
TEST(mt_sweep_condensed_expanded_italics) {
    fm_face f[FM_MAXF]; int n = 0, wd, st; fm m; int w;
    for (wd = 3; wd <= 7; wd += 2) for (st = 0; st <= 2; st += 2) { f[n++] = (fm_face){ 400, wd, st }; f[n++] = (fm_face){ 700, wd, st }; }
    if (!fm_open(&m, "FmSwCE", f, n, NULL)) return;
    EXPECT_EQ(m.count, n);
    for (wd = 0; wd <= 10; wd++) for (st = 0; st <= 2; st++) for (w = 100; w <= 900; w += 100) fm_check_request(&m, w, wd, st, (wd % 4) == 0);
    fm_close(&m);
}

/* deterministic pseudo-random families */
static unsigned lcg(unsigned *x) { *x = *x * 1103515245u + 12345u; return (*x >> 16) & 0x7FFF; }
static void random_families(unsigned seed, int nfam) {
    static const int pw[] = { 100, 200, 300, 350, 400, 500, 600, 700, 800, 900 }, pwd[] = { 3, 4, 5, 6, 7 };
    static const int rwv[] = { 1, 150, 250, 340, 380, 400, 420, 480, 520, 560, 575, 620, 680, 740, 790, 860, 950, 1000 };
    unsigned x = seed; int fi;
    for (fi = 0; fi < nfam; fi++) {
        fm_face f[FM_MAXF]; int n = 0, want = 2 + (int)(lcg(&x) % 7), tries = 0, k; fm m; char nm[40];
        while (n < want && tries++ < 200) {
            fm_face c = { pw[lcg(&x) % 10], pwd[lcg(&x) % 5], (int)(lcg(&x) % 3) };
            int dup = 0;
            for (k = 0; k < n; k++) if (f[k].w == c.w && f[k].wd == c.wd && f[k].st == c.st) dup = 1;
            if (!dup) f[n++] = c;
        }
        snprintf(nm, sizeof nm, "FmRnd%u_%d", seed, fi);
        if (!fm_open(&m, nm, f, n, NULL)) return;
        for (k = 0; k < 22; k++) {
            int rw = rwv[lcg(&x) % 18], rwd = 1 + (int)(lcg(&x) % 9), rst = (int)(lcg(&x) % 3);
            fm_check_request(&m, rw, rwd, rst, k % 4 == 0);
        }
        fm_close(&m);
    }
}
#define RND_TEST(s) TEST(mt_random_families_seed_##s) { random_families(s, 5); }
RND_TEST(1) RND_TEST(2) RND_TEST(3) RND_TEST(4) RND_TEST(5) RND_TEST(6) RND_TEST(7) RND_TEST(8)

/* ---- list order ------------------------------------------------------------------------------------- */
typedef struct { int w, wd, st, sims; } lrow;
static void list_expect(fm *m, int rw, int rwd, int rst, const lrow *e, int ne) {
    IDWriteFontList *l = NULL; UINT32 cnt, i; char tag[96];
    snprintf(tag, sizeof tag, "list%s", fm_req_name(rw, rwd, rst));
    CHECK_HR_OK(IDWriteFontFamily_GetMatchingFonts(m->fam, (DWRITE_FONT_WEIGHT)rw, (DWRITE_FONT_STRETCH)rwd, (DWRITE_FONT_STYLE)rst, &l));
    if (!l) return;
    cnt = IDWriteFontList_GetFontCount(l);
    CHECKF((int)cnt == ne, "%s: %u entries, expected %d", tag, cnt, ne);
    for (i = 0; i < cnt && (int)i < ne; i++) {
        IDWriteFont *font = NULL; int idx = fm_find(m, e[i].w, e[i].wd, e[i].st); char t2[120];
        snprintf(t2, sizeof t2, "%s[%u]", tag, i);
        if (SUCCEEDED(IDWriteFontList_GetFont(l, i, &font)) && idx >= 0) fm_check_font(m, font, &m->obs[idx], e[i].sims, t2);
        else CHECKF(0, "%s: GetFont failed or face missing", t2);
        DW_RELEASE(font);
    }
    IDWriteFontList_Release(l);
}
TEST(mt_list_order_ribbi_regular_request) {
    fm m;
    if (!fm_open(&m, "FmLoRibbi1", F_RIBBI, 4, NULL)) return;
    /* distances from (400,normal): regular 0, italic 1400, bold 1500, bold italic 1400 and 1500 together */
    const lrow e[] = { {400,5,0,0}, {400,5,2,0}, {700,5,0,0}, {700,5,2,0} };
    list_expect(&m, 400, 5, 0, e, 4);
    fm_close(&m);
}
TEST(mt_list_order_ribbi_bold_request) {
    fm m;
    if (!fm_open(&m, "FmLoRibbi2", F_RIBBI, 4, NULL)) return;
    const lrow e[] = { {700,5,0,0}, {400,5,0,1}, {700,5,2,0}, {400,5,2,1} };   /* regular faces come with simulated bold; the style step (700) outweighs the weight gap */
    list_expect(&m, 700, 5, 0, e, 4);
    fm_close(&m);
}
TEST(mt_list_order_ribbi_italic_request) {
    fm m;
    if (!fm_open(&m, "FmLoRibbi3", F_RIBBI, 4, NULL)) return;
    const lrow e[] = { {400,5,2,0}, {400,5,0,2}, {700,5,2,0}, {700,5,0,2} };
    list_expect(&m, 400, 5, 2, e, 4);
    fm_close(&m);
}
TEST(mt_list_order_weight_ladder) {
    fm m; fm_face f[9]; int i;
    for (i = 0; i < 9; i++) f[i] = (fm_face){ 100 * (i + 1), 5, 0 };
    if (!fm_open(&m, "FmLoW9", f, 9, NULL)) return;
    /* request 450: 500 and 400 tie on distance, heavier first; then 600/300, 700/200, ... alternating by distance (ties heavier first at >= 400) */
    const lrow e[] = { {500,5,0,0}, {400,5,0,0}, {600,5,0,0}, {300,5,0,0}, {700,5,0,0}, {200,5,0,0}, {800,5,0,0}, {100,5,0,0}, {900,5,0,0} };
    list_expect(&m, 450, 5, 0, e, 9);
    /* request 350: 300 and 400 tie, lighter first at < 400 */
    const lrow g[] = { {300,5,0,0}, {400,5,0,0}, {200,5,0,0}, {500,5,0,0}, {100,5,0,0}, {600,5,0,0}, {700,5,0,0}, {800,5,0,0}, {900,5,0,0} };
    list_expect(&m, 350, 5, 0, g, 9);
    fm_close(&m);
}
TEST(mt_list_order_stretch_family) {
    fm m;
    if (!fm_open(&m, "FmLoStr", F_STRETCH, 3, NULL)) return;
    const lrow e[] = { {400,3,0,0}, {400,5,0,0}, {400,7,0,0} };
    list_expect(&m, 400, 3, 0, e, 3);
    const lrow g[] = { {400,7,0,0}, {400,5,0,0}, {400,3,0,0} };
    list_expect(&m, 400, 8, 0, g, 3);
    const lrow h[] = { {400,3,0,0}, {400,5,0,0}, {400,7,0,0} };
    list_expect(&m, 400, 4, 0, h, 3);   /* tie between condensed and normal at stretch 4: narrower first */
    fm_close(&m);
}
TEST(mt_list_single_face_all_simulations) {
    fm m;
    if (!fm_open(&m, "FmLoOne", F_ONE, 1, NULL)) return;
    const lrow a[] = { {400,5,0,3} }; list_expect(&m, 700, 5, 2, a, 1);
    const lrow b[] = { {400,5,0,1} }; list_expect(&m, 700, 5, 0, b, 1);
    const lrow c[] = { {400,5,0,2} }; list_expect(&m, 400, 5, 1, c, 1);
    const lrow d[] = { {400,5,0,0} }; list_expect(&m, 400, 5, 0, d, 1);
    fm_close(&m);
}
TEST(mt_list_has_no_duplicates) {
    fm_face f[FM_MAXF]; int n = grid27(f), w, wd, st; fm m;
    if (!fm_open(&m, "FmLoUniq", f, n, NULL)) return;
    for (w = 100; w <= 900; w += 200) for (wd = 3; wd <= 7; wd += 2) for (st = 0; st <= 2; st++) {
        IDWriteFontList *l = NULL;
        if (SUCCEEDED(IDWriteFontFamily_GetMatchingFonts(m.fam, (DWRITE_FONT_WEIGHT)w, (DWRITE_FONT_STRETCH)wd, (DWRITE_FONT_STYLE)st, &l)) && l) {
            UINT32 c = IDWriteFontList_GetFontCount(l), i, j; fm_face a[FM_MAXF]; int s[FM_MAXF];
            if (c > FM_MAXF) c = FM_MAXF;
            for (i = 0; i < c; i++) { IDWriteFont *font = NULL; IDWriteFontList_GetFont(l, i, &font); fm_read_font(font, &a[i], &s[i]); DW_RELEASE(font); }
            for (i = 0; i < c; i++) for (j = i + 1; j < c; j++)
                CHECKF(!(a[i].w == a[j].w && a[i].wd == a[j].wd && a[i].st == a[j].st && s[i] == s[j]), "request%s: entries %u and %u are the same face", fm_req_name(w, wd, st), i, j);
        }
        DW_RELEASE(l);
    }
    fm_close(&m);
}
