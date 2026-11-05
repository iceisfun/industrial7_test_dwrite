/* Randomised (fixed-seed, deterministic) comparison of the range reported by every getter with a reference model of the run table.
 * Model: the complete format of every position; a reported range is the maximal span of positions with an identical complete format
 * (positions at and beyond the model size hold the default and extend to 0xFFFFFFFF). Ranges may start and end anywhere, including past
 * the 24-character text: nothing is clipped.
 */
#include "tlr.h"

#define TEXT24 L"abcdefghijklmnopqrstuvwx"
enum { P_WEIGHT, P_STYLE, P_STRETCH, P_SIZE, P_UL, P_ST, P_EFFECT, P_COUNT };
static const DWRITE_FONT_WEIGHT k_w[3] = { DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_WEIGHT_LIGHT };
static const DWRITE_FONT_STYLE k_s[3] = { DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STYLE_ITALIC, DWRITE_FONT_STYLE_OBLIQUE };
static const DWRITE_FONT_STRETCH k_t[3] = { DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, DWRITE_FONT_STRETCH_EXPANDED };
static const float k_z[3] = { 16.0f, 24.0f, 8.5f };

typedef struct mctx { tlr_model m; IDWriteTextLayout *l; dwx_effect *eff[3]; int failures; UINT32 seed; } mctx;

static void apply(mctx *c, int prop, int v, UINT32 s, UINT32 len)
{
    UINT32 i; HRESULT hr = S_OK; DWRITE_TEXT_RANGE r = RNG(s, len);
    switch (prop) {
    case P_WEIGHT:  hr = IDWriteTextLayout_SetFontWeight(c->l, k_w[v], r); break;
    case P_STYLE:   hr = IDWriteTextLayout_SetFontStyle(c->l, k_s[v], r); break;
    case P_STRETCH: hr = IDWriteTextLayout_SetFontStretch(c->l, k_t[v], r); break;
    case P_SIZE:    hr = IDWriteTextLayout_SetFontSize(c->l, k_z[v], r); break;
    case P_UL:      hr = IDWriteTextLayout_SetUnderline(c->l, v != 0, r); break;
    case P_ST:      hr = IDWriteTextLayout_SetStrikethrough(c->l, v != 0, r); break;
    case P_EFFECT:  hr = IDWriteTextLayout_SetDrawingEffect(c->l, v ? (IUnknown *)c->eff[v] : NULL, r); break;
    }
    t_check(hr == S_OK, __FILE__, __LINE__, "model set", "prop %d value %d {%u,%u} -> %s", prop, v, s, len, dw_hr_name(hr));
    for (i = s; i < s + len && i < TLR_MODEL_N; i++) {
        tlr_cell *cell = &c->m.c[i];
        switch (prop) {
        case P_WEIGHT: cell->weight = v; break; case P_STYLE: cell->style = v; break; case P_STRETCH: cell->stretch = v; break;
        case P_SIZE: cell->size = k_z[v]; break; case P_UL: cell->ul = v != 0; break; case P_ST: cell->st = v != 0; break; case P_EFFECT: cell->effect = v; break;
        }
    }
}

static int verify(mctx *c, int step)
{
    UINT32 pos;
    for (pos = 0; pos < 175; pos++) {
        tlr_cell want; DWRITE_TEXT_RANGE wr, r; int i, got; float z; IUnknown *eu;
#define RANGE_OK() (r.startPosition == wr.startPosition && r.length == wr.length)
#define BAD(what, g, w) do { t_check(0, __FILE__, __LINE__, "model comparison", "step %d, %s at position %u: value %d (model %d), range {%u,%u} (model {%u,%u})", \
                              step, what, pos, (int)(g), (int)(w), r.startPosition, r.length, wr.startPosition, wr.length); return 1; } while (0)
        tlr_model_run(&c->m, pos, &want, &wr);
        got = -1; { DWRITE_FONT_WEIGHT x = tlr_weight(c->l, pos, &r); for (i = 0; i < 3; i++) if (x == k_w[i]) got = i; }
        if (got != want.weight || !RANGE_OK()) BAD("weight", got, want.weight);
        got = -1; { DWRITE_FONT_STYLE x = tlr_style(c->l, pos, &r); for (i = 0; i < 3; i++) if (x == k_s[i]) got = i; }
        if (got != want.style || !RANGE_OK()) BAD("style", got, want.style);
        got = -1; { DWRITE_FONT_STRETCH x = tlr_stretch(c->l, pos, &r); for (i = 0; i < 3; i++) if (x == k_t[i]) got = i; }
        if (got != want.stretch || !RANGE_OK()) BAD("stretch", got, want.stretch);
        z = tlr_size(c->l, pos, &r);
        if (!tlr_fbits(z, want.size) || !RANGE_OK()) BAD("size", (int)z, (int)want.size);
        got = tlr_underline(c->l, pos, &r) ? 1 : 0;
        if (got != want.ul || !RANGE_OK()) BAD("underline", got, want.ul);
        got = tlr_strike(c->l, pos, &r) ? 1 : 0;
        if (got != want.st || !RANGE_OK()) BAD("strikethrough", got, want.st);
        eu = tlr_effect(c->l, pos, &r); got = 0; for (i = 1; i < 3; i++) if (eu && eu == (IUnknown *)c->eff[i]) got = i;
        DW_RELEASE(eu);
        if (got != want.effect || !RANGE_OK()) BAD("effect", got, want.effect);
    }
    return 0;
}

