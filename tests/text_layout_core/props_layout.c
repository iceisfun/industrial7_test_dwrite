/* Layout-wide properties: the IDWriteTextFormat half of IDWriteTextLayout (alignment, wrapping, directions, tab stop, trimming, line
 * spacing, max size) plus the creation-time inheritance from the format.
 *
 * Validation performed by the layout's own setters (not shared with any format object):
 *   SetTextAlignment 0..3, SetParagraphAlignment 0..2, SetWordWrapping 0..4, SetReadingDirection 0..3, SetFlowDirection 0..3,
 *   SetIncrementalTabStop > 0 (NaN and <= 0 rejected, +INF accepted), SetMaxWidth/SetMaxHeight >= 0 and not NaN,
 *   SetTrimming granularity 0..2 and delimiter <= 0x10FFFF with a non-NULL options pointer,
 *   SetLineSpacing(method, height, baseline): method 0..2, height >= 0, neither NaN (negative baseline allowed).
 * A rejected call changes nothing. A conflicting reading/flow direction pair is accepted by the setters and reported by the first
 * operation that needs the layout (DWRITE_E_FLOWDIRECTIONCONFLICTS). */
#include "tl.h"

#define LY(var) TL_BEGIN(); TL_LAYOUT(var, L"ABCD", 16.0f, 200.0f, 100.0f)
#define DW_E_FLOWCONFLICT ((HRESULT)0x8898500B)

/* ---- defaults and inheritance ----------------------------------------------------------------------------------------- */
TEST(layout_defaults_documented)
{
    LY(l);
    DWRITE_TRIMMING tr; IDWriteInlineObject *sign = (IDWriteInlineObject *)(ULONG_PTR)1;
    DWRITE_LINE_SPACING_METHOD m = (DWRITE_LINE_SPACING_METHOD)99; float sp = -1, bl = -1;
    EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_LEADING);
    EXPECT_EQ(IDWriteTextLayout_GetParagraphAlignment(l), DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    EXPECT_EQ(IDWriteTextLayout_GetWordWrapping(l), DWRITE_WORD_WRAPPING_WRAP);
    EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &tr, &sign));
    EXPECT_EQ(tr.granularity, DWRITE_TRIMMING_GRANULARITY_NONE); EXPECT_EQ(tr.delimiter, 0); EXPECT_EQ(tr.delimiterCount, 0); CHECK(sign == NULL);
    CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &sp, &bl));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_DEFAULT); CHECK_FLOAT_EQ(sp, 0.0f); CHECK_FLOAT_EQ(bl, 0.0f);
    CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), 200.0f); CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), 100.0f);
    DW_RELEASE(l); TL_END();
}

TEST(layout_defaults_match_fresh_format)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    DWRITE_TRIMMING a, b; IDWriteInlineObject *sa = NULL, *sb = NULL; DWRITE_LINE_SPACING_METHOD ma, mb; float sa_, sb_, ba, bb;
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 200.0f, 100.0f, &l));
    if (!l) { DW_RELEASE(t); TL_END(); t_skip("layout"); return; }
    EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), IDWriteTextFormat_GetTextAlignment(t));
    EXPECT_EQ(IDWriteTextLayout_GetParagraphAlignment(l), IDWriteTextFormat_GetParagraphAlignment(t));
    EXPECT_EQ(IDWriteTextLayout_GetWordWrapping(l), IDWriteTextFormat_GetWordWrapping(t));
    EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), IDWriteTextFormat_GetReadingDirection(t));
    EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), IDWriteTextFormat_GetFlowDirection(t));
    CHECK_FBITS(IDWriteTextLayout_GetIncrementalTabStop(l), IDWriteTextFormat_GetIncrementalTabStop(t));
    t_note("default incremental tab stop at size 16: %g", IDWriteTextLayout_GetIncrementalTabStop(l));
    IDWriteTextLayout_GetTrimming(l, &a, &sa); IDWriteTextFormat_GetTrimming(t, &b, &sb);
    EXPECT_EQ(a.granularity, b.granularity); EXPECT_EQ(a.delimiter, b.delimiter); EXPECT_EQ(a.delimiterCount, b.delimiterCount); CHECK(sa == sb);
    DW_RELEASE(sa); DW_RELEASE(sb);
    IDWriteTextLayout_GetLineSpacing(l, &ma, &sa_, &ba); IDWriteTextFormat_GetLineSpacing(t, &mb, &sb_, &bb);
    EXPECT_EQ(ma, mb); CHECK_FBITS(sa_, sb_); CHECK_FBITS(ba, bb);
    DW_RELEASE(l); DW_RELEASE(t); TL_END();
}

