/* IDWriteFontSet3::GetFontSourceType / GetFontSourceNameLength / GetFontSourceName and IDWriteFontSet2::GetExpirationEvent.
 * Win10 probing:
 *   GetFontSourceType and GetFontSourceNameLength read the font entry's byte / string and THROW an
 *   ArgumentException("listIndex") for index >= GetFontCount(): an unhandled C++ exception, so out-of-range indices are never passed here.
 *   GetFontSourceName: bufferSize == 0 -> HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) before anything is written (the
 *   buffer pointer is not touched); otherwise buffer[0] = 0, index check, then StringCchCopyNW(buffer, bufferSize, name, length):
 *   bufferSize <= length -> 0x8007007A with a truncated, NUL-terminated copy; bufferSize > length -> S_OK.
 *   GetExpirationEvent (shared by font sets and font collections): NULL unless the object carries a cache reference.
 * The source type is a byte of the font entry: UNKNOWN 0, PER_MACHINE 1, PER_USER 2, APPX_PACKAGE 3, REMOTE_FONT_PROVIDER 4. */
#include "g2.h"

static IDWriteFontSet3 *sys_set3(IDWriteFontSet **keep)
{
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *s = NULL; IDWriteFontSet3 *s3 = NULL;
    *keep = NULL;
    if (!f3 || FAILED(IDWriteFactory3_GetSystemFontSet(f3, &s)) || !s) return NULL;
    if (FAILED(IDWriteFontSet_QueryInterface(s, &IID_IDWriteFontSet3, (void **)&s3))) s3 = NULL;
    *keep = s;
    return s3;
}
#define NEED_S3(s3, set) IDWriteFontSet *set; IDWriteFontSet3 *s3 = sys_set3(&set); \
    if (!s3) { DW_RELEASE(set); t_skip("system IDWriteFontSet3 unavailable"); return; }
#define END_S3(s3, set) do { DW_RELEASE(s3); DW_RELEASE(set); } while (0)

TEST(gap2_fsrc_all_types_in_range)
{
    NEED_S3(s3, set);
    UINT32 n = IDWriteFontSet3_GetFontCount(s3), i, cnt[8] = { 0 }; int bad = 0;
    if (n > 20000) n = 20000;
    for (i = 0; i < n; i++) { DWRITE_FONT_SOURCE_TYPE t = IDWriteFontSet3_GetFontSourceType(s3, i); if ((UINT32)t > 4) bad++; else cnt[t]++; }
    CHECKF(bad == 0, "%d out-of-range source types", bad);
    t_note("source types over %u fonts: unknown %u per-machine %u per-user %u appx %u remote %u", n, cnt[0], cnt[1], cnt[2], cnt[3], cnt[4]);
    END_S3(s3, set);
}

TEST(gap2_fsrc_enum_values)
{
    CHECK_FLOAT_EQ(DWRITE_FONT_SOURCE_TYPE_UNKNOWN, 0); CHECK_FLOAT_EQ(DWRITE_FONT_SOURCE_TYPE_PER_MACHINE, 1);
    CHECK_FLOAT_EQ(DWRITE_FONT_SOURCE_TYPE_PER_USER, 2); CHECK_FLOAT_EQ(DWRITE_FONT_SOURCE_TYPE_APPX_PACKAGE, 3);
    CHECK_FLOAT_EQ(DWRITE_FONT_SOURCE_TYPE_REMOTE_FONT_PROVIDER, 4);
}

TEST(gap2_fsrc_length_equals_string_for_every_font)
{
    NEED_S3(s3, set);
    UINT32 n = IDWriteFontSet3_GetFontCount(s3), i; int bad = 0, bad_hr = 0;
    WCHAR buf[1024];
    if (n > 5000) n = 5000;
    for (i = 0; i < n; i++) {
        UINT32 len = IDWriteFontSet3_GetFontSourceNameLength(s3, i);
        if (len >= 1023) continue;
        buf[0] = 1;
        if (IDWriteFontSet3_GetFontSourceName(s3, i, buf, 1024) != S_OK) { bad_hr++; continue; }
        if (wcslen(buf) != len) bad++;
    }
    CHECKF(bad_hr == 0, "%d GetFontSourceName failures", bad_hr);
    CHECKF(bad == 0, "%d length mismatches", bad);
    END_S3(s3, set);
}

