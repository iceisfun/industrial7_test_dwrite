/* SetTrimming/GetTrimming (options + inline trimming sign) and the two line spacing interfaces:
 *   IDWriteTextFormat::SetLineSpacing(method, spacing, baseline)    (also resets the IDWriteTextFormat2 fields)
 *   IDWriteTextFormat2::SetLineSpacing(const DWRITE_LINE_SPACING *) (adds leadingBefore and fontLineGapUsage)
 *
 * The trimming sign is only reference counted by the format (AddRef on Set/Get, Release on replacement and destruction); no other
 * method of the sign is called while it is stored or read back. */
#include "tf.h"

#define SIGN(o) ((IDWriteInlineObject *)(o))

static int sign_idle(dwx_inline *s) { return s->n_draw == 0 && s->n_metrics == 0 && s->n_overhang == 0 && s->n_break == 0 && dwx_qi_calls(s) == 0; }

/* ---- trimming options ---------------------------------------------------------------------------------------------- */
TEST(trimming_roundtrip_matrix)
{
    TF_NEW(t);
    static const UINT32 delim[] = { 0, 0x20, 0x2e, 0x2026, 0xd800, 0xffff, 0x10000, 0x10ffff };
    static const UINT32 cnt[] = { 0, 1, 2, 5, 0x7fffffff, 0x80000000u, 0xffffffffu };
    int g, bad = 0; size_t d, c;
    for (g = 0; g <= 2; g++) for (d = 0; d < sizeof delim / sizeof delim[0]; d++) for (c = 0; c < sizeof cnt / sizeof cnt[0]; c++) {
        DWRITE_TRIMMING in, out; IDWriteInlineObject *sg = SIGN(1);
        in.granularity = (DWRITE_TRIMMING_GRANULARITY)g; in.delimiter = delim[d]; in.delimiterCount = cnt[c];
        memset(&out, 0xCC, sizeof out);
        if (IDWriteTextFormat_SetTrimming(t, &in, NULL) != S_OK) { bad++; continue; }
        if (IDWriteTextFormat_GetTrimming(t, &out, &sg) != S_OK) { bad++; continue; }
        if (out.granularity != in.granularity || out.delimiter != in.delimiter || out.delimiterCount != in.delimiterCount || sg != NULL) bad++;
    }
    CHECKF(bad == 0, "%d combinations failed", bad);
    DW_RELEASE(t);
}

TEST(trimming_granularity_invalid)
{
    TF_NEW(t);
    static const UINT32 bad[] = { 3, 4, 7, 255, 256, 0x10000, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    DWRITE_TRIMMING keep = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2d, 9 }, now; IDWriteInlineObject *sg = NULL; size_t i;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &keep, NULL));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWRITE_TRIMMING in = { (DWRITE_TRIMMING_GRANULARITY)bad[i], 0x20, 1 };
        HRESULT hr = IDWriteTextFormat_SetTrimming(t, &in, NULL);
        CHECKF(hr == E_INVALIDARG, "granularity 0x%x -> %s", bad[i], dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &now, &sg));
        EXPECT_EQ(now.granularity, keep.granularity); EXPECT_EQ(now.delimiter, keep.delimiter); EXPECT_EQ(now.delimiterCount, keep.delimiterCount);
    }
    DW_RELEASE(t);
}

TEST(trimming_delimiter_invalid_above_unicode_range)
{
    TF_NEW(t);
    static const UINT32 bad[] = { 0x110000, 0x110001, 0x200000, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    DWRITE_TRIMMING keep = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0x10ffff, 4 }, now; IDWriteInlineObject *sg = NULL; size_t i;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &keep, NULL));        /* 0x10FFFF itself is the last accepted delimiter */
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWRITE_TRIMMING in = { DWRITE_TRIMMING_GRANULARITY_WORD, bad[i], 1 };
        HRESULT hr = IDWriteTextFormat_SetTrimming(t, &in, NULL);
        CHECKF(hr == E_INVALIDARG, "delimiter 0x%x -> %s", bad[i], dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &now, &sg));
        EXPECT_EQ(now.granularity, keep.granularity); EXPECT_EQ(now.delimiter, keep.delimiter); EXPECT_EQ(now.delimiterCount, keep.delimiterCount);
    }
    DW_RELEASE(t);
}

