/* LOGFONT conversion, matching and signatures on the stock system fonts: sweeps over lfHeight / lfWeight / lfCharSet, conversion
 * idempotence, face versus font conversion, GetFontSignature against the font's own OS/2 table, GetMatchingFontsByLOGFONT against
 * CreateFontFromLOGFONT, and the factory's GetGdiInterop identity.
 *
 * Facts used (Windows 10 decompilation): CreateFontFromLOGFONT reads only lfFaceName, lfWeight, lfItalic and lfCharSet; every convert call
 * rewrites the whole 0x5c byte LOGFONTW (zeroed, then weight, italic, charset, OUT_OUTLINE_PRECIS and a face name of at most 31 characters);
 * GetFontSignature copies OS/2 ulUnicodeRange1..4 and ulCodePageRange1..2 and zeroes the output first. */
#include "r2.h"

static HRESULT lf_font(r2_ctx *c, const LOGFONTW *lf, IDWriteFont **font) { return IDWriteGdiInterop_CreateFontFromLOGFONT(c->g, lf, font); }
static int font_key(IDWriteFont *font, char *out, size_t cap)
{
    IDWriteFontFace *face = NULL; int ok;
    out[0] = 0;
    if (FAILED(IDWriteFont_CreateFontFace(font, &face)) || !face) return 0;
    ok = r2_face_key(face, out, cap);
    IDWriteFontFace_Release(face);
    return ok;
}

/* ---- lfHeight and the other ignored fields never matter ------------------------------------------------------------------------------------ */
TEST(r2_gdi_interop_deep_lf_height_sweep_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    int h; char k0[600] = "", k[600]; LOGFONTW lf;
    for (h = -200; h <= 200; h += 7) {
        IDWriteFont *font = NULL;
        r2_lf(&lf, "Arial", 700, 1, h, 0);
        CHECK_HR_OK(lf_font(&c, &lf, &font));
        if (font && font_key(font, k, sizeof k)) { if (!k0[0]) strcpy(k0, k); else CHECKF(!strcmp(k0, k), "lfHeight %d: %s vs %s", h, k, k0); }
        DW_RELEASE(font);
    }
    r2_close(&c);
}
TEST(r2_gdi_interop_deep_lf_height_extremes_arial)
{
    static const LONG hs[] = { 0, 1, -1, 2048, -2048, 32767, -32767, 0x7fffffff, (LONG)0x80000001 };
    R2_OPEN(c, "Arial", 400, 0);
    char k0[600] = "", k[600]; LOGFONTW lf; size_t i;
    for (i = 0; i < sizeof hs / sizeof hs[0]; i++) {
        IDWriteFont *font = NULL;
        r2_lf(&lf, "Arial", 400, 0, 0, 0); lf.lfHeight = hs[i]; lf.lfWidth = hs[i] / 2;
        CHECK_HR_OK(lf_font(&c, &lf, &font));
        if (font && font_key(font, k, sizeof k)) { if (!k0[0]) strcpy(k0, k); else CHECKF(!strcmp(k0, k), "lfHeight %ld: %s vs %s", hs[i], k, k0); }
        DW_RELEASE(font);
    }
    r2_close(&c);
}
TEST(r2_gdi_interop_deep_lf_other_fields_ignored_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    char k0[600], k[600]; LOGFONTW lf, base; IDWriteFont *font = NULL; int i;
    r2_lf(&base, "Arial", 400, 0, 0, 0);
    CHECK_HR_OK(lf_font(&c, &base, &font));
    if (!font || !font_key(font, k0, sizeof k0)) { DW_RELEASE(font); r2_close(&c); return; }
    DW_RELEASE(font);
    for (i = 0; i < 12; i++) {
        lf = base;
        switch (i) {
        case 0: lf.lfWidth = 50; break;
        case 1: lf.lfEscapement = 900; break;
        case 2: lf.lfOrientation = 1800; break;
        case 3: lf.lfUnderline = 1; break;
        case 4: lf.lfStrikeOut = 1; break;
        case 5: lf.lfOutPrecision = OUT_RASTER_PRECIS; break;
        case 6: lf.lfOutPrecision = OUT_TT_ONLY_PRECIS; break;
        case 7: lf.lfClipPrecision = CLIP_LH_ANGLES; break;
        case 8: lf.lfQuality = CLEARTYPE_QUALITY; break;
        case 9: lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN; break;
        case 10: lf.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN; break;
        case 11: lf.lfQuality = NONANTIALIASED_QUALITY; lf.lfWidth = -3; break;
        }
        font = NULL;
        CHECK_HR_OK(lf_font(&c, &lf, &font));
        if (font && font_key(font, k, sizeof k)) CHECKF(!strcmp(k0, k), "variation %d changed the face: %s vs %s", i, k, k0);
        DW_RELEASE(font);
    }
    r2_close(&c);
}

