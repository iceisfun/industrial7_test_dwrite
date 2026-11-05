/* Layout-wide property setters that steer bidi / flow behaviour: argument validation and round trips.
 *
 * Facts (IDWriteTextLayout:: setters in dwrite.dll):
 *  - SetReadingDirection accepts 0..3 (LTR, RTL, TTB, BTT), SetFlowDirection 0..3, SetTextAlignment 0..3, SetOpticalAlignment 0..1,
 *    SetVerticalGlyphOrientation 0..1; anything else returns E_INVALIDARG and stores nothing;
 *  - SetIncrementalTabStop rejects values <= 0 and NaN (also -0.0), accepts everything else;
 *  - SetLastLineWrapping never fails and stores (value != 0), GetLastLineWrapping returns 0 or 1;
 *  - SetCharacterSpacing rejects a range whose start+length wraps, NaN leading/trailing, NaN or negative minimum advance;
 *  - SetPairKerning rejects a wrapping range and accepts a zero-length range as a no-op.
 */
#include "bx.h"

#define LTR DWRITE_READING_DIRECTION_LEFT_TO_RIGHT

#define ENUM_CASE(name, setter, getter, type, val, ok, dflt) \
TEST(r2_layout_bidi_reorder_props_##name) \
{ \
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1 ab", BX_W, 50.0f); \
    HRESULT hr = IDWriteTextLayout_##setter(l, (type)(val)); \
    CHECK_HR(hr, (ok) ? S_OK : E_INVALIDARG); \
    CHECK(IDWriteTextLayout_##getter(l) == (type)((ok) ? (val) : (dflt))); \
    BX_DONE(l); \
}

ENUM_CASE(reading_ltr, SetReadingDirection, GetReadingDirection, DWRITE_READING_DIRECTION, 0, 1, 0)
ENUM_CASE(reading_rtl, SetReadingDirection, GetReadingDirection, DWRITE_READING_DIRECTION, 1, 1, 0)
ENUM_CASE(reading_ttb, SetReadingDirection, GetReadingDirection, DWRITE_READING_DIRECTION, 2, 1, 0)
ENUM_CASE(reading_btt, SetReadingDirection, GetReadingDirection, DWRITE_READING_DIRECTION, 3, 1, 0)
ENUM_CASE(reading_4,   SetReadingDirection, GetReadingDirection, DWRITE_READING_DIRECTION, 4, 0, 0)
ENUM_CASE(reading_255, SetReadingDirection, GetReadingDirection, DWRITE_READING_DIRECTION, 255, 0, 0)
ENUM_CASE(reading_neg, SetReadingDirection, GetReadingDirection, DWRITE_READING_DIRECTION, -1, 0, 0)
ENUM_CASE(reading_max, SetReadingDirection, GetReadingDirection, DWRITE_READING_DIRECTION, 0x7fffffff, 0, 0)
ENUM_CASE(flow_ttb, SetFlowDirection, GetFlowDirection, DWRITE_FLOW_DIRECTION, 0, 1, 0)
ENUM_CASE(flow_btt, SetFlowDirection, GetFlowDirection, DWRITE_FLOW_DIRECTION, 1, 1, 0)
ENUM_CASE(flow_ltr, SetFlowDirection, GetFlowDirection, DWRITE_FLOW_DIRECTION, 2, 1, 0)
ENUM_CASE(flow_rtl, SetFlowDirection, GetFlowDirection, DWRITE_FLOW_DIRECTION, 3, 1, 0)
ENUM_CASE(flow_4,   SetFlowDirection, GetFlowDirection, DWRITE_FLOW_DIRECTION, 4, 0, 0)
ENUM_CASE(flow_neg, SetFlowDirection, GetFlowDirection, DWRITE_FLOW_DIRECTION, -1, 0, 0)
ENUM_CASE(flow_max, SetFlowDirection, GetFlowDirection, DWRITE_FLOW_DIRECTION, 0x7fffffff, 0, 0)
ENUM_CASE(talign_leading,  SetTextAlignment, GetTextAlignment, DWRITE_TEXT_ALIGNMENT, 0, 1, 0)
ENUM_CASE(talign_trailing, SetTextAlignment, GetTextAlignment, DWRITE_TEXT_ALIGNMENT, 1, 1, 0)
ENUM_CASE(talign_center,   SetTextAlignment, GetTextAlignment, DWRITE_TEXT_ALIGNMENT, 2, 1, 0)
ENUM_CASE(talign_justified,SetTextAlignment, GetTextAlignment, DWRITE_TEXT_ALIGNMENT, 3, 1, 0)
ENUM_CASE(talign_4,        SetTextAlignment, GetTextAlignment, DWRITE_TEXT_ALIGNMENT, 4, 0, 0)
ENUM_CASE(talign_neg,      SetTextAlignment, GetTextAlignment, DWRITE_TEXT_ALIGNMENT, -1, 0, 0)

/* the state is untouched by a rejected call even after a valid one */
TEST(r2_layout_bidi_reorder_props_rejected_keeps_previous)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1", BX_W, 50.0f);
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)9), DW_E_INVALIDARG);
    CHECK(IDWriteTextLayout_GetReadingDirection(l) == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)9), DW_E_INVALIDARG);
    CHECK(IDWriteTextLayout_GetFlowDirection(l) == DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    CHECK_HR(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)9), DW_E_INVALIDARG);
    CHECK(IDWriteTextLayout_GetTextAlignment(l) == DWRITE_TEXT_ALIGNMENT_CENTER);
    BX_DONE(l);
}

