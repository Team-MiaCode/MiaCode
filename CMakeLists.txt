cmake_minimum_required(VERSION 3.24)

set(MIACODE_VERSION_MAJOR "2")
set(MIACODE_VERSION_MINOR "0")
set(MIACODE_VERSION_PATCH "0")
# Single source of truth for the prerelease tag and package version. The
# About dialog appends its compile date to this version at runtime.
set(MIACODE_VERSION_PRERELEASE "alpha")
set(MIACODE_BUILD_LAYOUT_REVISION "2026-04-21-first-unification")
set(MIACODE_VERSION "${MIACODE_VERSION_MAJOR}.${MIACODE_VERSION_MINOR}.${MIACODE_VERSION_PATCH}")
set(MIACODE_VERSION_STRING "${MIACODE_VERSION}")
if (NOT "${MIACODE_VERSION_PRERELEASE}" STREQUAL "")
    set(MIACODE_VERSION_STRING "${MIACODE_VERSION}-${MIACODE_VERSION_PRERELEASE}")
endif()
# The application metadata version remains the package version. The About
# dialog adds its compile date to the visible version separately.
set(MIACODE_DISPLAY_VERSION_STRING "${MIACODE_VERSION_STRING}")

if (APPLE)
    # MiaCode's macOS release is Apple-Silicon-only. Keeping this fixed before
    # project() also makes every bundled dependency and helper inherit the same
    # architecture contract instead of silently following the build machine.
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Supported macOS architecture" FORCE)
endif()

