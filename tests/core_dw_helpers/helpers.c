/* Self-test of the DirectWrite helper layer (common/dw.*, dwx.*, ttfgen.*).
 * Everything here goes LoadLibrary -> GetProcAddress("DWriteCreateFactory") -> vtable calls. */
#include "dwx.h"

#define NEED_FACTORY() IDWriteFactory *f = dw_shared(); if (!f) { t_skip("DWriteCreateFactory unavailable"); return; }

/* ---- factory acquisition ------------------------------------------------- */
TEST(dwh_export_resolves) {
    REQUIRE(dw_proc());
    /* dwrite.dll has exactly one export; by name and by ordinal 1 it is the same function. */
    FARPROC byord = t_proc_ord("dwrite.dll", 1);
    if (byord) CHECK(byord == (FARPROC)dw_proc()); else t_note("ordinal 1 not resolvable");
}
TEST(dwh_create_shared_and_isolated) {
    REQUIRE(dw_proc());
    void *a = NULL, *b = NULL;
    CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &a));
    CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &b));
    CHECK(a && b);
    if (a && b) CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(dwh_shared_is_cached) {
    NEED_FACTORY();
    CHECK(dw_shared() == f);
    CHECK(dw_isolated() != f);
    CHECK(dw_refcount((IUnknown *)f) >= 1);
}
TEST(dwh_invalid_factory_type_zeroes_out) {
    REQUIRE(dw_proc());
    void *p = (void *)(ULONG_PTR)1;
    /* Out parameter is written before validation; an invalid type must fail and leave NULL. */
    HRESULT hr = dw_proc()((DWRITE_FACTORY_TYPE)7, &IID_IDWriteFactory, (IUnknown **)&p);
    /* Windows: *ppFactory = NULL first; only SHARED(0)/ISOLATED(1) accepted, anything else throws an ArgumentException that
     * the export converts to a failure HRESULT. */
    if (t_is_wine()) { t_note("invalid factory type -> %s", dw_hr_name(hr)); DW_RELEASE(p); return; }
    CHECK(FAILED(hr)); CHECK_OUT_NULL(p);
}
TEST(dwh_factory_versions_probe) {
    REQUIRE(dw_proc());
    int v;
    for (v = 0; v < 8; v++) {
        void *p = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, v);
        t_note("IDWriteFactory%d: %s", v, p ? "supported" : "not supported");
        if (v <= 2) CHECK(p != NULL);   /* Win8.1+ */
    }
}
TEST(dwh_factory_qi_matrix_all_iids) {
    NEED_FACTORY();
    int n = 0, i; const dw_qi_row *rows = dw_iid_table(&n);
    CHECK(n >= 90);
    dw_qi_row r[2] = { DW_QI_ROW(IDWriteFactory, 1), DW_QI_ROW(IDWriteFont, 0) };
    DW_CHECK_QI(f, r);
    for (i = 0; i < n; i++) { void *p = NULL; if (SUCCEEDED(IUnknown_QueryInterface((IUnknown *)f, rows[i].iid, &p)) && p) { t_note("factory also answers %s", rows[i].name); IUnknown_Release((IUnknown *)p); } }
}
TEST(dwh_hr_names) {
    EXPECT_STR(dw_hr_name(S_OK), "S_OK");
    EXPECT_STR(dw_hr_name((HRESULT)0x88985002), "DWRITE_E_NOFONT");
    EXPECT_STR(dw_hr_name((HRESULT)0x88985010), "DWRITE_E_TOOMANYDOWNLOADS");
    EXPECT_STR(dw_hr_name((HRESULT)0x8007007A), "E_NOT_SUFFICIENT_BUFFER");
    EXPECT_STR(dw_hr_name((HRESULT)0x12345678), "0x12345678");
    CHECK_HR(S_OK, S_OK);
    CHECK_HR(E_INVALIDARG, E_INVALIDARG);
}

