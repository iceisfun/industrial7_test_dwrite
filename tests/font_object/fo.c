#include "fo.h"

void fo_close(fo_fx *x) {
    int i;
    for (i = 0; i < FO_MAX; i++) DW_RELEASE(x->font[i]);
    DW_RELEASE(x->coll);
    if (x->cl) dwx_coll_unregister(x->cl);
    if (x->fl) dwx_loader_unregister(x->fl);
    if (x->cl) { dwx_release(x->cl); x->cl = NULL; }
    if (x->fl) { dwx_release(x->fl); x->fl = NULL; }
    DW_RELEASE(x->f);
}

UINT32 fo_font_key(IDWriteFont *font) {
    IDWriteFontFace *face = NULL; IDWriteFontFile *files[2] = { 0 }; UINT32 nf = 0, key = 0;
    if (FAILED(IDWriteFont_CreateFontFace(font, &face)) || !face) return 0;
    if (SUCCEEDED(IDWriteFontFace_GetFiles(face, &nf, NULL)) && nf >= 1 && nf <= 2 &&
        SUCCEEDED(IDWriteFontFace_GetFiles(face, &nf, files))) {
        const void *k = NULL; UINT32 ks = 0;
        if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(files[0], &k, &ks)) && ks == 4) memcpy(&key, k, 4);
        { UINT32 i; for (i = 0; i < nf; i++) DW_RELEASE(files[i]); }
    }
    IDWriteFontFace_Release(face);
    return key;
}

int fo_open(fo_fx *x, const ttf_spec *specs, int n) {
    int i, ok = 1; UINT32 nfam, fi, k;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { x->hr_coll = E_FAIL; return 0; }
    x->fl = dwx_loader_new();
    for (i = 0; i < n; i++) {
        size_t sz; uint8_t *b = ttf_build(&specs[i], &sz);
        if (!b) { x->hr_coll = E_FAIL; return 0; }
        x->keys[i] = dwx_loader_add(x->fl, b, sz); free(b);
    }
    x->n = n;
    if (FAILED(x->hr_coll = dwx_loader_register(x->fl, x->f))) return 0;
    x->cl = dwx_coll_new(x->fl);
    for (i = 0; i < n; i++) dwx_coll_add_key(x->cl, x->keys[i]);
    if (FAILED(x->hr_coll = dwx_coll_register(x->cl, x->f))) return 0;
    if (FAILED(x->hr_coll = dwx_coll_create(x->cl, x->f, &x->coll)) || !x->coll) return 0;
    nfam = IDWriteFontCollection_GetFontFamilyCount(x->coll);
    for (fi = 0; fi < nfam; fi++) {
        IDWriteFontFamily *fam = NULL; UINT32 nf;
        if (FAILED(IDWriteFontCollection_GetFontFamily(x->coll, fi, &fam)) || !fam) continue;
        nf = IDWriteFontFamily_GetFontCount(fam);
        for (k = 0; k < nf; k++) {
            IDWriteFont *font = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &font)) && font) {
                UINT32 key = fo_font_key(font); int found = 0;
                for (i = 0; i < n; i++) if (x->keys[i] == key && !x->font[i]) { x->font[i] = font; found = 1; break; }
                if (!found) IDWriteFont_Release(font);
            }
        }
        IDWriteFontFamily_Release(fam);
    }
    for (i = 0; i < n; i++) if (!x->font[i]) ok = 0;
    if (!ok) x->hr_coll = DWRITE_E_NOFONT;
    return ok;
}
int fo_open1(fo_fx *x, const ttf_spec *spec) { return fo_open(x, spec, 1); }

IDWriteFont *fo_sysfont(IDWriteFactory *f, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STRETCH s, DWRITE_FONT_STYLE st) {
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c) return NULL;
    IDWriteFontCollection_FindFamilyName(c, family, &idx, &ex);
    if (ex && SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam) {
        if (FAILED(IDWriteFontFamily_GetFirstMatchingFont(fam, w, s, st, &font))) font = NULL;
        IDWriteFontFamily_Release(fam);
    }
    IDWriteFontCollection_Release(c);
    return font;
}

