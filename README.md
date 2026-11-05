# dwrite.dll conformance corpus

Black-box conformance tests for Windows 10 (22H2, build 19041) x64 `dwrite.dll` (DirectWrite), written in C and cross-compiled with
mingw-w64. `dwrite.dll` exports exactly one function, `DWriteCreateFactory`; every other behaviour lives behind the COM vtables of
the objects it returns and of the objects tests pass in (font loaders, text renderers, inline objects, analysis sources/sinks, ...).
The DLL is never linked: each test loads it with `LoadLibrary`, resolves `DWriteCreateFactory` with `GetProcAddress`
(`U(DWriteCreateFactory)`, typed from the SDK header) and then calls interface methods through the vtables.

The corpus is meant to help someone *implementing* dwrite.dll: expectations are what real Windows does (callback order, arguments,
HRESULTs, out-parameter state), established from observed Windows 10 behaviour, the documentation and deterministic synthetic fonts.

Helper layer (see `docs/DWRITE_TESTING.md`): `common/dw.*` factory/HRESULT/QI/refcount helpers, `common/dwx.*` recording COM test doubles,
`common/ttfgen.*` deterministic synthetic TrueType/OpenType builder. `tools/dwrite_interfaces.tsv` is the public interface surface,
`tools/dw_coverage.py` (`make coverage`) reports interface-method coverage into `COVERAGE.md`.

Modules searched by `U()`: dwrite.dll gdi32.dll user32.dll ole32.dll ntdll.dll kernel32.dll. gdi32/user32 are linked statically only for the
memory-DC scaffolding in `common/dwx.c`; dwrite is never linked.

    make                         # cross-compile build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]    # on Windows; optional test-name filter
    tools\run_all.bat            # everything, results\<area>.txt (add /destructive for dx_* areas)
    python tools\summarize.py results [--failures]

Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last;
exit code 1 if any test failed. `NOTE` lines record observed, version-dependent behaviour.

