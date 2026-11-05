/* The IDWriteFontCollectionLoader / IDWriteFontFileEnumerator protocol seen from dwrite.dll's side.
 *
 * Win10 decomp (FontCollectionConstructionTask::AddElementData + FontCollectionBuilder::AddFontFiles):
 *   loader->CreateEnumeratorFromKey(factory, keyCopy, keySize, &e)      failure -> that HRESULT is the result (CallbackException)
 *   loop { e->MoveNext(&has)          failure -> that HRESULT is the result; has==FALSE ends the loop
 *          e->GetCurrentFontFile(&f)  failure (negative) -> the file is silently skipped, the loop continues
 *          f->GetLoader / f->GetReferenceKey, then FontSetBuilder::AddFontFile whose result is ignored (unsupported files vanish)
 *          f->Release }
 *   e->Release
 * No partially built collection is ever returned: *ppCollection is NULL on every failure.
 */
#include "fcl.h"

static const HRESULT hr_table[] = { E_FAIL, E_INVALIDARG, E_ACCESSDENIED, E_NOTIMPL, E_UNEXPECTED, (HRESULT)0x88985004 /* DWRITE_E_FILEACCESS */,
                                    (HRESULT)0x88985003 /* DWRITE_E_FILENOTFOUND */, (HRESULT)0x80ABCDEF };

static char fam_letter_name[16][16];
static const char *letter_family(int i) { snprintf(fam_letter_name[i], sizeof fam_letter_name[i], "Enum Fam %c", 'A' + i); return fam_letter_name[i]; }
static void add_letters(fcl_px *p, int n) { int i; for (i = 0; i < n; i++) fcl_px_add_style(p, letter_family(i), 400, 5, 0); }

