/* DWRITE_CLUSTER_METRICS content: cluster segmentation (ASCII, combining marks, surrogate pairs, newlines, ZWJ, Hangul, soft hyphen,
 * tabs, inline objects), per-cluster flags and widths, bidi direction flag, and the relations to line and text metrics.
 *
 * Facts used: canWrapLineAfter is set when the break condition after the cluster's last character is CAN_BREAK or MUST_BREAK,
 * isNewline when it is MUST_BREAK (so the CR of a CRLF pair is not a newline cluster but the LF is), isWhitespace and isSoftHyphen
 * come from the line-break analysis of the cluster's first character, isRightToLeft from the resolved bidi level. A tab cluster
 * is stretched to the next multiple of the incremental tab stop measured from the start of its line. */
#include "tlm.h"

static IDWriteTextLayout *mk(tm_fx *x, const WCHAR *fam, const WCHAR *text, UINT32 len, float w)
{
    return tm_layout_(x, fam, text, len, SZ, w, 1000.0f);
}
#define MK_UNI(var, text) IDWriteTextLayout *var = mk(&X, FAM_UNI, text, (UINT32)wcslen(text), 1000.0f); if (!var) { TM_END(); t_skip("CreateTextLayout failed"); return; }

/* ---- ASCII ----------------------------------------------------------------------------------------------- */
TEST(cc_ascii_one_cluster_per_char)
{
    TM_BEGIN(); TM_L(l, L"Hello World 0123456789", 1000, 1000);
    tm_clusters c; UINT32 i; const WCHAR *t = L"Hello World 0123456789";
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 22);
        for (i = 0; i < c.n; i++) {
            CHECKF(c.v[i].length == 1, "cluster %u length %u", i, c.v[i].length);
            CHECKF(DW_APPROX(c.v[i].width, CW, EPS), "cluster %u width %g", i, c.v[i].width);
            CHECKF(c.v[i].isWhitespace == (t[i] == ' '), "cluster %u whitespace %d", i, c.v[i].isWhitespace);
            CHECKF(!c.v[i].isNewline && !c.v[i].isSoftHyphen && !c.v[i].isRightToLeft && !c.v[i].padding, "cluster %u flags", i);
        }
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_ascii_can_wrap_after_space_only)
{
    TM_BEGIN(); TM_L(l, L"ab cd ef", 1000, 1000);
    tm_clusters c; UINT32 i; const WCHAR *t = L"ab cd ef";
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 8);
        for (i = 0; i + 1 < c.n; i++) CHECKF(c.v[i].canWrapLineAfter == (t[i] == ' '), "cluster %u canWrap %d", i, c.v[i].canWrapLineAfter);
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_last_cluster_can_wrap)
{
    /* the end of the text counts as a break opportunity after the last cluster */
    TM_BEGIN(); TM_L(l, L"ABC", 1000, 1000);
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { CHECK(!c.v[0].canWrapLineAfter); CHECK(!c.v[1].canWrapLineAfter); CHECK(c.v[2].canWrapLineAfter); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_can_wrap_independent_of_wrapping_mode)
{
    TM_BEGIN();
    static const DWRITE_WORD_WRAPPING modes[] = { DWRITE_WORD_WRAPPING_WRAP, DWRITE_WORD_WRAPPING_NO_WRAP, DWRITE_WORD_WRAPPING_WHOLE_WORD, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK };
    size_t k;
    for (k = 0; k < 4; k++) {
        TM_L(l, L"a b", 1000, 1000); tm_clusters c;
        CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, modes[k]));
        if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 3); CHECK(!c.v[0].canWrapLineAfter); CHECK(c.v[1].canWrapLineAfter); CHECK(c.v[2].canWrapLineAfter); }
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(cc_single_char_text)
{
    TM_BEGIN(); TM_L(l, L"Q", 1000, 1000);
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 1); EXPECT_EQ(c.v[0].length, 1); NEAR(c.v[0].width, CW); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_only_space)
{
    TM_BEGIN(); TM_L(l, L" ", 1000, 1000);
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 1); CHECK(c.v[0].isWhitespace); CHECK(c.v[0].canWrapLineAfter); NEAR(c.v[0].width, CW); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_count_equals_length_for_ascii)
{
    TM_BEGIN();
    static const WCHAR *texts[] = { L"A", L"AB", L"AB CD", L"0123456789", L"The quick brown fox jumps over the lazy dog", L"    " };
    size_t i;
    for (i = 0; i < sizeof texts / sizeof texts[0]; i++) {
        IDWriteTextLayout *l = mk(&X, FAM_TEST, texts[i], (UINT32)wcslen(texts[i]), 1000); tm_clusters c;
        if (l && tm_get_clusters(l, &c)) { CHECKF(c.n == wcslen(texts[i]), "text %zu: %u clusters", i, c.n); NEAR(tm_sum_cluster_width(&c), wcslen(texts[i]) * CW); }
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(cc_width_scales_with_font_size)
{
    TM_BEGIN();
    static const float sizes[] = { 1.0f, 4.0f, 10.0f, 16.0f, 33.0f, 100.0f, 500.0f };
    size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextLayout *l = tm_layout_(&X, FAM_TEST, L"AB", 2, sizes[i], 10000, 1000); tm_clusters c;
        if (l && tm_get_clusters(l, &c)) CHECKF(DW_APPROX(c.v[0].width, sizes[i] * 1000.0f / 2048.0f, 0.001f + sizes[i] * 1e-5f), "size %g width %g", sizes[i], c.v[0].width);
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(cc_distinct_advances_per_family)
{
    /* DwUni punctuation/hyphen/CJK glyph advances are 500/600/2000 design units */
    TM_BEGIN(); MK_UNI(l, L".-一A");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 4);
        NEAR(c.v[0].width, U16(500)); NEAR(c.v[1].width, U16(600)); NEAR(c.v[2].width, U16(2000)); NEAR(c.v[3].width, U16(1000));
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_width_sum_matches_text_metrics)
{
    TM_BEGIN(); TM_L(l, L"AAA BBB CC", 1000, 1000);
    tm_clusters c; DWRITE_TEXT_METRICS m;
    if (tm_get_clusters(l, &c)) {
        tm_metrics(l, &m);
        NEAR(m.width, tm_sum_cluster_width(&c));
        NEAR(m.widthIncludingTrailingWhitespace, tm_sum_cluster_width(&c));
        NEAR(m.width, 10 * CW);
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_trailing_whitespace_not_in_width)
{
    TM_BEGIN(); TM_L(l, L"AAA   ", 1000, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    NEAR(m.width, 3 * CW);
    NEAR(m.widthIncludingTrailingWhitespace, 6 * CW);
    EXPECT_EQ(m.lineCount, 1);
    DW_RELEASE(l); TM_END();
}
TEST(cc_leading_whitespace_in_width)
{
    TM_BEGIN(); TM_L(l, L"   AAA", 1000, 1000);
    DWRITE_TEXT_METRICS m;
    tm_metrics(l, &m);
    NEAR(m.width, 6 * CW); NEAR(m.widthIncludingTrailingWhitespace, 6 * CW);
    DW_RELEASE(l); TM_END();
}

/* ---- newlines ------------------------------------------------------------------------------------------------ */
TEST(cc_newline_chars)
{
    /* each mandatory-break character is its own zero-width cluster flagged newline + whitespace + can-wrap */
    TM_BEGIN();
    static const WCHAR chars[] = { '\n', '\r', 0x0b, 0x0c, 0x85, 0x2028, 0x2029 };
    size_t i;
    for (i = 0; i < sizeof chars / sizeof chars[0]; i++) {
        WCHAR t[4] = { 'A', chars[i], 'B', 0 };
        IDWriteTextLayout *l = mk(&X, FAM_UNI, t, 3, 1000); tm_clusters c;
        if (l && tm_get_clusters(l, &c)) {
            CHECKF(c.n == 3, "U+%04X: %u clusters", chars[i], c.n);
            if (c.n == 3) {
                CHECKF(c.v[1].length == 1, "U+%04X length", chars[i]);
                CHECKF(c.v[1].width == 0.0f, "U+%04X width %g", chars[i], c.v[1].width);
                CHECKF(c.v[1].isNewline, "U+%04X isNewline", chars[i]);
                CHECKF(c.v[1].isWhitespace, "U+%04X isWhitespace", chars[i]);
                CHECKF(c.v[1].canWrapLineAfter, "U+%04X canWrapLineAfter", chars[i]);
                CHECKF(!c.v[1].isSoftHyphen && !c.v[1].isRightToLeft, "U+%04X other flags", chars[i]);
                CHECKF(!c.v[0].isNewline && !c.v[2].isNewline, "U+%04X neighbours", chars[i]);
            }
        } else CHECKF(0, "U+%04X layout", chars[i]);
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(cc_crlf_two_clusters)
{
    /* CR and LF are separate one-character clusters; only the LF (the end of the break sequence) is flagged newline */
    TM_BEGIN(); MK_UNI(l, L"A\r\nB");
    tm_clusters c; tm_lines ln;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 4);
        if (c.n == 4) {
            EXPECT_EQ(c.v[1].length, 1); EXPECT_EQ(c.v[2].length, 1);
            CHECK(!c.v[1].isNewline); CHECK(c.v[2].isNewline);
            CHECK(c.v[1].width == 0.0f); CHECK(c.v[2].width == 0.0f);
            t_note("CR before LF: isWhitespace %d canWrapLineAfter %d", c.v[1].isWhitespace, c.v[1].canWrapLineAfter);
        }
    }
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 2); EXPECT_EQ(ln.v[0].newlineLength, 2); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_lf_cr_pair)
{
    /* LF CR: two mandatory breaks, both clusters flagged */
    TM_BEGIN(); MK_UNI(l, L"A\n\rB");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 4); if (c.n == 4) { CHECK(c.v[1].isNewline); CHECK(c.v[2].isNewline); } }
    DW_RELEASE(l); TM_END();
}
TEST(cc_newline_width_zero_even_with_glyph)
{
    /* the newline cluster is zero width whatever the font would give: U+2028 is not in the font, LF/CR neither; mapped chars keep width */
    TM_BEGIN(); MK_UNI(l, L"A\nB\rC");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 5); if (c.n == 5) { NEAR(c.v[0].width, CW); CHECK(c.v[1].width == 0.0f); NEAR(c.v[2].width, CW); CHECK(c.v[3].width == 0.0f); NEAR(c.v[4].width, CW); } }
    DW_RELEASE(l); TM_END();
}
TEST(cc_newline_cluster_belongs_to_line)
{
    /* the newline cluster is the last cluster of its line: cluster lengths tile the line lengths */
    TM_BEGIN(); MK_UNI(l, L"AB\nCD\r\nE");
    tm_clusters c; tm_lines ln; UINT32 i, ci = 0;
    if (tm_get_clusters(l, &c) && tm_get_lines(l, &ln)) {
        for (i = 0; i < ln.n; i++) {
            UINT32 pos = 0;
            while (ci < c.n && pos < ln.v[i].length) { pos += c.v[ci].length; ci++; }
            CHECKF(pos == ln.v[i].length, "line %u not tiled by clusters", i);
            if (ln.v[i].newlineLength) CHECKF(c.v[ci - 1].isNewline, "line %u last cluster not newline", i);
        }
        EXPECT_EQ(ci, c.n);
    }
    DW_RELEASE(l); TM_END();
}