/* ---- string / file helpers ---------------------------------------------------- */
TEST(dwh_utf_roundtrip) {
    WCHAR w[32]; char n[64];
    CHECK(dw_wide("Caf\xC3\xA9", w, 32) == 5);
    CHECK(w[3] == 0xE9);
    CHECK(dw_narrow(w, n, 64) == 6);
    EXPECT_STR(n, "Caf\xC3\xA9");
}
TEST(dwh_fonts_dir) {
    char d[MAX_PATH]; dw_fonts_dir(d, sizeof d);
    CHECK(strlen(d) > 3); CHECK(d[strlen(d) - 1] == '\\');
    const char *none[] = { "no_such_font_xyz.ttf", NULL }; char out[MAX_PATH];
    CHECK(!dw_find_system_font(none, out, sizeof out));
}
TEST(dwh_system_font_probe) {
    NEED_FACTORY();
    BOOL a = dw_have_font(f, L"Arial");
    t_note("Arial present: %d", a);
    CHECK(!dw_have_font(f, L"Definitely Not A Font 12345"));
    if (a) {
        IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
        CHECK(face != NULL);
        if (face) { UINT32 len = 0; uint8_t *head = dw_face_table(face, DW_TAG('h','e','a','d'), &len); CHECK(head && len == 54); free(head); IDWriteFontFace_Release(face); }
    }
}

