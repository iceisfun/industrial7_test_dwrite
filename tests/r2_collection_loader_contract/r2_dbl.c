/* Implementation of the r2 callback doubles (see r2.h). */
#include "r2.h"

r2_ev r2_log[R2_LOGMAX];
volatile LONG r2_nlog, r2_uaf, r2_overrel;
static CRITICAL_SECTION r2_cs;
static void __attribute__((constructor)) r2_cs_init(void) { InitializeCriticalSection(&r2_cs); }
static volatile LONG r2_ids;

static void ev(int kind, UINT64 a, UINT64 b, int obj)
{
    LONG i = InterlockedIncrement(&r2_nlog) - 1;
    if (i < R2_LOGMAX) { r2_log[i].kind = kind; r2_log[i].tid = GetCurrentThreadId(); r2_log[i].a = a; r2_log[i].b = b; r2_log[i].obj = obj; }
}
void r2_log_reset(void) { r2_nlog = 0; }
int r2_n(void) { return r2_nlog > R2_LOGMAX ? R2_LOGMAX : (int)r2_nlog; }
int r2_count(int kind) { int i, n = r2_n(), c = 0; for (i = 0; i < n; i++) if (r2_log[i].kind == kind) c++; return c; }
int r2_find(int kind, int from) { int i, n = r2_n(); for (i = from < 0 ? 0 : from; i < n; i++) if (r2_log[i].kind == kind) return i; return -1; }
int r2_find_obj(int kind, int obj, int from) { int i, n = r2_n(); for (i = from < 0 ? 0 : from; i < n; i++) if (r2_log[i].kind == kind && r2_log[i].obj == obj) return i; return -1; }
int r2_tids_all(DWORD tid) { int i, n = r2_n(); for (i = 0; i < n; i++) if (r2_log[i].tid != tid) return 0; return 1; }
void r2_trace(char *out, int cap, const char *only)
{
    static const char map[] = " arCMGDFswRxdLK";
    int i, n = r2_n(), len = 0;
    for (i = 0; i < n && len < cap - 1; i++) {
        char ch = map[r2_log[i].kind];
        if (only && !strchr(only, ch)) continue;
        out[len++] = ch;
    }
    out[len] = 0;
}

/* ---- tripwire vtable for destroyed objects ---- */
static HRESULT WINAPI dead_call(void) { InterlockedIncrement(&r2_uaf); return E_FAIL; }
static const void *const dead_vt[8] = { dead_call, dead_call, dead_call, dead_call, dead_call, dead_call, dead_call, dead_call };

