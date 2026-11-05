/* Custom-loader contracts: the reference-key namespace and key storage, and concurrent use of one stream.
 * Basis: CreateCustomFontFileReference takes opaque key bytes plus a size and a loader; the key means something only to the loader it was
 * given with (two loaders may be handed byte-identical keys and serve unrelated fonts). GetReferenceKey storage belongs to the font-file
 * object. A single IDWriteFontFileStream may receive simultaneous calls (ReadFileFragment from several threads, no external locking),
 * and the built-in streams answer reads that do not fit in the file with fixed failure HRESULTs. */
#include "fl.h"

/* ---------------- a loader whose streams depend on the key ---------------- */
#define KL_MAXE 6
typedef struct { uint8_t key[32]; UINT32 ks; const uint8_t *data; size_t n; } kl_ent;
typedef struct kl {
    const IDWriteFontFileLoaderVtbl *lpVtbl; volatile LONG ref;
    kl_ent e[KL_MAXE]; int ne;
    volatile LONG n_create; UINT32 last_ks; uint8_t last_key[32]; const void *last_ptr;
} kl;
typedef struct { const IDWriteFontFileStreamVtbl *lpVtbl; volatile LONG ref; const uint8_t *data; size_t n; } kstream;

static HRESULT WINAPI ks_QI(IDWriteFontFileStream *T, REFIID r, void **o) {
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteFontFileStream)) { *o = T; InterlockedIncrement(&((kstream *)T)->ref); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ks_AR(IDWriteFontFileStream *T) { return InterlockedIncrement(&((kstream *)T)->ref); }
static ULONG WINAPI ks_RL(IDWriteFontFileStream *T) { LONG r = InterlockedDecrement(&((kstream *)T)->ref); if (r == 0) free(T); return r; }
static HRESULT WINAPI ks_Read(IDWriteFontFileStream *T, const void **st, UINT64 off, UINT64 size, void **ctx) {
    kstream *s = (kstream *)T; *st = NULL; *ctx = NULL;
    if (off > s->n || size > s->n - off) return E_FAIL;
    *st = s->data + off; return S_OK;
}
static void WINAPI ks_Rel(IDWriteFontFileStream *T, void *ctx) { }
static HRESULT WINAPI ks_Size(IDWriteFontFileStream *T, UINT64 *sz) { *sz = ((kstream *)T)->n; return S_OK; }
static HRESULT WINAPI ks_LastW(IDWriteFontFileStream *T, UINT64 *t) { *t = 0; return E_NOTIMPL; }
static const IDWriteFontFileStreamVtbl ks_vt = { ks_QI, ks_AR, ks_RL, ks_Read, ks_Rel, ks_Size, ks_LastW };

static HRESULT WINAPI kl_QI(IDWriteFontFileLoader *T, REFIID r, void **o) {
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteFontFileLoader)) { *o = T; InterlockedIncrement(&((kl *)T)->ref); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI kl_AR(IDWriteFontFileLoader *T) { return InterlockedIncrement(&((kl *)T)->ref); }
static ULONG WINAPI kl_RL(IDWriteFontFileLoader *T) { return InterlockedDecrement(&((kl *)T)->ref); }
static HRESULT WINAPI kl_Create(IDWriteFontFileLoader *T, const void *key, UINT32 ks, IDWriteFontFileStream **out) {
    kl *l = (kl *)T; int i; kstream *s;
    *out = NULL; InterlockedIncrement(&l->n_create);
    l->last_ks = ks; l->last_ptr = key; memset(l->last_key, 0, sizeof l->last_key); if (key && ks <= 32) memcpy(l->last_key, key, ks);
    for (i = 0; i < l->ne; i++) if (l->e[i].ks == ks && !memcmp(l->e[i].key, key, ks)) break;
    if (i == l->ne) return H_FILENOTFOUND;
    s = (kstream *)calloc(1, sizeof *s); s->lpVtbl = &ks_vt; s->ref = 1; s->data = l->e[i].data; s->n = l->e[i].n;
    *out = (IDWriteFontFileStream *)s; return S_OK;
}
static const IDWriteFontFileLoaderVtbl kl_vt = { kl_QI, kl_AR, kl_RL, kl_Create };
static kl *kl_new(void) { kl *l = (kl *)calloc(1, sizeof *l); l->lpVtbl = &kl_vt; l->ref = 1; return l; }
static void kl_add(kl *l, const void *key, UINT32 ks, const uint8_t *data, size_t n) {
    kl_ent *e = &l->e[l->ne++]; memcpy(e->key, key, ks); e->ks = ks; e->data = data; e->n = n;
}
#define KLDR(l) ((IDWriteFontFileLoader *)(l))

/* second synthetic font: same glyphs, different vertical metrics, so a face reveals which blob it came from */
static uint8_t *fx_alt(size_t *n) {
    ttf_spec s; ttf_default_spec(&s);
    s.hhea_asc = 1500; s.hhea_desc = -300; s.hhea_gap = 10; s.typo_asc = 1450; s.typo_desc = -310; s.typo_gap = 20; s.win_asc = 1550; s.win_desc = 330;
    s.cap_height = 1250; s.x_height = 900; s.upm = 2048;
    return ttf_build(&s, n);
}
static int face_metrics(IDWriteFactory *f, IDWriteFontFile *file, DWRITE_FONT_METRICS *m) {
    IDWriteFontFace *face = NULL;
    HRESULT hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    memset(m, 0, sizeof *m);
    CHECK_HR(hr, S_OK);
    if (!face) return 0;
    IDWriteFontFace_GetMetrics(face, m); DW_RELEASE(face); return 1;
}
/* metrics of a blob served by a fresh isolated factory (the reference the keyed tests compare against) */
static int ref_metrics(const uint8_t *b, size_t n, DWRITE_FONT_METRICS *m) {
    IDWriteFactory *f = fl_factory(); fl_loader *l; IDWriteFontFile *file = NULL; int ok = 0;
    memset(m, 0, sizeof *m);
    if (!f) return 0;
    l = fl_new(b, n); fl_register(l, f);
    if (SUCCEEDED(fl_file4(l, f, 0, &file))) ok = face_metrics(f, file, m);
    DW_RELEASE(file); fl_unregister(l); fl_release(l); IDWriteFactory_Release(f);
    return ok;
}
static int same_metrics(const DWRITE_FONT_METRICS *a, const DWRITE_FONT_METRICS *b) { return !memcmp(a, b, sizeof *a); }

/* ================= key namespace belongs to the loader ================= */
static const UINT32 K1020[4] = { 1, 0, 2, 0 };

#define NS_SETUP() \
    size_t na, nb; uint8_t *A = fx_ttf(&na), *B = fx_alt(&nb); DWRITE_FONT_METRICS ma, mb; \
    IDWriteFactory *f = fl_factory(); kl *l1, *l2; IDWriteFontFile *fa = NULL, *fb = NULL; \
    if (!f) { free(A); free(B); t_skip("factory"); return; } \
    if (!ref_metrics(A, na, &ma) || !ref_metrics(B, nb, &mb)) { IDWriteFactory_Release(f); free(A); free(B); t_skip("reference faces"); return; } \
    CHECK(!same_metrics(&ma, &mb)); \
    l1 = kl_new(); l2 = kl_new(); kl_add(l1, K1020, 16, A, na); kl_add(l2, K1020, 16, B, nb); \
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, KLDR(l1)), S_OK); CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, KLDR(l2)), S_OK);
#define NS_END() do { DW_RELEASE(fa); DW_RELEASE(fb); IDWriteFactory_UnregisterFontFileLoader(f, KLDR(l1)); IDWriteFactory_UnregisterFontFileLoader(f, KLDR(l2)); \
    IDWriteFactory_Release(f); free(A); free(B); } while (0)

