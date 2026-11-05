#include "lsfx.h"

/* ---- name table builder ----------------------------------------------------------------------------- */
static int cmp_rec(const void *a, const void *b) {
    const ls_rec *x = *(const ls_rec *const *)a, *y = *(const ls_rec *const *)b;
    if (x->plat != y->plat) return x->plat - y->plat;
    if (x->enc != y->enc) return x->enc - y->enc;
    if (x->lang != y->lang) return x->lang - y->lang;
    return x->id - y->id;
}
static void w16(uint8_t *p, unsigned v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

uint8_t *ls_name_table(const ls_rec *r, int n, size_t *len) {
    const ls_rec **ord = (const ls_rec **)malloc((n + 1) * sizeof *ord);
    size_t total = 6 + 12 * (size_t)n, off = 0, i;
    uint8_t *t;
    for (i = 0; i < (size_t)n; i++) { ord[i] = &r[i]; total += r[i].len; }
    qsort(ord, n, sizeof *ord, cmp_rec);        /* stable enough: equal keys keep any order */
    t = (uint8_t *)calloc(1, total + 4);
    w16(t, 0); w16(t + 2, n); w16(t + 4, 6 + 12 * n);
    for (i = 0; i < (size_t)n; i++) {
        const ls_rec *q = ord[i]; uint8_t *rec = t + 6 + 12 * i, *dst = t + 6 + 12 * n + off;
        w16(rec, q->plat); w16(rec + 2, q->enc); w16(rec + 4, q->lang); w16(rec + 6, q->id);
        w16(rec + 8, (unsigned)q->len); w16(rec + 10, (unsigned)off);
        if (q->kind == 1) { size_t k; const WCHAR *w = (const WCHAR *)q->p; for (k = 0; k < q->len / 2; k++) w16(dst + 2 * k, w[k]); }
        else if (q->len) memcpy(dst, q->p, q->len);
        off += q->len;
    }
    free(ord);
    *len = total;
    return t;
}

const ls_rec ls_base_recs[4] = {
    RW(3, 1, 0x409, 1, L"LsFam"), RW(3, 1, 0x409, 2, L"Regular"),
    RW(3, 1, 0x409, 4, L"LsFam"), RW(3, 1, 0x409, 6, L"LsFam"),
};

uint8_t *ls_font_blob(const ls_rec *r, int n, size_t *size) {
    ttf_spec s; ttf_rawtable raw; size_t tl; uint8_t *nt, *blob;
    ttf_default_spec(&s);
    nt = ls_name_table(r, n, &tl);
    s.omit |= TTF_OMIT_NAME;
    memcpy(raw.tag, "name", 5); raw.data = nt; raw.len = (unsigned)tl;
    s.raw = &raw; s.nraw = 1;
    blob = ttf_build(&s, size);
    free(nt);
    return blob;
}

static int merge_recs(ls_rec *dst, int cap, int base, const ls_rec *r, int n) {
    int k = 0, i;
    if (base) for (i = 0; i < 4 && k < cap; i++) dst[k++] = ls_base_recs[i];
    for (i = 0; i < n && k < cap; i++) dst[k++] = r[i];
    return k;
}

HRESULT ls_open_face(ls_fx *x, IDWriteFactory *f, int base, const ls_rec *r, int n) {
    ls_rec all[160]; int k = merge_recs(all, 160, base, r, n);
    size_t sz; uint8_t *blob = ls_font_blob(all, k, &sz); HRESULT hr;
    memset(x, 0, sizeof *x);
    if (!blob) return E_FAIL;
    hr = dwx_font_open(&x->h, f, blob, sz, 0, DWRITE_FONT_SIMULATIONS_NONE);
    free(blob);
    x->f = f;
    x->face = x->h.face;
    if (SUCCEEDED(hr) && x->face && hr == S_OK) {
        if (FAILED(IDWriteFontFace_QueryInterface(x->face, &IID_IDWriteFontFace3, (void **)&x->face3))) x->face3 = NULL;
    } else if (SUCCEEDED(hr)) hr = E_FAIL;
    return hr;
}

HRESULT ls_open_coll(ls_fx *x, IDWriteFactory *f, int base, const ls_rec *r, int n) {
    ls_rec all[160]; int k = merge_recs(all, 160, base, r, n);
    size_t sz; uint8_t *blob = ls_font_blob(all, k, &sz); HRESULT hr; UINT32 key;
    memset(x, 0, sizeof *x);
    x->is_coll = 1; x->f = f;
    if (!blob) return E_FAIL;
    x->fl = dwx_loader_new();
    key = dwx_loader_add(x->fl, blob, sz); free(blob);
    hr = dwx_loader_register(x->fl, f); if (FAILED(hr)) return hr;
    x->cl = dwx_coll_new(x->fl); dwx_coll_add_key(x->cl, key);
    hr = dwx_coll_register(x->cl, f); if (FAILED(hr)) return hr;
    hr = dwx_coll_create(x->cl, f, &x->coll); if (FAILED(hr)) return hr;
    if (IDWriteFontCollection_GetFontFamilyCount(x->coll) < 1) return E_FAIL;
    hr = IDWriteFontCollection_GetFontFamily(x->coll, 0, &x->fam); if (FAILED(hr)) return hr;
    hr = IDWriteFontFamily_GetFont(x->fam, 0, &x->font); if (FAILED(hr)) return hr;
    hr = IDWriteFont_CreateFontFace(x->font, &x->cface); if (FAILED(hr)) return hr;
    if (FAILED(IDWriteFontFace_QueryInterface(x->cface, &IID_IDWriteFontFace3, (void **)&x->cface3))) x->cface3 = NULL;
    x->face = x->cface; x->face3 = x->cface3;
    return x->cface3 ? S_OK : E_NOINTERFACE;
}

void ls_close(ls_fx *x) {
    if (x->is_coll) {
        if (x->cface3 == x->face3) x->face3 = NULL;
        DW_RELEASE(x->cface3); DW_RELEASE(x->cface); x->face = NULL;
        DW_RELEASE(x->font); DW_RELEASE(x->fam); DW_RELEASE(x->coll);
        if (x->cl) { dwx_coll_unregister(x->cl); dwx_release(x->cl); x->cl = NULL; }
        if (x->fl) { dwx_loader_unregister(x->fl); dwx_release(x->fl); x->fl = NULL; }
    } else {
        DW_RELEASE(x->face3);
        dwx_font_close(&x->h);
    }
}

/* ---- multi-locale fixture -------------------------------------------------------------------------------- */
const ls_loc ls_multi[LS_NMULTI] = {
    { "en-us", 0x409, L"LsFam", L"Regular" },
    { "de-de", 0x407, L"LsFam Schön", L"Normal" },
    { "fr-fr", 0x40C, L"LsFam Été", L"Régulier" },
    { "ja-jp", 0x411, L"LsFam テスト", L"標準" },
    { "ru-ru", 0x419, L"LsFam Тест", L"Обычный" },
};
static const ls_rec multi_recs[] = {
    RW(3, 1, 0x409, 0, L"Copyright (c) LsFam"), RW(3, 1, 0x407, 0, L"Urheberrecht © LsFam"),
    RW(3, 1, 0x409, 1, L"LsFam"), RW(3, 1, 0x407, 1, L"LsFam Schön"), RW(3, 1, 0x40C, 1, L"LsFam Été"),
    RW(3, 1, 0x411, 1, L"LsFam テスト"), RW(3, 1, 0x419, 1, L"LsFam Тест"),
    RW(3, 1, 0x409, 2, L"Regular"), RW(3, 1, 0x407, 2, L"Normal"), RW(3, 1, 0x40C, 2, L"Régulier"),
    RW(3, 1, 0x411, 2, L"標準"), RW(3, 1, 0x419, 2, L"Обычный"),
    RW(3, 1, 0x409, 4, L"LsFam"), RW(3, 1, 0x407, 4, L"LsFam Schön"),
    RW(3, 1, 0x409, 5, L"Version 1.000"), RW(3, 1, 0x409, 6, L"LsFam"),
    RW(3, 1, 0x409, 7, L"LsFam is a trademark"), RW(3, 1, 0x409, 8, L"LsFam Foundry"), RW(3, 1, 0x409, 9, L"Ls Designer"),
    RW(3, 1, 0x409, 10, L"A synthetic test font"), RW(3, 1, 0x407, 10, L"Eine synthetische Testschrift"),
    RW(3, 1, 0x409, 11, L"http://example.com/vendor"), RW(3, 1, 0x409, 12, L"http://example.com/designer"),
    RW(3, 1, 0x409, 13, L"Free to use"), RW(3, 1, 0x409, 14, L"http://example.com/license"),
    RW(3, 1, 0x409, 19, L"Sphinx of black quartz"),
};
HRESULT ls_open_multi_coll(ls_fx *x, IDWriteFactory *f) { return ls_open_coll(x, f, 0, multi_recs, (int)(sizeof multi_recs / sizeof multi_recs[0])); }
HRESULT ls_open_multi_face(ls_fx *x, IDWriteFactory *f) { return ls_open_face(x, f, 0, multi_recs, (int)(sizeof multi_recs / sizeof multi_recs[0])); }

/* ---- sources ------------------------------------------------------------------------------------------------ */
static const char *const src_names[LS_S_COUNT] = {
    "FontFace3::GetFamilyNames", "FontFace3::GetFaceNames", "FontFace3::GetInformationalStrings(WIN32_FAMILY)",
    "FontFamily::GetFamilyNames", "Font::GetFaceNames", "Font::GetInformationalStrings(WIN32_FAMILY)",
    "Font::GetInformationalStrings(FULL_NAME)", "FontFace6::GetFamilyNames", "FontFace6::GetFaceNames"
};
const char *ls_src_name(int src) { return src >= 0 && src < LS_S_COUNT ? src_names[src] : "?"; }

HRESULT ls_get_src(ls_fx *x, int src, IDWriteLocalizedStrings **out) {
    BOOL ex = FALSE; HRESULT hr; IDWriteFontFace6 *f6 = NULL;
    *out = NULL;
    switch (src) {
    case LS_S_FACE_FAMILY: return IDWriteFontFace3_GetFamilyNames(x->cface3, out);
    case LS_S_FACE_FACE: return IDWriteFontFace3_GetFaceNames(x->cface3, out);
    case LS_S_FACE_INFO_WIN32FAM:
        hr = IDWriteFontFace3_GetInformationalStrings(x->cface3, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, out, &ex);
        return SUCCEEDED(hr) && !ex ? E_FAIL : hr;
    case LS_S_FAMILY_FAMILY: return IDWriteFontFamily_GetFamilyNames(x->fam, out);
    case LS_S_FONT_FACE: return IDWriteFont_GetFaceNames(x->font, out);
    case LS_S_FONT_INFO_WIN32FAM:
        hr = IDWriteFont_GetInformationalStrings(x->font, DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES, out, &ex);
        return SUCCEEDED(hr) && !ex ? E_FAIL : hr;
    case LS_S_FONT_INFO_FULL:
        hr = IDWriteFont_GetInformationalStrings(x->font, DWRITE_INFORMATIONAL_STRING_FULL_NAME, out, &ex);
        return SUCCEEDED(hr) && !ex ? E_FAIL : hr;
    case LS_S_FACE6_FAMILY: case LS_S_FACE6_FACE:
        if (FAILED(IDWriteFontFace_QueryInterface(x->cface, &IID_IDWriteFontFace6, (void **)&f6)) || !f6) return S_FALSE;
        hr = src == LS_S_FACE6_FAMILY ? IDWriteFontFace6_GetFamilyNames(f6, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, out)
                                      : IDWriteFontFace6_GetFaceNames(f6, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, out);
        IDWriteFontFace6_Release(f6);
        return hr;
    }
    return E_INVALIDARG;
}
const WCHAR *ls_src_expect_en(int src) {
    switch (src) {
    case LS_S_FACE_FACE: case LS_S_FONT_FACE: case LS_S_FACE6_FACE: return L"Regular";
    default: return L"LsFam";
    }
}
unsigned ls_src_expect_count(int src) {
    switch (src) {
    case LS_S_FACE_FAMILY: case LS_S_FACE_FACE: case LS_S_FACE_INFO_WIN32FAM: case LS_S_FONT_FACE: case LS_S_FONT_INFO_WIN32FAM: return 5;
    case LS_S_FONT_INFO_FULL: return 2;
    default: return 0;      /* not pinned (family / FontFace6 models) */
    }
}

/* ---- string helpers --------------------------------------------------------------------------------------------- */
WCHAR *ls_dup_string(IDWriteLocalizedStrings *s, UINT32 i) {
    UINT32 len = 0; WCHAR *w;
    if (FAILED(IDWriteLocalizedStrings_GetStringLength(s, i, &len))) return NULL;
    w = (WCHAR *)calloc((size_t)len + 2, sizeof(WCHAR));
    if (FAILED(IDWriteLocalizedStrings_GetString(s, i, w, len + 1))) { free(w); return NULL; }
    return w;
}
WCHAR *ls_dup_locale(IDWriteLocalizedStrings *s, UINT32 i) {
    UINT32 len = 0; WCHAR *w;
    if (FAILED(IDWriteLocalizedStrings_GetLocaleNameLength(s, i, &len))) return NULL;
    w = (WCHAR *)calloc((size_t)len + 2, sizeof(WCHAR));
    if (FAILED(IDWriteLocalizedStrings_GetLocaleName(s, i, w, len + 1))) { free(w); return NULL; }
    return w;
}
int ls_wcsieq(const WCHAR *a, const WCHAR *b) { return CompareStringOrdinal(a, -1, b, -1, TRUE) == CSTR_EQUAL; }
int ls_index_of(IDWriteLocalizedStrings *s, const WCHAR *loc) {
    UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteLocalizedStrings_FindLocaleName(s, loc, &idx, &ex)) || !ex) return -1;
    return (int)idx;
}
WCHAR *ls_en(IDWriteLocalizedStrings *s) { int i = ls_index_of(s, L"en-us"); return i < 0 ? NULL : ls_dup_string(s, (UINT32)i); }

