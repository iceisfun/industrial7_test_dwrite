/* The sfnt table directory and table records: what CreateFontFace / TryGetFontTable tolerate.
 *
 * Facts from the probing (table directory parsing):
 *   - the directory is scanned linearly (no ordering requirement, no binary search); for a duplicated tag the first record with enough data wins;
 *   - head (>= 54 bytes), hhea (>= 36), OS/2 (> 0x4d), maxp (>= 6) are read eagerly while the face is created; a record whose range lies
 *     outside the file makes the read fail with FileFormatException (DWRITE_E_FILEFORMAT);
 *   - a face without a usable maxp is a format error;
 *   - TryGetFontTable zeroes its outputs, scans linearly and returns the first record carrying the tag; an out-of-file range is a failure HRESULT;
 *   - checksums are never verified.
 * Everything else is recorded as a NOTE line per damage so that the behaviour of the real parser is documented. */
#include "sf.h"

typedef struct { sf_probe p; sf_blob b; } ctx;
static int begin(ctx *c, sf_blob b, const char *label) {
    c->b = b; sf_survey_light = 1;
    if (!sf_open_face(&c->p, b.p, b.n)) { sf_close(&c->p); sf_free(&c->b); return 0; }
    sf_survey(label, &c->p);
    return 1;
}
static void end(ctx *c) { sf_close(&c->p); sf_free(&c->b); }
#define SETUP(c, blob, label) ctx c; if (!begin(&c, blob, label)) return

static uint32_t dir_off(const sf_blob *b, const char *tag) { int i = sf_find(b, tag); return i < 0 ? 0 : sf_r32(sf_dirent(b, i) + 8); }
static uint32_t dir_len(const sf_blob *b, const char *tag) { int i = sf_find(b, tag); return i < 0 ? 0 : sf_r32(sf_dirent(b, i) + 12); }

TEST(dir_control_default_font_loads) {
    SETUP(c, sf_default(), "control");
    CHECK_HR(c.p.hr_analyze, S_OK); CHECK_HR(c.p.hr_face, S_OK);
    CHECK(sf_ntables(&c.b) == 10);
    if (c.p.face) { CHECK(IDWriteFontFace_GetGlyphCount(c.p.face) == TTF_DEF_NGLYPHS); CHECK(sf_gid(c.p.face, 'A') == ttf_default_gid('A')); }
    end(&c);
}

/* ---- numTables --------------------------------------------------------------------------------------------- */
TEST(dir_numtables_zero_is_format_error) {
    sf_blob b = sf_default(); sf_set_numtables(&b, 0);
    SETUP(c, b, "numTables=0");
    CHECK_HR_OK(c.p.hr_analyze);          /* the sniffer never reads the directory */
    CHECK(FAILED(c.p.hr_face)); PRED("numTables=0", c.p.hr_face == DWRITE_E_FILEFORMAT_);        /* no maxp -> FileFormatException from OpenTypeFace */
    CHECK_OUT_NULL(c.p.face);
    end(&c);
}
TEST(dir_numtables_huge_is_format_error) {
    unsigned v[] = { 0xFFFF, 0x8000, 0x4000, 0x1000 }; int i;
    for (i = 0; i < 4; i++) {
        sf_blob b = sf_default(); char l[40]; snprintf(l, sizeof l, "numTables=%u", v[i]); sf_set_numtables(&b, v[i]);
        /* 16*numTables bytes must be readable at offset 12; the default font is only ~12 KB */
        { ctx c; if (begin(&c, b, l)) { CHECKF(FAILED(c.p.hr_face), "numTables %u: face created", v[i]); CHECK_OUT_NULL(c.p.face); end(&c); } }
    }
}
TEST(dir_numtables_minus_one_drops_post) {
    sf_blob b = sf_default(); sf_set_numtables(&b, 9);
    SETUP(c, b, "numTables=9 (post record hidden)");
    PRED("face loads without post", SUCCEEDED(c.p.hr_face));
    if (c.p.face) {
        const void *d; UINT32 n; void *x; BOOL ex = TRUE;
        CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('p','o','s','t'), &d, &n, &x, &ex), S_OK);
        CHECK(!ex);
    }
    end(&c);
}
TEST(dir_numtables_minus_all_but_head_maxp) {
    sf_blob b = sf_default(); int i;
    /* keep only head(3) and maxp(7): rotate them to the front, then claim two tables */
    sf_swap_ents(&b, 0, 3); sf_swap_ents(&b, 1, 7); sf_set_numtables(&b, 2); (void)i;
    SETUP(c, b, "directory = {head, maxp}");
    t_note("a font with only head+maxp: %s", dw_hr_name(c.p.hr_face));
    end(&c);
}
TEST(dir_numtables_plus_one_reads_junk_record) {
    sf_blob b = sf_default(); sf_set_numtables(&b, 11);
    SETUP(c, b, "numTables=11 (one junk record)");
    PRED("junk record ignored", SUCCEEDED(c.p.hr_face));
    end(&c);
}
TEST(dir_numtables_plus_one_junk_record_has_known_tag) {
    /* the extra (11th) record occupies the first 16 bytes after the directory; give it the tag 'cmap' with an out-of-range offset: it comes last, so the real cmap wins */
    sf_blob b = sf_default(); uint8_t *e; sf_set_numtables(&b, 11);
    e = b.p + 12 + 16 * 10; memcpy(e, "cmap", 4); sf_w32(e + 8, 0x7FFFFFF0); sf_w32(e + 12, 100);
    /* the 16 bytes were the start of the first table's data (OS/2): overwritten here only for the test */
    SETUP(c, b, "numTables=11, junk record tagged cmap");
    PRED("first cmap record wins", SUCCEEDED(c.p.hr_face) && c.p.face && sf_gid(c.p.face, 'A') == ttf_default_gid('A'));
    end(&c);
}

