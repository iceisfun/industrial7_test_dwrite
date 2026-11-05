/* Shared helpers for the com_lifetime_threads area: object makers, generic COM refcount checks, thread runner. */
#ifndef CLT_H
#define CLT_H
#include "dwx.h"

#define NEED_PROC() do { if (!dw_proc()) { t_skip("DWriteCreateFactory unavailable"); return; } } while (0)

/* ---- context: an isolated factory plus lazily created synthetic font fixtures -------------------- */
typedef struct {
    IDWriteFactory *f;                 /* own isolated factory (released by clt_ctx_done) */
    dwx_font font;                     /* synthetic default font (loader registered on f) */
    int font_open;
    dwx_mem_loader *ld;                /* extra loader for custom collections */
    dwx_coll_loader *cl;
    IDWriteFontCollection *coll;
    dwx_dc dc; int dc_open;
} clt_ctx;
int  clt_ctx_init(clt_ctx *c);                  /* 0 on failure (DWriteCreateFactory missing) */
void clt_ctx_done(clt_ctx *c);
void clt_ctx_abandon(clt_ctx *c);               /* release our refs without calling the (possibly destroyed) factory */
IDWriteFontFace *clt_ctx_face(clt_ctx *c);      /* synthetic face, NULL on failure; owned by ctx */
IDWriteFontFile *clt_ctx_file(clt_ctx *c);
IDWriteFontCollection *clt_ctx_coll(clt_ctx *c);/* custom collection holding one synthetic font; owned by ctx */

/* Isolated factory + file loader with n synthetic fonts (families CltFam0..) + collection loader enumerating them. */
typedef struct { IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; } clt_cfix;
int  clt_cfix_open(clt_cfix *x, int nfonts);     /* 0 on failure; always call clt_cfix_close */
void clt_cfix_close(clt_cfix *x);

/* ---- generic COM checks ------------------------------------------------------------------------------ */
/* QueryInterface every public IID: successes raise the count by exactly one and keep the COM identity,
 * failures return E_NOINTERFACE with a NULL out pointer; the count is balanced afterwards. Returns #successes. */
int clt_qi_balance(IUnknown *o, const char *file, int line);
#define CLT_QI_BALANCE(o) clt_qi_balance((IUnknown *)(o), __FILE__, __LINE__)
/* AddRef/Release arithmetic relative to the current count b. */
void clt_refs_arith(IUnknown *o, const char *file, int line);
#define CLT_REFS_ARITH(o) clt_refs_arith((IUnknown *)(o), __FILE__, __LINE__)

/* ---- makers ------------------------------------------------------------------------------------------- */
typedef struct clt_maker {
    const char *name;
    IUnknown *(*make)(clt_ctx *c);                 /* NULL when unsupported on this OS */
    void (*touch)(IUnknown *o);                    /* a harmless method call that must keep working */
    int exact1;                                    /* a fresh object starts at refcount 1 */
} clt_maker;
const clt_maker *clt_makers(int *n);

/* ---- threads ------------------------------------------------------------------------------------------ */
#define CLT_MAXTH 16
typedef struct clt_thr {
    int idx, nthreads, iters;
    void *arg;                         /* shared argument */
    void *out;                         /* per-thread output slot */
    uint64_t hash;                     /* result digest, must be equal across threads */
    volatile LONG fail;                /* number of failed in-thread assertions */
    int fail_line; char fail_expr[96];
    HANDLE start;
    DWORD com_mode;                    /* 0 none, 1 STA, 2 MTA */
    void *ptr;                         /* pointer to publish (identity checks) */
    HRESULT hr;
} clt_thr;
#define TCHK(t, cond) do { if (!(cond)) { if (InterlockedIncrement(&(t)->fail) == 1) { (t)->fail_line = __LINE__; lstrcpynA((t)->fail_expr, #cond, sizeof (t)->fail_expr); } } } while (0)
typedef void (*clt_thr_fn)(clt_thr *t);
/* Runs n threads with fn behind a start barrier; returns 1 when all finished within the bound. */
int clt_run(int n, int iters, clt_thr_fn fn, void *arg, clt_thr *t, DWORD com_mode);
/* Reports per-thread failures with CHECK and requires equal hashes across threads. */
void clt_verify(const clt_thr *t, int n, const char *what);
static __inline uint64_t clt_h(uint64_t h, const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p; size_t i;
    for (i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}
#define CLT_H0 14695981039346656037ull
static __inline uint64_t clt_hi(uint64_t h, uint64_t v) { return clt_h(h, &v, sizeof v); }
static __inline uint64_t clt_hf(uint64_t h, float v) { unsigned u; memcpy(&u, &v, 4); return clt_hi(h, u); }
static __inline uint64_t clt_hw(uint64_t h, const WCHAR *w) { return clt_h(h, w, lstrlenW(w) * 2); }

/* The per-iteration workload shared by the thread tests (uses factory f, iteration i); returns a digest and counts
 * in-thread assertion failures in t. Operation k = i % CLT_NOPS. */
#define CLT_NOPS 8
uint64_t clt_op(clt_thr *t, IDWriteFactory *f, int op, int i, dwx_mem_loader *ld);
#endif
