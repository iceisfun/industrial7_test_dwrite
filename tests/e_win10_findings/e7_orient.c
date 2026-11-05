/* Vertical glyph orientation model (IDWriteTextAnalyzer2::AnalyzeVerticalGlyphOrientation) on Windows 10.
 * Three character classes: CJK-like (ideographs, kana, hangul, fullwidth forms), latin-like (other scripts, digits, ASCII punctuation)
 * and rotating (white space, cursive scripts such as Arabic letters).
 *   DEFAULT: CJK -> angle 0, sideways TRUE; latin and rotating -> angle 90, sideways FALSE; level and rtl follow the source level.
 *   STACKED: level' = (level + 1) & ~1; CJK and latin -> angle 0, sideways TRUE, rtl FALSE; rotating -> angle 90 (even level) or 270 (odd).
 * An earlier pass had the polarity of sideways/angle reversed; the negative controls pin it. */
#include "e7.h"

typedef struct { int ok; DWRITE_GLYPH_ORIENTATION_ANGLE ang; UINT8 lvl; BOOL sw, rtl; HRESULT hr; int covered; } e7_oi;

static e7_oi orient(WCHAR ch, DWRITE_VERTICAL_GLYPH_ORIENTATION o, UINT8 level)
{
    e7_oi r; IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; IDWriteTextAnalyzer2 *a2 = NULL;
    WCHAR t[2]; dwx_ana_src *s; dwx_ana_sink *k;
    memset(&r, 0, sizeof r); t[0] = ch; t[1] = 0;
    if (!f || FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) return r;
    if (FAILED(IDWriteTextAnalyzer_QueryInterface(an, &IID_IDWriteTextAnalyzer2, (void **)&a2)) || !a2) { IDWriteTextAnalyzer_Release(an); return r; }
    s = dwx_ana_src_new(t, 1); s->support_v1 = 1; s->orientation = o; s->orient_bidi = level;
    k = dwx_ana_sink_new(); k->support_v1 = 1;
    r.hr = IDWriteTextAnalyzer2_AnalyzeVerticalGlyphOrientation(a2, (IDWriteTextAnalysisSource1 *)s, 0, 1, (IDWriteTextAnalysisSink1 *)k);
    r.covered = k->orient_set[0] && k->n_orient == 1;
    r.ang = k->angle[0]; r.lvl = k->orient_bidi[0]; r.sw = k->orient_sideways[0]; r.rtl = k->orient_rtl[0]; r.ok = 1;
    IDWriteTextAnalyzer2_Release(a2); IDWriteTextAnalyzer_Release(an);
    dwx_release(s); dwx_release(k);
    return r;
}
#define A0 DWRITE_GLYPH_ORIENTATION_ANGLE_0_DEGREES
#define A90 DWRITE_GLYPH_ORIENTATION_ANGLE_90_DEGREES
#define A270 DWRITE_GLYPH_ORIENTATION_ANGLE_270_DEGREES
#define DEF DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT
#define STK DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED

static void expect(WCHAR ch, int o, UINT8 lvl, int ang, UINT8 elvl, BOOL sw, BOOL rtl)
{
    e7_oi r = orient(ch, (DWRITE_VERTICAL_GLYPH_ORIENTATION)o, lvl);
    if (!r.ok) { t_skip("analyzer2 unavailable"); return; }
    CHECK_HR(r.hr, S_OK);
    CHECKF(r.covered, "U+%04X: one SetGlyphOrientation call expected", ch);
    CHECKF(r.ang == (DWRITE_GLYPH_ORIENTATION_ANGLE)ang && r.lvl == elvl && !r.sw == !sw && !r.rtl == !rtl,
           "U+%04X o=%d lvl=%u: got ang %d lvl %u sw %d rtl %d, want ang %d lvl %u sw %d rtl %d", ch, o, lvl, (int)r.ang, r.lvl, r.sw, r.rtl, ang, elvl, sw, rtl);
}

/* default orientation: CJK sideways at 0 degrees, everything else rotated 90 and not sideways */
TEST(e7_orient_default_cjk_class)
{
    static const WCHAR cjk[] = { 0x4e00, 0x3400, 0x9fa5, 0x3042, 0x30a2, 0xac00, 0xd7a3 };
    unsigned i;
    for (i = 0; i < sizeof cjk / sizeof cjk[0]; i++) expect(cjk[i], DEF, 0, A0, 0, TRUE, FALSE);
}
TEST(e7_orient_default_latin_class)
{
    static const WCHAR lat[] = { 'A', 'z', '0', '9', 0x00e9, 0x03b1, 0x0430, 0x05d0, 0x0915, 0x0e01 };
    unsigned i;
    for (i = 0; i < sizeof lat / sizeof lat[0]; i++) expect(lat[i], DEF, 0, A90, 0, FALSE, FALSE);
}
TEST(e7_orient_default_rotating_class)
{
    /* white space and cursive scripts: same result as latin in default orientation (they differ only when stacked) */
    static const WCHAR rot[] = { ' ', '\t', 0x00a0, 0x200b, 0x0627, 0x0628 };
    unsigned i;
    for (i = 0; i < sizeof rot / sizeof rot[0]; i++) expect(rot[i], DEF, 0, A90, 0, FALSE, FALSE);
}
TEST(e7_orient_default_level_passthrough)
{
    expect(0x4e00, DEF, 1, A0, 1, TRUE, TRUE);
    expect('A', DEF, 1, A90, 1, FALSE, TRUE);
    expect('A', DEF, 2, A90, 2, FALSE, FALSE);
    expect(0x0627, DEF, 3, A90, 3, FALSE, TRUE);
}
/* stacked orientation */
TEST(e7_orient_stacked_cjk_and_latin)
{
    expect(0x4e00, STK, 0, A0, 0, TRUE, FALSE);
    expect('A', STK, 0, A0, 0, TRUE, FALSE);
    expect(0x4e00, STK, 1, A0, 2, TRUE, FALSE);
    expect('A', STK, 1, A0, 2, TRUE, FALSE);
    expect('A', STK, 2, A0, 2, TRUE, FALSE);
    expect('A', STK, 3, A0, 4, TRUE, FALSE);
}
TEST(e7_orient_stacked_rotating_class)
{
    expect(' ', STK, 0, A90, 0, FALSE, FALSE);
    expect(' ', STK, 1, A270, 2, FALSE, TRUE);
    expect(0x0627, STK, 0, A90, 0, FALSE, FALSE);
    expect(0x0627, STK, 1, A270, 2, FALSE, TRUE);
    expect(0x0627, STK, 2, A90, 2, FALSE, FALSE);
}
TEST(e7_orient_unknown_value_is_zeroed)
{
    expect(0x4e00, 2, 5, A0, 0, FALSE, FALSE);
    expect('A', 7, 5, A0, 0, FALSE, FALSE);
}
/* negative control: the classes are told apart by the answer (a model that applied one rule to all, or reversed the
 * polarity, differs from at least one of these) */
TEST(e7_orient_negative_control_classes_differ)
{
    e7_oi cjk = orient(0x4e00, DEF, 0), lat = orient('A', DEF, 0), rot = orient(' ', STK, 1), lat1 = orient('A', STK, 1);
    if (!cjk.ok) { t_skip("analyzer2 unavailable"); return; }
    CHECK(cjk.sw && !lat.sw);
    CHECK(cjk.ang != lat.ang);
    CHECK(cjk.ang == A0 && lat.ang != A0);
    CHECK(rot.ang == A270 && lat1.ang == A0);   /* odd-level rotating class is the only 270 case */
    CHECK(rot.ang != lat1.ang && rot.sw != lat1.sw);
}
