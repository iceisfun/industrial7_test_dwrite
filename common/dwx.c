/* COM test doubles; see dwx.h. */
#include "dwx.h"

/* ---- registry / generic IUnknown ------------------------------------------------------------ */
static CRITICAL_SECTION g_cs; static int g_cs_init; static dwx_base *g_head; static volatile LONG g_live;
static void lock(void) { if (!g_cs_init) { InitializeCriticalSection(&g_cs); g_cs_init = 1; } EnterCriticalSection(&g_cs); }
static void unlock(void) { LeaveCriticalSection(&g_cs); }
static void *dwx_alloc(size_t size, void (*dtor)(void *)) {
    void *o = calloc(1, size);
    dwx_base *b = DWX_BASE(o);
    b->ref = 1; b->dtor = dtor; b->self = o; b->live_counted = 1;
    lock(); b->next = g_head; g_head = b; unlock();
    InterlockedIncrement(&g_live);
    return o;
}
int dwx_live(void) { return (int)g_live; }
void dwx_free(void *o) {
    dwx_base *b = DWX_BASE(o), **pp;
    lock();
    for (pp = &g_head; *pp; pp = &(*pp)->next) if (*pp == b) { *pp = b->next; break; }
    unlock();
    if (b->live_counted) InterlockedDecrement(&g_live);
    if (b->dtor) b->dtor(o);
    free(o);
}
void dwx_free_all(void) { while (g_head) dwx_free(g_head->self); }
ULONG dwx_addref(void *o) { dwx_base *b = DWX_BASE(o); InterlockedIncrement(&b->addref_calls); return InterlockedIncrement(&b->ref); }
ULONG dwx_release(void *o) {
    dwx_base *b = DWX_BASE(o); LONG r;
    InterlockedIncrement(&b->release_calls);
    r = InterlockedDecrement(&b->ref);
    if (r == 0 && b->live_counted) { b->destroyed = 1; b->live_counted = 0; InterlockedDecrement(&g_live); if (b->on_zero) b->on_zero(o); }
    return r < 0 ? 0 : r;
}
static HRESULT dwx_qi(void *o, REFIID riid, void **ppv, const IID *const *ids, int n) {
    dwx_base *b = DWX_BASE(o); int i;
    InterlockedIncrement(&b->qi_calls);
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (!IsEqualIID(riid, &IID_IUnknown)) {
        for (i = 0; i < n; i++) if (IsEqualIID(riid, ids[i])) break;
        if (i == n) return E_NOINTERFACE;
    }
    *ppv = o; dwx_addref(o);
    return S_OK;
}
#define UNK(P, T, IDS, N) \
    static HRESULT WINAPI P##_QI(T *This, REFIID r, void **o) { return dwx_qi(This, r, o, IDS, N); } \
    static ULONG WINAPI P##_AR(T *This) { return dwx_addref(This); } \
    static ULONG WINAPI P##_RL(T *This) { return dwx_release(This); }
static void *dup_mem(const void *p, size_t n) { void *d; if (!p) return NULL; d = malloc(n ? n : 1); memcpy(d, p, n); return d; }
static WCHAR *dup_w(const WCHAR *s, size_t n) { WCHAR *d; if (!s) return NULL; d = (WCHAR *)malloc((n + 1) * sizeof(WCHAR)); memcpy(d, s, n * sizeof(WCHAR)); d[n] = 0; return d; }

/* ===== memory font stream ================================================================= */
static void *g_issued[1024]; static int g_nissued;     /* process-wide set of outstanding fragment contexts */
static const IID *const ids_stream[] = { &IID_IDWriteFontFileStream };
UNK(st, IDWriteFontFileStream, ids_stream, 1)
static HRESULT WINAPI st_Read(IDWriteFontFileStream *This, const void **start, UINT64 off, UINT64 size, void **ctx) {
    dwx_mem_stream *s = (dwx_mem_stream *)This; LONG idx = InterlockedIncrement(&s->n_read) - 1;
    if (start) *start = NULL; if (ctx) *ctx = NULL;
    if (idx < 256) { s->reads[idx].offset = off; s->reads[idx].size = size; s->nlogged = (int)idx + 1; }
    s->last_offset = off; s->last_size = size;
    if (s->fail_read && idx >= s->fail_read_from) return s->fail_read;
    if (off > s->size || size > s->size - off) return E_FAIL;
    {
        void *tok;
        if (s->copy_fragments) { tok = malloc(size ? (size_t)size : 1); memcpy(tok, s->data + off, (size_t)size); *start = tok; }
        else { *start = s->data + off; tok = (void *)(ULONG_PTR)(0x10000 + idx); }
        lock(); if (g_nissued < 1024) g_issued[g_nissued++] = tok; unlock();
        *ctx = tok;
    }
    { LONG o = InterlockedIncrement(&s->outstanding); if (o > s->max_outstanding) s->max_outstanding = o; }
    return S_OK;
}
static void WINAPI st_ReleaseFrag(IDWriteFontFileStream *This, void *ctx) {
    dwx_mem_stream *s = (dwx_mem_stream *)This; int i, found = 0;
    InterlockedIncrement(&s->n_release);
    lock();
    for (i = 0; i < g_nissued; i++) if (g_issued[i] == ctx) { g_issued[i] = g_issued[--g_nissued]; found = 1; break; }
    unlock();
    if (!found) { s->bad_release_ctx++; return; }
    if (s->copy_fragments) free(ctx);
    InterlockedDecrement(&s->outstanding);
}
static HRESULT WINAPI st_GetFileSize(IDWriteFontFileStream *This, UINT64 *size) {
    dwx_mem_stream *s = (dwx_mem_stream *)This; InterlockedIncrement(&s->n_getsize);
    if (size) *size = 0;
    if (s->fail_getsize) return s->fail_getsize;
    *size = s->size; return S_OK;
}
static HRESULT WINAPI st_GetLastWrite(IDWriteFontFileStream *This, UINT64 *t) {
    dwx_mem_stream *s = (dwx_mem_stream *)This; InterlockedIncrement(&s->n_lastwrite);
    if (t) *t = 0;
    if (s->lastwrite_notimpl) return E_NOTIMPL;
    if (s->fail_lastwrite) return s->fail_lastwrite;
    *t = s->last_write; return S_OK;
}
static const IDWriteFontFileStreamVtbl st_vt = { st_QI, st_AR, st_RL, st_Read, st_ReleaseFrag, st_GetFileSize, st_GetLastWrite };

