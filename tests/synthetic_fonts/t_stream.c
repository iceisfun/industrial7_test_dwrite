/* How CreateFontFace / TryGetFontTable behave when the application's font stream misbehaves, and how the fragments it hands out are balanced.
 * Observed: ThrowIfFailed turns a failing ReadFileFragment into DWRITE_E_FILEFORMAT when the requested range lies beyond
 * GetFileSize, otherwise into a CallbackException carrying the stream's own HRESULT. */
#include "sf.h"

#define BADHR ((HRESULT)0x80041234)

TEST(st_read_failure_at_every_position_during_create) {
    int k, failed = 0, ok_at = -1;
    for (k = 0; k < 120; k++) {
        sf_blob b = sf_default(); sf_probe p; dwx_mem_stream *s;
        if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
        s = sf_stream(&p);
        if (!s) { sf_close(&p); sf_free(&b); t_skip("no stream"); return; }
        s->fail_read = BADHR; s->fail_read_from = s->n_read + k;
        sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, 0);
        if (FAILED(p.hr_face)) {
            failed++;
            CHECK_OUT_NULL(p.face);
            if (k < 4 || k % 20 == 0) t_note("ReadFileFragment #%d of CreateFontFace -> %s: CreateFontFace %s", k, dw_hr_name(BADHR), dw_hr_name(p.hr_face));
            PRED("stream HRESULT propagates", p.hr_face == BADHR);
            PRED("no fragment leaks on the failure path", s->outstanding == 0);
        } else { ok_at = k; sf_close(&p); sf_free(&b); if (k == 0 && t_is_wine()) { t_skip("this implementation does not read the stream during CreateFontFace"); return; } break; }
        sf_close(&p); sf_free(&b);
    }
    t_note("CreateFontFace reads the stream %d times before the first read that no longer matters (%d)", failed, ok_at);
    CHECK(failed >= 1);
}
TEST(st_getfilesize_failure_during_create) {
    sf_blob b = sf_default(); sf_probe p; dwx_mem_stream *s;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    s = sf_stream(&p);
    if (s) { s->fail_getsize = BADHR; sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, 0); t_note("CreateFontFace with GetFileSize failing: %s (GetFileSize called %ld times)", dw_hr_name(p.hr_face), (long)s->n_getsize); }
    sf_close(&p); sf_free(&b);
}
TEST(st_copy_fragments_mode) {
    sf_blob b = sf_default(); sf_probe p; dwx_mem_stream *s; UINT16 g;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    s = sf_stream(&p);
    if (s) s->copy_fragments = 1;            /* every fragment is a fresh malloc'd copy and its context is the copy */
    CHECK_HR(sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, 0), S_OK);
    if (p.face) {
        g = sf_gid(p.face, 'Q'); CHECK(g == ttf_default_gid('Q'));
        CHECK(IDWriteFontFace_GetGlyphCount(p.face) == TTF_DEF_NGLYPHS);
    }
    if (s) CHECK(s->bad_release_ctx == 0);
    sf_close(&p); sf_free(&b);
}
TEST(st_fragments_are_released_after_face_release) {
    sf_blob b = sf_default(); sf_probe p; dwx_mem_stream *s;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    s = sf_stream(&p);
    if (p.face && s) {
        t_note("fragments held by a live face: %ld", (long)s->outstanding);
        DW_RELEASE(p.face); DW_RELEASE(p.file);
        t_note("fragments held after face and file are released: %ld", (long)s->outstanding);
        PRED("every fragment released once face and file are gone", s->outstanding == 0);
        CHECK(s->bad_release_ctx == 0);
    }
    sf_close(&p); sf_free(&b);
}
TEST(st_stream_released_after_file_release) {
    sf_blob b = sf_default(); sf_probe p; dwx_mem_stream *s; int rf;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    s = sf_stream(&p);
    if (s && p.face) {
        DW_RELEASE(p.face); DW_RELEASE(p.file);
        rf = (int)dwx_refs(s);
        t_note("stream refcount after the face and the file are gone: %d (destroyed=%d)", rf, dwx_destroyed(s));
        PRED("dwrite drops its stream reference", rf == 0);
    }
    sf_close(&p); sf_free(&b);
}
TEST(st_stream_created_once_per_file) {
    sf_blob b = sf_default(); sf_probe p;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    if (p.face) {
        UINT32 l; uint8_t *t = dw_face_table(p.face, DW_TAG('c','m','a','p'), &l); free(t);
        t_note("CreateStreamFromKey calls after Analyze + CreateFontFace + a table fetch: %ld", (long)p.ld->n_create);
        CHECK(p.ld->n_create >= 1); PRED("stream is cached by the file reference", p.ld->n_create == 1);
    }
    sf_close(&p); sf_free(&b);
}
TEST(st_wrong_key_size_loader_rejection) {
    /* the loader refuses keys that are not 4 bytes; dwrite passes the key it was given unchanged */
    sf_blob b = sf_default(); dwx_mem_loader *ld = dwx_loader_new(); IDWriteFactory *f = dw_isolated(); IDWriteFontFile *file = NULL; uint8_t key[8] = { 0 }; HRESULT hr;
    BOOL sup = 7; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf;
    dwx_loader_add(ld, b.p, b.n);
    if (f && SUCCEEDED(dwx_loader_register(ld, f))) {
        hr = IDWriteFactory_CreateCustomFontFileReference(f, key, 8, (IDWriteFontFileLoader *)ld, &file);
        t_note("CreateCustomFontFileReference with an 8 byte key the loader rejects: %s", dw_hr_name(hr));
        if (SUCCEEDED(hr) && file) { hr = IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf); t_note("  Analyze: %s", dw_hr_name(hr)); CHECK_HR_FAILED(hr); }
        else CHECK_OUT_NULL(file);
        if (ld->last_key_size) CHECK(ld->last_key_size == 8);
    }
    DW_RELEASE(file); dwx_loader_unregister(ld); dwx_release(ld); sf_free(&b);
}
TEST(st_filesize_larger_than_data_is_a_stream_bug) {
    /* the stream reports fewer bytes than the font needs: reads beyond the reported size fail and are format errors */
    sf_blob b = sf_default(); sf_probe p; dwx_mem_stream *s;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    s = sf_stream(&p);
    if (s) {
        s->size = 2000;            /* claim a shorter file: tables further in are out of range */
        sf_face(&p, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, 0);
        t_note("stream truncated to 2000 bytes after Analyze: CreateFontFace %s", dw_hr_name(p.hr_face));
        CHECK(FAILED(p.hr_face)); PRED("range beyond the stream size", p.hr_face == DWRITE_E_FILEFORMAT_);
    }
    sf_close(&p); sf_free(&b);
}
TEST(st_two_faces_one_stream_reads_shared) {
    sf_blob b = sf_default(); sf_probe p; IDWriteFontFace *f2 = NULL; dwx_mem_stream *s; LONG r1;
    if (!sf_open_face(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    s = sf_stream(&p);
    if (s && p.face) {
        r1 = s->n_read;
        CHECK_HR(IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &f2), S_OK);
        t_note("a second face (bold simulation) over the same file cost %ld more reads", (long)(s->n_read - r1));
        DW_RELEASE(f2);
    }
    sf_close(&p); sf_free(&b);
}
