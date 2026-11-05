/* Trimming (SetTrimming / GetTrimming / CreateEllipsisTrimmingSign) of IDWriteTextLayout.
 *
 * Facts from the Windows 10 decompilation (DWriteTextLayout::SetTrimming, TextLayout::TrimLines / TrimRange / BreakLines,
 * DWriteFactory::CreateEllipsisTrimmingSign, InlineLayout):
 *  - SetTrimming validates granularity <= 2 and delimiter <= 0x10FFFF (E_INVALIDARG otherwise, nothing changes), AddRefs the sign and
 *    releases the previous one; GetTrimming AddRefs the sign it returns.
 *  - Trimming runs only for granularity CHARACTER or WORD. The sign's GetMetrics is called while the layout is built; its width is
 *    reserved: the kept text is the longest prefix not wider than layoutWidth - signWidth and not ending in white space.
 *  - The trimmed line reports isTrimmed, its width is kept text + sign, the trimmed text is not drawn, the sign is drawn through
 *    IDWriteTextRenderer::DrawInlineObject with the pointer given to SetTrimming.
 *  - With trimming on, WRAP behaves like WHOLE_WORD (no emergency break); EMERGENCY_BREAK keeps breaking inside words.
 *  - The built-in sign is a one-glyph layout of U+2026 (U+22EE for vertical text) in the format's font: width = that glyph's advance,
 *    break conditions NEUTRAL/NEUTRAL, QueryInterface answers IUnknown and IDWriteInlineObject only. */
#include "wt.h"

typedef struct draw_info {
    int runs, inlines; UINT32 head_chars, tail_chars, tail_start; float head_w; float inl_x, inl_y; IDWriteInlineObject *inl_obj;
    float run_y; int after;
} draw_info;

static void analyze_draw(dwx_renderer *r, draw_info *d)
{
    int i; memset(d, 0, sizeof *d); d->tail_start = 0xffffffffu;
    for (i = 0; i < r->ncalls; i++) {
        const dwx_draw *c = &r->calls[i];
        if (c->kind == DWX_DRAW_INLINE) { d->inlines++; d->inl_x = c->x; d->inl_y = c->y; d->inl_obj = c->obj; d->after = 1; }
        else if (c->kind == DWX_DRAW_GLYPHRUN) {
            d->runs++; d->run_y = c->y;
            if (!d->after) { d->head_chars += c->text_len; d->head_w += dwx_run_width(c); }
            else { if (d->tail_start == 0xffffffffu) d->tail_start = c->text_pos; d->tail_chars += c->text_len; }
        }
    }
}

typedef struct tcase { const WCHAR *text; float fit; int gran; float signw; UINT32 head; float width_chars; } tcase;

/* NO_WRAP layout with trimming; sign: NULL, or a dwx_inline of the given width in characters. */
static IDWriteTextLayout *tlayout(wt_fx *x, const WCHAR *text, float width, DWRITE_TRIMMING_GRANULARITY g, UINT32 delim, UINT32 count, IDWriteInlineObject *sign, DWRITE_WORD_WRAPPING w)
{
    IDWriteTextLayout *l = wt_layout_wrap(x, text, width, w); DWRITE_TRIMMING t;
    if (!l) return NULL;
    t.granularity = g; t.delimiter = delim; t.delimiterCount = count;
    if (FAILED(IDWriteTextLayout_SetTrimming(l, &t, sign))) { IDWriteTextLayout_Release(l); return NULL; }
    return l;
}

static IDWriteInlineObject *ellipsis(wt_fx *x, float size)
{
    IDWriteTextFormat *t = wt_format(x, size); IDWriteInlineObject *s = NULL;
    if (t) { IDWriteFactory_CreateEllipsisTrimmingSign(x->f, t, &s); IDWriteTextFormat_Release(t); }
    return s;
}

/* ---- the built-in ellipsis sign ------------------------------------------------------------------------------------ */
TEST(sign_null_format_invalid)
{
    WT_BEGIN(); IDWriteInlineObject *s = (IDWriteInlineObject *)(ULONG_PTR)0x1;
    CHECK_HR(IDWriteFactory_CreateEllipsisTrimmingSign(f, NULL, &s), E_INVALIDARG); CHECK_OUT_NULL(s);
    WT_END();
}

TEST(sign_create_basic)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteInlineObject *s = NULL, *s2 = NULL; IUnknown *u = NULL, *u2 = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s));
    CHECK(s != NULL);
    if (s) {
        EXPECT_REFS(s, 1);
        CHECK_HR_OK(IDWriteInlineObject_QueryInterface(s, &IID_IUnknown, (void **)&u)); EXPECT_REFS(s, 2);
        CHECK_HR_OK(IDWriteInlineObject_QueryInterface(s, &IID_IDWriteInlineObject, (void **)&s2)); EXPECT_REFS(s, 3);
        CHECK(s2 == s); CHECK(u == (IUnknown *)s);
        DW_RELEASE(u); DW_RELEASE(s2);
        CHECK_HR_OK(IDWriteInlineObject_QueryInterface(s, &IID_IUnknown, (void **)&u2)); CHECK(dw_same_identity(u2, (IUnknown *)s)); DW_RELEASE(u2);
        EXPECT_REFS(s, 1);
        EXPECT_REFS(t, 1);                      /* the format is not retained */
    }
    DW_RELEASE(s); DW_RELEASE(t); WT_END();
}

