/* IDWriteTextAnalyzer::GetGlyphs / GetGlyphPlacements / GetGdiCompatibleGlyphPlacements.
 *
 * Deterministic fixtures: the synthetic font "DwShape" (UPM 2048, every letter/digit/space advance 1000, 'A'..'Z' gid 2..27,
 * 'a'..'z' 28..53, '0'..'9' 54..63, space 1, notdef 0) with GSUB 'liga' f+i -> gid 65 and GPOS 'kern' A+V, V+A -100, T+o -150, A+Y -80.
 * At em 16 one advance is 1000 * 16 / 2048 = 7.8125 DIPs.
 *
 * Observed facts: GetGlyphs zeroes *actualGlyphCount first; fontFace NULL => E_INVALIDARG; textLength 0 => S_OK (nothing written);
 * maxGlyphCount 0 (with text) or too small => E_NOT_SUFFICIENT_BUFFER (0x8007007A).  GetGlyphPlacements / GetGdiCompatibleGlyphPlacements:
 * fontFace NULL or textLength 0 => E_INVALIDARG, glyphCount 0 => S_OK; the GDI variant also needs fontEmSize > 0, pixelsPerDip > 0 and
 * no NaN in the transform. */
#include "ta.h"

#define SF_INSUFFICIENT ((HRESULT)0x8007007A)
static const float EM = 16.0f;
static const float ADV16 = 1000.0f * 16.0f / 2048.0f;     /* 7.8125 */

#define SHAPE_BEGIN(shaping) TA_BEGIN(); TA_FONT_BEGIN(F, shaping)
#define SHAPE_END() do { ta_font_close(&F); TA_END(); } while (0)

static HRESULT shape(IDWriteTextAnalyzer *an, ta_font *F, const WCHAR *t, ta_shaped *o)
{
    return ta_shape(an, F->face, t, LEN(t), FALSE, FALSE, L"en-us", NULL, NULL, 0, 64, o);
}

/* ---- plain Latin: one glyph per character ----------------------------------------------------------------------------------------- */
TEST(glyphs_latin_basic)
{
    SHAPE_BEGIN(0);
    ta_shaped o; UINT32 i;
    CHECK_HR_OK(shape(an, &F, L"ABC", &o));
    EXPECT_EQ(o.actual, 3);
    for (i = 0; i < 3; i++) { EXPECT_EQ(o.gid[i], 2 + i); EXPECT_EQ(o.cluster[i], i); }
    SHAPE_END();
}
TEST(glyphs_lowercase_digits_space)
{
    SHAPE_BEGIN(0);
    static const WCHAR t[] = L"az09 ";
    ta_shaped o; static const UINT16 want[] = { 28, 53, 54, 63, 1 }; int i;
    CHECK_HR_OK(shape(an, &F, t, &o));
    EXPECT_EQ(o.actual, 5);
    for (i = 0; i < 5; i++) CHECKF(o.gid[i] == want[i] && o.cluster[i] == i, "unit %d: gid %u cluster %u", i, o.gid[i], o.cluster[i]);
    SHAPE_END();
}
TEST(glyphs_every_mapped_char_matches_cmap)
{
    SHAPE_BEGIN(0);
    WCHAR t[64]; UINT32 n = 0, c; ta_shaped o;
    for (c = 'A'; c <= 'Z'; c++) t[n++] = (WCHAR)c;
    for (c = 'a'; c <= 'z'; c++) t[n++] = (WCHAR)c;
    t[n] = 0;
    CHECK_HR_OK(ta_shape(an, F.face, t, n, FALSE, FALSE, L"en-us", NULL, NULL, 0, 128, &o));
    EXPECT_EQ(o.actual, 52);
    for (c = 0; c < n; c++) CHECKF(o.gid[c] == ttf_default_gid(t[c]), "U+%04X -> %u want %u", t[c], o.gid[c], ttf_default_gid(t[c]));
    SHAPE_END();
}
TEST(glyphs_digits_all)
{
    SHAPE_BEGIN(0);
    static const WCHAR t[] = L"0123456789";
    ta_shaped o; int i;
    CHECK_HR_OK(shape(an, &F, t, &o));
    EXPECT_EQ(o.actual, 10);
    for (i = 0; i < 10; i++) EXPECT_EQ(o.gid[i], 54 + i);
    SHAPE_END();
}
TEST(glyphs_unmapped_is_notdef)
{
    SHAPE_BEGIN(0);
    static const WCHAR t[] = L"a\x00e9" L"b";
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, t, &o));
    EXPECT_EQ(o.actual, 3);
    EXPECT_EQ(o.gid[0], 28); EXPECT_EQ(o.gid[1], 0); EXPECT_EQ(o.gid[2], 29);
    SHAPE_END();
}
TEST(glyphs_single_char)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"Q", &o));
    EXPECT_EQ(o.actual, 1); EXPECT_EQ(o.gid[0], 18); EXPECT_EQ(o.cluster[0], 0);
    SHAPE_END();
}
TEST(glyphs_cluster_map_identity_and_monotone)
{
    SHAPE_BEGIN(0);
    static const WCHAR *const texts[] = { L"Hello World 123", L"the quick brown fox", L"A B C D E F G", L"zzzzzzzzzzzzzzzzzzzz" };
    int ti;
    for (ti = 0; ti < 4; ti++) {
        ta_shaped o; UINT32 i, n = LEN(texts[ti]);
        CHECK_HR_OK(shape(an, &F, texts[ti], &o));
        EXPECT_EQ(o.actual, n);
        for (i = 0; i < n; i++) CHECKF(o.cluster[i] == i, "text %d unit %u cluster %u", ti, i, o.cluster[i]);
        for (i = 1; i < n; i++) CHECK(o.cluster[i] >= o.cluster[i - 1]);
    }
    SHAPE_END();
}
TEST(glyphs_glyph_props_latin)
{
    SHAPE_BEGIN(0);
    ta_shaped o; UINT32 i;
    CHECK_HR_OK(shape(an, &F, L"Hello", &o));
    for (i = 0; i < 5; i++) {
        CHECKF(o.gp[i].isClusterStart == 1, "glyph %u isClusterStart %u", i, o.gp[i].isClusterStart);
        CHECKF(o.gp[i].isDiacritic == 0, "glyph %u isDiacritic", i);
        CHECKF(o.gp[i].isZeroWidthSpace == 0, "glyph %u isZeroWidthSpace", i);
        if (o.gp[i].reserved) t_note("glyph %u reserved property bits %u", i, o.gp[i].reserved);
    }
    t_note("Latin letter justification class %u, text props shapedAlone %u canBreakAfter %u", o.gp[0].justification, o.tp[0].isShapedAlone, o.tp[0].canBreakShapingAfter);
    SHAPE_END();
}
TEST(glyphs_space_glyph_props_note)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"a b", &o));
    CHECK(o.gp[1].isClusterStart == 1);
    CHECK(o.gp[1].isZeroWidthSpace == 0);
    t_note("space glyph justification %u (letter %u), text props space: alone %u break-after %u", o.gp[1].justification, o.gp[0].justification, o.tp[1].isShapedAlone, o.tp[1].canBreakShapingAfter);
    SHAPE_END();
}
TEST(glyphs_text_props_reserved_zero)
{
    SHAPE_BEGIN(0);
    ta_shaped o; UINT32 i;
    CHECK_HR_OK(shape(an, &F, L"ab cd", &o));
    for (i = 0; i < 5; i++) if (o.tp[i].reserved1 || o.tp[i].reserved) t_note("unit %u reserved text property bits %u/%u", i, o.tp[i].reserved1, o.tp[i].reserved);
    SHAPE_END();
}
TEST(glyphs_locale_variants)
{
    SHAPE_BEGIN(0);
    static const WCHAR *const loc[] = { NULL, L"", L"en-us", L"en", L"de-de", L"tr-tr", L"zz-zz", L"x-unknown" };
    int i;
    for (i = 0; i < 8; i++) {
        ta_shaped o;
        CHECKF(ta_shape(an, F.face, L"abc", 3, FALSE, FALSE, loc[i], NULL, NULL, 0, 16, &o) == S_OK, "locale %d", i);
        EXPECT_EQ(o.actual, 3);
    }
    SHAPE_END();
}
TEST(glyphs_sideways_same_glyphs)
{
    SHAPE_BEGIN(0);
    ta_shaped a, b;
    CHECK_HR_OK(ta_shape(an, F.face, L"ABC", 3, FALSE, FALSE, L"en-us", NULL, NULL, 0, 16, &a));
    CHECK_HR_OK(ta_shape(an, F.face, L"ABC", 3, FALSE, TRUE, L"en-us", NULL, NULL, 0, 16, &b));
    EXPECT_EQ(b.actual, 3);
    CHECK(!memcmp(a.gid, b.gid, 3 * sizeof a.gid[0]));
    SHAPE_END();
}
TEST(glyphs_rtl_reverses_visual_order)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"ABC", 3, TRUE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    EXPECT_EQ(o.actual, 3);
    /* either logical or visual glyph order is possible; the cluster map must agree with whichever the analyzer chose */
    if (o.cluster[0] == 2) { EXPECT_EQ(o.gid[0], 4); EXPECT_EQ(o.gid[1], 3); EXPECT_EQ(o.gid[2], 2); EXPECT_EQ(o.cluster[1], 1); EXPECT_EQ(o.cluster[2], 0); t_note("RTL glyph array is in visual (reversed) order"); }
    else { EXPECT_EQ(o.gid[0], 2); EXPECT_EQ(o.gid[1], 3); EXPECT_EQ(o.gid[2], 4); EXPECT_EQ(o.cluster[0], 0); EXPECT_EQ(o.cluster[1], 1); EXPECT_EQ(o.cluster[2], 2); t_note("RTL glyph array is in logical order"); }
    SHAPE_END();
}
TEST(glyphs_rtl_single_char)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"B", 1, TRUE, FALSE, L"en-us", NULL, NULL, 0, 4, &o));
    EXPECT_EQ(o.actual, 1); EXPECT_EQ(o.gid[0], 3); EXPECT_EQ(o.cluster[0], 0);
    SHAPE_END();
}
TEST(glyphs_isolated_text_pointer_not_retained)
{
    SHAPE_BEGIN(0);
    WCHAR *buf = (WCHAR *)malloc(8 * sizeof(WCHAR)); ta_shaped o;
    if (!buf) { SHAPE_END(); t_skip("alloc"); return; }
    buf[0] = 'A'; buf[1] = 'B'; buf[2] = 'C'; buf[3] = 0;
    CHECK_HR_OK(ta_shape(an, F.face, buf, 3, FALSE, FALSE, L"en-us", NULL, NULL, 0, 8, &o));
    memset(buf, 0, 8 * sizeof(WCHAR));
    free(buf);
    /* the face is untouched, the analyzer still works */
    CHECK_HR_OK(shape(an, &F, L"ABC", &o));
    EXPECT_EQ(o.gid[0], 2);
    SHAPE_END();
}

