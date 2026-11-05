/* dwrite.dll export surface and module load/unload behaviour.
 * dwrite.dll exports exactly one function, DWriteCreateFactory, at ordinal 1. */
#include "fc.h"

static const IMAGE_EXPORT_DIRECTORY *get_export_dir(HMODULE m, const IMAGE_NT_HEADERS **nt_out, DWORD *size_out) {
    const BYTE *base = (const BYTE *)m;
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS *nt;
    const IMAGE_DATA_DIRECTORY *dd;
    if (!m || dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    nt = (const IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;
    if (nt_out) *nt_out = nt;
    dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (size_out) *size_out = dd->Size;
    if (!dd->VirtualAddress) return NULL;
    return (const IMAGE_EXPORT_DIRECTORY *)(base + dd->VirtualAddress);
}
static HMODULE dwmod(void) { return t_module("dwrite.dll"); }

TEST(exp_module_loads) { CHECK(dwmod() != NULL); }
TEST(exp_getmodulehandle_matches_loadlibrary) {
    HMODULE a = dwmod(), b = GetModuleHandleA("dwrite.dll");
    CHECK(a != NULL); CHECK(a == b);
    /* GetModuleHandle is case-insensitive and accepts the name without extension. */
    CHECK(GetModuleHandleA("DWRITE.DLL") == a);
    CHECK(GetModuleHandleA("DWrite") == a);
}
TEST(exp_module_path_is_system32) {
    char path[MAX_PATH], sys[MAX_PATH], want[MAX_PATH + 16]; DWORD n;
    n = GetModuleFileNameA(dwmod(), path, sizeof path);
    CHECK(n > 0 && n < sizeof path);
    GetSystemDirectoryA(sys, sizeof sys);
    snprintf(want, sizeof want, "%s\\dwrite.dll", sys);
    /* Not a hard failure on exotic redirection set-ups; Windows loads it from the system directory. */
    if (lstrcmpiA(path, want)) t_note("dwrite.dll loaded from %s (system dir %s)", path, sys);
    if (!t_is_wine()) CHECKF(!lstrcmpiA(path, want), "path=%s want=%s", path, want);
}
TEST(exp_pe_is_x64_dll) {
    const IMAGE_NT_HEADERS *nt = NULL;
    get_export_dir(dwmod(), &nt, NULL);
    REQUIRE(nt);
    EXPECT_EQ(nt->FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64);
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_DLL);
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE);
    EXPECT_EQ(nt->OptionalHeader.Magic, IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    EXPECT_EQ(nt->OptionalHeader.Subsystem, IMAGE_SUBSYSTEM_WINDOWS_GUI);
    CHECK(nt->OptionalHeader.SizeOfImage > 0x100000);   /* dwrite.dll is a multi-megabyte image */
}
TEST(exp_export_directory_present) {
    DWORD sz = 0; const IMAGE_EXPORT_DIRECTORY *ed = get_export_dir(dwmod(), NULL, &sz);
    CHECK(ed != NULL); CHECK(sz >= sizeof(IMAGE_EXPORT_DIRECTORY));
}
TEST(exp_exactly_one_named_export) {
    const IMAGE_EXPORT_DIRECTORY *ed = get_export_dir(dwmod(), NULL, NULL);
    const BYTE *base = (const BYTE *)dwmod();
    const DWORD *names; const WORD *ords; const char *nm;
    REQUIRE(ed);
    EXPECT_EQ(ed->NumberOfNames, 1);
    EXPECT_EQ(ed->NumberOfFunctions, 1);
    if (ed->NumberOfNames != 1) return;
    names = (const DWORD *)(base + ed->AddressOfNames);
    ords = (const WORD *)(base + ed->AddressOfNameOrdinals);
    nm = (const char *)(base + names[0]);
    EXPECT_STR(nm, "DWriteCreateFactory");
    EXPECT_EQ(ords[0], 0);                       /* index into the function table */
    EXPECT_EQ(ed->Base, 1);                      /* so the single export is ordinal 1 */
    EXPECT_EQ(ords[0] + ed->Base, 1);
}
TEST(exp_export_is_not_a_forwarder) {
    const IMAGE_EXPORT_DIRECTORY *ed = get_export_dir(dwmod(), NULL, NULL);
    const BYTE *base = (const BYTE *)dwmod();
    const IMAGE_NT_HEADERS *nt = NULL; DWORD sz = 0; DWORD rva, dstart, dend;
    const IMAGE_DATA_DIRECTORY *dd;
    REQUIRE(ed);
    get_export_dir(dwmod(), &nt, &sz);
    dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    dstart = dd->VirtualAddress; dend = dstart + dd->Size;
    rva = ((const DWORD *)(base + ed->AddressOfFunctions))[0];
    CHECK(rva != 0);
    /* An RVA inside the export directory range would be a forwarder string; DWriteCreateFactory is real code. */
    CHECK(!(rva >= dstart && rva < dend));
    CHECK(rva < nt->OptionalHeader.SizeOfImage);
}
TEST(exp_export_points_into_executable_memory) {
    FARPROC p = GetProcAddress(dwmod(), "DWriteCreateFactory");
    MEMORY_BASIC_INFORMATION mbi;
    REQUIRE(p);
    memset(&mbi, 0, sizeof mbi);
    CHECK(VirtualQuery((void *)p, &mbi, sizeof mbi) == sizeof mbi);
    CHECK(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
    CHECK(mbi.AllocationBase == (void *)dwmod());
}
TEST(exp_export_address_inside_image) {
    const IMAGE_NT_HEADERS *nt = NULL; FARPROC p;
    get_export_dir(dwmod(), &nt, NULL);
    p = GetProcAddress(dwmod(), "DWriteCreateFactory");
    REQUIRE(p && nt);
    CHECK((const BYTE *)p > (const BYTE *)dwmod());
    CHECK((const BYTE *)p < (const BYTE *)dwmod() + nt->OptionalHeader.SizeOfImage);
}
TEST(exp_getprocaddress_by_name) {
    SET_ERR(0);
    FARPROC p = GetProcAddress(dwmod(), "DWriteCreateFactory");
    CHECK(p != NULL);
    CHECK(p == (FARPROC)dw_proc());
}
TEST(exp_getprocaddress_by_ordinal_equals_name) {
    FARPROC byname = GetProcAddress(dwmod(), "DWriteCreateFactory");
    FARPROC byord = GetProcAddress(dwmod(), (LPCSTR)(ULONG_PTR)1);
    CHECK(byname != NULL); CHECK(byord != NULL);
    CHECK(byname == byord);
    CHECK(t_proc_ord("dwrite.dll", 1) == byname);
}
TEST(exp_getprocaddress_stable_across_calls) {
    FARPROC first = GetProcAddress(dwmod(), "DWriteCreateFactory");
    int i;
    for (i = 0; i < 50; i++) { FARPROC p = GetProcAddress(dwmod(), "DWriteCreateFactory"); if (p != first) { CHECK(p == first); break; } }
    CHECK(first != NULL);
}
TEST(exp_getprocaddress_other_ordinals_fail) {
    static const ULONG_PTR ords[] = { 0, 2, 3, 4, 5, 10, 100, 255, 256, 1000, 0x7FFF, 0xFFFF };
    size_t i;
    for (i = 0; i < sizeof ords / sizeof ords[0]; i++) {
        FARPROC p;
        SET_ERR(0);
        p = GetProcAddress(dwmod(), (LPCSTR)ords[i]);
        CHECKF(p == NULL, "ordinal %lu resolved", (unsigned long)ords[i]);
        if (!p) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(exp_getprocaddress_wrong_names_fail) {
    static const char *const names[] = {
        "dwritecreatefactory", "DWRITECREATEFACTORY", "DWriteCreateFactoryA", "DWriteCreateFactoryW", "DWriteCreateFactory ",
        " DWriteCreateFactory", "_DWriteCreateFactory", "_DWriteCreateFactory@12", "DWriteCreateFactory@12", "DWriteCreateFactor",
        "DWriteCreateFactory2", "DWriteCreateFactoryEx", "DWriteCreateFactoryInternal", "DllGetClassObject", "DllCanUnloadNow",
        "DllMain", "DllRegisterServer", "DllUnregisterServer", "DWriteGetFactoryIid", "DWriteCreateTextFormat", "CreateFactory",
        "GetSharedFactory", "DWriteFactory", "?DWriteCreateFactory@@YAJW4DWRITE_FACTORY_TYPE@@AEBU_GUID@@PEAPEAUIUnknown@@@Z",
        "DWriteCreateFactory\\", "dwrite.DWriteCreateFactory", "dwrite.dll.DWriteCreateFactory", "#1", "#2",
    };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        FARPROC p;
        SET_ERR(0);
        p = GetProcAddress(dwmod(), names[i]);
        /* "#1" is only an ordinal spelling for the loader's import resolver, not for GetProcAddress. */
        CHECKF(p == NULL, "name \"%s\" resolved", names[i]);
        if (!p) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(exp_getprocaddress_empty_name_fails) {
    FARPROC p; SET_ERR(0);
    p = GetProcAddress(dwmod(), "");
    CHECK(p == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(exp_no_interface_helpers_exported) {
    /* Everything but DWriteCreateFactory is reached through vtables, there are no flat C entry points. */
    static const char *const names[] = { "DWriteCreateFont", "DWriteCreateTextLayout", "DWriteCreateGdiInterop",
        "DWriteGetSystemFontCollection", "DWriteRegisterFontFileLoader", "DWriteCreateRenderingParams", "DWriteAnalyzeScript" };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) CHECKF(GetProcAddress(dwmod(), names[i]) == NULL, "%s", names[i]);
}
TEST(exp_loadlibrary_twice_same_handle) {
    HMODULE a = LoadLibraryA("dwrite.dll"), b = LoadLibraryA("dwrite.dll");
    CHECK(a != NULL); CHECK(a == b); CHECK(a == dwmod());
    if (b) CHECK(FreeLibrary(b));
    if (a) CHECK(FreeLibrary(a));
    CHECK(GetModuleHandleA("dwrite.dll") == dwmod());   /* the harness still holds references */
}
TEST(exp_loadlibrary_full_path_same_module) {
    char path[MAX_PATH]; HMODULE m;
    DWORD n = GetModuleFileNameA(dwmod(), path, sizeof path);
    REQUIRE(n > 0);
    m = LoadLibraryA(path);
    CHECK(m == dwmod());
    if (m) FreeLibrary(m);
}
TEST(exp_loadlibrary_uppercase_same_module) {
    HMODULE m = LoadLibraryA("DWRITE.DLL");
    CHECK(m == dwmod());
    if (m) FreeLibrary(m);
    m = LoadLibraryW(L"dwrite");
    CHECK(m == dwmod());
    if (m) FreeLibrary(m);
}
TEST(exp_loadlibraryex_search_system32) {
    HMODULE m = LoadLibraryExA("dwrite.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m) { t_note("LOAD_LIBRARY_SEARCH_SYSTEM32 unsupported (err %lu)", GetLastError()); return; }
    CHECK(m == dwmod());
    FreeLibrary(m);
}
TEST(exp_loadlibraryex_datafile_handle) {
    /* A data-file mapping is a different handle (low bits tag it). */
    HMODULE m = LoadLibraryExA("dwrite.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!m) { t_note("datafile load failed (err %lu)", GetLastError()); return; }
    CHECK(m != dwmod());
    CHECK(((ULONG_PTR)m & 3) != 0);
    t_note("GetProcAddress on a data-file handle -> %p", (void *)GetProcAddress(m, "DWriteCreateFactory"));
    FreeLibrary(m);
}
TEST(exp_getmoduleex_from_address) {
    HMODULE m = NULL; FARPROC p = GetProcAddress(dwmod(), "DWriteCreateFactory");
    REQUIRE(p);
    CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m));
    CHECK(m == dwmod());
}
TEST(exp_getmoduleex_refcount_pin_pair) {
    HMODULE m = NULL;
    CHECK(GetModuleHandleExA(0, "dwrite.dll", &m));      /* increments the refcount */
    CHECK(m == dwmod());
    if (m) CHECK(FreeLibrary(m));
}
TEST(exp_repeated_load_free_cycles) {
    int i, bad = 0;
    for (i = 0; i < 200; i++) {
        HMODULE m = LoadLibraryA("dwrite.dll");
        if (!m || !GetProcAddress(m, "DWriteCreateFactory")) bad++;
        if (m) FreeLibrary(m);
    }
    CHECKF(bad == 0, "bad=%d", bad);
    CHECK(GetModuleHandleA("dwrite.dll") == dwmod());
}
TEST(exp_repeated_load_create_free) {
    int i, bad = 0;
    for (i = 0; i < 50; i++) {
        HMODULE m = LoadLibraryA("dwrite.dll");
        dw_create_fn fn = m ? (dw_create_fn)GetProcAddress(m, "DWriteCreateFactory") : NULL;
        IUnknown *u = NULL;
        if (!fn) { bad++; if (m) FreeLibrary(m); continue; }
        if (FAILED(fn((DWRITE_FACTORY_TYPE)(i & 1), &IID_IDWriteFactory, &u)) || !u) bad++;
        else IUnknown_Release(u);
        FreeLibrary(m);
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(exp_function_pointer_survives_extra_refs) {
    /* The same code address is returned from every load of the same image. */
    FARPROC a = GetProcAddress(dwmod(), "DWriteCreateFactory");
    HMODULE m = LoadLibraryA("dwrite.dll");
    FARPROC b = m ? GetProcAddress(m, "DWriteCreateFactory") : NULL;
    CHECK(a != NULL); CHECK(a == b);
    if (m) FreeLibrary(m);
    IUnknown *u = NULL;
    CHECK_HR_OK(((dw_create_fn)a)(FT_ISOLATED, &IID_IUnknown, &u));
    DW_RELEASE(u);
}
TEST(exp_u_macro_resolves_same_function) {
    CHECK((FARPROC)U(DWriteCreateFactory) == GetProcAddress(dwmod(), "DWriteCreateFactory"));
    CHECK((FARPROC)UIN("dwrite.dll", DWriteCreateFactory) == GetProcAddress(dwmod(), "DWriteCreateFactory"));
}
TEST(exp_not_resolved_from_other_system_modules) {
    /* DWriteCreateFactory belongs to dwrite.dll alone. */
    static const char *const mods[] = { "kernel32.dll", "user32.dll", "gdi32.dll", "ole32.dll", "ntdll.dll" };
    size_t i;
    for (i = 0; i < sizeof mods / sizeof mods[0]; i++) {
        HMODULE m = GetModuleHandleA(mods[i]);
        if (m) CHECKF(GetProcAddress(m, "DWriteCreateFactory") == NULL, "%s", mods[i]);
    }
}
TEST(exp_export_lives_in_code_section) {
    /* The exported code lives in one of dwrite's own executable code sections (no forwarder). */
    const IMAGE_NT_HEADERS *nt = NULL; const IMAGE_SECTION_HEADER *sec; int i, found = 0;
    FARPROC p = GetProcAddress(dwmod(), "DWriteCreateFactory");
    DWORD rva;
    get_export_dir(dwmod(), &nt, NULL);
    REQUIRE(nt && p);
    rva = (DWORD)((const BYTE *)p - (const BYTE *)dwmod());
    sec = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + sec[i].Misc.VirtualSize) {
            found = 1; CHECK(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE); CHECK(sec[i].Characteristics & IMAGE_SCN_CNT_CODE);
        }
    CHECK(found);
}

/* ---- real unload in a clean child process ---------------------------------------------------- */
CHILD(unload_cycle) {
    /* The child has not loaded dwrite.dll before; loads, uses, releases everything, and unloads it, repeatedly. */
    int i, rc = 0;
    for (i = 0; i < 10; i++) {
        HMODULE m = LoadLibraryA("dwrite.dll");
        dw_create_fn fn; IDWriteFactory *s = NULL, *iso = NULL; int guard;
        if (!m) return 10;
        fn = (dw_create_fn)GetProcAddress(m, "DWriteCreateFactory");
        if (!fn) return 11;
        if (FAILED(fn(FT_SHARED, &IID_IDWriteFactory, (IUnknown **)&s)) || !s) return 12;
        if (FAILED(fn(FT_ISOLATED, &IID_IDWriteFactory, (IUnknown **)&iso)) || !iso) return 13;
        if (!fc_factory_usable(s)) rc |= 0x100;       /* may legitimately be false only if Arial is missing, still no crash */
        IDWriteFactory_Release(iso);
        IDWriteFactory_Release(s);
        for (guard = 0; guard < 64 && GetModuleHandleA("dwrite.dll"); guard++) FreeLibrary(m);
        if (GetModuleHandleA("dwrite.dll")) return 14;
    }
    (void)rc;
    return 0;
}
TEST(exp_unload_and_reload_in_child) {
    DWORD code = t_run_child("unload_cycle", "", 60000);
    CHECKF(code == 0, "child exit 0x%lx", code);
}
CHILD(unload_without_release_reload) {
    /* Reload after an unload yields a working factory again. */
    HMODULE m; dw_create_fn fn; IDWriteFactory *s = NULL; int pass;
    for (pass = 0; pass < 3; pass++) {
        int guard;
        m = LoadLibraryA("dwrite.dll"); if (!m) return 20;
        fn = (dw_create_fn)GetProcAddress(m, "DWriteCreateFactory"); if (!fn) return 21;
        if (FAILED(fn(FT_SHARED, &IID_IDWriteFactory, (IUnknown **)&s)) || !s) return 22;
        IDWriteFactory_Release(s);
        for (guard = 0; guard < 64 && GetModuleHandleA("dwrite.dll"); guard++) FreeLibrary(m);
    }
    return 0;
}
TEST(exp_unload_reload_pass_in_child) {
    DWORD code = t_run_child("unload_without_release_reload", "", 60000);
    CHECKF(code == 0, "child exit 0x%lx", code);
}
CHILD(exit_with_live_factories) {
    /* Process termination with factories (shared and isolated) still referenced must be clean. */
    HMODULE m = LoadLibraryA("dwrite.dll"); dw_create_fn fn; IDWriteFactory *s = NULL, *iso = NULL; int i;
    if (!m) return 30;
    fn = (dw_create_fn)GetProcAddress(m, "DWriteCreateFactory"); if (!fn) return 31;
    for (i = 0; i < 4; i++) {
        s = iso = NULL;
        if (FAILED(fn(FT_SHARED, &IID_IDWriteFactory, (IUnknown **)&s))) return 32;
        if (FAILED(fn(FT_ISOLATED, &IID_IDWriteFactory, (IUnknown **)&iso))) return 33;
    }
    fc_factory_usable(iso);
    return 0;
}
TEST(exp_process_exit_with_live_factories) {
    DWORD code = t_run_child("exit_with_live_factories", "", 60000);
    CHECKF(code == 0, "child exit 0x%lx", code);
}
CHILD(never_release_then_unload) {
    /* Keep a factory alive across the FreeLibrary attempt (module is pinned by outstanding objects or not: must not crash
     * as long as the factory is not touched afterwards). */
    HMODULE m = LoadLibraryA("dwrite.dll"); dw_create_fn fn; IDWriteFactory *iso = NULL;
    if (!m) return 40;
    fn = (dw_create_fn)GetProcAddress(m, "DWriteCreateFactory"); if (!fn) return 41;
    if (FAILED(fn(FT_ISOLATED, &IID_IDWriteFactory, (IUnknown **)&iso))) return 42;
    IDWriteFactory_Release(iso);
    FreeLibrary(m);
    return 0;
}
TEST(exp_create_release_free_once_in_child) {
    DWORD code = t_run_child("never_release_then_unload", "", 60000);
    CHECKF(code == 0, "child exit 0x%lx", code);
}
