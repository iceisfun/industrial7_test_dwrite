/* IDWriteFactory3/5/6/7::CreateFontSetBuilder.
 * Win10 decompilation: all four vtable entries (IDWriteFactory3 !0xd7410 returning IDWriteFontSetBuilder**, IDWriteFactory5/6 !0xd7400
 * returning IDWriteFontSetBuilder1**; the Factory7 slot is the Factory6 one) share one body: *out = NULL, operator new
 * DWriteFontSetBuilder(factory), AddRef, S_OK. Every call creates a NEW builder with a reference count of 1; builders are
 * independent of each other and never cached. The out pointer is not validated (NULL crashes). */
#include "g2.h"

/* the SDK macros expand to vtable members that are themselves macro names */
#undef IDWriteFactory5_CreateFontSetBuilder
#undef IDWriteFactory6_CreateFontSetBuilder
#undef IDWriteFactory7_CreateFontSetBuilder

typedef struct { IDWriteFactory *f; IDWriteFactory3 *f3; IDWriteFactory5 *f5; IDWriteFactory6 *f6; IDWriteFactory7 *f7; } bfx;
static int bf_open(bfx *x)
{
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("factory"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory3, (void **)&x->f3))) x->f3 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory5, (void **)&x->f5))) x->f5 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory6, (void **)&x->f6))) x->f6 = NULL;
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory7, (void **)&x->f7))) x->f7 = NULL;
    if (!x->f3) { DW_RELEASE(x->f); t_skip("IDWriteFactory3 unsupported"); return 0; }
    return 1;
}
static void bf_close(bfx *x) { DW_RELEASE(x->f7); DW_RELEASE(x->f6); DW_RELEASE(x->f5); DW_RELEASE(x->f3); DW_RELEASE(x->f); }

/* variant: 3, 5, 6, 7 -> IDWriteFontSetBuilder1* (the base builder pointer for variant 3 is converted by QI) */
static HRESULT make(bfx *x, int v, void **out)
{
    *out = G2_SENT;
    switch (v) {
    case 3: return IDWriteFactory3_CreateFontSetBuilder(x->f3, (IDWriteFontSetBuilder **)out);
    case 5: return x->f5 ? x->f5->lpVtbl->IDWriteFactory5_CreateFontSetBuilder(x->f5, (IDWriteFontSetBuilder1 **)out) : (*out = NULL, E_NOTIMPL);
    case 6: return x->f6 ? x->f6->lpVtbl->IDWriteFactory6_CreateFontSetBuilder(x->f6, (IDWriteFontSetBuilder2 **)out) : (*out = NULL, E_NOTIMPL);
    default: return x->f7 ? x->f7->lpVtbl->IDWriteFactory6_CreateFontSetBuilder((IDWriteFactory6 *)x->f7, (IDWriteFontSetBuilder2 **)out) : (*out = NULL, E_NOTIMPL);
    }
}
static const int variants[4] = { 3, 5, 6, 7 };

TEST(gap2_fsb_each_variant_succeeds_with_one_reference)
{
    bfx x; int i;
    if (!bf_open(&x)) return;
    for (i = 0; i < 4; i++) {
        void *b; HRESULT hr = make(&x, variants[i], &b);
        if (hr == E_NOTIMPL && !b) { t_note("Factory%d variant unavailable", variants[i]); continue; }
        CHECKF(hr == S_OK && b != NULL && b != G2_SENT, "Factory%d: %s", variants[i], dw_hr_name(hr));
        if (SUCCEEDED(hr) && b && b != G2_SENT) { EXPECT_REFS(b, 1); IUnknown_Release((IUnknown *)b); }
    }
    bf_close(&x);
}