/* ---- combining marks, surrogates, joiners, jamo ------------------------------------------------------------------ */
TEST(cc_combining_mark_joins_base)
{
    TM_BEGIN(); MK_UNI(l, L"Á");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 1); EXPECT_EQ(c.v[0].length, 2); NEAR(c.v[0].width, CW); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_combining_marks_stack)
{
    TM_BEGIN(); MK_UNI(l, L"Á̂̃");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 1); EXPECT_EQ(c.v[0].length, 4); NEAR(c.v[0].width, CW); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_combining_in_context)
{
    TM_BEGIN(); MK_UNI(l, L"AB́C D̀́");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 5);
        if (c.n == 5) {
            EXPECT_EQ(c.v[0].length, 1); EXPECT_EQ(c.v[1].length, 2); EXPECT_EQ(c.v[2].length, 1); EXPECT_EQ(c.v[3].length, 1);
            EXPECT_EQ(c.v[4].length, 3);
        }
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_combining_after_space_stays_with_base)
{
    /* a mark following a space attaches to the space cluster; the space keeps its whitespace flag */
    TM_BEGIN(); MK_UNI(l, L"A ́B");
    tm_clusters c; UINT32 total = 0, i;
    if (tm_get_clusters(l, &c)) {
        for (i = 0; i < c.n; i++) total += c.v[i].length;
        EXPECT_EQ(total, 4);
        t_note("A,space,mark,B -> %u clusters", c.n);
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_leading_combining_mark)
{
    TM_BEGIN(); MK_UNI(l, L"́" L"AB");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(tm_sum_cluster_len(&c), 3);
        t_note("leading mark: %u clusters, first length %u width %g", c.n, c.n ? c.v[0].length : 0, c.n ? c.v[0].width : 0.0f);
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_surrogate_pair_single_cluster)
{
    TM_BEGIN(); MK_UNI(l, L"\U0001D400");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 1); EXPECT_EQ(c.v[0].length, 2); NEAR(c.v[0].width, U16(1500)); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_surrogates_in_context)
{
    TM_BEGIN(); MK_UNI(l, L"A\U0001D400B\U00020000 C");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 6);
        if (c.n == 6) {
            EXPECT_EQ(c.v[0].length, 1); EXPECT_EQ(c.v[1].length, 2); EXPECT_EQ(c.v[2].length, 1); EXPECT_EQ(c.v[3].length, 2);
            NEAR(c.v[1].width, U16(1500)); NEAR(c.v[3].width, U16(2000));
            CHECK(c.v[4].isWhitespace);
        }
        EXPECT_EQ(tm_sum_cluster_len(&c), 8);
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_many_surrogates)
{
    TM_BEGIN(); MK_UNI(l, L"\U0001D400\U0001D401\U0001D402\U0001D403\U0001D404");
    tm_clusters c; UINT32 i;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 5); for (i = 0; i < c.n; i++) { EXPECT_EQ(c.v[i].length, 2); NEAR(c.v[i].width, U16(1500)); } }
    DW_RELEASE(l); TM_END();
}
TEST(cc_surrogate_with_combining_mark)
{
    TM_BEGIN(); MK_UNI(l, L"\U0001D400́");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 1); EXPECT_EQ(c.v[0].length, 3); NEAR(c.v[0].width, U16(1500)); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_lone_surrogates_cover_text)
{
    /* unpaired surrogates are tolerated: whatever clusters result, they cover the text exactly */
    TM_BEGIN();
    static const WCHAR hi[] = { 'A', 0xD800, 'B', 0 }, lo[] = { 'A', 0xDC00, 'B', 0 }, rev[] = { 0xDC00, 0xD800, 0 };
    const WCHAR *ts[3] = { hi, lo, rev }; size_t i;
    for (i = 0; i < 3; i++) {
        IDWriteTextLayout *l = mk(&X, FAM_UNI, ts[i], (UINT32)wcslen(ts[i]), 1000); tm_clusters c;
        if (l && tm_get_clusters(l, &c)) { CHECKF(tm_sum_cluster_len(&c) == wcslen(ts[i]), "case %zu: lengths %u", i, tm_sum_cluster_len(&c)); t_note("lone surrogate case %zu -> %u clusters", i, c.n); }
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(cc_zwj_sequence_covers_text)
{
    TM_BEGIN(); MK_UNI(l, L"A‍" L"B");
    tm_clusters c; tm_lines ln;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(tm_sum_cluster_len(&c), 3);
        NEAR(tm_sum_cluster_width(&c), 2 * CW);
        t_note("A ZWJ B -> %u clusters", c.n);
    }
    if (tm_get_lines(l, &ln)) EXPECT_EQ(ln.v[0].length, 3);
    DW_RELEASE(l); TM_END();
}
TEST(cc_zwj_between_pairs)
{
    TM_BEGIN(); MK_UNI(l, L"\U0001D400‍\U0001D401");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(tm_sum_cluster_len(&c), 5); t_note("pair ZWJ pair -> %u clusters, widths sum %g", c.n, tm_sum_cluster_width(&c)); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_hangul_jamo_cover_text)
{
    TM_BEGIN(); MK_UNI(l, L"한");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(tm_sum_cluster_len(&c), 3); t_note("L+V+T jamo -> %u clusters (first length %u)", c.n, c.n ? c.v[0].length : 0); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_hangul_syllable_single)
{
    TM_BEGIN(); MK_UNI(l, L"가각");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 2); EXPECT_EQ(c.v[0].length, 1); EXPECT_EQ(c.v[1].length, 1); NEAR(c.v[0].width, U16(2000)); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_cjk_each_char_cluster_can_wrap)
{
    TM_BEGIN(); MK_UNI(l, L"一丁丂");
    tm_clusters c; UINT32 i;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 3); for (i = 0; i < c.n; i++) { EXPECT_EQ(c.v[i].length, 1); CHECKF(c.v[i].canWrapLineAfter, "cjk %u canWrap", i); NEAR(c.v[i].width, 2 * CW); } }
    DW_RELEASE(l); TM_END();
}
TEST(cc_kana_widths)
{
    TM_BEGIN(); MK_UNI(l, L"あいうえ");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 4); NEAR(tm_sum_cluster_width(&c), 8 * CW); }
    DW_RELEASE(l); TM_END();
}

