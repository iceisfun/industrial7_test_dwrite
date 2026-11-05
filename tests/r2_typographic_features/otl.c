/* Minimal OpenType layout (GSUB / GPOS) serialiser used to give the typography tests fonts with known feature lists. */
#include "r2.h"
#include <stdarg.h>

typedef struct { uint8_t *p; size_t n, cap; } ob;
static void o_put(ob *b, const void *d, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->n + n) * 2 + 64; b->p = (uint8_t *)realloc(b->p, b->cap); }
    memcpy(b->p + b->n, d, n); b->n += n;
}
static void o16(ob *b, unsigned v) { uint8_t t[2] = { (uint8_t)(v >> 8), (uint8_t)v }; o_put(b, t, 2); }
static void o32(ob *b, unsigned v) { o16(b, v >> 16); o16(b, v & 0xFFFF); }
static void o_set16(ob *b, size_t at, unsigned v) { b->p[at] = (uint8_t)(v >> 8); b->p[at + 1] = (uint8_t)v; }
static void o_set32(ob *b, size_t at, unsigned v) { o_set16(b, at, v >> 16); o_set16(b, at + 2, v & 0xFFFF); }
static void o_app(ob *b, const ob *s) { o_put(b, s->p, s->n); }

void otl_init(otl_table *t) { memset(t, 0, sizeof *t); }
int otl_add_lookup(otl_table *t, int type, int cov_fmt, int nsub, int ext, const unsigned short *gids, int n) {
    otl_lookup *l = &t->lk[t->nlk];
    memset(l, 0, sizeof *l);
    l->type = type; l->cov_fmt = cov_fmt; l->nsub = nsub; l->ext = ext; l->n = n;
    memcpy(l->gids, gids, n * sizeof *gids);
    return t->nlk++;
}
int otl_add_feature(otl_table *t, const char *tag, int nlk, ...) {
    otl_feature *f = &t->ft[t->nft]; va_list ap; int i;
    memset(f, 0, sizeof *f); memcpy(f->tag, tag, 4); f->nlk = nlk;
    va_start(ap, nlk); for (i = 0; i < nlk; i++) f->lk[i] = va_arg(ap, int); va_end(ap);
    return t->nft++;
}
int otl_add_script(otl_table *t, const char *tag) {
    otl_script *s = &t->sc[t->nsc]; memset(s, 0, sizeof *s); memcpy(s->tag, tag, 4); return t->nsc++;
}
void otl_add_langsys(otl_table *t, int script, const char *lang, int req, int nf, ...) {
    otl_script *s = &t->sc[script]; otl_langsys *l = &s->ls[s->nls++]; va_list ap; int i;
    memset(l, 0, sizeof *l); memcpy(l->tag, lang, 4); l->req = req; l->nf = nf;
    va_start(ap, nf); for (i = 0; i < nf; i++) l->f[i] = va_arg(ap, int); va_end(ap);
}
UINT32 r2_tag(const char *s) { return R2_TAG(s[0], s[1], s[2], s[3]); }

