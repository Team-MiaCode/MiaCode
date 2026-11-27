# Qt dependencies for diagnostic executables and specs.
find_package(Qt6 6.10 REQUIRED COMPONENTS Test Network Widgets)

include(${CMAKE_CURRENT_LIST_DIR}/MiaCodeDevToolHelpers.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/CliTools.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/specs/index.cmake)
