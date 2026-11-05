/* The same synthetic font bytes handed to dwrite through IDWriteFactory5::CreateInMemoryFontFileLoader / CreateInMemoryFontFileReference
 * instead of an application loader.  Decomp (MemoryFontFileLoader::CreateInMemoryFontFileReference): *fontFile = NULL first; NULL data or
 * size 0 -> E_INVALIDARG; without an owner object the bytes are copied, with one they are referenced in place and the owner is AddRef'd;
 * each call appends a file entry whose 4-byte reference key is its index, and builds the IDWriteFontFile through
 * IDWriteFactory::CreateCustomFontFileReference with the in-memory loader itself as the loader. */
#include "sf.h"

typedef struct { IDWriteFactory5 *f5; IDWriteInMemoryFontFileLoader *ld; int reg; } im;
static int im_begin(im *m) {
    memset(m, 0, sizeof *m);
    m->f5 = (IDWriteFactory5 *)dw_factory_v(DWRITE_FACTORY_TYPE_ISOLATED, 5);
    if (!m->f5) { t_skip("IDWriteFactory5 unsupported"); return 0; }
    if (FAILED(IDWriteFactory5_CreateInMemoryFontFileLoader(m->f5, &m->ld)) || !m->ld) { t_skip("CreateInMemoryFontFileLoader failed"); return 0; }
    return 1;
}
static void im_end(im *m) {
    if (m->reg) IDWriteFactory5_UnregisterFontFileLoader(m->f5, (IDWriteFontFileLoader *)m->ld);
    DW_RELEASE(m->ld);
}
static int im_register(im *m) {
    HRESULT hr = IDWriteFactory5_RegisterFontFileLoader(m->f5, (IDWriteFontFileLoader *)m->ld);
    if (FAILED(hr)) { t_skip("RegisterFontFileLoader(in-memory loader): %s", dw_hr_name(hr)); return 0; }
    m->reg = 1; return 1;
}

