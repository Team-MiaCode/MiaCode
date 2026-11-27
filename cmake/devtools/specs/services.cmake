miacode_add_dev_tool(chart_workspace_spec TEST
    SOURCES
        src/tools/services/ChartWorkspaceSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
    LIBS miacode_chart Qt6::Core
    INCLUDES src
)

miacode_add_dev_tool(chart_workspace_file_service_spec TEST
    SOURCES
        src/tools/services/ChartWorkspaceFileServiceSpec.cpp
        src/app/services/ChartWorkspace.cpp
        src/app/services/ChartWorkspace.h
        src/app/services/ChartWorkspaceFileService.cpp
        src/app/services/ChartWorkspaceFileService.h
    LIBS miacode_chart Qt6::Core Qt6::Gui
    INCLUDES src
)
