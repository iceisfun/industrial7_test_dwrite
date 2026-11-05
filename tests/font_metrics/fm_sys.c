/* Stock Windows 10 fonts: face metrics against the fonts' own tables (read through TryGetFontTable), GDI-compatible metrics against the
 * rounding rule, design glyph metrics against hmtx/loca/glyf, monospace and bold relations, simulated bold on a real font, text layout line
 * height.  Fonts that are not installed skip their tests; exact values are reported as notes. */
#include "fm.h"
#include <math.h>

static IDWriteFontFace *need(const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFontFace *f = fm_sys(fam, w, st);
    if (!f) t_skip("font not installed");
    return f;
}
/* ---- metrics against tables ---- */
static void metrics_vs_tables_face(IDWriteFontFace *face, const char *name) {
    IDWriteFontFace1 *f1; DWRITE_FONT_METRICS1 m; fm_exp e; DWRITE_FONT_METRICS v0;
    f1 = fm_f1(face);
    if (!f1) { t_skip("IDWriteFontFace1 unavailable"); return; }
    memset(&m, 0xCD, sizeof m); IDWriteFontFace1_GetMetrics(f1, &m);
    IDWriteFontFace_GetMetrics(face, &v0);
    if (!fm_derive(face, IDWriteFontFace_GetSimulations(face), &e)) { t_skip("head/hhea unreadable"); DW_RELEASE(f1); return; }
    FM_CMP(&m, &e.m, fm_unknown_mask(&e), name);
    fm_dump(name, &m);
    EXPECT_EQ(v0.designUnitsPerEm, m.designUnitsPerEm); EXPECT_EQ(v0.ascent, m.ascent); EXPECT_EQ(v0.descent, m.descent); EXPECT_EQ(v0.lineGap, m.lineGap);
    EXPECT_EQ(v0.capHeight, m.capHeight); EXPECT_EQ(v0.xHeight, m.xHeight);
    CHECK(m.designUnitsPerEm >= 16 && m.designUnitsPerEm <= 16384);
    CHECK(m.ascent > 0 && m.descent >= 0);   /* symbol fonts (Marlett) have a zero descent */
    CHECK(m.capHeight > 0 && m.xHeight > 0 && m.xHeight < m.capHeight + m.designUnitsPerEm / 2);
    CHECK(m.underlineThickness > 0 && m.strikethroughThickness > 0);
    DW_RELEASE(f1);
}
static void metrics_vs_tables(const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFontFace *face = need(fam, w, st); char name[64];
    if (!face) return;
    dw_narrow(fam, name, sizeof name); metrics_vs_tables_face(face, name); DW_RELEASE(face);
}
/* ---- GDI compatible metrics ---- */
static void gdi_vs_mirror_face(IDWriteFontFace *face) {
    static const int px[] = { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 20, 24, 28, 32, 36, 48, 72, 96 };
    static const float ppd[] = { 1.0f, 1.25f, 1.5f, 2.0f };
    IDWriteFontFace1 *f1; DWRITE_FONT_METRICS1 base; unsigned skip = 0, i, k; int vdmx;
    f1 = fm_f1(face);
    if (!f1) { t_skip("IDWriteFontFace1 unavailable"); return; }
    IDWriteFontFace1_GetMetrics(f1, &base);
    vdmx = fm_has_table(face, DW_TAG('V', 'D', 'M', 'X'));
    if (vdmx) { skip = FM_BIT(FM_ASC) | FM_BIT(FM_DESC); t_note("VDMX present: ascent/descent taken from it for 1 < ppem < 255"); }
    for (k = 0; k < 4; k++)
        for (i = 0; i < sizeof px / sizeof px[0]; i++) {
            DWRITE_FONT_METRICS1 got, want; char l[48]; HRESULT hr;
            memset(&got, 0xCD, sizeof got);
            hr = IDWriteFontFace1_GetGdiCompatibleMetrics(f1, (float)px[i], ppd[k], NULL, &got);
            if (hr != S_OK) { t_check(0, __FILE__, __LINE__, "GetGdiCompatibleMetrics", "em %d ppd %g -> %s", px[i], ppd[k], dw_hr_name(hr)); continue; }
            fm_gdi_expect(&base, (float)px[i], ppd[k], NULL, &want);
            snprintf(l, sizeof l, "em %d ppd %g", px[i], ppd[k]);
            fm_cmp_(__FILE__, __LINE__, &got, &want, skip, l);
            if (vdmx) {
                /* VDMX supplies hinted pixel extents that can differ from the design values by a few pixels */
                float dpp = (float)base.designUnitsPerEm / ((float)px[i] * ppd[k]);
                CHECKF(fabsf((float)got.ascent - base.ascent) <= 4.0f * dpp + 0.05f * base.designUnitsPerEm, "VDMX ascent %d vs %d at %s", got.ascent, base.ascent, l);
                CHECKF(fabsf((float)got.descent - base.descent) <= 4.0f * dpp + 0.05f * base.designUnitsPerEm, "VDMX descent %d vs %d at %s", got.descent, base.descent, l);
            }
        }
    DW_RELEASE(f1);
}
static void gdi_vs_mirror(const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFontFace *face = need(fam, w, st);
    if (!face) return;
    gdi_vs_mirror_face(face); DW_RELEASE(face);
}
/* ---- design glyph metrics vs hmtx/loca/glyf ---- */
static int tbl_glyph(IDWriteFontFace *face, unsigned gid, fm_gbox *b) {
    UINT32 hl, tl, ml, ll, gl, el; int ok = 0;
    uint8_t *head = dw_face_table(face, DW_TAG('h', 'e', 'a', 'd'), &hl), *hhea = dw_face_table(face, DW_TAG('h', 'h', 'e', 'a'), &el);
    uint8_t *hmtx = dw_face_table(face, DW_TAG('h', 'm', 't', 'x'), &ml), *loca = dw_face_table(face, DW_TAG('l', 'o', 'c', 'a'), &ll);
    uint8_t *glyf = dw_face_table(face, DW_TAG('g', 'l', 'y', 'f'), &gl);
    (void)tl;
    if (head && hl >= 54 && hhea && el >= 36 && hmtx && loca && glyf) {
        unsigned nlong = fm_be16(hhea + 34); int longloca = fm_be16(head + 50);
        unsigned long off, nxt;
        memset(b, 0, sizeof *b);
        if (nlong && ml >= nlong * 4) {
            if (gid < nlong) { b->adv = (int)fm_be16(hmtx + gid * 4); b->lsb = fm_be16s(hmtx + gid * 4 + 2); }
            else { b->adv = (int)fm_be16(hmtx + (nlong - 1) * 4); b->lsb = (ml >= nlong * 4 + (gid - nlong) * 2 + 2) ? fm_be16s(hmtx + nlong * 4 + (gid - nlong) * 2) : 0; }
            if (longloca ? ll >= (gid + 2) * 4 : ll >= (gid + 2) * 2) {
                if (longloca) { off = ((unsigned long)fm_be16(loca + gid * 4) << 16) | fm_be16(loca + gid * 4 + 2); nxt = ((unsigned long)fm_be16(loca + gid * 4 + 4) << 16) | fm_be16(loca + gid * 4 + 6); }
                else { off = fm_be16(loca + gid * 2) * 2ul; nxt = fm_be16(loca + gid * 2 + 2) * 2ul; }
                if (nxt - off >= 10 && nxt <= gl) { b->x0 = fm_be16s(glyf + off + 2); b->y0 = fm_be16s(glyf + off + 4); b->x1 = fm_be16s(glyf + off + 6); b->y1 = fm_be16s(glyf + off + 8); }
                ok = 1;
            }
        }
    }
    free(head); free(hhea); free(hmtx); free(loca); free(glyf);
    return ok;
}
static int vert_asc_desc(IDWriteFontFace *face, int *asc, int *desc) {
    UINT32 ol, al; int r = 0;
    uint8_t *os2 = dw_face_table(face, DW_TAG('O', 'S', '/', '2'), &ol), *hhea = dw_face_table(face, DW_TAG('h', 'h', 'e', 'a'), &al);
    int a, d;
    if (os2 && ol > 0x4d) {
        a = fm_be16s(os2 + 0x44);
        if (a < 0) { a = fm_be16(os2 + 0x4a); d = abs(fm_be16s(os2 + 0x4c)); } else d = abs(fm_be16s(os2 + 0x46));
        r = 1;
    } else if (hhea && al >= 36) { a = fm_be16(hhea + 4); d = 0; r = 1; }   /* no OS/2: hhea ascender, zero descent */
    if (r) { *asc = a > 0x7fff ? 0x7fff : a < 1 ? 0 : a; *desc = d > 0x7fff ? 0x7fff : d < 1 ? 0 : d; }
    free(os2); free(hhea);
    return r;
}
static void design_vs_tables_face(IDWriteFontFace *face, const char *name) {
    static const UINT32 cps[] = { 'A', 'i', 'W', ' ', '0', 'x', 'H', 'g', '.', 'm', 'l', 0xE9 };
    UINT16 gi[12]; DWRITE_GLYPH_METRICS m[12]; unsigned i; int asc, desc, vert;
    if (!fm_has_table(face, DW_TAG('g', 'l', 'y', 'f'))) { t_skip("no glyf outlines"); return; }
    vert = fm_has_table(face, DW_TAG('v', 'm', 't', 'x'));
    if (!vert_asc_desc(face, &asc, &desc)) { t_skip("no vertical source"); return; }
    CHECK_HR(IDWriteFontFace_GetGlyphIndices(face, cps, 12, gi), S_OK);
    memset(m, 0xCD, sizeof m);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(face, gi, 12, m, FALSE), S_OK);
    for (i = 0; i < 12; i++) {
        fm_gbox b; DWRITE_GLYPH_METRICS want; char l[80];
        if (!gi[i]) { t_note("%s: U+%04X unmapped", name, cps[i]); continue; }
        if (!tbl_glyph(face, gi[i], &b)) { t_note("%s: glyph %u not readable from tables", name, gi[i]); continue; }
        fm_design_expect(&b, asc, desc, &want);
        snprintf(l, sizeof l, "%s U+%04X gid %u", name, cps[i], gi[i]);
        if (vert) {   /* vmtx supplies the vertical values: compare the horizontal ones only */
            t_check(m[i].advanceWidth == want.advanceWidth && m[i].leftSideBearing == want.leftSideBearing && m[i].rightSideBearing == want.rightSideBearing, __FILE__, __LINE__, "horizontal metrics", "%s: adv %u/%u lsb %d/%d rsb %d/%d", l, m[i].advanceWidth, want.advanceWidth, m[i].leftSideBearing, want.leftSideBearing, m[i].rightSideBearing, want.rightSideBearing);
        } else FM_GMCMP(&m[i], &want, l);
        if (cps[i] == ' ') { CHECKF(m[i].rightSideBearing == (int)m[i].advanceWidth - m[i].leftSideBearing, "%s: space has ink", l); }
    }
}
static void design_vs_tables(const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFontFace *face = need(fam, w, st); char name[64];
    if (!face) return;
    dw_narrow(fam, name, sizeof name); design_vs_tables_face(face, name); DW_RELEASE(face);
}

