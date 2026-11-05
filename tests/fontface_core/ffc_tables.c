/* IDWriteFontFace::TryGetFontTable / ReleaseFontTable.
 *
 * Windows 10 (DWriteFontFace::TryGetFontTable): all four outputs (tableData, tableSize, tableContext, exists) are zeroed first; faces of an
 * unsupported kind (TYPE1 / RAW_CFF) answer DWRITE_E_UNSUPPORTEDOPERATION; otherwise the table directory of the font (at faceIndex for
 * collections) is searched by tag, exists = whether the tag is present, the returned pointer/size describe the table exactly as stored
 * in the file (unpadded length from the directory) and tableContext is the fragment context that ReleaseFontTable hands back to the stream.
 * Tags are DWRITE_MAKE_OPENTYPE_TAG values: first character in the lowest byte (DW_TAG). */
#include "ffc.h"

typedef struct { const void *data; UINT32 size; void *ctx; BOOL exists; HRESULT hr; } ffc_tab;
static ffc_tab ffc_get_tab(IDWriteFontFace *face, UINT32 tag) {
    ffc_tab t; t.data = (const void *)(ULONG_PTR)0x1111; t.size = 0x2222; t.ctx = (void *)(ULONG_PTR)0x3333; t.exists = (BOOL)0x4444;
    t.hr = IDWriteFontFace_TryGetFontTable(face, tag, &t.data, &t.size, &t.ctx, &t.exists);
    return t;
}

/* ---- the standard tables ---------------------------------------------------------------------------------------------------------- */
static void ffc_table_equals_blob(IDWriteFactory *f, const char *tag4) {
    ttf_spec s; size_t n; uint8_t *b; const uint8_t *want; unsigned wl = 0; dwx_font h; IDWriteFontFace *face; ffc_tab t;
    ttf_default_spec(&s);
    b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    want = ffc_blob_table(b, n, tag4, &wl);
    if (!want) { free(b); t_skip("table %s not generated", tag4); return; }
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) { free(b); return; }
    t = ffc_get_tab(face, DW_TAG(tag4[0], tag4[1], tag4[2], tag4[3]));
    CHECKF(t.hr == S_OK, "%s -> %s", tag4, dw_hr_name(t.hr));
    CHECKF(t.exists == TRUE, "%s exists=%d", tag4, (int)t.exists);
    if (t.hr == S_OK && t.exists) {
        CHECKF(t.size == wl, "%s size %u want %u", tag4, t.size, wl);
        CHECKF(t.data != NULL, "%s data NULL", tag4);
        if (t.data && t.size == wl) CHECKF(!memcmp(t.data, want, wl), "%s bytes differ", tag4);
        IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    }
    dwx_font_close(&h); free(b);
}
FTEST(fft_table_head) { ffc_table_equals_blob(f, "head"); }
FTEST(fft_table_hhea) { ffc_table_equals_blob(f, "hhea"); }
FTEST(fft_table_maxp) { ffc_table_equals_blob(f, "maxp"); }
FTEST(fft_table_os2) { ffc_table_equals_blob(f, "OS/2"); }
FTEST(fft_table_hmtx) { ffc_table_equals_blob(f, "hmtx"); }
FTEST(fft_table_cmap) { ffc_table_equals_blob(f, "cmap"); }
FTEST(fft_table_loca) { ffc_table_equals_blob(f, "loca"); }
FTEST(fft_table_glyf) { ffc_table_equals_blob(f, "glyf"); }
FTEST(fft_table_name) { ffc_table_equals_blob(f, "name"); }
FTEST(fft_table_post) { ffc_table_equals_blob(f, "post"); }

FTEST(fft_optional_tables_present_when_generated) {
    ttf_spec s; static const ttf_kernpair kp[] = { { 2, 3, -100 } }; static const ttf_gasp gp[] = { { 8, 2 }, { 0xFFFF, 0xF } }; size_t n; uint8_t *b; dwx_font h; IDWriteFontFace *face; int i;
    static const char *tags[] = { "kern", "GPOS", "vhea", "vmtx", "gasp", "BASE" };
    ttf_default_spec(&s); s.kern = kp; s.nkern = 1; s.gpos_kern = kp; s.ngpos_kern = 1; s.vertical = 1; s.gasp = gp; s.ngasp = 2; s.has_base = 1;
    b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) { free(b); return; }
    for (i = 0; i < 6; i++) {
        unsigned wl = 0; const uint8_t *want = ffc_blob_table(b, n, tags[i], &wl); ffc_tab t = ffc_get_tab(face, DW_TAG(tags[i][0], tags[i][1], tags[i][2], tags[i][3]));
        CHECKF(want != NULL, "generator lacks %s", tags[i]);
        CHECKF(t.hr == S_OK && t.exists, "%s hr %s exists %d", tags[i], dw_hr_name(t.hr), (int)t.exists);
        if (t.hr == S_OK && t.exists) {
            CHECKF(t.size == wl, "%s size %u want %u", tags[i], t.size, wl);
            if (want && t.size == wl) CHECKF(!memcmp(t.data, want, wl), "%s bytes", tags[i]);
            IDWriteFontFace_ReleaseFontTable(face, t.ctx);
        }
    }
    dwx_font_close(&h); free(b);
}

