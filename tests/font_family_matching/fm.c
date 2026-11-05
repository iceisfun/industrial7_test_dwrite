#include "fm.h"

static int g_notes, g_quiet, g_dumped;
void fm_set_quiet(int q) { g_quiet = q; }
#define FAILF(...) do { if (g_quiet) t_note(__VA_ARGS__); else CHECKF(0, __VA_ARGS__); } while (0)
static void fm_note(const char *fmt, const char *a, int b, int c) {
    if (g_notes++ < 24) t_note(fmt, a, b, c);
}

/* ---- names / blobs --------------------------------------------------------------------------- */
static const char *st_letter(int st) { return st == 2 ? "I" : st == 1 ? "O" : "N"; }

/* Typographic subfamily words (name ID 17): Windows 10 only groups faces into one weight/stretch/style family when the subfamily
 * string is made of recognised weight / width / style words; an opaque tag such as "W300S5N" splits every face into its own family. */
static const char *st_weight_word(int w) {
    return w <= 100 ? "Thin" : w <= 200 ? "ExtraLight" : w <= 300 ? "Light" : w <= 400 ? "Regular" : w <= 500 ? "Medium" : w <= 600 ? "SemiBold" : w <= 700 ? "Bold" : w <= 800 ? "ExtraBold" : "Black";
}
static const char *st_width_word(int wd) {
    static const char *t[] = { "", "UltraCondensed ", "ExtraCondensed ", "Condensed ", "SemiCondensed ", "", "SemiExpanded ", "Expanded ", "ExtraExpanded ", "UltraExpanded " };
    return wd >= 1 && wd <= 9 ? t[wd] : "";
}

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
        if (!ribbi) { ttf_add_name(&s, 3, 1, 0x409, 16, family); { char sty17[96]; snprintf(sty17, sizeof sty17, "%s%s%s", st_width_word(f->wd), st_weight_word(f->w), f->st == 2 ? " Italic" : f->st == 1 ? " Oblique" : ""); ttf_add_name(&s, 3, 1, 0x409, 17, sty17); } }
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
    if (n < 1 || n > DWX_MAX_BLOBS) { FAILF("bad face count %d", n); return 0; }
    m->n = n;
    memcpy(m->spec, faces, sizeof(fm_face) * n);
    m->f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
    if (!m->f) { FAILF("isolated factory not created"); return 0; }
    m->fl = dwx_loader_new();
    hr = dwx_loader_register(m->fl, m->f);
    if (FAILED(hr)) { FAILF("RegisterFontFileLoader %s", dw_hr_name(hr)); goto fail; }
    m->cl = dwx_coll_new(m->fl);
    for (i = 0; i < n; i++) {
        size_t sz; uint8_t *b = fm_blob(family, &faces[i], i, tw, &sz);
        UINT32 k = b ? dwx_loader_add(m->fl, b, sz) : 0xFFFFFFFFu;
        free(b);
        if (k != (UINT32)i) { FAILF("blob %d key %u", i, k); goto fail; }
        dwx_coll_add_key(m->cl, k);
    }
    hr = dwx_coll_register(m->cl, m->f);
    if (FAILED(hr)) { FAILF("RegisterFontCollectionLoader %s", dw_hr_name(hr)); goto fail; }
    hr = dwx_coll_create(m->cl, m->f, &m->coll);
    if (FAILED(hr) || !m->coll) { FAILF("CreateCustomFontCollection %s", dw_hr_name(hr)); goto fail; }
    dw_wide(family, wfam, 96);
    hr = IDWriteFontCollection_FindFamilyName(m->coll, wfam, &idx, &ex);
    if (FAILED(hr) || !ex) {
        UINT32 k, nf = IDWriteFontCollection_GetFontFamilyCount(m->coll);
        FAILF("FindFamilyName(%s) %s exists=%d count=%u", family, dw_hr_name(hr), ex, nf);
        for (k = 0; k < nf && k < 4; k++) {
            IDWriteFontFamily *ff = NULL; IDWriteLocalizedStrings *ls = NULL; char nm[96] = "?";
            if (SUCCEEDED(IDWriteFontCollection_GetFontFamily(m->coll, k, &ff)) && ff && SUCCEEDED(IDWriteFontFamily_GetFamilyNames(ff, &ls)) && ls) dw_ls_en(ls, nm, sizeof nm);
            t_note("collection family %u: '%s' fonts=%u", k, nm, ff ? IDWriteFontFamily_GetFontCount(ff) : 0);
            DW_RELEASE(ls); DW_RELEASE(ff);
        }
        goto fail;
    }
    hr = IDWriteFontCollection_GetFontFamily(m->coll, idx, &m->fam);
    if (FAILED(hr) || !m->fam) { FAILF("GetFontFamily %s", dw_hr_name(hr)); goto fail; }
    m->count = IDWriteFontFamily_GetFontCount(m->fam);
    if (m->count > FM_MAXF) { FAILF("family font count %u", m->count); goto fail; }
    for (i = 0; i < (int)m->count; i++) {
        IDWriteFont *font = NULL; int k;
        hr = IDWriteFontFamily_GetFont(m->fam, i, &font);
        if (FAILED(hr) || !font) { FAILF("GetFont(%d) %s", i, dw_hr_name(hr)); goto fail; }
        fm_read_font(font, &m->obs[i], &m->obs_sims[i]);
        m->src[i] = -1;
        if (m->obs_sims[i] == 0) m->nreal++;
        for (k = 0; k < n && m->obs_sims[i] == 0; k++) if (faces[k].w == m->obs[i].w && faces[k].wd == m->obs[i].wd && faces[k].st == m->obs[i].st) m->src[i] = k;
        IDWriteFont_Release(font);
    }
    return 1;