TEST(ns_two_loaders_same_key_serve_their_own_fonts) {
    NS_SETUP(); DWRITE_FONT_METRICS m;
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l1), &fa), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l2), &fb), S_OK);
    if (fa && fb) {
        if (face_metrics(f, fa, &m)) CHECK(same_metrics(&m, &ma));
        if (face_metrics(f, fb, &m)) CHECK(same_metrics(&m, &mb));
        EXPECT_EQ(l1->n_create, 1); EXPECT_EQ(l2->n_create, 1);
        EXPECT_EQ(l1->last_ks, 16); CHECK(!memcmp(l1->last_key, K1020, 16));
        EXPECT_EQ(l2->last_ks, 16); CHECK(!memcmp(l2->last_key, K1020, 16));
    }
    NS_END();
}
TEST(ns_two_loaders_same_key_second_registered_first_used) {
    NS_SETUP(); DWRITE_FONT_METRICS m;
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l2), &fb), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l1), &fa), S_OK);
    if (fa && fb) {
        if (face_metrics(f, fb, &m)) CHECK(same_metrics(&m, &mb));
        if (face_metrics(f, fa, &m)) CHECK(same_metrics(&m, &ma));
        EXPECT_EQ(l1->n_create, 1); EXPECT_EQ(l2->n_create, 1);
    }
    NS_END();
}
TEST(ns_two_loaders_same_key_files_report_the_same_key_bytes) {
    NS_SETUP(); const void *ka = NULL, *kb = NULL; UINT32 sa = 0, sb = 0;
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l1), &fa), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l2), &fb), S_OK);
    if (fa && fb) {
        CHECK_HR(IDWriteFontFile_GetReferenceKey(fa, &ka, &sa), S_OK); CHECK_HR(IDWriteFontFile_GetReferenceKey(fb, &kb, &sb), S_OK);
        EXPECT_EQ(sa, 16); EXPECT_EQ(sb, 16);
        CHECK(ka && kb && !memcmp(ka, K1020, 16) && !memcmp(kb, K1020, 16));
        CHECK(ka != (const void *)K1020 && kb != (const void *)K1020);       /* each file object owns a copy; no identity between files is assumed */
        IDWriteFontFileLoader *ga = NULL, *gb = NULL;
        CHECK_HR(IDWriteFontFile_GetLoader(fa, &ga), S_OK); CHECK_HR(IDWriteFontFile_GetLoader(fb, &gb), S_OK);
        CHECK(ga == KLDR(l1)); CHECK(gb == KLDR(l2));                          /* the loader a file reports is the one it was created with */
        DW_RELEASE(ga); DW_RELEASE(gb);
    }
    NS_END();
}
TEST(ns_no_cross_loader_reuse_after_release) {
    /* a face made from loader 1's key, fully released, must not be handed back for the same key under loader 2 */
    NS_SETUP(); DWRITE_FONT_METRICS m; int round;
    for (round = 0; round < 3; round++) {
        IDWriteFontFile *x = NULL, *y = NULL;
        CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l1), &x), S_OK);
        if (x && face_metrics(f, x, &m)) CHECK(same_metrics(&m, &ma));
        DW_RELEASE(x);
        CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l2), &y), S_OK);
        if (y && face_metrics(f, y, &m)) CHECK(same_metrics(&m, &mb));
        DW_RELEASE(y);
    }
    EXPECT_EQ(l1->n_create, 1); EXPECT_EQ(l2->n_create, 1);                   /* the factory remembers a face per (loader, key bytes) */
    NS_END();
}
TEST(ns_loader_serves_by_its_own_key_not_by_equal_key_elsewhere) {
    /* loader 2 knows only a different key: the key loader 1 knows is not found there, and vice versa */
    NS_SETUP(); static const UINT32 other[4] = { 1, 0, 2, 1 }; IDWriteFontFile *x = NULL; DWRITE_FONT_FACE_TYPE ft; BOOL sup; DWRITE_FONT_FILE_TYPE t; UINT32 nf;
    l2->e[0].key[12] = 1;                                                       /* loader 2 now serves {1,0,2,1} */
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, K1020, 16, KLDR(l2), &x), S_OK);   /* creation never consults the loader */
    EXPECT_EQ(l2->n_create, 0);
    if (x) { HRESULT hr = IDWriteFontFile_Analyze(x, &sup, &t, &ft, &nf); CHECK_HR(hr, H_FILENOTFOUND); CHECK(!sup); EXPECT_EQ(l2->n_create, 1); DW_RELEASE(x); }
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, other, 16, KLDR(l2), &x), S_OK);
    if (x) { CHECK_HR(IDWriteFontFile_Analyze(x, &sup, &t, &ft, &nf), S_OK); CHECK(sup); DW_RELEASE(x); }
    NS_END();
}

