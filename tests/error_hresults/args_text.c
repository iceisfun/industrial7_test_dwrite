/* TextFormat / TextLayout argument validation: enum ranges, float predicates, text-range overflow, NULL pointers and the
 * buffer-size protocol of the string / metric getters. All ranges come from the compare-and-reject prologues of the
 * DWriteTextFormat / DWriteTextLayout methods; a rejected call never changes the stored value. */
#include "eh.h"

/* ---- enum setters, table driven; the same table runs against a TextFormat and a TextLayout --------------------------------------- */
typedef struct {
    const char *name;
    HRESULT (*set)(IUnknown *o, int v);
    int (*get)(IUnknown *o);
    const int *good; int ngood; const int *bad; int nbad;
} enum_row;
#define TF(o) ((IDWriteTextFormat *)(o))
static IDWriteTextFormat1 *as_tf1(IUnknown *o) { IDWriteTextFormat1 *t = NULL; IUnknown_QueryInterface(o, &IID_IDWriteTextFormat1, (void **)&t); return t; }
static HRESULT s_ta(IUnknown *o, int v) { return IDWriteTextFormat_SetTextAlignment(TF(o), (DWRITE_TEXT_ALIGNMENT)v); }
static int g_ta(IUnknown *o) { return IDWriteTextFormat_GetTextAlignment(TF(o)); }
static HRESULT s_pa(IUnknown *o, int v) { return IDWriteTextFormat_SetParagraphAlignment(TF(o), (DWRITE_PARAGRAPH_ALIGNMENT)v); }
static int g_pa(IUnknown *o) { return IDWriteTextFormat_GetParagraphAlignment(TF(o)); }
static HRESULT s_ww(IUnknown *o, int v) { return IDWriteTextFormat_SetWordWrapping(TF(o), (DWRITE_WORD_WRAPPING)v); }
static int g_ww(IUnknown *o) { return IDWriteTextFormat_GetWordWrapping(TF(o)); }
static HRESULT s_rd(IUnknown *o, int v) { return IDWriteTextFormat_SetReadingDirection(TF(o), (DWRITE_READING_DIRECTION)v); }
static int g_rd(IUnknown *o) { return IDWriteTextFormat_GetReadingDirection(TF(o)); }
static HRESULT s_fd(IUnknown *o, int v) { return IDWriteTextFormat_SetFlowDirection(TF(o), (DWRITE_FLOW_DIRECTION)v); }
static int g_fd(IUnknown *o) { return IDWriteTextFormat_GetFlowDirection(TF(o)); }
static HRESULT s_vgo(IUnknown *o, int v) { IDWriteTextFormat1 *t = as_tf1(o); HRESULT hr = t ? IDWriteTextFormat1_SetVerticalGlyphOrientation(t, (DWRITE_VERTICAL_GLYPH_ORIENTATION)v) : E_NOINTERFACE; DW_RELEASE(t); return hr; }
static int g_vgo(IUnknown *o) { IDWriteTextFormat1 *t = as_tf1(o); int r = t ? IDWriteTextFormat1_GetVerticalGlyphOrientation(t) : -1; DW_RELEASE(t); return r; }
static HRESULT s_oa(IUnknown *o, int v) { IDWriteTextFormat1 *t = as_tf1(o); HRESULT hr = t ? IDWriteTextFormat1_SetOpticalAlignment(t, (DWRITE_OPTICAL_ALIGNMENT)v) : E_NOINTERFACE; DW_RELEASE(t); return hr; }
static int g_oa(IUnknown *o) { IDWriteTextFormat1 *t = as_tf1(o); int r = t ? IDWriteTextFormat1_GetOpticalAlignment(t) : -1; DW_RELEASE(t); return r; }

