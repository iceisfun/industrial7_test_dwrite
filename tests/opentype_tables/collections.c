/* TrueType collections ('ttcf'): face counts through IDWriteFontFile::Analyze, per-face index, per-face table directories.
 * Windows: for DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION the face index selects the offset table found at 12 + 4*index; an index at or
 * beyond numFonts throws (argument out of range) and CreateFontFace fails; a non-collection face with a non-zero index fails too. */
#include "ot.h"

typedef struct ttcset { dwx_font h; uint8_t *blob; size_t n; int nfonts; } ttcset;

static uint8_t *build_member(int which, size_t *n) {
    ttf_spec sp;
    switch (which) {
    case 0: ttf_style_spec(&sp, "TtcFam", 400, 5, 0); break;
    case 1: ttf_style_spec(&sp, "TtcFam", 700, 5, 0); break;
    case 2: ttf_style_spec(&sp, "TtcFam", 400, 5, 1); break;
    case 3: ttf_style_spec(&sp, "TtcOther", 300, 5, 0); sp.upm = 1000; break;
    default: ttf_style_spec(&sp, "TtcOther", 900, 5, 0); sp.upm = 4096; break;
    }
    return ttf_build(&sp, n);
}
/* Build a collection of `count` members (0..4) and open face `index` of it. */
static int ttc_open(ttcset *t, int count, UINT32 index, HRESULT *hr_out) {
    const uint8_t *blobs[5]; size_t sizes[5]; uint8_t *own[5]; int i; HRESULT hr; IDWriteFactory *f = dw_shared();
    memset(t, 0, sizeof *t);
    if (!f) return 0;
    for (i = 0; i < count; i++) { own[i] = build_member(i, &sizes[i]); blobs[i] = own[i]; }
    t->blob = ttf_build_ttc(blobs, sizes, count, &t->n); t->nfonts = count;
    for (i = 0; i < count; i++) free(own[i]);
    hr = dwx_font_open(&t->h, f, t->blob, t->n, index, DWRITE_FONT_SIMULATIONS_NONE);
    if (hr_out) *hr_out = hr;
    return hr == S_OK && t->h.face;
}
static void ttc_close(ttcset *t) { dwx_font_close(&t->h); free(t->blob); t->blob = NULL; }

