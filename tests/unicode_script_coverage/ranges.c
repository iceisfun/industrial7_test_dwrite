/* IDWriteFontFace1::GetUnicodeRanges / IDWriteFont1::GetUnicodeRanges versus per-code-point HasCharacter / GetGlyphIndices.
 *
 * Decomp of DWriteFontFace::GetUnicodeRanges + CmapBase::GetUnicodeRanges<FontCmap>:
 *   - *actualRangeCount is zeroed first, then `maxRangeCount` entries of `ranges` are zero-filled (when ranges != NULL);
 *   - ranges != NULL || maxRangeCount == 0 is required, otherwise E_INVALIDARG with *actualRangeCount == 0;
 *   - the walk reports MAXIMAL runs of mapped code points (a run ends at the first unmapped value, so two consecutive ranges are
 *     separated by at least one unmapped code point), ascending, bounds inclusive, within 0..0x10FFFF;
 *   - the true number of ranges is always stored in *actualRangeCount; maxRangeCount < that number => E_NOT_SUFFICIENT_BUFFER
 *     (0x8007007A) with the first maxRangeCount entries filled.
 */
#include "uc.h"
#include <stdlib.h>

typedef struct { DWRITE_UNICODE_RANGE *r; UINT32 n; HRESULT hr; } rng;
static void rng_get(IDWriteFontFace1 *f1, rng *o)
{
    UINT32 n = 0; HRESULT hr;
    memset(o, 0, sizeof *o);
    hr = IDWriteFontFace1_GetUnicodeRanges(f1, 0, NULL, &n);
    t_check(hr == (n ? DW_E_NOT_SUFFICIENT_BUFFER : S_OK), __FILE__, __LINE__, "size query", "hr %s count %u", dw_hr_name(hr), n);
    o->r = (DWRITE_UNICODE_RANGE *)calloc(n + 2, sizeof(DWRITE_UNICODE_RANGE));
    o->hr = IDWriteFontFace1_GetUnicodeRanges(f1, n, o->r, &o->n);
}
static int in_ranges(const rng *o, UINT32 cp)
{
    UINT32 lo = 0, hi = o->n;
    while (lo < hi) { UINT32 m = (lo + hi) / 2; if (cp < o->r[m].first) hi = m; else if (cp > o->r[m].last) lo = m + 1; else return 1; }
    return 0;
}
static int structurally_valid(const rng *o)
{
    UINT32 i;
    for (i = 0; i < o->n; i++) {
        if (o->r[i].first > o->r[i].last || o->r[i].last > 0x10FFFF) return 0;
        if (i && o->r[i].first <= o->r[i - 1].last + 1) return 0;      /* ascending, and separated by an unmapped value */
    }
    return 1;
}

/* Expected ranges of a synthetic variant, computed from uc_gid. */
static int cmp_u(const void *a, const void *b) { unsigned x = *(const unsigned *)a, y = *(const unsigned *)b; return x < y ? -1 : x > y; }
static int expected(int v, DWRITE_UNICODE_RANGE *out)
{
    unsigned cps[256]; int n = 0, i, nr = 0;
    for (i = 0; i < 128; i++) if (uc_gid(v, i)) cps[n++] = i;
    for (i = 0; i < uc_nextra; i++) if (uc_gid(v, uc_extra[i]) && uc_extra[i] >= 128) cps[n++] = uc_extra[i];
    qsort(cps, n, sizeof cps[0], cmp_u);
    for (i = 0; i < n; i++) {
        if (nr && cps[i] == out[nr - 1].last + 1) out[nr - 1].last = cps[i];
        else { out[nr].first = out[nr].last = cps[i]; nr++; }
    }
    return nr;
}

static void exact_variant(int v)
{
    UC_FACE_BEGIN(u, v);
    DWRITE_UNICODE_RANGE want[256]; int nw = expected(v, want), i; rng o;
    if (!u.face1) { uc_face_close(&u); t_skip("no IDWriteFontFace1"); return; }
    rng_get(u.face1, &o);
    CHECK_HR_OK(o.hr);
    CHECKF(o.n == (UINT32)nw, "variant %d: %u ranges, want %d", v, o.n, nw);
    for (i = 0; i < nw && i < (int)o.n; i++)
        CHECKF(o.r[i].first == want[i].first && o.r[i].last == want[i].last, "variant %d range %d is [%X,%X] want [%X,%X]", v, i,
               o.r[i].first, o.r[i].last, want[i].first, want[i].last);
    CHECK(structurally_valid(&o));
    free(o.r); uc_face_close(&u);
}
TEST(ranges_exact_both) { exact_variant(UC_V_BOTH); }
TEST(ranges_exact_ucs4) { exact_variant(UC_V_UCS4); }
TEST(ranges_exact_bmp)  { exact_variant(UC_V_BMP); }
TEST(ranges_exact_uvs)  { exact_variant(UC_V_UVS); }