/* ---- synthetic font generator ---------------------------------------------------- */
static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static unsigned be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
TEST(dwh_ttf_default_structure) {
    size_t n; uint8_t *b = ttf_build_default(&n); unsigned l; int i;
    CHECK(b != NULL && n > 1000);
    if (!b) return;
    EXPECT_EQ(be32(b), 0x00010000);
    unsigned nt = be16(b + 4);
    CHECK(nt == 10);                          /* head hhea maxp OS/2 hmtx cmap loca glyf name post */
    for (i = 1; i < (int)nt; i++) CHECK(memcmp(b + 12 + 16 * (i - 1), b + 12 + 16 * i, 4) < 0);   /* directory sorted by tag */
    const uint8_t *head = ttf_find_table(b, n, "head", &l);
    CHECK(head && l == 54);
    if (head) { EXPECT_EQ(be32(head + 12), 0x5F0F3CF5); EXPECT_EQ(be16(head + 18), 2048); }
    /* whole-file checksum identity */
    { uint32_t s = 0; size_t k; for (k = 0; k + 4 <= n; k += 4) s += be32(b + k); EXPECT_EQ(s, 0xB1B0AFBAu); }
    const uint8_t *maxp = ttf_find_table(b, n, "maxp", &l);
    CHECK(maxp && be16(maxp + 4) == TTF_DEF_NGLYPHS);
    const uint8_t *hhea = ttf_find_table(b, n, "hhea", &l);
    if (hhea) { EXPECT_EQ((short)be16(hhea + 4), 1700); EXPECT_EQ((short)be16(hhea + 6), -400); EXPECT_EQ(be16(hhea + 34), TTF_DEF_NGLYPHS); }
    const uint8_t *os2 = ttf_find_table(b, n, "OS/2", &l);
    CHECK(os2 && l == 96);
    free(b);
}
TEST(dwh_ttf_style_names) {
    struct { int w, wd, it; const char *id1, *id2, *id16, *id17, *full; } t[] = {
        { 400, 5, 0, "Fam", "Regular", NULL, NULL, "Fam" }, { 700, 5, 0, "Fam", "Bold", NULL, NULL, "Fam Bold" },
        { 400, 5, 1, "Fam", "Italic", NULL, NULL, "Fam Italic" }, { 700, 5, 1, "Fam", "Bold Italic", NULL, NULL, "Fam Bold Italic" },
        { 300, 5, 0, "Fam Light", "Regular", "Fam", "Light", "Fam Light" }, { 300, 5, 1, "Fam Light", "Italic", "Fam", "Light Italic", "Fam Light Italic" },
        { 900, 5, 0, "Fam Black", "Regular", "Fam", "Black", "Fam Black" }, { 400, 3, 0, "Fam Condensed", "Regular", "Fam", "Condensed", "Fam Condensed" },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        ttf_spec s; ttf_style_spec(&s, "Fam", t[i].w, t[i].wd, t[i].it);
        const char *id[20] = { 0 }; int k;
        for (k = 0; k < s.nnames; k++) if (s.names[k].plat == 3) id[s.names[k].id] = s.names[k].text;
        CHECKF(id[1] && !strcmp(id[1], t[i].id1), "case %zu id1 '%s'", i, id[1] ? id[1] : "-");
        CHECKF(id[2] && !strcmp(id[2], t[i].id2), "case %zu id2 '%s'", i, id[2] ? id[2] : "-");
        CHECKF(id[4] && !strcmp(id[4], t[i].full), "case %zu id4 '%s'", i, id[4] ? id[4] : "-");
        if (t[i].id16) { CHECK(id[16] && !strcmp(id[16], t[i].id16)); CHECK(id[17] && !strcmp(id[17], t[i].id17)); } else CHECK(!id[16]);
        CHECK(s.weight == t[i].w);
    }
}
TEST(dwh_ttf_cmap_formats) {
    static const ttf_map m[] = { { 0x41, 5 }, { 0x42, 6 }, { 0x43, 7 }, { 0x100, 9 }, { 0x1F600, 11 } };
    static const int fmts[] = { 0, 4, 6, 12, 13 };
    for (int i = 0; i < 5; i++) {
        ttf_spec s; ttf_default_spec(&s);
        ttf_cmap c; memset(&c, 0, sizeof c); c.format = fmts[i]; c.plat = 3; c.enc = fmts[i] >= 12 ? 10 : 1; c.n = fmts[i] == 0 ? 3 : (fmts[i] == 12 || fmts[i] == 13) ? 5 : 4; c.map = m;
        if (fmts[i] == 4 || fmts[i] == 6) c.n = 4;
        s.cmaps = &c; s.ncmaps = 1;
        size_t n; uint8_t *b = ttf_build(&s, &n); unsigned l;
        const uint8_t *cm = b ? ttf_find_table(b, n, "cmap", &l) : NULL;
        CHECKF(cm != NULL, "format %d", fmts[i]);
        if (cm) { const uint8_t *sub = cm + be32(cm + 8); EXPECT_EQ(be16(sub), (unsigned)fmts[i]); }
        free(b);
    }
}
TEST(dwh_ttf_features_build) {
    static const ttf_sub1 sg[] = { { 2, 64 } }; static const ttf_sub4 li[] = { { { 28, 29 }, 2, 65 } }; static const ttf_sub3 al[] = { { 2, 2, { 64, 65 } } };
    static const ttf_feature fe[] = { { "smcp", TTF_LK_SINGLE, 1, sg }, { "liga", TTF_LK_LIGATURE, 1, li }, { "salt", TTF_LK_ALTERNATE, 1, al } };
    static const ttf_kernpair kp[] = { { 2, 3, -100 } };
    ttf_spec s; ttf_default_spec(&s); s.gsub = fe; s.ngsub = 3; s.gpos_kern = kp; s.ngpos_kern = 1; s.kern = kp; s.nkern = 1; s.vertical = 1; s.has_base = 1; s.long_loca = 1; s.compress_hmtx = 1;
    size_t n; uint8_t *b = ttf_build(&s, &n); unsigned l;
    CHECK(b != NULL);
    if (!b) return;
    CHECK(ttf_find_table(b, n, "GSUB", &l) && l > 20); CHECK(ttf_find_table(b, n, "GPOS", &l) != NULL); CHECK(ttf_find_table(b, n, "kern", &l) != NULL);
    CHECK(ttf_find_table(b, n, "vhea", &l) != NULL); CHECK(ttf_find_table(b, n, "vmtx", &l) != NULL); CHECK(ttf_find_table(b, n, "BASE", &l) != NULL);
    const uint8_t *head = ttf_find_table(b, n, "head", &l); CHECK(head && be16(head + 50) == 1);
    free(b);
}
TEST(dwh_ttf_containers) {
    size_t n, wn, tn; uint8_t *b = ttf_build_default(&n), *b2 = ttf_build_styled("Other", 700, 5, 1, &tn);
    const uint8_t *blobs[2] = { b, b2 }; size_t sizes[2] = { n, tn };
    uint8_t *w = ttf_wrap_woff(b, n, &wn); size_t cn; uint8_t *c = ttf_build_ttc(blobs, sizes, 2, &cn);
    EXPECT_EQ(be32(w), 0x774F4646); EXPECT_EQ(be32(w + 8), wn);
    EXPECT_EQ(be32(c), 0x74746366); EXPECT_EQ(be32(c + 8), 2);
    unsigned l; CHECK(ttf_find_table(c, cn, "glyf", &l) != NULL);
    free(b); free(b2); free(w); free(c);
}
TEST(dwh_ttf_corruptions) {
    size_t n, cn; uint8_t *b = ttf_build_default(&n);
    for (int kind = 0; kind <= TTF_CORRUPT_ZERO_FILL; kind++) {
        uint8_t *c = ttf_corrupt(b, n, kind, kind == TTF_CORRUPT_TRUNCATE ? 100 : kind == TTF_CORRUPT_NUMTABLES ? 30 : 2, &cn);
        CHECKF(c != NULL && cn > 0, "kind %d", kind);
        if (kind != TTF_CORRUPT_TRUNCATE && kind != TTF_CORRUPT_ZERO_FILL && kind != TTF_CORRUPT_HEAD_MAGIC) CHECK(memcmp(c, b, n) != 0);
        free(c);
    }
    free(b);
}

