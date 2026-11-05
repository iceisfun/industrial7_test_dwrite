/* Object model of the text format: QueryInterface matrix, reference counting, font fallback ownership (IDWriteTextFormat1),
 * font axis values (IDWriteTextFormat3) and the way a text layout copies the format it is created from.
 *
 * IDWriteTextFormat, IDWriteTextFormat1 and IDWriteTextFormat2 are answered with the object's own interface pointer; IDWriteTextFormat3
 * and an internal debug interface complete the list. Every other IID, including IDWriteTextLayout, fails with E_NOINTERFACE. */
#include "tf.h"

#define EQ_CALL(call, want) do { ULONG r_ = (call); EXPECT_EQ(r_, want); } while (0)

/* ---- QueryInterface -------------------------------------------------------------------------------------------------- */
TEST(qi_supported_interfaces)
{
    TF_NEW(t);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteTextFormat, 1), DW_QI_ROW(IDWriteTextFormat1, 1), DW_QI_ROW(IDWriteTextFormat2, 1), DW_QI_ROW(IDWriteTextFormat3, 1) };
    DW_CHECK_QI(t, rows);
    DW_RELEASE(t);
}

TEST(qi_unsupported_interfaces)
{
    TF_NEW(t);
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteTextLayout, 0), DW_QI_ROW(IDWriteTextLayout1, 0), DW_QI_ROW(IDWriteTextLayout2, 0), DW_QI_ROW(IDWriteTextLayout3, 0),
        DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontFamily, 0),
        DW_QI_ROW(IDWriteInlineObject, 0), DW_QI_ROW(IDWriteTextRenderer, 0), DW_QI_ROW(IDWriteFontFallback, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0),
        DW_QI_ROW(IDWriteTypography, 0), DW_QI_ROW(IDWriteNumberSubstitution, 0), DW_QI_ROW(IDWriteGdiInterop, 0), DW_QI_ROW(IDWriteTextAnalyzer, 0) };
    DW_CHECK_QI(t, rows);
    DW_RELEASE(t);
}

TEST(qi_every_public_iid_except_the_four)
{
    TF_NEW(t);
    int n = 0, i, ok = 0; const dw_qi_row *rows = dw_iid_table(&n);
    CHECK(n > 50);
    for (i = 0; i < n; i++) {
        const IID *id = rows[i].iid; void *p = (void *)(ULONG_PTR)1; HRESULT hr;
        int is_fmt = IsEqualIID(id, &IID_IDWriteTextFormat) || IsEqualIID(id, &IID_IDWriteTextFormat1) || IsEqualIID(id, &IID_IDWriteTextFormat2) ||
                     IsEqualIID(id, &IID_IDWriteTextFormat3) || IsEqualIID(id, &IID_IUnknown);
        hr = IUnknown_QueryInterface((IUnknown *)t, id, &p);
        if (is_fmt) { CHECKF(hr == S_OK, "%s -> %s", rows[i].name, dw_hr_name(hr)); if (hr == S_OK) { ok++; IUnknown_Release((IUnknown *)p); } continue; }
        CHECKF(hr == E_NOINTERFACE, "%s -> %s", rows[i].name, dw_hr_name(hr));
        CHECKF(p == NULL, "%s: out pointer not zeroed", rows[i].name);
        if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    }
    CHECK(ok >= 4);
    EXPECT_REFS(t, 1);
    DW_RELEASE(t);
}

