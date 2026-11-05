#include "fm_model.h"

/* ---- matcher model --------------------------------------------------------------------------- */
typedef struct { int idx, flags, ds, dt, dw; } mfont;

/* MatchingFont::MatchingFont(requestWeight, requestStyle, entry): flag 1 = simulate bold, flag 2 = simulate oblique;
 * distances are stretch*1100, style*700, weight*5 relative to the DWRITE defaults (5, 0, 400). */
static mfont mkfont(const fm_face *fs, int idx, int rw, int rst) {
    const fm_face *f = &fs[idx]; mfont m; int fl = 0, sty, we;
    if (rw > 0x226 && f->w < 0x227) fl = (f->w < rw - 100);
    if (rst != 0 && f->st == 0) fl |= 2;
    sty = (fl & 2) ? 1 : f->st;
    we = f->w; if ((fl & 1) && we < 700) we = 700;
    m.idx = idx; m.flags = fl;
    m.ds = (f->wd - 5) * 0x44c; m.dt = sty * 700; m.dw = (we - 400) * 5;
    return m;
}
/* MatchingStyle::IsBetterMatch(a, b) with request vector (rs, rt, rw) */
static int better(const fm_face *fs, const mfont *a, const mfont *b, int rs, int rt, int rw) {
    const fm_face *fa = &fs[a->idx], *fb = &fs[b->idx];
    long long da, db;
    if (a->flags != b->flags && fa->wd == fb->wd) {
        int u14 = a->flags & 2;
        if (u14 != (b->flags & 2)) {
            int wa = fa->w, wb = fb->w;
            if ((a->flags & 1) && wa < 700) wa = 700;
            if ((b->flags & 1) && wb < 700) wb = 700;
            if (wa == wb) {
                if (u14 == 0) { if (fa->st == 1) return 1; }
                else if (fb->st == 1) return 0;
            }
        }
        if ((a->flags & 1) != (b->flags & 1)) {
            int ca = u14 == 0 ? fa->st : 1;
            int cb = (b->flags & 2) == 0 ? fb->st : 1;
            if (ca == cb) {
                if ((a->flags & 1) == 0) { if (fa->w > 0x226) return 1; }
                else if (fb->w > 0x226) return 0;
            }
        }
    }
    da = (long long)(a->ds - rs) * (a->ds - rs) + (long long)(a->dt - rt) * (a->dt - rt) + (long long)(a->dw - rw) * (a->dw - rw);
    db = (long long)(b->ds - rs) * (b->ds - rs) + (long long)(b->dt - rt) * (b->dt - rt) + (long long)(b->dw - rw) * (b->dw - rw);
    if (da != db) return da < db;
    da = (long long)rs * a->ds + (long long)rt * a->dt + (long long)rw * a->dw;
    db = (long long)rs * b->ds + (long long)rt * b->dt + (long long)rw * b->dw;
    if (da != db) return da > db;
    if (a->ds != b->ds) return a->ds > b->ds;
    if (a->dt != b->dt) return a->dt > b->dt;
    return a->dw > b->dw;
}

fm_pick fm_oracle_first(const fm_face *fs, int n, int rw, int rwd, int rst) {
    fm_pick p = { -1, 0 }; int i, best = 0; mfont mb;
    int rs = (rwd - 5) * 0x44c, rt = rst * 700, rwt = (rw - 400) * 5;
    if (n <= 0) return p;
    for (i = 0; i < n; i++)
        if (fs[i].w == rw && fs[i].wd == rwd && fs[i].st == rst) { p.idx = i; p.sims = 0; return p; }   /* exact match short-cut */
    mb = mkfont(fs, 0, rw, rst);
    for (i = 1; i < n; i++) {
        mfont c = mkfont(fs, i, rw, rst);
        if (better(fs, &c, &mb, rs, rt, rwt)) { mb = c; best = i; }
    }
    p.idx = best; p.sims = mb.flags;
    return p;
}

int fm_oracle_list(const fm_face *fs, int n, int rw, int rwd, int rst, fm_pick *out) {
    mfont a[FM_MAXF]; int i, j;
    int rs = (rwd - 5) * 0x44c, rt = rst * 700, rwt = (rw - 400) * 5;
    for (i = 0; i < n; i++) a[i] = mkfont(fs, i, rw, rst);
    for (i = 1; i < n; i++) {
        mfont v = a[i];
        for (j = i; j > 0 && better(fs, &v, &a[j - 1], rs, rt, rwt); j--) a[j] = a[j - 1];
        a[j] = v;
    }
    for (i = 0; i < n; i++) { out[i].idx = a[i].idx; out[i].sims = a[i].flags; }
    return n;
}

fm_face fm_reported(const fm_face *f, int sims) {
    fm_face r = *f;
    if (sims & 1) r.w = 700;
    if (sims & 2) r.st = 1;
    return r;
}


/* 1 when some other face is indistinguishable from the winner of GetFirstMatchingFont (result then depends on enumeration order) */
int fm_oracle_first_tied(const fm_face *fs, int n, int rw, int rwd, int rst) {
    fm_pick p = fm_oracle_first(fs, n, rw, rwd, rst); int j;
    int rs = (rwd - 5) * 0x44c, rt = rst * 700, rwt = (rw - 400) * 5;
    mfont a;
    if (p.idx < 0) return 0;
    a = mkfont(fs, p.idx, rw, rst);
    for (j = 0; j < n; j++) {
        mfont b;
        if (j == p.idx) continue;
        b = mkfont(fs, j, rw, rst);
        if (!better(fs, &a, &b, rs, rt, rwt) && !better(fs, &b, &a, rs, rt, rwt)) return 1;
    }
    return 0;
}
/* 1 when two faces are indistinguishable anywhere in the list: the order of the list is then enumeration-dependent */
int fm_oracle_list_tied(const fm_face *fs, int n, int rw, int rwd, int rst) {
    int i, j; int rs = (rwd - 5) * 0x44c, rt = rst * 700, rwt = (rw - 400) * 5;
    for (i = 0; i < n; i++) for (j = i + 1; j < n; j++) {
        mfont a = mkfont(fs, i, rw, rst), b = mkfont(fs, j, rw, rst);
        if (!better(fs, &a, &b, rs, rt, rwt) && !better(fs, &b, &a, rs, rt, rwt)) return 1;
    }
    return 0;
}
