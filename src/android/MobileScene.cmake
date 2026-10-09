# The production v2 scene graph is shared by interactive preview and export.
find_package(Qt6 REQUIRED COMPONENTS Multimedia Svg ShaderTools OpenGL QuickDialogs2 Network)
add_library(MiaCodeMobileMiniz STATIC "${repo}/third_party/miniz/miniz.c")
set_target_properties(MiaCodeMobileMiniz PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF POSITION_INDEPENDENT_CODE ON)
target_include_directories(MiaCodeMobileMiniz PUBLIC "${repo}/third_party/miniz")
file(GLOB mobileSceneSources CONFIGURE_DEPENDS
    "${repo}/src/core/scene/*.cpp"
    "${repo}/src/preview/quick_scene/*.cpp")
file(GLOB mobileMuriSources CONFIGURE_DEPENDS "${repo}/src/tools/muri/*.cpp")
file(GLOB mobileTimelineSources CONFIGURE_DEPENDS
    "${repo}/src/timeline/quick/*.cpp"
    "${repo}/src/timeline/TimelineQuickModel*.cpp")
list(FILTER mobileMuriSources EXCLUDE REGEX "(MuriSpec|MuriDump).cpp$")
add_library(MiaCodeMobileScene STATIC ${mobileSceneSources}
    ${mobileMuriSources}
    ${mobileTimelineSources}
    "${repo}/src/timeline/TimelineSceneState.cpp"
    "${repo}/src/timeline/TimelineSceneStateBuilder.cpp"
    "${repo}/src/timeline/TimelineSlowRefresh.cpp"
    "${repo}/src/common/InputShortcutGesture.cpp"
    "${repo}/src/common/ProcessDiagnostics.cpp"
    "${repo}/src/common/WaveformCache.cpp"
    "${repo}/src/common/ProjectPreferences.cpp"
    "${repo}/src/android/MobileOfflineAudioDecoder.cpp"
    "${repo}/src/android/MobileSfxMixer.cpp"
    "${repo}/src/android/MobileSfxMixer.h"
    "${repo}/src/android/MobileExportTask.cpp"
    "${repo}/src/android/MobileBatchExport.cpp"
    "${repo}/src/app/services/AnalysisService.cpp"
    "${repo}/src/app/ui/document/AnalysisModel.cpp"
    "${repo}/src/app/ui/document/AnalysisProjection.cpp"
    "${repo}/src/app/ui/chrome/ShortcutRegistry.cpp"
    "${repo}/src/app/ui/chrome/ShortcutModel.cpp"
    "${repo}/src/common/MuriTypes.cpp"
    "${repo}/src/app/ui/preferences/PreferenceDocument.cpp"
    "${repo}/src/app/services/LatencyAudition.h"
    "${repo}/src/app/ui/latency/LatencyModel.cpp"
    "${repo}/src/app/ui/latency/LatencyModel.h"
    "${repo}/src/tools/latency/LatencyAnalysis.cpp"
    "${repo}/src/tools/latency/LatencyTestChartBuilder.cpp"
    "${repo}/src/app/ui/layout/WorkbenchSettings.cpp"
    "${repo}/src/app/ui/layout/WorkbenchSettings.h"
    "${repo}/src/app/ui/theme/ThemeVariantResolver.cpp"
    "${repo}/src/app/ui/preferences/AppBackgroundSettings.cpp"
    "${repo}/src/app/ui/preferences/AppBackgroundModel.cpp"
    "${repo}/src/timeline/TimelineNoteAssets.cpp"
    "${repo}/src/preview/runtime/PreviewRuntime.cpp"
    "${repo}/src/preview/runtime/PreviewQuickExportSession.cpp"
    "${repo}/src/preview/runtime/PreviewQuickExportSession.h"
    "${repo}/src/audio/PreviewAudioSettings.cpp"
    "${repo}/src/app/services/ShellNotifications.cpp"
    "${repo}/src/app/services/UiRequestService.cpp"
    "${repo}/src/app/services/JobProgressService.cpp"
    "${repo}/src/app/services/PreviewAppearanceState.cpp"
    "${repo}/src/app/ui/export/ExportSession.cpp"
    "${repo}/src/app/ui/export/CoverExportSession.cpp"
    "${repo}/src/tools/cover_export/CoverLayoutModel.cpp"
    "${repo}/src/tools/cover_export/CoverCompositionState.cpp"
    "${repo}/src/tools/cover_export/CoverCompositionPersistenceGuard.cpp"
    "${repo}/src/tools/cover_export/CoverFramePlaybackController.cpp"
    "${repo}/src/tools/cover_export/CoverFrameExportPlan.cpp"
    "${repo}/src/tools/cover_export/CoverFrameSceneBinder.cpp"
    "${repo}/src/tools/cover_export/SceneFrameRenderer.cpp"
    "${repo}/src/tools/cover_export/CoverCompositeRenderer.cpp"
    "${repo}/src/tools/cover_export/CoverBatchExport.cpp"
    "${repo}/src/app/ui/preview/PreviewSettingsModel.cpp"
    "${repo}/src/app/ui/preview/AudioSettingsModel.cpp"
    "${repo}/src/app/ui/preferences/LocaleService.cpp"
    "${repo}/src/app/ui/preferences/PreferencesModel.cpp"
    "${repo}/src/app/services/update/UpdateService.cpp"
    "${repo}/src/app/services/update/UpdateManifest.cpp"
    "${repo}/src/app/services/update/SemanticVersion.cpp"
    "${repo}/src/app/services/update/NetworkUpdateFetcher.cpp"
    "${repo}/src/app/services/update/PreferenceUpdateStateStore.cpp"
    "${repo}/src/tools/video_export/VideoExportSettings.cpp"
    "${repo}/src/tools/video_export/VideoExportRuntimePolicy.cpp"
    "${repo}/src/tools/video_export/VideoExportAudioRenderPlan.cpp"
    "${repo}/src/tools/video_export/VideoExportPauseOverlay.cpp"
    "${repo}/src/tools/video_export/FontLibrary.cpp"
    "${repo}/src/preview/runtime/PreviewRuntime.h"
    "${repo}/src/preview/runtime/PreviewSceneAssetLoader.cpp"
    "${repo}/src/preview/runtime/PreviewSceneAssetRepository.cpp"
    "${repo}/src/common/Mmcss.cpp")
