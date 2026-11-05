/* IDWriteTextLayout::GetClusterMetrics on bidirectional text: buffer protocol, logical order, isRightToLeft, cluster formation.
 *
 * Facts (DWriteTextLayout::GetClusterMetrics -> TextLayout::GetClusterMetrics):
 *  - *actualClusterCount is zeroed first; a NULL buffer with a non-zero capacity is E_INVALIDARG (count stays 0);
 *  - otherwise the layout is brought up to date, the metrics of up to `capacity` clusters are written and the full count is
 *    returned in *actualClusterCount; capacity < count => HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) (a capacity of 0 with
 *    any text included);
 *  - clusters come in logical (text) order, each starting where the previous ended, whatever their visual placement.
 */
#include "bx.h"

#define P(s) s

static void protocol(const WCHAR *text, int base)
{
    BX_BEGIN(); BX_LAYOUT(l, text, BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS full[64], buf[64]; UINT32 n, cnt, i; HRESULT hr;
    if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_clusters(l, full, 64);
    CHECK(n >= 1);
    /* NULL buffer, zero capacity: reports the needed count with the insufficient-buffer error */
    cnt = 0x1234; hr = IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &cnt);
    CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(cnt == n);
    /* non-NULL buffer, zero capacity */
    memset(buf, 0xcc, sizeof buf); cnt = 0x1234; hr = IDWriteTextLayout_GetClusterMetrics(l, buf, 0, &cnt);
    CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(cnt == n);
    CHECK(((unsigned char *)buf)[0] == 0xcc);                      /* nothing written */
    /* NULL buffer with a capacity: invalid argument, count zeroed */
    cnt = 0x1234; hr = IDWriteTextLayout_GetClusterMetrics(l, NULL, 5, &cnt);
    CHECK_HR(hr, DW_E_INVALIDARG); CHECK(cnt == 0);
    /* exact capacity */
    memset(buf, 0xcc, sizeof buf); cnt = 0;
    CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(l, buf, n, &cnt)); CHECK(cnt == n);
    CHECK(!memcmp(buf, full, n * sizeof buf[0]));
    /* one short: the first n-1 are filled, the last is not touched, error reported with the full count */
    if (n >= 2) {
        memset(buf, 0xcc, sizeof buf); cnt = 0;
        hr = IDWriteTextLayout_GetClusterMetrics(l, buf, n - 1, &cnt);
        CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(cnt == n);
        CHECK(!memcmp(buf, full, (n - 1) * sizeof buf[0]));
        for (i = 0; i < sizeof buf[0]; i++) CHECK(((unsigned char *)&buf[n - 1])[i] == 0xcc);
    }
    /* larger than needed: only n entries written */
    memset(buf, 0xcc, sizeof buf); cnt = 0;
    CHECK_HR_OK(IDWriteTextLayout_GetClusterMetrics(l, buf, 64, &cnt)); CHECK(cnt == n);
    for (i = 0; i < sizeof buf[0]; i++) CHECK(((unsigned char *)&buf[n])[i] == 0xcc);
    BX_DONE(l);
}
#define PROTO(nm, text, base) TEST(r2_layout_bidi_reorder_cluster_protocol_##nm) { protocol(text, base); }
PROTO(latin,          L"abcd", 0)
PROTO(hebrew,         L"\u05d0\u05d1\u05d2", 0)
PROTO(hebrew_rtl,     L"\u05d0\u05d1\u05d2", 1)
PROTO(mixed,          L"ab \u05d0\u05d1\u05d2 cd", 0)
PROTO(mixed_rtl,      L"ab \u05d0\u05d1\u05d2 cd", 1)
PROTO(single,         L"\u05d0", 0)
PROTO(arabic_digits,  L"\u0627\u0628 123", 0)
PROTO(marks,          L"a\u0301\u0302b", 0)
PROTO(surrogates,     L"a\U00010800b", 0)
PROTO(tab_and_space,  L"\u05d0\t\u05d1 \u05d2", 1)