/* ---- loader / stream round trip ---------------------------------------------------- */
TEST(dwh_loader_stream_roundtrip) {
    NEED_FACTORY();
    DWX_LEAK_BEGIN(a);
    dwx_font h; HRESULT hr = dwx_font_open_default(&h, f);
    CHECK_HR_OK(hr);
    if (SUCCEEDED(hr)) {
        CHECK(h.supported); CHECK(h.face_type == DWRITE_FONT_FACE_TYPE_TRUETYPE); CHECK(h.file_type == DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(h.nfaces, 1);
        CHECK(h.ld->n_create >= 1); CHECK(h.ld->last_key_size == 4); CHECK(h.ld->nstreams >= 1);
        CHECK(h.ld->last_stream && h.ld->last_stream->n_read >= 1);
        CHECK(h.face != NULL);
        if (h.face) {
            EXPECT_EQ(IDWriteFontFace_GetGlyphCount(h.face), TTF_DEF_NGLYPHS);
            DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(h.face, &m); EXPECT_EQ(m.designUnitsPerEm, 2048);
            UINT32 cps[4] = { 'A', 'z', '5', '!' }; UINT16 g[4];
            CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(h.face, cps, 4, g));
            EXPECT_EQ(g[0], 2); EXPECT_EQ(g[1], 53); EXPECT_EQ(g[2], 59); EXPECT_EQ(g[3], 0);
            DWRITE_GLYPH_METRICS gm[2]; UINT16 ids[2] = { 0, 2 };
            CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(h.face, ids, 2, gm, FALSE));
            EXPECT_EQ(gm[0].advanceWidth, TTF_NOTDEF_ADV); EXPECT_EQ(gm[1].advanceWidth, TTF_DEF_ADV);
            UINT32 len = 0; uint8_t *head = dw_face_table(h.face, DW_TAG('h','e','a','d'), &len);
            CHECK(head && len == 54); free(head);
        }
    }
    dwx_font_close(&h);
    CHECK(h.ld == NULL);
    DWX_LEAK_END(a);
}
TEST(dwh_stream_balance_after_close) {
    NEED_FACTORY();
    dwx_font h; dwx_font_open_default(&h, f);
    dwx_mem_loader *ld = h.ld;
    dwx_font_close(&h);
    /* Whether dwrite has returned every fragment once the file and face are released is a cache-lifetime detail: report it. */
    t_note("outstanding fragments after close: %ld (reads %ld)", (long)dwx_loader_total_outstanding(ld), (long)dwx_loader_total_reads(ld));
    CHECK(dwx_loader_total_reads(ld) >= 1); EXPECT_EQ(ld->streams[0]->bad_release_ctx, 0);
}
TEST(dwh_stream_failure_injection) {
    NEED_FACTORY();
    size_t n; uint8_t *blob = ttf_build_default(&n);
    dwx_mem_loader *ld = dwx_loader_new(); UINT32 key = dwx_loader_add(ld, blob, n); free(blob);
    CHECK_HR_OK(dwx_loader_register(ld, f));
    ld->fail_create = (HRESULT)0x88985003;   /* DWRITE_E_FILENOTFOUND */
    IDWriteFontFile *file = NULL; BOOL sup = TRUE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf;
    HRESULT hr = dwx_loader_file(ld, f, key, &file);
    if (SUCCEEDED(hr) && file) {
        hr = IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf);
        t_note("Analyze with failing loader: %s supported=%d", dw_hr_name(hr), sup);
        CHECK(FAILED(hr) || !sup);
        IDWriteFontFile_Release(file);
    } else t_note("CreateCustomFontFileReference with failing loader: %s", dw_hr_name(hr));
    CHECK_HR_OK(dwx_loader_unregister(ld));
    dwx_release(ld);
}
TEST(dwh_double_qi_matrices) {
    dwx_mem_loader *ld = dwx_loader_new();
    dw_qi_row r1[] = { DW_QI_ROW(IDWriteFontFileLoader, 1), DW_QI_ROW(IDWriteFontFileStream, 0), DW_QI_ROW(IDWriteFactory, 0) };
    DW_CHECK_QI(ld, r1);
    dwx_coll_loader *cl = dwx_coll_new(ld);
    dw_qi_row r2[] = { DW_QI_ROW(IDWriteFontCollectionLoader, 1), DW_QI_ROW(IDWriteFontFileLoader, 0) };
    DW_CHECK_QI(cl, r2);
    dwx_renderer *rd = dwx_renderer_new();
    dw_qi_row r3[] = { DW_QI_ROW(IDWriteTextRenderer, 1), DW_QI_ROW(IDWritePixelSnapping, 1), DW_QI_ROW(IDWriteTextRenderer1, 0) };
    DW_CHECK_QI(rd, r3);
    rd->support_r1 = 1;
    dw_qi_row r3b[] = { DW_QI_ROW(IDWriteTextRenderer1, 1) };
    DW_CHECK_QI(rd, r3b);
    dwx_geom *g = dwx_geom_new();
    dw_qi_row r4[] = { DW_QI_ROW(ID2D1SimplifiedGeometrySink, 1), DW_QI_ROW(IDWriteTextRenderer, 0) };
    DW_CHECK_QI(g, r4);
    dwx_ana_src *as = dwx_ana_src_new(L"x", 1); dwx_ana_sink *sk = dwx_ana_sink_new();
    dw_qi_row r5[] = { DW_QI_ROW(IDWriteTextAnalysisSource, 1), DW_QI_ROW(IDWriteTextAnalysisSource1, 0), DW_QI_ROW(IDWriteTextAnalysisSink, 0) };
    DW_CHECK_QI(as, r5);
    dw_qi_row r6[] = { DW_QI_ROW(IDWriteTextAnalysisSink, 1), DW_QI_ROW(IDWriteTextAnalysisSink1, 0) };
    DW_CHECK_QI(sk, r6);
    dwx_inline *io = dwx_inline_new(10, 10, 8, FALSE);
    dw_qi_row r7[] = { DW_QI_ROW(IDWriteInlineObject, 1), DW_QI_ROW(IDWriteTextRenderer, 0) };
    DW_CHECK_QI(io, r7);
    dwx_listener *dl = dwx_listener_new();
    dw_qi_row r8[] = { DW_QI_ROW(IDWriteFontDownloadListener, 1), DW_QI_ROW(IDWriteInlineObject, 0) };
    DW_CHECK_QI(dl, r8);
    dwx_effect *fx = dwx_effect_new(1);
    dw_qi_row r9[] = { DW_QI_ROW(IDWriteInlineObject, 0) };
    DW_CHECK_QI(fx, r9);
    DWX_LEAK_BEGIN(q);
    dwx_release(cl); dwx_release(ld); dwx_release(rd); dwx_release(g); dwx_release(as); dwx_release(sk); dwx_release(io); dwx_release(dl); dwx_release(fx);
    CHECK(dwx_destroyed(ld) && dwx_destroyed(fx) && dwx_destroyed(cl));
    CHECK(dwx_live() == q_live0_ - 9);
}
TEST(dwh_double_refcount_semantics) {
    dwx_effect *fx = dwx_effect_new(7);
    EXPECT_EQ(dwx_refs(fx), 1);
    IUnknown *u = (IUnknown *)fx;
    /* note: EXPECT_EQ evaluates its arguments twice, so side-effecting calls go through a temporary */
    ULONG r = IUnknown_AddRef(u); EXPECT_EQ(r, 2);
    r = dw_refcount(u); EXPECT_EQ(r, 2);        /* the AddRef/Release pair inside is not counted by `r` */
    r = IUnknown_Release(u); EXPECT_EQ(r, 1);
    CHECK(!dwx_destroyed(fx));
    r = IUnknown_Release(u); EXPECT_EQ(r, 0);
    CHECK(dwx_destroyed(fx)); EXPECT_EQ(fx->b.addref_calls, 2); EXPECT_EQ(fx->b.release_calls, 2 + 1);
    dwx_free(fx);
}

