/* The system font set: IDWriteFactory3::GetSystemFontSet and the IDWriteFontSet methods that read it.
 *
 * Win10 decompilation: DWriteFactory::GetSystemFontSet caches one DWriteFontSet per (includeDownloadable) flag under the
 * factory lock and hands out that same object with an AddRef; DWriteFontSet::GetFontFaceReference throws ArgumentException
 * ("listIndex", E_INVALIDARG) for index >= count; FindFontFaceReference/FindFontFace write index = 0xFFFFFFFF and exists = FALSE
 * first, reject NULL with E_INVALIDARG and otherwise search by FontFaceKey.
 */
#include "fs.h"

/* up to `max` indexes spread over [0,n): first, last and evenly spaced ones. */
static int fs_samples(UINT32 n, UINT32 *out, int max)
{
    int k = 0; UINT32 step, i;
    if (n == 0) return 0;
    step = n / (UINT32)(max > 2 ? max - 2 : 1); if (step == 0) step = 1;
    out[k++] = 0;
    for (i = step; i < n - 1 && k < max - 1; i += step) out[k++] = i;
    if (n > 1) out[k++] = n - 1;
    return k;
}

TEST(fset_sys_acquire)
{
    NEED_F3(f3);
    IDWriteFontSet *set = (IDWriteFontSet *)(ULONG_PTR)1;
    HRESULT hr = IDWriteFactory3_GetSystemFontSet(f3, &set);
    CHECK_HR_OK(hr);
    CHECK(set != NULL && set != (IDWriteFontSet *)(ULONG_PTR)1);
    FS_DROP(set, hr);
}

TEST(fset_sys_count_gt_100)
{
    NEED_SYSSET(set);
    /* A stock Windows 10 installation ships far more than 100 font faces. */
    UINT32 n = IDWriteFontSet_GetFontCount(set);
    CHECKF(n > 100, "count %u", n);
    t_note("system font set count: %u", n);
    DW_RELEASE(set);
}

TEST(fset_sys_cached_identity)
{
    NEED_F3(f3);
    IDWriteFontSet *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &a));
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &b));
    /* The factory keeps one DWriteFontSet for the system set and AddRefs it for every caller. */
    CHECKF(a == b, "%p vs %p", (void *)a, (void *)b);
    DW_RELEASE(a); DW_RELEASE(b);
}

TEST(fset_sys_refcount_balance)
{
    NEED_F3(f3);
    IDWriteFontSet *a = NULL, *b = NULL; ULONG r0, r1, r2;
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &a));
    r0 = dw_refcount((IUnknown *)a);
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &b));
    r1 = dw_refcount((IUnknown *)a);
    CHECKF(r1 == r0 + 1, "refs %lu -> %lu", r0, r1);
    IDWriteFontSet_Release(b); b = NULL;
    r2 = dw_refcount((IUnknown *)a);
    CHECKF(r2 == r0, "refs back %lu want %lu", r2, r0);
    IDWriteFontSet_AddRef(a); IDWriteFontSet_AddRef(a);
    EXPECT_REFS(a, r0 + 2);
    IDWriteFontSet_Release(a); IDWriteFontSet_Release(a);
    EXPECT_REFS(a, r0);
    DW_RELEASE(a);
}

TEST(fset_sys_qi_matrix)
{
    NEED_SYSSET(set);
    dw_qi_row rows[] = {
        DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontSet, 1), DW_QI_ROW(IDWriteFontSet1, 1), DW_QI_ROW(IDWriteFontSet2, 1),
        DW_QI_ROW(IDWriteFontSet3, -1), DW_QI_ROW(IDWriteFontSet4, -1),
        DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontCollection1, 0), DW_QI_ROW(IDWriteFactory, 0),
        DW_QI_ROW(IDWriteFontSetBuilder, 0), DW_QI_ROW(IDWriteStringList, 0), DW_QI_ROW(IDWriteLocalizedStrings, 0),
        DW_QI_ROW(IDWriteFontFaceReference, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFontList, 0),
    };
    DW_CHECK_QI(set, rows);
    DW_RELEASE(set);
}

