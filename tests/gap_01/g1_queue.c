/* IDWriteFontDownloadQueue::CancelDownload and IDWriteFontDownloadListener::DownloadCompleted: extra angles on an idle queue.
 *
 * Win10 dwrite.dll (DWriteFontDownloadQueue::CancelDownload !0x40680): takes the queue lock, cancels the pending downloads (or sets the cancel flag of the running
 * one), leaves the lock, waits for the completion event and returns S_OK. On an idle queue it neither changes the generation count nor calls a listener.
 */
#include "g1.h"

static IDWriteFontDownloadQueue *get_queue(IDWriteFactory *f)
{
    IDWriteFactory3 *f3 = (IDWriteFactory3 *)g1_qi((IUnknown *)f, &IID_IDWriteFactory3); IDWriteFontDownloadQueue *q = NULL;
    if (f3) IDWriteFactory3_GetFontDownloadQueue(f3, &q);
    DW_RELEASE(f3);
    return q;
}
TEST(gap1_dq_cancel_returns_exactly_s_ok_repeatedly)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontDownloadQueue *q = f ? get_queue(f) : NULL; int i;
    if (!q) { DW_RELEASE(f); t_skip("no download queue"); return; }
    for (i = 0; i < 100; i++) { HRESULT hr = IDWriteFontDownloadQueue_CancelDownload(q); if (hr != S_OK) { CHECKF(0, "call %d -> %s", i, dw_hr_name(hr)); break; } }
    DW_RELEASE(q); DW_RELEASE(f);
}
TEST(gap1_dq_cancel_does_not_notify_registered_listeners)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontDownloadQueue *q = f ? get_queue(f) : NULL; dwx_listener *l[4]; UINT32 tok[4]; int i;
    if (!q) { DW_RELEASE(f); t_skip("no download queue"); return; }
    for (i = 0; i < 4; i++) { l[i] = dwx_listener_new(); CHECK_HR_OK(IDWriteFontDownloadQueue_AddListener(q, (IDWriteFontDownloadListener *)l[i], &tok[i])); }
    for (i = 0; i < 5; i++) CHECK_HR(IDWriteFontDownloadQueue_CancelDownload(q), S_OK);
    for (i = 0; i < 4; i++) EXPECT_EQ(l[i]->n_completed, 0);
    EXPECT_EQ(IDWriteFontDownloadQueue_IsEmpty(q), TRUE);
    for (i = 0; i < 4; i++) CHECK_HR_OK(IDWriteFontDownloadQueue_RemoveListener(q, tok[i]));
    for (i = 0; i < 4; i++) { EXPECT_EQ(dwx_refs(l[i]), 1); dwx_release(l[i]); }
    DW_RELEASE(q); DW_RELEASE(f);
}
TEST(gap1_dq_cancel_leaves_generation_count_alone)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontDownloadQueue *q = f ? get_queue(f) : NULL; UINT64 g0, g1; int i;
    if (!q) { DW_RELEASE(f); t_skip("no download queue"); return; }
    g0 = IDWriteFontDownloadQueue_GetGenerationCount(q);
    for (i = 0; i < 10; i++) IDWriteFontDownloadQueue_CancelDownload(q);
    g1 = IDWriteFontDownloadQueue_GetGenerationCount(q);
    EXPECT_EQ(g0, g1);
    DW_RELEASE(q); DW_RELEASE(f);
}
TEST(gap1_dq_cancel_after_factory_release)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontDownloadQueue *q = f ? get_queue(f) : NULL;
    if (!q) { DW_RELEASE(f); t_skip("no download queue"); return; }
    DW_RELEASE(f);
    CHECK_HR(IDWriteFontDownloadQueue_CancelDownload(q), S_OK);            /* the queue keeps its factory alive */
    EXPECT_EQ(IDWriteFontDownloadQueue_IsEmpty(q), TRUE);
    DW_RELEASE(q);
}
TEST(gap1_dq_cancel_then_begin_completes_normally)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontDownloadQueue *q = f ? get_queue(f) : NULL; dwx_listener *l = dwx_listener_new(); UINT32 tok = 0;
    if (!q) { dwx_release(l); DW_RELEASE(f); t_skip("no download queue"); return; }
    CHECK_HR_OK(IDWriteFontDownloadQueue_AddListener(q, (IDWriteFontDownloadListener *)l, &tok));
    CHECK_HR(IDWriteFontDownloadQueue_CancelDownload(q), S_OK);
    CHECK_HR_OK(IDWriteFontDownloadQueue_BeginDownload(q, NULL));          /* a cancel on an idle queue leaves no stale cancel state behind */
    EXPECT_EQ(l->n_completed, 1);
    if (l->n_completed == 1) { EXPECT_EQ(l->last_hr, S_OK); EXPECT_EQ(l->last_queue, q); }
    CHECK_HR(IDWriteFontDownloadQueue_CancelDownload(q), S_OK);
    EXPECT_EQ(l->n_completed, 1);
    CHECK_HR_OK(IDWriteFontDownloadQueue_RemoveListener(q, tok));
    dwx_release(l); DW_RELEASE(q); DW_RELEASE(f);
}
TEST(gap1_dq_listener_receives_queue_identity_and_context)
{
    IDWriteFactory *f = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED); IDWriteFontDownloadQueue *q = f ? get_queue(f) : NULL; dwx_listener *l = dwx_listener_new(), *ctx = dwx_listener_new(); UINT32 tok = 0;
    if (!q) { dwx_release(l); dwx_release(ctx); DW_RELEASE(f); t_skip("no download queue"); return; }
    CHECK_HR_OK(IDWriteFontDownloadQueue_AddListener(q, (IDWriteFontDownloadListener *)l, &tok));
    CHECK_HR_OK(IDWriteFontDownloadQueue_BeginDownload(q, (IUnknown *)ctx));
    EXPECT_EQ(l->n_completed, 1); EXPECT_EQ(ctx->n_completed, 1);
    EXPECT_EQ(l->last_queue, q); EXPECT_EQ(l->last_context, ctx); EXPECT_EQ(ctx->last_queue, q); EXPECT_EQ(ctx->last_context, ctx);
    EXPECT_EQ(l->last_hr, S_OK); EXPECT_EQ(ctx->last_hr, S_OK);
    CHECK_HR_OK(IDWriteFontDownloadQueue_RemoveListener(q, tok));
    EXPECT_EQ(dwx_refs(ctx), 1);                                           /* the context is not retained */
    dwx_release(l); dwx_release(ctx); DW_RELEASE(q); DW_RELEASE(f);
}
