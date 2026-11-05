#include "r2fs.h"

/* IDWriteTextFormat1 / IDWriteTextLayout2 SetFontFallback / GetFontFallback and what the layout does with the fallback.
 * IDWriteTextFormat/IDWriteTextLayout::SetFontFallback: AddRef the new pointer (NULL allowed), store it, Release the old one, return S_OK;
 * GetFontFallback: AddRef + return the stored pointer or NULL, always S_OK. The layout constructor copies the text format's fallback
 * (it queries IDWriteTextFormat1::GetFontFallback), so a layout starts with the format's fallback and later changes of the format do not
 * reach it. MapCharacters calls the stored fallback once per formatting span (a span being text with identical font
 * family, collection, weight, style, stretch, locale, size ...) with the span's base family/collection/weight/style/stretch and, when the
 * returned mappedLength is shorter than the span, again for the remaining text; mappedFont == NULL means "use the span's own font";
 * the returned scale multiplies the span's font size. With no fallback set the system fallback is used. */

/* ---- helpers ---------------------------------------------------------------------------------------------------------------------------------- */
static IDWriteTextFormat1 *fmt1_of(IDWriteTextFormat *fm)
{
    IDWriteTextFormat1 *p = NULL;
    if (!fm || FAILED(IDWriteTextFormat_QueryInterface(fm, &IID_IDWriteTextFormat1, (void **)&p))) return NULL;
    return p;
}
static IDWriteTextLayout2 *lay2_of(IDWriteTextLayout *l)
{
    IDWriteTextLayout2 *p = NULL;
    if (!l || FAILED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextLayout2, (void **)&p))) return NULL;
    return p;
}

typedef struct lfx { r2_fx X; IDWriteTextFormat *fm; IDWriteTextFormat1 *f1; } lfx;
static int lfx_open(lfx *L, const WCHAR *family)
{
    memset(L, 0, sizeof *L);
    if (!r2_open(&L->X)) return 0;
    L->fm = r2_format(&L->X, family, 20.0f);
    L->f1 = fmt1_of(L->fm);
    return L->fm && L->f1;
}
static void lfx_close(lfx *L) { DW_RELEASE(L->f1); DW_RELEASE(L->fm); r2_close(&L->X); }
#define LFX_BEGIN(L, fam) lfx L; if (!lfx_open(&L, fam)) { lfx_close(&L); t_skip("text format / IDWriteTextFormat1 unavailable"); return; } IDWriteFactory *f = L.X.f; (void)f

static IDWriteTextLayout *mk_layout(lfx *L, const WCHAR *text, float w)
{
    IDWriteTextLayout *l = NULL;
    if (FAILED(IDWriteFactory_CreateTextLayout(L->X.f, text, (UINT32)wcslen(text), L->fm, w, 200.0f, &l))) return NULL;
    return l;
}

/* "pos+len:Family[g,g]|..." for the recorded glyph runs */
static void runs_str(dwx_renderer *r, char *buf, int cap)
{
    int cur = 0, used = 0, k = 0; const dwx_draw *d;
    buf[0] = 0;
    while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
        char fam[64]; UINT32 i; int n;
        r2_run_family(r, k, fam, sizeof fam);
        n = snprintf(buf + used, cap - used, "%s%u+%u:%s[", k ? "|" : "", d->has_desc ? d->text_pos : 0xffff, d->has_desc ? d->text_len : 0, fam);
        if (n > 0 && used + n < cap) used += n;
        for (i = 0; i < d->nglyphs; i++) { n = snprintf(buf + used, cap - used, "%s%u", i ? "," : "", d->indices[i]); if (n > 0 && used + n < cap) used += n; }
        n = snprintf(buf + used, cap - used, "]"); if (n > 0 && used + n < cap) used += n;
        k++;
    }
}

#define CHECK_RUNS(layout, want) do { dwx_renderer *r_ = r2_draw(layout); char b_[512]; runs_str(r_, b_, sizeof b_); \
    t_check(!strcmp(b_, (want)), __FILE__, __LINE__, "glyph runs match", "got \"%s\" want \"%s\"", b_, (want)); dwx_release(r_); } while (0)

/* ---- round trip on the text format ------------------------------------------------------------------------------------------------------- */
RT(format_unset_get_is_null_and_ok)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *g = (IDWriteFontFallback *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(L.f1, &g)); CHECK(g == NULL);
    lfx_close(&L);
}

RT(format_set_get_same_pointer_and_refcounts)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X), *g = NULL;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    EXPECT_REFS(fb, 1);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, fb)); EXPECT_REFS(fb, 2);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(L.f1, &g)); CHECK(g == fb); EXPECT_REFS(fb, 3);
    DW_RELEASE(g); EXPECT_REFS(fb, 2);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(L.f1, &g)); CHECK(g == fb); DW_RELEASE(g);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL)); EXPECT_REFS(fb, 1);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(L.f1, &g)); CHECK(g == NULL);
    DW_RELEASE(fb); lfx_close(&L);
}

RT(format_set_same_fallback_twice_keeps_one_reference)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X);
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, fb));
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, fb));      /* AddRef before the old one is released: no transient zero */
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, fb));
    EXPECT_REFS(fb, 2);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL)); EXPECT_REFS(fb, 1);
    DW_RELEASE(fb); lfx_close(&L);
}

RT(format_replace_releases_the_previous_fallback)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *a = r2_std_fallback(&L.X), *b = r2_std_fallback(&L.X), *g = NULL;
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, a)); EXPECT_REFS(a, 2);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, b)); EXPECT_REFS(a, 1); EXPECT_REFS(b, 2);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(L.f1, &g)); CHECK(g == b); DW_RELEASE(g);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, a)); EXPECT_REFS(a, 2); EXPECT_REFS(b, 1);
    DW_RELEASE(a); DW_RELEASE(b); lfx_close(&L);
}

RT(format_release_drops_the_fallback_reference)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextFormat *fm2 = r2_format(&L.X, R2_MAIN, 12); IDWriteTextFormat1 *g1 = fmt1_of(fm2);
    if (!fb || !g1) { DW_RELEASE(g1); DW_RELEASE(fm2); DW_RELEASE(fb); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(g1, fb)); EXPECT_REFS(fb, 2);
    DW_RELEASE(g1); EXPECT_REFS(fb, 2);                          /* the QI'd interface is the same object */
    DW_RELEASE(fm2);
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb); lfx_close(&L);
}