TEST(ns_one_loader_equal_keys_in_two_buffers) {
    size_t na, nb; uint8_t *A = fx_ttf(&na), *B = fx_alt(&nb); DWRITE_FONT_METRICS ma, mb, m;
    IDWriteFactory *f = fl_factory(); kl *l; uint8_t *k1, *k2, *k3; IDWriteFontFile *x = NULL, *y = NULL, *z = NULL;
    if (!f) { free(A); free(B); t_skip("factory"); return; }
    if (!ref_metrics(A, na, &ma) || !ref_metrics(B, nb, &mb)) { IDWriteFactory_Release(f); free(A); free(B); t_skip("reference faces"); return; }
    k1 = (uint8_t *)malloc(16); k2 = (uint8_t *)malloc(16); k3 = (uint8_t *)malloc(16);
    memcpy(k1, K1020, 16); memcpy(k2, K1020, 16); memcpy(k3, K1020, 16); k3[15] = 0x80;     /* differs in the last byte only */
    l = kl_new(); kl_add(l, K1020, 16, A, na); kl_add(l, k3, 16, B, nb);
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, KLDR(l)), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, k1, 16, KLDR(l), &x), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, k2, 16, KLDR(l), &y), S_OK);
    CHECK_HR(IDWriteFactory_CreateCustomFontFileReference(f, k3, 16, KLDR(l), &z), S_OK);
    memset(k1, 0xCC, 16); free(k1); k1 = NULL;                                    /* the buffers are the caller's again at once */
    memset(k2, 0xDD, 16); free(k2); k2 = NULL;
    if (x && y && z) {
        const void *px, *py, *pz; UINT32 sx, sy, sz;
        CHECK_HR(IDWriteFontFile_GetReferenceKey(x, &px, &sx), S_OK); CHECK_HR(IDWriteFontFile_GetReferenceKey(y, &py, &sy), S_OK); CHECK_HR(IDWriteFontFile_GetReferenceKey(z, &pz, &sz), S_OK);
        EXPECT_EQ(sx, 16); EXPECT_EQ(sy, 16); EXPECT_EQ(sz, 16);
        CHECK(px && py && !memcmp(px, K1020, 16) && !memcmp(py, K1020, 16));
        CHECK(pz && !memcmp(pz, k3, 16));
        if (face_metrics(f, x, &m)) CHECK(same_metrics(&m, &ma));
        if (face_metrics(f, y, &m)) CHECK(same_metrics(&m, &ma));
        if (face_metrics(f, z, &m)) CHECK(same_metrics(&m, &mb));
        EXPECT_EQ(l->n_create, 2);                                                 /* x and y are one (loader, key bytes) pair: the face made from x serves y */
        EXPECT_EQ(l->last_ks, 16);
    }
    DW_RELEASE(x); DW_RELEASE(y); DW_RELEASE(z);
    IDWriteFactory_UnregisterFontFileLoader(f, KLDR(l)); IDWriteFactory_Release(f); free(k3); free(A); free(B);
}

