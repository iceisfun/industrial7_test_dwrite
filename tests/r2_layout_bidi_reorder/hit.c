/* Hit testing on bidirectional runs: multiple boxes per range, caret splits at direction changes, point -> position.
 *
 * Facts (HitTestTextRange / HitTestPointInSpan / HitTestTextPosition):
 *  - HitTestTextRange clamps the range to the text, returns the boxes merged per visual run, writes nothing when more
 *    boxes are needed than the buffer holds (E_NOT_SUFFICIENT_BUFFER with the required count), and rejects start+length
 *    wrap-around or a NULL buffer with a non-zero capacity (E_INVALIDARG, count 0);
 *  - the leading edge of an odd-level cluster is its right edge, so the carets of two logically adjacent characters of
 *    opposite parity are NOT at the same x;
 *  - a point in the left half of a right-to-left cluster is a trailing hit.
 * Visual slots come from the reference bidi algorithm (rule L2).
 */
#include "bx.h"

typedef struct { const char *nm; int base; const WCHAR *text; } ht_row;

static IDWriteTextLayout *ht_make(bx_fx *x, const ht_row *r)
{
    IDWriteTextLayout *l = bx_layout_(x, BX_FAM, r->text, 16.0f, BX_W, 100.0f);
    if (l && r->base) IDWriteTextLayout_SetReadingDirection(l, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT);
    return l;
}

typedef struct { float l, r; } span;
static int cmp_span(const void *a, const void *b) { float x = ((const span *)a)->l, y = ((const span *)b)->l; return x < y ? -1 : x > y; }
/* merge touching spans into maximal intervals; returns count */
static int merge_spans(span *s, int n)
{
    int i, k = 0;
    qsort(s, n, sizeof *s, cmp_span);
    for (i = 0; i < n; i++) {
        if (k && s[i].l <= s[k - 1].r + EPS) { if (s[i].r > s[k - 1].r) s[k - 1].r = s[i].r; }
        else s[k++] = s[i];
    }
    return k;
}

/* ---- every sub-range of the text: merged visual extent equals the union of the characters' slots ------------------ */
static void ranges(const ht_row *r)
{
    BX_BEGIN(); UINT32 n = (UINT32)wcslen(r->text), a, len, i, s, vis[64], inv[64]; UINT8 lev[64]; float off; int bad = 0;
    IDWriteTextLayout *l = ht_make(&X, r);
    if (!l) { BX_END(); t_skip("layout"); return; }
    bx_ref_bidi(r->text, n, r->base, lev, vis);
    for (s = 0; s < n; s++) inv[vis[s]] = s;
    off = r->base ? BX_W - n * CW : 0.0f;
    for (a = 0; a < n; a++) for (len = 1; a + len <= n; len++) {
        bx_rg h = bx_range(l, a, len); span got[BX_MAXR], want[64]; int ng = 0, nw = 0, mg, mw; UINT32 total = 0, covered_ok = 1;
        for (i = 0; i < h.count; i++) {
            got[ng].l = h.m[i].left; got[ng].r = h.m[i].left + h.m[i].width; ng++; total += h.m[i].length;
            if (h.m[i].textPosition < a || h.m[i].textPosition + h.m[i].length > a + len) covered_ok = 0;
        }
        for (i = a; i < a + len; i++) { want[nw].l = off + inv[i] * CW; want[nw].r = want[nw].l + CW; nw++; }
        mg = merge_spans(got, ng); mw = merge_spans(want, nw);
        if (mg != mw || total != len || !covered_ok) bad++;
        else for (i = 0; i < (UINT32)mg; i++) if (!bx_feq(got[i].l, want[i].l) || !bx_feq(got[i].r, want[i].r)) { bad++; break; }
        if (bad == 1 && (mg != mw)) t_note("first mismatch range (%u,%u): %d visual intervals, want %d", a, len, mg, mw);
    }
    CHECKF(bad == 0, "%d sub-ranges with a wrong box set", bad);
    BX_DONE(l);
}