TEST(im_loader_starts_empty) {
    im m; if (!im_begin(&m)) return;
    CHECK(IDWriteInMemoryFontFileLoader_GetFileCount(m.ld) == 0);
    EXPECT_REFS(m.ld, 1);
    im_end(&m);
}
TEST(im_loader_qi_matrix) {
    im m; if (!im_begin(&m)) return;
    { dw_qi_row rows[] = { DW_QI_ROW(IDWriteInMemoryFontFileLoader, 1), DW_QI_ROW(IDWriteFontFileLoader, 1), DW_QI_ROW(IDWriteFactory, 0), DW_QI_ROW(IDWriteFontFile, 0) }; DW_CHECK_QI(m.ld, rows); }
    im_end(&m);
}
TEST(im_two_loaders_are_distinct) {
    im m; IDWriteInMemoryFontFileLoader *b = NULL; if (!im_begin(&m)) return;
    CHECK_HR(IDWriteFactory5_CreateInMemoryFontFileLoader(m.f5, &b), S_OK);
    CHECK(b != NULL && b != m.ld); DW_RELEASE(b); im_end(&m);
}
TEST(im_file_from_copied_bytes) {
    im m; sf_blob font = sf_default(); IDWriteFontFile *file = NULL; BOOL sup = 0; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf = 0; IDWriteFontFace *face = NULL;
    if (!im_begin(&m)) { sf_free(&font); return; }
    if (!im_register(&m)) { im_end(&m); sf_free(&font); return; }
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, font.p, (UINT32)font.n, NULL, &file), S_OK);
    CHECK(file != NULL); CHECK(IDWriteInMemoryFontFileLoader_GetFileCount(m.ld) == 1);
    if (file) {
        CHECK_HR(IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf), S_OK);
        CHECK(sup && ft == DWRITE_FONT_FILE_TYPE_TRUETYPE && fc == DWRITE_FONT_FACE_TYPE_TRUETYPE && nf == 1);
        memset(font.p, 0, font.n);                               /* the loader copied the bytes: wiping ours changes nothing */
        CHECK_HR(IDWriteFactory5_CreateFontFace(m.f5, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, 0, &face), S_OK);
        if (face) { CHECK(sf_gid(face, 'A') == 2); CHECK(IDWriteFontFace_GetGlyphCount(face) == TTF_DEF_NGLYPHS); IDWriteFontFace_Release(face); }
        DW_RELEASE(file);
    }
    im_end(&m); sf_free(&font);
}
TEST(im_reference_key_is_entry_index) {
    im m; sf_blob font = sf_default(); IDWriteFontFile *f[3] = { 0 }; int i; if (!im_begin(&m)) { sf_free(&font); return; }
    if (!im_register(&m)) { im_end(&m); sf_free(&font); return; }
    for (i = 0; i < 3; i++) {
        const void *key = NULL; UINT32 ks = 0; IDWriteFontFileLoader *l = NULL;
        CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, font.p, (UINT32)font.n, NULL, &f[i]), S_OK);
        CHECK(IDWriteInMemoryFontFileLoader_GetFileCount(m.ld) == (UINT32)(i + 1));
        if (f[i]) {
            CHECK_HR(IDWriteFontFile_GetReferenceKey(f[i], &key, &ks), S_OK); CHECK(ks == 4); if (key && ks == 4) { UINT32 k; memcpy(&k, key, 4); CHECKF(k == (UINT32)i, "key %u want %d", (unsigned)k, i); }
            CHECK_HR(IDWriteFontFile_GetLoader(f[i], &l), S_OK); CHECK(l == (IDWriteFontFileLoader *)m.ld); DW_RELEASE(l);
        }
    }
    for (i = 0; i < 3; i++) DW_RELEASE(f[i]);
    CHECK(IDWriteInMemoryFontFileLoader_GetFileCount(m.ld) == 3);       /* entries are never removed */
    im_end(&m); sf_free(&font);
}
TEST(im_null_data_or_zero_size) {
    if (t_is_wine()) { t_skip("Wine dereferences the NULL data pointer"); return; }
    im m; sf_blob font = sf_default(); IDWriteFontFile *file = (IDWriteFontFile *)(ULONG_PTR)1; if (!im_begin(&m)) { sf_free(&font); return; }
    if (!im_register(&m)) { im_end(&m); sf_free(&font); return; }
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, NULL, 100, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    file = (IDWriteFontFile *)(ULONG_PTR)1;
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, font.p, 0, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    file = (IDWriteFontFile *)(ULONG_PTR)1;
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, NULL, 0, NULL, &file), E_INVALIDARG); CHECK_OUT_NULL(file);
    CHECK(IDWriteInMemoryFontFileLoader_GetFileCount(m.ld) == 0);       /* failures leave no entry behind */
    im_end(&m); sf_free(&font);
}
TEST(im_unregistered_loader) {
    im m; sf_blob font = sf_default(); IDWriteFontFile *file = (IDWriteFontFile *)(ULONG_PTR)1; HRESULT hr; if (!im_begin(&m)) { sf_free(&font); return; }
    hr = IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, font.p, (UINT32)font.n, NULL, &file);
    t_note("CreateInMemoryFontFileReference before RegisterFontFileLoader: %s", dw_hr_name(hr));
    if (SUCCEEDED(hr)) { DW_RELEASE(file); } else CHECK_OUT_NULL(file);
    im_end(&m); sf_free(&font);
}
TEST(im_owner_object_is_referenced) {
    im m; sf_blob font = sf_default(); IDWriteFontFile *file = NULL; dwx_effect *own = dwx_effect_new(1); LONG r0;
    if (!im_begin(&m)) { sf_free(&font); dwx_release(own); return; }
    if (!im_register(&m)) { im_end(&m); sf_free(&font); dwx_release(own); return; }
    r0 = dwx_refs(own);
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, font.p, (UINT32)font.n, (IUnknown *)own, &file), S_OK);
    t_note("owner refcount %ld -> %ld", (long)r0, (long)dwx_refs(own));
    CHECKF(dwx_refs(own) > r0, "the owner is AddRef'd");
    DW_RELEASE(file);
    t_note("owner refcount after the file is released: %ld (the loader keeps its entry)", (long)dwx_refs(own));
    CHECK(dwx_refs(own) > r0);
    im_end(&m);
    t_note("owner refcount after the loader is released: %ld", (long)dwx_refs(own));
    PRED("owner released with the loader", dwx_refs(own) == r0);
    dwx_release(own); sf_free(&font);
}
TEST(im_owner_object_data_is_not_copied) {
    im m; sf_blob font = sf_default(); IDWriteFontFile *file = NULL; dwx_effect *own = dwx_effect_new(2); BOOL sup = 1; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf;
    if (!im_begin(&m)) { sf_free(&font); dwx_release(own); return; }
    if (!im_register(&m)) { im_end(&m); sf_free(&font); dwx_release(own); return; }
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, font.p, (UINT32)font.n, (IUnknown *)own, &file), S_OK);
    if (file) {
        memcpy(font.p, "XXXX", 4);                                    /* the loader reads our buffer in place */
        CHECK_HR(IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf), S_OK);
        t_note("after overwriting the sfnt tag in the caller's buffer: supported=%d", (int)sup);
        PRED("in-place reference sees the change", sup == FALSE);
        DW_RELEASE(file);
    }
    im_end(&m); dwx_release(own); sf_free(&font);
}
TEST(im_truncated_data_matches_application_loader) {
    im m; sf_blob font = sf_default(); IDWriteFontFile *file = NULL; BOOL sup = 1; DWRITE_FONT_FILE_TYPE ft; DWRITE_FONT_FACE_TYPE fc; UINT32 nf; HRESULT hr;
    if (!im_begin(&m)) { sf_free(&font); return; }
    if (!im_register(&m)) { im_end(&m); sf_free(&font); return; }
    CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, font.p, 50, NULL, &file), S_OK);
    if (file) { hr = IDWriteFontFile_Analyze(file, &sup, &ft, &fc, &nf); CHECK(hr == S_OK || hr == DWRITE_E_FILEFORMAT_);      /* Windows 10: S_OK, unsupported */ CHECK(!sup); DW_RELEASE(file); }
    im_end(&m); sf_free(&font);
}
TEST(im_many_files_share_one_loader) {
    im m; IDWriteFontFile *f[6] = { 0 }; IDWriteFontFace *fa[6] = { 0 }; int i;
    if (!im_begin(&m)) return;
    if (!im_register(&m)) { im_end(&m); return; }
    for (i = 0; i < 6; i++) {
        ttf_spec s; sf_blob b; char nm[16]; snprintf(nm, sizeof nm, "Mem%d", i); ttf_style_spec(&s, nm, 400, 5, 0); b = sf_build(&s);
        CHECK_HR(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(m.ld, (IDWriteFactory *)m.f5, b.p, (UINT32)b.n, NULL, &f[i]), S_OK);
        sf_free(&b);
        if (f[i]) IDWriteFactory5_CreateFontFace(m.f5, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &f[i], 0, 0, &fa[i]);
    }
    for (i = 0; i < 6; i++) { CHECK(fa[i] != NULL); if (i) CHECK(fa[i] != fa[i - 1]); }
    CHECK(IDWriteInMemoryFontFileLoader_GetFileCount(m.ld) == 6);
    for (i = 0; i < 6; i++) { DW_RELEASE(fa[i]); DW_RELEASE(f[i]); }
    im_end(&m);
}
TEST(im_register_unregister_cycle) {
    im m; if (!im_begin(&m)) return;
    CHECK_HR(IDWriteFactory5_RegisterFontFileLoader(m.f5, (IDWriteFontFileLoader *)m.ld), S_OK);
    CHECK_HR(IDWriteFactory5_RegisterFontFileLoader(m.f5, (IDWriteFontFileLoader *)m.ld), DWRITE_E_ALREADYREGISTERED);
    CHECK_HR(IDWriteFactory5_UnregisterFontFileLoader(m.f5, (IDWriteFontFileLoader *)m.ld), S_OK);
    CHECK_HR(IDWriteFactory5_UnregisterFontFileLoader(m.f5, (IDWriteFontFileLoader *)m.ld), E_INVALIDARG);
    m.reg = 0; im_end(&m);
}
