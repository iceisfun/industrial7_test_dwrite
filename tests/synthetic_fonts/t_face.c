/* IDWriteFactory::CreateFontFace argument validation and the basic accessors of the face it returns.
 * Observed: IDWriteFactory::CreateFontFace validates, in this order, faceType in {0,1,2,6,7}; faceIndex == 0 unless faceType == 2;
 * numberOfFiles == 1; fontFiles != NULL and fontFiles[0] != NULL; (simulations & ~3) == 0.  Any violation -> E_INVALIDARG with *ppFace = NULL.
 * IDWriteFontFace::GetFiles/GetType/GetIndex/GetSimulations/GetGlyphCount read members of the face. */
#include "sf.h"

#define NEED_PROBE(p) sf_blob blob_ = sf_default(); sf_probe p; if (!sf_open(&p, blob_.p, blob_.n)) { sf_close(&p); sf_free(&blob_); return; } \
    if (FAILED(p.hr_analyze) || !p.supported) { t_skip("default synthetic font not analysable"); sf_close(&p); sf_free(&blob_); return; }
#define END_PROBE(p) do { sf_close(&p); sf_free(&blob_); } while (0)

static IDWriteFontFace *const FACE_POISON = (IDWriteFontFace *)(ULONG_PTR)1;
#define REL_FACE(f) do { if ((f) && (f) != FACE_POISON) IDWriteFontFace_Release(f); (f) = NULL; } while (0)

/* ---- face type -------------------------------------------------------------------------------------------- */
#define BADTYPE(name, t) TEST(cf_type_##name##_is_invalidarg) { NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON; \
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, (DWRITE_FONT_FACE_TYPE)(t), 1, &p.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG); \
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p); }
BADTYPE(type1, 3)
BADTYPE(vector, 4)
BADTYPE(bitmap, 5)
BADTYPE(8, 8)
BADTYPE(9, 9)
BADTYPE(16, 16)
BADTYPE(255, 255)
BADTYPE(256, 256)
BADTYPE(minus1, -1)
BADTYPE(80000000, 0x80000000u)
/* the other values pass argument validation; what happens then depends on the file */
#define SOFTTYPE(name, t) TEST(cf_type_##name##_passes_validation) { NEED_PROBE(p); IDWriteFontFace *face = NULL; \
    HRESULT hr = IDWriteFactory_CreateFontFace(p.f, (DWRITE_FONT_FACE_TYPE)(t), 1, &p.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face); \
    t_note("CreateFontFace(type %d) over a TrueType file: %s, GetType=%d", (int)(t), dw_hr_name(hr), face ? (int)IDWriteFontFace_GetType(face) : -1); \
    CHECKF(hr != E_INVALIDARG || !strcmp(#name, "x"), "type %d must not be rejected by argument validation", (int)(t)); \
    if (SUCCEEDED(hr)) CHECK(face != NULL); else CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p); }
SOFTTYPE(cff, 0)
SOFTTYPE(unknown, 6)
SOFTTYPE(rawcff, 7)
SOFTTYPE(collection_on_plain_file, 2)
TEST(cf_type_truetype_succeeds) {
    NEED_PROBE(p); IDWriteFontFace *face = NULL;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), S_OK);
    CHECK(face != NULL);
    if (face) CHECK(IDWriteFontFace_GetType(face) == DWRITE_FONT_FACE_TYPE_TRUETYPE);
    REL_FACE(face); END_PROBE(p);
}

/* ---- face index ------------------------------------------------------------------------------------------- */
#define BADINDEX(name, i) TEST(cf_index_##name##_on_truetype_is_invalidarg) { NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON; \
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, (i), DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG); \
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p); }
BADINDEX(1, 1) BADINDEX(2, 2) BADINDEX(255, 255) BADINDEX(65535, 65535) BADINDEX(65536, 65536) BADINDEX(ffffffff, 0xFFFFFFFFu)
TEST(cf_index_nonzero_with_unknown_type_is_invalidarg) {
    NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, &p.file, 1, DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG);
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p);
}
TEST(cf_index_nonzero_with_cff_type_is_invalidarg) {
    NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_CFF, 1, &p.file, 3, DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG);
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p);
}
TEST(cf_index_validation_precedes_type_acceptance_of_bad_type) {
    /* an invalid type is rejected even when the index is also invalid: same HRESULT either way */
    NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TYPE1, 1, &p.file, 5, DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG);
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p);
}