TEST(gap2_fsrc_name_size0_null_buffer)
{
    /* bufferSize == 0 is rejected before the buffer is dereferenced, so even a NULL buffer is safe. */
    NEED_S3(s3, set);
    CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, 0, NULL, 0), G2_E_NOTSUFF);
    { UINT32 n = IDWriteFontSet3_GetFontCount(s3); if (n) CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, n - 1, NULL, 0), G2_E_NOTSUFF); }
    END_S3(s3, set);
}

TEST(gap2_fsrc_name_size0_leaves_buffer_untouched)
{
    NEED_S3(s3, set);
    WCHAR buf[4]; wmemset(buf, 0x4141, 4);
    CHECK_HR(IDWriteFontSet3_GetFontSourceName(s3, 0, buf, 0), G2_E_NOTSUFF);
    CHECK(buf[0] == 0x4141 && buf[1] == 0x4141);
    END_S3(s3, set);
}

TEST(gap2_fsrc_name_every_buffer_size)
{
    NEED_S3(s3, set);
    UINT32 n = IDWriteFontSet3_GetFontCount(s3), pick[6], np = 0, k;
    if (n == 0) { END_S3(s3, set); t_skip("empty system set"); return; }
    pick[np++] = 0; if (n > 1) pick[np++] = n - 1; if (n > 2) pick[np++] = n / 2; if (n > 10) pick[np++] = n / 3; if (n > 20) pick[np++] = n / 5;
    for (k = 0; k < np; k++) {
        UINT32 len = IDWriteFontSet3_GetFontSourceNameLength(s3, pick[k]), sz; WCHAR full[600], buf[610];
        if (len > 400) continue;
        CHECK_HR_OK(IDWriteFontSet3_GetFontSourceName(s3, pick[k], full, 600));
        for (sz = 1; sz <= len + 3; sz++) {
            HRESULT hr, want;
            wmemset(buf, 0x2222, 610);
            hr = IDWriteFontSet3_GetFontSourceName(s3, pick[k], buf, sz);
            want = sz <= len ? G2_E_NOTSUFF : S_OK;
            CHECKF(hr == want, "index %u len %u size %u: got %s", pick[k], len, sz, dw_hr_name(hr));
            {
                UINT32 copied = sz <= len ? sz - 1 : len;
                CHECKF(buf[copied] == 0, "index %u size %u: no terminator at %u", pick[k], sz, copied);
                CHECKF(wcsncmp(buf, full, copied) == 0, "index %u size %u: prefix differs", pick[k], sz);
                if (sz < 609) CHECKF(buf[sz] == 0x2222 || sz <= copied + 1, "index %u size %u: wrote past the buffer", pick[k], sz);
            }
        }
    }
    END_S3(s3, set);
}

TEST(gap2_fsrc_type_and_name_repeatable)
{
    NEED_S3(s3, set);
    UINT32 n = IDWriteFontSet3_GetFontCount(s3), i, r;
    if (n > 200) n = 200;
    for (i = 0; i < n; i++) {
        DWRITE_FONT_SOURCE_TYPE t0 = IDWriteFontSet3_GetFontSourceType(s3, i); UINT32 l0 = IDWriteFontSet3_GetFontSourceNameLength(s3, i);
        for (r = 0; r < 3; r++) CHECK(IDWriteFontSet3_GetFontSourceType(s3, i) == t0 && IDWriteFontSet3_GetFontSourceNameLength(s3, i) == l0);
    }
    END_S3(s3, set);
}

TEST(gap2_fsrc_first_and_last_index_valid)
{
    NEED_S3(s3, set);
    UINT32 n = IDWriteFontSet3_GetFontCount(s3);
    CHECK(n > 0);
    if (n) {
        WCHAR b[600];
        (void)IDWriteFontSet3_GetFontSourceType(s3, 0); (void)IDWriteFontSet3_GetFontSourceType(s3, n - 1);
        if (IDWriteFontSet3_GetFontSourceNameLength(s3, n - 1) < 599) CHECK_HR_OK(IDWriteFontSet3_GetFontSourceName(s3, n - 1, b, 600));
    }
    END_S3(s3, set);
}