TEST(tc_synth_analyze_reports_collection) {
    ttcset t; HRESULT hr;
    if (!ttc_open(&t, 3, 0, &hr)) { t_skip("open %s", dw_hr_name(hr)); ttc_close(&t); return; }
    CHECK(t.h.supported);
    EXPECT_EQ(t.h.file_type, DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION);
    EXPECT_EQ(t.h.face_type, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
    EXPECT_EQ(t.h.nfaces, 3);
    ttc_close(&t);
}
TEST(tc_synth_header_layout) {
    ttcset t; HRESULT hr;
    if (!ttc_open(&t, 3, 0, &hr)) { t_skip("open"); ttc_close(&t); return; }
    EXPECT_EQ(BE32(t.blob), 0x74746366u); EXPECT_EQ(BE32(t.blob + 4), 0x00010000u); EXPECT_EQ(BE32(t.blob + 8), 3);
    EXPECT_EQ(ot_ttc_count(t.blob, t.n), 3);
    ttc_close(&t);
}
static void synth_face_checks(int count, UINT32 idx) {
    ttcset t; HRESULT hr; ot_dir d; ot_font f; int n = 0, bad;
    if (!ttc_open(&t, count, idx, &hr)) { t_skip("open %s", dw_hr_name(hr)); ttc_close(&t); return; }
    EXPECT_EQ(IDWriteFontFace_GetIndex(t.h.face), idx);
    EXPECT_EQ(IDWriteFontFace_GetType(t.h.face), DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
    {
        UINT32 nf = 0; CHECK_HR_OK(IDWriteFontFace_GetFiles(t.h.face, &nf, NULL)); EXPECT_EQ(nf, 1);
    }
    CHECK(ot_parse_dir(t.blob, t.n, idx, &d) == 0);
    bad = compare_all_tables(t.h.face, t.blob, t.n, &d, &n);
    EXPECT_EQ(bad, 0); CHECK(n >= 9);
    if (ot_font_init(&f, t.blob, t.n, &d) == 0) {
        EXPECT_EQ(IDWriteFontFace_GetGlyphCount(t.h.face), f.nglyphs);
        { DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(t.h.face, &m); EXPECT_EQ(m.designUnitsPerEm, f.upm); }
    }
    {
        /* pointers handed back lie inside the one shared stream at the directory offset of this member */
        dwx_mem_stream *st = t.h.ld->last_stream; const ot_rec *r = ot_find(&d, TG("head")); tbl h = tget(t.h.face, TG("head"));
        if (st && r) CHECK(h.d == (const void *)(st->data + r->off));
        trel(t.h.face, &h);
    }
    ttc_close(&t);
}
#define SYNTH_FACE_TEST(c, i) TEST(tc_synth_count##c##_face##i) { synth_face_checks(c, i); }
SYNTH_FACE_TEST(1, 0) SYNTH_FACE_TEST(2, 0) SYNTH_FACE_TEST(2, 1) SYNTH_FACE_TEST(3, 0) SYNTH_FACE_TEST(3, 1) SYNTH_FACE_TEST(3, 2)
SYNTH_FACE_TEST(5, 0) SYNTH_FACE_TEST(5, 1) SYNTH_FACE_TEST(5, 2) SYNTH_FACE_TEST(5, 3) SYNTH_FACE_TEST(5, 4)

TEST(tc_synth_analyze_count_one_member) {
    ttcset t; HRESULT hr;
    if (!ttc_open(&t, 1, 0, &hr)) { t_skip("open"); ttc_close(&t); return; }
    EXPECT_EQ(t.h.nfaces, 1); EXPECT_EQ(t.h.face_type, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
    ttc_close(&t);
}
TEST(tc_synth_analyze_count_five_members) {
    ttcset t; HRESULT hr;
    if (!ttc_open(&t, 5, 0, &hr)) { t_skip("open"); ttc_close(&t); return; }
    EXPECT_EQ(t.h.nfaces, 5);
    ttc_close(&t);
}
TEST(tc_synth_plain_font_reports_one_face) {
    dwx_font h; HRESULT hr = dwx_font_open_default(&h, dw_shared());
    if (hr == S_OK) { EXPECT_EQ(h.nfaces, 1); EXPECT_EQ(h.file_type, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(h.face_type, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(IDWriteFontFace_GetIndex(h.face), 0); EXPECT_EQ(IDWriteFontFace_GetType(h.face), DWRITE_FONT_FACE_TYPE_TRUETYPE); }
    else t_skip("open %s", dw_hr_name(hr));
    dwx_font_close(&h);
}
TEST(tc_synth_members_have_distinct_head_and_os2) {
    ttcset t; HRESULT hr; IDWriteFontFace *face[3] = { 0 }; int i, ok = 1;
    if (!ttc_open(&t, 3, 0, &hr)) { t_skip("open"); ttc_close(&t); return; }
    face[0] = t.h.face;
    for (i = 1; i < 3; i++) if (FAILED(IDWriteFactory_CreateFontFace(t.h.f, t.h.face_type, 1, &t.h.file, i, DWRITE_FONT_SIMULATIONS_NONE, &face[i])) || !face[i]) ok = 0;
    if (ok) {
        UINT32 n; uint8_t *h0 = dw_face_table(face[0], TG("head"), &n), *h1 = dw_face_table(face[1], TG("head"), &n), *h2 = dw_face_table(face[2], TG("head"), &n);
        uint8_t *o0 = dw_face_table(face[0], TG("OS/2"), &n), *o1 = dw_face_table(face[1], TG("OS/2"), &n), *o2 = dw_face_table(face[2], TG("OS/2"), &n);
        CHECK(h0 && h1 && h2 && o0 && o1 && o2);
        if (h0 && h1 && h2) { EXPECT_EQ(BE16(h0 + 44), 0); EXPECT_EQ(BE16(h1 + 44), 1); EXPECT_EQ(BE16(h2 + 44), 2); }       /* macStyle regular/bold/italic */
        if (o0 && o1 && o2) { EXPECT_EQ(BE16(o0 + 4), 400); EXPECT_EQ(BE16(o1 + 4), 700); EXPECT_EQ(BE16(o2 + 4), 400); }   /* usWeightClass */
        free(h0); free(h1); free(h2); free(o0); free(o1); free(o2);
    } else CHECK(0);
    for (i = 1; i < 3; i++) DW_RELEASE(face[i]);
    ttc_close(&t);
}
TEST(tc_synth_members_have_distinct_names) {
    ttcset t; HRESULT hr; IDWriteFontFace *face[3] = { 0 }; int i, ok = 1;
    if (!ttc_open(&t, 3, 0, &hr)) { t_skip("open"); ttc_close(&t); return; }
    face[0] = t.h.face;
    for (i = 1; i < 3; i++) if (FAILED(IDWriteFactory_CreateFontFace(t.h.f, t.h.face_type, 1, &t.h.file, i, DWRITE_FONT_SIMULATIONS_NONE, &face[i])) || !face[i]) ok = 0;
    if (ok) {
        UINT32 n0, n1, n2; uint8_t *a = dw_face_table(face[0], TG("name"), &n0), *b = dw_face_table(face[1], TG("name"), &n1), *c = dw_face_table(face[2], TG("name"), &n2);
        CHECK(a && b && c);
        if (a && b && c) { CHECK(n0 != n1 || memcmp(a, b, n0)); CHECK(n0 != n2 || memcmp(a, c, n0)); CHECK(n1 != n2 || memcmp(b, c, n1)); }
        free(a); free(b); free(c);
    } else CHECK(0);
    for (i = 1; i < 3; i++) DW_RELEASE(face[i]);
    ttc_close(&t);
}
TEST(tc_synth_members_design_units_differ) {
    ttcset t; HRESULT hr; IDWriteFontFace *face[5] = { 0 }; int i, ok = 1; static const int upm[5] = { 2048, 2048, 2048, 1000, 4096 };
    if (!ttc_open(&t, 5, 0, &hr)) { t_skip("open"); ttc_close(&t); return; }
    face[0] = t.h.face;
    for (i = 1; i < 5; i++) if (FAILED(IDWriteFactory_CreateFontFace(t.h.f, t.h.face_type, 1, &t.h.file, i, DWRITE_FONT_SIMULATIONS_NONE, &face[i])) || !face[i]) ok = 0;
    if (ok) for (i = 0; i < 5; i++) { DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face[i], &m); CHECKF(m.designUnitsPerEm == upm[i], "member %d upm %u", i, m.designUnitsPerEm); }
    else CHECK(0);
    for (i = 1; i < 5; i++) DW_RELEASE(face[i]);
    ttc_close(&t);
}
TEST(tc_synth_interleaved_table_use_across_faces) {
    ttcset t; HRESULT hr; IDWriteFontFace *face[3] = { 0 }; int i, ok = 1; ot_dir d[3];
    if (!ttc_open(&t, 3, 0, &hr)) { t_skip("open"); ttc_close(&t); return; }
    face[0] = t.h.face;
    for (i = 1; i < 3; i++) if (FAILED(IDWriteFactory_CreateFontFace(t.h.f, t.h.face_type, 1, &t.h.file, i, DWRITE_FONT_SIMULATIONS_NONE, &face[i])) || !face[i]) ok = 0;
    for (i = 0; i < 3; i++) ot_parse_dir(t.blob, t.n, i, &d[i]);
    if (ok) {
        tbl x[3]; for (i = 0; i < 3; i++) x[i] = tget(face[i], TG("cmap"));
        for (i = 0; i < 3; i++) { const ot_rec *r = ot_find(&d[i], TG("cmap")); CHECK(x[i].ex && x[i].n == r->len && !memcmp(x[i].d, t.blob + r->off, r->len)); }
        for (i = 2; i >= 0; i--) trel(face[i], &x[i]);
        EXPECT_EQ(dwx_loader_total_outstanding(t.h.ld) >= 0, 1);
    } else CHECK(0);
    for (i = 1; i < 3; i++) DW_RELEASE(face[i]);
    ttc_close(&t);
}
TEST(tc_synth_glyph_indices_per_member) {
    ttcset t; HRESULT hr; UINT32 cps[3] = { 'A', 'a', '0' }; UINT16 g[3];
    if (!ttc_open(&t, 3, 2, &hr)) { t_skip("open"); ttc_close(&t); return; }
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(t.h.face, cps, 3, g));
    EXPECT_EQ(g[0], 2); EXPECT_EQ(g[1], 28); EXPECT_EQ(g[2], 54);
    ttc_close(&t);
}
static void out_of_range(int count, UINT32 idx) {
    IDWriteFactory *f = dw_shared(); ttcset t; HRESULT hr; IDWriteFontFace *face = (IDWriteFontFace *)(ULONG_PTR)0;
    if (!ttc_open(&t, count, 0, &hr)) { t_skip("open"); ttc_close(&t); return; }
    face = NULL;
    hr = IDWriteFactory_CreateFontFace(f, t.h.face_type, 1, &t.h.file, idx, DWRITE_FONT_SIMULATIONS_NONE, &face);
    /* A face index past the last member makes the directory lookup throw: creation fails and *fontFace is NULL. */
    CHECKF(FAILED(hr), "index %u of %d members: %s", idx, count, dw_hr_name(hr));
    CHECK(face == NULL);
    t_note("CreateFontFace(index %u of %d) -> %s", idx, count, dw_hr_name(hr));
    DW_RELEASE(face);
    ttc_close(&t);
}
TEST(tc_synth_index_equal_to_count_fails) { out_of_range(3, 3); }
TEST(tc_synth_index_far_beyond_count_fails) { out_of_range(3, 1000); }
TEST(tc_synth_index_max_fails) { out_of_range(2, 0xFFFFFFFFu); }
TEST(tc_synth_index_one_of_one_fails) { out_of_range(1, 1); }
TEST(tc_plain_font_with_nonzero_index_fails) {
    IDWriteFactory *f = dw_shared(); synth s; IDWriteFontFace *face = NULL; HRESULT hr;
    memset(&s, 0, sizeof s);
    if (!synth_open(&s, NULL)) { t_skip("open"); synth_close(&s); return; }
    hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &s.h.file, 1, DWRITE_FONT_SIMULATIONS_NONE, &face);
    CHECKF(FAILED(hr), "faceIndex 1 of a plain sfnt: %s", dw_hr_name(hr));
    CHECK(face == NULL);
    DW_RELEASE(face); synth_close(&s);
}

/* ---- installed collections ---- */
static uint8_t *read_all(const char *path, size_t *len) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); LARGE_INTEGER sz; uint8_t *b = NULL; DWORD got = 0;
    if (h == INVALID_HANDLE_VALUE) return NULL;
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < 256 * 1024 * 1024 && (b = (uint8_t *)malloc((size_t)sz.QuadPart))) {
        if (!ReadFile(h, b, (DWORD)sz.QuadPart, &got, NULL) || got != sz.QuadPart) { free(b); b = NULL; } else *len = (size_t)sz.QuadPart;
    }
    CloseHandle(h);
    return b;
}
typedef struct { char path[MAX_PATH]; uint8_t *file; size_t len; IDWriteFontFile *ff; BOOL sup; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fct; UINT32 nf; int hdr_count; } systtc;
static int systtc_open(systtc *t, const char *leaf) {
    IDWriteFactory *f = dw_shared(); WCHAR w[MAX_PATH]; const char *cand[2]; HRESULT hr;
    memset(t, 0, sizeof *t);
    if (!f) { t_skip("no factory"); return 0; }
    cand[0] = leaf; cand[1] = NULL;
    if (!dw_find_system_font(cand, t->path, sizeof t->path)) { t_skip("%s not installed", leaf); return 0; }
    t->file = read_all(t->path, &t->len);
    if (!t->file) { t_skip("%s unreadable", leaf); return 0; }
    t->hdr_count = ot_ttc_count(t->file, t->len);
    dw_wide(t->path, w, MAX_PATH);
    hr = IDWriteFactory_CreateFontFileReference(f, w, NULL, &t->ff);
    if (FAILED(hr) || !t->ff) { CHECKF(0, "CreateFontFileReference %s", dw_hr_name(hr)); return 0; }
    hr = IDWriteFontFile_Analyze(t->ff, &t->sup, &t->ft, &t->fct, &t->nf);
    CHECK_HR_OK(hr);
    return SUCCEEDED(hr);
}
static void systtc_close(systtc *t) { free(t->file); DW_RELEASE(t->ff); }

