// The MIDI remote page of the settings window: which port the foot controller
// is on, and which controller or note moves which parameter.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MidiRemote.h"

#include <memory>
#include <vector>

class MappingPanel final : public juce::Component,
                           private juce::ChangeListener,
                           private juce::Timer
{
public:
    explicit MappingPanel (MidiRemote&);
    ~MappingPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Row;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuildInputList();
    void refreshRows();
    void refreshLastMessage();

    MidiRemote& remote;

    juce::Label inputLabel, lastLabel;
    juce::ComboBox inputBox;
    juce::StringArray inputNames;   // port behind each list entry, "" = no input
    bool listedMissing = false;     // the last entry is a port that is not plugged in

    // Declared before the viewport, so the viewport lets go of them first.
    juce::Component content;
    std::vector<std::unique_ptr<Row>> rows;
    juce::Viewport viewport;

    int separatorY = 0;
    juce::Rectangle<int> headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MappingPanel)
};