/* ---- buffer protocol --------------------------------------------------------------------------------------------------------------- */
TEST(glyphs_zero_text_length)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"", 0, FALSE, FALSE, L"en-us", NULL, NULL, 0, 8, &o));
    EXPECT_EQ(o.actual, 0);
    /* a zero text length is accepted even with maxGlyphCount 0 */
    CHECK_HR_OK(ta_shape(an, F.face, L"", 0, FALSE, FALSE, L"en-us", NULL, NULL, 0, 0, &o));
    EXPECT_EQ(o.actual, 0);
    SHAPE_END();
}
TEST(glyphs_zero_length_leaves_outputs)
{
    SHAPE_BEGIN(0);
    UINT16 cm[4] = { 0x7777, 0x7777, 0x7777, 0x7777 }, gi[4] = { 0x7777, 0x7777, 0x7777, 0x7777 }; UINT32 act = 99;
    DWRITE_SCRIPT_ANALYSIS sa = ta_script_of(an, L"a", 1);
    DWRITE_SHAPING_TEXT_PROPERTIES tp[4]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[4];
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGlyphs(an, L"abc", 0, F.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 4, cm, tp, gi, gp, &act));
    EXPECT_EQ(act, 0);
    CHECK(cm[0] == 0x7777 && gi[0] == 0x7777);
    SHAPE_END();
}
TEST(glyphs_max_zero_is_insufficient)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR(ta_shape(an, F.face, L"ABC", 3, FALSE, FALSE, L"en-us", NULL, NULL, 0, 0, &o), SF_INSUFFICIENT);
    EXPECT_EQ(o.actual, 0);
    SHAPE_END();
}
TEST(glyphs_exact_capacity_ok)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"ABC", 3, FALSE, FALSE, L"en-us", NULL, NULL, 0, 3, &o));
    EXPECT_EQ(o.actual, 3);
    SHAPE_END();
}
TEST(glyphs_capacity_minus_one_insufficient)
{
    SHAPE_BEGIN(0);
    static const WCHAR *const texts[] = { L"ABC", L"Hello", L"a b c d", L"0123456789" };
    int i;
    for (i = 0; i < 4; i++) {
        ta_shaped o; UINT32 n = LEN(texts[i]);
        HRESULT hr = ta_shape(an, F.face, texts[i], n, FALSE, FALSE, L"en-us", NULL, NULL, 0, n - 1, &o);
        CHECKF(hr == SF_INSUFFICIENT, "text %d: %s", i, dw_hr_name(hr));
        t_note("text %d capacity %u -> required count reported %u", i, n - 1, o.actual);
    }
    SHAPE_END();
}
TEST(glyphs_insufficient_then_retry_with_reported)
{
    SHAPE_BEGIN(0);
    ta_shaped o; HRESULT hr;
    hr = ta_shape(an, F.face, L"ABCDEFGH", 8, FALSE, FALSE, L"en-us", NULL, NULL, 0, 2, &o);
    CHECK_HR(hr, SF_INSUFFICIENT);
    /* retrying with a generous buffer succeeds with the full count */
    CHECK_HR_OK(ta_shape(an, F.face, L"ABCDEFGH", 8, FALSE, FALSE, L"en-us", NULL, NULL, 0, 8 * 3 / 2 + 16, &o));
    EXPECT_EQ(o.actual, 8);
    SHAPE_END();
}
TEST(glyphs_no_write_past_capacity)
{
    SHAPE_BEGIN(0);
    UINT16 cm[16], gi[16]; DWRITE_SHAPING_TEXT_PROPERTIES tp[16]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[16]; UINT32 act = 77, i;
    DWRITE_SCRIPT_ANALYSIS sa = ta_script_of(an, L"a", 1);
    for (i = 0; i < 16; i++) { cm[i] = gi[i] = 0xBEEF; memset(&tp[i], 0xCC, sizeof tp[i]); memset(&gp[i], 0xCC, sizeof gp[i]); }
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGlyphs(an, L"ABCD", 4, F.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 4, cm, tp, gi, gp, &act));
    EXPECT_EQ(act, 4);
    for (i = 4; i < 16; i++) CHECKF(cm[i] == 0xBEEF && gi[i] == 0xBEEF, "canary %u overwritten (cluster %04x glyph %04x)", i, cm[i], gi[i]);
    for (i = 4; i < 16; i++) { DWRITE_SHAPING_GLYPH_PROPERTIES x; memset(&x, 0xCC, sizeof x); CHECKF(!memcmp(&gp[i], &x, sizeof x), "glyph props canary %u", i); }
    for (i = 4; i < 16; i++) { DWRITE_SHAPING_TEXT_PROPERTIES x; memset(&x, 0xCC, sizeof x); CHECKF(!memcmp(&tp[i], &x, sizeof x), "text props canary %u", i); }
    SHAPE_END();
}
TEST(glyphs_capacity_larger_than_needed)
{
    SHAPE_BEGIN(0);
    ta_shaped o; UINT32 m;
    for (m = 3; m <= 300; m += 37) { CHECK_HR_OK(ta_shape(an, F.face, L"ABC", 3, FALSE, FALSE, L"en-us", NULL, NULL, 0, m, &o)); EXPECT_EQ(o.actual, 3); }
    SHAPE_END();
}
TEST(glyphs_null_face_invalid)
{
    TA_BEGIN();
    UINT16 cm[4], gi[4]; DWRITE_SHAPING_TEXT_PROPERTIES tp[4]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[4]; UINT32 act = 0x1234;
    DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT };
    if (t_is_wine()) { TA_END(); t_skip("emulator dereferences a NULL face"); return; }
    HRESULT hr = IDWriteTextAnalyzer_GetGlyphs(an, L"abc", 3, NULL, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 4, cm, tp, gi, gp, &act);
    CHECK_HR(hr, E_INVALIDARG);
    EXPECT_EQ(act, 0);
    act = 0x1234;
    hr = IDWriteTextAnalyzer_GetGlyphs(an, L"", 0, NULL, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 4, cm, tp, gi, gp, &act);
    CHECK_HR(hr, E_INVALIDARG);          /* the face check precedes the zero-length shortcut */
    EXPECT_EQ(act, 0);
    TA_END();
}
TEST(glyphs_actual_zeroed_first)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    o.actual = 0x1234;
    CHECK_HR(ta_shape(an, F.face, L"ABC", 3, FALSE, FALSE, L"en-us", NULL, NULL, 0, 0, &o), SF_INSUFFICIENT);
    CHECK(o.actual != 0x1234 && o.actual != 0xdeadbeef);
    SHAPE_END();
}
TEST(glyphs_large_text)
{
    SHAPE_BEGIN(0);
    WCHAR t[401]; int i; ta_shaped o;
    for (i = 0; i < 400; i++) t[i] = (WCHAR)(i % 5 == 4 ? ' ' : 'a' + i % 26);
    t[400] = 0;
    CHECK_HR_OK(ta_shape(an, F.face, t, 400, FALSE, FALSE, L"en-us", NULL, NULL, 0, 512, &o));
    EXPECT_EQ(o.actual, 400);
    for (i = 0; i < 400; i++) if (o.gid[i] != ttf_default_gid(t[i]) || o.cluster[i] != i) { CHECKF(0, "mismatch at %d", i); break; }
    SHAPE_END();
}
TEST(glyphs_embedded_nul_counted)
{
    SHAPE_BEGIN(0);
    static const WCHAR t[] = { 'a', 0, 'b' };
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, t, 3, FALSE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    EXPECT_EQ(o.actual, 3);
    EXPECT_EQ(o.gid[0], 28); EXPECT_EQ(o.gid[2], 29);
    t_note("U+0000 maps to glyph %u", o.gid[1]);
    SHAPE_END();
}
TEST(glyphs_controls_note)
{
    SHAPE_BEGIN(0);
    static const WCHAR t[] = L"a\t\n\x200b\x200d\x00ad" L"b";
    ta_shaped o; UINT32 i;
    HRESULT hr = ta_shape(an, F.face, t, LEN(t), FALSE, FALSE, L"en-us", NULL, NULL, 0, 32, &o);
    CHECK_HR_OK(hr);
    if (hr == S_OK) {
        CHECK(o.actual >= 1 && o.actual <= LEN(t));
        for (i = 0; i < LEN(t); i++) CHECK(o.cluster[i] < o.actual);
        t_note("controls: %u glyphs for %u units, ZWSP props zeroWidth=%u", o.actual, LEN(t), o.gp[3 < o.actual ? 3 : 0].isZeroWidthSpace);
    }
    SHAPE_END();
}
TEST(glyphs_surrogate_pair_cluster)
{
    SHAPE_BEGIN(0);
    static const WCHAR t[] = L"a\xd840\xdc00" L"b";
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, t, 4, FALSE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    EXPECT_EQ(o.actual, 3);
    EXPECT_EQ(o.cluster[0], 0);
    EXPECT_EQ(o.cluster[1], 1); EXPECT_EQ(o.cluster[2], 1);
    EXPECT_EQ(o.cluster[3], 2);
    EXPECT_EQ(o.gid[1], 0);                      /* the pair has no cmap entry in the synthetic font */
    SHAPE_END();
}

