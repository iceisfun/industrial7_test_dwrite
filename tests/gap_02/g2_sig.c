/* IDWriteGdiInterop1::GetFontSignature (IDWriteFontFace* and IDWriteFont* overloads).
 * Win10 decompilation (dwrite.dll!0xdb3d0 face overload, !0xdb3b0 font overload): both forward to GetFontSignatureInternal with the
 * address of the embedded FontFaceLight (NULL for a NULL object); it zeroes the 24-byte FONTSIGNATURE, returns E_INVALIDARG for NULL
 * and otherwise copies the face's cached OS/2 ulUnicodeRange1..4 and ulCodePageRange1..2 (fsUsb[0..3], fsCsb[0..1]). Vtable order:
 * the IDWriteFontFace* overload (GetFontSignature_) precedes the IDWriteFont* one. */
#include "g2.h"

static IDWriteGdiInterop1 *gi1(IDWriteFactory *f)
{
    IDWriteGdiInterop *gi = NULL; IDWriteGdiInterop1 *g1 = NULL;
    if (FAILED(IDWriteFactory_GetGdiInterop(f, &gi)) || !gi) return NULL;
    if (FAILED(IDWriteGdiInterop_QueryInterface(gi, &IID_IDWriteGdiInterop1, (void **)&g1))) g1 = NULL;
    IDWriteGdiInterop_Release(gi);
    return g1;
}
static unsigned be32(const uint8_t *p) { return ((unsigned)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static unsigned be16(const uint8_t *p) { return ((unsigned)p[0] << 8) | p[1]; }

static HRESULT sig_face(IDWriteGdiInterop1 *g, IDWriteFontFace *face, FONTSIGNATURE *s) { memset(s, 0xCC, sizeof *s); return IDWriteGdiInterop1_GetFontSignature_(g, face, s); }

/* a face built from a spec with given OS/2 version and ranges, in a private factory */
static int sig_of_spec(const ttf_spec *sp, FONTSIGNATURE *out, UINT32 face_index, const uint8_t *prebuilt, size_t pn)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteGdiInterop1 *g; dwx_font h; HRESULT hr; int ok = 0;
    uint8_t *b = NULL; size_t n = 0;
    if (!f) return 0;
    if (!prebuilt) { b = ttf_build(sp, &n); prebuilt = b; pn = n; }
    g = gi1(f);
    if (g && prebuilt && SUCCEEDED(dwx_font_open(&h, f, prebuilt, pn, face_index, DWRITE_FONT_SIMULATIONS_NONE)) && h.face) {
        hr = sig_face(g, h.face, out);
        ok = SUCCEEDED(hr);
        dwx_font_close(&h);
    } else if (g) dwx_font_close(&h);
    free(b); DW_RELEASE(g); DW_RELEASE(f);
    return ok;
}

TEST(gap2_sig_os2_versions_roundtrip)
{
    /* Versions 1..5 all carry ulCodePageRange1/2; every one must round-trip exactly. */
    int v;
    for (v = 1; v <= 5; v++) {
        ttf_spec s; FONTSIGNATURE sig;
        g2_spec(&s, "G2Sig");
        s.os2_version = v;
        s.unicode_range[0] = 0x80000001u + v; s.unicode_range[1] = 0x00400000u; s.unicode_range[2] = 0x0000F00Du; s.unicode_range[3] = 0x10000000u + v;
        s.codepage_range[0] = 0x20000000u + v; s.codepage_range[1] = 0x00010000u;
        if (!sig_of_spec(&s, &sig, 0, NULL, 0)) { t_skip("interop unavailable"); return; }
        CHECKF(sig.fsUsb[0] == s.unicode_range[0] && sig.fsUsb[1] == s.unicode_range[1] && sig.fsUsb[2] == s.unicode_range[2] && sig.fsUsb[3] == s.unicode_range[3], "OS/2 v%d usb %08lx %08lx %08lx %08lx", v, sig.fsUsb[0], sig.fsUsb[1], sig.fsUsb[2], sig.fsUsb[3]);
        CHECKF(sig.fsCsb[0] == s.codepage_range[0] && sig.fsCsb[1] == s.codepage_range[1], "OS/2 v%d csb %08lx %08lx", v, sig.fsCsb[0], sig.fsCsb[1]);
    }
}

TEST(gap2_sig_os2_version0_unicode_ranges)
{
    /* A 78-byte version 0 table ends before ulCodePageRange; the Unicode ranges (offset 42) are still inside it. */
    ttf_spec s; FONTSIGNATURE sig;
    g2_spec(&s, "G2Sig0");
    s.os2_version = 0;
    s.unicode_range[0] = 0x11111111u; s.unicode_range[1] = 0x22222222u; s.unicode_range[2] = 0x33333333u; s.unicode_range[3] = 0x44444444u;
    s.codepage_range[0] = 0x55555555u; s.codepage_range[1] = 0x66666666u;
    if (!sig_of_spec(&s, &sig, 0, NULL, 0)) { t_note("a version 0 OS/2 table is not accepted for GetFontSignature"); return; }
    CHECK(sig.fsUsb[0] == 0x11111111u && sig.fsUsb[1] == 0x22222222u && sig.fsUsb[2] == 0x33333333u && sig.fsUsb[3] == 0x44444444u);
    t_note("OS/2 v0 code page ranges reported as %08lx %08lx", sig.fsCsb[0], sig.fsCsb[1]);
}

TEST(gap2_sig_single_bit_patterns)
{
    int bit;
    for (bit = 0; bit < 32; bit += 3) {
        ttf_spec s; FONTSIGNATURE sig; unsigned m = 1u << bit;
        g2_spec(&s, "G2SigBit");
        s.os2_version = 3;
        s.unicode_range[0] = m; s.unicode_range[1] = ~m; s.unicode_range[2] = 0; s.unicode_range[3] = m;
        s.codepage_range[0] = ~m; s.codepage_range[1] = m;
        if (!sig_of_spec(&s, &sig, 0, NULL, 0)) { t_skip("interop unavailable"); return; }
        CHECKF(sig.fsUsb[0] == m && sig.fsUsb[1] == ~m && sig.fsUsb[2] == 0 && sig.fsUsb[3] == m && sig.fsCsb[0] == ~m && sig.fsCsb[1] == m, "bit %d", bit);
    }
}

TEST(gap2_sig_all_zero_and_all_ones)
{
    unsigned vals[2] = { 0u, 0xFFFFFFFFu }; int i;
    for (i = 0; i < 2; i++) {
        ttf_spec s; FONTSIGNATURE sig; int k;
        g2_spec(&s, "G2SigAll"); s.os2_version = 4;
        for (k = 0; k < 4; k++) s.unicode_range[k] = vals[i];
        s.codepage_range[0] = s.codepage_range[1] = vals[i];
        if (!sig_of_spec(&s, &sig, 0, NULL, 0)) { t_skip("interop unavailable"); return; }
        for (k = 0; k < 4; k++) CHECK(sig.fsUsb[k] == vals[i]);
        CHECK(sig.fsCsb[0] == vals[i] && sig.fsCsb[1] == vals[i]);
    }
}

TEST(gap2_sig_ttc_each_face_has_own_ranges)
{
    ttf_spec s0, s1; uint8_t *b0, *b1, *ttc; size_t n0, n1, nt; const uint8_t *bl[2]; size_t sz[2]; FONTSIGNATURE a, b;
    g2_spec(&s0, "G2SigTA"); g2_spec(&s1, "G2SigTB");
    s0.unicode_range[0] = 0xAAAA0001u; s0.codepage_range[0] = 0x0000000Fu;
    s1.unicode_range[0] = 0xBBBB0002u; s1.codepage_range[0] = 0x000000F0u;
    b0 = ttf_build(&s0, &n0); b1 = ttf_build(&s1, &n1); bl[0] = b0; bl[1] = b1; sz[0] = n0; sz[1] = n1;
    ttc = ttf_build_ttc(bl, sz, 2, &nt);
    if (ttc && sig_of_spec(NULL, &a, 0, ttc, nt) && sig_of_spec(NULL, &b, 1, ttc, nt)) {
        CHECK(a.fsUsb[0] == 0xAAAA0001u && b.fsUsb[0] == 0xBBBB0002u);
        CHECK(a.fsCsb[0] == 0x0Fu && b.fsCsb[0] == 0xF0u);
    } else t_skip("ttc not available");
    free(b0); free(b1); free(ttc);
}

TEST(gap2_sig_overwrites_prefilled_output)
{
    g2_face x; IDWriteGdiInterop1 *g; FONTSIGNATURE s; int i; const BYTE *p = (const BYTE *)&s;
    if (!g2_face_open(&x, NULL)) return;
    g = gi1(x.f);
    if (g) {
        sig_face(g, x.face, &s);
        for (i = 0; i < (int)sizeof s; i++) (void)p[i];
        { FONTSIGNATURE s2; memset(&s2, 0x5A, sizeof s2); CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature_(g, x.face, &s2)); CHECK(!memcmp(&s, &s2, sizeof s)); }
        { FONTSIGNATURE s3; memset(&s3, 0x00, sizeof s3); CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature_(g, x.face, &s3)); CHECK(!memcmp(&s, &s3, sizeof s)); }
        DW_RELEASE(g);
    } else t_skip("no interop1");
    g2_face_close(&x);
}

