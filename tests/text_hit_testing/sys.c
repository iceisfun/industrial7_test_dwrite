/* Hit testing with installed fonts (Arial, Courier New, Segoe UI Emoji, ...). Exact glyph advances depend on the font files, so the checks are
 * relations (against GetClusterMetrics / GetLineMetrics / GetMetrics) and t_note for the numbers.
 */
#include "th.h"

static IDWriteTextLayout *sys_layout(const WCHAR *fam, const WCHAR *text, float size, float w, float h)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    if (!f || !dw_have_font(f, fam)) return NULL;
    if (FAILED(IDWriteFactory_CreateTextFormat(f, fam, NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &t))) return NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(f, text, (UINT32)wcslen(text), t, w, h, &l))) l = NULL;
    IDWriteTextFormat_Release(t);
    return l;
}
#define SYS_LAYOUT(var, fam, text, size, w, h) IDWriteTextLayout *var = sys_layout(fam, text, size, w, h); if (!var) { t_skip("font %ls unavailable", fam); return; }

TEST(sys_arial_consistency_plain)
{
    SYS_LAYOUT(l, L"Arial", L"Hello, World! The quick brown fox.", 14.0f, 1000.0f, 300.0f);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_consistency_wrapped)
{
    SYS_LAYOUT(l, L"Arial", L"The quick brown fox jumps over the lazy dog and keeps running to the end", 14.0f, 120.0f, 400.0f);
    CHECK(th_lines(l) > 1);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_consistency_multiline)
{
    SYS_LAYOUT(l, L"Arial", L"first line\nsecond\r\nthird", 20.0f, 1000.0f, 300.0f);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_total_width_matches_end_caret)
{
    SYS_LAYOUT(l, L"Arial", L"Hello", 16.0f, 1000.0f, 100.0f);
    DWRITE_TEXT_METRICS tm; th_metrics(l, &tm);
    th_tp e = th_pos(l, 5, FALSE), t = th_pos(l, 4, TRUE);
    CHK_NEAR(e.x, tm.left + tm.widthIncludingTrailingWhitespace);
    CHK_NEAR(t.x, e.x);
    th_pt p = th_point(l, tm.left + tm.width + 1.0f, 2.0f);
    CHK_PT(p, 4, 1, 1, 0);
    t_note("Arial 16 \"Hello\": width %g", tm.width);
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_carets_increase)
{
    SYS_LAYOUT(l, L"Arial", L"Illumination WWW", 24.0f, 1000.0f, 100.0f);
    UINT32 i; float prev = -1.0f;
    for (i = 0; i <= 16; i++) { th_tp q = th_pos(l, i, FALSE); CHECK(q.x > prev || (i == 0 && q.x >= 0)); prev = q.x; }
    IDWriteTextLayout_Release(l);
}

