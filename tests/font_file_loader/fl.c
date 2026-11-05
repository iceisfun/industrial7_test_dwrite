#include "fl.h"

static CRITICAL_SECTION g_cs; static int g_cs_init;
static void lk(void) { if (!g_cs_init) { InitializeCriticalSection(&g_cs); g_cs_init = 1; } EnterCriticalSection(&g_cs); }
static void ul(void) { LeaveCriticalSection(&g_cs); }
static void logev(fl_loader *l, int kind, UINT64 a, UINT64 b, HRESULT hr) {
    LONG i = InterlockedIncrement(&l->nev) - 1;
    if (i < 1024) { l->ev[i].kind = kind; l->ev[i].a = a; l->ev[i].b = b; l->ev[i].hr = hr; }
}
static void poke(fl_loader *l) {
    if (l->mxcsr_poke) _mm_setcsr((_mm_getcsr() & ~0x6000u) | 0x6000u);   /* round toward zero */
}

/* ---- stream ---- */
static HRESULT WINAPI st_QI(IDWriteFontFileStream *This, REFIID r, void **o) {
    fl_stream *s = (fl_stream *)This; InterlockedIncrement(&s->qi);
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteFontFileStream)) { *o = This; InterlockedIncrement(&s->ref); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI st_AR(IDWriteFontFileStream *This) { fl_stream *s = (fl_stream *)This; InterlockedIncrement(&s->addref); return InterlockedIncrement(&s->ref); }
static ULONG WINAPI st_RL(IDWriteFontFileStream *This) {
    fl_stream *s = (fl_stream *)This; LONG r; InterlockedIncrement(&s->release);
    r = InterlockedDecrement(&s->ref); if (r == 0) s->destroyed = 1; return r;
}
static HRESULT WINAPI st_Read(IDWriteFontFileStream *This, const void **start, UINT64 off, UINT64 size, void **ctx) {
    fl_stream *s = (fl_stream *)This; fl_loader *l = s->ld; LONG idx = InterlockedIncrement(&l->n_read) - 1;
    if (start) *start = NULL; if (ctx) *ctx = NULL;
    logev(l, FL_READ, off, size, S_OK);
    poke(l);
    if (l->on_read) l->on_read(l, off, size);
    if (l->fail_read && idx >= l->fail_read_from) return l->fail_read;
    if (off > l->size || size > l->size - off) return l->oob_hr;
    {
        void *tok;
        if (l->copy_frag) {
            uint8_t *c = (uint8_t *)malloc((size_t)size + 8); memcpy(c + (l->misalign ? 1 : 0), l->data + off, (size_t)size);
            *start = c + (l->misalign ? 1 : 0); tok = c;
        } else { *start = l->data + off; tok = (void *)(ULONG_PTR)(0x100000 + idx); }
        lk(); if (l->nissued < 256) l->issued[l->nissued++] = tok; ul();
        *ctx = tok;
    }
    InterlockedIncrement(&l->outstanding);
    return S_OK;
}
static void WINAPI st_Rel(IDWriteFontFileStream *This, void *ctx) {
    fl_stream *s = (fl_stream *)This; fl_loader *l = s->ld; int i, found = 0;
    InterlockedIncrement(&l->n_rel);
    logev(l, FL_REL, (UINT64)(ULONG_PTR)ctx, 0, S_OK);
    lk();
    for (i = 0; i < l->nissued; i++) if (l->issued[i] == ctx) { l->issued[i] = l->issued[--l->nissued]; found = 1; break; }
    ul();
    if (!found) { l->bad_rel++; return; }
    if (l->copy_frag) free(ctx);
    InterlockedDecrement(&l->outstanding);
}
static HRESULT WINAPI st_Size(IDWriteFontFileStream *This, UINT64 *size) {
    fl_stream *s = (fl_stream *)This; fl_loader *l = s->ld;
    InterlockedIncrement(&l->n_size); logev(l, FL_SIZE, 0, 0, l->fail_size);
    poke(l);
    *size = 0;
    if (l->fail_size) return l->fail_size;
    *size = l->reported_size; return S_OK;
}
static HRESULT WINAPI st_LastW(IDWriteFontFileStream *This, UINT64 *t) {
    fl_stream *s = (fl_stream *)This; fl_loader *l = s->ld;
    InterlockedIncrement(&l->n_lastw); logev(l, FL_LASTW, 0, 0, l->lastw_hr);
    *t = 0; if (l->lastw_hr) return l->lastw_hr;
    *t = l->last_write; return S_OK;
}
static const IDWriteFontFileStreamVtbl st_vt = { st_QI, st_AR, st_RL, st_Read, st_Rel, st_Size, st_LastW };

/* ---- loader ---- */
static HRESULT WINAPI ld_QI(IDWriteFontFileLoader *This, REFIID r, void **o) {
    fl_loader *l = (fl_loader *)This; InterlockedIncrement(&l->qi);
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDWriteFontFileLoader)) { *o = This; InterlockedIncrement(&l->ref); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ld_AR(IDWriteFontFileLoader *This) { return InterlockedIncrement(&((fl_loader *)This)->ref); }
static ULONG WINAPI ld_RL(IDWriteFontFileLoader *This) {
    fl_loader *l = (fl_loader *)This; LONG r = InterlockedDecrement(&l->ref); if (r == 0) l->destroyed = 1; return r;
}
static HRESULT WINAPI ld_Create(IDWriteFontFileLoader *This, const void *key, UINT32 ks, IDWriteFontFileStream **out) {
    fl_loader *l = (fl_loader *)This; fl_stream *s;
    InterlockedIncrement(&l->n_create);
    logev(l, FL_CREATE, ks, (UINT64)(ULONG_PTR)key, l->fail_create);
    poke(l);
    *out = NULL;
    l->last_key_size = ks; l->last_key_ptr = key; memset(l->last_key, 0, sizeof l->last_key);
    if (key && ks) memcpy(l->last_key, key, ks < sizeof l->last_key ? ks : sizeof l->last_key);
    if (l->keys_log_n < 16) l->key_ptrs[l->keys_log_n++] = key;
    if (l->fail_create) return l->fail_create;
    s = (fl_stream *)calloc(1, sizeof *s); s->lpVtbl = &st_vt; s->ref = 1; s->ld = l;
    if (l->nstreams < 16) l->streams[l->nstreams++] = s;
    *out = (IDWriteFontFileStream *)s;
    return S_OK;
}
static const IDWriteFontFileLoaderVtbl ld_vt = { ld_QI, ld_AR, ld_RL, ld_Create };

fl_loader *fl_new(const void *data, size_t n) {
    fl_loader *l = (fl_loader *)calloc(1, sizeof *l);
    l->lpVtbl = &ld_vt; l->ref = 1; l->oob_hr = E_FAIL; l->last_write = 0x01D0000000000000ull;
    fl_set_data(l, data, n);
    return l;
}
void fl_set_data(fl_loader *l, const void *data, size_t n) {
    l->data = (uint8_t *)malloc(n ? n : 1); if (data) memcpy(l->data, data, n); l->size = n; l->reported_size = n;
}
void fl_free(fl_loader *l) { int i; for (i = 0; i < l->nstreams; i++) free(l->streams[i]); free(l->data); free(l); }
HRESULT fl_register(fl_loader *l, IDWriteFactory *f) {
    HRESULT hr = IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)l);
    if (SUCCEEDED(hr)) { l->registered = 1; l->reg_factory = f; }
    return hr;
}
HRESULT fl_unregister(fl_loader *l) {
    HRESULT hr = S_OK;
    if (l->registered) { hr = IDWriteFactory_UnregisterFontFileLoader(l->reg_factory, (IDWriteFontFileLoader *)l); l->registered = 0; }
    return hr;
}
HRESULT fl_file(fl_loader *l, IDWriteFactory *f, const void *key, UINT32 ks, IDWriteFontFile **out) {
    return IDWriteFactory_CreateCustomFontFileReference(f, key, ks, (IDWriteFontFileLoader *)l, out);
}
HRESULT fl_file4(fl_loader *l, IDWriteFactory *f, UINT32 key, IDWriteFontFile **out) { return fl_file(l, f, &key, 4, out); }
ULONG fl_addref(fl_loader *l) { return ld_AR((IDWriteFontFileLoader *)l); }
ULONG fl_release(fl_loader *l) { return ld_RL((IDWriteFontFileLoader *)l); }
LONG fl_refs(fl_loader *l) { return l->ref; }
LONG fl_stream_refs(fl_loader *l, int i) { return i < l->nstreams ? l->streams[i]->ref : -1; }
int fl_nev(fl_loader *l, int kind) { int i, c = 0, n = l->nev < 1024 ? (int)l->nev : 1024; for (i = 0; i < n; i++) if (l->ev[i].kind == kind) c++; return c; }
void fl_clear(fl_loader *l) {
    l->nev = 0; l->n_create = l->n_size = l->n_read = l->n_rel = l->n_lastw = 0; l->keys_log_n = 0;
}
int fl_log_is_(fl_loader *l, const fl_ev *w, int n, const char *file, int line) {
    static const char *nm[] = { "?", "CREATE", "SIZE", "READ", "REL", "LASTW" };
    int i, ok = (l->nev == n);
    for (i = 0; ok && i < n; i++) {
        const fl_ev *g = &l->ev[i];
        if (g->kind != w[i].kind) ok = 0;
        else if (w[i].kind != FL_REL && w[i].kind != FL_CREATE) {
            if (w[i].a != ANY && g->a != w[i].a) ok = 0;
            if (w[i].b != ANY && g->b != w[i].b) ok = 0;
        } else if (w[i].kind == FL_CREATE && w[i].a != ANY && g->a != w[i].a) ok = 0;
    }
    if (!ok) {
        char buf[600]; int p = 0, m = l->nev < 24 ? (int)l->nev : 24;
        for (i = 0; i < m && p < 540; i++) p += snprintf(buf + p, sizeof buf - p, "%s(%llx,%llx) ", nm[l->ev[i].kind], (unsigned long long)l->ev[i].a, (unsigned long long)l->ev[i].b);
        t_check(0, file, line, "callback log matches", "got %ld events: %s", (long)l->nev, buf);
    } else t_check(1, file, line, "callback log matches", NULL);
    return ok;
}

