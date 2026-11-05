/* IDWriteFactory5::AnalyzeContainerType and UnpackFontFile.
 * Observed: IDWriteFactory::AnalyzeContainerType -> AnalyzeContainerType (first four bytes only: 'wOFF' = 1, 'wOF2' = 2,
 * anything else, including fewer than four bytes, = UNKNOWN).  UnpackFontFile -> WoffDecoder::ParseWoffHeader / UnpackWoffData and
 * Woff2Decoder::UnpackFont; every inconsistency throws FileFormatException (DWRITE_E_FILEFORMAT), *fontFileStream is NULL first. */
#include "sf.h"

static IDWriteFactory5 *fac5(void) { return (IDWriteFactory5 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 5); }
#define POISON ((IDWriteFontFileStream *)(ULONG_PTR)1)
#define REL_STREAM(s) do { if ((s) && (s) != POISON) IDWriteFontFileStream_Release(s); (s) = NULL; } while (0)
#define NEED5() IDWriteFactory5 *f5 = fac5(); if (!f5) { t_skip("IDWriteFactory5 unsupported"); return; }
#define CT_UNKNOWN DWRITE_CONTAINER_TYPE_UNKNOWN
#define CT_WOFF    DWRITE_CONTAINER_TYPE_WOFF
#define CT_WOFF2   DWRITE_CONTAINER_TYPE_WOFF2

TEST(wf_enum_values) {
    EXPECT_EQ(DWRITE_CONTAINER_TYPE_UNKNOWN, 0); EXPECT_EQ(DWRITE_CONTAINER_TYPE_WOFF, 1); EXPECT_EQ(DWRITE_CONTAINER_TYPE_WOFF2, 2);
}
TEST(wf_factory5_is_available) {
    void *p = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 5);
    t_note("IDWriteFactory5 on the shared factory: %s", p ? "yes" : "no");
    CHECK(p != NULL);        /* Windows 10 1703 and later */
}

/* ---- AnalyzeContainerType --------------------------------------------------------------------------------- */
#define ACT(name, len, want, ...) TEST(wf_act_##name) { NEED5(); static const uint8_t d[] = { __VA_ARGS__ }; \
    CHECK(IDWriteFactory5_AnalyzeContainerType(f5, d, len) == want); }
ACT(woff_4, 4, CT_WOFF, 'w','O','F','F')
ACT(woff2_4, 4, CT_WOFF2, 'w','O','F','2')
ACT(woff_long, 12, CT_WOFF, 'w','O','F','F',1,2,3,4,5,6,7,8)
ACT(woff2_long, 8, CT_WOFF2, 'w','O','F','2',0,0,0,0)
ACT(woff_3_bytes, 3, CT_UNKNOWN, 'w','O','F')
ACT(woff_1_byte, 1, CT_UNKNOWN, 'w')
ACT(woff_size_zero_nonnull, 0, CT_UNKNOWN, 'w','O','F','F')
ACT(upper_woff, 4, CT_UNKNOWN, 'W','O','F','F')
ACT(woff3, 4, CT_UNKNOWN, 'w','O','F','3')
ACT(woff_lowercase_f, 4, CT_UNKNOWN, 'w','O','F','f')
ACT(sfnt, 8, CT_UNKNOWN, 0,1,0,0,0,0,0,0)
ACT(otto, 4, CT_UNKNOWN, 'O','T','T','O')
ACT(ttcf, 4, CT_UNKNOWN, 't','t','c','f')
ACT(shifted_signature, 5, CT_UNKNOWN, 0,'w','O','F','F')
ACT(size_limits_signature_to_size, 3, CT_UNKNOWN, 'w','O','F','F',0,0,0,0)
ACT(zeros, 4, CT_UNKNOWN, 0,0,0,0)
TEST(wf_act_null_zero_size) { NEED5(); CHECK(IDWriteFactory5_AnalyzeContainerType(f5, NULL, 0) == CT_UNKNOWN); }
TEST(wf_act_null_size_below_four) { NEED5(); if (t_is_wine()) { t_skip("Wine may touch the pointer"); return; }
    /* Windows only dereferences when at least four bytes are available (length test comes first). */
    CHECK(IDWriteFactory5_AnalyzeContainerType(f5, NULL, 3) == CT_UNKNOWN); }