TEST(qi_random_and_null_iids)
{
    TF_NEW(t);
    static const GUID nul = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
    static const GUID rnd = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    void *p = (void *)(ULONG_PTR)1;
    CHECK_HR(IUnknown_QueryInterface((IUnknown *)t, &rnd, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
    p = (void *)(ULONG_PTR)1;
    CHECK_HR(IUnknown_QueryInterface((IUnknown *)t, &nul, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
    EXPECT_REFS(t, 1);
    DW_RELEASE(t);
}

TEST(qi_pointer_identity)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    IDWriteTextFormat *b = (IDWriteTextFormat *)tf_qi((IUnknown *)t, &IID_IDWriteTextFormat, "IDWriteTextFormat");
    IUnknown *u = (IUnknown *)tf_qi((IUnknown *)t, &IID_IUnknown, "IUnknown");
    /* IDWriteTextFormat, 1 and 2 are all handed back as the primary interface pointer. */
    CHECK((void *)b == (void *)t); CHECK((void *)t1 == (void *)t); CHECK((void *)t2 == (void *)t);
    t_note("IDWriteTextFormat3 pointer %s the creation pointer; IUnknown pointer %s it", (void *)t3 == (void *)t ? "equals" : "differs from", (void *)u == (void *)t ? "equals" : "differs from");
    CHECK(dw_same_identity((IUnknown *)t1, (IUnknown *)t3));
    CHECK(dw_same_identity((IUnknown *)t2, (IUnknown *)u));
    CHECK(dw_same_identity((IUnknown *)b, (IUnknown *)t3));
    DW_RELEASE(u); DW_RELEASE(b); DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(qi_from_every_pointer_reaches_every_interface)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    IUnknown *src[4]; int i, j; src[0] = (IUnknown *)t; src[1] = (IUnknown *)t1; src[2] = (IUnknown *)t2; src[3] = (IUnknown *)t3;
    const IID *ids[5] = { &IID_IUnknown, &IID_IDWriteTextFormat, &IID_IDWriteTextFormat1, &IID_IDWriteTextFormat2, &IID_IDWriteTextFormat3 };
    for (i = 0; i < 4; i++) for (j = 0; j < 5; j++) {
        void *p = NULL; HRESULT hr = IUnknown_QueryInterface(src[i], ids[j], &p);
        CHECKF(hr == S_OK && p, "source %d iid %d -> %s", i, j, dw_hr_name(hr));
        if (p) { CHECK(dw_same_identity((IUnknown *)p, (IUnknown *)t)); IUnknown_Release((IUnknown *)p); }
    }
    EXPECT_REFS(t, 4);                   /* t, t1, t2, t3 */
    DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3);
    EXPECT_REFS(t, 1);
    DW_RELEASE(t);
}

TEST(qi_failure_does_not_touch_refcount)
{
    TF_NEW(t);
    int i;
    for (i = 0; i < 50; i++) { void *p = NULL; IUnknown_QueryInterface((IUnknown *)t, &IID_IDWriteFactory, &p); }
    EXPECT_REFS(t, 1);
    DW_RELEASE(t);
}

/* ---- reference counting ---------------------------------------------------------------------------------------------------- */
TEST(refcount_addref_release_return_values)
{
    TF_NEW(t);
    EQ_CALL(IDWriteTextFormat_AddRef(t), 2);
    EQ_CALL(IDWriteTextFormat_AddRef(t), 3);
    EQ_CALL(IDWriteTextFormat_Release(t), 2);
    EQ_CALL(IDWriteTextFormat_Release(t), 1);
    EXPECT_REFS(t, 1);
    DW_RELEASE(t);
}

TEST(refcount_shared_by_all_interface_pointers)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    EXPECT_REFS(t, 4); EXPECT_REFS(t1, 4); EXPECT_REFS(t2, 4); EXPECT_REFS(t3, 4);
    EQ_CALL(IDWriteTextFormat3_AddRef(t3), 5); EQ_CALL(IDWriteTextFormat1_AddRef(t1), 6);
    EQ_CALL(IDWriteTextFormat_Release(t), 5); EQ_CALL(IDWriteTextFormat2_Release(t2), 4);
    EQ_CALL(IDWriteTextFormat1_Release(t1), 3); EQ_CALL(IDWriteTextFormat3_Release(t3), 2);
    IDWriteTextFormat1_Release(t1); IDWriteTextFormat3_Release(t3);        /* the two extra AddRefs; the object is gone now */
}

TEST(refcount_object_lives_until_last_reference_any_order)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    dwx_inline *sg = dwx_inline_new(1, 1, 1, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, (IDWriteInlineObject *)sg));
    EXPECT_EQ(dwx_refs(sg), 2);
    IDWriteTextFormat_Release(t);                 /* first reference gone: the object (and its sign reference) must remain */
    EXPECT_EQ(dwx_refs(sg), 2);
    CHECK_HR_OK(IDWriteTextFormat3_SetTrimming(t3, &tr, NULL));
    EXPECT_EQ(dwx_refs(sg), 1);
    CHECK_HR_OK(IDWriteTextFormat3_SetTrimming(t3, &tr, (IDWriteInlineObject *)sg));
    IDWriteTextFormat3_Release(t3);               /* last reference: the sign is released with the object */
    EXPECT_EQ(dwx_refs(sg), 1);
    dwx_release(sg);
}

TEST(refcount_many_addref_release_pairs)
{
    TF_NEW(t);
    int i;
    for (i = 0; i < 1000; i++) IDWriteTextFormat_AddRef(t);
    EXPECT_REFS(t, 1001);
    for (i = 0; i < 1000; i++) IDWriteTextFormat_Release(t);
    EXPECT_REFS(t, 1);
    DW_RELEASE(t);
}

TEST(getters_do_not_change_refcount)
{
    TF_NEW(t);
    WCHAR b[32]; DWRITE_TRIMMING tr; IDWriteInlineObject *sg = NULL; DWRITE_LINE_SPACING_METHOD m; float a, c;
    IDWriteTextFormat_GetTextAlignment(t); IDWriteTextFormat_GetFontSize(t); IDWriteTextFormat_GetFontFamilyName(t, b, 32); IDWriteTextFormat_GetLocaleName(t, b, 32);
    IDWriteTextFormat_GetTrimming(t, &tr, &sg); IDWriteTextFormat_GetLineSpacing(t, &m, &a, &c);
    EXPECT_REFS(t, 1);
    DW_RELEASE(t);
}