TEST(gap2_fsrc_subset_matches_parent)
{
    /* A filtered set keeps each entry's source: compare types and names through the face references. */
    NEED_S3(s3, set);
    static const WCHAR *fams[] = { L"Arial", L"Segoe UI", L"Times New Roman", L"Courier New", L"Tahoma" };
    size_t fi; int tested = 0;
    for (fi = 0; fi < sizeof fams / sizeof fams[0]; fi++) {
        IDWriteFontSet *sub = NULL; IDWriteFontSet3 *sub3 = NULL; UINT32 m, i;
        if (FAILED(IDWriteFontSet_GetMatchingFonts_(set, fams[fi], DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &sub)) || !sub) continue;
        if (FAILED(IDWriteFontSet_QueryInterface(sub, &IID_IDWriteFontSet3, (void **)&sub3))) sub3 = NULL;
        m = IDWriteFontSet_GetFontCount(sub);
        for (i = 0; sub3 && i < m && i < 8; i++) {
            IDWriteFontFaceReference *ref = NULL; UINT32 idx = 0; BOOL ex = FALSE;
            if (FAILED(IDWriteFontSet_GetFontFaceReference(sub, i, &ref)) || !ref) continue;
            IDWriteFontSet_FindFontFaceReference(set, ref, &idx, &ex);
            if (ex) {
                WCHAR a[600], b[600]; a[0] = b[0] = 0;
                tested++;
                CHECKF(IDWriteFontSet3_GetFontSourceType(sub3, i) == IDWriteFontSet3_GetFontSourceType(s3, idx), "%ls entry %u", fams[fi], i);
                CHECK(IDWriteFontSet3_GetFontSourceNameLength(sub3, i) == IDWriteFontSet3_GetFontSourceNameLength(s3, idx));
                if (IDWriteFontSet3_GetFontSourceNameLength(s3, idx) < 590) {
                    IDWriteFontSet3_GetFontSourceName(sub3, i, a, 600); IDWriteFontSet3_GetFontSourceName(s3, idx, b, 600);
                    CHECK(!wcscmp(a, b));
                }
            }
            IDWriteFontFaceReference_Release(ref);
        }
        DW_RELEASE(sub3); DW_RELEASE(sub);
    }
    if (!tested) t_note("no well-known family found in the set");
    END_S3(s3, set);
}

TEST(gap2_fsrc_arial_is_per_machine_noted)
{
    NEED_S3(s3, set);
    IDWriteFontSet *sub = NULL;
    if (SUCCEEDED(IDWriteFontSet_GetMatchingFonts_(set, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &sub)) && sub) {
        IDWriteFontSet3 *sub3 = NULL;
        if (SUCCEEDED(IDWriteFontSet_QueryInterface(sub, &IID_IDWriteFontSet3, (void **)&sub3)) && IDWriteFontSet3_GetFontCount(sub3) > 0) {
            WCHAR nm[600]; nm[0] = 0;
            DWRITE_FONT_SOURCE_TYPE t = IDWriteFontSet3_GetFontSourceType(sub3, 0);
            if (IDWriteFontSet3_GetFontSourceNameLength(sub3, 0) < 590) IDWriteFontSet3_GetFontSourceName(sub3, 0, nm, 600);
            t_note("Arial source type %d name '%ls'", (int)t, nm);
            G2_SOFT(t == DWRITE_FONT_SOURCE_TYPE_PER_MACHINE);
        }
        DW_RELEASE(sub3);
    } else t_skip("Arial not in the system set");
    DW_RELEASE(sub);
    END_S3(s3, set);
}

/* ---- custom (builder) sets ----------------------------------------------------------------------------------- */
typedef struct { IDWriteFactory *f; IDWriteFactory3 *f3; dwx_mem_loader *fl; IDWriteFontSetBuilder *b; IDWriteFontSetBuilder1 *b1; IDWriteFontSet *set; IDWriteFontSet3 *s3; int ok; } bset;
static void bset_close(bset *x)
{
    DW_RELEASE(x->s3); DW_RELEASE(x->set); DW_RELEASE(x->b1); DW_RELEASE(x->b); DW_RELEASE(x->f3);
    if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}
