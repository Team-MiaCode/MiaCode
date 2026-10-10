# Library-combination boundary specs. Each target links only the libraries of
# one platform combination (docs/specs/architecture/MODULE_LAYERING_CURRENT_ZH.md),
# never MiaCode, so a library that grows a dependency outside its combination
# fails to link here before a platform build would notice.

miacode_add_spec(web_module_boundary_spec
    OWNER src
    CONTRACT architecture.web-module-boundary
    DOMAIN architecture KIND boundary RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/boundary/WebModuleBoundarySpec.cpp
    LIBS
        miacode_base miacode_chart miacode_analysis miacode_scene
        miacode_preview_quick miacode_preview_quickplugin
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    INCLUDES src
)

if(MIACODE_BUILD_APP OR CMAKE_SCRIPT_MODE_FILE)
miacode_add_spec(android_module_boundary_spec
    OWNER src
    CONTRACT architecture.android-module-boundary
    DOMAIN architecture KIND boundary RISK high
    EXECUTION ctest STATUS active PLATFORM all
    SOURCES
        src/tools/boundary/AndroidModuleBoundarySpec.cpp
    LIBS
        miacode_base miacode_chart miacode_analysis miacode_scene miacode_audio
        miacode_preview_quick miacode_preview_quickplugin
        miacode_timeline miacode_timeline_quick miacode_timeline_quickplugin
        miacode_audio_bass
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    INCLUDES src
)
endif()
