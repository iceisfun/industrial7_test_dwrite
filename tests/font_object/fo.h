/* Fixtures for the IDWriteFont object tests: synthetic fonts served through a custom collection, system font lookup. */
#ifndef FO_H
#define FO_H
#include "dwx.h"

#define FO_MAX 12
/* A private (isolated) factory with a custom collection built from synthetic fonts.  font[i] is the IDWriteFont whose
 * file is blob i (identified through its font file reference key), independent of how families are grouped. */
typedef struct fo_fx {
    IDWriteFactory *f;
    dwx_mem_loader *fl; dwx_coll_loader *cl; IDWriteFontCollection *coll;
    int n; UINT32 keys[FO_MAX]; IDWriteFont *font[FO_MAX];
    HRESULT hr_coll;
} fo_fx;
/* Builds blobs from specs (specs may be NULL entries only when blobs given) and opens them all in one collection.
 * Returns 1 when every font was found; always call fo_close(). */
int  fo_open(fo_fx *x, const ttf_spec *specs, int n);
int  fo_open1(fo_fx *x, const ttf_spec *spec);
void fo_close(fo_fx *x);
/* Skips the rest of the calling test when the fixture could not be created. */
#define FO_OPEN1(x, spec) do { if (!fo_open1(&(x), &(spec))) { fo_close(&(x)); t_skip("custom collection unavailable (hr=%s)", dw_hr_name((x).hr_coll)); return; } } while (0)
#define FO_OPEN(x, specs, n) do { if (!fo_open(&(x), (specs), (n))) { fo_close(&(x)); t_skip("custom collection unavailable (hr=%s)", dw_hr_name((x).hr_coll)); return; } } while (0)

/* System font: first font of the named family matching weight/stretch/style (NULL when family missing). Caller releases. */
IDWriteFont *fo_sysfont(IDWriteFactory *f, const WCHAR *family, DWRITE_FONT_WEIGHT w, DWRITE_FONT_STRETCH s, DWRITE_FONT_STYLE st);
#define FO_SYS(var, fam, w, st) IDWriteFont *var = fo_sysfont(dw_shared(), fam, w, DWRITE_FONT_STRETCH_NORMAL, st); \
    if (!var) { t_skip("system family %ls not installed", fam); return; }
#define FO_SYS_REG(var, fam) FO_SYS(var, fam, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL)

/* OS/2 table patching: replaces spec->raw OS/2 by a copy of the generated table with `len` bytes at `off` overwritten. */
void fo_os2_patch(ttf_spec *s, unsigned off, const void *bytes, unsigned len);
void fo_set_panose(ttf_spec *s, const uint8_t p[10]);
/* Spec with default glyphs, unique names ("FoTest"), regular. */
void fo_spec(ttf_spec *s, const char *family);
/* Spec with a symbol (3,0) cmap: U+F041..U+F05A -> gids 2.. (A-Z), U+F020 -> space. */
void fo_symbol_spec(ttf_spec *s, const char *family);
/* UTF-8 of a localized string at index idx into out (empty on failure). */
void fo_ls(IDWriteLocalizedStrings *ls, UINT32 idx, char *out, int cap);
/* locale name of string idx (utf-8) */
void fo_ls_locale(IDWriteLocalizedStrings *ls, UINT32 idx, char *out, int cap);
/* font -> reference key of its first file (dwx key value); 0 when not available */
UINT32 fo_font_key(IDWriteFont *font);
/* System Arial table byte helpers */
int fo_sys_table(IDWriteFont *font, const char *tag, uint8_t **data, UINT32 *len);
#endif
/* Parses an sfnt 'name' table: UTF-16BE record (plat 3 / 0) to WCHAR (NUL terminated); returns length in WCHARs, -1 when absent. */
int fo_name_find(const uint8_t *t, UINT32 len, unsigned plat, unsigned enc, unsigned lang, unsigned id, WCHAR *out, int cap);
/* wide string at index idx of a localized string set; returns length (0 on failure) */
int fo_ls_w(IDWriteLocalizedStrings *ls, UINT32 idx, WCHAR *out, int cap);
/* index of locale (case-insensitive compare done by the caller via the returned name), -1 when not found */
int fo_ls_find_ci(IDWriteLocalizedStrings *ls, const char *locale);
