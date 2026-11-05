#include "fm.h"

static int g_notes;
static void fm_note(const char *fmt, const char *a, int b, int c) {
    if (g_notes++ < 24) t_note(fmt, a, b, c);
}

/* ---- names / blobs --------------------------------------------------------------------------- */
static const char *st_letter(int st) { return st == 2 ? "I" : st == 1 ? "O" : "N"; }

static uint8_t *fm_blob(const char *family, const fm_face *f, int i, fm_tweak tw, size_t *n) {
    ttf_spec s; char style[64], id1[160], full[160], ps[160], uniq[200];
    ttf_style_spec(&s, family, 400, 5, 0);
    s.weight = f->w; s.width = f->wd;
    s.os2_version = 4;
    s.fs_selection = f->st == 2 ? 0x01 : f->st == 1 ? 0x200 : 0x40;
    s.mac_style = (f->w >= 700 ? 1 : 0) | (f->st ? 2 : 0);
    s.italic_angle_fixed = f->st ? -12 * 65536 : 0;
    ttf_clear_names(&s);
    snprintf(style, sizeof style, "W%dS%d%s", f->w, f->wd, st_letter(f->st));
    /* Real-world naming: RIBBI faces (400/700, normal width, normal/italic) use the legacy family only; every other face carries
     * "<family> <style>" as legacy family name plus the typographic family (16) and subfamily (17). */
    {
        int ribbi = (f->w == 400 || f->w == 700) && f->wd == 5 && f->st != 1;
        const char *sub = f->st == 2 ? (f->w == 700 && ribbi ? "Bold Italic" : "Italic") : (f->w == 700 && ribbi ? "Bold" : "Regular");
        snprintf(id1, sizeof id1, ribbi ? "%s" : "%s %s", family, style);
        snprintf(full, sizeof full, "%s %s", family, style);
        snprintf(ps, sizeof ps, "%s-%s", family, style);
        snprintf(uniq, sizeof uniq, "%s;%s;%d", family, style, i);
        ttf_add_name(&s, 3, 1, 0x409, 1, id1);  ttf_add_name(&s, 3, 1, 0x409, 2, sub);
        ttf_add_name(&s, 3, 1, 0x409, 3, uniq); ttf_add_name(&s, 3, 1, 0x409, 4, full);
        ttf_add_name(&s, 3, 1, 0x409, 6, ps);
        ttf_add_name(&s, 1, 0, 0, 1, id1); ttf_add_name(&s, 1, 0, 0, 2, sub);
        ttf_add_name(&s, 1, 0, 0, 4, full); ttf_add_name(&s, 1, 0, 0, 6, ps);
        if (!ribbi) { ttf_add_name(&s, 3, 1, 0x409, 16, family); ttf_add_name(&s, 3, 1, 0x409, 17, style); }
    }
    if (tw) tw(&s, i);
    return ttf_build(&s, n);
}

int fm_read_font(IDWriteFont *font, fm_face *o, int *sims) {
    if (!font) return 0;
    o->w = (int)IDWriteFont_GetWeight(font);
    o->wd = (int)IDWriteFont_GetStretch(font);
    o->st = (int)IDWriteFont_GetStyle(font);
    *sims = (int)IDWriteFont_GetSimulations(font);
    return 1;
}