/* ========================================================================== stream */
static HRESULT WINAPI st_QI(IDWriteFontFileStream *This, REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFileStream)) { *ppv = This; InterlockedIncrement(&((r2_st *)This)->ref); return S_OK; }
    return E_NOINTERFACE;
}
static ULONG WINAPI st_AR(IDWriteFontFileStream *This) { return InterlockedIncrement(&((r2_st *)This)->ref); }
static ULONG WINAPI st_RL(IDWriteFontFileStream *This)
{
    r2_st *s = (r2_st *)This; LONG r = InterlockedDecrement(&s->ref);
    if (r < 0) { InterlockedIncrement(&r2_overrel); return 0; }
    if (r == 0) { s->destroyed = 1; ev(EV_ST_DESTROY, 0, 0, s->id); s->lpVtbl = dead_vt; }
    return (ULONG)r;
}
static HRESULT WINAPI st_Read(IDWriteFontFileStream *This, const void **start, UINT64 off, UINT64 len, void **ctx)
{
    r2_st *s = (r2_st *)This; LONG n = InterlockedIncrement(&s->n_read), idx = n - 1;
    ev(EV_ST_READ, off, len, s->id);
    EnterCriticalSection(&r2_cs);
    if (idx < 64) { s->reads[idx].off = off; s->reads[idx].len = len; s->nlogged = idx + 1; }
    s->last_off = off; s->last_len = len;
    if (off + len >= off && off + len > s->max_end) s->max_end = off + len;
    LeaveCriticalSection(&r2_cs);
    if (start) *start = NULL;
    if (ctx) *ctx = NULL;
    if (s->fail_read && idx >= s->fail_read_from) return s->fail_read;
    if (off + len < off || off + len > s->size_real) { InterlockedIncrement(&s->reads_beyond); return E_FAIL; }
    {
        void *c;
        if (s->copy) { c = malloc(len ? (size_t)len : 1); memcpy(c, s->data + off, (size_t)len); if (start) *start = c; }
        else { c = (void *)(ULONG_PTR)(0x7000 + 16 * (ULONG_PTR)n); if (start) *start = s->data + off; }
        if (ctx) *ctx = c;
        EnterCriticalSection(&r2_cs);
        if (s->nopen < 256) s->open_ctx[s->nopen++] = c;
        s->outstanding++; if (s->outstanding > s->max_out) s->max_out = s->outstanding;
        LeaveCriticalSection(&r2_cs);
    }
    return S_OK;
}
static void WINAPI st_Rel(IDWriteFontFileStream *This, void *ctx)
{
    r2_st *s = (r2_st *)This; int i, found = 0;
    InterlockedIncrement(&s->n_rel);
    ev(EV_ST_REL, (UINT64)(ULONG_PTR)ctx, 0, s->id);
    EnterCriticalSection(&r2_cs);
    for (i = 0; i < s->nopen; i++) if (s->open_ctx[i] == ctx) { s->open_ctx[i] = s->open_ctx[--s->nopen]; found = 1; break; }
    if (found) s->outstanding--; else s->bad_ctx++;
    LeaveCriticalSection(&r2_cs);
    if (found && s->copy) { memset(ctx, 0xDD, 1); free(ctx); }
}
static HRESULT WINAPI st_Size(IDWriteFontFileStream *This, UINT64 *size)
{
    r2_st *s = (r2_st *)This; InterlockedIncrement(&s->n_size); ev(EV_ST_SIZE, s->size_report, 0, s->id);
    if (size) *size = 0;
    if (s->fail_size) return s->fail_size;
    if (size) *size = s->size_report;
    return S_OK;
}
static HRESULT WINAPI st_Last(IDWriteFontFileStream *This, UINT64 *t)
{
    r2_st *s = (r2_st *)This; InterlockedIncrement(&s->n_last); ev(EV_ST_LAST, 0, 0, s->id);
    if (t) *t = 0;
    if (s->fail_last) return s->fail_last;
    if (t) *t = 0x01D6000000000000ULL;
    return S_OK;
}
static const IDWriteFontFileStreamVtbl st_vt = { st_QI, st_AR, st_RL, st_Read, st_Rel, st_Size, st_Last };