Tests that need a second process re-launch the exe (`--child <name>`). Scratch files live in
`%TEMP%\dwritet_<pid>\` and are removed on exit. `tools/*_exports.txt` list the named exports.

See `HANDOFF.md` (when present) for how to run and triage the Windows passes.

## Areas

| Area | Purpose |
|---|---|
| bitmap_render_target | IDWriteBitmapRenderTarget/GdiInterop drawing into a memory DC, pixel readback |
| color_glyphs | TranslateColorGlyphRun, color run enumerators, palettes |
| com_lifetime_threads | refcounts, object lifetime after factory release, multi-thread use of shared/isolated factories |
| core_dw_helpers | self-test of the common/dw and dwx helper layer |
| core_smoke | LoadLibrary/GetProcAddress/DWriteCreateFactory smoke |
| dx_cleartype_settings | DESTRUCTIVE: system/registry rendering settings (ClearType, gamma) and their effect on rendering params; restores state |
| dx_gap_00 | DESTRUCTIVE gap tests (system font installation / remote font paths), restore after |
| dx_gap_01 | DESTRUCTIVE gap tests |
| dx_gap_02 | DESTRUCTIVE gap tests |
| error_hresults | argument validation, HRESULT values, out-parameters zeroed on failure across all factory/face/layout entry points |
| factory_create | DWriteCreateFactory: types, IIDs, shared vs isolated, loader registration state, export surface |
| factory_interfaces_qi | QueryInterface matrix for every object the factory creates, per interface version |
| factory_text_objects | CreateTextFormat/TextLayout/Typography/NumberSubstitution/RenderingParams and friends |
| font_collection_loader | custom IDWriteFontCollectionLoader/Enumerator contract (call order, keys, failures) |
| font_collection_system | system font collection enumeration, FindFamilyName, GetFontFromFontFace |
| font_family_matching | GetMatchingFonts, weight/stretch/style matching, family lists |
| font_file_loader | custom IDWriteFontFileLoader/Stream contract, analyze, fragments |
| font_metrics | design/GDI-compatible metrics, glyph metrics |
| font_object | IDWriteFont/FontFamily/FontList/FontFace creation, names, simulations |
| font_resource_reference | IDWriteFontResource/FontFaceReference (IDWriteFactory3) |
| font_set | IDWriteFontSet/Builder/Set1, property filtering |
| fontface_core | IDWriteFontFace basics (glyph indices, advances, metrics, tables) |
| fontface_ext | IDWriteFontFace1..5 extensions (adv/kerning/color/render) |
| fontface_outline | GetGlyphRunOutline via recording geometry sink, exact outlines of synthetic glyphs |
| gap_00 | gap areas: analysis, collections, remote/download interfaces |
| gap_01 | gap areas: leftover interface methods |
| gap_02 | gap areas: leftover interface methods |
| gdi_interop | IDWriteGdiInterop LOGFONT/HFONT conversion, metrics, signatures |
| glyph_run_analysis | CreateGlyphRunAnalysis bounds/alpha textures |
| localized_strings | IDWriteLocalizedStrings, informational strings, name tables |
| number_typography_fallback | NumberSubstitution, Typography, FontFallback/Builder |
| opentype_tables | TryGetFontTable and OpenType table parsing via synthetic fonts |
| r2_bidi_linebreak_matrix | text analyzer bidi levels and line break opportunities matrices |
| r2_collection_loader_contract | second-round collection loader contract and names |
| r2_color_bitmap_glyphs | IDWriteFontFace4 bitmap/SVG/color glyph data |
| r2_factory_platform_dll | PE image facts, export, platform behaviour, in-memory loaders, threads |
| r2_fallback_semantics | MapCharacters/fallback semantics |
| r2_gdi_interop_deep | deeper GDI interop (needs a display driver; skipped headless) |
| r2_layout_bidi_reorder | layout bidi reordering, kerning, last-line wrapping |
| r2_rendering_modes | rendering mode / measuring mode / grid-fit resolution |
| r2_shaping_features | shaping with synthetic GSUB/GPOS features |
| r2_text_render_callbacks | IDWriteTextRenderer callbacks from Draw |
| r2_typographic_features | typographic feature application in layout |
| r2_variable_fonts | variable font axes, instances, resources |
| rendering_params | IDWriteRenderingParams 1/2/3 getters and creation |
| synthetic_fonts | synthetic TrueType fixtures through factory, face, in-memory loader |
| text_analyzer_core | IDWriteTextAnalyzer script/bidi/linebreak/shaping/placements |
| text_analyzer_ext | IDWriteTextAnalyzer1/2 extensions, glyph orientation, vertical |
| text_format | IDWriteTextFormat/1/2/3 properties and validation |
| text_hit_testing | HitTestPoint/TextPosition/TextRange |
| text_layout_core | IDWriteTextLayout core API |
| text_layout_draw | Draw, inline objects, effects |
| text_layout_metrics | line/cluster/overhang metrics |
| text_layout_ranges | per-range property setters and getters |
| text_layout_wrap_trim | wrapping, trimming, ellipsis |
| unicode_script_coverage | script and character coverage matrices |
| vertical_text_orientation | vertical reading, glyph orientation transforms |

**Method.** Every area is a single exe built from `tests/<area>/*.c` plus `common/`. Expectations come from observed Windows 10 behaviour,
vendored docs and public headers; deterministic synthetic fonts (`common/ttfgen`) give exact numbers, system fonts are used for relations and
`t_note` only (unless the value was verified in the font tables). Callback objects we pass in (loaders, renderers, analysis source/sink) record order and arguments.

**Running on Windows.** Copy `build\*.exe` to a disposable Win10 22H2 x64 VM, run `tools\run_all.bat` (add `/destructive` for `dx_*`), then
`python tools\summarize.py build\results --failures`. See HANDOFF.md.

**Fixture policy.** Hard CHECKs only for facts from docs/IDL or synthetic fixtures. Font-file, OS-version, locale, DPI and ClearType dependent facts are
`t_note`; missing prerequisites are `t_skip`. Non-`dx_` areas never write the registry, change system settings, install fonts or show windows;
scratch files live under `t_tmpdir()`; every registered loader is unregistered by the test that registered it.