static void systtc_analyze(const char *leaf, int expect_note) {
    systtc t; if (!systtc_open(&t, leaf)) { systtc_close(&t); return; }
    CHECK(t.sup);
    if (t.hdr_count > 0) {
        EXPECT_EQ(t.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION);
        EXPECT_EQ(t.fct, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
        EXPECT_EQ(t.nf, t.hdr_count);          /* the face count is the ttcf numFonts field */
        if (expect_note && t.nf != (UINT32)expect_note) t_note("%s holds %u faces (expected %d on stock Windows 10)", leaf, t.nf, expect_note);
        else t_note("%s holds %u faces", leaf, t.nf);
    } else t_note("%s is a plain font file here (face count %u)", leaf, t.nf);
    systtc_close(&t);
}
static void systtc_faces(const char *leaf) {
    systtc t; UINT32 i; if (!systtc_open(&t, leaf)) { systtc_close(&t); return; }
    if (t.hdr_count <= 0) { t_skip("not a collection"); systtc_close(&t); return; }
    for (i = 0; i < t.nf && i < 8; i++) {
        IDWriteFontFace *face = NULL; ot_dir d; ot_font f; int n = 0, bad;
        HRESULT hr = IDWriteFactory_CreateFontFace(dw_shared(), t.fct, 1, &t.ff, i, DWRITE_FONT_SIMULATIONS_NONE, &face);
        CHECKF(hr == S_OK && face, "face %u: %s", i, dw_hr_name(hr));
        if (!face) continue;
        EXPECT_EQ(IDWriteFontFace_GetIndex(face), i);
        EXPECT_EQ(IDWriteFontFace_GetType(face), DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION);
        CHECK(ot_parse_dir(t.file, t.len, i, &d) == 0);
        bad = compare_all_tables(face, t.file, t.len, &d, &n);
        CHECKF(bad == 0, "face %u: %d of %d tables differ from the file bytes", i, bad, n);
        if (ot_font_init(&f, t.file, t.len, &d) == 0) {
            DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(face, &m);
            EXPECT_EQ(IDWriteFontFace_GetGlyphCount(face), f.nglyphs); EXPECT_EQ(m.designUnitsPerEm, f.upm);
        }
        DW_RELEASE(face);
    }
    systtc_close(&t);
}
static void systtc_names_differ(const char *leaf) {
    systtc t; UINT32 i, j; uint8_t *nm[8] = { 0 }; uint32_t nl[8] = { 0 }; int diff = 0; if (!systtc_open(&t, leaf)) { systtc_close(&t); return; }
    if (t.hdr_count < 2) { t_skip("fewer than two faces"); systtc_close(&t); return; }
    for (i = 0; i < t.nf && i < 8; i++) {
        IDWriteFontFace *face = NULL;
        if (SUCCEEDED(IDWriteFactory_CreateFontFace(dw_shared(), t.fct, 1, &t.ff, i, DWRITE_FONT_SIMULATIONS_NONE, &face)) && face) { nm[i] = dw_face_table(face, TG("name"), &nl[i]); IDWriteFontFace_Release(face); }
    }
    for (i = 0; i < t.nf && i < 8; i++) for (j = i + 1; j < t.nf && j < 8; j++) if (nm[i] && nm[j] && (nl[i] != nl[j] || memcmp(nm[i], nm[j], nl[i]))) diff = 1;
    CHECKF(diff, "all %u faces report identical name tables", t.nf);
    for (i = 0; i < 8; i++) free(nm[i]);
    systtc_close(&t);
}
static void systtc_out_of_range(const char *leaf) {
    systtc t; IDWriteFontFace *face = NULL; HRESULT hr; if (!systtc_open(&t, leaf)) { systtc_close(&t); return; }
    if (t.hdr_count <= 0) { t_skip("not a collection"); systtc_close(&t); return; }
    hr = IDWriteFactory_CreateFontFace(dw_shared(), t.fct, 1, &t.ff, t.nf, DWRITE_FONT_SIMULATIONS_NONE, &face);
    CHECKF(FAILED(hr), "index == face count: %s", dw_hr_name(hr)); CHECK(face == NULL);
    t_note("%s: CreateFontFace(index %u) -> %s", leaf, t.nf, dw_hr_name(hr));
    DW_RELEASE(face); face = NULL;
    hr = IDWriteFactory_CreateFontFace(dw_shared(), t.fct, 1, &t.ff, 0xFFFF, DWRITE_FONT_SIMULATIONS_NONE, &face);
    CHECK(FAILED(hr)); CHECK(face == NULL); DW_RELEASE(face);
    systtc_close(&t);
}
static void systtc_directories_independent(const char *leaf) {
    systtc t; UINT32 i; uint32_t first_off = 0; int shared = 0, own = 0; if (!systtc_open(&t, leaf)) { systtc_close(&t); return; }
    if (t.hdr_count < 2) { t_skip("fewer than two faces"); systtc_close(&t); return; }
    for (i = 0; i < t.nf && i < 8; i++) {
        ot_dir d; const ot_rec *r;
        if (ot_parse_dir(t.file, t.len, i, &d)) continue;
        EXPECT_NE(d.sfnt_off, 0);         /* member offset tables never sit at file offset 0: that is the ttcf header */
        r = ot_find(&d, TG("glyf"));
        if (!r) r = ot_find(&d, TG("CFF "));
        if (r) { if (i == 0) first_off = r->off; else if (r->off == first_off) shared++; else own++; }
    }
    t_note("%s: outline table shared between faces: %d, separate: %d", leaf, shared, own);
    systtc_close(&t);
}
#define SYSTTC(id, leaf, expected) \
    TEST(tc_##id##_analyze) { systtc_analyze(leaf, expected); } \
    TEST(tc_##id##_each_face) { systtc_faces(leaf); } \
    TEST(tc_##id##_names_differ) { systtc_names_differ(leaf); } \
    TEST(tc_##id##_index_out_of_range) { systtc_out_of_range(leaf); } \
    TEST(tc_##id##_directories) { systtc_directories_independent(leaf); }
SYSTTC(msgothic, "msgothic.ttc", 3)
SYSTTC(cambria, "cambria.ttc", 2)
SYSTTC(simsun, "simsun.ttc", 2)
SYSTTC(meiryo, "meiryo.ttc", 4)
SYSTTC(msjh, "msjh.ttc", 2)
SYSTTC(gulim, "gulim.ttc", 4)
SYSTTC(mingliu, "mingliu.ttc", 3)
SYSTTC(batang, "batang.ttc", 4)

TEST(tc_msgothic_family_faces_have_expected_names) {
    systtc t; UINT32 i; if (!systtc_open(&t, "msgothic.ttc")) { systtc_close(&t); return; }
    if (t.hdr_count <= 0) { t_skip("not a collection"); systtc_close(&t); return; }
    for (i = 0; i < t.nf && i < 4; i++) {
        IDWriteFontFace *face = NULL;
        if (SUCCEEDED(IDWriteFactory_CreateFontFace(dw_shared(), t.fct, 1, &t.ff, i, DWRITE_FONT_SIMULATIONS_NONE, &face)) && face) {
            UINT32 n; uint8_t *nm = dw_face_table(face, TG("name"), &n); uint8_t b[256]; int l = nm ? ot_name_get(nm, n, 3, 1, 0x409, 4, b, sizeof b) : -1;
            if (l > 0) { char u8[256]; WCHAR w[128]; int k; for (k = 0; k < l / 2 && k < 127; k++) w[k] = (WCHAR)BE16(b + 2 * k); w[k] = 0; dw_narrow(w, u8, sizeof u8); t_note("msgothic.ttc face %u full name: %s", i, u8); }
            free(nm); IDWriteFontFace_Release(face);
        }
    }
    systtc_close(&t);
}
