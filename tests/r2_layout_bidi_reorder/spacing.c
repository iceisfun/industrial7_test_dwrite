/* IDWriteTextLayout1::SetCharacterSpacing on left-to-right, right-to-left and mixed text.
 *
 * Facts (TextAnalyzer::ApplyCharacterSpacing, used per cluster):
 *  - with non-negative leading/trailing the cluster advance becomes max(base, minimumAdvanceWidth) + leading + trailing,
 *    the shortfall to the minimum being shared half and half between the two sides;
 *  - a single negative side can never push the advance below minimumAdvanceWidth;
 *  - the spacing is per cluster (a base with combining marks gets it once) and does not depend on the direction of the run.
 */
#include "bx.h"

static float total_width(IDWriteTextLayout *l)
{
    DWRITE_CLUSTER_METRICS cm[64]; UINT32 n = bx_clusters(l, cm, 64), i; float s = 0;
    for (i = 0; i < n; i++) s += cm[i].width;
    return s;
}

typedef struct { const char *nm; const WCHAR *text; UINT32 n; int base; float lead, trail, min; float adv; } sp_row;

/* every cluster of the text gets the same advance */
static void run_spacing(const sp_row *r)
{
    BX_BEGIN(); BX_LAYOUT(l, r->text, 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    DWRITE_CLUSTER_METRICS cm[64]; UINT32 n, i; DWRITE_TEXT_METRICS tm;
    if (r->base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, r->lead, r->trail, r->min, RNG(0, r->n)));
    n = bx_clusters(l, cm, 64);
    CHECK(n == r->n);
    for (i = 0; i < n && i < r->n; i++) CHECKF(bx_feq(cm[i].width, r->adv), "cluster %u width %g want %g", i, cm[i].width, r->adv);
    bx_metrics(l, &tm);
    CHK_NEAR(tm.width, r->n * r->adv);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}
#define SP(nm, text, n, base, lead, trail, min, adv) static const sp_row sp_##nm = { #nm, text, n, base, lead, trail, min, adv }; TEST(r2_layout_bidi_reorder_spacing_##nm) { run_spacing(&sp_##nm); }
#define HEB3 L"\u05d0\u05d1\u05d2"
SP(latin_lead,            L"abc", 3, 0, 2.0f, 0.0f, 0.0f, CW + 2.0f)
SP(latin_trail,           L"abc", 3, 0, 0.0f, 3.0f, 0.0f, CW + 3.0f)
SP(latin_both,            L"abc", 3, 0, 2.0f, 3.0f, 0.0f, CW + 5.0f)
SP(hebrew_both_ltr,       HEB3,   3, 0, 2.0f, 3.0f, 0.0f, CW + 5.0f)
SP(hebrew_both_rtl,       HEB3,   3, 1, 2.0f, 3.0f, 0.0f, CW + 5.0f)
SP(latin_rtl_para,        L"abc", 3, 1, 2.0f, 3.0f, 0.0f, CW + 5.0f)
SP(min_below_base,        HEB3,   3, 0, 0.0f, 0.0f, 3.0f, CW)
SP(min_equal_base,        HEB3,   3, 0, 0.0f, 0.0f, CW, CW)
SP(min_above_base,        HEB3,   3, 0, 0.0f, 0.0f, 20.0f, 20.0f)
SP(min_above_base_rtl,    HEB3,   3, 1, 0.0f, 0.0f, 20.0f, 20.0f)
SP(min_plus_spacing,      HEB3,   3, 0, 1.0f, 2.0f, 20.0f, 23.0f)
SP(min_plus_spacing_rtl,  HEB3,   3, 1, 1.0f, 2.0f, 20.0f, 23.0f)
SP(neg_lead,              L"abc", 3, 0, -2.0f, 0.0f, 0.0f, CW - 2.0f)
SP(neg_trail,             L"abc", 3, 0, 0.0f, -2.0f, 0.0f, CW - 2.0f)
SP(neg_both_within_base,  L"abc", 3, 0, -1.0f, -1.5f, 0.0f, CW - 2.5f)
SP(neg_lead_clamped_at_zero, L"abc", 3, 0, -20.0f, 0.0f, 0.0f, 0.0f)
SP(neg_trail_clamped_at_zero,HEB3, 3, 1, 0.0f, -20.0f, 0.0f, 0.0f)
SP(neg_lead_clamped_at_min, L"abc", 3, 0, -20.0f, 0.0f, 3.0f, 3.0f)
SP(neg_trail_clamped_at_min,HEB3, 3, 0, 0.0f, -20.0f, 3.0f, 3.0f)
SP(zero_is_identity,      HEB3,   3, 1, 0.0f, 0.0f, 0.0f, CW)
SP(mixed_sign_small,      L"abc", 3, 0, 2.0f, -1.0f, 0.0f, CW + 1.0f)