TEST(layout_inherits_every_format_property)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 21.5f); IDWriteTextLayout *l = NULL;
    dwx_inline *sign = dwx_inline_new(4, 4, 3, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2e, 3 }, got; IDWriteInlineObject *gs = NULL;
    DWRITE_LINE_SPACING_METHOD m; float sp, bl;
    if (!t) { dwx_release(sign); TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_JUSTIFIED));
    CHECK_HR_OK(IDWriteTextFormat_SetParagraphAlignment(t, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_CHARACTER));
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(t, 37.25f));
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, (IDWriteInlineObject *)sign));
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 22.0f));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 200.0f, 100.0f, &l));
    if (!l) { DW_RELEASE(t); dwx_release(sign); TL_END(); t_skip("layout"); return; }
    EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_JUSTIFIED);
    EXPECT_EQ(IDWriteTextLayout_GetParagraphAlignment(l), DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    EXPECT_EQ(IDWriteTextLayout_GetWordWrapping(l), DWRITE_WORD_WRAPPING_CHARACTER);
    EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), 37.25f);
    CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &got, &gs));
    EXPECT_EQ(got.granularity, DWRITE_TRIMMING_GRANULARITY_WORD); EXPECT_EQ(got.delimiter, 0x2e); EXPECT_EQ(got.delimiterCount, 3); CHECK(gs == (IDWriteInlineObject *)sign);
    DW_RELEASE(gs);
    CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &sp, &bl));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(sp, 30.0f); CHECK_FLOAT_EQ(bl, 22.0f);
    CHECK_FLOAT_EQ(l->lpVtbl->GetFontSize(l), 21.5f);
    DW_RELEASE(l); DW_RELEASE(t); dwx_release(sign); TL_END();
}

TEST(layout_inherits_font_properties)
{
    TL_BEGIN(); IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_RANGE r;
    DWRITE_FONT_WEIGHT w; DWRITE_FONT_STYLE s; DWRITE_FONT_STRETCH st; float sz; WCHAR b[64];
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, TL_NARROW, X.coll, DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_SEMI_CONDENSED, 11.25f, L"de-DE", &t));
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 200.0f, 100.0f, &l));
    if (!l) { DW_RELEASE(t); TL_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetFontWeight(l, 0, &w, &r)); EXPECT_EQ(w, DWRITE_FONT_WEIGHT_LIGHT);
    CHECK_HR_OK(IDWriteTextLayout_GetFontStyle(l, 0, &s, &r)); EXPECT_EQ(s, DWRITE_FONT_STYLE_ITALIC);
    CHECK_HR_OK(IDWriteTextLayout_GetFontStretch(l, 0, &st, &r)); EXPECT_EQ(st, DWRITE_FONT_STRETCH_SEMI_CONDENSED);
    CHECK_HR_OK(IDWriteTextLayout_GetFontSize(l, 0, &sz, &r)); CHECK_FLOAT_EQ(sz, 11.25f);
    CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 0, b, 64, &r)); CHECK_WEQ(b, TL_NARROW);
    CHECK_HR_OK(IDWriteTextLayout_GetLocaleName(l, 0, b, 64, &r)); CHECK(!_wcsicmp(b, L"de-DE"));
    DW_RELEASE(l); DW_RELEASE(t); TL_END();
}

TEST(format_changes_after_creation_do_not_propagate)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextLayout *l = NULL; tl_state s0, s1;
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 200.0f, 100.0f, &l));
    if (!l) { DW_RELEASE(t); TL_END(); t_skip("layout"); return; }
    tl_snap(l, &s0);
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_CENTER));
    CHECK_HR_OK(IDWriteTextFormat_SetParagraphAlignment(t, DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
    CHECK_HR_OK(IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(t, 99.0f));
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 50.0f, 40.0f));
    { DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }; CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, NULL)); }
    tl_snap(l, &s1);
    CHECK(tl_state_eq(&s0, &s1));
    DW_RELEASE(l); DW_RELEASE(t); TL_END();
}

TEST(layout_changes_do_not_propagate_to_format)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 200.0f, 100.0f, &l));
    if (!l) { DW_RELEASE(t); TL_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING));
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 77.0f));
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 99.0f, RNG(0, 4)));
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(t), DWRITE_TEXT_ALIGNMENT_LEADING);
    EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(t), DWRITE_WORD_WRAPPING_WRAP);
    CHECK(IDWriteTextFormat_GetIncrementalTabStop(t) != 77.0f);
    CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(t), 16.0f);
    DW_RELEASE(l); DW_RELEASE(t); TL_END();
}

