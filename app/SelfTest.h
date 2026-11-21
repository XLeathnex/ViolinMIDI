// "ViolinMIDI --selftest": checks on the real machine what cannot be checked
// anywhere else - that the engine works inside the app binary and that the
// virtual MIDI port really delivers messages to other programs.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>
#include <memory>

/** Runs the checks on the message thread, where JUCE's MIDI device handling
    lives, and reports the process exit code when done. */
class SelfTest final : private juce::Timer
{
public:
    explicit SelfTest (std::function<void (int)> onFinished);
    ~SelfTest() override;

    void start();

private:
    struct Collector;
    enum class State { waitingForPort, sending, done };

    void timerCallback() override;
    void finish (bool midiOk);

    std::function<void (int)> finished;
    std::unique_ptr<Collector> collector;
    std::unique_ptr<juce::MidiOutput> output;
    std::unique_ptr<juce::MidiInput> input;
    State state = State::done;
    bool engineOk = false;
    int ticks = 0;
};
