/* Colour glyph translation with the real Segoe UI Emoji (COLR/CPAL v0 on Windows 10 19041) and other system fonts.
 * The expected layers are derived from the font's own COLR/CPAL tables (read through TryGetFontTable) by the oracle in cg.c, so the exact
 * layer lists do not depend on the installed font revision; only the structure of the API is asserted. */
#include "cg.h"

static const UINT32 k_emoji[] = { 0x1F600, 0x1F44D, 0x2764, 0x26C4, 0x1F680, 0x1F4A9, 0x1F525, 0x2B50, 0x1F602, 0x1F60D, 0x1F31F, 0x1F3E0, 0x2705, 0x1F4A1, 0x1F34E };
#define NEMOJI ((int)(sizeof k_emoji / sizeof k_emoji[0]))

typedef struct emo { cgfont x; cg_model m; int have_model; UINT32 ff; } emo;
static int emo_open(emo *e) {
    memset(e, 0, sizeof *e);
    if (!cg_open_sys(&e->x, L"Segoe UI Emoji")) return 0;
    e->ff = e->x.f4 ? (UINT32)e->x.f4->lpVtbl->GetGlyphImageFormats(e->x.f4) : (CGF_TT | CGF_COLR);
    e->have_model = cg_model_from_face(e->x.face, &e->m);
    if (!e->have_model) { t_note("COLR/CPAL v0 tables not parseable (formats 0x%x)", e->ff); }
    return 1;
}
static void emo_close(emo *e) { cg_model_override = NULL; if (e->have_model) cg_model_free(&e->m); cg_close(&e->x); }
static UINT16 gid_of(cgfont *x, UINT32 cp) { UINT16 g = 0; IDWriteFontFace_GetGlyphIndices(x->face, &cp, 1, &g); return g; }
#define EMO(e) emo e; if (!emo_open(&e)) { emo_close(&e); return; } if (!e.have_model) { t_skip("COLR v0 model unavailable"); emo_close(&e); return; } cg_model_override = &e.m

