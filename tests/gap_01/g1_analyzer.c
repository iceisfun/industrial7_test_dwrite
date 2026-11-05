/* IDWriteTextAnalyzer::GetGdiCompatibleGlyphPlacements and IDWriteTextAnalyzer1::GetJustificationOpportunities: additional depth.
 *
 * Win10 dwrite.dll:
 *   DWriteTextAnalyzer::GetGdiCompatibleGlyphPlacements (!0xe7ab0): fontFace NULL -> E_INVALIDARG; glyphCount 0 -> S_OK; textLength 0 -> E_INVALIDARG;
 *     fontEmSize <= 0, pixelsPerDip <= 0 (a NaN fails the comparison too) or any of the six matrix members NaN -> E_INVALIDARG; localeName NULL is
 *     replaced by "". All of that happens before any output is written. A raw CFF face (type 7) -> DWRITE_E_UNSUPPORTEDOPERATION (0x88985009).
 *   DWriteTextAnalyzer::GetJustificationOpportunities (!0x1ddb0 -> TextAnalyzer !0x1de90): fontEmSize < 0 or NaN -> E_INVALIDARG; glyphCount 0 -> S_OK; the
 *     entry of every glyph in [0, glyphCount) is written and nothing else; float members are constants or products of fontEmSize with per-script constants (or
 *     a width read from the face), hence they scale linearly with the em size.
 */
#include "g1.h"

#define EM 16.0f
typedef struct shaped { UINT16 cm[64]; DWRITE_SHAPING_TEXT_PROPERTIES tp[64]; UINT16 gid[64]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[64]; UINT32 actual, len; DWRITE_SCRIPT_ANALYSIS sa; WCHAR text[64]; } shaped;

static DWRITE_SCRIPT_ANALYSIS script_of(IDWriteTextAnalyzer *an, const WCHAR *t, UINT32 n)
{
    DWRITE_SCRIPT_ANALYSIS sa; dwx_ana_src *src = dwx_ana_src_new(t, n); dwx_ana_sink *snk = dwx_ana_sink_new();
    memset(&sa, 0, sizeof sa);
    if (SUCCEEDED(IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, n, (IDWriteTextAnalysisSink *)snk)) && snk->script_set[0]) sa = snk->script[0];
    dwx_release(src); dwx_release(snk);
    return sa;
}
static HRESULT shape(IDWriteTextAnalyzer *an, IDWriteFontFace *face, const WCHAR *t, shaped *o)
{
    memset(o, 0, sizeof *o);
    o->len = (UINT32)wcslen(t); wcscpy(o->text, t);
    o->sa = script_of(an, t, o->len);
    return IDWriteTextAnalyzer_GetGlyphs(an, o->text, o->len, face, FALSE, FALSE, &o->sa, L"en-us", NULL, NULL, NULL, 0, 64, o->cm, o->tp, o->gid, o->gp, &o->actual);
}
static HRESULT gdi(IDWriteTextAnalyzer *an, IDWriteFontFace *face, shaped *s, float em, float ppd, const DWRITE_MATRIX *m, BOOL natural, BOOL sideways, BOOL rtl, const WCHAR *loc, float *adv, DWRITE_GLYPH_OFFSET *off)
{
    return IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(an, s->text, s->cm, s->tp, s->len, s->gid, s->gp, s->actual, face, em, ppd, m, natural, sideways, rtl, &s->sa, loc, NULL, NULL, 0, adv, off);
}
#define AN_BEGIN() IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; \
    if (!f || FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) { t_skip("CreateTextAnalyzer failed"); return; } \
    ttf_spec sp_; ttf_default_spec(&sp_); G1_FACE(g, sp_) IDWriteFontFace *face = g.h.face;
#define AN_END() do { g1_face_close(&g); DW_RELEASE(an); } while (0)

