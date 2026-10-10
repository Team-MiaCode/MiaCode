include_guard(GLOBAL)

# Shared executable registration; TEST also adds a CTest entry.
function(miacode_add_dev_tool NAME)
    cmake_parse_arguments(DT "TEST" "" "SOURCES;LIBS;INCLUDES" ${ARGN})
    add_executable(${NAME} ${DT_SOURCES})
    # Keep QObject-based specs self-contained even when a source header is
    # also part of the main application target.
    set_target_properties(${NAME} PROPERTIES AUTOMOC ON)
    if (DT_LIBS)
        target_link_libraries(${NAME} PRIVATE ${DT_LIBS})
    endif()
    if (DT_INCLUDES)
        target_include_directories(${NAME} PRIVATE ${DT_INCLUDES})
    endif()
    if (DT_TEST)
        # Resolve product assets relative to the source tree.
        add_test(NAME ${NAME} COMMAND ${NAME} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        # Make Qt's runtime available to specs on Windows, and resolve Qt plugins
        # (the offscreen QPA used by QML specs) from the Qt installation rather
        # than from whatever windeployqt staged beside MiaCode.
        set_tests_properties(${NAME} PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_PLUGIN_PATH=set:${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}")
    endif()
endfunction()
