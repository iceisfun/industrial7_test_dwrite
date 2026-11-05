#include "otb.h"
#include <stdlib.h>
#include <string.h>

typedef struct { uint8_t *p; size_t n, cap; } ob;
static void bgrow(ob *o, size_t k) { if (o->n + k > o->cap) { o->cap = (o->n + k) * 2 + 64; o->p = (uint8_t *)realloc(o->p, o->cap); } }
static void b16(ob *o, unsigned v) { bgrow(o, 2); o->p[o->n++] = (uint8_t)(v >> 8); o->p[o->n++] = (uint8_t)v; }
static void b32(ob *o, uint32_t v) { b16(o, v >> 16); b16(o, v & 0xFFFF); }
static void bat(ob *o, size_t at, unsigned v) { o->p[at] = (uint8_t)(v >> 8); o->p[at + 1] = (uint8_t)v; }
static void bapp(ob *o, const ob *x) { bgrow(o, x->n); memcpy(o->p + o->n, x->p, x->n); o->n += x->n; }
static otb_blob fin(ob *o) { otb_blob b; b.p = o->p; b.n = o->n; return b; }
void otb_free(otb_blob *b) { free(b->p); b->p = NULL; b->n = 0; }

static int cmp16(const void *a, const void *b) { return (int)*(const unsigned short *)a - (int)*(const unsigned short *)b; }
/* coverage table format 1 for the (unsorted) glyph list; the sorted order is the coverage index order */
static void cov(ob *o, const unsigned short *g, int n)
{
    unsigned short *s = (unsigned short *)malloc((n ? n : 1) * 2); int i;
    memcpy(s, g, n * 2); qsort(s, n, 2, cmp16);
    b16(o, 1); b16(o, n); for (i = 0; i < n; i++) b16(o, s[i]);
    free(s);
}
static int cov_index(const unsigned short *g, int n, unsigned short x)
{
    int i, r = 0; for (i = 0; i < n; i++) if (g[i] < x) r++; return r;
}

otb_blob otb_gsub1_list(const otb_s1 *m, int n)
{
    ob o = { 0 }, c = { 0 }; unsigned short *g = (unsigned short *)malloc(n * 2); unsigned short *out = (unsigned short *)calloc(n, 2); int i;
    for (i = 0; i < n; i++) g[i] = m[i].from;
    for (i = 0; i < n; i++) out[cov_index(g, n, m[i].from)] = m[i].to;
    cov(&c, g, n);
    b16(&o, 2); b16(&o, 6 + 2 * n); b16(&o, n); for (i = 0; i < n; i++) b16(&o, out[i]);
    bapp(&o, &c); free(c.p); free(g); free(out); return fin(&o);
}
otb_blob otb_gsub1_delta(const unsigned short *gids, int n, int delta)
{
    ob o = { 0 }, c = { 0 }; cov(&c, gids, n);
    b16(&o, 1); b16(&o, 6); b16(&o, (unsigned)delta & 0xFFFF); bapp(&o, &c); free(c.p); return fin(&o);
}
/* type 2 and 3 share the layout: coverage -> offsets to {count, glyphs} sets */
static otb_blob seqsets(const otb_seq *m, int n)
{
    ob o = { 0 }, c = { 0 }, sets = { 0 }; unsigned short *g = (unsigned short *)malloc(n * 2); int i, k;
    int *idx = (int *)malloc(n * sizeof(int));
    for (i = 0; i < n; i++) g[i] = m[i].from;
    for (i = 0; i < n; i++) idx[cov_index(g, n, m[i].from)] = i;
    cov(&c, g, n);
    size_t base = 6 + 2 * (size_t)n + c.n;
    b16(&o, 1); b16(&o, 6 + 2 * n); b16(&o, n);
    for (i = 0; i < n; i++) { b16(&o, (unsigned)(base + sets.n)); b16(&sets, m[idx[i]].n); for (k = 0; k < m[idx[i]].n; k++) b16(&sets, m[idx[i]].out[k]); }
    bapp(&o, &c); bapp(&o, &sets); free(c.p); free(sets.p); free(g); free(idx); return fin(&o);
}
otb_blob otb_gsub2(const otb_seq *m, int n) { return seqsets(m, n); }
otb_blob otb_gsub3(const otb_seq *m, int n) { return seqsets(m, n); }

