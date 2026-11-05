# dwrite interface-method coverage

Total: 429/469 methods covered (91.5%)

| Interface | Covered | Total | Missing |
|---|---:|---:|---|
| IDWriteFontFileStream | 4 | 4 |  |
| IDWriteFontFileLoader | 1 | 1 |  |
| IDWriteLocalFontFileLoader | 3 | 3 |  |
| IDWriteFontFile | 3 | 3 |  |
| IDWriteFontFileEnumerator | 0 | 2 | MoveNext, GetCurrentFontFile |
| IDWriteFontCollectionLoader | 0 | 1 | CreateEnumeratorFromKey |
| IDWriteLocalizedStrings | 6 | 6 |  |
| IDWriteRenderingParams | 5 | 5 |  |
| IDWriteFontFace | 15 | 15 |  |
| IDWriteFont | 11 | 11 |  |
| IDWriteFontList | 3 | 3 |  |
| IDWriteFontFamily | 3 | 3 |  |
| IDWriteFontCollection | 4 | 4 |  |
| IDWritePixelSnapping | 3 | 3 |  |
| IDWriteTextRenderer | 4 | 4 |  |
| IDWriteInlineObject | 4 | 4 |  |
| IDWriteTextFormat | 25 | 25 |  |
| IDWriteTypography | 3 | 3 |  |
| IDWriteBitmapRenderTarget | 8 | 8 |  |
| IDWriteGdiInterop | 5 | 5 |  |
| IDWriteTextLayout | 39 | 39 |  |
| IDWriteTextAnalysisSource | 1 | 5 | GetTextAtPosition, GetTextBeforePosition, GetParagraphReadingDirection, GetNumberSubstitution |
| IDWriteTextAnalysisSink | 0 | 4 | SetScriptAnalysis, SetLineBreakpoints, SetBidiLevel, SetNumberSubstitution |
| IDWriteTextAnalyzer | 7 | 7 |  |
| IDWriteGlyphRunAnalysis | 3 | 3 |  |
| IDWriteFactory | 21 | 21 |  |
| IDWriteFactory1 | 2 | 2 |  |
| IDWriteFontFace1 | 12 | 12 |  |
| IDWriteFont1 | 4 | 4 |  |
| IDWriteRenderingParams1 | 1 | 1 |  |
| IDWriteTextAnalyzer1 | 9 | 9 |  |
| IDWriteTextAnalysisSource1 | 0 | 1 | GetVerticalGlyphOrientation |
| IDWriteTextAnalysisSink1 | 0 | 1 | SetGlyphOrientation |
| IDWriteTextLayout1 | 4 | 4 |  |
| IDWriteBitmapRenderTarget1 | 2 | 2 |  |
| IDWriteTextRenderer1 | 4 | 4 |  |
| IDWriteFontFallback | 1 | 1 |  |
| IDWriteTextFormat1 | 8 | 8 |  |
| IDWriteTextLayout2 | 9 | 9 |  |
| IDWriteTextAnalyzer2 | 3 | 3 |  |
| IDWriteFontFallbackBuilder | 3 | 3 |  |
| IDWriteFont2 | 1 | 1 |  |
| IDWriteFontFace2 | 5 | 5 |  |
| IDWriteColorGlyphRunEnumerator | 2 | 2 |  |
| IDWriteRenderingParams2 | 1 | 1 |  |
| IDWriteFactory2 | 5 | 5 |  |
| IDWriteFontDownloadListener | 1 | 1 |  |
| IDWriteFontDownloadQueue | 6 | 6 |  |
| IDWriteRenderingParams3 | 1 | 1 |  |
| IDWriteStringList | 5 | 5 |  |
| IDWriteFontSet | 9 | 10 | GetPropertyValues__ |
| IDWriteFontResource | 12 | 12 |  |
| IDWriteFontSet1 | 12 | 13 | GetFilteredFonts__ |
| IDWriteFont3 | 5 | 5 |  |
| IDWriteFontFamily1 | 3 | 3 |  |
| IDWriteFontFamily2 | 2 | 2 |  |
| IDWriteFontCollection1 | 2 | 2 |  |
| IDWriteFontCollection2 | 4 | 4 |  |
| IDWriteFontCollection3 | 1 | 1 |  |
| IDWriteFontFaceReference | 14 | 14 |  |
| IDWriteFontFaceReference1 | 3 | 3 |  |
| IDWriteFontList1 | 3 | 3 |  |
| IDWriteFontList2 | 1 | 1 |  |
| IDWriteFontSet2 | 1 | 1 |  |
| IDWriteFontSet3 | 3 | 3 |  |
| IDWriteFontSet4 | 0 | 2 | ConvertWeightStretchStyleToFontAxisValues, GetMatchingFonts |
| IDWriteFontFace3 | 14 | 14 |  |
| IDWriteTextFormat2 | 2 | 2 |  |
| IDWriteTextFormat3 | 5 | 5 |  |
| IDWriteTextLayout3 | 4 | 4 |  |
| IDWriteTextLayout4 | 5 | 5 |  |
| IDWriteFontFallback1 | 1 | 1 |  |
| IDWriteGdiInterop1 | 4 | 4 |  |
| IDWriteFontSetBuilder | 4 | 4 |  |
| IDWriteFontSetBuilder1 | 1 | 1 |  |
| IDWriteFontSetBuilder2 | 2 | 2 |  |
| IDWriteFactory3 | 9 | 9 |  |
| IDWriteFontFace4 | 4 | 4 |  |
| IDWriteFontFace5 | 5 | 5 |  |
| IDWriteFontFace6 | 2 | 2 |  |
| IDWritePaintReader | 0 | 9 | SetCurrentGlyph, SetTextColor, SetColorPaletteIndex, SetCustomColorPalette, MoveToFirstChild, MoveToNextSibling, MoveToParent, GetGradientStops, GetGradientStopColors |
| IDWriteFontFace7 | 0 | 2 | GetPaintFeatureLevel, CreatePaintReader |
| IDWriteColorGlyphRunEnumerator1 | 1 | 1 |  |
| IDWriteFactory4 | 3 | 3 |  |
| IDWriteAsyncResult | 0 | 2 | GetWaitHandle, GetResult |
| IDWriteRemoteFontFileStream | 0 | 4 | GetLocalFileSize, GetFileFragmentLocality, GetLocality, BeginDownload |
| IDWriteRemoteFontFileLoader | 1 | 3 | CreateRemoteStreamFromKey, GetLocalityFromKey |
| IDWriteInMemoryFontFileLoader | 2 | 2 |  |
| IDWriteFactory5 | 5 | 5 |  |
| IDWriteFactory6 | 7 | 7 |  |
| IDWriteFactory7 | 2 | 2 |  |
| IDWriteFactory8 | 1 | 1 |  |
| IDWriteBitmapRenderTarget2 | 0 | 1 | GetBitmapData |
| IDWriteBitmapRenderTarget3 | 0 | 3 | GetPaintFeatureLevel, DrawPaintGlyphRun, DrawGlyphRunWithColorSupport |

