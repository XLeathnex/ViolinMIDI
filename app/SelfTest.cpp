#include "SelfTest.h"

#include "Brand.h"
#include "Engine.h"
#include "PresetStore.h"
#include "SettingsFields.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    constexpr const char* kTestPortName = "ViolinMIDI Selftest";
    constexpr int kTickMs = 50;
    constexpr int kListTimeoutTicks = 100;  // 5 s for the port to appear in the system's list
    constexpr int kSendTimeoutTicks = 60;   // 3 s for the messages to arrive

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
}

namespace
{
    /** Presets and the settings file: what is written must come back unchanged. */
    bool storageCheck()
    {
        vm::Settings wanted;
        wanted.bendMode = vm::BendMode::Smoothed;
        wanted.bendRangeSemis = 12.0f;
        wanted.gateDb = -40.5f;
        wanted.gateOffDb = -57.0f;
        wanted.keyFilter = true;
        wanted.keyRoot = 9;
        wanted.scaleMode = 12;
        wanted.pentaSustain = true;
        wanted.triad = true;
        wanted.triadChannel = 5;
        wanted.voicing = vm::scale::kFreeVoicing;
        wanted.harmonyMode = vm::HarmonyMode::Chromatic;
        wanted.freeVoicing = (1ull << 0) | (1ull << 24) | (1ull << 31) | (1ull << 48);
        wanted.mpe = true;
        wanted.octaveAttackGuard = false;
        wanted.octaveSustainMs = 80.0f;
        wanted.articulation = vm::Articulation::Strong;
        wanted.dynamicsTarget = vm::DynamicsTarget::ModAndExpression;
        wanted.dynamicsSmoothingMs = 120.0f;
        const auto image = [] (const vm::Settings& s) { return juce::JSON::toString (fields::toVar (s), true); };

        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("ViolinMIDI-selftest-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
        bool ok = true;
        const auto expect = [&ok] (bool condition, const char* what)
        {
            if (! condition)
            {
                say (juce::String ("storage: FAILED - ") + what);
                ok = false;
            }
        };

        {
            PresetStore store (folder.getChildFile ("Presets"));
            expect (store.entries().empty() && store.firstFreeSlot() == 1, "an empty folder has no presets");
            expect (store.save (3, "Irish Dorisch Pad", wanted), "save");
            expect (store.save (7, "Zwei", vm::Settings()), "save a second preset");

            vm::Settings loaded;
            loaded.enabled = false;   // not part of a preset: must survive loading
            loaded.freezeSolo = true;
            expect (store.load (3, loaded), "load");
            expect (image (loaded) == image (wanted), "a loaded preset equals the saved one");
            expect (! loaded.enabled && loaded.freezeSolo, "master switch and freeze are left alone");

            expect (store.rename (3, "Neuer Name"), "rename");
            PresetStore again (folder.getChildFile ("Presets"));
            again.refresh();
            expect (again.entries().size() == 2 && again.find (3) != nullptr && again.find (3)->name == "Neuer Name", "the renamed preset is found again");
            expect (again.next (3) == 7 && again.next (7) == 3 && again.previous (3) == 7, "next and previous wrap");
            expect (again.remove (7) && again.find (7) == nullptr && again.entries().size() == 1, "remove");
            vm::Settings untouched;
            expect (! again.load (7, untouched), "a removed preset does not load");
        }

        {
            juce::PropertiesFile::Options options;
            options.millisecondsBeforeSaving = -1;
            const auto file = folder.getChildFile ("test.settings");
            {
                juce::PropertiesFile props (file, options);
                fields::save (props, wanted);
                props.save();
            }
            juce::PropertiesFile props (file, options);
            vm::Settings loaded;
            fields::load (props, loaded);
            expect (image (loaded) == image (wanted), "the settings file gives back what was stored");

            // a file from version 1.3: one threshold, the pentatonic as a switch
            juce::PropertiesFile old (folder.getChildFile ("old.settings"), options);
            old.setValue ("gateDb", -38.0);
            old.setValue ("scaleMode", 5);
            old.setValue ("pentatonic", true);
            old.setValue ("keyFilter", true);
            vm::Settings migrated;
            fields::load (old, migrated);
            expect (migrated.gateOffDb == -44.0f, "an older file ends notes 6 dB below its threshold");
            expect (migrated.scaleMode == 11 && ! migrated.pentatonic, "the pentatonic switch becomes the pentatonic scale");
        }

        folder.deleteRecursively();
        say (ok ? "storage: presets and settings file read back what was written" : "storage: FAILED");
        return ok;
    }
}

/** Listens to the port from the outside, the way MainStage would. */
struct SelfTest::Collector final : public juce::MidiInputCallback
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

SelfTest::SelfTest (std::function<void (int)> onFinished)
    : finished (std::move (onFinished)), collector (std::make_unique<Collector>())
{
}

SelfTest::~SelfTest()
{
    stopTimer();
    if (input != nullptr)
        input->stop();
}

void SelfTest::start()
{
    say (juce::String ("ViolinMIDI self-test, version ") + JUCE_APPLICATION_VERSION_STRING);
    say ("fonts: " + brand::fontDescription());
    engineOk = engineCheck();
    engineOk = storageCheck() && engineOk;

    // The virtual port, created exactly as the app creates it.
    output = juce::MidiOutput::createNewDevice (kTestPortName);

    if (output == nullptr)
    {
       #if JUCE_MAC
        say ("midi: FAILED to create the virtual port");
        finish (false);
       #else
        say ("midi: virtual ports are not available on this system - skipped");
        finish (true);
       #endif
        return;
    }

    say ("midi: virtual port created");
    state = State::waitingForPort;
    ticks = 0;
    startTimer (kTickMs);
}

void SelfTest::timerCallback()
{
    ++ticks;

    if (state == State::waitingForPort)
    {
        // Other programs find the port in the system's list of MIDI sources.
        juce::String identifier;
        for (const auto& device : juce::MidiInput::getAvailableDevices())
            if (device.name == kTestPortName)
                identifier = device.identifier;

        if (identifier.isEmpty())
        {
            if (ticks > kListTimeoutTicks)
            {
                say ("midi: FAILED - the port does not show up in the list of MIDI sources");
                finish (false);
            }
            return;
        }

        say ("midi: port is listed for other programs");
        input = juce::MidiInput::openDevice (identifier, collector.get());
        if (input == nullptr)
        {
            say ("midi: FAILED to open the port as another program would");
            finish (false);
            return;
        }

        input->start();
        state = State::sending;
        ticks = 0;
        return;
    }

    if (state == State::sending)
    {
        if (collector->complete() || ticks > kSendTimeoutTicks)
        {
            say ("midi: received note-on " + juce::String ((int) collector->gotNoteOn.load())
                 + ", note-off " + juce::String ((int) collector->gotNoteOff.load())
                 + ", pitch bend " + juce::String ((int) collector->gotBend.load())
                 + ", expression " + juce::String ((int) collector->gotController.load()));
            finish (collector->complete());
            return;
        }

        output->sendMessageNow (juce::MidiMessage::pitchWheel (1, 9000));
        output->sendMessageNow (juce::MidiMessage::controllerEvent (1, 11, 77));
        output->sendMessageNow (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100));
        output->sendMessageNow (juce::MidiMessage::noteOff (1, 69));
    }
}

void SelfTest::finish (bool midiOk)
{
    stopTimer();
    state = State::done;
    if (input != nullptr)
        input->stop();
    input.reset();
    output.reset();

    const int code = (engineOk ? 0 : 1) + (midiOk ? 0 : 2);
    say (code == 0 ? "SELFTEST OK" : "SELFTEST FAILED (code " + juce::String (code) + ")");

    // Reported from a fresh message, so the app can quit cleanly whether this
    // was reached from the timer or straight from start().
    if (finished != nullptr)
    {
        auto callback = finished;
        juce::MessageManager::callAsync ([callback, code] { callback (code); });
    }
}
