// The MIDI input of the chord keyboard (see ChordInput.h).
#include "ChordInput.h"

#include "AudioHost.h"

ChordInput::ChordInput (AudioHost& h) : host (h) {}

ChordInput::~ChordInput() = default;

void ChordInput::setExcludedName (const juce::String& ownOutputName)
{
    excluded = ownOutputName;
}

juce::StringArray ChordInput::availableInputs() const
{
    return {};
}

void ChordInput::setInput (const juce::String& name)
{
    wanted = name;
}

bool ChordInput::isInputOpen() const
{
    juce::ignoreUnused (host);
    return false;
}

void ChordInput::learnNextKey (std::function<void (int)>) {}

void ChordInput::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) {}

void ChordInput::handleAsyncUpdate() {}