/* ---- every slot, quarter points: HitTestPoint position/trailing ------------------------------------------------- */
static void points(const ht_row *r)
{
    BX_BEGIN(); UINT32 n = (UINT32)wcslen(r->text), s, vis[64]; UINT8 lev[64]; float off;
    IDWriteTextLayout *l = ht_make(&X, r);
    if (!l) { BX_END(); t_skip("layout"); return; }
    bx_ref_bidi(r->text, n, r->base, lev, vis);
    off = r->base ? BX_W - n * CW : 0.0f;
    for (s = 0; s < n; s++) {
        UINT32 i = vis[s]; int odd = lev[i] & 1; float left = off + s * CW; int q;
        for (q = 0; q < 2; q++) {
            float x = left + (q ? 0.75f : 0.25f) * CW; bx_pt p = bx_point(l, x, 3.0f);
            int trailing = odd ? !q : q;                           /* left half of an odd cluster is its trailing half */
            CHECKF(p.m.textPosition == i && p.m.length == 1, "slot %u x=%g: position %u len %u want %u", s, x, p.m.textPosition, p.m.length, i);
            CHECKF(p.tr == trailing, "slot %u x=%g: isTrailingHit %d want %d", s, x, p.tr, trailing);
            CHECK(p.in == TRUE);
            CHECK(p.m.bidiLevel == lev[i]);
        }
    }
    BX_DONE(l);
}

/* ---- caret positions: leading / trailing edge of every character, plus the split between neighbours ---------------- */
static void carets(const ht_row *r)
{
    BX_BEGIN(); UINT32 n = (UINT32)wcslen(r->text), i, vis[64], inv[64], s; UINT8 lev[64]; float off; int splits = 0;
    IDWriteTextLayout *l = ht_make(&X, r);
    if (!l) { BX_END(); t_skip("layout"); return; }
    bx_ref_bidi(r->text, n, r->base, lev, vis);
    for (s = 0; s < n; s++) inv[vis[s]] = s;
    off = r->base ? BX_W - n * CW : 0.0f;
    for (i = 1; i < n; i++) {
        bx_tp prev_trail = bx_pos(l, i - 1, TRUE), next_lead = bx_pos(l, i, FALSE);
        float pl = off + inv[i - 1] * CW, nl = off + inv[i] * CW;
        float ex_prev = (lev[i - 1] & 1) ? pl : pl + CW, ex_next = (lev[i] & 1) ? nl + CW : nl;
        CHK_NEAR(prev_trail.x, ex_prev); CHK_NEAR(next_lead.x, ex_next);
        if (fabsf(ex_prev - ex_next) > EPS) splits++;
        /* same parity neighbours that are visually adjacent share one caret x */
        if (((lev[i - 1] ^ lev[i]) & 1) == 0 && lev[i - 1] == lev[i]) CHK_NEAR(prev_trail.x, next_lead.x);
    }
    t_note("%d of %u inter-character carets are split", splits, n - 1);
    BX_DONE(l);
}