TEST(fset_sys_qi_null_out_free)
{
    NEED_SYSSET(set);
    /* QI to an unsupported IID zeroes the out pointer. */
    void *p = (void *)(ULONG_PTR)0x1;
    HRESULT hr = IDWriteFontSet_QueryInterface(set, &IID_IDWriteFactory, &p);
    CHECK_HR(hr, E_NOINTERFACE);
    CHECK_OUT_NULL(p);
    DW_RELEASE(set);
}

TEST(fset_sys_f6_overloads)
{
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    IDWriteFontSet1 *s_off = NULL, *s_on = NULL; HRESULT hr;
    hr = IDWriteFactory6_GetSystemFontSet(f6, FALSE, &s_off);
    CHECK_HR_OK(hr);
    hr = IDWriteFactory6_GetSystemFontSet(f6, TRUE, &s_on);
    CHECK_HR_OK(hr);
    if (s_off && s_on) {
        UINT32 a = IDWriteFontSet1_GetFontCount(s_off), b = IDWriteFontSet1_GetFontCount(s_on);
        CHECKF(a > 100 && b > 100, "counts %u %u", a, b);
        t_note("system set count includeDownloadable FALSE=%u TRUE=%u", a, b);
        /* includeDownloadable=TRUE is the flavour IDWriteFactory3::GetSystemFontSet returns */
        IDWriteFontSet *s3 = NULL; IDWriteFactory3 *f3 = (IDWriteFactory3 *)f6;
        if (SUCCEEDED(IDWriteFactory3_GetSystemFontSet(f3, &s3))) {
            FS_SOFT(dw_same_identity((IUnknown *)s3, (IUnknown *)s_on));
            DW_RELEASE(s3);
        }
    }
    DW_RELEASE(s_off); DW_RELEASE(s_on);
}

TEST(fset_sys_f6_cached_per_flag)
{
    IDWriteFactory6 *f6 = (IDWriteFactory6 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 6);
    if (!f6) { t_skip("IDWriteFactory6 unsupported"); return; }
    IDWriteFontSet1 *a = NULL, *b = NULL, *c = NULL;
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, FALSE, &a));
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, FALSE, &b));
    CHECK_HR_OK(IDWriteFactory6_GetSystemFontSet(f6, TRUE, &c));
    CHECKF(a == b, "same flag, same object: %p %p", (void *)a, (void *)b);
    if (a && c && a != c) t_note("TRUE and FALSE flavours are distinct objects");
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c);
}

TEST(fset_sys_f7_set2)
{
    IDWriteFactory7 *f7 = (IDWriteFactory7 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 7);
    if (!f7) { t_skip("IDWriteFactory7 unsupported"); return; }
    IDWriteFontSet2 *s2 = NULL;
    HRESULT hr = IDWriteFactory7_GetSystemFontSet(f7, FALSE, &s2);
    CHECK_HR_OK(hr);
    if (s2) {
        CHECK(IDWriteFontSet2_GetFontCount(s2) > 100);
        DW_RELEASE(s2);
    }
}

TEST(fset_sys_ref_first_middle_last)
{
    NEED_SYSSET(set);
    UINT32 n = IDWriteFontSet_GetFontCount(set), idx[3], i;
    idx[0] = 0; idx[1] = n / 2; idx[2] = n - 1;
    for (i = 0; i < 3; i++) {
        IDWriteFontFaceReference *ref = NULL;
        HRESULT hr = IDWriteFontSet_GetFontFaceReference(set, idx[i], &ref);
        CHECK_HR_OK(hr);
        CHECKF(ref != NULL, "ref at %u", idx[i]);
        if (ref) {
            IDWriteFontFile *file = NULL;
            CHECK_HR_OK(IDWriteFontFaceReference_GetFontFile(ref, &file));
            CHECK(file != NULL);
            DW_RELEASE(file);
            DW_RELEASE(ref);
        }
    }
    DW_RELEASE(set);
}

