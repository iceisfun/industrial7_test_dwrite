/* IDWriteInlineObject::GetBreakConditions and IDWriteInlineObject::GetOverhangMetrics as called back by IDWriteTextLayout.
 *
 * Win10 dwrite.dll (TextLayout::SlowPath !0x573f8 -> ApplyInlineObjectProperties !0x6ece0; TextLayout::GetBlackBox !0x96234,
 * TextLayout::GetOverhangMetrics !0x96188, DWriteTextLayout::SetInlineObject !0xdf290):
 *   - SetInlineObject only stores the object (overflow of start + length -> E_INVALIDARG); no callback happens until the layout is computed
 *     (the first metrics / line / cluster / hit-test query or Draw).
 *   - GetBreakConditions is called once per maximal run of adjacent text spans that share the same object pointer, with the values the line
 *     breaker currently holds: *before = the break-after condition of the character in front of the run (CAN_BREAK = 1 at the start of
 *     the text) and *after = the break-after condition of the last character of the run. A returned value replaces the held one only when it
 *     is numerically LARGER (NEUTRAL 0 < CAN_BREAK 1 < MAY_NOT_BREAK 2 < MUST_BREAK 3), only if the call returned a non-negative HRESULT (S_FALSE
 *     counts as success; failures are swallowed, they never fail the layout). Characters inside the run get MAY_NOT_BREAK.
 *   - GetOverhangMetrics of the object is queried by IDWriteTextLayout::GetOverhangMetrics once per laid-out span holding the object, its HRESULT
 *     is ignored (the struct is zero before the call). The black box of the object is its metrics box grown by the overhang; the layout reports
 *     the distance of the union of all black boxes to the edges of the layout box (left/top negated, right/bottom measured against the maximum
 *     width / height). The result is cached until the layout changes.
 */
#include "g1.h"

static HRESULT setio(IDWriteTextLayout *l, IDWriteInlineObject *o, UINT32 start, UINT32 len)
{
    DWRITE_TEXT_RANGE r; r.startPosition = start; r.length = len;
    return IDWriteTextLayout_SetInlineObject(l, o, r);
}
static void force(IDWriteTextLayout *l) { DWRITE_TEXT_METRICS m; IDWriteTextLayout_GetMetrics(l, &m); }
#define BC_NEUTRAL  DWRITE_BREAK_CONDITION_NEUTRAL
#define BC_CAN      DWRITE_BREAK_CONDITION_CAN_BREAK
#define BC_MAYNOT   DWRITE_BREAK_CONDITION_MAY_NOT_BREAK
#define BC_MUST     DWRITE_BREAK_CONDITION_MUST_BREAK

#define OBJ_BEGIN(text, width) G1_BEGIN(); \
    IDWriteTextLayout *l = g1_layout(&X, (text), (width), G1_BIG, G1_SIZE); \
    if (!l) { G1_END(); t_skip("CreateTextLayout failed"); return; }
#define OBJ_END() do { DW_RELEASE(l); G1_END(); } while (0)

static g1_io *attach(IDWriteTextLayout *l, UINT32 start, UINT32 len, float w)
{
    g1_io *o = g1_io_new(w, 64.0f, 48.0f);
    CHECK_HR_OK(setio(l, G1_IO(o), start, len));
    return o;
}
static g1_io *attach_break(IDWriteTextLayout *l, UINT32 start, UINT32 len, float w, DWRITE_BREAK_CONDITION b, DWRITE_BREAK_CONDITION a)
{
    g1_io *o = attach(l, start, len, w);
    o->write_break = 1; o->out_before = b; o->out_after = a;
    return o;
}
static void drop(g1_io *o) { IDWriteInlineObject_Release(G1_IO(o)); }