/* ---- every getter through every interface pointer -------------------------------------------------------------------------------- */
TEST(getters_agree_across_interface_pointers)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_make(f, L"Consolas", DWRITE_FONT_WEIGHT_SEMI_LIGHT, DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STRETCH_SEMI_EXPANDED, 17.5f, L"nb-NO");
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    WCHAR b1[32], b2[32], b3[32]; IDWriteFontCollection *c1 = NULL, *c2 = NULL, *c3 = NULL;
    IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_JUSTIFIED); IDWriteTextFormat_SetParagraphAlignment(t, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_EMERGENCY_BREAK); IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextFormat_SetFlowDirection(t, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT); IDWriteTextFormat_SetIncrementalTabStop(t, 31.0f);
    EXPECT_EQ(IDWriteTextFormat1_GetTextAlignment(t1), DWRITE_TEXT_ALIGNMENT_JUSTIFIED); EXPECT_EQ(IDWriteTextFormat2_GetTextAlignment(t2), DWRITE_TEXT_ALIGNMENT_JUSTIFIED); EXPECT_EQ(IDWriteTextFormat3_GetTextAlignment(t3), DWRITE_TEXT_ALIGNMENT_JUSTIFIED);
    EXPECT_EQ(IDWriteTextFormat1_GetParagraphAlignment(t1), DWRITE_PARAGRAPH_ALIGNMENT_FAR); EXPECT_EQ(IDWriteTextFormat3_GetParagraphAlignment(t3), DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    EXPECT_EQ(IDWriteTextFormat2_GetWordWrapping(t2), DWRITE_WORD_WRAPPING_EMERGENCY_BREAK); EXPECT_EQ(IDWriteTextFormat3_GetWordWrapping(t3), DWRITE_WORD_WRAPPING_EMERGENCY_BREAK);
    EXPECT_EQ(IDWriteTextFormat1_GetReadingDirection(t1), DWRITE_READING_DIRECTION_RIGHT_TO_LEFT); EXPECT_EQ(IDWriteTextFormat3_GetReadingDirection(t3), DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    EXPECT_EQ(IDWriteTextFormat2_GetFlowDirection(t2), DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT); EXPECT_EQ(IDWriteTextFormat3_GetFlowDirection(t3), DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT);
    CHECK_FBITS(IDWriteTextFormat1_GetIncrementalTabStop(t1), 31.0f); CHECK_FBITS(IDWriteTextFormat2_GetIncrementalTabStop(t2), 31.0f); CHECK_FBITS(IDWriteTextFormat3_GetIncrementalTabStop(t3), 31.0f);
    EXPECT_EQ(IDWriteTextFormat1_GetFontWeight(t1), DWRITE_FONT_WEIGHT_SEMI_LIGHT); EXPECT_EQ(IDWriteTextFormat2_GetFontWeight(t2), DWRITE_FONT_WEIGHT_SEMI_LIGHT); EXPECT_EQ(IDWriteTextFormat3_GetFontWeight(t3), DWRITE_FONT_WEIGHT_SEMI_LIGHT);
    EXPECT_EQ(IDWriteTextFormat1_GetFontStyle(t1), DWRITE_FONT_STYLE_OBLIQUE); EXPECT_EQ(IDWriteTextFormat3_GetFontStyle(t3), DWRITE_FONT_STYLE_OBLIQUE);
    EXPECT_EQ(IDWriteTextFormat2_GetFontStretch(t2), DWRITE_FONT_STRETCH_SEMI_EXPANDED); EXPECT_EQ(IDWriteTextFormat3_GetFontStretch(t3), DWRITE_FONT_STRETCH_SEMI_EXPANDED);
    CHECK_FBITS(IDWriteTextFormat1_GetFontSize(t1), 17.5f); CHECK_FBITS(IDWriteTextFormat2_GetFontSize(t2), 17.5f); CHECK_FBITS(IDWriteTextFormat3_GetFontSize(t3), 17.5f);
    EXPECT_EQ(IDWriteTextFormat1_GetFontFamilyNameLength(t1), 8); EXPECT_EQ(IDWriteTextFormat3_GetFontFamilyNameLength(t3), 8);
    EXPECT_EQ(IDWriteTextFormat2_GetLocaleNameLength(t2), 5); EXPECT_EQ(IDWriteTextFormat3_GetLocaleNameLength(t3), 5);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFamilyName(t1, b1, 32)); CHECK_HR_OK(IDWriteTextFormat2_GetFontFamilyName(t2, b2, 32)); CHECK_HR_OK(IDWriteTextFormat3_GetFontFamilyName(t3, b3, 32));
    CHECK_WEQ(b1, L"Consolas"); CHECK_WEQ(b2, L"Consolas"); CHECK_WEQ(b3, L"Consolas");
    CHECK_HR_OK(IDWriteTextFormat1_GetLocaleName(t1, b1, 32)); CHECK_HR_OK(IDWriteTextFormat2_GetLocaleName(t2, b2, 32)); CHECK_HR_OK(IDWriteTextFormat3_GetLocaleName(t3, b3, 32));
    CHECK_WEQ(b1, L"nb-no"); CHECK_WEQ(b2, L"nb-no"); CHECK_WEQ(b3, L"nb-no");
    CHECK_HR_OK(IDWriteTextFormat1_GetFontCollection(t1, &c1)); CHECK_HR_OK(IDWriteTextFormat2_GetFontCollection(t2, &c2)); CHECK_HR_OK(IDWriteTextFormat3_GetFontCollection(t3, &c3));
    CHECK(c1 != NULL && c1 == c2 && c2 == c3);
    DW_RELEASE(c1); DW_RELEASE(c2); DW_RELEASE(c3);
    DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3); DW_RELEASE(t);
}