static void *g_keep[512]; static int g_nkeep;
void fo_os2_patch(ttf_spec *s, unsigned off, const void *bytes, unsigned len) {
    static ttf_rawtable raws[1024]; static int nr;
    size_t sz; uint8_t *blob = ttf_build(s, &sz); unsigned tl = 0; const uint8_t *t;
    uint8_t *copy; int i;
    if (!blob) return;
    t = ttf_find_table(blob, sz, "OS/2", &tl);
    if (!t || off + len > tl || g_nkeep >= 512 || nr >= 1000) { free(blob); return; }
    copy = (uint8_t *)malloc(tl); memcpy(copy, t, tl); memcpy(copy + off, bytes, len); free(blob);
    g_keep[g_nkeep++] = copy;
    /* replace an existing raw OS/2 of this spec, else append (the spec keeps pointers: copy the raw array) */
    {
        ttf_rawtable *arr = &raws[nr]; int cnt = 0;
        for (i = 0; i < s->nraw; i++) { if (!memcmp(s->raw[i].tag, "OS/2", 4)) continue; arr[cnt++] = s->raw[i]; }
        memcpy(arr[cnt].tag, "OS/2", 5); arr[cnt].data = copy; arr[cnt].len = tl; cnt++;
        s->raw = arr; s->nraw = cnt; nr += cnt;
    }
}
void fo_set_panose(ttf_spec *s, const uint8_t p[10]) { fo_os2_patch(s, 32, p, 10); }   /* OS/2.panose at byte offset 32 */

void fo_spec(ttf_spec *s, const char *family) {
    char ps[96]; snprintf(ps, sizeof ps, "%s-Regular", family);
    ttf_default_spec(s); ttf_set_names(s, family, "Regular", family, ps);
}
void fo_symbol_spec(ttf_spec *s, const char *family) {
    static ttf_map map[27]; static ttf_cmap cm; int i;
    fo_spec(s, family);
    map[0].cp = 0xF020; map[0].gid = 1;
    for (i = 0; i < 26; i++) { map[1 + i].cp = 0xF041 + i; map[1 + i].gid = 2 + i; }
    cm.format = 4; cm.plat = 3; cm.enc = 0; cm.n = 27; cm.map = map;
    s->cmaps = &cm; s->ncmaps = 1;
}
void fo_ls(IDWriteLocalizedStrings *ls, UINT32 idx, char *out, int cap) {
    out[0] = 0; if (FAILED(dw_ls_get(ls, idx, out, cap))) out[0] = 0;
}
void fo_ls_locale(IDWriteLocalizedStrings *ls, UINT32 idx, char *out, int cap) {
    WCHAR w[LOCALE_NAME_MAX_LENGTH]; UINT32 len = 0; out[0] = 0;
    if (SUCCEEDED(IDWriteLocalizedStrings_GetLocaleNameLength(ls, idx, &len)) && len < LOCALE_NAME_MAX_LENGTH &&
        SUCCEEDED(IDWriteLocalizedStrings_GetLocaleName(ls, idx, w, len + 1))) dw_narrow(w, out, cap);
}
int fo_sys_table(IDWriteFont *font, const char *tag, uint8_t **data, UINT32 *len) {
    IDWriteFontFace *face = NULL; int ok = 0;
    *data = NULL; *len = 0;
    if (FAILED(IDWriteFont_CreateFontFace(font, &face)) || !face) return 0;
    *data = dw_face_table(face, DW_TAG(tag[0], tag[1], tag[2], tag[3]), len);
    ok = *data != NULL;
    IDWriteFontFace_Release(face);
    return ok;
}
static unsigned be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
int fo_name_find(const uint8_t *t, UINT32 len, unsigned plat, unsigned enc, unsigned lang, unsigned id, WCHAR *out, int cap) {
    unsigned n, so, i;
    if (len < 6) return -1;
    n = be16(t + 2); so = be16(t + 4);
    for (i = 0; i < n; i++) {
        const uint8_t *r = t + 6 + 12 * i; unsigned sl, soff, k;
        if (6 + 12 * (i + 1) > len) return -1;
        if (be16(r) != plat || be16(r + 2) != enc || be16(r + 4) != lang || be16(r + 6) != id) continue;
        sl = be16(r + 8); soff = be16(r + 10);
        if (so + soff + sl > len) return -1;
        for (k = 0; k < sl / 2 && (int)k < cap - 1; k++) out[k] = (WCHAR)be16(t + so + soff + 2 * k);
        out[k] = 0; return (int)k;
    }
    return -1;
}
int fo_ls_w(IDWriteLocalizedStrings *ls, UINT32 idx, WCHAR *out, int cap) {
    UINT32 len = 0; out[0] = 0;
    if (FAILED(IDWriteLocalizedStrings_GetStringLength(ls, idx, &len)) || (int)len + 1 > cap) return 0;
    if (FAILED(IDWriteLocalizedStrings_GetString(ls, idx, out, len + 1))) return 0;
    return (int)len;
}
int fo_ls_find_ci(IDWriteLocalizedStrings *ls, const char *locale) {
    UINT32 i, n = IDWriteLocalizedStrings_GetCount(ls); char b[LOCALE_NAME_MAX_LENGTH];
    for (i = 0; i < n; i++) { fo_ls_locale(ls, i, b, sizeof b); if (!_stricmp(b, locale)) return (int)i; }
    return -1;
}