TEST(fset_sys_ref_out_of_range)
{
    NEED_SYSSET(set);
    UINT32 n = IDWriteFontSet_GetFontCount(set);
    UINT32 bad[] = { n, n + 1, n + 1000, 0x7FFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFFu };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        IDWriteFontFaceReference *ref = (IDWriteFontFaceReference *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontSet_GetFontFaceReference(set, bad[i], &ref);
        /* ArgumentException("listIndex") carries E_INVALIDARG; the out parameter is zeroed first. */
        CHECKF(hr == E_INVALIDARG, "index %u -> %s", bad[i], dw_hr_name(hr));
        CHECK_OUT_NULL(ref);
        FS_DROP(ref, hr);
    }
    DW_RELEASE(set);
}

TEST(fset_sys_ref_distinct_objects)
{
    NEED_SYSSET(set);
    IDWriteFontFaceReference *a = NULL, *b = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(set, 0, &a));
    CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(set, 0, &b));
    /* A new reference object is allocated per call, but they compare equal. */
    CHECK(a != b);
    if (a && b) CHECK(IDWriteFontFaceReference_Equals(a, b));
    if (a) EXPECT_REFS(a, 1);
    DW_RELEASE(a); DW_RELEASE(b);
    DW_RELEASE(set);
}

TEST(fset_sys_ref_qi_identity)
{
    NEED_SYSSET(set);
    IDWriteFontFaceReference *r = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(set, 0, &r));
    if (r) {
        dw_qi_row rows[] = { DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontFaceReference, 1), DW_QI_ROW(IDWriteFontFaceReference1, -1),
                             DW_QI_ROW(IDWriteFontSet, 0), DW_QI_ROW(IDWriteFontFace, 0) };
        DW_CHECK_QI(r, rows);
        DW_RELEASE(r);
    }
    DW_RELEASE(set);
}

TEST(fset_sys_ref_outlives_set)
{
    NEED_SYSSET(set);
    IDWriteFontFaceReference *r = NULL;
    CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(set, 1, &r));
    DW_RELEASE(set);
    if (r) {
        IDWriteFontFile *file = NULL;
        CHECK_HR_OK(IDWriteFontFaceReference_GetFontFile(r, &file));
        CHECK(file != NULL);
        DW_RELEASE(file);
        DW_RELEASE(r);
    }
}

TEST(fset_sys_find_ref_roundtrip)
{
    NEED_SYSSET(set);
    UINT32 n = IDWriteFontSet_GetFontCount(set), s[40], k, i;
    k = (UINT32)fs_samples(n, s, 40);
    for (i = 0; i < k; i++) {
        IDWriteFontFaceReference *r = NULL; UINT32 idx = 0x12345; BOOL ex = 2;
        if (FAILED(IDWriteFontSet_GetFontFaceReference(set, s[i], &r)) || !r) { CHECKF(0, "get ref %u", s[i]); continue; }
        CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, r, &idx, &ex));
        CHECKF(ex == TRUE, "ref %u exists", s[i]);
        CHECKF(idx < n, "ref %u -> index %u", s[i], idx);
        if (ex && idx < n) {
            IDWriteFontFaceReference *r2 = NULL;
            if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(set, idx, &r2)) && r2) {
                CHECKF(IDWriteFontFaceReference_Equals(r, r2), "ref at %u equals found ref at %u", s[i], idx);
                DW_RELEASE(r2);
            }
            if (idx != s[i]) t_note("ref %u found at index %u", s[i], idx);
        }
        DW_RELEASE(r);
    }
    DW_RELEASE(set);
}

TEST(fset_sys_find_face_roundtrip)
{
    NEED_SYSSET(set);
    UINT32 n = IDWriteFontSet_GetFontCount(set), s[12], k, i;
    int tested = 0;
    k = (UINT32)fs_samples(n, s, 12);
    for (i = 0; i < k; i++) {
        IDWriteFontFaceReference *r = NULL; IDWriteFontFace3 *face = NULL; UINT32 idx = 0x12345; BOOL ex = 2;
        if (FAILED(IDWriteFontSet_GetFontFaceReference(set, s[i], &r)) || !r) continue;
        if (FAILED(IDWriteFontFaceReference_CreateFontFace(r, &face)) || !face) { DW_RELEASE(r); continue; }
        CHECK_HR_OK(IDWriteFontSet_FindFontFace(set, (IDWriteFontFace *)face, &idx, &ex));
        CHECKF(ex == TRUE, "face %u exists", s[i]);
        CHECKF(idx < n, "face %u -> index %u", s[i], idx);
        tested++;
        DW_RELEASE(face); DW_RELEASE(r);
    }
    if (!tested) t_skip("no face could be created");
    DW_RELEASE(set);
}