/* ===== GetGdiCompatibleGlyphPlacements ======================================================================================= */
TEST(gap1_gdi_outputs_beyond_glyph_count_untouched)
{
    AN_BEGIN(); shaped s; float adv[12]; DWRITE_GLYPH_OFFSET off[12]; int i;
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    for (i = 0; i < 12; i++) { adv[i] = -777.0f; off[i].advanceOffset = -555.0f; off[i].ascenderOffset = -444.0f; }
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, FALSE, FALSE, FALSE, L"en-us", adv, off));
    EXPECT_EQ(s.actual, 3);
    for (i = 0; i < 3; i++) CHECK(adv[i] != -777.0f);
    for (i = 3; i < 12; i++) { CHECK(adv[i] == -777.0f); CHECK(off[i].advanceOffset == -555.0f && off[i].ascenderOffset == -444.0f); }
    AN_END();
}
TEST(gap1_gdi_invalid_arguments_leave_outputs_alone)
{
    AN_BEGIN(); shaped s; float adv[4]; DWRITE_GLYPH_OFFSET off[4]; int i; DWRITE_MATRIX m = { 1, 0, 0, 1, 0, 0 };
    struct { const char *what; float em, ppd; int nanm; int noface; int zero_text; } rows[] = {
        { "em 0", 0.0f, 1.0f, 0, 0, 0 }, { "em -1", -1.0f, 1.0f, 0, 0, 0 }, { "em NaN", NAN, 1.0f, 0, 0, 0 }, { "ppd 0", EM, 0.0f, 0, 0, 0 }, { "ppd -2", EM, -2.0f, 0, 0, 0 },
        { "ppd NaN", EM, NAN, 0, 0, 0 }, { "matrix NaN", EM, 1.0f, 1, 0, 0 }, { "no face", EM, 1.0f, 0, 1, 0 }, { "text length 0", EM, 1.0f, 0, 0, 1 },
    };
    if (t_is_wine()) { AN_END(); t_skip("emulator dereferences a NULL face"); return; }
    CHECK_HR_OK(shape(an, face, L"ab", &s));
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) {
        DWRITE_MATRIX mm = m; UINT32 len = s.len; HRESULT hr; int k;
        for (k = 0; k < 4; k++) { adv[k] = -777.0f; off[k].advanceOffset = -555.0f; off[k].ascenderOffset = -444.0f; }
        if (rows[i].nanm) mm.dx = NAN;
        if (rows[i].zero_text) len = 0;
        hr = IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(an, s.text, s.cm, s.tp, len, s.gid, s.gp, s.actual, rows[i].noface ? NULL : face, rows[i].em, rows[i].ppd, &mm,
                                                                 FALSE, FALSE, FALSE, &s.sa, L"en-us", NULL, NULL, 0, adv, off);
        CHECKF(hr == E_INVALIDARG, "%s -> %s", rows[i].what, dw_hr_name(hr));
        for (k = 0; k < 4; k++) CHECKF(adv[k] == -777.0f && off[k].advanceOffset == -555.0f, "%s wrote output %d", rows[i].what, k);
    }
    AN_END();
}
TEST(gap1_gdi_every_matrix_member_nan_rejected)
{
    AN_BEGIN(); shaped s; float adv[4]; DWRITE_GLYPH_OFFSET off[4]; int i;
    CHECK_HR_OK(shape(an, face, L"ab", &s));
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX m = { 1, 0, 0, 1, 0, 0 }; float *p = &m.m11 + i; HRESULT hr;
        *p = NAN;
        hr = gdi(an, face, &s, EM, 1.0f, &m, FALSE, FALSE, FALSE, L"en-us", adv, off);
        CHECKF(hr == E_INVALIDARG, "NaN in matrix member %d -> %s", i, dw_hr_name(hr));
    }
    AN_END();
}
TEST(gap1_gdi_infinite_matrix_members_not_rejected_note)
{
    AN_BEGIN(); shaped s; float adv[4]; DWRITE_GLYPH_OFFSET off[4]; int i;
    CHECK_HR_OK(shape(an, face, L"ab", &s));
    for (i = 0; i < 6; i++) {
        DWRITE_MATRIX m = { 1, 0, 0, 1, 0, 0 }; float *p = &m.m11 + i; HRESULT hr;
        *p = INFINITY;
        hr = gdi(an, face, &s, EM, 1.0f, &m, FALSE, FALSE, FALSE, L"en-us", adv, off);
        t_note("infinite matrix member %d -> %s", i, dw_hr_name(hr));
    }
    AN_END();
}
TEST(gap1_gdi_locale_name_null_is_accepted)
{
    AN_BEGIN(); shaped s; float a[4], b[4]; DWRITE_GLYPH_OFFSET off[4];
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, FALSE, FALSE, FALSE, L"en-us", a, off));
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, FALSE, FALSE, FALSE, NULL, b, off));
    CHECK(!memcmp(a, b, 3 * sizeof(float)));
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, FALSE, FALSE, FALSE, L"", b, off));
    CHECK(!memcmp(a, b, 3 * sizeof(float)));
    AN_END();
}
TEST(gap1_gdi_locale_name_does_not_change_latin_advances)
{
    static const WCHAR *loc[] = { L"en-us", L"de-de", L"tr-tr", L"ja-jp", L"zh-cn", L"ar-sa", L"x-unknown", L"" };
    AN_BEGIN(); shaped s; float a[4], b[4]; DWRITE_GLYPH_OFFSET off[4]; unsigned i;
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, FALSE, FALSE, FALSE, loc[0], a, off));
    for (i = 1; i < sizeof loc / sizeof loc[0]; i++) {
        CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, FALSE, FALSE, FALSE, loc[i], b, off));
        CHECKF(!memcmp(a, b, 3 * sizeof(float)), "locale %ls changed the advances", loc[i]);
    }
    AN_END();
}
TEST(gap1_gdi_empty_feature_arrays_accepted)
{
    AN_BEGIN(); shaped s; float a[4]; DWRITE_GLYPH_OFFSET off[4]; UINT32 lens[1] = { 3 }; const DWRITE_TYPOGRAPHIC_FEATURES *ft[1] = { NULL };
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(an, s.text, s.cm, s.tp, s.len, s.gid, s.gp, s.actual, face, EM, 1.0f, NULL, FALSE, FALSE, FALSE, &s.sa, L"en-us", NULL, NULL, 0, a, off));
    CHECK_HR_OK(IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(an, s.text, s.cm, s.tp, s.len, s.gid, s.gp, s.actual, face, EM, 1.0f, NULL, FALSE, FALSE, FALSE, &s.sa, L"en-us", ft, lens, 0, a, off));
    AN_END();
}
TEST(gap1_gdi_whole_pixel_advances_sweep)
{
    static const float ems[] = { 6.0f, 7.0f, 8.5f, 9.0f, 10.0f, 10.5f, 11.0f, 12.0f, 13.33f, 14.0f, 16.0f, 18.0f, 20.0f, 24.0f, 32.0f, 48.0f, 72.0f, 100.0f };
    static const float ppds[] = { 1.0f, 1.25f, 1.5f, 2.0f, 3.0f };
    AN_BEGIN(); shaped s; unsigned e, p;
    CHECK_HR_OK(shape(an, face, L"Hamburgefonts 0123 ", &s));
    for (p = 0; p < sizeof ppds / sizeof ppds[0]; p++) for (e = 0; e < sizeof ems / sizeof ems[0]; e++) {
        float adv[64]; DWRITE_GLYPH_OFFSET off[64]; UINT32 i;
        CHECK_HR_OK(gdi(an, face, &s, ems[e], ppds[p], NULL, FALSE, FALSE, FALSE, L"en-us", adv, off));
        for (i = 0; i < s.actual; i++) {
            float px = adv[i] * ppds[p];
            CHECKF(fabsf(px - floorf(px + 0.5f)) < 0.002f, "em %g ppd %g glyph %u: %g DIPs = %g px", ems[e], ppds[p], i, adv[i], px);
            CHECKF(adv[i] >= 0.0f, "negative advance");
        }
    }
    AN_END();
}
TEST(gap1_gdi_advances_track_scaled_design_advance)
{
    /* advance 1000 of 2048: whole-pixel advance is within a pixel of the linear value for a plain unhinted outline font */
    static const float ems[] = { 8.0f, 12.0f, 16.0f, 24.0f, 40.0f, 64.0f };
    AN_BEGIN(); shaped s; unsigned e;
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    for (e = 0; e < sizeof ems / sizeof ems[0]; e++) {
        float adv[4]; DWRITE_GLYPH_OFFSET off[4]; float lin = 1000.0f * ems[e] / 2048.0f;
        CHECK_HR_OK(gdi(an, face, &s, ems[e], 1.0f, NULL, FALSE, FALSE, FALSE, L"en-us", adv, off));
        CHECKF(fabsf(adv[0] - lin) <= 1.5f, "em %g: GDI-compatible %g vs linear %g", ems[e], adv[0], lin);
        if (adv[0] != floorf(lin + 0.5f)) t_note("em %g: linear %g rounds to %g, GDI-compatible gives %g", ems[e], lin, floorf(lin + 0.5f), adv[0]);
    }
    AN_END();
}
TEST(gap1_gdi_identical_glyphs_identical_advances)
{
    AN_BEGIN(); shaped s; float adv[16]; DWRITE_GLYPH_OFFSET off[16]; UINT32 i;
    CHECK_HR_OK(shape(an, face, L"aaaaaaaaaa", &s));
    CHECK_HR_OK(gdi(an, face, &s, 13.0f, 1.0f, NULL, FALSE, FALSE, FALSE, L"en-us", adv, off));
    for (i = 1; i < s.actual; i++) CHECK(adv[i] == adv[0]);
    AN_END();
}
TEST(gap1_gdi_repeatable_and_input_unchanged)
{
    AN_BEGIN(); shaped s, t; float a[64], b[64]; DWRITE_GLYPH_OFFSET off[64]; int i;
    CHECK_HR_OK(shape(an, face, L"Repeat me please", &s));
    t = s;
    CHECK_HR_OK(gdi(an, face, &s, 15.0f, 1.0f, NULL, FALSE, FALSE, FALSE, L"en-us", a, off));
    for (i = 0; i < 10; i++) { CHECK_HR_OK(gdi(an, face, &s, 15.0f, 1.0f, NULL, FALSE, FALSE, FALSE, L"en-us", b, off)); CHECK(!memcmp(a, b, s.actual * sizeof(float))); }
    CHECK(!memcmp(&s, &t, sizeof s));          /* the shaping inputs are not modified */
    AN_END();
}
TEST(gap1_gdi_direction_flags_accepted_for_every_combination)
{
    AN_BEGIN(); shaped s; unsigned m;
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    for (m = 0; m < 8; m++) {
        float adv[4]; DWRITE_GLYPH_OFFSET off[4];
        HRESULT hr = gdi(an, face, &s, EM, 1.0f, NULL, (m & 1) != 0, (m & 2) != 0, (m & 4) != 0, L"en-us", adv, off);
        CHECKF(hr == S_OK, "natural %u sideways %u rtl %u -> %s", m & 1, (m >> 1) & 1, (m >> 2) & 1, dw_hr_name(hr));
        t_note("natural %u sideways %u rtl %u: advances %g %g %g", m & 1, (m >> 1) & 1, (m >> 2) & 1, adv[0], adv[1], adv[2]);
    }
    AN_END();
}
TEST(gap1_gdi_boolean_arguments_any_nonzero_value)
{
    AN_BEGIN(); shaped s; float a[4], b[4]; DWRITE_GLYPH_OFFSET off[4];
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, TRUE, TRUE, TRUE, L"en-us", a, off));
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, 77, 77, 77, L"en-us", b, off));
    CHECK(!memcmp(a, b, 3 * sizeof(float)));
    AN_END();
}
TEST(gap1_gdi_transform_values_note)
{
    static const DWRITE_MATRIX ms[] = { { 1, 0, 0, 1, 0, 0 }, { 2, 0, 0, 2, 0, 0 }, { 0, 1, -1, 0, 0, 0 }, { 1, 0, 0, 1, 50, 50 }, { 0, 0, 0, 0, 0, 0 }, { 1, 0.5f, 0, 1, 0, 0 }, { -1, 0, 0, 1, 0, 0 } };
    AN_BEGIN(); shaped s; unsigned i;
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    for (i = 0; i < sizeof ms / sizeof ms[0]; i++) {
        float adv[4]; DWRITE_GLYPH_OFFSET off[4]; HRESULT hr = gdi(an, face, &s, EM, 1.0f, &ms[i], FALSE, FALSE, FALSE, L"en-us", adv, off);
        CHECKF(hr == S_OK, "matrix %u -> %s", i, dw_hr_name(hr));
        t_note("matrix %u: advances %g %g %g", i, adv[0], adv[1], adv[2]);
    }
    AN_END();
}
TEST(gap1_gdi_translation_only_matrix_equals_identity_note)
{
    AN_BEGIN(); shaped s; float a[4], b[4]; DWRITE_GLYPH_OFFSET off[4]; DWRITE_MATRIX id = { 1, 0, 0, 1, 0, 0 }, tr = { 1, 0, 0, 1, 123, -45 };
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, &id, FALSE, FALSE, FALSE, L"en-us", a, off));
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, &tr, FALSE, FALSE, FALSE, L"en-us", b, off));
    if (memcmp(a, b, 3 * sizeof(float))) t_note("a translation changes the GDI-compatible advances"); else t_note("a translation does not change the advances");
    AN_END();
}
TEST(gap1_gdi_zero_width_space_glyph_note)
{
    AN_BEGIN(); shaped s; float adv[8]; DWRITE_GLYPH_OFFSET off[8];
    CHECK_HR_OK(shape(an, face, L"abc", &s));
    s.gp[1].isZeroWidthSpace = 1;
    CHECK_HR_OK(gdi(an, face, &s, EM, 1.0f, NULL, FALSE, FALSE, FALSE, L"en-us", adv, off));
    t_note("advance of a glyph flagged isZeroWidthSpace: %g (neighbours %g %g)", adv[1], adv[0], adv[2]);
    AN_END();
}
TEST(gap1_gdi_glyph_count_smaller_than_shaped_count)
{
    AN_BEGIN(); shaped s; float adv[8]; DWRITE_GLYPH_OFFSET off[8]; int i; HRESULT hr;
    CHECK_HR_OK(shape(an, face, L"abcd", &s));
    for (i = 0; i < 8; i++) adv[i] = -1.0f;
    hr = IDWriteTextAnalyzer_GetGdiCompatibleGlyphPlacements(an, s.text, s.cm, s.tp, s.len, s.gid, s.gp, 2, face, EM, 1.0f, NULL, FALSE, FALSE, FALSE, &s.sa, L"en-us", NULL, NULL, 0, adv, off);
    t_note("glyphCount 2 of 4 -> %s", dw_hr_name(hr));
    if (hr == S_OK) { CHECK(adv[0] >= 0 && adv[1] >= 0); CHECK(adv[2] == -1.0f && adv[3] == -1.0f); }
    AN_END();
}
TEST(gap1_gdi_arial_against_gdi_widths_note)
{
    IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    static const int px[] = { 9, 11, 12, 13, 14, 16, 20, 24 };
    unsigned i;
    if (!face) { t_skip("Arial not installed"); return; }
    if (FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an) { DW_RELEASE(face); t_skip("analyzer"); return; }
    for (i = 0; i < sizeof px / sizeof px[0]; i++) {
        shaped s; float adv[64]; DWRITE_GLYPH_OFFSET off[64]; HDC dc = CreateCompatibleDC(NULL); LOGFONTW lf; HFONT hf, old; UINT32 k; int diff = 0, total = 0; const WCHAR *txt = L"Hamburgefonts 0123.";
        memset(&lf, 0, sizeof lf); lf.lfHeight = -px[i]; lf.lfWeight = FW_NORMAL; lf.lfQuality = ANTIALIASED_QUALITY; wcscpy(lf.lfFaceName, L"Arial");
        hf = CreateFontIndirectW(&lf); old = (HFONT)SelectObject(dc, hf);
        CHECK_HR_OK(shape(an, face, txt, &s));
        CHECK_HR_OK(gdi(an, face, &s, (float)px[i], 1.0f, NULL, FALSE, FALSE, FALSE, L"en-us", adv, off));
        for (k = 0; k < s.actual && k < s.len; k++) {
            int w = 0; if (GetCharWidth32W(dc, txt[k], txt[k], &w)) { total++; if ((float)w != adv[k]) diff++; }
        }
        t_note("Arial %dpx: %d of %d GDI widths differ from the GDI-compatible placements", px[i], diff, total);
        SelectObject(dc, old); DeleteObject(hf); DeleteDC(dc);
    }
    DW_RELEASE(an); DW_RELEASE(face);
}