/* ---- custom collection + text layout drawing into the recording renderer ----------------------- */
static void setup_collection(IDWriteFactory *f, dwx_mem_loader **fl, dwx_coll_loader **cl, IDWriteFontCollection **coll, const char *fam) {
    ttf_spec s; ttf_style_spec(&s, fam, 400, 5, 0); size_t n; uint8_t *b = ttf_build(&s, &n);
    *fl = dwx_loader_new(); UINT32 k = dwx_loader_add(*fl, b, n); free(b);
    dwx_loader_register(*fl, f);
    *cl = dwx_coll_new(*fl); dwx_coll_add_key(*cl, k);
    dwx_coll_register(*cl, f);
    *coll = NULL;
    HRESULT hr = dwx_coll_create(*cl, f, coll);
    if (FAILED(hr)) t_note("CreateCustomFontCollection: %s", dw_hr_name(hr));
}
static void teardown_collection(dwx_mem_loader *fl, dwx_coll_loader *cl, IDWriteFontCollection *coll) {
    DW_RELEASE(coll);
    dwx_coll_unregister(cl); dwx_loader_unregister(fl);
    dwx_release(cl); dwx_release(fl);
}
TEST(dwh_custom_collection_enumeration) {
    NEED_FACTORY();
    dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll;
    setup_collection(f, &fl, &cl, &coll, "DwTest");
    CHECK(coll != NULL);
    if (coll) {
        EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(coll), 1);
        UINT32 idx; BOOL ex;
        CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(coll, L"DwTest", &idx, &ex)); CHECK(ex); EXPECT_EQ(idx, 0);
        CHECK(cl->n_create >= 1); CHECK(cl->last_enum != NULL);
        if (cl->last_enum) { CHECK(cl->last_enum->n_movenext >= 2); CHECK(cl->last_enum->n_getcurrent >= 1); }
    }
    teardown_collection(fl, cl, coll);
}
TEST(dwh_layout_draw_records_glyphs) {
    NEED_FACTORY();
    dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll;
    setup_collection(f, &fl, &cl, &coll, "DwTest");
    if (!coll) { teardown_collection(fl, cl, coll); t_skip("no custom collection"); return; }
    IDWriteTextFormat *fmt = NULL; IDWriteTextLayout *lay = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"DwTest", coll, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 20.0f, L"en-us", &fmt));
    if (fmt) CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"AB", 2, fmt, 1000.0f, 100.0f, &lay));
    if (lay) {
        dwx_renderer *rd = dwx_renderer_new(); dwx_effect *fx = dwx_effect_new(3);
        DWRITE_TEXT_RANGE all = { 0, 2 };
        CHECK_HR_OK(IDWriteTextLayout_SetDrawingEffect(lay, (IUnknown *)fx, all));
        CHECK_HR_OK(IDWriteTextLayout_Draw(lay, (void *)(ULONG_PTR)0xC0FFEE, (IDWriteTextRenderer *)rd, 10.0f, 20.0f));
        CHECK(rd->count[DWX_DRAW_GLYPHRUN] >= 1);
        int cur = 0; const dwx_draw *d = dwx_renderer_next(rd, DWX_DRAW_GLYPHRUN, &cur);
        if (d) {
            CHECK(d->ctx == (void *)(ULONG_PTR)0xC0FFEE); CHECK(d->nglyphs == 2); CHECK(d->effect == (IUnknown *)fx);
            if (d->nglyphs == 2 && d->indices) { EXPECT_EQ(d->indices[0], 2); EXPECT_EQ(d->indices[1], 3); }
            if (d->advances) { CHECK_NEAR(d->advances[0], 20.0f * 1000 / 2048, 0.001); CHECK_NEAR(dwx_run_width(d), 2 * 20.0f * 1000 / 2048, 0.002); }
            CHECK_NEAR(d->em, 20.0f, 0.0001); CHECK(d->has_desc); if (d->has_desc) { EXPECT_EQ(d->text_len, 2); CHECK(d->text && d->text[0] == L'A'); }
            CHECK_NEAR(d->x, 10.0f, 0.5);
        }
        EXPECT_EQ(rd->b.ref, 1);
        dwx_release(rd); dwx_release(fx);
        DW_RELEASE(lay);
    }
    DW_RELEASE(fmt);
    teardown_collection(fl, cl, coll);
}

