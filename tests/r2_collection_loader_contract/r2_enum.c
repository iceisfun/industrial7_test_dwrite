/* IDWriteFontFileEnumerator::MoveNext / GetCurrentFontFile as driven by dwrite.
 *
 * Win10 decomp (FontCollectionBuilder::AddFontFiles):
 *   for (;;) {
 *       hr = MoveNext(&has); ThrowIfFailed(hr);        // a failing HRESULT aborts the whole creation, `has` is not looked at
 *       if (has == 0) break;                           // any non-zero BOOL means "there is a current file"
 *       hr = GetCurrentFontFile(&file);
 *       if (hr >= 0) { file->GetLoader; file->GetReferenceKey; FontSetBuilder::AddFontFile(...); }   // a failing hr only skips the file
 *       if (file) file->Release();                      // also after a failure when *out was non-NULL
 *   }
 * MoveNext is never called again after it reported "no current file"; GetCurrentFontFile only after a successful MoveNext.
 */
#include "r2.h"

static void add_n(r2_fx *x, int n)
{
    int i;
    for (i = 0; i < n; i++) { char nm[32]; snprintf(nm, sizeof nm, "En%02d", i); r2_add_family(x, nm); }
}
static void fam_name(int i, char *b) { snprintf(b, 32, "En%02d", i); }

/* ------------------------------------------------------------------------- the BOOL MoveNext reports */
static void has_case(BOOL v)
{
    FX(x);
    add_n(&x, 3);
    x.cl->has_value = v;
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(x.cl->last_en->n_move, 4);
    EXPECT_EQ(x.cl->last_en->n_get, 3);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 3);
    DW_RELEASE(c);
    r2_close(&x);
}
#define HASCASE(tag, v) RT(movenext_has_current_value_##tag) { has_case((BOOL)(v)); }
HASCASE(1, 1) HASCASE(2, 2) HASCASE(0xff, 0xff) HASCASE(0x100, 0x100) HASCASE(0x10000, 0x10000) HASCASE(0x7fffffff, 0x7fffffff)
HASCASE(0x80000000, 0x80000000) HASCASE(minus_one, -1)