TEST(wf_act_real_woff_wrapper) {
    NEED5(); sf_blob a = sf_default(); size_t n; uint8_t *w = ttf_wrap_woff(a.p, a.n, &n);
    CHECK(IDWriteFactory5_AnalyzeContainerType(f5, w, (UINT32)n) == CT_WOFF);
    CHECK(IDWriteFactory5_AnalyzeContainerType(f5, a.p, (UINT32)a.n) == CT_UNKNOWN);
    free(w); sf_free(&a);
}
TEST(wf_act_isolated_factory_agrees) {
    IDWriteFactory5 *f = (IDWriteFactory5 *)dw_factory_v(DWRITE_FACTORY_TYPE_ISOLATED, 5);
    static const uint8_t d[] = { 'w','O','F','2' };
    if (!f) { t_skip("IDWriteFactory5 unsupported"); return; }
    CHECK(IDWriteFactory5_AnalyzeContainerType(f, d, 4) == CT_WOFF2);
}

/* ---- helpers for streams returned by UnpackFontFile -------------------------------------------------------------- */
static uint8_t *drain(IDWriteFontFileStream *s, size_t *n) {
    UINT64 sz = 0; const void *start = NULL; void *ctx = NULL; uint8_t *out = NULL;
    *n = 0;
    if (FAILED(IDWriteFontFileStream_GetFileSize(s, &sz))) return NULL;
    if (FAILED(IDWriteFontFileStream_ReadFileFragment(s, &start, 0, sz, &ctx))) return NULL;
    out = (uint8_t *)malloc((size_t)sz + 1); memcpy(out, start, (size_t)sz);
    IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
    *n = (size_t)sz; return out;
}
static uint8_t *make_woff(size_t *n) { sf_blob a = sf_default(); uint8_t *w = ttf_wrap_woff(a.p, a.n, n); sf_free(&a); return w; }

