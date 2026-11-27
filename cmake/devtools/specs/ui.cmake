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
        src/app/ui/preferences/ApplicationMenuTranslation.cpp
        src/app/services/UiRequestService.h
        src/app/services/UiRequestService.cpp
    LIBS miacode_base Qt6::Core Qt6::Gui Qt6::Qml
    INCLUDES src
)
if(APPLE)
    target_sources(qml_app_background_model_spec PRIVATE src/app/ui/preferences/ApplicationMenuTranslationMac.mm)
    target_link_libraries(qml_app_background_model_spec PRIVATE "-framework AppKit")
endif()

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
        src/app/ui/preferences/ApplicationMenuTranslation.cpp
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
if(APPLE)
    target_sources(document_model_save_spec PRIVATE src/app/ui/preferences/ApplicationMenuTranslationMac.mm)
    target_link_libraries(document_model_save_spec PRIVATE "-framework AppKit")
endif()

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
        src/app/ui/preferences/ApplicationMenuTranslation.cpp
    LIBS miacode_base Qt6::Core Qt6::Qml
    INCLUDES src
)
if(APPLE)
    target_sources(timeline_surface_ready_spec PRIVATE src/app/ui/preferences/ApplicationMenuTranslationMac.mm)
    target_link_libraries(timeline_surface_ready_spec PRIVATE "-framework AppKit")
endif()

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
        src/app/ui/preferences/ApplicationMenuTranslation.cpp
        src/app/services/PreferenceDocument.cpp
        src/app/services/ProjectPreferences.cpp
        src/app/services/UiRequestService.cpp
        src/app/services/UiRequestService.h
    LIBS miacode_export miacode_preview_quickplugin Qt6::Quick
    INCLUDES src
)
target_compile_definitions(cover_export_session_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
if(APPLE)
    target_sources(cover_export_session_spec PRIVATE src/app/ui/preferences/ApplicationMenuTranslationMac.mm)
    target_link_libraries(cover_export_session_spec PRIVATE "-framework AppKit")
endif()

miacode_add_dev_tool(qml_preview_rate_feedback_spec TEST
    SOURCES
        src/tools/ui/QmlPreviewRateFeedbackSpec.cpp
        src/app/ui/preview/PreviewModel.cpp
        src/app/ui/preview/PreviewModel.h
        src/app/ui/preferences/LocaleService.cpp
        src/app/ui/preferences/LocaleService.h
        src/app/ui/preferences/ApplicationMenuTranslation.cpp
        src/app/services/PreferenceDocument.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/ShellNotifications.cpp
        src/app/services/ShellNotifications.h
        src/app/services/PlaybackControl.h
        src/app/services/PreviewSurface.h
    LIBS miacode_base Qt6::Core Qt6::Gui Qt6::Qml Qt6::Test
    INCLUDES src
)
if(APPLE)
    target_sources(qml_preview_rate_feedback_spec PRIVATE src/app/ui/preferences/ApplicationMenuTranslationMac.mm)
    target_link_libraries(qml_preview_rate_feedback_spec PRIVATE "-framework AppKit")
endif()
