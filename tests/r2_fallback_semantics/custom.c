#include "r2fs.h"

/* Application-created font collections (CreateCustomFontCollection) as fallback targets:
 *   - AddMapping keeps a reference to the collection (AddFontCollection stores a ComPtr), CreateFontFallback copies the list, so the
 *     collection stays alive as long as a builder or a fallback built from it exists and is released with them;
 *   - the target family is looked up in the mapping's collection, not in the base collection passed to MapCharacters;
 *   - a font returned by MapCharacters keeps its collection and font data alive on its own. */

static ULONG refs(IUnknown *u) { return dw_refcount(u); }

RT(coll_target_collection_differs_from_base_collection)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; r2_args a; r2_res r;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_DUP, X.coll2, NULL, NULL, 1.0f));      /* R2Dup exists in coll2 (U+3042 -> 65) and in coll (U+3042 -> 64) */
    fb = r2_fb(b); r2_defaults(&a, &X);                                              /* base collection = coll */
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        CHECK(r.font != NULL); EXPECT_STR(r.fi.fam, "R2Dup");
        EXPECT_EQ(r2_font_gid(r.font, 0x3042), 65);          /* the coll2 version */
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(coll_same_family_name_selects_by_mapping_collection)
{
    R2_BEGIN(); R2_NEED_B(b1); IDWriteFontFallbackBuilder *b2 = r2_builder(f); IDWriteFontFallback *f1, *f2; r2_args a; r2_res r;
    CHECK_HR_OK(r2_add(b1, 0x3040, 0x30ff, R2_DUP, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(r2_add(b2, 0x3040, 0x30ff, R2_DUP, X.coll2, NULL, NULL, 1.0f));
    f1 = r2_fb(b1); f2 = r2_fb(b2); r2_defaults(&a, &X);
    if (f1 && f2) {
        CHECK_HR_OK(r2_map(f1, L"\x3042", 1, &a, &r, NULL)); EXPECT_EQ(r2_font_gid(r.font, 0x3042), 64); DW_RELEASE(r.font);
        CHECK_HR_OK(r2_map(f2, L"\x3042", 1, &a, &r, NULL)); EXPECT_EQ(r2_font_gid(r.font, 0x3042), 65); DW_RELEASE(r.font);
    }
    DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(b1); DW_RELEASE(b2); R2_END();
}

RT(coll_base_collection_is_where_the_base_family_is_found)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; r2_args a; r2_res r;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_DUP, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    if (fb) {
        r2_defaults(&a, &X); a.coll = X.coll2; a.family = R2_DUP;      /* base family R2Dup from coll2 covers U+3042 itself */
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        EXPECT_EQ(r.ml, 1); EXPECT_EQ(r2_font_gid(r.font, 0x3042), 65); CHECK_FLOAT_EQ(r.scale, 1.0f);
        DW_RELEASE(r.font);
        a.coll = X.coll; a.family = R2_DUP;                            /* the same name in coll covers it with gid 64 */
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        EXPECT_EQ(r2_font_gid(r.font, 0x3042), 64);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(coll_target_absent_from_mapping_collection_maps_nothing)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; r2_args a;
    CHECK_HR_OK(r2_add(b, 0x4e00, 0x4e02, R2_CJK, X.coll2, NULL, NULL, 1.0f));      /* R2Cjk is not in coll2 */
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) R2_WALK_EQ(fb, L"\x4e00", &a, "1:-");
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(coll_two_collections_in_one_fallback)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; r2_args a;
    CHECK_HR_OK(r2_add(b, 0x3042, 0x3042, R2_DUP, X.coll2, NULL, NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x4e00, 0x4e02, R2_CJK, X.coll, NULL, NULL, 1.0f));
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) { R2_WALK_EQ(fb, L"\x3042\x4e00", &a, "1:R2Dup|1:R2Cjk"); R2_WALK_EQ(fb, L"a\x4e01\x3042", &a, "1:R2Main|1:R2Cjk|1:R2Dup"); }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(coll_mapped_font_belongs_to_the_mapping_collection)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; r2_args a; r2_res r;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_DUP, X.coll2, NULL, NULL, 1.0f));
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) {
        CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL));
        if (r.font) {
            IDWriteFontFamily *fam = NULL; IDWriteFontCollection *c = NULL; UINT32 idx; BOOL ex = FALSE;
            CHECK_HR_OK(IDWriteFont_GetFontFamily(r.font, &fam));
            if (fam) CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(fam, &c));
            if (c) {
                CHECK_HR_OK(IDWriteFontCollection_FindFamilyName(c, R2_DUP, &idx, &ex)); CHECK(ex);
                EXPECT_EQ(IDWriteFontCollection_GetFontFamilyCount(c), 1);                 /* coll2 has exactly one family */
                t_note("the mapped font's collection is %s the collection given to AddMapping", c == X.coll2 ? "the same object as" : "another object than");
            }
            DW_RELEASE(c); DW_RELEASE(fam);
        } else CHECK(r.font != NULL);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(coll_refcount_is_retained_by_builder_and_fallback_and_balanced)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; ULONG base = refs((IUnknown *)X.coll2), after_add, after_create;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_DUP, X.coll2, NULL, NULL, 1.0f));
    after_add = refs((IUnknown *)X.coll2);
    CHECKF(after_add > base, "AddMapping must keep a reference to the collection: %lu -> %lu", base, after_add);
    fb = r2_fb(b);
    after_create = refs((IUnknown *)X.coll2);
    CHECKF(after_create >= after_add, "CreateFontFallback lost a reference: %lu -> %lu", after_add, after_create);
    DW_RELEASE(b);
    CHECKF(refs((IUnknown *)X.coll2) >= base + 1, "the fallback must still hold the collection after the builder is gone");
    DW_RELEASE(fb);
    EXPECT_REFS(X.coll2, base);                                                            /* everything released: balanced */
    R2_END();
}

