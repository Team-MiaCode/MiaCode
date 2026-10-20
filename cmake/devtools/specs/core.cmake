# ---- Spec executables (registered with CTest) ----
miacode_add_dev_tool(oplog_self_test TEST
    SOURCES
        src/tools/oplog/OperationLogSpec.cpp
    LIBS miacode_base Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(simai_parser_spec TEST
    SOURCES
        src/tools/simai_parser/SimaiParserSpec.cpp
    LIBS miacode_chart Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(chart_batch_transform_spec TEST
    SOURCES
        src/tools/chart_transform/ChartBatchTransformSpec.cpp
    LIBS miacode_chart Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(chart_selection_beat_summary_spec TEST
    SOURCES
        src/tools/chart_selection/ChartSelectionBeatSummarySpec.cpp
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(extension_manifest_spec TEST
    SOURCES
        src/tools/extensions/ExtensionManifestSpec.cpp
        src/extensions/ExtensionManifest.h
        src/extensions/ExtensionManifest.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(app_background_settings_spec TEST
    SOURCES
        src/tools/ui/AppBackgroundSettingsSpec.cpp
        src/app/ui/preferences/AppBackgroundSettings.h
        src/app/ui/preferences/AppBackgroundSettings.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(theme_variant_resolver_spec TEST
    SOURCES
        src/tools/ui/ThemeVariantResolverSpec.cpp
        src/app/ui/theme/ThemeVariantResolver.h
        src/app/ui/theme/ThemeVariantResolver.cpp
        src/app/services/PreferenceDocument.h
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(simai_document_spec TEST
    SOURCES
        src/tools/chart_document/SimaiDocumentSpec.cpp
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(cover_layout_model_spec TEST
    SOURCES
        src/tools/cover_export/CoverLayoutModelSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(cover_pv_frame_source_spec TEST
    SOURCES
        src/tools/cover_export/CoverPvFrameSourceSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(cover_frame_playback_controller_spec TEST
    SOURCES
        src/tools/cover_export/CoverFramePlaybackControllerSpec.cpp
    LIBS miacode_export Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(cover_frame_scene_binder_spec TEST
    SOURCES
        src/tools/cover_export/CoverFrameSceneBinderSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(muri_spec TEST
    SOURCES
        src/tools/muri/MuriSpec.cpp
    LIBS miacode_analysis Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(touch_pad_authoring_edit_spec TEST
    SOURCES
        src/tools/editor/TouchPadAuthoringEditSpec.cpp
    LIBS miacode_editor_core Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(simai_completion_catalog_spec TEST
    SOURCES
        src/tools/editor/SimaiCompletionCatalogSpec.cpp
    LIBS miacode_editor_core Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(simai_text_edit_policy_spec TEST
    SOURCES
        src/tools/editor/SimaiTextEditPolicySpec.cpp
    LIBS miacode_editor_core Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(video_export_runtime_policy_spec TEST
    SOURCES
        src/tools/video_export/VideoExportRuntimePolicySpec.cpp
    LIBS miacode_export Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(raw_video_pipe_frame_conservation_spec TEST
    SOURCES
        src/tools/video_export/RawVideoPipeFrameConservationSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(video_export_pending_frame_redraw_spec TEST
    SOURCES
        src/tools/video_export/VideoExportPendingFrameRedrawSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(video_export_intro_mode_spec TEST
    SOURCES
        src/tools/video_export/VideoExportIntroModeSpec.cpp
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(font_library_isolation_spec TEST
    SOURCES src/tools/video_export/FontLibraryIsolationSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)
target_compile_definitions(font_library_isolation_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_dev_tool(video_export_hud_font_snapshot_spec TEST
    SOURCES src/tools/video_export/VideoExportHudFontSnapshotSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)
target_compile_definitions(video_export_hud_font_snapshot_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_dev_tool(video_export_intro_sound_spec TEST
    SOURCES
        src/tools/video_export/VideoExportIntroSoundSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(video_export_audio_render_plan_spec TEST
    SOURCES
        src/tools/video_export/VideoExportAudioRenderPlanSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(video_export_media_timeline_spec TEST
    SOURCES
        src/tools/video_export/VideoExportMediaTimelineSpec.cpp
    LIBS miacode_export Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(chart_zip_packager_spec TEST
    SOURCES
        src/tools/zip_export/ChartZipPackagerSpec.cpp
    LIBS miacode_media_tools Qt6::Core Qt6::Gui miniz
    INCLUDES src
)

# Net behavior specifications consume the same media_tools library as the product.
# The engine specification also compiles the existing batch workers.
miacode_add_dev_tool(net_client_spec TEST
    SOURCES
        src/tools/net/NetClientSpec.cpp
    LIBS miacode_media_tools Qt6::Core Qt6::Gui Qt6::Network miniz
    INCLUDES src
)

miacode_add_dev_tool(net_query_rules_spec TEST
    SOURCES src/tools/net/NetQueryRulesSpec.cpp
    LIBS miacode_media_tools Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(net_http_transport_spec TEST
    SOURCES src/tools/net/NetHttpTransportSpec.cpp
    LIBS miacode_media_tools Qt6::Core Qt6::Network
    INCLUDES src
)

miacode_add_dev_tool(net_provider_spec TEST
    SOURCES src/tools/net/NetProviderSpec.cpp
    LIBS miacode_media_tools Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(net_download_flow_spec TEST
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

miacode_add_dev_tool(pv_compression_policy_spec TEST
    SOURCES
        src/tools/media/PvCompressionPolicySpec.cpp
    LIBS miacode_media_tools Qt6::Core
    INCLUDES src
)

# Drift guard for ID-based QTranslator catalogs (.ts parity + source literal ids).
miacode_add_dev_tool(ui_text_locale_spec TEST
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

miacode_add_dev_tool(native_chrome_policy_spec TEST
    SOURCES
        src/tools/ui/NativeChromePolicySpec.cpp
        src/app/quick_shell/QuickShellPopupPosition.h
        src/app/ui/chrome/NativeWindowThemePolicy.h
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(ui_text_preferences_spec TEST
    SOURCES
        src/tools/ui_text/UiTextPreferencesSpec.cpp
        src/app/services/PreferenceDocument.h
        src/app/services/PreferenceDocument.cpp
    LIBS miacode_base Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(video_export_estimate_history_spec TEST
    SOURCES src/tools/video_export/VideoExportEstimateHistorySpec.cpp
    LIBS miacode_export Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(video_export_output_spec TEST
    SOURCES src/tools/video_export/VideoExportOutputSpec.cpp
    LIBS miacode_export Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(media_prepend_policy_spec TEST
    SOURCES src/tools/media/MediaPrependPolicySpec.cpp
    LIBS miacode_media_tools Qt6::Core
    INCLUDES src
)
