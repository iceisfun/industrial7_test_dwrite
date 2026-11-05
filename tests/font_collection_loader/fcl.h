/* Shared fixtures for the custom font collection tests.
 *
 * A fixture owns an ISOLATED factory (so loader registrations and the factory's collection cache never leak between tests),
 * one in-memory font file loader and one collection loader, both registered on that factory.  Fonts are synthetic TrueType
 * blobs; each blob becomes one entry in the collection loader's enumeration.
 *
 * Every CreateCustomFontCollection call made through fcl_create() uses a fresh collection key: the factory caches the
 * result of an enumeration by (loader id, key bytes), so a repeated key may legitimately not reach the loader again.
 */
#ifndef FCL_H
#define FCL_H
#include "dwx.h"

#ifndef DWRITE_E_ALREADYREGISTERED
#define DWRITE_E_ALREADYREGISTERED ((HRESULT)0x88985006)
#endif
#ifndef DWRITE_E_NOFONT
#define DWRITE_E_NOFONT ((HRESULT)0x88985002)
#endif
#define FCL_E_FILEFORMAT ((HRESULT)0x88985000)
#define FCL_E_NOFONT     ((HRESULT)0x88985002)
#define FCL_E_FAIL       ((HRESULT)0x80004005)

typedef struct fcl_fx {
    IDWriteFactory *f;
    dwx_mem_loader *fl;
    dwx_coll_loader *cl;
    int ok;
} fcl_fx;

/* Creates the factory + loaders and registers both loaders.  Returns 0 (and skips nothing) when the factory is unavailable. */
static int fcl_open_ex(fcl_fx *x, DWRITE_FACTORY_TYPE type, int reg)
{
    HRESULT hr;
    memset(x, 0, sizeof *x);
    x->f = dw_new_factory(type);
    if (!x->f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    x->fl = dwx_loader_new();
    x->cl = dwx_coll_new(x->fl);
    if (reg) {
        hr = dwx_loader_register(x->fl, x->f);
        if (FAILED(hr)) t_note("RegisterFontFileLoader failed: %s", dw_hr_name(hr));
        hr = dwx_coll_register(x->cl, x->f);
        if (FAILED(hr)) t_note("RegisterFontCollectionLoader failed: %s", dw_hr_name(hr));
    }
    x->ok = 1;
    return 1;
}
static int fcl_open(fcl_fx *x, DWRITE_FACTORY_TYPE type) { return fcl_open_ex(x, type, 1); }
#define FCL_OPEN(x) fcl_fx x; if (!fcl_open(&x, DWRITE_FACTORY_TYPE_ISOLATED)) return
#define FCL_OPEN_BARE(x) fcl_fx x; if (!fcl_open_ex(&x, DWRITE_FACTORY_TYPE_ISOLATED, 0)) return

/* Order matters: collection loader first, then the file loader, then the factory. */
static void fcl_close(fcl_fx *x)
{
    if (!x->ok) return;
    dwx_coll_unregister(x->cl);
    dwx_loader_unregister(x->fl);
    dwx_release(x->cl);
    dwx_release(x->fl);
    DW_RELEASE(x->f);
    x->ok = 0;
}

/* Adds a font blob to the file loader and to the collection loader's enumeration; returns the file key. */
static UINT32 fcl_add(fcl_fx *x, const void *blob, size_t n)
{
    UINT32 k = dwx_loader_add(x->fl, blob, n);
    dwx_coll_add_key(x->cl, k);
    return k;
}
/* Adds a blob to the file loader only (not enumerated). */
static UINT32 fcl_add_hidden(fcl_fx *x, const void *blob, size_t n) { return dwx_loader_add(x->fl, blob, n); }

static UINT32 fcl_add_spec(fcl_fx *x, const ttf_spec *s)
{
    size_t n; uint8_t *b = ttf_build(s, &n); UINT32 k;
    if (!b) { t_note("ttf_build failed"); return 0xFFFFFFFFu; }
    k = fcl_add(x, b, n);
    free(b);
    return k;
}
static UINT32 fcl_add_style(fcl_fx *x, const char *family, int weight, int width, int italic)
{
    ttf_spec s; ttf_style_spec(&s, family, weight, width, italic);
    return fcl_add_spec(x, &s);
}
static UINT32 fcl_hidden_style(fcl_fx *x, const char *family, int weight, int width, int italic)
{
    ttf_spec s; size_t n; uint8_t *b; UINT32 k;
    ttf_style_spec(&s, family, weight, width, italic);
    b = ttf_build(&s, &n); k = fcl_add_hidden(x, b, n); free(b);
    return k;
}
/* A regular-only family. */
static UINT32 fcl_add_family(fcl_fx *x, const char *family) { return fcl_add_style(x, family, 400, 5, 0); }

/* Unique 8-byte key text. */
static int fcl_uniq_key(char *buf /* >= 16 */)
{
    static volatile LONG ctr;
    LONG n = InterlockedIncrement(&ctr);
    snprintf(buf, 16, "key%05ld", (long)(n % 100000));
    return 8;
}

static HRESULT fcl_create_with(fcl_fx *x, const void *key, UINT32 ks, IDWriteFontCollection **out)
{
    return IDWriteFactory_CreateCustomFontCollection(x->f, (IDWriteFontCollectionLoader *)x->cl, key, ks, out);
}
/* CreateCustomFontCollection with a fresh key. */
static HRESULT fcl_create(fcl_fx *x, IDWriteFontCollection **out)
{
    char key[16]; int n = fcl_uniq_key(key);
    return fcl_create_with(x, key, (UINT32)n, out);
}
/* Creates and skips the test on failure. */
#define FCL_COLL(x, c) IDWriteFontCollection *c = NULL; do { HRESULT hr_ = fcl_create(&x, &c); \
    if (FAILED(hr_) || !c) { t_skip("prerequisite CreateCustomFontCollection failed: %s", dw_hr_name(hr_)); DW_RELEASE(c); fcl_close(&x); return; } } while (0)

