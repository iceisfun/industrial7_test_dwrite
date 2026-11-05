/* IDWriteFontFaceReference::Enqueue{Font,Character,Glyph,FileFragment}DownloadRequest on references that are not remote.
 *
 * Win10 dwrite.dll (!0xe4030, !0xe3e10, !0xe4130, !0xe3f20): each method asks the reference's loader for IDWriteRemoteFontFileLoader
 * (via TryGetRemoteFileLoader, !0xe4594). When the loader does not implement it the method returns S_OK immediately: the arguments are
 * never read, the download queue is not touched (not even created) and no listener is invoked. Only a remote loader leads to
 * FontDownloadQueue::Add*DownloadRequest; that path needs a real network font provider and is outside this corpus.
 */
#include "frr.h"

#define SIM_NONE DWRITE_FONT_SIMULATIONS_NONE

static IDWriteFontDownloadQueue *queue_of(frr_fix *x) {
    IDWriteFontDownloadQueue *q = NULL;
    HRESULT hr = IDWriteFactory3_GetFontDownloadQueue(x->f3, &q);
    CHECK_HR(hr, S_OK);
    return q;
}

TEST(enq_font_request_local_is_ok) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) { int k; for (k = 0; k < 3; k++) CHECK_HR(IDWriteFontFaceReference_EnqueueFontDownloadRequest(r), S_OK); }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(enq_character_request_local_is_ok) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        static const WCHAR text[] = { 'A', 'b', '1', 0x00E9, 0x4E2D, 0xD83D, 0xDE00 };
        UINT32 n;
        for (n = 0; n <= 7; n++) t_check(IDWriteFontFaceReference_EnqueueCharacterDownloadRequest(r, text, n) == S_OK, __FILE__, __LINE__, "chars", "count %u", n);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(enq_character_request_empty_and_null) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        /* a non-remote reference returns before looking at its arguments */
        CHECK_HR(IDWriteFontFaceReference_EnqueueCharacterDownloadRequest(r, NULL, 0), S_OK);
        CHECK_HR(IDWriteFontFaceReference_EnqueueCharacterDownloadRequest(r, L"", 0), S_OK);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(enq_glyph_request_local_is_ok) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        UINT16 g[8] = { 0, 1, 2, 27, 64, 68, 69, 0xFFFF };
        UINT32 n;
        for (n = 0; n <= 8; n++) t_check(IDWriteFontFaceReference_EnqueueGlyphDownloadRequest(r, g, n) == S_OK, __FILE__, __LINE__, "glyphs", "count %u", n);
        CHECK_HR(IDWriteFontFaceReference_EnqueueGlyphDownloadRequest(r, NULL, 0), S_OK);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(enq_fragment_request_local_is_ok) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    if (r) {
        static const UINT64 offs[] = { 0, 1, 512, 0xE28, 0x100000, 0xFFFFFFFFull, 0x100000000ull, 0xFFFFFFFFFFFFFFFFull };
        static const UINT64 lens[] = { 0, 1, 64, 4096, 0xFFFFFFFFull, 0x100000000ull, 0xFFFFFFFFFFFFFFFFull };
        size_t i, j;
        for (i = 0; i < sizeof offs / sizeof offs[0]; i++) for (j = 0; j < sizeof lens / sizeof lens[0]; j++) {
            HRESULT hr = IDWriteFontFaceReference_EnqueueFileFragmentDownloadRequest(r, offs[i], lens[j]);
            t_check(hr == S_OK, __FILE__, __LINE__, "fragment", "offset %llx size %llx -> %s", (unsigned long long)offs[i], (unsigned long long)lens[j], dw_hr_name(hr));
        }
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(enq_local_requests_leave_queue_idle) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontDownloadQueue *q = queue_of(&x);
    if (r && q) {
        UINT64 gen = IDWriteFontDownloadQueue_GetGenerationCount(q);
        UINT16 g[2] = { 1, 2 };
        IDWriteFontFaceReference_EnqueueFontDownloadRequest(r);
        IDWriteFontFaceReference_EnqueueCharacterDownloadRequest(r, L"AB", 2);
        IDWriteFontFaceReference_EnqueueGlyphDownloadRequest(r, g, 2);
        IDWriteFontFaceReference_EnqueueFileFragmentDownloadRequest(r, 0, 128);
        EXPECT_EQ(IDWriteFontDownloadQueue_IsEmpty(q), TRUE);
        EXPECT_EQ(IDWriteFontDownloadQueue_GetGenerationCount(q), gen);
    }
    DW_RELEASE(q); DW_RELEASE(r);
    frr_close(&x);
}
TEST(enq_local_requests_do_not_notify_listeners) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontDownloadQueue *q = queue_of(&x);
    dwx_listener *l = dwx_listener_new();
    UINT32 tok = 0;
    if (r && q && SUCCEEDED(IDWriteFontDownloadQueue_AddListener(q, (IDWriteFontDownloadListener *)l, &tok))) {
        UINT16 g = 3;
        IDWriteFontFaceReference_EnqueueFontDownloadRequest(r);
        IDWriteFontFaceReference_EnqueueCharacterDownloadRequest(r, L"x", 1);
        IDWriteFontFaceReference_EnqueueGlyphDownloadRequest(r, &g, 1);
        IDWriteFontFaceReference_EnqueueFileFragmentDownloadRequest(r, 0, 1);
        Sleep(50);
        EXPECT_EQ(l->n_completed, 0);
        CHECK_HR(IDWriteFontDownloadQueue_RemoveListener(q, tok), S_OK);
    }
    DW_RELEASE(q); DW_RELEASE(r);
    EXPECT_EQ(dwx_refs(l), 1);
    dwx_release(l);
    frr_close(&x);
}
TEST(enq_requests_on_every_simulation_and_index) {
    FRR_OPEN(x);
    UINT32 s, i;
    for (s = 0; s <= 3; s++) for (i = 0; i < 3; i++) {
        IDWriteFontFaceReference *r = frr_ref(&x, i, (DWRITE_FONT_SIMULATIONS)s);
        if (!r) continue;
        t_check(IDWriteFontFaceReference_EnqueueFontDownloadRequest(r) == S_OK, __FILE__, __LINE__, "font", "i=%u s=%u", i, s);
        t_check(IDWriteFontFaceReference_EnqueueFileFragmentDownloadRequest(r, 0, 16) == S_OK, __FILE__, __LINE__, "frag", "i=%u s=%u", i, s);
        DW_RELEASE(r);
    }
    frr_close(&x);
}
TEST(enq_requests_through_reference1_interface) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 0, SIM_NONE);
    IDWriteFontFaceReference1 *r1 = r ? frr_ref1(r) : NULL;
    if (r1) {
        UINT16 g = 1;
        CHECK_HR(IDWriteFontFaceReference1_EnqueueFontDownloadRequest(r1), S_OK);
        CHECK_HR(IDWriteFontFaceReference1_EnqueueCharacterDownloadRequest(r1, L"A", 1), S_OK);
        CHECK_HR(IDWriteFontFaceReference1_EnqueueGlyphDownloadRequest(r1, &g, 1), S_OK);
        CHECK_HR(IDWriteFontFaceReference1_EnqueueFileFragmentDownloadRequest(r1, 0, 8), S_OK);
    }
    DW_RELEASE(r1); DW_RELEASE(r);
    frr_close(&x);
}
TEST(enq_requests_on_system_path_reference) {
    IDWriteFactory3 *f3 = frr_shared3(); if (!f3) { t_skip("IDWriteFactory3 unsupported"); return; }
    IDWriteFontFaceReference *r = frr_sys_ref(f3, "arial.ttf", 0, SIM_NONE);
    if (!r) return;
    UINT16 g = 36;
    CHECK_HR(IDWriteFontFaceReference_EnqueueFontDownloadRequest(r), S_OK);
    CHECK_HR(IDWriteFontFaceReference_EnqueueCharacterDownloadRequest(r, L"Hello", 5), S_OK);
    CHECK_HR(IDWriteFontFaceReference_EnqueueGlyphDownloadRequest(r, &g, 1), S_OK);
    CHECK_HR(IDWriteFontFaceReference_EnqueueFileFragmentDownloadRequest(r, 0, 1024), S_OK);
    DW_RELEASE(r);
}
TEST(enq_requests_do_not_change_reference_state) {
    FRR_OPEN(x);
    IDWriteFontFaceReference *r = frr_ref(&x, 1, DWRITE_FONT_SIMULATIONS_BOLD);
    if (r) {
        IDWriteFontFaceReference_EnqueueFontDownloadRequest(r);
        EXPECT_EQ(IDWriteFontFaceReference_GetFontFaceIndex(r), 1);
        EXPECT_EQ(IDWriteFontFaceReference_GetSimulations(r), DWRITE_FONT_SIMULATIONS_BOLD);
        EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_LOCAL);
        EXPECT_REFS(r, 1);
    }
    DW_RELEASE(r);
    frr_close(&x);
}
TEST(enq_request_on_remote_flavoured_reference_does_not_touch_stream_methods) {
    /* Not run: with a loader that implements IDWriteRemoteFontFileLoader the request goes to the font download queue, which starts a
     * download thread that drives the provider's remote stream. This test only records that creating such a reference and asking for its
     * locality (the part exercised in props) never calls CreateRemoteStreamFromKey or the URL entry point. */
    FRR_OPEN(x);
    frr_remote *rm = frr_remote_new(x.ld, DWRITE_LOCALITY_REMOTE);
    IDWriteFontFile *file = NULL; IDWriteFontFaceReference *r = NULL;
    if (SUCCEEDED(frr_remote_register(rm, x.f)) && SUCCEEDED(IDWriteFactory_CreateCustomFontFileReference(x.f, &x.key, sizeof x.key, (IDWriteFontFileLoader *)rm, &file)) && file &&
        SUCCEEDED(IDWriteFactory3_CreateFontFaceReference_(x.f3, file, 0, SIM_NONE, &r)) && r) {
        EXPECT_EQ(IDWriteFontFaceReference_GetLocality(r), DWRITE_LOCALITY_REMOTE);
        EXPECT_EQ(rm->n_remote_create, 0);
        EXPECT_EQ(rm->n_url, 0);
    } else t_skip("cannot build a reference over the remote-flavoured loader");
    DW_RELEASE(r); DW_RELEASE(file);
    frr_remote_unregister(rm); dwx_release(rm);
    frr_close(&x);
}