/* ---- font fallback (IDWriteTextFormat1) ------------------------------------------------------------------------------------------ */
TEST(fallback_default_is_null_with_s_ok)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    IDWriteFontFallback *fb = (IDWriteFontFallback *)(ULONG_PTR)7;
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(t1, &fb));
    CHECK(fb == NULL);
    DW_RELEASE(t1); DW_RELEASE(t);
}

TEST(fallback_set_get_reference_counting)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    tf_fb o; IDWriteFontFallback *me = (IDWriteFontFallback *)&o, *g1 = NULL, *g2 = NULL;
    tf_fb_init(&o);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, me));
    EXPECT_EQ(o.ref, 2); EXPECT_EQ(o.addref, 1);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(t1, &g1));
    CHECK(g1 == me); EXPECT_EQ(o.ref, 3);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(t1, &g2));
    CHECK(g2 == me); EXPECT_EQ(o.ref, 4);
    IDWriteFontFallback_Release(g1); IDWriteFontFallback_Release(g2);
    EXPECT_EQ(o.ref, 2);
    EXPECT_EQ(o.qi, 0); EXPECT_EQ(o.map, 0);             /* only reference counting happens */
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, NULL));
    EXPECT_EQ(o.ref, 1);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(t1, &g1)); CHECK(g1 == NULL);
    DW_RELEASE(t1); DW_RELEASE(t);
}

TEST(fallback_same_object_twice)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    tf_fb o; IDWriteFontFallback *me = (IDWriteFontFallback *)&o;
    tf_fb_init(&o);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, me));
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, me));
    EXPECT_EQ(o.ref, 2);                                 /* AddRef first, then Release of the previous value */
    DW_RELEASE(t1); DW_RELEASE(t);
    EXPECT_EQ(o.ref, 1);
}

TEST(fallback_replacement_and_destruction)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    tf_fb a, b; tf_fb_init(&a); tf_fb_init(&b);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, (IDWriteFontFallback *)&a)); EXPECT_EQ(a.ref, 2);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, (IDWriteFontFallback *)&b)); EXPECT_EQ(a.ref, 1); EXPECT_EQ(b.ref, 2);
    DW_RELEASE(t1); EXPECT_EQ(b.ref, 2);                 /* the primary pointer still owns the object */
    DW_RELEASE(t);
    EXPECT_EQ(b.ref, 1); EXPECT_EQ(a.map + b.map, 0); EXPECT_EQ(a.qi + b.qi, 0);
}

TEST(fallback_shared_by_two_formats)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *x = tf_new(f), *y = tf_new(f);
    TF_QI(x1, IDWriteTextFormat1, x); TF_QI(y1, IDWriteTextFormat1, y);
    tf_fb o; IDWriteFontFallback *me = (IDWriteFontFallback *)&o, *got = NULL;
    tf_fb_init(&o);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(x1, me));
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(y1, &got)); CHECK(got == NULL);       /* independent objects */
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(y1, me));
    EXPECT_EQ(o.ref, 3);
    DW_RELEASE(x1); DW_RELEASE(x); EXPECT_EQ(o.ref, 2);
    DW_RELEASE(y1); DW_RELEASE(y); EXPECT_EQ(o.ref, 1);
}

TEST(fallback_through_format2_and_3_pointers)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    tf_fb o; IDWriteFontFallback *me = (IDWriteFontFallback *)&o, *got = NULL;
    tf_fb_init(&o);
    CHECK_HR_OK(IDWriteTextFormat2_SetFontFallback(t2, me));
    CHECK_HR_OK(IDWriteTextFormat3_GetFontFallback(t3, &got)); CHECK(got == me); DW_RELEASE(got);
    CHECK_HR_OK(IDWriteTextFormat3_SetFontFallback(t3, NULL));
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(t1, &got)); CHECK(got == NULL);
    EXPECT_EQ(o.ref, 1);
    DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(fallback_set_does_not_validate_or_call_the_object)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    tf_fb o; tf_fb_init(&o);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, (IDWriteFontFallback *)&o));
    EXPECT_EQ(o.map, 0); EXPECT_EQ(o.qi, 0); EXPECT_EQ(o.release, 0);
    DW_RELEASE(t1); DW_RELEASE(t);
    EXPECT_EQ(o.release, 1); EXPECT_EQ(o.addref, 1);
}

TEST(fallback_system_fallback_object_roundtrip)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t);
    IDWriteFactory2 *f2 = (IDWriteFactory2 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 2);
    IDWriteFontFallback *sys = NULL, *got = NULL;
    if (!f2) { t_skip("IDWriteFactory2 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory2_GetSystemFontFallback(f2, &sys));
    if (!sys) { t_skip("no system fallback"); return; }
    ULONG r0 = dw_refcount((IUnknown *)sys);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(t1, sys));
    EXPECT_REFS(sys, r0 + 1);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(t1, &got));
    CHECK(got == sys);
    DW_RELEASE(got);
    DW_RELEASE(t1); DW_RELEASE(t);
    EXPECT_REFS(sys, r0);
    DW_RELEASE(sys);
}

