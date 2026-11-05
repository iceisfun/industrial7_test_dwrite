/* IDWriteTextAnalyzer1::GetJustificationOpportunities.
 *
 * Windows 10 (IDWriteTextAnalyzer::GetJustificationOpportunities -> TextAnalyzer::GetJustificationOpportunities):
 *   - fontEmSize < 0 or NaN -> E_INVALIDARG; then glyphCount == 0 -> S_OK (nothing is touched, arrays may be NULL);
 *     otherwise textLength == 0 or a NULL textString / clusterMap / glyphProperties -> E_INVALIDARG (a NULL output array faults).
 *   - fontFace is optional (NULL is accepted: the face only refines the compression limit and Arabic kashida opportunities).
 *   - Every entry [0, glyphCount) of the output is written. The shaper's justification class lives in the low 4 bits of
 *     DWRITE_SHAPING_GLYPH_PROPERTIES (Uniscribe numbering: 0 none, 1 Arabic blank, 2 character, 4 blank, 7.. Arabic classes).
 *       - zero-width-space glyphs, and with fontFace == NULL the Arabic classes >= 7, get the neutral default entry;
 *       - BLANK (4) and ARABIC_BLANK (1): expansionMaximum = fontEmSize * 0.5, expansionPriority 2, compressionPriority 2,
 *         allowResidualExpansion, applyToTrailingEdge; compressionMaximum = min(fontEmSize * 0.25, half the space advance)
 *         which is fontEmSize * 0.125 when no fontFace is supplied. A glyph class of NONE/CHARACTER whose character is white
 *         space is promoted to BLANK;
 *       - in a multi-glyph cluster only the LAST glyph keeps applyToTrailingEdge, the others get copies of the cluster's
 *         values without leading/trailing edges.
 */
#include "ta.h"
#include "ta_shape.h"

#define EM 16.0f
static HRESULT go(IDWriteTextAnalyzer1 *a1, IDWriteFontFace *face, float em, DWRITE_SCRIPT_ANALYSIS sa, UINT32 tl, UINT32 gc, const WCHAR *t, const UINT16 *cm,
                  const DWRITE_SHAPING_GLYPH_PROPERTIES *gp, DWRITE_JUSTIFICATION_OPPORTUNITY *out) {
    return IDWriteTextAnalyzer1_GetJustificationOpportunities(a1, face, em, sa, tl, gc, t, cm, gp, out);
}
static DWRITE_SCRIPT_ANALYSIS SA(UINT16 s) { DWRITE_SCRIPT_ANALYSIS sa; sa.script = s; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT; return sa; }
static int opp_eq(const DWRITE_JUSTIFICATION_OPPORTUNITY *a, const DWRITE_JUSTIFICATION_OPPORTUNITY *b) { return !memcmp(a, b, sizeof *a); }
static void note_opp(const char *what, const DWRITE_JUSTIFICATION_OPPORTUNITY *o) {
    t_note("%s: expMin=%g expMax=%g compMax=%g expPri=%u compPri=%u resExp=%u resComp=%u lead=%u trail=%u", what, o->expansionMinimum, o->expansionMaximum,
           o->compressionMaximum, o->expansionPriority, o->compressionPriority, o->allowResidualExpansion, o->allowResidualCompression, o->applyToLeadingEdge, o->applyToTrailingEdge);
}