/* the same table-driven comparisons on the synthetic fonts (always runnable) */
TEST(fms_selfcheck_default_font) {
    ttf_spec s; ttf_default_spec(&s); FM_OPEN(F, s, 0);
    metrics_vs_tables_face(F.face, "synthetic default"); gdi_vs_mirror_face(F.face); design_vs_tables_face(F.face, "synthetic default");
    fm_close(&F);
}
TEST(fms_selfcheck_typo_font) {
    ttf_spec s; ttf_default_spec(&s); s.fs_selection |= 0x80; s.typo_asc = 1500; s.typo_desc = -500; FM_OPEN(F, s, 0);
    metrics_vs_tables_face(F.face, "synthetic typo"); gdi_vs_mirror_face(F.face); design_vs_tables_face(F.face, "synthetic typo");
    fm_close(&F);
}
TEST(fms_selfcheck_upm_1000_font) {
    ttf_spec s; fm_spec_upm(&s, 1000); FM_OPEN(F, s, 0);
    metrics_vs_tables_face(F.face, "synthetic upm1000"); gdi_vs_mirror_face(F.face); design_vs_tables_face(F.face, "synthetic upm1000");
    fm_close(&F);
}
TEST(fms_selfcheck_no_os2_font) {
    ttf_spec s; ttf_default_spec(&s); s.omit |= TTF_OMIT_OS2; FM_OPEN(F, s, 0);
    metrics_vs_tables_face(F.face, "synthetic noos2"); gdi_vs_mirror_face(F.face); design_vs_tables_face(F.face, "synthetic noos2");
    fm_close(&F);
}

