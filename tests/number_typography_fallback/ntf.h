/* Shared fixtures for the number_typography_fallback area: IDWriteNumberSubstitution, IDWriteTypography,
 * IDWriteFontFallback / IDWriteFontFallback1 / IDWriteFontFallbackBuilder.
 *
 * A private collection (isolated factory) holds deterministic synthetic fonts, all UPM 2048, every letter advance 1000:
 *   "NtfMain"  ASCII (default glyph ids: 'A'=2.., 'a'=28.., '0'=54..), Arabic-Indic digits U+0660..0669 -> gids 2..11,
 *              extended Arabic-Indic U+06F0..06F9 -> gids 38..47, U+066A/066B/066C -> 64/65/66, kern A,V = -200,
 *              GSUB: liga "ab"->67 (advance 500), dlig "cd"->64, smcp a->65 b->64, c2sc A->64, ss01 b->68, ss02 b->64
 *   "NtfJa"    U+0020, hiragana U+3042 U+3044 U+3046, ideographs U+4E00..4E02
 *   "NtfSym"   U+0020, U+2603 U+2605
 *   "NtfEmoji" U+0020, U+1F600 U+1F601 (format 12 only)
 *   "NtfAr"    U+0020, Arabic letters U+0627..U+062A
 * None of NtfJa/NtfSym/NtfEmoji/NtfAr covers ASCII letters, NtfMain covers none of their ranges.
 */
#ifndef NTF_H
#define NTF_H
#include "dwx.h"
#include <wchar.h>

#define NTF_MAIN  L"NtfMain"
#define NTF_JA    L"NtfJa"
#define NTF_SYM   L"NtfSym"
#define NTF_EMOJI L"NtfEmoji"
#define NTF_AR    L"NtfAr"

typedef struct ntf_fx {
    IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll;
} ntf_fx;
int  ntf_open(ntf_fx *x);
void ntf_close(ntf_fx *x);
#define NTF_BEGIN() ntf_fx X; memset(&X, 0, sizeof X); if (!ntf_open(&X)) { ntf_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define NTF_END()   ntf_close(&X)

/* A factory2 view of the fixture factory (AddRef'd; NULL when unsupported). */
IDWriteFactory2 *ntf_f2(IDWriteFactory *f);

/* Family name (en-us else first) of an IDWriteFont, UTF-8. */
void ntf_font_family(IDWriteFont *font, char *buf, int cap);
void ntf_face_family(IDWriteFontFace5 *face, char *buf, int cap);

/* MapCharacters (v0) over a fresh analysis source; text is not copied. */
HRESULT ntf_map(IDWriteFontFallback *fb, const WCHAR *text, UINT32 textlen, UINT32 pos, UINT32 len, IDWriteFontCollection *base,
                const WCHAR *family, UINT32 *mapped_len, IDWriteFont **font, float *scale, dwx_ana_src **src_out);

IDWriteTextFormat *ntf_format(ntf_fx *x, const WCHAR *family, float size);
IDWriteTextLayout *ntf_layout(ntf_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h);

/* Draw the layout into a fresh recording renderer (caller releases with dwx_release). */
dwx_renderer *ntf_draw(IDWriteTextLayout *l);
/* All glyph ids of all glyph runs in call order; returns the count (<= cap). */
int ntf_glyphs(dwx_renderer *r, unsigned short *out, int cap);
/* Family of the face of the n-th glyph run, UTF-8 (empty when absent). */
void ntf_run_family(dwx_renderer *r, int n, char *buf, int cap);

/* Font face of the first font of a fixture/system family; NULL when absent. */
IDWriteFontFace *ntf_face(IDWriteFontCollection *coll, const WCHAR *family);
/* IDWriteTextAnalyzer::GetGlyphs over the whole text with one script analysis (from AnalyzeScript on the same text).
 * typo may be NULL; returns the HRESULT, *nglyphs = actualGlyphCount. out/cluster capacity 64. */
HRESULT ntf_shape(IDWriteFactory *f, IDWriteFontFace *face, const WCHAR *text, const WCHAR *locale, IDWriteNumberSubstitution *ns,
                  IDWriteTypography *typo, unsigned short *out, unsigned short *cluster, UINT32 *nglyphs);
/* QueryInterface every public IID: those in allowed[] (n entries) must succeed, all others must fail with E_NOINTERFACE and NULL. */
void ntf_qi_only(IUnknown *obj, const IID *const *allowed, int n, const char *file, int line);
#define NTF_QI_ONLY(obj, ...) do { static const IID *const a_[] = { __VA_ARGS__ }; ntf_qi_only((IUnknown *)(obj), a_, (int)(sizeof a_ / sizeof a_[0]), __FILE__, __LINE__); } while (0)

#define NTF_RNG(s, l) ((DWRITE_TEXT_RANGE){ (s), (l) })
#define NTF_ADV(units, size) ((float)(units) * (float)(size) / 2048.0f)
#endif
