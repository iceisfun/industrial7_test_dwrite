/* Property setters/getters of IDWriteTextFormat / IDWriteTextFormat1 that take enumerations or plain numbers.
 *
 * Each setter validates its argument against the enumeration's range only (unsigned compare, so negative values are out of range
 * too) and returns E_INVALIDARG, leaving the stored value untouched; there is no cross-property validation at set time. */
#include "tf.h"

static const UINT32 bad_tail[] = { 99, 100, 255, 256, 0xffff, 0x10000, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu };

/* Generates the standard suite for one enumeration property.
 *   NAME = method suffix, PFX = interface whose macros are used, ETYPE, NVALID = number of accepted values (0..NVALID-1), DEFV = default */
#define ENUM_SUITE(NAME, PFX, ETYPE, NVALID, DEFV) \
TEST(NAME##_default) { TF_NEW(t); TF_QI(p, PFX, t); EXPECT_EQ(PFX##_Get##NAME(p), DEFV); DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_valid_roundtrip) { \
    TF_NEW(t); TF_QI(p, PFX, t); int v, r; \
    for (r = 0; r < 2; r++) for (v = 0; v < (int)(NVALID); v++) { \
        CHECK_HR_OK(PFX##_Set##NAME(p, (ETYPE)v)); EXPECT_EQ(PFX##_Get##NAME(p), v); } \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_valid_values_in_reverse_order) { \
    TF_NEW(t); TF_QI(p, PFX, t); int v; \
    for (v = (int)(NVALID) - 1; v >= 0; v--) { CHECK_HR_OK(PFX##_Set##NAME(p, (ETYPE)v)); EXPECT_EQ(PFX##_Get##NAME(p), v); } \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_first_invalid_value_rejected) { \
    TF_NEW(t); TF_QI(p, PFX, t); \
    CHECK_HR(PFX##_Set##NAME(p, (ETYPE)(NVALID)), E_INVALIDARG); EXPECT_EQ(PFX##_Get##NAME(p), DEFV); \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_invalid_values_leave_state_unchanged) { \
    TF_NEW(t); TF_QI(p, PFX, t); size_t i; tf_state snap; \
    CHECK_HR_OK(PFX##_Set##NAME(p, (ETYPE)((NVALID) - 1))); tf_snap(t, &snap); \
    for (i = 0; i < sizeof bad_tail / sizeof bad_tail[0]; i++) { \
        HRESULT hr = PFX##_Set##NAME(p, (ETYPE)bad_tail[i]); \
        CHECKF(hr == E_INVALIDARG, "value 0x%x -> %s", bad_tail[i], dw_hr_name(hr)); \
        EXPECT_EQ(PFX##_Get##NAME(p), (NVALID) - 1); } \
    CHECK_STATE_SAME(t, snap); \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_range_scan) { \
    TF_NEW(t); TF_QI(p, PFX, t); int v, last = DEFV; \
    for (v = -40; v <= 40; v++) { \
        HRESULT hr = PFX##_Set##NAME(p, (ETYPE)v); \
        if (v >= 0 && v < (int)(NVALID)) { CHECKF(hr == S_OK, "value %d -> %s", v, dw_hr_name(hr)); last = v; } \
        else CHECKF(hr == E_INVALIDARG, "value %d -> %s", v, dw_hr_name(hr)); \
        EXPECT_EQ(PFX##_Get##NAME(p), last); } \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_set_leaves_other_properties_alone) { \
    TF_NEW(t); TF_QI(p, PFX, t); tf_state a, b; DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2e, 3 }; \
    IDWriteTextFormat_SetTrimming(t, &tr, NULL); IDWriteTextFormat_SetIncrementalTabStop(t, 33.0f); IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.0f, 15.0f); \
    tf_snap(t, &a); CHECK_HR_OK(PFX##_Set##NAME(p, (ETYPE)((NVALID) - 1))); tf_snap(t, &b); \
    /* only the property under test may differ: restore it in the snapshot comparison by writing the default back */ \
    CHECK_HR_OK(PFX##_Set##NAME(p, (ETYPE)(DEFV))); tf_snap(t, &b); CHECK(tf_state_eq(&a, &b) || (NVALID) - 1 == DEFV); \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_formats_are_independent) { \
    TF_NEED_FACTORY(); IDWriteTextFormat *a = tf_new(f), *b = tf_new(f); \
    TF_QI(pa, PFX, a); TF_QI(pb, PFX, b); \
    CHECK_HR_OK(PFX##_Set##NAME(pa, (ETYPE)((NVALID) - 1))); \
    EXPECT_EQ(PFX##_Get##NAME(pa), (NVALID) - 1); EXPECT_EQ(PFX##_Get##NAME(pb), DEFV); \
    CHECK_HR_OK(PFX##_Set##NAME(pb, (ETYPE)(DEFV))); EXPECT_EQ(PFX##_Get##NAME(pa), (NVALID) - 1); \
    DW_RELEASE(pa); DW_RELEASE(pb); DW_RELEASE(a); DW_RELEASE(b); } \
TEST(NAME##_does_not_change_refcount) { \
    TF_NEW(t); TF_QI(p, PFX, t); ULONG r0 = dw_refcount((IUnknown *)t); \
    PFX##_Set##NAME(p, (ETYPE)((NVALID) - 1)); PFX##_Get##NAME(p); PFX##_Set##NAME(p, (ETYPE)(NVALID)); \
    EXPECT_REFS(t, r0); DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_set_through_textformat3_read_through_base) { \
    TF_NEW(t); TF_QI(t3, IDWriteTextFormat3, t); TF_QI(p, PFX, t); int v; \
    for (v = 0; v < (int)(NVALID); v++) { CHECK_HR_OK(IDWriteTextFormat3_Set##NAME(t3, (ETYPE)v)); EXPECT_EQ(PFX##_Get##NAME(p), v); EXPECT_EQ(IDWriteTextFormat3_Get##NAME(t3), v); } \
    CHECK_HR(IDWriteTextFormat3_Set##NAME(t3, (ETYPE)(NVALID)), E_INVALIDARG); \
    DW_RELEASE(p); DW_RELEASE(t3); DW_RELEASE(t); } \
TEST(NAME##_set_through_textformat2_read_through_base) { \
    TF_NEW(t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(p, PFX, t); int v; \
    for (v = 0; v < (int)(NVALID); v++) { CHECK_HR_OK(IDWriteTextFormat2_Set##NAME(t2, (ETYPE)v)); EXPECT_EQ(PFX##_Get##NAME(p), v); EXPECT_EQ(IDWriteTextFormat2_Get##NAME(t2), v); } \
    CHECK_HR(IDWriteTextFormat2_Set##NAME(t2, (ETYPE)(NVALID)), E_INVALIDARG); \
    DW_RELEASE(p); DW_RELEASE(t2); DW_RELEASE(t); }

/* Properties introduced with IDWriteTextFormat1 are reachable through 1/2/3 only; macro names for them exist from Format1 on. */
#define ENUM_SUITE_F1(NAME, ETYPE, NVALID, DEFV) \
TEST(NAME##_default) { TF_NEW(t); TF_QI(p, IDWriteTextFormat1, t); EXPECT_EQ(IDWriteTextFormat1_Get##NAME(p), DEFV); DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_valid_roundtrip) { \
    TF_NEW(t); TF_QI(p, IDWriteTextFormat1, t); int v, r; \
    for (r = 0; r < 2; r++) for (v = 0; v < (int)(NVALID); v++) { CHECK_HR_OK(IDWriteTextFormat1_Set##NAME(p, (ETYPE)v)); EXPECT_EQ(IDWriteTextFormat1_Get##NAME(p), v); } \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_invalid_values_leave_state_unchanged) { \
    TF_NEW(t); TF_QI(p, IDWriteTextFormat1, t); size_t i; \
    CHECK_HR_OK(IDWriteTextFormat1_Set##NAME(p, (ETYPE)((NVALID) - 1))); \
    for (i = 0; i < sizeof bad_tail / sizeof bad_tail[0]; i++) { \
        HRESULT hr = IDWriteTextFormat1_Set##NAME(p, (ETYPE)bad_tail[i]); \
        CHECKF(hr == E_INVALIDARG, "value 0x%x -> %s", bad_tail[i], dw_hr_name(hr)); EXPECT_EQ(IDWriteTextFormat1_Get##NAME(p), (NVALID) - 1); } \
    CHECK_HR(IDWriteTextFormat1_Set##NAME(p, (ETYPE)(NVALID)), E_INVALIDARG); \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_range_scan) { \
    TF_NEW(t); TF_QI(p, IDWriteTextFormat1, t); int v, last = DEFV; \
    for (v = -40; v <= 40; v++) { HRESULT hr = IDWriteTextFormat1_Set##NAME(p, (ETYPE)v); \
        if (v >= 0 && v < (int)(NVALID)) { CHECKF(hr == S_OK, "value %d -> %s", v, dw_hr_name(hr)); last = v; } else CHECKF(hr == E_INVALIDARG, "value %d -> %s", v, dw_hr_name(hr)); \
        EXPECT_EQ(IDWriteTextFormat1_Get##NAME(p), last); } \
    DW_RELEASE(p); DW_RELEASE(t); } \
TEST(NAME##_formats_are_independent) { \
    TF_NEED_FACTORY(); IDWriteTextFormat *a = tf_new(f), *b = tf_new(f); TF_QI(pa, IDWriteTextFormat1, a); TF_QI(pb, IDWriteTextFormat1, b); \
    CHECK_HR_OK(IDWriteTextFormat1_Set##NAME(pa, (ETYPE)((NVALID) - 1))); \
    EXPECT_EQ(IDWriteTextFormat1_Get##NAME(pa), (NVALID) - 1); EXPECT_EQ(IDWriteTextFormat1_Get##NAME(pb), DEFV); \
    DW_RELEASE(pa); DW_RELEASE(pb); DW_RELEASE(a); DW_RELEASE(b); } \
TEST(NAME##_set_through_textformat3) { \
    TF_NEW(t); TF_QI(t3, IDWriteTextFormat3, t); TF_QI(p, IDWriteTextFormat1, t); int v; \
    for (v = 0; v < (int)(NVALID); v++) { CHECK_HR_OK(IDWriteTextFormat3_Set##NAME(t3, (ETYPE)v)); EXPECT_EQ(IDWriteTextFormat1_Get##NAME(p), v); EXPECT_EQ(IDWriteTextFormat3_Get##NAME(t3), v); } \
    CHECK_HR(IDWriteTextFormat3_Set##NAME(t3, (ETYPE)(NVALID)), E_INVALIDARG); \
    DW_RELEASE(p); DW_RELEASE(t3); DW_RELEASE(t); } \
TEST(NAME##_set_through_textformat2) { \
    TF_NEW(t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(p, IDWriteTextFormat1, t); int v; \
    for (v = 0; v < (int)(NVALID); v++) { CHECK_HR_OK(IDWriteTextFormat2_Set##NAME(t2, (ETYPE)v)); EXPECT_EQ(IDWriteTextFormat1_Get##NAME(p), v); } \
    DW_RELEASE(p); DW_RELEASE(t2); DW_RELEASE(t); } \
TEST(NAME##_does_not_change_refcount) { \
    TF_NEW(t); TF_QI(p, IDWriteTextFormat1, t); ULONG r0 = dw_refcount((IUnknown *)t); \
    IDWriteTextFormat1_Set##NAME(p, (ETYPE)((NVALID) - 1)); IDWriteTextFormat1_Get##NAME(p); IDWriteTextFormat1_Set##NAME(p, (ETYPE)(NVALID)); \
    EXPECT_REFS(t, r0); DW_RELEASE(p); DW_RELEASE(t); }

/* ---- text alignment: LEADING, TRAILING, CENTER, JUSTIFIED ---------------------------------------------------------- */
ENUM_SUITE(TextAlignment, IDWriteTextFormat, DWRITE_TEXT_ALIGNMENT, 4, DWRITE_TEXT_ALIGNMENT_LEADING)
/* ---- paragraph alignment: NEAR, FAR, CENTER -------------------------------------------------------------------------- */
ENUM_SUITE(ParagraphAlignment, IDWriteTextFormat, DWRITE_PARAGRAPH_ALIGNMENT, 3, DWRITE_PARAGRAPH_ALIGNMENT_NEAR)
/* ---- word wrapping: WRAP, NO_WRAP, EMERGENCY_BREAK, WHOLE_WORD, CHARACTER (the last three need Windows 10 1607+) ---------- */
ENUM_SUITE(WordWrapping, IDWriteTextFormat, DWRITE_WORD_WRAPPING, 5, DWRITE_WORD_WRAPPING_WRAP)
/* ---- reading direction: all four values are accepted, including the vertical ones ------------------------------------- */
ENUM_SUITE(ReadingDirection, IDWriteTextFormat, DWRITE_READING_DIRECTION, 4, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT)
/* ---- flow direction ------------------------------------------------------------------------------------------------- */
ENUM_SUITE(FlowDirection, IDWriteTextFormat, DWRITE_FLOW_DIRECTION, 4, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM)
/* ---- IDWriteTextFormat1: vertical glyph orientation (DEFAULT, STACKED) and optical alignment (NONE, NO_SIDE_BEARINGS) ---- */
ENUM_SUITE_F1(VerticalGlyphOrientation, DWRITE_VERTICAL_GLYPH_ORIENTATION, 2, DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT)
ENUM_SUITE_F1(OpticalAlignment, DWRITE_OPTICAL_ALIGNMENT, 2, DWRITE_OPTICAL_ALIGNMENT_NONE)

/* ---- enumeration constants themselves -------------------------------------------------------------------------------- */
TEST(enum_values_match_the_abi)
{
    CHECK(DWRITE_TEXT_ALIGNMENT_LEADING == 0 && DWRITE_TEXT_ALIGNMENT_TRAILING == 1 && DWRITE_TEXT_ALIGNMENT_CENTER == 2 && DWRITE_TEXT_ALIGNMENT_JUSTIFIED == 3);
    CHECK(DWRITE_PARAGRAPH_ALIGNMENT_NEAR == 0 && DWRITE_PARAGRAPH_ALIGNMENT_FAR == 1 && DWRITE_PARAGRAPH_ALIGNMENT_CENTER == 2);
    CHECK(DWRITE_WORD_WRAPPING_WRAP == 0 && DWRITE_WORD_WRAPPING_NO_WRAP == 1 && DWRITE_WORD_WRAPPING_EMERGENCY_BREAK == 2 && DWRITE_WORD_WRAPPING_WHOLE_WORD == 3 && DWRITE_WORD_WRAPPING_CHARACTER == 4);
    CHECK(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT == 0 && DWRITE_READING_DIRECTION_RIGHT_TO_LEFT == 1 && DWRITE_READING_DIRECTION_TOP_TO_BOTTOM == 2 && DWRITE_READING_DIRECTION_BOTTOM_TO_TOP == 3);
    CHECK(DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM == 0 && DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP == 1 && DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT == 2 && DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT == 3);
    CHECK(DWRITE_LINE_SPACING_METHOD_DEFAULT == 0 && DWRITE_LINE_SPACING_METHOD_UNIFORM == 1 && DWRITE_LINE_SPACING_METHOD_PROPORTIONAL == 2);
    CHECK(DWRITE_TRIMMING_GRANULARITY_NONE == 0 && DWRITE_TRIMMING_GRANULARITY_CHARACTER == 1 && DWRITE_TRIMMING_GRANULARITY_WORD == 2);
    CHECK(DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT == 0 && DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED == 1);
    CHECK(DWRITE_OPTICAL_ALIGNMENT_NONE == 0 && DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS == 1);
    CHECK(DWRITE_AUTOMATIC_FONT_AXES_NONE == 0 && DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE == 1);
    CHECK(sizeof(DWRITE_TRIMMING) == 12 && sizeof(DWRITE_LINE_SPACING) == 20 && sizeof(DWRITE_FONT_AXIS_VALUE) == 8);
}

/* ---- cross-property behaviour ---------------------------------------------------------------------------------------- */
TEST(reading_and_flow_direction_conflicts_are_not_checked_at_set_time)
{
    TF_NEW(t);
    /* Horizontal reading with horizontal flow (and vertical with vertical) is a layout-time conflict
     * (DWRITE_E_FLOWDIRECTIONCONFLICTS); the setters accept every pair in either order. */
    int r, fl, bad = 0;
    for (r = 0; r < 4; r++) for (fl = 0; fl < 4; fl++) {
        if (IDWriteTextFormat_SetReadingDirection(t, (DWRITE_READING_DIRECTION)r) != S_OK) bad++;
        if (IDWriteTextFormat_SetFlowDirection(t, (DWRITE_FLOW_DIRECTION)fl) != S_OK) bad++;
        if (IDWriteTextFormat_GetReadingDirection(t) != (DWRITE_READING_DIRECTION)r || IDWriteTextFormat_GetFlowDirection(t) != (DWRITE_FLOW_DIRECTION)fl) bad++;
    }
    CHECK(bad == 0);
    DW_RELEASE(t);
}

TEST(alignment_triples_all_combinations_independent)
{
    TF_NEW(t);
    int a, p, w, bad = 0;
    for (a = 0; a < 4; a++) for (p = 0; p < 3; p++) for (w = 0; w < 5; w++) {
        IDWriteTextFormat_SetTextAlignment(t, (DWRITE_TEXT_ALIGNMENT)a);
        IDWriteTextFormat_SetParagraphAlignment(t, (DWRITE_PARAGRAPH_ALIGNMENT)p);
        IDWriteTextFormat_SetWordWrapping(t, (DWRITE_WORD_WRAPPING)w);
        if (IDWriteTextFormat_GetTextAlignment(t) != (DWRITE_TEXT_ALIGNMENT)a || IDWriteTextFormat_GetParagraphAlignment(t) != (DWRITE_PARAGRAPH_ALIGNMENT)p ||
            IDWriteTextFormat_GetWordWrapping(t) != (DWRITE_WORD_WRAPPING)w) bad++;
    }
    CHECK(bad == 0);
    DW_RELEASE(t);
}

TEST(all_enum_properties_hold_distinct_values_simultaneously)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_JUSTIFIED);
    IDWriteTextFormat_SetParagraphAlignment(t, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_CHARACTER);
    IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_BOTTOM_TO_TOP);
    IDWriteTextFormat_SetFlowDirection(t, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    IDWriteTextFormat1_SetOpticalAlignment(t1, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
    IDWriteTextFormat1_SetLastLineWrapping(t1, FALSE);
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(t), DWRITE_TEXT_ALIGNMENT_JUSTIFIED);
    EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(t), DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(t), DWRITE_WORD_WRAPPING_CHARACTER);
    EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(t), DWRITE_READING_DIRECTION_BOTTOM_TO_TOP);
    EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(t), DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
    EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(t1), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED);
    EXPECT_EQ(IDWriteTextFormat1_GetOpticalAlignment(t1), DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
    EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), FALSE);
    DW_RELEASE(t1); DW_RELEASE(t);
}

TEST(setters_do_not_touch_font_description)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    WCHAR buf[32];
    IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteTextFormat_SetIncrementalTabStop(t, 7.0f);
    IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 24.0f);
    IDWriteTextFormat1_SetOpticalAlignment(t1, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
    CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(t, buf, 32)); CHECK_WEQ(buf, TF_FAM);
    CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(t, buf, 32)); CHECK_WEQ(buf, TF_LOC);
    EXPECT_EQ(IDWriteTextFormat_GetFontWeight(t), DWRITE_FONT_WEIGHT_NORMAL);
    EXPECT_EQ(IDWriteTextFormat_GetFontStyle(t), DWRITE_FONT_STYLE_NORMAL);
    EXPECT_EQ(IDWriteTextFormat_GetFontStretch(t), DWRITE_FONT_STRETCH_NORMAL);
    CHECK_FBITS(IDWriteTextFormat_GetFontSize(t), 16.0f);
    DW_RELEASE(t1); DW_RELEASE(t);
}

TEST(no_setter_exists_for_font_description)
{
    /* Weight, style, stretch, size, family and locale are fixed at creation: the interface offers getters only. Documented here
     * through layout independence: a format cannot be re-described, only copied from. */
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 11.0f, L"de-de");
    IDWriteTextFormat *b = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 11.0f, L"de-de");
    tf_state sa, sb;
    IDWriteTextFormat_SetIncrementalTabStop(a, 5.0f);
    tf_snap(a, &sa); tf_snap(b, &sb);
    CHECK(sa.w == sb.w && sa.s == sb.s && sa.st == sb.st && tf_fbits(sa.size, sb.size) && !wcscmp(sa.fam, sb.fam) && !wcscmp(sa.loc, sb.loc));
    CHECK(!tf_fbits(sa.tab, sb.tab));
    DW_RELEASE(a); DW_RELEASE(b);
}

/* ---- incremental tab stop ---------------------------------------------------------------------------------------------- */
TEST(tabstop_valid_values_roundtrip)
{
    TF_NEW(t);
    static const float v[] = { FLT_MIN, 1e-30f, 0.001f, 0.5f, 1.0f, 2.0f, 4.0f, 7.25f, 10.0f, 64.0f, 100.0f, 1234.5f, 1e6f, 1e30f, FLT_MAX, INFINITY };
    size_t i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECKF(IDWriteTextFormat_SetIncrementalTabStop(t, v[i]) == S_OK, "tab stop %g rejected", v[i]);
        CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(t), v[i]);
    }
    DW_RELEASE(t);
}

TEST(tabstop_invalid_values_rejected_and_state_kept)
{
    TF_NEW(t);
    static const float v[] = { 0.0f, -0.0f, -FLT_MIN, -1e-30f, -0.001f, -1.0f, -4.0f, -100.0f, -FLT_MAX, -INFINITY, NAN, -NAN };
    size_t i;
    CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(t, 12.5f));
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        HRESULT hr = IDWriteTextFormat_SetIncrementalTabStop(t, v[i]);
        CHECKF(hr == E_INVALIDARG, "tab stop #%u (%g) -> %s", (unsigned)i, v[i], dw_hr_name(hr));
        CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(t), 12.5f);
    }
    DW_RELEASE(t);
}

TEST(tabstop_invalid_on_fresh_format_keeps_default)
{
    TF_NEW(t);
    CHECK_HR(IDWriteTextFormat_SetIncrementalTabStop(t, 0.0f), E_INVALIDARG);
    CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(t), 64.0f);       /* 4 * 16 */
    CHECK_HR(IDWriteTextFormat_SetIncrementalTabStop(t, -64.0f), E_INVALIDARG);
    CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(t), 64.0f);
    DW_RELEASE(t);
}

TEST(tabstop_independent_of_font_size)
{
    TF_NEW(t);
    CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(t, 3.0f));
    CHECK_FBITS(IDWriteTextFormat_GetFontSize(t), 16.0f);
    CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(t), 3.0f);
    DW_RELEASE(t);
}

TEST(tabstop_formats_independent)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    CHECK_HR_OK(IDWriteTextFormat_SetIncrementalTabStop(a, 9.0f));
    CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(a), 9.0f);
    CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(b), 64.0f);
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(tabstop_through_every_interface_pointer)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    CHECK_HR_OK(IDWriteTextFormat1_SetIncrementalTabStop(t1, 21.0f)); CHECK_FBITS(IDWriteTextFormat2_GetIncrementalTabStop(t2), 21.0f);
    CHECK_HR_OK(IDWriteTextFormat2_SetIncrementalTabStop(t2, 22.0f)); CHECK_FBITS(IDWriteTextFormat3_GetIncrementalTabStop(t3), 22.0f);
    CHECK_HR_OK(IDWriteTextFormat3_SetIncrementalTabStop(t3, 23.0f)); CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(t), 23.0f);
    CHECK_HR(IDWriteTextFormat3_SetIncrementalTabStop(t3, 0.0f), E_INVALIDARG);
    CHECK_FBITS(IDWriteTextFormat1_GetIncrementalTabStop(t1), 23.0f);
    DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(tabstop_denormal_and_tiny_positive)
{
    TF_NEW(t);
    /* Any strictly positive float passes, even below FLT_MIN. */
    float tiny = 1e-44f;
    HRESULT hr = IDWriteTextFormat_SetIncrementalTabStop(t, tiny);
    CHECKF(hr == S_OK, "denormal tab stop -> %s", dw_hr_name(hr));
    if (hr == S_OK) t_note("denormal tab stop reads back as %g", IDWriteTextFormat_GetIncrementalTabStop(t));
    DW_RELEASE(t);
}

/* ---- last line wrapping (IDWriteTextFormat1) --------------------------------------------------------------------------- */
TEST(lastlinewrapping_normalises_to_boolean)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    static const int v[] = { 0, 1, 2, -1, 0x100, 0x7fffffff, INT_MIN, 0x01000000 };
    size_t i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECKF(IDWriteTextFormat1_SetLastLineWrapping(t1, v[i]) == S_OK, "value %d rejected", v[i]);
        EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), v[i] != 0 ? 1 : 0);      /* any non-zero value reads back as exactly TRUE */
    }
    DW_RELEASE(t1); DW_RELEASE(t);
}

TEST(lastlinewrapping_toggle_and_independence)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    TF_QI(a1, IDWriteTextFormat1, a); TF_QI(b1, IDWriteTextFormat1, b);
    CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(a1, FALSE));
    EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(a1), FALSE);
    EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(b1), TRUE);
    CHECK_HR_OK(IDWriteTextFormat1_SetLastLineWrapping(a1, TRUE));
    EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(a1), TRUE);
    DW_RELEASE(a1); DW_RELEASE(b1); DW_RELEASE(a); DW_RELEASE(b);
}

TEST(lastlinewrapping_through_format3)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t); TF_QI(t1, IDWriteTextFormat1, t);
    CHECK_HR_OK(IDWriteTextFormat3_SetLastLineWrapping(t3, FALSE));
    EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(t1), FALSE);
    EXPECT_EQ(IDWriteTextFormat3_GetLastLineWrapping(t3), FALSE);
    DW_RELEASE(t1); DW_RELEASE(t3); DW_RELEASE(t);
}

/* ---- automatic font axes (IDWriteTextFormat3) -------------------------------------------------------------------------- */
TEST(automatic_axes_roundtrip_known_values)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t3, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t3, DWRITE_AUTOMATIC_FONT_AXES_NONE));
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), DWRITE_AUTOMATIC_FONT_AXES_NONE);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(automatic_axes_value_is_not_validated)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    /* The setter stores whatever it is given (no range check) and always succeeds. */
    static const UINT32 v[] = { 2, 3, 0xff, 0x100, 0x80000000u, 0xffffffffu };
    size_t i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        HRESULT hr = IDWriteTextFormat3_SetAutomaticFontAxes(t3, (DWRITE_AUTOMATIC_FONT_AXES)v[i]);
        CHECKF(hr == S_OK, "value 0x%x -> %s", v[i], dw_hr_name(hr));
        EXPECT_EQ((UINT32)IDWriteTextFormat3_GetAutomaticFontAxes(t3), v[i]);
    }
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(automatic_axes_formats_independent)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    TF_QI(a3, IDWriteTextFormat3, a); TF_QI(b3, IDWriteTextFormat3, b);
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(a3, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(b3), DWRITE_AUTOMATIC_FONT_AXES_NONE);
    DW_RELEASE(a3); DW_RELEASE(b3); DW_RELEASE(a); DW_RELEASE(b);
}