/* ===== when the callbacks happen =================================================================================== */
TEST(gap1_io_setinlineobject_makes_no_callbacks)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f);
    EXPECT_EQ(o->n_break, 0); EXPECT_EQ(o->n_metrics, 0); EXPECT_EQ(o->n_overhang, 0); EXPECT_EQ(o->n_draw, 0);
    drop(o); OBJ_END();
}
TEST(gap1_io_first_layout_query_calls_break_conditions_once)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f);
    force(l);
    EXPECT_EQ(o->n_break, 1); CHECK(o->n_metrics >= 1);
    drop(o); OBJ_END();
}
TEST(gap1_io_repeated_queries_do_not_repeat_break_conditions)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f); UINT32 n = 0; int i;
    force(l);
    for (i = 0; i < 5; i++) {
        DWRITE_CLUSTER_METRICS cm[8]; g1_lines ln; float x, y; DWRITE_HIT_TEST_METRICS hm;
        force(l);
        IDWriteTextLayout_GetClusterMetrics(l, cm, 8, &n);
        g1_get_lines(l, &ln);
        IDWriteTextLayout_HitTestTextPosition(l, 2, FALSE, &x, &y, &hm);
    }
    EXPECT_EQ(o->n_break, 1);
    drop(o); OBJ_END();
}
TEST(gap1_io_draw_triggers_layout_callbacks)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f); dwx_renderer *r = dwx_renderer_new();
    CHECK_HR_OK(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0, 0));
    EXPECT_EQ(o->n_break, 1);       /* the stock renderer does not forward DrawInlineObject to the object, so n_draw stays 0 */
    t_note("Draw: n_draw %ld", (long)o->n_draw);
    dwx_release(r); drop(o); OBJ_END();
}
TEST(gap1_io_line_metrics_query_triggers_callbacks)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f); g1_lines ln;
    CHECK_HR_OK(g1_get_lines(l, &ln));
    EXPECT_EQ(o->n_break, 1);
    drop(o); OBJ_END();
}
TEST(gap1_io_overhang_query_triggers_layout)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f); DWRITE_OVERHANG_METRICS om;
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    EXPECT_EQ(o->n_break, 1);
    EXPECT_EQ(o->n_overhang, 1);          /* one laid-out span holds the object: one black box query */
    drop(o); OBJ_END();
}
TEST(gap1_io_metrics_query_alone_does_not_ask_for_overhang)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f);
    force(l);
    if (o->n_overhang) t_note("GetMetrics queried the object overhang %ld time(s)", o->n_overhang);
    drop(o); OBJ_END();
}
TEST(gap1_io_object_replacing_range_changes_nothing_before_layout)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *a = attach(l, 2, 1, 31.25f), *b = attach(l, 2, 1, 62.5f);
    EXPECT_EQ(a->n_break + a->n_metrics, 0); EXPECT_EQ(b->n_break + b->n_metrics, 0);
    force(l);
    EXPECT_EQ(a->n_break, 0); EXPECT_EQ(b->n_break, 1);       /* the replaced object is never consulted */
    EXPECT_EQ(a->n_metrics, 0);
    drop(a); drop(b); OBJ_END();
}
TEST(gap1_io_removed_object_not_consulted)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f);
    CHECK_HR_OK(setio(l, NULL, 2, 1));
    force(l);
    EXPECT_EQ(o->n_break, 0); EXPECT_EQ(o->n_metrics, 0);
    drop(o); OBJ_END();
}

