
# Qt dependencies for diagnostic executables and specs.
find_package(Qt6 6.10 REQUIRED COMPONENTS Test Network Widgets)

# Mirror the product QML module for specs that instantiate its components.
set(MIACODE_QML_SPEC_IMPORT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/qml_spec_imports")
set(_miacode_qml_spec_module_dir "${MIACODE_QML_SPEC_IMPORT_ROOT}/MiaCode/UI")
set(_miacode_qml_spec_qmldir "module MiaCode.UI\n")
foreach(qml_file IN LISTS MIACODE_UI_QML_FILES)
    get_filename_component(_qml_name "${qml_file}" NAME)
    get_filename_component(_qml_type "${qml_file}" NAME_WE)
    if (_qml_name STREQUAL "Theme.qml")
        string(APPEND _miacode_qml_spec_qmldir "singleton ${_qml_type} 1.0 ${_qml_name}\n")
    else()
        string(APPEND _miacode_qml_spec_qmldir "${_qml_type} 1.0 ${_qml_name}\n")
    endif()
    configure_file("${qml_file}" "${_miacode_qml_spec_module_dir}/${_qml_name}" COPYONLY)
endforeach()
file(WRITE "${_miacode_qml_spec_module_dir}/qmldir" "${_miacode_qml_spec_qmldir}")

include(${CMAKE_CURRENT_LIST_DIR}/MiaCodeDevToolHelpers.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/CliTools.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/MiaCodeSpecRegistry.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/specs/index.cmake)
miacode_finalize_spec_registry()