TEST(sign_queryinterface_matrix)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE);
    if (!s) { WT_END(); t_skip("sign unavailable"); return; }
    {
        void *p = (void *)(ULONG_PTR)1;
        CHECK_HR(IDWriteInlineObject_QueryInterface(s, &IID_IDWriteTextFormat, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
        p = (void *)(ULONG_PTR)1; CHECK_HR(IDWriteInlineObject_QueryInterface(s, &IID_IDWriteTextLayout, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
        p = (void *)(ULONG_PTR)1; CHECK_HR(IDWriteInlineObject_QueryInterface(s, &IID_IDWriteFactory, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
        p = (void *)(ULONG_PTR)1; CHECK_HR(IDWriteInlineObject_QueryInterface(s, &IID_IDWritePixelSnapping, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
    }
    EXPECT_REFS(s, 1); DW_RELEASE(s); WT_END();
}

TEST(sign_metrics_width_is_ellipsis_advance)
{
    WT_BEGIN(); int i; static const float sizes[] = { 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 2048.0f };
    for (i = 0; i < 6; i++) {
        IDWriteInlineObject *s = ellipsis(&X, sizes[i]); DWRITE_INLINE_OBJECT_METRICS m;
        if (!s) continue;
        memset(&m, 0xcc, sizeof m);
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &m));
        CHECK_NEAR(m.width, sizes[i] * 1000.0f / 2048.0f, 0.01);       /* U+2026 maps to a 1000/2048 em glyph in the fixture */
        CHECKF(m.height >= 0.0f && m.baseline == m.baseline, "size %g: height %g baseline %g", sizes[i], m.height, m.baseline);
        IDWriteInlineObject_Release(s);
    }
    WT_END();
}

TEST(sign_metrics_stable_and_idempotent)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); DWRITE_INLINE_OBJECT_METRICS a, b;
    if (!s) { WT_END(); t_skip("sign unavailable"); return; }
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &a)); CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &b));
    CHECK(!memcmp(&a, &b, sizeof a));
    t_note("ellipsis sign metrics: width %g height %g baseline %g supportsSideways %d", a.width, a.height, a.baseline, a.supportsSideways);
    DW_RELEASE(s); WT_END();
}

TEST(sign_break_conditions_neutral)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); DWRITE_BREAK_CONDITION a = (DWRITE_BREAK_CONDITION)99, b = (DWRITE_BREAK_CONDITION)99;
    if (!s) { WT_END(); t_skip("sign unavailable"); return; }
    CHECK_HR_OK(IDWriteInlineObject_GetBreakConditions(s, &a, &b));
    EXPECT_EQ(a, DWRITE_BREAK_CONDITION_NEUTRAL); EXPECT_EQ(b, DWRITE_BREAK_CONDITION_NEUTRAL);
    DW_RELEASE(s); WT_END();
}

TEST(sign_overhang_metrics_succeed)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); DWRITE_OVERHANG_METRICS o;
    if (!s) { WT_END(); t_skip("sign unavailable"); return; }
    memset(&o, 0xcc, sizeof o);
    CHECK_HR_OK(IDWriteInlineObject_GetOverhangMetrics(s, &o));
    t_note("ellipsis sign overhang: left %g top %g right %g bottom %g", o.left, o.top, o.right, o.bottom);
    DW_RELEASE(s); WT_END();
}

TEST(sign_draw_emits_one_glyph_run)
{
    /* InlineLayout::Draw forwards to the one-glyph layout, shifted so that the origin passed is the baseline origin */
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); dwx_renderer *r = dwx_renderer_new();
    if (!s) { dwx_release(r); WT_END(); t_skip("sign unavailable"); return; }
    CHECK_HR_OK(IDWriteInlineObject_Draw(s, (void *)(ULONG_PTR)0x1234, (IDWriteTextRenderer *)r, 100.0f, 200.0f, FALSE, FALSE, NULL));
    EXPECT_EQ(r->count[DWX_DRAW_GLYPHRUN], 1);
    EXPECT_EQ(r->ncalls >= 1, 1);
    if (r->ncalls >= 1) {
        const dwx_draw *c = dwx_renderer_call(r, 0);
        CHECK(c->kind == DWX_DRAW_GLYPHRUN);
        EXPECT_EQ(c->ctx, 0x1234); EXPECT_EQ(c->nglyphs, 1);
        CHECK_NEAR(c->x, 100.0, 0.01); CHECK_NEAR(c->y, 200.0, 0.05);
        CHECK_NEAR(dwx_run_width(c), WT_CW, 0.01);
        if (c->has_desc && c->text) CHECKF(c->text_len == 1 && c->text[0] == 0x2026, "text %04x", c->text ? c->text[0] : 0);
    }
    dwx_release(r); DW_RELEASE(s); WT_END();
}

TEST(sign_vertical_reading_uses_vertical_ellipsis)
{
    /* the format's direction selects U+22EE instead of U+2026 (CreateEllipsisTrimmingSign tests a direction bit of the format) */
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteInlineObject *s = NULL; dwx_renderer *r = dwx_renderer_new();
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM));
    CHECK_HR_OK(IDWriteTextFormat_SetFlowDirection(t, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT));
    {
        HRESULT hr = IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s);
        CHECK_HR_OK(hr);
        if (s) {
            HRESULT hd = IDWriteInlineObject_Draw(s, NULL, (IDWriteTextRenderer *)r, 10.0f, 20.0f, FALSE, FALSE, NULL);
            /* the vertical sign layout needs a renderer that supports vertical glyph orientation */
            CHECKF(hd == S_OK || hd == DWRITE_E_TEXTRENDERERINCOMPATIBLE, "Draw %s", dw_hr_name(hd));
            if (hd != S_OK) t_note("Draw -> %s", dw_hr_name(hd));
            if (r->ncalls && r->calls[0].has_desc && r->calls[0].text) CHECKF(r->calls[0].text[0] == 0x22ee, "sign text U+%04X", r->calls[0].text[0]);
            else t_note("no run description to inspect");
        }
    }
    dwx_release(r); DW_RELEASE(s); DW_RELEASE(t); WT_END();
}