/* ---- font axis values (IDWriteTextFormat3) ------------------------------------------------------------------------------------------ */
static DWRITE_FONT_AXIS_VALUE ax_sample[6] = {
    { DWRITE_FONT_AXIS_TAG_WEIGHT, 700.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 80.0f }, { DWRITE_FONT_AXIS_TAG_ITALIC, 1.0f },
    { DWRITE_FONT_AXIS_TAG_OPTICAL_SIZE, 12.0f }, { DWRITE_FONT_AXIS_TAG_SLANT, -9.5f }, { 0x41424344, 0.125f } };

TEST(axes_set_get_roundtrip_all_counts)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    UINT32 n, i, bad = 0;
    for (n = 0; n <= 6; n++) {
        DWRITE_FONT_AXIS_VALUE got[8];
        memset(got, 0xCC, sizeof got);
        CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, n ? ax_sample : NULL, n));
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), n);
        CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, n));
        for (i = 0; i < n; i++) if (got[i].axisTag != ax_sample[i].axisTag || !tf_fbits(got[i].value, ax_sample[i].value)) bad++;
    }
    CHECK(bad == 0);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_get_zero_fills_the_whole_buffer)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    DWRITE_FONT_AXIS_VALUE got[8]; unsigned char *p = (unsigned char *)got; size_t i, nz = 0;
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, ax_sample, 2));
    memset(got, 0xCC, sizeof got);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, 8));
    for (i = 2 * sizeof got[0]; i < sizeof got; i++) if (p[i] != 0) nz++;
    CHECKF(nz == 0, "%u bytes beyond the stored values are not zero", (unsigned)nz);
    EXPECT_EQ(got[0].axisTag, DWRITE_FONT_AXIS_TAG_WEIGHT); EXPECT_EQ(got[1].axisTag, DWRITE_FONT_AXIS_TAG_WIDTH);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_get_with_no_axes_zeroes_buffer_and_succeeds)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    DWRITE_FONT_AXIS_VALUE got[4]; unsigned char *p = (unsigned char *)got; size_t i, nz = 0;
    memset(got, 0xCC, sizeof got);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, 4));
    for (i = 0; i < sizeof got; i++) if (p[i]) nz++;
    CHECK(nz == 0);
    memset(got, 0xCC, sizeof got);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, 0));       /* zero capacity with zero axes */
    EXPECT_EQ(p[0], 0xCC);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_set_validation)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    DWRITE_FONT_AXIS_VALUE one = { DWRITE_FONT_AXIS_TAG_WEIGHT, 500.0f }; DWRITE_FONT_AXIS_VALUE *big;
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, ax_sample, 3));
    if (!t_is_wine()) {
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, NULL, 1), E_INVALIDARG);
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, NULL, 0xffff), E_INVALIDARG);
    }
    if (!t_is_wine()) {         /* the count is rejected before the array is read; the emulated implementation reads it */
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, &one, 0x10000), E_INVALIDARG);
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, &one, 0x10001), E_INVALIDARG);
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, &one, 0x7fffffff), E_INVALIDARG);
        CHECK_HR(IDWriteTextFormat3_SetFontAxisValues(t3, &one, 0xffffffffu), E_INVALIDARG);
    }
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 3);           /* failed calls keep the previous axes */
    big = (DWRITE_FONT_AXIS_VALUE *)calloc(0xFFFF, sizeof *big);
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, big, 0xFFFF));
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 0xFFFF);
    free(big);
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, NULL, 0));       /* NULL is fine for an empty list and clears the axes */
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 0);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_values_are_stored_verbatim)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    /* No validation of tags or values, no sorting, no de-duplication. */
    DWRITE_FONT_AXIS_VALUE in[7] = { { 0, 0.0f }, { 0xffffffffu, -1.0f }, { DWRITE_FONT_AXIS_TAG_WEIGHT, 1.0f }, { DWRITE_FONT_AXIS_TAG_WEIGHT, 2.0f },
        { DWRITE_FONT_AXIS_TAG_WIDTH, INFINITY }, { DWRITE_FONT_AXIS_TAG_ITALIC, NAN }, { DWRITE_FONT_AXIS_TAG_SLANT, FLT_MAX } }, got[7];
    UINT32 i, bad = 0;
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, in, 7));
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 7);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, 7));
    for (i = 0; i < 7; i++) if (got[i].axisTag != in[i].axisTag || memcmp(&got[i].value, &in[i].value, 4)) bad++;
    CHECKF(bad == 0, "%u entries differ", bad);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_input_array_is_copied)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    DWRITE_FONT_AXIS_VALUE in[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 100.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 50.0f } }, got[2];
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, in, 2));
    in[0].value = 900.0f; in[1].axisTag = 0;
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, 2));
    CHECK_FBITS(got[0].value, 100.0f); EXPECT_EQ(got[1].axisTag, DWRITE_FONT_AXIS_TAG_WIDTH);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_replace_with_shorter_and_longer)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    DWRITE_FONT_AXIS_VALUE got[6];
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, ax_sample, 6)); EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 6);
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, ax_sample + 4, 1)); EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 1);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, 1)); EXPECT_EQ(got[0].axisTag, DWRITE_FONT_AXIS_TAG_SLANT);
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, ax_sample, 6)); EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(t3), 6);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, 6)); EXPECT_EQ(got[5].axisTag, 0x41424344);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_formats_independent)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    TF_QI(a3, IDWriteTextFormat3, a); TF_QI(b3, IDWriteTextFormat3, b);
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(a3, ax_sample, 4));
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(a3), 4); EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(b3), 0);
    DW_RELEASE(a3); DW_RELEASE(b3); DW_RELEASE(a); DW_RELEASE(b);
}