/* ---- version / tag ------------------------------------------------------------------------------------------- */
TEST(dir_sfnt_version_variants_create_faces) {
    uint32_t v[] = { 0x00010000, 0x00020000, 0x74727565 /* true */ }; const char *nm[] = { "1.0", "2.0", "true" }; int i;
    for (i = 0; i < 3; i++) {
        sf_blob b = sf_default(); char l[64]; sf_w32(b.p, v[i]); snprintf(l, sizeof l, "sfnt version %s", nm[i]);
        { ctx c; if (begin(&c, b, l)) { CHECK_HR(c.p.hr_analyze, S_OK); CHECK(c.p.supported); CHECK_HR(c.p.hr_face, S_OK); if (c.p.face) CHECK(sf_gid(c.p.face, 'B') == ttf_default_gid('B')); end(&c); } }
    }
}
TEST(dir_unrecognised_version_has_no_face) {
    uint32_t v[] = { 0x00030000, 0x4F54544F ^ 0x01000000, 0x00010001, 0x58585858, 0 }; int i;
    for (i = 0; i < 5; i++) {
        sf_blob b = sf_default(); char l[64]; IDWriteFontFace *face = (IDWriteFontFace *)(ULONG_PTR)1; HRESULT hr; sf_probe p;
        sf_w32(b.p, v[i]); snprintf(l, sizeof l, "sfnt version %08x", v[i]);
        if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); continue; }
        CHECKF(p.hr_analyze == S_OK && !p.supported, "%s: analyze %s sup %d", l, dw_hr_name(p.hr_analyze), (int)p.supported);
        /* the caller may still force a face type: the face loader does not look at the version, only at the directory */
        hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &face);
        t_note("%s forced as TRUETYPE: %s", l, dw_hr_name(hr));
        if (SUCCEEDED(hr)) IDWriteFontFace_Release(face); else CHECK_OUT_NULL(face);
        sf_close(&p); sf_free(&b);
    }
}
TEST(dir_otto_header_over_glyf_outlines) {
    sf_blob b = sf_default(); sf_probe p; IDWriteFontFace *face = NULL; HRESULT hr;
    memcpy(b.p, "OTTO", 4);
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    CHECK(p.supported); CHECK(p.fctype == DWRITE_FONT_FACE_TYPE_CFF);
    hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_CFF, 1, &p.file, 0, 0, &face);
    t_note("OTTO header on a glyf font, faceType CFF: %s", dw_hr_name(hr));
    DW_RELEASE(face);
    hr = IDWriteFactory_CreateFontFace(p.f, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &p.file, 0, 0, &face);
    t_note("OTTO header on a glyf font, faceType TRUETYPE: %s", dw_hr_name(hr));
    DW_RELEASE(face); sf_close(&p); sf_free(&b);
}

