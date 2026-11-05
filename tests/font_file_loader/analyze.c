/* IDWriteFontFile::Analyze.
 * Basis: DWriteFontFile::Analyze (outputs preset to FALSE / UNKNOWN / DWRITE_FONT_FACE_TYPE_UNKNOWN / 0 before the argument
 * check; isSupported = faceType < 3; fontFaceType may be NULL), FontFileAnalyzer::Analyze = IsPfm, then IsPfb, then
 * IsOpenType, then IsFon, each reading a fixed header prefix through the stream (0x75 bytes, 7 bytes, 0x10 bytes). */
#include "fl.h"

#define FT_UNK  DWRITE_FONT_FILE_TYPE_UNKNOWN
#define FC_UNK  DWRITE_FONT_FACE_TYPE_UNKNOWN

/* Analyze `blob` through a fresh isolated factory + recording loader. */
static an_res an_blob_ex(const void *blob, size_t n, fl_loader **keep) {
    rig r; an_res a;
    memset(&a, 0, sizeof a); a.hr = E_FAIL;
    if (!rig_open(&r, blob, n)) { rig_close(&r); return a; }
    a = fl_analyze(r.file);
    if (keep) { *keep = r.ld; fl_addref(r.ld); }
    rig_close(&r);
    return a;
}
static an_res an_blob(const void *blob, size_t n) { return an_blob_ex(blob, n, NULL); }

#define CHECK_UNSUPPORTED(a) do { CHECK(!(a).sup); EXPECT_EQ((a).ft, FT_UNK); EXPECT_EQ((a).fc, FC_UNK); EXPECT_EQ((a).nf, 0); } while (0)
/* garbage that is long enough to be read in full: Analyze succeeds and reports "not a font" */
#define CHECK_GARBAGE_OK(a) do { CHECK_HR((a).hr, S_OK); CHECK_UNSUPPORTED(a); } while (0)
/* garbage too short for the 0x75 byte prefix read: FileFormatException at the API boundary, or "not supported" */
#define CHECK_SHORT(a) do { CHECKF((a).hr == S_OK || (a).hr == H_FILEFORMAT, "short file Analyze hr=%s", dw_hr_name((a).hr)); CHECK_UNSUPPORTED(a); } while (0)

/* ---------------- genuine fonts ---------------- */
TEST(an_truetype_default) {
    size_t n; uint8_t *b = fx_ttf(&n); an_res a = an_blob(b, n); free(b);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(a.nf, 1);
}
TEST(an_truetype_styles) {
    static const struct { int w, wd, it; } s[] = { { 400, 5, 0 }, { 700, 5, 0 }, { 400, 5, 1 }, { 300, 3, 0 }, { 900, 7, 1 } };
    int i;
    for (i = 0; i < 5; i++) {
        size_t n; uint8_t *b = ttf_build_styled("FlStyle", s[i].w, s[i].wd, s[i].it, &n); an_res a = an_blob(b, n); free(b);
        CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(a.nf, 1);
    }
}
TEST(an_otto_from_spec) {
    ttf_spec s; size_t n; ttf_default_spec(&s); s.sfnt_version = 0x4F54544F;
    uint8_t *b = ttf_build(&s, &n); an_res a = an_blob(b, n); free(b);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_CFF); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_CFF); EXPECT_EQ(a.nf, 1);
}
TEST(an_true_tag_from_spec) {
    ttf_spec s; size_t n; ttf_default_spec(&s); s.sfnt_version = 0x74727565;
    uint8_t *b = ttf_build(&s, &n); an_res a = an_blob(b, n); free(b);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(a.nf, 1);
}
TEST(an_sfnt_2_0_from_spec) {
    ttf_spec s; size_t n; ttf_default_spec(&s); s.sfnt_version = 0x00020000;
    uint8_t *b = ttf_build(&s, &n); an_res a = an_blob(b, n); free(b);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE);
}
TEST(an_ttc_real_counts) {
    int cnt;
    for (cnt = 1; cnt <= 4; cnt++) {
        const uint8_t *blobs[4]; size_t sizes[4]; int i;
        for (i = 0; i < cnt; i++) { char nm[32]; snprintf(nm, sizeof nm, "FlTtc%d", i); blobs[i] = ttf_build_styled(nm, 400, 5, 0, &sizes[i]); }
        size_t n; uint8_t *c = ttf_build_ttc(blobs, sizes, cnt, &n);
        an_res a = an_blob(c, n);
        CHECK_HR(a.hr, S_OK); CHECK(a.sup);
        EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION); EXPECT_EQ(a.nf, (UINT32)cnt);
        free(c); for (i = 0; i < cnt; i++) free((void *)blobs[i]);
    }
}

