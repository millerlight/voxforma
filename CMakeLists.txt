cmake_minimum_required(VERSION 3.22)

project(voxforma VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is vendored as a git submodule in "JUCE/". A checkout outside the repository can still be
# used by pointing the JUCE_DIR cache variable at it.
if(NOT JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE was not found at ${JUCE_DIR}. Run 'git submodule update --init --recursive' "
        "or configure with -DJUCE_DIR=/path/to/JUCE.")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")

# JUCE generates the VST3 "moduleinfo.json" by running a helper binary that is compiled for the
# target platform. During a cross-compile that binary cannot be executed, so JUCE writes a "{}"
# placeholder instead. A "{}" manifest is not a valid VST3 module description, and the class IDs
# in a VST3 module are byte-order dependent, so the placeholder has to be replaced by a manifest
# that matches the target platform.
set(VOXFORMA_VST3_MANIFEST_OVERRIDE FALSE)

if(CMAKE_CROSSCOMPILING AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
    set(VOXFORMA_VST3_MANIFEST_OVERRIDE TRUE)
endif()

if(DEFINED JUCE_WINDOWS_HELPERS_CAN_RUN AND NOT JUCE_WINDOWS_HELPERS_CAN_RUN)
    set(VOXFORMA_VST3_MANIFEST_OVERRIDE TRUE)
endif()

if(VOXFORMA_VST3_MANIFEST_OVERRIDE)
    set(VOXFORMA_VST3_AUTO_MANIFEST FALSE)
else()
    set(VOXFORMA_VST3_AUTO_MANIFEST TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(VOXFORMA_MINGW_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/juce_win_mingw_compat.h")
    set(MINGW_COMPAT_HEADER "${VOXFORMA_MINGW_COMPAT}" CACHE INTERNAL "MinGW compatibility header for JUCE")
    add_compile_definitions(_WIN64=1 JUCE_MINGW=1)
    add_compile_options(-include "${VOXFORMA_MINGW_COMPAT}")

    # JUCE rejects MinGW with a hard #error unless the compatibility patches have been applied
    # to the submodule. Catch that here instead of letting it fail deep inside a compile.
    file(READ "${JUCE_DIR}/modules/juce_core/system/juce_TargetPlatform.h" vox_target_platform)
    string(FIND "${vox_target_platform}" "JUCE_MINGW" vox_mingw_guard_pos)

    if(vox_mingw_guard_pos EQUAL -1)
        message(FATAL_ERROR
            "The JUCE submodule has not been patched for MinGW support.\n"
            "Run:  ${CMAKE_CURRENT_SOURCE_DIR}/patches/apply-juce-patches.sh")
    endif()
endif()

juce_add_plugin(voxforma
    COMPANY_NAME "clicktone"
    BUNDLE_ID "dev.clicktone.voxforma"
    PLUGIN_MANUFACTURER_CODE Cltn
    PLUGIN_CODE Vxfm
    FORMATS VST3
    PRODUCT_NAME "voxforma"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Filter
    VST3_AUTO_MANIFEST ${VOXFORMA_VST3_AUTO_MANIFEST})

target_sources(voxforma PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(voxforma PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(voxforma PUBLIC JUCE_USE_XINPUT=0)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(voxforma PRIVATE
        -include "${CMAKE_CURRENT_SOURCE_DIR}/cmake/juce_win_mingw_compat.h")
endif()

target_link_libraries(voxforma
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_definitions(voxforma PUBLIC _WIN64=1)
    target_link_libraries(voxforma PRIVATE
        ole32 uuid oleaut32
        user32 gdi32 shell32 comdlg32 advapi32
        winmm ws2_32 version wininet imm32
        d2d1 dwrite d3d11 dxgi dxguid dcomp dwmapi shcore
        comctl32 shlwapi rpcrt4 cfgmgr32
        dbghelp sapi wbemuuid)
endif()

if(VOXFORMA_VST3_MANIFEST_OVERRIDE)
    get_target_property(vox_vst3_artefact voxforma_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
    get_target_property(vox_manufacturer_code voxforma JUCE_PLUGIN_MANUFACTURER_CODE)
    get_target_property(vox_plugin_code voxforma JUCE_PLUGIN_CODE)
    get_target_property(vox_plugin_name voxforma JUCE_PLUGIN_NAME)
    get_target_property(vox_plugin_version voxforma JUCE_VERSION)
    get_target_property(vox_vendor_name voxforma JUCE_COMPANY_NAME)
    get_target_property(vox_vst3_categories voxforma JUCE_VST3_CATEGORIES)
    string(REPLACE ";" "," vox_subcategories "${vox_vst3_categories}")

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(vox_windows_byte_order ON)
    else()
        set(vox_windows_byte_order OFF)
    endif()

    add_custom_command(TARGET voxforma_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DVOX_MANIFEST_OUT=${vox_vst3_artefact}/Contents/Resources/moduleinfo.json"
            "-DVOX_MANUFACTURER_CODE=${vox_manufacturer_code}"
            "-DVOX_PLUGIN_CODE=${vox_plugin_code}"
            "-DVOX_NAME=${vox_plugin_name}"
            "-DVOX_VERSION=${vox_plugin_version}"
            "-DVOX_VENDOR=${vox_vendor_name}"
            "-DVOX_SUBCATEGORIES=${vox_subcategories}"
            "-DVOX_WINDOWS_BYTE_ORDER=${vox_windows_byte_order}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/vst3_moduleinfo.cmake"
        COMMENT "Generating VST3 moduleinfo.json for ${CMAKE_SYSTEM_NAME}"
        VERBATIM)
endif()

option(VOXFORMA_BUILD_TESTS "Build engine smoke test" ON)
if(VOXFORMA_BUILD_TESTS AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(engine_smoke tests/engine_smoke.cpp)
    target_link_libraries(engine_smoke PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
    target_compile_definitions(engine_smoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

    add_executable(editor_smoke tests/editor_smoke.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_link_libraries(editor_smoke PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    target_compile_definitions(editor_smoke PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_XINPUT=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_Name="voxforma")

    add_executable(vowels_smoke tests/vowels_smoke.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_link_libraries(vowels_smoke PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    target_compile_definitions(vowels_smoke PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_XINPUT=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_Name="voxforma")

    get_target_property(vox_vst3_artefact_for_smoke voxforma_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
    add_executable(vst3_smoke tests/vst3_smoke.cpp)
    target_link_libraries(vst3_smoke PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
    target_compile_definitions(vst3_smoke PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_XINPUT=0
        JUCE_PLUGINHOST_VST3=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        VOXFORMA_VST3_PATH="${vox_vst3_artefact_for_smoke}")
    add_dependencies(vst3_smoke voxforma_VST3)
endif()