TEST(fset_sys_find_ref_null)
{
    NEED_SYSSET(set);
    UINT32 idx = 5; BOOL ex = TRUE;
    HRESULT hr = IDWriteFontSet_FindFontFaceReference(set, NULL, &idx, &ex);
    CHECK_HR(hr, E_INVALIDARG);
    /* index and exists are initialised before the NULL check. */
    CHECKF(idx == 0xFFFFFFFFu, "index %u", idx);
    CHECKF(ex == FALSE, "exists %d", ex);
    DW_RELEASE(set);
}

TEST(fset_sys_find_face_null)
{
    NEED_SYSSET(set);
    UINT32 idx = 5; BOOL ex = TRUE;
    HRESULT hr = IDWriteFontSet_FindFontFace(set, NULL, &idx, &ex);
    CHECK_HR(hr, E_INVALIDARG);
    CHECKF(idx == 0xFFFFFFFFu, "index %u", idx);
    CHECKF(ex == FALSE, "exists %d", ex);
    DW_RELEASE(set);
}

TEST(fset_sys_find_ref_not_found)
{
    NEED_SYSSET(set);
    fs_fx x;
    if (!fs_open(&x)) { DW_RELEASE(set); return; }
    UINT32 k = fs_add(&x, "NotInSystemSet", 400, 5, 0);
    IDWriteFontFaceReference *ref = fs_ref(&x, k);
    if (!ref) { t_skip("no reference"); fs_close(&x); DW_RELEASE(set); return; }
    UINT32 idx = 7; BOOL ex = TRUE;
    CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(set, ref, &idx, &ex));
    CHECK(ex == FALSE);
    CHECKF(idx == 0xFFFFFFFFu, "index %u", idx);
    DW_RELEASE(ref);
    fs_close(&x);
    DW_RELEASE(set);
}

TEST(fset_sys_find_face_not_found)
{
    NEED_SYSSET(set);
    dwx_font h;
    IDWriteFactory *f = dw_shared();
    if (FAILED(dwx_font_open_default(&h, f)) || !h.face) { t_skip("default synthetic font"); dwx_font_close(&h); DW_RELEASE(set); return; }
    UINT32 idx = 7; BOOL ex = TRUE;
    CHECK_HR_OK(IDWriteFontSet_FindFontFace(set, h.face, &idx, &ex));
    CHECK(ex == FALSE);
    CHECKF(idx == 0xFFFFFFFFu, "index %u", idx);
    dwx_font_close(&h);
    DW_RELEASE(set);
}

TEST(fset_sys_refs_pairwise_distinct)
{
    NEED_SYSSET(set);
    UINT32 n = IDWriteFontSet_GetFontCount(set), s[24], k, i, j;
    IDWriteFontFaceReference *r[24];
    k = (UINT32)fs_samples(n, s, 24);
    for (i = 0; i < k; i++) { r[i] = NULL; IDWriteFontSet_GetFontFaceReference(set, s[i], &r[i]); }
    for (i = 0; i < k; i++)
        for (j = i + 1; j < k; j++)
            if (r[i] && r[j]) CHECKF(!IDWriteFontFaceReference_Equals(r[i], r[j]), "refs %u and %u equal", s[i], s[j]);
    for (i = 0; i < k; i++) DW_RELEASE(r[i]);
    DW_RELEASE(set);
}

