/* Shared scaffolding for the rendering_params area (IDWriteRenderingParams .. IDWriteRenderingParams3 and the factory
 * methods that create them).
 *
 * Facts established from the Windows 10 dwrite.dll probing that the tests below rely on:
 *  - every IDWriteFactory*::CreateCustomRenderingParams overload writes *params = NULL first, then rejects with E_INVALIDARG
 *    when NOT (0 < gamma <= 256), enhancedContrast < 0, (grayscaleEnhancedContrast < 0), clearTypeLevel < 0,
 *    pixelGeometry > 2, renderingMode > 6 (> 7 for the DWRITE_RENDERING_MODE1 overload), gridFitMode > 2.
 *    There is no upper bound on the contrast values or on clearTypeLevel.
 *  - all objects are one class (DWriteRenderingParams): QueryInterface answers IDWriteRenderingParams .. 3 and IUnknown with
 *    the same pointer whatever overload created the object; everything else gives E_NOINTERFACE.
 *  - IDWriteRenderingParams::GetRenderingMode reports DWRITE_RENDERING_MODE1 value 7 (NATURAL_SYMMETRIC_DOWNSAMPLED) as 5.
 */
#ifndef RP_H
#define RP_H
#include "dwx.h"
#include <float.h>
#include <limits.h>

#define RP_SENT ((void *)(ULONG_PTR)0x1)

/* The complete state of one rendering-params object. `mode` is the DWRITE_RENDERING_MODE / MODE1 value that goes in. */
typedef struct { float gamma, ec, gec, ct; int pg; int mode; int grid; } rp_vals;

/* What the getters must report for an object created through overload `ver` (0..3) from the inputs `in`. */
typedef struct { float gamma, ec, gec, ct; int pg; int mode; int mode1; int grid; } rp_out;

static __inline int rp_fbits(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }
static __inline float rp_nan(void) { unsigned u = 0x7fc00000u; float f; memcpy(&f, &u, 4); return f; }
static __inline float rp_inf(void) { unsigned u = 0x7f800000u; float f; memcpy(&f, &u, 4); return f; }
static __inline float rp_negzero(void) { unsigned u = 0x80000000u; float f; memcpy(&f, &u, 4); return f; }
static __inline float rp_next_up(float x) { return nextafterf(x, rp_inf()); }
static __inline float rp_next_down(float x) { return nextafterf(x, -rp_inf()); }

#define RP_NEED(ver) do { if (!dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, (ver))) { t_skip("IDWriteFactory%d unsupported", (ver)); return; } } while (0)

static __inline rp_vals rp_base(void) { rp_vals v = { 2.2f, 0.5f, 0.25f, 1.0f, 1, 0, 0 }; return v; }

/* Create through overload `ver` of the factory of type t.  *out is normalised: NULL on failure (and a missing NULL-write is reported). */
static HRESULT rp_make_t(DWRITE_FACTORY_TYPE t, int ver, const rp_vals *v, void **out) {
    HRESULT hr;
    void *f = dw_factory_v(t, ver);
    *out = RP_SENT;
    if (!f) { *out = NULL; return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND); }
    switch (ver) {
    case 0: hr = IDWriteFactory_CreateCustomRenderingParams((IDWriteFactory *)f, v->gamma, v->ec, v->ct, (DWRITE_PIXEL_GEOMETRY)v->pg, (DWRITE_RENDERING_MODE)v->mode, (IDWriteRenderingParams **)out); break;
    case 1: hr = IDWriteFactory1_CreateCustomRenderingParams((IDWriteFactory1 *)f, v->gamma, v->ec, v->gec, v->ct, (DWRITE_PIXEL_GEOMETRY)v->pg, (DWRITE_RENDERING_MODE)v->mode, (IDWriteRenderingParams1 **)out); break;
    case 2: hr = IDWriteFactory2_CreateCustomRenderingParams((IDWriteFactory2 *)f, v->gamma, v->ec, v->gec, v->ct, (DWRITE_PIXEL_GEOMETRY)v->pg, (DWRITE_RENDERING_MODE)v->mode, (DWRITE_GRID_FIT_MODE)v->grid, (IDWriteRenderingParams2 **)out); break;
    default: hr = IDWriteFactory3_CreateCustomRenderingParams((IDWriteFactory3 *)f, v->gamma, v->ec, v->gec, v->ct, (DWRITE_PIXEL_GEOMETRY)v->pg, (DWRITE_RENDERING_MODE1)v->mode, (DWRITE_GRID_FIT_MODE)v->grid, (IDWriteRenderingParams3 **)out); break;
    }
    if (*out == RP_SENT) { CHECKF(0, "out parameter was not written (ver %d)", ver); *out = NULL; }
    return hr;
}
static HRESULT rp_make(int ver, const rp_vals *v, void **out) { return rp_make_t(DWRITE_FACTORY_TYPE_SHARED, ver, v, out); }

