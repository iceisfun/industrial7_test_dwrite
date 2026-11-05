/* IDWriteFontCollectionLoader::CreateEnumeratorFromKey as seen from the application.
 *
 * Win10 decomp facts (dwrite.dll):
 *  - DWriteFactory::CreateCustomFontCollection writes *out = NULL, rejects a NULL loader with E_INVALIDARG, then builds the
 *    collection synchronously (FontCollection ctor -> FontCollectionConstructionTask::AddElementData).
 *  - AddElementData fetches the loader by its registration id (an unknown id throws ArgumentException -> E_INVALIDARG; the
 *    ComPtr holds a reference on the loader for the duration), calls
 *      CreateEnumeratorFromKey(loader, factory, keyPtr, keySize, &enumerator)
 *    where keyPtr/keySize come from a private std::vector copy of the caller's key (an empty key is passed as keyPtr == NULL),
 *    runs CallbackException::ThrowIfFailed on the result, drives FontCollectionBuilder::AddFontFiles, then releases the
 *    enumerator and finally the loader.
 *  - AddFontFiles: loop { MoveNext; ThrowIfFailed; if !has break; GetCurrentFontFile; if FAILED skip file; else analyze }.
 *  - Callbacks run between FPUStateSSE/FPUStateSandboxSSE guards, i.e. the caller's SSE control state is restored on return.
 */
#include "r2.h"

/* ------------------------------------------------------------------------------------------ key bytes */
static void key_case(UINT32 ks)
{
    FX(x);
    uint8_t *buf = (uint8_t *)malloc(ks ? ks : 1); UINT32 i;
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    for (i = 0; i < ks; i++) buf[i] = (uint8_t)(i * 131u + ks * 7u + 1u);
    HRESULT hr = r2_create_key(&x, buf, ks, &c);
    CHECK_HR(hr, S_OK);
    EXPECT_EQ(x.cl->n_create, 1);
    EXPECT_EQ(x.cl->last_ks, ks);
    if (ks == 0) CHECKF(x.cl->last_key_null, "empty key must reach the loader as NULL, got %p", x.cl->last_key_ptr);   /* empty vector -> NULL */
    else {
        CHECK(x.cl->last_key_ptr != NULL);
        CHECKF(x.cl->last_key_ptr != buf, "loader received the caller's own buffer (not a private copy)");
        CHECK(memcmp(x.cl->last_key, buf, ks < sizeof x.cl->last_key ? ks : sizeof x.cl->last_key) == 0);
    }
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 0);
    DW_RELEASE(c);
    r2_close(&x);
    free(buf);
}
#define KEYCASE(tag, ks) RT(key_size_##tag) { key_case(ks); }
KEYCASE(0, 0) KEYCASE(1, 1) KEYCASE(2, 2) KEYCASE(3, 3) KEYCASE(4, 4) KEYCASE(5, 5) KEYCASE(7, 7) KEYCASE(8, 8) KEYCASE(15, 15)
KEYCASE(16, 16) KEYCASE(17, 17) KEYCASE(63, 63) KEYCASE(64, 64) KEYCASE(65, 65) KEYCASE(255, 255) KEYCASE(256, 256) KEYCASE(1023, 1023)
KEYCASE(4096, 4096)

static void key_pattern(const void *key, UINT32 ks)
{
    FX(x);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create_key(&x, key, ks, &c), S_OK);
    EXPECT_EQ(x.cl->last_ks, ks);
    CHECK(memcmp(x.cl->last_key, key, ks) == 0);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(key_pattern_all_zero_bytes) { uint8_t k[16]; memset(k, 0, sizeof k); key_pattern(k, sizeof k); }
