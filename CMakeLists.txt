cmake_minimum_required(VERSION 3.22)

# macOS builds target High Sierra (10.13) on Intel, which is the oldest
# supported deployment target for JUCE 8. These must be set before project()
# so the compiler checks use the same target; command-line -D overrides them.
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
    endif()

    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Architectures to build")
    endif()
endif()

project(ABmatch VERSION 1.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_TAG "8.0.15")
set(JUCE_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/../.tools/juce-${JUCE_TAG}.tar.gz")

include(FetchContent)

if(EXISTS "${JUCE_ARCHIVE}")
    set(JUCE_URL "${JUCE_ARCHIVE}")
else()
    set(JUCE_URL "https://github.com/juce-framework/JUCE/archive/refs/tags/${JUCE_TAG}.tar.gz")
endif()

FetchContent_Declare(JUCE
    URL ${JUCE_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_BINARY_DIR}/juce"
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ABmatch
    PRODUCT_NAME "ABmatch"
    COMPANY_NAME "Auralis Audio"
    BUNDLE_ID "com.auralisaudio.abmatch"
    PLUGIN_MANUFACTURER_CODE Aura
    PLUGIN_CODE Abm1
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    VST3_AUTO_MANIFEST FALSE
)

target_sources(ABmatch PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(ABmatch PRIVATE Source)

target_compile_definitions(ABmatch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(ABmatch PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
)
