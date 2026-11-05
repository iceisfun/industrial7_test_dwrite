/* Shared fixtures for the font family / font matching area.
 *
 * Families are served from a custom font collection built from synthetic TrueType blobs whose OS/2 usWeightClass,
 * usWidthClass and fsSelection are chosen by the test, so every expectation is exact and independent of installed fonts.
 *
 * The matching oracle (fm_oracle_*) is a straight transcription of the Windows 10 matcher:
 *   FontSet::FindNearestMatchingWwsFont / ApplySimulationsForWwsFallback / MatchingFont::MatchingFont / MatchingStyle::IsBetterMatch.
 */
#ifndef FM_H
#define FM_H
#include "dwx.h"

#include "fm_model.h"
typedef void (*fm_tweak)(ttf_spec *s, int i);

typedef struct fm {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; IDWriteFontFamily *fam;
    int n;                          /* faces requested */
    fm_face spec[FM_MAXF];
    UINT32 count;                   /* IDWriteFontFamily::GetFontCount */
    fm_face obs[FM_MAXF];           /* family index -> triple reported by the IDWriteFont */
    int obs_sims[FM_MAXF];
    int src[FM_MAXF];               /* family index -> spec index (== loader key), -1 when unknown */
    char name[64];
} fm;

/* Builds the collection and fetches the family. Returns 1 on success. On failure reports a failed CHECK (or a skip when
 * DWriteCreateFactory is unavailable), cleans up and returns 0. Always call fm_close() when 1 was returned. */
int  fm_open(fm *m, const char *family, const fm_face *faces, int n, fm_tweak tw);
void fm_close(fm *m);
#define FM_OPEN(m, fam, arr) fm m; if (!fm_open(&m, fam, arr, (int)(sizeof(arr) / sizeof((arr)[0])), NULL)) return
#define FM_OPEN_TW(m, fam, arr, tw) fm m; if (!fm_open(&m, fam, arr, (int)(sizeof(arr) / sizeof((arr)[0])), tw)) return
#define FM_N(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* family index of the face with this triple, -1 when absent */
int fm_find(const fm *m, int w, int wd, int st);
const char *fm_req_name(int w, int wd, int st);


/* Compare an IDWriteFont with the expected face + simulations (weight/stretch/style/simulations, family identity). */
void fm_check_font(fm *m, IDWriteFont *font, const fm_face *face, int sims, const char *tag);
/* Run GetFirstMatchingFont and compare with the expected family index + simulations. */
void fm_expect_first(fm *m, int rw, int rwd, int rst, int fam_idx, int sims);
/* Run GetFirstMatchingFont and GetMatchingFonts and compare both with the oracle. full_list: also verify the whole order. */
void fm_check_request(fm *m, int rw, int rwd, int rst, int full_list);
/* Reads all attributes of the font at family index i (via IDWriteFontFamily::GetFont). */
int  fm_read_font(IDWriteFont *font, fm_face *out, int *sims);
#endif
