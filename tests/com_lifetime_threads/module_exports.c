/* dwrite.dll is not a COM server: its only export is DWriteCreateFactory (ordinal 1). Module load/unload cycles keep the shared
 * factory a process-wide singleton as long as the image stays mapped. */
#include "clt.h"

static HMODULE dw_mod(void) { return t_module("dwrite.dll"); }

static void missing_export(const char *name) {
    FARPROC p; HMODULE m = dw_mod();
    if (!m) { t_skip("dwrite.dll not loadable"); return; }
    SET_ERR(0);
    p = GetProcAddress(m, name);
    CHECKF(p == NULL, "%s unexpectedly exported", name);
    if (!p) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
#define MISSING(id, name) TEST(exp_missing_##id) { missing_export(name); }
MISSING(DllGetClassObject, "DllGetClassObject")
MISSING(DllCanUnloadNow, "DllCanUnloadNow")
MISSING(DllRegisterServer, "DllRegisterServer")
MISSING(DllUnregisterServer, "DllUnregisterServer")
MISSING(DllInstall, "DllInstall")
MISSING(DllMain, "DllMain")
MISSING(DllEntryPoint, "DllEntryPoint")
MISSING(DWriteCreateFactoryW, "DWriteCreateFactoryW")
MISSING(DWriteCreateFactoryA, "DWriteCreateFactoryA")
MISSING(DWriteCreateFactory_stdcall12, "DWriteCreateFactory@12")
MISSING(_DWriteCreateFactory_stdcall12, "_DWriteCreateFactory@12")
MISSING(DWriteCreateFactory_lowercase, "dwritecreatefactory")
MISSING(DWriteCreateFactory_trailing, "DWriteCreateFactory ")
MISSING(DWriteCreateFactory1, "DWriteCreateFactory1")
MISSING(DWriteCreateFactory2, "DWriteCreateFactory2")
MISSING(DWriteGetFontFileLoader, "DWriteGetFontFileLoader")
MISSING(DWriteCoreCreateFactory, "DWriteCoreCreateFactory")
MISSING(GetFontFileLoader, "GetFontFileLoader")
MISSING(CreateTextFormat, "CreateTextFormat")

TEST(exp_named_export_present_once) {
    HMODULE m = dw_mod(); FARPROC a, b;
    if (!m) { t_skip("dwrite.dll"); return; }
    a = GetProcAddress(m, "DWriteCreateFactory"); b = GetProcAddress(m, "DWriteCreateFactory");
    CHECK(a != NULL); CHECK(a == b);
    CHECK((FARPROC)dw_proc() == a);
    CHECK(clt_in_dwrite((void *)a));           /* the code is inside the dwrite image, not a forwarder to another module */
}
TEST(exp_ordinal_one_is_the_named_export) {
    HMODULE m = dw_mod(); FARPROC a, o;
    if (!m) { t_skip("dwrite.dll"); return; }
    a = GetProcAddress(m, "DWriteCreateFactory");
    o = GetProcAddress(m, MAKEINTRESOURCEA(1));
    CHECK(o != NULL); CHECK(o == a);
}
static void missing_ordinal(int ord) {
    HMODULE m = dw_mod(); FARPROC p;
    if (!m) { t_skip("dwrite.dll"); return; }
    SET_ERR(0);
    p = GetProcAddress(m, MAKEINTRESOURCEA(ord));
    CHECKF(p == NULL, "ordinal %d unexpectedly exported", ord);
    if (!p) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(exp_ordinal_2_absent) { missing_ordinal(2); }
TEST(exp_ordinal_3_absent) { missing_ordinal(3); }
TEST(exp_ordinal_16_absent) { missing_ordinal(16); }
TEST(exp_ordinal_100_absent) { missing_ordinal(100); }
TEST(exp_ordinal_65535_absent) { missing_ordinal(65535); }

TEST(exp_export_directory_has_exactly_one_entry) {
    HMODULE m = dw_mod(); const BYTE *base = (const BYTE *)m; const IMAGE_DOS_HEADER *dos; const IMAGE_NT_HEADERS64 *nt; const IMAGE_DATA_DIRECTORY *dd; const IMAGE_EXPORT_DIRECTORY *ed;
    if (!m) { t_skip("dwrite.dll"); return; }
    dos = (const IMAGE_DOS_HEADER *)base;
    CHECK(dos->e_magic == IMAGE_DOS_SIGNATURE);
    nt = (const IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
    CHECK(nt->Signature == IMAGE_NT_SIGNATURE);
    EXPECT_EQ(nt->FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64);
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_DLL);
    dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    CHECK(dd->VirtualAddress != 0);
    if (!dd->VirtualAddress) return;
    ed = (const IMAGE_EXPORT_DIRECTORY *)(base + dd->VirtualAddress);
    EXPECT_EQ(ed->NumberOfFunctions, 1);
    EXPECT_EQ(ed->NumberOfNames, 1);
    EXPECT_EQ(ed->Base, 1);
    if (ed->NumberOfNames == 1) {
        const DWORD *names = (const DWORD *)(base + ed->AddressOfNames);
        EXPECT_STR((const char *)(base + names[0]), "DWriteCreateFactory");
    }
    {   /* the single function is a real code RVA (not a forwarder string inside the export directory) */
        const DWORD *fn = (const DWORD *)(base + ed->AddressOfFunctions);
        CHECK(fn[0] < dd->VirtualAddress || fn[0] >= dd->VirtualAddress + dd->Size);
    }
}
TEST(exp_module_path_is_system32_dwrite) {
    char path[MAX_PATH * 2]; DWORD n; size_t l;
    HMODULE m = dw_mod();
    if (!m) { t_skip("dwrite.dll"); return; }
    n = GetModuleFileNameA(m, path, sizeof path);
    CHECK(n > 0);
    l = strlen(path);
    CHECK(l > 10 && !_stricmp(path + l - 10, "dwrite.dll"));
    t_note("dwrite.dll loaded from %s", path);
}

/* ---- load / free cycles --------------------------------------------------------------------------------------------------- */
TEST(mod_loadlibrary_returns_same_module) {
    HMODULE m = dw_mod(), m2, m3;
    if (!m) { t_skip("dwrite.dll"); return; }
    m2 = LoadLibraryA("dwrite.dll"); m3 = LoadLibraryA("DWRITE.DLL");
    CHECK(m2 == m); CHECK(m3 == m);
    CHECK(GetModuleHandleA("dwrite.dll") == m);
    CHECK(FreeLibrary(m2)); CHECK(FreeLibrary(m3));
    CHECK(GetModuleHandleA("dwrite.dll") == m);            /* our original reference keeps it mapped */
}
TEST(mod_shared_factory_survives_load_free_cycles) {
    IDWriteFactory *f = dw_shared(); HMODULE m = dw_mod(); int i;
    if (!f || !m) { t_skip("no factory"); return; }
    for (i = 0; i < 50; i++) {
        HMODULE x = LoadLibraryA("dwrite.dll"); void *p = NULL; FARPROC cf;
        CHECK(x == m);
        cf = GetProcAddress(x, "DWriteCreateFactory");
        CHECK(cf == (FARPROC)dw_proc());
        CHECK_HR_OK(((dw_create_fn)cf)(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&p));
        if (p != (void *)f) { CHECKF(0, "cycle %d: shared factory changed from %p to %p", i, (void *)f, p); }
        DW_RELEASE(p);
        FreeLibrary(x);
    }
    EXPECT_EQ(dw_shared(), f);
}
TEST(mod_shared_factory_refcount_stable_across_cycles) {
    IDWriteFactory *f = dw_shared(); ULONG b; int i;
    if (!f) { t_skip("no factory"); return; }
    b = dw_refcount((IUnknown *)f);
    for (i = 0; i < 20; i++) {
        HMODULE x = LoadLibraryA("dwrite.dll"); void *p = NULL;
        CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, &p));
        EXPECT_REFS(f, b + 1);
        IUnknown_Release((IUnknown *)p);
        FreeLibrary(x);
    }
    EXPECT_REFS(f, b);
}
TEST(mod_objects_work_after_extra_loadlibrary_free) {
    IDWriteFactory *f = dw_shared(); IDWriteTextFormat *fm = NULL; HMODULE x;
    if (!f) { t_skip("no factory"); return; }
    CHECK_HR_OK(IDWriteFactory_CreateTextFormat(f, L"Arial", NULL, 400, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &fm));
    x = LoadLibraryA("dwrite.dll"); FreeLibrary(x);
    if (fm) { CHECK_FLOAT_EQ(IDWriteTextFormat_GetFontSize(fm), 13.0f); DW_RELEASE(fm); }
}
TEST(mod_isolated_factory_from_every_load_is_new) {
    HMODULE x; void *a = NULL, *b = NULL; int i;
    NEED_PROC();
    for (i = 0; i < 5; i++) {
        x = LoadLibraryA("dwrite.dll");
        a = NULL; b = NULL;
        CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &a));
        CHECK_HR_OK(dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &b));
        CHECK(a != b); CHECK(a != (void *)dw_shared());
        EXPECT_REFS(a, 1);
        DW_RELEASE(a); DW_RELEASE(b);
        FreeLibrary(x);
    }
}
TEST(mod_independent_loadlibraryex_flags_keep_export) {
    HMODULE x;
    NEED_PROC();
    x = LoadLibraryExA("dwrite.dll", NULL, 0);
    CHECK(x == dw_mod());
    if (x) FreeLibrary(x);
    SET_ERR(0);
    x = LoadLibraryA("dwrite_does_not_exist_zz.dll");
    CHECK(x == NULL); EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
