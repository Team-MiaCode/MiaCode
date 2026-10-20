miacode_add_dev_tool(preview_asset_loader_spec TEST
    SOURCES
        src/tools/preview/PreviewSceneAssetLoaderSpec.cpp
    LIBS miacode_preview_quick Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(preview_firework_lifecycle_spec TEST
    SOURCES
        src/tools/preview/PreviewFireworkLifecycleSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(preview_end_of_media_policy_spec TEST
    SOURCES
        src/tools/preview/PreviewEndOfMediaPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_firework_warmup_policy_spec TEST
    SOURCES
        src/tools/preview/PreviewFireworkWarmupPolicySpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(preview_head_layer_spec TEST
    SOURCES
        src/tools/preview/PreviewHeadLayerSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(preview_guide_layer_spec TEST
    SOURCES
        src/tools/preview/PreviewGuideLayerSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(preview_slide_erase_by_area_spec TEST
    SOURCES
        src/tools/preview/PreviewSlideEraseByAreaSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(preview_realtime_object_hot_path_spec TEST
    SOURCES
        src/tools/preview/PreviewRealtimeObjectHotPathSpec.cpp
    LIBS miacode_scene Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(preview_quick_sprite_batch_spec TEST
    SOURCES
        src/tools/preview/PreviewQuickSpriteBatchSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_texture_generation_policy_spec TEST
    SOURCES
        src/tools/preview/PreviewTextureGenerationPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_sfx_timeline_spec TEST
    SOURCES
        src/tools/preview/PreviewSfxTimelineSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_audio_settings_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioSettingsSpec.cpp
    LIBS miacode_audio Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_audio_command_queue_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioCommandQueueSpec.cpp
    LIBS miacode_audio Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_audio_worker_protocol_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioWorkerProtocolSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_audio_playback_flow_policy_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioPlaybackFlowPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
miacode_add_dev_tool(preview_audio_worker_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioWorkerSpec.cpp
    LIBS miacode_audio_bass Qt6::Core soundtouch
    INCLUDES src
)
# BASS import libraries come from miacode_audio_bass; stage the runtime the
# production backend loads beside the spec.
if (WIN32)
    add_custom_command(TARGET preview_audio_worker_spec POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MIACODE_BASS_WINDOWS_BIN_DIR}/bass.dll"
            "${MIACODE_BASS_WINDOWS_BIN_DIR}/bassmix.dll"
            "${MIACODE_BASS_WINDOWS_BIN_DIR}/bassflac.dll"
            $<TARGET_FILE_DIR:preview_audio_worker_spec>
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_command(TARGET preview_audio_worker_spec POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MIACODE_BASS_LINUX_LIBRARIES}
            $<TARGET_FILE_DIR:preview_audio_worker_spec>
    )
endif()

miacode_add_dev_tool(preview_audio_non_gui_barrier_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioNonGuiBarrierSpec.cpp
    LIBS miacode_audio Qt6::Core
    INCLUDES src
)
if (WIN32)
    target_link_libraries(preview_audio_non_gui_barrier_spec PRIVATE avrt)
endif()

miacode_add_dev_tool(touch_pad_authoring_state_spec TEST
    SOURCES
        src/tools/preview/TouchPadAuthoringStateSpec.cpp
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(bass_preview_retained_state_spec TEST
    SOURCES
        src/tools/preview/BassPreviewRetainedStateSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(bass_preview_debug_log_routing_spec TEST
    SOURCES
        src/tools/preview/BassPreviewDebugLogRoutingSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_bass_device_lease_spec TEST
    SOURCES
        src/tools/preview/PreviewBassDeviceLeaseSpec.cpp
    LIBS miacode_audio_bass Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_audio_health_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioHealthSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_audio_output_glitch_probe_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioOutputGlitchProbeSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_audio_device_change_policy_spec TEST
    SOURCES
        src/tools/preview/PreviewAudioDeviceChangePolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(bass_preview_sfx_scheduler_policy_spec TEST
    SOURCES
        src/tools/preview/BassPreviewSfxSchedulerPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

# Simulation spec for the preview audio/stage-media cache mechanism: the
# content-stamp skip decision (size:mtime, NOT path-only) and the live,
# uncached track/bg resolvers. Drives the real production functions against
# real temp files, simulating an in-place track.mp3/bg.png rewrite.
miacode_add_dev_tool(preview_media_cache_stamp_spec TEST
    SOURCES
        src/tools/preview/PreviewMediaCacheStampSpec.cpp
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)

miacode_add_dev_tool(pv_memory_diagnostics_spec TEST
    SOURCES
        src/tools/preview/PvMemoryDiagnosticsSpec.cpp
    LIBS miacode_stage_media Qt6::Core
    INCLUDES src
)
target_compile_definitions(pv_memory_diagnostics_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_dev_tool(quickshell_preview_surface_policy_spec TEST
    SOURCES
        src/tools/preview/QuickShellPreviewSurfacePolicySpec.cpp
        src/app/quick_shell/QuickShellPreviewSurfacePolicy.h
    LIBS Qt6::Core
    INCLUDES src
)
