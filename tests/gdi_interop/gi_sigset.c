/* IDWriteGdiInterop1::GetFontSignature (font and face overloads) and GetMatchingFontsByLOGFONT.
 *
 * GetFontSignatureInternal zeroes the 24-byte FONTSIGNATURE first, returns E_INVALIDARG for a NULL object and otherwise copies the
 * face's cached OS/2 ulUnicodeRange1..4 / ulCodePageRange1..2.  Slot order: GetFontSignature_ (IDWriteFontFace*) precedes
 * GetFontSignature (IDWriteFont*) in the vtable.
 * GetMatchingFontsByLOGFONT returns a font set holding every font of the Win32 family (full-name fallback), ordered by ascending
 * Win32ComputePenalty with the match's simulations applied; an unknown family gives an empty set and S_OK. */
#include "gi.h"

static void gi_sig_fill(FONTSIGNATURE *s) { memset(s, 0xCC, sizeof *s); }
static int gi_sig_zero(const FONTSIGNATURE *s) { const BYTE *b = (const BYTE *)s; size_t i; for (i = 0; i < sizeof *s; i++) if (b[i]) return 0; return 1; }
static int gi_sig_eq(const FONTSIGNATURE *a, const FONTSIGNATURE *b) { return !memcmp(a, b, sizeof *a); }
static unsigned gi_be32(const uint8_t *p) { return ((unsigned)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

TEST(gis_sizeof_fontsignature)
{
    EXPECT_EQ(sizeof(FONTSIGNATURE), 24);
    EXPECT_EQ(offsetof(FONTSIGNATURE, fsCsb), 16);
}
TEST(gis_font_null)
{
    gi_fix x; FONTSIGNATURE s; static const gi_cf set[] = { { "GiSig", 400, 0, 5 } }; GI_FIX_OR_SKIP(x, set);
    if (!x.gdi1) { t_skip("no interop1"); gi_fix_close(&x); return; }
    gi_sig_fill(&s);
    CHECK_HR(IDWriteGdiInterop1_GetFontSignature(x.gdi1, (IDWriteFont *)NULL, &s), DW_E_INVALIDARG);
    CHECK(gi_sig_zero(&s));   /* zeroed before the argument check */
    gi_fix_close(&x);
}
TEST(gis_face_null)
{
    gi_fix x; FONTSIGNATURE s; static const gi_cf set[] = { { "GiSig", 400, 0, 5 } }; GI_FIX_OR_SKIP(x, set);
    if (!x.gdi1) { t_skip("no interop1"); gi_fix_close(&x); return; }
    gi_sig_fill(&s);
    CHECK_HR(IDWriteGdiInterop1_GetFontSignature_(x.gdi1, (IDWriteFontFace *)NULL, &s), DW_E_INVALIDARG);
    CHECK(gi_sig_zero(&s));
    gi_fix_close(&x);
}
TEST(gis_synthetic_ranges_roundtrip)
{
    static const gi_cf set[] = { { "GiSig", 400, 0, 5, 0x20000003u, 0x80000000u, { 0xA0000001u, 0x00008002u, 0x00000040u, 0x10000000u } } };
    gi_fix x; IDWriteFont *font; IDWriteFontFace *face = NULL; FONTSIGNATURE a, b; GI_FIX_OR_SKIP(x, set);
    if (!x.gdi1) { t_skip("no interop1"); gi_fix_close(&x); return; }
    CHECK_HR_OK(gi_font_lf(&x, "GiSig", 400, 0, 1, &font));
    if (font) {
        gi_sig_fill(&a);
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(x.gdi1, font, &a));
        EXPECT_EQ(a.fsUsb[0], 0xA0000001u); EXPECT_EQ(a.fsUsb[1], 0x00008002u); EXPECT_EQ(a.fsUsb[2], 0x00000040u); EXPECT_EQ(a.fsUsb[3], 0x10000000u);
        EXPECT_EQ(a.fsCsb[0], 0x20000003u); EXPECT_EQ(a.fsCsb[1], 0x80000000u);
        IDWriteFont_CreateFontFace(font, &face);
        if (face) {
            gi_sig_fill(&b);
            CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature_(x.gdi1, face, &b));
            CHECK(gi_sig_eq(&a, &b));
        }
        DW_RELEASE(face); DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gis_synthetic_default_ranges)
{
    static const gi_cf set[] = { { "GiSigDef", 400, 0, 5 } };
    gi_fix x; IDWriteFont *font; FONTSIGNATURE a; GI_FIX_OR_SKIP(x, set);
    if (!x.gdi1) { t_skip("no interop1"); gi_fix_close(&x); return; }
    CHECK_HR_OK(gi_font_lf(&x, "GiSigDef", 400, 0, 1, &font));
    if (font) {
        gi_sig_fill(&a);
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(x.gdi1, font, &a));
        EXPECT_EQ(a.fsUsb[0], 1u);   /* default font: Basic Latin, Latin 1 code page */
        EXPECT_EQ(a.fsUsb[1], 0u); EXPECT_EQ(a.fsUsb[2], 0u); EXPECT_EQ(a.fsUsb[3], 0u);
        EXPECT_EQ(a.fsCsb[0], 1u); EXPECT_EQ(a.fsCsb[1], 0u);
        DW_RELEASE(font);
    }
    gi_fix_close(&x);
}
TEST(gis_distinct_per_font)
{
    static const gi_cf set[] = { { "GiSigA", 400, 0, 5, 0x00000001, 0, { 1 } }, { "GiSigB", 400, 0, 5, 0x00020000, 0, { 0x00000400, 0, 0x08000000, 0 } } };
    gi_fix x; IDWriteFont *a, *b; FONTSIGNATURE sa, sb; GI_FIX_OR_SKIP(x, set);
    if (!x.gdi1) { t_skip("no interop1"); gi_fix_close(&x); return; }
    CHECK_HR_OK(gi_font_lf(&x, "GiSigA", 400, 0, 1, &a));
    CHECK_HR_OK(gi_font_lf(&x, "GiSigB", 400, 0, 1, &b));
    if (a && b) {
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(x.gdi1, a, &sa));
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(x.gdi1, b, &sb));
        CHECK(!gi_sig_eq(&sa, &sb));
        EXPECT_EQ(sb.fsCsb[0], 0x20000u); EXPECT_EQ(sb.fsUsb[0], 0x400u); EXPECT_EQ(sb.fsUsb[2], 0x08000000u);
    }
    DW_RELEASE(a); DW_RELEASE(b);
    gi_fix_close(&x);
}
TEST(gis_simulated_font_same_signature)
{
    /* synthetic bold/oblique does not change the font's coverage */
    static const gi_cf set[] = { { "GiSigS", 400, 0, 5, 0x00000003, 0, { 0x80000003u } } };
    gi_fix x; IDWriteFont *plain, *sim; FONTSIGNATURE a, b; GI_FIX_OR_SKIP(x, set);
    if (!x.gdi1) { t_skip("no interop1"); gi_fix_close(&x); return; }
    CHECK_HR_OK(gi_font_lf(&x, "GiSigS", 400, 0, 1, &plain));
    CHECK_HR_OK(gi_font_lf(&x, "GiSigS", 900, 1, 1, &sim));
    if (plain && sim) {
        CHECK(IDWriteFont_GetSimulations(sim) != 0);
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(x.gdi1, plain, &a));
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(x.gdi1, sim, &b));
        CHECK(gi_sig_eq(&a, &b));
    }
    DW_RELEASE(plain); DW_RELEASE(sim);
    gi_fix_close(&x);
}
static void gi_sig_system(const char *face)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteGdiInterop1 *g1; IDWriteFont *font = NULL; IDWriteFontFace *ff = NULL; LOGFONTW lf;
    FONTSIGNATURE a, b; uint8_t *os2; UINT32 len = 0;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1 || !gi_sys_has(f, face)) { t_skip("%s or interop1 missing", face); DW_RELEASE(g1); DW_RELEASE(g); return; }
    gi_lf(&lf, face, 400, 0, 1);
    CHECK_HR_OK(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, &lf, NULL, &font));
    if (font && SUCCEEDED(IDWriteFont_CreateFontFace(font, &ff)) && ff) {
        gi_sig_fill(&a); gi_sig_fill(&b);
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(g1, font, &a));
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature_(g1, ff, &b));
        CHECK(gi_sig_eq(&a, &b));
        os2 = dw_face_table(ff, DW_TAG('O', 'S', '/', '2'), &len);
        if (os2 && len >= 86) {
            /* OS/2 version >= 1: ulUnicodeRange1..4 at 42, ulCodePageRange1..2 at 78, big endian */
            EXPECT_EQ(a.fsUsb[0], gi_be32(os2 + 42)); EXPECT_EQ(a.fsUsb[1], gi_be32(os2 + 46));
            EXPECT_EQ(a.fsUsb[2], gi_be32(os2 + 50)); EXPECT_EQ(a.fsUsb[3], gi_be32(os2 + 54));
            EXPECT_EQ(a.fsCsb[0], gi_be32(os2 + 78)); EXPECT_EQ(a.fsCsb[1], gi_be32(os2 + 82));
        } else t_note("%s: OS/2 table missing or older than version 1 (%u bytes)", face, len);
        t_note("%s signature usb %08x %08x %08x %08x csb %08x %08x", face, a.fsUsb[0], a.fsUsb[1], a.fsUsb[2], a.fsUsb[3], a.fsCsb[0], a.fsCsb[1]);
        free(os2);
    }
    DW_RELEASE(ff); DW_RELEASE(font); DW_RELEASE(g1); DW_RELEASE(g);
}
#define SIGSYS(id, name) TEST(gis_sys_##id) { gi_sig_system(name); }
SIGSYS(arial, "Arial")
SIGSYS(times, "Times New Roman")
SIGSYS(courier, "Courier New")
SIGSYS(tahoma, "Tahoma")
SIGSYS(segoeui, "Segoe UI")
SIGSYS(marlett, "Marlett")
SIGSYS(consolas, "Consolas")
SIGSYS(segoe_symbol, "Segoe UI Symbol")
SIGSYS(ms_gothic, "MS Gothic")
TEST(gis_arial_latin_bits)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteGdiInterop1 *g1; IDWriteFont *font = NULL; LOGFONTW lf; FONTSIGNATURE s;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1 || !gi_sys_has(f, "Arial")) { t_skip("Arial or interop1 missing"); DW_RELEASE(g1); DW_RELEASE(g); return; }
    gi_lf(&lf, "Arial", 400, 0, 1);
    IDWriteGdiInterop_CreateFontFromLOGFONT(g, &lf, &font);
    if (font) {
        CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(g1, font, &s));
        CHECK(s.fsUsb[0] & 1);        /* Basic Latin */
        CHECK(s.fsCsb[0] & 1);        /* Latin 1 code page */
        DW_RELEASE(font);
    }
    DW_RELEASE(g1); DW_RELEASE(g);
}

