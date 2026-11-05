/* State-changing probes of the system font set expiration event and system collection (IDWriteFontSet2::GetExpirationEvent,
 * IDWriteFactory3/7::GetSystemFontSet/Collection) with a session-wide GDI font resource (AddFontResourceW without FR_PRIVATE).
 * Nothing persists: the font file lives in the scratch directory, no registry key is written, and every resource added is removed
 * (also after failed checks) before the test returns. The DirectWrite system collection is built from installed fonts, so what a
 * session font does to it is recorded with t_note; only the add/remove pairing and the state after removal are hard checks. */
#include "dwx.h"

static int seq;
typedef struct { WCHAR wpath[MAX_PATH]; WCHAR wfam[64]; int ok, added; } sf;
static int sf_make(sf *p)
{
    uint8_t *blob; size_t n = 0; FILE *fp; char path[MAX_PATH], leaf[96], fam[64];
    memset(p, 0, sizeof *p);
    snprintf(fam, sizeof fam, "DxGap2Sess%lu_%d", (unsigned long)GetCurrentProcessId(), ++seq);
    dw_wide(fam, p->wfam, 64);
    blob = ttf_build_styled(fam, 400, 5, 0, &n);
    if (!blob) return 0;
    snprintf(leaf, sizeof leaf, "%s.ttf", fam);
    t_tmpfile(path, sizeof path, leaf);
    fp = fopen(path, "wb");
    if (!fp) { free(blob); return 0; }
    fwrite(blob, 1, n, fp); fclose(fp); free(blob);
    MultiByteToWideChar(CP_ACP, 0, path, -1, p->wpath, MAX_PATH);
    p->ok = 1;
    return 1;
}
static void sf_remove(sf *p) { if (p->added) { RemoveFontResourceW(p->wpath); p->added = 0; } }
static BOOL has_family(IDWriteFactory *f, BOOL check, const WCHAR *fam)
{
    IDWriteFontCollection *c = NULL; UINT32 idx = 0; BOOL ex = FALSE;
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, check)) || !c) return FALSE;
    IDWriteFontCollection_FindFamilyName(c, fam, &idx, &ex);
    IDWriteFontCollection_Release(c);
    return ex;
}

TEST(dxgap2_session_font_add_remove_roundtrip)
{
    IDWriteFactory *f = dw_shared(); sf p; UINT32 n0, n1, n2;
    if (!sf_make(&p)) { t_skip("cannot write scratch font"); return; }
    {
        IDWriteFontCollection *c = NULL; IDWriteFactory_GetSystemFontCollection(f, &c, TRUE); n0 = c ? IDWriteFontCollection_GetFontFamilyCount(c) : 0; DW_RELEASE(c);
    }
    CHECKF(AddFontResourceW(p.wpath) == 1, "AddFontResourceW failed (err %lu)", GetLastError());
    p.added = 1;
    {
        IDWriteFontCollection *c = NULL; IDWriteFactory_GetSystemFontCollection(f, &c, TRUE); n1 = c ? IDWriteFontCollection_GetFontFamilyCount(c) : 0; DW_RELEASE(c);
    }
    t_note("families before %u, with session font %u, session family visible: %d", n0, n1, (int)has_family(f, TRUE, p.wfam));
    CHECK(RemoveFontResourceW(p.wpath));
    p.added = 0;
    {
        IDWriteFontCollection *c = NULL; IDWriteFactory_GetSystemFontCollection(f, &c, TRUE); n2 = c ? IDWriteFontCollection_GetFontFamilyCount(c) : 0; DW_RELEASE(c);
    }
    CHECKF(n2 == n0, "family count after removal %u vs %u before", n2, n0);
    CHECK(!has_family(f, TRUE, p.wfam));
    sf_remove(&p);
}

TEST(dxgap2_session_font_expiration_event_observed)
{
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *set = NULL; IDWriteFontSet2 *s2 = NULL; sf p; HANDLE ev = NULL; DWORD w0, w1;
    if (!f3) { t_skip("Factory3 unsupported"); return; }
    if (!sf_make(&p)) { t_skip("cannot write scratch font"); return; }
    if (FAILED(IDWriteFactory3_GetSystemFontSet(f3, &set)) || !set || FAILED(IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet2, (void **)&s2))) { DW_RELEASE(set); t_skip("no IDWriteFontSet2"); return; }
    ev = IDWriteFontSet2_GetExpirationEvent(s2);
    if (!ev) { t_note("system set has no expiration event on this build"); DW_RELEASE(s2); DW_RELEASE(set); return; }
    w0 = WaitForSingleObject(ev, 0);
    AddFontResourceW(p.wpath); p.added = 1;
    w1 = WaitForSingleObject(ev, 1500);
    t_note("expiration event before add: %lu, within 1.5s after add: %lu", w0, w1);
    CHECK(w0 == WAIT_TIMEOUT || w0 == WAIT_OBJECT_0);
    CHECK(w1 == WAIT_TIMEOUT || w1 == WAIT_OBJECT_0);
    CHECK(IDWriteFontSet2_GetExpirationEvent(s2) == ev);        /* still the same handle for this set object */
    sf_remove(&p);
    DW_RELEASE(s2); DW_RELEASE(set);
}

TEST(dxgap2_session_font_set_contents_unchanged_after_removal)
{
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3); IDWriteFontSet *a = NULL, *b = NULL; sf p;
    if (!f3) { t_skip("Factory3 unsupported"); return; }
    if (!sf_make(&p)) { t_skip("cannot write scratch font"); return; }
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &a));
    AddFontResourceW(p.wpath); p.added = 1;
    sf_remove(&p);
    CHECK_HR_OK(IDWriteFactory3_GetSystemFontSet(f3, &b));
    if (a && b) { CHECK(IDWriteFontSet_GetFontCount(a) == IDWriteFontSet_GetFontCount(b)); }
    DW_RELEASE(a); DW_RELEASE(b);
}