/* The factory keeps the face it built for a (loader, key bytes) pair after every reference is gone, so the same pair gives the same data
 * even if the loader would now serve something else. Another loader, another key or another factory is a different pair; unregistering
 * the loader drops its pairs. */
static void mkface(IDWriteFactory *f, fl_loader *l, UINT32 key, DWRITE_FONT_METRICS *m) {
    IDWriteFontFile *q = NULL; IDWriteFontFace *fc = NULL; fl_file4(l, f, key, &q);
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &q, 0, DWRITE_FONT_SIMULATIONS_NONE, &fc), S_OK);
    memset(m, 0, sizeof *m); if (fc) IDWriteFontFace_GetMetrics(fc, m);
    DW_RELEASE(fc); DW_RELEASE(q);
}
TEST(ns_face_is_remembered_per_factory_loader_and_key) {
    size_t na, nb; uint8_t *A = fx_ttf(&na), *B = fx_alt(&nb); DWRITE_FONT_METRICS ma, mb, m;
    IDWriteFactory *f = fl_factory(), *g = fl_factory(); fl_loader *l, *l2;
    if (!f || !g || !ref_metrics(A, na, &ma) || !ref_metrics(B, nb, &mb)) { if (f) IDWriteFactory_Release(f); if (g) IDWriteFactory_Release(g); free(A); free(B); t_skip("setup"); return; }
    l = fl_new(A, na); fl_register(l, f); fl_register(l, g);
    mkface(f, l, 0, &m); CHECK(same_metrics(&m, &ma)); EXPECT_EQ(l->n_create, 1);
    fl_set_data(l, B, nb);                                           /* from now on the loader serves the other font for every key */
    mkface(f, l, 0, &m); CHECK(same_metrics(&m, &ma)); EXPECT_EQ(l->n_create, 1);          /* same factory, same loader, same key bytes: remembered */
    mkface(f, l, 1, &m); CHECK(same_metrics(&m, &mb)); EXPECT_EQ(l->n_create, 2);          /* other key bytes */
    mkface(g, l, 0, &m); CHECK(same_metrics(&m, &mb)); EXPECT_EQ(l->n_create, 3);          /* other factory */
    l2 = fl_new(B, nb); fl_register(l2, f);
    mkface(f, l2, 0, &m); CHECK(same_metrics(&m, &mb)); EXPECT_EQ(l2->n_create, 1);        /* other loader, same key bytes */
    CHECK_HR(IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)), S_OK);   /* unregistering forgets the pair (g still has the loader) */
    CHECK_HR(IDWriteFactory_RegisterFontFileLoader(f, LDR(l)), S_OK);
    mkface(f, l, 0, &m); CHECK(same_metrics(&m, &mb)); EXPECT_EQ(l->n_create, 4);
    IDWriteFactory_UnregisterFontFileLoader(f, LDR(l)); IDWriteFactory_UnregisterFontFileLoader(f, LDR(l2)); IDWriteFactory_UnregisterFontFileLoader(g, LDR(l));
    fl_release(l); fl_release(l2); IDWriteFactory_Release(f); IDWriteFactory_Release(g); free(A); free(B);
}

