# Explicit spec targets; contract IDs stay stable across source/target renames.

miacode_add_spec(net_api_contract_spec
    OWNER src/app/services/api
    CONTRACT net.api-contract
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
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

miacode_add_spec(job_registry_spec
    OWNER src/app/services/jobs
    CONTRACT jobs.registry
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/JobRegistrySpec.cpp
        src/app/services/jobs/JobRegistry.cpp
        src/app/services/jobs/JobRegistry.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(net_service_spec
    OWNER src/app/services/net
    CONTRACT net.application-service
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
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

miacode_add_spec(chart_workspace_spec
    OWNER src/app/services
    CONTRACT v2.chart-workspace
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/ChartWorkspaceSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_spec(chart_media_service_spec
    OWNER src/app/services
    CONTRACT v2.chart-media-service
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/ChartMediaServiceSpec.cpp
        src/app/services/ChartMediaService.cpp
        src/app/services/ChartMediaService.h
        src/app/services/ChartMediaImport.cpp
        src/app/services/ChartMediaImport.h
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(chart_workspace_file_service_spec
    OWNER src/app/services
    CONTRACT v2.chart-workspace-file-service
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/ChartWorkspaceFileServiceSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/ChartWorkspaceFileService.cpp
        src/app/services/ChartWorkspaceFileService.h
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

# Qt6::Core + Qt6::Test only: this target failing to link is how we notice a
# QFileDialog / QMessageBox creeping back into the request boundary.
miacode_add_spec(ui_request_service_spec
    OWNER src/app/services
    CONTRACT v2.ui-request-service
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/UiRequestServiceSpec.cpp
        src/app/services/UiRequestService.cpp
        src/app/services/UiRequestService.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

miacode_add_spec(editor_sync_controller_spec
    OWNER src/app/services
    CONTRACT v2.editor-sync-controller
    DOMAIN services KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/EditorSyncControllerSpec.cpp
        src/app/services/EditorSyncController.cpp
        src/app/services/EditorSyncController.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

miacode_add_spec(job_progress_service_spec
    OWNER src/app/services
    CONTRACT v2.job-progress-service
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/JobProgressServiceSpec.cpp
        src/app/services/JobProgressService.cpp
        src/app/services/JobProgressService.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

miacode_add_spec(analysis_service_spec
    OWNER src/app/services
    CONTRACT v2.analysis-service
    DOMAIN services KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/AnalysisServiceSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/AnalysisService.cpp
        src/app/services/AnalysisService.h
    LIBS miacode_analysis Qt6::Core Qt6::Gui
    INCLUDES src
)

# Stage 3.5 items 2-3: the editor page-routing seam. EditorPageRouter is not
# even a QObject, so anything Qt-GUI-shaped creeping into the contract fails
# to link here.
miacode_add_spec(editor_page_router_spec
    OWNER src/app/services
    CONTRACT v2.editor-page-router
    DOMAIN services KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
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
target_compile_definitions(editor_page_router_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Stage 3.5 item 2: the export page's engine seam. The implementation still
# lives inside a QMainWindow, so linking Core+Gui+Test only is how a
# QtWidgets type creeping into the contract gets caught.
miacode_add_spec(export_engine_spec
    OWNER src/app/services
    CONTRACT v2.export-engine
    DOMAIN services KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
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
target_compile_definitions(export_engine_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Stage 3.5 item 2: the preview appearance settings must have an owner that
# is not a window. Core+Gui+Test only — a QtWidgets or Qt Quick include
# reaching this state fails to link here.
miacode_add_spec(preview_appearance_state_spec
    OWNER src/app/services
    CONTRACT v2.preview-appearance-state
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/PreviewAppearanceStateSpec.cpp
        src/app/services/PreviewAppearanceState.h
        src/app/services/PreviewAppearanceState.cpp
    LIBS Qt6::Core Qt6::Gui Qt6::Test
    INCLUDES src
)

# Stage 3.5 item 1: the application service assembly must stand up with no
# window and no QApplication. Linking Qt6::Core + Qt6::Gui only is the
# guarantee — a QtWidgets include reaching ApplicationServices fails to link
# here rather than silently re-coupling the document domain to the shell.
miacode_add_spec(application_services_spec
    OWNER src/app/services
    CONTRACT v2.application-services
    DOMAIN services KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
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
target_compile_definitions(application_services_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Stage 4.9d-4b-2a: the playback coordinator's first narrow port onto
# preferences and persisted state. Session implements it, but the port
# itself must not need Session, QWidget, or QML/QSG to be implemented —
# linking Core+Test only (no Gui) is the guarantee: if the port ever grows
# a method whose type reaches beyond Qt6::Core, this target fails to LINK.
miacode_add_spec(preferences_port_spec
    OWNER src/app/services
    CONTRACT v2.preferences-port
    DOMAIN services KIND boundary RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/PreferencesPortSpec.cpp
        src/app/services/PlaybackPreferencesPort.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

# Stage 4.9d-4b-2b: the playback coordinator's second narrow port, this one
# onto muri validation/analysis presentation. All five methods already
# belong to one host (ValidationHost), so the port is cut by host rather
# than by capability — see PlaybackValidationPort.h. Linking Core+Test only
# (no Gui) is the same link-time guarantee as preferences_port_spec above.
miacode_add_spec(validation_port_spec
    OWNER src/app/services
    CONTRACT v2.validation-port
    DOMAIN services KIND boundary RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/ValidationPortSpec.cpp
        src/app/services/PlaybackValidationPort.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

# Stage 4.9d-4b-2c: the playback coordinator's third narrow port, onto
# document state (dirty tracking, committing the field QML is holding,
# editor navigation, and a read-only query for the applied workspace
# revision). All four methods already belong to one host
# (DocumentSessionHost), so the port is cut by host, same as
# validation_port_spec above. Linking Core+Test only (no Gui) is the same
# link-time guarantee.
miacode_add_spec(document_port_spec
    OWNER src/app/services
    CONTRACT v2.document-port
    DOMAIN services KIND boundary RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/DocumentPortSpec.cpp
        src/app/services/PlaybackDocumentPort.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

# Stage 4.9d-4b-2d: the playback coordinator's fourth narrow port, onto
# the preview stage-media route (warmup, chart-path resync,
# initialization-on-demand), the audio-runtime/outline-canvas re-applies,
# and preview shutdown. Cut by capability, same as preferences_port_spec
# above (eight of the nine methods' eventual owner is StageMediaHost, the
# ninth is Session's own orchestration). Linking Core+Test only (no Gui)
# is the same link-time guarantee as the other three port specs.
miacode_add_spec(preview_port_spec
    OWNER src/app/services
    CONTRACT v2.preview-port
    DOMAIN services KIND boundary RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/PreviewPortSpec.cpp
        src/app/services/PlaybackPreviewPort.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

# Stage 4.9e-3: the coordinator's second playback contract, alongside
# PlaybackControl — non-command state writes rather than user transport
# commands (see PlaybackStateAuthority.h). Linking Core+Test only (no Gui)
# is the same link-time guarantee as the four port specs above.
miacode_add_spec(playback_state_authority_spec
    OWNER src/app/services
    CONTRACT v2.playback-state-authority
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/PlaybackStateAuthoritySpec.cpp
        src/app/services/PlaybackStateAuthority.h
    LIBS Qt6::Core Qt6::Test
    INCLUDES src
)

# Stage 4.6: Timeline commands receive their own identity before the
# compatibility host forwards them to the current composite implementation.
miacode_add_spec(timeline_host_spec
    OWNER src/app/runtime
    CONTRACT v2.timeline-host
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
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

# Stage 4.7: PreviewHost keeps rendering/settings projection separate from
# the playback authority and consumes transport through typed ports.
miacode_add_spec(preview_host_spec
    OWNER src/app/runtime
    CONTRACT v2.preview-host
    DOMAIN services KIND source-contract RISK high
    EXECUTION ctest STATUS active PLATFORM all
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
target_compile_definitions(preview_host_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Stage 4.8: the coordinator owns playback contracts; legacy Preview and
# Timeline surface compatibility lives in explicit projection adapters.
miacode_add_spec(playback_coordinator_spec
    OWNER src/app/runtime
    CONTRACT v2.playback-coordinator
    DOMAIN services KIND source-contract RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/PlaybackCoordinatorSpec.cpp
        src/app/runtime/playback/PlaybackIdentityGate.cpp
        src/app/runtime/playback/PlaybackIdentityGate.h
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(playback_coordinator_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Stage 4.9b: the independent translation unit that parses RuntimeContext.h.
# Compile-only — the static assertions inside fail the build if the timeline
# storage split regresses.
miacode_add_spec(runtime_context_boundary_spec
    OWNER src/app/runtime
    CONTRACT v2.runtime-context-boundary
    DOMAIN services KIND boundary RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/RuntimeContextBoundarySpec.cpp
        src/app/runtime/RuntimeContext.h
        src/app/runtime/SessionMembers.inc
    LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

# Stage 4.9e-4: same shape as runtime_context_boundary_spec above, but for
# the canonical playback-authority storage split (RuntimeContext::PlaybackState).
miacode_add_spec(playback_storage_boundary_spec
    OWNER src/app/runtime
    CONTRACT v2.playback-storage-boundary
    DOMAIN services KIND boundary RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/PlaybackStorageBoundarySpec.cpp
        src/app/runtime/RuntimeContext.h
        src/app/runtime/SessionMembers.inc
    LIBS Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_spec(update_version_spec
    OWNER src/app/services/update
    CONTRACT v2.update-version
    DOMAIN services KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/update/UpdateVersionSpec.cpp
        src/app/services/update/SemanticVersion.cpp
        src/app/services/update/SemanticVersion.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(update_manifest_spec
    OWNER src/app/services/update
    CONTRACT v2.update-manifest
    DOMAIN services KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/update/UpdateManifestSpec.cpp
        src/app/services/update/UpdateManifest.cpp
        src/app/services/update/UpdateManifest.h
        src/app/services/update/SemanticVersion.cpp
        src/app/services/update/SemanticVersion.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(update_service_spec
    OWNER src/app/services/update
    CONTRACT v2.update-service
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
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

miacode_add_spec(preference_json_file_spec
    OWNER src/app/services
    CONTRACT preferences.json-file-recovery
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/services/PreferenceJsonFileSpec.cpp
    LIBS miacode_base Qt6::Core
    INCLUDES src
)

miacode_add_spec(project_preference_read_recovery_spec
    OWNER src/app/services
    CONTRACT preferences.project-unread-source
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/ProjectPreferenceReadRecoverySpec.cpp
        src/app/services/ProjectPreferences.cpp
        src/app/services/ProjectPreferences.h
    LIBS miacode_base miacode_audio Qt6::Core
    INCLUDES src
)

miacode_add_spec(net_configuration_spec
    OWNER src/app/services/net
    CONTRACT net.optional-configuration
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/NetConfigurationSpec.cpp
        src/app/services/net/NetConfiguration.cpp
        src/app/services/net/NetConfiguration.h
        src/app/services/net/NetAccountStore.cpp
        src/app/services/PreferenceDocument.cpp
    LIBS miacode_base Qt6::Core $<$<PLATFORM_ID:Windows>:Advapi32>
    INCLUDES src
)

miacode_add_spec(preference_repository_spec
    OWNER src/app/services
    CONTRACT preferences.runtime-document
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/PreferenceRepositorySpec.cpp
        src/app/services/PreferenceDocument.cpp
        src/app/services/PreferenceDocument.h
    LIBS miacode_base Qt6::Core
    INCLUDES src
)

miacode_add_spec(export_preferences_spec
    OWNER src/app/services
    CONTRACT preferences.export-storage-boundary
    DOMAIN services KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/services/ExportPreferencesSpec.cpp
    LIBS miacode_export Qt6::Core
    INCLUDES src
)

miacode_add_spec(shortcut_preference_persistence_spec
    OWNER src/app/services
    CONTRACT shortcuts.persistence-state
    DOMAIN services KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
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

miacode_add_spec(hud_font_preference_migration_spec
    OWNER src/app/services
    CONTRACT preferences.hud-font-legacy-migration
    DOMAIN services KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/services/HudFontPreferenceMigrationSpec.cpp
        src/app/services/PreferenceDocument.cpp
        src/app/services/PreferenceDocument.h
    LIBS miacode_scene miacode_base Qt6::Core Qt6::Gui
    INCLUDES src
)
target_compile_definitions(hud_font_preference_migration_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
