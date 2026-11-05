/* The situation-specific codes: DWRITE_E_NOCOLOR, DWRITE_E_UNSUPPORTEDOPERATION (raw CFF faces), DWRITE_E_TEXTRENDERERINCOMPATIBLE,
 * DWRITE_E_FLOWDIRECTIONCONFLICTS and failures that application renderer callbacks inject into IDWriteTextLayout::Draw. */
#include "eh.h"

/* ---- colour fixture: 'A' has two COLR layers (triangle red, round green), every other glyph is monochrome ----------------------------- */
static const ttf_colr_base cbase[] = { { 2, 0, 2 } };
static const ttf_colr_layer clay[] = { { TTF_GID_TRIANGLE, 0 }, { TTF_GID_ROUND, 1 } };
static const uint32_t cpal[] = { 0xFFFF0000u, 0xFF00FF00u };
static uint8_t *color_blob(size_t *n) {
    ttf_spec s; ttf_default_spec(&s);
    s.colr_base = cbase; s.ncolr_base = 1; s.colr_layers = clay; s.ncolr_layers = 2; s.palette = cpal; s.npalette = 2;
    return ttf_build(&s, n);
}
typedef struct { dwx_font h; uint8_t *blob; int ok; } cface;
static int cface_open(cface *c, IDWriteFactory *f, int color) {
    size_t n; memset(c, 0, sizeof *c);
    c->blob = color ? color_blob(&n) : ttf_build_default(&n);
    c->ok = c->blob && SUCCEEDED(dwx_font_open(&c->h, f, c->blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE)) && c->h.face;
    return c->ok;
}
static void cface_close(cface *c) { dwx_font_close(&c->h); free(c->blob); }
static DWRITE_GLYPH_RUN make_run(IDWriteFontFace *face, UINT16 *idx, float *adv, UINT32 n) {
    DWRITE_GLYPH_RUN r; memset(&r, 0, sizeof r); r.fontFace = face; r.fontEmSize = 32.f; r.glyphCount = n; r.glyphIndices = idx; r.glyphAdvances = adv; return r;
}