/* ---- file array ------------------------------------------------------------------------------------------- */
#define BADCOUNT(name, c) TEST(cf_numfiles_##name##_is_invalidarg) { NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON; IDWriteFontFile *files[2]; files[0] = p.file; files[1] = p.file; \
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, (c), files, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG); \
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p); }
BADCOUNT(0, 0) BADCOUNT(2, 2) BADCOUNT(3, 3) BADCOUNT(ffffffff, 0xFFFFFFFFu)
TEST(cf_null_file_array_is_invalidarg) {
    if (t_is_wine()) { t_skip("Wine dereferences NULL/empty file arrays"); return; }
    NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG);
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p);
}
TEST(cf_null_file_pointer_is_invalidarg) {
    if (t_is_wine()) { t_skip("Wine dereferences NULL/empty file arrays"); return; }
    NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON; IDWriteFontFile *files[1] = { NULL };
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, files, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG);
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p);
}
TEST(cf_zero_files_with_null_array_is_invalidarg) {
    if (t_is_wine()) { t_skip("Wine dereferences NULL/empty file arrays"); return; }
    NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, NULL, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), E_INVALIDARG);
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p);
}

/* ---- simulations ------------------------------------------------------------------------------------------ */
#define BADSIM(name, v) TEST(cf_simulations_##name##_is_invalidarg) { NEED_PROBE(p); IDWriteFontFace *face = FACE_POISON; \
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, (DWRITE_FONT_SIMULATIONS)(v), &face), E_INVALIDARG); \
    CHECK_OUT_NULL(face); REL_FACE(face); END_PROBE(p); }
BADSIM(4, 4) BADSIM(5, 5) BADSIM(7, 7) BADSIM(8, 8) BADSIM(16, 16) BADSIM(0x100, 0x100) BADSIM(0x8000, 0x8000) BADSIM(fffffffc, 0xFFFFFFFCu) BADSIM(ffffffff, 0xFFFFFFFFu)
#define GOODSIM(name, v) TEST(cf_simulations_##name##_roundtrips) { NEED_PROBE(p); IDWriteFontFace *face = NULL; \
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, (DWRITE_FONT_SIMULATIONS)(v), &face), S_OK); \
    if (face) CHECK(IDWriteFontFace_GetSimulations(face) == (DWRITE_FONT_SIMULATIONS)(v)); REL_FACE(face); END_PROBE(p); }
GOODSIM(none, 0) GOODSIM(bold, 1) GOODSIM(oblique, 2) GOODSIM(bold_oblique, 3)
TEST(cf_simulations_make_distinct_faces) {
    NEED_PROBE(p); IDWriteFontFace *a = NULL, *b = NULL, *c = NULL;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &a), S_OK);
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &b), S_OK);
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &c), S_OK);
    CHECK(a && b && a != b);
    CHECK(a == c);       /* same key while `a` is alive: the factory's active face cache hands out the same object */
    REL_FACE(a); REL_FACE(b); REL_FACE(c); END_PROBE(p);
}

