# The application: audio input, virtual MIDI output and a small window.

# JUCE: use a local checkout if one is given, otherwise fetch the pinned release.
set(VIOLINMIDI_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(VIOLINMIDI_JUCE_DIR)
    add_subdirectory("${VIOLINMIDI_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.6
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# The product is called VioLogic (1.2 to 1.5: Archet). The bundle keeps its file
# name, executable and identifier from the first name (ViolinMIDI): every earlier
# version looks for exactly these when it installs an update, and macOS keeps the
# microphone permission. What the user sees - menu bar, Dock, window, "About" -
# comes from CFBundleName below.
set(VIOLINMIDI_DISPLAY_NAME "VioLogic")
set(VIOLINMIDI_ICON "${PROJECT_SOURCE_DIR}/assets/AppIcon.png")
set(VIOLINMIDI_ICON_ARGS)
if(EXISTS "${VIOLINMIDI_ICON}")
    set(VIOLINMIDI_ICON_ARGS ICON_BIG "${VIOLINMIDI_ICON}")
endif()

juce_add_gui_app(ViolinMIDI
    PRODUCT_NAME "ViolinMIDI"
    COMPANY_NAME "ViolinMIDI"
    BUNDLE_ID "app.violinmidi.ViolinMIDI"
    ${VIOLINMIDI_ICON_ARGS}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "${VIOLINMIDI_DISPLAY_NAME} wandelt das Signal deiner Geige in MIDI-Noten um."
    CAMERA_PERMISSION_ENABLED TRUE
    CAMERA_PERMISSION_TEXT "${VIOLINMIDI_DISPLAY_NAME} kann die Haltung der Geige und Kopfbewegungen als Steuerung nutzen. Das Bild bleibt auf diesem Mac."
    # MainStage is in front while playing. macOS must not put this app to sleep behind it.
    # The minimum system version lets older Macs say clearly why the app will not open.
    PLIST_TO_MERGE "<plist version=\"1.0\"><dict><key>CFBundleName</key><string>${VIOLINMIDI_DISPLAY_NAME}</string><key>CFBundleDisplayName</key><string>${VIOLINMIDI_DISPLAY_NAME}</string><key>NSAppSleepDisabled</key><true/><key>LSMinimumSystemVersion</key><string>${CMAKE_OSX_DEPLOYMENT_TARGET}</string></dict></plist>"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

# Pictures the app draws: the emblem (icon, header, watermark, "About", camera)
# and the frames of the start animation (assets/splash/splash_NN.jpg, played in
# the order of their numbers). Reached as VioLogicAssets::<file name> in the code.
file(GLOB VIOLOGIC_SPLASH_FRAMES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/splash/splash_*.jpg")
list(SORT VIOLOGIC_SPLASH_FRAMES)
juce_add_binary_data(VioLogicAssets
    HEADER_NAME VioLogicAssets.h
    NAMESPACE VioLogicAssets
    SOURCES
        "${PROJECT_SOURCE_DIR}/assets/Emblem.png"
        ${VIOLOGIC_SPLASH_FRAMES})

target_sources(ViolinMIDI PRIVATE
    Main.cpp
    MainComponent.cpp
    AudioHost.cpp
    Brand.cpp
    SettingsStore.cpp
    SettingsFields.cpp
    Params.cpp
    MidiRemote.cpp
    MappingPanel.cpp
    PresetStore.cpp
    PresetBar.cpp
    DescribedMenu.cpp
    VoicingPanel.cpp
    TrackingPanel.cpp
    AudioPanel.cpp
    CameraPanel.cpp
    CameraLogic.cpp
    Theme.cpp
    Widgets.cpp
    SettingsPanel.cpp
    UpdateManager.cpp
    Updater.cpp
    SelfTest.cpp)

# The camera: AVFoundation and Vision on the Mac, a stand-in elsewhere.
if(APPLE)
    target_sources(ViolinMIDI PRIVATE CameraTracker_mac.mm)
    set_source_files_properties(CameraTracker_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(ViolinMIDI PRIVATE
        "-framework AVFoundation" "-framework Vision" "-framework CoreMedia" "-framework CoreVideo")
else()
    target_sources(ViolinMIDI PRIVATE CameraTracker_stub.cpp)
endif()

target_compile_definitions(ViolinMIDI PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ViolinMIDI,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ViolinMIDI,JUCE_VERSION>")

if(MSVC)
    # The sources contain German text in UTF-8.
    target_compile_options(ViolinMIDI PRIVATE /utf-8)
endif()

target_link_libraries(ViolinMIDI
    PRIVATE
        violinmidi_core
        VioLogicAssets
        juce::juce_audio_utils
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