RT(key_pattern_all_ff_bytes) { uint8_t k[16]; memset(k, 0xFF, sizeof k); key_pattern(k, sizeof k); }
RT(key_pattern_embedded_nul) { static const uint8_t k[] = { 'a', 0, 'b', 0, 0, 'c' }; key_pattern(k, sizeof k); }
RT(key_pattern_text_without_terminator) { key_pattern("collection-key", 14); }
RT(key_pattern_utf16_text) { key_pattern(L"Famé", 8); }
RT(key_pattern_trailing_nul_is_part_of_the_key) { key_pattern("abc\0", 4); }
RT(key_null_pointer_with_size_zero)
{
    FX(x);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create_key(&x, NULL, 0, &c), S_OK);
    EXPECT_EQ(x.cl->n_create, 1);
    EXPECT_EQ(x.cl->last_ks, 0);
    CHECK(x.cl->last_key_null);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(key_keysize_is_not_a_string_length)
{
    /* "abcd" with size 2 delivers exactly two bytes: the third byte is never inspected by the loader contract */
    FX(x);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create_key(&x, "abcd", 2, &c), S_OK);
    EXPECT_EQ(x.cl->last_ks, 2);
    CHECK(x.cl->last_key[0] == 'a' && x.cl->last_key[1] == 'b');
    DW_RELEASE(c);
    r2_close(&x);
}

/* --------------------------------------------------------------------------------- factory argument */
RT(factory_arg_is_the_creating_factory)
{
    FX(x);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    CHECK(x.cl->last_factory != NULL);
    CHECK(x.cl->last_factory && dw_same_identity((IUnknown *)x.cl->last_factory, (IUnknown *)x.f));
    DW_RELEASE(c);
    r2_close(&x);
}
RT(factory_arg_is_the_same_object_for_every_factory_interface_version)
{
    int v;
    for (v = 0; v <= 7; v++) {
        FX(x);
        void *p = NULL; IDWriteFontCollection *c = NULL;
        if (FAILED(IUnknown_QueryInterface((IUnknown *)x.f, dw_factory_iid(v), &p)) || !p) { r2_close(&x); continue; }
        /* the vtable of every IDWriteFactoryN starts with the IDWriteFactory methods: same call, other interface pointer */
        char k[16]; int n = r2_uniq_key(k);
        HRESULT hr = IDWriteFactory_CreateCustomFontCollection((IDWriteFactory *)p, CLP(x.cl), k, (UINT32)n, &c);
        CHECKF(hr == S_OK, "factory v%d: %s", v, dw_hr_name(hr));
        CHECKF(x.cl->last_factory && dw_same_identity((IUnknown *)x.cl->last_factory, (IUnknown *)x.f), "factory v%d identity", v);
        if (x.cl->last_factory) {
            IDWriteFactory *lf = x.cl->last_factory; void *again = NULL;
            CHECK_HR(IUnknown_QueryInterface((IUnknown *)lf, &IID_IDWriteFactory, &again), S_OK);
            if (again) IUnknown_Release((IUnknown *)again);
        }
        DW_RELEASE(c);
        IUnknown_Release((IUnknown *)p);
        r2_close(&x);
    }
}
RT(factory_arg_has_no_extra_reference_after_return)
{
    FX(x);
    IDWriteFontCollection *c = NULL;
    ULONG before = dw_refcount((IUnknown *)x.f);
    CHECK_HR(r2_create(&x, &c), S_OK);
    DW_RELEASE(c);
    ULONG after = dw_refcount((IUnknown *)x.f);
    EXPECT_EQ(after, before);
    r2_close(&x);
}
RT(factory_arg_stays_valid_for_file_reference_creation)
{
    /* the factory argument is a usable factory: the loader creates its font file references through it */
    FX(x);
    r2_add_family(&x, "ArgFactory");
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(x.cl->nens, 1);
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); CHECK(r2_find_family(c, "ArgFactory") == 0); }
    DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------------ loader reference counts */