/* ---- basic accessors ---------------------------------------------------------------------------------------- */
TEST(cf_accessors_on_default_font) {
    NEED_PROBE(p);
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, DWRITE_FONT_SIMULATIONS_NONE), S_OK);
    if (p.face) {
        CHECK(IDWriteFontFace_GetType(p.face) == DWRITE_FONT_FACE_TYPE_TRUETYPE);
        CHECK(IDWriteFontFace_GetIndex(p.face) == 0);
        CHECK(IDWriteFontFace_GetSimulations(p.face) == DWRITE_FONT_SIMULATIONS_NONE);
        CHECK(IDWriteFontFace_GetGlyphCount(p.face) == TTF_DEF_NGLYPHS);
        CHECK(IDWriteFontFace_IsSymbolFont(p.face) == FALSE);
    }
    END_PROBE(p);
}
TEST(cf_getfiles_count_query) {
    NEED_PROBE(p); UINT32 n = 99;
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, DWRITE_FONT_SIMULATIONS_NONE), S_OK);
    if (p.face) {
        CHECK_HR(IDWriteFontFace_GetFiles(p.face, &n, NULL), S_OK); CHECK(n == 1);
        n = 0; CHECK_HR(IDWriteFontFace_GetFiles(p.face, &n, NULL), S_OK); CHECK(n == 1);       /* input value ignored when files == NULL */
        n = 77; CHECK_HR(IDWriteFontFace_GetFiles(p.face, &n, NULL), S_OK); CHECK(n == 1);
    }
    END_PROBE(p);
}
TEST(cf_getfiles_returns_the_file_it_was_created_from) {
    NEED_PROBE(p); UINT32 n = 1; IDWriteFontFile *got = NULL; const void *key = NULL; UINT32 ksz = 0; IDWriteFontFileLoader *ldr = NULL;
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, DWRITE_FONT_SIMULATIONS_NONE), S_OK);
    if (p.face) {
        CHECK_HR(IDWriteFontFace_GetFiles(p.face, &n, &got), S_OK);
        CHECK(got != NULL);
        if (got) {
            CHECK(got == p.file);          /* ctor stores and AddRefs the file passed to CreateFontFace; GetFiles returns it */
            CHECK_HR(IDWriteFontFile_GetReferenceKey(got, &key, &ksz), S_OK);
            CHECK(ksz == 4); if (key && ksz == 4) CHECK(!memcmp(key, &p.key, 4));
            CHECK_HR(IDWriteFontFile_GetLoader(got, &ldr), S_OK);
            CHECK(ldr == (IDWriteFontFileLoader *)p.ld);
            DW_RELEASE(ldr); DW_RELEASE(got);
        }
    }
    END_PROBE(p);
}
TEST(cf_getfiles_wrong_count_is_invalidarg) {
    NEED_PROBE(p); IDWriteFontFile *got = (IDWriteFontFile *)(ULONG_PTR)1; UINT32 n;
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, DWRITE_FONT_SIMULATIONS_NONE), S_OK);
    if (p.face) {
        UINT32 bad[] = { 0, 2, 3, 0xFFFFFFFFu }; int i;
        for (i = 0; i < 4; i++) { n = bad[i]; got = (IDWriteFontFile *)(ULONG_PTR)1; CHECK_HR(IDWriteFontFace_GetFiles(p.face, &n, &got), E_INVALIDARG); }
    }
    END_PROBE(p);
}
TEST(cf_getfiles_null_count_is_invalidarg) {
    NEED_PROBE(p); IDWriteFontFile *got = NULL;
    if (t_is_wine()) { t_skip("Wine dereferences the NULL count"); END_PROBE(p); return; }
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, DWRITE_FONT_SIMULATIONS_NONE), S_OK);
    if (p.face) { CHECK_HR(IDWriteFontFace_GetFiles(p.face, NULL, &got), E_INVALIDARG); CHECK_HR(IDWriteFontFace_GetFiles(p.face, NULL, NULL), E_INVALIDARG); }
    END_PROBE(p);
}
TEST(cf_getfiles_adds_one_reference) {
    NEED_PROBE(p); UINT32 n = 1; IDWriteFontFile *got = NULL; ULONG before, after;
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, DWRITE_FONT_SIMULATIONS_NONE), S_OK);
    if (p.face) {
        before = dw_refcount((IUnknown *)p.file);
        CHECK_HR(IDWriteFontFace_GetFiles(p.face, &n, &got), S_OK);
        after = dw_refcount((IUnknown *)p.file);
        CHECKF(after == before + 1, "file refcount %lu -> %lu", before, after);
        DW_RELEASE(got);
        EXPECT_REFS(p.file, before);
    }
    END_PROBE(p);
}