/* ========================================================================== file loader */
static HRESULT WINAPI fl_QI(IDWriteFontFileLoader *This, REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFileLoader)) { *ppv = This; InterlockedIncrement(&((r2_fl *)This)->ref); return S_OK; }
    return E_NOINTERFACE;
}
static ULONG WINAPI fl_AR(IDWriteFontFileLoader *This) { r2_fl *l = (r2_fl *)This; InterlockedIncrement(&l->n_addref); return InterlockedIncrement(&l->ref); }
static ULONG WINAPI fl_RL(IDWriteFontFileLoader *This)
{
    r2_fl *l = (r2_fl *)This; LONG r; InterlockedIncrement(&l->n_release);
    r = InterlockedDecrement(&l->ref);
    if (r < 0) { InterlockedIncrement(&r2_overrel); return 0; }
    if (r == 0) { l->destroyed = 1; ev(EV_FL_DESTROY, 0, 0, l->id); l->lpVtbl = dead_vt; }
    return (ULONG)r;
}
static HRESULT WINAPI fl_Create(IDWriteFontFileLoader *This, const void *key, UINT32 ks, IDWriteFontFileStream **out)
{
    r2_fl *l = (r2_fl *)This; UINT32 k; const r2_stream_cfg *cfg; r2_st *s; LONG slot;
    InterlockedIncrement(&l->n_create);
    ev(EV_FL_CREATE, ks, (key && ks >= 4) ? *(const UINT32 *)key : 0xFFFFFFFFu, l->id);
    l->last_key_size = ks; l->last_key_ptr = key;
    if (out) *out = NULL;
    if (!key || ks != 4 || !out) return E_INVALIDARG;
    k = *(const UINT32 *)key; l->last_key_val = k;
    if (k >= (UINT32)l->nblobs) return E_INVALIDARG;
    cfg = &l->cfg[k];
    if (cfg->fail_create) return cfg->fail_create;
    s = (r2_st *)calloc(1, sizeof *s);
    s->lpVtbl = &st_vt; s->ref = 1; s->id = InterlockedIncrement(&r2_ids);
    s->data = l->blob[k].data; s->size_real = l->blob[k].size;
    s->size_report = cfg->has_size_override ? cfg->size_override : s->size_real;
    s->fail_size = cfg->fail_size; s->fail_last = cfg->fail_last; s->fail_read = cfg->fail_read; s->fail_read_from = cfg->fail_read_from;
    s->copy = cfg->copy; s->fl = l; s->blob = (int)k;
    slot = InterlockedIncrement(&l->nstreams) - 1;
    if (slot < 512) l->streams[slot] = s;
    *out = (IDWriteFontFileStream *)s;
    return S_OK;
}
static const IDWriteFontFileLoaderVtbl fl_vt = { fl_QI, fl_AR, fl_RL, fl_Create };
r2_fl *r2_fl_new(void)
{
    r2_fl *l = (r2_fl *)calloc(1, sizeof *l);
    l->lpVtbl = &fl_vt; l->ref = 1; l->id = InterlockedIncrement(&r2_ids);
    return l;
}
UINT32 r2_fl_add(r2_fl *l, const void *data, size_t n)
{
    UINT32 k;
    if (l->nblobs >= R2_MAXBLOB) return 0xFFFFFFFFu;
    k = (UINT32)l->nblobs++;
    l->blob[k].data = (uint8_t *)malloc(n ? n : 1); memcpy(l->blob[k].data, data, n); l->blob[k].size = n;
    memset(&l->cfg[k], 0, sizeof l->cfg[k]);
    return k;
}
HRESULT r2_fl_register(r2_fl *l, IDWriteFactory *f)
{
    HRESULT hr = IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)l);
    if (SUCCEEDED(hr)) l->registered_on = f;
    return hr;
}
void r2_fl_unregister(r2_fl *l)
{
    if (l->registered_on) { IDWriteFactory_UnregisterFontFileLoader(l->registered_on, (IDWriteFontFileLoader *)l); l->registered_on = NULL; }
}
int r2_fl_streams_alive(r2_fl *l) { int i, n = l->nstreams > 512 ? 512 : (int)l->nstreams, c = 0; for (i = 0; i < n; i++) if (l->streams[i]->ref > 0) c++; return c; }
LONG r2_fl_outstanding(r2_fl *l) { int i, n = l->nstreams > 512 ? 512 : (int)l->nstreams; LONG c = 0; for (i = 0; i < n; i++) c += l->streams[i]->outstanding; return c; }
LONG r2_fl_bad_ctx(r2_fl *l) { int i, n = l->nstreams > 512 ? 512 : (int)l->nstreams; LONG c = 0; for (i = 0; i < n; i++) c += l->streams[i]->bad_ctx; return c; }
LONG r2_fl_reads(r2_fl *l) { int i, n = l->nstreams > 512 ? 512 : (int)l->nstreams; LONG c = 0; for (i = 0; i < n; i++) c += l->streams[i]->n_read; return c; }
LONG r2_fl_reads_beyond(r2_fl *l) { int i, n = l->nstreams > 512 ? 512 : (int)l->nstreams; LONG c = 0; for (i = 0; i < n; i++) c += l->streams[i]->reads_beyond; return c; }

