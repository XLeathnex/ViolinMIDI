// Everything the app remembers between runs, in one file shared by all
// versions: going forward or back keeps the settings.
//
// Adding an engine setting: one line in SettingsFields.cpp. Anything else: a
// field in AppSettings, one line in load() and one in save(). Older versions
// ignore keys they do not know, so nothing breaks when switching back. If a key ever changes meaning, raise kSchema and convert the
// old value in migrate().
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
};

class SettingsStore
{
public:
    static constexpr int kSchema = 2;

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
