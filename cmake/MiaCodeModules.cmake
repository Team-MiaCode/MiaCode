# Layered MiaCode libraries (docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md).
#
# Every library is static, exports the src root so headers are included by
# their src-rooted path, and links only the layers it may depend on.
# scripts/governance/module_layering.py checks the source lists and the
# miacode_* link edges below against the layering table; keep them explicit.
include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/MiaCodeModuleHelpers.cmake)

# ---- base: logging, diagnostics, cancellation, preference port -------------
miacode_add_module(miacode_base
    SOURCES
        src/common/CrashRecovery.cpp
        src/common/CrashRecovery.h
        src/common/DebugLog.cpp
        src/common/DebugLog.h
        src/common/DebugOptions.h
        src/common/FileContentStamp.h
        src/common/GpuDevicePolicy.cpp
        src/common/GpuDevicePolicy.h
        src/common/InputShortcutGesture.cpp
        src/common/InputShortcutGesture.h
        src/common/LocalizedText.h
        src/common/LogEmissionPolicy.h
        src/common/Mmcss.cpp
        src/common/Mmcss.h
        src/common/OperationLog.cpp
        src/common/OperationLog.h
        src/common/PreferenceProvider.cpp
        src/common/PreferenceProvider.h
        src/common/ProcessDiagnostics.cpp
        src/common/ProcessDiagnostics.h
        src/common/TaskCancellation.h
        src/common/UiHangWatchdog.cpp
        src/common/UiHangWatchdog.h
        src/common/UiHangWatchdogPolicy.h
    PUBLIC Qt6::Core Qt6::Gui
)
if (WIN32)
    target_link_libraries(miacode_base PRIVATE
        dxgi         # GpuDevicePolicy adapter enumeration, ProcessDiagnostics VRAM gauge
        avrt         # Mmcss: AvSetMmThreadCharacteristicsW
    )
endif()

# ---- chart: document, parser, transforms, note model ----------------------
miacode_add_module(miacode_chart
    SOURCES
        src/core/chart/ChartAssetPaths.h
        src/core/chart/IntroConfig.h
        src/core/chart/SlideReferenceData.cpp
        src/core/chart/SlideReferenceData.h
        src/core/chart/document/ChartClockCount.h
        src/core/chart/document/SimaiDocument.cpp
        src/core/chart/document/SimaiDocument.h
        src/core/chart/document/SimaiTimingMetadata.cpp
        src/core/chart/document/SimaiTimingMetadata.h
        src/core/chart/model/TimelineData.h
        src/core/chart/model/TimelineMarkerOffset.h
        src/core/chart/parser/SimaiCommentScan.cpp
        src/core/chart/parser/SimaiCommentScan.h
        src/core/chart/parser/SimaiParser.Driver.cpp
        src/core/chart/parser/SimaiParser.Slide.cpp
        src/core/chart/parser/SimaiParser.StrictChecks.cpp
        src/core/chart/parser/SimaiParser.TouchTap.cpp
        src/core/chart/parser/SimaiParser.cpp
        src/core/chart/parser/SimaiParser.h
        src/core/chart/selection/ChartSelectionBeatSummary.cpp
        src/core/chart/selection/ChartSelectionBeatSummary.h
        src/core/chart/transform/ChartBatchTransform.Internal.h
        src/core/chart/transform/ChartBatchTransform.Parsers.cpp
        src/core/chart/transform/ChartBatchTransform.Selection.cpp
        src/core/chart/transform/ChartBatchTransform.Subdivision.cpp
        src/core/chart/transform/ChartBatchTransform.Transform.cpp
        src/core/chart/transform/ChartBatchTransform.h
        src/core/chart/transform/ChartNormalization.cpp
        src/core/chart/transform/ChartNormalization.h
        src/core/chart/transform/ChartNormalizationSegmentPolicy.cpp
        src/core/chart/transform/ChartNormalizationSegmentPolicy.h
        src/core/chart/transform/Non384SnapTable.cpp
        src/core/chart/transform/Non384SnapTable.h
    PUBLIC miacode_base
    QRC resources/slide_data.qrc
)
# SimaiParser.cpp includes its partial translation units.
set_source_files_properties(
    src/core/chart/parser/SimaiParser.Driver.cpp
    src/core/chart/parser/SimaiParser.Slide.cpp
    src/core/chart/parser/SimaiParser.StrictChecks.cpp
    src/core/chart/parser/SimaiParser.TouchTap.cpp
    PROPERTIES HEADER_FILE_ONLY ON)