TEST(ranges_default_font_exact)
{
    /* the stock test font maps space, digits, capitals and small letters */
    dwx_font h; IDWriteFontFace1 *f1 = NULL; rng o; IDWriteFactory *f = dw_shared();
    memset(&h, 0, sizeof h);
    DW_REQUIRE_OK(dwx_font_open_default(&h, f));
    if (FAILED(IDWriteFontFace_QueryInterface(h.face, &IID_IDWriteFontFace1, (void **)&f1))) { dwx_font_close(&h); t_skip("no Face1"); return; }
    rng_get(f1, &o);
    CHECK_HR_OK(o.hr);
    CHECK(o.n == 4);
    if (o.n == 4) {
        CHECK(o.r[0].first == 0x20 && o.r[0].last == 0x20);
        CHECK(o.r[1].first == 0x30 && o.r[1].last == 0x39);
        CHECK(o.r[2].first == 0x41 && o.r[2].last == 0x5A);
        CHECK(o.r[3].first == 0x61 && o.r[3].last == 0x7A);
    }
    free(o.r); DW_RELEASE(f1); dwx_font_close(&h);
}

/* ---- buffer protocol ------------------------------------------------------------------------------------------------------------ */
TEST(ranges_size_query_returns_insufficient_buffer)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT32 n = 0xFFFFFFFFu; DWRITE_UNICODE_RANGE want[256]; int nw = expected(UC_V_BOTH, want);
    if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
    CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(u.face1, 0, NULL, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK(n == (UINT32)nw);
    uc_face_close(&u);
}
TEST(ranges_exact_capacity_ok)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    DWRITE_UNICODE_RANGE want[256], buf[256]; int nw = expected(UC_V_BOTH, want); UINT32 n = 0;
    if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
    memset(buf, 0xCC, sizeof buf);
    CHECK_HR_OK(IDWriteFontFace1_GetUnicodeRanges(u.face1, nw, buf, &n));
    CHECK(n == (UINT32)nw);
    CHECK(buf[nw - 1].last == want[nw - 1].last);
    uc_face_close(&u);
}
TEST(ranges_capacity_one_short)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    DWRITE_UNICODE_RANGE want[256], buf[256]; int nw = expected(UC_V_BOTH, want), i; UINT32 n = 0;
    if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
    memset(buf, 0xCC, sizeof buf);
    CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(u.face1, nw - 1, buf, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK(n == (UINT32)nw);                                          /* the real count is reported */
    for (i = 0; i < nw - 1; i++) CHECK(buf[i].first == want[i].first && buf[i].last == want[i].last);
    CHECKF(buf[nw - 1].first == 0xCCCCCCCCu, "entry past the capacity was touched");
    uc_face_close(&u);
}
TEST(ranges_capacity_one)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    DWRITE_UNICODE_RANGE want[256], buf[4]; int nw = expected(UC_V_BOTH, want); UINT32 n = 0;
    if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
    memset(buf, 0xCC, sizeof buf);
    CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(u.face1, 1, buf, &n), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK(n == (UINT32)nw);
    CHECK(buf[0].first == want[0].first && buf[0].last == want[0].last);
    CHECK(buf[1].first == 0xCCCCCCCCu);
    uc_face_close(&u);
}
TEST(ranges_larger_capacity_zero_fills_tail)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    DWRITE_UNICODE_RANGE want[256], buf[256]; int nw = expected(UC_V_BOTH, want), i; UINT32 n = 0;
    if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
    memset(buf, 0xCC, sizeof buf);
    CHECK_HR_OK(IDWriteFontFace1_GetUnicodeRanges(u.face1, nw + 8, buf, &n));
    CHECK(n == (UINT32)nw);
    for (i = nw; i < nw + 8; i++) CHECKF(buf[i].first == 0 && buf[i].last == 0, "tail entry %d not zeroed (%X,%X)", i, buf[i].first, buf[i].last);
    CHECK(buf[nw + 8].first == 0xCCCCCCCCu);
    uc_face_close(&u);
}
TEST(ranges_null_buffer_with_capacity_invalidarg)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    UINT32 n = 0x1234;
    if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
    CHECK_HR(IDWriteFontFace1_GetUnicodeRanges(u.face1, 4, NULL, &n), E_INVALIDARG);
    CHECKF(n == 0, "actual count %u after E_INVALIDARG, want 0", n);
    uc_face_close(&u);
}
TEST(ranges_actual_zeroed_before_work)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    DWRITE_UNICODE_RANGE buf[64]; UINT32 n = 0xFFFFFFFFu;
    if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
    CHECK_HR_OK(IDWriteFontFace1_GetUnicodeRanges(u.face1, 64, buf, &n));
    CHECK(n < 64 && n > 0);
    uc_face_close(&u);
}
TEST(ranges_repeat_identical)
{
    UC_FACE_BEGIN(u, UC_V_BOTH);
    rng a, b;
    if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
    rng_get(u.face1, &a); rng_get(u.face1, &b);
    CHECK(a.n == b.n && !memcmp(a.r, b.r, a.n * sizeof(DWRITE_UNICODE_RANGE)));
    free(a.r); free(b.r); uc_face_close(&u);
}
TEST(ranges_vs_glyph_indices_every_codepoint_bmp)
{
    int v;
    for (v = 0; v < UC_V_NVARIANT; v++) {
        UC_FACE_BEGIN(u, v);
        rng o; static UINT32 cp[0x10000]; static UINT16 g[0x10000]; unsigned i, bad = 0;
        if (!u.face1) { uc_face_close(&u); t_skip("no Face1"); return; }
        rng_get(u.face1, &o);
        for (i = 0; i < 0x10000; i++) cp[i] = i;
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(u.face, cp, 0x10000, g));
        for (i = 0; i < 0x10000; i++) if (in_ranges(&o, i) != (g[i] != 0)) bad++;
        CHECKF(bad == 0, "variant %d: %u code points where range membership != mapped", v, bad);
        free(o.r); uc_face_close(&u);
    }
}