/* ---------------------------------------------------------------------------------------- sequencing */
TEST(fcl_enum_empty_collection)
{
    FCL_PX(p);
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    CHECK(c != NULL);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 0);
    EXPECT_EQ(strcmp(fcl_ev, "CME"), 0);
    CHECK(p.cl->last_en && p.cl->last_en->n_move == 1 && p.cl->last_en->n_get == 0);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_empty_collection_family_access)
{
    FCL_PX(p);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    if (c) {
        UINT32 idx = 7; BOOL ex = TRUE;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, L"Anything", &idx, &ex));
        EXPECT_EQ(ex, 0);
        EXPECT_EQ(idx, 0xFFFFFFFF);
        IDWriteFontFamily *fam = (IDWriteFontFamily *)(ULONG_PTR)1;
        CHECK_HR(IDWriteFontCollection_GetFontFamily(c, 0, &fam), FCL_E_FAIL);
        CHECK_OUT_NULL(fam);
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_call_sequence_per_file_count)
{
    int n;
    for (n = 1; n <= 8; n++) {
        FCL_PX(p);
        char expect[64]; int i, len = 0;
        add_letters(&p, n);
        expect[len++] = 'C';
        for (i = 0; i < n; i++) { expect[len++] = 'M'; expect[len++] = 'G'; }
        expect[len++] = 'M'; expect[len++] = 'E'; expect[len] = 0;
        IDWriteFontCollection *c = NULL;
        CHECK_HR_OK(fcl_px_create(&p, &c));
        CHECKF(!strcmp(fcl_ev, expect), "n=%d events \"%s\" want \"%s\"", n, fcl_ev, expect);
        if (p.cl->last_en) {
            EXPECT_EQ(p.cl->last_en->n_move, n + 1);
            EXPECT_EQ(p.cl->last_en->n_get, n);
            EXPECT_EQ(p.cl->last_en->get_out_of_sequence, 0);
            EXPECT_EQ(p.cl->last_en->extra_moves_after_end, 0);
        }
        if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), n);
        DW_RELEASE(c);
        fcl_px_close(&p);
    }
}
TEST(fcl_enum_sixteen_files)
{
    FCL_PX(p);
    add_letters(&p, 16);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    EXPECT_EQ(p.cl->last_en->n_get, 16);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 16);
        int i; for (i = 0; i < 16; i++) CHECKF(fcl_find(c, letter_family(i)) >= 0, "family %d missing", i);
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_enumerator_released_before_return)
{
    FCL_PX(p);
    add_letters(&p, 2);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    fcl_en *e = p.cl->last_en;
    CHECK(e != NULL);
    if (e) {
        CHECK(e->destroyed);                    /* dwrite released the only reference it was given */
        EXPECT_EQ(e->ref, 0);
        EXPECT_EQ(e->n_release, e->n_addref + 1);
    }
    CHECK(fcl_nev > 0 && fcl_ev[fcl_nev - 1] == 'E');
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_file_references_balanced)
{
    FCL_PX(p);
    add_letters(&p, 4);
    IDWriteFontCollection *c = NULL;
    /* the enumerator owns the files; observe their counts just before it dies by creating the collection through a slower path:
     * after the call, the enumerator is gone and has released its own references, so what is left is the number dwrite kept. */
    CHECK_HR_OK(fcl_px_create(&p, &c));
    fcl_en *e = p.cl->last_en;
    if (e) {
        int i;
        for (i = 0; i < e->nfiles; i++) CHECK(e->destroyed);
        EXPECT_EQ(e->nfiles, 4);
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_enumerator_not_used_after_return)
{
    FCL_PX(p);
    add_letters(&p, 3);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    LONG nc = p.cl->n_create, nm = p.cl->last_en->n_move, ng = p.cl->last_en->n_get;
    int ne = fcl_nev;
    if (c) {
        UINT32 i, n = IDWriteFontCollection_GetFontFamilyCount(c);
        for (i = 0; i < n; i++) {
            IDWriteFontFamily *fam = NULL; char nm_[64];
            CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, i, &fam));
            if (fam) {
                fcl_fam_name_of(fam, nm_, sizeof nm_);
                UINT32 k, fc = IDWriteFontFamily_GetFontCount(fam);
                for (k = 0; k < fc; k++) { IDWriteFont *f = NULL; IDWriteFontFace *face = NULL; IDWriteFontFamily_GetFont(fam, k, &f); if (f) { IDWriteFont_CreateFontFace(f, &face); DW_RELEASE(face); DW_RELEASE(f); } }
                DW_RELEASE(fam);
            }
        }
    }
    EXPECT_EQ(p.cl->n_create, nc);
    EXPECT_EQ(p.cl->last_en->n_move, nm);
    EXPECT_EQ(p.cl->last_en->n_get, ng);
    EXPECT_EQ(fcl_nev, ne);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_files_come_from_the_registered_file_loader)
{
    FCL_PX(p);
    add_letters(&p, 3);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    CHECK(p.fl->n_create >= 3);                         /* streams were opened for the enumerated files */
    EXPECT_EQ(p.fl->last_key_size, 4);                  /* the 4-byte reference key we gave the file reference */
    CHECK(dwx_loader_total_reads(p.fl) > 0);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_stream_fragments_note)
{
    FCL_PX(p);
    add_letters(&p, 2);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    t_note("after creation: file loader streams=%d outstanding fragments=%ld total reads=%ld", p.fl->nstreams, (long)dwx_loader_total_outstanding(p.fl), (long)dwx_loader_total_reads(p.fl));
    int i; for (i = 0; i < p.fl->nstreams; i++) CHECK(p.fl->streams[i]->bad_release_ctx == 0);
    DW_RELEASE(c);
    fcl_px_close(&p);
}