fail:
    fm_close(m);
    return 0;
}

/* real (non simulated) members of the family, with their index in m->obs */
static int fm_reals(const fm *m, fm_face *out, int *r2o) {
    int i, k = 0;
    for (i = 0; i < (int)m->count; i++) if (m->obs_sims[i] == 0) { out[k] = m->obs[i]; r2o[k] = i; k++; }
    return k;
}
fm_pick fm_ofirst(const fm *m, int rw, int rwd, int rst) {
    fm_face r[FM_MAXF]; int o[FM_MAXF]; int n = fm_reals(m, r, o); fm_pick p = fm_oracle_first(r, n, rw, rwd, rst);
    if (p.idx >= 0) p.idx = o[p.idx];
    return p;
}
int fm_ofirst_tied(const fm *m, int rw, int rwd, int rst) {
    fm_face r[FM_MAXF]; int o[FM_MAXF]; int n = fm_reals(m, r, o);
    return fm_oracle_first_tied(r, n, rw, rwd, rst);
}
int fm_olist(const fm *m, int rw, int rwd, int rst, fm_pick *out) {
    fm_face r[FM_MAXF]; int o[FM_MAXF]; int n = fm_reals(m, r, o), k, i;
    k = fm_oracle_list(r, n, rw, rwd, rst, out);
    for (i = 0; i < k; i++) out[i].idx = o[out[i].idx];
    return k;
}
int fm_olist_tied(const fm *m, int rw, int rwd, int rst) {
    fm_face r[FM_MAXF]; int o[FM_MAXF]; int n = fm_reals(m, r, o);
    return fm_oracle_list_tied(r, n, rw, rwd, rst);
}

