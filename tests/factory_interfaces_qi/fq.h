/* factory_interfaces_qi: shared table-driven machinery.
 *
 * Every COM class dwrite.dll hands out is described by one fq_class: how to obtain an instance and which interfaces its
 * QueryInterface accepts on Windows 10 build 19041. Test files instantiate the same checks for every class through the
 * FQ_CLASSES X-macro, so the matrix is "creator x interface list".
 */
#ifndef FQ_H
#define FQ_H
#include "dw.h"
#include "dwx.h"

/* Resources a creator keeps alive for the object's lifetime (released in reverse order by fq_ctx_free). */
typedef struct fq_ctx {
    IUnknown *hold[10]; int nhold;
    dwx_font font; int font_open;
    IDWriteFactory *own_factory;     /* optional private factory (released at the end) */
    dwx_mem_loader *ld; dwx_coll_loader *cl;   /* custom collection loaders, kept registered for the object's lifetime */
} fq_ctx;
void fq_ctx_init(fq_ctx *c);
void fq_ctx_free(fq_ctx *c);
void fq_hold(fq_ctx *c, void *unk);   /* takes ownership of one reference */

typedef struct fq_class {
    const char *name;
    IUnknown *(*create)(fq_ctx *c);   /* returns one owned reference or NULL (prerequisite missing -> test skips) */
    /* Interfaces accepted by QueryInterface (besides IUnknown). A leading '?' means "not established": noted, never checked. */
    const char *const *ifaces;
    HRESULT noiface;                  /* HRESULT returned for an unsupported IID: E_NOINTERFACE (E_NOTIMPL for DWriteFontFace) */
    int fresh;                        /* 1: every creator call returns a new object whose first refcount is exactly 1 */
    int distinct;                     /* 1: two creator calls must give distinct objects */
} fq_class;

#define FQ_CLASSES(X) \
    X(factory_shared) X(factory_isolated) X(text_format) X(text_format_axes) X(text_layout) X(text_layout_gdi) \
    X(font_collection_system) X(font_collection_custom) X(font_collection_fontset) X(font_collection_eudc) \
    X(font_family) X(font_list) X(font) X(font_face) X(font_face_custom) X(font_file) X(font_face_reference) X(font_resource) \
    X(localized_strings) X(rendering_params) X(rendering_params_custom) X(rendering_params_monitor) \
    X(typography) X(number_substitution) X(gdi_interop) X(font_set_builder) X(font_set_builder1) X(font_set_system) \
    X(font_set_built) X(glyph_run_analysis) X(bitmap_render_target) X(text_analyzer) X(font_fallback_system) \
    X(font_fallback_custom) X(font_fallback_builder) X(inline_object) X(font_download_queue) X(in_memory_loader) \
    X(color_glyph_run_enumerator)

#define X(n) extern const fq_class fq_cls_##n;
FQ_CLASSES(X)
#undef X

/* Expectation of `iid_name` for class: 1 supported, 0 must fail, -1 not established. */
int fq_expect(const fq_class *cl, const char *iid_name);
const char *fq_first_iface(const fq_class *cl);            /* name of the first listed (non '?') interface */
/* Obtain the instance as a pointer to interface `name` (QueryInterface), NULL on failure. Caller releases. */
void *fq_qi_by_name(IUnknown *obj, const char *name);
const IID *fq_iid_by_name(const char *name);
int fq_slots_of(const char *name);                         /* vtable length from the SDK header (0 = unknown) */

/* ---- dwrite.dll image helpers (vtable placement) ---- */
HMODULE fq_dwrite(void);
int fq_in_image(const void *p);
int fq_is_exec(const void *p);       /* inside the image and on an executable page */
int fq_is_readonly_data(const void *p);   /* inside the image, readable, not writable, not executable */
const char *fq_section_of(const void *p);

/* ---- checks shared by test files ---- */
void fq_check_row(IUnknown *obj, const char *iid_name, const IID *iid, int expect, HRESULT noiface, const char *file, int line);
void fq_matrix(const fq_class *cl);
void fq_matrix_from_each(const fq_class *cl);
void fq_identity(const fq_class *cl);
void fq_refcount(const fq_class *cl);
void fq_vtables(const fq_class *cl);
void fq_fresh(const fq_class *cl);
#endif