RT(format_accepts_system_fallback_object)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFactory2 *f2 = r2_f2(f); IDWriteFontFallback *sys = NULL, *g = NULL;
    if (!f2 || FAILED(IDWriteFactory2_GetSystemFontFallback(f2, &sys)) || !sys) { DW_RELEASE(f2); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, sys)); EXPECT_REFS(sys, 2);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(L.f1, &g)); CHECK(g == sys); DW_RELEASE(g);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL));
    DW_RELEASE(sys); DW_RELEASE(f2); lfx_close(&L);
}

RT(format_accepts_application_fallback_object)
{
    LFX_BEGIN(L, R2_MAIN); r2_cfb *c = r2_cfb_new(L.X.coll); IDWriteFontFallback *g = NULL;
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, R2_CFB(c)));
    CHECKF(c->ref == 2, "application object refcount after SetFontFallback: %ld", (long)c->ref);
    CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(L.f1, &g)); CHECK(g == R2_CFB(c)); DW_RELEASE(g);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL));
    CHECKF(c->ref == 1, "application object refcount after clearing: %ld", (long)c->ref);
    EXPECT_EQ(c->n_map, 0);                                      /* setting a fallback never maps anything */
    IDWriteFontFallback_Release(R2_CFB(c)); CHECK(c->destroyed); r2_cfb_free(c);
    lfx_close(&L);
}

RT(format_fallback_survives_destruction_order)
{
    LFX_BEGIN(L, R2_MAIN); r2_cfb *c = r2_cfb_new(L.X.coll); IDWriteTextFormat *fm2 = r2_format(&L.X, R2_MAIN, 12); IDWriteTextFormat1 *g1 = fmt1_of(fm2);
    if (!g1) { r2_cfb_free(c); DW_RELEASE(fm2); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(g1, R2_CFB(c)));
    IDWriteFontFallback_Release(R2_CFB(c));                      /* the format now holds the only reference */
    CHECK(!c->destroyed); CHECKF(c->ref == 1, "ref %ld", (long)c->ref);
    DW_RELEASE(g1); DW_RELEASE(fm2);
    CHECK(c->destroyed);
    r2_cfb_free(c); lfx_close(&L);
}

/* ---- round trip on the layout ------------------------------------------------------------------------------------------------------------ */
RT(layout_unset_get_is_null)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteTextLayout *l = mk_layout(&L, L"abc", 100); IDWriteTextLayout2 *l2 = lay2_of(l); IDWriteFontFallback *g = (IDWriteFontFallback *)(ULONG_PTR)1;
    if (!l2) { DW_RELEASE(l); lfx_close(&L); t_skip("IDWriteTextLayout2 unavailable"); return; }
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == NULL);
    DW_RELEASE(l2); DW_RELEASE(l); lfx_close(&L);
}

RT(layout_set_get_and_refcounts)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteTextLayout *l = mk_layout(&L, L"abc", 100); IDWriteTextLayout2 *l2 = lay2_of(l); IDWriteFontFallback *fb = r2_std_fallback(&L.X), *g = NULL;
    if (!l2 || !fb) { DW_RELEASE(fb); DW_RELEASE(l2); DW_RELEASE(l); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, fb)); EXPECT_REFS(fb, 2);
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == fb); EXPECT_REFS(fb, 3); DW_RELEASE(g);
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, NULL)); EXPECT_REFS(fb, 1);
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == NULL);
    DW_RELEASE(fb); DW_RELEASE(l2); DW_RELEASE(l); lfx_close(&L);
}

RT(layout_release_drops_the_fallback_reference)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteTextLayout *l = mk_layout(&L, L"abc", 100); IDWriteTextLayout2 *l2 = lay2_of(l); IDWriteFontFallback *fb = r2_std_fallback(&L.X);
    if (!l2 || !fb) { DW_RELEASE(fb); DW_RELEASE(l2); DW_RELEASE(l); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, fb)); EXPECT_REFS(fb, 2);
    DW_RELEASE(l2); DW_RELEASE(l);
    EXPECT_REFS(fb, 1);
    DW_RELEASE(fb); lfx_close(&L);
}

RT(layout_starts_with_the_formats_fallback)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X), *g = NULL; IDWriteTextLayout *l; IDWriteTextLayout2 *l2;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, fb)); EXPECT_REFS(fb, 2);
    l = mk_layout(&L, L"abc", 100); l2 = lay2_of(l);
    if (l2) {
        EXPECT_REFS(fb, 3);                                       /* the layout took its own reference */
        CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == fb); DW_RELEASE(g);
    }
    DW_RELEASE(l2); DW_RELEASE(l);
    EXPECT_REFS(fb, 2);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL));
    DW_RELEASE(fb); lfx_close(&L);
}

RT(layout_without_format_fallback_has_none)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteTextLayout *l = mk_layout(&L, L"abc", 100); IDWriteTextLayout2 *l2 = lay2_of(l); IDWriteFontFallback *g = (IDWriteFontFallback *)(ULONG_PTR)1;
    if (!l2) { DW_RELEASE(l); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == NULL);
    DW_RELEASE(l2); DW_RELEASE(l); lfx_close(&L);
}

RT(layout_ignores_later_format_changes)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *a = r2_std_fallback(&L.X), *b = r2_std_fallback(&L.X), *g = NULL; IDWriteTextLayout *l; IDWriteTextLayout2 *l2;
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, a));
    l = mk_layout(&L, L"abc", 100); l2 = lay2_of(l);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, b));       /* after the layout exists */
    if (l2) { CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == a); DW_RELEASE(g); }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL));
    if (l2) { CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == a); DW_RELEASE(g); }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(a); DW_RELEASE(b); lfx_close(&L);
}

RT(layout_set_does_not_change_the_format)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *a = r2_std_fallback(&L.X), *b = r2_std_fallback(&L.X), *g = NULL; IDWriteTextLayout *l; IDWriteTextLayout2 *l2;
    if (!a || !b) { DW_RELEASE(a); DW_RELEASE(b); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, a));
    l = mk_layout(&L, L"abc", 100); l2 = lay2_of(l);
    if (l2) {
        CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, b));
        CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(L.f1, &g)); CHECK(g == a); DW_RELEASE(g);
        CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == b); DW_RELEASE(g);
    }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL));
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(a); DW_RELEASE(b); lfx_close(&L);
}