/* ===== how runs map to callbacks =================================================================================== */
TEST(gap1_io_long_range_is_one_call)
{
    OBJ_BEGIN(L"aabbbbcc", 1000.0f);
    g1_io *o = attach(l, 2, 4, 62.5f);
    force(l);
    EXPECT_EQ(o->n_break, 1);
    drop(o); OBJ_END();
}
TEST(gap1_io_same_object_adjacent_ranges_merge)
{
    OBJ_BEGIN(L"aabbcc", 1000.0f);
    g1_io *o = g1_io_new(31.25f, 64, 48);
    CHECK_HR_OK(setio(l, G1_IO(o), 2, 1));
    CHECK_HR_OK(setio(l, G1_IO(o), 3, 1));
    force(l);
    EXPECT_EQ(o->n_break, 1);
    drop(o); OBJ_END();
}
TEST(gap1_io_same_object_separate_ranges_two_calls)
{
    OBJ_BEGIN(L"aabbcc", 1000.0f);
    g1_io *o = g1_io_new(31.25f, 64, 48);
    CHECK_HR_OK(setio(l, G1_IO(o), 1, 1));
    CHECK_HR_OK(setio(l, G1_IO(o), 4, 1));
    force(l);
    EXPECT_EQ(o->n_break, 2);
    drop(o); OBJ_END();
}
TEST(gap1_io_adjacent_different_objects_one_call_each)
{
    OBJ_BEGIN(L"aabbcc", 1000.0f);
    g1_io *a = attach(l, 2, 1, 31.25f), *b = attach(l, 3, 1, 31.25f);
    force(l);
    EXPECT_EQ(a->n_break, 1); EXPECT_EQ(b->n_break, 1);
    drop(a); drop(b); OBJ_END();
}
TEST(gap1_io_object_at_text_start_gets_can_break_before)
{
    OBJ_BEGIN(L"Xabc", 1000.0f);
    g1_io *o = attach(l, 0, 1, 31.25f);
    force(l);
    EXPECT_EQ(o->n_break, 1);
    if (o->nlog > 0) EXPECT_EQ(o->in_before[0], BC_CAN);     /* nothing precedes the run: CAN_BREAK is passed in */
    drop(o); OBJ_END();
}
TEST(gap1_io_incoming_values_are_valid_conditions)
{
    static const struct { const WCHAR *t; UINT32 s, n; } rows[] = {
        { L"abXcd", 2, 1 }, { L"ab Xcd", 3, 1 }, { L"abc X", 4, 1 }, { L"ab XY cd", 3, 2 }, { L"abcXd", 3, 1 }, { L"Xabcd", 0, 1 }, { L"abcdX", 4, 1 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        OBJ_BEGIN(rows[i].t, 1000.0f);
        g1_io *o = attach(l, rows[i].s, rows[i].n, 31.25f * rows[i].n);
        force(l);
        EXPECT_EQ(o->n_break, 1);
        if (o->nlog > 0) {
            CHECKF((unsigned)o->in_before[0] <= 3, "row %u before %d", i, o->in_before[0]);
            CHECKF((unsigned)o->in_after[0] <= 3, "row %u after %d", i, o->in_after[0]);
        }
        drop(o); OBJ_END();
    }
}
TEST(gap1_io_incoming_values_follow_the_line_breaker_note)
{
    /* The values passed in are the break-after conditions of the neighbouring characters as the line breaker holds them; compare with a plain
     * analysis of the same text. */
    static const struct { const WCHAR *t; UINT32 s, n; } rows[] = {
        { L"abXcd", 2, 1 }, { L"ab Xcd", 3, 1 }, { L"abc X", 4, 1 }, { L"ab XY cd", 3, 2 }, { L"abcdX", 4, 1 }, { L"aa bb cc", 3, 2 },
    };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        OBJ_BEGIN(rows[i].t, 1000.0f);
        DWRITE_LINE_BREAKPOINT bp[64]; UINT32 len = (UINT32)wcslen(rows[i].t);
        g1_io *o = attach(l, rows[i].s, rows[i].n, 31.25f * rows[i].n);
        force(l);
        if (o->nlog > 0 && g1_analyze_breaks(f, rows[i].t, len, bp)) {
            UINT32 last = rows[i].s + rows[i].n - 1;
            DWRITE_BREAK_CONDITION wb = rows[i].s ? (DWRITE_BREAK_CONDITION)bp[rows[i].s - 1].breakConditionAfter : BC_CAN;
            t_note("\"%ls\" object [%u,%u): in before %d (analyzer %d) in after %d (analyzer %d)", rows[i].t, rows[i].s, rows[i].n,
                   o->in_before[0], wb, o->in_after[0], bp[last].breakConditionAfter);
        }
        drop(o); OBJ_END();
    }
}

