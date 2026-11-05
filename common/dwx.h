/* Reusable COM test doubles for objects the application hands INTO dwrite.dll.
 *
 * Every double:
 *   - is a plain C struct whose first member is the COM vtable pointer (cast to the interface type to pass it in);
 *   - starts with refcount 1 (the creator's reference); Release() to zero sets .b.destroyed = 1 and the object stops counting
 *     as live, but its memory is kept until dwx_free(obj) / dwx_free_all() / process exit, so counters stay readable
 *     after dwrite dropped its last reference ("did dwrite release what it AddRef'd?");
 *   - records every call (counters, last arguments) and supports fail injection (an HRESULT returned by the call);
 *   - answers QueryInterface for IUnknown plus its own interfaces; any other IID => E_NOINTERFACE with *ppv = NULL.
 * Use dwx_refs(obj), dwx_destroyed(obj), dwx_qi_calls(obj) on any of them. DWX_LEAK_BEGIN/END check that every double created
 * in between reached refcount 0 (i.e. dwrite released everything it retained).
 */
#ifndef DWX_H
#define DWX_H
#include "dw.h"

typedef struct dwx_base {
    volatile LONG ref;
    int destroyed;                  /* set when ref reached 0 */
    LONG addref_calls, release_calls, qi_calls;
    struct dwx_base *next;          /* registry of all doubles */
    int live_counted;
    void (*dtor)(void *obj);        /* releases owned memory when the object is freed */
    void (*on_zero)(void *obj);     /* runs when the refcount reaches 0 (drops references the double holds) */
    void *self;
} dwx_base;
#define DWX_BASE(o) ((dwx_base *)((void **)(o) + 1))      /* base lives right after the vtable pointer */
static __inline LONG dwx_refs(void *o)       { return DWX_BASE(o)->ref; }
static __inline int  dwx_destroyed(void *o)  { return DWX_BASE(o)->destroyed; }
static __inline LONG dwx_qi_calls(void *o)   { return DWX_BASE(o)->qi_calls; }
ULONG dwx_addref(void *obj);                               /* AddRef/Release by pointer (any double) */
ULONG dwx_release(void *obj);
int  dwx_live(void);                                       /* doubles with refcount > 0 */
void dwx_free(void *o);                                    /* free memory now (object must not be used by dwrite any more) */
void dwx_free_all(void);
#define DWX_LEAK_BEGIN(tag) int tag##_live0_ = dwx_live()
#define DWX_LEAK_END(tag)   CHECKF(dwx_live() == tag##_live0_, "test doubles still referenced after the scope: %d -> %d", tag##_live0_, dwx_live())

/* ===== Font file loader + stream over memory blobs ======================================================== */
typedef struct { uint64_t offset, size; } dwx_read_rec;
typedef struct dwx_mem_stream {
    const void *lpVtbl; dwx_base b;                        /* IDWriteFontFileStream */
    const uint8_t *data; uint64_t size; uint64_t last_write;
    LONG n_read, n_release, n_getsize, n_lastwrite;
    LONG outstanding;                                      /* ReadFileFragment calls minus ReleaseFileFragment calls */
    LONG max_outstanding;
    dwx_read_rec reads[256]; int nlogged;                  /* first 256 ReadFileFragment (offset,size) */
    uint64_t last_offset, last_size;
    HRESULT fail_read, fail_getsize, fail_lastwrite;       /* returned instead of S_OK (0 = off) */
    LONG fail_read_from;                                   /* only reads with index >= this fail (default 0) */
    int copy_fragments;                                    /* ReadFileFragment hands out a malloc'd copy, context = the copy */
    int bad_release_ctx;                                   /* count of ReleaseFileFragment with a context we never issued */
    int lastwrite_notimpl;                                 /* GetLastWriteTime returns E_NOTIMPL */
    struct dwx_mem_loader *loader;
} dwx_mem_stream;
/* Capacity of a loader's blob table and of a collection loader's key list. dwx_loader_add returns 0xFFFFFFFF (nothing
 * stored) once 32 blobs exist; dwx_coll_add_key silently drops keys past 32. Tests needing more must use several loaders. */