TEST(fset_sys_ref_file_fields)
{
    NEED_SYSSET(set);
    UINT32 n = IDWriteFontSet_GetFontCount(set), s[10], k, i;
    k = (UINT32)fs_samples(n, s, 10);
    for (i = 0; i < k; i++) {
        IDWriteFontFaceReference *r = NULL;
        if (FAILED(IDWriteFontSet_GetFontFaceReference(set, s[i], &r)) || !r) continue;
        UINT64 fsz = IDWriteFontFaceReference_GetFileSize(r);
        UINT32 fi = IDWriteFontFaceReference_GetFontFaceIndex(r);
        DWRITE_FONT_SIMULATIONS sim = IDWriteFontFaceReference_GetSimulations(r);
        CHECKF(fsz > 0, "file size at %u", s[i]);
        CHECKF(fi < 64, "face index %u", fi);
        CHECKF((sim & ~3u) == 0, "simulations 0x%x", sim);
        DW_RELEASE(r);
    }
    DW_RELEASE(set);
}

TEST(fset_sys_lifetime_after_factory_release)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteFactory3 *f3 = NULL; IDWriteFontSet *set = NULL;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) || !f3) { t_skip("no factory3"); DW_RELEASE(f); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &set));
    DW_RELEASE(f3); DW_RELEASE(f);
    if (set) {
        /* The set keeps the factory alive. */
        UINT32 n = IDWriteFontSet_GetFontCount(set);
        CHECK(n > 100);
        IDWriteFontFaceReference *r = NULL;
        CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(set, 0, &r));
        DW_RELEASE(r);
        IDWriteStringList *l = fs_props(set, DWRITE_FONT_PROPERTY_ID_WEIGHT);
        CHECK(l != NULL);
        DW_RELEASE(l);
        DW_RELEASE(set);
    }
}

TEST(fset_sys_isolated_factory_set)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteFactory3 *f3 = NULL; IDWriteFontSet *set = NULL;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) || !f3) { t_skip("no factory3"); DW_RELEASE(f); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &set));
    if (set) {
        UINT32 n = IDWriteFontSet_GetFontCount(set);
        CHECKF(n > 100, "count %u", n);
        DW_RELEASE(set);
    }
    DW_RELEASE(f3); DW_RELEASE(f);
}

TEST(fset_sys_two_factories_same_content)
{
    NEED_SYSSET(a);
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    IDWriteFactory3 *f3 = NULL; IDWriteFontSet *b = NULL;
    if (!f || FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) || !f3) { t_skip("no isolated factory3"); DW_RELEASE(f); DW_RELEASE(a); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &b));
    if (b) {
        CHECK(a != b);
        UINT32 na = IDWriteFontSet_GetFontCount(a), nb = IDWriteFontSet_GetFontCount(b);
        CHECKF(na == nb, "counts %u vs %u", na, nb);
        IDWriteFontFaceReference *r = NULL; UINT32 idx = 0; BOOL ex = FALSE;
        if (SUCCEEDED(IDWriteFontSet_GetFontFaceReference(a, 0, &r)) && r) {
            /* references are keyed by file and face, so they are valid across factories */
            CHECK_HR_OK(IDWriteFontSet_FindFontFaceReference(b, r, &idx, &ex));
            FS_SOFT(ex == TRUE);
            DW_RELEASE(r);
        }
        DW_RELEASE(b);
    }
    DW_RELEASE(f3); DW_RELEASE(f); DW_RELEASE(a);
}