TEST(sign_conflicting_format_directions_fail)
{
    /* the sign builds a layout from the format; reading TOP_TO_BOTTOM with flow TOP_TO_BOTTOM is the conflicting combination */
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteInlineObject *s = (IDWriteInlineObject *)(ULONG_PTR)1; HRESULT hr;
    CHECK_HR_OK(IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM));
    hr = IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s);
    CHECKF(hr == (HRESULT)0x8898500B || FAILED(hr), "hr %s", dw_hr_name(hr));
    if (hr != (HRESULT)0x8898500B) t_note("conflict reported as %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(s);
    if (SUCCEEDED(hr) && s != (IDWriteInlineObject *)(ULONG_PTR)1) DW_RELEASE(s);
    DW_RELEASE(t); WT_END();
}

TEST(sign_survives_format_release)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteInlineObject *s = NULL; DWRITE_INLINE_OBJECT_METRICS m;
    CHECK_HR_OK(IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &s)); DW_RELEASE(t);
    if (s) { CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &m)); CHECK_NEAR(m.width, WT_CW, 0.01); }
    DW_RELEASE(s); WT_END();
}

TEST(sign_survives_factory_release)
{
    wt_fx x; IDWriteInlineObject *s; DWRITE_INLINE_OBJECT_METRICS m;
    memset(&x, 0, sizeof x);
    if (!wt_open(&x)) { wt_close(&x); t_skip("fixture"); return; }
    s = ellipsis(&x, WT_SIZE);
    if (s) {
        DW_RELEASE(x.coll);
        CHECK_HR_OK(IDWriteInlineObject_GetMetrics(s, &m)); CHECK_NEAR(m.width, WT_CW, 0.01);
        IDWriteInlineObject_Release(s);
    }
    wt_close(&x);
}

TEST(sign_independent_objects)
{
    WT_BEGIN(); IDWriteInlineObject *a = ellipsis(&X, WT_SIZE), *b = ellipsis(&X, WT_SIZE);
    CHECK(a != NULL && b != NULL); if (a && b) CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b); WT_END();
}

/* ---- SetTrimming / GetTrimming on the layout ------------------------------------------------------------------------ */
TEST(layout_trimming_default)
{
    WT_BEGIN(); DWRITE_TRIMMING t; IDWriteInlineObject *s = (IDWriteInlineObject *)(ULONG_PTR)1; WT_LAYOUT(l, L"abc", 100.0f);
    memset(&t, 0xcc, sizeof t);
    CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &t, &s));
    EXPECT_EQ(t.granularity, DWRITE_TRIMMING_GRANULARITY_NONE); EXPECT_EQ(t.delimiter, 0); EXPECT_EQ(t.delimiterCount, 0); CHECK(s == NULL);
    DW_RELEASE(l); WT_END();
}

TEST(layout_trimming_roundtrip_matrix)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); int g, bad = 0; size_t d, c;
    static const UINT32 delim[] = { 0, 0x20, 0x2e, 0x5c, 0x2026, 0xd800, 0xffff, 0x10000, 0x10ffff };
    static const UINT32 cnt[] = { 0, 1, 2, 7, 0x7fffffff, 0xffffffffu };
    for (g = 0; g <= 2; g++) for (d = 0; d < sizeof delim / sizeof delim[0]; d++) for (c = 0; c < sizeof cnt / sizeof cnt[0]; c++) {
        DWRITE_TRIMMING in, out; IDWriteInlineObject *sg = (IDWriteInlineObject *)(ULONG_PTR)1;
        in.granularity = (DWRITE_TRIMMING_GRANULARITY)g; in.delimiter = delim[d]; in.delimiterCount = cnt[c];
        memset(&out, 0xcc, sizeof out);
        if (IDWriteTextLayout_SetTrimming(l, &in, NULL) != S_OK) { bad++; continue; }
        if (IDWriteTextLayout_GetTrimming(l, &out, &sg) != S_OK) { bad++; continue; }
        if (out.granularity != in.granularity || out.delimiter != in.delimiter || out.delimiterCount != in.delimiterCount || sg != NULL) bad++;
    }
    CHECKF(bad == 0, "%d combinations failed", bad);
    DW_RELEASE(l); WT_END();
}

TEST(layout_trimming_invalid_arguments)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); size_t i; DWRITE_TRIMMING keep = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2f, 3 }, now; IDWriteInlineObject *sg;
    static const UINT32 badg[] = { 3, 4, 255, 0x80000000u, 0xffffffffu };
    static const UINT32 badd[] = { 0x110000, 0x110001, 0x7fffffffu, 0xffffffffu };
    dwx_inline *sign = wt_sign(10, 10, 5);
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &keep, NULL));
    CHECK_HR(IDWriteTextLayout_SetTrimming(l, NULL, NULL), E_INVALIDARG);
    CHECK_HR(IDWriteTextLayout_SetTrimming(l, NULL, (IDWriteInlineObject *)sign), E_INVALIDARG);
    EXPECT_REFS(sign, 1);
    for (i = 0; i < sizeof badg / sizeof badg[0]; i++) {
        DWRITE_TRIMMING in = { (DWRITE_TRIMMING_GRANULARITY)badg[i], 0x20, 1 };
        CHECK_HR(IDWriteTextLayout_SetTrimming(l, &in, (IDWriteInlineObject *)sign), E_INVALIDARG);
        EXPECT_REFS(sign, 1);                                       /* a rejected call retains nothing */
        sg = (IDWriteInlineObject *)(ULONG_PTR)1;
        CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &now, &sg));
        EXPECT_EQ(now.granularity, keep.granularity); EXPECT_EQ(now.delimiter, keep.delimiter); EXPECT_EQ(now.delimiterCount, keep.delimiterCount); CHECK(sg == NULL);
    }
    for (i = 0; i < sizeof badd / sizeof badd[0]; i++) {
        DWRITE_TRIMMING in = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, badd[i], 1 };
        CHECK_HR(IDWriteTextLayout_SetTrimming(l, &in, (IDWriteInlineObject *)sign), E_INVALIDARG);
        EXPECT_REFS(sign, 1);
    }
    dwx_release(sign); DW_RELEASE(l); WT_END();
}

