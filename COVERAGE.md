# dwrite interface-method coverage

Total: 20/469 methods covered (4.3%)

| Interface | Covered | Total | Missing |
|---|---:|---:|---|
| IDWriteFontFileStream | 0 | 4 | ReadFileFragment, ReleaseFileFragment, GetFileSize, GetLastWriteTime |
| IDWriteFontFileLoader | 0 | 1 | CreateStreamFromKey |
| IDWriteLocalFontFileLoader | 0 | 3 | GetFilePathLengthFromKey, GetFilePathFromKey, GetLastWriteTimeFromKey |
| IDWriteFontFile | 1 | 3 | GetReferenceKey, GetLoader |
| IDWriteFontFileEnumerator | 0 | 2 | MoveNext, GetCurrentFontFile |
| IDWriteFontCollectionLoader | 0 | 1 | CreateEnumeratorFromKey |
| IDWriteLocalizedStrings | 0 | 6 | GetCount, FindLocaleName, GetLocaleNameLength, GetLocaleName, GetStringLength, GetString |
| IDWriteRenderingParams | 0 | 5 | GetGamma, GetEnhancedContrast, GetClearTypeLevel, GetPixelGeometry, GetRenderingMode |
| IDWriteFontFace | 5 | 15 | GetType, GetFiles, GetIndex, GetSimulations, IsSymbolFont, TryGetFontTable, ReleaseFontTable, GetRecommendedRenderingMode, GetGdiCompatibleMetrics, GetGdiCompatibleGlyphMetrics |
| IDWriteFont | 0 | 11 | GetFontFamily, GetWeight, GetStretch, GetStyle, IsSymbolFont, GetFaceNames, GetInformationalStrings, GetSimulations, GetMetrics, HasCharacter, CreateFontFace |
| IDWriteFontList | 0 | 3 | GetFontCollection, GetFontCount, GetFont |
| IDWriteFontFamily | 0 | 3 | GetFamilyNames, GetFirstMatchingFont, GetMatchingFonts |
| IDWriteFontCollection | 2 | 4 | GetFontFamily, GetFontFromFontFace |
| IDWritePixelSnapping | 0 | 3 | IsPixelSnappingDisabled, GetCurrentTransform, GetPixelsPerDip |
| IDWriteTextRenderer | 0 | 4 | DrawGlyphRun, DrawUnderline, DrawStrikethrough, DrawInlineObject |
| IDWriteInlineObject | 3 | 4 | Draw |
| IDWriteTextFormat | 0 | 25 | SetTextAlignment, SetParagraphAlignment, SetWordWrapping, SetReadingDirection, SetFlowDirection, SetIncrementalTabStop, SetTrimming, SetLineSpacing, GetTextAlignment, GetParagraphAlignment, GetWordWrapping, GetReadingDirection, GetFlowDirection, GetIncrementalTabStop, GetTrimming, GetLineSpacing, GetFontCollection, GetFontFamilyNameLength, GetFontFamilyName, GetFontWeight, GetFontStyle, GetFontStretch, GetFontSize, GetLocaleNameLength, GetLocaleName |
| IDWriteTypography | 0 | 3 | AddFontFeature, GetFontFeatureCount, GetFontFeature |
| IDWriteBitmapRenderTarget | 0 | 8 | DrawGlyphRun, GetMemoryDC, GetPixelsPerDip, SetPixelsPerDip, GetCurrentTransform, SetCurrentTransform, GetSize, Resize |
| IDWriteGdiInterop | 0 | 5 | CreateFontFromLOGFONT, ConvertFontToLOGFONT, ConvertFontFaceToLOGFONT, CreateFontFaceFromHdc, CreateBitmapRenderTarget |
| IDWriteTextLayout | 2 | 39 | SetMaxWidth, SetMaxHeight, SetFontCollection, SetFontFamilyName, SetFontWeight, SetFontStyle, SetFontStretch, SetFontSize, SetUnderline, SetStrikethrough, SetInlineObject, SetTypography, SetLocaleName, GetMaxWidth, GetMaxHeight, GetFontCollection, GetFontFamilyNameLength, GetFontFamilyName, GetFontWeight, GetFontStyle, GetFontStretch, GetFontSize, GetUnderline, GetStrikethrough, GetDrawingEffect, GetInlineObject, GetTypography, GetLocaleNameLength, GetLocaleName, GetLineMetrics, GetMetrics, GetOverhangMetrics, GetClusterMetrics, DetermineMinWidth, HitTestPoint, HitTestTextPosition, HitTestTextRange |
| IDWriteTextAnalysisSource | 0 | 5 | GetTextAtPosition, GetTextBeforePosition, GetParagraphReadingDirection, GetLocaleName, GetNumberSubstitution |
| IDWriteTextAnalysisSink | 0 | 4 | SetScriptAnalysis, SetLineBreakpoints, SetBidiLevel, SetNumberSubstitution |
| IDWriteTextAnalyzer | 3 | 7 | AnalyzeNumberSubstitution, GetGlyphs, GetGlyphPlacements, GetGdiCompatibleGlyphPlacements |
| IDWriteGlyphRunAnalysis | 0 | 3 | GetAlphaTextureBounds, CreateAlphaTexture, GetAlphaBlendParams |
| IDWriteFactory | 3 | 21 | GetSystemFontCollection, CreateCustomFontCollection, RegisterFontCollectionLoader, UnregisterFontCollectionLoader, CreateFontFileReference, CreateCustomFontFileReference, CreateFontFace, CreateRenderingParams, CreateMonitorRenderingParams, CreateCustomRenderingParams, RegisterFontFileLoader, UnregisterFontFileLoader, CreateTypography, GetGdiInterop, CreateGdiCompatibleTextLayout, CreateEllipsisTrimmingSign, CreateNumberSubstitution, CreateGlyphRunAnalysis |
| IDWriteFactory1 | 0 | 2 | GetEudcFontCollection, CreateCustomRenderingParams |
| IDWriteFontFace1 | 0 | 12 | GetMetrics, GetGdiCompatibleMetrics, GetCaretMetrics, GetUnicodeRanges, IsMonospacedFont, GetDesignGlyphAdvances, GetGdiCompatibleGlyphAdvances, GetKerningPairAdjustments, HasKerningPairs, GetRecommendedRenderingMode, GetVerticalGlyphVariants, HasVerticalGlyphVariants |
| IDWriteFont1 | 0 | 4 | GetMetrics, GetPanose, GetUnicodeRanges, IsMonospacedFont |
| IDWriteRenderingParams1 | 0 | 1 | GetGrayscaleEnhancedContrast |
| IDWriteTextAnalyzer1 | 0 | 9 | ApplyCharacterSpacing, GetBaseline, AnalyzeVerticalGlyphOrientation, GetGlyphOrientationTransform, GetScriptProperties, GetTextComplexity, GetJustificationOpportunities, JustifyGlyphAdvances, GetJustifiedGlyphs |
| IDWriteTextAnalysisSource1 | 0 | 1 | GetVerticalGlyphOrientation |
| IDWriteTextAnalysisSink1 | 0 | 1 | SetGlyphOrientation |
| IDWriteTextLayout1 | 0 | 4 | SetPairKerning, GetPairKerning, SetCharacterSpacing, GetCharacterSpacing |
| IDWriteBitmapRenderTarget1 | 0 | 2 | GetTextAntialiasMode, SetTextAntialiasMode |
| IDWriteTextRenderer1 | 0 | 4 | DrawGlyphRun, DrawUnderline, DrawStrikethrough, DrawInlineObject |
| IDWriteFontFallback | 0 | 1 | MapCharacters |
| IDWriteTextFormat1 | 0 | 8 | SetVerticalGlyphOrientation, GetVerticalGlyphOrientation, SetLastLineWrapping, GetLastLineWrapping, SetOpticalAlignment, GetOpticalAlignment, SetFontFallback, GetFontFallback |
| IDWriteTextLayout2 | 0 | 9 | GetMetrics, SetVerticalGlyphOrientation, GetVerticalGlyphOrientation, SetLastLineWrapping, GetLastLineWrapping, SetOpticalAlignment, GetOpticalAlignment, SetFontFallback, GetFontFallback |
| IDWriteTextAnalyzer2 | 0 | 3 | GetGlyphOrientationTransform, GetTypographicFeatures, CheckTypographicFeature |
| IDWriteFontFallbackBuilder | 0 | 3 | AddMapping, AddMappings, CreateFontFallback |
| IDWriteFont2 | 0 | 1 | IsColorFont |
| IDWriteFontFace2 | 0 | 5 | IsColorFont, GetColorPaletteCount, GetPaletteEntryCount, GetPaletteEntries, GetRecommendedRenderingMode |
| IDWriteColorGlyphRunEnumerator | 0 | 2 | MoveNext, GetCurrentRun |
| IDWriteRenderingParams2 | 0 | 1 | GetGridFitMode |
| IDWriteFactory2 | 0 | 5 | GetSystemFontFallback, CreateFontFallbackBuilder, TranslateColorGlyphRun, CreateCustomRenderingParams, CreateGlyphRunAnalysis |
| IDWriteFontDownloadListener | 1 | 1 |  |
| IDWriteFontDownloadQueue | 0 | 6 | AddListener, RemoveListener, IsEmpty, BeginDownload, CancelDownload, GetGenerationCount |
| IDWriteRenderingParams3 | 0 | 1 | GetRenderingMode1 |
| IDWriteStringList | 0 | 5 | GetCount, GetLocaleNameLength, GetLocaleName, GetStringLength, GetString |
| IDWriteFontSet | 0 | 10 | GetFontCount, GetFontFaceReference, FindFontFaceReference, FindFontFace, GetPropertyValues__, GetPropertyValues_, GetPropertyValues, GetPropertyOccurrenceCount, GetMatchingFonts_, GetMatchingFonts |
| IDWriteFontResource | 0 | 12 | GetFontFile, GetFontFaceIndex, GetFontAxisCount, GetDefaultFontAxisValues, GetFontAxisRanges, GetFontAxisAttributes, GetAxisNames, GetAxisValueNameCount, GetAxisValueNames, HasVariations, CreateFontFace, CreateFontFaceReference |
| IDWriteFontSet1 | 0 | 13 | GetMatchingFonts, GetFirstFontResources, GetFilteredFonts__, GetFilteredFonts_, GetFilteredFonts, GetFilteredFontIndices_, GetFilteredFontIndices, GetFontAxisRanges_, GetFontAxisRanges, GetFontFaceReference, CreateFontResource, CreateFontFace, GetFontLocality |
| IDWriteFont3 | 0 | 5 | CreateFontFace, Equals, GetFontFaceReference, HasCharacter, GetLocality |
| IDWriteFontFamily1 | 0 | 3 | GetFontLocality, GetFont, GetFontFaceReference |
| IDWriteFontFamily2 | 0 | 2 | GetMatchingFonts, GetFontSet |
| IDWriteFontCollection1 | 0 | 2 | GetFontSet, GetFontFamily |
| IDWriteFontCollection2 | 0 | 4 | GetFontFamily, GetMatchingFonts, GetFontFamilyModel, GetFontSet |
| IDWriteFontCollection3 | 0 | 1 | GetExpirationEvent |
| IDWriteFontFaceReference | 0 | 14 | CreateFontFace, CreateFontFaceWithSimulations, Equals, GetFontFaceIndex, GetSimulations, GetFontFile, GetLocalFileSize, GetFileSize, GetFileTime, GetLocality, EnqueueFontDownloadRequest, EnqueueCharacterDownloadRequest, EnqueueGlyphDownloadRequest, EnqueueFileFragmentDownloadRequest |
| IDWriteFontFaceReference1 | 0 | 3 | CreateFontFace, GetFontAxisValueCount, GetFontAxisValues |
| IDWriteFontList1 | 0 | 3 | GetFontLocality, GetFont, GetFontFaceReference |
| IDWriteFontList2 | 0 | 1 | GetFontSet |
| IDWriteFontSet2 | 0 | 1 | GetExpirationEvent |
| IDWriteFontSet3 | 0 | 3 | GetFontSourceType, GetFontSourceNameLength, GetFontSourceName |
| IDWriteFontSet4 | 0 | 2 | ConvertWeightStretchStyleToFontAxisValues, GetMatchingFonts |
| IDWriteFontFace3 | 0 | 14 | GetFontFaceReference, GetPanose, GetWeight, GetStretch, GetStyle, GetFamilyNames, GetFaceNames, GetInformationalStrings, HasCharacter, GetRecommendedRenderingMode, IsCharacterLocal, IsGlyphLocal, AreCharactersLocal, AreGlyphsLocal |
| IDWriteTextFormat2 | 0 | 2 | SetLineSpacing, GetLineSpacing |
| IDWriteTextFormat3 | 0 | 5 | SetFontAxisValues, GetFontAxisValueCount, GetFontAxisValues, GetAutomaticFontAxes, SetAutomaticFontAxes |
| IDWriteTextLayout3 | 0 | 4 | InvalidateLayout, SetLineSpacing, GetLineSpacing, GetLineMetrics |
| IDWriteTextLayout4 | 0 | 5 | SetFontAxisValues, GetFontAxisValueCount, GetFontAxisValues, GetAutomaticFontAxes, SetAutomaticFontAxes |
| IDWriteFontFallback1 | 0 | 1 | MapCharacters |
| IDWriteGdiInterop1 | 0 | 4 | CreateFontFromLOGFONT, GetFontSignature_, GetFontSignature, GetMatchingFontsByLOGFONT |
| IDWriteFontSetBuilder | 0 | 4 | AddFontFaceReference_, AddFontFaceReference, AddFontSet, CreateFontSet |
| IDWriteFontSetBuilder1 | 0 | 1 | AddFontFile |
| IDWriteFontSetBuilder2 | 0 | 2 | AddFont, AddFontFile |
| IDWriteFactory3 | 0 | 9 | CreateGlyphRunAnalysis, CreateCustomRenderingParams, CreateFontFaceReference_, CreateFontFaceReference, GetSystemFontSet, CreateFontSetBuilder, CreateFontCollectionFromFontSet, GetSystemFontCollection, GetFontDownloadQueue |
| IDWriteFontFace4 | 0 | 4 | GetGlyphImageFormats_, GetGlyphImageFormats, GetGlyphImageData, ReleaseGlyphImageData |
| IDWriteFontFace5 | 0 | 5 | GetFontAxisValueCount, GetFontAxisValues, HasVariations, GetFontResource, Equals |
| IDWriteFontFace6 | 0 | 2 | GetFamilyNames, GetFaceNames |
| IDWritePaintReader | 0 | 9 | SetCurrentGlyph, SetTextColor, SetColorPaletteIndex, SetCustomColorPalette, MoveToFirstChild, MoveToNextSibling, MoveToParent, GetGradientStops, GetGradientStopColors |
| IDWriteFontFace7 | 0 | 2 | GetPaintFeatureLevel, CreatePaintReader |
| IDWriteColorGlyphRunEnumerator1 | 0 | 1 | GetCurrentRun |
| IDWriteFactory4 | 0 | 3 | TranslateColorGlyphRun, ComputeGlyphOrigins_, ComputeGlyphOrigins |
| IDWriteAsyncResult | 0 | 2 | GetWaitHandle, GetResult |
| IDWriteRemoteFontFileStream | 0 | 4 | GetLocalFileSize, GetFileFragmentLocality, GetLocality, BeginDownload |
| IDWriteRemoteFontFileLoader | 0 | 3 | CreateRemoteStreamFromKey, GetLocalityFromKey, CreateFontFileReferenceFromUrl |
| IDWriteInMemoryFontFileLoader | 0 | 2 | CreateInMemoryFontFileReference, GetFileCount |
| IDWriteFactory5 | 0 | 5 | CreateFontSetBuilder, CreateInMemoryFontFileLoader, CreateHttpFontFileLoader, AnalyzeContainerType, UnpackFontFile |
| IDWriteFactory6 | 0 | 7 | CreateFontFaceReference, CreateFontResource, GetSystemFontSet, GetSystemFontCollection, CreateFontCollectionFromFontSet, CreateFontSetBuilder, CreateTextFormat |
| IDWriteFactory7 | 0 | 2 | GetSystemFontSet, GetSystemFontCollection |
| IDWriteFactory8 | 0 | 1 | TranslateColorGlyphRun |
| IDWriteBitmapRenderTarget2 | 0 | 1 | GetBitmapData |
| IDWriteBitmapRenderTarget3 | 0 | 3 | GetPaintFeatureLevel, DrawPaintGlyphRun, DrawGlyphRunWithColorSupport |