TEST(layout_format_view_shares_state)
{
    /* setters on the IDWriteTextFormat view act on the layout object itself */
    LY(l);
    IDWriteTextFormat *fv = NULL;
    CHECK_HR_OK(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat, (void **)&fv));
    if (fv) {
        CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(fv, DWRITE_TEXT_ALIGNMENT_TRAILING));
        EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_TRAILING);
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_CHARACTER));
        EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(fv), DWRITE_WORD_WRAPPING_CHARACTER);
        CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(fv, 12.5f));
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetIncrementalTabStop(l), 12.5f);
        CHECK_HR(IDWriteTextFormat_SetTextAlignment(fv, (DWRITE_TEXT_ALIGNMENT)4), E_INVALIDARG);
        IDWriteTextFormat_Release(fv);
    }
    DW_RELEASE(l); TL_END();
}

/* ---- enum properties ----------------------------------------------------------------------------------------------------- */
typedef struct { const char *name; HRESULT (*set)(IDWriteTextLayout *, UINT32); UINT32 (*get)(IDWriteTextLayout *); UINT32 nvalid; UINT32 def; } enum_prop;
static HRESULT s_ta(IDWriteTextLayout *l, UINT32 v) { return IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)v); }
static UINT32  g_ta(IDWriteTextLayout *l) { return IDWriteTextLayout_GetTextAlignment(l); }
static HRESULT s_pa(IDWriteTextLayout *l, UINT32 v) { return IDWriteTextLayout_SetParagraphAlignment(l, (DWRITE_PARAGRAPH_ALIGNMENT)v); }
static UINT32  g_pa(IDWriteTextLayout *l) { return IDWriteTextLayout_GetParagraphAlignment(l); }
static HRESULT s_ww(IDWriteTextLayout *l, UINT32 v) { return IDWriteTextLayout_SetWordWrapping(l, (DWRITE_WORD_WRAPPING)v); }
static UINT32  g_ww(IDWriteTextLayout *l) { return IDWriteTextLayout_GetWordWrapping(l); }
static HRESULT s_rd(IDWriteTextLayout *l, UINT32 v) { return IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)v); }
static UINT32  g_rd(IDWriteTextLayout *l) { return IDWriteTextLayout_GetReadingDirection(l); }
static HRESULT s_fd(IDWriteTextLayout *l, UINT32 v) { return IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)v); }
static UINT32  g_fd(IDWriteTextLayout *l) { return IDWriteTextLayout_GetFlowDirection(l); }
static HRESULT s_vo(IDWriteTextLayout *l, UINT32 v) { IDWriteTextLayout2 *l2 = NULL; HRESULT hr; if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2))) return 0x7fffffff; hr = IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, (DWRITE_VERTICAL_GLYPH_ORIENTATION)v); IDWriteTextLayout2_Release(l2); return hr; }
static UINT32  g_vo(IDWriteTextLayout *l) { IDWriteTextLayout2 *l2 = NULL; UINT32 v = 99; if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2))) { v = IDWriteTextLayout2_GetVerticalGlyphOrientation(l2); IDWriteTextLayout2_Release(l2); } return v; }
static HRESULT s_oa(IDWriteTextLayout *l, UINT32 v) { IDWriteTextLayout2 *l2 = NULL; HRESULT hr; if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2))) return 0x7fffffff; hr = IDWriteTextLayout2_SetOpticalAlignment(l2, (DWRITE_OPTICAL_ALIGNMENT)v); IDWriteTextLayout2_Release(l2); return hr; }
static UINT32  g_oa(IDWriteTextLayout *l) { IDWriteTextLayout2 *l2 = NULL; UINT32 v = 99; if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&l2))) { v = IDWriteTextLayout2_GetOpticalAlignment(l2); IDWriteTextLayout2_Release(l2); } return v; }
static HRESULT s_af(IDWriteTextLayout *l, UINT32 v) { IDWriteTextLayout4 *l4 = NULL; HRESULT hr; if (FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4))) return 0x7fffffff; hr = IDWriteTextLayout4_SetAutomaticFontAxes(l4, (DWRITE_AUTOMATIC_FONT_AXES)v); IDWriteTextLayout4_Release(l4); return hr; }
static UINT32  g_af(IDWriteTextLayout *l) { IDWriteTextLayout4 *l4 = NULL; UINT32 v = 99; if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4))) { v = IDWriteTextLayout4_GetAutomaticFontAxes(l4); IDWriteTextLayout4_Release(l4); } return v; }