/* ================= GetReferenceKey storage lives as long as the font-file object ================= */
static int key_is(IDWriteFontFile *file, const void *want, UINT32 ws, const void *ptr0) {
    const void *p = NULL; UINT32 s = 0; HRESULT hr = IDWriteFontFile_GetReferenceKey(file, &p, &s);
    return hr == S_OK && s == ws && p && !memcmp(p, want, ws) && (!ptr0 || p == ptr0);
}
TEST(keylife_storage_valid_until_file_released) {
    size_t n; uint8_t *b = fx_ttf(&n); IDWriteFactory *f = fl_factory(); fl_loader *l; IDWriteFontFile *x = NULL, *twin = NULL; uint8_t *src = (uint8_t *)malloc(64), want[64]; int i;
    const void *p0 = NULL; UINT32 s0 = 0; IDWriteFontFace *face = NULL;
    if (!f) { free(b); free(src); t_skip("factory"); return; }
    for (i = 0; i < 64; i++) want[i] = src[i] = (uint8_t)(0x30 + i * 5);
    l = fl_new(b, n); fl_register(l, f);
    CHECK_HR(fl_file(l, f, src, 64, &x), S_OK); CHECK_HR(fl_file(l, f, want, 64, &twin), S_OK);
    CHECK_HR(IDWriteFontFile_GetReferenceKey(x, &p0, &s0), S_OK); EXPECT_EQ(s0, 64);
    CHECK(p0 != NULL && p0 != (void *)src);
    memset(src, 0, 64); free(src);                                                 /* caller buffer gone */
    CHECK(key_is(x, want, 64, p0));
    an_res a = fl_analyze(x); CHECK_HR(a.hr, S_OK);                                /* stream creation hands the same storage to the loader */
    CHECK(l->last_key_ptr == p0); EXPECT_EQ(l->last_key_size, 64);
    CHECK(key_is(x, want, 64, p0));
    CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &x, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), S_OK);
    CHECK(key_is(x, want, 64, p0));
    DW_RELEASE(face);
    DW_RELEASE(twin);                                                              /* releasing another file with the same key bytes */
    CHECK(key_is(x, want, 64, p0));
    CHECK_HR(fl_unregister(l), S_OK);                                              /* unregistering the loader does not touch the key */
    CHECK(key_is(x, want, 64, p0));
    IDWriteFactory_Release(f);                                                     /* nor does dropping our factory reference */
    CHECK(key_is(x, want, 64, p0));
    DW_RELEASE(x); fl_release(l); free(b);
}
TEST(keylife_face_keeps_file_and_key_alive) {
    size_t n; uint8_t *b = fx_ttf(&n); rig r; IDWriteFontFace *face = NULL; IDWriteFontFile *got = NULL; UINT32 cnt = 1;
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    CHECK_HR(IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), S_OK);
    DW_RELEASE(r.file);                                                            /* the face's own reference remains */
    if (face) {
        CHECK_HR(IDWriteFontFace_GetFiles(face, &cnt, &got), S_OK);
        if (got) {
            UINT32 want = 0; CHECK(key_is(got, &want, 4, NULL));
            DW_RELEASE(got);
        }
        DW_RELEASE(face);
    }
    rig_close(&r); free(b);
}
TEST(keylife_repeated_calls_same_storage) {
    size_t n; uint8_t *b = fx_ttf(&n); rig r; int i;
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    const void *p0 = NULL; UINT32 s0; CHECK_HR(IDWriteFontFile_GetReferenceKey(r.file, &p0, &s0), S_OK);
    for (i = 0; i < 50; i++) { fl_analyze(r.file); CHECK(key_is(r.file, &(UINT32){0}, 4, p0)); }
    rig_close(&r); free(b);
}