RT(layout_textformat1_interface_shares_the_layout_slot)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteTextLayout *l = mk_layout(&L, L"abc", 100); IDWriteTextLayout2 *l2 = lay2_of(l); IDWriteTextFormat1 *tf1 = NULL; IDWriteFontFallback *fb = r2_std_fallback(&L.X), *g = NULL;
    if (!l2 || !fb) { DW_RELEASE(fb); DW_RELEASE(l2); DW_RELEASE(l); lfx_close(&L); t_skip("unavailable"); return; }
    if (SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat1, (void **)&tf1)) && tf1) {
        CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(tf1, fb));      /* the adjustor thunk forwards to the layout's own SetFontFallback */
        CHECK_HR_OK(IDWriteTextLayout2_GetFontFallback(l2, &g)); CHECK(g == fb); DW_RELEASE(g);
        CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, NULL));
        CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(tf1, &g)); CHECK(g == NULL);
        DW_RELEASE(tf1);
    } else t_note("IDWriteTextLayout does not expose IDWriteTextFormat1");
    DW_RELEASE(fb); DW_RELEASE(l2); DW_RELEASE(l); lfx_close(&L);
}

RT(layout_fallback_outlives_the_factory_and_format)
{
    LFX_BEGIN(L, R2_MAIN); r2_cfb *c = r2_cfb_new(L.X.coll); IDWriteTextLayout *l = mk_layout(&L, L"abc", 100); IDWriteTextLayout2 *l2 = lay2_of(l);
    if (!l2) { r2_cfb_free(c); DW_RELEASE(l); lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, R2_CFB(c)));
    IDWriteFontFallback_Release(R2_CFB(c));
    DW_RELEASE(L.f1); DW_RELEASE(L.fm);                              /* the format goes away, the layout keeps the fallback */
    CHECK(!c->destroyed);
    DW_RELEASE(l2); DW_RELEASE(l);
    CHECK(c->destroyed);
    r2_cfb_free(c); lfx_close(&L);
}

/* ---- the fallback object the layout calls (application implementation) ------------------------------------------------------------------------ */
static IDWriteTextLayout2 *app_layout(lfx *L, r2_cfb *c, const WCHAR *text, IDWriteTextLayout **base)
{
    IDWriteTextLayout *l = mk_layout(L, text, 1000); IDWriteTextLayout2 *l2 = lay2_of(l);
    if (l2) IDWriteTextLayout2_SetFontFallback(l2, R2_CFB(c));
    *base = l;
    return l2;
}

#define APP_SETUP(L, c, text, l, l2) \
    LFX_BEGIN(L, R2_MAIN); r2_cfb *c = r2_cfb_new(L.X.coll); IDWriteTextLayout *l = NULL; IDWriteTextLayout2 *l2 = app_layout(&L, c, text, &l); \
    if (!l2) { DW_RELEASE(l); IDWriteFontFallback_Release(R2_CFB(c)); r2_cfb_free(c); lfx_close(&L); t_skip("IDWriteTextLayout2 unavailable"); return; }
#define APP_TEARDOWN(L, c, l, l2) do { DW_RELEASE(l2); DW_RELEASE(l); IDWriteFontFallback_Release(R2_CFB(c)); r2_cfb_free(c); lfx_close(&L); } while (0)

RT(app_fallback_is_called_when_the_layout_is_drawn)
{
    APP_SETUP(L, c, L"a\x3042", l, l2);       /* the fallback is only consulted for text the layout font cannot display */
    EXPECT_EQ(c->n_map, 0);
    { dwx_renderer *r = r2_draw(l); dwx_release(r); }
    CHECK(c->n_map >= 1);
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_receives_the_formats_properties)
{
    LFX_BEGIN(L, R2_MAIN); r2_cfb *c = r2_cfb_new(L.X.coll); IDWriteTextLayout *l = NULL; IDWriteTextLayout2 *l2; IDWriteTextFormat *fm = NULL;
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, R2_ALT, L.X.coll, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STRETCH_CONDENSED, 18.0f, L"fr-FR", &fm));
    CHECK_HR_OK(IDWriteFactory_CreateTextLayout(f, L"ab\x3042", 3, fm, 500, 100, &l)); l2 = lay2_of(l);
    if (l2) {
        dwx_renderer *r; const r2_cfb_call *k;
        CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, R2_CFB(c)));
        r = r2_draw(l); dwx_release(r);
        CHECK(c->n_map >= 1);
        k = &c->log[0];
        CHECK(k->coll == L.X.coll);
        CHECK(k->has_family && !lstrcmpW(k->family, R2_ALT));
        EXPECT_EQ(k->w, DWRITE_FONT_WEIGHT_BOLD); EXPECT_EQ(k->st, DWRITE_FONT_STYLE_ITALIC); EXPECT_EQ(k->sx, DWRITE_FONT_STRETCH_CONDENSED);
        CHECK(!lstrcmpiW(k->locale, L"fr-FR"));
        CHECKF(k->ntext >= 1 && k->ntext <= 3 && wcsstr(L"ab\x3042", k->text) != NULL, "window text \"%ls\" (%u chars)", k->text, k->ntext);
        t_note("first call: pos %u len %u locale \"%ls\"", k->pos, k->len, k->locale);
        IDWriteFontFallback_Release(R2_CFB(c));
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fm); r2_cfb_free(c); lfx_close(&L);
}

