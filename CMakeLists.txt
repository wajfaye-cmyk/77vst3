cmake_minimum_required(VERSION 3.22)

project(SEVENTYSEVEN_MP3 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the plugin needs no Visual C++ redistributable on the user's machine
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(SeventySevenMP3
    COMPANY_NAME                "WajFay"
    COMPANY_WEBSITE             "https://www.instagram.com/wajfay"
    COMPANY_COPYRIGHT           "Copyright (c) 2026 WajFay. All rights reserved."
    BUNDLE_ID                   "com.wajfay.77mp3"
    PLUGIN_MANUFACTURER_CODE    Wjfy
    PLUGIN_CODE                 S7mp
    PRODUCT_NAME                "77mp3"
    PLUGIN_NAME                 "77.MP3"
    DESCRIPTION                 "Vocal Draw and Auto-Tune"
    VERSION                     ${PROJECT_VERSION}
    FORMATS                     VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             Fx Pitch\ Shift
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_1024.png"
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_generate_juce_header(SeventySevenMP3)

juce_add_binary_data(SeventySevenAssets
    SOURCES
        Resources/logo_mark.png
        Resources/fonts/Inter-Regular.otf
        Resources/fonts/Inter-Medium.otf
        Resources/fonts/Inter-SemiBold.otf
        Resources/fonts/Inter-Bold.otf)

target_sources(SeventySevenMP3 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/PianoRoll.cpp)

target_include_directories(SeventySevenMP3 PRIVATE Source)

target_compile_definitions(SeventySevenMP3 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(SeventySevenMP3
    PRIVATE
        SeventySevenAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
