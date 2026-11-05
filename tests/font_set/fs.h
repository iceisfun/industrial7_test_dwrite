/* Shared scaffolding for the font set area (IDWriteFontSet*, IDWriteFontSetBuilder*, IDWriteStringList).
 *
 * Fixture: an ISOLATED factory (so loader registrations never leak between tests) with one in-memory font file loader.
 * Synthetic fonts are added as blobs; sets are built by IDWriteFontSetBuilder1::AddFontFile.
 * Order of teardown: release every object built from the loader, then fs_close() (unregisters the loader, releases the factory).
 *
 * Synthetic font naming (ttf_style_spec): RIBBI faces (400/700 at width 5, regular or italic) carry id1 = family, id2 = Regular /
 * Bold / Italic / Bold Italic and no typographic names; every other face carries id1 = "<family> <style words>" plus id16/id17.
 */
#ifndef FS_H
#define FS_H
#include "dwx.h"
#include <wchar.h>

#define FS_E_NOTSUFF   ((HRESULT)0x8007007A)            /* HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) */
#define FS_NOT_FOUND   0xFFFFFFFFu

/* Pointer sentinel for out-parameter zeroing checks; release only what a successful call produced. */
#define FS_SENT ((void *)(ULONG_PTR)0x1)
#define FS_DROP(p, hr) do { if (SUCCEEDED(hr)) DW_RELEASE(p); (p) = NULL; } while (0)

/* Observed-but-not-proven expectation: reported with t_note, never fails the test. */
#define FS_SOFT(c) do { if (!(c)) t_note("soft expectation not met at line %d: %s", __LINE__, #c); } while (0)

typedef struct fs_fx {
    IDWriteFactory *f; IDWriteFactory3 *f3; dwx_mem_loader *fl; int ok;
} fs_fx;

static int fs_open(fs_fx *x)
{
    HRESULT hr;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!x->f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    hr = IDWriteFactory_QueryInterface(x->f, &IID_IDWriteFactory3, (void **)&x->f3);
    if (FAILED(hr) || !x->f3) { DW_RELEASE(x->f); t_skip("IDWriteFactory3 unsupported"); return 0; }
    x->fl = dwx_loader_new();
    hr = dwx_loader_register(x->fl, x->f);
    if (FAILED(hr)) t_note("RegisterFontFileLoader failed: %s", dw_hr_name(hr));
    x->ok = 1;
    return 1;
}
static void fs_close(fs_fx *x)
{
    if (!x->ok) return;
    dwx_loader_unregister(x->fl);
    dwx_release(x->fl);
    DW_RELEASE(x->f3);
    DW_RELEASE(x->f);
    x->ok = 0;
}
#define FS_OPEN(x) fs_fx x; if (!fs_open(&x)) return

/* Windows-only (platform 3) names: removes the Macintosh name records from a spec. */
static void fs_drop_mac_names(ttf_spec *s)
{
    int i, j = 0;
    for (i = 0; i < s->nnames; i++) if (s->names[i].plat != 1) s->names[j++] = s->names[i];
    s->nnames = j;
}
static void fs_style(ttf_spec *s, const char *family, int weight, int width, int italic)
{
    ttf_style_spec(s, family, weight, width, italic);
    fs_drop_mac_names(s);
}
static UINT32 fs_add_spec(fs_fx *x, const ttf_spec *s)
{
    size_t n; uint8_t *b = ttf_build(s, &n); UINT32 k;
    if (!b) { t_note("ttf_build failed"); return 0xFFFFFFFFu; }
    k = dwx_loader_add(x->fl, b, n);
    free(b);
    return k;
}
static UINT32 fs_add(fs_fx *x, const char *family, int weight, int width, int italic)
{
    ttf_spec s; fs_style(&s, family, weight, width, italic);
    return fs_add_spec(x, &s);
}
static UINT32 fs_add_blob(fs_fx *x, const void *b, size_t n) { return dwx_loader_add(x->fl, b, n); }

static IDWriteFontFile *fs_file(fs_fx *x, UINT32 key)
{
    IDWriteFontFile *file = NULL;
    HRESULT hr = dwx_loader_file(x->fl, x->f, key, &file);
    if (FAILED(hr)) { t_note("CreateCustomFontFileReference failed: %s", dw_hr_name(hr)); return NULL; }
    return file;
}
static IDWriteFontFaceReference *fs_ref_ex(fs_fx *x, UINT32 key, UINT32 face_index, DWRITE_FONT_SIMULATIONS sim)
{
    IDWriteFontFile *file = fs_file(x, key); IDWriteFontFaceReference *ref = NULL; HRESULT hr;
    if (!file) return NULL;
    hr = IDWriteFactory3_CreateFontFaceReference_(x->f3, file, face_index, sim, &ref);
    IDWriteFontFile_Release(file);
    if (FAILED(hr)) { t_note("CreateFontFaceReference failed: %s", dw_hr_name(hr)); return NULL; }
    return ref;
}
static IDWriteFontFaceReference *fs_ref(fs_fx *x, UINT32 key) { return fs_ref_ex(x, key, 0, DWRITE_FONT_SIMULATIONS_NONE); }