TEST(fset_sys_font_source_fields)
{
    NEED_SYSSET(set);
    IDWriteFontSet3 *s3 = NULL;
    if (FAILED(IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet3, (void **)&s3)) || !s3) { t_skip("IDWriteFontSet3 not exposed"); DW_RELEASE(set); return; }
    UINT32 n = IDWriteFontSet3_GetFontCount(s3), s[8], k, i;
    k = (UINT32)fs_samples(n, s, 8);
    for (i = 0; i < k; i++) {
        DWRITE_FONT_SOURCE_TYPE t = IDWriteFontSet3_GetFontSourceType(s3, s[i]);
        CHECKF((int)t >= DWRITE_FONT_SOURCE_TYPE_UNKNOWN && (int)t <= DWRITE_FONT_SOURCE_TYPE_REMOTE_FONT_PROVIDER, "type %d", (int)t);
        UINT32 len = IDWriteFontSet3_GetFontSourceNameLength(s3, s[i]);
        WCHAR buf[512] = { 0 };
        if (len < 511) {
            CHECK_HR_OK(IDWriteFontSet3_GetFontSourceName(s3, s[i], buf, 512));
            CHECKF(wcslen(buf) == len, "name length %u vs %u", (unsigned)wcslen(buf), len);
        }
        FS_SOFT(t == DWRITE_FONT_SOURCE_TYPE_PER_MACHINE || t == DWRITE_FONT_SOURCE_TYPE_PER_USER || t == DWRITE_FONT_SOURCE_TYPE_APPX_PACKAGE);
    }
    DW_RELEASE(s3); DW_RELEASE(set);
}

TEST(fset_sys_font_source_name_buffer)
{
    NEED_SYSSET(set);
    IDWriteFontSet3 *s3 = NULL;
    if (FAILED(IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet3, (void **)&s3)) || !s3) { t_skip("IDWriteFontSet3 not exposed"); DW_RELEASE(set); return; }
    UINT32 len = IDWriteFontSet3_GetFontSourceNameLength(s3, 0);
    WCHAR buf[600];
    /* size 0 is rejected up front; a short buffer is truncated + terminated by StringCchCopyN. */
    CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, 0, buf, 0), FS_E_NOTSUFF);
    if (len > 0 && len < 590) {
        wmemset(buf, 0x2222, 600);
        HRESULT hr = IDWriteFontSet3_GetFontSourceName(s3, 0, buf, len);
        CHECK_HR(hr, FS_E_NOTSUFF);
        CHECK(buf[len - 1] == 0);
        wmemset(buf, 0x2222, 600);
        CHECK_HR_OK(IDWriteFontSet3_GetFontSourceName(s3, 0, buf, len + 1));
        CHECK(buf[len] == 0);
    }
    DW_RELEASE(s3); DW_RELEASE(set);
}

TEST(fset_sys_locality)
{
    NEED_SYSSET(set);
    IDWriteFontSet1 *s1 = fs_set1(set);
    if (!s1) { t_skip("no FontSet1"); DW_RELEASE(set); return; }
    UINT32 n = IDWriteFontSet1_GetFontCount(s1), s[6], k, i;
    k = (UINT32)fs_samples(n, s, 6);
    for (i = 0; i < k; i++) {
        DWRITE_LOCALITY loc = IDWriteFontSet1_GetFontLocality(s1, s[i]);
        CHECKF((int)loc >= DWRITE_LOCALITY_REMOTE && (int)loc <= DWRITE_LOCALITY_LOCAL, "locality %d at %u", (int)loc, s[i]);
        FS_SOFT(loc == DWRITE_LOCALITY_LOCAL);   /* installed fonts are local */
    }
    DW_RELEASE(s1); DW_RELEASE(set);
}

TEST(fset_sys_expiration_event)
{
    NEED_SYSSET(set);
    IDWriteFontSet2 *s2 = NULL;
    if (FAILED(IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet2, (void **)&s2)) || !s2) { t_skip("no FontSet2"); DW_RELEASE(set); return; }
    HANDLE h = IDWriteFontSet2_GetExpirationEvent(s2);
    if (h) {
        /* an unsignalled event unless fonts were installed meanwhile: poll without waiting */
        DWORD w = WaitForSingleObject(h, 0);
        CHECK(w == WAIT_TIMEOUT || w == WAIT_OBJECT_0);
        t_note("expiration event wait(0) = %lu", w);
    } else t_note("no expiration event for the system set");
    DW_RELEASE(s2); DW_RELEASE(set);
}