/* ===== effect of the returned conditions on line breaking ============================================================= */
TEST(gap1_io_neutral_object_layout_baseline)
{
    /* "aa " + 2-character object + " cc" at 4.5 characters per line breaks in front of the object and after the space behind it */
    OBJ_BEGIN(L"aa bb cc", G1_FIT(4));
    g1_io *o = attach(l, 3, 2, G1_CH(2));
    G1_LENS(l, 3, 3, 2);
    drop(o); OBJ_END();
}
TEST(gap1_io_may_not_break_before_suppresses_legal_break)
{
    OBJ_BEGIN(L"aa bb cc", G1_FIT(4));
    g1_io *o = attach_break(l, 3, 2, G1_CH(2), BC_MAYNOT, BC_NEUTRAL);
    g1_lines ln; CHECK_HR_OK(g1_get_lines(l, &ln));
    t_note("n_break %ld", (long)o->n_break);
    /* the break in front of the object is legal (a space precedes it): the held value is below MAY_NOT_BREAK, so the escalation applies */
    t_note("MAY_NOT_BREAK before the object at a legal boundary (held %d): lines {%s}", o->nlog ? o->in_before[0] : -1, g1_lens_str(l));
    drop(o); OBJ_END();
}
TEST(gap1_io_may_not_break_before_exact_lines)
{
    OBJ_BEGIN(L"aa bb cc", G1_FIT(4));
    g1_io *o = attach_break(l, 3, 2, G1_CH(2), BC_MAYNOT, BC_NEUTRAL);
    force(l);
    /* Win10 keeps the break in front of the object whatever the returned value */
    t_note("held value %d: lines {%s}", o->nlog ? o->in_before[0] : -1, g1_lens_str(l));
    drop(o); OBJ_END();
}
TEST(gap1_io_may_not_break_after_suppresses_legal_break)
{
    /* "aa bb cc": object over "bb"; the break behind the following space is not touched, but a MAY_NOT_BREAK after the object's last character
     * escalates that character's condition; the space still ends the line */
    OBJ_BEGIN(L"aa bb cc", G1_FIT(4));
    g1_io *o = attach_break(l, 3, 2, G1_CH(2), BC_NEUTRAL, BC_MAYNOT);
    t_note("n_break %ld", (long)o->n_break);
    t_note("MAY_NOT_BREAK after the object: lines {%s}", g1_lens_str(l));
    drop(o); OBJ_END();
}
TEST(gap1_io_must_break_after_forces_line_end)
{
    /* the break condition after the last character of the run becomes MUST_BREAK (3 is larger than whatever the line breaker held) */
    OBJ_BEGIN(L"aaaXbbb", 1000.0f);
    g1_io *o = attach_break(l, 3, 1, G1_CW, BC_NEUTRAL, BC_MUST);
    G1_LENS(l, 4, 3);
    drop(o); OBJ_END();
}
TEST(gap1_io_must_break_after_in_the_middle_of_a_word_with_narrow_width)
{
    OBJ_BEGIN(L"aaaXbbb", G1_FIT(2));
    g1_io *o = attach_break(l, 3, 1, G1_CW, BC_NEUTRAL, BC_MUST);
    t_note("lines {%s}", g1_lens_str(l));
    {
        g1_lines ln; UINT32 i, pos = 0, brk_at_4 = 0;
        g1_get_lines(l, &ln);
        for (i = 0; i < ln.n; i++) { pos += ln.lm[i].length; if (pos == 4) brk_at_4 = 1; }
        CHECK(brk_at_4);       /* a line ends right behind the object whatever the width is */
    }
    drop(o); OBJ_END();
}
TEST(gap1_io_must_break_before_note)
{
    OBJ_BEGIN(L"aaaXbbb", 1000.0f);
    g1_io *o = attach_break(l, 3, 1, G1_CW, BC_MUST, BC_NEUTRAL);
    t_note("n_break %ld", (long)o->n_break);
    t_note("MUST_BREAK before the object inside a word: lines {%s}", g1_lens_str(l));
    drop(o); OBJ_END();
}
TEST(gap1_io_must_break_at_both_edges_note)
{
    OBJ_BEGIN(L"aaaXbbb", 1000.0f);
    g1_io *o = attach_break(l, 3, 1, G1_CW, BC_MUST, BC_MUST);
    t_note("MUST_BREAK on both edges: lines {%s}", g1_lens_str(l));
    {
        g1_lines ln; g1_get_lines(l, &ln);
        CHECK(ln.n >= 2);          /* the edge behind the object is a forced break (see must_break_after) */
    }
    drop(o); OBJ_END();
}
TEST(gap1_io_conditions_cannot_be_lowered_inside_a_word)
{
    /* inside a word the line breaker forbids breaks (MAY_NOT_BREAK, 2); NEUTRAL (0) and CAN_BREAK (1) from the object are smaller and ignored */
    static const DWRITE_BREAK_CONDITION vals[] = { BC_NEUTRAL, BC_CAN };
    unsigned i;
    for (i = 0; i < 2; i++) {
        OBJ_BEGIN(L"aaaXbbb", 1000.0f);
        g1_io *o = attach_break(l, 3, 1, G1_CW, vals[i], vals[i]);
        force(l);
        if (o->nlog > 0 && o->in_before[0] == BC_MAYNOT && o->in_after[0] == BC_MAYNOT) { G1_LENS(l, 7); }
        else t_note("held values %d/%d inside the word; lines {%s}", o->nlog ? o->in_before[0] : -1, o->nlog ? o->in_after[0] : -1, g1_lens_str(l));
        drop(o); OBJ_END();
    }
}
TEST(gap1_io_failure_hresult_discards_outputs)
{
    static const HRESULT bad[] = { E_FAIL, E_NOTIMPL, E_INVALIDARG, E_OUTOFMEMORY, (HRESULT)0x80041234, (HRESULT)0x88985002 };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        OBJ_BEGIN(L"aaaXbbb", 1000.0f);
        g1_io *o = attach_break(l, 3, 1, G1_CW, BC_MUST, BC_MUST);
        DWRITE_TEXT_METRICS m;
        o->hr_break = bad[i]; o->write_on_fail = 1;
        CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));         /* the failure never reaches the caller */
        EXPECT_EQ(o->n_break, 1);
        G1_LENS(l, 7);                                            /* the MUST_BREAK values that were written are not applied */
        drop(o); OBJ_END();
    }
}
TEST(gap1_io_success_codes_apply_outputs)
{
    static const HRESULT ok[] = { S_OK, S_FALSE, (HRESULT)1, (HRESULT)0x00041234 };
    unsigned i;
    for (i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        OBJ_BEGIN(L"aaaXbbb", 1000.0f);
        g1_io *o = attach_break(l, 3, 1, G1_CW, BC_NEUTRAL, BC_MUST);
        o->hr_break = ok[i];
        G1_LENS(l, 4, 3);                                         /* any non-negative HRESULT counts as success */
        drop(o); OBJ_END();
    }
}
TEST(gap1_io_out_of_range_values_are_masked_note)
{
    /* values outside the enumeration are written into a 2-bit field */
    static const int vals[] = { 4, 5, 6, 7, 0x103, -1 };
    unsigned i;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        OBJ_BEGIN(L"aaaXbbb", 1000.0f);
        g1_io *o = attach_break(l, 3, 1, G1_CW, BC_NEUTRAL, (DWRITE_BREAK_CONDITION)vals[i]);
        force(l);
        EXPECT_EQ(o->n_break, 1);
        t_note("after = %d: lines {%s}", vals[i], g1_lens_str(l));
        drop(o); OBJ_END();
    }
}
TEST(gap1_io_failed_call_still_marks_interior_no_break)
{
    /* characters inside the object's range never take a break, even when the callback failed */
    OBJ_BEGIN(L"aa bcd ee", G1_FIT(3));
    g1_io *o = attach(l, 3, 3, G1_CH(3));
    g1_lines ln; UINT32 i, pos = 0;
    o->hr_break = E_FAIL;
    CHECK_HR_OK(g1_get_lines(l, &ln));
    for (i = 0; i < ln.n; i++) { pos += ln.lm[i].length; CHECKF(!(pos > 3 && pos < 6), "line ends inside the object range at %u", pos); }
    drop(o); OBJ_END();
}
TEST(gap1_io_lines_never_split_the_object_range)
{
    static const struct { const WCHAR *t; UINT32 s, n; } rows[] = { { L"aa b c dd", 3, 3 }, { L"aaa bbb ccc", 4, 5 }, { L"x yy zz w", 2, 4 } };
    unsigned i;
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        OBJ_BEGIN(rows[i].t, G1_FIT(2));
        g1_io *o = attach(l, rows[i].s, rows[i].n, G1_CH(rows[i].n));
        g1_lines ln; UINT32 j, pos = 0;
        CHECK_HR_OK(g1_get_lines(l, &ln));
        for (j = 0; j < ln.n; j++) { pos += ln.lm[j].length; CHECKF(!(pos > rows[i].s && pos < rows[i].s + rows[i].n), "row %u: line ends inside the object range at %u", i, pos); }
        drop(o); OBJ_END();
    }
}
TEST(gap1_io_conditions_apply_per_run)
{
    /* two separate ranges of one object: each run is asked, each run's own edge conditions apply */
    OBJ_BEGIN(L"aaXbbYccc", 1000.0f);
    g1_io *o = g1_io_new(G1_CW, 64, 48);
    o->write_break = 1; o->out_before = BC_NEUTRAL; o->out_after = BC_MUST;
    CHECK_HR_OK(setio(l, G1_IO(o), 2, 1));
    CHECK_HR_OK(setio(l, G1_IO(o), 5, 1));
    G1_LENS(l, 3, 3, 3);
    EXPECT_EQ(o->n_break, 2);
    drop(o); OBJ_END();
}
TEST(gap1_io_log_has_one_entry_per_call)
{
    OBJ_BEGIN(L"aaXbbYccc", 1000.0f);
    g1_io *o = g1_io_new(G1_CW, 64, 48);
    CHECK_HR_OK(setio(l, G1_IO(o), 2, 1));
    CHECK_HR_OK(setio(l, G1_IO(o), 5, 1));
    force(l);
    EXPECT_EQ(o->nlog, 2);
    drop(o); OBJ_END();
}

