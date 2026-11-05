/* Unsupported IIDs on Windows 10: font FACES (system and custom) answer E_NOTIMPL, every other object answers E_NOINTERFACE;
 * both leave the out pointer NULL. */
#include "e7.h"

static const GUID BOGUS = { 0x7e57c0de, 0x1234, 0x4321, { 9, 8, 7, 6, 5, 4, 3, 2 } };

static HRESULT qi_bogus(void *o, void **out)
{
    *out = (void *)(ULONG_PTR)1;
    return IUnknown_QueryInterface((IUnknown *)o, &BOGUS, out);
}
static void expect_qi(void *o, HRESULT want, const char *what, REFIID foreign)
{
    void *p; HRESULT hr; ULONG before, after;
    if (!o) { t_skip("%s unavailable", what); return; }
    before = dw_refcount((IUnknown *)o);
    hr = qi_bogus(o, &p);
    CHECKF(hr == want, "%s: bogus IID -> %s want %s", what, dw_hr_name(hr), dw_hr_name(want));
    CHECKF(p == NULL, "%s: out pointer %p", what, p);
    after = dw_refcount((IUnknown *)o);
    EXPECT_EQ(before, after);
    /* a real but unrelated interface of another object fails the same way */
    p = (void *)(ULONG_PTR)1;
    hr = IUnknown_QueryInterface((IUnknown *)o, foreign, &p);
    CHECKF(hr == want, "%s: foreign interface -> %s want %s", what, dw_hr_name(hr), dw_hr_name(want));
    CHECK(p == NULL);
}
TEST(e7_qi_font_faces_answer_e_notimpl)
{
    dwx_font h; IDWriteFontFace *sys;
    memset(&h, 0, sizeof h);
    if (!dw_shared()) { t_skip("factory", &IID_IDWriteTextFormat); return; }
    if (SUCCEEDED(dwx_font_open_default(&h, dw_shared())) && h.face) expect_qi(h.face, E_NOTIMPL, "custom face", &IID_IDWriteTextFormat);
    dwx_font_close(&h);
    sys = dw_sysface(dw_shared(), L"Arial", DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (sys) expect_qi(sys, E_NOTIMPL, "system face", &IID_IDWriteTextFormat); else t_skip("Arial not installed");
    DW_RELEASE(sys);
}
TEST(e7_qi_other_objects_answer_e_nointerface)
{
    IDWriteFactory *f = dw_shared(); IDWriteFontCollection *c = NULL; IDWriteTextFormat *tf = NULL; IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL;
    IDWriteFontFile *ff = NULL; dwx_font h; UINT32 idx; BOOL ex;
    if (!f) { t_skip("factory", &IID_IDWriteTextFormat); return; }
    expect_qi(f, E_NOINTERFACE, "factory", &IID_IDWriteTextFormat);
    CHECK_HR(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE), S_OK);
    expect_qi(c, E_NOINTERFACE, "collection", &IID_IDWriteTextFormat);
    if (c && SUCCEEDED(IDWriteFontCollection_FindFamilyName(c, L"Arial", &idx, &ex)) && ex &&
        SUCCEEDED(IDWriteFontCollection_GetFontFamily(c, idx, &fam)) && fam) {
        expect_qi(fam, E_NOINTERFACE, "family", &IID_IDWriteTextFormat);
        if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) && font) expect_qi(font, E_NOINTERFACE, "font", &IID_IDWriteTextFormat);
    }
    CHECK_HR(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, 0, 5, 12.f, L"en-us", &tf), S_OK);
    expect_qi(tf, E_NOINTERFACE, "text format", &IID_IDWriteFontCollection);
    memset(&h, 0, sizeof h);
    if (SUCCEEDED(dwx_font_open_default(&h, f)) && h.file) expect_qi(h.file, E_NOINTERFACE, "font file", &IID_IDWriteTextFormat);
    dwx_font_close(&h);
    DW_RELEASE(ff); DW_RELEASE(font); DW_RELEASE(fam); DW_RELEASE(tf); DW_RELEASE(c);
}
/* negative control: a SUPPORTED interface still succeeds on the same objects, and the two failure codes differ */
TEST(e7_qi_negative_control_supported_and_codes_differ)
{
    IDWriteFactory *f = dw_shared(); dwx_font h; void *p = NULL, *q = NULL; HRESULT hf, hn;
    if (!f) { t_skip("factory", &IID_IDWriteTextFormat); return; }
    memset(&h, 0, sizeof h);
    if (FAILED(dwx_font_open_default(&h, f)) || !h.face) { dwx_font_close(&h); t_skip("face"); return; }
    CHECK_HR(IUnknown_QueryInterface((IUnknown *)h.face, &IID_IDWriteFontFace, &p), S_OK);
    if (p) IUnknown_Release((IUnknown *)p);
    CHECK_HR(IUnknown_QueryInterface((IUnknown *)f, &IID_IDWriteFactory, &q), S_OK);
    if (q) IUnknown_Release((IUnknown *)q);
    hf = qi_bogus(h.face, &p); hn = qi_bogus(f, &q);
    CHECK(hf != hn);
    CHECK(hf == E_NOTIMPL && hn == E_NOINTERFACE);
    dwx_font_close(&h);
}