TEST(gap2_sig_through_derived_face_pointers)
{
    g2_face x; IDWriteGdiInterop1 *g; FONTSIGNATURE a, b, c;
    if (!g2_face_open(&x, NULL)) return;
    g = gi1(x.f);
    if (g && x.face3) {
        IDWriteFontFace1 *f1 = NULL;
        CHECK_HR_OK(sig_face(g, x.face, &a));
        CHECK_HR_OK(sig_face(g, (IDWriteFontFace *)x.face3, &b));
        CHECK(!memcmp(&a, &b, sizeof a));
        if (SUCCEEDED(IDWriteFontFace_QueryInterface(x.face, &IID_IDWriteFontFace1, (void **)&f1))) {
            CHECK_HR_OK(sig_face(g, (IDWriteFontFace *)f1, &c));
            CHECK(!memcmp(&a, &c, sizeof a));
            DW_RELEASE(f1);
        }
    } else t_skip("no interop1 / face3");
    DW_RELEASE(g);
    g2_face_close(&x);
}

TEST(gap2_sig_refcounts_balanced)
{
    g2_face x; IDWriteGdiInterop1 *g;
    if (!g2_face_open(&x, NULL)) return;
    g = gi1(x.f);
    if (g) {
        FONTSIGNATURE s; ULONG a = dw_refcount((IUnknown *)x.face), b, ga = dw_refcount((IUnknown *)g), gb; int i;
        for (i = 0; i < 10; i++) sig_face(g, x.face, &s);
        b = dw_refcount((IUnknown *)x.face); gb = dw_refcount((IUnknown *)g);
        CHECK(a == b && ga == gb);
        DW_RELEASE(g);
    } else t_skip("no interop1");
    g2_face_close(&x);
}