RT(loader_gets_a_reference_for_the_duration_of_the_call)
{
    FX(x);
    IDWriteFontCollection *c = NULL;
    LONG before = x.cl->ref;                  /* ours + the registration */
    CHECK_HR(r2_create(&x, &c), S_OK);
    CHECKF(x.cl->ref_in_create >= before + 1, "refs inside CreateEnumeratorFromKey %ld, before %ld", (long)x.cl->ref_in_create, (long)before);
    t_note("loader refs: before %ld, inside callback %ld, after %ld", (long)before, (long)x.cl->ref_in_create, (long)x.cl->ref);
    EXPECT_EQ(x.cl->ref, before);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(loader_reference_balance_over_many_creates)
{
    FX(x);
    int i; LONG before = x.cl->ref;
    r2_add_family(&x, "Balance");
    for (i = 0; i < 25; i++) {
        IDWriteFontCollection *c = NULL;
        CHECK_HR(r2_create(&x, &c), S_OK);
        DW_RELEASE(c);
    }
    EXPECT_EQ(x.cl->ref, before);
    EXPECT_EQ(x.cl->n_create, 25);
    CHECK(x.cl->n_addref >= 25);
    EXPECT_EQ(r2_overrel, 0);
    r2_close(&x);
}
RT(loader_is_released_after_the_enumerator_and_after_the_call_started)
{
    FX(x);
    r2_add_family(&x, "Order");
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int a = r2_find(EV_CL_ADDREF, 0), cr = r2_find(EV_CL_CREATE, 0), d = r2_find(EV_EN_DESTROY, 0);
    int lastr = -1, i, n = r2_n();
    for (i = 0; i < n; i++) if (r2_log[i].kind == EV_CL_RELEASE) lastr = i;
    CHECK(a >= 0 && cr >= 0 && d >= 0 && lastr >= 0);
    CHECKF(a < cr, "loader AddRef (%d) must precede CreateEnumeratorFromKey (%d)", a, cr);
    CHECKF(cr < d, "enumerator destroyed (%d) after it was created (%d)", d, cr);
    CHECKF(d < lastr, "last loader Release (%d) comes after the enumerator was destroyed (%d)", lastr, d);
    DW_RELEASE(c);
    r2_close(&x);
}

/* ---------------------------------------------------------------------------------------- call order */
RT(order_empty_enumerator)
{
    FX(x);
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    char tr[64]; r2_trace(tr, sizeof tr, "CMGD");
    CHECKF(!strcmp(tr, "CMD"), "trace \"%s\" want \"CMD\"", tr);
    EXPECT_EQ(x.cl->last_en->n_move, 1);
    EXPECT_EQ(x.cl->last_en->n_get, 0);
    if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 0);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(order_move_get_alternate_for_one_to_eight_files)
{
    int n;
    for (n = 1; n <= 8; n++) {
        FX(x);
        char want[64], tr[128]; int i, len = 0;
        for (i = 0; i < n; i++) { char nm[32]; snprintf(nm, sizeof nm, "Alt%d", i); r2_add_family(&x, nm); }
        want[len++] = 'C';
        for (i = 0; i < n; i++) { want[len++] = 'M'; want[len++] = 'G'; }
        want[len++] = 'M'; want[len++] = 'D'; want[len] = 0;
        r2_log_reset();
        IDWriteFontCollection *c = NULL;
        CHECK_HR(r2_create(&x, &c), S_OK);
        r2_trace(tr, sizeof tr, "CMGD");
        CHECKF(!strcmp(tr, want), "n=%d trace \"%s\" want \"%s\"", n, tr, want);
        EXPECT_EQ(x.cl->last_en->n_move, n + 1);
        EXPECT_EQ(x.cl->last_en->n_get, n);
        EXPECT_EQ(x.cl->last_en->moves_after_end, 0);
        EXPECT_EQ(x.cl->last_en->gets_out_of_seq, 0);
        if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), n);
        DW_RELEASE(c);
        r2_close(&x);
    }
}
RT(order_loader_called_once_per_create_call)
{
    FX(x);
    IDWriteFontCollection *c[3] = { NULL, NULL, NULL }; int i;
    r2_add_family(&x, "Once");
    for (i = 0; i < 3; i++) {
        LONG before = x.cl->n_create;
        CHECK_HR(r2_create(&x, &c[i]), S_OK);
        EXPECT_EQ(x.cl->n_create, before + 1);
    }
    EXPECT_EQ(x.cl->nens, 3);
    CHECK(x.cl->ens[0] != x.cl->ens[1] && x.cl->ens[1] != x.cl->ens[2]);
    for (i = 0; i < 3; i++) { CHECK(x.cl->ens[i] && x.cl->ens[i]->destroyed); DW_RELEASE(c[i]); }
    r2_close(&x);
}
RT(order_create_call_returns_only_after_enumeration_is_finished)
{
    FX(x);
    int i;
    for (i = 0; i < 4; i++) { char nm[32]; snprintf(nm, sizeof nm, "Fin%d", i); r2_add_family(&x, nm); }
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int n_after = r2_n();
    EXPECT_EQ(x.cl->last_en->n_move, 5);
    /* nothing of the loader/enumerator contract happens later, whatever the collection is used for */
    if (c) {
        IDWriteFontFamily *fam = NULL; UINT32 cnt = IDWriteFontCollection_GetFontFamilyCount(c);
        for (i = 0; i < (int)cnt; i++) { fam = NULL; IDWriteFontCollection_GetFontFamily(c, i, &fam); DW_RELEASE(fam); }
    }
    EXPECT_EQ(r2_count(EV_CL_CREATE), 1);
    EXPECT_EQ(r2_count(EV_EN_MOVE), 5);
    EXPECT_EQ(r2_count(EV_EN_GET), 4);
    (void)n_after;
    DW_RELEASE(c);
    r2_close(&x);
}
RT(order_no_loader_calls_while_using_the_collection)
{
    FX(x);
    r2_add_family(&x, "UseA"); r2_add_style(&x, "UseB", 700, 5, 0);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    LONG creates = x.cl->n_create, moves = x.cl->last_en->n_move;
    if (c) {
        UINT32 i, cnt = IDWriteFontCollection_GetFontFamilyCount(c);
        for (i = 0; i < cnt; i++) {
            IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) && fam) {
                if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) && font) {
                    IDWriteFont_CreateFontFace(font, &face);
                    DW_RELEASE(face); DW_RELEASE(font);
                }
                DW_RELEASE(fam);
            }
        }
        UINT32 idx; BOOL ex; IDWriteFontCollection_FindFamilyName(c, L"UseA", &idx, &ex);
    }
    EXPECT_EQ(x.cl->n_create, creates);
    EXPECT_EQ(x.cl->last_en->n_move, moves);
    DW_RELEASE(c);
    r2_close(&x);
}