target_sources(MiaCodeMobileScene PRIVATE "${repo}/src/tools/zip_export/ChartZipPackager.cpp")
target_include_directories(MiaCodeMobileScene PUBLIC "${repo}/src" "${repo}/src/core/chart/parser" "${repo}/src/app/ui" "${repo}/src/app")
target_include_directories(MiaCodeMobileScene PRIVATE "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(MiaCodeMobileScene PUBLIC HAVE_QT_MULTIMEDIA MIACODE_MOBILE)
target_link_libraries(MiaCodeMobileScene PUBLIC MiaCodeAndroidFoundation MiaCodeMobileMiniz Qt6::Quick Qt6::Multimedia Qt6::Svg Qt6::OpenGL Qt6::Network)
target_link_libraries(MiaCodeAndroid PRIVATE MiaCodeMobileScene Qt6::QuickDialogs2)
if(WIN32)
    target_link_libraries(MiaCodeMobileScene PRIVATE avrt)
endif()
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/MobilePreview.cpp" "${repo}/src/android/MobilePreview.h"
    "${repo}/src/android/AndroidFileRequests.cpp" "${repo}/src/android/AndroidFileRequests.h"
    "${repo}/src/android/MobileVideoExport.cpp" "${repo}/src/android/MobileVideoExport.h"
    "${repo}/src/android/MobileExportComposition.cpp" "${repo}/src/android/MobileExportComposition.h"
    "${repo}/src/android/MobileCoverComposition.cpp" "${repo}/src/android/MobileCoverComposition.h"
    "${repo}/src/android/MobileExportAudio.cpp"
    "${repo}/src/android/MobileTimeline.cpp" "${repo}/src/android/MobileTimeline.h")
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/MobileZipExport.cpp" "${repo}/src/android/MobileZipExport.h")
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/MobileAudioAudition.cpp" "${repo}/src/android/MobileAudioAudition.h")
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/MobileLatency.cpp" "${repo}/src/android/MobileLatency.h")
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/MobileSfxOutput.cpp" "${repo}/src/android/MobileSfxOutput.h")
target_sources(MiaCodeAndroid PRIVATE "${repo}/src/android/MobilePreferencesStore.cpp" "${repo}/src/android/MobilePreferencesStore.h"
    "${repo}/src/android/MobileVideoFrameRouter.cpp" "${repo}/src/android/MobileVideoFrameRouter.h")
qt_add_resources(MiaCodeAndroid mobileZipLicense PREFIX "/licenses/miniz"
    BASE "${repo}/third_party/miniz" FILES "${repo}/third_party/miniz/LICENSE")
if(ANDROID)
    set(mobilePackage "${CMAKE_BINARY_DIR}/android-package")
    file(COPY "${repo}/packaging/android/" DESTINATION "${mobilePackage}")
    foreach(assetFolder skin background noteguide SFX fonts)
        file(COPY "${repo}/assets/${assetFolder}" DESTINATION "${mobilePackage}/assets/miacode")
    endforeach()
    set_target_properties(MiaCodeAndroid PROPERTIES QT_ANDROID_PACKAGE_SOURCE_DIR "${mobilePackage}")
