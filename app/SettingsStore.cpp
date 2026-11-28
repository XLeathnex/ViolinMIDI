#include "SettingsStore.h"

#include "Brand.h"
#include "SettingsFields.h"

#include <cmath>

namespace key
{
    // The engine settings and their keys are in SettingsFields.cpp.
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
    // added in 1.5
    constexpr const char* camera        = "camera";
    constexpr const char* bufferChosen  = "bufferChosen";
    // an engine setting (SettingsFields.cpp) whose default changed in 2.0
    constexpr const char* stereoSpread  = "stereoSpread";
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
    const int schema = props.getIntValue (key::schema, 0);
    if (schema >= kSchema)
        return;

    // 2.0: the app is called VioLogic, and so is its MIDI port - for every
    // installation, also those set up for "Archet" or "ViolinMIDI" (MainStage
    // needs the port chosen once more). Until 1.5 the port of an installation
    // from 1.0 or 1.1 kept the name "ViolinMIDI".
    props.setValue (key::midiPortName, brand::kName);

    // 2.0: the voices of the audio harmonizer are spread +-35 % by default
    // instead of +-50 %. A spread still at the old default moves with it - once:
    // 1.5 writes schema 2 again after a visit, but it leaves "ensemble", which
    // only 2.0 writes, so a 50 % chosen in 2.0 is never taken for the old default.
    if (schema >= 1 && ! props.containsKey ("ensemble") && props.containsKey (key::stereoSpread)
        && std::abs (props.getDoubleValue (key::stereoSpread) - 0.5) < 1.0e-6)
        props.setValue (key::stereoSpread, (double) vm::Settings().stereoSpread);

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
    s.cameraJson    = props.getValue (key::camera);
    s.bufferChosen  = props.getBoolValue (key::bufferChosen, false);

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
    props.setValue (key::camera, s.cameraJson);
    props.setValue (key::bufferChosen, s.bufferChosen);
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
