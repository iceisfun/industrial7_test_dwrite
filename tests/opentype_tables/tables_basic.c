/* IDWriteFontFace::TryGetFontTable / ReleaseFontTable protocol, on a synthetic font opened through our own memory loader.
 * Windows behaviour (dwrite.dll IDWriteFontFace::TryGetFontTable): all four out parameters are zeroed first, the table directory of the
 * face is searched linearly for the first record whose tag (read as a little-endian ULONG from the file bytes) equals the argument,
 * the table bytes are read with exactly one ReadFileFragment(offset, directory length) on the file stream, and the pointer handed
 * back is the pointer the stream returned. ReleaseFontTable forwards its context to ReleaseFileFragment unchanged. */
#include "ot.h"

/* ---- tag packing ---- */
TEST(tb_tag_packing_is_little_endian) {
    /* DWRITE_MAKE_OPENTYPE_TAG(a,b,c,d) puts 'a' in the lowest byte: the four file bytes 'h','e','a','d' read as a LE ULONG. */
    EXPECT_EQ(DWRITE_MAKE_OPENTYPE_TAG('h', 'e', 'a', 'd'), 0x64616568u);
    EXPECT_EQ(DW_TAG('h', 'e', 'a', 'd'), 0x64616568u);
    EXPECT_EQ(DWRITE_MAKE_OPENTYPE_TAG('c', 'm', 'a', 'p'), 0x70616D63u);
    EXPECT_EQ(DWRITE_MAKE_OPENTYPE_TAG('O', 'S', '/', '2'), 0x322F534Fu);
    EXPECT_EQ(DWRITE_MAKE_OPENTYPE_TAG('c', 'v', 't', ' '), 0x20747663u);
    EXPECT_EQ(TG("GSUB"), DWRITE_MAKE_OPENTYPE_TAG('G', 'S', 'U', 'B'));
}
TEST(tb_directory_tags_read_as_le) {
    NEED_SYNTH(s);
    /* The raw directory bytes of 'head' are 68 65 61 64; our directory reader packs them LE. */
    const ot_rec *r = ot_find(&s.dir, TG("head"));
    CHECK(r != NULL);
    if (r) { const uint8_t *e = s.blob + s.dir.sfnt_off + 12; int i; for (i = 0; i < s.dir.n; i++) if (BE32(e + 16 * i) == 0x68656164) break; CHECK(i < s.dir.n); }
    synth_close(&s);
}

/* ---- every table of the default font, byte for byte ---- */
static void check_synth_table(const char *tag) {
    NEED_SYNTH(s);
    const ot_rec *r = ot_find(&s.dir, TG(tag));
    dwx_mem_stream *st = synth_stream(&s);
    if (!r) { t_skip("synthetic font has no %s", tag); synth_close(&s); return; }
    CHECK(st != NULL);
    {
        LONG out0 = st ? st->outstanding : 0, reads0 = st ? st->n_read : 0;
        tbl t = tget(s.h.face, TG(tag));
        CHECK_HR(t.hr, S_OK);
        CHECK(t.ex == TRUE);
        EXPECT_EQ(t.n, r->len);
        CHECK(t.d != NULL);
        CHECK(t.ctx != NULL);
        if (t.d && t.n == r->len) CHECK(!memcmp(t.d, s.blob + r->off, r->len));
        if (st) {
            /* The pointer returned is the one the stream handed out for (offset, length) of the directory record. */
            CHECK(t.d == (const void *)(st->data + r->off));
            CHECK(st->n_read > reads0);
            EXPECT_EQ(st->last_offset, r->off);
            EXPECT_EQ(st->last_size, r->len);
            CHECK(st->outstanding > out0);
        }
        trel(s.h.face, &t);
        if (st) EXPECT_EQ(st->outstanding, out0);
    }
    synth_close(&s);
}
#define SYNTH_TBL_TEST(id, tag) TEST(tb_table_##id) { check_synth_table(tag); }
SYNTH_TBL_TEST(head, "head") SYNTH_TBL_TEST(hhea, "hhea") SYNTH_TBL_TEST(maxp, "maxp") SYNTH_TBL_TEST(os2, "OS/2")
SYNTH_TBL_TEST(hmtx, "hmtx") SYNTH_TBL_TEST(cmap, "cmap") SYNTH_TBL_TEST(loca, "loca") SYNTH_TBL_TEST(glyf, "glyf")
SYNTH_TBL_TEST(name, "name") SYNTH_TBL_TEST(post, "post")