static const enum_prop ENUMS[] = {
    { "textAlignment", s_ta, g_ta, 4, 0 }, { "paragraphAlignment", s_pa, g_pa, 3, 0 }, { "wordWrapping", s_ww, g_ww, 5, 0 },
    { "readingDirection", s_rd, g_rd, 4, 0 }, { "flowDirection", s_fd, g_fd, 4, 0 },
    { "verticalGlyphOrientation", s_vo, g_vo, 2, 0 }, { "opticalAlignment", s_oa, g_oa, 2, 0 }, { "automaticFontAxes", s_af, g_af, 2, 0 },
};
#define NENUMS ((int)(sizeof ENUMS / sizeof ENUMS[0]))

TEST(enum_defaults)
{
    LY(l);
    int i;
    for (i = 0; i < NENUMS; i++) CHECKF(ENUMS[i].get(l) == ENUMS[i].def, "%s default %u", ENUMS[i].name, ENUMS[i].get(l));
    DW_RELEASE(l); TL_END();
}

TEST(enum_valid_values_roundtrip)
{
    LY(l);
    int i; UINT32 v;
    for (i = 0; i < NENUMS; i++)
        for (v = 0; v < ENUMS[i].nvalid; v++) {
            HRESULT hr = ENUMS[i].set(l, v);
            if (hr == 0x7fffffff) break;
            CHECKF(hr == S_OK, "%s=%u -> %s", ENUMS[i].name, v, dw_hr_name(hr));
            CHECKF(ENUMS[i].get(l) == v, "%s=%u read back %u", ENUMS[i].name, v, ENUMS[i].get(l));
            ENUMS[i].set(l, ENUMS[i].def);
        }
    DW_RELEASE(l); TL_END();
}

TEST(enum_invalid_values_rejected_and_ignored)
{
    LY(l);
    int i; size_t k; static const UINT32 extra[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 16, 255, 256, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    for (i = 0; i < NENUMS; i++) {
        UINT32 before;
        ENUMS[i].set(l, ENUMS[i].nvalid > 1 ? 1 : 0);
        before = ENUMS[i].get(l);
        for (k = 0; k < sizeof extra / sizeof extra[0]; k++) {
            HRESULT hr; UINT32 v = extra[k];
            if (v < ENUMS[i].nvalid) continue;
            hr = ENUMS[i].set(l, v);
            if (hr == 0x7fffffff) break;
            CHECKF(hr == E_INVALIDARG, "%s=%u -> %s", ENUMS[i].name, v, dw_hr_name(hr));
            CHECKF(ENUMS[i].get(l) == before, "%s changed to %u by a rejected call", ENUMS[i].name, ENUMS[i].get(l));
        }
        ENUMS[i].set(l, ENUMS[i].def);
    }
    DW_RELEASE(l); TL_END();
}

TEST(enum_setters_are_independent)
{
    LY(l);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK));
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM));
    EXPECT_EQ(IDWriteTextLayout_GetTextAlignment(l), DWRITE_TEXT_ALIGNMENT_CENTER);
    EXPECT_EQ(IDWriteTextLayout_GetParagraphAlignment(l), DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    EXPECT_EQ(IDWriteTextLayout_GetWordWrapping(l), DWRITE_WORD_WRAPPING_EMERGENCY_BREAK);
    EXPECT_EQ(IDWriteTextLayout_GetReadingDirection(l), DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    EXPECT_EQ(IDWriteTextLayout_GetFlowDirection(l), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    DW_RELEASE(l); TL_END();
}

TEST(enum_constants_match_header)
{
    CHECK(DWRITE_TEXT_ALIGNMENT_LEADING == 0 && DWRITE_TEXT_ALIGNMENT_TRAILING == 1 && DWRITE_TEXT_ALIGNMENT_CENTER == 2 && DWRITE_TEXT_ALIGNMENT_JUSTIFIED == 3);
    CHECK(DWRITE_PARAGRAPH_ALIGNMENT_NEAR == 0 && DWRITE_PARAGRAPH_ALIGNMENT_FAR == 1 && DWRITE_PARAGRAPH_ALIGNMENT_CENTER == 2);
    CHECK(DWRITE_WORD_WRAPPING_WRAP == 0 && DWRITE_WORD_WRAPPING_NO_WRAP == 1 && DWRITE_WORD_WRAPPING_EMERGENCY_BREAK == 2 && DWRITE_WORD_WRAPPING_WHOLE_WORD == 3 && DWRITE_WORD_WRAPPING_CHARACTER == 4);
    CHECK(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT == 0 && DWRITE_READING_DIRECTION_RIGHT_TO_LEFT == 1 && DWRITE_READING_DIRECTION_TOP_TO_BOTTOM == 2 && DWRITE_READING_DIRECTION_BOTTOM_TO_TOP == 3);
    CHECK(DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM == 0 && DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP == 1 && DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT == 2 && DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT == 3);
}

/* ---- reading / flow direction conflicts -------------------------------------------------------------------------------- */
TEST(direction_conflict_table)
{
    /* a reading direction along the same axis as the flow direction is unusable: horizontal reading needs a vertical flow and
     * vice versa. The setters accept every pair; GetMetrics reports the conflict. */
    LY(l);
    UINT32 rd, fd;
    for (rd = 0; rd < 4; rd++)
        for (fd = 0; fd < 4; fd++) {
            DWRITE_TEXT_METRICS m; HRESULT hr; int conflict = ((rd >> 1) ^ (fd >> 1)) & 1;
            CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)rd));
            CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)fd));
            memset(&m, 0xcc, sizeof m);
            hr = IDWriteTextLayout_GetMetrics(l, &m);
            CHECKF(hr == (conflict ? DW_E_FLOWCONFLICT : S_OK), "reading %u flow %u -> %s", rd, fd, dw_hr_name(hr));
        }
    DW_RELEASE(l); TL_END();
}

