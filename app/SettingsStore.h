// Everything the app remembers between runs, in one file shared by all
// versions: going forward or back keeps the settings.
//
// Adding a setting: a field in AppSettings, one line in load() and one in
// save(). Older versions ignore keys they do not know, so nothing breaks when
// switching back. If a key ever changes meaning, raise kSchema and convert the
// old value in migrate().
#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "Types.h"

struct AppSettings
{
    vm::Settings engine;
    juce::String midiPortName;
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

    /** Pitch-bend ranges offered to the user. */
    static bool isOfferedBendRange (int semitones);

private:
    void migrate();

    juce::PropertiesFile& props;
};
