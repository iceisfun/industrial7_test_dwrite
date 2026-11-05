/* Callbacks into application-supplied loaders and streams: error propagation, ordering and fragment protocol.
 * Basis: FontFileReference::GetStreamInternal (loader HRESULT thrown as CallbackException, DWRITE_E_REMOTEFONT kept),
 * FontFileReference::ThrowIfFailed(hr, offset, size, stream) (a failed ReadFileFragment is re-classified as
 * DWRITE_E_FILEFORMAT when GetFileSize says the requested range lies outside the file), FontFragmentPtr (every fragment is
 * released with the context ReadFileFragment returned), the FPU state sandbox around callbacks. */
#include "fl.h"

static const HRESULT g_errs[] = {
    E_FAIL, E_INVALIDARG, E_OUTOFMEMORY, E_NOTIMPL, E_ACCESSDENIED, E_UNEXPECTED, E_ABORT, E_HANDLE, E_POINTER,
    (HRESULT)0x80070002, (HRESULT)0x80070003, (HRESULT)0x80070005, (HRESULT)0x8007000D, (HRESULT)0x80070020, (HRESULT)0x8007007A, (HRESULT)0x800700EA,
    (HRESULT)0x88985001, (HRESULT)0x88985002, (HRESULT)0x88985003, (HRESULT)0x88985004, (HRESULT)0x88985005, (HRESULT)0x88985006,
    (HRESULT)0x88985007, (HRESULT)0x88985009, (HRESULT)0x8898500A, (HRESULT)0x8898500C, (HRESULT)0x8898500E, (HRESULT)0x8898500F, (HRESULT)0x88985010,
    (HRESULT)0x80041234, (HRESULT)0xC0000005, (HRESULT)0x8000FFFF,
};
#define NERR ((int)(sizeof g_errs / sizeof g_errs[0]))

static void open_ttf(rig *r, int *ok) {
    size_t n; uint8_t *b = fx_ttf(&n);
    *ok = rig_open(r, b, n); free(b);
}
#define OPEN_TTF(r) rig r; int ok_; open_ttf(&r, &ok_); if (!ok_) { rig_close(&r); return; }
#define CHECK_DEFAULT_OUTS(a) do { CHECK(!(a).sup); EXPECT_EQ((a).ft, DWRITE_FONT_FILE_TYPE_UNKNOWN); EXPECT_EQ((a).fc, DWRITE_FONT_FACE_TYPE_UNKNOWN); EXPECT_EQ((a).nf, 0); } while (0)

/* ---------------- CreateStreamFromKey failures ---------------- */
TEST(stream_create_failure_propagates) {
    int i;
    for (i = 0; i < NERR; i++) {
        OPEN_TTF(r);
        r.ld->fail_create = g_errs[i];
        an_res a = fl_analyze(r.file);
        CHECKF(a.hr == g_errs[i], "loader returned %s, Analyze returned %s", dw_hr_name(g_errs[i]), dw_hr_name(a.hr));
        CHECK_DEFAULT_OUTS(a);
        EXPECT_EQ(r.ld->n_create, 1); EXPECT_EQ(r.ld->n_read, 0); EXPECT_EQ(r.ld->n_size, 0);
        rig_close(&r);
    }
}
TEST(stream_create_failure_is_not_cached) {
    OPEN_TTF(r);
    r.ld->fail_create = E_FAIL;
    an_res a = fl_analyze(r.file); CHECK_HR(a.hr, E_FAIL);
    a = fl_analyze(r.file); CHECK_HR(a.hr, E_FAIL);
    EXPECT_EQ(r.ld->n_create, 2);                      /* each attempt asks the loader again */
    r.ld->fail_create = 0;
    a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(r.ld->n_create, 3);
    a = fl_analyze(r.file); CHECK_HR(a.hr, S_OK); EXPECT_EQ(r.ld->n_create, 3);   /* now it is */
    rig_close(&r);
}
TEST(stream_create_failure_leaves_no_stream) {
    OPEN_TTF(r);
    r.ld->fail_create = E_FAIL;
    fl_analyze(r.file);
    EXPECT_EQ(r.ld->nstreams, 0);
    CHECK(r.ld->last_key_size == 4);
    rig_close(&r);
}
TEST(stream_create_failure_message_win32_codes) {
    static const DWORD codes[] = { ERROR_FILE_NOT_FOUND, ERROR_PATH_NOT_FOUND, ERROR_ACCESS_DENIED, ERROR_SHARING_VIOLATION, ERROR_INVALID_DATA, ERROR_NOT_ENOUGH_MEMORY, ERROR_GEN_FAILURE };
    size_t i;
    for (i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        OPEN_TTF(r);
        r.ld->fail_create = HRESULT_FROM_WIN32(codes[i]);
        an_res a = fl_analyze(r.file);
        CHECKF(a.hr == HRESULT_FROM_WIN32(codes[i]), "win32 %lu: Analyze=%s", codes[i], dw_hr_name(a.hr));   /* loader HRESULTs are not translated */
        rig_close(&r);
    }
}
TEST(stream_create_failure_other_file_unaffected) {
    OPEN_TTF(r);
    IDWriteFontFile *second = NULL;
    r.ld->fail_create = E_FAIL;
    an_res a = fl_analyze(r.file); CHECK_HR(a.hr, E_FAIL);
    r.ld->fail_create = 0;
    CHECK_HR(fl_file4(r.ld, r.f, 0, &second), S_OK);
    a = fl_analyze(second); CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    DW_RELEASE(second);
    rig_close(&r);
}