TEST(direction_conflict_reported_by_other_operations)
{
    LY(l);
    DWRITE_TEXT_METRICS m; DWRITE_OVERHANG_METRICS oh; float mw; UINT32 n = 0; DWRITE_LINE_METRICS lm[2];
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT));        /* with the default LTR reading direction */
    CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), DW_E_FLOWCONFLICT);
    CHECK_HR(IDWriteTextLayout_GetOverhangMetrics(l, &oh), DW_E_FLOWCONFLICT);
    CHECK_HR(IDWriteTextLayout_DetermineMinWidth(l, &mw), DW_E_FLOWCONFLICT);
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, lm, 2, &n), DW_E_FLOWCONFLICT);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &oh));
    CHECK_HR_OK(IDWriteTextLayout_DetermineMinWidth(l, &mw));
    DW_RELEASE(l); TL_END();
}

TEST(direction_conflict_from_format)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m; HRESULT hr;
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM));     /* vertical reading + default vertical flow */
    hr = IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 200.0f, 100.0f, &l);
    CHECK_HR_OK(hr);                                    /* creation itself does not lay the text out */
    if (l) { CHECK_HR(IDWriteTextLayout_GetMetrics(l, &m), DW_E_FLOWCONFLICT); DW_RELEASE(l); }
    DW_RELEASE(t); TL_END();
}

TEST(direction_vertical_reading_with_horizontal_flow_ok)
{
    LY(l);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM));
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK(m.lineCount >= 1);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_BOTTOM_TO_TOP));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    DW_RELEASE(l); TL_END();
}

/* ---- incremental tab stop ---------------------------------------------------------------------------------------------- */
TEST(tabstop_roundtrip)
{
    LY(l);
    static const float v[] = { 1.0f, 0.5f, 4.0f, 100.0f, 33.25f, FLT_MIN, 1e-20f, 1e20f, FLT_MAX, INFINITY };
    size_t k;
    for (k = 0; k < sizeof v / sizeof v[0]; k++) {
        CHECKF(IDWriteTextLayout_SetIncrementalTabStop(l, v[k]) == S_OK, "tab %g", v[k]);
        CHECK_FBITS(IDWriteTextLayout_GetIncrementalTabStop(l), v[k]);
    }
    DW_RELEASE(l); TL_END();
}

TEST(tabstop_invalid)
{
    LY(l);
    static const float bad[] = { 0.0f, -0.0f, -1.0f, -0.5f, -FLT_MIN, -FLT_MAX, -INFINITY, NAN, -NAN };
    size_t k;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 24.0f));
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetIncrementalTabStop(l, bad[k]);
        CHECKF(hr == E_INVALIDARG, "tab %g -> %s", bad[k], dw_hr_name(hr));
        CHECK_FBITS(IDWriteTextLayout_GetIncrementalTabStop(l), 24.0f);
    }
    DW_RELEASE(l); TL_END();
}