/* ===== SetInlineObject argument handling ================================================================================ */
TEST(gap1_io_set_range_overflow_invalid)
{
    static const DWRITE_TEXT_RANGE bad[] = { { 0xFFFFFFFFu, 1 }, { 0xFFFFFFFFu, 2 }, { 0x80000000u, 0x80000000u }, { 10, 0xFFFFFFFFu }, { 0xFFFFFFF0u, 0x20 } };
    unsigned i;
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = g1_io_new(31.25f, 64, 48);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) CHECK_HR(IDWriteTextLayout_SetInlineObject(l, G1_IO(o), bad[i]), E_INVALIDARG);
    EXPECT_EQ(o->ref, 1);              /* a rejected call keeps no reference */
    drop(o); OBJ_END();
}
TEST(gap1_io_set_empty_range_succeeds)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = g1_io_new(31.25f, 64, 48);
    CHECK_HR_OK(setio(l, G1_IO(o), 0, 0));
    CHECK_HR_OK(setio(l, G1_IO(o), 5, 0));
    CHECK_HR_OK(setio(l, G1_IO(o), 3, 0));
    force(l);
    EXPECT_EQ(o->n_break, 0); EXPECT_EQ(o->n_metrics, 0);
    drop(o); OBJ_END();
}
TEST(gap1_io_set_range_past_text_end_layout_survives)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = g1_io_new(31.25f, 64, 48); DWRITE_TEXT_METRICS m; HRESULT hr;
    hr = setio(l, G1_IO(o), 3, 100);
    t_note("SetInlineObject({3,100}) on 5 characters -> %s", dw_hr_name(hr));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    EXPECT_EQ(m.lineCount >= 1, 1);
    drop(o); OBJ_END();
}
TEST(gap1_io_set_takes_reference_and_layout_release_returns_it)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f);
    CHECK(o->ref >= 2);
    force(l);
    CHECK(o->ref >= 2);
    DW_RELEASE(l);
    EXPECT_EQ(o->ref, 1);
    drop(o); EXPECT_EQ(o->destroyed, 1); G1_END();
}
TEST(gap1_io_replacing_with_null_returns_reference)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f);
    CHECK_HR_OK(setio(l, NULL, 2, 1));
    EXPECT_EQ(o->ref, 1);
    drop(o); OBJ_END();
}
TEST(gap1_io_get_inline_object_roundtrip)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f); IDWriteInlineObject *got = G1_SENT(IDWriteInlineObject *); DWRITE_TEXT_RANGE r = { 99, 99 };
    CHECK_HR_OK(IDWriteTextLayout_GetInlineObject(l, 2, &got, &r));
    EXPECT_EQ(got, o); EXPECT_EQ(r.startPosition, 2); EXPECT_EQ(r.length, 1);
    if (got == (IDWriteInlineObject *)o) IDWriteInlineObject_Release(got);
    got = G1_SENT(IDWriteInlineObject *);
    CHECK_HR_OK(IDWriteTextLayout_GetInlineObject(l, 0, &got, &r));
    EXPECT_EQ(got, NULL); EXPECT_EQ(r.startPosition, 0); CHECK(r.length >= 1 && r.length <= 2);
    drop(o); OBJ_END();
}