otb_blob otb_gsub4(const otb_lig *m, int n)
{
    ob o = { 0 }, c = { 0 }; unsigned short *firsts = (unsigned short *)malloc(n * 2); int nf = 0, i, j, k;
    for (i = 0; i < n; i++) { int dup = 0; for (j = 0; j < nf; j++) if (firsts[j] == m[i].comps[0]) dup = 1; if (!dup) firsts[nf++] = m[i].comps[0]; }
    qsort(firsts, nf, 2, cmp16);
    cov(&c, firsts, nf);
    ob sets = { 0 };
    size_t setsbase = 6 + 2 * (size_t)nf + c.n;
    b16(&o, 1); b16(&o, 6 + 2 * nf); b16(&o, nf);
    for (j = 0; j < nf; j++) {
        ob set = { 0 }, ligs = { 0 }; int cnt = 0;
        for (i = 0; i < n; i++) if (m[i].comps[0] == firsts[j]) cnt++;
        b16(&set, cnt);
        size_t lbase = 2 + 2 * (size_t)cnt;
        for (i = 0; i < n; i++) if (m[i].comps[0] == firsts[j]) {
            b16(&set, (unsigned)(lbase + ligs.n)); b16(&ligs, m[i].lig); b16(&ligs, m[i].ncomp);
            for (k = 1; k < m[i].ncomp; k++) b16(&ligs, m[i].comps[k]);
        }
        bapp(&set, &ligs);
        b16(&o, (unsigned)(setsbase + sets.n)); bapp(&sets, &set);
        free(set.p); free(ligs.p);
    }
    bapp(&o, &c); bapp(&o, &sets); free(c.p); free(sets.p); free(firsts); return fin(&o);
}

otb_blob otb_gsub6(const otb_glyphset *back, int nback, const otb_glyphset *in, int nin,
                   const otb_glyphset *ahead, int nahead, const otb_seqrec *rec, int nrec)
{
    ob o = { 0 }, covs = { 0 }; int i, total = nback + nin + nahead;
    size_t hdr = 2 + 2 + 2 * (size_t)nback + 2 + 2 * (size_t)nin + 2 + 2 * (size_t)nahead + 2 + 4 * (size_t)nrec;
    b16(&o, 3);
    b16(&o, nback); for (i = 0; i < nback; i++) { b16(&o, (unsigned)(hdr + covs.n)); cov(&covs, back[i].g, back[i].n); }
    b16(&o, nin); for (i = 0; i < nin; i++) { b16(&o, (unsigned)(hdr + covs.n)); cov(&covs, in[i].g, in[i].n); }
    b16(&o, nahead); for (i = 0; i < nahead; i++) { b16(&o, (unsigned)(hdr + covs.n)); cov(&covs, ahead[i].g, ahead[i].n); }
    b16(&o, nrec); for (i = 0; i < nrec; i++) { b16(&o, rec[i].seq); b16(&o, rec[i].lookup); }
    (void)total; bapp(&o, &covs); free(covs.p); return fin(&o);
}

static int vsize(unsigned vf) { int k = 0; unsigned b; for (b = 1; b <= 8; b <<= 1) if (vf & b) k++; return k; }
static void putval(ob *o, unsigned vf, const otb_val *v) { unsigned b; int i = 0; for (b = 1; b <= 8; b <<= 1, i++) if (vf & b) b16(o, (unsigned)v->v[i] & 0xFFFF); }

otb_blob otb_gpos1_one(const unsigned short *gids, int n, unsigned vf, otb_val v)
{
    ob o = { 0 }, c = { 0 }; cov(&c, gids, n);
    b16(&o, 1); b16(&o, (unsigned)(6 + 2 * vsize(vf))); b16(&o, vf); putval(&o, vf, &v); bapp(&o, &c); free(c.p); return fin(&o);
}
otb_blob otb_gpos1_each(const unsigned short *gids, int n, unsigned vf, const otb_val *v)
{
    ob o = { 0 }, c = { 0 }; int i; otb_val *sorted = (otb_val *)calloc(n, sizeof *sorted);
    for (i = 0; i < n; i++) sorted[cov_index(gids, n, gids[i])] = v[i];
    cov(&c, gids, n);
    b16(&o, 2); b16(&o, (unsigned)(8 + 2 * vsize(vf) * n)); b16(&o, vf); b16(&o, n);
    for (i = 0; i < n; i++) putval(&o, vf, &sorted[i]);
    bapp(&o, &c); free(c.p); free(sorted); return fin(&o);
}

