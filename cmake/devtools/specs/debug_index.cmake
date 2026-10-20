

# Behavior spec locking DebugOptions.h accessor semantics (defaults, clamps,
# debug-category gating), the safety net for any future table-driven
# registry refactor.
miacode_add_dev_tool(debug_options_spec TEST
    SOURCES
        src/tools/debug_index/DebugOptionsSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(ui_hang_watchdog_policy_spec TEST
    SOURCES
        src/tools/debug_index/UiHangWatchdogPolicySpec.cpp
    LIBS miacode_base Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(log_pruning_policy_spec TEST
    SOURCES
        src/tools/debug_index/LogPruningPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(process_diagnostics_spec TEST
    SOURCES
        src/tools/debug_index/ProcessDiagnosticsSpec.cpp
    LIBS miacode_base Qt6::Core Qt6::Gui
    INCLUDES src
)
if (WIN32)
    # Per-adapter VRAM gauge: CreateDXGIFactory1 + IDXGIAdapter3::QueryVideoMemoryInfo.
    target_link_libraries(process_diagnostics_spec PRIVATE dxgi)
endif()

miacode_add_dev_tool(window_visibility_diagnostics_spec TEST
    SOURCES
        src/tools/debug_index/WindowVisibilityDiagnosticsSpec.cpp
        src/app/platform/WindowVisibilityDiagnostics.h
        src/app/platform/WindowVisibilityDiagnostics.cpp
    LIBS miacode_base Qt6::Core Qt6::Gui
    INCLUDES src
)
