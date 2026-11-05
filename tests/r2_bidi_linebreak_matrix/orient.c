/* AnalyzeVerticalGlyphOrientation matrices (IDWriteTextAnalyzer2, IDWriteTextAnalysisSource1 / Sink1).
 *
 * Observed on Windows 10 (probed per orientation x level x character). Three character classes matter:
 *   CJK-like (ideographs, kana, hangul, fullwidth forms, CJK punctuation, vertical forms, symbols such as U+2605)
 *   latin-like (letters of all other scripts, digits, ASCII punctuation, combining marks)
 *   rotating (white space: space, tab, line feed, no-break space, zero width space, general punctuation spaces, U+2028; and the cursive
 *   scripts Arabic letters and marks, Syriac, N'Ko, Mongolian, Arabic presentation forms; NOT Thaana, Arabic punctuation or digits)
 * information per character:
 *   DEFAULT (0): CJK-like -> angle 0, isSideways TRUE;  latin-like and rotating class -> angle 90, isSideways FALSE;
 *                level = source level, isRightToLeft = level & 1 (for every class)
 *   STACKED (1): level' = (level + 1 unless level > 61) & ~1; CJK-like AND latin-like -> angle 0, isSideways TRUE, isRightToLeft FALSE;
 *                rotating class -> angle 90 (even source level) or 270 (odd), isSideways FALSE, isRightToLeft = level & 1
 *   any other orientation value -> angle 0, level 0, isSideways FALSE, isRightToLeft FALSE
 * SetGlyphOrientation is called once per maximal run of equal information, in ascending order, and the runs cover the range. */
#include "ms.h"
#define MT(n) TEST(r2_bidi_linebreak_matrix_##n)

enum { OC_LATIN = 0, OC_CJK = 1, OC_ROT = 2 };
typedef struct { DWRITE_GLYPH_ORIENTATION_ANGLE ang; UINT8 lvl; BOOL sw, rtl; } oinfo;
static oinfo model(DWRITE_VERTICAL_GLYPH_ORIENTATION o, UINT8 level, int cls)
{
    oinfo r; memset(&r, 0, sizeof r);
    if ((int)o == 0) {
        r.lvl = level; r.rtl = level & 1;
        if (cls == OC_CJK) r.sw = TRUE; else r.ang = DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES;
    } else if ((int)o == 1) {
        UINT8 l = level > 0x3d ? level : (UINT8)(level + 1);
        r.lvl = l & ~1;
        if (cls == OC_ROT) { r.rtl = level & 1; r.ang = (level & 1) ? DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES : DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES; }
        else r.sw = TRUE;
    }
    return r;
}
static int info_eq(const oinfo *a, const oinfo *b) { return a->ang == b->ang && a->lvl == b->lvl && !a->sw == !b->sw && !a->rtl == !b->rtl; }

static ms_sink *or_one(IDWriteTextAnalyzer2 *a2, const WCHAR *t, UINT32 n, DWRITE_VERTICAL_GLYPH_ORIENTATION o, UINT8 lvl, HRESULT *hr)
{
    ms_src *s = ms_src_new(t, n); ms_sink *k = ms_sink_new(1);
    s->nor = 1; s->or_len[0] = 0; s->or_o[0] = o; s->or_bidi[0] = lvl;
    *hr = ms_orient(a2, s, 0, n, k);
    ms_src_free(s);
    return k;
}