/* ---- ligatures and features -------------------------------------------------------------------------------------------------------- */
TEST(liga_default_on)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"fi", &o));
    EXPECT_EQ(o.actual, 1); EXPECT_EQ(o.gid[0], 65);
    EXPECT_EQ(o.cluster[0], 0); EXPECT_EQ(o.cluster[1], 0);
    CHECK(o.gp[0].isClusterStart == 1);
    SHAPE_END();
}
TEST(liga_in_word)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"afib", &o));
    EXPECT_EQ(o.actual, 3);
    EXPECT_EQ(o.gid[0], 28); EXPECT_EQ(o.gid[1], 65); EXPECT_EQ(o.gid[2], 29);
    EXPECT_EQ(o.cluster[0], 0); EXPECT_EQ(o.cluster[1], 1); EXPECT_EQ(o.cluster[2], 1); EXPECT_EQ(o.cluster[3], 2);
    SHAPE_END();
}
TEST(liga_ffi_sequence)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"ffi", &o));
    EXPECT_EQ(o.actual, 2);
    EXPECT_EQ(o.gid[0], 33); EXPECT_EQ(o.gid[1], 65);
    EXPECT_EQ(o.cluster[0], 0); EXPECT_EQ(o.cluster[1], 1); EXPECT_EQ(o.cluster[2], 1);
    SHAPE_END();
}
TEST(liga_not_formed_for_reverse_order)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"if", &o));
    EXPECT_EQ(o.actual, 2); EXPECT_EQ(o.gid[0], 36); EXPECT_EQ(o.gid[1], 33);
    CHECK_HR_OK(shape(an, &F, L"fai", &o));
    EXPECT_EQ(o.actual, 3);
    SHAPE_END();
}
TEST(liga_twice)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"fifi", &o));
    EXPECT_EQ(o.actual, 2); EXPECT_EQ(o.gid[0], 65); EXPECT_EQ(o.gid[1], 65);
    EXPECT_EQ(o.cluster[0], 0); EXPECT_EQ(o.cluster[1], 0); EXPECT_EQ(o.cluster[2], 1); EXPECT_EQ(o.cluster[3], 1);
    SHAPE_END();
}
TEST(liga_disabled_by_feature)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &off, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf }; UINT32 flen[1] = { 2 };
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"fi", 2, FALSE, FALSE, L"en-us", feat, flen, 1, 16, &o));
    EXPECT_EQ(o.actual, 2); EXPECT_EQ(o.gid[0], 33); EXPECT_EQ(o.gid[1], 36);
    EXPECT_EQ(o.cluster[0], 0); EXPECT_EQ(o.cluster[1], 1);
    SHAPE_END();
}
TEST(liga_enabled_explicitly)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE on = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &on, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf }; UINT32 flen[1] = { 2 };
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"fi", 2, FALSE, FALSE, L"en-us", feat, flen, 1, 16, &o));
    EXPECT_EQ(o.actual, 1); EXPECT_EQ(o.gid[0], 65);
    SHAPE_END();
}
TEST(liga_feature_ranges)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES t_off = { &off, 1 }, t_none = { NULL, 0 };
    const DWRITE_TYPOGRAPHIC_FEATURES *feat[2] = { &t_off, &t_none }; UINT32 flen[2] = { 2, 2 };
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"fifi", 4, FALSE, FALSE, L"en-us", feat, flen, 2, 16, &o));
    /* explicit feature sets replace the defaults in every range, so the empty one gets no ligature either */
    EXPECT_EQ(o.actual, 4);
    EXPECT_EQ(o.gid[0], 33); EXPECT_EQ(o.gid[1], 36); EXPECT_EQ(o.gid[2], 33); EXPECT_EQ(o.gid[3], 36);
    EXPECT_EQ(o.cluster[0], 0); EXPECT_EQ(o.cluster[1], 1); EXPECT_EQ(o.cluster[2], 2); EXPECT_EQ(o.cluster[3], 3);
    SHAPE_END();
}
TEST(liga_feature_range_split_inside_pair)
{
    /* a range boundary between 'f' and 'i' with liga disabled on the second part: no ligature */
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES t_off = { &off, 1 }, t_none = { NULL, 0 };
    const DWRITE_TYPOGRAPHIC_FEATURES *feat[2] = { &t_none, &t_off }; UINT32 flen[2] = { 1, 1 };
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"fi", 2, FALSE, FALSE, L"en-us", feat, flen, 2, 16, &o));
    EXPECT_EQ(o.actual, 2);
    EXPECT_EQ(o.gid[0], 33); EXPECT_EQ(o.gid[1], 36);
    SHAPE_END();
}
TEST(features_unknown_tag_ignored)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE x = { (DWRITE_FONT_FEATURE_TAG)0x31313131, 1 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &x, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf }; UINT32 flen[1] = { 3 };
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"abc", 3, FALSE, FALSE, L"en-us", feat, flen, 1, 16, &o));
    EXPECT_EQ(o.actual, 3); EXPECT_EQ(o.gid[0], 28);
    SHAPE_END();
}
TEST(features_zero_count_entry)
{
    SHAPE_BEGIN(1);
    DWRITE_TYPOGRAPHIC_FEATURES tf = { NULL, 0 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf }; UINT32 flen[1] = { 2 };
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"fi", 2, FALSE, FALSE, L"en-us", feat, flen, 1, 16, &o));
    /* an explicit (even empty) feature set replaces the default features: no ligature */
    EXPECT_EQ(o.actual, 2);
    SHAPE_END();
}
TEST(features_ranges_zero_with_null_arrays)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"fi", 2, FALSE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    EXPECT_EQ(o.actual, 1);
    SHAPE_END();
}
TEST(features_range_length_sum_mismatch_note)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &off, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf };
    UINT32 shortlen[1] = { 1 }, longlen[1] = { 9 };
    ta_shaped o; HRESULT hr;
    hr = ta_shape(an, F.face, L"fi", 2, FALSE, FALSE, L"en-us", feat, shortlen, 1, 16, &o);
    t_note("range lengths sum 1 for 2 units: %s, %u glyphs", dw_hr_name(hr), o.actual);
    hr = ta_shape(an, F.face, L"fi", 2, FALSE, FALSE, L"en-us", feat, longlen, 1, 16, &o);
    t_note("range lengths sum 9 for 2 units: %s, %u glyphs", dw_hr_name(hr), o.actual);
    SHAPE_END();
}
TEST(features_kerning_flag_does_not_change_glyphs)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_KERNING, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &off, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf }; UINT32 flen[1] = { 2 };
    ta_shaped a, b;
    CHECK_HR_OK(ta_shape(an, F.face, L"AV", 2, FALSE, FALSE, L"en-us", NULL, NULL, 0, 16, &a));
    CHECK_HR_OK(ta_shape(an, F.face, L"AV", 2, FALSE, FALSE, L"en-us", feat, flen, 1, 16, &b));
    EXPECT_EQ(a.actual, 2); EXPECT_EQ(b.actual, 2);
    CHECK(a.gid[0] == b.gid[0] && a.gid[1] == b.gid[1]);
    SHAPE_END();
}
TEST(liga_not_applied_without_gsub)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"fi", &o));
    EXPECT_EQ(o.actual, 2); EXPECT_EQ(o.gid[0], 33); EXPECT_EQ(o.gid[1], 36);
    SHAPE_END();
}
TEST(liga_rtl_run)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"fi", 2, TRUE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    EXPECT_EQ(o.actual, 1); EXPECT_EQ(o.gid[0], 65);
    SHAPE_END();
}