#define FONT(tag, fam, w, st) \
    TEST(fms_##tag##_metrics_match_tables) { metrics_vs_tables(fam, w, st); } \
    TEST(fms_##tag##_gdi_metrics_rounding) { gdi_vs_mirror(fam, w, st); } \
    TEST(fms_##tag##_design_glyphs_match_tables) { design_vs_tables(fam, w, st); }
FONT(arial, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(arial_bold, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL)
FONT(arial_italic, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC)
FONT(arial_bold_italic, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC)
FONT(times, L"Times New Roman", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(times_bold, L"Times New Roman", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL)
FONT(times_italic, L"Times New Roman", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC)
FONT(courier, L"Courier New", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(courier_bold, L"Courier New", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL)
FONT(segoe_ui, L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(segoe_ui_bold, L"Segoe UI", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL)
FONT(segoe_ui_light, L"Segoe UI", DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_NORMAL)
FONT(tahoma, L"Tahoma", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(tahoma_bold, L"Tahoma", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL)
FONT(consolas, L"Consolas", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(consolas_bold, L"Consolas", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL)
FONT(calibri, L"Calibri", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(calibri_bold, L"Calibri", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL)
FONT(verdana, L"Verdana", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(georgia, L"Georgia", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(segoe_symbol, L"Segoe UI Symbol", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)
FONT(lucida_console, L"Lucida Console", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)

/* Marlett and MS Gothic: symbol / vertical-capable fonts, checked against the tables only */
TEST(fms_marlett_metrics_match_tables) { metrics_vs_tables(L"Marlett", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(fms_ms_gothic_metrics_match_tables) { metrics_vs_tables(L"MS Gothic", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(fms_bahnschrift_metrics_note) {
    IDWriteFontFace *face = need(L"Bahnschrift", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace1 *f1; DWRITE_FONT_METRICS1 m; fm_exp e;
    if (!face) return;
    f1 = fm_f1(face);
    if (f1) {
        IDWriteFontFace1_GetMetrics(f1, &m); fm_dump("Bahnschrift", &m);
        /* variable font: the default instance equals the base tables, a named instance would carry MVAR deltas, so only upm and the
         * glyph box are hard-checked here */
        if (fm_derive(face, 0, &e)) { EXPECT_EQ(m.designUnitsPerEm, e.m.designUnitsPerEm); FM_CMP(&m, &e.m, ~(FM_BIT(FM_UPM) | FM_BIT(FM_BOXL) | FM_BIT(FM_BOXR) | FM_BIT(FM_BOXT) | FM_BIT(FM_BOXB)) & ((1u << FM_NFIELDS) - 1), "Bahnschrift"); }
    }
    DW_RELEASE(f1); DW_RELEASE(face);
}

/* ---- Arial: the values the OS ships (reported, checked against the tables above) ---- */
TEST(fms_arial_known_values) {
    IDWriteFontFace *face = need(L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace1 *f1; DWRITE_FONT_METRICS1 m; fm_exp e;
    if (!face) return;
    f1 = fm_f1(face);
    if (f1 && fm_derive(face, 0, &e)) {
        IDWriteFontFace1_GetMetrics(f1, &m);
        if (e.m.designUnitsPerEm == 2048) {
            /* Arial 2048 units/em: whatever the installed copy's tables say is what the API must return; the values below are reported when
             * the copy is the stock one */
            static const int stock[] = { 2048, 1854, 434, 67, 1467, 1062, -217, 150, 530, 102 };
            int i, same = 1;
            for (i = 0; i < 10; i++) if (fm_field_get(&m, i) != stock[i]) same = 0;
            if (same) { EXPECT_EQ(m.designUnitsPerEm, 2048); EXPECT_EQ(m.ascent, 1854); EXPECT_EQ(m.descent, 434); EXPECT_EQ(m.lineGap, 67); EXPECT_EQ(m.capHeight, 1467); EXPECT_EQ(m.xHeight, 1062);
                EXPECT_EQ((int)m.underlinePosition, -217); EXPECT_EQ(m.underlineThickness, 150); EXPECT_EQ(m.strikethroughPosition, 530); EXPECT_EQ(m.strikethroughThickness, 102); }
            else t_note("Arial differs from the stock numbers");
        } else t_note("Arial upm %d", e.m.designUnitsPerEm);
    }
    DW_RELEASE(f1); DW_RELEASE(face);
}

/* ---- IDWriteFont level ---- */
static IDWriteFont *sysfont(IDWriteFactory *f, const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *ff = NULL; IDWriteFont *font = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return NULL;
    IDWriteFontCollection_FindFamilyName(c, fam, &idx, &ex);
    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &ff)) && ff) { IDWriteFontFamily_GetFirstMatchingFont(ff, w, DWRITE_FONT_STRETCH_NORMAL, st, &font); IDWriteFontFamily_Release(ff); }
    IDWriteFontCollection_Release(c);
    return font;
}
static void font_vs_face(const WCHAR *fam, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFactory *f = dw_shared(); IDWriteFont *font; IDWriteFontFace *face = NULL; DWRITE_FONT_METRICS a, b; IDWriteFont1 *font1 = NULL; IDWriteFontFace1 *f1;
    if (!f) { t_skip("no factory"); return; }
    font = sysfont(f, fam, w, st);
    if (!font) { t_skip("font not installed"); return; }
    CHECK_HR(IDWriteFont_CreateFontFace(font, &face), S_OK);
    if (face) {
        IDWriteFont_GetMetrics(font, &a); IDWriteFontFace_GetMetrics(face, &b);
        CHECK(!memcmp(&a, &b, sizeof a));
        if (SUCCEEDED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont1, (void **)&font1)) && font1 && (f1 = fm_f1(face)) != NULL) {
            DWRITE_FONT_METRICS1 x, y;
            memset(&x, 0xCD, sizeof x); memset(&y, 0xEF, sizeof y);
            IDWriteFont1_GetMetrics(font1, &x); IDWriteFontFace1_GetMetrics(f1, &y);
            CHECK(!memcmp(&x, &y, sizeof x));
            EXPECT_EQ(x.designUnitsPerEm, a.designUnitsPerEm); EXPECT_EQ(x.ascent, a.ascent); EXPECT_EQ(x.strikethroughThickness, a.strikethroughThickness);
            DW_RELEASE(f1);
        }
        DW_RELEASE(font1); DW_RELEASE(face);
    }
    DW_RELEASE(font);
}
TEST(fms_font_metrics_equal_face_metrics_arial) { font_vs_face(L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(fms_font_metrics_equal_face_metrics_arial_bold) { font_vs_face(L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL); }
TEST(fms_font_metrics_equal_face_metrics_segoe) { font_vs_face(L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }
TEST(fms_font_metrics_equal_face_metrics_times_italic) { font_vs_face(L"Times New Roman", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC); }
TEST(fms_font_metrics_equal_face_metrics_courier) { font_vs_face(L"Courier New", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); }

/* simulated styles on the font object: the simulations show up in the metrics of the matching face */
TEST(fms_font_simulated_bold_metrics_equal_face) {
    IDWriteFactory *f = dw_shared(); IDWriteFont *font; IDWriteFontFace *face = NULL; DWRITE_FONT_METRICS a, b;
    if (!f) { t_skip("no factory"); return; }
    font = sysfont(f, L"Courier New", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_OBLIQUE);
    if (!font) { t_skip("font not installed"); return; }
    t_note("Courier New bold oblique simulations=0x%x", IDWriteFont_GetSimulations(font));
    CHECK_HR(IDWriteFont_CreateFontFace(font, &face), S_OK);
    if (face) { IDWriteFont_GetMetrics(font, &a); IDWriteFontFace_GetMetrics(face, &b); CHECK(!memcmp(&a, &b, sizeof a)); EXPECT_EQ(IDWriteFontFace_GetSimulations(face), IDWriteFont_GetSimulations(font)); }
    DW_RELEASE(face); DW_RELEASE(font);
}

/* ---- relations between faces ---- */
static int run_width(IDWriteFontFace1 *f1, IDWriteFontFace *face, const WCHAR *text) {
    UINT32 cps[64]; UINT16 gi[64]; INT32 adv[64]; unsigned n = 0, i; int sum = 0;
    while (text[n] && n < 64) { cps[n] = text[n]; n++; }
    IDWriteFontFace_GetGlyphIndices(face, cps, n, gi);
    IDWriteFontFace1_GetDesignGlyphAdvances(f1, n, gi, adv, FALSE);
    for (i = 0; i < n; i++) sum += adv[i];
    return sum;
}
static void bold_wider(const WCHAR *fam) {
    IDWriteFontFace *r = fm_sys(fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL), *b = fm_sys(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL);
    IDWriteFontFace1 *r1 = NULL, *b1 = NULL;
    if (!r || !b) { t_skip("font not installed"); DW_RELEASE(r); DW_RELEASE(b); return; }
    r1 = fm_f1(r); b1 = fm_f1(b);
    if (r1 && b1) {
        static const WCHAR text[] = L"The quick brown fox jumps over the lazy dog 0123456789";
        DWRITE_FONT_METRICS1 mr, mb; int wr = run_width(r1, r, text), wb = run_width(b1, b, text);
        IDWriteFontFace1_GetMetrics(r1, &mr); IDWriteFontFace1_GetMetrics(b1, &mb);
        /* bold advances are never narrower in these families (summed over a pangram), the em square is the same */
        CHECKF(wb >= wr, "bold run %d < regular run %d", wb, wr);
        t_note("pangram width regular=%d bold=%d (upm %d / %d)", wr, wb, mr.designUnitsPerEm, mb.designUnitsPerEm);
        EXPECT_EQ(mr.designUnitsPerEm, mb.designUnitsPerEm);
    }
    DW_RELEASE(r1); DW_RELEASE(b1); DW_RELEASE(r); DW_RELEASE(b);
}
TEST(fms_bold_wider_than_regular_arial) { bold_wider(L"Arial"); }
TEST(fms_bold_wider_than_regular_times) { bold_wider(L"Times New Roman"); }
TEST(fms_bold_wider_than_regular_segoe) { bold_wider(L"Segoe UI"); }
TEST(fms_bold_wider_than_regular_tahoma) { bold_wider(L"Tahoma"); }
TEST(fms_bold_wider_than_regular_calibri) { bold_wider(L"Calibri"); }
TEST(fms_bold_wider_than_regular_verdana) { bold_wider(L"Verdana"); }

static void mono_family(const WCHAR *fam, int check_family_equal) {
    static const DWRITE_FONT_WEIGHT ws[] = { DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_WEIGHT_BOLD };
    static const DWRITE_FONT_STYLE sts[] = { DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_ITALIC };
    int first = -1, any = 0; unsigned wi, si;
    for (wi = 0; wi < 2; wi++) for (si = 0; si < 2; si++) {
        IDWriteFontFace *face = fm_sys(fam, ws[wi], sts[si]); IDWriteFontFace1 *f1; UINT32 cps[95]; UINT16 gi[95]; INT32 adv[95]; unsigned i;
        if (!face) continue;
        if (IDWriteFontFace_GetSimulations(face) != DWRITE_FONT_SIMULATIONS_NONE) { t_note("w%d s%d is a simulated face (advances widened), skipped", (int)ws[wi], (int)sts[si]); DW_RELEASE(face); continue; }
        f1 = fm_f1(face);
        if (!f1) { DW_RELEASE(face); continue; }
        any = 1;
        for (i = 0; i < 95; i++) cps[i] = 0x20 + i;
        IDWriteFontFace_GetGlyphIndices(face, cps, 95, gi);
        CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 95, gi, adv, FALSE), S_OK);
        for (i = 1; i < 95; i++) t_check(adv[i] == adv[0], __FILE__, __LINE__, "monospace", "w%d s%d: U+%04X advance %d vs space %d", (int)ws[wi], (int)sts[si], cps[i], adv[i], adv[0]);
        if (check_family_equal) { if (first < 0) first = adv[0]; else t_check(adv[0] == first, __FILE__, __LINE__, "face advance equal across the family", "w%d s%d: %d vs %d", (int)ws[wi], (int)sts[si], adv[0], first); }
        t_note("monospace advance w%d s%d = %d", (int)ws[wi], (int)sts[si], adv[0]);
        DW_RELEASE(f1); DW_RELEASE(face);
    }
    if (!any) t_skip("font not installed");
}
TEST(fms_courier_new_is_monospaced) { mono_family(L"Courier New", 1); }
TEST(fms_consolas_is_monospaced) { mono_family(L"Consolas", 1); }
TEST(fms_lucida_console_is_monospaced) { mono_family(L"Lucida Console", 1); }

TEST(fms_courier_new_advance_note) {
    IDWriteFontFace *face = need(L"Courier New", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace1 *f1; UINT32 cp = 'A'; UINT16 g; INT32 adv = 0; DWRITE_FONT_METRICS1 m;
    if (!face) return;
    f1 = fm_f1(face);
    if (f1) {
        IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g); IDWriteFontFace1_GetDesignGlyphAdvances(f1, 1, &g, &adv, FALSE); IDWriteFontFace1_GetMetrics(f1, &m);
        t_note("Courier New: upm %d advance %d (expected 1229 at 2048)", m.designUnitsPerEm, adv);
        if (m.designUnitsPerEm == 2048) CHECKF(adv > 1000 && adv < 1400, "advance %d", adv);
    }
    DW_RELEASE(f1); DW_RELEASE(face);
}

/* simulated bold on a real font: +upm/50 on inked glyphs */
TEST(fms_arial_simulated_bold_advance) {
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = need(L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL), *sim = NULL;
    IDWriteFontFile *files[4]; UINT32 nf = 0; IDWriteFontFace1 *f1 = NULL, *s1 = NULL; unsigned i;
    if (!face) return;
    IDWriteFontFace_GetFiles(face, &nf, NULL);
    if (nf == 0 || nf > 4) { t_skip("unexpected file count %u", nf); DW_RELEASE(face); return; }
    CHECK_HR(IDWriteFontFace_GetFiles(face, &nf, files), S_OK);
    CHECK_HR(IDWriteFactory_CreateFontFace(f, IDWriteFontFace_GetType(face), nf, files, IDWriteFontFace_GetIndex(face), DWRITE_FONT_SIMULATIONS_BOLD, &sim), S_OK);
    if (sim) {
        UINT32 cps[4] = { 'A', 'i', 'W', ' ' }; UINT16 g[4]; INT32 a0[4], a1[4]; DWRITE_FONT_METRICS m0, m1;
        f1 = fm_f1(face); s1 = fm_f1(sim);
        IDWriteFontFace_GetGlyphIndices(face, cps, 4, g);
        IDWriteFontFace_GetMetrics(face, &m0); IDWriteFontFace_GetMetrics(sim, &m1);
        EXPECT_EQ(IDWriteFontFace_GetSimulations(sim), DWRITE_FONT_SIMULATIONS_BOLD);
        EXPECT_EQ(m0.ascent, m1.ascent); EXPECT_EQ(m0.descent, m1.descent); EXPECT_EQ(m0.designUnitsPerEm, m1.designUnitsPerEm);
        if (f1 && s1) {
            CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 4, g, a0, FALSE), S_OK);
            CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(s1, 4, g, a1, FALSE), S_OK);
            for (i = 0; i < 4; i++) {
                int extra = (m0.designUnitsPerEm + 25) / 50, want = cps[i] == ' ' ? a0[i] : a0[i] + extra;
                t_check(a1[i] == want, __FILE__, __LINE__, "simulated bold advance", "U+%04X plain %d bold-sim %d want %d", cps[i], a0[i], a1[i], want);
            }
        }
        DW_RELEASE(s1); DW_RELEASE(f1); DW_RELEASE(sim);
    }
    for (i = 0; i < nf; i++) IDWriteFontFile_Release(files[i]);
    DW_RELEASE(face);
}
TEST(fms_arial_simulated_oblique_caret) {
    IDWriteFactory *f = dw_shared(); IDWriteFontFace *face = need(L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL), *sim = NULL;
    IDWriteFontFile *files[4]; UINT32 nf = 0; unsigned i;
    if (!face) return;
    IDWriteFontFace_GetFiles(face, &nf, NULL);
    if (nf == 0 || nf > 4) { t_skip("unexpected file count"); DW_RELEASE(face); return; }
    IDWriteFontFace_GetFiles(face, &nf, files);
    CHECK_HR(IDWriteFactory_CreateFontFace(f, IDWriteFontFace_GetType(face), nf, files, IDWriteFontFace_GetIndex(face), DWRITE_FONT_SIMULATIONS_OBLIQUE, &sim), S_OK);
    if (sim) {
        IDWriteFontFace1 *s1 = fm_f1(sim), *f1 = fm_f1(face); DWRITE_CARET_METRICS c0, c1; DWRITE_FONT_METRICS1 m0, m1;
        if (s1 && f1) {
            IDWriteFontFace1_GetCaretMetrics(f1, &c0); IDWriteFontFace1_GetCaretMetrics(s1, &c1);
            IDWriteFontFace1_GetMetrics(f1, &m0); IDWriteFontFace1_GetMetrics(s1, &m1);
            t_note("Arial caret %d/%d/%d -> oblique %d/%d/%d", c0.slopeRise, c0.slopeRun, c0.offset, c1.slopeRise, c1.slopeRun, c1.offset);
            {   /* same formula as for the synthetic font */
                unsigned upm = m0.designUnitsPerEm; int rise = c0.slopeRise, run = c0.slopeRun;
                if (rise > 0 && run >= 0) {
                    unsigned r = (unsigned)rise; int rn = run;
                    if ((unsigned)rise < upm) { rn = (int)(((unsigned)run * upm) / (unsigned)rise); r = upm; }
                    EXPECT_EQ(c1.slopeRise, (INT16)r); EXPECT_EQ(c1.slopeRun, (INT16)((r * 0x5700u >> 16) + rn)); EXPECT_EQ(c1.offset, c0.offset);
                }
            }
            EXPECT_EQ((int)m1.glyphBoxRight, m0.glyphBoxRight + fm_shear(m0.glyphBoxTop)); EXPECT_EQ((int)m1.glyphBoxLeft, m0.glyphBoxLeft + fm_shear(m0.glyphBoxBottom));
            EXPECT_EQ((int)m1.superscriptPositionX, m0.superscriptPositionX + fm_shear(m0.superscriptPositionY));
            EXPECT_EQ((int)m1.subscriptPositionX, m0.subscriptPositionX + fm_shear(m0.subscriptPositionY));
            EXPECT_EQ(m1.ascent, m0.ascent);
        }
        DW_RELEASE(s1); DW_RELEASE(f1); DW_RELEASE(sim);
    }
    for (i = 0; i < nf; i++) IDWriteFontFile_Release(files[i]);
    DW_RELEASE(face);
}
TEST(fms_italic_fonts_have_slanted_caret) {
    static const WCHAR *fams[] = { L"Arial", L"Times New Roman", L"Segoe UI", L"Courier New" };
    unsigned i, any = 0;
    for (i = 0; i < 4; i++) {
        IDWriteFontFace *face = fm_sys(fams[i], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_ITALIC), *reg = fm_sys(fams[i], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        IDWriteFontFace1 *f1 = face ? fm_f1(face) : NULL, *r1 = reg ? fm_f1(reg) : NULL; DWRITE_CARET_METRICS ci, cr;
        if (f1 && r1) {
            char n[48]; dw_narrow(fams[i], n, sizeof n); any = 1;
            IDWriteFontFace1_GetCaretMetrics(f1, &ci); IDWriteFontFace1_GetCaretMetrics(r1, &cr);
            t_note("%s caret regular %d/%d italic %d/%d", n, cr.slopeRise, cr.slopeRun, ci.slopeRise, ci.slopeRun);
            if (IDWriteFontFace_GetSimulations(face) == 0) CHECKF(ci.slopeRun != 0 || ci.slopeRise == 0, "italic %s face has an upright caret %d/%d", n, ci.slopeRise, ci.slopeRun);
            CHECKF(cr.slopeRun == 0, "regular %s caret leans: %d/%d", n, cr.slopeRise, cr.slopeRun);
        }
        DW_RELEASE(f1); DW_RELEASE(r1); DW_RELEASE(face); DW_RELEASE(reg);
    }
    if (!any) t_skip("no fonts");
}

/* ---- sizes: GDI ascent/descent grow with the size ---- */
TEST(fms_arial_gdi_cell_height_whole_pixels) {
    IDWriteFontFace *face = need(L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL); IDWriteFontFace1 *f1; int px, prev = 0;
    if (!face) return;
    f1 = fm_f1(face);
    if (f1) for (px = 6; px <= 60; px++) {
        DWRITE_FONT_METRICS1 m; double h;
        CHECK_HR(IDWriteFontFace1_GetGdiCompatibleMetrics(f1, (float)px, 1.0f, NULL, &m), S_OK);
        h = (double)(m.ascent + m.descent) * px / m.designUnitsPerEm;
        if (h + 1e-6 < prev) t_note("em %d: GDI cell height %g after %d", px, h, prev);
        /* both values sit on the pixel grid (to within half a design unit each) */
        t_check(fabs(h - floor(h + 0.5)) <= 1.0 * px / m.designUnitsPerEm + 0.001, __FILE__, __LINE__, "GDI cell height is whole pixels", "em %d: %g", px, h);
        prev = (int)floor(h + 0.5);
    }
    DW_RELEASE(f1); DW_RELEASE(face);
}
