/* IDWriteFontFile::Analyze: what the sniffer accepts, classifies and rejects.
 *
 * Windows reads the file in a fixed order (decomp: DWriteFontFile::Analyze -> FontFileAnalyzer::Analyze):
 *   GetFileSize, then IsPfm (fragment 0,0x75), IsPfb (0,7), IsOpenType (0,0x10), IsFon (MZ/NE resource walk).
 * Any short read or inconsistent header throws FileFormatException, surfaced as DWRITE_E_FILEFORMAT, with the
 * four out values left at their defaults {FALSE, UNKNOWN(0), UNKNOWN(6), 0}. */
#include "sf.h"

#define NEED() IDWriteFactory *f_ = dw_isolated(); if (!f_) { t_skip("DWriteCreateFactory unavailable"); return; }

static uint8_t *zeros(size_t n) { uint8_t *p = (uint8_t *)calloc(n ? n : 1, 1); return p; }

/* Analyze `blob` and check the four outputs. */
static void expect_analyze(const char *label, const uint8_t *blob, size_t n, HRESULT hr, int sup, int ftype, int fctype, UINT32 nfaces) {
    sf_probe p;
    if (!sf_open(&p, blob, n)) { sf_close(&p); return; }
    CHECKF(p.hr_analyze == hr, "%s: Analyze -> %s want %s", label, dw_hr_name(p.hr_analyze), dw_hr_name(hr));
    CHECKF(p.supported == sup, "%s: isSupported %d want %d", label, (int)p.supported, sup);
    CHECKF((int)p.ftype == ftype, "%s: fileType %d want %d", label, (int)p.ftype, ftype);
    CHECKF((int)p.fctype == fctype, "%s: faceType %d want %d", label, (int)p.fctype, fctype);
    CHECKF(p.nfaces == nfaces, "%s: numberOfFaces %u want %u", label, (unsigned)p.nfaces, (unsigned)nfaces);
    sf_close(&p);
}
enum { FT_UNKNOWN = 0, FT_CFF = 1, FT_TRUETYPE = 2, FT_TTC = 3, FT_PFM = 4, FT_PFB = 5, FT_VECTOR = 6, FT_BITMAP = 7 };
enum { FC_CFF = 0, FC_TRUETYPE = 1, FC_TTC = 2, FC_TYPE1 = 3, FC_VECTOR = 4, FC_BITMAP = 5, FC_UNKNOWN = 6 };