RT(app_fallback_default_properties)
{
    APP_SETUP(L, c, L"a\x3042", l, l2);
    { dwx_renderer *r = r2_draw(l); dwx_release(r); }
    CHECK(c->n_map >= 1);
    if (c->nlog) {
        CHECK(c->log[0].coll == L.X.coll); CHECK(c->log[0].has_family && !lstrcmpW(c->log[0].family, R2_MAIN));
        EXPECT_EQ(c->log[0].w, 400); EXPECT_EQ(c->log[0].st, DWRITE_FONT_STYLE_NORMAL); EXPECT_EQ(c->log[0].sx, DWRITE_FONT_STRETCH_NORMAL);
        CHECK(!lstrcmpiW(c->log[0].locale, L"en-us"));
    }
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_window_covers_the_whole_uniform_text)
{
    APP_SETUP(L, c, L"abc\x3042\x3044\x3046", l, l2);
    { dwx_renderer *r = r2_draw(l); dwx_release(r); }
    CHECK(c->nlog >= 1);
    if (c->nlog) {
        CHECK(c->log[0].ntext >= 1 && c->log[0].ntext <= 6 && wcsstr(L"abc\x3042\x3044\x3046", c->log[0].text) != NULL);
        t_note("window of the first call: pos %u len %u; calls %ld", c->log[0].pos, c->log[0].len, (long)c->n_map);
        EXPECT_EQ(c->log[0].ret_ml, c->log[0].ntext);           /* no rules: the application maps the whole window to "no font" */
    }
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_mapped_length_below_window_means_more_calls)
{
    APP_SETUP(L, c, L"\x3042\x3044\x3046", l, l2);
    r2_cfb_rule(c, 0x3040, 0x30ff, R2_CJK, 1.0f); c->max_ml = 1;
    { dwx_renderer *r = r2_draw(l); dwx_release(r); }
    CHECKF(c->n_map >= 3, "calls: %ld", (long)c->n_map);
    t_note("calls %ld for three characters mapped one at a time", (long)c->n_map);
    if (c->nlog >= 3) {
        EXPECT_EQ(c->log[0].ntext, 3);
        CHECK(c->log[0].ret_ml == 1);
        CHECK(!wcsncmp(c->log[1].text, L"\x3044\x3046", 2) || !wcsncmp(c->log[1].text, L"\x3044", 1));      /* the remaining text of the span */
    }
    { dwx_renderer *r = r2_draw(l); unsigned short g[8]; int n = r2_glyphs(r, g, 8); CHECK(n == 3); if (n == 3) CHECK(g[0] == 67 && g[1] == 67 && g[2] == 67); dwx_release(r); }
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_font_is_used_for_the_run)
{
    APP_SETUP(L, c, L"a\x3042" L"b", l, l2);
    r2_cfb_rule(c, 0x3040, 0x30ff, R2_CJK, 1.0f);
    CHECK_RUNS(l, "0+1:R2Main[28]|1+1:R2Cjk[67]|2+1:R2Main[29]");
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_null_font_means_the_base_font)
{
    APP_SETUP(L, c, L"a\x3042" L"b", l, l2);                    /* no rule: mappedFont NULL, mappedLength = the window (text read to the end) */
    CHECK_RUNS(l, "0+1:R2Main[28]|1+1:R2Main[0]|2+1:R2Main[29]");   /* U+3042 is the .notdef glyph of the base font; runs still split at the script changes */
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_scale_multiplies_the_font_size)
{
    APP_SETUP(L, c, L"a\x3042", l, l2);
    r2_cfb_rule(c, 0x3040, 0x30ff, R2_CJK, 2.0f);
    {
        dwx_renderer *r = r2_draw(l); const dwx_draw *d; int cur = 0;
        d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur); CHECK(d != NULL); if (d) CHECK_FLOAT_EQ(d->em, 20.0f);
        d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur); CHECK(d != NULL);
        if (d) { CHECK_FLOAT_EQ(d->em, 40.0f); if (d->nglyphs == 1) CHECK_NEAR(d->advances[0], R2_ADV(500, 40), 0.01); }
        dwx_release(r);
    }
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_scale_half)
{
    APP_SETUP(L, c, L"\x3042", l, l2);
    r2_cfb_rule(c, 0x3040, 0x30ff, R2_CJK, 0.5f);
    {
        dwx_renderer *r = r2_draw(l); const dwx_draw *d; int cur = 0;
        d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur); CHECK(d != NULL);
        if (d) { CHECK_FLOAT_EQ(d->em, 10.0f); if (d->nglyphs == 1) CHECK_NEAR(d->advances[0], R2_ADV(500, 10), 0.01); }
        dwx_release(r);
    }
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_per_range_weight_reaches_the_callback)
{
    APP_SETUP(L, c, L"\x3042\x3044\x3046\x3048\x304a\x304b", l, l2);
    DWRITE_TEXT_RANGE mid = { 2, 2 }; int i;
    CHECK_HR_OK(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, mid));
    { dwx_renderer *r = r2_draw(l); dwx_release(r); }
    CHECK(c->n_map >= 3);
    for (i = 0; i < c->nlog; i++) {
        const r2_cfb_call *k = &c->log[i]; const WCHAR *p = wcsstr(L"\x3042\x3044\x3046\x3048\x304a\x304b", k->text); int off = p ? (int)(p - L"\x3042\x3044\x3046\x3048\x304a\x304b") : -1;
        int bold_expected;
        if (off < 0 || !k->ntext) { t_check(0, __FILE__, __LINE__, "window text is a substring of the layout text", "call %d: \"%ls\"", i, k->text); continue; }
        bold_expected = (off >= 2 && off + (int)k->ntext <= 4);
        t_check(k->w == (bold_expected ? 700 : 400), __FILE__, __LINE__, "weight follows the range", "call %d window \"%ls\" at %d: weight %d", i, k->text, off, k->w);
        t_check(!(off < 2 && off + (int)k->ntext > 2) && !(off < 4 && off + (int)k->ntext > 4), __FILE__, __LINE__, "windows do not straddle format runs", "call %d window \"%ls\" at %d", i, k->text, off);
    }
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_per_range_family_reaches_the_callback)
{
    APP_SETUP(L, c, L"\x3042\x3044\x3046\x3048\x304a\x304b", l, l2);
    DWRITE_TEXT_RANGE first = { 0, 3 }; int i, seen_alt = 0, seen_main = 0;
    CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, R2_ALT, first));
    { dwx_renderer *r = r2_draw(l); dwx_release(r); }
    for (i = 0; i < c->nlog; i++) {
        const r2_cfb_call *k = &c->log[i]; const WCHAR *p = wcsstr(L"\x3042\x3044\x3046\x3048\x304a\x304b", k->text); int off = p ? (int)(p - L"\x3042\x3044\x3046\x3048\x304a\x304b") : -1;
        if (off < 0 || !k->ntext) continue;
        if (off < 3) { CHECKF(!lstrcmpW(k->family, R2_ALT), "call %d at %d: family \"%ls\"", i, off, k->family); seen_alt = 1; }
        else { CHECKF(!lstrcmpW(k->family, R2_MAIN), "call %d at %d: family \"%ls\"", i, off, k->family); seen_main = 1; }
    }
    CHECK(seen_alt && seen_main);
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_per_range_locale_reaches_the_callback)
{
    APP_SETUP(L, c, L"\x3042\x3044\x3046\x3048\x304a\x304b", l, l2);
    DWRITE_TEXT_RANGE tail = { 3, 3 }; int i, seen_ja = 0;
    CHECK_HR_OK(IDWriteTextLayout_SetLocaleName(l, L"ja-JP", tail));
    { dwx_renderer *r = r2_draw(l); dwx_release(r); }
    for (i = 0; i < c->nlog; i++) {
        const r2_cfb_call *k = &c->log[i]; const WCHAR *p = wcsstr(L"\x3042\x3044\x3046\x3048\x304a\x304b", k->text); int off = p ? (int)(p - L"\x3042\x3044\x3046\x3048\x304a\x304b") : -1;
        if (off < 0 || !k->ntext) continue;
        if (off >= 3) { CHECKF(!lstrcmpiW(k->locale, L"ja-JP"), "call %d at %d: locale \"%ls\"", i, off, k->locale); seen_ja = 1; }
        else CHECKF(!lstrcmpiW(k->locale, L"en-us"), "call %d at %d: locale \"%ls\"", i, off, k->locale);
    }
    CHECK(seen_ja);
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_not_asked_about_inline_objects)
{
    APP_SETUP(L, c, L"a\x3042" L"b", l, l2);
    dwx_inline *io = dwx_inline_new(10, 10, 8, FALSE); DWRITE_TEXT_RANGE r1 = { 1, 1 }; int i;
    CHECK_HR_OK(IDWriteTextLayout_SetInlineObject(l, (IDWriteInlineObject *)io, r1));
    { dwx_renderer *r = r2_draw(l); dwx_release(r); }
    for (i = 0; i < c->nlog; i++) CHECKF(wcschr(c->log[i].text, 0x3042) == NULL, "call %d window \"%ls\" contains the inline object's character", i, c->log[i].text);
    dwx_release(io);
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_replaced_between_draws)
{
    APP_SETUP(L, c, L"a\x3042", l, l2);
    r2_cfb *c2 = r2_cfb_new(L.X.coll); LONG n1;
    r2_cfb_rule(c, 0x3040, 0x30ff, R2_CJK, 1.0f); r2_cfb_rule(c2, 0x3040, 0x30ff, R2_CJK2, 1.0f);
    CHECK_RUNS(l, "0+1:R2Main[28]|1+1:R2Cjk[67]");
    n1 = c->n_map;
    CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, R2_CFB(c2)));
    EXPECT_EQ(c->ref, 1);                                       /* the first fallback's layout reference is gone; ours remains */
    CHECK_RUNS(l, "0+1:R2Main[28]|1+1:R2Cjk2[64]");
    CHECK(c2->n_map >= 1); EXPECT_EQ(c->n_map, n1);
    IDWriteTextLayout2_SetFontFallback(l2, NULL);
    IDWriteFontFallback_Release(R2_CFB(c2)); r2_cfb_free(c2);
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_second_draw_note)
{
    APP_SETUP(L, c, L"a\x3042", l, l2);
    LONG n1, n2;
    { dwx_renderer *r = r2_draw(l); dwx_release(r); } n1 = c->n_map;
    { dwx_renderer *r = r2_draw(l); dwx_release(r); } n2 = c->n_map;
    t_note("calls after the first draw: %ld, after the second draw: %ld", (long)n1, (long)n2);
    CHECK(n1 >= 1 && n2 >= n1);
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_failure_is_reported_note)
{
    APP_SETUP(L, c, L"abc", l, l2);
    dwx_renderer *r = dwx_renderer_new(); HRESULT hr;
    c->fail_hr = E_FAIL;
    hr = IDWriteTextLayout_Draw(l, NULL, (IDWriteTextRenderer *)r, 0, 0);
    t_note("Draw with a failing fallback: %s, glyph run callbacks %ld", dw_hr_name(hr), (long)r->count[DWX_DRAW_GLYPHRUN]);
    c->fail_hr = 0;
    dwx_release(r);
    { dwx_renderer *r2 = r2_draw(l); dwx_release(r2); }          /* the layout is still usable afterwards */
    APP_TEARDOWN(L, c, l, l2);
}

