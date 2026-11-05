/* Local test doubles for the font_file_loader area: an event-logging IDWriteFontFileLoader / IDWriteFontFileStream pair
 * that serves one blob for every key, records the exact order of loader and stream callbacks, and supports fault injection.
 * Everything lives in this area so the shared dwx doubles stay untouched. */
#ifndef FL_H
#define FL_H
#include "dwx.h"
#include <xmmintrin.h>

#define H_FILEFORMAT   ((HRESULT)0x88985000)
#define H_FILENOTFOUND ((HRESULT)0x88985003)
#define H_FILEACCESS   ((HRESULT)0x88985004)
#define H_ALREADYREG   ((HRESULT)0x88985006)
#define H_REMOTEFONT   ((HRESULT)0x8898500D)
#define H_E_NOTIMPL    ((HRESULT)0x80004001)
#define LDR(l) ((IDWriteFontFileLoader *)(l))

enum { FL_CREATE = 1, FL_SIZE, FL_READ, FL_REL, FL_LASTW };
typedef struct { int kind; UINT64 a, b; HRESULT hr; } fl_ev;

typedef struct fl_stream {
    const IDWriteFontFileStreamVtbl *lpVtbl;
    volatile LONG ref; int destroyed; LONG qi, addref, release;
    struct fl_loader *ld;
} fl_stream;

typedef struct fl_loader {
    const IDWriteRemoteFontFileLoaderVtbl *lpVtbl;
    volatile LONG ref; int destroyed; LONG qi;
    uint8_t *data; UINT64 size;                 /* served for every key */
    UINT64 reported_size;                       /* GetFileSize result (default = size) */
    UINT64 last_write; HRESULT lastw_hr;        /* GetLastWriteTime result (default S_OK) */
    /* fault injection */
    HRESULT fail_create, fail_size, fail_read; LONG fail_read_from;
    HRESULT oob_hr;                             /* returned for reads outside [0,size] (default E_FAIL) */
    int copy_frag;                              /* fragments are malloc'd copies (context = the copy) */
    int misalign;                               /* with copy_frag: fragment start = copy + 1 */
    int mxcsr_poke;                             /* callbacks disturb the SSE rounding mode */
    /* recording */
    fl_ev ev[1024]; LONG nev;
    LONG n_create, n_size, n_read, n_rel, n_lastw, outstanding, bad_rel;
    uint8_t last_key[1100]; UINT32 last_key_size; const void *last_key_ptr;
    int keys_log_n; const void *key_ptrs[16];
    fl_stream *streams[16]; int nstreams;
    void *issued[256]; int nissued;
    void (*on_read)(struct fl_loader *, UINT64 off, UINT64 size);
    int registered; IDWriteFactory *reg_factory;
    /* optional IDWriteRemoteFontFileLoader face */
    int remote; DWRITE_LOCALITY loc; HRESULT loc_hr; LONG n_loc; UINT32 loc_key_size; uint8_t loc_key[64];
    LONG n_qi_remote; IID last_qi_iid; LONG n_qi_other;
} fl_loader;

fl_loader *fl_new(const void *data, size_t n);
void fl_set_data(fl_loader *l, const void *data, size_t n);        /* replace the served blob (old one is leaked on purpose) */
void fl_free(fl_loader *l);                                        /* frees memory; only after dwrite dropped everything */
HRESULT fl_register(fl_loader *l, IDWriteFactory *f);
HRESULT fl_unregister(fl_loader *l);                               /* safe when not registered */
HRESULT fl_file(fl_loader *l, IDWriteFactory *f, const void *key, UINT32 ks, IDWriteFontFile **out);
HRESULT fl_file4(fl_loader *l, IDWriteFactory *f, UINT32 key, IDWriteFontFile **out);
ULONG fl_addref(fl_loader *l);
ULONG fl_release(fl_loader *l);
LONG fl_refs(fl_loader *l);
LONG fl_stream_refs(fl_loader *l, int i);
int fl_nev(fl_loader *l, int kind);                                /* number of events of a kind */
void fl_clear(fl_loader *l);                                       /* forget recorded events and counters (not refcounts) */
/* Expect the event log to be exactly the listed (kind,a,b) triples; a/b of -1 are wildcards. Returns 1 on match. */
int fl_log_is_(fl_loader *l, const fl_ev *want, int n, const char *file, int line);
#define FL_LOG_IS(l, ...) do { const fl_ev w_[] = { __VA_ARGS__ }; fl_log_is_(l, w_, (int)(sizeof w_ / sizeof w_[0]), __FILE__, __LINE__); } while (0)
#define EV(k, a, b) { (k), (UINT64)(a), (UINT64)(b), 0 }
#define ANY ((UINT64)-1)

/* Common fixtures */
uint8_t *fx_ttf(size_t *n);                                        /* default synthetic font (fresh malloc each call) */
uint8_t *fx_magic(uint32_t be_magic, size_t total, size_t *n);     /* 4 magic bytes + zeros (header-only fonts for Analyze) */
uint8_t *fx_ttc_hdr(uint32_t count, size_t total, size_t *n);      /* 'ttcf' v1.0 count, zero padded */
uint8_t *fx_pfb(size_t total, size_t *n);                          /* Type 1 PFB-looking blob */
uint8_t *fx_pfm(size_t total, size_t *n);                          /* Type 1 PFM-looking blob */

/* One factory + loader + file */
typedef struct { IDWriteFactory *f; fl_loader *ld; IDWriteFontFile *file; uint8_t *blob; size_t n; HRESULT hr; } rig;
int rig_open(rig *r, const void *blob, size_t n);                  /* isolated factory, registered loader, file for key 0; 0 on failure */
void rig_close(rig *r);                                            /* releases everything in the right order */
IDWriteFactory *fl_factory(void);                                  /* new isolated factory (caller releases) */
IDWriteFactory5 *fl_factory5(IDWriteFactory *f);                   /* QI to IDWriteFactory5 or NULL (caller releases) */

/* Analyze with all outputs, returning hr. */
typedef struct { BOOL sup; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf; HRESULT hr; } an_res;
an_res fl_analyze(IDWriteFontFile *file);
/* Windows font helpers */
int fl_sysfont(const char *leaf, char *out, size_t cap);          /* full path of %WINDIR%\Fonts\leaf if it exists */
int fl_write_tmp(const char *leaf, const void *data, size_t n, char *path, size_t cap);   /* scratch file under t_tmpdir() */
#endif