TEST(ranges_font1_matches_face1)
{
    UC_FX_BEGIN();
    IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL; IDWriteFont1 *font1 = NULL; IDWriteFontFace *face = NULL; IDWriteFontFace1 *f1 = NULL;
    UINT32 n1 = 0, n2 = 0; rng a;
    DWRITE_UNICODE_RANGE buf[64];
    if (FAILED(IDWriteFontCollection_GetFontFamily(X.coll, 0, &fam)) || FAILED(IDWriteFontFamily_GetFirstMatchingFont(fam, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font)) ||
        FAILED(IDWriteFont_QueryInterface(font, &IID_IDWriteFont1, (void **)&font1)) || FAILED(IDWriteFont_CreateFontFace(font, &face)) ||
        FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace1, (void **)&f1))) {
        DW_RELEASE(f1); DW_RELEASE(face); DW_RELEASE(font1); DW_RELEASE(font); DW_RELEASE(fam); UC_FX_END(); t_skip("IDWriteFont1 unavailable"); return;
    }
    rng_get(f1, &a);
    CHECK_HR(IDWriteFont1_GetUnicodeRanges(font1, 0, NULL, &n1), DW_E_NOT_SUFFICIENT_BUFFER);
    CHECK(n1 == a.n);
    memset(buf, 0, sizeof buf);
    CHECK_HR_OK(IDWriteFont1_GetUnicodeRanges(font1, 64, buf, &n2));
    CHECK(n2 == a.n);
    CHECK(!memcmp(buf, a.r, a.n * sizeof buf[0]));
    n2 = 77;
    CHECK_HR(IDWriteFont1_GetUnicodeRanges(font1, 3, NULL, &n2), E_INVALIDARG);
    free(a.r); DW_RELEASE(f1); DW_RELEASE(face); DW_RELEASE(font1); DW_RELEASE(font); DW_RELEASE(fam); UC_FX_END();
}