static void coverage(ob *b, const unsigned short *g, int n, int fmt) {
    int i;
    if (fmt == 2) {
        int starts[96], ends[96], idx[96], nr = 0;
        for (i = 0; i < n; i++) {
            if (nr && g[i] == ends[nr - 1] + 1) { ends[nr - 1] = g[i]; continue; }
            starts[nr] = ends[nr] = g[i]; idx[nr] = i; nr++;
        }
        o16(b, 2); o16(b, nr);
        for (i = 0; i < nr; i++) { o16(b, starts[i]); o16(b, ends[i]); o16(b, idx[i]); }
    } else {
        o16(b, 1); o16(b, n);
        for (i = 0; i < n; i++) o16(b, g[i]);
    }
}
/* one subtable of lookup type `type` covering g[0..n) */
static void subtable(ob *b, int gpos, int type, const unsigned short *g, int n, int fmt) {
    int i;
    ob cov = { 0, 0, 0 }; coverage(&cov, g, n, fmt);
    if (!gpos) {
        if (type == OTL_SINGLE) {                       /* format 1: coverage, delta */
            o16(b, 1); o16(b, 6); o16(b, 1); o_app(b, &cov);   /* delta +1: a real substitution */
        } else {
            size_t hdr = 6 + 2 * (size_t)n, at = 0; ob body = { 0, 0, 0 };
            o16(b, 1); o16(b, (unsigned)hdr); o16(b, n);
            /* sets laid out after the header, coverage after the sets */
            for (i = 0; i < n; i++) {
                size_t setoff = hdr + body.n;
                o16(b, (unsigned)setoff);
                if (type == OTL_MULTIPLE) { o16(&body, 2); o16(&body, g[i]); o16(&body, 1); }   /* Sequence: glyph + space */
                else if (type == OTL_ALTERNATE) { o16(&body, 1); o16(&body, g[i] + 1); }   /* AlternateSet: 1 alternate */
                else { /* ligature set: 1 ligature at +4, Ligature { ligGlyph, compCount 2, comp[1] = 3 } */
                    o16(&body, 1); o16(&body, 4); o16(&body, g[i]); o16(&body, 2); o16(&body, 3);
                }
            }
            at = b->n; (void)at;
            o_app(b, &body);
            /* patch coverage offset (field at byte 2 of the subtable) */
            { size_t covoff = b->n; o_set16(b, 2, (unsigned)covoff); o_app(b, &cov); }
            free(body.p);
        }
    } else {
        if (type == OTL_SINGLE) {                       /* SinglePos format 1: coverage, valueFormat XAdvance, value */
            o16(b, 1); o16(b, 8); o16(b, 0x0004); o16(b, (unsigned)(short)-10); o_app(b, &cov);
        } else {                                        /* PairPos format 1 */
            size_t hdr = 10 + 2 * (size_t)n; ob body = { 0, 0, 0 };
            o16(b, 1); o16(b, 0); o16(b, 0x0004); o16(b, 0); o16(b, n);
            for (i = 0; i < n; i++) {
                o16(b, (unsigned)(hdr + body.n));
                o16(&body, 1); o16(&body, 3); o16(&body, (unsigned)(short)-20);            /* PairSet: 1 pair (second glyph 3, XAdvance -20) */
            }
            o_app(b, &body);
            { size_t covoff = b->n; o_set16(b, 2, (unsigned)covoff); o_app(b, &cov); }
            free(body.p);
        }
    }
    free(cov.p);
}
static int cmp_tag(const void *a, const void *b) { return memcmp(a, b, 4); }