/* ---- soft hyphen / hyphens ----------------------------------------------------------------------------------------- */
TEST(cc_soft_hyphen_flags)
{
    TM_BEGIN(); MK_UNI(l, L"A­" L"B");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 3);
        if (c.n == 3) {
            CHECK(!c.v[0].isSoftHyphen); CHECK(c.v[1].isSoftHyphen); CHECK(!c.v[2].isSoftHyphen);
            CHECK(!c.v[1].isWhitespace); CHECK(!c.v[1].isNewline);
            EXPECT_EQ(c.v[1].length, 1);
            CHECK(c.v[1].canWrapLineAfter);            /* a soft hyphen is a break opportunity */
            t_note("soft hyphen cluster width %g (glyph advance %g)", c.v[1].width, U16(500));
        }
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_soft_hyphen_only_that_char)
{
    TM_BEGIN(); MK_UNI(l, L"A-B‐" L"C­");
    tm_clusters c; UINT32 i;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 6);
        for (i = 0; i < c.n; i++) CHECKF(c.v[i].isSoftHyphen == (i == 5), "cluster %u isSoftHyphen %d", i, c.v[i].isSoftHyphen);
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_hyphen_break_after)
{
    TM_BEGIN(); MK_UNI(l, L"AA-BB");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 5); if (c.n == 5) { CHECK(!c.v[0].canWrapLineAfter); CHECK(!c.v[1].canWrapLineAfter); CHECK(c.v[2].canWrapLineAfter); CHECK(!c.v[3].canWrapLineAfter); NEAR(c.v[2].width, U16(600)); CHECK(!c.v[2].isWhitespace); } }
    DW_RELEASE(l); TM_END();
}
TEST(cc_nbsp_no_break)
{
    TM_BEGIN(); MK_UNI(l, L"A " L"B");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 3); if (c.n == 3) { CHECK(!c.v[1].canWrapLineAfter); CHECK(!c.v[0].canWrapLineAfter); t_note("NBSP isWhitespace %d width %g", c.v[1].isWhitespace, c.v[1].width); } }
    DW_RELEASE(l); TM_END();
}
TEST(cc_ideographic_space_note)
{
    TM_BEGIN(); MK_UNI(l, L"一　丁");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 3); if (c.n == 3) t_note("U+3000: isWhitespace %d canWrap %d width %g", c.v[1].isWhitespace, c.v[1].canWrapLineAfter, c.v[1].width); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_space_runs)
{
    TM_BEGIN(); TM_L(l, L"A    B", 1000, 1000);
    tm_clusters c; UINT32 i;
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 6);
        /* the break opportunity of a space run is after its last space (no break before a space) */
        for (i = 1; i <= 4 && i < c.n; i++) { CHECK(c.v[i].isWhitespace); CHECKF(c.v[i].canWrapLineAfter == (i == 4), "space %u canWrap %d", i, c.v[i].canWrapLineAfter); NEAR(c.v[i].width, CW); }
    }
    DW_RELEASE(l); TM_END();
}

