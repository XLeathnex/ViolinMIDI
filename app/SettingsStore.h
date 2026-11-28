// Everything the app remembers between runs, in one file shared by all
// versions: going forward or back keeps the settings.
//
// Adding an engine setting: one line in core/SettingsText.cpp. Anything else: a
// field in AppSettings, one line in load() and one in save(). Older versions
// ignore keys they do not know and write them back, so nothing breaks when
// switching back. If a key ever changes meaning, convert the old value once:
// by kSchema in migrate(), or, when an older version writes the same schema
// again after a visit, by the presence of a key only the new version writes
// (2.0's "ensemble", 2.1's "harmonySource").
#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "Types.h"

struct AppSettings
{
    vm::Settings engine;
    juce::String midiPortName;
    juce::String midiInputName;   ///< port for remote control, empty = none
    juce::String midiMapJson;     ///< remote-control bindings, see MidiRemote::toJson
    int midiProgramChannel = 0;   ///< program changes load presets: -1 = off, 0 = every channel, 1..16
    int presetSlot = 0;           ///< the preset last loaded or saved, 0 = none
    juce::String cameraJson;      ///< camera control, see CameraSettings::toJson
    bool bufferChosen = false;    ///< the user picked the audio buffer size; otherwise the smallest is used
    bool hasWindowPosition = false;
    int windowX = 0, windowY = 0;

    // --- 2.1: external chords. The keyboardist's rig, so app-wide and not in presets;
    // the app copies split and latch into the engine settings (vm::Settings::chordSplit ...).
    juce::String chordInputName;      ///< MIDI input of the chord keyboard, empty = none
    int  chordSplit = 60;             ///< keys below this note make the chord, 24 .. 96 (60 = C4, middle C)
    bool chordWholeKeyboard = false;  ///< every key makes the chord
    bool chordLatch = true;           ///< the last chord stays after all keys and the pedal are up

    // --- 2.1: instrument profiles
    juce::String profilesJson;        ///< the profiles and the active one, see ProfileStore
    juce::String profilesUnreadable;  ///< profile text that could not be read, kept once; written only when not empty
    juce::String lastVersion;         ///< read only: the version that wrote the file last (before this run saves)

    // --- 2.1: automatic calibration
    int calibrationMode = 0;          ///< the wizard's last choice: 0 Normalmodus, 1 Profimodus
    juce::String lastCalibration;     ///< JSON: date, version, mode, profile, preset, report folder of the last saved calibration

    // --- 2.1: main window and harmonizer output
    bool fullscreen = false;          ///< the window was in fullscreen when the app last saved
    int  displayZoomX = 2;            ///< time zoom step of the live display, 0 .. 4 (2 = standard)
    int  displayZoomY = 2;            ///< pitch / level zoom step, 0 .. 4 (2 = standard)
    juce::String harmonizerOutputDevice;   ///< output device of the harmony voices, empty = "Automatisch"
    int  harmonizerOutputPair = 0;    ///< output channel pair: 0 = Out 1/2, 1 = Out 3/4 ... 0 .. 31
    bool harmonizerOutputStored = false;   ///< the file had the output keys, or the app has decided them since (after the
                                           ///< first start of 2.1, MainComponent); save() writes them only then
};

class SettingsStore
{
public:
    /** 2: since 1.2 (the MIDI port's name is stored). 3: since 2.0 (VioLogic). */
    static constexpr int kSchema = 3;

    explicit SettingsStore (juce::PropertiesFile& file);

    AppSettings load();
    void save (const AppSettings&);

    std::unique_ptr<juce::XmlElement> audioState() const;
    void saveAudioState (const juce::XmlElement&);

    /** Writes pending changes to disk now. */
    void flush();

    /** Pitch-bend ranges offered to the user, in semitones. */
    static constexpr int kNumBendRanges = 10;
    static constexpr float kBendRanges[kNumBendRanges] = { 0.2f, 0.3f, 0.4f, 0.5f, 1.0f, 2.0f, 4.0f, 12.0f, 24.0f, 48.0f };
    /** The offered range closest to the given one. */
    static int bendRangeIndex (float semitones);

private:
    void migrate();

    juce::PropertiesFile& props;
};