TEST(gap2_fsb_every_call_creates_new_object)
{
    bfx x; void *seen[16]; int n = 0, i, j, k;
    if (!bf_open(&x)) return;
    for (i = 0; i < 4; i++) for (k = 0; k < 3; k++) {
        void *b; HRESULT hr = make(&x, variants[i], &b);
        if (SUCCEEDED(hr) && b && b != G2_SENT) {
            for (j = 0; j < n; j++) CHECKF(g2_ident(seen[j]) != g2_ident(b), "variant %d call %d returned an earlier builder", variants[i], k);
            seen[n++] = b;
        }
    }
    for (j = 0; j < n; j++) IUnknown_Release((IUnknown *)seen[j]);
    CHECK(n >= 3);
    bf_close(&x);
}

TEST(gap2_fsb_builders_are_independent)
{
    bfx x; uint8_t *blob; size_t n; dwx_mem_loader *ld; UINT32 key; IDWriteFontFile *file = NULL;
    IDWriteFontSetBuilder *a = NULL, *b = NULL; IDWriteFontSetBuilder1 *a1 = NULL; IDWriteFontSet *sa = NULL, *sb = NULL;
    if (!bf_open(&x)) return;
    ld = dwx_loader_new(); dwx_loader_register(ld, x.f);
    blob = ttf_build_default(&n); key = dwx_loader_add(ld, blob, n); free(blob);
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(x.f3, &a));
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(x.f3, &b));
    if (a && b && SUCCEEDED(dwx_loader_file(ld, x.f, key, &file)) && file && SUCCEEDED(IDWriteFontSetBuilder_QueryInterface(a, &IID_IDWriteFontSetBuilder1, (void **)&a1))) {
        CHECK_HR_OK(IDWriteFontSetBuilder1_AddFontFile(a1, file));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(a, &sa));
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &sb));
        if (sa) EXPECT_EQ(IDWriteFontSet_GetFontCount(sa), 1);
        if (sb) EXPECT_EQ(IDWriteFontSet_GetFontCount(sb), 0);
    } else t_skip("builder1/file unavailable");
    DW_RELEASE(sa); DW_RELEASE(sb); DW_RELEASE(a1); DW_RELEASE(file); DW_RELEASE(a); DW_RELEASE(b);
    dwx_loader_unregister(ld); dwx_release(ld);
    bf_close(&x);
}

TEST(gap2_fsb_empty_builder_yields_empty_set)
{
    bfx x; int i;
    if (!bf_open(&x)) return;
    for (i = 0; i < 4; i++) {
        void *b; HRESULT hr = make(&x, variants[i], &b);
        if (SUCCEEDED(hr) && b && b != G2_SENT) {
            IDWriteFontSetBuilder *base = NULL; IDWriteFontSet *s = G2_SENT;
            if (SUCCEEDED(IUnknown_QueryInterface((IUnknown *)b, &IID_IDWriteFontSetBuilder, (void **)&base))) {
                hr = IDWriteFontSetBuilder_CreateFontSet(base, &s);
                CHECKF(hr == S_OK && s != NULL && s != G2_SENT, "variant %d: %s", variants[i], dw_hr_name(hr));
                if (SUCCEEDED(hr) && s != G2_SENT) { EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 0); IDWriteFontSet_Release(s); }
                DW_RELEASE(base);
            }
            IUnknown_Release((IUnknown *)b);
        }
    }
    bf_close(&x);
}

TEST(gap2_fsb_qi_matrix)
{
    bfx x; IDWriteFontSetBuilder *b = NULL; IUnknown *u1 = NULL, *u2 = NULL; IDWriteFontSetBuilder1 *b1 = NULL;
    if (!bf_open(&x)) return;
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(x.f3, &b));
    if (b) {
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontSetBuilder, 1), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontFileLoader, 0), DW_QI_ROW(IDWriteFontSetBuilder1, -1) };
        DW_CHECK_QI(b, rows);
        IDWriteFontSetBuilder_QueryInterface(b, &IID_IUnknown, (void **)&u1);
        if (SUCCEEDED(IDWriteFontSetBuilder_QueryInterface(b, &IID_IDWriteFontSetBuilder1, (void **)&b1))) {
            IUnknown_QueryInterface((IUnknown *)b1, &IID_IUnknown, (void **)&u2);
            CHECK(u1 == u2);                                  /* one object implements both versions */
        }
    }
    DW_RELEASE(u1); DW_RELEASE(u2); DW_RELEASE(b1); DW_RELEASE(b);
    bf_close(&x);
}