int fm_open(fm *m, const char *family, const fm_face *faces, int n, fm_tweak tw) {
    int i; HRESULT hr; WCHAR wfam[96]; UINT32 idx = 0; BOOL ex = FALSE;
    memset(m, 0, sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", family);
    if (!dw_proc()) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    if (n < 1 || n > FM_MAXF) { CHECKF(0, "bad face count %d", n); return 0; }
    m->n = n;
    memcpy(m->spec, faces, sizeof(fm_face) * n);
    m->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!m->f) { CHECKF(0, "isolated factory not created"); return 0; }
    m->fl = dwx_loader_new();
    hr = dwx_loader_register(m->fl, m->f);
    if (FAILED(hr)) { CHECKF(0, "RegisterFontFileLoader %s", dw_hr_name(hr)); goto fail; }
    m->cl = dwx_coll_new(m->fl);
    for (i = 0; i < n; i++) {
        size_t sz; uint8_t *b = fm_blob(family, &faces[i], i, tw, &sz);
        UINT32 k = b ? dwx_loader_add(m->fl, b, sz) : 0xFFFFFFFFu;
        free(b);
        if (k != (UINT32)i) { CHECKF(0, "blob %d key %u", i, k); goto fail; }
        dwx_coll_add_key(m->cl, k);
    }
    hr = dwx_coll_register(m->cl, m->f);
    if (FAILED(hr)) { CHECKF(0, "RegisterFontCollectionLoader %s", dw_hr_name(hr)); goto fail; }
    hr = dwx_coll_create(m->cl, m->f, &m->coll);
    if (FAILED(hr) || !m->coll) { CHECKF(0, "CreateCustomFontCollection %s", dw_hr_name(hr)); goto fail; }
    dw_wide(family, wfam, 96);
    hr = IDWriteFontCollection_FindFamilyName(m->coll, wfam, &idx, &ex);
    if (FAILED(hr) || !ex) {
        UINT32 k, nf = IDWriteFontCollection_GetFontFamilyCount(m->coll);
        CHECKF(0, "FindFamilyName(%s) %s exists=%d count=%u", family, dw_hr_name(hr), ex, nf);
        for (k = 0; k < nf && k < 4; k++) {
            IDWriteFontFamily *ff = NULL; IDWriteLocalizedStrings *ls = NULL; char nm[96] = "?";
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(m->coll, k, &ff)) && ff && SUCCEEDED(IDWriteFontFamily_GetFamilyNames(ff, &ls)) && ls) dw_ls_en(ls, nm, sizeof nm);
            t_note("collection family %u: '%s' fonts=%u", k, nm, ff ? IDWriteFontFamily_GetFontCount(ff) : 0);
            DW_RELEASE(ls); DW_RELEASE(ff);
        }
        goto fail;
    }
    hr = IDWriteFontCollection_GetFontFamily(m->coll, idx, &m->fam);
    if (FAILED(hr) || !m->fam) { CHECKF(0, "GetFontFamily %s", dw_hr_name(hr)); goto fail; }
    m->count = IDWriteFontFamily_GetFontCount(m->fam);
    if (m->count > FM_MAXF) { CHECKF(0, "family font count %u", m->count); goto fail; }
    for (i = 0; i < (int)m->count; i++) {
        IDWriteFont *font = NULL; int k;
        hr = IDWriteFontFamily_GetFont(m->fam, i, &font);
        if (FAILED(hr) || !font) { CHECKF(0, "GetFont(%d) %s", i, dw_hr_name(hr)); goto fail; }
        fm_read_font(font, &m->obs[i], &m->obs_sims[i]);
        m->src[i] = -1;
        for (k = 0; k < n; k++) if (faces[k].w == m->obs[i].w && faces[k].wd == m->obs[i].wd && faces[k].st == m->obs[i].st) m->src[i] = k;
        IDWriteFont_Release(font);
    }
    return 1;
fail:
    fm_close(m);
    return 0;
}

void fm_close(fm *m) {
    DW_RELEASE(m->fam);
    DW_RELEASE(m->coll);
    if (m->cl) { dwx_coll_unregister(m->cl); dwx_release(m->cl); m->cl = NULL; }
    if (m->fl) { dwx_loader_unregister(m->fl); dwx_release(m->fl); m->fl = NULL; }
    if (m->f) { IDWriteFactory_Release(m->f); m->f = NULL; }
}

int fm_find(const fm *m, int w, int wd, int st) {
    int i;
    for (i = 0; i < (int)m->count; i++) if (m->obs[i].w == w && m->obs[i].wd == wd && m->obs[i].st == st) return i;
    return -1;
}
const char *fm_req_name(int w, int wd, int st) {
    static char b[4][64]; static int k;
    char *o = b[k++ & 3];
    snprintf(o, 64, "(w=%d,wd=%d,st=%d)", w, wd, st);
    return o;
}

