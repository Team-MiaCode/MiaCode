miacode_add_dev_tool(timeline_model_spec TEST
    SOURCES
        src/tools/timeline/TimelineModelSpec.cpp
    LIBS miacode_timeline Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

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