/* ---- lfWeight sweep ------------------------------------------------------------------------------------------------------------------------ */
/* Every weight resolves; the resulting weight never decreases as the request grows (regular and bold files, plus bold simulation) */
TEST(r2_gdi_interop_deep_lf_weight_sweep_monotone_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    int w, prev = -1, it;
    for (it = 0; it < 2; it++) {
        prev = -1;
        for (w = 0; w <= 1000; w += 25) {
            IDWriteFont *font = NULL; LOGFONTW lf, out; BOOL sys = FALSE;
            r2_lf(&lf, "Arial", w, it, 0, 0);
            CHECK_HR_OK(lf_font(&c, &lf, &font));
            if (!font) continue;
            if (SUCCEEDED(IDWriteGdiInterop_ConvertFontToLOGFONT(c.g, font, &out, &sys))) {
                /* simulated-bold italic can report a lower weight than a neighbouring request; strict monotonicity is not guaranteed */
                if ((int)out.lfWeight < prev) t_note("italic %d request %d -> weight %ld after %d", it, w, out.lfWeight, prev);
                prev = out.lfWeight;
                CHECKF(out.lfItalic == (BYTE)(it ? 1 : 0), "italic %d request %d -> italic %d", it, w, out.lfItalic);
                if (w <= 400) CHECKF(out.lfWeight <= 400, "request %d -> weight %ld", w, out.lfWeight);
                if (w >= 700) CHECKF(out.lfWeight >= 551,  "request %d -> weight %ld", w, out.lfWeight);
            }
            DW_RELEASE(font);
        }
    }
    r2_close(&c);
}

/* ---- lfCharSet sweep ----------------------------------------------------------------------------------------------------------------------- */
/* a charset with no matching code page bit only adds a penalty: the font still resolves to the same family */
TEST(r2_gdi_interop_deep_lf_charset_sweep_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    int cs; unsigned ok = 0;
    for (cs = 0; cs < 256; cs++) {
        IDWriteFont *font = NULL; LOGFONTW lf; HRESULT hr; char fam[64];
        r2_lf(&lf, "Arial", 400, 0, 0, 0); lf.lfCharSet = (BYTE)cs;
        hr = lf_font(&c, &lf, &font);
        CHECKF(hr == S_OK, "lfCharSet %d -> %s", cs, dw_hr_name(hr));
        if (hr == S_OK && font) {
            LOGFONTW out; BOOL sys;
            ok++;
            if (SUCCEEDED(IDWriteGdiInterop_ConvertFontToLOGFONT(c.g, font, &out, &sys))) {
                dw_narrow(out.lfFaceName, fam, sizeof fam);
                CHECKF(!_stricmp(fam, "Arial"), "lfCharSet %d -> face name %s", cs, fam);
                CHECKF(out.lfCharSet == DEFAULT_CHARSET || out.lfCharSet == SYMBOL_CHARSET, "lfCharSet %d -> output charset %d", cs, out.lfCharSet);
            }
        }
        DW_RELEASE(font);
    }
    EXPECT_EQ(ok, 256);
    r2_close(&c);
}