/* ========================================================================== enumerator */
static HRESULT WINAPI en_QI(IDWriteFontFileEnumerator *This, REFIID riid, void **ppv)
{
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFileEnumerator)) { *ppv = This; InterlockedIncrement(&((r2_en *)This)->ref); return S_OK; }
    return E_NOINTERFACE;
}
static ULONG WINAPI en_AR(IDWriteFontFileEnumerator *This) { return InterlockedIncrement(&((r2_en *)This)->ref); }
static ULONG WINAPI en_RL(IDWriteFontFileEnumerator *This)
{
    r2_en *e = (r2_en *)This; LONG r = InterlockedDecrement(&e->ref); int i;
    if (r < 0) { InterlockedIncrement(&r2_overrel); return 0; }
    if (r == 0) {
        e->destroyed = 1; ev(EV_EN_DESTROY, 0, 0, e->id); e->lpVtbl = dead_vt;
        for (i = 0; i < e->nsteps; i++) if (e->files[i]) { IDWriteFontFile_Release(e->files[i]); e->files[i] = NULL; }
    }
    return (ULONG)r;
}
static HRESULT WINAPI en_Move(IDWriteFontFileEnumerator *This, BOOL *has)
{
    r2_en *e = (r2_en *)This; LONG n = InterlockedIncrement(&e->n_move);
    ev(EV_EN_MOVE, (UINT64)n, 0, e->id);
    if (has) *has = FALSE;
    if (e->move_fail_at && n == e->move_fail_at) { if (has) *has = e->move_fail_has; return e->move_fail_hr; }
    if (e->pos < e->nsteps) e->pos++;
    if (has) *has = e->pos < e->nsteps ? e->has_value : FALSE;
    if (e->pos >= e->nsteps) { if (e->ended) InterlockedIncrement(&e->moves_after_end); e->ended = 1; }
    return S_OK;
}
static HRESULT WINAPI en_Get(IDWriteFontFileEnumerator *This, IDWriteFontFile **out)
{
    r2_en *e = (r2_en *)This; LONG n = InterlockedIncrement(&e->n_get);
    ev(EV_EN_GET, (UINT64)n, (UINT64)(e->pos < 0 ? 0xFFFF : e->pos), e->id);
    if (out) *out = NULL;
    if (e->pos < 0 || e->pos >= e->nsteps) { InterlockedIncrement(&e->gets_out_of_seq); return E_FAIL; }
    {
        const r2_step *st = &e->steps[e->pos];
        if (st->type == R2_STEP_GET_FAIL) return st->hr;
        if (!e->files[e->pos]) return E_FAIL;
        IDWriteFontFile_AddRef(e->files[e->pos]);
        if (out) *out = e->files[e->pos];
        if (st->type == R2_STEP_GET_FAIL_KEEPOUT) return st->hr;
        return S_OK;
    }
}
static const IDWriteFontFileEnumeratorVtbl en_vt = { en_QI, en_AR, en_RL, en_Move, en_Get };

