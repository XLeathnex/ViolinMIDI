// The main window: live view on the left, settings on the right.
//
// Adding a control: create it in the constructor, put it on a card with
// addRow() and store its value in SettingsStore. The layout follows by itself.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioHost.h"
#include "SettingsStore.h"
#include "UpdateManager.h"
#include "Widgets.h"

#include <vector>

/** UTF-8 text literal to juce::String (the sources contain German text). */
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

/** Buttons side by side, sharing the width evenly (segmented control). */
class ButtonRow final : public juce::Component
{
public:
    void add (juce::Button& b)
    {
        items.push_back (&b);
        addAndMakeVisible (b);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int n = (int) items.size();
        for (int i = 0; i < n; ++i)
            items[(size_t) i]->setBounds (r.removeFromLeft (r.getWidth() / (n - i)));
    }

private:
    std::vector<juce::Button*> items;
};

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    struct Options
    {
        bool openAudio = true;          ///< false: no audio input, no MIDI port (layout checks)
        bool demo = false;              ///< play a built-in melody into the displays
        bool showSettingsAtStart = false;
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
    struct Row
    {
        juce::Component* label = nullptr;   ///< nullptr: the control takes the full width
        juce::Component* control = nullptr;
        int height = 28;
        bool dividerAbove = false;
    };

    struct Card
    {
        juce::String title;
        std::vector<Row> rows;
        juce::Rectangle<int> bounds;
        std::vector<int> dividerY;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    Card& addCard (const juce::String& title);
    void addRow (juce::Label* label, juce::Component& control, int height = 28, bool dividerAbove = false);
    void setupLabel (juce::Label&, const juce::String& text);
    void setupSlider (juce::Slider&, double low, double high, double step, const juce::String& suffix, int decimals);
    void setupSegments (juce::TextButton* buttons, ButtonRow& row, int radioGroup,
                        const juce::StringArray& names, const juce::StringArray& tips);

    void applySettingsToControls();
    void pushSettings();        // controls -> engine and settings file
    void setMode (int index);
    void showSettings();
    void refreshUpdateButton();
    AudioHost::Meters demoMeters();
    void updateStatus (const AudioHost::Meters&);

    juce::ApplicationProperties appProperties;
    std::unique_ptr<SettingsStore> store;
    AppSettings settings;

    AudioHost host;
    juce::AudioDeviceManager deviceManager;
    UpdateManager updates;
    juce::TooltipWindow tooltips { this, 700 };
    juce::Component::SafePointer<juce::DialogWindow> settingsDialog;

    NoteDisplay noteDisplay;
    PitchHistory history;
    LevelMeter levelMeter;

    // expression
    juce::TextButton modeButtons[3];
    ButtonRow modeRow;
    juce::Label bendRangeLabel, dynamicsLabel, velocityLabel;
    juce::ComboBox bendRangeBox, dynamicsBox;
    juce::Slider velocitySlider;

    // input and detection
    juce::Label meterLabel, gainLabel, gateLabel, responseLabel, lowestLabel, tuningLabel;
    juce::Slider gainSlider, gateSlider, tuningSlider;
    juce::TextButton responseButtons[3];
    ButtonRow responseRow;
    juce::ComboBox lowestBox;

    // header and footer
    juce::ComboBox channelBox;
    juce::TextButton settingsButton, updateButton, panicButton;

    std::vector<Card> cards;

    // status shown in the header and the footer
    juce::String inputChip, midiChip, footerText;
    juce::Colour inputDot, midiDot, footerDot;

    Options options;
    juce::String audioInitError;
    bool midiPortOk = false, loading = false;
    int silentTicks = 0, clipTicks = 0, midiTicks = 0;
    unsigned int lastMidiCount = 0;
    double demoTime = 0.0;
    unsigned int demoMessages = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
