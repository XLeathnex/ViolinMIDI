// "ViolinMIDI --selftest": checks on the real machine what cannot be checked
// anywhere else - that the engine works inside the app binary and that the
// virtual MIDI port really delivers messages to other programs.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <functional>
#include <memory>

/** Runs the checks on a background thread and reports the process exit code. */
class SelfTest final : private juce::Thread
{
public:
    explicit SelfTest (std::function<void (int)> onFinished);
    ~SelfTest() override;

    void start() { startThread(); }

private:
    void run() override;

    std::function<void (int)> finished;
};
