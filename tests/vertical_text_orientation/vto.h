/* Shared fixtures for the vertical_text_orientation area: vertical / sideways text in DirectWrite.
 *
 * How Windows 10 builds a vertical layout (TextLayout / MutableTextLayout / DWriteTextLayout, dwrite.dll 10.0.19041):
 *   - "vertical" is bit 1 of the READING direction (TOP_TO_BOTTOM = 2, BOTTOM_TO_TOP = 3); "vertical flow" is bit 1 of the FLOW direction
 *     (LEFT_TO_RIGHT = 2, RIGHT_TO_LEFT = 3). Reading and flow must agree on that bit, otherwise the first operation that needs the laid-out
 *     text fails with DWRITE_E_FLOWDIRECTIONCONFLICTS (0x8898500B); the setters and CreateTextLayout accept every combination.
 *   - The layout is computed in an abstract horizontal frame: GetLayoutWidth (the wrapping length) is maxHeight, GetLayoutHeight (the
 *     paragraph-alignment extent) is maxWidth. Afterwards the metrics are swapped back (left<->top, width<->height,
 *     widthIncludingTrailingWhitespace<->heightIncludingTrailingWhitespace) and left = maxWidth - width - leftInAbstractFrame.
 *     Glyph baseline origins are reoriented with  world.x = maxWidth - abstract.y,  world.y = abstract.x.
 *   - Draw of a vertical layout asks the renderer for IDWriteTextRenderer1 (GUID d3e0e934-22a0-427e-aae4-7d9574b59db1) and fails with
 *     DWRITE_E_TEXTRENDERERINCOMPATIBLE (0x8898500A) when the renderer does not implement it.
 *   - SetFlowDirection / SetReadingDirection accept 0..3, SetVerticalGlyphOrientation 0..1, anything else is E_INVALIDARG (nothing changes).
 *
 * Private font collection (all UPM 2048; at size 16 one horizontal letter advance is 7.8125 DIPs):
 *   "DwVert"      default test font + Han / kana cmap entries + vhea/vmtx (vertical advance 1500 for letters, digits, Han and kana, 600 for space)
 *   "DwVertSub"   DwVert + GSUB 'vert' (single substitution): gid 2 -> 64 (triangle), gid 3 -> 65 (round)
 *   "DwPlain"     default test font + the same cmap, no vhea/vmtx and no GSUB
 *   cmap: U+4E00..U+4E03 -> gids 2..5, U+3042 -> 28, U+3044 -> 29, U+3000 -> 1, U+22EE -> 64 (triangle), U+2026 -> 65 (round)
 */
#ifndef VTO_H
#define VTO_H
#include "dwx.h"
#include <float.h>
#include <limits.h>
#include <wchar.h>
#include <math.h>

#define VT_FAM    L"DwVert"
#define VT_SUB    L"DwVertSub"
#define VT_PLAIN  L"DwPlain"
#define VT_LOC    L"en-us"
#define VT_HADV(size)  (1000.0f * (float)(size) / 2048.0f)     /* horizontal letter advance */
#define VT_VADV(size)  (1500.0f * (float)(size) / 2048.0f)     /* vertical letter advance of DwVert / DwVertSub */
#define VT_VADV_SPACE  600
#define VT_E_CONFLICT       ((HRESULT)0x8898500B)
#define VT_E_INCOMPATIBLE   ((HRESULT)0x8898500A)
#define VT_E_INVALIDARG     ((HRESULT)0x80070057)

#define HAN4 L"\x4e00\x4e01\x4e02\x4e03"
#define RD_LTR DWRITE_READING_DIRECTION_LEFT_TO_RIGHT
#define RD_RTL DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
#define RD_TTB DWRITE_READING_DIRECTION_TOP_TO_BOTTOM
#define RD_BTT DWRITE_READING_DIRECTION_BOTTOM_TO_TOP
#define FD_TTB DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM
#define FD_BTT DWRITE_FLOW_DIRECTION_BOTTOM_TO_TOP
#define FD_LTR DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT
#define FD_RTL DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT

typedef struct vt_fx { IDWriteFactory *f; dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll; UINT32 key[3]; } vt_fx;
int  vt_open(vt_fx *x);
void vt_close(vt_fx *x);
#define VT_BEGIN() vt_fx X; memset(&X, 0, sizeof X); if (!vt_open(&X)) { vt_close(&X); t_skip("synthetic font collection unavailable"); return; } IDWriteFactory *f = X.f; (void)f
#define VT_END()   vt_close(&X)

IDWriteTextFormat *vt_format(vt_fx *x, const WCHAR *family, float size);
/* Layout in the given family; the reading / flow directions are applied with the LAYOUT setters (NULL when creation or a setter fails). */
IDWriteTextLayout *vt_layout_(vt_fx *x, const WCHAR *family, const WCHAR *text, float size, float w, float h, int reading, int flow);
/* Vertical Japanese-style layout: reading TOP_TO_BOTTOM, flow RIGHT_TO_LEFT. */
#define VT_VL(var, family, text, size, w, h) IDWriteTextLayout *var = vt_layout_(&X, family, text, size, w, h, RD_TTB, FD_RTL); if (!var) { VT_END(); t_skip("vertical layout prerequisite failed"); return; }
/* Horizontal reference layout (reading LTR, flow TTB). */
#define VT_HL(var, family, text, size, w, h) IDWriteTextLayout *var = vt_layout_(&X, family, text, size, w, h, RD_LTR, FD_TTB); if (!var) { VT_END(); t_skip("horizontal layout prerequisite failed"); return; }

/* Renderer that implements IDWriteTextRenderer1 (records angle + isSideways) / only IDWriteTextRenderer. */
dwx_renderer *vt_r1(void);
dwx_renderer *vt_r0(void);
HRESULT vt_draw(IDWriteTextLayout *l, dwx_renderer *r, float ox, float oy);
const dwx_draw *vt_run(dwx_renderer *r, int nth);                 /* nth glyph run in callback order */
int vt_runs(dwx_renderer *r);

typedef struct vt_lines { DWRITE_LINE_METRICS lm[64]; UINT32 n; } vt_lines;
void vt_get_lines(IDWriteTextLayout *l, vt_lines *o);
float vt_sum_heights(const vt_lines *o);
DWRITE_TEXT_METRICS vt_metrics(IDWriteTextLayout *l);
/* Expected ink of one horizontal / vertical advance run. */
float vt_runw(const dwx_draw *d);
#define CHECK_NEARF(a, b) CHECK_NEAR((a), (b), 0.001)
#define CHECK_WEQ(a, b) do { const WCHAR *a_ = (a), *b_ = (b); t_check(a_ && b_ && !wcscmp(a_, b_), __FILE__, __LINE__, #a " == " #b, "wide string mismatch: got \"%ls\" want \"%ls\"", a_ ? a_ : L"(null)", b_ ? b_ : L"(null)"); } while (0)
/* Font face of a family of the private collection (first font); caller releases. */
IDWriteFontFace *vt_face(vt_fx *x, const WCHAR *family);
#endif