TEST(sys_emoji_font_reports_color) {
    emo e; if (!emo_open(&e)) { emo_close(&e); return; }
    BOOL c = IDWriteFontFace2_IsColorFont(e.x.f2); UINT32 pc = IDWriteFontFace2_GetColorPaletteCount(e.x.f2), pe = IDWriteFontFace2_GetPaletteEntryCount(e.x.f2);
    t_note("Segoe UI Emoji: IsColorFont=%d palettes=%u entries=%u face formats 0x%x", c, pc, pe, e.ff);
    EXPECT_EQ(c, TRUE); CHECK(pc >= 1); CHECK(pe >= 1);
    CHECK((e.ff & CGF_COLR) != 0); CHECK((e.ff & CGF_TT) != 0);
    emo_close(&e);
}
TEST(sys_emoji_codepoints_have_glyphs) {
    emo e; if (!emo_open(&e)) { emo_close(&e); return; } int i, found = 0;
    for (i = 0; i < NEMOJI; i++) { UINT16 g = gid_of(&e.x, k_emoji[i]); if (g) found++; else t_note("U+%X has no glyph", k_emoji[i]); }
    CHECK(found >= NEMOJI / 2);
    emo_close(&e);
}
TEST(sys_emoji_single_glyph_translate_matches_tables) {
    CG_NEED_F2(f); EMO(e); int i, ok = 0;
    for (i = 0; i < NEMOJI; i++) {
        UINT16 g = gid_of(&e.x, k_emoji[i]); static cgrun r; char tag[40]; if (!g) continue;
        cg_mkrun(&r, e.x.face, 72.0f, &g, 1, 0, 0, FALSE, 0); snprintf(tag, sizeof tag, "U+%X gid %u", k_emoji[i], g);
        if (cg_check_f2(f, tag, &r.run, 10, 90, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, e.ff) == S_OK) ok++;
    }
    t_note("%d of %d sample emoji translate to colour layers", ok, NEMOJI);
    CHECK(ok >= 1);
    emo_close(&e);
}
TEST(sys_emoji_every_palette) {
    CG_NEED_F2(f); EMO(e); UINT32 pc = IDWriteFontFace2_GetColorPaletteCount(e.x.f2), p; int i;
    for (p = 0; p < pc && p < 8; p++) for (i = 0; i < NEMOJI; i += 3) {
        UINT16 g = gid_of(&e.x, k_emoji[i]); static cgrun r; char tag[40]; if (!g) continue;
        cg_mkrun(&r, e.x.face, 48.0f, &g, 1, 1, 0, FALSE, 0); snprintf(tag, sizeof tag, "U+%X pal %u", k_emoji[i], p);
        cg_check_f2(f, tag, &r.run, 0, 0, DWRITE_MEASURING_MODE_NATURAL, NULL, p, e.ff);
    }
    emo_close(&e);
}
TEST(sys_emoji_palette_count_is_first_invalid_index) {
    CG_NEED_F2(f); emo e; if (!emo_open(&e)) { emo_close(&e); return; }
    UINT32 pc = IDWriteFontFace2_GetColorPaletteCount(e.x.f2); int i, tested = 0;
    for (i = 0; i < NEMOJI && !tested; i++) {
        UINT16 g = gid_of(&e.x, k_emoji[i]); cgrun r; IDWriteColorGlyphRunEnumerator *en = NULL; if (!g) continue;
        cg_mkrun(&r, e.x.face, 48.0f, &g, 1, 0, 0, FALSE, 0);
        if (FAILED(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &en)) || !en) continue;
        DW_RELEASE(en);
        CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, pc - 1, &en)); DW_RELEASE(en);
        en = (void *)(ULONG_PTR)1; CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, pc, &en), CG_E_NOCOLOR); CHECK_OUT_NULL(en);
        en = (void *)(ULONG_PTR)1; CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0xFFFFFFFFu, &en), CG_E_NOCOLOR); CHECK_OUT_NULL(en);
        tested = 1;
    }
    if (!tested) t_skip("no sample emoji has colour layers");
    emo_close(&e);
}
TEST(sys_emoji_multi_glyph_runs) {
    CG_NEED_F2(f); EMO(e); UINT16 g[8]; int n = 0, i; static cgrun r;
    for (i = 0; i < NEMOJI && n < 8; i++) { UINT16 x = gid_of(&e.x, k_emoji[i]); if (x) g[n++] = x; }
    if (n < 3) { t_skip("not enough emoji glyphs"); emo_close(&e); return; }
    cg_mkrun(&r, e.x.face, 64.0f, g, n, 0, 0, FALSE, 0); cg_check_f2(f, "emoji run", &r.run, 5, 70, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, e.ff);
    cg_mkrun(&r, e.x.face, 64.0f, g, n, 0, 0, FALSE, 1); cg_check_f2(f, "emoji run rtl", &r.run, 600, 70, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, e.ff);
    cg_mkrun(&r, e.x.face, 64.0f, g, n, 2, 1, FALSE, 0); cg_check_f2(f, "emoji run provided", &r.run, 5, 70, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, e.ff);
    emo_close(&e);
}
TEST(sys_emoji_mixed_with_latin_letters) {
    CG_NEED_F2(f); EMO(e); UINT16 a = gid_of(&e.x, 'A'), sp = gid_of(&e.x, ' '), em = gid_of(&e.x, 0x1F600), st = gid_of(&e.x, 0x2B50); UINT16 g[5]; static cgrun r;
    if (!a || !em) { t_skip("glyphs missing"); emo_close(&e); return; }
    g[0] = a; g[1] = em; g[2] = sp ? sp : a; g[3] = st ? st : em; g[4] = a;
    cg_mkrun(&r, e.x.face, 40.0f, g, 5, 0, 0, FALSE, 0); cg_check_f2(f, "latin + emoji", &r.run, 0, 50, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, e.ff);
    emo_close(&e);
}
TEST(sys_emoji_latin_glyph_has_no_color) {
    CG_NEED_F2(f); emo e; if (!emo_open(&e)) { emo_close(&e); return; }
    UINT16 a = gid_of(&e.x, 'A'); cgrun r; IDWriteColorGlyphRunEnumerator *en = (void *)(ULONG_PTR)1;
    if (!a) { t_skip("no glyph for A"); emo_close(&e); return; }
    cg_mkrun(&r, e.x.face, 20.0f, &a, 1, 0, 0, FALSE, 0);
    CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &en), CG_E_NOCOLOR); CHECK_OUT_NULL(en);
    emo_close(&e);
}
TEST(sys_emoji_layers_are_single_glyph_runs_on_the_same_face) {
    CG_NEED_F2(f); emo e; if (!emo_open(&e)) { emo_close(&e); return; } int i, saw = 0;
    for (i = 0; i < NEMOJI; i++) {
        UINT16 g = gid_of(&e.x, k_emoji[i]); cgrun r; IDWriteColorGlyphRunEnumerator *en = NULL; BOOL has; int k = 0; if (!g) continue;
        cg_mkrun(&r, e.x.face, 36.0f, &g, 1, 0, 0, FALSE, 0);
        if (FAILED(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &en)) || !en) continue;
        while (SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(en, &has)) && has && k < 200) {
            const DWRITE_COLOR_GLYPH_RUN *c = NULL; IDWriteColorGlyphRunEnumerator_GetCurrentRun(en, &c);
            if (c && k > 0) { CHECKF(c->glyphRun.glyphCount == 1 && c->glyphRun.fontFace == e.x.face, "U+%X layer %d", k_emoji[i], k); CHECKF(c->glyphRun.glyphIndices[0] != g, "U+%X layer %d is the base glyph", k_emoji[i], k);
                CHECKF(c->paletteIndex == 0xFFFF || c->paletteIndex < IDWriteFontFace2_GetPaletteEntryCount(e.x.f2), "U+%X palette index %u", k_emoji[i], c->paletteIndex);
                CHECK(c->runColor.r >= 0 && c->runColor.r <= 1 && c->runColor.g >= 0 && c->runColor.g <= 1 && c->runColor.b >= 0 && c->runColor.b <= 1 && c->runColor.a >= 0 && c->runColor.a <= 1); }
            k++;
        }
        t_note("U+%X (gid %u): %d runs", k_emoji[i], g, k); if (k > 1) saw = 1;
        DW_RELEASE(en);
    }
    CHECK(saw);
    emo_close(&e);
}
TEST(sys_emoji_grinning_face_has_many_layers) {
    CG_NEED_F2(f); emo e; if (!emo_open(&e)) { emo_close(&e); return; }
    UINT16 g = gid_of(&e.x, 0x1F600); cgrun r; IDWriteColorGlyphRunEnumerator *en = NULL; BOOL has; int k = 0;
    if (!g) { t_skip("no glyph for U+1F600"); emo_close(&e); return; }
    cg_mkrun(&r, e.x.face, 36.0f, &g, 1, 0, 0, FALSE, 0);
    CHECK_HR_OK(IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &en));
    while (en && SUCCEEDED(IDWriteColorGlyphRunEnumerator_MoveNext(en, &has)) && has && k < 200) k++;
    t_note("U+1F600 enumerates %d runs (1 base + layers)", k); CHECK(k >= 3);
    DW_RELEASE(en); emo_close(&e);
}
TEST(sys_emoji_factory4_formats) {
    CG_NEED_F4(f); emo e; if (!emo_open(&e)) { emo_close(&e); return; }
    UINT16 g = 0; int i; for (i = 0; i < NEMOJI && !g; i++) g = gid_of(&e.x, k_emoji[i]);
    if (!g) { t_skip("no emoji glyph"); emo_close(&e); return; }
    cgrun r; D2D1_POINT_2F o = { 0, 0 }; IDWriteColorGlyphRunEnumerator1 *en;
    cg_mkrun(&r, e.x.face, 36.0f, &g, 1, 0, 0, FALSE, 0);
    static const struct { UINT32 fm; HRESULT hr; } t[] = { { 0, CG_E_NOCOLOR }, { CGF_TT, CG_E_NOCOLOR }, { CGF_CFF, CG_E_NOCOLOR }, { CGF_COLR, S_OK }, { CGF_TT | CGF_COLR, S_OK }, { 0x100, CG_E_INVALIDARG }, { CGF_PNG, CG_E_NOCOLOR }, { CGF_JPEG | CGF_TIFF, CG_E_NOCOLOR } };
    for (i = 0; i < 8; i++) {
        en = (void *)(ULONG_PTR)1; HRESULT hr = IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)t[i].fm, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &en);
        if (t[i].fm == CGF_PNG || t[i].fm == (CGF_JPEG | CGF_TIFF)) { if (hr != t[i].hr) t_note("formats 0x%x -> %s", t[i].fm, dw_hr_name(hr)); }
        else CHECKF(hr == t[i].hr || (t[i].hr == S_OK && hr == CG_E_NOCOLOR), "formats 0x%x -> %s", t[i].fm, dw_hr_name(hr));
        if (SUCCEEDED(hr)) { DW_RELEASE(en); } else CHECK_OUT_NULL(en);
    }
    emo_close(&e);
}
TEST(sys_emoji_factory4_matches_tables) {
    CG_NEED_F4(f); EMO(e); int i;
    for (i = 0; i < NEMOJI; i++) {
        UINT16 g = gid_of(&e.x, k_emoji[i]); static cgrun r; char tag[40]; if (!g) continue;
        cg_mkrun(&r, e.x.face, 80.0f, &g, 1, 1, 0, FALSE, 0); snprintf(tag, sizeof tag, "f4 U+%X COLR", k_emoji[i]);
        cg_check_f4(f, tag, &r.run, 3, 4, CGF_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, e.ff);
        snprintf(tag, sizeof tag, "f4 U+%X TT|COLR", k_emoji[i]);
        cg_check_f4(f, tag, &r.run, 3, 4, CGF_TT | CGF_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, e.ff);
    }
    emo_close(&e);
}
TEST(sys_non_color_fonts_reject_colr_format) {
    CG_NEED_F4(f); static const WCHAR *const fam[] = { L"Arial", L"Segoe UI", L"Times New Roman", L"Courier New", L"Tahoma" }; int i;
    for (i = 0; i < 5; i++) {
        cgfont x; if (!cg_open_sys(&x, fam[i])) { cg_close(&x); continue; }
        UINT32 cp = 'A'; UINT16 g = 0; cgrun r; D2D1_POINT_2F o = { 0, 0 }; IDWriteColorGlyphRunEnumerator1 *en = (void *)(ULONG_PTR)1;
        IDWriteFontFace_GetGlyphIndices(x.face, &cp, 1, &g);
        cg_mkrun(&r, x.face, 20.0f, &g, 1, 0, 0, FALSE, 0);
        CHECKF(IDWriteFactory4_TranslateColorGlyphRun(f, o, &r.run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &en) == CG_E_NOCOLOR && !en, "%ls", fam[i]);
        cg_close(&x);
    }
}
TEST(sys_segoe_ui_symbol_color_state_note) {
    CG_NEED_F2(f); cgfont x; if (!cg_open_sys(&x, L"Segoe UI Symbol")) { cg_close(&x); return; }
    BOOL c = IDWriteFontFace2_IsColorFont(x.f2); t_note("Segoe UI Symbol IsColorFont=%d palettes=%u", c, IDWriteFontFace2_GetColorPaletteCount(x.f2));
    UINT32 cps[] = { 0x2764, 0x2605, 0x263A, 0x2600 }; int i;
    for (i = 0; i < 4; i++) {
        UINT16 g = 0; cgrun r; IDWriteColorGlyphRunEnumerator *en = (void *)(ULONG_PTR)1; IDWriteFontFace_GetGlyphIndices(x.face, &cps[i], 1, &g); if (!g) continue;
        cg_mkrun(&r, x.face, 24.0f, &g, 1, 0, 0, FALSE, 0);
        HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f, 0, 0, &r.run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &en);
        t_note("Segoe UI Symbol U+%04X -> %s", cps[i], dw_hr_name(hr)); if (!c) CHECK_HR(hr, CG_E_NOCOLOR);
        if (SUCCEEDED(hr)) { DW_RELEASE(en); } else CHECK_OUT_NULL(en);
    }
    cg_close(&x);
}
TEST(sys_emoji_palette_entries_match_cpal) {
    emo e; if (!emo_open(&e)) { emo_close(&e); return; }
    if (!e.have_model) { t_skip("CPAL not parseable"); emo_close(&e); return; }
    UINT32 pc = IDWriteFontFace2_GetColorPaletteCount(e.x.f2), pe = IDWriteFontFace2_GetPaletteEntryCount(e.x.f2), p, i; EXPECT_EQ(pc, (UINT32)e.m.npal); EXPECT_EQ(pe, (UINT32)e.m.nent);
    for (p = 0; p < pc && p < 4; p++) {
        DWRITE_COLOR_F *c = (DWRITE_COLOR_F *)calloc(pe, sizeof *c); CHECK_HR_OK(IDWriteFontFace2_GetPaletteEntries(e.x.f2, p, 0, pe, c));
        for (i = 0; i < pe; i++) { uint32_t v = e.m.argb[p * pe + i]; CHECKF(DW_APPROX(c[i].r, ((v >> 16) & 255) / 255.0f, 1e-6) && DW_APPROX(c[i].a, (v >> 24) / 255.0f, 1e-6) && DW_APPROX(c[i].g, ((v >> 8) & 255) / 255.0f, 1e-6) && DW_APPROX(c[i].b, (v & 255) / 255.0f, 1e-6), "palette %u entry %u", p, i); }
        free(c);
    }
    emo_close(&e);
}
