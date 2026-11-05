/* Probing IDWriteFontCollectionLoader / IDWriteFontFileEnumerator doubles with an ordered event log. */
#include "fcl.h"

char fcl_ev[FCL_EVMAX]; int fcl_nev;
void fcl_ev_reset(void) { fcl_nev = 0; fcl_ev[0] = 0; }
void fcl_evc(char c) { if (fcl_nev < FCL_EVMAX - 1) { fcl_ev[fcl_nev++] = c; fcl_ev[fcl_nev] = 0; } }

/* ---- enumerator ---- */
static HRESULT WINAPI en_QI(IDWriteFontFileEnumerator *This, REFIID riid, void **ppv)
{
    fcl_en *e = (fcl_en *)This;
    InterlockedIncrement(&e->n_qi);
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontFileEnumerator)) {
        *ppv = This; InterlockedIncrement(&e->n_addref); InterlockedIncrement(&e->ref); return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI en_AR(IDWriteFontFileEnumerator *This)
{
    fcl_en *e = (fcl_en *)This; InterlockedIncrement(&e->n_addref); return InterlockedIncrement(&e->ref);
}
static ULONG WINAPI en_RL(IDWriteFontFileEnumerator *This)
{
    fcl_en *e = (fcl_en *)This; LONG r; int i;
    InterlockedIncrement(&e->n_release);
    r = InterlockedDecrement(&e->ref);
    if (r == 0 && !e->destroyed) {
        e->destroyed = 1; fcl_evc('E');
        for (i = 0; i < e->nfiles; i++) DW_RELEASE(e->files[i]);
    }
    return r < 0 ? 0 : (ULONG)r;
}
static HRESULT WINAPI en_Move(IDWriteFontFileEnumerator *This, BOOL *has)
{
    fcl_en *e = (fcl_en *)This; LONG n = InterlockedIncrement(&e->n_move);
    fcl_evc('M');
    if (has) *has = FALSE;
    if (e->ended) e->extra_moves_after_end++;
    if (e->move_hr_at && n == e->move_hr_at) { if (has) *has = e->move_hr_true ? TRUE : FALSE; return e->move_hr; }
    if (e->pos < e->nfiles) e->pos++;
    *has = e->pos < e->nfiles;
    if (!*has) e->ended = 1;
    return S_OK;
}
static HRESULT WINAPI en_Get(IDWriteFontFileEnumerator *This, IDWriteFontFile **out)
{
    fcl_en *e = (fcl_en *)This;
    InterlockedIncrement(&e->n_get);
    fcl_evc('G');
    if (out) *out = NULL;
    if (e->pos < 0 || e->pos >= e->nfiles) { e->get_out_of_sequence++; return e->after_end_hr ? e->after_end_hr : E_FAIL; }
    if (e->get_hr && (e->get_mask & (1u << e->pos))) return e->get_hr;
    IDWriteFontFile_AddRef(e->files[e->pos]);
    *out = e->files[e->pos];
    return S_OK;
}
static const IDWriteFontFileEnumeratorVtbl en_vt = { en_QI, en_AR, en_RL, en_Move, en_Get };

/* ---- collection loader ---- */
static HRESULT WINAPI cl_QI(IDWriteFontCollectionLoader *This, REFIID riid, void **ppv)
{
    fcl_cl *c = (fcl_cl *)This;
    InterlockedIncrement(&c->n_qi);
    if (c->n_qi_iids < 8) c->qi_iids[c->n_qi_iids++] = *riid;
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDWriteFontCollectionLoader)) {
        *ppv = This; InterlockedIncrement(&c->n_addref); InterlockedIncrement(&c->ref); return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI cl_AR(IDWriteFontCollectionLoader *This)
{
    fcl_cl *c = (fcl_cl *)This; InterlockedIncrement(&c->n_addref); return InterlockedIncrement(&c->ref);
}
static ULONG WINAPI cl_RL(IDWriteFontCollectionLoader *This)
{
    fcl_cl *c = (fcl_cl *)This; LONG r;
    InterlockedIncrement(&c->n_release);
    r = InterlockedDecrement(&c->ref);
    if (r == 0 && !c->destroyed) { c->destroyed = 1; fcl_evc('L'); }
    return r < 0 ? 0 : (ULONG)r;
}
static HRESULT WINAPI cl_Create(IDWriteFontCollectionLoader *This, IDWriteFactory *f, const void *key, UINT32 ks, IDWriteFontFileEnumerator **out)
{
    fcl_cl *c = (fcl_cl *)This; fcl_en *e; int i;
    InterlockedIncrement(&c->n_create);
    fcl_evc('C');
    c->tid = GetCurrentThreadId();
    c->last_key = key; c->last_ks = ks; c->key_was_null = key == NULL;
    memset(c->keycopy, 0, sizeof c->keycopy);
    if (key && ks) memcpy(c->keycopy, key, ks < sizeof c->keycopy ? ks : sizeof c->keycopy);
    c->last_factory = f;
    c->factory_identity_ok = f && c->expect_factory && dw_same_identity((IUnknown *)f, (IUnknown *)c->expect_factory);
    if (out) *out = NULL;
    if (c->create_hr && !c->create_hr_positive) return c->create_hr;
    e = (fcl_en *)calloc(1, sizeof *e);
    e->lpVtbl = &en_vt; e->ref = 1; e->pos = -1; e->owner = c; e->after_end_hr = E_FAIL;
    e->move_hr = c->move_hr; e->move_hr_at = c->move_hr_at; e->move_hr_true = c->move_hr_true;
    e->get_hr = c->get_hr; e->get_mask = c->get_mask;
    for (i = 0; i < c->nkeys && e->nfiles < FCL_MAXF; i++) {
        IDWriteFontFile *file = NULL;
        if (f && SUCCEEDED(dwx_loader_file(c->fl, f, c->keys[i], &file)) && file) e->files[e->nfiles++] = file;
    }
    { LONG slot = InterlockedIncrement(&c->nens) - 1; if (slot < FCL_MAXF) c->ens[slot] = e; }
    c->last_en = e;
    if (out) *out = (IDWriteFontFileEnumerator *)e;
    return c->create_hr_positive ? c->create_hr : S_OK;
}
static const IDWriteFontCollectionLoaderVtbl cl_vt = { cl_QI, cl_AR, cl_RL, cl_Create };

fcl_cl *fcl_cl_new(dwx_mem_loader *fl)
{
    fcl_cl *c = (fcl_cl *)calloc(1, sizeof *c);
    c->lpVtbl = &cl_vt; c->ref = 1; c->fl = fl;
    return c;
}
void fcl_cl_add(fcl_cl *c, UINT32 k) { if (c->nkeys < FCL_MAXF) c->keys[c->nkeys++] = k; }
HRESULT fcl_cl_register(fcl_cl *c, IDWriteFactory *f) { return IDWriteFactory_RegisterFontCollectionLoader(f, (IDWriteFontCollectionLoader *)c); }
HRESULT fcl_cl_unregister(fcl_cl *c, IDWriteFactory *f) { return IDWriteFactory_UnregisterFontCollectionLoader(f, (IDWriteFontCollectionLoader *)c); }
ULONG fcl_cl_release(fcl_cl *c) { return cl_RL((IDWriteFontCollectionLoader *)c); }