static void sys_sig(const WCHAR *fam)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop1 *g = gi1(f); IDWriteFontFace *face;
    uint8_t *os2; UINT32 len = 0; FONTSIGNATURE s;
    if (!g) { t_skip("no interop1"); return; }
    face = dw_sysface(f, fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { DW_RELEASE(g); t_skip("%ls not installed", fam); return; }
    os2 = dw_face_table(face, DW_TAG('O', 'S', '/', '2'), &len);
    CHECK_HR_OK(sig_face(g, face, &s));
    if (os2 && len >= 78) {
        CHECKF(s.fsUsb[0] == be32(os2 + 42) && s.fsUsb[1] == be32(os2 + 46) && s.fsUsb[2] == be32(os2 + 50) && s.fsUsb[3] == be32(os2 + 54), "%ls usb %08lx %08lx %08lx %08lx vs OS/2", fam, s.fsUsb[0], s.fsUsb[1], s.fsUsb[2], s.fsUsb[3]);
        if (be16(os2) >= 1 && len >= 86) CHECKF(s.fsCsb[0] == be32(os2 + 78) && s.fsCsb[1] == be32(os2 + 82), "%ls csb %08lx %08lx", fam, s.fsCsb[0], s.fsCsb[1]);
        t_note("%ls: OS/2 v%u usb %08lx %08lx %08lx %08lx csb %08lx %08lx", fam, be16(os2), s.fsUsb[0], s.fsUsb[1], s.fsUsb[2], s.fsUsb[3], s.fsCsb[0], s.fsCsb[1]);
    } else t_note("%ls has no usable OS/2 table", fam);
    free(os2); DW_RELEASE(face); DW_RELEASE(g);
}
TEST(gap2_sig_system_arial)    { sys_sig(L"Arial"); }
TEST(gap2_sig_system_times)    { sys_sig(L"Times New Roman"); }
TEST(gap2_sig_system_courier)  { sys_sig(L"Courier New"); }
TEST(gap2_sig_system_segoe)    { sys_sig(L"Segoe UI"); }
TEST(gap2_sig_system_tahoma)   { sys_sig(L"Tahoma"); }
TEST(gap2_sig_system_consolas) { sys_sig(L"Consolas"); }
TEST(gap2_sig_system_symbol)   { sys_sig(L"Segoe UI Symbol"); }
TEST(gap2_sig_system_marlett)  { sys_sig(L"Marlett"); }
TEST(gap2_sig_system_ms_gothic){ sys_sig(L"MS Gothic"); }
TEST(gap2_sig_system_emoji)    { sys_sig(L"Segoe UI Emoji"); }

