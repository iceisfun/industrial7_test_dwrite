/* DWRITE_FACTORY_TYPE validation and out-parameter protocol of DWriteCreateFactory.
 * The export zeroes *ppFactory first, then: type 1 builds a new IDWriteFactory, type 0 returns the process-wide one,
 * anything else throws an ArgumentException("factoryType") whose HRESULT is E_INVALIDARG (0x80070057). */
#include "fc.h"

TEST(type_shared_ok) {
    IUnknown *u = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, (void **)&u));
    CHECK(u != NULL && u != FC_SENTINEL);
    DW_RELEASE(u);
}
TEST(type_isolated_ok) {
    IUnknown *u = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory, (void **)&u));
    CHECK(u != NULL && u != FC_SENTINEL);
    DW_RELEASE(u);
}
TEST(type_enum_values) {
    EXPECT_EQ(DWRITE_FACTORY_TYPE_SHARED, 0);
    EXPECT_EQ(DWRITE_FACTORY_TYPE_ISOLATED, 1);
}
TEST(type_success_overwrites_sentinel) {
    void *p = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IUnknown, &p));
    CHECK(p != FC_SENTINEL && p != NULL);
    DW_RELEASE(p);
}

/* One test per invalid value: E_INVALIDARG, out pointer zeroed. */
#define BADTYPE(name, val) \
TEST(type_invalid_##name) { \
    void *p = FC_SENTINEL; HRESULT hr; NEED_EXPORT(); \
    hr = fc_create((int)(val), &IID_IDWriteFactory, &p); \
    CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(p); if (p && p != FC_SENTINEL) DW_RELEASE(p); \
}
BADTYPE(2, 2)
BADTYPE(3, 3)
BADTYPE(4, 4)
BADTYPE(5, 5)
BADTYPE(7, 7)
BADTYPE(8, 8)
BADTYPE(15, 15)
BADTYPE(16, 16)
BADTYPE(255, 255)
BADTYPE(256, 256)
BADTYPE(0x1000, 0x1000)
BADTYPE(0xFFFF, 0xFFFF)
BADTYPE(0x10000, 0x10000)
BADTYPE(0x7FFFFFFF, 0x7FFFFFFF)
BADTYPE(0x80000000, 0x80000000u)
BADTYPE(0x80000001, 0x80000001u)
BADTYPE(minus1, -1)
BADTYPE(minus2, -2)
BADTYPE(0xFFFFFFFE, 0xFFFFFFFEu)
BADTYPE(0x100, 0x100)
BADTYPE(0x40000000, 0x40000000)
BADTYPE(0x55555555, 0x55555555)
BADTYPE(0xAAAAAAAA, 0xAAAAAAAAu)
BADTYPE(0x10001, 0x00010001)
BADTYPE(0x7FFFFFFE, 0x7FFFFFFE)
BADTYPE(0x80000002, 0x80000002u)