/* ---------------- header-only blobs: Analyze looks at the magic only ---------------- */
static const struct { const char *name; uint32_t magic; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf; } g_magic[] = {
    { "sfnt 1.0",  0x00010000, DWRITE_FONT_FILE_TYPE_TRUETYPE, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1 },
    { "sfnt 2.0",  0x00020000, DWRITE_FONT_FILE_TYPE_TRUETYPE, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1 },
    { "true",      0x74727565, DWRITE_FONT_FILE_TYPE_TRUETYPE, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1 },
    { "OTTO",      0x4F54544F, DWRITE_FONT_FILE_TYPE_CFF,      DWRITE_FONT_FACE_TYPE_CFF,      1 },
};
TEST(an_magic_table_header_only) {
    static const size_t sizes[] = { 0x75, 0x76, 0x80, 0x100, 0x1000, 0x20000 };
    size_t i, k;
    for (i = 0; i < sizeof g_magic / sizeof g_magic[0]; i++)
        for (k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
            size_t n; uint8_t *b = fx_magic(g_magic[i].magic, sizes[k], &n); an_res a = an_blob(b, n); free(b);
            CHECKF(a.hr == S_OK && a.sup && a.ft == g_magic[i].ft && a.fc == g_magic[i].fc && a.nf == g_magic[i].nf,
                   "%s size %zx: hr=%s sup=%d ft=%d fc=%d nf=%u", g_magic[i].name, sizes[k], dw_hr_name(a.hr), a.sup, a.ft, a.fc, a.nf);
        }
}
TEST(an_magic_not_a_font) {
    static const uint32_t bad[] = { 0, 0xFFFFFFFF, 0x00030000, 0x01000000, 0x74797031 /* typ1 */, 0x774F4646 /* wOFF */, 0x774F4632 /* wOF2 */,
                                    0x4F545430 /* OTT0 */, 0x74746367 /* ttcg */, 0x54544346 /* TTCF */, 0x5452554E, 0x00010001, 0x10000000, 0x7472756F };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        size_t n; uint8_t *b = fx_magic(bad[i], 0x100, &n); an_res a = an_blob(b, n); free(b);
        CHECKF(a.hr == S_OK, "magic %08x hr=%s", bad[i], dw_hr_name(a.hr));
        CHECKF(!a.sup && a.ft == FT_UNK && a.fc == FC_UNK && a.nf == 0, "magic %08x: sup=%d ft=%d fc=%d nf=%u", bad[i], a.sup, a.ft, a.fc, a.nf);
    }
}
TEST(an_magic_little_endian_is_not_accepted) {
    /* the tags are matched in file byte order: 'OTTO' reversed and 0x00000100 are not fonts */
    static const uint8_t pats[][4] = { { 'O', 'T', 'T', 'O' }, { 'O', 'T', 'T', 'O' } };
    size_t n; uint8_t *b = fx_magic(0x4F54544F, 0x100, &n); b[0] = 'O'; b[1] = 'T'; b[2] = 'T'; b[3] = 'O';
    an_res a = an_blob(b, n); CHECK(a.sup); free(b);
    b = fx_magic(0, 0x100, &n); b[0] = 0; b[1] = 0; b[2] = 1; b[3] = 0;   /* 0x00000100 */
    a = an_blob(b, n); free(b); CHECK_GARBAGE_OK(a);
    (void)pats;
}
TEST(an_ttc_header_counts) {
    static const uint32_t cnt[] = { 1, 2, 3, 4, 8, 16, 255, 256, 1000, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF };
    size_t i;
    for (i = 0; i < sizeof cnt / sizeof cnt[0]; i++) {
        size_t n; uint8_t *b = fx_ttc_hdr(cnt[i], 0x80, &n); an_res a = an_blob(b, n); free(b);
        /* the face count is taken from the header unchecked (IsOpenType stores it as is) */
        CHECKF(a.hr == S_OK && a.sup && a.ft == DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION && a.fc == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION && a.nf == cnt[i],
               "count %u: hr=%s sup=%d ft=%d fc=%d nf=%u", cnt[i], dw_hr_name(a.hr), a.sup, a.ft, a.fc, a.nf);
    }
}
TEST(an_ttc_count_zero) {
    size_t n; uint8_t *b = fx_ttc_hdr(0, 0x80, &n); an_res a = an_blob(b, n); free(b);
    /* zero fonts in a collection is rejected (FileFormatException, or an assert-only path): not a supported font either way */
    t_note("ttcf with zero fonts: hr=%s sup=%d ft=%d fc=%d nf=%u", dw_hr_name(a.hr), a.sup, a.ft, a.fc, a.nf);
    CHECK(a.hr == S_OK || a.hr == H_FILEFORMAT);
    CHECK(!a.sup);
    CHECK(a.nf == 0);
}
TEST(an_ttc_other_version_fields_ignored) {
    static const uint32_t vers[] = { 0x00010000, 0x00020000, 0x00030000, 0, 0xFFFFFFFF };
    size_t i;
    for (i = 0; i < sizeof vers / sizeof vers[0]; i++) {
        size_t n; uint8_t *b = fx_ttc_hdr(2, 0x80, &n);
        b[4] = vers[i] >> 24; b[5] = vers[i] >> 16; b[6] = vers[i] >> 8; b[7] = vers[i];
        an_res a = an_blob(b, n); free(b);
        CHECKF(a.hr == S_OK && a.sup && a.nf == 2, "ttc version %08x: hr=%s sup=%d nf=%u", vers[i], dw_hr_name(a.hr), a.sup, a.nf);
    }
}
TEST(an_corrupted_real_fonts_still_pass_analyze) {
    /* Analyze reads the 4 byte magic only: damage deeper in the file is found later, when a face is created */
    size_t n; uint8_t *b = fx_ttf(&n); size_t cn; uint8_t *c; an_res a;
    c = ttf_corrupt(b, n, TTF_CORRUPT_TRUNCATE, 0x80, &cn); a = an_blob(c, cn); free(c);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    c = ttf_corrupt(b, n, TTF_CORRUPT_NUMTABLES, 200, &cn); a = an_blob(c, cn); free(c);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    c = ttf_corrupt(b, n, TTF_CORRUPT_TABLE_OFFSET, 2, &cn); a = an_blob(c, cn); free(c);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    c = ttf_corrupt(b, n, TTF_CORRUPT_TABLE_LENGTH, 1, &cn); a = an_blob(c, cn); free(c);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    c = ttf_corrupt(b, n, TTF_CORRUPT_TABLE_CHECKSUM, 0, &cn); a = an_blob(c, cn); free(c);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    c = ttf_corrupt(b, n, TTF_CORRUPT_HEAD_MAGIC, 0, &cn); a = an_blob(c, cn); free(c);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    c = ttf_corrupt(b, n, TTF_CORRUPT_BAD_MAGIC, 0, &cn); a = an_blob(c, cn); free(c);
    CHECK_HR(a.hr, S_OK); CHECK_UNSUPPORTED(a);
    free(b);
}
TEST(an_woff_is_not_a_font_file) {
    size_t n, wn; uint8_t *b = fx_ttf(&n); uint8_t *w = ttf_wrap_woff(b, n, &wn); free(b);
    an_res a = an_blob(w, wn); free(w);
    CHECK_GARBAGE_OK(a);   /* containers are unpacked with UnpackFontFile, not by Analyze */
}

