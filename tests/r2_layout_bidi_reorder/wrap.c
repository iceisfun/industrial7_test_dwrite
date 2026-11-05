/* Line breaking, line order and alignment of right-to-left paragraphs.
 *
 * Facts: line breaking works on the logical text, so lines hold consecutive logical characters whatever the paragraph
 * direction; lines stack top to bottom in logical order; in a right-to-left paragraph each line starts at the right edge
 * (leading alignment) and the box of a line grows leftwards. DWRITE_TEXT_METRICS.width excludes trailing white space,
 * widthIncludingTrailingWhitespace includes it.
 */
#include "bx.h"

#define HW L"\u05d0\u05d1\u05d2 \u05d3\u05d4\u05d5 \u05d6\u05d7\u05d8 \u05d9\u05db\u05dc"     /* four 3-letter words, 15 chars */

static void wrap_lines(int base)
{
    BX_BEGIN(); BX_LAYOUT(l, HW, 6.5f * CW, 200.0f);
    DWRITE_LINE_METRICS lm[8]; UINT32 n, i; DWRITE_TEXT_METRICS tm;
    static const UINT32 len[] = { 4, 4, 4, 3 }, tws[] = { 1, 1, 1, 0 };
    if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_lines(l, lm, 8);
    CHECK(n == 4);
    for (i = 0; i < n && i < 4; i++) {
        CHECKF(lm[i].length == len[i], "line %u length %u want %u", i, lm[i].length, len[i]);
        CHECKF(lm[i].trailingWhitespaceLength == tws[i], "line %u trailing ws %u want %u", i, lm[i].trailingWhitespaceLength, tws[i]);
        CHECK(lm[i].newlineLength == 0);
        CHECK(lm[i].height > 0 && lm[i].baseline > 0 && lm[i].baseline <= lm[i].height);
    }
    bx_metrics(l, &tm);
    CHECK(tm.lineCount == 4);
    CHK_NEAR(tm.width, 3 * CW); CHK_NEAR(tm.widthIncludingTrailingWhitespace, 4 * CW);
    CHK_NEAR(tm.height, lm[0].height + lm[1].height + lm[2].height + lm[3].height);
    CHK_NEAR(tm.top, 0.0f);
    CHK_NEAR(tm.left, base ? 6.5f * CW - 3 * CW : 0.0f);
    BX_DONE(l);
}
TEST(r2_layout_bidi_reorder_wrap_hebrew_lines_ltr_paragraph) { wrap_lines(0); }
TEST(r2_layout_bidi_reorder_wrap_hebrew_lines_rtl_paragraph) { wrap_lines(1); }

static void wrap_positions(int base)
{
    BX_BEGIN(); BX_LAYOUT(l, HW, 6.5f * CW, 200.0f);
    DWRITE_LINE_METRICS lm[8]; UINT32 n, k, i; float top = 0, W = 6.5f * CW;
    if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_lines(l, lm, 8);
    if (n != 4) { BX_DONE(l); CHECK(n == 4); return; }
    for (k = 0; k < 4; k++) {
        UINT32 first = 4 * k, vis = 3;
        bx_tp a = bx_pos(l, first, FALSE);
        CHK_NEAR(a.y, top);                                                    /* lines stack in logical order */
        CHK_NEAR(a.x, base ? W : 3 * CW);                                      /* RTL paragraph: a line starts at the right edge; LTR: the Hebrew word is a right-to-left run spanning [0,3CW] */
        CHECK(a.m.textPosition == first);
        for (i = 0; i < vis; i++) {                                            /* the three letters step away from the edge */
            bx_tp p = bx_pos(l, first + i, FALSE), q = bx_pos(l, first + i, TRUE);
            CHK_NEAR(p.x, base ? W - i * CW : (3 - i) * CW);
            CHK_NEAR(q.x, base ? W - (i + 1) * CW : (2 - i) * CW);
            CHK_NEAR(p.y, top);
            CHECK(p.m.bidiLevel == 1);
        }
        top += lm[k].height;
    }
    BX_DONE(l);
}
TEST(r2_layout_bidi_reorder_wrap_line_positions_ltr_paragraph) { wrap_positions(0); }
TEST(r2_layout_bidi_reorder_wrap_line_positions_rtl_paragraph) { wrap_positions(1); }