## Interface status (19041) and callback interfaces

Interfaces marked callback are implemented by the test; their methods are driven by dwrite and observed through the common/dwx doubles, so zero macro calls is expected.
not-in-19041 interfaces are not reachable on build 19041 (QueryInterface must fail).

- IDWriteFontFileStream: callback
- IDWriteFontFileLoader: callback
- IDWriteFontFileEnumerator: callback
- IDWriteFontCollectionLoader: callback
- IDWritePixelSnapping: callback
- IDWriteTextRenderer: callback
- IDWriteTextAnalysisSource: callback
- IDWriteTextAnalysisSink: callback
- IDWriteTextAnalysisSource1: callback
- IDWriteTextAnalysisSink1: callback
- IDWriteTextRenderer1: callback
- IDWriteFontDownloadListener: callback
- IDWriteFontFamily2: not-in-19041
- IDWriteFontSet3: not-in-19041
- IDWriteFontSet4: not-in-19041
- IDWriteTextLayout4: not-in-19041
- IDWriteFontFace6: not-in-19041
- IDWritePaintReader: not-in-19041
- IDWriteFontFace7: not-in-19041
- IDWriteColorGlyphRunEnumerator1: not-in-19041
- IDWriteFactory8: not-in-19041
- IDWriteBitmapRenderTarget2: not-in-19041
- IDWriteBitmapRenderTarget3: not-in-19041

## Thin (1-3 macro call sites)

- IDWriteFontFileStream::GetLastWriteTime (3)
- IDWriteInlineObject::GetOverhangMetrics (3)
- IDWriteInlineObject::GetBreakConditions (3)
- IDWriteTextAnalyzer::GetGdiCompatibleGlyphPlacements (3)
- IDWriteFont1::GetPanose (3)
- IDWriteFont1::IsMonospacedFont (3)
- IDWriteTextAnalyzer1::GetJustificationOpportunities (3)
- IDWriteTextLayout2::SetLastLineWrapping (3)
- IDWriteFont2::IsColorFont (2)
- IDWriteFontDownloadListener::DownloadCompleted (1)
- IDWriteFontDownloadQueue::CancelDownload (1)
- IDWriteFontResource::GetAxisNames (2)
- IDWriteFontResource::GetAxisValueNameCount (2)
- IDWriteFontResource::GetAxisValueNames (2)
- IDWriteFontSet1::GetFontAxisRanges_ (3)
- IDWriteFontSet1::GetFontAxisRanges (3)
- IDWriteFontSet1::GetFontLocality (1)
- IDWriteFontCollection3::GetExpirationEvent (3)
- IDWriteFontList1::GetFontLocality (3)
- IDWriteFontList2::GetFontSet (2)
- IDWriteFontSet2::GetExpirationEvent (1)
- IDWriteFontSet3::GetFontSourceType (1)
- IDWriteFontSet3::GetFontSourceNameLength (3)
- IDWriteFontFace3::IsGlyphLocal (3)
- IDWriteFontFallback1::MapCharacters (2)
- IDWriteGdiInterop1::GetFontSignature_ (3)
- IDWriteFactory4::ComputeGlyphOrigins_ (1)
- IDWriteFactory4::ComputeGlyphOrigins (1)
- IDWriteRemoteFontFileLoader::CreateFontFileReferenceFromUrl (3)
- IDWriteFactory6::CreateFontSetBuilder (2)
- IDWriteFactory7::GetSystemFontSet (1)
- IDWriteFactory7::GetSystemFontCollection (3)
