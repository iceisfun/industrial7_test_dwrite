/* Reference counting contract of every creatable object: AddRef/Release return values, QueryInterface adds exactly one
 * reference on the same count and keeps COM identity, unsupported interfaces fail with E_NOINTERFACE and a NULL out
 * pointer, an object survives as long as a second reference is held. Objects are made from an isolated factory. */
#include "clt.h"

static const IID zero_iid = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
static const clt_maker *mk_get(int idx) { int n; const clt_maker *m = clt_makers(&n); return idx < n ? &m[idx] : NULL; }

/* Make the object or skip; the caller releases c and the object. */
#define MAKE_OR_SKIP(idx) \
    clt_ctx c; IUnknown *o; const clt_maker *mk = mk_get(idx); \
    NEED_PROC(); \
    if (!clt_ctx_init(&c)) { t_skip("cannot create isolated factory"); return; } \
    o = mk->make(&c); \
    if (!o) { t_skip("%s: not creatable here (unsupported interface or missing prerequisite)", mk->name); clt_ctx_done(&c); return; }

static void body_refs(int idx) {
    MAKE_OR_SKIP(idx)
    ULONG b = dw_refcount(o), r;
    if (mk->exact1) EXPECT_EQ(b, 1);
    else if (b != 1) t_note("%s: fresh object refcount is %lu", mk->name, b);
    CLT_REFS_ARITH(o);
    if (mk->touch) mk->touch(o);
    r = IUnknown_Release(o);
    CHECKF(r == b - 1, "final Release returned %lu want %lu", r, b - 1);
    if (b == 1) EXPECT_EQ(r, 0);
    clt_ctx_done(&c);
}
static void body_qi(int idx) {
    MAKE_OR_SKIP(idx)
    int nok = CLT_QI_BALANCE(o);
    CHECKF(nok >= 1, "no public interface answered QueryInterface (%d)", nok);
    {
        IUnknown *p1 = NULL, *p2 = NULL; IID bogus = { 0x12345678, 0x9abc, 0xdef0, { 1, 2, 3, 4, 5, 6, 7, 8 } };
        ULONG b = dw_refcount(o);
        void *pp = (void *)(ULONG_PTR)1;
        HRESULT hr = IUnknown_QueryInterface(o, &bogus, &pp);
        CHECK_HR(hr, E_NOINTERFACE); CHECK_OUT_NULL(pp);
        pp = (void *)(ULONG_PTR)1;
        hr = IUnknown_QueryInterface(o, &zero_iid, &pp);
        CHECK_HR(hr, E_NOINTERFACE); CHECK_OUT_NULL(pp);
        EXPECT_REFS(o, b);
        /* IUnknown identity: stable, and the same through any interface that answers */
        CHECK_HR_OK(IUnknown_QueryInterface(o, &IID_IUnknown, (void **)&p1));
        CHECK_HR_OK(IUnknown_QueryInterface(o, &IID_IUnknown, (void **)&p2));
        CHECK(p1 == p2 && p1 != NULL);
        EXPECT_REFS(o, b + 2);
        if (p1) IUnknown_QueryInterface(p1, &IID_IUnknown, (void **)&pp);   /* QI on the identity pointer itself */
        CHECK(pp == (void *)p1);
        if (pp != (void *)(ULONG_PTR)1 && pp) IUnknown_Release((IUnknown *)pp);
        DW_RELEASE(p1); DW_RELEASE(p2);
        EXPECT_REFS(o, b);
    }
    IUnknown_Release(o);
    clt_ctx_done(&c);
}
static void body_second_ref(int idx) {
    MAKE_OR_SKIP(idx)
    IUnknown *o2 = NULL; ULONG b = dw_refcount(o);
    IUnknown_AddRef(o); o2 = o;                      /* a second owner */
    IUnknown_Release(o);                             /* first owner lets go */
    EXPECT_REFS(o2, b);
    if (mk->touch) mk->touch(o2);                    /* still alive and functional */
    CLT_REFS_ARITH(o2);
    IUnknown_Release(o2);
    clt_ctx_done(&c);
}
static void body_identity_two(int idx) {
    /* Two creations are two distinct COM objects unless the OS caches them; either way each Release balances its own AddRef. */
    clt_ctx c; IUnknown *a, *b2; const clt_maker *mk = mk_get(idx);
    NEED_PROC();
    if (!clt_ctx_init(&c)) { t_skip("cannot create isolated factory"); return; }
    a = mk->make(&c); b2 = a ? mk->make(&c) : NULL;
    if (!a || !b2) { t_skip("%s: not creatable twice", mk->name); DW_RELEASE(a); DW_RELEASE(b2); clt_ctx_done(&c); return; }
    {
        ULONG ra = dw_refcount(a), rb = dw_refcount(b2);
        if (dw_same_identity(a, b2)) {
            t_note("%s: second creation returns the same object (cached/shared)", mk->name);
            CHECKF(dw_refcount(a) == ra, "shared object count unstable");
        } else {
            IUnknown_AddRef(a);
            EXPECT_REFS(b2, rb);                     /* distinct objects do not share a count */
            IUnknown_Release(a);
            EXPECT_REFS(a, ra);
        }
    }
    IUnknown_Release(a); IUnknown_Release(b2);
    clt_ctx_done(&c);
}
TEST(refs_textformat) { body_refs(0); }
TEST(qi_textformat) { body_qi(0); }
TEST(second_ref_textformat) { body_second_ref(0); }
TEST(two_objects_textformat) { body_identity_two(0); }
TEST(refs_textlayout) { body_refs(1); }
TEST(qi_textlayout) { body_qi(1); }
TEST(second_ref_textlayout) { body_second_ref(1); }
TEST(two_objects_textlayout) { body_identity_two(1); }
TEST(refs_textformat_view_of_layout) { body_refs(2); }
TEST(qi_textformat_view_of_layout) { body_qi(2); }
TEST(second_ref_textformat_view_of_layout) { body_second_ref(2); }
TEST(two_objects_textformat_view_of_layout) { body_identity_two(2); }
TEST(refs_typography) { body_refs(3); }
TEST(qi_typography) { body_qi(3); }
TEST(second_ref_typography) { body_second_ref(3); }
TEST(two_objects_typography) { body_identity_two(3); }
TEST(refs_numbersubstitution) { body_refs(4); }
TEST(qi_numbersubstitution) { body_qi(4); }
TEST(second_ref_numbersubstitution) { body_second_ref(4); }
TEST(two_objects_numbersubstitution) { body_identity_two(4); }
TEST(refs_renderingparams) { body_refs(5); }
TEST(qi_renderingparams) { body_qi(5); }
TEST(second_ref_renderingparams) { body_second_ref(5); }
TEST(two_objects_renderingparams) { body_identity_two(5); }
TEST(refs_customrenderingparams) { body_refs(6); }
TEST(qi_customrenderingparams) { body_qi(6); }
TEST(second_ref_customrenderingparams) { body_second_ref(6); }
TEST(two_objects_customrenderingparams) { body_identity_two(6); }
TEST(refs_textanalyzer) { body_refs(7); }
TEST(qi_textanalyzer) { body_qi(7); }
TEST(second_ref_textanalyzer) { body_second_ref(7); }
TEST(two_objects_textanalyzer) { body_identity_two(7); }
TEST(refs_gdiinterop) { body_refs(8); }
TEST(qi_gdiinterop) { body_qi(8); }
TEST(second_ref_gdiinterop) { body_second_ref(8); }
TEST(two_objects_gdiinterop) { body_identity_two(8); }
TEST(refs_ellipsissign) { body_refs(9); }
TEST(qi_ellipsissign) { body_qi(9); }
TEST(second_ref_ellipsissign) { body_second_ref(9); }
TEST(two_objects_ellipsissign) { body_identity_two(9); }
TEST(refs_fontfile) { body_refs(10); }
TEST(qi_fontfile) { body_qi(10); }
TEST(second_ref_fontfile) { body_second_ref(10); }
TEST(two_objects_fontfile) { body_identity_two(10); }
TEST(refs_fontface) { body_refs(11); }
TEST(qi_fontface) { body_qi(11); }
TEST(second_ref_fontface) { body_second_ref(11); }
TEST(two_objects_fontface) { body_identity_two(11); }
TEST(refs_customcollection) { body_refs(12); }
TEST(qi_customcollection) { body_qi(12); }
TEST(second_ref_customcollection) { body_second_ref(12); }
TEST(two_objects_customcollection) { body_identity_two(12); }
TEST(refs_systemcollection) { body_refs(13); }
TEST(qi_systemcollection) { body_qi(13); }
TEST(second_ref_systemcollection) { body_second_ref(13); }
TEST(two_objects_systemcollection) { body_identity_two(13); }
TEST(refs_fontfamily) { body_refs(14); }
TEST(qi_fontfamily) { body_qi(14); }
TEST(second_ref_fontfamily) { body_second_ref(14); }
TEST(two_objects_fontfamily) { body_identity_two(14); }
TEST(refs_font) { body_refs(15); }
TEST(qi_font) { body_qi(15); }
TEST(second_ref_font) { body_second_ref(15); }
TEST(two_objects_font) { body_identity_two(15); }
TEST(refs_familynames) { body_refs(16); }
TEST(qi_familynames) { body_qi(16); }
TEST(second_ref_familynames) { body_second_ref(16); }
TEST(two_objects_familynames) { body_identity_two(16); }
TEST(refs_facenames) { body_refs(17); }
TEST(qi_facenames) { body_qi(17); }
TEST(second_ref_facenames) { body_second_ref(17); }
TEST(two_objects_facenames) { body_identity_two(17); }
TEST(refs_fontface_from_font) { body_refs(18); }
TEST(qi_fontface_from_font) { body_qi(18); }
TEST(second_ref_fontface_from_font) { body_second_ref(18); }
TEST(two_objects_fontface_from_font) { body_identity_two(18); }
TEST(refs_bitmaprendertarget) { body_refs(19); }
TEST(qi_bitmaprendertarget) { body_qi(19); }
TEST(second_ref_bitmaprendertarget) { body_second_ref(19); }
TEST(two_objects_bitmaprendertarget) { body_identity_two(19); }
TEST(refs_glyphrunanalysis) { body_refs(20); }
TEST(qi_glyphrunanalysis) { body_qi(20); }
TEST(second_ref_glyphrunanalysis) { body_second_ref(20); }
TEST(two_objects_glyphrunanalysis) { body_identity_two(20); }
TEST(refs_isolatedfactory) { body_refs(21); }
TEST(qi_isolatedfactory) { body_qi(21); }
TEST(second_ref_isolatedfactory) { body_second_ref(21); }
TEST(two_objects_isolatedfactory) { body_identity_two(21); }
TEST(refs_sharedfactory) { body_refs(22); }
TEST(qi_sharedfactory) { body_qi(22); }
TEST(second_ref_sharedfactory) { body_second_ref(22); }
TEST(two_objects_sharedfactory) { body_identity_two(22); }
TEST(refs_fontsetbuilder) { body_refs(23); }
TEST(qi_fontsetbuilder) { body_qi(23); }
TEST(second_ref_fontsetbuilder) { body_second_ref(23); }
TEST(two_objects_fontsetbuilder) { body_identity_two(23); }
TEST(refs_systemfontset) { body_refs(24); }
TEST(qi_systemfontset) { body_qi(24); }
TEST(second_ref_systemfontset) { body_second_ref(24); }
TEST(two_objects_systemfontset) { body_identity_two(24); }
TEST(refs_fontfacereference) { body_refs(25); }
TEST(qi_fontfacereference) { body_qi(25); }
TEST(second_ref_fontfacereference) { body_second_ref(25); }
TEST(two_objects_fontfacereference) { body_identity_two(25); }
TEST(refs_inmemoryloader) { body_refs(26); }
TEST(qi_inmemoryloader) { body_qi(26); }
TEST(second_ref_inmemoryloader) { body_second_ref(26); }
TEST(two_objects_inmemoryloader) { body_identity_two(26); }
TEST(refs_downloadqueue) { body_refs(27); }
TEST(qi_downloadqueue) { body_qi(27); }
TEST(second_ref_downloadqueue) { body_second_ref(27); }
TEST(two_objects_downloadqueue) { body_identity_two(27); }
TEST(refs_collection_from_fontset) { body_refs(28); }
TEST(qi_collection_from_fontset) { body_qi(28); }
TEST(second_ref_collection_from_fontset) { body_second_ref(28); }
TEST(two_objects_collection_from_fontset) { body_identity_two(28); }
TEST(refs_systemcollection1) { body_refs(29); }
TEST(qi_systemcollection1) { body_qi(29); }
TEST(second_ref_systemcollection1) { body_second_ref(29); }
TEST(two_objects_systemcollection1) { body_identity_two(29); }