TEST(tb_all_tables_match_file) {
    NEED_SYNTH(s);
    int n = 0, bad = compare_all_tables(s.h.face, s.blob, s.n, &s.dir, &n);
    EXPECT_EQ(bad, 0);
    CHECK(n >= 9);
    synth_close(&s);
}

/* ---- not found ---- */
static void check_missing(UINT32 tag, const char *what) {
    NEED_SYNTH(s);
    dwx_mem_stream *st = synth_stream(&s);
    LONG out0 = st ? st->outstanding : 0;
    tbl t = tget(s.h.face, tag);
    if (ot_find(&s.dir, tag)) { t_skip("%s unexpectedly present", what); synth_close(&s); return; }
    /* Out parameters are zeroed first and the call still succeeds: absence is reported through *exists. */
    CHECK_HR(t.hr, S_OK);
    CHECKF(t.ex == FALSE, "%s: exists", what);
    CHECKF(t.d == NULL, "%s: data %p", what, t.d);
    EXPECT_EQ(t.n, 0);
    CHECK(t.ctx == NULL);
    if (st) EXPECT_EQ(st->outstanding, out0);
    trel(s.h.face, &t);
    synth_close(&s);
}
TEST(tb_missing_zzzz) { check_missing(TG("zzzz"), "zzzz"); }
TEST(tb_missing_zero_tag) { check_missing(0, "tag 0"); }
TEST(tb_missing_ffffffff) { check_missing(0xFFFFFFFFu, "tag ffffffff"); }
TEST(tb_missing_uppercase_head) { check_missing(TG("HEAD"), "HEAD"); }
TEST(tb_missing_big_endian_packed_head) { check_missing(0x68656164u, "BE-packed head"); }
TEST(tb_missing_big_endian_packed_cmap) { check_missing(0x636D6170u, "BE-packed cmap"); }
TEST(tb_missing_cvt_without_space) { check_missing(TG("cvt\0"), "cvt\\0"); }
TEST(tb_missing_os2_lowercase) { check_missing(TG("os/2"), "os/2"); }
TEST(tb_missing_os2_without_slash) { check_missing(TG("OS 2"), "OS 2"); }
TEST(tb_missing_ttcf) { check_missing(TG("ttcf"), "ttcf"); }
TEST(tb_missing_true) { check_missing(TG("true"), "true"); }
TEST(tb_missing_otto) { check_missing(TG("OTTO"), "OTTO"); }
TEST(tb_missing_version_tag) { check_missing(0x00010000u, "0x00010000"); }
TEST(tb_missing_head_prefix_only) { check_missing(TG("hea\0"), "hea\\0"); }

static const char *const k_absent_in_default[] = {
    "GSUB", "GPOS", "GDEF", "kern", "cvt ", "fpgm", "prep", "gasp", "DSIG", "COLR", "CPAL", "fvar", "avar", "STAT", "HVAR",
    "MVAR", "gvar", "cvar", "vhea", "vmtx", "VORG", "BASE", "JSTF", "MATH", "CFF ", "CFF2", "EBDT", "EBLC", "EBSC", "sbix", "CBDT", "CBLC",
    "SVG ", "hdmx", "LTSH", "VDMX", "PCLT", "meta", "Zapf", "morx", "mort", "feat", "trak", "bsln", "just", "lcar", "opbd", "prop", "Feat", "Glat", "Gloc", "Silf", "Sill"
};
TEST(tb_absent_tags_report_not_exists) {
    NEED_SYNTH(s);
    unsigned i;
    dwx_mem_stream *st = synth_stream(&s);
    LONG out0 = st ? st->outstanding : 0;
    for (i = 0; i < sizeof k_absent_in_default / sizeof k_absent_in_default[0]; i++) {
        UINT32 tag = ot_tag(k_absent_in_default[i]);
        tbl t;
        if (ot_find(&s.dir, tag)) continue;
        t = tget(s.h.face, tag);
        CHECKF(t.hr == S_OK && !t.ex && !t.d && !t.n && !t.ctx, "tag %s: hr %s ex %d d %p n %u ctx %p", k_absent_in_default[i], dw_hr_name(t.hr), t.ex, t.d, t.n, t.ctx);
        trel(s.h.face, &t);
    }
    if (st) EXPECT_EQ(st->outstanding, out0);
    synth_close(&s);
}

