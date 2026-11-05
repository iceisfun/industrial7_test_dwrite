/* SystemParametersInfo font smoothing settings: the system-wide values DirectWrite consults (SPI 0x4A, 0x200A, 0x200C, 0x2012).
 * Every test that changes a value restores the original state before it ends (cts_end), and cts_begin restores it again if a
 * previous test was aborted.  Only SPIF_SENDCHANGE is used (no profile update). */
#include "cts.h"

static const char *tname(UINT t) { return t == 1 ? "STANDARD" : t == 2 ? "CLEARTYPE" : t == 0 ? "DEFAULT(0)" : "other"; }

TEST(spi_smoothing_get_succeeds) {
    BOOL v = 0xdead;
    CHECK(SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &v, 0));
    CHECKF(v == TRUE || v == FALSE, "BOOL value %d", (int)v);
    t_note("SPI_GETFONTSMOOTHING = %d", (int)v);
}
TEST(spi_smoothing_get_stable) {
    BOOL a = cts_get_smoothing(), b = cts_get_smoothing(), c = cts_get_smoothing();
    CHECK(a == b && b == c);
}
TEST(spi_type_get_succeeds) {
    UINT v = 0xdead;
    CHECK(SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &v, 0));
    CHECKF(v == 0 || v == CTS_FE_STANDARD || v == CTS_FE_CLEARTYPE, "type %u", v);
    t_note("SPI_GETFONTSMOOTHINGTYPE = %u (%s)", v, tname(v));
}
TEST(spi_contrast_get_in_documented_range) {
    UINT v = 0xdead;
    CHECK(SystemParametersInfoW(SPI_GETFONTSMOOTHINGCONTRAST, 0, &v, 0));
    /* documented: 1000..2200, default 1400; a profile that stores 0 (unset) may be reported as is, so 0 is only noted */
    if (v == 0) t_note("contrast reported as 0 (unset in the profile)");
    else CHECKF(v >= 1000 && v <= 2200, "contrast %u", v);
    t_note("SPI_GETFONTSMOOTHINGCONTRAST = %u", v);
}
TEST(spi_orientation_get_succeeds) {
    UINT v = 0xdead;
    CHECK(SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &v, 0));
    CHECKF(v == CTS_ORIENT_BGR || v == CTS_ORIENT_RGB, "orientation %u", v);
    t_note("SPI_GETFONTSMOOTHINGORIENTATION = %u (%s)", v, v ? "RGB" : "BGR");
}
TEST(spi_cleartype_get_note) {
    BOOL v = 0xdead; BOOL ok = SystemParametersInfoW(SPI_GETCLEARTYPE, 0, &v, 0);
    t_note("SPI_GETCLEARTYPE ok=%d value=%d (type=%u smoothing=%d)", ok, (int)v, cts_get_type(), (int)cts_get_smoothing());
    if (ok) CHECKF(v == TRUE || v == FALSE, "BOOL value %d", (int)v);
}
TEST(spi_gets_do_not_change_state) {
    cts_state a = cts_read_state(), b = cts_read_state();
    CHECK(a.smoothing == b.smoothing && a.type == b.type && a.contrast == b.contrast && a.orient == b.orient);
}