void fm_close(fm *m) {
    DW_RELEASE(m->fam);
    DW_RELEASE(m->coll);
    if (m->borrowed) return;
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

int fm_open_system(fm *m, const WCHAR *family) {
    UINT32 idx = 0; BOOL ex = FALSE; HRESULT hr; int i;
    memset(m, 0, sizeof *m);
    if (!dw_proc()) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    m->f = dw_shared(); m->borrowed = 1;
    dw_narrow(family, m->name, sizeof m->name);
    hr = IDWriteFactory_GetSystemFontCollection(m->f, &m->coll, FALSE);
    if (FAILED(hr) || !m->coll) { CHECKF(0, "GetSystemFontCollection %s", dw_hr_name(hr)); return 0; }
    hr = IDWriteFontCollection_FindFamilyName(m->coll, family, &idx, &ex);
    if (FAILED(hr) || !ex) { DW_RELEASE(m->coll); t_skip("font family %s not installed", m->name); return 0; }
    hr = IDWriteFontCollection_GetFontFamily(m->coll, idx, &m->fam);
    if (FAILED(hr) || !m->fam) { CHECKF(0, "GetFontFamily %s", dw_hr_name(hr)); DW_RELEASE(m->coll); return 0; }
    m->count = IDWriteFontFamily_GetFontCount(m->fam);
    if (m->count > FM_MAXF) { t_note("family %s has %u fonts, only the first %d are modelled", m->name, m->count, FM_MAXF); m->count = FM_MAXF; }
    for (i = 0; i < (int)m->count; i++) {
        IDWriteFont *font = NULL;
        m->src[i] = -1;
        if (FAILED(IDWriteFontFamily_GetFont(m->fam, i, &font)) || !font) { CHECKF(0, "GetFont(%d) failed", i); fm_close(m); return 0; }
        fm_read_font(font, &m->obs[i], &m->obs_sims[i]);
        if (m->obs_sims[i] == 0) m->nreal++;
        IDWriteFont_Release(font);
    }
    return 1;
}

/* ---- checks ----------------------------------------------------------------------------------- */
void fm_check_font(fm *m, IDWriteFont *font, const fm_face *face, int sims, const char *tag) {
    fm_face got; int gs; fm_face want;
    IDWriteFontFamily *ff = NULL;
    /* Windows 10 family lists carry simulated members (entries with their own simulations); the matcher then adds more only on top */
    if (face >= m->obs && face < m->obs + FM_MAXF) sims |= m->obs_sims[face - m->obs];
    want = fm_reported(face, sims);
    if (!font) { CHECKF(0, "%s: no font", tag); return; }
    fm_read_font(font, &got, &gs);
    if (!(got.w == want.w && got.wd == want.wd && got.st == want.st && gs == sims) && m->count && g_dumped < 6) {
        int q; char b[1400]; int o = 0;
        g_dumped++;
        for (q = 0; q < (int)m->count && o < 1300; q++) o += snprintf(b + o, sizeof b - o, " (%d,%d,%d s%d)", m->obs[q].w, m->obs[q].wd, m->obs[q].st, m->obs_sims[q]);
        t_note("family %s members:%s", m->name, b);
    }
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
    fm_pick p = fm_ofirst(m, rw, rwd, rst);
    fm_pick lp[FM_MAXF]; int ln, i; UINT32 cnt;
    IDWriteFontList *list = NULL; HRESULT hr;
    char tag[96];
    int ft = fm_ofirst_tied(m, rw, rwd, rst), lt = fm_olist_tied(m, rw, rwd, rst);
    if (p.idx >= 0 && !ft) fm_expect_first(m, rw, rwd, rst, p.idx, p.sims);
    else if (p.idx >= 0) { IDWriteFont *fo = NULL; CHECK_HR_OK(IDWriteFontFamily_GetFirstMatchingFont(m->fam, (DWRITE_FONT_WEIGHT)rw, (DWRITE_FONT_STRETCH)rwd, (DWRITE_FONT_STYLE)rst, &fo)); CHECK(fo != NULL); DW_RELEASE(fo); }
    ln = fm_olist(m, rw, rwd, rst, lp);
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
        /* only the head of the list is modelled: Windows 10 interleaves the simulated members in the rest of the order */
        if (i > 0) break;
        hr = IDWriteFontList_GetFont(list, i, &font);
        CHECKF(hr == S_OK, "%s[%d] hr %s", tag, i, dw_hr_name(hr));
        if (hr == S_OK) { char t2[120]; snprintf(t2, sizeof t2, "%s[%d]", tag, i); fm_check_font(m, font, &m->obs[lp[i].idx], lp[i].sims, t2); }
        DW_RELEASE(font);
    }
    IDWriteFontList_Release(list);
}