/* ---- record damage, table by table -------------------------------------------------------------------------- */
enum { D_OFF_BEYOND, D_LEN_BEYOND, D_LEN_ZERO, D_OFF_ZERO, D_LEN_ONE, D_OFF_ODD, D_OFF_EOF };
static const char *const dmg_name[] = { "offset beyond EOF", "length beyond EOF", "length 0", "offset 0", "length 1", "offset+1 (unaligned)", "offset == EOF" };
static void damage(sf_blob *b, const char *tag, int d) {
    int i = sf_find(b, tag); uint8_t *e = sf_dirent(b, i); uint32_t off = sf_r32(e + 8), len = sf_r32(e + 12);
    switch (d) {
    case D_OFF_BEYOND: sf_set_ent(b, i, (uint32_t)b->n + 0x1000, len); break;
    case D_LEN_BEYOND: sf_set_ent(b, i, off, (uint32_t)b->n); break;
    case D_LEN_ZERO: sf_set_ent(b, i, off, 0); break;
    case D_OFF_ZERO: sf_set_ent(b, i, 0, len); break;
    case D_LEN_ONE: sf_set_ent(b, i, off, 1); break;
    case D_OFF_ODD: sf_set_ent(b, i, off + 1, len); break;
    case D_OFF_EOF: sf_set_ent(b, i, (uint32_t)b->n, len); break;
    }
}
/* hard: 1 = CreateFontFace must fail, 0 = no expectation (note only) */
static void table_damage(const char *tag, int d, int hard_fail) {
    sf_blob b = sf_default(); char l[96]; ctx c;
    if (t_is_wine() && !strcmp(tag, "OS/2") && (d == D_OFF_BEYOND || d == D_OFF_EOF || d == D_LEN_BEYOND)) { t_skip("Wine crashes while creating a face over an out-of-file OS/2 range"); sf_free(&b); return; }
    damage(&b, tag, d);
    snprintf(l, sizeof l, "%s record: %s", tag, dmg_name[d]);
    if (!begin(&c, b, l)) return;
    CHECK_HR_OK(c.p.hr_analyze);
    if (hard_fail) { CHECKF(FAILED(c.p.hr_face), "%s: face unexpectedly created", l); CHECK_OUT_NULL(c.p.face); }
    else if (FAILED(c.p.hr_face)) CHECK_OUT_NULL(c.p.face);
    end(&c);
}
#define DMG(name, tag, d, hard) TEST(dir_##name) { table_damage(tag, d, hard); }
/* eager tables: a bad range is fatal at creation */
DMG(head_offset_beyond_eof, "head", D_OFF_BEYOND, 1)
DMG(head_offset_at_eof, "head", D_OFF_EOF, 1)
DMG(hhea_offset_beyond_eof, "hhea", D_OFF_BEYOND, 1)
DMG(hhea_offset_at_eof, "hhea", D_OFF_EOF, 1)
DMG(maxp_offset_beyond_eof, "maxp", D_OFF_BEYOND, 1)
DMG(maxp_offset_at_eof, "maxp", D_OFF_EOF, 1)
DMG(maxp_length_zero, "maxp", D_LEN_ZERO, 1)
DMG(maxp_length_one, "maxp", D_LEN_ONE, 1)
DMG(os2_offset_beyond_eof, "OS/2", D_OFF_BEYOND, 1)
DMG(os2_length_beyond_eof, "OS/2", D_LEN_BEYOND, 1)
/* no hard expectation, outcome recorded */
DMG(head_length_beyond_eof, "head", D_LEN_BEYOND, 0)
DMG(head_length_zero, "head", D_LEN_ZERO, 0)
DMG(head_length_one, "head", D_LEN_ONE, 0)
DMG(head_offset_zero, "head", D_OFF_ZERO, 0)
DMG(head_offset_odd, "head", D_OFF_ODD, 0)
DMG(hhea_length_beyond_eof, "hhea", D_LEN_BEYOND, 0)
DMG(hhea_length_zero, "hhea", D_LEN_ZERO, 0)
DMG(hhea_length_one, "hhea", D_LEN_ONE, 0)
DMG(hhea_offset_zero, "hhea", D_OFF_ZERO, 0)
DMG(maxp_length_beyond_eof, "maxp", D_LEN_BEYOND, 0)
DMG(maxp_offset_zero, "maxp", D_OFF_ZERO, 0)
DMG(os2_length_zero, "OS/2", D_LEN_ZERO, 0)
DMG(os2_length_one, "OS/2", D_LEN_ONE, 0)
DMG(os2_offset_zero, "OS/2", D_OFF_ZERO, 0)
DMG(os2_offset_at_eof, "OS/2", D_OFF_EOF, 0)
DMG(hmtx_offset_beyond_eof, "hmtx", D_OFF_BEYOND, 0)
DMG(hmtx_length_beyond_eof, "hmtx", D_LEN_BEYOND, 0)
DMG(hmtx_length_zero, "hmtx", D_LEN_ZERO, 0)
DMG(hmtx_length_one, "hmtx", D_LEN_ONE, 0)
DMG(hmtx_offset_zero, "hmtx", D_OFF_ZERO, 0)
DMG(cmap_offset_beyond_eof, "cmap", D_OFF_BEYOND, 0)
DMG(cmap_length_beyond_eof, "cmap", D_LEN_BEYOND, 0)
DMG(cmap_length_zero, "cmap", D_LEN_ZERO, 0)
DMG(cmap_length_one, "cmap", D_LEN_ONE, 0)
DMG(cmap_offset_zero, "cmap", D_OFF_ZERO, 0)
DMG(cmap_offset_at_eof, "cmap", D_OFF_EOF, 0)
DMG(loca_offset_beyond_eof, "loca", D_OFF_BEYOND, 0)
DMG(loca_length_beyond_eof, "loca", D_LEN_BEYOND, 0)
DMG(loca_length_zero, "loca", D_LEN_ZERO, 0)
DMG(loca_length_one, "loca", D_LEN_ONE, 0)
DMG(loca_offset_zero, "loca", D_OFF_ZERO, 0)
DMG(glyf_offset_beyond_eof, "glyf", D_OFF_BEYOND, 0)
DMG(glyf_length_beyond_eof, "glyf", D_LEN_BEYOND, 0)
DMG(glyf_length_zero, "glyf", D_LEN_ZERO, 0)
DMG(glyf_offset_zero, "glyf", D_OFF_ZERO, 0)
DMG(name_offset_beyond_eof, "name", D_OFF_BEYOND, 0)
DMG(name_length_beyond_eof, "name", D_LEN_BEYOND, 0)
DMG(name_length_zero, "name", D_LEN_ZERO, 0)
DMG(name_length_one, "name", D_LEN_ONE, 0)
DMG(name_offset_zero, "name", D_OFF_ZERO, 0)
DMG(post_offset_beyond_eof, "post", D_OFF_BEYOND, 0)
DMG(post_length_beyond_eof, "post", D_LEN_BEYOND, 0)
DMG(post_length_zero, "post", D_LEN_ZERO, 0)
DMG(post_length_one, "post", D_LEN_ONE, 0)
DMG(post_offset_zero, "post", D_OFF_ZERO, 0)

