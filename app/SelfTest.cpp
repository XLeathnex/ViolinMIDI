#include "SelfTest.h"

#include "Engine.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    void say (const juce::String& text)
    {
        std::printf ("%s\n", text.toRawUTF8());
        std::fflush (stdout);
    }

    /** A bowed-string-like tone (sawtooth) must come out as the right note. */
    bool engineCheck()
    {
        const double fs = 48000.0;
        vm::Engine engine;
        engine.prepare (fs);
        vm::Settings settings;
        vm::MidiEventList events;
        events.reserve (4096);

        std::vector<float> block (64);
        double phase = 0.0;
        int noteOn = -1, noteOff = -1;

        for (int n = 0; n < 48000; n += 64)
        {
            const bool sounding = n > 4800 && n < 36000;
            for (auto& sample : block)
            {
                phase += 220.0 / fs;
                phase -= std::floor (phase);
                sample = sounding ? (float) (0.2 * (2.0 * phase - 1.0)) : 0.0f;
            }
            events.clear();
            engine.process (block.data(), 64, settings, events);
            for (const auto& e : events)
            {
                if ((e.bytes[0] & 0xF0) == 0x90 && noteOn < 0) noteOn = e.bytes[1];
                if ((e.bytes[0] & 0xF0) == 0x80) noteOff = e.bytes[1];
            }
        }

        say ("engine: 220 Hz tone -> note-on " + juce::String (noteOn) + ", note-off " + juce::String (noteOff));
        return noteOn == 57 && noteOff == 57;
    }

    struct Collector final : public juce::MidiInputCallback
    {
        void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
        {
            if (message.isNoteOn() && message.getNoteNumber() == 69 && message.getVelocity() == 100) gotNoteOn = true;
            if (message.isNoteOff() && message.getNoteNumber() == 69) gotNoteOff = true;
            if (message.isPitchWheel() && message.getPitchWheelValue() == 9000) gotBend = true;
            if (message.isController() && message.getControllerNumber() == 11 && message.getControllerValue() == 77) gotController = true;
        }

        bool complete() const { return gotNoteOn && gotNoteOff && gotBend && gotController; }

        std::atomic<bool> gotNoteOn { false }, gotNoteOff { false }, gotBend { false }, gotController { false };
    };

    /** Creates the virtual port exactly as the app does and listens to it
        from the outside, the way MainStage would. */
    bool midiCheck()
    {
        const juce::String name = "ViolinMIDI Selftest";
        auto output = juce::MidiOutput::createNewDevice (name);

       #if ! JUCE_MAC
        if (output == nullptr)
        {
            say ("midi: virtual ports are not available on this system - skipped");
            return true;
        }
       #endif

        if (output == nullptr)
        {
            say ("midi: FAILED to create the virtual port");
            return false;
        }
        say ("midi: virtual port created");

        // Other programs find the port in the system's list of MIDI sources.
        juce::String identifier;
        for (int attempt = 0; attempt < 40 && identifier.isEmpty(); ++attempt)
        {
            for (const auto& device : juce::MidiInput::getAvailableDevices())
                if (device.name == name)
                    identifier = device.identifier;
            if (identifier.isEmpty())
                juce::Thread::sleep (50);
        }
        if (identifier.isEmpty())
        {
            say ("midi: FAILED - the port does not show up in the list of MIDI sources");
            return false;
        }
        say ("midi: port is listed for other programs");

        Collector collector;
        auto input = juce::MidiInput::openDevice (identifier, &collector);
        if (input == nullptr)
        {
            say ("midi: FAILED to open the port as another program would");
            return false;
        }
        input->start();
        juce::Thread::sleep (200);

        for (int attempt = 0; attempt < 20 && ! collector.complete(); ++attempt)
        {
            output->sendMessageNow (juce::MidiMessage::pitchWheel (1, 9000));
            output->sendMessageNow (juce::MidiMessage::controllerEvent (1, 11, 77));
            output->sendMessageNow (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100));
            output->sendMessageNow (juce::MidiMessage::noteOff (1, 69));
            juce::Thread::sleep (100);
        }

        input->stop();
        say ("midi: received note-on " + juce::String ((int) collector.gotNoteOn.load())
             + ", note-off " + juce::String ((int) collector.gotNoteOff.load())
             + ", pitch bend " + juce::String ((int) collector.gotBend.load())
             + ", expression " + juce::String ((int) collector.gotController.load()));
        return collector.complete();
    }
}

SelfTest::SelfTest (std::function<void (int)> onFinished)
    : juce::Thread ("ViolinMIDI self-test"), finished (std::move (onFinished))
{
}

SelfTest::~SelfTest()
{
    stopThread (15000);
}

void SelfTest::run()
{
    say (juce::String ("ViolinMIDI self-test, version ") + JUCE_APPLICATION_VERSION_STRING);
    const bool engineOk = engineCheck();
    const bool midiOk = midiCheck();
    const int code = (engineOk ? 0 : 1) + (midiOk ? 0 : 2);
    say (code == 0 ? "SELFTEST OK" : "SELFTEST FAILED (code " + juce::String (code) + ")");

    auto callback = finished;
    juce::MessageManager::callAsync ([callback, code] { callback (code); });
}