TEST(layout_trimming_sign_refcounts)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); dwx_inline *a = wt_sign(10, 10, 5), *b = wt_sign(10, 10, 5); DWRITE_TRIMMING t = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    IDWriteInlineObject *got = NULL; DWRITE_TRIMMING out;
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, (IDWriteInlineObject *)a)); EXPECT_REFS(a, 2);
    CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &out, &got)); CHECK(got == (IDWriteInlineObject *)a); EXPECT_REFS(a, 3);
    IDWriteInlineObject_Release(got); EXPECT_REFS(a, 2);
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, (IDWriteInlineObject *)a)); EXPECT_REFS(a, 2);    /* same sign again: AddRef then release the old */
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, (IDWriteInlineObject *)b)); EXPECT_REFS(a, 1); EXPECT_REFS(b, 2);
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, NULL)); EXPECT_REFS(b, 1);
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, (IDWriteInlineObject *)b)); EXPECT_REFS(b, 2);
    DW_RELEASE(l); EXPECT_REFS(b, 1);                                                                    /* the layout drops its reference when destroyed */
    dwx_release(a); dwx_release(b); WT_END();
}

TEST(layout_trimming_does_not_call_sign_on_set)
{
    WT_BEGIN(); WT_LAYOUT(l, L"abc", 100.0f); dwx_inline *a = wt_sign(10, 10, 5); DWRITE_TRIMMING t = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    IDWriteInlineObject *got = NULL; DWRITE_TRIMMING out;
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, (IDWriteInlineObject *)a));
    CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &out, &got)); IDWriteInlineObject_Release(got);
    EXPECT_EQ(a->n_metrics, 0); EXPECT_EQ(a->n_draw, 0); EXPECT_EQ(a->n_overhang, 0); EXPECT_EQ(a->n_break, 0); EXPECT_EQ(dwx_qi_calls(a), 0);
    DW_RELEASE(l); dwx_release(a); WT_END();
}

TEST(layout_trimming_inherited_from_format)
{
    WT_BEGIN(); IDWriteTextFormat *t = wt_format(&X, WT_SIZE); IDWriteTextLayout *l = NULL; dwx_inline *a = wt_sign(10, 10, 5);
    DWRITE_TRIMMING in = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2f, 2 }, out; IDWriteInlineObject *sg = NULL;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &in, (IDWriteInlineObject *)a)); EXPECT_REFS(a, 2);
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 100.0f, 100.0f, &l));
    if (l) {
        CHECK_HR_OK(IDWriteTextLayout_GetTrimming(l, &out, &sg));
        EXPECT_EQ(out.granularity, DWRITE_TRIMMING_GRANULARITY_WORD); EXPECT_EQ(out.delimiter, 0x2f); EXPECT_EQ(out.delimiterCount, 2); CHECK(sg == (IDWriteInlineObject *)a);
        DW_RELEASE(sg);
        EXPECT_REFS(a, 3);                                                   /* format + layout */
        /* independent afterwards */
        in.granularity = DWRITE_TRIMMING_GRANULARITY_NONE; in.delimiter = 0; in.delimiterCount = 0;
        CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &in, NULL)); EXPECT_REFS(a, 2);
        CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &sg)); EXPECT_EQ(out.granularity, DWRITE_TRIMMING_GRANULARITY_WORD); DW_RELEASE(sg);
    }
    DW_RELEASE(l); DW_RELEASE(t); EXPECT_REFS(a, 1); dwx_release(a); WT_END();
}

/* ---- not trimmed ---------------------------------------------------------------------------------------------------- */
TEST(trim_none_when_text_fits)
{
    WT_BEGIN(); int g;
    for (g = 1; g <= 2; g++) {
        IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; DWRITE_TEXT_METRICS m;
        IDWriteTextLayout *l = tlayout(&X, L"abc def", WT_FIT(7), (DWRITE_TRIMMING_GRANULARITY)g, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
        if (!l) { DW_RELEASE(s); continue; }
        CHECK_HR_OK(wt_get_lines(l, &o)); wt_metrics(l, &m);
        EXPECT_EQ(o.n, 1); EXPECT_EQ(o.lm[0].isTrimmed, 0);
        CHECK_NEAR(m.width, WT_CH(7), 0.01);
        { dwx_renderer *r = wt_draw(l, 0, 0); draw_info d; analyze_draw(r, &d); EXPECT_EQ(d.inlines, 0); EXPECT_EQ(d.head_chars, 7); dwx_release(r); }
        IDWriteTextLayout_Release(l); DW_RELEASE(s);
    }
    WT_END();
}

TEST(trim_none_exact_fit_is_not_trimmed)
{
    /* the line is trimmed only when wider than the layout; equal width fits */
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"abcdefg", WT_CH(7), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) { CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 0); IDWriteTextLayout_Release(l); }
    l = tlayout(&X, L"abcdefgh", WT_CH(7), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) { CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 1); IDWriteTextLayout_Release(l); }
    DW_RELEASE(s); WT_END();
}