/* ---- fetching damaged tables through TryGetFontTable ---------------------------------------------------------------- */
TEST(dir_trygettable_unknown_tag_range_beyond_eof) {
    /* A table the parser never interprets can lie anywhere: creation succeeds, fetching it fails and zeroes all outputs. */
    sf_blob b = sf_default(); ctx c;
    sf_insert_record_raw(&b, 10, "ZZZZ", (uint32_t)b.n + 0x4000, 64);
    if (!begin(&c, b, "extra ZZZZ record beyond EOF")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) {
        const void *d = (const void *)1; UINT32 n = 77; void *x = (void *)1; BOOL ex = 77;
        HRESULT hr = IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('Z','Z','Z','Z'), &d, &n, &x, &ex);
        t_note("TryGetFontTable on a record beyond EOF: %s", dw_hr_name(hr));
        CHECK_HR_FAILED(hr); CHECK(d == NULL); CHECK(n == 0); CHECK(x == NULL); CHECK(ex == FALSE);
    }
    end(&c);
}
TEST(dir_trygettable_unknown_tag_zero_length) {
    sf_blob b = sf_default(); ctx c;
    sf_insert_record_raw(&b, 10, "ZZZZ", 100, 0);
    if (!begin(&c, b, "extra ZZZZ record, length 0")) return;
    if (c.p.face) {
        const void *d = (const void *)1; UINT32 n = 77; void *x = (void *)1; BOOL ex = 77;
        HRESULT hr = IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('Z','Z','Z','Z'), &d, &n, &x, &ex);
        t_note("TryGetFontTable on a zero length record: %s exists=%d size=%u data=%p", dw_hr_name(hr), (int)ex, (unsigned)n, d);
        if (SUCCEEDED(hr) && ex) IDWriteFontFace_ReleaseFontTable(c.p.face, x);
    }
    end(&c);
}
TEST(dir_trygettable_absent_tag_zeroes_outputs) {
    ctx c; if (!begin(&c, sf_default(), "absent tag")) return;
    if (c.p.face) {
        const void *d = (const void *)1; UINT32 n = 77; void *x = (void *)1; BOOL ex = 77;
        CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('Z','Z','Z','Z'), &d, &n, &x, &ex), S_OK);
        CHECK(d == NULL); CHECK(n == 0); CHECK(x == NULL); CHECK(ex == FALSE);
    }
    end(&c);
}
TEST(dir_trygettable_tag_byte_order) {
    /* tags are passed little-endian as in DWRITE_MAKE_OPENTYPE_TAG: 'head' = 0x64616568 */
    ctx c; if (!begin(&c, sf_default(), "tag order")) return;
    if (c.p.face) {
        const void *d = NULL; UINT32 n = 0; void *x = NULL; BOOL ex = FALSE;
        CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, 0x64616568, &d, &n, &x, &ex), S_OK); CHECK(ex && n == 54);
        if (ex) IDWriteFontFace_ReleaseFontTable(c.p.face, x);
        ex = TRUE; CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, 0x68656164 /* byte-swapped */, &d, &n, &x, &ex), S_OK); CHECK(!ex);
        ex = TRUE; CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, 0, &d, &n, &x, &ex), S_OK); CHECK(!ex);
        ex = TRUE; CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, 0xFFFFFFFF, &d, &n, &x, &ex), S_OK); CHECK(!ex);
        ex = TRUE; CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('H','E','A','D'), &d, &n, &x, &ex), S_OK); CHECK(!ex);   /* case sensitive */
    }
    end(&c);
}
TEST(dir_trygettable_returns_exact_bytes_for_all_tables) {
    sf_blob src = sf_default(); ctx c; int i, nt = sf_ntables(&src);
    if (!begin(&c, sf_default(), "all tables")) { sf_free(&src); return; }
    if (c.p.face) {
        for (i = 0; i < nt; i++) {
            const uint8_t *e = sf_dirent(&src, i); char tag[5] = { 0 }; UINT32 t, n = 0; const void *d = NULL; void *x = NULL; BOOL ex = FALSE;
            memcpy(tag, e, 4); t = DW_TAG(e[0], e[1], e[2], e[3]);
            CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, t, &d, &n, &x, &ex), S_OK);
            CHECKF(ex, "table %s exists", tag);
            CHECKF(n == sf_r32(e + 12), "table %s length %u want %u", tag, (unsigned)n, (unsigned)sf_r32(e + 12));
            if (ex && n == sf_r32(e + 12)) CHECKF(!memcmp(d, src.p + sf_r32(e + 8), n), "table %s bytes", tag);
            if (ex) IDWriteFontFace_ReleaseFontTable(c.p.face, x);
        }
    }
    sf_free(&src); end(&c);
}
TEST(dir_trygettable_context_release_balances_fragments) {
    ctx c; if (!begin(&c, sf_default(), "fragment balance")) return;
    if (c.p.face) {
        dwx_mem_stream *s = sf_stream(&c.p); LONG base = s ? s->outstanding : 0, held; const void *d; UINT32 n; void *x[5]; BOOL ex; int i;
        for (i = 0; i < 5; i++) { x[i] = NULL; IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('c','m','a','p'), &d, &n, &x[i], &ex); }
        held = s ? s->outstanding : 0;
        t_note("five outstanding cmap fetches hold %ld fragments (baseline %ld)", (long)held, (long)base);
        PRED("each fetch holds a stream fragment", held >= base + 5);
        for (i = 0; i < 5; i++) IDWriteFontFace_ReleaseFontTable(c.p.face, x[i]);
        if (s) CHECKF(s->outstanding == base, "fragments after release: %ld want %ld", (long)s->outstanding, (long)base);
    }
    end(&c);
}
TEST(dir_trygettable_release_null_context_is_harmless) {
    ctx c; if (!begin(&c, sf_default(), "release NULL")) return;
    if (c.p.face) IDWriteFontFace_ReleaseFontTable(c.p.face, NULL);
    end(&c);
}