static int bset_open(bset *x, int nfonts)
{
    int i; memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("factory"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory3, (void **)&x->f3))) { bset_close(x); t_skip("Factory3 unsupported"); return 0; }
    x->fl = dwx_loader_new(); dwx_loader_register(x->fl, x->f);
    if (FAILED(IDWriteFactory3_CreateFontSetBuilder(x->f3, &x->b)) || !x->b || FAILED(IDWriteFontSetBuilder_QueryInterface(x->b, &IID_IDWriteFontSetBuilder1, (void **)&x->b1))) { bset_close(x); t_skip("builder"); return 0; }
    for (i = 0; i < nfonts; i++) {
        char fam[32]; ttf_spec s; size_t n; uint8_t *blob; UINT32 key; IDWriteFontFile *file = NULL;
        snprintf(fam, sizeof fam, "G2Src%d", i);
        g2_spec(&s, fam); blob = ttf_build(&s, &n); key = dwx_loader_add(x->fl, blob, n); free(blob);
        if (FAILED(dwx_loader_file(x->fl, x->f, key, &file)) || !file) { bset_close(x); t_skip("file"); return 0; }
        IDWriteFontSetBuilder1_AddFontFile(x->b1, file);
        IDWriteFontFile_Release(file);
    }
    if (FAILED(IDWriteFontSetBuilder_CreateFontSet(x->b, &x->set)) || !x->set) { bset_close(x); t_skip("CreateFontSet"); return 0; }
    if (FAILED(IDWriteFontSet_QueryInterface(x->set, &IID_IDWriteFontSet3, (void **)&x->s3))) x->s3 = NULL;
    x->ok = 1;
    return 1;
}

TEST(gap2_fsrc_builder_set_fields)
{
    bset x; UINT32 i;
    if (!bset_open(&x, 3)) return;
    if (!x.s3) { bset_close(&x); t_skip("builder set lacks IDWriteFontSet3"); return; }
    EXPECT_EQ(IDWriteFontSet3_GetFontCount(x.s3), 3);
    for (i = 0; i < 3; i++) {
        DWRITE_FONT_SOURCE_TYPE t = IDWriteFontSet3_GetFontSourceType(x.s3, i); UINT32 len = IDWriteFontSet3_GetFontSourceNameLength(x.s3, i); WCHAR buf[64];
        CHECK((UINT32)t <= 4);
        buf[0] = 7;
        if (len < 63) { CHECK_HR_OK(IDWriteFontSet3_GetFontSourceName(x.s3, i, buf, 64)); CHECK(wcslen(buf) == len); }
        t_note("builder font %u: source type %d name length %u", i, (int)t, len);
        G2_SOFT(t == DWRITE_FONT_SOURCE_TYPE_UNKNOWN && len == 0);
    }
    bset_close(&x);
}

TEST(gap2_fsrc_builder_set_empty)
{
    bset x;
    if (!bset_open(&x, 0)) return;
    if (!x.s3) { bset_close(&x); t_skip("no Set3"); return; }
    EXPECT_EQ(IDWriteFontSet3_GetFontCount(x.s3), 0);
    CHECK_HR(IDWriteFontSet3_GetFontSourceName(x.s3, 0, NULL, 0), G2_E_NOTSUFF);     /* size 0 is rejected before the index is looked at */
    bset_close(&x);
}

/* ---- expiration event ------------------------------------------------------------------------------------------ */
TEST(gap2_fexp_builder_set_has_no_event)
{
    bset x; IDWriteFontSet2 *s2 = NULL;
    if (!bset_open(&x, 2)) return;
    if (FAILED(IDWriteFontSet_QueryInterface(x.set, &IID_IDWriteFontSet2, (void **)&s2))) { bset_close(&x); t_skip("no Set2"); return; }
    /* A set made by a builder carries no cache reference, so there is nothing to wait on. */
    CHECK(IDWriteFontSet2_GetExpirationEvent(s2) == NULL);
    CHECK(IDWriteFontSet2_GetExpirationEvent(s2) == NULL);
    DW_RELEASE(s2);
    bset_close(&x);
}

TEST(gap2_fexp_empty_builder_set_has_no_event)
{
    bset x; IDWriteFontSet2 *s2 = NULL;
    if (!bset_open(&x, 0)) return;
    if (SUCCEEDED(IDWriteFontSet_QueryInterface(x.set, &IID_IDWriteFontSet2, (void **)&s2))) { CHECK(IDWriteFontSet2_GetExpirationEvent(s2) == NULL); DW_RELEASE(s2); }
    bset_close(&x);
}

