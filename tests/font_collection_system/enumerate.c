/* System font collection: enumerating families and fonts.
 * Windows 10 (probing): GetFontFamilyCount is the length of the family index vector; GetFontFamily(i) builds a NEW
 * IDWriteFontFamily object per call (operator new) that references its owning collection, GetFontCount is the length of the
 * family's font list, IDWriteFont::GetFontFamily returns the family that created the font and
 * IDWriteFontFamily::GetFontCollection returns the owning collection. */
#include "fcs.h"

TEST(enum_family_count_positive) {
    NEED_SYS();
    UINT32 n = fcs_count(c);
    t_note("system collection: %u families", n);
    if (t_is_wine()) CHECK(n > 0); else CHECK(n > 50);
    END_SYS();
}
TEST(enum_family_count_stable) {
    NEED_SYS();
    UINT32 n = fcs_count(c);
    for (int i = 0; i < 100; i++) if (fcs_count(c) != n) { CHECK(0); break; }
    IDWriteFontCollection *c2 = NULL;
    DW_REQUIRE_OK(IDWriteFactory_GetSystemFontCollection(f, &c2, TRUE));
    CHECK(fcs_count(c2) == n);
    DW_RELEASE(c2);
    END_SYS();
}
TEST(enum_all_families_get_ok) {
    NEED_SYS();
    UINT32 seen = 0;
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        seen++;
        CHECK(IDWriteFontFamily_GetFontCount(fam) >= 1);
    } END_FAMILY(fam)
    CHECK(seen == (fcs_count(c) < FCS_MAX_FAMILIES ? fcs_count(c) : FCS_MAX_FAMILIES));
    END_SYS();
}
TEST(enum_family_objects_distinct_per_call) {
    NEED_SYS();
    if (!fcs_count(c)) { t_skip("no families"); END_SYS(); return; }
    IDWriteFontFamily *a = NULL, *b = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_GetFontFamily(c, 0, &a));
    DW_REQUIRE_OK(IDWriteFontCollection_GetFontFamily(c, 0, &b));
    CHECK(a != NULL && b != NULL);
    CHECK(a != b);
    if (a && b) {
        char na[256], nb[256];
        fcs_family_en(a, na, sizeof na); fcs_family_en(b, nb, sizeof nb);
        CHECK(!strcmp(na, nb));
        CHECK(IDWriteFontFamily_GetFontCount(a) == IDWriteFontFamily_GetFontCount(b));
    }
    DW_RELEASE(a); DW_RELEASE(b);
    END_SYS();
}
TEST(enum_family_outparam_cleared_first) {
    NEED_SYS();
    if (!fcs_count(c)) { t_skip("no families"); END_SYS(); return; }
    IDWriteFontFamily *fam = (IDWriteFontFamily *)(ULONG_PTR)1;
    CHECK_HR_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    CHECK(fam != NULL && fam != (void *)1);
    if (fam == (void *)1) fam = NULL;
    DW_RELEASE(fam);
    END_SYS();
}
TEST(enum_family_refcount_semantics) {
    NEED_SYS();
    if (!fcs_count(c)) { t_skip("no families"); END_SYS(); return; }
    ULONG base = dw_refcount((IUnknown *)c);
    IDWriteFontFamily *fam = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    ULONG held = dw_refcount((IUnknown *)c);
    CHECKF(held >= base, "collection refcount fell %lu -> %lu", base, held);
    t_note("collection refcount %lu -> %lu while a family exists", base, held);
    CHECK(dw_refcount((IUnknown *)fam) == 1);
    ULONG a = IDWriteFontFamily_AddRef(fam), r = IDWriteFontFamily_Release(fam);
    CHECK(a == 2 && r == 1);
    IDWriteFontFamily_Release(fam);
    EXPECT_REFS(c, base);
    END_SYS();
}
TEST(enum_family_names_ok) {
    NEED_SYS();
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        IDWriteLocalizedStrings *s = (IDWriteLocalizedStrings *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontFamily_GetFamilyNames(fam, &s);
        CHECKF(hr == S_OK && s != NULL && s != (void *)1, "family %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && s && s != (void *)1) {
            UINT32 cnt = IDWriteLocalizedStrings_GetCount(s);
            CHECKF(cnt >= 1, "family %u has no names", i);
            for (UINT32 k = 0; k < cnt && k < 64; k++) {
                UINT32 len = 0; WCHAR buf[600];
                CHECK_HR_OK(IDWriteLocalizedStrings_GetStringLength(s, k, &len));
                CHECKF(len > 0 && len < 590, "family %u name %u length %u", i, k, len);
                if (len < 590) {
                    CHECK_HR_OK(IDWriteLocalizedStrings_GetString(s, k, buf, len + 1));
                    CHECK(wcslen(buf) == len);
                }
                UINT32 ll = 0; WCHAR loc[100];
                CHECK_HR_OK(IDWriteLocalizedStrings_GetLocaleNameLength(s, k, &ll));
                CHECKF(ll > 0 && ll < 90, "family %u locale %u length %u", i, k, ll);
                if (ll < 90) { CHECK_HR_OK(IDWriteLocalizedStrings_GetLocaleName(s, k, loc, ll + 1)); CHECK(wcslen(loc) == ll); }
            }
            IDWriteLocalizedStrings_Release(s);
        }
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_family_names_en_us_presence) {
    NEED_SYS();
    UINT32 with = 0, without = 0;
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        IDWriteLocalizedStrings *s = NULL;
        if (SUCCEEDED(IDWriteFontFamily_GetFamilyNames(fam, &s)) && s) {
            UINT32 idx = 0; BOOL ex = 0;
            CHECK_HR_OK(IDWriteLocalizedStrings_FindLocaleName(s, L"en-us", &idx, &ex));
            if (ex) { with++; CHECK(idx < IDWriteLocalizedStrings_GetCount(s)); } else { without++; CHECK(idx == 0xFFFFFFFFu || idx == 0); }
            IDWriteLocalizedStrings_Release(s);
        }
    } END_FAMILY(fam)
    t_note("families with an en-us name: %u, without: %u", with, without);
    CHECK(with + without > 0 || t_is_wine());
    END_SYS();
}
TEST(enum_family_names_unique) {
    NEED_SYS();
    UINT32 n = fcs_count(c); if (n > FCS_MAX_FAMILIES) n = FCS_MAX_FAMILIES;
    char (*names)[128] = (char (*)[128])calloc(n ? n : 1, 128);
    if (!names) { t_skip("no memory"); END_SYS(); return; }
    for (UINT32 i = 0; i < n; i++) fcs_name_at(c, i, names[i], 128);
    UINT32 dups = 0;
    for (UINT32 i = 0; i < n; i++) for (UINT32 j = i + 1; j < n; j++)
        if (names[i][0] && fcs_ieq(names[i], names[j])) { dups++; CHECKF(0, "families %u and %u share the name \"%s\"", i, j, names[i]); }
    CHECK(dups == 0);
    free(names);
    END_SYS();
}
TEST(enum_family_order_recorded) {
    NEED_SYS();
    UINT32 n = fcs_count(c); if (n > FCS_MAX_FAMILIES) n = FCS_MAX_FAMILIES;
    char prev[128] = "", cur[128]; UINT32 inversions = 0, first_bad = 0;
    for (UINT32 i = 0; i < n; i++) {
        if (FAILED(fcs_name_at(c, i, cur, sizeof cur))) continue;
        if (i && _stricmp(prev, cur) > 0) { if (!inversions) first_bad = i; inversions++; }
        strcpy(prev, cur);
    }
    t_note("family order: %s (%u inversions in %u families%s)", inversions ? "not sorted by English name" : "sorted by English name", inversions, n,
           inversions ? "" : "");
    if (inversions) { char a[128], b[128]; fcs_name_at(c, first_bad - 1, a, 128); fcs_name_at(c, first_bad, b, 128); t_note("first inversion at %u: \"%s\" then \"%s\"", first_bad, a, b); }
    /* Whatever the order is, it must not change between two walks. */
    UINT32 changed = 0;
    for (UINT32 i = 0; i < n; i++) {
        char a[128], b[128]; fcs_name_at(c, i, a, sizeof a); fcs_name_at(c, i, b, sizeof b);
        if (strcmp(a, b)) changed++;
    }
    CHECK(changed == 0);
    END_SYS();
}
TEST(enum_family_order_same_in_second_collection_object) {
    NEED_SYS();
    IDWriteFactory *iso = dw_isolated(); IDWriteFontCollection *ic = NULL;
    if (!iso || FAILED(IDWriteFactory_GetSystemFontCollection(iso, &ic, FALSE)) || !ic) { t_skip("isolated factory unavailable"); END_SYS(); return; }
    UINT32 n = fcs_count(c);
    CHECK(n == fcs_count(ic));
    UINT32 diff = 0;
    for (UINT32 i = 0; i < n && i < FCS_MAX_FAMILIES && i < fcs_count(ic); i++) {
        char a[128], b[128]; fcs_name_at(c, i, a, sizeof a); fcs_name_at(ic, i, b, sizeof b);
        if (strcmp(a, b)) diff++;
    }
    CHECKF(diff == 0, "%u families differ in position between the shared and isolated collection", diff);
    DW_RELEASE(ic);
    END_SYS();
}
TEST(enum_every_family_has_fonts) {
    NEED_SYS();
    UINT32 total = 0, max = 0, single = 0;
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        CHECKF(nf >= 1, "family %u has no fonts", i);
        total += nf; if (nf > max) max = nf; if (nf == 1) single++;
    } END_FAMILY(fam)
    t_note("fonts over the first families: total %u, largest family %u fonts, %u families with a single font", total, max, single);
    END_SYS();
}
TEST(enum_get_font_all_indices_ok) {
    NEED_SYS();
    UINT32 fonts = 0;
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        for (UINT32 k = 0; k < nf; k++) {
            IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1;
            HRESULT hr = IDWriteFontFamily_GetFont(fam, k, &font);
            CHECKF(hr == S_OK && font && font != (void *)1, "family %u font %u: %s", i, k, dw_hr_name(hr));
            if (hr == S_OK && font != (void *)1) { fonts++; DW_RELEASE(font); }
        }
    } END_FAMILY(fam)
    CHECK(fonts > 0 || t_is_wine());
    END_SYS();
}
TEST(enum_get_font_first_and_last) {
    NEED_SYS();
    FOREACH_FAMILY(c, i, fam, 120) {
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        IDWriteFont *a = NULL, *b = NULL;
        CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, 0, &a));
        CHECK_HR_OK(IDWriteFontFamily_GetFont(fam, nf - 1, &b));
        DW_RELEASE(a); DW_RELEASE(b);
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_font_family_back_pointer) {
    NEED_SYS();
    FOREACH_FAMILY(c, i, fam, 150) {
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        for (UINT32 k = 0; k < nf && k < 8; k++) {
            IDWriteFont *font = NULL; IDWriteFontFamily *back = NULL;
            if (FAILED(IDWriteFontFamily_GetFont(fam, k, &font)) || !font) { CHECK(0); continue; }
            CHECK_HR_OK(IDWriteFont_GetFontFamily(font, &back));
            if (back) {
                CHECKF(fcs_same(back, fam), "family %u font %u: owning family differs (%p vs %p)", i, k, (void *)back, (void *)fam);
                IDWriteFontFamily_Release(back);
            }
            IDWriteFont_Release(font);
        }
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_family_collection_back_pointer) {
    NEED_SYS();
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        IDWriteFontCollection *owner = (IDWriteFontCollection *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontFamily_GetFontCollection(fam, &owner);
        CHECKF(hr == S_OK, "family %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && owner != (void *)1) { CHECKF(owner == c, "family %u owner %p want %p", i, (void *)owner, (void *)c); DW_RELEASE(owner); }
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_font_collection_chain_refcount) {
    /* Family -> collection reference is counted: dropping our collection reference does not invalidate the family. */
    NEED_SYS();
    if (!fcs_count(c)) { t_skip("no families"); END_SYS(); return; }
    IDWriteFontFamily *fam = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    char before[256]; fcs_family_en(fam, before, sizeof before);
    END_SYS();
    c = NULL;
    char after[256]; HRESULT hr = fcs_family_en(fam, after, sizeof after);
    CHECK_HR_OK(hr);
    CHECK(!strcmp(before, after));
    IDWriteFontCollection *owner = NULL;
    CHECK_HR_OK(IDWriteFontFamily_GetFontCollection(fam, &owner));
    if (owner) { CHECK(fcs_count(owner) > 0 || t_is_wine()); DW_RELEASE(owner); }
    IDWriteFontFamily_Release(fam);
}
TEST(enum_font_property_ranges) {
    NEED_SYS();
    UINT32 fonts = 0;
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        for (UINT32 k = 0; k < nf; k++) {
            IDWriteFont *font = NULL;
            if (FAILED(IDWriteFontFamily_GetFont(fam, k, &font)) || !font) continue;
            DWRITE_FONT_WEIGHT w = IDWriteFont_GetWeight(font);
            DWRITE_FONT_STRETCH st = IDWriteFont_GetStretch(font);
            DWRITE_FONT_STYLE sy = IDWriteFont_GetStyle(font);
            DWRITE_FONT_SIMULATIONS sim = IDWriteFont_GetSimulations(font);
            BOOL sym = IDWriteFont_IsSymbolFont(font);
            CHECKF(w >= 1 && w <= 999, "family %u font %u weight %d", i, k, w);
            CHECKF(st >= 1 && st <= 9, "family %u font %u stretch %d", i, k, st);
            CHECKF(sy >= 0 && sy <= 2, "family %u font %u style %d", i, k, sy);
            CHECKF((sim & ~3) == 0, "family %u font %u simulations 0x%x", i, k, sim);
            CHECKF(sym == 0 || sym == 1, "family %u font %u IsSymbolFont %d", i, k, sym);
            fonts++;
            IDWriteFont_Release(font);
        }
    } END_FAMILY(fam)
    CHECK(fonts > 0 || t_is_wine());
    END_SYS();
}
TEST(enum_font_face_names_ok) {
    NEED_SYS();
    FOREACH_FAMILY(c, i, fam, 120) {
        IDWriteFont *font = NULL;
        if (FAILED(IDWriteFontFamily_GetFont(fam, 0, &font)) || !font) { CHECK(0); continue; }
        IDWriteLocalizedStrings *s = NULL;
        CHECK_HR_OK(IDWriteFont_GetFaceNames(font, &s));
        if (s) { if (IDWriteLocalizedStrings_GetCount(s) < 1) t_note("family %u face names empty", i); IDWriteLocalizedStrings_Release(s); }
        BOOL ex = 5; IDWriteLocalizedStrings *fn = NULL;
        CHECK_HR_OK(IDWriteFont_GetInformationalStrings(font, DWRITE_INFORMATIONAL_STRING_FULL_NAME, &fn, &ex));
        CHECK(ex == 0 || ex == 1);
        if (ex) CHECK(fn != NULL); else CHECK(fn == NULL);
        DW_RELEASE(fn);
        IDWriteFont_Release(font);
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_font_family_names_match_owner) {
    NEED_SYS();
    FOREACH_FAMILY(c, i, fam, 120) {
        char owner[256], viaf[256];
        fcs_family_en(fam, owner, sizeof owner);
        IDWriteFont *font = NULL;
        if (FAILED(IDWriteFontFamily_GetFont(fam, 0, &font)) || !font) { CHECK(0); continue; }
        IDWriteFontFamily *back = NULL;
        if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &back)) && back) {
            fcs_family_en(back, viaf, sizeof viaf);
            CHECKF(!strcmp(owner, viaf), "family %u: \"%s\" vs font's family \"%s\"", i, owner, viaf);
            IDWriteFontFamily_Release(back);
        }
        IDWriteFont_Release(font);
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_create_font_face_each) {
    NEED_SYS();
    UINT32 made = 0;
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        for (UINT32 k = 0; k < nf && k < 2 && made < FCS_MAX_FONTS; k++) {
            IDWriteFont *font = NULL; IDWriteFontFace *face = (IDWriteFontFace *)(ULONG_PTR)1;
            if (FAILED(IDWriteFontFamily_GetFont(fam, k, &font)) || !font) continue;
            HRESULT hr = IDWriteFont_CreateFontFace(font, &face);
            CHECKF(hr == S_OK && face && face != (void *)1, "family %u font %u CreateFontFace: %s", i, k, dw_hr_name(hr));
            if (hr == S_OK && face && face != (void *)1) {
                made++;
                CHECKF(IDWriteFontFace_GetSimulations(face) == IDWriteFont_GetSimulations(font), "family %u font %u simulations differ", i, k);
                CHECK(IDWriteFontFace_GetGlyphCount(face) > 0);
                UINT32 nfiles = 0;
                CHECK_HR_OK(IDWriteFontFace_GetFiles(face, &nfiles, NULL));
                CHECK(nfiles >= 1);
                DWRITE_FONT_FACE_TYPE ft = IDWriteFontFace_GetType(face);
                CHECKF(ft != DWRITE_FONT_FACE_TYPE_UNKNOWN && ft <= DWRITE_FONT_FACE_TYPE_OPENTYPE_COLLECTION + 8, "family %u font %u face type %d", i, k, ft);
                IDWriteFontFace_Release(face);
            }
            IDWriteFont_Release(font);
        }
    } END_FAMILY(fam)
    t_note("created %u font faces", made);
    CHECK(made > 0 || t_is_wine());
    END_SYS();
}
TEST(enum_font_files_exist_on_disk) {
    NEED_SYS();
    UINT32 checked = 0, missing = 0, nolocal = 0;
    FOREACH_FAMILY(c, i, fam, 80) {
        IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
        if (FAILED(IDWriteFontFamily_GetFont(fam, 0, &font)) || !font) { CHECK(0); continue; }
        if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
            UINT32 nf = 1; IDWriteFontFile *file = NULL;
            if (SUCCEEDED(IDWriteFontFace_GetFiles(face, &nf, &file)) && file) {
                const void *key = NULL; UINT32 ks = 0; IDWriteFontFileLoader *ld = NULL; IDWriteLocalFontFileLoader *ll = NULL;
                if (SUCCEEDED(IDWriteFontFile_GetReferenceKey(file, &key, &ks)) && SUCCEEDED(IDWriteFontFile_GetLoader(file, &ld)) && ld) {
                    if (SUCCEEDED(IDWriteFontFileLoader_QueryInterface(ld, &IID_IDWriteLocalFontFileLoader, (void **)&ll)) && ll) {
                        UINT32 len = 0; WCHAR path[1024];
                        if (SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathLengthFromKey(ll, key, ks, &len)) && len < 1000 &&
                            SUCCEEDED(IDWriteLocalFontFileLoader_GetFilePathFromKey(ll, key, ks, path, len + 1))) {
                            checked++;
                            if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) { missing++; t_note("font file missing: %s", fcs_u8(path)); }
                        }
                        IDWriteLocalFontFileLoader_Release(ll);
                    } else nolocal++;
                    IDWriteFontFileLoader_Release(ld);
                }
                IDWriteFontFile_Release(file);
            }
            IDWriteFontFace_Release(face);
        }
        IDWriteFont_Release(font);
    } END_FAMILY(fam)
    t_note("%u font files checked, %u missing, %u fonts without a local loader", checked, missing, nolocal);
    CHECK(missing == 0);
    END_SYS();
}
TEST(enum_get_font_from_font_face_round_trip) {
    NEED_SYS();
    UINT32 tried = 0, ok = 0, differs = 0;
    FOREACH_FAMILY(c, i, fam, FCS_MAX_FAMILIES) {
        IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
        if (tried < 150 && SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) && font) {
            if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
                IDWriteFont *got = (IDWriteFont *)(ULONG_PTR)1;
                HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(c, face, &got);
                tried++;
                CHECKF(hr == S_OK && got && got != (void *)1, "family %u GetFontFromFontFace: %s", i, dw_hr_name(hr));
                if (hr == S_OK && got && got != (void *)1) {
                    ok++;
                    if (IDWriteFont_GetWeight(got) != IDWriteFont_GetWeight(font) || IDWriteFont_GetStretch(got) != IDWriteFont_GetStretch(font) ||
                        IDWriteFont_GetStyle(got) != IDWriteFont_GetStyle(font) || IDWriteFont_GetSimulations(got) != IDWriteFont_GetSimulations(font)) differs++;
                    IDWriteFont_Release(got);
                } else if (got == (void *)1) CHECK(0);
                IDWriteFontFace_Release(face);
            }
            IDWriteFont_Release(font);
        }
    } END_FAMILY(fam)
    t_note("GetFontFromFontFace: %u tried, %u succeeded, %u returned a font with other weight/stretch/style/simulations", tried, ok, differs);
    CHECK(tried > 0 || t_is_wine());
    END_SYS();
}
TEST(enum_get_font_from_font_face_family_matches) {
    NEED_SYS();
    UINT32 differs = 0, tried = 0;
    FOREACH_FAMILY(c, i, fam, 100) {
        IDWriteFont *font = NULL; IDWriteFontFace *face = NULL;
        if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, 0, &font)) && font) {
            if (SUCCEEDED(IDWriteFont_CreateFontFace(font, &face)) && face) {
                IDWriteFont *got = NULL;
                if (SUCCEEDED(IDWriteFontCollection_GetFontFromFontFace(c, face, &got)) && got) {
                    IDWriteFontFamily *gf = NULL; char a[256], b[256];
                    if (SUCCEEDED(IDWriteFont_GetFontFamily(got, &gf)) && gf) {
                        fcs_family_en(fam, a, sizeof a); fcs_family_en(gf, b, sizeof b); tried++;
                        if (strcmp(a, b)) { differs++; if (differs < 5) t_note("face of \"%s\" maps back to family \"%s\"", a, b); }
                        IDWriteFontFamily_Release(gf);
                    }
                    IDWriteFont_Release(got);
                }
                IDWriteFontFace_Release(face);
            }
            IDWriteFont_Release(font);
        }
    } END_FAMILY(fam)
    t_note("%u of %u faces map back to a different family", differs, tried);
    END_SYS();
}
TEST(enum_get_font_from_font_face_synthetic_is_nofont) {
    /* A face that is not part of the collection: DWRITE_E_NOFONT and the out pointer cleared. */
    NEED_SYS();
    dwx_font h; memset(&h, 0, sizeof h);
    HRESULT hr0 = dwx_font_open_default(&h, f);
    if (FAILED(hr0) || !h.face) { dwx_font_close(&h); t_skip("synthetic face unavailable: %s", dw_hr_name(hr0)); END_SYS(); return; }
    IDWriteFont *got = (IDWriteFont *)(ULONG_PTR)1;
    CHECK_HR(IDWriteFontCollection_GetFontFromFontFace(c, h.face, &got), FCS_NOFONT);
    CHECK_OUT_NULL(got);
    dwx_font_close(&h);
    END_SYS();
}
TEST(enum_get_font_from_font_face_synthetic_each_time) {
    NEED_SYS();
    dwx_font h; memset(&h, 0, sizeof h);
    if (FAILED(dwx_font_open_default(&h, f)) || !h.face) { dwx_font_close(&h); t_skip("synthetic face unavailable"); END_SYS(); return; }
    for (int i = 0; i < 20; i++) {
        IDWriteFont *got = (IDWriteFont *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(c, h.face, &got);
        CHECKF(hr == FCS_NOFONT && got == NULL, "attempt %d: %s %p", i, dw_hr_name(hr), (void *)got);
    }
    ULONG fr = dw_refcount((IUnknown *)h.face);
    CHECKF(fr >= 1, "face refcount %lu", fr);
    dwx_font_close(&h);
    END_SYS();
}
TEST(enum_get_font_from_font_face_other_factory_face) {
    /* A face from another factory identifies the same physical font by file key; record whether it is accepted. */
    NEED_SYS();
    IDWriteFactory *iso = dw_isolated();
    IDWriteFontFace *face = iso ? dw_sysface(iso, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL) : NULL;
    if (!face) { t_skip("no Arial face from the isolated factory"); END_SYS(); return; }
    IDWriteFont *got = (IDWriteFont *)(ULONG_PTR)1;
    HRESULT hr = IDWriteFontCollection_GetFontFromFontFace(c, face, &got);
    t_note("face from another factory -> %s", dw_hr_name(hr));
    CHECK(hr == S_OK || hr == FCS_NOFONT);
    if (hr == S_OK) { CHECK(got != NULL); DW_RELEASE(got); } else CHECK_OUT_NULL(got);
    IDWriteFontFace_Release(face);
    END_SYS();
}
TEST(enum_get_font_from_font_face_in_typographic_collection) {
    NEED_FV(f6, IDWriteFactory6, 6);
    NEED_SYS();
    IDWriteFontCollection2 *t = NULL;
    DW_REQUIRE_OK(IDWriteFactory6_GetSystemFontCollection(f6, FALSE, DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, &t));
    IDWriteFontFace *face = dw_sysface(f, L"Arial", DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
    if (!face) { t_skip("Arial not installed"); DW_RELEASE(t); END_SYS(); return; }
    IDWriteFont *got = NULL;
    HRESULT hr = IDWriteFontCollection2_GetFontFromFontFace(t, face, &got);
    CHECK_HR_OK(hr);
    DW_RELEASE(got);
    IDWriteFontFace_Release(face);
    DW_RELEASE(t);
    END_SYS();
}
TEST(enum_first_matching_font_matrix) {
    NEED_SYS();
    static const DWRITE_FONT_WEIGHT ws[] = { 100, 200, 300, 350, 400, 500, 600, 700, 800, 900, 950, 1, 999 };
    static const DWRITE_FONT_STRETCH ss[] = { 1, 3, 5, 7, 9 };
    FOREACH_FAMILY(c, i, fam, 12) {
        for (size_t a = 0; a < sizeof ws / sizeof ws[0]; a++) for (size_t b = 0; b < sizeof ss / sizeof ss[0]; b++) for (int st = 0; st < 3; st++) {
            IDWriteFont *font = (IDWriteFont *)(ULONG_PTR)1;
            HRESULT hr = IDWriteFontFamily_GetFirstMatchingFont(fam, ws[a], ss[b], (DWRITE_FONT_STYLE)st, &font);
            CHECKF(hr == S_OK && font && font != (void *)1, "family %u w%d s%d st%d: %s", i, ws[a], ss[b], st, dw_hr_name(hr));
            if (hr == S_OK && font != (void *)1) {
                IDWriteFontFamily *back = NULL;
                if (SUCCEEDED(IDWriteFont_GetFontFamily(font, &back)) && back) { CHECK(fcs_same(back, fam)); IDWriteFontFamily_Release(back); }
                IDWriteFont_Release(font);
            }
        }
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_first_matching_font_exact_match_preferred) {
    /* A font whose weight/stretch/style equals the request exactly is chosen (zero distance). */
    NEED_SYS();
    UINT32 exact_ok = 0, tried = 0;
    FOREACH_FAMILY(c, i, fam, 80) {
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        for (UINT32 k = 0; k < nf && k < 6; k++) {
            IDWriteFont *font = NULL, *m = NULL;
            if (FAILED(IDWriteFontFamily_GetFont(fam, k, &font)) || !font) continue;
            if (IDWriteFont_GetSimulations(font) == 0 &&
                SUCCEEDED(IDWriteFontFamily_GetFirstMatchingFont(fam, IDWriteFont_GetWeight(font), IDWriteFont_GetStretch(font), IDWriteFont_GetStyle(font), &m)) && m) {
                tried++;
                if (IDWriteFont_GetWeight(m) == IDWriteFont_GetWeight(font) && IDWriteFont_GetStretch(m) == IDWriteFont_GetStretch(font) && IDWriteFont_GetStyle(m) == IDWriteFont_GetStyle(font)) exact_ok++;
                IDWriteFont_Release(m);
            }
            IDWriteFont_Release(font);
        }
    } END_FAMILY(fam)
    CHECKF(exact_ok == tried, "%u of %u exact requests returned a font with other properties", tried - exact_ok, tried);
    END_SYS();
}
TEST(enum_family_get_matching_fonts_list) {
    NEED_SYS();
    FOREACH_FAMILY(c, i, fam, 80) {
        IDWriteFontList *list = (IDWriteFontList *)(ULONG_PTR)1;
        HRESULT hr = IDWriteFontFamily_GetMatchingFonts(fam, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC, &list);
        CHECKF(hr == S_OK && list && list != (void *)1, "family %u: %s", i, dw_hr_name(hr));
        if (hr == S_OK && list != (void *)1) {
            UINT32 n = IDWriteFontList_GetFontCount(list);
            CHECKF(n >= 1 && n <= IDWriteFontFamily_GetFontCount(fam), "family %u matching list has %u fonts of %u", i, n, IDWriteFontFamily_GetFontCount(fam));
            IDWriteFont *first = NULL;
            CHECK_HR_OK(IDWriteFontList_GetFont(list, 0, &first));
            DW_RELEASE(first);
            IDWriteFontCollection *lc = NULL;
            if (SUCCEEDED(IDWriteFontList_GetFontCollection(list, &lc)) && lc) { CHECK(lc == c); IDWriteFontCollection_Release(lc); }
            IDWriteFontList_Release(list);
        }
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_family_is_a_font_list) {
    NEED_SYS();
    FOREACH_FAMILY(c, i, fam, 60) {
        IDWriteFontList *list = NULL;
        CHECK_HR_OK(IDWriteFontFamily_QueryInterface(fam, &IID_IDWriteFontList, (void **)&list));
        if (list) {
            CHECK(fcs_same(list, fam));  /* FontList is a secondary base of IDWriteFontFamily: another address, same identity */
            CHECK(IDWriteFontList_GetFontCount(list) == IDWriteFontFamily_GetFontCount(fam));
            IDWriteFontList_Release(list);
        }
    } END_FAMILY(fam)
    END_SYS();
}
TEST(enum_family_qi_matrix) {
    NEED_SYS();
    if (!fcs_count(c)) { t_skip("no families"); END_SYS(); return; }
    IDWriteFontFamily *fam = NULL;
    DW_REQUIRE_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    dw_qi_row rows[] = {
        DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFontFamily, 1), DW_QI_ROW(IDWriteFontList, 1),
        DW_QI_ROW(IDWriteFontFamily1, -1), DW_QI_ROW(IDWriteFontFamily2, -1), DW_QI_ROW(IDWriteFontList1, -1), DW_QI_ROW(IDWriteFontList2, -1),
        DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFont, 0), DW_QI_ROW(IDWriteFontFace, 0), DW_QI_ROW(IDWriteFactory, 0),
        DW_QI_ROW(IDWriteLocalizedStrings, 0), DW_QI_ROW(IDWriteFontSet, 0),
    };
    DW_CHECK_QI(fam, rows);
    IDWriteFontFamily_Release(fam);
    END_SYS();
}
TEST(enum_font_qi_matrix) {
    NEED_SYS();
    IDWriteFontFamily *fam = NULL; IDWriteFont *font = NULL;
    if (!fcs_count(c)) { t_skip("no families"); END_SYS(); return; }
    DW_REQUIRE_OK(IDWriteFontCollection_GetFontFamily(c, 0, &fam));
    DW_REQUIRE_OK(IDWriteFontFamily_GetFont(fam, 0, &font));
    dw_qi_row rows[] = {
        DW_QI_ROW(IUnknown, 1), DW_QI_ROW(IDWriteFont, 1), DW_QI_ROW(IDWriteFont1, -1), DW_QI_ROW(IDWriteFont2, -1), DW_QI_ROW(IDWriteFont3, -1),
        DW_QI_ROW(IDWriteFontFamily, 0), DW_QI_ROW(IDWriteFontList, 0), DW_QI_ROW(IDWriteFontCollection, 0), DW_QI_ROW(IDWriteFontFace, 0),
    };
    DW_CHECK_QI(font, rows);
    IDWriteFont_Release(font); IDWriteFontFamily_Release(fam);
    END_SYS();
}
TEST(enum_bounded_full_walk) {
    NEED_SYS();
    DWORD t0 = GetTickCount();
    UINT32 fams = 0, fonts = 0;
    FOREACH_FAMILY(c, i, fam, 100000) {
        fams++;
        UINT32 nf = IDWriteFontFamily_GetFontCount(fam);
        for (UINT32 k = 0; k < nf && fonts < 100000; k++) {
            IDWriteFont *font = NULL;
            if (SUCCEEDED(IDWriteFontFamily_GetFont(fam, k, &font)) && font) { (void)IDWriteFont_GetWeight(font); IDWriteFont_Release(font); fonts++; }
        }
    } END_FAMILY(fam)
    t_note("walked %u families / %u fonts in %lu ms", fams, fonts, GetTickCount() - t0);
    CHECK(fams == fcs_count(c) || fams == 100000);
    END_SYS();
}
TEST(enum_walk_with_collection_released_midway) {
    /* Families keep what they need; interleaving other Get calls does not disturb them. */
    NEED_SYS();
    UINT32 n = fcs_count(c); if (n > 30) n = 30;
    IDWriteFontFamily *fams[30] = { 0 };
    for (UINT32 i = 0; i < n; i++) IDWriteFontCollection_GetFontFamily(c, i, &fams[i]);
    END_SYS();
    for (UINT32 i = 0; i < n; i++) {
        if (!fams[i]) continue;
        char nm[256];
        CHECK_HR_OK(fcs_family_en(fams[i], nm, sizeof nm));
        CHECK(IDWriteFontFamily_GetFontCount(fams[i]) >= 1);
        IDWriteFontFamily_Release(fams[i]);
    }
}