TEST(nocolor_monochrome_font_translate_run) {
    NEED_FACTORY(); cface c; IDWriteFactory2 *f2 = NULL;
    if (cface_open(&c, f, 0) && SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2)) && f2) {
        UINT16 idx[2] = { 2, 3 }; float adv[2] = { 20, 20 }; DWRITE_GLYPH_RUN run = make_run(c.h.face, idx, adv, 2); IDWriteColorGlyphRunEnumerator *e = SENT(IDWriteColorGlyphRunEnumerator *);
        /* the run has no coloured glyph: the enumerator is not created and the pre-set result stays DWRITE_E_NOCOLOR */
        CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f2, 0.f, 0.f, &run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), EH_NOCOLOR); CHECK_OUT_NULL(e);
        e = SENT(IDWriteColorGlyphRunEnumerator *);
        CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f2, 10.f, 20.f, &run, NULL, DWRITE_MEASURING_MODE_GDI_NATURAL, NULL, 0, &e), EH_NOCOLOR); CHECK_OUT_NULL(e);
        e = SENT(IDWriteColorGlyphRunEnumerator *);
        CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f2, 0.f, 0.f, &run, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 5, &e), EH_NOCOLOR); CHECK_OUT_NULL(e);   /* palette index is irrelevant */
        DW_RELEASE(f2);
    } else t_skip("IDWriteFactory2 or the synthetic face unavailable");
    cface_close(&c); DONE_FACTORY();
}
TEST(nocolor_translate_run_argument_checks) {
    NEED_FACTORY(); cface c; IDWriteFactory2 *f2 = NULL;
    if (cface_open(&c, f, 0) && SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2)) && f2) {
        IDWriteColorGlyphRunEnumerator *e = SENT(IDWriteColorGlyphRunEnumerator *);
        if (!t_is_wine()) { CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f2, 0.f, 0.f, NULL, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), E_INVALIDARG); CHECK_OUT_NULL(e); }
        DW_RELEASE(f2);
    } else t_skip("IDWriteFactory2 or the synthetic face unavailable");
    cface_close(&c); DONE_FACTORY();
}
TEST(color_font_translate_run_succeeds_for_coloured_glyph) {
    NEED_FACTORY(); cface c; IDWriteFactory2 *f2 = NULL;
    if (cface_open(&c, f, 1) && SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory2, (void **)&f2)) && f2) {
        UINT16 idxc[1] = { 2 }, idxm[1] = { 3 }; float adv[1] = { 20 }; DWRITE_GLYPH_RUN rc = make_run(c.h.face, idxc, adv, 1), rm = make_run(c.h.face, idxm, adv, 1);
        IDWriteColorGlyphRunEnumerator *e = NULL;
        HRESULT hr = IDWriteFactory2_TranslateColorGlyphRun(f2, 0.f, 0.f, &rc, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
        CHECK_HR(hr, S_OK); CHECK(e != NULL);
        if (e) {
            BOOL more = FALSE; CHECK_HR(IDWriteColorGlyphRunEnumerator_MoveNext(e, &more), S_OK); CHECK(more);
            DW_RELEASE(e);
        }
        e = SENT(IDWriteColorGlyphRunEnumerator *);
        /* a font that has colour data still answers NOCOLOR for a run made of monochrome glyphs only */
        CHECK_HR(IDWriteFactory2_TranslateColorGlyphRun(f2, 0.f, 0.f, &rm, NULL, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), EH_NOCOLOR); CHECK_OUT_NULL(e);
        DW_RELEASE(f2);
    } else t_skip("IDWriteFactory2 or the colour face unavailable");
    cface_close(&c); DONE_FACTORY();
}
TEST(nocolor_factory4_translate_formats) {
    NEED_FACTORY(); cface c; IDWriteFactory4 *f4 = NULL;
    if (cface_open(&c, f, 0) && SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory4, (void **)&f4)) && f4) {
        UINT16 idx[2] = { 2, 3 }; float adv[2] = { 20, 20 }; DWRITE_GLYPH_RUN run = make_run(c.h.face, idx, adv, 2); IDWriteColorGlyphRunEnumerator1 *e = SENT(IDWriteColorGlyphRunEnumerator1 *);
        D2D1_POINT_2F org = { 0.f, 0.f };
        CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f4, org, &run, NULL, DWRITE_GLYPH_IMAGE_FORMATS_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), EH_NOCOLOR); CHECK_OUT_NULL(e);
        e = SENT(IDWriteColorGlyphRunEnumerator1 *);
        CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f4, org, &run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x7, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), EH_NOCOLOR); CHECK_OUT_NULL(e);
        if (!t_is_wine()) {
            /* formats outside the low byte are rejected (checked before the font is consulted); a NULL run too */
            e = SENT(IDWriteColorGlyphRunEnumerator1 *);
            CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f4, org, &run, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)0x100, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), E_INVALIDARG); CHECK_OUT_NULL(e);
            e = SENT(IDWriteColorGlyphRunEnumerator1 *);
            CHECK_HR(IDWriteFactory4_TranslateColorGlyphRun(f4, org, NULL, NULL, DWRITE_GLYPH_IMAGE_FORMATS_COLR, DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e), E_INVALIDARG); CHECK_OUT_NULL(e);
        }
        DW_RELEASE(f4);
    } else t_skip("IDWriteFactory4 or the synthetic face unavailable");
    cface_close(&c); DONE_FACTORY();
}
TEST(nocolor_palette_entries_on_monochrome_face) {
    NEED_FACTORY(); cface c; IDWriteFontFace2 *f2 = NULL;
    if (cface_open(&c, f, 0) && SUCCEEDED(IDWriteFontFace_QueryInterface(c.h.face, &IID_IDWriteFontFace2, (void **)&f2)) && f2) {
        DWRITE_COLOR_F col[4]; 
        CHECK(!IDWriteFontFace2_IsColorFont(f2)); EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(f2), 0); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(f2), 0);
        /* GetPalette(idx) is NULL for every index when the font has no CPAL: DWRITE_E_NOCOLOR */
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 0, 1, col), EH_NOCOLOR);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 0, 0, col), EH_NOCOLOR);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 1, 0, 1, col), EH_NOCOLOR);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0xFFFFFFFFu, 0, 1, col), EH_NOCOLOR);
        DW_RELEASE(f2);
    } else t_skip("IDWriteFontFace2 unavailable");
    cface_close(&c); DONE_FACTORY();
}
TEST(palette_entries_range_checks_on_colour_face) {
    if (t_is_wine()) { t_skip("invalid palette ranges fault in the emulated implementation"); return; }
    NEED_FACTORY(); cface c; IDWriteFontFace2 *f2 = NULL;
    if (cface_open(&c, f, 1) && SUCCEEDED(IDWriteFontFace_QueryInterface(c.h.face, &IID_IDWriteFontFace2, (void **)&f2)) && f2) {
        DWRITE_COLOR_F col[4]; memset(col, 0, sizeof col);
        CHECK(IDWriteFontFace2_IsColorFont(f2)); EXPECT_EQ(IDWriteFontFace2_GetColorPaletteCount(f2), 1); EXPECT_EQ(IDWriteFontFace2_GetPaletteEntryCount(f2), 2);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 0, 2, col), S_OK);
        CHECK_NEAR(col[0].r, 1.f, 0.01); CHECK_NEAR(col[0].g, 0.f, 0.01); CHECK_NEAR(col[0].b, 0.f, 0.01); CHECK_NEAR(col[0].a, 1.f, 0.01);
        CHECK_NEAR(col[1].r, 0.f, 0.01); CHECK_NEAR(col[1].g, 1.f, 0.01);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 1, 1, col), S_OK);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 2, 0, col), S_OK);        /* empty range at the end: first == count, remaining 0 */
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 0, 0, col), S_OK);
        /* first > count, or count > count - first */
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 3, 0, col), E_INVALIDARG);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 0, 3, col), E_INVALIDARG);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 1, 2, col), E_INVALIDARG);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 2, 1, col), E_INVALIDARG);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 0xFFFFFFFFu, 1, col), E_INVALIDARG);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 0, 1, 0xFFFFFFFFu, col), E_INVALIDARG);
        /* palette index beyond the palette count: NOCOLOR (checked before the entry range) */
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 1, 0, 1, col), EH_NOCOLOR);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 7, 0, 0, col), EH_NOCOLOR);
        CHECK_HR(IDWriteFontFace2_GetPaletteEntries(f2, 1, 99, 99, col), EH_NOCOLOR);
        DW_RELEASE(f2);
    } else t_skip("colour face unavailable");
    cface_close(&c); DONE_FACTORY();
}