#define DWX_MAX_BLOBS 32
typedef struct dwx_mem_loader {
    const void *lpVtbl; dwx_base b;                        /* IDWriteFontFileLoader */
    struct { uint8_t *data; size_t size; } blob[DWX_MAX_BLOBS]; int nblobs;
    LONG n_create; UINT32 last_key_size; uint8_t last_key[64]; UINT32 last_key_value;
    HRESULT fail_create;                                   /* CreateStreamFromKey result when non-zero */
    int fail_key_size_check;                               /* reject key sizes != 4 with E_INVALIDARG (default 1) */
    dwx_mem_stream *streams[128]; int nstreams;            /* every stream handed out */
    dwx_mem_stream *last_stream;
    uint64_t last_write;                                   /* value streams report */
    int stream_copy_fragments;                             /* initial copy_fragments for new streams */
    HRESULT stream_fail_read, stream_fail_getsize;         /* initial fail codes for new streams */
    int registered; IDWriteFactory *reg_factory;
} dwx_mem_loader;
dwx_mem_loader *dwx_loader_new(void);
UINT32 dwx_loader_add(dwx_mem_loader *ld, const void *data, size_t size);          /* copies; returns the key value (4-byte key) */
HRESULT dwx_loader_register(dwx_mem_loader *ld, IDWriteFactory *f);                /* RegisterFontFileLoader (no extra ref taken by us) */
HRESULT dwx_loader_unregister(dwx_mem_loader *ld);                                 /* safe when not registered */
/* IDWriteFontFile for blob `key` (needs the loader registered). */
HRESULT dwx_loader_file(dwx_mem_loader *ld, IDWriteFactory *f, UINT32 key, IDWriteFontFile **out);
LONG dwx_loader_total_reads(dwx_mem_loader *ld);          /* sum of n_read over all streams */
LONG dwx_loader_total_outstanding(dwx_mem_loader *ld);    /* sum of outstanding fragments (0 when balanced) */

/* One-shot font open: loader + file + face over a blob.  Use dwx_font_close() always (even after a failure). */
typedef struct dwx_font {
    dwx_mem_loader *ld; IDWriteFactory *f; IDWriteFontFile *file; IDWriteFontFace *face; UINT32 key;
    BOOL supported; DWRITE_FONT_FILE_TYPE file_type; DWRITE_FONT_FACE_TYPE face_type; UINT32 nfaces;
} dwx_font;
HRESULT dwx_font_open(dwx_font *h, IDWriteFactory *f, const void *blob, size_t size, UINT32 face_index, DWRITE_FONT_SIMULATIONS sim);
HRESULT dwx_font_open_default(dwx_font *h, IDWriteFactory *f);                     /* default synthetic font, see ttfgen.h */
void dwx_font_close(dwx_font *h);                                                  /* releases face/file, unregisters, releases loader */