TEST(wf_unpack_woff_roundtrip) {
    NEED5(); sf_blob a = sf_default(); size_t wn, un = 0; uint8_t *w = ttf_wrap_woff(a.p, a.n, &wn), *u;
    IDWriteFontFileStream *s = POISON; int i;
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s), S_OK);
    CHECK(s != NULL && s != (IDWriteFontFileStream *)(ULONG_PTR)1);
    if (s && s != (IDWriteFontFileStream *)(ULONG_PTR)1) {
        EXPECT_REFS(s, 1);
        u = drain(s, &un);
        CHECK(u != NULL);
        CHECKF(un == a.n, "unpacked size %u want %u (header totalSfntSize)", (unsigned)un, (unsigned)a.n);
        if (u && un == a.n) {
            int nt = sf_ntables(&a); CHECK(sf_r32(u) == 0x00010000); CHECK(sf_r16(u + 4) == (unsigned)nt);
            for (i = 0; i < nt; i++) {
                const uint8_t *e = sf_dirent(&a, i); unsigned l = 0; const uint8_t *src, *dst; char tag[5] = { 0 };
                memcpy(tag, e, 4);
                src = ttf_find_table(a.p, a.n, tag, &l); dst = ttf_find_table(u, un, tag, &l);
                CHECKF(dst != NULL, "table %s present after unpack", tag);
                if (dst && src) CHECKF(!memcmp(src, dst, l), "table %s bytes identical", tag);
            }
            t_note("unpacked sfnt byte-identical to the original: %s", !memcmp(u, a.p, a.n) ? "yes" : "no");
        }
        free(u);
        IDWriteFontFileStream_Release(s);
    }
    free(w); sf_free(&a);
}
TEST(wf_unpack_result_is_a_loadable_font) {
    NEED5(); sf_blob a = sf_default(); size_t wn, un = 0; uint8_t *w = ttf_wrap_woff(a.p, a.n, &wn), *u; IDWriteFontFileStream *s = NULL;
    if (FAILED(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s)) || !s) { t_skip("unpack failed"); free(w); sf_free(&a); return; }
    u = drain(s, &un);
    IDWriteFontFileStream_Release(s);
    if (u) {
        sf_probe p;
        if (sf_open_face(&p, u, un)) {
            CHECK_HR(p.hr_analyze, S_OK); CHECK(p.supported); CHECK_HR(p.hr_face, S_OK);
            if (p.face) { CHECK(sf_gid(p.face, 'A') == ttf_default_gid('A')); CHECK(sf_gid(p.face, 'z') == ttf_default_gid('z')); }
        }
        sf_close(&p); free(u);
    }
    free(w); sf_free(&a);
}
TEST(wf_unpack_stream_interface) {
    NEED5(); size_t wn; uint8_t *w = make_woff(&wn); IDWriteFontFileStream *s = NULL; UINT64 t = 0; HRESULT hr; IUnknown *u = NULL;
    if (FAILED(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s)) || !s) { t_skip("unpack failed"); free(w); return; }
    hr = IDWriteFontFileStream_GetLastWriteTime(s, &t);
    t_note("unpacked stream GetLastWriteTime: %s (%llu)", dw_hr_name(hr), (unsigned long long)t);
    CHECK_HR(IDWriteFontFileStream_QueryInterface(s, &IID_IUnknown, (void **)&u), S_OK);
    if (u) { IUnknown_Release(u); }
    CHECK_HR(IDWriteFontFileStream_QueryInterface(s, &IID_IDWriteFontFileStream, (void **)&u), S_OK);
    if (u) { IUnknown_Release(u); }
    EXPECT_REFS(s, 1);
    {   /* out of range fragment request */
        const void *st = (const void *)1; void *ctx = (void *)1; UINT64 sz = 0; IDWriteFontFileStream_GetFileSize(s, &sz);
        hr = IDWriteFontFileStream_ReadFileFragment(s, &st, sz, 1, &ctx);
        CHECK_HR_FAILED(hr); PRED("fragment outputs cleared on failure", st == NULL && ctx == NULL);
        hr = IDWriteFontFileStream_ReadFileFragment(s, &st, sz + 1, 0, &ctx);
        t_note("fragment (size+1, 0): %s", dw_hr_name(hr)); if (SUCCEEDED(hr)) IDWriteFontFileStream_ReleaseFileFragment(s, ctx);
        hr = IDWriteFontFileStream_ReadFileFragment(s, &st, 0, sz + 1, &ctx);
        CHECK_HR_FAILED(hr);
    }
    IDWriteFontFileStream_Release(s); free(w);
}
TEST(wf_unpack_twice_gives_distinct_streams) {
    NEED5(); size_t wn; uint8_t *w = make_woff(&wn); IDWriteFontFileStream *a = NULL, *b = NULL;
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &a), S_OK);
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &b), S_OK);
    CHECK(a && b && a != b);
    DW_RELEASE(a); DW_RELEASE(b); free(w);
}
TEST(wf_unpack_input_buffer_not_retained) {
    NEED5(); size_t wn, un = 0, un2 = 0; uint8_t *w = make_woff(&wn), *u1, *u2; IDWriteFontFileStream *s = NULL;
    if (FAILED(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s)) || !s) { t_skip("unpack failed"); free(w); return; }
    u1 = drain(s, &un); memset(w, 0xEE, wn); u2 = drain(s, &un2);       /* the output is a separate copy */
    CHECK(u1 && u2 && un == un2 && !memcmp(u1, u2, un));
    free(u1); free(u2); IDWriteFontFileStream_Release(s); free(w);
}
TEST(wf_unpack_invalid_container_types) {
    NEED5(); size_t wn; uint8_t *w = make_woff(&wn); IDWriteFontFileStream *s = POISON;
    DWRITE_CONTAINER_TYPE bad[] = { CT_UNKNOWN, (DWRITE_CONTAINER_TYPE)3, (DWRITE_CONTAINER_TYPE)4, (DWRITE_CONTAINER_TYPE)0x100, (DWRITE_CONTAINER_TYPE)0xFFFFFFFF };
    size_t i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        s = POISON;
        CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, bad[i], w, (UINT32)wn, &s), E_INVALIDARG);   /* ArgumentException("containerType") */
        CHECK_OUT_NULL(s);
    }
    free(w);
}
TEST(wf_unpack_container_type_decides_not_the_signature) {
    NEED5(); size_t wn; uint8_t *w = make_woff(&wn); IDWriteFontFileStream *s = POISON;
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF2, w, (UINT32)wn, &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s);
    free(w);
}
TEST(wf_unpack_woff_type_with_other_signatures) {
    NEED5(); sf_blob a = sf_default(); IDWriteFontFileStream *s = POISON;
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, a.p, (UINT32)a.n, &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s);
    s = POISON;
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF2, a.p, (UINT32)a.n, &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s);
    sf_free(&a);
}
TEST(wf_unpack_woff_empty_input) {
    NEED5(); IDWriteFontFileStream *s = POISON; uint8_t one = 0;
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, &one, 0, &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s);
    s = POISON;
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF2, &one, 0, &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s);
}
TEST(wf_unpack_woff_header_sizes_below_44) {
    NEED5(); size_t wn; uint8_t *w = make_woff(&wn); unsigned n; IDWriteFontFileStream *s;
    for (n = 0; n < 44; n += 4) {
        s = POISON;
        CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, n, &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s);
    }
    free(w);
}
TEST(wf_unpack_woff_truncated_everywhere) {
    NEED5(); size_t wn; uint8_t *w = make_woff(&wn); IDWriteFontFileStream *s; size_t cut[] = { 44, 45, 100, 200, 0, 0 }; size_t i;
    cut[4] = wn / 2; cut[5] = wn - 1;
    for (i = 0; i < 6; i++) {
        s = POISON;
        /* header.length no longer equals the buffer size */
        CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)cut[i], &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s);
    }
    free(w);
}