/* ---- smoothing on/off ---------------------------------------------------------------------------------------------- */
TEST(spi_smoothing_set_off_roundtrip) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    CHECK(cts_get_smoothing() == TRUE);
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    CHECK(cts_get_smoothing() == FALSE);
    cts_end();
    CHECK(cts_get_smoothing() == g_cts_saved.smoothing);
}
TEST(spi_smoothing_set_on_roundtrip) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    CHECK(cts_get_smoothing() == FALSE);
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    CHECK(cts_get_smoothing() == TRUE);
    cts_end();
}
TEST(spi_smoothing_set_same_value_idempotent) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    CHECK(cts_set_smoothing(TRUE));
    CHECK(cts_get_smoothing() == TRUE);
    CHECK(cts_set_smoothing(FALSE));
    CHECK(cts_set_smoothing(FALSE));
    CHECK(cts_get_smoothing() == FALSE);
    cts_end();
}
TEST(spi_smoothing_rapid_toggle_ends_in_last_state) {
    int i;
    CTS_BEGIN();
    for (i = 0; i < 6; i++) CTS_SET_OR_SKIP(cts_set_smoothing(i & 1));
    CHECK(cts_get_smoothing() == TRUE);          /* last iteration set TRUE */
    for (i = 0; i < 5; i++) CTS_SET_OR_SKIP(cts_set_smoothing(!(i & 1)));
    CHECK(cts_get_smoothing() == TRUE);
    cts_end();
}
TEST(spi_smoothing_nonzero_uiparam_note) {
    BOOL ok;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    ok = SystemParametersInfoW(SPI_SETFONTSMOOTHING, 7, NULL, SPIF_SENDCHANGE);
    t_note("SET smoothing uiParam=7 -> ok=%d, GET -> %d", ok, (int)cts_get_smoothing());
    cts_end();
}
TEST(spi_smoothing_independent_of_other_settings) {
    UINT t, c, o;
    CTS_BEGIN();
    t = cts_get_type(); c = cts_get_contrast(); o = cts_get_orient();
    CTS_SET_OR_SKIP(cts_set_smoothing(!cts_get_smoothing()));
    /* contrast and orientation are separate system values */
    CHECK(cts_get_contrast() == c);
    CHECK(cts_get_orient() == o);
    if (cts_get_type() != t) t_note("changing smoothing also changed the smoothing type %u -> %u", t, cts_get_type());
    cts_end();
}

/* ---- smoothing type ------------------------------------------------------------------------------------------------ */
TEST(spi_type_set_standard_roundtrip) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_CLEARTYPE));
    CHECK(cts_get_type() == CTS_FE_CLEARTYPE);
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_STANDARD));
    CHECK(cts_get_type() == CTS_FE_STANDARD);
    cts_end();
    CHECK(cts_get_type() == g_cts_saved.type);
}
TEST(spi_type_set_cleartype_roundtrip) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_STANDARD));
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_CLEARTYPE));
    CHECK(cts_get_type() == CTS_FE_CLEARTYPE);
    cts_end();
}
TEST(spi_type_toggle_matrix) {
    static const UINT seq[] = { 1, 2, 2, 1, 1, 2, 1, 2 };
    int i;
    CTS_BEGIN();
    for (i = 0; i < (int)(sizeof seq / sizeof seq[0]); i++) {
        CTS_SET_OR_SKIP(cts_set_type(seq[i]));
        CHECKF(cts_get_type() == seq[i], "step %d: got %u want %u", i, cts_get_type(), seq[i]);
    }
    cts_end();
}
TEST(spi_type_independent_of_smoothing_flag) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_STANDARD));
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    CHECK(cts_get_type() == CTS_FE_STANDARD);        /* the type value survives switching smoothing off */
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    CHECK(cts_get_type() == CTS_FE_STANDARD);
    cts_end();
}
TEST(spi_type_invalid_values_note) {
    static const UINT bad[] = { 3, 0x8000, 0xffff, 0x10000 };
    int i;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_STANDARD));
    for (i = 0; i < 4; i++) {
        BOOL ok; DWORD e;
        SetLastError(0);
        ok = cts_set_type(bad[i]); e = GetLastError();
        t_note("SET type %#x -> ok=%d err=%lu, GET -> %u", bad[i], ok, e, cts_get_type());
        cts_set_type(CTS_FE_STANDARD);
    }
    cts_end();
}

