/* The DWRITE_E_* catalogue itself: numeric values, HRESULT structure, neighbouring Win32 mappings and the exception translation
 * table (MapExceptionToHresult plus the exception constructors), and the two DWriteCreateFactory failure paths. */
#include "eh.h"

static const struct { HRESULT hr; unsigned code; const char *name; } dwe[] = {
    { EH_FILEFORMAT, 0x5000, "FILEFORMAT" }, { EH_UNEXPECTED, 0x5001, "UNEXPECTED" }, { EH_NOFONT, 0x5002, "NOFONT" },
    { EH_FILENOTFOUND, 0x5003, "FILENOTFOUND" }, { EH_FILEACCESS, 0x5004, "FILEACCESS" },
    { EH_FONTCOLLECTIONOBSOLETE, 0x5005, "FONTCOLLECTIONOBSOLETE" }, { EH_ALREADYREGISTERED, 0x5006, "ALREADYREGISTERED" },
    { EH_CACHEFORMAT, 0x5007, "CACHEFORMAT" }, { EH_CACHEVERSION, 0x5008, "CACHEVERSION" },
    { EH_UNSUPPORTEDOPERATION, 0x5009, "UNSUPPORTEDOPERATION" }, { EH_TEXTRENDERERINCOMPATIBLE, 0x500A, "TEXTRENDERERINCOMPATIBLE" },
    { EH_FLOWDIRECTIONCONFLICTS, 0x500B, "FLOWDIRECTIONCONFLICTS" }, { EH_NOCOLOR, 0x500C, "NOCOLOR" },
    { EH_REMOTEFONT, 0x500D, "REMOTEFONT" }, { EH_DOWNLOADCANCELLED, 0x500E, "DOWNLOADCANCELLED" },
    { EH_DOWNLOADFAILED, 0x500F, "DOWNLOADFAILED" }, { EH_TOOMANYDOWNLOADS, 0x5010, "TOOMANYDOWNLOADS" },
};
#define NDWE ((int)(sizeof dwe / sizeof dwe[0]))

TEST(codes_dwrite_e_exact_values) {
    /* The constructors in dwrite.dll hard-code these: FileFormatException -0x7767b000, NotSupportedException -0x7767aff7,
     * InvalidCacheDataException -0x7767aff9, RemoteFontException -0x7767aff3; the rest follow the same numbering. */
    EXPECT_EQ((UINT32)DWRITE_E_FILEFORMAT, 0x88985000u);
    int i;
    for (i = 0; i < NDWE; i++) CHECKF((UINT32)dwe[i].hr == (0x88980000u | dwe[i].code), "DWRITE_E_%s = 0x%08lx", dwe[i].name, (unsigned long)dwe[i].hr);
}
TEST(codes_dwrite_e_contiguous_and_unique) {
    int i, j;
    for (i = 0; i < NDWE; i++) {
        EXPECT_EQ((UINT32)dwe[i].hr, 0x88985000u + (UINT32)i);
        for (j = i + 1; j < NDWE; j++) CHECK(dwe[i].hr != dwe[j].hr);
    }
}
TEST(codes_hresult_structure) {
    int i;
    for (i = 0; i < NDWE; i++) {
        HRESULT hr = dwe[i].hr;
        CHECK(FAILED(hr)); CHECK(!SUCCEEDED(hr));
        EXPECT_EQ(HRESULT_SEVERITY(hr), 1);
        EXPECT_EQ(HRESULT_FACILITY(hr), 0x898);        /* FACILITY_DWRITE */
        EXPECT_EQ(HRESULT_CODE(hr), 0x5000 + i);
        CHECK(((UINT32)hr & 0x40000000u) == 0);        /* R bit clear */
        CHECK(((UINT32)hr & 0x10000000u) == 0);        /* N (NTSTATUS-mapped) bit clear */
    }
}
TEST(codes_helper_names) {
    int i;
    for (i = 0; i < NDWE; i++) {
        char want[64]; snprintf(want, sizeof want, "DWRITE_E_%s", dwe[i].name);
        EXPECT_STR(dw_hr_name(dwe[i].hr), want);
    }
    EXPECT_STR(dw_hr_name(E_INVALIDARG), "E_INVALIDARG");
    EXPECT_STR(dw_hr_name(EH_NSB), "E_NOT_SUFFICIENT_BUFFER");
}
TEST(codes_win32_wrapped_codes) {
    /* HRESULT_FROM_WIN32 values that file and buffer paths in dwrite surface instead of (or next to) DWRITE_E_*. */
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), 0x80070002u);
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND), 0x80070003u);
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), 0x80070005u);
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION), 0x80070020u);
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), 0x8007007Au);
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), 0x80070057u);
    EXPECT_EQ((UINT32)E_INVALIDARG, 0x80070057u);
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW), 0x80070216u);    /* IntegerOverflowException */
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY), 0x80070008u);
    EXPECT_EQ((UINT32)E_OUTOFMEMORY, 0x8007000Eu);                                    /* std::bad_alloc */
    EXPECT_EQ((UINT32)HRESULT_FROM_WIN32(ERROR_INVALID_INDEX), 0x80070585u);
    EXPECT_EQ((UINT32)E_FAIL, 0x80004005u);                                           /* IndexOutOfRangeException, unknown exceptions */
    EXPECT_EQ((UINT32)E_UNEXPECTED, 0x8000FFFFu);
}
TEST(codes_catalog_of_translations_is_documentary) {
    /* Documents the translation table the other test files rely on; every row is exercised somewhere in this area. */
    static const struct { const char *cause; HRESULT hr; } rows[] = {
        { "ArgumentException", E_INVALIDARG }, { "ArgumentInsufficientBufferException", EH_NSB }, { "IndexOutOfRangeException", E_FAIL },
        { "FileFormatException", EH_FILEFORMAT }, { "NotSupportedException", EH_UNSUPPORTEDOPERATION },
        { "InvalidCacheDataException", EH_CACHEFORMAT }, { "RemoteFontException", EH_REMOTEFONT },
        { "OSException(ALREADYREGISTERED)", EH_ALREADYREGISTERED }, { "IOException(file missing)", EH_FILENOTFOUND },
        { "IOException(access/sharing)", EH_FILEACCESS },
    };
    int i;
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); i++) CHECKF(FAILED(rows[i].hr), "%s", rows[i].cause);
}