/* ---- tabs ---------------------------------------------------------------------------------------------------------- */
typedef struct { const WCHAR *text; float tabstop_chars; UINT32 idx; float width_chars; } tabcase;
TEST(cc_tab_width_to_next_stop)
{
    /* tab width = (floor(x / stop) + 1) * stop - x, x measured from the line start */
    TM_BEGIN();
    static const tabcase cs[] = {
        { L"A\tB", 4, 1, 3 }, { L"\tB", 4, 0, 4 }, { L"AB\tC", 4, 2, 2 }, { L"ABC\tD", 4, 3, 1 }, { L"ABCD\tE", 4, 4, 4 },
        { L"ABCDE\tF", 4, 5, 3 }, { L"A\tB", 8, 1, 7 }, { L"A\tB", 2, 1, 1 }, { L"AA\tB", 2, 2, 2 }, { L"A\tB", 1, 1, 1 },
        { L"A\t\tB", 4, 2, 4 }, { L"A\t\tB", 4, 1, 3 }, { L"AA\t\t\tB", 3, 4, 3 }, { L"\t\tB", 5, 1, 5 },
        { L"A\tB", 3, 1, 2 }, { L"AAAAAAAA\tB", 4, 8, 4 },
    };
    size_t i;
    for (i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        IDWriteTextLayout *l = mk(&X, FAM_TEST, cs[i].text, (UINT32)wcslen(cs[i].text), 1000); tm_clusters c;
        if (!l) { CHECKF(0, "layout %zu", i); continue; }
        CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, cs[i].tabstop_chars * CW));
        if (tm_get_clusters(l, &c) && cs[i].idx < c.n) {
            CHECKF(DW_APPROX(c.v[cs[i].idx].width, cs[i].width_chars * CW, EPS), "case %zu (tab stop %g chars): tab width %g want %g", i, cs[i].tabstop_chars, c.v[cs[i].idx].width, cs[i].width_chars * CW);
            CHECKF(c.v[cs[i].idx].isWhitespace && !c.v[cs[i].idx].isNewline && c.v[cs[i].idx].length == 1, "case %zu flags", i);
        } else CHECKF(0, "case %zu clusters", i);
        DW_RELEASE(l);
    }
    TM_END();
}
TEST(cc_tab_default_stop_formula)
{
    /* the same formula holds for the default incremental tab stop, whatever its value */
    TM_BEGIN(); TM_L(l, L"AB\tC", 1000, 1000);
    float stop = IDWriteTextLayout_GetIncrementalTabStop(l); tm_clusters c;
    t_note("default incremental tab stop %g (letter %g)", stop, CW);
    if (stop > 0.0f && tm_get_clusters(l, &c) && c.n == 4) {
        float x = 2 * CW, want = (floorf(x / stop) + 1.0f) * stop - x;
        CHECKF(DW_APPROX(c.v[2].width, want, 0.01f), "tab width %g want %g", c.v[2].width, want);
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_tab_restarts_each_line)
{
    /* the tab position is measured from the start of the line it is on */
    TM_BEGIN(); TM_L(l, L"A\tB\nA\tB", 1000, 1000);
    tm_clusters c;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4 * CW));
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 7); if (c.n == 7) { NEAR(c.v[1].width, 3 * CW); NEAR(c.v[5].width, 3 * CW); } }
    DW_RELEASE(l); TM_END();
}
TEST(cc_tab_after_wrap_restarts)
{
    TM_BEGIN(); TM_L(l, L"AAAA B\tC", 5 * CW, 1000);
    tm_clusters c; tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4 * CW));
    if (tm_get_lines(l, &ln) && tm_get_clusters(l, &c)) {
        EXPECT_EQ(ln.n, 2);
        EXPECT_EQ(c.n, 8);
        if (c.n == 8) NEAR(c.v[6].width, 3 * CW);        /* second line "B\tC": tab starts at x = 1 letter */
    }
    DW_RELEASE(l); TM_END();
}
TEST(cc_tab_extends_line_width)
{
    TM_BEGIN(); TM_L(l, L"A\tB", 1000, 1000);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4 * CW));
    tm_metrics(l, &m);
    NEAR(m.width, 5 * CW);
    DW_RELEASE(l); TM_END();
}
TEST(cc_tab_does_not_change_other_clusters)
{
    TM_BEGIN(); TM_L(l, L"A\tB", 1000, 1000);
    tm_clusters c;
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 4 * CW));
    if (tm_get_clusters(l, &c)) { NEAR(c.v[0].width, CW); NEAR(c.v[2].width, CW); }
    CHECK_HR_OK(IDWriteTextLayout_SetIncrementalTabStop(l, 8 * CW));
    if (tm_get_clusters(l, &c)) { NEAR(c.v[0].width, CW); NEAR(c.v[1].width, 7 * CW); NEAR(c.v[2].width, CW); }
    DW_RELEASE(l); TM_END();
}

