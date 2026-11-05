/* Vertical glyph support of IDWriteFontFace / IDWriteFontFace1: GetVerticalGlyphVariants, HasVerticalGlyphVariants, vertical design metrics
 * and vertical advances.
 *
 * IDWriteFontFace::GetVerticalGlyphVariants: count 0 returns S_OK without touching the buffers; otherwise a NULL nominal array zeroes the output
 * array and returns E_INVALIDARG. The variants come from the GSUB 'vert' feature of the DFLT script's default language system, single substitution lookups
 * only (type 1, directly or via an extension subtable); a glyph without an entry (or whose variant is glyph 0) keeps its index; nominal == variants is allowed.
 * HasVerticalGlyphVariants is TRUE exactly when such a substitution map was found.
 * IDWriteFontFace::GetDesignGlyphMetrics fills advanceHeight and topSideBearing from vmtx; the isSideways argument only matters for simulated
 * oblique fonts. A NULL glyph index array is E_INVALIDARG (even for a zero count) and the metrics array is zeroed first.
 * IDWriteFontFace::GetDesignGlyphAdvances zeroes the advance array first, takes advanceHeight when isSideways is TRUE, and rejects a NULL index array
 * only for a non-zero count.
 */
#include "vto.h"

typedef struct vf { vt_fx X; IDWriteFontFace *face; IDWriteFontFace1 *f1; } vf;
#define VF_BEGIN(fam) VT_BEGIN(); IDWriteFontFace *face = vt_face(&X, fam); IDWriteFontFace1 *f1 = NULL; \
    if (!face || FAILED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace1, (void **)&f1))) { DW_RELEASE(face); VT_END(); t_skip("IDWriteFontFace1 unavailable"); return; }
#define VF_END() do { DW_RELEASE(f1); DW_RELEASE(face); VT_END(); } while (0)

/* ---- HasVerticalGlyphVariants -------------------------------------------------------------------------------- */
TEST(has_variants_true_with_vert_feature)
{
    VF_BEGIN(VT_SUB); CHECK(IDWriteFontFace1_HasVerticalGlyphVariants(f1)); VF_END();
}
TEST(has_variants_false_without_gsub)
{
    VF_BEGIN(VT_FAM); CHECK(!IDWriteFontFace1_HasVerticalGlyphVariants(f1)); VF_END();
}
TEST(has_variants_false_for_plain_font)
{
    VF_BEGIN(VT_PLAIN); CHECK(!IDWriteFontFace1_HasVerticalGlyphVariants(f1)); VF_END();
}
TEST(has_variants_is_repeatable_and_refcount_neutral)
{
    VF_BEGIN(VT_SUB); ULONG r0 = dw_refcount((IUnknown *)f1); int i;
    for (i = 0; i < 5; i++) CHECK(IDWriteFontFace1_HasVerticalGlyphVariants(f1));
    EXPECT_REFS(f1, r0);
    VF_END();
}
TEST(has_variants_same_through_other_face_interfaces)
{
    VF_BEGIN(VT_SUB); IDWriteFontFace2 *f2 = NULL; IDWriteFontFace3 *f3 = NULL;
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace2, (void **)&f2))) { CHECK(IDWriteFontFace1_HasVerticalGlyphVariants((IDWriteFontFace1 *)f2)); DW_RELEASE(f2); }
    if (SUCCEEDED(IDWriteFontFace_QueryInterface(face, &IID_IDWriteFontFace3, (void **)&f3))) { CHECK(IDWriteFontFace1_HasVerticalGlyphVariants((IDWriteFontFace1 *)f3)); DW_RELEASE(f3); }
    VF_END();
}

