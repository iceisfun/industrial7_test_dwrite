# Writing DirectWrite tests

`dwrite.dll` exports exactly one function, `DWriteCreateFactory` (ordinal 1). Everything else is reached through COM
vtables of the objects it returns, and through COM objects *we* pass in (font loaders, text renderers, ...). A test is therefore:

    LoadLibrary("dwrite.dll")  ->  GetProcAddress("DWriteCreateFactory")  ->  call  ->  interface methods through vtables

The DLL is never linked. `#include "dwx.h"` gives you everything below (it pulls in `dw.h`, `t.h`, `ttfgen.h`).

## Factory acquisition
    IDWriteFactory *f = dw_shared();                 // cached, owned by the helper, never Release it
    IDWriteFactory *i = dw_isolated();
    IDWriteFactory3 *f3 = dw_factory_v(DWRITE_FACTORY_TYPE_SHARED, 3);   // NULL when the interface is unsupported
    void *raw; HRESULT hr = dw_create_factory(DWRITE_FACTORY_TYPE_ISOLATED, &IID_IDWriteFactory, &raw);   // fresh, you release
    IDWriteFactory *own = dw_new_factory(DWRITE_FACTORY_TYPE_ISOLATED);
Facts from the Win10 decompilation: `*ppFactory` is written NULL before anything else (never pass a NULL out pointer);
type 0 (SHARED) returns the process-wide factory, type 1 (ISOLATED) builds a new `DWriteFactory`; any other type throws an
argument exception that the export converts to a failure HRESULT.
Call interface methods COBJMACROS style: `IDWriteFactory_CreateTextFormat(f, ...)`, or `obj->lpVtbl->Method(obj, ...)`.
Inherited methods work through the derived name (`IDWriteFactory3_CreateTextFormat`). Guard version-specific tests:
`void *p = dw_factory_v(SHARED, 4); if (!p) { t_skip("IDWriteFactory4 unsupported"); return; }`.

## Checking HRESULTs
`CHECK_HR(expr, expected)` compares exactly and prints symbolic names (`DWRITE_E_NOFONT`, `E_NOINTERFACE`, ...); it returns the HRESULT.
`CHECK_HR_OK`, `CHECK_HR_FAILED`, `CHECK_OUT_NULL(p)` (out-parameters must be zeroed on failure), `DW_REQUIRE_OK(expr)` (skip on setup failure),
`EXPECT_REFS(obj, n)`, `DW_RELEASE(p)`. `CHECK_NEAR(a, b, eps)` / `CHECK_FLOAT_EQ`. Warning: `EXPECT_EQ`/`CHECK` macros evaluate
their arguments twice - never put an AddRef/Release/QueryInterface call inside them; assign to a temporary first.

## QueryInterface matrices
    dw_qi_row rows[] = { DW_QI_ROW(IDWriteFont, 1), DW_QI_ROW(IDWriteFont1, -1), DW_QI_ROW(IDWriteFactory, 0) };
    DW_CHECK_QI(obj, rows);        // 1 = must succeed, 0 = must fail with E_NOINTERFACE and *ppv NULL, -1 = t_note only
`dw_iid_table()` lists every public IID (dwrite.h..dwrite_3.h, plus Win11 interfaces absent from the SDK headers via `DW_IID_*`).
The check also verifies COM identity (QI(IUnknown) is stable, every result has the same identity) and refcount balance.

## Test doubles (common/dwx.h)
Every double is a C struct with the vtable pointer first; cast it to the interface to pass it to dwrite. All start with refcount 1.
Use `dwx_release(obj)` at the end; `dwx_refs`, `dwx_destroyed` stay readable after the last release (memory is kept until
`dwx_free`/exit). `DWX_LEAK_BEGIN(tag)` / `DWX_LEAK_END(tag)` check that every double created in between reached refcount 0.

| Helper | Interfaces | Notes |
|---|---|---|
| `dwx_mem_loader` / `dwx_mem_stream` | IDWriteFontFileLoader / FileStream | blobs by 4-byte key; records every ReadFileFragment (offset,size), balance of Release, GetFileSize, GetLastWriteTime; fail injection; `copy_fragments` hands out malloc'd copies with real contexts |
| `dwx_font` (`dwx_font_open`, `dwx_font_open_default`, `dwx_font_close`) | - | blob -> loader -> file -> Analyze -> face in one call; always call close |
| `dwx_coll_loader` / `dwx_enum` | IDWriteFontCollectionLoader / FileEnumerator | enumerates keys of a `dwx_mem_loader`; records create/MoveNext/GetCurrentFontFile; fail injection |
| `dwx_renderer` | IDWriteTextRenderer, IDWritePixelSnapping, optional IDWriteTextRenderer1 | deep copies of glyph runs, run descriptions, underline/strikethrough, inline calls, in call order; configurable snapping/transform/ppd; fail injection |
| `dwx_inline` | IDWriteInlineObject | configurable metrics/overhangs/break conditions, records Draw arguments |
| `dwx_geom` | ID2D1SimplifiedGeometrySink (= IDWriteGeometrySink) | event list, all points, fill mode, figure balance, order violations |
| `dwx_ana_src` | IDWriteTextAnalysisSource(+1) | text, locale, direction, number substitution, chunking, call log |
| `dwx_ana_sink` | IDWriteTextAnalysisSink(+1) | per-position script/breakpoint/bidi/orientation tables, run lists, overlap detection |
| `dwx_listener` | IDWriteFontDownloadListener | counts, last args, event handle |
| `dwx_effect` | IUnknown | counting client drawing effect; `dwx_draw.effect_refs` records its refcount at draw time |
| `dwx_dc` | - | 32-bpp memory DC, pixel/bbox/hash readback for GDI interop and bitmap render target tests |

