/* Callback-contract fixtures for custom font collection loaders.
 *
 * Four hand-written COM objects share one ordered, thread-stamped event log, so the order in which dwrite drives
 * the application's code (collection loader -> enumerator -> font file loader -> font file stream) can be asserted:
 *
 *   r2_cl  IDWriteFontCollectionLoader   scripted: which files its enumerator yields, injected failures, a hook
 *   r2_en  IDWriteFontFileEnumerator     MoveNext / GetCurrentFontFile with per-call fault injection
 *   r2_fl  IDWriteFontFileLoader         blobs by 4-byte key, per-blob stream faults
 *   r2_st  IDWriteFontFileStream         records every read, pairing of ReleaseFileFragment contexts, size lies
 *
 * Objects start with refcount 1 (the creator).  Their memory is never freed (counters stay readable); after the
 * last Release the vtable is swapped for a tripwire vtable, so a call made on a destroyed object is counted
 * (r2_uaf) instead of crashing the process.
 */
#ifndef R2_H
#define R2_H
#include "dwx.h"
#include <xmmintrin.h>

#define RT(n) TEST(r2_collection_loader_contract_##n)
#define R2_E_FILEFORMAT        ((HRESULT)0x88985000)
#define R2_E_ALREADYREGISTERED ((HRESULT)0x88985006)
#define R2_MAXSTEP 64
#define R2_MAXBLOB 64

enum { EV_CL_ADDREF = 1, EV_CL_RELEASE, EV_CL_CREATE, EV_EN_MOVE, EV_EN_GET, EV_EN_DESTROY, EV_FL_CREATE, EV_ST_SIZE,
       EV_ST_LAST, EV_ST_READ, EV_ST_REL, EV_ST_DESTROY, EV_FL_DESTROY, EV_CL_DESTROY };
typedef struct { int kind; DWORD tid; UINT64 a, b; int obj; } r2_ev;
#define R2_LOGMAX 32768
extern r2_ev r2_log[R2_LOGMAX];
extern volatile LONG r2_nlog;
extern volatile LONG r2_uaf;          /* calls received by destroyed objects */
extern volatile LONG r2_overrel;      /* releases below zero */
void r2_log_reset(void);
int  r2_n(void);                      /* number of logged events (clamped) */
int  r2_count(int kind);
int  r2_find(int kind, int from);     /* first index >= from with that kind, -1 */
int  r2_find_obj(int kind, int obj, int from);
int  r2_tids_all(DWORD tid);          /* 1 when every logged event ran on `tid` */
/* compact text of the log, one char per event, for the kinds listed in `only` (NULL = all):
 * a=CL_ADDREF r=CL_RELEASE C=CL_CREATE M=EN_MOVE G=EN_GET D=EN_DESTROY F=FL_CREATE s=ST_SIZE w=ST_LAST R=ST_READ x=ST_REL d=ST_DESTROY */
void r2_trace(char *out, int cap, const char *only);

struct r2_fl; struct r2_cl; struct r2_en;

/* ---------------------------------------------------------------- file stream */
typedef struct r2_st {
    const void *lpVtbl; volatile LONG ref; int id;
    const uint8_t *data; uint64_t size_real, size_report;
    HRESULT fail_size, fail_last, fail_read; LONG fail_read_from;
    LONG n_size, n_last, n_read, n_rel, outstanding, max_out, bad_ctx, reads_beyond, destroyed;
    uint64_t last_off, last_len, max_end;
    int copy; void *open_ctx[256]; int nopen;
    struct { uint64_t off, len; } reads[64]; int nlogged;
    struct r2_fl *fl; int blob;
} r2_st;

/* ---------------------------------------------------------------- file loader */
typedef struct { HRESULT fail_create, fail_size, fail_last, fail_read; LONG fail_read_from; int copy; int lie_size; uint64_t size_override; int has_size_override; } r2_stream_cfg;
typedef struct r2_fl {
    const void *lpVtbl; volatile LONG ref; int id;
    struct { uint8_t *data; size_t size; } blob[R2_MAXBLOB]; int nblobs;
    r2_stream_cfg cfg[R2_MAXBLOB];
    volatile LONG n_create, n_addref, n_release, destroyed;
    UINT32 last_key_size; UINT32 last_key_val; const void *last_key_ptr;
    r2_st *streams[512]; volatile LONG nstreams;
    IDWriteFactory *registered_on;
} r2_fl;
r2_fl *r2_fl_new(void);
UINT32 r2_fl_add(r2_fl *l, const void *data, size_t n);         /* returns the 4-byte key (the blob index) */
HRESULT r2_fl_register(r2_fl *l, IDWriteFactory *f);
void    r2_fl_unregister(r2_fl *l);
int     r2_fl_streams_alive(r2_fl *l);                          /* streams with refcount > 0 */
LONG    r2_fl_outstanding(r2_fl *l);                            /* sum of unreleased fragments */
LONG    r2_fl_bad_ctx(r2_fl *l);
LONG    r2_fl_reads(r2_fl *l);
LONG    r2_fl_reads_beyond(r2_fl *l);

