/* Shared fixtures of the r2_shaping_features area: synthetic fonts with hand-built GSUB/GPOS/GDEF tables and a shaping driver.
 *
 * All fonts start from the default synthetic font (UPM 2048, letters/digits/space advance 1000, notdef 1200; 'A'..'Z' gid 2..27,
 * 'a'..'z' 28..53, '0'..'9' 54..63, space 1) and append glyphs 69... .  The shaping tests run at em 2048 so that one design unit is
 * exactly one DIP and every advance/offset is an integer.
 *
 * LATN font ("R2Latn"), extra glyphs (advance):
 *   69 fi 1500  70 ff 1400  71 ffi 2000  72 smallcap-A 800  73 smallcap-B 700  74 pnum-1 600  75 tnum-1 1000  76 ss01-a 900
 *   77 calt-b 950  78 acute (mark, 0)  79 circumflex (mark, 0)  80 e-acute 1000  81 dotless-i 500  82 xy-lig 1800  83 ck-lig 1600
 *   84 salt-A#1 1100  85 salt-A#2 1200  86 slashed-zero 1000  87 ZWSP (0)
 *   cmap adds U+0301 -> 78, U+0302 -> 79, U+00E9 -> 80, U+200B -> 87
 *   GSUB (script latn and DFLT, default langsys):
 *     ccmp  type2  80 -> 32 78                      liga  type4  f f i -> 71, f f -> 70, f i -> 69
 *     clig  type4  c k -> 83                        rlig  type4  x y -> 82
 *     calt  type6 (format 3)  backtrack [a] input [b]  -> type1 lookup b -> 77
 *     smcp  type1  a -> 72, b -> 73                 c2sc  type1  A -> 72, B -> 73
 *     ss01  type1  a -> 76                          pnum  type1  1 -> 74         tnum  type1  1 -> 75
 *     salt  type3  A -> {84, 85}                    zero  type1 (format 1, delta 32)  0 -> 86
 *     locl  type1  i -> 81, only in the latn language system 'TRK '
 *   GPOS:
 *     kern  type2  format 1: A V -100, V A -100, T o -150, A Y -80;   format 2 (classes): W {A, o} -60
 *     mark  type4  marks 78, 79 (anchor 100,0) on bases A-Z a-z (anchor 500,1400)
 *     mkmk  type6  mark 79 on mark 78 (anchors 100,0 / 100,600)
 *     cpsp  type1  format 2: A-Z xAdvance +100          ss02  type1 format 1: z xPlacement 50 yPlacement 70
 *   GDEF: 69-71 ligature, 78/79 mark, 72-77 80-86 base (class 1)
 */
#ifndef R2_H
#define R2_H
#include "dw.h"
#include "dwx.h"
#include "otb.h"
#include <wchar.h>
#include <float.h>

#define TG(a, b, c, d) ((UINT32)(a) | ((UINT32)(b) << 8) | ((UINT32)(c) << 16) | ((UINT32)(d) << 24))

enum { G_FI = 69, G_FF, G_FFI, G_SCA, G_SCB, G_P1, G_T1, G_SS01A, G_CALTB, G_ACUTE, G_CIRC, G_EACUTE, G_DOTLESSI, G_XY, G_CK, G_ALT1, G_ALT2, G_ZERO, G_ZWSP };
#define LG(c) ((UINT16)ttf_default_gid(c))     /* default-font glyph of an ASCII char */

enum { FX_LATN, FX_LATN_IGNM, FX_ARAB, FX_DEVA, FX_THAI, FX_THAI_NOAM, FX_PLAIN };

typedef struct fx {
    dwx_font h; IDWriteFontFace *face; IDWriteTextAnalyzer *an; IDWriteTextAnalyzer1 *an1; int ok;
} fx;
/* Builds the font, opens it in the shared factory and creates an analyzer.  Returns 1 on success (fx_close always safe). */
int  fx_open(fx *x, int kind);
int  fx_open_blob(fx *x, const void *blob, size_t n);
void fx_close(fx *x);
/* The blob of a font kind (malloc'd, caller frees). */
uint8_t *fx_blob(int kind, size_t *n);
#define FX_BEGIN(name, kind) fx name; if (!fx_open(&name, kind)) { fx_close(&name); t_skip("synthetic font fixture unavailable"); return; }

/* Script analysis of the first character of text (AnalyzeScript through our own source/sink). */
DWRITE_SCRIPT_ANALYSIS fx_script(fx *x, const WCHAR *text, UINT32 n);