TEST(type_invalid_with_unknown_iid_is_invalidarg) {
    /* The type is validated before the IID is looked at. */
    static const GUID rnd = { 0x12345678, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    void *p = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR(fc_create(2, &rnd, &p), E_INVALIDARG); CHECK_OUT_NULL(p);
    p = FC_SENTINEL;
    CHECK_HR(fc_create(-1, &fc_null_guid, &p), E_INVALIDARG); CHECK_OUT_NULL(p);
}
TEST(type_invalid_leaves_state_unchanged) {
    /* A rejected call must not disturb the shared factory. */
    void *a = NULL, *b = NULL, *bad = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IUnknown, &a));
    CHECK_HR(fc_create(9, &IID_IUnknown, &bad), E_INVALIDARG);
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IUnknown, &b));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(type_invalid_does_not_change_refcount) {
    void *a = NULL, *bad = FC_SENTINEL; ULONG r1, r2; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IUnknown, &a));
    if (!a) return;
    r1 = dw_refcount((IUnknown *)a);
    CHECK_HR(fc_create(3, &IID_IUnknown, &bad), E_INVALIDARG);
    r2 = dw_refcount((IUnknown *)a);
    CHECK(r1 == r2);
    DW_RELEASE(a);
}
TEST(type_invalid_repeated_is_stable) {
    int i; NEED_EXPORT();
    for (i = 0; i < 100; i++) {
        void *p = FC_SENTINEL;
        HRESULT hr = fc_create(2 + i, &IID_IDWriteFactory, &p);
        if (hr != E_INVALIDARG || p != NULL) { CHECKF(0, "iter %d hr=%s p=%p", i, dw_hr_name(hr), p); break; }
    }
    CHECK(dw_shared() != NULL);
}
TEST(type_invalid_sweep_small) {
    int bad = 0, v; NEED_EXPORT();
    for (v = 2; v < 2000; v++) {
        void *p = FC_SENTINEL;
        HRESULT hr = fc_create(v, &IID_IDWriteFactory, &p);
        if (hr != E_INVALIDARG || p != NULL) { bad++; if (p && p != FC_SENTINEL) DW_RELEASE(p); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(type_invalid_sweep_negative) {
    int bad = 0, v; NEED_EXPORT();
    for (v = -1; v > -2000; v--) {
        void *p = FC_SENTINEL;
        HRESULT hr = fc_create(v, &IID_IDWriteFactory, &p);
        if (hr != E_INVALIDARG || p != NULL) { bad++; if (p && p != FC_SENTINEL) DW_RELEASE(p); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(type_invalid_sweep_powers_of_two) {
    int bad = 0, b; NEED_EXPORT();
    for (b = 1; b < 32; b++) {
        void *p = FC_SENTINEL; HRESULT hr;
        hr = fc_create((int)(1u << b), &IID_IDWriteFactory, &p);
        if (hr != E_INVALIDARG || p != NULL) { bad++; if (p && p != FC_SENTINEL) DW_RELEASE(p); }
        p = FC_SENTINEL;
        hr = fc_create((int)((1u << b) | 1u), &IID_IDWriteFactory, &p);   /* ISOLATED bit set with others: still invalid */
        if (hr != E_INVALIDARG || p != NULL) { bad++; if (p && p != FC_SENTINEL) DW_RELEASE(p); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(type_invalid_with_every_known_iid) {
    int n = 0, i, bad = 0; const dw_qi_row *rows; NEED_EXPORT();
    rows = dw_iid_table(&n);
    for (i = 0; i < n; i++) {
        void *p = FC_SENTINEL;
        HRESULT hr = fc_create(2, rows[i].iid, &p);
        if (hr != E_INVALIDARG || p != NULL) { bad++; if (p && p != FC_SENTINEL) DW_RELEASE(p); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(type_out_pointer_zeroed_even_with_prior_value_shared_badiid) {
    static const GUID rnd = { 0xdeadbeef, 0x1234, 0x5678, { 9, 9, 9, 9, 9, 9, 9, 9 } };
    void *p = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR(fc_create(FT_SHARED, &rnd, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
}
TEST(type_out_pointer_zeroed_even_with_prior_value_isolated_badiid) {
    static const GUID rnd = { 0xdeadbeef, 0x1234, 0x5678, { 9, 9, 9, 9, 9, 9, 9, 8 } };
    void *p = FC_SENTINEL; NEED_EXPORT();
    CHECK_HR(fc_create(FT_ISOLATED, &rnd, &p), E_NOINTERFACE); CHECK_OUT_NULL(p);
}
TEST(type_out_pointer_prior_real_object_is_overwritten_not_released) {
    /* The export does not look at the previous value: a held reference in *out is overwritten and never released. */
    IUnknown *held = NULL, *p; ULONG before, after; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IUnknown, (void **)&held));
    if (!held) return;
    before = dw_refcount(held);
    p = held;
    CHECK_HR(fc_create(FT_ISOLATED, &IID_IUnknown, (void **)&p), S_OK);
    CHECK(p != held);
    after = dw_refcount(held);
    CHECK(before == after);
    DW_RELEASE(p); DW_RELEASE(held);
}
TEST(type_failure_with_prior_real_object_zeroes_without_release) {
    IUnknown *held = NULL, *p; ULONG before, after; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IUnknown, (void **)&held));
    if (!held) return;
    before = dw_refcount(held);
    p = held;
    CHECK_HR(fc_create(5, &IID_IUnknown, (void **)&p), E_INVALIDARG);
    CHECK_OUT_NULL(p);
    after = dw_refcount(held);
    CHECK(before == after);
    DW_RELEASE(held);
}
TEST(type_each_success_returns_s_ok_not_s_false) {
    void *p = NULL; HRESULT hr; NEED_EXPORT();
    hr = fc_create(FT_SHARED, &IID_IDWriteFactory, &p);
    CHECK(hr == S_OK); DW_RELEASE(p);
    hr = fc_create(FT_ISOLATED, &IID_IDWriteFactory, &p);
    CHECK(hr == S_OK); DW_RELEASE(p);
}
TEST(type_usable_after_invalid_type_call) {
    void *bad = FC_SENTINEL; IDWriteFactory *f = NULL; NEED_EXPORT();
    fc_create(77, &IID_IDWriteFactory, &bad);
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory, (void **)&f));
    if (f) { t_note("usable after invalid call: %d", fc_factory_usable(f)); DW_RELEASE(f); }
}
TEST(type_calling_convention_return_in_eax_only) {
    /* HRESULT is 32 bits: compare through an int-returning view of the same pointer. */
    typedef int (WINAPI *ifn)(int, REFIID, void **);
    ifn fn = (ifn)dw_proc(); void *p = NULL; int r; NEED_EXPORT();
    r = fn(0, &IID_IUnknown, &p);
    CHECK(r == 0); DW_RELEASE(p);
    r = fn(42, &IID_IUnknown, &p);
    CHECK((unsigned)r == 0x80070057u);
}