# ---- analysis: Muri and the analysis pipeline -----------------------------
miacode_add_module(miacode_analysis
    SOURCES
        src/core/analysis/MuriAnalyzer.cpp
        src/core/analysis/MuriAnalyzer.h
        src/core/analysis/MuriAnalyzerGeometry.cpp
        src/core/analysis/MuriAnalyzerGeometry.h
        src/core/analysis/MuriAnalyzerInternal.h
        src/core/analysis/MuriAnalyzerModel.h
        src/core/analysis/MuriConfig.h
        src/core/analysis/MuriDiagnosticCollector.cpp
        src/core/analysis/MuriDiagnosticCollector.h
        src/core/analysis/MuriDiagnosticLabels.cpp
        src/core/analysis/MuriDiagnosticLabels.h
        src/core/analysis/MuriOverlayBuilder.cpp
        src/core/analysis/MuriOverlayBuilder.h
        src/core/analysis/MuriPanelEntries.cpp
        src/core/analysis/MuriPanelEntries.h
        src/core/analysis/MuriRenderOptions.h
        src/core/analysis/MuriRuntimeModelBuilder.cpp
        src/core/analysis/MuriRuntimeModelBuilder.h
        src/core/analysis/MuriSimpleNoteJudge.cpp
        src/core/analysis/MuriSimpleNoteJudge.h
        src/core/analysis/MuriSlideReferenceData.cpp
        src/core/analysis/MuriSlideReferenceData.h
        src/core/analysis/MuriSlideWifiJudge.cpp
        src/core/analysis/MuriSlideWifiJudge.h
        src/core/analysis/MuriStaticChecker.cpp
        src/core/analysis/MuriStaticChecker.h
        src/core/analysis/MuriTypes.cpp
        src/core/analysis/MuriTypes.h
        src/core/analysis/TimelineSlowRefresh.cpp
        src/core/analysis/TimelineSlowRefresh.h
    PUBLIC miacode_chart
)

if(MIACODE_BUILD_APP)
# ---- editor_core: text policy, completion, bookmark syntax ---------------
miacode_add_module(miacode_editor_core
    SOURCES
        src/editor/BookmarkCommentSyntax.cpp
        src/editor/BookmarkCommentSyntax.h
        src/editor/SimaiCompletionCatalog.cpp
        src/editor/SimaiCompletionCatalog.h
        src/editor/SimaiTextEditPolicy.cpp
        src/editor/SimaiTextEditPolicy.h
        src/editor/TouchPadAuthoringEdit.cpp
        src/editor/TouchPadAuthoringEdit.h
    PUBLIC miacode_chart
)
endif()