TEST(axes_do_not_change_weight_style_stretch)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    DWRITE_FONT_AXIS_VALUE w = { DWRITE_FONT_AXIS_TAG_WEIGHT, 900.0f }, wd = { DWRITE_FONT_AXIS_TAG_WIDTH, 50.0f };
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, &w, 1)); CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, &wd, 1));
    EXPECT_EQ(IDWriteTextFormat_GetFontWeight(t), DWRITE_FONT_WEIGHT_NORMAL);
    EXPECT_EQ(IDWriteTextFormat_GetFontStretch(t), DWRITE_FONT_STRETCH_NORMAL);
    EXPECT_EQ(IDWriteTextFormat_GetFontStyle(t), DWRITE_FONT_STYLE_NORMAL);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_independent_of_automatic_axes)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    CHECK_HR_OK(IDWriteTextFormat3_SetAutomaticFontAxes(t3, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE));
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, ax_sample, 2));
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, NULL, 0));
    EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(t3), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
    DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_visible_through_format_pointer_chain)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    TF_QI(u3, IDWriteTextFormat3, t);
    DWRITE_FONT_AXIS_VALUE got[2];
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, ax_sample, 2));
    EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(u3), 2);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(u3, got, 2)); EXPECT_EQ(got[1].axisTag, DWRITE_FONT_AXIS_TAG_WIDTH);
    DW_RELEASE(u3); DW_RELEASE(t3); DW_RELEASE(t);
}

TEST(axes_set_survives_buffer_larger_than_count_on_get)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t);
    DWRITE_FONT_AXIS_VALUE got[64]; UINT32 i, bad = 0;
    CHECK_HR_OK(IDWriteTextFormat3_SetFontAxisValues(t3, ax_sample, 6));
    memset(got, 0x7F, sizeof got);
    CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(t3, got, 64));
    for (i = 0; i < 6; i++) if (got[i].axisTag != ax_sample[i].axisTag) bad++;
    for (i = 6; i < 64; i++) if (got[i].axisTag != 0 || got[i].value != 0.0f) bad++;
    CHECK(bad == 0);
    DW_RELEASE(t3); DW_RELEASE(t);
}

/* ---- layout inherits the format ------------------------------------------------------------------------------------------------------ */
static IDWriteTextLayout *mk_layout(IDWriteFactory *f, IDWriteTextFormat *t, const WCHAR *txt)
{
    IDWriteTextLayout *l = NULL;
    HRESULT hr = IDWriteFactory_CreateTextLayout(f, txt, (UINT32)wcslen(txt), t, 300.0f, 200.0f, &l);
    if (FAILED(hr) || !l) t_skip("CreateTextLayout -> %s", dw_hr_name(hr));
    return l;
}

