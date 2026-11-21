cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Intel + Apple Silicon). Must be set before project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

# Windows: link the C++ runtime statically so the plugin needs no VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(FlatTop VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE (same framework PeakEater uses), pulled in automatically at configure time
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(FlatTop
    COMPANY_NAME             "FlatTop Audio"
    PLUGIN_MANUFACTURER_CODE Ftop
    PLUGIN_CODE              Ftcl
    FORMATS                  VST3 Standalone
    VST3_CATEGORIES          Fx Distortion Dynamics
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE
    PRODUCT_NAME             "FlatTop")

target_sources(FlatTop PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp)

target_include_directories(FlatTop PRIVATE source)

target_compile_definitions(FlatTop PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(FlatTop
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Optional: standalone DSP unit tests (no JUCE needed)
#   cmake -B build -DFLATTOP_BUILD_TESTS=ON && cmake --build build --target flattop_dsp_test
# ---------------------------------------------------------------------------
option(FLATTOP_BUILD_TESTS "Build DSP unit tests" OFF)
if(FLATTOP_BUILD_TESTS)
    add_executable(flattop_dsp_test tests/dsp_test.cpp)
    target_include_directories(flattop_dsp_test PRIVATE source)
endif()