uint8_t *otl_build(const otl_table *t, int gpos, size_t *size) {
    ob out = { 0, 0, 0 }, sl = { 0, 0, 0 }, fl = { 0, 0, 0 }, ll = { 0, 0, 0 };
    int i, j, k;
    /* ---- ScriptList (records sorted by tag) ---- */
    {
        int order[OTL_MAX_SC]; ob body = { 0, 0, 0 };
        for (i = 0; i < t->nsc; i++) order[i] = i;
        for (i = 0; i < t->nsc; i++) for (j = i + 1; j < t->nsc; j++) if (memcmp(t->sc[order[j]].tag, t->sc[order[i]].tag, 4) < 0) { int x = order[i]; order[i] = order[j]; order[j] = x; }
        o16(&sl, t->nsc);
        size_t hdr = 2 + 6 * (size_t)t->nsc;
        for (i = 0; i < t->nsc; i++) {
            const otl_script *s = &t->sc[order[i]]; ob sb = { 0, 0, 0 }, lsb = { 0, 0, 0 };
            int lorder[6], nl = 0, has_def = -1;
            for (j = 0; j < s->nls; j++) { if (!memcmp(s->ls[j].tag, "dflt", 4)) has_def = j; else lorder[nl++] = j; }
            for (j = 0; j < nl; j++) for (k = j + 1; k < nl; k++) if (memcmp(s->ls[lorder[k]].tag, s->ls[lorder[j]].tag, 4) < 0) { int x = lorder[j]; lorder[j] = lorder[k]; lorder[k] = x; }
            size_t shdr = 4 + 6 * (size_t)nl;
            /* LangSys tables */
            if (has_def >= 0) {
                const otl_langsys *l = &s->ls[has_def];
                o16(&sb, (unsigned)(shdr + lsb.n));
                o16(&lsb, 0); o16(&lsb, l->req < 0 ? 0xFFFF : (unsigned)l->req); o16(&lsb, l->nf); for (k = 0; k < l->nf; k++) o16(&lsb, l->f[k]);
            } else o16(&sb, 0);
            o16(&sb, nl);
            for (j = 0; j < nl; j++) {
                const otl_langsys *l = &s->ls[lorder[j]];
                o_put(&sb, l->tag, 4); o16(&sb, (unsigned)(shdr + lsb.n));
                o16(&lsb, 0); o16(&lsb, l->req < 0 ? 0xFFFF : (unsigned)l->req); o16(&lsb, l->nf); for (k = 0; k < l->nf; k++) o16(&lsb, l->f[k]);
            }
            o_app(&sb, &lsb);
            o_put(&sl, s->tag, 4); o16(&sl, (unsigned)(hdr + body.n));
            o_app(&body, &sb);
            free(sb.p); free(lsb.p);
        }
        o_app(&sl, &body); free(body.p);
    }
    /* ---- FeatureList (given order) ---- */
    {
        ob body = { 0, 0, 0 }; size_t hdr = 2 + 6 * (size_t)t->nft;
        o16(&fl, t->nft);
        for (i = 0; i < t->nft; i++) {
            const otl_feature *f = &t->ft[i];
            o_put(&fl, f->tag, 4); o16(&fl, (unsigned)(hdr + body.n));
            o16(&body, 0); o16(&body, f->nlk); for (j = 0; j < f->nlk; j++) o16(&body, f->lk[j]);
        }
        o_app(&fl, &body); free(body.p);
    }
    /* ---- LookupList ---- */
    {
        ob body = { 0, 0, 0 }; size_t hdr = 2 + 2 * (size_t)t->nlk;
        o16(&ll, t->nlk);
        for (i = 0; i < t->nlk; i++) {
            const otl_lookup *l = &t->lk[i]; int ns = l->nsub ? l->nsub : 1, per = (l->n + ns - 1) / ns, s;
            ob lb = { 0, 0, 0 }, subs = { 0, 0, 0 }; size_t lhdr = 6 + 2 * (size_t)ns;
            int ltype = l->ext ? (gpos ? 9 : 7) : l->type;
            o16(&lb, ltype); o16(&lb, 0); o16(&lb, ns);
            for (s = 0; s < ns; s++) {
                int from = s * per, cnt = l->n - from < per ? l->n - from : per;
                ob sb = { 0, 0, 0 };
                if (cnt < 0) cnt = 0;
                o16(&lb, (unsigned)(lhdr + subs.n));
                if (l->ext) {
                    ob real = { 0, 0, 0 };
                    subtable(&real, gpos, l->type, l->gids + from, cnt, l->cov_fmt);
                    o16(&sb, 1); o16(&sb, l->type); o32(&sb, 8);
                    o_app(&sb, &real); free(real.p);
                } else subtable(&sb, gpos, l->type, l->gids + from, cnt, l->cov_fmt);
                o_app(&subs, &sb); free(sb.p);
            }
            o_app(&lb, &subs);
            o16(&ll, (unsigned)(hdr + body.n));
            o_app(&body, &lb); free(lb.p); free(subs.p);
        }
        o_app(&ll, &body); free(body.p);
    }
    o32(&out, 0x00010000);
    o16(&out, 10); o16(&out, (unsigned)(10 + sl.n)); o16(&out, (unsigned)(10 + sl.n + fl.n));
    o_app(&out, &sl); o_app(&out, &fl); o_app(&out, &ll);
    free(sl.p); free(fl.p); free(ll.p);
    (void)o_set32; (void)cmp_tag;
    *size = out.n;
    return out.p;
}