/* ---- setters via a different interface pointer than the one that created the format ---------------------------------- */
TEST(state_is_shared_between_interface_pointers)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_TRAILING);
    EXPECT_EQ(IDWriteTextFormat1_GetTextAlignment(t1), DWRITE_TEXT_ALIGNMENT_TRAILING);
    EXPECT_EQ(IDWriteTextFormat2_GetTextAlignment(t2), DWRITE_TEXT_ALIGNMENT_TRAILING);
    EXPECT_EQ(IDWriteTextFormat3_GetTextAlignment(t3), DWRITE_TEXT_ALIGNMENT_TRAILING);
    IDWriteTextFormat3_SetParagraphAlignment(t3, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(t), DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    IDWriteTextFormat1_SetWordWrapping(t1, DWRITE_WORD_WRAPPING_WHOLE_WORD);
    EXPECT_EQ(IDWriteTextFormat2_GetWordWrapping(t2), DWRITE_WORD_WRAPPING_WHOLE_WORD);
    DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(state_survives_releasing_the_creating_pointer)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat_SetIncrementalTabStop(t, 6.0f);
    IDWriteTextFormat_Release(t);       /* the object lives on through t3 */
    EXPECT_EQ(IDWriteTextFormat3_GetTextAlignment(t3), DWRITE_TEXT_ALIGNMENT_CENTER);
    CHECK_FBITS(IDWriteTextFormat3_GetIncrementalTabStop(t3), 6.0f);
    EXPECT_REFS(t3, 1);
    DW_RELEASE(t3);
}
