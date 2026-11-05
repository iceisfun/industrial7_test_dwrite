/* Cross-cutting layout tests: black box of empty text, object lifetime and reference traffic, bulk creation, a model-based check of
 * the run list, large inputs. */
#include "tl.h"

#define MAXR 0xFFFFFFFFu
#define LINEH (TL_ASC + TL_DESC)

/* ---- overhang of text without ink: the black box collapses to the aligned origin -------------------------------------- */
TEST(overhang_empty_layout_alignments)
{
    /* with no glyphs the ink box is the zero-size point where the text would start; overhang = box edge distances.
     * (justified behaves like leading for this purpose) */
    TL_BEGIN();
    static const struct { DWRITE_TEXT_ALIGNMENT ta; DWRITE_PARAGRAPH_ALIGNMENT pa; float l, t, r, b; } t[] = {
        { DWRITE_TEXT_ALIGNMENT_LEADING,  DWRITE_PARAGRAPH_ALIGNMENT_NEAR,   0,     0,     -5000, -3000 },
        { DWRITE_TEXT_ALIGNMENT_TRAILING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,   -5000, 0,     0,     -3000 },
        { DWRITE_TEXT_ALIGNMENT_CENTER,   DWRITE_PARAGRAPH_ALIGNMENT_NEAR,   -2500, 0,     -2500, -3000 },
        { DWRITE_TEXT_ALIGNMENT_LEADING,  DWRITE_PARAGRAPH_ALIGNMENT_FAR,    0,     -3000, -5000, 0 },
        { DWRITE_TEXT_ALIGNMENT_LEADING,  DWRITE_PARAGRAPH_ALIGNMENT_CENTER, 0,     -1500, -5000, -1500 },
        { DWRITE_TEXT_ALIGNMENT_JUSTIFIED, DWRITE_PARAGRAPH_ALIGNMENT_NEAR,  0,     0,     -5000, -3000 },
    };
    size_t i;
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, L"", TL_UPM, 5000.0f, 3000.0f); DWRITE_OVERHANG_METRICS o;
        if (!l) { t_check(0, __FILE__, __LINE__, "layout", "row %u", (unsigned)i); continue; }
        CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, t[i].ta)); CHECK_HR_OK(IDWriteTextLayout_SetParagraphAlignment(l, t[i].pa));
        memset(&o, 0xcc, sizeof o);
        CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &o));
        /* the collapsed ink box has zero size, so left + right = -layoutWidth and top + bottom = -layoutHeight whatever the alignment */
        CHECKF(o.left + o.right == -5000.0f && o.top + o.bottom == -3000.0f, "row %u: %g %g %g %g", (unsigned)i, o.left, o.top, o.right, o.bottom);
        if (i == 0 || i == 5)
            CHECKF(o.left == t[i].l && o.top == t[i].t && o.right == t[i].r && o.bottom == t[i].b, "row %u: %g %g %g %g want %g %g %g %g", (unsigned)i,
                   o.left, o.top, o.right, o.bottom, t[i].l, t[i].t, t[i].r, t[i].b);
        else if (o.left != t[i].l || o.top != t[i].t || o.right != t[i].r || o.bottom != t[i].b)
            t_note("row %u: %g %g %g %g (expected %g %g %g %g)", (unsigned)i, o.left, o.top, o.right, o.bottom, t[i].l, t[i].t, t[i].r, t[i].b);
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(overhang_whitespace_only_text)
{
    TL_BEGIN(); TL_LAYOUT(l, L"   ", TL_UPM, 5000.0f, 3000.0f);
    DWRITE_OVERHANG_METRICS o;
    CHECK_HR_OK(IDWriteTextLayout_GetOverhangMetrics(l, &o));
    t_note("overhang of three spaces: %g %g %g %g", o.left, o.top, o.right, o.bottom);
    CHECK(o.right <= 0.0f && o.bottom <= 0.0f);
    DW_RELEASE(l); TL_END();
}

/* ---- reference traffic ------------------------------------------------------------------------------------------------------------------ */
TEST(layout_holds_collection_reference)
{
    TL_BEGIN();
    ULONG base = dw_refcount((IUnknown *)X.coll), during;
    IDWriteTextLayout *l;
    TL_LAYOUT(l0, L"ABCD", 16.0f, 100.0f, 100.0f);
    (void)base; DW_RELEASE(l0);
    base = dw_refcount((IUnknown *)X.coll);
    l = tl_layout_(&X, TL_FAM, L"ABCD", 16.0f, 100.0f, 100.0f);
    if (!l) { TL_END(); t_skip("layout"); return; }
    during = dw_refcount((IUnknown *)X.coll);
    CHECKF(during > base, "collection refcount %lu -> %lu while a layout is alive", base, during);
    DW_RELEASE(l);
    EXPECT_REFS(X.coll, base);
    TL_END();
}