TEST(layout_copies_every_format_property)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    dwx_inline *sg = dwx_inline_new(10, 10, 8, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2e, 3 }, otr; IDWriteInlineObject *osg = NULL;
    DWRITE_LINE_SPACING ls = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 25.0f, 20.0f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, ols;
    tf_fb fb; IDWriteFontFallback *ofb = NULL;
    DWRITE_FONT_AXIS_VALUE ax[2] = { { DWRITE_FONT_AXIS_TAG_WEIGHT, 600.0f }, { DWRITE_FONT_AXIS_TAG_WIDTH, 90.0f } }, oax[2];
    tf_fb_init(&fb);
    IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_TRAILING); IDWriteTextFormat_SetParagraphAlignment(t, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_NO_WRAP); IDWriteTextFormat_SetReadingDirection(t, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextFormat_SetIncrementalTabStop(t, 44.0f); IDWriteTextFormat_SetTrimming(t, &tr, (IDWriteInlineObject *)sg);
    IDWriteTextFormat2_SetLineSpacing(t2, &ls);
    IDWriteTextFormat1_SetVerticalGlyphOrientation(t1, DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED); IDWriteTextFormat1_SetOpticalAlignment(t1, DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
    IDWriteTextFormat1_SetLastLineWrapping(t1, FALSE); IDWriteTextFormat1_SetFontFallback(t1, (IDWriteFontFallback *)&fb);
    IDWriteTextFormat3_SetAutomaticFontAxes(t3, DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE); IDWriteTextFormat3_SetFontAxisValues(t3, ax, 2);
    {
        IDWriteTextLayout *l = mk_layout(f, t, L"Hello");
        TF_QI(l0, IDWriteTextFormat, l); TF_QI(l1, IDWriteTextFormat1, l); TF_QI(l2, IDWriteTextFormat2, l); TF_QI(l3, IDWriteTextFormat3, l);
        WCHAR b[32]; IDWriteFontCollection *c = NULL, *tc = NULL;
        EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(l0), DWRITE_TEXT_ALIGNMENT_TRAILING); EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(l0), DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(l0), DWRITE_WORD_WRAPPING_NO_WRAP); EXPECT_EQ(IDWriteTextFormat_GetReadingDirection(l0), DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
        EXPECT_EQ(IDWriteTextFormat_GetFlowDirection(l0), DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
        CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(l0), 44.0f);
        CHECK_HR_OK(IDWriteTextFormat_GetTrimming(l0, &otr, &osg)); EXPECT_EQ(otr.granularity, DWRITE_TRIMMING_GRANULARITY_WORD); EXPECT_EQ(otr.delimiter, 0x2e); EXPECT_EQ(otr.delimiterCount, 3);
        CHECK(osg == (IDWriteInlineObject *)sg); DW_RELEASE(osg);
        CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(l2, &ols)); EXPECT_EQ(ols.method, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FBITS(ols.height, 25.0f);
        CHECK_FBITS(ols.baseline, 20.0f); CHECK_FBITS(ols.leadingBefore, 0.5f); EXPECT_EQ(ols.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_ENABLED);
        EXPECT_EQ(IDWriteTextFormat1_GetVerticalGlyphOrientation(l1), DWRITE_VERTICAL_GLYPH_ORIENTATION_STACKED); EXPECT_EQ(IDWriteTextFormat1_GetOpticalAlignment(l1), DWRITE_OPTICAL_ALIGNMENT_NO_SIDE_BEARINGS);
        EXPECT_EQ(IDWriteTextFormat1_GetLastLineWrapping(l1), FALSE);
        CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(l1, &ofb)); CHECK(ofb == (IDWriteFontFallback *)&fb); DW_RELEASE(ofb);
        EXPECT_EQ(IDWriteTextFormat3_GetAutomaticFontAxes(l3), DWRITE_AUTOMATIC_FONT_AXES_OPTICAL_SIZE);
        EXPECT_EQ(IDWriteTextFormat3_GetFontAxisValueCount(l3), 2);
        memset(oax, 0, sizeof oax); CHECK_HR_OK(IDWriteTextFormat3_GetFontAxisValues(l3, oax, 2)); EXPECT_EQ(oax[0].axisTag, DWRITE_FONT_AXIS_TAG_WEIGHT); CHECK_FBITS(oax[1].value, 90.0f);
        EXPECT_EQ(IDWriteTextFormat_GetFontWeight(l0), DWRITE_FONT_WEIGHT_NORMAL); EXPECT_EQ(IDWriteTextFormat_GetFontStyle(l0), DWRITE_FONT_STYLE_NORMAL);
        EXPECT_EQ(IDWriteTextFormat_GetFontStretch(l0), DWRITE_FONT_STRETCH_NORMAL); CHECK_FBITS(IDWriteTextFormat_GetFontSize(l0), 16.0f);
        CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(l0, b, 32)); CHECK_WEQ(b, TF_FAM);
        CHECK_HR_OK(IDWriteTextFormat_GetLocaleName(l0, b, 32)); CHECK_WEQ(b, TF_LOC);
        CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(l0, &c)); CHECK_HR_OK(IDWriteTextFormat_GetFontCollection(t, &tc)); CHECK(c != NULL && c == tc);
        DW_RELEASE(c); DW_RELEASE(tc);
        DW_RELEASE(l0); DW_RELEASE(l1); DW_RELEASE(l2); DW_RELEASE(l3); DW_RELEASE(l);
    }
    DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3);
    IDWriteTextFormat_SetTrimming(t, &tr, NULL);
    DW_RELEASE(t);
    EXPECT_EQ(dwx_refs(sg), 1); EXPECT_EQ(fb.ref, 1);       /* layout and format released everything they retained */
    dwx_release(sg);
}

TEST(layout_holds_its_own_sign_reference)
{
    TF_NEW(t);
    dwx_inline *sg = dwx_inline_new(10, 10, 8, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, (IDWriteInlineObject *)sg));
    EXPECT_EQ(dwx_refs(sg), 2);
    IDWriteTextLayout *l = mk_layout(f, t, L"abc");
    CHECK(dwx_refs(sg) >= 3);                               /* the layout copied the sign reference */
    IDWriteTextFormat_SetTrimming(t, &tr, NULL);
    CHECK(dwx_refs(sg) >= 2);                               /* ... which survives the format dropping its own */
    DW_RELEASE(l);
    EXPECT_EQ(dwx_refs(sg), 1);
    DW_RELEASE(t);
    dwx_release(sg);
}

