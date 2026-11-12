miacode_add_dev_tool(timeline_model_spec TEST
    SOURCES
        src/tools/timeline/TimelineModelSpec.cpp
    LIBS miacode_timeline Qt6::Core Qt6::Gui Qt6::Widgets
    INCLUDES src
)

miacode_add_dev_tool(timeline_note_selection_interaction_spec TEST
    SOURCES src/tools/timeline/TimelineNoteSelectionInteractionSpec.cpp
    LIBS miacode_timeline_quick Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test
    INCLUDES src
)
set_tests_properties(timeline_note_selection_interaction_spec PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

miacode_add_dev_tool(chart_note_selection_spec TEST
    SOURCES src/tools/chart_selection/ChartNoteSelectionSpec.cpp
    LIBS miacode_timeline Qt6::Core Qt6::Gui
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