/* Reading and flow direction are independent settings. */
TEST(r2_layout_bidi_reorder_props_reading_flow_independent)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1", BX_W, 50.0f);
    int r, fl;
    for (r = 0; r < 4; r++) for (fl = 0; fl < 4; fl++) {
        CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, (DWRITE_READING_DIRECTION)r));
        CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, (DWRITE_FLOW_DIRECTION)fl));
        CHECK(IDWriteTextLayout_GetReadingDirection(l) == (DWRITE_READING_DIRECTION)r);
        CHECK(IDWriteTextLayout_GetFlowDirection(l) == (DWRITE_FLOW_DIRECTION)fl);
    }
    BX_DONE(l);
}

/* Defaults of a layout built from a format that did not set them. */
TEST(r2_layout_bidi_reorder_props_defaults)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1 ab", BX_W, 50.0f);
    CHECK(IDWriteTextLayout_GetReadingDirection(l) == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    CHECK(IDWriteTextLayout_GetFlowDirection(l) == DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
    CHECK(IDWriteTextLayout_GetTextAlignment(l) == DWRITE_TEXT_ALIGNMENT_LEADING);
    BX_DONE(l);
}

/* A format's reading direction is inherited by the layout. */
TEST(r2_layout_bidi_reorder_props_format_rtl_inherited)
{
    BX_BEGIN(); IDWriteTextFormat *t = bx_format(&X, BX_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t) { BX_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK_HR_OK(IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_TRAILING));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(X.f, L"ab", 2, t, BX_W, 50.0f, &l));
    if (l) {
        CHECK(IDWriteTextLayout_GetReadingDirection(l) == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        CHECK(IDWriteTextLayout_GetTextAlignment(l) == DWRITE_TEXT_ALIGNMENT_TRAILING);
        /* later changes on the format do not reach an existing layout */
        CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT));
        CHECK(IDWriteTextLayout_GetReadingDirection(l) == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        IDWriteTextLayout_Release(l);
    }
    IDWriteTextFormat_Release(t); BX_END();
}