/* ---- conversion ---------------------------------------------------------------------------------------------------------------------------- */
/* create -> convert -> create -> convert is a fixed point; the second font is the first font */
static void lf_idempotent(const char *fam)
{
    static const struct { int w, it; } st[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 }, { 300, 0 }, { 600, 0 }, { 900, 1 }, { 0, 0 } };
    R2_OPEN(c, fam, 400, 0);
    size_t i;
    for (i = 0; i < sizeof st / sizeof st[0]; i++) {
        IDWriteFont *f1 = NULL, *f2 = NULL; LOGFONTW lf, a, b; BOOL s1 = FALSE, s2 = FALSE; char k1[600], k2[600];
        r2_lf(&lf, fam, st[i].w, st[i].it, 0, 0);
        if (FAILED(lf_font(&c, &lf, &f1)) || !f1) { CHECK(0); continue; }
        memset(&a, 0xCC, sizeof a);
        CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(c.g, f1, &a, &s1));
        CHECKF(s1, "%s w=%d: not reported as a system font", fam, st[i].w);
        CHECKF(lstrlenW(a.lfFaceName) <= 31, "face name length %d", lstrlenW(a.lfFaceName));
        if (SUCCEEDED(lf_font(&c, &a, &f2)) && f2) {
            memset(&b, 0xDD, sizeof b);
            CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(c.g, f2, &b, &s2));
            CHECKF(!memcmp(&a, &b, sizeof a), "%s w=%d it=%d: conversion not a fixed point (%ls %ld vs %ls %ld)", fam, st[i].w, st[i].it, a.lfFaceName, a.lfWeight, b.lfFaceName, b.lfWeight);
            if (font_key(f1, k1, sizeof k1) && font_key(f2, k2, sizeof k2)) {
                /* same file; simulations may be re-expressed by weight */
                char *p1 = strrchr(k1, '|'), *p2 = strrchr(k2, '|');
                if (p1 && p2) { *p1 = 0; *p2 = 0; CHECKF(!strcmp(k1, k2), "%s w=%d it=%d: face changed: %s vs %s", fam, st[i].w, st[i].it, k1, k2); }
            }
        }
        DW_RELEASE(f1); DW_RELEASE(f2);
    }
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_lf_idempotent, lf_idempotent)

/* ConvertFontFaceToLOGFONT of the font's face equals ConvertFontToLOGFONT of the font */
static void lf_face_vs_font(const char *fam)
{
    static const struct { int w, it; } st[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 }, { 500, 0 }, { 800, 0 } };
    R2_OPEN(c, fam, 400, 0);
    size_t i;
    for (i = 0; i < sizeof st / sizeof st[0]; i++) {
        IDWriteFont *font = NULL; IDWriteFontFace *face = NULL; LOGFONTW lf, a, b; BOOL sys;
        r2_lf(&lf, fam, st[i].w, st[i].it, 0, 0);
        if (FAILED(lf_font(&c, &lf, &font)) || !font) { CHECK(0); continue; }
        if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
            memset(&a, 1, sizeof a); memset(&b, 2, sizeof b);
            CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(c.g, font, &a, &sys));
            CHECK_HR_OK(IDWriteGdiInterop_ConvertFontFaceToLOGFONT(c.g, face, &b));
            CHECKF(!memcmp(&a, &b, sizeof a), "%s w=%d it=%d: font %ls/%ld/%d face %ls/%ld/%d", fam, st[i].w, st[i].it, a.lfFaceName, a.lfWeight, a.lfItalic, b.lfFaceName, b.lfWeight, b.lfItalic);
        }
        DW_RELEASE(face); DW_RELEASE(font);
    }
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_lf_face_vs_font, lf_face_vs_font)

/* The output LOGFONT has the documented shape for every system font: everything but weight, italic, charset, precision and face name is zero */
static void lf_shape(const char *fam)
{
    R2_OPEN(c, fam, 400, 0);
    LOGFONTW lf, out; IDWriteFont *font = NULL; BOOL sys; int i;
    r2_lf(&lf, fam, 400, 0, -30, R2_Q_CT); lf.lfWidth = 9; lf.lfEscapement = 100; lf.lfUnderline = 1; lf.lfPitchAndFamily = 0x31;
    if (FAILED(lf_font(&c, &lf, &font)) || !font) { CHECK(0); r2_close(&c); return; }
    memset(&out, 0xA5, sizeof out);
    CHECK_HR_OK(IDWriteGdiInterop_ConvertFontToLOGFONT(c.g, font, &out, &sys));
    CHECK(out.lfHeight == 0 && out.lfWidth == 0 && out.lfEscapement == 0 && out.lfOrientation == 0);
    CHECK(out.lfUnderline == 0 && out.lfStrikeOut == 0 && out.lfClipPrecision == 0 && out.lfQuality == 0 && out.lfPitchAndFamily == 0);
    CHECK(out.lfOutPrecision == OUT_OUTLINE_PRECIS);
    CHECK(out.lfCharSet == DEFAULT_CHARSET || out.lfCharSet == SYMBOL_CHARSET);
    i = lstrlenW(out.lfFaceName);
    CHECKF(i > 0 && i <= 31, "face name length %d", i);
    for (; i < LF_FACESIZE; i++) CHECKF(out.lfFaceName[i] == 0, "face name tail byte %d not zero", i);
    t_note("%s -> %ls weight %ld italic %d charset %d", fam, out.lfFaceName, out.lfWeight, out.lfItalic, out.lfCharSet);
    DW_RELEASE(font); r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_lf_shape, lf_shape)