/* a range inside a mixed string only changes its own clusters; the others keep the font advance */
TEST(r2_layout_bidi_reorder_spacing_range_in_mixed_text)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    DWRITE_CLUSTER_METRICS cm[16]; UINT32 n, i;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 1.0f, 2.0f, 0.0f, RNG(3, 3)));      /* the Hebrew letters */
    n = bx_clusters(l, cm, 16); CHECK(n == 9);
    for (i = 0; i < n && i < 9; i++) CHECKF(bx_feq(cm[i].width, (i >= 3 && i < 6) ? CW + 3.0f : CW), "cluster %u width %g", i, cm[i].width);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_spacing_range_straddling_runs)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    DWRITE_CLUSTER_METRICS cm[16]; UINT32 n, i;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0.0f, 2.0f, 0.0f, RNG(1, 7)));      /* b .. space after the Hebrew */
    n = bx_clusters(l, cm, 16); CHECK(n == 9);
    for (i = 0; i < n && i < 9; i++) CHECKF(bx_feq(cm[i].width, (i >= 1 && i < 8) ? CW + 2.0f : CW), "cluster %u width %g", i, cm[i].width);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* spacing is per cluster: a base and its combining marks share one spacing */
TEST(r2_layout_bidi_reorder_spacing_applies_per_cluster_with_marks)
{
    BX_BEGIN(); BX_LAYOUT(l, L"a\u0301\u0302\u05d0\u05b0\u05b1", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    DWRITE_CLUSTER_METRICS cm[8]; UINT32 n;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 2.0f, 3.0f, 0.0f, RNG(0, 6)));
    n = bx_clusters(l, cm, 8); CHECK(n == 2);
    if (n == 2) { CHK_NEAR(cm[0].width, CW + 5.0f); CHK_NEAR(cm[1].width, CW + 5.0f); }
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* spacing alters wrapping: wider clusters break earlier */
TEST(r2_layout_bidi_reorder_spacing_changes_line_breaks)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 \u05d3\u05d4\u05d5", 7.5f * CW, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    DWRITE_TEXT_METRICS tm;
    bx_metrics(l, &tm); CHECK(tm.lineCount == 1);                               /* 7 chars = 7CW <= 7.5CW */
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0.0f, 1.0f, 0.0f, RNG(0, 7)));
    bx_metrics(l, &tm); CHECK(tm.lineCount == 2);                               /* 7 * (CW + 1) > 7.5CW */
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0.0f, 0.0f, 0.0f, RNG(0, 7)));
    bx_metrics(l, &tm); CHECK(tm.lineCount == 1);
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* the glyph run advances carry the spacing */
TEST(r2_layout_bidi_reorder_spacing_draw_advances)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab\u05d0\u05d1", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    dwx_renderer *r; int i; UINT32 k;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 1.0f, 2.0f, 0.0f, RNG(0, 4)));
    r = bx_draw(l, 0, 0);
    if (r) {
        for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN)
            for (k = 0; k < r->calls[i].nglyphs; k++) CHK_NEAR(r->calls[i].advances[k], CW + 3.0f);
        dwx_release(r);
    }
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* hit-test positions follow the widened clusters in both directions */
TEST(r2_layout_bidi_reorder_spacing_hit_positions)
{
    BX_BEGIN(); BX_LAYOUT(a, L"abc", 1000.0f, 100.0f); BX_LAYOUT(b, L"\u05d0\u05d1\u05d2", 1000.0f, 100.0f);
    BX_QI(a1, IDWriteTextLayout1, a); BX_QI(b1, IDWriteTextLayout1, b);
    float w = CW + 5.0f; UINT32 i;
    IDWriteTextLayout1_SetCharacterSpacing(a1, 2.0f, 3.0f, 0.0f, RNG(0, 3));
    IDWriteTextLayout1_SetCharacterSpacing(b1, 2.0f, 3.0f, 0.0f, RNG(0, 3));
    for (i = 0; i < 3; i++) {
        bx_tp p = bx_pos(a, i, FALSE), q = bx_pos(b, i, FALSE);
        CHK_NEAR(p.x, i * w); CHK_NEAR(p.m.width, w);
        CHK_NEAR(q.m.left, (2 - i) * w); CHK_NEAR(q.x, (3 - i) * w);
    }
    IDWriteTextLayout1_Release(a1); IDWriteTextLayout1_Release(b1); BX_DONE(a, b);
}

