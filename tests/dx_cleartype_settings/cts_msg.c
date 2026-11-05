/* WM_SETTINGCHANGE broadcast caused by SystemParametersInfo on a private, never-shown top-level window.
 * Windows 10 22H2 (measured, standard and elevated user): SPIF_SENDCHANGE alone never delivers WM_SETTINGCHANGE, and
 * neither does SPIF_UPDATEINIFILE alone.  SPIF_UPDATEINIFILE|SPIF_SENDCHANGE delivers exactly one synchronous
 * WM_SETTINGCHANGE (wParam = the SPI_SET* action) per call, also when the value does not change; the new value is already
 * readable from inside the handler.  Each test flips a value and restores the original, with the same flags. */
#include "cts.h"

static const char CLS[] = "cts_msg_window";
static struct { LONG n[0x3000]; LONG total; UINT last_msg; WPARAM last_w; LPARAM last_l; LONG wininichange; } g_m;

static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SETTINGCHANGE) {
        g_m.total++; g_m.last_msg = m; g_m.last_w = w; g_m.last_l = l;
        if (w < 0x3000) g_m.n[w]++;
    }
    return DefWindowProcA(h, m, w, l);
}
static HWND mk(void) {
    WNDCLASSA wc; HWND h;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wp; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = CLS;
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return NULL;
    memset(&g_m, 0, sizeof g_m);
    h = CreateWindowExA(0, CLS, "cts", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);   /* never shown */
    return h;
}
static void pump(void) { MSG m; int i; for (i = 0; i < 100 && PeekMessageA(&m, NULL, 0, 0, PM_REMOVE); i++) DispatchMessageA(&m); }
static void kill_win(HWND h) { if (h) DestroyWindow(h); UnregisterClassA(CLS, GetModuleHandleA(NULL)); }
#define WIN_OR_SKIP(h) do { (h) = mk(); if (!(h)) { cts_end(); t_skip("cannot create a private window"); return; } } while (0)

#define F_SEND SPIF_SENDCHANGE
#define F_BOTH (SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)
/* set/get one of the four settings; kind 0 smoothing (uiParam), 1 type, 2 contrast, 3 orientation (pvParam) */
static const UINT SETA[4] = { SPI_SETFONTSMOOTHING, SPI_SETFONTSMOOTHINGTYPE, SPI_SETFONTSMOOTHINGCONTRAST, SPI_SETFONTSMOOTHINGORIENTATION };
static const UINT GETA[4] = { SPI_GETFONTSMOOTHING, SPI_GETFONTSMOOTHINGTYPE, SPI_GETFONTSMOOTHINGCONTRAST, SPI_GETFONTSMOOTHINGORIENTATION };
static BOOL spi_set(int k, UINT v, UINT fl) {
    return k == 0 ? SystemParametersInfoW(SETA[0], v, NULL, fl) : SystemParametersInfoW(SETA[k], 0, (PVOID)(ULONG_PTR)v, fl);
}
static UINT spi_get(int k) { UINT v = 0; SystemParametersInfoW(GETA[k], 0, &v, 0); return v; }
static UINT other_value(int k, UINT cur) {
    switch (k) { case 0: return !cur; case 1: return cur == CTS_FE_CLEARTYPE ? CTS_FE_STANDARD : CTS_FE_CLEARTYPE;
                 case 2: return cur == 1400 ? 1600 : 1400; default: return cur ? CTS_ORIENT_BGR : CTS_ORIENT_RGB; }
}
static const char *KN[4] = { "smoothing", "type", "contrast", "orientation" };

/* flip then restore with the given flags; returns notifications for the action seen after (flip, restore) */
static void flip_restore(int k, UINT fl, LONG *n_flip, LONG *n_restore, int *ok) {
    UINT cur = spi_get(k), oth = other_value(k, cur); LONG a = g_m.n[SETA[k]];
    *ok = spi_set(k, oth, fl) != 0;
    *n_flip = g_m.n[SETA[k]] - a;
    a = g_m.n[SETA[k]];
    spi_set(k, cur, fl);
    *n_restore = g_m.n[SETA[k]] - a;
}

static void send_only(int k) {
    HWND h; LONG nf, nr; int ok;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    flip_restore(k, F_SEND, &nf, &nr, &ok); pump();
    if (!ok) { kill_win(h); cts_end(); t_skip("%s refused", KN[k]); return; }
    t_note("%s with SPIF_SENDCHANGE only: %ld + %ld notifications", KN[k], nf, nr);
    CHECKF(nf == 0 && nr == 0 && g_m.n[SETA[k]] == 0, "SPIF_SENDCHANGE alone delivered %ld/%ld", nf, nr);
    CHECK(g_m.total == 0);
    kill_win(h); cts_end();
}
static void send_ini(int k) {
    HWND h; LONG nf, nr; int ok;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    flip_restore(k, F_BOTH, &nf, &nr, &ok);
    if (!ok) { kill_win(h); cts_end(); t_skip("%s refused", KN[k]); return; }
    /* no message pump in between: delivery is synchronous */
    CHECKF(nf == 1, "%s change delivered %ld notifications, want 1", KN[k], nf);
    CHECKF(nr == 1, "%s restore delivered %ld notifications, want 1", KN[k], nr);
    pump();
    CHECKF(g_m.n[SETA[k]] == 2, "%s total %ld after pump, want 2", KN[k], g_m.n[SETA[k]]);
    CHECKF(g_m.total == 2, "%ld notifications in all, want 2 (only the action's own)", g_m.total);
    kill_win(h); cts_end();
}
TEST(msg_smoothing_sendchange_alone_silent)   { send_only(0); }
TEST(msg_type_sendchange_alone_silent)        { send_only(1); }
TEST(msg_contrast_sendchange_alone_silent)    { send_only(2); }
TEST(msg_orientation_sendchange_alone_silent) { send_only(3); }
TEST(msg_smoothing_update_and_send_notifies)   { send_ini(0); }
TEST(msg_type_update_and_send_notifies)        { send_ini(1); }
TEST(msg_contrast_update_and_send_notifies)    { send_ini(2); }
TEST(msg_orientation_update_and_send_notifies) { send_ini(3); }