/* Expected getter results for an object created through overload `ver`. */
static rp_out rp_expect(int ver, const rp_vals *v) {
    rp_out o;
    o.gamma = v->gamma; o.ec = v->ec; o.ct = v->ct; o.pg = v->pg; o.mode1 = v->mode;
    o.mode = (v->mode == 7) ? 5 : v->mode;
    o.gec = (ver == 0) ? 1.0f : v->gec;
    o.grid = (ver >= 2) ? v->grid : 0;
    return o;
}

/* QueryInterface to IDWriteRenderingParams3 (releases nothing; caller releases the result). */
static IDWriteRenderingParams3 *rp_p3(void *p) {
    void *q = NULL;
    HRESULT hr = IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams3, &q);
    t_check(hr == S_OK && q, __FILE__, __LINE__, "QI IDWriteRenderingParams3", "got %s", dw_hr_name(hr));
    return (IDWriteRenderingParams3 *)q;
}

/* Read everything through all four interface levels; the levels must agree. Fills *o (from the newest level). Returns 0 when a QI failed. */
static int rp_read(void *p, rp_out *o) {
    void *q1 = NULL, *q2 = NULL, *q3 = NULL, *q0 = NULL;
    int ok = 1;
    memset(o, 0, sizeof *o);
    ok &= SUCCEEDED(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams, &q0)) && q0;
    ok &= SUCCEEDED(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams1, &q1)) && q1;
    ok &= SUCCEEDED(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams2, &q2)) && q2;
    ok &= SUCCEEDED(IUnknown_QueryInterface((IUnknown *)p, &IID_IDWriteRenderingParams3, &q3)) && q3;
    t_check(ok, __FILE__, __LINE__, "QI chain on rendering params", NULL);
    if (ok) {
        IDWriteRenderingParams3 *a = q3; IDWriteRenderingParams2 *b = q2; IDWriteRenderingParams1 *c = q1; IDWriteRenderingParams *d = q0;
        o->gamma = IDWriteRenderingParams3_GetGamma(a);
        o->ec = IDWriteRenderingParams3_GetEnhancedContrast(a);
        o->gec = IDWriteRenderingParams3_GetGrayscaleEnhancedContrast(a);
        o->ct = IDWriteRenderingParams3_GetClearTypeLevel(a);
        o->pg = IDWriteRenderingParams3_GetPixelGeometry(a);
        o->mode = IDWriteRenderingParams3_GetRenderingMode(a);
        o->mode1 = IDWriteRenderingParams3_GetRenderingMode1(a);
        o->grid = IDWriteRenderingParams3_GetGridFitMode(a);
        /* every older level reports the same numbers */
        t_check(rp_fbits(IDWriteRenderingParams2_GetGamma(b), o->gamma) && rp_fbits(IDWriteRenderingParams1_GetGamma(c), o->gamma) && rp_fbits(IDWriteRenderingParams_GetGamma(d), o->gamma), __FILE__, __LINE__, "GetGamma agrees across interface levels", NULL);
        t_check(rp_fbits(IDWriteRenderingParams2_GetEnhancedContrast(b), o->ec) && rp_fbits(IDWriteRenderingParams1_GetEnhancedContrast(c), o->ec) && rp_fbits(IDWriteRenderingParams_GetEnhancedContrast(d), o->ec), __FILE__, __LINE__, "GetEnhancedContrast agrees across interface levels", NULL);
        t_check(rp_fbits(IDWriteRenderingParams2_GetGrayscaleEnhancedContrast(b), o->gec) && rp_fbits(IDWriteRenderingParams1_GetGrayscaleEnhancedContrast(c), o->gec), __FILE__, __LINE__, "GetGrayscaleEnhancedContrast agrees across interface levels", NULL);
        t_check(rp_fbits(IDWriteRenderingParams2_GetClearTypeLevel(b), o->ct) && rp_fbits(IDWriteRenderingParams1_GetClearTypeLevel(c), o->ct) && rp_fbits(IDWriteRenderingParams_GetClearTypeLevel(d), o->ct), __FILE__, __LINE__, "GetClearTypeLevel agrees across interface levels", NULL);
        t_check(IDWriteRenderingParams2_GetPixelGeometry(b) == (DWRITE_PIXEL_GEOMETRY)o->pg && IDWriteRenderingParams1_GetPixelGeometry(c) == (DWRITE_PIXEL_GEOMETRY)o->pg && IDWriteRenderingParams_GetPixelGeometry(d) == (DWRITE_PIXEL_GEOMETRY)o->pg, __FILE__, __LINE__, "GetPixelGeometry agrees across interface levels", NULL);
        t_check(IDWriteRenderingParams2_GetRenderingMode(b) == (DWRITE_RENDERING_MODE)o->mode && IDWriteRenderingParams1_GetRenderingMode(c) == (DWRITE_RENDERING_MODE)o->mode && IDWriteRenderingParams_GetRenderingMode(d) == (DWRITE_RENDERING_MODE)o->mode, __FILE__, __LINE__, "GetRenderingMode agrees across interface levels", NULL);
        t_check(IDWriteRenderingParams2_GetGridFitMode(b) == (DWRITE_GRID_FIT_MODE)o->grid, __FILE__, __LINE__, "GetGridFitMode agrees (levels 2 and 3)", NULL);
    }
    DW_RELEASE(q0); DW_RELEASE(q1); DW_RELEASE(q2); DW_RELEASE(q3);
    return ok;
}

