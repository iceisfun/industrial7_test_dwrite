/* Property table for the generic range-semantics tests. */
#include "tlr.h"

static int get_hr(HRESULT hr, const char *what) { t_check(hr == S_OK, __FILE__, __LINE__, what, "got %s", dw_hr_name(hr)); return hr == S_OK; }

/* weight */
static const DWRITE_FONT_WEIGHT k_w[3] = { DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_WEIGHT_LIGHT };
static HRESULT w_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontWeight(e->l, k_w[v], r); }
static int w_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { DWRITE_FONT_WEIGHT w = tlr_weight(e->l, pos, r); int i; for (i = 0; i < 3; i++) if (w == k_w[i]) return i; return -1; }
/* style */
static const DWRITE_FONT_STYLE k_s[3] = { DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STYLE_OBLIQUE };
static HRESULT s_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontStyle(e->l, k_s[v], r); }
static int s_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { DWRITE_FONT_STYLE w = tlr_style(e->l, pos, r); int i; for (i = 0; i < 3; i++) if (w == k_s[i]) return i; return -1; }
/* stretch */
static const DWRITE_FONT_STRETCH k_t[3] = { DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, DWRITE_FONT_STRETCH_EXPANDED };
static HRESULT t_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontStretch(e->l, k_t[v], r); }
static int t_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { DWRITE_FONT_STRETCH w = tlr_stretch(e->l, pos, r); int i; for (i = 0; i < 3; i++) if (w == k_t[i]) return i; return -1; }
/* size */
static const float k_z[3] = { 16.0f, 24.0f, 8.5f };
static HRESULT z_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontSize(e->l, k_z[v], r); }
static int z_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { float w = tlr_size(e->l, pos, r); int i; for (i = 0; i < 3; i++) if (tlr_fbits(w, k_z[i])) return i; return -1; }
/* underline */
static HRESULT u_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetUnderline(e->l, v != 0, r); }
static int u_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { return tlr_underline(e->l, pos, r) ? 1 : 0; }
/* strikethrough */
static HRESULT k_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetStrikethrough(e->l, v != 0, r); }
static int k_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { return tlr_strike(e->l, pos, r) ? 1 : 0; }
/* family */
static const WCHAR *const k_f[3] = { TLR_FAM, TLR_NARROW, TLR_WIDE };
static HRESULT f_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontFamilyName(e->l, k_f[v], r); }
static int f_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { const WCHAR *w = tlr_family(e->l, pos, r); int i; for (i = 0; i < 3; i++) if (!wcscmp(w, k_f[i])) return i; return -1; }
/* locale (lower case tags: casing normalisation does not matter) */
static const WCHAR *const k_l[3] = { L"en-us", L"fr-fr", L"de-de" };
static HRESULT l_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetLocaleName(e->l, k_l[v], r); }
static int l_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { const WCHAR *w = tlr_locale(e->l, pos, r); int i; for (i = 0; i < 3; i++) if (!_wcsicmp(w, k_l[i])) return i; return -1; }
/* drawing effect */
static HRESULT e_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetDrawingEffect(e->l, v ? (IUnknown *)e->eff[v] : NULL, r); }
static int e_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { IUnknown *u = tlr_effect(e->l, pos, r); int i, v = -1; for (i = 0; i < 3; i++) if (u == (i ? (IUnknown *)e->eff[i] : NULL)) v = i; DW_RELEASE(u); return v; }
/* inline object */
static HRESULT i_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetInlineObject(e->l, v ? (IDWriteInlineObject *)e->inl[v] : NULL, r); }
static int i_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { IDWriteInlineObject *u = tlr_inline(e->l, pos, r); int i, v = -1; for (i = 0; i < 3; i++) if (u == (i ? (IDWriteInlineObject *)e->inl[i] : NULL)) v = i; DW_RELEASE(u); return v; }
/* typography */
static HRESULT y_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetTypography(e->l, v ? e->typo[v] : NULL, r); }
static int y_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { IDWriteTypography *u = tlr_typo(e->l, pos, r); int i, v = -1; for (i = 0; i < 3; i++) if (u == (i ? e->typo[i] : NULL)) v = i; DW_RELEASE(u); return v; }
/* font collection (0 = the collection of the format, 1 = a second custom instance, 2 = the system collection) */
static HRESULT c_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return IDWriteTextLayout_SetFontCollection(e->l, e->coll[v], r); }
static int c_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r) { IDWriteFontCollection *u = tlr_coll(e->l, pos, r); int i, v = -1; for (i = 0; i < 3; i++) if (u == e->coll[i]) v = i; DW_RELEASE(u); return v; }
/* character spacing (leading, trailing); minimum advance fixed at 0 */
static const float k_cs[3][2] = { { 0, 0 }, { 1.5f, 0 }, { 0, 2.5f } };
static HRESULT cs_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return e->l1 ? IDWriteTextLayout1_SetCharacterSpacing(e->l1, k_cs[v][0], k_cs[v][1], 0.0f, r) : E_NOINTERFACE; }
static int cs_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r)
{
    float a = 7, b = 7, c = 7; int i, v = -1; DWRITE_TEXT_RANGE rr = POISON_RANGE;
    if (!e->l1) return -1;
    get_hr(IDWriteTextLayout1_GetCharacterSpacing(e->l1, pos, &a, &b, &c, &rr), "GetCharacterSpacing"); if (r) *r = rr;
    for (i = 0; i < 3; i++) if (tlr_fbits(a, k_cs[i][0]) && tlr_fbits(b, k_cs[i][1])) v = i;
    return v;
}
/* pair kerning */
static HRESULT pk_set(tlr_env *e, int v, DWRITE_TEXT_RANGE r) { return e->l1 ? IDWriteTextLayout1_SetPairKerning(e->l1, v != 0, r) : E_NOINTERFACE; }
static int pk_get(tlr_env *e, UINT32 pos, DWRITE_TEXT_RANGE *r)
{
    BOOL k = 0x5a5a; DWRITE_TEXT_RANGE rr = POISON_RANGE;
    if (!e->l1) return -1;
    get_hr(IDWriteTextLayout1_GetPairKerning(e->l1, pos, &k, &rr), "GetPairKerning"); if (r) *r = rr;
    return k ? 1 : 0;
}

