# HANDOFF: dwrite.dll conformance corpus

## Purpose
57 exes (about 14.5k tests, 20k+ check sites) asserting the behaviour of Windows 10 22H2 x64 `dwrite.dll` (DirectWrite). The DLL has one export,
`DWriteCreateFactory`; all tests load it with LoadLibrary/GetProcAddress and drive it through COM vtables and through COM objects we pass in.
Windows 10 results are the authority. So far the corpus has only run headless under Wine (compile/hang/crash validation), where Wine's own dwrite and fonts
make thousands of checks differ. Expect some wrong expectations on the first real run: triage each.

## Prerequisites
- Disposable Win10 22H2 (19041) x64 VM with a snapshot, no domain, stock fonts: Arial, Times New Roman, Courier New, Segoe UI, Segoe UI Emoji, Segoe UI Symbol, Tahoma, Consolas, Marlett, Bahnschrift, MS Gothic (msgothic.ttc). Default ClearType/DPI settings preferred (note the DPI scale and locale in the report).
- A standard user account and an elevated administrator account.
- Build host: Linux with mingw-w64 (`make -j8`), or copy a prebuilt `build/`. Python 3 on the VM for summarize.py (or copy result txt files back).

## Build
    make clean && make -j8        # build/<area>.exe, static, dwrite never linked (a few harmless compiler warnings)

## Run (revert the snapshot between passes)
1. Standard user:  `tools\run_all.bat`
2. Elevated admin: `tools\run_all.bat`
3. Elevated admin, destructive: `tools\run_all.bat /destructive` (dx_cleartype_settings, dx_gap_00/01/02; they restore what they change)
Single area/test: `build\<area>.exe <name-substring>`. Collect `build\results\*.txt`, then `python tools\summarize.py build\results --failures`.
Exit code is 1 when any test failed. Per-exe hard limit is 600 s in run_all.bat; the slowest exes under Wine take about 90 s.

## Windows 10 baseline method
Run the whole corpus twice from a fresh snapshot each time: once as a standard user and once from an elevated administrator session (the `/destructive`
pass is a third, elevated-only run). Use a hard per-exe time limit (600 s) so a hang is recorded rather than blocking the run. Exes are independent
(each uses its own scratch directory), so several can run in parallel; keep parallelism modest (for example 4 to 8) so timing-sensitive tests
(com_lifetime_threads, r2_factory_platform_dll) are not starved, and re-run any exe that fails only under load on its own before judging it.
Save one result file per exe and compare the two runs with `tools\summarize.py`. Known standard-vs-admin difference: tests that probe
elevation-dependent behaviour (dx_* steps and a few registry/system-font visibility checks) skip or differ for the standard user; every other area should
produce identical results in both runs, and a difference elsewhere is a flaky test or an environment issue to investigate.

## Triage
1. Wrong expectation: Windows differs from the test. Re-check against the documentation and observed behaviour, fix the test.
2. Environment/SKU/locale/font revision: turn the hard CHECK into `t_note` or `t_skip`.
3. Real divergence: this is what the corpus is for; record it.
Re-run only the test (`build\<area>.exe <name>`), commit with a short message.

## Expected skips
- Missing fonts (Bahnschrift, MS Gothic, Segoe UI Emoji, ...) skip font-dependent tests; opentype_tables, localized_strings, gdi_interop, fontface_ext and font_metrics skip the most.
- r2_gdi_interop_deep skipped 228/234 headless under Wine (needs a display driver); on a Win10 desktop session it should run.
- dx_* skip entirely without /destructive. Elevation-dependent steps in dx_* skip when not admin.

## Wine run (headless, Wine's own dwrite and fonts), per area
Wine differs on fonts and some DirectWrite behaviour; these numbers are a baseline for compile/hang/crash health only. Failures here are expected divergence, not proof of wrong tests.
Totals: 14469 tests, 3221 failed, 1923 skipped, 172335 failed checks (dominated by color_glyphs, gap_01, text_hit_testing, r2_color_bitmap_glyphs, r2_rendering_modes: one root cause repeats per glyph/pixel).