# ---- scene: frame/layer math and preview configuration --------------------
miacode_add_module(miacode_scene
    SOURCES
        src/core/scene/PreviewActiveMarkerView.h
        src/core/scene/PreviewAnimatedSpriteHelpers.cpp
        src/core/scene/PreviewAnimatedSpriteHelpers.h
        src/core/scene/PreviewArcDescriptor.h
        src/core/scene/PreviewChartReviewLayerState.cpp
        src/core/scene/PreviewChartReviewLayerState.h
        src/core/scene/PreviewCircleDescriptor.h
        src/core/scene/PreviewFireworkWarmupPolicy.h
        src/core/scene/PreviewFrameState.h
        src/core/scene/PreviewGuideLayerState.cpp
        src/core/scene/PreviewGuideLayerState.h
        src/core/scene/PreviewHeadLayerState.cpp
        src/core/scene/PreviewHeadLayerState.h
        src/core/scene/PreviewHudState.cpp
        src/core/scene/PreviewHudState.h
        src/core/scene/PreviewJudgeEffectLayerState.cpp
        src/core/scene/PreviewJudgeEffectLayerState.h
        src/core/scene/PreviewJudgeFireworkLayerState.cpp
        src/core/scene/PreviewJudgeFireworkLayerState.h
        src/core/scene/PreviewJudgeOverlayShared.cpp
        src/core/scene/PreviewJudgeOverlayShared.h
        src/core/scene/PreviewLayerOrder.h
        src/core/scene/PreviewMaimuriDxJudgeLayerState.cpp
        src/core/scene/PreviewMaimuriDxJudgeLayerState.h
        src/core/scene/PreviewMarkerDrawOrder.cpp
        src/core/scene/PreviewMarkerDrawOrder.h
        src/core/scene/PreviewMuriActionLayerState.cpp
        src/core/scene/PreviewMuriActionLayerState.h
        src/core/scene/PreviewMuriPadLayerState.cpp
        src/core/scene/PreviewMuriPadLayerState.h
        src/core/scene/PreviewOpacityCurves.cpp
        src/core/scene/PreviewOpacityCurves.h
        src/core/scene/PreviewPreparedSceneCache.cpp
        src/core/scene/PreviewPreparedSceneCache.h
        src/core/scene/PreviewProgressStatsCache.cpp
        src/core/scene/PreviewProgressStatsCache.h
        src/core/scene/PreviewSceneConstants.h
        src/core/scene/PreviewSceneGeometry.cpp
        src/core/scene/PreviewSceneGeometry.h
        src/core/scene/PreviewSceneMath.cpp
        src/core/scene/PreviewSceneMath.h
        src/core/scene/PreviewSectorDescriptor.h
        src/core/scene/PreviewSfxAssets.h
        src/core/scene/PreviewSfxSemantics.h
        src/core/scene/PreviewSfxTimeline.h
        src/core/scene/PreviewSfxTiming.h
        src/core/scene/PreviewSkinSelectors.cpp
        src/core/scene/PreviewSkinSelectors.h
        src/core/scene/PreviewSlideMotionLayerState.cpp
        src/core/scene/PreviewSlideMotionLayerState.h
        src/core/scene/PreviewSpriteDescriptor.h
        src/core/scene/PreviewTouchHoldLayerState.cpp
        src/core/scene/PreviewTouchHoldLayerState.h
        src/core/scene/PreviewTouchJudgeLayerState.cpp
        src/core/scene/PreviewTouchJudgeLayerState.h
        src/core/scene/PreviewTouchLayerState.cpp
        src/core/scene/PreviewTouchLayerState.h
        src/core/scene/PreviewTrackLayerState.cpp
        src/core/scene/PreviewTrackLayerState.h
        src/core/scene/PreviewTrackShared.cpp
        src/core/scene/PreviewTrackShared.h
        src/core/scene/TouchPadAuthoringState.h
        src/core/video/AssetPaths.h
        src/core/video/LayoutRingConfig.h
        src/core/video/PreviewEndOfMediaPolicy.h
        src/core/video/PreviewGameplayConfig.h
        src/core/video/PreviewInteractionConfig.h
        src/core/video/PreviewRenderSettings.h
        src/core/video/PreviewSkinConfig.h
        src/core/video/PreviewTimingSettings.h
        src/core/video/PreviewVideoGeometryConfig.h
    PUBLIC miacode_analysis
    QRC resources/fonts.qrc
)

if(MIACODE_BUILD_APP)
# ---- audio: backend interfaces, worker, settings, waveform ----------------
miacode_add_module(miacode_audio
    SOURCES
        src/audio/AudioFileDecoder.h
        src/audio/PreviewAudioBackend.h
        src/audio/PreviewAudioCommandQueue.cpp
        src/audio/PreviewAudioCommandQueue.h
        src/audio/PreviewAudioDeviceChangePolicy.h
        src/audio/PreviewAudioDeviceCutoff.h
        src/audio/PreviewAudioDeviceWatcher.cpp
        src/audio/PreviewAudioDeviceWatcher.h
        src/audio/PreviewAudioHealth.h
        src/audio/PreviewAudioMixConfig.h
        src/audio/PreviewAudioOutputGlitchProbe.h
        src/audio/PreviewAudioOutputGlitchRing.h
        src/audio/PreviewAudioPlaybackFlowPolicy.h
        src/audio/PreviewAudioSettings.cpp
        src/audio/PreviewAudioSettings.h
        src/audio/PreviewAudioWorker.cpp
        src/audio/PreviewAudioWorker.h
        src/audio/PreviewAudioWorkerFactory.cpp
        src/audio/PreviewAudioWorkerFactory.h
        src/audio/PreviewAudioWorkerProtocol.h
        src/audio/QtPreviewSfxRuntime.cpp
        src/audio/QtPreviewSfxRuntime.h
        src/audio/WaveformCache.cpp
        src/audio/WaveformCache.h
    PUBLIC miacode_scene Qt6::Multimedia
)
if (WIN32)
    target_link_libraries(miacode_audio PRIVATE
        ole32        # PreviewAudioDeviceWatcher: Core Audio endpoint notifications
    )
