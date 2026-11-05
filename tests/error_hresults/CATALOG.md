# error_hresults catalogue

Failure HRESULTs produced by `dwrite.dll` (Windows 10 22H2, build 19041, x64), the public call that provokes each one, the
decompiled function that fixes the value, and how sure the expectation is. All values are read from the Ghidra decompilation of
the Windows binary; nothing here is taken from another implementation.

## Where the values come from

DirectWrite reports failures through C++ exceptions that every public entry point converts to an HRESULT
(`MapExceptionToHresult` at RVA 0x136e00 and the exception constructors next to it):

| Exception type | HRESULT | Constructor / evidence |
|---|---|---|
| `ArgumentException` (and `ArgumentOutOfRangeException`) | `E_INVALIDARG` 0x80070057 | 0x136b5c |
| `ArgumentInsufficientBufferException` | 0x8007007A (`HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)`) | 0x136b94 |
| `IndexOutOfRangeException` | `E_FAIL` 0x80004005 | 0x136c70 |
| `FileFormatException` | `DWRITE_E_FILEFORMAT` 0x88985000 | 0x136c48 |
| `NotSupportedException` | `DWRITE_E_UNSUPPORTEDOPERATION` 0x88985009 | 0x136d0c |
| `InvalidCacheDataException` | `DWRITE_E_CACHEFORMAT` 0x88985007 | 0x136cd4 |
| `RemoteFontException` | `DWRITE_E_REMOTEFONT` 0x8898500D | 0x136d54 |
| `IntegerOverflowException` | 0x80070216 | 0x136cac |
| `std::bad_alloc` | `E_OUTOFMEMORY` 0x8007000E | MapExceptionToHresult catch_2 |
| `CallbackException(hr)` | `hr`, unchanged | 0x136bec; thrown by `CallbackException::ThrowIfFailed` for loader/stream/enumerator/sink/source/renderer callbacks |
| `OSException(hr)` | `hr` | 0x136d34 (ALREADYREGISTERED 0x88985006, TEXTRENDERERINCOMPATIBLE, FLOWDIRECTIONCONFLICTS) |
| `IOException` | `IOException::MapFileOpenError(GetLastError())` | 0x136f5c |
| any other exception | `E_FAIL` | MapExceptionToHresult catch_5/6 |

`MapFileOpenError`: ERROR_FILE_NOT_FOUND(2), PATH_NOT_FOUND(3), 0xF, 0x15, 0x37, 0x40, INVALID_PARAMETER(0x57), 0x7B INVALID_NAME,
0xA1, 0xCE, 0x10B map to `DWRITE_E_FILENOTFOUND`; ACCESS_DENIED(5), SHARING_VIOLATION(0x20), 0x21, 0x35, 0x41-0x43, 0x56, 0x51F, 0x522,
0x52E map to `DWRITE_E_FILEACCESS`; anything else positive becomes `HRESULT_FROM_WIN32(code)`.

## DWRITE_E_* codes

Facility 0x898 (`0x8898xxxx`), codes 0x5000..0x5010, contiguous (test file `codes.c`).

