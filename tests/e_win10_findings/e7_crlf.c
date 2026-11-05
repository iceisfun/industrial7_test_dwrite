/* CR LF handling in a text layout (synthetic font: every glyph is 1000/2048 em wide, so cluster widths are exact).
 * On Windows 10: CR and LF are two separate one-unit clusters (CR is white space, LF additionally isNewline), both zero width,
 * and they share ONE line break: the first line has length 4 of which newlineLength 2. */
#include "e7.h"

#define EM 20.0f
#define CW (EM * 1000.0f / 2048.0f)

typedef struct { UINT32 nclu, nlines; DWRITE_CLUSTER_METRICS cm[16]; DWRITE_LINE_METRICS lm[8]; int ok; } e7_lres;
static e7_lres measure(e7_lay *m, const WCHAR *t)
{
    e7_lres r; IDWriteTextLayout *l;
    memset(&r, 0, sizeof r);
    l = e7_layout(m, t, 1000.0f, 200.0f);
    if (!l) return r;
    CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, r.cm, 16, &r.nclu), S_OK);
    CHECK_HR(IDWriteTextLayout_GetLineMetrics(l, r.lm, 8, &r.nlines), S_OK);
    r.ok = 1;
    IDWriteTextLayout_Release(l);
    return r;
}

TEST(e7_crlf_clusters_and_line)
{
    e7_lay m; e7_lres r;
    if (!e7_lay_open(&m, EM)) return;
    r = measure(&m, L"AB\r\nCD");
    if (r.ok) {
        EXPECT_EQ(r.nclu, 6);
        EXPECT_EQ(r.nlines, 2);
        EXPECT_EQ(r.cm[2].length, 1); EXPECT_EQ(r.cm[3].length, 1);
        CHECK(r.cm[2].isWhitespace && !r.cm[2].isNewline);      /* CR */
        CHECK(r.cm[3].isWhitespace && r.cm[3].isNewline);       /* LF */
        CHECK_NEAR(r.cm[2].width, 0.0f, 0.001f); CHECK_NEAR(r.cm[3].width, 0.0f, 0.001f);
        CHECK_NEAR(r.cm[0].width, CW, 0.01f); CHECK_NEAR(r.cm[4].width, CW, 0.01f);
        EXPECT_EQ(r.lm[0].length, 4); EXPECT_EQ(r.lm[0].newlineLength, 2); EXPECT_EQ(r.lm[0].trailingWhitespaceLength, 2);
        EXPECT_EQ(r.lm[1].length, 2); EXPECT_EQ(r.lm[1].newlineLength, 0);
    }
    e7_lay_close(&m);
}
TEST(e7_crlf_hit_test_positions)
{
    e7_lay m; IDWriteTextLayout *l; DWRITE_HIT_TEST_METRICS h2, h3, h4; FLOAT x2, y2, x3, y3, x4, y4;
    if (!e7_lay_open(&m, EM)) return;
    l = e7_layout(&m, L"AB\r\nCD", 1000.0f, 200.0f);
    if (l) {
        CHECK_HR(IDWriteTextLayout_HitTestTextPosition(l, 2, FALSE, &x2, &y2, &h2), S_OK);
        CHECK_HR(IDWriteTextLayout_HitTestTextPosition(l, 3, FALSE, &x3, &y3, &h3), S_OK);
        CHECK_HR(IDWriteTextLayout_HitTestTextPosition(l, 4, FALSE, &x4, &y4, &h4), S_OK);
        /* two separate one-unit clusters at the same x, both on the first line */
        EXPECT_EQ(h2.textPosition, 2); EXPECT_EQ(h2.length, 1);
        EXPECT_EQ(h3.textPosition, 3); EXPECT_EQ(h3.length, 1);
        CHECK_NEAR(x2, 2 * CW, 0.01f); CHECK_NEAR(x3, 2 * CW, 0.01f);
        CHECK_NEAR(y2, 0.0f, 0.001f); CHECK_NEAR(y3, 0.0f, 0.001f);
        /* the character after the pair starts the second line */
        CHECK_NEAR(x4, 0.0f, 0.001f); CHECK(y4 > 1.0f);
        EXPECT_EQ(h4.textPosition, 4); EXPECT_EQ(h4.length, 1);
        IDWriteTextLayout_Release(l);
    }
    e7_lay_close(&m);
}
/* negative control: CR LF is ONE break, but LF LF is two, and a lone CR or LF is one */
TEST(e7_crlf_negative_control_other_breaks)
{
    e7_lay m; e7_lres a, b, c, d;
    if (!e7_lay_open(&m, EM)) return;
    a = measure(&m, L"AB\r\nCD"); b = measure(&m, L"AB\n\nCD"); c = measure(&m, L"AB\rCD"); d = measure(&m, L"AB\nCD");
    if (a.ok && b.ok && c.ok && d.ok) {
        EXPECT_EQ(a.nlines, 2);
        EXPECT_EQ(b.nlines, 3);
        EXPECT_EQ(c.nlines, 2);
        EXPECT_EQ(d.nlines, 2);
        EXPECT_EQ(a.lm[0].newlineLength, 2);
        EXPECT_EQ(b.lm[0].newlineLength, 1);
        EXPECT_EQ(c.lm[0].newlineLength, 1);
        EXPECT_EQ(d.lm[0].newlineLength, 1);
        CHECK(a.nclu == b.nclu + 0 && a.nclu == 6);
        CHECK(d.nclu == 5);
        CHECK(a.lm[0].length != d.lm[0].length);
    }
    e7_lay_close(&m);
}
