/* IDWriteFontFileLoader / IDWriteFontFileStream traffic caused by building a custom collection.
 *
 * Win10 decomp facts:
 *  - Every enumerated file is analysed inside the enumeration loop: FontFileAnalyzer::Analyze calls
 *    FontFileReference::GetFileSize first (GetStreamInternal -> CreateStreamFromKey(key, 4), then stream->GetFileSize;
 *    both through CallbackException::ThrowIfFailed), then probes the header with FontFragmentPtr reads starting at offset 0
 *    (IsPfm reads 0x75 bytes, IsPfb 7 bytes, IsOpenType 16 bytes).  Each FontFragmentPtr releases its fragment with
 *    ReleaseFileFragment(context) in its destructor.
 *  - An analyzed type that is not a supported font makes AddFontFileInternal return DWRITE_E_FILEFORMAT, which AddFontFiles
 *    ignores: the file is silently left out.
 */
#include "r2.h"

static void mk_fonts(r2_fx *x, int n)
{
    int i;
    for (i = 0; i < n; i++) { char nm[32]; snprintf(nm, sizeof nm, "St%02d", i); r2_add_family(x, nm); }
}
static void each_stream(r2_fl *l, void (*fn)(r2_st *, void *), void *arg)
{
    int i, n = l->nstreams > 512 ? 512 : (int)l->nstreams;
    for (i = 0; i < n; i++) fn(l->streams[i], arg);
}