/* ---- fixtures ---- */
uint8_t *fx_ttf(size_t *n) { return ttf_build_default(n); }
uint8_t *fx_magic(uint32_t m, size_t total, size_t *n) {
    uint8_t *b = (uint8_t *)calloc(1, total); b[0] = m >> 24; b[1] = m >> 16; b[2] = m >> 8; b[3] = m; *n = total; return b;
}
uint8_t *fx_ttc_hdr(uint32_t c, size_t total, size_t *n) {
    uint8_t *b = fx_magic(0x74746366, total, n);   /* 'ttcf' */
    b[4] = 0; b[5] = 1; b[6] = 0; b[7] = 0; b[8] = c >> 24; b[9] = c >> 16; b[10] = c >> 8; b[11] = c; return b;
}
uint8_t *fx_pfb(size_t total, size_t *n) {
    uint8_t *b = (uint8_t *)calloc(1, total); uint32_t len = (uint32_t)(total - 6);
    b[0] = 0x80; b[1] = 1; memcpy(b + 2, &len, 4); memset(b + 6, 'x', total - 6); *n = total; return b;
}
uint8_t *fx_pfm(size_t total, size_t *n) {
    uint8_t *b = (uint8_t *)calloc(1, total); uint32_t sz = (uint32_t)total;
    b[0] = 0; b[1] = 1; memcpy(b + 2, &sz, 4); b[0x42] = 0x81; b[0x43] = 0; b[0x44] = 10; b[0x45] = 0; *n = total; return b;
}