TEST(gap2_sig_font_overload_matches_face_overload_system)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop1 *g = gi1(f); IDWriteFontCollection *sc = NULL; IDWriteFont *font; IDWriteFontFace *face = NULL;
    FONTSIGNATURE a, b;
    if (!g) { t_skip("no interop1"); return; }
    IDWriteFactory_GetSystemFontCollection(f, &sc, FALSE);
    font = sc ? g2_font(sc, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    if (!font) { DW_RELEASE(sc); DW_RELEASE(g); t_skip("Arial not installed"); return; }
    IDWriteFont_CreateFontFace(font, &face);
    memset(&a, 0xCC, sizeof a);
    CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(g, font, &a));
    if (face) { CHECK_HR_OK(sig_face(g, face, &b)); CHECK(!memcmp(&a, &b, sizeof a)); }
    DW_RELEASE(face); DW_RELEASE(font); DW_RELEASE(sc); DW_RELEASE(g);
}

TEST(gap2_sig_bold_and_regular_arial_have_signatures)
{
    IDWriteFactory *f = dw_shared(); IDWriteGdiInterop1 *g = gi1(f); IDWriteFontFace *r, *b; FONTSIGNATURE sr, sb;
    if (!g) { t_skip("no interop1"); return; }
    r = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    b = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (r && b) {
        CHECK_HR_OK(sig_face(g, r, &sr)); CHECK_HR_OK(sig_face(g, b, &sb));
        CHECK(sr.fsUsb[0] != 0 && sb.fsUsb[0] != 0);
        CHECK((sr.fsUsb[0] & 1) && (sb.fsUsb[0] & 1));            /* Basic Latin bit */
    } else t_skip("Arial not installed");
    DW_RELEASE(r); DW_RELEASE(b); DW_RELEASE(g);
}