/* ---- table-driven WOFF header damage: every mutation must be a format error ----------------------------------------- */
typedef void (*woff_mut)(uint8_t *w, size_t n);
static void m_len_plus1(uint8_t *w, size_t n) { sf_w32(w + 8, sf_r32(w + 8) + 1); }
static void m_len_minus4(uint8_t *w, size_t n) { sf_w32(w + 8, sf_r32(w + 8) - 4); }
static void m_numtables0(uint8_t *w, size_t n) { sf_w16(w + 12, 0); }
static void m_numtables_plus1(uint8_t *w, size_t n) { sf_w16(w + 12, sf_r16(w + 12) + 1); }
static void m_numtables_huge(uint8_t *w, size_t n) { sf_w16(w + 12, 0xFFFF); }
static void m_reserved(uint8_t *w, size_t n) { sf_w16(w + 14, 1); }
static void m_sig(uint8_t *w, size_t n) { w[0] = 'W'; }
static void m_meta_off_only(uint8_t *w, size_t n) { sf_w32(w + 0x18, 0x1000); }
static void m_meta_len_only(uint8_t *w, size_t n) { sf_w32(w + 0x1c, 16); }
static void m_priv_off_only(uint8_t *w, size_t n) { sf_w32(w + 0x24, 0x1000); }
static void m_priv_len_only(uint8_t *w, size_t n) { sf_w32(w + 0x28, 16); }
static void m_meta_beyond(uint8_t *w, size_t n) { sf_w32(w + 0x18, (uint32_t)n); sf_w32(w + 0x1c, 16); sf_w32(w + 0x20, 16); }
static void m_priv_beyond(uint8_t *w, size_t n) { sf_w32(w + 0x24, (uint32_t)n); sf_w32(w + 0x28, 16); }
static void m_sfntsize_plus4(uint8_t *w, size_t n) { sf_w32(w + 0x10, sf_r32(w + 0x10) + 4); }
static void m_sfntsize_minus4(uint8_t *w, size_t n) { sf_w32(w + 0x10, sf_r32(w + 0x10) - 4); }
static void m_sfntsize_zero(uint8_t *w, size_t n) { sf_w32(w + 0x10, 0); }
static void m_tab0_offset_plus4(uint8_t *w, size_t n) { sf_w32(w + 44 + 4, sf_r32(w + 44 + 4) + 4); }
static void m_tab0_offset_minus4(uint8_t *w, size_t n) { sf_w32(w + 44 + 4, sf_r32(w + 44 + 4) - 4); }
static void m_tab0_offset_zero(uint8_t *w, size_t n) { sf_w32(w + 44 + 4, 0); }
static void m_tab0_orig_less_than_comp(uint8_t *w, size_t n) { sf_w32(w + 44 + 12, sf_r32(w + 44 + 8) - 1); }
static void m_tab1_comp_bigger(uint8_t *w, size_t n) { sf_w32(w + 44 + 20 + 8, sf_r32(w + 44 + 20 + 8) + 4); }
static void m_tab_last_orig_bigger(uint8_t *w, size_t n) { int k = sf_r16(w + 12) - 1; sf_w32(w + 44 + 20 * k + 12, sf_r32(w + 44 + 20 * k + 12) + 8); }
static void m_comp_garbage_zlib(uint8_t *w, size_t n) {
    /* claim table 0 is compressed (comp < orig) but its bytes are not a zlib stream */
    uint32_t orig = sf_r32(w + 44 + 12); sf_w32(w + 44 + 8, orig >= 8 ? orig - 4 : 0); }
