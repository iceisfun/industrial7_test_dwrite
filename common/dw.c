/* DirectWrite helper layer; see dw.h. */
#define INITGUID
#include "dw.h"

const IID DW_IID_IDWriteBitmapRenderTarget2 = { 0xc553a742, 0xfc01, 0x44da, { 0xa6, 0x6e, 0xb8, 0xb9, 0xed, 0x6c, 0x39, 0x95 } };
const IID DW_IID_IDWriteBitmapRenderTarget3 = { 0xaeec37db, 0xc337, 0x40f1, { 0x8e, 0x2a, 0x9a, 0x41, 0xb1, 0x67, 0xb2, 0x38 } };
const IID DW_IID_IDWriteFactory8            = { 0xee0a7fb5, 0xdef4, 0x4c23, { 0xa4, 0x54, 0xc9, 0xc7, 0xdc, 0x87, 0x83, 0x98 } };
const IID DW_IID_IDWriteFontFace7           = { 0x3945b85b, 0xbc95, 0x40f7, { 0xb7, 0x2c, 0x8b, 0x73, 0xbf, 0xc7, 0xe1, 0x3b } };
const IID DW_IID_IDWritePaintReader         = { 0x8128e912, 0x3b97, 0x42a5, { 0xab, 0x6c, 0x24, 0xaa, 0xd3, 0xa8, 0x6e, 0x54 } };

/* ---- factory ------------------------------------------------------------------------------- */
dw_create_fn dw_proc(void) { return (dw_create_fn)U(DWriteCreateFactory); }
HRESULT dw_create_factory(DWRITE_FACTORY_TYPE type, REFIID riid, void **out) {
    dw_create_fn fn = dw_proc();
    *out = NULL;
    if (!fn) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
    return fn(type, riid, (IUnknown **)out);
}
const IID *dw_factory_iid(int ver) {
    static const IID *t[8];
    if (!t[0]) {
        t[0] = &IID_IDWriteFactory;  t[1] = &IID_IDWriteFactory1; t[2] = &IID_IDWriteFactory2; t[3] = &IID_IDWriteFactory3;
        t[4] = &IID_IDWriteFactory4; t[5] = &IID_IDWriteFactory5; t[6] = &IID_IDWriteFactory6; t[7] = &IID_IDWriteFactory7;
    }
    return (ver >= 0 && ver < 8) ? t[ver] : NULL;
}
void *dw_factory_v(DWRITE_FACTORY_TYPE type, int ver) {
    static void *cache[2][8]; static int tried[2][8];
    if (ver < 0 || ver > 7 || (int)type < 0 || (int)type > 1) return NULL;
    if (!tried[type][ver]) {
        tried[type][ver] = 1;
        if (ver == 0) dw_create_factory(type, &IID_IDWriteFactory, &cache[type][0]);
        else {
            void *base = dw_factory_v(type, 0);
            if (base) { void *q = NULL; if (SUCCEEDED(IUnknown_QueryInterface((IUnknown *)base, dw_factory_iid(ver), &q))) cache[type][ver] = q; }
        }
    }
    return cache[type][ver];
}
IDWriteFactory *dw_shared(void)   { return (IDWriteFactory *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 0); }
IDWriteFactory *dw_isolated(void) { return (IDWriteFactory *)dw_factory_v(DWRITE_FACTORY_TYPE_ISOLATED, 0); }
IDWriteFactory *dw_new_factory(DWRITE_FACTORY_TYPE type) { void *f = NULL; return SUCCEEDED(dw_create_factory(type, &IID_IDWriteFactory, &f)) ? (IDWriteFactory *)f : NULL; }