TEST(tb_missing_tag_does_not_read_a_table) {
    NEED_SYNTH(s);
    dwx_mem_stream *st = synth_stream(&s);
    if (!st) { t_skip("no stream"); synth_close(&s); return; }
    {
        /* A miss only walks the directory; the last read must still be the directory (or earlier), never a table body. */
        tbl a = tget(s.h.face, TG("glyf"));
        uint64_t glyf_off = st->last_offset;
        trel(s.h.face, &a);
        tbl m = tget(s.h.face, TG("zzzz"));
        CHECK(!m.ex);
        CHECK(st->last_offset != glyf_off || st->last_offset < 12 + 16 * 4);
        trel(s.h.face, &m);
        EXPECT_EQ(st->outstanding, s.base_out);
    }
    synth_close(&s);
}

TEST(tb_outparams_prefilled_are_overwritten_on_hit) {
    NEED_SYNTH(s);
    tbl t = tget(s.h.face, TG("head"));   /* tget prefills data/size/context/exists with sentinels */
    CHECK(t.d != (const void *)1); CHECK(t.n != 0xDEADBEEF); CHECK(t.ctx != (void *)1); CHECK(t.ex == TRUE);
    trel(s.h.face, &t);
    synth_close(&s);
}
TEST(tb_outparams_prefilled_are_zeroed_on_miss) {
    NEED_SYNTH(s);
    tbl t = tget(s.h.face, TG("zzzz"));
    CHECK(t.d == NULL); CHECK(t.n == 0); CHECK(t.ctx == NULL); CHECK(t.ex == FALSE);
    synth_close(&s);
}