otb_blob otb_gpos2_pairs(const otb_pair *p, int n)
{
    ob o = { 0 }, c = { 0 }, sets = { 0 }; unsigned short *firsts = (unsigned short *)malloc(n * 2); int nf = 0, i, j; unsigned vf2 = 0;
    for (i = 0; i < n; i++) { int dup = 0; for (j = 0; j < nf; j++) if (firsts[j] == p[i].l) dup = 1; if (!dup) firsts[nf++] = p[i].l; if (p[i].xadv2) vf2 = 4; }
    qsort(firsts, nf, 2, cmp16);
    cov(&c, firsts, nf);
    size_t base = 10 + 2 * (size_t)nf + c.n;
    int recsz = 2 + 2 + (vf2 ? 2 : 0);
    b16(&o, 1); b16(&o, 10 + 2 * nf); b16(&o, 4); b16(&o, vf2); b16(&o, nf);
    for (j = 0; j < nf; j++) {
        /* pair records sorted by second glyph */
        int cnt = 0, *ix = (int *)malloc(n * sizeof(int)), a, b;
        for (i = 0; i < n; i++) if (p[i].l == firsts[j]) ix[cnt++] = i;
        for (a = 0; a < cnt; a++) for (b = a + 1; b < cnt; b++) if (p[ix[b]].r < p[ix[a]].r) { int t = ix[a]; ix[a] = ix[b]; ix[b] = t; }
        b16(&o, (unsigned)(base + sets.n)); b16(&sets, cnt);
        for (a = 0; a < cnt; a++) { b16(&sets, p[ix[a]].r); b16(&sets, (unsigned)p[ix[a]].xadv1 & 0xFFFF); if (vf2) b16(&sets, (unsigned)p[ix[a]].xadv2 & 0xFFFF); }
        free(ix);
    }
    (void)recsz; bapp(&o, &c); bapp(&o, &sets); free(c.p); free(sets.p); free(firsts); return fin(&o);
}

static void classdef(ob *o, const otb_cls *c, int n)
{
    unsigned short *g = (unsigned short *)malloc(n * 2); int i, k, cnt = 0; unsigned short *cl = (unsigned short *)malloc(n * 2);
    for (i = 0; i < n; i++) g[i] = c[i].gid;
    qsort(g, n, 2, cmp16);
    for (i = 0; i < n; i++) for (k = 0; k < n; k++) if (c[k].gid == g[i]) cl[i] = c[k].cls;
    b16(o, 2);
    /* one range per glyph keeps the builder trivial */
    b16(o, n); for (i = 0; i < n; i++) { b16(o, g[i]); b16(o, g[i]); b16(o, cl[i]); }
    (void)cnt; free(g); free(cl);
}
otb_blob otb_gpos2_class(const otb_cls *first, int nf, const otb_cls *second, int ns, int ncls1, int ncls2, const short *xadv)
{
    ob o = { 0 }, c = { 0 }, cd1 = { 0 }, cd2 = { 0 }; unsigned short *g = (unsigned short *)malloc(nf * 2); int i, j;
    for (i = 0; i < nf; i++) g[i] = first[i].gid;
    cov(&c, g, nf); classdef(&cd1, first, nf); classdef(&cd2, second, ns);
    size_t hdr = 16 + 2 * (size_t)ncls1 * ncls2;
    b16(&o, 2); b16(&o, (unsigned)hdr); b16(&o, 4); b16(&o, 0);
    b16(&o, (unsigned)(hdr + c.n)); b16(&o, (unsigned)(hdr + c.n + cd1.n)); b16(&o, ncls1); b16(&o, ncls2);
    for (i = 0; i < ncls1; i++) for (j = 0; j < ncls2; j++) b16(&o, (unsigned)xadv[i * ncls2 + j] & 0xFFFF);
    bapp(&o, &c); bapp(&o, &cd1); bapp(&o, &cd2); free(c.p); free(cd1.p); free(cd2.p); free(g); return fin(&o);
}