/* ---- DWRITE_E_UNSUPPORTEDOPERATION on a raw CFF face ------------------------------------------------------------------------------ */
static size_t build_raw_cff(uint8_t *o) {
    /* header, Name INDEX "Eh", Top DICT INDEX (CharStrings @37, empty Private @43), empty String / Global Subr INDEXes, CharStrings INDEX with one 'endchar' */
    static const uint8_t cff[] = {
        0x01, 0x00, 0x04, 0x01,
        0x00, 0x01, 0x01, 0x01, 0x03, 'E', 'h',
        0x00, 0x01, 0x01, 0x01, 0x12,
        0x1d, 0x00, 0x00, 0x00, 0x25, 0x11,
        0x1d, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x2b, 0x12,
        0x00, 0x00,
        0x00, 0x00,
        0x00, 0x01, 0x01, 0x01, 0x02, 0x0e,
    };
    memcpy(o, cff, sizeof cff); return sizeof cff;
}
typedef struct { dwx_mem_loader *ld; IDWriteFontFile *file; IDWriteFontFace *face; HRESULT hr; } rawface;
static int rawface_open(rawface *r, IDWriteFactory *f) {
    uint8_t cff[64]; size_t n = build_raw_cff(cff); UINT32 key;
    memset(r, 0, sizeof *r);
    r->ld = dwx_loader_new(); key = dwx_loader_add(r->ld, cff, n);
    if (FAILED(dwx_loader_register(r->ld, f)) || FAILED(dwx_loader_file(r->ld, f, key, &r->file))) return 0;
    r->hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_RAW_CFF, 1, &r->file, 0, DWRITE_FONT_SIMULATIONS_NONE, &r->face);
    return SUCCEEDED(r->hr) && r->face;
}
static void rawface_close(rawface *r) { DW_RELEASE(r->face); DW_RELEASE(r->file); if (r->ld) { dwx_loader_unregister(r->ld); dwx_release(r->ld); } }
TEST(unsupported_raw_cff_face_operations) {
    NEED_FACTORY(); rawface r;
    if (!rawface_open(&r, f)) { t_note("CreateFontFace(RAW_CFF) over a minimal bare CFF blob = %s", dw_hr_name(r.hr)); rawface_close(&r); DONE_FACTORY(); t_skip("minimal bare CFF not accepted"); return; }
    /* Face types 3 (Type 1) and 7 (raw CFF) have no sfnt tables: operations that need them throw NotSupportedException. */
    UINT16 idx[2] = { 0, 0 }; DWRITE_GLYPH_METRICS gm[2];
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(r.face, idx, 2, gm, FALSE), EH_UNSUPPORTEDOPERATION);
    { const void *d = SENT(const void *); UINT32 sz = 9; void *ctx = SENT(void *); BOOL ex = 5;
      CHECK_HR(IDWriteFontFace_TryGetFontTable(r.face, DW_TAG('h', 'e', 'a', 'd'), &d, &sz, &ctx, &ex), EH_UNSUPPORTEDOPERATION);
      CHECK(d == NULL && sz == 0 && ctx == NULL && ex == 0); }
    { DWRITE_FONT_METRICS fm; memset(&fm, 0xAA, sizeof fm); CHECK_HR(IDWriteFontFace_GetGdiCompatibleMetrics(r.face, 16.f, 1.f, NULL, &fm), EH_UNSUPPORTEDOPERATION); EXPECT_EQ(fm.designUnitsPerEm, 0); }
    { IDWriteGdiInterop *g = NULL; LOGFONTW lf;
      if (SUCCEEDED(IDWriteFactory_GetGdiInterop(f, &g)) && g) { CHECK_HR(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, r.face, &lf), EH_UNSUPPORTEDOPERATION); IDWriteGdiInterop_Release(g); } }
    { IDWriteFontFace1 *f1 = NULL;
      if (SUCCEEDED(IDWriteFontFace_QueryInterface(r.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
          INT32 adv[2]; UINT16 pair[2] = { 1, 2 }; INT32 ka[2];
          CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 2, idx, adv, TRUE), EH_UNSUPPORTEDOPERATION);       /* only the sideways form is refused */
          CHECK_HR(IDWriteFontFace1_GetKerningPairAdjustments(f1, 2, pair, ka), EH_UNSUPPORTEDOPERATION);
          DW_RELEASE(f1);
      } }
    { IDWriteTextAnalyzer *a = NULL; DWRITE_SCRIPT_ANALYSIS sa = { 0, 0 }; UINT16 cm[4], gi[8]; DWRITE_SHAPING_TEXT_PROPERTIES tp[4]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[8]; UINT32 n = 0;
      if (SUCCEEDED(IDWriteFactory_CreateTextAnalyzer(f, &a)) && a) {
          CHECK_HR(IDWriteTextAnalyzer_GetGlyphs(a, L"abc", 3, r.face, FALSE, FALSE, &sa, L"en-us", NULL, NULL, NULL, 0, 8, cm, tp, gi, gp, &n), EH_UNSUPPORTEDOPERATION);
          DW_RELEASE(a);
      } }
    rawface_close(&r); DONE_FACTORY();
}