/* ---- repeated and simultaneous fetches ---- */
TEST(tb_same_table_twice_same_pointer_distinct_contexts) {
    NEED_SYNTH(s);
    tbl a = tget(s.h.face, TG("hmtx")), b = tget(s.h.face, TG("hmtx"));
    CHECK(a.ex && b.ex);
    /* Our stream hands out the in-memory pointer for every read, and dwrite passes it through. */
    CHECK(a.d == b.d);
    EXPECT_EQ(a.n, b.n);
    CHECK(a.ctx != NULL && b.ctx != NULL);
    CHECK(a.ctx != b.ctx);       /* each fetch is its own ReadFileFragment with its own context */
    {
        dwx_mem_stream *st = synth_stream(&s);
        if (st) CHECK(st->outstanding >= 2 + s.base_out);
    }
    trel(s.h.face, &a); trel(s.h.face, &b);
    { dwx_mem_stream *st = synth_stream(&s); if (st) EXPECT_EQ(st->outstanding, s.base_out); }
    synth_close(&s);
}
TEST(tb_same_table_twice_copy_fragments_distinct_pointers) {
    IDWriteFactory *f = dw_shared(); if (!f) { t_skip("no factory"); return; }
    size_t n; uint8_t *blob = ttf_build_default(&n); dwx_font h; ot_dir d;
    memset(&h, 0, sizeof h);
    {
        dwx_mem_loader *ld = NULL; (void)ld;
    }
    /* Stream that hands out malloc'd copies: two fetches of one table give two different pointers with equal bytes. */
    HRESULT hr;
    memset(&h, 0, sizeof h); h.f = f; h.ld = dwx_loader_new(); h.ld->stream_copy_fragments = 1;
    h.key = dwx_loader_add(h.ld, blob, n);
    hr = dwx_loader_register(h.ld, f);
    if (SUCCEEDED(hr)) hr = dwx_loader_file(h.ld, f, h.key, &h.file);
    if (SUCCEEDED(hr)) hr = IDWriteFontFile_Analyze(h.file, &h.supported, &h.file_type, &h.face_type, &h.nfaces);
    if (SUCCEEDED(hr)) hr = IDWriteFactory_CreateFontFace(f, h.face_type, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &h.face);
    if (FAILED(hr) || !h.face) { t_skip("setup %s", dw_hr_name(hr)); dwx_font_close(&h); free(blob); return; }
    ot_parse_dir(blob, n, 0, &d);
    {
        LONG base0 = dwx_loader_total_outstanding(h.ld);
        tbl a = tget(h.face, TG("cmap")), b = tget(h.face, TG("cmap"));
        const ot_rec *r = ot_find(&d, TG("cmap"));
        CHECK(a.ex && b.ex && a.d && b.d);
        CHECK(a.d != b.d);
        EXPECT_EQ(a.n, r->len); EXPECT_EQ(b.n, r->len);
        if (a.d && a.n == r->len) CHECK(!memcmp(a.d, blob + r->off, r->len));
        if (b.d && b.n == r->len) CHECK(!memcmp(b.d, blob + r->off, r->len));
        trel(h.face, &b);
        /* the first copy stays valid after the second is released */
        if (a.d && a.n == r->len) CHECK(!memcmp(a.d, blob + r->off, r->len));
        trel(h.face, &a);
        EXPECT_EQ(dwx_loader_total_outstanding(h.ld), base0);
    }
    dwx_font_close(&h); free(blob);
}
TEST(tb_hold_all_tables_then_release_forward) {
    NEED_SYNTH(s);
    tbl t[32]; int i, n = s.dir.n < 32 ? s.dir.n : 32;
    dwx_mem_stream *st = synth_stream(&s);
    for (i = 0; i < n; i++) { t[i] = tget(s.h.face, s.dir.r[i].tag); CHECK(t[i].ex); }
    if (st) CHECK(st->outstanding >= n + s.base_out);
    for (i = 0; i < n; i++) { if (t[i].d && t[i].n == s.dir.r[i].len) CHECK(!memcmp(t[i].d, s.blob + s.dir.r[i].off, t[i].n)); }
    for (i = 0; i < n; i++) trel(s.h.face, &t[i]);
    if (st) EXPECT_EQ(st->outstanding, s.base_out);
    synth_close(&s);
}
TEST(tb_hold_all_tables_then_release_reverse) {
    NEED_SYNTH(s);
    tbl t[32]; int i, n = s.dir.n < 32 ? s.dir.n : 32;
    dwx_mem_stream *st = synth_stream(&s);
    for (i = 0; i < n; i++) t[i] = tget(s.h.face, s.dir.r[i].tag);
    for (i = n - 1; i >= 0; i--) trel(s.h.face, &t[i]);
    if (st) EXPECT_EQ(st->outstanding, s.base_out);
    synth_close(&s);
}
TEST(tb_hold_all_tables_then_release_interleaved) {
    NEED_SYNTH(s);
    tbl t[32]; int i, n = s.dir.n < 32 ? s.dir.n : 32;
    dwx_mem_stream *st = synth_stream(&s);
    for (i = 0; i < n; i++) t[i] = tget(s.h.face, s.dir.r[i].tag);
    for (i = 0; i < n; i += 2) trel(s.h.face, &t[i]);
    if (st) CHECK(st->outstanding >= n / 2 + s.base_out);
    for (i = 1; i < n; i += 2) { if (t[i].d && t[i].n == s.dir.r[i].len) CHECK(!memcmp(t[i].d, s.blob + s.dir.r[i].off, t[i].n)); trel(s.h.face, &t[i]); }
    if (st) EXPECT_EQ(st->outstanding, s.base_out);
    synth_close(&s);
}
TEST(tb_hundred_fetches_balance) {
    NEED_SYNTH(s);
    int i; dwx_mem_stream *st = synth_stream(&s);
    LONG reads0 = st ? st->n_read : 0;
    for (i = 0; i < 100; i++) { tbl t = tget(s.h.face, TG("maxp")); CHECK(t.ex && t.n == 32); trel(s.h.face, &t); }
    if (st) { EXPECT_EQ(st->outstanding, s.base_out); CHECK(st->n_read - reads0 >= 100); }
    synth_close(&s);
}
TEST(tb_stream_created_once_per_face) {
    NEED_SYNTH(s);
    LONG c0 = s.h.ld->n_create; int i;
    for (i = 0; i < 20; i++) { tbl t = tget(s.h.face, TG("head")); trel(s.h.face, &t); }
    for (i = 0; i < 20; i++) { tbl t = tget(s.h.face, TG("zzzz")); trel(s.h.face, &t); }
    /* The face keeps the stream it opened; table access never calls CreateStreamFromKey again. */
    EXPECT_EQ(s.h.ld->n_create, c0);
    synth_close(&s);
}
TEST(tb_table_read_is_exact_offset_and_length) {
    NEED_SYNTH(s);
    dwx_mem_stream *st = synth_stream(&s);
    int i;
    if (!st) { t_skip("no stream"); synth_close(&s); return; }
    for (i = 0; i < s.dir.n; i++) {
        tbl t = tget(s.h.face, s.dir.r[i].tag);
        CHECKF(st->last_offset == s.dir.r[i].off && st->last_size == s.dir.r[i].len, "table %d: read (%llu,%llu) want (%u,%u)", i,
               (unsigned long long)st->last_offset, (unsigned long long)st->last_size, s.dir.r[i].off, s.dir.r[i].len);
        trel(s.h.face, &t);
    }
    synth_close(&s);
}

