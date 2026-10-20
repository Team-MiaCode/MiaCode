miacode_add_dev_tool(timeline_model_spec TEST
    SOURCES
        src/tools/timeline/TimelineModelSpec.cpp
    LIBS miacode_timeline Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)
# Source-contract assertions in TimelineModelSpec must work from both the
# standard `build/` CTest directory and the developer build tree.
target_compile_definitions(timeline_model_spec PRIVATE
    "MIACODE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")

miacode_add_dev_tool(timeline_marker_offset_spec TEST
    SOURCES
        src/tools/timeline/TimelineMarkerOffsetSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(timeline_cadence_arbitration_policy_spec TEST
    SOURCES
        src/tools/timeline/TimelineCadenceArbitrationPolicySpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)