/* ---- identity, lifetime ------------------------------------------------------------------------------------------ */
TEST(cf_same_file_same_key_same_face) {
    NEED_PROBE(p); IDWriteFontFace *a = NULL, *b = NULL;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &a), S_OK);
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &b), S_OK);
    CHECK(a != NULL && a == b);
    if (a == b && a) { ULONG r = dw_refcount((IUnknown *)a); t_note("refcount with two outstanding references: %lu", r); }
    REL_FACE(a); REL_FACE(b); END_PROBE(p);
}
TEST(cf_two_file_objects_same_key_same_face) {
    NEED_PROBE(p); IDWriteFontFile *f2 = NULL; IDWriteFontFace *a = NULL, *b = NULL;
    CHECK_HR(dwx_loader_file(p.ld, p.f, p.key, &f2), S_OK);
    CHECK(f2 != NULL && f2 != p.file);
    if (f2) {
        CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &a), S_OK);
        CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &f2, 0, 0, &b), S_OK);
        t_note("two IDWriteFontFile objects with identical loader+key give %s face object", a == b ? "the same" : "different");
        PRED("faces are keyed by loader+key, not by file object", a == b);
        REL_FACE(a); REL_FACE(b); DW_RELEASE(f2);
    }
    END_PROBE(p);
}
TEST(cf_face_survives_release_of_file_reference) {
    NEED_PROBE(p); IDWriteFontFace *face = NULL; UINT16 g;
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &face), S_OK);
    DW_RELEASE(p.file);
    if (face) {
        g = sf_gid(face, 'A'); CHECK(g == ttf_default_gid('A'));
        { const void *d = NULL; UINT32 n = 0; void *ctx = NULL; BOOL ex = FALSE;
          CHECK_HR(IDWriteFontFace_TryGetFontTable(face, DW_TAG('h','e','a','d'), &d, &n, &ctx, &ex), S_OK);
          CHECK(ex && n == 54 && d); IDWriteFontFace_ReleaseFontTable(face, ctx); }
        IDWriteFontFace_Release(face);
    }
    END_PROBE(p);
}
TEST(cf_face_holds_the_file_alive) {
    NEED_PROBE(p); IDWriteFontFace *face = NULL; ULONG before, after;
    before = dw_refcount((IUnknown *)p.file);
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &face), S_OK);
    after = dw_refcount((IUnknown *)p.file);
    CHECKF(after == before + 1, "file refcount %lu -> %lu (the face keeps the file)", before, after);
    REL_FACE(face);
    after = dw_refcount((IUnknown *)p.file);
    CHECKF(after == before, "file refcount after face release: %lu want %lu", after, before);
    END_PROBE(p);
}
TEST(cf_face_refcount_basics) {
    NEED_PROBE(p);
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, 0), S_OK);
    if (p.face) {
        EXPECT_REFS(p.face, 1);
        IDWriteFontFace_AddRef(p.face); EXPECT_REFS(p.face, 2); IDWriteFontFace_Release(p.face); EXPECT_REFS(p.face, 1);
    }
    END_PROBE(p);
}
TEST(cf_face_qi_matrix) {
    NEED_PROBE(p);
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, 0), S_OK);
    if (p.face) {
        dw_qi_row rows[] = { DW_QI_ROW(IDWriteFontFace, 1), DW_QI_ROW(IDWriteFontFace1, 1), DW_QI_ROW(IDWriteFontFace2, -1), DW_QI_ROW(IDWriteFontFace3, -1),
                             DW_QI_ROW(IDWriteFontFace4, -1), DW_QI_ROW(IDWriteFontFace5, -1), DW_QI_ROW(IDWriteFontFaceReference, -1) };
        /* Windows answers E_NOTIMPL (not E_NOINTERFACE) for some well-known IIDs the face does not implement */
        const IID *no[] = { &IID_IDWriteFactory, &IID_IDWriteFont, &IID_IDWriteFontFile, &IID_IDWriteTextFormat }; const char *nn[] = { "IDWriteFactory", "IDWriteFont", "IDWriteFontFile", "IDWriteTextFormat" }; int i;
        DW_CHECK_QI(p.face, rows);
        for (i = 0; i < 4; i++) {
            void *q = (void *)(ULONG_PTR)1; HRESULT hq = IUnknown_QueryInterface((IUnknown *)p.face, no[i], &q);
            CHECKF(hq == E_NOINTERFACE || hq == E_NOTIMPL, "%s: QI failure code %s", nn[i], dw_hr_name(hq));
            CHECKF(q == NULL, "%s: out pointer not zeroed on failure (%p)", nn[i], q);
            if (SUCCEEDED(hq) && q) IUnknown_Release((IUnknown *)q);
        }
    }
    END_PROBE(p);
}
TEST(cf_creation_reads_the_file_and_balances_fragments) {
    NEED_PROBE(p); dwx_mem_stream *s = sf_stream(&p); LONG r0 = s ? s->n_read : 0, o0 = s ? s->outstanding : 0, r1;
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, 0), S_OK);
    s = sf_stream(&p); r1 = s ? s->n_read : 0;
    t_note("ReadFileFragment calls made by CreateFontFace on the default font: %ld; fragments still held: %ld", (long)(r1 - r0), s ? (long)(s->outstanding - o0) : -1L);
    PRED("CreateFontFace reads the font", r1 > r0);
    if (p.face) { DW_RELEASE(p.face); }
    if (s) t_note("fragments held after the face was released: %ld", (long)(s->outstanding - o0));
    END_PROBE(p);
}
TEST(cf_unsupported_file_cannot_make_a_face) {
    static uint8_t junk[300]; sf_probe p; IDWriteFontFace *face = FACE_POISON; HRESULT hr;
    memset(junk, 0x41, sizeof junk);
    if (!sf_open(&p, junk, sizeof junk)) { sf_close(&p); return; }
    CHECK(p.supported == FALSE);
    hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &face);
    t_note("CreateFontFace over a non-font file: %s", dw_hr_name(hr));
    CHECK_HR_FAILED(hr); CHECK_OUT_NULL(face); REL_FACE(face); sf_close(&p);
}
TEST(cf_every_failure_leaves_out_null) {
    /* damaged files fail with a HRESULT and a NULL face; repeat to make sure a failed creation is not cached */
    sf_blob b = sf_default(); sf_probe p; int i;
    sf_set_numtables(&b, 0);
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    for (i = 0; i < 3; i++) {
        IDWriteFontFace *face = FACE_POISON; HRESULT hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &face);
        CHECK_HR_FAILED(hr); CHECK_OUT_NULL(face); REL_FACE(face);
        if (i == 0) t_note("CreateFontFace with numTables = 0: %s", dw_hr_name(hr));
    }
    sf_close(&p); sf_free(&b);
}
TEST(cf_oneshot_helper_matches_manual_path) {
    IDWriteFactory *f = dw_isolated(); dwx_font h; 
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR(dwx_font_open_default(&h, f), S_OK);
    CHECK(h.supported && h.nfaces == 1 && h.file_type == DWRITE_FONT_FILE_TYPE_TRUETYPE && h.face_type == DWRITE_FONT_FACE_TYPE_TRUETYPE);
    CHECK(h.face != NULL);
    dwx_font_close(&h);
}