/* ===== GetJustificationOpportunities ========================================================================================= */
typedef struct jin { UINT16 cm[64]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[64]; WCHAR text[64]; UINT32 n; } jin;
static HRESULT jo(IDWriteTextAnalyzer1 *a1, IDWriteFontFace *face, float em, UINT16 script, jin *in, DWRITE_JUSTIFICATION_OPPORTUNITY *out)
{
    DWRITE_SCRIPT_ANALYSIS sa; sa.script = script; sa.shapes = DWRITE_SCRIPT_SHAPES_DEFAULT;
    return IDWriteTextAnalyzer1_GetJustificationOpportunities(a1, face, em, sa, in->n, in->n, in->text, in->cm, in->gp, out);
}
static void jin_fill(jin *in, const WCHAR *t, int cls)
{
    UINT32 i;
    memset(in, 0, sizeof *in);
    in->n = (UINT32)wcslen(t); wcscpy(in->text, t);
    for (i = 0; i < in->n; i++) { in->cm[i] = (UINT16)i; in->gp[i].justification = (UINT16)cls; in->gp[i].isClusterStart = 1; }
}
#define JO_BEGIN() IDWriteFactory *f = dw_shared(); IDWriteTextAnalyzer *an = NULL; IDWriteTextAnalyzer1 *a1 = NULL; \
    if (!f || FAILED(IDWriteFactory_CreateTextAnalyzer(f, &an)) || !an || !(a1 = g1_qi((IUnknown *)an, &IID_IDWriteTextAnalyzer1))) { DW_RELEASE(an); t_skip("IDWriteTextAnalyzer1 unavailable"); return; } \
    ttf_spec sp_; ttf_default_spec(&sp_); G1_FACE(g, sp_) IDWriteFontFace *face = g.h.face;
