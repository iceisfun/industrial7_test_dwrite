/* Typography queries on installed fonts: only invariants that hold for any font are hard (sorted unique lists, repeatability, agreement
 * between GetTypographicFeatures and CheckTypographicFeature); exact feature sets of fonts are noted for the implementer. */
#include "r2.h"

static UINT32 bswap(UINT32 v) { return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24); }
static const WCHAR *k_fonts[] = { L"Arial", L"Times New Roman", L"Courier New", L"Segoe UI", L"Tahoma", L"Consolas", L"Bahnschrift", L"Calibri", L"Cambria", L"Verdana", L"Georgia", L"Segoe UI Symbol", L"Microsoft Sans Serif" };
static const struct { const WCHAR *s; const char *name; } k_scripts[] = {
    { L"A", "Latin" }, { L"\x0391", "Greek" }, { L"\x0416", "Cyrillic" }, { L"\x0627", "Arabic" }, { L"\x05D0", "Hebrew" }, { L"\x0E01", "Thai" },
    { L"\x0905", "Devanagari" }, { L"\x4E00", "Han" }, { L"\x3042", "Hiragana" }, { L"\xAC00", "Hangul" },
};
static void tagstr(UINT32 t, char *o) { o[0] = (char)t; o[1] = (char)(t >> 8); o[2] = (char)(t >> 16); o[3] = (char)(t >> 24); o[4] = 0; }