TEST(trimming_delimiter_count_is_not_validated)
{
    TF_NEW(t);
    static const UINT32 cnt[] = { 0, 1, 100, 0x7fffffffu, 0x80000000u, 0xffffffffu };
    size_t i;
    for (i = 0; i < sizeof cnt / sizeof cnt[0]; i++) {
        DWRITE_TRIMMING in = { DWRITE_TRIMMING_GRANULARITY_NONE, 0, cnt[i] }, out; IDWriteInlineObject *sg = NULL;
        CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &in, NULL));
        CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &sg));
        EXPECT_EQ(out.delimiterCount, cnt[i]);
    }
    DW_RELEASE(t);
}

TEST(trimming_null_options_is_invalid)
{
    TF_NEW(t);
    dwx_inline *sg = dwx_inline_new(10, 10, 8, FALSE);
    DWRITE_TRIMMING keep = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &keep, SIGN(sg)));
    EXPECT_EQ(dwx_refs(sg), 2);
    if (!t_is_wine()) {      /* a NULL options pointer faults in the emulated implementation */
        CHECK_HR(IDWriteTextFormat_SetTrimming(t, NULL, NULL), E_INVALIDARG);
        CHECK_HR(IDWriteTextFormat_SetTrimming(t, NULL, SIGN(sg)), E_INVALIDARG);
    }
    EXPECT_EQ(dwx_refs(sg), 2);                  /* the sign is neither replaced nor released by a failed call */
    {
        DWRITE_TRIMMING now; IDWriteInlineObject *got = NULL;
        CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &now, &got));
        CHECK(got == SIGN(sg)); EXPECT_EQ(now.granularity, DWRITE_TRIMMING_GRANULARITY_CHARACTER);
        DW_RELEASE(got);
    }
    DW_RELEASE(t);
    EXPECT_EQ(dwx_refs(sg), 1);
    dwx_release(sg);
}

TEST(trimming_failed_set_does_not_addref_the_new_sign)
{
    TF_NEW(t);
    dwx_inline *old = dwx_inline_new(1, 1, 1, FALSE), *nw = dwx_inline_new(2, 2, 2, FALSE);
    DWRITE_TRIMMING ok = { DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 }, bad = { (DWRITE_TRIMMING_GRANULARITY)3, 0, 0 }, bad2 = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x110000, 0 };
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &ok, SIGN(old)));
    CHECK_HR(IDWriteTextFormat_SetTrimming(t, &bad, SIGN(nw)), E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_SetTrimming(t, &bad2, SIGN(nw)), E_INVALIDARG);
    EXPECT_EQ(dwx_refs(nw), 1); EXPECT_EQ(dwx_refs(old), 2);
    DW_RELEASE(t);
    EXPECT_EQ(dwx_refs(old), 1);
    dwx_release(old); dwx_release(nw);
}

TEST(trimming_sign_reference_counting)
{
    TF_NEW(t);
    dwx_inline *sg = dwx_inline_new(12, 14, 11, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }, out; IDWriteInlineObject *g1 = NULL, *g2 = NULL;
    EXPECT_EQ(dwx_refs(sg), 1);
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(sg)));
    EXPECT_EQ(dwx_refs(sg), 2);                                   /* the format holds one reference */
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &g1));
    CHECK(g1 == SIGN(sg)); EXPECT_EQ(dwx_refs(sg), 3);            /* Get hands out an owned reference */
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &g2));
    CHECK(g2 == SIGN(sg)); EXPECT_EQ(dwx_refs(sg), 4);
    IDWriteInlineObject_Release(g1); IDWriteInlineObject_Release(g2);
    EXPECT_EQ(dwx_refs(sg), 2);
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, NULL));      /* replacing with NULL releases the stored sign */
    EXPECT_EQ(dwx_refs(sg), 1);
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &g1)); CHECK(g1 == NULL);
    CHECK(sign_idle(sg));                                          /* nothing but AddRef/Release was ever invoked on it */
    DW_RELEASE(t);
    dwx_release(sg);
}