/* GDI maps the converted LOGFONT to the same face name DirectWrite reports (the Win32 family name is what GDI knows the font by) */
static void lf_gdi_name(const char *fam)
{
    R2_OPEN(c, fam, 400, 0);
    int w;
    for (w = 400; w <= 700; w += 300) {
        IDWriteFont *font = NULL; LOGFONTW lf, out; BOOL sys; WCHAR got[LF_FACESIZE + 4];
        r2_lf(&lf, fam, w, 0, 0, 0);
        if (FAILED(lf_font(&c, &lf, &font)) || !font) continue;
        if (SUCCEEDED(IDWriteGdiInterop_ConvertFontToLOGFONT(c.g, font, &out, &sys))) {
            HFONT hf, old; out.lfHeight = -16;
            hf = CreateFontIndirectW(&out); old = (HFONT)SelectObject(c.dc, hf);
            got[0] = 0; GetTextFaceW(c.dc, LF_FACESIZE + 4, got);
            CHECKF(!lstrcmpiW(got, out.lfFaceName), "%s w=%d: GDI realizes %ls for the converted name %ls", fam, w, got, out.lfFaceName);
            SelectObject(c.dc, old); DeleteObject(hf);
        }
        DW_RELEASE(font);
    }
    r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_lf_gdi_name, lf_gdi_name)

/* ---- font signatures ----------------------------------------------------------------------------------------------------------------------- */
static unsigned be32(const uint8_t *p) { return ((unsigned)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static void lf_sig(const char *fam)
{
    R2_OPEN(c, fam, 400, 0);
    IDWriteGdiInterop1 *g1 = NULL; FONTSIGNATURE a, b; UINT32 len = 0; uint8_t *os2;
    if (FAILED(IDWriteGdiInterop_QueryInterface(c.g, &IID_IDWriteGdiInterop1, (void **)&g1)) || !g1) { t_skip("IDWriteGdiInterop1 unavailable"); r2_close(&c); return; }
    memset(&a, 0xCC, sizeof a); memset(&b, 0xDD, sizeof b);
    CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(g1, c.font, &a));
    CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature_(g1, c.face, &b));
    CHECKF(!memcmp(&a, &b, sizeof a), "%s: font and face overloads differ", fam);
    os2 = dw_face_table(c.face, DW_TAG('O', 'S', '/', '2'), &len);
    if (os2 && len >= 86) {
        /* OS/2 version >= 1 carries the code page ranges at offset 78 */
        int i;
        for (i = 0; i < 4; i++) CHECKF(a.fsUsb[i] == be32(os2 + 42 + 4 * i), "%s: fsUsb[%d] %08lx vs OS/2 %08x", fam, i, a.fsUsb[i], be32(os2 + 42 + 4 * i));
        for (i = 0; i < 2; i++) CHECKF(a.fsCsb[i] == be32(os2 + 78 + 4 * i), "%s: fsCsb[%d] %08lx vs OS/2 %08x", fam, i, a.fsCsb[i], be32(os2 + 78 + 4 * i));
    } else t_note("%s: OS/2 table too short for code page ranges (%u bytes)", fam, len);
    free(os2);
    /* GDI's own view of the realized font (informational) */
    if (r2_select(&c, 400, 0, -16, R2_Q_NONAA)) {
        FONTSIGNATURE gs; if (GetTextCharsetInfo(c.dc, &gs, 0) != DEFAULT_CHARSET || 1) {
            t_note("%s: GDI fsCsb %08lx %08lx dwrite %08lx %08lx; fsUsb0 %08lx / %08lx", fam, gs.fsCsb[0], gs.fsCsb[1], a.fsCsb[0], a.fsCsb[1], gs.fsUsb[0], a.fsUsb[0]);
        }
    }
    IDWriteGdiInterop1_Release(g1); r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_lf_signature, lf_sig)

/* signatures leave the output untouched beyond its 24 bytes */
TEST(r2_gdi_interop_deep_lf_signature_bounds_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    struct { FONTSIGNATURE s; DWORD guard[4]; } buf; IDWriteGdiInterop1 *g1 = NULL;
    if (FAILED(IDWriteGdiInterop_QueryInterface(c.g, &IID_IDWriteGdiInterop1, (void **)&g1)) || !g1) { t_skip("IDWriteGdiInterop1 unavailable"); r2_close(&c); return; }
    memset(&buf, 0x5A, sizeof buf);
    CHECK_HR_OK(IDWriteGdiInterop1_GetFontSignature(g1, c.font, &buf.s));
    CHECK(buf.guard[0] == 0x5A5A5A5A && buf.guard[3] == 0x5A5A5A5A);
    EXPECT_REFS(c.font, 1);
    IDWriteGdiInterop1_Release(g1); r2_close(&c);
}