static const int ok_ta[] = { 0, 1, 2, 3, 2, 0 }, bad_ta[] = { 4, 5, 100, -1, 0x7fffffff, (int)0x80000000 };      /* 3 = JUSTIFIED is accepted */
static const int ok_pa[] = { 0, 1, 2, 0 }, bad_pa[] = { 3, 4, 100, -1, (int)0x80000000 };
static const int ok_ww[] = { 0, 1, 2, 3, 4, 0 }, bad_ww[] = { 5, 6, 100, -1, 0x7fffffff };
static const int ok_rd[] = { 0, 1, 2, 3, 0 }, bad_rd[] = { 4, 5, 100, -1, 0x7fffffff };
static const int ok_fd[] = { 0, 1, 2, 3, 0 }, bad_fd[] = { 4, 5, 100, -1, 0x7fffffff };
static const int ok_vgo[] = { 0, 1, 0 }, bad_vgo[] = { 2, 3, 100, -1 };
static const int ok_oa[] = { 0, 1, 0 }, bad_oa[] = { 2, 3, 100, -1 };
#define ROW(n, s, g, ok, bad) { n, s, g, ok, (int)(sizeof ok / sizeof ok[0]), bad, (int)(sizeof bad / sizeof bad[0]) }
static const enum_row enum_rows[] = {
    ROW("TextAlignment", s_ta, g_ta, ok_ta, bad_ta), ROW("ParagraphAlignment", s_pa, g_pa, ok_pa, bad_pa), ROW("WordWrapping", s_ww, g_ww, ok_ww, bad_ww),
    ROW("ReadingDirection", s_rd, g_rd, ok_rd, bad_rd), ROW("FlowDirection", s_fd, g_fd, ok_fd, bad_fd),
    ROW("VerticalGlyphOrientation", s_vgo, g_vgo, ok_vgo, bad_vgo), ROW("OpticalAlignment", s_oa, g_oa, ok_oa, bad_oa),
};
#define NENUM ((int)(sizeof enum_rows / sizeof enum_rows[0]))
static void run_enum_rows(IUnknown *obj, const char *kind) {
    int r, i;
    for (r = 0; r < NENUM; r++) {
        const enum_row *e = &enum_rows[r];
        for (i = 0; i < e->ngood; i++) {
            HRESULT hr = e->set(obj, e->good[i]);
            CHECKF(hr == S_OK, "%s %s(%d) -> %s", kind, e->name, e->good[i], dw_hr_name(hr));
            if (hr == S_OK) CHECKF(e->get(obj) == e->good[i], "%s %s round trip %d got %d", kind, e->name, e->good[i], e->get(obj));
        }
        e->set(obj, e->good[1]);
        for (i = 0; i < e->nbad; i++) {
            int before = e->get(obj);
            HRESULT hr = e->set(obj, e->bad[i]);
            CHECKF(hr == E_INVALIDARG, "%s %s(%d) -> %s", kind, e->name, e->bad[i], dw_hr_name(hr));
            CHECKF(e->get(obj) == before, "%s %s kept value after rejection: %d -> %d", kind, e->name, before, e->get(obj));
        }
    }
}
TEST(textformat_enum_setters) {
    NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f);
    if (t) { run_enum_rows((IUnknown *)t, "format"); DW_RELEASE(t); }
    DONE_FACTORY();
}
TEST(textlayout_enum_setters) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello world", 200.f, 50.f, NULL);
    if (l) { run_enum_rows((IUnknown *)l, "layout"); DW_RELEASE(l); }
    DONE_FACTORY();
}
TEST(textformat_enum_setters_per_value_nonmutating_on_layout_copy) {
    /* A layout copies the format at creation: invalid sets on the layout leave the source format alone and vice versa. */
    NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f); IDWriteTextLayout *l = NULL;
    if (t && SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 100, 100, &l)) && l) {
        CHECK_HR(IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_TRAILING), S_OK);
        CHECK_HR(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)9), E_INVALIDARG);
        EXPECT_EQ(IDWriteTextFormat_GetTextAlignment((IDWriteTextFormat *)l), DWRITE_TEXT_ALIGNMENT_LEADING);
        EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(t), DWRITE_TEXT_ALIGNMENT_TRAILING);
    }
    DW_RELEASE(l); DW_RELEASE(t); DONE_FACTORY();
}

/* ---- float setters ------------------------------------------------------------------------------------------------------------- */
typedef struct { const char *name; HRESULT (*set)(IUnknown *o, float v); float (*get)(IUnknown *o); const float *good; int ngood; const float *bad; int nbad; } float_row;
static HRESULT s_tab(IUnknown *o, float v) { return IDWriteTextFormat_SetIncrementalTabStop(TF(o), v); }
static float g_tab(IUnknown *o) { return IDWriteTextFormat_GetIncrementalTabStop(TF(o)); }
static HRESULT s_mw(IUnknown *o, float v) { return IDWriteTextLayout_SetMaxWidth((IDWriteTextLayout *)o, v); }
static float g_mw(IUnknown *o) { return IDWriteTextLayout_GetMaxWidth((IDWriteTextLayout *)o); }
static HRESULT s_mh(IUnknown *o, float v) { return IDWriteTextLayout_SetMaxHeight((IDWriteTextLayout *)o, v); }
static float g_mh(IUnknown *o) { return IDWriteTextLayout_GetMaxHeight((IDWriteTextLayout *)o); }
TEST(tabstop_validation) {
    NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f); IDWriteTextLayout *l = eh_layout(f, L"a\tb", 100, 100, NULL);
    static const float good[] = { 0.001f, 1.f, 36.f, 1e6f }, bad[] = { 0.f, -0.f, -1.f, -1e-30f, NAN_F, -INF_F }; int k, i;
    IUnknown *objs[2] = { (IUnknown *)t, (IUnknown *)l };
    for (k = 0; k < 2; k++) if (objs[k]) {
        for (i = 0; i < 4; i++) { CHECKF(s_tab(objs[k], good[i]) == S_OK, "obj %d tab %g", k, good[i]); CHECK_FLOAT_EQ(g_tab(objs[k]), good[i]); }
        for (i = 0; i < 6; i++) { float before = g_tab(objs[k]); CHECKF(s_tab(objs[k], bad[i]) == E_INVALIDARG, "obj %d tab %g", k, bad[i]); CHECK_FLOAT_EQ(g_tab(objs[k]), before); }
    }
    DW_RELEASE(t); DW_RELEASE(l); DONE_FACTORY();
}
TEST(layout_max_size_validation) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); int i;
    static const float good[] = { 0.f, 0.5f, 1e9f }, bad[] = { -1.f, -1e-30f, NAN_F, -INF_F };
    if (l) {
        for (i = 0; i < 3; i++) {
            CHECKF(s_mw((IUnknown *)l, good[i]) == S_OK, "width %g", good[i]); CHECK_FLOAT_EQ(g_mw((IUnknown *)l), good[i]);
            CHECKF(s_mh((IUnknown *)l, good[i]) == S_OK, "height %g", good[i]); CHECK_FLOAT_EQ(g_mh((IUnknown *)l), good[i]);
        }
        CHECK_HR(IDWriteTextLayout_SetMaxWidth(l, INF_F), S_OK); CHECK_HR(IDWriteTextLayout_SetMaxHeight(l, INF_F), S_OK);
        s_mw((IUnknown *)l, 10.f); s_mh((IUnknown *)l, 20.f);
        for (i = 0; i < 4; i++) {
            CHECKF(s_mw((IUnknown *)l, bad[i]) == E_INVALIDARG, "width %g", bad[i]); CHECK_FLOAT_EQ(g_mw((IUnknown *)l), 10.f);
            CHECKF(s_mh((IUnknown *)l, bad[i]) == E_INVALIDARG, "height %g", bad[i]); CHECK_FLOAT_EQ(g_mh((IUnknown *)l), 20.f);
        }
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}