RT(app_fallback_text_beyond_mapped_is_requeried_with_suffix)
{
    APP_SETUP(L, c, L"ab\x3042\x3044" L"cd", l, l2);
    r2_cfb_rule(c, 0x3040, 0x30ff, R2_CJK, 1.0f);
    CHECK_RUNS(l, "0+2:R2Main[28,29]|2+2:R2Cjk[67,67]|4+2:R2Main[30,31]");
    CHECK(c->n_map >= 3);
    {
        int i, tot = 0;
        for (i = 0; i < c->nlog; i++) tot += (int)c->log[i].ret_ml;
        t_note("calls %ld, sum of returned mappedLength %d", (long)c->n_map, tot);
        CHECK(tot >= 6);
    }
    APP_TEARDOWN(L, c, l, l2);
}

/* ---- builder-made fallback and the layout ----------------------------------------------------------------------------------------------------------- */
static IDWriteTextLayout2 *builder_layout(lfx *L, IDWriteFontFallback *fb, const WCHAR *text, IDWriteTextLayout **base)
{
    IDWriteTextLayout *l = mk_layout(L, text, 1000); IDWriteTextLayout2 *l2 = lay2_of(l);
    if (l2) IDWriteTextLayout2_SetFontFallback(l2, fb);
    *base = l;
    return l2;
}