/* ---- DWriteCreateFactory failure paths (function-level: ordinal 1) ------------------------------------------------------- */
TEST(create_factory_export_present) {
    HMODULE m = t_module("dwrite.dll");
    REQUIRE(m);
    FARPROC p = GetProcAddress(m, "DWriteCreateFactory");
    CHECK(p != NULL);
    FARPROC o = GetProcAddress(m, (LPCSTR)(ULONG_PTR)1);
    CHECK(o == p);            /* ordinal 1 is DWriteCreateFactory */
}
TEST(create_factory_bad_type_is_invalidarg) {
    /* DWriteCreateFactory throws ArgumentException("factoryType") for anything but 0 (shared) and 1 (isolated). */
    static const int bad[] = { 2, 3, 7, 100, -1, 0x7fffffff, (int)0x80000000 };
    unsigned i;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        void *p = SENT(void *);
        HRESULT hr = dw_create_factory((DWRITE_FACTORY_TYPE)bad[i], &IID_IDWriteFactory, &p);
        CHECK_HR(hr, E_INVALIDARG); CHECK_OUT_NULL(p);
        if (p && p != SENT(void *)) DW_RELEASE(p);
    }
}
TEST(create_factory_unknown_iid_is_nointerface) {
    /* The factory object is built, then QueryInterface is invoked with the caller's IID: unknown IID -> E_NOINTERFACE, out stays NULL. */
    static const IID rnd = { 0x5a4e7b31, 0x1d2c, 0x4e9f, { 0x90, 0x11, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd } };
    int type;
    for (type = 0; type <= 1; type++) {
        void *p = SENT(void *);
        HRESULT hr = dw_create_factory((DWRITE_FACTORY_TYPE)type, &rnd, &p);
        CHECK_HR(hr, E_NOINTERFACE); CHECK_OUT_NULL(p);
        if (p && p != SENT(void *)) DW_RELEASE(p);
    }
}
TEST(create_factory_non_factory_interface_is_nointerface) {
    /* Interfaces that exist elsewhere in dwrite but are not implemented by the factory object. */
    static const IID *const iids[] = { &IID_IDWriteFont, &IID_IDWriteFontFace, &IID_IDWriteTextFormat, &IID_IDWriteTextLayout, &IID_IDWriteFontCollection,
                                      &IID_IDWriteLocalizedStrings, &IID_IDWriteRenderingParams, &IID_IDWriteTextAnalyzer, &IID_IDWriteGdiInterop };
    unsigned i;
    for (i = 0; i < sizeof iids / sizeof iids[0]; i++) {
        void *p = SENT(void *);
        HRESULT hr = dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, iids[i], &p);
        CHECKF(hr == E_NOINTERFACE && p == NULL, "iid #%u hr=%s p=%p", i, dw_hr_name(hr), p);
        if (p && p != SENT(void *)) DW_RELEASE(p);
    }
}
TEST(create_factory_failure_leaves_no_object) {
    /* After failures the next call still works and the shared factory is unaffected. */
    void *p = SENT(void *);
    CHECK_HR(dw_create_factory((DWRITE_FACTORY_TYPE)5, &IID_IDWriteFactory, &p), E_INVALIDARG);
    p = NULL;
    CHECK_HR(dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &p), S_OK);
    CHECK(p != NULL); DW_RELEASE(p);
}
TEST(create_factory_iunknown_always_works) {
    int type;
    for (type = 0; type <= 1; type++) {
        void *p = NULL;
        CHECK_HR(dw_create_factory((DWRITE_FACTORY_TYPE)type, &IID_IUnknown, &p), S_OK);
        CHECK(p != NULL); DW_RELEASE(p);
    }
}