/* ---- analysis doubles --------------------------------------------------------------------------- */
TEST(dwh_analyzer_callbacks) {
    NEED_FACTORY();
    IDWriteTextAnalyzer *an = NULL; CHECK_HR_OK(IDWriteFactory_CreateTextAnalyzer(f, &an));
    if (!an) return;
    static const WCHAR text[] = L"Hello world";
    dwx_ana_src *src = dwx_ana_src_new(text, 11); dwx_ana_sink *sk = dwx_ana_sink_new();
    CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeScript(an, (IDWriteTextAnalysisSource *)src, 0, 11, (IDWriteTextAnalysisSink *)sk));
    CHECK(sk->n_script >= 1); CHECK(dwx_ana_covered(sk, 's', 11)); CHECK(src->n_at >= 1);
    CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeLineBreakpoints(an, (IDWriteTextAnalysisSource *)src, 0, 11, (IDWriteTextAnalysisSink *)sk));
    CHECK(dwx_ana_covered(sk, 'b', 11));
    CHECK(sk->brk[5].isWhitespace == 1); CHECK(sk->brk[0].isWhitespace == 0);
    CHECK_HR_OK(IDWriteTextAnalyzer_AnalyzeBidi(an, (IDWriteTextAnalysisSource *)src, 0, 11, (IDWriteTextAnalysisSink *)sk));
    CHECK(dwx_ana_covered(sk, 'l', 11)); CHECK(sk->bidi_resolved[0] == 0);
    CHECK(src->n_dir >= 1);
    dwx_release(src); dwx_release(sk); IDWriteTextAnalyzer_Release(an);
}