RT(draw_runs_follow_the_mappings)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2;
    static const struct { const WCHAR *text; const char *want; } rows[] = {
        { L"abc", "0+3:R2Main[28,29,30]" }, { L"ac\x3042\x2603" L"c", "0+2:R2Main[28,30]|2+1:R2Cjk[67]|3+1:R2Sym[64]|4+1:R2Main[30]" },
        { L"\x3042\x3044", "0+2:R2Cjk[67,67]" }, { L"\x4e00\x4e01\x4e02", "0+3:R2Cjk[64,65,68]" }, { L"\x2603\x2605\x2600", "0+3:R2Sym[64,65,68]" },
        { L"a\x3042" L"b", "0+1:R2Main[28]|1+1:R2Cjk[67]|2+1:R2Main[29]" },
        { L"\x3042" L"a", "0+1:R2Cjk[67]|1+1:R2Main[28]" },
    };
    unsigned i;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        l2 = builder_layout(&L, fb, rows[i].text, &l);
        if (l2) CHECK_RUNS(l, rows[i].want); else CHECK(l2 != NULL);
        DW_RELEASE(l2); DW_RELEASE(l);
    }
    DW_RELEASE(fb); lfx_close(&L);
}

RT(draw_supplementary_character_is_one_glyph_run_element)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    l2 = builder_layout(&L, fb, L"a\xD83D\xDE00" L"b\xD83D\xDE01", &l);
    if (l2) CHECK_RUNS(l, "0+1:R2Main[28]|1+2:R2Emoji[64]|3+1:R2Main[29]|4+2:R2Emoji[65]");
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}

RT(draw_unmapped_character_is_notdef_of_the_base_font)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    l2 = builder_layout(&L, fb, L"a\x0905", &l);                 /* nothing maps U+0905; the layout falls back to the base font's .notdef */
    if (l2) {
        dwx_renderer *r = r2_draw(l); unsigned short g[8]; int n = r2_glyphs(r, g, 8);
        CHECK(n == 2); if (n == 2) { EXPECT_EQ(g[0], 28); EXPECT_EQ(g[1], 0); }
        dwx_release(r);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}

RT(draw_empty_fallback_does_not_use_system_fallback)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; IDWriteTextLayout *l; IDWriteTextLayout2 *l2;
    if (!b) { lfx_close(&L); t_skip("unavailable"); return; }
    fb = r2_fb(b);
    if (fb && t_is_wine()) { DW_RELEASE(fb); DW_RELEASE(b); lfx_close(&L); t_skip("Wine crashes in MapCharacters when no mapping exists"); return; }
    l2 = builder_layout(&L, fb, L"a\x4e00", &l);
    if (l2) CHECK_RUNS(l, "0+1:R2Main[28]|1+1:R2Main[0]");      /* an empty custom fallback replaces the system fallback: CJK stays .notdef (Latin and Han are separate script runs) */
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); DW_RELEASE(b); lfx_close(&L);
}

RT(draw_without_fallback_uses_the_system_fallback_for_cjk)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteTextLayout *l = mk_layout(&L, L"a\x4e00", 1000); dwx_renderer *r;
    if (!l) { lfx_close(&L); t_skip("unavailable"); return; }
    r = r2_draw(l);
    {
        char fam[64]; int n = (int)r->count[DWX_DRAW_GLYPHRUN]; r2_run_family(r, n > 1 ? 1 : 0, fam, sizeof fam);
        CHECK(n >= 2);                                           /* the ideograph is rendered by another font than R2Main */
        t_note("CJK ideograph with the default fallback: family \"%s\"", fam);
        CHECK(strcmp(fam, "R2Main") != 0 || n < 2);
    }
    dwx_release(r); DW_RELEASE(l); lfx_close(&L);
}

RT(draw_base_family_filter_follows_the_layout_family)
{
    LFX_BEGIN(L, R2_ALT); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; IDWriteTextLayout *l; IDWriteTextLayout2 *l2; DWRITE_TEXT_RANGE first = { 0, 2 };
    if (!b) { lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, L.X.coll, NULL, R2_MAIN, 1.0f));          /* applies to text whose base family is R2Main */
    fb = r2_fb(b);
    if (fb && t_is_wine()) { DW_RELEASE(fb); DW_RELEASE(b); lfx_close(&L); t_skip("Wine ignores the base family filter"); return; }
    l2 = builder_layout(&L, fb, L"\x3042\x3044\x3046\x3042", &l);                           /* the layout's family is R2Alt: not mapped */
    if (l2) {
        dwx_renderer *r;
        CHECK_RUNS(l, "0+4:R2Alt[0,0,0,0]");
        CHECK_HR_OK(IDWriteTextLayout_SetFontFamilyName(l, R2_MAIN, first));
        r = r2_draw(l); {
            char s[512]; runs_str(r, s, sizeof s);
            CHECKF(!strncmp(s, "0+2:R2Cjk[67,67]|2+2:R2Alt[0,0]", 31), "got \"%s\"", s);
        }
        dwx_release(r);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); DW_RELEASE(b); lfx_close(&L);
}

RT(draw_locale_range_selects_the_mapping)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; IDWriteTextLayout *l; IDWriteTextLayout2 *l2; DWRITE_TEXT_RANGE a = { 0, 1 }, c = { 1, 1 };
    if (!b) { lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, L.X.coll, L"ja-JP", NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK2, L.X.coll, L"zh-CN", NULL, 1.0f));
    fb = r2_fb(b);
    l2 = builder_layout(&L, fb, L"\x3042\x3042", &l);
    if (l2) {
        CHECK_HR_OK(IDWriteTextLayout_SetLocaleName(l, L"ja-JP", a));
        CHECK_HR_OK(IDWriteTextLayout_SetLocaleName(l, L"zh-CN", c));
        CHECK_RUNS(l, "0+1:R2Cjk[67]|1+1:R2Cjk2[64]");
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); DW_RELEASE(b); lfx_close(&L);
}