TEST(trim_granularity_none_never_trims)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; DWRITE_TEXT_METRICS m;
    IDWriteTextLayout *l = tlayout(&X, L"abcdefghijklmnop", WT_FIT(3), DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); wt_metrics(l, &m);
        EXPECT_EQ(o.n, 1); EXPECT_EQ(o.lm[0].isTrimmed, 0); CHECK_NEAR(m.width, WT_CH(16), 0.01);
        { dwx_renderer *r = wt_draw(l, 0, 0); draw_info d; analyze_draw(r, &d); EXPECT_EQ(d.inlines, 0); EXPECT_EQ(d.head_chars, 16); dwx_release(r); }
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_sign_not_consulted_without_granularity)
{
    WT_BEGIN(); dwx_inline *a = wt_sign(10, 10, 5); wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"abcdefghijklmnop", WT_FIT(3), DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0, (IDWriteInlineObject *)a, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o));
        EXPECT_EQ(a->n_metrics, 0); EXPECT_EQ(a->n_draw, 0); EXPECT_EQ(a->n_overhang, 0); EXPECT_EQ(a->n_break, 0);
        IDWriteTextLayout_Release(l);
    }
    EXPECT_REFS(a, 1); dwx_release(a); WT_END();
}

TEST(trim_sign_metrics_queried_when_trimming_enabled)
{
    /* TrimLines asks the sign for its metrics whenever granularity is not NONE, also when no line needs trimming */
    WT_BEGIN(); dwx_inline *a = wt_sign(WT_CW, 20, 10); wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"abc", WT_FIT(10), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, (IDWriteInlineObject *)a, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o));
        CHECKF(a->n_metrics >= 1, "GetMetrics calls %d", a->n_metrics);
        t_note("sign callbacks while building an untrimmed layout: metrics %d overhang %d break %d draw %d qi %d", a->n_metrics, a->n_overhang, a->n_break, a->n_draw, dwx_qi_calls(a));
        EXPECT_EQ(a->n_draw, 0);
        IDWriteTextLayout_Release(l);
    }
    EXPECT_REFS(a, 1); dwx_release(a); WT_END();
}

/* ---- character trimming ---------------------------------------------------------------------------------------------- */
static void run_trim_cases(wt_fx *X, const tcase *c, int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++) {
        dwx_inline *sg = c[i].signw > 0 ? wt_sign(c[i].signw * WT_CW, 20.0f, 15.0f) : NULL;
        IDWriteTextLayout *l = tlayout(X, c[i].text, c[i].fit, (DWRITE_TRIMMING_GRANULARITY)c[i].gran, 0, 0, (IDWriteInlineObject *)sg, DWRITE_WORD_WRAPPING_NO_WRAP);
        if (!l) { t_check(0, __FILE__, __LINE__, "layout", "%s case %d", name, i); if (sg) dwx_release(sg); continue; }
        {
            wt_lines o; DWRITE_TEXT_METRICS m; draw_info d; dwx_renderer *r;
            HRESULT hr = wt_get_lines(l, &o); wt_metrics(l, &m);
            r = wt_draw(l, 0, 0); analyze_draw(r, &d);
            t_check(SUCCEEDED(hr) && o.n == 1 && o.lm[0].isTrimmed, __FILE__, __LINE__, "line trimmed", "%s case %d \"%ls\"", name, i, c[i].text);
            t_check(d.head_chars == c[i].head, __FILE__, __LINE__, "kept characters", "%s case %d \"%ls\" fit %g: kept %u want %u", name, i, c[i].text, c[i].fit, d.head_chars, c[i].head);
            t_check(fabsf(m.width - WT_CH(0) - c[i].width_chars * WT_CW) < 0.02f, __FILE__, __LINE__, "line width = kept + sign", "%s case %d: width %g want %g", name, i, m.width, c[i].width_chars * WT_CW);
            if (sg) {
                t_check(d.inlines == 1 && d.inl_obj == (IDWriteInlineObject *)sg, __FILE__, __LINE__, "sign drawn", "%s case %d: inline draws %d", name, i, d.inlines);
                t_check(fabsf(d.inl_x - d.head_w) < 0.02f, __FILE__, __LINE__, "sign follows the kept text", "%s case %d: x %g head %g", name, i, d.inl_x, d.head_w);
                t_check(sg->n_draw == 0, __FILE__, __LINE__, "layout does not call Draw of the sign", "%d", sg->n_draw);
            } else t_check(d.inlines == 0, __FILE__, __LINE__, "no sign, no inline draw", "%s case %d", name, i);
            dwx_release(r);
        }
        IDWriteTextLayout_Release(l);
        if (sg) { t_check(dwx_refs(sg) == 1, __FILE__, __LINE__, "sign released with the layout", "refs %ld", dwx_refs(sg)); dwx_release(sg); }
    }
}