/* ---- checks ----------------------------------------------------------------------------------- */
void fm_check_font(fm *m, IDWriteFont *font, const fm_face *face, int sims, const char *tag) {
    fm_face got; int gs; fm_face want = fm_reported(face, sims);
    IDWriteFontFamily *ff = NULL;
    if (!font) { CHECKF(0, "%s: no font", tag); return; }
    fm_read_font(font, &got, &gs);
    CHECKF(got.w == want.w && got.wd == want.wd && got.st == want.st && gs == sims,
           "%s: got w=%d wd=%d st=%d sims=%d want w=%d wd=%d st=%d sims=%d", tag, got.w, got.wd, got.st, gs, want.w, want.wd, want.st, sims);
    if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &ff)) && ff) {
        CHECKF(dw_same_identity((IUnknown *)ff, (IUnknown *)m->fam), "%s: GetFontFamily is not the family the font came from", tag);
        IDWriteFontFamily_Release(ff);
    } else CHECKF(0, "%s: GetFontFamily failed", tag);
}

void fm_expect_first(fm *m, int rw, int rwd, int rst, int fam_idx, int sims) {
    IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1; HRESULT hr; char tag[96];
    font = NULL;
    snprintf(tag, sizeof tag, "first%s", fm_req_name(rw, rwd, rst));
    hr = IDWriteFontFamily_GetFirstMatchingFont(m->fam, (DWRITE_FONT_WEIGHT)rw, (DWRITE_FONT_STRETCH)rwd, (DWRITE_FONT_STYLE)rst, &font);
    CHECKF(hr == S_OK, "%s hr %s", tag, dw_hr_name(hr));
    if (hr == S_OK && fam_idx >= 0 && fam_idx < (int)m->count) fm_check_font(m, font, &m->obs[fam_idx], sims, tag);
    else CHECKF(fam_idx >= 0, "%s: expected face missing from family", tag);
    DW_RELEASE(font);
}

void fm_check_request(fm *m, int rw, int rwd, int rst, int full_list) {
    fm_pick p = fm_oracle_first(m->obs, (int)m->count, rw, rwd, rst);
    fm_pick lp[FM_MAXF]; int ln, i; UINT32 cnt;
    IDWriteFontList *list = NULL; HRESULT hr;
    char tag[96];
    int ft = fm_oracle_first_tied(m->obs, (int)m->count, rw, rwd, rst), lt = fm_oracle_list_tied(m->obs, (int)m->count, rw, rwd, rst);
    if (p.idx >= 0 && !ft) fm_expect_first(m, rw, rwd, rst, p.idx, p.sims);
    else if (p.idx >= 0) { IDWriteFont *fo = NULL; CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m->fam, (DWRITE_FONT_WEIGHT)rw, (DWRITE_FONT_STRETCH)rwd, (DWRITE_FONT_STYLE)rst, &fo)); CHECK(fo != NULL); DW_RELEASE(fo); }
    ln = fm_oracle_list(m->obs, (int)m->count, rw, rwd, rst, lp);
    hr = IDWriteFontFamily_GetMatchingFonts(m->fam, (DWRITE_FONT_WEIGHT)rw, (DWRITE_FONT_STRETCH)rwd, (DWRITE_FONT_STYLE)rst, &list);
    snprintf(tag, sizeof tag, "list%s", fm_req_name(rw, rwd, rst));
    CHECKF(hr == S_OK && list, "%s hr %s", tag, dw_hr_name(hr));
    if (hr != S_OK || !list) return;
    cnt = IDWriteFontList_GetFontCount(list);
    CHECKF(cnt >= 1 && cnt <= m->count, "%s: count %u of %u", tag, cnt, m->count);
    if (cnt != (UINT32)ln) fm_note("list%s: %d of %d fonts returned", fm_req_name(rw, rwd, rst), (int)cnt, ln);
    for (i = 0; i < (int)cnt && i < ln; i++) {
        IDWriteFont *font = NULL;
        if (lt || ft) break;
        if (!full_list && i > 0) break;
        if (cnt != (UINT32)ln && i > 0) break;
        hr = IDWriteFontList_GetFont(list, i, &font);
        CHECKF(hr == S_OK, "%s[%d] hr %s", tag, i, dw_hr_name(hr));
        if (hr == S_OK) { char t2[120]; snprintf(t2, sizeof t2, "%s[%d]", tag, i); fm_check_font(m, font, &m->obs[lp[i].idx], lp[i].sims, t2); }
        DW_RELEASE(font);
    }
    IDWriteFontList_Release(list);
}