/* ---- reading helpers ----------------------------------------------------------------------- */
static int fcl_ls_en(IDWriteLocalizedStrings *s, char *buf, int cap)
{
    buf[0] = 0;
    return s && SUCCEEDED(dw_ls_en(s, buf, cap));
}
/* English family name of family i (empty string on failure). */
static int fcl_fam_name_of(IDWriteFontFamily *fam, char *buf, int cap)
{
    IDWriteLocalizedStrings *s = NULL; int ok = 0;
    buf[0] = 0;
    if (fam && SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) { ok = fcl_ls_en(s, buf, cap); IDWriteLocalizedStrings_Release(s); }
    return ok;
}
static int fcl_name_at(IDWriteFontCollection *c, UINT32 i, char *buf, int cap)
{
    IDWriteFontFamily *fam = NULL; int ok;
    buf[0] = 0;
    if (FAILED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) || !fam) return 0;
    ok = fcl_fam_name_of(fam, buf, cap);
    IDWriteFontFamily_Release(fam);
    return ok;
}
/* Index of a family by name (case-insensitive lookup through the API), -1 when absent. */
static long fcl_find(IDWriteFontCollection *c, const char *name)
{
    WCHAR w[128]; UINT32 idx = 0xdeadbeef; BOOL ex = FALSE;
    dw_wide(name, w, 128);
    if (FAILED(IDWriteFontCollection_FindFamilyName(c, w, &idx, &ex)) || !ex) return -1;
    return (long)idx;
}
static void fcl_note_families(IDWriteFontCollection *c, const char *tag)
{
    UINT32 i, n = IDWriteFontCollection_GetFontFamilyCount(c);
    char line[512], nm[128]; line[0] = 0;
    for (i = 0; i < n && strlen(line) < 380; i++) { fcl_name_at(c, i, nm, sizeof nm); strcat(line, i ? ", " : ""); strcat(line, nm); }
    t_note("%s: %u families: %s", tag, n, line);
}
/* Counts the fonts in family i. */
static UINT32 fcl_font_count(IDWriteFontCollection *c, UINT32 i)
{
    IDWriteFontFamily *fam = NULL; UINT32 n = 0;
    if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, i, &fam)) && fam) { n = IDWriteFontFamily_GetFontCount(fam); IDWriteFontFamily_Release(fam); }
    return n;
}
/* Finds, in family `fi`, a font with exactly the given properties (sim -1 = any); caller releases. */
static IDWriteFont *fcl_pick(IDWriteFontCollection *c, UINT32 fi, int weight, int stretch, int style, int sim)
{
    IDWriteFontFamily *fam = NULL; IDWriteFont *res = NULL; UINT32 n, k;
    if (FAILED(IDWriteFontCollection_GetFontFamily(c, fi, &fam)) || !fam) return NULL;
    n = IDWriteFontFamily_GetFontCount(fam);
    for (k = 0; k < n && !res; k++) {
        IDWriteFont *f = NULL;
        if (FAILED(IDWriteFontFamily_GetFont(fam, k, &f)) || !f) continue;
        if ((int)IDWriteFont_GetWeight(f) == weight && (int)IDWriteFont_GetStretch(f) == stretch && (int)IDWriteFont_GetStyle(f) == style &&
            (sim < 0 || (int)IDWriteFont_GetSimulations(f) == sim)) res = f;
        else IDWriteFont_Release(f);
    }
    IDWriteFontFamily_Release(fam);
    return res;
}
/* Face for the first font of family fi with the given simulations (any when sim<0). */
static IDWriteFontFace *fcl_face_of(IDWriteFontCollection *c, UINT32 fi, int sim)
{
    IDWriteFontFamily *fam = NULL; IDWriteFontFace *face = NULL; UINT32 n, k;
    if (FAILED(IDWriteFontCollection_GetFontFamily(c, fi, &fam)) || !fam) return NULL;
    n = IDWriteFontFamily_GetFontCount(fam);
    for (k = 0; k < n && !face; k++) {
        IDWriteFont *f = NULL;
        if (FAILED(IDWriteFontFamily_GetFont(fam, k, &f)) || !f) continue;
        if (sim < 0 || (int)IDWriteFont_GetSimulations(f) == sim) IDWriteFont_CreateFontFace(f, &face);
        IDWriteFont_Release(f);
    }
    IDWriteFontFamily_Release(fam);
    return face;
}
/* ---- probing collection loader / enumerator doubles (fcl_dbl.c) ------------------------------------------------ */
/* Ordered event log shared by the doubles: 'C' CreateEnumeratorFromKey, 'M' MoveNext, 'G' GetCurrentFontFile,
 * 'E' enumerator reached refcount 0, 'L' collection loader reached refcount 0. */
