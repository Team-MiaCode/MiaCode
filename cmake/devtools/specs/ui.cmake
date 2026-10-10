# Explicit spec targets; contract IDs stay stable across source/target renames.

miacode_add_spec(qml_app_background_model_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-app-background-model
    DOMAIN ui KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
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

miacode_add_spec(qml_chart_drop_bridge_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-chart-drop-bridge
    DOMAIN ui KIND integration RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlChartDropBridgeSpec.cpp
        src/app/ui/drop/ChartDropBridge.h
        src/app/ui/drop/ChartDropBridge.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Quick
    INCLUDES src
)

miacode_add_spec(qml_document_projection_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-document-projection
    DOMAIN ui KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlDocumentProjectionSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/DocumentProjection.cpp
        src/app/services/DocumentProjection.h
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_spec(qml_analysis_model_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-analysis-model
    DOMAIN ui KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlAnalysisModelSpec.cpp
        src/app/services/AnalysisProjection.cpp
        src/app/services/AnalysisProjection.h
        src/app/services/DocumentProjection.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(qml_shortcut_binding_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-shortcut-binding
    DOMAIN ui KIND integration RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlShortcutBindingSpec.cpp
        src/app/ui/chrome/ShortcutModel.cpp
        src/app/ui/chrome/ShortcutModel.h
        src/app/ui/chrome/ShortcutCommands.h
        src/app/ui/document/ChartTransformCommands.h
        src/app/services/ShortcutRegistry.cpp
        src/app/services/ShortcutRegistry.h
        resources/app_icons.qrc
    LIBS miacode_chart Qt6::Core Qt6::Gui
    INCLUDES src
)
target_compile_definitions(qml_shortcut_binding_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(v1_shell_removal_spec
    OWNER src/app/ui
    CONTRACT qml-ui.v1-shell-removal
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/V1ShellRemovalSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(v1_shell_removal_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(preview_transport_push_spec
    OWNER src/app/ui
    CONTRACT qml-ui.preview-transport-push
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/PreviewTransportPushSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(preview_transport_push_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# The timeline half of the same push: the QSG surface has to tell the window
# it is writable before playback may move the playhead. Links the real
# TimelineModel so the readiness report is exercised through the contract
# rather than described.
miacode_add_spec(timeline_surface_ready_spec
    OWNER src/app/ui
    CONTRACT qml-ui.timeline-surface-ready
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
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
target_compile_definitions(timeline_surface_ready_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_ui_bootstrap_lifecycle_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-ui-bootstrap-lifecycle
    DOMAIN ui KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlUiBootstrapLifecycleSpec.cpp
        src/app/ui/shell/RootLifecycle.h
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(qml_ui_bootstrap_lifecycle_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_document_lifecycle_contract_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-document-lifecycle-contract
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlDocumentLifecycleContractSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(qml_document_lifecycle_contract_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_export_intro_sound_contract_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-export-intro-sound-contract
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlExportIntroSoundContractSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(qml_export_intro_sound_contract_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_export_font_contract_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-export-font-contract
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlExportFontContractSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(qml_export_font_contract_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_selection_range_export_contract_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-selection-range-export-contract
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlSelectionRangeExportContractSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(qml_selection_range_export_contract_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_cover_export_contract_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-cover-export-contract
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlCoverExportContractSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(qml_cover_export_contract_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_export_video_page_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-export-video-page
    DOMAIN ui KIND integration RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlExportVideoPageSpec.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Test
    INCLUDES src
)
target_compile_definitions(qml_export_video_page_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\"")

miacode_add_spec(qml_preview_rate_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-preview-rate
    DOMAIN ui KIND integration RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlPreviewRateSpec.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    INCLUDES src
)
target_compile_definitions(qml_preview_rate_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\"")

miacode_add_spec(qml_preview_rate_feedback_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-preview-rate-feedback
    DOMAIN ui KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
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

miacode_add_spec(qml_main_menu_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-main-menu
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlMainMenuSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(qml_main_menu_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_net_page_compile_spec
    OWNER src/app/ui
    CONTRACT net.page-compilation
    DOMAIN ui KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/ui/QmlNetPageCompileSpec.cpp
    LIBS Qt6::Core Qt6::Qml
    INCLUDES src
)
target_compile_definitions(qml_net_page_compile_spec PRIVATE
    "MIACODE_QML_SPEC_IMPORT_ROOT=\"${MIACODE_QML_SPEC_IMPORT_ROOT}\"")

miacode_add_spec(qml_net_tabs_spec
    OWNER src/app/ui
    CONTRACT net.tab-lifecycle
    DOMAIN ui KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/ui/QmlNetTabsSpec.cpp
    LIBS Qt6::Core Qt6::Qml
    INCLUDES src
)
target_compile_definitions(qml_net_tabs_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(qml_document_replacement_sequence_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-document-replacement-sequence
    DOMAIN ui KIND integration RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlDocumentReplacementSequenceSpec.cpp
    LIBS Qt6::Core Qt6::Qml Qt6::Quick
    INCLUDES src
)
target_compile_definitions(qml_document_replacement_sequence_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Ratchet for docs/specs/ui/UI_BACKEND_SURFACE_ZH.md (stage 3.5, item 2).
# Set-equality between the MainWindow surface src/app/ui actually reaches
# and the inventory the doc lists, plus the friend grants and the recorded
# counts. New coupling fails; a migration that forgets the doc fails too,
# which is what keeps the remaining count honest and monotonic.
miacode_add_spec(qml_ui_backend_surface_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-ui-backend-surface
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlUiBackendSurfaceSpec.cpp
    LIBS Qt6::Core
)
target_compile_definitions(qml_ui_backend_surface_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# A theme change has two entry points — the OS colour scheme and the user's
# own choice — and only the first used to notify QML, so picking a theme
# repainted the timeline and nothing else. Source contract because
# WorkbenchSettings pulls in MainWindowShared and cannot link Core-only.
miacode_add_spec(qml_ui_theme_contract_spec
    OWNER src/app/ui
    CONTRACT qml-ui.qml-ui-theme-contract
    DOMAIN ui KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/QmlUiThemeContractSpec.cpp
    LIBS Qt6::Core
)
target_compile_definitions(qml_ui_theme_contract_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