/* ---- max width / height --------------------------------------------------------------------------------------------------- */
TEST(maxsize_roundtrip)
{
    LY(l);
    static const float v[] = { 0.0f, -0.0f, 1.0f, 0.25f, 1e6f, 123.456f, FLT_MIN, FLT_MAX, INFINITY };
    size_t k;
    for (k = 0; k < sizeof v / sizeof v[0]; k++) {
        /* the setters skip the store when the new value compares equal, so -0.0 after 0.0 keeps +0.0: compare by value */
        CHECKF(IDWriteTextLayout_SetMaxWidth(l, v[k]) == S_OK, "w %g", v[k]);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxWidth(l), v[k]);
        CHECKF(IDWriteTextLayout_SetMaxHeight(l, v[k]) == S_OK, "h %g", v[k]);
        CHECK_FLOAT_EQ(IDWriteTextLayout_GetMaxHeight(l), v[k]);
    }
    DW_RELEASE(l); TL_END();
}

TEST(maxsize_invalid)
{
    LY(l);
    static const float bad[] = { -1.0f, -0.001f, -FLT_MIN, -FLT_MAX, -INFINITY, NAN, -NAN };
    size_t k;
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetMaxWidth(l, bad[k]); CHECKF(hr == E_INVALIDARG, "w %g -> %s", bad[k], dw_hr_name(hr));
        hr = IDWriteTextLayout_SetMaxHeight(l, bad[k]); CHECKF(hr == E_INVALIDARG, "h %g -> %s", bad[k], dw_hr_name(hr));
        CHECK_FBITS(IDWriteTextLayout_GetMaxWidth(l), 200.0f); CHECK_FBITS(IDWriteTextLayout_GetMaxHeight(l), 100.0f);
    }
    DW_RELEASE(l); TL_END();
}

TEST(maxsize_independent)
{
    LY(l);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 7.0f));
    CHECK_FBITS(IDWriteTextLayout_GetMaxHeight(l), 100.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 9.0f));
    CHECK_FBITS(IDWriteTextLayout_GetMaxWidth(l), 7.0f);
    DW_RELEASE(l); TL_END();
}

TEST(maxsize_reflected_in_metrics)
{
    LY(l);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.layoutWidth, 200.0f); CHECK_FLOAT_EQ(m.layoutHeight, 100.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 300.5f)); CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 40.25f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.layoutWidth, 300.5f); CHECK_FLOAT_EQ(m.layoutHeight, 40.25f);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 0.0f)); CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 0.0f));
    tl_metrics(l, &m); CHECK_FLOAT_EQ(m.layoutWidth, 0.0f); CHECK_FLOAT_EQ(m.layoutHeight, 0.0f);
    DW_RELEASE(l); TL_END();
}

/* ---- trimming ----------------------------------------------------------------------------------------------------------------- */
TEST(trimming_roundtrip_granularities)
{
    LY(l);
    UINT32 g;
    for (g = 0; g <= 2; g++) {
        DWRITE_TRIMMING tr = { (DWRITE_TRIMMING_GRANULARITY)g, 0x2e, 4 }, got; IDWriteInlineObject *s = (IDWriteInlineObject *)(ULONG_PTR)1;
        CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, NULL));
        memset(&got, 0xcc, sizeof got);
        CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &got, &s));
        EXPECT_EQ(got.granularity, g); EXPECT_EQ(got.delimiter, 0x2e); EXPECT_EQ(got.delimiterCount, 4); CHECK(s == NULL);
    }
    DW_RELEASE(l); TL_END();
}

TEST(trimming_invalid)
{
    LY(l);
    static const DWRITE_TRIMMING bad[] = {
        { (DWRITE_TRIMMING_GRANULARITY)3, 0, 0 }, { (DWRITE_TRIMMING_GRANULARITY)4, 0, 0 }, { (DWRITE_TRIMMING_GRANULARITY)0xffffffffu, 0, 0 },
        { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0x110000, 0 }, { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0xffffffffu, 0 }, { DWRITE_TRIMMING_GRANULARITY_NONE, 0x80000000u, 1 },
    };
    DWRITE_TRIMMING ok = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x41, 2 }, got; IDWriteInlineObject *s;
    size_t k;
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &ok, NULL));
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetTrimming(l, &bad[k], NULL);
        CHECKF(hr == E_INVALIDARG, "row %u gran %u delim %#x -> %s", (unsigned)k, bad[k].granularity, bad[k].delimiter, dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &got, &s));
        EXPECT_EQ(got.granularity, DWRITE_TRIMMING_GRANULARITY_WORD); EXPECT_EQ(got.delimiter, 0x41); EXPECT_EQ(got.delimiterCount, 2);
    }
    if (!t_is_wine()) CHECK_HR(IDWriteTextLayout_SetTrimming(l, NULL, NULL), E_INVALIDARG);      /* the emulated implementation dereferences it */
    CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &got, &s)); EXPECT_EQ(got.granularity, DWRITE_TRIMMING_GRANULARITY_WORD);
    DW_RELEASE(l); TL_END();
}