#define FCL_EVMAX 512
extern char fcl_ev[FCL_EVMAX]; extern int fcl_nev;
void fcl_ev_reset(void);
void fcl_evc(char c);

#define FCL_MAXF 16
typedef struct fcl_en {
    const void *lpVtbl; volatile LONG ref; int destroyed;
    IDWriteFontFile *files[FCL_MAXF]; int nfiles; int pos;      /* pos: -1 before the first MoveNext */
    LONG n_move, n_get, n_addref, n_release, n_qi;
    HRESULT move_hr; int move_hr_at;      /* MoveNext call number (1-based) that returns move_hr (0: none); has=FALSE on that call */
    int move_hr_true;                     /* the failing MoveNext also writes hasCurrent=TRUE */
    HRESULT get_hr; unsigned get_mask;    /* GetCurrentFontFile of file i returns get_hr when bit i is set */
    HRESULT after_end_hr;                 /* result of GetCurrentFontFile outside the sequence (E_FAIL) */
    int get_out_of_sequence;              /* calls made while pos is outside [0,nfiles) */
    int extra_moves_after_end;            /* MoveNext calls after it already returned FALSE */
    int ended;
    struct fcl_cl *owner;
} fcl_en;
typedef struct fcl_cl {
    const void *lpVtbl; volatile LONG ref; int destroyed;
    dwx_mem_loader *fl; UINT32 keys[FCL_MAXF]; int nkeys;       /* blobs enumerated (file-loader keys) */
    LONG n_create, n_qi, n_addref, n_release;
    const void *last_key; UINT32 last_ks; uint8_t keycopy[128]; int key_was_null;
    IDWriteFactory *last_factory; IDWriteFactory *expect_factory; int factory_identity_ok;
    HRESULT create_hr;                    /* fail injection (0 = off) */
    int create_hr_positive;               /* return create_hr but still hand out an enumerator (S_FALSE style) */
    fcl_en *last_en; fcl_en *ens[FCL_MAXF]; volatile LONG nens;
    HRESULT move_hr; int move_hr_at; int move_hr_true; HRESULT get_hr; unsigned get_mask;   /* copied into new enumerators */
    DWORD tid;
    IID qi_iids[8]; int n_qi_iids;
} fcl_cl;
fcl_cl *fcl_cl_new(dwx_mem_loader *fl);
void fcl_cl_add(fcl_cl *c, UINT32 filekey);
HRESULT fcl_cl_register(fcl_cl *c, IDWriteFactory *f);
HRESULT fcl_cl_unregister(fcl_cl *c, IDWriteFactory *f);
ULONG fcl_cl_release(fcl_cl *c);