/* ------------------------------------------------------------------------- the order inside one stream */
RT(stream_first_call_after_creation_is_getfilesize)
{
    FX(x);
    mk_fonts(&x, 1);
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int f = r2_find(EV_FL_CREATE, 0), s = r2_find(EV_ST_SIZE, 0), r = r2_find(EV_ST_READ, 0);
    CHECK(f >= 0 && s >= 0 && r >= 0);
    CHECKF(f < s, "CreateStreamFromKey (%d) before GetFileSize (%d)", f, s);
    CHECKF(s < r, "GetFileSize (%d) before the first ReadFileFragment (%d)", s, r);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(every_stream_is_asked_for_its_size_before_it_is_read)
{
    FX(x);
    mk_fonts(&x, 5);
    IDWriteFontCollection *c = NULL;
    r2_log_reset();
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = x.fl->nstreams > 512 ? 512 : (int)x.fl->nstreams;
    CHECK(n >= 5);
    for (i = 0; i < n; i++) {
        int id = x.fl->streams[i]->id;
        int s = r2_find_obj(EV_ST_SIZE, id, 0), r = r2_find_obj(EV_ST_READ, id, 0);
        CHECKF(s >= 0, "stream %d never asked for its size", id);
        CHECKF(r < 0 || s < r, "stream %d read (%d) before its size was requested (%d)", id, r, s);
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(first_read_of_every_stream_starts_at_offset_zero)
{
    FX(x);
    mk_fonts(&x, 3);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = x.fl->nstreams > 512 ? 512 : (int)x.fl->nstreams;
    for (i = 0; i < n; i++) {
        r2_st *s = x.fl->streams[i];
        CHECK(s->nlogged > 0);
        if (s->nlogged) EXPECT_EQ(s->reads[0].off, 0);
        t_note("stream %d first reads: (%llu,%llu) (%llu,%llu) (%llu,%llu)", s->id, (unsigned long long)s->reads[0].off, (unsigned long long)s->reads[0].len,
               (unsigned long long)s->reads[1].off, (unsigned long long)s->reads[1].len, (unsigned long long)s->reads[2].off, (unsigned long long)s->reads[2].len);
    }
    DW_RELEASE(c);
    r2_close(&x);
}
RT(header_probe_reads_are_requested_from_the_start_of_the_file)
{
    FX(x);
    mk_fonts(&x, 1);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    r2_st *s = x.fl->streams[0];
    /* the three header probes (PFM 0x75 bytes, PFB 7 bytes, OpenType 16 bytes) all start at 0; the sizes are what the analyzer asks for */
    int i, zero_len_seen[3] = { 0, 0, 0 };
    for (i = 0; i < s->nlogged && i < 8; i++) {
        if (s->reads[i].off != 0) continue;
        if (s->reads[i].len == 0x75) zero_len_seen[0]++;
        if (s->reads[i].len == 7) zero_len_seen[1]++;
        if (s->reads[i].len == 16) zero_len_seen[2]++;
    }
    CHECKF(zero_len_seen[0] && zero_len_seen[1] && zero_len_seen[2], "probe reads seen: pfm=%d pfb=%d otf=%d", zero_len_seen[0], zero_len_seen[1], zero_len_seen[2]);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(getlastwritetime_traffic_note)
{
    FX(x);
    mk_fonts(&x, 3);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = x.fl->nstreams > 512 ? 512 : (int)x.fl->nstreams; LONG tot = 0, sizes = 0, reads = 0;
    for (i = 0; i < n; i++) { tot += x.fl->streams[i]->n_last; sizes += x.fl->streams[i]->n_size; reads += x.fl->streams[i]->n_read; }
    t_note("3 files: %d streams, GetFileSize x%ld, ReadFileFragment x%ld, GetLastWriteTime x%ld", n, (long)sizes, (long)reads, (long)tot);
    CHECK(sizes >= n);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(one_stream_per_file_or_more_note)
{
    FX(x);
    mk_fonts(&x, 4);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    CHECK(x.fl->n_create >= 4);
    t_note("4 files -> %ld CreateStreamFromKey calls", (long)x.fl->n_create);
    DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- fragments and contexts */
RT(release_gets_back_the_context_that_read_returned)
{
    FX(x);
    mk_fonts(&x, 4);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(r2_fl_bad_ctx(x.fl), 0);
    CHECK(r2_fl_reads(x.fl) > 0);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(every_release_belongs_to_a_read)
{
    FX(x);
    mk_fonts(&x, 4);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    int i, n = x.fl->nstreams > 512 ? 512 : (int)x.fl->nstreams;
    for (i = 0; i < n; i++) CHECKF(x.fl->streams[i]->n_rel <= x.fl->streams[i]->n_read, "stream %d: %ld releases for %ld reads", x.fl->streams[i]->id, (long)x.fl->streams[i]->n_rel, (long)x.fl->streams[i]->n_read);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(release_never_comes_for_a_failed_read)
{
    int from;
    for (from = 0; from < 4; from++) {
        FX(x);
        mk_fonts(&x, 1);
        x.fl->cfg[0].fail_read = E_FAIL; x.fl->cfg[0].fail_read_from = from;
        IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
        HRESULT hr = r2_create(&x, &c);
        t_note("reads fail from #%d on -> %s", from, dw_hr_name(hr));
        if (FAILED(hr)) CHECK_OUT_NULL(c);
        EXPECT_EQ(r2_fl_bad_ctx(x.fl), 0);              /* a fragment that was never handed out is never released */
        DW_RELEASE(c);
        r2_close(&x);
        EXPECT_EQ(r2_fl_outstanding(x.fl), 0);
    }
}
RT(fragments_are_balanced_after_creation_note)
{
    FX(x);
    mk_fonts(&x, 3);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    t_note("after creation: %ld fragments outstanding, %d streams alive", (long)r2_fl_outstanding(x.fl), r2_fl_streams_alive(x.fl));
    CHECK(r2_fl_outstanding(x.fl) >= 0);
    DW_RELEASE(c);
    r2_close(&x);
    EXPECT_EQ(r2_fl_outstanding(x.fl), 0);              /* nothing keeps a fragment once factory and collection are gone */
}
RT(copied_fragments_give_the_same_collection)
{
    FX(x);
    mk_fonts(&x, 3);
    int i; for (i = 0; i < 3; i++) x.fl->cfg[i].copy = 1;
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(r2_fl_bad_ctx(x.fl), 0);
    if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 3); for (i = 0; i < 3; i++) { char nm[32]; snprintf(nm, sizeof nm, "St%02d", i); CHECK(r2_find_family(c, nm) >= 0); } }
    DW_RELEASE(c);
    r2_close(&x);
    EXPECT_EQ(r2_fl_outstanding(x.fl), 0);
}
RT(valid_fonts_are_never_read_beyond_their_size)
{
    FX(x);
    mk_fonts(&x, 3);
    size_t n1, n2, n3; uint8_t *a = ttf_build_styled("Wide", 700, 5, 0, &n1), *b = ttf_build_styled("Wide", 700, 5, 1, &n2);
    r2_add_blob(&x, a, n1); r2_add_blob(&x, b, n2);
    const uint8_t *cf[2] = { a, b }; size_t cs[2] = { n1, n2 };
    uint8_t *ttc = ttf_build_ttc(cf, cs, 2, &n3);
    r2_add_blob(&x, ttc, n3);
    free(a); free(b); free(ttc);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    EXPECT_EQ(r2_fl_reads_beyond(x.fl), 0);
    int i, n = x.fl->nstreams > 512 ? 512 : (int)x.fl->nstreams;
    for (i = 0; i < n; i++) CHECKF(x.fl->streams[i]->max_end <= x.fl->streams[i]->size_real, "stream %d requested up to %llu of %llu", x.fl->streams[i]->id,
                                   (unsigned long long)x.fl->streams[i]->max_end, (unsigned long long)x.fl->streams[i]->size_real);
    DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- stream faults */
static void fault_consistency(HRESULT hr, IDWriteFontCollection *c, UINT32 survivors, const char *what)
{
    if (FAILED(hr)) { CHECKF(c == NULL, "%s: failure %s but out parameter %p", what, dw_hr_name(hr), (void *)c); t_note("%s -> %s (creation fails)", what, dw_hr_name(hr)); }
    else {
        CHECKF(hr == S_OK, "%s: %s", what, dw_hr_name(hr));
        if (c) { UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c); t_note("%s -> S_OK, %u families (file skipped)", what, n); CHECKF(n == survivors, "%s: %u families, want %u", what, n, survivors); }
    }
}
static void getsize_case(HRESULT fail)
{
    FX(x);
    r2_add_family(&x, "Sick");
    x.fl->cfg[0].fail_size = fail;
    r2_add_family(&x, "Healthy");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = r2_create(&x, &c);
    fault_consistency(hr, c, 1, "GetFileSize fails");
    if (SUCCEEDED(hr) && c) CHECK(r2_find_family(c, "Healthy") >= 0);
    EXPECT_EQ(r2_fl_bad_ctx(x.fl), 0);
    DW_RELEASE(c);
    r2_close(&x);
}
#define SIZEFAIL(tag, hr) RT(getfilesize_failure_##tag) { getsize_case((HRESULT)(hr)); }
SIZEFAIL(e_fail, E_FAIL) SIZEFAIL(e_notimpl, E_NOTIMPL) SIZEFAIL(e_outofmemory, E_OUTOFMEMORY) SIZEFAIL(e_accessdenied, E_ACCESSDENIED) SIZEFAIL(fileformat, 0x88985000)
static void createstream_case(HRESULT fail)
{
    FX(x);
    r2_add_family(&x, "NoStream");
    x.fl->cfg[0].fail_create = fail;
    r2_add_family(&x, "Healthy");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = r2_create(&x, &c);
    fault_consistency(hr, c, 1, "CreateStreamFromKey fails");
    CHECK(x.fl->n_create >= 1);
    DW_RELEASE(c);
    r2_close(&x);
}
#define CREATEFAIL(tag, hr) RT(createstreamfromkey_failure_##tag) { createstream_case((HRESULT)(hr)); }
CREATEFAIL(e_fail, E_FAIL) CREATEFAIL(e_invalidarg, E_INVALIDARG) CREATEFAIL(e_outofmemory, E_OUTOFMEMORY) CREATEFAIL(e_accessdenied, E_ACCESSDENIED)
CREATEFAIL(filenotfound, 0x88985003) CREATEFAIL(win32_file_not_found, 0x80070002) CREATEFAIL(e_notimpl, E_NOTIMPL)
static void read_fail_case(int from)
{
    FX(x);
    r2_add_family(&x, "ReadSick");
    x.fl->cfg[0].fail_read = E_FAIL; x.fl->cfg[0].fail_read_from = from;
    r2_add_family(&x, "Healthy");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = r2_create(&x, &c);
    char w[64]; snprintf(w, sizeof w, "ReadFileFragment fails from read #%d", from);
    fault_consistency(hr, c, 1, w);
    DW_RELEASE(c);
    r2_close(&x);
    EXPECT_EQ(r2_fl_outstanding(x.fl), 0);
}
#define READFAIL(n) RT(readfilefragment_failure_from_read_##n) { read_fail_case(n); }
READFAIL(0) READFAIL(1) READFAIL(2) READFAIL(3) READFAIL(4) READFAIL(6) READFAIL(10)
RT(getlastwritetime_failure_is_harmless_when_it_is_never_called)
{
    FX(x);
    mk_fonts(&x, 2);
    IDWriteFontCollection *c0 = NULL;
    CHECK_HR(r2_create(&x, &c0), S_OK);
    LONG calls = 0; int i, n = x.fl->nstreams > 512 ? 512 : (int)x.fl->nstreams;
    for (i = 0; i < n; i++) calls += x.fl->streams[i]->n_last;
    DW_RELEASE(c0);
    r2_close(&x);
    FX(y);
    mk_fonts(&y, 2);
    y.fl->cfg[0].fail_last = E_FAIL; y.fl->cfg[1].fail_last = E_NOTIMPL;
    IDWriteFontCollection *c = NULL;
    HRESULT hr = r2_create(&y, &c);
    t_note("GetLastWriteTime is called %ld times while building a collection; failing it gives %s", (long)calls, dw_hr_name(hr));
    if (calls == 0) { CHECK_HR(hr, S_OK); if (c) EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2); }
    DW_RELEASE(c);
    r2_close(&y);
}

/* ------------------------------------------------------------------------- lying about the size */
RT(size_larger_than_the_data)
{
    FX(x);
    r2_add_family(&x, "Longer");
    x.fl->cfg[0].has_size_override = 1; x.fl->cfg[0].size_override = x.fl->blob[0].size + 4096;
    r2_add_family(&x, "Healthy");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = r2_create(&x, &c);
    fault_consistency(hr, c, SUCCEEDED(hr) && c ? IDWriteFontCollection_GetFontFamilyCount(c) : 0, "size reported 4096 bytes too large");
    t_note("reads beyond the real data: %ld", (long)r2_fl_reads_beyond(x.fl));
    DW_RELEASE(c);
    r2_close(&x);
    EXPECT_EQ(r2_fl_outstanding(x.fl), 0);
}
RT(size_smaller_than_the_data)
{
    FX(x);
    r2_add_family(&x, "Shorter");
    x.fl->cfg[0].has_size_override = 1; x.fl->cfg[0].size_override = 160;
    r2_add_family(&x, "Healthy");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = r2_create(&x, &c);
    fault_consistency(hr, c, SUCCEEDED(hr) && c ? IDWriteFontCollection_GetFontFamilyCount(c) : 0, "size reported 160 bytes");
    t_note("stream[0] max requested end %llu of reported 160", (unsigned long long)x.fl->streams[0]->max_end);
    EXPECT_EQ(r2_fl_bad_ctx(x.fl), 0);
    DW_RELEASE(c);
    r2_close(&x);
}
RT(size_zero_stream)
{
    FX(x);
    r2_add_family(&x, "ZeroSize");
    x.fl->cfg[0].has_size_override = 1; x.fl->cfg[0].size_override = 0;
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = r2_create(&x, &c);
    /* a zero reported size is tolerated by the reference implementation (the family is still listed): accept either outcome */
    if (SUCCEEDED(hr) && c) { UINT32 n = IDWriteFontCollection_GetFontFamilyCount(c); t_note("size reported 0 -> S_OK, %u families", n); CHECKF(n <= 1, "size reported 0: %u families", n); }
    else fault_consistency(hr, c, 0, "size reported 0");
    DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- non-font content */
static void junk_case(const uint8_t *blob, size_t n, int hard)
{
    FX(x);
    r2_add_family(&x, "Before");
    UINT32 jk = r2_add_blob(&x, blob, n);
    r2_add_family(&x, "After");
    IDWriteFontCollection *c = (IDWriteFontCollection *)(ULONG_PTR)1;
    HRESULT hr = r2_create(&x, &c);
    if (hard) {
        CHECK_HR(hr, S_OK);                               /* not a supported font type: the entry is dropped, not an error */
        if (c) { EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 2); CHECK(r2_find_family(c, "Before") >= 0); CHECK(r2_find_family(c, "After") >= 0); }
        EXPECT_EQ(x.cl->last_en->n_get, 3);
        int i, n_ev = r2_n(), opened = 0;
        for (i = 0; i < n_ev; i++) if (r2_log[i].kind == EV_FL_CREATE && r2_log[i].b == jk) opened = 1;
        CHECKF(opened, "the non-font file was never opened");
    } else {
        t_note("%lu-byte non-font file -> %s", (unsigned long)n, dw_hr_name(hr));
        if (FAILED(hr)) CHECK_OUT_NULL(c);
        else if (c) { UINT32 cnt = IDWriteFontCollection_GetFontFamilyCount(c); CHECK(cnt == 2); }
    }
    DW_RELEASE(c);
    r2_close(&x);
    EXPECT_EQ(r2_fl_outstanding(x.fl), 0);
}
#define JUNKZ(n, hard) RT(non_font_zero_bytes_##n) { uint8_t *b = (uint8_t *)calloc(1, n ? n : 1); junk_case(b, n, hard); free(b); }
JUNKZ(117, 1) JUNKZ(118, 1) JUNKZ(128, 1) JUNKZ(200, 1) JUNKZ(1024, 1) JUNKZ(65536, 1)
JUNKZ(0, 0) JUNKZ(1, 0) JUNKZ(4, 0) JUNKZ(7, 0) JUNKZ(15, 0) JUNKZ(16, 0) JUNKZ(64, 0) JUNKZ(116, 0)
RT(non_font_text_file)
{
    uint8_t b[300]; size_t i; const char *t = "This is a plain text file and not a font. ";
    for (i = 0; i < sizeof b; i++) b[i] = (uint8_t)t[i % strlen(t)];
    junk_case(b, sizeof b, 1);
}
RT(non_font_mz_header_note)
{
    /* FontFileAnalyzer::IsFon parses the NE/PE resource directory of an 'MZ' file and may reject it with an exception */
    uint8_t b[300]; memset(b, 0, sizeof b); b[0] = 'M'; b[1] = 'Z';
    junk_case(b, sizeof b, 0);
}
RT(non_font_png_header)
{
    uint8_t b[300]; memset(b, 0x11, sizeof b); memcpy(b, "\x89PNG\r\n\x1a\n", 8);
    junk_case(b, sizeof b, 1);
}
RT(non_font_sfnt_lookalike_with_bad_version)
{
    size_t n; uint8_t *f = ttf_build_styled("Lookalike", 400, 5, 0, &n);
    uint8_t *b = ttf_corrupt(f, n, TTF_CORRUPT_BAD_MAGIC, 0, &n);
    junk_case(b, n, 1);
    free(f); free(b);
}

/* ------------------------------------------------------------------------- the same data through the collection */
RT(table_data_read_through_the_collection_matches_the_font)
{
    FX(x);
    size_t n; uint8_t *blob = ttf_build_styled("TblData", 400, 5, 0, &n);
    r2_add_blob(&x, blob, n);
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (!c || FAILED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) || FAILED(IDWriteFontFamily_GetFont(fam, 0, &font)) || FAILED(IDWriteFont_CreateFontFace(font, &face))) {
        t_skip("no face from collection"); DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c); free(blob); r2_close(&x); return;
    }
    static const char *const tags[] = { "head", "hhea", "maxp", "OS/2", "hmtx", "cmap", "loca", "glyf", "name", "post" };
    unsigned i;
    for (i = 0; i < sizeof tags / sizeof tags[0]; i++) {
        const void *data = NULL; UINT32 len = 0; void *ctx = NULL; BOOL ex = FALSE; unsigned tl = 0;
        const uint8_t *want = ttf_find_table(blob, n, tags[i], &tl);
        UINT32 tag = DW_TAG(tags[i][0], tags[i][1], tags[i][2], tags[i][3]);
        HRESULT hr = IDWriteFontFace_TryGetFontTable(face, tag, &data, &len, &ctx, &ex);
        CHECKF(SUCCEEDED(hr), "TryGetFontTable %s: %s", tags[i], dw_hr_name(hr));
        CHECKF(ex && want, "table %s exists=%d", tags[i], ex);
        if (ex && want) { CHECKF(len == tl, "table %s length %u want %u", tags[i], len, tl); CHECK(len == tl && !memcmp(data, want, tl)); }
        if (ex) IDWriteFontFace_ReleaseFontTable(face, ctx);
    }
    EXPECT_EQ(r2_fl_bad_ctx(x.fl), 0);
    DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c);
    free(blob);
    r2_close(&x);
}
RT(font_table_fragment_is_held_until_releasefonttable_note)
{
    FX(x);
    r2_add_family(&x, "Held");
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (!c || FAILED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) || FAILED(IDWriteFontFamily_GetFont(fam, 0, &font)) || FAILED(IDWriteFont_CreateFontFace(font, &face))) {
        t_skip("no face from collection"); DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c); r2_close(&x); return;
    }
    LONG base = r2_fl_outstanding(x.fl);
    const void *data = NULL; UINT32 len = 0; void *ctx = NULL; BOOL ex = FALSE;
    CHECK_HR_OK(IDWriteFontFace_TryGetFontTable(face, DW_TAG('g', 'l', 'y', 'f'), &data, &len, &ctx, &ex));
    LONG held = r2_fl_outstanding(x.fl);
    if (ex) IDWriteFontFace_ReleaseFontTable(face, ctx);
    LONG after = r2_fl_outstanding(x.fl);
    t_note("outstanding fragments: before %ld, table held %ld, after ReleaseFontTable %ld", (long)base, (long)held, (long)after);
    CHECK(held >= base);
    CHECK(after <= held);
    EXPECT_EQ(r2_fl_bad_ctx(x.fl), 0);
    DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c);
    r2_close(&x);
    EXPECT_EQ(r2_fl_outstanding(x.fl), 0);
}
RT(face_files_report_our_loader_and_key)
{
    FX(x);
    UINT32 key = r2_add_family(&x, "FaceFile");
    IDWriteFontCollection *c = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    if (!c || FAILED(IDWriteFontCollection_GetFontFamily(c, 0, &fam)) || FAILED(IDWriteFontFamily_GetFont(fam, 0, &font)) || FAILED(IDWriteFont_CreateFontFace(font, &face))) {
        t_skip("no face from collection"); DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c); r2_close(&x); return;
    }
    UINT32 nfiles = 0; IDWriteFontFile *file = NULL;
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &nfiles, NULL));
    EXPECT_EQ(nfiles, 1);
    nfiles = 1;
    CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &nfiles, &file));
    if (file) {
        IDWriteFontFileLoader *ld = NULL; const void *rk = NULL; UINT32 rks = 0;
        CHECK_HR_OK(IDWriteFontFile_GetLoader(file, &ld));
        CHECK(ld == (IDWriteFontFileLoader *)x.fl);
        CHECK_HR_OK(IDWriteFontFile_GetReferenceKey(file, &rk, &rks));
        EXPECT_EQ(rks, 4);
        CHECK(rk && rks == 4 && *(const UINT32 *)rk == key);
        DW_RELEASE(ld);
        DW_RELEASE(file);
    }
    DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(c);
    r2_close(&x);
}