/* ---- FLOWDIRECTIONCONFLICTS / TEXTRENDERERINCOMPATIBLE -------------------------------------------------------------------------------- */
static int vertical_axis(int v) { return (v >> 1) & 1; }
static IDWriteTextLayout *dir_layout(IDWriteFactory *f, DWRITE_READING_DIRECTION rd, DWRITE_FLOW_DIRECTION fd) {
    IDWriteTextLayout *l = eh_layout(f, L"Hello world", 200.f, 100.f, NULL);
    if (l) { IDWriteTextLayout_SetReadingDirection(l, rd); IDWriteTextLayout_SetFlowDirection(l, fd); }
    return l;
}
TEST(flowdirection_conflict_matrix_via_metrics) {
    NEED_FACTORY(); int rd, fd;
    /* TextLayout construction throws OSException(DWRITE_E_FLOWDIRECTIONCONFLICTS) when the reading axis and the flow axis are parallel:
     * horizontal reading (LTR/RTL) needs a vertical flow (TOP_TO_BOTTOM/BOTTOM_TO_TOP), vertical reading needs a horizontal flow. */
    for (rd = 0; rd < 4; rd++) for (fd = 0; fd < 4; fd++) {
        IDWriteTextLayout *l = dir_layout(f, (DWRITE_READING_DIRECTION)rd, (DWRITE_FLOW_DIRECTION)fd);
        if (!l) continue;
        UINT32 n = 55; DWRITE_TEXT_METRICS tm; FLOAT mw;
        int conflict = vertical_axis(rd) != vertical_axis(fd);
        HRESULT want = conflict ? EH_FLOWDIRECTIONCONFLICTS : S_OK;
        HRESULT h1 = IDWriteTextLayout_GetLineMetrics(l, NULL, 0, &n);
        CHECKF(conflict ? h1 == want : (h1 == EH_NSB || h1 == S_OK), "reading %d flow %d GetLineMetrics -> %s", rd, fd, dw_hr_name(h1));
        CHECKF(IDWriteTextLayout_GetMetrics(l, &tm) == want, "reading %d flow %d GetMetrics", rd, fd);
        CHECKF(IDWriteTextLayout_DetermineMinWidth(l, &mw) == want, "reading %d flow %d DetermineMinWidth", rd, fd);
        n = 55; HRESULT h2 = IDWriteTextLayout_GetClusterMetrics(l, NULL, 0, &n);
        CHECKF(conflict ? h2 == want : (h2 == EH_NSB || h2 == S_OK), "reading %d flow %d GetClusterMetrics -> %s", rd, fd, dw_hr_name(h2));
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
TEST(flowdirection_conflict_clears_when_fixed) {
    NEED_FACTORY(); IDWriteTextLayout *l = dir_layout(f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT); DWRITE_TEXT_METRICS tm;
    if (l) {
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), EH_FLOWDIRECTIONCONFLICTS);
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), EH_FLOWDIRECTIONCONFLICTS);          /* the failure is not cached as a success */
        CHECK_HR(IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM), S_OK);
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), S_OK);
        CHECK_HR(IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM), S_OK);
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), EH_FLOWDIRECTIONCONFLICTS);
        DW_RELEASE(l);
    }
    DONE_FACTORY();
}
TEST(flowdirection_conflict_draw_never_reaches_renderer) {
    NEED_FACTORY(); IDWriteTextLayout *l = dir_layout(f, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT); dwx_renderer *r = dwx_renderer_new(); r->support_r1 = 1;
    if (l) {
        CHECK_HR(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f), EH_FLOWDIRECTIONCONFLICTS);
        EXPECT_EQ(r->ncalls, 0);
        DW_RELEASE(l);
    }
    dwx_release(r); DONE_FACTORY();
}
TEST(flowdirection_createtextlayout_with_conflicting_format) {
    NEED_FACTORY(); IDWriteTextFormat *t = eh_format(f, 12.f); IDWriteTextLayout *l = SENT(IDWriteTextLayout *);
    if (t) {
        CHECK_HR(IDWriteTextFormat_SetFlowDirection(t, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT), S_OK);       /* setting is always accepted */
        HRESULT hr = IDWriteFactory_CreateTextLayout(f, L"Hello", 5, t, 100, 100, &l);
        t_note("CreateTextLayout from a format with conflicting directions = %s", dw_hr_name(hr));
        CHECKF(hr == S_OK || hr == EH_FLOWDIRECTIONCONFLICTS, "unexpected %s", dw_hr_name(hr));
        if (SUCCEEDED(hr) && l) { DWRITE_TEXT_METRICS tm; CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), EH_FLOWDIRECTIONCONFLICTS); DW_RELEASE(l); } else CHECK_OUT_NULL(l);
        DW_RELEASE(t);
    }
    DONE_FACTORY();
}
TEST(textrenderer_incompatible_matrix) {
    NEED_FACTORY(); int rd, fd;
    /* TextLayout::Draw queries the renderer for IDWriteTextRenderer1 ({d3e0e934-22a0-427e-aae4-7d9574b59db1}); vertical reading needs it. */
    for (rd = 0; rd < 4; rd++) for (fd = 0; fd < 4; fd++) {
        if (vertical_axis(rd) != vertical_axis(fd)) continue;
        IDWriteTextLayout *l = dir_layout(f, (DWRITE_READING_DIRECTION)rd, (DWRITE_FLOW_DIRECTION)fd); dwx_renderer *r = dwx_renderer_new();
        if (l) {
            HRESULT hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f);
            if (vertical_axis(rd)) { CHECKF(hr == EH_TEXTRENDERERINCOMPATIBLE, "reading %d flow %d without R1 -> %s", rd, fd, dw_hr_name(hr)); EXPECT_EQ(r->ncalls, 0); }
            else CHECKF(hr == S_OK, "reading %d flow %d without R1 -> %s", rd, fd, dw_hr_name(hr));
            dwx_renderer_clear(r); r->support_r1 = 1;
            hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f);
            CHECKF(hr == S_OK, "reading %d flow %d with R1 -> %s", rd, fd, dw_hr_name(hr));
            DW_RELEASE(l);
        }
        dwx_release(r);
    }
    DONE_FACTORY();
}
TEST(textrenderer_incompatible_does_not_depend_on_text) {
    NEED_FACTORY(); static const WCHAR *const texts[] = { L"A", L"Hello world", L"\x05d0\x05d1\x05d2", L"line1\nline2" }; unsigned i;
    for (i = 0; i < 4; i++) {
        IDWriteTextFormat *t = eh_format(f, 16.f); IDWriteTextLayout *l = NULL; dwx_renderer *r = dwx_renderer_new();
        if (t && SUCCEEDED(IDWriteFactory_CreateTextLayout(f, texts[i], lstrlenW(texts[i]), t, 200, 200, &l)) && l) {
            IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_TOP_TO_BOTTOM); IDWriteTextLayout_SetFlowDirection(l, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
            CHECKF(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f) == EH_TEXTRENDERERINCOMPATIBLE, "text %u", i);
        }
        DW_RELEASE(l); DW_RELEASE(t); dwx_release(r);
    }
    DONE_FACTORY();
}
TEST(textrenderer_r1_qi_is_queried_once_per_draw) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); dwx_renderer *r = dwx_renderer_new(); r->support_r1 = 1;
    if (l) {
        LONG before = dwx_qi_calls(r);
        CHECK_HR(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f), S_OK);
        t_note("renderer QueryInterface calls during one horizontal Draw: %ld", dwx_qi_calls(r) - before);
        CHECK(dwx_qi_calls(r) - before >= 1);
        DW_RELEASE(l);
    }
    dwx_release(r); DONE_FACTORY();
}