/* ---- line spacing ---------------------------------------------------------------------------------------------------------------- */
typedef struct { DWRITE_LINE_SPACING_METHOD m; float sp, bl; HRESULT want; const char *why; } ls_row;
static const ls_row ls_rows[] = {
    { 0, 0.f, 0.f, S_OK, "default, zero" }, { 1, 20.f, 16.f, S_OK, "uniform" }, { 2, 1.5f, 1.f, S_OK, "proportional" }, { 1, 0.f, -5.f, S_OK, "negative baseline" },
    { 1, 1e9f, 1e9f, S_OK, "huge" }, { 3, 10.f, 8.f, E_INVALIDARG, "method 3" }, { (DWRITE_LINE_SPACING_METHOD)-1, 10.f, 8.f, E_INVALIDARG, "method -1" },
    { (DWRITE_LINE_SPACING_METHOD)100, 10.f, 8.f, E_INVALIDARG, "method 100" }, { 1, -1.f, 8.f, E_INVALIDARG, "negative spacing" },
    { 1, -1e-30f, 8.f, E_INVALIDARG, "tiny negative spacing" }, { 1, NAN_F, 8.f, E_INVALIDARG, "NaN spacing" }, { 1, 10.f, NAN_F, E_INVALIDARG, "NaN baseline" },
    { 1, -INF_F, 8.f, E_INVALIDARG, "-inf spacing" },
};
static void run_linespacing(IUnknown *o, const char *kind) {
    unsigned i; DWRITE_LINE_SPACING_METHOD m0 = 1; float s0 = 7.f, b0 = 3.f;
    IDWriteTextFormat_SetLineSpacing(TF(o), m0, s0, b0);
    for (i = 0; i < sizeof ls_rows / sizeof ls_rows[0]; i++) {
        const ls_row *r = &ls_rows[i]; DWRITE_LINE_SPACING_METHOD gm = 9; float gs = 99.f, gb = 99.f;
        if (r->want == E_INVALIDARG) { IDWriteTextFormat_SetLineSpacing(TF(o), m0, s0, b0); }
        HRESULT hr = IDWriteTextFormat_SetLineSpacing(TF(o), r->m, r->sp, r->bl);
        CHECKF(hr == r->want, "%s %s -> %s", kind, r->why, dw_hr_name(hr));
        CHECK_HR(IDWriteTextFormat_GetLineSpacing(TF(o), &gm, &gs, &gb), S_OK);
        if (r->want == S_OK) { EXPECT_EQ(gm, r->m); CHECK_FLOAT_EQ(gs, r->sp); CHECK_FLOAT_EQ(gb, r->bl); }
        else { EXPECT_EQ(gm, m0); CHECK_FLOAT_EQ(gs, s0); CHECK_FLOAT_EQ(gb, b0); }
    }
}
TEST(textformat_linespacing_validation) { NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f); if (t) { run_linespacing((IUnknown *)t, "format"); DW_RELEASE(t); } DONE_FACTORY(); }
TEST(textlayout_linespacing_validation) { NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); if (l) { run_linespacing((IUnknown *)l, "layout"); DW_RELEASE(l); } DONE_FACTORY(); }
TEST(textformat2_linespacing_struct_validation) {
    NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f); IDWriteTextFormat2 *t2 = NULL;
    if (t && SUCCEEDED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat2, (void **)&t2)) && t2) {
        DWRITE_LINE_SPACING ok = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.f, 16.f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, ls; HRESULT hr;
        CHECK_HR(IDWriteTextFormat2_SetLineSpacing(t2, &ok), S_OK);
        if (!t_is_wine()) CHECK_HR(IDWriteTextFormat2_SetLineSpacing(t2, NULL), E_INVALIDARG);
        /* height >= 0, 0 <= leadingBefore <= 1, no NaN anywhere, method <= 2 */
        struct { DWRITE_LINE_SPACING s; const char *why; } bad[] = {
            { { 3, 20.f, 16.f, 0.5f, 0 }, "method 3" }, { { 1, -1.f, 16.f, 0.5f, 0 }, "height -1" }, { { 1, NAN_F, 16.f, 0.5f, 0 }, "height NaN" },
            { { 1, 20.f, NAN_F, 0.5f, 0 }, "baseline NaN" }, { { 1, 20.f, 16.f, -0.1f, 0 }, "leadingBefore < 0" }, { { 1, 20.f, 16.f, 1.1f, 0 }, "leadingBefore > 1" },
            { { 1, 20.f, 16.f, NAN_F, 0 }, "leadingBefore NaN" },
        };
        unsigned i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) { hr = IDWriteTextFormat2_SetLineSpacing(t2, &bad[i].s); CHECKF(hr == E_INVALIDARG, "%s -> %s", bad[i].why, dw_hr_name(hr)); }
        memset(&ls, 0, sizeof ls);
        CHECK_HR(IDWriteTextFormat2_GetLineSpacing(t2, &ls), S_OK);
        EXPECT_EQ(ls.method, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FLOAT_EQ(ls.height, 20.f); CHECK_FLOAT_EQ(ls.leadingBefore, 0.5f);
        { DWRITE_LINE_SPACING edge = { 2, 0.f, -3.f, 1.f, 0 }; CHECK_HR(IDWriteTextFormat2_SetLineSpacing(t2, &edge), S_OK); }
        DW_RELEASE(t2);
    } else t_skip("IDWriteTextFormat2 not available");
    DW_RELEASE(t); DONE_FACTORY();
}

