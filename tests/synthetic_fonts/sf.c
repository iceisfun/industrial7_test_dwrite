#include "sf.h"

/* ---- blobs --------------------------------------------------------------------------------------------- */
sf_blob sf_default(void) { sf_blob b; b.p = ttf_build_default(&b.n); return b; }
sf_blob sf_build(const ttf_spec *s) { sf_blob b; b.p = ttf_build(s, &b.n); return b; }
sf_blob sf_dup(const sf_blob *s) { sf_blob b; b.n = s->n; b.p = (uint8_t *)malloc(s->n ? s->n : 1); if (s->n) memcpy(b.p, s->p, s->n); return b; }
void sf_free(sf_blob *b) { free(b->p); b->p = NULL; b->n = 0; }

int sf_ntables(const sf_blob *b) { return b->n >= 12 ? (int)sf_r16(b->p + 4) : 0; }
uint8_t *sf_dirent(const sf_blob *b, int i) { size_t e = 12 + 16 * (size_t)i; return e + 16 <= b->n ? b->p + e : NULL; }
int sf_find(const sf_blob *b, const char *tag) {
    int i, n = sf_ntables(b);
    for (i = 0; i < n; i++) { uint8_t *e = sf_dirent(b, i); if (e && !memcmp(e, tag, 4)) return i; }
    return -1;
}
uint8_t *sf_table(const sf_blob *b, const char *tag, unsigned *len) {
    int i = sf_find(b, tag); uint8_t *e; uint32_t o, l;
    if (i < 0) return NULL;
    e = sf_dirent(b, i); o = sf_r32(e + 8); l = sf_r32(e + 12);
    if (o > b->n || l > b->n - o) return NULL;
    if (len) *len = l;
    return b->p + o;
}
void sf_set_ent(sf_blob *b, int i, uint32_t off, uint32_t len) { uint8_t *e = sf_dirent(b, i); if (e) { sf_w32(e + 8, off); sf_w32(e + 12, len); } }
void sf_set_tag(sf_blob *b, int i, const char *tag) { uint8_t *e = sf_dirent(b, i); if (e) memcpy(e, tag, 4); }
void sf_set_numtables(sf_blob *b, unsigned n) { sf_w16(b->p + 4, n); }
void sf_swap_ents(sf_blob *b, int i, int j) { uint8_t t[16], *a = sf_dirent(b, i), *c = sf_dirent(b, j); if (a && c) { memcpy(t, a, 16); memcpy(a, c, 16); memcpy(c, t, 16); } }
static uint32_t csum(const uint8_t *p, size_t n) {
    uint32_t s = 0; size_t i;
    for (i = 0; i + 4 <= n; i += 4) s += sf_r32(p + i);
    if (n & 3) { uint8_t t[4] = { 0, 0, 0, 0 }; memcpy(t, p + (n & ~(size_t)3), n & 3); s += sf_r32(t); }
    return s;
}
void sf_replace_table(sf_blob *b, const char *tag, const void *data, size_t n) {
    int i = sf_find(b, tag); size_t pad = (4 - (b->n & 3)) & 3, off; uint8_t *e;
    if (i < 0) return;
    b->p = (uint8_t *)realloc(b->p, b->n + pad + n + 4);
    memset(b->p + b->n, 0, pad); off = b->n + pad;
    if (n) memcpy(b->p + off, data, n);
    b->n = off + n;
    e = sf_dirent(b, i); sf_w32(e + 4, csum(b->p + off, n)); sf_w32(e + 8, (uint32_t)off); sf_w32(e + 12, (uint32_t)n);
}
void sf_drop_record(sf_blob *b, const char *tag) {
    int i = sf_find(b, tag), n = sf_ntables(b);
    if (i < 0) return;
    memmove(sf_dirent(b, i), sf_dirent(b, i + 1), 16 * (size_t)(n - i - 1));
    memset(sf_dirent(b, n - 1), 0, 16);
    sf_set_numtables(b, n - 1);          /* the freed 16 bytes stay as a hole in the file; offsets are unchanged */
}
void sf_fix_all_checksums(sf_blob *b) {
    int i, n = sf_ntables(b);
    for (i = 0; i < n; i++) {
        uint8_t *e = sf_dirent(b, i); uint32_t o = sf_r32(e + 8), l = sf_r32(e + 12);
        if (o <= b->n && l <= b->n - o) sf_w32(e + 4, csum(b->p + o, l));
    }
}

