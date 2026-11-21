// The window: pitch display, level meter and the handful of settings.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "AudioHost.h"
#include "UpdateManager.h"

/** UTF-8 text literal to juce::String (the sources contain German text). */
inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }

//==============================================================================
/** Large note name with a tuner-style cents scale. */
class PitchDisplay final : public juce::Component
{
public:
    void update (const AudioHost::Meters& meters, float gateDb, bool midiActive);
    void paint (juce::Graphics&) override;

private:
    bool  active = false, heard = false, midi = false;
    int   note = -1;
    float pitch = 0.0f;
};

//==============================================================================
/** Input level with the gate threshold marked. */
class LevelMeter final : public juce::Component
{
public:
    void update (float levelDb, float gateDb, bool clipped);
    void paint (juce::Graphics&) override;

private:
    float shown = -120.0f, gate = -45.0f;
    bool  clip = false;
};

//==============================================================================
class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    /** @param openAudio            false shows the window without opening an audio input
        @param startupNotice        something to tell the user in the status lines, or empty
        @param showSettingsAtStart  opens the settings window right away */
    MainComponent (bool openAudio, const juce::String& startupNotice, bool showSettingsAtStart);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void loadSettings();
    void saveSettings();
    void pushSettings();              // controls -> engine and settings file
    void setMode (int index);
    void showSettings();
    void refreshUpdateButton();
    void saveAudioState();
    void updateStatus (const AudioHost::Meters&);
    juce::PropertiesFile* properties();

    AudioHost host;
    juce::AudioDeviceManager deviceManager;
    juce::ApplicationProperties appProperties;
    juce::TooltipWindow tooltips { this, 600 };
    UpdateManager updates;
    juce::Component::SafePointer<juce::DialogWindow> settingsDialog;

    PitchDisplay pitchDisplay;
    LevelMeter levelMeter;

    juce::Label gainLabel, gateLabel, bendRangeLabel, dynamicsLabel, velocityLabel;
    juce::Label responseLabel, lowestLabel, tuningLabel, channelLabel, statusLabel;
    juce::Slider gainSlider, gateSlider, velocitySlider, tuningSlider;
    juce::TextButton modeButtons[3];
    juce::ComboBox bendRangeBox, dynamicsBox, responseBox, lowestBox, channelBox;
    juce::TextButton settingsButton, updateButton, panicButton;

    struct Heading { int y; juce::String text; };
    juce::Array<Heading> headings;

    juce::String audioInitError;
    bool midiPortOk = false;
    bool loading = false;
    int silentTicks = 0, clipTicks = 0, midiTicks = 0;
    unsigned int lastMidiCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