/* ---- placements -------------------------------------------------------------------------------------------------------------------- */
TEST(place_advances_no_kern)
{
    SHAPE_BEGIN(1);
    ta_shaped o; UINT32 i;
    CHECK_HR_OK(shape(an, &F, L"abc", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"abc", FALSE, FALSE, EM, NULL, NULL, 0, &o));
    for (i = 0; i < 3; i++) { CHECK_NEAR(o.adv[i], ADV16, 0.0001); CHECK(o.off[i].advanceOffset == 0.0f && o.off[i].ascenderOffset == 0.0f); }
    SHAPE_END();
}
TEST(place_scaling_with_em)
{
    SHAPE_BEGIN(0);
    static const float sizes[] = { 1.0f, 10.0f, 12.0f, 16.0f, 32.0f, 100.0f, 2048.0f };
    int i;
    for (i = 0; i < 7; i++) {
        ta_shaped o;
        CHECK_HR_OK(shape(an, &F, L"ab", &o));
        CHECK_HR_OK(ta_place(an, F.face, L"ab", FALSE, FALSE, sizes[i], NULL, NULL, 0, &o));
        CHECKF(fabsf(o.adv[0] - 1000.0f * sizes[i] / 2048.0f) < 0.001f * (sizes[i] > 100 ? 100 : 1), "em %g: advance %g", sizes[i], o.adv[0]);
        CHECK(o.adv[0] == o.adv[1]);
    }
    SHAPE_END();
}
TEST(place_em_2048_exact)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"A ", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"A ", FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    CHECK_FLOAT_EQ(o.adv[0], 1000.0f); CHECK_FLOAT_EQ(o.adv[1], 1000.0f);
    SHAPE_END();
}
TEST(place_notdef_advance)
{
    SHAPE_BEGIN(0);
    static const WCHAR t[] = L"a\x00e9" L"b";
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, t, &o));
    CHECK_HR_OK(ta_place(an, F.face, t, FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    CHECK_FLOAT_EQ(o.adv[1], 1200.0f);
    SHAPE_END();
}
TEST(place_kern_pair_applied)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"AV", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"AV", FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    CHECK_FLOAT_EQ(o.adv[0], 900.0f);          /* kern -100 reduces the advance of the first glyph */
    CHECK_FLOAT_EQ(o.adv[1], 1000.0f);
    SHAPE_END();
}
TEST(place_kern_pairs_table)
{
    SHAPE_BEGIN(1);
    static const struct { const WCHAR *t; float a0; } rows[] = {
        { L"AV", 900 }, { L"VA", 900 }, { L"To", 850 }, { L"AY", 920 }, { L"AA", 1000 }, { L"VV", 1000 }, { L"oT", 1000 }, { L"YA", 1000 }, { L"Av", 1000 },
    };
    int i;
    for (i = 0; i < 9; i++) {
        ta_shaped o;
        CHECK_HR_OK(shape(an, &F, rows[i].t, &o));
        CHECK_HR_OK(ta_place(an, F.face, rows[i].t, FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
        CHECKF(o.adv[0] == rows[i].a0, "%ls: first advance %g want %g", rows[i].t, o.adv[0], rows[i].a0);
        CHECK(o.adv[1] == 1000.0f);
    }
    SHAPE_END();
}
TEST(place_kern_at_em16)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"AV", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"AV", FALSE, FALSE, EM, NULL, NULL, 0, &o));
    CHECK_NEAR(o.adv[0], ADV16 - 100.0f * 16.0f / 2048.0f, 0.0001);
    CHECK_NEAR(o.adv[1], ADV16, 0.0001);
    SHAPE_END();
}
TEST(place_kern_feature_off)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_KERNING, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &off, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf }; UINT32 flen[1] = { 2 };
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"AV", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"AV", FALSE, FALSE, 2048.0f, feat, flen, 1, &o));
    CHECK_FLOAT_EQ(o.adv[0], 1000.0f); CHECK_FLOAT_EQ(o.adv[1], 1000.0f);
    SHAPE_END();
}
TEST(place_kern_feature_on_explicit)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE on = { DWRITE_FONT_FEATURE_TAG_KERNING, 1 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &on, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf }; UINT32 flen[1] = { 2 };
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"AV", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"AV", FALSE, FALSE, 2048.0f, feat, flen, 1, &o));
    CHECK_FLOAT_EQ(o.adv[0], 900.0f);
    SHAPE_END();
}
TEST(place_kern_range_only_first_pair)
{
    SHAPE_BEGIN(1);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_KERNING, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES t_off = { &off, 1 }, t_none = { NULL, 0 };
    const DWRITE_TYPOGRAPHIC_FEATURES *feat[2] = { &t_off, &t_none }; UINT32 flen[2] = { 2, 2 };
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"AVAV", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"AVAV", FALSE, FALSE, 2048.0f, feat, flen, 2, &o));
    CHECK_FLOAT_EQ(o.adv[0], 1000.0f);        /* kerning disabled for characters 0..1 */
    CHECK_FLOAT_EQ(o.adv[1], 1000.0f);
    /* an explicit (empty) feature list replaces the defaults, so the second pair is not kerned either (Win10 22H2) */
    CHECK_FLOAT_EQ(o.adv[2], 1000.0f);
    CHECK_FLOAT_EQ(o.adv[3], 1000.0f);
    SHAPE_END();
}
TEST(place_kern_not_across_space)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"A V", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"A V", FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    CHECK_FLOAT_EQ(o.adv[0], 1000.0f);
    SHAPE_END();
}
TEST(place_ligature_advance)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"fi", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"fi", FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    EXPECT_EQ(o.actual, 1);
    CHECK_FLOAT_EQ(o.adv[0], 1000.0f);        /* the ligature glyph's own advance */
    SHAPE_END();
}
TEST(place_rtl_order_follows_glyphs)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"AVX", 3, TRUE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    CHECK_HR_OK(ta_place(an, F.face, L"AVX", TRUE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    EXPECT_EQ(o.actual, 3);
    t_note("RTL AVX advances: %g %g %g", o.adv[0], o.adv[1], o.adv[2]);
    SHAPE_END();
}
TEST(place_offsets_zero_plain_font)
{
    SHAPE_BEGIN(1);
    ta_shaped o; UINT32 i;
    CHECK_HR_OK(shape(an, &F, L"Hello World", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"Hello World", FALSE, FALSE, 30.0f, NULL, NULL, 0, &o));
    for (i = 0; i < o.actual; i++) CHECKF(o.off[i].advanceOffset == 0.0f && o.off[i].ascenderOffset == 0.0f, "glyph %u offset (%g,%g)", i, o.off[i].advanceOffset, o.off[i].ascenderOffset);
    SHAPE_END();
}
TEST(place_total_width)
{
    SHAPE_BEGIN(0);
    ta_shaped o; UINT32 i; float w = 0;
    CHECK_HR_OK(shape(an, &F, L"Hello World", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"Hello World", FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    for (i = 0; i < o.actual; i++) w += o.adv[i];
    CHECK_FLOAT_EQ(w, 11000.0f);
    SHAPE_END();
}
TEST(place_sideways_note)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"ab", FALSE, TRUE, 2048.0f, NULL, NULL, 0, &o));
    t_note("sideways placement advances %g %g (horizontal run: %g)", o.adv[0], o.adv[1], 1000.0f);
    SHAPE_END();
}
TEST(place_em_zero_negative_note)
{
    SHAPE_BEGIN(0);
    static const float v[] = { 0.0f, -16.0f, 0.0001f, 1e6f };
    int i;
    for (i = 0; i < 4; i++) {
        ta_shaped o; HRESULT hr;
        CHECK_HR_OK(shape(an, &F, L"ab", &o));
        hr = ta_place(an, F.face, L"ab", FALSE, FALSE, v[i], NULL, NULL, 0, &o);
        t_note("GetGlyphPlacements em %g -> %s, advance %g", v[i], dw_hr_name(hr), hr == S_OK ? o.adv[0] : 0.0);
    }
    SHAPE_END();
}
TEST(place_null_face_invalid)
{
    SHAPE_BEGIN(0);
    ta_shaped o; HRESULT hr;
    if (t_is_wine()) { SHAPE_END(); t_skip("emulator dereferences a NULL face"); return; }
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    hr = ta_place(an, NULL, L"ab", FALSE, FALSE, EM, NULL, NULL, 0, &o);
    CHECK_HR(hr, E_INVALIDARG);
    SHAPE_END();
}
TEST(place_glyph_count_zero_ok)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    o.actual = 0; o.adv[0] = 123.0f;
    CHECK_HR_OK(ta_place(an, F.face, L"ab", FALSE, FALSE, EM, NULL, NULL, 0, &o));
    CHECK(o.adv[0] == 123.0f);
    SHAPE_END();
}
TEST(place_text_length_zero_invalid)
{
    SHAPE_BEGIN(0);
    ta_shaped o; HRESULT hr;
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    o.len = 0;
    hr = ta_place(an, F.face, L"ab", FALSE, FALSE, EM, NULL, NULL, 0, &o);
    CHECK_HR(hr, E_INVALIDARG);
    SHAPE_END();
}
TEST(place_null_face_with_zero_glyphs_still_invalid)
{
    SHAPE_BEGIN(0);
    ta_shaped o; HRESULT hr;
    if (t_is_wine()) { SHAPE_END(); t_skip("emulator dereferences a NULL face"); return; }
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    o.actual = 0;
    hr = ta_place(an, NULL, L"ab", FALSE, FALSE, EM, NULL, NULL, 0, &o);
    CHECK_HR(hr, E_INVALIDARG);
    SHAPE_END();
}
TEST(place_repeatable)
{
    SHAPE_BEGIN(1);
    ta_shaped o, p; int i;
    CHECK_HR_OK(shape(an, &F, L"AVATAR To", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"AVATAR To", FALSE, FALSE, 14.0f, NULL, NULL, 0, &o));
    for (i = 0; i < 20; i++) {
        p = o;
        CHECK_HR_OK(ta_place(an, F.face, L"AVATAR To", FALSE, FALSE, 14.0f, NULL, NULL, 0, &p));
        CHECK(!memcmp(p.adv, o.adv, o.actual * sizeof(float)));
    }
    SHAPE_END();
}

