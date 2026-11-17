cmake_minimum_required(VERSION 3.22)

project(DGUITAR VERSION 1.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS" FORCE)
endif()

# JUCE : soit un dossier local (-DJUCE_DIR=...), soit téléchargé automatiquement
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(DGUITAR_LINUX_CHECK "Build Linux test targets (no Xrandr/Xinerama/Xcursor/ALSA)" OFF)

file(GLOB IMAGE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/images/*.png)
file(GLOB FONT_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/*.ttf)
juce_add_binary_data(DGuitarData SOURCES ${IMAGE_FILES} ${FONT_FILES})

set(DGUITAR_FORMATS AU VST3)
if(DGUITAR_LINUX_CHECK)
    set(DGUITAR_FORMATS VST3)
endif()

juce_add_plugin(DGUITAR
    COMPANY_NAME "Danova"
    COMPANY_WEBSITE "https://danova.app"
    BUNDLE_ID "com.danova.dguitar"
    PLUGIN_MANUFACTURER_CODE Dnva
    PLUGIN_CODE Dgtr
    FORMATS ${DGUITAR_FORMATS}
    PRODUCT_NAME "D-GUITAR"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/icon-1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon/icon-1024.png"
    DESCRIPTION "D-GUITAR - guitares électriques à modélisation physique, Studio Danova")

set(DGUITAR_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(DGUITAR PRIVATE ${DGUITAR_SOURCES})
target_include_directories(DGUITAR PRIVATE Source)

set(DGUITAR_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
if(DGUITAR_LINUX_CHECK)
    list(APPEND DGUITAR_DEFS JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_compile_definitions(DGUITAR PUBLIC ${DGUITAR_DEFS})

if(MSVC)
    target_compile_options(DGUITAR PRIVATE /utf-8)
endif()

target_link_libraries(DGUITAR
    PRIVATE
        DGuitarData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Outil de test (Linux) : rendu hors ligne + capture de l'interface
if(DGUITAR_LINUX_CHECK)
    juce_add_console_app(DGuitarTests PRODUCT_NAME "DGuitarTests")
    target_sources(DGuitarTests PRIVATE tests/TestMain.cpp ${DGUITAR_SOURCES})
    target_compile_definitions(DGuitarTests PRIVATE ${DGUITAR_DEFS}
        JucePlugin_Name="D-GUITAR" JucePlugin_VersionString="${PROJECT_VERSION}" DGUITAR_TESTS=1)
    target_include_directories(DGuitarTests PRIVATE Source)
    target_link_libraries(DGuitarTests PRIVATE DGuitarData juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