/* ---- geometry sink --------------------------------------------------------------------------------- */
TEST(dwh_outline_into_geometry_sink) {
    NEED_FACTORY();
    dwx_font h; if (FAILED(dwx_font_open_default(&h, f)) || !h.face) { dwx_font_close(&h); t_skip("no synthetic face"); return; }
    dwx_geom *g = dwx_geom_new(); UINT16 gid = 2; FLOAT adv = 0;
    CHECK_HR_OK(IDWriteFontFace_GetGlyphRunOutline(h.face, 2048.0f, &gid, &adv, NULL, 1, FALSE, FALSE, (IDWriteGeometrySink *)g));
    EXPECT_EQ(g->nfigures_begun, 1); EXPECT_EQ(g->nfigures_ended, 1); EXPECT_EQ(g->order_violations, 0);
    float x0, y0, x1, y1; dwx_geom_bounds(g, &x0, &y0, &x1, &y1);
    CHECK_NEAR(x0, 100, 0.01); CHECK_NEAR(x1, 900, 0.01);
    CHECK_NEAR(fabsf(y1 - y0), 1400, 0.01);
    t_note("box outline: fill=%d lines=%d npts=%d close_calls=%d y=[%g,%g]", g->fill_mode, g->nlines, g->npts, g->close_calls, y0, y1);
    dwx_release(g); dwx_font_close(&h);
}