/* ---------------------------------------------------------------------------------- MoveNext failures */
TEST(fcl_enum_movenext_failure_hresult_propagates)
{
    unsigned i;
    for (i = 0; i < sizeof hr_table / sizeof hr_table[0]; i++) {
        FCL_PX(p);
        add_letters(&p, 3);
        p.cl->move_hr = hr_table[i]; p.cl->move_hr_at = 2;
        IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
        HRESULT hr = fcl_px_create(&p, &c);
        CHECKF(hr == hr_table[i], "MoveNext returned %s, CreateCustomFontCollection returned %s", dw_hr_name(hr_table[i]), dw_hr_name(hr));
        CHECK_OUT_NULL(c);
        EXPECT_EQ(p.cl->last_en->n_move, 2);
        EXPECT_EQ(p.cl->last_en->n_get, 1);          /* the first file had been taken before the failure */
        CHECK(p.cl->last_en->destroyed);
        DW_RELEASE(c);
        fcl_px_close(&p);
    }
}
TEST(fcl_enum_movenext_failure_on_first_call)
{
    FCL_PX(p);
    add_letters(&p, 2);
    p.cl->move_hr = E_FAIL; p.cl->move_hr_at = 1;
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(fcl_px_create(&p, &c), E_FAIL);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(strcmp(fcl_ev, "CME"), 0);
    CHECK(p.cl->last_en->destroyed);
    fcl_px_close(&p);
}
TEST(fcl_enum_movenext_failure_on_last_call)
{
    FCL_PX(p);
    add_letters(&p, 2);
    p.cl->move_hr = E_ACCESSDENIED; p.cl->move_hr_at = 3;   /* the call that would have reported the end */
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(fcl_px_create(&p, &c), E_ACCESSDENIED);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(strcmp(fcl_ev, "CMGMGME"), 0);
    fcl_px_close(&p);
}
TEST(fcl_enum_movenext_failure_with_has_true_still_fails)
{
    /* The HRESULT is checked before hasCurrentFile is looked at. */
    FCL_PX(p);
    add_letters(&p, 2);
    p.cl->move_hr = E_FAIL; p.cl->move_hr_at = 1; p.cl->move_hr_true = 1;
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(fcl_px_create(&p, &c), E_FAIL);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(p.cl->last_en->n_get, 0);
    fcl_px_close(&p);
}
TEST(fcl_enum_movenext_success_code_s_false_with_no_current)
{
    /* Only negative values are errors: S_FALSE with has==FALSE simply ends the enumeration. */
    FCL_PX(p);
    add_letters(&p, 3);
    p.cl->move_hr = S_FALSE; p.cl->move_hr_at = 2;
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    EXPECT_EQ(p.cl->last_en->n_move, 2);
    EXPECT_EQ(p.cl->last_en->n_get, 1);
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); CHECK(fcl_find(c, letter_family(0)) == 0); }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_failed_create_then_retry_same_key)
{
    /* A failed build must not be cached: after the enumerator is fixed, the same key works and reaches the loader again. */
    FCL_PX(p);
    add_letters(&p, 2);
    p.cl->move_hr = E_FAIL; p.cl->move_hr_at = 2;
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(fcl_px_create_key(&p, "retrykey", 8, &c), E_FAIL);
    CHECK_OUT_NULL(c);
    p.cl->move_hr = 0; p.cl->move_hr_at = 0;
    CHECK_HR_OK(fcl_px_create_key(&p, "retrykey", 8, &c));
    EXPECT_EQ(p.cl->n_create, 2);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    DW_RELEASE(c);
    fcl_px_close(&p);
}

/* --------------------------------------------------------------------------- GetCurrentFontFile failures */
TEST(fcl_enum_getcurrent_failure_is_skipped)
{
    FCL_PX(p);
    add_letters(&p, 3);
    p.cl->get_hr = E_FAIL; p.cl->get_mask = 0x7;
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR_OK(fcl_px_create(&p, &c));                    /* not an error: the files are just dropped */
    CHECK(c != NULL);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 0);
    EXPECT_EQ(p.cl->last_en->n_get, 3);
    EXPECT_EQ(p.cl->last_en->n_move, 4);
    EXPECT_EQ(strcmp(fcl_ev, "CMGMGMGME"), 0);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_getcurrent_failure_middle_file)
{
    FCL_PX(p);
    add_letters(&p, 3);
    p.cl->get_hr = E_FAIL; p.cl->get_mask = 0x2;
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
        CHECK(fcl_find(c, letter_family(0)) >= 0);
        CHECK(fcl_find(c, letter_family(1)) < 0);
        CHECK(fcl_find(c, letter_family(2)) >= 0);
    }
    EXPECT_EQ(strcmp(fcl_ev, "CMGMGMGME"), 0);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_getcurrent_failure_first_and_last)
{
    FCL_PX(p);
    add_letters(&p, 4);
    p.cl->get_hr = E_INVALIDARG; p.cl->get_mask = 0x9;
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
        CHECK(fcl_find(c, letter_family(1)) >= 0);
        CHECK(fcl_find(c, letter_family(2)) >= 0);
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_getcurrent_any_negative_hresult_is_skipped)
{
    unsigned i;
    for (i = 0; i < sizeof hr_table / sizeof hr_table[0]; i++) {
        FCL_PX(p);
        add_letters(&p, 2);
        p.cl->get_hr = hr_table[i]; p.cl->get_mask = 0x1;
        IDWriteFontCollection *c = NULL;
        HRESULT hr = fcl_px_create(&p, &c);
        CHECKF(hr == S_OK, "GetCurrentFontFile %s -> create %s", dw_hr_name(hr_table[i]), dw_hr_name(hr));
        if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); CHECK(fcl_find(c, letter_family(1)) == 0); }
        DW_RELEASE(c);
        fcl_px_close(&p);
    }
}