/* ------------------------------------------------------------------------- sequencing */
RT(movenext_is_not_called_again_after_the_end)
{
    int n;
    for (n = 0; n <= 5; n++) {
        FX(x);
        add_n(&x, n);
        IDWriteFontCollection *c = NULL;
        CHECK_HR(r2_create(&x, &c), S_OK);
        EXPECT_EQ(x.cl->last_en->n_move, n + 1);
        EXPECT_EQ(x.cl->last_en->moves_after_end, 0);
        DW_RELEASE(c);
        r2_close(&x);
    }
}
RT(getcurrent_is_only_called_after_a_successful_movenext_with_a_file)
{
    int n;
    for (n = 0; n <= 5; n++) {
        FX(x);
        add_n(&x, n);
        IDWriteFontCollection *c = NULL;
        CHECK_HR(r2_create(&x, &c), S_OK);
        EXPECT_EQ(x.cl->last_en->n_get, n);
        EXPECT_EQ(x.cl->last_en->gets_out_of_seq, 0);
        DW_RELEASE(c);
        r2_close(&x);
    }
}
RT(movenext_and_getcurrent_alternate_strictly)
{
    FX(x);
    add_n(&x, 6);
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = r2_n(), expect_move = 1, started = 0, ok = 1;
    for (i = 0; i < n; i++) {
        int k = r2_log[i].kind;
        if (k == EV_CL_CREATE) started = 1;
        if (!started) continue;
        if (k == EV_EN_MOVE) { if (!expect_move) ok = 0; expect_move = 0; }
        if (k == EV_EN_GET)  { if (expect_move) ok = 0; expect_move = 1; }
    }
    CHECK(ok);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(arguments_of_the_nth_call_are_the_nth_entries)
{
    FX(x);
    add_n(&x, 4);
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = r2_n(), moves = 0, gets = 0;
    for (i = 0; i < n; i++) {
        if (r2_log[i].kind == EV_EN_MOVE) { moves++; EXPECT_EQ(r2_log[i].a, moves); }
        if (r2_log[i].kind == EV_EN_GET)  { EXPECT_EQ(r2_log[i].b, gets); gets++; }   /* position of the entry handed out */
    }
    EXPECT_EQ(moves, 5);
    EXPECT_EQ(gets, 4);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(forty_files)
{
    FX(x);
    add_n(&x, 40);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(x.cl->last_en->n_move, 41);
    EXPECT_EQ(x.cl->last_en->n_get, 40);
    if (c) {
        int i; EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 40);
        for (i = 0; i < 40; i++) { char nm[32]; fam_name(i, nm); CHECKF(r2_find_family(c, nm) >= 0, "family %s missing", nm); }
    }
    DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- MoveNext failures */
static void move_fail_case(int at, BOOL has_on_fail, HRESULT hr)
{
    FX(x);
    add_n(&x, 4);
    x.cl->move_fail_at = at; x.cl->move_fail_hr = hr; x.cl->move_fail_has = has_on_fail;
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    LONG refs = x.cl->ref;
    HRESULT got = r2_create(&x, &c);
    CHECKF(got == hr, "at=%d has=%d: MoveNext returned %s, create returned %s", at, has_on_fail, dw_hr_name(hr), dw_hr_name(got));
    CHECK_OUT_NULL(c);
    EXPECT_EQ(x.cl->last_en->n_move, at);
    EXPECT_EQ(x.cl->last_en->n_get, at - 1);
    CHECK(x.cl->last_en->destroyed);
    EXPECT_EQ(x.cl->ref, refs);
    DW_RELEASE(c);
    r2_close(&x);
}
#define MOVEFAIL(at) \
    RT(movenext_failure_on_call_##at##_has_false) { move_fail_case(at, FALSE, E_FAIL); } \
    RT(movenext_failure_on_call_##at##_has_true)  { move_fail_case(at, TRUE, E_FAIL); } \
    RT(movenext_failure_on_call_##at##_other_hresult) { move_fail_case(at, FALSE, (HRESULT)0x80070057); }
MOVEFAIL(1) MOVEFAIL(2) MOVEFAIL(3) MOVEFAIL(4) MOVEFAIL(5)
RT(movenext_failure_after_the_end_position_is_never_reached)
{
    /* the failing call number is beyond the terminating call: never triggered, creation succeeds */
    FX(x);
    add_n(&x, 2);
    x.cl->move_fail_at = 4; x.cl->move_fail_hr = E_FAIL;
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(x.cl->last_en->n_move, 3);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(movenext_failure_does_not_poison_the_next_create)
{
    FX(x);
    add_n(&x, 3);
    IDWriteFontCollection *c = NULL;
    x.cl->move_fail_at = 2; x.cl->move_fail_hr = E_ABORT;
    CHECK_HR(r2_create(&x, &c), E_ABORT);
    x.cl->move_fail_at = 0;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 3);
    DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- GetCurrentFontFile failures */
RT(getcurrent_failure_skips_exactly_that_file_for_every_mask_of_four)
{
    unsigned mask;
    for (mask = 0; mask < 16; mask++) {
        FX(x);
        int i, expect = 0;
        for (i = 0; i < 4; i++) {
            char nm[32]; fam_name(i, nm);
            if (mask & (1u << i)) r2_cl_step_get_fail(x.cl, 0, E_FAIL);
            else { size_t n; uint8_t *b = ttf_build_styled(nm, 400, 5, 0, &n); UINT32 k = r2_add_blob(&x, b, n); free(b); (void)k; expect++; }
        }
        IDWriteFontCollection *c = NULL;
        HRESULT hr = r2_create(&x, &c);
        CHECKF(hr == S_OK, "mask %x: %s", mask, dw_hr_name(hr));
        EXPECT_EQ(x.cl->last_en->n_get, 4);
        if (c) {
            EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), expect);
        }
        DW_RELEASE(c);
        r2_close(&x);
    }
}
RT(getcurrent_failure_skips_but_neighbours_keep_their_families)
{
    FX(x);
    r2_add_family(&x, "Before");
    r2_cl_step_get_fail(x.cl, 0, E_FAIL);
    r2_add_family(&x, "After");
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2); CHECK(r2_find_family(c, "Before") >= 0); CHECK(r2_find_family(c, "After") >= 0); }
    EXPECT_EQ(x.cl->last_en->n_get, 3);
    EXPECT_EQ(x.cl->last_en->n_move, 4);
    DW_RELEASE(c);
    r2_close(&x);
}
static void get_fail_case(HRESULT hr)
{
    FX(x);
    r2_cl_step_get_fail(x.cl, 0, hr);
    r2_add_family(&x, "Survivor");
    IDWriteFontCollection *c = NULL;
    HRESULT got = r2_create(&x, &c);
    CHECKF(got == S_OK, "GetCurrentFontFile %s -> create %s", dw_hr_name(hr), dw_hr_name(got));
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); CHECK(r2_find_family(c, "Survivor") == 0); }
    DW_RELEASE(c);
    r2_close(&x);
}
#define GETFAIL(tag, hr) RT(getcurrent_failure_hresult_##tag##_is_skipped) { get_fail_case((HRESULT)(hr)); }
GETFAIL(e_fail, E_FAIL) GETFAIL(e_notimpl, E_NOTIMPL) GETFAIL(e_outofmemory, E_OUTOFMEMORY) GETFAIL(e_invalidarg, E_INVALIDARG)
GETFAIL(e_unexpected, E_UNEXPECTED) GETFAIL(fileformat, 0x88985000) GETFAIL(nofont, 0x88985002) GETFAIL(filenotfound, 0x88985003)
GETFAIL(win32_file_not_found, 0x80070002) GETFAIL(custom, 0x8abc1234) GETFAIL(severity_only, 0x80000000)
RT(getcurrent_all_files_failing_gives_an_empty_collection)
{
    FX(x);
    int i;
    for (i = 0; i < 5; i++) r2_cl_step_get_fail(x.cl, 0, E_FAIL);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 0);
    EXPECT_EQ(x.cl->last_en->n_get, 5);
    EXPECT_EQ(x.fl->n_create, 0);                  /* no file => no stream was ever requested */
    DW_RELEASE(c);
    r2_close(&x);
}
RT(getcurrent_failure_with_a_non_null_out_file_is_released_by_dwrite)
{
    FX(x);
    size_t n; uint8_t *b = ttf_build_styled("Keepout", 400, 5, 0, &n);
    UINT32 k = r2_fl_add(x.fl, b, n); free(b);
    IDWriteFontFile *file = NULL; UINT32 key4 = k;
    CHECK_HR_OK(IDWriteFactory_CreateCustomFontFileReference(x.f, &key4, 4, (IDWriteFontFileLoader *)x.fl, &file));
    if (!file) { r2_close(&x); return; }
    r2_cl_step_get_fail_keepout(x.cl, k, E_FAIL);
    x.cl->steps[0].prebuilt = file;
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 0);          /* the failing entry is skipped */
    ULONG refs = dw_refcount((IUnknown *)file);
    t_note("file refs after a failed GetCurrentFontFile that still filled *out: %lu (1 = released by dwrite)", refs);
    CHECKF(refs == 1, "file refcount %lu", refs);
    EXPECT_EQ(x.fl->n_create, 0);
    DW_RELEASE(c);
    DW_RELEASE(file);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- ownership of the returned file */