/* ===== Font collection loader + enumerator ================================================================ */
typedef struct dwx_enum {
    const void *lpVtbl; dwx_base b;                        /* IDWriteFontFileEnumerator */
    dwx_mem_loader *files; IDWriteFactory *factory; UINT32 keys[DWX_MAX_BLOBS]; int nkeys;
    int pos;                                               /* -1 before first MoveNext */
    LONG n_movenext, n_getcurrent;
    HRESULT fail_move, fail_get;                           /* when non-zero returned by MoveNext / GetCurrentFontFile */
    int fail_move_at;                                      /* MoveNext call index (1-based) that fails; 0 = use fail_move on every call */
    int get_without_current_hr_set; HRESULT get_without_current_hr;   /* result of GetCurrentFontFile outside [first,last] (default E_FAIL) */
} dwx_enum;
typedef struct dwx_coll_loader {
    const void *lpVtbl; dwx_base b;                        /* IDWriteFontCollectionLoader */
    dwx_mem_loader *files;                                 /* file loader supplying the fonts (must be registered by the test) */
    UINT32 keys[DWX_MAX_BLOBS]; int nkeys;                 /* blob keys enumerated, in order */
    LONG n_create; UINT32 last_key_size; uint8_t last_key[64]; IDWriteFactory *last_factory;
    HRESULT fail_create;
    dwx_enum *enums[32]; int nenums; dwx_enum *last_enum;
    HRESULT enum_fail_move, enum_fail_get;                 /* initial fail codes for created enumerators */
    int registered; IDWriteFactory *reg_factory;
} dwx_coll_loader;
dwx_coll_loader *dwx_coll_new(dwx_mem_loader *files);       /* takes a reference on `files` */
void dwx_coll_add_key(dwx_coll_loader *c, UINT32 file_key);
HRESULT dwx_coll_register(dwx_coll_loader *c, IDWriteFactory *f);
HRESULT dwx_coll_unregister(dwx_coll_loader *c);
/* CreateCustomFontCollection(f, loader, key, keysize) with the collection key = "dwx" 4 bytes. */
HRESULT dwx_coll_create(dwx_coll_loader *c, IDWriteFactory *f, IDWriteFontCollection **out);

/* ===== Text renderer (+ IDWritePixelSnapping, optional IDWriteTextRenderer1) ============================== */
enum { DWX_DRAW_GLYPHRUN = 1, DWX_DRAW_UNDERLINE, DWX_DRAW_STRIKETHROUGH, DWX_DRAW_INLINE };
typedef struct dwx_draw {
    int kind; int seq;
    void *ctx; float x, y;                                 /* baseline origin / inline origin */
    DWRITE_MEASURING_MODE mm;
    /* glyph run (deep copies) */
    IDWriteFontFace *face; float em; UINT32 nglyphs; UINT16 *indices; float *advances; DWRITE_GLYPH_OFFSET *offsets; BOOL sideways; UINT8 bidi;
    /* run description (deep copies); has_desc = pointer was non-NULL */
    int has_desc; WCHAR *locale; WCHAR *text; UINT32 text_len; UINT16 *cluster_map; UINT32 text_pos;
    /* underline / strikethrough (struct copy; locale copied) */
    DWRITE_UNDERLINE ul; DWRITE_STRIKETHROUGH st; WCHAR *line_locale;
    /* inline object */
    IDWriteInlineObject *obj; BOOL obj_sideways, obj_rtl;
    IUnknown *effect;                                      /* raw pointer as passed (no AddRef) */
    LONG effect_refs;                                      /* effect's refcount observed right at the call if it is a dwx_effect, else -1 */
    int has_orientation; DWRITE_GLYPH_ORIENTATION_ANGLE angle;   /* IDWriteTextRenderer1 entry points */
} dwx_draw;
typedef struct dwx_renderer {
    const void *lpVtbl; dwx_base b;
    dwx_draw *calls; int ncalls, cap; LONG count[5];       /* count[DWX_DRAW_*] */
    LONG n_snapping, n_transform, n_ppd;                   /* pixel snapping callbacks */
    void *last_snap_ctx;
    BOOL snapping_disabled; DWRITE_MATRIX transform; float pixels_per_dip;
    HRESULT fail_draw;                                     /* returned by every Draw* when non-zero */
    int fail_at;                                           /* 1-based draw call that fails with fail_draw (0 = all) */
    HRESULT fail_snapping, fail_transform, fail_ppd;
    int support_r1;                                        /* QI answers IDWriteTextRenderer1 (default 0) */
    int recurse_inline;                                    /* DrawInlineObject calls IDWriteInlineObject_Draw (default 0); applies to both renderer entry points */
    LONG r1_calls;
} dwx_renderer;
dwx_renderer *dwx_renderer_new(void);                       /* snapping enabled=FALSE, identity transform, ppd 1.0 */
void dwx_renderer_clear(dwx_renderer *r);                   /* forget recorded calls */
const dwx_draw *dwx_renderer_call(dwx_renderer *r, int index);
/* Next recorded call of the given kind after position *cursor (cursor advanced); NULL when none. */
const dwx_draw *dwx_renderer_next(dwx_renderer *r, int kind, int *cursor);
float dwx_run_width(const dwx_draw *d);                     /* sum of advances */