/* ---- logical order and partition ---------------------------------------------------------------------------- */
static void partition(const WCHAR *text, int base)
{
    BX_BEGIN(); BX_LAYOUT(l, text, BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[64]; UINT32 n, i, pos = 0, len = (UINT32)wcslen(text);
    DWRITE_HIT_TEST_METRICS hm;
    if (base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_clusters(l, cm, 64);
    float sum = 0;
    for (i = 0; i < n; i++) {
        CHECK(cm[i].length >= 1);
        /* the cluster starting at `pos` is the one hit testing reports for that position */
        bx_tp q = bx_pos(l, pos, FALSE); hm = q.m;
        CHECKF(hm.textPosition == pos && hm.length == cm[i].length, "cluster %u (%u,+%u) vs hit-test (%u,+%u)", i, pos, cm[i].length, hm.textPosition, hm.length);
        CHK_NEAR(hm.width, cm[i].width);
        pos += cm[i].length; sum += cm[i].width;
    }
    CHECK(pos == len);
    {   DWRITE_TEXT_METRICS tm; bx_metrics(l, &tm); CHK_NEAR(sum, tm.widthIncludingTrailingWhitespace); }
    BX_DONE(l);
}
#define PART(nm, text, base) TEST(r2_layout_bidi_reorder_cluster_partition_##nm) { partition(text, base); }
PART(mixed_ltr,    L"ab \u05d0\u05d1\u05d2 cd", 0)
PART(mixed_rtl,    L"ab \u05d0\u05d1\u05d2 cd", 1)
PART(hebrew_digits,L"\u05d0\u05d1 123 \u05d2", 0)
PART(arabic_latin, L"\u0627\u0628\u062c abc \u062f", 1)
PART(marks_hebrew, L"\u05d0\u05b0\u05d1\u05b1\u05b2", 0)
PART(marks_latin,  L"a\u0301 b\u0302\u0303", 0)
PART(surrogate_rtl,L"\U00010800\U00010801 a", 0)
PART(surrogate_ltr,L"a\U00010000b", 1)

/* ---- cluster formation: combining marks, surrogate pairs, ZWJ -------------------------------------------------- */
typedef struct { const WCHAR *text; UINT32 nclusters; UINT16 lens[8]; int soft; } form_row;
static void formation(const form_row *r)
{
    if (r->soft) {   /* grouping not fixed by the documentation: recorded, only the partition is checked */
        BX_BEGIN(); BX_LAYOUT(l, r->text, BX_W, 100.0f);
        DWRITE_CLUSTER_METRICS cm[16]; UINT32 n = bx_clusters(l, cm, 16), i, pos = 0; char b[100]; int k = 0;
        for (i = 0; i < n; i++) { pos += cm[i].length; k += snprintf(b + k, sizeof b - k - 1, "%u ", cm[i].length); }
        CHECK(pos == (UINT32)wcslen(r->text));
        t_note("cluster lengths %s(expected %u clusters)", b, r->nclusters);
        BX_DONE(l); return;
    }
    BX_BEGIN(); BX_LAYOUT(l, r->text, BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[16]; UINT32 n, i;
    n = bx_clusters(l, cm, 16);
    CHECKF(n == r->nclusters, "cluster count %u want %u", n, r->nclusters);
    for (i = 0; i < n && i < r->nclusters; i++) CHECKF(cm[i].length == r->lens[i], "cluster %u length %u want %u", i, cm[i].length, r->lens[i]);
    BX_DONE(l);
}
#define FORM(nm, text, n, ...) static const form_row form_##nm = { text, n, { __VA_ARGS__ }, 0 }; TEST(r2_layout_bidi_reorder_cluster_formation_##nm) { formation(&form_##nm); }
#define FORMS(nm, text, n, ...) static const form_row form_##nm = { text, n, { __VA_ARGS__ }, 1 }; TEST(r2_layout_bidi_reorder_cluster_formation_##nm) { formation(&form_##nm); }
FORM(plain_latin,        L"abc", 3, 1, 1, 1)
FORM(latin_mark,         L"a\u0301b", 2, 2, 1)
FORM(latin_two_marks,    L"a\u0301\u0302b", 2, 3, 1)
FORM(latin_mark_end,     L"ba\u0301", 2, 1, 2)
FORM(hebrew_point,       L"\u05d0\u05b0\u05d1", 2, 2, 1)
FORM(hebrew_two_points,  L"\u05d0\u05b0\u05b1\u05d1", 3, 2, 1, 1)   /* only the first point joins the letter */
FORM(arabic_mark,        L"\u0627\u064e\u0628", 2, 2, 1)
FORM(arabic_two_marks,   L"\u0627\u064e\u064f\u0628", 2, 3, 1)
FORM(surrogate_alone,    L"\U00010800", 1, 2)
FORM(surrogate_between,  L"a\U00010000b", 3, 1, 2, 1)
FORM(surrogate_two,      L"\U00010800\U00010801", 2, 2, 2)
FORM(surrogate_hebrew,   L"\u05d0\U00010800\u05d1", 3, 1, 2, 1)
FORMS(surrogate_mark,     L"\U00010800\u05b0", 1, 3)
FORM(combining_after_space, L"a \u0301b", 3, 1, 2, 1)
FORMS(crlf,               L"a\r\nb", 3, 1, 2, 1)
FORM(lf,                 L"a\nb", 3, 1, 1, 1)
FORM(cr,                 L"a\rb", 3, 1, 1, 1)
FORM(hebrew_lf,          L"\u05d0\n\u05d1", 3, 1, 1, 1)
FORM(tab,                L"a\tb", 3, 1, 1, 1)

TEST(r2_layout_bidi_reorder_cluster_zwj_sequences)
{
    /* Family emoji joined by ZWJ and a base with ZWNJ: only the invariants are fixed, the grouping depends on the OS segmentation. */
    static const WCHAR *texts[] = { L"\U0001F468\u200d\U0001F469", L"\U0001F468\u200d\U0001F469\u200d\U0001F466", L"a\u200db", L"\u05d0\u200d\u05d1", L"\u0627\u200c\u0628", L"a\u200cb", L"\U0001F600\U0001F3FB" };
    BX_BEGIN(); unsigned t;
    for (t = 0; t < sizeof texts / sizeof texts[0]; t++) {
        BX_LAYOUT(l, texts[t], BX_W, 100.0f);
        DWRITE_CLUSTER_METRICS cm[16]; UINT32 n = bx_clusters(l, cm, 16), i, pos = 0, len = (UINT32)wcslen(texts[t]);
        for (i = 0; i < n; i++) {
            CHECK(cm[i].length >= 1);
            /* a surrogate pair is never split */
            if (pos + cm[i].length < len && texts[t][pos + cm[i].length - 1] >= 0xd800 && texts[t][pos + cm[i].length - 1] <= 0xdbff) CHECKF(0, "cluster splits a surrogate pair at %u", pos);
            pos += cm[i].length;
        }
        CHECK(pos == len);
        { char b[200]; int k = 0; for (i = 0; i < n && k < 180; i++) k += snprintf(b + k, sizeof b - k, "%u ", cm[i].length); t_note("text #%u clusters: %s", t, b); }
        IDWriteTextLayout_Release(l);
    }
    BX_END();
}

/* ---- flags ------------------------------------------------------------------------------------------------------ */
TEST(r2_layout_bidi_reorder_cluster_flags_hebrew_words)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1 \u05d2\u05d3", BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[8]; UINT32 n = bx_clusters(l, cm, 8);
    CHECK(n == 5);
    if (n == 5) {
        CHECK(cm[2].isWhitespace); CHECK(cm[2].canWrapLineAfter);                    /* the space ends the first word */
        CHECK(!cm[0].isWhitespace && !cm[1].isWhitespace && !cm[3].isWhitespace);
        CHECK(!cm[0].canWrapLineAfter); CHECK(!cm[3].canWrapLineAfter);              /* no break inside a Hebrew word */
        CHECK(!cm[0].isNewline && !cm[2].isNewline);
        CHECK(!cm[0].isSoftHyphen);
        { UINT32 i; for (i = 0; i < n; i++) CHECK(cm[i].isRightToLeft == 1); }
    }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_cluster_flags_newline)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\n\u05d1", BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[8]; UINT32 n = bx_clusters(l, cm, 8);
    CHECK(n == 3);
    if (n == 3) { CHECK(cm[1].isNewline); CHECK(!cm[0].isNewline); CHECK(!cm[2].isNewline); CHECK(cm[1].canWrapLineAfter); }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_cluster_flags_mixed_direction)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1 cd", BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[16]; UINT32 n = bx_clusters(l, cm, 16), i;
    static const unsigned char rtl[] = { 0, 0, 0, 1, 1, 0, 0, 0 };
    CHECK(n == 8);
    for (i = 0; i < n && i < 8; i++) CHECKF(cm[i].isRightToLeft == rtl[i], "cluster %u isRightToLeft %u want %u", i, cm[i].isRightToLeft, rtl[i]);
    CHECK(cm[2].isWhitespace && cm[2].canWrapLineAfter && cm[5].isWhitespace && cm[5].canWrapLineAfter);
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_cluster_flags_rtl_paragraph_latin)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1", BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[16]; UINT32 n, i;
    static const unsigned char rtl[] = { 0, 0, 1, 1, 1 };            /* the space between Latin and Hebrew takes the paragraph level 1 */
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_clusters(l, cm, 16);
    CHECK(n == 5);
    for (i = 0; i < n && i < 5; i++) CHECKF(cm[i].isRightToLeft == rtl[i], "cluster %u isRightToLeft %u want %u", i, cm[i].isRightToLeft, rtl[i]);
    BX_DONE(l);
}