int ls_sweep(IDWriteLocalizedStrings *s, const char *what) {
    UINT32 n = IDWriteLocalizedStrings_GetCount(s), i; int bad = 0;
    for (i = 0; i < n && i < 256; i++) {
        UINT32 ll = 0xCCCC, sl = 0xCCCC, idx = 0; BOOL ex = FALSE; WCHAR lb[200], sb[1200]; HRESULT hr;
        hr = IDWriteLocalizedStrings_GetLocaleNameLength(s, i, &ll);
        if (hr != S_OK || ll == 0 || ll >= 100) { bad++; CHECKF(0, "%s[%u] locale length hr=%s len=%u", what, i, dw_hr_name(hr), ll); continue; }
        hr = IDWriteLocalizedStrings_GetStringLength(s, i, &sl);
        if (hr != S_OK || sl >= 1100) { bad++; CHECKF(0, "%s[%u] string length hr=%s len=%u", what, i, dw_hr_name(hr), sl); continue; }
        /* locale: exact fit */
        memset(lb, 0x55, sizeof lb);
        hr = IDWriteLocalizedStrings_GetLocaleName(s, i, lb, ll + 1);
        if (hr != S_OK || lstrlenW(lb) != (int)ll) { bad++; CHECKF(0, "%s[%u] GetLocaleName exact hr=%s", what, i, dw_hr_name(hr)); }
        /* locale: one short -> insufficient buffer (the truncated content is pinned by the protocol tests) */
        memset(lb, 0x55, sizeof lb);
        hr = IDWriteLocalizedStrings_GetLocaleName(s, i, lb, ll);
        if (hr != DW_E_NOT_SUFFICIENT_BUFFER) { bad++; CHECKF(0, "%s[%u] GetLocaleName short hr=%s", what, i, dw_hr_name(hr)); }
        /* string: exact fit / short */
        memset(sb, 0x55, sizeof sb);
        hr = IDWriteLocalizedStrings_GetString(s, i, sb, sl + 1);
        if (hr != S_OK || (UINT32)lstrlenW(sb) > sl) { bad++; CHECKF(0, "%s[%u] GetString exact hr=%s", what, i, dw_hr_name(hr)); }
        if (sl > 0) {
            memset(sb, 0x55, sizeof sb);
            hr = IDWriteLocalizedStrings_GetString(s, i, sb, sl);
            if (hr != DW_E_NOT_SUFFICIENT_BUFFER) { bad++; CHECKF(0, "%s[%u] GetString short hr=%s", what, i, dw_hr_name(hr)); }
        }
        /* find by its own locale name is exact */
        {
            WCHAR l2[200]; IDWriteLocalizedStrings_GetLocaleName(s, i, l2, 200);
            hr = IDWriteLocalizedStrings_FindLocaleName(s, l2, &idx, &ex);
            if (hr != S_OK || !ex || idx != i) { bad++; CHECKF(0, "%s[%u] FindLocaleName(own) hr=%s ex=%d idx=%u", what, i, dw_hr_name(hr), ex, idx); }
        }
    }
    return bad;
}
