cmake_minimum_required(VERSION 3.24)
project(DettatoPlugin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal macOS plug-in architectures")
endif()
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version")
endif()

set(JUCE_SOURCE_DIR "" CACHE PATH "Path to an existing JUCE 8 checkout")
if(JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(DETTATO_FORMATS VST3)
option(DETTATO_BUILD_STANDALONE "Also build a standalone test application" OFF)
if(DETTATO_BUILD_STANDALONE)
    list(APPEND DETTATO_FORMATS Standalone)
endif()
if(APPLE)
    list(APPEND DETTATO_FORMATS AU)
endif()

option(DETTATO_ENABLE_AAX "Build AAX (requires the proprietary Avid SDK)" OFF)
set(AAX_SDK_PATH "" CACHE PATH "Path to the Avid AAX SDK")
if(DETTATO_ENABLE_AAX)
    if(NOT APPLE)
        message(FATAL_ERROR "Dettato AAX can only be built on macOS with the Avid SDK")
    endif()
    if(AAX_SDK_PATH STREQUAL "" OR NOT EXISTS "${AAX_SDK_PATH}/Interfaces/ACF")
        message(FATAL_ERROR "DETTATO_ENABLE_AAX=ON requires -DAAX_SDK_PATH=/path/to/AAX_SDK with Interfaces/ACF")
    endif()
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
    list(APPEND DETTATO_FORMATS AAX)
endif()

juce_add_plugin(Dettato
    VERSION 0.1.0
    COMPANY_NAME "KobiMusic"
    COMPANY_WEBSITE "https://kobi.music"
    BUNDLE_ID com.kobimusic.dettato
    PLUGIN_MANUFACTURER_CODE Kobi
    PLUGIN_CODE Dett
    FORMATS ${DETTATO_FORMATS}
    PRODUCT_NAME "Dettato"
    DESCRIPTION "Multi-instrument audio-to-MIDI transcription"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_IDENTIFIER com.kobimusic.dettato
    AAX_CATEGORY AAX_ePlugInCategory_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Dettato PRIVATE
    source/EngineClient.cpp
    source/JobState.cpp
    source/PluginEditor.cpp
    source/PluginProcessor.cpp)

target_include_directories(Dettato PRIVATE source)
target_compile_definitions(Dettato PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(Dettato PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

include(CTest)
if(BUILD_TESTING)
    add_executable(dettato_job_state_test tests/job_state_test.cpp source/JobState.cpp)
    target_include_directories(dettato_job_state_test PRIVATE source)
    target_compile_features(dettato_job_state_test PRIVATE cxx_std_17)
    add_test(NAME job_state COMMAND dettato_job_state_test)

    juce_add_console_app(dettato_host_smoke PRODUCT_NAME "Dettato Host Smoke")
    target_sources(dettato_host_smoke PRIVATE
        tests/host_smoke.cpp
        source/EngineClient.cpp
        source/JobState.cpp
        source/PluginEditor.cpp
        source/PluginProcessor.cpp)
    target_include_directories(dettato_host_smoke PRIVATE source)
    target_compile_definitions(dettato_host_smoke PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Build_Standalone=0 JucePlugin_Name="Dettato")
    target_link_libraries(dettato_host_smoke PRIVATE juce::juce_audio_utils)
    add_test(NAME host_smoke COMMAND dettato_host_smoke $<TARGET_FILE:dettato_fake_engine>)

    add_executable(dettato_fake_engine tests/fake_engine.cpp)
    target_compile_features(dettato_fake_engine PRIVATE cxx_std_17)

    juce_add_console_app(dettato_engine_client_test PRODUCT_NAME "Dettato Engine Client Test")
    target_sources(dettato_engine_client_test PRIVATE tests/engine_client_test.cpp source/EngineClient.cpp)
    target_include_directories(dettato_engine_client_test PRIVATE source)
    target_compile_definitions(dettato_engine_client_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(dettato_engine_client_test PRIVATE juce::juce_core juce::juce_events)
    add_test(NAME engine_transport COMMAND dettato_engine_client_test $<TARGET_FILE:dettato_fake_engine>)

    juce_add_console_app(dettato_vst3_bundle_smoke PRODUCT_NAME "Dettato VST3 Bundle Smoke")
    target_sources(dettato_vst3_bundle_smoke PRIVATE tests/vst3_bundle_smoke.cpp)
    target_compile_definitions(dettato_vst3_bundle_smoke PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(dettato_vst3_bundle_smoke PRIVATE juce::juce_audio_processors juce::juce_gui_basics)
    add_test(NAME vst3_bundle_smoke COMMAND dettato_vst3_bundle_smoke
        "${CMAKE_CURRENT_BINARY_DIR}/Dettato_artefacts/$<CONFIG>/VST3/Dettato.vst3")
endif()