static void m_padding_nonzero(uint8_t *w, size_t n) {
    int k, nt = sf_r16(w + 12);
    for (k = 0; k < nt; k++) { uint32_t off = sf_r32(w + 44 + 20 * k + 4), l = sf_r32(w + 44 + 20 * k + 8); if (l & 3) { w[off + l] = 0x5A; return; } }
}
#define WOFF_BAD(name, mut) TEST(wf_bad_##name) { NEED5(); size_t wn; uint8_t *w = make_woff(&wn); IDWriteFontFileStream *s = POISON; \
    mut(w, wn); CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s); REL_STREAM(s); free(w); }
WOFF_BAD(length_plus_1, m_len_plus1)
WOFF_BAD(length_minus_4, m_len_minus4)
WOFF_BAD(numtables_zero, m_numtables0)
WOFF_BAD(numtables_plus_1, m_numtables_plus1)
WOFF_BAD(numtables_ffff, m_numtables_huge)
WOFF_BAD(reserved_nonzero, m_reserved)
WOFF_BAD(signature_case, m_sig)
WOFF_BAD(meta_offset_without_length, m_meta_off_only)
WOFF_BAD(meta_length_without_offset, m_meta_len_only)
WOFF_BAD(priv_offset_without_length, m_priv_off_only)
WOFF_BAD(priv_length_without_offset, m_priv_len_only)
WOFF_BAD(meta_block_beyond_end, m_meta_beyond)
WOFF_BAD(priv_block_beyond_end, m_priv_beyond)
WOFF_BAD(totalsfntsize_plus_4, m_sfntsize_plus4)
WOFF_BAD(totalsfntsize_minus_4, m_sfntsize_minus4)
WOFF_BAD(totalsfntsize_zero, m_sfntsize_zero)
WOFF_BAD(table0_offset_plus_4, m_tab0_offset_plus4)
WOFF_BAD(table0_offset_minus_4, m_tab0_offset_minus4)
WOFF_BAD(table0_offset_zero, m_tab0_offset_zero)
WOFF_BAD(table0_origlength_below_complength, m_tab0_orig_less_than_comp)
WOFF_BAD(table1_complength_above_origlength, m_tab1_comp_bigger)
WOFF_BAD(lasttable_origlength_inflated, m_tab_last_orig_bigger)
TEST(wf_nonzero_table_padding) { NEED5(); size_t wn; uint8_t *w = make_woff(&wn); IDWriteFontFileStream *s = POISON; HRESULT hr; m_padding_nonzero(w, wn);
    hr = IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s); t_note("WOFF with a non-zero pad byte after a table: %s", dw_hr_name(hr)); PRED("padding must be zero", hr == DWRITE_E_FILEFORMAT_);
    if (FAILED(hr)) CHECK_OUT_NULL(s); REL_STREAM(s); free(w); }