RT(coll_each_add_mapping_holds_one_reference_note)
{
    R2_BEGIN(); R2_NEED_B(b); ULONG r0 = refs((IUnknown *)X.coll2), r1, r2, r3;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x3040, R2_DUP, X.coll2, NULL, NULL, 1.0f)); r1 = refs((IUnknown *)X.coll2);
    CHECK_HR_OK(r2_add(b, 0x3041, 0x3041, R2_DUP, X.coll2, NULL, NULL, 1.0f)); r2 = refs((IUnknown *)X.coll2);
    CHECK_HR_OK(r2_add(b, 0x3042, 0x3042, R2_DUP, X.coll,  NULL, NULL, 1.0f)); r3 = refs((IUnknown *)X.coll2);
    t_note("collection refcount after 0/1/2/3 AddMapping calls (the third uses the other collection): %lu %lu %lu %lu", r0, r1, r2, r3);
    CHECK(r1 > r0); CHECK(r2 >= r1); CHECK(r3 == r2);
    DW_RELEASE(b);
    EXPECT_REFS(X.coll2, r0);
    R2_END();
}

RT(coll_failed_add_mapping_keeps_no_reference)
{
    R2_BEGIN(); R2_NEED_B(b); ULONG r0 = refs((IUnknown *)X.coll2); DWRITE_UNICODE_RANGE rg = { 0x3040, 0x30ff }; const WCHAR *nm[1] = { R2_DUP };
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 1, nm, 1, X.coll2, NULL, NULL, -1.0f), E_INVALIDARG);
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, &rg, 0, nm, 1, X.coll2, NULL, NULL, 1.0f), E_INVALIDARG);
    CHECK_HR(IDWriteFontFallbackBuilder_AddMapping(b, NULL, 1, nm, 1, X.coll2, NULL, NULL, 1.0f), E_INVALIDARG);
    EXPECT_REFS(X.coll2, r0);                                                               /* validation happens before anything is stored */
    DW_RELEASE(b); R2_END();
}

RT(coll_null_collection_adds_no_reference_to_ours)
{
    R2_BEGIN(); R2_NEED_B(b); ULONG r0 = refs((IUnknown *)X.coll2), r1 = refs((IUnknown *)X.coll);
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, L"Arial", NULL, NULL, NULL, 1.0f));
    EXPECT_REFS(X.coll2, r0); EXPECT_REFS(X.coll, r1);
    DW_RELEASE(b); R2_END();
}

RT(coll_mapped_font_survives_release_of_everything_else)
{
    R2_BEGIN(); IDWriteFontCollection *c = NULL; IDWriteFontFallbackBuilder *b = r2_builder(f); IDWriteFontFallback *fb; r2_args a; r2_res r; IDWriteFontFace *face = NULL; UINT32 cp = 0x3042; UINT16 g = 0;
    if (!b) { R2_END(); t_skip("unavailable"); return; }
    CHECK_HR_OK(dwx_coll_create(X.cl2, f, &c));                                             /* a private extra collection object */
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_DUP, c, NULL, NULL, 1.0f));
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) CHECK_HR_OK(r2_map(fb, L"\x3042", 1, &a, &r, NULL)); else memset(&r, 0, sizeof r);
    DW_RELEASE(b); DW_RELEASE(fb); DW_RELEASE(c);
    CHECK(r.font != NULL);
    if (r.font) {
        CHECK_HR_OK(IDWriteFont_CreateFontFace(r.font, &face));
        if (face) { CHECK_HR_OK(IDWriteFontFace_GetGlyphIndices(face, &cp, 1, &g)); EXPECT_EQ(g, 65); DW_RELEASE(face); }
    }
    DW_RELEASE(r.font);
    R2_END();
}