TEST(fset_sys_create_font_resource)
{
    NEED_SYSSET(set);
    IDWriteFontSet1 *s1 = fs_set1(set);
    if (!s1) { t_skip("no FontSet1"); DW_RELEASE(set); return; }
    UINT32 n = IDWriteFontSet1_GetFontCount(s1);
    IDWriteFontResource *res = NULL;
    HRESULT hr = IDWriteFontSet1_CreateFontResource(s1, 0, &res);
    CHECK_HR_OK(hr);
    CHECK(res != NULL);
    DW_RELEASE(res);
    res = (IDWriteFontResource *)(ULONG_PTR)1;
    hr = IDWriteFontSet1_CreateFontResource(s1, n, &res);
    CHECK_HR(hr, E_INVALIDARG);
    CHECK_OUT_NULL(res);
    FS_DROP(res, hr);
    IDWriteFontFace5 *face = (IDWriteFontFace5 *)(ULONG_PTR)1;
    hr = IDWriteFontSet1_CreateFontFace(s1, n, &face);
    CHECKF(FAILED(hr), "CreateFontFace(count) -> %s", dw_hr_name(hr));
    CHECK_OUT_NULL(face);
    FS_DROP(face, hr);
    DW_RELEASE(s1); DW_RELEASE(set);
}

TEST(fset_sys_create_font_face_samples)
{
    NEED_SYSSET(set);
    IDWriteFontSet1 *s1 = fs_set1(set);
    if (!s1) { t_skip("no FontSet1"); DW_RELEASE(set); return; }
    UINT32 n = IDWriteFontSet1_GetFontCount(s1), s[8], k, i;
    k = (UINT32)fs_samples(n, s, 8);
    for (i = 0; i < k; i++) {
        IDWriteFontFace5 *face = NULL;
        HRESULT hr = IDWriteFontSet1_CreateFontFace(s1, s[i], &face);
        CHECKF(SUCCEEDED(hr) && face, "CreateFontFace(%u) -> %s", s[i], dw_hr_name(hr));
        if (face) {
            UINT32 idx = 0; BOOL ex = FALSE;
            CHECK_HR_OK(IDWriteFontSet_FindFontFace(set, (IDWriteFontFace *)face, &idx, &ex));
            CHECK(ex == TRUE);
            DW_RELEASE(face);
        }
    }
    DW_RELEASE(s1); DW_RELEASE(set);
}

TEST(fset_sys_ref1_variant)
{
    NEED_SYSSET(set);
    IDWriteFontSet1 *s1 = fs_set1(set);
    if (!s1) { t_skip("no FontSet1"); DW_RELEASE(set); return; }
    IDWriteFontFaceReference1 *r1 = NULL;
    HRESULT hr = IDWriteFontSet1_GetFontFaceReference(s1, 0, &r1);
    CHECK_HR_OK(hr);
    CHECK(r1 != NULL);
    DW_RELEASE(r1);
    r1 = (IDWriteFontFaceReference1 *)(ULONG_PTR)1;
    hr = IDWriteFontSet1_GetFontFaceReference(s1, 0xFFFFFFFFu, &r1);
    CHECK_HR(hr, E_INVALIDARG);
    CHECK_OUT_NULL(r1);
    FS_DROP(r1, hr);
    DW_RELEASE(s1); DW_RELEASE(set);
}

TEST(fset_sys_collection_font_set_roundtrip)
{
    NEED_F3(f3);
    IDWriteFontCollection1 *c = NULL; IDWriteFontSet *fs = NULL, *sys = NULL;
    HRESULT hr = IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &c, FALSE);
    if (FAILED(hr) || !c) { t_skip("system collection1 unavailable: %s", dw_hr_name(hr)); return; }
    CHECK_HR_OK(IDWriteFontCollection1_GetFontSet(c, &fs));
    CHECK(fs != NULL);
    if (fs && SUCCEEDED(IDWriteFactory3_GetSystemFontSet(f3, &sys)) && sys) {
        UINT32 a = IDWriteFontSet_GetFontCount(fs), b = IDWriteFontSet_GetFontCount(sys);
        CHECKF(a > 100, "collection font set count %u", a);
        t_note("collection->GetFontSet count %u, GetSystemFontSet count %u", a, b);
    }
    DW_RELEASE(sys); DW_RELEASE(fs); DW_RELEASE(c);
}