/* Rebuild the WOFF so that table k is declared compressed (compLength 8 < origLength) but holds bytes that are not a zlib stream. */
static uint8_t *woff_bad_zlib(const uint8_t *w, size_t wn, int k, size_t *out_n) {
    int nt = sf_r16(w + 12), i; sf_buf o = { 0, 0, 0 }; size_t pos = 44 + 20 * (size_t)nt;
    for (i = 0; i < 44; i++) sf_b8(&o, w[i]);
    for (i = 0; i < nt; i++) {
        const uint8_t *e = w + 44 + 20 * i; uint32_t l = i == k ? 8 : sf_r32(e + 8);
        sf_b8(&o, e[0]); sf_b8(&o, e[1]); sf_b8(&o, e[2]); sf_b8(&o, e[3]);
        sf_b32(&o, (uint32_t)pos); sf_b32(&o, l); sf_b32(&o, sf_r32(e + 12)); sf_b32(&o, sf_r32(e + 16));
        pos += (l + 3) & ~3u;
    }
    for (i = 0; i < nt; i++) {
        const uint8_t *e = w + 44 + 20 * i; uint32_t l = i == k ? 8 : sf_r32(e + 8), j;
        for (j = 0; j < l; j++) sf_b8(&o, i == k ? (0xA0 + j) : w[sf_r32(e + 4) + j]);
        while (o.n & 3) sf_b8(&o, 0);
    }
    sf_w32(o.p + 8, (uint32_t)o.n); *out_n = o.n; (void)wn; return o.p;
}
TEST(wf_bad_corrupt_zlib_stream) {
    NEED5(); size_t wn, n2; uint8_t *w = make_woff(&wn), *w2; IDWriteFontFileStream *s = POISON; HRESULT hr;
    w2 = woff_bad_zlib(w, wn, 3, &n2);
    hr = IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w2, (UINT32)n2, &s);
    t_note("WOFF with a table declared compressed but holding non-zlib bytes: %s", dw_hr_name(hr));
    CHECK_HR_FAILED(hr); CHECK_OUT_NULL(s); REL_STREAM(s); free(w); free(w2);
}
TEST(wf_bad_every_table_corrupt_zlib) {
    NEED5(); size_t wn, n2; uint8_t *w = make_woff(&wn), *w2; int k, nt; IDWriteFontFileStream *s;
    nt = sf_r16(w + 12);
    for (k = 0; k < nt; k++) {
        s = POISON; w2 = woff_bad_zlib(w, wn, k, &n2);
        CHECKF(FAILED(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w2, (UINT32)n2, &s)), "table %d", k);
        CHECK_OUT_NULL(s); REL_STREAM(s); free(w2);
    }
    free(w);
}
TEST(wf_woff_flavor_field) {
    NEED5(); size_t wn, un = 0; uint8_t *w = make_woff(&wn), *u; IDWriteFontFileStream *s = NULL; HRESULT hr;
    memcpy(w + 4, "OTTO", 4);
    hr = IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s);
    t_note("WOFF flavor 'OTTO' on TrueType tables: %s", dw_hr_name(hr));
    if (SUCCEEDED(hr) && s) { u = drain(s, &un); if (u) { t_note("unpacked sfnt version: %02x%02x%02x%02x", u[0], u[1], u[2], u[3]); PRED("flavor copied to sfnt version", !memcmp(u, "OTTO", 4)); free(u); } }
    REL_STREAM(s); free(w);
}
TEST(wf_woff_directory_in_any_order_is_accepted) {
    /* ParseWoffHeader sorts the records by file offset before validating, so the directory order is free; the sfnt keeps the WOFF directory order. */
    NEED5(); size_t wn, un = 0; uint8_t *w = make_woff(&wn), *u; IDWriteFontFileStream *s = NULL; uint8_t t[20]; HRESULT hr;
    memcpy(t, w + 44, 20); memcpy(w + 44, w + 44 + 20, 20); memcpy(w + 44 + 20, t, 20);
    hr = IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s);
    t_note("WOFF with the first two directory records swapped: %s", dw_hr_name(hr));
    PRED("swapped directory accepted", SUCCEEDED(hr));
    if (SUCCEEDED(hr) && s) {
        u = drain(s, &un);
        if (u) { PRED("size preserved", un == sf_r32(w + 0x10)); PRED("first sfnt record is the WOFF first record", !memcmp(u + 12, w + 44, 4)); free(u); }
    }
    DW_RELEASE(s); free(w);
}
TEST(wf_woff_with_metadata_block) {
    /* A well-formed metadata block (offset/length consistent, zero padded) after the tables is skipped over. */
    NEED5(); size_t wn, un; uint8_t *w = make_woff(&wn), *w2; IDWriteFontFileStream *s = NULL; HRESULT hr;
    size_t total = wn + 16;
    w2 = (uint8_t *)calloc(total, 1); memcpy(w2, w, wn); memset(w2 + wn, 0x41, 16);
    sf_w32(w2 + 8, (uint32_t)total); sf_w32(w2 + 0x18, (uint32_t)wn); sf_w32(w2 + 0x1c, 16); sf_w32(w2 + 0x20, 16);
    hr = IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w2, (UINT32)total, &s);
    t_note("WOFF with a metadata block: %s", dw_hr_name(hr));
    PRED("accepted", SUCCEEDED(hr));
    if (s) { uint8_t *u = drain(s, &un); PRED("sfnt size unchanged", un == sf_r32(w + 0x10)); free(u); }
    DW_RELEASE(s); free(w); free(w2);
}
TEST(wf_woff_with_private_block) {
    NEED5(); size_t wn; uint8_t *w = make_woff(&wn), *w2; IDWriteFontFileStream *s = NULL; HRESULT hr; size_t total = wn + 8;
    w2 = (uint8_t *)calloc(total, 1); memcpy(w2, w, wn); memset(w2 + wn, 0x42, 8);
    sf_w32(w2 + 8, (uint32_t)total); sf_w32(w2 + 0x24, (uint32_t)wn); sf_w32(w2 + 0x28, 8);
    hr = IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w2, (UINT32)total, &s);
    t_note("WOFF with a private block: %s", dw_hr_name(hr));
    PRED("accepted", SUCCEEDED(hr));
    DW_RELEASE(s); free(w); free(w2);
}
TEST(wf_woff_trailing_garbage_is_rejected) {
    NEED5(); size_t wn; uint8_t *w = make_woff(&wn), *w2; IDWriteFontFileStream *s = POISON;
    w2 = (uint8_t *)calloc(wn + 4, 1); memcpy(w2, w, wn); sf_w32(w2 + 8, (uint32_t)wn + 4);      /* header.length covers 4 unexplained bytes */
    CHECK_HR(IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w2, (UINT32)wn + 4, &s), DWRITE_E_FILEFORMAT_); CHECK_OUT_NULL(s);
    free(w); free(w2);
}
TEST(wf_woff_collection_flavor_tables) {
    /* a WOFF wrapping a font with a single table: the minimal legal container */
    NEED5(); ttf_spec sp; sf_blob a; size_t wn; uint8_t *w; IDWriteFontFileStream *s = NULL; HRESULT hr;
    ttf_default_spec(&sp); a = sf_build(&sp);
    w = ttf_wrap_woff(a.p, a.n, &wn);
    hr = IDWriteFactory5_UnpackFontFile(f5, CT_WOFF, w, (UINT32)wn, &s);
    CHECK_HR(hr, S_OK); DW_RELEASE(s); free(w); sf_free(&a);
}