TEST(msg_update_alone_silent) {
    HWND h; LONG nf, nr; int ok;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    flip_restore(0, SPIF_UPDATEINIFILE, &nf, &nr, &ok); pump();
    if (!ok) { kill_win(h); cts_end(); t_skip("smoothing refused"); return; }
    CHECKF(g_m.total == 0, "SPIF_UPDATEINIFILE alone delivered %ld", g_m.total);
    kill_win(h); cts_end();
}
TEST(msg_no_broadcast_without_flags) {
    HWND h; UINT cur;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    cur = spi_get(0);
    /* fWinIni = 0: change in memory only, no notification */
    CTS_SET_OR_SKIP(spi_set(0, !cur, 0));
    pump();
    CHECKF(g_m.total == 0, "unexpected notification with fWinIni 0 (%ld)", g_m.total);
    CHECK(spi_get(0) == !cur);
    spi_set(0, cur, 0);
    kill_win(h); cts_end();
}
TEST(msg_unchanged_value_still_notifies) {
    HWND h; UINT cur; int r;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    cur = spi_get(0);
    r = spi_set(0, cur, F_BOTH);
    if (!r) { kill_win(h); cts_end(); t_skip("smoothing refused"); return; }
    CHECKF(g_m.n[SPI_SETFONTSMOOTHING] == 1, "setting smoothing to its current value delivered %ld, want 1", g_m.n[SPI_SETFONTSMOOTHING]);
    kill_win(h); cts_end();
}
TEST(msg_four_changes_four_notifications) {
    HWND h; int i; UINT cur;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    cur = spi_get(3);
    for (i = 0; i < 4; i++) CTS_SET_OR_SKIP(spi_set(3, i & 1 ? cur : other_value(3, cur), F_BOTH));
    pump();
    CHECKF(g_m.n[SPI_SETFONTSMOOTHINGORIENTATION] == 4, "four changes delivered %ld, want 4", g_m.n[SPI_SETFONTSMOOTHINGORIENTATION]);
    spi_set(3, cur, F_BOTH);
    kill_win(h); cts_end();
}
TEST(msg_wparam_is_the_action) {
    HWND h; UINT cur;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    cur = spi_get(0);
    CTS_SET_OR_SKIP(spi_set(0, !cur, F_BOTH));
    CHECKF(g_m.last_msg == WM_SETTINGCHANGE, "last message %#x", g_m.last_msg);
    CHECKF(g_m.last_w == SPI_SETFONTSMOOTHING, "wParam %#llx, want %#x", (unsigned long long)g_m.last_w, SPI_SETFONTSMOOTHING);
    spi_set(0, cur, F_BOTH);
    kill_win(h); cts_end();
}
static BOOL g_seen; static LONG g_seen_n;
static LRESULT CALLBACK wp2(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SETTINGCHANGE && w == SPI_SETFONTSMOOTHING) { g_seen_n++; SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &g_seen, 0); }
    return DefWindowProcA(h, m, w, l);
}
TEST(msg_state_already_updated_when_message_arrives) {
    /* the new value is readable from inside the notification */
    WNDCLASSA wc; HWND h; UINT cur;
    CTS_BEGIN();
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wp2; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "cts_msg_window2";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { cts_end(); t_skip("class"); return; }
    h = CreateWindowExA(0, wc.lpszClassName, "cts", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    if (!h) { UnregisterClassA(wc.lpszClassName, wc.hInstance); cts_end(); t_skip("window"); return; }
    cur = spi_get(0); g_seen = 0xdead; g_seen_n = 0;
    CTS_SET_OR_SKIP(spi_set(0, !cur, F_BOTH));
    CHECKF(g_seen_n == 1, "handler ran %ld times", g_seen_n);
    CHECKF(g_seen == (BOOL)!cur, "value seen from the notification: %d, want %d", (int)g_seen, (int)!cur);
    spi_set(0, cur, F_BOTH);
    DestroyWindow(h); UnregisterClassA(wc.lpszClassName, wc.hInstance);
    cts_end();
}
TEST(msg_hidden_window_still_receives) {
    HWND h; UINT cur;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CHECK(!IsWindowVisible(h));
    cur = spi_get(1);
    CTS_SET_OR_SKIP(spi_set(1, other_value(1, cur), F_BOTH));
    spi_set(1, cur, F_BOTH);
    CHECKF(g_m.n[SPI_SETFONTSMOOTHINGTYPE] == 2, "%ld notifications with wParam SPI_SETFONTSMOOTHINGTYPE, want 2", g_m.n[SPI_SETFONTSMOOTHINGTYPE]);
    kill_win(h); cts_end();
}