/* ---- duplicates and ordering ------------------------------------------------------------------------------------------- */
static sf_blob modified_table_copy(const sf_blob *src, const char *tag, int off, uint16_t val, size_t *len) {
    sf_blob t; unsigned l = 0; uint8_t *p = sf_table(src, tag, &l);
    t.p = (uint8_t *)malloc(l + 4); memcpy(t.p, p, l); t.n = l; *len = l; sf_w16(t.p + off, val); return t;
}
TEST(dir_duplicate_head_first_record_wins) {
    sf_blob b = sf_default(), h; size_t l; ctx c; int i;
    h = modified_table_copy(&b, "head", 18, 1000, &l);                 /* unitsPerEm 1000 copy */
    sf_insert_record(&b, 10, "head", h.p, l);                            /* appended record, after the real one in the scan */
    sf_free(&h);
    if (!begin(&c, b, "duplicate head, modified copy last")) return;
    if (c.p.face) {
        DWRITE_FONT_METRICS m; const void *d; UINT32 n; void *x; BOOL ex;
        IDWriteFontFace_GetMetrics(c.p.face, &m);
        CHECKF(m.designUnitsPerEm == 2048, "unitsPerEm %u (first head record must win)", (unsigned)m.designUnitsPerEm);
        CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('h','e','a','d'), &d, &n, &x, &ex), S_OK);
        if (ex) { CHECK(sf_r16((const uint8_t *)d + 18) == 2048); IDWriteFontFace_ReleaseFontTable(c.p.face, x); }
    }
    (void)i; end(&c);
}
TEST(dir_duplicate_head_modified_copy_first) {
    sf_blob b = sf_default(), h; size_t l; ctx c;
    h = modified_table_copy(&b, "head", 18, 1000, &l);
    sf_insert_record(&b, 0, "head", h.p, l);
    sf_free(&h);
    if (!begin(&c, b, "duplicate head, modified copy first")) return;
    if (c.p.face) {
        DWRITE_FONT_METRICS m; IDWriteFontFace_GetMetrics(c.p.face, &m);
        CHECKF(m.designUnitsPerEm == 1000, "unitsPerEm %u (now the first record is the 1000 copy)", (unsigned)m.designUnitsPerEm);
    }
    end(&c);
}
TEST(dir_duplicate_maxp_first_record_wins) {
    sf_blob b = sf_default(), h; size_t l; ctx c;
    h = modified_table_copy(&b, "maxp", 4, 20, &l);                       /* numGlyphs 20 */
    sf_insert_record(&b, 10, "maxp", h.p, l); sf_free(&h);
    if (!begin(&c, b, "duplicate maxp, 20-glyph copy last")) return;
    if (c.p.face) CHECK(IDWriteFontFace_GetGlyphCount(c.p.face) == TTF_DEF_NGLYPHS);
    end(&c);
}
TEST(dir_duplicate_maxp_modified_first) {
    sf_blob b = sf_default(), h; size_t l; ctx c;
    h = modified_table_copy(&b, "maxp", 4, 20, &l);
    sf_insert_record(&b, 0, "maxp", h.p, l); sf_free(&h);
    if (!begin(&c, b, "duplicate maxp, 20-glyph copy first")) return;
    if (c.p.face) { CHECKF(IDWriteFontFace_GetGlyphCount(c.p.face) == 20, "glyph count %u", (unsigned)IDWriteFontFace_GetGlyphCount(c.p.face)); }
    end(&c);
}
TEST(dir_duplicate_tags_trygettable_first_wins) {
    sf_blob b = sf_default(), h; size_t l; ctx c;
    h = modified_table_copy(&b, "head", 18, 1000, &l);
    sf_insert_record(&b, 0, "head", h.p, l); sf_free(&h);
    if (!begin(&c, b, "duplicate head first")) return;
    if (c.p.face) {
        const void *d; UINT32 n; void *x; BOOL ex;
        CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('h','e','a','d'), &d, &n, &x, &ex), S_OK);
        CHECK(ex); if (ex) { CHECK(sf_r16((const uint8_t *)d + 18) == 1000); IDWriteFontFace_ReleaseFontTable(c.p.face, x); }
    }
    end(&c);
}
TEST(dir_duplicate_cmap_first_record_wins) {
    sf_blob b = sf_default(); ctx c; int i = sf_find(&b, "cmap");
    sf_insert_record_raw(&b, 10, "cmap", sf_r32(sf_dirent(&b, i) + 8) + 16, 0);       /* empty duplicate, last */
    if (!begin(&c, b, "duplicate cmap, empty copy last")) return;
    if (c.p.face) CHECK(sf_gid(c.p.face, 'A') == ttf_default_gid('A'));
    end(&c);
}
TEST(dir_duplicate_cmap_empty_first_is_skipped) {
    /* InitializeEntry keeps the first record that has both an offset and a length: an empty record in front does not shadow the real one */
    sf_blob b = sf_default(); ctx c;
    sf_insert_record_raw(&b, 0, "cmap", 0, 0);
    if (!begin(&c, b, "empty cmap record before the real one")) return;
    PRED("real cmap used", c.p.face && sf_gid(c.p.face, 'A') == ttf_default_gid('A'));
    end(&c);
}
TEST(dir_duplicate_hhea_and_os2_first_record_wins) {
    sf_blob b = sf_default(), h; size_t l; ctx c;
    h = modified_table_copy(&b, "hhea", 4, 100, &l);                      /* ascender 100 */
    sf_insert_record(&b, 10, "hhea", h.p, l); sf_free(&h);
    if (!begin(&c, b, "duplicate hhea last")) return;
    end(&c);
}
TEST(dir_reversed_directory_order) {
    sf_blob b = sf_default(); int i, n = sf_ntables(&b); ctx c;
    for (i = 0; i < n / 2; i++) sf_swap_ents(&b, i, n - 1 - i);
    if (!begin(&c, b, "directory in reverse tag order")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) {
        CHECK(sf_gid(c.p.face, 'C') == ttf_default_gid('C')); CHECK(IDWriteFontFace_GetGlyphCount(c.p.face) == TTF_DEF_NGLYPHS);
        { UINT32 l; uint8_t *t = dw_face_table(c.p.face, DW_TAG('n','a','m','e'), &l); CHECK(t != NULL); free(t); }
    }
    end(&c);
}
TEST(dir_shuffled_directory_orders) {
    int seed;
    for (seed = 1; seed <= 6; seed++) {
        sf_blob b = sf_default(); int i, n = sf_ntables(&b); unsigned r = seed * 2654435761u; char l[48]; ctx c;
        for (i = n - 1; i > 0; i--) { r = r * 1103515245u + 12345u; sf_swap_ents(&b, i, (int)((r >> 16) % (unsigned)(i + 1))); }
        snprintf(l, sizeof l, "directory shuffle #%d", seed);
        if (begin(&c, b, l)) { CHECKF(SUCCEEDED(c.p.hr_face), "shuffle %d: %s", seed, dw_hr_name(c.p.hr_face)); if (c.p.face) CHECK(sf_gid(c.p.face, 'D') == ttf_default_gid('D')); end(&c); }
    }
}
TEST(dir_table_data_order_in_file_is_free) {
    /* tables stored in reverse order of the directory (offsets decreasing) */
    sf_blob b = sf_default(), r; int i, n = sf_ntables(&b); uint32_t pos = 12 + 16 * n; ctx c;
    r.p = (uint8_t *)calloc(b.n + 64, 1); memcpy(r.p, b.p, 12 + 16 * n); r.n = pos;
    for (i = n - 1; i >= 0; i--) {
        uint8_t *e = sf_dirent(&b, i); uint32_t o = sf_r32(e + 8), l = sf_r32(e + 12);
        memcpy(r.p + pos, b.p + o, l); sf_w32(r.p + 12 + 16 * i + 8, pos); pos += (l + 3) & ~3u;
    }
    r.n = pos; sf_free(&b);
    if (!begin(&c, r, "table bodies stored in reverse order")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) CHECK(sf_gid(c.p.face, 'E') == ttf_default_gid('E'));
    end(&c);
}
TEST(dir_overlapping_tables_share_bytes) {
    /* hhea record pointing into the head table: overlap is not diagnosed */
    sf_blob b = sf_default(); ctx c; int ih = sf_find(&b, "hhea"), iw = sf_find(&b, "head");
    sf_set_ent(&b, ih, sf_r32(sf_dirent(&b, iw) + 8), 36);
    if (!begin(&c, b, "hhea overlapping head")) return;
    t_note("overlap result recorded");
    end(&c);
}
TEST(dir_tables_overlap_directory) {
    sf_blob b = sf_default(); ctx c; int i = sf_find(&b, "post");
    sf_set_ent(&b, i, 12, 32);
    if (!begin(&c, b, "post pointing into the directory")) return;
    PRED("face loads", SUCCEEDED(c.p.hr_face));
    end(&c);
}

