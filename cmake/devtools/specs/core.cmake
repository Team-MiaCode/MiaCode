# Explicit spec targets; contract IDs stay stable across source/target renames.

# ---- Spec executables (registered with CTest) ----
miacode_add_spec(oplog_self_test
    OWNER src/common
    CONTRACT oplog.oplog
    DOMAIN oplog KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/oplog/OperationLogSpec.cpp
    LIBS miacode_base Qt6::Core
    INCLUDES src
)

miacode_add_spec(simai_parser_spec
    OWNER src/core/chart/parser
    CONTRACT simai-parser.simai-parser
    DOMAIN simai_parser KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/simai_parser/SimaiParserSpec.cpp
    LIBS miacode_chart Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_spec(chart_batch_transform_spec
    OWNER src/core/chart/transform
    CONTRACT chart-transform.chart-batch-transform
    DOMAIN chart_transform KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/chart_transform/ChartBatchTransformSpec.cpp
    LIBS miacode_chart Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_spec(chart_selection_beat_summary_spec
    OWNER src/core/chart/selection
    CONTRACT chart-selection.chart-selection-beat-summary
    DOMAIN chart_selection KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/chart_selection/ChartSelectionBeatSummarySpec.cpp
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_spec(extension_manifest_spec
    OWNER src/extensions
    CONTRACT extensions.extension-manifest
    DOMAIN extensions KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/extensions/ExtensionManifestSpec.cpp
        src/extensions/ExtensionManifest.h
        src/extensions/ExtensionManifest.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(app_background_settings_spec
    OWNER src/app/ui
    CONTRACT ui.app-background-settings
    DOMAIN ui KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/AppBackgroundSettingsSpec.cpp
        src/app/ui/preferences/AppBackgroundSettings.h
        src/app/ui/preferences/AppBackgroundSettings.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(theme_variant_resolver_spec
    OWNER src/app/ui
    CONTRACT ui.theme-variant-resolver
    DOMAIN ui KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/ThemeVariantResolverSpec.cpp
        src/app/ui/theme/ThemeVariantResolver.h
        src/app/ui/theme/ThemeVariantResolver.cpp
        src/app/services/PreferenceDocument.h
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(extension_product_boundary_spec
    OWNER src/extensions
    CONTRACT extensions.extension-product-boundary
    DOMAIN extensions KIND boundary RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/extensions/ExtensionProductBoundarySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
target_compile_definitions(extension_product_boundary_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(simai_document_spec
    OWNER src/core/chart/document
    CONTRACT chart-document.simai-document
    DOMAIN chart_document KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/chart_document/SimaiDocumentSpec.cpp
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_spec(cover_layout_model_spec
    OWNER src/export/cover_export
    CONTRACT cover-export.cover-layout-model
    DOMAIN cover_export KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/cover_export/CoverLayoutModelSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_spec(cover_pv_frame_source_spec
    OWNER src/export/cover_export
    CONTRACT cover-export.cover-pv-frame-source
    DOMAIN cover_export KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/cover_export/CoverPvFrameSourceSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(cover_frame_playback_controller_spec
    OWNER src/export/cover_export
    CONTRACT cover-export.cover-frame-playback-controller
    DOMAIN cover_export KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/cover_export/CoverFramePlaybackControllerSpec.cpp
    LIBS miacode_export Qt6::Core
    INCLUDES src
)

miacode_add_spec(cover_frame_scene_binder_spec
    OWNER src/export/cover_export
    CONTRACT cover-export.cover-frame-scene-binder
    DOMAIN cover_export KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/cover_export/CoverFrameSceneBinderSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(muri_spec
    OWNER src/core/analysis
    CONTRACT muri.muri
    DOMAIN muri KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/muri/MuriSpec.cpp
    LIBS miacode_analysis Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_spec(touch_pad_authoring_edit_spec
    OWNER src/editor
    CONTRACT editor.touch-pad-authoring-edit
    DOMAIN editor KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/editor/TouchPadAuthoringEditSpec.cpp
    LIBS miacode_editor_core Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(simai_completion_catalog_spec
    OWNER src/editor
    CONTRACT editor.simai-completion-catalog
    DOMAIN editor KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/editor/SimaiCompletionCatalogSpec.cpp
    LIBS miacode_editor_core Qt6::Core
    INCLUDES src
)

miacode_add_spec(simai_text_edit_policy_spec
    OWNER src/editor
    CONTRACT editor.simai-text-edit-policy
    DOMAIN editor KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/editor/SimaiTextEditPolicySpec.cpp
    LIBS miacode_editor_core Qt6::Core
    INCLUDES src
)

miacode_add_spec(video_export_runtime_policy_spec
    OWNER src/export/video_export
    CONTRACT video-export.video-export-runtime-policy
    DOMAIN video_export KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/video_export/VideoExportRuntimePolicySpec.cpp
    LIBS miacode_export Qt6::Core
    INCLUDES src
)

miacode_add_spec(raw_video_pipe_frame_conservation_spec
    OWNER src/export/video_export
    CONTRACT video-export.raw-video-pipe-frame-conservation
    DOMAIN video_export KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/video_export/RawVideoPipeFrameConservationSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(video_export_pending_frame_redraw_spec
    OWNER src/export/video_export
    CONTRACT video-export.video-export-pending-frame-redraw
    DOMAIN video_export KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/video_export/VideoExportPendingFrameRedrawSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(video_export_intro_mode_spec
    OWNER src/export/video_export
    CONTRACT video-export.video-export-intro-mode
    DOMAIN video_export KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/video_export/VideoExportIntroModeSpec.cpp
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(font_library_isolation_spec
    OWNER src/export/video_export
    CONTRACT video-export.font-library-isolation
    DOMAIN video_export KIND integration RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/video_export/FontLibraryIsolationSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)
target_compile_definitions(font_library_isolation_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(video_export_hud_font_snapshot_spec
    OWNER src/export/video_export
    CONTRACT video-export.hud-font-snapshot
    DOMAIN video_export KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/video_export/VideoExportHudFontSnapshotSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)
target_compile_definitions(video_export_hud_font_snapshot_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_spec(video_export_intro_sound_spec
    OWNER src/export/video_export
    CONTRACT video-export.video-export-intro-sound
    DOMAIN video_export KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/video_export/VideoExportIntroSoundSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(video_export_audio_render_plan_spec
    OWNER src/export/video_export
    CONTRACT video-export.video-export-audio-render-plan
    DOMAIN video_export KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/video_export/VideoExportAudioRenderPlanSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(video_export_media_timeline_spec
    OWNER src/export/video_export
    CONTRACT video-export.video-export-media-timeline
    DOMAIN video_export KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/video_export/VideoExportMediaTimelineSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_spec(chart_zip_packager_spec
    OWNER src/media_tools/zip_export
    CONTRACT zip-export.chart-zip-packager
    DOMAIN zip_export KIND integration RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/zip_export/ChartZipPackagerSpec.cpp
    LIBS miacode_media_tools Qt6::Core Qt6::Gui miniz
    INCLUDES src
)

# Net behavior specifications consume the same media_tools library as the product.
# The engine specification also compiles the existing batch workers.
miacode_add_spec(net_client_spec
    OWNER src/media_tools/net
    CONTRACT net.net-client
    DOMAIN net KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/net/NetClientSpec.cpp
    LIBS miacode_media_tools Qt6::Core Qt6::Gui Qt6::Network miniz
    INCLUDES src
)

miacode_add_spec(net_query_rules_spec
    OWNER src/media_tools/net
    CONTRACT net.query-rules
    DOMAIN net KIND behavior RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/net/NetQueryRulesSpec.cpp
    LIBS miacode_media_tools Qt6::Core
    INCLUDES src
)

miacode_add_spec(net_http_transport_spec
    OWNER src/media_tools/net
    CONTRACT net.http-transport
    DOMAIN net KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/net/NetHttpTransportSpec.cpp
    LIBS miacode_media_tools Qt6::Core Qt6::Network
    INCLUDES src
)

miacode_add_spec(net_provider_spec
    OWNER src/media_tools/net
    CONTRACT net.provider
    DOMAIN net KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES src/tools/net/NetProviderSpec.cpp
    LIBS miacode_media_tools Qt6::Core
    INCLUDES src
)

miacode_add_spec(net_download_flow_spec
    OWNER src/media_tools/net
    CONTRACT net.download-preview
    DOMAIN net KIND integration RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/net/NetDownloadFlowSpec.cpp
        src/app/services/api/ApiCatalog.cpp
        src/app/services/api/ApiDispatcher.cpp
        src/app/services/net/NetService.cpp
        src/app/services/net/NetService.h
        src/app/services/net/NetAccountStore.cpp
        src/app/services/jobs/JobRegistry.cpp
        src/app/services/jobs/JobRegistry.h
    LIBS miacode_media_tools Qt6::Core Qt6::Network miniz $<$<PLATFORM_ID:Windows>:Advapi32>
    INCLUDES src
)
target_compile_definitions(net_download_flow_spec PRIVATE
    "MIACODE_TEST_OUTPUT_ROOT=\"${CMAKE_CURRENT_BINARY_DIR}\""
    "MIACODE_ZH_CN_QM_PATH=\"${CMAKE_CURRENT_BINARY_DIR}/zh_CN.qm\"")
add_dependencies(net_download_flow_spec miacode_lrelease)

miacode_add_spec(pv_compression_policy_spec
    OWNER src/media_tools/media
    CONTRACT media.pv-compression-policy
    DOMAIN media KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/media/PvCompressionPolicySpec.cpp
    LIBS miacode_media_tools Qt6::Core
    INCLUDES src
)

# Drift guard for docs/ops/DEPENDENCY_ALLOWLIST.md (stage 3.5, item 4).
# Parses every target_link_libraries(MiaCode …) call plus the doc's
# allow/forbid/media-adapter tables, so an undocumented dependency, a stale
# doc row, a forbidden link (Qt6::Network today, Qt6::Widgets after the
# product source migration),
# an unpinned Qt version, or QtAVPlayer headers leaking outside the media
# adapter layer all fail the build's test suite.
miacode_add_spec(dependency_allowlist_spec
    OWNER src/common
    CONTRACT deps.dependency-allowlist
    DOMAIN deps KIND source-contract RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/deps/DependencyAllowlistSpec.cpp
    LIBS Qt6::Core
)
target_compile_definitions(dependency_allowlist_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Drift guard for ID-based QTranslator catalogs (.ts parity + source literal ids).
miacode_add_spec(ui_text_locale_spec
    OWNER src/app/ui
    CONTRACT ui-text.ui-text-locale
    DOMAIN ui_text KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui_text/UiTextLocaleSpec.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/PreferenceDocument.cpp
    LIBS miacode_base Qt6::Core
    INCLUDES src
)
target_compile_definitions(ui_text_locale_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\""
    "MIACODE_EN_US_QM_PATH=\"${CMAKE_CURRENT_BINARY_DIR}/en_US.qm\"")
add_dependencies(ui_text_locale_spec miacode_lrelease)

miacode_add_spec(native_chrome_policy_spec
    OWNER src/app/ui
    CONTRACT ui.native-chrome-policy
    DOMAIN ui KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui/NativeChromePolicySpec.cpp
        src/app/quick_shell/QuickShellPopupPosition.h
        src/app/ui/chrome/NativeWindowThemePolicy.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_spec(ui_text_preferences_spec
    OWNER src/app/ui
    CONTRACT ui-text.ui-text-preferences
    DOMAIN ui_text KIND behavior RISK normal
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/ui_text/UiTextPreferencesSpec.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/PreferenceDocument.cpp
    LIBS miacode_base Qt6::Core
    INCLUDES src
)
