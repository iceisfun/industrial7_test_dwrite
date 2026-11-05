/* File-level failures: missing / unreadable / locked local files (IOException::MapFileOpenError), corrupt font data (FileFormatException ->
 * DWRITE_E_FILEFORMAT), CreateFontFace argument validation, GDI interop NOFONT, and font-not-in-collection NOFONT. */
#include "eh.h"

static int write_file(const char *path, const void *data, size_t n) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); DWORD w = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    BOOL ok = n ? WriteFile(h, data, (DWORD)n, &w, NULL) : TRUE;
    CloseHandle(h);
    return ok && w == n;
}
static HRESULT ref_a(IDWriteFactory *f, const char *path, const FILETIME *ft, IDWriteFontFile **out) {
    WCHAR w[1024]; dw_wide(path, w, 1024);
    return IDWriteFactory_CreateFontFileReference(f, w, ft, out);
}
static HRESULT face_of(IDWriteFactory *f, IDWriteFontFile *file, IDWriteFontFace **face) {
    return IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, face);
}
#define FT_ANY(name) FILETIME name = { 0x11111111, 0x01D11111 }

/* ---- CreateFontFileReference: path errors ------------------------------------------------------------------------------------ */
TEST(path_missing_file_no_time_is_filenotfound) {
    NEED_FACTORY(); char p[MAX_PATH]; IDWriteFontFile *file = SENT(IDWriteFontFile *);
    t_tmpfile(p, sizeof p, "eh_missing.ttf");
    /* With no FILETIME the key builder reads the last-write time; the failed open maps ERROR_FILE_NOT_FOUND -> DWRITE_E_FILENOTFOUND. */
    CHECK_HR(ref_a(f, p, NULL, &file), EH_FILENOTFOUND); CHECK_OUT_NULL(file);
    DONE_FACTORY();
}
TEST(path_missing_directory_no_time_is_filenotfound) {
    NEED_FACTORY(); char p[MAX_PATH]; IDWriteFontFile *file = SENT(IDWriteFontFile *);
    t_tmpfile(p, sizeof p, "eh_no_dir\\nested\\x.ttf");
    CHECK_HR(ref_a(f, p, NULL, &file), EH_FILENOTFOUND); CHECK_OUT_NULL(file);       /* ERROR_PATH_NOT_FOUND maps the same way */
    DONE_FACTORY();
}
TEST(path_null_is_invalidarg) {
    NEED_FACTORY(); IDWriteFontFile *file = SENT(IDWriteFontFile *); FILETIME ft = { 1, 2 };
    CHECK_HR(IDWriteFactory_CreateFontFileReference(f, NULL, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    file = SENT(IDWriteFontFile *);
    CHECK_HR(IDWriteFactory_CreateFontFileReference(f, NULL, &ft, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    DONE_FACTORY();
}
TEST(path_empty_string_fails) {
    NEED_FACTORY(); IDWriteFontFile *file = SENT(IDWriteFontFile *);
    HRESULT hr = IDWriteFactory_CreateFontFileReference(f, L"", NULL, &file);
    t_note("CreateFontFileReference(L\"\") = %s", dw_hr_name(hr));
    CHECK(FAILED(hr)); CHECK_OUT_NULL(file);
    DONE_FACTORY();
}
TEST(path_invalid_characters_fail) {
    NEED_FACTORY(); static const WCHAR *const paths[] = { L"C:\\eh_bad<name>.ttf", L"C:\\eh_bad|name.ttf", L"C:\\eh_bad\"name.ttf", L"C:\\eh_bad*name.ttf", L"C:\\eh_bad?name.ttf" };
    unsigned i;
    for (i = 0; i < sizeof paths / sizeof paths[0]; i++) {
        IDWriteFontFile *file = SENT(IDWriteFontFile *);
        HRESULT hr = IDWriteFactory_CreateFontFileReference(f, paths[i], NULL, &file);
        CHECKF(FAILED(hr), "path %u accepted", i); CHECK_OUT_NULL(file);
        t_note("invalid-name path #%u -> %s (ERROR_INVALID_NAME 123 / ERROR_FILE_NOT_FOUND map to FILENOTFOUND)", i, dw_hr_name(hr));
        CHECKF(hr == EH_FILENOTFOUND || hr == EH_FILEACCESS || hr == HRESULT_FROM_WIN32(ERROR_INVALID_NAME), "unexpected %s", dw_hr_name(hr));
    }
    DONE_FACTORY();
}
TEST(path_directory_is_not_a_font_file) {
    NEED_FACTORY(); char p[MAX_PATH]; IDWriteFontFile *file = SENT(IDWriteFontFile *); size_t l;
    snprintf(p, sizeof p, "%s", t_tmpdir()); l = strlen(p); if (l && p[l - 1] == '\\') p[l - 1] = 0;
    HRESULT hr = ref_a(f, p, NULL, &file);
    t_note("CreateFontFileReference(directory) = %s (ERROR_ACCESS_DENIED maps to FILEACCESS)", dw_hr_name(hr));
    CHECK(FAILED(hr)); CHECK_OUT_NULL(file);
    CHECKF(hr == EH_FILEACCESS || hr == EH_FILENOTFOUND, "unexpected %s", dw_hr_name(hr));
    DONE_FACTORY();
}
TEST(path_explicit_time_defers_all_io) {
    /* The key is built from the supplied FILETIME alone: no file access, so even an impossible path yields a reference. */
    NEED_FACTORY(); FT_ANY(ft); IDWriteFontFile *file = NULL;
    CHECK_HR(IDWriteFactory_CreateFontFileReference(f, L"Q:\\no\\such\\dir\\eh_deferred.ttf", &ft, &file), S_OK); CHECK(file != NULL);
    DW_RELEASE(file);
    DONE_FACTORY();
}
TEST(path_missing_file_with_time_fails_at_face_creation) {
    NEED_FACTORY(); char p[MAX_PATH]; FT_ANY(ft); IDWriteFontFile *file = NULL; IDWriteFontFace *face = SENT(IDWriteFontFace *);
    t_tmpfile(p, sizeof p, "eh_deferred_missing.ttf");
    CHECK_HR(ref_a(f, p, &ft, &file), S_OK);
    if (file) { CHECK_HR(face_of(f, file, &face), EH_FILENOTFOUND); CHECK_OUT_NULL(face); }
    DW_RELEASE(file); DONE_FACTORY();
}
TEST(path_missing_dir_with_time_fails_at_face_creation) {
    NEED_FACTORY(); char p[MAX_PATH]; FT_ANY(ft); IDWriteFontFile *file = NULL; IDWriteFontFace *face = SENT(IDWriteFontFace *);
    t_tmpfile(p, sizeof p, "eh_deferred_dir\\x.ttf");
    CHECK_HR(ref_a(f, p, &ft, &file), S_OK);
    if (file) { CHECK_HR(face_of(f, file, &face), EH_FILENOTFOUND); CHECK_OUT_NULL(face); }
    DW_RELEASE(file); DONE_FACTORY();
}
TEST(path_analyze_missing_file_reports_failure_or_unsupported) {
    NEED_FACTORY(); char p[MAX_PATH]; FT_ANY(ft); IDWriteFontFile *file = NULL;
    t_tmpfile(p, sizeof p, "eh_analyze_missing.ttf");
    if (SUCCEEDED(ref_a(f, p, &ft, &file)) && file) {
        BOOL sup = 9; DWRITE_FONT_FILE_TYPE ft2 = (DWRITE_FONT_FILE_TYPE)9; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)9; UINT32 n = 9;
        HRESULT hr = IDWriteFontFile_Analyze(file, &sup, &ft2, &fc, &n);
        t_note("Analyze(missing file) = %s supported=%d type=%d face=%d n=%u", dw_hr_name(hr), sup, ft2, fc, n);
        CHECK(FAILED(hr) || !sup);
        CHECK(!sup); EXPECT_EQ(n, 0);
        DW_RELEASE(file);
    }
    DONE_FACTORY();
}
TEST(path_baseline_file_on_disk_works) {
    NEED_FACTORY(); char p[MAX_PATH]; size_t n; uint8_t *b = eh_blob(&n); IDWriteFontFile *file = NULL; IDWriteFontFace *face = NULL;
    t_tmpfile(p, sizeof p, "eh_ok.ttf");
    if (b && write_file(p, b, n)) {
        CHECK_HR(ref_a(f, p, NULL, &file), S_OK);
        if (file) { CHECK_HR(face_of(f, file, &face), S_OK); CHECK(face != NULL); }
    } else t_skip("cannot write scratch file");
    DW_RELEASE(face); DW_RELEASE(file); free(b); DONE_FACTORY();
}
TEST(path_exclusively_locked_file_is_fileaccess) {
    /* ERROR_SHARING_VIOLATION (32) is mapped by MapFileOpenError to DWRITE_E_FILEACCESS, not to HRESULT_FROM_WIN32(32). */
    NEED_FACTORY(); char p[MAX_PATH]; size_t n; uint8_t *b = eh_blob(&n); FT_ANY(ft); IDWriteFontFile *file = NULL; IDWriteFontFace *face = SENT(IDWriteFontFace *);
    t_tmpfile(p, sizeof p, "eh_locked.ttf");
    if (b && write_file(p, b, n)) {
        HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CHECK_HR(ref_a(f, p, &ft, &file), S_OK);
            if (file) { CHECK_HR(face_of(f, file, &face), EH_FILEACCESS); CHECK_OUT_NULL(face); }
            DW_RELEASE(file);
            IDWriteFontFile *file2 = SENT(IDWriteFontFile *);
            HRESULT hr = ref_a(f, p, NULL, &file2);
            t_note("CreateFontFileReference(exclusively locked file, no time) = %s", dw_hr_name(hr));
            if (FAILED(hr)) { CHECK_OUT_NULL(file2); CHECK_HR(hr, EH_FILEACCESS); } else DW_RELEASE(file2);
            CloseHandle(h);
            /* once the lock is gone the same path works */
            file = NULL; face = NULL;
            CHECK_HR(ref_a(f, p, NULL, &file), S_OK);
            if (file) { CHECK_HR(face_of(f, file, &face), S_OK); }
        } else t_skip("cannot lock scratch file");
    } else t_skip("cannot write scratch file");
    DW_RELEASE(face); DW_RELEASE(file); free(b); DONE_FACTORY();
}
TEST(path_zero_length_file_fails) {
    NEED_FACTORY(); char p[MAX_PATH]; IDWriteFontFile *file = NULL; IDWriteFontFace *face = SENT(IDWriteFontFace *); FT_ANY(ft);
    t_tmpfile(p, sizeof p, "eh_empty.ttf");
    if (write_file(p, "", 0)) {
        CHECK_HR(ref_a(f, p, &ft, &file), S_OK);
        if (file) {
            HRESULT hr = face_of(f, file, &face);
            t_note("CreateFontFace(zero length file) = %s", dw_hr_name(hr));
            CHECK(FAILED(hr)); CHECK_OUT_NULL(face);
        }
    } else t_skip("cannot write scratch file");
    DW_RELEASE(file); DONE_FACTORY();
}
TEST(path_deleted_after_reference_fails_then_recovers) {
    NEED_FACTORY(); char p[MAX_PATH]; size_t n; uint8_t *b = eh_blob(&n); IDWriteFontFile *file = NULL; IDWriteFontFace *face = SENT(IDWriteFontFace *); FT_ANY(ft);
    t_tmpfile(p, sizeof p, "eh_deleted.ttf");
    if (b) {
        CHECK_HR(ref_a(f, p, &ft, &file), S_OK);
        if (file) {
            CHECK_HR(face_of(f, file, &face), EH_FILENOTFOUND); CHECK_OUT_NULL(face);
            if (write_file(p, b, n)) {
                face = NULL; HRESULT hr = face_of(f, file, &face);
                t_note("CreateFontFace after the file appeared (reference carries a stale time) = %s", dw_hr_name(hr));
                DW_RELEASE(face);
                DeleteFileA(p);
            }
        }
    }
    DW_RELEASE(file); free(b); DONE_FACTORY();
}

/* ---- corrupt font data through the in-memory loader ---------------------------------------------------------------------------- */
static HRESULT face_from_blob(IDWriteFactory *f, const void *blob, size_t n, DWRITE_FONT_FACE_TYPE type, UINT32 index, IDWriteFontFace **face) {
    dwx_mem_loader *ld = dwx_loader_new(); IDWriteFontFile *file = NULL; HRESULT hr;
    UINT32 key = dwx_loader_add(ld, blob ? blob : "", n);
    hr = dwx_loader_register(ld, f);
    if (SUCCEEDED(hr)) hr = dwx_loader_file(ld, f, key, &file);
    if (SUCCEEDED(hr)) hr = IDWriteFactory_CreateFontFace(f, type, 1, &file, index, DWRITE_FONT_SIMULATIONS_NONE, face);
    DW_RELEASE(file); dwx_loader_unregister(ld); dwx_release(ld);
    return hr;
}
TEST(corrupt_baseline_blob_opens) {
    NEED_FACTORY(); size_t n; uint8_t *b = eh_blob(&n); IDWriteFontFace *face = NULL;
    CHECK_HR(face_from_blob(f, b, n, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, &face), S_OK); CHECK(face != NULL);
    DW_RELEASE(face); free(b); DONE_FACTORY();
}
TEST(corrupt_truncated_header_is_fileformat) {
    /* The sfnt header (12 bytes) and the table directory are read through CheckedPtr<..., FailAsExceptionPolicy<FileFormatException>>. */
    NEED_FACTORY(); size_t n; uint8_t *b = eh_blob(&n); static const size_t cuts[] = { 0, 1, 3, 4, 8, 11, 12, 13, 27, 28, 60, 100, 150 };
    unsigned i;
    for (i = 0; i < sizeof cuts / sizeof cuts[0] && b; i++) {
        IDWriteFontFace *face = SENT(IDWriteFontFace *); size_t m;
        uint8_t *c = ttf_corrupt(b, n, TTF_CORRUPT_TRUNCATE, cuts[i], &m);
        HRESULT hr = face_from_blob(f, c, m, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, &face);
        CHECKF(hr == EH_FILEFORMAT && face == NULL, "cut at %zu -> %s face=%p", cuts[i], dw_hr_name(hr), face);
        if (face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
        free(c);
    }
    free(b); DONE_FACTORY();
}
TEST(corrupt_garbage_blobs_are_fileformat) {
    NEED_FACTORY(); unsigned k;
    for (k = 0; k < 4; k++) {
        size_t sizes[] = { 64, 4096, 20000, 1 }; size_t n = sizes[k]; uint8_t *g = malloc(n); size_t i; IDWriteFontFace *face = SENT(IDWriteFontFace *);
        for (i = 0; i < n; i++) g[i] = (k == 0) ? 0x00 : (k == 1) ? 0xFF : (k == 2) ? (uint8_t)(i * 131u + 7u) : 0x41;
        HRESULT hr = face_from_blob(f, g, n, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, &face);
        CHECKF(hr == EH_FILEFORMAT && face == NULL, "garbage kind %u (%zu bytes) -> %s", k, n, dw_hr_name(hr));
        if (face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
        free(g);
    }
    DONE_FACTORY();
}
TEST(corrupt_text_file_is_fileformat) {
    NEED_FACTORY(); static const char txt[] = "This is not a font. It is a sentence that is a little longer than an sfnt header.\r\n"; IDWriteFontFace *face = SENT(IDWriteFontFace *);
    CHECK_HR(face_from_blob(f, txt, sizeof txt, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, &face), EH_FILEFORMAT); CHECK_OUT_NULL(face);
    DONE_FACTORY();
}
typedef struct { int kind; size_t param; const char *what; } corr_row;
TEST(corrupt_structural_damage_yields_ok_or_fileformat) {
    /* Damage a good font in structured ways. Which damages dwrite tolerates at face creation is table-specific, so the exact outcome is
     * recorded; the hard rule is: success, or DWRITE_E_FILEFORMAT with the out pointer cleared (never another code, never a crash). */
    NEED_FACTORY(); size_t n; uint8_t *b = eh_blob(&n); unsigned i, t;
    static const corr_row rows[] = {
        { TTF_CORRUPT_BAD_MAGIC, 0, "bad sfnt version" }, { TTF_CORRUPT_NUMTABLES, 0, "numTables=0" }, { TTF_CORRUPT_NUMTABLES, 1, "numTables=1" },
        { TTF_CORRUPT_NUMTABLES, 0xFFFF, "numTables=65535" }, { TTF_CORRUPT_HEAD_MAGIC, 0, "head magic" },
    };
    for (i = 0; i < sizeof rows / sizeof rows[0] && b; i++) {
        IDWriteFontFace *face = SENT(IDWriteFontFace *); size_t m; uint8_t *c = ttf_corrupt(b, n, rows[i].kind, rows[i].param, &m);
        HRESULT hr = face_from_blob(f, c, m, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, &face);
        t_note("%s -> %s", rows[i].what, dw_hr_name(hr));
        CHECKF(hr == S_OK || hr == EH_FILEFORMAT, "%s -> %s", rows[i].what, dw_hr_name(hr));
        if (FAILED(hr)) CHECK_OUT_NULL(face);
        if (face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
        free(c);
    }
    for (t = 0; t < 10 && b && !t_is_wine(); t++) {   /* the emulated parser faults on some damaged directories */
        static const int kinds[] = { TTF_CORRUPT_TABLE_OFFSET, TTF_CORRUPT_TABLE_LENGTH, TTF_CORRUPT_ZERO_TABLE, TTF_CORRUPT_ZERO_FILL, TTF_CORRUPT_TABLE_CHECKSUM };
        static const char *const kn[] = { "table offset past EOF", "table length past EOF", "table length 0", "table zero-filled", "checksum flipped" };
        for (i = 0; i < 5; i++) {
            IDWriteFontFace *face = SENT(IDWriteFontFace *); size_t m; uint8_t *c = ttf_corrupt(b, n, kinds[i], t, &m);
            HRESULT hr = face_from_blob(f, c, m, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, &face);
            t_note("table #%u %s -> %s", t, kn[i], dw_hr_name(hr));
            CHECKF(hr == S_OK || hr == EH_FILEFORMAT, "table #%u %s -> %s", t, kn[i], dw_hr_name(hr));
            if (FAILED(hr)) CHECK_OUT_NULL(face);
            if (face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
            free(c);
        }
    }
    free(b); DONE_FACTORY();
}
TEST(corrupt_checksum_flip_is_not_validated) {
    NEED_FACTORY(); size_t n, m; uint8_t *b = eh_blob(&n); IDWriteFontFace *face = NULL; unsigned t; int ok = 0, tot = 0;
    for (t = 0; t < 10 && b; t++) {
        uint8_t *c = ttf_corrupt(b, n, TTF_CORRUPT_TABLE_CHECKSUM, t, &m); IDWriteFontFace *fc = NULL;
        HRESULT hr = face_from_blob(f, c, m, DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, &fc);
        tot++; if (SUCCEEDED(hr)) ok++;
        DW_RELEASE(fc); free(c);
    }
    t_note("%d of %d checksum-damaged fonts opened", ok, tot);
    CHECK(ok == tot);       /* the directory checksums are advisory: dwrite never verifies them */
    DW_RELEASE(face); free(b); DONE_FACTORY();
}
TEST(corrupt_ttc_member_index_out_of_range) {
    NEED_FACTORY(); size_t n, m; uint8_t *a = eh_blob(&n); uint8_t *b2 = ttf_build_styled("EhTwo", 700, 5, 0, &m);
    const uint8_t *blobs[2] = { a, b2 }; size_t sizes[2] = { n, m }; size_t tn; uint8_t *ttc = ttf_build_ttc(blobs, sizes, 2, &tn);
    IDWriteFontFace *face = NULL; UINT32 idx;
    for (idx = 0; idx < 2 && ttc; idx++) { face = NULL; CHECK_HR(face_from_blob(f, ttc, tn, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, idx, &face), S_OK); DW_RELEASE(face); }
    for (idx = 2; idx < 5 && ttc; idx++) {
        face = SENT(IDWriteFontFace *);
        HRESULT hr = face_from_blob(f, ttc, tn, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, idx, &face);
        t_note("TTC member %u of 2 -> %s", idx, dw_hr_name(hr));
        CHECKF(FAILED(hr), "member %u accepted", idx); CHECK_OUT_NULL(face);
        CHECKF(hr == EH_FILEFORMAT || hr == E_INVALIDARG, "member %u -> %s", idx, dw_hr_name(hr));
    }
    free(ttc); free(a); free(b2); DONE_FACTORY();
}

/* ---- CreateFontFace argument validation (checked before any file access) ------------------------------------------------------ */
typedef struct { DWRITE_FONT_FACE_TYPE type; UINT32 count; UINT32 index; UINT32 sim; int null_array; int null_elem; const char *why; } cff_row;
static const cff_row cff_bad[] = {
    { DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, 0, 0, 0, 0, "fileCount 0" }, { DWRITE_FONT_FACE_TYPE_TRUETYPE, 2, 0, 0, 0, 0, "fileCount 2" },
    { DWRITE_FONT_FACE_TYPE_TRUETYPE, 0xFFFFFFFF, 0, 0, 0, 0, "fileCount -1" }, { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 0, 0, 1, 0, "NULL array" },
    { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 0, 0, 0, 1, "NULL file" },
    { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 1, 0, 0, 0, "faceIndex 1 for a single TrueType face type" },
    { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 0xFFFFFFFF, 0, 0, 0, "faceIndex -1" }, { DWRITE_FONT_FACE_TYPE_CFF, 1, 1, 0, 0, 0, "faceIndex 1 for CFF" },
    { DWRITE_FONT_FACE_TYPE_RAW_CFF, 1, 1, 0, 0, 0, "faceIndex 1 for RAW_CFF" }, { DWRITE_FONT_FACE_TYPE_UNKNOWN, 1, 1, 0, 0, 0, "faceIndex 1 for UNKNOWN" },
    { DWRITE_FONT_FACE_TYPE_TYPE1, 1, 0, 0, 0, 0, "TYPE1" }, { DWRITE_FONT_FACE_TYPE_VECTOR, 1, 0, 0, 0, 0, "VECTOR" },
    { DWRITE_FONT_FACE_TYPE_BITMAP, 1, 0, 0, 0, 0, "BITMAP" }, { (DWRITE_FONT_FACE_TYPE)8, 1, 0, 0, 0, 0, "type 8" },
    { (DWRITE_FONT_FACE_TYPE)0x100, 1, 0, 0, 0, 0, "type 0x100" }, { (DWRITE_FONT_FACE_TYPE)-1, 1, 0, 0, 0, 0, "type -1" },
    { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 0, 4, 0, 0, "simulations 4" }, { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 0, 8, 0, 0, "simulations 8" },
    { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 0, 0x80000000u, 0, 0, "simulations 0x80000000" }, { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 0, 0xFFFFFFFFu, 0, 0, "simulations -1" },
    { DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, 0, 5, 0, 0, "simulations 5" }, { DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION, 1, 0, 4, 0, 0, "collection with simulations 4" },
};
TEST(createfontface_invalid_arguments) {
    NEED_FACTORY(); size_t n; uint8_t *b = eh_blob(&n); dwx_mem_loader *ld = dwx_loader_new(); IDWriteFontFile *file = NULL; unsigned i;
    UINT32 key = dwx_loader_add(ld, b, n);
    if (SUCCEEDED(dwx_loader_register(ld, f)) && SUCCEEDED(dwx_loader_file(ld, f, key, &file))) {
        for (i = 0; i < sizeof cff_bad / sizeof cff_bad[0]; i++) {
            const cff_row *r = &cff_bad[i]; if (t_is_wine() && (r->null_array || r->null_elem || r->count != 1)) continue;
            IDWriteFontFace *face = SENT(IDWriteFontFace *); IDWriteFontFile *arr[2] = { file, file };
            HRESULT hr = IDWriteFactory_CreateFontFace(f, r->type, r->count, r->null_array ? NULL : (r->null_elem ? (IDWriteFontFile *const *)&(IDWriteFontFile *){ NULL } : arr), r->index, (DWRITE_FONT_SIMULATIONS)r->sim, &face);
            CHECKF(hr == E_INVALIDARG && face == NULL, "%s -> %s face=%p", r->why, dw_hr_name(hr), face);
            if (face && face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
        }
        EXPECT_EQ(ld->n_create, 0);       /* validation fails before the file is touched */
    }
    DW_RELEASE(file); dwx_loader_unregister(ld); dwx_release(ld); free(b); DONE_FACTORY();
}
TEST(createfontface_valid_simulations) {
    NEED_FACTORY(); size_t n; uint8_t *b = eh_blob(&n); dwx_mem_loader *ld = dwx_loader_new(); IDWriteFontFile *file = NULL; UINT32 key = dwx_loader_add(ld, b, n); unsigned s;
    if (SUCCEEDED(dwx_loader_register(ld, f)) && SUCCEEDED(dwx_loader_file(ld, f, key, &file))) {
        for (s = 0; s < 4; s++) {
            IDWriteFontFace *face = NULL;
            CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, (DWRITE_FONT_SIMULATIONS)s, &face), S_OK);
            if (face) EXPECT_EQ(IDWriteFontFace_GetSimulations(face), s);
            DW_RELEASE(face);
        }
    }
    DW_RELEASE(file); dwx_loader_unregister(ld); dwx_release(ld); free(b); DONE_FACTORY();
}
TEST(createfontface_type_mismatch_is_reported) {
    /* Face types that pass argument validation but do not describe the data: failure (not a crash) with the out pointer cleared. */
    NEED_FACTORY(); size_t n; uint8_t *b = eh_blob(&n); static const DWRITE_FONT_FACE_TYPE types[] = { DWRITE_FONT_FACE_TYPE_CFF, DWRITE_FONT_FACE_TYPE_RAW_CFF, DWRITE_FONT_FACE_TYPE_UNKNOWN, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION };
    unsigned i;
    for (i = 0; i < sizeof types / sizeof types[0]; i++) {
        IDWriteFontFace *face = SENT(IDWriteFontFace *);
        HRESULT hr = face_from_blob(f, b, n, types[i], 0, &face);
        t_note("TrueType data opened as face type %d -> %s", types[i], dw_hr_name(hr));
        if (FAILED(hr)) { CHECK_OUT_NULL(face); CHECKF(hr == EH_FILEFORMAT || hr == EH_UNSUPPORTEDOPERATION || hr == E_INVALIDARG, "type %d -> %s", types[i], dw_hr_name(hr)); }
        else if (face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
    }
    free(b); DONE_FACTORY();
}

/* ---- DWRITE_E_NOFONT -------------------------------------------------------------------------------------------------------- */
TEST(gdi_createfontfromlogfont_unknown_family_is_nofont) {
    NEED_FACTORY(); IDWriteGdiInterop *g = NULL; LOGFONTW lf; IDWriteFont *font = SENT(IDWriteFont *);
    CHECK_HR(IDWriteFactory_GetGdiInterop(f, &g), S_OK);
    if (g) {
        memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lf.lfFaceName, L"Zq No Such Family Qz");
        CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font), EH_NOFONT); CHECK_OUT_NULL(font);   /* FindNearestMatchingWin32Font fails -> 0x88985002 */
        font = SENT(IDWriteFont *);
        lstrcpyW(lf.lfFaceName, L"Another Missing Family 12345"); lf.lfWeight = FW_BOLD; lf.lfItalic = TRUE;
        CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font), EH_NOFONT); CHECK_OUT_NULL(font);
        IDWriteGdiInterop_Release(g);
    }
    DONE_FACTORY();
}
TEST(gdi_createfontfromlogfont_null_is_invalidarg) {
    NEED_FACTORY(); IDWriteGdiInterop *g = NULL; IDWriteFont *font = SENT(IDWriteFont *);
    CHECK_HR(IDWriteFactory_GetGdiInterop(f, &g), S_OK);
    if (g) { CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(g, NULL, &font), E_INVALIDARG); CHECK_OUT_NULL(font); IDWriteGdiInterop_Release(g); }
    DONE_FACTORY();
}
TEST(gdi_createfontfromlogfont_name_not_in_custom_collection) {
    NEED_FACTORY(); IDWriteGdiInterop *g = NULL; IDWriteGdiInterop1 *g1 = NULL; dwx_font h; memset(&h, 0, sizeof h);
    CHECK_HR(IDWriteFactory_GetGdiInterop(f, &g), S_OK);
    if (g && SUCCEEDED(IDWriteGdiInterop_QueryInterface(g, &IID_IDWriteGdiInterop1, (void **)&g1)) && g1) {
        IDWriteFontCollection *sys = NULL; LOGFONTW lf; IDWriteFont *font = SENT(IDWriteFont *);
        memset(&lf, 0, sizeof lf); lf.lfHeight = -16; lf.lfWeight = FW_NORMAL; lf.lfCharSet = DEFAULT_CHARSET; lstrcpyW(lf.lfFaceName, L"Zq No Such Family Qz");
        if (SUCCEEDED(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE)) && sys) {
            CHECK_HR(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, &lf, sys, &font), EH_NOFONT); CHECK_OUT_NULL(font);
            DW_RELEASE(sys);
        }
        font = SENT(IDWriteFont *);
        CHECK_HR(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, NULL, NULL, &font), E_INVALIDARG); CHECK_OUT_NULL(font);
        IDWriteGdiInterop1_Release(g1);
    } else t_skip("IDWriteGdiInterop1 not available");
    DW_RELEASE(g); DONE_FACTORY();
}
TEST(gdi_getmatchingfontsbylogfont_null_arguments) {
    NEED_FACTORY(); IDWriteGdiInterop *g = NULL; IDWriteGdiInterop1 *g1 = NULL;
    CHECK_HR(IDWriteFactory_GetGdiInterop(f, &g), S_OK);
    if (g && SUCCEEDED(IDWriteGdiInterop_QueryInterface(g, &IID_IDWriteGdiInterop1, (void **)&g1)) && g1) {
        IDWriteFactory3 *f3 = NULL; IDWriteFontSet *set = NULL; IDWriteFontSet *sub = SENT(IDWriteFontSet *); LOGFONTW lf; memset(&lf, 0, sizeof lf);
        if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) && SUCCEEDED(IDWriteFactory3_GetSystemFontSet(f3, &set)) && set) {
            CHECK_HR(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(g1, NULL, set, &sub), E_INVALIDARG); CHECK_OUT_NULL(sub);
            sub = SENT(IDWriteFontSet *);
            CHECK_HR(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(g1, &lf, NULL, &sub), E_INVALIDARG); CHECK_OUT_NULL(sub);
        } else t_skip("system font set unavailable");
        DW_RELEASE(set); DW_RELEASE(f3); IDWriteGdiInterop1_Release(g1);
    } else t_skip("IDWriteGdiInterop1 not available");
    DW_RELEASE(g); DONE_FACTORY();
}
TEST(gdi_convert_null_arguments) {
    NEED_FACTORY(); IDWriteGdiInterop *g = NULL; LOGFONTW lf; BOOL sys = 7; size_t i;
    CHECK_HR(IDWriteFactory_GetGdiInterop(f, &g), S_OK);
    if (g) {
        memset(&lf, 0xAA, sizeof lf);
        CHECK_HR(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(g, NULL, &lf), E_INVALIDARG);
        for (i = 0; i < sizeof lf; i++) if (((uint8_t *)&lf)[i]) { CHECKF(0, "LOGFONT byte %zu not cleared", i); break; }
        memset(&lf, 0xAA, sizeof lf);
        CHECK_HR(IDWriteGdiInterop_ConvertFontToLOGFONT(g, NULL, &lf, &sys), E_INVALIDARG);
        EXPECT_EQ(sys, 0);
        for (i = 0; i < sizeof lf; i++) if (((uint8_t *)&lf)[i]) { CHECKF(0, "LOGFONT byte %zu not cleared", i); break; }
        IDWriteGdiInterop_Release(g);
    }
    DONE_FACTORY();
}
TEST(gdi_createfontfacefromhdc_invalid_arguments) {
    NEED_FACTORY(); IDWriteGdiInterop *g = NULL; IDWriteFontFace *face = SENT(IDWriteFontFace *);
    CHECK_HR(IDWriteFactory_GetGdiInterop(f, &g), S_OK);
    if (g) {
        CHECK_HR(IDWriteGdiInterop_CreateFontFaceFromHdc(g, NULL, &face), E_INVALIDARG); CHECK_OUT_NULL(face);
        HDC dc = CreateCompatibleDC(NULL);
        if (dc && !t_is_wine()) CHECK_HR(IDWriteGdiInterop_CreateFontFaceFromHdc(g, dc, NULL), E_INVALIDARG);   /* NULL out is rejected, not dereferenced */
        if (dc) DeleteDC(dc);
        IDWriteGdiInterop_Release(g);
    }
    DONE_FACTORY();
}
TEST(gdi_createfontfacefromhdc_stock_bitmap_font) {
    NEED_FACTORY(); IDWriteGdiInterop *g = NULL; IDWriteFontFace *face = SENT(IDWriteFontFace *);
    CHECK_HR(IDWriteFactory_GetGdiInterop(f, &g), S_OK);
    HDC dc = CreateCompatibleDC(NULL);
    if (g && dc) {
        HGDIOBJ old = SelectObject(dc, GetStockObject(SYSTEM_FONT));
        HRESULT hr = IDWriteGdiInterop_CreateFontFaceFromHdc(g, dc, &face);
        t_note("CreateFontFaceFromHdc(stock SYSTEM_FONT) = %s", dw_hr_name(hr));
        if (FAILED(hr)) CHECK_OUT_NULL(face); else if (face != SENT(IDWriteFontFace *)) DW_RELEASE(face);
        SelectObject(dc, old);
    }
    if (dc) DeleteDC(dc);
    DW_RELEASE(g); DONE_FACTORY();
}
TEST(collection_getfontfromfontface_foreign_face_is_nofont) {
    NEED_FACTORY(); IDWriteFontCollection *sys = NULL; IDWriteFont *font = SENT(IDWriteFont *);
    NEED_FACE(h, f);
    if (SUCCEEDED(IDWriteFactory_GetSystemFontCollection(f, &sys, FALSE)) && sys) {
        /* A face built from an application loader is not part of the system collection: FindFontListIndexByPhysicalKey fails twice -> NOFONT. */
        CHECK_HR(IDWriteFontCollection_GetFontFromFontFace(sys, h.face, &font), EH_NOFONT); CHECK_OUT_NULL(font);
        DW_RELEASE(sys);
    }
    dwx_font_close(&h); DONE_FACTORY();
}
TEST(collection_custom_getfontfromfontface_roundtrip_and_foreign) {
    NEED_FACTORY(); dwx_mem_loader *files = dwx_loader_new(); dwx_coll_loader *cl = dwx_coll_new(files); size_t n; uint8_t *b = eh_blob(&n);
    UINT32 key = dwx_loader_add(files, b, n); IDWriteFontCollection *c = NULL; IDWriteFontFace *face = NULL; IDWriteFontFile *file = NULL;
    dwx_coll_add_key(cl, key);
    if (SUCCEEDED(dwx_loader_register(files, f)) && SUCCEEDED(dwx_coll_register(cl, f)) && SUCCEEDED(dwx_coll_create(cl, f, &c)) && c) {
        CHECK_HR(dwx_loader_file(files, f, key, &file), S_OK);
        if (file) CHECK_HR(IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face), S_OK);
        if (face) {
            IDWriteFont *font = NULL; HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(c, face, &font);
            CHECK_HR(hr, S_OK); DW_RELEASE(font);
            /* the same face with simulations is a different physical key variant of the same file: still a member or NOFONT, never a crash */
        }
        /* a face over a different file (not enumerated by the collection) is foreign to it */
        {
            dwx_font h; memset(&h, 0, sizeof h);
            size_t m; uint8_t *b2 = ttf_build_styled("EhForeign", 400, 5, 0, &m);
            if (b2 && SUCCEEDED(dwx_font_open(&h, f, b2, m, 0, DWRITE_FONT_SIMULATIONS_NONE)) && h.face) {
                IDWriteFont *font2 = SENT(IDWriteFont *);
                HRESULT hr2 = IDWriteFontCollection_GetFontFromFontFace(c, h.face, &font2);
                CHECK_HR(hr2, EH_NOFONT); CHECK_OUT_NULL(font2);
            }
            dwx_font_close(&h); free(b2);
        }
    } else t_skip("custom collection unavailable");
    DW_RELEASE(face); DW_RELEASE(file); DW_RELEASE(c); dwx_coll_unregister(cl); dwx_loader_unregister(files); dwx_release(cl); dwx_release(files); free(b); DONE_FACTORY();
}