/* ---- contrast ------------------------------------------------------------------------------------------------------ */
TEST(spi_contrast_roundtrip_values) {
    static const UINT v[] = { 1000, 1100, 1400, 1800, 2000, 2200, 1001, 2199 };
    int i;
    CTS_BEGIN();
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        CTS_SET_OR_SKIP(cts_set_contrast(v[i]));
        CHECKF(cts_get_contrast() == v[i], "set %u got %u", v[i], cts_get_contrast());
    }
    cts_end();
    CHECK(cts_get_contrast() == g_cts_saved.contrast);
}
TEST(spi_contrast_independent_of_orientation_and_type) {
    UINT o, t;
    CTS_BEGIN();
    o = cts_get_orient(); t = cts_get_type();
    CTS_SET_OR_SKIP(cts_set_contrast(1234));
    CHECK(cts_get_orient() == o);
    CHECK(cts_get_type() == t);
    cts_end();
}
TEST(spi_contrast_out_of_range_note) {
    static const UINT bad[] = { 0, 999, 2201, 5000, 0xffffffffu };
    int i;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_contrast(1400));
    for (i = 0; i < 5; i++) {
        BOOL ok; DWORD e; UINT after;
        SetLastError(0); ok = cts_set_contrast(bad[i]); e = GetLastError(); after = cts_get_contrast();
        t_note("SET contrast %u -> ok=%d err=%lu, GET -> %u", bad[i], ok, e, after);
        /* the setter may store a raw out-of-range value or refuse it: not asserted */
        cts_set_contrast(1400);
    }
    cts_end();
}

/* ---- orientation --------------------------------------------------------------------------------------------------- */
TEST(spi_orientation_roundtrip_bgr_rgb) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    CHECK(cts_get_orient() == CTS_ORIENT_BGR);
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    CHECK(cts_get_orient() == CTS_ORIENT_RGB);
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_BGR));
    CHECK(cts_get_orient() == CTS_ORIENT_BGR);
    cts_end();
    CHECK(cts_get_orient() == g_cts_saved.orient);
}
TEST(spi_orientation_independent_of_smoothing) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    CTS_SET_OR_SKIP(cts_set_smoothing(!cts_get_smoothing()));
    CHECK(cts_get_orient() == CTS_ORIENT_RGB);
    cts_end();
}
TEST(spi_orientation_invalid_values_note) {
    static const UINT bad[] = { 2, 3, 0xff, 0x100 };
    int i;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_orient(CTS_ORIENT_RGB));
    for (i = 0; i < 4; i++) {
        BOOL ok; DWORD e;
        SetLastError(0); ok = cts_set_orient(bad[i]); e = GetLastError();
        t_note("SET orientation %#x -> ok=%d err=%lu, GET -> %u", bad[i], ok, e, cts_get_orient());
        cts_set_orient(CTS_ORIENT_RGB);
    }
    cts_end();
}

/* ---- ClearType flag ------------------------------------------------------------------------------------------------ */
TEST(spi_cleartype_set_roundtrip_note) {
    cts_state s;
    BOOL ok;
    CTS_BEGIN();
    s = cts_read_state();
    if (!s.ok_ct) { cts_end(); t_skip("SPI_GETCLEARTYPE unsupported"); return; }
    ok = cts_set_cleartype(!s.cleartype);
    t_note("SET cleartype %d -> ok=%d, GET cleartype %d, type %u, smoothing %d", !s.cleartype, ok, (int)cts_get_cleartype(), cts_get_type(), (int)cts_get_smoothing());
    cts_end();
    CHECK(cts_get_cleartype() == g_cts_saved.cleartype);
}
TEST(spi_cleartype_follows_type_note) {
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(TRUE));
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_CLEARTYPE));
    t_note("type CLEARTYPE, smoothing on -> SPI_GETCLEARTYPE %d", (int)cts_get_cleartype());
    CTS_SET_OR_SKIP(cts_set_type(CTS_FE_STANDARD));
    t_note("type STANDARD, smoothing on -> SPI_GETCLEARTYPE %d", (int)cts_get_cleartype());
    CTS_SET_OR_SKIP(cts_set_smoothing(FALSE));
    t_note("smoothing off -> SPI_GETCLEARTYPE %d", (int)cts_get_cleartype());
    cts_end();
}