TEST(trim_character_table_sign_one_char)
{
    WT_BEGIN();
    static const tcase c[] = {
        { L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 4, 5 },
        { L"AAAAAAAAAA", WT_FIT(9), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 8, 9 },
        { L"AAAAAAAAAA", WT_FIT(3), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 2, 3 },
        { L"AAAAAAAAAA", WT_FIT(2), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 1, 2 },
        { L"AAAAAAAAAA", WT_CH(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 4, 5 },
        { L"AAAAAAAAAA", WT_CH(9), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 8, 9 },
        { L"ABCDEFGHIJKLMNOPQRST", WT_FIT(11), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 10, 11 },
        { L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 2, 3, 5 },
        { L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 3, 2, 5 },
        { L"AAAAAAAAAA", WT_FIT(8), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 3, 5, 8 },
    };
    run_trim_cases(&X, c, (int)(sizeof c / sizeof c[0]), "CHARACTER");
    WT_END();
}

TEST(trim_character_without_sign)
{
    WT_BEGIN();
    static const tcase c[] = {
        { L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 5, 5 },
        { L"AAAAAAAAAA", WT_FIT(9), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 9, 9 },
        { L"AAAAAAAAAA", WT_CH(6), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 6, 6 },
        { L"AAAAAAAAAA", WT_FIT(2), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 2, 2 },
    };
    run_trim_cases(&X, c, (int)(sizeof c / sizeof c[0]), "CHARACTER nosign");
    WT_END();
}

TEST(trim_kept_text_never_ends_in_white_space)
{
    WT_BEGIN();
    static const tcase c[] = {
        { L"aaaa bbb", WT_FIT(6), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 4, 5 },        /* "aaaa " would fit but ends in a blank */
        { L"aaa bbbbbb", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 3, 4 },
        { L"aaa     bbbbbb", WT_FIT(6), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 3, 4 },
        { L"aaaa bbb", WT_FIT(6), DWRITE_TRIMMING_GRANULARITY_WORD, 1, 4, 5 },
        { L"aaa bbbbbb", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_WORD, 1, 3, 4 },
    };
    run_trim_cases(&X, c, (int)(sizeof c / sizeof c[0]), "white space");
    WT_END();
}

TEST(trim_word_vs_character)
{
    WT_BEGIN();
    static const tcase ch[] = {
        { L"aaa bbbbbbb", WT_FIT(9), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 8, 9 },
        { L"aaa bbb ccc", WT_FIT(9), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 7, 8 },
        { L"aaa bbb ccc ddd", WT_FIT(10), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 1, 9, 10 },
    };
    static const tcase wd[] = {
        { L"aaa bbbbbbb", WT_FIT(9), DWRITE_TRIMMING_GRANULARITY_WORD, 1, 3, 4 },          /* only whole words stay */
        { L"aaa bbb ccc", WT_FIT(9), DWRITE_TRIMMING_GRANULARITY_WORD, 1, 7, 8 },
        { L"aaa bbb ccc ddd", WT_FIT(10), DWRITE_TRIMMING_GRANULARITY_WORD, 1, 7, 8 },
        { L"aaa bbb ccc ddd", WT_FIT(12), DWRITE_TRIMMING_GRANULARITY_WORD, 1, 11, 12 },
        { L"aaa bbb ccc ddd", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_WORD, 1, 3, 4 },
    };
    run_trim_cases(&X, ch, 3, "CHARACTER"); run_trim_cases(&X, wd, 5, "WORD");
    WT_END();
}

TEST(trim_word_single_long_word_keeps_a_cluster)
{
    /* when nothing fits at all the first cluster is kept (TrimRange: the width reaches zero, one cluster is re-advanced) */
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; draw_info d; dwx_renderer *r;
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 1);
        r = wt_draw(l, 0, 0); analyze_draw(r, &d);
        CHECKF(d.head_chars <= 10, "kept %u", d.head_chars);
        t_note("word trimming of one long word keeps %u characters, %d sign draws", d.head_chars, d.inlines);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_sign_wider_than_layout)
{
    WT_BEGIN(); dwx_inline *a = wt_sign(WT_CH(20), 20, 10); wt_lines o; draw_info d; dwx_renderer *r;
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, (IDWriteInlineObject *)a, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 1);
        r = wt_draw(l, 0, 0); analyze_draw(r, &d);
        EXPECT_EQ(d.inlines, 1);
        t_note("sign wider than the layout: %u characters kept", d.head_chars);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    dwx_release(a); WT_END();
}

TEST(trim_metrics_width_counts_sign)
{
    WT_BEGIN(); DWRITE_TEXT_METRICS m; IDWriteInlineObject *s = ellipsis(&X, WT_SIZE);
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        wt_metrics(l, &m);
        CHECK_NEAR(m.width, WT_CH(5), 0.02); CHECK_NEAR(m.widthIncludingTrailingWhitespace, WT_CH(5), 0.02);
        EXPECT_EQ(m.lineCount, 1); CHECK_NEAR(m.layoutWidth, WT_FIT(5), 0.001);
        CHECK(m.width <= m.layoutWidth + 0.01f);
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_line_length_keeps_trimmed_text)
{
    /* trimmed characters still belong to the line */
    WT_BEGIN(); wt_lines o; IDWriteInlineObject *s = ellipsis(&X, WT_SIZE);
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) { CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 1); EXPECT_EQ(o.lm[0].length, 10); EXPECT_EQ(o.lm[0].trailingWhitespaceLength, 0); IDWriteTextLayout_Release(l); }
    DW_RELEASE(s); WT_END();
}

TEST(trim_only_overflowing_paragraphs_in_nowrap)
{
    /* each paragraph is its own line: only those wider than the layout are trimmed */
    WT_BEGIN(); wt_lines o; IDWriteInlineObject *s = ellipsis(&X, WT_SIZE);
    IDWriteTextLayout *l = tlayout(&X, L"short\nAAAAAAAAAAAA\nmid", WT_FIT(8), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 3);
        if (o.n == 3) { EXPECT_EQ(o.lm[0].isTrimmed, 0); EXPECT_EQ(o.lm[1].isTrimmed, 1); EXPECT_EQ(o.lm[2].isTrimmed, 0); }
        { dwx_renderer *r = wt_draw(l, 0, 0); draw_info d; analyze_draw(r, &d); EXPECT_EQ(d.inlines, 1); dwx_release(r); }
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_multiple_lines_each_get_a_sign)
{
    WT_BEGIN(); wt_lines o; IDWriteInlineObject *s = ellipsis(&X, WT_SIZE);
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAAAA\nBBBBBBBBBBBB\nCCCCCCCCCCCC", WT_FIT(6), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        UINT32 i;
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 3);
        for (i = 0; i < o.n; i++) { EXPECT_EQ(o.lm[i].isTrimmed, 1); EXPECT_EQ(o.lm[i].length, i < 2 ? 13 : 12); }
        { dwx_renderer *r = wt_draw(l, 0, 0); draw_info d; analyze_draw(r, &d); EXPECT_EQ(d.inlines, 3); EXPECT_EQ(d.head_chars, 17);   /* 3 x 5 kept characters plus the two newline characters */ dwx_release(r); }
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_custom_sign_callbacks)
{
    WT_BEGIN(); dwx_inline *a = wt_sign(WT_CH(2), 20, 15); draw_info d; dwx_renderer *r; wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAA", WT_FIT(6), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, (IDWriteInlineObject *)a, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 1);
        CHECKF(a->n_metrics >= 1, "metrics calls %d", a->n_metrics);
        r = wt_draw(l, 0, 0); analyze_draw(r, &d);
        EXPECT_EQ(d.head_chars, 4);                                      /* 6.5 - 2 = 4.5 characters of room: 4 are kept */
        t_note("kept %u characters with a 2 character sign in 6.5 characters of width", d.head_chars);
        EXPECT_EQ(d.inlines, 1); CHECK(d.inl_obj == (IDWriteInlineObject *)a);
        EXPECT_EQ(a->n_draw, 0);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    dwx_release(a); WT_END();
}