/* ---- trimming ------------------------------------------------------------------------------------------------------------------- */
static void run_trimming(IUnknown *o, const char *kind) {
    unsigned i; DWRITE_TRIMMING good = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x20, 3 }, got; IDWriteInlineObject *sign = NULL;
    struct { DWRITE_TRIMMING t; const char *why; } bad[] = {
        { { 3, 0, 0 }, "granularity 3" }, { { (DWRITE_TRIMMING_GRANULARITY)-1, 0, 0 }, "granularity -1" }, { { 100, 0, 0 }, "granularity 100" },
        { { 1, 0x110000, 0 }, "delimiter 0x110000" }, { { 1, 0xFFFFFFFFu, 0 }, "delimiter -1" },
    };
    CHECK_HR(IDWriteTextFormat_SetTrimming(TF(o), &good, NULL), S_OK);
    if (!t_is_wine()) CHECK_HR(IDWriteTextFormat_SetTrimming(TF(o), NULL, NULL), E_INVALIDARG);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteTextFormat_SetTrimming(TF(o), &bad[i].t, NULL);
        CHECKF(hr == E_INVALIDARG, "%s %s -> %s", kind, bad[i].why, dw_hr_name(hr));
    }
    memset(&got, 0xEE, sizeof got);
    CHECK_HR(IDWriteTextFormat_GetTrimming(TF(o), &got, &sign), S_OK);
    EXPECT_EQ(got.granularity, DWRITE_TRIMMING_GRANULARITY_WORD); EXPECT_EQ(got.delimiter, 0x20); EXPECT_EQ(got.delimiterCount, 3); CHECK(sign == NULL);
    { DWRITE_TRIMMING edge = { 0, 0x10FFFF, 0xFFFFFFFFu }; CHECK_HR(IDWriteTextFormat_SetTrimming(TF(o), &edge, NULL), S_OK); }
    { DWRITE_TRIMMING edge = { 2, 0, 0 }; CHECK_HR(IDWriteTextFormat_SetTrimming(TF(o), &edge, NULL), S_OK); }
}
TEST(textformat_trimming_validation) { NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f); if (t) { run_trimming((IUnknown *)t, "format"); DW_RELEASE(t); } DONE_FACTORY(); }
TEST(textlayout_trimming_validation) { NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); if (l) { run_trimming((IUnknown *)l, "layout"); DW_RELEASE(l); } DONE_FACTORY(); }

