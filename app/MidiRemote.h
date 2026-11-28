// Remote control: a MIDI input port, one binding per parameter and MIDI learn.
//
// Threads: the port delivers messages on a MIDI thread. They go through a
// lock-free queue to the message thread, where bindings are matched and
// onParam is called. Everything public here is for the message thread only.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class MidiRemote final : public juce::ChangeBroadcaster,   // bindings, port or learn state changed
                         private juce::MidiInputCallback,
                         private juce::AsyncUpdater
{
public:
    struct Binding
    {
        enum class Type { Off = 0, Controller = 1, Note = 2 };
        Type type = Type::Off;
        int channel = 0;   ///< 0 = every channel, else 1..16
        int number = 0;    ///< controller or note number, 0..127

        bool operator== (const Binding& o) const noexcept { return type == o.type && channel == o.channel && number == o.number; }
        bool operator!= (const Binding& o) const noexcept { return ! (*this == o); }
    };

    MidiRemote();
    ~MidiRemote() override;

    //--- input port
    /** Our own virtual output must not be offered as an input: that would be a loop. */
    void setExcludedName (const juce::String& ownOutputName);
    juce::StringArray availableInputs() const;
    /** Opens the port with this name; an empty name closes the input. The name
        is remembered either way, so the port is opened as soon as it appears. */
    void setInput (const juce::String& name);
    juce::String inputName() const   { return wantedInput; }
    bool isInputOpen() const         { return input != nullptr; }

    //--- bindings: one per parameter, same order as params::all()
    Binding binding (int paramIndex) const;
    void setBinding (int paramIndex, const Binding&);
    /** For the settings file. Parameters are stored by id, so the list may grow or change order. */
    juce::String toJson() const;
    void fromJson (const juce::String&);

    //--- learn: the next controller or note-on becomes the binding of that parameter
    void startLearn (int paramIndex);
    void cancelLearn();
    int learningIndex() const noexcept { return learning; }

    //--- program change: switches presets; never a binding
    /** Which channel's program changes are passed on: -1 = none, 0 = every channel, 1..16. */
    void setProgramChannel (int channel);
    int  programChannel() const noexcept   { return programChan; }

    /** The last message received, for the table ("CC 64, Kanal 1, Wert 127"); empty before the first. */
    juce::String lastMessageText() const { return lastText; }

    /** Message thread: a bound controller (value 0..127) or note-on (velocity) arrived. */
    std::function<void (int paramIndex, bool isNote, int value)> onParam;

    /** Message thread: a program change arrived on the chosen channel, program 0..127. */
    std::function<void (int program)> onProgramChange;

private:
    struct Message { uint8_t status = 0, number = 0, value = 0; };   // status with channel; value is 0 for a program change

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;
    void tryOpen();

    juce::String wantedInput, excluded, lastText;
    std::unique_ptr<juce::MidiInput> input;
    juce::MidiDeviceListConnection deviceListConnection;
    std::vector<Binding> bindings;
    juce::Array<juce::var> foreign;   // stored bindings of parameters this version does not have: kept for the versions that do
    int learning = -1;
    int programChan = 0;

    // MIDI thread -> message thread
    static constexpr int kQueueSize = 256;
    juce::AbstractFifo fifo { kQueueSize };
    std::array<Message, kQueueSize> queue {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiRemote)
};
