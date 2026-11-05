/* QueryInterface expectation matrix around the colour/bitmap glyph objects.  Interfaces introduced after Windows 10 19041 (IDWriteFontFace6/7, IDWritePaintReader,
 * IDWriteFontSet4, IDWriteBitmapRenderTarget2/3, IDWriteFactory8) are only recorded with t_note; the structure of the answers (NULL out pointer on failure, one COM identity,
 * balanced reference counts, one failure code for every unknown IID) is checked for every object.
 * IDWriteFontFace::QueryInterface answers IUnknown and IDWriteFontFace..5 with the object itself and falls through to *ppv = NULL and one fixed failure code for
 * everything else. */
#include "crb.h"

typedef struct { const char *name; int ok; HRESULT hr; } qres;
/* Queries every public IID on obj; returns the number of successes. fail_hr = the common failure HRESULT (S_OK when none failed); *mixed = failures with different codes */
static int probe_all(IUnknown *obj, const char *what, qres *res, int *nres, HRESULT *fail_hr, int *mixed) {
    int n, i, ok = 0; const dw_qi_row *rows = dw_iid_table(&n); ULONG base = dw_refcount(obj); IUnknown *self = NULL; *fail_hr = S_OK; *mixed = 0; *nres = n;
    IUnknown_QueryInterface(obj, &IID_IUnknown, (void **)&self);
    for (i = 0; i < n; i++) {
        void *p = (void *)(ULONG_PTR)1; HRESULT hr = IUnknown_QueryInterface(obj, rows[i].iid, &p);
        res[i].name = rows[i].name; res[i].hr = hr; res[i].ok = SUCCEEDED(hr);
        if (SUCCEEDED(hr)) {
            ok++;
            CHECKF(p != NULL && p != (void *)1, "%s: QI %s succeeded with out pointer %p", what, rows[i].name, p);
            if (p && p != (void *)1) {
                IUnknown *id = NULL; IUnknown_QueryInterface((IUnknown *)p, &IID_IUnknown, (void **)&id);
                CHECKF(id == self, "%s: QI %s has a different identity", what, rows[i].name);
                if (id) IUnknown_Release(id);
                IUnknown_Release((IUnknown *)p);
            }
        } else {
            CHECKF(p == NULL, "%s: QI %s failed with out pointer %p", what, rows[i].name, p);
            if (*fail_hr == S_OK) *fail_hr = hr; else if (*fail_hr != hr) *mixed = 1;
        }
    }
    if (self) IUnknown_Release(self);
    CHECKF(dw_refcount(obj) == base, "%s: refcount changed %lu -> %lu", what, base, dw_refcount(obj));
    return ok;
}
static void note_row(const qres *r, int n, const char *name, const char *what) {
    int i; for (i = 0; i < n; i++) if (!strcmp(r[i].name, name)) { t_note("%s: QI %s -> %s", what, name, dw_hr_name(r[i].hr)); return; }
}
static int succeeded(const qres *r, int n, const char *name) { int i; for (i = 0; i < n; i++) if (!strcmp(r[i].name, name)) return r[i].ok; return -1; }