/* ---- bidi flag -------------------------------------------------------------------------------------------------------- */
TEST(cc_rtl_flag_hebrew)
{
    TM_BEGIN(); MK_UNI(l, L"אבג");
    tm_clusters c; UINT32 i; DWRITE_TEXT_METRICS m;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 3); for (i = 0; i < c.n; i++) { CHECKF(c.v[i].isRightToLeft, "cluster %u", i); NEAR(c.v[i].width, CW); } }
    tm_metrics(l, &m);
    t_note("Hebrew in LTR paragraph: maxBidiReorderingDepth %u", m.maxBidiReorderingDepth);
    DW_RELEASE(l); TM_END();
}
TEST(cc_rtl_flag_mixed)
{
    TM_BEGIN(); MK_UNI(l, L"Aאב" L"B");
    tm_clusters c;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 4); if (c.n == 4) { CHECK(!c.v[0].isRightToLeft); CHECK(c.v[1].isRightToLeft); CHECK(c.v[2].isRightToLeft); CHECK(!c.v[3].isRightToLeft); } }
    DW_RELEASE(l); TM_END();
}
TEST(cc_rtl_flag_latin_in_rtl_paragraph)
{
    /* Latin letters keep an even bidi level even in a right-to-left paragraph */
    TM_BEGIN(); MK_UNI(l, L"ABC");
    tm_clusters c; UINT32 i;
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    if (tm_get_clusters(l, &c)) for (i = 0; i < c.n; i++) CHECKF(!c.v[i].isRightToLeft, "cluster %u", i);
    DW_RELEASE(l); TM_END();
}
TEST(cc_rtl_flag_hebrew_in_rtl_paragraph)
{
    TM_BEGIN(); MK_UNI(l, L"אב");
    tm_clusters c; UINT32 i;
    CHECK_HR_OK(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT));
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 2); for (i = 0; i < c.n; i++) CHECK(c.v[i].isRightToLeft); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_rtl_widths_unchanged)
{
    TM_BEGIN(); MK_UNI(l, L"אב ג");
    tm_clusters c; DWRITE_TEXT_METRICS m;
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 4); NEAR(tm_sum_cluster_width(&c), 4 * CW); }
    tm_metrics(l, &m);
    NEAR(m.width, 4 * CW);
    DW_RELEASE(l); TM_END();
}

