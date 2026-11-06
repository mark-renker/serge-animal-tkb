# Serge Animal TKB v2 — hand-written C++/JUCE rebuild on the Paperface 50
# framework. Gauntlet gate G1: this file must build AU + VST3 + Standalone +
# satk_gauntlet + satk_uxtests + satk_snapshot with zero warnings in our sources.
cmake_minimum_required(VERSION 3.22)
project(SergeAnimalTKB VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # universal binary (Apple Silicon + Intel) for distribution; override with
  # -DSATK_ARCHS=arm64 for a faster native-only dev build
  set(SATK_ARCHS "arm64;x86_64" CACHE STRING "macOS architectures to build")
  set(CMAKE_OSX_ARCHITECTURES "${SATK_ARCHS}" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# ---- JUCE: use a local checkout when present, otherwise fetch 8.0.6 --------
if(EXISTS "$ENV{HOME}/JUCE/CMakeLists.txt")
  message(STATUS "Using local JUCE at $ENV{HOME}/JUCE")
  add_subdirectory("$ENV{HOME}/JUCE" JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(SATK_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND SATK_FORMATS AU)
endif()

set(SATK_SOURCES
  src/plugin/PluginProcessor.cpp)

function(satk_common target)
  target_compile_definitions(${target} PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(${target} PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
endfunction()

# ---- Instrument (own identity, coexists with v1: aumu / Sat2 / Rnky) --------
# Amended 2026-11-03 (Mark): v1 keeps aumu Satk Rnky; see gauntlet/LOG.md.
juce_add_plugin(SergeAnimalTKB
  PRODUCT_NAME "Serge Animal TKB v2"
  COMPANY_NAME "Renky Denk"
  BUNDLE_ID com.renkydenk.SergeAnimalTKB2
  PLUGIN_MANUFACTURER_CODE Rnky
  PLUGIN_CODE Sat2
  FORMATS ${SATK_FORMATS}
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(SergeAnimalTKB PRIVATE ${SATK_SOURCES})
satk_common(SergeAnimalTKB)

# ---- Gauntlet DSP suite (G2 — no JUCE) --------------------------------------
add_executable(satk_gauntlet tests/GauntletTests.cpp)
set_target_properties(satk_gauntlet PROPERTIES CXX_STANDARD 17)
if(MSVC)
  target_compile_options(satk_gauntlet PRIVATE /W4)
else()
  target_compile_options(satk_gauntlet PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- Foundation perf spike (scratch target, no JUCE; results in SPEC.md §10)
add_executable(satk_perfspike tools/PerfSpike.cpp)
set_target_properties(satk_perfspike PROPERTIES CXX_STANDARD 17)
if(MSVC)
  target_compile_options(satk_perfspike PRIVATE /W4)
else()
  target_compile_options(satk_perfspike PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- UX suite (G2u — JUCE-linked, U01-U04) -----------------------------------
juce_add_console_app(satk_uxtests PRODUCT_NAME "satk_uxtests")
target_sources(satk_uxtests PRIVATE tests/UxTests.cpp src/plugin/PluginProcessor.cpp)
target_compile_definitions(satk_uxtests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
satk_common(satk_uxtests)
target_link_libraries(satk_uxtests PRIVATE juce::juce_gui_extra)

# ---- UI snapshot tool (headless editor -> PNG, for blind critics) -----------
juce_add_console_app(satk_snapshot PRODUCT_NAME "satk_snapshot")
target_sources(satk_snapshot PRIVATE tools/Snapshot.cpp src/plugin/PluginProcessor.cpp)
target_compile_definitions(satk_snapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
satk_common(satk_snapshot)
target_link_libraries(satk_snapshot PRIVATE juce::juce_gui_extra)

enable_testing()
add_test(NAME gauntlet COMMAND satk_gauntlet)
# G2u: needs a logged-in GUI session (documented constraint)
add_test(NAME uxtests COMMAND satk_uxtests)
