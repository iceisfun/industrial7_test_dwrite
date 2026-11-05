/* Range semantics shared by every per-range property (table driven over tlr_props).
 *
 * The run table behind all Set and Get methods (RunIndexer<RunFormat> / RunVector<RunFormat>) is independent of the text length: a Set stores
 * its range exactly as given (no clipping to the text, no validation of the start), runs whose complete formats are equal are merged, and
 * every Get reports the run containing the position. The text used here is 10 characters long.
 */
#include "tlr.h"

#define T10 L"abcdefghij"

/* Value + range expectation for one position. */
#define EXPECT_AT(P, POS, V, S, L) do { DWRITE_TEXT_RANGE r_ = POISON_RANGE; int v_ = (P)->get(&E, (POS), &r_); \
    t_check(v_ == (V), __FILE__, __LINE__, (P)->name, "value at %u: got %d want %d", (unsigned)(POS), v_, (int)(V)); \
    t_check(r_.startPosition == (UINT32)(S) && r_.length == (UINT32)(L), __FILE__, __LINE__, (P)->name, "range at %u: got {%u,%u} want {%u,%u}", \
            (unsigned)(POS), (unsigned)r_.startPosition, (unsigned)r_.length, (unsigned)(UINT32)(S), (unsigned)(UINT32)(L)); } while (0)
#define SET_OK(P, V, S, L) do { HRESULT hr_ = (P)->set(&E, (V), RNG((S), (L))); \
    t_check(hr_ == S_OK, __FILE__, __LINE__, (P)->name, "set(%d, {%u,%u}) -> %s", (int)(V), (unsigned)(UINT32)(S), (unsigned)(UINT32)(L), dw_hr_name(hr_)); } while (0)
#define SET_BAD(P, V, S, L) do { HRESULT hr_ = (P)->set(&E, (V), RNG((S), (L))); \
    t_check(hr_ == TLR_EINVALIDARG, __FILE__, __LINE__, (P)->name, "set(%d, {%u,%u}) -> %s want E_INVALIDARG", (int)(V), (unsigned)(UINT32)(S), (unsigned)(UINT32)(L), dw_hr_name(hr_)); } while (0)

/* skip a property whose interface is not available (IDWriteTextLayout1 exists on every Windows 8+ system) */
/* Properties share one run table, so every property starts from a freshly created layout. */
#define SKIP_NO_L1(P) if (!E.l1 && (!strcmp((P)->name, "charspacing") || !strcmp((P)->name, "pairkerning"))) continue; \
    if (!tlr_env_layout(&E, tlr_cur_text)) { t_check(0, __FILE__, __LINE__, "recreate layout", "failed"); break; }