/* ================= built-in streams: failures for requests that do not fit ================= */
static int mk_local_stream(IDWriteFactory *f, IDWriteFontFile **file, IDWriteFontFileLoader **ld, IDWriteFontFileStream **st, const char *leaf, uint8_t **blob, size_t *n) {
    char path[MAX_PATH]; WCHAR w[MAX_PATH]; const void *k = NULL; UINT32 ks = 0;
    *file = NULL; *ld = NULL; *st = NULL; *blob = fx_ttf(n);
    if (!fl_write_tmp(leaf, *blob, *n, path, sizeof path)) return 0;
    dw_wide(path, w, MAX_PATH);
    if (FAILED(IDWriteFactory_CreateFontFileReference(f, w, NULL, file)) || !*file) return 0;
    IDWriteFontFile_GetReferenceKey(*file, &k, &ks); IDWriteFontFile_GetLoader(*file, ld);
    return *ld && SUCCEEDED(IDWriteFontFileLoader_CreateStreamFromKey(*ld, k, ks, st)) && *st;
}
#define ERR_OVERFLOW ((HRESULT)0x80070216)   /* HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) */
TEST(local_stream_unfittable_reads_fail_with_fixed_hresults) {
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file; IDWriteFontFileLoader *ld; IDWriteFontFileStream *s; uint8_t *b; size_t n; UINT64 M = ~0ull, sz; int i;
    if (!f) { t_skip("factory"); return; }
    if (mk_local_stream(f, &file, &ld, &s, "ct_eof.ttf", &b, &n)) {
        IDWriteFontFileStream_GetFileSize(s, &sz); EXPECT_EQ(sz, n);
        /* inside 64 bits but past the end: E_FAIL; offset + size wrapping 2^64: ERROR_ARITHMETIC_OVERFLOW */
        struct { UINT64 off, len; HRESULT hr; } t[] = {
            { sz, 1, E_FAIL }, { sz - 1, 2, E_FAIL }, { 0, sz + 1, E_FAIL }, { sz + 1, 0, E_FAIL }, { sz, 0xFFFFFFFFull, E_FAIL }, { 0, M, E_FAIL }, { M, 0, E_FAIL },
            { 0x100000000ull, 1, E_FAIL }, { 0x7FFFFFFFFFFFFFFFull, 2, E_FAIL }, { 0x8000000000000000ull, 1, E_FAIL }, { 0x8000000000000000ull, 0x7FFFFFFFFFFFFFFFull, E_FAIL },
            { M, 1, ERR_OVERFLOW }, { 1, M, ERR_OVERFLOW }, { M, M, ERR_OVERFLOW }, { M - 3, 8, ERR_OVERFLOW }, { 0x8000000000000000ull, 0x8000000000000000ull, ERR_OVERFLOW },
            { sz / 2, M - sz / 2 + 1, ERR_OVERFLOW },
        };
        for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
            const void *st = (const void *)1; void *ctx = (void *)1;
            HRESULT hr = IDWriteFontFileStream_ReadFileFragment(s, &st, t[i].off, t[i].len, &ctx);
            CHECKF(hr == t[i].hr, "local read(%llx,%llx): got %s", (unsigned long long)t[i].off, (unsigned long long)t[i].len, dw_hr_name(hr));
            CHECKF(st == NULL && ctx == NULL, "local read(%llx,%llx) outputs %p %p", (unsigned long long)t[i].off, (unsigned long long)t[i].len, st, ctx);
        }
        {   /* the stream is still good, and the exact fits still succeed */
            const void *st = NULL; void *ctx = NULL;
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, 0, sz, &ctx), S_OK); CHECK(st && !memcmp(st, b, n)); IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
            CHECK_HR(IDWriteFontFileStream_ReadFileFragment(s, &st, sz, 0, &ctx), S_OK); IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
        }
        IDWriteFontFileStream_Release(s);
    }
    DW_RELEASE(ld); DW_RELEASE(file); IDWriteFactory_Release(f); free(b);
}
TEST(mem_stream_unfittable_reads_fail_with_invalidarg) {
    IDWriteFactory *f = fl_factory(); IDWriteFactory5 *f5 = f ? fl_factory5(f) : NULL; IDWriteInMemoryFontFileLoader *m = NULL; IDWriteFontFile *file = NULL; IDWriteFontFileStream *s = NULL;
    size_t n; uint8_t *b = fx_ttf(&n); UINT32 key = 0; UINT64 M = ~0ull; int i;
    if (!f5) { if (f) IDWriteFactory_Release(f); free(b); t_skip("IDWriteFactory5 not available"); return; }
    IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &m);
    if (m) {
        IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)m);
        IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m, f, b, (UINT32)n, NULL, &file);
        IDWriteFontFileLoader_CreateStreamFromKey((IDWriteFontFileLoader *)m, &key, 4, &s);
    }
    if (s) {
        UINT64 o[] = { 0, M, M, 0x8000000000000000ull, 0x7FFFFFFFFFFFFFFFull, n, 0x100000000ull, 1 };
        UINT64 l[] = { M, 0, M, 1, 2, 1, 1, M };
        for (i = 0; i < 8; i++) {
            const void *st = (const void *)1; void *ctx = (void *)1;
            HRESULT hr = IDWriteFontFileStream_ReadFileFragment(s, &st, o[i], l[i], &ctx);
            CHECKF(hr == E_INVALIDARG, "mem read(%llx,%llx): got %s", (unsigned long long)o[i], (unsigned long long)l[i], dw_hr_name(hr));
            CHECKF(st == NULL && ctx == NULL, "mem read(%llx,%llx) outputs %p %p", (unsigned long long)o[i], (unsigned long long)l[i], st, ctx);
        }
        IDWriteFontFileStream_Release(s);
    }
    DW_RELEASE(file);
    if (m) { IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)m); IDWriteInMemoryFontFileLoader_Release(m); }
    IDWriteFactory5_Release(f5); IDWriteFactory_Release(f); free(b);
}