/* ---- GetVerticalGlyphVariants -------------------------------------------------------------------------------- */
TEST(variants_substitute_listed_glyphs)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_SUB); UINT16 in[6] = { 2, 3, 4, 0, 1, 64 }, out[6]; int i; UINT16 want[6] = { 64, 65, 4, 0, 1, 64 };
    memset(out, 0xee, sizeof out);
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 6, in, out));
    for (i = 0; i < 6; i++) CHECKF(out[i] == want[i], "glyph %u -> %u want %u", in[i], out[i], want[i]);
    { UINT16 orig[6] = { 2, 3, 4, 0, 1, 64 }; for (i = 0; i < 6; i++) CHECK(in[i] == orig[i]); }          /* input is not modified */
    VF_END();
}
TEST(variants_in_place)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_SUB); UINT16 g[4] = { 2, 3, 4, 2 };
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 4, g, g));
    CHECK(g[0] == 64 && g[1] == 65 && g[2] == 4 && g[3] == 64);
    VF_END();
}
TEST(variants_apply_once_not_transitively)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    /* 2 -> 64 and 64 has no entry: feeding the result in again leaves it alone. */
    VF_BEGIN(VT_SUB); UINT16 g[2] = { 2, 3 };
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 2, g, g));
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 2, g, g));
    CHECK(g[0] == 64 && g[1] == 65);
    VF_END();
}
TEST(variants_zero_count_touches_nothing)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_SUB); UINT16 out[2] = { 0x1234, 0x5678 };
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 0, NULL, NULL));
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 0, NULL, out));
    CHECK(out[0] == 0x1234 && out[1] == 0x5678);
    VF_END();
}
TEST(variants_null_nominal_invalid_and_zeroes_output)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_SUB); UINT16 out[3] = { 0x1111, 0x2222, 0x3333 };
    CHECK_HR(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 3, NULL, out), VT_E_INVALIDARG);
    CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0);
    VF_END();
}
TEST(variants_null_nominal_invalid_without_vert_feature_too)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_PLAIN); UINT16 out[2] = { 7, 8 };
    CHECK_HR(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 2, NULL, out), VT_E_INVALIDARG);
    CHECK(out[0] == 0 && out[1] == 0);
    VF_END();
}
TEST(variants_identity_without_vert_feature)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_FAM); UINT16 in[5] = { 0, 2, 3, 64, 68 }, out[5]; int i;
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 5, in, out));
    for (i = 0; i < 5; i++) CHECK(out[i] == in[i]);
    VF_END();
}
TEST(variants_identity_for_plain_font)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_PLAIN); UINT16 in[3] = { 2, 3, 28 }, out[3] = { 9, 9, 9 };
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 3, in, out));
    CHECK(out[0] == 2 && out[1] == 3 && out[2] == 28);
    VF_END();
}
TEST(variants_unknown_glyph_ids_pass_through)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_SUB); UINT16 in[4] = { 1000, 0x7fff, 0xffff, 69 }, out[4]; int i;
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 4, in, out));
    for (i = 0; i < 4; i++) CHECK(out[i] == in[i]);
    VF_END();
}
TEST(variants_unmapped_glyphs_in_a_vert_font_keep_index)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_SUB); UINT16 in[6] = { 5, 28, 53, 1, 66, 67 }, out[6]; int i;
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 6, in, out));
    for (i = 0; i < 6; i++) CHECK(out[i] == in[i]);
    VF_END();
}
TEST(variants_long_array_all_substituted)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_SUB); UINT16 in[100], out[100]; int i;
    for (i = 0; i < 100; i++) in[i] = (UINT16)(2 + (i & 1));
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 100, in, out));
    for (i = 0; i < 100; i++) CHECK(out[i] == (i & 1 ? 65 : 64));
    VF_END();
}
TEST(variants_do_not_follow_cmap_only_gids)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    /* The map is keyed by glyph index, not by code point: U+4E00 maps to gid 2 in the cmap, and gid 2 is what the substitution replaces. */
    VF_BEGIN(VT_SUB); UINT32 cp[3] = { 0x4e00, 0x4e01, 0x4e02 }; UINT16 gid[3], out[3];
    CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, cp, 3, gid));
    CHECK(gid[0] == 2 && gid[1] == 3 && gid[2] == 4);
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(f1, 3, gid, out));
    CHECK(out[0] == 64 && out[1] == 65 && out[2] == 4);
    VF_END();
}
TEST(variants_survive_factory_release_of_helper_refs)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_SUB); UINT16 g[1] = { 2 };
    IDWriteFontFace1 *copy = f1; IDWriteFontFace1_AddRef(copy);
    DW_RELEASE(f1);
    CHECK_HR_OK(IDWriteFontFace1_GetVerticalGlyphVariants(copy, 1, g, g)); CHECK(g[0] == 64);
    f1 = copy;
    VF_END();
}

