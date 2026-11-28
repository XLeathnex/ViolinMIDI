// The main window: the emblem and the presets across the top, the live display below them,
// the settings in columns (in the order the signal takes), one status line at
// the bottom.
//
// Adding a control: create it in the constructor, add it to a column with
// addItem(), read and write it in pushSettings() / applySettingsToControls().
// The layout follows by itself. An engine setting is stored (settings file and
// presets) by one line in SettingsFields.cpp; to make it controllable by MIDI
// as well, add one entry in Params.cpp.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioHost.h"
#include "AudioPanel.h"
#include "CameraSettings.h"
#include "CameraTracker.h"
#include "MidiRemote.h"
#include "PadHud.h"
#include "PresetBar.h"
#include "PresetStore.h"
#include "SettingsStore.h"
#include "SplashOverlay.h"
#include "TrackingPanel.h"
#include "UpdateManager.h"
#include "VoicingPanel.h"
#include "Widgets.h"

#include <memory>
#include <vector>

/** UTF-8 text literal to juce::String (the sources contain German text). */
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

/** Two controls in one row: one of them with a fixed width. */
class SplitRow final : public juce::Component
{
public:
    void set (juce::Component& leftControl, juce::Component& rightControl, int fixedWidth, bool fixedIsRight = false)
    {
        left = &leftControl;
        right = &rightControl;
        width = fixedWidth;
        rightFixed = fixedIsRight;
        addAndMakeVisible (leftControl);
        addAndMakeVisible (rightControl);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (left == nullptr || right == nullptr)
            return;

        if (rightFixed)
        {
            right->setBounds (r.removeFromRight (width));
            r.removeFromRight (6);
            left->setBounds (r);
        }
        else
        {
            left->setBounds (r.removeFromLeft (width));
            r.removeFromLeft (6);
            right->setBounds (r);
        }
    }

private:
    juce::Component* left = nullptr;
    juce::Component* right = nullptr;
    int width = 60;
    bool rightFixed = false;
};

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    struct Options
    {
        bool openAudio = true;          ///< false: no audio input, no MIDI ports (layout checks)
        bool demo = false;              ///< play a built-in melody into the displays
        bool showSettingsAtStart = false;
        int  settingsPage = 0;          ///< page of the settings window opened at start
        bool showVoicingAtStart = false; ///< open the free-voicing panel (layout checks)
        int  showPanelAtStart = 0;       ///< 1: tracking filters, 2: audio harmonizer, 3: camera (layout checks)
        int  showMenuAtStart = 0;        ///< 1: open the list of scales, 2: the list of voicings (layout checks)
        juce::String notice;            ///< shown in the status line at start
    };

    explicit MainComponent (const Options&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Window position, so the window opens where it was left. */
    bool savedWindowPosition (juce::Point<int>& topLeft) const;
    void rememberWindowPosition (juce::Point<int> topLeft);

private:
    struct Item
    {
        juce::Label* name = nullptr;
        juce::Label* value = nullptr;     ///< shown right-aligned next to the name, may be null
        juce::Component* control = nullptr;
    };

    struct Column
    {
        juce::String title;
        std::vector<Item> items;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void addColumn (const juce::String& title);
    void addItem (juce::Label& name, juce::Component& control, juce::Label* value = nullptr);
    void setupName (juce::Label&, const juce::String& text);
    void setupValue (juce::Label&);
    void setupSlider (juce::Slider&, double low, double high, double step);
    void setupBox (juce::ComboBox&, const juce::String& tooltip);
    void setupSegments (juce::TextButton* buttons, ButtonRow& row, int radioGroup,
                        const juce::StringArray& names, const juce::StringArray& tips);
    void setupLatch (juce::TextButton&, const juce::String& text, const juce::String& tooltip);
    static int selectedSegment (const juce::TextButton* buttons, int count);

    void applySettingsToControls();
    void pushSettings();        // controls -> engine and settings file
    void updateValueLabels();
    void refreshVoicingLabel(); // "Enger Dreiklang FV" while the voices differ from the preset's
    void initialiseAudio();     // audio in, MIDI ports, update check: after the window (and the start animation) is up
    void setMode (int index);
    void remoteControl (int paramIndex, bool isNote, int value);
    void allNotesOff();
    void showSettings();
    void showVoicingPanel();
    void showTrackingPanel();
    void showAudioPanel();
    AudioPanel::Values audioPanelValues() const;
    void showCamera();
    juce::String audioOutputInfo (bool& isProblem) const;
    void ensureAudioOutput();
    void useSmallBuffer();
    void applyLowestLatency();
    vm::Settings hostSettings() const;   // what the engine gets: the settings plus what the camera adds
    void applyCameraSettings (const CameraSettings&);
    void updateCameraStatus();
    void cameraStateChanged();
    void cameraFrame (const juce::Image& preview, const cam::Pose&);
    void cameraAction (int action);
    void sendPadValue (int slot, int target, int value);
    static juce::String padTargetName (int target);
    void refreshUpdateButton();
    AudioHost::Meters demoMeters();
    void updateStatus (const AudioHost::Meters&);

    juce::ApplicationProperties appProperties;
    std::unique_ptr<SettingsStore> store;
    AppSettings settings;

    AudioHost host;
    juce::AudioDeviceManager deviceManager;
    MidiRemote remote;
    UpdateManager updates;
    juce::TooltipWindow tooltips { this, 700 };
    juce::Component::SafePointer<juce::DialogWindow> settingsDialog;

    // presets
    std::unique_ptr<PresetStore> presets;
    std::unique_ptr<PresetBar> presetBar;
    juce::String presetImage;        // the settings as the current preset holds them
    juce::TextButton audioButton, cameraButton;   // at the right of the presets

    // camera control: the violin scroll as a two-axis pad, and head gestures
    CameraTracker camera;
    CameraSettings cameraSettings;
    cam::PadTracker pad;
    cam::GestureDetector gestures;
    juce::Component::SafePointer<juce::DialogWindow> cameraDialog;
    float cameraMix = 1.0f;          // 0..1 from an axis of the pad, for the target "Harmonie-Mix"
    int   padSent[AudioHost::kControllerSlots] = { -1, -1 };   // the last value each axis sent, -1 = none yet
    bool  cameraHasPicture = false;  // a frame arrived since the camera was switched on

    juce::Component::SafePointer<juce::DialogWindow> audioDialog;   // the audio harmonizer's window
    juce::Component::SafePointer<VoicingPanel> voicingPanel;        // the open voicing matrix, if any
    int voicingPanelType = -1;                                      // ...and the voicing it was opened for
    juce::Component::SafePointer<TrackingPanel> trackingPanel;      // the open tracking filter callout, if any

    LiveDisplay display;
    PadHud padHud;                   // over the display while the pad moves values
    juce::TextButton masterButton;   // these three sit on the display, under the note
    juce::TextButton freezeSoloButton, freezeHarmonyButton;

    // input
    LevelMeter levelMeter;
    juce::Label meterName, meterValue, gainName, gainValue, gateName, gateValue, gateOffName, gateOffValue;
    juce::Slider gainSlider, gateSlider, gateOffSlider;

    // detection
    juce::Label responseName, lowestName, tuningName, tuningValue, octaveName, articulationName;
    juce::TextButton responseButtons[3], articulationButtons[3];
    ButtonRow responseRow, articulationRow;
    juce::ComboBox lowestBox;
    juce::Slider tuningSlider;
    juce::TextButton octaveButton;

    // pitch
    juce::Label modeName, bendRangeName, smoothingName, smoothingValue, overlapName, overlapValue, releaseName, releaseValue;
    juce::TextButton modeButtons[3];
    ButtonRow modeRow;
    juce::ComboBox bendRangeBox;
    juce::Slider smoothingSlider, overlapSlider, releaseSlider;

    // key
    juce::Label keyName, filterName, holdName;
    juce::ComboBox rootBox, scaleBox;
    SplitRow keyRow;
    juce::TextButton filterButtons[2], holdButtons[2];
    ButtonRow filterRow, holdRow;

    // harmony
    juce::Label triadName, voicingName, harmonyModeName, triadChannelName, offsetName, offsetValue;
    juce::Slider offsetSlider;
    juce::TextButton triadButtons[2], harmonyModeButtons[2], voicingEditButton;
    ButtonRow triadRow, harmonyModeRow;
    juce::ComboBox voicingBox, triadChannelBox;
    SplitRow voicingRow;

    // MIDI
    juce::Label channelName, dynamicsName, targetName, dynSmoothingName, dynSmoothingValue, velocityName, velocityValue;
    juce::ComboBox channelBox, dynamicsBox, targetBox;
    juce::Slider dynSmoothingSlider, velocitySlider;

    // status line
    juce::TextButton settingsButton, updateButton, panicButton;
    juce::String statusText;
    bool statusIsProblem = false;

    std::vector<Column> columns;

    std::unique_ptr<SplashOverlay> splash;   // the start animation, until audio and MIDI are up

    Options options;
    juce::String audioInitError;
    bool midiPortOk = false, loading = false, audioReady = false;
    int silentTicks = 0, clipTicks = 0;
    int lastBufferSize = 0;          // to notice a change made in the settings window
    float lastLevelDb = -120.0f;
    double demoTime = 0.0;
    unsigned int demoMessages = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