/* ---------------- Type 1 ---------------- */
TEST(an_pfb_synthetic) {
    static const size_t sizes[] = { 0x75, 0x80, 0x200, 0x4000 }; size_t i;
    for (i = 0; i < 4; i++) {
        size_t n; uint8_t *b = fx_pfb(sizes[i], &n); an_res a = an_blob(b, n);
        CHECKF(a.hr == S_OK && !a.sup && a.ft == DWRITE_FONT_FILE_TYPE_TYPE1_PFB && a.fc == DWRITE_FONT_FACE_TYPE_TYPE1 && a.nf == 1,
               "pfb %zx: hr=%s sup=%d ft=%d fc=%d nf=%u", sizes[i], dw_hr_name(a.hr), a.sup, a.ft, a.fc, a.nf);
        b[1] = 2; a = an_blob(b, n);                      /* segment type 2 (binary) is accepted as well */
        CHECKF(a.hr == S_OK && a.ft == DWRITE_FONT_FILE_TYPE_TYPE1_PFB, "pfb type 2 %zx: ft=%d", sizes[i], a.ft);
        free(b);
    }
}
TEST(an_pfb_variants_rejected) {
    size_t n; uint8_t *b = fx_pfb(0x100, &n); an_res a; uint32_t len;
    b[1] = 3; a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    b[1] = 0; a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    b[1] = 1; b[0] = 0x7F; a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    b[0] = 0x81; a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    b[0] = 0x80; len = 0x100 - 5; memcpy(b + 2, &len, 4); a = an_blob(b, n); CHECK_GARBAGE_OK(a);      /* first segment would end one byte past the file */
    len = 0xFFFFFFFFu; memcpy(b + 2, &len, 4); a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    len = 0x100 - 6; memcpy(b + 2, &len, 4); a = an_blob(b, n); CHECK(a.ft == DWRITE_FONT_FILE_TYPE_TYPE1_PFB);   /* exactly to the end of file: accepted */
    len = 0; memcpy(b + 2, &len, 4); a = an_blob(b, n); CHECK(a.ft == DWRITE_FONT_FILE_TYPE_TYPE1_PFB);          /* empty segment is fine */
    free(b);
}
TEST(an_pfm_synthetic) {
    static const size_t sizes[] = { 0x75, 0x80, 0x123, 0x1000, 0x5000 }; size_t i;
    for (i = 0; i < 5; i++) {
        size_t n; uint8_t *b = fx_pfm(sizes[i], &n); an_res a = an_blob(b, n); free(b);
        CHECKF(a.hr == S_OK && !a.sup && a.ft == DWRITE_FONT_FILE_TYPE_TYPE1_PFM && a.fc == DWRITE_FONT_FACE_TYPE_TYPE1 && a.nf == 1,
               "pfm %zx: hr=%s sup=%d ft=%d fc=%d nf=%u", sizes[i], dw_hr_name(a.hr), a.sup, a.ft, a.fc, a.nf);
    }
}
TEST(an_pfm_variants_rejected) {
    size_t n; uint8_t *b = fx_pfm(0x100, &n); an_res a; uint32_t v;
    v = 0x101; memcpy(b + 2, &v, 4); a = an_blob(b, n); CHECK_GARBAGE_OK(a);     /* embedded size must equal the file size */
    v = 0xFF;  memcpy(b + 2, &v, 4); a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    v = 0x100; memcpy(b + 2, &v, 4);
    b[0x42] = 0x80; a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    b[0x42] = 0x81; b[0x44] = 11; a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    b[0x44] = 10; b[0x45] = 1; a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    b[0x45] = 0; b[1] = 2; a = an_blob(b, n); CHECK_GARBAGE_OK(a);
    b[1] = 1; a = an_blob(b, n); CHECK(a.ft == DWRITE_FONT_FILE_TYPE_TYPE1_PFM);
    free(b);
}
TEST(an_pfm_checked_before_truetype) {
    /* bytes 00 01 00 00 are both the sfnt 1.0 tag and a PFM version word; with a consistent embedded size the PFM test wins */
    size_t n = 0x10000; uint8_t *b = (uint8_t *)calloc(1, n); uint32_t v = (uint32_t)n;
    b[0] = 0; b[1] = 1; memcpy(b + 2, &v, 4); b[0x42] = 0x81; b[0x44] = 10;
    an_res a = an_blob(b, n);
    CHECK_HR(a.hr, S_OK); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TYPE1_PFM); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TYPE1); CHECK(!a.sup);
    b[0x42] = 0; a = an_blob(b, n);                   /* PFM test fails: now it is read as the TrueType tag it also is */
    CHECK_HR(a.hr, S_OK); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); CHECK(a.sup);
    free(b);
}

