#include "SettingsStore.h"

#include "Brand.h"

namespace key
{
    // Keys from version 1.0/1.1 keep their names, so every version reads the same file.
    constexpr const char* bendMode      = "bendMode";
    constexpr const char* bendRange     = "bendRange";
    constexpr const char* dynamics      = "dynamics";
    constexpr const char* response      = "response";
    constexpr const char* lowestNote    = "lowestNote";
    constexpr const char* gainDb        = "gainDb";
    constexpr const char* gateDb        = "gateDb";
    constexpr const char* fixedVelocity = "fixedVelocity";
    constexpr const char* a4Hz          = "a4Hz";
    constexpr const char* midiChannel   = "midiChannel";
    constexpr const char* audioState    = "audioState";
    // added in 1.2
    constexpr const char* schema        = "settingsSchema";
    constexpr const char* lastVersion   = "lastVersion";
    constexpr const char* midiPortName  = "midiPortName";
    constexpr const char* windowX       = "windowX";
    constexpr const char* windowY       = "windowY";
}

SettingsStore::SettingsStore (juce::PropertiesFile& file) : props (file)
{
    migrate();
}

bool SettingsStore::isOfferedBendRange (int semitones)
{
    return semitones == 2 || semitones == 4 || semitones == 12 || semitones == 24 || semitones == 48;
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
    const vm::Settings d;
    auto& e = s.engine;

    e.bendMode      = (vm::BendMode) juce::jlimit (0, 2, props.getIntValue (key::bendMode, (int) d.bendMode));
    e.dynamics      = (vm::DynamicsMode) juce::jlimit (0, 2, props.getIntValue (key::dynamics, (int) d.dynamics));
    e.response      = (vm::Response) juce::jlimit (0, 2, props.getIntValue (key::response, (int) d.response));
    e.lowestNote    = juce::jlimit (vm::kAbsLowestNote, 60, props.getIntValue (key::lowestNote, d.lowestNote));
    e.inputGainDb   = (float) juce::jlimit (-12.0, 36.0, props.getDoubleValue (key::gainDb, d.inputGainDb));
    e.gateDb        = (float) juce::jlimit (-70.0, -20.0, props.getDoubleValue (key::gateDb, d.gateDb));
    e.fixedVelocity = juce::jlimit (1, 127, props.getIntValue (key::fixedVelocity, d.fixedVelocity));
    e.a4Hz          = (float) juce::jlimit (430.0, 450.0, props.getDoubleValue (key::a4Hz, d.a4Hz));
    e.midiChannel   = juce::jlimit (1, 16, props.getIntValue (key::midiChannel, d.midiChannel));

    const int range = props.getIntValue (key::bendRange, d.bendRangeSemis);
    e.bendRangeSemis = isOfferedBendRange (range) ? range : d.bendRangeSemis;

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
    const auto& e = s.engine;
    props.setValue (key::bendMode, (int) e.bendMode);
    props.setValue (key::bendRange, e.bendRangeSemis);
    props.setValue (key::dynamics, (int) e.dynamics);
    props.setValue (key::response, (int) e.response);
    props.setValue (key::lowestNote, e.lowestNote);
    props.setValue (key::gainDb, (double) e.inputGainDb);
    props.setValue (key::gateDb, (double) e.gateDb);
    props.setValue (key::fixedVelocity, e.fixedVelocity);
    props.setValue (key::a4Hz, (double) e.a4Hz);
    props.setValue (key::midiChannel, e.midiChannel);
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