#define SH_MAX 256
typedef struct shaped {
    HRESULT hr, hr_place; UINT32 len, actual;
    UINT16 cmap[SH_MAX]; DWRITE_SHAPING_TEXT_PROPERTIES tp[SH_MAX];
    UINT16 gid[SH_MAX]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[SH_MAX];
    float adv[SH_MAX]; DWRITE_GLYPH_OFFSET off[SH_MAX];
    DWRITE_SCRIPT_ANALYSIS sa; const WCHAR *text; const WCHAR *loc; BOOL rtl;
} shaped;

/* Feature request: ranges of text, each with its own feature list. */
typedef struct featrange { const DWRITE_FONT_FEATURE *f; UINT32 nf; UINT32 len; } featrange;
/* GetGlyphs.  loc may be NULL.  fr/nfr: typographic feature ranges (NULL = none). */
HRESULT sh_glyphs(fx *x, shaped *s, const WCHAR *text, UINT32 n, BOOL rtl, const WCHAR *loc, const featrange *fr, UINT32 nfr);
/* GetGlyphPlacements at em (default 2048) with the same features as the preceding sh_glyphs call. */
HRESULT sh_place(fx *x, shaped *s, float em, const featrange *fr, UINT32 nfr);
/* Convenience: shape text (LTR) with an optional single feature set applied to the whole text and place at em 2048. */
HRESULT sh_run(fx *x, shaped *s, const WCHAR *text, BOOL rtl, const DWRITE_FONT_FEATURE *f, UINT32 nf);
#define WLEN(t) ((UINT32)wcslen(t))
#define EM2048 2048.0f

#define FEAT(tag, val) { (DWRITE_FONT_FEATURE_TAG)(tag), (val) }
#define T_liga TG('l','i','g','a')
#define T_clig TG('c','l','i','g')
#define T_calt TG('c','a','l','t')
#define T_ccmp TG('c','c','m','p')
#define T_locl TG('l','o','c','l')
#define T_kern TG('k','e','r','n')
#define T_mark TG('m','a','r','k')
#define T_mkmk TG('m','k','m','k')
#define T_smcp TG('s','m','c','p')
#define T_c2sc TG('c','2','s','c')
#define T_ss01 TG('s','s','0','1')
#define T_ss02 TG('s','s','0','2')
#define T_pnum TG('p','n','u','m')
#define T_tnum TG('t','n','u','m')
#define T_salt TG('s','a','l','t')
#define T_zero TG('z','e','r','o')
#define T_cpsp TG('c','p','s','p')
#define T_rlig TG('r','l','i','g')
#define T_init TG('i','n','i','t')
#define T_medi TG('m','e','d','i')
#define T_fina TG('f','i','n','a')
#define T_isol TG('i','s','o','l')
#define T_half TG('h','a','l','f')
#define T_rphf TG('r','p','h','f')

/* all glyph ids of the shaped run equal the list */
int sh_gids_are(const shaped *s, const UINT16 *want, UINT32 n);
/* human readable glyph list for failure messages (static buffer) */
const char *sh_dump(const shaped *s);
#define CHECK_GIDS(s, ...) do { static const UINT16 w_[] = { __VA_ARGS__ }; \
    CHECKF(sh_gids_are((s), w_, (UINT32)(sizeof w_ / sizeof w_[0])), "glyphs %s want %u glyphs, first %u", sh_dump(s), (unsigned)(sizeof w_ / sizeof w_[0]), (unsigned)w_[0]); } while (0)
#define CHECK_CMAP(s, ...) do { static const UINT16 c_[] = { __VA_ARGS__ }; UINT32 i_; \
    for (i_ = 0; i_ < sizeof c_ / sizeof c_[0] && i_ < SH_MAX; i_++) CHECKF((s)->cmap[i_] == c_[i_], "clusterMap[%u] = %u want %u", i_, (s)->cmap[i_], c_[i_]); } while (0)
#define CHECK_ADV(s, ...) do { static const float a_[] = { __VA_ARGS__ }; UINT32 i_; \
    for (i_ = 0; i_ < sizeof a_ / sizeof a_[0] && i_ < SH_MAX; i_++) CHECKF((s)->adv[i_] == a_[i_], "advance[%u] = %g want %g", i_, (s)->adv[i_], a_[i_]); } while (0)
#define CHECK_OFF(s, i, dx, dy) do { CHECKF((s)->off[i].advanceOffset == (float)(dx) && (s)->off[i].ascenderOffset == (float)(dy), \
    "offset[%u] = (%g,%g) want (%g,%g)", (unsigned)(i), (s)->off[i].advanceOffset, (s)->off[i].ascenderOffset, (float)(dx), (float)(dy)); } while (0)
#endif