/* ---------------------------------------------------------------------------------- failure results */
static void create_failure_case(HRESULT want)
{
    FX(x);
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    r2_add_family(&x, "NeverSeen");
    x.cl->create_hr = want;
    LONG refs = x.cl->ref;
    HRESULT hr = r2_create(&x, &c);
    CHECKF(hr == want, "loader returned %s, CreateCustomFontCollection returned %s", dw_hr_name(want), dw_hr_name(hr));
    CHECK_OUT_NULL(c);
    EXPECT_EQ(x.cl->n_create, 1);
    EXPECT_EQ(x.cl->nens, 0);
    EXPECT_EQ(x.cl->ref, refs);
    EXPECT_EQ(r2_count(EV_EN_MOVE), 0);
    DW_RELEASE(c);
    r2_close(&x);
}
#define FAILCASE(tag, hr) RT(failure_propagates_##tag) { create_failure_case((HRESULT)(hr)); }
FAILCASE(e_fail, E_FAIL) FAILCASE(e_outofmemory, E_OUTOFMEMORY) FAILCASE(e_invalidarg, E_INVALIDARG) FAILCASE(e_notimpl, E_NOTIMPL)
FAILCASE(e_abort, E_ABORT) FAILCASE(e_unexpected, E_UNEXPECTED) FAILCASE(e_accessdenied, E_ACCESSDENIED) FAILCASE(e_nointerface, E_NOINTERFACE)
FAILCASE(e_pointer, E_POINTER) FAILCASE(dwrite_fileformat, 0x88985000) FAILCASE(dwrite_nofont, 0x88985002) FAILCASE(insufficient_buffer, 0x8007007A)
FAILCASE(win32_access_denied, 0x80070005) FAILCASE(custom_facility_itf, 0x8abc1234) FAILCASE(custom_severity_bit_only, 0x80000001)

