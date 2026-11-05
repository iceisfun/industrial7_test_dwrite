/* GetGlyphImageData / ReleaseGlyphImageData and the application's font file stream.
 *
 * FontFace::GetGlyphImageData (dwrite.dll!0xfb340) calls FontFileReference::ReadFragment (!0x63780) with the entry's absolute file offset and size only when the
 * entry size is non-zero; ReadFragment is a direct IDWriteFontFileStream::ReadFileFragment on the stream the reference holds.  The fragment pointer and the
 * fragment context returned by the stream are handed to the caller unchanged, and DWriteFontFace::ReleaseGlyphImageData (!0xdd490) forwards the context to
 * IDWriteFontFileStream::ReleaseFileFragment (FontFileReference::ReleaseFragment, !0x77760). */
#include "crb.h"
#include <process.h>

typedef struct { LONG reads[16], releases[16]; int n; LONG outstanding, bad; } snap;
static void take(crb *x, snap *s) {
    dwx_mem_loader *l = x->h.ld; int i; memset(s, 0, sizeof *s); s->n = l->nstreams < 16 ? l->nstreams : 16;
    for (i = 0; i < s->n; i++) { s->reads[i] = l->streams[i]->n_read; s->releases[i] = l->streams[i]->n_release; s->bad += l->streams[i]->bad_release_ctx; }
    s->outstanding = dwx_loader_total_outstanding(l);
}
/* index of the (single) stream whose read count grew between a and b, -1 when none / several */
static int grew(const snap *a, const snap *b, LONG *by) {
    int i, hit = -1; *by = 0;
    for (i = 0; i < 16; i++) if (b->reads[i] != a->reads[i]) { if (hit >= 0) return -1; hit = i; *by = b->reads[i] - a->reads[i]; }
    return hit;
}
static void copy_mode(crb *x, int on) { int i; for (i = 0; i < x->h.ld->nstreams; i++) x->h.ld->streams[i]->copy_fragments = on; }
static uint8_t *font(size_t *n) { return crb_cbdt_font(n); }