/* ---- SetIncrementalTabStop ------------------------------------------------------------------------------------ */
#define TAB_CASE(name, val, ok) \
TEST(r2_layout_bidi_reorder_props_tabstop_##name) \
{ \
    BX_BEGIN(); BX_LAYOUT(l, L"a\tb", BX_W, 50.0f); \
    float v = (val), before = IDWriteTextLayout_GetIncrementalTabStop(l); \
    CHECK_HR(IDWriteTextLayout_SetIncrementalTabStop(l, v), (ok) ? S_OK : E_INVALIDARG); \
    if (ok) { CHECK(IDWriteTextLayout_GetIncrementalTabStop(l) == v); } \
    else { CHECK(IDWriteTextLayout_GetIncrementalTabStop(l) == before); } \
    BX_DONE(l); \
}
TAB_CASE(one,        1.0f, 1)
TAB_CASE(half,       0.5f, 1)
TAB_CASE(fifty,      50.0f, 1)
TAB_CASE(tiny,       FLT_MIN, 1)
TAB_CASE(large,      1e9f, 1)
TAB_CASE(zero,       0.0f, 0)
TAB_CASE(negzero,    -0.0f, 0)
TAB_CASE(negative,   -1.0f, 0)
TAB_CASE(neg_tiny,   -FLT_MIN, 0)
TAB_CASE(nan,        NAN, 0)
TAB_CASE(neg_inf,    -INFINITY, 0)

TEST(r2_layout_bidi_reorder_props_tabstop_default_positive)
{
    BX_BEGIN(); BX_LAYOUT(l, L"a", BX_W, 50.0f);
    float v = IDWriteTextLayout_GetIncrementalTabStop(l);
    CHECKF(v > 0.0f, "default tab stop %g", v);
    t_note("default incremental tab stop at size 16: %g", v);
    BX_DONE(l);
}

/* ---- IDWriteTextLayout2::Set/GetLastLineWrapping, Set/GetOpticalAlignment, Set/GetVerticalGlyphOrientation ----- */
#define LLW_CASE(name, val, want) \
TEST(r2_layout_bidi_reorder_props_lastline_##name) \
{ \
    BX_BEGIN(); BX_LAYOUT(l, L"ab cd", BX_W, 50.0f); BX_QI(l2, IDWriteTextLayout2, l); \
    CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, (BOOL)(val))); \
    BOOL got = IDWriteTextLayout2_GetLastLineWrapping(l2); \
    CHECKF(got == (want), "GetLastLineWrapping %d want %d", got, want); \
    IDWriteTextLayout2_Release(l2); BX_DONE(l); \
}
LLW_CASE(false, 0, 0)
LLW_CASE(true, 1, 1)
LLW_CASE(two, 2, 1)
LLW_CASE(minus1, -1, 1)
LLW_CASE(high_bit, 0x80000000, 1)
LLW_CASE(byte_aligned, 0x100, 1)

#define OPT_CASE(name, val, ok) \
TEST(r2_layout_bidi_reorder_props_optical_##name) \
{ \
    BX_BEGIN(); BX_LAYOUT(l, L"ab cd", BX_W, 50.0f); BX_QI(l2, IDWriteTextLayout2, l); \
    CHECK_HR(IDWriteTextLayout2_SetOpticalAlignment(l2, (DWRITE_OPTICAL_ALIGNMENT)(val)), (ok) ? S_OK : E_INVALIDARG); \
    CHECK(IDWriteTextLayout2_GetOpticalAlignment(l2) == (DWRITE_OPTICAL_ALIGNMENT)((ok) ? (val) : 0)); \
    IDWriteTextLayout2_Release(l2); BX_DONE(l); \
}
OPT_CASE(none, 0, 1)
OPT_CASE(side_bearings, 1, 1)
OPT_CASE(two, 2, 0)
OPT_CASE(neg, -1, 0)
OPT_CASE(max, 0x7fffffff, 0)

#define VGO_CASE(name, val, ok) \
TEST(r2_layout_bidi_reorder_props_vglyph_##name) \
{ \
    BX_BEGIN(); BX_LAYOUT(l, L"ab cd", BX_W, 50.0f); BX_QI(l2, IDWriteTextLayout2, l); \
    CHECK_HR(IDWriteTextLayout2_SetVerticalGlyphOrientation(l2, (DWRITE_VERTICAL_GLYPH_ORIENTATION)(val)), (ok) ? S_OK : E_INVALIDARG); \
    CHECK(IDWriteTextLayout2_GetVerticalGlyphOrientation(l2) == (DWRITE_VERTICAL_GLYPH_ORIENTATION)((ok) ? (val) : 0)); \
    IDWriteTextLayout2_Release(l2); BX_DONE(l); \
}
VGO_CASE(default, 0, 1)
VGO_CASE(stacked, 1, 1)
VGO_CASE(two, 2, 0)
VGO_CASE(neg, -1, 0)

TEST(r2_layout_bidi_reorder_props_layout2_same_object_state)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab cd", BX_W, 50.0f); BX_QI(l2, IDWriteTextLayout2, l);
    CHECK_HR_OK(IDWriteTextLayout2_SetReadingDirection(l2, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    CHECK(IDWriteTextLayout_GetReadingDirection(l) == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT));
    CHECK(IDWriteTextLayout2_GetFlowDirection(l2) == DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout2_SetLastLineWrapping(l2, FALSE));
    {   IDWriteTextLayout3 *l3 = NULL;
        if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout3, (void **)&l3)) && l3) {
            CHECK(IDWriteTextLayout3_GetLastLineWrapping(l3) == FALSE);
            IDWriteTextLayout3_Release(l3);
        }
    }
    IDWriteTextLayout2_Release(l2); BX_DONE(l);
}

