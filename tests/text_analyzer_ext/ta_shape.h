/* Shaping fixture: runs IDWriteTextAnalyzer::GetGlyphs / GetGlyphPlacements for one script run so that the extension methods
 * can be fed with the real cluster map, glyph properties and advances. */
#ifndef TA_SHAPE_H
#define TA_SHAPE_H
#include "ta.h"
#define TA_SHAPE_MAX 512
typedef struct ta_shaped {
    DWRITE_SCRIPT_ANALYSIS sa; UINT32 nglyphs, ntext;
    UINT16 cmap[TA_SHAPE_MAX]; DWRITE_SHAPING_TEXT_PROPERTIES tprops[TA_SHAPE_MAX];
    UINT16 glyphs[TA_SHAPE_MAX * 2]; DWRITE_SHAPING_GLYPH_PROPERTIES gprops[TA_SHAPE_MAX * 2];
    float adv[TA_SHAPE_MAX * 2]; DWRITE_GLYPH_OFFSET off[TA_SHAPE_MAX * 2];
} ta_shaped;
/* Returns 1 on success. rtl: right-to-left run. The script is taken from the first character. */
int ta_shape(ta_shaped *s, ta_ctx *c, IDWriteFontFace *face, const WCHAR *text, UINT32 n, int rtl, float em);
#endif