/* ---- design metrics -------------------------------------------------------------------------------------------- */
TEST(design_metrics_advance_height_from_vmtx)
{
    VF_BEGIN(VT_FAM); UINT16 g[5] = { 2, 28, 54, 64, TTF_GID_SPACE }; DWRITE_GLYPH_METRICS m[5]; int i; int want[5] = { 1500, 1500, 1500, 1500, VT_VADV_SPACE };
    memset(m, 0xcc, sizeof m);
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 5, m, FALSE));
    for (i = 0; i < 5; i++) CHECKF((int)m[i].advanceHeight == want[i], "glyph %u advanceHeight %u want %d", g[i], m[i].advanceHeight, want[i]);
    VF_END();
}
TEST(design_metrics_notdef_advance_height)
{
    VF_BEGIN(VT_FAM); UINT16 g = 0; DWRITE_GLYPH_METRICS m;
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, &g, 1, &m, FALSE));
    EXPECT_EQ(m.advanceHeight, 1700); EXPECT_EQ(m.advanceWidth, TTF_NOTDEF_ADV);
    VF_END();
}
TEST(design_metrics_advance_width_independent_of_vmtx)
{
    VF_BEGIN(VT_FAM); UINT16 g[3] = { 2, TTF_GID_SPACE, 64 }; DWRITE_GLYPH_METRICS m[3];
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 3, m, FALSE));
    EXPECT_EQ(m[0].advanceWidth, 1000); EXPECT_EQ(m[1].advanceWidth, 1000); EXPECT_EQ(m[2].advanceWidth, 1000);
    VF_END();
}
TEST(design_metrics_top_side_bearing_from_vmtx)
{
    /* vmtx tsb = v_ascent - yMax = 1500 - 1400 for the 800x1400 boxes and the triangle. */
    VF_BEGIN(VT_FAM); UINT16 g[3] = { 2, 28, 64 }; DWRITE_GLYPH_METRICS m[3]; int i;
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 3, m, FALSE));
    for (i = 0; i < 3; i++) EXPECT_EQ(m[i].topSideBearing, 100);
    VF_END();
}
TEST(design_metrics_bottom_side_bearing_closes_the_box)
{
    /* bottomSideBearing = advanceHeight - topSideBearing - (yMax - yMin) by definition of the glyph box. */
    VF_BEGIN(VT_FAM); UINT16 g[3] = { 2, 3, 64 }; DWRITE_GLYPH_METRICS m[3]; int i;
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 3, m, FALSE));
    for (i = 0; i < 3; i++) EXPECT_EQ(m[i].bottomSideBearing, (INT32)m[i].advanceHeight - m[i].topSideBearing - 1400);
    VF_END();
}
TEST(design_metrics_vertical_origin_noted)
{
    VF_BEGIN(VT_FAM); UINT16 g[2] = { 2, TTF_GID_SPACE }; DWRITE_GLYPH_METRICS m[2];
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 2, m, FALSE));
    t_note("verticalOriginY: 'A' %d (v_ascent 1500, tsb %d, yMax 1400), space %d", m[0].verticalOriginY, m[0].topSideBearing, m[1].verticalOriginY);
    VF_END();
}
TEST(design_metrics_sideways_flag_does_not_change_values)
{
    VF_BEGIN(VT_FAM); UINT16 g[4] = { 0, 2, 64, TTF_GID_SPACE }; DWRITE_GLYPH_METRICS a[4], b[4];
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 4, a, FALSE)); CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 4, b, TRUE));
    CHECK(!memcmp(a, b, sizeof a));
    VF_END();
}
TEST(design_metrics_null_indices_invalid_and_zeroed)
{
    VF_BEGIN(VT_FAM); DWRITE_GLYPH_METRICS m[2]; int i;
    memset(m, 0xcc, sizeof m);
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(face, NULL, 2, m, FALSE), VT_E_INVALIDARG);
    for (i = 0; i < 2; i++) { CHECK(m[i].advanceWidth == 0 && m[i].advanceHeight == 0 && m[i].verticalOriginY == 0 && m[i].leftSideBearing == 0); }
    VF_END();
}
TEST(design_metrics_null_indices_with_zero_count_still_invalid)
{
    VF_BEGIN(VT_FAM); DWRITE_GLYPH_METRICS m;
    CHECK_HR(IDWriteFontFace_GetDesignGlyphMetrics(face, NULL, 0, &m, FALSE), VT_E_INVALIDARG);
    VF_END();
}
TEST(design_metrics_zero_count_ok)
{
    VF_BEGIN(VT_FAM); UINT16 g = 2; DWRITE_GLYPH_METRICS m;
    memset(&m, 0xcc, sizeof m);
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, &g, 0, &m, FALSE));
    CHECK(((unsigned char *)&m)[0] == 0xcc);                                          /* nothing written */
    VF_END();
}
TEST(design_metrics_plain_font_vertical_fields_noted)
{
    VF_BEGIN(VT_PLAIN); UINT16 g[3] = { 2, TTF_GID_SPACE, 0 }; DWRITE_GLYPH_METRICS m[3]; int i;
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 3, m, FALSE));
    for (i = 0; i < 3; i++) t_note("DwPlain gid %u: advW %u advH %u tsb %d bsb %d vorgY %d", g[i], m[i].advanceWidth, m[i].advanceHeight, m[i].topSideBearing, m[i].bottomSideBearing, m[i].verticalOriginY);
    EXPECT_EQ(m[0].advanceWidth, 1000); EXPECT_EQ(m[1].advanceWidth, 1000); EXPECT_EQ(m[2].advanceWidth, 1200);
    VF_END();
}
TEST(design_metrics_out_of_range_glyph_noted)
{
    VF_BEGIN(VT_FAM); UINT16 g[2] = { 1000, 0xffff }; DWRITE_GLYPH_METRICS m[2]; HRESULT hr;
    hr = IDWriteFontFace_GetDesignGlyphMetrics(face, g, 2, m, FALSE);
    t_note("out-of-range glyph metrics: %s; advW %u advH %u", dw_hr_name(hr), m[0].advanceWidth, m[0].advanceHeight);
    VF_END();
}