/* ---------------- GetFileSize failures ---------------- */
TEST(stream_size_failure_propagates) {
    int i;
    for (i = 0; i < NERR; i++) {
        OPEN_TTF(r);
        r.ld->fail_size = g_errs[i];
        an_res a = fl_analyze(r.file);
        CHECKF(a.hr == g_errs[i], "stream GetFileSize returned %s, Analyze returned %s", dw_hr_name(g_errs[i]), dw_hr_name(a.hr));
        CHECK_DEFAULT_OUTS(a);
        EXPECT_EQ(r.ld->n_size, 1); EXPECT_EQ(r.ld->n_read, 0);   /* the size comes first; nothing is read after it failed */
        rig_close(&r);
    }
}
TEST(stream_size_failure_stream_is_kept) {
    OPEN_TTF(r);
    r.ld->fail_size = E_FAIL;
    fl_analyze(r.file); fl_analyze(r.file);
    EXPECT_EQ(r.ld->n_create, 1); EXPECT_EQ(r.ld->n_size, 2);    /* the stream itself was created fine and stays cached */
    r.ld->fail_size = 0;
    an_res a = fl_analyze(r.file); CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    rig_close(&r);
}

/* ---------------- ReadFileFragment failures ---------------- */
TEST(stream_read_failure_first_probe) {
    int i;
    for (i = 0; i < NERR; i++) {
        OPEN_TTF(r);
        r.ld->fail_read = g_errs[i]; r.ld->fail_read_from = 0;
        an_res a = fl_analyze(r.file);
        /* the file is much larger than the 0x75 bytes asked for, so the failure is not a range problem: the callback's HRESULT is reported */
        CHECKF(a.hr == g_errs[i], "stream read returned %s, Analyze returned %s", dw_hr_name(g_errs[i]), dw_hr_name(a.hr));
        CHECK_DEFAULT_OUTS(a);
        EXPECT_EQ(r.ld->n_read, 1); EXPECT_EQ(r.ld->n_rel, 0); EXPECT_EQ(r.ld->outstanding, 0);
        rig_close(&r);
    }
}
TEST(stream_read_failure_second_probe) {
    int i;
    for (i = 0; i < NERR; i += 3) {
        OPEN_TTF(r);
        r.ld->fail_read = g_errs[i]; r.ld->fail_read_from = 1;
        an_res a = fl_analyze(r.file);
        CHECKF(a.hr == g_errs[i], "second read returned %s, Analyze returned %s", dw_hr_name(g_errs[i]), dw_hr_name(a.hr));
        EXPECT_EQ(r.ld->n_read, 2); EXPECT_EQ(r.ld->n_rel, 1);   /* the first fragment was already handed back */
        EXPECT_EQ(r.ld->outstanding, 0);
        rig_close(&r);
    }
}
TEST(stream_read_failure_third_probe) {
    int i;
    for (i = 1; i < NERR; i += 4) {
        OPEN_TTF(r);
        r.ld->fail_read = g_errs[i]; r.ld->fail_read_from = 2;
        an_res a = fl_analyze(r.file);
        CHECKF(a.hr == g_errs[i], "third read returned %s, Analyze returned %s", dw_hr_name(g_errs[i]), dw_hr_name(a.hr));
        CHECK_DEFAULT_OUTS(a);
        EXPECT_EQ(r.ld->n_read, 3); EXPECT_EQ(r.ld->n_rel, 2); EXPECT_EQ(r.ld->outstanding, 0);
        rig_close(&r);
    }
}
TEST(stream_read_failure_then_recovery) {
    OPEN_TTF(r);
    r.ld->fail_read = E_FAIL;
    an_res a = fl_analyze(r.file); CHECK_HR(a.hr, E_FAIL);
    r.ld->fail_read = 0; r.ld->fail_read_from = 0;
    a = fl_analyze(r.file); CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    EXPECT_EQ(r.ld->n_create, 1);                       /* the stream survived the failed read */
    rig_close(&r);
}
TEST(stream_read_failure_range_reclassified) {
    /* a failed read whose range lies outside the (reported) file becomes DWRITE_E_FILEFORMAT, whatever the callback returned */
    int i;
    for (i = 0; i < NERR; i++) {
        size_t n; uint8_t *b = fx_magic(0x00010000, 0x40, &n); rig r;
        if (g_errs[i] == (HRESULT)0x8898500D) { free(b); continue; }
        if (rig_open(&r, b, n)) {
            r.ld->fail_read = g_errs[i]; r.ld->fail_read_from = 0;
            an_res a = fl_analyze(r.file);
            CHECKF(a.hr == S_OK || a.hr == H_FILEFORMAT, "small file, read error %s: Analyze=%s", dw_hr_name(g_errs[i]), dw_hr_name(a.hr));
            CHECK_DEFAULT_OUTS(a);
            if (i == 0) t_note("0x40 byte file, failing read: Analyze=%s", dw_hr_name(a.hr));
        }
        rig_close(&r); free(b);
    }
}
TEST(stream_read_failure_boundary_0x75) {
    size_t n; uint8_t *b = fx_magic(0x00010000, 0x75, &n); rig r;
    if (rig_open(&r, b, n)) {
        r.ld->fail_read = E_ACCESSDENIED; r.ld->fail_read_from = 0;
        an_res a = fl_analyze(r.file);
        CHECK_HR(a.hr, E_ACCESSDENIED);                 /* range (0,0x75) fits exactly: genuine callback failure */
    }
    rig_close(&r); free(b);
    b = fx_magic(0x00010000, 0x74, &n);
    if (rig_open(&r, b, n)) {
        r.ld->fail_read = E_ACCESSDENIED; r.ld->fail_read_from = 0;
        an_res a = fl_analyze(r.file);
        CHECK(a.hr == S_OK || a.hr == H_FILEFORMAT);    /* one byte short: reclassified */
        CHECK(a.hr != E_ACCESSDENIED);
    }
    rig_close(&r); free(b);
}
TEST(stream_read_failure_with_size_failure) {
    OPEN_TTF(r);
    r.ld->fail_read = E_ACCESSDENIED;
    /* GetFileSize is consulted before the probes: its failure wins */
    r.ld->fail_size = E_UNEXPECTED;
    an_res a = fl_analyze(r.file); CHECK_HR(a.hr, E_UNEXPECTED);
    EXPECT_EQ(r.ld->n_read, 0);
    rig_close(&r);
}
TEST(stream_read_remote_error_kept_on_small_file) {
    size_t n; uint8_t *b = fx_magic(0x00010000, 0x10, &n); rig r;
    if (rig_open(&r, b, n)) {
        r.ld->fail_read = H_REMOTEFONT; r.ld->fail_read_from = 0;
        an_res a = fl_analyze(r.file);
        CHECK_HR(a.hr, H_REMOTEFONT);                   /* DWRITE_E_REMOTEFONT is tested for before the size comparison */
    }
    rig_close(&r); free(b);
}
TEST(stream_out_of_range_error_code_irrelevant) {
    static const HRESULT oobs[] = { E_FAIL, E_INVALIDARG, E_UNEXPECTED, (HRESULT)0x80070057, (HRESULT)0x88985000, E_NOTIMPL, E_OUTOFMEMORY, (HRESULT)0x80070216 };
    size_t i;
    for (i = 0; i < sizeof oobs / sizeof oobs[0]; i++) {
        size_t n; uint8_t *b = fx_magic(0x00010000, 0x20, &n); rig r;
        if (rig_open(&r, b, n)) {
            r.ld->oob_hr = oobs[i];
            an_res a = fl_analyze(r.file);
            CHECKF(a.hr == S_OK || a.hr == H_FILEFORMAT, "oob result %s -> Analyze=%s", dw_hr_name(oobs[i]), dw_hr_name(a.hr));
        }
        rig_close(&r); free(b);
    }
}