/* ---------------- garbage and short files ---------------- */
TEST(an_garbage_patterns) {
    static const uint8_t fills[] = { 0x00, 0xFF, 0x41, 0x5A, 0x80, 0x20, 0x7F };
    size_t i;
    for (i = 0; i < sizeof fills; i++) {
        size_t n = 0x400; uint8_t *b = (uint8_t *)malloc(n); an_res a; memset(b, fills[i], n);
        if (fills[i] == 0x80) b[1] = 0x55;
        a = an_blob(b, n); free(b);
        CHECKF(a.hr == S_OK && !a.sup && a.ft == FT_UNK && a.fc == FC_UNK && a.nf == 0, "fill %02x: hr=%s sup=%d ft=%d fc=%d nf=%u", fills[i], dw_hr_name(a.hr), a.sup, a.ft, a.fc, a.nf);
    }
}
TEST(an_garbage_text) {
    static const char txt[] = "This is not a font file. It is a plain ASCII text that is longer than the 0x75 byte header prefix read by the analysis, "
                              "so every probe can be satisfied from the stream and none of them matches.\r\n";
    an_res a = an_blob(txt, sizeof txt); CHECK_GARBAGE_OK(a);
}
TEST(an_garbage_pseudo_random) {
    uint32_t s = 12345; int k;
    for (k = 0; k < 20; k++) {
        size_t n = 0x80 + k * 37, i; uint8_t *b = (uint8_t *)malloc(n);
        for (i = 0; i < n; i++) { s = s * 1664525u + 1013904223u; b[i] = (uint8_t)(s >> 24); }
        b[0] = 0x55;                                   /* keep clear of every accepted first byte pattern */
        an_res a = an_blob(b, n); free(b);
        CHECKF(a.hr == S_OK && !a.sup && a.ft == FT_UNK, "random %d: hr=%s sup=%d ft=%d", k, dw_hr_name(a.hr), a.sup, a.ft);
    }
}
TEST(an_short_files) {
    static const size_t sizes[] = { 0, 1, 2, 3, 4, 6, 7, 8, 15, 16, 17, 64, 0x73, 0x74 }; size_t i;
    for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        size_t n; uint8_t *b = fx_magic(0x00010000, sizes[i] ? sizes[i] : 1, &n); an_res a;
        a = an_blob(b, sizes[i]); free(b);
        CHECKF(a.hr == S_OK || a.hr == H_FILEFORMAT, "size %zx: hr=%s", sizes[i], dw_hr_name(a.hr));
        CHECKF(!a.sup && a.ft == FT_UNK && a.fc == FC_UNK && a.nf == 0, "size %zx: sup=%d ft=%d fc=%d nf=%u", sizes[i], a.sup, a.ft, a.fc, a.nf);
        t_note("Analyze of a %zu byte file: %s", sizes[i], dw_hr_name(a.hr));
    }
}
TEST(an_exactly_0x75_bytes_is_enough) {
    size_t n; uint8_t *b = fx_magic(0x00010000, 0x75, &n); an_res a = an_blob(b, n); free(b);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    b = fx_magic(0x00010000, 0x74, &n); a = an_blob(b, n); free(b);
    CHECK(!a.sup);
}
TEST(an_short_file_requests_not_clamped) {
    /* the 0x75 byte prefix is requested even when the file is shorter: the failing read shows up in the callback log */
    size_t n; uint8_t *b = fx_magic(0x00010000, 0x60, &n); fl_loader *ld = NULL; an_res a = an_blob_ex(b, n, &ld); free(b);
    CHECK(a.hr == S_OK || a.hr == H_FILEFORMAT);
    CHECK(ld->nev >= 3);
    if (ld->nev >= 3) { CHECK(ld->ev[0].kind == FL_CREATE); CHECK(ld->ev[1].kind == FL_SIZE); CHECK(ld->ev[2].kind == FL_READ); EXPECT_EQ(ld->ev[2].a, 0); EXPECT_EQ(ld->ev[2].b, 0x75); }
    EXPECT_EQ(ld->outstanding, 0);
    fl_release(ld);
}
TEST(an_mz_stub_without_resources) {
    size_t n = 0x400; uint8_t *b = (uint8_t *)calloc(1, n); b[0] = 'M'; b[1] = 'Z'; b[0x3C] = 0x80;
    an_res a = an_blob(b, n); free(b);
    t_note("Analyze of an MZ stub without NE/PE resources: hr=%s sup=%d ft=%d fc=%d nf=%u", dw_hr_name(a.hr), a.sup, a.ft, a.fc, a.nf);
    CHECK(a.hr == S_OK || a.hr == H_FILEFORMAT);
    CHECK(!a.sup);
}