/* ---- ReleaseFontTable ---- */
TEST(tb_release_forwards_context_to_stream) {
    NEED_SYNTH(s);
    dwx_mem_stream *st = synth_stream(&s);
    tbl t = tget(s.h.face, TG("head"));
    if (!st) { t_skip("no stream"); trel(s.h.face, &t); synth_close(&s); return; }
    {
        LONG rel0 = st->n_release, bad0 = st->bad_release_ctx;
        IDWriteFontFace_ReleaseFontTable(s.h.face, t.ctx);
        EXPECT_EQ(st->n_release, rel0 + 1);
        EXPECT_EQ(st->bad_release_ctx, bad0);     /* the very context the stream issued comes back */
        EXPECT_EQ(st->outstanding, s.base_out);
    }
    synth_close(&s);
}
TEST(tb_release_null_context_reaches_stream) {
    NEED_SYNTH(s);
    dwx_mem_stream *st = synth_stream(&s);
    tbl t = tget(s.h.face, TG("head"));   /* make sure the face has its stream */
    trel(s.h.face, &t);
    if (!st) { t_skip("no stream"); synth_close(&s); return; }
    {
        /* ReleaseFontTable does no filtering: even NULL goes to ReleaseFileFragment (our stream counts it as a foreign context). */
        LONG rel0 = st->n_release, bad0 = st->bad_release_ctx;
        IDWriteFontFace_ReleaseFontTable(s.h.face, NULL);
        EXPECT_EQ(st->n_release, rel0 + 1);
        EXPECT_EQ(st->bad_release_ctx, bad0 + 1);
        EXPECT_EQ(st->outstanding, s.base_out);
    }
    synth_close(&s);
}
TEST(tb_release_after_miss_context_is_null) {
    NEED_SYNTH(s);
    tbl t = tget(s.h.face, TG("zzzz"));
    CHECK(t.ctx == NULL);
    /* The documented pattern: a miss leaves nothing to release, and callers only release hits. */
    dwx_mem_stream *st = synth_stream(&s);
    if (st) EXPECT_EQ(st->outstanding, s.base_out);
    synth_close(&s);
}
TEST(tb_release_order_independent_of_fetch_order) {
    NEED_SYNTH(s);
    tbl a = tget(s.h.face, TG("head")), b = tget(s.h.face, TG("hhea")), c = tget(s.h.face, TG("maxp"));
    trel(s.h.face, &b); trel(s.h.face, &c); trel(s.h.face, &a);
    dwx_mem_stream *st = synth_stream(&s);
    if (st) { EXPECT_EQ(st->outstanding, s.base_out); EXPECT_EQ(st->bad_release_ctx, 0); }
    synth_close(&s);
}