/* ---- character classification ---------------------------------------------------------------------------------------------------------- */
static void classify(const WCHAR *t, UINT32 n, int want_upright /* 1 upright, 0 rotated, -1 unknown */, const char *what)
{
    MS_BEGIN2(); HRESULT hr; ms_sink *k = or_one(an2, t, n, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    int cjk;
    CHECK_HR_OK(hr);
    CHECK(ms_sink_exact(k, MK_ORIENT, 0, n));
    cjk = !!k->sw[0];
    if (want_upright >= 0) {
        CHECKF(cjk == want_upright, "%s: %s", what, cjk ? "sideways class" : "angle-90 class");
        if (cjk) { EXPECT_EQ(k->ang[0], DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES); }
        else { EXPECT_EQ(k->ang[0], DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES); }
    } else t_note("%s: %s (angle %d, sideways %d)", what, cjk ? "sideways class" : "angle-90 class", (int)k->ang[0], (int)k->sw[0]);
    EXPECT_EQ(k->abl[0], 0); CHECK(!k->rtl[0]);
    ms_sink_free(k); MS_END();
}
#define CLS1(nm, cp, up) MT(orient_class_##nm) { static const WCHAR t[] = { cp, 0 }; classify(t, 1, up, #nm); }
CLS1(latin_A, 'A', 0) CLS1(latin_Z, 'Z', 0) CLS1(latin_a, 'a', 0) CLS1(latin_z, 'z', 0)
CLS1(digit_0, '0', 0) CLS1(digit_9, '9', 0) CLS1(latin1_e_acute, 0x00e9, 0) CLS1(latin_ext_A, 0x0100, 0) CLS1(latin_ext_b, 0x0180, 0)
CLS1(greek_alpha, 0x03b1, 0) CLS1(cyrillic_a, 0x0430, 0) CLS1(hebrew_alef, 0x05d0, 0) CLS1(arabic_alef, 0x0627, 0) CLS1(devanagari_ka, 0x0915, 0) CLS1(thai_ko_kai, 0x0e01, 0)
CLS1(han_4e00, 0x4e00, 1) CLS1(han_ext_a, 0x3400, 1) CLS1(han_9fa5, 0x9fa5, 1) CLS1(han_f900_compat, 0xf900, -1)
CLS1(hiragana_a, 0x3042, 1) CLS1(hiragana_ka, 0x304b, 1) CLS1(katakana_a, 0x30a2, 1) CLS1(katakana_ka, 0x30ab, 1) CLS1(hangul_ga, 0xac00, 1) CLS1(hangul_hih, 0xd7a3, 1)
CLS1(bopomofo_b, 0x3105, -1) CLS1(hangul_jamo_g, 0x1100, -1) CLS1(hangul_compat_g, 0x3131, -1) CLS1(kanbun, 0x3190, -1)
CLS1(fullwidth_A, 0xff21, -1) CLS1(fullwidth_0, 0xff10, -1) CLS1(halfwidth_katakana, 0xff71, -1)
CLS1(ideographic_space, 0x3000, -1) CLS1(ideographic_comma, 0x3001, -1) CLS1(ideographic_stop, 0x3002, -1)
CLS1(corner_open, 0x300c, -1) CLS1(corner_close, 0x300d, -1) CLS1(prolonged_sound, 0x30fc, -1) CLS1(small_kana_a, 0x3041, -1) CLS1(iteration_mark, 0x3005, -1)
CLS1(space, ' ', -1) CLS1(tab, '\t', -1) CLS1(nbsp, 0x00a0, -1) CLS1(zwsp, 0x200b, -1) CLS1(zwj, 0x200d, -1)
CLS1(period, '.', -1) CLS1(comma, ',', -1) CLS1(colon, ':', -1) CLS1(exclamation, '!', -1) CLS1(question, '?', -1) CLS1(hyphen_minus, '-', -1)
CLS1(open_paren, '(', -1) CLS1(close_paren, ')', -1) CLS1(open_bracket, '[', -1) CLS1(close_brace, '}', -1) CLS1(slash, '/', -1) CLS1(plus, '+', -1) CLS1(equals, '=', -1)
CLS1(at_sign, '@', -1) CLS1(hash, '#', -1) CLS1(dollar, '$', -1) CLS1(percent, '%', -1) CLS1(ampersand, '&', -1) CLS1(asterisk, '*', -1) CLS1(underscore, '_', -1) CLS1(tilde, '~', -1)
CLS1(quote, '"', -1) CLS1(apostrophe, '\'', -1) CLS1(left_dquote, 0x201c, -1) CLS1(right_dquote, 0x201d, -1) CLS1(en_dash, 0x2013, -1) CLS1(em_dash, 0x2014, -1) CLS1(ellipsis, 0x2026, -1)
CLS1(bullet, 0x2022, -1) CLS1(section, 0x00a7, -1) CLS1(copyright, 0x00a9, -1) CLS1(degree, 0x00b0, -1) CLS1(plus_minus, 0x00b1, -1) CLS1(multiply, 0x00d7, -1) CLS1(divide, 0x00f7, -1)
CLS1(arrow_right, 0x2192, -1) CLS1(check_mark, 0x2713, -1) CLS1(black_star, 0x2605, -1) CLS1(circled_digit_1, 0x2460, -1) CLS1(box_drawing, 0x2500, -1) CLS1(block_element, 0x2588, -1)
CLS1(dingbat, 0x2702, -1) CLS1(cjk_radical, 0x2e80, -1) CLS1(kangxi_radical, 0x2f00, -1) CLS1(cjk_strokes, 0x31c0, -1) CLS1(enclosed_cjk, 0x3220, -1) CLS1(cjk_compat, 0x3300, -1)
CLS1(yi_syllable, 0xa000, -1) CLS1(vertical_forms, 0xfe10, -1) CLS1(small_form, 0xfe50, -1) CLS1(cjk_compat_forms, 0xfe30, -1) CLS1(combining_acute, 0x0301, -1) CLS1(combining_dakuten, 0x3099, -1)
CLS1(variation_selector, 0xfe0f, -1) CLS1(object_replacement, 0xfffc, -1) CLS1(replacement_char, 0xfffd, -1) CLS1(private_use, 0xe000, -1) CLS1(emoji_bmp_sun, 0x2600, -1) CLS1(emoji_heart, 0x2764, -1)

MT(orient_class_han_ext_b_pair) { static const WCHAR t[] = { 0xd840, 0xdc00, 0 }; classify(t, 2, 1, "U+20000"); }
MT(orient_class_han_ext_b_pair_runs) {
    MS_BEGIN2(); static const WCHAR t[] = { 0xd840, 0xdc00, 0xd840, 0xdc01, 0 }; HRESULT hr; ms_sink *k = or_one(an2, t, 4, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 1); CHECK(ms_sink_exact(k, MK_ORIENT, 0, 4));
    ms_sink_free(k); MS_END();
}
MT(orient_class_emoji_pair) { static const WCHAR t[] = { 0xd83d, 0xde00, 0 }; classify(t, 2, -1, "U+1F600"); }
MT(orient_class_math_alnum_pair) { static const WCHAR t[] = { 0xd835, 0xdc00, 0 }; classify(t, 2, -1, "U+1D400"); }
MT(orient_class_lone_high_surrogate) { static const WCHAR t[] = { 0xd800, 0 }; classify(t, 1, -1, "lone high surrogate"); }
MT(orient_class_lone_low_surrogate) { static const WCHAR t[] = { 0xdc00, 0 }; classify(t, 1, -1, "lone low surrogate"); }

/* every ASCII letter / digit is angle 90 / not sideways in DEFAULT mode, every Hiragana / Katakana / Han ideograph in a block is angle 0 / sideways */
MT(orient_class_ascii_alnum_all_rotated) {
    MS_BEGIN2(); WCHAR t[80]; int n = 0, c; HRESULT hr; ms_sink *k; int i;
    for (c = 'A'; c <= 'Z'; c++) t[n++] = (WCHAR)c; for (c = 'a'; c <= 'z'; c++) t[n++] = (WCHAR)c; for (c = '0'; c <= '9'; c++) t[n++] = (WCHAR)c;
    k = or_one(an2, t, n, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 1);
    for (i = 0; i < n; i++) { CHECK(!k->sw[i]); EXPECT_EQ(k->ang[i], 1); }
    ms_sink_free(k); MS_END();
}
MT(orient_class_hiragana_block_upright) {
    MS_BEGIN2(); WCHAR t[96]; int n = 0, c; HRESULT hr; ms_sink *k; int i;
    for (c = 0x3042; c <= 0x3093; c++) t[n++] = (WCHAR)c;
    k = or_one(an2, t, n, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 1);
    for (i = 0; i < n; i++) { CHECK(k->sw[i]); EXPECT_EQ(k->ang[i], 0); }
    ms_sink_free(k); MS_END();
}
MT(orient_class_katakana_block_upright) {
    MS_BEGIN2(); WCHAR t[96]; int n = 0, c; HRESULT hr; ms_sink *k; int i;
    for (c = 0x30a2; c <= 0x30f3; c++) t[n++] = (WCHAR)c;
    k = or_one(an2, t, n, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 1);
    for (i = 0; i < n; i++) { CHECK(k->sw[i]); EXPECT_EQ(k->ang[i], 0); }
    ms_sink_free(k); MS_END();
}
MT(orient_class_han_sample_upright) {
    MS_BEGIN2(); WCHAR t[200]; int n = 0, c; HRESULT hr; ms_sink *k; int i;
    for (c = 0x4e00; c < 0x4e00 + 160; c += 1) t[n++] = (WCHAR)c;
    k = or_one(an2, t, n, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 1);
    for (i = 0; i < n; i++) CHECK(k->sw[i]);
    ms_sink_free(k); MS_END();
}
MT(orient_class_hangul_syllable_sample_upright) {
    MS_BEGIN2(); WCHAR t[200]; int n = 0, c; HRESULT hr; ms_sink *k; int i;
    for (c = 0xac00; c < 0xac00 + 150; c += 1) t[n++] = (WCHAR)c;
    k = or_one(an2, t, n, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 1);
    for (i = 0; i < n; i++) CHECK(k->sw[i]);
    ms_sink_free(k); MS_END();
}

/* ---- level x orientation x class ------------------------------------------------------------------------------------------------------- */
static void level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION o, WCHAR ch, int cls)
{
    MS_BEGIN2(); int lv;
    for (lv = 0; lv < 128; lv++) {
        WCHAR t[2] = { ch, 0 }; HRESULT hr; ms_sink *k = or_one(an2, t, 1, o, (UINT8)lv, &hr); oinfo want = model(o, (UINT8)lv, cls);
        CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 1);
        CHECKF(k->ang[0] == want.ang && k->abl[0] == want.lvl && !k->sw[0] == !want.sw && !k->rtl[0] == !want.rtl,
               "orientation %d level %d: angle %d level %u sideways %d rtl %d, want %d %u %d %d", (int)o, lv, (int)k->ang[0], k->abl[0], (int)k->sw[0], (int)k->rtl[0], (int)want.ang, want.lvl, (int)want.sw, (int)want.rtl);
        ms_sink_free(k);
    }
    MS_END();
}
MT(orient_levels_default_latin)  { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 'a', 0); }
MT(orient_levels_default_digit)  { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, '7', 0); }
MT(orient_levels_default_han)    { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0x4e2d, 1); }
MT(orient_levels_default_kana)   { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0x3042, 1); }
MT(orient_levels_default_hangul) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0xac00, 1); }
MT(orient_levels_default_hebrew) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0x05d0, 0); }
MT(orient_levels_default_arabic) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0x0627, 0); }
MT(orient_levels_stacked_latin)  { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 'a', 0); }
MT(orient_levels_stacked_digit)  { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, '7', 0); }
MT(orient_levels_stacked_han)    { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0x4e2d, 1); }
MT(orient_levels_stacked_kana)   { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0x3042, 1); }
MT(orient_levels_stacked_hangul) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0xac00, 1); }
MT(orient_levels_stacked_hebrew) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0x05d0, 0); }
MT(orient_levels_stacked_arabic) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0x0627, OC_ROT); }
MT(orient_levels_stacked_syriac) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0x0710, OC_ROT); }
MT(orient_levels_stacked_mongolian) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0x1820, OC_ROT); }
MT(orient_levels_stacked_thaana) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0x0780, OC_LATIN); }

