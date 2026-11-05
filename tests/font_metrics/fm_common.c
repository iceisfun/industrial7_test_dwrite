#include "fm.h"

/* ---- fonts ------------------------------------------------------------------------------------------------ */
int fm_open_blob(fm_font *F, const void *blob, size_t n, DWRITE_FONT_SIMULATIONS sim) {
    IDWriteFactory *f = dw_shared(); HRESULT hr;
    if (!f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    F->sim = sim;
    hr = dwx_font_open(&F->h, f, blob, n, 0, sim);
    if (FAILED(hr) || !F->h.face) { t_skip("cannot open synthetic font: %s", dw_hr_name(hr)); return 0; }
    F->face = F->h.face;
    IDWriteFontFace_QueryInterface(F->face, &IID_IDWriteFontFace1, (void **)&F->f1);
    return 1;
}
int fm_open(fm_font *F, const ttf_spec *s, DWRITE_FONT_SIMULATIONS sim) {
    size_t n = 0; uint8_t *b = ttf_build(s, &n); int ok;
    if (!b) { t_skip("ttf_build failed"); return 0; }
    ok = fm_open_blob(F, b, n, sim);
    free(b);
    return ok;
}
void fm_close(fm_font *F) { DW_RELEASE(F->f1); dwx_font_close(&F->h); F->face = NULL; }
uint8_t *fm_tbl(uint8_t *blob, size_t n, const char *tag, unsigned *len) { return (uint8_t *)ttf_find_table(blob, n, tag, len); }
void fm_spec_upm(ttf_spec *s, int upm) {
    ttf_default_spec(s);
    s->upm = upm;
    s->hhea_asc = upm * 1700 / 2048; s->hhea_desc = -(upm * 400 / 2048); s->hhea_gap = upm * 90 / 2048;
    s->typo_asc = upm * 1600 / 2048; s->typo_desc = -(upm * 420 / 2048); s->typo_gap = upm * 80 / 2048;
    s->win_asc = upm * 1800 / 2048; s->win_desc = upm * 450 / 2048;
    s->cap_height = upm * 1400 / 2048; s->x_height = upm * 1000 / 2048;
    s->underline_pos = -(upm * 200 / 2048); s->underline_thick = upm * 100 / 2048;
    s->strike_pos = upm * 600 / 2048; s->strike_size = upm * 100 / 2048;
}

/* ---- field table --------------------------------------------------------------------------------------------- */
static const struct { const char *n; size_t off; int sgn; } F_[FM_NFIELDS] = {
    { "designUnitsPerEm", offsetof(DWRITE_FONT_METRICS1, designUnitsPerEm), 0 }, { "ascent", offsetof(DWRITE_FONT_METRICS1, ascent), 0 },
    { "descent", offsetof(DWRITE_FONT_METRICS1, descent), 0 }, { "lineGap", offsetof(DWRITE_FONT_METRICS1, lineGap), 1 },
    { "capHeight", offsetof(DWRITE_FONT_METRICS1, capHeight), 0 }, { "xHeight", offsetof(DWRITE_FONT_METRICS1, xHeight), 0 },
    { "underlinePosition", offsetof(DWRITE_FONT_METRICS1, underlinePosition), 1 }, { "underlineThickness", offsetof(DWRITE_FONT_METRICS1, underlineThickness), 0 },
    { "strikethroughPosition", offsetof(DWRITE_FONT_METRICS1, strikethroughPosition), 1 }, { "strikethroughThickness", offsetof(DWRITE_FONT_METRICS1, strikethroughThickness), 0 },
    { "glyphBoxLeft", offsetof(DWRITE_FONT_METRICS1, glyphBoxLeft), 1 }, { "glyphBoxTop", offsetof(DWRITE_FONT_METRICS1, glyphBoxTop), 1 },
    { "glyphBoxRight", offsetof(DWRITE_FONT_METRICS1, glyphBoxRight), 1 }, { "glyphBoxBottom", offsetof(DWRITE_FONT_METRICS1, glyphBoxBottom), 1 },
    { "subscriptPositionX", offsetof(DWRITE_FONT_METRICS1, subscriptPositionX), 1 }, { "subscriptPositionY", offsetof(DWRITE_FONT_METRICS1, subscriptPositionY), 1 },
    { "subscriptSizeX", offsetof(DWRITE_FONT_METRICS1, subscriptSizeX), 1 }, { "subscriptSizeY", offsetof(DWRITE_FONT_METRICS1, subscriptSizeY), 1 },
    { "superscriptPositionX", offsetof(DWRITE_FONT_METRICS1, superscriptPositionX), 1 }, { "superscriptPositionY", offsetof(DWRITE_FONT_METRICS1, superscriptPositionY), 1 },
    { "superscriptSizeX", offsetof(DWRITE_FONT_METRICS1, superscriptSizeX), 1 }, { "superscriptSizeY", offsetof(DWRITE_FONT_METRICS1, superscriptSizeY), 1 },
    { "hasTypographicMetrics", offsetof(DWRITE_FONT_METRICS1, hasTypographicMetrics), 2 },
};
const char *fm_field_name(int i) { return F_[i].n; }
int fm_field_signed(int i) { return F_[i].sgn; }
int fm_field_get(const DWRITE_FONT_METRICS1 *m, int i) {
    const uint8_t *p = (const uint8_t *)m + F_[i].off;
    if (F_[i].sgn == 2) return *(const int32_t *)p != 0;
    return F_[i].sgn ? (int)*(const int16_t *)p : (int)*(const uint16_t *)p;
}
void fm_field_set(DWRITE_FONT_METRICS1 *m, int i, int v) {
    uint8_t *p = (uint8_t *)m + F_[i].off;
    if (F_[i].sgn == 2) *(int32_t *)p = v; else *(uint16_t *)p = (uint16_t)v;
}
void fm_cmp_(const char *file, int line, const DWRITE_FONT_METRICS1 *got, const DWRITE_FONT_METRICS1 *want, unsigned skip, const char *label) {
    int i;
    for (i = 0; i < FM_NFIELDS; i++) {
        int g, w;
        if (skip & (1u << i)) continue;
        g = fm_field_get(got, i); w = fm_field_get(want, i);
        t_check(g == w, file, line, F_[i].n, "%s: got %d want %d", label ? label : "", g, w);
    }
}
void fm_dump(const char *label, const DWRITE_FONT_METRICS1 *m) {
    t_note("%s: upm=%d asc=%d desc=%d gap=%d cap=%d xh=%d ul=%d/%d st=%d/%d box=%d,%d,%d,%d sub=%d,%d,%d,%d sup=%d,%d,%d,%d typo=%d", label,
           m->designUnitsPerEm, m->ascent, m->descent, m->lineGap, m->capHeight, m->xHeight, m->underlinePosition, m->underlineThickness,
           m->strikethroughPosition, m->strikethroughThickness, m->glyphBoxLeft, m->glyphBoxTop, m->glyphBoxRight, m->glyphBoxBottom,
           m->subscriptPositionX, m->subscriptPositionY, m->subscriptSizeX, m->subscriptSizeY,
           m->superscriptPositionX, m->superscriptPositionY, m->superscriptSizeX, m->superscriptSizeY, (int)m->hasTypographicMetrics);
}

/* ---- derivation from tables ------------------------------------------------------------------------------------ */
int fm_x_valid(unsigned upm, unsigned x) { unsigned t = x * 10; return upm < t && t < upm * 9; }
int fm_cap_valid(unsigned upm, unsigned cap) { return upm < cap * 10 && cap <= upm; }
int fm_shear(int v) { return (int16_t)((v * 0x5700) >> 16); }
static int clamp_us(int v) { return v < 0 ? 0 : v >= 0x10000 ? 0xffff : v; }
int fm_has_table(IDWriteFontFace *face, UINT32 tag) { UINT32 n = 0; uint8_t *t = dw_face_table(face, tag, &n); int r = t && n; free(t); return r; }

int fm_derive(IDWriteFontFace *face, DWRITE_FONT_SIMULATIONS sims, fm_exp *e) {
    UINT32 hl = 0, al = 0, ol = 0, pl = 0;
    uint8_t *head = dw_face_table(face, DW_TAG('h', 'e', 'a', 'd'), &hl), *hhea = dw_face_table(face, DW_TAG('h', 'h', 'e', 'a'), &al);
    uint8_t *os2 = dw_face_table(face, DW_TAG('O', 'S', '/', '2'), &ol), *post = dw_face_table(face, DW_TAG('p', 'o', 's', 't'), &pl);
    DWRITE_FONT_METRICS1 *m = &e->m; unsigned upm; int has_aat, has_otl, i;
    memset(e, 0, sizeof *e);
    if (!head || hl < 54 || !hhea || al < 36) { free(head); free(hhea); free(os2); free(post); return 0; }
    e->ok = 1;
    upm = fm_be16(head + 18);
    m->designUnitsPerEm = (UINT16)upm;
    m->glyphBoxLeft = (INT16)fm_be16s(head + 36); m->glyphBoxTop = (INT16)fm_be16s(head + 42);
    m->glyphBoxRight = (INT16)fm_be16s(head + 40); m->glyphBoxBottom = (INT16)fm_be16s(head + 38);
    e->caret.slopeRise = (INT16)fm_be16s(hhea + 18); e->caret.slopeRun = (INT16)fm_be16s(hhea + 20); e->caret.offset = (INT16)fm_be16s(hhea + 22);
    e->os2_present = os2 && ol > 0x4d;
    e->post_present = post && pl >= 12;
    has_aat = fm_has_table(face, DW_TAG('m', 'o', 'r', 't')) || fm_has_table(face, DW_TAG('m', 'o', 'r', 'x'));
    has_otl = fm_has_table(face, DW_TAG('G', 'S', 'U', 'B')) || fm_has_table(face, DW_TAG('G', 'P', 'O', 'S')) || fm_has_table(face, DW_TAG('G', 'D', 'E', 'F'));
    {
        int hasc = fm_be16(hhea + 4), hdesc = abs(fm_be16s(hhea + 6)), hgap = fm_be16s(hhea + 8); if (hgap < 0) hgap = 0;
        int generic = 1;
        if (e->os2_present) {
            e->os2_version = fm_be16(os2);
            m->subscriptPositionX = (INT16)fm_be16s(os2 + 14); m->subscriptPositionY = (INT16)-fm_be16s(os2 + 16);
            m->subscriptSizeX = (INT16)fm_be16s(os2 + 10); m->subscriptSizeY = (INT16)fm_be16s(os2 + 12);
            m->superscriptPositionX = (INT16)fm_be16s(os2 + 22); m->superscriptPositionY = (INT16)fm_be16s(os2 + 24);
            m->superscriptSizeX = (INT16)fm_be16s(os2 + 18); m->superscriptSizeY = (INT16)fm_be16s(os2 + 20);
            if (os2[0x3f] & 0x80) {
                m->ascent = (UINT16)clamp_us(fm_be16s(os2 + 0x44)); m->descent = (UINT16)clamp_us(-fm_be16s(os2 + 0x46));
                m->lineGap = (INT16)fm_be16s(os2 + 0x48); m->hasTypographicMetrics = TRUE; generic = 0;
            }
        }
        if (generic) {
            unsigned wasc = e->os2_present ? fm_be16(os2 + 0x4a) : 0, wdesc = e->os2_present ? fm_be16(os2 + 0x4c) : 0;
            if (e->os2_present && (!has_aat || has_otl) && (wasc || wdesc)) {
                int g = hgap + hdesc + hasc - (int)wdesc - (int)wasc;
                m->ascent = (UINT16)clamp_us((int)wasc); m->descent = (UINT16)clamp_us((int)wdesc);
                m->lineGap = (INT16)(g < 0 ? 0 : g > 0x7fff ? 0x7fff : g);
            } else {
                m->ascent = (UINT16)clamp_us(hasc); m->descent = (UINT16)clamp_us(hdesc);
                m->lineGap = (INT16)(hgap > 0x7fff ? 0x7fff : hgap);
            }
        }
    }
    if (e->post_present) {
        m->underlinePosition = (INT16)fm_be16s(post + 8); m->underlineThickness = (UINT16)fm_be16(post + 10);
        if (!m->underlineThickness) m->underlineThickness = (UINT16)((upm + 7) / 14);
    } else {
        m->underlinePosition = (INT16)-(int)((upm + 5) / 10); m->underlineThickness = (UINT16)((upm + 7) / 14);
    }
    if (e->os2_present) {
        int ss = fm_be16s(os2 + 26);
        m->strikethroughThickness = ss > 0 ? (UINT16)ss : m->underlineThickness;
        m->strikethroughPosition = (INT16)fm_be16s(os2 + 28);
        if (e->os2_version >= 2 && ol > 0x5f) {
            unsigned xh = fm_be16(os2 + 0x56), cap = fm_be16(os2 + 0x58);
            if (fm_x_valid(upm, xh)) { m->xHeight = (UINT16)xh; e->x_known = 1; }
            if (fm_cap_valid(upm, cap)) { m->capHeight = (UINT16)cap; e->cap_known = 1; }
        }
    } else {
        m->strikethroughThickness = m->underlineThickness; m->strikethroughPosition = (INT16)(upm / 3);
    }
    if (sims & DWRITE_FONT_SIMULATIONS_OBLIQUE) {
        m->glyphBoxRight = (INT16)(m->glyphBoxRight + fm_shear(m->glyphBoxTop));
        m->glyphBoxLeft = (INT16)(m->glyphBoxLeft + fm_shear(m->glyphBoxBottom));
        m->subscriptPositionX = (INT16)(m->subscriptPositionX + fm_shear(m->subscriptPositionY));
        m->superscriptPositionX = (INT16)(m->superscriptPositionX + fm_shear(m->superscriptPositionY));
    }
    (void)i;
    free(head); free(hhea); free(os2); free(post);
    return 1;
}
unsigned fm_unknown_mask(const fm_exp *e) {
    unsigned m = 0;
    if (!e->x_known) m |= FM_BIT(FM_XH);
    if (!e->cap_known) m |= FM_BIT(FM_CAP);
    return m;
}

/* ---- GDI mirror ---------------------------------------------------------------------------------------------------- */
int fm_rnd(float x) { int i = (int)lrintf(x); if ((float)i - x == -0.5f) i++; return i; }
static int zero_tol(float v) { return v < 1.52587890625e-05f && v > -1.52587890625e-05f; }
float fm_scale(const DWRITE_MATRIX *m) {
    float a = 0, b = 0, sc; int kind;
    if (!m) return 1.0f;
    if (zero_tol(m->m12)) { if (zero_tol(m->m21)) { kind = 1; a = m->m11; b = m->m22; } else kind = 4; }
    else if (zero_tol(m->m11)) { if (zero_tol(m->m22)) { kind = 3; a = m->m12; b = m->m21; } else kind = 6; }
    else { if (zero_tol(m->m21)) kind = 4; else if (zero_tol(m->m22)) kind = 6; else kind = 0; }
    if (kind & 1) { sc = fabsf(a); if (zero_tol(sc - fabsf(b))) return sc; }
    { float x = m->m11 * 0.0f + m->m21, y = m->m12 * 0.0f + m->m22; return sqrtf(y * y + x * x); }
}
static float grid(int v, float dpp) {
    float f = (float)v / dpp;
    if (fabsf(f) < 8388608.0f) f = (float)fm_rnd(f);
    return (float)fm_rnd(f * dpp);
}
void fm_gdi_expect(const DWRITE_FONT_METRICS1 *base, float em, float ppd, const DWRITE_MATRIX *mx, DWRITE_FONT_METRICS1 *out) {
    float sc = fm_scale(mx), ppem = em * ppd * sc, dpp; int i;
    *out = *base;
    if (zero_tol(ppem)) return;
    dpp = (float)base->designUnitsPerEm / ppem;
    if (zero_tol(dpp)) return;
    /* ascent / descent: no VDMX for the synthetic fonts; callers skip these two fields when a VDMX table exists */
    for (i = FM_ASC; i <= FM_SUPSY; i++) {
        int v = (int)grid(fm_field_get(base, i), dpp);
        if (i == FM_UPM) continue;
        fm_field_set(out, i, v);
    }
}

/* ---- glyph metrics ------------------------------------------------------------------------------------------------- */
void fm_design_expect(const fm_gbox *g, int vasc, int vdesc, DWRITE_GLYPH_METRICS *o) {
    int advh = vasc + vdesc, tsb = vasc - g->y1;
    o->advanceWidth = (UINT32)g->adv; o->leftSideBearing = g->lsb;
    o->rightSideBearing = g->adv - g->lsb - g->x1 + g->x0;
    o->advanceHeight = (UINT32)advh; o->topSideBearing = tsb;
    o->bottomSideBearing = (g->y0 - g->y1) - tsb + advh;
    o->verticalOriginY = g->y1 + tsb;
}
void fm_gm_cmp_(const char *file, int line, const DWRITE_GLYPH_METRICS *g, const DWRITE_GLYPH_METRICS *w, const char *label) {
#define F(f) t_check((int)g->f == (int)w->f, file, line, #f, "%s: got %d want %d", label ? label : "", (int)g->f, (int)w->f)
    F(leftSideBearing); F(advanceWidth); F(rightSideBearing); F(topSideBearing); F(advanceHeight); F(bottomSideBearing); F(verticalOriginY);
#undef F
}
const fm_gbox *fm_def_glyph(unsigned gid) {
    static fm_gbox box = { 1000, 100, 100, 0, 900, 1400 }, notdef = { 1200, 100, 100, 0, 1100, 1400 }, space = { 1000, 0, 0, 0, 0, 0 },
                   comp = { 1000, 100, 100, 0, 900, 2800 }, e500 = { 500, 0, 0, 0, 0, 0 };
    if (gid == 0) return &notdef; if (gid == 1) return &space; if (gid == 66) return &comp; if (gid == 67) return &e500;
    if (gid < 69) return &box;
    return NULL;
}

/* ---- system fonts ---------------------------------------------------------------------------------------------------- */
IDWriteFontFace *fm_sys(const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STYLE st) {
    IDWriteFactory *f = dw_shared();
    if (!f) return NULL;
    return dw_sysface(f, family, w, DWRITE_FONT_STRETCH_NORMAL, st);
}
IDWriteFontFace1 *fm_f1(IDWriteFontFace *f) { void *p = NULL; if (SUCCEEDED(IDWriteFontFace_QueryInterface(f, &IID_IDWriteFontFace1, &p))) return (IDWriteFontFace1 *)p; return NULL; }