TEST(jo_struct_layout) {
    CHECK(sizeof(DWRITE_JUSTIFICATION_OPPORTUNITY) == 16);
    CHECK(sizeof(DWRITE_SHAPING_GLYPH_PROPERTIES) == 2);
    DWRITE_JUSTIFICATION_OPPORTUNITY o; UINT32 raw; memset(&o, 0, sizeof o);
    o.expansionPriority = 0x11; o.compressionPriority = 0x22; o.allowResidualExpansion = 1; o.allowResidualCompression = 0; o.applyToLeadingEdge = 1; o.applyToTrailingEdge = 1;
    memcpy(&raw, (char *)&o + 12, 4);
    EXPECT_EQ(raw, 0x000D2211);     /* bit layout the implementation indexes directly: 8,8,1,1,1,1 */
}
TEST(jo_negative_or_nan_em_invalid) {
    TA_BEGIN(); WCHAR t[] = L"a"; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(0, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[1];
    CHECK_HR(go(a1, NULL, -1.0f, SA(0), 1, 1, t, cm, gp, o), TA_E_INVALIDARG);
    CHECK_HR(go(a1, NULL, NAN, SA(0), 1, 1, t, cm, gp, o), TA_E_INVALIDARG);
    CHECK_HR(go(a1, NULL, -FLT_MIN, SA(0), 1, 1, t, cm, gp, o), TA_E_INVALIDARG);
    CHECK_HR(go(a1, NULL, -1.0f, SA(0), 0, 0, NULL, NULL, NULL, NULL), TA_E_INVALIDARG);     /* checked before glyphCount == 0 */
    TA_END();
}
TEST(jo_zero_glyph_count_is_success_with_nulls) {
    TA_BEGIN();
    CHECK_HR(go(a1, NULL, EM, SA(0), 0, 0, NULL, NULL, NULL, NULL), S_OK);
    CHECK_HR(go(a1, NULL, EM, SA(0), 5, 0, NULL, NULL, NULL, NULL), S_OK);
    CHECK_HR(go(a1, NULL, 0.0f, SA(0), 0, 0, NULL, NULL, NULL, NULL), S_OK);
    TA_END();
}
TEST(jo_zero_text_length_invalid) {
    TA_BEGIN(); WCHAR t[] = L"a"; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(0, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[1];
    CHECK_HR(go(a1, NULL, EM, SA(0), 0, 1, t, cm, gp, o), TA_E_INVALIDARG);
    TA_END();
}
TEST(jo_null_arguments_invalid) {
    TA_BEGIN(); WCHAR t[] = L"a"; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(0, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[1];
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, NULL, cm, gp, o), TA_E_INVALIDARG);
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, NULL, gp, o), TA_E_INVALIDARG);
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, cm, NULL, o), TA_E_INVALIDARG);
    /* a NULL justificationOpportunities pointer is not validated: Windows 10 faults on it, so it is not exercised */
    TA_END();
}
TEST(jo_null_face_is_accepted) {
    TA_BEGIN(); WCHAR t[] = L"a b"; UINT16 cm[3] = { 0, 1, 2 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3] = { ta_gp(0, 1, 0, 0), ta_gp(4, 1, 0, 0), ta_gp(0, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[3];
    CHECK_HR(go(a1, NULL, EM, SA(0), 3, 3, t, cm, gp, o), S_OK);
    TA_END();
}
TEST(jo_writes_every_entry_and_nothing_else) {
    TA_BEGIN(); WCHAR t[] = L"ab cd"; UINT16 cm[5] = { 0, 1, 2, 3, 4 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[5]; DWRITE_JUSTIFICATION_OPPORTUNITY o[7]; int i;
    for (i = 0; i < 5; i++) gp[i] = ta_gp(i == 2 ? 4 : 0, 1, 0, 0);
    memset(o, 0xCC, sizeof o);
    CHECK_HR(go(a1, NULL, EM, SA(0), 5, 5, t, cm, gp, o), S_OK);
    for (i = 0; i < 5; i++) { UINT32 w[4]; memcpy(w, &o[i], 16); CHECKF(!(w[0] == 0xCCCCCCCC && w[1] == 0xCCCCCCCC && w[2] == 0xCCCCCCCC && w[3] == 0xCCCCCCCC), "entry %d not written", i); }
    for (i = 5; i < 7; i++) { UINT32 w[4]; memcpy(w, &o[i], 16); CHECKF(w[0] == 0xCCCCCCCC && w[3] == 0xCCCCCCCC, "entry %d overwritten", i); }
    TA_END();
}
TEST(jo_blank_glyph_entry_without_face) {
    TA_BEGIN(); WCHAR t[] = L"a b"; UINT16 cm[3] = { 0, 1, 2 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3] = { ta_gp(0, 1, 0, 0), ta_gp(4, 1, 0, 0), ta_gp(0, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[3];
    CHECK_HR(go(a1, NULL, EM, SA(0), 3, 3, t, cm, gp, o), S_OK);
    CHECK_F(o[1].expansionMaximum, EM * 0.5f);
    CHECK_F(o[1].compressionMaximum, EM * 0.125f);
    EXPECT_EQ(o[1].expansionPriority, 2); EXPECT_EQ(o[1].compressionPriority, 2);
    EXPECT_EQ(o[1].allowResidualExpansion, 1); EXPECT_EQ(o[1].applyToTrailingEdge, 1);
    note_opp("BLANK glyph", &o[1]);
    TA_END();
}
TEST(jo_blank_expansion_scales_with_em) {
    TA_BEGIN(); WCHAR t[] = L" "; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(4, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[1];
    static const float ems[] = { 0.0f, 1.0f, 8.0f, 12.5f, 16.0f, 100.0f, 1000.0f }; unsigned i;
    for (i = 0; i < sizeof ems / sizeof *ems; i++) {
        CHECK_HR(go(a1, NULL, ems[i], SA(0), 1, 1, t, cm, gp, o), S_OK);
        CHECK_F(o[0].expansionMaximum, ems[i] * 0.5f); CHECK_F(o[0].compressionMaximum, ems[i] * 0.125f);
    }
    TA_END();
}
TEST(jo_arabic_blank_class_entry) {
    TA_BEGIN(); WCHAR t[] = L" "; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(1, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[1];
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, cm, gp, o), S_OK);
    CHECK_F(o[0].expansionMaximum, EM * 0.5f); CHECK_F(o[0].compressionMaximum, EM * 0.125f);
    EXPECT_EQ(o[0].expansionPriority, 2); EXPECT_EQ(o[0].compressionPriority, 2); EXPECT_EQ(o[0].allowResidualExpansion, 1); EXPECT_EQ(o[0].applyToTrailingEdge, 1);
    TA_END();
}
TEST(jo_space_with_class_none_is_promoted_to_blank) {
    TA_BEGIN(); WCHAR t[] = L" "; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES g0 = ta_gp(0, 1, 0, 0), g2 = ta_gp(2, 1, 0, 0), g4 = ta_gp(4, 1, 0, 0);
    DWRITE_JUSTIFICATION_OPPORTUNITY o0, o2, o4;
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, cm, &g0, &o0), S_OK);
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, cm, &g2, &o2), S_OK);
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, cm, &g4, &o4), S_OK);
    CHECK(opp_eq(&o0, &o4)); CHECK(opp_eq(&o2, &o4));
    TA_END();
}
TEST(jo_zero_width_space_glyph_gets_default_entry) {
    TA_BEGIN(); WCHAR t[] = L"a b"; UINT16 cm[3] = { 0, 1, 2 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3] = { ta_gp(0, 1, 0, 1), ta_gp(4, 1, 0, 1), ta_gp(8, 1, 0, 1) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[3];
    CHECK_HR(go(a1, NULL, EM, SA(0), 3, 3, t, cm, gp, o), S_OK);
    CHECK(opp_eq(&o[0], &o[1])); CHECK(opp_eq(&o[1], &o[2]));              /* class is irrelevant once the glyph is a zero-width space */
    note_opp("default (zero-width-space) entry", &o[0]);
    TA_END();
}
TEST(jo_default_entry_independent_of_em) {
    TA_BEGIN(); WCHAR t[] = L"a"; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(0, 1, 0, 1) }; DWRITE_JUSTIFICATION_OPPORTUNITY a, b;
    CHECK_HR(go(a1, NULL, 1.0f, SA(0), 1, 1, t, cm, gp, &a), S_OK);
    CHECK_HR(go(a1, NULL, 100.0f, SA(0), 1, 1, t, cm, gp, &b), S_OK);
    CHECK(opp_eq(&a, &b));
    TA_END();
}
TEST(jo_arabic_extender_classes_without_face_are_neutral) {
    /* kashida opportunities need the font's tatweel glyph; without a face the classes above 6 yield the neutral entry */
    TA_BEGIN(); WCHAR t[] = L"x"; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES zws = ta_gp(0, 1, 0, 1); DWRITE_JUSTIFICATION_OPPORTUNITY d, o; unsigned c;
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, cm, &zws, &d), S_OK);
    for (c = 7; c < 16; c++) {
        DWRITE_SHAPING_GLYPH_PROPERTIES gp = ta_gp(c, 1, 0, 0);
        CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, cm, &gp, &o), S_OK);
        CHECKF(opp_eq(&o, &d), "class %u", c);
    }
    TA_END();
}
TEST(jo_multi_glyph_cluster_edges) {
    /* one character -> {blank, mark}: values are shared across the cluster, the trailing edge moves to the last glyph */
    TA_BEGIN(); WCHAR t[] = L" "; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[2] = { ta_gp(4, 1, 0, 0), ta_gp(0, 0, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[2];
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 2, t, cm, gp, o), S_OK);
    CHECK_F(o[0].expansionMaximum, o[1].expansionMaximum); CHECK_F(o[0].compressionMaximum, o[1].compressionMaximum);
    EXPECT_EQ(o[0].expansionPriority, o[1].expansionPriority); EXPECT_EQ(o[0].compressionPriority, o[1].compressionPriority);
    EXPECT_EQ(o[0].applyToTrailingEdge, 0); EXPECT_EQ(o[1].applyToTrailingEdge, 1);
    EXPECT_EQ(o[1].applyToLeadingEdge, 0);
    TA_END();
}
TEST(jo_three_glyph_cluster_middle_has_no_edges) {
    TA_BEGIN(); WCHAR t[] = L" "; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3] = { ta_gp(4, 1, 0, 0), ta_gp(0, 0, 0, 0), ta_gp(0, 0, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[3];
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 3, t, cm, gp, o), S_OK);
    EXPECT_EQ(o[0].applyToTrailingEdge, 0); EXPECT_EQ(o[1].applyToTrailingEdge, 0); EXPECT_EQ(o[1].applyToLeadingEdge, 0); EXPECT_EQ(o[2].applyToTrailingEdge, 1);
    CHECK_F(o[1].expansionMaximum, o[0].expansionMaximum);
    TA_END();
}
TEST(jo_entries_per_cluster_independent) {
    TA_BEGIN(); WCHAR t[] = L" a "; UINT16 cm[3] = { 0, 1, 2 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[3] = { ta_gp(4, 1, 0, 0), ta_gp(0, 1, 0, 0), ta_gp(4, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[3];
    CHECK_HR(go(a1, NULL, EM, SA(0), 3, 3, t, cm, gp, o), S_OK);
    CHECK(opp_eq(&o[0], &o[2]));
    TA_END();
}
TEST(jo_same_input_same_output) {
    TA_BEGIN(); WCHAR t[] = L"ab cd"; UINT16 cm[5] = { 0, 1, 2, 3, 4 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[5]; DWRITE_JUSTIFICATION_OPPORTUNITY a[5], b[5]; int i;
    for (i = 0; i < 5; i++) gp[i] = ta_gp(i == 2 ? 4 : 0, 1, 0, 0);
    go(a1, NULL, EM, SA(0), 5, 5, t, cm, gp, a); go(a1, NULL, EM, SA(0), 5, 5, t, cm, gp, b);
    CHECK(!memcmp(a, b, sizeof a));
    TA_END();
}
TEST(jo_via_analyzer2_interface) {
    TA_BEGIN(); WCHAR t[] = L" "; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(4, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[1], p[1];
    CHECK_HR(go(a1, NULL, EM, SA(0), 1, 1, t, cm, gp, o), S_OK);
    CHECK_HR(IDWriteTextAnalyzer2_GetJustificationOpportunities(a2, NULL, EM, SA(0), 1, 1, t, cm, gp, p), S_OK);
    CHECK(opp_eq(&o[0], &p[0]));
    TA_END();
}

/* ---- with real shaping data (Arial and a synthetic face) ------------------------------------------------------------------ */
TEST(jo_arial_words_and_spaces) {
    TA_BEGIN(); IDWriteFontFace *arial = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!arial) { TA_END(); t_skip("Arial not installed"); return; }
    static const WCHAR text[] = L"Hello big world"; ta_shaped sh; DWRITE_JUSTIFICATION_OPPORTUNITY o[64]; UINT32 i, n = (UINT32)lstrlenW(text); int words = 0;
    if (!ta_shape(&sh, &c_, arial, text, n, 0, 16.0f)) { IDWriteFontFace_Release(arial); TA_END(); t_skip("GetGlyphs failed"); return; }
    CHECK_HR(go(a1, arial, 16.0f, sh.sa, n, sh.nglyphs, text, sh.cmap, sh.gprops, o), S_OK);
    for (i = 0; i < n; i++) {
        UINT32 g = sh.cmap[i];
        if (text[i] == ' ') {
            CHECKF(o[g].expansionMaximum == 8.0f && o[g].applyToTrailingEdge == 1 && o[g].expansionPriority == 2, "space at %u", i);
            CHECK(o[g].compressionMaximum > 0.0f && o[g].compressionMaximum <= 4.0f);
            if (!words++) note_opp("Arial space", &o[g]);
        }
    }
    CHECK(words == 2);
    /* all letters of one script share one entry */
    for (i = 1; i < 5; i++) CHECK(opp_eq(&o[sh.cmap[i]], &o[sh.cmap[0]]));
    note_opp("Arial letter", &o[sh.cmap[0]]);
    IDWriteFontFace_Release(arial); TA_END();
}
TEST(jo_synthetic_face_blank_compression_limit_note) {
    TA_BEGIN(); dwx_font hf; if (FAILED(dwx_font_open_default(&hf, f)) || !hf.face) { dwx_font_close(&hf); TA_END(); t_skip("synthetic font unavailable"); return; }
    WCHAR t[] = L" "; UINT16 cm[1] = { 0 }; DWRITE_SHAPING_GLYPH_PROPERTIES gp[1] = { ta_gp(4, 1, 0, 0) }; DWRITE_JUSTIFICATION_OPPORTUNITY o[1];
    CHECK_HR(go(a1, hf.face, EM, SA(ta_script_of(&c_, L"A", 1)), 1, 1, t, cm, gp, o), S_OK);
    float half_space = (1000.0f * EM / 2048.0f) * 0.5f, expect = half_space < EM * 0.25f ? half_space : EM * 0.25f;
    CHECK_F(o[0].expansionMaximum, EM * 0.5f);
    CHECK(o[0].compressionMaximum <= EM * 0.25f && o[0].compressionMaximum > 0.0f);
    if (o[0].compressionMaximum != expect) t_note("compressionMaximum %g (model min(em/4, half space advance) = %g)", o[0].compressionMaximum, expect);
    dwx_font_close(&hf); TA_END();
}