TEST(gen_set_inside_text)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 3);
        EXPECT_AT(p, 0, 0, 0, 2); EXPECT_AT(p, 1, 0, 0, 2);
        EXPECT_AT(p, 2, 1, 2, 3); EXPECT_AT(p, 3, 1, 2, 3); EXPECT_AT(p, 4, 1, 2, 3);
        EXPECT_AT(p, 5, 0, 5, A(5)); EXPECT_AT(p, 9, 0, 5, A(5));
    }
    TLR_ENV_END();
}
TEST(gen_positions_past_text_report_tail_run)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 3);
        EXPECT_AT(p, 10, 0, 5, A(5)); EXPECT_AT(p, 11, 0, 5, A(5)); EXPECT_AT(p, 1000, 0, 5, A(5));
        EXPECT_AT(p, 0x7FFFFFFFu, 0, 5, A(5)); EXPECT_AT(p, 0xFFFFFFFEu, 0, 5, A(5));
        if (!t_is_wine()) EXPECT_AT(p, 0xFFFFFFFFu, 0, 5, A(5));
    }
    TLR_ENV_END();
}
TEST(gen_set_at_start)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 0, 4);
        EXPECT_AT(p, 0, 1, 0, 4); EXPECT_AT(p, 3, 1, 0, 4); EXPECT_AT(p, 4, 0, 4, A(4)); EXPECT_AT(p, 9, 0, 4, A(4));
    }
    TLR_ENV_END();
}
TEST(gen_set_whole_text)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 0, 10);
        EXPECT_AT(p, 0, 1, 0, 10); EXPECT_AT(p, 9, 1, 0, 10);
        EXPECT_AT(p, 10, 0, 10, A(10)); EXPECT_AT(p, 500, 0, 10, A(10));
    }
    TLR_ENV_END();
}
TEST(gen_set_last_character)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 9, 1);
        EXPECT_AT(p, 8, 0, 0, 9); EXPECT_AT(p, 9, 1, 9, 1); EXPECT_AT(p, 10, 0, 10, A(10));
    }
    TLR_ENV_END();
}
TEST(gen_length_zero_is_noop)
{
    const tlr_prop *p; static const UINT32 starts[] = { 0, 3, 10, 11, 1000, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu }; int i;
    TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        for (i = 0; i < 8; i++) {
            SET_OK(p, 1, starts[i], 0);
            EXPECT_AT(p, 0, 0, 0, TLR_ALL); EXPECT_AT(p, 3, 0, 0, TLR_ALL); EXPECT_AT(p, 9, 0, 0, TLR_ALL);
        }
    }
    TLR_ENV_END();
}
TEST(gen_length_zero_inside_existing_run_changes_nothing)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 5);
        SET_OK(p, 2, 4, 0);        /* inside the run: nothing may split it */
        SET_OK(p, 0, 2, 0);
        EXPECT_AT(p, 2, 1, 2, 5); EXPECT_AT(p, 4, 1, 2, 5); EXPECT_AT(p, 6, 1, 2, 5);
    }
    TLR_ENV_END();
}
TEST(gen_range_exceeding_text_is_not_clipped)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 5, 100);          /* text has 10 characters; the stored range is {5,100} */
        EXPECT_AT(p, 4, 0, 0, 5);
        EXPECT_AT(p, 5, 1, 5, 100); EXPECT_AT(p, 9, 1, 5, 100); EXPECT_AT(p, 10, 1, 5, 100); EXPECT_AT(p, 104, 1, 5, 100);
        EXPECT_AT(p, 105, 0, 105, A(105)); EXPECT_AT(p, 100000, 0, 105, A(105));
    }
    TLR_ENV_END();
}
TEST(gen_range_starting_past_text)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 50, 5);
        EXPECT_AT(p, 0, 0, 0, 50); EXPECT_AT(p, 9, 0, 0, 50); EXPECT_AT(p, 49, 0, 0, 50);
        EXPECT_AT(p, 50, 1, 50, 5); EXPECT_AT(p, 54, 1, 50, 5); EXPECT_AT(p, 55, 0, 55, A(55));
    }
    TLR_ENV_END();
}
TEST(gen_range_starting_exactly_at_text_end)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 10, 3);
        EXPECT_AT(p, 9, 0, 0, 10); EXPECT_AT(p, 10, 1, 10, 3); EXPECT_AT(p, 12, 1, 10, 3); EXPECT_AT(p, 13, 0, 13, A(13));
    }
    TLR_ENV_END();
}
TEST(gen_whole_space_range_0_to_max)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        DWRITE_TEXT_RANGE r = POISON_RANGE; int v;
        SKIP_NO_L1(p);
        SET_OK(p, 1, 0, TLR_ALL);          /* start + length == 0xFFFFFFFF does not wrap */
        v = p->get(&E, 0, &r);   CHECK(v == 1); CHECK_RANGE(r, 0, TLR_ALL);
        v = p->get(&E, 9, &r);   CHECK(v == 1); CHECK_RANGE(r, 0, TLR_ALL);
        v = p->get(&E, 123456, &r); CHECK(v == 1); CHECK_RANGE(r, 0, TLR_ALL);
    }
    TLR_ENV_END();
}
TEST(gen_overflowing_ranges_rejected)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    static const DWRITE_TEXT_RANGE bad[] = { { 1, 0xFFFFFFFFu }, { 0xFFFFFFFFu, 1 }, { 0xFFFFFFFFu, 0xFFFFFFFFu }, { 0x80000000u, 0x80000000u },
                                             { 0xFFFFFFFEu, 2 }, { 10, 0xFFFFFFF6u }, { 0x7FFFFFFFu, 0x80000001u }, { 2, 0xFFFFFFFFu } };
    int i;
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        for (i = 0; i < 8; i++) {
            /* start + length wraps in 32 bits <=> E_INVALIDARG; the table only holds wrapping pairs */
            if ((UINT32)(bad[i].startPosition + bad[i].length) >= bad[i].startPosition) continue;
            SET_BAD(p, 1, bad[i].startPosition, bad[i].length);
        }
        EXPECT_AT(p, 0, 0, 0, TLR_ALL); EXPECT_AT(p, 9, 0, 0, TLR_ALL);          /* nothing was applied */
    }
    TLR_ENV_END();
}
TEST(gen_failed_set_keeps_previous_state)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 3);
        SET_BAD(p, 2, 5, 0xFFFFFFFFu);
        EXPECT_AT(p, 0, 0, 0, 2); EXPECT_AT(p, 2, 1, 2, 3); EXPECT_AT(p, 5, 0, 5, A(5));
    }
    TLR_ENV_END();
}
TEST(gen_adjacent_ranges_merge_forward)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 0, 3); SET_OK(p, 1, 3, 4);
        EXPECT_AT(p, 0, 1, 0, 7); EXPECT_AT(p, 3, 1, 0, 7); EXPECT_AT(p, 6, 1, 0, 7); EXPECT_AT(p, 7, 0, 7, A(7));
    }
    TLR_ENV_END();
}
TEST(gen_adjacent_ranges_merge_backward)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 3, 4); SET_OK(p, 1, 0, 3);
        EXPECT_AT(p, 0, 1, 0, 7); EXPECT_AT(p, 3, 1, 0, 7); EXPECT_AT(p, 6, 1, 0, 7); EXPECT_AT(p, 7, 0, 7, A(7));
    }
    TLR_ENV_END();
}
TEST(gen_filling_gap_merges_three_ranges)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 0, 2); SET_OK(p, 1, 4, 2);
        EXPECT_AT(p, 1, 1, 0, 2); EXPECT_AT(p, 3, 0, 2, 2); EXPECT_AT(p, 5, 1, 4, 2);
        SET_OK(p, 1, 2, 2);
        EXPECT_AT(p, 0, 1, 0, 6); EXPECT_AT(p, 3, 1, 0, 6); EXPECT_AT(p, 5, 1, 0, 6); EXPECT_AT(p, 6, 0, 6, A(6));
    }
    TLR_ENV_END();
}
TEST(gen_separated_ranges_stay_separate)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 1, 2); SET_OK(p, 1, 4, 2);      /* one character between */
        EXPECT_AT(p, 1, 1, 1, 2); EXPECT_AT(p, 3, 0, 3, 1); EXPECT_AT(p, 4, 1, 4, 2); EXPECT_AT(p, 6, 0, 6, A(6)); EXPECT_AT(p, 0, 0, 0, 1);
    }
    TLR_ENV_END();
}
TEST(gen_overlap_same_prop_new_value_wins)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        if (p->nvals < 3) continue;
        SET_OK(p, 1, 0, 6); SET_OK(p, 2, 3, 6);
        EXPECT_AT(p, 2, 1, 0, 3); EXPECT_AT(p, 3, 2, 3, 6); EXPECT_AT(p, 8, 2, 3, 6); EXPECT_AT(p, 9, 0, 9, A(9));
    }
    TLR_ENV_END();
}
TEST(gen_overlap_contained_range_splits_run)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 0, 10); SET_OK(p, 0, 3, 2);
        EXPECT_AT(p, 0, 1, 0, 3); EXPECT_AT(p, 2, 1, 0, 3); EXPECT_AT(p, 3, 0, 3, 2); EXPECT_AT(p, 4, 0, 3, 2);
        EXPECT_AT(p, 5, 1, 5, 5); EXPECT_AT(p, 9, 1, 5, 5); EXPECT_AT(p, 10, 0, 10, A(10));
    }
    TLR_ENV_END();
}
TEST(gen_overlap_containing_range_replaces_runs)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        if (p->nvals < 3) continue;
        SET_OK(p, 1, 2, 2); SET_OK(p, 2, 5, 2);
        SET_OK(p, 1, 0, 10);
        EXPECT_AT(p, 0, 1, 0, 10); EXPECT_AT(p, 3, 1, 0, 10); EXPECT_AT(p, 6, 1, 0, 10); EXPECT_AT(p, 9, 1, 0, 10); EXPECT_AT(p, 10, 0, 10, A(10));
    }
    TLR_ENV_END();
}
TEST(gen_overlap_same_value_extends)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 5); SET_OK(p, 1, 4, 5);
        EXPECT_AT(p, 1, 0, 0, 2); EXPECT_AT(p, 2, 1, 2, 7); EXPECT_AT(p, 8, 1, 2, 7); EXPECT_AT(p, 9, 0, 9, A(9));
    }
    TLR_ENV_END();
}
TEST(gen_overlap_straddling_start_of_existing_run)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        if (p->nvals < 3) continue;
        SET_OK(p, 1, 4, 4); SET_OK(p, 2, 2, 4);
        EXPECT_AT(p, 1, 0, 0, 2); EXPECT_AT(p, 2, 2, 2, 4); EXPECT_AT(p, 5, 2, 2, 4); EXPECT_AT(p, 6, 1, 6, 2); EXPECT_AT(p, 7, 1, 6, 2); EXPECT_AT(p, 8, 0, 8, A(8));
    }
    TLR_ENV_END();
}
TEST(gen_same_set_twice_is_idempotent)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 3); SET_OK(p, 1, 2, 3); SET_OK(p, 1, 3, 1);
        EXPECT_AT(p, 1, 0, 0, 2); EXPECT_AT(p, 2, 1, 2, 3); EXPECT_AT(p, 4, 1, 2, 3); EXPECT_AT(p, 5, 0, 5, A(5));
    }
    TLR_ENV_END();
}
TEST(gen_reverting_to_default_merges_all_runs)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 3);
        SET_OK(p, 0, 2, 3);
        EXPECT_AT(p, 0, 0, 0, TLR_ALL); EXPECT_AT(p, 2, 0, 0, TLR_ALL); EXPECT_AT(p, 4, 0, 0, TLR_ALL); EXPECT_AT(p, 5, 0, 0, TLR_ALL); EXPECT_AT(p, 100, 0, 0, TLR_ALL);
    }
    TLR_ENV_END();
}
TEST(gen_reverting_part_of_a_run_to_default)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 6);
        SET_OK(p, 0, 2, 2);
        EXPECT_AT(p, 0, 0, 0, 4); EXPECT_AT(p, 3, 0, 0, 4); EXPECT_AT(p, 4, 1, 4, 4); EXPECT_AT(p, 7, 1, 4, 4); EXPECT_AT(p, 8, 0, 8, A(8));
    }
    TLR_ENV_END();
}
TEST(gen_revert_to_default_beyond_all_runs_changes_nothing)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 0, 20, 30);
        EXPECT_AT(p, 0, 0, 0, TLR_ALL); EXPECT_AT(p, 25, 0, 0, TLR_ALL);
    }
    TLR_ENV_END();
}
TEST(gen_alternating_single_character_ranges)
{
    const tlr_prop *p; int i; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        for (i = 0; i < 10; i += 2) SET_OK(p, 1, (UINT32)i, 1);
        for (i = 0; i < 9; i++) {
            if (i & 1) EXPECT_AT(p, (UINT32)i, 0, (UINT32)i, 1);
            else       EXPECT_AT(p, (UINT32)i, 1, (UINT32)i, 1);
        }
        EXPECT_AT(p, 9, 0, 9, A(9));             /* position 9 is default and merges with the untouched tail */
        EXPECT_AT(p, 10, 0, 9, A(9));
    }
    TLR_ENV_END();
}
TEST(gen_alternating_then_fill_collapses)
{
    const tlr_prop *p; int i; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        for (i = 0; i < 10; i += 2) SET_OK(p, 1, (UINT32)i, 1);
        SET_OK(p, 1, 0, 10);
        EXPECT_AT(p, 0, 1, 0, 10); EXPECT_AT(p, 5, 1, 0, 10); EXPECT_AT(p, 9, 1, 0, 10);
        SET_OK(p, 0, 0, 10);
        EXPECT_AT(p, 0, 0, 0, TLR_ALL); EXPECT_AT(p, 9, 0, 0, TLR_ALL);
    }
    TLR_ENV_END();
}
TEST(gen_run_boundaries_are_half_open)
{
    const tlr_prop *p; UINT32 pos; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 3, 4);
        for (pos = 0; pos < 14; pos++) {
            if (pos < 3) EXPECT_AT(p, pos, 0, 0, 3);
            else if (pos < 7) EXPECT_AT(p, pos, 1, 3, 4);
            else EXPECT_AT(p, pos, 0, 7, A(7));
        }
    }
    TLR_ENV_END();
}
TEST(gen_ranges_split_by_other_property)
{
    /* the reported range is the run of the COMPLETE format: changing an unrelated property splits it */
    const tlr_prop *p; const tlr_prop *other; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        other = (&tlr_props[0] == p) ? &tlr_props[1] : &tlr_props[0];
        SET_OK(p, 1, 0, 10);
        SET_OK(other, 1, 4, 2);
        EXPECT_AT(p, 0, 1, 0, 4); EXPECT_AT(p, 3, 1, 0, 4); EXPECT_AT(p, 4, 1, 4, 2); EXPECT_AT(p, 5, 1, 4, 2); EXPECT_AT(p, 6, 1, 6, 4); EXPECT_AT(p, 9, 1, 6, 4);
        EXPECT_AT(p, 10, 0, 10, A(10));
        /* and the other property reports the same boundaries */
        EXPECT_AT(other, 3, 0, 0, 4); EXPECT_AT(other, 4, 1, 4, 2); EXPECT_AT(other, 6, 0, 6, 4);
        /* undoing the other property merges the pieces again */
        SET_OK(other, 0, 4, 2);
        EXPECT_AT(p, 0, 1, 0, 10); EXPECT_AT(p, 5, 1, 0, 10);
    }
    TLR_ENV_END();
}
TEST(gen_same_range_for_two_properties_shares_one_run)
{
    const tlr_prop *p; const tlr_prop *q; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        q = (&tlr_props[3] == p) ? &tlr_props[4] : &tlr_props[3];
        SET_OK(p, 1, 2, 4); SET_OK(q, 1, 2, 4);
        EXPECT_AT(p, 2, 1, 2, 4); EXPECT_AT(q, 5, 1, 2, 4); EXPECT_AT(p, 6, 0, 6, A(6)); EXPECT_AT(q, 1, 0, 0, 2);
    }
    TLR_ENV_END();
}
TEST(gen_survives_invalidate_layout_and_metrics)
{
    const tlr_prop *p; DWRITE_TEXT_METRICS m; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 3);
        CHECK_HR(IDWriteTextLayout_GetMetrics(E.l, &m), S_OK);
        EXPECT_AT(p, 3, 1, 2, 3);
        { IDWriteTextLayout3 *l3 = NULL;
          if (SUCCEEDED(IDWriteTextLayout_QueryInterface(E.l, &IID_IDWriteTextLayout3, (void **)&l3)) && l3) {
              CHECK_HR(IDWriteTextLayout3_InvalidateLayout(l3), S_OK); IDWriteTextLayout3_Release(l3);
          } }
        EXPECT_AT(p, 0, 0, 0, 2); EXPECT_AT(p, 3, 1, 2, 3); EXPECT_AT(p, 5, 0, 5, A(5));
        CHECK_HR(IDWriteTextLayout_GetMetrics(E.l, &m), S_OK);
        EXPECT_AT(p, 3, 1, 2, 3);
    }
    TLR_ENV_END();
}
TEST(gen_layout_still_lays_out_after_each_set)
{
    const tlr_prop *p; DWRITE_TEXT_METRICS m; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        HRESULT hr;
        SKIP_NO_L1(p);
        SET_OK(p, 1, 3, 4);
        memset(&m, 0xcc, sizeof m);
        hr = IDWriteTextLayout_GetMetrics(E.l, &m);
        t_check(hr == S_OK, __FILE__, __LINE__, p->name, "GetMetrics -> %s", dw_hr_name(hr));
        t_check(m.lineCount >= 1, __FILE__, __LINE__, p->name, "line count %u", m.lineCount);
    }
    TLR_ENV_END();
}
TEST(gen_get_ranges_with_null_range_pointer)
{
    const tlr_prop *p; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        int v;
        SKIP_NO_L1(p);
        SET_OK(p, 1, 2, 3);
        v = p->get(&E, 3, NULL);          /* the getter helpers poison and pass a range; the NULL case is exercised in def_getter_null_range_pointer_accepted */
        t_check(v == 1, __FILE__, __LINE__, p->name, "value %d", v);
    }
    TLR_ENV_END();
}
TEST(gen_empty_text_layout_ranges)
{
    const tlr_prop *p; TLR_ENV_BEGIN(L"");
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 0, 4);
        EXPECT_AT(p, 0, 1, 0, 4); EXPECT_AT(p, 3, 1, 0, 4); EXPECT_AT(p, 4, 0, 4, A(4));
        SET_OK(p, 0, 0, 4);
        EXPECT_AT(p, 0, 0, 0, TLR_ALL);
    }
    TLR_ENV_END();
}
TEST(gen_surrogate_pair_halves_are_ordinary_positions)
{
    /* positions are UTF-16 code units: a range may start or end between the two halves of a pair */
    const tlr_prop *p; TLR_ENV_BEGIN(L"a\xD83D\xDE00z");
    FOR_PROPS(p) {
        DWRITE_TEXT_METRICS m; HRESULT hr;
        SKIP_NO_L1(p);
        SET_OK(p, 1, 1, 1);             /* high surrogate only */
        EXPECT_AT(p, 0, 0, 0, 1); EXPECT_AT(p, 1, 1, 1, 1); EXPECT_AT(p, 2, 0, 2, A(2));
        hr = IDWriteTextLayout_GetMetrics(E.l, &m);
        t_check(hr == S_OK, __FILE__, __LINE__, p->name, "layout over split pair -> %s", dw_hr_name(hr));
        SET_OK(p, 2 < p->nvals ? 2 : 1, 2, 1);   /* low surrogate only */
        if (p->nvals >= 3) { EXPECT_AT(p, 1, 1, 1, 1); EXPECT_AT(p, 2, 2, 2, 1); EXPECT_AT(p, 3, 0, 3, A(3)); }
        else EXPECT_AT(p, 2, 1, 1, 2);
    }
    TLR_ENV_END();
}
TEST(gen_ranges_do_not_leak_between_layouts)
{
    const tlr_prop *p; IDWriteTextLayout *other; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        SET_OK(p, 1, 0, 10);
    }
    other = tlr_layout_(&E.x, TLR_FAM, T10, 16.0f);
    if (other) {
        DWRITE_TEXT_RANGE r;
        CHECK(tlr_weight(other, 0, &r) == DWRITE_FONT_WEIGHT_NORMAL); CHECK_RANGE(r, 0, TLR_ALL);
        CHECK(tlr_underline(other, 0, &r) == FALSE);
        DW_RELEASE(other);
    }
    TLR_ENV_END();
}
TEST(gen_many_ranges_then_clear)
{
    const tlr_prop *p; int i; TLR_ENV_BEGIN(T10);
    FOR_PROPS(p) {
        SKIP_NO_L1(p);
        for (i = 0; i < 40; i++) SET_OK(p, 1 + (i % (p->nvals - 1)), (UINT32)(i * 3), 2);
        SET_OK(p, 0, 0, 200);
        EXPECT_AT(p, 0, 0, 0, TLR_ALL); EXPECT_AT(p, 60, 0, 0, TLR_ALL); EXPECT_AT(p, 119, 0, 0, TLR_ALL);
    }
    TLR_ENV_END();
}