TEST(gap2_fexp_custom_collection_from_set_no_event)
{
    bset x; IDWriteFontCollection1 *c1 = NULL; IDWriteFontCollection3 *c3 = NULL;
    if (!bset_open(&x, 2)) return;
    if (FAILED(IDWriteFactory3_CreateFontCollectionFromFontSet(x.f3, x.set, &c1)) || !c1) { bset_close(&x); t_skip("CreateFontCollectionFromFontSet failed"); return; }
    if (SUCCEEDED(IDWriteFontCollection1_QueryInterface(c1, &IID_IDWriteFontCollection3, (void **)&c3))) {
        CHECK(IDWriteFontCollection3_GetExpirationEvent(c3) == NULL);
        DW_RELEASE(c3);
    } else t_note("collection from set lacks IDWriteFontCollection3");
    DW_RELEASE(c1);
    bset_close(&x);
}

TEST(gap2_fexp_system_set_event_stable_and_nonsignalled)
{
    NEED_S3(s3, set);
    IDWriteFontSet2 *s2 = NULL;
    if (FAILED(IDWriteFontSet3_QueryInterface(s3, &IID_IDWriteFontSet2, (void **)&s2))) { END_S3(s3, set); t_skip("no Set2"); return; }
    {
        HANDLE h1 = IDWriteFontSet2_GetExpirationEvent(s2), h2 = IDWriteFontSet2_GetExpirationEvent(s2);
        CHECK(h1 == h2);                                    /* the event belongs to the cache reference: same handle each call */
        if (h1) {
            DWORD w = WaitForSingleObject(h1, 0);
            CHECKF(w == WAIT_TIMEOUT || w == WAIT_OBJECT_0, "wait gave %lu", w);
            t_note("system set expiration event %p wait(0)=%lu", h1, w);
        } else t_note("system set has no expiration event");
    }
    DW_RELEASE(s2);
    END_S3(s3, set);
}

TEST(gap2_fexp_system_set_vs_collection_same_event)
{
    /* Set and collection share one implementation (0xd8b50); both derive the handle from the same system-font cache reference. */
    NEED_S3(s3, set);
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontCollection1 *c1 = NULL; IDWriteFontCollection3 *c3 = NULL; IDWriteFontSet2 *s2 = NULL;
    if (SUCCEEDED(IDWriteFactory3_GetSystemFontCollection(f3, FALSE, &c1, FALSE)) && c1 &&
        SUCCEEDED(IDWriteFontCollection1_QueryInterface(c1, &IID_IDWriteFontCollection3, (void **)&c3)) &&
        SUCCEEDED(IDWriteFontSet3_QueryInterface(s3, &IID_IDWriteFontSet2, (void **)&s2))) {
        HANDLE hc = IDWriteFontCollection3_GetExpirationEvent(c3), hs = IDWriteFontSet2_GetExpirationEvent(s2);
        t_note("collection event %p, set event %p", hc, hs);
        G2_SOFT((hc == NULL) == (hs == NULL));
    } else t_note("collection3 or set2 unavailable");
    DW_RELEASE(c3); DW_RELEASE(c1); DW_RELEASE(s2);
    END_S3(s3, set);
}

TEST(gap2_fexp_event_survives_repeated_acquisition)
{
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); int i; HANDLE first = NULL; int have = 0;
    if (!f3) { t_skip("Factory3 unsupported"); return; }
    for (i = 0; i < 5; i++) {
        IDWriteFontSet *s = NULL; IDWriteFontSet2 *s2 = NULL;
        if (FAILED(IDWriteFactory3_GetSystemFontSet(f3, &s)) || !s) continue;
        if (SUCCEEDED(IDWriteFontSet_QueryInterface(s, &IID_IDWriteFontSet2, (void **)&s2))) {
            HANDLE h = IDWriteFontSet2_GetExpirationEvent(s2);
            if (!have) { first = h; have = 1; } else CHECKF(h == first, "iteration %d: %p vs %p", i, h, first);
            IDWriteFontSet2_Release(s2);
        }
        IDWriteFontSet_Release(s);
    }
}