Registration discipline: every `RegisterFontFileLoader` / `RegisterFontCollectionLoader` must be unregistered before the test
ends, also after failed checks (`dwx_font_close`, `dwx_loader_unregister`, `dwx_coll_unregister` are safe when not registered).
Order matters: unregister a file loader only after the objects built from it have been released.
Callback order, arguments and counts are part of the contract being verified, so assert on the recorded values.

## Synthetic fonts (common/ttfgen.h)
Prefer a synthetic font whenever the expectation is a number: every byte is deterministic, so metrics, advances, glyph indices,
outlines and layout widths are exact on any Windows build. `ttf_default_spec(&s)` describes the default font (UPM 2048; gid 0
notdef square advance 1200; gid 1 space; A-Z, a-z, 0-9 are boxes (100,0)-(900,1400) advance 1000; extra glyphs 64..68: triangle,
quadratic round, composite, empty-500, two-contour ring; hhea/typo/win metrics all different on purpose). Edit the spec
(names, metrics, `fs_selection`, cmap subtables, GSUB/GPOS/kern/vhea/BASE/COLR/CPAL/gasp, raw tables) and `ttf_build()` it:

    ttf_spec s; ttf_style_spec(&s, "MyFam", 700, 5, /*italic*/1);  s.typo_asc = 1500; s.fs_selection |= 0x80;
    size_t n; uint8_t *blob = ttf_build(&s, &n);  dwx_font h; dwx_font_open(&h, f, blob, n, 0, DWRITE_FONT_SIMULATIONS_NONE); ...

`ttf_style_spec` writes RIBBI naming for weight 400/700 at normal width and typographic family/subfamily (ids 16/17) otherwise.
`ttf_build_ttc`, `ttf_wrap_woff`, `ttf_corrupt` (truncate, bad magic, numTables, table offset/length/checksum, head magic, zeroed tables)
cover container and error paths. To serve a whole family, add several blobs to one `dwx_mem_loader` and enumerate them with
`dwx_coll_loader` (see tests/core_dw_helpers/helpers.c for a complete custom collection plus text layout).
Hard-check numbers derived from the spec; they are facts about the synthetic font, not about the OS (but how DirectWrite
rounds/derives them - e.g. which of hhea / typo / win metrics it reports - is exactly what you are testing: confirm in the decompilation).

## System fonts
Installed fonts vary by SKU, language and updates. Use them only for relations (bold wider than regular, fallback picks a family)
and `t_note` of exact values. `dw_have_font(f, L"Arial")` / `dw_sysface()` / `dw_find_system_font({"arial.ttf",NULL})`; when the
font is absent `t_skip`. Arial, Times New Roman, Courier New, Segoe UI, Tahoma, Consolas, Marlett, Segoe UI Symbol/Emoji,
Bahnschrift (variable), MS Gothic (msgothic.ttc) are expected on Windows 10. Exact Arial 2048-unit metrics may be hard-checked
once verified against the font tables with `dw_face_table(face, DW_TAG('h','h','e','a'), &len)`.

## Valid and invalid tests
Valid: argument validation visible in the decompilation or docs (E_INVALIDARG, E_NOINTERFACE, E_NOT_SUFFICIENT_BUFFER, DWRITE_E_*),
out-parameters zeroed on failure, buffer protocols (size 0 / exact-1 / exact / NULL), enum range checks, index out of range,
ranges beyond the text length, refcounts, QI identity, object lifetime after factory release.
Invalid: environment failures (OOM, absurd sizes), non-NULL garbage pointers, fuzzing, tight timing, locale/SKU/DPI/ClearType-dependent
hard checks (`t_note` them). Destructive tests (registry, SPI, system fonts) live in `dx_<name>` areas only.

## Wine caveats (validation only; Windows is the authority)
`tools/wine_run.sh build/<area>.exe [filter]` runs headless in a throw-away prefix. Wine ships its own dwrite and different fonts
(no Arial/Segoe UI), accepts invalid factory types, and differs in many corner cases, so a failing check under Wine is not proof the test is wrong.
Guard with `t_is_wine()` + `t_skip` only where Wine would crash or hang. Every area must still build and run to completion.

## Interface coverage
`tools/gen_interfaces.py` regenerates `tools/dwrite_interfaces.tsv` (interface, base, method, vtable index, introduced-in) and `common/dw_iids.inc`.
`tools/dw_coverage.py` (`make coverage`) scans tests/ for `I<Interface>_<Method>(` and `lpVtbl->Method(` calls and writes `COVERAGE.md`.

## Helper API summary
dw.h: `dw_proc`, `dw_create_factory`, `dw_shared`, `dw_isolated`, `dw_new_factory`, `dw_factory_v`, `dw_factory_iid`, `dw_hr_name`, `dw_check_hr`/`CHECK_HR*`,
`dw_refcount`, `EXPECT_REFS`, `dw_qi_check_`/`DW_CHECK_QI`, `dw_iid_table`, `dw_same_identity`, `dw_wide`, `dw_narrow`, `dw_ls_find/get/en`, `dw_fonts_dir`,
`dw_find_system_font`, `dw_have_font`, `dw_sysface`, `dw_face_table`, `DW_TAG`.
dwx.h: see the table above. ttfgen.h: `ttf_default_spec`, `ttf_style_spec`, `ttf_set_names/add_name`, `ttf_build[_default|_styled|_ttc]`, `ttf_wrap_woff`, `ttf_find_table`, `ttf_corrupt`.
