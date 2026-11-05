#include "e7.h"

static uint8_t *face_blob(const char *family, const e7_face *fc, int i, size_t *n)
{
    ttf_spec s; char uniq[160];
    ttf_style_spec(&s, family, fc->weight, 5, fc->italic);
    snprintf(uniq, sizeof uniq, "%s;e7;%d", family, i);
    ttf_add_name(&s, 3, 1, 0x409, 3, uniq);
    return ttf_build(&s, n);
}

int e7_fam_open(e7_fam *m, const char *family, const e7_face *faces, int n)
{
    int i; HRESULT hr; WCHAR wfam[96]; UINT32 idx = 0; BOOL ex = FALSE;
    memset(m, 0, sizeof *m);
    if (!dw_proc()) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    m->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!m->f) { CHECKF(0, "isolated factory"); return 0; }
    m->fl = dwx_loader_new();
    if (FAILED(dwx_loader_register(m->fl, m->f))) { CHECKF(0, "RegisterFontFileLoader"); goto fail; }
    m->cl = dwx_coll_new(m->fl);
    for (i = 0; i < n; i++) {
        size_t sz; uint8_t *b = face_blob(family, &faces[i], i, &sz);
        UINT32 k = b ? dwx_loader_add(m->fl, b, sz) : 0xFFFFFFFFu;
        free(b);
        if (k != (UINT32)i) { CHECKF(0, "blob %d key %u", i, k); goto fail; }
        dwx_coll_add_key(m->cl, k);
    }
    if (FAILED(dwx_coll_register(m->cl, m->f))) { CHECKF(0, "RegisterFontCollectionLoader"); goto fail; }
    hr = dwx_coll_create(m->cl, m->f, &m->coll);
    if (FAILED(hr) || !m->coll) { CHECKF(0, "CreateCustomFontCollection %s", dw_hr_name(hr)); goto fail; }
    dw_wide(family, wfam, 96);
    hr = IDWriteFontCollection_FindFamilyName(m->coll, wfam, &idx, &ex);
    if (FAILED(hr) || !ex) { CHECKF(0, "FindFamilyName(%s) %s ex=%d", family, dw_hr_name(hr), ex); goto fail; }
    if (FAILED(IDWriteFontCollection_GetFontFamily(m->coll, idx, &m->fam)) || !m->fam) { CHECKF(0, "GetFontFamily"); goto fail; }
    m->count = IDWriteFontFamily_GetFontCount(m->fam);
    if (m->count > 16) { CHECKF(0, "family font count %u", m->count); goto fail; }
    for (i = 0; i < (int)m->count; i++) {
        IDWriteFont *font = NULL;
        if (FAILED(IDWriteFontFamily_GetFont(m->fam, i, &font)) || !font) { CHECKF(0, "GetFont(%d)", i); goto fail; }
        m->w[i] = (int)IDWriteFont_GetWeight(font); m->st[i] = (int)IDWriteFont_GetStyle(font); m->sims[i] = (int)IDWriteFont_GetSimulations(font);
        IDWriteFont_Release(font);
    }
    return 1;
fail:
    e7_fam_close(m);
    return 0;
}
void e7_fam_close(e7_fam *m)
{
    DW_RELEASE(m->fam); DW_RELEASE(m->coll);
    if (m->cl) dwx_coll_unregister(m->cl);
    if (m->fl) dwx_loader_unregister(m->fl);
    if (m->cl) { dwx_release(m->cl); m->cl = NULL; }
    if (m->fl) { dwx_release(m->fl); m->fl = NULL; }
    DW_RELEASE(m->f);
}
int e7_fam_has(const e7_fam *m, int w, int st, int sims)
{
    UINT32 i; int n = 0;
    for (i = 0; i < m->count; i++) if (m->w[i] == w && m->st[i] == st && m->sims[i] == sims) n++;
    return n;
}

int e7_lay_open(e7_lay *m, float size)
{
    size_t sz; uint8_t *b; ttf_spec s; HRESULT hr; UINT32 k;
    memset(m, 0, sizeof *m);
    if (!dw_proc()) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    m->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!m->f) { CHECKF(0, "isolated factory"); return 0; }
    m->fl = dwx_loader_new();
    if (FAILED(dwx_loader_register(m->fl, m->f))) goto fail;
    ttf_style_spec(&s, "E7Lay", 400, 5, 0);
    b = ttf_build(&s, &sz);
    k = b ? dwx_loader_add(m->fl, b, sz) : 0xFFFFFFFFu; free(b);
    if (k != 0) goto fail;
    m->cl = dwx_coll_new(m->fl); dwx_coll_add_key(m->cl, k);
    if (FAILED(dwx_coll_register(m->cl, m->f))) goto fail;
    hr = dwx_coll_create(m->cl, m->f, &m->coll);
    if (FAILED(hr) || !m->coll) goto fail;
    hr = IDWriteFactory_CreateTextFormat(m->f, L"E7Lay", m->coll, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &m->fmt);
    if (FAILED(hr) || !m->fmt) goto fail;
    return 1;
fail:
    e7_lay_close(m);
    CHECKF(0, "layout fixture could not be built");
    return 0;
}
void e7_lay_close(e7_lay *m)
{
    DW_RELEASE(m->fmt); DW_RELEASE(m->coll);
    if (m->cl) dwx_coll_unregister(m->cl);
    if (m->fl) dwx_loader_unregister(m->fl);
    if (m->cl) { dwx_release(m->cl); m->cl = NULL; }
    if (m->fl) { dwx_release(m->fl); m->fl = NULL; }
    DW_RELEASE(m->f);
}
IDWriteTextLayout *e7_layout(e7_lay *m, const WCHAR *text, float w, float h)
{
    IDWriteTextLayout *l = NULL;
    HRESULT hr = IDWriteFactory_CreateTextLayout(m->f, text, (UINT32)lstrlenW(text), m->fmt, w, h, &l);
    CHECK_HR(hr, S_OK);
    return l;
}