MT(orient_levels_stacked_space)  { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, ' ', OC_ROT); }
MT(orient_levels_stacked_tab)    { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, '\t', OC_ROT); }
MT(orient_levels_stacked_nbsp)   { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0x00a0, OC_ROT); }
MT(orient_levels_default_space)  { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, ' ', OC_ROT); }
MT(orient_levels_default_ideographic_space) { level_matrix(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0x3000, OC_CJK); }

MT(orient_unknown_orientation_values_zero_info) {
    MS_BEGIN2(); static const UINT32 vals[] = { 2, 3, 4, 7, 8, 15, 16, 100, 255, 256, 0x7fffffff, 0x80000000u, 0xfffffffeu, 0xffffffffu }; int i, j;
    for (i = 0; i < (int)(sizeof vals / sizeof *vals); i++) for (j = 0; j < 2; j++) {
        WCHAR t[2] = { j ? (WCHAR)0x4e2d : (WCHAR)'a', 0 }; HRESULT hr; ms_sink *k = or_one(an2, t, 1, (DWRITE_VERTICAL_GLYPH_ORIENTATION)vals[i], (UINT8)(5 + j), &hr);
        CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 1);
        EXPECT_EQ(k->ang[0], 0); EXPECT_EQ(k->abl[0], 0); CHECK(!k->sw[0]); CHECK(!k->rtl[0]);
        ms_sink_free(k);
    }
    MS_END();
}