TEST(gap2_fsb_outlives_factory_release)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory3 *f3 = NULL; IDWriteFontSetBuilder *b = NULL; IDWriteFontSet *s = NULL;
    if (!f) { t_skip("factory"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3))) { DW_RELEASE(f); t_skip("Factory3 unsupported"); return; }
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(f3, &b));
    DW_RELEASE(f3); DW_RELEASE(f);
    if (b) {
        CHECK_HR_OK(IDWriteFontSetBuilder_CreateFontSet(b, &s));
        if (s) EXPECT_EQ(IDWriteFontSet_GetFontCount(s), 0);
        DW_RELEASE(s); DW_RELEASE(b);
    }
}

TEST(gap2_fsb_shared_and_isolated_factories)
{
    int t;
    for (t = 0; t < 2; t++) {
        void *p = dw_factory_v(t ? DWRITE_FACTORY_TYPE_ISOLATED : DWRITE_FACTORY_TYPE_SHARED, 3);
        IDWriteFontSetBuilder *b = (IDWriteFontSetBuilder *)G2_SENT;
        if (!p) { t_skip("Factory3 unsupported"); return; }
        CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder((IDWriteFactory3 *)p, &b));
        CHECK(b != NULL && b != G2_SENT);
        if (b && b != G2_SENT) { EXPECT_REFS(b, 1); IDWriteFontSetBuilder_Release(b); }
    }
}

TEST(gap2_fsb_many_builders_no_leak)
{
    bfx x; int i; ULONG r0, r1;
    if (!bf_open(&x)) return;
    r0 = dw_refcount((IUnknown *)x.f);
    for (i = 0; i < 200; i++) {
        IDWriteFontSetBuilder *b = NULL;
        if (SUCCEEDED(IDWriteFactory3_CreateFontSetBuilder(x.f3, &b))) IDWriteFontSetBuilder_Release(b);
    }
    r1 = dw_refcount((IUnknown *)x.f);
    CHECKF(r0 == r1, "factory refs %lu -> %lu", r0, r1);
    bf_close(&x);
}

TEST(gap2_fsb_builder_holds_factory_alive_until_release)
{
    bfx x; IDWriteFontSetBuilder *b = NULL; ULONG before, during, after;
    if (!bf_open(&x)) return;
    before = dw_refcount((IUnknown *)x.f);
    CHECK_HR_OK(IDWriteFactory3_CreateFontSetBuilder(x.f3, &b));
    during = dw_refcount((IUnknown *)x.f);
    DW_RELEASE(b);
    after = dw_refcount((IUnknown *)x.f);
    t_note("factory refs before %lu during %lu after %lu", before, during, after);
    CHECK(after == before);
    bf_close(&x);
}

TEST(gap2_fsb_variants_have_matching_behaviour)
{
    bfx x; int i;
    if (!bf_open(&x)) return;
    for (i = 0; i < 4; i++) {
        void *b; HRESULT hr = make(&x, variants[i], &b);
        if (SUCCEEDED(hr) && b && b != G2_SENT) {
            IDWriteFontSetBuilder1 *b1 = NULL;
            hr = IUnknown_QueryInterface((IUnknown *)b, &IID_IDWriteFontSetBuilder1, (void **)&b1);
            t_note("variant %d object exposes IDWriteFontSetBuilder1: %s", variants[i], dw_hr_name(hr));
            if (variants[i] >= 5) CHECK_HR_OK(hr);                 /* those entry points hand out IDWriteFontSetBuilder1 (or newer) */
            DW_RELEASE(b1);
            IUnknown_Release((IUnknown *)b);
        }
    }
    bf_close(&x);
}
