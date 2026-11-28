#include "SelfTest.h"

#include "Brand.h"
#include "Engine.h"
#include "PresetStore.h"
#include "CameraSettings.h"
#include "MidiRemote.h"
#include "SettingsFields.h"
#include "SettingsStore.h"

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
        // 2.0: the voices of a preset voicing as edited, the two curves, drop voicing, ensemble
        wanted.voicingEdited = true;
        wanted.voicingVoices = vm::scale::maskBit (-12) | vm::scale::maskBit (0) | vm::scale::maskBit (7);
        wanted.formantCurve = vm::curve::make ({ { 55.0f, 0.1f }, { 70.5f, 0.35f }, { 88.0f, 0.9f } });
        wanted.hiCutCurve = vm::curve::make ({ { 55.0f, 9000.0f }, { 80.0f, 2800.0f } });
        wanted.dropVoicing = false;
        wanted.ensemble = 0.4f;
        wanted.stereoSpread = 0.7f;
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

            // A 1.5 preset saved again in 2.0 keeps 1.5's own keys (its harmonizer values)
            // when it goes back into its own slot; a different preset written over it does not.
            expect (again.save (9, "Aus 1.5", wanted), "save a preset to turn into a 1.5 one");
            again.refresh();
            if (const auto* entry = again.find (9))
            {
                auto doc = juce::JSON::parse (entry->file);
                if (auto* stored = doc["settings"].getDynamicObject())
                    stored->setProperty ("bodyKeep", 0.42);
                entry->file.replaceWithText (juce::JSON::toString (doc));
                const auto file = entry->file;
                expect (again.save (9, "Aus 1.5", wanted, true) && file.loadFileAsString().contains ("bodyKeep"),
                        "saved back into its own slot: the keys of other versions stay");
                expect (again.save (9, "Aus 1.5", wanted, false) && file.existsAsFile() && ! file.loadFileAsString().contains ("bodyKeep"),
                        "another preset written over it: they go");
            }
            else
            {
                expect (false, "the preset in slot 9 is found");
            }

            // 2.1: patch() replaces only the given keys; every other one stays, also those of other versions.
            expect (again.save (11, "Zum Patchen", wanted), "save a preset to patch");
            if (const auto* entry = again.find (11))
            {
                auto doc = juce::JSON::parse (entry->file);
                if (auto* stored = doc["settings"].getDynamicObject())
                    stored->setProperty ("future", 7);
                entry->file.replaceWithText (juce::JSON::toString (doc));
            }
            auto louder = wanted;
            louder.harmonyGainDb = -3.0f;
            juce::NamedValueSet before;
            expect (again.patch (11, fields::subset (louder, { "harmonyGainDb", "noSuchKey" }), &before), "patch");
            vm::Settings patched;
            expect (again.load (11, patched) && std::abs (patched.harmonyGainDb + 3.0f) < 1.0e-6f, "patch: the new value is in the preset");
            patched.harmonyGainDb = wanted.harmonyGainDb;
            expect (image (patched) == image (wanted), "patch: every other setting stays");
            expect (again.find (11) != nullptr && again.find (11)->file.loadFileAsString().contains ("future"), "patch: keys of other versions stay");
            expect (before.size() == 1 && std::abs ((double) before["harmonyGainDb"] - (double) wanted.harmonyGainDb) < 1.0e-6,
                    "patch: the old value is handed back for undo");
            vm::Settings undone;
            expect (again.patch (11, before) && again.load (11, undone) && image (undone) == image (wanted), "patch: undo restores the preset");
            expect (! again.patch (12, before), "patch: no such preset");

            // 2.1: the preset image leaves out the instrument profile and the wiring.
            const auto presetPart = fields::toVar (wanted, fields::Part::presetImage);
            expect (fields::toVar (wanted).hasProperty ("gainDb") && ! presetPart.hasProperty ("gainDb") && ! presetPart.hasProperty ("dryKill")
                        && presetPart.hasProperty ("harmonyGainDb") && fields::profileKeys().size() == 13,
                    "the preset image has no profile or wiring keys");
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
            expect (std::abs (migrated.gateOffDb + 44.0f) < 1.0e-4f, "an older file ends notes 6 dB below its threshold");
            expect (migrated.scaleMode == 11 && ! migrated.pentatonic, "the pentatonic switch becomes the pentatonic scale");
            expect (! migrated.voicingEdited && vm::voicingMask (migrated) == vm::scale::presetMask (migrated.voicing),
                    "a file from before 2.0 plays the voicing's own voices");
            expect (migrated.formantCurve == vm::curve::defaultFormant() && migrated.hiCutCurve == vm::curve::defaultHiCut(),
                    "a file from before 2.0 starts with the standard curves");
        }

        {
            // A settings file of 1.5 (schema 2): the port is renamed, an untouched spread moves to the new default.
            juce::PropertiesFile::Options options;
            options.millisecondsBeforeSaving = -1;
            juce::PropertiesFile old15 (folder.getChildFile ("v15.settings"), options);
            old15.setValue ("settingsSchema", 2);
            old15.setValue ("midiPortName", "Archet");
            old15.setValue ("stereoSpread", 0.5);
            const SettingsStore store15 (old15);
            expect (old15.getValue ("midiPortName") == brand::kName && old15.getIntValue ("settingsSchema") == SettingsStore::kSchema,
                    "1.5 -> 2.0: the MIDI port is called VioLogic");
            expect (std::abs (old15.getDoubleValue ("stereoSpread") - 0.35) < 1.0e-6, "1.5 -> 2.0: the untouched spread of 50 % becomes 35 %");

            juce::PropertiesFile chosen (folder.getChildFile ("chosen.settings"), options);
            chosen.setValue ("settingsSchema", 2);
            chosen.setValue ("midiPortName", "ViolinMIDI");
            chosen.setValue ("stereoSpread", 0.8);
            const SettingsStore storeChosen (chosen);
            expect (chosen.getValue ("midiPortName") == brand::kName && std::abs (chosen.getDoubleValue ("stereoSpread") - 0.8) < 1.0e-6,
                    "1.5 -> 2.0: a spread the player chose stays");

            juce::PropertiesFile fresh (folder.getChildFile ("fresh.settings"), options);
            const SettingsStore storeFresh (fresh);
            expect (fresh.getValue ("midiPortName") == brand::kName && ! fresh.containsKey ("stereoSpread"), "a new installation: VioLogic");

            // 2.0 -> 1.5 -> 2.0: 1.5 writes schema 2 again but leaves "ensemble": a 50 % chosen in 2.0 stays.
            juce::PropertiesFile visited (folder.getChildFile ("visited.settings"), options);
            visited.setValue ("settingsSchema", 2);
            visited.setValue ("ensemble", 1.0);
            visited.setValue ("stereoSpread", 0.5);
            const SettingsStore storeVisited (visited);
            expect (std::abs (visited.getDoubleValue ("stereoSpread") - 0.5) < 1.0e-6, "after a visit to 1.5 a spread chosen in 2.0 stays");
        }

        {
            // Voices edited in 2.0 for "Enger Dreiklang"; 1.5 then chose the seventh chord (it only knows "voicing").
            juce::PropertiesFile::Options options;
            options.millisecondsBeforeSaving = -1;
            juce::PropertiesFile props (folder.getChildFile ("voicing.settings"), options);
            vm::Settings edited;
            edited.voicing = 0;
            edited.voicingEdited = true;
            edited.voicingVoices = vm::scale::presetMask (0) | vm::scale::maskBit (12);
            fields::save (props, edited);
            vm::Settings same;
            fields::load (props, same);
            expect (same.voicingEdited && vm::voicingMask (same) == edited.voicingVoices, "edited voices come back with their voicing");
            props.setValue ("voicing", 1);
            vm::Settings changed;
            fields::load (props, changed);
            expect (! changed.voicingEdited && vm::voicingMask (changed) == vm::scale::presetMask (1),
                    "voices edited for another voicing are not played on this one");
        }

        {
            // MIDI bindings of parameters another version has (1.5's "bodyKeep") are written back unchanged.
            MidiRemote remote;
            remote.fromJson ("[{\"id\":\"bodyKeep\",\"type\":\"cc\",\"channel\":0,\"number\":20},"
                             "{\"id\":\"triad\",\"type\":\"note\",\"channel\":3,\"number\":60}]");
            const auto json = remote.toJson();
            expect (json.contains ("bodyKeep") && json.contains ("\"triad\""), "MIDI bindings of other versions are kept");

            // The camera settings of 1.5 stay in the file for 1.5.
            auto camera = CameraSettings::fromJson ("{\"heightTarget\":2,\"low\":-0.3,\"high\":0.7}");
            expect (camera.yTarget == 11, "1.5's camera height on CC 11 becomes the Y axis on CC 11");
            camera.xTarget = 74;
            const auto cameraJson = camera.toJson();
            expect (cameraJson.contains ("heightTarget") && cameraJson.contains ("\"high\"") && cameraJson.contains ("xTarget"),
                    "1.5's camera keys are kept next to 2.0's");
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
    say (juce::String ("VioLogic (ViolinMIDI) self-test, version ") + JUCE_APPLICATION_VERSION_STRING);
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