endif()
endif()

if(MIACODE_BUILD_APP)
# ---- audio_bass: BASS backend and offline decoder -------------------------
miacode_add_module(miacode_audio_bass
    SOURCES
        src/audio/bass/BassFlacPlugin.h
        src/audio/bass/BassPreviewAudioBackend.cpp
        src/audio/bass/BassPreviewAudioBackend.h
        src/audio/bass/BassPreviewAudioBackendImpl.h
        src/audio/bass/BassPreviewAudioBackendSample.h
        src/audio/bass/BassPreviewAudioBackend_Assets.cpp
        src/audio/bass/BassPreviewAudioBackend_EngineInit.cpp
        src/audio/bass/BassPreviewAudioBackend_EventDrain.cpp
        src/audio/bass/BassPreviewAudioBackend_PlaybackClock.cpp
        src/audio/bass/BassPreviewAudioBackend_Transport.cpp
        src/audio/bass/BassPreviewDebugLogRouting.h
        src/audio/bass/BassPreviewMasterMixerPolicy.h
        src/audio/bass/BassPreviewOutputGlitchProbeState.h
        src/audio/bass/BassPreviewRetainedState.h
        src/audio/bass/BassPreviewSfxCallbackRing.h
        src/audio/bass/BassPreviewSfxSchedulerPolicy.h
        src/audio/bass/OfflineAudioDecoder.cpp
        src/audio/bass/OfflineAudioDecoder.h
        src/audio/bass/PreviewBassDefaultDevice.cpp
        src/audio/bass/PreviewBassDefaultDevice.h
        src/audio/bass/PreviewBassDeviceLease.cpp
        src/audio/bass/PreviewBassDeviceLease.h
        src/audio/bass/PreviewBassEmergencyPause.cpp
        src/audio/bass/PreviewBassEmergencyPause.h
    PUBLIC miacode_audio
)
target_include_directories(miacode_audio_bass PUBLIC "${PROJECT_SOURCE_DIR}/third_party/bass/include")
if (WIN32)
    target_link_libraries(miacode_audio_bass PUBLIC
        "${MIACODE_BASS_WINDOWS_LIB_DIR}/bass.lib"
        "${MIACODE_BASS_WINDOWS_LIB_DIR}/bassmix.lib"
        ole32        # Windows Core Audio endpoint notification COM API
        avrt
    )