TEST(trimming_delimiter_bounds)
{
    LY(l);
    static const UINT32 good[] = { 0, 1, 0x20, 0xFFFF, 0x10000, 0x10FFFF };
    size_t k; DWRITE_TRIMMING got; IDWriteInlineObject *s;
    for (k = 0; k < sizeof good / sizeof good[0]; k++) {
        DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, good[k], 1 };
        HRESULT hr = IDWriteTextLayout_SetTrimming(l, &tr, NULL);
        CHECKF(hr == S_OK, "delimiter %#x -> %s", good[k], dw_hr_name(hr));
        IDWriteTextLayout_GetTrimming(l, &got, &s); EXPECT_EQ(got.delimiter, good[k]);
    }
    DW_RELEASE(l); TL_END();
}

TEST(trimming_delimiter_count_unvalidated)
{
    LY(l);
    static const UINT32 cnt[] = { 0, 1, 100, 0x7fffffff, 0x80000000u, 0xffffffffu };
    size_t k; DWRITE_TRIMMING got; IDWriteInlineObject *s;
    for (k = 0; k < sizeof cnt / sizeof cnt[0]; k++) {
        DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2e, cnt[k] };
        HRESULT hr = IDWriteTextLayout_SetTrimming(l, &tr, NULL);
        CHECKF(hr == S_OK, "count %u -> %s", cnt[k], dw_hr_name(hr));
        IDWriteTextLayout_GetTrimming(l, &got, &s); EXPECT_EQ(got.delimiterCount, cnt[k]);
    }
    DW_RELEASE(l); TL_END();
}

TEST(trimming_sign_refcounts)
{
    LY(l);
    dwx_inline *a = dwx_inline_new(5, 5, 4, FALSE), *b = dwx_inline_new(6, 6, 5, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }, got; IDWriteInlineObject *s = NULL;
    LONG base;
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, (IDWriteInlineObject *)a));
    EXPECT_EQ(dwx_refs(a), 2);                                  /* the layout holds one reference */
    CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &got, &s)); CHECK(s == (IDWriteInlineObject *)a);
    EXPECT_EQ(dwx_refs(a), 3);                                  /* GetTrimming returns an owned reference */
    IDWriteInlineObject_Release(s); EXPECT_EQ(dwx_refs(a), 2);
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, (IDWriteInlineObject *)a));      /* same object again: still one reference */
    EXPECT_EQ(dwx_refs(a), 2);
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, (IDWriteInlineObject *)b));      /* replaced: old released, new retained */
    EXPECT_EQ(dwx_refs(a), 1); EXPECT_EQ(dwx_refs(b), 2);
    { DWRITE_TRIMMING bad = { (DWRITE_TRIMMING_GRANULARITY)9, 0, 0 };
      CHECK_HR(IDWriteTextLayout_SetTrimming(l, &bad, (IDWriteInlineObject *)a), E_INVALIDARG);   /* a failed call retains nothing and releases nothing */
      EXPECT_EQ(dwx_refs(a), 1); EXPECT_EQ(dwx_refs(b), 2); }
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, NULL));
    EXPECT_EQ(dwx_refs(b), 1);
    base = dwx_refs(a);
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, (IDWriteInlineObject *)a)); EXPECT_EQ(dwx_refs(a), base + 1);
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(a), base);                               /* the layout released its sign when it died */
    dwx_release(a); dwx_release(b); TL_END();
}

TEST(trimming_sign_is_not_called)
{
    /* setting a sign never queries it; its methods run only when the layout is drawn */
    LY(l);
    dwx_inline *a = dwx_inline_new(5, 5, 4, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, (IDWriteInlineObject *)a));
    tl_lines(l);
    t_note("sign GetMetrics calls during layout: %u", (unsigned)a->n_metrics);
    EXPECT_EQ(a->n_draw, 0); EXPECT_EQ(a->n_overhang, 0); EXPECT_EQ(a->n_break, 0);
    DW_RELEASE(l); dwx_release(a); TL_END();
}