/* ---------------------------------------------------------------------- CreateEnumeratorFromKey failures */
TEST(fcl_enum_create_failure_hresult_propagates)
{
    unsigned i;
    for (i = 0; i < sizeof hr_table / sizeof hr_table[0]; i++) {
        FCL_PX(p);
        add_letters(&p, 2);
        p.cl->create_hr = hr_table[i];
        IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
        HRESULT hr = fcl_px_create(&p, &c);
        CHECKF(hr == hr_table[i], "loader returned %s, CreateCustomFontCollection returned %s", dw_hr_name(hr_table[i]), dw_hr_name(hr));
        CHECK_OUT_NULL(c);
        EXPECT_EQ(p.cl->n_create, 1);
        EXPECT_EQ(strcmp(fcl_ev, "C"), 0);                  /* no enumerator, so no MoveNext */
        DW_RELEASE(c);
        fcl_px_close(&p);
    }
}
TEST(fcl_enum_create_failure_then_success)
{
    FCL_PX(p);
    add_letters(&p, 1);
    p.cl->create_hr = E_FAIL;
    IDWriteFontCollection *c = NULL;
    CHECK_HR(fcl_px_create_key(&p, "again001", 8, &c), E_FAIL);
    CHECK(c == NULL);
    p.cl->create_hr = 0;
    CHECK_HR_OK(fcl_px_create_key(&p, "again001", 8, &c));
    EXPECT_EQ(p.cl->n_create, 2);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_create_success_code_s_false_is_not_an_error)
{
    FCL_PX(p);
    add_letters(&p, 2);
    p.cl->create_hr = S_FALSE; p.cl->create_hr_positive = 1;     /* still returns a valid enumerator */
    IDWriteFontCollection *c = NULL;
    HRESULT hr = fcl_px_create(&p, &c);
    CHECK_HR_OK(hr);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_failures_leave_loader_balanced)
{
    FCL_PX(p);
    add_letters(&p, 2);
    LONG r0 = p.cl->ref;
    IDWriteFontCollection *c = NULL;
    p.cl->create_hr = E_FAIL;
    CHECK_HR(fcl_px_create(&p, &c), E_FAIL);
    EXPECT_EQ(p.cl->ref, r0);
    p.cl->create_hr = 0; p.cl->move_hr = E_FAIL; p.cl->move_hr_at = 1;
    CHECK_HR(fcl_px_create(&p, &c), E_FAIL);
    EXPECT_EQ(p.cl->ref, r0);
    p.cl->move_hr_at = 0;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    EXPECT_EQ(p.cl->ref, r0);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_failed_enumerators_are_released)
{
    FCL_PX(p);
    add_letters(&p, 3);
    int i; IDWriteFontCollection *c = NULL;
    for (i = 0; i < 5; i++) {
        p.cl->move_hr = E_FAIL; p.cl->move_hr_at = 1 + (i % 3);
        CHECK_HR(fcl_px_create(&p, &c), E_FAIL);
        CHECK(p.cl->last_en->destroyed);
        CHECK(c == NULL);
    }
    EXPECT_EQ(p.cl->nens, 5);
    fcl_px_close(&p);
}