endif()
target_sources(MiaCodeAndroid PRIVATE
    "${repo}/resources/slide_data.qrc"
    "${repo}/resources/preview_judge_effects.qrc"
    "${repo}/resources/preview_runtime_qml.qrc")
target_sources(MiaCodeAndroid PRIVATE "${repo}/resources/intro.qrc")
if(MIACODE_ANDROID_HOST_PROBE AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(MobileIntroTextureSpec "${repo}/src/android/tests/MobileIntroTextureSpec.cpp")
    set_target_properties(MobileIntroTextureSpec PROPERTIES WIN32_EXECUTABLE FALSE)
    target_include_directories(MobileIntroTextureSpec PRIVATE "${repo}/src")
    target_link_libraries(MobileIntroTextureSpec PRIVATE Qt6::Quick Qt6::Test)
    add_test(NAME MobileIntroTextureSpec COMMAND MobileIntroTextureSpec)
    set_tests_properties(MobileIntroTextureSpec PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    qt_add_executable(MobileLatencySpec "${repo}/src/android/tests/MobileLatencySpec.cpp"
        "${repo}/src/android/MobileLatency.cpp" "${repo}/src/android/MobileLatency.h"
        "${repo}/src/android/MobilePreview.cpp" "${repo}/src/android/MobilePreview.h"
        "${repo}/src/android/MobileTimeline.cpp" "${repo}/src/android/MobileTimeline.h"
        "${repo}/src/android/MobileSfxOutput.cpp" "${repo}/src/android/MobileSfxOutput.h"
        "${repo}/src/android/MobileVideoFrameRouter.cpp" "${repo}/src/android/MobileVideoFrameRouter.h"
        "${repo}/src/app/services/EditorSyncController.cpp" "${repo}/src/app/services/EditorSyncController.h")
    set_target_properties(MobileLatencySpec PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(MobileLatencySpec PRIVATE MiaCodeMobileScene Qt6::Test)
    add_test(NAME MobileLatencySpec COMMAND MobileLatencySpec)
    qt_add_executable(MobilePreferenceMigrationSpec "${repo}/src/android/tests/MobilePreferenceMigrationSpec.cpp")
    target_link_libraries(MobilePreferenceMigrationSpec PRIVATE MiaCodeMobileScene)
    add_test(NAME MobilePreferenceMigrationSpec COMMAND MobilePreferenceMigrationSpec)
    foreach(preferencesSpec UpdateServiceSpec UpdateManifestSpec UpdateVersionSpec)
        qt_add_executable(${preferencesSpec} "${repo}/src/tools/update/${preferencesSpec}.cpp")
        target_link_libraries(${preferencesSpec} PRIVATE MiaCodeMobileScene Qt6::Test)
        if(MSVC)
            # These test names contain "Update". Embed the execution level so
            # Windows installer detection does not request elevation for tests.
            target_link_options(${preferencesSpec} PRIVATE "/MANIFEST:EMBED"
                "/MANIFESTUAC:level='asInvoker' uiAccess='false'")
        endif()
        add_test(NAME ${preferencesSpec} COMMAND ${preferencesSpec})
    endforeach()
    qt_add_executable(AnalysisServiceSpec "${repo}/src/tools/services/AnalysisServiceSpec.cpp")
    target_link_libraries(AnalysisServiceSpec PRIVATE MiaCodeMobileScene)
    add_test(NAME AnalysisServiceSpec COMMAND AnalysisServiceSpec)
    qt_add_executable(MobileFramePacingSpec "${repo}/src/android/tests/MobileFramePacingSpec.cpp"
        "${repo}/src/android/MobileVideoFrameRouter.cpp" "${repo}/src/android/MobileVideoFrameRouter.h")
    target_include_directories(MobileFramePacingSpec PRIVATE "${repo}/src")
    target_link_libraries(MobileFramePacingSpec PRIVATE Qt6::Quick Qt6::Multimedia)
    add_test(NAME MobileFramePacingSpec COMMAND MobileFramePacingSpec)
    qt_add_executable(MobileTimelineOverlaySpec "${repo}/src/android/tests/MobileTimelineOverlaySpec.cpp")
    set_target_properties(MobileTimelineOverlaySpec PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(MobileTimelineOverlaySpec PRIVATE MiaCodeMobileScene Qt6::Test)
    add_test(NAME MobileTimelineOverlaySpec COMMAND MobileTimelineOverlaySpec)
    set_tests_properties(MobileTimelineOverlaySpec PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    if(WIN32)
        add_test(NAME MobileTimelineOverlayRhiSpec COMMAND MobileTimelineOverlaySpec)
        set_tests_properties(MobileTimelineOverlayRhiSpec PROPERTIES ENVIRONMENT
            "QT_QPA_PLATFORM=windows;QT_QUICK_BACKEND=;MIACODE_TIMELINE_REQUIRE_RHI=1")
    endif()
    qt_add_executable(MobileWorkbenchSettingsSpec "${repo}/src/android/tests/MobileWorkbenchSettingsSpec.cpp"
        "${repo}/resources/fonts.qrc")
    target_link_libraries(MobileWorkbenchSettingsSpec PRIVATE MiaCodeMobileScene)
    add_test(NAME MobileWorkbenchSettingsSpec COMMAND MobileWorkbenchSettingsSpec)
    qt_add_executable(MobileSfxMixerSpec "${repo}/src/android/tests/MobileSfxMixerSpec.cpp")
    target_compile_definitions(MobileSfxMixerSpec PRIVATE MIACODE_MOBILE_SFX_DIRECTORY="${repo}/assets/SFX")
    target_link_libraries(MobileSfxMixerSpec PRIVATE MiaCodeMobileScene)
    add_test(NAME MobileSfxMixerSpec COMMAND MobileSfxMixerSpec)
    qt_add_executable(MobileZipExportSpec "${repo}/src/android/tests/MobileZipExportSpec.cpp"
        "${repo}/src/android/MobileZipExport.cpp" "${repo}/src/android/MobileZipExport.h")
    target_link_libraries(MobileZipExportSpec PRIVATE MiaCodeMobileScene)
    add_test(NAME MobileZipExportSpec COMMAND MobileZipExportSpec)
    qt_add_executable(ChartZipPackagerSpec "${repo}/src/tools/zip_export/ChartZipPackagerSpec.cpp")
    target_link_libraries(ChartZipPackagerSpec PRIVATE MiaCodeMobileScene)
    add_test(NAME ChartZipPackagerSpec COMMAND ChartZipPackagerSpec)
    qt_add_executable(VideoExportRuntimePolicySpec "${repo}/src/tools/video_export/VideoExportRuntimePolicySpec.cpp")
    target_link_libraries(VideoExportRuntimePolicySpec PRIVATE MiaCodeMobileScene)
    add_test(NAME VideoExportRuntimePolicySpec COMMAND VideoExportRuntimePolicySpec)
    foreach(coverSpec CoverLayoutModelSpec CoverFramePlaybackControllerSpec CoverFrameSceneBinderSpec)
        qt_add_executable(${coverSpec} "${repo}/src/tools/cover_export/${coverSpec}.cpp")
        target_link_libraries(${coverSpec} PRIVATE MiaCodeMobileScene)
        add_test(NAME ${coverSpec} COMMAND ${coverSpec})
    endforeach()
    qt_add_executable(MobileBatchExportSpec "${repo}/src/android/tests/MobileBatchExportSpec.cpp")
    target_link_libraries(MobileBatchExportSpec PRIVATE MiaCodeMobileScene)
    add_test(NAME MobileBatchExportSpec COMMAND MobileBatchExportSpec)
    qt_add_executable(ExportDestinationSpec "${repo}/src/android/tests/ExportDestinationSpec.cpp")
    target_include_directories(ExportDestinationSpec PRIVATE "${repo}/src")
    target_link_libraries(ExportDestinationSpec PRIVATE Qt6::Core)
    add_test(NAME ExportDestinationSpec COMMAND ExportDestinationSpec)
    qt_add_executable(MobileExportAudioSpec
        "${repo}/src/android/tests/MobileExportAudioSpec.cpp"
        "${repo}/src/android/MobileExportAudio.cpp")
    target_include_directories(MobileExportAudioSpec PRIVATE "${repo}/src")
    target_link_libraries(MobileExportAudioSpec PRIVATE Qt6::Gui Qt6::Multimedia)
    add_test(NAME MobileExportAudioSpec COMMAND MobileExportAudioSpec)
endif()
qt_add_shaders(MiaCodeAndroid mobilePreviewShaders PREFIX "/" GLSL "300 es,330" FILES
    "src/preview/quick_scene/shaders/PreviewSpriteMaterial.vert"
    "src/preview/quick_scene/shaders/PreviewSpriteMaterial.frag"
    "src/preview/quick_scene/shaders/PreviewStageDimMaterial.vert"
    "src/preview/quick_scene/shaders/PreviewStageDimMaterial.frag"
    "src/preview/quick_scene/shaders/PreviewFireworkMaterial.vert"
    "src/preview/quick_scene/shaders/PreviewFireworkMaterial.frag"
    "src/intro/shaders/bg_texture.frag")