TEST(trimming_sign_replacement_releases_previous)
{
    TF_NEW(t);
    dwx_inline *a = dwx_inline_new(1, 1, 1, FALSE), *b = dwx_inline_new(2, 2, 2, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 }, out; IDWriteInlineObject *got = NULL;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(a))); EXPECT_EQ(dwx_refs(a), 2); EXPECT_EQ(dwx_refs(b), 1);
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(b))); EXPECT_EQ(dwx_refs(a), 1); EXPECT_EQ(dwx_refs(b), 2);
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &got)); CHECK(got == SIGN(b)); DW_RELEASE(got);
    DW_RELEASE(t);
    EXPECT_EQ(dwx_refs(b), 1);
    dwx_release(a); dwx_release(b);
}

TEST(trimming_same_sign_set_twice_keeps_one_reference)
{
    TF_NEW(t);
    dwx_inline *sg = dwx_inline_new(1, 1, 1, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(sg)));
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(sg)));         /* new reference is taken before the old one is dropped */
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(sg)));
    EXPECT_EQ(dwx_refs(sg), 2); EXPECT_EQ(sg->b.destroyed, 0);
    DW_RELEASE(t);
    EXPECT_EQ(dwx_refs(sg), 1);
    dwx_release(sg);
}

TEST(trimming_sign_kept_alive_by_format)
{
    TF_NEW(t);
    dwx_inline *sg = dwx_inline_new(1, 1, 1, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }, out; IDWriteInlineObject *got = NULL;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(sg)));
    dwx_release(sg);                                                       /* application drops its reference */
    EXPECT_EQ(dwx_destroyed(sg), 0); EXPECT_EQ(dwx_refs(sg), 1);
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &got));
    CHECK(got == SIGN(sg)); DW_RELEASE(got);
    EXPECT_EQ(dwx_destroyed(sg), 0);
    DW_RELEASE(t);
    EXPECT_EQ(dwx_destroyed(sg), 1);                                       /* the format destroyed its last reference */
}

TEST(trimming_sign_shared_between_formats)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    dwx_inline *sg = dwx_inline_new(1, 1, 1, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(a, &tr, SIGN(sg)));
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(b, &tr, SIGN(sg)));
    EXPECT_EQ(dwx_refs(sg), 3);
    DW_RELEASE(a); EXPECT_EQ(dwx_refs(sg), 2);
    DW_RELEASE(b); EXPECT_EQ(dwx_refs(sg), 1);
    dwx_release(sg);
}

TEST(trimming_sign_not_inspected_at_set_time)
{
    TF_NEW(t);
    /* A sign whose methods all fail is accepted: nothing but reference counting happens until a layout uses it. */
    dwx_inline *sg = dwx_inline_new(1, 1, 1, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0, 0 };
    sg->fail_metrics = E_FAIL; sg->fail_overhang = E_FAIL; sg->fail_break = E_FAIL; sg->fail_draw = E_FAIL;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(sg)));
    CHECK(sign_idle(sg));
    DW_RELEASE(t);
    dwx_release(sg);
}

TEST(trimming_sign_without_granularity_still_stored)
{
    TF_NEW(t);
    dwx_inline *sg = dwx_inline_new(1, 1, 1, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_NONE, 0, 0 }, out; IDWriteInlineObject *got = NULL;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(sg)));          /* the sign is kept even though nothing will be trimmed */
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &got));
    CHECK(got == SIGN(sg)); DW_RELEASE(got);
    DW_RELEASE(t); dwx_release(sg);
}

TEST(trimming_get_overwrites_all_output_fields)
{
    TF_NEW(t);
    DWRITE_TRIMMING in = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x41, 0x42 }, out; IDWriteInlineObject *got = SIGN(0x5);
    unsigned char *p = (unsigned char *)&out; size_t i, bad = 0;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &in, NULL));
    memset(&out, 0xEE, sizeof out);
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &got));
    CHECK(got == NULL);
    for (i = 0; i < sizeof out; i++) if (p[i] == 0xEE) bad++;
    EXPECT_EQ(out.granularity, 2); EXPECT_EQ(out.delimiter, 0x41); EXPECT_EQ(out.delimiterCount, 0x42);
    DW_RELEASE(t);
}