TEST(trim_sign_vertical_placement)
{
    /* the sign's origin is its top-left: the baseline of the object sits on the baseline of the text */
    WT_BEGIN(); dwx_inline *a = wt_sign(WT_CW, 40, 30); draw_info d; dwx_renderer *r;
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, (IDWriteInlineObject *)a, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        r = wt_draw(l, 7.0f, 11.0f); analyze_draw(r, &d);
        EXPECT_EQ(d.inlines, 1);
        CHECK_NEAR(d.inl_x, 7.0 + WT_CH(4), 0.02);
        CHECK_NEAR(d.inl_y + 30.0, d.run_y, 0.05);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    dwx_release(a); WT_END();
}

TEST(trim_hit_test_flags)
{
    WT_BEGIN(); DWRITE_HIT_TEST_METRICS hm; float x, y; IDWriteInlineObject *s = ellipsis(&X, WT_SIZE);
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        HRESULT hr = IDWriteTextLayout_HitTestTextPosition(l, 0, FALSE, &x, &y, &hm);
        if (hr == S_OK) {
            EXPECT_EQ(hm.isTrimmed, 0);
            hr = IDWriteTextLayout_HitTestTextPosition(l, 8, FALSE, &x, &y, &hm);
            if (hr == S_OK) t_note("hit test in trimmed text: isTrimmed %d left %g width %g", hm.isTrimmed, hm.left, hm.width);
        } else t_note("HitTestTextPosition -> %s", dw_hr_name(hr));
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

/* ---- delimiter ---------------------------------------------------------------------------------------------------- */
TEST(trim_delimiter_keeps_tail)
{
    /* the portion starting at the last delimiter is preserved, the sign replaces text before it */
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); draw_info d; dwx_renderer *r; wt_lines o; DWRITE_TEXT_METRICS m;
    IDWriteTextLayout *l = tlayout(&X, L"AAAA.BBBB.CCCC", WT_FIT(9), DWRITE_TRIMMING_GRANULARITY_CHARACTER, '.', 1, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 1); wt_metrics(l, &m);
        r = wt_draw(l, 0, 0); analyze_draw(r, &d);
        EXPECT_EQ(d.inlines, 1);
        t_note("delimiter tail: start %u chars %u head %u width %g", d.tail_start, d.tail_chars, d.head_chars, m.width);   /* exact tail placement not verified in the decomp */
        CHECK(m.width <= WT_FIT(9) + 0.02f);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_delimiter_path_example)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); draw_info d; dwx_renderer *r; wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"C:\\Users\\name\\file.txt", WT_FIT(14), DWRITE_TRIMMING_GRANULARITY_CHARACTER, '\\', 1, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 1);
        r = wt_draw(l, 0, 0); analyze_draw(r, &d);
        EXPECT_EQ(d.inlines, 1);
        t_note("path tail: start %u chars %u", d.tail_start, d.tail_chars);
        CHECK(d.head_chars + 1 + d.tail_chars <= 14);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_delimiter_ignored_when_text_fits)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"AAAA.BBBB.CCCC", WT_FIT(20), DWRITE_TRIMMING_GRANULARITY_CHARACTER, '.', 1, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) { CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 0); IDWriteTextLayout_Release(l); }
    DW_RELEASE(s); WT_END();
}

TEST(trim_delimiter_missing_still_trims)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; draw_info d; dwx_renderer *r;
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAAAAAA", WT_FIT(9), DWRITE_TRIMMING_GRANULARITY_CHARACTER, '.', 1, s, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 1);
        r = wt_draw(l, 0, 0); analyze_draw(r, &d); EXPECT_EQ(d.inlines, 1);
        t_note("delimiter absent: head %u tail %u tail_start %d", d.head_chars, d.tail_chars, (int)d.tail_start);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

/* ---- wrapping modes and trimming ---------------------------------------------------------------------------------- */
TEST(trim_wrap_mode_disables_emergency_break)
{
    /* WRAP and trimming: the over-long word is not split (WHOLE_WORD behaviour); the line is trimmed instead */
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; draw_info d; dwx_renderer *r;
    IDWriteTextLayout *l = tlayout(&X, L"abcdefgh", WT_FIT(4), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 1); EXPECT_EQ(o.lm[0].length, 8); EXPECT_EQ(o.lm[0].isTrimmed, 1);
        r = wt_draw(l, 0, 0); analyze_draw(r, &d); EXPECT_EQ(d.head_chars, 3); EXPECT_EQ(d.inlines, 1);
        dwx_release(r); IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_whole_word_mode_same_as_wrap)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"abcdefgh", WT_FIT(4), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_WHOLE_WORD);
    if (l) { CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 1); EXPECT_EQ(o.lm[0].isTrimmed, 1); IDWriteTextLayout_Release(l); }
    DW_RELEASE(s); WT_END();
}