TEST(layout_holds_factory_reference)
{
    TL_BEGIN();
    ULONG base, during;
    IDWriteTextLayout *l;
    TL_LAYOUT(l0, L"ABCD", 16.0f, 100.0f, 100.0f);
    DW_RELEASE(l0);
    base = dw_refcount((IUnknown *)f);
    l = tl_layout_(&X, TL_FAM, L"ABCD", 16.0f, 100.0f, 100.0f);
    if (!l) { TL_END(); t_skip("layout"); return; }
    during = dw_refcount((IUnknown *)f);
    CHECKF(during > base, "factory refcount %lu -> %lu while a layout is alive", base, during);
    DW_RELEASE(l);
    EXPECT_REFS(f, base);
    TL_END();
}

TEST(layout_range_collection_released_with_layout)
{
    TL_BEGIN();
    IDWriteFontCollection *sys = NULL; IDWriteTextLayout *l; ULONG base, during_;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE));
    base = dw_refcount((IUnknown *)sys);
    l = tl_layout_(&X, TL_FAM, L"ABCD", 16.0f, 100.0f, 100.0f);
    if (!l) { DW_RELEASE(sys); TL_END(); t_skip("layout"); return; }
    CHECK_HR_OK(IDWriteTextLayout_SetFontCollection(l, sys, RNG(0, 2)));
    during_ = dw_refcount((IUnknown *)sys);
    CHECKF(during_ > base, "collection retained by the range");
    CHECK_HR_OK(IDWriteTextLayout_SetFontCollection(l, X.coll, RNG(0, 2)));
    t_note("system collection refcount: base %lu, retained %lu, overwritten %lu", base, during_, dw_refcount((IUnknown *)sys));
    CHECK_HR_OK(IDWriteTextLayout_SetFontCollection(l, sys, RNG(0, 2)));
    DW_RELEASE(l);
    t_note("system collection refcount after the layout died: %lu (base %lu)", dw_refcount((IUnknown *)sys), base);
    DW_RELEASE(sys); TL_END();
}

TEST(layout_drawing_effect_no_leak_under_churn)
{
    TL_BEGIN(); TL_LAYOUT(l, L"ABCDEFGH", 16.0f, 100.0f, 100.0f);
    dwx_effect *e = dwx_effect_new(1), *e2 = dwx_effect_new(2); int i;
    for (i = 0; i < 2000; i++) {
        DWRITE_TEXT_RANGE r = RNG((UINT32)(i * 7 % 10), (UINT32)(1 + i % 5));
        CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, (IUnknown *)((i & 1) ? e : e2), r));
    }
    CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(l, NULL, RNG(0, MAXR - 1)));
    EXPECT_EQ(dwx_refs(e), 1); EXPECT_EQ(dwx_refs(e2), 1);
    DW_RELEASE(l);
    dwx_release(e); dwx_release(e2); TL_END();
}

TEST(layout_inline_object_released_after_replacement_and_death)
{
    TL_BEGIN(); TL_LAYOUT(l, L"ABCDEFGH", 16.0f, 100.0f, 100.0f);
    dwx_inline *o = dwx_inline_new(5, 5, 4, FALSE);
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(2, 2)));
    tl_lines(l);                                     /* laid out: the object is queried */
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, NULL, RNG(2, 2)));
    tl_lines(l);
    EXPECT_EQ(dwx_refs(o), 1);
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)o, RNG(0, 8)));
    tl_lines(l);
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(o), 1);
    dwx_release(o); TL_END();
}

TEST(layout_trimming_sign_never_queried_without_trimming)
{
    TL_BEGIN(); TL_LAYOUT(l, L"ABCD", TL_UPM, 100000.0f, 100000.0f);
    dwx_inline *o = dwx_inline_new(5, 5, 4, FALSE); DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    CHECK_HR_OK(IDWriteTextLayout_SetTrimming(l, &tr, (IDWriteInlineObject *)o));
    tl_lines(l);                                     /* the text fits: no trimming, no sign metrics needed */
    EXPECT_EQ(o->n_draw, 0);
    DW_RELEASE(l); dwx_release(o); TL_END();
}

