/* Pure-C model of the Windows 10 DirectWrite family matcher (no Windows dependencies; also used by the host-side table generator). */
#ifndef FM_MODEL_H
#define FM_MODEL_H
#define FM_MAXF 128
/* st: DWRITE_FONT_STYLE_NORMAL 0, OBLIQUE 1, ITALIC 2 */
typedef struct { int w, wd, st; } fm_face;
typedef struct { int idx; int sims; } fm_pick;
/* Straight transcription of FindNearestMatchingWwsFont, ApplySimulationsForWwsFallback, MatchingFont::MatchingFont and
 * MatchingStyle::IsBetterMatch. faces[] in family index order. sims: 1 = DWRITE_FONT_SIMULATIONS_BOLD, 2 = OBLIQUE. */
fm_pick fm_oracle_first(const fm_face *faces, int n, int rw, int rwd, int rst);
int     fm_oracle_list(const fm_face *faces, int n, int rw, int rwd, int rst, fm_pick *out);
/* 1 when two faces are at the same distance from the request (a pure tie of the distance function) */
int     fm_oracle_first_equal(const fm_face *fs, int n, int rw, int rwd, int rst);
int     fm_oracle_first_tied(const fm_face *faces, int n, int rw, int rwd, int rst);
int     fm_oracle_list_tied(const fm_face *faces, int n, int rw, int rwd, int rst);
/* triple an IDWriteFont reports for face f when created with simulations sims */
fm_face fm_reported(const fm_face *f, int sims);
#endif