TEST(sys_courier_monospaced_widths)
{
    SYS_LAYOUT(l, L"Courier New", L"iiiWWWmm..", 16.0f, 1000.0f, 100.0f);
    th_tp a = th_pos(l, 0, FALSE);
    UINT32 i;
    for (i = 1; i < 10; i++) { th_tp q = th_pos(l, i, FALSE); CHK_NEAR(q.m.width, a.m.width); CHK_NEAR(q.m.left, i * a.m.width); }
    t_note("Courier New 16: advance %g (design 1229/2048 -> 9.6015625)", a.m.width);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_hebrew_rtl_run)
{
    SYS_LAYOUT(l, L"Arial", L"\x05e9\x05dc\x05d5\x05dd", 16.0f, 1000.0f, 100.0f);
    DWRITE_TEXT_METRICS tm; th_metrics(l, &tm);
    th_tp a = th_pos(l, 0, FALSE), z = th_pos(l, 3, FALSE);
    CHECK(a.m.bidiLevel == 1 && z.m.bidiLevel == 1);
    CHECK(a.m.left > z.m.left);                                      /* first logical char is rightmost */
    CHK_NEAR(a.m.left + a.m.width, tm.left + tm.width);
    CHK_NEAR(a.x, a.m.left + a.m.width);
    th_rg r = th_range(l, 0, 4, 0, 0);
    CHECK(r.count == 1);
    if (r.count == 1) { CHECK(r.m[0].bidiLevel == 1 && r.m[0].length == 4); CHK_NEAR(r.m[0].width, tm.width); }
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_mixed_direction_ranges)
{
    SYS_LAYOUT(l, L"Arial", L"abc \x05d0\x05d1\x05d2 def", 16.0f, 1000.0f, 100.0f);
    th_rg r = th_range(l, 0, 11, 0, 0);
    CHECK(r.count == 3);
    if (r.count == 3) {
        CHECK(r.m[0].textPosition == 0 && r.m[0].length == 4 && r.m[0].bidiLevel == 0);
        CHECK(r.m[1].textPosition == 4 && r.m[1].length == 3 && r.m[1].bidiLevel == 1);
        CHECK(r.m[2].textPosition == 7 && r.m[2].length == 4 && r.m[2].bidiLevel == 0);
        CHK_NEAR(r.m[1].left, r.m[0].left + r.m[0].width);
        CHK_NEAR(r.m[2].left, r.m[1].left + r.m[1].width);
    }
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_arabic_clusters_noted)
{
    SYS_LAYOUT(l, L"Arial", L"\x0644\x0627 \x0645\x0631\x062d\x0628\x0627", 18.0f, 1000.0f, 100.0f);
    DWRITE_CLUSTER_METRICS c[16]; UINT32 n = th_clusters(l, c, 16), i, at = 0;
    t_note("Arabic text: %u clusters for 8 chars", n);
    for (i = 0; i < n; i++) {
        th_tp q = th_pos(l, at, FALSE);
        CHECKF(q.m.textPosition == at && q.m.length == c[i].length, "cluster %u", i);
        CHK_NEAR(q.m.width, c[i].width);
        CHECK(q.m.bidiLevel == 1 || q.m.bidiLevel == 0);
        at += c[i].length;
    }
    th_rg r = th_range(l, 0, 8, 0, 0);
    CHECK(r.count >= 1);
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_combining_cluster)
{
    SYS_LAYOUT(l, L"Arial", L"xe\x0301y", 20.0f, 1000.0f, 100.0f);
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    CHECK(n == 3);
    if (n == 3) {
        CHECK(c[1].length == 2);
        th_pt p = th_point(l, c[0].width + c[1].width * 0.25f, 2.0f);
        CHK_PT(p, 1, 2, 0, 1);
        th_tp q = th_pos(l, 2, TRUE);
        CHK_NEAR(q.x, c[0].width + c[1].width);
    }
    IDWriteTextLayout_Release(l);
}

TEST(sys_emoji_surrogate_cluster)
{
    SYS_LAYOUT(l, L"Segoe UI Emoji", L"a\xd83d\xde00" L"b", 20.0f, 1000.0f, 100.0f);
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    CHECK(n == 3);
    if (n == 3) {
        CHECK(c[1].length == 2);
        th_tp a = th_pos(l, 1, FALSE), b = th_pos(l, 2, FALSE);
        CHECK(a.m.textPosition == 1 && a.m.length == 2 && b.m.textPosition == 1 && b.m.length == 2);
        CHK_NEAR(a.m.width, c[1].width);
        th_pt p = th_point(l, c[0].width + c[1].width * 0.75f, 2.0f);
        CHK_PT(p, 1, 2, 1, 1);
    }
    IDWriteTextLayout_Release(l);
}

TEST(sys_emoji_zwj_or_modifier_noted)
{
    SYS_LAYOUT(l, L"Segoe UI Emoji", L"\xd83d\xdc4d\xd83c\xdffd", 20.0f, 1000.0f, 100.0f);   /* thumbs up + skin tone modifier */
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    t_note("thumbs-up + modifier: %u clusters", n);
    th_tp a = th_pos(l, 0, FALSE);
    CHECK(a.m.textPosition == 0 && a.m.length >= 2);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout_Release(l);
}