/* ---- restore / persistence ----------------------------------------------------------------------------------------- */
TEST(spi_restore_returns_every_setting) {
    cts_state before, after;
    CTS_BEGIN();
    before = cts_read_state();
    CTS_SET_OR_SKIP(cts_set_smoothing(!before.smoothing));
    CTS_SET_OR_SKIP(cts_set_type(before.type == 1 ? 2 : 1));
    CTS_SET_OR_SKIP(cts_set_contrast(before.contrast == 1500 ? 1600 : 1500));
    CTS_SET_OR_SKIP(cts_set_orient(before.orient ? 0 : 1));
    cts_end();
    after = cts_read_state();
    CHECK(after.smoothing == before.smoothing);
    CHECK(after.type == before.type);
    CHECK(after.contrast == before.contrast);
    CHECK(after.orient == before.orient);
}
TEST(spi_sendchange_only_does_not_persist) {
    /* Without SPIF_UPDATEINIFILE the profile is not written: the HKCU values read at the start are identical afterwards. */
    static const char *names[] = { "FontSmoothing", "FontSmoothingType", "FontSmoothingGamma", "FontSmoothingOrientation" };
    char before[4][128], after[4][128]; int tb[4], ta[4], i;
    CTS_BEGIN();
    for (i = 0; i < 4; i++) tb[i] = cts_reg_any(HKEY_CURRENT_USER, CTS_DESKTOP, names[i], before[i], sizeof before[i]);
    CTS_SET_OR_SKIP(cts_set_smoothing(!cts_get_smoothing()));
    CTS_SET_OR_SKIP(cts_set_type(cts_get_type() == 1 ? 2 : 1));
    CTS_SET_OR_SKIP(cts_set_contrast(cts_get_contrast() == 1700 ? 1800 : 1700));
    CTS_SET_OR_SKIP(cts_set_orient(cts_get_orient() ? 0 : 1));
    for (i = 0; i < 4; i++) ta[i] = cts_reg_any(HKEY_CURRENT_USER, CTS_DESKTOP, names[i], after[i], sizeof after[i]);
    for (i = 0; i < 4; i++) {
        CHECKF(tb[i] == ta[i] && !strcmp(before[i], after[i]), "%s changed: type %d '%s' -> type %d '%s'", names[i], tb[i], before[i], ta[i], after[i]);
    }
    cts_end();
}
TEST(spi_values_visible_from_other_thread) {
    /* the settings are per window station, not per thread: a second thread sees the same value */
    struct R { BOOL smoothing; UINT orient; } r = { 0xdead, 0xdead };
    DWORD WINAPI th(void *arg) {
        struct R *p = arg; SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &p->smoothing, 0);
        SystemParametersInfoW(SPI_GETFONTSMOOTHINGORIENTATION, 0, &p->orient, 0); return 0;
    }
    HANDLE h;
    CTS_BEGIN();
    CTS_SET_OR_SKIP(cts_set_smoothing(!cts_get_smoothing()));
    CTS_SET_OR_SKIP(cts_set_orient(cts_get_orient() ? 0 : 1));
    h = CreateThread(NULL, 0, th, &r, 0, NULL);
    if (!h) { cts_end(); t_skip("CreateThread failed"); return; }
    CHECK(WaitForSingleObject(h, 10000) == WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(r.smoothing == cts_get_smoothing());
    CHECK(r.orient == cts_get_orient());
    cts_end();
}
TEST(spi_get_null_pointer_note) {
    BOOL ok; DWORD e;
    SetLastError(0);
    ok = SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, NULL, 0); e = GetLastError();
    t_note("GET smoothing with NULL pvParam -> ok=%d err=%lu", ok, e);
    SetLastError(0);
    ok = SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, NULL, 0); e = GetLastError();
    t_note("GET type with NULL pvParam -> ok=%d err=%lu", ok, e);
}
