/* Shared fixtures for the unicode_script_coverage area.
 *
 * Synthetic font "UcTest" (UPM 2048): the default test font (letters, digits, space, advance 1000, notdef 1200) extended with
 * 60 more box glyphs (gid 69..128, advance 1000) and a cmap that maps a table of "interesting" code points (uc_extra) to one
 * distinct glyph each: gid = 69 + index in uc_extra.  Every expected glyph id is therefore a pure function of the table.
 *   variant UC_V_BOTH   cmap (3,1) format 4 + (3,10) format 12     (BMP code points in both, supplementary only in format 12)
 *   variant UC_V_UCS4   cmap (3,10) format 12 only
 *   variant UC_V_BMP    cmap (3,1) format 4 only                    (supplementary and U+FFFF unmapped)
 *   variant UC_V_UVS    BOTH + (0,5) format 14 (default UVS for 'A'+FE00, non-default for 'B'+FE00 -> gid 100)
 * The same bytes are used for a private font collection (layout tests) so every character of a fixture string comes from this font.
 */
#ifndef UC_H
#define UC_H
#include "dwx.h"
#include <wchar.h>
#include <float.h>
#include <limits.h>

enum { UC_V_BOTH, UC_V_UCS4, UC_V_BMP, UC_V_UVS, UC_V_NVARIANT };
extern const unsigned uc_extra[];
extern const int uc_nextra;
#define UC_FIRST_EXTRA_GID 69
#define UC_UVS_NONDEFAULT_GID 100
/* Expected glyph for a code point under a variant (0 when unmapped). */
unsigned uc_gid(int variant, unsigned cp);
uint8_t *uc_build(int variant, size_t *n);

typedef struct uc_face { dwx_font h; IDWriteFontFace *face; IDWriteFontFace1 *face1; int variant; } uc_face;
int  uc_face_open(uc_face *u, int variant);       /* 1 on success; shared factory */
void uc_face_close(uc_face *u);
#define UC_FACE_BEGIN(u, variant) uc_face u; memset(&u, 0, sizeof u); \
    if (!uc_face_open(&u, variant)) { uc_face_close(&u); t_skip("synthetic face unavailable"); return; }

/* Layout fixture over a private collection holding UcTest (isolated factory). */
typedef struct uc_fx { IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; } uc_fx;
int  uc_fx_open(uc_fx *x);
void uc_fx_close(uc_fx *x);
#define UC_FX_BEGIN() uc_fx X; memset(&X, 0, sizeof X); if (!uc_fx_open(&X)) { uc_fx_close(&X); t_skip("synthetic collection unavailable"); return; }
#define UC_FX_END() uc_fx_close(&X)
/* Layout of UcTest text at `size` (2048 => every character 1000 DIPs wide). NULL on failure. */
IDWriteTextLayout *uc_layout(uc_fx *x, const WCHAR *text, UINT32 len, float size, float w, float h);

/* System font access */
IDWriteFont *uc_sysfont(IDWriteFactory *f, const WCHAR *family);       /* regular, NULL if family missing */
IDWriteFontFace1 *uc_sysface1(IDWriteFactory *f, const WCHAR *family); /* NULL if missing */

/* Text analysis helpers */
typedef struct uc_run { UINT32 pos, len; DWRITE_SCRIPT_ANALYSIS sa; UINT32 iso, num; } uc_run;
typedef struct uc_ana { int n; uc_run r[64]; HRESULT hr; int covered; } uc_ana;
IDWriteTextAnalyzer *uc_analyzer(void);                                /* cached; do not release */
void uc_analyze(const WCHAR *t, UINT32 len, uc_ana *o);                /* AnalyzeScript over the whole text */
DWRITE_SCRIPT_ANALYSIS uc_script_at(const WCHAR *t, UINT32 len, UINT32 pos, int *ok);
HRESULT uc_props(DWRITE_SCRIPT_ANALYSIS sa, DWRITE_SCRIPT_PROPERTIES *p);
#define UC_TAG(a, b, c, d) ((UINT32)(a) | ((UINT32)(b) << 8) | ((UINT32)(c) << 16) | ((UINT32)(d) << 24))

/* Shaping */
#define UC_MAXG 256
typedef struct uc_shaped { HRESULT hr; UINT32 nglyphs; UINT16 cluster[UC_MAXG]; DWRITE_SHAPING_TEXT_PROPERTIES tp[UC_MAXG];
    UINT16 gid[UC_MAXG]; DWRITE_SHAPING_GLYPH_PROPERTIES gp[UC_MAXG]; UINT32 actual; } uc_shaped;
HRESULT uc_shape(IDWriteFontFace *face, const WCHAR *t, UINT32 len, BOOL rtl, uc_shaped *o);                                /* script of first char */
HRESULT uc_shape_sa(IDWriteFontFace *face, const WCHAR *t, UINT32 len, BOOL rtl, DWRITE_SCRIPT_ANALYSIS sa, uc_shaped *o);

/* UTF-16 encoding of a UTF-32 value (surrogate pair above 0xFFFF); returns units written. */
int uc_utf16(unsigned cp, WCHAR *out);
#define LEN(s) ((UINT32)wcslen(s))
#define W(...) ((const WCHAR[]){ __VA_ARGS__, 0 })
#endif
