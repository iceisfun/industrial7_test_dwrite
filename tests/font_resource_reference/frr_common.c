#include "frr.h"

/* ---- fixture ------------------------------------------------------------------------------------------------------- */
int frr_open(frr_fix *x, const void *blob, size_t n) {
    HRESULT hr;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("cannot create an isolated factory"); return 0; }
    if (FAILED(IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory3, (void **)&x->f3)) || !x->f3) { x->f3 = NULL; t_skip("IDWriteFactory3 unsupported"); return 0; }
    IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory5, (void **)&x->f5);
    IDWriteFactory_QueryInterface(x->f, dw_factory_iid(6), (void **)&x->f6);
    x->ld = dwx_loader_new();
    x->key = dwx_loader_add(x->ld, blob, n);
    hr = dwx_loader_register(x->ld, x->f);
    if (FAILED(hr)) { t_skip("RegisterFontFileLoader: %s", dw_hr_name(hr)); return 0; }
    hr = dwx_loader_file(x->ld, x->f, x->key, &x->file);
    if (FAILED(hr) || !x->file) { t_skip("CreateCustomFontFileReference: %s", dw_hr_name(hr)); return 0; }
    return 1;
}
int frr_open_default(frr_fix *x) {
    size_t n; uint8_t *b = ttf_build_default(&n); int ok;
    if (!b) { memset(x, 0, sizeof *x); t_skip("ttf_build_default failed"); return 0; }
    ok = frr_open(x, b, n);
    free(b);
    return ok;
}
void frr_close(frr_fix *x) {
    DW_RELEASE(x->file);
    DW_RELEASE(x->f6); DW_RELEASE(x->f5); DW_RELEASE(x->f3);
    if (x->ld) { dwx_loader_unregister(x->ld); dwx_release(x->ld); }
    DW_RELEASE(x->f);
    memset(x, 0, sizeof *x);
}
IDWriteFontFile *frr_file(frr_fix *x) {
    IDWriteFontFile *f = NULL;
    HRESULT hr = dwx_loader_file(x->ld, x->f, x->key, &f);
    CHECK(SUCCEEDED(hr) && f);
    return f;
}
IDWriteFontFaceReference *frr_ref(frr_fix *x, UINT32 index, DWRITE_FONT_SIMULATIONS sim) {
    IDWriteFontFaceReference *r = NULL;
    HRESULT hr = IDWriteFactory3_CreateFontFaceReference_(x->f3, x->file, index, sim, &r);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? r : NULL;
}
IDWriteFontFaceReference1 *frr_ref1(IDWriteFontFaceReference *r) {
    IDWriteFontFaceReference1 *r1 = NULL;
    if (FAILED(IDWriteFontFaceReference_QueryInterface(r, &IID_IDWriteFontFaceReference1, (void **)&r1))) return NULL;
    return r1;
}

/* ---- synthetic variable font ----------------------------------------------------------------------------------------- */
static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
uint8_t *frr_build_var(const frr_axis *ax, int n, size_t *size) {
    ttf_spec s; ttf_rawtable raw[2]; uint8_t *fv, *gv; unsigned gv_len; unsigned len = 16 + 20 * (unsigned)n; int i; uint8_t *blob;
    ttf_style_spec(&s, "FrrVar", 400, 5, 0);
    fv = (uint8_t *)calloc(1, len ? len : 1);
    put16(fv, 1); put16(fv + 2, 0); put16(fv + 4, 16); put16(fv + 6, 2); put16(fv + 8, (unsigned)n); put16(fv + 10, 20);
    put16(fv + 12, 0); put16(fv + 14, 4 + 4 * (unsigned)n);          /* no named instances */
    for (i = 0; i < n; i++) {
        uint8_t *a = fv + 16 + 20 * i;
        memcpy(a, ax[i].tag, 4);
        put32(a + 4, (uint32_t)FRR_FIXED(ax[i].mn)); put32(a + 8, (uint32_t)FRR_FIXED(ax[i].df)); put32(a + 12, (uint32_t)FRR_FIXED(ax[i].mx));
        put16(a + 16, ax[i].flags); put16(a + 18, ax[i].nameid);
        if (ax[i].name && ax[i].nameid) ttf_add_name(&s, 3, 1, 0x409, ax[i].nameid, ax[i].name);
    }
    memset(raw, 0, sizeof raw); memcpy(raw[0].tag, "fvar", 5); raw[0].data = fv; raw[0].len = len;
    /* A well-formed 'gvar' without any glyph variation data (every glyph has an empty variation record) so the font is a complete,
     * if flat, variable font. */
    gv_len = 20 + 2 * (unsigned)(TTF_DEF_NGLYPHS + 1);
    gv = (uint8_t *)calloc(1, gv_len);
    put16(gv, 1); put16(gv + 2, 0); put16(gv + 4, (unsigned)n); put16(gv + 6, 0);
    put32(gv + 8, gv_len); put16(gv + 12, TTF_DEF_NGLYPHS); put16(gv + 14, 0); put32(gv + 16, gv_len);
    memcpy(raw[1].tag, "gvar", 5); raw[1].data = gv; raw[1].len = gv_len;
    s.raw = raw; s.nraw = 2;
    blob = ttf_build(&s, size);
    free(fv); free(gv);
    return blob;
}
uint8_t *frr_build_var2(size_t *size) {
    frr_axis ax[2] = { { "wght", 100, 400, 900, 0, 256, "Weight" }, { "XTRA", 10, 50, 90, 0, 257, "Extra Axis" } };
    return frr_build_var(ax, 2, size);
}

