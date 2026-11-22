#include "SettingsStore.h"

#include "Brand.h"
#include "SettingsFields.h"

#include <cmath>

namespace key
{
    // The engine settings and their keys are in SettingsFields.cpp.
    constexpr const char* bendMode      = "bendMode";
    constexpr const char* audioState    = "audioState";
    // added in 1.2
    constexpr const char* schema        = "settingsSchema";
    constexpr const char* lastVersion   = "lastVersion";
    constexpr const char* midiPortName  = "midiPortName";
    constexpr const char* windowX       = "windowX";
    constexpr const char* windowY       = "windowY";
    // added in 1.3
    constexpr const char* midiInput     = "midiInput";
    constexpr const char* midiMap       = "midiMap";
    // added in 1.4
    constexpr const char* midiProgramChannel = "midiProgramChannel";
    constexpr const char* presetSlot    = "presetSlot";
}

SettingsStore::SettingsStore (juce::PropertiesFile& file) : props (file)
{
    migrate();
}

int SettingsStore::bendRangeIndex (float semitones)
{
    int best = 0;
    for (int i = 1; i < kNumBendRanges; ++i)
        if (std::abs (kBendRanges[i] - semitones) < std::abs (kBendRanges[best] - semitones))
            best = i;
    return best;
}

void SettingsStore::migrate()
{
    if (props.getIntValue (key::schema, 0) >= kSchema)
        return;

    // Files from 1.0 and 1.1 have no schema number. Someone who used those
    // versions has set up MainStage for the port called "ViolinMIDI": keep it.
    const bool earlierInstall = props.containsKey (key::bendMode) || props.containsKey (key::audioState);
    if (! props.containsKey (key::midiPortName))
        props.setValue (key::midiPortName, earlierInstall ? brand::kLegacyName : brand::kName);

    props.setValue (key::schema, kSchema);
}

AppSettings SettingsStore::load()
{
    AppSettings s;
    fields::load (props, s.engine);

    s.midiInputName = props.getValue (key::midiInput);
    s.midiMapJson   = props.getValue (key::midiMap);
    s.midiProgramChannel = juce::jlimit (-1, 16, props.getIntValue (key::midiProgramChannel, 0));
    s.presetSlot    = juce::jlimit (0, 128, props.getIntValue (key::presetSlot, 0));

    s.midiPortName = props.getValue (key::midiPortName, brand::kName).trim();
    if (s.midiPortName.isEmpty())
        s.midiPortName = brand::kName;

    s.hasWindowPosition = props.containsKey (key::windowX) && props.containsKey (key::windowY);
    s.windowX = props.getIntValue (key::windowX);
    s.windowY = props.getIntValue (key::windowY);
    return s;
}

void SettingsStore::save (const AppSettings& s)
{
    fields::save (props, s.engine);
    props.setValue (key::midiInput, s.midiInputName);
    props.setValue (key::midiMap, s.midiMapJson);
    props.setValue (key::midiProgramChannel, s.midiProgramChannel);
    props.setValue (key::presetSlot, s.presetSlot);
    props.setValue (key::midiPortName, s.midiPortName);
    props.setValue (key::schema, kSchema);
    props.setValue (key::lastVersion, JUCE_APPLICATION_VERSION_STRING);

    if (s.hasWindowPosition)
    {
        props.setValue (key::windowX, s.windowX);
        props.setValue (key::windowY, s.windowY);
    }
}

std::unique_ptr<juce::XmlElement> SettingsStore::audioState() const
{
    auto state = props.getXmlValue (key::audioState);
    if (state != nullptr)
        state->removeAttribute ("audioDeviceRate"); // never change the rate other programs are using
    return state;
}

void SettingsStore::saveAudioState (const juce::XmlElement& state)
{
    juce::XmlElement copy (state);
    copy.removeAttribute ("audioDeviceRate");
    props.setValue (key::audioState, &copy);
}

void SettingsStore::flush()
{
    props.saveIfNeeded();
}