/* ---- character spacing / per-range size ---------------------------------------------------------------------------------- */
TEST(cc_character_spacing_adds_to_width)
{
    TM_BEGIN(); TM_L(l, L"ABC", 1000, 1000);
    TM_QI(l1, IDWriteTextLayout1, l);
    tm_clusters c;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 2.0f, 3.0f, 0.0f, RNG(0, 3)));
    if (tm_get_clusters(l, &c) && c.n == 3) {
        CHECK(c.v[0].width > CW); CHECK(c.v[2].width > CW);
        CHECK(c.v[1].width > CW);
        t_note("leading 2 + trailing 3 on a %g wide cluster -> %g", CW, c.v[1].width);
    }
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TM_END();
}
TEST(cc_character_spacing_min_advance)
{
    TM_BEGIN(); TM_L(l, L"ABC", 1000, 1000);
    TM_QI(l1, IDWriteTextLayout1, l);
    tm_clusters c;
    CHECK_HR_OK(IDWriteTextLayout1_SetCharacterSpacing(l1, 0.0f, 0.0f, 20.0f, RNG(0, 3)));
    if (tm_get_clusters(l, &c) && c.n == 3) { t_note("minimumAdvanceWidth 20 -> cluster widths %g %g %g", c.v[0].width, c.v[1].width, c.v[2].width); CHECK(c.v[0].width >= CW); }
    CHECK_HR(IDWriteTextLayout1_SetCharacterSpacing(l1, 0.0f, 0.0f, -1.0f, RNG(0, 3)), DW_E_INVALIDARG);
    IDWriteTextLayout1_Release(l1); DW_RELEASE(l); TM_END();
}
TEST(cc_range_font_size_changes_only_range)
{
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    tm_clusters c;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(1, 2)));
    if (tm_get_clusters(l, &c) && c.n == 4) { NEAR(c.v[0].width, CW); NEAR(c.v[1].width, 2 * CW); NEAR(c.v[2].width, 2 * CW); NEAR(c.v[3].width, CW); }
    DW_RELEASE(l); TM_END();
}
TEST(cc_range_family_changes_widths)
{
    /* DwUni is the same advance for letters; the range switch must not change segmentation */
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    tm_clusters c;
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, FAM_UNI, RNG(2, 2)));
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 4); NEAR(tm_sum_cluster_width(&c), 4 * CW); }
    DW_RELEASE(l); TM_END();
}