TEST(trimming_with_factory_ellipsis_sign)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *t = tf_new(f);
    IDWriteInlineObject *sg = NULL, *got = NULL; DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 }, out;
    HRESULT hr = IDWriteFactory_CreateEllipsisTrimmingSign(f, t, &sg);
    if (FAILED(hr) || !sg) { t_skip("CreateEllipsisTrimmingSign -> %s", dw_hr_name(hr)); }
    ULONG r0 = dw_refcount((IUnknown *)sg);
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, sg));
    EXPECT_REFS(sg, r0 + 1);
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &got));
    CHECK(got == sg); DW_RELEASE(got);
    EXPECT_REFS(sg, r0 + 1);
    DW_RELEASE(t);
    EXPECT_REFS(sg, r0);
    DW_RELEASE(sg);
}

TEST(trimming_through_every_interface_pointer)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    dwx_inline *sg = dwx_inline_new(1, 1, 1, FALSE);
    DWRITE_TRIMMING a = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0x2e, 1 }, b = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x20, 2 }, out; IDWriteInlineObject *got = NULL;
    CHECK_HR_OK(IDWriteTextFormat1_SetTrimming(t1, &a, SIGN(sg)));
    CHECK_HR_OK(IDWriteTextFormat2_GetTrimming(t2, &out, &got)); EXPECT_EQ(out.delimiter, 0x2e); CHECK(got == SIGN(sg)); DW_RELEASE(got);
    CHECK_HR_OK(IDWriteTextFormat3_SetTrimming(t3, &b, NULL));
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &got)); EXPECT_EQ(out.granularity, DWRITE_TRIMMING_GRANULARITY_WORD); EXPECT_EQ(out.delimiterCount, 2); CHECK(got == NULL);
    EXPECT_EQ(dwx_refs(sg), 1);
    if (!t_is_wine()) CHECK_HR(IDWriteTextFormat3_SetTrimming(t3, NULL, NULL), E_INVALIDARG);
    DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3); DW_RELEASE(t);
    dwx_release(sg);
}

TEST(trimming_leaves_other_properties_alone)
{
    TF_NEW(t);
    tf_state a, b; dwx_inline *sg = dwx_inline_new(1, 1, 1, FALSE);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2e, 3 };
    IDWriteTextFormat_SetTextAlignment(t, DWRITE_TEXT_ALIGNMENT_CENTER);
    IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 0.0f);
    tf_snap(t, &a);
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, SIGN(sg)));
    tf_snap(t, &b);
    CHECK(a.ta == b.ta && a.pa == b.pa && a.ww == b.ww && a.rd == b.rd && a.fd == b.fd && tf_fbits(a.tab, b.tab) && a.lsm == b.lsm && tf_fbits(a.ls, b.ls) && tf_fbits(a.bl, b.bl));
    CHECK(a.w == b.w && a.size == b.size && !wcscmp(a.fam, b.fam) && !wcscmp(a.loc, b.loc));
    DW_RELEASE(t); dwx_release(sg);
}

TEST(trimming_formats_independent)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0x2e, 9 }, out; IDWriteInlineObject *got = NULL;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(a, &tr, NULL));
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(b, &out, &got));
    EXPECT_EQ(out.granularity, DWRITE_TRIMMING_GRANULARITY_NONE); EXPECT_EQ(out.delimiter, 0); EXPECT_EQ(out.delimiterCount, 0);
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(trimming_options_copied_not_referenced)
{
    TF_NEW(t);
    DWRITE_TRIMMING tr = { DWRITE_TRIMMING_GRANULARITY_WORD, 0x2e, 3 }, out; IDWriteInlineObject *got = NULL;
    CHECK_HR_OK(IDWriteTextFormat_SetTrimming(t, &tr, NULL));
    tr.granularity = DWRITE_TRIMMING_GRANULARITY_NONE; tr.delimiter = 0; tr.delimiterCount = 0;
    CHECK_HR_OK(IDWriteTextFormat_GetTrimming(t, &out, &got));
    EXPECT_EQ(out.granularity, DWRITE_TRIMMING_GRANULARITY_WORD); EXPECT_EQ(out.delimiter, 0x2e); EXPECT_EQ(out.delimiterCount, 3);
    DW_RELEASE(t);
}