RT(sys_features_are_sorted_unique_and_stable) {
    R2_BEGIN(); unsigned fi, si, tested = 0;
    for (fi = 0; fi < sizeof k_fonts / sizeof *k_fonts; fi++) {
        IDWriteFontFace *face = dw_sysface(f, k_fonts[fi], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!face) continue;
        tested++;
        for (si = 0; si < sizeof k_scripts / sizeof *k_scripts; si++) {
            DWRITE_SCRIPT_ANALYSIS sa = r2_sa_of(&c_, k_scripts[si].s, (UINT32)lstrlenW(k_scripts[si].s)); UINT32 n = 0, m = 0, i; DWRITE_FONT_FEATURE_TAG t[128], u[128];
            HRESULT hr = r2_features(&c_, face, sa, L"en-US", 0, &n, NULL);
            CHECKF(hr == S_OK || hr == R2_E_BUFFER, "%ls/%s: size query %s", k_fonts[fi], k_scripts[si].name, dw_hr_name(hr));
            CHECKF((hr == S_OK) == (n == 0), "%ls/%s: hr %s for %u tags", k_fonts[fi], k_scripts[si].name, dw_hr_name(hr), n);
            if (n == 0 || n > 128) continue;
            CHECK_HR(r2_features(&c_, face, sa, L"en-US", 128, &m, t), S_OK); EXPECT_EQ(m, n);
            CHECK_HR(r2_features(&c_, face, sa, NULL, 128, &m, u), S_OK); EXPECT_EQ(m, n);
            CHECKF(!memcmp(t, u, n * sizeof t[0]), "%ls/%s: en-US and NULL locale differ", k_fonts[fi], k_scripts[si].name);
            for (i = 1; i < n; i++) CHECKF(bswap(t[i - 1]) < bswap(t[i]), "%ls/%s: slot %u not strictly ascending", k_fonts[fi], k_scripts[si].name, i);
            if (si == 0) { char b[600]; int o = 0; for (i = 0; i < n && o < 560; i++) { char s[5]; tagstr(t[i], s); o += sprintf(b + o, "%s ", s); } t_note("%ls latin features (%u): %s", k_fonts[fi], n, b); }
        }
        IDWriteFontFace_Release(face);
    }
    if (!tested) t_skip("none of the reference fonts is installed");
    R2_END();
}
RT(sys_listed_features_check_consistently) {
    /* CheckTypographicFeature must flag at least the glyphs of a feature that is listed in nearly every case; the hard part is that the
     * query never fails and never sets values other than 0/1 */
    R2_BEGIN(); unsigned fi, tested = 0;
    for (fi = 0; fi < sizeof k_fonts / sizeof *k_fonts; fi++) {
        IDWriteFontFace *face = dw_sysface(f, k_fonts[fi], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        DWRITE_FONT_FEATURE_TAG t[128]; UINT32 n = 0, i, g; DWRITE_SCRIPT_ANALYSIS sa;
        static UINT16 gl[4096]; static UINT8 out[4096]; UINT16 ng;
        if (!face) continue;
        tested++; sa = r2_sa_of(&c_, L"A", 1); ng = IDWriteFontFace_GetGlyphCount(face); if (ng > 4096) ng = 4096;
        for (g = 0; g < ng; g++) gl[g] = (UINT16)g;
        if (r2_features(&c_, face, sa, NULL, 128, &n, t) != S_OK) { IDWriteFontFace_Release(face); continue; }
        for (i = 0; i < n; i++) {
            unsigned hits = 0, bad = 0; char s[5]; tagstr(t[i], s);
            memset(out, 0xEE, sizeof out);
            CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, face, sa, NULL, t[i], ng, gl, out), S_OK);
            for (g = 0; g < ng; g++) { if (out[g] > 1) bad++; else hits += out[g]; }
            CHECKF(bad == 0, "%ls feature %s: %u flags outside 0/1", k_fonts[fi], s, bad);
            t_note("%ls '%s' covers %u of %u glyphs", k_fonts[fi], s, hits, ng);
        }
        IDWriteFontFace_Release(face);
    }
    if (!tested) t_skip("none of the reference fonts is installed");
    R2_END();
}
RT(sys_unlisted_tags_flag_nothing) {
    R2_BEGIN(); unsigned fi, tested = 0;
    for (fi = 0; fi < sizeof k_fonts / sizeof *k_fonts; fi++) {
        IDWriteFontFace *face = dw_sysface(f, k_fonts[fi], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        DWRITE_FONT_FEATURE_TAG t[128]; UINT32 n = 0, i; UINT16 gl[8] = { 0, 1, 2, 3, 36, 68, 100, 200 }; UINT8 out[8]; DWRITE_SCRIPT_ANALYSIS sa;
        static const char *probe[] = { "zzzz", "abcd", "    ", "q9q9", "ss99", "xxxx" };
        if (!face) continue;
        tested++; sa = r2_sa_of(&c_, L"A", 1);
        if (r2_features(&c_, face, sa, NULL, 128, &n, t) != S_OK) { IDWriteFontFace_Release(face); continue; }
        for (i = 0; i < sizeof probe / sizeof *probe; i++) {
            UINT32 tag = r2_tag(probe[i]), j; int listed = 0, k;
            for (j = 0; j < n; j++) if (t[j] == tag) listed = 1;
            if (listed) continue;
            memset(out, 0xEE, sizeof out);
            CHECK_HR(IDWriteTextAnalyzer2_CheckTypographicFeature(a2, face, sa, NULL, (DWRITE_FONT_FEATURE_TAG)tag, 8, gl, out), S_OK);
            for (k = 0; k < 8; k++) CHECKF(out[k] == 0, "%ls tag %s glyph %u flagged", k_fonts[fi], probe[i], gl[k]);
        }
        IDWriteFontFace_Release(face);
    }
    if (!tested) t_skip("none of the reference fonts is installed");
    R2_END();
}
RT(sys_complexity_latin_matches_glyph_indices) {
    R2_BEGIN(); unsigned fi, tested = 0;
    for (fi = 0; fi < sizeof k_fonts / sizeof *k_fonts; fi++) {
        IDWriteFontFace *face = dw_sysface(f, k_fonts[fi], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        static const WCHAR txt[] = L"Hello, World 0123456789";
        BOOL simple = 0; UINT32 read = 0, n = (UINT32)lstrlenW(txt), i; UINT16 g[32], gi[32]; UINT32 cps[32];
        if (!face) continue;
        tested++;
        CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(a1, txt, n, face, &simple, &read, g), S_OK);
        for (i = 0; i < n; i++) cps[i] = txt[i];
        CHECK_HR(IDWriteFontFace_GetGlyphIndices(face, cps, n, gi), S_OK);
        if (simple) { EXPECT_EQ(read, n); for (i = 0; i < n; i++) CHECKF(g[i] == gi[i], "%ls char %u: glyph %u vs %u", k_fonts[fi], i, g[i], gi[i]); }
        else t_note("%ls: Latin pangram not simple (read %u of %u)", k_fonts[fi], read, n);
        IDWriteFontFace_Release(face);
    }
    if (!tested) t_skip("none of the reference fonts is installed");
    R2_END();
}
RT(sys_complexity_note_scripts) {
    R2_BEGIN(); unsigned fi, si, tested = 0;
    for (fi = 0; fi < sizeof k_fonts / sizeof *k_fonts; fi++) {
        IDWriteFontFace *face = dw_sysface(f, k_fonts[fi], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!face) continue;
        tested++;
        for (si = 0; si < sizeof k_scripts / sizeof *k_scripts; si++) {
            BOOL simple = 0; UINT32 read = 0; UINT16 g[4]; UINT32 n = (UINT32)lstrlenW(k_scripts[si].s);
            CHECK_HR(IDWriteTextAnalyzer1_GetTextComplexity(a1, k_scripts[si].s, n, face, &simple, &read, g), S_OK);
            CHECKF(read >= 1 && read <= n, "%ls/%s: read %u of %u", k_fonts[fi], k_scripts[si].name, read, n);
            t_note("%ls %s sample: simple=%d glyph=%u", k_fonts[fi], k_scripts[si].name, simple, g[0]);
        }
        IDWriteFontFace_Release(face);
    }
    if (!tested) t_skip("none of the reference fonts is installed");
    R2_END();
}
RT(sys_legacy_kern_table_is_not_an_opentype_feature) {
    /* only GSUB / GPOS feed the feature list: a font with a legacy 'kern' table but no layout tables lists nothing */
    R2_BEGIN(); ttf_spec s; static const ttf_kernpair kp[] = { { 2, 3, -50 } }; uint8_t *blob; size_t n; dwx_font h; DWRITE_FONT_FEATURE_TAG t[4]; UINT32 cnt = 99;
    ttf_default_spec(&s); s.kern = kp; s.nkern = 1;
    blob = ttf_build(&s, &n);
    if (!blob) { R2_END(); t_skip("builder failed"); return; }
    if (FAILED(dwx_font_open(&h, f, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE)) || !h.face) { free(blob); dwx_font_close(&h); R2_END(); t_skip("font rejected"); return; }
    CHECK_HR(r2_features(&c_, h.face, r2_sa_of(&c_, L"A", 1), NULL, 4, &cnt, t), S_OK); EXPECT_EQ(cnt, 0);
    free(blob); dwx_font_close(&h); R2_END();
}