/* ---- probe ---------------------------------------------------------------------------------------------- */
int sf_open(sf_probe *p, const void *blob, size_t n) {
    static const uint8_t one = 0;
    memset(p, 0, sizeof *p);
    p->supported = 0x55; p->ftype = (DWRITE_FONT_FILE_TYPE)0x55; p->fctype = (DWRITE_FONT_FACE_TYPE)0x55; p->nfaces = 0x55555555;
    p->f = dw_isolated();
    if (!p->f) { t_skip("DWriteCreateFactory unavailable"); return 0; }
    p->ld = dwx_loader_new();
    p->key = dwx_loader_add(p->ld, blob ? blob : &one, blob ? n : 0);
    p->hr_reg = dwx_loader_register(p->ld, p->f);
    if (FAILED(p->hr_reg)) { t_skip("RegisterFontFileLoader failed: %s", dw_hr_name(p->hr_reg)); return 0; }
    p->hr_file = dwx_loader_file(p->ld, p->f, p->key, &p->file);
    if (FAILED(p->hr_file) || !p->file) { t_skip("CreateCustomFontFileReference failed: %s", dw_hr_name(p->hr_file)); return 0; }
    p->reads_before_analyze = dwx_loader_total_reads(p->ld);
    p->hr_analyze = IDWriteFontFile_Analyze(p->file, &p->supported, &p->ftype, &p->fctype, &p->nfaces);
    p->reads_after_analyze = dwx_loader_total_reads(p->ld);
    return 1;
}
HRESULT sf_face(sf_probe *p, DWRITE_FONT_FACE_TYPE type, UINT32 index, DWRITE_FONT_SIMULATIONS sim) {
    DW_RELEASE(p->face);
    p->hr_face = IDWriteFactory_CreateFontFace(p->f, type, 1, &p->file, index, sim, &p->face);
    return p->hr_face;
}
int sf_open_face(sf_probe *p, const void *blob, size_t n) {
    if (!sf_open(p, blob, n)) return 0;
    sf_face(p, (SUCCEEDED(p->hr_analyze) && p->supported) ? p->fctype : DWRITE_FONT_FACE_TYPE_TRUETYPE, 0, DWRITE_FONT_SIMULATIONS_NONE);
    return 1;
}
void sf_close(sf_probe *p) {
    DW_RELEASE(p->face); DW_RELEASE(p->file);
    if (p->ld) { dwx_loader_unregister(p->ld); dwx_release(p->ld); p->ld = NULL; }
}
dwx_mem_stream *sf_stream(sf_probe *p) { return p->ld ? p->ld->last_stream : NULL; }

void sf_pred(int ok, const char *label, const char *expr) { if (!ok) t_note("PREDICTION-MISS %s: %s", label, expr); }
void sf_note(const char *label, const sf_probe *p) {
    t_note("%s: file=%s analyze=%s supported=%d fileType=%d faceType=%d nfaces=%u face=%s%s", label, dw_hr_name(p->hr_file), dw_hr_name(p->hr_analyze),
           (int)p->supported, (int)p->ftype, (int)p->fctype, (unsigned)p->nfaces, dw_hr_name(p->hr_face), p->face ? "(object)" : "(null)");
}
UINT16 sf_gid(IDWriteFontFace *face, UINT32 cp) {
    UINT16 g = 0xFFFF;
    if (!face || FAILED(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g))) return 0xFFFF;
    return g;
}