TEST(trimming_sign_replaced_by_format_snapshot)
{
    /* the trimming sign handed over by the format is shared with the layout, not copied */
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    dwx_inline *a = dwx_inline_new(5, 5, 4, FALSE); DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
    if (!t) { dwx_release(a); TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, (IDWriteInlineObject *)a));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 200.0f, 100.0f, &l));
    if (l) {
        IDWriteInlineObject *s = NULL; DWRITE_TRIMMING g;
        CHECK(dwx_refs(a) >= 3);
        CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &g, &s)); CHECK(s == (IDWriteInlineObject *)a); DW_RELEASE(s);
        DW_RELEASE(t);
        CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &g, &s)); CHECK(s == (IDWriteInlineObject *)a); DW_RELEASE(s);
        DW_RELEASE(l);
    }
    DW_RELEASE(t); EXPECT_EQ(dwx_refs(a), 1); dwx_release(a); TL_END();
}

/* ---- line spacing (legacy triple) ----------------------------------------------------------------------------------------------- */
TEST(linespacing_legacy_roundtrip)
{
    LY(l);
    static const struct { DWRITE_LINE_SPACING_METHOD m; float h, b; } v[] = {
        { DWRITE_LINE_SPACING_METHOD_DEFAULT, 0, 0 }, { DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.0f, 15.0f }, { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 0.0f },
        { DWRITE_LINE_SPACING_METHOD_DEFAULT, 12.0f, 7.0f }, { DWRITE_LINE_SPACING_METHOD_UNIFORM, 0.0f, 0.0f }, { DWRITE_LINE_SPACING_METHOD_UNIFORM, 1e6f, 1e6f },
        { DWRITE_LINE_SPACING_METHOD_DEFAULT, 0.0f, -10.0f }, { DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.0f, -5.5f }, { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, FLT_MAX, -FLT_MAX },
        { DWRITE_LINE_SPACING_METHOD_UNIFORM, INFINITY, INFINITY },
    };
    size_t k;
    for (k = 0; k < sizeof v / sizeof v[0]; k++) {
        DWRITE_LINE_SPACING_METHOD m; float h, b;
        HRESULT hr = IDWriteTextLayout_SetLineSpacing(l, v[k].m, v[k].h, v[k].b);
        CHECKF(hr == S_OK, "row %u -> %s", (unsigned)k, dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &h, &b));
        EXPECT_EQ(m, v[k].m); CHECK_FBITS(h, v[k].h); CHECK_FBITS(b, v[k].b);
    }
    DW_RELEASE(l); TL_END();
}

TEST(linespacing_legacy_invalid)
{
    LY(l);
    static const struct { UINT32 m; float h, b; } bad[] = {
        { 3, 0, 0 }, { 4, 10, 10 }, { 0xffffffffu, 0, 0 }, { 0, -1.0f, 0 }, { 1, -0.001f, 5 }, { 2, -FLT_MAX, 0 }, { 0, -INFINITY, 0 },
        { 0, NAN, 0 }, { 1, 10, NAN }, { 2, NAN, NAN }, { 0, 0, -NAN }, { 1, 10, -NAN },
    };
    size_t k; DWRITE_LINE_SPACING_METHOD m; float h, b;
    CHECK_HR_OK(IDWriteTextLayout_SetLineSpacing(l, DWRITE_LINE_SPACING_METHOD_UNIFORM, 18.0f, 14.0f));
    for (k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = IDWriteTextLayout_SetLineSpacing(l, (DWRITE_LINE_SPACING_METHOD)bad[k].m, bad[k].h, bad[k].b);
        CHECKF(hr == E_INVALIDARG, "row %u -> %s", (unsigned)k, dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextLayout_GetLineSpacing(l, &m, &h, &b));
        EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(h, 18.0f); CHECK_FLOAT_EQ(b, 14.0f);
    }
    DW_RELEASE(l); TL_END();
}

TEST(linespacing_matches_format_validation)
{
    /* the same triple validation holds on the format object and on the layout */
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f);
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_DEFAULT, 0.0f, -10.0f));
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_DEFAULT, -10.0f, 0.0f), E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(t, (DWRITE_LINE_SPACING_METHOD)3, 0.0f, 0.0f), E_INVALIDARG);
    DW_RELEASE(t); TL_END();
}