/* ---------------- fragment protocol ---------------- */
TEST(stream_every_fragment_released_once_with_its_context) {
    int mode;
    for (mode = 0; mode < 2; mode++) {
        OPEN_TTF(r);
        r.ld->copy_frag = mode;
        int i; for (i = 0; i < 3; i++) fl_analyze(r.file);
        EXPECT_EQ(r.ld->n_read, 9); EXPECT_EQ(r.ld->n_rel, 9);
        EXPECT_EQ(r.ld->bad_rel, 0); EXPECT_EQ(r.ld->outstanding, 0);
        rig_close(&r);
    }
}
TEST(stream_release_contexts_match_reads) {
    OPEN_TTF(r);
    fl_analyze(r.file);
    int i, reads = 0;
    for (i = 0; i < (int)r.ld->nev; i++) if (r.ld->ev[i].kind == FL_READ) reads++;
    EXPECT_EQ(reads, 3);
    /* contexts of the in-place mode are 0x100000 + read index; the release events carry them back in order */
    int k = 0;
    for (i = 0; i < (int)r.ld->nev; i++) if (r.ld->ev[i].kind == FL_REL) { CHECKF(r.ld->ev[i].a == 0x100000u + (UINT64)k, "release %d ctx %llx", k, (unsigned long long)r.ld->ev[i].a); k++; }
    rig_close(&r);
}
TEST(stream_no_fragment_outstanding_between_probes) {
    OPEN_TTF(r);
    r.ld->on_read = NULL;
    fl_analyze(r.file);
    LONG out = 0, max = 0; int i;
    for (i = 0; i < (int)r.ld->nev; i++) { if (r.ld->ev[i].kind == FL_READ) out++; if (r.ld->ev[i].kind == FL_REL) out--; if (out > max) max = out; }
    EXPECT_EQ(max, 1);                                  /* a probe never holds two fragments at once */
    rig_close(&r);
}
TEST(stream_copy_fragment_pointer_used_not_original) {
    /* with private copies, results must come from the copy: poison the original data after the stream is created */
    OPEN_TTF(r);
    r.ld->copy_frag = 1;
    fl_analyze(r.file);                                 /* creates the stream */
    memset(r.ld->data, 0xEE, 64);                       /* the original bytes now say "not a font"; the next probe copies from them */
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(!a.sup);
    rig_close(&r);
}