/* ---- line spacing, classic form --------------------------------------------------------------------------------------- */
TEST(linespacing_roundtrip_matrix)
{
    TF_NEW(t);
    static const float sp[] = { 0.0f, -0.0f, 0.001f, 1.0f, 12.5f, 100.0f, 1e6f, FLT_MAX, INFINITY };
    static const float bl[] = { 0.0f, -0.0f, 0.5f, 10.0f, -1.0f, -100.0f, 1e6f, -FLT_MAX, FLT_MAX, -INFINITY, INFINITY };
    int m, bad = 0; size_t i, j;
    for (m = 0; m <= 2; m++) for (i = 0; i < sizeof sp / sizeof sp[0]; i++) for (j = 0; j < sizeof bl / sizeof bl[0]; j++) {
        DWRITE_LINE_SPACING_METHOD om = (DWRITE_LINE_SPACING_METHOD)77; float os = 7, ob = 7;
        if (IDWriteTextFormat_SetLineSpacing(t, (DWRITE_LINE_SPACING_METHOD)m, sp[i], bl[j]) != S_OK) { bad++; continue; }
        if (IDWriteTextFormat_GetLineSpacing(t, &om, &os, &ob) != S_OK) { bad++; continue; }
        if (om != (DWRITE_LINE_SPACING_METHOD)m || !tf_fbits(os, sp[i]) || !tf_fbits(ob, bl[j])) bad++;
    }
    CHECKF(bad == 0, "%d combinations failed", bad);
    DW_RELEASE(t);
}

TEST(linespacing_negative_baseline_is_accepted)
{
    TF_NEW(t);
    DWRITE_LINE_SPACING_METHOD m; float sp, bl;
    /* Only NaN is rejected for the baseline; a negative baseline is stored as given. */
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.0f, -8.0f));
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FBITS(sp, 20.0f); CHECK_FBITS(bl, -8.0f);
    DW_RELEASE(t);
}

TEST(linespacing_default_method_keeps_values)
{
    TF_NEW(t);
    DWRITE_LINE_SPACING_METHOD m; float sp, bl;
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_DEFAULT, 33.0f, 22.0f));
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_DEFAULT); CHECK_FBITS(sp, 33.0f); CHECK_FBITS(bl, 22.0f);   /* stored although DEFAULT ignores them */
    DW_RELEASE(t);
}

TEST(linespacing_invalid_method)
{
    TF_NEW(t);
    static const UINT32 bad[] = { 3, 4, 255, 0x10000, 0x7fffffffu, 0x80000000u, 0xffffffffu };
    DWRITE_LINE_SPACING_METHOD m; float sp, bl; size_t i;
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.25f, 0.5f));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteTextFormat_SetLineSpacing(t, (DWRITE_LINE_SPACING_METHOD)bad[i], 10.0f, 5.0f);
        CHECKF(hr == E_INVALIDARG, "method 0x%x -> %s", bad[i], dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl));
        EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL); CHECK_FBITS(sp, 1.25f); CHECK_FBITS(bl, 0.5f);
    }
    DW_RELEASE(t);
}

TEST(linespacing_invalid_spacing)
{
    TF_NEW(t);
    static const float bad[] = { -FLT_MIN, -0.001f, -1.0f, -100.0f, -FLT_MAX, -INFINITY, NAN, -NAN };
    DWRITE_LINE_SPACING_METHOD m; float sp, bl; size_t i;
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 18.0f, 14.0f));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HRESULT hr = IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, bad[i], 1.0f);
        CHECKF(hr == E_INVALIDARG, "spacing #%u (%g) -> %s", (unsigned)i, bad[i], dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl));
        EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FBITS(sp, 18.0f); CHECK_FBITS(bl, 14.0f);
    }
    DW_RELEASE(t);
}