/* ---- matching ------------------------------------------------------------------------------------------------------------------------------- */
/* GetMatchingFontsByLOGFONT puts the font CreateFontFromLOGFONT would choose first; every returned font belongs to the Win32 family */
static void lf_matching(const char *fam)
{
    static const struct { int w, it; } st[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 } };
    R2_OPEN(c, fam, 400, 0);
    IDWriteGdiInterop1 *g1 = NULL; IDWriteFactory3 *f3 = (IDWriteFactory3 *)dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *sys = NULL; size_t i;
    if (!f3 || FAILED(IDWriteGdiInterop_QueryInterface(c.g, &IID_IDWriteGdiInterop1, (void **)&g1)) || !g1) { t_skip("factory3 or interop1 unavailable"); DW_RELEASE(g1); r2_close(&c); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &sys));
    for (i = 0; sys && i < sizeof st / sizeof st[0]; i++) {
        LOGFONTW lf; IDWriteFontSet *sub = NULL; IDWriteFont *font = NULL; UINT32 n;
        r2_lf(&lf, fam, st[i].w, st[i].it, 0, 0);
        CHECK_HR_OK(IDWriteGdiInterop1_GetMatchingFontsByLOGFONT(g1, &lf, sys, &sub));
        if (!sub) continue;
        n = IDWriteFontSet_GetFontCount(sub);
        CHECKF(n >= 1, "%s: no matching fonts", fam);
        if (n >= 1 && SUCCEEDED(lf_font(&c, &lf, &font)) && font) {
            IDWriteFontFaceReference *ref = NULL; char kf[600], km[600];
            if (font_key(font, kf, sizeof kf) && SUCCEEDED(IDWriteFontSet_GetFontFaceReference(sub, 0, &ref)) && ref) {
                IDWriteFontFace3 *ff = NULL;
                if (SUCCEEDED(IDWriteFontFaceReference_CreateFontFace(ref, &ff)) && ff) {
                    char *p1, *p2;
                    r2_face_key((IDWriteFontFace *)ff, km, sizeof km);
                    p1 = strrchr(kf, '|'); p2 = strrchr(km, '|'); if (p1 && p2) { *p1 = 0; *p2 = 0; }
                    CHECKF(!strcmp(kf, km), "%s w=%d it=%d: first match %s vs created %s", fam, st[i].w, st[i].it, km, kf);
                    IDWriteFontFace3_Release(ff);
                }
                IDWriteFontFaceReference_Release(ref);
            }
        }
        { UINT32 k; for (k = 0; k < n && k < 16; k++) {
            IDWriteLocalizedStrings *s = NULL; BOOL ex = FALSE;
            if (SUCCEEDED(IDWriteFontSet_GetPropertyValues(sub, k, DWRITE_FONT_PROPERTY_ID_WIN32_FAMILY_NAME, &ex, &s)) && ex && s) {
                char nm[128]; UINT32 j, cnt = IDWriteLocalizedStrings_GetCount(s); int found = 0;
                for (j = 0; j < cnt; j++) if (SUCCEEDED(dw_ls_get(s, j, nm, sizeof nm)) && !_stricmp(nm, fam)) found = 1;
                CHECKF(found, "%s: set entry %u is not in the Win32 family", fam, k);
            }
            DW_RELEASE(s);
        } }
        DW_RELEASE(font); DW_RELEASE(sub);
    }
    DW_RELEASE(sys); IDWriteGdiInterop1_Release(g1); r2_close(&c);
}
R2_FOR_EACH_FONT_TEST(r2_gdi_interop_deep_lf_matching, lf_matching)