/* ---- GDI-compatible placements ----------------------------------------------------------------------------------------------------- */
static HRESULT gdi(IDWriteTextAnalyzer *an, ta_font *F, const WCHAR *t, ta_shaped *o, float em, float ppd, const DWRITE_MATRIX *m, BOOL natural, BOOL rtl, BOOL sw)
{
    o->hr_place = IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(an, t, o->cluster, o->tp, o->len, o->gid, o->gp, o->actual, F->face, em, ppd, m, natural, sw, rtl,
                                                                      &o->sa, o->loc, NULL, NULL, 0, o->adv, o->off);
    return o->hr_place;
}
TEST(gdi_integer_advances)
{
    SHAPE_BEGIN(0);
    ta_shaped o; UINT32 i;
    CHECK_HR_OK(shape(an, &F, L"abcdef", &o));
    CHECK_HR_OK(gdi(an, &F, L"abcdef", &o, 16.0f, 1.0f, NULL, FALSE, FALSE, FALSE));
    for (i = 0; i < 6; i++) CHECKF(o.adv[i] == floorf(o.adv[i]), "advance %g is not an integer pixel count", o.adv[i]);
    CHECK_FLOAT_EQ(o.adv[0], 8.0f);        /* 1000/2048 * 16 = 7.8125 rounds to 8 */
    SHAPE_END();
}
TEST(gdi_rounding_table)
{
    SHAPE_BEGIN(0);
    static const struct { float em; float adv; } rows[] = { { 8, 4 }, { 10, 5 }, { 12, 6 }, { 14, 7 }, { 16, 8 }, { 20, 10 }, { 24, 12 }, { 32, 16 }, { 48, 23 }, { 96, 47 } };
    int i;
    for (i = 0; i < 10; i++) {
        ta_shaped o;
        CHECK_HR_OK(shape(an, &F, L"ab", &o));
        CHECK_HR_OK(gdi(an, &F, L"ab", &o, rows[i].em, 1.0f, NULL, FALSE, FALSE, FALSE));
        CHECKF(o.adv[0] == floorf(o.adv[0]), "em %g: advance %g not integral", rows[i].em, o.adv[0]);
        CHECKF(o.adv[0] >= 0 && o.adv[0] <= 2 * (1000.0f * rows[i].em / 2048.0f) + 1.0f, "em %g: advance %g implausible", rows[i].em, o.adv[0]);
        if (o.adv[0] != rows[i].adv) t_note("em %g: GDI-compatible advance %g (linear rounding %g)", rows[i].em, o.adv[0], rows[i].adv);
    }
    SHAPE_END();
}
TEST(gdi_pixels_per_dip_scales)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    CHECK_HR_OK(gdi(an, &F, L"ab", &o, 16.0f, 2.0f, NULL, FALSE, FALSE, FALSE));
    /* 32 device pixels per em: 15.625 px rounds to 16 px = 8 DIPs; advances are whole pixels */
    CHECKF(o.adv[0] * 2.0f == floorf(o.adv[0] * 2.0f), "advance %g DIPs is not a whole number of pixels", o.adv[0]);
    CHECK_NEAR(o.adv[0], 8.0, 0.0001);
    CHECK_HR_OK(gdi(an, &F, L"ab", &o, 16.0f, 1.5f, NULL, FALSE, FALSE, FALSE));
    CHECKF(fabsf(o.adv[0] * 1.5f - floorf(o.adv[0] * 1.5f + 0.5f)) < 0.001f, "advance %g DIPs at 1.5 ppd is not whole pixels", o.adv[0]);
    SHAPE_END();
}
TEST(gdi_natural_vs_compatible_note)
{
    SHAPE_BEGIN(0);
    ta_shaped a, b;
    CHECK_HR_OK(shape(an, &F, L"abc", &a)); b = a;
    CHECK_HR_OK(gdi(an, &F, L"abc", &a, 13.0f, 1.0f, NULL, FALSE, FALSE, FALSE));
    CHECK_HR_OK(gdi(an, &F, L"abc", &b, 13.0f, 1.0f, NULL, TRUE, FALSE, FALSE));
    t_note("em 13: compatible advance %g, natural advance %g (linear %g)", a.adv[0], b.adv[0], 1000.0f * 13.0f / 2048.0f);
    SHAPE_END();
}
TEST(gdi_identity_transform_same_as_null)
{
    SHAPE_BEGIN(0);
    static const DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 };
    ta_shaped a, b;
    CHECK_HR_OK(shape(an, &F, L"abc", &a)); b = a;
    CHECK_HR_OK(gdi(an, &F, L"abc", &a, 16.0f, 1.0f, NULL, FALSE, FALSE, FALSE));
    CHECK_HR_OK(gdi(an, &F, L"abc", &b, 16.0f, 1.0f, &id, FALSE, FALSE, FALSE));
    CHECK(!memcmp(a.adv, b.adv, 3 * sizeof(float)));
    SHAPE_END();
}
TEST(gdi_kern_pair_integral)
{
    SHAPE_BEGIN(1);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"AV", &o));
    CHECK_HR_OK(gdi(an, &F, L"AV", &o, 16.0f, 1.0f, NULL, FALSE, FALSE, FALSE));
    CHECK(o.adv[0] <= o.adv[1]);
    t_note("GDI-compatible AV at 16: %g %g (integral: %d)", o.adv[0], o.adv[1], o.adv[0] == floorf(o.adv[0]));
    SHAPE_END();
}
TEST(gdi_null_face_invalid)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    if (t_is_wine()) { SHAPE_END(); t_skip("emulator dereferences a NULL face"); return; }
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    o.hr_place = IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(an, L"ab", o.cluster, o.tp, o.len, o.gid, o.gp, o.actual, NULL, 16.0f, 1.0f, NULL, FALSE, FALSE, FALSE, &o.sa, o.loc, NULL, NULL, 0, o.adv, o.off);
    CHECK_HR(o.hr_place, E_INVALIDARG);
    SHAPE_END();
}
TEST(gdi_invalid_sizes)
{
    SHAPE_BEGIN(0);
    static const float bad[] = { 0.0f, -1.0f, -16.0f, NAN, -INFINITY, -0.0f };
    int i;
    for (i = 0; i < 6; i++) {
        ta_shaped o;
        CHECK_HR_OK(shape(an, &F, L"ab", &o));
        CHECK_HR(gdi(an, &F, L"ab", &o, bad[i], 1.0f, NULL, FALSE, FALSE, FALSE), E_INVALIDARG);
        CHECK_HR(gdi(an, &F, L"ab", &o, 16.0f, bad[i], NULL, FALSE, FALSE, FALSE), E_INVALIDARG);
    }
    SHAPE_END();
}
TEST(gdi_matrix_nan_invalid)
{
    SHAPE_BEGIN(0);
    ta_shaped o; int i;
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX m = { 1, 0, 0, 1, 0, 0 };
        float *p = &m.m11; p[i] = NAN;
        CHECKF(gdi(an, &F, L"ab", &o, 16.0f, 1.0f, &m, FALSE, FALSE, FALSE) == E_INVALIDARG, "NaN in matrix member %d accepted", i);
    }
    SHAPE_END();
}
TEST(gdi_matrix_odd_values_accepted)
{
    SHAPE_BEGIN(0);
    static const DWRITE_MATRIX ms[] = { { 0, 0, 0, 0, 0, 0 }, { 2, 0, 0, 2, 0, 0 }, { 1, 0.5f, 0, 1, 10, 10 }, { -1, 0, 0, 1, 0, 0 }, { INFINITY, 0, 0, 1, 0, 0 } };
    int i;
    for (i = 0; i < 5; i++) {
        ta_shaped o; HRESULT hr;
        CHECK_HR_OK(shape(an, &F, L"ab", &o));
        hr = gdi(an, &F, L"ab", &o, 16.0f, 1.0f, &ms[i], FALSE, FALSE, FALSE);
        t_note("matrix %d: %s", i, dw_hr_name(hr));
        CHECK(hr == S_OK || hr == E_INVALIDARG);
    }
    SHAPE_END();
}
TEST(gdi_glyph_count_zero_ok)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    o.actual = 0;
    CHECK_HR_OK(gdi(an, &F, L"ab", &o, 16.0f, 1.0f, NULL, FALSE, FALSE, FALSE));
    SHAPE_END();
}
TEST(gdi_glyph_count_zero_bypasses_size_checks)
{
    /* the glyph-count shortcut precedes the size validation */
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    o.actual = 0;
    CHECK_HR_OK(gdi(an, &F, L"ab", &o, -5.0f, -5.0f, NULL, FALSE, FALSE, FALSE));
    SHAPE_END();
}
TEST(gdi_text_length_zero_invalid)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(shape(an, &F, L"ab", &o));
    o.len = 0;
    CHECK_HR(gdi(an, &F, L"ab", &o, 16.0f, 1.0f, NULL, FALSE, FALSE, FALSE), E_INVALIDARG);
    SHAPE_END();
}
TEST(gdi_rtl_and_sideways_accepted)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, L"abc", 3, TRUE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    CHECK_HR_OK(gdi(an, &F, L"abc", &o, 16.0f, 1.0f, NULL, FALSE, TRUE, FALSE));
    CHECK_HR_OK(gdi(an, &F, L"abc", &o, 16.0f, 1.0f, NULL, FALSE, TRUE, TRUE));
    SHAPE_END();
}