static void own_case(int n, int same_object)
{
    FX(x);
    IDWriteFontFile *files[6] = { 0 }; int i;
    size_t sz; uint8_t *b = ttf_build_styled("Own", 400, 5, 0, &sz);
    UINT32 k = r2_fl_add(x.fl, b, sz); free(b);
    for (i = 0; i < n; i++) {
        UINT32 key4 = k;
        if (same_object && i > 0) { files[i] = files[0]; IDWriteFontFile_AddRef(files[i]); }
        else CHECK_HR_OK(IDWriteFactory_CreateCustomFontFileReference(x.f, &key4, 4, (IDWriteFontFileLoader *)x.fl, &files[i]));
        r2_cl_step_file(x.cl, k);
        x.cl->steps[i].prebuilt = files[i];
    }
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    for (i = 0; i < n; i++) {
        ULONG r = files[i] ? dw_refcount((IUnknown *)files[i]) : 0;
        ULONG want = same_object ? (ULONG)n : 1;
        CHECKF(r == want, "file %d refs %lu want %lu", i, r, want);
    }
    CHECK(x.cl->last_en->destroyed);
    for (i = 0; i < n; i++) DW_RELEASE(files[i]);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(returned_file_is_released_once_per_getcurrent_one_file) { own_case(1, 0); }
RT(returned_file_is_released_once_per_getcurrent_three_files) { own_case(3, 0); }
RT(returned_file_is_released_once_per_getcurrent_same_object_three_times) { own_case(3, 1); }
RT(returned_file_is_released_once_per_getcurrent_six_files) { own_case(6, 0); }

/* ------------------------------------------------------------------------- what the enumerated files cause */
RT(file_loader_receives_the_reference_key_of_every_enumerated_file)
{
    FX(x);
    add_n(&x, 5);
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    unsigned seen = 0; int i, n = r2_n();
    for (i = 0; i < n; i++) if (r2_log[i].kind == EV_FL_CREATE) {
        EXPECT_EQ(r2_log[i].a, 4);                                  /* the 4-byte key as stored in the font file reference */
        CHECK(r2_log[i].b < 5);
        if (r2_log[i].b < 5) seen |= 1u << r2_log[i].b;
    }
    EXPECT_EQ(seen, 0x1f);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(files_are_opened_in_enumeration_order)
{
    FX(x);
    add_n(&x, 6);
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = r2_n(); unsigned next = 0, order_ok = 1, firsts = 0, seen = 0;
    for (i = 0; i < n; i++) if (r2_log[i].kind == EV_FL_CREATE) {
        unsigned k = (unsigned)r2_log[i].b;
        if (k < 6 && !(seen & (1u << k))) { seen |= 1u << k; firsts++; if (k != next) order_ok = 0; next++; }
    }
    EXPECT_EQ(firsts, 6);
    CHECKF(order_ok, "first stream request per file is not in enumeration order");
    DW_RELEASE(c);
    r2_close(&x);
}
RT(each_file_is_analysed_before_the_next_movenext)
{
    /* the file of position i is opened between GetCurrentFontFile #i and MoveNext #i+1: analysis happens inside the loop */
    FX(x);
    add_n(&x, 4);
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = r2_n(), gets = 0, ok = 1; unsigned opened_since_get = 0;
    for (i = 0; i < n; i++) {
        int k = r2_log[i].kind;
        if (k == EV_EN_GET) { gets++; opened_since_get = 0; }
        if (k == EV_FL_CREATE) opened_since_get = 1;
        if (k == EV_EN_MOVE && gets > 0 && !opened_since_get) ok = 0;
    }
    CHECKF(ok, "a file was not opened before the following MoveNext");
    DW_RELEASE(c);
    r2_close(&x);
}
RT(duplicate_file_entries_are_all_opened)
{
    FX(x);
    UINT32 k = r2_add_family(&x, "Twice");
    r2_cl_step_file(x.cl, k); r2_cl_step_file(x.cl, k);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(x.cl->last_en->n_get, 3);
    CHECK(x.fl->n_create >= 1);
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); CHECK(r2_find_family(c, "Twice") == 0); }
    t_note("3 entries for one key: %ld CreateStreamFromKey calls", (long)x.fl->n_create);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(failed_and_skipped_entries_do_not_open_streams)
{
    FX(x);
    r2_add_family(&x, "First");
    r2_cl_step_get_fail(x.cl, 0, E_FAIL);
    r2_add_family(&x, "Last");
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = r2_n(); unsigned seen = 0;
    for (i = 0; i < n; i++) if (r2_log[i].kind == EV_FL_CREATE) seen |= 1u << (unsigned)r2_log[i].b;
    EXPECT_EQ(seen, 0x3);                           /* blobs 0 and 1 only: the failed entry had no blob */
    DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- enumerator lifetime */
RT(enumerator_is_released_on_every_exit_path)
{
    int path;
    for (path = 0; path < 3; path++) {
        FX(x);
        add_n(&x, 3);
        if (path == 1) { x.cl->move_fail_at = 2; x.cl->move_fail_hr = E_FAIL; }
        if (path == 2) { x.cl->steps[1].type = R2_STEP_GET_FAIL; x.cl->steps[1].hr = E_FAIL; }
        IDWriteFontCollection *c = NULL;
        HRESULT hr = r2_create(&x, &c);
        CHECKF(hr == (path == 1 ? E_FAIL : S_OK), "path %d: %s", path, dw_hr_name(hr));
        CHECKF(x.cl->last_en && x.cl->last_en->destroyed, "path %d: enumerator not destroyed on return", path);
        EXPECT_EQ(x.cl->last_en->ref, 0);
        DW_RELEASE(c);
        r2_close(&x);
        EXPECT_EQ(r2_overrel, 0);
    }
}
RT(enumerator_gets_no_extra_references_from_dwrite)
{
    FX(x);
    add_n(&x, 2);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    /* ours (1) at creation, released exactly once by dwrite -> destroyed, never resurrected */
    EXPECT_EQ(x.cl->last_en->ref, 0);
    EXPECT_EQ(x.cl->last_en->destroyed, 1);
    EXPECT_EQ(r2_uaf, 0);
    DW_RELEASE(c);
    r2_close(&x);
}