/* ---- remaining doubles ------------------------------------------------------------------------------ */
TEST(dwh_inline_object_double) {
    dwx_inline *io = dwx_inline_new(12.0f, 14.0f, 10.0f, TRUE);
    IDWriteInlineObject *o = (IDWriteInlineObject *)io; DWRITE_INLINE_OBJECT_METRICS m; DWRITE_OVERHANG_METRICS ov; DWRITE_BREAK_CONDITION b, a;
    CHECK_HR_OK(IDWriteInlineObject_GetMetrics(o, &m)); CHECK_FLOAT_EQ(m.width, 12.0f); CHECK(m.supportsSideways);
    CHECK_HR_OK(IDWriteInlineObject_GetOverhangMetrics(o, &ov)); CHECK_FLOAT_EQ(ov.left, 0);
    CHECK_HR_OK(IDWriteInlineObject_GetBreakConditions(o, &b, &a)); EXPECT_EQ(b, DWRITE_BREAK_CONDITION_NEUTRAL);
    io->fail_metrics = E_FAIL; CHECK_HR(IDWriteInlineObject_GetMetrics(o, &m), E_FAIL);
    EXPECT_EQ(io->n_metrics, 2); EXPECT_EQ(io->n_overhang, 1);
    dwx_release(io);
}
TEST(dwh_listener_double) {
    dwx_listener *l = dwx_listener_new();
    IDWriteFontDownloadListener_DownloadCompleted((IDWriteFontDownloadListener *)l, NULL, NULL, S_OK);
    EXPECT_EQ(l->n_completed, 1);
    CHECK(WaitForSingleObject(l->event, 1000) == WAIT_OBJECT_0);
    dwx_release(l);
}
TEST(dwh_memory_dc_readback) {
    dwx_dc d;
    if (!dwx_dc_create(&d, 16, 8, 0xFFFFFF)) { t_skip("no memory DC"); return; }
    EXPECT_EQ(dwx_dc_pixel(&d, 3, 3), 0xFFFFFF); EXPECT_EQ(dwx_dc_count_not(&d, 0xFFFFFF), 0);
    SetPixel(d.dc, 4, 2, RGB(0, 0, 0)); GdiFlush();
    EXPECT_EQ(dwx_dc_pixel(&d, 4, 2), 0); EXPECT_EQ(dwx_dc_count_not(&d, 0xFFFFFF), 1);
    RECT rc; CHECK(dwx_dc_bbox_not(&d, 0xFFFFFF, &rc)); EXPECT_EQ(rc.left, 4); EXPECT_EQ(rc.bottom, 3);
    dwx_dc_destroy(&d);
}
TEST(dwh_interface_table_is_complete) {
    int n; const dw_qi_row *r = dw_iid_table(&n);
    CHECK(n >= 94);
    int found = 0; for (int i = 0; i < n; i++) if (!strcmp(r[i].name, "IDWriteFactory7") || !strcmp(r[i].name, "IDWriteFontDownloadListener")) found++;
    EXPECT_EQ(found, 2);
}
