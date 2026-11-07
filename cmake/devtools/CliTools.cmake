# Stage the BASS runtime beside a diagnostic tool. The import libraries come
# from miacode_audio_bass, which the tool links.
function(miacode_link_dev_audio NAME)
    if(WIN32)
        set(runtime_files ${MIACODE_BASS_WINDOWS_DLLS})
        set(runtime_dir "${MIACODE_BASS_WINDOWS_BIN_DIR}")
    elseif(APPLE)
        set(runtime_files ${MIACODE_BASS_MACOS_LIBRARIES})
        set(runtime_dir "${MIACODE_BASS_MACOS_DIR}")
        set_target_properties(${NAME} PROPERTIES BUILD_RPATH "@executable_path")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(runtime_files ${MIACODE_BASS_LINUX_LIBRARIES})
        set(runtime_dir "${MIACODE_BASS_LINUX_DIR}")
        set_target_properties(${NAME} PROPERTIES BUILD_RPATH "$ORIGIN")
    endif()
    add_custom_command(TARGET ${NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${runtime_files} $<TARGET_FILE_DIR:${NAME}>
        WORKING_DIRECTORY "${runtime_dir}"
        VERBATIM)
endfunction()

# ---- CLI dump / probe helpers (manual diagnostics, not CTest cases) ----
miacode_add_dev_tool(miacode_muri_dump
    SOURCES
        src/devtools/MuriDump.cpp
    LIBS miacode_analysis Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(miacode_simai_dump
    SOURCES
        src/devtools/SimaiDump.cpp
    LIBS miacode_chart Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

# Cover compositor without the editor UI: one chart-frame layer (PV / 曲绘 /
# transparent disk background) next to the difficulty card.
miacode_add_dev_tool(miacode_cover_render
    SOURCES
        src/devtools/CoverRender.cpp
    LIBS miacode_export miacode_preview_quick miacode_preview_quickplugin
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    INCLUDES src
)
miacode_link_dev_audio(miacode_cover_render)

miacode_add_dev_tool(miacode_audio_probe
    SOURCES
        src/devtools/AudioProbe.cpp
    LIBS miacode_audio_bass Qt6::Concurrent Qt6::Core Qt6::Gui Qt6::Widgets soundtouch
    INCLUDES src
)
miacode_link_dev_audio(miacode_audio_probe)

# Batch offset-detection evaluator. Walks a chart corpus and scores the
# offset detector against each chart's &first (error folded mod one
# 8th-note). Manual diagnostic — needs a real corpus, so NOT a CTest case.
miacode_add_dev_tool(miacode_latency_offset_batch
    SOURCES
        src/devtools/LatencyOffsetBatch.cpp
        src/app/runtime/latency/LatencyAnalysis.h
        src/app/runtime/latency/LatencyAnalysis.cpp
    LIBS miacode_audio_bass Qt6::Core
    INCLUDES src
)
miacode_link_dev_audio(miacode_latency_offset_batch)