/* ---------------- output protocol ---------------- */
TEST(an_outputs_preset_on_unsupported) {
    rig r; size_t n; uint8_t *b = fx_magic(0x12345678, 0x100, &n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    BOOL sup = TRUE; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)99; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)99; UINT32 nf = 99;
    CHECK_HR(IDWriteFontFile_Analyze(r.file, &sup, &ft, &fc, &nf), S_OK);
    CHECK(sup == FALSE); EXPECT_EQ(ft, FT_UNK); EXPECT_EQ(fc, FC_UNK); EXPECT_EQ(nf, 0);
    rig_close(&r); free(b);
}
TEST(an_fontfacetype_may_be_null) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    BOOL sup = FALSE; DWRITE_FONT_FILE_TYPE ft = FT_UNK; UINT32 nf = 0;
    CHECK_HR(IDWriteFontFile_Analyze(r.file, &sup, &ft, NULL, &nf), S_OK);
    CHECK(sup); EXPECT_EQ(ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(nf, 1);
    rig_close(&r); free(b);
}
TEST(an_null_required_outputs) {
    if (t_is_wine()) { t_skip("other implementations dereference the NULL"); return; }
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    BOOL sup = TRUE; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)9; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)9; UINT32 nf = 9;
    CHECK_HR(IDWriteFontFile_Analyze(r.file, NULL, &ft, &fc, &nf), E_INVALIDARG);
    EXPECT_EQ(ft, FT_UNK); EXPECT_EQ(fc, FC_UNK); EXPECT_EQ(nf, 0);                  /* the other outputs are preset before the check */
    ft = (DWRITE_FONT_FILE_TYPE)9; fc = (DWRITE_FONT_FACE_TYPE)9; nf = 9;
    CHECK_HR(IDWriteFontFile_Analyze(r.file, &sup, NULL, &fc, &nf), E_INVALIDARG);
    CHECK(sup == FALSE); EXPECT_EQ(fc, FC_UNK); EXPECT_EQ(nf, 0);
    sup = TRUE; fc = (DWRITE_FONT_FACE_TYPE)9;
    CHECK_HR(IDWriteFontFile_Analyze(r.file, &sup, &ft, &fc, NULL), E_INVALIDARG);
    CHECK(sup == FALSE); EXPECT_EQ(ft, FT_UNK); EXPECT_EQ(fc, FC_UNK);
    CHECK_HR(IDWriteFontFile_Analyze(r.file, NULL, NULL, NULL, NULL), E_INVALIDARG);
    EXPECT_EQ(r.ld->n_create, 0);                                                    /* argument errors never reach the stream */
    rig_close(&r); free(b);
}
TEST(an_null_args_do_not_touch_stream) {
    if (t_is_wine()) { t_skip("other implementations dereference the NULL"); return; }
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    BOOL sup; DWRITE_FONT_FILE_TYPE ft; UINT32 nf;
    CHECK_HR(IDWriteFontFile_Analyze(r.file, &sup, &ft, NULL, NULL), E_INVALIDARG);
    EXPECT_EQ(r.ld->nev, 0);
    rig_close(&r); free(b);
}