TEST(linespacing_invalid_baseline_nan)
{
    TF_NEW(t);
    DWRITE_LINE_SPACING_METHOD m; float sp, bl;
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 18.0f, 14.0f));
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 18.0f, NAN), E_INVALIDARG);
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 18.0f, -NAN), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl));
    CHECK_FBITS(sp, 18.0f); CHECK_FBITS(bl, 14.0f);
    DW_RELEASE(t);
}

TEST(linespacing_negative_zero_spacing_accepted)
{
    TF_NEW(t);
    DWRITE_LINE_SPACING_METHOD m; float sp, bl;
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, -0.0f, 0.0f));       /* -0.0 compares equal to 0.0 */
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl));
    CHECK(sp == 0.0f);
    DW_RELEASE(t);
}

TEST(linespacing_formats_independent)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    DWRITE_LINE_SPACING_METHOD m; float sp, bl;
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(a, DWRITE_LINE_SPACING_METHOD_UNIFORM, 40.0f, 30.0f));
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(b, &m, &sp, &bl));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_DEFAULT); CHECK_FBITS(sp, 0.0f); CHECK_FBITS(bl, 0.0f);
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(linespacing_classic_through_every_interface)
{
    TF_NEW(t);
    TF_QI(t1, IDWriteTextFormat1, t); TF_QI(t2, IDWriteTextFormat2, t); TF_QI(t3, IDWriteTextFormat3, t);
    DWRITE_LINE_SPACING_METHOD m; float sp, bl;
    CHECK_HR_OK(IDWriteTextFormat1_SetLineSpacing(t1, DWRITE_LINE_SPACING_METHOD_UNIFORM, 11.0f, 9.0f));
    /* IDWriteTextFormat2/3 inherit the classic methods in their vtable; the struct forms have their own slots. */
    CHECK_HR_OK(t2->lpVtbl->GetLineSpacing(t2, &m, &sp, &bl)); EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FBITS(sp, 11.0f); CHECK_FBITS(bl, 9.0f);
    CHECK_HR_OK(t3->lpVtbl->SetLineSpacing(t3, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 2.0f, 3.0f));
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl)); EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL); CHECK_FBITS(sp, 2.0f); CHECK_FBITS(bl, 3.0f);
    CHECK_HR(t2->lpVtbl->SetLineSpacing(t2, (DWRITE_LINE_SPACING_METHOD)3, 1.0f, 1.0f), E_INVALIDARG);
    DW_RELEASE(t1); DW_RELEASE(t2); DW_RELEASE(t3); DW_RELEASE(t);
}