/* ---- run coalescing model -------------------------------------------------------------------------------------------------------------- */
typedef struct { DWRITE_VERTICAL_GLYPH_ORIENTATION o; UINT8 lvl; } ocomb;
static const ocomb combos[] = {
    { DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0 }, { DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 1 }, { DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 2 },
    { DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 0 }, { DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 1 }, { DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED, 2 },
};
/* text: rotated letter / upright kana alternate; source runs of 2 units each with combination c0..c3 */
static void coalesce_matrix(const WCHAR *t, UINT32 n, UINT32 runlen)
{
    MS_BEGIN2(); int nruns = (int)((n + runlen - 1) / runlen), total = 1, i, j, c;
    for (i = 0; i < nruns; i++) total *= 6;
    for (c = 0; c < total; c++) {
        int idx[8], v = c; UINT32 p; oinfo per[64]; int expect_runs = 0; ms_src *s; ms_sink *k; HRESULT hr; int ev = 0;
        for (j = 0; j < nruns; j++) { idx[j] = v % 6; v /= 6; }
        s = ms_src_new(t, n); k = ms_sink_new(1);
        s->nor = nruns; for (j = 0; j < nruns; j++) { s->or_len[j] = runlen; s->or_o[j] = combos[idx[j]].o; s->or_bidi[j] = combos[idx[j]].lvl; }
        hr = ms_orient(an2, s, 0, n, k);
        CHECKF(hr == S_OK, "combination %d: %s", c, dw_hr_name(hr));
        for (p = 0; p < n; p++) {
            const ocomb *cb = &combos[idx[p / runlen]]; int upright = t[p] >= 0x3000 ? OC_CJK : OC_LATIN;
            per[p] = model(cb->o, cb->lvl, upright);
            if (p == 0 || !info_eq(&per[p], &per[p - 1])) expect_runs++;
            CHECKF(k->ang[p] == per[p].ang && k->abl[p] == per[p].lvl && !k->sw[p] == !per[p].sw && !k->rtl[p] == !per[p].rtl, "combination %d position %u", c, p);
        }
        CHECKF(k->n[MK_ORIENT] == expect_runs, "combination %d: %d sink runs, expected %d", c, k->n[MK_ORIENT], expect_runs);
        CHECK(ms_runs_exact(k, MK_ORIENT, 0, n)); CHECK(ms_sink_exact(k, MK_ORIENT, 0, n));
        /* adjacent sink runs always differ in information */
        for (j = 0; j + 1 < k->nev; j++) if (k->ev[j].fam == MK_ORIENT && k->ev[j + 1].fam == MK_ORIENT) {
            oinfo a = { k->ev[j].ang, k->ev[j].abl, k->ev[j].sw, k->ev[j].rtl }, b = { k->ev[j + 1].ang, k->ev[j + 1].abl, k->ev[j + 1].sw, k->ev[j + 1].rtl };
            CHECK(!info_eq(&a, &b)); ev++;
        }
        ms_src_free(s); ms_sink_free(k);
    }
    MS_END();
}
MT(orient_coalesce_latin_kana_alternating_runs2) { static const WCHAR t[] = { 'a', 0x3042, 'b', 0x3044, 'c', 0x3046, 'd', 0x3048, 0 }; coalesce_matrix(t, 8, 2); }
MT(orient_coalesce_latin_only_runs2)  { static const WCHAR t[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 0 }; coalesce_matrix(t, 8, 2); }
MT(orient_coalesce_kana_only_runs2)   { static const WCHAR t[] = { 0x3042, 0x3044, 0x3046, 0x3048, 0x304a, 0x304b, 0x304d, 0x304f, 0 }; coalesce_matrix(t, 8, 2); }
MT(orient_coalesce_mixed_blocks_runs2) { static const WCHAR t[] = { 'a', 'b', 0x3042, 0x3044, 'c', 'd', 0x3046, 0x3048, 0 }; coalesce_matrix(t, 8, 2); }
MT(orient_coalesce_latin_kana_alternating_runs3) { static const WCHAR t[] = { 'a', 0x3042, 'b', 0x3044, 'c', 0x3046, 0 }; coalesce_matrix(t, 6, 3); }
MT(orient_coalesce_runs1_alternating) { static const WCHAR t[] = { 'a', 0x3042, 'b', 0x3044, 0 }; coalesce_matrix(t, 4, 1); }