/* ===== memory font file loader ================================================================ */
static const IID *const ids_loader[] = { &IID_IDWriteFontFileLoader };
UNK(ld, IDWriteFontFileLoader, ids_loader, 1)
static HRESULT WINAPI ld_Create(IDWriteFontFileLoader *This, const void *key, UINT32 size, IDWriteFontFileStream **out) {
    dwx_mem_loader *l = (dwx_mem_loader *)This; dwx_mem_stream *s; UINT32 k;
    InterlockedIncrement(&l->n_create);
    if (out) *out = NULL;
    l->last_key_size = size; memset(l->last_key, 0, sizeof l->last_key);
    if (key && size) memcpy(l->last_key, key, size < 64 ? size : 64);
    if (l->fail_create) return l->fail_create;
    if (!out || !key || (l->fail_key_size_check && size != 4)) return E_INVALIDARG;
    memcpy(&k, key, 4); l->last_key_value = k;
    if (k >= (UINT32)l->nblobs) return E_INVALIDARG;
    s = (dwx_mem_stream *)dwx_alloc(sizeof *s, NULL);
    s->lpVtbl = &st_vt; s->data = l->blob[k].data; s->size = l->blob[k].size; s->last_write = l->last_write;
    s->copy_fragments = l->stream_copy_fragments; s->fail_read = l->stream_fail_read; s->fail_getsize = l->stream_fail_getsize;
    s->loader = l;
    lock(); if (l->nstreams < 128) l->streams[l->nstreams++] = s;
    l->last_stream = s; unlock();
    *out = (IDWriteFontFileStream *)s;
    return S_OK;
}
static const IDWriteFontFileLoaderVtbl ld_vt = { ld_QI, ld_AR, ld_RL, ld_Create };
static void ld_dtor(void *o) { dwx_mem_loader *l = o; int i; for (i = 0; i < l->nblobs; i++) free(l->blob[i].data); }
dwx_mem_loader *dwx_loader_new(void) {
    dwx_mem_loader *l = (dwx_mem_loader *)dwx_alloc(sizeof *l, ld_dtor);
    l->lpVtbl = &ld_vt; l->fail_key_size_check = 1; l->last_write = 0x01D0000000000000ull;
    return l;
}
UINT32 dwx_loader_add(dwx_mem_loader *l, const void *data, size_t size) {
    UINT32 k = (UINT32)l->nblobs;
    if (l->nblobs >= DWX_MAX_BLOBS) return 0xFFFFFFFFu;
    l->blob[l->nblobs].data = (uint8_t *)dup_mem(data, size); l->blob[l->nblobs].size = size; l->nblobs++;
    return k;
}
HRESULT dwx_loader_register(dwx_mem_loader *l, IDWriteFactory *f) {
    HRESULT hr = IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)l);
    if (SUCCEEDED(hr)) { l->registered = 1; l->reg_factory = f; }
    return hr;
}
HRESULT dwx_loader_unregister(dwx_mem_loader *l) {
    HRESULT hr = S_OK;
    if (l->registered) { hr = IDWriteFactory_UnregisterFontFileLoader(l->reg_factory, (IDWriteFontFileLoader *)l); l->registered = 0; }
    return hr;
}
HRESULT dwx_loader_file(dwx_mem_loader *l, IDWriteFactory *f, UINT32 key, IDWriteFontFile **out) {
    return IDWriteFactory_CreateCustomFontFileReference(f, &key, sizeof key, (IDWriteFontFileLoader *)l, out);
}
LONG dwx_loader_total_reads(dwx_mem_loader *l) { LONG t = 0; int i; for (i = 0; i < l->nstreams; i++) t += l->streams[i]->n_read; return t; }
LONG dwx_loader_total_outstanding(dwx_mem_loader *l) { LONG t = 0; int i; for (i = 0; i < l->nstreams; i++) t += l->streams[i]->outstanding; return t; }