/* ---- HRESULT names --------------------------------------------------------------------------- */
static const struct { HRESULT hr; const char *n; } g_hr[] = {
    { S_OK, "S_OK" }, { S_FALSE, "S_FALSE" }, { E_FAIL, "E_FAIL" }, { E_INVALIDARG, "E_INVALIDARG" }, { E_NOINTERFACE, "E_NOINTERFACE" },
    { E_NOTIMPL, "E_NOTIMPL" }, { E_OUTOFMEMORY, "E_OUTOFMEMORY" }, { E_POINTER, "E_POINTER" }, { E_UNEXPECTED, "E_UNEXPECTED" },
    { E_ABORT, "E_ABORT" }, { E_ACCESSDENIED, "E_ACCESSDENIED" }, { E_HANDLE, "E_HANDLE" }, { (HRESULT)0x8007007A, "E_NOT_SUFFICIENT_BUFFER" },
    { (HRESULT)0x80070002, "HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)" }, { (HRESULT)0x80070003, "HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)" },
    { (HRESULT)0x80070005, "HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)" }, { (HRESULT)0x8007000D, "HRESULT_FROM_WIN32(ERROR_INVALID_DATA)" },
    { (HRESULT)0x8007007F, "HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND)" }, { (HRESULT)0x800700EA, "HRESULT_FROM_WIN32(ERROR_MORE_DATA)" },
    { (HRESULT)0x80070216, "HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW)" },
    { (HRESULT)0x88985000, "DWRITE_E_FILEFORMAT" }, { (HRESULT)0x88985001, "DWRITE_E_UNEXPECTED" }, { (HRESULT)0x88985002, "DWRITE_E_NOFONT" },
    { (HRESULT)0x88985003, "DWRITE_E_FILENOTFOUND" }, { (HRESULT)0x88985004, "DWRITE_E_FILEACCESS" },
    { (HRESULT)0x88985005, "DWRITE_E_FONTCOLLECTIONOBSOLETE" }, { (HRESULT)0x88985006, "DWRITE_E_ALREADYREGISTERED" },
    { (HRESULT)0x88985007, "DWRITE_E_CACHEFORMAT" }, { (HRESULT)0x88985008, "DWRITE_E_CACHEVERSION" },
    { (HRESULT)0x88985009, "DWRITE_E_UNSUPPORTEDOPERATION" }, { (HRESULT)0x8898500A, "DWRITE_E_TEXTRENDERERINCOMPATIBLE" },
    { (HRESULT)0x8898500B, "DWRITE_E_FLOWDIRECTIONCONFLICTS" }, { (HRESULT)0x8898500C, "DWRITE_E_NOCOLOR" },
    { (HRESULT)0x8898500D, "DWRITE_E_REMOTEFONT" }, { (HRESULT)0x8898500E, "DWRITE_E_DOWNLOADCANCELLED" },
    { (HRESULT)0x8898500F, "DWRITE_E_DOWNLOADFAILED" }, { (HRESULT)0x88985010, "DWRITE_E_TOOMANYDOWNLOADS" },
};
const char *dw_hr_name(HRESULT hr) {
    static char buf[4][40]; static int k;
    size_t i;
    for (i = 0; i < sizeof g_hr / sizeof g_hr[0]; i++) if (g_hr[i].n && g_hr[i].hr == hr) return g_hr[i].n;
    k = (k + 1) & 3;
    snprintf(buf[k], sizeof buf[k], "0x%08lx", (unsigned long)hr);
    return buf[k];
}
HRESULT dw_check_hr(HRESULT got, HRESULT want, const char *file, int line, const char *expr) {
    char g[64];
    snprintf(g, sizeof g, "%s", dw_hr_name(got));
    t_check(got == want, file, line, expr, "got %s (0x%08lx) want %s (0x%08lx)", g, (unsigned long)got, dw_hr_name(want), (unsigned long)want);
    return got;
}