/* The cluster list is recomputed when the paragraph direction changes. */
TEST(r2_layout_bidi_reorder_cluster_flags_follow_reading_direction)
{
    BX_BEGIN(); BX_LAYOUT(l, L"abc", BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[4]; UINT32 n, i;
    n = bx_clusters(l, cm, 4); CHECK(n == 3);
    for (i = 0; i < n; i++) CHECK(!cm[i].isRightToLeft);
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    n = bx_clusters(l, cm, 4); CHECK(n == 3);
    for (i = 0; i < n; i++) CHECK(!cm[i].isRightToLeft);          /* Latin is level 2 in an RTL paragraph: still left-to-right */
    IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    n = bx_clusters(l, cm, 4); CHECK(n == 3);
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_cluster_widths_combining_zero_advance)
{
    BX_BEGIN(); BX_LAYOUT(l, L"a\u0301\u0302\u05d0\u05b0", BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[8]; UINT32 n = bx_clusters(l, cm, 8);
    CHECK(n == 2);
    if (n == 2) { CHK_NEAR(cm[0].width, CW); CHK_NEAR(cm[1].width, CW); CHECK(cm[0].length == 3 && cm[1].length == 2); CHECK(!cm[0].isRightToLeft && cm[1].isRightToLeft); }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_cluster_ranges_with_font_sizes)
{
    /* different font sizes on Hebrew and Latin runs: widths are per run, order stays logical */
    BX_BEGIN(); BX_LAYOUT(l, L"ab\u05d0\u05d1", BX_W, 100.0f);
    DWRITE_CLUSTER_METRICS cm[8]; UINT32 n;
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 32.0f, RNG(2, 2)));
    n = bx_clusters(l, cm, 8); CHECK(n == 4);
    if (n == 4) { CHK_NEAR(cm[0].width, CW); CHK_NEAR(cm[1].width, CW); CHK_NEAR(cm[2].width, 2 * CW); CHK_NEAR(cm[3].width, 2 * CW);
                  CHECK(!cm[0].isRightToLeft && cm[2].isRightToLeft && cm[3].isRightToLeft); }
    BX_DONE(l);
}