| Code | Value | Trigger used in the corpus | Verified by | Confidence | Test |
|---|---|---|---|---|---|
| FILEFORMAT | 0x88985000 | `CreateFontFace(TRUETYPE)` over garbage, an empty blob or an sfnt truncated inside the header or table directory | `FileFormatException` thrown by `CheckedPtr<..., FailAsExceptionPolicy<FileFormatException>>` in the face parser; `DWriteFontFace::Create` 0x693c8 | high for header/directory truncation and garbage, medium for the structured damage matrix (outcome recorded with `t_note`, rule: S_OK or FILEFORMAT) | `corrupt_*` |
| UNEXPECTED | 0x88985001 | not reproducible | no public path found in the decompilation | - | documented only |
| NOFONT | 0x88985002 | `IDWriteGdiInterop::CreateFontFromLOGFONT` for an unknown family; `IDWriteFontCollection::GetFontFromFontFace` with a foreign face; `IDWriteFontCollection2::GetMatchingFonts` with an unknown family; `ConvertFontToLOGFONT` for fonts without a Win32 description | `DWriteGdiInterop::CreateFontFromLOGFONT` 0x8c800, `DWriteFontCollection::GetFontFromFontFace` 0x45fd0, `DWriteFontCollection::GetMatchingFonts` 0xe2b30 | high | `gdi_createfontfromlogfont_*`, `collection_*`, `collection2_getmatchingfonts_*` |
| FILENOTFOUND | 0x88985003 | `CreateFontFileReference` of a missing file / missing directory without a FILETIME; `CreateFontFace` over a reference to a missing file | `MapFileOpenError`, `CreateLocalFontFileReferenceInternal` 0x42fc4, `LocalFileLoader::CreateStreamFromKey` | high (create without time), medium-high (create face lazily) | `path_*` |
| FILEACCESS | 0x88985004 | file opened exclusively (no sharing) by the test, then `CreateFontFace`; directory used as a font path | `MapFileOpenError` (0x20 and 5 map here) | high for the mapping, medium for which API opens the file (exact result of the no-FILETIME reference case is a `t_note`) | `path_exclusively_locked_file_is_fileaccess`, `path_directory_is_not_a_font_file` |
| FONTCOLLECTIONOBSOLETE | 0x88985005 | not reproducible without installing or removing a system font | - | - | documented only |
| ALREADYREGISTERED | 0x88985006 | `RegisterFontFileLoader` / `RegisterFontCollectionLoader` with a pointer that is already registered | `FontLoaderManagerBase<..>::Register` 0x4da64 / 0x42388 (`OSException(-0x7767affa)`) | high | `reg_file_*`, `reg_coll_*` |
| CACHEFORMAT | 0x88985007 | not reproducible (needs a damaged font-cache service blob) | `InvalidCacheDataException` | - | documented only |
| CACHEVERSION | 0x88985008 | not reproducible | - | - | documented only |
| UNSUPPORTEDOPERATION | 0x88985009 | operations on a raw-CFF face (`GetDesignGlyphMetrics`, `TryGetFontTable`, `GetGdiCompatibleMetrics`, `ConvertFontFaceToLOGFONT`, sideways `GetDesignGlyphAdvances`, `GetKerningPairAdjustments`, `GetGlyphs`, `GetUnicodeRanges`) | face-type byte 7 checks in 0x5e2e0, 0x900e0, 0x8ea30, 0x256c0, 0x68d90, 0x24eb0, 0x61be0, 0xdd0b0 | high that those paths return it; the test builds a minimal bare CFF blob and skips when `CreateFontFace(RAW_CFF)` rejects it | `unsupported_raw_cff_face_operations` |
| TEXTRENDERERINCOMPATIBLE | 0x8898500A | `IDWriteTextLayout::Draw` with vertical reading direction and a renderer that does not implement `IDWriteTextRenderer1` | `TextLayout::Draw` 0x73b9c (`OSException(-0x7767aff6)` when QI for {d3e0e934-22a0-427e-aae4-7d9574b59db1} fails and the reading-direction axis bit is set) | high | `textrenderer_incompatible_*` |
| FLOWDIRECTIONCONFLICTS | 0x8898500B | reading direction and flow direction on the same axis (horizontal+horizontal, vertical+vertical); surfaced by the first call that builds the layout (`GetMetrics`, `GetLineMetrics`, `GetClusterMetrics`, `DetermineMinWidth`, `Draw`) | `TextLayout::TextLayout` 0x70698 (`OSException(-0x7767aff5)`) | high | `flowdirection_*` |
| NOCOLOR | 0x8898500C | `IDWriteFactory2::TranslateColorGlyphRun` / `IDWriteFactory4::TranslateColorGlyphRun` for a run without coloured glyphs; `IDWriteFontFace2::GetPaletteEntries` for a palette index the font does not have | 0x25240 / 0x1cf40 (result pre-set to -0x7767aff4 until an enumerator exists), 0xdcbe0 | high | `nocolor_*`, `palette_*` |
| REMOTEFONT | 0x8898500D | a font-file stream returning `DWRITE_E_REMOTEFONT` from `GetFileSize` / `ReadFileFragment` | `FontFileReference::ThrowIfFailed` 0x8d2d0 (`RemoteFontException`), `DWriteFontFace::Create` 0x693c8 | high for pass-through | `callback_stream_*_remote_font` |
| DOWNLOADCANCELLED | 0x8898500E | not reproducible without a remote font source | `FontDownloadQueue` 0x40768 | - | documented only |
| DOWNLOADFAILED | 0x8898500F | not reproducible | 0xec8f4 | - | documented only |
| TOOMANYDOWNLOADS | 0x88985010 | not reproducible | 0xea404.. | - | documented only |