/* ------------------------------------------------------------------------- stream lifetime */
RT(streams_are_gone_after_collection_and_factory_are_released)
{
    FX(x);
    mk_fonts(&x, 4);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    t_note("streams alive right after creation: %d of %ld", r2_fl_streams_alive(x.fl), (long)x.fl->nstreams);
    DW_RELEASE(c);
    t_note("streams alive after the collection was released: %d", r2_fl_streams_alive(x.fl));
    r2_close(&x);
    EXPECT_EQ(r2_fl_streams_alive(x.fl), 0);
    EXPECT_EQ(x.fl->destroyed, 1);
    EXPECT_EQ(r2_overrel, 0);
    EXPECT_EQ(r2_uaf, 0);
}
RT(file_loader_is_not_retained_beyond_unregistration_and_factory)
{
    FX(x);
    mk_fonts(&x, 2);
    IDWriteFontCollection *c = NULL;
    CHECK_HR(r2_create(&x, &c), S_OK);
    DW_RELEASE(c);
    LONG with_f = x.fl->ref;
    r2_fl_unregister(x.fl);
    LONG without = x.fl->ref;
    t_note("file loader refs: registered+used %ld, after unregister %ld", (long)with_f, (long)without);
    CHECK(without <= with_f);
    r2_close(&x);
}