static void run_model(UINT32 seed, int steps, int props_mask, int verify_every)
{
    mctx c; tlr_fx x; tlr_cell def; int i; memset(&c, 0, sizeof c); memset(&x, 0, sizeof x);
    if (!tlr_open(&x)) { tlr_close(&x); t_skip("fixture"); return; }
    c.l = tlr_layout_(&x, TLR_FAM, TEXT24, 16.0f);
    if (!c.l) { tlr_close(&x); t_skip("layout"); return; }
    c.eff[1] = dwx_effect_new(1); c.eff[2] = dwx_effect_new(2);
    memset(&def, 0, sizeof def); def.size = 16.0f;
    tlr_model_init(&c.m, &def);
    c.seed = seed;
    for (i = 0; i < steps; i++) {
        int prop, v; UINT32 s, len, form;
        do prop = (int)(tlr_rand(&c.seed) % P_COUNT); while (!(props_mask & (1 << prop)));
        v = (int)(tlr_rand(&c.seed) % ((prop == P_UL || prop == P_ST) ? 2 : 3));
        s = tlr_rand(&c.seed) % 100;
        form = tlr_rand(&c.seed) % 8;
        len = form == 0 ? 0 : form == 1 ? 1 : (tlr_rand(&c.seed) % 60);
        apply(&c, prop, v, s, len);
        if ((i % verify_every) == verify_every - 1 && verify(&c, i)) break;
    }
    verify(&c, steps);
    IDWriteTextLayout_Release(c.l); dwx_release(c.eff[1]); dwx_release(c.eff[2]);
    tlr_close(&x);
}

TEST(model_weight_only_seed_1)       { run_model(1, 400, 1 << P_WEIGHT, 1); }
TEST(model_weight_only_seed_2)       { run_model(0xC0FFEE, 400, 1 << P_WEIGHT, 1); }
TEST(model_weight_only_seed_3)       { run_model(77777, 400, 1 << P_WEIGHT, 5); }
TEST(model_size_only)                { run_model(5, 300, 1 << P_SIZE, 1); }
TEST(model_underline_only)           { run_model(6, 300, 1 << P_UL, 1); }
TEST(model_strikethrough_only)       { run_model(7, 300, 1 << P_ST, 1); }
TEST(model_effect_only)              { run_model(8, 300, 1 << P_EFFECT, 1); }
TEST(model_style_stretch)            { run_model(9, 300, (1 << P_STYLE) | (1 << P_STRETCH), 1); }
TEST(model_two_properties_weight_underline) { run_model(10, 400, (1 << P_WEIGHT) | (1 << P_UL), 1); }
TEST(model_all_properties_seed_1)    { run_model(11, 500, (1 << P_COUNT) - 1, 1); }
TEST(model_all_properties_seed_2)    { run_model(0xBEEF, 500, (1 << P_COUNT) - 1, 1); }
TEST(model_all_properties_seed_3)    { run_model(31337, 800, (1 << P_COUNT) - 1, 10); }
TEST(model_all_properties_seed_4)    { run_model(4242, 800, (1 << P_COUNT) - 1, 10); }
TEST(model_short_sequences_many_seeds)
{
    UINT32 s;
    for (s = 100; s < 130; s++) run_model(s, 12, (1 << P_COUNT) - 1, 1);
}
TEST(model_dense_small_ranges)
{
    /* mostly single-character ranges: exercises splitting and re-merging at every position */
    mctx c; tlr_fx x; tlr_cell def; int i; memset(&c, 0, sizeof c); memset(&x, 0, sizeof x);
    if (!tlr_open(&x)) { tlr_close(&x); t_skip("fixture"); return; }
    c.l = tlr_layout_(&x, TLR_FAM, TEXT24, 16.0f);
    if (!c.l) { tlr_close(&x); t_skip("layout"); return; }
    c.eff[1] = dwx_effect_new(1); c.eff[2] = dwx_effect_new(2);
    memset(&def, 0, sizeof def); def.size = 16.0f; tlr_model_init(&c.m, &def); c.seed = 99;
    for (i = 0; i < 600; i++) {
        int v = (int)(tlr_rand(&c.seed) % 3); UINT32 s = tlr_rand(&c.seed) % 40, len = 1 + tlr_rand(&c.seed) % 2;
        apply(&c, P_WEIGHT, v, s, len);
        if (i % 3 == 0) apply(&c, P_UL, (int)(tlr_rand(&c.seed) % 2), tlr_rand(&c.seed) % 40, 1 + tlr_rand(&c.seed) % 3);
        if (verify(&c, i)) break;
    }
    IDWriteTextLayout_Release(c.l); dwx_release(c.eff[1]); dwx_release(c.eff[2]); tlr_close(&x);
}