project(MiaCode VERSION ${MIACODE_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum macOS deployment version" FORCE)
endif()

if (MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>"
    )
    # MSBuild hands each project's sources to one cl.exe; without /MP that
    # process compiles them serially. Ninja already parallelizes per file.
    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/MP>)
    endif()
endif()

# <windows.h> defines min/max as function-like macros unless NOMINMAX is set,
# which mangles `std::numeric_limits<T>::max()` into a macro invocation and
# fails to compile (C4003/C2589/C2059). Vendored headers pull <windows.h> in
# without the guard — third_party/bass/include/bass.h does — so whether a
# translation unit breaks depends on include order alone: any TU that reaches a
# Bass* header before common/DebugOptions.h gets the macros, and the
# `#define NOMINMAX` inside DebugOptions.h is then too late to help. Defining it
# for every target up front makes the outcome order-independent. (Qt does the
# same thing in QtCore/qt_windows.h.)
if (WIN32)
    add_compile_definitions(NOMINMAX)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(MIACODE_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.ico" CACHE FILEPATH "Optional application icon (.ico) for Windows builds")
set(MIACODE_MACOS_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.icns" CACHE FILEPATH "Optional application icon (.icns) for macOS bundles")
option(MIACODE_BUILD_DEV_TOOLS "Build developer helper tools" OFF)
option(MIACODE_BUILD_APP "Build the desktop application and its platform dependencies" ON)
option(MIACODE_PREVIEW_MULTIMEDIA "Enable QVideoFrame input in the shared preview" ${MIACODE_BUILD_APP})

# External preview hosts configure only the chart/scene/Quick dependency closure.
# Return before configuring desktop UI, BASS, FFmpeg, or app deployment.
if(NOT MIACODE_BUILD_APP)
    include(cmake/MiaCodePreviewBuild.cmake)
    return()
endif()

# Dev-tool spec executables register themselves with CTest (see the
# MIACODE_BUILD_DEV_TOOLS block below). enable_testing() must be called at
# top level for `ctest` to discover them; it is harmless when no tests exist.
enable_testing()

set(MIACODE_GIT_REVISION "unknown")
set(MIACODE_GIT_DIRTY "unknown")
find_package(Git QUIET)
if (GIT_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE MIACODE_GIT_REVISION_RESULT
        OUTPUT_VARIABLE MIACODE_GIT_REVISION_OUTPUT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (MIACODE_GIT_REVISION_RESULT EQUAL 0
        AND NOT "${MIACODE_GIT_REVISION_OUTPUT}" STREQUAL "")
        set(MIACODE_GIT_REVISION "${MIACODE_GIT_REVISION_OUTPUT}")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal -- .
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE MIACODE_GIT_DIRTY_RESULT
            OUTPUT_VARIABLE MIACODE_GIT_DIRTY_OUTPUT
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (MIACODE_GIT_DIRTY_RESULT EQUAL 0)
            if ("${MIACODE_GIT_DIRTY_OUTPUT}" STREQUAL "")
                set(MIACODE_GIT_DIRTY "0")
            else()
                set(MIACODE_GIT_DIRTY "1")
            endif()
        endif()
    endif()
endif()

configure_file(
    src/app/AppVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/AppVersion.h
    @ONLY
)

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Concurrent Gui LinguistTools Multimedia Quick Quick3D Quick3DHelpers Qml QuickControls2 ShaderTools)

qt_policy(SET QTP0001 NEW)

# MultimediaQuickPrivate exposes the private QtMultimedia headers
# (qhwvideobuffer_p.h / qvideotexturehelper_p.h) that QtAVPlayer's
# QAVVideoFrame -> QVideoFrame bridge needs. The Windows, macOS, and Linux
# QtAVPlayer preview backends use it (see the QtAVPlayer block below).
find_package(Qt6 6.10 REQUIRED COMPONENTS MultimediaQuickPrivate)

# Pinned ScintillaQuick sources are maintained as a Git submodule.
set(SCINTILLAQUICK_BUILD_TESTS OFF CACHE BOOL "Build ScintillaQuick tests" FORCE)
set(SCINTILLAQUICK_BUILD_EXAMPLES OFF CACHE BOOL "Build ScintillaQuick examples" FORCE)
set(SCINTILLAQUICK_BUILD_BENCHMARKS OFF CACHE BOOL "Build ScintillaQuick benchmarks" FORCE)
set(CMAKE_PROJECT_ScintillaQuick_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ScintillaQuickIntegration.cmake")
add_subdirectory(third_party/ScintillaQuick)
unset(CMAKE_PROJECT_ScintillaQuick_INCLUDE)

add_library(soundtouch STATIC
    third_party/soundtouch/source/SoundTouch/AAFilter.cpp
    third_party/soundtouch/source/SoundTouch/BPMDetect.cpp
    third_party/soundtouch/source/SoundTouch/cpu_detect_x86.cpp
    third_party/soundtouch/source/SoundTouch/FIFOSampleBuffer.cpp
    third_party/soundtouch/source/SoundTouch/FIRFilter.cpp
    third_party/soundtouch/source/SoundTouch/InterpolateCubic.cpp
    third_party/soundtouch/source/SoundTouch/InterpolateLinear.cpp
    third_party/soundtouch/source/SoundTouch/InterpolateShannon.cpp
    third_party/soundtouch/source/SoundTouch/mmx_optimized.cpp
    third_party/soundtouch/source/SoundTouch/PeakFinder.cpp
    third_party/soundtouch/source/SoundTouch/RateTransposer.cpp
    third_party/soundtouch/source/SoundTouch/SoundTouch.cpp
    third_party/soundtouch/source/SoundTouch/sse_optimized.cpp
    third_party/soundtouch/source/SoundTouch/TDStretch.cpp
)
target_include_directories(soundtouch PUBLIC
    third_party/soundtouch/include
    third_party/soundtouch/source/SoundTouch
)
target_compile_definitions(soundtouch PUBLIC SOUNDTOUCH_FLOAT_SAMPLES=1)

# Vendored single-file ZIP writer (third_party/miniz), used by the
# "Export as ZIP" feature. Public stable API, no new runtime DLL — the
# amalgamated miniz.c links straight into the static lib.
add_library(miniz STATIC
    third_party/miniz/miniz.c
)
target_include_directories(miniz PUBLIC
    third_party/miniz
)
# Plain C, no Qt — keep the global AUTOMOC/UIC/RCC off this target.
set_target_properties(miniz PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# BASS runtime and import libraries (linked by miacode_audio_bass and
# miacode_export, staged beside MiaCode below).
if (WIN32)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(MIACODE_BASS_WINDOWS_ARCH "winarm64")
    else()
        set(MIACODE_BASS_WINDOWS_ARCH "win64")
    endif()
    set(MIACODE_BASS_WINDOWS_LIB_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/${MIACODE_BASS_WINDOWS_ARCH}")
    set(MIACODE_BASS_WINDOWS_BIN_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/bin/${MIACODE_BASS_WINDOWS_ARCH}")
    # bass_aac ships for x86/x64 only; the arm64 runtime uses the remaining
    # plugins and the audio backend already tolerates a missing add-on.
    set(MIACODE_BASS_WINDOWS_DLLS bass.dll bassmix.dll bass_fx.dll bassopus.dll bassflac.dll)
    if (MIACODE_BASS_WINDOWS_ARCH STREQUAL "win64")
        list(APPEND MIACODE_BASS_WINDOWS_DLLS bass_aac.dll)
    endif()
    foreach(MIACODE_BASS_DLL IN LISTS MIACODE_BASS_WINDOWS_DLLS)
        if (NOT EXISTS "${MIACODE_BASS_WINDOWS_BIN_DIR}/${MIACODE_BASS_DLL}")
            message(FATAL_ERROR
                "Missing required Windows BASS runtime: ${MIACODE_BASS_WINDOWS_BIN_DIR}/${MIACODE_BASS_DLL}")
        endif()
    endforeach()
elseif (APPLE)
    set(MIACODE_BASS_MACOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/macos/universal")
    set(MIACODE_BASS_MACOS_LIBRARIES
        "${MIACODE_BASS_MACOS_DIR}/libbass.dylib"
        "${MIACODE_BASS_MACOS_DIR}/libbassmix.dylib"
        "${MIACODE_BASS_MACOS_DIR}/libbass_fx.dylib"
        "${MIACODE_BASS_MACOS_DIR}/libbassopus.dylib"
        "${MIACODE_BASS_MACOS_DIR}/libbassflac.dylib"
    )
    foreach(MIACODE_BASS_LIBRARY IN LISTS MIACODE_BASS_MACOS_LIBRARIES)
        if (NOT EXISTS "${MIACODE_BASS_LIBRARY}")
            message(FATAL_ERROR "Missing required macOS BASS runtime: ${MIACODE_BASS_LIBRARY}")
        endif()
    endforeach()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        set(MIACODE_BASS_LINUX_ARCH "x86_64")
    elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(MIACODE_BASS_LINUX_ARCH "aarch64")
    else()
        message(FATAL_ERROR "MiaCode's bundled Linux BASS runtime currently supports x86_64 and aarch64 only")
    endif()

    set(MIACODE_BASS_LINUX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bass/lib/linux/${MIACODE_BASS_LINUX_ARCH}")
    set(MIACODE_BASS_LINUX_LIBRARIES
        "${MIACODE_BASS_LINUX_DIR}/libbass.so"
        "${MIACODE_BASS_LINUX_DIR}/libbassmix.so"
        "${MIACODE_BASS_LINUX_DIR}/libbass_fx.so"
        "${MIACODE_BASS_LINUX_DIR}/libbassflac.so"
    )
    foreach(MIACODE_BASS_LIBRARY IN LISTS MIACODE_BASS_LINUX_LIBRARIES)
        if (NOT EXISTS "${MIACODE_BASS_LIBRARY}")
            message(FATAL_ERROR "Missing required Linux BASS runtime: ${MIACODE_BASS_LIBRARY}")
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# QtAVPlayer (FFmpeg) — preview background-video decode backend.
#
# Decodes PV/BG video inside PreviewStageMediaHost; see
# docs/ops/DEBUG_INDEX.md. QtAVPlayer is the
# MIT-licensed source vendored under third_party/QtAVPlayer, compiled into
# miacode_stage_media. Windows and macOS use the project-selected FFmpeg SDK through
# MIACODE_FFMPEG_DEV_DIR. Linux resolves FFmpeg and VA-API from the host
# toolchain through pkg-config.
#
# All desktop platforms use QtAVPlayer. Windows selects D3D11VA, macOS selects
# VideoToolbox/Metal, and Linux selects VA-API through DRM/EGL.
# ---------------------------------------------------------------------------
    if (WIN32)
        if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
            set(MIACODE_FFMPEG_DEV_DIR_DEFAULT
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/windows/winarm64/dev")
        else()
            set(MIACODE_FFMPEG_DEV_DIR_DEFAULT
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/windows/win64/dev")
        endif()
    elseif (APPLE)
        set(MIACODE_FFMPEG_DEV_DIR_DEFAULT
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg/macos/dev")
    endif()
    if (WIN32 OR APPLE)
        set(MIACODE_FFMPEG_DEV_DIR "${MIACODE_FFMPEG_DEV_DIR_DEFAULT}"
            CACHE PATH "FFmpeg shared dev SDK root (include/ + lib/) for the QtAVPlayer preview backend")
        if (NOT EXISTS "${MIACODE_FFMPEG_DEV_DIR}/include" OR NOT EXISTS "${MIACODE_FFMPEG_DEV_DIR}/lib")
            if (APPLE)
                set(MIACODE_FFMPEG_PROVISION_COMMAND
                    "bash scripts/ffmpeg/ensure-macos-ffmpeg-dev.sh")
            else()
                set(MIACODE_FFMPEG_PROVISION_COMMAND
                    "scripts/ffmpeg/ensure-windows-ffmpeg-dev.ps1")
            endif()
            message(FATAL_ERROR
                "FFmpeg dev SDK not found at '${MIACODE_FFMPEG_DEV_DIR}'. "
                "Run '${MIACODE_FFMPEG_PROVISION_COMMAND}', or pass "
                "-DMIACODE_FFMPEG_DEV_DIR=<root containing include/ lib/>.")
        endif()
        if (APPLE AND MIACODE_FFMPEG_DEV_DIR MATCHES "^/(opt/homebrew|usr/local)/")
            message(FATAL_ERROR
                "macOS release builds must use the project-provisioned FFmpeg SDK, not Homebrew: "
                "${MIACODE_FFMPEG_DEV_DIR}. Run bash scripts/ffmpeg/ensure-macos-ffmpeg-dev.sh.")
        endif()

        # find_library() in QtAVPlayer.cmake resolves the FFmpeg import libs from
        # <prefix>/lib; prepending the dev root makes our SDK win over any system one.
        list(PREPEND CMAKE_PREFIX_PATH "${MIACODE_FFMPEG_DEV_DIR}")
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(MIACODE_FFMPEG REQUIRED IMPORTED_TARGET
            libavfilter libavcodec libavformat libavutil libswresample libswscale)
        pkg_check_modules(MIACODE_VAAPI REQUIRED IMPORTED_TARGET libva libva-drm libdrm)
        find_library(AVFILTER_LIBRARY NAMES avfilter
            HINTS ${MIACODE_FFMPEG_LIBRARY_DIRS} NO_DEFAULT_PATH REQUIRED NO_CACHE)
        find_library(AVCODEC_LIBRARY NAMES avcodec
            HINTS ${MIACODE_FFMPEG_LIBRARY_DIRS} NO_DEFAULT_PATH REQUIRED NO_CACHE)
        find_library(AVFORMAT_LIBRARY NAMES avformat
            HINTS ${MIACODE_FFMPEG_LIBRARY_DIRS} NO_DEFAULT_PATH REQUIRED NO_CACHE)
        find_library(AVUTIL_LIBRARY NAMES avutil
            HINTS ${MIACODE_FFMPEG_LIBRARY_DIRS} NO_DEFAULT_PATH REQUIRED NO_CACHE)
        find_library(SWRESAMPLE_LIBRARY NAMES swresample
            HINTS ${MIACODE_FFMPEG_LIBRARY_DIRS} NO_DEFAULT_PATH REQUIRED NO_CACHE)
        find_library(SWSCALE_LIBRARY NAMES swscale
            HINTS ${MIACODE_FFMPEG_LIBRARY_DIRS} NO_DEFAULT_PATH REQUIRED NO_CACHE)
        set(QT_AVPLAYER_VA_DRM ON CACHE BOOL "" FORCE)
    endif()

    set(QTAVPLAYER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/QtAVPlayer/src")
    set(QT_AVPLAYER_DIR "${QTAVPLAYER_SRC}/QtAVPlayer")
    set(QT_AVPLAYER_MULTIMEDIA ON CACHE BOOL "" FORCE)
    # Drop avdevice (capture-device I/O only, unused for file playback):
    # QtAVPlayer skips finding/linking it and qavdemuxer compiles out its lone
    # avdevice_register_all() call.
    set(QT_AVPLAYER_NO_AVDEVICE ON)
    if (APPLE)
        # QtAVPlayer.cmake uses find_library(). These cache entries otherwise
        # survive an SDK change and can keep linking an old Homebrew path even
        # though CMAKE_PREFIX_PATH now starts with the project-local SDK.
        foreach(MIACODE_QTAVPLAYER_FFMPEG_LIBRARY
                AVFILTER_LIBRARY AVCODEC_LIBRARY AVFORMAT_LIBRARY AVUTIL_LIBRARY
                SWRESAMPLE_LIBRARY SWSCALE_LIBRARY)
            unset(${MIACODE_QTAVPLAYER_FFMPEG_LIBRARY} CACHE)
            unset(${MIACODE_QTAVPLAYER_FFMPEG_LIBRARY})
        endforeach()
    endif()
    # Sets QtAVPlayer_SOURCES / QtAVPlayer_LIBS (absolute import-lib paths via
    # find_library) and globally defines QT_BUILD_QTAVPLAYER_LIB.
    include("${QT_AVPLAYER_DIR}/QtAVPlayer.cmake")

include(cmake/MiaCodeModules.cmake)

add_executable(MiaCode WIN32 MACOSX_BUNDLE
    src/app/main.cpp
    src/app/services/ApplicationServices.cpp
    src/app/services/ApplicationServices.h
    src/app/services/jobs/JobRegistry.cpp
    src/app/services/jobs/JobRegistry.h
    src/app/services/net/NetService.cpp
    src/app/services/net/NetConfiguration.cpp
    src/app/services/net/NetConfiguration.h
    src/app/services/net/NetAccountStore.cpp
    src/app/services/net/NetAccountStore.h
    src/app/services/net/NetService.h
    src/app/services/api/ApiCatalog.cpp
    src/app/services/api/ApiCatalog.h
    src/app/services/api/ApiDispatcher.cpp
    src/app/services/api/ApiDispatcher.h
    src/app/services/DocumentBridge.h
    src/app/services/EditorPageRouter.h
    src/app/services/ExportEngine.h
    src/app/services/ExportPagePort.h
    src/app/services/LatencyEngine.h
    src/app/services/MediaToolsEngine.h
    src/app/services/PreferencesStore.h
    src/app/services/PreviewSurface.h
    src/app/services/PlaybackControl.h
    src/app/services/PlaybackStateAuthority.h
    src/app/services/SessionGeneration.h
    src/app/services/AudioClockSource.h
    src/app/services/PreviewPlaybackPort.h
    src/app/services/ShellNotifications.cpp
    src/app/services/ShellNotifications.h
    src/app/services/TimelineSurface.h
    src/app/services/PreviewAppearanceState.cpp
    src/app/services/PreviewAppearanceState.h
    src/app/services/ChartWorkspace.cpp
    src/app/services/ChartWorkspace.h
    src/app/services/ChartMediaService.cpp
    src/app/services/ChartMediaService.h
    src/app/services/ChartMediaImport.cpp
    src/app/services/ChartMediaImport.h
    src/app/services/ChartWorkspaceFileService.cpp
    src/app/services/ChartWorkspaceFileService.h
    src/app/services/AnalysisService.cpp
    src/app/services/AnalysisService.h
    src/app/services/EditorSyncController.cpp
    src/app/services/EditorSyncController.h
    src/app/services/UiRequestService.cpp
    src/app/services/UiRequestService.h
    src/app/services/JobProgressService.cpp
    src/app/services/JobProgressService.h
    src/app/ui/media/MediaToolsModel.cpp
    src/app/ui/media/MediaToolsModel.h
    src/app/ui/preview/AudioSettingsModel.cpp
    src/app/ui/preview/AudioSettingsModel.h
    src/app/ui/preview/PreviewSettingsModel.cpp
    src/app/ui/preview/PreviewSettingsModel.h
    src/app/ui/preferences/PreferencesModel.cpp
    src/app/ui/preferences/PreferencesModel.h
    src/app/ui/preferences/AppBackgroundModel.cpp
    src/app/ui/preferences/AppBackgroundModel.h
    src/app/ui/latency/LatencyModel.cpp
    src/app/ui/latency/LatencyModel.h
    src/app/startup_diagnostics_win32.cpp
    src/app/graphics_backend.cpp
    src/app/platform/PlatformDiagnostics.h
    src/app/platform/process_identity.cpp
    src/app/platform/WindowVisibilityDiagnostics.h
    src/app/platform/WindowVisibilityDiagnostics.cpp
    src/app/platform/gpu_adapter_probe.cpp
    src/app/platform/gpu_device_provider.cpp
    src/app/gpu_high_performance_hint.cpp
    src/app/cli_shared.cpp
    src/app/cli_video_export.cpp
    src/app/cli_video_export_worker.cpp
    src/app/quick_shell/QuickShellPreviewCompositeSurface.h
    src/app/quick_shell/QuickShellPreviewCompositeSurface.cpp
    src/app/quick_shell/QuickShellPreviewSurfacePolicy.h
    src/app/runtime/Session.h
    src/app/runtime/SessionLifecycle.cpp
    src/app/runtime/RuntimeContext.h
    src/app/runtime/Shared.h
    src/app/runtime/Shared.cpp
    src/app/runtime/Shared.Preview.cpp
    src/app/runtime/document/DocumentFlow.cpp
    src/app/runtime/document/DocumentBridge.cpp
    src/app/runtime/document/DocumentFileFlow.cpp
    src/app/runtime/document/DocumentDesignerFlow.cpp
    src/app/runtime/document/DocumentAutosave.cpp
    src/app/runtime/document/DocumentEditorState.cpp
    src/app/runtime/document/DocumentTransforms.cpp
    src/app/runtime/document/DocumentPages.cpp
    src/app/runtime/media/MediaJobs.cpp
    src/app/runtime/media/MediaTools.cpp
    src/app/runtime/media/MediaJobsHost.cpp
    src/app/runtime/editor/EditorDisplay.cpp
    src/app/runtime/editor/EditorState.cpp
    src/app/runtime/export/ExportFlow.cpp
    src/app/runtime/export/ExportSnapshot.cpp
    src/app/runtime/export/VideoExportHost.cpp
    src/app/runtime/export/ExportWorker.cpp
    src/app/runtime/export/PackZip.cpp
    src/app/runtime/SessionBootstrap.cpp
    src/app/runtime/SessionBootstrapFinalize.cpp
    src/app/runtime/settings/Settings.cpp
    src/app/runtime/settings/EncoderProbeCache.h
    src/app/runtime/settings/PreferenceDocumentProvider.h
    src/app/runtime/settings/PreferenceDocumentProvider.cpp
    src/app/runtime/preview/StageMediaRoute.cpp
    src/app/runtime/preview/WarmupAndSettings.cpp
    src/app/runtime/playback/TimelineFlow.cpp
    src/app/runtime/playback/SessionForwarding.TimelineFlow.cpp
    src/app/runtime/playback/FollowSync.cpp
    src/app/runtime/playback/AnalysisFlow.cpp
    src/app/runtime/playback/QuickParse.cpp
    src/app/runtime/playback/Layout.cpp
    src/app/runtime/playback/FramePacing.cpp
    src/app/runtime/playback/SessionForwarding.FramePacing.cpp
    src/app/runtime/playback/LayoutUi.cpp
    src/app/runtime/playback/SessionForwarding.cpp
    src/app/runtime/playback/Playback.cpp
    src/app/runtime/playback/SessionForwarding.Playback.cpp
    src/app/runtime/playback/Seek.cpp
    src/app/runtime/playback/PlaybackState.cpp
    src/app/runtime/playback/IntroRegion.cpp
    src/app/runtime/playback/Tick.cpp
    src/app/runtime/playback/PlaybackGlue.cpp
    src/app/runtime/playback/SessionForwarding.PlaybackGlue.cpp
    src/app/runtime/playback/PlaybackCoordinator.h
    src/app/runtime/playback/CoordinatorContract.cpp
    src/app/runtime/playback/PlaybackIdentityGate.cpp
    src/app/runtime/playback/PlaybackIdentityGate.h
    src/app/runtime/playback/PlaybackSurfaceAdapters.cpp
    src/app/runtime/playback/PlaybackSurfaceAdapters.h
    src/app/runtime/timeline/TimelineCommandGate.cpp
    src/app/runtime/timeline/TimelineCommandGate.h
    src/app/runtime/timeline/TimelineHost.cpp
    src/app/runtime/timeline/TimelineHost.h
    src/app/runtime/preview/PreviewHost.cpp
    src/app/runtime/preview/PreviewHost.h
    src/app/runtime/validation/ValidationFlow.cpp
    src/app/runtime/validation/ValidationListUi.cpp
    src/app/runtime/validation/ValidationRender.cpp
    src/app/runtime/validation/ValidationRuntime.cpp
    src/app/runtime/playback/SurfaceContract.cpp
    src/app/runtime/playback/SessionForwarding.SurfaceContract.cpp
    src/app/runtime/shell/ShellHost.cpp
    src/app/services/PreferenceJsonFile.h
    src/app/services/CoverExportPreferences.h
    src/app/services/VideoExportPreferences.h
    src/app/services/PreferenceVersionGuard.h
    src/app/services/ChartNormalizationPreferences.h
    src/app/services/HudFontPreferences.h
    src/app/services/UserFontLibrary.h
    src/app/services/PreferenceDocument.h
    src/app/services/PreferenceDocument.cpp
    src/app/ui/preferences/LocaleService.h
    src/app/ui/preferences/LocaleService.cpp
    src/app/ui/preferences/AppBackgroundSettings.h
    src/app/ui/preferences/AppBackgroundSettings.cpp
    src/app/ui/theme/ThemeVariantResolver.h
    src/app/ui/theme/ThemeVariantResolver.cpp
    src/app/services/ShortcutRegistry.h
    src/app/services/ShortcutRegistry.cpp
    src/app/ui/theme/UiTheme.h
    src/app/ui/theme/UiTheme.cpp
    src/app/ui/chrome/NativeWindowTheme.h
    src/app/ui/chrome/NativeWindowTheme.cpp
    src/app/ui/chrome/NativeWindowThemePolicy.h
    src/app/runtime/shell/WindowParityMetrics.h
    src/app/runtime/shell/WindowParityMetrics.cpp
    src/app/services/Id3TagReader.h
    src/app/services/Id3TagReader.cpp
    src/app/services/ProjectPreferences.h
    src/app/services/ProjectPreferences.cpp
    src/app/services/ExportIntroPreferences.h
    src/app/services/ExportIntroPreferences.cpp
    src/app/runtime/latency/LatencyAnalysis.h
    src/app/runtime/latency/LatencyAnalysis.cpp
    src/app/runtime/latency/LatencyTestChartBuilder.h
    src/app/runtime/latency/LatencyTestChartBuilder.cpp
    src/app/runtime/latency/LatencySandboxController.h
    src/app/runtime/latency/LatencySandboxController.cpp
    src/app/ui/drop/ChartDropBridge.h
    src/app/ui/drop/ChartDropBridge.cpp
    src/app/services/ImageResourceModel.cpp
    src/app/services/ImageResourceModel.h
    src/app/services/DocumentProjection.cpp
    src/app/services/DocumentProjection.h
    src/app/services/AnalysisProjection.cpp
    src/app/services/AnalysisProjection.h
    src/app/services/update/SemanticVersion.cpp
    src/app/services/update/SemanticVersion.h
    src/app/services/update/UpdateManifest.cpp
    src/app/services/update/UpdateManifest.h
    src/app/services/update/UpdateStateStore.h
    src/app/services/update/UpdateFetcher.h
    src/app/services/update/PreferenceUpdateStateStore.cpp
    src/app/services/update/PreferenceUpdateStateStore.h
    src/app/services/update/NetworkUpdateFetcher.cpp
    src/app/services/update/NetworkUpdateFetcher.h
    src/app/services/update/UpdateService.cpp
    src/app/services/update/UpdateService.h
)
miacode_add_qrc(MiaCode resources/app_icons.qrc)

qt_add_shaders(MiaCode "ui_shaders"
    PREFIX "/"
    FILES
        src/app/ui/shaders/corner_mask.frag
)
set_target_properties(MiaCode PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "MiaCode"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.fanfaredash.MiaCode"
)

if(UNIX AND NOT APPLE)
    set_target_properties(MiaCode PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
endif()

if (WIN32 AND EXISTS "${MIACODE_APP_ICON}")
    file(TO_CMAKE_PATH "${MIACODE_APP_ICON}" MIACODE_APP_ICON_RC_PATH)
    set(MIACODE_APP_RC "${CMAKE_CURRENT_BINARY_DIR}/generated/miacode_app_icon.rc")
    file(WRITE "${MIACODE_APP_RC}" "IDI_APP_ICON ICON \"${MIACODE_APP_ICON_RC_PATH}\"\n")
    target_sources(MiaCode PRIVATE "${MIACODE_APP_RC}")
endif()

if (APPLE AND EXISTS "${MIACODE_MACOS_APP_ICON}")
    set_source_files_properties("${MIACODE_MACOS_APP_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(MiaCode PRIVATE "${MIACODE_MACOS_APP_ICON}")
    set_target_properties(MiaCode PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "app.icns"
    )
endif()

# Default UI (v2).
# QTP0001 NEW embeds this module at :/qt/qml/MiaCode/UI/.
set(MIACODE_UI_QML_FILES
    src/app/ui/net/NetDownloadPage.qml
    src/app/ui/net/NetUploadPage.qml
    src/app/ui/net/NetResultTable.qml
    src/app/ui/components/AppDateField.qml
    src/app/ui/Main.qml
    src/app/ui/ViewState.qml
    src/app/ui/theme/Theme.qml
    src/app/ui/components/IconButton.qml
    src/app/ui/components/AppCheckBox.qml
    src/app/ui/components/AppDropDownButton.qml
    src/app/ui/components/AppTab.qml
    src/app/ui/components/AppTabButton.qml
    src/app/ui/components/AppTabBar.qml
    src/app/ui/components/AppTabPages.qml
    src/app/ui/components/AppChoiceButton.qml
    src/app/ui/components/DifficultySelector.qml
    src/app/ui/components/DifficultySwatch.qml
    src/app/ui/components/PanelHeader.qml
    src/app/ui/components/SplitHandle.qml
    src/app/ui/components/CornerMask.qml
    src/app/ui/components/Tooltip.qml
    src/app/ui/components/HoverChrome.qml
    src/app/ui/components/ChromeRow.qml
    src/app/ui/components/NavRow.qml
    src/app/ui/components/AppTextField.qml
    src/app/ui/components/AppTextArea.qml
    src/app/ui/components/AppTextContextMenu.qml
    src/app/ui/components/AppButton.qml
    src/app/ui/components/AppSwitch.qml
    src/app/ui/components/AppComboBox.qml
    src/app/ui/components/AppSlider.qml
    src/app/ui/components/AppScrollBar.qml
    src/app/ui/components/AppMenu.qml
    src/app/ui/components/AppMenuAction.qml
    src/app/ui/components/FloatingCard.qml
    src/app/ui/components/BackdropBlur.qml
    src/app/ui/components/FadeTransition.qml
    src/app/ui/components/AppDialog.qml
    src/app/ui/components/AppStickyPopup.qml
    src/app/ui/components/AppDropdownPanel.qml
    src/app/ui/components/AppMenuItem.qml
    src/app/ui/components/AppMenuSeparator.qml
    src/app/ui/components/UiRequestHost.qml
    src/app/ui/components/DialogFooter.qml
    src/app/ui/components/ChoiceDialog.qml
    src/app/ui/components/EditableValue.qml
    src/app/ui/components/LabeledCombo.qml
    src/app/ui/components/LabeledSlider.qml
    src/app/ui/components/SettingsSection.qml
    src/app/ui/components/AboutDialog.qml
    src/app/ui/components/JobProgressOverlay.qml
    src/app/ui/components/ImageSequence.qml
    src/app/ui/chrome/WindowGestureArea.qml
    src/app/ui/chrome/WindowTitleBar.qml
    src/app/ui/chrome/WindowCaptionButtons.qml
    src/app/ui/chrome/WindowResizeBorder.qml
    src/app/ui/chrome/MainMenu.qml
    src/app/ui/chrome/NativeMenuBar.qml
    src/app/ui/chrome/ShortcutBindings.qml
    src/app/ui/chrome/MainMenuCommands.qml
    src/app/ui/chrome/MainToolBar.qml
    src/app/ui/chrome/StatusBar.qml
    src/app/ui/layout/MainView.qml
    src/app/ui/layout/MainSplitView.qml
    src/app/ui/layout/WelcomePage.qml
    src/app/ui/layout/BeginnerGuideOverlay.qml
    src/app/ui/layout/CompactPanelLayer.qml
    src/app/ui/sidebar/Sidebar.qml
    src/app/ui/sidebar/ActivityBar.qml
    src/app/ui/sidebar/ChartFieldSidebar.qml
    src/app/ui/sidebar/DifficultyList.qml
    src/app/ui/sidebar/ExportSidebarPage.qml
    src/app/ui/export/ExportRangeSelector.qml
    src/app/ui/export/ExportVideoPage.qml
    src/app/ui/export/CoverExportPage.qml
    src/app/ui/export/CoverLayerList.qml
    src/app/ui/export/CoverExportWindow.qml
    src/app/ui/editor/EditorPane.qml
    src/app/ui/editor/EditorTabBar.qml
    src/app/ui/editor/SourceEditor.qml
    src/app/ui/editor/CompletionPopup.qml
    src/app/ui/editor/NormalizeOptionsDialog.qml
    src/app/ui/editor/DesignerSlotsDialog.qml
    src/app/ui/media/AudioProcessingMenu.qml
    src/app/ui/media/VideoProcessingMenu.qml
    src/app/ui/media/PvBatchCompressionDialog.qml
    src/app/ui/media/PrependBlankDialog.qml
    src/app/ui/preferences/PreferencesDialog.qml
    src/app/ui/pet/PetOverlay.qml
    src/app/ui/pet/model/Fox.qml
    src/app/ui/latency/LatencyPage.qml
    src/app/ui/editor/FindReplaceBar.qml
    src/app/ui/editor/BookmarkList.qml
    src/app/ui/timeline/BottomPanel.qml
    src/app/ui/timeline/BottomTabBar.qml
    src/app/ui/timeline/TimelineZoomMenu.qml
    src/app/ui/timeline/TimelineSettingsMenu.qml
    src/app/ui/preview/PreviewPane.qml
    src/app/ui/preview/PreviewRenderModeMenu.qml
    src/app/ui/preview/PreviewRateMenu.qml
    src/app/ui/preview/PreviewRateToast.qml
    src/app/ui/preview/PreviewCanvasMenu.qml
    src/app/ui/preview/PreviewTransport.qml
    src/app/ui/preview/NoteStatistics.qml
    src/app/ui/preview/AudioSettingsDialog.qml
    src/app/ui/preview/PreviewAppearancePages.qml
    src/app/ui/preview/PreviewSettingsDialog.qml
)
foreach(qml_file IN LISTS MIACODE_UI_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
set_source_files_properties(
    src/app/ui/theme/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

set(MIACODE_UI_ICON_FILES
    src/app/ui/resources/icons/range-play.svg
    src/app/ui/resources/icons/range-play-active.svg
    src/app/ui/resources/icons/repeat.svg
    src/app/ui/resources/icons/repeat-active.svg
    src/app/ui/resources/icons/maximize.svg
    src/app/ui/resources/icons/restore.svg
    src/app/ui/resources/icons/check.svg
    src/app/ui/resources/icons/file-fill.svg
    src/app/ui/resources/icons/export-fill.svg
    src/app/ui/resources/icons/video.svg
    src/app/ui/resources/icons/video-fill.svg
    src/app/ui/resources/icons/image.svg
    src/app/ui/resources/icons/image-fill.svg
    src/app/ui/resources/icons/folder-zip.svg
    src/app/ui/resources/icons/folder-zip-fill.svg
    src/app/ui/resources/icons/tools-fill.svg
    src/app/ui/resources/icons/panel-left-fill.svg
    src/app/ui/resources/icons/panel-bottom-fill.svg
    src/app/ui/resources/icons/metadata-fill.svg
    src/app/ui/resources/icons/metronome-fill.svg
    src/app/ui/resources/icons/stop-fill.svg
    src/app/ui/resources/icons/play-fill.svg
    src/app/ui/resources/icons/pause-fill.svg
    src/app/ui/resources/icons/circle-play-fill.svg
    src/app/ui/resources/icons/preview-settings-fill.svg
    src/app/ui/resources/icons/sliders-horizontal-fill.svg
    src/app/ui/resources/icons/more-fill.svg
    src/app/ui/resources/icons/folder-open.svg
    src/app/ui/resources/icons/save.svg
    src/app/ui/resources/icons/undo.svg
    src/app/ui/resources/icons/redo.svg
    src/app/ui/resources/icons/play.svg
    src/app/ui/resources/icons/circle-play.svg
    src/app/ui/resources/icons/pause.svg
    src/app/ui/resources/icons/stop.svg
    src/app/ui/resources/icons/preview-detach.svg
    src/app/ui/resources/icons/preview-dock.svg
    src/app/ui/resources/icons/panel-left.svg
    src/app/ui/resources/icons/panel-bottom.svg
    src/app/ui/resources/icons/panel-right.svg
    src/app/ui/resources/icons/chevron-left.svg
    src/app/ui/resources/icons/chevron-right.svg
    src/app/ui/resources/icons/settings.svg
    src/app/ui/resources/icons/chart.svg
    src/app/ui/resources/icons/export.svg
    src/app/ui/resources/icons/tools.svg
    src/app/ui/resources/icons/audio-settings.svg
    src/app/ui/resources/icons/preview-settings.svg
    src/app/ui/resources/icons/file.svg
    src/app/ui/resources/icons/sliders-horizontal.svg
    src/app/ui/resources/icons/volume-2.svg
    src/app/ui/resources/icons/volume-x.svg
    src/app/ui/resources/icons/close.svg
    src/app/ui/resources/icons/more.svg
    src/app/ui/resources/icons/metadata.svg
    src/app/ui/resources/icons/metronome.svg
    src/app/ui/resources/icons/app-titlebar.png
    src/app/ui/resources/icons/app-titlebar@2x.png
    src/app/ui/resources/icons/chevron-down.svg
    src/app/ui/resources/icons/add.svg
    src/app/ui/resources/icons/remove.svg
    src/app/ui/resources/icons/arrow-up.svg
    src/app/ui/resources/icons/arrow-down.svg
    src/app/ui/resources/icons/copy.svg
    src/app/ui/resources/icons/trash.svg
    src/app/ui/resources/icons/eye.svg
    src/app/ui/resources/icons/eye-off.svg
    src/app/ui/resources/icons/lock.svg
    src/app/ui/resources/icons/lock-open.svg
    # Canonical MiaCode brand PNG (same asset as resources/app_icons.qrc).
    resources/icons/app.png
)
foreach(icon_file IN LISTS MIACODE_UI_ICON_FILES)
    get_filename_component(icon_name "${icon_file}" NAME)
    set_source_files_properties("${icon_file}" PROPERTIES QT_RESOURCE_ALIAS "icons/${icon_name}")
endforeach()

set(MIACODE_PET_ASSET_FILES
    src/app/ui/resources/pet/minecraft/fox.png
)
foreach(pet_asset IN LISTS MIACODE_PET_ASSET_FILES)
    get_filename_component(pet_asset_name "${pet_asset}" NAME)
    set_source_files_properties("${pet_asset}" PROPERTIES QT_RESOURCE_ALIAS "pet/minecraft/${pet_asset_name}")
endforeach()

target_link_libraries(MiaCode PRIVATE ScintillaQuick::ScintillaQuick)

qt_add_qml_module(MiaCode
    URI MiaCode.UI
    VERSION 1.0
    SOURCES
        src/app/ui/ApplicationContext.cpp
        src/app/ui/net/NetModel.cpp
        src/app/ui/net/NetModel.h
        src/app/ui/net/NetUploadModel.cpp
        src/app/ui/net/NetUploadModel.h
        src/app/ui/ApplicationContext.h
        src/app/ui/document/CommandService.cpp
        src/app/ui/document/CommandService.h
        src/app/ui/chrome/ShortcutCommands.h
        src/app/ui/document/DocumentModel.cpp
        src/app/ui/document/DocumentModel.h
        src/app/ui/document/AnalysisModel.cpp
        src/app/ui/document/AnalysisModel.h
        src/app/ui/preview/PreviewModel.cpp
        src/app/ui/preview/PreviewModel.h
        src/app/ui/timeline/TimelineModel.cpp
        src/app/ui/timeline/TimelineModel.h
        src/app/ui/shell/ShellLifecycle.cpp
        src/app/ui/shell/ShellLifecycle.h
        src/app/ui/preview/NoteImageProvider.cpp
        src/app/ui/preview/NoteImageProvider.h
        src/app/ui/layout/WorkbenchSettings.cpp
        src/app/ui/layout/WorkbenchSettings.h
        src/app/ui/chrome/PlatformChrome.h
        src/app/ui/layout/PageHost.cpp
        src/app/ui/layout/PageHost.h
        src/app/ui/editor/EditorController.cpp
        src/app/ui/editor/EditorController.h
        src/app/ui/editor/ScintillaEditorBridge.cpp
        src/app/ui/editor/ScintillaEditorBridge.h
        src/app/ui/editor/ScintillaDocumentAdapter.cpp
        src/app/ui/editor/ScintillaDocumentAdapter.h
        src/app/ui/editor/ScintillaDslStyler.cpp
        src/app/ui/editor/ScintillaDslStyler.h
        src/app/ui/chrome/ShortcutModel.cpp
        src/app/ui/chrome/ShortcutModel.h
        src/app/ui/export/ExportSession.cpp
        src/app/ui/export/ExportSession.h
        src/app/ui/export/CoverExportSession.cpp
        src/app/ui/export/CoverExportSession.h
        src/app/ui/export/CoverExportWindow.cpp
        src/app/ui/export/CoverExportWindow.h
        src/app/ui/chrome/WindowChrome.cpp
        src/app/ui/chrome/WindowChrome.h
        src/app/ui/Bootstrap.cpp
        src/app/ui/Bootstrap.h
        src/app/ui/pet/PetOverlayController.cpp
        src/app/ui/pet/PetOverlayController.h
        src/app/ui/shell/RootLifecycle.h
        src/app/ui/timeline/TimelineThemeBridge.cpp
        src/app/ui/timeline/TimelineThemeBridge.h
    QML_FILES
        ${MIACODE_UI_QML_FILES}
    RESOURCES
        ${MIACODE_UI_ICON_FILES}
        ${MIACODE_PET_ASSET_FILES}
    IMPORTS
        QtQuick3D
        QtQuick3D.Helpers
)

set(MIACODE_TRANSLATION_TS_FILES
    translations/en_US.ts
    translations/zh_CN.ts
    translations/ja_JP.ts
)
qt_add_lrelease(
    LRELEASE_TARGET miacode_lrelease
    TS_FILES ${MIACODE_TRANSLATION_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE MIACODE_TRANSLATION_QM_FILES
    OPTIONS -idbased
)
qt_add_resources(MiaCode "miacode_i18n"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${MIACODE_TRANSLATION_QM_FILES}
)
add_dependencies(MiaCode miacode_lrelease)

target_link_libraries(MiaCode PRIVATE
    miacode_base
    miacode_chart
    miacode_analysis
    miacode_editor_core
    miacode_scene
    miacode_audio
    miacode_audio_bass
    miacode_timeline
    miacode_timeline_quick
    miacode_timeline_quickplugin
    miacode_preview_quick
    miacode_preview_quickplugin
    miacode_stage_media
    miacode_export
    miacode_media_tools
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::Quick3D
    Qt6::Quick3DHelpers
    Qt6::QuickControls2
    Qt6::Multimedia
    soundtouch
)
target_include_directories(MiaCode PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    src
    # qmltyperegistrar includes QML_ELEMENT headers by file name only; project
    # code includes them by their src-rooted path.
    src/app/ui/editor
    src/app/ui/timeline
)
target_compile_definitions(MiaCode PRIVATE HAVE_QT_MULTIMEDIA=1)

if (WIN32)
    target_link_libraries(MiaCode PRIVATE
        Advapi32     # Windows Credential Manager
        winmm        # timeBeginPeriod for preview frame pacing
        d3d11        # Startup D3D11 probe (startup_diagnostics_win32.cpp)
        User32       # RegisterPowerSettingNotification
        Wtsapi32     # Session lock/unlock notifications for idle-freeze diagnostics
        version      # Startup diagnostic file-version queries (MinGW)
        rstrtmgr     # Restart Manager process lookup for media tools (MinGW)
        dwmapi       # v2 WindowChrome DwmExtendFrameIntoClientArea
    )
    add_custom_command(TARGET MiaCode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MIACODE_BASS_WINDOWS_DLLS}
            $<TARGET_FILE_DIR:MiaCode>
        WORKING_DIRECTORY "${MIACODE_BASS_WINDOWS_BIN_DIR}"
    )
    # Bundle the assets directory so the application is self-contained and
    # can run from any location without relying on the source tree.
    add_custom_command(TARGET MiaCode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:MiaCode>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:MiaCode>/assets"
        COMMENT "Bundling assets directory into output"
        VERBATIM
    )
elseif (APPLE)
    set_target_properties(MiaCode PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    add_custom_command(TARGET MiaCode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:MiaCode>/../Frameworks"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MIACODE_BASS_MACOS_LIBRARIES}
            "$<TARGET_FILE_DIR:MiaCode>/../Frameworks"
        COMMENT "Deploying macOS BASS runtime libraries"
    )
    # Bundle the assets directory into the macOS bundle Resources/ so the
    # application is self-contained and can run from any location.
    add_custom_command(TARGET MiaCode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:MiaCode>/../Resources/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:MiaCode>/../Resources/assets"
        COMMENT "Bundling assets directory into macOS bundle"
        VERBATIM
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_target_properties(MiaCode PROPERTIES
        BUILD_RPATH "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
    add_custom_command(TARGET MiaCode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MIACODE_BASS_LINUX_LIBRARIES}
            $<TARGET_FILE_DIR:MiaCode>
        COMMENT "Deploying Linux BASS runtime libraries"
    )
    # Bundle the assets directory so the application is self-contained and
    # can run from any location without relying on the source tree.
    add_custom_command(TARGET MiaCode POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:MiaCode>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:MiaCode>/assets"
        COMMENT "Bundling assets directory into output"
        VERBATIM
    )
endif()

# Extension packages are archive/offline inputs only. Remove any stale
# install-root payload left by an older build so an incremental build cannot
# accidentally turn historical extension folders into shipped product data.
add_custom_command(TARGET MiaCode POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "$<TARGET_FILE_DIR:MiaCode>/extensions"
    COMMENT "Removing archived extension payload from product output"
    VERBATIM
)

if (APPLE)
    target_sources(MiaCode PRIVATE
        src/app/ui/chrome/NativeWindowThemeMac.h
        src/app/ui/chrome/NativeWindowThemeMac.mm
        src/app/ui/chrome/WindowChrome.mm
        src/app/ui/pet/PetNativeWindowMac.mm
    )
    target_link_libraries(MiaCode PRIVATE "-framework AppKit" "-framework QuartzCore")
endif()

# FFmpeg runtime the preview backend (miacode_stage_media) loads next to MiaCode.
    if (WIN32)
        # Runtime: the preview backend loads these next to MiaCode.exe. Resolve
        # the six families from the project FFmpeg 8.1 SDK; the package contract
        # in scripts/build/windows-toolchain.psd1 pins the exact names.
        file(GLOB MIACODE_FFMPEG_WINDOWS_RUNTIME_DLLS
            "${MIACODE_FFMPEG_DEV_DIR}/bin/avcodec-*.dll"
            "${MIACODE_FFMPEG_DEV_DIR}/bin/avformat-*.dll"
            "${MIACODE_FFMPEG_DEV_DIR}/bin/avutil-*.dll"
            "${MIACODE_FFMPEG_DEV_DIR}/bin/swresample-*.dll"
            "${MIACODE_FFMPEG_DEV_DIR}/bin/swscale-*.dll"
            "${MIACODE_FFMPEG_DEV_DIR}/bin/avfilter-*.dll"
        )
        if (NOT MIACODE_FFMPEG_WINDOWS_RUNTIME_DLLS)
            message(FATAL_ERROR
                "No FFmpeg preview runtime DLLs found under '${MIACODE_FFMPEG_DEV_DIR}/bin'.")
        endif()
        add_custom_command(TARGET MiaCode POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MIACODE_FFMPEG_WINDOWS_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:MiaCode>
            VERBATIM
        )
    elseif (APPLE)
        # The project-local macOS SDK has @rpath install names. Make the local
        # app bundle runnable too, then let the release packager stage the
        # same six dylibs in its final Frameworks directory.
        set(MIACODE_MACOS_FFMPEG_RUNTIME_LIBRARIES
            "${MIACODE_FFMPEG_DEV_DIR}/lib/libavcodec.62.dylib"
            "${MIACODE_FFMPEG_DEV_DIR}/lib/libavfilter.11.dylib"
            "${MIACODE_FFMPEG_DEV_DIR}/lib/libavformat.62.dylib"
            "${MIACODE_FFMPEG_DEV_DIR}/lib/libavutil.60.dylib"
            "${MIACODE_FFMPEG_DEV_DIR}/lib/libswresample.6.dylib"
            "${MIACODE_FFMPEG_DEV_DIR}/lib/libswscale.9.dylib"
        )
        foreach(MIACODE_MACOS_FFMPEG_RUNTIME_LIBRARY IN LISTS MIACODE_MACOS_FFMPEG_RUNTIME_LIBRARIES)
            if (NOT EXISTS "${MIACODE_MACOS_FFMPEG_RUNTIME_LIBRARY}")
                message(FATAL_ERROR
                    "macOS FFmpeg SDK is missing required runtime library: "
                    "${MIACODE_MACOS_FFMPEG_RUNTIME_LIBRARY}")
            endif()
        endforeach()
        set_property(TARGET MiaCode APPEND PROPERTY BUILD_RPATH
            "@executable_path/../Frameworks")
        set_property(TARGET MiaCode APPEND PROPERTY INSTALL_RPATH
            "@executable_path/../Frameworks")
        add_custom_command(TARGET MiaCode POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:MiaCode>/../Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${MIACODE_MACOS_FFMPEG_RUNTIME_LIBRARIES}
                "$<TARGET_FILE_DIR:MiaCode>/../Frameworks"
            VERBATIM
        )
    endif()

if (MSVC)
    target_compile_options(MiaCode PRIVATE /FS)
endif()

# Tiny launcher exe that lives at the dist-package root and forwards
# execution to app/MiaCode.exe. Lets the package keep the ~40 Qt/BASS/
# Qt6Multimedia-ffmpeg/D3D DLLs inside app/ rather than scattering them
# across the root the user sees in File Explorer. Built only on Windows
# — other platforms ship MiaCode.exe at the bundle root directly.
if (WIN32)
    add_executable(MiaCodeLauncher WIN32
        src/wrapper/MiaCodeLauncher.cpp
        # P2 — export the same NVIDIA Optimus / AMD PowerXpress high-performance
        # GPU hint symbols from the launcher's export table as well as the real
        # app, so the preference is declared whichever binary the OS inspects.
        src/app/gpu_high_performance_hint.cpp
    )
    set_target_properties(MiaCodeLauncher PROPERTIES
        OUTPUT_NAME "MiaCodeLauncher"
        # Static-link the MSVC runtime so the launcher itself has zero
        # MSVCP140/VCRUNTIME140 dependency. Without this, the preflight
        # probe is useless: the launcher would itself fail to load when
        # the user is missing the same DLLs we're trying to detect for
        # them. Pure Win32 from here on — kernel32 + user32 only.
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    if (MSVC)
        # /utf-8: parse source as UTF-8 (default would be the system
        # codepage — GBK on zh-CN Windows — which mangles the CJK
        # characters embedded in the bilingual MessageBox strings).
        # Without this, MessageBoxW shows garbage where Chinese should
        # be. Only needed on the launcher since the main MiaCode target
        # routes all user-visible text through Qt's tr() / QStringLiteral
        # which encodes correctly regardless of compiler defaults.
        target_compile_options(MiaCodeLauncher PRIVATE /utf-8)
    endif()
    if (MINGW)
        # MinGW counterpart of MSVC_RUNTIME_LIBRARY MultiThreaded above: the
        # launcher sits at the dist root while the packaged libstdc++-6 /
        # libgcc_s_seh-1 / libwinpthread-1 DLLs live in app/, so the launcher
        # has to carry no MinGW runtime dependency of its own. -municode
        # selects the wWinMain entry point the source defines; MinGW's default
        # crt startup looks for WinMain and fails to link without it.
        target_link_options(MiaCodeLauncher PRIVATE -municode -static -static-libgcc -static-libstdc++)
    endif()
    # Shared app icon — the wrapper exe lives at the dist root and is the
    # user-clickable face of MiaCode in File Explorer, so it must show
    # the same icon as the real binary inside app/. Reuses the generated
    # .rc emitted alongside the MiaCode target above.
    if (EXISTS "${MIACODE_APP_ICON}" AND DEFINED MIACODE_APP_RC)
        target_sources(MiaCodeLauncher PRIVATE "${MIACODE_APP_RC}")
    endif()
endif()

if (MIACODE_BUILD_DEV_TOOLS)
    include(cmake/devtools/DevTools.cmake)
endif()

if (WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${QT_BIN_DIR}")
    set(MIACODE_REQUIRED_QT_RUNTIME_DLLS
        Qt6Quick
        Qt6Qml
        Qt6QmlMeta
        Qt6QmlModels
        Qt6QmlWorkerScript
    )
    set(MIACODE_REMOVED_QT_RUNTIME_DLLS
        Qt6OpenGLWidgets
        Qt6Concurrent
    )

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET MiaCode POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                "$<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>"
                --compiler-runtime
                --no-translations
                --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/src"
                "$<TARGET_FILE:MiaCode>"
            COMMENT "Deploying Qt runtime next to MiaCode.exe"
            VERBATIM
        )
        foreach(runtime_dll_base IN LISTS MIACODE_REQUIRED_QT_RUNTIME_DLLS)
            add_custom_command(TARGET MiaCode POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${QT_BIN_DIR}/${runtime_dll_base}$<$<CONFIG:Debug>:d>.dll"
                    "$<TARGET_FILE_DIR:MiaCode>/${runtime_dll_base}$<$<CONFIG:Debug>:d>.dll"
                VERBATIM
            )
        endforeach()
        foreach(runtime_dll_base IN LISTS MIACODE_REMOVED_QT_RUNTIME_DLLS)
            add_custom_command(TARGET MiaCode POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E rm -f
                    "$<TARGET_FILE_DIR:MiaCode>/${runtime_dll_base}$<$<CONFIG:Debug>:d>.dll"
                VERBATIM
            )
        endforeach()
    else()
        message(WARNING "windeployqt not found. Qt runtime DLLs will not be auto-deployed.")
    endif()
endif()