/* ---- lifetime ---- */
TEST(tb_table_survives_other_table_release) {
    NEED_SYNTH(s);
    tbl a = tget(s.h.face, TG("cmap")), b = tget(s.h.face, TG("name"));
    const ot_rec *ra = ot_find(&s.dir, TG("cmap"));
    trel(s.h.face, &b);
    CHECK(a.n == ra->len && !memcmp(a.d, s.blob + ra->off, ra->len));
    trel(s.h.face, &a);
    synth_close(&s);
}
TEST(tb_face_released_with_balanced_table_use) {
    NEED_SYNTH(s);
    {
        tbl t = tget(s.h.face, TG("head"));
        CHECK(t.ex);
        trel(s.h.face, &t);
    }
    /* Release the face while the loader is still registered: the stream the face opened is released and no fragment is left. */
    dwx_mem_stream *st = synth_stream(&s);
    IDWriteFontFace *face = s.h.face; s.h.face = NULL;
    IDWriteFontFace_Release(face);
    if (st) t_note("outstanding fragments after face release: %ld (base %ld)", (long)st->outstanding, (long)s.base_out);
    synth_close(&s);
}
TEST(tb_face_released_with_held_table_note) {
    NEED_SYNTH(s);
    tbl t = tget(s.h.face, TG("head"));
    dwx_mem_stream *st = synth_stream(&s);
    IDWriteFontFace *face = s.h.face; s.h.face = NULL;
    IDWriteFontFace_Release(face);
    /* Keeping a table past the face is outside the contract (the face owns the stream); record what the stream saw. */
    if (st) t_note("stream refs %ld destroyed %d outstanding %ld after the face was released with a table held", (long)dwx_refs(st), dwx_destroyed(st), (long)st->outstanding);
    (void)t;
    synth_close(&s);
}
TEST(tb_two_faces_share_nothing) {
    NEED_SYNTH(s);
    IDWriteFontFace *f2 = NULL;
    HRESULT hr = IDWriteFactory_CreateFontFace(s.h.f, s.h.face_type, 1, &s.h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &f2);
    CHECK_HR_OK(hr);
    if (f2) {
        tbl a = tget(s.h.face, TG("head")), b = tget(f2, TG("head"));
        CHECK(a.ex && b.ex);
        EXPECT_EQ(a.n, b.n);
        if (a.d && b.d && a.n == b.n) CHECK(!memcmp(a.d, b.d, a.n));
        trel(s.h.face, &a); trel(f2, &b);
        DW_RELEASE(f2);
    }
    synth_close(&s);
}
TEST(tb_simulated_face_same_tables) {
    NEED_SYNTH(s);
    IDWriteFontFace *fb = NULL;
    /* Simulations alter rendering, not the font's tables: a bold-simulated face reads the same bytes. */
    HRESULT hr = IDWriteFactory_CreateFontFace(s.h.f, s.h.face_type, 1, &s.h.file, 0, DWRITE_FONT_SIMULATIONS_BOLD, &fb);
    CHECK_HR_OK(hr);
    if (fb) {
        int n = 0, bad = compare_all_tables(fb, s.blob, s.n, &s.dir, &n);
        EXPECT_EQ(bad, 0);
        DW_RELEASE(fb);
    }
    synth_close(&s);
}

/* ---- face interfaces expose the same method ---- */
TEST(tb_via_derived_interfaces) {
    NEED_SYNTH(s);
    IDWriteFontFace1 *f1 = NULL; IDWriteFontFace3 *f3 = NULL;
    const ot_rec *r = ot_find(&s.dir, TG("head"));
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.h.face, &IID_IDWriteFontFace1, (void **)&f1)) && f1) {
        const void *d = NULL; UINT32 n = 0; void *ctx = NULL; BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteFontFace1_TryGetFontTable(f1, TG("head"), &d, &n, &ctx, &ex));
        CHECK(ex && n == r->len && d == (const void *)(synth_stream(&s)->data + r->off));
        IDWriteFontFace1_ReleaseFontTable(f1, ctx);
        DW_RELEASE(f1);
    } else t_note("IDWriteFontFace1 unsupported");
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(s.h.face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) {
        const void *d = NULL; UINT32 n = 0; void *ctx = NULL; BOOL ex = FALSE;
        CHECK_HR_OK(IDWriteFontFace3_TryGetFontTable(f3, TG("head"), &d, &n, &ctx, &ex));
        CHECK(ex && n == r->len);
        IDWriteFontFace3_ReleaseFontTable(f3, ctx);
        DW_RELEASE(f3);
    } else t_note("IDWriteFontFace3 unsupported");
    synth_close(&s);
}