T(qi_face_matrix_on_synthetic_fonts) {
    size_t n; uint8_t *b = crb_cbdt_font(&n); CRB_NEED(x, b, n);
    qres r[128]; int nr, mixed, ok, i; HRESULT fh; static const char *const yes[] = { "IDWriteFontFace", "IDWriteFontFace1", "IDWriteFontFace2", "IDWriteFontFace3", "IDWriteFontFace4", "IDWriteFontFace5" };
    ok = probe_all((IUnknown *)x.h.face, "cbdt face", r, &nr, &fh, &mixed);
    for (i = 0; i < 6; i++) CHECKF(succeeded(r, nr, yes[i]) == 1, "%s must be answered", yes[i]);
    CHECKF(!mixed, "unknown IIDs fail with different codes");
    CHECK(succeeded(r, nr, "IDWriteFactory") == 0 && succeeded(r, nr, "IDWriteFont") == 0 && succeeded(r, nr, "IDWriteTextFormat") == 0);
    t_note("synthetic face: %d of %d public IIDs answered, common failure code %s", ok, nr, dw_hr_name(fh));
    note_row(r, nr, "IDWriteFontFace6", "synthetic face"); note_row(r, nr, "IDWriteFontFace7", "synthetic face"); note_row(r, nr, "IDWritePaintReader", "synthetic face");
    note_row(r, nr, "IDWriteFontFaceReference", "synthetic face"); note_row(r, nr, "IDWriteFontFaceReference1", "synthetic face"); note_row(r, nr, "IDWriteFontResource", "synthetic face");
    crb_close(&x);
}
T(qi_answer_set_does_not_depend_on_the_color_tables) {
    size_t n1, n2, n3, n4; uint8_t *b1 = crb_cbdt_font(&n1), *b2 = crb_blob(NULL, 0, 0, &n2), *b3, *b4; crb a, b, c, d; qres ra[128], rb[128], rc[128], rd[128]; int na, nb, nc2, nd, ma, i; HRESULT fh;
    { static const crb_cbase bs[1] = { { 2, 0, 1 } }; static const crb_clayer ls[1] = { { 64, 0 } }; static const uint32_t pal[1] = { 0xFFFF0000u }; static const unsigned st[1] = { 0 };
      size_t nc, np; uint8_t *colr = crb_colr(0, bs, 1, ls, 1, &nc), *cpal = crb_cpal(0, 1, 1, st, 1, pal, &np); ttf_rawtable raw[2];
      memcpy(raw[0].tag, "COLR", 5); raw[0].data = colr; raw[0].len = (unsigned)nc; memcpy(raw[1].tag, "CPAL", 5); raw[1].data = cpal; raw[1].len = (unsigned)np;
      b3 = crb_blob(raw, 2, 0, &n3); free(colr); free(cpal); }
    { static const char D[] = "<svg xmlns=\"http://www.w3.org/2000/svg\"/>"; crb_svgdoc d1 = { 3, 3, D, sizeof D - 1 }; size_t sl; uint8_t *svg = crb_svg(&d1, 1, &sl); ttf_rawtable raw[1];
      memcpy(raw[0].tag, "SVG ", 5); raw[0].data = svg; raw[0].len = (unsigned)sl; b4 = crb_blob(raw, 1, 0, &n4); free(svg); }
    if (!crb_open(&a, b1, n1)) { crb_close(&a); free(b2); free(b3); free(b4); return; }
    if (!crb_open(&b, b2, n2)) { crb_close(&a); crb_close(&b); free(b3); free(b4); return; }
    if (!crb_open(&c, b3, n3)) { crb_close(&a); crb_close(&b); crb_close(&c); free(b4); return; }
    if (!crb_open(&d, b4, n4)) { crb_close(&a); crb_close(&b); crb_close(&c); crb_close(&d); return; }
    probe_all((IUnknown *)a.h.face, "cbdt", ra, &na, &fh, &ma); probe_all((IUnknown *)b.h.face, "plain", rb, &nb, &fh, &ma); probe_all((IUnknown *)c.h.face, "colr", rc, &nc2, &fh, &ma); probe_all((IUnknown *)d.h.face, "svg", rd, &nd, &fh, &ma);
    for (i = 0; i < na; i++) CHECKF(ra[i].ok == rb[i].ok && rb[i].ok == rc[i].ok && rc[i].ok == rd[i].ok && ra[i].hr == rb[i].hr && rb[i].hr == rc[i].hr && rc[i].hr == rd[i].hr, "IID %s: answers differ between faces (%s %s %s %s)", ra[i].name, dw_hr_name(ra[i].hr), dw_hr_name(rb[i].hr), dw_hr_name(rc[i].hr), dw_hr_name(rd[i].hr));
    crb_close(&a); crb_close(&b); crb_close(&c); crb_close(&d);
}
T(qi_system_faces_match_the_synthetic_answer_set) {
    static const WCHAR *const fam[] = { L"Arial", L"Segoe UI Emoji", L"Segoe UI Symbol", L"Bahnschrift", L"Consolas" }; size_t sn; uint8_t *sb = crb_blob(NULL, 0, 0, &sn); crb s; qres rs[128], rf[128]; int ns, nf, m, i, k; HRESULT fh;
    if (!crb_open(&s, sb, sn)) { crb_close(&s); return; }
    probe_all((IUnknown *)s.h.face, "synthetic", rs, &ns, &fh, &m);
    for (k = 0; k < 5; k++) {
        IDWriteFontFace *face = dw_sysface(dw_shared(), fam[k], DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        if (!face) { t_note("family %d not installed", k); continue; }
        probe_all((IUnknown *)face, "system", rf, &nf, &fh, &m);
        for (i = 0; i < ns && i < nf; i++) CHECKF(rs[i].ok == rf[i].ok, "family %d IID %s: system %d synthetic %d", k, rs[i].name, rf[i].ok, rs[i].ok);
        note_row(rf, nf, "IDWriteFontFace6", "system face"); note_row(rf, nf, "IDWriteFontFace7", "system face"); note_row(rf, nf, "IDWritePaintReader", "system face");
        IDWriteFontFace_Release(face);
    }
    crb_close(&s);
}
T(qi_color_run_enumerator_matrix) {
    IDWriteFactory4 *f4 = crb_f4(); size_t n; uint8_t *b = crb_cbdt_font(&n); crb x;
    if (!f4) { free(b); t_skip("IDWriteFactory4 unsupported"); return; }
    if (!crb_open(&x, b, n)) { crb_close(&x); return; }
    {   /* a run with a PNG glyph (glyph 2, strike 16/32) */
        UINT16 g = 2; float adv = 10; DWRITE_GLYPH_RUN r; IDWriteColorGlyphRunEnumerator1 *e = NULL; D2D1_POINT_2F o = { 0, 0 };
        memset(&r, 0, sizeof r); r.fontFace = x.h.face; r.fontEmSize = 8; r.glyphCount = 1; r.glyphIndices = &g; r.glyphAdvances = &adv;
        { HRESULT hr = IDWriteFactory4_TranslateColorGlyphRun(f4, o, &r, NULL, (DWRITE_GLYPH_IMAGE_FORMATS)(F_TT | F_PNG), DWRITE_MEASURING_MODE_NATURAL, NULL, 0, &e);
          if (hr != S_OK || !e) { t_note("no enumerator available: %s", dw_hr_name(hr)); crb_close(&x); return; } }
        { qres rr[128]; int nr, mixed, i, ok; HRESULT fh; ok = probe_all((IUnknown *)e, "enumerator", rr, &nr, &fh, &mixed);
          CHECK(succeeded(rr, nr, "IDWriteColorGlyphRunEnumerator") == 1); CHECK(succeeded(rr, nr, "IDWriteColorGlyphRunEnumerator1") == 1);
          CHECKF(!mixed, "different failure codes"); EXPECT_EQ(ok, 2);
          for (i = 0; i < nr; i++) if (rr[i].ok && strcmp(rr[i].name, "IDWriteColorGlyphRunEnumerator") && strcmp(rr[i].name, "IDWriteColorGlyphRunEnumerator1")) CHECKF(0, "unexpected interface %s on the enumerator", rr[i].name);
          t_note("enumerator: failure code for other IIDs %s", dw_hr_name(fh)); }
        DW_RELEASE(e);
    }
    crb_close(&x);
}
T(qi_factory_matrix_note) {
    IDWriteFactory *f = dw_shared(); qres r[128]; int nr, mixed; HRESULT fh; int ok, v;
    if (!f) { t_skip("no factory"); return; }
    ok = probe_all((IUnknown *)f, "shared factory", r, &nr, &fh, &mixed);
    CHECK(succeeded(r, nr, "IDWriteFactory") == 1);
    for (v = 0; v <= 7; v++) {
        char nm[32]; snprintf(nm, sizeof nm, v ? "IDWriteFactory%d" : "IDWriteFactory", v);
        t_note("shared factory: %s -> %s", nm, succeeded(r, nr, nm) == 1 ? "S_OK" : "not answered");
    }
    note_row(r, nr, "IDWriteFactory8", "shared factory"); note_row(r, nr, "IDWriteBitmapRenderTarget2", "shared factory");
    t_note("shared factory answers %d of %d IIDs", ok, nr);
}
T(qi_bitmap_render_target_matrix_note) {
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *gi = NULL; IDWriteBitmapRenderTarget *rt = NULL;
    if (!f) { t_skip("no factory"); return; }
    if (FAILED(IDWriteFactory_GetGdiInterop(f, &gi)) || !gi) { t_skip("no GDI interop"); return; }
    if (FAILED(IDWriteGdiInterop_CreateBitmapRenderTarget(gi, NULL, 64, 64, &rt)) || !rt) { DW_RELEASE(gi); t_skip("cannot create a bitmap render target"); return; }
    {
        qres r[128]; int nr, mixed; HRESULT fh; probe_all((IUnknown *)rt, "bitmap render target", r, &nr, &fh, &mixed);
        CHECK(succeeded(r, nr, "IDWriteBitmapRenderTarget") == 1);
        note_row(r, nr, "IDWriteBitmapRenderTarget1", "bitmap render target"); note_row(r, nr, "IDWriteBitmapRenderTarget2", "bitmap render target"); note_row(r, nr, "IDWriteBitmapRenderTarget3", "bitmap render target");
        CHECKF(!mixed, "different failure codes for unknown IIDs");
    }
    DW_RELEASE(rt); DW_RELEASE(gi);
}
T(qi_system_font_set_matrix_note) {
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *fs = NULL;
    if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    if (FAILED(IDWriteFactory3_GetSystemFontSet(f3, &fs)) || !fs) { t_skip("no system font set"); return; }
    { qres r[128]; int nr, mixed; HRESULT fh; probe_all((IUnknown *)fs, "system font set", r, &nr, &fh, &mixed);
      CHECK(succeeded(r, nr, "IDWriteFontSet") == 1);
      note_row(r, nr, "IDWriteFontSet1", "system font set"); note_row(r, nr, "IDWriteFontSet2", "system font set"); note_row(r, nr, "IDWriteFontSet3", "system font set"); note_row(r, nr, "IDWriteFontSet4", "system font set"); }
    DW_RELEASE(fs);
}
T(qi_win11_interfaces_never_hard_required) {
    /* the Windows 11 only IIDs: whatever the OS answers must be a coherent QI result (identity, refcount); no presence is asserted */
    size_t n; uint8_t *b = crb_cbdt_font(&n); CRB_NEED(x, b, n);
    static const struct { const IID *iid; const char *name; } w11[] = { { &DW_IID_IDWriteFontFace7, "IDWriteFontFace7" }, { &DW_IID_IDWritePaintReader, "IDWritePaintReader" } }; int i;
    for (i = 0; i < 2; i++) {
        void *p = (void *)(ULONG_PTR)1; ULONG before = dw_refcount((IUnknown *)x.h.face); HRESULT hr = IDWriteFontFace_QueryInterface(x.h.face, w11[i].iid, &p);
        t_note("%s on a 19041 face: %s", w11[i].name, dw_hr_name(hr));
        if (SUCCEEDED(hr)) { CHECK(p != NULL); EXPECT_EQ(dw_refcount((IUnknown *)x.h.face), before + 1); IUnknown_Release((IUnknown *)p); } else CHECK(p == NULL);
        EXPECT_EQ(dw_refcount((IUnknown *)x.h.face), before);
    }
    {
        IDWriteFactory *f = dw_shared(); void *p = (void *)(ULONG_PTR)1; HRESULT hr = IDWriteFactory_QueryInterface(f, &DW_IID_IDWriteFactory8, &p);
        t_note("IDWriteFactory8 on the shared factory: %s", dw_hr_name(hr)); if (SUCCEEDED(hr)) IUnknown_Release((IUnknown *)p); else CHECK(p == NULL);
    }
    crb_close(&x);
}
T(qi_released_interfaces_keep_the_face_alive) {
    size_t n; uint8_t *b = crb_cbdt_font(&n); CRB_NEED(x, b, n);
    ULONG base = dw_refcount((IUnknown *)x.h.face); IDWriteFontFace4 *a = NULL, *c = NULL;
    CHECK_HR_OK(IDWriteFontFace_QueryInterface(x.h.face, &IID_IDWriteFontFace4, (void **)&a)); CHECK_HR_OK(IDWriteFontFace4_QueryInterface(a, &IID_IDWriteFontFace4, (void **)&c));
    EXPECT_EQ((void *)a, (void *)c); EXPECT_EQ(dw_refcount((IUnknown *)x.h.face), base + 2);
    IDWriteFontFace4_Release(a); EXPECT_EQ(dw_refcount((IUnknown *)x.h.face), base + 1);
    /* the remaining reference still serves image data */
    { DWRITE_GLYPH_IMAGE_DATA d; void *ctx = NULL; CHECK_HR_OK(IDWriteFontFace4_GetGlyphImageData(c, 2, 16, F_PNG, &d, &ctx)); CHECK(d.imageData != NULL); if (ctx) IDWriteFontFace4_ReleaseGlyphImageData(c, ctx); }
    IDWriteFontFace4_Release(c); EXPECT_EQ(dw_refcount((IUnknown *)x.h.face), base);
    crb_close(&x);
}