TEST(r2_layout_bidi_reorder_wrap_point_selects_line)
{
    BX_BEGIN(); BX_LAYOUT(l, HW, 6.5f * CW, 200.0f);
    DWRITE_LINE_METRICS lm[8]; UINT32 n; float h;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_lines(l, lm, 8); h = lm[0].height;
    if (n == 4) {
        bx_pt p = bx_point(l, 6.5f * CW - 0.5f * CW, 0.5f * h);      /* inside the right-most slot of line 0: aleph, leading half */
        CHECK(p.m.textPosition == 0 && p.tr == FALSE && p.in == TRUE);
        p = bx_point(l, 6.5f * CW - 0.5f * CW, 1.5f * h);            /* same x, second line: first letter of the second word */
        CHECK(p.m.textPosition == 4 && p.in == TRUE);
        p = bx_point(l, 6.5f * CW - 0.5f * CW, 3.5f * h);
        CHECK(p.m.textPosition == 12 && p.in == TRUE);
        p = bx_point(l, 6.5f * CW - 2.5f * CW, 3.5f * h);            /* third slot from the right of the last line */
        CHECK(p.m.textPosition == 14);
    }
    BX_DONE(l);
}

/* A newline separates paragraphs inside one layout: each line is aligned independently. */
TEST(r2_layout_bidi_reorder_wrap_newline_lines_rtl)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2\n\u05d3\u05d4", BX_W, 100.0f);
    DWRITE_LINE_METRICS lm[4]; UINT32 n; DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_lines(l, lm, 4);
    CHECK(n == 2);
    if (n == 2) {
        CHECK(lm[0].length == 4 && lm[0].newlineLength == 1 && lm[1].length == 2 && lm[1].newlineLength == 0);
        bx_tp a = bx_pos(l, 0, FALSE), b = bx_pos(l, 4, FALSE);
        CHK_NEAR(a.x, BX_W); CHK_NEAR(b.x, BX_W); CHK_NEAR(a.y, 0.0f); CHK_NEAR(b.y, lm[0].height);
        bx_metrics(l, &tm); CHK_NEAR(tm.width, 3 * CW); CHK_NEAR(tm.left, BX_W - 3 * CW);
    }
    BX_DONE(l);
}

/* Line breaking is independent of the paragraph direction, also for mixed text. */
typedef struct { const char *nm; const WCHAR *text; float w; } lb_row;
static void breaks_same(const lb_row *r)
{
    BX_BEGIN(); BX_LAYOUT(a, r->text, r->w, 300.0f); BX_LAYOUT(b, r->text, r->w, 300.0f);
    DWRITE_LINE_METRICS la[16], lb[16]; UINT32 na, nb, i;
    IDWriteTextLayout_SetReadingDirection(b, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    na = bx_lines(a, la, 16); nb = bx_lines(b, lb, 16);
    CHECK(na >= 2 && na == nb);
    for (i = 0; i < na && i < nb; i++) CHECK(la[i].length == lb[i].length && la[i].trailingWhitespaceLength == lb[i].trailingWhitespaceLength && la[i].newlineLength == lb[i].newlineLength);
    BX_DONE(a, b);
}
#define LB(nm, text, w) static const lb_row lb_##nm = { #nm, text, w }; TEST(r2_layout_bidi_reorder_wrap_breaks_same_##nm) { breaks_same(&lb_##nm); }
LB(hebrew_words,    HW, 6.5f * CW)
LB(hebrew_narrow,   HW, 3.5f * CW)
LB(mixed_words,     L"\u05d0\u05d1\u05d2 abc \u05d3\u05d4\u05d5 def", 7.5f * CW)
LB(arabic_digits,   L"\u0627\u0628\u062c 123 \u062f\u0647\u0648 456", 7.5f * CW)
LB(latin_in_hebrew, L"\u05d0\u05d1 abc def \u05d2\u05d3", 6.5f * CW)

/* Each logical line of mixed text is reordered on its own: a Latin word wrapped to line 2 keeps its level. */
TEST(r2_layout_bidi_reorder_wrap_mixed_levels_per_line)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2 abc \u05d3\u05d4\u05d5", 5.5f * CW, 200.0f);
    DWRITE_LINE_METRICS lm[8]; UINT32 n = bx_lines(l, lm, 8); UINT8 lev[16];
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_lines(l, lm, 8);
    CHECK(n == 3);
    dwx_renderer *r = bx_draw(l, 0, 0);
    if (r) {
        CHECK(bx_draw_levels(r, 11, lev));
        CHECK(lev[0] == 1 && lev[3] == 1 && lev[4] == 2 && lev[6] == 2 && lev[7] == 1 && lev[8] == 1);
        dwx_release(r);
    }
    if (n == 3) {
        bx_tp a = bx_pos(l, 4, FALSE);                                   /* 'a' starts line 2: LTR run flush against the right edge */
        bx_tp c = bx_pos(l, 6, TRUE);
        CHK_NEAR(c.x, 5.5f * CW);                                        /* 'c' trailing edge is the right edge of line 2's content */
        CHK_NEAR(a.x, 5.5f * CW - 3 * CW);
        CHECK(a.y > 0);
    }
    BX_DONE(l);
}

