miacode_add_dev_tool(qml_scintilla_editor_spec TEST
    SOURCES
        src/tools/ui/QmlScintillaEditorSpec.cpp
        src/app/ui/editor/ScintillaDocumentAdapter.cpp
        src/app/ui/editor/ScintillaDslStyler.cpp
    LIBS miacode_editor_core ScintillaQuick::ScintillaQuick Qt6::Quick
    INCLUDES src
        third_party/ScintillaQuick/tests/support
        third_party/ScintillaQuick/src/core
        third_party/ScintillaQuick/src/platform
        third_party/ScintillaQuick/src/render
)
set_tests_properties(qml_scintilla_editor_spec PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
target_compile_definitions(qml_scintilla_editor_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\""
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_dev_tool(qml_app_background_model_spec TEST
    SOURCES
        src/tools/ui/QmlAppBackgroundModelSpec.cpp
        src/app/ui/preferences/AppBackgroundModel.h
        src/app/ui/preferences/AppBackgroundModel.cpp
        src/app/ui/preferences/AppBackgroundSettings.h
        src/app/ui/preferences/AppBackgroundSettings.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/PreferenceDocument.cpp
        src/app/ui/preferences/LocaleService.h
        src/app/ui/preferences/LocaleService.cpp
        src/app/services/UiRequestService.h
        src/app/services/UiRequestService.cpp
    LIBS miacode_base Qt6::Core Qt6::Gui Qt6::Qml
    INCLUDES src
)

miacode_add_dev_tool(qml_chart_drop_bridge_spec TEST
    SOURCES
        src/tools/ui/QmlChartDropBridgeSpec.cpp
        src/app/ui/drop/ChartDropBridge.h
        src/app/ui/drop/ChartDropBridge.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Quick
    INCLUDES src
)

miacode_add_dev_tool(document_model_save_spec TEST
    SOURCES
        src/tools/ui/DocumentModelSaveSpec.cpp
        src/app/ui/document/DocumentModel.cpp
        src/app/ui/document/DocumentModel.h
        src/app/ui/preferences/LocaleService.cpp
        src/app/ui/preferences/LocaleService.h
        src/app/services/PreferenceDocument.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/ChartWorkspaceFileService.cpp
        src/app/services/DocumentProjection.cpp
        src/app/services/AnalysisService.cpp
        src/app/services/AnalysisService.h
        src/app/services/ShellNotifications.cpp
        src/app/services/ShellNotifications.h
        src/app/services/UiRequestService.cpp
        src/app/services/UiRequestService.h
        src/app/services/ChartMediaService.cpp
        src/app/services/ChartMediaImport.cpp
        src/app/services/Id3TagReader.cpp
        src/app/services/ProjectPreferences.cpp
    LIBS miacode_analysis miacode_audio miacode_editor_core Qt6::Core Qt6::Gui Qt6::Qml
    INCLUDES src
)

miacode_add_dev_tool(qml_document_projection_spec TEST
    SOURCES
        src/tools/ui/QmlDocumentProjectionSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/DocumentProjection.cpp
        src/app/services/DocumentProjection.h
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(qml_analysis_model_spec TEST
    SOURCES
        src/tools/ui/QmlAnalysisModelSpec.cpp
        src/app/services/AnalysisProjection.cpp
        src/app/services/AnalysisProjection.h
        src/app/services/DocumentProjection.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(qml_shortcut_binding_spec TEST
    SOURCES
        src/tools/ui/QmlShortcutBindingSpec.cpp
        src/app/ui/chrome/ShortcutModel.cpp
        src/app/ui/chrome/ShortcutModel.h
        src/app/ui/chrome/ShortcutCommands.h
        src/app/ui/document/ChartTransformCommands.h
        src/app/services/ShortcutRegistry.cpp
        src/app/services/ShortcutRegistry.h
        resources/app_icons.qrc
    LIBS miacode_chart Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test
    INCLUDES src
)
target_compile_definitions(qml_shortcut_binding_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# The timeline half of the same push: the QSG surface has to tell the window
# it is writable before playback may move the playhead. Links the real
# TimelineModel so the readiness report is exercised through the contract
# rather than described.
miacode_add_dev_tool(timeline_surface_ready_spec TEST
    SOURCES
        src/tools/ui/TimelineSurfaceReadySpec.cpp
        src/app/ui/timeline/TimelineModel.cpp
        src/app/ui/timeline/TimelineModel.h
        src/app/services/ShellNotifications.cpp
        src/app/services/ShellNotifications.h
        src/app/services/TimelineSurface.h
        src/app/services/PreferenceDocument.cpp
        src/app/services/PreferenceDocument.h
        src/app/ui/preferences/LocaleService.cpp
        src/app/ui/preferences/LocaleService.h
    LIBS miacode_base Qt6::Core Qt6::Qml
    INCLUDES src
)

miacode_add_dev_tool(qml_ui_bootstrap_lifecycle_spec TEST
    SOURCES
        src/tools/ui/QmlUiBootstrapLifecycleSpec.cpp
        src/app/ui/shell/RootLifecycle.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(cover_export_session_spec TEST
    SOURCES
        src/tools/ui/CoverExportSessionSpec.cpp
        src/app/ui/export/CoverExportSession.cpp
        src/app/ui/export/CoverExportSession.h
        src/app/ui/preferences/LocaleService.cpp
        src/app/ui/preferences/LocaleService.h
        src/app/services/PreferenceDocument.cpp
        src/app/services/ProjectPreferences.cpp
        src/app/services/UiRequestService.cpp
        src/app/services/UiRequestService.h
    LIBS miacode_export Qt6::Quick
    INCLUDES src
)
target_compile_definitions(cover_export_session_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_dev_tool(qml_export_video_page_spec TEST
    SOURCES
        src/tools/ui/QmlExportVideoPageSpec.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test
    INCLUDES src
)
target_compile_definitions(qml_export_video_page_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\"")

miacode_add_dev_tool(qml_preview_rate_spec TEST
    SOURCES
        src/tools/ui/QmlPreviewRateSpec.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    INCLUDES src
)
target_compile_definitions(qml_preview_rate_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\"")

miacode_add_dev_tool(qml_ui_scale_spec TEST
    SOURCES src/tools/ui/QmlUiScaleSpec.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test
    INCLUDES src
)
target_compile_definitions(qml_ui_scale_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\"")

miacode_add_dev_tool(qml_preview_rate_feedback_spec TEST
    SOURCES
        src/tools/ui/QmlPreviewRateFeedbackSpec.cpp
        src/app/ui/preview/PreviewModel.cpp
        src/app/ui/preview/PreviewModel.h
        src/app/ui/preferences/LocaleService.cpp
        src/app/ui/preferences/LocaleService.h
        src/app/services/PreferenceDocument.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/ShellNotifications.cpp
        src/app/services/ShellNotifications.h
        src/app/services/PlaybackControl.h
        src/app/services/PreviewSurface.h
    LIBS miacode_base Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(qml_net_page_compile_spec TEST
    SOURCES src/tools/ui/QmlNetPageCompileSpec.cpp
    LIBS Qt6::Core Qt6::Qml
    INCLUDES src
)
target_compile_definitions(qml_net_page_compile_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\"")

miacode_add_dev_tool(qml_net_tabs_spec TEST
    SOURCES src/tools/ui/QmlNetTabsSpec.cpp
    LIBS Qt6::Core Qt6::Qml
    INCLUDES src
)
target_compile_definitions(qml_net_tabs_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_dev_tool(qml_document_replacement_sequence_spec TEST
    SOURCES
        src/tools/ui/QmlDocumentReplacementSequenceSpec.cpp
    LIBS Qt6::Core Qt6::Qml Qt6::Quick
    INCLUDES src
)
target_compile_definitions(qml_document_replacement_sequence_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_dev_tool(latency_model_spec TEST
    SOURCES
        src/tools/ui/LatencyModelSpec.cpp
        src/app/ui/latency/LatencyModel.cpp
        src/app/ui/latency/LatencyModel.h
        src/app/runtime/latency/LatencySandboxController.h
    LIBS Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    INCLUDES src
)
set_tests_properties(latency_model_spec PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
target_compile_definitions(latency_model_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\"")
