#include "fto.h"

void fto_both(fto_body_fn fn) {
    IDWriteFactory *sh = dw_shared(), *iso;
    if (!sh) { t_skip("DWriteCreateFactory unavailable"); return; }
    fn(sh, "shared");
    iso = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!iso) { t_note("isolated factory unavailable"); return; }
    fn(iso, "isolated");
    IDWriteFactory_Release(iso);
}

WCHAR *fto_repeat(WCHAR c, size_t n) {
    WCHAR *s = (WCHAR *)malloc((n + 1) * sizeof(WCHAR));
    size_t i;
    if (!s) return NULL;
    for (i = 0; i < n; i++) s[i] = c;
    s[n] = 0;
    return s;
}

WCHAR *fto_long_locale(size_t n) {
    WCHAR *s = (WCHAR *)malloc((n + 1) * sizeof(WCHAR));
    size_t i;
    if (!s) return NULL;
    s[0] = 'e'; s[1] = 'n';
    for (i = 2; i < n; i++) s[i] = ((i - 2) % 9 == 0) ? '-' : 'a' + (WCHAR)((i - 2) % 9 - 1);
    s[n] = 0;
    /* A trailing '-' would be an empty subtag: turn the last two characters into "-z" (previous subtag shrinks to 7). */
    if (n > 3 && s[n - 1] == '-') { s[n - 2] = '-'; s[n - 1] = 'z'; }
    return s;
}

IDWriteTextFormat *fto_format(IDWriteFactory *f, float size) {
    IDWriteTextFormat *fmt = NULL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(f, L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                 DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmt);
    if (FAILED(hr)) return NULL;
    return fmt;
}

void fto_coll_setup(fto_coll *fc, IDWriteFactory *f, const char *family) {
    ttf_spec s; size_t n = 0; uint8_t *b;
    memset(fc, 0, sizeof *fc);
    fc->f = f;
    dw_wide(family, fc->family, 32);
    ttf_style_spec(&s, family, 400, 5, 0);
    b = ttf_build(&s, &n);
    if (!b) { fc->hr = E_FAIL; return; }
    fc->fl = dwx_loader_new();
    fc->key = dwx_loader_add(fc->fl, b, n);
    free(b);
    fc->hr = dwx_loader_register(fc->fl, f);
    if (FAILED(fc->hr)) return;
    fc->cl = dwx_coll_new(fc->fl);
    dwx_coll_add_key(fc->cl, fc->key);
    fc->hr = dwx_coll_register(fc->cl, f);
    if (FAILED(fc->hr)) return;
    fc->hr = dwx_coll_create(fc->cl, f, &fc->coll);
}

void fto_coll_teardown(fto_coll *fc) {
    DW_RELEASE(fc->coll);
    if (fc->cl) { dwx_coll_unregister(fc->cl); dwx_release(fc->cl); fc->cl = NULL; }
    if (fc->fl) { dwx_loader_unregister(fc->fl); dwx_release(fc->fl); fc->fl = NULL; }
}

WCHAR *fto_get_locale(IDWriteTextFormat *fmt) {
    UINT32 n = IDWriteTextFormat_GetLocaleNameLength(fmt);
    WCHAR *s = (WCHAR *)malloc((n + 1) * sizeof(WCHAR));
    if (!s) return NULL;
    if (FAILED(IDWriteTextFormat_GetLocaleName(fmt, s, n + 1))) { free(s); return NULL; }
    return s;
}

WCHAR *fto_get_family(IDWriteTextFormat *fmt) {
    UINT32 n = IDWriteTextFormat_GetFontFamilyNameLength(fmt);
    WCHAR *s = (WCHAR *)malloc((n + 1) * sizeof(WCHAR));
    if (!s) return NULL;
    if (FAILED(IDWriteTextFormat_GetFontFamilyName(fmt, s, n + 1))) { free(s); return NULL; }
    return s;
}