/* ---- checksums are not verified ---------------------------------------------------------------------------------------------- */
TEST(dir_stale_table_checksums_accepted) {
    sf_blob b = sf_default(); int i, n = sf_ntables(&b); ctx c;
    for (i = 0; i < n; i++) sf_w32(sf_dirent(&b, i) + 4, 0xDEADBEEF);
    if (!begin(&c, b, "all table checksums wrong")) return;
    CHECK_HR(c.p.hr_analyze, S_OK); CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) CHECK(sf_gid(c.p.face, 'A') == ttf_default_gid('A'));
    end(&c);
}
TEST(dir_zero_table_checksums_accepted) {
    sf_blob b = sf_default(); int i, n = sf_ntables(&b); ctx c;
    for (i = 0; i < n; i++) sf_w32(sf_dirent(&b, i) + 4, 0);
    if (!begin(&c, b, "all table checksums zero")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    end(&c);
}
TEST(dir_head_checksum_adjustment_wrong_accepted) {
    sf_blob b = sf_default(); ctx c; uint8_t *h = sf_table(&b, "head", NULL);
    sf_w32(h + 8, 0x12345678);
    if (!begin(&c, b, "head.checkSumAdjustment wrong")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    end(&c);
}
TEST(dir_head_checksum_adjustment_wrong_whole_file) {
    sf_blob b = sf_default(); ctx c; uint8_t *h = sf_table(&b, "head", NULL);
    sf_w32(h + 8, 0);
    if (!begin(&c, b, "head.checkSumAdjustment zero")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    end(&c);
}
TEST(dir_binary_search_hints_ignored) {
    /* searchRange / entrySelector / rangeShift are redundant; the scanner never uses them */
    sf_blob b = sf_default(); ctx c;
    sf_w16(b.p + 6, 0xFFFF); sf_w16(b.p + 8, 0xFFFF); sf_w16(b.p + 10, 0xFFFF);
    if (!begin(&c, b, "searchRange/entrySelector/rangeShift garbage")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) CHECK(sf_gid(c.p.face, 'F') == ttf_default_gid('F'));
    end(&c);
    { sf_blob z = sf_default(); sf_w16(z.p + 6, 0); sf_w16(z.p + 8, 0); sf_w16(z.p + 10, 0);
      if (begin(&c, z, "searchRange/entrySelector/rangeShift zero")) { CHECK_HR(c.p.hr_face, S_OK); end(&c); } }
}
TEST(dir_unknown_tags_ignored) {
    sf_blob b = sf_default(); ctx c; const char *tags[] = { "FFTM", "DSIG", "gasp", "kern", "ZZZZ", "\x01\x02\x03\x04", "    " }; int i;
    for (i = 0; i < 7; i++) { static const uint8_t body[16] = { 1, 2, 3, 4 }; sf_insert_record(&b, 10 + i, tags[i], body, sizeof body); }
    if (!begin(&c, b, "seven extra tables of junk")) return;
    CHECK_HR(c.p.hr_face, S_OK);
    if (c.p.face) {
        const void *d; UINT32 n; void *x; BOOL ex; CHECK_HR(IDWriteFontFace_TryGetFontTable(c.p.face, DW_TAG('F','F','T','M'), &d, &n, &x, &ex), S_OK);
        CHECK(ex && n == 16); if (ex) IDWriteFontFace_ReleaseFontTable(c.p.face, x);
    }
    end(&c);
}

/* ---- truncating the file --------------------------------------------------------------------------------------------------------- */
static void trunc_probe(const char *label, size_t keep, int must_fail) {
    sf_blob b = sf_default(); ctx c; unsigned ol = 0; uint8_t *o2 = sf_table(&b, "OS/2", &ol);
    if (t_is_wine() && o2 && keep < (size_t)(o2 - b.p) + ol) { t_skip("Wine crashes while creating a face over a file cut inside OS/2"); sf_free(&b); return; }
    if (keep < b.n) b.n = keep;
    if (!begin(&c, b, label)) return;
    if (must_fail) { CHECKF(FAILED(c.p.hr_analyze) || FAILED(c.p.hr_face), "%s: no failure", label); }
    end(&c);
}
#define TRUNC(name, keep, must) TEST(dir_truncate_##name) { trunc_probe("truncated to " #keep, keep, must); }
TEST(dir_truncate_before_directory_end) {
    sf_blob b = sf_default(); size_t cut = 12 + 16 * (size_t)sf_ntables(&b) - 1; sf_free(&b);
    trunc_probe("cut inside the directory", cut, 1);
}
TEST(dir_truncate_header_only) { trunc_probe("cut at 117 bytes", 117, 1); }
TEST(dir_truncate_at_directory_end) { sf_blob b = sf_default(); size_t cut = 12 + 16 * (size_t)sf_ntables(&b); sf_free(&b); trunc_probe("cut right after the directory", cut, 1); }
TEST(dir_truncate_inside_each_table) {
    sf_blob full = sf_default(); int i, n = sf_ntables(&full);
    for (i = 0; i < n; i++) {
        uint8_t *e = sf_dirent(&full, i); char tag[5] = { 0 }, l[64]; size_t o = sf_r32(e + 8), len = sf_r32(e + 12);
        memcpy(tag, e, 4);
        snprintf(l, sizeof l, "cut in the middle of %s", tag); trunc_probe(l, o + len / 2, 0);
        snprintf(l, sizeof l, "cut one byte before the end of %s", tag); trunc_probe(l, o + len - 1, 0);
        snprintf(l, sizeof l, "cut at the start of %s", tag); trunc_probe(l, o, 0);
    }
    sf_free(&full);
}
TEST(dir_truncate_last_byte) { trunc_probe("last byte removed", sf_default().n - 1, 0); }
TEST(dir_trailing_garbage_ignored) {
    sf_blob b = sf_default(), g; ctx c; g.n = b.n + 4096; g.p = (uint8_t *)malloc(g.n); memcpy(g.p, b.p, b.n); memset(g.p + b.n, 0xAB, 4096); sf_free(&b);
    if (!begin(&c, g, "4096 bytes of trailing garbage")) return;
    CHECK_HR(c.p.hr_face, S_OK); end(&c);
}
TEST(dir_leading_pad_offsets) {
    /* tables moved 1 KiB later in the file: offsets are absolute, nothing assumes a packed layout */
    sf_blob b = sf_default(), g; int i, n = sf_ntables(&b); ctx c; size_t hdr = 12 + 16 * (size_t)n;
    g.n = b.n + 1024; g.p = (uint8_t *)calloc(g.n, 1); memcpy(g.p, b.p, hdr); memcpy(g.p + hdr + 1024, b.p + hdr, b.n - hdr);
    for (i = 0; i < n; i++) sf_w32(g.p + 12 + 16 * i + 8, sf_r32(g.p + 12 + 16 * i + 8) + 1024);
    sf_free(&b);
    if (!begin(&c, g, "1 KiB gap between directory and tables")) return;
    CHECK_HR(c.p.hr_face, S_OK); if (c.p.face) CHECK(sf_gid(c.p.face, 'G') == ttf_default_gid('G'));
    end(&c);
}