/* Fixture over the probing doubles (fcl_cl / fcl_en): isolated factory, in-memory file loader (registered), fcl_cl. */
typedef struct fcl_px { IDWriteFactory *f; dwx_mem_loader *fl; fcl_cl *cl; int reg_cl, reg_fl, ok; } fcl_px;
static int fcl_px_open(fcl_px *p, int reg_cl)
{
    memset(p, 0, sizeof *p);
    p->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!p->f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    p->fl = dwx_loader_new();
    p->cl = fcl_cl_new(p->fl);
    p->cl->expect_factory = p->f;
    p->reg_fl = SUCCEEDED(dwx_loader_register(p->fl, p->f));
    if (reg_cl) p->reg_cl = SUCCEEDED(fcl_cl_register(p->cl, p->f));
    p->ok = 1;
    fcl_ev_reset();
    return 1;
}
static void fcl_px_close(fcl_px *p)
{
    if (!p->ok) return;
    if (p->reg_cl) fcl_cl_unregister(p->cl, p->f);
    dwx_loader_unregister(p->fl);
    fcl_cl_release(p->cl);
    dwx_release(p->fl);
    DW_RELEASE(p->f);
    p->ok = 0;
}
static UINT32 fcl_px_add_style(fcl_px *p, const char *family, int weight, int width, int italic)
{
    ttf_spec s; size_t n; uint8_t *b; UINT32 k;
    ttf_style_spec(&s, family, weight, width, italic);
    b = ttf_build(&s, &n); k = dwx_loader_add(p->fl, b, n); free(b);
    fcl_cl_add(p->cl, k);
    return k;
}
static UINT32 fcl_px_add_blob(fcl_px *p, const void *blob, size_t n)
{
    UINT32 k = dwx_loader_add(p->fl, blob, n);
    fcl_cl_add(p->cl, k);
    return k;
}
static HRESULT fcl_px_create_key(fcl_px *p, const void *key, UINT32 ks, IDWriteFontCollection **out)
{
    return IDWriteFactory_CreateCustomFontCollection(p->f, (IDWriteFontCollectionLoader *)p->cl, key, ks, out);
}
static HRESULT fcl_px_create(fcl_px *p, IDWriteFontCollection **out)
{
    char key[16]; int n = fcl_uniq_key(key);
    return fcl_px_create_key(p, key, (UINT32)n, out);
}
#define FCL_PX(p) fcl_px p; if (!fcl_px_open(&p, 1)) return
#endif