/* ========================================================================== collection loader */
static HRESULT WINAPI cl_QI(IDWriteFontCollectionLoader *This, REFIID riid, void **ppv)
{
    r2_cl *c = (r2_cl *)This; LONG k;
    InterlockedIncrement(&c->n_qi);
    k = c->n_qi - 1; if (k < 8) c->qi_iids[k] = *riid, c->n_qi_iids = (int)(k + 1);
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontCollectionLoader)) { *ppv = This; r2_obj_addref(c); return S_OK; }
    return E_NOINTERFACE;
}
static ULONG WINAPI cl_AR(IDWriteFontCollectionLoader *This) { return r2_obj_addref(This); }
static ULONG WINAPI cl_RL(IDWriteFontCollectionLoader *This) { return r2_obj_release(This); }
static HRESULT WINAPI cl_Create(IDWriteFontCollectionLoader *This, IDWriteFactory *f, const void *key, UINT32 ks, IDWriteFontFileEnumerator **out)
{
    r2_cl *c = (r2_cl *)This; LONG n = InterlockedIncrement(&c->n_create); r2_en *e; int i;
    ev(EV_CL_CREATE, ks, (UINT64)(ULONG_PTR)key, c->id);
    c->last_tid = GetCurrentThreadId(); c->ref_in_create = c->ref; c->csr_in_create = _mm_getcsr();
    c->last_ks = ks; c->last_key_ptr = key; c->last_key_null = key == NULL; c->last_factory = f;
    memset(c->last_key, 0, 16);
    if (key && ks) memcpy(c->last_key, key, ks < sizeof c->last_key ? ks : sizeof c->last_key);
    { int slot = (int)((n - 1) & 255); c->calls[slot].tid = GetCurrentThreadId(); c->calls[slot].ks = ks; memset(c->calls[slot].head, 0, 8); if (key && ks) memcpy(c->calls[slot].head, key, ks < 8 ? ks : 8); }
    if (out) *out = NULL;
    if (c->set_csr) _mm_setcsr(c->set_csr);
    if (c->create_hr && n >= c->create_fail_from) return c->create_hr;
    e = (r2_en *)calloc(1, sizeof *e);
    e->lpVtbl = &en_vt; e->ref = 1; e->id = InterlockedIncrement(&r2_ids); e->pos = -1; e->owner = c; e->has_value = c->has_value;
    e->move_fail_hr = c->move_fail_hr; e->move_fail_at = c->move_fail_at; e->move_fail_has = c->move_fail_has;
    for (i = 0; i < c->nsteps; i++) {
        IDWriteFactory *ff = c->file_factory ? c->file_factory : f;
        e->steps[i] = c->steps[i];
        if (e->steps[i].type == R2_STEP_GET_FAIL) continue;
        if (e->steps[i].prebuilt) { IDWriteFontFile_AddRef(e->steps[i].prebuilt); e->files[i] = e->steps[i].prebuilt; }
        else {
            IDWriteFontFile *file = NULL; UINT32 key4 = e->steps[i].key;
            if (SUCCEEDED(IDWriteFactory_CreateCustomFontFileReference(ff, &key4, 4, (IDWriteFontFileLoader *)c->fl, &file))) e->files[i] = file;
        }
    }
    e->nsteps = c->nsteps;
    if (c->hook) c->hook(c, f, c->hook_arg);
    { LONG slot = InterlockedIncrement(&c->nens) - 1; if (slot < 64) c->ens[slot] = e; }
    c->last_en = e;
    if (out) *out = (IDWriteFontFileEnumerator *)e;
    return S_OK;
}
static const IDWriteFontCollectionLoaderVtbl cl_vt = { cl_QI, cl_AR, cl_RL, cl_Create };
r2_cl *r2_cl_new(r2_fl *fl)
{
    r2_cl *c = (r2_cl *)calloc(1, sizeof *c);
    c->lpVtbl = &cl_vt; c->ref = 1; c->id = InterlockedIncrement(&r2_ids); c->fl = fl; c->has_value = TRUE; c->create_fail_from = 1;
    return c;
}
static void add_step(r2_cl *c, int type, UINT32 key, HRESULT hr)
{
    if (c->nsteps >= R2_MAXSTEP) return;
    memset(&c->steps[c->nsteps], 0, sizeof c->steps[0]);
    c->steps[c->nsteps].type = type; c->steps[c->nsteps].key = key; c->steps[c->nsteps].hr = hr;
    c->nsteps++;
}
void r2_cl_step_file(r2_cl *c, UINT32 key) { add_step(c, R2_STEP_FILE, key, S_OK); }
void r2_cl_step_get_fail(r2_cl *c, UINT32 key, HRESULT hr) { add_step(c, R2_STEP_GET_FAIL, key, hr); }
void r2_cl_step_get_fail_keepout(r2_cl *c, UINT32 key, HRESULT hr) { add_step(c, R2_STEP_GET_FAIL_KEEPOUT, key, hr); }
void r2_cl_clear_steps(r2_cl *c) { c->nsteps = 0; }

/* ========================================================================== generic refcounting */
ULONG r2_obj_addref(void *o)
{
    const void *vt = *(const void **)o;
    if (vt == &cl_vt) { r2_cl *c = o; InterlockedIncrement(&c->n_addref); ev(EV_CL_ADDREF, 0, 0, c->id); return InterlockedIncrement(&c->ref); }
    if (vt == &fl_vt) return fl_AR(o);
    if (vt == &en_vt) return en_AR(o);
    if (vt == &st_vt) return st_AR(o);
    return 0;
}
ULONG r2_obj_release(void *o)
{
    const void *vt = *(const void **)o;
    if (vt == &cl_vt) {
        r2_cl *c = o; LONG r; InterlockedIncrement(&c->n_release); ev(EV_CL_RELEASE, 0, 0, c->id);
        r = InterlockedDecrement(&c->ref);
        if (r < 0) { InterlockedIncrement(&r2_overrel); return 0; }
        if (r == 0) { c->destroyed = 1; ev(EV_CL_DESTROY, 0, 0, c->id); c->lpVtbl = dead_vt; }
        return (ULONG)r;
    }
    if (vt == &fl_vt) return fl_RL(o);
    if (vt == &en_vt) return en_RL(o);
    if (vt == &st_vt) return st_RL(o);
    return 0;
}
LONG r2_obj_ref(void *o) { return ((r2_cl *)o)->ref; }