const tlr_prop tlr_props[] = {
    { "weight", 3, w_set, w_get },   { "style", 3, s_set, s_get },   { "stretch", 3, t_set, t_get },  { "size", 3, z_set, z_get },
    { "underline", 2, u_set, u_get }, { "strikethrough", 2, k_set, k_get }, { "family", 3, f_set, f_get }, { "locale", 3, l_set, l_get },
    { "effect", 3, e_set, e_get },   { "inline", 3, i_set, i_get },  { "typography", 3, y_set, y_get },  { "collection", 3, c_set, c_get },
    { "charspacing", 3, cs_set, cs_get }, { "pairkerning", 2, pk_set, pk_get },
};
const int tlr_nprops = (int)(sizeof tlr_props / sizeof tlr_props[0]);

const WCHAR *tlr_cur_text = L"";
int tlr_env_layout(tlr_env *e, const WCHAR *text)
{
    DW_RELEASE(e->l1); DW_RELEASE(e->l);
    e->l = tlr_layout_(&e->x, TLR_FAM, text, 16.0f);
    if (!e->l) return 0;
    IDWriteTextLayout_QueryInterface(e->l, &IID_IDWriteTextLayout1, (void **)&e->l1);
    return 1;
}

int tlr_env_open(tlr_env *e)
{
    int i;
    if (!tlr_open(&e->x)) return 0;
    for (i = 1; i < 3; i++) {
        static const DWRITE_FONT_FEATURE_TAG tags[3] = { 0, DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES };
        DWRITE_FONT_FEATURE ft; ft.nameTag = tags[i]; ft.parameter = 1;
        e->eff[i] = dwx_effect_new(i);
        e->inl[i] = dwx_inline_new(100.0f * i, 20.0f, 15.0f, FALSE);
        if (FAILED(IDWriteFactory_CreateTypography(e->x.f, &e->typo[i]))) return 0;
        IDWriteTypography_AddFontFeature(e->typo[i], ft);
    }
    {
        /* collection 0 = whatever the format reports; 1 = a second instance over the same loader (contains the fixture families); 2 = the system collection */
        IDWriteTextFormat *t = tlr_format(&e->x, TLR_FAM, 16.0f);
        if (!t) return 0;
        IDWriteTextFormat_GetFontCollection(t, &e->coll[0]);
        IDWriteTextFormat_Release(t);
        if (!e->coll[0]) return 0;
        if (FAILED(dwx_coll_create(e->x.cl, e->x.f, &e->coll[1]))) return 0;
        if (FAILED(IDWriteFactory_GetSystemFontCollection(e->x.f, &e->coll[2], FALSE))) return 0;
    }
    return 1;
}

void tlr_env_close(tlr_env *e)
{
    int i;
    DW_RELEASE(e->l1); DW_RELEASE(e->l);
    for (i = 0; i < 3; i++) { DW_RELEASE(e->typo[i]); DW_RELEASE(e->coll[i]); if (e->eff[i]) dwx_release(e->eff[i]); if (e->inl[i]) dwx_release(e->inl[i]); }
    tlr_close(&e->x);
}