/* ---------------- callback traffic of Analyze ---------------- */
static fl_loader *traffic(const void *blob, size_t n, an_res *a) {
    rig r; fl_loader *ld;
    if (!rig_open(&r, blob, n)) { rig_close(&r); return NULL; }
    *a = fl_analyze(r.file);
    ld = r.ld; fl_addref(ld);
    rig_close(&r);
    return ld;
}
TEST(an_traffic_truetype) {
    size_t n; uint8_t *b = fx_ttf(&n); an_res a; fl_loader *ld = traffic(b, n, &a); free(b);
    if (!ld) return;
    CHECK_HR(a.hr, S_OK);
    /* stream is created for the first size query, then the three prefix probes IsPfm / IsPfb / IsOpenType, each released at once */
    FL_LOG_IS(ld, EV(FL_CREATE, 4, ANY), EV(FL_SIZE, 0, 0), EV(FL_READ, 0, 0x75), EV(FL_REL, 0, 0), EV(FL_READ, 0, 7), EV(FL_REL, 0, 0), EV(FL_READ, 0, 0x10), EV(FL_REL, 0, 0));
    EXPECT_EQ(ld->outstanding, 0); EXPECT_EQ(ld->bad_rel, 0);
    fl_release(ld);
}
TEST(an_traffic_ttc_and_cff) {
    size_t n; an_res a; fl_loader *ld; uint8_t *b = fx_ttc_hdr(3, 0x100, &n);
    ld = traffic(b, n, &a); free(b);
    if (ld) { CHECK(a.sup); FL_LOG_IS(ld, EV(FL_CREATE, 4, ANY), EV(FL_SIZE, 0, 0), EV(FL_READ, 0, 0x75), EV(FL_REL, 0, 0), EV(FL_READ, 0, 7), EV(FL_REL, 0, 0), EV(FL_READ, 0, 0x10), EV(FL_REL, 0, 0)); fl_release(ld); }
    b = fx_magic(0x4F54544F, 0x100, &n); ld = traffic(b, n, &a); free(b);
    if (ld) { CHECK(a.sup); FL_LOG_IS(ld, EV(FL_CREATE, 4, ANY), EV(FL_SIZE, 0, 0), EV(FL_READ, 0, 0x75), EV(FL_REL, 0, 0), EV(FL_READ, 0, 7), EV(FL_REL, 0, 0), EV(FL_READ, 0, 0x10), EV(FL_REL, 0, 0)); fl_release(ld); }
}
TEST(an_traffic_pfm_stops_after_first_probe) {
    size_t n; an_res a; uint8_t *b = fx_pfm(0x100, &n); fl_loader *ld = traffic(b, n, &a); free(b);
    if (!ld) return;
    EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TYPE1_PFM);
    FL_LOG_IS(ld, EV(FL_CREATE, 4, ANY), EV(FL_SIZE, 0, 0), EV(FL_READ, 0, 0x75), EV(FL_REL, 0, 0));
    fl_release(ld);
}
TEST(an_traffic_pfb_stops_after_second_probe) {
    size_t n; an_res a; uint8_t *b = fx_pfb(0x100, &n); fl_loader *ld = traffic(b, n, &a); free(b);
    if (!ld) return;
    EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TYPE1_PFB);
    FL_LOG_IS(ld, EV(FL_CREATE, 4, ANY), EV(FL_SIZE, 0, 0), EV(FL_READ, 0, 0x75), EV(FL_REL, 0, 0), EV(FL_READ, 0, 7), EV(FL_REL, 0, 0));
    fl_release(ld);
}
TEST(an_traffic_never_asks_last_write_time) {
    size_t n; an_res a; uint8_t *b = fx_ttf(&n); fl_loader *ld = traffic(b, n, &a); free(b);
    if (!ld) return;
    EXPECT_EQ(ld->n_lastw, 0);
    fl_release(ld);
}
TEST(an_traffic_garbage_reads_are_balanced) {
    size_t n = 0x300; uint8_t *b = (uint8_t *)malloc(n); an_res a; memset(b, 0x61, n);
    fl_loader *ld = traffic(b, n, &a); free(b);
    if (!ld) return;
    CHECK_HR(a.hr, S_OK);
    CHECK(ld->nev >= 8);                            /* create, size, three header probes and whatever the executable-resource probe does */
    CHECK(ld->ev[0].kind == FL_CREATE && ld->ev[1].kind == FL_SIZE);
    EXPECT_EQ(ld->n_read, ld->n_rel); EXPECT_EQ(ld->outstanding, 0); EXPECT_EQ(ld->bad_rel, 0);
    fl_release(ld);
}
TEST(an_repeated_calls_reread_the_stream) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n); int i;
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    for (i = 1; i <= 4; i++) {
        an_res a = fl_analyze(r.file);
        CHECK_HR(a.hr, S_OK);
        EXPECT_EQ(r.ld->n_read, 3 * i);                 /* nothing about the analysis is cached */
        EXPECT_EQ(r.ld->n_size, i);
        EXPECT_EQ(r.ld->n_create, 1);
    }
    rig_close(&r); free(b);
}
TEST(an_follows_changing_stream_content) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    an_res a = fl_analyze(r.file); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE);
    r.ld->data[0] = 'O'; r.ld->data[1] = 'T'; r.ld->data[2] = 'T'; r.ld->data[3] = 'O';
    a = fl_analyze(r.file); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_CFF); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_CFF);
    memset(r.ld->data, 0x33, 4);
    a = fl_analyze(r.file); CHECK(!a.sup); EXPECT_EQ(a.ft, FT_UNK);
    rig_close(&r); free(b);
}
TEST(an_reported_size_larger_than_data) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    r.ld->reported_size = 0x10000000000ull;
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE);
    rig_close(&r); free(b);
}
TEST(an_reported_size_zero_with_data) {
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    r.ld->reported_size = 0;                              /* the probes still read the data; the size only matters for the PFM/PFB tests and for error mapping */
    an_res a = fl_analyze(r.file);
    CHECK_HR(a.hr, S_OK); CHECK(a.sup);
    rig_close(&r); free(b);
}
TEST(an_stream_pointer_variants) {
    size_t n; uint8_t *b = fx_ttf(&n); int mode;
    for (mode = 0; mode < 3; mode++) {
        rig r;
        if (!rig_open(&r, b, n)) { rig_close(&r); continue; }
        r.ld->copy_frag = mode >= 1; r.ld->misalign = mode == 2;      /* in-place fragments / private copies / private copies at an odd address */
        an_res a = fl_analyze(r.file);
        CHECKF(a.hr == S_OK && a.sup && a.ft == DWRITE_FONT_FILE_TYPE_TRUETYPE, "mode %d: hr=%s", mode, dw_hr_name(a.hr));
        EXPECT_EQ(r.ld->outstanding, 0); EXPECT_EQ(r.ld->bad_rel, 0);
        rig_close(&r);
    }
    free(b);
}
TEST(an_fragments_of_ttc_header_misaligned) {
    size_t n; uint8_t *b = fx_ttc_hdr(5, 0x100, &n); rig r;
    if (rig_open(&r, b, n)) {
        r.ld->copy_frag = 1; r.ld->misalign = 1;
        an_res a = fl_analyze(r.file);
        CHECK_HR(a.hr, S_OK); EXPECT_EQ(a.nf, 5);
    }
    rig_close(&r); free(b);
}
TEST(an_sse_state_not_leaked_from_callbacks) {
    size_t n; uint8_t *b = fx_ttf(&n); rig r;
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    unsigned before = _mm_getcsr();
    r.ld->mxcsr_poke = 1;                                  /* callbacks switch to round-toward-zero */
    an_res a = fl_analyze(r.file);
    unsigned after = _mm_getcsr();
    _mm_setcsr(before);
    CHECK_HR(a.hr, S_OK);
    EXPECT_EQ(after & 0x6000, before & 0x6000);            /* dwrite resets the rounding control after calling out */
    EXPECT_EQ(after & 0x1F80, before & 0x1F80);
    rig_close(&r); free(b);
}
struct cctx { IDWriteFontFile *f; an_res r[8]; };
static DWORD WINAPI cworker(LPVOID p) { struct cctx *cx = (struct cctx *)p; int k; for (k = 0; k < 8; k++) cx->r[k] = fl_analyze(cx->f); return 0; }
TEST(an_concurrent_analyze_same_file) {
    /* a handful of threads analysing one file object at once; the cached stream is installed with a compare-exchange */
    struct cctx cs[4]; HANDLE th[4]; int i;
    rig r; size_t n; uint8_t *b = fx_ttf(&n);
    if (!rig_open(&r, b, n)) { rig_close(&r); free(b); return; }
    for (i = 0; i < 4; i++) { cs[i].f = r.file; th[i] = CreateThread(NULL, 0, cworker, &cs[i], 0, NULL); }
    WaitForMultipleObjects(4, th, TRUE, 20000);
    for (i = 0; i < 4; i++) { int k; CloseHandle(th[i]); for (k = 0; k < 8; k++) CHECK(cs[i].r[k].hr == S_OK && cs[i].r[k].sup); }
    CHECK(r.ld->n_create >= 1 && r.ld->n_create <= 4);
    EXPECT_EQ(r.ld->outstanding, 0); EXPECT_EQ(r.ld->bad_rel, 0);
    rig_close(&r); free(b);
}