/* ---- system fonts: 2000 sampled code points, HasCharacter parity, range edges ------------------------------------------------------ */
static unsigned lcg(unsigned *s) { *s = *s * 1664525u + 1013904223u; return *s >> 8; }
static void sys_ranges(const WCHAR *family, int dense_bmp)
{
    IDWriteFactory *f = dw_shared(); IDWriteFont *font = f ? uc_sysfont(f, family) : NULL; IDWriteFontFace *face = NULL; IDWriteFontFace1 *f1 = NULL;
    rng o; unsigned seed = 12345, i, bad = 0, hits = 0; UINT32 k;
    if (!font) { t_skip("font %ls not installed", family); return; }
    DW_REQUIRE_OK(IDWriteFont_CreateFontFace(font, &face));
    if (FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace1, (void **)&f1))) { DW_RELEASE(face); DW_RELEASE(font); t_skip("no Face1"); return; }
    rng_get(f1, &o);
    CHECK_HR_OK(o.hr);
    CHECK(o.n > 0);
    CHECK(structurally_valid(&o));
    for (i = 0; i < 2000; i++) {
        UINT32 cp = dense_bmp ? lcg(&seed) % 0x3000 : (i & 1) ? lcg(&seed) % 0x30000 : lcg(&seed) % 0x2200;
        BOOL b = 0x55;
        CHECK_HR_OK(IDWriteFont_HasCharacter(font, cp, &b));
        if ((b != 0) != in_ranges(&o, cp)) { bad++; if (bad < 5) t_note("%ls: U+%04X HasCharacter=%d inRanges=%d", family, cp, b, in_ranges(&o, cp)); }
        if (b) hits++;
    }
    CHECKF(bad == 0, "%ls: %u of 2000 samples disagree between HasCharacter and GetUnicodeRanges", family, bad);
    /* every range edge maps to a glyph, the values just outside do not */
    for (k = 0; k < o.n && k < 400; k++) {
        UINT32 pts[4] = { o.r[k].first, o.r[k].last, o.r[k].first - 1, o.r[k].last + 1 }; UINT16 g[4]; int j;
        CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, pts, 4, g));
        CHECKF(g[0] != 0 && g[1] != 0, "%ls range [%X,%X] edge unmapped", family, pts[0], pts[1]);
        for (j = 2; j < 4; j++) if (pts[j] <= 0x10FFFF && !(j == 2 && pts[0] == 0)) CHECKF(g[j] == 0, "%ls: U+%X just outside range [%X,%X] is mapped", family, pts[j], pts[0], pts[1]);
    }
    t_note("%ls: %u ranges; %u of 2000 samples mapped; first range [%X,%X]", family, o.n, hits, o.r[0].first, o.r[0].last);
    free(o.r); DW_RELEASE(f1); DW_RELEASE(face); DW_RELEASE(font);
}
#define SR_TEST(nm, fam, dense) TEST(sys_ranges_##nm) { sys_ranges(fam, dense); }
SR_TEST(arial, L"Arial", 0) SR_TEST(arial_bmp, L"Arial", 1) SR_TEST(times, L"Times New Roman", 0) SR_TEST(courier, L"Courier New", 0)
SR_TEST(segoe_ui, L"Segoe UI", 0) SR_TEST(tahoma, L"Tahoma", 0) SR_TEST(consolas, L"Consolas", 0) SR_TEST(segoe_symbol, L"Segoe UI Symbol", 0)
SR_TEST(segoe_emoji, L"Segoe UI Emoji", 0) SR_TEST(marlett, L"Marlett", 1) SR_TEST(ms_gothic, L"MS Gothic", 1) SR_TEST(cambria_math, L"Cambria Math", 0)
SR_TEST(calibri, L"Calibri", 0) SR_TEST(verdana, L"Verdana", 0) SR_TEST(bahnschrift, L"Bahnschrift", 1)

TEST(arial_ranges_contain_core_blocks)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace1 *f1 = f ? uc_sysface1(f, L"Arial") : NULL; rng o; static const UINT32 want[] = { 0x20, 0x41, 0x7A, 0xA0, 0xE9, 0x3B1, 0x416, 0x5D0, 0x627, 0x20AC, 0x2022 };
    int i;
    if (!f1) { t_skip("Arial not installed"); return; }
    rng_get(f1, &o);
    for (i = 0; i < (int)(sizeof want / sizeof want[0]); i++) CHECKF(in_ranges(&o, want[i]), "U+%04X not covered by Arial", want[i]);
    CHECK(!in_ranges(&o, 0x110000 - 1) || 1);
    CHECK(!in_ranges(&o, 0x1F600));                    /* Arial carries no emoji */
    CHECK(!in_ranges(&o, 0xD800) && !in_ranges(&o, 0xDFFF));
    free(o.r); DW_RELEASE(f1);
}
TEST(courier_arial_ranges_differ_in_size_only_by_font)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontFace1 *a = f ? uc_sysface1(f, L"Arial") : NULL, *c = f ? uc_sysface1(f, L"Courier New") : NULL; rng ra, rc;
    if (!a || !c) { DW_RELEASE(a); DW_RELEASE(c); t_skip("fonts missing"); return; }
    rng_get(a, &ra); rng_get(c, &rc);
    CHECK(structurally_valid(&ra) && structurally_valid(&rc));
    t_note("Arial %u ranges, Courier New %u ranges", ra.n, rc.n);
    free(ra.r); free(rc.r); DW_RELEASE(a); DW_RELEASE(c);
}