/* ---- remote-flavoured loader ------------------------------------------------------------------------------------------- */
static const IID *const ids_remote[] = { &IID_IDWriteFontFileLoader, &IID_IDWriteRemoteFontFileLoader };
static HRESULT WINAPI rm_QI(IDWriteRemoteFontFileLoader *This, REFIID r, void **o) {
    frr_remote *l = (frr_remote *)This; int i;
    InterlockedIncrement(&l->b.qi_calls);
    *o = NULL;
    if (IsEqualIID(r, &IID_IUnknown)) { *o = This; dwx_addref(This); return S_OK; }
    for (i = 0; i < 2; i++) if (IsEqualIID(r, ids_remote[i])) { *o = This; dwx_addref(This); return S_OK; }
    return E_NOINTERFACE;
}
static ULONG WINAPI rm_AR(IDWriteRemoteFontFileLoader *This) { return dwx_addref(This); }
static ULONG WINAPI rm_RL(IDWriteRemoteFontFileLoader *This) { return dwx_release(This); }
static HRESULT WINAPI rm_CreateStream(IDWriteRemoteFontFileLoader *This, const void *key, UINT32 ks, IDWriteFontFileStream **s) {
    frr_remote *l = (frr_remote *)This;
    InterlockedIncrement(&l->n_create);
    return IDWriteFontFileLoader_CreateStreamFromKey((IDWriteFontFileLoader *)l->inner, key, ks, s);
}
static HRESULT WINAPI rm_CreateRemoteStream(IDWriteRemoteFontFileLoader *This, const void *key, UINT32 ks, IDWriteRemoteFontFileStream **s) {
    frr_remote *l = (frr_remote *)This;
    InterlockedIncrement(&l->n_remote_create);
    if (s) *s = NULL;
    return E_NOTIMPL;
}
static HRESULT WINAPI rm_Locality(IDWriteRemoteFontFileLoader *This, const void *key, UINT32 ks, DWRITE_LOCALITY *loc) {
    frr_remote *l = (frr_remote *)This;
    InterlockedIncrement(&l->n_locality);
    l->last_key_size = ks; memset(l->last_key, 0, sizeof l->last_key);
    if (key && ks <= sizeof l->last_key) memcpy(l->last_key, key, ks);
    if (l->locality_hr) return l->locality_hr;
    *loc = l->locality;
    return S_OK;
}
static HRESULT WINAPI rm_Url(IDWriteRemoteFontFileLoader *This, IDWriteFactory *f, const WCHAR *b, const WCHAR *u, IDWriteFontFile **out) {
    frr_remote *l = (frr_remote *)This;
    InterlockedIncrement(&l->n_url);
    if (out) *out = NULL;
    return E_NOTIMPL;
}
static const IDWriteRemoteFontFileLoaderVtbl rm_vt = { rm_QI, rm_AR, rm_RL, rm_CreateStream, rm_CreateRemoteStream, rm_Locality, rm_Url };
frr_remote *frr_remote_new(dwx_mem_loader *inner, DWRITE_LOCALITY loc) {
    frr_remote *r = (frr_remote *)calloc(1, sizeof *r);
    r->lpVtbl = &rm_vt; r->b.ref = 1; r->inner = inner; dwx_addref(inner); r->locality = loc;
    return r;
}
HRESULT frr_remote_register(frr_remote *r, IDWriteFactory *f) {
    HRESULT hr = IDWriteFactory_RegisterFontFileLoader(f, (IDWriteFontFileLoader *)r);
    if (SUCCEEDED(hr)) { r->registered = 1; r->reg_factory = f; }
    return hr;
}
void frr_remote_unregister(frr_remote *r) {
    if (r->registered) { IDWriteFactory_UnregisterFontFileLoader(r->reg_factory, (IDWriteFontFileLoader *)r); r->registered = 0; }
}

/* ---- system fonts ------------------------------------------------------------------------------------------------------ */
int frr_sys_path(const char *leaf, WCHAR *out, int cap) {
    const char *c[2]; char full[MAX_PATH];
    c[0] = leaf; c[1] = NULL;
    if (!dw_find_system_font(c, full, sizeof full)) return 0;
    return dw_wide(full, out, cap) > 0;
}
int frr_disk_info(const WCHAR *path, UINT64 *size, FILETIME *ft) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &d)) return 0;
    if (size) *size = ((UINT64)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    if (ft) *ft = d.ftLastWriteTime;
    return 1;
}
IDWriteFontFaceReference *frr_sys_ref(IDWriteFactory3 *f3, const char *leaf, UINT32 index, DWRITE_FONT_SIMULATIONS sim) {
    WCHAR p[MAX_PATH]; IDWriteFontFaceReference *r = NULL; HRESULT hr;
    if (!frr_sys_path(leaf, p, MAX_PATH)) { t_skip("system font %s not installed", leaf); return NULL; }
    hr = IDWriteFactory3_CreateFontFaceReference(f3, p, NULL, index, sim, &r);
    CHECK_HR(hr, S_OK);
    return SUCCEEDED(hr) ? r : NULL;
}
IDWriteFactory3 *frr_shared3(void) { return (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); }
int frr_face_equals(IUnknown *a, IUnknown *b) {
    IDWriteFontFace5 *f5 = NULL; IDWriteFontFace *fb = NULL; int r = -1;
    if (SUCCEEDED(IUnknown_QueryInterface(a, &IID_IDWriteFontFace5, (void **)&f5)) && f5 && SUCCEEDED(IUnknown_QueryInterface(b, &IID_IDWriteFontFace, (void **)&fb)) && fb)
        r = IDWriteFontFace5_Equals(f5, fb) ? 1 : 0;
    DW_RELEASE(fb); DW_RELEASE(f5);
    return r;
}
