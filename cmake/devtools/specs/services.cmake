miacode_add_dev_tool(net_api_contract_spec TEST
    SOURCES
        src/tools/services/NetApiContractSpec.cpp
        src/app/services/api/ApiCatalog.cpp
        src/app/services/api/ApiDispatcher.cpp
        src/app/services/net/NetService.cpp
        src/app/services/net/NetAccountStore.cpp
        src/app/services/net/NetService.h
        src/app/services/jobs/JobRegistry.cpp
        src/app/services/jobs/JobRegistry.h
    LIBS miacode_media_tools Qt6::Core $<$<PLATFORM_ID:Windows>:Advapi32>
    INCLUDES src
)

miacode_add_dev_tool(job_registry_spec TEST
    SOURCES
        src/tools/services/JobRegistrySpec.cpp
        src/app/services/jobs/JobRegistry.cpp
        src/app/services/jobs/JobRegistry.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(net_service_spec TEST
    SOURCES
        src/tools/services/NetServiceSpec.cpp
        src/app/services/api/ApiCatalog.cpp
        src/app/services/net/NetService.cpp
        src/app/services/net/NetAccountStore.cpp
        src/app/services/net/NetService.h
        src/app/services/jobs/JobRegistry.cpp
        src/app/services/jobs/JobRegistry.h
    LIBS Qt6::Core $<$<PLATFORM_ID:Windows>:Advapi32>
    INCLUDES src
)

miacode_add_dev_tool(chart_workspace_spec TEST
    SOURCES
        src/tools/services/ChartWorkspaceSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(chart_media_service_spec TEST
    SOURCES
        src/tools/services/ChartMediaServiceSpec.cpp
        src/app/services/ChartMediaService.cpp
        src/app/services/ChartMediaService.h
        src/app/services/ChartMediaImport.cpp
        src/app/services/ChartMediaImport.h
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(chart_workspace_file_service_spec TEST
    SOURCES
        src/tools/services/ChartWorkspaceFileServiceSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/ChartWorkspaceFileService.cpp
        src/app/services/ChartWorkspaceFileService.h
    LIBS miacode_chart Qt6::Core Qt6::Gui
    INCLUDES src
)

# Qt6::Core + Qt6::Test only: this target failing to link is how we notice a
# QFileDialog / QMessageBox creeping back into the request boundary.
miacode_add_dev_tool(ui_request_service_spec TEST
    SOURCES
        src/tools/services/UiRequestServiceSpec.cpp
        src/app/services/UiRequestService.cpp
        src/app/services/UiRequestService.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(editor_sync_controller_spec TEST
    SOURCES
        src/tools/services/EditorSyncControllerSpec.cpp
        src/app/services/EditorSyncController.cpp
        src/app/services/EditorSyncController.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(job_progress_service_spec TEST
    SOURCES
        src/tools/services/JobProgressServiceSpec.cpp
        src/app/services/JobProgressService.cpp
        src/app/services/JobProgressService.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(analysis_service_spec TEST
    SOURCES
        src/tools/services/AnalysisServiceSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/AnalysisService.cpp
        src/app/services/AnalysisService.h
    LIBS miacode_analysis Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(editor_page_router_spec TEST
    SOURCES
        src/tools/services/EditorPageRouterSpec.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/PreferenceDocument.cpp
        src/app/services/ApplicationServices.h
        src/app/services/ApplicationServices.cpp
        src/app/services/AnalysisService.h
        src/app/services/AnalysisService.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspaceFileService.h
        src/app/services/ChartWorkspaceFileService.cpp
        src/app/services/EditorPageRouter.h
        src/app/services/EditorSyncController.h
        src/app/services/EditorSyncController.cpp
        src/app/services/JobProgressService.h
        src/app/services/JobProgressService.cpp
        src/app/services/PreviewAppearanceState.h
        src/app/services/PreviewAppearanceState.cpp
        src/app/services/ShellNotifications.h
        src/app/services/ShellNotifications.cpp
        src/app/services/UiRequestService.h
        src/app/services/UiRequestService.cpp
    LIBS miacode_analysis Qt6::Core Qt6::Gui Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(export_engine_spec TEST
    SOURCES
        src/tools/services/ExportEngineSpec.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/PreferenceDocument.cpp
        src/app/services/ApplicationServices.h
        src/app/services/ApplicationServices.cpp
        src/app/services/AnalysisService.h
        src/app/services/AnalysisService.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspaceFileService.h
        src/app/services/ChartWorkspaceFileService.cpp
        src/app/services/EditorSyncController.h
        src/app/services/EditorSyncController.cpp
        src/app/services/ExportEngine.h
        src/app/services/JobProgressService.h
        src/app/services/JobProgressService.cpp
        src/app/services/PreviewAppearanceState.h
        src/app/services/PreviewAppearanceState.cpp
        src/app/services/ShellNotifications.h
        src/app/services/ShellNotifications.cpp
        src/app/services/UiRequestService.h
        src/app/services/UiRequestService.cpp
    LIBS miacode_analysis Qt6::Core Qt6::Gui Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(preview_appearance_state_spec TEST
    SOURCES
        src/tools/services/PreviewAppearanceStateSpec.cpp
        src/app/services/PreviewAppearanceState.h
        src/app/services/PreviewAppearanceState.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(application_services_spec TEST
    SOURCES
        src/tools/services/ApplicationServicesSpec.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/PreferenceDocument.cpp
        src/app/services/ApplicationServices.h
        src/app/services/ApplicationServices.cpp
        src/app/services/AnalysisService.h
        src/app/services/AnalysisService.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspaceFileService.h
        src/app/services/ChartWorkspaceFileService.cpp
        src/app/services/EditorSyncController.h
        src/app/services/EditorSyncController.cpp
        src/app/services/JobProgressService.h
        src/app/services/JobProgressService.cpp
        src/app/services/PreviewAppearanceState.h
        src/app/services/PreviewAppearanceState.cpp
        src/app/services/ShellNotifications.h
        src/app/services/ShellNotifications.cpp
        src/app/services/UiRequestService.h
        src/app/services/UiRequestService.cpp
    LIBS miacode_analysis Qt6::Core Qt6::Gui Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(timeline_host_spec TEST
    SOURCES
        src/tools/services/TimelineHostSpec.cpp
        src/app/runtime/timeline/TimelineCommandGate.cpp
        src/app/runtime/timeline/TimelineCommandGate.h
        src/app/runtime/timeline/TimelineHost.cpp
        src/app/runtime/timeline/TimelineHost.h
        src/app/services/TimelineSurface.h
        src/app/services/SessionGeneration.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_host_spec TEST
    SOURCES
        src/tools/services/PreviewHostSpec.cpp
        src/app/runtime/preview/PreviewHost.cpp
        src/app/runtime/preview/PreviewHost.h
        src/app/services/AudioClockSource.h
        src/app/services/PreviewPlaybackPort.h
        src/app/services/PlaybackControl.h
        src/app/services/PreviewSurface.h
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

# Playback callback identity acceptance.
miacode_add_dev_tool(playback_coordinator_spec TEST
    SOURCES
        src/tools/services/PlaybackCoordinatorSpec.cpp
        src/app/runtime/playback/PlaybackIdentityGate.cpp
        src/app/runtime/playback/PlaybackIdentityGate.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(update_version_spec TEST
    SOURCES
        src/tools/update/UpdateVersionSpec.cpp
        src/app/services/update/SemanticVersion.cpp
        src/app/services/update/SemanticVersion.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(update_manifest_spec TEST
    SOURCES
        src/tools/update/UpdateManifestSpec.cpp
        src/app/services/update/UpdateManifest.cpp
        src/app/services/update/UpdateManifest.h
        src/app/services/update/SemanticVersion.cpp
        src/app/services/update/SemanticVersion.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(update_service_spec TEST
    SOURCES
        src/tools/update/UpdateServiceSpec.cpp
        src/app/services/update/UpdateService.cpp
        src/app/services/update/UpdateService.h
        src/app/services/update/UpdateManifest.cpp
        src/app/services/update/UpdateManifest.h
        src/app/services/update/SemanticVersion.cpp
        src/app/services/update/SemanticVersion.h
    LIBS Qt6::Core Qt6::Gui Qt6::Test
    INCLUDES src
)

miacode_add_dev_tool(preference_json_file_spec TEST
    SOURCES src/tools/services/PreferenceJsonFileSpec.cpp
    LIBS miacode_base Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(project_preference_read_recovery_spec TEST
    SOURCES
        src/tools/services/ProjectPreferenceReadRecoverySpec.cpp
        src/app/services/ProjectPreferences.cpp
        src/app/services/ProjectPreferences.h
    LIBS miacode_base miacode_audio Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(net_configuration_spec TEST
    SOURCES
        src/tools/services/NetConfigurationSpec.cpp
        src/app/services/net/NetConfiguration.cpp
        src/app/services/net/NetConfiguration.h
        src/app/services/net/NetAccountStore.cpp
        src/app/services/PreferenceDocument.cpp
    LIBS miacode_base Qt6::Core $<$<PLATFORM_ID:Windows>:Advapi32>
    INCLUDES src
)

miacode_add_dev_tool(preference_repository_spec TEST
    SOURCES
        src/tools/services/PreferenceRepositorySpec.cpp
        src/app/services/PreferenceDocument.cpp
        src/app/services/PreferenceDocument.h
    LIBS miacode_base Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(export_preferences_spec TEST
    SOURCES src/tools/services/ExportPreferencesSpec.cpp
    LIBS miacode_export Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(shortcut_preference_persistence_spec TEST
    SOURCES
        src/tools/services/ShortcutPreferencePersistenceSpec.cpp
        src/app/services/ShortcutRegistry.cpp
        src/app/services/ShortcutRegistry.h
        src/app/ui/chrome/ShortcutModel.cpp
        src/app/ui/chrome/ShortcutModel.h
        resources/app_icons.qrc
    LIBS miacode_base Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(hud_font_preference_migration_spec TEST
    SOURCES
        src/tools/services/HudFontPreferenceMigrationSpec.cpp
        src/app/services/PreferenceDocument.cpp
        src/app/services/PreferenceDocument.h
    LIBS miacode_scene miacode_base Qt6::Core Qt6::Gui
    INCLUDES src
)
target_compile_definitions(hud_font_preference_migration_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