static IDWriteFontSetBuilder *fs_builder(fs_fx *x)
{
    IDWriteFontSetBuilder *b = NULL;
    HRESULT hr = IDWriteFactory3_CreateFontSetBuilder(x->f3, &b);
    if (FAILED(hr) || !b) { t_note("CreateFontSetBuilder failed: %s", dw_hr_name(hr)); return NULL; }
    return b;
}
static IDWriteFontSetBuilder1 *fs_builder1(IDWriteFontSetBuilder *b)
{
    IDWriteFontSetBuilder1 *b1 = NULL;
    if (FAILED(IDWriteFontSetBuilder_QueryInterface(b, &IID_IDWriteFontSetBuilder1, (void **)&b1))) return NULL;
    return b1;
}

/* Builds a set from blobs by AddFontFile; skips the calling test when anything fails. keys[] are loader keys. */
static HRESULT fs_build_keys(fs_fx *x, const UINT32 *keys, int n, IDWriteFontSet **out)
{
    IDWriteFontSetBuilder *b = fs_builder(x); IDWriteFontSetBuilder1 *b1; int i; HRESULT hr = E_FAIL;
    *out = NULL;
    if (!b) return E_FAIL;
    b1 = fs_builder1(b);
    if (!b1) { IDWriteFontSetBuilder_Release(b); return E_NOINTERFACE; }
    for (i = 0; i < n; i++) {
        IDWriteFontFile *file = fs_file(x, keys[i]);
        if (!file) { IDWriteFontSetBuilder1_Release(b1); IDWriteFontSetBuilder_Release(b); return E_FAIL; }
        hr = IDWriteFontSetBuilder1_AddFontFile(b1, file);
        IDWriteFontFile_Release(file);
        if (FAILED(hr)) t_note("AddFontFile(key %u) failed: %s", keys[i], dw_hr_name(hr));
    }
    hr = IDWriteFontSetBuilder_CreateFontSet(b, out);
    IDWriteFontSetBuilder1_Release(b1);
    IDWriteFontSetBuilder_Release(b);
    return hr;
}

/* The standard 5-face fixture: SynthA {Regular, Bold, Italic, Bold Italic}, SynthB Regular. Keys in fk[0..4]. */
typedef struct fs_std { fs_fx x; UINT32 fk[8]; int nk; IDWriteFontSet *set; int ok; } fs_std;
static int fs_std_open(fs_std *s)
{
    memset(s, 0, sizeof *s);
    if (!fs_open(&s->x)) return 0;
    s->fk[s->nk++] = fs_add(&s->x, "SynthA", 400, 5, 0);
    s->fk[s->nk++] = fs_add(&s->x, "SynthA", 700, 5, 0);
    s->fk[s->nk++] = fs_add(&s->x, "SynthA", 400, 5, 1);
    s->fk[s->nk++] = fs_add(&s->x, "SynthA", 700, 5, 1);
    s->fk[s->nk++] = fs_add(&s->x, "SynthB", 400, 5, 0);
    {
        HRESULT hr = fs_build_keys(&s->x, s->fk, s->nk, &s->set);
        if (FAILED(hr) || !s->set) { t_skip("prerequisite font set build failed: %s", dw_hr_name(hr)); fs_close(&s->x); return 0; }
    }
    s->ok = 1;
    return 1;
}
static void fs_std_close(fs_std *s)
{
    if (!s->ok) return;
    DW_RELEASE(s->set);
    fs_close(&s->x);
    s->ok = 0;
}
#define FS_STD(s) fs_std s; if (!fs_std_open(&s)) return