/* ---- SetCharacterSpacing / GetCharacterSpacing ---------------------------------------------------------------- */
typedef struct { const char *nm; float lead, trail, min; DWRITE_TEXT_RANGE r; int ok; } cs_row;
#define CS_CASE(nm, lead, trail, mn, rs, rl, ok) \
TEST(r2_layout_bidi_reorder_props_spacing_args_##nm) \
{ \
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 abc", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l); \
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, (lead), (trail), (mn), RNG(rs, rl)), (ok) ? S_OK : E_INVALIDARG); \
    IDWriteTextLayout1_Release(l1); BX_DONE(l); \
}
CS_CASE(zeros,        0.0f, 0.0f, 0.0f, 0, 3, 1)
CS_CASE(positive,     1.0f, 2.0f, 3.0f, 0, 3, 1)
CS_CASE(negative_lead,-1.0f, 2.0f, 0.0f, 0, 3, 1)
CS_CASE(negative_trail,1.0f,-2.0f, 0.0f, 0, 3, 1)
CS_CASE(neg_both,     -3.0f,-3.0f, 0.0f, 0, 3, 1)
CS_CASE(negzero_min,  0.0f, 0.0f, -0.0f, 0, 3, 1)
CS_CASE(nan_lead,     NAN,  0.0f, 0.0f, 0, 3, 0)
CS_CASE(nan_trail,    0.0f, NAN,  0.0f, 0, 3, 0)
CS_CASE(nan_min,      0.0f, 0.0f, NAN,  0, 3, 0)
CS_CASE(neg_min,      0.0f, 0.0f, -1.0f, 0, 3, 0)
CS_CASE(neg_min_tiny, 0.0f, 0.0f, -FLT_MIN, 0, 3, 0)
CS_CASE(inf_lead,     INFINITY, 0.0f, 0.0f, 0, 3, 1)
CS_CASE(range_wrap,   1.0f, 1.0f, 0.0f, 0xfffffffeu, 4, 0)
CS_CASE(range_wrap_max, 1.0f, 1.0f, 0.0f, 1, 0xffffffffu, 0)
CS_CASE(range_max_zero, 1.0f, 1.0f, 0.0f, 0xffffffffu, 0, 1)
CS_CASE(range_zero_len, 1.0f, 1.0f, 0.0f, 2, 0, 1)
CS_CASE(range_beyond_text, 1.0f, 1.0f, 0.0f, 100, 5, 1)

TEST(r2_layout_bidi_reorder_props_spacing_default_zero)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 abc", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    float a = 9, b = 9, c = 9; DWRITE_TEXT_RANGE r = { 99, 99 };
    CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 0, &a, &b, &c, &r));
    CHECK(a == 0.0f && b == 0.0f && c == 0.0f);
    CHECK_RANGE_TAIL(r, 0, 7);                                     /* one run covers the whole text */
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_props_spacing_roundtrip_ranges)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 abcd", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    float a, b, c; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 1.5f, 2.5f, 4.0f, RNG(2, 4)));        /* text positions 2..5 */
    a = b = c = 9; CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 0, &a, &b, &c, &r));
    CHECK(a == 0 && b == 0 && c == 0); CHECK_RANGE(r, 0, 2);
    a = b = c = 9; CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 3, &a, &b, &c, &r));
    CHECK(a == 1.5f && b == 2.5f && c == 4.0f); CHECK_RANGE(r, 2, 4);
    a = b = c = 9; CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 6, &a, &b, &c, &r));
    CHECK(a == 0 && b == 0 && c == 0); CHECK_RANGE_TAIL(r, 6, 8);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_props_spacing_rejected_keeps_state)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    float a, b, c; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 1.0f, 2.0f, 3.0f, RNG(0, 3)));
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 5.0f, 5.0f, -1.0f, RNG(0, 3)), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, NAN, 5.0f, 1.0f, RNG(0, 3)), DW_E_INVALIDARG);
    a = b = c = 9; CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 1, &a, &b, &c, &r));
    CHECK(a == 1.0f && b == 2.0f && c == 3.0f); CHECK_RANGE(r, 0, 3);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_props_spacing_set_twice_last_wins)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    float a, b, c; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 1.0f, 2.0f, 3.0f, RNG(0, 3)));
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 4.0f, 5.0f, 6.0f, RNG(1, 1)));
    a = b = c = 9; CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 0, &a, &b, &c, &r));
    CHECK(a == 1.0f && b == 2.0f && c == 3.0f); CHECK_RANGE(r, 0, 1);
    a = b = c = 9; CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 1, &a, &b, &c, &r));
    CHECK(a == 4.0f && b == 5.0f && c == 6.0f); CHECK_RANGE(r, 1, 1);
    a = b = c = 9; CHECK_HR_OK(IDWriteTextLayout1_GetCharacterSpacing(l1, 2, &a, &b, &c, &r));
    CHECK(a == 1.0f && b == 2.0f && c == 3.0f); CHECK_RANGE(r, 2, 1);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* ---- SetPairKerning / GetPairKerning ---------------------------------------------------------------------------- */