/* ===== GetOverhangMetrics =================================================================================================== */
static g1_io *attach_ov(IDWriteTextLayout *l, UINT32 s, UINT32 n, float w, float lo, float to, float ro, float bo)
{
    g1_io *o = attach(l, s, n, w);
    o->overhang.left = lo; o->overhang.top = to; o->overhang.right = ro; o->overhang.bottom = bo;
    return o;
}
static IDWriteTextLayout *mk(g1_fx *x, const WCHAR *t, float w, float h) { return g1_layout(x, t, w, h, G1_SIZE); }

TEST(gap1_ov_single_object_left_and_right)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"X", 100.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("layout"); return; }
    g1_io *o = attach_ov(l, 0, 1, 64.0f, 3, 5, 7, 9);
    memset(&om, 0xcc, sizeof om);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    CHECK_NEAR(om.left, 3.0, 0.01);                 /* the object starts at x = 0 and sticks out 3 to the left */
    CHECK_NEAR(om.right, 64.0 + 7.0 - 100.0, 0.01); /* right edge of the black box minus the layout width */
    t_note("top %g bottom %g (object top offset depends on the line metrics)", om.top, om.bottom);
    drop(o); DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_zero_overhang_object_box_only)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"X", 100.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("layout"); return; }
    g1_io *o = attach_ov(l, 0, 1, 64.0f, 0, 0, 0, 0);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    CHECK_NEAR(om.left, 0.0, 0.01); CHECK_NEAR(om.right, -36.0, 0.01);
    drop(o); DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_left_overhang_sweep)
{
    static const float lo[] = { 0.0f, 1.0f, 17.0f, 100.0f, 1000.0f, -5.0f, -40.0f };
    unsigned i;
    for (i = 0; i < sizeof lo / sizeof lo[0]; i++) {
        G1_BEGIN();
        IDWriteTextLayout *l = mk(&X, L"X", 500.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
        if (!l) { G1_END(); t_skip("layout"); return; }
        g1_io *o = attach_ov(l, 0, 1, 64.0f, lo[i], 0, 0, 0);
        CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
        CHECKF(fabsf(om.left - lo[i]) < 0.01f, "left overhang %g reported as %g", lo[i], om.left);
        drop(o); DW_RELEASE(l); G1_END();
    }
}
TEST(gap1_ov_right_overhang_sweep)
{
    static const float ro[] = { 0.0f, 1.0f, 17.0f, 100.0f, 1000.0f, -5.0f, -40.0f };
    unsigned i;
    for (i = 0; i < sizeof ro / sizeof ro[0]; i++) {
        G1_BEGIN();
        IDWriteTextLayout *l = mk(&X, L"X", 500.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
        if (!l) { G1_END(); t_skip("layout"); return; }
        g1_io *o = attach_ov(l, 0, 1, 64.0f, 0, 0, ro[i], 0);
        CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
        CHECKF(fabsf(om.right - (64.0f + ro[i] - 500.0f)) < 0.01f, "right overhang %g reported as %g", ro[i], om.right);
        drop(o); DW_RELEASE(l); G1_END();
    }
}
TEST(gap1_ov_right_tracks_layout_width)
{
    static const float widths[] = { 10.0f, 64.0f, 100.0f, 250.0f, 4000.0f };
    unsigned i;
    for (i = 0; i < sizeof widths / sizeof widths[0]; i++) {
        G1_BEGIN();
        IDWriteTextLayout *l = mk(&X, L"X", widths[i], 200.0f); DWRITE_OVERHANG_METRICS om;
        if (!l) { G1_END(); t_skip("layout"); return; }
        g1_io *o = attach_ov(l, 0, 1, 64.0f, 0, 0, 8, 0);
        CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
        CHECKF(fabsf(om.right - (72.0f - widths[i])) < 0.01f, "width %g: right %g want %g", widths[i], om.right, 72.0f - widths[i]);
        drop(o); DW_RELEASE(l); G1_END();
    }
}
TEST(gap1_ov_vertical_overhang_moves_top_and_bottom)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"X", 100.0f, 200.0f); DWRITE_OVERHANG_METRICS a, b;
    if (!l) { G1_END(); t_skip("layout"); return; }
    g1_io *o = attach_ov(l, 0, 1, 64.0f, 0, 0, 0, 0), *o2;
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &a));
    o2 = attach_ov(l, 0, 1, 64.0f, 0, 11, 0, 13);          /* same box, taller black box: replacing the object changes the layout */
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &b));
    CHECK_NEAR(b.top - a.top, 11.0, 0.01);
    CHECK_NEAR(b.bottom - a.bottom, 13.0, 0.01);
    CHECK_NEAR(b.left, a.left, 0.01); CHECK_NEAR(b.right, a.right, 0.01);
    drop(o); drop(o2); DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_failure_leaves_zero_overhang)
{
    static const HRESULT bad[] = { E_FAIL, E_NOTIMPL, E_INVALIDARG, (HRESULT)0x80041234 };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        G1_BEGIN();
        IDWriteTextLayout *l = mk(&X, L"X", 100.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
        if (!l) { G1_END(); t_skip("layout"); return; }
        g1_io *o = attach_ov(l, 0, 1, 64.0f, 30, 30, 30, 30);
        o->hr_overhang = bad[i]; o->write_on_fail = 1;
        memset(&om, 0xcc, sizeof om);
        CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));            /* the callback's HRESULT is ignored */
        EXPECT_EQ(o->n_overhang, 1);
        /* Win10 uses the values the callback wrote even when it failed */
        CHECK_NEAR(om.left, 30.0, 0.01); CHECK_NEAR(om.right, 64.0 + 30.0 - 100.0, 0.01);
        drop(o); DW_RELEASE(l); G1_END();
    }
}
TEST(gap1_ov_result_cached_between_calls)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"X", 100.0f, 200.0f); DWRITE_OVERHANG_METRICS a, b; int i;
    if (!l) { G1_END(); t_skip("layout"); return; }
    g1_io *o = attach_ov(l, 0, 1, 64.0f, 3, 5, 7, 9);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &a));
    for (i = 0; i < 4; i++) { CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &b)); CHECK(!memcmp(&a, &b, sizeof a)); }
    EXPECT_EQ(o->n_overhang, 1);
    drop(o); DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_object_replaced_gives_new_values)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"X", 100.0f, 200.0f); DWRITE_OVERHANG_METRICS a, b;
    if (!l) { G1_END(); t_skip("layout"); return; }
    g1_io *o1 = attach_ov(l, 0, 1, 64.0f, 3, 0, 0, 0);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &a));
    g1_io *o2 = attach_ov(l, 0, 1, 64.0f, 21, 0, 0, 0);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &b));
    CHECK_NEAR(a.left, 3.0, 0.01); CHECK_NEAR(b.left, 21.0, 0.01);
    EXPECT_EQ(o1->n_overhang, 1); EXPECT_EQ(o2->n_overhang, 1);
    drop(o1); drop(o2); DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_two_objects_union)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"XY", 300.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("layout"); return; }
    g1_io *a = attach_ov(l, 0, 1, 64.0f, 2, 0, 500, 0), *b = attach_ov(l, 1, 1, 64.0f, 200, 0, 3, 0);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    /* object b starts at x = 64 and reaches 200 to the left: -136 relative to the layout; object a reaches 2 to the left */
    CHECK_NEAR(om.left, 136.0, 0.01);
    CHECK_NEAR(om.right, 64.0 + 500.0 - 300.0, 0.01);
    EXPECT_EQ(a->n_overhang, 1); EXPECT_EQ(b->n_overhang, 1);
    drop(a); drop(b); DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_object_positions_follow_preceding_text)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"abX", 300.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("layout"); return; }
    g1_io *o = attach_ov(l, 2, 1, 64.0f, 100, 0, 0, 0);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    CHECK_NEAR(om.left, 100.0 - 2 * G1_CW, 0.51);           /* the object starts behind two 31.25 wide characters */
    drop(o); DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_negative_inset_object_inside_box)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"X", 300.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("layout"); return; }
    g1_io *o = attach_ov(l, 0, 1, 64.0f, -10, -10, -10, -10);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    CHECK_NEAR(om.left, -10.0, 0.01); CHECK_NEAR(om.right, 54.0 - 300.0, 0.01);
    drop(o); DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_null_pointer_invalid)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"abc", 300.0f, 200.0f);
    if (!l) { G1_END(); t_skip("layout"); return; }
    if (t_is_wine()) { DW_RELEASE(l); G1_END(); t_skip("emulator dereferences a NULL out pointer"); return; }
    CHECK_HR(IDWriteTextLayout_GetOverhangMetrics(l, NULL), E_INVALIDARG);
    DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_plain_text_ink_extent)
{
    /* glyph boxes of the fixture: x 100..900 of 1000 design units advance, y 0..1400 above the baseline (ascent 1800 / 2048 em) */
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"ab", 200.0f, 200.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    CHECK_NEAR(om.left, -100.0 * 64.0 / 2048.0, 0.51);
    CHECK_NEAR(om.right, (31.25 + 900.0 * 64.0 / 2048.0) - 200.0, 0.51);
    t_note("plain text overhang: left %g top %g right %g bottom %g", om.left, om.top, om.right, om.bottom);
    DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_plain_text_is_inside_the_box_when_it_fits)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"abcd", 500.0f, 500.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    CHECK(om.left <= 0.0f); CHECK(om.right <= 0.0f); CHECK(om.top <= 0.0f); CHECK(om.bottom <= 0.0f);
    DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_text_overflowing_box_reports_positive_right)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"abcdefgh", 100.0f, 500.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("layout"); return; }
    IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    CHECK(om.right > 0.0f);                   /* 8 characters of 31.25 cannot fit in 100 */
    DW_RELEASE(l); G1_END();
}
TEST(gap1_ov_empty_text_layout)
{
    G1_BEGIN();
    IDWriteTextLayout *l = mk(&X, L"", 100.0f, 50.0f); DWRITE_OVERHANG_METRICS om;
    if (!l) { G1_END(); t_skip("empty layout unavailable"); return; }
    memset(&om, 0xcc, sizeof om);
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &om));
    CHECK(om.left == om.left && om.right == om.right && om.top == om.top && om.bottom == om.bottom);
    t_note("empty layout overhang: left %g top %g right %g bottom %g", om.left, om.top, om.right, om.bottom);
    DW_RELEASE(l); G1_END();
}