/* ---------------------------------------------------------------- collection loader + enumerator */
enum { R2_STEP_FILE = 1, R2_STEP_GET_FAIL, R2_STEP_GET_FAIL_KEEPOUT };
typedef struct { int type; UINT32 key; HRESULT hr; IDWriteFontFile *prebuilt; } r2_step;
typedef struct r2_en {
    const void *lpVtbl; volatile LONG ref; int id;
    struct r2_cl *owner;
    r2_step steps[R2_MAXSTEP]; int nsteps; IDWriteFontFile *files[R2_MAXSTEP];
    int pos, ended;
    LONG n_move, n_get, moves_after_end, gets_out_of_seq, destroyed;
    HRESULT move_fail_hr; int move_fail_at; BOOL move_fail_has;
    BOOL has_value;                      /* what MoveNext reports for "has a current file" (default TRUE) */
} r2_en;
typedef struct r2_cl {
    const void *lpVtbl; volatile LONG ref; int id;
    r2_fl *fl; IDWriteFactory *file_factory;
    r2_step steps[R2_MAXSTEP]; int nsteps;
    volatile LONG n_create, n_addref, n_release, n_qi, destroyed;
    HRESULT create_hr; LONG create_fail_from;           /* from call number N on (1-based), 0 = never */
    HRESULT move_fail_hr; int move_fail_at; BOOL move_fail_has; BOOL has_value;
    /* observations of the last CreateEnumeratorFromKey call */
    UINT32 last_ks; uint8_t last_key[4200]; const void *last_key_ptr; int last_key_null;
    IDWriteFactory *last_factory; DWORD last_tid; LONG ref_in_create; unsigned csr_in_create;
    struct { DWORD tid; UINT32 ks; uint8_t head[8]; } calls[256];
    r2_en *ens[64]; volatile LONG nens; r2_en *last_en;
    IID qi_iids[8]; int n_qi_iids;
    void (*hook)(struct r2_cl *, IDWriteFactory *, void *arg); void *hook_arg;
    unsigned set_csr;                                    /* when non-zero, the callback changes MXCSR to this and leaves it */
    IDWriteFactory *registered_on;
} r2_cl;
r2_cl *r2_cl_new(r2_fl *fl);                              /* does not take a reference on fl */
void   r2_cl_step_file(r2_cl *c, UINT32 key);
void   r2_cl_step_get_fail(r2_cl *c, UINT32 key, HRESULT hr);
void   r2_cl_step_get_fail_keepout(r2_cl *c, UINT32 key, HRESULT hr);
void   r2_cl_clear_steps(r2_cl *c);
ULONG  r2_obj_addref(void *o);
ULONG  r2_obj_release(void *o);                           /* any of the four objects */
LONG   r2_obj_ref(void *o);

/* ---------------------------------------------------------------- fixture */
typedef struct r2_fx { IDWriteFactory *f; r2_fl *fl; r2_cl *cl; int ok; int reg_fl, reg_cl; } r2_fx;
int  r2_open(r2_fx *x, DWRITE_FACTORY_TYPE type, int reg_fl, int reg_cl);   /* 0 (and t_skip) when no factory */
void r2_close(r2_fx *x);                                                     /* unregister + release everything; safe twice */
#define FX(x) r2_fx x; if (!r2_open(&x, DWRITE_FACTORY_TYPE_ISOLATED, 1, 1)) return
UINT32 r2_add_blob(r2_fx *x, const void *blob, size_t n);                    /* blob + enumerated step */
UINT32 r2_add_hidden(r2_fx *x, const void *blob, size_t n);                  /* blob only */
UINT32 r2_add_style(r2_fx *x, const char *family, int weight, int width, int italic);
UINT32 r2_add_family(r2_fx *x, const char *family);                          /* regular-only synthetic font */
/* unique 8-byte key text so the factory's collection cache never answers */
int  r2_uniq_key(char *buf);
HRESULT r2_create(r2_fx *x, IDWriteFontCollection **out);                    /* fresh unique key */
HRESULT r2_create_key(r2_fx *x, const void *key, UINT32 ks, IDWriteFontCollection **out);
#define CLP(c) ((IDWriteFontCollectionLoader *)(c))
/* family helpers */
int  r2_find_family(IDWriteFontCollection *c, const char *name_utf8);         /* index or -1 */
int  r2_family_name(IDWriteFontCollection *c, UINT32 i, char *buf, int cap);  /* en-us (or first) name, 1 on success */
/* sets MXCSR rounding mode bits (not exception masks) and returns the previous value */
#endif