TEST(sys_arial_fi_not_ligated_noted)
{
    SYS_LAYOUT(l, L"Arial", L"office", 20.0f, 1000.0f, 100.0f);
    DWRITE_CLUSTER_METRICS c[8]; UINT32 n = th_clusters(l, c, 8);
    t_note("Arial \"office\": %u clusters", n);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout_Release(l);
}

TEST(sys_segoe_ui_consistency)
{
    SYS_LAYOUT(l, L"Segoe UI", L"Segoe UI text, with: punctuation (and) numbers 12345.", 15.0f, 200.0f, 300.0f);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout_Release(l);
}

TEST(sys_font_fallback_cluster_consistency)
{
    /* Text outside the primary font's coverage is shaped with fallback fonts: hit testing still follows the clusters */
    SYS_LAYOUT(l, L"Arial", L"abc \x4e2d\x6587 def \x3042\x3044", 16.0f, 1000.0f, 100.0f);
    CHECK_CONSISTENT(l);
    IDWriteTextLayout_Release(l);
}

TEST(sys_gdi_compatible_layout_integer_widths)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL;
    if (!dw_have_font(f, L"Arial")) { t_skip("Arial unavailable"); return; }
    if (FAILED(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &t))) { t_skip("format"); return; }
    if (FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"Hello GDI world", 15, t, 1000.0f, 100.0f, 1.0f, NULL, FALSE, &l))) { DW_RELEASE(t); t_skip("gdi layout"); return; }
    CHECK_CONSISTENT(l);
    DWRITE_CLUSTER_METRICS c[32]; UINT32 n = th_clusters(l, c, 32), i, at = 0;
    for (i = 0; i < n; i++) {
        th_tp q = th_pos(l, at, FALSE);
        CHECKF(q.m.width == floorf(q.m.width), "cluster %u width %g not a whole pixel in GDI classic mode", i, q.m.width);
        at += c[i].length;
    }
    DW_RELEASE(l); DW_RELEASE(t);
}

TEST(sys_gdi_compatible_synthetic_integer_widths)
{
    TH_BEGIN();
    IDWriteTextFormat *t = th_format(&X, TH_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t || FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"ABCD", 4, t, 1000.0f, 100.0f, 1.0f, NULL, FALSE, &l))) { DW_RELEASE(t); TH_END(); t_skip("gdi layout"); return; }
    th_tp q = th_pos(l, 0, FALSE);
    t_note("GDI classic, synthetic 16px: advance %g (ideal 7.8125)", q.m.width);
    CHECK(q.m.width == floorf(q.m.width));
    th_tp e = th_pos(l, 4, FALSE);
    CHK_NEAR(e.x, 4 * q.m.width);
    th_pt p = th_point(l, 2 * q.m.width + 1.0f, 3.0f);
    CHK_PT(p, 2, 1, 0, 1);
    DW_RELEASE(l); DW_RELEASE(t); TH_END();
}

TEST(sys_gdi_natural_layout_consistency)
{
    TH_BEGIN();
    IDWriteTextFormat *t = th_format(&X, TH_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t || FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"ABCD EFGH", 9, t, 1000.0f, 100.0f, 1.0f, NULL, TRUE, &l))) { DW_RELEASE(t); TH_END(); t_skip("gdi layout"); return; }
    CHECK_CONSISTENT(l);
    DW_RELEASE(l); DW_RELEASE(t); TH_END();
}

TEST(sys_pixels_per_dip_scaling_consistency)
{
    TH_BEGIN();
    IDWriteTextFormat *t = th_format(&X, TH_FAM, 16.0f); IDWriteTextLayout *l = NULL;
    if (!t || FAILED(IDWriteFactory_CreateGdiCompatibleTextLayout(f, L"ABCD EFGH", 9, t, 1000.0f, 100.0f, 2.0f, NULL, TRUE, &l))) { DW_RELEASE(t); TH_END(); t_skip("gdi layout"); return; }
    CHECK_CONSISTENT(l);
    DW_RELEASE(l); DW_RELEASE(t); TH_END();
}