RT(coll_building_a_fallback_reads_no_font_data)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; LONG r0 = dwx_loader_total_reads(X.fl), r1;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f));
    CHECK_HR_OK(r2_add(b, 0x2600, 0x26ff, R2_SYM, X.coll2, NULL, NULL, 1.0f));
    fb = r2_fb(b);
    r1 = dwx_loader_total_reads(X.fl);
    CHECKF(r1 == r0, "AddMapping/CreateFontFallback read font data: %ld -> %ld stream reads", r0, r1);
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(coll_walk_and_face_creation_leave_collections_usable)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; const WCHAR *text = L"ab\x3042\x2603\xD83D\xDE00\x0301" L"c"; char buf[256];
    r2_defaults(&a, &X);
    if (fb) {
        UINT32 pos = 0, n = (UINT32)wcslen(text);
        r2_walk(fb, text, &a, buf, sizeof buf, NULL);
        while (pos < n) {
            r2_res r; IDWriteFontFace *face = NULL;
            a.pos = pos; a.len = n - pos;
            if (FAILED(r2_map(fb, text, n, &a, &r, NULL)) || !r.ml) break;
            if (r.font && SUCCEEDED(IDWriteFont_CreateFontFace(r.font, &face)) && face) { DW_RELEASE(face); }
            DW_RELEASE(r.font); pos += r.ml;
        }
    }
    DW_RELEASE(fb);
    t_note("fragments still held by the live collections: %ld", (long)dwx_loader_total_outstanding(X.fl));
    R2_END();
}

RT(coll_font_file_stream_reads_happen_for_mapped_text_note)
{
    R2_BEGIN(); IDWriteFontFallback *fb = r2_std_fallback(&X); r2_args a; r2_res r; LONG r0, r1;
    r2_defaults(&a, &X);
    if (fb) {
        r0 = dwx_loader_total_reads(X.fl);
        CHECK_HR_OK(r2_map(fb, L"\x2603", 1, &a, &r, NULL));
        r1 = dwx_loader_total_reads(X.fl);
        t_note("stream reads while mapping U+2603 to R2Sym: %ld", r1 - r0);
        DW_RELEASE(r.font);
    }
    DW_RELEASE(fb); R2_END();
}

RT(coll_many_single_code_point_mappings)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *fb; r2_args a; int i; const WCHAR *text = L"\x4e00\x4e01\x4e02";
    /* 40 single-code-point mappings, alternating between two target families that both cover U+4E00..U+4E02 */
    for (i = 0; i < 40; i++) {
        UINT32 cp = 0x4e00 + (UINT32)i;
        CHECK_HR_OK(r2_add(b, cp, cp, (i & 1) ? R2_CJK2 : R2_CJK, X.coll, NULL, NULL, 1.0f));
    }
    fb = r2_fb(b); r2_defaults(&a, &X);
    if (fb) {
        char buf[256]; int segs = r2_walk(fb, text, &a, buf, sizeof buf, NULL);
        CHECK(segs >= 1);
        t_note("three single-code-point mappings alternating R2Cjk / R2Cjk2: %s", buf);
        CHECK(strstr(buf, "-") == NULL);
        r2_walk_invariants(fb, text, &a, __FILE__, __LINE__);
    }
    DW_RELEASE(fb); DW_RELEASE(b); R2_END();
}

RT(coll_builder_reuse_after_create_keeps_accumulating)
{
    R2_BEGIN(); R2_NEED_B(b); IDWriteFontFallback *f1, *f2, *f3; r2_args a;
    CHECK_HR_OK(r2_add(b, 0x3040, 0x30ff, R2_CJK, X.coll, NULL, NULL, 1.0f)); f1 = r2_fb(b);
    CHECK_HR_OK(r2_add(b, 0x2600, 0x26ff, R2_SYM, X.coll, NULL, NULL, 1.0f)); f2 = r2_fb(b);
    CHECK_HR_OK(r2_add(b, 0x1f600, 0x1f64f, R2_EMOJI, X.coll, NULL, NULL, 1.0f)); f3 = r2_fb(b);
    r2_defaults(&a, &X);
    if (f1 && f2 && f3) {
        R2_WALK_EQ(f1, L"\x3042\x2603", &a, "1:R2Cjk|1:-");
        R2_WALK_EQ(f2, L"\x3042\x2603", &a, "1:R2Cjk|1:R2Sym");
        R2_WALK_EQ(f3, L"\x3042\xD83D\xDE00", &a, "1:R2Cjk|2:R2Emoji");
        R2_WALK_EQ(f3, L"\x2603\x3042", &a, "1:R2Sym|1:R2Cjk");
    }
    DW_RELEASE(f1); DW_RELEASE(f2); DW_RELEASE(f3); DW_RELEASE(b); R2_END();
}
