# MiaCodeBatchCleaner: standalone batch chart cleanup (normalize, strip
# comments, unify designer). It links only miacode_chart and builds its own
# copy of the MiaCode.UI QML module from the shared theme and components, so
# the main application's module, resources and behavior stay untouched.
#
# Included from the top-level CMakeLists after the MiaCode target, whose
# QML/icon source properties (flat resource aliases, Theme singleton) and
# translation outputs this target reuses.
include_guard(GLOBAL)

set(MIACODE_BATCH_CLEANER_SHARED_QML_FILES
    src/app/ui/theme/Theme.qml
    src/app/ui/components/AppButton.qml
    src/app/ui/components/AppCheckBox.qml
    src/app/ui/components/AppComboBox.qml
    src/app/ui/components/AppDropdownPanel.qml
    src/app/ui/components/AppMenu.qml
    src/app/ui/components/AppMenuItem.qml
    src/app/ui/components/AppMenuSeparator.qml
    src/app/ui/components/AppScrollBar.qml
    src/app/ui/components/AppSwitch.qml
    src/app/ui/components/AppTextContextMenu.qml
    src/app/ui/components/AppTextField.qml
    src/app/ui/components/BackdropBlur.qml
    src/app/ui/components/ChromeRow.qml
    src/app/ui/components/DifficultySwatch.qml
    src/app/ui/components/FadeTransition.qml
    src/app/ui/components/FloatingCard.qml
    src/app/ui/components/HoverChrome.qml
    src/app/ui/components/SettingsSection.qml
    src/app/ui/components/Tooltip.qml
)
set(MIACODE_BATCH_CLEANER_SHARED_ICON_FILES
    src/app/ui/resources/icons/check.svg
    src/app/ui/resources/icons/chevron-down.svg
    src/app/ui/resources/icons/chevron-right.svg
)
set(MIACODE_BATCH_CLEANER_QML_FILES
    src/batch_cleaner/qml/BatchCleanerWindow.qml
)
set_source_files_properties(src/batch_cleaner/qml/BatchCleanerWindow.qml
    PROPERTIES QT_RESOURCE_ALIAS "BatchCleanerWindow.qml")

qt_add_executable(MiaCodeBatchCleaner WIN32 MACOSX_BUNDLE
    src/batch_cleaner/main.cpp
    src/batch_cleaner/BatchCleanupCore.cpp
    src/batch_cleaner/BatchCleanupCore.h
    src/batch_cleaner/BatchCleanerController.cpp
    src/batch_cleaner/BatchCleanerController.h
)

# Same URI as the application module so the shared components' `import
# MiaCode.UI` resolves here; a private output directory keeps its qmldir and
# type info apart from the application's.
qt_add_qml_module(MiaCodeBatchCleaner
    URI MiaCode.UI
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/batch_cleaner_qml/MiaCode/UI"
    NO_LINT
    QML_FILES
        ${MIACODE_BATCH_CLEANER_SHARED_QML_FILES}
        ${MIACODE_BATCH_CLEANER_QML_FILES}
    RESOURCES
        ${MIACODE_BATCH_CLEANER_SHARED_ICON_FILES}
)
miacode_add_qrc(MiaCodeBatchCleaner resources/app_icons.qrc)

qt_add_resources(MiaCodeBatchCleaner "batch_cleaner_i18n"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${MIACODE_TRANSLATION_QM_FILES}
)
add_dependencies(MiaCodeBatchCleaner miacode_lrelease)

target_include_directories(MiaCodeBatchCleaner PRIVATE src)
target_compile_definitions(MiaCodeBatchCleaner PRIVATE
    MIACODE_BATCH_CLEANER_VERSION="${PROJECT_VERSION}")
target_link_libraries(MiaCodeBatchCleaner PRIVATE
    miacode_chart
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)
set_target_properties(MiaCodeBatchCleaner PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "MiaCodeBatchCleaner"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.fanfaredash.MiaCodeBatchCleaner"
)
if (UNIX AND NOT APPLE)
    set_target_properties(MiaCodeBatchCleaner PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()
if (WIN32 AND DEFINED MIACODE_APP_RC)
    target_sources(MiaCodeBatchCleaner PRIVATE "${MIACODE_APP_RC}")
endif()
if (MSVC)
    target_compile_options(MiaCodeBatchCleaner PRIVATE /FS /utf-8)
endif()