/* the glyph offset of a spaced glyph carries the leading spacing (recorded: the sign of the offset in RTL runs is not fixed by docs) */
TEST(r2_layout_bidi_reorder_spacing_glyph_offsets_recorded)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab\u05d0\u05d1", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    dwx_renderer *r; int i;
    IDWriteTextLayout1_SetCharacterSpacing(l1, 4.0f, 0.0f, 0.0f, RNG(0, 4));
    r = bx_draw(l, 0, 0);
    if (r) {
        for (i = 0; i < r->ncalls; i++) if (r->calls[i].kind == DWX_DRAW_GLYPHRUN && r->calls[i].offsets && r->calls[i].nglyphs) {
            t_note("run at %u level %u first offset (%g, %g) advance %g", r->calls[i].text_pos, r->calls[i].bidi, r->calls[i].offsets[0].advanceOffset, r->calls[i].offsets[0].ascenderOffset, r->calls[i].advances[0]);
            if (!(r->calls[i].bidi & 1)) CHK_NEAR(r->calls[i].offsets[0].advanceOffset, 4.0f);       /* ApplyCharacterSpacing adds the leading spacing to the glyph's advanceOffset */
        }
        dwx_release(r);
    }
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}

/* clearing spacing restores the original widths; total widths of identical text agree with a fresh layout */
TEST(r2_layout_bidi_reorder_spacing_reset_restores)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 abc", 1000.0f, 100.0f); BX_LAYOUT(fresh, L"\u05d0\u05d1\u05d2 abc", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    IDWriteTextLayout1_SetCharacterSpacing(l1, 3.0f, 3.0f, 9.0f, RNG(0, 7));
    CHECK(total_width(l) > total_width(fresh));
    IDWriteTextLayout1_SetCharacterSpacing(l1, 0.0f, 0.0f, 0.0f, RNG(0, 7));
    CHK_NEAR(total_width(l), total_width(fresh));
    IDWriteTextLayout1_Release(l1); BX_DONE(l, fresh);
}

/* spacing set on a layout built from a different run structure (font size ranges) stays per range */
TEST(r2_layout_bidi_reorder_spacing_with_font_size_ranges)
{
    BX_BEGIN(); BX_LAYOUT(l, L"abcd", 1000.0f, 100.0f); BX_QI(l1, IDWriteTextLayout1, l);
    DWRITE_CLUSTER_METRICS cm[4]; UINT32 n;
    IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(2, 2));
    IDWriteTextLayout1_SetCharacterSpacing(l1, 1.0f, 1.0f, 0.0f, RNG(1, 2));
    n = bx_clusters(l, cm, 4); CHECK(n == 4);
    if (n == 4) { CHK_NEAR(cm[0].width, CW); CHK_NEAR(cm[1].width, CW + 2.0f); CHK_NEAR(cm[2].width, 2 * CW + 2.0f); CHK_NEAR(cm[3].width, 2 * CW); }
    IDWriteTextLayout1_Release(l1); BX_DONE(l);
}