/* ---- line spacing, IDWriteTextFormat2 struct form ------------------------------------------------------------------------ */
TEST(linespacing2_roundtrip)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 24.0f, 18.0f, 0.25f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, out;
    memset(&out, 0x5A, sizeof out);
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(t2, &in));
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out));
    EXPECT_EQ(out.method, DWRITE_LINE_SPACING_METHOD_UNIFORM); CHECK_FBITS(out.height, 24.0f); CHECK_FBITS(out.baseline, 18.0f);
    CHECK_FBITS(out.leadingBefore, 0.25f); EXPECT_EQ(out.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_ENABLED);
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_struct_set_visible_through_classic_get)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 1.5f, 7.0f, 1.0f, DWRITE_FONT_LINE_GAP_USAGE_DISABLED };
    DWRITE_LINE_SPACING_METHOD m; float sp, bl;
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(t2, &in));
    CHECK_HR_OK(IDWriteTextFormat_GetLineSpacing(t, &m, &sp, &bl));
    EXPECT_EQ(m, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL); CHECK_FBITS(sp, 1.5f); CHECK_FBITS(bl, 7.0f);
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_classic_set_resets_extra_fields)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 24.0f, 18.0f, 0.75f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, out;
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(t2, &in));
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 30.0f, 20.0f));
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out));
    CHECK_FBITS(out.height, 30.0f); CHECK_FBITS(out.baseline, 20.0f);
    CHECK_FBITS(out.leadingBefore, 0.0f);                              /* the classic setter zeroes leadingBefore ... */
    EXPECT_EQ(out.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_DEFAULT);   /* ... and fontLineGapUsage */
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_failed_classic_set_keeps_extra_fields)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 24.0f, 18.0f, 0.75f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, out;
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(t2, &in));
    CHECK_HR(IDWriteTextFormat_SetLineSpacing(t, (DWRITE_LINE_SPACING_METHOD)9, 1.0f, 1.0f), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out));
    CHECK_FBITS(out.height, 24.0f); CHECK_FBITS(out.leadingBefore, 0.75f); EXPECT_EQ(out.fontLineGapUsage, DWRITE_FONT_LINE_GAP_USAGE_ENABLED);
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_null_struct_is_invalid)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING out;
    CHECK_HR_OK(IDWriteTextFormat_SetLineSpacing(t, DWRITE_LINE_SPACING_METHOD_UNIFORM, 5.0f, 4.0f));
    if (!t_is_wine()) CHECK_HR(IDWriteTextFormat2_SetLineSpacing(t2, NULL), E_INVALIDARG);
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out)); CHECK_FBITS(out.height, 5.0f);
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_validation_matrix)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    struct { DWRITE_LINE_SPACING ls; int ok; const char *why; } c[] = {
        { { 0, 0.0f, 0.0f, 0.0f, 0 }, 1, "all zero" },
        { { 2, 1.0f, 0.0f, 1.0f, 2 }, 1, "max method, leadingBefore 1" },
        { { 1, 10.0f, -5.0f, 0.5f, 0 }, 1, "negative baseline" },
        { { 1, 10.0f, 5.0f, -0.0f, 0 }, 1, "leadingBefore -0.0" },
        { { 3, 10.0f, 5.0f, 0.5f, 0 }, 0, "method 3" },
        { { (DWRITE_LINE_SPACING_METHOD)0xffffffffu, 10.0f, 5.0f, 0.5f, 0 }, 0, "method -1" },
        { { 1, -1.0f, 5.0f, 0.5f, 0 }, 0, "negative height" },
        { { 1, -INFINITY, 5.0f, 0.5f, 0 }, 0, "-inf height" },
        { { 1, NAN, 5.0f, 0.5f, 0 }, 0, "NaN height" },
        { { 1, 10.0f, NAN, 0.5f, 0 }, 0, "NaN baseline" },
        { { 1, 10.0f, 5.0f, NAN, 0 }, 0, "NaN leadingBefore" },
        { { 1, 10.0f, 5.0f, -0.001f, 0 }, 0, "leadingBefore < 0" },
        { { 1, 10.0f, 5.0f, 1.0001f, 0 }, 0, "leadingBefore > 1" },
        { { 1, 10.0f, 5.0f, 2.0f, 0 }, 0, "leadingBefore 2" },
        { { 1, 10.0f, 5.0f, INFINITY, 0 }, 0, "leadingBefore +inf" },
        { { 1, 10.0f, 5.0f, -INFINITY, 0 }, 0, "leadingBefore -inf" },
    };
    DWRITE_LINE_SPACING base = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 9.0f, 8.0f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_DISABLED }, out;
    size_t i;
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        HRESULT hr;
        CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(t2, &base));
        hr = IDWriteTextFormat2_SetLineSpacing(t2, &c[i].ls);
        CHECKF(hr == (c[i].ok ? S_OK : E_INVALIDARG), "%s -> %s", c[i].why, dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out));
        if (c[i].ok) { EXPECT_EQ(out.method, c[i].ls.method); CHECK_FBITS(out.height, c[i].ls.height); CHECK_FBITS(out.baseline, c[i].ls.baseline); CHECK_FBITS(out.leadingBefore, c[i].ls.leadingBefore); }
        else { EXPECT_EQ(out.method, base.method); CHECK_FBITS(out.height, base.height); CHECK_FBITS(out.baseline, base.baseline); CHECK_FBITS(out.leadingBefore, base.leadingBefore); EXPECT_EQ(out.fontLineGapUsage, base.fontLineGapUsage); }
    }
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_font_line_gap_usage_not_validated)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    static const UINT32 u[] = { 0, 1, 2, 3, 100, 0x7fffffffu, 0x80000000u, 0xffffffffu };
    size_t i;
    for (i = 0; i < sizeof u / sizeof u[0]; i++) {
        DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 10.0f, 5.0f, 0.5f, (DWRITE_FONT_LINE_GAP_USAGE)u[i] }, out;
        HRESULT hr = IDWriteTextFormat2_SetLineSpacing(t2, &in);
        CHECKF(hr == S_OK, "usage 0x%x -> %s", u[i], dw_hr_name(hr));
        CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out));
        EXPECT_EQ((UINT32)out.fontLineGapUsage, u[i]);
    }
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_leading_before_boundaries)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    static const float ok[] = { 0.0f, 1e-30f, 0.5f, 0.999999f, 1.0f }, bad[] = { 1.0000001f, 1.5f, 1e30f, -1e-30f, -1.0f };
    DWRITE_LINE_SPACING ls = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 10.0f, 5.0f, 0.0f, 0 };
    size_t i;
    for (i = 0; i < sizeof ok / sizeof ok[0]; i++) { ls.leadingBefore = ok[i]; CHECKF(IDWriteTextFormat2_SetLineSpacing(t2, &ls) == S_OK, "leadingBefore %g rejected", ok[i]); }
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) { ls.leadingBefore = bad[i]; CHECKF(IDWriteTextFormat2_SetLineSpacing(t2, &ls) == E_INVALIDARG, "leadingBefore %g accepted", bad[i]); }
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_through_format3_pointer)
{
    TF_NEW(t);
    TF_QI(t3, IDWriteTextFormat3, t); TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_PROPORTIONAL, 3.0f, 2.0f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, out;
    CHECK_HR_OK(TF3_SetLineSpacing(t3, &in));
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out));
    EXPECT_EQ(out.method, DWRITE_LINE_SPACING_METHOD_PROPORTIONAL); CHECK_FBITS(out.leadingBefore, 0.5f);
    CHECK_HR_OK(TF3_GetLineSpacing(t3, &out)); CHECK_FBITS(out.height, 3.0f);
    if (!t_is_wine()) CHECK_HR(TF3_SetLineSpacing(t3, NULL), E_INVALIDARG);
    DW_RELEASE(t3); DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_formats_independent)
{
    TF_NEED_FACTORY();
    IDWriteTextFormat *a = tf_new(f), *b = tf_new(f);
    TF_QI(a2, IDWriteTextFormat2, a); TF_QI(b2, IDWriteTextFormat2, b);
    DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 3.0f, 2.0f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, out;
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(a2, &in));
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(b2, &out));
    EXPECT_EQ(out.method, DWRITE_LINE_SPACING_METHOD_DEFAULT); CHECK_FBITS(out.leadingBefore, 0.0f) ;
    DW_RELEASE(a2); DW_RELEASE(b2); DW_RELEASE(a); DW_RELEASE(b);
}