/* ---- text ranges on the layout setters --------------------------------------------------------------------------------------------- */
typedef HRESULT (*range_fn)(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r);
static HRESULT r_coll(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontCollection(l, NULL, r); }
static HRESULT r_fam(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontFamilyName(l, L"Arial", r); }
static HRESULT r_wt(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, r); }
static HRESULT r_st(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, r); }
static HRESULT r_sx(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontStretch(l, DWRITE_FONT_STRETCH_CONDENSED, r); }
static HRESULT r_sz(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontSize(l, 20.f, r); }
static HRESULT r_ul(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetUnderline(l, TRUE, r); }
static HRESULT r_sk(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetStrikethrough(l, TRUE, r); }
static HRESULT r_de(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetDrawingEffect(l, NULL, r); }
static HRESULT r_io(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetInlineObject(l, NULL, r); }
static HRESULT r_ty(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetTypography(l, NULL, r); }
static HRESULT r_lc(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetLocaleName(l, L"en-us", r); }
static HRESULT r_pk(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { IDWriteTextLayout1 *l1 = NULL; HRESULT hr = IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1); if (SUCCEEDED(hr)) { hr = IDWriteTextLayout1_SetPairKerning(l1, TRUE, r); IDWriteTextLayout1_Release(l1); } return hr; }
static HRESULT r_cs(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { IDWriteTextLayout1 *l1 = NULL; HRESULT hr = IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1); if (SUCCEEDED(hr)) { hr = IDWriteTextLayout1_SetCharacterSpacing(l1, 1.f, 1.f, 0.f, r); IDWriteTextLayout1_Release(l1); } return hr; }
static HRESULT r_ax(IDWriteTextLayout *l, DWRITE_TEXT_RANGE r) { IDWriteTextLayout4 *l4 = NULL; HRESULT hr = IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4); if (SUCCEEDED(hr)) { hr = IDWriteTextLayout4_SetFontAxisValues(l4, NULL, 0, r); IDWriteTextLayout4_Release(l4); } return hr; }
static const struct { const char *name; range_fn fn; } range_fns[] = {
    { "SetFontCollection", r_coll }, { "SetFontFamilyName", r_fam }, { "SetFontWeight", r_wt }, { "SetFontStyle", r_st }, { "SetFontStretch", r_sx }, { "SetFontSize", r_sz },
    { "SetUnderline", r_ul }, { "SetStrikethrough", r_sk }, { "SetDrawingEffect", r_de }, { "SetInlineObject", r_io }, { "SetTypography", r_ty }, { "SetLocaleName", r_lc },
    { "SetPairKerning", r_pk }, { "SetCharacterSpacing", r_cs }, { "SetFontAxisValues(range)", r_ax },
};
static const struct { UINT32 start, len; int ok; const char *why; } ranges[] = {
    { 0, 0, 1, "empty at 0" }, { 0, 5, 1, "whole text" }, { 2, 2, 1, "inside" }, { 3, 100, 1, "runs past the end" }, { 100, 5, 1, "starts past the end" },
    { 0xFFFFFFFFu, 0, 1, "start -1 length 0" }, { 0xFFFFFFF0u, 0x0F, 1, "sum 0xffffffff" }, { 0, 0xFFFFFFFFu, 1, "length -1 from 0" },
    { 0xFFFFFFFFu, 1, 0, "start -1 length 1 (wraps)" }, { 0xFFFFFFFFu, 0xFFFFFFFFu, 0, "both -1" }, { 0x80000000u, 0x80000000u, 0, "sum wraps to 0" },
    { 1, 0xFFFFFFFFu, 0, "1 + -1 wraps" }, { 0xFFFFFFF0u, 0x10, 0, "sum 0x100000000" }, { 0x7fffffffu, 0x80000001u, 0, "wraps to 0" },
};
TEST(layout_range_overflow_is_invalidarg) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); unsigned i, k;
    if (l) {
        for (k = 0; k < sizeof range_fns / sizeof range_fns[0]; k++) {
            HRESULT probe = range_fns[k].fn(l, (DWRITE_TEXT_RANGE){ 0, 1 });
            if (probe == E_NOINTERFACE) continue;                      /* newer interface absent on this OS */
            for (i = 0; i < sizeof ranges / sizeof ranges[0]; i++) {
                HRESULT hr = range_fns[k].fn(l, (DWRITE_TEXT_RANGE){ ranges[i].start, ranges[i].len });
                HRESULT want = ranges[i].ok ? S_OK : E_INVALIDARG;
                CHECKF(hr == want, "%s range {%u,%u} (%s) -> %s", range_fns[k].name, ranges[i].start, ranges[i].len, ranges[i].why, dw_hr_name(hr));
            }
        }
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
TEST(layout_value_validation_with_valid_range) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); DWRITE_TEXT_RANGE r = { 0, 5 }; int i;
    if (l) {
        static const int wgood[] = { 1, 100, 400, 700, 950, 999 }, wbad[] = { 0, 1000, 1001, 5000, -1 };
        for (i = 0; i < 6; i++) CHECKF(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)wgood[i], r) == S_OK, "weight %d", wgood[i]);
        for (i = 0; i < 5; i++) CHECKF(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)wbad[i], r) == E_INVALIDARG, "weight %d", wbad[i]);
        for (i = 0; i <= 2; i++) CHECKF(IDWriteTextLayout_SetFontStyle(l, (DWRITE_FONT_STYLE)i, r) == S_OK, "style %d", i);
        for (i = 3; i <= 5; i++) CHECKF(IDWriteTextLayout_SetFontStyle(l, (DWRITE_FONT_STYLE)i, r) == E_INVALIDARG, "style %d", i);
        CHECK_HR(IDWriteTextLayout_SetFontStyle(l, (DWRITE_FONT_STYLE)-1, r), E_INVALIDARG);
        for (i = 1; i <= 9; i++) CHECKF(IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)i, r) == S_OK, "stretch %d", i);
        CHECK_HR(IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)0, r), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)10, r), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout_SetFontStretch(l, (DWRITE_FONT_STRETCH)-1, r), E_INVALIDARG);
        static const float sgood[] = { 0.001f, 1.f, 12.5f, 1e5f }, sbad[] = { 0.f, -0.f, -1.f, NAN_F, -INF_F };
        for (i = 0; i < 4; i++) CHECKF(IDWriteTextLayout_SetFontSize(l, sgood[i], r) == S_OK, "size %g", sgood[i]);
        for (i = 0; i < 5; i++) CHECKF(IDWriteTextLayout_SetFontSize(l, sbad[i], r) == E_INVALIDARG, "size %g", sbad[i]);
        CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, r), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout_SetLocaleName(l, NULL, r), E_INVALIDARG);
        { DWRITE_TEXT_RANGE e0 = { 0, 0 };
          CHECK_HR(IDWriteTextLayout_SetFontFamilyName(l, NULL, e0), E_INVALIDARG);      /* NULL name is rejected even for an empty range */
          CHECK_HR(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)0, e0), E_INVALIDARG); }
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
TEST(layout_rejected_set_keeps_previous_value) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); DWRITE_TEXT_RANGE r = { 0, 5 }, got;
    if (l) {
        CHECK_HR(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, r), S_OK);
        CHECK_HR(IDWriteTextLayout_SetFontWeight(l, (DWRITE_FONT_WEIGHT)0, r), E_INVALIDARG);
        { DWRITE_FONT_WEIGHT w = 0; CHECK_HR(IDWriteTextLayout_GetFontWeight(l, 0, &w, &got), S_OK); EXPECT_EQ(w, DWRITE_FONT_WEIGHT_BOLD); }
        CHECK_HR(IDWriteTextLayout_SetFontSize(l, 30.f, r), S_OK);
        CHECK_HR(IDWriteTextLayout_SetFontSize(l, -3.f, r), E_INVALIDARG);
        { FLOAT sz = 0; CHECK_HR(IDWriteTextLayout_GetFontSize(l, 0, &sz, &got), S_OK); CHECK_FLOAT_EQ(sz, 30.f); }
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
TEST(layout_character_spacing_validation) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); IDWriteTextLayout1 *l1 = NULL; DWRITE_TEXT_RANGE r = { 0, 5 };
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout1, (void **)&l1)) && l1) {
        CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 1.f, 2.f, 3.f, r), S_OK);
        CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, -1.f, -2.f, 0.f, r), S_OK);                 /* negative leading/trailing are legal */
        CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, NAN_F, 0.f, 0.f, r), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0.f, NAN_F, 0.f, r), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0.f, 0.f, -1.f, r), E_INVALIDARG);          /* minimumAdvanceWidth >= 0 */
        CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0.f, 0.f, NAN_F, r), E_INVALIDARG);
        { FLOAT a = -9, b = -9, c = -9; DWRITE_TEXT_RANGE g;
          CHECK_HR(IDWriteTextLayout1_GetCharacterSpacing(l1, 0, &a, &b, &c, &g), S_OK);
          CHECK_FLOAT_EQ(a, -1.f); CHECK_FLOAT_EQ(b, -2.f); CHECK_FLOAT_EQ(c, 0.f); }
        DW_RELEASE(l1);
    } else t_skip("IDWriteTextLayout1 not available");
    DW_RELEASE(l); DONE_FACTORY();
}
TEST(layout_font_axis_values_validation) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); IDWriteTextLayout4 *l4 = NULL; DWRITE_TEXT_RANGE r = { 0, 5 };
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4)) && l4) {
        DWRITE_FONT_AXIS_VALUE v[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 100.f } };
        CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, v, 2, r), S_OK);
        CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, NULL, 0, r), S_OK);
        if (!t_is_wine()) CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, NULL, 1, r), E_INVALIDARG);                    /* count > 0 needs a pointer */
        if (!t_is_wine()) {
            CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, v, 0x10000, r), E_INVALIDARG);                 /* count must be < 65536 */
            CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, v, 0xFFFFFFFFu, r), E_INVALIDARG);
        }
        { DWRITE_TEXT_RANGE wr = { 0xFFFFFFFFu, 2 }; CHECK_HR(IDWriteTextLayout4_SetFontAxisValues(l4, v, 2, wr), E_INVALIDARG); }
        DW_RELEASE(l4);
    } else t_skip("IDWriteTextLayout4 not available");
    DW_RELEASE(l); DONE_FACTORY();
}
TEST(textformat3_font_axis_and_automatic_axes_validation) {
    NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f); IDWriteTextFormat3 *t3 = NULL;
    if (t && SUCCEEDED(IDWriteTextFormat_QueryInterface(t, &IID_IDWriteTextFormat3, (void **)&t3)) && t3) {
        DWRITE_FONT_AXIS_VALUE v[1] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.f } };
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, v, 1), S_OK);
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, NULL, 0), S_OK);
        if (!t_is_wine()) CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, NULL, 1), E_INVALIDARG);
        if (!t_is_wine()) CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, v, 0x10000), E_INVALIDARG);
        /* The plain format stores any mask without validation (DWriteTextFormat::SetAutomaticFontAxes just assigns). */
        CHECK_HR(IDWriteTextFormat3_SetAutomaticFontAxes(t3, (DWRITE_AUTOMATIC_FONT_AXES)2), S_OK);
        CHECK_HR(IDWriteTextFormat3_SetAutomaticFontAxes(t3, (DWRITE_AUTOMATIC_FONT_AXES)0x100), S_OK);
        EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), 0x100);
        CHECK_HR(IDWriteTextFormat3_SetAutomaticFontAxes(t3, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE), S_OK);
        DW_RELEASE(t3);
    } else t_skip("IDWriteTextFormat3 not available");
    DW_RELEASE(t); DONE_FACTORY();
}

