# dwrite.dll conformance corpus

Black-box conformance tests for Windows 10 (22H2, build 19041) x64 `dwrite.dll` (DirectWrite), written in C and cross-compiled with
mingw-w64. `dwrite.dll` exports exactly one function, `DWriteCreateFactory`; every other behaviour lives behind the COM vtables of
the objects it returns and of the objects tests pass in (font loaders, text renderers, inline objects, analysis sources/sinks, ...).
The DLL is never linked: each test loads it with `LoadLibrary`, resolves `DWriteCreateFactory` with `GetProcAddress`
(`U(DWriteCreateFactory)`, typed from the SDK header) and then calls interface methods through the vtables.

The corpus is meant to help someone *implementing* dwrite.dll: expectations are what real Windows does (callback order, arguments,
HRESULTs, out-parameter state), established from the Windows 10 decompilation, the documentation and deterministic synthetic fonts.

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