/* ================= one stream, simultaneous calls ================= */
struct rd { IDWriteFontFileStream *s; const uint8_t *ref; UINT64 off, len, sz; int iters; int ok; HRESULT bad_hr; };
static DWORD WINAPI rd_worker(LPVOID p) {
    struct rd *c = (struct rd *)p; int i; c->ok = 1;
    for (i = 0; i < c->iters; i++) {
        const void *st = NULL; void *ctx = NULL; HRESULT hr;
        hr = IDWriteFontFileStream_ReadFileFragment(c->s, &st, c->off, c->len, &ctx);
        if (hr != S_OK || !st || memcmp(st, c->ref + c->off, (size_t)c->len)) c->ok = 0;
        if (hr == S_OK) IDWriteFontFileStream_ReleaseFileFragment(c->s, ctx);
        if (i % 7 == 0) {   /* a request that cannot be served interleaved with the good ones */
            st = (const void *)1; ctx = (void *)1;
            hr = IDWriteFontFileStream_ReadFileFragment(c->s, &st, c->sz, 1, &ctx);
            if (hr != c->bad_hr || st != NULL || ctx != NULL) c->ok = 0;
        }
    }
    return 0;
}
static void run_two_ranges(IDWriteFontFileStream *s, const uint8_t *ref, UINT64 sz, HRESULT bad_hr) {
    struct rd c[4]; HANDLE th[4]; int i;
    UINT64 offs[4] = { 0, 40, 1000, sz - 100 }, lens[4] = { 40, 96, 512, 100 };
    for (i = 0; i < 4; i++) { c[i].s = s; c[i].ref = ref; c[i].off = offs[i]; c[i].len = lens[i]; c[i].sz = sz; c[i].iters = 3000; c[i].ok = 0; c[i].bad_hr = bad_hr; th[i] = CreateThread(NULL, 0, rd_worker, &c[i], 0, NULL); }
    WaitForMultipleObjects(4, th, TRUE, 60000);
    for (i = 0; i < 4; i++) { CloseHandle(th[i]); CHECKF(c[i].ok, "thread %d (range %llu+%llu)", i, (unsigned long long)offs[i], (unsigned long long)lens[i]); }
}
TEST(local_stream_concurrent_reads_of_two_ranges) {
    IDWriteFactory *f = fl_factory(); IDWriteFontFile *file; IDWriteFontFileLoader *ld; IDWriteFontFileStream *s; uint8_t *b; size_t n;
    if (!f) { t_skip("factory"); return; }
    if (mk_local_stream(f, &file, &ld, &s, "ct_conc.ttf", &b, &n)) { run_two_ranges(s, b, n, E_FAIL); IDWriteFontFileStream_Release(s); }
    DW_RELEASE(ld); DW_RELEASE(file); IDWriteFactory_Release(f); free(b);
}
TEST(mem_stream_concurrent_reads_of_two_ranges) {
    IDWriteFactory *f = fl_factory(); IDWriteFactory5 *f5 = f ? fl_factory5(f) : NULL; IDWriteInMemoryFontFileLoader *m = NULL; IDWriteFontFile *file = NULL; IDWriteFontFileStream *s = NULL;
    size_t n; uint8_t *b = fx_ttf(&n); UINT32 key = 0;
    if (!f5) { if (f) IDWriteFactory_Release(f); free(b); t_skip("IDWriteFactory5 not available"); return; }
    IDWriteFactory5_CreateInMemoryFontFileLoader(f5, &m);
    if (m) {
        IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)m);
        IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m, f, b, (UINT32)n, NULL, &file);
        IDWriteFontFileLoader_CreateStreamFromKey((IDWriteFontFileLoader *)m, &key, 4, &s);
    }
    if (s) { run_two_ranges(s, b, n, E_INVALIDARG); IDWriteFontFileStream_Release(s); }
    DW_RELEASE(file);
    if (m) { IDWriteFactory_UnregisterFontFileLoader(f, (IDWriteFontFileLoader *)m); IDWriteInMemoryFontFileLoader_Release(m); }
    IDWriteFactory5_Release(f5); IDWriteFactory_Release(f); free(b);
}