/* ---- renderer callback failures ------------------------------------------------------------------------------------------------------- */
static const HRESULT rpass[] = { E_FAIL, EH_CUSTOM, E_ACCESSDENIED, EH_FILEFORMAT, (HRESULT)0x87654321 };
TEST(draw_pixel_snapping_callback_failure_passes_through) {
    NEED_FACTORY(); int i;
    for (i = 0; i < 5; i++) {
        IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); dwx_renderer *r = dwx_renderer_new();
        if (l) {
            r->fail_snapping = rpass[i];       /* IsPixelSnappingDisabled -> CallbackException::ThrowIfFailed */
            HRESULT hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f);
            CHECKF(hr == rpass[i], "IsPixelSnappingDisabled injected %s got %s", dw_hr_name(rpass[i]), dw_hr_name(hr)); EXPECT_EQ(r->ncalls, 0);
            r->fail_snapping = S_OK; r->fail_transform = rpass[i];
            hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f);
            CHECKF(hr == rpass[i], "GetCurrentTransform injected %s got %s", dw_hr_name(rpass[i]), dw_hr_name(hr)); EXPECT_EQ(r->ncalls, 0);
            r->fail_transform = S_OK; r->fail_ppd = rpass[i];
            hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f);
            CHECKF(hr == rpass[i], "GetPixelsPerDip injected %s got %s", dw_hr_name(rpass[i]), dw_hr_name(hr)); EXPECT_EQ(r->ncalls, 0);
            r->fail_ppd = S_OK;
            CHECK_HR(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f), S_OK); CHECK(r->ncalls > 0);
            DW_RELEASE(l);
        }
        dwx_release(r);
    }
    DONE_FACTORY();
}
TEST(draw_snapping_disabled_skips_transform_and_dpi_callbacks) {
    NEED_FACTORY(); IDWriteTextLayout *l = eh_layout(f, L"Hello", 100, 100, NULL); dwx_renderer *r = dwx_renderer_new();
    if (l) {
        r->snapping_disabled = TRUE; r->fail_transform = EH_CUSTOM; r->fail_ppd = EH_CUSTOM;
        /* with snapping disabled the transform and pixelsPerDip callbacks are not consulted, so their failures never surface */
        CHECK_HR(IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f), S_OK);
        EXPECT_EQ(r->n_transform, 0); EXPECT_EQ(r->n_ppd, 0);
        DW_RELEASE(l);
    }
    dwx_release(r); DONE_FACTORY();
}
TEST(draw_glyphrun_callback_failure_is_reported_or_ignored_consistently) {
    NEED_FACTORY(); int i;
    for (i = 0; i < 5; i++) {
        IDWriteTextLayout *l = eh_layout(f, L"Hello world", 200, 100, NULL); dwx_renderer *r = dwx_renderer_new();
        if (l) {
            r->fail_draw = rpass[i];
            HRESULT hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0.f, 0.f);
            t_note("DrawGlyphRun injected %s -> Draw = %s (calls=%d)", dw_hr_name(rpass[i]), dw_hr_name(hr), r->ncalls);
            CHECKF(hr == S_OK || hr == rpass[i], "Draw returned %s", dw_hr_name(hr));
            CHECK(r->ncalls >= 1);
            DW_RELEASE(l);
        }
        dwx_release(r);
    }
    DONE_FACTORY();
}

/* ---- codes that cannot be provoked through a public call on a running system ------------------------------------------------------------- */
TEST(unreachable_codes_are_documented) {
    /* FONTCOLLECTIONOBSOLETE (system collection replaced while an old collection is queried), CACHEFORMAT/CACHEVERSION (corrupt or mismatched
     * font cache service data), UNEXPECTED, DOWNLOADCANCELLED/DOWNLOADFAILED/TOOMANYDOWNLOADS (remote-font downloader) need an installed-font change,
     * a damaged cache or a network font source. The catalogue lists them in CATALOG.md; nothing to provoke here. */
    static const HRESULT unreachable[] = { EH_FONTCOLLECTIONOBSOLETE, EH_CACHEFORMAT, EH_CACHEVERSION, EH_UNEXPECTED, EH_DOWNLOADCANCELLED, EH_DOWNLOADFAILED, EH_TOOMANYDOWNLOADS };
    unsigned i;
    for (i = 0; i < sizeof unreachable / sizeof unreachable[0]; i++) CHECK(FAILED(unreachable[i]));
    t_note("%u DWRITE_E_* codes are not reproducible from a test process", (unsigned)(sizeof unreachable / sizeof unreachable[0]));
}