/* ---- byte builders --------------------------------------------------------------------------------------- */
static void bgrow(sf_buf *b, size_t add) {
    if (b->n + add > b->cap) { b->cap = (b->n + add) * 2 + 64; b->p = (uint8_t *)realloc(b->p, b->cap); }
}
void sf_b8(sf_buf *b, unsigned v) { bgrow(b, 1); b->p[b->n++] = (uint8_t)v; }
void sf_b16(sf_buf *b, unsigned v) { sf_b8(b, v >> 8); sf_b8(b, v); }
void sf_b32(sf_buf *b, uint32_t v) { sf_b16(b, v >> 16); sf_b16(b, v & 0xFFFF); }
void sf_bz(sf_buf *b, size_t n) { while (n--) sf_b8(b, 0); }
void sf_buf_free(sf_buf *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

sf_buf sf_cmap_table(const sf_cmap_rec *recs, int n) {
    sf_buf o = { 0, 0, 0 }; int i; size_t off = 4 + 8 * (size_t)n;
    sf_b16(&o, 0); sf_b16(&o, n);
    for (i = 0; i < n; i++) { sf_b16(&o, recs[i].plat); sf_b16(&o, recs[i].enc); sf_b32(&o, (uint32_t)off); off += recs[i].len; }
    for (i = 0; i < n; i++) { size_t k; for (k = 0; k < recs[i].len; k++) sf_b8(&o, recs[i].data[k]); }
    return o;
}
sf_buf sf_cmap0(const uint8_t map[256]) {
    sf_buf o = { 0, 0, 0 }; int i;
    sf_b16(&o, 0); sf_b16(&o, 262); sf_b16(&o, 0);
    for (i = 0; i < 256; i++) sf_b8(&o, map[i]);
    return o;
}
sf_buf sf_cmap6(unsigned first, const uint16_t *gids, unsigned n) {
    sf_buf o = { 0, 0, 0 }; unsigned i;
    sf_b16(&o, 6); sf_b16(&o, 10 + 2 * n); sf_b16(&o, 0); sf_b16(&o, first); sf_b16(&o, n);
    for (i = 0; i < n; i++) sf_b16(&o, gids[i]);
    return o;
}
sf_buf sf_cmap4(const sf_seg4g *segs, int n, int term) {
    sf_buf o = { 0, 0, 0 }; int i, sc = n + (term ? 1 : 0); size_t ga_total = 0, ga_pos = 0;
    for (i = 0; i < n; i++) if (segs[i].garr) ga_total += segs[i].end - segs[i].start + 1;
    sf_b16(&o, 4); sf_b16(&o, (unsigned)(16 + 8 * sc + 2 * ga_total)); sf_b16(&o, 0);
    sf_b16(&o, sc * 2); sf_b16(&o, 0); sf_b16(&o, 0); sf_b16(&o, 0);
    for (i = 0; i < n; i++) sf_b16(&o, segs[i].end);
    if (term) sf_b16(&o, 0xFFFF);
    sf_b16(&o, 0);
    for (i = 0; i < n; i++) sf_b16(&o, segs[i].start);
    if (term) sf_b16(&o, 0xFFFF);
    for (i = 0; i < n; i++) sf_b16(&o, (unsigned)segs[i].delta & 0xFFFF);
    if (term) sf_b16(&o, 1);
    for (i = 0; i < n; i++) {
        if (segs[i].garr) {
            /* distance from idRangeOffset[i] to the first glyph array entry belonging to this segment */
            size_t here = 2 * (size_t)(sc - i) + 2 * ga_pos;
            sf_b16(&o, (unsigned)here); ga_pos += segs[i].end - segs[i].start + 1;
        } else sf_b16(&o, 0);
    }
    if (term) sf_b16(&o, 0);
    for (i = 0; i < n; i++) if (segs[i].garr) { unsigned k; for (k = 0; k < segs[i].end - segs[i].start + 1u; k++) sf_b16(&o, segs[i].garr[k]); }
    return o;
}
sf_buf sf_cmap12(const sf_grp *g, int n, int fmt) {
    sf_buf o = { 0, 0, 0 }; int i;
    sf_b16(&o, fmt); sf_b16(&o, 0); sf_b32(&o, 16 + 12 * n); sf_b32(&o, 0); sf_b32(&o, n);
    for (i = 0; i < n; i++) { sf_b32(&o, g[i].start); sf_b32(&o, g[i].end); sf_b32(&o, g[i].gid); }
    return o;
}

void sf_insert_record_raw(sf_blob *b, int pos, const char *tag, uint32_t off, uint32_t len) {
    int n = sf_ntables(b), i; size_t hdr = 12 + 16 * (size_t)n; uint8_t *nb = (uint8_t *)calloc(b->n + 16 + 4, 1); uint8_t *e;
    memcpy(nb, b->p, 12);
    sf_w16(nb + 4, n + 1);
    for (i = 0; i < n; i++) {
        uint8_t *dst = nb + 12 + 16 * (size_t)(i < pos ? i : i + 1);
        memcpy(dst, b->p + 12 + 16 * (size_t)i, 16); sf_w32(dst + 8, sf_r32(dst + 8) + 16);
    }
    e = nb + 12 + 16 * (size_t)pos; memcpy(e, tag, 4); sf_w32(e + 4, 0); sf_w32(e + 8, off); sf_w32(e + 12, len);
    memcpy(nb + hdr + 16, b->p + hdr, b->n - hdr);
    free(b->p); b->p = nb; b->n += 16;
}
void sf_insert_record(sf_blob *b, int pos, const char *tag, const void *data, size_t n) {
    size_t pad, off;
    sf_insert_record_raw(b, pos, tag, 0, 0);
    pad = (4 - (b->n & 3)) & 3; off = b->n + pad;
    b->p = (uint8_t *)realloc(b->p, off + n + 4);
    memset(b->p + b->n, 0, pad); if (n) memcpy(b->p + off, data, n); b->n = off + n;
    sf_w32(sf_dirent(b, pos) + 8, (uint32_t)off); sf_w32(sf_dirent(b, pos) + 12, (uint32_t)n);
    sf_w32(sf_dirent(b, pos) + 4, csum((const uint8_t *)data, n));
}

static const char *const g_survey_tags[10] = { "head", "hhea", "maxp", "OS/2", "hmtx", "cmap", "loca", "glyf", "name", "post" };
int sf_survey_light;   /* set by tests that survey damaged fonts: under an emulator skip the calls that dereference unparsed tables */
void sf_survey(const char *label, sf_probe *p) {
    char line[900]; int len = 0, i;
    len += snprintf(line + len, sizeof line - len, "%s: analyze=%s sup=%d ft=%d fc=%d nf=%u face=%s", label, dw_hr_name(p->hr_analyze), (int)p->supported, (int)p->ftype, (int)p->fctype, (unsigned)p->nfaces, dw_hr_name(p->hr_face));
    if (p->face && sf_survey_light && t_is_wine()) {
        len += snprintf(line + len, sizeof line - len, " | (face object created; metrics skipped under Wine)");
    } else if (p->face) {
        DWRITE_FONT_METRICS m; memset(&m, 0, sizeof m);
        IDWriteFontFace_GetMetrics(p->face, &m);
        len += snprintf(line + len, sizeof line - len, " | glyphs=%u upem=%u asc=%u desc=%u gap=%d cap=%u xh=%u ul=%d/%u st=%d/%u | A=%u space=%u z=%u |",
                        (unsigned)IDWriteFontFace_GetGlyphCount(p->face), (unsigned)m.designUnitsPerEm, (unsigned)m.ascent, (unsigned)m.descent, (int)m.lineGap, (unsigned)m.capHeight,
                        (unsigned)m.xHeight, (int)m.underlinePosition, (unsigned)m.underlineThickness, (int)m.strikethroughPosition, (unsigned)m.strikethroughThickness,
                        (unsigned)sf_gid(p->face, 'A'), (unsigned)sf_gid(p->face, ' '), (unsigned)sf_gid(p->face, 'z'));
        for (i = 0; i < 10; i++) {
            const void *d = NULL; UINT32 n = 0; void *ctx = NULL; BOOL ex = FALSE; HRESULT hr;
            UINT32 tag = (UINT32)g_survey_tags[i][0] | ((UINT32)g_survey_tags[i][1] << 8) | ((UINT32)g_survey_tags[i][2] << 16) | ((UINT32)g_survey_tags[i][3] << 24);
            hr = IDWriteFontFace_TryGetFontTable(p->face, tag, &d, &n, &ctx, &ex);
            if (FAILED(hr)) len += snprintf(line + len, sizeof line - len, " %s:ERR(%s)", g_survey_tags[i], dw_hr_name(hr));
            else if (!ex) len += snprintf(line + len, sizeof line - len, " %s:absent", g_survey_tags[i]);
            else { len += snprintf(line + len, sizeof line - len, " %s:%u", g_survey_tags[i], (unsigned)n); IDWriteFontFace_ReleaseFontTable(p->face, ctx); }
        }
    }
    t_note("%s", line);
}