/* ---------------- installed fonts through the built-in local loader ---------------- */
static an_res an_sys(const char *leaf, int *found) {
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; an_res a; IDWriteFontFile *file = NULL;
    memset(&a, 0, sizeof a); a.hr = E_FAIL; *found = 0;
    if (!fl_sysfont(leaf, p, sizeof p)) return a;
    IDWriteFactory *f = fl_factory(); if (!f) return a;
    dw_wide(p, w, MAX_PATH);
    *found = 1;
    a.hr = IDWriteFactory_CreateFontFileReference(f, w, NULL, &file);
    if (SUCCEEDED(a.hr) && file) { a = fl_analyze(file); IDWriteFontFile_Release(file); }
    IDWriteFactory_Release(f);
    return a;
}
#define SYS_TTF(id, leaf) TEST(an_sys_##id) { int fd; an_res a = an_sys(leaf, &fd); if (!fd) { t_skip(leaf " is not installed"); return; } \
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE); EXPECT_EQ(a.nf, 1); }
SYS_TTF(arial, "arial.ttf")
SYS_TTF(arialbd, "arialbd.ttf")
SYS_TTF(times, "times.ttf")
SYS_TTF(cour, "cour.ttf")
SYS_TTF(segoeui, "segoeui.ttf")
SYS_TTF(tahoma, "tahoma.ttf")
SYS_TTF(consola, "consola.ttf")
SYS_TTF(marlett, "marlett.ttf")
SYS_TTF(seguisym, "seguisym.ttf")
SYS_TTF(seguiemj, "seguiemj.ttf")
SYS_TTF(bahnschrift, "bahnschrift.ttf")
SYS_TTF(verdana, "verdana.ttf")
SYS_TTF(calibri, "calibri.ttf")