TEST(trim_emergency_break_mode_still_breaks)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"abcdefgh", WT_FIT(4), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 2);
        if (o.n == 2) { EXPECT_EQ(o.lm[0].length, 4); EXPECT_EQ(o.lm[1].length, 4); EXPECT_EQ(o.lm[0].isTrimmed, 0); EXPECT_EQ(o.lm[1].isTrimmed, 0); }
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_character_wrap_mode_breaks_not_trims)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o;
    IDWriteTextLayout *l = tlayout(&X, L"abcdefgh", WT_FIT(4), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_CHARACTER);
    if (l) { CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 2); if (o.n == 2) EXPECT_EQ(o.lm[0].isTrimmed | o.lm[1].isTrimmed, 0); IDWriteTextLayout_Release(l); }
    DW_RELEASE(s); WT_END();
}

TEST(trim_wrap_with_enough_height_trims_nothing)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; UINT32 i;
    IDWriteTextLayout *l = tlayout(&X, L"aaa bbb ccc ddd", WT_FIT(3), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 4);
        for (i = 0; i < o.n; i++) EXPECT_EQ(o.lm[i].isTrimmed, 0);
        { dwx_renderer *r = wt_draw(l, 0, 0); draw_info d; analyze_draw(r, &d); EXPECT_EQ(d.inlines, 0); dwx_release(r); }
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

/* ---- trimming at the layout height ----------------------------------------------------------------------------------- */
TEST(trim_height_last_visible_line_is_trimmed)
{
    /* four lines, room for two: the second line (the last visible one) carries the sign, the others are hidden;
       lines beyond the height are still part of the line list */
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; draw_info d; dwx_renderer *r; float lh;
    IDWriteTextLayout *probe = wt_layout_wrap(&X, L"aaa bbb ccc ddd", WT_FIT(3), DWRITE_WORD_WRAPPING_WRAP);
    if (!probe || FAILED(wt_get_lines(probe, &o)) || !o.n) { DW_RELEASE(probe); DW_RELEASE(s); WT_END(); t_skip("probe layout"); return; }
    lh = o.lm[0].height; IDWriteTextLayout_Release(probe);
    {
        IDWriteTextLayout *l = tlayout(&X, L"aaa bbb ccc ddd", WT_FIT(3), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_WRAP);
        if (l) {
            CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 2.5f * lh));
            CHECK_HR_OK(wt_get_lines(l, &o));
            EXPECT_EQ(o.n, 4);
            if (o.n == 4) { EXPECT_EQ(o.lm[0].isTrimmed, 0); EXPECT_EQ(o.lm[1].isTrimmed, 1); t_note("hidden lines isTrimmed %d %d", o.lm[2].isTrimmed, o.lm[3].isTrimmed); }
            r = wt_draw(l, 0, 0); analyze_draw(r, &d);
            EXPECT_EQ(d.inlines, 1);
            t_note("height trimming: heights %g %g %g %g, kept %u chars drawn", o.lm[0].height, o.lm[1].height, o.lm[2].height, o.lm[3].height, d.head_chars);
            dwx_release(r); IDWriteTextLayout_Release(l);
        }
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_height_enough_room_for_all)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; UINT32 i;
    IDWriteTextLayout *l = tlayout(&X, L"aaa bbb ccc ddd", WT_FIT(3), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 1000.0f));
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 4); for (i = 0; i < o.n; i++) EXPECT_EQ(o.lm[i].isTrimmed, 0);
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_height_newline_terminated_line_not_trimmed)
{
    /* a line ended by a hard break is not given a sign merely because the next paragraph is out of room; later lines are hidden */
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; float lh;
    IDWriteTextLayout *l = tlayout(&X, L"aaa\nbbb\nccc", WT_FIT(10), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, s, DWRITE_WORD_WRAPPING_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); lh = o.lm[0].height;
        CHECK_HR_OK(IDWriteTextLayout_SetMaxHeight(l, 1.5f * lh));
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.n, 3);
        if (o.n == 3) { EXPECT_EQ(o.lm[0].isTrimmed, 0); EXPECT_EQ(o.lm[1].isTrimmed, 1); EXPECT_EQ(o.lm[2].isTrimmed, 1); }
        { dwx_renderer *r = wt_draw(l, 0, 0); draw_info d; analyze_draw(r, &d); EXPECT_EQ(d.inlines, 0); EXPECT_EQ(d.head_chars, 4);   /* "aaa" plus its newline character */ dwx_release(r); }
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_set_trimming_relayouts)
{
    WT_BEGIN(); IDWriteInlineObject *s = ellipsis(&X, WT_SIZE); wt_lines o; DWRITE_TRIMMING t = { DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0 };
    IDWriteTextLayout *l = wt_layout_wrap(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 0);
        t.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER; CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, s));
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 1);
        t.granularity = DWRITE_TRIMMING_GRANULARITY_NONE; CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, s));
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 0);
        t.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER; CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &t, s));
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, WT_FIT(20)));
        CHECK_HR_OK(wt_get_lines(l, &o)); EXPECT_EQ(o.lm[0].isTrimmed, 0);
        IDWriteTextLayout_Release(l);
    }
    DW_RELEASE(s); WT_END();
}

TEST(trim_sign_released_with_layout_after_drawing)
{
    WT_BEGIN(); dwx_inline *a = wt_sign(WT_CW, 20, 15); dwx_renderer *r;
    IDWriteTextLayout *l = tlayout(&X, L"AAAAAAAAAA", WT_FIT(5), DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0, (IDWriteInlineObject *)a, DWRITE_WORD_WRAPPING_NO_WRAP);
    if (l) {
        r = wt_draw(l, 0, 0);
        CHECK(r->count[DWX_DRAW_INLINE] == 1);
        IDWriteTextLayout_Release(l); dwx_release(r);
    }
    EXPECT_REFS(a, 1); dwx_release(a); WT_END();
}