/* ---- NULL pointers on the layout --------------------------------------------------------------------------------------------------- */
TEST(layout_null_output_pointers) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL);
    if (l && !t_is_wine()) {
        CHECK_HR(IDWriteTextLayout_GetOverhangMetrics(l, NULL), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout_DetermineMinWidth(l, NULL), E_INVALIDARG);
        CHECK_HR(IDWriteTextLayout_Draw(l, NULL, NULL, 0.f, 0.f), E_INVALIDARG);                       /* renderer is mandatory, the context is not */
        CHECK_HR(IDWriteTextLayout_Draw(l, (void *)0x1234, NULL, 10.f, 10.f), E_INVALIDARG);
    } else if (l) t_skip("NULL outputs are dereferenced by the emulated implementation");
    DW_RELEASE(l); DONE_FACTORY();
}
TEST(layout_getters_with_valid_arguments_succeed) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL);
    if (l) {
        DWRITE_OVERHANG_METRICS om; FLOAT mw = -1; DWRITE_TEXT_METRICS tm;
        CHECK_HR(IDWriteTextLayout_GetOverhangMetrics(l, &om), S_OK);
        CHECK_HR(IDWriteTextLayout_DetermineMinWidth(l, &mw), S_OK); CHECK(mw >= 0.f);
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), S_OK);
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}