TEST(linespacing2_struct_copied_not_referenced)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING in = { DWRITE_LINE_SPACING_METHOD_UNIFORM, 3.0f, 2.0f, 0.5f, DWRITE_FONT_LINE_GAP_USAGE_ENABLED }, out;
    CHECK_HR_OK(IDWriteTextFormat2_SetLineSpacing(t2, &in));
    in.height = 99.0f; in.method = DWRITE_LINE_SPACING_METHOD_DEFAULT;
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out));
    CHECK_FBITS(out.height, 3.0f); EXPECT_EQ(out.method, DWRITE_LINE_SPACING_METHOD_UNIFORM);
    DW_RELEASE(t2); DW_RELEASE(t);
}

TEST(linespacing2_get_overwrites_all_fields)
{
    TF_NEW(t);
    TF_QI(t2, IDWriteTextFormat2, t);
    DWRITE_LINE_SPACING out; unsigned char *p = (unsigned char *)&out; size_t i, same = 0;
    memset(&out, 0xA5, sizeof out);
    CHECK_HR_OK(IDWriteTextFormat2_GetLineSpacing(t2, &out));
    for (i = 0; i < sizeof out; i++) if (p[i] == 0xA5) same++;
    CHECKF(same == 0, "%u of %u output bytes untouched", (unsigned)same, (unsigned)sizeof out);
    DW_RELEASE(t2); DW_RELEASE(t);
}