/* ---- text alignment of a single line vs reading direction ------------------------------------------------------ */
typedef struct { int base; DWRITE_TEXT_ALIGNMENT al; float left; } al_row;
static void align_one(const WCHAR *text, UINT32 n, const al_row *r)
{
    BX_BEGIN(); BX_LAYOUT(l, text, 100.0f, 50.0f); DWRITE_TEXT_METRICS tm; float w = n * CW;
    if (r->base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, r->al));
    bx_metrics(l, &tm);
    CHK_NEAR(tm.width, w);
    CHK_NEAR(tm.left, r->left < 0 ? 0.0f : r->left);
    if (r->left < 0) {}
    BX_DONE(l);
}
#define W3 (3 * CW)
#define AL(nm, text, n, base, al, lft) static const al_row al_##nm = { base, al, lft }; TEST(r2_layout_bidi_reorder_wrap_align_##nm) { align_one(text, n, &al_##nm); }
AL(heb_ltr_leading,   L"\u05d0\u05d1\u05d2", 3, 0, DWRITE_TEXT_ALIGNMENT_LEADING,  0.0f)
AL(heb_ltr_trailing,  L"\u05d0\u05d1\u05d2", 3, 0, DWRITE_TEXT_ALIGNMENT_TRAILING, 100.0f - W3)
AL(heb_ltr_center,    L"\u05d0\u05d1\u05d2", 3, 0, DWRITE_TEXT_ALIGNMENT_CENTER,   (100.0f - W3) / 2)
AL(heb_rtl_leading,   L"\u05d0\u05d1\u05d2", 3, 1, DWRITE_TEXT_ALIGNMENT_LEADING,  100.0f - W3)
AL(heb_rtl_trailing,  L"\u05d0\u05d1\u05d2", 3, 1, DWRITE_TEXT_ALIGNMENT_TRAILING, 0.0f)
AL(heb_rtl_center,    L"\u05d0\u05d1\u05d2", 3, 1, DWRITE_TEXT_ALIGNMENT_CENTER,   (100.0f - W3) / 2)
AL(lat_ltr_leading,   L"abc", 3, 0, DWRITE_TEXT_ALIGNMENT_LEADING,  0.0f)
AL(lat_ltr_trailing,  L"abc", 3, 0, DWRITE_TEXT_ALIGNMENT_TRAILING, 100.0f - W3)
AL(lat_rtl_leading,   L"abc", 3, 1, DWRITE_TEXT_ALIGNMENT_LEADING,  100.0f - W3)
AL(lat_rtl_trailing,  L"abc", 3, 1, DWRITE_TEXT_ALIGNMENT_TRAILING, 0.0f)
AL(lat_rtl_center,    L"abc", 3, 1, DWRITE_TEXT_ALIGNMENT_CENTER,   (100.0f - W3) / 2)
AL(mixed_ltr_leading, L"ab \u05d0\u05d1\u05d2", 6, 0, DWRITE_TEXT_ALIGNMENT_LEADING, 0.0f)
AL(mixed_rtl_leading, L"ab \u05d0\u05d1\u05d2", 6, 1, DWRITE_TEXT_ALIGNMENT_LEADING, 100.0f - 6 * CW)
AL(mixed_rtl_trailing,L"ab \u05d0\u05d1\u05d2", 6, 1, DWRITE_TEXT_ALIGNMENT_TRAILING, 0.0f)
AL(mixed_ltr_trailing,L"ab \u05d0\u05d1\u05d2", 6, 0, DWRITE_TEXT_ALIGNMENT_TRAILING, 100.0f - 6 * CW)