T(release_one_get_is_one_stream_read_with_entry_offset_and_size) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int k, i; const crb_cbexp *e = crb_cb_entries(&k);
    for (i = 0; i < k; i++) {
        snap a, m, z; DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; LONG by; int si;
        take(&x, &a);
        CHECK_HR_OK(crb_gid(&x, (UINT16)e[i].gid, e[i].ppem, F_PNG, &d, &ctx));
        take(&x, &m); si = grew(&a, &m, &by);
        CHECKF(si >= 0 && by == 1, "entry %d: stream reads grew by %ld on stream %d", i, by, si);
        if (si >= 0) {
            dwx_mem_stream *s = x.h.ld->streams[si];
            CHECKF(s->last_offset == d.uniqueDataId && s->last_size == d.imageDataSize, "entry %d: read (%llu,%llu) want (%u,%u)", i, (unsigned long long)s->last_offset, (unsigned long long)s->last_size, d.uniqueDataId, d.imageDataSize);
            CHECKF(d.imageData == s->data + s->last_offset || x.h.ld->streams[si]->copy_fragments, "entry %d: pointer is not the stream's fragment", i);
        }
        CHECKF(m.outstanding == a.outstanding + 1, "entry %d: outstanding %ld -> %ld", i, a.outstanding, m.outstanding);
        IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
        take(&x, &z);
        CHECKF(z.outstanding == a.outstanding && z.bad == 0, "entry %d: after release outstanding %ld (want %ld) bad contexts %ld", i, z.outstanding, a.outstanding, z.bad);
        { int r = 0, j; for (j = 0; j < z.n; j++) r += (int)(z.releases[j] - a.releases[j]); CHECKF(r == 1, "entry %d: %d ReleaseFileFragment calls", i, r); }
    }
    crb_close(&x);
}
T(release_context_and_pointer_pass_through_unchanged) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL;
    copy_mode(&x, 1);
    CHECK_HR_OK(crb_gid(&x, 2, 32, F_PNG, &d, &ctx));
    /* copy mode: the stream hands out a malloc'd copy and uses that same pointer as the fragment context */
    CHECKF(d.imageData != NULL && ctx == d.imageData, "data %p context %p", d.imageData, ctx);
    { int k, i; const crb_cbexp *e = crb_cb_entries(&k); for (i = 0; i < k; i++) if (e[i].gid == 2 && e[i].ppem == 32) CHECK(d.imageDataSize == e[i].pnglen && !memcmp(d.imageData, e[i].png, e[i].pnglen)); }
    IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    CHECK_HR_OK(crb_gid(&x, 2, 32, F_PNG, &d, &ctx));
    { snap s; take(&x, &s); CHECKF(s.bad == 0 && s.outstanding >= 1, "bad %ld outstanding %ld", s.bad, s.outstanding); }
    IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx);
    { snap s; take(&x, &s); EXPECT_EQ(s.outstanding, 0); EXPECT_EQ(s.bad, 0); }
    crb_close(&x);
}
T(release_empty_results_never_touch_the_stream) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    snap a, z; DWRITE_GLYPH_IMAGE_DATA d; void *ctx = (void *)(ULONG_PTR)1; unsigned g, i;
    take(&x, &a);
    for (g = 0; g < 16; g++) {
        for (i = 0; i < 12; i++) {
            static const struct { UINT32 ppem, fmt; } q[12] = { {1000, F_PNG}, {65, F_PNG}, {0, F_SVG}, {0, F_JPEG}, {0, F_TIFF}, {0, F_PREM}, {16, F_SVG}, {0xFFFFFFFFu, F_PNG}, {0, 0}, {0, 0x1}, {0, 0x3}, {0, 0x1F0} };
            HRESULT hr; ctx = (void *)(ULONG_PTR)1;
            if (g >= 2 && g <= 7 && q[i].fmt == F_PNG && q[i].ppem < 65) continue;
            hr = crb_gid(&x, (UINT16)g, q[i].ppem, q[i].fmt, &d, &ctx);
            /* the DWriteFontFace wrapper (0xdc780) accepts exactly one of SVG/PNG/JPEG/TIFF/PREMULTIPLIED and returns E_INVALIDARG otherwise */
            CHECKF(hr == (i >= 8 && i <= 11 ? E_INVALIDARG_ : S_OK), "glyph %u query %u: %s", g, i, dw_hr_name(hr));
            CHECKF(ctx == NULL, "glyph %u query %u: context %p", g, i, ctx);
        }
    }
    take(&x, &z);
    { int j; LONG tot = 0; for (j = 0; j < 16; j++) tot += z.reads[j] - a.reads[j]; EXPECT_EQ(tot, 0); }
    EXPECT_EQ(z.outstanding, a.outstanding);
    crb_close(&x);
}
T(release_many_outstanding_contexts_release_in_any_order) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int k, i, j; const crb_cbexp *e = crb_cb_entries(&k); void *ctx[16]; snap a, m, z;
    take(&x, &a);
    for (i = 0; i < k; i++) { DWRITE_GLYPH_IMAGE_DATA d; ctx[i] = NULL; CHECK_HR_OK(crb_gid(&x, (UINT16)e[i].gid, e[i].ppem, F_PNG, &d, &ctx[i])); CHECK(ctx[i] != NULL); }
    take(&x, &m); CHECKF(m.outstanding == a.outstanding + k, "outstanding %ld want %ld", m.outstanding, a.outstanding + k);
    for (i = 0; i < k; i++) for (j = i + 1; j < k; j++) CHECKF(ctx[i] != ctx[j], "contexts %d and %d are equal", i, j);
    for (i = 0; i < k; i += 2) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx[i]);
    for (i = k - 1; i >= 0; i--) if (i & 1) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx[i]);
    take(&x, &z); CHECKF(z.outstanding == a.outstanding && z.bad == 0, "outstanding %ld (want %ld) bad %ld", z.outstanding, a.outstanding, z.bad);
    crb_close(&x);
}
T(release_same_entry_twice_gives_two_contexts) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    DWRITE_GLYPH_IMAGE_DATA d1, d2; void *c1 = NULL, *c2 = NULL; snap a, m;
    take(&x, &a);
    CHECK_HR_OK(crb_gid(&x, 3, 24, F_PNG, &d1, &c1)); CHECK_HR_OK(crb_gid(&x, 3, 24, F_PNG, &d2, &c2));
    take(&x, &m); EXPECT_EQ(m.outstanding, a.outstanding + 2);
    CHECK(c1 != c2);
    EXPECT_EQ(d1.uniqueDataId, d2.uniqueDataId);
    CHECK(d1.imageDataSize == d2.imageDataSize && !memcmp(d1.imageData, d2.imageData, d1.imageDataSize));
    IDWriteFontFace4_ReleaseGlyphImageData(x.f4, c2); IDWriteFontFace4_ReleaseGlyphImageData(x.f4, c1);
    take(&x, &m); EXPECT_EQ(m.outstanding, a.outstanding);
    crb_close(&x);
}
T(release_data_stays_valid_while_other_requests_run) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    DWRITE_GLYPH_IMAGE_DATA d0; void *c0 = NULL; uint8_t *keep; unsigned g, p;
    copy_mode(&x, 1);
    CHECK_HR_OK(crb_gid(&x, 2, 16, F_PNG, &d0, &c0)); CHECK(d0.imageData != NULL);
    keep = (uint8_t *)malloc(d0.imageDataSize); memcpy(keep, d0.imageData, d0.imageDataSize);
    for (g = 2; g < 8; g++) for (p = 0; p < 70; p += 9) { DWRITE_GLYPH_IMAGE_DATA d; void *c = NULL; CHECK_HR_OK(crb_gid(&x, (UINT16)g, p, F_PNG, &d, &c)); if (c) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, c); }
    CHECKF(!memcmp(d0.imageData, keep, d0.imageDataSize), "data changed while another request ran");
    IDWriteFontFace4_ReleaseGlyphImageData(x.f4, c0);
    free(keep); crb_close(&x);
}
T(release_svg_documents_use_the_same_protocol) {
    static const char D[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"><rect width=\"9\" height=\"9\"/></svg>"; crb_svgdoc d[1] = { { 3, 5, D, sizeof D - 1 } };
    size_t sl, n; uint8_t *svg = crb_svg(d, 1, &sl), *out; ttf_rawtable raw[1];
    memcpy(raw[0].tag, "SVG ", 5); raw[0].data = svg; raw[0].len = (unsigned)sl; out = crb_blob(raw, 1, 0, &n);
    { crb x; if (crb_open(&x, out, n)) {
        snap a, m, z; DWRITE_GLYPH_IMAGE_DATA im; void *ctx = NULL; LONG by;
        take(&x, &a); CHECK_HR_OK(crb_gid(&x, 4, 0, F_SVG, &im, &ctx)); take(&x, &m);
        { int si = grew(&a, &m, &by); CHECKF(si >= 0 && by == 1, "svg: reads grew by %ld", by);
          if (si >= 0) CHECKF(x.h.ld->streams[si]->last_offset == im.uniqueDataId && x.h.ld->streams[si]->last_size == sizeof D - 1, "svg read args"); }
        EXPECT_EQ(m.outstanding, a.outstanding + 1);
        IDWriteFontFace4_ReleaseGlyphImageData(x.f4, ctx); take(&x, &z); EXPECT_EQ(z.outstanding, a.outstanding);
        crb_close(&x);
    } else crb_close(&x); }
    free(svg);
}
static unsigned __stdcall worker(void *p) {
    struct { crb *x; int iters; int bad; } *w = p; int i;
    for (i = 0; i < w->iters; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *c = NULL; UINT16 g = (UINT16)(2 + (i % 6)); UINT32 ppem = (UINT32)((i * 7) % 33);
        HRESULT hr = IDWriteFontFace4_GetGlyphImageData(w->x->f4, g, ppem, F_PNG, &d, &c);
        if (hr != S_OK) w->bad++;
        if (c) IDWriteFontFace4_ReleaseGlyphImageData(w->x->f4, c);
    }
    return 0;
}
T(release_concurrent_get_and_release_balance) {
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    struct { crb *x; int iters; int bad; } w[4]; HANDLE h[4]; int i; snap a, z;
    take(&x, &a);
    for (i = 0; i < 4; i++) { w[i].x = &x; w[i].iters = 300; w[i].bad = 0; h[i] = (HANDLE)_beginthreadex(NULL, 0, worker, &w[i], 0, NULL); }
    for (i = 0; i < 4; i++) { if (h[i]) { CHECK(WaitForSingleObject(h[i], 20000) == WAIT_OBJECT_0); CloseHandle(h[i]); } CHECK(w[i].bad == 0); }
    take(&x, &z);
    CHECKF(z.outstanding == a.outstanding && z.bad == 0, "outstanding %ld (want %ld) bad %ld", z.outstanding, a.outstanding, z.bad);
    crb_close(&x);
}
T(release_unique_id_equals_stream_offset_for_every_format) {
    /* sanity relation used by caches: reading (uniqueDataId, imageDataSize) from the file yields exactly imageData */
    size_t n; uint8_t *b = font(&n); CRB_NEED(x, b, n);
    int k, i; const crb_cbexp *e = crb_cb_entries(&k);
    for (i = 0; i < k; i++) {
        DWRITE_GLYPH_IMAGE_DATA d; void *c = NULL; CHECK_HR_OK(crb_gid(&x, (UINT16)e[i].gid, e[i].ppem, F_PNG, &d, &c));
        CHECKF(d.uniqueDataId + (size_t)d.imageDataSize <= x.n && !memcmp(x.blob + d.uniqueDataId, d.imageData, d.imageDataSize), "entry %d", i);
        if (c) IDWriteFontFace4_ReleaseGlyphImageData(x.f4, c);
    }
    crb_close(&x);
}