/* ---- threads ---- */
typedef struct { IDWriteFontFace *face; const uint8_t *blob; const ot_dir *dir; int bad; } thr_arg;
static DWORD WINAPI thr_fetch(LPVOID p) {
    thr_arg *a = (thr_arg *)p; int i;
    for (i = 0; i < 300; i++) {
        const ot_rec *r = &a->dir->r[i % a->dir->n];
        tbl t = tget(a->face, r->tag);
        if (t.hr != S_OK || !t.ex || t.n != r->len || (r->len && memcmp(t.d, a->blob + r->off, r->len))) a->bad++;
        trel(a->face, &t);
    }
    return 0;
}
TEST(tb_concurrent_fetch_eight_threads) {
    NEED_SYNTH(s);
    HANDLE h[8]; thr_arg a[8]; int i, ok = 0;
    for (i = 0; i < 8; i++) { a[i].face = s.h.face; a[i].blob = s.blob; a[i].dir = &s.dir; a[i].bad = 0; h[i] = CreateThread(NULL, 0, thr_fetch, &a[i], 0, NULL); if (h[i]) ok++; }
    for (i = 0; i < 8; i++) if (h[i]) { CHECK(WaitForSingleObject(h[i], 20000) == WAIT_OBJECT_0); CloseHandle(h[i]); EXPECT_EQ(a[i].bad, 0); }
    CHECK(ok == 8);
    { dwx_mem_stream *st = synth_stream(&s); if (st) EXPECT_EQ(st->outstanding, s.base_out); }
    synth_close(&s);
}

/* ---- directory quirks ---- */
static int open_blob(dwx_font *h, const uint8_t *b, size_t n) {
    IDWriteFactory *f = dw_shared();
    HRESULT hr = f ? dwx_font_open(h, f, b, n, 0, DWRITE_FONT_SIMULATIONS_NONE) : E_FAIL;
    return hr == S_OK && h->face;
}
TEST(tb_duplicate_tag_first_record_wins) {
    size_t n; uint8_t *b = ttf_build_default(&n); ot_dir d; dwx_font h;
    memset(&h, 0, sizeof h);
    ot_parse_dir(b, n, 0, &d);
    {
        /* Make the 'post' record a second 'name' record: the directory search returns the first match. */
        int ip = -1, in = -1, i;
        for (i = 0; i < d.n; i++) { if (d.r[i].tag == TG("post")) ip = i; if (d.r[i].tag == TG("name")) in = i; }
        if (ip < 0 || in < 0) { t_skip("tables missing"); free(b); return; }
        {
            int first = ip < in ? ip : in, second = ip < in ? in : ip;
            uint8_t *e2 = b + 12 + 16 * second, *e1 = b + 12 + 16 * first;
            UINT32 keep = d.r[first].tag, lost = d.r[second].tag;
            memcpy(e2, e1, 4);
            ot_parse_dir(b, n, 0, &d);
            if (open_blob(&h, b, n)) {
                tbl t = tget(h.face, keep), g = tget(h.face, lost);
                CHECK(t.ex);
                CHECK(t.d == (const void *)(h.ld->last_stream->data + ot_find(&d, keep)->off));
                CHECK(!g.ex);          /* the shadowed table's own tag no longer exists */
                trel(h.face, &t); trel(h.face, &g);
            } else t_note("face over duplicate-tag directory: %s", dw_hr_name(h.ld ? S_OK : E_FAIL));
        }
    }
    dwx_font_close(&h); free(b);
}
TEST(tb_reversed_directory_still_found) {
    size_t n; uint8_t *b = ttf_build_default(&n); ot_dir d; dwx_font h; int i;
    memset(&h, 0, sizeof h);
    ot_parse_dir(b, n, 0, &d);
    for (i = 0; i < d.n / 2; i++) { uint8_t tmp[16]; uint8_t *p = b + 12 + 16 * i, *q = b + 12 + 16 * (d.n - 1 - i); memcpy(tmp, p, 16); memcpy(p, q, 16); memcpy(q, tmp, 16); }
    ot_parse_dir(b, n, 0, &d);
    if (open_blob(&h, b, n)) {
        /* GetTableByTag is a linear search, so directory order is irrelevant for TryGetFontTable. */
        int cnt = 0, bad = compare_all_tables(h.face, b, n, &d, &cnt);
        EXPECT_EQ(bad, 0); CHECK(cnt >= 9);
    } else t_note("face over unsorted directory could not be created");
    dwx_font_close(&h); free(b);
}
TEST(tb_zero_length_table_exists) {
    ttf_spec sp; static const uint8_t raw[12] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    ttf_rawtable rt = { "DSIG", raw, 12 }; size_t n; uint8_t *b; ot_dir d; dwx_font h; int i, idx = -1;
    memset(&h, 0, sizeof h);
    ttf_default_spec(&sp); sp.raw = &rt; sp.nraw = 1;
    b = ttf_build(&sp, &n); ot_parse_dir(b, n, 0, &d);
    for (i = 0; i < d.n; i++) if (d.r[i].tag == TG("DSIG")) idx = i;
    if (idx >= 0) {
        size_t n2; uint8_t *z = ttf_corrupt(b, n, TTF_CORRUPT_ZERO_TABLE, idx, &n2);
        if (z && open_blob(&h, z, n2)) {
            tbl t = tget(h.face, TG("DSIG"));
            /* A directory record with length 0 is still a table: exists = TRUE, size 0. */
            CHECK_HR(t.hr, S_OK); CHECK(t.ex == TRUE); EXPECT_EQ(t.n, 0);
            trel(h.face, &t);
        } else t_note("face over zero-length DSIG not created");
        free(z);
    } else t_skip("DSIG not generated");
    dwx_font_close(&h); free(b);
}