/* Alignment re-applies as soon as the alignment or direction changes. */
TEST(r2_layout_bidi_reorder_wrap_align_flip_updates_positions)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2", 100.0f, 50.0f);
    bx_tp p = bx_pos(l, 0, FALSE);
    CHK_NEAR(p.x, W3);                                                    /* LTR paragraph: aleph sits at the right end of its box [0, 3CW] */
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    p = bx_pos(l, 0, FALSE); CHK_NEAR(p.x, 100.0f);
    IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_TRAILING);
    p = bx_pos(l, 0, FALSE); CHK_NEAR(p.x, W3);
    IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER);
    p = bx_pos(l, 0, FALSE); CHK_NEAR(p.x, (100.0f + W3) / 2);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    p = bx_pos(l, 0, FALSE); CHK_NEAR(p.x, (100.0f + W3) / 2);             /* centred either way */
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_wrap_nowrap_rtl_overflows_to_the_left)
{
    BX_BEGIN(); BX_LAYOUT(l, HW, 6.5f * CW, 200.0f); DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, DWRITE_WORD_WRAPPING_NO_WRAP));
    bx_metrics(l, &tm);
    CHECK(tm.lineCount == 1);
    CHK_NEAR(tm.width, 15 * CW);
    CHK_NEAR(tm.left, 6.5f * CW - 15 * CW);                                  /* aligned at the right edge, overflowing leftwards */
    { bx_tp a = bx_pos(l, 0, FALSE); CHK_NEAR(a.x, 6.5f * CW); }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_wrap_max_width_change_relayouts_rtl)
{
    BX_BEGIN(); BX_LAYOUT(l, HW, 100.0f, 200.0f); DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    bx_metrics(l, &tm); CHECK(tm.lineCount == 2);                              /* 100 DIPs hold 12 chars: 3 words (11 visible) on line 1 */
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 6.5f * CW));
    bx_metrics(l, &tm); CHECK(tm.lineCount == 4);
    { bx_tp a = bx_pos(l, 0, FALSE); CHK_NEAR(a.x, 6.5f * CW); }
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 300.0f));
    bx_metrics(l, &tm); CHECK(tm.lineCount == 1);
    { bx_tp a = bx_pos(l, 0, FALSE); CHK_NEAR(a.x, 300.0f); }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_wrap_justified_last_line_recorded)
{
    BX_BEGIN(); BX_LAYOUT(l, HW, 6.5f * CW, 200.0f); DWRITE_TEXT_METRICS tm;
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_JUSTIFIED));
    bx_metrics(l, &tm);
    CHECK(tm.lineCount == 4);
    t_note("justified RTL: left %g width %g layoutWidth %g", tm.left, tm.width, tm.layoutWidth);
    CHECK(tm.width <= 6.5f * CW + EPS);
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_wrap_paragraph_alignment_independent_of_direction)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2", 100.0f, 80.0f); DWRITE_TEXT_METRICS a, b;
    CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, DWRITE_PARAGRAPH_ALIGNMENT_FAR));
    bx_metrics(l, &a);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    bx_metrics(l, &b);
    CHK_NEAR(a.top, b.top); CHECK(a.top > 0);
    BX_DONE(l);
}