/* ---- first-four-byte signatures, each in an otherwise zero-filled 200 byte file ------------------------------ */
#define MAGIC_TEST(name, b0, b1, b2, b3, sup, ft, fc, nf) \
    TEST(an_magic_##name) { uint8_t *b = zeros(200); b[0] = b0; b[1] = b1; b[2] = b2; b[3] = b3; \
        expect_analyze(#name, b, 200, S_OK, sup, ft, fc, nf); free(b); }
/* IsOpenType: 0x00010000, 0x00020000 and 'true' are TrueType, 'OTTO' is CFF flavoured OpenType. */
MAGIC_TEST(sfnt_00010000, 0x00, 0x01, 0x00, 0x00, TRUE, FT_TRUETYPE, FC_TRUETYPE, 1)
MAGIC_TEST(sfnt_00020000, 0x00, 0x02, 0x00, 0x00, TRUE, FT_TRUETYPE, FC_TRUETYPE, 1)
MAGIC_TEST(sfnt_true, 't', 'r', 'u', 'e', TRUE, FT_TRUETYPE, FC_TRUETYPE, 1)
MAGIC_TEST(sfnt_OTTO, 'O', 'T', 'T', 'O', TRUE, FT_CFF, FC_CFF, 1)
/* near misses are not sniffed as fonts: the result is S_OK with isSupported FALSE and UNKNOWN types. */
MAGIC_TEST(near_00010001, 0x00, 0x01, 0x00, 0x01, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(near_00030000, 0x00, 0x03, 0x00, 0x00, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(near_01000000, 0x01, 0x00, 0x00, 0x00, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(near_lower_otto, 'o', 't', 't', 'o', FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(near_TRUE_upper, 'T', 'R', 'U', 'E', FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(near_ttcF, 't', 't', 'c', 'F', FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(near_typ1, 't', 'y', 'p', '1', FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(near_OTTO_reversed, 'O', 'T', 'O', 'T', FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(all_zero, 0, 0, 0, 0, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(all_ff, 0xFF, 0xFF, 0xFF, 0xFF, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
/* WOFF/WOFF2 are containers, not fonts: Analyze does not unpack them. */
MAGIC_TEST(woff_not_unpacked, 'w', 'O', 'F', 'F', FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(woff2_not_unpacked, 'w', 'O', 'F', '2', FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)
MAGIC_TEST(text_file, 'H', 'e', 'l', 'l', FALSE, FT_UNKNOWN, FC_UNKNOWN, 0)

/* ---- collections: the face count is read verbatim from offset 8; zero is a format error --------------------------- */
static void ttc_probe(const char *label, uint32_t nfonts, HRESULT hr, int sup, int ft, int fc, UINT32 nf) {
    uint8_t *b = zeros(200); memcpy(b, "ttcf", 4); sf_w32(b + 4, 0x00010000); sf_w32(b + 8, nfonts);
    expect_analyze(label, b, 200, hr, sup, ft, fc, nf); free(b);
}
TEST(an_ttcf_1_face) { ttc_probe("ttcf n=1", 1, S_OK, TRUE, FT_TTC, FC_TTC, 1); }
TEST(an_ttcf_2_faces) { ttc_probe("ttcf n=2", 2, S_OK, TRUE, FT_TTC, FC_TTC, 2); }
TEST(an_ttcf_7_faces) { ttc_probe("ttcf n=7", 7, S_OK, TRUE, FT_TTC, FC_TTC, 7); }
TEST(an_ttcf_65536_faces_unchecked) { ttc_probe("ttcf n=65536", 65536, S_OK, TRUE, FT_TTC, FC_TTC, 65536); }
TEST(an_ttcf_ffffffff_faces_unchecked) { ttc_probe("ttcf n=0xFFFFFFFF", 0xFFFFFFFFu, S_OK, TRUE, FT_TTC, FC_TTC, 0xFFFFFFFFu); }
/* IsOpenType: a zero face count throws FileFormatException (decomp: numFonts == 0 -> throw when asserts are off). */
TEST(an_ttcf_zero_faces_is_format_error) { ttc_probe("ttcf n=0", 0, DWRITE_E_FILEFORMAT_, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); }
TEST(an_ttcf_version_not_checked) {
    uint8_t *b = zeros(200); memcpy(b, "ttcf", 4); sf_w32(b + 4, 0x00020000); sf_w32(b + 8, 3);
    expect_analyze("ttcf v2.0 n=3", b, 200, S_OK, TRUE, FT_TTC, FC_TTC, 3); sf_w32(b + 4, 0xDEADBEEF);
    expect_analyze("ttcf garbage version n=3", b, 200, S_OK, TRUE, FT_TTC, FC_TTC, 3); free(b);
}
TEST(an_real_ttc_counts_faces) {
    sf_blob a = sf_default(), b2; size_t sz; uint8_t *ttc; const uint8_t *bl[3]; size_t sizes[3];
    ttf_spec s; ttf_style_spec(&s, "TwoB", 700, 5, 0); b2 = sf_build(&s);
    bl[0] = a.p; bl[1] = b2.p; bl[2] = a.p; sizes[0] = a.n; sizes[1] = b2.n; sizes[2] = a.n;
    ttc = ttf_build_ttc(bl, sizes, 3, &sz);
    expect_analyze("real ttc", ttc, sz, S_OK, TRUE, FT_TTC, FC_TTC, 3);
    free(ttc); sf_free(&a); sf_free(&b2);
}
TEST(an_real_font_variants) {
    sf_blob a = sf_default();
    expect_analyze("default synthetic font", a.p, a.n, S_OK, TRUE, FT_TRUETYPE, FC_TRUETYPE, 1);
    a.p[0] = 0; a.p[1] = 2; expect_analyze("same font, sfnt 0x00020000", a.p, a.n, S_OK, TRUE, FT_TRUETYPE, FC_TRUETYPE, 1);
    memcpy(a.p, "true", 4); expect_analyze("same font, 'true'", a.p, a.n, S_OK, TRUE, FT_TRUETYPE, FC_TRUETYPE, 1);
    memcpy(a.p, "OTTO", 4); expect_analyze("TrueType outlines relabelled OTTO", a.p, a.n, S_OK, TRUE, FT_CFF, FC_CFF, 1);
    memcpy(a.p, "XXXX", 4); expect_analyze("bad magic", a.p, a.n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    sf_free(&a);
}
/* The sniffer looks only at the first 4 bytes: a header-only 'OTTO' file is "supported" at Analyze time. */
TEST(an_header_only_files_are_sniffed_without_validation) {
    uint8_t *b = zeros(0x75);
    memcpy(b, "OTTO", 4); expect_analyze("OTTO + zeros, 0x75 bytes", b, 0x75, S_OK, TRUE, FT_CFF, FC_CFF, 1);
    memcpy(b, "\0\1\0\0", 4); expect_analyze("00010000 + zeros, 0x75 bytes", b, 0x75, S_OK, TRUE, FT_TRUETYPE, FC_TRUETYPE, 1);
    free(b);
}

/* ---- minimum file size: IsPfm always requests 0x75 bytes at offset 0 -------------------------------------------- */
#define SIZE_TEST(n) \
    TEST(an_size_##n) { uint8_t *b = zeros(n); if (n >= 4) memcpy(b, "OTTO", 4); \
        expect_analyze("size " #n, b, n, DWRITE_E_FILEFORMAT_, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); free(b); }
SIZE_TEST(0) SIZE_TEST(1) SIZE_TEST(2) SIZE_TEST(3) SIZE_TEST(4) SIZE_TEST(5) SIZE_TEST(7) SIZE_TEST(8) SIZE_TEST(11) SIZE_TEST(12)
SIZE_TEST(13) SIZE_TEST(15) SIZE_TEST(16) SIZE_TEST(17) SIZE_TEST(31) SIZE_TEST(32) SIZE_TEST(63) SIZE_TEST(64) SIZE_TEST(100) SIZE_TEST(116)
TEST(an_size_117_is_the_smallest_analyzable_file) {
    uint8_t *b = zeros(117); memcpy(b, "OTTO", 4);
    expect_analyze("OTTO size 117", b, 117, S_OK, TRUE, FT_CFF, FC_CFF, 1);
    memcpy(b, "xxxx", 4); expect_analyze("junk size 117", b, 117, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    free(b);
}
TEST(an_truncated_real_font_sizes) {
    sf_blob a = sf_default(); size_t cut[] = { 0, 1, 4, 12, 16, 100, 116 }; size_t i;
    for (i = 0; i < sizeof cut / sizeof cut[0]; i++) {
        char l[64]; snprintf(l, sizeof l, "default font cut to %u", (unsigned)cut[i]);
        expect_analyze(l, a.p, cut[i], DWRITE_E_FILEFORMAT_, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    }
    expect_analyze("default font cut to 117", a.p, 117, S_OK, TRUE, FT_TRUETYPE, FC_TRUETYPE, 1);
    expect_analyze("default font cut to 1000 (Analyze reads only the header)", a.p, 1000, S_OK, TRUE, FT_TRUETYPE, FC_TRUETYPE, 1);
    sf_free(&a);
}

/* ---- Type 1 and bitmap/vector containers ---------------------------------------------------------------- */
TEST(an_pfm_header_is_recognised_but_unsupported) {
    /* IsPfm: version word 0x0100, dfSize (LE32 at 2) == file size, word@0x42 == 0x81, word@0x44 == 10. */
    uint8_t *b = zeros(200); b[0] = 0; b[1] = 1; b[2] = 200; b[0x42] = 0x81; b[0x44] = 10;
    expect_analyze("pfm", b, 200, S_OK, FALSE, FT_PFM, FC_TYPE1, 1);
    b[2] = 199; expect_analyze("pfm, dfSize != file size (and not an sfnt signature either)", b, 200, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    free(b);
}
TEST(an_pfm_field_by_field) {
    uint8_t *b = zeros(200); b[1] = 1; b[2] = 200; b[0x42] = 0x81; b[0x44] = 10;
    b[0x42] = 0x80; expect_analyze("pfm word@0x42=0x80", b, 200, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); b[0x42] = 0x81;
    b[0x44] = 11; expect_analyze("pfm word@0x44=11", b, 200, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); b[0x44] = 10;
    expect_analyze("pfm restored", b, 200, S_OK, FALSE, FT_PFM, FC_TYPE1, 1);
    free(b);
}
TEST(an_pfm_signature_wins_over_sfnt_signature) {
    /* A 65536-byte file starting 00 01 00 00 01 00: LE32 at offset 2 is 0x10000 == file size, so IsPfm (checked first) wins. */
    size_t n = 65536; uint8_t *b = zeros(n); b[1] = 1; b[4] = 1; b[5] = 0; b[0x42] = 0x81; b[0x44] = 10;
    expect_analyze("sfnt-looking pfm", b, n, S_OK, FALSE, FT_PFM, FC_TYPE1, 1);
    free(b);
}
TEST(an_pfb_header_is_recognised_but_unsupported) {
    uint8_t *b = zeros(200); b[0] = 0x80; b[1] = 1; b[2] = 100;     /* segment length 100 (LE32): 6 + 100 <= 200 */
    expect_analyze("pfb ascii segment", b, 200, S_OK, FALSE, FT_PFB, FC_TYPE1, 1);
    b[1] = 2; expect_analyze("pfb binary segment", b, 200, S_OK, FALSE, FT_PFB, FC_TYPE1, 1);
    b[1] = 3; expect_analyze("pfb segment type 3", b, 200, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    b[1] = 0; expect_analyze("pfb segment type 0", b, 200, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    b[1] = 1; b[2] = 194; expect_analyze("pfb 6+len == size", b, 200, S_OK, FALSE, FT_PFB, FC_TYPE1, 1);
    b[2] = 195; expect_analyze("pfb 6+len == size+1", b, 200, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    free(b);
}

/* Minimal MZ + NE executable carrying one RT_FONT resource (0x8008) with an FNT of `fnt_len` bytes. */
static uint8_t *make_fon(size_t *out_n, unsigned version, unsigned type_bit0, size_t fnt_len, uint32_t dfsize) {
    size_t n = 0x200 + fnt_len; uint8_t *b = zeros(n); uint8_t *ne = b + 0x40, *rt = ne + 0x40, *fnt = b + 0x200;
    b[0] = 'M'; b[1] = 'Z'; b[0x3c] = 0x40;
    ne[0] = 'N'; ne[1] = 'E'; ne[0x24] = 0x40; ne[0x26] = 0x80;       /* resource table at NE+0x40, resident names at NE+0x80 */
    rt[0] = 0; rt[1] = 0;                                              /* alignment shift 0: offsets are plain file offsets */
    rt[2] = 0x08; rt[3] = 0x80; rt[4] = 1; rt[5] = 0;                  /* type 0x8008 (RT_FONT), count 1 */
    rt[10] = 0x00; rt[11] = 0x02; rt[12] = (uint8_t)fnt_len; rt[13] = (uint8_t)(fnt_len >> 8);   /* offset 0x200, length */
    /* next type id is 0: terminator */
    fnt[0] = (uint8_t)version; fnt[1] = (uint8_t)(version >> 8);
    fnt[2] = (uint8_t)dfsize; fnt[3] = (uint8_t)(dfsize >> 8); fnt[4] = (uint8_t)(dfsize >> 16); fnt[5] = (uint8_t)(dfsize >> 24);
    fnt[0x42] = (uint8_t)type_bit0;
    *out_n = n; return b;
}
TEST(an_fon_bitmap_font_resource) {
    size_t n; uint8_t *b = make_fon(&n, 0x300, 0, 0x98, 0x98);
    expect_analyze("fon raster v3.00", b, n, S_OK, FALSE, FT_BITMAP, FC_BITMAP, 1); free(b);
    b = make_fon(&n, 0x200, 0, 0x98, 0x98);
    expect_analyze("fon raster v2.00", b, n, S_OK, FALSE, FT_BITMAP, FC_BITMAP, 1); free(b);
}
TEST(an_fon_raster_version_and_length_rules) {
    size_t n; uint8_t *b;
    b = make_fon(&n, 0x100, 0, 0x98, 0x98); expect_analyze("fon raster v1.00 is not accepted", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); free(b);
    b = make_fon(&n, 0x400, 0, 0x98, 0x98); expect_analyze("fon raster v4.00", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); free(b);
    b = make_fon(&n, 0x300, 0, 0x93, 0x93); expect_analyze("fon raster v3 needs length > 0x93", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); free(b);
    b = make_fon(&n, 0x200, 0, 0x76, 0x76); expect_analyze("fon raster v2, length 0x76", b, n, S_OK, FALSE, FT_BITMAP, FC_BITMAP, 1); free(b);
    b = make_fon(&n, 0x200, 0, 0x75, 0x75); expect_analyze("fon raster v2, length 0x75 is not > 0x75", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); free(b);
    b = make_fon(&n, 0x200, 0, 0x74, 0x74); expect_analyze("fon font resource shorter than 0x75", b, n, DWRITE_E_FILEFORMAT_, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); free(b);
}
TEST(an_fon_vector_font_resource) {
    size_t n; uint8_t *b = make_fon(&n, 0x100, 1, 0x98, 0x98);
    expect_analyze("fon vector v1.00", b, n, S_OK, FALSE, FT_VECTOR, FC_VECTOR, 1); free(b);
    b = make_fon(&n, 0x200, 1, 0x98, 0x98);
    expect_analyze("fon vector v2.00", b, n, S_OK, FALSE, FT_VECTOR, FC_VECTOR, 1); free(b);
    b = make_fon(&n, 0x300, 1, 0x98, 0x98);
    expect_analyze("fon vector v3.00 is not accepted", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); free(b);
    b = make_fon(&n, 0x100, 1, 0x98, 0x99);
    expect_analyze("fon vector dfSize > resource length", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); free(b);
}
TEST(an_fon_without_font_resource_is_unknown) {
    size_t n; uint8_t *b = make_fon(&n, 0x300, 0, 0x98, 0x98);
    b[0x40 + 0x40 + 2] = 0x09;             /* RT_FONTDIR-like type 0x8009 instead of 0x8008 -> resource not found, type walk ends at id 0 */
    expect_analyze("fon with another resource type", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    free(b);
}
TEST(an_fon_ne_header_checks) {
    size_t n; uint8_t *b = make_fon(&n, 0x300, 0, 0x98, 0x98);
    b[0x40] = 'P'; expect_analyze("MZ + 'PE' header", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); b[0x40] = 'N';
    b[0x40 + 0x26] = 0x40; expect_analyze("NE resource table offset == names offset", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); b[0x40 + 0x26] = 0x80;
    b[0] = 'Z'; expect_analyze("no MZ", b, n, S_OK, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0); b[0] = 'M';
    expect_analyze("restored", b, n, S_OK, FALSE, FT_BITMAP, FC_BITMAP, 1);
    free(b);
}
TEST(an_fon_new_header_offset_beyond_file) {
    size_t n; uint8_t *b = make_fon(&n, 0x300, 0, 0x98, 0x98);
    b[0x3c] = 0xF0; b[0x3d] = 0xFF; b[0x3e] = 0xFF; b[0x3f] = 0x7F;      /* e_lfanew far past EOF: the 0x40 byte read fails -> format error */
    expect_analyze("MZ with e_lfanew beyond the file", b, n, DWRITE_E_FILEFORMAT_, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    free(b);
}
TEST(an_fon_resource_beyond_file) {
    size_t n; uint8_t *b = make_fon(&n, 0x300, 0, 0x98, 0x98);
    b[0x40 + 0x40 + 10] = 0xF0; b[0x40 + 0x40 + 11] = 0xFF;             /* resource offset 0xFFF0 > file size */
    expect_analyze("FNT resource offset beyond EOF", b, n, DWRITE_E_FILEFORMAT_, FALSE, FT_UNKNOWN, FC_UNKNOWN, 0);
    free(b);
}

/* ---- argument validation (decomp of DWriteFontFile::Analyze) ----------------------------------------------- */
TEST(an_null_issupported_is_invalidarg_and_others_defaulted) {
    if (t_is_wine()) { t_skip("Wine dereferences NULL output pointers; Windows returns E_INVALIDARG"); return; }
    sf_blob b = sf_default(); sf_probe p; UINT32 nf = 77; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)77; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)77;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    CHECK_HR(IDWriteFontFile_Analyze(p.file, NULL, &ft, &fc, &nf), E_INVALIDARG);
    /* the non-NULL outputs are still written with their defaults before the argument check */
    CHECK(ft == 0); CHECK(fc == DWRITE_FONT_FACE_TYPE_UNKNOWN); CHECK(nf == 0);
    sf_close(&p); sf_free(&b);
}
TEST(an_null_filetype_is_invalidarg) {
    if (t_is_wine()) { t_skip("Wine dereferences NULL output pointers; Windows returns E_INVALIDARG"); return; }
    sf_blob b = sf_default(); sf_probe p; BOOL s = 77; UINT32 nf = 77; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)77;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    CHECK_HR(IDWriteFontFile_Analyze(p.file, &s, NULL, &fc, &nf), E_INVALIDARG);
    CHECK(s == FALSE); CHECK(fc == DWRITE_FONT_FACE_TYPE_UNKNOWN); CHECK(nf == 0);
    sf_close(&p); sf_free(&b);
}
TEST(an_null_numfaces_is_invalidarg) {
    if (t_is_wine()) { t_skip("Wine dereferences NULL output pointers; Windows returns E_INVALIDARG"); return; }
    sf_blob b = sf_default(); sf_probe p; BOOL s = 77; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)77; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)77;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    CHECK_HR(IDWriteFontFile_Analyze(p.file, &s, &ft, &fc, NULL), E_INVALIDARG);
    CHECK(s == FALSE); CHECK(ft == 0); CHECK(fc == DWRITE_FONT_FACE_TYPE_UNKNOWN);
    sf_close(&p); sf_free(&b);
}
TEST(an_null_facetype_is_accepted) {
    /* Unlike the other three outputs, the DWRITE_FONT_FACE_TYPE pointer is optional in the Windows 10 implementation. */
    sf_blob b = sf_default(); sf_probe p; BOOL s = 77; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)77; UINT32 nf = 77;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    CHECK_HR(IDWriteFontFile_Analyze(p.file, &s, &ft, NULL, &nf), S_OK);
    CHECK(s == TRUE); CHECK(ft == DWRITE_FONT_FILE_TYPE_TRUETYPE); CHECK(nf == 1);
    sf_close(&p); sf_free(&b);
}
TEST(an_all_null_is_invalidarg) {
    if (t_is_wine()) { t_skip("Wine dereferences NULL output pointers; Windows returns E_INVALIDARG"); return; }
    sf_blob b = sf_default(); sf_probe p;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    CHECK_HR(IDWriteFontFile_Analyze(p.file, NULL, NULL, NULL, NULL), E_INVALIDARG);
    sf_close(&p); sf_free(&b);
}
TEST(an_repeatable) {
    sf_blob b = sf_default(); sf_probe p; int i;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    for (i = 0; i < 4; i++) {
        BOOL s = 0; DWRITE_FONT_FILE_TYPE ft = (DWRITE_FONT_FILE_TYPE)0; DWRITE_FONT_FACE_TYPE fc = (DWRITE_FONT_FACE_TYPE)0; UINT32 nf = 0;
        CHECK_HR(IDWriteFontFile_Analyze(p.file, &s, &ft, &fc, &nf), S_OK);
        CHECK(s == TRUE); CHECK(ft == DWRITE_FONT_FILE_TYPE_TRUETYPE); CHECK(fc == DWRITE_FONT_FACE_TYPE_TRUETYPE); CHECK(nf == 1);
    }
    sf_close(&p); sf_free(&b);
}
TEST(an_outputs_defaulted_when_failing) {
    sf_blob b = sf_default(); sf_probe p;
    if (!sf_open(&p, b.p, 50)) { sf_close(&p); sf_free(&b); return; }
    CHECK_HR(p.hr_analyze, DWRITE_E_FILEFORMAT_);
    CHECK(p.supported == FALSE); CHECK(p.ftype == 0); CHECK(p.fctype == DWRITE_FONT_FACE_TYPE_UNKNOWN); CHECK(p.nfaces == 0);
    sf_close(&p); sf_free(&b);
}

/* ---- callbacks seen by the application's stream ------------------------------------------------------------- */
TEST(an_stream_call_sequence_for_a_truetype_file) {
    sf_blob b = sf_default(); sf_probe p; dwx_mem_stream *s;
    if (!sf_open(&p, b.p, b.n)) { sf_close(&p); sf_free(&b); return; }
    CHECK_HR(p.hr_analyze, S_OK);
    s = sf_stream(&p);
    CHECK(s != NULL);
    if (s) {
        /* FileSize first, then the fragments IsPfm (0,0x75), IsPfb (0,7), IsOpenType (0,0x10). */
        CHECK(s->n_getsize >= 1);
        CHECK(s->nlogged >= 3);
        if (s->nlogged >= 3) {
            CHECK(s->reads[0].offset == 0 && s->reads[0].size == 0x75);
            CHECK(s->reads[1].offset == 0 && s->reads[1].size == 7);
            CHECK(s->reads[2].offset == 0 && s->reads[2].size == 0x10);
        }
        CHECK(s->nlogged == 3);
        CHECKF(s->outstanding == 0, "fragments still outstanding: %ld", (long)s->outstanding);
        CHECK(s->n_release == s->n_read);
        t_note("stream refcount after Analyze: %ld", (long)dwx_refs(s));
    }
    sf_close(&p); sf_free(&b);
}
TEST(an_stream_reads_stop_early_for_pfm) {
    uint8_t *b = zeros(200); sf_probe p; dwx_mem_stream *s;
    b[1] = 1; b[2] = 200; b[0x42] = 0x81; b[0x44] = 10;
    if (!sf_open(&p, b, 200)) { sf_close(&p); free(b); return; }
    s = sf_stream(&p);
    CHECK_HR(p.hr_analyze, S_OK);
    if (s) { CHECK(s->nlogged == 1); CHECK(s->reads[0].size == 0x75); CHECK(s->outstanding == 0); }
    sf_close(&p); free(b);
}
TEST(an_stream_reads_for_unknown_content) {
    uint8_t *b = zeros(200); sf_probe p; dwx_mem_stream *s;
    if (!sf_open(&p, b, 200)) { sf_close(&p); free(b); return; }
    s = sf_stream(&p);
    CHECK_HR(p.hr_analyze, S_OK);
    if (s) {
        /* IsPfm, IsPfb, IsOpenType, then IsFon -> HasFntResource reads 0x40 bytes at 0. */
        CHECK(s->nlogged == 4);
        if (s->nlogged == 4) { CHECK(s->reads[3].offset == 0 && s->reads[3].size == 0x40); }
        CHECK(s->outstanding == 0);
    }
    sf_close(&p); free(b);
}
TEST(an_stream_getfilesize_failure_propagates) {
    sf_blob b = sf_default(); sf_probe p;
    {
        dwx_mem_loader *ld = dwx_loader_new(); IDWriteFactory *f = dw_isolated(); IDWriteFontFile *file = NULL; UINT32 key;
        BOOL s = 77; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf; HRESULT hr;
        key = dwx_loader_add(ld, b.p, b.n);
        ld->stream_fail_getsize = E_FAIL;
        if (SUCCEEDED(dwx_loader_register(ld, f)) && SUCCEEDED(dwx_loader_file(ld, f, key, &file))) {
            hr = IDWriteFontFile_Analyze(file, &s, &ft, &fc, &nf);
            t_note("Analyze with GetFileSize -> E_FAIL: %s", dw_hr_name(hr));
            CHECK_HR_FAILED(hr);
            CHECK(s == FALSE);
        }
        DW_RELEASE(file); dwx_loader_unregister(ld); dwx_release(ld);
    }
    sf_free(&b);
}
TEST(an_stream_readfragment_failure_propagates) {
    sf_blob b = sf_default();
    dwx_mem_loader *ld = dwx_loader_new(); IDWriteFactory *f = dw_isolated(); IDWriteFontFile *file = NULL; UINT32 key;
    BOOL s = 77; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf; HRESULT hr;
    key = dwx_loader_add(ld, b.p, b.n);
    ld->stream_fail_read = E_ACCESSDENIED;
    if (SUCCEEDED(dwx_loader_register(ld, f)) && SUCCEEDED(dwx_loader_file(ld, f, key, &file))) {
        hr = IDWriteFontFile_Analyze(file, &s, &ft, &fc, &nf);
        t_note("Analyze with ReadFileFragment -> E_ACCESSDENIED (file size is fine): %s", dw_hr_name(hr));
        CHECK_HR_FAILED(hr);
        CHECK(s == FALSE);
    }
    DW_RELEASE(file); dwx_loader_unregister(ld); dwx_release(ld); sf_free(&b);
}
TEST(an_loader_createstream_failure_surfaces_at_analyze) {
    sf_blob b = sf_default();
    dwx_mem_loader *ld = dwx_loader_new(); IDWriteFactory *f = dw_isolated(); IDWriteFontFile *file = NULL; UINT32 key;
    BOOL s = 77; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf; HRESULT hr, hrf;
    key = dwx_loader_add(ld, b.p, b.n);
    ld->fail_create = E_FAIL;
    if (SUCCEEDED(dwx_loader_register(ld, f))) {
        hrf = dwx_loader_file(ld, f, key, &file);
        t_note("CreateCustomFontFileReference with a failing loader: %s (file %s)", dw_hr_name(hrf), file ? "created" : "null");
        if (SUCCEEDED(hrf) && file) {
            hr = IDWriteFontFile_Analyze(file, &s, &ft, &fc, &nf);
            t_note("Analyze when CreateStreamFromKey fails: %s", dw_hr_name(hr));
            CHECK_HR_FAILED(hr);
        } else CHECK_OUT_NULL(file);
    }
    DW_RELEASE(file); dwx_loader_unregister(ld); dwx_release(ld); sf_free(&b);
}
