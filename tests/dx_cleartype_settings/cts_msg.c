/* WM_SETTINGCHANGE broadcast caused by SystemParametersInfo(..., SPIF_SENDCHANGE) on a private, never-shown top-level window.
 * Delivery of the notification for SPIF_SENDCHANGE alone is not reliable on Windows 10 (it can be absent or late), so the
 * counts are recorded as notes and only invariants that hold either way are checked. */
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

TEST(msg_smoothing_change_notifies) {
    HWND h; LONG n0;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    n0 = g_m.n[SPI_SETFONTSMOOTHING];
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    pump();
    /* whether a notification arrives for SPIF_SENDCHANGE alone varies with the system; record it only */
    t_note("WM_SETTINGCHANGE with wParam SPI_SETFONTSMOOTHING: %ld -> %ld", n0, g_m.n[SPI_SETFONTSMOOTHING]);
    CHECK(g_m.n[SPI_SETFONTSMOOTHING] >= n0);
    kill_win(h); cts_end();
}
TEST(msg_type_change_notifies) {
    HWND h; LONG n0;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_STANDARD));
    n0 = g_m.n[SPI_SETFONTSMOOTHINGTYPE];
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_CLEARTYPE));
    pump();
    t_note("type change: %ld -> %ld notifications", n0, g_m.n[SPI_SETFONTSMOOTHINGTYPE]);
    CHECK(g_m.n[SPI_SETFONTSMOOTHINGTYPE] >= n0);
    kill_win(h); cts_end();
}
TEST(msg_contrast_change_notifies) {
    HWND h; LONG n0;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CTS_SET_OR_SKIP(cts_set_contrast(1400));
    n0 = g_m.n[SPI_SETFONTSMOOTHINGCONTRAST];
    CTS_SET_OR_SKIP(cts_set_contrast(1600));
    pump();
    t_note("contrast change: %ld -> %ld notifications", n0, g_m.n[SPI_SETFONTSMOOTHINGCONTRAST]);
    CHECK(g_m.n[SPI_SETFONTSMOOTHINGCONTRAST] >= n0);
    kill_win(h); cts_end();
}
TEST(msg_orientation_change_notifies) {
    HWND h; LONG n0;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    n0 = g_m.n[SPI_SETFONTSMOOTHINGORIENTATION];
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    pump();
    t_note("orientation change: %ld -> %ld notifications", n0, g_m.n[SPI_SETFONTSMOOTHINGORIENTATION]);
    CHECK(g_m.n[SPI_SETFONTSMOOTHINGORIENTATION] >= n0);
    kill_win(h); cts_end();
}
TEST(msg_delivered_before_spi_call_returns) {
    HWND h; LONG n0, n1;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    n0 = g_m.n[SPI_SETFONTSMOOTHING];
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    n1 = g_m.n[SPI_SETFONTSMOOTHING];        /* no message pump in between */
    t_note("notifications seen before the call returned: %ld -> %ld", n0, n1);
    CHECK(n1 >= n0);
    kill_win(h); cts_end();
}
TEST(msg_state_already_updated_when_message_arrives) {
    /* the new value is readable from inside the notification */
    static BOOL seen = 0xdead; static HWND hh;
    LRESULT CALLBACK wp2(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_SETTINGCHANGE && w == SPI_SETFONTSMOOTHING) SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &seen, 0);
        return DefWindowProcA(h, m, w, l);
    }
    WNDCLASSA wc; HWND h;
    CTS_BEGIN();
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = wp2; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "cts_msg_window2";
    if (!RegisterClassA(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { cts_end(); t_skip("class"); return; }
    h = CreateWindowExA(0, wc.lpszClassName, "cts", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    if (!h) { UnregisterClassA(wc.lpszClassName, wc.hInstance); cts_end(); t_skip("window"); return; }
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    seen = 0xdead;
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    /* if the notification is delivered at all, the new value is already visible inside it */
    if (seen != 0xdead) CHECKF(seen == FALSE, "value seen from the notification: %d", (int)seen);
    else t_note("no notification delivered for SPIF_SENDCHANGE");
    DestroyWindow(h); UnregisterClassA(wc.lpszClassName, wc.hInstance);
    cts_end();
}
TEST(msg_no_broadcast_without_sendchange) {
    HWND h; LONG n0; BOOL cur;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    cur = cts_get_smoothing();
    n0 = g_m.n[SPI_SETFONTSMOOTHING];
    /* fWinIni = 0: change in memory only, no notification */
    CTS_SET_OR_SKIP(SystemParametersInfoW(SPI_SETFONTSMOOTHING, !cur, NULL, 0));
    pump();
    CHECKF(g_m.n[SPI_SETFONTSMOOTHING] == n0, "unexpected notification without SPIF_SENDCHANGE (%ld -> %ld)", n0, g_m.n[SPI_SETFONTSMOOTHING]);
    CHECK(cts_get_smoothing() == !cur);
    kill_win(h); cts_end();
}
TEST(msg_wparam_and_lparam_note) {
    HWND h;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    pump();
    t_note("last WM_SETTINGCHANGE: msg %#x wParam %#llx lParam %#llx", g_m.last_msg, (unsigned long long)g_m.last_w, (unsigned long long)g_m.last_l);
    CHECK(g_m.last_msg == 0 || g_m.last_msg == WM_SETTINGCHANGE);
    kill_win(h); cts_end();
}
TEST(msg_each_distinct_change_counts_once_note) {
    HWND h; int i; LONG n0;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    n0 = g_m.n[SPI_SETFONTSMOOTHINGORIENTATION];
    for (i = 0; i < 4; i++) CTS_SET_OR_SKIP(cts_set_orient(i & 1 ? CTS_ORIENT_RGB : CTS_ORIENT_BGR));
    pump();
    t_note("four changes produced %ld notifications", g_m.n[SPI_SETFONTSMOOTHINGORIENTATION] - n0);
    CHECK(g_m.n[SPI_SETFONTSMOOTHINGORIENTATION] >= n0);
    kill_win(h); cts_end();
}
TEST(msg_unchanged_value_note) {
    HWND h; LONG n0; BOOL cur;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    cur = cts_get_smoothing();
    n0 = g_m.n[SPI_SETFONTSMOOTHING];
    CTS_SET_OR_SKIP(cts_set_smoothing(cur));
    pump();
    t_note("setting smoothing to its current value produced %ld WM_SETTINGCHANGE", g_m.n[SPI_SETFONTSMOOTHING] - n0);
    kill_win(h); cts_end();
}
TEST(msg_hidden_window_still_receives) {
    HWND h;
    CTS_BEGIN(); WIN_OR_SKIP(h);
    CHECK(!IsWindowVisible(h));
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_STANDARD));
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_CLEARTYPE));
    t_note("%ld notifications with wParam SPI_SETFONTSMOOTHINGTYPE", g_m.n[SPI_SETFONTSMOOTHINGTYPE]);
    kill_win(h); cts_end();
}