#define HT(nm, base, text) \
    static const ht_row ht_##nm = { #nm, base, text }; \
    TEST(r2_layout_bidi_reorder_hit_ranges_##nm) { ranges(&ht_##nm); } \
    TEST(r2_layout_bidi_reorder_hit_points_##nm) { points(&ht_##nm); } \
    TEST(r2_layout_bidi_reorder_hit_carets_##nm) { carets(&ht_##nm); }

HT(mixed_ltr,       0, L"ab \u05d0\u05d1\u05d2 cd")
HT(mixed_rtl,       1, L"ab \u05d0\u05d1\u05d2 cd")
HT(hebrew_latin,    0, L"\u05d0\u05d1\u05d2 abc")
HT(hebrew_latin_rtl,1, L"\u05d0\u05d1\u05d2 abc")
HT(latin_hebrew,    0, L"abc \u05d0\u05d1\u05d2")
HT(digits_in_hebrew,0, L"\u05d0\u05d1 123 \u05d2\u05d3")
HT(digits_in_rtl,   1, L"\u05d0\u05d1 123 \u05d2\u05d3")
HT(arabic_numbers,  0, L"\u0627\u0628 \u0661\u0662 ab")
HT(nested_runs,     0, L"a \u05d0\u05d1 12 \u05d2 b")
HT(tight_alternate, 0, L"a\u05d0b\u05d1c")
HT(tight_alternate_rtl, 1, L"a\u05d0b\u05d1c")
HT(punct_between,   0, L"ab!\u05d0\u05d1!cd")
HT(only_hebrew,     0, L"\u05d0\u05d1\u05d2\u05d3")
HT(only_latin_rtl,  1, L"abcd")

/* ---- the two carets at a direction change really are at different places ----------------------------------------- */
TEST(r2_layout_bidi_reorder_hit_caret_split_at_boundary)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2", BX_W, 100.0f);
    bx_tp t2 = bx_pos(l, 2, TRUE), l3 = bx_pos(l, 3, FALSE);          /* trailing of the space / leading of the first Hebrew letter */
    CHK_NEAR(t2.x, 3 * CW);                                            /* right edge of the space */
    CHK_NEAR(l3.x, 6 * CW);                                            /* aleph is visually last: leading edge = right edge of the run */
    bx_tp t5 = bx_pos(l, 4, TRUE), e = bx_pos(l, 6, FALSE);
    CHK_NEAR(t5.x, 4 * CW);                                            /* bet trailing = its left edge */
    CHK_NEAR(e.x, 3 * CW);                                             /* end of text: trailing edge of the last letter = left edge of the run */
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_hit_caret_hebrew_then_latin)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1 ab", BX_W, 100.0f);
    /* visual: bet aleph space a b  (slots 0..4) */
    CHK_NEAR(bx_pos(l, 0, FALSE).x, 2 * CW); CHK_NEAR(bx_pos(l, 0, TRUE).x, CW);
    CHK_NEAR(bx_pos(l, 1, FALSE).x, CW);     CHK_NEAR(bx_pos(l, 1, TRUE).x, 0.0f);
    CHK_NEAR(bx_pos(l, 2, FALSE).x, 2 * CW); CHK_NEAR(bx_pos(l, 2, TRUE).x, 3 * CW);
    CHK_NEAR(bx_pos(l, 3, FALSE).x, 3 * CW); CHK_NEAR(bx_pos(l, 4, TRUE).x, 5 * CW);
    BX_DONE(l);
}

/* ---- HitTestTextRange protocol on a range that crosses runs ------------------------------------------------------- */
TEST(r2_layout_bidi_reorder_hit_range_buffer_protocol)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", BX_W, 100.0f);
    DWRITE_HIT_TEST_METRICS m[16], full[16]; UINT32 n = 0, cnt; HRESULT hr; unsigned i;
    hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, full, 16, &n);
    CHECK_HR_OK(hr); CHECK(n >= 3);
    if (n >= 3) {
        /* exact capacity returns the same boxes */
        cnt = 0; CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, m, n, &cnt)); CHECK(cnt == n);
        CHECK(!memcmp(m, full, n * sizeof m[0]));
        /* one short: error + needed count, and no box is written at all */
        memset(m, 0xcc, sizeof m); cnt = 0;
        hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, m, n - 1, &cnt);
        CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(cnt == n);
        for (i = 0; i < sizeof m[0]; i++) CHECK(((unsigned char *)&m[0])[i] == 0xcc);
        /* zero capacity */
        cnt = 0; hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, m, 0, &cnt);
        CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(cnt == n);
        /* NULL buffer, zero capacity: still the count */
        cnt = 0; hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, NULL, 0, &cnt);
        CHECK_HR(hr, DW_E_NOT_SUFFICIENT_BUFFER); CHECK(cnt == n);
    }
    /* NULL buffer with a capacity: invalid, count zeroed */
    cnt = 77; hr = IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, NULL, 4, &cnt);
    CHECK_HR(hr, DW_E_INVALIDARG); CHECK(cnt == 0);
    /* wrap-around of start + length */
    cnt = 77; hr = IDWriteTextLayout_HitTestTextRange(l, 5, 0xfffffffeu, 0, 0, m, 16, &cnt);
    CHECK_HR(hr, DW_E_INVALIDARG); CHECK(cnt == 0);
    cnt = 77; hr = IDWriteTextLayout_HitTestTextRange(l, 0xffffffffu, 2, 0, 0, m, 16, &cnt);
    CHECK_HR(hr, DW_E_INVALIDARG); CHECK(cnt == 0);
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_hit_range_origin_offsets_boxes)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", BX_W, 100.0f);
    DWRITE_HIT_TEST_METRICS a[16], b[16]; UINT32 na = 0, nb = 0, i;
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(l, 0, 9, 0, 0, a, 16, &na));
    CHECK_HR_OK(IDWriteTextLayout_HitTestTextRange(l, 0, 9, 10.5f, 20.25f, b, 16, &nb));
    CHECK(na == nb);
    for (i = 0; i < na && i < nb; i++) {
        CHK_NEAR(b[i].left - a[i].left, 10.5f); CHK_NEAR(b[i].top - a[i].top, 20.25f);
        CHK_NEAR(b[i].width, a[i].width); CHK_NEAR(b[i].height, a[i].height);
        CHECK(a[i].textPosition == b[i].textPosition && a[i].length == b[i].length && a[i].bidiLevel == b[i].bidiLevel);
    }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_hit_range_clamped_to_text)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2", BX_W, 100.0f);
    bx_rg a = bx_range(l, 0, 6), b = bx_range(l, 0, 100), c = bx_range(l, 3, 1000);
    UINT32 i, ta = 0, tb = 0, tc = 0;
    for (i = 0; i < a.count; i++) ta += a.m[i].length;
    for (i = 0; i < b.count; i++) tb += b.m[i].length;
    for (i = 0; i < c.count; i++) tc += c.m[i].length;
    CHECK(ta == 6 && tb == 6 && tc == 3);
    CHECK(a.count == b.count);
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_hit_range_box_levels)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2 cd", BX_W, 100.0f);
    bx_rg h = bx_range(l, 0, 9); UINT32 i;
    for (i = 0; i < h.count; i++) {
        UINT32 p;
        CHECK(h.m[i].length >= 1 && h.m[i].textPosition + h.m[i].length <= 9);
        CHECK(h.m[i].isText);
        for (p = h.m[i].textPosition; p < h.m[i].textPosition + h.m[i].length; p++) CHECKF(h.m[i].bidiLevel == ((p >= 3 && p <= 5) ? 1u : 0u), "box %u level %u at text %u", i, h.m[i].bidiLevel, p);
        CHECK(h.m[i].height > 0.0f);
    }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_hit_range_hebrew_only_one_box)
{
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1\u05d2\u05d3", BX_W, 100.0f);
    bx_rg h = bx_range(l, 1, 2);
    CHECK(h.count == 1);
    if (h.count == 1) { CHECK(h.m[0].textPosition == 1 && h.m[0].length == 2 && h.m[0].bidiLevel == 1); CHK_NEAR(h.m[0].left, CW); CHK_NEAR(h.m[0].width, 2 * CW); }
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_hit_range_split_by_nested_run)
{
    /* visual: bet aleph sp a b sp dalet gimel (slots 0..7). The logical range [1,6) = bet sp a b sp is visually discontiguous
     * (slot 0 and slots 2..5), the whole text is one block. */
    BX_BEGIN(); BX_LAYOUT(l, L"\u05d0\u05d1 ab \u05d2\u05d3", BX_W, 100.0f);
    bx_rg whole = bx_range(l, 0, 8), part = bx_range(l, 0, 2), mid = bx_range(l, 1, 5);
    span s[BX_MAXR]; UINT32 i; int k = 0;
    for (i = 0; i < whole.count; i++) { s[k].l = whole.m[i].left; s[k].r = whole.m[i].left + whole.m[i].width; k++; }
    CHECK(merge_spans(s, k) == 1);
    k = 0; for (i = 0; i < part.count; i++) { s[k].l = part.m[i].left; s[k].r = part.m[i].left + part.m[i].width; k++; }
    CHECK(merge_spans(s, k) == 1);
    CHECK(part.count >= 1 && mid.count >= 2);
    k = 0; for (i = 0; i < mid.count; i++) { s[k].l = mid.m[i].left; s[k].r = mid.m[i].left + mid.m[i].width; k++; }
    CHECK(merge_spans(s, k) == 2);
    BX_DONE(l);
}

TEST(r2_layout_bidi_reorder_hit_point_outside_mixed)
{
    BX_BEGIN(); BX_LAYOUT(l, L"ab \u05d0\u05d1\u05d2", BX_W, 100.0f);
    bx_pt p;
    p = bx_point(l, -5.0f, 3.0f);   CHECK(p.in == FALSE); CHECK(p.m.textPosition == 0);       /* left of the line: first slot is logical 0 */
    CHECK(p.tr == FALSE);
    p = bx_point(l, 6 * CW + 20.0f, 3.0f);  CHECK(p.in == FALSE);                              /* right of the line: last slot is the aleph (pos 3) */
    CHECK(p.m.textPosition == 3 && p.tr == FALSE);
    p = bx_point(l, 2.5f * CW, 500.0f); CHECK(p.in == FALSE);
    BX_DONE(l);
}