/* ---- private and odd tags ---- */
static void check_raw_table(const char *tag, unsigned len) {
    ttf_spec sp; uint8_t *raw = (uint8_t *)malloc(len + 1); unsigned i; ttf_rawtable rt; size_t n; uint8_t *b; ot_dir d; dwx_font h;
    memset(&h, 0, sizeof h);
    for (i = 0; i < len; i++) raw[i] = (uint8_t)(i * 7 + 3);
    rt.data = raw; rt.len = len; memset(rt.tag, 0, 5); memcpy(rt.tag, tag, 4);
    ttf_default_spec(&sp); sp.raw = &rt; sp.nraw = 1;
    b = ttf_build(&sp, &n);
    if (!b) { t_skip("build failed"); free(raw); return; }
    ot_parse_dir(b, n, 0, &d);
    if (open_blob(&h, b, n)) {
        const ot_rec *r = ot_find(&d, TG(tag));
        tbl t = tget(h.face, TG(tag));
        CHECK(r != NULL);
        /* The size is the directory length verbatim, not the 4-byte padded size. */
        CHECK(t.ex); EXPECT_EQ(t.n, len);
        if (r) EXPECT_EQ(r->len, len);
        if (t.d && t.n == len) CHECK(!memcmp(t.d, raw, len));
        trel(h.face, &t);
    } else t_note("face with raw %s table not created", tag);
    dwx_font_close(&h); free(b); free(raw);
}
#define RAW_TEST(id, tag, len) TEST(tb_raw_##id) { check_raw_table(tag, len); }
RAW_TEST(len1, "TST1", 1) RAW_TEST(len2, "TST2", 2) RAW_TEST(len3, "TST3", 3) RAW_TEST(len4, "TST4", 4) RAW_TEST(len5, "TST5", 5)
RAW_TEST(len7, "TST7", 7) RAW_TEST(len8, "TST8", 8) RAW_TEST(len13, "TSTD", 13) RAW_TEST(len100, "TSTH", 100) RAW_TEST(len4097, "TSTK", 4097)
RAW_TEST(len65535, "TSTM", 65535) RAW_TEST(len262145, "TSTN", 262145)
RAW_TEST(cvt, "cvt ", 40) RAW_TEST(cvt_odd, "cvt ", 41) RAW_TEST(fpgm, "fpgm", 17) RAW_TEST(prep, "prep", 30) RAW_TEST(dsig, "DSIG", 20)
RAW_TEST(spacey_tag, "a b ", 10) RAW_TEST(digits_tag, "1234", 10) RAW_TEST(high_ascii_tag, "\x7e\x7f\x41\x42", 10) RAW_TEST(lowercase_tag, "abcd", 10)
RAW_TEST(upper_tag, "ZZZZ", 10)