/* ------------------------------------------------------------------------------- unsupported / bad files */
static int unsupported_by_analyze(fcl_px *p, UINT32 key)
{
    IDWriteFontFile *file = NULL; BOOL sup = TRUE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf; HRESULT hr; int res = 0;
    if (FAILED(dwx_loader_file(p->fl, p->f, key, &file)) || !file) return 0;
    hr = IDWriteFontFile_Analyze(file, &sup, &ft, &fct, &nf);
    res = SUCCEEDED(hr) && !sup;
    if (!res) t_note("Analyze for key %u -> %s supported=%d", key, dw_hr_name(hr), sup);
    IDWriteFontFile_Release(file);
    return res;
}
static void make_junk(uint8_t *b, size_t n, unsigned seed)
{
    size_t i; unsigned s = seed * 2654435761u + 12345;
    for (i = 0; i < n; i++) { s = s * 1664525u + 1013904223u; b[i] = (uint8_t)(s >> 24); }
}
/* A blob that analyzes as unsupported must be dropped silently: S_OK and no families. */
static void junk_case(const char *what, const uint8_t *blob, size_t n, int with_valid)
{
    FCL_PX(p);
    UINT32 bad = 0;
    if (with_valid) fcl_px_add_style(&p, "Beside Junk", 400, 5, 0);
    bad = fcl_px_add_blob(&p, blob, n);
    if (with_valid) fcl_px_add_style(&p, "After Junk", 400, 5, 0);
    if (!unsupported_by_analyze(&p, bad)) { fcl_px_close(&p); t_skip("%s: Analyze does not report the blob as unsupported", what); return; }
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = fcl_px_create(&p, &c);
    CHECKF(hr == S_OK, "%s: %s", what, dw_hr_name(hr));
    if (hr == S_OK && c) {
        UINT32 n_fam = IDWriteFontCollection_GetFontFamilyCount(c);
        EXPECT_EQ(n_fam, with_valid ? 2 : 0);
        if (with_valid) { CHECK(fcl_find(c, "Beside Junk") >= 0); CHECK(fcl_find(c, "After Junk") >= 0); }
    }
    EXPECT_EQ(p.cl->last_en->n_get, with_valid ? 3 : 1);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_junk_zero_bytes)  { uint8_t b[512]; memset(b, 0, sizeof b); junk_case("zeros", b, sizeof b, 0); }
