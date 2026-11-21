// The settings window: audio input on top, version choice below.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "UpdateManager.h"

class SettingsPanel final : public juce::Component,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    SettingsPanel (juce::AudioDeviceManager& deviceManager, UpdateManager& updateManager);
    ~SettingsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuildList();
    void selectionChanged();
    void refreshStatus();

    UpdateManager& updates;
    juce::AudioDeviceSelectorComponent selector;

    juce::Label runningLabel, statusLabel;
    juce::ComboBox versionBox;
    juce::TextEditor notes;
    juce::TextButton useButton, checkButton;

    juce::StringArray itemVersions; // version behind each list entry
    juce::StringArray itemNotes;
    int headingY = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};