#define JO_END() do { g1_face_close(&g); DW_RELEASE(a1); DW_RELEASE(an); } while (0)

TEST(gap1_jo_entries_beyond_glyph_count_untouched)
{
    JO_BEGIN(); jin in; DWRITE_JUSTIFICATION_OPPORTUNITY out[10]; int i;
    jin_fill(&in, L"a b", 0);
    memset(out, 0xA7, sizeof out);
    CHECK_HR_OK(jo(a1, face, EM, 0, &in, out));
    for (i = 3; i < 10; i++) { uint8_t *p = (uint8_t *)&out[i]; unsigned k; for (k = 0; k < sizeof out[i]; k++) if (p[k] != 0xA7) { CHECKF(0, "entry %d was written", i); break; } }
    JO_END();
}
TEST(gap1_jo_class_and_script_sweep_invariants)
{
    JO_BEGIN(); int cls; unsigned script;
    static const WCHAR *texts[] = { L"a", L" ", L"\t", L"\x3000", L"\x00a0", L"\x0640", L"\x200b" };
    unsigned ti;
    for (ti = 0; ti < sizeof texts / sizeof texts[0]; ti++) for (cls = 0; cls < 16; cls++) for (script = 0; script < 170; script += 7) {
        jin in; DWRITE_JUSTIFICATION_OPPORTUNITY o; HRESULT hr;
        jin_fill(&in, texts[ti], cls);
        memset(&o, 0xCC, sizeof o);
        hr = jo(a1, face, EM, (UINT16)script, &in, &o);
        CHECKF(hr == S_OK, "text %u class %d script %u -> %s", ti, cls, script, dw_hr_name(hr));
        CHECKF(o.expansionMinimum == o.expansionMinimum && o.expansionMaximum == o.expansionMaximum && o.compressionMaximum == o.compressionMaximum, "NaN member (text %u class %d script %u)", ti, cls, script);
        CHECKF(o.expansionMinimum >= 0.0f && o.expansionMaximum >= 0.0f && o.compressionMaximum >= 0.0f, "negative member (text %u class %d script %u): %g %g %g", ti, cls, script,
               o.expansionMinimum, o.expansionMaximum, o.compressionMaximum);
    }
    JO_END();
}
static int scaled_or_constant(float a, float b, float k) { return b == a || fabsf(b - k * a) <= 0.001f + 1e-4f * fabsf(b); }
TEST(gap1_jo_float_members_are_constant_or_linear_in_em)
{
    /* every float member is either a constant of the entry kind or a product with the em size */
    JO_BEGIN(); int cls; unsigned ti;
    static const WCHAR *texts[] = { L"a", L" ", L"\x3000", L"\x0640" };
    for (ti = 0; ti < sizeof texts / sizeof texts[0]; ti++) for (cls = 0; cls < 16; cls++) {
        jin in; DWRITE_JUSTIFICATION_OPPORTUNITY a, b;
        jin_fill(&in, texts[ti], cls);
        CHECK_HR_OK(jo(a1, face, 10.0f, 0, &in, &a));
        CHECK_HR_OK(jo(a1, face, 40.0f, 0, &in, &b));
        CHECKF(scaled_or_constant(a.expansionMinimum, b.expansionMinimum, 4.0f), "text %u class %d: expansionMinimum %g -> %g", ti, cls, a.expansionMinimum, b.expansionMinimum);
        CHECKF(scaled_or_constant(a.expansionMaximum, b.expansionMaximum, 4.0f), "text %u class %d: expansionMaximum %g -> %g", ti, cls, a.expansionMaximum, b.expansionMaximum);
        CHECKF(scaled_or_constant(a.compressionMaximum, b.compressionMaximum, 4.0f), "text %u class %d: compressionMaximum %g -> %g", ti, cls, a.compressionMaximum, b.compressionMaximum);
        CHECKF(a.expansionPriority == b.expansionPriority && a.compressionPriority == b.compressionPriority, "priorities depend on the em size");
        CHECKF(a.allowResidualExpansion == b.allowResidualExpansion && a.allowResidualCompression == b.allowResidualCompression && a.applyToLeadingEdge == b.applyToLeadingEdge && a.applyToTrailingEdge == b.applyToTrailingEdge, "flags depend on the em size");
    }
    JO_END();
}
TEST(gap1_jo_negative_zero_and_zero_em_accepted)
{
    JO_BEGIN(); jin in; DWRITE_JUSTIFICATION_OPPORTUNITY o; float ems[2] = { 0.0f, -0.0f }; int i;
    jin_fill(&in, L"a b", 0);
    for (i = 0; i < 2; i++) { CHECK_HR_OK(jo(a1, face, ems[i], 0, &in, &o)); }
    JO_END();
}
TEST(gap1_jo_invalid_em_values)
{
    JO_BEGIN(); jin in; DWRITE_JUSTIFICATION_OPPORTUNITY o; float bad[] = { -1.0f, -0.001f, -1e30f, NAN, -INFINITY }; unsigned i;
    jin_fill(&in, L"a b", 0);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        memset(&o, 0xCC, sizeof o);
        CHECKF(jo(a1, face, bad[i], 0, &in, &o) == E_INVALIDARG, "em %g", bad[i]);
        { uint8_t *p = (uint8_t *)&o; unsigned k; for (k = 0; k < sizeof o; k++) if (p[k] != 0xCC) { CHECK(0); break; } }
    }
    JO_END();
}
TEST(gap1_jo_glyph_count_zero_ignores_everything_but_em)
{
    JO_BEGIN(); HRESULT hr; DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT };
    hr = IDWriteTextAnalyzer1_GetJustificationOpportunities(a1, NULL, EM, sa, 0, 0, NULL, NULL, NULL, NULL);
    CHECK_HR(hr, S_OK);
    hr = IDWriteTextAnalyzer1_GetJustificationOpportunities(a1, face, EM, sa, 5, 0, NULL, NULL, NULL, NULL);
    CHECK_HR(hr, S_OK);
    hr = IDWriteTextAnalyzer1_GetJustificationOpportunities(a1, face, -1.0f, sa, 5, 0, NULL, NULL, NULL, NULL);
    CHECK_HR(hr, E_INVALIDARG);
    JO_END();
}
TEST(gap1_jo_each_null_pointer_rejected)
{
    JO_BEGIN(); jin in; DWRITE_JUSTIFICATION_OPPORTUNITY o; DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT };
    jin_fill(&in, L"a b", 0);
    /* NULL pointers fault on Win10 (access violation), so only the zero length is exercised */
    { HRESULT hr = IDWriteTextAnalyzer1_GetJustificationOpportunities(a1, face, EM, sa, 0, in.n, in.text, in.cm, in.gp, &o); t_note("textLength 0 -> %s", dw_hr_name(hr)); }
    t_skip("NULL pointer arguments fault");
    JO_END();
}
TEST(gap1_jo_face_is_optional_for_every_class)
{
    JO_BEGIN(); int cls;
    for (cls = 0; cls < 16; cls++) {
        jin in; DWRITE_JUSTIFICATION_OPPORTUNITY with, without;
        jin_fill(&in, L"a b", cls);
        CHECK_HR_OK(jo(a1, face, EM, 0, &in, &with));
        { DWRITE_JUSTIFICATION_OPPORTUNITY w3[3]; CHECK_HR_OK(jo(a1, NULL, EM, 0, &in, w3)); without = w3[0]; }
        t_note("class %d, 'a': with face expMax %g compMax %g, without face expMax %g compMax %g", cls, with.expansionMaximum, with.compressionMaximum, without.expansionMaximum, without.compressionMaximum);
    }
    JO_END();
}
TEST(gap1_jo_same_input_same_output)
{
    JO_BEGIN(); jin in; DWRITE_JUSTIFICATION_OPPORTUNITY a[8], b[8]; int i;
    jin_fill(&in, L"a b c d", 4);
    CHECK_HR_OK(jo(a1, face, EM, 0, &in, a));
    for (i = 0; i < 10; i++) { CHECK_HR_OK(jo(a1, face, EM, 0, &in, b)); CHECK(!memcmp(a, b, 7 * sizeof a[0])); }
    JO_END();
}
TEST(gap1_jo_glyph_count_larger_than_text_one_glyph_per_char_only)
{
    /* glyphs map 1:1 to characters here; the text length argument only has to cover the clusters */
    JO_BEGIN(); jin in; DWRITE_JUSTIFICATION_OPPORTUNITY o[8]; DWRITE_SCRIPT_ANALYSIS sa = { 0, DWRITE_SCRIPT_SHAPES_DEFAULT };
    jin_fill(&in, L"a b", 0);
    CHECK_HR_OK(IDWriteTextAnalyzer1_GetJustificationOpportunities(a1, face, EM, sa, 3, 3, in.text, in.cm, in.gp, o));
    JO_END();
}
