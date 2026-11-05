/* IDWriteFontCollection3::GetExpirationEvent / IDWriteFontSet2::GetExpirationEvent of the system collection while a font is added to and removed from the
 * running session.
 *
 * Destructive area (run with /destructive): the tests call AddFontResourceW with a scratch file (a synthetic font under the process scratch directory) and always
 * RemoveFontResourceW it again before returning, whatever the checks said. No registry writes, no font broadcast. Whether the DirectWrite system collection
 * reacts to a session font is host dependent: the outcomes are notes, the invariants (valid handle, same handle on every call while the collection lives, the
 * session font registration is balanced again after RemoveFontResource) are hard checks.
 */
#include "dwx.h"

#define DXG_FAMILY "DxGap01Expiry"
#define DXG_WAIT_MS 3000

typedef struct dxg_font { char path[MAX_PATH]; WCHAR wpath[MAX_PATH]; int added; } dxg_font;

static int dxg_write_font(dxg_font *f)
{
    ttf_spec s; size_t n; uint8_t *b; HANDLE h; DWORD wr = 0; char ps[64];
    ttf_default_spec(&s); snprintf(ps, sizeof ps, "%s-Regular", DXG_FAMILY); ttf_set_names(&s, DXG_FAMILY, "Regular", DXG_FAMILY, ps);
    b = ttf_build(&s, &n);
    if (!b) return 0;
    t_tmpfile(f->path, sizeof f->path, "dxg_expiry.ttf");
    h = CreateFileA(f->path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { free(b); return 0; }
    WriteFile(h, b, (DWORD)n, &wr, NULL); CloseHandle(h); free(b);
    return wr == n && dw_wide(f->path, f->wpath, MAX_PATH) > 0;
}
static int dxg_add(dxg_font *f) { int n = AddFontResourceW(f->wpath); if (n > 0) f->added = n; return n; }
static void dxg_remove(dxg_font *f)
{
    int guard = 0;
    while (f->added > 0 && guard++ < 8) { if (!RemoveFontResourceW(f->wpath)) break; f->added--; }
    f->added = 0;
}
static int dxg_family_present(IDWriteFactory *fac, BOOL check_updates)
{
    IDWriteFontCollection *c = NULL; UINT32 idx = 0; BOOL ex = FALSE; WCHAR name[64];
    dw_wide(DXG_FAMILY, name, 64);
    if (FAILED(IDWriteFactory_GetSystemFontCollection(fac, &c, check_updates)) || !c) return -1;
    IDWriteFontCollection_FindFamilyName(c, name, &idx, &ex);
    IDWriteFontCollection_Release(c);
    return ex ? 1 : 0;
}

TEST(dxg_expiry_handle_stable_while_a_session_font_is_added)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontCollection *c = NULL; IDWriteFontCollection3 *c3 = NULL; dxg_font font; HANDLE h0, h1, h2; int before;
    memset(&font, 0, sizeof font);
    if (!f) { t_skip("DWriteCreateFactory failed"); return; }
    if (FAILED(IDWriteFactory_GetSystemFontCollection(f, &c, FALSE)) || !c || FAILED(IDWriteFontCollection_QueryInterface(c, &IID_IDWriteFontCollection3, (void **)&c3)) || !c3) {
        DW_RELEASE(c); IDWriteFactory_Release(f); t_skip("IDWriteFontCollection3 unavailable"); return;
    }
    before = dxg_family_present(f, FALSE);
    if (before != 0 || !dxg_write_font(&font)) { IDWriteFontCollection3_Release(c3); IDWriteFontCollection_Release(c); IDWriteFactory_Release(f); t_skip("fixture unavailable (family present %d)", before); return; }
    h0 = IDWriteFontCollection3_GetExpirationEvent(c3);
    {
        int added = dxg_add(&font);
        t_note("AddFontResourceW -> %d", added);
        if (added > 0) {
            DWORD w = h0 ? WaitForSingleObject(h0, DXG_WAIT_MS) : WAIT_FAILED;
            h1 = IDWriteFontCollection3_GetExpirationEvent(c3);
            t_note("event %p (before %p), wait after the session font was added: %lu", h1, h0, w);
            CHECKF(h0 == h1, "the event handle of one collection object changed (%p -> %p)", h0, h1);
            t_note("old collection object sees the family: %d (cached snapshot)", dxg_family_present(f, FALSE));
            t_note("fresh collection with checkForUpdates sees the family: %d", dxg_family_present(f, TRUE));
        }
        dxg_remove(&font);
        t_note("after RemoveFontResourceW the family is %s", dxg_family_present(f, TRUE) == 1 ? "still visible" : "gone");
    }
    h2 = IDWriteFontCollection3_GetExpirationEvent(c3);
    CHECKF(h2 == h0, "the event handle changed after the session font was removed (%p -> %p)", h0, h2);
    CHECKF(!font.added, "session font still added");
    DeleteFileA(font.path);
    IDWriteFontCollection3_Release(c3); IDWriteFontCollection_Release(c); IDWriteFactory_Release(f);
}
TEST(dxg_expiry_font_set_event_while_a_session_font_is_added)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFactory3 *f3 = NULL; IDWriteFontSet *set = NULL; IDWriteFontSet2 *s2 = NULL; dxg_font font;
    memset(&font, 0, sizeof font);
    if (!f) { t_skip("DWriteCreateFactory failed"); return; }
    if (FAILED(IDWriteFactory_QueryInterface(f, &IID_IDWriteFactory3, (void **)&f3)) || !f3 || FAILED(IDWriteFactory3_GetSystemFontSet(f3, &set)) || !set ||
        FAILED(IDWriteFontSet_QueryInterface(set, &IID_IDWriteFontSet2, (void **)&s2)) || !s2) {
        DW_RELEASE(s2); DW_RELEASE(set); DW_RELEASE(f3); IDWriteFactory_Release(f); t_skip("system font set (IDWriteFontSet2) unavailable"); return;
    }
    if (!dxg_write_font(&font)) { IDWriteFontSet2_Release(s2); IDWriteFontSet_Release(set); IDWriteFactory3_Release(f3); IDWriteFactory_Release(f); t_skip("cannot write the scratch font"); return; }
    {
        HANDLE h0 = IDWriteFontSet2_GetExpirationEvent(s2), h1; int added = dxg_add(&font);
        t_note("AddFontResourceW -> %d, set event %p", added, h0);
        if (added > 0 && h0) { DWORD w = WaitForSingleObject(h0, DXG_WAIT_MS); t_note("wait after the session font was added: %lu", w); }
        h1 = IDWriteFontSet2_GetExpirationEvent(s2);
        CHECKF(h0 == h1, "the event handle of one font set object changed (%p -> %p)", h0, h1);
        dxg_remove(&font);
    }
    CHECKF(!font.added, "session font still added");
    DeleteFileA(font.path);
    IDWriteFontSet2_Release(s2); IDWriteFontSet_Release(set); IDWriteFactory3_Release(f3); IDWriteFactory_Release(f);
}
TEST(dxg_expiry_session_font_roundtrip_leaves_no_trace)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); dxg_font font; int before, after, i;
    memset(&font, 0, sizeof font);
    if (!f) { t_skip("DWriteCreateFactory failed"); return; }
    before = dxg_family_present(f, TRUE);
    if (before != 0 || !dxg_write_font(&font)) { IDWriteFactory_Release(f); t_skip("fixture unavailable (family present %d)", before); return; }
    for (i = 0; i < 3; i++) {
        int added = dxg_add(&font);
        CHECKF(added > 0, "round %d: AddFontResourceW -> %d", i, added);
        dxg_remove(&font);
    }
    after = dxg_family_present(f, TRUE);
    t_note("family visible after three add/remove rounds: %d", after);
    if (after != 0) t_note("the system collection still lists the family after RemoveFontResourceW (cached snapshot)");
    DeleteFileA(font.path);
    IDWriteFactory_Release(f);
}