/* ========================================================================== fixture */
int r2_open(r2_fx *x, DWRITE_FACTORY_TYPE type, int reg_fl, int reg_cl)
{
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(type);
    if (!x->f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    r2_log_reset(); r2_uaf = 0; r2_overrel = 0;
    x->fl = r2_fl_new();
    x->cl = r2_cl_new(x->fl);
    if (reg_fl) { HRESULT hr = r2_fl_register(x->fl, x->f); x->reg_fl = SUCCEEDED(hr); if (FAILED(hr)) t_note("RegisterFontFileLoader: %s", dw_hr_name(hr)); }
    if (reg_cl) { HRESULT hr = IDWriteFactory_RegisterFontCollectionLoader(x->f, CLP(x->cl)); x->reg_cl = SUCCEEDED(hr); if (SUCCEEDED(hr)) x->cl->registered_on = x->f; else t_note("RegisterFontCollectionLoader: %s", dw_hr_name(hr)); }
    x->ok = 1;
    return 1;
}
void r2_close(r2_fx *x)
{
    if (!x->ok) return;
    if (x->reg_cl) { IDWriteFactory_UnregisterFontCollectionLoader(x->f, CLP(x->cl)); x->reg_cl = 0; x->cl->registered_on = NULL; }
    r2_fl_unregister(x->fl); x->reg_fl = 0;
    r2_obj_release(x->cl);
    r2_obj_release(x->fl);
    DW_RELEASE(x->f);
    x->ok = 0;
    CHECKF(r2_uaf == 0, "%ld calls reached destroyed test objects", (long)r2_uaf);
    CHECKF(r2_overrel == 0, "%ld releases below zero on test objects", (long)r2_overrel);
}
UINT32 r2_add_hidden(r2_fx *x, const void *blob, size_t n) { return r2_fl_add(x->fl, blob, n); }
UINT32 r2_add_blob(r2_fx *x, const void *blob, size_t n) { UINT32 k = r2_fl_add(x->fl, blob, n); r2_cl_step_file(x->cl, k); return k; }
UINT32 r2_add_style(r2_fx *x, const char *family, int weight, int width, int italic)
{
    size_t n; uint8_t *b = ttf_build_styled(family, weight, width, italic, &n); UINT32 k;
    if (!b) { t_note("ttf_build_styled failed"); return 0xFFFFFFFFu; }
    k = r2_add_blob(x, b, n); free(b); return k;
}
UINT32 r2_add_family(r2_fx *x, const char *family) { return r2_add_style(x, family, 400, 5, 0); }
int r2_uniq_key(char *buf)
{
    static volatile LONG ctr;
    LONG n = InterlockedIncrement(&ctr);
    snprintf(buf, 16, "r2k%05ld", (long)(n % 100000));
    return 8;
}
HRESULT r2_create_key(r2_fx *x, const void *key, UINT32 ks, IDWriteFontCollection **out)
{
    return IDWriteFactory_CreateCustomFontCollection(x->f, CLP(x->cl), key, ks, out);
}
HRESULT r2_create(r2_fx *x, IDWriteFontCollection **out)
{
    char k[16]; int n = r2_uniq_key(k);
    return r2_create_key(x, k, (UINT32)n, out);
}
int r2_find_family(IDWriteFontCollection *c, const char *name)
{
    WCHAR w[128]; UINT32 idx = 0; BOOL ex = FALSE;
    if (!dw_wide(name, w, 128)) return -1;
    if (FAILED(IDWriteFontCollection_FindFamilyName(c, w, &idx, &ex)) || !ex) return -1;
    return (int)idx;
}
int r2_family_name(IDWriteFontCollection *c, UINT32 i, char *buf, int cap)
{
    IDWriteFontFamily *fam = NULL; IDWriteLocalizedStrings *s = NULL; int ok = 0;
    buf[0] = 0;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) && fam) {
        if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) { ok = SUCCEEDED(dw_ls_en(s, buf, cap)); IDWriteLocalizedStrings_Release(s); }
        IDWriteFontFamily_Release(fam);
    }
    return ok;
}