elseif (APPLE)
    target_link_libraries(miacode_audio_bass PUBLIC
        "${MIACODE_BASS_MACOS_DIR}/libbass.dylib"
        "${MIACODE_BASS_MACOS_DIR}/libbassmix.dylib"
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(miacode_audio_bass PUBLIC
        "${MIACODE_BASS_LINUX_DIR}/libbass.so"
        "${MIACODE_BASS_LINUX_DIR}/libbassmix.so"
        ${CMAKE_DL_LIBS}   # dlopen of libbass_fx.so (BassPreviewAudioBackend_EngineInit.cpp)
    )
endif()
endif()

if(MIACODE_BUILD_APP)
# ---- timeline: timeline model ---------------------------------------------
miacode_add_module(miacode_timeline
    SOURCES
        src/timeline/TimelineAnalysisPublication.h
        src/timeline/TimelineCadenceArbitrationPolicy.h
        src/timeline/TimelineNoteAssets.cpp
        src/timeline/TimelineNoteAssets.h
        src/timeline/TimelineQuickModel.cpp
        src/timeline/TimelineQuickModel.h
        src/timeline/TimelineQuickModelIndexing.cpp
        src/timeline/TimelineQuickModelParser.cpp
        src/timeline/TimelineQuickModelPrivate.h
        src/timeline/TimelineQuickModelSnapshot.cpp
        src/timeline/TimelineRenderData.h
        src/timeline/TimelineSceneState.cpp
        src/timeline/TimelineSceneState.h
        src/timeline/TimelineSceneStateBuilder.cpp
        src/timeline/TimelineSceneStateBuilder.h
        src/timeline/TimelineThemeConfig.h
    PUBLIC miacode_analysis miacode_audio
)
endif()

if(MIACODE_BUILD_APP)
# ---- timeline_quick: QSG layers, QML module MiaCode.Timeline --------------
miacode_add_module(miacode_timeline_quick
    SOURCES
        src/timeline/quick/TimelineQuickGridLayer.cpp
        src/timeline/quick/TimelineQuickGridLayer.h
        src/timeline/quick/TimelineQuickGridLinesLayer.cpp
        src/timeline/quick/TimelineQuickGridLinesLayer.h
        src/timeline/quick/TimelineQuickHeaderLayer.cpp
        src/timeline/quick/TimelineQuickHeaderLayer.h
        src/timeline/quick/TimelineQuickItem.cpp
        src/timeline/quick/TimelineQuickItem.h
        src/timeline/quick/TimelineQuickLayerUtils.cpp
        src/timeline/quick/TimelineQuickLayerUtils.h
        src/timeline/quick/TimelineQuickNotesLayer.cpp
        src/timeline/quick/TimelineQuickNotesLayer.h
        src/timeline/quick/TimelineQuickOverlayLayer.cpp
        src/timeline/quick/TimelineQuickOverlayLayer.h
        src/timeline/quick/TimelineQuickStateBridge.cpp
        src/timeline/quick/TimelineQuickStateBridge.h
        src/timeline/quick/TimelineQuickTextureCache.cpp
        src/timeline/quick/TimelineQuickTextureCache.h
        src/timeline/quick/TimelineQuickTextureCachePolicy.h
        src/timeline/quick/TimelineQuickWaveformLayer.cpp
        src/timeline/quick/TimelineQuickWaveformLayer.h
    PUBLIC miacode_timeline Qt6::Quick Qt6::Qml
)
# qmltyperegistrar includes QML_ELEMENT headers by file name only.
target_include_directories(miacode_timeline_quick PRIVATE src/timeline/quick)
if (WIN32)
    target_link_libraries(miacode_timeline_quick PRIVATE
        d3d11
        dxgi         # TimelineQuickItem per-process VRAM gauge
    )
endif()
qt_add_qml_module(miacode_timeline_quick
    URI MiaCode.Timeline
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml_modules/MiaCode/Timeline"
)
endif()

# ---- preview_quick: QSG scene, PreviewRuntime, QML module MiaCode.Preview -
miacode_add_module(miacode_preview_quick
    SOURCES
        src/preview/quick_scene/PreviewQuickArcNodes.cpp
        src/preview/quick_scene/PreviewQuickArcNodes.h
        src/preview/quick_scene/PreviewQuickBackdropLayer.cpp
        src/preview/quick_scene/PreviewQuickBackdropLayer.h
        src/preview/quick_scene/PreviewQuickChartReviewLayer.cpp
        src/preview/quick_scene/PreviewQuickChartReviewLayer.h
        src/preview/quick_scene/PreviewQuickCircleNodes.cpp
        src/preview/quick_scene/PreviewQuickCircleNodes.h
        src/preview/quick_scene/PreviewQuickGraphicsInfo.cpp
        src/preview/quick_scene/PreviewQuickGraphicsInfo.h
        src/preview/quick_scene/PreviewQuickGuideLayer.cpp
        src/preview/quick_scene/PreviewQuickGuideLayer.h
        src/preview/quick_scene/PreviewQuickHeadLayer.cpp
        src/preview/quick_scene/PreviewQuickHeadLayer.h
        src/preview/quick_scene/PreviewQuickHudLayer.cpp
        src/preview/quick_scene/PreviewQuickHudLayer.h
        src/preview/quick_scene/PreviewQuickJudgeEffectLayer.cpp
        src/preview/quick_scene/PreviewQuickJudgeEffectLayer.h
        src/preview/quick_scene/PreviewQuickJudgeFireworkLayer.cpp
        src/preview/quick_scene/PreviewQuickJudgeFireworkLayer.h
        src/preview/quick_scene/PreviewQuickMaimuriDxJudgeLayer.cpp
        src/preview/quick_scene/PreviewQuickMaimuriDxJudgeLayer.h
        src/preview/quick_scene/PreviewQuickMuriActionLayer.cpp
        src/preview/quick_scene/PreviewQuickMuriActionLayer.h
        src/preview/quick_scene/PreviewQuickMuriPadLayer.cpp
        src/preview/quick_scene/PreviewQuickMuriPadLayer.h
        src/preview/quick_scene/PreviewQuickSceneRoot.cpp
        src/preview/quick_scene/PreviewQuickSceneRoot.h
        src/preview/quick_scene/PreviewQuickSectorNodes.cpp
        src/preview/quick_scene/PreviewQuickSectorNodes.h
        src/preview/quick_scene/PreviewQuickSlideMotionLayer.cpp
        src/preview/quick_scene/PreviewQuickSlideMotionLayer.h
        src/preview/quick_scene/PreviewQuickSpriteBatchPolicy.h
        src/preview/quick_scene/PreviewQuickSpriteNodes.cpp
        src/preview/quick_scene/PreviewQuickSpriteNodes.h
        src/preview/quick_scene/PreviewQuickStageBackgroundLayer.cpp
        src/preview/quick_scene/PreviewQuickStageBackgroundLayer.h
        src/preview/quick_scene/PreviewQuickTouchHoldLayer.cpp
        src/preview/quick_scene/PreviewQuickTouchHoldLayer.h
        src/preview/quick_scene/PreviewQuickTouchHoverLayer.cpp
        src/preview/quick_scene/PreviewQuickTouchHoverLayer.h
        src/preview/quick_scene/PreviewQuickTouchJudgeLayer.cpp
        src/preview/quick_scene/PreviewQuickTouchJudgeLayer.h
        src/preview/quick_scene/PreviewQuickTouchLayer.cpp
        src/preview/quick_scene/PreviewQuickTouchLayer.h
        src/preview/quick_scene/PreviewQuickTrackLayer.cpp
        src/preview/quick_scene/PreviewQuickTrackLayer.h
        src/preview/quick_scene/PreviewTextureGenerationPolicy.h
        src/preview/quick_scene/PreviewTextureRepository.cpp
        src/preview/quick_scene/PreviewTextureRepository.h
        src/preview/runtime/PreviewRuntime.cpp
        src/preview/runtime/PreviewRuntime.h
        src/preview/runtime/PreviewSceneAssetLoader.cpp
        src/preview/runtime/PreviewSceneAssetLoader.h
        src/preview/runtime/PreviewSceneAssetRepository.cpp
        src/preview/runtime/PreviewSceneAssetRepository.h
    PUBLIC miacode_scene Qt6::Quick Qt6::Qml
    QRC resources/preview_judge_effects.qrc
)
if(MIACODE_PREVIEW_MULTIMEDIA)
    target_link_libraries(miacode_preview_quick PUBLIC Qt6::Multimedia)
    target_compile_definitions(miacode_preview_quick PRIVATE HAVE_QT_MULTIMEDIA=1)
endif()
# qmltyperegistrar includes QML_ELEMENT headers by file name only.
target_include_directories(miacode_preview_quick PRIVATE src/preview/quick_scene)
if (WIN32)
    target_link_libraries(miacode_preview_quick PRIVATE
        d3d11
        dxgi         # PreviewQuickGraphicsInfo adapter details
    )
endif()
qt_add_shaders(miacode_preview_quick "preview_sprite_shaders"
    GLSL "100es,300es,120,150,330"
    PREFIX "/"
    FILES
        src/preview/quick_scene/shaders/PreviewSpriteMaterial.vert
        src/preview/quick_scene/shaders/PreviewSpriteMaterial.frag
        src/preview/quick_scene/shaders/PreviewStageDimMaterial.vert
        src/preview/quick_scene/shaders/PreviewStageDimMaterial.frag
        src/preview/quick_scene/shaders/PreviewFireworkMaterial.vert
        src/preview/quick_scene/shaders/PreviewFireworkMaterial.frag
)
qt_add_qml_module(miacode_preview_quick
    URI MiaCode.Preview
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml_modules/MiaCode/Preview"
)

if(MIACODE_BUILD_APP)
# ---- stage_media: PreviewStageMediaHost over QtAVPlayer -------------------
miacode_add_module(miacode_stage_media
    SOURCES
        src/preview/stage_media/PreviewSharedD3D11Device.cpp
        src/preview/stage_media/PreviewSharedD3D11Device.h
        src/preview/stage_media/PreviewStageMediaHost.cpp
        src/preview/stage_media/PreviewStageMediaHost.h
        src/preview/stage_media/PreviewStageMediaHostInternal.h
        src/preview/stage_media/PreviewStageMediaHost_Backend.cpp
        src/preview/stage_media/PreviewStageMediaHost_Diagnostics.cpp
        src/preview/stage_media/PreviewStageMediaHost_Media.cpp
        src/preview/stage_media/PreviewStageMediaHost_Playback.cpp
        src/preview/stage_media/PreviewStageMediaHost_Timeout.cpp
        src/preview/stage_media/PvMemoryDiagnostics.cpp
        src/preview/stage_media/PvMemoryDiagnostics.h
    PUBLIC miacode_preview_quick Qt6::Multimedia
    PRIVATE Qt6::MultimediaQuickPrivate
    QRC resources/preview_runtime_qml.qrc
)
# QtAVPlayer is compiled into this library. QT_AVPLAYER_MULTIMEDIA builds the
# QAVVideoFrame -> QVideoFrame bridge; QT_BUILD_QTAVPLAYER_LIB exports its
# symbols from the executable that links this library.
target_sources(miacode_stage_media PRIVATE ${QtAVPlayer_SOURCES})
target_include_directories(miacode_stage_media PRIVATE
    "${QTAVPLAYER_SRC}"                  # makes <QtAVPlayer/...> resolve
)
if (WIN32 OR APPLE)
    target_include_directories(miacode_stage_media PRIVATE
        "${MIACODE_FFMPEG_DEV_DIR}/include")
endif()
target_compile_definitions(miacode_stage_media PRIVATE
    QT_AVPLAYER_MULTIMEDIA
    QT_BUILD_QTAVPLAYER_LIB
    QT_AVPLAYER_NO_AVDEVICE
)
target_link_libraries(miacode_stage_media PRIVATE ${QtAVPlayer_LIBS})
if (WIN32)
    target_link_libraries(miacode_stage_media PRIVATE
        d3d11        # Shared preview device and D3D11VA video share
        dxgi
        opengl32     # QtAVPlayer D3D11/OpenGL texture interop (MinGW)
        d3dcompiler  # QtAVPlayer D3DCompile (MinGW; MSVC uses pragma comment)
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(miacode_stage_media PRIVATE
        PkgConfig::MIACODE_FFMPEG
        PkgConfig::MIACODE_VAAPI)
endif()
endif()

if(MIACODE_BUILD_APP)
# ---- export: video/cover export, QSG/D3D11 export sessions ----------------
miacode_add_module(miacode_export
    SOURCES
        src/export/cover_export/CoverCompositeRenderer.cpp
        src/export/cover_export/CoverCompositeRenderer.h
        src/export/cover_export/CoverCompositionPersistenceGuard.cpp
        src/export/cover_export/CoverCompositionPersistenceGuard.h
        src/export/cover_export/CoverCompositionState.cpp
        src/export/cover_export/CoverCompositionState.h
        src/export/cover_export/CoverFrameExportPlan.cpp
        src/export/cover_export/CoverFrameExportPlan.h
        src/export/cover_export/CoverFramePlaybackController.cpp
        src/export/cover_export/CoverFramePlaybackController.h
        src/export/cover_export/CoverFrameSceneBinder.cpp
        src/export/cover_export/CoverFrameSceneBinder.h
        src/export/cover_export/CoverLayoutModel.cpp
        src/export/cover_export/CoverLayoutModel.h
        src/export/cover_export/SceneFrameRenderer.cpp
        src/export/cover_export/SceneFrameRenderer.h
        src/export/session/PreviewQuickD3D11ExportSession.cpp
        src/export/session/PreviewQuickD3D11ExportSession.h
        src/export/session/PreviewQuickExportSession.cpp
        src/export/session/PreviewQuickExportSession.h
        src/export/video_export/BassExportAudioBackend.cpp
        src/export/video_export/BassExportAudioBackend.h
        src/export/video_export/FontLibrary.cpp
        src/export/video_export/FontLibrary.h
        src/export/video_export/RawVideoPipeTransport.cpp
        src/export/video_export/RawVideoPipeTransport.h
        src/export/video_export/VideoExportAudioBackend.h
        src/export/video_export/VideoExportAudioRenderPlan.cpp
        src/export/video_export/VideoExportAudioRenderPlan.h
        src/export/video_export/VideoExportConfig.h
        src/export/video_export/VideoExportController.cpp
        src/export/video_export/VideoExportController.h
        src/export/video_export/VideoExportControllerInternal.h
        src/export/video_export/VideoExportDiagnostics.cpp
        src/export/video_export/EncoderProbeCache.h
        src/export/video_export/EncoderProbeCache.cpp
        src/export/video_export/VideoExportEncoder.cpp
        src/export/video_export/VideoExportFrameRender.cpp
        src/export/video_export/VideoExportMediaTimeline.cpp
        src/export/video_export/VideoExportMediaTimeline.h
        src/export/video_export/VideoExportPendingFrameRedraw.h
        src/export/video_export/VideoExportPipeline.cpp
        src/export/video_export/VideoExportPreparedTask.cpp
        src/export/video_export/VideoExportQuickRenderBackend.cpp
        src/export/video_export/VideoExportQuickRenderBackend.h
        src/export/video_export/VideoExportRuntimePolicy.cpp
        src/export/video_export/VideoExportRuntimePolicy.h
        src/export/video_export/VideoExportSettings.cpp
        src/export/video_export/VideoExportSettings.h
        src/export/video_export/VideoExportSnapshot.cpp
        src/export/video_export/VideoExportSnapshot.h
    PUBLIC miacode_audio_bass miacode_preview_quick Qt6::Quick Qt6::Qml
    QRC resources/intro.qrc
)
if (WIN32)
    target_link_libraries(miacode_export PRIVATE
        d3d11        # D3D11 export session
        dxgi
    )
endif()
qt_add_shaders(miacode_export "intro_shaders"
    PREFIX "/"
    FILES
        src/intro/shaders/bg_texture.frag
)
endif()

if(MIACODE_BUILD_APP)
# ---- media_tools: PV compression, ZIP packaging, network client ----------
miacode_add_module(miacode_media_tools
    SOURCES
        src/media_tools/media/PvBatchCompressionScanner.cpp
        src/media_tools/media/PvBatchCompressionScanner.h
        src/media_tools/media/PvBatchCompressionWorker.cpp
        src/media_tools/media/PvBatchCompressionWorker.h
        src/media_tools/media/PvCompressionPolicy.cpp
        src/media_tools/media/PvCompressionPolicy.h
        src/media_tools/net/NetBatchDownloadWorker.cpp
        src/media_tools/net/NetBatchDownloadWorker.h
        src/media_tools/net/NetBatchUploadScanner.cpp
        src/media_tools/net/NetBatchUploadScanner.h
        src/media_tools/net/NetBatchUploadWorker.cpp
        src/media_tools/net/NetBatchUploadWorker.h
        src/media_tools/net/NetClient.cpp
        src/media_tools/net/NetClient.h
        src/media_tools/net/NetChart.h
        src/media_tools/net/NetQueryRules.cpp
        src/media_tools/net/NetQueryRules.h
        src/media_tools/net/NetTransportPort.h
        src/media_tools/net/NetHttpTransport.cpp
        src/media_tools/net/NetHttpTransport.h
        src/media_tools/net/NetEnginePort.h
        src/media_tools/net/NetProvider.cpp
        src/media_tools/net/NetProvider.h
        src/media_tools/net/NetResourceOperation.cpp
        src/media_tools/net/NetUploadOperation.cpp
        src/media_tools/net/NetUploadDiagnostics.cpp
        src/media_tools/net/NetUploadDiagnostics.h
        src/media_tools/zip_export/ChartZipPackager.cpp
        src/media_tools/zip_export/ChartZipPackager.h
    PUBLIC miacode_chart
    PRIVATE Qt6::Network Qt6::Concurrent miniz
)
endif()
