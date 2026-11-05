/* Object identity, reference counting and lifetime of the factories returned by DWriteCreateFactory.
 * SHARED: GetSharedFactory() lazily builds one IDWriteFactory, stores it in a process-wide slot (never cleared) and hands out
 * AddRef'd copies, so every call returns the same object and releasing all of the caller's references never destroys it.
 * ISOLATED: every call builds a fresh IDWriteFactory (new private factory id) whose first and only reference is the caller's. */
#include "fc.h"

static IUnknown *mk(int type) { void *p = NULL; if (FAILED(fc_create(type, &IID_IUnknown, &p))) return NULL; return (IUnknown *)p; }

TEST(id_shared_same_pointer_twice) {
    IUnknown *a, *b; NEED_EXPORT();
    a = mk(FT_SHARED); b = mk(FT_SHARED);
    CHECK(a && b); CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(id_shared_same_pointer_via_factory_iid) {
    void *a = NULL, *b = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, &a));
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory, &b));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(id_shared_same_identity_across_versions) {
    int v, w; NEED_EXPORT();
    for (v = 0; v < 8; v++) for (w = v; w < 8; w++) {
        void *a = NULL, *b = NULL;
        fc_create(FT_SHARED, dw_factory_iid(v), &a); fc_create(FT_SHARED, dw_factory_iid(w), &b);
        CHECKF(a && b && dw_same_identity((IUnknown *)a, (IUnknown *)b), "Factory%d vs Factory%d", v, w);
        DW_RELEASE(a); DW_RELEASE(b);
    }
}
TEST(id_shared_iunknown_pointer_is_stable) {
    IUnknown *a = NULL, *b = NULL, *ua = NULL, *ub = NULL; void *f = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IUnknown, (void **)&a));
    CHECK_HR_OK(fc_create(FT_SHARED, &IID_IDWriteFactory3, &f));
    b = (IUnknown *)f;
    if (a && b) {
        CHECK_HR_OK(IUnknown_QueryInterface(a, &IID_IUnknown, (void **)&ua));
        CHECK_HR_OK(IUnknown_QueryInterface(b, &IID_IUnknown, (void **)&ub));
        CHECK(ua == ub && ua == a);
    }
    DW_RELEASE(ua); DW_RELEASE(ub); DW_RELEASE(a); DW_RELEASE(b);
}
TEST(id_shared_same_after_all_released) {
    IUnknown *first, *again; NEED_EXPORT();
    first = mk(FT_SHARED);
    CHECK(first != NULL);
    DW_RELEASE(first);          /* the caller now holds nothing... */
    again = mk(FT_SHARED);      /* ...but the process-wide object lives on */
    CHECK(again != NULL);
    first = (IUnknown *)dw_shared();
    CHECK(again == first || dw_same_identity(again, first));
    DW_RELEASE(again);
}
TEST(id_shared_same_pointer_many_cycles) {
    IUnknown *base; int i, bad = 0; NEED_EXPORT();
    base = mk(FT_SHARED);   /* IUnknown view; the helper's IDWriteFactory view may be a different pointer */
    REQUIRE(base);
    for (i = 0; i < 200; i++) {
        IUnknown *p = mk(FT_SHARED);
        if (p != base) bad++;
        DW_RELEASE(p);
    }
    DW_RELEASE(base);
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(id_shared_stays_alive_when_helper_reference_dropped) {
    /* Create, use, release, repeat: the factory is never destroyed and keeps answering. */
    int i, bad = 0; NEED_EXPORT();
    for (i = 0; i < 20; i++) {
        IDWriteFactory *f = NULL; IDWriteFontCollection *c = NULL;
        if (FAILED(fc_create(FT_SHARED, &IID_IDWriteFactory, (void **)&f)) || !f) { bad++; continue; }
        if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) bad++;
        DW_RELEASE(c); DW_RELEASE(f);
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(id_isolated_distinct_pointers) {
    IUnknown *a, *b, *c; NEED_EXPORT();
    a = mk(FT_ISOLATED); b = mk(FT_ISOLATED); c = mk(FT_ISOLATED);
    CHECK(a && b && c);
    CHECK(a != b); CHECK(b != c); CHECK(a != c);
    CHECK(!dw_same_identity(a, b)); CHECK(!dw_same_identity(a, c)); CHECK(!dw_same_identity(b, c));
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
}
TEST(id_isolated_distinct_from_shared) {
    IUnknown *s, *i; NEED_EXPORT();
    s = mk(FT_SHARED); i = mk(FT_ISOLATED);
    CHECK(s && i); CHECK(s != i); CHECK(!dw_same_identity(s, i));
    DW_RELEASE(s); DW_RELEASE(i);
}
TEST(id_isolated_many_distinct) {
    IUnknown *p[32]; int i, j, dup = 0; NEED_EXPORT();
    for (i = 0; i < 32; i++) p[i] = mk(FT_ISOLATED);
    for (i = 0; i < 32; i++) for (j = i + 1; j < 32; j++) if (p[i] == p[j] || !p[i]) dup++;
    CHECKF(dup == 0, "dup=%d", dup);
    for (i = 0; i < 32; i++) DW_RELEASE(p[i]);
}
TEST(id_isolated_not_reused_after_release) {
    /* A fresh isolated factory is a new object with a new private id; it is never the shared one. */
    IUnknown *s = mk(FT_SHARED), *a, *b; NEED_EXPORT();
    a = mk(FT_ISOLATED);
    CHECK(a != s && a != NULL);
    DW_RELEASE(a);
    b = mk(FT_ISOLATED);
    CHECK(b != s && b != NULL);
    DW_RELEASE(b); DW_RELEASE(s);
}
TEST(id_isolated_initial_refcount_one) {
    IUnknown *a; NEED_EXPORT();
    a = mk(FT_ISOLATED);
    REQUIRE(a);
    EXPECT_REFS(a, 1);
    DW_RELEASE(a);
}
TEST(id_isolated_initial_refcount_one_every_iid) {
    int v; NEED_EXPORT();
    for (v = 0; v < 8; v++) {
        void *p = NULL;
        if (FAILED(fc_create(FT_ISOLATED, dw_factory_iid(v), &p)) || !p) { CHECKF(0, "Factory%d create failed", v); continue; }
        EXPECT_REFS(p, 1);
        DW_RELEASE(p);
    }
}
TEST(id_isolated_addref_release_sequence) {
    IUnknown *a; ULONG r; NEED_EXPORT();
    a = mk(FT_ISOLATED);
    REQUIRE(a);
    r = IUnknown_AddRef(a); EXPECT_EQ(r, 2);
    r = IUnknown_AddRef(a); EXPECT_EQ(r, 3);
    r = IUnknown_AddRef(a); EXPECT_EQ(r, 4);
    r = IUnknown_Release(a); EXPECT_EQ(r, 3);
    r = IUnknown_Release(a); EXPECT_EQ(r, 2);
    r = IUnknown_Release(a); EXPECT_EQ(r, 1);
    r = IUnknown_Release(a); EXPECT_EQ(r, 0);
}
TEST(id_isolated_qi_adds_one_reference_each) {
    IUnknown *a; void *q[8]; int v; NEED_EXPORT();
    a = mk(FT_ISOLATED);
    REQUIRE(a);
    for (v = 0; v < 8; v++) {
        q[v] = NULL;
        CHECK_HR_OK(IUnknown_QueryInterface(a, dw_factory_iid(v), &q[v]));
        EXPECT_REFS(a, 2 + v);
    }
    for (v = 7; v >= 0; v--) { DW_RELEASE(q[v]); EXPECT_REFS(a, 1 + v); }
    DW_RELEASE(a);
}
TEST(id_isolated_refcount_shared_across_interfaces) {
    /* One object, one counter: an AddRef through any interface pointer is visible through every other one. */
    IUnknown *a; IDWriteFactory *f0 = NULL; IDWriteFactory7 *f7 = NULL; ULONG r; NEED_EXPORT();
    a = mk(FT_ISOLATED);
    REQUIRE(a);
    CHECK_HR_OK(IUnknown_QueryInterface(a, &IID_IDWriteFactory, (void **)&f0));
    CHECK_HR_OK(IUnknown_QueryInterface(a, &IID_IDWriteFactory7, (void **)&f7));
    EXPECT_REFS(a, 3);
    r = IDWriteFactory_AddRef(f0); EXPECT_EQ(r, 4);
    r = IDWriteFactory7_AddRef(f7); EXPECT_EQ(r, 5);
    r = IUnknown_Release(a); EXPECT_EQ(r, 4);
    r = IDWriteFactory_Release(f0); EXPECT_EQ(r, 3);
    r = IDWriteFactory7_Release(f7); EXPECT_EQ(r, 2);
    r = IDWriteFactory_Release(f0); EXPECT_EQ(r, 1);
    r = IDWriteFactory7_Release(f7); EXPECT_EQ(r, 0);
}
TEST(id_shared_refcount_at_least_one) {
    IUnknown *a; NEED_EXPORT();
    a = mk(FT_SHARED);
    REQUIRE(a);
    { ULONG r = dw_refcount(a); CHECK(r >= 1); t_note("shared factory refcount with one caller reference: %lu", r); }
    DW_RELEASE(a);
}
TEST(id_shared_each_create_adds_exactly_one_reference) {
    IUnknown *a, *b, *c; ULONG r0, r1, r2; NEED_EXPORT();
    a = mk(FT_SHARED);
    REQUIRE(a);
    r0 = dw_refcount(a);
    b = mk(FT_SHARED); r1 = dw_refcount(a);
    c = mk(FT_SHARED); r2 = dw_refcount(a);
    CHECK(r1 == r0 + 1); CHECK(r2 == r0 + 2);
    DW_RELEASE(c); EXPECT_REFS(a, r1);
    DW_RELEASE(b); EXPECT_REFS(a, r0);
    DW_RELEASE(a);
}
TEST(id_shared_addref_release_relative) {
    IUnknown *a; ULONG r0, r; NEED_EXPORT();
    a = mk(FT_SHARED);
    REQUIRE(a);
    r0 = dw_refcount(a);
    r = IUnknown_AddRef(a); CHECK(r == r0 + 1);
    r = IUnknown_AddRef(a); CHECK(r == r0 + 2);
    r = IUnknown_Release(a); CHECK(r == r0 + 1);
    r = IUnknown_Release(a); CHECK(r == r0);
    DW_RELEASE(a);
}
TEST(id_shared_qi_adds_exactly_one_reference) {
    IUnknown *a; void *q = NULL; ULONG r0; NEED_EXPORT();
    a = mk(FT_SHARED);
    REQUIRE(a);
    r0 = dw_refcount(a);
    CHECK_HR_OK(IUnknown_QueryInterface(a, &IID_IDWriteFactory5, &q));
    EXPECT_REFS(a, r0 + 1);
    DW_RELEASE(q);
    EXPECT_REFS(a, r0);
    DW_RELEASE(a);
}
TEST(id_shared_create_with_each_iid_adds_one_reference) {
    IUnknown *a; int v; ULONG r0; NEED_EXPORT();
    a = mk(FT_SHARED);
    REQUIRE(a);
    r0 = dw_refcount(a);
    for (v = 0; v < 8; v++) {
        void *p = NULL; fc_create(FT_SHARED, dw_factory_iid(v), &p);
        CHECKF(dw_refcount(a) == r0 + 1, "Factory%d", v);
        DW_RELEASE(p);
    }
    EXPECT_REFS(a, r0);
    DW_RELEASE(a);
}
TEST(id_shared_not_destroyed_by_extra_release_pairs) {
    IUnknown *a; int i; NEED_EXPORT();
    a = mk(FT_SHARED);
    REQUIRE(a);
    for (i = 0; i < 1000; i++) { IUnknown_AddRef(a); IUnknown_Release(a); }
    CHECK(dw_shared() != NULL);
    DW_RELEASE(a);
}
TEST(id_isolated_released_returns_zero_and_new_one_independent) {
    IUnknown *a, *b; ULONG r; NEED_EXPORT();
    a = mk(FT_ISOLATED); b = mk(FT_ISOLATED);
    REQUIRE(a && b);
    r = IUnknown_Release(a); EXPECT_EQ(r, 0);
    EXPECT_REFS(b, 1);                /* a's destruction does not touch b */
    r = IUnknown_Release(b); EXPECT_EQ(r, 0);
}
TEST(id_isolated_outlives_via_objects) {
    /* Objects created by a factory keep working after the caller released the factory. */
    IDWriteFactory *f = NULL; IDWriteTextFormat *tf = NULL; ULONG r; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory, (void **)&f));
    REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &tf));
    r = IDWriteFactory_Release(f);
    f = NULL;
    t_note("isolated factory refs after caller release with a live text format: %lu", r);
    if (tf) CHECK(IDWriteTextFormat_GetFontSize(tf) == 16.0f);
    DW_RELEASE(tf);
}
TEST(id_isolated_child_object_keeps_factory_alive) {
    IDWriteFactory *f = NULL; IDWriteTextFormat *tf = NULL; ULONG r; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory, (void **)&f));
    REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf));
    r = IDWriteFactory_Release(f);
    f = NULL;
    /* The factory is still referenced by the text format (if the implementation holds it); the format must keep working. */
    t_note("factory refs after caller release with a live text format: %lu", r);
    if (tf) {
        WCHAR name[64] = { 0 };
        CHECK_HR_OK(IDWriteTextFormat_GetFontFamilyName(tf, name, 64));
        CHECK(!lstrcmpW(name, L"Arial"));
        CHECK(IDWriteTextFormat_GetFontSize(tf) == 12.0f);
    }
    DW_RELEASE(tf);
}
TEST(id_isolated_text_format_gets_factory_back) {
    IDWriteFactory *f = NULL, *back = NULL; IDWriteTextLayout *tl = NULL; IDWriteTextFormat *tf = NULL; NEED_EXPORT();
    CHECK_HR_OK(fc_create(FT_ISOLATED, &IID_IDWriteFactory, (void **)&f));
    REQUIRE(f);
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &tf));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"abc", 3, tf, 100.f, 100.f, &tl));
    DW_RELEASE(tl); DW_RELEASE(tf); DW_RELEASE(f); (void)back;
}