/* ---- refcounts / QI ----------------------------------------------------------------------------- */
ULONG dw_refcount(IUnknown *u) { if (!u) return 0; IUnknown_AddRef(u); return IUnknown_Release(u); }
BOOL dw_same_identity(IUnknown *a, IUnknown *b) {
    IUnknown *ia = NULL, *ib = NULL; BOOL same;
    if (!a || !b) return FALSE;
    if (FAILED(IUnknown_QueryInterface(a, &IID_IUnknown, (void **)&ia))) return FALSE;
    if (FAILED(IUnknown_QueryInterface(b, &IID_IUnknown, (void **)&ib))) { IUnknown_Release(ia); return FALSE; }
    same = ia == ib;
    IUnknown_Release(ia); IUnknown_Release(ib);
    return same;
}
void dw_qi_check_(IUnknown *obj, const dw_qi_row *rows, int n, const char *file, int line) {
    IUnknown *self = NULL, *self2 = NULL; ULONG base;
    int i;
    t_check(SUCCEEDED(IUnknown_QueryInterface(obj, &IID_IUnknown, (void **)&self)), file, line, "QI(IUnknown)", NULL);
    if (!self) return;
    IUnknown_QueryInterface(obj, &IID_IUnknown, (void **)&self2);
    t_check(self == self2, file, line, "QI(IUnknown) stable identity", "%p vs %p", (void *)self, (void *)self2);
    if (self2) IUnknown_Release(self2);
    IUnknown_Release(self);
    base = dw_refcount(obj);
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)1;     /* sentinel: must be overwritten */
        HRESULT hr = IUnknown_QueryInterface(obj, rows[i].iid, &p);
        if (SUCCEEDED(hr)) {
            if (rows[i].expect == 0) t_check(0, file, line, rows[i].name, "QI unexpectedly succeeded");
            else if (rows[i].expect == 1) t_check(hr == S_OK, file, line, rows[i].name, "hr=%s", dw_hr_name(hr));
            else t_note("QI %s -> %s", rows[i].name, dw_hr_name(hr));
            t_check(p != NULL && p != (void *)1, file, line, rows[i].name, "success with bad out pointer");
            if (p && p != (void *)1) {
                t_check(dw_same_identity((IUnknown *)p, obj), file, line, rows[i].name, "QI result has a different COM identity");
                IUnknown_Release((IUnknown *)p);
            }
        } else {
            if (rows[i].expect == 1) t_check(0, file, line, rows[i].name, "QI failed: %s", dw_hr_name(hr));
            else if (rows[i].expect == 0) {
                t_check(hr == E_NOINTERFACE, file, line, rows[i].name, "QI failure code %s want E_NOINTERFACE", dw_hr_name(hr));
                t_check(p == NULL, file, line, rows[i].name, "out pointer not zeroed on failure (%p)", p);
            } else t_note("QI %s -> %s", rows[i].name, dw_hr_name(hr));
        }
    }
    t_check(dw_refcount(obj) == base, file, line, "QI matrix leaves refcount balanced", "before %lu after %lu", base, dw_refcount(obj));
}
const dw_qi_row *dw_iid_table(int *n) {
    static dw_qi_row rows[128]; static int cnt;
    if (!cnt) {
#define DWIID(x) rows[cnt].iid = &IID_##x; rows[cnt].name = #x; rows[cnt].expect = -1; cnt++;
#include "dw_iids.inc"
#undef DWIID
        rows[cnt].iid = &DW_IID_IDWriteBitmapRenderTarget2; rows[cnt].name = "IDWriteBitmapRenderTarget2"; rows[cnt++].expect = -1;
        rows[cnt].iid = &DW_IID_IDWriteBitmapRenderTarget3; rows[cnt].name = "IDWriteBitmapRenderTarget3"; rows[cnt++].expect = -1;
        rows[cnt].iid = &DW_IID_IDWriteFactory8;            rows[cnt].name = "IDWriteFactory8";            rows[cnt++].expect = -1;
        rows[cnt].iid = &DW_IID_IDWriteFontFace7;           rows[cnt].name = "IDWriteFontFace7";           rows[cnt++].expect = -1;
        rows[cnt].iid = &DW_IID_IDWritePaintReader;         rows[cnt].name = "IDWritePaintReader";         rows[cnt++].expect = -1;
    }
    if (n) *n = cnt;
    return rows;
}