/* ---- string helpers ------------------------------------------------------------------------------------ */
static const char *fs_u8(const WCHAR *w)
{
    static char ring[8][256]; static int n; char *b = ring[n++ & 7];
    if (!w) return "(null)";
    if (!dw_narrow(w, b, 256)) strcpy(b, "?");
    return b;
}
/* String #i of an IDWriteStringList as UTF-8 ("" on failure). */
static HRESULT fs_sl_get(IDWriteStringList *l, UINT32 i, char *buf, int cap)
{
    UINT32 len = 0; WCHAR w[512]; HRESULT hr;
    buf[0] = 0;
    hr = IDWriteStringList_GetStringLength(l, i, &len);
    if (FAILED(hr) || len >= 512) return FAILED(hr) ? hr : E_FAIL;
    hr = IDWriteStringList_GetString(l, i, w, 512);
    if (FAILED(hr)) return hr;
    dw_narrow(w, buf, cap);
    return S_OK;
}
static HRESULT fs_sl_locale(IDWriteStringList *l, UINT32 i, char *buf, int cap)
{
    UINT32 len = 0; WCHAR w[128]; HRESULT hr;
    buf[0] = 0;
    hr = IDWriteStringList_GetLocaleNameLength(l, i, &len);
    if (FAILED(hr) || len >= 128) return FAILED(hr) ? hr : E_FAIL;
    hr = IDWriteStringList_GetLocaleName(l, i, w, 128);
    if (FAILED(hr)) return hr;
    dw_narrow(w, buf, cap);
    return S_OK;
}
/* Does the list contain the string (case-sensitive)? */
static int fs_sl_has(IDWriteStringList *l, const char *s)
{
    UINT32 i, n = IDWriteStringList_GetCount(l); char b[256];
    for (i = 0; i < n; i++) if (SUCCEEDED(fs_sl_get(l, i, b, sizeof b)) && !strcmp(b, s)) return 1;
    return 0;
}
/* Property values of one id for the whole set (NULL locale form); NULL on failure. */
static IDWriteStringList *fs_props(IDWriteFontSet *set, DWRITE_FONT_PROPERTY_ID id)
{
    IDWriteStringList *l = NULL;
    HRESULT hr = IDWriteFontSet_GetPropertyValues__(set, id, &l);
    if (FAILED(hr)) { t_note("GetPropertyValues(%d) failed: %s", (int)id, dw_hr_name(hr)); DW_RELEASE(l); return NULL; }
    return l;
}
/* First string of the per-font form, UTF-8; returns exists. */
static BOOL fs_font_prop(IDWriteFontSet *set, UINT32 index, DWRITE_FONT_PROPERTY_ID id, char *buf, int cap)
{
    BOOL exists = FALSE; IDWriteLocalizedStrings *s = NULL; HRESULT hr;
    buf[0] = 0;
    hr = IDWriteFontSet_GetPropertyValues(set, index, id, &exists, &s);
    if (FAILED(hr)) { DW_RELEASE(s); return FALSE; }
    if (exists && s && IDWriteLocalizedStrings_GetCount(s) > 0) dw_ls_en(s, buf, cap);
    DW_RELEASE(s);
    return exists;
}
/* Index of the font in the set whose property `id` equals `value` (first match), or FS_NOT_FOUND. */
static UINT32 fs_find_by(IDWriteFontSet *set, DWRITE_FONT_PROPERTY_ID id, const char *value)
{
    UINT32 i, n = IDWriteFontSet_GetFontCount(set); char b[256];
    for (i = 0; i < n; i++) if (fs_font_prop(set, i, id, b, sizeof b) && !strcmp(b, value)) return i;
    return FS_NOT_FOUND;
}
/* Indexes (ascending) of the fonts whose property `id` equals `value`; returns how many (at most max are stored). */
static int fs_indices_of(IDWriteFontSet *set, DWRITE_FONT_PROPERTY_ID id, const char *value, UINT32 *out, int max)
{
    UINT32 i, n = IDWriteFontSet_GetFontCount(set); int k = 0; char b[256];
    for (i = 0; i < n; i++)
        if (fs_font_prop(set, i, id, b, sizeof b) && !strcmp(b, value)) { if (k < max) out[k] = i; k++; }
    return k;
}
static int fs_atoi(const char *s) { return atoi(s); }
/* DWRITE_FONT_PROPERTY with a wide copy of an ASCII value. */
typedef struct { DWRITE_FONT_PROPERTY p; WCHAR v[128]; WCHAR l[32]; } fs_prop;
static void fs_prop_set(fs_prop *p, DWRITE_FONT_PROPERTY_ID id, const char *value, const char *locale)
{
    memset(p, 0, sizeof *p);
    p->p.propertyId = id;
    dw_wide(value, p->v, 128); p->p.propertyValue = p->v;
    if (locale) { dw_wide(locale, p->l, 32); p->p.localeName = p->l; } else p->p.localeName = NULL;
}
/* Font set pointer for an IDWriteFontSet1 / base conversions (same object, base vtable prefix). */
#define FS_AS_SET(p) ((IDWriteFontSet *)(p))

static IDWriteFontSet1 *fs_set1(IDWriteFontSet *set)
{
    IDWriteFontSet1 *s1 = NULL;
    if (FAILED(IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet1, (void **)&s1))) return NULL;
    return s1;
}
#define NEED_F3(var) IDWriteFactory3 *var = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); \
    if (!var) { t_skip("IDWriteFactory3 unsupported"); return; }
/* The shared factory's system font set (released by the caller), or skip. */
#define NEED_SYSSET(set) IDWriteFontSet *set = NULL; { NEED_F3(f3_); \
    HRESULT hr_ = IDWriteFactory3_GetSystemFontSet(f3_, &set); \
    if (FAILED(hr_) || !set) { t_skip("GetSystemFontSet failed: %s", dw_hr_name(hr_)); return; } }
#endif