TEST(layout_is_a_snapshot_of_the_format)
{
    TF_NEW(t);
    IDWriteTextLayout *l = mk_layout(f, t, L"snapshot");
    TF_QI(l0, IDWriteTextFormat, l);
    IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat_SetIncrementalTabStop(t, 5.0f);
    IDWriteTextFormat_SetWordWrapping(t, DWRITE_WORD_WRAPPING_NO_WRAP);
    EXPECT_EQ(IDWriteTextFormat_GetTextAlignment(l0), DWRITE_TEXT_ALIGNMENT_LEADING);       /* later format changes do not reach the layout */
    CHECK_FBITS(IDWriteTextFormat_GetIncrementalTabStop(l0), 64.0f);
    EXPECT_EQ(IDWriteTextFormat_GetWordWrapping(l0), DWRITE_WORD_WRAPPING_WRAP);
    CHECK_HR_OK(IDWriteTextFormat_SetParagraphAlignment(l0, DWRITE_PARAGRAPH_ALIGNMENT_FAR));        /* ... and layout changes do not reach the format */
    EXPECT_EQ(IDWriteTextFormat_GetParagraphAlignment(t), DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    DW_RELEASE(l0); DW_RELEASE(l); DW_RELEASE(t);
}

TEST(layout_qi_reaches_the_format_interfaces)
{
    TF_NEW(t);
    IDWriteTextLayout *l = mk_layout(f, t, L"qi");
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteTextFormat, 1), DW_QI_ROW(IDWriteTextFormat1, 1), DW_QI_ROW(IDWriteTextFormat2, 1), DW_QI_ROW(IDWriteTextFormat3, 1),
                         DW_QI_ROW(IDWriteTextLayout, 1), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontCollection, 0) };
    DW_CHECK_QI(l, rows);
    DW_RELEASE(l); DW_RELEASE(t);
}

TEST(layout_format_reads_do_not_touch_format_refcount)
{
    TF_NEW(t);
    ULONG r0 = dw_refcount((IUnknown *)t);
    IDWriteTextLayout *l = mk_layout(f, t, L"refs");
    EXPECT_REFS(t, r0);                                     /* the layout copies values; it does not keep the format alive */
    DW_RELEASE(l);
    EXPECT_REFS(t, r0);
    DW_RELEASE(t);
}

TEST(layout_create_with_null_format_is_invalid)
{
    TF_NEED_FACTORY();
    if (t_is_wine()) { t_skip("NULL format faults in the emulated implementation"); return; }
    IDWriteTextLayout *l = (IDWriteTextLayout *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateTextLayout(f, L"abc", 3, NULL, 100.0f, 100.0f, &l), E_INVALIDARG);
    CHECK_OUT_NULL(l);
}

TEST(layout_direction_matrix_notes)
{
    TF_NEED_FACTORY();
    int r, fl;
    for (r = 0; r < 4; r++) for (fl = 0; fl < 4; fl++) {
        IDWriteTextFormat *t = tf_new(f); IDWriteTextLayout *l = NULL; DWRITE_TEXT_METRICS m; HRESULT hc, hm = E_FAIL;
        IDWriteTextFormat_SetReadingDirection(t, (DWRITE_READING_DIRECTION)r);
        IDWriteTextFormat_SetFlowDirection(t, (DWRITE_FLOW_DIRECTION)fl);
        hc = IDWriteFactory_CreateTextLayout(f, L"abc", 3, t, 200.0f, 100.0f, &l);
        if (SUCCEEDED(hc) && l) hm = IDWriteTextLayout_GetMetrics(l, &m);
        /* Horizontal reading with horizontal flow, or vertical with vertical, conflicts (DWRITE_E_FLOWDIRECTIONCONFLICTS) - recorded only. */
        t_note("reading %d flow %d: CreateTextLayout %s GetMetrics %s", r, fl, dw_hr_name(hc), dw_hr_name(hm));
        if (r == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT && fl == DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM) { CHECK_HR_OK(hc); if (SUCCEEDED(hc) && !t_is_wine()) CHECK_HR_OK(hm); }
        DW_RELEASE(l); DW_RELEASE(t);
    }
}

/* ---- two formats from one factory ----------------------------------------------------------------------------------------------------- */
TEST(two_formats_keep_independent_complete_state)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_make(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us");
    IDWriteTextFormat *b = tf_make(f, L"Times New Roman", DWRITE_FONT_WEIGHT_LIGHT, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 30.0f, L"de-de");
    tf_state sa0, sb0;
    tf_snap(b, &sb0);
    IDWriteTextFormat_SetTextAlignment(a, DWRITE_TEXT_ALIGNMENT_CENTER); IDWriteTextFormat_SetParagraphAlignment(a, DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    IDWriteTextFormat_SetWordWrapping(a, DWRITE_WORD_WRAPPING_NO_WRAP); IDWriteTextFormat_SetReadingDirection(a, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    IDWriteTextFormat_SetFlowDirection(a, DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT); IDWriteTextFormat_SetIncrementalTabStop(a, 1.0f);
    IDWriteTextFormat_SetLineSpacing(a, DWRITE_LINE_SPACING_METHOD_UNIFORM, 9.0f, 7.0f);
    { DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2e, 2 }; IDWriteTextFormat_SetTrimming(a, &tr, NULL); }
    tf_snap(a, &sa0);
    CHECK_STATE_SAME(b, sb0);
    CHECK(sa0.w == DWRITE_FONT_WEIGHT_BOLD && sb0.w == DWRITE_FONT_WEIGHT_LIGHT);
    DW_RELEASE(a);
    CHECK_STATE_SAME(b, sb0);
    DW_RELEASE(b);
}

TEST(state_snapshot_of_fresh_format_is_stable)
{
    TF_NEW(t);
    tf_state a, b;
    tf_snap(t, &a); tf_snap(t, &b);
    CHECK(tf_state_eq(&a, &b));
    EXPECT_EQ(a.famlen, 5); EXPECT_EQ(a.loclen, 5);
    DW_RELEASE(t);
}