otb_blob otb_gpos_mark(const otb_mark *marks, int nm, const otb_base *bases, int nb, int nclass)
{
    ob o = { 0 }, mc = { 0 }, bc = { 0 }, ma = { 0 }, ba = { 0 }; int i, k;
    unsigned short *mg = (unsigned short *)malloc(nm * 2), *bg = (unsigned short *)malloc(nb * 2);
    for (i = 0; i < nm; i++) mg[i] = marks[i].gid;
    for (i = 0; i < nb; i++) bg[i] = bases[i].gid;
    cov(&mc, mg, nm); cov(&bc, bg, nb);
    /* MarkArray: count, {class, anchorOffset from MarkArray}, then anchors (format 1: 6 bytes each) in coverage order */
    int *mi = (int *)malloc(nm * sizeof(int)), *bi = (int *)malloc(nb * sizeof(int));
    for (i = 0; i < nm; i++) mi[cov_index(mg, nm, mg[i])] = i;
    for (i = 0; i < nb; i++) bi[cov_index(bg, nb, bg[i])] = i;
    b16(&ma, nm);
    for (i = 0; i < nm; i++) { b16(&ma, marks[mi[i]].cls); b16(&ma, 2 + 4 * nm + 6 * i); }
    for (i = 0; i < nm; i++) { b16(&ma, 1); b16(&ma, (unsigned)marks[mi[i]].a.x & 0xFFFF); b16(&ma, (unsigned)marks[mi[i]].a.y & 0xFFFF); }
    /* BaseArray: count, {anchorOffset per class}, anchors */
    b16(&ba, nb);
    for (i = 0; i < nb; i++) for (k = 0; k < nclass; k++) b16(&ba, 2 + 2 * nclass * nb + 6 * (i * nclass + k));
    for (i = 0; i < nb; i++) for (k = 0; k < nclass; k++) { b16(&ba, 1); b16(&ba, (unsigned)bases[bi[i]].a[k].x & 0xFFFF); b16(&ba, (unsigned)bases[bi[i]].a[k].y & 0xFFFF); }
    size_t hdr = 12;
    b16(&o, 1); b16(&o, (unsigned)hdr); b16(&o, (unsigned)(hdr + mc.n)); b16(&o, nclass);
    b16(&o, (unsigned)(hdr + mc.n + bc.n)); b16(&o, (unsigned)(hdr + mc.n + bc.n + ma.n));
    bapp(&o, &mc); bapp(&o, &bc); bapp(&o, &ma); bapp(&o, &ba);
    free(mc.p); free(bc.p); free(ma.p); free(ba.p); free(mg); free(bg); free(mi); free(bi); return fin(&o);
}

