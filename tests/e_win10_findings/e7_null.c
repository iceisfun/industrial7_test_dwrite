/* NULL arguments on Windows 10: many entry points do not validate pointers. They fault (0xC0000005) or throw a C++ exception
 * (0xE06D7363); the SEH helper t_try traps either. Entry points that do validate return E_INVALIDARG. One group per behaviour,
 * each case on fresh objects. */
#include "e7.h"

#define EXC_AV  0xC0000005u
#define EXC_CPP 0xE06D7363u
typedef struct { int id; HRESULT hr; IDWriteFactory *f; IDWriteTextFormat *tf; IDWriteTextLayout *tl; IDWriteFontFace *face; IDWriteFontCollection *c; IDWriteLocalizedStrings *ls; IDWriteTextAnalyzer *an; IDWriteFont *fo; } nc;

enum { N_FMT_LOCALE = 1, N_FMT_FAMILY, N_LAY_LOCALE, N_FMT_OUT, N_LAY_TEXT, N_GLYPHIDX, N_DESIGNMET, N_FINDFAM_NAME, N_LS_GETSTRING, N_ANALYZE, N_FMT_COLL,
       N_FACE_METRICS, N_SYSCOLL, N_FINDFAM_IDX, N_HASCHAR, N_LS_LEN, N_LS_FIND, N_HIT, N_LAY_FMT, N_GLYPHIDX_ZERO, N_LAY_METRICS, N_CLUSTER, N_LINE, N_GLYPHIDX_OUT, N_VALID };