RT(draw_weight_range_selects_the_face_of_the_target_family)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; IDWriteTextLayout *l; IDWriteTextLayout2 *l2; DWRITE_TEXT_RANGE bold = { 1, 1 };
    IDWriteFontFamily *fam = NULL; UINT32 idx; BOOL ex = FALSE;
    if (!b) { lfx_close(&L); t_skip("unavailable"); return; }
    if (FAILED(IDWriteFontCollection_FindFamilyName(L.X.coll, R2_LADDER, &idx, &ex)) || !ex || FAILED(IDWriteFontCollection_GetFontFamily(L.X.coll, idx, &fam)) ||
        IDWriteFontFamily_GetFontCount(fam) < 8) { DW_RELEASE(fam); DW_RELEASE(b); lfx_close(&L); t_skip("R2Ladder not grouped into one family"); return; }
    DW_RELEASE(fam);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_LADDER, L.X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    l2 = builder_layout(&L, fb, L"\x3042\x3042\x3042", &l);
    if (l2) {
        dwx_renderer *r; const dwx_draw *d; int cur = 0; int seen_bold = 0, seen_reg = 0;
        CHECK_HR_OK(IDWriteTextLayout_SetFontWeight(l, DWRITE_FONT_WEIGHT_BOLD, bold));
        r = r2_draw(l);
        while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
            if (d->has_desc && d->text_pos == 1) {
                IDWriteFontFace3 *f3 = NULL;
                if (SUCCEEDED(IDWriteFontFace_QueryInterface(d->face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) { CHECK(IDWriteFontFace3_GetWeight(f3) == DWRITE_FONT_WEIGHT_BOLD); seen_bold = 1; DW_RELEASE(f3); }
            } else if (d->has_desc) {
                IDWriteFontFace3 *f3 = NULL;
                if (SUCCEEDED(IDWriteFontFace_QueryInterface(d->face, &IID_IDWriteFontFace3, (void **)&f3)) && f3) { CHECK(IDWriteFontFace3_GetWeight(f3) == DWRITE_FONT_WEIGHT_NORMAL); seen_reg = 1; DW_RELEASE(f3); }
            }
        }
        CHECK(seen_bold && seen_reg);
        dwx_release(r);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); DW_RELEASE(b); lfx_close(&L);
}

RT(layout_family_name_is_not_replaced_by_the_fallback_font)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2; WCHAR name[64]; DWRITE_TEXT_RANGE rg;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    l2 = builder_layout(&L, fb, L"a\x3042" L"b", &l);
    if (l2) {
        { dwx_renderer *r = r2_draw(l); dwx_release(r); }
        CHECK_HR_OK(IDWriteTextLayout_GetFontFamilyName(l, 1, name, 64, &rg));       /* the position of the ideograph */
        CHECK(!lstrcmpW(name, R2_MAIN)); EXPECT_EQ(rg.startPosition, 0); t_note("range reported for the family name: length %u", rg.length);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}

/* ---- cluster metrics ------------------------------------------------------------------------------------------------------------------------------- */
typedef struct { const WCHAR *text; UINT32 n; UINT16 len[8]; int adv[8]; } crow;

RT(clusters_widths_follow_the_fallback_fonts)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2; unsigned i;
    /* advances in font units at 20 DIP / 2048: R2Main letters 1000, R2Cjk hiragana (gid 67) 500, R2Cjk ideographs 1000, R2Sym 1000, R2Emoji 1000 */
    static const crow rows[] = {
        { L"abc", 3, { 1, 1, 1 }, { 1000, 1000, 1000 } }, { L"a\x3042" L"b", 3, { 1, 1, 1 }, { 1000, 500, 1000 } }, { L"\x3042\x3044\x3046", 3, { 1, 1, 1 }, { 500, 500, 500 } },
        { L"\x4e00\x4e01", 2, { 1, 1 }, { 1000, 1000 } }, { L"a\x2603" L"b", 3, { 1, 1, 1 }, { 1000, 1000, 1000 } }, { L"a\xD83D\xDE00" L"b", 3, { 1, 2, 1 }, { 1000, 1000, 1000 } },
        { L"\xD83D\xDE00\xD83D\xDE01", 2, { 2, 2 }, { 1000, 1000 } },
    };
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    for (i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        DWRITE_CLUSTER_METRICS m[16]; UINT32 cnt = 0, k;
        l2 = builder_layout(&L, fb, rows[i].text, &l);
        if (!l2) { CHECK(l2 != NULL); continue; }
        CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, m, 16, &cnt), S_OK);
        CHECKF(cnt == rows[i].n, "row %u: %u clusters want %u", i, cnt, rows[i].n);
        for (k = 0; k < cnt && k < rows[i].n; k++) {
            CHECKF(m[k].length == rows[i].len[k], "row %u cluster %u: length %u want %u", i, k, m[k].length, rows[i].len[k]);
            CHECKF(DW_APPROX(m[k].width, R2_ADV(rows[i].adv[k], 20), 0.01), "row %u cluster %u: width %g want %g", i, k, m[k].width, R2_ADV(rows[i].adv[k], 20));
        }
        DW_RELEASE(l2); DW_RELEASE(l);
    }
    DW_RELEASE(fb); lfx_close(&L);
}

RT(clusters_unmapped_character_has_notdef_advance)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2; DWRITE_CLUSTER_METRICS m[4]; UINT32 cnt = 0;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    l2 = builder_layout(&L, fb, L"a\x0905", &l);
    if (l2) {
        CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, m, 4, &cnt), S_OK);
        EXPECT_EQ(cnt, 2);
        if (cnt == 2) CHECK_NEAR(m[1].width, R2_ADV(TTF_NOTDEF_ADV, 20), 0.01);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}

RT(clusters_mapping_scale_scales_the_advance)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; IDWriteTextLayout *l; IDWriteTextLayout2 *l2; DWRITE_CLUSTER_METRICS m[4]; UINT32 cnt = 0;
    static const float scales[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };
    unsigned i;
    if (!b) { lfx_close(&L); t_skip("unavailable"); return; }
    DW_RELEASE(b);
    for (i = 0; i < sizeof scales / sizeof scales[0]; i++) {
        b = r2_builder(f);
        CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, L.X.coll, NULL, NULL, scales[i]));
        fb = r2_fb(b);
        l2 = builder_layout(&L, fb, L"\x3042", &l);
        if (l2) {
            CHECK_HR(IDWriteTextLayout_GetClusterMetrics(l, m, 4, &cnt), S_OK);
            EXPECT_EQ(cnt, 1);
            if (cnt == 1) CHECKF(DW_APPROX(m[0].width, R2_ADV(500, 20) * scales[i], 0.01), "scale %g: width %g want %g", scales[i], m[0].width, R2_ADV(500, 20) * scales[i]);
        }
        DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); DW_RELEASE(b);
    }
    lfx_close(&L);
}