/* ===== objects that are not asked for overhang ========================================================================================== */
TEST(gap1_ov_layout_without_query_never_asks_object)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f); g1_lines ln; DWRITE_CLUSTER_METRICS cm[8]; UINT32 n;
    force(l); g1_get_lines(l, &ln); IDWriteTextLayout_GetClusterMetrics(l, cm, 8, &n);
    if (o->n_overhang) t_note("object overhang requested %ld time(s) without GetOverhangMetrics", o->n_overhang);
    drop(o); OBJ_END();
}
TEST(gap1_ov_object_metrics_failure_swallowed)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 31.25f); DWRITE_TEXT_METRICS m;
    o->hr_metrics = E_FAIL;
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));         /* metrics failure is replaced by a zero-size object */
    CHECK(o->n_metrics >= 1);
    drop(o); OBJ_END();
}
TEST(gap1_ov_object_metrics_failure_zero_size)
{
    OBJ_BEGIN(L"abXcd", 1000.0f);
    g1_io *o = attach(l, 2, 1, 62.5f), *o2; DWRITE_TEXT_METRICS a, b;
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &a));
    o2 = attach(l, 2, 1, 62.5f); o2->hr_metrics = E_FAIL;
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &b));
    CHECK_NEAR(a.width - b.width, 62.5, 0.01);                /* a failing object contributes width 0 */
    drop(o); drop(o2); OBJ_END();
}