static void body(void *vp)
{
    nc *p = (nc *)vp; UINT32 u = 0; DWRITE_TEXT_RANGE r; UINT16 g[2]; BOOL b; WCHAR w[16]; UINT32 cp = 'A';
    switch (p->id) {
    case N_FMT_LOCALE:   p->hr = IDWriteTextFormat_GetLocaleName(p->tf, NULL, 10); break;
    case N_FMT_FAMILY:   p->hr = IDWriteTextFormat_GetFontFamilyName(p->tf, NULL, 10); break;
    case N_LAY_LOCALE:   p->hr = IDWriteTextLayout_GetLocaleName(p->tl, 0, NULL, 10, &r); break;
    case N_FMT_OUT:      p->hr = IDWriteFactory_CreateTextFormat(p->f, L"Arial", NULL, 400, 0, 5, 12.f, L"en-us", NULL); break;
    case N_LAY_TEXT:     p->hr = IDWriteFactory_CreateTextLayout(p->f, NULL, 3, p->tf, 100, 100, &p->tl); break;
    case N_GLYPHIDX:     p->hr = IDWriteFontFace_GetGlyphIndices(p->face, NULL, 1, g); break;
    case N_DESIGNMET:    p->hr = IDWriteFontFace_GetDesignGlyphMetrics(p->face, NULL, 1, NULL, FALSE); break;
    case N_FINDFAM_NAME: p->hr = IDWriteFontCollection_FindFamilyName(p->c, NULL, &u, &b); break;
    case N_LS_GETSTRING: p->hr = IDWriteLocalizedStrings_GetString(p->ls, 0, NULL, 5); break;
    case N_ANALYZE:      p->hr = IDWriteTextAnalyzer_AnalyzeScript(p->an, NULL, 0, 1, NULL); break;
    case N_FMT_COLL:     p->hr = IDWriteTextFormat_GetFontCollection(p->tf, NULL); break;
    case N_FACE_METRICS: IDWriteFontFace_GetMetrics(p->face, NULL); p->hr = S_OK; break;
    case N_SYSCOLL:      p->hr = IDWriteFactory_GetSystemFontCollection(p->f, NULL, FALSE); break;
    case N_FINDFAM_IDX:  p->hr = IDWriteFontCollection_FindFamilyName(p->c, L"Arial", NULL, &b); break;
    case N_HASCHAR:      p->hr = IDWriteFont_HasCharacter(p->fo, 'A', NULL); break;
    case N_LS_LEN:       p->hr = IDWriteLocalizedStrings_GetStringLength(p->ls, 0, NULL); break;
    case N_LS_FIND:      p->hr = IDWriteLocalizedStrings_FindLocaleName(p->ls, NULL, &u, &b); break;
    case N_HIT:          p->hr = IDWriteTextLayout_HitTestTextPosition(p->tl, 0, FALSE, NULL, NULL, NULL); break;
    case N_LAY_FMT:      p->hr = IDWriteFactory_CreateTextLayout(p->f, L"abc", 3, NULL, 100, 100, &p->tl); break;
    case N_GLYPHIDX_ZERO:p->hr = IDWriteFontFace_GetGlyphIndices(p->face, NULL, 0, NULL); break;
    case N_LAY_METRICS:  p->hr = IDWriteTextLayout_GetMetrics(p->tl, NULL); break;
    case N_CLUSTER:      p->hr = IDWriteTextLayout_GetClusterMetrics(p->tl, NULL, 5, &u); break;
    case N_LINE:         p->hr = IDWriteTextLayout_GetLineMetrics(p->tl, NULL, 5, &u); break;
    case N_GLYPHIDX_OUT: p->hr = IDWriteFontFace_GetGlyphIndices(p->face, &cp, 1, NULL); break;
    case N_VALID:        /* control: the same calls with valid pointers */
        p->hr = IDWriteTextFormat_GetLocaleName(p->tf, w, 16);
        if (SUCCEEDED(p->hr)) p->hr = IDWriteTextFormat_GetFontFamilyName(p->tf, w, 16);
        if (SUCCEEDED(p->hr)) p->hr = IDWriteTextLayout_GetMetrics(p->tl, &(DWRITE_TEXT_METRICS){ 0 });
        if (SUCCEEDED(p->hr)) p->hr = IDWriteFontFace_GetGlyphIndices(p->face, &cp, 1, g);
        break;
    }
}
/* runs one case on fresh objects; returns the exception code (0 = none) and the HRESULT in *hr */
static DWORD run_case(int id, HRESULT *hr, int *have)
{
    IDWriteFactory *f = dw_shared(); nc p; UINT32 idx; BOOL ex; DWORD code;
    memset(&p, 0, sizeof p); p.id = id; p.hr = 0x7fffffff; p.f = f; *have = 0;
    if (!f) return 0;
    IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, 0, 5, 12.f, L"en-us", &p.tf);
    IDWriteFactory_CreateTextLayout(f, L"abc", 3, p.tf, 100, 100, &p.tl);
    IDWriteFactory_GetSystemFontCollection(f, &p.c, FALSE);
    if (p.c && SUCCEEDED(IDWriteFontCollection_FindFamilyName(p.c, L"Arial", &idx, &ex)) && ex) {
        IDWriteFontFamily *fam = NULL;
        IDWriteFontCollection_GetFontFamily(p.c, idx, &fam);
        if (fam) { IDWriteFontFamily_GetFont(fam, 0, &p.fo); IDWriteFontFamily_GetFamilyNames(fam, &p.ls); }
        if (p.fo) IDWriteFont_CreateFontFace(p.fo, &p.face);
        DW_RELEASE(fam);
    }
    IDWriteFactory_CreateTextAnalyzer(f, &p.an);
    if (p.tf && p.tl && p.c && p.fo && p.face && p.ls && p.an) *have = 1;
    code = *have ? t_try(body, &p) : 0;
    *hr = p.hr;
    DW_RELEASE(p.an); DW_RELEASE(p.ls); DW_RELEASE(p.face); DW_RELEASE(p.fo); DW_RELEASE(p.c); DW_RELEASE(p.tl); DW_RELEASE(p.tf);
    return code;
}
static void expect_exc(int id, DWORD want, const char *what)
{
    HRESULT hr; int have; DWORD code = run_case(id, &hr, &have);
    if (!have) { t_skip("stock objects unavailable"); return; }
    CHECKF(code == want, "%s: exception 0x%08lx, want 0x%08lx", what, (unsigned long)code, (unsigned long)want);
}
static void expect_hr(int id, HRESULT want, const char *what)
{
    HRESULT hr; int have; DWORD code = run_case(id, &hr, &have);
    if (!have) { t_skip("stock objects unavailable"); return; }
    CHECKF(code == 0, "%s: unexpected exception 0x%08lx", what, (unsigned long)code);
    CHECKF(hr == want, "%s: got %s want %s", what, dw_hr_name(hr), dw_hr_name(want));
}
TEST(e7_null_text_format_name_buffers_throw_cpp_exception)
{
    expect_exc(N_FMT_LOCALE, EXC_CPP, "TextFormat::GetLocaleName(NULL)");
    expect_exc(N_FMT_FAMILY, EXC_CPP, "TextFormat::GetFontFamilyName(NULL)");
}
TEST(e7_null_pointers_fault_with_access_violation)
{
    static const struct { int id; const char *what; } cases[] = {
        { N_LAY_LOCALE, "TextLayout::GetLocaleName(NULL)" }, { N_FMT_OUT, "CreateTextFormat(out NULL)" },
        { N_DESIGNMET, "GetDesignGlyphMetrics(NULL)" }, { N_FINDFAM_NAME, "FindFamilyName(name NULL)" },
        { N_LS_GETSTRING, "LocalizedStrings::GetString(NULL)" }, { N_FMT_COLL, "TextFormat::GetFontCollection(NULL)" },
        { N_FACE_METRICS, "FontFace::GetMetrics(NULL)" }, { N_SYSCOLL, "GetSystemFontCollection(out NULL)" },
        { N_FINDFAM_IDX, "FindFamilyName(index NULL)" }, { N_HASCHAR, "Font::HasCharacter(out NULL)" },
        { N_LS_LEN, "GetStringLength(out NULL)" }, { N_HIT, "HitTestTextPosition(outs NULL)" },
        { N_LAY_METRICS, "TextLayout::GetMetrics(NULL)" }, { N_GLYPHIDX_OUT, "GetGlyphIndices(out NULL)" },
    };
    unsigned i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) expect_exc(cases[i].id, EXC_AV, cases[i].what);
}
TEST(e7_null_validated_arguments_return_hresults)
{
    expect_hr(N_LAY_TEXT, E_INVALIDARG, "CreateTextLayout(text NULL)");
    expect_hr(N_LAY_FMT, E_INVALIDARG, "CreateTextLayout(format NULL)");
    expect_hr(N_GLYPHIDX, E_INVALIDARG, "GetGlyphIndices(codepoints NULL)");
    expect_hr(N_ANALYZE, E_INVALIDARG, "AnalyzeScript(source NULL)");
    expect_hr(N_LS_FIND, E_INVALIDARG, "FindLocaleName(NULL)");
    expect_hr(N_CLUSTER, E_INVALIDARG, "GetClusterMetrics(NULL, 5)");
    expect_hr(N_LINE, E_INVALIDARG, "GetLineMetrics(NULL, 5)");
    expect_hr(N_GLYPHIDX_ZERO, S_OK, "GetGlyphIndices(count 0)");
}
/* negative control: valid pointers neither fault nor throw, and the three failure modes are distinguishable */
TEST(e7_null_negative_control_valid_arguments)
{
    HRESULT hr; int have; DWORD code = run_case(N_VALID, &hr, &have);
    if (!have) { t_skip("stock objects unavailable"); return; }
    EXPECT_EQ(code, 0); CHECK_HR(hr, S_OK);
    CHECK(EXC_AV != EXC_CPP);
    CHECK(run_case(N_FMT_LOCALE, &hr, &have) != run_case(N_FMT_COLL, &hr, &have));   /* a throw is not a fault */
}