/* ---------------- object lifetimes of what the application hands in ---------------- */
TEST(stream_loader_addref_release_balanced_after_file) {
    OPEN_TTF(r);
    fl_analyze(r.file);
    LONG base = fl_refs(r.ld);
    fl_analyze(r.file); fl_analyze(r.file);
    EXPECT_EQ(fl_refs(r.ld), base);
    rig_close(&r);
}
TEST(stream_no_qi_on_loader_or_stream) {
    OPEN_TTF(r);
    fl_analyze(r.file);
    EXPECT_EQ(r.ld->qi, 0);
    EXPECT_EQ(r.ld->nstreams, 1);
    EXPECT_EQ(r.ld->streams[0]->qi, 0);
    t_note("stream AddRef/Release calls during Analyze: %ld/%ld, refcount %ld", (long)r.ld->streams[0]->addref, (long)r.ld->streams[0]->release, (long)r.ld->streams[0]->ref);
    rig_close(&r);
}
TEST(stream_stream_object_kept_until_file_gone) {
    OPEN_TTF(r);
    fl_analyze(r.file);
    fl_stream *s = r.ld->streams[0];
    CHECK(!s->destroyed);
    fl_unregister(r.ld);
    CHECK(!s->destroyed);                               /* unregistering a loader does not tear down streams of live files */
    DW_RELEASE(r.file);
    CHECK(s->destroyed);
    rig_close(&r);
}
TEST(stream_released_when_last_file_released_with_two_files) {
    OPEN_TTF(r);
    IDWriteFontFile *second = NULL; fl_file4(r.ld, r.f, 0, &second);
    fl_analyze(r.file); fl_analyze(second);
    EXPECT_EQ(r.ld->nstreams, 2);
    DW_RELEASE(r.file);
    CHECK(r.ld->streams[0]->destroyed); CHECK(!r.ld->streams[1]->destroyed);
    DW_RELEASE(second);
    CHECK(r.ld->streams[1]->destroyed);
    rig_close(&r);
}
TEST(stream_create_receives_key_pointer_of_file) {
    OPEN_TTF(r);
    fl_analyze(r.file);
    const void *k; UINT32 ks; IDWriteFontFile_GetReferenceKey(r.file, &k, &ks);
    CHECK(r.ld->last_key_ptr == k); EXPECT_EQ(r.ld->last_key_size, ks);
    rig_close(&r);
}