/* ---- layout assembly ---- */
static int cmp_tag(const void *a, const void *b) { return memcmp(a, b, 4); }
otb_blob otb_layout(const otb_script *sc, int nsc, const otb_feature *ft, int nft, const otb_lookup *lk, int nlk)
{
    int i, j, k; ob o = { 0 };
    /* sorted feature order: indices by ascending tag */
    int *order = (int *)malloc(nft * sizeof(int));
    for (i = 0; i < nft; i++) order[i] = i;
    for (i = 0; i < nft; i++) for (j = i + 1; j < nft; j++) if (memcmp(ft[order[j]].tag, ft[order[i]].tag, 4) < 0) { int t = order[i]; order[i] = order[j]; order[j] = t; }
#define FIDX(want_, out) do { out = -1; for (int q_ = 0; q_ < nft; q_++) if (!memcmp(ft[order[q_]].tag, (want_), 4)) { out = q_; break; } } while (0)
    /* script list */
    int *sord = (int *)malloc(nsc * sizeof(int));
    for (i = 0; i < nsc; i++) sord[i] = i;
    for (i = 0; i < nsc; i++) for (j = i + 1; j < nsc; j++) if (memcmp(sc[sord[j]].tag, sc[sord[i]].tag, 4) < 0) { int t = sord[i]; sord[i] = sord[j]; sord[j] = t; }
    ob sl = { 0 }, stabs = { 0 };
    b16(&sl, nsc);
    size_t sbase = 2 + 6 * (size_t)nsc;
    for (i = 0; i < nsc; i++) {
        const otb_script *s = &sc[sord[i]]; ob st = { 0 }, ls = { 0 };
        int nl = s->nlang, ndef = s->nf;
        size_t lbase = 4 + 6 * (size_t)nl;
        b16(&st, (unsigned)lbase); b16(&st, nl);
        /* default langsys first (right after the record array), then the named ones in tag order */
        ob d = { 0 };
        b16(&d, 0); b16(&d, 0xFFFF); b16(&d, ndef); for (j = 0; j < ndef; j++) { int fi; FIDX(s->f[j], fi); b16(&d, fi < 0 ? 0 : fi); }
        int lo[3] = { 0, 1, 2 };
        for (j = 0; j < nl; j++) for (k = j + 1; k < nl; k++) if (memcmp(s->lang[lo[k]].tag, s->lang[lo[j]].tag, 4) < 0) { int t = lo[j]; lo[j] = lo[k]; lo[k] = t; }
        ob lbody = { 0 };
        for (j = 0; j < nl; j++) {
            const otb_langsys *l = &s->lang[lo[j]]; int m;
            bgrow(&st, 6); memcpy(st.p + st.n, l->tag, 4); st.n += 4;
            b16(&st, (unsigned)(lbase + d.n + lbody.n));
            b16(&lbody, 0); b16(&lbody, 0xFFFF); b16(&lbody, l->nf); for (m = 0; m < l->nf; m++) { int fi; FIDX(l->f[m], fi); b16(&lbody, fi < 0 ? 0 : fi); }
        }
        bapp(&st, &d); bapp(&st, &lbody); free(d.p); free(lbody.p); free(ls.p);
        bgrow(&sl, 6); memcpy(sl.p + sl.n, s->tag, 4); sl.n += 4; b16(&sl, (unsigned)(sbase + stabs.n));
        bapp(&stabs, &st); free(st.p);
    }
    bapp(&sl, &stabs); free(stabs.p);
    /* feature list */
    ob fl = { 0 }, ftabs = { 0 };
    b16(&fl, nft);
    size_t fbase = 2 + 6 * (size_t)nft;
    for (i = 0; i < nft; i++) {
        const otb_feature *f = &ft[order[i]];
        bgrow(&fl, 6); memcpy(fl.p + fl.n, f->tag, 4); fl.n += 4; b16(&fl, (unsigned)(fbase + ftabs.n));
        b16(&ftabs, 0); b16(&ftabs, f->nl); for (j = 0; j < f->nl; j++) b16(&ftabs, f->lk[j]);
    }
    bapp(&fl, &ftabs); free(ftabs.p);
    /* lookup list */
    ob ll = { 0 }, ltabs = { 0 };
    b16(&ll, nlk);
    size_t lbase = 2 + 2 * (size_t)nlk;
    for (i = 0; i < nlk; i++) {
        const otb_lookup *l = &lk[i]; ob t = { 0 }, subs = { 0 };
        size_t hdr = 6 + 2 * (size_t)l->nsub;
        b16(&t, l->type); b16(&t, l->flag); b16(&t, l->nsub);
        for (j = 0; j < l->nsub; j++) { b16(&t, (unsigned)(hdr + subs.n)); bgrow(&subs, l->sub[j].n); memcpy(subs.p + subs.n, l->sub[j].p, l->sub[j].n); subs.n += l->sub[j].n; }
        bapp(&t, &subs);
        b16(&ll, (unsigned)(lbase + ltabs.n)); bapp(&ltabs, &t); free(t.p); free(subs.p);
    }
    bapp(&ll, &ltabs); free(ltabs.p);
    b32(&o, 0x00010000); b16(&o, 10); b16(&o, (unsigned)(10 + sl.n)); b16(&o, (unsigned)(10 + sl.n + fl.n));
    bapp(&o, &sl); bapp(&o, &fl); bapp(&o, &ll);
    free(sl.p); free(fl.p); free(ll.p); free(order); free(sord);
    return fin(&o);
}

otb_blob otb_gdef(const unsigned char *cls, int ng)
{
    ob o = { 0 }, cd = { 0 }; int i, nr = 0;
    ob rr = { 0 };
    for (i = 0; i < ng; ) {
        int j = i; if (!cls[i]) { i++; continue; }
        while (j + 1 < ng && cls[j + 1] == cls[i]) j++;
        b16(&rr, i); b16(&rr, j); b16(&rr, cls[i]); nr++; i = j + 1;
    }
    b16(&cd, 2); b16(&cd, nr); bapp(&cd, &rr);
    b32(&o, 0x00010000); b16(&o, 12); b16(&o, 0); b16(&o, 0); b16(&o, 0); bapp(&o, &cd);
    free(rr.p); free(cd.p); return fin(&o);
}