/* ---- absent tables --------------------------------------------------------------------------------------------------------------------- */
FTEST(fft_missing_tag_outputs_zero) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const UINT32 tags[] = { DW_TAG('Z','Z','Z','Z'), DW_TAG('k','e','r','n'), DW_TAG('G','S','U','B'), DW_TAG('C','F','F',' '), 0, 0xFFFFFFFF, 0x20202020, DW_TAG('g','l','y','F') };
    int i; if (!face) return;
    for (i = 0; i < (int)(sizeof tags / sizeof tags[0]); i++) {
        ffc_tab t = ffc_get_tab(face, tags[i]);
        CHECKF(t.hr == S_OK, "tag 0x%08x -> %s", tags[i], dw_hr_name(t.hr));
        CHECKF(t.exists == FALSE, "tag 0x%08x exists", tags[i]);
        CHECKF(t.data == NULL && t.size == 0 && t.ctx == NULL, "tag 0x%08x outputs not zeroed (%p %u %p)", tags[i], t.data, t.size, t.ctx);
    }
    dwx_font_close(&h);
}
FTEST(fft_tag_byte_order_is_first_char_lowest) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ffc_tab t, u; if (!face) return;
    t = ffc_get_tab(face, DW_TAG('c','m','a','p')); CHECK(t.hr == S_OK && t.exists); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    u = ffc_get_tab(face, 0x636d6170);      /* the big-endian spelling of 'cmap' as a host integer: not a tag in this order */
    CHECK(u.hr == S_OK); CHECK(!u.exists);
    if (u.exists) IDWriteFontFace_ReleaseFontTable(face, u.ctx);
    dwx_font_close(&h);
}
FTEST(fft_tag_case_sensitive) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); static const UINT32 tags[] = { DW_TAG('C','M','A','P'), DW_TAG('H','e','a','d'), DW_TAG('o','s','/','2'), DW_TAG('O','S','/','2') };
    int i; if (!face) return;
    for (i = 0; i < 4; i++) {
        ffc_tab t = ffc_get_tab(face, tags[i]);
        CHECK(t.hr == S_OK);
        if (i < 3) CHECKF(!t.exists, "tag %d matched case-insensitively", i); else CHECK(t.exists);
        if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    }
    dwx_font_close(&h);
}
FTEST(fft_tag_with_space_padding) {
    /* tags shorter than four characters are space padded in the directory: 'cvt ' style */
    ttf_spec s; static const uint8_t data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }; ttf_rawtable raw; dwx_font h; IDWriteFontFace *face; ffc_tab t;
    ttf_default_spec(&s); memcpy(raw.tag, "cvt ", 5); raw.data = data; raw.len = 8; s.raw = &raw; s.nraw = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    t = ffc_get_tab(face, DW_TAG('c','v','t',' ')); CHECK(t.hr == S_OK && t.exists);
    if (t.exists) { EXPECT_EQ(t.size, 8); if (t.data && t.size == 8) CHECK(!memcmp(t.data, data, 8)); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    t = ffc_get_tab(face, DW_TAG('c','v','t',0)); CHECK(t.hr == S_OK && !t.exists); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    dwx_font_close(&h);
}