/* ---------------- floating point state ---------------- */
TEST(stream_callbacks_see_clean_sse_state_and_cannot_leak_theirs) {
    OPEN_TTF(r);
    unsigned before = _mm_getcsr();
    r.ld->mxcsr_poke = 1;
    fl_analyze(r.file);
    fl_analyze(r.file);
    unsigned after = _mm_getcsr();
    _mm_setcsr(before);
    EXPECT_EQ(after & 0x6000, before & 0x6000);
    rig_close(&r);
}
TEST(stream_callbacks_with_nondefault_caller_state) {
    OPEN_TTF(r);
    unsigned before = _mm_getcsr();
    _mm_setcsr((before & ~0x6000u) | 0x2000u);            /* caller runs with round-down */
    an_res a = fl_analyze(r.file);
    unsigned after = _mm_getcsr();
    _mm_setcsr(before);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    EXPECT_EQ(after & 0x6000, 0x2000);                    /* the caller's mode is restored on return */
    rig_close(&r);
}

/* ---------------- faces created over custom streams ---------------- */
static void open_face(rig *r, IDWriteFontFace **face, HRESULT *hr, UINT32 idx) {
    DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf; BOOL sup;
    *face = NULL;
    IDWriteFontFile_Analyze(r->file, &sup, &ft, &fc, &nf);
    *hr = IDWriteFactory_CreateFontFace(r->f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r->file, idx, DWRITE_FONT_SIMULATIONS_NONE, face);
}
TEST(stream_face_over_custom_loader) {
    OPEN_TTF(r); IDWriteFontFace *face; HRESULT hr;
    open_face(&r, &face, &hr, 0);
    CHECK_HR(hr, S_OK);
    if (face) {
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
        DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m); EXPECT_EQ(m.designUnitsPerEm, 2048);
        t_note("custom stream: %ld creates, %ld reads, %ld releases after face creation", (long)r.ld->n_create, (long)r.ld->n_read, (long)r.ld->n_rel);
        DW_RELEASE(face);
    }
    if (r.ld->bad_rel) t_note("ReleaseFileFragment calls with a context that was never issued: %ld", (long)r.ld->bad_rel);
    rig_close(&r);
}
TEST(stream_face_reads_stay_inside_file) {
    OPEN_TTF(r); IDWriteFontFace *face; HRESULT hr;
    open_face(&r, &face, &hr, 0);
    if (face) { UINT16 g[2]; UINT32 cps[2] = { 'A', 'B' }; IDWriteFontFace_GetGlyphIndices(face, cps, 2, g); EXPECT_EQ(g[0], 2); DWRITE_GLYPH_METRICS gm[2]; UINT16 ids[2] = { 0, 2 }; IDWriteFontFace_GetDesignGlyphMetrics(face, ids, 2, gm, FALSE); }
    int i, bad = 0;
    for (i = 0; i < (int)r.ld->nev && i < 1024; i++) if (r.ld->ev[i].kind == FL_READ && (r.ld->ev[i].a > r.ld->size || r.ld->ev[i].b > r.ld->size - r.ld->ev[i].a)) bad++;
    EXPECT_EQ(bad, 0);
    DW_RELEASE(face);
    rig_close(&r);
}
TEST(stream_face_fragments_returned_after_everything_released) {
    OPEN_TTF(r); IDWriteFontFace *face; HRESULT hr;
    open_face(&r, &face, &hr, 0);
    DW_RELEASE(face);
    DW_RELEASE(r.file);
    t_note("fragments outstanding after releasing face and file (factory alive): %ld", (long)r.ld->outstanding);
    fl_unregister(r.ld);
    DW_RELEASE(r.f);
    t_note("fragments outstanding after releasing the factory: %ld, stream destroyed: %d", (long)r.ld->outstanding, r.ld->nstreams ? r.ld->streams[0]->destroyed : -1);
    if (r.ld->bad_rel) t_note("ReleaseFileFragment calls with a context that was never issued: %ld", (long)r.ld->bad_rel);
    rig_close(&r);
}
TEST(stream_face_copy_and_misaligned_fragments) {
    int mode;
    for (mode = 1; mode <= 2; mode++) {
        OPEN_TTF(r); IDWriteFontFace *face; HRESULT hr;
        r.ld->copy_frag = 1; r.ld->misalign = mode == 2;
        open_face(&r, &face, &hr, 0);
        CHECKF(hr == S_OK, "mode %d: %s", mode, dw_hr_name(hr));
        if (face) {
            EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), TTF_DEF_NGLYPHS);
            UINT32 cps[1] = { 'z' }; UINT16 g[1]; IDWriteFontFace_GetGlyphIndices(face, cps, 1, g); EXPECT_EQ(g[0], 53);
            UINT32 len = 0; uint8_t *head = dw_face_table(face, DW_TAG('h','e','a','d'), &len); CHECK(head && len == 54); free(head);
        }
        DW_RELEASE(face); if (r.ld->bad_rel) t_note("ReleaseFileFragment calls with a context that was never issued: %ld", (long)r.ld->bad_rel);
        rig_close(&r);
    }
}
TEST(stream_face_creation_read_failure) {
    int i;
    for (i = 0; i < 6; i++) {
        OPEN_TTF(r); IDWriteFontFace *face = (IDWriteFontFace *)1; HRESULT hr;
        r.ld->fail_read = E_FAIL; r.ld->fail_read_from = i;
        hr = IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
        CHECKF(FAILED(hr), "fail_read_from=%d: CreateFontFace=%s", i, dw_hr_name(hr));
        CHECKF(face == NULL, "face out param after failure: %p", (void *)face);
        t_note("CreateFontFace with ReadFileFragment failing from read #%d: %s", i, dw_hr_name(hr));
        EXPECT_EQ(r.ld->outstanding, 0);
        rig_close(&r);
    }
}
TEST(stream_face_creation_stream_create_failure) {
    OPEN_TTF(r); IDWriteFontFace *face = (IDWriteFontFace *)1;
    r.ld->fail_create = E_ACCESSDENIED;
    HRESULT hr = IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    CHECK(FAILED(hr)); CHECK_OUT_NULL(face);
    t_note("CreateFontFace with CreateStreamFromKey = E_ACCESSDENIED: %s", dw_hr_name(hr));
    rig_close(&r);
}
TEST(stream_face_creation_size_failure) {
    OPEN_TTF(r); IDWriteFontFace *face = (IDWriteFontFace *)1;
    r.ld->fail_size = E_ABORT;
    HRESULT hr = IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
    t_note("CreateFontFace with GetFileSize = E_ABORT: %s", dw_hr_name(hr));
    if (FAILED(hr)) CHECK_OUT_NULL(face); else DW_RELEASE(face);
    rig_close(&r);
}
TEST(stream_face_index_out_of_range) {
    OPEN_TTF(r); IDWriteFontFace *face = (IDWriteFontFace *)1; HRESULT hr;
    hr = IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r.file, 1, DWRITE_FONT_SIMULATIONS_NONE, &face);
    CHECK(FAILED(hr)); CHECK_OUT_NULL(face);
    t_note("CreateFontFace(faceIndex=1) on a single-face file: %s", dw_hr_name(hr));
    face = (IDWriteFontFace *)1;
    hr = IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &r.file, 0xFFFFFFFF, DWRITE_FONT_SIMULATIONS_NONE, &face);
    CHECK(FAILED(hr)); CHECK_OUT_NULL(face);
    rig_close(&r);
}
TEST(stream_ttc_faces_share_one_stream_object) {
    const uint8_t *blobs[3]; size_t sizes[3]; int i;
    for (i = 0; i < 3; i++) { char nm[32]; snprintf(nm, sizeof nm, "FlShare%d", i); blobs[i] = ttf_build_styled(nm, 400 + 100 * i, 5, 0, &sizes[i]); }
    size_t n; uint8_t *c = ttf_build_ttc(blobs, sizes, 3, &n);
    rig r;
    if (rig_open(&r, c, n)) {
        IDWriteFontFace *faces[3] = { 0 };
        for (i = 0; i < 3; i++) {
            HRESULT hr = IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &r.file, (UINT32)i, DWRITE_FONT_SIMULATIONS_NONE, &faces[i]);
            CHECKF(hr == S_OK, "TTC face %d: %s", i, dw_hr_name(hr));
            if (faces[i]) EXPECT_EQ(IDWriteFontFace_GetIndex(faces[i]), (UINT32)i);
        }
        t_note("3 faces of one TTC: %ld CreateStreamFromKey calls", (long)r.ld->n_create);
        CHECK(r.ld->n_create >= 1);
        IDWriteFontFace *bad = (IDWriteFontFace *)1;
        HRESULT hr = IDWriteFactory_CreateFontFace(r.f, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, &r.file, 3, DWRITE_FONT_SIMULATIONS_NONE, &bad);
        CHECK(FAILED(hr)); CHECK_OUT_NULL(bad);
        for (i = 0; i < 3; i++) DW_RELEASE(faces[i]);
    }
    rig_close(&r); free(c); for (i = 0; i < 3; i++) free((void *)blobs[i]);
}
TEST(stream_face_does_not_call_last_write_time_note) {
    OPEN_TTF(r); IDWriteFontFace *face; HRESULT hr;
    open_face(&r, &face, &hr, 0);
    t_note("GetLastWriteTime calls during face creation: %ld", (long)r.ld->n_lastw);
    DW_RELEASE(face); rig_close(&r);
}
TEST(stream_lastwrite_failure_does_not_break_face) {
    OPEN_TTF(r); IDWriteFontFace *face; HRESULT hr;
    r.ld->lastw_hr = H_E_NOTIMPL;
    open_face(&r, &face, &hr, 0);
    CHECK_HR(hr, S_OK);
    DW_RELEASE(face); rig_close(&r);
}
TEST(stream_two_loaders_one_factory_distinct_streams) {
    size_t n; uint8_t *b = fx_ttf(&n);
    IDWriteFactory *f = fl_factory(); if (!f) { free(b); t_skip("no factory"); return; }
    fl_loader *a = fl_new(b, n), *c = fl_new(b, n); IDWriteFontFile *fa = NULL, *fc = NULL;
    fl_register(a, f); fl_register(c, f);
    fl_file4(a, f, 0, &fa); fl_file4(c, f, 0, &fc);
    fl_analyze(fa); fl_analyze(fc);
    EXPECT_EQ(a->n_create, 1); EXPECT_EQ(c->n_create, 1);
    EXPECT_EQ(a->n_read, 3); EXPECT_EQ(c->n_read, 3);
    DW_RELEASE(fa); DW_RELEASE(fc); fl_unregister(a); fl_unregister(c); fl_release(a); fl_release(c); IDWriteFactory_Release(f); free(b);
}

/* ---------------- DWRITE_E_REMOTEFONT is a distinct exception type but still surfaces as the same HRESULT ---------------- */
TEST(stream_remote_font_error_from_create) {
    OPEN_TTF(r);
    r.ld->fail_create = H_REMOTEFONT;
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, H_REMOTEFONT); CHECK_DEFAULT_OUTS(a);
    rig_close(&r);
}
TEST(stream_remote_font_error_from_size) {
    OPEN_TTF(r);
    r.ld->fail_size = H_REMOTEFONT;
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, H_REMOTEFONT); CHECK_DEFAULT_OUTS(a);
    rig_close(&r);
}
TEST(stream_remote_font_error_from_read) {
    int from;
    for (from = 0; from < 3; from++) {
        OPEN_TTF(r);
        r.ld->fail_read = H_REMOTEFONT; r.ld->fail_read_from = from;
        an_res a = fl_analyze(r.file);
        CHECKF(a.hr == H_REMOTEFONT, "from read %d: %s", from, dw_hr_name(a.hr));
        EXPECT_EQ(r.ld->n_read, from + 1);
        rig_close(&r);
    }
}