#define SYS_TTC(id, leaf, minfaces) TEST(an_sys_##id) { int fd; an_res a = an_sys(leaf, &fd); if (!fd) { t_skip(leaf " is not installed"); return; } \
    CHECK_HR(a.hr, S_OK); CHECK(a.sup); EXPECT_EQ(a.ft, DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION); EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION); \
    CHECKF(a.nf >= (minfaces), "%s faces %u", leaf, a.nf); t_note("%s: %u faces", leaf, a.nf); }
SYS_TTC(cambria, "cambria.ttc", 2)
SYS_TTC(msgothic, "msgothic.ttc", 2)
SYS_TTC(simsun, "simsun.ttc", 2)
SYS_TTC(meiryo, "meiryo.ttc", 2)
SYS_TTC(msyh, "msyh.ttc", 2)
SYS_TTC(msjh, "msjh.ttc", 2)
SYS_TTC(segoeui_variable, "SegUIVar.ttf", 1)

TEST(an_sys_bitmap_and_vector_fonts) {
    static const char *const fons[] = { "vgasys.fon", "vgaoem.fon", "vgafix.fon", "app850.fon", "dosapp.fon", "8514fix.fon", "8514sys.fon", "ega80850.fon", "serife.fon", "sserife.fon", "smalle.fon", "coure.fon", "modern.fon", "roman.fon", "script.fon" };
    size_t i; int seen = 0;
    for (i = 0; i < sizeof fons / sizeof fons[0]; i++) {
        int fd; an_res a = an_sys(fons[i], &fd);
        if (!fd) continue;
        seen++;
        t_note("%s: hr=%s sup=%d ft=%d fc=%d nf=%u", fons[i], dw_hr_name(a.hr), a.sup, a.ft, a.fc, a.nf);
        CHECK_HR(a.hr, S_OK);
        CHECK(!a.sup);                                  /* faceType VECTOR/BITMAP (4/5) is >= 3: never "supported" */
        CHECKF(a.ft == FT_UNK || a.ft == DWRITE_FONT_FILE_TYPE_VECTOR || a.ft == DWRITE_FONT_FILE_TYPE_BITMAP, "%s ft=%d", fons[i], a.ft);
        if (a.ft == DWRITE_FONT_FILE_TYPE_VECTOR) EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_VECTOR);
        if (a.ft == DWRITE_FONT_FILE_TYPE_BITMAP) EXPECT_EQ(a.fc, DWRITE_FONT_FACE_TYPE_BITMAP);
    }
    if (!seen) t_skip("no .fon file installed");
}

/* Relation check over the first installed files of each kind: file type and face type always agree. */
static void sweep(const char *pattern, int max) {
    char dir[MAX_PATH], pat[MAX_PATH]; WIN32_FIND_DATAA fd; HANDLE h; int done = 0;
    dw_fonts_dir(dir, sizeof dir); snprintf(pat, sizeof pat, "%s%s", dir, pattern);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no %s in the fonts directory", pattern); return; }
    do {
        int found; an_res a = an_sys(fd.cFileName, &found);
        if (!found) continue;
        CHECKF(a.hr == S_OK, "%s: hr=%s", fd.cFileName, dw_hr_name(a.hr));
        if (a.hr != S_OK) continue;
        CHECKF(a.sup == (a.fc <= DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION), "%s: sup=%d fc=%d", fd.cFileName, a.sup, a.fc);
        if (a.sup) {
            CHECKF(a.nf >= 1, "%s: faces %u", fd.cFileName, a.nf);
            CHECKF((a.ft == DWRITE_FONT_FILE_TYPE_TRUETYPE && a.fc == DWRITE_FONT_FACE_TYPE_TRUETYPE) ||
                   (a.ft == DWRITE_FONT_FILE_TYPE_CFF && a.fc == DWRITE_FONT_FACE_TYPE_CFF) ||
                   (a.ft == DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION && a.fc == DWRITE_FONT_FACE_TYPE_TRUETYPE_COLLECTION), "%s: ft=%d fc=%d", fd.cFileName, a.ft, a.fc);
            if (a.ft != DWRITE_FONT_FILE_TYPE_TRUETYPE_COLLECTION) CHECKF(a.nf == 1, "%s: nf=%u", fd.cFileName, a.nf);
        }
    } while (++done < max && FindNextFileA(h, &fd));
    FindClose(h);
}
TEST(an_sweep_ttf) { sweep("*.ttf", 60); }
TEST(an_sweep_ttc) { sweep("*.ttc", 20); }
TEST(an_sweep_otf) { sweep("*.otf", 20); }
TEST(an_sweep_fon) { sweep("*.fon", 20); }
