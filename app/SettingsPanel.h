// The settings window, three pages: audio input, remote control by MIDI, versions.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "MappingPanel.h"
#include "MidiRemote.h"
#include "UpdateManager.h"
#include "Widgets.h"

class SettingsPanel final : public juce::Component,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    SettingsPanel (juce::AudioDeviceManager& deviceManager, UpdateManager& updateManager, MidiRemote& remote, int page = 0);
    ~SettingsPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuildList();
    void selectionChanged();
    void refreshStatus();
    void showPage (int index);

    UpdateManager& updates;
    juce::TextButton pageButtons[3];
    ButtonRow pageRow;
    int page = 0;

    juce::AudioDeviceSelectorComponent selector;
    MappingPanel mapping;

    juce::Label runningLabel, statusLabel;
    juce::ComboBox versionBox;
    juce::TextEditor notes;
    juce::TextButton useButton, checkButton;

    juce::StringArray itemVersions; // version behind each list entry
    juce::StringArray itemNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};
