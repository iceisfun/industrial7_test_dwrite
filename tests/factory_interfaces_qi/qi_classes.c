/* Table-driven QueryInterface / refcount / vtable checks, instantiated for every class in FQ_CLASSES.
 * The expectations per class come from the QueryInterface implementations of build 19041 (dwrite.dll):
 *   - every ComObject<> answers exactly the interfaces of its type list plus IUnknown; others give E_NOINTERFACE and *ppv = NULL
 *   - DWriteFontFace::QueryInterface answers E_NOTIMPL (0x80004001) for an unsupported IID, still with *ppv = NULL.
 */
#include "fq.h"

#define X(n) TEST(qi_matrix_##n) { fq_matrix(&fq_cls_##n); }
FQ_CLASSES(X)
#undef X

#define X(n) TEST(qi_from_each_##n) { fq_matrix_from_each(&fq_cls_##n); }
FQ_CLASSES(X)
#undef X

#define X(n) TEST(qi_identity_##n) { fq_identity(&fq_cls_##n); }
FQ_CLASSES(X)
#undef X

#define X(n) TEST(refcount_##n) { fq_refcount(&fq_cls_##n); }
FQ_CLASSES(X)
#undef X

#define X(n) TEST(vtable_##n) { fq_vtables(&fq_cls_##n); }
FQ_CLASSES(X)
#undef X

#define X(n) TEST(creation_##n) { fq_fresh(&fq_cls_##n); }
FQ_CLASSES(X)
#undef X