/* ---- strings ----------------------------------------------------------------------------------- */
int dw_wide(const char *s, WCHAR *out, int cap) { return MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cap); }
int dw_narrow(const WCHAR *w, char *out, int cap) { return WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL); }
HRESULT dw_ls_find(IDWriteLocalizedStrings *s, const WCHAR *locale, UINT32 *index, BOOL *exists) { return IDWriteLocalizedStrings_FindLocaleName(s, locale, index, exists); }
HRESULT dw_ls_get(IDWriteLocalizedStrings *s, UINT32 idx, char *buf, int cap) {
    UINT32 len = 0; WCHAR *w; HRESULT hr;
    hr = IDWriteLocalizedStrings_GetStringLength(s, idx, &len);
    if (FAILED(hr)) { if (cap) buf[0] = 0; return hr; }
    w = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
    hr = IDWriteLocalizedStrings_GetString(s, idx, w, len + 1);
    if (SUCCEEDED(hr)) dw_narrow(w, buf, cap); else if (cap) buf[0] = 0;
    free(w);
    return hr;
}
HRESULT dw_ls_en(IDWriteLocalizedStrings *s, char *buf, int cap) {
    UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteLocalizedStrings_FindLocaleName(s, L"en-us", &idx, &ex)) || !ex) idx = 0;
    return dw_ls_get(s, idx, buf, cap);
}

/* ---- system fonts ------------------------------------------------------------------------------- */
void dw_fonts_dir(char *out, size_t cap) {
    char w[MAX_PATH];
    if (!GetWindowsDirectoryA(w, sizeof w)) strcpy(w, "C:\\Windows");
    snprintf(out, cap, "%s\\Fonts\\", w);
}
int dw_find_system_font(const char *const *cand, char *out, size_t cap) {
    char dir[MAX_PATH]; int i;
    dw_fonts_dir(dir, sizeof dir);
    for (i = 0; cand[i]; i++) {
        snprintf(out, cap, "%s%s", dir, cand[i]);
        if (GetFileAttributesA(out) != INVALID_FILE_ATTRIBUTES) return 1;
    }
    if (cap) out[0] = 0;
    return 0;
}
BOOL dw_have_font(IDWriteFactory *f, const WCHAR *family) {
    IDWriteFontCollection *c = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return FALSE;
    IDWriteFontCollection_FindFamilyName(c, family, &idx, &ex);
    IDWriteFontCollection_Release(c);
    return ex;
}
IDWriteFontFace *dw_sysface(IDWriteFactory *f, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STRETCH s, DWRITE_FONT_STYLE st) {
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
    UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return NULL;
    IDWriteFontCollection_FindFamilyName(c, family, &idx, &ex);
    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam) {
        if (SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, w, s, st, &font)) && font) {
            IDWriteFont_CreateFontFace(font, &face);
            IDWriteFont_Release(font);
        }
        IDWriteFontFamily_Release(fam);
    }
    IDWriteFontCollection_Release(c);
    return face;
}
uint8_t *dw_face_table(IDWriteFontFace *face, UINT32 tag, UINT32 *len) {
    const void *d = NULL; UINT32 sz = 0; void *ctx = NULL; BOOL ex = FALSE; uint8_t *copy = NULL;
    if (len) *len = 0;
    if (FAILED(IDWriteFontFace_TryGetFontTable(face, tag, &d, &sz, &ctx, &ex)) || !ex) return NULL;
    copy = (uint8_t *)malloc(sz + 1);
    if (copy) { memcpy(copy, d, sz); if (len) *len = sz; }
    IDWriteFontFace_ReleaseFontTable(face, ctx);
    return copy;
}