/* ===== Inline object ===================================================================================== */
typedef struct dwx_inline {
    const void *lpVtbl; dwx_base b;                        /* IDWriteInlineObject */
    DWRITE_INLINE_OBJECT_METRICS metrics; DWRITE_OVERHANG_METRICS overhang;
    DWRITE_BREAK_CONDITION before, after;
    LONG n_draw, n_metrics, n_overhang, n_break;
    HRESULT fail_draw, fail_metrics, fail_overhang, fail_break;
    void *last_ctx; IDWriteTextRenderer *last_renderer; float last_x, last_y; BOOL last_sideways, last_rtl; IUnknown *last_effect;
} dwx_inline;
dwx_inline *dwx_inline_new(float width, float height, float baseline, BOOL sideways);   /* overhangs 0, break conditions NEUTRAL */

/* ===== Geometry sink (ID2D1SimplifiedGeometrySink == IDWriteGeometrySink) ================================= */
enum { DWX_G_FILL = 1, DWX_G_SEGFLAGS, DWX_G_BEGIN, DWX_G_LINES, DWX_G_BEZIERS, DWX_G_END, DWX_G_CLOSE };
typedef struct { int kind; int count; int arg; D2D1_POINT_2F pt; } dwx_gev;     /* arg: fill mode / flags / figure begin / end */
typedef struct dwx_geom {
    const void *lpVtbl; dwx_base b;
    dwx_gev ev[2048]; int nev;
    D2D1_POINT_2F pts[8192]; int npts;                      /* every point delivered (begin point, line points, 3 per bezier) */
    int fill_mode, fill_calls, seg_flags, close_calls;
    int nfigures_begun, nfigures_ended, nlines, nbeziers, last_end;
    HRESULT fail_close;
    int calls_after_close;                                  /* any call after Close() */
    int open_figure;                                        /* BeginFigure without EndFigure so far */
    int order_violations;                                   /* line/bezier/end outside a figure, begin inside a figure */
} dwx_geom;
dwx_geom *dwx_geom_new(void);
void dwx_geom_bounds(const dwx_geom *g, float *x0, float *y0, float *x1, float *y1);   /* control-point bounds; 0s when no points */

/* ===== Text analysis source / sink ======================================================================== */
typedef struct dwx_ana_src {
    const void *lpVtbl; dwx_base b;                        /* IDWriteTextAnalysisSource (+ IDWriteTextAnalysisSource1 when support_v1) */
    const WCHAR *text; UINT32 len; WCHAR locale[LOCALE_NAME_MAX_LENGTH]; DWRITE_READING_DIRECTION dir;
    IDWriteNumberSubstitution *subst;                       /* returned (AddRef'd) by GetNumberSubstitution; may be NULL */
    UINT32 chunk;                                          /* max characters returned per GetTextAtPosition/BeforePosition (0 = all) */
    UINT32 locale_run;                                     /* GetLocaleName run length (0 = rest of text) */
    UINT32 subst_run;
    DWRITE_VERTICAL_GLYPH_ORIENTATION orientation; UINT8 orient_bidi; UINT32 orient_run; int support_v1;
    LONG n_at, n_before, n_dir, n_locale, n_subst, n_orient;
    UINT32 last_pos; HRESULT fail_locale, fail_subst, fail_orient;
    int log_n; struct { int fn; UINT32 pos; } log[1024];   /* fn: 1 at, 2 before, 3 dir, 4 locale, 5 number substitution, 6 orientation */
} dwx_ana_src;
/* Text is NOT copied (must outlive the object). Locale "en-us", LTR, no substitution. */
dwx_ana_src *dwx_ana_src_new(const WCHAR *text, UINT32 len);

