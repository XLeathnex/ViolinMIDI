// The MIDI input of the chord keyboard (2.1, "Externe Chords"), e.g. the
// Network MIDI session of the keyboardist's Mac. Every channel counts. The port
// is opened by name and reopened when it appears again, like MidiRemote's; the
// messages that matter for chords (note on/off, CC 64/120/121/123) go to the
// audio thread through AudioHost::pushChordMidi, without locks or allocation.
// When the open device disappears, the held keys are released
// (AudioHost::requestChordKeysUp); a latched chord stays.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>

class AudioHost;

class ChordInput final : public juce::ChangeBroadcaster,   // the port opened or closed, the device list changed
                         private juce::MidiInputCallback,
                         private juce::AsyncUpdater
{
public:
    explicit ChordInput (AudioHost&);
    /** Closes the port: no callback after this. */
    ~ChordInput() override;

    /** Never offer or open this one (our own "VioLogic" output). */
    void setExcludedName (const juce::String& ownOutputName);
    /** The MIDI inputs that can be chosen, by name. */
    juce::StringArray availableInputs() const;
    /** "" = none. Remembered, and opened when it appears. */
    void setInput (const juce::String& name);
    juce::String inputName() const    { return wanted; }
    bool isInputOpen() const;

    /** The next note-on (any key, any channel) is handed to `callback` on the message thread,
        once ("Lernen" of the split). Another call replaces it; nullptr cancels. */
    void learnNextKey (std::function<void (int note)> callback);
    /** Messages received so far (the panel's activity dot). */
    unsigned int messagesReceived() const noexcept   { return received.load(); }

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;   // MIDI thread
    void handleAsyncUpdate() override;

    AudioHost& host;
    juce::String wanted, excluded;
    std::atomic<unsigned int> received { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordInput)
};