IDWriteFactory *fl_factory(void) { return dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); }
IDWriteFactory5 *fl_factory5(IDWriteFactory *f) {
    void *q = NULL;
    if (SUCCEEDED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory5, &q))) return (IDWriteFactory5 *)q;
    return NULL;
}
int rig_open(rig *r, const void *blob, size_t n) {
    memset(r, 0, sizeof *r);
    r->f = fl_factory(); if (!r->f) { t_skip("DWriteCreateFactory failed"); return 0; }
    r->ld = fl_new(blob, n);
    r->hr = fl_register(r->ld, r->f);
    CHECK_HR(r->hr, S_OK);
    if (FAILED(r->hr)) return 0;
    r->hr = fl_file4(r->ld, r->f, 0, &r->file);
    CHECK_HR(r->hr, S_OK);
    return SUCCEEDED(r->hr) && r->file != NULL;
}
void rig_close(rig *r) {
    DW_RELEASE(r->file);
    if (r->ld) { fl_unregister(r->ld); fl_release(r->ld); }
    DW_RELEASE(r->f);
    /* memory of the loader is kept until process exit: dwrite must not touch it after this point anyway */
}
an_res fl_analyze(IDWriteFontFile *file) {
    an_res a; a.sup = (BOOL)0x55; a.ft = (DWRITE_FONT_FILE_TYPE)0x55; a.fc = (DWRITE_FONT_FACE_TYPE)0x55; a.nf = 0x55555555;
    a.hr = IDWriteFontFile_Analyze(file, &a.sup, &a.ft, &a.fc, &a.nf);
    return a;
}
int fl_sysfont(const char *leaf, char *out, size_t cap) {
    const char *c[2] = { leaf, NULL };
    return dw_find_system_font(c, out, cap);
}
int fl_write_tmp(const char *leaf, const void *data, size_t n, char *path, size_t cap) {
    HANDLE h; DWORD w = 0;
    t_tmpfile(path, cap, leaf);
    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    WriteFile(h, data, (DWORD)n, &w, NULL); CloseHandle(h);
    return w == n;
}