RT(failure_leaves_factory_and_loader_usable)
{
    FX(x);
    r2_add_family(&x, "AfterFail");
    IDWriteFontCollection *c = NULL;
    x.cl->create_hr = E_FAIL;
    CHECK_HR(r2_create(&x, &c), E_FAIL);
    x.cl->create_hr = 0;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); CHECK(r2_find_family(c, "AfterFail") == 0); }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(failure_is_not_cached_for_the_same_key)
{
    FX(x);
    int i;
    for (i = 0; i < 3; i++) {
        IDWriteFontCollection *c = NULL;
        x.cl->create_hr = E_FAIL;
        HRESULT hf = r2_create_key(&x, "samekey!", 8, &c);
        /* once a success is cached for the key the loader is no longer consulted, so only the first attempt must fail */
        if (i == 0) CHECK_HR(hf, E_FAIL); else CHECKF(hf == E_FAIL || hf == S_OK, "attempt %d: %s", i, dw_hr_name(hf));
        DW_RELEASE(c);
        c = NULL;
        x.cl->create_hr = 0;
        CHECK_HR(r2_create_key(&x, "samekey!", 8, &c), S_OK);
        DW_RELEASE(c);
    }
    /* the first failure reaches the loader and is not cached; later attempts may be answered from the factory cache of the earlier success */
    CHECK(x.cl->n_create >= 2);
    r2_close(&x);
}
RT(failure_in_one_loader_does_not_disturb_another)
{
    FX(x);
    r2_cl *other = r2_cl_new(x.fl);
    r2_add_family(&x, "Shared");
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(other)));
    r2_cl_step_file(other, 0);
    x.cl->create_hr = E_FAIL;
    IDWriteFontCollection *c1 = NULL, *c2 = NULL;
    char k[16]; int n = r2_uniq_key(k);
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, CLP(x.cl), k, n, &c1), E_FAIL);
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, CLP(other), k, n, &c2), S_OK);
    CHECK_OUT_NULL(c1);
    if (c2) CHECK(r2_find_family(c2, "Shared") == 0);
    EXPECT_EQ(other->n_create, 1);
    DW_RELEASE(c2);
    IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(other));
    r2_obj_release(other);
    r2_close(&x);
}
RT(null_loader_is_invalid_argument)
{
    FX(x);
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, NULL, "abcd", 4, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(x.cl->n_create, 0);
    r2_close(&x);
}
RT(unregistered_loader_is_invalid_argument_and_is_not_called)
{
    FX(x);
    r2_cl *stray = r2_cl_new(x.fl);
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    LONG refs = stray->ref;
    CHECK_HR(IDWriteFactory_CreateCustomFontCollection(x.f, CLP(stray), "abcd", 4, &c), E_INVALIDARG);
    CHECK_OUT_NULL(c);
    EXPECT_EQ(stray->n_create, 0);
    EXPECT_EQ(stray->ref, refs);
    r2_obj_release(stray);
    r2_close(&x);
}
struct peek { IDWriteFontCollection **pc; void *seen; };
static void hook_peek_out(r2_cl *c, IDWriteFactory *f, void *arg)
{
    struct peek *p = (struct peek *)arg;
    p->seen = *p->pc;
}
RT(out_pointer_is_zeroed_before_the_loader_is_called)
{
    /* CreateCustomFontCollection writes *ppCollection = NULL first; the hook reads the caller's variable from inside the callback */
    FX(x);
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    struct peek pr; pr.pc = &c; pr.seen = (void *)(ULONG_PTR)0x55;
    x.cl->hook = hook_peek_out; x.cl->hook_arg = &pr;
    CHECK_HR(r2_create(&x, &c), S_OK);
    CHECKF(pr.seen == NULL, "caller's out variable held %p inside the callback", pr.seen);
    CHECK(c != (IDWriteFontCollection *)(ULONG_PTR)1);
    DW_RELEASE(c);
    r2_close(&x);
}