#define DWX_ANA_MAX 4096
typedef struct dwx_ana_sink {
    const void *lpVtbl; dwx_base b;                        /* IDWriteTextAnalysisSink (+ Sink1 when support_v1) */
    int support_v1;
    LONG n_script, n_break, n_bidi, n_subst, n_orient;
    HRESULT fail_script, fail_break, fail_bidi, fail_subst, fail_orient;     /* non-zero => returned (and nothing recorded) */
    DWRITE_SCRIPT_ANALYSIS script[DWX_ANA_MAX]; unsigned char script_set[DWX_ANA_MAX];     /* per text position */
    DWRITE_LINE_BREAKPOINT brk[DWX_ANA_MAX];                unsigned char brk_set[DWX_ANA_MAX];
    UINT8 bidi_explicit[DWX_ANA_MAX], bidi_resolved[DWX_ANA_MAX]; unsigned char bidi_set[DWX_ANA_MAX];
    IDWriteNumberSubstitution *subst[DWX_ANA_MAX];          unsigned char subst_set[DWX_ANA_MAX];
    DWRITE_GLYPH_ORIENTATION_ANGLE angle[DWX_ANA_MAX]; UINT8 orient_bidi[DWX_ANA_MAX]; BOOL orient_sideways[DWX_ANA_MAX], orient_rtl[DWX_ANA_MAX]; unsigned char orient_set[DWX_ANA_MAX];
    int nrun_script, nrun_break, nrun_bidi;                 /* number of callbacks */
    struct { UINT32 pos, len; } script_runs[256], bidi_runs[256], break_runs[256];
    int overflow;                                           /* a callback exceeded DWX_ANA_MAX */
    int overlap;                                            /* a position was reported twice within one callback family */
} dwx_ana_sink;
dwx_ana_sink *dwx_ana_sink_new(void);
/* True when every position in [0,len) was reported exactly once by the given callback family
 * ('s' script, 'b' breakpoints, 'l' bidi levels). */
int dwx_ana_covered(const dwx_ana_sink *s, char family, UINT32 len);

/* ===== Font download listener, counting effect ============================================================= */
typedef struct dwx_listener {
    const void *lpVtbl; dwx_base b;                        /* IDWriteFontDownloadListener */
    LONG n_completed; IDWriteFontDownloadQueue *last_queue; IUnknown *last_context; HRESULT last_hr;
    HANDLE event;                                          /* signalled on DownloadCompleted */
} dwx_listener;
dwx_listener *dwx_listener_new(void);
typedef struct dwx_effect {
    const void *lpVtbl; dwx_base b;                        /* plain IUnknown: a "client drawing effect" */
    int tag;
} dwx_effect;
dwx_effect *dwx_effect_new(int tag);

/* ===== Memory-DC pixel readback (GDI interop / bitmap render target tests) ================================ */
typedef struct dwx_dc {
    HDC dc; HBITMAP bmp, old; int w, h; uint32_t *bits; uint32_t bg;
} dwx_dc;
BOOL dwx_dc_create(dwx_dc *d, int w, int h, uint32_t bg_xrgb);   /* top-down 32bpp DIB selected into a memory DC, filled with bg */
void dwx_dc_destroy(dwx_dc *d);
void dwx_dc_fill(dwx_dc *d, uint32_t xrgb);
uint32_t dwx_dc_pixel(const dwx_dc *d, int x, int y);            /* 0x00RRGGBB */
int dwx_dc_count_not(const dwx_dc *d, uint32_t xrgb);            /* pixels differing from the colour */
BOOL dwx_dc_bbox_not(const dwx_dc *d, uint32_t xrgb, RECT *rc);  /* bounding box (right/bottom exclusive) of differing pixels */
uint64_t dwx_dc_hash(const dwx_dc *d);
#endif