RT(metrics_total_width_is_the_sum_of_the_fonts_advances)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2; DWRITE_TEXT_METRICS tm;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    l2 = builder_layout(&L, fb, L"ab\x3042\x3044\x2603\xD83D\xDE00", &l);
    if (l2) {
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), S_OK);
        CHECK_NEAR(tm.width, R2_ADV(1000 + 1000 + 500 + 500 + 1000 + 1000, 20), 0.02);
        EXPECT_EQ(tm.lineCount, 1);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}

RT(metrics_width_changes_when_the_fallback_changes)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *a, *c; IDWriteTextLayout *l; IDWriteTextLayout2 *l2; DWRITE_TEXT_METRICS tm;
    if (!b) { lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, L.X.coll, NULL, NULL, 1.0f)); a = r2_fb(b);        /* hiragana advance 500 */
    DW_RELEASE(b); b = r2_builder(f);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK2, L.X.coll, NULL, NULL, 1.0f)); c = r2_fb(b);       /* hiragana gid 64: advance 1000 */
    l2 = builder_layout(&L, a, L"\x3042\x3044", &l);
    if (l2) {
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), S_OK); CHECK_NEAR(tm.width, R2_ADV(1000, 20), 0.02);
        CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, c));
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), S_OK); CHECK_NEAR(tm.width, R2_ADV(2000, 20), 0.02);        /* the change invalidates the cached layout */
        CHECK_HR_OK(IDWriteTextLayout2_SetFontFallback(l2, a));
        CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), S_OK); CHECK_NEAR(tm.width, R2_ADV(1000, 20), 0.02);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(a); DW_RELEASE(c); DW_RELEASE(b); lfx_close(&L);
}

RT(hit_test_positions_follow_cluster_widths)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2; unsigned i;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    l2 = builder_layout(&L, fb, L"a\x3042\x3044" L"b", &l);
    if (l2) {
        static const int want[5] = { 0, 1000, 1500, 2000, 3000 };
        for (i = 0; i < 5; i++) {
            float x = -1, y = -1; DWRITE_HIT_TEST_METRICS hm;
            CHECK_HR(IDWriteTextLayout_HitTestTextPosition(l, i, FALSE, &x, &y, &hm), S_OK);
            CHECKF(DW_APPROX(x, R2_ADV(want[i], 20), 0.02), "position %u: x %g want %g", i, x, R2_ADV(want[i], 20));
        }
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}

RT(line_wrap_uses_fallback_advances)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2; DWRITE_TEXT_METRICS tm;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    /* 6 hiragana of advance 500 = 3000 units = 29.3 DIP at 20 pt: a 20 DIP wide layout needs two lines, a 40 DIP wide one fits on one */
    l2 = builder_layout(&L, fb, L"\x3042\x3044\x3046\x3042\x3044\x3046", &l);
    if (l2) {
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 40.0f)); CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), S_OK); EXPECT_EQ(tm.lineCount, 1);
        CHECK_HR_OK(IDWriteTextLayout_SetMaxWidth(l, 20.0f)); CHECK_HR(IDWriteTextLayout_GetMetrics(l, &tm), S_OK); EXPECT_EQ(tm.lineCount, 2);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}

RT(glyph_run_descriptions_cover_the_text_exactly)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l; IDWriteTextLayout2 *l2; const WCHAR *text = L"ab\x3042\xD83D\xDE00\x2603\x0905" L"c";
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    l2 = builder_layout(&L, fb, text, &l);
    if (l2) {
        dwx_renderer *r = r2_draw(l); const dwx_draw *d; int cur = 0; UINT32 next = 0;
        while ((d = dwx_renderer_next(r, DWX_DRAW_GLYPHRUN, &cur)) != NULL) {
            CHECK(d->has_desc);
            if (!d->has_desc) continue;
            CHECKF(d->text_pos == next, "run starts at %u, expected %u", d->text_pos, next);
            next = d->text_pos + d->text_len;
        }
        EXPECT_EQ(next, wcslen(text));
        dwx_release(r);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}

RT(format_level_fallback_drives_layouts_created_later)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X); IDWriteTextLayout *l;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, fb));
    l = mk_layout(&L, L"a\x3042\x2603", 1000);
    if (l) CHECK_RUNS(l, "0+1:R2Main[28]|1+1:R2Cjk[67]|2+1:R2Sym[64]");
    DW_RELEASE(l);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL));
    DW_RELEASE(fb); lfx_close(&L);
}

RT(format_level_application_fallback_is_called_by_layouts)
{
    LFX_BEGIN(L, R2_MAIN); r2_cfb *c = r2_cfb_new(L.X.coll); IDWriteTextLayout *l;
    r2_cfb_rule(c, 0x3040, 0x30ff, R2_CJK, 1.0f);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, R2_CFB(c)));
    l = mk_layout(&L, L"a\x3042", 1000);
    if (l) { CHECK_RUNS(l, "0+1:R2Main[28]|1+1:R2Cjk[67]"); CHECK(c->n_map >= 1); }
    DW_RELEASE(l);
    CHECK_HR_OK(IDWriteTextFormat1_SetFontFallback(L.f1, NULL));
    IDWriteFontFallback_Release(R2_CFB(c)); CHECK(c->destroyed); r2_cfb_free(c);
    lfx_close(&L);
}

RT(layout_textformat_interface_exposes_the_layout_fallback)
{
    LFX_BEGIN(L, R2_MAIN); IDWriteFontFallback *fb = r2_std_fallback(&L.X), *g = NULL; IDWriteTextLayout *l; IDWriteTextLayout2 *l2; IDWriteTextFormat *fm2 = NULL; IDWriteTextFormat1 *f1b;
    if (!fb) { lfx_close(&L); t_skip("unavailable"); return; }
    l2 = builder_layout(&L, fb, L"abc", &l);
    /* a text format built from the layout's own IDWriteTextFormat interface */
    if (l2 && SUCCEEDED(IDWriteTextLayout_QueryInterface(l, &IID_IDWriteTextFormat, (void **)&fm2)) && fm2) {
        f1b = fmt1_of(fm2);
        if (f1b) { CHECK_HR_OK(IDWriteTextFormat1_GetFontFallback(f1b, &g)); CHECK(g == fb); DW_RELEASE(g); DW_RELEASE(f1b); }
        DW_RELEASE(fm2);
    }
    DW_RELEASE(l2); DW_RELEASE(l); DW_RELEASE(fb); lfx_close(&L);
}