static void rp_cmp_(const rp_out *got, const rp_out *want, const char *tag, const char *file, int line) {
#define RPF(fld) t_check(rp_fbits(got->fld, want->fld), file, line, #fld, "%s: got %g want %g", tag, (double)got->fld, (double)want->fld)
#define RPI(fld) t_check(got->fld == want->fld, file, line, #fld, "%s: got %d want %d", tag, got->fld, want->fld)
    RPF(gamma); RPF(ec); RPF(gec); RPF(ct); RPI(pg); RPI(mode); RPI(mode1); RPI(grid);
#undef RPF
#undef RPI
}
#define RP_CMP(got, want, tag) rp_cmp_(&(got), &(want), tag, __FILE__, __LINE__)

/* Create through overload `ver` with the given inputs, require S_OK, read everything back and compare. Releases the object. */
static void rp_roundtrip_(DWRITE_FACTORY_TYPE t, int ver, const rp_vals *v, const char *file, int line) {
    void *p = NULL; rp_out got, want = rp_expect(ver, v);
    HRESULT hr = rp_make_t(t, ver, v, &p);
    char tag[96]; _snprintf(tag, sizeof tag, "v%d(g=%g,ec=%g,gec=%g,ct=%g,pg=%d,m=%d,gf=%d)", ver, (double)v->gamma, (double)v->ec, (double)v->gec, (double)v->ct, v->pg, v->mode, v->grid);
    dw_check_hr(hr, S_OK, file, line, tag);
    if (hr == S_OK && p) {
        t_check(rp_read(p, &got), file, line, "read back", NULL);
        rp_cmp_(&got, &want, tag, file, line);
    }
    DW_RELEASE(p);
}
#define RP_ROUNDTRIP(ver, vp) rp_roundtrip_(DWRITE_FACTORY_TYPE_SHARED, (ver), (vp), __FILE__, __LINE__)

/* The creation attempt must fail with E_INVALIDARG and leave *out NULL. */
static void rp_reject_(int ver, const rp_vals *v, const char *what, const char *file, int line) {
    void *p = NULL;
    HRESULT hr = rp_make(ver, v, &p);
    char tag[96]; _snprintf(tag, sizeof tag, "v%d rejects %s", ver, what);
    dw_check_hr(hr, DW_E_INVALIDARG, file, line, tag);
    t_check(p == NULL, file, line, tag, "out parameter not NULL: %p", p);
    DW_RELEASE(p);
}
#define RP_REJECT(ver, vp, what) rp_reject_((ver), (vp), (what), __FILE__, __LINE__)

/* Read the current values of the default / monitor params object */
static int rp_get_default(DWRITE_FACTORY_TYPE t, void **out) {
    IDWriteFactory *f = dw_factory_v(t, 0);
    *out = RP_SENT;
    if (!f) { *out = NULL; return 0; }
    HRESULT hr = IDWriteFactory_CreateRenderingParams(f, (IDWriteRenderingParams **)out);
    if (*out == RP_SENT) *out = NULL;
    return hr == S_OK && *out;
}
#endif