/* ---- buffer protocols ---------------------------------------------------------------------------------------------------------------- */
static const WCHAR *const names[] = { L"Arial", L"Segoe UI", L"A" };
TEST(textformat_family_name_buffer_protocol) {
    NEED_FACTORY(); unsigned k;
    for (k = 0; k < 3; k++) {
        IDWriteTextFormat *t = NULL; UINT32 len, n; WCHAR buf[64]; unsigned i;
        if (FAILED(IDWriteFactory_CreateTextFormat(f, names[k], NULL, 400, 0, 5, 12.f, L"en-us", &t)) || !t) continue;
        len = IDWriteTextFormat_GetFontFamilyNameLength(t); EXPECT_EQ(len, (UINT32)lstrlenW(names[k]));
        for (i = 0; i < 64; i++) buf[i] = 0x7777;
        CHECK_HR(IDWriteTextFormat_GetFontFamilyName(t, buf, len + 1), S_OK); EXPECT_WSTR(buf, names[k]);
        for (n = 1; n <= len; n++) {                                           /* every too-small size: ERROR_INSUFFICIENT_BUFFER, string truncated and terminated */
            for (i = 0; i < 64; i++) buf[i] = 0x7777;
            HRESULT hr = IDWriteTextFormat_GetFontFamilyName(t, buf, n);
            CHECKF(hr == EH_NSB, "size %u of %u -> %s", n, len + 1, dw_hr_name(hr));
            CHECKF(buf[n - 1] == 0 && !memcmp(buf, names[k], (n - 1) * sizeof(WCHAR)), "size %u: truncated copy wrong", n);
            CHECK(buf[n] == 0x7777 || n >= 63);
        }
        for (i = 0; i < 64; i++) buf[i] = 0x7777;
        CHECK_HR(IDWriteTextFormat_GetFontFamilyName(t, buf, 0), EH_NSB); EXPECT_EQ(buf[0], 0x7777);          /* size 0: untouched */
        CHECK_HR(IDWriteTextFormat_GetFontFamilyName(t, buf, 64), S_OK); EXPECT_WSTR(buf, names[k]);
        DW_RELEASE(t);
    }
    DONE_FACTORY();
}
TEST(textformat_locale_name_buffer_protocol) {
    NEED_FACTORY(); static const WCHAR *const locs[] = { L"en-us", L"fr-FR", L"de", L"zh-Hans-CN" }; unsigned k;
    for (k = 0; k < 4; k++) {
        IDWriteTextFormat *t = NULL; UINT32 len, n; WCHAR buf[64]; unsigned i;
        if (FAILED(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, 0, 5, 12.f, locs[k], &t)) || !t) continue;
        len = IDWriteTextFormat_GetLocaleNameLength(t); EXPECT_EQ(len, (UINT32)lstrlenW(locs[k]));
        CHECK_HR(IDWriteTextFormat_GetLocaleName(t, buf, len + 1), S_OK);
        CHECKF(!lstrcmpiW(buf, locs[k]), "locale %u: got %ls", k, buf);
        for (n = 1; n <= len; n++) { for (i = 0; i < 64; i++) buf[i] = 0x7777; CHECKF(IDWriteTextFormat_GetLocaleName(t, buf, n) == EH_NSB, "size %u", n); CHECK(buf[n - 1] == 0); }
        CHECK_HR(IDWriteTextFormat_GetLocaleName(t, buf, 0), EH_NSB);
        DW_RELEASE(t);
    }
    DONE_FACTORY();
}
TEST(layout_automatic_font_axes_validation) {
    /* DWriteTextLayout::SetAutomaticFontAxes rejects every bit but bit 0 (OPTICAL_SIZE). */
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); IDWriteTextLayout4 *l4 = NULL;
    if (l && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout4, (void **)&l4)) && l4) {
        CHECK_HR(IDWriteTextLayout4_SetAutomaticFontAxes(l4, DWRITE_AUTOMATIC_FONT_AXES_NONE), S_OK);
        CHECK_HR(IDWriteTextLayout4_SetAutomaticFontAxes(l4, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE), S_OK);
        EXPECT_EQ(IDWriteTextLayout4_GetAutomaticFontAxes(l4), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
        static const int bad[] = { 2, 3, 4, 0x100, 0x80000000, -1 }; unsigned i;
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) CHECKF(IDWriteTextLayout4_SetAutomaticFontAxes(l4, (DWRITE_AUTOMATIC_FONT_AXES)bad[i]) == E_INVALIDARG, "mask 0x%x", bad[i]);
        EXPECT_EQ(IDWriteTextLayout4_GetAutomaticFontAxes(l4), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
        DW_RELEASE(l4);
    } else t_skip("IDWriteTextLayout4 not available");
    DW_RELEASE(l); DONE_FACTORY();
}
TEST(layout_family_and_locale_name_buffer_protocol) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL);
    if (l) {
        UINT32 len = 0, n; WCHAR buf[64]; DWRITE_TEXT_RANGE r; unsigned i;
        CHECK_HR(IDWriteTextLayout_GetFontFamilyNameLength(l, 0, &len, &r), S_OK); CHECK(len >= 1);
        CHECK_HR(IDWriteTextLayout_GetFontFamilyName(l, 0, buf, len + 1, &r), S_OK); EXPECT_EQ(lstrlenW(buf), len);
        for (n = 0; n <= len; n++) {
            for (i = 0; i < 64; i++) buf[i] = 0x7777;
            HRESULT hr = IDWriteTextLayout_GetFontFamilyName(l, 0, buf, n, &r);
            CHECKF(hr == EH_NSB, "family size %u of %u -> %s", n, len + 1, dw_hr_name(hr));
            if (n) EXPECT_EQ(buf[n - 1], 0); else EXPECT_EQ(buf[0], 0x7777);
        }
        CHECK_HR(IDWriteTextLayout_GetLocaleNameLength(l, 0, &len, &r), S_OK);
        CHECK_HR(IDWriteTextLayout_GetLocaleName(l, 0, buf, len + 1, &r), S_OK);
        for (n = 0; n <= len; n++) { for (i = 0; i < 64; i++) buf[i] = 0x7777; HRESULT hr = IDWriteTextLayout_GetLocaleName(l, 0, buf, n, &r); CHECKF(hr == EH_NSB, "locale size %u of %u -> %s", n, len + 1, dw_hr_name(hr)); }
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
typedef struct { const char *name; HRESULT (*get)(IDWriteTextLayout *l, void *buf, UINT32 max, UINT32 *actual); } metric_row;
static HRESULT m_cluster(IDWriteTextLayout *l, void *b, UINT32 m, UINT32 *a) { return IDWriteTextLayout_GetClusterMetrics(l, (DWRITE_CLUSTER_METRICS *)b, m, a); }
static HRESULT m_line(IDWriteTextLayout *l, void *b, UINT32 m, UINT32 *a) { return IDWriteTextLayout_GetLineMetrics(l, (DWRITE_LINE_METRICS *)b, m, a); }
static HRESULT m_line1(IDWriteTextLayout *l, void *b, UINT32 m, UINT32 *a) { IDWriteTextLayout1 *x = NULL; HRESULT hr = IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&x); if (FAILED(hr)) return E_NOINTERFACE; hr = IDWriteTextLayout2_GetLineMetrics((IDWriteTextLayout2 *)x, (DWRITE_LINE_METRICS1 *)b, m, a); IDWriteTextLayout2_Release((IDWriteTextLayout2 *)x); return hr; }
static const metric_row metric_rows[] = { { "GetClusterMetrics", m_cluster }, { "GetLineMetrics", m_line }, { "GetLineMetrics(Layout2)", m_line1 } };
TEST(layout_metric_buffer_protocol) {
    NEED_FACTORY(); unsigned r;
    for (r = 0; r < 3; r++) {
        IDWriteTextLayout *l = eh_layout(f, L"Hello world, this text wraps over lines", 40.f, 400.f, NULL);
        if (!l) continue;
        UINT32 need = 77; uint8_t buf[256 * 64]; memset(buf, 0, sizeof buf);
        HRESULT hr = metric_rows[r].get(l, NULL, 0, &need);
        if (hr == E_NOINTERFACE) { DW_RELEASE(l); continue; }
        CHECKF(hr == EH_NSB, "%s(NULL,0) -> %s", metric_rows[r].name, dw_hr_name(hr)); CHECK(need >= 1);
        UINT32 n = need, a2 = 0;
        CHECK_HR(metric_rows[r].get(l, buf, n, &a2), S_OK); EXPECT_EQ(a2, n);                        /* exact size */
        if (n > 1) { a2 = 0; hr = metric_rows[r].get(l, buf, n - 1, &a2); CHECKF(hr == EH_NSB, "%s(n-1) -> %s", metric_rows[r].name, dw_hr_name(hr)); EXPECT_EQ(a2, n); }
        a2 = 0; CHECK_HR(metric_rows[r].get(l, buf, n + 50, &a2), S_OK); EXPECT_EQ(a2, n);
        a2 = 55; hr = metric_rows[r].get(l, NULL, 3, &a2); CHECKF(hr == E_INVALIDARG && a2 == 0, "%s(NULL,3) -> %s actual=%u", metric_rows[r].name, dw_hr_name(hr), a2);
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
TEST(layout_empty_text_metric_protocol) {
    NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f); IDWriteTextLayout *l = NULL; UINT32 n = 55;
    if (t && SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"", 0, t, 100, 100, &l)) && l) {
        CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &n), S_OK); EXPECT_EQ(n, 0);        /* zero clusters fit in zero slots */
        n = 55; CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n), EH_NSB); EXPECT_EQ(n, 1);  /* an empty paragraph still has one line */
    }
    DW_RELEASE(l); DW_RELEASE(t); DONE_FACTORY();
}
TEST(layout_hittest_range_protocol) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello world", 300.f, 100.f, NULL);
    if (l) {
        UINT32 need = 55; DWRITE_HIT_TEST_METRICS hm[16]; UINT32 a2 = 0;
        HRESULT hr = IDWriteTextLayout_HitTestTextRange(l, 0, 11, 0.f, 0.f, NULL, 0, &need);
        CHECKF(hr == EH_NSB && need >= 1, "(NULL,0) -> %s need=%u", dw_hr_name(hr), need);
        CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0, 11, 0.f, 0.f, hm, 16, &a2), S_OK); EXPECT_EQ(a2, need);
        a2 = 55; CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0, 11, 0.f, 0.f, NULL, 4, &a2), E_INVALIDARG); EXPECT_EQ(a2, 0);
        a2 = 55; CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 0xFFFFFFFFu, 2, 0.f, 0.f, hm, 16, &a2), E_INVALIDARG); EXPECT_EQ(a2, 0);       /* position + length wraps */
        a2 = 55; CHECK_HR(IDWriteTextLayout_HitTestTextRange(l, 5, 0xFFFFFFFFu, 0.f, 0.f, hm, 16, &a2), E_INVALIDARG); EXPECT_EQ(a2, 0);
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
TEST(layout_hittest_range_multiline_needs_larger_buffer) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"aaa bbb ccc ddd eee fff", 30.f, 400.f, NULL);
    if (l) {
        UINT32 need = 0, a2 = 0; DWRITE_HIT_TEST_METRICS hm[1];
        HRESULT hr = IDWriteTextLayout_HitTestTextRange(l, 0, 23, 0.f, 0.f, NULL, 0, &need);
        CHECK_HR(hr, EH_NSB); CHECK(need > 1);
        hr = IDWriteTextLayout_HitTestTextRange(l, 0, 23, 0.f, 0.f, hm, 1, &a2);
        CHECKF(hr == EH_NSB, "one slot for %u -> %s", need, dw_hr_name(hr)); EXPECT_EQ(a2, need);
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