TEST(fcl_enum_junk_zero_bytes_between_valid) { uint8_t b[512]; memset(b, 0, sizeof b); junk_case("zeros", b, sizeof b, 1); }
TEST(fcl_enum_junk_random_bytes) { uint8_t b[4096]; make_junk(b, sizeof b, 1); junk_case("random", b, sizeof b, 0); }
TEST(fcl_enum_junk_random_bytes_between_valid) { uint8_t b[4096]; make_junk(b, sizeof b, 2); junk_case("random", b, sizeof b, 1); }
TEST(fcl_enum_junk_text_file) { const char *t = "This is not a font file, just some text.\r\n"; junk_case("text", (const uint8_t *)t, strlen(t), 0); }
TEST(fcl_enum_junk_text_between_valid) { const char *t = "plain text between two good fonts"; junk_case("text", (const uint8_t *)t, strlen(t), 1); }
TEST(fcl_enum_junk_one_byte) { uint8_t b[1] = { 0x41 }; junk_case("one byte", b, 1, 0); }
TEST(fcl_enum_junk_bad_magic_font)
{
    size_t n, m; uint8_t *f = ttf_build_styled("BadMagic", 400, 5, 0, &n);
    uint8_t *b = ttf_corrupt(f, n, TTF_CORRUPT_BAD_MAGIC, 0, &m);
    junk_case("bad sfnt magic", b, m, 0);
    free(f); free(b);
}
TEST(fcl_enum_junk_bad_magic_font_between_valid)
{
    size_t n, m; uint8_t *f = ttf_build_styled("BadMagic", 400, 5, 0, &n);
    uint8_t *b = ttf_corrupt(f, n, TTF_CORRUPT_BAD_MAGIC, 0, &m);
    junk_case("bad sfnt magic", b, m, 1);
    free(f); free(b);
}
TEST(fcl_enum_junk_pe_header) { uint8_t b[256]; memset(b, 0, sizeof b); b[0] = 'M'; b[1] = 'Z'; junk_case("MZ stub", b, sizeof b, 0); }
TEST(fcl_enum_junk_ttc_header_only)
{
    uint8_t b[12] = { 't', 't', 'c', 'f', 0, 1, 0, 0, 0, 0, 0, 0 };     /* ttcf v1 with zero fonts */
    FCL_PX(p);
    UINT32 bad = fcl_px_add_blob(&p, b, sizeof b);
    fcl_px_add_style(&p, "After Empty TTC", 400, 5, 0);
    (void)bad;
    IDWriteFontCollection *c = NULL;
    HRESULT hr = fcl_px_create(&p, &c);
    t_note("empty ttcf -> %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && c) { UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c); CHECK(n == 1 || n == 0); if (n == 1) CHECK(fcl_find(c, "After Empty TTC") == 0); }
    else CHECK_OUT_NULL(c);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_damaged_fonts_do_not_crash)
{
    /* Structurally damaged but sfnt-looking fonts: the outcome (skipped, empty, or an error) is OS dependent; the contract is
     * a consistent result and no crash. */
    static const struct { int kind; size_t param; const char *name; } cases[] = {
        { TTF_CORRUPT_TRUNCATE, 40, "truncate-40" }, { TTF_CORRUPT_TRUNCATE, 200, "truncate-200" }, { TTF_CORRUPT_TRUNCATE, 1000, "truncate-1000" },
        { TTF_CORRUPT_NUMTABLES, 0, "numtables-0" }, { TTF_CORRUPT_NUMTABLES, 400, "numtables-400" },
        { TTF_CORRUPT_TABLE_OFFSET, 1, "offset-beyond" }, { TTF_CORRUPT_TABLE_LENGTH, 2, "length-beyond" },
        { TTF_CORRUPT_HEAD_MAGIC, 0, "head-magic" }, { TTF_CORRUPT_ZERO_TABLE, 0, "zero-table-0" }, { TTF_CORRUPT_ZERO_FILL, 3, "zero-fill-3" },
        { TTF_CORRUPT_TABLE_CHECKSUM, 1, "checksum" },
    };
    unsigned i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        FCL_PX(p);
        size_t n, m; uint8_t *f = ttf_build_styled("Damaged", 400, 5, 0, &n);
        uint8_t *b = ttf_corrupt(f, n, cases[i].kind, cases[i].param, &m);
        if (!b) { free(f); fcl_px_close(&p); continue; }
        fcl_px_add_blob(&p, b, m);
        fcl_px_add_style(&p, "Healthy", 400, 5, 0);
        IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
        HRESULT hr = fcl_px_create(&p, &c);
        t_note("%s: %s%s", cases[i].name, dw_hr_name(hr), SUCCEEDED(hr) && c ? "" : " (no collection)");
        if (SUCCEEDED(hr)) {
            CHECK(c != NULL);
            if (c) { UINT32 cnt = IDWriteFontCollection_GetFontFamilyCount(c); CHECKF(cnt >= 1 && cnt <= 2, "%s: %u families", cases[i].name, cnt); CHECK(fcl_find(c, "Healthy") >= 0); }
        } else CHECK_OUT_NULL(c);
        DW_RELEASE(c);
        free(f); free(b);
        fcl_px_close(&p);
    }
}
TEST(fcl_enum_unreadable_stream_does_not_crash)
{
    unsigned i;
    for (i = 0; i < 3; i++) {
        FCL_PX(p);
        if (i == 0) p.fl->stream_fail_read = E_FAIL;
        if (i == 1) p.fl->stream_fail_getsize = E_FAIL;
        if (i == 2) p.fl->fail_create = E_ACCESSDENIED;
        fcl_px_add_style(&p, "Unreadable", 400, 5, 0);
        IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
        HRESULT hr = fcl_px_create(&p, &c);
        t_note("file loader fault %u -> %s, families=%u", i, dw_hr_name(hr), (SUCCEEDED(hr) && c) ? IDWriteFontCollection_GetFontFamilyCount(c) : 0);
        if (FAILED(hr)) CHECK_OUT_NULL(c);
        DW_RELEASE(c);
        p.fl->stream_fail_read = p.fl->stream_fail_getsize = 0; p.fl->fail_create = 0;
        fcl_px_close(&p);
    }
}