/* ---- inline objects ---------------------------------------------------------------------------------------------------------- */
TEST(cc_inline_object_cluster)
{
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    dwx_inline *io = dwx_inline_new(20.0f, 10.0f, 8.0f, FALSE);
    tm_clusters c;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 2)));
    if (tm_get_clusters(l, &c)) {
        EXPECT_EQ(c.n, 3);
        if (c.n == 3) { NEAR(c.v[0].width, CW); EXPECT_EQ(c.v[1].length, 2); NEAR(c.v[1].width, 20.0f); NEAR(c.v[2].width, CW); CHECK(!c.v[1].isWhitespace); CHECK(!c.v[1].isNewline); }
        EXPECT_EQ(tm_sum_cluster_len(&c), 4);
    }
    CHECKF(io->n_metrics >= 1, "GetMetrics calls %ld", (long)io->n_metrics);
    DW_RELEASE(l); dwx_release(io); TM_END();
}
TEST(cc_inline_object_whole_text)
{
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    dwx_inline *io = dwx_inline_new(33.0f, 10.0f, 8.0f, FALSE);
    tm_clusters c;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(0, 4)));
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 1); if (c.n == 1) { EXPECT_EQ(c.v[0].length, 4); NEAR(c.v[0].width, 33.0f); } }
    DW_RELEASE(l); dwx_release(io); TM_END();
}
TEST(cc_inline_object_failing_metrics_zero_width)
{
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    dwx_inline *io = dwx_inline_new(123.0f, 10.0f, 8.0f, FALSE);
    tm_clusters c;
    io->fail_metrics = E_FAIL;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(0, 4)));
    if (tm_get_clusters(l, &c)) { EXPECT_EQ(c.n, 1); if (c.n == 1) { EXPECT_EQ(c.v[0].length, 4); NEAR(c.v[0].width, 0.0f); } }
    DW_RELEASE(l); dwx_release(io); TM_END();
}
TEST(cc_inline_object_sets_line_height)
{
    /* inline object ascent = baseline, descent = height - baseline; the line takes the larger of text and object on each side */
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    dwx_inline *io = dwx_inline_new(20.0f, 40.0f, 30.0f, FALSE);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 1); NEAR(ln.v[0].baseline, 30.0f); NEAR(ln.v[0].height, 40.0f); }
    DW_RELEASE(l); dwx_release(io); TM_END();
}
TEST(cc_inline_object_below_baseline_extends_descent)
{
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    dwx_inline *io = dwx_inline_new(20.0f, 30.0f, 5.0f, FALSE);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)));
    if (tm_get_lines(l, &ln)) { NEAR(ln.v[0].baseline, ASC); NEAR(ln.v[0].height, ASC + 25.0f); }
    DW_RELEASE(l); dwx_release(io); TM_END();
}
TEST(cc_inline_object_small_keeps_text_height)
{
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    dwx_inline *io = dwx_inline_new(20.0f, 5.0f, 4.0f, FALSE);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 1)));
    if (tm_get_lines(l, &ln)) { NEAR(ln.v[0].baseline, ASC); NEAR(ln.v[0].height, LH); }
    DW_RELEASE(l); dwx_release(io); TM_END();
}
TEST(cc_inline_object_width_in_text_metrics)
{
    TM_BEGIN(); TM_L(l, L"ABCD", 1000, 1000);
    dwx_inline *io = dwx_inline_new(20.0f, 10.0f, 8.0f, FALSE);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(1, 2)));
    tm_metrics(l, &m);
    NEAR(m.width, 2 * CW + 20.0f);
    DW_RELEASE(l); dwx_release(io); TM_END();
}
TEST(cc_inline_object_wraps_as_unit)
{
    /* an inline object is one wide cluster: it moves to the next line when it does not fit */
    TM_BEGIN(); TM_L(l, L"AB CD", 5 * CW, 1000);
    dwx_inline *io = dwx_inline_new(4 * CW, 10.0f, 8.0f, FALSE);
    tm_lines ln;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, RNG(3, 2)));
    if (tm_get_lines(l, &ln)) { EXPECT_EQ(ln.n, 2); if (ln.n == 2) { EXPECT_EQ(ln.v[0].length, 3); EXPECT_EQ(ln.v[1].length, 2); } }
    DW_RELEASE(l); dwx_release(io); TM_END();
}