/* ---- design advances ------------------------------------------------------------------------------------------ */
TEST(design_advances_vertical_when_sideways)
{
    VF_BEGIN(VT_FAM); UINT16 g[4] = { 0, 2, TTF_GID_SPACE, 64 }; INT32 v[4], h[4]; int wv[4] = { 1700, 1500, VT_VADV_SPACE, 1500 }, wh[4] = { 1200, 1000, 1000, 1000 }, i;
    CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 4, g, v, TRUE)); CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 4, g, h, FALSE));
    for (i = 0; i < 4; i++) { CHECKF(v[i] == wv[i], "sideways advance of %u: %d want %d", g[i], v[i], wv[i]); CHECKF(h[i] == wh[i], "horizontal advance of %u: %d want %d", g[i], h[i], wh[i]); }
    VF_END();
}
TEST(design_advances_any_nonzero_sideways_is_true)
{
    VF_BEGIN(VT_FAM); UINT16 g = 2; INT32 a = 0, b = 0, c = 0;
    CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 1, &g, &a, 2)); CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 1, &g, &b, -1)); CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 1, &g, &c, 0x10000));
    EXPECT_EQ(a, 1500); EXPECT_EQ(b, 1500); EXPECT_EQ(c, 1500);
    VF_END();
}
TEST(design_advances_match_design_metrics_fields)
{
    VF_BEGIN(VT_FAM); UINT16 g[5] = { 0, 2, 28, 64, TTF_GID_SPACE }; DWRITE_GLYPH_METRICS m[5]; INT32 v[5], h[5]; int i;
    CHECK_HR_OK(IDWriteFontFace_GetDesignGlyphMetrics(face, g, 5, m, FALSE));
    CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 5, g, v, TRUE)); CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 5, g, h, FALSE));
    for (i = 0; i < 5; i++) { EXPECT_EQ(v[i], m[i].advanceHeight); EXPECT_EQ(h[i], m[i].advanceWidth); }
    VF_END();
}
TEST(design_advances_zero_count_null_indices_ok)
{
    VF_BEGIN(VT_FAM); INT32 a[1] = { 77 };
    CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 0, NULL, a, TRUE));
    EXPECT_EQ(a[0], 77);
    VF_END();
}
TEST(design_advances_null_indices_invalid_and_zeroed)
{
    if (t_is_wine()) { t_skip("Wine leaves this IDWriteFontFace1 entry unimplemented (NULL call)"); return; }
    VF_BEGIN(VT_FAM); INT32 a[3] = { 5, 6, 7 };
    CHECK_HR(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 3, NULL, a, TRUE), VT_E_INVALIDARG);
    CHECK(a[0] == 0 && a[1] == 0 && a[2] == 0);
    VF_END();
}
TEST(design_advances_plain_font_sideways_noted)
{
    VF_BEGIN(VT_PLAIN); UINT16 g[3] = { 0, 2, TTF_GID_SPACE }; INT32 v[3]; INT32 h[3]; int i;
    CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 3, g, v, TRUE)); CHECK_HR_OK(IDWriteFontFace1_GetDesignGlyphAdvances(f1, 3, g, h, FALSE));
    for (i = 0; i < 3; i++) t_note("DwPlain gid %u: sideways advance %d horizontal %d", g[i], v[i], h[i]);
    EXPECT_EQ(h[0], 1200); EXPECT_EQ(h[1], 1000); EXPECT_EQ(h[2], 1000);
    VF_END();
}
TEST(vertical_tables_are_present_in_the_raw_font)
{
    VF_BEGIN(VT_FAM); UINT32 len = 0; uint8_t *vh = dw_face_table(face, DW_TAG('v', 'h', 'e', 'a'), &len), *vm; UINT32 mlen = 0;
    CHECK(vh != NULL && len >= 36);
    vm = dw_face_table(face, DW_TAG('v', 'm', 't', 'x'), &mlen);
    CHECK(vm != NULL && mlen == 4u * TTF_DEF_NGLYPHS);
    free(vh); free(vm);
    VF_END();
}
TEST(plain_font_has_no_vertical_tables)
{
    VF_BEGIN(VT_PLAIN); UINT32 len = 0; uint8_t *vh = dw_face_table(face, DW_TAG('v', 'h', 'e', 'a'), &len);
    CHECK(vh == NULL); free(vh);
    VF_END();
}
TEST(gsub_table_present_only_in_sub_font)
{
    VF_BEGIN(VT_SUB); UINT32 len = 0; uint8_t *g = dw_face_table(face, DW_TAG('G', 'S', 'U', 'B'), &len); CHECK(g != NULL); free(g);
    VF_END();
}