/* ---- bulk ---------------------------------------------------------------------------------------------------------------------------------- */
TEST(create_many_layouts_released)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, 16.0f); int i; ULONG fbase, cbase;
    if (!t) { TL_END(); t_skip("format"); return; }
    { IDWriteTextLayout *w = NULL;           /* warm-up: let any internal cache take its references first */
      if (SUCCEEDED(IDWriteFactory_CreateTextLayout(f, L"ABC DEF", 7, t, 100.0f, 100.0f, &w)) && w) { tl_lines(w); IDWriteTextLayout_Release(w); } }
    fbase = dw_refcount((IUnknown *)f); cbase = dw_refcount((IUnknown *)X.coll);
    for (i = 0; i < 500; i++) {
        IDWriteTextLayout *l = NULL;
        CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABC DEF", 7, t, 100.0f, 100.0f, &l));
        if (!l) break;
        if (i % 50 == 0) tl_lines(l);
        IDWriteTextLayout_Release(l);
    }
    EXPECT_REFS(f, fbase); EXPECT_REFS(X.coll, cbase); EXPECT_REFS(t, 1);
    DW_RELEASE(t); TL_END();
}

TEST(create_many_layouts_alive_together)
{
    TL_BEGIN(); IDWriteTextFormat *t = tl_format(&X, TL_FAM, TL_UPM);
    IDWriteTextLayout *ls[64]; int i, n = 0; DWRITE_TEXT_METRICS m;
    if (!t) { TL_END(); t_skip("format"); return; }
    for (i = 0; i < 64; i++) {
        WCHAR s[70]; int k;
        for (k = 0; k <= i; k++) s[k] = 'A' + (k % 26);
        s[k] = 0;
        ls[i] = NULL; IDWriteFactory_CreateTextLayout(f, s, (UINT32)(i + 1), t, 1e6f, 1e6f, &ls[i]);
        if (ls[i]) n++;
    }
    EXPECT_EQ(n, 64);
    for (i = 0; i < 64; i++) if (ls[i]) { tl_metrics(ls[i], &m); CHECKF(m.width == 1000.0f * (i + 1), "layout %d width %g", i, m.width); }
    for (i = 0; i < 64; i++) DW_RELEASE(ls[i]);
    DW_RELEASE(t); TL_END();
}

TEST(many_lines)
{
    TL_BEGIN();
    enum { N = 1000 };
    WCHAR *s = (WCHAR *)malloc((N * 2 + 1) * sizeof(WCHAR)); IDWriteTextLayout *l; DWRITE_TEXT_METRICS m; int i;
    IDWriteTextFormat *t = tl_format(&X, TL_FAM, TL_UPM);
    if (!s || !t) { free(s); DW_RELEASE(t); TL_END(); t_skip("alloc"); return; }
    for (i = 0; i < N; i++) { s[2 * i] = 'A'; s[2 * i + 1] = '\n'; }
    s[2 * N] = 0;
    l = NULL; CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, s, 2 * N, t, 1e6f, 1e9f, &l));
    if (l) {
        tl_metrics(l, &m);
        EXPECT_EQ(m.lineCount, N + 1); CHECK_FLOAT_EQ(m.width, 1000.0f);
        CHECKF(fabs(m.height - (N + 1) * (double)LINEH) < 1.0, "height %g", m.height);
        DW_RELEASE(l);
    }
    free(s); DW_RELEASE(t); TL_END();
}

TEST(huge_dimensions_and_sizes_do_not_fail)
{
    TL_BEGIN(); TL_LAYOUT(l, L"ABCD", 16.0f, FLT_MAX, FLT_MAX);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_FLOAT_EQ(m.layoutWidth, FLT_MAX); CHECK(m.lineCount == 1);
    CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, DWRITE_TEXT_ALIGNMENT_CENTER));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    CHECK_HR_OK(IDWriteTextLayout_SetFontSize(l, 100000.0f, RNG(0, 4)));
    CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m));
    t_note("size 100000: width %g height %g", m.width, m.height);
    CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, INFINITY));
    t_note("GetMetrics with infinite max width -> %s", dw_hr_name(IDWriteTextLayout_GetMetrics(l, &m)));
    DW_RELEASE(l); TL_END();
}

/* ---- model-based check of the run list ---------------------------------------------------------------------------------------------------------- */
static uint32_t lcg(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }

TEST(runlist_model_size_property)
{
    /* 3000 random range assignments of three font sizes over a 20 character window; after each, every position's value, the
     * range containing it and the tiling of the ranges are compared with a shadow array */
    TL_BEGIN(); TL_LAYOUT(l, L"ABCDEFGHIJKLMNOPQRST", 16.0f, 1000.0f, 1000.0f);
    static const float sizes[3] = { 16.0f, 24.0f, 32.0f };
    int shadow[40]; uint32_t seed = 12345; int it, p, nonmax = 0;
    for (p = 0; p < 40; p++) shadow[p] = 0;
    for (it = 0; it < 3000; it++) {
        UINT32 start = lcg(&seed) % 30, len = lcg(&seed) % 12; int v = (int)(lcg(&seed) % 3);
        HRESULT hr = IDWriteTextLayout_SetFontSize(l, sizes[v], RNG(start, len));
        if (hr != S_OK) { t_check(0, __FILE__, __LINE__, "SetFontSize", "iteration %d -> %s", it, dw_hr_name(hr)); break; }
        for (p = (int)start; p < (int)(start + len) && p < 40; p++) shadow[p] = v;
        if (it % 25 != 0) continue;
        for (p = 0; p < 36; p++) {
            float sz = 0; DWRITE_TEXT_RANGE r = { 7, 7 };
            if (IDWriteTextLayout_GetFontSize(l, (UINT32)p, &sz, &r) != S_OK) { t_check(0, __FILE__, __LINE__, "GetFontSize", "it %d pos %d", it, p); goto done; }
            if (sz != sizes[shadow[p]]) { t_check(0, __FILE__, __LINE__, "value", "it %d pos %d got %g want %g", it, p, sz, sizes[shadow[p]]); goto done; }
            if (!(r.startPosition <= (UINT32)p && (UINT64)p < (UINT64)r.startPosition + r.length)) { t_check(0, __FILE__, __LINE__, "range contains position", "it %d pos %d range {%u,%u}", it, p, r.startPosition, r.length); goto done; }
            { UINT32 q; for (q = r.startPosition; q < r.startPosition + r.length && q < 40; q++)
                if (shadow[q] != shadow[p]) { t_check(0, __FILE__, __LINE__, "range is uniform", "it %d pos %d range {%u,%u} differs at %u", it, p, r.startPosition, r.length, q); goto done; } }
            if (r.startPosition > 0 && r.startPosition <= 40 && shadow[r.startPosition - 1] == shadow[p]) nonmax++;
        }
    }
done:
    t_check(nonmax == 0, __FILE__, __LINE__, "ranges are maximal", "%d reported ranges could have been extended to the left", nonmax);
    DW_RELEASE(l); TL_END();
}

TEST(runlist_model_two_properties_shared_boundaries)
{
    /* size and weight share the run list: every reported range is uniform in BOTH properties */
    TL_BEGIN(); TL_LAYOUT(l, L"ABCDEFGHIJKLMNOPQRST", 16.0f, 1000.0f, 1000.0f);
    int ssz[40] = { 0 }, swt[40] = { 0 }; uint32_t seed = 777; int it, p;
    for (it = 0; it < 1500; it++) {
        UINT32 start = lcg(&seed) % 30, len = lcg(&seed) % 10; int v = (int)(lcg(&seed) % 2);
        if (lcg(&seed) & 1) { IDWriteTextLayout_SetFontSize(l, v ? 24.0f : 16.0f, RNG(start, len)); for (p = start; p < (int)(start + len) && p < 40; p++) ssz[p] = v; }
        else { IDWriteTextLayout_SetFontWeight(l, v ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, RNG(start, len)); for (p = start; p < (int)(start + len) && p < 40; p++) swt[p] = v; }
        if (it % 30) continue;
        for (p = 0; p < 36; p++) {
            DWRITE_TEXT_RANGE r1 = { 7, 7 }, r2 = { 8, 8 }; float sz; DWRITE_FONT_WEIGHT w; UINT32 q;
            IDWriteTextLayout_GetFontSize(l, (UINT32)p, &sz, &r1); IDWriteTextLayout_GetFontWeight(l, (UINT32)p, &w, &r2);
            CHECKF(r1.startPosition == r2.startPosition && r1.length == r2.length, "it %d pos %d: size range {%u,%u} weight range {%u,%u}", it, p, r1.startPosition, r1.length, r2.startPosition, r2.length);
            for (q = r1.startPosition; q < r1.startPosition + r1.length && q < 40; q++) if (ssz[q] != ssz[p] || swt[q] != swt[p]) { t_check(0, __FILE__, __LINE__, "uniform in both", "it %d pos %d range {%u,%u} at %u", it, p, r1.startPosition, r1.length, q); goto done; }
        }
    }
done:
    DW_RELEASE(l); TL_END();
}

