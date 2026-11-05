/* PE-level properties of dwrite.dll and module load/unload behaviour, all through LoadLibrary / GetProcAddress.
 * dwrite.dll exports exactly one function, DWriteCreateFactory, at ordinal 1. */
#include "rfp.h"

static const IMAGE_NT_HEADERS *nt_of(HMODULE m) {
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)m;
    const IMAGE_NT_HEADERS *nt;
    if (!m || dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    nt = (const IMAGE_NT_HEADERS *)((const BYTE *)m + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE ? nt : NULL;
}
static const IMAGE_EXPORT_DIRECTORY *edir(HMODULE m) {
    const IMAGE_NT_HEADERS *nt = nt_of(m);
    const IMAGE_DATA_DIRECTORY *dd;
    if (!nt) return NULL;
    dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    return dd->VirtualAddress ? (const IMAGE_EXPORT_DIRECTORY *)((const BYTE *)m + dd->VirtualAddress) : NULL;
}

TEST(pe_ordinal1_by_makeintresource_equals_by_name) {
    FARPROC a, b;
    REQUIRE(rfp_mod());
    a = GetProcAddress(rfp_mod(), "DWriteCreateFactory");
    b = GetProcAddress(rfp_mod(), MAKEINTRESOURCEA(1));
    CHECK(a != NULL); CHECK(b != NULL); CHECK(a == b);
}
TEST(pe_ordinal_by_integer_resource_roundtrip_call) {
    /* the ordinal-resolved pointer is a working DWriteCreateFactory */
    typedef HRESULT (WINAPI *fn_t)(DWRITE_FACTORY_TYPE, REFIID, IUnknown **);
    fn_t fn; IUnknown *u = RFP_SENTINEL;
    REQUIRE(rfp_mod());
    fn = (fn_t)GetProcAddress(rfp_mod(), MAKEINTRESOURCEA(1));
    REQUIRE(fn);
    CHECK_HR_OK(fn(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &u));
    CHECK(u != NULL && u != RFP_SENTINEL);
    EXPECT_REFS(u, 1);
    DW_RELEASE(u);
}
TEST(pe_export_ordinal_base_is_one) {
    const IMAGE_EXPORT_DIRECTORY *ed = edir(rfp_mod());
    REQUIRE(ed);
    EXPECT_EQ(ed->Base, 1);   /* ordinal 1 is the first (and only) function slot */
}
TEST(pe_export_counts_are_one_and_one) {
    const IMAGE_EXPORT_DIRECTORY *ed = edir(rfp_mod());
    REQUIRE(ed);
    EXPECT_EQ(ed->NumberOfFunctions, 1);
    EXPECT_EQ(ed->NumberOfNames, 1);
}
TEST(pe_export_name_ordinal_table_maps_to_slot_zero) {
    const IMAGE_EXPORT_DIRECTORY *ed = edir(rfp_mod());
    const WORD *ords;
    REQUIRE(ed && ed->NumberOfNames == 1);
    ords = (const WORD *)((const BYTE *)rfp_mod() + ed->AddressOfNameOrdinals);
    EXPECT_EQ(ords[0], 0);   /* biased index 0 + Base 1 = ordinal 1 */
}
TEST(pe_export_name_string_exact) {
    const IMAGE_EXPORT_DIRECTORY *ed = edir(rfp_mod());
    const DWORD *names; const char *nm;
    REQUIRE(ed && ed->NumberOfNames == 1);
    names = (const DWORD *)((const BYTE *)rfp_mod() + ed->AddressOfNames);
    nm = (const char *)rfp_mod() + names[0];
    EXPECT_STR(nm, "DWriteCreateFactory");
}
TEST(pe_export_image_name_is_dwrite_dll) {
    const IMAGE_EXPORT_DIRECTORY *ed = edir(rfp_mod());
    REQUIRE(ed && ed->Name);
    {
        const char *nm = (const char *)rfp_mod() + ed->Name;
        t_note("export directory image name: %s", nm);
        CHECKF(!lstrcmpiA(nm, "DWrite.dll"), "image name %s", nm);
    }
}
TEST(pe_export_address_matches_getprocaddress) {
    const IMAGE_EXPORT_DIRECTORY *ed = edir(rfp_mod());
    const DWORD *funcs; FARPROC p;
    REQUIRE(ed && ed->NumberOfFunctions == 1);
    funcs = (const DWORD *)((const BYTE *)rfp_mod() + ed->AddressOfFunctions);
    p = GetProcAddress(rfp_mod(), "DWriteCreateFactory");
    CHECK(p == (FARPROC)((const BYTE *)rfp_mod() + funcs[0]));
}
TEST(pe_getprocaddress_is_case_sensitive) {
    static const char *const names[] = { "dwritecreatefactory", "DWRITECREATEFACTORY", "dWriteCreateFactory", "DWriteCreateFactory ", " DWriteCreateFactory" };
    int i;
    REQUIRE(rfp_mod());
    for (i = 0; i < 5; i++) {
        SetLastError(0);
        CHECKF(GetProcAddress(rfp_mod(), names[i]) == NULL, "'%s' resolved", names[i]);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(pe_getprocaddress_decorated_names_fail) {
    static const char *const names[] = { "DWriteCreateFactory@12", "_DWriteCreateFactory@12", "_DWriteCreateFactory", "DWriteCreateFactoryW", "DWriteCreateFactoryA", "?DWriteCreateFactory@@YAJW4DWRITE_FACTORY_TYPE@@AEBU_GUID@@PEAPEAUIUnknown@@@Z" };
    int i;
    REQUIRE(rfp_mod());
    for (i = 0; i < 6; i++) CHECKF(GetProcAddress(rfp_mod(), names[i]) == NULL, "'%s' resolved", names[i]);
}
TEST(pe_getprocaddress_ordinals_other_than_one_fail) {
    int ord;
    REQUIRE(rfp_mod());
    for (ord = 2; ord <= 40; ord++) {
        if (ord == 1) continue;
        SetLastError(0);
        CHECKF(GetProcAddress(rfp_mod(), MAKEINTRESOURCEA(ord)) == NULL, "ordinal %d resolved", ord);
    }
}
TEST(pe_dll_characteristics_note) {
    const IMAGE_NT_HEADERS *nt = nt_of(rfp_mod());
    REQUIRE(nt);
    t_note("DllCharacteristics 0x%04x Subsystem %u OSver %u.%u ImageVer %u.%u LinkerVer %u.%u",
           nt->OptionalHeader.DllCharacteristics, nt->OptionalHeader.Subsystem,
           nt->OptionalHeader.MajorOperatingSystemVersion, nt->OptionalHeader.MinorOperatingSystemVersion,
           nt->OptionalHeader.MajorImageVersion, nt->OptionalHeader.MinorImageVersion,
           nt->OptionalHeader.MajorLinkerVersion, nt->OptionalHeader.MinorLinkerVersion);
    CHECK(nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI || nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI);
}
TEST(pe_dll_is_aslr_and_nx_aware) {
    const IMAGE_NT_HEADERS *nt = nt_of(rfp_mod());
    REQUIRE(nt);
    if (t_is_wine()) { t_skip("emulated image"); return; }
    /* system DLLs of Windows 10 are built with /DYNAMICBASE and /NXCOMPAT */
    CHECK(nt->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE);
    CHECK(nt->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT);
}
TEST(pe_not_a_dotnet_or_resource_only_image) {
    const IMAGE_NT_HEADERS *nt = nt_of(rfp_mod());
    REQUIRE(nt);
    CHECK(nt->OptionalHeader.AddressOfEntryPoint != 0);   /* has a DllMain */
    CHECK(nt->OptionalHeader.AddressOfEntryPoint < nt->OptionalHeader.SizeOfImage);
    CHECK(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress == 0);
}
TEST(pe_image_base_is_module_handle_or_relocated) {
    const IMAGE_NT_HEADERS *nt = nt_of(rfp_mod());
    REQUIRE(nt);
    CHECK(((ULONG_PTR)rfp_mod() & 0xFFFF) == 0);   /* modules are 64K aligned */
    t_note("preferred base 0x%llx loaded at %p", (unsigned long long)nt->OptionalHeader.ImageBase, (void *)rfp_mod());
}
TEST(pe_section_names_include_text_and_rsrc_note) {
    const IMAGE_NT_HEADERS *nt = nt_of(rfp_mod());
    const IMAGE_SECTION_HEADER *sh; int i, text = 0;
    REQUIRE(nt);
    sh = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        char nm[9]; memcpy(nm, sh[i].Name, 8); nm[8] = 0;
        t_note("section %s va=0x%lx vsz=0x%lx chars=0x%lx", nm, sh[i].VirtualAddress, sh[i].Misc.VirtualSize, sh[i].Characteristics);
        if (!strcmp(nm, ".text")) text = 1;
    }
    CHECK(text);
    CHECK(nt->FileHeader.NumberOfSections >= 3);
}
TEST(pe_entry_point_in_executable_section) {
    const IMAGE_NT_HEADERS *nt = nt_of(rfp_mod());
    const IMAGE_SECTION_HEADER *sh; int i, ok = 0;
    DWORD ep;
    REQUIRE(nt);
    ep = nt->OptionalHeader.AddressOfEntryPoint;
    sh = IMAGE_FIRST_SECTION(nt);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
        if (ep >= sh[i].VirtualAddress && ep < sh[i].VirtualAddress + sh[i].Misc.VirtualSize && (sh[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) ok = 1;
    CHECK(ok);
}
TEST(pe_import_directory_walk_does_not_include_dwrite_itself) {
    const IMAGE_NT_HEADERS *nt = nt_of(rfp_mod());
    const IMAGE_IMPORT_DESCRIPTOR *id; int n = 0;
    const IMAGE_DATA_DIRECTORY *dd;
    REQUIRE(nt);
    dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd->VirtualAddress) { t_note("no import directory"); return; }
    for (id = (const IMAGE_IMPORT_DESCRIPTOR *)((const BYTE *)rfp_mod() + dd->VirtualAddress); id->Name && n < 200; id++, n++) {
        const char *nm = (const char *)rfp_mod() + id->Name;
        t_note("import %s", nm);
        CHECKF(lstrcmpiA(nm, "dwrite.dll"), "self import");
    }
    CHECK(n > 0);
}
/* ---- load / unload cycles -------------------------------------------------------------------------- */
TEST(load_n_times_free_n_times_keeps_module) {
    HMODULE base = rfp_mod(); int i; HMODULE h[20];
    REQUIRE(base);
    for (i = 0; i < 20; i++) { h[i] = LoadLibraryA("dwrite.dll"); CHECK(h[i] == base); }
    for (i = 0; i < 20; i++) CHECK(FreeLibrary(h[i]));
    CHECK(GetModuleHandleA("dwrite.dll") == base);   /* the harness reference keeps it alive */
    CHECK(GetProcAddress(base, "DWriteCreateFactory") != NULL);
}
TEST(load_cycles_do_not_disturb_shared_factory_identity) {
    IDWriteFactory *f = dw_shared(); IUnknown *a = NULL, *b = NULL; int i;
    REQUIRE(f);
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&a));
    for (i = 0; i < 25; i++) { HMODULE h = LoadLibraryA("dwrite.dll"); CHECK(h != NULL); if (h) FreeLibrary(h); }
    CHECK_HR_OK(rfp_create(FT_SHARED, &IID_IUnknown, (void **)&b));
    CHECK(a == b);
    DW_RELEASE(a); DW_RELEASE(b);
}
TEST(load_cycles_leave_shared_refcount_untouched) {
    IDWriteFactory *f = dw_shared(); ULONG r0, r1; int i;
    REQUIRE(f);
    r0 = dw_refcount((IUnknown *)f);
    for (i = 0; i < 25; i++) { HMODULE h = LoadLibraryA("dwrite.dll"); if (h) FreeLibrary(h); }
    r1 = dw_refcount((IUnknown *)f);
    /* DllMain attach/detach is not invoked for secondary loads, and a module reference never touches factory counts */
    EXPECT_EQ(r1, r0);
}
TEST(load_loadlibraryex_flags_return_same_module) {
    HMODULE h1, h2;
    REQUIRE(rfp_mod());
    h1 = LoadLibraryExA("dwrite.dll", NULL, 0);
    h2 = LoadLibraryExA("dwrite.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    CHECK(h1 == rfp_mod()); CHECK(h2 == rfp_mod());
    if (h1) FreeLibrary(h1); if (h2) FreeLibrary(h2);
}
TEST(load_getmodulehandleex_pin_free_is_balanced) {
    HMODULE h = NULL; BOOL ok;
    REQUIRE(rfp_mod());
    ok = GetModuleHandleExA(0, "dwrite.dll", &h);   /* takes a reference */
    CHECK(ok); CHECK(h == rfp_mod());
    if (ok) CHECK(FreeLibrary(h));
    h = NULL;
    ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "dwrite.dll", &h);
    CHECK(ok); CHECK(h == rfp_mod());
}
TEST(load_getmodulehandleex_from_export_address) {
    HMODULE h = NULL; FARPROC p;
    REQUIRE(rfp_mod());
    p = GetProcAddress(rfp_mod(), "DWriteCreateFactory");
    CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h));
    CHECK(h == rfp_mod());
}
TEST(load_isolated_factory_per_load_is_distinct) {
    IUnknown *a = NULL, *b = NULL; HMODULE h;
    NEED_EXPORT();
    h = LoadLibraryA("dwrite.dll");
    CHECK_HR_OK(rfp_create(FT_ISOLATED, &IID_IUnknown, (void **)&a));
    CHECK_HR_OK(rfp_create(FT_ISOLATED, &IID_IUnknown, (void **)&b));
    CHECK(a != b);
    DW_RELEASE(a); DW_RELEASE(b);
    if (h) FreeLibrary(h);
}
TEST(load_proc_pointer_stable_over_many_lookups) {
    FARPROC p0; int i;
    REQUIRE(rfp_mod());
    p0 = GetProcAddress(rfp_mod(), "DWriteCreateFactory");
    for (i = 0; i < 1000; i++) if (GetProcAddress(rfp_mod(), "DWriteCreateFactory") != p0) { CHECK(0); break; }
}
TEST(load_datafile_when_already_loaded_as_image) {
    HMODULE h;
    REQUIRE(rfp_mod());
    h = LoadLibraryExA("dwrite.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!h) { t_skip("LOAD_LIBRARY_AS_DATAFILE failed"); return; }
    /* the loader may hand back the already-mapped image or a separate data mapping; either way a valid PE header is mapped */
    t_note("datafile load %s the image handle", h == rfp_mod() ? "returned" : "differs from");
    CHECK(nt_of((HMODULE)((ULONG_PTR)h & ~(ULONG_PTR)3)) != NULL);
    FreeLibrary(h);
}