/* ---- GetMatchingFontsByLOGFONT ---------------------------------------------------------------------------------------------------------- */
static IDWriteFontSet *gi_set_of(gi_fix *x)
{
    IDWriteFontCollection1 *c1 = NULL; IDWriteFontSet *set = NULL;
    if (SUCCEEDED(IDWriteFontCollection_QueryInterface(x->coll, &IID_IDWriteFontCollection1, (void **)&c1)) && c1) { IDWriteFontCollection1_GetFontSet(c1, &set); IDWriteFontCollection1_Release(c1); }
    return set;
}
/* index of the blob key behind a face reference, -1 when unknown */
static int gi_ref_key(IDWriteFontFaceReference *r)
{
    IDWriteFontFile *file = NULL; const void *key = NULL; UINT32 ks = 0, v = 0xffffffffu; int out = -1;
    if (SUCCEEDED(IDWriteFontFaceReference_GetFontFile(r, &file)) && file) {
        if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(file, &key, &ks)) && ks == 4) { memcpy(&v, key, 4); out = (int)v; }
        IDWriteFontFile_Release(file);
    }
    return out;
}
static void gi_match_check(gi_fix *x, IDWriteFontSet *set, const char *face, int w, int it, int cs, const gi_ent *ents, int n, int hard_order, int keybase)
{
    LOGFONTW lf; IDWriteFontSet *sub = (IDWriteFontSet *)(ULONG_PTR)0x1234; unsigned sc[16]; int sim = 0, i, distinct = 1, j; UINT32 cnt;
    gi_lf(&lf, face, w, it, cs);
    CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(x->gdi1, &lf, set, &sub));
    CHECK(sub != NULL && sub != (IDWriteFontSet *)(ULONG_PTR)0x1234);
    if (!sub || sub == (IDWriteFontSet *)(ULONG_PTR)0x1234) return;
    cnt = IDWriteFontSet_GetFontCount(sub);
    CHECKF(cnt == (UINT32)n, "%s w=%d it=%d: %u fonts, family has %d", face, w, it, cnt, n);
    gi_best(ents, n, gi_charset_mask(cs), w, it, &sim, sc);
    for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) if (sc[i] == sc[j]) distinct = 0;
    if (distinct && cnt == (UINT32)n && hard_order) {
        int order[16], k; UINT32 pos;
        for (i = 0; i < n; i++) order[i] = i;
        for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) if (sc[order[j]] < sc[order[i]]) { k = order[i]; order[i] = order[j]; order[j] = k; }
        for (pos = 0; pos < cnt; pos++) {
            IDWriteFontFaceReference *r = NULL; int e = order[pos], s2 = 0; unsigned scs[16];
            CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(sub, pos, &r));
            if (!r) continue;
            gi_score(&ents[e], gi_charset_mask(cs), w, it, &s2); (void)scs;
            CHECKF(gi_ref_key(r) == keybase + e, "%s w=%d it=%d pos %u: font key %d want %d", face, w, it, pos, gi_ref_key(r), keybase + e);
            CHECKF((int)IDWriteFontFaceReference_GetSimulations(r) == s2, "%s w=%d it=%d pos %u: simulations %d want %d", face, w, it, pos, (int)IDWriteFontFaceReference_GetSimulations(r), s2);
            IDWriteFontFaceReference_Release(r);
        }
    }
    IDWriteFontSet_Release(sub);
}
static const gi_cf gi_ms_ribbi[] = { { "GiMatch", 400, 0, 5 }, { "GiMatch", 700, 0, 5 }, { "GiMatch", 400, 1, 5 }, { "GiMatch", 700, 1, 5 } };
static const gi_cf gi_ms_ribbi_mixed[] = { { "GiMatch", 400, 0, 5 }, { "GiMatch", 700, 0, 5 }, { "GiMatch", 400, 1, 5 }, { "GiMatch", 700, 1, 5 }, { "Other", 400, 0, 5 } };
static const gi_ent gi_ms_ents[] = { { 400, 0, 1, 5 }, { 700, 0, 1, 5 }, { 400, 2, 1, 5 }, { 700, 2, 1, 5 } };
TEST(gis_match_ribbi_orders_by_penalty)
{
    static const int ws[] = { 0, 100, 300, 400, 450, 500, 550, 600, 650, 700, 800, 900, 1000 };
    gi_fix x; IDWriteFontSet *set; size_t i; int it; GI_FIX_OR_SKIP(x, gi_ms_ribbi);
    set = x.gdi1 ? gi_set_of(&x) : NULL;
    if (!set) { t_skip("no interop1 / font set"); gi_fix_close(&x); return; }
    if (IDWriteFontSet_GetFontCount(set) != 4) { t_skip("font set merged fonts"); DW_RELEASE(set); gi_fix_close(&x); return; }
    for (it = 0; it < 2; it++) for (i = 0; i < sizeof ws / sizeof ws[0]; i++) gi_match_check(&x, set, "GiMatch", ws[i], it, DEFAULT_CHARSET, gi_ms_ents, 4, 1, 0);
    DW_RELEASE(set); gi_fix_close(&x);
}
TEST(gis_match_only_requested_family)
{
    gi_fix x; IDWriteFontSet *set; GI_FIX_OR_SKIP(x, gi_ms_ribbi_mixed);
    set = x.gdi1 ? gi_set_of(&x) : NULL;
    if (!set) { t_skip("no interop1 / font set"); gi_fix_close(&x); return; }
    EXPECT_EQ(IDWriteFontSet_GetFontCount(set), 5);
    gi_match_check(&x, set, "GiMatch", 400, 0, 1, gi_ms_ents, 4, 0, 0);
    {
        static const gi_ent one[] = { { 400, 0, 1, 5 } };
        gi_match_check(&x, set, "Other", 400, 0, 1, one, 1, 1, 4);
    }
    DW_RELEASE(set); gi_fix_close(&x);
}
TEST(gis_match_unknown_family_empty_set)
{
    gi_fix x; IDWriteFontSet *set, *sub = (IDWriteFontSet *)(ULONG_PTR)0x1234; LOGFONTW lf; GI_FIX_OR_SKIP(x, gi_ms_ribbi);
    set = x.gdi1 ? gi_set_of(&x) : NULL;
    if (!set) { t_skip("no interop1 / font set"); gi_fix_close(&x); return; }
    gi_lf(&lf, "NoSuchGiFamily", 400, 0, 1);
    CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(x.gdi1, &lf, set, &sub));   /* not NOFONT: an empty set is a valid answer */
    CHECK(sub != NULL && sub != (IDWriteFontSet *)(ULONG_PTR)0x1234);
    if (sub && sub != (IDWriteFontSet *)(ULONG_PTR)0x1234) { EXPECT_EQ(IDWriteFontSet_GetFontCount(sub), 0); EXPECT_REFS(sub, 1); IDWriteFontSet_Release(sub); }
    DW_RELEASE(set); gi_fix_close(&x);
}
TEST(gis_match_empty_face_name)
{
    gi_fix x; IDWriteFontSet *set, *sub = NULL; LOGFONTW lf; GI_FIX_OR_SKIP(x, gi_ms_ribbi);
    set = x.gdi1 ? gi_set_of(&x) : NULL;
    if (!set) { t_skip("no interop1 / font set"); gi_fix_close(&x); return; }
    gi_lf(&lf, "", 400, 0, 1);
    CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(x.gdi1, &lf, set, &sub));
    if (sub) { t_note("empty lfFaceName -> %u fonts", IDWriteFontSet_GetFontCount(sub)); IDWriteFontSet_Release(sub); }
    DW_RELEASE(set); gi_fix_close(&x);
}
TEST(gis_match_full_name_fallback)
{
    gi_fix x; IDWriteFontSet *set, *sub = NULL; LOGFONTW lf; GI_FIX_OR_SKIP(x, gi_ms_ribbi);
    set = x.gdi1 ? gi_set_of(&x) : NULL;
    if (!set) { t_skip("no interop1 / font set"); gi_fix_close(&x); return; }
    gi_lf(&lf, "GiMatch Bold Italic", 400, 0, 1);
    CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(x.gdi1, &lf, set, &sub));
    if (sub) {
        EXPECT_EQ(IDWriteFontSet_GetFontCount(sub), 1);   /* the single font with that full name */
        if (IDWriteFontSet_GetFontCount(sub) == 1) {
            IDWriteFontFaceReference *r = NULL;
            CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(sub, 0, &r));
            if (r) { EXPECT_EQ(gi_ref_key(r), 3); IDWriteFontFaceReference_Release(r); }
        }
        IDWriteFontSet_Release(sub);
    }
    DW_RELEASE(set); gi_fix_close(&x);
}
TEST(gis_match_charset_penalty_orders_code_pages)
{
    static const gi_cf set3[] = { { "GiMc", 400, 0, 5, 0x00000001 }, { "GiMc", 400, 0, 5, 0x00020000 }, { "GiMc", 400, 0, 5, 0x00040000 } };
    gi_fix x; IDWriteFontSet *set, *sub = NULL; LOGFONTW lf; GI_FIX_OR_SKIP(x, set3);
    set = x.gdi1 ? gi_set_of(&x) : NULL;
    if (!set) { t_skip("no interop1 / font set"); gi_fix_close(&x); return; }
    if (IDWriteFontSet_GetFontCount(set) != 3) { t_skip("font set merged fonts"); DW_RELEASE(set); gi_fix_close(&x); return; }
    gi_lf(&lf, "GiMc", 400, 0, SHIFTJIS_CHARSET);
    CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(x.gdi1, &lf, set, &sub));
    if (sub && IDWriteFontSet_GetFontCount(sub) == 3) {
        IDWriteFontFaceReference *r = NULL;
        CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(sub, 0, &r));
        if (r) { EXPECT_EQ(gi_ref_key(r), 1); IDWriteFontFaceReference_Release(r); }   /* the only font with the Japanese code page leads the list */
    } else CHECK(sub != NULL);
    DW_RELEASE(sub); DW_RELEASE(set); gi_fix_close(&x);
}
TEST(gis_match_result_is_independent_set)
{
    gi_fix x; IDWriteFontSet *set, *a = NULL, *b = NULL; LOGFONTW lf; GI_FIX_OR_SKIP(x, gi_ms_ribbi);
    set = x.gdi1 ? gi_set_of(&x) : NULL;
    if (!set) { t_skip("no interop1 / font set"); gi_fix_close(&x); return; }
    gi_lf(&lf, "GiMatch", 400, 0, 1);
    CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(x.gdi1, &lf, set, &a));
    CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(x.gdi1, &lf, set, &b));
    CHECK(a != NULL && b != NULL && a != b);
    if (a) EXPECT_REFS(a, 1);
    DW_RELEASE(a);
    if (b) EXPECT_EQ(IDWriteFontSet_GetFontCount(b), 4);
    DW_RELEASE(b); DW_RELEASE(set); gi_fix_close(&x);
}
TEST(gis_match_set_holds_source_after_release)
{
    gi_fix x; IDWriteFontSet *set, *sub = NULL; LOGFONTW lf; GI_FIX_OR_SKIP(x, gi_ms_ribbi);
    set = x.gdi1 ? gi_set_of(&x) : NULL;
    if (!set) { t_skip("no interop1 / font set"); gi_fix_close(&x); return; }
    gi_lf(&lf, "GiMatch", 700, 1, 1);
    CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(x.gdi1, &lf, set, &sub));
    DW_RELEASE(set);
    if (sub) {
        IDWriteFontFaceReference *r = NULL;
        EXPECT_EQ(IDWriteFontSet_GetFontCount(sub), 4);
        CHECK_HR_OK(IDWriteFontSet_GetFontFaceReference(sub, 0, &r));
        if (r) { EXPECT_EQ(gi_ref_key(r), 3); IDWriteFontFaceReference_Release(r); }
        IDWriteFontSet_Release(sub);
    }
    gi_fix_close(&x);
}
TEST(gis_match_system_arial)
{
    static const char *files[4] = { "arial.ttf", "arialbd.ttf", "ariali.ttf", "arialbi.ttf" };
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteGdiInterop1 *g1; IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    IDWriteFontSet *sys = NULL, *sub = NULL; LOGFONTW lf;
    static const int ws[] = { 0, 300, 400, 550, 600, 700, 900 }; size_t wi; int it;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1 || !f3 || !gi_sys_has(f, "Arial")) { t_skip("Arial, factory3 or interop1 missing"); DW_RELEASE(g1); DW_RELEASE(g); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &sys));
    if (sys)
        for (it = 0; it < 2; it++) for (wi = 0; wi < sizeof ws / sizeof ws[0]; wi++) {
            unsigned sc[4]; int sim = 0, i, j, distinct = 1; UINT32 cnt;
            gi_lf(&lf, "Arial", ws[wi], it, 1);
            sub = NULL;
            CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(g1, &lf, sys, &sub));
            if (!sub) continue;
            cnt = IDWriteFontSet_GetFontCount(sub);
            if (cnt == 4) {
                int order[4] = { 0, 1, 2, 3 }, k; UINT32 pos;
                gi_best(gi_ms_ents, 4, 0xffffffffu, ws[wi], it, &sim, sc);
                for (i = 0; i < 4; i++) for (j = i + 1; j < 4; j++) if (sc[i] == sc[j]) distinct = 0;
                if (distinct) {
                    for (i = 0; i < 4; i++) for (j = i + 1; j < 4; j++) if (sc[order[j]] < sc[order[i]]) { k = order[i]; order[i] = order[j]; order[j] = k; }
                    for (pos = 0; pos < 4; pos++) {
                        IDWriteFontFaceReference *r = NULL; IDWriteFontFace3 *ff3 = NULL; char b[64]; int s2 = 0;
                        if (FAILED(IDWriteFontSet_GetFontFaceReference(sub, pos, &r)) || !r) continue;
                        gi_score(&gi_ms_ents[order[pos]], 0xffffffffu, ws[wi], it, &s2);
                        CHECKF((int)IDWriteFontFaceReference_GetSimulations(r) == s2, "w=%d it=%d pos %u sims %d want %d", ws[wi], it, pos, (int)IDWriteFontFaceReference_GetSimulations(r), s2);
                        if (SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(r, &ff3)) && ff3) {
                            if (gi_face_file((IDWriteFontFace *)ff3, b, sizeof b, NULL, 0)) CHECKF(!strcmp(b, files[order[pos]]), "w=%d it=%d pos %u file %s want %s", ws[wi], it, pos, b, files[order[pos]]);
                            IDWriteFontFace3_Release(ff3);
                        }
                        IDWriteFontFaceReference_Release(r);
                    }
                }
            } else t_note("Arial Win32 family has %u fonts here", cnt);
            IDWriteFontSet_Release(sub);
        }
    DW_RELEASE(sys); DW_RELEASE(g1); DW_RELEASE(g);
}
TEST(gis_match_system_segoe_family_members)
{
    /* "Segoe UI Semibold" is a one-font Win32 family; "Segoe UI" has the four RIBBI fonts only */
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteGdiInterop1 *g1; IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    IDWriteFontSet *sys = NULL, *sub = NULL; LOGFONTW lf;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1 || !f3 || !gi_sys_has(f, "Segoe UI")) { t_skip("Segoe UI, factory3 or interop1 missing"); DW_RELEASE(g1); DW_RELEASE(g); return; }
    IDWriteFactory3_GetSystemFontSet(f3, &sys);
    if (sys) {
        gi_lf(&lf, "Segoe UI Semibold", 600, 0, 1);
        CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(g1, &lf, sys, &sub));
        if (sub) { EXPECT_EQ(IDWriteFontSet_GetFontCount(sub), 2); DW_RELEASE(sub); }   /* Semibold + Semibold Italic share the legacy family */
        gi_lf(&lf, "Segoe UI", 400, 0, 1);
        CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(g1, &lf, sys, &sub));
        if (sub) { EXPECT_EQ(IDWriteFontSet_GetFontCount(sub), 4); DW_RELEASE(sub); }
    }
    DW_RELEASE(sys); DW_RELEASE(g1); DW_RELEASE(g);
}
TEST(gis_match_unknown_family_system_set)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop *g = gi_get(f); IDWriteGdiInterop1 *g1; IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);
    IDWriteFontSet *sys = NULL, *sub = NULL; LOGFONTW lf;
    if (!g) { t_skip("no interop"); return; }
    g1 = gi_get1(g);
    if (!g1 || !f3) { t_skip("factory3 or interop1 missing"); DW_RELEASE(g1); DW_RELEASE(g); return; }
    IDWriteFactory3_GetSystemFontSet(f3, &sys);
    if (sys) {
        gi_lf(&lf, "NoSuchFamilyAnywhere", 400, 0, 1);
        CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(g1, &lf, sys, &sub));
        CHECK(sub != NULL);
        if (sub) EXPECT_EQ(IDWriteFontSet_GetFontCount(sub), 0);
        DW_RELEASE(sub);
    }
    DW_RELEASE(sys); DW_RELEASE(g1); DW_RELEASE(g);
}