/* ---- WOFF2 ---------------------------------------------------------------------------------------------------------- */
static void woff2_probe(const char *label, const uint8_t *d, size_t n) {
    IDWriteFactory5 *f = fac5(); IDWriteFontFileStream *s = POISON;
    if (!f) { t_skip("IDWriteFactory5 unsupported"); return; }
    CHECKF(IDWriteFactory5_UnpackFontFile(f, CT_WOFF2, d, (UINT32)n, &s) == DWRITE_E_FILEFORMAT_, "%s", label);
    CHECKF(s == NULL, "%s: stream not NULL", label);
}
TEST(wf2_signature_only) { static const uint8_t d[] = { 'w','O','F','2' }; woff2_probe("wOF2 only", d, 4); }
TEST(wf2_header_too_short_for_total_size) {
    uint8_t d[19]; memset(d, 0, sizeof d); memcpy(d, "wOF2", 4); woff2_probe("19 byte header", d, sizeof d);
}
TEST(wf2_total_size_zero) { uint8_t d[64]; memset(d, 0, sizeof d); memcpy(d, "wOF2", 4); woff2_probe("totalSfntSize 0", d, sizeof d); }
TEST(wf2_total_size_above_512mb) {
    uint8_t d[64]; memset(d, 0, sizeof d); memcpy(d, "wOF2", 4); sf_w32(d + 16, 0x20000001); woff2_probe("totalSfntSize 0x20000001", d, sizeof d);
}
TEST(wf2_total_size_ratio_over_100x) {
    uint8_t d[64]; memset(d, 0, sizeof d); memcpy(d, "wOF2", 4); sf_w32(d + 16, 64 * 101); woff2_probe("expansion 101x", d, sizeof d);
}
TEST(wf2_zero_filled_header) {
    uint8_t d[200]; memset(d, 0, sizeof d); memcpy(d, "wOF2", 4); sf_w32(d + 16, 1000); sf_w32(d + 8, 200); woff2_probe("zero header body", d, sizeof d);
}
TEST(wf2_garbage_body) {
    uint8_t d[200]; size_t i; for (i = 0; i < sizeof d; i++) d[i] = (uint8_t)(i * 37 + 11); memcpy(d, "wOF2", 4); sf_w32(d + 16, 500);
    woff2_probe("garbage body", d, sizeof d);
}
TEST(wf2_flavor_ttcf_without_collection_directory) {
    uint8_t d[200]; memset(d, 0, sizeof d); memcpy(d, "wOF2", 4); memcpy(d + 4, "ttcf", 4); sf_w32(d + 8, 200); sf_w16(d + 12, 1); sf_w32(d + 16, 1000);
    woff2_probe("ttcf flavor, no directory", d, sizeof d);
}