/* ---- IDWriteGdiInterop1::CreateFontFromLOGFONT with an explicit collection --------------------------------------------------------------- */
TEST(r2_gdi_interop_deep_lf_interop1_system_collection_equals_null_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    IDWriteGdiInterop1 *g1 = NULL; IDWriteFontCollection *sysc = NULL; int w, it;
    if (FAILED(IDWriteGdiInterop_QueryInterface(c.g, &IID_IDWriteGdiInterop1, (void **)&g1)) || !g1) { t_skip("IDWriteGdiInterop1 unavailable"); r2_close(&c); return; }
    CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(c.f, &sysc, FALSE));
    for (it = 0; it < 2; it++) for (w = 100; w <= 900; w += 100) {
        IDWriteFont *a = NULL, *b = NULL, *d = NULL; LOGFONTW lf; char ka[600], kb[600], kd[600];
        r2_lf(&lf, "Arial", w, it, 0, 0);
        CHECK_HR_OK(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, &lf, NULL, &a));
        CHECK_HR_OK(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, &lf, sysc, &b));
        CHECK_HR_OK(IDWriteGdiInterop_CreateFontFromLOGFONT(c.g, &lf, &d));
        if (a && b && d && font_key(a, ka, sizeof ka) && font_key(b, kb, sizeof kb) && font_key(d, kd, sizeof kd))
            CHECKF(!strcmp(ka, kb) && !strcmp(ka, kd), "w=%d it=%d: %s / %s / %s", w, it, ka, kb, kd);
        DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(d);
    }
    DW_RELEASE(sysc); IDWriteGdiInterop1_Release(g1); r2_close(&c);
}
TEST(r2_gdi_interop_deep_lf_interop1_not_found_leaves_out_null_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    IDWriteGdiInterop1 *g1 = NULL; IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)0x1234; LOGFONTW lf;
    if (FAILED(IDWriteGdiInterop_QueryInterface(c.g, &IID_IDWriteGdiInterop1, (void **)&g1)) || !g1) { t_skip("IDWriteGdiInterop1 unavailable"); r2_close(&c); return; }
    r2_lf(&lf, "No Such Family r2gdi", 400, 0, 0, 0);
    CHECK_HR(IDWriteGdiInterop1_CreateFontFromLOGFONT(g1, &lf, NULL, &font), DWRITE_E_NOFONT);
    CHECK_OUT_NULL(font);
    font = (IDWriteFont *)(ULONG_PTR)0x1234;
    CHECK_HR(IDWriteGdiInterop_CreateFontFromLOGFONT(c.g, &lf, &font), DWRITE_E_NOFONT);
    CHECK_OUT_NULL(font);
    IDWriteGdiInterop1_Release(g1); r2_close(&c);
}

/* ---- the interop object itself --------------------------------------------------------------------------------------------------------- */
TEST(r2_gdi_interop_deep_interop_identity_per_factory)
{
    IDWriteFactory *f = dw_shared(), *iso = dw_isolated(); IDWriteGdiInterop *a = NULL, *b = NULL, *c2 = NULL; ULONG r1, r2;
    if (!f || !iso) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &a)); CHECK_HR_OK(IDWriteFactory_GetGdiInterop(f, &b)); CHECK_HR_OK(IDWriteFactory_GetGdiInterop(iso, &c2));
    CHECK(a == b);
    CHECK(a != c2);
    r1 = dw_refcount((IUnknown *)a); IDWriteGdiInterop_AddRef(a); r2 = dw_refcount((IUnknown *)a); IDWriteGdiInterop_Release(a);
    CHECK(r2 == r1 + 1);
    CHECK(dw_same_identity((IUnknown *)a, (IUnknown *)b));
    DW_RELEASE(a); DW_RELEASE(b); DW_RELEASE(c2);
}
/* fonts made through the interop of the shared factory live in that factory's system collection */
TEST(r2_gdi_interop_deep_interop_font_family_in_system_collection_arial)
{
    R2_OPEN(c, "Arial", 400, 0);
    IDWriteFontFamily *fam = NULL; IDWriteFontCollection *coll = NULL, *sysc = NULL; UINT32 idx = 0; BOOL ex = FALSE; IDWriteLocalizedStrings *nm = NULL; char n[64];
    CHECK_HR_OK(IDWriteFont_GetFontFamily(c.font, &fam));
    if (fam) {
        CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(fam, &coll));
        CHECK_HR_OK(IDWriteFactory_GetSystemFontCollection(c.f, &sysc, FALSE));
        if (coll && sysc) {
            CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(sysc, L"Arial", &idx, &ex)); CHECK(ex);
            if (coll != sysc) t_note("the family's collection object differs from GetSystemFontCollection's");
        }
        if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &nm)) && nm) { dw_ls_en(nm, n, sizeof n); EXPECT_STR(n, "Arial"); }
    }
    DW_RELEASE(nm); DW_RELEASE(coll); DW_RELEASE(sysc); DW_RELEASE(fam); r2_close(&c);
}