TEST(r2_layout_bidi_reorder_props_pairkern_default_false)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 abc", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    BOOL k = 7; DWRITE_TEXT_RANGE r = { 99, 99 };
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 0, &k, &r));
    CHECK(k == FALSE); CHECK_RANGE_TAIL(r, 0, 7);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_props_pairkern_roundtrip)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 abcd", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    BOOL k; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(1, 5)));
    k = 7; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 0, &k, &r)); CHECK(k == FALSE); CHECK_RANGE(r, 0, 1);
    k = 7; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 3, &k, &r)); CHECK(k == TRUE); CHECK_RANGE(r, 1, 5);
    k = 7; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 6, &k, &r)); CHECK(k == FALSE); CHECK_RANGE_TAIL(r, 6, 8);
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, FALSE, RNG(2, 1)));
    k = 7; CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 2, &k, &r)); CHECK(k == FALSE); CHECK_RANGE(r, 2, 1);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_props_pairkern_nonzero_is_true)
{
    BX_BEGIN(); BX_LAYOUT(l, L"abc", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    BOOL k = 7; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, 5, RNG(0, 3)));
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 0, &k, &r));
    CHECKF(k == 1, "stored as %d, want a normalised TRUE", k);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_props_pairkern_range_wrap_invalid)
{
    BX_BEGIN(); BX_LAYOUT(l, L"abc", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    BOOL k = 7; DWRITE_TEXT_RANGE r;
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0xfffffffeu, 4)), DW_E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(1, 0xffffffffu)), DW_E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 0, &k, &r)); CHECK(k == FALSE);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_props_pairkern_zero_length_noop)
{
    BX_BEGIN(); BX_LAYOUT(l, L"abc", BX_W, 50.0f); BX_QI(l1, IDWriteTextLayout1, l);
    BOOL k = 7; DWRITE_TEXT_RANGE r;
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(1, 0)));
    CHECK_HR_OK(IDWriteTextLayout1_SetPairKerning(l1, TRUE, RNG(0xffffffffu, 0)));
    CHECK_HR_OK(IDWriteTextLayout1_GetPairKerning(l1, 1, &k, &r)); CHECK(k == FALSE); CHECK_RANGE_TAIL(r, 0, 3);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* A layout-level setter invalidates the cached layout: metrics follow the new reading direction immediately. */
TEST(r2_layout_bidi_reorder_props_setter_relayouts)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2", 100.0f, 50.0f);
    DWRITE_TEXT_METRICS a, b;
    bx_metrics(l, &a);
    CHK_NEAR(a.left, 0.0f); CHK_NEAR(a.width, 3 * CW);               /* leading edge is the left */
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    bx_metrics(l, &b);
    CHK_NEAR(b.left, 100.0f - 3 * CW); CHK_NEAR(b.width, 3 * CW);     /* leading edge is now the right */
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT));
    bx_metrics(l, &b);
    CHK_NEAR(b.left, 0.0f);
    BX_DONE(l);
}