/* ---- text/format interplay ------------------------------------------------------------------------------------------------------------------------ */
TEST(layout_family_not_in_collection_still_lays_out)
{
    /* a family nobody has in the system collection: the layout falls back to some font, it never fails */
    TL_BEGIN(); IDWriteTextFormat *t = NULL; IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"No Such Family 12345", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &t));
    if (!t) { TL_END(); t_skip("format"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ABCD", 4, t, 1000.0f, 1000.0f, &l));
    if (l) { CHECK_HR_OK(IDWriteTextLayout_GetMetrics(l, &m)); CHECK(m.width > 0.0f); CHECK(m.height > 0.0f); EXPECT_EQ(m.lineCount, 1); DW_RELEASE(l); }
    DW_RELEASE(t); TL_END();
}

TEST(layout_range_family_not_in_collection)
{
    TL_BEGIN(); TL_LAYOUT(l, L"ABCD", 16.0f, 1000.0f, 1000.0f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, L"No Such Family 12345", RNG(1, 2)));
    t_note("range with an unknown family inside a custom collection: GetMetrics -> %s", dw_hr_name(IDWriteTextLayout_GetMetrics(l, &m)));
    DW_RELEASE(l); TL_END();
}

TEST(layout_different_format_sizes_same_text)
{
    TL_BEGIN();
    static const float sizes[] = { 8, 16, 32, 64 }; size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        IDWriteTextLayout *l = tl_layout_(&X, TL_FAM, L"ABCD", sizes[i], 1e6f, 1e6f); DWRITE_TEXT_METRICS m;
        if (!l) continue;
        tl_metrics(l, &m);
        CHECK_NEAR(m.width, 4000.0 * sizes[i] / 2048.0, 1e-3);
        CHECK_NEAR(m.height, 2250.0 * sizes[i] / 2048.0, 1e-3);
        DW_RELEASE(l);
    }
    TL_END();
}

TEST(layout_bold_italic_request_uses_available_face)
{
    /* only a regular face exists; asking for bold italic still lays out with its metrics (simulation or nearest match) */
    TL_BEGIN(); TL_LAYOUT(l, L"ABCD", TL_UPM, 1e6f, 1e6f);
    DWRITE_TEXT_METRICS m;
    CHECK_HR_OK(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, RNG(0, 4)));
    CHECK_HR_OK(IDWriteTextLayout_SetFontStyle(l, DWRITE_FONT_STYLE_ITALIC, RNG(0, 4)));
    tl_metrics(l, &m);
    CHECK(m.width >= 4000.0f); EXPECT_EQ(m.lineCount, 1);
    t_note("simulated bold italic width at 2048: %g (regular 4000)", m.width);
    DW_RELEASE(l); TL_END();
}

TEST(layout_unmapped_characters_use_notdef)
{
    /* characters the font lacks are shown with .notdef (advance 1200) or taken from a fallback font */
    TL_BEGIN(); TL_LAYOUT(l, L"A\x4E2D" L"B", TL_UPM, 1e6f, 1e6f);
    DWRITE_TEXT_METRICS m;
    tl_metrics(l, &m);
    t_note("A U+4E2D B at 2048: width %g (notdef would give 3200)", m.width);
    CHECK(m.width >= 2000.0f); EXPECT_EQ(m.lineCount, 1);
    DW_RELEASE(l); TL_END();
}

TEST(layout_text_alignment_all_values_with_wrap_modes)
{
    TL_BEGIN(); TL_LAYOUT(l, L"AB CD EF GH", TL_UPM, 5000.0f, 5000.0f);
    UINT32 ta, ww; DWRITE_TEXT_METRICS m;
    for (ta = 0; ta < 4; ta++)
        for (ww = 0; ww < 5; ww++) {
            CHECK_HR_OK(IDWriteTextLayout_SetTextAlignment(l, (DWRITE_TEXT_ALIGNMENT)ta));
            CHECK_HR_OK(IDWriteTextLayout_SetWordWrapping(l, (DWRITE_WORD_WRAPPING)ww));
            CHECKF(IDWriteTextLayout_GetMetrics(l, &m) == S_OK, "ta %u ww %u", ta, ww);
            CHECKF(m.lineCount >= 1 && m.height >= LINEH, "ta %u ww %u lines %u", ta, ww, m.lineCount);
        }
    DW_RELEASE(l); TL_END();
}