/* ---- TrueType collections ----------------------------------------------------------------------------------------- */
static sf_blob make_ttc(int n, const char *fam) {
    sf_blob parts[4], r; const uint8_t *bl[4]; size_t sz[4], total; int i; ttf_spec s; char nm[32];
    for (i = 0; i < n; i++) { snprintf(nm, sizeof nm, "%s%d", fam, i); ttf_style_spec(&s, nm, 400, 5, 0); parts[i] = sf_build(&s); bl[i] = parts[i].p; sz[i] = parts[i].n; }
    r.p = ttf_build_ttc(bl, sz, n, &total); r.n = total;
    for (i = 0; i < n; i++) sf_free(&parts[i]);
    return r;
}
TEST(cf_ttc_faces_by_index) {
    sf_blob t = make_ttc(3, "Coll"); sf_probe p; UINT32 i;
    if (!sf_open(&p, t.p, t.n)) { sf_close(&p); sf_free(&t); return; }
    CHECK_HR(p.hr_analyze, S_OK); CHECK(p.supported); CHECK(p.ftype == DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION);
    CHECK(p.fctype == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION); CHECK(p.nfaces == 3);
    for (i = 0; i < 3; i++) {
        IDWriteFontFace *face = NULL;
        CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, i, 0, &face), S_OK);
        if (face) {
            CHECK(IDWriteFontFace_GetIndex(face) == i);
            PRED("collection faces report TRUETYPE_COLLECTION", IDWriteFontFace_GetType(face) == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
            CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS);
            CHECK(sf_gid(face, 'B') == ttf_default_gid('B'));
            IDWriteFontFace_Release(face);
        }
    }
    sf_close(&p); sf_free(&t);
}
TEST(cf_ttc_faces_are_distinct_objects_with_own_tables) {
    sf_blob t = make_ttc(2, "Dist"); sf_probe p; IDWriteFontFace *a = NULL, *b = NULL;
    if (!sf_open(&p, t.p, t.n)) { sf_close(&p); sf_free(&t); return; }
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, 0, 0, &a), S_OK);
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, 1, 0, &b), S_OK);
    CHECK(a && b && a != b);
    if (a && b) {
        UINT32 la = 0, lb = 0; uint8_t *na = dw_face_table(a, DW_TAG('n','a','m','e'), &la), *nb = dw_face_table(b, DW_TAG('n','a','m','e'), &lb);
        CHECK(na && nb); if (na && nb) CHECK(la != lb || memcmp(na, nb, la) != 0);       /* "Dist0" vs "Dist1" */
        free(na); free(nb);
    }
    REL_FACE(a); REL_FACE(b); sf_close(&p); sf_free(&t);
}
TEST(cf_ttc_index_out_of_range) {
    sf_blob t = make_ttc(2, "Rng"); sf_probe p; UINT32 idx[] = { 2, 3, 100, 65535 }; int i;
    if (!sf_open(&p, t.p, t.n)) { sf_close(&p); sf_free(&t); return; }
    for (i = 0; i < 4; i++) {
        IDWriteFontFace *face = FACE_POISON; HRESULT hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, idx[i], 0, &face);
        t_note("collection of 2, face index %u: %s", (unsigned)idx[i], dw_hr_name(hr));
        CHECK_HR_FAILED(hr); CHECK_OUT_NULL(face); REL_FACE(face);
    }
    sf_close(&p); sf_free(&t);
}
TEST(cf_ttc_index_beyond_16_bits) {
    sf_blob t = make_ttc(2, "Big"); sf_probe p; IDWriteFontFace *face = FACE_POISON; HRESULT hr;
    if (!sf_open(&p, t.p, t.n)) { sf_close(&p); sf_free(&t); return; }
    hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, 0x10000, 0, &face);
    t_note("face index 0x10000 on a 2-face collection: %s", dw_hr_name(hr));
    /* GetIndex() is a 16-bit member, so 0x10000 may be truncated to face 0 and succeed */
    if (FAILED(hr)) CHECK_OUT_NULL(face); else t_note("  GetIndex() of the resulting face: %u", (unsigned)IDWriteFontFace_GetIndex(face));
    REL_FACE(face); sf_close(&p); sf_free(&t);
}
TEST(cf_ttc_as_plain_truetype_face) {
    sf_blob t = make_ttc(2, "Plain"); sf_probe p; IDWriteFontFace *face = FACE_POISON; HRESULT hr;
    if (!sf_open(&p, t.p, t.n)) { sf_close(&p); sf_free(&t); return; }
    hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &face);
    t_note("collection file opened with faceType TRUETYPE index 0: %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(face);
    REL_FACE(face); sf_close(&p); sf_free(&t);
}
TEST(cf_ttc_directory_offset_beyond_file) {
    sf_blob t = make_ttc(2, "Off"); sf_probe p; IDWriteFontFace *face = FACE_POISON; HRESULT hr;
    sf_w32(t.p + 16, (uint32_t)t.n + 0x100);        /* offset of font #1 */
    if (!sf_open(&p, t.p, t.n)) { sf_close(&p); sf_free(&t); return; }
    hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, 1, 0, &face);
    t_note("face 1 with an offset table entry beyond the file: %s", dw_hr_name(hr));
    CHECK_HR_FAILED(hr); CHECK_OUT_NULL(face); REL_FACE(face);
    face = NULL; CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, 0, 0, &face), S_OK);     /* the other face is fine */
    REL_FACE(face); sf_close(&p); sf_free(&t);
}
TEST(cf_ttc_claims_more_faces_than_offsets) {
    sf_blob t = make_ttc(2, "Cl"); sf_probe p; IDWriteFontFace *face = FACE_POISON; HRESULT hr;
    sf_w32(t.p + 8, 50);                             /* numFonts = 50, only 2 offsets exist */
    if (!sf_open(&p, t.p, t.n)) { sf_close(&p); sf_free(&t); return; }
    CHECK(p.nfaces == 50);
    hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, 10, 0, &face);
    t_note("face 10 of a collection whose header claims 50 faces but has 2 offsets: %s", dw_hr_name(hr));
    REL_FACE(face); sf_close(&p); sf_free(&t);
}
TEST(cf_ttc_same_font_listed_twice) {
    sf_blob d = sf_default(), t; const uint8_t *bl[2]; size_t sz[2], total; sf_probe p; IDWriteFontFace *a = NULL, *b = NULL;
    bl[0] = d.p; bl[1] = d.p; sz[0] = d.n; sz[1] = d.n; t.p = ttf_build_ttc(bl, sz, 2, &total); t.n = total;
    if (!sf_open(&p, t.p, t.n)) { sf_close(&p); sf_free(&t); sf_free(&d); return; }
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, 0, 0, &a), S_OK);
    CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &p.file, 1, 0, &b), S_OK);
    CHECK(a != b);
    REL_FACE(a); REL_FACE(b); sf_close(&p); sf_free(&t); sf_free(&d);
}