/* ---- system font relations (Arial) ------------------------------------------------------------------------------------------------- */
static IDWriteFontFace *arial(IDWriteFactory *f) { return dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); }

static int hmtx_adv(IDWriteFontFace *face, UINT16 gid, int *upm)
{
    UINT32 hl, hdl, ml, hd; uint8_t *hhea = dw_face_table(face, DW_TAG('h', 'h', 'e', 'a'), &hl), *hmtx = dw_face_table(face, DW_TAG('h', 'm', 't', 'x'), &ml), *head = dw_face_table(face, DW_TAG('h', 'e', 'a', 'd'), &hd);
    int r = -1; (void)hdl;
    if (hhea && hmtx && head && hl >= 36 && hd >= 20) {
        unsigned nh = (hhea[34] << 8) | hhea[35]; unsigned g = gid < nh ? gid : nh - 1;
        *upm = (head[18] << 8) | head[19];
        if (nh && (g * 4 + 2) <= ml) r = (hmtx[g * 4] << 8) | hmtx[g * 4 + 1];
    }
    free(hhea); free(hmtx); free(head);
    return r;
}
TEST(arial_latin_glyphs_match_glyph_indices)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    static const WCHAR t[] = L"Hello, World 0123";
    UINT32 cp[32], i, n = LEN(t); UINT16 want[32]; ta_shaped o;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    for (i = 0; i < n; i++) cp[i] = t[i];
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, n, want));
    CHECK_HR_OK(ta_shape(an, face, t, n, FALSE, FALSE, L"en-us", NULL, NULL, 0, 64, &o));
    EXPECT_EQ(o.actual, n);
    for (i = 0; i < n; i++) CHECKF(o.gid[i] == want[i] && o.cluster[i] == i, "unit %u: shaped glyph %u cmap glyph %u", i, o.gid[i], want[i]);
    DW_RELEASE(face); TA_END();
}
TEST(arial_advances_match_hmtx_without_kerning)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_KERNING, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &off, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf };
    static const WCHAR t[] = L"Test AVAT";
    UINT32 n = LEN(t), i; UINT32 flen[1]; ta_shaped o;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    flen[0] = n;
    CHECK_HR_OK(ta_shape(an, face, t, n, FALSE, FALSE, L"en-us", feat, flen, 1, 64, &o));
    CHECK_HR_OK(ta_place(an, face, t, FALSE, FALSE, 2048.0f, feat, flen, 1, &o));
    for (i = 0; i < o.actual; i++) {
        int upm = 0, a = hmtx_adv(face, o.gid[i], &upm);
        if (a < 0) { t_note("could not read hmtx"); break; }
        CHECKF(fabsf(o.adv[i] - (float)a * 2048.0f / (float)upm) < 0.01f, "glyph %u (gid %u): advance %g, hmtx %d (upm %d)", i, o.gid[i], o.adv[i], a, upm);
    }
    DW_RELEASE(face); TA_END();
}
TEST(arial_kerning_av_not_wider)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    DWRITE_FONT_FEATURE off = { DWRITE_FONT_FEATURE_TAG_KERNING, 0 };
    DWRITE_TYPOGRAPHIC_FEATURES tf = { &off, 1 }; const DWRITE_TYPOGRAPHIC_FEATURES *feat[1] = { &tf }; UINT32 flen[1] = { 2 };
    ta_shaped on, offs;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    CHECK_HR_OK(ta_shape(an, face, L"AV", 2, FALSE, FALSE, L"en-us", NULL, NULL, 0, 8, &on));
    CHECK_HR_OK(ta_place(an, face, L"AV", FALSE, FALSE, 2048.0f, NULL, NULL, 0, &on));
    offs = on;
    CHECK_HR_OK(ta_place(an, face, L"AV", FALSE, FALSE, 2048.0f, feat, flen, 1, &offs));
    CHECK(on.adv[0] <= offs.adv[0]);
    CHECK(on.adv[1] == offs.adv[1]);
    t_note("Arial AV at 2048: kern on %g, kern off %g", on.adv[0], offs.adv[0]);
    DW_RELEASE(face); TA_END();
}
TEST(arial_arabic_joining_forms_differ)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    static const WCHAR t3[] = { 0x628, 0x628, 0x628 }, t1[] = { 0x628 };
    UINT32 cp = 0x628; UINT16 gi = 0; ta_shaped o, iso;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gi);
    if (!gi) { DW_RELEASE(face); TA_END(); t_skip("Arial lacks Arabic"); return; }
    CHECK_HR_OK(ta_shape(an, face, t3, 3, TRUE, FALSE, L"ar-sa", NULL, NULL, 0, 16, &o));
    CHECK_HR_OK(ta_shape(an, face, t1, 1, TRUE, FALSE, L"ar-sa", NULL, NULL, 0, 16, &iso));
    EXPECT_EQ(o.actual, 3);
    if (o.actual == 3) {
        /* initial, medial and final forms are three different glyphs, none of them the isolated form */
        t_note("Arabic gids: %u %u %u iso %u", o.gid[0], o.gid[1], o.gid[2], iso.gid[0]);
        CHECK((o.cluster[0] == 2 && o.cluster[2] == 0) || (o.cluster[0] == 0 && o.cluster[2] == 2));
        t_note("Arabic glyph array order: %s", o.cluster[0] == 2 ? "visual (reversed)" : "logical");
    }
    DW_RELEASE(face); TA_END();
}
TEST(arial_arabic_nonjoiner_isolated)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    static const WCHAR t[] = { 0x627, 0x627 };       /* alef does not join to its left */
    UINT32 cp = 0x627; UINT16 gi = 0; ta_shaped o;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &gi);
    if (!gi) { DW_RELEASE(face); TA_END(); t_skip("Arial lacks Arabic"); return; }
    CHECK_HR_OK(ta_shape(an, face, t, 2, TRUE, FALSE, L"ar-sa", NULL, NULL, 0, 16, &o));
    EXPECT_EQ(o.actual, 2);
    if (o.actual == 2) CHECK(o.gid[0] != 0 && o.gid[1] != 0);
    DW_RELEASE(face); TA_END();
}
TEST(arial_hebrew_rtl_visual_order)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    static const WCHAR t[] = { 0x5d0, 0x5d1, 0x5d2 };
    UINT32 cp[3] = { 0x5d0, 0x5d1, 0x5d2 }; UINT16 gi[3] = { 0 }; ta_shaped o;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    IDWriteFontFace_GetGlyphIndices(face, cp, 3, gi);
    if (!gi[0]) { DW_RELEASE(face); TA_END(); t_skip("Arial lacks Hebrew"); return; }
    CHECK_HR_OK(ta_shape(an, face, t, 3, TRUE, FALSE, L"he-il", NULL, NULL, 0, 16, &o));
    EXPECT_EQ(o.actual, 3);
    if (o.actual == 3) {
        if (o.cluster[0] == 2) { EXPECT_EQ(o.gid[0], gi[2]); EXPECT_EQ(o.gid[1], gi[1]); EXPECT_EQ(o.gid[2], gi[0]); }
        else { EXPECT_EQ(o.gid[0], gi[0]); EXPECT_EQ(o.gid[1], gi[1]); EXPECT_EQ(o.gid[2], gi[2]); }
    }
    DW_RELEASE(face); TA_END();
}
TEST(arial_combining_acute_cluster)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    static const WCHAR t[] = { 'e', 0x0301, 'x' };
    ta_shaped o;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    CHECK_HR_OK(ta_shape(an, face, t, 3, FALSE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    CHECK(o.actual >= 2 && o.actual <= 3);
    CHECK(o.cluster[0] == 0 && o.cluster[2] == o.actual - 1);
    CHECK(o.cluster[1] == 0 || o.cluster[1] == 1);
    t_note("e + U+0301 + x: %u glyph(s), cluster map %u %u %u, glyph1 diacritic=%u", o.actual, o.cluster[0], o.cluster[1], o.cluster[2], o.actual > 1 ? o.gp[1].isDiacritic : 0);
    DW_RELEASE(face); TA_END();
}
TEST(arial_gdi_placements_integral)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    static const WCHAR t[] = L"The quick brown fox";
    ta_shaped o; UINT32 i, n = LEN(t); static const float sizes[] = { 9, 11, 12, 13, 16, 20 };
    int si;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    for (si = 0; si < 6; si++) {
        CHECK_HR_OK(ta_shape(an, face, t, n, FALSE, FALSE, L"en-us", NULL, NULL, 0, 64, &o));
        o.hr_place = IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(an, t, o.cluster, o.tp, o.len, o.gid, o.gp, o.actual, face, sizes[si], 1.0f, NULL, FALSE, FALSE, FALSE, &o.sa, o.loc, NULL, NULL, 0, o.adv, o.off);
        CHECK_HR_OK(o.hr_place);
        for (i = 0; i < o.actual; i++) CHECKF(o.adv[i] == floorf(o.adv[i]) && o.adv[i] >= 0, "size %g glyph %u advance %g", sizes[si], i, o.adv[i]);
    }
    DW_RELEASE(face); TA_END();
}
TEST(arial_arial_em_metrics_note)
{
    TA_BEGIN();
    IDWriteFontFace *face = arial(f);
    ta_shaped o;
    if (!face) { TA_END(); t_skip("Arial not installed"); return; }
    CHECK_HR_OK(ta_shape(an, face, L"A", 1, FALSE, FALSE, L"en-us", NULL, NULL, 0, 4, &o));
    CHECK_HR_OK(ta_place(an, face, L"A", FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    t_note("Arial 'A' advance at em 2048: %g", o.adv[0]);
    DW_RELEASE(face); TA_END();
}

/* ---- shaping through the analysis pipeline ----------------------------------------------------------------------------------------- */
TEST(pipeline_script_runs_then_shape_each)
{
    SHAPE_BEGIN(1);
    static const WCHAR t[] = L"fi AV fi";
    dwx_ana_src *s; dwx_ana_sink *k; ta_shaped o; float w = 0; UINT32 i;
    CHECK_HR_OK(ta_run(an, TA_SCRIPT, t, &s, &k));
    CHECK(k->n_script == 1);
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGlyphs(an, t, 8, F.face, FALSE, FALSE, &k->script[0], L"en-us", NULL, NULL, NULL, 0, 32, o.cluster, o.tp, o.gid, o.gp, &o.actual));
    EXPECT_EQ(o.actual, 6);           /* lig, space, A, V, space, lig */
    o.len = 8; o.sa = k->script[0]; o.loc = L"en-us";
    CHECK_HR_OK(ta_place(an, F.face, t, FALSE, FALSE, 2048.0f, NULL, NULL, 0, &o));
    for (i = 0; i < o.actual; i++) w += o.adv[i];
    CHECK_FLOAT_EQ(w, 6000.0f - 100.0f);
    ta_free(s, k); SHAPE_END();
}
TEST(pipeline_shape_text_prefix_of_longer_buffer)
{
    SHAPE_BEGIN(0);
    static const WCHAR t[] = L"ABCDEFGH";
    ta_shaped o;
    CHECK_HR_OK(ta_shape(an, F.face, t, 4, FALSE, FALSE, L"en-us", NULL, NULL, 0, 16, &o));
    EXPECT_EQ(o.actual, 4);
    EXPECT_EQ(o.gid[3], 5);
    SHAPE_END();
}
TEST(pipeline_repeated_shaping_stable)
{
    SHAPE_BEGIN(1);
    ta_shaped a, b; int i;
    CHECK_HR_OK(shape(an, &F, L"affine office fifi", &a));
    for (i = 0; i < 30; i++) {
        CHECK_HR_OK(shape(an, &F, L"affine office fifi", &b));
        CHECK(a.actual == b.actual && !memcmp(a.gid, b.gid, a.actual * sizeof a.gid[0]) && !memcmp(a.cluster, b.cluster, 18 * sizeof a.cluster[0]));
    }
    SHAPE_END();
}
TEST(pipeline_two_analyzers_same_result)
{
    SHAPE_BEGIN(1);
    IDWriteTextAnalyzer *b = NULL; ta_shaped x, y;
    CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &b));
    if (b) {
        CHECK_HR_OK(shape(an, &F, L"fi AV", &x));
        CHECK_HR_OK(shape(b, &F, L"fi AV", &y));
        CHECK(x.actual == y.actual && !memcmp(x.gid, y.gid, x.actual * sizeof x.gid[0]));
        DW_RELEASE(b);
    }
    SHAPE_END();
}
TEST(pipeline_face_released_after_use)
{
    SHAPE_BEGIN(0);
    ta_shaped o;
    EXPECT_REFS(F.face, 1);
    CHECK_HR_OK(shape(an, &F, L"abc", &o));
    CHECK_HR_OK(ta_place(an, F.face, L"abc", FALSE, FALSE, EM, NULL, NULL, 0, &o));
    EXPECT_REFS(F.face, 1);
    SHAPE_END();
}
