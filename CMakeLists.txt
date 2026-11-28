cmake_minimum_required(VERSION 3.22)

# Runs on every Mac that can run a current MainStage, Intel and Apple Silicon.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(ViolinMIDI VERSION 2.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VIOLINMIDI_BUILD_APP   "Build the application (needs JUCE)" ON)
option(VIOLINMIDI_BUILD_TESTS "Build the tests of the pitch-to-MIDI core" ON)

# --- pitch-to-MIDI core: plain C++, no dependencies --------------------------
add_library(violinmidi_core STATIC
    core/PitchDetector.cpp
    core/NoteTracker.cpp
    core/OutputStage.cpp
    core/Harmonizer.cpp
    core/Engine.cpp
    core/SettingsText.cpp
    core/Chord.cpp
    core/ChordTracker.cpp
    core/Profiles.cpp
    core/CalibrationProtocol.cpp
    core/CalibrationAnalysis.cpp
    core/CalibrationSound.cpp
    core/CalibrationText.cpp
    core/CalibrationReport.cpp)
target_include_directories(violinmidi_core PUBLIC core)

if(MSVC)
    target_compile_options(violinmidi_core PRIVATE /W4)
else()
    target_compile_options(violinmidi_core PRIVATE -Wall -Wextra)
endif()

# --- tests --------------------------------------------------------------------
if(VIOLINMIDI_BUILD_TESTS)
    enable_testing()

    # The app's plain C++ units have no dependencies either, so they are tested here too:
    # the camera's logic, marker and bow hand, the updater's rules, the window's geometry
    # and the harmonizer output's routing.
    add_executable(violinmidi_tests tests/test_main.cpp
        app/CameraLogic.cpp
        app/CameraMarker.cpp
        app/CameraBow.cpp
        app/UpdatePolicy.cpp
        app/WindowLayout.cpp
        app/OutputRouting.cpp)
    target_include_directories(violinmidi_tests PRIVATE app)
    target_link_libraries(violinmidi_tests PRIVATE violinmidi_core)
    add_test(NAME core COMMAND violinmidi_tests)

    # Offline tool: runs a WAV recording through the engine and lists the notes.
    add_executable(violinmidi_wav tools/wav2midi.cpp)
    target_link_libraries(violinmidi_wav PRIVATE violinmidi_core)

    # Offline tool: plays the harmony voices of a recording with its own sound (the audio harmonizer).
    add_executable(violinmidi_harmonize tools/harmonize.cpp)
    target_link_libraries(violinmidi_harmonize PRIVATE violinmidi_core)

    # Offline tool: runs the calibration's analysis again on a Profimodus diagnosis folder.
    add_executable(violinmidi_calibrate tools/calibrate.cpp)
    target_link_libraries(violinmidi_calibrate PRIVATE violinmidi_core)
endif()

# --- application --------------------------------------------------------------
if(VIOLINMIDI_BUILD_APP)
    add_subdirectory(app)
endif()
