# ---- Spec executables (registered with CTest) ----
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

miacode_add_dev_tool(simai_document_spec TEST
    SOURCES
        src/tools/chart_document/SimaiDocumentSpec.cpp
    LIBS miacode_chart Qt6::Core
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

miacode_add_dev_tool(simai_text_edit_policy_spec TEST
    SOURCES
        src/tools/editor/SimaiTextEditPolicySpec.cpp
    LIBS miacode_editor_core Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(chart_zip_packager_spec TEST
    SOURCES
        src/tools/zip_export/ChartZipPackagerSpec.cpp
    LIBS miacode_media_tools Qt6::Core Qt6::Gui miniz
    INCLUDES src
)

miacode_add_dev_tool(preview_sfx_timeline_spec TEST
    SOURCES
        src/tools/preview/PreviewSfxTimelineSpec.cpp
    LIBS Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(preview_media_cache_stamp_spec TEST
    SOURCES
        src/tools/preview/PreviewMediaCacheStampSpec.cpp
    LIBS Qt6::Core Qt6::Gui
    INCLUDES src
)
