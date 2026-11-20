cmake_minimum_required(VERSION 3.22)
project(PhoenixII_papielguapo VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory("C:/Users/anton/GrowthTune/third_party/JUCE" JUCE_build EXCLUDE_FROM_ALL)

juce_add_binary_data(PhoenixIIAssets
    SOURCES
        extracted_assets/res_PNG_31010_60x8100.png
        extracted_assets/res_PNG_31011_515x161.png
        extracted_assets/res_PNG_31013_60x180.png
        extracted_assets/res_PNG_31015_60x300.png
)

juce_add_plugin(PhoenixII_papielguapo
    COMPANY_NAME "papielguapo"
    PLUGIN_MANUFACTURER_CODE Papi
    PLUGIN_CODE Phx2
    FORMATS VST3 Standalone
    PRODUCT_NAME "Phoenix II (papielguapo)"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion Mastering
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(PhoenixII_papielguapo PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    PhoenixDSP.h
    PhoenixDSPData.h
)

target_include_directories(PhoenixII_papielguapo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_compile_definitions(PhoenixII_papielguapo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(PhoenixII_papielguapo
    PRIVATE
        PhoenixIIAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
)