/* dwrite calls an application stream from several threads at once: a deliberately slow ReadFileFragment shows the overlap */
static volatile LONG g_inflight, g_maxin;
static void slow_read(fl_loader *l, UINT64 off, UINT64 size) {
    LONG c = InterlockedIncrement(&g_inflight), m;
    while ((m = g_maxin) < c) InterlockedCompareExchange(&g_maxin, c, m);
    Sleep(25);
    InterlockedDecrement(&g_inflight);
}
struct tw { IDWriteFontFace *face; IDWriteFontFile *file; const char *tag; const uint8_t *want; unsigned wlen; int mode, ok; };
static DWORD WINAPI tw_worker(LPVOID p) {
    struct tw *c = (struct tw *)p; int i; c->ok = 1;
    for (i = 0; i < 3; i++) {
        if (c->mode == 0) { an_res a = fl_analyze(c->file); if (a.hr != S_OK || !a.sup) c->ok = 0; }
        else {
            const void *t = NULL; void *ctx = NULL; UINT32 n = 0; BOOL ex = FALSE;
            HRESULT hr = IDWriteFontFace_TryGetFontTable(c->face, DWRITE_MAKE_OPENTYPE_TAG(c->tag[0], c->tag[1], c->tag[2], c->tag[3]), &t, &n, &ctx, &ex);
            if (hr != S_OK || !ex || n != c->wlen || memcmp(t, c->want, n)) c->ok = 0;
            if (hr == S_OK && ex) IDWriteFontFace_ReleaseFontTable(c->face, ctx);
        }
    }
    return 0;
}
static void slow_stream_run(int mode, int *maxin) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n); struct tw cs[4]; HANDLE th[4]; int i; IDWriteFontFace *face = NULL;
    static const char *tags[4] = { "cmap", "glyf", "hmtx", "name" };
    *maxin = 0;
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    if (mode == 1) CHECK_HR(IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), S_OK);
    if (mode == 1 && !face) { rig_close(&r); free(b); return; }
    r.ld->on_read = slow_read; fl_clear(r.ld); g_maxin = 0; g_inflight = 0;
    for (i = 0; i < 4; i++) {
        unsigned tl = 0; cs[i].file = r.file; cs[i].face = face; cs[i].mode = mode; cs[i].tag = tags[i]; cs[i].ok = 0;
        cs[i].want = ttf_find_table(b, n, tags[i], &tl); cs[i].wlen = tl;
        th[i] = CreateThread(NULL, 0, tw_worker, &cs[i], 0, NULL);
    }
    WaitForMultipleObjects(4, th, TRUE, 120000);
    for (i = 0; i < 4; i++) { CloseHandle(th[i]); CHECKF(cs[i].ok, "mode %d thread %d", mode, i); }
    EXPECT_EQ(r.ld->outstanding, 0); EXPECT_EQ(r.ld->bad_rel, 0);
    EXPECT_EQ(g_inflight, 0);
    *maxin = (int)g_maxin;
    r.ld->on_read = NULL;
    DW_RELEASE(face); rig_close(&r); free(b);
}
TEST(slow_stream_concurrent_analyze_overlaps) {
    int m; slow_stream_run(0, &m);
    t_note("analyze: max simultaneous ReadFileFragment calls into the loader's streams = %d (each Analyze makes its own stream, so this is concurrency across the loader, not on one stream)", m);
    CHECKF(m >= 2, "reads into the loader never overlapped (max %d)", m);                 /* dwrite does not serialise the stream for the application */
}
TEST(slow_stream_concurrent_table_reads_overlap) {
    int m; slow_stream_run(1, &m);
    t_note("TryGetFontTable: max simultaneous ReadFileFragment calls on the one stream = %d", m);
    CHECKF(m >= 2, "reads into the loader never overlapped (max %d)", m);
}