/* ---------------------------------------------------------------------------------------- FP state */
RT(sse_state_is_restored_when_the_callback_changes_it)
{
    FX(x);
    unsigned orig = _mm_getcsr(), want = (orig & ~0x6000u) | 0x6000u;   /* caller: round toward zero */
    IDWriteFontCollection *c = NULL;
    _mm_setcsr(want);
    x.cl->set_csr = orig & ~0x6000u;                                    /* callback leaves round-to-nearest behind */
    r2_add_family(&x, "Csr");
    HRESULT hr = r2_create(&x, &c);
    unsigned after = _mm_getcsr();
    _mm_setcsr(orig);
    CHECK_HR(hr, S_OK);
    t_note("MXCSR: caller 0x%x, seen in callback 0x%x, after return 0x%x", want, x.cl->csr_in_create, after);
    CHECKF((after & 0x6000u) == (want & 0x6000u), "rounding control after return 0x%x want 0x%x", after & 0x6000u, want & 0x6000u);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(sse_state_caller_flags_are_preserved_across_the_call)
{
    FX(x);
    unsigned orig = _mm_getcsr(), want = orig | 0x8040u;                 /* FTZ + DAZ */
    IDWriteFontCollection *c = NULL;
    _mm_setcsr(want);
    HRESULT hr = r2_create(&x, &c);
    unsigned after = _mm_getcsr();
    _mm_setcsr(orig);
    CHECK_HR(hr, S_OK);
    CHECKF((after & 0x8040u) == 0x8040u, "FTZ/DAZ lost: 0x%x", after);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(sse_state_restored_after_failed_create)
{
    FX(x);
    unsigned orig = _mm_getcsr(), want = orig | 0x6000u;
    IDWriteFontCollection *c = NULL;
    x.cl->create_hr = E_FAIL; x.cl->set_csr = orig & ~0x6000u;
    _mm_setcsr(want);
    HRESULT hr = r2_create(&x, &c);
    unsigned after = _mm_getcsr();
    _mm_setcsr(orig);
    CHECK_HR(hr, E_FAIL);
    CHECKF((after & 0x6000u) == 0x6000u, "rounding control after failed create 0x%x", after);
    r2_close(&x);
}

/* ------------------------------------------------------------------------------------------ threads */
typedef struct { r2_fx *x; HRESULT hr; DWORD tid; UINT32 families; int rounds; char tag[8]; } thr_arg;
static DWORD WINAPI thr_one(void *p)
{
    thr_arg *a = (thr_arg *)p; IDWriteFontCollection *c = NULL;
    a->tid = GetCurrentThreadId();
    a->hr = r2_create(a->x, &c);
    if (c) { a->families = IDWriteFontCollection_GetFontFamilyCount(c); IDWriteFontCollection_Release(c); }
    return 0;
}
RT(callbacks_run_on_the_calling_thread_main)
{
    FX(x);
    r2_add_family(&x, "Thr0"); r2_add_family(&x, "Thr1");
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    CHECK(r2_n() > 0);
    CHECKF(r2_tids_all(GetCurrentThreadId()), "a callback ran on another thread");
    EXPECT_EQ(x.cl->last_tid, GetCurrentThreadId());
    DW_RELEASE(c);
    r2_close(&x);
}
RT(callbacks_run_on_the_calling_thread_worker)
{
    FX(x);
    r2_add_family(&x, "Thr0"); r2_add_family(&x, "Thr1");
    thr_arg a; memset(&a, 0, sizeof a); a.x = &x;
    r2_log_reset();
    HANDLE h = CreateThread(NULL, 0, thr_one, &a, 0, NULL);
    if (!h) { t_skip("CreateThread failed"); r2_close(&x); return; }
    EXPECT_EQ(WaitForSingleObject(h, 20000), WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK_HR(a.hr, S_OK);
    EXPECT_EQ(a.families, 2);
    CHECK(r2_n() > 0);
    CHECKF(r2_tids_all(a.tid), "a callback ran on a thread other than the caller's worker");
    CHECK(a.tid != GetCurrentThreadId());
    r2_close(&x);
}
RT(callbacks_from_four_concurrent_creates_stay_on_their_own_threads)
{
    FX(x);
    enum { N = 4 };
    thr_arg a[N]; HANDLE h[N]; int i, started = 0;
    r2_add_family(&x, "Conc");
    memset(a, 0, sizeof a);
    for (i = 0; i < N; i++) { a[i].x = &x; h[i] = CreateThread(NULL, 0, thr_one, &a[i], 0, NULL); if (h[i]) started++; }
    for (i = 0; i < N; i++) if (h[i]) { EXPECT_EQ(WaitForSingleObject(h[i], 20000), WAIT_OBJECT_0); CloseHandle(h[i]); }
    EXPECT_EQ(started, N);
    for (i = 0; i < N; i++) { CHECK_HR(a[i].hr, S_OK); EXPECT_EQ(a[i].families, 1); }
    EXPECT_EQ(x.cl->n_create, N);
    /* every call record carries a tid; each worker thread id must appear exactly once */
    for (i = 0; i < N; i++) {
        int k, seen = 0;
        for (k = 0; k < N; k++) if (x.cl->calls[k].tid == a[i].tid) seen++;
        CHECKF(seen == 1, "worker %d tid %lu seen %d times in loader calls", i, a[i].tid, seen);
    }
    EXPECT_EQ(r2_overrel, 0);
    r2_close(&x);
}

/* ------------------------------------------------------------------------------------- re-entrancy */
static void hook_unregister_self(r2_cl *c, IDWriteFactory *f, void *arg)
{
    *(HRESULT *)arg = IDWriteFactory_UnregisterFontCollectionLoader(f, CLP(c));
}
RT(loader_may_unregister_itself_inside_the_callback)
{
    FX(x);
    HRESULT inner = 0x7FFFFFFF;
    r2_add_family(&x, "SelfUnreg");
    x.cl->hook = hook_unregister_self; x.cl->hook_arg = &inner;
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    CHECK_HR(inner, S_OK);
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1); CHECK(r2_find_family(c, "SelfUnreg") == 0); }
    x.reg_cl = 0;
    /* and the loader is gone for the next create */
    IDWriteFontCollection *c2 = (IDWriteFontCollection *)(ULONG_PTR)1;
    CHECK_HR(r2_create(&x, &c2), E_INVALIDARG);
    CHECK_OUT_NULL(c2);
    DW_RELEASE(c); DW_RELEASE(c2);
    r2_close(&x);
}
static void hook_create_other(r2_cl *c, IDWriteFactory *f, void *arg)
{
    r2_cl *other = (r2_cl *)arg; IDWriteFontCollection *inner = NULL;
    c->hook = NULL;
    IDWriteFactory_CreateCustomFontCollection(f, CLP(other), "inner!!!", 8, &inner);
    DW_RELEASE(inner);
}
RT(loader_may_build_another_collection_inside_the_callback)
{
    FX(x);
    r2_cl *other = r2_cl_new(x.fl);
    CHECK_HR_OK(IDWriteFactory_RegisterFontCollectionLoader(x.f, CLP(other)));
    UINT32 k = r2_add_family(&x, "Outer");
    r2_cl_step_file(other, k);
    x.cl->hook = hook_create_other; x.cl->hook_arg = other;
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(other->n_create, 1);
    if (c) CHECK(r2_find_family(c, "Outer") == 0);
    DW_RELEASE(c);
    IDWriteFactory_UnregisterFontCollectionLoader(x.f, CLP(other));
    r2_obj_release(other);
    r2_close(&x);
}

/* ------------------------------------------------------------------------------------ QueryInterface */
RT(loader_interface_queries_note)
{
    FX(x);
    r2_add_family(&x, "Qi");
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    t_note("QueryInterface calls on the collection loader during register+create: %ld", (long)x.cl->n_qi);
    int i; for (i = 0; i < x.cl->n_qi_iids; i++) { char b[64]; snprintf(b, sizeof b, "%08lx-%04x-%04x", x.cl->qi_iids[i].Data1, x.cl->qi_iids[i].Data2, x.cl->qi_iids[i].Data3); t_note("  QI iid %s", b); }
    DW_RELEASE(c);
    r2_close(&x);
}