MT(orient_sink_runs_split_inside_a_source_run_on_class_change) {
    MS_BEGIN2(); static const WCHAR t[] = { 'a', 'b', 0x3042, 0x3044, 'c', 0 }; HRESULT hr; ms_sink *k = or_one(an2, t, 5, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    CHECK_HR_OK(hr); EXPECT_EQ(k->n[MK_ORIENT], 3);
    EXPECT_EQ(k->ev[0].pos, 0); EXPECT_EQ(k->ev[0].len, 2); EXPECT_EQ(k->ev[1].pos, 2); EXPECT_EQ(k->ev[1].len, 2); EXPECT_EQ(k->ev[2].pos, 4); EXPECT_EQ(k->ev[2].len, 1);
    CHECK(!k->ev[0].sw); CHECK(k->ev[1].sw); CHECK(!k->ev[2].sw);
    ms_sink_free(k); MS_END();
}
MT(orient_sink_one_run_for_uniform_text) {
    MS_BEGIN2(); HRESULT hr; ms_sink *k = or_one(an2, L"hello world", 11, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT, 0, &hr);
    CHECK_HR_OK(hr); CHECK(k->n[MK_ORIENT] <= 2); CHECK(ms_sink_exact(k, MK_ORIENT, 0, 11)); CHECK(ms_runs_exact(k, MK_ORIENT, 0, 11));
    t_note("\"hello world\" in DEFAULT orientation: %d sink run(s), space is %s", k->n[MK_ORIENT], k->sw[5] ? "sideways" : "not sideways");
    ms_sink_free(k); MS_END();
}
MT(orient_sink_subrange_of_mixed_text) {
    MS_BEGIN2(); static const WCHAR t[] = { 'a', 'b', 0x3042, 0x3044, 'c', 'd', 0 }; ms_src *s = ms_src_new(t, 6); ms_sink *k = ms_sink_new(1);
    CHECK_HR_OK(ms_orient(an2, s, 1, 4, k));
    CHECK(ms_sink_exact(k, MK_ORIENT, 1, 4)); CHECK(ms_runs_exact(k, MK_ORIENT, 1, 4)); EXPECT_EQ(k->n[MK_ORIENT], 3);
    CHECK(!k->sw[1]); CHECK(k->sw[2]); CHECK(k->sw[3]); CHECK(!k->sw[4]);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(orient_sink_pieces_do_not_change_result) {
    MS_BEGIN2(); static const WCHAR t[] = { 'a', 'b', 0x3042, 0xd840, 0xdc00, 'c', 0x3044, 'd', 0 }; int p;
    for (p = 0; p < ms_npatterns; p++) {
        ms_src *s = ms_src_new(t, 8); ms_sink *k = ms_sink_new(1); ms_src *s0 = ms_src_new(t, 8); ms_sink *k0 = ms_sink_new(1); int i;
        ms_src_cuts(s, ms_patterns[p], 6);
        CHECK_HR_OK(ms_orient(an2, s, 0, 8, k)); CHECK_HR_OK(ms_orient(an2, s0, 0, 8, k0));
        CHECK(ms_sink_exact(k, MK_ORIENT, 0, 8)); EXPECT_EQ(k->n[MK_ORIENT], k0->n[MK_ORIENT]);
        for (i = 0; i < 8; i++) { EXPECT_EQ(k->ang[i], k0->ang[i]); EXPECT_EQ(k->sw[i], k0->sw[i]); }
        ms_src_free(s); ms_sink_free(k); ms_src_free(s0); ms_sink_free(k0);
    }
    MS_END();
}
MT(orient_sink_requires_sink1_interface) {
    /* the sink must be usable as IDWriteTextAnalysisSink1: QueryInterface is how the analyzer reaches SetGlyphOrientation or the reference is taken directly; the sink here supports it */
    MS_BEGIN2(); ms_src *s = ms_src_new(L"ab", 2); ms_sink *k = ms_sink_new(1);
    CHECK_HR_OK(ms_orient(an2, s, 0, 2, k));
    EXPECT_EQ(k->n[MK_ORIENT], 1);
    ms_src_free(s); ms_sink_free(k); MS_END();
}
MT(orient_args_invalid_null_source) {
    MS_BEGIN2(); ms_sink *k = ms_sink_new(1);
    CHECK_HR(IDWriteTextAnalyzer2_AnalyzeVerticalGlyphOrientation(an2, NULL, 0, 1, (IDWriteTextAnalysisSink1 *)k), E_INVALIDARG);
    EXPECT_EQ(k->nev, 0); ms_sink_free(k); MS_END();
}
MT(orient_args_invalid_null_sink) {
    MS_BEGIN2(); ms_src *s = ms_src_new(L"ab", 2);
    CHECK_HR(IDWriteTextAnalyzer2_AnalyzeVerticalGlyphOrientation(an2, (IDWriteTextAnalysisSource1 *)s, 0, 1, NULL), E_INVALIDARG);
    EXPECT_EQ(s->nlog, 0); ms_src_free(s); MS_END();
}
MT(orient_args_overflow_range) {
    MS_BEGIN2(); ms_src *s = ms_src_new(L"ab", 2); ms_sink *k = ms_sink_new(1);
    CHECK_HR(ms_orient(an2, s, 0xffffffffu, 2, k), E_INVALIDARG);
    CHECK_HR(ms_orient(an2, s, 1, 0xffffffffu, k), E_INVALIDARG);
    EXPECT_EQ(k->nev, 0); ms_src_free(s); ms_sink_free(k); MS_END();
}