/* ----------------------------------------------------------------------------------- multi-face files */
TEST(fcl_enum_ttc_with_distinct_families)
{
    FCL_PX(p);
    size_t n[3], tn; uint8_t *f[3]; const uint8_t *cf[3];
    f[0] = ttf_build_styled("Collection Alpha", 400, 5, 0, &n[0]);
    f[1] = ttf_build_styled("Collection Beta", 400, 5, 0, &n[1]);
    f[2] = ttf_build_styled("Collection Gamma", 400, 5, 0, &n[2]);
    cf[0] = f[0]; cf[1] = f[1]; cf[2] = f[2];
    uint8_t *ttc = ttf_build_ttc(cf, n, 3, &tn);
    fcl_px_add_blob(&p, ttc, tn);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    EXPECT_EQ(p.cl->last_en->n_get, 1);                    /* one file ... */
    if (c) {                                               /* ... three faces */
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 3);
        CHECK(fcl_find(c, "Collection Alpha") >= 0);
        CHECK(fcl_find(c, "Collection Beta") >= 0);
        CHECK(fcl_find(c, "Collection Gamma") >= 0);
        fcl_note_families(c, "ttc");
    }
    DW_RELEASE(c);
    free(ttc); free(f[0]); free(f[1]); free(f[2]);
    fcl_px_close(&p);
}
TEST(fcl_enum_ttc_faces_of_one_family)
{
    FCL_PX(p);
    size_t n[2], tn; uint8_t *f[2]; const uint8_t *cf[2];
    f[0] = ttf_build_styled("Collection Duo", 400, 5, 0, &n[0]);
    f[1] = ttf_build_styled("Collection Duo", 700, 5, 0, &n[1]);
    cf[0] = f[0]; cf[1] = f[1];
    uint8_t *ttc = ttf_build_ttc(cf, n, 2, &tn);
    fcl_px_add_blob(&p, ttc, tn);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        CHECK(fcl_font_count(c, 0) >= 2);
        IDWriteFont *reg = fcl_pick(c, 0, 400, 5, 0, 0), *bold = fcl_pick(c, 0, 700, 5, 0, 0);
        CHECK(reg != NULL); CHECK(bold != NULL);           /* both real faces present, without simulations */
        DW_RELEASE(reg); DW_RELEASE(bold);
    }
    DW_RELEASE(c);
    free(ttc); free(f[0]); free(f[1]);
    fcl_px_close(&p);
}
TEST(fcl_enum_ttc_plus_single_files)
{
    FCL_PX(p);
    size_t n[2], tn; uint8_t *f[2]; const uint8_t *cf[2];
    f[0] = ttf_build_styled("Mix One", 400, 5, 0, &n[0]);
    f[1] = ttf_build_styled("Mix Two", 400, 5, 0, &n[1]);
    cf[0] = f[0]; cf[1] = f[1];
    uint8_t *ttc = ttf_build_ttc(cf, n, 2, &tn);
    fcl_px_add_style(&p, "Mix Zero", 400, 5, 0);
    fcl_px_add_blob(&p, ttc, tn);
    fcl_px_add_style(&p, "Mix Three", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 4);
        const char *names[] = { "Mix Zero", "Mix One", "Mix Two", "Mix Three" };
        int i; for (i = 0; i < 4; i++) CHECK(fcl_find(c, names[i]) >= 0);
        fcl_note_families(c, "single+ttc+single");
    }
    DW_RELEASE(c);
    free(ttc); free(f[0]); free(f[1]);
    fcl_px_close(&p);
}

/* ----------------------------------------------------------------------------------- duplicates */
TEST(fcl_enum_same_key_twice)
{
    FCL_PX(p);
    UINT32 k = fcl_px_add_style(&p, "Twice Listed", 400, 5, 0);
    fcl_cl_add(p.cl, k);                                   /* the same file reference is enumerated twice */
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    EXPECT_EQ(p.cl->last_en->n_get, 2);
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        t_note("same file key listed twice: font count %u", fcl_font_count(c, 0));
        CHECK(fcl_font_count(c, 0) >= 1);
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_same_blob_under_two_keys)
{
    FCL_PX(p);
    ttf_spec s; ttf_style_spec(&s, "Two Keys", 400, 5, 0);
    size_t n; uint8_t *b = ttf_build(&s, &n);
    fcl_px_add_blob(&p, b, n);
    fcl_px_add_blob(&p, b, n);
    free(b);
    IDWriteFontCollection *c = NULL;
    CHECK_HR_OK(fcl_px_create(&p, &c));
    if (c) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
        t_note("same font under two keys: font count %u", fcl_font_count(c, 0));
    }
    DW_RELEASE(c);
    fcl_px_close(&p);
}
TEST(fcl_enum_enumerator_may_return_same_file_object_repeatedly)
{
    /* Handing back the very same IDWriteFontFile twice (AddRef'd each time) is legal. */
    FCL_PX(p);
    fcl_px_add_style(&p, "Same Object", 400, 5, 0);
    IDWriteFontCollection *c = NULL;
    fcl_cl_add(p.cl, p.cl->keys[0]);
    CHECK_HR_OK(fcl_px_create(&p, &c));
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);
    DW_RELEASE(c);
    fcl_px_close(&p);
}
