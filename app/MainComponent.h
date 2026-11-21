// The main window: the live display across the top, the settings in columns
// below (in the order the signal takes), one status line at the bottom.
//
// Adding a control: create it in the constructor, add it to a column with
// addItem(), read and write it in pushSettings() / applySettingsToControls()
// and store its value in SettingsStore. The layout follows by itself. To make
// it controllable by MIDI as well, add one entry in Params.cpp.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioHost.h"
#include "MidiRemote.h"
#include "SettingsStore.h"
#include "UpdateManager.h"
#include "Widgets.h"

#include <vector>

/** UTF-8 text literal to juce::String (the sources contain German text). */
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

/** Two controls in one row: the left one with a fixed width. */
class SplitRow final : public juce::Component
{
public:
    void set (juce::Component& leftControl, juce::Component& rightControl, int leftWidth)
    {
        left = &leftControl;
        right = &rightControl;
        width = leftWidth;
        addAndMakeVisible (leftControl);
        addAndMakeVisible (rightControl);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        if (left != nullptr)  left->setBounds (r.removeFromLeft (width));
        r.removeFromLeft (6);
        if (right != nullptr) right->setBounds (r);
    }

private:
    juce::Component* left = nullptr;
    juce::Component* right = nullptr;
    int width = 60;
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
    static int selectedSegment (const juce::TextButton* buttons, int count);

    void applySettingsToControls();
    void pushSettings();        // controls -> engine and settings file
    void updateValueLabels();
    void setMode (int index);
    void remoteControl (int paramIndex, bool isNote, int value);
    void showSettings();
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

    LiveDisplay display;
    juce::TextButton masterButton;   // sits on the display, under the note

    // input
    LevelMeter levelMeter;
    juce::Label meterName, meterValue, gainName, gainValue, gateName, gateValue;
    juce::Slider gainSlider, gateSlider;

    // detection
    juce::Label responseName, lowestName, tuningName, tuningValue;
    juce::TextButton responseButtons[3];
    ButtonRow responseRow;
    juce::ComboBox lowestBox;
    juce::Slider tuningSlider;

    // pitch
    juce::Label modeName, bendRangeName, smoothingName, smoothingValue;
    juce::TextButton modeButtons[3];
    ButtonRow modeRow;
    juce::ComboBox bendRangeBox;
    juce::Slider smoothingSlider;

    // key and triad
    juce::Label keyName, filterName, holdName, triadName;
    juce::ComboBox rootBox, scaleBox;
    SplitRow keyRow;
    juce::TextButton filterButtons[3], holdButtons[2], triadButtons[3];
    ButtonRow filterRow, holdRow, triadRow;

    // MIDI
    juce::Label channelName, triadChannelName, dynamicsName, velocityName, velocityValue;
    juce::ComboBox channelBox, triadChannelBox, dynamicsBox;
    juce::Slider velocitySlider;

    // status line
    juce::TextButton settingsButton, updateButton, panicButton;
    juce::String statusText;
    bool statusIsProblem = false;

    std::vector<Column> columns;

    Options options;
    juce::String audioInitError;
    bool midiPortOk = false, loading = false;
    int silentTicks = 0, clipTicks = 0;
    float lastLevelDb = -120.0f;
    double demoTime = 0.0;
    unsigned int demoMessages = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
