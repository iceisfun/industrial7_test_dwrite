/* Simulated members in a family's font list (Windows 10): besides its real faces a family lists simulated ones.
 * Probed rule: a bold member (weight 700, BOLD simulation) is added to a (stretch, style) group whose faces are 400..550 (not for 100..300);
 * a normal-style member without an oblique twin gets an OBLIQUE member (style OBLIQUE, simulation OBLIQUE, same weight).
 * style: 0 normal, 1 oblique, 2 italic. simulations: 1 bold, 2 oblique. */
#include "e7.h"

typedef struct { int w, st, sims; } exp_t;
static void family_is(const char *name, const e7_face *f, int nf, const exp_t *e, int ne)
{
    e7_fam m; int i;
    if (!e7_fam_open(&m, name, f, nf)) return;
    EXPECT_EQ(m.count, ne);
    for (i = 0; i < ne; i++) CHECKF(e7_fam_has(&m, e[i].w, e[i].st, e[i].sims) == 1, "%s: member (w=%d st=%d sims=%d) not listed exactly once", name, e[i].w, e[i].st, e[i].sims);
    e7_fam_close(&m);
}
TEST(e7_sim_single_regular_gets_bold_and_oblique_members)
{
    static const e7_face f[] = { { 400, 0 } };
    static const exp_t e[] = { { 400, 0, 0 }, { 400, 1, 2 }, { 700, 0, 1 }, { 700, 1, 3 } };
    family_is("E7SimReg", f, 1, e, 4);
}
TEST(e7_sim_single_bold_gets_only_oblique)
{
    static const e7_face f[] = { { 700, 0 } };
    static const exp_t e[] = { { 700, 0, 0 }, { 700, 1, 2 } };
    family_is("E7SimBold", f, 1, e, 2);
}
TEST(e7_sim_single_italic_gets_bold_italic)
{
    static const e7_face f[] = { { 400, 1 } };
    static const exp_t e[] = { { 400, 2, 0 }, { 700, 2, 1 } };
    family_is("E7SimIta", f, 1, e, 2);
}
TEST(e7_sim_regular_and_bold_gets_oblique_pair)
{
    static const e7_face f[] = { { 400, 0 }, { 700, 0 } };
    static const exp_t e[] = { { 400, 0, 0 }, { 700, 0, 0 }, { 400, 1, 2 }, { 700, 1, 2 } };
    family_is("E7SimRB", f, 2, e, 4);
}
TEST(e7_sim_ribbi_set_only_adds_oblique)
{
    static const e7_face f[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 } };
    static const exp_t e[] = { { 400, 0, 0 }, { 700, 0, 0 }, { 400, 2, 0 }, { 700, 2, 0 }, { 400, 1, 2 }, { 700, 1, 2 } };
    family_is("E7SimRIBBI", f, 4, e, 6);
}
TEST(e7_sim_member_faces_carry_the_simulation)
{
    e7_face f = { 400, 0 }; e7_fam m; UINT32 i; int seen = 0;
    if (!e7_fam_open(&m, "E7SimFace", &f, 1)) return;
    for (i = 0; i < m.count; i++) {
        IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
        if (FAILED(IDWriteFontFamily_GetFont(m.fam, i, &font)) || !font) { CHECK(0); continue; }
        if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
            EXPECT_EQ(IDWriteFontFace_GetSimulations(face), IDWriteFont_GetSimulations(font));
            if (IDWriteFont_GetSimulations(font) == DWRITE_FONT_SIMULATIONS_BOLD) seen++;
            IDWriteFontFace_Release(face);
        }
        IDWriteFont_Release(font);
    }
    EXPECT_EQ(seen, 1);
    e7_fam_close(&m);
}
/* negative control: the rule depends on the face's weight (light faces get no bold member) and on the face set (more real
 * faces, fewer simulated ones), so the counts are not constant */
TEST(e7_sim_negative_control_light_and_counts)
{
    e7_fam a, b, c; static const e7_face fl = { 300, 0 }, fr = { 400, 0 }, f4[] = { { 400, 0 }, { 700, 0 }, { 400, 1 }, { 700, 1 } };
    if (!e7_fam_open(&a, "E7NegLight", &fl, 1)) return;
    EXPECT_EQ(a.count, 2);
    CHECK(e7_fam_has(&a, 300, 0, 0) == 1 && e7_fam_has(&a, 300, 1, 2) == 1);
    CHECK(e7_fam_has(&a, 700, 0, 1) == 0 && e7_fam_has(&a, 700, 1, 3) == 0);   /* no bold simulation for a light face */
    if (e7_fam_open(&b, "E7NegReg", &fr, 1)) {
        CHECK(b.count != a.count);
        CHECK(e7_fam_has(&b, 700, 0, 1) == 1);
        e7_fam_close(&b);
    }
    if (e7_fam_open(&c, "E7NegFull", f4, 4)) {
        CHECK(e7_fam_has(&c, 700, 0, 1) == 0 && e7_fam_has(&c, 700, 0, 0) == 1);   /* a real bold face suppresses the bold simulation */
        e7_fam_close(&c);
    }
    e7_fam_close(&a);
}