## Generic codes

| Code | Where it is produced | Corpus coverage |
|---|---|---|
| `E_INVALIDARG` 0x80070057 | `ArgumentException` prologues: NULL pointers, enum ranges (`TextAlignment` 0..3, `ParagraphAlignment` 0..2, `WordWrapping` 0..4, `ReadingDirection` 0..3, `FlowDirection` 0..3, `FontWeight` 1..999, `FontStyle` 0..2, `FontStretch` 1..9, `OpticalAlignment`/`VerticalGlyphOrientation` 0..1, trimming granularity 0..2 and delimiter <= 0x10FFFF, line spacing method 0..2, face type 0/1/2/6/7, simulations 0..3, rendering mode 0..6 (7 with `RENDERING_MODE1`), pixel geometry 0..2, grid fit 0..2, glyph orientation angle 0..3, baseline 0..8, texture type 0..1), float predicates (size > 0, tab stop > 0, widths >= 0, no NaN), range overflow `start + length < start`, count/pointer pairs | `args_factory.c`, `args_text.c`, `args_face.c`, `args_sets.c`, `loaders.c`, `files.c` |
| 0x8007007A | `StringCchCopy` failure and `ArgumentInsufficientBufferException`: `GetFontFamilyName`, `GetLocaleName`, `GetString`, `GetLocaleName` (strings), `GetClusterMetrics`, `GetLineMetrics`, `HitTestTextRange`, `GetUnicodeRanges`, `GetFontAxisValues`, `GetFilteredFontIndices`, `GetFontSourceName`, `GetGlyphs` (maxGlyphCount too small), `GetTypographicFeatures`, `GetJustifiedGlyphs`. Size 0 is answered before the destination is touched; a too-small copy is truncated and NUL terminated | `args_text.c` (`*_buffer_protocol`), `args_face.c`, `args_sets.c` |
| `E_FAIL` 0x80004005 | `IndexOutOfRangeException`: `IDWriteLocalizedStrings` index accessors, `GetFontFamily`, `IDWriteFontFamily::GetFont`; any other C++ exception | `strings_*`, `collection_family_index_out_of_range`, `family_font_index_out_of_range` |
| `HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)` etc. | never returned for local font files: `MapFileOpenError` turns these into DWRITE_E_FILENOTFOUND / FILEACCESS | `codes_win32_wrapped_codes` (value table only) |
| application HRESULTs | pass through unchanged from loader, stream, enumerator, analysis source/sink and renderer pixel-snapping callbacks | `callback_*`, `analyzer_*_failure_passes_through`, `draw_pixel_snapping_callback_failure_passes_through` |

## Behaviours worth reproducing that are easy to miss

* Out parameters are written (NULL / 0 / identity / sentinel) before the argument check, so a rejected call leaves them cleared.
* `DWriteCreateFactory` writes `*ppFactory = NULL` first; a NULL out pointer is fatal and is never passed by the corpus.
* A rejected setter never changes the stored value.
* `CreateTextLayout`, `CreateTextFormat` etc. accept unknown families and locales; only the documented range and NULL checks fail.
* `IDWriteFontFile` references are lazy: with an explicit FILETIME nothing is opened until the face is created.
* `TextFormat::SetAutomaticFontAxes` stores any mask; `TextLayout::SetAutomaticFontAxes` accepts only bit 0.
* Range arguments on layout setters are not clipped or rejected when they extend past the text; only `start + length` wrapping past 2^32 is an error.
* A failing `GetCurrentFontFile` in a collection enumerator is skipped silently; a failing `MoveNext` or `CreateEnumeratorFromKey` aborts `CreateCustomFontCollection` with the callback's HRESULT.