/* ---- raw tables of assorted shapes -------------------------------------------------------------------------------------------------------- */
FTEST(fft_raw_table_lengths) {
    static const unsigned lens[] = { 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 100, 255, 256, 1000, 4095, 4096, 4097, 65535, 65536, 65537, 300000 };
    int i;
    for (i = 0; i < (int)(sizeof lens / sizeof lens[0]); i++) {
        ttf_spec s; uint8_t *buf = (uint8_t *)malloc(lens[i]); ttf_rawtable raw; dwx_font h; IDWriteFontFace *face; ffc_tab t; unsigned k;
        for (k = 0; k < lens[i]; k++) buf[k] = (uint8_t)(k * 31 + lens[i]);
        ttf_default_spec(&s); memcpy(raw.tag, "TSTA", 5); raw.data = buf; raw.len = lens[i]; s.raw = &raw; s.nraw = 1;
        face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE);
        if (face) {
            t = ffc_get_tab(face, DW_TAG('T','S','T','A'));
            CHECKF(t.hr == S_OK && t.exists, "len %u: hr %s exists %d", lens[i], dw_hr_name(t.hr), (int)t.exists);
            if (t.exists) { CHECKF(t.size == lens[i], "len %u: size %u", lens[i], t.size); if (t.data && t.size == lens[i]) CHECKF(!memcmp(t.data, buf, lens[i]), "len %u bytes differ", lens[i]); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
            dwx_font_close(&h);
        }
        free(buf);
        if (!face) return;
    }
}
FTEST(fft_raw_table_two_megabytes) {
    ttf_spec s; unsigned len = 2 * 1024 * 1024; uint8_t *buf = (uint8_t *)malloc(len); ttf_rawtable raw; dwx_font h; IDWriteFontFace *face; ffc_tab t; unsigned k;
    for (k = 0; k < len; k++) buf[k] = (uint8_t)(k ^ (k >> 8) ^ (k >> 16));
    ttf_default_spec(&s); memcpy(raw.tag, "BIGT", 5); raw.data = buf; raw.len = len; s.raw = &raw; s.nraw = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE);
    if (face) {
        t = ffc_get_tab(face, DW_TAG('B','I','G','T'));
        CHECK(t.hr == S_OK && t.exists);
        if (t.exists) { EXPECT_EQ(t.size, len); if (t.data && t.size == len) CHECK(!memcmp(t.data, buf, len)); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
        dwx_font_close(&h);
    }
    free(buf);
}
FTEST(fft_raw_many_tables) {
    ttf_spec s; ttf_rawtable raw[24]; uint8_t data[24][16]; dwx_font h; IDWriteFontFace *face; int i;
    ttf_default_spec(&s);
    for (i = 0; i < 24; i++) { memset(data[i], 0x40 + i, 16); data[i][0] = (uint8_t)i; sprintf(raw[i].tag, "TB%02d", i); raw[i].data = data[i]; raw[i].len = 16; }
    s.raw = raw; s.nraw = 24;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    for (i = 23; i >= 0; i--) {
        ffc_tab t = ffc_get_tab(face, DW_TAG(raw[i].tag[0], raw[i].tag[1], raw[i].tag[2], raw[i].tag[3]));
        CHECKF(t.hr == S_OK && t.exists, "table %d", i);
        if (t.exists) { EXPECT_EQ(t.size, 16); if (t.data && t.size == 16) CHECKF(!memcmp(t.data, data[i], 16), "table %d bytes", i); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    }
    { ffc_tab t = ffc_get_tab(face, DW_TAG('T','B','2','4')); CHECK(t.hr == S_OK && !t.exists); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    dwx_font_close(&h);
}
FTEST(fft_raw_replaces_generated_table) {
    ttf_spec s; static uint8_t post[32]; ttf_rawtable raw; dwx_font h; IDWriteFontFace *face; ffc_tab t; DWRITE_FONT_METRICS m;
    memset(post, 0, sizeof post); post[1] = 3;     /* version 3.0 */
    post[8] = 0xFF; post[9] = 0x38; post[10] = 0; post[11] = 0x40;    /* underlinePosition -200, thickness 64 */
    ttf_default_spec(&s); memcpy(raw.tag, "post", 5); raw.data = post; raw.len = 32; s.raw = &raw; s.nraw = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    t = ffc_get_tab(face, DW_TAG('p','o','s','t'));
    CHECK(t.hr == S_OK && t.exists);
    if (t.exists) { EXPECT_EQ(t.size, 32); if (t.data && t.size == 32) CHECK(!memcmp(t.data, post, 32)); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    IDWriteFontFace_GetMetrics(face, &m);
    EXPECT_EQ(m.underlineThickness, 64); EXPECT_EQ(m.underlinePosition, -200);
    dwx_font_close(&h);
}
FTEST(fft_empty_raw_table_note) {
    ttf_spec s; static const uint8_t one = 0; ttf_rawtable raw; dwx_font h; IDWriteFontFace *face; ffc_tab t;
    ttf_default_spec(&s); memcpy(raw.tag, "EMPT", 5); raw.data = &one; raw.len = 0; s.raw = &raw; s.nraw = 1;
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) return;
    t = ffc_get_tab(face, DW_TAG('E','M','P','T'));
    t_note("zero-length table: hr %s exists %d size %u data %p", dw_hr_name(t.hr), (int)t.exists, t.size, t.data);
    CHECK(t.hr == S_OK);
    if (t.exists) { EXPECT_EQ(t.size, 0); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    dwx_font_close(&h);
}

/* ---- repeated retrieval / context / release ----------------------------------------------------------------------------------------------- */
FTEST(fft_repeat_same_table) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ffc_tab a, b; if (!face) return;
    a = ffc_get_tab(face, DW_TAG('c','m','a','p')); b = ffc_get_tab(face, DW_TAG('c','m','a','p'));
    CHECK(a.hr == S_OK && a.exists && b.hr == S_OK && b.exists);
    EXPECT_EQ(a.size, b.size);
    if (a.exists && b.exists && a.size == b.size) CHECK(!memcmp(a.data, b.data, a.size));
    t_note("repeated TryGetFontTable: same pointer %d, same context %d", a.data == b.data, a.ctx == b.ctx);
    if (a.exists) IDWriteFontFace_ReleaseFontTable(face, a.ctx);
    if (b.exists) IDWriteFontFace_ReleaseFontTable(face, b.ctx);
    dwx_font_close(&h);
}
FTEST(fft_hold_several_tables_then_release_in_any_order) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ffc_tab t[5]; static const char *tg[5] = { "head", "cmap", "glyf", "name", "hmtx" }; uint8_t *copy[5]; int i;
    if (!face) return;
    for (i = 0; i < 5; i++) { t[i] = ffc_get_tab(face, DW_TAG(tg[i][0], tg[i][1], tg[i][2], tg[i][3])); CHECKF(t[i].hr == S_OK && t[i].exists, "%s", tg[i]); copy[i] = NULL; if (t[i].exists && t[i].data) { copy[i] = (uint8_t *)malloc(t[i].size); memcpy(copy[i], t[i].data, t[i].size); } }
    /* more retrievals while the first ones are held must not disturb them */
    for (i = 0; i < 5; i++) { ffc_tab x = ffc_get_tab(face, DW_TAG(tg[4 - i][0], tg[4 - i][1], tg[4 - i][2], tg[4 - i][3])); if (x.exists) IDWriteFontFace_ReleaseFontTable(face, x.ctx); }
    for (i = 0; i < 5; i++) if (copy[i] && t[i].data) CHECKF(!memcmp(copy[i], t[i].data, t[i].size), "%s changed while held", tg[i]);
    IDWriteFontFace_ReleaseFontTable(face, t[2].ctx); IDWriteFontFace_ReleaseFontTable(face, t[0].ctx); IDWriteFontFace_ReleaseFontTable(face, t[4].ctx);
    if (copy[1] && t[1].data) CHECK(!memcmp(copy[1], t[1].data, t[1].size));
    IDWriteFontFace_ReleaseFontTable(face, t[1].ctx); IDWriteFontFace_ReleaseFontTable(face, t[3].ctx);
    for (i = 0; i < 5; i++) free(copy[i]);
    dwx_font_close(&h);
}
FTEST(fft_release_balance_on_stream) {
    /* every fragment obtained for a table goes back to the stream when the table is released */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); LONG before, during, after; ffc_tab t; if (!face) return;
    before = dwx_loader_total_outstanding(h.ld);
    t = ffc_get_tab(face, DW_TAG('g','l','y','f'));
    during = dwx_loader_total_outstanding(h.ld);
    CHECK(t.hr == S_OK && t.exists);
    if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    after = dwx_loader_total_outstanding(h.ld);
    t_note("outstanding fragments: before %ld, while held %ld, after release %ld", (long)before, (long)during, (long)after);
    CHECK(after == before);
    CHECK(during >= before);
    dwx_font_close(&h);
}
FTEST(fft_release_balance_many_cycles) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); LONG before, after; int i; if (!face) return;
    (void)ffc_get_tab(face, 0);   /* warm any internal caches */
    before = dwx_loader_total_outstanding(h.ld);
    for (i = 0; i < 200; i++) { ffc_tab t = ffc_get_tab(face, DW_TAG('c','m','a','p')); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    after = dwx_loader_total_outstanding(h.ld);
    CHECKF(after == before, "outstanding %ld -> %ld", (long)before, (long)after);
    dwx_font_close(&h);
}
FTEST(fft_stream_reads_recorded_in_bounds) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ffc_tab t; dwx_mem_stream *st; int i; if (!face) return;
    t = ffc_get_tab(face, DW_TAG('g','l','y','f'));
    if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    CHECK(h.ld->nstreams >= 1);
    st = h.ld->last_stream;
    if (st) for (i = 0; i < st->nlogged; i++) CHECKF(st->reads[i].offset + st->reads[i].size <= st->size, "read %d beyond the stream: %llu+%llu > %llu", i, (unsigned long long)st->reads[i].offset, (unsigned long long)st->reads[i].size, (unsigned long long)st->size);
    dwx_font_close(&h);
}
FTEST(fft_stream_fragment_copy_mode) {
    /* streams that hand out private copies of fragments (non-memory-mapped sources): the table bytes are still right and releases pair up */
    ttf_spec s; size_t n; uint8_t *b; dwx_font h; IDWriteFontFace *face; ffc_tab t; unsigned wl = 0; const uint8_t *want; HRESULT hr;
    ttf_default_spec(&s); b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    memset(&h, 0, sizeof h); h.f = f; h.ld = dwx_loader_new(); h.ld->stream_copy_fragments = 1; h.key = dwx_loader_add(h.ld, b, n);
    hr = dwx_loader_register(h.ld, f); if (SUCCEEDED(hr)) hr = dwx_loader_file(h.ld, f, h.key, &h.file);
    if (SUCCEEDED(hr)) hr = IDWriteFactory_CreateFontFace(f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &h.file, 0, DWRITE_FONT_SIMULATIONS_NONE, &h.face);
    if (FAILED(hr) || !h.face) { t_skip("setup %s", dw_hr_name(hr)); dwx_font_close(&h); free(b); return; }
    face = h.face; want = ffc_blob_table(b, n, "name", &wl);
    t = ffc_get_tab(face, DW_TAG('n','a','m','e'));
    CHECK(t.hr == S_OK && t.exists);
    if (t.exists && want) { EXPECT_EQ(t.size, wl); if (t.size == wl) CHECK(!memcmp(t.data, want, wl)); }
    if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    EXPECT_EQ(h.ld->streams[0]->bad_release_ctx, 0);
    dwx_font_close(&h); free(b);
}
FTEST(fft_stream_read_failure_surfaces) {
    /* a stream whose reads fail after the face exists: TryGetFontTable must not crash; record the outcome */
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ffc_tab t; int i; if (!face) return;
    for (i = 0; i < h.ld->nstreams; i++) { h.ld->streams[i]->fail_read = E_FAIL; h.ld->streams[i]->fail_read_from = h.ld->streams[i]->n_read; }
    t = ffc_get_tab(face, DW_TAG('g','l','y','f'));
    t_note("stream reads failing: hr %s exists %d", dw_hr_name(t.hr), (int)t.exists);
    if (t.hr == S_OK && t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    for (i = 0; i < h.ld->nstreams; i++) h.ld->streams[i]->fail_read = 0;
    dwx_font_close(&h);
}

/* ---- faces of all kinds ---------------------------------------------------------------------------------------------------------------------- */
FTEST(fft_independent_of_simulations) {
    int sm;
    for (sm = 0; sm < 4; sm++) {
        ttf_spec s; size_t n; uint8_t *b; const uint8_t *want; unsigned wl = 0; dwx_font h; IDWriteFontFace *face; ffc_tab t;
        ttf_default_spec(&s); b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
        want = ffc_blob_table(b, n, "head", &wl);
        face = ffc_open(&h, f, &s, (DWRITE_FONT_SIMULATIONS)sm); if (!face) { free(b); return; }
        t = ffc_get_tab(face, DW_TAG('h','e','a','d'));
        CHECK(t.hr == S_OK && t.exists);
        if (t.exists) { EXPECT_EQ(t.size, wl); if (t.size == wl) CHECKF(!memcmp(t.data, want, wl), "sim %d", sm); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
        dwx_font_close(&h); free(b);
    }
}
FTEST(fft_collection_member_tables) {
    size_t n1, n2, tn; uint8_t *b1 = ttf_build_styled("FftTtcOne", 400, 5, 0, &n1), *b2 = ttf_build_styled("FftTtcTwoLonger", 700, 5, 1, &n2); const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &tn); UINT32 k;
    if (!ttc) { t_skip("ttc"); free(b1); free(b2); return; }
    for (k = 0; k < 2; k++) {
        dwx_font h; HRESULT hr = dwx_font_open(&h, f, ttc, tn, k, DWRITE_FONT_SIMULATIONS_NONE); const uint8_t *want; unsigned wl = 0; ffc_tab t; static const char *tg[3] = { "name", "head", "cmap" }; int i;
        if (FAILED(hr) || !h.face) { t_skip("ttc open %u: %s", k, dw_hr_name(hr)); dwx_font_close(&h); break; }
        for (i = 0; i < 3; i++) {
            want = ffc_blob_table(k ? b2 : b1, k ? n2 : n1, tg[i], &wl);
            t = ffc_get_tab(h.face, DW_TAG(tg[i][0], tg[i][1], tg[i][2], tg[i][3]));
            CHECKF(t.hr == S_OK && t.exists, "face %u %s", k, tg[i]);
            if (t.exists && want) { CHECKF(t.size == wl, "face %u %s size %u want %u", k, tg[i], t.size, wl); if (t.size == wl) CHECKF(!memcmp(t.data, want, wl), "face %u %s bytes", k, tg[i]); }
            if (t.exists) IDWriteFontFace_ReleaseFontTable(h.face, t.ctx);
        }
        dwx_font_close(&h);
    }
    free(b1); free(b2); free(ttc);
}
FTEST(fft_collection_member_tables_differ_between_faces) {
    size_t n1, n2, tn; uint8_t *b1 = ttf_build_styled("FftAlpha", 400, 5, 0, &n1), *b2 = ttf_build_styled("FftBetaBetaBeta", 700, 5, 0, &n2); const uint8_t *bl[2] = { b1, b2 }; size_t sz[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(bl, sz, 2, &tn); dwx_font h0, h1; HRESULT hr0, hr1; ffc_tab a, b;
    if (!ttc) { t_skip("ttc"); free(b1); free(b2); return; }
    hr0 = dwx_font_open(&h0, f, ttc, tn, 0, DWRITE_FONT_SIMULATIONS_NONE); hr1 = dwx_font_open(&h1, f, ttc, tn, 1, DWRITE_FONT_SIMULATIONS_NONE);
    if (SUCCEEDED(hr0) && SUCCEEDED(hr1) && h0.face && h1.face) {
        a = ffc_get_tab(h0.face, DW_TAG('n','a','m','e')); b = ffc_get_tab(h1.face, DW_TAG('n','a','m','e'));
        CHECK(a.exists && b.exists);
        if (a.exists && b.exists) CHECK(a.size != b.size || memcmp(a.data, b.data, a.size));
        if (a.exists) IDWriteFontFace_ReleaseFontTable(h0.face, a.ctx);
        if (b.exists) IDWriteFontFace_ReleaseFontTable(h1.face, b.ctx);
    } else t_skip("ttc open failed");
    dwx_font_close(&h1); dwx_font_close(&h0);
    free(b1); free(b2); free(ttc);
}
FTEST(fft_after_creation_file_released) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ffc_tab t; if (!face) return;
    DW_RELEASE(h.file);
    t = ffc_get_tab(face, DW_TAG('h','h','e','a'));
    CHECK(t.hr == S_OK && t.exists);
    if (t.exists) { EXPECT_EQ(t.size, 36); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    dwx_font_close(&h);
}
FTEST(fft_fixed_table_sizes) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ffc_tab t; if (!face) return;
    t = ffc_get_tab(face, DW_TAG('h','e','a','d')); CHECK(t.exists); if (t.exists) { EXPECT_EQ(t.size, 54); if (t.size >= 20) { EXPECT_EQ(ffc_be32((const uint8_t *)t.data + 12), 0x5F0F3CF5); EXPECT_EQ(ffc_be16((const uint8_t *)t.data + 18), 2048); } IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    t = ffc_get_tab(face, DW_TAG('h','h','e','a')); CHECK(t.exists); if (t.exists) { EXPECT_EQ(t.size, 36); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    t = ffc_get_tab(face, DW_TAG('m','a','x','p')); CHECK(t.exists); if (t.exists) { CHECK(t.size == 32 || t.size == 6); if (t.size >= 6) EXPECT_EQ(ffc_be16((const uint8_t *)t.data + 4), TTF_DEF_NGLYPHS); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    dwx_font_close(&h);
}
FTEST(fft_maxp_matches_glyph_count) {
    dwx_font h; IDWriteFontFace *face = ffc_open_def(&h, f); ffc_tab t; if (!face) return;
    t = ffc_get_tab(face, DW_TAG('m','a','x','p')); CHECK(t.exists);
    if (t.exists && t.size >= 6) EXPECT_EQ(ffc_be16((const uint8_t *)t.data + 4), IDWriteFontFace_GetGlyphCount(face));
    if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    dwx_font_close(&h);
}

/* ---- damaged directories ---------------------------------------------------------------------------------------------------------------------- */
static int ffc_dir_index(const uint8_t *b, size_t n, const char *tag) {
    unsigned nt = ffc_be16(b + 4), i;
    for (i = 0; i < nt && 12 + 16 * (i + 1) <= n; i++) if (!memcmp(b + 12 + 16 * i, tag, 4)) return (int)i;
    return -1;
}
static void ffc_damaged_case(IDWriteFactory *f, int kind, const char *tag, size_t param_extra) {
    ttf_spec s; size_t n, cn; uint8_t *b, *c; dwx_font h; HRESULT hr; int idx; ffc_tab t;
    ttf_default_spec(&s); b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    idx = ffc_dir_index(b, n, tag); if (idx < 0) { free(b); t_skip("no %s", tag); return; }
    c = ttf_corrupt(b, n, kind, (size_t)idx + param_extra, &cn);
    hr = dwx_font_open(&h, f, c, cn, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (SUCCEEDED(hr) && h.face) {
        t = ffc_get_tab(h.face, DW_TAG(tag[0], tag[1], tag[2], tag[3]));
        t_note("kind %d on '%s': TryGetFontTable hr %s exists %d size %u", kind, tag, dw_hr_name(t.hr), (int)t.exists, t.size);
        if (t.hr == S_OK) { CHECK(t.exists == TRUE || t.exists == FALSE); if (!t.exists) CHECK(t.data == NULL && t.size == 0 && t.ctx == NULL); }
        if (t.hr == S_OK && t.exists) IDWriteFontFace_ReleaseFontTable(h.face, t.ctx);
    } else t_note("kind %d on '%s': face not created (%s)", kind, tag, dw_hr_name(hr));
    dwx_font_close(&h); free(b); free(c);
}
FTEST(fft_damaged_offset_beyond_file) { ffc_damaged_case(f, TTF_CORRUPT_TABLE_OFFSET, "name", 0); }
FTEST(fft_damaged_length_beyond_file) { ffc_damaged_case(f, TTF_CORRUPT_TABLE_LENGTH, "name", 0); }
FTEST(fft_damaged_checksum_flipped) {
    /* table checksums are not verified when serving a table */
    ttf_spec s; size_t n, cn; uint8_t *b, *c; dwx_font h; HRESULT hr; int idx; ffc_tab t; unsigned wl = 0; const uint8_t *want;
    ttf_default_spec(&s); b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    idx = ffc_dir_index(b, n, "hhea"); c = ttf_corrupt(b, n, TTF_CORRUPT_TABLE_CHECKSUM, (size_t)idx, &cn);
    hr = dwx_font_open(&h, f, c, cn, 0, DWRITE_FONT_SIMULATIONS_NONE);
    if (SUCCEEDED(hr) && h.face) {
        t = ffc_get_tab(h.face, DW_TAG('h','h','e','a')); want = ffc_blob_table(b, n, "hhea", &wl);
        t_note("flipped hhea checksum: hr %s exists %d", dw_hr_name(t.hr), (int)t.exists);
        CHECK(t.hr == S_OK && t.exists);
        if (t.exists) { EXPECT_EQ(t.size, wl); if (t.size == wl) CHECK(!memcmp(t.data, want, wl)); IDWriteFontFace_ReleaseFontTable(h.face, t.ctx); }
    } else t_note("face with flipped checksum not created: %s", dw_hr_name(hr));
    dwx_font_close(&h); free(b); free(c);
}
FTEST(fft_damaged_zeroed_name_table) { ffc_damaged_case(f, TTF_CORRUPT_ZERO_FILL, "name", 0); }
FTEST(fft_damaged_zero_length_table) { ffc_damaged_case(f, TTF_CORRUPT_ZERO_TABLE, "post", 0); }

/* ---- concurrency -------------------------------------------------------------------------------------------------------------------------------- */
typedef struct { IDWriteFontFace *face; const uint8_t *want; unsigned wl; UINT32 tag; int bad; } fft_thr;
static DWORD WINAPI fft_thread(LPVOID p) {
    fft_thr *t = (fft_thr *)p; int i;
    for (i = 0; i < 200; i++) {
        const void *d = NULL; UINT32 sz = 0; void *ctx = NULL; BOOL ex = FALSE;
        if (FAILED(IDWriteFontFace_TryGetFontTable(t->face, t->tag, &d, &sz, &ctx, &ex)) || !ex) { t->bad++; continue; }
        if (sz != t->wl || memcmp(d, t->want, sz)) t->bad++;
        IDWriteFontFace_ReleaseFontTable(t->face, ctx);
    }
    return 0;
}
FTEST(fft_concurrent_retrieval) {
    ttf_spec s; size_t n; uint8_t *b; dwx_font h; IDWriteFontFace *face; HANDLE th[4]; fft_thr t[4]; static const char *tg[4] = { "cmap", "glyf", "name", "hmtx" }; int i;
    ttf_default_spec(&s); b = ttf_build(&s, &n); if (!b) { t_skip("build"); return; }
    face = ffc_open(&h, f, &s, DWRITE_FONT_SIMULATIONS_NONE); if (!face) { free(b); return; }
    for (i = 0; i < 4; i++) { t[i].face = face; t[i].want = ffc_blob_table(b, n, tg[i], &t[i].wl); t[i].tag = DW_TAG(tg[i][0], tg[i][1], tg[i][2], tg[i][3]); t[i].bad = 0; th[i] = CreateThread(NULL, 0, fft_thread, &t[i], 0, NULL); }
    for (i = 0; i < 4; i++) if (th[i]) { CHECK(WaitForSingleObject(th[i], 20000) == WAIT_OBJECT_0); CloseHandle(th[i]); }
    for (i = 0; i < 4; i++) EXPECT_EQ(t[i].bad, 0);
    dwx_font_close(&h); free(b);
}

/* ---- system fonts -------------------------------------------------------------------------------------------------------------------------------- */
FTEST(fft_sys_arial_core_tables) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); static const char *tg[] = { "head", "hhea", "maxp", "OS/2", "hmtx", "cmap", "loca", "glyf", "name", "post" }; int i;
    if (!face) { t_skip("Arial not installed"); return; }
    for (i = 0; i < 10; i++) {
        ffc_tab t = ffc_get_tab(face, DW_TAG(tg[i][0], tg[i][1], tg[i][2], tg[i][3]));
        CHECKF(t.hr == S_OK && t.exists && t.size > 0 && t.data, "Arial %s", tg[i]);
        if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    }
    IDWriteFontFace_Release(face);
}
FTEST(fft_sys_arial_head_magic_and_maxp_count) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); UINT32 hl = 0, ml = 0; uint8_t *head, *maxp;
    if (!face) { t_skip("Arial not installed"); return; }
    head = dw_face_table(face, DW_TAG('h','e','a','d'), &hl); maxp = dw_face_table(face, DW_TAG('m','a','x','p'), &ml);
    CHECK(head && hl >= 54); CHECK(maxp && ml >= 6);
    if (head && hl >= 54) { EXPECT_EQ(ffc_be32(head + 12), 0x5F0F3CF5); EXPECT_EQ(ffc_be16(head + 18), 2048); }
    if (maxp && ml >= 6) EXPECT_EQ(ffc_be16(maxp + 4), IDWriteFontFace_GetGlyphCount(face));
    free(head); free(maxp); IDWriteFontFace_Release(face);
}
FTEST(fft_sys_arial_layout_tables_and_absent_tags) {
    IDWriteFontFace *face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); static const char *tg[] = { "GSUB", "GPOS", "GDEF", "gasp", "kern", "cvt ", "fpgm", "prep", "VDMX", "hdmx", "LTSH" }; int i;
    if (!face) { t_skip("Arial not installed"); return; }
    for (i = 0; i < 11; i++) {
        ffc_tab t = ffc_get_tab(face, DW_TAG(tg[i][0], tg[i][1], tg[i][2], tg[i][3]));
        CHECKF(t.hr == S_OK, "Arial %s hr %s", tg[i], dw_hr_name(t.hr));
        t_note("Arial table '%s': %s (%u bytes)", tg[i], t.exists ? "present" : "absent", t.exists ? t.size : 0);
        if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
    }
    { ffc_tab t = ffc_get_tab(face, DW_TAG('f','v','a','r')); CHECK(t.hr == S_OK); CHECK(!t.exists); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    IDWriteFontFace_Release(face);
}
FTEST(fft_sys_variable_font_tables) {
    IDWriteFontFace *face = dw_sysface(f, L"Bahnschrift", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); static const char *tg[] = { "fvar", "avar", "STAT", "gvar" }; int i;
    if (!face) { t_skip("Bahnschrift not installed"); return; }
    for (i = 0; i < 4; i++) { ffc_tab t = ffc_get_tab(face, DW_TAG(tg[i][0], tg[i][1], tg[i][2], tg[i][3])); CHECKF(t.hr == S_OK && t.exists, "Bahnschrift %s", tg[i]); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    IDWriteFontFace_Release(face);
}
FTEST(fft_sys_color_font_tables) {
    IDWriteFontFace *face = dw_sysface(f, L"Segoe UI Emoji", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); static const char *tg[] = { "COLR", "CPAL" }; int i;
    if (!face) { t_skip("Segoe UI Emoji not installed"); return; }
    for (i = 0; i < 2; i++) { ffc_tab t = ffc_get_tab(face, DW_TAG(tg[i][0], tg[i][1], tg[i][2], tg[i][3])); t_note("Segoe UI Emoji %s: %s", tg[i], t.exists ? "present" : "absent"); CHECK(t.hr == S_OK); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
    IDWriteFontFace_Release(face);
}
FTEST(fft_sys_ttc_member_tables) {
    char path[MAX_PATH]; const char *cand[] = { "msgothic.ttc", NULL }; WCHAR wp[MAX_PATH]; IDWriteFontFile *file = NULL; BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf = 0, i;
    if (!dw_find_system_font(cand, path, sizeof path)) { t_skip("msgothic.ttc not found"); return; }
    dw_wide(path, wp, MAX_PATH);
    DW_REQUIRE_OK(IDWriteFactory_CreateFontFileReference(f, wp, NULL, &file));
    CHECK_HR_OK(IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf));
    for (i = 0; i < nf && i < 3; i++) {
        IDWriteFontFace *face = NULL; ffc_tab t;
        if (FAILED(IDWriteFactory_CreateFontFace(f, fc, 1, &file, i, DWRITE_FONT_SIMULATIONS_NONE, &face)) || !face) continue;
        t = ffc_get_tab(face, DW_TAG('h','e','a','d')); CHECKF(t.hr == S_OK && t.exists, "msgothic face %u head", i); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
        t = ffc_get_tab(face, DW_TAG('n','a','m','e')); CHECKF(t.hr == S_OK && t.exists, "msgothic face %u name", i); if (t.exists) IDWriteFontFace_ReleaseFontTable(face, t.ctx);
        IDWriteFontFace_Release(face);
    }
    DW_RELEASE(file);
}
FTEST(fft_sys_tables_match_file_bytes) {
    /* the table pointer holds the bytes of the font file: compare Arial's 'head' with the file read through the plain Win32 API */
    char path[MAX_PATH]; const char *cand[] = { "arial.ttf", NULL }; IDWriteFontFace *face; HANDLE fh; uint8_t dir[12 + 16 * 40]; DWORD got = 0; unsigned nt, i, off = 0, len = 0;
    if (!dw_find_system_font(cand, path, sizeof path)) { t_skip("arial.ttf not found"); return; }
    face = dw_sysface(f, L"Arial", 400, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL); if (!face) { t_skip("Arial not installed"); return; }
    fh = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (fh == INVALID_HANDLE_VALUE) { t_skip("cannot open arial.ttf"); IDWriteFontFace_Release(face); return; }
    if (ReadFile(fh, dir, sizeof dir, &got, NULL) && got >= 12) {
        nt = ffc_be16(dir + 4);
        for (i = 0; i < nt && 12 + 16 * i + 16 <= got; i++) if (!memcmp(dir + 12 + 16 * i, "head", 4)) { off = ffc_be32(dir + 12 + 16 * i + 8); len = ffc_be32(dir + 12 + 16 * i + 12); }
        if (off && len == 54) {
            uint8_t file_head[54]; DWORD g2 = 0; ffc_tab t;
            SetFilePointer(fh, off, NULL, FILE_BEGIN);
            if (ReadFile(fh, file_head, 54, &g2, NULL) && g2 == 54) {
                t = ffc_get_tab(face, DW_TAG('h','e','a','d'));
                CHECK(t.hr == S_OK && t.exists);
                if (t.exists) { EXPECT_EQ(t.size, 54); if (t.size == 54) CHECK(!memcmp(t.data, file_head, 54)); IDWriteFontFace_ReleaseFontTable(face, t.ctx); }
            }
        } else t_note("head not found in arial.ttf directory (off %u len %u)", off, len);
    }
    CloseHandle(fh); IDWriteFontFace_Release(face);
}