HRESULT dwx_font_open(dwx_font *h, IDWriteFactory *f, const void *blob, size_t size, UINT32 idx, DWRITE_FONT_SIMULATIONS sim) {
    HRESULT hr;
    memset(h, 0, sizeof *h); h->f = f;
    h->ld = dwx_loader_new();
    h->key = dwx_loader_add(h->ld, blob, size);
    hr = dwx_loader_register(h->ld, f);
    if (FAILED(hr)) return hr;
    hr = dwx_loader_file(h->ld, f, h->key, &h->file);
    if (FAILED(hr)) return hr;
    hr = IDWriteFontFile_Analyze(h->file, &h->supported, &h->file_type, &h->face_type, &h->nfaces);
    if (FAILED(hr)) return hr;
    if (!h->supported) return S_FALSE;
    hr = IDWriteFactory_CreateFontFace(f, h->face_type, 1, &h->file, idx, sim, &h->face);
    return hr;
}
HRESULT dwx_font_open_default(dwx_font *h, IDWriteFactory *f) {
    size_t n; uint8_t *b = ttf_build_default(&n); HRESULT hr = dwx_font_open(h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(b); return hr;
}
void dwx_font_close(dwx_font *h) {
    DW_RELEASE(h->face); DW_RELEASE(h->file);
    if (h->ld) { dwx_loader_unregister(h->ld); dwx_release(h->ld); h->ld = NULL; }
}

/* ===== font file enumerator + collection loader ======================================================= */
static const IID *const ids_enum[] = { &IID_IDWriteFontFileEnumerator };
UNK(en, IDWriteFontFileEnumerator, ids_enum, 1)
static HRESULT WINAPI en_MoveNext(IDWriteFontFileEnumerator *This, BOOL *has) {
    dwx_enum *e = (dwx_enum *)This; LONG n = InterlockedIncrement(&e->n_movenext);
    if (has) *has = FALSE;
    if (e->fail_move && (e->fail_move_at == 0 || e->fail_move_at == n)) return e->fail_move;
    if (e->pos < e->nkeys) e->pos++;
    *has = e->pos < e->nkeys;
    return S_OK;
}
static HRESULT WINAPI en_GetCurrent(IDWriteFontFileEnumerator *This, IDWriteFontFile **file) {
    dwx_enum *e = (dwx_enum *)This; InterlockedIncrement(&e->n_getcurrent);
    if (file) *file = NULL;
    if (e->fail_get) return e->fail_get;
    if (e->pos < 0 || e->pos >= e->nkeys) return e->get_without_current_hr_set ? e->get_without_current_hr : E_FAIL;
    return dwx_loader_file(e->files, e->factory, e->keys[e->pos], file);
}
static const IDWriteFontFileEnumeratorVtbl en_vt = { en_QI, en_AR, en_RL, en_MoveNext, en_GetCurrent };
static const IID *const ids_cl[] = { &IID_IDWriteFontCollectionLoader };
UNK(cl, IDWriteFontCollectionLoader, ids_cl, 1)
static HRESULT WINAPI cl_Create(IDWriteFontCollectionLoader *This, IDWriteFactory *f, const void *key, UINT32 size, IDWriteFontFileEnumerator **out) {
    dwx_coll_loader *c = (dwx_coll_loader *)This; dwx_enum *e;
    InterlockedIncrement(&c->n_create);
    if (out) *out = NULL;
    c->last_factory = f; c->last_key_size = size; memset(c->last_key, 0, sizeof c->last_key);
    if (key && size) memcpy(c->last_key, key, size < 64 ? size : 64);
    if (c->fail_create) return c->fail_create;
    if (!out) return E_INVALIDARG;
    e = (dwx_enum *)dwx_alloc(sizeof *e, NULL);
    e->lpVtbl = &en_vt; e->files = c->files; e->factory = f; e->pos = -1;
    e->nkeys = c->nkeys; memcpy(e->keys, c->keys, sizeof e->keys);
    e->fail_move = c->enum_fail_move; e->fail_get = c->enum_fail_get;
    if (c->nenums < 32) c->enums[c->nenums++] = e;
    c->last_enum = e;
    *out = (IDWriteFontFileEnumerator *)e;
    return S_OK;
}
static const IDWriteFontCollectionLoaderVtbl cl_vt = { cl_QI, cl_AR, cl_RL, cl_Create };
static void cl_zero(void *o) { dwx_coll_loader *c = o; if (c->files) { dwx_release(c->files); c->files = NULL; } }
dwx_coll_loader *dwx_coll_new(dwx_mem_loader *files) {
    dwx_coll_loader *c = (dwx_coll_loader *)dwx_alloc(sizeof *c, NULL);
    DWX_BASE(c)->on_zero = cl_zero;
    c->lpVtbl = &cl_vt; c->files = files; if (files) dwx_addref(files);
    return c;
}
void dwx_coll_add_key(dwx_coll_loader *c, UINT32 k) { if (c->nkeys < DWX_MAX_BLOBS) c->keys[c->nkeys++] = k; }
HRESULT dwx_coll_register(dwx_coll_loader *c, IDWriteFactory *f) {
    HRESULT hr = IDWriteFactory_RegisterFontCollectionLoader(f, (IDWriteFontCollectionLoader *)c);
    if (SUCCEEDED(hr)) { c->registered = 1; c->reg_factory = f; }
    return hr;
}
HRESULT dwx_coll_unregister(dwx_coll_loader *c) {
    HRESULT hr = S_OK;
    if (c->registered) { hr = IDWriteFactory_UnregisterFontCollectionLoader(c->reg_factory, (IDWriteFontCollectionLoader *)c); c->registered = 0; }
    return hr;
}
HRESULT dwx_coll_create(dwx_coll_loader *c, IDWriteFactory *f, IDWriteFontCollection **out) {
    return IDWriteFactory_CreateCustomFontCollection(f, (IDWriteFontCollectionLoader *)c, "dwx", 4, out);
}

/* ===== effect (plain IUnknown) =================================================================== */
UNK(fx, IUnknown, ((const IID *const *)NULL), 0)
static const IUnknownVtbl fx_vt = { fx_QI, fx_AR, fx_RL };
dwx_effect *dwx_effect_new(int tag) { dwx_effect *e = (dwx_effect *)dwx_alloc(sizeof *e, NULL); e->lpVtbl = &fx_vt; e->tag = tag; return e; }

/* ===== text renderer =============================================================================== */
static void draw_free(dwx_draw *d) {
    free(d->indices); free(d->advances); free(d->offsets); free(d->locale); free(d->text); free(d->cluster_map); free(d->line_locale);
    memset(d, 0, sizeof *d);
}
static void rd_dtor(void *o) { dwx_renderer *r = o; int i; for (i = 0; i < r->ncalls; i++) draw_free(&r->calls[i]); free(r->calls); }
void dwx_renderer_clear(dwx_renderer *r) {
    int i; for (i = 0; i < r->ncalls; i++) draw_free(&r->calls[i]);
    r->ncalls = 0; memset(r->count, 0, sizeof r->count);
}
static dwx_draw *rec(dwx_renderer *r, int kind, void *ctx, float x, float y, IUnknown *effect) {
    dwx_draw *d;
    if (r->ncalls == r->cap) { r->cap = r->cap ? r->cap * 2 : 32; r->calls = (dwx_draw *)realloc(r->calls, r->cap * sizeof(dwx_draw)); }
    d = &r->calls[r->ncalls]; memset(d, 0, sizeof *d);
    d->kind = kind; d->seq = ++r->ncalls; d->ctx = ctx; d->x = x; d->y = y; d->effect = effect; d->effect_refs = -1;
    r->count[kind]++;
    if (effect && *(const void **)effect == &fx_vt) d->effect_refs = dwx_refs(effect);
    return d;
}
static HRESULT draw_result(dwx_renderer *r, int seq) { return (r->fail_draw && (r->fail_at == 0 || r->fail_at == seq)) ? r->fail_draw : S_OK; }
static void copy_run(dwx_draw *d, DWRITE_MEASURING_MODE mm, const DWRITE_GLYPH_RUN *g, const DWRITE_GLYPH_RUN_DESCRIPTION *desc) {
    d->mm = mm;
    if (g) {
        d->face = g->fontFace; d->em = g->fontEmSize; d->nglyphs = g->glyphCount; d->sideways = g->isSideways; d->bidi = (UINT8)g->bidiLevel;
        d->indices = (UINT16 *)dup_mem(g->glyphIndices, g->glyphCount * sizeof(UINT16));
        d->advances = (float *)dup_mem(g->glyphAdvances, g->glyphCount * sizeof(float));
        d->offsets = (DWRITE_GLYPH_OFFSET *)dup_mem(g->glyphOffsets, g->glyphCount * sizeof(DWRITE_GLYPH_OFFSET));
    }
    if (desc) {
        d->has_desc = 1; d->locale = dup_w(desc->localeName, desc->localeName ? lstrlenW(desc->localeName) : 0);
        d->text = dup_w(desc->string, desc->stringLength); d->text_len = desc->stringLength; d->text_pos = desc->textPosition;
        d->cluster_map = (UINT16 *)dup_mem(desc->clusterMap, desc->stringLength * sizeof(UINT16));
    }
}
static const IID *const ids_rd[] = { &IID_IDWritePixelSnapping, &IID_IDWriteTextRenderer };
static const IID *const ids_rd1[] = { &IID_IDWritePixelSnapping, &IID_IDWriteTextRenderer, &IID_IDWriteTextRenderer1 };
static HRESULT WINAPI rd_QI(IDWriteTextRenderer1 *This, REFIID r, void **o) { dwx_renderer *x = (dwx_renderer *)This; return x->support_r1 ? dwx_qi(This, r, o, ids_rd1, 3) : dwx_qi(This, r, o, ids_rd, 2); }
static ULONG WINAPI rd_AR(IDWriteTextRenderer1 *This) { return dwx_addref(This); }
static ULONG WINAPI rd_RL(IDWriteTextRenderer1 *This) { return dwx_release(This); }
static HRESULT WINAPI rd_Snap(IDWriteTextRenderer1 *This, void *ctx, BOOL *v) {
    dwx_renderer *r = (dwx_renderer *)This; InterlockedIncrement(&r->n_snapping); r->last_snap_ctx = ctx;
    if (v) *v = FALSE; if (r->fail_snapping) return r->fail_snapping;
    *v = r->snapping_disabled; return S_OK;
}
static HRESULT WINAPI rd_Xform(IDWriteTextRenderer1 *This, void *ctx, DWRITE_MATRIX *m) {
    dwx_renderer *r = (dwx_renderer *)This; InterlockedIncrement(&r->n_transform); r->last_snap_ctx = ctx;
    if (r->fail_transform) return r->fail_transform;
    *m = r->transform; return S_OK;
}
static HRESULT WINAPI rd_Ppd(IDWriteTextRenderer1 *This, void *ctx, FLOAT *v) {
    dwx_renderer *r = (dwx_renderer *)This; InterlockedIncrement(&r->n_ppd); r->last_snap_ctx = ctx;
    if (r->fail_ppd) return r->fail_ppd;
    *v = r->pixels_per_dip; return S_OK;
}
static HRESULT WINAPI rd_Glyph(IDWriteTextRenderer1 *This, void *ctx, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE mm, const DWRITE_GLYPH_RUN *g, const DWRITE_GLYPH_RUN_DESCRIPTION *desc, IUnknown *fx) {
    dwx_renderer *r = (dwx_renderer *)This; dwx_draw *d = rec(r, DWX_DRAW_GLYPHRUN, ctx, x, y, fx);
    copy_run(d, mm, g, desc); return draw_result(r, d->seq);
}
static void copy_line(dwx_draw *d, const DWRITE_UNDERLINE *u, const DWRITE_STRIKETHROUGH *s) {
    if (u) { d->ul = *u; d->line_locale = dup_w(u->localeName, u->localeName ? lstrlenW(u->localeName) : 0); d->ul.localeName = d->line_locale; d->mm = u->measuringMode; }
    if (s) { d->st = *s; d->line_locale = dup_w(s->localeName, s->localeName ? lstrlenW(s->localeName) : 0); d->st.localeName = d->line_locale; d->mm = s->measuringMode; }
}
static HRESULT WINAPI rd_Under(IDWriteTextRenderer1 *This, void *ctx, FLOAT x, FLOAT y, const DWRITE_UNDERLINE *u, IUnknown *fx) {
    dwx_renderer *r = (dwx_renderer *)This; dwx_draw *d = rec(r, DWX_DRAW_UNDERLINE, ctx, x, y, fx);
    copy_line(d, u, NULL); return draw_result(r, d->seq);
}
static HRESULT WINAPI rd_Strike(IDWriteTextRenderer1 *This, void *ctx, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH *s, IUnknown *fx) {
    dwx_renderer *r = (dwx_renderer *)This; dwx_draw *d = rec(r, DWX_DRAW_STRIKETHROUGH, ctx, x, y, fx);
    copy_line(d, NULL, s); return draw_result(r, d->seq);
}
static HRESULT WINAPI rd_Inline(IDWriteTextRenderer1 *This, void *ctx, FLOAT x, FLOAT y, IDWriteInlineObject *obj, BOOL sw, BOOL rtl, IUnknown *fx) {
    dwx_renderer *r = (dwx_renderer *)This; dwx_draw *d = rec(r, DWX_DRAW_INLINE, ctx, x, y, fx);
    int seq = d->seq;
    d->obj = obj; d->obj_sideways = sw; d->obj_rtl = rtl;
    if (r->recurse_inline && obj) { HRESULT hr = IDWriteInlineObject_Draw(obj, ctx, (IDWriteTextRenderer *)This, x, y, sw, rtl, fx); if (FAILED(hr)) return hr; }
    return draw_result(r, seq);
}
static HRESULT WINAPI rd1_Glyph(IDWriteTextRenderer1 *This, void *ctx, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, DWRITE_MEASURING_MODE mm, const DWRITE_GLYPH_RUN *g, const DWRITE_GLYPH_RUN_DESCRIPTION *desc, IUnknown *fx) {
    dwx_renderer *r = (dwx_renderer *)This; dwx_draw *d = rec(r, DWX_DRAW_GLYPHRUN, ctx, x, y, fx);
    InterlockedIncrement(&r->r1_calls); d->has_orientation = 1; d->angle = a; copy_run(d, mm, g, desc); return draw_result(r, d->seq);
}
static HRESULT WINAPI rd1_Under(IDWriteTextRenderer1 *This, void *ctx, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, const DWRITE_UNDERLINE *u, IUnknown *fx) {
    dwx_renderer *r = (dwx_renderer *)This; dwx_draw *d = rec(r, DWX_DRAW_UNDERLINE, ctx, x, y, fx);
    InterlockedIncrement(&r->r1_calls); d->has_orientation = 1; d->angle = a; copy_line(d, u, NULL); return draw_result(r, d->seq);
}
static HRESULT WINAPI rd1_Strike(IDWriteTextRenderer1 *This, void *ctx, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, const DWRITE_STRIKETHROUGH *s, IUnknown *fx) {
    dwx_renderer *r = (dwx_renderer *)This; dwx_draw *d = rec(r, DWX_DRAW_STRIKETHROUGH, ctx, x, y, fx);
    InterlockedIncrement(&r->r1_calls); d->has_orientation = 1; d->angle = a; copy_line(d, NULL, s); return draw_result(r, d->seq);
}
static HRESULT WINAPI rd1_Inline(IDWriteTextRenderer1 *This, void *ctx, FLOAT x, FLOAT y, DWRITE_GLYPH_ORIENTATION_ANGLE a, IDWriteInlineObject *obj, BOOL sw, BOOL rtl, IUnknown *fx) {
    dwx_renderer *r = (dwx_renderer *)This; dwx_draw *d = rec(r, DWX_DRAW_INLINE, ctx, x, y, fx);
    InterlockedIncrement(&r->r1_calls); d->has_orientation = 1; d->angle = a; d->obj = obj; d->obj_sideways = sw; d->obj_rtl = rtl;
    if (r->recurse_inline && obj) { int seq = d->seq; HRESULT hr = IDWriteInlineObject_Draw(obj, ctx, (IDWriteTextRenderer *)This, x, y, sw, rtl, fx); if (FAILED(hr)) return hr; return draw_result(r, seq); }
    return draw_result(r, d->seq);
}
static const IDWriteTextRenderer1Vtbl rd_vt = { rd_QI, rd_AR, rd_RL, rd_Snap, rd_Xform, rd_Ppd, rd_Glyph, rd_Under, rd_Strike, rd_Inline,
                                                rd1_Glyph, rd1_Under, rd1_Strike, rd1_Inline };
dwx_renderer *dwx_renderer_new(void) {
    dwx_renderer *r = (dwx_renderer *)dwx_alloc(sizeof *r, rd_dtor);
    r->lpVtbl = &rd_vt; r->transform.m11 = 1.0f; r->transform.m22 = 1.0f; r->pixels_per_dip = 1.0f;
    return r;
}
const dwx_draw *dwx_renderer_call(dwx_renderer *r, int i) { return (i >= 0 && i < r->ncalls) ? &r->calls[i] : NULL; }
const dwx_draw *dwx_renderer_next(dwx_renderer *r, int kind, int *cursor) {
    int i;
    if (*cursor < 0) *cursor = 0;
    for (i = *cursor; i < r->ncalls; i++) if (r->calls[i].kind == kind) { *cursor = i + 1; return &r->calls[i]; }
    *cursor = r->ncalls; return NULL;
}
float dwx_run_width(const dwx_draw *d) { float w = 0; UINT32 i; if (d->advances) for (i = 0; i < d->nglyphs; i++) w += d->advances[i]; return w; }

/* ===== inline object ================================================================================ */
static const IID *const ids_io[] = { &IID_IDWriteInlineObject };
UNK(io, IDWriteInlineObject, ids_io, 1)
static HRESULT WINAPI io_Draw(IDWriteInlineObject *This, void *ctx, IDWriteTextRenderer *rd, FLOAT x, FLOAT y, BOOL sw, BOOL rtl, IUnknown *fx) {
    dwx_inline *o = (dwx_inline *)This; InterlockedIncrement(&o->n_draw);
    o->last_ctx = ctx; o->last_renderer = rd; o->last_x = x; o->last_y = y; o->last_sideways = sw; o->last_rtl = rtl; o->last_effect = fx;
    return o->fail_draw ? o->fail_draw : S_OK;
}
static HRESULT WINAPI io_Metrics(IDWriteInlineObject *This, DWRITE_INLINE_OBJECT_METRICS *m) {
    dwx_inline *o = (dwx_inline *)This; InterlockedIncrement(&o->n_metrics);
    if (o->fail_metrics) return o->fail_metrics;
    *m = o->metrics; return S_OK;
}
static HRESULT WINAPI io_Overhang(IDWriteInlineObject *This, DWRITE_OVERHANG_METRICS *m) {
    dwx_inline *o = (dwx_inline *)This; InterlockedIncrement(&o->n_overhang);
    if (o->fail_overhang) return o->fail_overhang;
    *m = o->overhang; return S_OK;
}
static HRESULT WINAPI io_Break(IDWriteInlineObject *This, DWRITE_BREAK_CONDITION *b, DWRITE_BREAK_CONDITION *a) {
    dwx_inline *o = (dwx_inline *)This; InterlockedIncrement(&o->n_break);
    if (o->fail_break) return o->fail_break;
    *b = o->before; *a = o->after; return S_OK;
}
static const IDWriteInlineObjectVtbl io_vt = { io_QI, io_AR, io_RL, io_Draw, io_Metrics, io_Overhang, io_Break };
dwx_inline *dwx_inline_new(float w, float h, float base, BOOL sideways) {
    dwx_inline *o = (dwx_inline *)dwx_alloc(sizeof *o, NULL);
    o->lpVtbl = &io_vt; o->metrics.width = w; o->metrics.height = h; o->metrics.baseline = base; o->metrics.supportsSideways = sideways;
    o->before = o->after = DWRITE_BREAK_CONDITION_NEUTRAL;
    return o;
}

/* ===== geometry sink ================================================================================= */
static const IID *const ids_gs[] = { &IID_ID2D1SimplifiedGeometrySink };
UNK(gs, ID2D1SimplifiedGeometrySink, ids_gs, 1)
static void gs_ev(dwx_geom *g, int kind, int count, int arg, D2D1_POINT_2F pt) {
    if (g->close_calls) g->calls_after_close++;
    if (g->nev < 2048) { g->ev[g->nev].kind = kind; g->ev[g->nev].count = count; g->ev[g->nev].arg = arg; g->ev[g->nev].pt = pt; g->nev++; }
}
static void gs_pt(dwx_geom *g, D2D1_POINT_2F p) { if (g->npts < 8192) g->pts[g->npts++] = p; }
static void WINAPI gs_Fill(ID2D1SimplifiedGeometrySink *This, D2D1_FILL_MODE m) { dwx_geom *g = (dwx_geom *)This; D2D1_POINT_2F z = { 0, 0 }; g->fill_mode = m; g->fill_calls++; gs_ev(g, DWX_G_FILL, 0, m, z); }
static void WINAPI gs_Seg(ID2D1SimplifiedGeometrySink *This, D2D1_PATH_SEGMENT f) { dwx_geom *g = (dwx_geom *)This; D2D1_POINT_2F z = { 0, 0 }; g->seg_flags = f; gs_ev(g, DWX_G_SEGFLAGS, 0, f, z); }
static void WINAPI gs_Begin(ID2D1SimplifiedGeometrySink *This, D2D1_POINT_2F p, D2D1_FIGURE_BEGIN b) {
    dwx_geom *g = (dwx_geom *)This; if (g->open_figure) g->order_violations++;
    g->open_figure = 1; g->nfigures_begun++; gs_pt(g, p); gs_ev(g, DWX_G_BEGIN, 1, b, p);
}
static void WINAPI gs_Lines(ID2D1SimplifiedGeometrySink *This, const D2D1_POINT_2F *p, UINT32 n) {
    dwx_geom *g = (dwx_geom *)This; D2D1_POINT_2F z = { 0, 0 }; UINT32 i; if (!g->open_figure) g->order_violations++;
    for (i = 0; i < n; i++) gs_pt(g, p[i]);
    g->nlines += n; gs_ev(g, DWX_G_LINES, n, 0, z);
}
static void WINAPI gs_Bez(ID2D1SimplifiedGeometrySink *This, const D2D1_BEZIER_SEGMENT *b, UINT32 n) {
    dwx_geom *g = (dwx_geom *)This; D2D1_POINT_2F z = { 0, 0 }; UINT32 i; if (!g->open_figure) g->order_violations++;
    for (i = 0; i < n; i++) { gs_pt(g, b[i].point1); gs_pt(g, b[i].point2); gs_pt(g, b[i].point3); }
    g->nbeziers += n; gs_ev(g, DWX_G_BEZIERS, n, 0, z);
}
static void WINAPI gs_End(ID2D1SimplifiedGeometrySink *This, D2D1_FIGURE_END e) {
    dwx_geom *g = (dwx_geom *)This; D2D1_POINT_2F z = { 0, 0 }; if (!g->open_figure) g->order_violations++;
    g->open_figure = 0; g->nfigures_ended++; g->last_end = e; gs_ev(g, DWX_G_END, 0, e, z);
}
static HRESULT WINAPI gs_Close(ID2D1SimplifiedGeometrySink *This) {
    dwx_geom *g = (dwx_geom *)This; D2D1_POINT_2F z = { 0, 0 }; gs_ev(g, DWX_G_CLOSE, 0, 0, z); g->close_calls++;
    return g->fail_close ? g->fail_close : S_OK;
}
static const ID2D1SimplifiedGeometrySinkVtbl gs_vt = { { gs_QI, gs_AR, gs_RL }, gs_Fill, gs_Seg, gs_Begin, gs_Lines, gs_Bez, gs_End, gs_Close };
dwx_geom *dwx_geom_new(void) { dwx_geom *g = (dwx_geom *)dwx_alloc(sizeof *g, NULL); g->lpVtbl = &gs_vt; g->fill_mode = -1; return g; }
void dwx_geom_bounds(const dwx_geom *g, float *x0, float *y0, float *x1, float *y1) {
    int i; float a = 0, b = 0, c = 0, d = 0;
    for (i = 0; i < g->npts; i++) {
        float x = g->pts[i].x, y = g->pts[i].y;
        if (!i) { a = c = x; b = d = y; } else { if (x < a) a = x; if (x > c) c = x; if (y < b) b = y; if (y > d) d = y; }
    }
    *x0 = a; *y0 = b; *x1 = c; *y1 = d;
}

/* ===== text analysis source ============================================================================ */
static const IID *const ids_as[] = { &IID_IDWriteTextAnalysisSource };
static const IID *const ids_as1[] = { &IID_IDWriteTextAnalysisSource, &IID_IDWriteTextAnalysisSource1 };
static HRESULT WINAPI as_QI(IDWriteTextAnalysisSource1 *This, REFIID r, void **o) { dwx_ana_src *s = (dwx_ana_src *)This; return s->support_v1 ? dwx_qi(This, r, o, ids_as1, 2) : dwx_qi(This, r, o, ids_as, 1); }
static ULONG WINAPI as_AR(IDWriteTextAnalysisSource1 *This) { return dwx_addref(This); }
static ULONG WINAPI as_RL(IDWriteTextAnalysisSource1 *This) { return dwx_release(This); }
static void as_log(dwx_ana_src *s, int fn, UINT32 pos) { if (s->log_n < 1024) { s->log[s->log_n].fn = fn; s->log[s->log_n].pos = pos; s->log_n++; } s->last_pos = pos; }
static HRESULT WINAPI as_At(IDWriteTextAnalysisSource1 *This, UINT32 pos, const WCHAR **text, UINT32 *len) {
    dwx_ana_src *s = (dwx_ana_src *)This; s->n_at++; as_log(s, 1, pos);
    if (pos >= s->len) { *text = NULL; *len = 0; return S_OK; }
    *text = s->text + pos; *len = s->len - pos;
    if (s->chunk && *len > s->chunk) *len = s->chunk;
    return S_OK;
}
static HRESULT WINAPI as_Before(IDWriteTextAnalysisSource1 *This, UINT32 pos, const WCHAR **text, UINT32 *len) {
    dwx_ana_src *s = (dwx_ana_src *)This; s->n_before++; as_log(s, 2, pos);
    if (pos == 0 || pos > s->len) { *text = NULL; *len = 0; return S_OK; }
    *len = pos; if (s->chunk && *len > s->chunk) *len = s->chunk;
    *text = s->text + pos - *len;
    return S_OK;
}
static DWRITE_READING_DIRECTION WINAPI as_Dir(IDWriteTextAnalysisSource1 *This) { dwx_ana_src *s = (dwx_ana_src *)This; s->n_dir++; as_log(s, 3, 0); return s->dir; }
static HRESULT WINAPI as_Locale(IDWriteTextAnalysisSource1 *This, UINT32 pos, UINT32 *len, const WCHAR **loc) {
    dwx_ana_src *s = (dwx_ana_src *)This; s->n_locale++; as_log(s, 4, pos);
    if (s->fail_locale) return s->fail_locale;
    *loc = s->locale; *len = pos < s->len ? s->len - pos : 0;
    if (s->locale_run && *len > s->locale_run) *len = s->locale_run;
    return S_OK;
}
static HRESULT WINAPI as_Subst(IDWriteTextAnalysisSource1 *This, UINT32 pos, UINT32 *len, IDWriteNumberSubstitution **ns) {
    dwx_ana_src *s = (dwx_ana_src *)This; s->n_subst++; as_log(s, 5, pos);
    if (s->fail_subst) return s->fail_subst;
    *len = pos < s->len ? s->len - pos : 0;
    if (s->subst_run && *len > s->subst_run) *len = s->subst_run;
    *ns = s->subst; if (*ns) IDWriteNumberSubstitution_AddRef(*ns);
    return S_OK;
}
static HRESULT WINAPI as_Orient(IDWriteTextAnalysisSource1 *This, UINT32 pos, UINT32 *len, DWRITE_VERTICAL_GLYPH_ORIENTATION *o, UINT8 *bidi) {
    dwx_ana_src *s = (dwx_ana_src *)This; s->n_orient++; as_log(s, 6, pos);
    if (s->fail_orient) return s->fail_orient;
    *len = pos < s->len ? s->len - pos : 0;
    if (s->orient_run && *len > s->orient_run) *len = s->orient_run;
    *o = s->orientation; *bidi = s->orient_bidi; return S_OK;
}
static const IDWriteTextAnalysisSource1Vtbl as_vt = { as_QI, as_AR, as_RL, as_At, as_Before, as_Dir, as_Locale, as_Subst, as_Orient };
dwx_ana_src *dwx_ana_src_new(const WCHAR *text, UINT32 len) {
    dwx_ana_src *s = (dwx_ana_src *)dwx_alloc(sizeof *s, NULL);
    s->lpVtbl = &as_vt; s->text = text; s->len = len; lstrcpyW(s->locale, L"en-us"); s->dir = DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    s->orientation = DWRITE_VERTICAL_GLYPH_ORIENTATION_DEFAULT;
    return s;
}

/* ===== text analysis sink ==================================================================================== */
static const IID *const ids_sk[] = { &IID_IDWriteTextAnalysisSink };
static const IID *const ids_sk1[] = { &IID_IDWriteTextAnalysisSink, &IID_IDWriteTextAnalysisSink1 };
static HRESULT WINAPI sk_QI(IDWriteTextAnalysisSink1 *This, REFIID r, void **o) { dwx_ana_sink *s = (dwx_ana_sink *)This; return s->support_v1 ? dwx_qi(This, r, o, ids_sk1, 2) : dwx_qi(This, r, o, ids_sk, 1); }
static ULONG WINAPI sk_AR(IDWriteTextAnalysisSink1 *This) { return dwx_addref(This); }
static ULONG WINAPI sk_RL(IDWriteTextAnalysisSink1 *This) { return dwx_release(This); }
static HRESULT WINAPI sk_Script(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, const DWRITE_SCRIPT_ANALYSIS *a) {
    dwx_ana_sink *s = (dwx_ana_sink *)This; UINT32 i; s->n_script++;
    if (s->fail_script) return s->fail_script;
    if (s->nrun_script < 256) { s->script_runs[s->nrun_script].pos = pos; s->script_runs[s->nrun_script].len = len; }
    s->nrun_script++;
    for (i = pos; i < pos + len; i++) { if (i >= DWX_ANA_MAX) { s->overflow = 1; break; } if (s->script_set[i]) s->overlap = 1; s->script[i] = *a; s->script_set[i] = 1; }
    return S_OK;
}
static HRESULT WINAPI sk_Break(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, const DWRITE_LINE_BREAKPOINT *b) {
    dwx_ana_sink *s = (dwx_ana_sink *)This; UINT32 i; s->n_break++;
    if (s->fail_break) return s->fail_break;
    if (s->nrun_break < 256) { s->break_runs[s->nrun_break].pos = pos; s->break_runs[s->nrun_break].len = len; }
    s->nrun_break++;
    for (i = 0; i < len; i++) { if (pos + i >= DWX_ANA_MAX) { s->overflow = 1; break; } if (s->brk_set[pos + i]) s->overlap = 1; s->brk[pos + i] = b[i]; s->brk_set[pos + i] = 1; }
    return S_OK;
}
static HRESULT WINAPI sk_Bidi(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, UINT8 ex, UINT8 res) {
    dwx_ana_sink *s = (dwx_ana_sink *)This; UINT32 i; s->n_bidi++;
    if (s->fail_bidi) return s->fail_bidi;
    if (s->nrun_bidi < 256) { s->bidi_runs[s->nrun_bidi].pos = pos; s->bidi_runs[s->nrun_bidi].len = len; }
    s->nrun_bidi++;
    for (i = pos; i < pos + len; i++) { if (i >= DWX_ANA_MAX) { s->overflow = 1; break; } if (s->bidi_set[i]) s->overlap = 1; s->bidi_explicit[i] = ex; s->bidi_resolved[i] = res; s->bidi_set[i] = 1; }
    return S_OK;
}
static HRESULT WINAPI sk_Subst(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, IDWriteNumberSubstitution *ns) {
    dwx_ana_sink *s = (dwx_ana_sink *)This; UINT32 i; s->n_subst++;
    if (s->fail_subst) return s->fail_subst;
    for (i = pos; i < pos + len; i++) { if (i >= DWX_ANA_MAX) { s->overflow = 1; break; } s->subst[i] = ns; s->subst_set[i] = 1; }
    return S_OK;
}
static HRESULT WINAPI sk_Orient(IDWriteTextAnalysisSink1 *This, UINT32 pos, UINT32 len, DWRITE_GLYPH_ORIENTATION_ANGLE a, UINT8 bidi, BOOL sw, BOOL rtl) {
    dwx_ana_sink *s = (dwx_ana_sink *)This; UINT32 i; s->n_orient++;
    if (s->fail_orient) return s->fail_orient;
    for (i = pos; i < pos + len; i++) { if (i >= DWX_ANA_MAX) { s->overflow = 1; break; } s->angle[i] = a; s->orient_bidi[i] = bidi; s->orient_sideways[i] = sw; s->orient_rtl[i] = rtl; s->orient_set[i] = 1; }
    return S_OK;
}
static const IDWriteTextAnalysisSink1Vtbl sk_vt = { sk_QI, sk_AR, sk_RL, sk_Script, sk_Break, sk_Bidi, sk_Subst, sk_Orient };
dwx_ana_sink *dwx_ana_sink_new(void) { dwx_ana_sink *s = (dwx_ana_sink *)dwx_alloc(sizeof *s, NULL); s->lpVtbl = &sk_vt; return s; }
int dwx_ana_covered(const dwx_ana_sink *s, char fam, UINT32 len) {
    const unsigned char *set = fam == 's' ? s->script_set : fam == 'b' ? s->brk_set : s->bidi_set; UINT32 i;
    if (s->overlap) return 0;
    for (i = 0; i < len && i < DWX_ANA_MAX; i++) if (!set[i]) return 0;
    return 1;
}

/* ===== download listener ==================================================================================== */
static const IID *const ids_dl[] = { &IID_IDWriteFontDownloadListener };
UNK(dl, IDWriteFontDownloadListener, ids_dl, 1)
static void WINAPI dl_Done(IDWriteFontDownloadListener *This, IDWriteFontDownloadQueue *q, IUnknown *ctx, HRESULT hr) {
    dwx_listener *l = (dwx_listener *)This; l->last_queue = q; l->last_context = ctx; l->last_hr = hr; InterlockedIncrement(&l->n_completed);
    if (l->event) SetEvent(l->event);
}
static const IDWriteFontDownloadListenerVtbl dl_vt = { dl_QI, dl_AR, dl_RL, dl_Done };
static void dl_dtor(void *o) { dwx_listener *l = o; if (l->event) CloseHandle(l->event); }
dwx_listener *dwx_listener_new(void) { dwx_listener *l = (dwx_listener *)dwx_alloc(sizeof *l, dl_dtor); l->lpVtbl = &dl_vt; l->event = CreateEventA(NULL, FALSE, FALSE, NULL); return l; }

/* ===== memory DC ================================================================================================ */
BOOL dwx_dc_create(dwx_dc *d, int w, int h, uint32_t bg) {
    BITMAPINFO bi; HDC sdc;
    memset(d, 0, sizeof *d); d->w = w; d->h = h; d->bg = bg;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    sdc = GetDC(NULL);
    d->dc = CreateCompatibleDC(sdc);
    d->bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, (void **)&d->bits, NULL, 0);
    ReleaseDC(NULL, sdc);
    if (!d->dc || !d->bmp || !d->bits) { dwx_dc_destroy(d); return FALSE; }
    d->old = (HBITMAP)SelectObject(d->dc, d->bmp);
    dwx_dc_fill(d, bg);
    return TRUE;
}
void dwx_dc_destroy(dwx_dc *d) {
    if (d->dc && d->old) SelectObject(d->dc, d->old);
    if (d->bmp) DeleteObject(d->bmp);
    if (d->dc) DeleteDC(d->dc);
    memset(d, 0, sizeof *d);
}
void dwx_dc_fill(dwx_dc *d, uint32_t c) { int i; for (i = 0; i < d->w * d->h; i++) d->bits[i] = c; }
uint32_t dwx_dc_pixel(const dwx_dc *d, int x, int y) { return (x < 0 || y < 0 || x >= d->w || y >= d->h) ? 0 : (d->bits[y * d->w + x] & 0x00FFFFFF); }
int dwx_dc_count_not(const dwx_dc *d, uint32_t c) { int i, n = 0; for (i = 0; i < d->w * d->h; i++) if ((d->bits[i] & 0xFFFFFF) != (c & 0xFFFFFF)) n++; return n; }
BOOL dwx_dc_bbox_not(const dwx_dc *d, uint32_t c, RECT *rc) {
    int x, y; rc->left = d->w; rc->top = d->h; rc->right = rc->bottom = 0;
    for (y = 0; y < d->h; y++) for (x = 0; x < d->w; x++) if ((d->bits[y * d->w + x] & 0xFFFFFF) != (c & 0xFFFFFF)) {
        if (x < rc->left) rc->left = x; if (y < rc->top) rc->top = y; if (x + 1 > rc->right) rc->right = x + 1; if (y + 1 > rc->bottom) rc->bottom = y + 1;
    }
    return rc->right > rc->left;
}
uint64_t dwx_dc_hash(const dwx_dc *d) { uint64_t h = 1469598103934665603ull; int i; for (i = 0; i < d->w * d->h; i++) { h ^= d->bits[i] & 0xFFFFFF; h *= 1099511628211ull; } return h; }