| Area | Tests | Failed | Skipped | Checks pass | Checks fail | Runtime s |
|---|---:|---:|---:|---:|---:|---:|
| bitmap_render_target | 228 | 32 | 16 | 2319 | 81 | 8 |
| color_glyphs | 218 | 95 | 29 | 81723 | 17853 | 7 |
| com_lifetime_threads | 301 | 23 | 3 | 17039 | 122 | ~20 |
| core_dw_helpers | 29 | 0 | 0 | 327 | 0 | 2 |
| core_smoke | 3 | 0 | 0 | 4 | 0 | 2 |
| dx_cleartype_settings | 202 | 56 | 13 | 792 | 136 | 2 |
| dx_gap_00 | 10 | 0 | 10 | 0 | 0 | 1 |
| dx_gap_01 | 3 | 0 | 0 | 8 | 0 | 2 |
| dx_gap_02 | 3 | 0 | 0 | 7 | 0 | 2 |
| error_hresults | 191 | 83 | 20 | 2024 | 509 | 6 |
| factory_create | 310 | 42 | 1 | 1730 | 80 | 26 |
| factory_interfaces_qi | 349 | 29 | 22 | 64125 | 2000 | 13 |
| factory_text_objects | 192 | 43 | 10 | 4936 | 442 | 17 |
| font_collection_loader | 190 | 36 | 3 | 1457 | 114 | 17 |
| font_collection_system | 298 | 12 | 46 | 27069 | 60 | 3 |
| font_family_matching | 325 | 192 | 62 | 30383 | 4257 | 6 |
| font_file_loader | 279 | 56 | 38 | 3610 | 618 | 2 |
| font_metrics | 245 | 49 | 92 | 34018 | 847 | 7 |
| font_object | 299 | 9 | 53 | 7200 | 25 | 7 |
| font_resource_reference | 190 | 71 | 28 | 1176 | 427 | 2 |
| font_set | 267 | 178 | 7 | 1836 | 1029 | 3 |
| fontface_core | 306 | 43 | 32 | 3524 | 346 | 4 |
| fontface_ext | 345 | 43 | 96 | 3125 | 196 | 6 |
| fontface_outline | 210 | 99 | 30 | 10752 | 1575 | 3 |
| gap_00 | 62 | 6 | 36 | 11319 | 151 | 2 |
| gap_01 | 224 | 66 | 37 | 65347 | 63267 | 32 |
| gap_02 | 205 | 44 | 71 | 3331 | 3288 | 5 |
| gdi_interop | 238 | 24 | 112 | 2828 | 319 | 16 |
| glyph_run_analysis | 162 | 23 | 13 | 2137 | 86 | 2 |
| localized_strings | 471 | 34 | 156 | 6937 | 486 | 2 |
| number_typography_fallback | 175 | 32 | 13 | 4533 | 110 | 7 |
| opentype_tables | 630 | 14 | 232 | 4286 | 92 | 8 |
| r2_bidi_linebreak_matrix | 586 | 70 | 155 | 633141 | 272 | 3 |
| r2_collection_loader_contract | 248 | 91 | 0 | 2080 | 162 | 2 |
| r2_color_bitmap_glyphs | 116 | 79 | 7 | 5787 | 12015 | 5 |
| r2_factory_platform_dll | 213 | 29 | 19 | 1909 | 203 | 76 |
| r2_fallback_semantics | 213 | 49 | 12 | 1652 | 294 | 13 |
| r2_gdi_interop_deep | 234 | 0 | 228 | 68 | 0 | 11 |
| r2_layout_bidi_reorder | 571 | 245 | 7 | 5158 | 4492 | 91 |
| r2_rendering_modes | 268 | 131 | 0 | 75230 | 7558 | 4 |
| r2_shaping_features | 278 | 7 | 1 | 1356 | 9 | 1 |
| r2_text_render_callbacks | 171 | 44 | 2 | 2209 | 211 | 12 |
| r2_typographic_features | 295 | 65 | 7 | 3095 | 1120 | 5 |
| r2_variable_fonts | 275 | 91 | 46 | 1180 | 402 | 4 |
| rendering_params | 236 | 24 | 0 | 13543 | 194 | 2 |
| synthetic_fonts | 638 | 129 | 26 | 2073 | 358 | 2 |
| text_analyzer_core | 465 | 53 | 18 | 7284 | 128 | 2 |
| text_analyzer_ext | 329 | 163 | 27 | 2903 | 3551 | 3 |
| text_format | 231 | 23 | 5 | 3684 | 176 | 5 |
| text_hit_testing | 210 | 193 | 16 | 611 | 40933 | 19 |
| text_layout_core | 266 | 40 | 1 | 8994 | 236 | 18 |
| text_layout_draw | 243 | 24 | 7 | 2286 | 66 | 16 |
| text_layout_metrics | 191 | 32 | 0 | 6877 | 161 | 13 |
| text_layout_ranges | 217 | 30 | 0 | 66004 | 186 | 17 |
| text_layout_wrap_trim | 187 | 56 | 0 | 3560 | 189 | 14 |
| unicode_script_coverage | 434 | 18 | 44 | 5045 | 287 | 9 |
| vertical_text_orientation | 194 | 101 | 14 | 1043 | 616 | 8 |

No exe hangs or crashes under Wine after the integration fixes below.

## Integration fixes made
- common/dwx.c: ld_Create appended to the loader's stream table without a lock; with 8 threads the counter passed 128 and corrupted the heap (crash in com_lifetime_threads thr_shared_factory_custom_loader_face_8x200). Now locked.
- tests/text_layout_draw/draw_glyphruns.c bidi_run_coverage_mixed read past the 17-character source string (loop to 19); fixed to 16.
- Known harmless: r2_layout_bidi_reorder helpers lines_with/width_of use skip macros (`return;`) in non-void functions; only reachable on a skip path.

## Known weak spots
- Font-dependent `t_note` lines (search results for NOTE) are observations to review, not assertions: Arial/Segoe UI metrics, Bahnschrift instances, MS Gothic vertical forms, emoji color layers, name-table strings per locale.
- Most failure-prone under Wine and therefore least validated against real behaviour: r2_layout_bidi_reorder, text_hit_testing, font_set, font_family_matching, text_analyzer_ext, r2_rendering_modes, r2_color_bitmap_glyphs, color_glyphs, vertical_text_orientation, synthetic_fonts. Review these first on Windows.
- Pixel-readback tests (bitmap_render_target, r2_rendering_modes, glyph_run_analysis, dx_cleartype_settings) depend on rasterizer/ClearType configuration; tolerances were chosen loosely.
- Never validated on Windows: everything.

## Cleanup of leftovers
Scratch directories: `rmdir /s /q %TEMP%\dwritet_*`. dx_* areas restore their own registry/SPI changes; if a run was killed, revert the VM snapshot (preferred) or check
`HKCU\Control Panel\Desktop` FontSmoothing* values and `HKCU\Software\Microsoft\Avalon.Graphics`. Kill stray `*.exe` from build\ in Task Manager.

## Report back
Totals per area (summarize.py output), wrong expectations fixed (with commit hashes), real divergences (area, test, check, observed vs expected), skipped areas and why, any crash/hang with the area and test name.
